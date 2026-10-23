#!/usr/bin/env bash
# Configure and build Magic Portals Enhanced on Linux: the game (MagicPortals) and its suites (test_mp), Release, Ninja.
#
# Written for Ubuntu 24.04 with g++ 13 or clang 18 and measured there (planning step 199, docs/planning/2026-09-11-magic-portals-remaster.md).
# It installs nothing and exits non-zero on the first failure. It writes into the build directory only, with two exceptions that it
# says aloud: with no engine checkout it runs `git submodule update --init engine`, and --clean removes the build directory (only a
# directory that holds THIS repository's CMakeCache.txt: any other is refused, and a missing one is left alone).
#
#   tools/build_linux.sh [options]
#
# The game's data is NOT in this repository (README, "The data"): the converted levels, the original's extracted assets and the
# achievements table are the owner's, gitignored, and a developer's build finds them through six paths it bakes in. Without them the
# game draws boxes and 31 suites skip (exit 77, reported as skipped, never as passed). Give them with one folder or one by one:
#
#   --assets-from DIR    a folder laid out like this repository after the converter and the extraction have run:
#                          DIR/out/levels   DIR/out/data/chapters.json   DIR/out/data/achievements.json
#                          DIR/reference/extracted/assets   DIR/reference/magic-rampage/controls   DIR/data
#                        (the repository root itself qualifies). Sets each variable below whose part DIR holds. DIR/data (Enhanced's
#                        tables) is taken when it exists and DIR is not this repository; this repository's own data/ is committed.
#   --levels DIR         env SUPERSONIC_MAGICPORTALS_LEVELS        the converted levels (default: <repo>/out/levels)
#   --chapters FILE      env SUPERSONIC_MAGICPORTALS_CHAPTERS      chapters.json (default: <levels>/../data/chapters.json)
#   --original DIR       env SUPERSONIC_MAGICPORTALS_ORIGINAL      the original's extracted assets (default: <levels>/../../reference/extracted/assets)
#   --achievements FILE  env SUPERSONIC_MAGICPORTALS_ACHIEVEMENTS achievements.json (default: <levels>/../data/achievements.json)
#   --data DIR           env SUPERSONIC_MAGICPORTALS_DATA          Enhanced's own tables (default: <repo>/data, committed)
#   --padart DIR         env SUPERSONIC_MAGICPORTALS_PADART        Magic Rampage's control pictures, optional (default:
#                                                                  <repo>/reference/magic-rampage/controls); without them a modern-controls
#                                                                  run draws the original's corner sprites, then its own glyphs
#
# A RELEASE build (planning step 200) bakes none of the above:
#   --no-baked-paths     configure with MAGICPORTALS_BAKED_PATHS=OFF: no data path, no checkout path and no account name is compiled into
#                        the program, which then finds its data at run time (MagicPortals --root DIR, a "content" folder next to the
#                        program, or the user's data folder; docs/packaging.md). The suites read the baked paths, so this builds the
#                        game alone (default target MagicPortals) and any data option above is ignored. Use a build directory of its own.
#
# What kind of program (planning step 201; docs/packaging.md says which belong in a release). Neither is passed unless given, so a build
# with neither is the build it was, and an existing build folder keeps what it has:
#   --dev-tools ON|OFF|AUTO   configure with MAGICPORTALS_DEV_TOOLS: the developer's flags (--route, --tap, --visit-levels, the measurement
#                        switches ...) and the two files behind them. OFF is a player's program: a developer's flag is refused (exit 1, a
#                        message that says why), --help does not list them, and the program is smaller. AUTO (the CMake default) is ON,
#                        and OFF with --mobile. The suites always build with the tools.
#   --mobile             configure with MAGICPORTALS_MOBILE=ON: a phone's build, compiled on this machine. The run starts under the phone
#                        profile (game/data/engine.json "mobile": a lowest Vulkan version of 1.1 among it) unless --profile desktop says
#                        otherwise, and the developer tools and the measurement tool are left out unless --dev-tools ON says they stay.
#                        It adds no Android or iOS entry point; that is a later step. (The Windows release's options, MAGICPORTALS_WIN_GUI
#                        and MAGICPORTALS_ICON, are MSVC's and are not passed here.)
#
# Build:
#   --build-dir DIR      where to build (default: <repo>/build; a folder named build is gitignored). Build on a Linux file system:
#                        a Windows drive mounted into WSL (/mnt/c) is case-insensitive and slow, and a build there is refused unless
#                        --allow-drvfs is given.
#   --jobs N             compile jobs (default: min(cores, RAM in GiB / 1.5): the layer's translation unit needs about 2 GiB)
#   --cxx g++|clang++    the compiler (default: $CXX, else g++, else clang++)
#   --sanitize           UndefinedBehaviorSanitizer + AddressSanitizer, Release with -g (use another --build-dir)
#   --validation         build the engine with Vulkan validation (needs the Khronos validation layer at run time)
#   --wayland            build GLFW's Wayland backend too (needs libwayland-dev and wayland-protocols; default: X11 only)
#   --target NAME        build this target (repeatable; default: MagicPortals test_mp)
#   --configure-only     stop after the configure step
#   --clean              remove the build directory first
#   -h, --help
#
# The engine is a git submodule at engine/. In a clone: `git submodule update --init` (or this script does it when .git is there).
# Packages the engine needs to build (the engine's own CI line; nothing here installs them):
#   sudo apt-get install -y build-essential cmake ninja-build pkg-config libvulkan-dev libx11-dev libxrandr-dev libxinerama-dev \
#        libxcursor-dev libxi-dev libxkbcommon-dev libasound2-dev
# To run: a display and a Vulkan driver (llvmpipe, from mesa-vulkan-drivers, is enough: software, slow). The suites need neither.
#   <build-dir>/tests/test_mp                      every suite;  test_mp --list names them;  test_mp test_mp_numbers  runs one
#   <build-dir>/game/MagicPortals --help           the game's flags
# Sound: the engine decodes MP3 through its vendored dr_mp3 here (Media Foundation on Windows), so the original's 46 sounds decode (planning step
# 202; a clip starts about 12 ms apart from Windows). Playing them needs a sound device (ALSA), which a headless run lacks.

