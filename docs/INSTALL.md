# Install — from a clean checkout to the game running on the console

*(Español: [INSTALL.es.md](INSTALL.es.md))*

Eight sections. Sections 1 to 5 happen on your PC, 6 and 7 on the console. Everything here was run
end to end on a fresh clone before being written down; where a step has a trap, the trap is in the
step and not in a footnote.

## 0. Where you type these commands

Two kinds of window, and it matters which:

- **PowerShell** — press the **Windows** key, type `powershell`, press **Enter**. Used for the
  Python scripts, the certificate and the Device Portal.
- **A `cmd` window with the build environment loaded** — press the **Windows** key, type `cmd`,
  press **Enter**, then run the `vcvarsall.bat` line from section 3. Used for CMake and Ninja. The
  environment only lasts as long as that window.
- **Git Bash** — installed with Git for Windows. Used once, for the project's regeneration script
  in section 2, which is a shell script.

**You do not need "run as administrator" for any of this**, and using it would actually break
section 4: the certificate is created in the certificate store of whoever runs the command, so an
elevated window would put it somewhere the normal signing step cannot find it.

Commands that run a `.ps1` carry `-ExecutionPolicy Bypass`. A stock Windows refuses to run script
files at all (`Restricted`), and that flag applies to that one process only — it changes nothing on
your system and needs no administrator.

## 1. Clone the game project

```powershell
git clone --branch v0.3.1 https://github.com/mstan/SuperMetroidRecomp.git C:\smr
cd C:\smr
git submodule update --init --depth 1 snesrecomp
git submodule update --init --depth 1 recomp-ui
```

Three things worth knowing, each of which costs an hour if you find it the hard way:

- **Keep the path short** (`C:\smr`, not a folder deep inside Documents). The recompiler's analyser
  is a Rust binary that hits `MAX_PATH` on long paths and fails at link time with `LNK1104`.
- **`recomp-ui` has to be there even though nothing links against it.** `src/sm_mods.h` includes
  `recomp_launcher.h` just for its types. Without that submodule the build stops with
  `fatal error C1083: Cannot open include file: 'recomp_launcher.h'`. The desktop build of the game
  does not need it, which is why it is easy to miss.
- Do not use `--recurse-submodules`: a nested submodule (`retcomm-rbengine`) has no URL in
  `.gitmodules` and git will complain about it. The two above are all this recipe needs.

Then put your own ROM in the root of the checkout, with the exact name the project expects:

```
C:\smr\Super Metroid (Japan, USA) (En,Ja).sfc
```

Its SHA-1 should be `da957f0d63d14cb441d215462904c4fa8519c613`
([`DEPENDENCIES.md`](DEPENDENCIES.md)).

## 2. Generate the recompilation

In **Git Bash**, from the root of the checkout:

```bash
PYTHON="C:/Users/<you>/AppData/Local/Programs/Python/Python313/python.exe" \
SNESRECOMP_ANALYSIS_BACKEND=native \
bash tools/regen.sh --no-tests
```

`PYTHON` has to point at a **real** Python. The `python3` that Windows puts on the PATH by default
is a stub that opens the Microsoft Store, and the script will fail in a way that does not say so.

⚠ **`SNESRECOMP_ANALYSIS_BACKEND=native` is not optional.** It is the project's default, so you
only have to be careful not to override it — but if the `python` backend runs instead, you get a
build that compiles, installs, launches, and then sits at **4 frames per second** without getting
past the intro. It emits 1,807 AOT variants against 864 from the native backend, and at least one
of them is generated wrong: it corrupts the guest stack. Nothing in the symptom points at the
cause, and the frame rate makes you suspect the console, the compiler and the presenter first.

What a correct run ends with:

```
analysis: 864 AOT-eligible, 142 LLE-only
v2_emit: 218 roots, 864 exact AOT variants, 142 LLE variants
v2_emit: 52 bank(s) emitted, 0 reused
```

and **54 `.c` files** in `src/gen`. If you see other numbers, stop here: the rest of the recipe
will build something, and that something will not run properly.

## 3. Patch and build

From this repository's folder, in **PowerShell**:

```powershell
python tools\apply_uwp.py C:\smr
```

It checks both patches against your checkout before writing anything, and refuses to apply one if
the other cannot go in — half-patched is the one state worth protecting against. Run it twice and
the second run says `already applied` and does nothing. `--check` reports without writing,
`--revert` undoes it, `--list` prints what each patch changes and why.

Then, in a **`cmd` window**, load the build environment and build:

```
"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 store
```

```
cmake -S "<this repo>\src" -B C:\smu -G Ninja ^
  -DCMAKE_SYSTEM_NAME=WindowsStore -DCMAKE_SYSTEM_VERSION=10.0 ^
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL ^
  -DSM_SRC=C:/smr -DSDL3_UWP=<path to your UWP SDL3>/x64

cmake --build C:\smu --target sm_uwp
```

- **`x64 store`, not `vcvars64`.** That is what links against the `_app` CRT. With the desktop CRT
  the package installs and then the console says *"the app didn't start"* (`0x8027025B`).
- **`-DSM_SRC` and `-DSDL3_UWP` take forward slashes**, even on Windows.
- **Keep the build path short too** (`C:\smu`), same `MAX_PATH` reason as the checkout.
- The output is `C:\smu\SuperMetroidRecomp.exe`, around 3.3 MB.

