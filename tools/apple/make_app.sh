#!/usr/bin/env bash
# Assemble MagicPortals.app from a build, on a Mac, and package it: the counterpart of tools/package/package.py shell. It builds nothing and runs
# nothing of the game.
#
# WRITTEN, NEVER RUN ON A MAC (planning step 206, PORT-8). This is the sibling port's tools/apple/make_app.sh and make_release.sh, the one Apple glue of
# the sibling that ran on GitHub's macOS runners, made into one script and changed only where this game differs (Step 206 of the planning record,
# docs/planning/2026-09-11-magic-portals-remaster.md, has the table of every changed line). What was CHECKED here is the argument list of every external
# command, by tools/apple/test_make_app.py: it runs this file under bash on a machine with no Apple tool, with stand-ins for codesign, ditto, iconutil,
# lipo, otool, plutil, python3, unzip, xcrun and zip placed first on PATH, and compares what they were called with to a pinned list (and to --dry-run's,
# which prints each command and runs none). NOT KNOWN: that any of those commands, given their real arguments, does on a Mac what this script means.
#
# Usage:  bash tools/apple/make_app.sh macos <build dir> <out dir> [--moltenvk-license FILE] [--dry-run]
#         bash tools/apple/make_app.sh ios   <build dir> <out dir> [--sdk iphoneos|iphonesimulator] [--moltenvk-license FILE] [--dry-run]
#
# <build dir> is a configured and built tree (cmake --build <build dir> --target MagicPortals): it holds game/MagicPortals and, written by the configure,
# game/macos/Info.plist or game/ios/Info.plist (game/CMakeLists.txt makes them of the templates with the program's one version in them).
#
# What is written, and only under <out dir> (never beside the sources; a folder inside this repository that is not under out/ is refused, and so is /):
#   <out dir>/MagicPortals.app                       the bundle, signed ad hoc
#   <out dir>/MagicPortals-macOS-shell.zip           macos: the bundle and READ ME FIRST.txt in a folder MagicPortals/, made with ditto
#   <out dir>/MagicPortals-iOS-shell.ipa             ios --sdk iphoneos: Payload/MagicPortals.app zipped
#   <out dir>/MagicPortals-iOS-simulator-shell.zip   ios --sdk iphonesimulator: the bundle, made with ditto
#   <out dir>/.make_app-work/                        scratch (staging, icons, unpacked copy), removed at the end; a name no other folder has
#
# THE SHELL HAS NO GAME DATA, on purpose. The levels, art and sound are the owner's and never in this repository, so a CI run produces a program that
# starts, draws a fixture level and says plainly that it has no game. The data goes in later, on the owner's machine (docs/apple.md, three routes, none
# proven): into Contents/Resources/GameData on a Mac (then the signature is stale and must be made again), into GameData at the root of the iOS bundle.
#
# What goes in, and why each piece is where it is:
#   the executable     <build>/game/MagicPortals: Contents/MacOS/ on a Mac, the bundle's root on iOS.
#   Info.plist         <build>/game/<platform>/Info.plist, with the minimum OS version the executable was built for, and on iOS the platform
#                      (simulator or device) and the SDK facts Xcode records in every app it builds.
#   engine/assets/shaders/*.spv
#                      the engine's compiled shaders. Not assets/shaders, which would make the bundle itself the engine's asset root
#                      (ChooseAssetRoot): the engine writes where it runs, and a bundle is read-only. The game's entry copies them to a folder it may
#                      write in and runs from there (game/macos/MacMain.mm, game/ios/IOSMain.mm).
#   licenses/          what a bundle carries of the licences: this repository's LICENSE and NOTICE.md (the game's licence and the notices of the
#                      software it contains), the engine's licence and its list of third-party notices, MoltenVK's licence text when --moltenvk-license
#                      names it (MoltenVK is linked into the program, Apache-2.0), and a READ ME that says which file is which.
#   the icon           a PLACEHOLDER drawn by tools/art/make_app_icon.py from arithmetic alone (no file of the original is read): MagicPortals.icns on a
#                      Mac, AppIcon*.png on iOS (opaque: iOS fills what is transparent and the App Store refuses an alpha channel). The real icon is the
#                      owner's.
#
# Signed ad hoc (codesign --force --sign -), which is no identity: what arm64 needs to run it at all, and what a simulator installs. A device or the App
# Store needs a real signature, and a Mac run from a download needs notarising; neither is done here, and both are the owner's.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd -P)"

usage() {
    sed -n '/^# Usage:/,/^#$/p' "$0" | sed 's/^# \{0,1\}//' >&2
    exit 2
}
refuse() {
    echo "make_app.sh: $1" >&2
    exit 2
}
fail() {
    echo "make_app.sh: $1" >&2
    exit 1
}

