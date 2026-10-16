# How it works

*(Español: [HOW-IT-WORKS.es.md](HOW-IT-WORKS.es.md))*

What the port actually does, why each piece is the way it is, and what was measured. If you only
read one section, read [the last one](#how-this-was-verified): it is what gives anyone the right to
say the recipe works.

## The shape of the problem

A UWP process on an Xbox is not a desktop process with fewer privileges. Three differences decide
the whole port:

1. **It does not start at `main()`.** The system starts a `CoreApplication`, and the window is a
   `CoreWindow`, not an `HWND`.
2. **It runs inside an AppContainer.** `user32`, `comdlg32`, `dbghelp` and `tlhelp32` are not
   there. Code that merely *imports* one of them makes a package that installs and then refuses to
   activate.
3. **OpenGL is not allowed**, and SDL's own Direct3D 11 renderer asks for an `HWND` it cannot get.

The game, on the other hand, is a `main.c` of ~2,300 lines built on SDL, with its own renderer,
audio and input. So this port does **not** rewrite the host: it puts around it what UWP demands and
lets the game get on with it.

## The presenter

The engine already had the right seam: a `struct RendererFuncs` connector that the game fills in
with whatever draws the frame. The desktop build plugs its OpenGL presenter in there. The port
plugs in its own:

- [`src/uwp_entry.cpp`](../src/uwp_entry.cpp) creates the Direct3D 11 device against the
  `CoreWindow`, uploads the emulated PPU frame as a texture, and draws it with a full-screen
  triangle pair.
- [`src/uwp_shaders.hlsl`](../src/uwp_shaders.hlsl) is that pass; `uwp_shaders.h` is it compiled to
  bytecode so the build needs no shader compiler at run time.
- The entry point is `WinMain` → `SDL_RunApp` → the game's own main. SDL's UWP build carries the
  `CoreApplication` shell, so this is three lines rather than a rewrite.

The CRT is the **`_app`** one, which is what `vcvarsall.bat x64 store` links against, and the
executable carries the **`APPCONTAINER`** bit, which the Ninja generator for WindowsStore does not
add on its own (`src/CMakeLists.txt` passes `/APPCONTAINER` explicitly).

## The patches, one by one

`python tools/apply_uwp.py --list` prints this too.

**[`patches/01-game-src.patch`](../patches/01-game-src.patch)** — 261 lines added across three
files of the game:

- `src/main.c`: the UWP entry, the presenter wired into `RendererFuncs` instead of the OpenGL one,
  the console heartbeat that reports per-stage timings, the on-screen FPS counter, and
  `SM_RAM_DUMP` — a diagnostic that dumps the 128 KB of guest RAM on chosen frames, which is how
  "the CRC changed" became "these bytes changed".
- `src/post_mortem.c`: `dbghelp` and `tlhelp32` out, because they do not exist in an AppContainer.
  The file already had a portable dump in its `#else` branch; UWP takes that path.
- `src/sm_cpu_infra.c`: `SmOnStateLoaded`, the hook that makes suspension work. See below.

**[`patches/02-engine-snesrecomp.patch`](../patches/02-engine-snesrecomp.patch)** — 39 lines across
four files of the engine:

- `snes/interp_bridge.c`: `__attribute__((constructor))` is a GCC extension MSVC rejects. The same
  job is done with a pointer in the `.CRT$XCU` section, which is what the framework already does in
  `mod_runtime.h`.
- `runner/src/launcher_picker.c`: the ROM picker is compiled out. It drags `COMDLG32.dll` into the
  import table, and that is enough to make the package fail to activate.
- `runner/src/rom_image_verify.c`, `host_report.c`: no `user32`, no `MessageBox`; the breadcrumbs go
  to a trace file in the app data folder, which on a console is the only thing you can read
  afterwards.

Everything is behind `#if defined(SM_UWP)`, so your desktop build is untouched.

## Widescreen cost 8% of the simulation, and it was not emulation

With `EnhancedRenderer=0` and `Aspect=4:3` the console held a flat 60 fps. With the enhanced
renderer at 16:9 the **simulation** fell to 52-55 fps while **presentation** stayed at 58-59 Hz: no
visible stutter, the game just ran 8-10% slow.

The temptation is to blame the emulator. The per-stage heartbeat said otherwise: rasterising,
composing and presenting took about **10 ms of a 14 ms frame**. So presentation moved to its own
thread — `EndDraw` copies the finished frame into a hand-off buffer (~300 KB) and returns, and a
second thread presents it.

| | Without the thread | With it |
|---|---|---|
| Simulated frames per second | 52.9 | **58.3** |
| Cost of presenting, per frame | 4.26 ms | **0.03 ms** |
| Seconds at ≥59 fps | 84 of 120 | **115 of 130** |

It is a key, not a rebuild: `PresentThread=0` in `sm-video.ini` goes back to the synchronous path,
which is how the A/B was done. The frames that still dip are **the same exact frames** either way —
they are the game's (how much of it the recompiler could translate ahead of time), not the host's.

Two other measurements worth keeping, because both were plausible and both were wrong:

- **The package type does not matter.** Measured three times as `Game` and three as `App`: the
  spread between runs of the same type (3.5 fps) is larger than the distance between types.
- **The compiler did not matter either.** The first verdict blamed MSVC, from comparing *our tree
  built with MSVC* against *the project's official PC binary* — two variables at once. Building
  **our own tree** with GCC gives the same trace as MSVC. A stranger's binary is not a control for
  your tree.

## No controller button did anything

The map was right and the pad was dead. SDL reports the pad with **player index 1** (counting from
zero) and the game's `OpenOneGamepad()` takes that index as the first candidate slot: with player 2
enabled, the only controller landed in **SNES port 2**, which in *Super Metroid* does nothing.

`EnableGamepad2 = false` in [`config.ini`](../config/config.ini) discards that candidate and the pad
falls into player 1. The console's own log confirms it with `assigning to player 1`.

## Coming back from Home

In Developer Mode the console does not suspend a backgrounded game: it **terminates** it. So there
is nothing to resume — the game has to have saved itself.

`RtlLoadSnapshot` restores the SNES blob, but the **host** side of the execution state does not fit
in there, and the runner leaves a hook for the game to reconcile it. Without that hook the state
loaded and the game appeared at the intro, because `g_cpu.ram` still pointed where it pointed
before. `SmOnStateLoaded` (in `01-game-src.patch`) repoints it at the restored WRAM, resynchronises
the APU clock and clears the interpreter deadline.

## Line endings

`v0.3.1`'s `src/main.c` has **3 lines ending in CRLF** among 2,347 — and `01-game-src.patch` is the
hunk that removes them. That is why `.gitattributes` keeps patches out of the line-ending machinery
(`*.patch -text`): a clone gets the exact bytes `git diff` produced.

Measured, because the obvious story turned out to be wrong in an interesting way: a copy of the
patch flattened to LF **also applies**, and produces `main.c` byte for byte identical (0 CR, 2,577
LF, 100,854 bytes). But `git apply --reverse` of that flattened copy does **not** restore the three
CRs — it leaves the file 3 bytes shorter than upstream, and then the real patch no longer applies to
it. The tolerance is not symmetric, so the bytes are kept as they are.

## How this was verified

Not by reading it. On a **fresh clone of `v0.3.1`**, on 2026-10-14:

| What was measured | Result |
|---|---|
| Patches apply to a clean checkout | Both, clean. `--check` first, nothing written on failure |
| Idempotence | Second run: `already applied`, nothing written |
| The generated sources this recipe produces, against the tree the shipped package came from | **54 of 54 identical**, byte for byte (`864 AOT-eligible, 142 LLE-only`, as upstream prints) |
| The patched sources, against that same tree | **7 of 7 identical**, byte for byte |
| Does it build? | Yes: 115 objects, links `SuperMetroidRecomp.exe` (~3.3 MB), 0 errors |
| Is the build deterministic? | The same tree compiled from **two different paths**: **93 of 93 objects with an identical `.text`**, 0 different |
| Package and signature | `makepri` → `makeappx` → `signtool`, a signed `.msix` of 6,497,222 bytes |
| Against the `.msix` that runs on the console (0.2.0.0) | 1,536 bytes more of `.text` and one extra string, `FPS readout %s`: a trace line added to the tree after that package was built. Everything else — the presentation thread, the diagnostics, the controller map, the suspension — is in both |

That last row is the honest one. The recipe reproduces **the project's current tree**, which is
slightly newer than the package installed on the console; it is not a divergence, and it is
measured rather than assumed.

One more thing this verification found, and it is the reason it was worth doing: the patches saved
in the project itself were **out of date**. They carried 121 of the 267 changed lines — `main.c`
partially, and `src/sm_cpu_infra.c`, the suspension hook, not at all. Anyone following them would
have built a game that loses its place every time the console reclaims it. The patches here were
regenerated from the tree that actually runs, which is why they are extracted with `git diff` and
never transcribed by hand.
