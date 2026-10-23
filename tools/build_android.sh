#!/usr/bin/env bash
# Build Magic Portals Enhanced for Android and package an APK, without Gradle (planning step 203, PORT-5).
#
#   tools/build_android.sh (--data <content folder> | --no-data) [options]
#
# NOT RUN ON A PHONE OR AN EMULATOR as of this step: this builds, packages and statically checks. What it makes has never been installed.
#
# Two halves. This script finds the Android SDK and NDK, configures and builds the game's shared library for each ABI in a tree of its own
# under out/build/ (the repository's .gitignore covers out/ and nothing else), strips it, and hands it to tools/package/android_package.py, which
# does the rest in Python: the Java side, the resources and icon, the unpacked-on-first-run data list, the zip, zipalign, apksigner and the checks.
#
# THE DATA. The APK carries the game's data, unpacked on the first run into the app's private files folder (game/sim/Unpack.hpp). It comes from
# the content packager's output (tools/package/package.py content ..., docs/packaging.md), built from YOUR copy of the original: none of it is in
# this repository and none of it is ever committed.
#   --data <folder>      the content packager's output folder (it holds manifest.json). Every file is re-hashed against it.
#   --no-data            a shell with no game data in it (what CI can build): the game starts and says it has no data.
#
# Options:
#   --abi arm64-v8a|x86_64|all   which ABIs (default all: both go in the one APK)
#   --verification       THE VERIFICATION BUILD: MAGICPORTALS_DEV_TOOLS=ON, so the developer's flags (--route, --tap, --visit-levels ...) are in
#                        the program, for a run on an emulator driven by files/magicportals_args.txt. Its trees, libraries and APK carry
#                        VERIFICATION in their names, and it is never signed with a release key. A player's APK never has them.
#   --release            THE RELEASE APK: not debuggable, signed with YOUR key, never the debug one. Needs the three key options below. It is
#                        refused with --verification, and with --no-data unless --allow-data-less-release is also given.
#     --keystore <file>             the release keystore (PKCS12 or JKS). No default: this repository has no key and takes none from anywhere.
#     --keystore-pass-file <file>   a file whose first line is the keystore's password (also the key's)
#     --key-alias <alias>           the key's alias in the keystore (required)
#   Without --release the APK is a DEBUG APK: debuggable, signed with the machine's standard debug keystore (~/.android/debug.keystore).
#   --version-code <n>   the APK's versionCode (default 1): a whole number that goes UP BY ONE with every release, or Android refuses the update.
#                        versionName is the project VERSION in the top-level CMakeLists.txt, the one place it is written.
#   --config Release     the native build type (only Release is packaged)
#   --jobs <n>           compile jobs per ABI (default 6; fewer when memory is short)
#   --out <folder>       where the APK, SHA256SUMS.txt and the work folder go (default out/android, under the repository)
#   --build-only         stop after the shared libraries
#   --package-only       skip configure and build; package the libraries already built
#   --sdk <dir>, --ndk <dir>, --build-tools <version>   where the tools are (below)
#   -h, --help
#
# Finds, in this order: the SDK from --sdk, $ANDROID_SDK, $ANDROID_HOME, $ANDROID_SDK_ROOT, else the Android Studio default for this host;
# the NDK from --ndk, $ANDROID_NDK_HOME, $ANDROID_NDK_ROOT, else the newest <sdk>/ndk/<version> (r28 or newer: its 16 KB page alignment is what
# newer phones demand); build-tools matching the manifest's targetSdk, else the newest; the SDK's own CMake and Ninja when it has them, else
# the ones on PATH; a JDK 17 or newer (JAVA_HOME, else PATH) and Python 3. Written for Git Bash on Windows (run) and for bash on Linux and
# macOS (written from the same rules, NOT RUN: the Linux and macOS paths of this script have never been executed).
#
# Output: <out>/MagicPortals-<version>[-nodata][-VERIFICATION]-<debug|release>.apk and <out>/SHA256SUMS.txt.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
die() { echo "build_android.sh: $*" >&2; exit 1; }
note() { echo "== $*"; }

case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) HOST=windows ;;
    Darwin) HOST=macos ;;
    *) HOST=linux ;;
