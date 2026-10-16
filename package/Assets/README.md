# This folder is empty on purpose

*(Español: [README.es.md](README.es.md))*

The thirteen package tiles are **not** in this repository. The ones used on the real console were
made from *Super Metroid*'s own artwork, which belongs to Nintendo, and this repository distributes
nothing of theirs ([`../../NOTICE.md`](../../NOTICE.md)).

You supply your own and pass the folder with `--tiles`:

```powershell
python tools\package_uwp.py ... --tiles <your folder>
```

[`../../docs/ASSETS.md`](../../docs/ASSETS.md) lists the thirteen file names and the exact sizes.
`package_uwp.py` checks every one of them, by name and by PNG header, and stops with the list of
what is missing instead of producing a package the console shows with no icon.

Only two of the thirteen are really visible: `Square150x150Logo` (the tile in My games & apps) and
`SplashScreen` (the loading screen).

The `.gitignore` blocks `package/Assets/*.png`, so dropping your tiles straight in here is safe —
git will not pick them up and you cannot publish them by accident.