DRY=0
SDK="iphoneos"
MVK_LICENSE=""
POSITIONAL=()
while [ $# -gt 0 ]; do
    case "$1" in
    --dry-run) DRY=1 ;;
    --sdk)
        SDK="${2:-}"
        shift
        ;;
    --moltenvk-license)
        MVK_LICENSE="${2:-}"
        shift
        ;;
    -h | --help) usage ;;
    -*) refuse "unknown option $1" ;;
    *) POSITIONAL+=("$1") ;;
    esac
    shift
done
PLATFORM="${POSITIONAL[0]:-}"
BUILD="${POSITIONAL[1]:-}"
OUT="${POSITIONAL[2]:-}"

if { [ "$PLATFORM" != macos ] && [ "$PLATFORM" != ios ]; } || [ -z "$BUILD" ] || [ -z "$OUT" ]; then
    usage
fi
case "$SDK" in
iphoneos | iphonesimulator) ;;
*) refuse "--sdk is iphoneos or iphonesimulator, not '$SDK'" ;;
esac

# ---- where it may write -------------------------------------------------------------------------------------------------------------------------
# <out dir> is resolved without being made (a dry run makes nothing). Refused: the file system's root, a home folder, the repository itself or a folder
# inside it that is not under out/ (which is gitignored), and a folder that holds the repository.
case "$OUT" in
"" | / | . | ..) refuse "<out dir> '$OUT' is not a folder this script may fill" ;;
esac
if [ -d "$OUT" ]; then
    OUT_ABS="$(cd "$OUT" && pwd -P)"
else
    OUT_PARENT="$(cd "$(dirname "$OUT")" 2>/dev/null && pwd -P)" || refuse "the folder that should hold '$OUT' does not exist"
    OUT_ABS="$OUT_PARENT/$(basename "$OUT")"
fi
case "$OUT_ABS" in
/ | "${HOME:-/nonexistent}") refuse "<out dir> '$OUT_ABS' is a place no script should fill" ;;
"$REPO") refuse "<out dir> is the repository itself" ;;
"$REPO/out" | "$REPO/out/"*) ;;
"$REPO/"*) refuse "<out dir> '$OUT_ABS' is inside the repository and not under out/, where nothing is tracked: this script writes nothing beside the sources" ;;
esac
case "$REPO/" in
"$OUT_ABS/"*) refuse "<out dir> '$OUT_ABS' holds the repository" ;;
esac

# ---- how every external command is run ---------------------------------------------------------------------------------------------------------------
# One door for all of them, so that --dry-run can print the list and the test can pin it. `run` runs a command; `ask VAR CANNED cmd...` runs one whose
# output the script reads (a dry run answers CANNED and prints the command).
say() {
    printf '+'
    printf ' %q' "$@"
    printf '\n'
}
run() {
    if [ "$DRY" = 1 ]; then say "$@"; else "$@"; fi
}
ask() {
    local var="$1" canned="$2" got
    shift 2
    if [ "$DRY" = 1 ]; then
        say "$@"
        got="$canned"
    else
        got="$("$@")" || fail "the command failed: $*"
    fi
    printf -v "$var" '%s' "$got"
}
note() { echo "$*"; }

# ---- the inputs ---------------------------------------------------------------------------------------------------------------------------------------
EXE="$BUILD/game/MagicPortals"
# CMake builds an iOS program as a bundle of its own, game/MagicPortals.app/MagicPortals (seen on GitHub's runner, run 40236827494), and a macOS one as a plain file.
if [ "$PLATFORM" = ios ] && [ ! -f "$EXE" ] && [ -f "$EXE.app/MagicPortals" ]; then EXE="$EXE.app/MagicPortals"; fi
PLIST_SRC="$BUILD/game/$PLATFORM/Info.plist"
SHADERS="$REPO/engine/assets/shaders"
if [ "$DRY" != 1 ]; then
    [ -f "$EXE" ] || fail "No executable at $EXE: build the MagicPortals target first."
    [ -f "$PLIST_SRC" ] || fail "No $PLIST_SRC: configure the build on an Apple platform (game/CMakeLists.txt writes it)."
    [ -f "$SHADERS/frag.spv" ] || fail "Missing $SHADERS/frag.spv (the engine's submodule is not checked out)."
    [ -f "$REPO/engine/LICENSE" ] && [ -f "$REPO/engine/THIRD_PARTY_LICENSES.md" ] || fail "Missing the engine's LICENSE or THIRD_PARTY_LICENSES.md."
    if [ -n "$MVK_LICENSE" ] && [ ! -f "$MVK_LICENSE" ]; then fail "No MoltenVK licence at $MVK_LICENSE."; fi
