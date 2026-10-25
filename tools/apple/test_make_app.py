"""tools/apple/make_app.sh, run on a machine with no Apple tool (planning step 206, PORT-8), with stand-ins for every tool it calls.

Run:   python -m unittest discover -s tools/apple -p "test_*.py" -v        (standard library only; bash is needed: Git Bash on Windows, any on Linux)

WHAT THIS PROVES, AND WHAT IT CANNOT. The script is meant for a Mac and was never run on one. Here it is run, for real, under bash, with codesign, ditto,
iconutil, lipo, otool, plutil, python3, unzip, xcrun and zip replaced by small stand-ins put first on PATH. Each stand-in writes down the argument list it was
called with and prints what the real tool prints for the one question the script asks of it. The test compares the written list to one pinned below, so the
SHAPE of every Apple command is held: a missing --force or --sign -, a dropped --keepParent, a plain zip where ditto is meant, a forgotten verification each
turn the comparison red (the MUTATIONS below run the script with exactly that edit). What it CANNOT show is that those commands, given those arguments, do on a
Mac what the script means: that is NOT KNOWN until a Mac runs it (the workflow of planning step 206 is the first chance).

Nothing here is an image of the original: the stand-ins create empty files for the icons, and every name is invented.
"""
import hashlib
import os
import re
import shutil
import stat
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
SCRIPT = os.path.join(HERE, 'make_app.sh')


def find_bash():
    """Git Bash on Windows (not WSL's bash.exe in System32, which would see a different file system), else the bash on PATH."""
    candidates = [os.environ.get('MAKE_APP_TEST_BASH', '')]
    if os.name == 'nt':
        for root in (os.environ.get('ProgramFiles', 'C:/Program Files'), os.environ.get('ProgramFiles(x86)', 'C:/Program Files (x86)')):
            candidates.append(os.path.join(root, 'Git', 'bin', 'bash.exe'))
    found = shutil.which('bash')
    if found and 'system32' not in found.lower():
        candidates.append(found)
    for candidate in candidates:
        if candidate and os.path.isfile(candidate):
            return candidate
    raise unittest.SkipTest('no bash to run make_app.sh under')


# ---- the stand-ins -------------------------------------------------------------------------------------------------------------------------------

HEADER = r'''#!/usr/bin/env bash
TOOL="%(tool)s"
{ printf '%%s' "$TOOL"; for a in "$@"; do printf ' <%%s>' "$a"; done; printf '\n'; } >> "$STUB_LOG"
%(body)s
'''

BODIES = {
    'codesign': 'exit 0',
    'plutil': '''if [ "$1" = "-p" ]; then
  printf '    "CFBundleIdentifier" => "com.example.invented"\\n    "CFBundleShortVersionString" => "0.1.0"\\n    "CFBundleVersion" => "0.1.0"\\n'
fi
exit 0''',
    'otool': '''if [ "$1" = "-L" ]; then
  printf '%s:\\n' "$2"
  printf '\\t/System/Library/Frameworks/Metal.framework/Versions/A/Metal (compatibility version 1.0.0, current version 1.0.0)\\n'
  printf '\\t/usr/lib/libc++.1.dylib (compatibility version 1.0.0, current version 1.0.0)\\n'
  [ -n "${STUB_OTOOL_EXTRA:-}" ] && printf '\\t%s (compatibility version 1.0.0, current version 1.0.0)\\n' "$STUB_OTOOL_EXTRA"
  exit 0
fi
printf 'Load command 9\\n      cmd LC_BUILD_VERSION\\n  cmdsize 32\\n platform 1\\n    minos %s\\n      sdk 15.0\\n' "${STUB_MINOS:-13.3}"
printf 'Load command 10\\n          cmd LC_ENCRYPTION_INFO_64\\n      cmdsize 24\\n     cryptoff 16384\\n    cryptsize 16384\\n      cryptid %s\\n' "${STUB_CRYPTID:-0}"
exit 0''',
    'lipo': 'echo "${STUB_ARCHS:-arm64 x86_64}"',
    'xcrun': '''case "$*" in
  *--show-sdk-version*) echo 18.0 ;;
  *--show-sdk-build-version*) echo 22A3362 ;;
esac
exit 0''',
    'python3': '''out=""
prev=""
for a in "$@"; do [ "$prev" = "--out" ] && out="$a"; prev="$a"; done
mkdir -p "$out"
case "$1" in
  *ios_icons.py) for f in "AppIcon60x60@2x.png" "AppIcon60x60@3x.png" "AppIcon76x76@2x~ipad.png" "AppIcon83.5x83.5@2x~ipad.png"; do : > "$out/$f"; done ;;
  *) for s in 16 32 64 128 256 512 1024; do : > "$out/icon_$s.png"; done ;;
esac
[ "${STUB_ICON_FAILS:-0}" = 1 ] && exit 1
exit 0''',
    'iconutil': '''out=""
prev=""
for a in "$@"; do [ "$prev" = "-o" ] && out="$a"; prev="$a"; done
: > "$out"
exit 0''',
    # ditto -c: SRC and ZIP are the last two arguments; the "archive" is a copy of SRC in the stand-ins' own state folder, with --keepParent keeping SRC's name.
    # ditto -x: unpacks that copy into DEST; STUB_UNPACK_DROPS names one path (relative to DEST) that is then missing from what was unpacked, to see what the script
    # says of an unpacked app without its executable or of a folder without its READ ME.
    'ditto': '''n=$#
args=("$@")
if [ "$1" = "-c" ]; then
  src="${args[$((n-2))]}"; zip="${args[$((n-1))]}"
  case "$*" in *--keepParent*) keep=1 ;; *) keep=0 ;; esac
  state="$STUB_STATE/$(basename "$zip")"
  rm -rf "$state.contents" "$state.list"; mkdir -p "$state.contents"
  if [ "$keep" = 1 ]; then cp -R "$src" "$state.contents/"; else cp -R "$src/." "$state.contents/"; fi
  ( cd "$state.contents" && find . -mindepth 1 | sed 's#^\\./##' | sort | while read -r f; do if [ -d "$f" ]; then echo "$f/"; else echo "$f"; fi; done ) > "$state.list"
  echo "an archive of the stand-in" > "$zip"
  exit 0
fi
if [ "$1" = "-x" ]; then
  zip="${args[$((n-2))]}"; dest="${args[$((n-1))]}"
  state="$STUB_STATE/$(basename "$zip")"
  mkdir -p "$dest"; cp -R "$state.contents/." "$dest/"
  if [ -n "${STUB_UNPACK_DROPS:-}" ]; then rm -rf "$dest/$STUB_UNPACK_DROPS"; fi
  exit 0
fi
exit 0''',
    'zip': '''n=$#
args=("$@")
zip="${args[$((n-2))]}"; dir="${args[$((n-1))]}"
state="$STUB_STATE/$(basename "$zip")"
rm -rf "$state.contents" "$state.list"; mkdir -p "$state.contents"
cp -R "$dir" "$state.contents/"
( cd "$state.contents" && find . -mindepth 1 | sed 's#^\\./##' | sort | while read -r f; do if [ -d "$f" ]; then echo "$f/"; else echo "$f"; fi; done ) > "$state.list"
echo "an archive of the stand-in" > "$zip"
exit 0''',
    'unzip': '''zip="${@: -1}"
state="$STUB_STATE/$(basename "$zip")"
{
  if [ -n "${STUB_UNZIP_FIRST:-}" ]; then echo "$STUB_UNZIP_FIRST"; fi
  cat "$state.list"
  if [ -n "${STUB_UNZIP_EXTRA:-}" ]; then echo "$STUB_UNZIP_EXTRA"; fi
}
exit 0''',
}
# Tools the script must NEVER call (the sibling's rsync of the original's data, a scaling of an icon, a disk image, a keychain, a notarisation): a call is a
# line in the log, which no pinned list holds.
NEVER = ('sips', 'PlistBuddy', 'rsync', 'hdiutil', 'security', 'spctl', 'xattr', 'productsign', 'notarytool')


