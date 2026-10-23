# Where the game finds its data, and how a package is built

Magic Portals Enhanced reads three kinds of data at run time: the levels the converter wrote, the original's art and sound, and its own
tables (`data/`, `game/data/`). None of the first two is in this repository (`CLAUDE.md`, the hard rule), so the game has to be told where
they are. Since planning step 200 it works that out when it starts; it no longer depends on the paths of the machine that built it.

This page is the record of that rule, of the folder a package holds, and of the tool that builds it. The step is in
[`planning/2026-09-11-magic-portals-remaster.md`](planning/2026-09-11-magic-portals-remaster.md) (Step 200); the code is
`game/sim/AssetRoot.{hpp,cpp}`, `game/sim/Scratch.{hpp,cpp}` and `tools/package/`.

## 1. How the game finds its data

At start the game looks for a *root*: the first folder from this list that holds the game, meaning it has the converted levels and the
chapters table. A folder is read in either of the two shapes in section 2.

| # | Tier | Folder |
|---|---|---|
| 1 | `--root <absolute folder>` | the folder you name. **Strict**: one that does not hold the game ends the run with a message that names it (exit 1); it is never skipped for a later tier. A relative path is refused. |
| 2 | next to the program | `<program folder>/content`, then `<program folder>` itself |
| 3 | the application bundle | a mobile or macOS entry point passes it (no desktop does) |
| 4 | the user's data folder | `<data folder>/Magic Portals/content`: `%APPDATA%\Magic Portals\content` on Windows, `~/.local/share/Magic Portals/content` (or under `$XDG_DATA_HOME`) on Linux, `~/Library/Application Support/Magic Portals/content` on macOS. `content` is only looked at, never made. The `Magic Portals` folder above it is the player's data folder and the engine makes it when it names it (`Supersonic::UserDataDirectory`), which the game asks whenever tiers 1-3 did not answer: so **a developer's run, and a failed start, leave an empty `Magic Portals` folder where there was none** (it is where the saves go anyway; the engine has no variant that only looks, and the resolver reads no environment variable to do without it: planning step 200, Recorded Deviation 2). |
| 5 | the paths the build was configured with | what a developer's build has always used (`SUPERSONIC_MAGICPORTALS_*` in CMake). Taken as they are, without asking whether they hold the game, so a build with no data still starts and draws boxes, as before. |

Nothing reads an environment variable for this. A tier that has no answer is skipped; when none answers the game says where it looked and
exits 1.

The first lines of the game's log name the root and the tier (`asset root: ... (tier: ..., layout: ...)`), then the scratch folder
(section 5), so a report from another machine says what it found.

**Flags** (beside the existing `--levels`, `--art`, `--data`, `--padart`): `--root`, `--original`, `--chapters`, `--achievements`,
`--port-data`. Each replaces the answer for that one path and nothing else. `--levels` brings the art with it (the folder above the levels,
as it always has) unless `--art` is given, and does not move the chapters table. **`--no-baked-paths`** ignores tier 5 for the run, so a
package can be proven to hold everything the game opens.

**A release build.** `-DMAGICPORTALS_BAKED_PATHS=OFF` (default ON) compiles every data path to an empty string, sets the engine's own
baked checkout path to empty, and maps the checkout and build folders out of the compiler's source paths, so the program carries no
account name and no absolute repository path (measured with g++ on Linux: the binary holds no `icvet`, no `/mnt/c` and no `/home`; the
MSVC build with the option off has not been made and has no `/d1trimfile` yet, which the Windows release step owns). It then has no tier 5: a run with no root fails with the message above, never with a stray
relative open. The suites read the baked paths, so the tests are not built by default in that configuration. Use a build folder of its own
(`tools/build_linux.sh --no-baked-paths --build-dir <folder>` does).

Every path handed on to the game is spelled out: no `..` segment is left in a resolved path.

## 2. The two shapes of a root

`Layout::Repo()` is this repository, relative to its root: `out/levels`, `out/data/chapters.json`, `out/data/achievements.json`, `data`,
`game/data`, `reference/extracted/assets`, `reference/magic-rampage/controls`, and `out` as the folder `res://` stands for (the converted
art is `out/assets/...`). It is what a developer's checkout, or a copy of one, looks like.

`Layout::Packaged()` is the folder the packager writes and a release ships. A folder is tried as Packaged first, then as Repo.

