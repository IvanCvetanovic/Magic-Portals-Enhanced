# Building Magic Portals Enhanced from source, on each platform

Planning step 209 (PORT-11). For a stranger with a clone and no game data. Every command below is one that exists in this repository; section 8 is the table of which ones were run as written, by whom, where,
and which were not. The game's data is not here and is not needed to build, test or run the fixture level.

## 0. What you need, whatever the platform

* A clone **with its engine**: `git clone --recurse-submodules <the repository's address>` or, in a clone, `git submodule update --init` (the engine, Supersonic Engine, is a git submodule at `engine/`
  pinned to a commit; the build scripts run the second command themselves when `engine/` is empty and `.git` is there).
* CMake 3.20 or newer, Ninja, a C++20 compiler, and Python 3 for the tools (`tools/package`, `tools/ci` and `tools/apple` use the standard library only; they were run on Python 3.12 and 3.14, and no older one was tried). The engine asks for MSVC 2022, GCC 11 or newer, or Clang 13 or newer; this project
  has been built with MSVC (Visual Studio Build Tools, the owner's PC), g++ 13.3, clang 18 (with libstdc++ and with libc++), the Android NDK's clang (r28) and Apple's clang (`apple.md`).
* **Build on a case-sensitive file system with no spaces in the path.** A Windows drive mounted into WSL (under `/mnt`) is refused by `tools/build_linux.sh` unless you pass `--allow-drvfs`.
* The engine's compiled shaders (`engine/assets/shaders/*.spv`) are committed. A shader compiler (`glslc`) is **not** needed, and if the build finds one it would rewrite them; the CI jobs and the Apple
  workflow pass `-DGLSL_COMPILER=GLSL_COMPILER-NOTFOUND` so that nothing is rewritten.

## 1. Linux (Ubuntu 22.04 and 24.04 are the two the project names; 24.04 is the one it was built on)

```
sudo apt-get install -y build-essential cmake ninja-build pkg-config libvulkan-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libxkbcommon-dev libasound2-dev
sudo apt-get install -y xvfb mesa-vulkan-drivers libvulkan1   # only to RUN it with no display or no GPU: a virtual display and a software Vulkan driver
tools/build_linux.sh                                          # MagicPortals and test_mp in build/, Release, Ninja; --help lists every option
```

Run the suites: `ctest --test-dir build` (every suite once; the ones that need the owner's data exit 77 and ctest reports them SKIPPED, never passed), or `build/tests/test_mp test_mp_numbers` for one,
`build/tests/test_mp --list` for the names.

**A release program** (no data path, no developer tools, libstdc++ and libgcc linked in, symbols stripped; ask nothing of the system but glibc, the Vulkan loader, X11 and ALSA):

```
tools/build_linux.sh --no-baked-paths --dev-tools OFF --portable --strip --build-dir build-release
python3 tools/package/release.py check-program build-release/game/MagicPortals --platform linux
```

The program asks the system for the glibc of the machine that built it (`ci.md` section 5 has the numbers): **build a release on the oldest system it should run on.**

## 2. Windows (MSVC)

From a shell where MSVC is on `PATH` (an x64 Native Tools prompt, or after `vcvars64.bat`):

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DSUPERSONIC_ENABLE_VALIDATION=OFF -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl -DGLSL_COMPILER=GLSL_COMPILER-NOTFOUND
cmake --build build --target MagicPortals test_mp
ctest --test-dir build
```

`tools\parity\port_build.bat` does the same on the owner's machine (his Build Tools path and Vulkan SDK are written in that one file, and it turns validation on). Name the two targets: the default target also builds `MagicPortalsSpike`, the stale spike that CLAUDE.md says is never built. **A release program**
(`packaging.md` section 6 has the exact command): add `-DMAGICPORTALS_BAKED_PATHS=OFF -DMAGICPORTALS_DEV_TOOLS=OFF -DMAGICPORTALS_WIN_GUI=ON` and, for the icon,
`-DMAGICPORTALS_ICON=<the .ico that python tools/art/make_app_icon.py writes>`, in a build folder of its own, and build the target `MagicPortals`.

## 3. Android

Needs the Android SDK (platform 35, build-tools 35.0.0), the NDK r28.2 (`28.2.13676358`; r28 or newer is what links 16 KB-aligned), a JDK 17 or newer, and Python. The script finds them from `ANDROID_HOME`
and `ANDROID_NDK_HOME` or its own options (`docs/android.md` section 1):

```
tools/build_android.sh --no-data --abi all                     # a shell APK with no game in it, debug-signed: what CI makes
tools/build_android.sh --data <content folder> --abi all       # the APK with a content package in it; docs/packaging.md section 3 makes the package
```

`--release` signs with a key you name (`--keystore`, `--keystore-pass-file`, `--key-alias`): this repository has none. The Linux and macOS paths of this script were written from the same rules and **have never been run** (`ci.yml`'s `android` job would be their first run);
on Windows it ran under Git Bash.

## 4. macOS and iOS

Need a Mac with Xcode, and MoltenVK's static library (`SUPERSONIC_MOLTENVK_LIBRARY`). The exact configure lines are the ones in `.github/workflows/apple.yml`, which ran twice on GitHub's runner
(`apple.md` section 4b); `bash tools/apple/make_app.sh macos|ios <build folder> <out folder>` assembles the shell from a build. `apple.md` section 3 is the long form. Nothing in this section has been run on a
Mac that a person owns.

## 5. Running the fixture level with no game data

The fixture is a level written for the purpose (`tests/fixtures/ci`), with its tables. It needs the port's own tables too. From the repository root, in bash (Git Bash on Windows), with the build folder of section 1 or 2; **every path flag is
absolute** (the game makes `engine/` its working directory, and `--root` refuses a relative path), so the commands use `$PWD`:

```
mkdir -p saves
build/game/MagicPortals --root "$PWD/tests/fixtures/ci" --port-data "$PWD/game/data" --level ci_level --window 1280x720 --fixed-step --frames 300 --saves "$PWD/saves" --screenshot "$PWD/ci_level.png"
python3 tools/apple/check_png.py ci_level.png --min-size 640x360      # it decodes the picture and fails if it is one flat colour
```

On a machine with no display, put `xvfb-run -a -s "-screen 0 1280x720x24"` before the program (and a software Vulkan driver, `mesa-vulkan-drivers`, if there is no GPU). **With no game data and no `--root`** (a release program,
whose paths are not baked) the program says that the game's data was not found, where it looked, and exits 1; a developer's build, whose paths are baked, draws boxes instead.

## 6. Where the content of the full game comes from

The game itself (levels, pictures, sounds) is not in this repository and not in any program. A release archive carries it in a `content` folder beside the program (macOS and iOS: inside the bundle). Put that
folder beside the program, or give `--root <that folder>`. That is the whole story: the game looks there first (`packaging.md` section 1).

## 7. Packaging (the owner's step)

`python3 tools/package/release.py assemble --platform windows|linux --program <program> --content <content package> --out <folder>` and the rest of `packaging.md` section 8. `--no-data` makes a
test archive with no game (named `-nodata`; it is no release and is left out of a release's sums). Every archive carries the repository's `LICENSE` and `NOTICE.md`, taken from its root.

## 8. What was run as written

All on the owner's PC in the Linux subsystem (Ubuntu 24.04, g++ 13.3, CMake 3.28, Ninja 1.11, Python 3.12, a case-sensitive ext4 folder), 2026-10-27, unless the row says Windows. Anything not in this table was
**not run**.

| Command (section) | Result |
|---|---|
| `tools/build_linux.sh --help` (1) | prints, exit 0; names `--portable` and `--strip` |
| `tools/build_linux.sh` with its defaults (1), in a standing tree and, as the scripts of `ci.yml`'s `linux` job (taken out of the YAML and run here, not on a runner; `ci.md` section 6), in a clean local clone of the checkout | built, 0 warnings in `game/` and `tests/` (`ci.md` section 6 has the clone's numbers) |
| `build/tests/test_mp --list`, `build/tests/test_mp test_mp_numbers` (1) | 77 suites; `test_mp_numbers`: 0 failures (its count of checks depends on the locales installed: 446 in the implementer's tree, 442 in the verifier's, and 444 in step 199's) |
| `ctest --test-dir build -N` (1) | 80 tests listed |
| `build/tests/test_mp` (every suite) with the owner's data present; `ctest --test-dir build` with it hidden (1) | with it: 77 suites, 77 passed, 87,943 checks; without it: 80 tests, 44 passed, 36 skipped (exit 77), 0 failed |
| `tools/build_linux.sh --no-baked-paths --dev-tools OFF --portable --strip --build-dir <a folder>` (1) | built; the program asks the system for `GLIBC_2.38` alone, four shared libraries, stripped, 12,135,904 bytes (the folder was not called `build-release`) |
| `release.py check-program <that program> --platform linux --max-glibc 2.38`, and the same with `2.35` (1) | passes; refused in words (the program asks for glibc 2.38, the floor is 2.35) |
| the block of section 5, taken from this file, under `xvfb-run -a` on the software Vulkan driver | each line exit 0; "finished: clean"; `ci_level.png: 1280x720, 4 distinct pixel value(s), 37014 bytes` |
| the same fixture level started with the program of an unpacked no-data archive of the release program (`--root` the fixture, as section 5) | exit 0, a 1280x720 picture of 4 distinct pixel values |
| the release program with no data and no `--root` (5) | exit 1, "the game's data was not found" |
| `tools/build_linux.sh --configure-only`, and with the release options (1) | exit 0; the cache shows `-static-libstdc++ -static-libgcc -Wl,--strip-all` for the second |
| `python3 tools/package/release.py check-version` (7) | ok |
| `python tools/art/make_app_icon.py` (2), **Windows** | wrote the icon |
| `python3 tools/package/release.py assemble --platform linux --program <the release program of planning step 209, not rebuilt in step 210> --no-data --max-glibc 2.38 --out <a folder>` (7), twice, from two folders (planning step 210) | exit 0 both times, the same bytes (5,212,292; sha256 `5eeb3e28...`, taken from the final `LICENSE` and `NOTICE.md`, which hold the GCC runtime exception); unpacked with `tar xzf`: `LICENSE`, `NOTICE.md`, `PLAYING.txt`, `MagicPortals` and `assets/shaders` (17 files); the `LICENSE` and `NOTICE.md` in it are the repository's, byte for byte |
| `python tools/ci/check_public.py --include-untracked` (the public-readiness check, `ci.md` section 8), **Windows**, Python 3.14 (planning step 210) | exit 0: no ERROR, 3 OWNER-FILE findings, 4 RECORD-PATH findings, 24 NOTE lines; the lines are the maintainer's to read |

**Not run:** `git clone --recurse-submodules` and `git submodule update --init` against the real address (the clean clone was a local one, with the engine cloned from the local module repository and
verified to be the pinned commit); `sudo apt-get install ...` (the packages were already there, checked with `dpkg -s`); everything in section 2 but the icon (**no Windows program was built, linked or started
in this step**, by the owner's rule); `tools/build_android.sh --no-data --abi all` and everything in section 3 (the Android SDK was not used; `ci.yml`'s `android` job is the first run of the Linux path of
that script); everything in section 4 (no Mac); the `windows`, `android` and `tools` jobs of `ci.yml`, `release.yml` and `android-emulator.yml` (`ci.md` section 6).