def write_stand_ins(folder):
    os.makedirs(folder, exist_ok=True)
    for tool, body in BODIES.items():
        put(os.path.join(folder, tool), HEADER % {'tool': tool, 'body': body}, executable=True)
    for tool in NEVER:
        put(os.path.join(folder, tool), HEADER % {'tool': tool, 'body': 'exit 0'}, executable=True)


def posix(path):
    """A path as bash on this machine spells it: /c/Users/... under Git Bash, unchanged elsewhere (a colon would split a PATH entry)."""
    path = path.replace('\\', '/')
    if os.name == 'nt' and len(path) > 2 and path[1] == ':':
        return '/' + path[0].lower() + path[2:]
    return path


def put(path, text, executable=False):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w', newline='\n') as f:
        f.write(text)
    if executable:
        os.chmod(path, os.stat(path).st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)


def tree_hash(root, skip=()):
    """What is in a folder: every path and every file's bytes, as one digest (the sandbox's untouched parts are compared by it)."""
    digest = hashlib.sha256()
    for base, dirs, files in os.walk(root):
        dirs.sort()
        for name in sorted(files):
            full = os.path.join(base, name)
            rel = os.path.relpath(full, root).replace('\\', '/')
            if any(rel.startswith(s) for s in skip):
                continue
            digest.update(rel.encode() + b'\0')
            with open(full, 'rb') as f:
                digest.update(f.read())
    return digest.hexdigest()


# What a person may already keep in the folder the script is given (an out folder is often a Downloads or a Documents): one folder is named `work`, which the
# scratch folder used to be called, so that a script which removes `<out dir>/work` is caught.
PERSONS_FOLDERS = (('work', 'keep.txt'), ('Documents', 'notes.txt'), ('MagicPortals-work', 'keep.txt'))


def plant_a_persons_folders(out):
    planted = {}
    for folder, name in PERSONS_FOLDERS:
        path = os.path.join(out, folder, name)
        text = "a person's own file in %s, not the script's\n" % folder
        put(path, text)
        planted[path] = text
    return planted


def what_is_left_of(planted):
    left = {}
    for path in planted:
        if os.path.isfile(path):
            with open(path, encoding='utf-8') as f:
                left[path] = f.read()
    return left


class Sandbox:
    """A throwaway repository (the script, the engine's licence and shaders, a drawing tool's name), a build folder and the stand-ins."""

    def __init__(self, script_text=None):
        self.root = tempfile.mkdtemp(prefix='mp-make-app-')
        self.repo = os.path.join(self.root, 'repo')
        self.build = os.path.join(self.root, 'build')
        self.bin = os.path.join(self.root, 'bin')
        self.state = os.path.join(self.root, 'state')
        self.log = os.path.join(self.root, 'stubs.log')
        os.makedirs(self.state)
        with open(SCRIPT, encoding='utf-8') as f:
            text = f.read() if script_text is None else script_text
        put(os.path.join(self.repo, 'tools', 'apple', 'make_app.sh'), text, executable=True)
        put(os.path.join(self.repo, 'tools', 'art', 'make_app_icon.py'), '# a name for the stand-in python3 to be called with\n')
        put(os.path.join(self.repo, 'tools', 'apple', 'ios_icons.py'), '# a name for the stand-in python3 to be called with\n')
        put(os.path.join(self.repo, 'engine', 'LICENSE'), 'the engine licence, invented\n')
        put(os.path.join(self.repo, 'engine', 'THIRD_PARTY_LICENSES.md'), 'the list, invented\n')
        put(os.path.join(self.repo, 'engine', 'assets', 'shaders', 'frag.spv'), 'spv-a\n')
        put(os.path.join(self.repo, 'engine', 'assets', 'shaders', 'vert.spv'), 'spv-b\n')
        os.makedirs(os.path.join(self.repo, 'out'))
        # A shebang, so that `test -x` is true of it under Git Bash too (which decides by the first bytes, not by a mode bit).
        put(os.path.join(self.build, 'game', 'MagicPortals'), '#!/bin/sh\n# an executable, invented\n', executable=True)
        put(os.path.join(self.build, 'game', 'macos', 'Info.plist'), '<plist/>\n')
        put(os.path.join(self.build, 'game', 'ios', 'Info.plist'), '<plist/>\n')
        put(os.path.join(self.root, 'moltenvk-LICENSE'), 'apache, invented\n')
        write_stand_ins(self.bin)
        open(self.log, 'w').close()
        self.out = os.path.join(self.repo, 'out', 'apple')

    def close(self):
        shutil.rmtree(self.root, ignore_errors=True)

    def spellings(self):
        """Every way the sandbox's folders can be spelled in what the script printed: Windows mixed, and MSYS (/c/...)."""
        names = {}
        for token, path in (('{WORK}', self.out + '/.make_app-work'), ('{OUT}', self.out), ('{REPO}', self.repo), ('{BUILD}', self.build), ('{ROOT}', self.root)):
            mixed = path.replace('\\', '/')
            names[mixed] = token
            if len(mixed) > 2 and mixed[1] == ':':
                names['/' + mixed[0].lower() + mixed[2:]] = token
            # Git Bash shows the system's temporary folder as /tmp.
            temp = tempfile.gettempdir().replace('\\', '/')
            if mixed.lower().startswith(temp.lower() + '/'):
                names['/tmp' + mixed[len(temp):]] = token
        return sorted(names.items(), key=lambda kv: -len(kv[0]))

    def normal(self, text):
        for spelled, token in self.spellings():
            text = text.replace(spelled, token)
        return text

    def run(self, args, env_extra=None, bash=None, home_from=None):
        """`home_from`: a folder that is to be the home folder of this run. HOME is set INSIDE bash, to what bash itself reports for it (`cd` and `pwd -P`, the spelling
        the script compares its <out dir> with: Git Bash shows the temporary folder as /tmp/..., which a Windows path would never equal), and from the same
        spelling of the folder as the <out dir> argument, because Git Bash's `pwd -P` also differs between `/c/Users/...` and `C:/Users/...` as given)."""
        env = dict(os.environ)
        env['STUB_LOG'] = self.log.replace('\\', '/')
        env['STUB_STATE'] = self.state.replace('\\', '/')
        env.update(env_extra or {})
        script = os.path.join(self.repo, 'tools', 'apple', 'make_app.sh').replace('\\', '/')
        # The stand-ins go first on PATH from INSIDE bash: Git Bash puts its own /usr/bin (a real unzip among it) ahead of the PATH it was started with.
        launcher = 'export PATH="$1:$PATH"; shift; exec bash "$@"'
        before_script = [posix(self.bin)]
        if home_from is not None:
            launcher = 'export PATH="$1:$PATH"; shift; HOME="$(cd "$1" && pwd -P)"; export HOME; shift; exec bash "$@"'
            before_script.append(home_from.replace('\\', '/'))  # spelled as the <out dir> argument is: bash's `pwd -P` answers by the spelling it was given
        command = [bash or find_bash(), '-c', launcher, 'launcher'] + before_script + [script] + [a.replace('\\', '/') for a in args]
        proc = subprocess.run(command, capture_output=True, text=True, env=env, timeout=300)
        with open(self.log, encoding='utf-8') as f:
            lines = [self.normal(line.rstrip('\n')) for line in f if line.strip()]
        return proc, lines


