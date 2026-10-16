#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Build, sign and verify the .msix package for the Xbox, out of what you compiled.

    python tools/package_uwp.py --exe <build>/SuperMetroidRecomp.exe ^
        --rom "<your rom>.sfc" --extra <sdl3-uwp>/bin/SDL3.dll ^
        --tiles <folder with your 13 PNGs> --certificate <thumbprint> ^
        --version 0.1.0.0 --work <scratch folder>

Nothing here is specific to this machine: every path is an argument and the Windows SDK is
located by looking, not by being told. The certificate thumbprint can also come from the
UWP_CERT_THUMBPRINT environment variable, so it need not appear in your shell history.

The checks before packaging are the point of this script, not the packaging. Each one stands for
a day someone lost on a console that installs the package happily and then refuses to start it:

  - the APPCONTAINER bit, which the Ninja generator for WindowsStore does not set on its own;
  - the "_app" CRT, which is what tells a `vcvarsall x64 store` build from a `vcvars64` one;
  - every DLL the executable imports either shipped inside the package or known to be on the
    console. SDL3 is the one that bites here: this game links it dynamically;
  - the manifest's Publisher against the subject of the certificate that is about to sign it.
    If they differ, the console rejects the package at install time;
  - the thirteen tiles, by name and by PNG header, because the manifest names them and a
    missing one makes the package build and then show no icon at all.

