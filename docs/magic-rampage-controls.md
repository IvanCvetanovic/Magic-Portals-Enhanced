# Magic Rampage's control buttons: where they come from and how they are made

This file records names, recipes, sizes and hashes. It holds no pixel and no image: the art itself
lives only in the gitignored `reference/magic-rampage/`, made by `tools/art/make_mr_controls.py`.

## Precondition (read this first)

The owner's request of 2026-10-15 ("Reuse the control buttons from Magic Rampage") is the go-ahead
for copying Magic Rampage's sprites into the gitignored `reference/magic-rampage/` for local use.
`docs/permission.md` is still the unfilled placeholder and says nothing about Magic Rampage (another
Asantee game). Nothing here is committed or shipped, and any release package containing
`reference/magic-rampage/` stays gated on the owner's word with the rights holder (the CTL design's
question Q2). Without that folder the modern controls fall back to art this repository draws itself
(CTL design, section 4.2); nothing else depends on it.

## What exists

```
reference/magic-rampage/              ignored by the unanchored `reference/` line of .gitignore
    assets/                           a verbatim copy of the sprites the tool and the editor stage read
        sprites/...  extract.json     (21 files; extract.json says the package, each file's sha256, its role)
    controls/                         the five controls, 128 x 128 straight-alpha RGBA PNG
        walk_left.png walk_right.png restart.png pause.png clear_portals.png  manifest.json
    editor/                           CTL-11 (`--stage editor`: "not yet", exits 2)
```

