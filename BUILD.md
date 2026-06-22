# Building & publishing Tens

Tens is a Pebble Time 2 (**emery**, 200×228, 64-color) watchface. The `src/c`
runtime is a hand-written port of the Python design in `python/` — the Python
preview (`previews/current.png`) is the visual source of truth; the C renders
the same scene live from the clock and stored settings.

## 1. Install the SDK (Core Devices / rePebble, 2025+)

Requires **Python 3.13** (pebble-tool doesn't support 3.14 yet) and [uv](https://docs.astral.sh/uv/).

```bash
uv tool install pebble-tool
pebble sdk install latest      # also installs the arm toolchain + QEMU
```

Docs: https://developer.repebble.com/sdk/

## 2. Build

From the repo root (where `package.json` and `wscript` live):

```bash
pebble build
```

This compiles `src/c/**/*.c` for emery and bundles `src/pkjs/**/*.js`
(Clay config) into `build/tens.pbw`.

> The first build runs `npm install` for the `pebble-clay` dependency declared
> in `package.json`. If your environment blocks that, run `npm install` once in
> the repo root first.

## 3. Test

```bash
pebble install --emulator emery       # run in the QEMU emulator
pebble install --phone <ip>           # sideload to a watch via the phone app
```

Open **Settings** (the gear in the emulator/app) to exercise the Clay config
page — every toggle maps to a `UserConfig` option (rainbow, dark mode, layout
4x6/6x4, hours direction, fill invert, missing style, birth date, life span).

## 4. Project layout

```
package.json          # manifest: uuid, targetPlatforms ["emery"], messageKeys
wscript               # standard SDK build driver
src/c/
  main.c              # lifecycle, MINUTE_UNIT tick, AppMessage inbox
  settings.{h,c}      # TensSettings <-> persistent storage + config dict
  layout.{h,c}        # 200x228 geometry (mirrors python layout.py)
  derived.{h,c}       # calendar math: indices, fractions, week/year segments
  palette.h           # GColor mapping + spectral ramp (mirrors palette.py)
  render.{h,c}        # draws the scene (mirrors scene.py build_scene)
src/pkjs/
  index.js            # Clay -> AppMessage bridge
  config.js           # the settings page definition
```

## 5. Submit to the official Pebble appstore

Target store: **https://apps.repebble.com/faces** via the rePebble developer
portal.

1. **UUID** — `package.json` ships a generated UUID
   (`0f61229e-…`). The store requires it to be unique and unused; if it's ever
   rejected, regenerate with `uuidgen` (or `python -c "import uuid;print(uuid.uuid4())"`)
   and bump the `version`.
2. **Build a release** `.pbw` (`pebble build` → `build/tens.pbw`).
3. **Create the listing** in the developer portal: title (*Tens*), source URL
   (this repo), support email, category *Watchfaces*.
4. **Upload the `.pbw`** as a release and publish it.
5. **Asset collection** for the emery platform: a description (≤1600 chars),
   up to 5 screenshots (use `pebble screenshot` from the emulator/watch), and a
   marketing banner.
6. **Publish** (public or private).

> The community Rebble store (dev-portal.rebble.io) accepts the same `.pbw` if
> you ever want to cross-list to older watches — but that would need a
> responsive layout for the 144×168 platforms (see below).

## Known gaps / next steps

- **emery + basalt.** Geometry is responsive: emery (200×228) uses the original
  10px boxes; the 144×168 displays (basalt / Pebble Time + Pebble Time Steel)
  use a proportionally smaller 7px field, centered from the live canvas bounds.
  Round (chalk) and the B&W aplite/diorite platforms aren't enabled yet — chalk
  needs the rectangular grid inset to clear the circle, and the color-centric
  bars would collapse to black/white on the 1-bit displays.
- **Rainbow/life dithering.** On device the spectral ramp is drawn per-column
  with nearest-color quantization (no Floyd–Steinberg), so it bands more than
  the Python preview. To match the preview, precompute dithered bitmaps from
  the same gradient and bundle them as resources.
- **Palette parity.** `palette.h` hardcodes the current Python palette values.
  If you retune colors in `python/src/tens/palette.py`, mirror them here.
```
