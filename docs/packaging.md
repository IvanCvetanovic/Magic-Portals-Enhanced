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
as it always has) unless `--art` is given, and does not move the chapters table. **`--no-baked-paths`** ignores tier 5 for the run, so a package can be proven to hold everything the game opens. A per-path flag replaces the answer for that one path and cannot stand in for the tier: with `--no-baked-paths` and all eight per-path flags but no `--root`, the run still exits 1 with "data not found" (a design note, planning step 208; PORT2-6 (d)).

**A release build.** `-DMAGICPORTALS_BAKED_PATHS=OFF` (default ON) compiles every data path to an empty string, sets the engine's own
baked checkout path to empty, and maps the checkout and build folders out of the compiler's source paths, so the program carries no
account name and no absolute repository path (measured with g++ on Linux: the binary holds no account name, no `/mnt/` path and no `/home/` path; a release program of MSVC with the option off was made in step 202 (`out/build-release`) and step 208's `release.py check-program` reads it: none of the strings it looks for (eight fixed ones, the name of the account that runs the check, and this checkout's own path) occurs in it. Nothing maps the checkout's path out of what MSVC compiles in (`/d1trimfile`), and the debug directory of both Windows programs read here (the developer's and step 202's release program) has one entry, of type 13, and no CodeView entry, so neither names a program database (a PE reader, `impl/pe_debug_directory.txt` of step 208); a program linked with `/DEBUG` is not covered by that reading, and `check-program` refuses a program that holds any of the strings). It then has no tier 5: a run with no root fails with the message above, never with a stray
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
On the owner's data (2026-10-23, taken again by Step 203's own run and by its verifier's) that is 883 files, 24,589,032 bytes (Step 200 measured
24,584,458; the 4,574 bytes between are presumably the edits of `engine.json` and `sounds.json` in Steps 201 and 202, inferred and not measured;
725 required, 158 fallback: 472 pictures, 180 entity files, 54 tables, 128 levels, 41 sounds, 5 music tracks, 3 fonts); 896 files and
24,704,515 bytes with `--with-pad-art` (Step 200's figure, **not retaken**: presumably the same 4,574 bytes higher now); `package.py shell` adds the 17
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

The Android build and the APK packager (`tools/package/android_package.py`, which takes this packager's output folder) are described in [`android.md`](android.md) (planning steps 203 and 204; built, statically checked and run on an x86_64 emulator, never on a device). The macOS and iOS entry points, their bundle layouts and the shell a CI runner makes are in [`apple.md`](apple.md) (planning step 206: **written; run on GitHub's macOS runner, never on a person's Apple machine**; section 7 below). Release archives are section 8 (planning step 208); the licence and the notices are `LICENSE` and `NOTICE.md` at the repository's root (planning step 210). (The
Windows release program and its icon are section 6. The original's 46 MP3 sounds decode off Windows since the engine pin of planning step 202,
through the engine's vendored dr_mp3: that was checked on the real files, with no sound device, so it proves the decoding and not the playback;
a clip starts about 12 ms apart from the same clip on Windows, which comes out as if its first 528 samples were cut (the engine's
audit, inferred from the frame counts of 46 files without an encoder header: it is not read out of Media Foundation).)

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
  Sound was not traced (those runs were made before the engine decoded MP3 off Windows, on a machine with no sound device, where the layer
  drops its sound events); the package holds every sound file of the original.

## 5. Where the game writes

The log (`magicportals.log`, and the last run's `.prev.log`) and the files the game generates and could make again (the collision
prisms the levels' polygons become, the control glyphs, the multiplies) go to a *scratch folder*, chosen when the game starts as the first
of these where a file can really be made (it is tried, not assumed):

1. the system's temporary folder (`%TEMP%` on Windows; the log is `<it>\magicportals.log`, the rest in `<it>\supersonic-magicportals`);
2. `<user data folder>/scratch`, the one place that is made if it is not there, and only if the first was refused;
3. the older last resorts: the user data folder, the program's folder, the working directory.

A phone's build (`MAGICPORTALS_MOBILE`) tries the second FIRST (`Scratch::ArrangedFor`, planning step 203), so that its log is always `<files>/Magic Portals/scratch/magicportals.log`,
the one path the Android manifest can name for the system's report of a run that ended unexpectedly; a desktop keeps the order above. The second matters on Android, where the temporary folder the C++ library names is one an app cannot write, and on a shared `/tmp` whose
prism folder belongs to another account. The half-written prism file has a name of its own per call, so two windows building one level
cannot take each other's file.

The player's saves (`scores.json`, `audio.json`, `achievements.json`, `controls.json`) stay where they were: the user data folder itself.

## 6. Build options, and which belong in a release

Planning step 201 (PORT-3). Five CMake options say what kind of program a configure makes. **At their defaults nothing moves**: `build/` is the
program it was, and the 128-level sweep, the 19 baselines and the 128 win routes are byte for byte what they were (Step 201's gates). Each option
is read once, at the top of `CMakeLists.txt`, and reaches every source file of the `MagicPortals` program and nothing else (a definition of that
target, `MAGICPORTALS_MOBILE` and `MAGICPORTALS_DEV_TOOLS`: the suites and the libraries are not rebuilt for it). `game/BuildProfile.hpp` stops a
translation unit that is given neither those two nor a suite's own declaration, so a second file of a mobile program cannot disagree with `main.cpp`
(planning step 202).

| Option | Default | What it does | In a release build |
|---|---|---|---|
| `MAGICPORTALS_BAKED_PATHS` | ON | section 1: the six data paths are compiled in | **OFF** |
| `MAGICPORTALS_DEV_TOOLS` | `AUTO`: ON, and OFF with `MAGICPORTALS_MOBILE`; `ON` or `OFF` says it outright. **Exactly these three words** (in any case): anything else (`OF`, `0`, `NO`, an empty value) stops the configure, naming the value | the developer's command-line flags and the two files behind them (`game/LevelVisit.cpp`, `game/Route.cpp`) | **OFF** |
| `MAGICPORTALS_MOBILE` | OFF, and ON by itself for an Android or iOS target | the run starts under the phone profile; no developer tools; no measurement tool (`MagicPortalsSpike`) | ON for a phone's build, and only there |
| `MAGICPORTALS_WIN_GUI` | OFF | the Windows release program (below) | **ON** on Windows, with MSVC; a configure with it ON on anything else stops |
| `MAGICPORTALS_ICON` | empty | an `.ico` for the release program (nothing is committed: `tools/art/make_app_icon.py` draws one) | the file that tool writes, or the owner's own; used only with `MAGICPORTALS_WIN_GUI` |

The version is `project(MagicPortals VERSION 0.1.5 ...)` in the top-level `CMakeLists.txt`: **one definition**, and the Windows file properties
(`FileVersion`, `ProductVersion` 0.1.5.0) are made from it. 0.1.0 began as a pre-release placeholder and is the number of the files of the first release (section 8); 0.1.1 is the second release's (planning step 212: the loading screen's portal, the credits' closing card, the sides of the menu pictures and the original's icon); 0.1.2 is the third's (planning step 214: a body off the floor keeps all its x velocity, which makes level 1-04's gold and 2-30's designed leap reachable); 0.1.3 is the fourth's (planning steps 215 and 216: a placed pair carries what is already inside a portal when its partner opens and swaps two bodies, and the player has the original's friction against a rolling stone); 0.1.4 is the fifth's (planning step 218: a stone the engine's brake stopped short of a breakable wall strikes it at the speed it carried, which 1-22's second wall needed); 0.1.5 is the sixth's (planning step 221: a destroyier kills no player, which 1-24's finish door, standing on one, needed); whether a public release keeps the numbering is the owner's.

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

`MAGICPORTALS_BAKED_PATHS=OFF` builds no suite (they read the baked paths), so no `test_mp` is linked. Not part of it: nothing maps the checkout's path out of what MSVC compiles in (`__FILE__`, assertion text, the program database's path); the GCC and Clang builds do (section 1). Planning step 208 measured the result instead of adding the mapping: step 202's release program holds none of the strings `release.py check-program` looks for (section 8), and the check refuses any build that does. A mapping would be a change of `game/` or of CMake, and is step 209's if it is wanted.

**What was actually built and run.** Step 201 linked and launched a program configured with `MAGICPORTALS_WIN_GUI=ON`, `MAGICPORTALS_DEV_TOOLS=OFF`,
the icon, `MAGICPORTALS_BAKED_PATHS=ON` and Vulkan validation ON (a tree it later deleted), and only configured and compiled the command above.
**Planning step 202 linked and launched the command above itself** (baked paths OFF, validation OFF, the icon) in `out/build-release`:
one `MagicPortals.exe`, GUI subsystem, `vulkan-1.dll` in the delay-load table alone, the version information and the icon in it, 13 `D9025` lines
(the engine's vendored sources) and no other warning. It finds nothing by itself, so the launches gave it `--root <this repository>` and
the 17 shader files beside it, and then level0 and 4-32 at frame 420 came out byte for byte as the default program's do, a developer's flag was
refused with exit 1, and a non-image `vulkan-1.dll` placed beside it ended the start with the Windows-error-193 text (the message box and the
126 text were not seen). **What a program built with `MAGICPORTALS_BAKED_PATHS=OFF` needs, as that run showed:** the engine's shaders in
`assets/shaders` beside the program (the engine makes the program's folder its asset root when it holds them, and writes its pipeline cache
there), and its data from `--root`, a `content` folder beside it or the user's data folder (section 1). The proof of the root logic from a
package is step 200's, on the other program.

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
  itself): a loader that will not load ends the start with ONE message box for a person's run, the same text in the log and on standard error, and
  exit code 1. The words follow the code Windows gave (`GetLastError`, planning step 202): **126**, a file not found ("No Vulkan graphics driver was
  found: Windows could not load vulkan-1.dll ... because it or a file it needs was not found (Windows error 126) ... Install or update the graphics
  driver from the maker of your graphics card (NVIDIA, AMD or Intel), or run Windows Update ... and start the game again."); **193**, a file that is
  there and is not a valid image ("Windows found vulkan-1.dll ... not a valid program file for this computer (Windows error 193) ... Reinstall the
  graphics driver ... If a file named vulkan-1.dll lies beside the game, take it away."); and any other code, shown as it is ("Windows could not load
  vulkan-1.dll ... (Windows error 1114)"). The name `vulkan-1.dll` has one definition, the constant the program asks for (`kVulkanLoaderName` in
  `sim/StartChecks.hpp`); the Windows cmake file reads it from there for `/DELAYLOAD`. A script's run (any engine flag that
  makes it unattended) gets the log and the exit code and no box. The check is asked on every Windows build, delay-loaded or not (where the loader
  is linked at process start the answer is yes, because the program is running). **No run showed the box:** `vulkan-1.dll` cannot be taken from
  this machine, so the box is covered by the pure question and its words (`test_mp_platform`) and by reading the code.
* **File properties** (`game/windows/MagicPortals.rc.in`): `FileDescription` and `ProductName` "Magic Portals Enhanced", `FileVersion` and
  `ProductVersion` from the one version, `InternalName` and `OriginalFilename` `MagicPortals(.exe)`, `CompanyName` "Ivan Cvetanovic",
  `LegalCopyright` "Enhanced edition by Ivan Cvetanovic. Based on Magic Portals by Asantee Games." The owner confirms
  them. The application manifest (UTF-8 code page, step 189) is not in the resource: it is a source of the target and the linker
  merges it.
* **The icon**, when `MAGICPORTALS_ICON` names a file: resource 1, the file's six sizes. Without it the program has none.

**What the program still loads at process start** (read from the import table of the default `MagicPortals.exe`; reported, not changed): `ole32.dll`, `MFPlat.DLL` and `MFReadWrite.dll` (the engine's Media Foundation, which decodes the original's MP3 sounds on Windows),
`KERNEL32`, `USER32`, `GDI32`, `SHELL32`, `IMM32`, `MSVCP140.dll`, `VCRUNTIME140.dll`, `VCRUNTIME140_1.dll` (the dynamic C++ runtime) and the
`api-ms-win-crt-*` set (the Universal CRT). A PC without the Visual C++ runtime, or a Windows N edition without the Media Feature Pack, would meet
Windows' own "the code execution cannot proceed because ... was not found" dialog before `main()` runs, and no message of this program can help
it. **This was not seen on such a machine**; it is a reading of the import table. Delay-loading Media Foundation is the engine's change, and the
choice of a static runtime is the whole build's (the engine would be built the same way); neither is made here. A release package therefore
either carries the Visual C++ redistributable's DLLs beside the program or tells the player to install it: the owner's decision. Planning step 208 gave the packager the mechanism for both: `release.py assemble --platform windows --vc-redist <folder>` copies the DLLs the program imports, and the archive's `PLAYING.txt` says, with or without them, what a PC without the runtime shows (section 8). **The default is not to carry them**; the choice stays the owner's.

**`MAGICPORTALS_DEV_TOOLS=OFF`** takes out the developer's flags: `--route`, `--route-trace`, `--tap`, `--hold`, `--drag`, `--press`,
`--place`, `--burn-dragon`, `--visit-levels`, `--visit-passes`, `--light-masks-off`, `--shadow-maps`, `--sprites-only`, `--asset-watching`,
`--sprite-report` and `--perf-log`. Naming one ends the run with exit code 1 and "`--route` is a developer's flag, and this build was made
without the developer tools ..."; `--help` does not list them, and names none of them in the text of its other entries (the `--controls` entry
says what a developer's run is, and without the tools that is `--fixed-step` alone); an unknown flag is the engine's parser's to refuse, as before. What a player's
build keeps: `--level`, `--root`, `--no-baked-paths`, `--levels`, `--art`, `--data`, `--original`, `--chapters`, `--achievements`, `--port-data`,
`--padart`, `--saves`, `--controls`, `--profile`, `--help` and every flag of the engine (`--window`, `--fixed-step`, `--frames`, `--screenshot`
...). (`--saves` is described in `--help` as a developer's flag, and stays: a capture and a package proof need it.) The suites always build with
the tools: `test_mp_routes` needs `Route.cpp`. The program is smaller (Linux, g++: 11,860,880 bytes against 11,986,016).

**A Linux program that runs on more than the machine that built it (planning step 209, PORT10-3).** Two options of `tools/build_linux.sh`, both linker flags and nothing in the source: `--portable` links libstdc++ and
libgcc into the program (`-static-libstdc++ -static-libgcc`) and `--strip` strips the symbol table at link time (`-Wl,--strip-all`), so that the program, its archive and its hash all see the same bytes. Neither is passed
unless given (a build with neither is the build it was), and a sanitizer build refuses both. **Measured in the Linux subsystem** (Ubuntu 24.04, g++ 13.3, the same sources), the release program of step 208 against the
release recipe with the two options: the version names the program asks of the system went from `GLIBC_2.38`, `GLIBCXX_3.4.32`, `CXXABI_1.3.9` and `GCC_4.3.0` to `GLIBC_2.38` alone; the shared libraries from six
(`libvulkan`, `libasound`, `libstdc++`, `libm`, `libgcc_s`, `libc`) to four (`libvulkan`, `libasound`, `libm`, `libc`, besides the dynamic loader both name); the size from 12,019,040 bytes with a symbol table to 12,135,904 without one (stripping
alone takes the first to 10,627,824; the C++ runtime is 1.5 MB). **The glibc cannot be linked in** (the game opens the Vulkan loader, ALSA and X11 at run time, which need the system's C library), so the program still names
the highest glibc of the machine that built it, and that is the floor: seven symbols carry `GLIBC_2.38` here (`fmod` and `fmodf`, which glibc 2.38 versioned anew, and five `__isoc23_*` functions that its headers redirect to),
and the same sources built on a system with an older glibc name an older one (READ from glibc's version maps; the measurement above was NOT RUN on a 22.04 machine, which this PC does not have). **So a release is built on the oldest system it should
run on**: `ci.yml` and `release.yml` build on Ubuntu 22.04 (glibc 2.35) and pass `--max-glibc 2.35`. `release.yml`'s `linux` job did so on GitHub on 2026-11-03 (g++ 12.3) and `check-program` passed the program (RUN; section 8); `ci.yml`'s own job has not been dispatched. `release.py check-program --max-glibc X.Y` (and `assemble --max-glibc`) refuses a program that names a newer glibc, in
words ("the program asks for glibc 2.38 and the floor is 2.35: it was built on a system newer than the oldest one it should run on"), and `--allow-dev` does not waive it; `PLAYING.txt` states the glibc the program names,
that it is stripped or not, and that the C++ runtime is inside. Whether to strip a release is the owner's (the options are independent). The archive of the portable program, made twice from two folders, is the same
bytes (sha256 `c73647a1...`), and the fixture level starts from it unpacked with no flag but `--level` (and the fixture's `--root`) and draws.

**The phone profile.** `game/data/engine.json` has a `mobile` block: the answers a phone's run gives where they are not the desktop's. A build
made with `MAGICPORTALS_MOBILE=ON` starts under it; `--profile mobile` asks the same of a desktop run (for a test), `--profile desktop` the
other way. The block is read, and checked, by every run, so a typo in it fails where the file is read. Its keys, each of which replaces the
desktop's answer when present:

| Key | Value | Why |
|---|---|---|
| `minimum_vulkan_minor` | 1 | the lowest Vulkan 1.x a GPU may report. The engine wants 1.2; the stock drivers of many phones on Android 11 and 12 report 1.1, so with the default the start ends before the first frame (the sibling port's step E39). A desktop run leaves it out and asks for 1.2. The log line says what the run was given: `... minimum Vulkan 1.2` or, under the profile, `1.1` |
| `shadow_map_resolution` | 16 | as the desktop's own key, repeated so that changing the desktop's does not move a phone |
| `asset_watching` | false | the engine's per-frame stat of every watched file, for both kinds of run: a phone has nobody to hot-reload for |
| `sprites_only_scene_pipelines` | true | the four scene pipelines built with the lit surface folded away (planning step 205). Every picture of this port was byte for byte the desktop's with it on (the 19 baselines and the 128-level f420 sweep, plain and classic, under `--profile mobile`); the gain is the sibling port's, on an Adreno 640 (a 2D menu in 75 ms instead of 117), and the cost is the developer's lit boxes, which draw flat |
| `sprite_report` | false | the layer's per-frame walk that logs a sprite crossing the view's edge (planning step 205): changes no pixel and no tick, and a phone writes every log line to flash |
| `dynamic_resolution` | `enabled` false | seven keys, the flag and the six numbers of the sibling's E41 and E47 (start budget, floor scale, target rate, relaxed rise, floor in lines, emergency rate), written and **off** (`enabled` false): turning dynamic resolution on is the owner's decision (below) |

The scene's sample cap, dynamic resolution and `edge_cover` are **the owner's decision** as one bundle (the block's `_owner_decisions` note says so). Since step 205 they have their keys and flags
and nothing is decided: `scene_samples` is absent from the block (the engine's own 4), `dynamic_resolution.enabled` is `false`, `--scene-samples <1|2|4>` and `--dynamic-res <on|off>` try either for one run,
and `docs/android.md` section 10.4 says what each cap costs in pixels on this PC. A key the reader does not know is refused by name when the file is read, and `Resolve(..., mobile=false)` gives the
answers it gave before the block had these keys (pinned over a grid in `tests/test_mp_tuning.cpp`). As shipped, `--profile mobile` on a desktop is byte-identical to the default for the 19 CTL baselines and for the
128-level f420 sweep, plain and classic. `shadow_map_resolution` is the one key of the block that would change pixels if it were edited: as shipped it equals the desktop's own 16, so it changes nothing, but the
developer's lit boxes sample the maps, and `engine.json`'s own `_about` records 165,280 of 921,600 pixels differing between 16 and the engine's sizes. The keys that could change a pixel and are not set are
`scene_samples` (edge pixels) and dynamic resolution (only outside a fixed-step run); `minimum_vulkan_minor` only through which GPU a machine with more than one picks.

**The performance-diagnostic build.** `-DMAGICPORTALS_PERF_DIAG=ON` (default OFF; independent of `MAGICPORTALS_DEV_TOOLS`) turns on the engine's frame readout over the picture and its statistics line in the
game's own log every 5 seconds, **for a player's (a tester's) run only** (a capture, a route or any other run of such a build gets neither unless a flag asks: fix round R of step 205); `tools/build_android.sh --perf-diag` builds it for a phone (`docs/android.md` section 10.3). It is a build constant like `MAGICPORTALS_MOBILE`
(`game/BuildProfile.hpp` stops a file that lacks it), and the string `PERFORMANCE-DIAGNOSTIC BUILD` is in a diagnostic program and in no other.

**The icon tool.** `python tools/art/make_app_icon.py [--out DIR]` draws the icon (two concentric rings, blue and orange, on a dark disc) with
arithmetic alone: nothing is read, no file of the original is opened, and the output is the same bytes on one machine twice. Into `out/dist/icons`
by default: `icon_<1024 512 256 192 128 64 48 32 16>.png`, `magic_portals.ico` (16 to 256, PNG-compressed entries), `android/mipmap-*/ic_launcher.png`
(48 72 96 144 192), `android/play_store_512.png` and `SHA256SUMS.txt`. It refuses to write inside the tracked tree. The colours are its own
choice (the original's portals are blue and red; the orange is for legibility on a dark disc): the owner may want another.
**The original's icon** (planning step 212). The owner asked for the original game's launcher icon on the phone. `python tools/package/original_icon.py --reference <reference folder> --out <folder>` reads the original APK's own icons from `<reference>/apk_work/decoded/res/drawable-{mdpi,hdpi,xhdpi}-v4/icon.png` (48, 72 and 96 pixels) and writes `android/mipmap-{mdpi,hdpi,xhdpi,xxhdpi,xxxhdpi}/ic_launcher.png`: the first three are the original's files byte for byte, the 144 and 192 are made from the 96 by bicubic resampling on premultiplied alpha (the original has nothing larger). It writes only under `out/` or outside the repository (the content packager's rule) because the icon is the original's file; `tools/build_android.sh --icons <folder>` hands the set to the packager. The Windows program keeps the drawn icon, which CI builds into the program and which cannot be the original's there (no file of the original is on a runner).

## 7. The Apple layouts, and what adding the data to a macOS bundle costs

Planning step 206 (PORT-8). Written, run on GitHub's macOS runner (twice on 2026-10-25, and in both runs of `release.yml` on 2026-11-03; the macOS shell started and drew the fixture level; nothing iOS has run) and never on a person's Apple machine: [`apple.md`](apple.md) says what was checked and by which tool, and lists what nobody knows.

The game's data (the shape `package.py content` writes, section 2's packaged shape; the packager's own folder is `content`, and the folder is named `GameData` inside an APK, as `android_package.py` has it, and inside a bundle) goes **inside** the bundle, and the entries (`game/macos/MacMain.mm`, `game/ios/IOSMain.mm`) look for it
with the pure rule in `game/sim/AppleLayout.hpp` (pinned by `test_mp_applelayout`) and hand what they find to the game through `--root`:

| Platform | Looked at, in this order | Notes |
|---|---|---|
| macOS | `<MagicPortals.app>/Contents/Resources/GameData`, then `~/Library/Application Support/Magic Portals/GameData` | the second is a fallback a player may fill by hand; it is only looked at, never made. A folder beside the app is not looked at (App Translocation, NOT KNOWN) |
| iOS | `<MagicPortals.app>/GameData` | iOS has no "beside" |

A `--root` already on the command line is used as it is and nothing is looked up. The engine's shaders are in `Resources/engine/assets/shaders` (iOS: `engine/assets/shaders` at the bundle's root) and are copied at every launch into the app's caches
folder, which becomes the working directory, because a sealed bundle is read-only and the engine writes where it runs (`apple.md` section 2).

**A data-bearing macOS bundle needs a re-seal.** `tools/apple/make_app.sh` signs the app ad hoc as its last step, and the signature covers every file in the bundle: a `GameData` folder put into `Contents/Resources` afterwards breaks it
(a Mac then says the app "is damaged and can't be opened"). The seal has to be made again after the data is in, by a Mac or by a signer run on another machine. **Which route makes the seal is the owner's to choose (D1 and D2 of the platform
plan); nothing here chooses it.** The three routes are in `apple.md` section 7; none has been run. On iOS the sideloading tool re-signs the whole bundle, so adding `GameData` to the shell's `.ipa` and zipping it again is expected to be harmless (inferred, never run).
`release.py assemble --platform macos|ios` (section 8, planning step 208) writes both layouts: the data inside the bundle, the macOS seal said to be invalid, nothing signed. It was run on synthetic bundles only in step 208: the real shell zips were not on the PC, and nothing was run on a Mac (on 2026-11-03 `release.yml`'s `collect` job ran it over the two real CI shells, without a content package: section 8).

## 8. Release archives

Planning step 208 (PORT-10). `tools/package/release.py` turns **a built program and the owner's content package** into the file a player downloads, one archive per platform with the whole game in it. CI holds no game data, so an archive with the game is **assembled on the owner's PC**, from a program built there or by CI and from his content package (section 3). Nothing the tool writes goes into the repository (it refuses an output folder in the tracked tree), and it decides nothing that is the owner's: the licence text and the notices are the repository's own `LICENSE` and `NOTICE.md` (the tool takes them from the repository root or from `--notices`, copies them unchanged, and **refuses to write a release archive without them**); the version is read from the one definition (`tools/package/version.py`; `0.1.0` began as a placeholder and is the number of the first release's files); the application id, the signing identities, the Android release key, whether the Apple packages carry the game's data and whether the Windows archive carries the C++ runtime are the owner's.

| Platform | File | Holds | The program is built by |
|---|---|---|---|
| Windows | `MagicPortals-<version>-windows-x64.zip` | `MagicPortals.exe`, `assets/shaders/*.spv` (17), `content/`, `PLAYING.txt`, `LICENSE`, `NOTICE.md`, and with `--vc-redist` the runtime DLLs the program imports | the owner's PC (section 6's exact command, `out/build-release`), or `release.yml` (MSVC on a Windows runner, the same options; written in step 209, **RUN on 2026-11-03**: its first dispatch failed at the configure, its second built the program on `windows-2025-vs2026` and `check-program` passed it, `ci.md` section 9) |
| Linux | `MagicPortals-<version>-linux-x86_64.tar.gz` | the same, the program with its executable bit | the owner's WSL or a CI Linux runner (`release.yml`, Ubuntu 22.04, g++ 12.3: RUN on 2026-11-03, `check-program --max-glibc 2.35` passed): `tools/build_linux.sh --no-baked-paths --dev-tools OFF --portable --strip --build-dir <folder>` (section 6: build it on the oldest system it should run on) |
| macOS | `MagicPortals-<version>-macos-universal.zip` | the CI shell's `MagicPortals/MagicPortals.app` with `Contents/Resources/GameData` added, `READ ME FIRST.txt`, `LICENSE`, `NOTICE.md` | GitHub's macOS runner (`.github/workflows/apple.yml`, the owner dispatches it) |
| iOS | `MagicPortals-<version>-ios-arm64.ipa` | `Payload/MagicPortals.app` with `GameData` at the bundle's root, `READ ME FIRST.txt`, `LICENSE`, `NOTICE.md` | the same workflow (`-simulator` for the simulator bundle) |
| Android | `MagicPortals-<version>-android.apk` (+ `LICENSE`, `NOTICE.md` beside it) | the signed release APK, copied unchanged | the owner's PC: `tools/build_android.sh --release --keystore ... --data <content>` |

Every archive set gets a `SHA256SUMS.txt` (`sha256sum -c` form) over the archives and the notice files beside an APK. A name that carries `-dev` (a smoke archive made with `--allow-dev`), `-shell` (an Apple shell passed through without the game) or `-nodata` (an archive made with `--no-data`: a program without the game, which `ci.yml` makes as a test and which is no release) is never in it, and an assembly of such an archive writes no sums (planning step 209, PORT10-6 b). **The names above are the packager's.** The GitHub Release of the first release publishes its three files under fixed names instead, so that the README's `releases/latest/download/<name>` links work: `MagicPortals-Windows.zip`, `MagicPortals-Linux.tar.gz` and `MagicPortals-Android.apk` (a copy of the release APK under a new name; its contents are unchanged), beside `LICENSE`, `NOTICE.md` and a `SHA256SUMS.txt` made with `sha256sum`, because `release.py sums` only knows the versioned names.

**The order of a release**, from the repository, with generic paths:

```
python tools/package/package.py content --repo <repository> --reference <reference folder> --out <content package>       (section 3; once, from his data)
python tools/package/release.py check-version                                                                            (the one version in every place it is written)
python tools/ci/check_public.py                                                                                          (the tracked files are what a stranger may read; docs/ci.md section 8)
python tools/package/release.py check-program <program> --platform windows|linux|macos|ios                                (reads the program, never starts it)
python tools/package/release.py assemble --platform windows --program <MagicPortals.exe> --content <content package> --out <release folder> [--vc-redist <redistributable folder>] [--notices <folder>]
python tools/package/release.py assemble --platform linux   --program <MagicPortals> --content <content package> --out <release folder>
python tools/package/release.py assemble --platform macos   --shell <MagicPortals-macOS-shell.zip> --content <content package> --out <release folder>
python tools/package/release.py assemble --platform ios     --shell <MagicPortals-iOS-shell.ipa> --content <content package> --out <release folder>
python tools/package/release.py android --apk <out/android/MagicPortals-<version>-release.apk> --sdk <Android SDK> --out <release folder>
python tools/package/release.py sums <release folder> --check
```

`--help` of every subcommand says what it refuses.

**What `check-program` reads, and refuses.** It reads a built program as bytes and **never starts it**. A refusal lists every failed fact, not the first. The facts are exactly those a program carries as bytes: (1) the format and the machine type match the platform name (a PE x64 program for Windows, ELF64 x86-64 for Linux, Mach-O arm64, x86-64 or fat for macOS, arm64 for iOS, an ELF shared object for an Android library: ELF64 for `arm64-v8a` and `x86_64`, and ELF32 with the machine type `EM_ARM` (40) for `armeabi-v7a`, which only an Android library may have); (2) **no baked path and no builder**: none of eight fixed strings (the checkout's two folder names as paths, each between two slashes or two backslashes, `/mnt/`, `/home/`, `/Users/` and `\Users\`) occurs, nor this checkout's own path in each of its spellings, nor **the name of the account that runs the check**, in ASCII or UTF-16 and in any case (`--also-forbid TEXT` adds one more). The account's name is read from the system when the check runs (the user name and the name of the home folder), so that no file of the repository spells it: it is found **anywhere**, as a substring, when it has five letters or more and is not an ordinary folder's name (a path, a build label with a digit glued to it and the local part of an address are all found: the literal this lookup replaced was a substring too), as a whole word (no letter or digit touching it) when it has four letters, and only as a path component (`/name/`, `\name\`) when it is shorter or is an ordinary folder's name (`runner`, `root`, ...), and the refusal says that it found the name without printing it (the first line of the output, which echoes the program's path as it was given, and every error line show `<account>` where the path holds the name; `assemble` and `sums` print the paths they were given and the paths they wrote as they are). **A program built by one account and checked by another is checked for the checking account's name and the `--also-forbid` list**, so name the builder's account there when the two differ. **The folder name is also the project's public name, so only the name BETWEEN TWO SEPARATORS is a path of a build tree** (a Windows runner builds in `D:\a\<repository>\<repository>`, which carries neither `/home/` nor `/Users/`): a line that ends at the name (a URL such as `https://github.com/<owner>/Magic-Portals-Enhanced`, a credits line that stops there) or that has the name among words with no separator on both sides is not refused, and a URL with a path after the name holds the name between two separators and is refused (planning steps 209 and 210, PORT10-2 g); (3) **the developer's tools are OFF**: none of four lines of `--help` that `game/main.cpp` prints only inside `#if MAGICPORTALS_DEV_TOOLS` is in the program (a test ties each to that region); the flag names and the sentence "is a developer's flag, and this build was made without the developer tools" are **not** used, because a program with the tools OFF holds them to refuse them and a linker may or may not keep the sentence in one with them ON (the Windows developer program does not, the Linux one does: measured); (4) **no performance readout**: the string `PERFORMANCE-DIAGNOSTIC BUILD`; (5) **it is the game**, by two strings every build carries, so that a file that is nothing does not pass the absences; (6) **Windows only**: `vulkan-1.dll` is delay-loaded, as a release's is (the developer program imports it at start, and a PC with no driver would then get Windows' own dialog), and the version information's `ProductVersion` and `FileVersion` equal the one version in its four-field form. The version is read **by scanning for the UTF-16 key and its value** (and the fixed block's binary quad), not by walking the resource directory. `--allow-dev` turns the refusals about the kind of build into printed warnings and writes `DEVELOPER BUILD.txt` into the archive, whose name carries `-dev`; a wrong format or machine is never allowed through.

**Deterministic.** The same inputs give the same archive bytes: sorted entries, one fixed date, modes by rule (the program 0755, every other file 0644, folders 0755), no owner names, no gzip name or time. The compressed bytes are those of the machine's Python zlib, so two runs on one machine are equal and another Python may differ. The tool re-reads its own archive, compares every file with what was meant to go in, and runs `package.py verify` on the `content/` folder as unpacked from it.

**What it refuses**, with a message that names the cause and nothing left behind (no archive, no `.part`, no staging folder): a program that `check-program` refuses; content that fails `package.py verify` or holds a file outside its manifest; a missing or empty `LICENSE` or `NOTICE.md`; an output folder inside the tracked tree or inside a folder it reads; an archive that already exists; runtime DLLs (`--vc-redist`) that are missing, are not DLLs or are of another machine type; an APK that is not a signed release of the one version; a `LICENSE` or `NOTICE.md` that already sits beside the APK and differs from the one given (compared before the APK is placed); a macOS or iOS shell that holds an entry that is not a plain file (a symbolic link: it would be written as a file holding the link's target).

**The Windows runtime (`--vc-redist`).** `--vc-redist <folder>` copies the Visual C++ runtime DLLs that the program's import table names (`MSVCP140.dll`, `VCRUNTIME140.dll`, `VCRUNTIME140_1.dll` for the program of section 6; the whole family counts, so a program that imported `msvcp140_atomic_wait.dll`, `msvcp140_codecvt_ids.dll` or `vcruntime140_threads.dll` would have them required and copied too) from Microsoft's redistributable folder, beside the program, verifying each is there and is a PE DLL of the program's machine type. **The default is NOT to carry them**; whether to is the owner's decision (section 6). In either case `PLAYING.txt` says what a PC without the runtime shows: Windows' own message before the game starts, which no message of the game can replace. The same file says what a PC without a Vulkan driver, and a Windows N edition, would see; those are readings of the code and the import table, **not seen on such a PC**.

**The Apple layouts (section 7 made true).** `assemble --platform macos|ios --shell <zip|ipa|folder> --content <package>` writes the game's data **inside the bundle**, in the folders `game/sim/AppleLayout.hpp` looks in: `Contents/Resources/GameData` (macOS) and `GameData` at the bundle's root (iOS), from the whole package including its `manifest.json`, so `package.py verify` can be run on the folder after unpacking. The archive is rewritten deterministically (an `.ipa` still starts with `Payload/`). **For macOS the seal is then invalid**: `make_app.sh` signed the app ad hoc as its last step and the signature covers every file in the bundle. The tool says so in its output and in `READ ME FIRST.txt` (which replaces the shell's, because the shell's says "without the game"), names the three routes of `apple.md` section 7, says it **cannot sign** (it runs on a PC) and that **nothing has ever been run on a Mac**. Without `--content` the shell passes through byte for byte under its new name (`-shell`): it has no data and no notices, and is a shell and not a release.

**The Android release.** `release.py android` runs `android_package.py inspect` on the APK **as a release** and refuses, listing every cause: a debug or debuggable APK, one signed with the debug key or not signed, the verification and performance-diagnostic variants (by the file name **and** by what their libraries hold: each `libMagicPortals.so` is read as a program), a `versionName` that is not the one version, a `versionCode` other than the rule of `version.py` (`major*10000 + minor*100 + patch`; `--version-code` says another if the owner chose one), a missing crash-report fact (`the crash report will call the game: ...`), no game data. `inspect` reads each library as an ELF object of its ABI (32-bit for `armeabi-v7a`, 64-bit for the others) and holds the 16 KB LOAD alignment to the two 64-bit ABIs only, because NDK r28 links `armeabi-v7a` with 4 KB pages. It **never signs and never reads a key**. The notices go **beside** the APK, not inside it: the APK is signed, and adding a file to it would be a rebuild. A different `LICENSE` or `NOTICE.md` already in the output folder refuses the run before the APK is placed, so the folder is left as it was. `build_android.sh` no longer defaults the code to 1: the packager derives it from the version unless told.

**What was run in planning step 208, and what was not.** (The table is the record of 2026-10-27 and is not edited; what changed afterwards, and so which of its "not run" cells are no longer true, is the paragraph after it.)

| Platform | Run | Not run |
|---|---|---|
| Windows | `check-program` on the developer's `build/game/MagicPortals.exe` as an input file: **refused with 7 facts**, no process started; on step 202's release program `out/build-release/game/MagicPortals.exe` (md5 `713a952e`): **passes** (the forbidden strings absent, tools OFF, GUI, version 0.1.0.0, `vulkan-1.dll` delay-loaded). An archive of the release program with an invented content package and invented notices: built twice, the same sha256, the program entry equal to the input file, the content inside verified; with the runtime DLLs of this PC's `System32` as the redistributable (real DLLs, read and zipped); `--no-data`; a `-dev` smoke archive of the developer exe. | **Nothing was started**: no Windows program was built, linked or launched, and no archive was unpacked and run. How an archive behaves on a PC with no graphics driver or on a Windows N edition. A new MSVC release build (nothing here relinked it). |
| Linux | A release program (`tools/build_linux.sh --no-baked-paths --dev-tools OFF`, g++ 13.3, WSL) **passes**; the developer program is **refused**. An archive of it with a synthetic content package (the CI fixture's level and tables, the port's `game/data` as port-data): built twice from two folders at different times, **the same sha256**; unpacked on ext4, `package.py verify` ok; the fixture level started from the unpacked archive **with no flag** (the `content/` beside the program found by tier 2) under Xvfb on lavapipe: exit 0, "finished: clean", a 1280x720 picture of 4 distinct pixel values (`check_png.py --min-size 640x360`); with `content/` moved away: exit 1 and the words "the game's data was not found (looked in ...)"; with the Vulkan loader told to find no driver (`VK_DRIVER_FILES=/nonexistent`): exit 1 and "fatal: GLFW could not report the required Vulkan instance extensions. This usually means no Vulkan loader or ICD is installed." | A real Vulkan GPU, a PC with no driver at all (the loader missing: expected to be the system's own "error while loading shared libraries", not run), a Wayland session, another distribution than Ubuntu 24.04 (the portable, stripped program of step 209, section 6, was run there only), an archive made on a CI runner. |
| macOS, iOS | Synthetic bundles (a real property list, a header-only executable) in a temporary folder: the data in the exact folders, deterministic, the seal text. | **The real shell zips were not on this PC** (searched: all of `out/`, `Downloads`, `Documents`, `Desktop`, `%TEMP%`; they were CI artifacts and are gone), so the tool was not run on them and `check-program` has never read a Magic Portals Mach-O. The only real Mach-O files it read were two executables of the sibling port's release archives, in memory: they hold `/Users/runner/work/<its checkout>` (6 times in the universal program, 3 in the iOS one), so `/Users/` fires on a real CI-built Apple executable; whether this game's shell carries one is **not known**. Nothing on a Mac, an iPhone or an iPad. The seal was not made again. |
| Android | `release.py android` on the two step-203 debug APKs with the real `aapt2`, `zipalign` and `apksigner`: **both refused** with the causes listed (debuggable, debug key, a JAR signature, the old configuration-change mask, a versionCode of 1, the verification libraries' tools). The release path through a stubbed inspect (tests). | A real signed release APK (there is no key in the repository, and none was made); a device. |

**What was run for the first release (2026-11-03, planning step 211).** The programs were built on GitHub by `release.yml` (`ci.md` section 9); the archives were assembled on the owner's PC, with `release.py assemble` from those programs and his content package. This changes the table above as follows.

* **Windows.** `MagicPortals-0.1.0-windows-x64.zip`, 24,800,932 bytes, sha256 `09d38e46604d57185bb637ab2a3715347c0d3c3880b9241166e241c245f5b2b6`, from the CI-built `MagicPortals.exe` (sha256 `306651b47b1c387c7950a8cdc1cb31d8ceabbc5e65b8f67be506741c95997556`; `check-program` passed it in CI). A different assembly of the same inputs gave the same bytes. **NOT RUN:** that program has not been started on any PC, so the archive has not been unpacked and started anywhere.
* **Linux.** `MagicPortals-0.1.0-linux-x86_64.tar.gz`, 26,285,851 bytes, sha256 `ac1440c6468e0ef31c36f4aa1d4e4fb8fbbfe8637b50e68c03a144c1b806bda0`, from the CI-built `MagicPortals` (sha256 `0d88887b16c933619c177a20e8305ed01e660df4a3b6afdf2223a887a93c2a8f`; built on Ubuntu 22.04, glibc ceiling 2.35 read with `objdump -T`); again the same bytes from a different assembly. The row's "an archive made on a CI runner" stays true in this sense: no archive is made on a runner; this one was made on the owner's PC from a program that a runner built. **RUN:** the archive was unpacked and run in the Linux subsystem (Ubuntu 24.04, glibc 2.39) under Xvfb on Mesa's software Vulkan driver, with no real GPU: it finds its `content` folder beside itself with no flag, a level starts and draws the original's art, exit 0 and "finished: clean"; all 128 levels start; with `content/` removed it exits 1 with a message; with the Vulkan driver hidden it exits 1 with the documented message. **NOT RUN:** Ubuntu 22.04 itself was not used to run it.
* **macOS, iOS.** `release.yml`'s `collect` job ran `release.py assemble` over the two real CI shells (without a content package) and it passed, so `check-program`'s `/Users/` rule did NOT fire on this game's real CI-built Mach-O files: the row's "whether this game's shell carries one is not known" is answered. **NOT RUN:** the archives with the game in them were not made, and nothing was run on a Mac, an iPhone or an iPad.
* **Android.** `MagicPortals-0.1.0-android.apk`, 33,775,997 bytes, sha256 `c69754e0b80bc4be5077d1c3af2919108b4c80729fec0bb56428bc72fdb08c51`, carries THREE libraries (`arm64-v8a`, `armeabi-v7a`, `x86_64`). It was built on the owner's PC with `tools/build_android.sh --release` and his existing release key (the key of his other game: certificate CN=Ivan Cvetanovic, O=Penumbra Enhanced, C=AT, SHA-256 fingerprint `AC:1D:43:BD:18:C5:85:27:E2:D3:10:72:17:3A:39:B5:1A:2A:95:19:6C:56:0E:8D:FE:EA:6C:DF:13:BD:66:DA`, valid to 2056), and `release.py android` passed it. The row's "there is no key in the repository, and none was made" is half true now: the key is still not in the repository, but a signed release APK was made. **NOT RUN:** that APK has never been installed or started on any device or emulator (the emulator runs of `android.md` section 0 were earlier debug APKs, at an older engine pin).
* **The packager** learned the 32-bit ARM ELF (ELF32, `EM_ARM` = 40) and holds the 16 KB LOAD alignment to the two 64-bit ABIs only, because NDK r28 links `armeabi-v7a` with 4 KB pages.

**How the tools are tested** (all standard library, except that the workflow checker needs PyYAML; counts of planning step 209): `python -m unittest discover -s tools/package -p "test_*.py"` (344 tests on Windows with Python 3.14, 4 skipped; 344 in the Linux subsystem with 3.12, 2 skipped; every skip prints `SKIPPED <test>: <reason>` on standard error), `... -s tools/ci ...` (193 on both, 1 skipped on Windows, none in the Linux subsystem), `... -s tools/apple ...` (139 on both; 1 skipped on Windows, and in the Linux subsystem 1 skipped where `actionlint` is not on `PATH`, none where it is), and in a Linux tree `ctest -R test_mp_build_` (the two build pins; `ctest -N` lists 80 tests there, 77 before step 209, 75 before step 208). At planning step 211 (2026-11-03): `tools/package` 390 tests (4 skipped on Windows), `tools/ci` 317 (1 skipped). The named mutations that were run on isolated copies are in the Step.

**Decisions that are the owner's, and left:** the version (0.1.0 began as a placeholder and is the number of the first release's files) and the versionCode numbering; the application id; whether the Windows archive carries the C++ runtime (default: not); the licence text (`LICENSE` is the one proposed in planning step 210, the same as the engine's, until the owner confirms it); the Android release key (the first release APK was signed with his existing key, the one of his other game); signing and notarising the Apple packages and which of section 7's routes makes the data-bearing macOS zip; whether the Apple packages carry the game's data at all; the archives' names beyond the pattern above.
