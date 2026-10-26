# The content packager

Builds the folder Magic Portals Enhanced reads its data from at run time, from the data that is on **your** machine. It is how a
release package gets its art, its sound and its levels; the repository holds none of them (`CLAUDE.md`, the hard rule), CI never holds
them, and nothing this tool writes goes into the tracked tree: it refuses to. Python 3, standard library only, one tool for
Windows, Linux and macOS (run on Python 3.14 on Windows and 3.12 on Linux; macOS has not been run). The folder it writes and how the game finds it are described in [`docs/packaging.md`](../../docs/packaging.md).

```
python tools/package/package.py content --repo <this repository> --reference <the reference folder> [--out <folder>] [--with-pad-art]
python tools/package/package.py verify  <content folder>
python tools/package/package.py shell   --content <content folder> --exe <the program> --out <folder> [--repo <this repository>]
python tools/package/package.py same    <folder> <folder>

python tools/package/release.py check-program <program> --platform windows|linux|macos|ios|android-lib [--allow-dev]
python tools/package/release.py assemble --platform windows|linux --program <program> (--content <content folder> | --no-data) --out <folder> [--vc-redist <folder>]
python tools/package/release.py assemble --platform macos|ios --shell <zip|ipa|folder> [--content <content folder>] --out <folder>
python tools/package/release.py android  --apk <release apk> --sdk <sdk> --out <folder>
python tools/package/release.py sums <release folder> [--check]       python tools/package/release.py check-version
python tools/package/version.py [code|windows]                        python tools/package/check_version.py
```

**Before you run it**: the original's APK unpacked into `reference/extracted/`, and the converter run so that `out/levels`, `out/data` and
`out/assets` exist (README, *Local setup after cloning*, steps 1-8; `docs/packaging.md` §3 lists them); `reference/magic-rampage/` only for
`--with-pad-art`. None of it is in a fresh clone.

* `--repo` is this repository after the converter has run (it reads `out/levels`, `out/data`, `out/assets`, and the committed `data/` and
  `game/data/`).
* `--reference` is the folder with the original's extracted assets (`<reference>/extracted/assets`) and, optionally,
  `<reference>/magic-rampage`.
* `--out` defaults to `<repo>/out/dist/content`. Give a full path; any folder outside the repository, or under its gitignored `out/`, is
  allowed.
* `--with-pad-art` also packages Magic Rampage's control pictures. They are optional: without them the game draws the original's corner
  sprites and then its own glyphs. Off by default.

## What goes in, and how that is decided

Nothing is listed in this repository. `closure.py` follows the chain the game follows, from the owner's own tables: the converted levels
name their textures; the committed JSON tables name images, sounds, fonts and entities; an entity names its particle bitmaps; a
picture is looked for in its density tiers (`fullhd/`, `hd/`, from `game/data/tiers.json`) and then as itself. An entity file also names
its own picture, its normal map and its light's halo, and a table that spells a suffix (`_blue.png`) means the entity's own name in front
of it: those are followed too. Every file those names land on is packaged, with the 1x file under a picked tier twin (what the game falls
back to), and nothing else. The folders the converter
or the extraction leave that nothing names (the original's scenes, `.pvr` and `.pkm` pictures, `.par` effects, the framework folder, the
Android bundle, the baked lightmaps) are left out, and a table that were to name one is an error, not a quiet inclusion.

The lightmap pictures the converter writes (`out/assets/lightmaps`) are **not** packaged: the layer bakes a lit sprite's lightmap in
memory (planning step 196) and never opens them. The 128-level sweep run from a package without them is what proves it.

## What it refuses (exit 2, with the cause)