```
content/
  levels/              the converted levels (*.tscn)
  assets/              what res:// stands for: assets/entities/** (the converted art, tier folders included)
  tables/              chapters.json, achievements.json
  data/                Enhanced's tables
  port-data/           the port's own tables
  original/            the original's files the game opens: sprites/, entities/, particles/, data/, soundfx/
  pad-art/             Magic Rampage's control pictures (controls/, editor/): only with the packager's --with-pad-art
  manifest.json        every file, with size, sha256, kind and role; and the references that name nothing in the data
  SHA256SUMS.txt       the same hashes in `sha256sum -c` form
```

A desktop package puts the program beside it (`package.py shell` lays this out and proves it):

```
MagicPortals(.exe)
assets/shaders/*.spv   the engine opens its 17 shaders by relative path from the program's own folder
content/               the folder above
```

The engine makes the program's folder its working directory when it holds `assets/shaders`. It writes there (a pipeline cache), so it
belongs in a folder the player may write to: a portable folder, or a per-user install; a read-only install location loses the cache.

## 3. What the packager puts in

**Before you run it** you need the owner's inputs on the machine; none of them is in this repository (README, *Local setup after cloning*,
has the steps):

1. the original's APK unpacked into `reference/extracted/` (its `assets/` folder is `<reference>/extracted/assets`);
2. the converter run, so that `out/levels`, `out/data` and `out/assets` exist (`tools/converter`, and `tools/asbc` for `out/data/achievements.json`);
3. optionally `reference/magic-rampage/` (the modern controls' pictures; `docs/magic-rampage-controls.md` says where they come from), for
   `--with-pad-art`.

```
python tools/package/package.py content --repo <this repository> --reference <the reference folder> [--out <folder>] [--with-pad-art]
```

It runs on the owner's machine (run on Windows and Linux; macOS has not been run), reads the owner's data, and writes by default to `<repo>/out/dist/content`.
It is not run in CI, and nothing it writes can go into the repository: it refuses an output folder in the tracked tree. See
[`tools/package/README.md`](../tools/package/README.md) for what it refuses and how it is tested.

The set of files is **derived from the tables each time**, not listed: the converted levels, the two tables beside them, the committed
tables, and exactly the files those name (a texture in its density tiers `fullhd/`, `hd/`, then as itself; the entity files; the particle
bitmaps each entity names; the fonts and their pages; the sounds and the music), then what the entity files themselves name (the picture
in `<Sprite>`, the normal map in `<Normal>`, the light's `<HaloBitmap>`, and the colour variants: the entity's own name in front of a
suffix a table spells, such as `_blue.png`), plus the 1x file under a tier twin that was picked.
On the owner's data (2026-10-23) that is 883 files, 24,584,458 bytes (725 required, 158 fallback: 472 pictures, 180 entity files, 54 tables,
128 levels, 41 sounds, 5 music tracks, 3 fonts); 896 files and 24,704,515 bytes with `--with-pad-art`; `package.py shell` adds the 17
shaders (118,716 bytes). Compared with the research's list (`PLAT/case_audit/package_files.csv`, 757 files and the 17 shaders): 5 page
pictures of fonts that no table names are left out (651,321 bytes), and 131 files are added (4,298,588 bytes): 55 sprite pictures, 27
normal maps, 1 halo and 6 colour variants of entities the closure already held, and the 1x fallbacks under them (42). **The first run of the
sweep from a package built without them found the gap**: eight pictures the game opens (the black bars' own, a shooter's bolt, the
coloured keys, keyholes and locked doors) are named by no table, only by an entity file or by a suffix; every other file, size and role is
equal to the research's.

