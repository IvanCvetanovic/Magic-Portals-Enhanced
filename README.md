# Magic Portals — Enhanced

An enhanced version of **Magic Portals**, a 2D mobile puzzle-platformer originally published by
**Asantee Games** and delisted in 2025, running on the **Supersonic Engine**. This
repository holds the game itself — the port, in `game/`, built against the engine as a git
submodule at `engine/` — together with the data pipeline that decodes the original's files
and converts its levels, the parity tools that compare the port with the original, and
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
git clone --recurse-submodules https://github.com/IvanCvetanovic/Magic-Portals-Enhanced.git
# in a clone made without --recurse-submodules:  git submodule update --init
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DSUPERSONIC_ENABLE_VALIDATION=ON
cmake --build build
ctest --test-dir build
```

Configure and build in a shell with MSVC's environment (an x64 Native Tools prompt, or after
`vcvars64.bat`); `tools\parity\port_build.bat` does both steps on the owner's machine. It
compiles from a fresh clone, but it plays nothing until the original's levels are converted
locally, and 31 of the 70 suites skip without them (measured on Linux with `out/levels` and `out/data` hidden; a 32nd, `test_mp_shoot`, then runs fewer
checks than its floor and fails): *Local setup* below, steps 5-8.

**Build on Linux.** `tools/build_linux.sh` configures (Ninja, Release, X11) and builds `MagicPortals` and `test_mp` on Ubuntu 24.04
with g++ 13 or clang 18, from a clone with its submodule; `--assets-from <folder>` (or the five `SUPERSONIC_MAGICPORTALS_*`
variables) gives it the converted levels and the original's assets, and without them it still builds (the game draws boxes and the
data suites skip). Build on a Linux file system, not a Windows drive mounted into WSL. The original's 46 MP3 sounds decode there
(the engine decodes MP3 through Media Foundation on Windows and through its vendored dr_mp3 everywhere else; a clip starts about 12 ms
apart between the two): the decoding was checked on the real files, but a Linux run has not been listened to, and a headless one has no
sound device. `tools/build_linux.sh --help` lists the options.

**Build for Android.** `tools/build_android.sh --data <the content packager's output folder>` builds the game for `arm64-v8a` and `x86_64` with the Android
NDK (r28 or newer) and packages one APK, with the game's data in it, unpacked into the app's private folder on the first run (`docs/android.md`: what you need,
the options, what the APK holds, what the owner must decide). It is **built, packaged and statically checked, and run on an x86_64 emulator** (installs, unpacks, plays all 128 levels'
recorded winning routes on the same ticks as Windows (the implementer's on the first build, a sample of 17 on the final one, and the verifier's own 128 of 128 on the final one), survives Back, Home and an update), **and has never been run on a phone**: `tools/android_emulator.py` is the one script that drives an emulator (`docs/android.md`, section 7).

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
        │               data/*.json   (Enhanced's tables, committed)
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
tests/             the port's test_mp_* suites, in one runner (test_mp); tests/routes/ the win routes test_mp_routes replays;
                   tests/golden/ the constants the controls are pinned to (the classic claim map and HUD digest, the modern census's accepted list)
data/              Enhanced's gameplay JSON, committed; the port reads entity_roles.json, player.json, portals.json
tools/converter/   ethanon2godot — Python 3; .esc/.ent → out/levels, chapters.json, entity art, lightmaps
tools/asbc/        AngelScript bytecode reader; achievements.py writes out/data/achievements.json
tools/parity/      emulator driver, reference-library capture, frame comparison, the reachability
                   check, port_build.bat
tools/art/         make_mr_controls.py: Magic Rampage's control buttons, read from its package into the gitignored
                   reference/magic-rampage/ for the modern controls (code only; docs/magic-rampage-controls.md)
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
tools\converter\.venv\Scripts\python -m ethanon2godot --world 0 --copy-textures --copy-lighting --copy-particles
tools\converter\.venv\Scripts\python -m ethanon2godot --world 1 --copy-textures --copy-lighting --copy-particles
tools\converter\.venv\Scripts\python -m ethanon2godot --world 2 --copy-textures --copy-lighting --copy-particles
tools\converter\.venv\Scripts\python -m ethanon2godot --world 3 --copy-textures --copy-lighting --copy-particles
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
  `out/assets/lighting_files.json` (`--copy-lighting`);
- `out/assets/entities/inline_particles/` — each placement's own particle systems, one UTF-16
  `.ent`-shaped file per distinct `<Particles>` block the levels carry (`--copy-particles`).

`--copy-lighting` copies each level's baked lightmaps (`add<id>.png`), the normal maps and halo
bitmaps the emitted `metadata/eth_*` keys point at. The keys are written with or without it, so a
run for the Supersonic port must include it for the normal maps and the halo bitmaps or the port's reader finds paths with no file behind them;
the port no longer reads the lightmaps: it bakes each one in memory when its level loads, and `test_mp_lighting` still checks the converter copied the ones the levels name.
The same holds for `--copy-particles` and the `metadata/eth_particles` key: without the files the
port refuses the key, and a level whose lighting is refused is drawn unlit.

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

On Windows, a clone at a deeply nested path can stop here with `Filename too long` on the
engine's vendored files (the Vulkan headers, VulkanMemoryAllocator's docs). A submodule does
not take the superproject's `core.longpaths`, so pass it on the command line, in the clone:
`git -c core.longpaths=true submodule update --init`.

The port's inputs default to their places in this repository (`game/sim/CMakeLists.txt`).
Set these when configuring only to read them from somewhere else:

| Cache variable | Default |
|---|---|
| `SUPERSONIC_MAGICPORTALS_LEVELS` | `out/levels` |
| `SUPERSONIC_MAGICPORTALS_CHAPTERS` | `out/data/chapters.json` |
| `SUPERSONIC_MAGICPORTALS_DATA` | `data` |
| `SUPERSONIC_MAGICPORTALS_ORIGINAL` | `reference/extracted/assets` |
| `SUPERSONIC_MAGICPORTALS_ACHIEVEMENTS` | `out/data/achievements.json` |
| `SUPERSONIC_MAGICPORTALS_PADART` | `reference/magic-rampage/controls` (the game only: a definition of `game/main.cpp` alone, so the suites do not rebuild for it) |

Pass each as `-D<variable>=<path>` on the configure line of step 7, e.g.
`-DSUPERSONIC_MAGICPORTALS_LEVELS=C:/elsewhere/out/levels`. `_CHAPTERS`, `_ORIGINAL` and
`_ACHIEVEMENTS` default relative to `_LEVELS`, so on the **first** configure of an empty
`build/` levels converted elsewhere need only `_LEVELS`. CMake never overwrites a cache entry
that already exists: if `build/` was configured before, pass all five (and `_PADART`, if the pictures of the modern controls are elsewhere), or delete
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
retry loop, and run only the suites a change touches (`build/tests/test_mp.exe <suite>`, step 8).

**8. Run the port's tests.** From this repository's root, after a full build:

```bat
ctest --test-dir build
```

These are the `test_mp_*` suites (a multi-config generator such as Visual Studio also needs
`-C Release`). All of them are built into one executable, `build/tests/test_mp.exe`, so a
change that relinks them makes one new exe for Smart App Control, not one per suite. ctest runs
each suite in its own process, as `test_mp.exe <suite>`, under the suite's own name. Run by
hand, `build/tests/test_mp.exe test_mp_<suite>` runs one suite (the `test_mp_` may be dropped),
`build/tests/test_mp.exe` runs them all in one process and ends with a tally, and `--list` names
them. Convert the levels (step 3) before running them. Without the converted levels:

- 29 suites (`levels`, `play`, `start`, `statics`, `chapters`, `timed`, `movers`,
  `hazards`, `demolish`, `launchers`, `shot`, `boss`, `layer`, `zerog`, `bounce`, `wells`,
  `dragon`, `darkdragon`, `turrets`, `fire`, `hinge`, `minions`, `ghost`, `torch`, `keys`,
  `diamonds`, `fields`, `routes`, `census`) exit 77 and print the path they looked in, which ctest reports as
  SKIPPED: each carries `SKIPS_WITH_77` on its `add_mp_test` line in `tests/CMakeLists.txt`
  (a mark made in Supersonic-Engine `80a4ed2`, before the port moved here; before it, the
  last nine were missing from the skip list, so ctest would have reported them as Failed);
- 6 suites (`sprites`, `lighting`, `sky`, `tiers`, `motion`, `sounds`) skip only the part
  that needs the converted levels or the original's extracted assets, say so, and report
  Passed.

**Winning the levels.** `test_mp_routes` replays every file in `tests/routes/`: a level won by
play from its start to its door - walks held over spans of ticks, taps at points of the level,
and the tick by which the level must be won (`game/Route.hpp` has the format). A route is added
by adding its file; the suite finds it. For writing one, the same suite replays a single route
and prints what it did every n ticks and on every tap:

```bash
build/tests/test_mp.exe test_mp_routes --only level8 --trace 10
```

and `--route <absolute path>` replays one in the game's window (the flags below); give it
`--saves <absolute dir>` too, because a replay that reaches the level's end records its medal,
and without `--saves` that lands in the player's own save; `--route-trace <n>` makes that replay
print the suite's trace lines. Beside it, a
static check that no level's door is sealed off in the port, over the converted levels, with
the verdicts pinned in `tools/parity/reachability.json` (it needs the parity venv, below):

```bash
tools/parity/.venv/Scripts/python tools/parity/reachability.py --check
```

**What the modern controls cost the routes** (`--controls modern`, planning step 182). `test_mp_routes --controls modern`
plays every route three times: the classic reference, whose every tick is recorded; the modern layout and draw with the CLASSIC
claims, which must equal the reference on every tick (it is the run in which each touch of the route is asked what the modern
claim would say of it); and the modern run, which must equal the reference up to the tick before the first touch a modern control
takes, and change something from there when that touch is a shot. It prints the touches the modern claim takes per route and per
control. `--classic-claims` runs the second alone and `--controls classic` the reference alone. `--arrows <split|grouped>` plays
the first layout's grouped pair instead of the shipped arrangement (one arrow in each bottom corner); the census reports, and asserts,
that the arrows take no recorded shot in the split arrangement, and reads the accepted list of the arrangement the layer actually played.
`--contacts <one|two>` says whether the modern run reads a second finger (planning step 191; the data's `input.two_contacts` when it is not given): no
recorded route has two taps that overlap, so the census is the same numbers either way, and it prints which reading it played and fails if it did not play the one it was asked for.

```bash
build/tests/test_mp.exe test_mp_routes --controls modern
```

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
`--level level0` starts straight in 1-01. Diagnostics also go to `%TEMP%\magicportals.log`,
and each launch first moves the last run's to `%TEMP%\magicportals.prev.log`: after a crash,
the crashed run's log is still there once the game has been started again (one run back).
The file's lines carry the seconds since the first (`INFO +1.234s [Window] ...`), so a log can say a
slow start from a stuck one; its first line names the build (Release or Debug, the engine commit, and
the process's code page: 65001 where the application manifest `game/MagicPortals.manifest` took effect,
which is what lets a profile name with a character outside the machine's ANSI page start the game at all),
its second the asset root and the tier that found it, its third the scratch folder, a later one the folders the run
reads and writes, and a run that ends on purpose ends with `finished: clean`
(or `failed`), which a run killed by a crash does not write. Where `%TEMP%` names nothing, or a file cannot be made
in it, the log goes to `scratch` in the user data directory, then the user data directory itself, the executable's
folder, then the working directory.

Where the game finds its data is decided when it starts, not when it is built: `--root <folder>`, a `content` folder
next to the program, the user's data folder, and last the paths the build was configured with
(`-DMAGICPORTALS_BAKED_PATHS=OFF` compiles none in). `tools/package/package.py` builds the folder a release ships
from your own data. All of it is in [docs/packaging.md](docs/packaging.md).

Exit codes (planning step 189): 0 for a run that played and ended; 1 for a run that stopped on an error
(the reason is the log's `fatal:` line and stderr, and a **player's** run, one with none of the developer's
flags, also puts it in a message box naming the log; a capture or a route never opens a box), that could
not start (the data it plays from would not read, or `--level` names no level; a level that merely will
not load is *not playable yet* and has a way out on the back key), whose `--frames` run did not write
every PNG it was asked for, or that reported a Vulkan validation error or a failed level walk. That
includes the engine's own start throwing (no Vulkan loader or device, the likeliest real fatal): the
`fatal:` line and the box happen as above and the process ends with 1. (Planning step 189 measured it
ending with Windows' fast-fail code 0xC0000409 instead, because the engine's job-system workers were
still running when the process left; step 190 shuts them down after the box, in `ReportFatal`, and the
same probe exits 1.)

**Performance options** (planning step 190). `game/data/engine.json` says what a run asks of the engine's optional performance
features, for a **player's** run (a window the game opened for someone to play: none of the developer's flags, the same rule that
decides the message box) and for every other run (a capture, a route, a level walk, a script), each value with the measurement that
decided it. Shipped: the shadow maps at 16 px (the engine allocates 120 MiB of them whatever a game draws; the art view casts no shadow,
so every picture of it stayed byte for byte the same while the process's GPU memory fell by 193 MiB; the developer's lit boxes (the `B`
key, or a level drawn without its art) are the one draw that does cast one, and at 16 px their shadows are gone: `--shadow-maps 0`
brings them back), and the engine's stat of every watched file
on every frame, which exists for a developer's hot reload of a saved texture, off for a player's run (0.6 to 0.9 ms of CPU a frame;
every other run keeps hot reload, and a developer who wants it in a plain window passes `--asset-watching on`). Measured and left
as the engine has them: the scene pipelines built for unlit draws only (`--sprites-only on`: every sprite, particle and halo is
one, and every picture stayed byte for byte the same, but no gain could be measured on this machine and each start pays 2 to 6 ms
for it; the lit boxes of the `B` key would draw flat), and the layer's per-frame walk over its sprites for the `on`/`off`/`GONE` log
lines (nothing it costs could be told from noise). Each is one line of the file and a flag for one run (`--shadow-maps`,
`--sprites-only`, `--asset-watching`, `--sprite-report`), so changing one is an edit of the file, not of the code, and the log's
third line says what the run was given. A table that will not read leaves the engine as it was. `--perf-log <seconds>` writes the
engine's frame statistics (fps, median, 95th percentile, worst frame, where the frame went, the queue waits) to the log every that
many seconds; nothing is measured unless asked. The log also says how long the start spent reading the port's tables (`tables:
read in 67.8 ms`) and each level's (`level0 data read in 7.7 ms`), to one decimal.

The medals and the sound and music switches are kept between runs, as the original keeps
them, in `%APPDATA%\Magic Portals\` (`scores.json`, and `audio.json` from the first press of
a switch). So is the choice of controls (`controls.json`, from the first tile pressed in
Options). `--saves <dir>` moves all of them, for a run that must not touch the player's.

Each of them is written in one step: a temporary of its own name beside the file, flushed and checked
after it is closed, renamed over the file, and the live file is never removed to make room; `scores.json`,
`audio.json` and `achievements.json` also keep the last good copy beside them as `<file>.bak` (never a file
that failed to read). **A `scores.json` that does not read is never deleted or written over**: it is
renamed `scores.json.bad-<n>` (the first free `n`), the medals it still holds are kept and saved at once
(a write cut short keeps every whole row before the cut), the log says so (`medals: ... kept as ...`), and
the game goes on. One that cannot be *opened* (another program holds it for a moment) is waited for
(three tries, 100 ms apart), then left alone: the run keeps its medals in memory, tries again at every
save and as the game ends, and merges them in (the better medal of each level). `audio.json`,
`achievements.json` and `controls.json` that do not read are still written over at the next save, as
before; the stores disagree on purpose until the owner says otherwise.

**To play, build `build-play\`:** `tools\parity\play_build.bat`, then
`build-play\game\MagicPortals.exe`. It is the same game on the same data (the first run
copies `build\`'s data paths), built without Vulkan validation, whose layers check every call the
renderer makes, so its log says `Vulkan validation layers: NOT LOADED`, as it should. It builds the
game only; `build\` stays the validated build the suites and parity captures run on. It
configures once - if `build\`'s data paths move, delete `build-play\` and run it again.

| Input | Action |
|---|---|
| `←`/`→` or `A`/`D`, held | walk |
| left mouse button | fire a portal shot toward the pointer; the portal opens where the shot arrives |
| left mouse on the on-screen controls | walk pads (held), restart, pause, clear portals. Classic: restart, pause and clear act on the press. Modern (what a player's run starts in; `--controls modern` asks for it in a rig run): they act when the button comes up, if the press began on it (slide off and it is cancelled); a held arrow or button is drawn brighter, and a press that did something else (the pause's Resume, a popup's close) and is still down when the level takes over works no arrow until it is lifted |
| a second finger (a touch device, or a replayed tick) | `--controls modern` with `input.two_contacts` on in `game/data/controls.json` (shipped on; `--controls modern,contacts=one` turns it off for a run): a finger held on an arrow or a corner button keeps its control while another finger shoots, aims, pans the camera or taps a help block - a thumb on the walk arrow no longer stops a shot. One finger works the controls and one the world; a further finger, or a second one on a control while another works the controls, is ignored (restart and pause are not pressed through a held arrow); a finger that lands on the very tick the one holding its place lifts is not lost, its press follows on the next tick. With one finger nothing differs from the one-pointer game, and the classic style never reads a second finger. **A desktop mouse is one finger and there is no touch build, so real multi-touch hardware is not tested: only replayed ticks are** |
| left mouse on a menu | buttons and tiles; drag scrolls the credits and the achievements, the wheel scrolls the achievements |
| left mouse on the main menu's top-right gear | Options (below); a run that is not a developer's and not `--controls classic` has it |
| left mouse on Options' "Adjust controls" button | the adjustment screen (below); shown and claimed only while the style in force is Modern |
| left mouse on the adjustment screen | the seven widgets (below): a tile acts on the release strictly inside it after a press strictly inside it; with the lock open, a press on a control's drawn box grabs it and the drag moves it (below); a press on empty space does nothing, and no pointer reaches a level while it is up |
| `Esc` | in a level: pause, and resume from it; closes a popup; in a menu: back a screen; on the main menu: quit |
| `R` | retry the level |
| `N` | skip to the next level (dev) |
| `B` | draw the physics bodies' boxes over the art (dev). These boxes are the one lit draw the port makes: with `--sprites-only on` (*Performance options*; off as shipped) they, and a level drawn without its art, draw flat; at the shipped 16 px shadow maps they cast no shadow (`--shadow-maps 0` brings it back) |
| `G` | write what the game believes it is drawing to the log (dev) |

**The Options screen** (planning step 185). A player's run has a gear in the main menu's top-right corner, in the slot of the
original's omitted Facebook button, which opens Options: two tiles, **Modern** (Magic Rampage's inset controls) and **Classic** (the
original's), each with a small picture of where its five controls stand, and the credits' own back button. Press a tile (down and up on
it) and that style is the one the next level plays; the lit tile is the one in force. The choice is kept in `controls.json` beside
`scores.json` and read at the next start (`controls: modern (saved)` in the log); a player who has never chosen gets the shipped default,
which is **Modern** since planning step 198 (the owner's word of 2026-10-22; `default_mode` in `game/data/controls.json`, one value): a new
install, and an existing save that holds `scores.json`, `audio.json` and `achievements.json` and no `controls.json`, both start in Modern
(`controls: modern (the shipped default)` in the log), with no migration and no prompt, and the Classic tile - kept in `controls.json` from
then on - or `--controls classic` is the way back. A run that is a developer's (`--fixed-step`, `--route`, `--visit-levels`) or says `--controls classic` has no gear
and no store: its main menu is the original's, picture for picture. `--controls modern` starts the run in modern, ignores `controls.json`, and
the tiles then change the run only: nothing is written. The size, the opacity and the moves a player keeps are in the same file, as
deltas on one line (`{"version":1,"controls":{"mode":"modern","layout":2,"size":1.2,"opacity":0.6,"move":{"walk_left":[10,-5]}}}`: a value
equal to the shipped default is not written, so a later change of the default reaches a player who never moved a slider; a move is in
design units from the control's own corner, in screen direction, and a file saved under another `layout` number keeps the size and the
opacity and drops the moves), and a modern run plays them. The size and the opacity are set on the adjustment screen (below); the moves are made by dragging
the controls on that screen (below), or written by hand in the file, or asked by a capture with the flag below. From a level it takes five presses to reach Options: the pause, Levels, back, back
(from the grid to chapter select, and from there to the main menu), the gear; `Esc` is the pause and the two backs.

**The adjustment screen** (planning step 192, CTL-11 of the modern-controls plan). Options has an "Adjust controls" button under its two tiles, drawn and pressable only while the style in force is
Modern (a classic run has nothing to adjust: no button, nothing under its place). It opens a screen of its own - a menu state like Options, with no level loaded under it - which shows **the five
controls as a level draws them** (the same layout, alpha and pictures; clear-portals shown always, as it is in a level only while a portal is alive) over a flat backdrop, and **seven widgets** in the middle:
the **size** row (a minus tile, the size in force, a plus tile: 0.4 to 1.4 by 0.1; the default look is 0.7), the **opacity** row (0.2 to 1.8 by 0.2; the default is 1.0), the **lock** and **Restore**, and
**Back** at the top left, under the clear-portals preview. A tile is pressed by going down inside it and coming up inside it (strictly: on its edge is outside); a tile at its limit, and Restore with nothing to restore, are drawn dim and do
nothing. A step takes effect at once and is kept at once - **one save per tap**, never per frame - in `controls.json` (the size and the opacity as deltas, as above), and the next level lays out, claims, draws and
hints with it; a classic choice reads none of it. The first step made on the screen also drops a `--controls modern,size=...` flag's tuning (the screen shows the flag's numbers, an edit applies on top of them, and from then on they
are the screen's); under any `--controls` flag the edit changes the run only and writes nothing. The **lock** opens the editing cues - an outline breathing round each control, each control's hit rectangle as a faint frame
and a line across the preview at the character's row (206 units) - and **a size step re-locks it, an opacity step does not**; the screen always opens locked and the lock is never saved. **Restore** (enabled only while a control is moved) clears every move, keeps the size and the opacity and re-locks. **Back and `Esc`
leave for Options**, committing what is unsaved. Its pictures are Magic Rampage's own, read from the folder `editor` beside the controls' folder (made by `tools/art/make_mr_controls.py --stage editor`, never committed), then
glyphs the game draws, then flat quads (`docs/magic-rampage-controls.md`). Capture it with the chain `--controls modern --fixed-step --tap 1235,45@300 --tap 640,605@360 --frames 480` (the entry, then the button, at 1280x720; `--tap`
and `--press back@<tick>` drive all of it).

**Dragging** (planning step 193, CTL-12). The lock arms it. With the lock open, a press on a control's **drawn box** - strictly inside it; not the larger hit rectangle, which reaches the view's
edge, and not under a widget's face (a widget wins where it covers a control: the control's uncovered part is all a drag can take) - grabs the control, and it follows the pointer, keeping the offset the pointer had
in its box. It stays wholly inside the safe rectangle (the one shared clamp: `Controls::Boxes` lays a move out and `Controls::MoveFor` turns a wish back into one, a move kept to a tenth of a unit), clear of
every other control by Magic Rampage's own 16 px (5.69 units) and clear of every widget's box, Back included: a place that would meet one is refused. The pointer's x and y are walked separately, x first, in
steps of half a unit (`adjust.drag.step_units`), so a control **slides along** an obstacle instead of sticking to it, stops within a tenth of a unit of it, and cannot jump a neighbour at any speed of the pointer - a
drag blocked along one axis, or sliding along one face, ends on the same place at 1 unit a tick and at 40 (a diagonal drag that ends inside an obstacle can end beside it or above it, depending on how
coarsely the pointer is sampled). A control that already meets a widget (restart at 5:4 and the larger sizes) can be dragged out of it and never deeper in. A control is **kept once, at
the release**, if it ended somewhere else: while the finger is down the tuning in force and the file do not change (the screen shows the held control where the walk has it, with a thicker solid outline), and a
drag that came back to where it began writes nothing - one save per gesture. `Back` or `Esc` while a finger holds a control drops that gesture (nothing is kept); a pointer that leaves the window with the button
down ends it where it stands. A **size step** sends home the controls whose new boxes meet (both of a pair; and if one sent home now meets a third, everyone goes home), and re-locks; Restore re-locks too. The next
level lays out, claims and draws what the screen left: a moved control is claimed where it is drawn and the place it left is the world's, and the pause's Resume and a popup's close swallow the press over a moved control as
over any. **A limit, recorded and not built:** a moved control keeps its original anchor, so one dragged across the screen stays attached to the edge it was anchored at when the window changes shape (the layout pulls it
back inside; a window resized inside a run can leave two stored moves meeting). Not verifiable here: a live mouse on a real window and real touch.

The mouse is contact 0 and stands in for the original's touch: the port reads the pointer and the left
button as that contact, and, under the modern controls, the engine's other contacts (the other fingers
its Android backend reports, or a replayed tick's contacts) through one path in the layer, by which a
second contact reaches the controls and the world; with one contact nothing differs from before. No
touch build exists, so real multi-touch is replay-tested only. **There is no jump.** The original's on-screen pad has only left
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

**Which controls a capture draws.** `--fixed-step` (and `--route`, and `--visit-levels`) makes a run a developer's, and a developer's run is
classic - the original's controls, byte for byte - whatever `default_mode` in `game/data/controls.json` says and whatever a `controls.json` holds:
the recipe above, the 128-level sweep and every parity baseline carry `--fixed-step`, so they did not move when the shipped default became
Modern (planning step 198). Give `--controls modern` to capture the modern HUD. A **windowed** run that uses `--tap`, `--hold` or `--press` **without**
`--fixed-step` is a player's run: it plays the style kept in its `--saves` directory or, with none, the shipped default - Modern - so say
`--controls classic` when the original's controls are wanted. A capture's log says which it played on its first lines (`controls: modern (the
shipped default)`, `controls: classic (--controls classic)`, ...): grep it for `controls: modern` when a capture must not be modern.

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
| `--tap <help\|x,y>@<tick>[-<release>]` | DEV: a touch at pixel `x,y` of 1280x720, or on the level's first help block, released on the next tick or at `release`. Taps go one at a time (the next goes down after the last has come up); in a level under `--controls modern` with two contacts on they overlap where two fingers can, each a finger of its own, so `--tap` on an arrow held for a while and another `--tap` that shoots play a thumb that keeps walking while the other finger fires; two that would hold the same place (both in the world, both on a control) still go down in order, and a menu, a popup or the loading screen always takes them one at a time |
| `--drag <x,y>:<x,y>@<tick>-<release>` | DEV: a touch that moves evenly from the first pixel to the second |
| `--place <x>,<y>@<tick>` | DEV: put the player at the level's pixel `(x, y)` after that tick's physics step, to film what a door opens; never a win route; pass `--saves` with it, since a win records a medal |
| `--press <what>@<tick>` | DEV: press `pause`, a pause button (`levels`, `resume`, `skip`, `achievements`, `sound`, `music`), or `back` on a menu |
| `--saves <dir>` | DEV: keep the medals (`scores.json`), the sound and music switches (`audio.json`) and the controls chosen in Options (`controls.json`) here instead of the user's data directory, for a capture that needs a medal recorded, presses a switch or chooses a style; without it a capture reads the player's switches, which its pause and menus show |
| `--levels <dir>` | read the converted levels from here instead of the configured path |
| `--art <dir>` | the directory the levels' art paths resolve against (default: the one above the levels) |
| `--data <dir>` | read Enhanced's `data/` from here instead of the configured path |
| `--padart <absolute dir>` | the folder a `--controls modern` run looks for Magic Rampage's five control pictures in (`walk_left.png`, `walk_right.png`, `restart.png`, `pause.png`, `clear_portals.png`, and the tool's `manifest.json`), instead of the one the build was told (`SUPERSONIC_MAGICPORTALS_PADART`). Absolute, because the game makes `engine/` its working directory, and given once (twice exits 1, as `--controls` does); an empty folder shows the next tiers. The adjustment screen's eight pictures are looked for in the folder `editor` beside it (`<dir>/../editor`). A picture there that cannot be read or decoded, or whose size is not the manifest's (else 128 x 128), is skipped with a WARN. Never part of a release package: the pictures are the owner's, read from where they are (`docs/magic-rampage-controls.md`) |
| `--controls <classic\|modern>[,key=value...]` | which on-screen controls the run plays: the original's (`classic`) or Magic Rampage's inset ones (`modern`; keys `preset=uniform\|balanced\|mr`, `arrows=split\|grouped`, `contacts=one\|two`, `size=`, `opacity=` and a move as `<control>=<dx>:<dy>`). A run with none of the rig's flags (`--fixed-step`, `--route`, `--visit-levels`) has the Options screen and plays what it kept (shipped default: modern, since planning step 198); with this flag the run STARTS in that style, ignores the kept choice and writes nothing (Options then chooses for the run only), `classic` has no Options entry at all, and a rig run is classic unless the flag says otherwise (a rig run has no entry either). **The layer plays the modern layout (CTL-6 of the modern-controls plan): five inset controls (the two walk arrows one in each bottom corner, restart and pause at the top right, clear-portals at the top left; `arrows=grouped` puts both arrows at the bottom left, as the first layout did), their pictures read at run time from Magic Rampage's own files in the gitignored `reference/magic-rampage/controls/` where they are there (`--padart`, below), else the original's own sprite for restart, pause and clear-portals, else a glyph the game draws and writes to a temporary directory (only for a control that reaches it, and only when the file there differs), else a flat quad - the log says which, once: `modern controls art: walk_left=mr walk_right=mr restart=mr pause=mr clear_portals=mr` (planning step 184); restart, pause and clear act on the release inside their area after a press inside it. Its log says `controls: modern (--controls modern)` at attach and, at each level start (the game's first level says it on its first tick, when the window is known), `controls: modern, walk_left (x,y)-(x,y) px hit (x,y)-(x,y) px, ...` (the settled boxes and hit areas in the window's pixels). Its keys are all played (planning step 187): `preset=` and `arrows=` choose the look and the arrangement (neither is ever kept), `size=` (0.4 to 1.4 by 0.1; 1.4 is the classic pad's 64 units), `opacity=` (0.2 to 1.8 by 0.2; it multiplies the byte the control is drawn at and stops at 255) and a move as `<control>=<dx>:<dy>` (design units from the control's own corner, never scaled by the size, pulled in by the layout where the view cannot hold it) tune it. A flag that carries a size, an opacity or a move REPLACES the tuning a player kept - it never merges with it - and writes nothing; a value off its grid exits 1 with the usage; the log says `controls: tuning from the flag: size 1.4, opacity 1, ...` or, for the kept one, `from controls.json`. The classic controls read none of it.** `contacts=one\|two` (planning step 191) says whether the run reads a second finger, over `controls.json`'s `input.two_contacts` (shipped on); like `preset=` and `arrows=` it is a capture's choice and never kept, and the log's `fingers on tick N: 2 down, controls finger ..., world finger ...` line says when two were read. Under it 1-21's "move the camera around" starts clear of the left arrow and stays on the screen at every window shape (where the text is wider than the free span between the two arrows it runs over the right one, as the classic text crosses its disc), and a device's safe area (a notch, a home bar) moves the controls in (planning step 183). A flag that is neither style, or a key that is not one, exits 1 with the usage; `docs/planning/2026-10-15-modern-controls.md` |
| `--light-masks-off` | DEV: draw lights and halos but let no light reach a sprite |
| `--shadow-maps <0\|16..4096>` | DEV: the shadow maps' size in pixels for this run (`0`: the engine's own 2048 and 1024); over `game/data/engine.json`, for an A/B. See *Performance options* |
| `--sprites-only <on\|off>` | DEV: build the scene pipelines for unlit draws only (every sprite, particle and halo); the lit boxes of the `B` key then draw flat. Over the table |
| `--asset-watching <on\|off>` | DEV: the engine's stat of every watched file on every frame (hot reload of a texture a developer saved). Over the table |
| `--sprite-report <on\|off>` | DEV: the layer's per-frame walk over its sprites that logs one crossing the view's edge (`on`/`off`/`GONE` lines). Over the table; `G` dumps all either way |
| `--perf-log <seconds>` | DEV: the engine's frame statistics (fps, median, 95th percentile, worst, where the frame went) in the log every `<seconds>`; off unless asked |
| `--visit-levels <lightmapped\|all\|name,...>` | DEV: visit these levels in one process; the run fails unless the engine's material descriptor sets return to its pool |
| `--visit-passes <n>` | DEV: how many times to walk that list (default 1) |
| `--route <file>` | DEV: replay a win route (`tests/routes/<level>.json`, absolute) in the window, opening its level; says on exit whether it won. Pair it with `--saves <dir>`: a run past the level's end writes its medal |
| `--route-trace <n>` | DEV: with `--route`, print the route's trace (`test_mp_routes --trace`'s lines) every n ticks, on each tap and at the end |
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
tools\parity\.venv\Scripts\python -m pip install numpy pillow opencv-python-headless imageio-ffmpeg pytest ruff
```

`tools/parity/` holds the scripts; the command-line ones carry their command line in their
docstring:

- `emu.py` — drives the original in the emulator over adb;
- `capture_original.py`, `capture_screens.py` — build the reference library, verifying each
  screen (`markers.py`) before tapping;
- `locate.py` — finds where, and at what scale, a sprite is drawn in a frame;
- `compare.py` — one original frame against one port frame: offset, edge overlap, mean
  absolute difference, a pair image and an overlay;
- `frames.py` — turns a screen recording into frames and contact sheets;
- `reachability.py` — whether each converted level's door can be reached in the port, pinned in
  `reachability.json`; its tests (`tests/`, on hand-authored scenes) and ruff run from
  `tools/parity` as `.venv/Scripts/python -m pytest -q` and `.venv/Scripts/python -m ruff check .`;
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
- `docs/magic-rampage-controls.md` — the modern controls' art: where it comes from, what the tool writes, the rights caveat
- `docs/planning/2026-09-11-magic-portals-remaster.md` — the port's record: every step,
  with its gates; beside it, the port's plan and the spike that started it
- `engine/README.md`, `engine/AGENTS.md`, `engine/ARCHITECTURE.md` — the engine the port
  is built on, and the conventions port code follows