* an output folder inside the repository's tracked tree, on top of a source folder, or inside the reference folder;
* a non-empty output folder that holds no `manifest.json` written by this tool (a package it wrote is replaced whole; anything else is not touched);
* a symbolic link ANYWHERE in a source tree it reads (a file, a sub-folder, the root, a broken link), at its own name, whatever it points to (planning step 208 closed PORT2-2: before, a linked sub-folder gave a package without its files and a broken link a traceback);
* a name that matches a file only when case is ignored (the package would work on Windows and macOS and fail on Linux and Android), also for a tier
  twin: a wrong-case `hd/` file would otherwise quietly fall back to the 1x one;
* anything on the never-package list (`NEVER_*` in `package.py`).

## What it writes

`manifest.json` (every file with its size, sha256, kind and role, plus the references that name nothing in the data) and `SHA256SUMS.txt`
(`sha256sum -c` form) sit at the top of the output. The same input gives the same bytes in the same order with the same times, so two runs
can be compared with `same`. `verify` re-checks a folder against its manifest and then runs the same model over **the package itself**:
every name the game can open must resolve, in exact case, inside it, and nothing in it may be surplus. A finished `content` run does that
before it moves the folder into place.

## The Android packager (`android_package.py`, planning step 203)

Builds the APK from the content folder this tool writes. `tools/build_android.sh` is the front door (it builds the libraries and calls it); the details,
what the APK holds, what was run on an emulator (planning step 204) and what has **not** been run (a phone) are in [`docs/android.md`](../../docs/android.md). It is Python standard library only and takes the Android SDK's
tools (aapt2, zipalign, d8, apksigner, javac) from `--sdk`.

```
python tools/package/android_package.py build   --repo <repo> --sdk <sdk> --build-tools <ver> --lib <abi>=<libMagicPortals.so> ... (--package <content folder> | --no-data) [--out <dir>]
python tools/package/android_package.py inspect <apk> --sdk <sdk>          (badging, zipalign, apksigner, the list against the assets, every library as an ELF file, no JAR signature, the activity's configChanges against the platform's, and since step 207 the name the engine's crash report will call the game, with the key `supersonic.reportTitle` read back from the built manifest as a string)
python tools/package/android_package.py compare <apk> --package <content folder>      (the APK's unpack list against manifest.json: files, sizes, sha256)
```

It reads the content folder **only through its `manifest.json`**: a listed file that is missing, short, altered, a link or outside the folder, a file the manifest does not list, a
name that is not a safe relative path, anything on the never-package list and a folder under `reference/apk` or `reference/ethanon` are all refused (exit 2, with the cause). The shaders
come from `--shaders` (default `engine/assets/shaders`, plain `.spv` files only). It writes `assets/gamedata.list` (a stamp, then `size TAB path` for each file) beside the files: the
list the app's first-run unpack reads (`game/sim/Unpack.hpp`). The APK is deterministic: two runs on the same inputs are the same bytes.

## The release packager (`release.py`, planning step 208)

Turns a built program and this packager's content folder into the archive a player downloads, one per platform, with the whole game in it: `docs/packaging.md` section 8 is the record (what each archive
holds, the order of a release, what was run and what was not). It reads a program as bytes and **never starts it**, refuses a developer's build (a baked path or an account name in it, the developer's tools,
the performance readout, a Windows version that is not the one version, a Vulkan loader imported at start, the wrong machine type), writes the same bytes twice, re-reads its own archive and runs `verify` on the
content inside it, copies `LICENSE` and `NOTICE.md` unchanged and refuses to write without them, and decides nothing that is the owner's (the version, the licence text, the runtime choice, the keys, whether
the Apple packages carry the data). `version.py` reads the ONE version (`project(MagicPortals VERSION ...)`) and gives the Android `versionCode` (`major*10000 + minor*100 + patch`) and the Windows four-field
form (the definition must have exactly three fields: `1.2` and `1.2.3.4` are refused by name, and `0.0.0` has no code, because Android needs at least 1); `check_version.py` compares it with every place it is written (a table of 8 files that are handed it, 4 sentences of the documents and the Android manifest) and fails naming a place that disagrees or a
new file that spells the number (folders named `build` or `build-<word>`, `venv` or `venv-<word>` are not scanned; `tests/build_pins/` is). Every subcommand's `--help` says what it refuses. Python standard library only, run on Windows (3.14) and in the Linux subsystem (3.12).

