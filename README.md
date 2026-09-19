# Magic Portals — Remake

A remake of **Magic Portals**, a 2D mobile puzzle-platformer originally published by
**Asantee Games** and delisted in 2025, running on the **Supersonic Engine**. This
repository holds the game itself — the port, in `game/`, built against the engine as a git
submodule at `engine/` — together with the data pipeline that decodes the original's files
and converts its levels, the parity tools that compare the remake with the original, and
the docs.

## Permission

This project exists with the **written permission of the rights holder** to rebuild the
game and reuse its assets. The original engine, [Ethanon](https://github.com/asantee/ethanon),
is MIT-licensed and is used here only as read-only reference for decoding the original
file formats.

**That permission covers rebuilding and reuse — it does not cover redistribution.** So:

> **No original asset, data file, or APK is ever committed to this repository.**
> `reference/` is gitignored in full and stays that way. See `CLAUDE.md` and the comment
> block at the top of `.gitignore`.

The authoritative record of the grant and its scope is `docs/permission.md`.

## Status

**All 128 levels start and play on the Supersonic port** — four chapters of 32, with no
mechanic the levels use left unbuilt, drawn with the original's own art apart from
placeholder boxes for the minions, the knights and some spawned effects. Around them the
port draws the original's lighting (lightmaps, 2D lights, halos), its camera-pinned skies,
entity particle systems and the player's walk animation, plays its sounds and music, and
has its screens: HUD, pause, level finished, game over, tutorial popups, loading screen,
main menu, credits, achievements, chapter select and the level grid.

**It does not yet match the original.** The work now is side-by-side parity against
captures of the original running in an emulator:

- entity visuals — particle brightness, minions and knights that still have no sprites,
  image tiers, spawned effects;
- the lighting and UI differences still open;
- the numbers the port still reads from `data/` as marked guesses: the player's walk
  speed, acceleration, friction, air control, gravity, fall speed and body size, and the
  camera's follow lag and hold time;
- last, a sweep of all 128 levels against the reference library.

The live record is the port's planning doc,
`docs/planning/2026-09-11-magic-portals-remaster.md` (every step, with its gates), and
`docs/parity-backlog.md` (what is left, in order).

## The Supersonic port

The game is `game/`: Magic Portals in C++20 on
[Supersonic-Engine](https://github.com/IvanCvetanovic/Supersonic-Engine), which this
repository builds as a subproject from a git submodule at `engine/`, pinned to one engine
commit. Its suites are `tests/test_mp_*`. It moved here from the engine repository's
`games/magicportals` on 2026-09-19, with its history.

```bash
git clone --recurse-submodules https://github.com/IvanCvetanovic/Magic-Portals-Remake.git
# in a clone made without --recurse-submodules:  git submodule update --init
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DSUPERSONIC_ENABLE_VALIDATION=ON
cmake --build build
ctest --test-dir build
```

Configure and build in a shell with MSVC's environment (an x64 Native Tools prompt, or after
`vcvars64.bat`); `tools\parity\port_build.bat` does both steps on the owner's machine. It
compiles from a fresh clone, but it plays nothing until the original's levels are converted
locally, and 27 of the 44 suites skip without them: *Local setup* below, steps 5-8.

## How it fits together

```
  reference/apk/              original APK                  (you supply it — never committed)
        │  unpack
        ▼
  reference/extracted/assets/ scenes, entities, sprites, fonts, sounds, android_game.bin
        │                                         │
        │  tools/converter  (ethanon2godot)       │  tools/asbc  (AngelScript bytecode)
        ▼                                         ▼
  out/levels/*.tscn                         out/data/achievements.json
  out/data/chapters.json
  out/assets/  entity art, lightmaps
        │
        │               data/*.json   (the remake's tables, committed)
        │               │
        ▼               ▼
  game/  (the port, built on engine/)   ◄── reference/extracted/assets, read directly
```

The converter writes each level as a `.tscn` text scene: that is the level format the
port's reader parses (`game/sim/Tscn.cpp`). Nothing is copied into the engine's
repository. The port reads `out/`, `data/` and `reference/extracted/assets/` where they are,
through CMake cache paths baked in when `build/` is configured (steps 5 and 7 below).

```
CMakeLists.txt     the build: engine/ as a subproject, then game/ and tests/
engine/            Supersonic-Engine, a git submodule pinned to one commit
game/              the port: main.cpp, MagicPortalsLayer, LevelVisit, sim/, spike/, and data/ (its own JSON)
tests/             the port's test_mp_* suites
data/              the remake's gameplay JSON, committed; the port reads entity_roles.json, player.json, portals.json
tools/converter/   ethanon2godot — Python 3; .esc/.ent → out/levels, chapters.json, entity art, lightmaps
tools/asbc/        AngelScript bytecode reader; achievements.py writes out/data/achievements.json
tools/parity/      emulator driver, reference-library capture, frame comparison, port_build.bat
docs/              format notes, the original's behaviour, parity rig and backlog, permission record
docs/planning/     the port's planning records
reference/         ⛔ gitignored in full — not present in a fresh clone (see below)
out/               converter output and parity captures — gitignored
build/             the CMake build tree — gitignored
```

## Local setup after cloning

> **A fresh clone has no game content and cannot be played.** That is by design, not an
> oversight. Levels are converted from the original APK, and a converted level is the
> original's level geometry in a different encoding — so converted output is gitignored
> for the same reason the APK is. **Everyone who builds this project supplies their own
> copy of the original and runs the converter locally.** There is no path around this
> that does not involve redistributing content we have no permission to redistribute.

**1. Recreate `reference/`** — intentionally absent from the repo:

```bash
mkdir -p reference/apk reference/extracted
```

Drop the original APK into `reference/apk/`, then unpack it (an APK is a zip archive) into
`reference/extracted/`, so that `reference/extracted/assets/scenes/` exists. From Git Bash:

```bash
unzip -q reference/apk/<the-original>.apk -d reference/extracted
```

From cmd or PowerShell, Windows' own `tar` reads it:
`tar -xf reference\apk\<the-original>.apk -C reference\extracted`. Nothing you put under
`reference/` will be committed.

The Ethanon engine source is optional: only format-decoding work (`tools/asbc`,
`docs/ethanon-formats.md`) reads it; converting, building, testing and playing do not.

```bash
git clone --depth 1 https://github.com/asantee/ethanon.git reference/ethanon
```

**2. Install Python 3.11+ and the converter's venv.** From the repo root:

```bat
python -m venv tools\converter\.venv
cd tools\converter
.venv\Scripts\python -m pip install -e ".[dev]"
cd ..\..
```

The converter has no runtime dependencies; `[dev]` adds pytest and ruff. Installing from
`tools\converter` is fine; *running* the converter from there is not (step 3).

**3. Convert all four worlds.** Run from the repo root: the default input
(`reference/extracted`) and output (`out`) are relative to the working directory. `--world`
takes one world, so it is one run per world, and all four must write to the same `out/`,
because each run adds its world to `out/data/chapters.json` and keeps the others.

```bat
tools\converter\.venv\Scripts\python -m ethanon2godot --world 0 --copy-textures --copy-lighting
tools\converter\.venv\Scripts\python -m ethanon2godot --world 1 --copy-textures --copy-lighting
tools\converter\.venv\Scripts\python -m ethanon2godot --world 2 --copy-textures --copy-lighting
tools\converter\.venv\Scripts\python -m ethanon2godot --world 3 --copy-textures --copy-lighting
```

World N is chapter N+1, and its levels are `level0`…`level31` with the suffix `''`, `a`,
`b` or `c`: `level0` is 1-01, `level31c` is 4-32. What lands in `out/`:

- `out/levels/<name>.tscn` — one per level;
- `out/data/chapters.json` — the level order plus each level's gold threshold, read from
  the original's `goldenScores.enml`;
- `out/assets/entities/` — every sprite a level references (`--copy-textures`), plus the density
  tier file the original draws at 720 px in `entities/hd/` or `entities/fullhd/`, recorded in
  `out/assets/entity_tiers.json` (the port picks the tier file itself; the `.tscn` names the 1× path);
- `out/assets/lightmaps/`, the normal maps, the halo bitmaps and
  `out/assets/lighting_files.json` (`--copy-lighting`).

`--copy-lighting` copies each level's baked lightmaps (`add<id>.png`), the normal maps and halo
bitmaps the emitted `metadata/eth_*` keys point at. The keys are written with or without it, so a
run for the Supersonic port must include it or the port's reader finds paths with no file behind them.

`--copy-ui` (UI sprites with their `hd/` tier, bitmap fonts and sound effects, into
`out/assets/`) is not needed by the port: it reads UI art, fonts and sounds straight from
`reference/extracted/assets/`.

**4. Extract the achievements** (optional — and skip it on a fresh clone: it needs a
disassembly listing that only the owner's machine has yet).

The `--listing` input is a full disassembly of `android_game.bin` that the committed
`tools/asbc` cannot produce yet — its reader stops before those functions
(`tools/asbc/README.md`). The listing in use was made by a disassembler kept in the
gitignored `out/parity/asbc/decoder_scratch/`. Where that listing exists:

```bat
python tools\asbc\achievements.py --listing out\parity\asbc\all_functions.txt
```

This writes `out/data/achievements.json`. Achievement titles, descriptions, icons and points
are Asantee content, so they exist only in that gitignored file; without it the
achievements dashboard draws no rows, and nothing else changes.

**5. Check out the engine.** It is a git submodule at `engine/`, pinned to one commit of
[Supersonic-Engine](https://github.com/IvanCvetanovic/Supersonic-Engine). A clone made with
`--recurse-submodules` has it already; otherwise, from the repo root:

```bash
git submodule update --init
```

The port's inputs default to their places in this repository (`game/sim/CMakeLists.txt`).
Set these when configuring only to read them from somewhere else:

| Cache variable | Default |
|---|---|
| `SUPERSONIC_MAGICPORTALS_LEVELS` | `out/levels` |
| `SUPERSONIC_MAGICPORTALS_CHAPTERS` | `out/data/chapters.json` |
| `SUPERSONIC_MAGICPORTALS_DATA` | `data` |
| `SUPERSONIC_MAGICPORTALS_ORIGINAL` | `reference/extracted/assets` |
| `SUPERSONIC_MAGICPORTALS_ACHIEVEMENTS` | `out/data/achievements.json` |

Pass each as `-D<variable>=<path>` on the configure line of step 7, e.g.
`-DSUPERSONIC_MAGICPORTALS_LEVELS=C:/elsewhere/out/levels`. `_CHAPTERS`, `_ORIGINAL` and
`_ACHIEVEMENTS` default relative to `_LEVELS`, so on the **first** configure of an empty
`build/` levels converted elsewhere need only `_LEVELS`. CMake never overwrites a cache entry
that already exists: if `build/` was configured before, pass all five, or delete
`build/CMakeCache.txt` first. The paths are baked in as absolute paths when `build/` is
configured.

**6. Engine prerequisites** (`engine/README.md`, *Requirements*):

- a C++20 compiler — MSVC 2022, GCC 11+ or Clang 13+;
- CMake 3.20 or newer;
- Ninja on `PATH`, for the configure line in step 7 (Visual Studio's "C++ CMake tools for
  Windows" component ships it, or `winget install Ninja-build.Ninja`);
- a GPU driver capable of Vulkan 1.2;
- the Vulkan SDK, strongly recommended, and required for parity captures: it supplies the
  validation layers and `glslc`. Without `glslc` CMake falls back to the committed SPIR-V;
  without the validation layers a run validates nothing (see *Capture flags*).

Every other dependency is vendored in the engine's `third_party/`. The owner builds with
MSVC from Visual Studio Build Tools and the Ninja generator.

**7. Configure and build.** From this repository's root, in a shell with MSVC's
environment (an x64 Native Tools prompt, or after `vcvars64.bat`):

```bat
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DSUPERSONIC_ENABLE_VALIDATION=ON
cmake --build build
```

The engine is a subproject here: it builds its library and nothing else — no editor, no
script plugin, none of its own suites — and leaves its committed shaders alone.
`SUPERSONIC_BUILD_MAGICPORTALS` (on) gates only the executables (`MagicPortals.exe` and the
`MagicPortalsSpike` measurement tool), and `MAGICPORTALS_BUILD_TESTS` (on) the suites; the
port's libraries always build. `SUPERSONIC_ENABLE_VALIDATION=ON` keeps the Vulkan validation
layers on in a Release build. The owner's build also passes the SDK's `glslc` as
`GLSL_COMPILER` and names MSVC's `cl` as the compiler (`-DCMAKE_C_COMPILER=cl
-DCMAKE_CXX_COMPILER=cl`), since CMake takes Strawberry Perl's GCC when that comes first on
`PATH`.

On the owner's machine, from cmd or PowerShell:

```bat
tools\parity\port_build.bat --target MagicPortals
```

From Git Bash, which would eat the backslashes, hand it to cmd:
`cmd //c "tools\parity\port_build.bat --target MagicPortals"`. It sets up MSVC's
environment with the owner's Visual Studio Build Tools path, configures `build/` as above
the first time (when it has no `CMakeCache.txt`), and runs `cmake --build` on it with the
arguments given. On any other machine, run the two lines above instead.

On Windows 11, Smart App Control sometimes blocks a freshly linked exe ("An Application
Control policy has blocked this file"). Deleting the exe and relinking gets past it. Each
blocked launch raises a Windows notification, so relink and relaunch once rather than in a
retry loop, and run only the suites a change touches.

**8. Run the port's tests.** From this repository's root, after a full build:

```bat
ctest --test-dir build
```

These are the `test_mp_*` suites (a multi-config generator such as Visual Studio also needs
`-C Release`). Convert the levels (step 3) before running them. Without the converted levels:

- 27 suites (`levels`, `play`, `start`, `statics`, `chapters`, `timed`, `movers`,
  `hazards`, `demolish`, `launchers`, `shot`, `boss`, `layer`, `zerog`, `bounce`, `wells`,
  `dragon`, `darkdragon`, `turrets`, `fire`, `hinge`, `minions`, `ghost`, `torch`, `keys`,
  `diamonds`, `fields`) exit 77 and print the path they looked in, which ctest reports as
  SKIPPED: each carries `SKIPS_WITH_77` on its `add_mp_test` line in `tests/CMakeLists.txt`
  (a mark made in Supersonic-Engine `80a4ed2`, before the port moved here; before it, the
  last nine were missing from the skip list, so ctest would have reported them as Failed);
- 6 suites (`sprites`, `lighting`, `sky`, `tiers`, `motion`, `sounds`) skip only the part
  that needs the converted levels or the original's extracted assets, say so, and report
  Passed.

## Run it

From this repository's root:

```bat
build\game\MagicPortals.exe
```

The game finds the engine's shaders in `engine/` by itself (the engine's
`SUPERSONIC_ASSET_ROOT`, set to the submodule) and makes `engine/` its working directory
before it opens anything, so it runs from any directory — and a relative path given to one
of its flags resolves inside `engine/`. Give the path flags below absolute paths.

With no `--level` the game opens as the original does: loading screen, then main menu.
`--level level0` starts straight in 1-01. Diagnostics also go to `%TEMP%\magicportals.log`.

| Input | Action |
|---|---|
| `←`/`→` or `A`/`D`, held | walk |
| left mouse button | fire a portal shot toward the pointer; the portal opens where the shot arrives |
| left mouse on the on-screen controls | walk pads (held), restart, pause, clear portals |
| left mouse on a menu | buttons and tiles; drag scrolls the credits and the achievements, the wheel scrolls the achievements |
| `Esc` | in a level: pause, and resume from it; closes a popup; in a menu: back a screen; on the main menu: quit |
| `R` | retry the level |
| `N` | skip to the next level (dev) |
| `B` | draw the physics bodies' boxes over the art (dev) |
| `G` | write what the game believes it is drawing to the log (dev) |

The mouse is the original's touch: the port reads the pointer and the left button, and has
no separate touch path. **There is no jump.** The original's on-screen pad has only left
and right (the decoded `SCREEN_PAD_BUTTON` enum), and the project owner confirms it from play.

### Capture flags

Run captures from **Bash** (Git Bash): PowerShell 5.1 mangles the arguments. A capture is
drawn with the original's art, so it is written under this repository's gitignored
`out/parity/`, never inside `engine/`, whose `.gitignore` does not cover a stray PNG. From
this repository's root, with an absolute `--screenshot` path (a relative one would land in
`engine/`, where the game runs):

```bash
mkdir -p out/parity/port
build/game/MagicPortals.exe --level level0 --window 1280x720 --fixed-step --frames 420 --screenshot "$PWD/out/parity/port/level0.png"
```

The flags a capture uses (`--help` prints every flag):

| Flag | Meaning |
|---|---|
| `--level <name>` | start at this level, as `chapters.json` names it (`level0` … `level31c`); without it the menu opens |
| `--window <W>x<H>` | window size, which is also the render resolution; parity captures use `1280x720` |
| `--fixed-step [s]` | simulate at a constant step (default 1/60 s), so a run reproduces: two runs of the same level and frame are bit-identical |
| `--frames <n>` | render exactly `n` frames, then exit |
| `--screenshot <path>` | write a PNG of the last frame of a `--frames` run (without `--frames`, nothing is written); the directory must exist; give it absolute |
| `--screenshot-every <n>` | with `--screenshot`, also write every `n`th frame as `<stem>_f<frame>.png`; pair it with `--fixed-step` |
| `--hold <left\|right>@<from>-<to>` | DEV: hold a walk from game tick `from` to `to` |
| `--tap <help\|x,y>@<tick>[-<release>]` | DEV: a touch at pixel `x,y` of 1280x720, or on the level's first help block, released on the next tick or at `release` |
| `--drag <x,y>:<x,y>@<tick>-<release>` | DEV: a touch that moves evenly from the first pixel to the second |
| `--press <what>@<tick>` | DEV: press `pause`, a pause button (`levels`, `resume`, `skip`, `achievements`, `sound`, `music`), or `back` on a menu |
| `--saves <dir>` | DEV: keep the medals (`scores.json`) here instead of the user's data directory, for a capture that needs a medal recorded |
| `--levels <dir>` | read the converted levels from here instead of the configured path |
| `--art <dir>` | the directory the levels' art paths resolve against (default: the one above the levels) |
| `--data <dir>` | read the remake's `data/` from here instead of the configured path |
| `--light-masks-off` | DEV: draw lights and halos but let no light reach a sprite |
| `--visit-levels <lightmapped\|all\|name,...>` | DEV: visit these levels in one process; the run fails unless the engine's material descriptor sets return to its pool |
| `--visit-passes <n>` | DEV: how many times to walk that list (default 1) |
| `--help` | every flag, the engine's own included |

`--hold`, `--tap`, `--drag` and `--press` repeat. `--screenshot`, `--saves`, `--levels`,
`--art` and `--data` take absolute paths: a relative one resolves inside `engine/`. A run exits non-zero on any Vulkan
validation error — when the validation layers are loaded, which needs the Vulkan SDK
(step 6). The log says which: `Vulkan validation layers: ACTIVE` or `NOT LOADED`; without
them the engine warns at startup and a run validates nothing.

## Parity work

`docs/parity.md` is the rig: why only the Android API 30 x86 emulator image runs the
original and the three fixes that get it to start, how to reach any of the 128 levels by
writing a save, the original's screen map, the reference library of captured frames, and
capturing the port. Read its §1-2 before running any script.

Prerequisites, none of which the setup above installs:

- the Android SDK: `emulator`, `platform-tools` (`adb` on `PATH`; `emu.py` runs it bare),
  `build-tools` (`zipalign`, `apksigner`) and `system-images;android-30;google_apis_playstore;x86`,
  plus a JDK for `keytool` and `apksigner`;
- a second Python venv, with the packages the scripts import (there is no requirements file):

```bat
python -m venv tools\parity\.venv
tools\parity\.venv\Scripts\python -m pip install numpy pillow opencv-python-headless imageio-ffmpeg
```

`tools/parity/` holds the scripts; the command-line ones carry their command line in their
docstring:

- `emu.py` — drives the original in the emulator over adb;
- `capture_original.py`, `capture_screens.py` — build the reference library, verifying each
  screen (`markers.py`) before tapping;
- `locate.py` — finds where, and at what scale, a sprite is drawn in a frame;
- `compare.py` — one original frame against one remake frame: offset, edge overlap, mean
  absolute difference, a pair image and an overlay;
- `frames.py` — turns a screen recording into frames and contact sheets;
- `port_build.bat` — builds the port (step 7 above).

Everything captured from the original is Asantee-origin imagery and stays under
`out/parity/`, which is gitignored. Captures of the port are drawn with the original's art as
well, so they also go under `out/parity/`, never inside `engine/`.

## Docs

- `CLAUDE.md` — the project's rules: legal basis, the hard rule on committed assets, the
  environment, and how to verify a converter or port change
- `DEVLOG.md` — append-only build diary
- `docs/parity.md` — the parity rig: emulator, reaching levels, reference library, captures
- `docs/parity-backlog.md` — what remains to match the original, in order
- `docs/ethanon-formats.md` — Ethanon file formats, decoded from the engine source
- `docs/original-gameplay.md` — how the ORIGINAL behaves, every claim tagged with its
  provenance (DATA / DECODED / IDENTIFIER / ABSENCE / OWNER / GUESS)
- `MILESTONE_1.md` — the first milestone, chapter 1 as a vertical slice. Its scope boundary
  is superseded and its Godot items carry dated notes; it is kept as the quality bar, above
  all §3's rule that the portal mechanic's values are measured, not guessed
- `tools/asbc/` — AngelScript bytecode reader, ported from the engine's own deserialiser
- `tools/converter/README.md` — the converter's modules, its checks and its test-fixture rule
- `docs/planning/2026-09-11-magic-portals-remaster.md` — the port's record: every step,
  with its gates; beside it, the port's plan and the spike that started it
- `engine/README.md`, `engine/AGENTS.md`, `engine/ARCHITECTURE.md` — the engine the port
  is built on, and the conventions port code follows