fi

APP="$OUT_ABS/MagicPortals.app"
WORK="$OUT_ABS/.make_app-work"
if [ "$PLATFORM" = macos ]; then
    BIN="$APP/Contents/MacOS"
    RES="$APP/Contents/Resources"
    PLIST="$APP/Contents/Info.plist"
else
    BIN="$APP"
    RES="$APP"
    PLIST="$APP/Info.plist"
fi

# ---- the bundle ---------------------------------------------------------------------------------------------------------------------------------------
run rm -rf "$APP" "$WORK"
run mkdir -p "$BIN" "$RES" "$WORK"

run cp "$EXE" "$BIN/MagicPortals"
run cp "$PLIST_SRC" "$PLIST"

# The minimum OS the executable was linked for (LC_BUILD_VERSION), so the plist cannot promise more.
ask OTOOL_L "      cmd LC_BUILD_VERSION
    minos 0.0" otool -l "$BIN/MagicPortals"
MINOS="$(printf '%s\n' "$OTOOL_L" | awk '/LC_BUILD_VERSION/ { found = 1 } found && $1 == "minos" { print $2; exit }')"
if [ -n "$MINOS" ]; then
    if [ "$PLATFORM" = macos ]; then
        run plutil -replace LSMinimumSystemVersion -string "$MINOS" "$PLIST"
    else
        run plutil -replace MinimumOSVersion -string "$MINOS" "$PLIST"
    fi
fi
if [ "$PLATFORM" = ios ]; then
    if [ "$SDK" = iphonesimulator ]; then
        run plutil -replace CFBundleSupportedPlatforms -json '["iPhoneSimulator"]' "$PLIST"
    else
        run plutil -replace CFBundleSupportedPlatforms -json '["iPhoneOS"]' "$PLIST"
    fi
    # The build facts Xcode records in every app it builds, which a bundle made without Xcode lacks; installers and sideloading tools may look for them.
    ask SDK_VERSION "0.0" xcrun --sdk "$SDK" --show-sdk-version
    ask SDK_BUILD "0A0" xcrun --sdk "$SDK" --show-sdk-build-version
    run plutil -replace DTPlatformName -string "$SDK" "$PLIST"
    run plutil -replace DTPlatformVersion -string "$SDK_VERSION" "$PLIST"
    run plutil -replace DTSDKName -string "$SDK$SDK_VERSION" "$PLIST"
    run plutil -replace DTSDKBuild -string "$SDK_BUILD" "$PLIST"
fi
run plutil -lint "$PLIST"