esac
if [ "$HOST" = windows ]; then
    EXE=.exe
    NDK_TAG=windows-x86_64
    native() { cygpath -m "$1"; }       # the form Windows programs take
    posixp() { cygpath -u "$1"; }       # the form this shell takes
elif [ "$HOST" = macos ]; then
    EXE=""
    NDK_TAG=darwin-x86_64
    native() { printf '%s' "$1"; }
    posixp() { printf '%s' "$1"; }
else
    EXE=""
    NDK_TAG=linux-x86_64
    native() { printf '%s' "$1"; }
    posixp() { printf '%s' "$1"; }
fi

ABI_ARG=all
CONFIG=Release
DATA=""
NO_DATA=0
VERIFICATION=0
RELEASE=0
ALLOW_DATALESS_RELEASE=0
KEYSTORE=""
PASS_FILE=""
KEY_ALIAS=""
VERSION_CODE=1
JOBS=6
OUT_ARG=""
BUILD_ONLY=0
PACKAGE_ONLY=0
SDK_ARG=""
NDK_ARG=""
BT_ARG=""
usage() { awk 'NR > 1 && /^set -euo/ { exit } NR > 1 { sub(/^# ?/, ""); print }' "${BASH_SOURCE[0]}"; }
while [ $# -gt 0 ]; do
    case "$1" in
        --abi) [ $# -ge 2 ] || die "--abi needs a value"; ABI_ARG="$2"; shift 2 ;;
        --config) [ $# -ge 2 ] || die "--config needs a value"; CONFIG="$2"; shift 2 ;;
        --data) [ $# -ge 2 ] || die "--data needs a folder"; DATA="$2"; shift 2 ;;
        --no-data) NO_DATA=1; shift ;;
        --verification) VERIFICATION=1; shift ;;
        --release) RELEASE=1; shift ;;
        --allow-data-less-release) ALLOW_DATALESS_RELEASE=1; shift ;;
        --keystore) [ $# -ge 2 ] || die "--keystore needs a file"; KEYSTORE="$2"; shift 2 ;;
        --keystore-pass-file) [ $# -ge 2 ] || die "--keystore-pass-file needs a file"; PASS_FILE="$2"; shift 2 ;;
        --key-alias) [ $# -ge 2 ] || die "--key-alias needs a value"; KEY_ALIAS="$2"; shift 2 ;;
        --version-code) [ $# -ge 2 ] || die "--version-code needs a number"; VERSION_CODE="$2"; shift 2 ;;
        --jobs) [ $# -ge 2 ] || die "--jobs needs a number"; JOBS="$2"; shift 2 ;;
        --out) [ $# -ge 2 ] || die "--out needs a folder"; OUT_ARG="$2"; shift 2 ;;
        --build-only) BUILD_ONLY=1; shift ;;
        --package-only) PACKAGE_ONLY=1; shift ;;
        --sdk) [ $# -ge 2 ] || die "--sdk needs a folder"; SDK_ARG="$2"; shift 2 ;;
        --ndk) [ $# -ge 2 ] || die "--ndk needs a folder"; NDK_ARG="$2"; shift 2 ;;
        --build-tools) [ $# -ge 2 ] || die "--build-tools needs a version"; BT_ARG="$2"; shift 2 ;;
        -h|--help) usage; exit 0 ;;
        *) die "unknown option $1 (--help lists them)" ;;
    esac
done

# ---- What was asked, checked before anything is built ---------------------------------------------------------------------------------
case "$ABI_ARG" in
    all) ABIS=(arm64-v8a x86_64) ;;
    arm64-v8a|x86_64) ABIS=("$ABI_ARG") ;;
    *) die "--abi wants arm64-v8a, x86_64 or all, got '$ABI_ARG'" ;;
esac
[ "$CONFIG" = Release ] || die "only --config Release is packaged (got '$CONFIG')"
case "$VERSION_CODE" in ''|*[!0-9]*|0) die "--version-code wants a whole number of at least 1, got '$VERSION_CODE'" ;; esac
case "$JOBS" in ''|*[!0-9]*|0) die "--jobs wants a whole number of at least 1, got '$JOBS'" ;; esac
if [ "$PACKAGE_ONLY" -eq 1 ] && [ "$BUILD_ONLY" -eq 1 ]; then die "--build-only and --package-only together build nothing"; fi
if [ "$BUILD_ONLY" -eq 0 ]; then
    if [ -n "$DATA" ] && [ "$NO_DATA" -eq 1 ]; then die "--data and --no-data are two answers to one question"; fi
    if [ -z "$DATA" ] && [ "$NO_DATA" -eq 0 ]; then die "say where the game's data is: --data <the content packager's output folder>, or --no-data for a shell without it"; fi