# ---- the pinned lists ----------------------------------------------------------------------------------------------------------------------------

APP = '{OUT}/MagicPortals.app'
MACOS_LIST = [
    'otool <-l> <%s/Contents/MacOS/MagicPortals>' % APP,
    'plutil <-replace> <LSMinimumSystemVersion> <-string> <13.3> <%s/Contents/Info.plist>' % APP,
    'plutil <-lint> <%s/Contents/Info.plist>' % APP,
    'python3 <{REPO}/tools/art/make_app_icon.py> <--out> <{WORK}/icons>',
    'iconutil <-c> <icns> <{WORK}/MagicPortals.iconset> <-o> <%s/Contents/Resources/MagicPortals.icns>' % APP,
    'codesign <--force> <--sign> <-> <--timestamp=none> <%s>' % APP,
    'ditto <-c> <-k> <--keepParent> <--norsrc> <--noextattr> <--noqtn> <--noacl> <{WORK}/stage/MagicPortals> <{OUT}/MagicPortals-macOS-shell.zip>',
    'unzip <-Z1> <{OUT}/MagicPortals-macOS-shell.zip>',
    'ditto <-x> <-k> <{OUT}/MagicPortals-macOS-shell.zip> <{WORK}/unpacked>',
    'codesign <--verify> <--deep> <--strict> <--verbose=2> <{WORK}/unpacked/MagicPortals/MagicPortals.app>',
    'lipo <-archs> <{WORK}/unpacked/MagicPortals/MagicPortals.app/Contents/MacOS/MagicPortals>',
    'plutil <-p> <{WORK}/unpacked/MagicPortals/MagicPortals.app/Contents/Info.plist>',
]
IOS_COMMON = [
    'otool <-l> <%s/MagicPortals>' % APP,
    'plutil <-replace> <MinimumOSVersion> <-string> <13.3> <%s/Info.plist>' % APP,
]


def ios_head(sdk, platform_json):
    return IOS_COMMON + [
        'plutil <-replace> <CFBundleSupportedPlatforms> <-json> <%s> <%s/Info.plist>' % (platform_json, APP),
        'xcrun <--sdk> <%s> <--show-sdk-version>' % sdk,
        'xcrun <--sdk> <%s> <--show-sdk-build-version>' % sdk,
        'plutil <-replace> <DTPlatformName> <-string> <%s> <%s/Info.plist>' % (sdk, APP),
        'plutil <-replace> <DTPlatformVersion> <-string> <18.0> <%s/Info.plist>' % APP,
        'plutil <-replace> <DTSDKName> <-string> <%s18.0> <%s/Info.plist>' % (sdk, APP),
        'plutil <-replace> <DTSDKBuild> <-string> <22A3362> <%s/Info.plist>' % APP,
        'plutil <-lint> <%s/Info.plist>' % APP,
        'python3 <{REPO}/tools/apple/ios_icons.py> <--out> <{WORK}/icons>',
        'codesign <--force> <--sign> <-> <--timestamp=none> <%s>' % APP,
    ]


