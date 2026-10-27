# Contributing

Thank you for looking. This page says how to build the project, how to test it, and what a change has to carry.

## Never send game files

**Never include game files, art, audio, levels or an APK in an issue or a pull request.** This repository holds none of the original game's content, and none is to be added to it. A test needs a picture or a sound? Write a small synthetic one in the test.

## Build and test

Every command below exists in this repository. [docs/building.md](docs/building.md) lists which ones were run as written, by whom, and which were not.

**Linux** (built on Ubuntu 24.04; 22.04 is named by the project and untried). The packages are the ones `tools/build_linux.sh` names in its header ([docs/building.md](docs/building.md), section 1):

```
git clone --recurse-submodules https://github.com/IvanCvetanovic/Magic-Portals-Enhanced.git
cd Magic-Portals-Enhanced
tools/build_linux.sh
ctest --test-dir build
```

**Windows** (MSVC, from an x64 Native Tools prompt):

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DSUPERSONIC_ENABLE_VALIDATION=OFF -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl -DGLSL_COMPILER=GLSL_COMPILER-NOTFOUND
cmake --build build --target MagicPortals test_mp
ctest --test-dir build
```

`-DGLSL_COMPILER=GLSL_COMPILER-NOTFOUND` keeps CMake from looking for a shader compiler: the compiled shaders are committed in `engine/`, and a compiler that was found would rewrite them ([docs/building.md](docs/building.md), section 0).

**Android:** `tools/build_android.sh --no-data --abi all` (the Android SDK, NDK r28 and a JDK 17; [docs/android.md](docs/android.md)).

**macOS and iOS** need a Mac. The configure lines are in `.github/workflows/apple.yml`; [docs/apple.md](docs/apple.md) has the rest.

Without the original's converted levels, some suites exit with code 77 (on Linux, 36 of the 80 tests in a clean clone), which `ctest` reports as skipped, never as passed. The suites that touch your change are the ones to run: `build/tests/test_mp test_mp_<suite>` (`test_mp.exe` on Windows); `build/tests/test_mp --list` names them.

**The Python tools** use the standard library. From the repository root:

```
python -m unittest discover -s tools/package -p "test_*.py"
python -m unittest discover -s tools/ci -p "test_*.py"
python -m unittest discover -s tools/apple -p "test_*.py"
python tools/package/check_version.py
python tools/ci/check_workflows.py
python tools/ci/check_public.py
```

(`check_workflows.py` needs PyYAML. `tools/converter` has its own `python -m pytest -q` and `python -m ruff check .`.)

## What a change carries

* **No compiler warning** in `game/` or `tests/`. The build must finish with zero warnings there.
* **A Step with its gates.** A change to the game is written down as a numbered Step in `docs/planning/2026-09-11-magic-portals-remaster.md`, and the Step says what was required and what was measured. A change that moves a pixel or a tick of a level says so.
* **Tunable numbers live in JSON** (`data/`, `game/data/`) with where each came from, not in code.
* **The engine is not edited here.** `engine/` is a git submodule; a gap in the engine is fixed in its own repository, and this one then moves to that commit.
* **Scripts have LF line endings**, and a shell script is committed as executable.

How a commit message is worded is for the maintainer to say; write it plainly and the maintainer will tell you if it should change.