fi
if [ "$RELEASE" -eq 1 ]; then
    [ "$VERIFICATION" -eq 0 ] || die "--release and --verification together: the verification build has the developer's tools in and is never signed with a release key"
    [ -n "$KEYSTORE" ] || die "--release needs --keystore <file>: this repository has no key and uses none that it did not make (the debug key is never used for a release)"
    [ -n "$PASS_FILE" ] || die "--release needs --keystore-pass-file <file>"
    [ -n "$KEY_ALIAS" ] || die "--release needs --key-alias <alias>"
    [ -f "$(posixp "$KEYSTORE")" ] || die "--release: no keystore at $KEYSTORE"
    [ -s "$(posixp "$PASS_FILE")" ] || die "--release: no password file at $PASS_FILE (or it is empty)"
    if [ "$NO_DATA" -eq 1 ] && [ "$ALLOW_DATALESS_RELEASE" -eq 0 ]; then die "--release with --no-data would publish an APK with no game in it; add --allow-data-less-release if that is meant"; fi
fi
if [ "$RELEASE" -eq 0 ] && { [ -n "$KEYSTORE" ] || [ -n "$PASS_FILE" ] || [ -n "$KEY_ALIAS" ]; }; then die "a key was named but --release was not given (a debug APK is signed with the debug key)"; fi

# ---- The tools ------------------------------------------------------------------------------------------------------------------------
if [ -n "$SDK_ARG" ]; then SDK="$SDK_ARG"
elif [ -n "${ANDROID_SDK:-}" ]; then SDK="$ANDROID_SDK"
elif [ -n "${ANDROID_HOME:-}" ]; then SDK="$ANDROID_HOME"
elif [ -n "${ANDROID_SDK_ROOT:-}" ]; then SDK="$ANDROID_SDK_ROOT"
elif [ "$HOST" = windows ] && [ -n "${LOCALAPPDATA:-}" ]; then SDK="$LOCALAPPDATA/Android/Sdk"
elif [ "$HOST" = macos ]; then SDK="$HOME/Library/Android/sdk"
else SDK="$HOME/Android/Sdk"
fi
SDK="$(posixp "$SDK")"
[ -d "$SDK" ] || die "no Android SDK at $SDK (give --sdk, or set ANDROID_HOME)"

newest() { # the highest version-sorted subfolder name of $1 (no release candidates)
    { ls -1 "$1" 2>/dev/null | grep -v -- '-' | sort -V | tail -n 1; } || true
}
if [ -n "$NDK_ARG" ]; then NDK="$NDK_ARG"
elif [ -n "${ANDROID_NDK_HOME:-}" ]; then NDK="$ANDROID_NDK_HOME"
elif [ -n "${ANDROID_NDK_ROOT:-}" ]; then NDK="$ANDROID_NDK_ROOT"
else
    ndk_version="$(newest "$SDK/ndk")"
    [ -n "$ndk_version" ] || die "no NDK under $SDK/ndk (install r28 or newer, or give --ndk)"
    NDK="$SDK/ndk/$ndk_version"
fi
NDK="$(posixp "$NDK")"
[ -f "$NDK/build/cmake/android.toolchain.cmake" ] || die "$NDK is not an NDK (no build/cmake/android.toolchain.cmake)"
ndk_major="$(sed -n 's/^Pkg.Revision *= *\([0-9]*\)\..*/\1/p' "$NDK/source.properties" | head -n 1)"
[ -n "$ndk_major" ] && [ "$ndk_major" -ge 28 ] || die "the NDK at $NDK is r${ndk_major:-?}; r28 or newer is required (its default 16 KB page alignment)"

