# Dependencies

*(Español: [DEPENDENCIES.es.md](DEPENDENCIES.es.md))*

Everything here was measured on the machine this port was built on. Where a version matters, it
says why; where it does not, it says that too, so you do not go hunting for an exact number that
makes no difference.

## The game project

| | |
|---|---|
| Repository | [mstan/SuperMetroidRecomp](https://github.com/mstan/SuperMetroidRecomp) |
| Tag | **`v0.3.1`** — the patches are generated against this tag. On a later one they may still apply, and `apply_uwp.py` tells you before it writes anything instead of leaving you half-patched |
| Submodule `snesrecomp` | Commit `01f3c73`, which is the one `v0.3.1` pins. This is the engine, and `02-engine-snesrecomp.patch` applies **inside** it |
| Submodule `recomp-ui` | Commit `76392fb`. **It has to be initialised even though nothing links against it:** `src/sm_mods.h` includes `recomp_launcher.h` for its types alone, and without the submodule the build stops at `fatal error C1083: Cannot open include file: 'recomp_launcher.h'`. This is the one step that is easy to miss, because the game's desktop build does not need it |

```powershell
git clone --branch v0.3.1 https://github.com/mstan/SuperMetroidRecomp.git <checkout>
cd <checkout>
git submodule update --init --depth 1 snesrecomp
git submodule update --init --depth 1 recomp-ui
```

A nested submodule, `retcomm-rbengine`, has no URL in `.gitmodules`; `--recurse-submodules` will
complain about it. Initialising the two above by name avoids the noise, and nothing in this recipe
needs any other.

**Keep the path short.** `<checkout>` at something like `C:\smr` or `D:\games\smr` saves you a build that
fails at link time with `LNK1104` for no visible reason: the recompiler's Rust analyser hits
`MAX_PATH` on a long one.

## Your own ROM

| | |
|---|---|
| File | A `.sfc` dump of *Super Metroid (Japan, USA) (En,Ja)* |
| SHA-1 | `da957f0d63d14cb441d215462904c4fa8519c613` |
| Size | 3,145,728 bytes |

The dump has to sit in the root of your checkout with that exact name for the project's own
regeneration script to find it. Nothing about it is distributed here; the hash is a fingerprint,
so that you can tell whether your copy is the one the recipe was built against.

## Build tools

| | Version used | Does the version matter? |
|---|---|---|
| Visual Studio 2022 | Community, MSVC 14.44 | **No**, but the *environment* does: `vcvarsall.bat x64 store`, never `vcvars64.bat`. That is what links against the `_app` CRT, which is the one the console has. With the desktop CRT the package installs and then refuses to start (`0x8027025B`). The C++ **UWP workload is not needed**: the packaging here is done by the SDK tools, not MSBuild |
| Windows SDK | 10.0.26100.0 | **No.** `package_uwp.py` looks for the newest SDK that has `makeappx`, `makepri` and `signtool`, and uses that one |
| CMake | 3.20 or newer | Ships with Visual Studio 2022 |
| Ninja | Any | Ships with Visual Studio 2022. ⚠ Its WindowsStore generator does **not** pass `/APPCONTAINER`; `src/CMakeLists.txt` passes it explicitly, and `package_uwp.py` refuses to package an executable without that bit |
| Python 3 | 3.13 | **No.** Standard library only, nothing to install. 3.8 or newer will do |
| Rust (`cargo`) | Any recent stable | Needed once: the recompiler's analysis backend is a Rust binary the project builds for you |
| `bash` | Git for Windows | The project's regeneration script is a shell script |
| PowerShell | The 5.1 that Windows already ships | **PowerShell 7 is not a dependency.** The examples carry `-ExecutionPolicy Bypass` because a stock Windows is set to `Restricted` and will not run a `.ps1` otherwise |

## SDL3, built for UWP

This is the dependency people get wrong, because a desktop SDL3 looks like it should work.

| | |
|---|---|
| What it has to be | An SDL3 built for WindowsStore: `APPCONTAINER` bit set, the `_app` CRT, and the WinRT video backend. SDL's own desktop binaries are none of those |
| What you need from it | `lib/SDL3.lib` to link against, `include/` to compile against, and `bin/SDL3.dll` **inside the package** — this game links SDL dynamically |
| Version used here | SDL 3.x built from source with the `store` environment. The `SDL3.dll` that shipped is 1,893,888 bytes |

If it is missing from the package the console installs it happily and then fails to launch with the
same `0x8027025B` a wrong CRT gives, which is what makes it so confusing. `package_uwp.py` reads
the executable's imports and stops if any DLL that is not on the console is also not in the
package.

## The console

| | |
|---|---|
| Hardware | Xbox Series X or Series S |
| Mode | **Developer Mode**, with Dev Home installed |
| Access | The Device Portal over your network, and a user signed in on the console — with no user signed in, a package installs and then does nothing when opened |
| Dependency on the console | `Microsoft.VCLibs.140.00` from 14.0.33519.0. It is declared in the manifest and installed alongside the package, not bundled inside it |