**Left out, on purpose:** the converter's lightmap pictures (`out/assets/lightmaps`, 730 files): the layer bakes a lit sprite's lightmap in
memory and never opens them (planning step 196), which the 128-level sweep run from a package without them proves; the original's
`scenes/` (the converter's input), `.pvr`, `.pkm`, `.par`, the framework folder, the Android bundle, `.enml` files, the APK, the engine
checkout, build folders, the converter's other output. A table that named one would be an error.

**Not yet:** sound off Windows (the engine decodes MP3 only there), the Android, iOS and macOS entry points, release archives and licence texts.
They are later steps of the platform program. (The Windows release program and its icon are section 6.)

## 4. Proving a package

* `package.py verify <folder>`: the manifest, every hash, nothing surplus, nothing on the never-package list, and the model of what the
  game opens run over the package itself: 0 references that match only ignoring case, the same references that name nothing as the
  source run, every lightmap key absent. `content` runs it before it moves the folder into place.
* `package.py same <a> <b>`: two runs are the same bytes in the same order.
* The 128-level sweep from the package alone: `MagicPortals --root <folder> --no-baked-paths --level <name> --fixed-step --frames 420
  ...` for every level, byte for byte against the sweep from the repository's own data. This proves the levels; the menu, the HUD and the
  dashboard are proved by `verify`'s model and by captures of the screens. **The proof is the byte comparison, not the game's log**: the
  game does not always say when a picture it wanted is missing (the verifier removed two pictures the first level draws from a copy of a package:
  the log did not change, the picture did), so "no log line says `could not be read`" is not evidence that nothing is missing. The
  comparison's power was measured that way, and the closure was cross-checked by tracing every file a game process opens over 150 planned
  Linux runs (the 128 win routes, the 18 screens, 4 modern-controls runs): 504 data files, all in the package except the optional pad art.
  Sound is not traced (Linux cannot decode MP3); the package holds every sound file of the original.

## 5. Where the game writes

The log (`magicportals.log`, and the last run's `.prev.log`) and the files the game generates and could make again (the collision
prisms the levels' polygons become, the control glyphs, the multiplies) go to a *scratch folder*, chosen when the game starts as the first
of these where a file can really be made (it is tried, not assumed):

1. the system's temporary folder (`%TEMP%` on Windows; the log is `<it>\magicportals.log`, the rest in `<it>\supersonic-magicportals`);
2. `<user data folder>/scratch`, the one place that is made if it is not there, and only if the first was refused;
3. the older last resorts: the user data folder, the program's folder, the working directory.

The second matters on Android, where the temporary folder the C++ library names is one an app cannot write, and on a shared `/tmp` whose
prism folder belongs to another account. The half-written prism file has a name of its own per call, so two windows building one level
cannot take each other's file.

The player's saves (`scores.json`, `audio.json`, `achievements.json`, `controls.json`) stay where they were: the user data folder itself.

## 6. Build options, and which belong in a release

Planning step 201 (PORT-3). Five CMake options say what kind of program a configure makes. **At their defaults nothing moves**: `build/` is the
program it was, and the 128-level sweep, the 19 baselines and the 128 win routes are byte for byte what they were (Step 201's gates). Each option
is read once, at the top of `CMakeLists.txt`, and reaches `game/main.cpp` alone (a per-source definition: the suites are not rebuilt for it).

| Option | Default | What it does | In a release build |
|---|---|---|---|
| `MAGICPORTALS_BAKED_PATHS` | ON | section 1: the six data paths are compiled in | **OFF** |
| `MAGICPORTALS_DEV_TOOLS` | `AUTO`: ON, and OFF with `MAGICPORTALS_MOBILE`; `ON` or `OFF` says it outright | the developer's command-line flags and the two files behind them (`game/LevelVisit.cpp`, `game/Route.cpp`) | **OFF** |
| `MAGICPORTALS_MOBILE` | OFF, and ON by itself for an Android or iOS target | the run starts under the phone profile; no developer tools; no measurement tool (`MagicPortalsSpike`) | ON for a phone's build, and only there |
| `MAGICPORTALS_WIN_GUI` | OFF | the Windows release program (below) | **ON** on Windows, with MSVC; a configure with it ON on anything else stops |
| `MAGICPORTALS_ICON` | empty | an `.ico` for the release program (nothing is committed: `tools/art/make_app_icon.py` draws one) | the file that tool writes, or the owner's own; used only with `MAGICPORTALS_WIN_GUI` |

The version is `project(MagicPortals VERSION 0.1.0 ...)` in the top-level `CMakeLists.txt`: **one definition**, and the Windows file properties
(`FileVersion`, `ProductVersion` 0.1.0.0) are made from it. 0.1.0 is a pre-release placeholder; what the first release is called is the owner's.

**The Windows release, exactly** (from a shell where MSVC is on `PATH`, e.g. after `vcvars64.bat`; use a build folder of its own that is
gitignored, such as `out/build-release` (`/out/` is ignored), and not `build-play/`, which is the owner's play build
(`tools/parity/play_build.bat`) and must stay the console game, so that `build/` stays the console program every script launches):

```
python tools/art/make_app_icon.py                      (writes out/dist/icons/, nothing under version control)
cmake -S . -B out/build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DSUPERSONIC_ENABLE_VALIDATION=OFF ^
      -DMAGICPORTALS_BAKED_PATHS=OFF -DMAGICPORTALS_DEV_TOOLS=OFF -DMAGICPORTALS_WIN_GUI=ON ^
      -DMAGICPORTALS_ICON=<repository>/out/dist/icons/magic_portals.ico ^
      -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl -DGLSL_COMPILER=<Vulkan SDK>/Bin/glslc.exe
cmake --build out/build-release --target MagicPortals
```

`MAGICPORTALS_BAKED_PATHS=OFF` builds no suite (they read the baked paths), so no `test_mp` is linked. Not yet part of it: nothing maps the
checkout's path out of what MSVC compiles in (`__FILE__`, assertion text, the program database's path); the GCC and Clang builds do (section 1).
That is the packaging and CI step's.

**What was actually built and run (planning step 201):** the program that was linked and launched was configured with `MAGICPORTALS_WIN_GUI=ON`,
`MAGICPORTALS_DEV_TOOLS=OFF`, the icon, **`MAGICPORTALS_BAKED_PATHS=ON`** (so that it finds its data without `--root`) and Vulkan validation ON.
The command above, with baked paths OFF and validation OFF, was configured in a scratch tree and its `main.cpp` and resource were compiled with
no warning, but **that program was not linked or run**: a Windows build with baked paths OFF has never been made into an executable. The proof of
the root logic (a run from a package with `--root` and `--no-baked-paths`) is step 200's, on the other program.

**What `MAGICPORTALS_WIN_GUI` makes** (`game/windows/MagicPortalsWindows.cmake`, one function the rehearsal of the link calls too):

* **No console window.** The program's subsystem is Windows' GUI (`WIN32_EXECUTABLE`), with `main()` still the entry point
  (`/ENTRY:mainCRTStartup`). A shell that redirects standard output and standard error to a file or a pipe still receives them (the handles are
  inherited, which the subsystem does not change); started from Explorer there is none (not run from Explorer here: every launch of this step had redirected handles), and the log (section 5) is where its words go. **A
  consequence to know: `--help` shows nothing in a terminal that has not redirected it**, because a GUI program is not attached to the terminal's
  console (Windows' rule for the subsystem; not tried in an interactive terminal here); `MagicPortals --help > help.txt` works, which is how it was
  read.
* **`vulkan-1.dll` is loaded late** (`/DELAYLOAD`, `delayimp.lib`): at the first Vulkan call and not at process start. At process start, a PC
  with no graphics driver at all gets Windows' own dialog, which names a DLL and nothing else, and the game never runs. Late, `main()` asks first
  (`sim/StartChecks.hpp`, `CheckVulkanLoader`, asked of `LoadLibraryExW(L"vulkan-1.dll", nullptr, 0)`, the search the delay-load helper makes
  itself): a loader that will not load ends the start with ONE message box for a person's run ("No Vulkan graphics driver was found: Windows could
  not load vulkan-1.dll ... Install or update the graphics driver from the maker of your graphics card (NVIDIA, AMD or Intel), or run Windows
  Update ... and start the game again."), the same text in the log and on standard error, and exit code 1; a script's run (any engine flag that
  makes it unattended) gets the log and the exit code and no box. The check is asked on every Windows build, delay-loaded or not (where the loader
  is linked at process start the answer is yes, because the program is running). **No run showed the box:** `vulkan-1.dll` cannot be taken from
  this machine, so the box is covered by the pure question and its words (`test_mp_platform`) and by reading the code.
* **File properties** (`game/windows/MagicPortals.rc.in`): `FileDescription` and `ProductName` "Magic Portals Enhanced", `FileVersion` and
  `ProductVersion` from the one version, `InternalName` and `OriginalFilename` `MagicPortals(.exe)`, `CompanyName` "Ivan Cvetanovic",
  `LegalCopyright` "Enhanced edition by Ivan Cvetanovic. Based on Magic Portals by Asantee Games." The words claim no right over the original; the
  owner confirms them. The application manifest (UTF-8 code page, step 189) is not in the resource: it is a source of the target and the linker
  merges it.
* **The icon**, when `MAGICPORTALS_ICON` names a file: resource 1, the file's six sizes. Without it the program has none.

**What the program still loads at process start** (read from the import table of the default `MagicPortals.exe`; reported, not changed): `ole32.dll`, `MFPlat.DLL` and `MFReadWrite.dll` (the engine's Media Foundation, which decodes the original's MP3 sounds on Windows),
`KERNEL32`, `USER32`, `GDI32`, `SHELL32`, `IMM32`, `MSVCP140.dll`, `VCRUNTIME140.dll`, `VCRUNTIME140_1.dll` (the dynamic C++ runtime) and the
`api-ms-win-crt-*` set (the Universal CRT). A PC without the Visual C++ runtime, or a Windows N edition without the Media Feature Pack, would meet
Windows' own "the code execution cannot proceed because ... was not found" dialog before `main()` runs, and no message of this program can help
it. **This was not seen on such a machine**; it is a reading of the import table. Delay-loading Media Foundation is the engine's change, and the
choice of a static runtime is the whole build's (the engine would be built the same way); neither is made here. A release package therefore
either carries the Visual C++ redistributable's DLLs beside the program or tells the player to install it: the owner's decision, and the
packaging step's.

**`MAGICPORTALS_DEV_TOOLS=OFF`** takes out the developer's flags: `--route`, `--route-trace`, `--tap`, `--hold`, `--drag`, `--press`,
`--place`, `--burn-dragon`, `--visit-levels`, `--visit-passes`, `--light-masks-off`, `--shadow-maps`, `--sprites-only`, `--asset-watching`,
`--sprite-report` and `--perf-log`. Naming one ends the run with exit code 1 and "`--route` is a developer's flag, and this build was made
without the developer tools ..."; `--help` does not list them; an unknown flag is the engine's parser's to refuse, as before. What a player's
build keeps: `--level`, `--root`, `--no-baked-paths`, `--levels`, `--art`, `--data`, `--original`, `--chapters`, `--achievements`, `--port-data`,
`--padart`, `--saves`, `--controls`, `--profile`, `--help` and every flag of the engine (`--window`, `--fixed-step`, `--frames`, `--screenshot`
...). (`--saves` is described in `--help` as a developer's flag, and stays: a capture and a package proof need it.) The suites always build with
the tools: `test_mp_routes` needs `Route.cpp`. The program is smaller (Linux, g++: 11,860,880 bytes against 11,986,016).

**The phone profile.** `game/data/engine.json` has a `mobile` block: the answers a phone's run gives where they are not the desktop's. A build
made with `MAGICPORTALS_MOBILE=ON` starts under it; `--profile mobile` asks the same of a desktop run (for a test), `--profile desktop` the
other way. The block is read, and checked, by every run, so a typo in it fails where the file is read. Its keys, each of which replaces the
desktop's answer when present:

| Key | Value | Why |
|---|---|---|
| `minimum_vulkan_minor` | 1 | the lowest Vulkan 1.x a GPU may report. The engine wants 1.2; the stock drivers of many phones on Android 11 and 12 report 1.1, so with the default the start ends before the first frame (the sibling port's step E39). A desktop run leaves it out and asks for 1.2. The log line says what the run was given: `... minimum Vulkan 1.2` or, under the profile, `1.1` |
| `shadow_map_resolution` | 16 | as the desktop's own key, repeated so that changing the desktop's does not move a phone |
| `asset_watching` | false | the engine's per-frame stat of every watched file, for both kinds of run: a phone has nobody to hot-reload for |

The scene's sample cap and dynamic resolution are **not** in the block, on purpose: they are the owner's decision (the block's
`_decision_pending` note says so), and a key for them is refused when the file is read until code that reads it exists. As shipped, nothing in the
block changes a picture on a desktop (the three keys concern which GPU is acceptable, the size of shadow maps, and a file watch);
`--profile mobile` on a desktop is byte-identical to the default for three levels at frame 420. `shadow_map_resolution` is the one key of the block
that would change pixels if it were edited: as shipped it equals the desktop's own 16, so it changes nothing, but the developer's lit boxes
sample the maps, and `engine.json`'s own `_about` records 165,280 of 921,600 pixels differing between 16 and the engine's sizes. The other keys
that could change a pixel are the ones that are not there: `scene_samples` (edge pixels), `sprites_only_scene_pipelines` (only the developer's
lit boxes), dynamic resolution (only outside a fixed-step run), and `minimum_vulkan_minor` only through which GPU a machine with more than one picks.

**The icon tool.** `python tools/art/make_app_icon.py [--out DIR]` draws the icon (two concentric rings, blue and orange, on a dark disc) with
arithmetic alone: nothing is read, no file of the original is opened, and the output is the same bytes on one machine twice. Into `out/dist/icons`
by default: `icon_<1024 512 256 192 128 64 48 32 16>.png`, `magic_portals.ico` (16 to 256, PNG-compressed entries), `android/mipmap-*/ic_launcher.png`
(48 72 96 144 192), `android/play_store_512.png` and `SHA256SUMS.txt`. It refuses to write inside the tracked tree. The colours are its own
choice (the original's portals are blue and red; the orange is for legibility on a dark disc): the owner may want another.