Nothing is written outside --work, and that folder's `content/` is rebuilt from scratch on each
run, so a stale file from a previous attempt cannot end up inside a signed package.
"""
import argparse
import base64
import os
import re
import shutil
import struct
import subprocess
import sys
from pathlib import Path

for stream in (sys.stdout, sys.stderr):
    if hasattr(stream, 'reconfigure'):
        stream.reconfigure(encoding='utf-8', errors='replace')

HERE = Path(__file__).resolve().parent
REPO = HERE.parent

# The thirteen the manifest names. Same list the console resolves through resources.pri.
TILES = (
    'LockScreenLogo.png', 'LockScreenLogo.scale-200.png',
    'SplashScreen.png', 'SplashScreen.scale-200.png',
    'Square150x150Logo.png', 'Square150x150Logo.scale-200.png',
    'Square44x44Logo.png', 'Square44x44Logo.scale-200.png',
    'Square44x44Logo.targetsize-24_altform-unplated.png',
    'StoreLogo.png', 'StoreLogo.scale-200.png',
    'Wide310x150Logo.png', 'Wide310x150Logo.scale-200.png',
)

# DLLs the console provides itself. Anything else the executable imports has to travel inside
# the package, or it installs and then fails to activate with 0x8027025B.
SYSTEM_DLLS = (
    'kernel32', 'user32', 'advapi32', 'ole32', 'oleaut32', 'shell32', 'shlwapi',
    'msvcp140_app', 'vcruntime140_app', 'vcruntime140_1_app', 'concrt140_app',
    'd3d11', 'd3d12', 'dxgi', 'd3dcompiler_47', 'xaudio2_9', 'xinput1_4',
    'windows.gaming.input', 'onecore', 'combase', 'bcrypt', 'crypt32', 'ws2_32',
    'winmm', 'mmdevapi', 'api-ms-win', 'ext-ms-win', 'ucrtbase', 'ntdll', 'dbghelp',
)


def find_sdk():
    """Newest Windows 10/11 SDK that has the three tools we need."""
    roots = [Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / 'Windows Kits' / '10' / 'bin',
             Path(os.environ.get('ProgramFiles', r'C:\Program Files')) / 'Windows Kits' / '10' / 'bin']
    found = []
    for root in roots:
        if not root.is_dir():
            continue
        for ver in root.glob('10.*'):
            x64 = ver / 'x64'
            if all((x64 / t).is_file() for t in ('makeappx.exe', 'makepri.exe', 'signtool.exe')):
                found.append((tuple(int(p) for p in re.findall(r'\d+', ver.name)), x64))
    if not found:
        sys.exit('No Windows SDK with makeappx/makepri/signtool found. Install the Windows SDK\n'
                 '(it comes with Visual Studio 2022; see docs/DEPENDENCIES.md).')
    found.sort()
    return found[-1][1]


def appcontainer_bit(exe):
    """DllCharacteristics in the PE header; 0x1000 is IMAGE_DLLCHARACTERISTICS_APPCONTAINER."""
    data = exe.read_bytes()
    e_lfanew = struct.unpack_from('<I', data, 0x3C)[0]
    if data[e_lfanew:e_lfanew + 4] != b'PE\0\0':
        sys.exit('%s is not a PE file' % exe)
    opt = e_lfanew + 24
    if struct.unpack_from('<H', data, opt)[0] != 0x20B:
        sys.exit('%s is not PE32+ (x64). An Xbox package has to be x64.' % exe)
    return bool(struct.unpack_from('<H', data, opt + 70)[0] & 0x1000)


def check_exe(exe, extras):
    if not appcontainer_bit(exe):
        sys.exit('The APPCONTAINER bit is missing. Link with /APPCONTAINER: the Ninja generator\n'
                 'for WindowsStore does not add it on its own, and the console rejects the\n'
                 'package when it tries to activate it. src/CMakeLists.txt already passes it,\n'
                 'so this usually means the exe came from a different build tree.')
    raw = exe.read_bytes()
    lowered = raw.lower()
    missing = [n for n in (b'msvcp140_app.dll', b'vcruntime140_app.dll') if n not in lowered]
    if missing:
        sys.exit('The exe does not import the "_app" CRT (%s). It was built with the desktop\n'
                 'environment: use `vcvarsall.bat x64 store`, not `vcvars64.bat`.\n'
                 'On the console this shows up as "the app didn\'t start" (0x8027025B).'
                 % b', '.join(missing).decode())
    print('  exe: APPCONTAINER bit and "_app" CRT present')

    asked = {n.decode('latin1').lower() for n in re.findall(rb'[A-Za-z0-9_.\-]+\.dll', raw)}
    brought = {p.name.lower() for p in extras}
    absent = sorted(d for d in asked
                    if not any(d.startswith(s) for s in SYSTEM_DLLS) and d not in brought)
    if absent:
        sys.exit('The exe imports DLLs that are not in the package: %s\n'
                 'Pass them with --extra, or the console will install the package and then\n'
                 'refuse to launch it. Note that the UWP build of SDL3 is NOT the desktop one.'
                 % ', '.join(absent))
    if brought:
        print('  exe: companion DLLs in the package: %s' % ', '.join(sorted(brought)))


def cert_subject(thumbprint):
    """Subject of the signing certificate, read from the current user's store.

    Two things here are deliberate, and both were paid for on the first run.

    The store is opened through .NET (X509Store) instead of the `Cert:` drive. That drive comes
    from a PowerShell module, and on a machine whose PSModulePath has been rewritten -- a
    PowerShell 7 install will do it -- Windows PowerShell 5.1 answers "a drive with the name
    'Cert' does not exist" and the certificate looks missing while it is sitting in the store.
    X509Store needs no module. The PSModulePath handed to the child is reset for the same reason.

    And the script goes over base64 (-EncodedCommand), so nothing depends on how one process
    quotes a command for another.
    """
    ps = ("$s = New-Object System.Security.Cryptography.X509Certificates.X509Store 'My','CurrentUser';"
          "$s.Open('ReadOnly');"
          "$c = $s.Certificates | Where-Object { $_.Thumbprint -eq '%s' } | Select-Object -First 1;"
          "if (-not $c) { exit 2 };"
          "if (-not $c.HasPrivateKey) { exit 3 };"
          "Write-Output $c.Subject" % thumbprint)
    encoded = base64.b64encode(ps.encode('utf-16-le')).decode('ascii')
    entorno = dict(os.environ)
    system_root = entorno.get('SystemRoot', r'C:\Windows')
    entorno['PSModulePath'] = ';'.join((
        str(Path(entorno.get('ProgramFiles', r'C:\Program Files')) / 'WindowsPowerShell' / 'Modules'),
        str(Path(system_root) / 'system32' / 'WindowsPowerShell' / 'v1.0' / 'Modules'),
    ))
    p = subprocess.run(['powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass',
                        '-EncodedCommand', encoded],
                       capture_output=True, text=True, errors='replace', env=entorno)
    if p.returncode == 2:
        sys.exit('No certificate with thumbprint %s in the current user\'s "My" store.\n'
                 'docs/INSTALL.md section 4 creates one in a single command.' % thumbprint)
    if p.returncode == 3:
        sys.exit('That certificate has no private key, so it cannot sign. Import the .pfx into\n'
                 'the current user\'s Personal store.')
    if p.returncode:
        sys.exit('Could not read the certificate store: %s' % p.stderr.strip())
    return p.stdout.strip()


def check_manifest(manifest, subject):
    text = manifest.read_text(encoding='utf-8')
    publisher = re.search(r'<Identity\b[^>]*?Publisher="([^"]*)"', text, re.S)
    name = re.search(r'<Identity\b[^>]*?Name="([^"]*)"', text, re.S)
    if not publisher or not name:
        sys.exit('%s has no <Identity> with Name and Publisher.' % manifest)
    if 'CHANGE-ME' in name.group(1) or 'CHANGE-ME' in publisher.group(1):
        sys.exit('The manifest still says CHANGE-ME. Put your own Name and Publisher in\n'
                 'package/Package.appxmanifest first: the comment above <Identity> says what\n'
                 'each one does and why the Name is worth choosing once.')
    if publisher.group(1).strip() != subject.strip():
        sys.exit('The manifest Publisher and the certificate subject are not the same string:\n'
                 '  manifest:    %s\n  certificate: %s\n'
                 'The console compares them literally and rejects the package if they differ.'
                 % (publisher.group(1), subject))
    print('  manifest: Publisher matches the certificate subject')
    return text


def run(cmd, what):
    p = subprocess.run([str(c) for c in cmd], capture_output=True, text=True, errors='replace')
    if p.returncode:
        print(p.stdout[-2000:], p.stderr[-2000:], file=sys.stderr)
        sys.exit('%s failed (exit %d)' % (what, p.returncode))
    print('  %s ok' % what)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--exe', required=True, type=Path, help='the executable you built')
    ap.add_argument('--rom', required=True, type=Path, help='your own .sfc dump')
    ap.add_argument('--rom-name', default='sm.sfc',
                    help='name the ROM takes inside the package (default: sm.sfc, which is '
                         'what the patched main.c looks for)')
    ap.add_argument('--tiles', required=True, type=Path,
                    help='folder with your own 13 PNG tiles (docs/ASSETS.md)')
    ap.add_argument('--extra', type=Path, nargs='*', default=[],
                    help='files that travel next to the exe, e.g. the UWP SDL3.dll')
    ap.add_argument('--manifest', type=Path, default=REPO / 'package' / 'Package.appxmanifest')
    ap.add_argument('--certificate', default=os.environ.get('UWP_CERT_THUMBPRINT'),
                    help='SHA-1 thumbprint of your signing certificate, or set '
                         'UWP_CERT_THUMBPRINT')
    ap.add_argument('--version', default='0.1.0.0',
                    help='package version, four numbers (default 0.1.0.0)')
    ap.add_argument('--work', required=True, type=Path, help='scratch folder, rebuilt each run')
    a = ap.parse_args()

    if not a.certificate:
        sys.exit('Pass --certificate <thumbprint> or set UWP_CERT_THUMBPRINT. '
                 'docs/INSTALL.md section 4.')
    if not re.fullmatch(r'\d+\.\d+\.\d+\.\d+', a.version):
        sys.exit('--version takes four numbers, like 0.1.0.0')
    for p in (a.exe, a.rom, a.manifest):
        if not p.is_file():
            sys.exit('missing: %s' % p)
    if not a.tiles.is_dir():
        sys.exit('missing tiles folder: %s\nSee docs/ASSETS.md: this repository ships no game '
                 'artwork, you supply your own.' % a.tiles)
    absent = [t for t in TILES if not (a.tiles / t).is_file()]
    if absent:
        sys.exit('These tiles are missing from %s:\n  %s\ndocs/ASSETS.md lists every size.'
                 % (a.tiles, '\n  '.join(absent)))
    for t in TILES:
        if (a.tiles / t).read_bytes()[:8] != b'\x89PNG\r\n\x1a\n':
            sys.exit('%s is not a PNG file.' % (a.tiles / t))

    sdk = find_sdk()
    print('Windows SDK :', sdk)
    print('exe         :', a.exe)
    print('rom         :', a.rom.name, '->', a.rom_name)
    print('version     :', a.version)
    subject = cert_subject(a.certificate)
    print('certificate :', subject)
    check_exe(a.exe, a.extra)
    manifest_text = check_manifest(a.manifest, subject)

    content = a.work / 'content'
    if content.exists():
        shutil.rmtree(content)
    content.mkdir(parents=True)
    shutil.copy2(a.exe, content / a.exe.name)
    shutil.copytree(a.tiles, content / 'Assets')
    shutil.copy2(a.rom, content / a.rom_name)
    for extra in a.extra:
        if not extra.is_file():
            sys.exit('missing --extra file: %s' % extra)
        shutil.copy2(extra, content / extra.name)

    # The version lives in the manifest; it is replaced here so that the copy in git stays at
    # the placeholder and two packages never claim the same version by accident.
    stamped, count = re.subn(r'(<Identity\b[^>]*?Version=")[^"]*(")',
                             lambda m: m.group(1) + a.version + m.group(2),
                             manifest_text, count=1, flags=re.S)
    if count != 1:
        sys.exit('Could not set the version in the manifest (found %d places to change).' % count)
    (content / 'AppxManifest.xml').write_text(stamped, encoding='utf-8')

    config = a.work / 'priconfig.xml'
    if config.exists():
        config.unlink()
    run([sdk / 'makepri.exe', 'createconfig', '/cf', config, '/dq', 'en-US', '/o'],
        'makepri createconfig')
    run([sdk / 'makepri.exe', 'new', '/pr', content, '/cf', config,
         '/of', content / 'resources.pri', '/o'], 'makepri new')

    msix = a.work / ('%s_%s_x64.msix' % (a.exe.stem, a.version))
    run([sdk / 'makeappx.exe', 'pack', '/o', '/d', content, '/p', msix], 'makeappx pack')
    run([sdk / 'signtool.exe', 'sign', '/fd', 'SHA256', '/sha1', a.certificate, '/s', 'My', msix],
        'signtool sign')
    print('\nDone: %s (%s bytes)' % (msix, format(msix.stat().st_size, ',')))
    print('Install it through the Device Portal: docs/INSTALL.md section 5.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
