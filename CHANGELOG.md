# Changelog

All notable changes to this recipe. Dates are `YYYY-MM-DD`.

The version here is the **recipe's**, not the game's and not the package's. The package version is
whatever you pass to `package_uwp.py --version`.

## 1.0.1 — 2026-10-20

### Changed

- **The README now says which console this was tested on, and which one it was not.** Everything here
  was built and measured on an **Xbox Series X** in Developer Mode; a **Series S has never been tried**,
  so nothing here says anything about one. The wording used to say "Xbox Series", which reads as if it
  covered both, and it does not.

## 1.0.0 — 2026-10-14

First release. The port itself was built and has been running on two Xbox Series X consoles since
2026-09-11; this is the recipe for it, extracted, cleaned of anything local, and verified against a
fresh clone.

### Included

- `patches/01-game-src.patch` and `patches/02-engine-snesrecomp.patch`: 300 added lines over seven
  files of the game project and its engine, all behind `#if defined(SM_UWP)`.
- `src/`: the Direct3D 11 presenter on `CoreWindow`, the WinRT entry point, the shaders as source
  and as bytecode, and the CMake target — 937 lines.
- `tools/apply_uwp.py`: applies, reverts, lists and checks the patches. Idempotent, and it refuses
  to leave a checkout half-patched.
- `tools/package_uwp.py`: `makepri` → `makeappx` → `signtool`, with the five checks that stand
  between a package that installs and a package that starts.
- `package/Package.appxmanifest` with `CHANGE-ME` identity, `config/` with the two console settings
  files, and documentation in English and Spanish.

### Verified before release

On a fresh `v0.3.1` clone: both patches apply clean and are idempotent; the recipe reproduces the
tree the shipped package was built from, **54 of 54 generated sources and 7 of 7 patched sources
identical byte for byte**; the build links; the same tree compiled from two different paths gives
**93 of 93 objects with an identical `.text`**; and the whole chain ends in a signed `.msix` of
6,497,222 bytes. Detail in [`docs/HOW-IT-WORKS.md`](docs/HOW-IT-WORKS.md).

### Three things this release fixes that were wrong before it

- **The project's own saved patches were out of date** — 121 of 267 changed lines, missing the
  suspension hook entirely. Regenerated from the tree that actually runs.
- **`recomp-ui` has to be initialised**, even though nothing links against it, or the build stops at
  `recomp_launcher.h`. It was not written down anywhere.
- **`package_uwp.py` read the certificate store through the `Cert:` drive**, which does not exist in
  Windows PowerShell 5.1 on a machine whose `PSModulePath` has been rewritten by a PowerShell 7
  install: the certificate looked missing while it was sitting in the store. It now goes through
  .NET, which needs no module.