MANIFEST="$REPO/game/android/AndroidManifest.xml"
[ -f "$MANIFEST" ] || die "no $MANIFEST"
manifest_number() { # one attribute of the manifest's <uses-sdk>, exactly one match
    local found
    found="$(sed -n "s/.*android:$1=\"\\([0-9][0-9]*\\)\".*/\\1/p" "$MANIFEST")"
    [ "$(printf '%s\n' "$found" | grep -c .)" -eq 1 ] || die "the manifest must state android:$1 exactly once (found: $found)"
    printf '%s' "$found"
}
MIN_SDK="$(manifest_number minSdkVersion)"
TARGET_SDK="$(manifest_number targetSdkVersion)"

if [ -n "$BT_ARG" ]; then BUILD_TOOLS_VERSION="$BT_ARG"
elif [ -d "$SDK/build-tools/$TARGET_SDK.0.0" ]; then BUILD_TOOLS_VERSION="$TARGET_SDK.0.0"
else BUILD_TOOLS_VERSION="$(newest "$SDK/build-tools")"
fi
BUILD_TOOLS="$SDK/build-tools/$BUILD_TOOLS_VERSION"
[ -d "$BUILD_TOOLS" ] || die "no build-tools '$BUILD_TOOLS_VERSION' under $SDK/build-tools"
ANDROID_JAR="$SDK/platforms/android-$TARGET_SDK/android.jar"
[ -f "$ANDROID_JAR" ] || die "no $ANDROID_JAR (install platform android-$TARGET_SDK)"

sdk_tool() { # a tool of the SDK's cmake folder, else the one on PATH
    local name="$1" newest_cmake found
    newest_cmake="$(newest "$SDK/cmake")"
    if [ -n "$newest_cmake" ] && [ -x "$SDK/cmake/$newest_cmake/bin/$name$EXE" ]; then printf '%s' "$SDK/cmake/$newest_cmake/bin/$name$EXE"; return; fi
    found="$(command -v "$name" 2>/dev/null || true)"
    [ -n "$found" ] || die "no $name (the SDK's cmake package has one; so does a normal install)"
    printf '%s' "$found"
}
CMAKE="$(sdk_tool cmake)"
NINJA="$(sdk_tool ninja)"
# Python 3: $PYTHON, else `python` on Windows (where `python3` is often the Store's stub that only prints a hint) and `python3` elsewhere.
pick_python() {
    local name
    for name in "$@"; do
        if command -v "$name" >/dev/null 2>&1 && "$name" -c 'import sys; sys.exit(0 if sys.version_info >= (3, 8) else 1)' >/dev/null 2>&1; then printf '%s' "$name"; return; fi
    done
}
if [ -n "${PYTHON:-}" ]; then PY="$PYTHON"
elif [ "$HOST" = windows ]; then PY="$(pick_python python python3)"
else PY="$(pick_python python3 python)"
fi
[ -n "$PY" ] || die "no Python 3.8 or newer (the packager is tools/package/android_package.py)"
if [ -n "${JAVA_HOME:-}" ] && [ -x "$(posixp "$JAVA_HOME")/bin/java$EXE" ]; then JAVA_BIN="$(posixp "$JAVA_HOME")/bin"
else JAVA_BIN="$(dirname "$(command -v java 2>/dev/null || die "no JDK: set JAVA_HOME (a JDK 17 or newer)")")"
fi
export PATH="$JAVA_BIN:$PATH"

OUT="${OUT_ARG:-$REPO/out/android}"
OUT="$(posixp "$OUT")"
# Inside the repository only under out/, the one part of it that is ignored: a build product anywhere else could be committed. The packager answers.
"$PY" "$(native "$REPO/tools/package/android_package.py")" check-out --repo "$(native "$REPO")" --out "$(native "$OUT")"
suffix=""
[ "$VERIFICATION" -eq 0 ] || suffix="-verification"
DEV_TOOLS=OFF
[ "$VERIFICATION" -eq 0 ] || DEV_TOOLS=ON

note "host $HOST; SDK $SDK; NDK $NDK (r$ndk_major); build-tools $BUILD_TOOLS_VERSION; platform android-$TARGET_SDK; minSdk $MIN_SDK"
note "cmake $CMAKE; ninja $NINJA; python $PY; java $JAVA_BIN"
note "variant: $([ "$VERIFICATION" -eq 1 ] && echo 'VERIFICATION (developer tools ON)' || echo 'player (developer tools OFF)'), $([ "$RELEASE" -eq 1 ] && echo release || echo debug)-signed, $([ "$NO_DATA" -eq 1 ] && echo 'no game data' || echo 'with game data')"