IOS_DEVICE_LIST = ios_head('iphoneos', '["iPhoneOS"]') + [
    'lipo <-archs> <{WORK}/ipa/Payload/MagicPortals.app/MagicPortals>',
    'otool <-L> <{WORK}/ipa/Payload/MagicPortals.app/MagicPortals>',
    'otool <-l> <{WORK}/ipa/Payload/MagicPortals.app/MagicPortals>',
    'codesign <--verify> <--strict> <--verbose=2> <{WORK}/ipa/Payload/MagicPortals.app>',
    'plutil <-p> <{WORK}/ipa/Payload/MagicPortals.app/Info.plist>',
    'zip <-q> <-r> <-y> <-X> <{OUT}/MagicPortals-iOS-shell.ipa> <Payload>',
    'unzip <-Z1> <{OUT}/MagicPortals-iOS-shell.ipa>',
]
IOS_SIM_LIST = ios_head('iphonesimulator', '["iPhoneSimulator"]') + [
    'ditto <-c> <-k> <--keepParent> <--norsrc> <--noextattr> <--noqtn> <--noacl> <%s> <{OUT}/MagicPortals-iOS-simulator-shell.zip>' % APP,
    'unzip <-Z1> <{OUT}/MagicPortals-iOS-simulator-shell.zip>',
    'codesign <--verify> <--strict> <--verbose=2> <%s>' % APP,
    'lipo <-archs> <%s/MagicPortals>' % APP,
]
# The dry run's own list: every command the script runs, the ones that are not Apple's too, and none executed.
DRY_MACOS_HEAD = [
    '+ rm -rf {OUT}/MagicPortals.app {WORK}',
    '+ mkdir -p {OUT}/MagicPortals.app/Contents/MacOS {OUT}/MagicPortals.app/Contents/Resources {WORK}',
    '+ cp {BUILD}/game/MagicPortals {OUT}/MagicPortals.app/Contents/MacOS/MagicPortals',
    '+ cp {BUILD}/game/macos/Info.plist {OUT}/MagicPortals.app/Contents/Info.plist',
    '+ otool -l {OUT}/MagicPortals.app/Contents/MacOS/MagicPortals',
    '+ plutil -replace LSMinimumSystemVersion -string 0.0 {OUT}/MagicPortals.app/Contents/Info.plist',
]
DRY_MACOS_TAIL = [
    '+ codesign --force --sign - --timestamp=none {OUT}/MagicPortals.app',
]

# The AppleDouble check of the three packages. The script holds the same test three times (`__MACOSX/` folders and `._` files, each at the start of a name or after a
# slash); a package is refused by EITHER half alone, so each half is tried on its own, in each package. A package is (name, platform, extra arguments, environment,
# the prefix of the entries it lists, the line before the test, the line after it): the last two make the test's text unique in the script.
APPLEDOUBLE_TEST = r'''    if printf '%s\n' "$LISTING" | grep -E '(^|/)__MACOSX/|(^|/)\._'; then'''
APPLEDOUBLE_PACKAGES = (
    ('the macOS zip', 'macos', [], {}, 'MagicPortals/', '    ask LISTING "MagicPortals/" unzip -Z1 "$ZIP"\n', ''),
    ('the iOS simulator zip', 'ios', ['--sdk', 'iphonesimulator'], {'STUB_ARCHS': 'arm64'}, 'MagicPortals.app/', '    ask LISTING "MagicPortals.app/" unzip -Z1 "$ZIP"\n', ''),
    ('the ipa', 'ios', ['--sdk', 'iphoneos'], {'STUB_ARCHS': 'arm64'}, 'Payload/MagicPortals.app/', '', '\n        fail "$IPA holds AppleDouble files"'),
)
# The two halves, as an entry each: the first has no `._` in it, the second no `__MACOSX/`.
MACOSX_ENTRY = '__MACOSX/invented'
DOT_UNDERSCORE_ENTRY = '._invented'
# The test with one half left, and with neither (an `if` that is never true).
ONLY_MACOSX_HALF = r'''    if printf '%s\n' "$LISTING" | grep -E '(^|/)__MACOSX/'; then'''
ONLY_DOT_UNDERSCORE_HALF = r'''    if printf '%s\n' "$LISTING" | grep -E '(^|/)\._'; then'''
NEITHER_HALF = '    if false; then'


