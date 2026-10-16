# tools/

*(Español: [README.es.md](README.es.md))*

Two scripts. Both are plain Python 3 with nothing but the standard library, both take every path as
an argument — there is no default pointing at anyone's machine — and both check before they write.

| Script | What it does |
|---|---|
| `apply_uwp.py` | Applies this repository's two patches to **your** checkout of the game project. `--check` reports without writing, `--revert` undoes, `--list` explains each patch. Idempotent: a second run says `already applied` |
| `package_uwp.py` | Turns what you built into a signed `.msix`: `makepri` → `makeappx` → `signtool`. Finds the Windows SDK itself |

```powershell
python tools\apply_uwp.py <your checkout>
python tools\package_uwp.py --exe <build>\SuperMetroidRecomp.exe --rom "<your rom>.sfc" ^
    --extra <uwp sdl3>\bin\SDL3.dll --tiles <your tiles> --certificate <thumbprint> ^
    --work <scratch folder>
```

The certificate thumbprint can come from the `UWP_CERT_THUMBPRINT` environment variable instead of
the command line, so it need not end up in your shell history.

## What `package_uwp.py` checks before it packages anything

These are the point of the script. Each one stands for a day lost to a console that installs a
package happily and then will not start it:

1. the **`APPCONTAINER`** bit on the executable;
2. the **`_app` CRT**, which is what tells a `vcvarsall x64 store` build from a `vcvars64` one;
3. **every DLL the executable imports** either inside the package or known to be on the console —
   SDL3 is the one that bites, because this game links it dynamically;
4. the manifest's **`Publisher`** against the subject of the certificate about to sign it, compared
   as strings, which is how the console compares them;
5. the **thirteen tiles**, by name and by PNG header.

It writes nothing outside `--work`, and rebuilds that folder's `content/` on every run so a leftover
from a previous attempt cannot end up inside a signed package.

## A note on language

These two scripts are in English, comments included. What is in Spanish is the code **inside the
patches** — the comments that go into the game tree were written there and are kept as they were
compiled and verified, rather than retyped for publication.