set -euo pipefail

die() { printf 'build_linux.sh: %s\n' "$*" >&2; exit 1; }
note() { printf 'build_linux.sh: %s\n' "$*"; }

REPO="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"

levels="${SUPERSONIC_MAGICPORTALS_LEVELS:-}"
chapters="${SUPERSONIC_MAGICPORTALS_CHAPTERS:-}"
original="${SUPERSONIC_MAGICPORTALS_ORIGINAL:-}"
achievements="${SUPERSONIC_MAGICPORTALS_ACHIEVEMENTS:-}"
datadir="${SUPERSONIC_MAGICPORTALS_DATA:-}"
padart="${SUPERSONIC_MAGICPORTALS_PADART:-}"
baked=ON
devtools=""
mobile=""
build_dir="$REPO/build"
jobs=""
cxx="${CXX:-}"
sanitize=0
validation=OFF
wayland=OFF
configure_only=0
clean=0
allow_drvfs=0
targets=()

while [ "$#" -gt 0 ]; do
    case "$1" in
        --assets-from)
            [ "$#" -ge 2 ] || die "--assets-from needs a folder"
            root="$(cd -- "$2" 2>/dev/null && pwd -P)" || die "--assets-from: no folder $2"
            [ -d "$root/out/levels" ] && levels="$root/out/levels"
            [ -f "$root/out/data/chapters.json" ] && chapters="$root/out/data/chapters.json"
            [ -f "$root/out/data/achievements.json" ] && achievements="$root/out/data/achievements.json"
            [ -d "$root/reference/extracted/assets" ] && original="$root/reference/extracted/assets"
            [ -d "$root/reference/magic-rampage/controls" ] && padart="$root/reference/magic-rampage/controls"
            [ -d "$root/data" ] && [ "$root" != "$REPO" ] && datadir="$root/data"
            shift 2 ;;
        --levels)       [ "$#" -ge 2 ] || die "--levels needs a folder"; levels="$2"; shift 2 ;;
        --chapters)     [ "$#" -ge 2 ] || die "--chapters needs a file"; chapters="$2"; shift 2 ;;
        --original)     [ "$#" -ge 2 ] || die "--original needs a folder"; original="$2"; shift 2 ;;
        --achievements) [ "$#" -ge 2 ] || die "--achievements needs a file"; achievements="$2"; shift 2 ;;
        --data)         [ "$#" -ge 2 ] || die "--data needs a folder"; datadir="$2"; shift 2 ;;
        --padart)       [ "$#" -ge 2 ] || die "--padart needs a folder"; padart="$2"; shift 2 ;;
        --no-baked-paths) baked=OFF; shift ;;
        --dev-tools)    [ "$#" -ge 2 ] || die "--dev-tools needs ON, OFF or AUTO"
                        case "$2" in ON|OFF|AUTO) devtools="$2" ;; *) die "--dev-tools takes ON, OFF or AUTO, not $2" ;; esac; shift 2 ;;
        --mobile)       mobile=ON; shift ;;
        --build-dir)    [ "$#" -ge 2 ] || die "--build-dir needs a folder"; build_dir="$2"; shift 2 ;;
        --jobs)         [ "$#" -ge 2 ] || die "--jobs needs a number"; jobs="$2"; shift 2 ;;
        --cxx)          [ "$#" -ge 2 ] || die "--cxx needs a compiler"; cxx="$2"; shift 2 ;;
        --target)       [ "$#" -ge 2 ] || die "--target needs a name"; targets+=("$2"); shift 2 ;;
        --sanitize)     sanitize=1; shift ;;
        --validation)   validation=ON; shift ;;
        --wayland)      wayland=ON; shift ;;
        --configure-only) configure_only=1; shift ;;
        --clean)        clean=1; shift ;;
        --allow-drvfs)  allow_drvfs=1; shift ;;
        -h|--help)      sed -n '2,/^set -euo/p' "${BASH_SOURCE[0]}" | sed '$d' | sed 's/^# \{0,1\}//'; exit 0 ;;
        *)              die "unknown option $1 (--help lists them)" ;;
    esac
