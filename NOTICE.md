# Notice

*(Español: [NOTICE.es.md](NOTICE.es.md))*

## Not affiliated

This is an unofficial, noncommercial fan project. It is **not affiliated with, endorsed by or
connected to Nintendo or Microsoft**, nor to the authors of
[SuperMetroidRecomp](https://github.com/mstan/SuperMetroidRecomp),
[snesrecomp](https://github.com/mstan/snesrecomp) or [SDL](https://github.com/libsdl-org/SDL).
*Super Metroid*, *Xbox* and all related names and marks are the property of their respective
owners.

Nothing here is for sale, and nothing here is monetized.

## What this repository does not distribute

No ROM. No game code of Nintendo's. No recompiled game sources. No built executable. No `.msix`
package. No save data. No package tiles — those were made from the game's own artwork. No signing
certificate, and no thumbprint of one.

The `.gitignore` blocks all of it by extension, so it cannot slip in by accident. That is a
safeguard, not a substitute for reading this file.

## What it does contain of other people's work

Measured, file by file, so that "we distribute nothing of theirs" is something you can check rather
than something you have to believe:

| | Lines | What it is |
|---|---|---|
| Our own code | 937 | `src/uwp_entry.cpp`, `src/uwp_shaders.h`, `src/uwp_shaders.hlsl` and `src/CMakeLists.txt`: the Direct3D 11 presenter, the WinRT entry point and the build target. Written here |
| Our own tooling | 422 | `tools/apply_uwp.py` and `tools/package_uwp.py` |
| Our own lines inside the patches | 300 | What the patches **add** to your checkout, all of it behind `#if defined(SM_UWP)` |
| Quoted from SuperMetroidRecomp and snesrecomp | 183 | The **context lines** a unified diff has to carry to know where a change goes, plus 9 lines the patches remove. Nothing else of theirs is here |

Those 183 lines are quoted from projects licensed under the
[PolyForm Noncommercial License 1.0.0](https://polyformproject.org/licenses/noncommercial/1.0.0),
which permits changes and new works for noncommercial purposes, and this repository is released
under that same license to match. Its `Required Notice:` line is in [`LICENSE`](LICENSE).

The ROM, and only the ROM, is yours to supply, and it never travels through here. What the recipe
does carry about it is a **SHA-1 hash** — a fingerprint, not content — so that you can tell whether
the dump you have is the one this was built against.

## Takedown

If you hold rights to any of the above and want something removed, write to
**jajdpmail@gmail.com** and it will be taken down. No argument, no delay.