run mkdir -p "$RES/engine/assets/shaders"
run cp "$SHADERS"/*.spv "$RES/engine/assets/shaders/"

# The licences a bundle carries, and a note of which file is which. The repository's LICENSE and NOTICE.md are the game's licence and the notices of what it contains (NOTICE.md
# reproduces the text of each component's licence). The engine's own list names every library it vendors; MoltenVK is not among them (the engine links it only when a build
# names one) and is Apache-2.0, so its text travels when the workflow hands it over.
run mkdir -p "$RES/licenses"
run cp "$REPO/LICENSE" "$RES/licenses/Magic-Portals-Enhanced-LICENSE.txt"
run cp "$REPO/NOTICE.md" "$RES/licenses/NOTICE.md"
run cp "$REPO/engine/LICENSE" "$RES/licenses/Supersonic-Engine-LICENSE.txt"
run cp "$REPO/engine/THIRD_PARTY_LICENSES.md" "$RES/licenses/Supersonic-Engine-THIRD_PARTY_LICENSES.md"
if [ -n "$MVK_LICENSE" ]; then run cp "$MVK_LICENSE" "$RES/licenses/MoltenVK-LICENSE.txt"; fi
if [ "$DRY" = 1 ]; then
    say write "$RES/licenses/README.txt"
else
    cat >"$RES/licenses/README.txt" <<'EOF'
What this folder holds.

Magic-Portals-Enhanced-LICENSE.txt and NOTICE.md: this game's licence, and the notices of the software it contains (the repository's LICENSE and NOTICE.md).

Supersonic-Engine-LICENSE.txt and Supersonic-Engine-THIRD_PARTY_LICENSES.md: the engine's licence, and the engine's own list of the libraries it vendors.

MoltenVK-LICENSE.txt, when it was given: MoltenVK's licence text. MoltenVK is linked into this program.
EOF
fi

# The icon: a placeholder drawn by arithmetic (neither tool reads a file). Not fatal: an app without an icon still runs.
if [ "$PLATFORM" = macos ]; then
    ICON_TOOL="$REPO/tools/art/make_app_icon.py"
else
    ICON_TOOL="$REPO/tools/apple/ios_icons.py"
fi
if run python3 "$ICON_TOOL" --out "$WORK/icons"; then
    if [ "$PLATFORM" = macos ]; then
        SET="$WORK/MagicPortals.iconset"
        run mkdir -p "$SET"
        # The tool draws each size itself (16 32 64 128 256 512 1024), so the iconset is a copy of the right size under each name, not a scaling.
        run cp "$WORK/icons/icon_16.png" "$SET/icon_16x16.png"
        run cp "$WORK/icons/icon_32.png" "$SET/icon_16x16@2x.png"
        run cp "$WORK/icons/icon_32.png" "$SET/icon_32x32.png"
        run cp "$WORK/icons/icon_64.png" "$SET/icon_32x32@2x.png"
        run cp "$WORK/icons/icon_128.png" "$SET/icon_128x128.png"
        run cp "$WORK/icons/icon_256.png" "$SET/icon_128x128@2x.png"
        run cp "$WORK/icons/icon_256.png" "$SET/icon_256x256.png"
        run cp "$WORK/icons/icon_512.png" "$SET/icon_256x256@2x.png"
        run cp "$WORK/icons/icon_512.png" "$SET/icon_512x512.png"
        run cp "$WORK/icons/icon_1024.png" "$SET/icon_512x512@2x.png"
        run iconutil -c icns "$SET" -o "$RES/MagicPortals.icns" || echo "warning: iconutil failed; no icon" >&2
    else
        # CFBundleIconFiles in game/ios/Info.plist.in: 60 pt (iPhone), 76 and 83.5 pt (iPad). tools/apple/ios_icons.py draws these OPAQUE (iOS fills
        # what is transparent and the App Store refuses an alpha channel), each at its own size.
        run cp "$WORK/icons/AppIcon60x60@2x.png" "$RES/AppIcon60x60@2x.png"
        run cp "$WORK/icons/AppIcon60x60@3x.png" "$RES/AppIcon60x60@3x.png"
        run cp "$WORK/icons/AppIcon76x76@2x~ipad.png" "$RES/AppIcon76x76@2x~ipad.png"
        run cp "$WORK/icons/AppIcon83.5x83.5@2x~ipad.png" "$RES/AppIcon83.5x83.5@2x~ipad.png"
    fi
else
    echo "warning: the icon could not be drawn; the app has none" >&2
fi

# Signed LAST, after the last file is in: anything added to the bundle later breaks the seal.
run codesign --force --sign - --timestamp=none "$APP"
note "Assembled $APP, minimum OS ${MINOS:-unknown}."

# ---- the package ---------------------------------------------------------------------------------------------------------------------------------------
write_read_me_first() {
    if [ "$DRY" = 1 ]; then
        say write "$1"
        return
    fi
    cat >"$1" <<'EOF'
READ ME FIRST

This is the Magic Portals program WITHOUT the game: no levels, no pictures, no sound. They are not part of this download.
It is a build for testing that the program starts and draws. It will say so, and stop, if it is started as it is.

It is not signed by Apple and not notarised, and nobody has run it on a Mac outside a build machine's test. Whether your Mac opens it is not known.
See docs/apple.md of the project for what is and is not known.
EOF
}

if [ "$PLATFORM" = macos ]; then
    STAGE="$WORK/stage/MagicPortals"
    ZIP="$OUT_ABS/MagicPortals-macOS-shell.zip"
    run mkdir -p "$STAGE"
    run cp -R "$APP" "$STAGE/MagicPortals.app"
    # Beside the app, never inside it: anything added to the bundle after the signature breaks the seal.
    write_read_me_first "$STAGE/READ ME FIRST.txt"
    run rm -f "$ZIP"
    # ditto, Apple's own way to zip a signed bundle: it keeps the executable bit and the bundle's seal, which a plain zip writer loses ("is damaged and
    # can't be opened" on the player's Mac). No AppleDouble files, extended attributes or quarantine flags go in.
    run ditto -c -k --keepParent --norsrc --noextattr --noqtn --noacl "$STAGE" "$ZIP"

    ask LISTING "MagicPortals/" unzip -Z1 "$ZIP"
    if printf '%s\n' "$LISTING" | grep -E '(^|/)__MACOSX/|(^|/)\._'; then
        fail "$ZIP holds AppleDouble files"
    fi
    # Then the zip is unpacked again and the signature verified strictly, as a player's Finder would unpack it.
    run mkdir -p "$WORK/unpacked"
    run ditto -x -k "$ZIP" "$WORK/unpacked"
    CHECKED="$WORK/unpacked/MagicPortals/MagicPortals.app"
    if [ "$DRY" != 1 ]; then
        test -x "$CHECKED/Contents/MacOS/MagicPortals" || fail "the unpacked app has no executable"
        test -f "$WORK/unpacked/MagicPortals/READ ME FIRST.txt" || fail "the unpacked folder has no READ ME FIRST.txt"
    fi
    run codesign --verify --deep --strict --verbose=2 "$CHECKED"
    ask ARCHS "arm64 x86_64" lipo -archs "$CHECKED/Contents/MacOS/MagicPortals"
    note "architectures: $ARCHS"
    ask INFO "CFBundleIdentifier => dry run" plutil -p "$CHECKED/Contents/Info.plist"
    printf '%s\n' "$INFO" | grep -E 'CFBundle(ShortVersionString|Version|Identifier)|LSMinimumSystemVersion' || true
    RESULT="$ZIP"
elif [ "$SDK" = iphonesimulator ]; then
    ZIP="$OUT_ABS/MagicPortals-iOS-simulator-shell.zip"
    run rm -f "$ZIP"
    run ditto -c -k --keepParent --norsrc --noextattr --noqtn --noacl "$APP" "$ZIP"
    ask LISTING "MagicPortals.app/" unzip -Z1 "$ZIP"
    if printf '%s\n' "$LISTING" | grep -E '(^|/)__MACOSX/|(^|/)\._'; then
        fail "$ZIP holds AppleDouble files"
    fi
    run codesign --verify --strict --verbose=2 "$APP"
    ask ARCHS "arm64" lipo -archs "$APP/MagicPortals"
    note "architectures: $ARCHS"
    RESULT="$ZIP"
else
    # The device build (iphoneos, arm64) as Payload/MagicPortals.app zipped: an .ipa with no signature but the ad-hoc one, for a sideloading tool
    # (Sideloadly, AltStore, SideStore) to sign with the player's own Apple ID. It refuses one that links anything outside the system or is encrypted,
    # which no such tool could install.
    PAYLOAD="$WORK/ipa/Payload"
    IPA="$OUT_ABS/MagicPortals-iOS-shell.ipa"
    run mkdir -p "$PAYLOAD"
    run cp -R "$APP" "$PAYLOAD/MagicPortals.app"
    CHECKED="$PAYLOAD/MagicPortals.app"
    ask ARCHS "arm64" lipo -archs "$CHECKED/MagicPortals"
    note "architectures: $ARCHS"
    # Only the system's libraries and frameworks: a sideloading tool signs what is in the bundle, and the program links MoltenVK statically, so nothing
    # else may be named.
    ask LINKED "	/System/Library/Frameworks/Metal.framework/Metal (dry run)" otool -L "$CHECKED/MagicPortals"
    if printf '%s\n' "$LINKED" | tail -n +2 | grep -v -E '^\s+(/System/Library/|/usr/lib/)'; then
        fail "MagicPortals links something outside the system"
    fi
    ask OTOOL_ENC "cryptid 0" otool -l "$CHECKED/MagicPortals"
    if printf '%s\n' "$OTOOL_ENC" | grep -A4 LC_ENCRYPTION_INFO | grep -q 'cryptid 1'; then
        fail "MagicPortals is encrypted"
    fi
    run codesign --verify --strict --verbose=2 "$CHECKED"
    ask INFO "CFBundleIdentifier => dry run" plutil -p "$CHECKED/Info.plist"
    printf '%s\n' "$INFO" | grep -E 'CFBundle(ShortVersionString|Version|Identifier)|MinimumOSVersion|DTPlatformName' || true

    run rm -f "$IPA"
    if [ "$DRY" = 1 ]; then
        say cd "$WORK/ipa"
        say zip -q -r -y -X "$IPA" Payload
    else
        (cd "$WORK/ipa" && zip -q -r -y -X "$IPA" Payload)
    fi
    ask LISTING "Payload/" unzip -Z1 "$IPA"
    if [ "$(printf '%s\n' "$LISTING" | head -1)" != "Payload/" ]; then
        fail "$IPA does not start with Payload/"
    fi
    if printf '%s\n' "$LISTING" | grep -E '(^|/)__MACOSX/|(^|/)\._'; then
        fail "$IPA holds AppleDouble files"
    fi
    RESULT="$IPA"
fi

run rm -rf "$WORK"
note "Wrote $RESULT."