# ---- Configure and build, one tree per ABI ----------------------------------------------------------------------------------------------
LIB_ARGS=()
TREE_ARGS=()
for abi in "${ABIS[@]}"; do
    tree="$REPO/out/build/android-$abi$suffix"
    TREE_ARGS+=(--build-tree "$(native "$tree")")
    if [ "$PACKAGE_ONLY" -eq 0 ]; then
        note "configure $abi in $tree"
        # Restated on EVERY run, so an option can never linger in a cache into the next build of the same tree: validation OFF (a phone has no
        # validation layers, and the packager refuses a tree whose cache says ON), the committed shaders used as they are (a different glslc rewrites
        # them), no baked data path (there is no repository on a phone), no suites, the developer's tools as the variant says.
        "$CMAKE" -S "$(native "$REPO")" -B "$(native "$tree")" -G Ninja \
            -DCMAKE_MAKE_PROGRAM="$(native "$NINJA")" \
            -DCMAKE_TOOLCHAIN_FILE="$(native "$NDK/build/cmake/android.toolchain.cmake")" \
            -DANDROID_ABI="$abi" \
            -DANDROID_PLATFORM="android-$MIN_SDK" \
            -DANDROID_STL=c++_static \
            -DCMAKE_BUILD_TYPE="$CONFIG" \
            -DSUPERSONIC_ENABLE_VALIDATION=OFF \
            -DGLSL_COMPILER=OFF \
            -DMAGICPORTALS_BAKED_PATHS=OFF \
            -DMAGICPORTALS_BUILD_TESTS=OFF \
            -DMAGICPORTALS_MOBILE=ON \
            -DMAGICPORTALS_DEV_TOOLS="$DEV_TOOLS"
        note "build $abi"
        "$CMAKE" --build "$(native "$tree")" --target MagicPortals -j "$JOBS"
    fi
    lib="$tree/game/libMagicPortals.so"
    [ -f "$lib" ] || die "not built: $lib"
    mkdir -p "$OUT/lib/$abi$suffix"
    "$NDK/toolchains/llvm/prebuilt/$NDK_TAG/bin/llvm-strip$EXE" --strip-unneeded -o "$(native "$OUT/lib/$abi$suffix/libMagicPortals.so")" "$(native "$lib")"
    LIB_ARGS+=(--lib "$abi=$(native "$OUT/lib/$abi$suffix/libMagicPortals.so")")
    note "stripped library: $OUT/lib/$abi$suffix/libMagicPortals.so ($(stat -c %s "$OUT/lib/$abi$suffix/libMagicPortals.so" 2>/dev/null || stat -f %z "$OUT/lib/$abi$suffix/libMagicPortals.so") bytes)"
done
if [ "$BUILD_ONLY" -eq 1 ]; then note "built (--build-only)"; exit 0; fi

# ---- Package ----------------------------------------------------------------------------------------------------------------------------
args=(build --repo "$(native "$REPO")" --sdk "$(native "$SDK")" --ndk "$(native "$NDK")" --build-tools "$BUILD_TOOLS_VERSION" --out "$(native "$OUT")"
      --version-code "$VERSION_CODE")
if [ "$NO_DATA" -eq 1 ]; then args+=(--no-data); else args+=(--package "$(native "$(posixp "$DATA")")"); fi
[ "$VERIFICATION" -eq 0 ] || args+=(--verification)
if [ "$RELEASE" -eq 1 ]; then
    args+=(--release --keystore "$(native "$(posixp "$KEYSTORE")")" --keystore-pass-file "$(native "$(posixp "$PASS_FILE")")" --key-alias "$KEY_ALIAS")
    [ "$ALLOW_DATALESS_RELEASE" -eq 0 ] || args+=(--allow-data-less-release)
fi
note "package"
"$PY" "$(native "$REPO/tools/package/android_package.py")" "${args[@]}" "${LIB_ARGS[@]}" "${TREE_ARGS[@]}"