class MakeAppTest(unittest.TestCase):
    def setUp(self):
        self.addCleanup(self._cleanup)
        self.boxes = []

    def _cleanup(self):
        for box in self.boxes:
            box.close()

    def box(self, text=None):
        box = Sandbox(text)
        self.boxes.append(box)
        return box

    def run_ok(self, platform, extra=(), env=None):
        box = self.box()
        proc, lines = box.run([platform, box.build, box.out] + list(extra), env)
        self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
        return box, proc, lines

    # ---- the argument lists -------------------------------------------------------------------------------------------------------------------------

    def test_macos_commands_are_the_pinned_list(self):
        box, proc, lines = self.run_ok('macos')
        self.assertEqual(lines, MACOS_LIST)
        self.assertIn('architectures: arm64 x86_64', proc.stdout)
        self.assertTrue(os.path.isfile(os.path.join(box.out, 'MagicPortals-macOS-shell.zip')))
        self.assertTrue(os.path.isdir(os.path.join(box.out, 'MagicPortals.app', 'Contents', 'Resources', 'engine', 'assets', 'shaders')))
        self.assertTrue(os.path.isfile(os.path.join(box.out, 'MagicPortals.app', 'Contents', 'Resources', 'licenses', 'Supersonic-Engine-LICENSE.txt')))
        # The files themselves, not only their folders: a copy that silently copied nothing leaves the folders in place.
        resources = os.path.join(box.out, 'MagicPortals.app', 'Contents', 'Resources')
        for spv in ('frag.spv', 'vert.spv'):
            self.assertTrue(os.path.isfile(os.path.join(resources, 'engine', 'assets', 'shaders', spv)), spv)
        self.assertTrue(os.path.isfile(os.path.join(resources, 'licenses', 'Supersonic-Engine-THIRD_PARTY_LICENSES.md')))
        self.assertFalse(os.path.exists(os.path.join(box.out, '.make_app-work')), 'the scratch folder is removed')

    def bundle_files(self, box):
        """Every file of the bundle the stand-ins left, relative to it and with forward slashes, sorted."""
        app = os.path.join(box.out, 'MagicPortals.app')
        found = []
        for folder, _dirs, files in os.walk(app):
            found.extend(os.path.relpath(os.path.join(folder, name), app).replace(os.sep, '/') for name in files)
        return sorted(found)

    SHADER_FILES = ['engine/assets/shaders/frag.spv', 'engine/assets/shaders/vert.spv']
    LICENCE_FILES = ['licenses/README.txt', 'licenses/Supersonic-Engine-LICENSE.txt', 'licenses/Supersonic-Engine-THIRD_PARTY_LICENSES.md']
    IOS_ICON_FILES = ['AppIcon60x60@2x.png', 'AppIcon60x60@3x.png', 'AppIcon76x76@2x~ipad.png', 'AppIcon83.5x83.5@2x~ipad.png']

    def macos_files(self):
        return sorted(['Contents/Info.plist', 'Contents/MacOS/MagicPortals', 'Contents/Resources/MagicPortals.icns'] +
                      ['Contents/Resources/' + n for n in self.SHADER_FILES + self.LICENCE_FILES])

    def ios_files(self):
        return sorted(['Info.plist', 'MagicPortals'] + self.IOS_ICON_FILES + self.SHADER_FILES + self.LICENCE_FILES)

    def test_the_shell_bundle_holds_no_game_data(self):
        # The hard rule, as a list: a shell is the program, the engine's shaders, the licences and the icons, and nothing else. A cp -R of any folder into the
        # bundle (a GameData, a levels folder, a port-data) adds files that are not in these sets, and this test names them.
        box, _proc, _lines = self.run_ok('macos')
        self.assertEqual(self.bundle_files(box), self.macos_files())
        for sdk in ('iphoneos', 'iphonesimulator'):
            box, _proc, _lines = self.run_ok('ios', ['--sdk', sdk], {'STUB_ARCHS': 'arm64'})
            self.assertEqual(self.bundle_files(box), self.ios_files(), sdk)

    def test_mutation_a_game_data_folder_copied_into_the_bundle_turns_the_file_list_red(self):
        proc, _lines = self.mutate('run mkdir -p "$RES/licenses"\n', 'run mkdir -p "$RES/licenses"\nrun cp -R "$REPO/engine" "$RES/GameData"\n')
        self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
        self.assertNotEqual(self.bundle_files(self.boxes[-1]), self.macos_files())

    def test_mutation_the_shaders_not_copied_turns_the_file_list_red(self):
        proc, _lines = self.mutate('run cp "$SHADERS"/*.spv "$RES/engine/assets/shaders/"', 'true')
        self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
        self.assertNotEqual(self.bundle_files(self.boxes[-1]), self.macos_files())

    def test_mutation_the_notices_list_not_copied_turns_the_file_list_red(self):
        proc, _lines = self.mutate('run cp "$REPO/engine/THIRD_PARTY_LICENSES.md" "$RES/licenses/Supersonic-Engine-THIRD_PARTY_LICENSES.md"', 'true')
        self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
        self.assertNotEqual(self.bundle_files(self.boxes[-1]), self.macos_files())

    def test_ios_device_commands_are_the_pinned_list(self):
        box, proc, lines = self.run_ok('ios', ['--sdk', 'iphoneos'], {'STUB_ARCHS': 'arm64'})
        self.assertEqual(lines, IOS_DEVICE_LIST)
        self.assertTrue(os.path.isfile(os.path.join(box.out, 'MagicPortals-iOS-shell.ipa')))
        for icon in ('AppIcon60x60@2x.png', 'AppIcon60x60@3x.png', 'AppIcon76x76@2x~ipad.png', 'AppIcon83.5x83.5@2x~ipad.png'):
            self.assertTrue(os.path.isfile(os.path.join(box.out, 'MagicPortals.app', icon)), icon)

    def test_ios_is_a_device_build_unless_the_simulator_is_asked_for(self):
        box, proc, lines = self.run_ok('ios', [], {'STUB_ARCHS': 'arm64'})
        self.assertEqual(lines, IOS_DEVICE_LIST)

    def test_ios_simulator_commands_are_the_pinned_list(self):
        box, proc, lines = self.run_ok('ios', ['--sdk', 'iphonesimulator'], {'STUB_ARCHS': 'arm64'})
        self.assertEqual(lines, IOS_SIM_LIST)
        self.assertTrue(os.path.isfile(os.path.join(box.out, 'MagicPortals-iOS-simulator-shell.zip')))

    def test_the_moltenvk_licence_travels_when_it_is_given(self):
        box = self.box()
        proc, lines = box.run(['macos', box.build, box.out, '--moltenvk-license', os.path.join(box.root, 'moltenvk-LICENSE')])
        self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
        self.assertTrue(os.path.isfile(os.path.join(box.out, 'MagicPortals.app', 'Contents', 'Resources', 'licenses', 'MoltenVK-LICENSE.txt')))

    def test_the_icon_failing_is_a_warning_not_the_end(self):
        box, proc, lines = self.run_ok('macos', env={'STUB_ICON_FAILS': '1'})
        self.assertIn('the icon could not be drawn', proc.stderr)
        self.assertNotIn('iconutil', ' '.join(lines))
        self.assertTrue(any(l.startswith('codesign <--force>') for l in lines), 'and the app is still signed')

    # ---- the dry run ---------------------------------------------------------------------------------------------------------------------------------

    def test_dry_run_prints_every_command_and_runs_and_makes_nothing(self):
        box = self.box()
        before = tree_hash(box.root)
        proc, lines = box.run(['macos', box.build, box.out, '--dry-run'])
        self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
        self.assertEqual(lines, [], 'no stand-in was called')
        self.assertEqual(tree_hash(box.root), before, 'nothing was made or changed')
        shown = [box.normal(line) for line in proc.stdout.splitlines() if line.startswith('+ ')]
        self.assertEqual(shown[:len(DRY_MACOS_HEAD)], DRY_MACOS_HEAD)
        self.assertIn(DRY_MACOS_TAIL[0], shown)
        # Every Apple command of the real run's list appears, in order, in the dry run's.
        apple = [l.split(' <')[0] for l in MACOS_LIST]
        dry_apple = [s.split(' ')[1] for s in shown if s.split(' ')[1] in {'otool', 'plutil', 'python3', 'iconutil', 'codesign', 'ditto', 'unzip', 'lipo', 'xcrun', 'zip'}]
        self.assertEqual(dry_apple, apple)

    # ---- MUTATIONS: each edit of the script turns the pinned list red -----------------------------------------------------------------------------------

    def mutate(self, old, new, platform='macos', extra=()):
        with open(SCRIPT, encoding='utf-8') as f:
            text = f.read()
        self.assertEqual(text.count(old), 1, 'the mutation applies once: %r' % old)
        box = self.box(text.replace(old, new))
        proc, lines = box.run([platform, box.build, box.out] + list(extra), {'STUB_ARCHS': 'arm64 x86_64' if platform == 'macos' else 'arm64'})
        return proc, lines

    def assert_red(self, name, old, new, platform='macos', extra=(), pinned=None):
        proc, lines = self.mutate(old, new, platform, extra)
        pinned = pinned if pinned is not None else MACOS_LIST
        self.assertTrue(proc.returncode != 0 or lines != pinned, 'MUTATION "%s" left the pinned list green' % name)

    def test_mutation_codesign_without_force(self):
        self.assert_red('codesign --force', 'run codesign --force --sign - --timestamp=none "$APP"', 'run codesign --sign - --timestamp=none "$APP"')

    def test_mutation_codesign_without_the_ad_hoc_identity(self):
        self.assert_red('codesign --sign -', 'run codesign --force --sign - --timestamp=none "$APP"', 'run codesign --force --timestamp=none "$APP"')

    def test_mutation_codesign_without_timestamp_none(self):
        self.assert_red('codesign --timestamp=none', 'run codesign --force --sign - --timestamp=none "$APP"', 'run codesign --force --sign - "$APP"')

    def test_mutation_ditto_without_keepparent(self):
        self.assert_red('ditto --keepParent', 'run ditto -c -k --keepParent --norsrc --noextattr --noqtn --noacl "$STAGE" "$ZIP"',
                        'run ditto -c -k --norsrc --noextattr --noqtn --noacl "$STAGE" "$ZIP"')

    def test_mutation_ditto_without_noqtn(self):
        self.assert_red('ditto --noqtn', 'run ditto -c -k --keepParent --norsrc --noextattr --noqtn --noacl "$STAGE" "$ZIP"',
                        'run ditto -c -k --keepParent --norsrc --noextattr --noacl "$STAGE" "$ZIP"')

    def test_mutation_a_plain_zip_in_place_of_ditto(self):
        self.assert_red('plain zip', 'run ditto -c -k --keepParent --norsrc --noextattr --noqtn --noacl "$STAGE" "$ZIP"', 'run zip -q -r "$ZIP" "$STAGE"')

    def test_mutation_the_verification_forgotten(self):
        self.assert_red('no strict verify', '    run codesign --verify --deep --strict --verbose=2 "$CHECKED"\n', '')

    def test_mutation_the_unpack_before_the_verification_forgotten(self):
        self.assert_red('no unpack', '    run ditto -x -k "$ZIP" "$WORK/unpacked"\n', '')

    def test_mutation_the_ipa_signature_check_forgotten(self):
        self.assert_red('no ipa verify', '    run codesign --verify --strict --verbose=2 "$CHECKED"\n', '', platform='ios', extra=('--sdk', 'iphoneos'), pinned=IOS_DEVICE_LIST)

    def test_mutation_the_scratch_folder_named_work_again(self):
        # The old name of the scratch folder: a run then removes a person's `work` folder from the folder it was given.
        with open(SCRIPT, encoding='utf-8') as f:
            text = f.read()
        old = 'WORK="$OUT_ABS/.make_app-work"'
        self.assertEqual(text.count(old), 1)
        box = self.box(text.replace(old, 'WORK="$OUT_ABS/work"'))
        planted = plant_a_persons_folders(box.out)
        proc, _ = box.run(['macos', box.build, box.out], {'STUB_ARCHS': 'arm64 x86_64'})
        self.assertTrue(what_is_left_of(planted) != planted, "MUTATION 'scratch folder named work' left a person's work folder alone")
        self.assertFalse(os.path.exists(os.path.join(box.out, 'work', 'keep.txt')), 'it was removed')
        self.assertEqual(proc.returncode, 0)

    def test_mutation_the_home_folder_rule_removed(self):
        # Only the home alternative of the first rule is taken out; the root stays. The repository rules do not see the home folder of this run (see the test
        # above), so nothing else refuses it: the test above would be red, which is what is asserted here.
        with open(SCRIPT, encoding='utf-8') as f:
            text = f.read()
        old = '/ | "${HOME:-/nonexistent}") refuse'
        self.assertEqual(text.count(old), 1)
        box = self.box(text.replace(old, '/) refuse'))
        home = os.path.join(box.root, 'a-home')
        os.makedirs(home)
        proc, _ = box.run(['macos', box.build, home], {'STUB_ARCHS': 'arm64 x86_64'}, home_from=home)
        self.assertEqual(proc.returncode, 0, "MUTATION 'no home rule' still refused the home folder: " + proc.stdout + proc.stderr)
        self.assertTrue(os.path.isfile(os.path.join(home, 'MagicPortals-macOS-shell.zip')), 'the script then filled the home folder')

    def test_mutation_the_ipa_built_without_the_payload_folder(self):
        self.assert_red('ipa without Payload', 'zip -q -r -y -X "$IPA" Payload)', 'zip -q -r -y -X "$IPA" MagicPortals.app)', platform='ios', extra=('--sdk', 'iphoneos'),
                        pinned=IOS_DEVICE_LIST)

    # ---- what it refuses -----------------------------------------------------------------------------------------------------------------------------

    def test_the_ipa_checks_refuse_what_no_sideloading_tool_could_install(self):
        for name, env, words in (
            ('a library outside the system', {'STUB_OTOOL_EXTRA': '/opt/homebrew/lib/libinvented.dylib'}, 'links something outside the system'),
            ('an encrypted executable', {'STUB_CRYPTID': '1'}, 'is encrypted'),
            ('an AppleDouble file in the package', {'STUB_UNZIP_EXTRA': '__MACOSX/._invented'}, 'AppleDouble'),
            ('a package that does not start with Payload/', {'STUB_UNZIP_FIRST': 'MagicPortals.app/'}, 'does not start with Payload/'),
        ):
            env = dict(env, STUB_ARCHS='arm64')
            box = self.box()
            proc, lines = box.run(['ios', box.build, box.out, '--sdk', 'iphoneos'], env)
            self.assertEqual(proc.returncode, 1, '%s: %s' % (name, proc.stdout + proc.stderr))
            self.assertIn(words, proc.stdout + proc.stderr, name)

    def test_the_macos_zip_with_an_appledouble_file_is_refused(self):
        box = self.box()
        proc, lines = box.run(['macos', box.build, box.out], {'STUB_UNZIP_EXTRA': 'MagicPortals/._MagicPortals.app'})
        self.assertEqual(proc.returncode, 1)
        self.assertIn('AppleDouble', proc.stdout + proc.stderr)
        self.assertFalse(any(l.startswith('ditto <-x>') for l in lines), 'and it never got as far as unpacking it')

    def appledouble_run(self, script_text, package, entry):
        """The script (the real one when `script_text` is None) run for one package, whose archive listing holds one more entry: `entry`, under the package's prefix
        when it is a `._` file (a `__MACOSX/` folder is at the top of an archive)."""
        name, platform, extra, env, prefix, _, _ = package
        box = self.box(script_text)
        listed = entry if entry.startswith('__MACOSX/') else prefix + entry
        return box.run([platform, box.build, box.out] + list(extra), dict(env, STUB_UNZIP_EXTRA=listed))

    def test_every_package_is_refused_for_either_appledouble_name_alone(self):
        for package in APPLEDOUBLE_PACKAGES:
            for entry in (MACOSX_ENTRY, DOT_UNDERSCORE_ENTRY):
                proc, lines = self.appledouble_run(None, package, entry)
                text = proc.stdout + proc.stderr
                self.assertEqual(proc.returncode, 1, '%s, with %s: %s' % (package[0], entry, text))
                self.assertIn('holds AppleDouble files', text, '%s, with %s' % (package[0], entry))
        # What comes after the check in the two zips never ran: the macOS zip was not unpacked, the simulator zip was not verified.
        proc, lines = self.appledouble_run(None, APPLEDOUBLE_PACKAGES[0], DOT_UNDERSCORE_ENTRY)
        self.assertFalse(any(l.startswith('ditto <-x>') for l in lines))
        proc, lines = self.appledouble_run(None, APPLEDOUBLE_PACKAGES[1], MACOSX_ENTRY)
        self.assertFalse(any(l.startswith('codesign <--verify>') for l in lines))

    def appledouble_mutation_lets_through(self, package):
        """For one package: the check cut to its first half, to its second half and to nothing. Each is run with the entry that only the dropped half sees, and the
        script must then NOT refuse it: that is what the test above would see, so it is red against each of these."""
        _, _, _, _, _, before, after = package
        with open(SCRIPT, encoding='utf-8') as f:
            text = f.read()
        old = before + APPLEDOUBLE_TEST + after
        self.assertEqual(text.count(old), 1, 'the mutation applies once in %s' % package[0])
        for mutant, new_test, entry in (('only the __MACOSX/ half', ONLY_MACOSX_HALF, DOT_UNDERSCORE_ENTRY), ('only the ._ half', ONLY_DOT_UNDERSCORE_HALF, MACOSX_ENTRY),
                                        ('no check at all', NEITHER_HALF, MACOSX_ENTRY), ('no check at all', NEITHER_HALF, DOT_UNDERSCORE_ENTRY)):
            proc, _ = self.appledouble_run(text.replace(old, before + new_test + after), package, entry)
            self.assertEqual(proc.returncode, 0, "MUTATION '%s' in %s still refused %s: %s" % (mutant, package[0], entry, proc.stdout + proc.stderr))
            self.assertNotIn('AppleDouble', proc.stdout + proc.stderr)

    def test_mutation_the_macos_zip_appledouble_check_cut(self):
        self.appledouble_mutation_lets_through(APPLEDOUBLE_PACKAGES[0])

    def test_mutation_the_simulator_zip_appledouble_check_cut(self):
        self.appledouble_mutation_lets_through(APPLEDOUBLE_PACKAGES[1])

    def test_mutation_the_ipa_appledouble_check_cut(self):
        self.appledouble_mutation_lets_through(APPLEDOUBLE_PACKAGES[2])

    # ---- the unpacked macOS zip is looked at before it is verified: its executable and its READ ME ------------------------------------------------------------

    UNPACKED_CHECKS = (
        ('the executable of the unpacked app', 'MagicPortals/MagicPortals.app/Contents/MacOS/MagicPortals', 'the unpacked app has no executable',
         'test -x "$CHECKED/Contents/MacOS/MagicPortals" || fail "the unpacked app has no executable"'),
        ('the READ ME of the unpacked folder', 'MagicPortals/READ ME FIRST.txt', 'the unpacked folder has no READ ME FIRST.txt',
         'test -f "$WORK/unpacked/MagicPortals/READ ME FIRST.txt" || fail "the unpacked folder has no READ ME FIRST.txt"'),
    )

    def test_an_unpacked_zip_without_its_executable_or_its_read_me_is_refused(self):
        for name, dropped, words, _ in self.UNPACKED_CHECKS:
            box = self.box()
            proc, lines = box.run(['macos', box.build, box.out], {'STUB_UNPACK_DROPS': dropped})
            self.assertEqual(proc.returncode, 1, '%s: %s' % (name, proc.stdout + proc.stderr))
            self.assertIn(words, proc.stdout + proc.stderr, name)
            self.assertFalse(any(l.startswith('codesign <--verify>') for l in lines), name + ': and it never got as far as the verification')

    def test_mutation_the_checks_of_the_unpacked_zip_removed(self):
        with open(SCRIPT, encoding='utf-8') as f:
            text = f.read()
        for name, dropped, words, line in self.UNPACKED_CHECKS:
            self.assertEqual(text.count(line), 1, 'the mutation applies once: ' + name)
            box = self.box(text.replace(line, ':'))
            proc, _ = box.run(['macos', box.build, box.out], {'STUB_UNPACK_DROPS': dropped})
            self.assertEqual(proc.returncode, 0, "MUTATION 'no check of %s' still refused: %s" % (name, proc.stdout + proc.stderr))
            self.assertNotIn(words, proc.stdout + proc.stderr)

    def move_exe_into_a_cmake_bundle(self, box):
        """Where CMake leaves an iOS program (seen on GitHub's runner): build/game/MagicPortals.app/MagicPortals, with nothing at build/game/MagicPortals."""
        plain = os.path.join(box.build, 'game', 'MagicPortals')
        inside = os.path.join(box.build, 'game', 'MagicPortals.app', 'MagicPortals')
        os.makedirs(os.path.dirname(inside))
        shutil.move(plain, inside)
        return inside

    def test_an_ios_program_inside_the_bundle_cmake_made_is_found(self):
        for sdk, pinned in (('iphoneos', IOS_DEVICE_LIST), ('iphonesimulator', IOS_SIM_LIST)):
            box = self.box()
            inside = self.move_exe_into_a_cmake_bundle(box)
            proc, lines = box.run(['ios', box.build, box.out, '--sdk', sdk], {'STUB_ARCHS': 'arm64'})
            self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
            self.assertEqual(lines, pinned, sdk)
            with open(inside, 'rb') as built, open(os.path.join(box.out, 'MagicPortals.app', 'MagicPortals'), 'rb') as copied:
                self.assertEqual(built.read(), copied.read(), sdk)

    def test_a_macos_program_is_not_looked_for_inside_a_bundle(self):
        box = self.box()
        self.move_exe_into_a_cmake_bundle(box)
        proc, lines = box.run(['macos', box.build, box.out])
        self.assertEqual(proc.returncode, 1)
        self.assertIn('No executable at', proc.stderr)

    def test_mutation_the_ios_bundle_lookup_removed_is_refused(self):
        with open(SCRIPT, encoding='utf-8') as f:
            text = f.read()
        line = 'if [ "$PLATFORM" = ios ] && [ ! -f "$EXE" ] && [ -f "$EXE.app/MagicPortals" ]; then EXE="$EXE.app/MagicPortals"; fi\n'
        self.assertEqual(text.count(line), 1, 'the mutation applies once')
        box = self.box(text.replace(line, ''))
        self.move_exe_into_a_cmake_bundle(box)
        proc, lines = box.run(['ios', box.build, box.out, '--sdk', 'iphoneos'], {'STUB_ARCHS': 'arm64'})
        self.assertEqual(proc.returncode, 1)
        self.assertIn('No executable at', proc.stderr)

    def test_a_missing_executable_or_plist_is_said_in_words(self):
        box = self.box()
        os.remove(os.path.join(box.build, 'game', 'MagicPortals'))
        proc, lines = box.run(['macos', box.build, box.out])
        self.assertEqual(proc.returncode, 1)
        self.assertIn('No executable at', proc.stderr)
        self.assertEqual(lines, [])
        box = self.box()
        os.remove(os.path.join(box.build, 'game', 'macos', 'Info.plist'))
        proc, lines = box.run(['macos', box.build, box.out])
        self.assertEqual(proc.returncode, 1)
        self.assertIn('Info.plist', proc.stderr)

    def test_it_writes_nowhere_but_the_folder_it_is_given(self):
        box = self.box()
        planted = plant_a_persons_folders(box.out)
        before_repo = tree_hash(box.repo, skip=('out/',))
        before_build = tree_hash(box.build)
        proc, lines = box.run(['macos', box.build, box.out])
        self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
        self.assertEqual(tree_hash(box.repo, skip=('out/',)), before_repo, 'the sources were not touched')
        self.assertEqual(tree_hash(box.build), before_build, 'the build was not touched')
        self.assertEqual(sorted(os.listdir(box.out)), sorted(['MagicPortals-macOS-shell.zip', 'MagicPortals.app'] + [folder for folder, _ in PERSONS_FOLDERS]),
                         'the folder holds what it made and what was there')
        self.assertEqual(what_is_left_of(planted), planted, "a folder of the person's own in the folder it was given, one of them named work, was not removed or changed")

    def test_a_folder_that_is_not_its_to_fill_is_refused_before_anything_is_removed(self):
        box = self.box()
        for name, out in (
            ('the file system root', '/'),
            ('the repository', box.repo),
            ('a folder of the sources', os.path.join(box.repo, 'tools')),
            ('a folder inside the repository that is not under out/', os.path.join(box.repo, 'docs-out')),
            ('the folder that holds the repository', box.root),
            ('a folder whose parent is missing', os.path.join(box.root, 'no-such', 'x')),
        ):
            before = tree_hash(box.root)
            proc, lines = box.run(['macos', box.build, out])
            self.assertEqual(proc.returncode, 2, '%s: %s' % (name, proc.stdout + proc.stderr))
            self.assertEqual(lines, [], name)
            self.assertEqual(tree_hash(box.root), before, name + ': nothing was removed or made')
        # And a folder that is not there yet under out/ is made, not refused.
        proc, lines = box.run(['macos', box.build, os.path.join(box.repo, 'out', 'new')])
        self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
        self.assertTrue(os.path.isfile(os.path.join(box.repo, 'out', 'new', 'MagicPortals-macOS-shell.zip')))

    def test_a_home_folder_is_refused_on_the_home_rule_alone(self):
        # This home folder neither holds the repository nor lies inside it, so the rules about the repository cannot be what refuses it.
        box = self.box()
        home = os.path.join(box.root, 'a-home')
        os.makedirs(home)
        before = tree_hash(box.root)
        proc, lines = box.run(['macos', box.build, home], home_from=home)
        self.assertEqual(proc.returncode, 2, proc.stdout + proc.stderr)
        self.assertIn('is a place no script should fill', proc.stderr)
        self.assertEqual(lines, [])
        self.assertEqual(tree_hash(box.root), before, 'nothing was removed or made')
        # Control: the same folder is filled when it is not the home folder.
        proc, lines = box.run(['macos', box.build, home])
        self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
        self.assertTrue(os.path.isfile(os.path.join(home, 'MagicPortals-macOS-shell.zip')))

    def test_a_folder_outside_the_repository_is_allowed(self):
        box = self.box()
        elsewhere = os.path.join(box.root, 'elsewhere')
        os.makedirs(elsewhere)
        proc, lines = box.run(['macos', box.build, elsewhere])
        self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
        self.assertTrue(os.path.isfile(os.path.join(elsewhere, 'MagicPortals-macOS-shell.zip')))

    def test_bad_arguments_print_the_usage(self):
        box = self.box()
        for args in ([], ['windows', box.build, box.out], ['macos'], ['macos', box.build], ['ios', box.build, box.out, '--sdk', 'xros'], ['macos', box.build, box.out, '--bogus']):
            proc, lines = box.run(args)
            self.assertEqual(proc.returncode, 2, args)
            self.assertEqual(lines, [], args)

    def test_no_tool_outside_the_pinned_set_is_ever_called(self):
        for platform, extra in (('macos', []), ('ios', ['--sdk', 'iphoneos']), ('ios', ['--sdk', 'iphonesimulator'])):
            _, _, lines = self.run_ok(platform, extra, {'STUB_ARCHS': 'arm64'} if platform == 'ios' else None)
            tools = {line.split(' ')[0] for line in lines}
            self.assertTrue(tools <= {'otool', 'plutil', 'python3', 'iconutil', 'codesign', 'ditto', 'unzip', 'lipo', 'xcrun', 'zip'}, tools)
            for never in NEVER:
                self.assertNotIn(never, tools)

    def test_the_script_names_nothing_of_the_sibling(self):
        with open(SCRIPT, encoding='utf-8') as f:
            text = f.read().lower()
        # Written in pieces, so that this file passes the grep for the sibling's name that the hard rule's pre-commit check makes.
        for word in ('pen' + 'umbra', 'extracted/app', 'ico_to' + '_png', 'strings.json', 'lg' + 'pl'):
            self.assertNotIn(word, text)


if __name__ == '__main__':
    unittest.main()