## Tests


```
python -m unittest discover -s tools/package -p "test_*.py" -v
```

`test_android_package.py` builds a synthetic package, hand-made ELF files and zips in a temporary folder (no SDK, no binary in the repository) and pins the list, the path rules (the same
table as the C++ suite `test_mp_unpack`), the package loader's refusals, the manifest's policy and its text ties to the C++, the build-tree check, the ELF check, the
byte-for-byte determinism of the assembled zip, and the command line's refusals; since step 204 also the 18 configuration kinds (against the SDK's own platform file when it is there), the signing options, and
the tie of the list's format to `game/sim/Unpack.hpp` (the three constants it reads out of the header, and the packager's list for four invented files, which `tests/test_mp_unpack.cpp` holds as text and parses); since step 205 also the list's limits (the 4 MiB text cap and the rounded-sum rule, read out of the same header) and the performance-diagnostic variant (`inspect` reads the variant out of the libraries and holds it to the `-PERFDIAG` file name; `build --perf-diag` is refused with a release or the verification build). `test_package.py` also ties `Layout::Packaged()` in `game/sim/AssetRoot.cpp` to this tool's `DEST`. Since step 207 also the name the engine's crash report calls the game (`TheReportTitle`, `TheInspectLine`): the engine's rule copied and tied to the engine's own source and README, the refusal of a `supersonic.reportTitle` that aapt2 would store as a number, a boolean, a colour or a reference (the typing table held to the real aapt2 where an SDK is present, skipped where it is not) and of a blank one, and the built manifest's key read back as a string.
`test_android_emulator.py` pins the guard of `tools/android_emulator.py` (planning step 204), the one script that drives an emulator: two slots and their serials only, every adb verb that acts on the shared adb server refused, a caller's `-s` refused; since step 205 also what `adb_argv` builds (`adb -s <serial> ...`, for both slots), the console the emulator starts with (no window, never detached), the output made safe for a console that lacks a character, and the tie of the script's package and activity to the manifest. It needs no emulator and no SDK. Since step 207 also the measurement behind the 6 GiB floor (a start at the measured 4.49 GB is refused, with the footprint and the ways to make room in the message and in the log) and the line with the available memory and the emulator's size that every boot writes to the log; every test of a start stubs the process, the SDK lookup, the process list and the boot wait, so a lowered floor cannot start a real emulator.

`test_release.py` (step 208) builds synthetic PE, ELF and Mach-O files (a header, a section, the import tables, the strings a build carries: nothing that can be started), a synthetic content package from `test_package.py`'s miniature world, invented notices and hand-made bundles, and pins: each forbidden string in ASCII and UTF-16 and in another case, each sentinel, each machine type, each version field, the ties of the sentinels and the marker to `game/main.cpp` (inside the right `#if`), the archives (layout, modes, no owner, no time, the same bytes from two folders at two times), every refusal leaving nothing behind, the Apple layouts and the seal text, the Android release check through a stubbed inspect, and the sums. `test_version.py` pins the rule and, on a copy of the tree, the red of each place that stops handing the number on; `test_build_pins.py` the machinery of the two build pins in `tests/build_pins/` (ctest `test_mp_build_dev_tools_option` and `test_mp_build_profile_header`, run in the Linux tree and exit 77 on Windows). `tools/ci/test_sync_fixture_tables.py` pins the fixture-table sync tool. A test whose check could not run says so on standard error (`SKIPPED <test>: <reason>`), not only in unittest's own count.

The tests build a miniature tree of invented names and bytes (`tile_a.png`, `boom_a.mp3`, a hand-made UTF-16 `.ent`): no file of the original
and no name from it is in this repository. They cover the closure, the refusals, the manifest, the never-package list, determinism and the
`shell` layout. Two of them make a symbolic link and skip themselves where the system will not.