## 4. A certificate to sign with

One command, in **PowerShell**, in an **ordinary** window (not administrator):

```powershell
New-SelfSignedCertificate -Type Custom -Subject "CN=<your name>" -KeyUsage DigitalSignature `
  -FriendlyName "<your name> UWP dev" -CertStoreLocation "Cert:\CurrentUser\My" `
  -TextExtension @("2.5.29.37={text}1.3.6.1.5.5.7.3.3", "2.5.29.19={text}")
```

Write down the `Thumbprint` it prints. Then open [`../package/Package.appxmanifest`](../package/Package.appxmanifest)
and replace the two `CHANGE-ME` values:

- `Publisher` must be **exactly** the `-Subject` above, letter for letter, `CN=` included. The
  console compares the two strings and rejects the package if they differ; `package_uwp.py` checks
  it for you before it packages anything.
- `Name` is the package's internal name. Together with the Publisher it forms the **family**, which
  is how the console recognises the game and where its `LocalState` — settings and save files —
  hangs from. Changing it later makes a second game as far as the console is concerned, with an
  empty folder, and leaves the old one installed. Pick it once.

A self-signed certificate is fine: a console in Developer Mode installs packages signed with one.

## 5. Package, sign, and install

Your own tiles first — this repository ships none, because the ones on the real console were made
from the game's artwork. [`ASSETS.md`](ASSETS.md) lists the thirteen files and their exact sizes.

```powershell
python tools\package_uwp.py --exe C:\smu\SuperMetroidRecomp.exe ^
  --rom "C:\smr\Super Metroid (Japan, USA) (En,Ja).sfc" ^
  --extra <path to your UWP SDL3>\x64\bin\SDL3.dll ^
  --tiles <your tiles folder> --certificate <your thumbprint> ^
  --version 0.1.0.0 --work C:\smu_pkg
```

Before it packages anything it checks the five things that make a package install and then not
start: the `APPCONTAINER` bit, the `_app` CRT, every imported DLL either in the package or on the
console, the Publisher against the certificate, and the thirteen tiles. The result is a signed
`.msix` of about 6.5 MB in `--work`.

Install it from the console's **Device Portal** (`https://<console ip>:11443`, with your Dev
Home username and password): **Add** → the `.msix` → and, as a dependency, the
`Microsoft.VCLibs.140.00` appx for x64 that comes with the Windows SDK. The console needs **a
user signed in**; with nobody signed in the package installs and then does nothing when you
open it.

## 6. The two settings files

They do **not** travel inside the package. They live in the app's data folder and survive
reinstalling it. Copy both from [`../config/`](../config/) to:

```
\LocalState\SuperMetroidRecomp\config.ini
\LocalState\SuperMetroidRecomp\sm-video.ini
```

through the Device Portal's file browser, then stop and relaunch the app so it reads them.

- [`config.ini`](../config/config.ini) is audio and the controller map. **`EnableGamepad2 = false`
  is what makes the controller work at all** — the reason is in the file and in
  [`HOW-IT-WORKS.md`](HOW-IT-WORKS.md).
- [`sm-video.ini`](../config/sm-video.ini) is the widescreen and the presentation thread.
  `PresentThread=1` is worth 5 frames per second; leave it at 1.

⚠ The game's own settings writer only knows its own five keys, so if it ever rewrites that file it
**drops the `PresentThread` line**. Nothing breaks: the built-in default is the right one.

## 7. Playing it

Connect the controller **before** launching, or the game will not see it (`No joysticks detected`).
Click the **right stick (R3)** for the on-screen frame rate counter.

Coming back from Home works: Developer Mode terminates a suspended game, so the port saves its
state while it runs and reloads it when the system relaunches it.

## 8. When something goes wrong

| What you see | What it is |
|---|---|
| `fatal error C1083: ... 'recomp_launcher.h'` | The `recomp-ui` submodule is not initialised (section 1) |
| `LNK1104` at link time | The checkout or the build path is too long (`MAX_PATH`) |
| *"the app didn't start"* / `0x8027025B` | Either the desktop CRT (built with `vcvars64`) or a missing DLL in the package. `package_uwp.py` catches both before signing |
| The console rejects the package on install | `Publisher` and the certificate subject are not the same string (section 4) |
| The console rejects it on activation | The `APPCONTAINER` bit is missing — a build that did not come from `src/CMakeLists.txt` |
| **4 fps** and it never passes the intro | The recompilation was generated with the `python` analysis backend (section 2) |
| Runs, but **no** controller button does anything | `EnableGamepad2 = false` is missing from `config.ini` (section 6) |
| It goes to a ROM picker | The ROM is not in the package under the name the patched `main.c` expects (`sm.sfc` by default, `--rom-name` to change it) |
| Pressing **B** closes the game | You are running something built without `01-game-src.patch` |
| The settings "came back" after reinstalling | `LocalState` survives reinstallation. Replace both files and relaunch |
| `No certificate with thumbprint ...` while it is clearly there | Your `PSModulePath` has been rewritten, usually by a PowerShell 7 install, and the `Cert:` drive is gone in 5.1. `package_uwp.py` reads the store through .NET for exactly this reason, so if you see this from some *other* script, that is the cause |
