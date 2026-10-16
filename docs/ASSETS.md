# Package tiles — the thirteen files you supply

*(Español: [ASSETS.es.md](ASSETS.es.md))*

This repository ships **no artwork**. The tiles on the real console were made from *Super Metroid*'s
own art, which belongs to Nintendo, so they are not here and `package/Assets/` is empty on purpose
([`../NOTICE.md`](../NOTICE.md)).

You put thirteen PNG files in a folder of your own and pass it with `--tiles`.
`package_uwp.py` checks all thirteen by name and by PNG header, and stops with the list of what is
missing rather than building a package the console shows with no icon.

## The thirteen, with the sizes that were used

| File | Size | What the console does with it |
|---|---|---|
| `Square150x150Logo.png` | 150×150 | **The tile you actually see** in My games & apps |
| `Square150x150Logo.scale-200.png` | 300×300 | The same at 200% scale, which is what a 4K dashboard picks |
| `SplashScreen.png` | 620×300 | **The loading screen**, the other image you really see |
| `SplashScreen.scale-200.png` | 1240×600 | The same at 200% |
| `Square44x44Logo.png` | 44×44 | Small icon, in lists |
| `Square44x44Logo.scale-200.png` | 88×88 | The same at 200% |
| `Square44x44Logo.targetsize-24_altform-unplated.png` | 24×24 | Small icon with no plate behind it |
| `StoreLogo.png` | 50×50 | The `<Logo>` in the manifest |
| `StoreLogo.scale-200.png` | 100×100 | The same at 200% |
| `LockScreenLogo.png` | 24×24 | Declared for completeness; a console does not use it |
| `LockScreenLogo.scale-200.png` | 48×48 | Idem |
| `Wide310x150Logo.png` | 310×150 | The wide tile: declared in the manifest, **not drawn** by the console |
| `Wide310x150Logo.scale-200.png` | 620×300 | Idem |

Only two of them matter visually: `Square150x150Logo` and `SplashScreen`. The rest exist because
the manifest names them and the packaging step indexes them.

## Making them

Any image editor will do. Three things that are easy to get wrong:

- **PNG, with real transparency where you want it.** `BackgroundColor` in the manifest is
  `transparent`, so the tile shows the dashboard behind whatever is transparent in your image.
- **The `.scale-200` files are a separate image at double the size**, not the same file renamed.
  `makepri` indexes them as alternatives and the console picks per display.
- **Exact sizes.** The console does not rescale them for you; an odd size shows up cropped or
  padded.

Do not put EXIF or text chunks in them. A PNG exported by a screenshot tool can carry a path or a
machine name inside, which is a strange thing to publish along with an image.