done
if [ "${#targets[@]}" -eq 0 ]; then
    # The suites read the baked paths, so a release configure (tests off) has no test_mp to build.
    if [ "$baked" = ON ]; then targets=(MagicPortals test_mp); else targets=(MagicPortals); fi
fi

# ---- the tools --------------------------------------------------------------------------------------------------------------
command -v cmake >/dev/null 2>&1 || die "cmake is not installed (3.20 or newer)"
command -v ninja >/dev/null 2>&1 || die "ninja is not installed (ninja-build)"
if [ -z "$cxx" ]; then
    if command -v g++ >/dev/null 2>&1; then cxx=g++; elif command -v clang++ >/dev/null 2>&1; then cxx=clang++; else die "no C++ compiler (g++ or clang++)"; fi
fi
command -v "$cxx" >/dev/null 2>&1 || die "no compiler $cxx"
case "$cxx" in
    *clang++*) cc="${cxx%clang++*}clang${cxx##*clang++}" ;;
    *g++*)     cc="${cxx%g++*}gcc${cxx##*g++}" ;;
    *)         cc="${CC:-cc}" ;;
esac
command -v "$cc" >/dev/null 2>&1 || cc=cc
command -v pkg-config >/dev/null 2>&1 || note "pkg-config is not installed: the engine's GLFW may not find X11 (apt: pkg-config)"