`assets/` is itself a valid `--assets` folder, so the original package (the `.xapk`, or another
session's extract of its base APK) is needed once. The first run, on 2026-10-15, read the base-APK
`assets/` of Magic Rampage (`com.asanteegames.magicrampage`, version_name 7.8.7, version_code 1214:
the `manifest.json` that sits beside that extract, copied from the `.xapk`) from the Penumbra
session's gitignored scratch extract; the `.xapk` is no longer on the machine. A folder package
carries no version of its own, so the tool's manifest records `expected_version` 7.8.7 and the
sha256 of every source it read; the `--xapk` reader, which can read the version, has only been
exercised against a zip-in-zip the tests build, because no real `.xapk` is on the machine.

## The five controls

All 128 x 128, 8-bit RGBA, straight (not premultiplied) alpha, colour under alpha 0 made black, no
pixel of exact magenta (255, 0, 255), and the same **body**: the solid 100 x 100 square (alpha >= 250)
inside the 14 px soft shadow every pad sprite carries, at **(14, 14, 114, 114)** (right and lower
edges exclusive; 14 / 128 = 0.109375 on each side). The layout reads its insets from the body, not
from the 128 px box, so the tool refuses a source whose body is anywhere else.

| Output | Sources (under the base APK's `assets/`) | Recipe |
|---|---|---|
| `walk_left.png` | `sprites/dpad-left.png` | as shipped: decoded, colour under alpha 0 made black, never resampled |
| `walk_right.png` | `sprites/dpad-right.png` | as shipped, the same |
| `restart.png` | `sprites/dpad-frame.png`, `sprites/icon-restart-64.png` | the frame, then the icon's white ink restyled as Magic Rampage's own pad icons are, in a 60 px box centred on the body |
| `pause.png` | `sprites/dpad-frame.png` | the frame, then two bars drawn by the tool in the same restyle: 12 x 40 px each, gap 10, corner radius 3 (design Q5: a framed square, not Magic Rampage's wide pill) |
| `clear_portals.png` | `sprites/dpad-frame.png` | the frame, then a ring (53 px across, 5 px stroke) with an X inside it (arms to +-8 px along both axes, 5 px stroke, round ends), drawn by the tool in the same restyle, concentric (design Q12; Magic Rampage has no such glyph, and the original game's own button puts its X at the ring's corner) |

**The restyle** (measured on `dpad-left.png`, the numbers are constants in the tool): fill grey 215,
black outline 3.5 px, shadow offset (3, 3) px, blur 3.5 px, strength 0.6, drawn at 4x supersample and
reduced with Lanczos, on a canvas that is an even number of output pixels so that centring on the
(even) body is exact. A glyph's own extent is centred (the shadow, which falls to the lower right,
is not counted). Measured on the real outputs: `pause` and `clear_portals` outline boxes centre on
(64.0, 64.0); `restart` on (64.0, 64.5) to the threshold the measure can resolve.

**`icon-restart-64.png` carries a dark soft shadow in its alpha.** It is a 64 x 64 grey + alpha
image: a white arrow (alpha 200 to 255, grey mostly 240 to 255) inside a baked-in dark halo (grey 0
to 77, alpha 1 to 25, over 467 of its 1,210 visible pixels). Its alpha box is 40 x 41 px, the white arrow's
31 x 33. The earlier figure "body (12,11)-(52,52)" (explorer C, 2.1) is the alpha box, halo included.
Scaling and centring on the alpha sized and placed the shadow, and its pale fringe muddied the
outline; so the tool takes the glyph's **white ink** (grey x alpha, normalised) as the mask. A glyph
with no halo is unchanged by this.

**Colour under alpha 0.** All 937 fully transparent pixels of `dpad-left.png`, `dpad-right.png` and
`dpad-frame.png` (their palette's transparent entry) carry one non-black colour, (71, 112, 76). The
outputs make it black (a filtered, magnified sprite would bleed it into
the edge). So `walk_left` and `walk_right` equal their sources exactly in every pixel with alpha > 0
(maximum difference 0), and in all pixels once the colour under alpha 0 is made black; compared with
the source decoded plainly, the maximum difference is 112, in RGB, in those 937 pixels only.

## What the tool refuses

* a run in which `git check-ignore` does not report as ignored **every** file about to be written,
  the five images and the manifest in `--out` and the extracted sprites in
  `reference/magic-rampage/assets` (a tracked path, a path outside the repository, no git): the
  check runs first, before the package is opened, so a refusal writes nothing. With no git on the
  `PATH` the tool says so and refuses, `--check` included. The repository root is `git rev-parse
  --show-toplevel` of the current directory, never a path computed from the script's own location;
* a source of another size (an as-shipped sprite is never resampled to fit), a source whose body is
  not (14, 14, 114, 114), an output with a pixel of exact magenta, or with colour under alpha 0;
* `--stage editor` and `--stage all`: "not yet: CTL-11" (exit 2). `--extract` already copies the
  sprites that stage will read.

It imports nothing but the standard library, numpy and Pillow, nothing from Penumbra (its helpers
were copied and rewritten, after `make_mr_touch_art.py` there), and no default points outside this
repository.

## How to run it

From inside this repository (the current directory decides the root), with the parity venv, which has
Pillow and numpy:

```
tools/parity/.venv/Scripts/python -I tools/art/make_mr_controls.py --assets <folder holding the base APK's assets/>
tools/parity/.venv/Scripts/python -I tools/art/make_mr_controls.py --xapk <Magic+Rampage_7.8.7_APKPure.xapk>
tools/parity/.venv/Scripts/python -I tools/art/make_mr_controls.py --check          # writes nothing
```

* No package argument: `reference/magic-rampage/assets`, the verbatim copy.
* `--out <dir>` (default `reference/magic-rampage/controls`) is where the five images and the
  manifest go. The sprites are copied into the fixed `reference/magic-rampage/assets` whatever
  `--out` is (never beside it: an `--out out/controls` leaves the converter's `out/assets`, and an
  `--out reference/extracted/controls` the port's `reference/extracted/assets`, alone), unless
  `--no-extract`, or unless the package already is that folder. Both destinations go through the
  ignore check below before anything is written.
* `--check` re-derives the five images in memory and compares them with the files on disk (bytes
  and pixels) and with `manifest.json` (sha256 of the file, sha256 of the pixels, the sources'
  sha256, the tool version). It exits 1 and names the output on any difference. A different Pillow
  version may resample differently: the manifest records the version it was made with.
* Exit codes: 0 done or checked; 1 refused, failed or a difference; 2 usage, or a stage not built.
* Tests: `tools/parity/.venv/Scripts/python -I -B -m pytest -q -p no:cacheprovider tools/art/tests`
  (29; every fixture is built inside the tests, no Magic Rampage file is opened). Lint:
  `tools/converter/.venv/Scripts/ruff check tools/art`. pytest is in the parity venv (9.1.1).

## Numbers of the first run (2026-10-15, Pillow 12.3.0)

`manifest.json` tool_version 1, package kind `assets-folder`, expected version 7.8.7.

| Output | sha256 of the file | bytes | sha256 of the pixels (first 16) |
|---|---|---|---|
| `walk_left.png` | `196e94b81ae7c71ce34bd5cce36307d0221a0643616229a2737f40d44ca61de9` | 9,389 | `706a0dfd332740b5` |
| `walk_right.png` | `b8e6721955f725949ba2d24d964b0afc420c55ef9a55ecf0a70b5eddf55495e0` | 9,276 | `b4b1d673c1e34b19` |
| `restart.png` | `6969ca877a92bebd10ce970617134c24458acd02386fbd05bbee223bc6d1c184` | 12,600 | `a42e4c6f12e7bb61` |
| `pause.png` | `c236ea95c53e23a277b041e87008f6d007344652210e79cf15d92190456174c8` | 8,797 | `fcd69479105d4cb2` |
| `clear_portals.png` | `0ca623bbd89d7f773902fe5dc0b3f0878ab98e3d7356db758da3b76030cebc5c` | 13,998 | `d243722df5a76dd5` |

| Source | sha256 | bytes |
|---|---|---|
| `sprites/dpad-frame.png` (128 x 128, palette + tRNS) | `2d7c1ede27d78524af7b0e7654538ed26fbe69f0eeec895e90c28f97282e8518` | 4,105 |
| `sprites/dpad-left.png` (128 x 128, palette + tRNS) | `ae44dfdfaac54b12840bbb138b7e255bfa8db3169826b8e25c42607d685ab833` | 5,121 |
| `sprites/dpad-right.png` (128 x 128, palette + tRNS) | `720958efca9b8ea9decb5f5b1f689e5d3b0b2a059e81970afb6301f236780a37` | 5,103 |
| `sprites/icon-restart-64.png` (64 x 64, grey + alpha) | `841cea0e095c0daf611bc3ff514b86195fd97ac0b70326cee8a8ffa1be0e2f7d` | 1,636 |

## The extracted sprites (`--extract`, on by default)

Copied byte for byte (21 files, 21 of 21 equal to the source by `cmp`), under the same paths, with
their role in `assets/extract.json`:

* **control** (read now): `sprites/dpad-frame.png`, `dpad-left.png`, `dpad-right.png`,
  `icon-restart-64.png`;
* **editor** (CTL-11's seven tiles and Back): `sprites/smaller-frame-bg.png`,
  `smaller-frame-red-bg.png`, `resize-symbol-shrink.png`, `resize-symbol-enlarge.png`,
  `adjust-symbol-transparent.png`, `adjust-symbol-opaque.png`, `lock-locked.png`,
  `lock-unlocked.png`, `inventory-back-button.png`;
* **spare** (extracted, no recipe uses them: owner's choices later, or the editor's panel and
  frames): `sprites/pause-pad-button.png` (Magic Rampage's own pause pill), `icon-quit-64.png`,
  `hd/portal-duel-icon.png`, `remove-symbol.png`, `smaller-purple-frame-bg.png`,
  `frame-title-bg.png`, `rounded-edges-frame.png`, `inventory-next-button.png`. A spare missing from
  a package is a note, not an error; a control or editor sprite missing is an error.

## How the game reads them (planning step 184)

The port reads the five pictures **at run time** and holds none of them. The precondition above is recorded again for this step: the owner's request of
2026-10-15 is the go-ahead for local, gitignored use; `docs/permission.md` is still the unfilled placeholder and says nothing about Magic Rampage; no
Magic Rampage file is read by any test, committed or shipped (the tier tests' own pictures are synthetic grey PNGs written into a scratch directory of their own; the one exception, `TheDrawRectUsesTheTiersMargin`, lets the original's own corner sprites stand and reads only their size); a release package that
bundles the folder is the owner's call and no packaging script knows about it.

* **Where.** `Paths::controls.artDir`, set by `game/main.cpp` from the compile definition `MAGICPORTALS_PADART_DIR` (cache variable
  `SUPERSONIC_MAGICPORTALS_PADART`, default `reference/magic-rampage/controls` of this repository, absolute; a definition of `main.cpp` alone, so the suites are
  not rebuilt for it) or from `--padart <absolute dir>`. A `--controls modern` run reads it; a classic run, a bare layer and every suite do not.
* **The tiers**, tried in this order for each of the five controls, once at attach and never again (a level's load does not touch them):
  1. Magic Rampage's file in that folder (`walk_left.png`, `walk_right.png`, `restart.png`, `pause.png`, `clear_portals.png`: the names `controls.json`'s `art.mr`
     gives), if it is there, can be read, and measures what `manifest.json` records for it (`outputs.<file>.size`), else `layout.box_px` square (128 x 128). One that
     is there and cannot be read, or measures otherwise (a 406 x 406 canvas dropped in by hand would be drawn mis-scaled and say nothing), is skipped with a WARN that
     gives both sizes. A file that is not there says nothing. A manifest that cannot be read is one WARN and holds every picture to 128 x 128.
  2. For restart, pause and clear-portals only: the original's own sprite (`art.original`: `restart_level_button.png`, `main_menu_shortcut.png`,
     `clear_portals_button.png`), found as the classic buttons find it (the hd twin first). It is opaque to its edges, so it is drawn into the box's face
     (22 percent smaller than the box), not into the box.
  3. The glyph the game draws (`sim/PadArt`), written as a PNG into `Paths::prisms` (a temporary directory) - only for a control that reaches this tier, only when the
     file there is not already exactly those bytes, and through a name of its own renamed over the target, so a second window never loads a half-written file.
  4. A flat quad on the box's face.
* **The log.** `modern controls art: walk_left=mr walk_right=mr restart=mr pause=mr clear_portals=mr` once per run (`generated`, `original` and `flat` as the case
  may be), and a WARN when the five do not all come from one tier (Magic Rampage's grey faces beside the original's white corner buttons, say).
* **File names in the data are bare.** `controls.json` names each picture's file; `LoadRules` refuses a name that is not one file in the folder of its tier (a
  separator, `..`, a drive), because the generated tier writes there. A refusal makes modern unavailable with one error line, as any other typo in the file does.

## Left to the owner

Looks are the owner's: pause as a framed square or Magic Rampage's pill (Q5); the clear-portals glyph
as drawn here or the original game's own button (Q12); the 60 px glyph box; and whether the art may
leave his machine at all (Q2, above).
