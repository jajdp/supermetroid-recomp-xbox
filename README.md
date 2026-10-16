# Super Metroid on Xbox — the UWP port

How to take the [SuperMetroidRecomp](https://github.com/mstan/SuperMetroidRecomp) static
recompilation of *Super Metroid* (Super Nintendo, 1994) and run it **on an Xbox Series in Developer
Mode**, as a UWP package you build and sign yourself.

*(Español: [README.es.md](README.es.md))*

It is not a theory: the game boots, runs with a controller in **16:9**, keeps the simulation at
**58.3 frames per second** while presenting at 59 Hz, shows its frame rate on screen when you click
the right stick, and comes back where you left it after the console terminates it.

## What this repository is

**A recipe and the tools that apply it.** Nobody had published a UWP build of this game, so the port
is ours: a Direct3D 11 presenter that draws on the console's CoreWindow, a WinRT entry point, and
the changes the engine needed to survive inside an AppContainer. Two patches, four source files, two
scripts.

Everything is applied **to your own checkout** of the game project, on your machine. Every addition
is guarded by `#if defined(SM_UWP)`, so a patched checkout still builds and runs for the desktop
exactly as it did before.

## What this repository is **not**

It contains **no ROM, no game code, no built executable, no `.msix` package and no artwork**. It
cannot produce a playable game on its own, and nothing you can download here is a game. You need
your own legally dumped copy of the cartridge and a working desktop build of the game project
first. See [`NOTICE.md`](NOTICE.md).

The package tiles are **not** here either: the ones on the real console were made from the game's
own art, which is Nintendo's. You supply your own — [`docs/ASSETS.md`](docs/ASSETS.md) lists the
exact thirteen files and sizes, and `package_uwp.py` stops with a clear message if one is missing.

## What you need first

| | |
|---|---|
| The game project | [mstan/SuperMetroidRecomp](https://github.com/mstan/SuperMetroidRecomp) `v0.3.1`, cloned with its `snesrecomp` submodule, **building and running on your PC first** |
| Your own ROM | A `.sfc` dump of *Super Metroid (Japan, USA)*. SHA-1 `da957f0d63d14cb441d215462904c4fa8519c613` is the one this was built against |
| Visual Studio | 2022, C++ tools. What matters is **which environment you build in**: `vcvarsall.bat x64 store`, never `vcvars64.bat` ([`docs/DEPENDENCIES.md`](docs/DEPENDENCIES.md)) |
| Windows SDK | Any recent 10/11 SDK; `package_uwp.py` finds the newest one itself |
| CMake and Ninja | Both ship with Visual Studio 2022 |
| Rust (`cargo`), `bash`, Python 3 | The recompiler's analysis backend is native code, and its regeneration script is a shell script. Python is only the standard library |
| SDL3 built for UWP | The desktop SDL3 will not do. [`docs/DEPENDENCIES.md`](docs/DEPENDENCIES.md) says what it has to be |
| An Xbox in Developer Mode | And a certificate to sign with — a self-signed one is fine ([`docs/INSTALL.md`](docs/INSTALL.md) section 4) |

## How it goes

Four steps, and the long version of each one is in [`docs/INSTALL.md`](docs/INSTALL.md).

> **Where to type this:** in **PowerShell** — press the **Windows** key, type `powershell` and press
> **Enter** — in an **ordinary** window: you do not need "run as administrator", and using it would
> break the signing step, because the certificate lives in the store of whoever runs the command.
> From this repository's folder.

```powershell
# 1. generate the recompilation in your checkout (bash, and the NATIVE analysis backend)
#    See docs/INSTALL.md section 2: the python backend produces a build that runs at 4 fps.

# 2. patch your checkout (idempotent, checked before anything is written)
python tools\apply_uwp.py <path-to-your-checkout>

# 3. configure and build, in a window where you ran `vcvarsall.bat x64 store`
cmake -S src -B <short-build-path> -G Ninja -DCMAKE_SYSTEM_NAME=WindowsStore ^
      -DCMAKE_SYSTEM_VERSION=10.0 -DCMAKE_BUILD_TYPE=Release ^
      -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL ^
      -DSM_SRC=<path-to-your-checkout> -DSDL3_UWP=<path-to-uwp-sdl3>
cmake --build <short-build-path> --target sm_uwp

# 4. package and sign (your own tiles, your own certificate)
python tools\package_uwp.py --exe <short-build-path>\SuperMetroidRecomp.exe ^
       --rom "<your rom>.sfc" --extra <path-to-uwp-sdl3>\bin\SDL3.dll ^
       --tiles <your tiles folder> --certificate <your thumbprint> --work <scratch folder>
```

`python tools\apply_uwp.py --list` prints both patches and what each one solves.

Then install the `.msix` through the Device Portal and copy the two settings files from
[`config/`](config/) into the app's `LocalState`. Step by step, including the console side:
**[`docs/INSTALL.md`](docs/INSTALL.md)** ([en español](docs/INSTALL.es.md)).

## What it had to solve

Every one of these was a real failure on the console, and most of them hid the next. The detail,
with what the log said in each case and what was measured, is in
**[`docs/HOW-IT-WORKS.md`](docs/HOW-IT-WORKS.md)**.

| | Symptom | Fix |
|---|---|---|
| 1 | `SDL_CreateRenderer` failed: "Couldn't get window handle" | SDL's D3D11 backend wants an HWND, and UWP has a CoreWindow instead. The engine already had a presenter connector (`RendererFuncs`), so the port plugs its own Direct3D 11 presenter into it |
| 2 | The console installed the package and then would not start it (`0x8027025B`) | Two separate causes wearing the same error: the executable imported the **desktop** CRT, and later, that `SDL3.dll` was not inside the package. Both are checked before signing now |
| 3 | The console rejected the package on activation | The `APPCONTAINER` bit was missing. The Ninja generator for WindowsStore does not add it on its own |
| 4 | **~4 fps**, and the game never got past the intro | The recompilation had been generated with the `python` analysis backend: 1,807 AOT variants against 864 from the native one, and at least one of them is generated wrong — it corrupts the guest stack. **The compiler was not involved**, which took a wrong turn to find out |
| 5 | Pressing **B** closed the game | On an Xbox, B is also the system "back", and the shell takes the app down unless it says it handled the key |
| 6 | The game jumped to the ROM picker on the console | It re-anchors its working directory to the executable's, which is read-only in an installed package. It is launched with `--config <absolute path>` and the ROM as a positional argument |
| 7 | The ROM picker dragged `COMDLG32.dll` into the import table | That DLL does not exist inside an AppContainer. The picker is compiled out |
| 8 | `__attribute__((constructor))` would not compile | It is a GCC extension. The same job is done with a pointer in the `.CRT$XCU` section, which is what the framework itself does elsewhere |
| 9 | The crash dump needed `dbghelp` and `tlhelp32` | Neither exists inside an AppContainer. The file already had a portable fallback; UWP takes it |
| 10 | **No** controller button did anything | SDL reports the pad with player index 1 (zero-based) and the game takes that index as the first free slot, so the only pad landed in SNES port 2. `EnableGamepad2 = false` is what makes the controller work — see [`config/config.ini`](config/config.ini) |
| 11 | 16:9 cost 8-10% of the simulation (52.9 fps) | Rasterising, composing and presenting took ~10 ms of a 14 ms frame; emulation was not the problem. Presentation moved to its own thread: **58.3 fps**, and the cost of presenting fell from 4.26 ms to 0.03 |
| 12 | No way to know the frame rate | A console has no title bar, which is where the runtime writes it. An on-screen counter, toggled with **R3** |
| 13 | Coming back from Home started the game over | Developer Mode *terminates* a suspended game. The state is saved while it runs and reloaded when the system relaunches it, which needs a host-side reconciliation hook (`src/sm_cpu_infra.c`) the engine leaves for the game to fill in |

## How it was verified

Not by reading it: by rebuilding it. On a fresh `v0.3.1` clone, this recipe reproduces the tree the
shipped package was built from — **7 of 7 files identical byte for byte**, measured, including the
one file the project's own saved patches had been missing. The numbers and the method are in
[`docs/HOW-IT-WORKS.md`](docs/HOW-IT-WORKS.md).

## Credits and license

Port and tooling by **Recompilaciones**. Released under the
[PolyForm Noncommercial License 1.0.0](LICENSE), matching the license of the SuperMetroidRecomp
project and the snesrecomp engine this is a derivative of.

The patches carry their comments in Spanish, which is where they were written; the documentation is
in both languages.

*Super Metroid* is © Nintendo. This project is not affiliated with them, with Microsoft, or with the
authors of the game project or the engine, and it distributes nothing that belongs to them. The
details — exactly what is and is not included here, and how to ask for a takedown — are in
[`NOTICE.md`](NOTICE.md). Rights holders can write to **jajdpmail@gmail.com**.