# ---- where ------------------------------------------------------------------------------------------------------------------
case "$build_dir" in /*) ;; *) build_dir="$PWD/$build_dir" ;; esac
if [ "$allow_drvfs" -eq 0 ]; then
    case "$build_dir" in /mnt/[a-zA-Z]/*|/mnt/[a-zA-Z]) die "$build_dir is on a Windows drive mounted into WSL (case-insensitive, 9p): build on the Linux file system, or pass --allow-drvfs" ;; esac
fi

# ---- the engine -------------------------------------------------------------------------------------------------------------
if [ ! -f "$REPO/engine/CMakeLists.txt" ]; then
    if [ -e "$REPO/.git" ] && command -v git >/dev/null 2>&1; then
        note "engine/ is empty: git submodule update --init"
        git -C "$REPO" submodule update --init engine || die "the engine submodule could not be fetched"
    fi
    [ -f "$REPO/engine/CMakeLists.txt" ] || die "engine/ is empty: the Supersonic Engine is a git submodule (git submodule update --init), or copy it there"
fi

# ---- the data's five paths, as CMake takes them (only the ones given: the others keep the repository's own defaults) ----------------
cmake_data=()
add_path() { # variable, value
    if [ -n "$2" ]; then cmake_data+=("-D$1=$2"); fi
}
[ -z "$levels" ] || case "$levels" in /*) ;; *) levels="$PWD/$levels" ;; esac
[ -z "$chapters" ] || case "$chapters" in /*) ;; *) chapters="$PWD/$chapters" ;; esac
[ -z "$original" ] || case "$original" in /*) ;; *) original="$PWD/$original" ;; esac
[ -z "$achievements" ] || case "$achievements" in /*) ;; *) achievements="$PWD/$achievements" ;; esac
[ -z "$datadir" ] || case "$datadir" in /*) ;; *) datadir="$PWD/$datadir" ;; esac
[ -z "$padart" ] || case "$padart" in /*) ;; *) padart="$PWD/$padart" ;; esac
# Given one by one, the derived defaults (chapters, original, achievements follow the levels with a lexical ..) would point nowhere
# useful: state them from the levels folder's parent when only the levels were given.
if [ -n "$levels" ]; then
    parent="$(dirname -- "$levels")"
    [ -n "$chapters" ] || { [ -f "$parent/data/chapters.json" ] && chapters="$parent/data/chapters.json" || true; }
    [ -n "$achievements" ] || { [ -f "$parent/data/achievements.json" ] && achievements="$parent/data/achievements.json" || true; }
    [ -n "$original" ] || { [ -d "$parent/../reference/extracted/assets" ] && original="$(cd -- "$parent/../reference/extracted/assets" && pwd -P)" || true; }
fi
add_path SUPERSONIC_MAGICPORTALS_LEVELS "$levels"
add_path SUPERSONIC_MAGICPORTALS_CHAPTERS "$chapters"
add_path SUPERSONIC_MAGICPORTALS_ORIGINAL "$original"
add_path SUPERSONIC_MAGICPORTALS_ACHIEVEMENTS "$achievements"
add_path SUPERSONIC_MAGICPORTALS_DATA "$datadir"
add_path SUPERSONIC_MAGICPORTALS_PADART "$padart"
if [ "$baked" = OFF ]; then
    # Nothing is baked, so nothing is passed: the cache would only hold paths that no longer reach the program.
    cmake_data=()
    note "MAGICPORTALS_BAKED_PATHS=OFF: no data path is compiled in; run it with --root <folder> (docs/packaging.md)"
elif [ -z "$levels" ] && [ ! -d "$REPO/out/levels" ]; then
    note "no converted levels given and none at $REPO/out/levels: the game will draw boxes and 31 suites will skip (exit 77)"
fi

# ---- jobs -------------------------------------------------------------------------------------------------------------------
if [ -z "$jobs" ]; then
    cores="$(nproc 2>/dev/null || echo 2)"
    mem_kib="$(awk '/^MemTotal:/ {print $2}' /proc/meminfo 2>/dev/null || echo 4194304)"
    by_mem=$(( mem_kib * 2 / 3 / 1048576 ))
    [ "$by_mem" -ge 1 ] || by_mem=1
    jobs=$(( cores < by_mem ? cores : by_mem ))
fi

# ---- configure --------------------------------------------------------------------------------------------------------------
if [ "$clean" -eq 1 ] && [ -e "$build_dir" ]; then
    # rm -rf on a directory the person named: only one that is THIS repository's build (its CMakeCache.txt says so). A mistyped
    # --build-dir "$HOME" --clean once meant the whole home directory.
    if [ -f "$build_dir/CMakeCache.txt" ] && grep -qxF "CMAKE_HOME_DIRECTORY:INTERNAL=$REPO" "$build_dir/CMakeCache.txt"; then
        note "removing $build_dir"
        rm -rf -- "$build_dir"
    else
        die "--clean: $build_dir is not a build directory of this repository (no CMakeCache.txt naming $REPO), so it is not removed"
    fi
fi
flags=()
if [ "$sanitize" -eq 1 ]; then
    san="-fsanitize=undefined,address -fno-omit-frame-pointer -g"
    flags+=("-DCMAKE_CXX_FLAGS=$san" "-DCMAKE_C_FLAGS=$san" "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=undefined,address" "-DCMAKE_SHARED_LINKER_FLAGS=-fsanitize=undefined,address")
fi
note "compiler $cxx, jobs $jobs, build dir $build_dir"
cmake -S "$REPO" -B "$build_dir" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER="$cc" -DCMAKE_CXX_COMPILER="$cxx" \
    -DGLFW_BUILD_WAYLAND="$wayland" \
    -DSUPERSONIC_ENABLE_VALIDATION="$validation" \
    -DMAGICPORTALS_BAKED_PATHS="$baked" \
    ${devtools:+-DMAGICPORTALS_DEV_TOOLS="$devtools"} \
    ${mobile:+-DMAGICPORTALS_MOBILE="$mobile"} \
    ${cmake_data[@]+"${cmake_data[@]}"} \
    ${flags[@]+"${flags[@]}"} \
    -DSUPERSONIC_MAGICPORTALS_PARITY_PYTHON="$(command -v python3 || true)"
[ "$configure_only" -eq 0 ] || { note "configured"; exit 0; }

# ---- build ------------------------------------------------------------------------------------------------------------------
cmake --build "$build_dir" --target "${targets[@]}" -j "$jobs"

note "built: ${targets[*]}"
for t in "${targets[@]}"; do
    case "$t" in
        MagicPortals) note "  $build_dir/game/MagicPortals" ;;
        test_mp)      note "  $build_dir/tests/test_mp   (run it from $build_dir/tests, as ctest does)" ;;
    esac
done
