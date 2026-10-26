#!/usr/bin/env python3
"""Checks the program's version in every place it is written (planning step 208, PORT-10).

    check_version.py [--repo <repository>]       exit 0 when every place agrees; exit 1 and name each place that does not

THE ONE DEFINITION is `project(MagicPortals VERSION x.y.z ...)` in the top-level CMakeLists.txt (tools/package/version.py reads it). A place that carries the
number is one of four kinds, and each place of this repository is in PLACES below, decided by reading it and by a scan for the literal:

  READS      a file that is given the number by CMake or by version.py and must not spell one of its own (the two Info.plist templates, the Windows resource
             template, game/CMakeLists.txt's two hand-overs, the Android packager). The check asks that the hand-over is still there.
  FOLLOWS    a sentence or an example that SAYS what the version is, in a document: the number in it must equal the definition (or the form the platform
             makes of it: Windows' four fields). History is not such a place; DEVLOG.md, docs/planning/ and docs/parity-backlog.md are records of what was true
             when they were written and are not scanned.
  NOT SPELLED a file that must not spell it at all: the Android manifest has neither versionName nor versionCode (the packager hands both to aapt2).
  ALLOWED    a hit of the literal that is not this program's version: another product's own number (the converter's) and invented test data.

The scan: every text file of the repository (except the history above, engine/, out/, reference/, build trees and caches) that holds the CURRENT version as a
literal must be a FOLLOWS place or an ALLOWED one. A new file that spells the number is therefore a failure naming it: decide it and add it to the table.
test_version.py mutates each place in a copy of the tree and sees this check go red.

Standard library only.
"""
import os
import re
import sys
import xml.etree.ElementTree as ET

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import version as vs  # noqa: E402

ANDROID_NS = '{http://schemas.android.com/apk/res/android}'

# ---- READS: (path, [(text that must be in the file, why)], [(regex that must NOT match, why)]) ----------------------------------------------
READS = (
    ('game/windows/MagicPortals.rc.in',
     (('VALUE "FileVersion", "@MAGICPORTALS_RC_VERSION@"', 'the file version is the one version, made by CMake'),
      ('VALUE "ProductVersion", "@MAGICPORTALS_RC_VERSION@"', 'the product version is the one version, made by CMake'),
      ('FILEVERSION @MAGICPORTALS_RC_VERSION_COMMAS@', 'the numeric file version is the one version'),
      ('PRODUCTVERSION @MAGICPORTALS_RC_VERSION_COMMAS@', 'the numeric product version is the one version')),
     ((r'VALUE\s+"(File|Product)Version",\s*"\d', 'a version spelled in the Windows resource template'),
      (r'(FILE|PRODUCT)VERSION\s+\d', 'a numeric version spelled in the Windows resource template'))),
    ('game/windows/MagicPortalsWindows.cmake',
     (('while (_count LESS 4)', 'the version is padded to the four fields a VERSIONINFO has (version.py windows_version does the same)'),
      ('set(MAGICPORTALS_RC_VERSION "${_major}.${_minor}.${_patch}.${_build}")', 'the string is made of the four fields')),
     ()),
    ('game/CMakeLists.txt',
     (('magicportals_windows_release(MagicPortals VERSION "${MagicPortals_VERSION}"', 'the Windows program is given the project version'),
      ('set(MAGICPORTALS_BUNDLE_VERSION "${MagicPortals_VERSION}")', 'the bundles are given the project version')),
     ()),
    ('game/macos/Info.plist.in',
     (('<key>CFBundleShortVersionString</key>\n    <string>@MAGICPORTALS_BUNDLE_VERSION@</string>', 'the macOS short version is the one version'),
      ('<key>CFBundleVersion</key>\n    <string>@MAGICPORTALS_BUNDLE_VERSION@</string>', 'the macOS bundle version is the one version')),
     ()),
    ('game/ios/Info.plist.in',
     (('<key>CFBundleShortVersionString</key>\n    <string>@MAGICPORTALS_BUNDLE_VERSION@</string>', 'the iOS short version is the one version'),
      ('<key>CFBundleVersion</key>\n    <string>@MAGICPORTALS_BUNDLE_VERSION@</string>', 'the iOS bundle version is the one version')),
     ()),
    ('tools/package/android_package.py',
     (('import version as vs', 'the Android packager reads the version through version.py'),
      ('return vs.read_version(repo)', 'its versionName is the one version'),
      ('return vs.version_code(version)', 'its default versionCode is the rule of version.py')),
     ((r"default=1\b", 'a literal default versionCode in the Android packager'),)),
    ('tools/build_android.sh',
     (('VERSION_CODE=""', 'the build script gives no versionCode of its own: the packager derives it'),),
     ((r'^VERSION_CODE=[0-9]', 'a literal versionCode default in the Android build script'),)),
    ('tools/package/release.py',
     (('import version as vs', 'the release packager reads the version through version.py'),),
     ()),
)

# ---- FOLLOWS: (path, regex with ONE group, how many matches, what the group must equal, the sentence) ---------------------------------------
THE_VERSION, WINDOWS_FORM = 'the version', 'the Windows form (four fields)'
FOLLOWS = (
    ('docs/packaging.md', r'`project\(MagicPortals VERSION (\d+\.\d+\.\d+) \.\.\.\)`', 1, THE_VERSION, 'section 6 says what the one definition is'),
    ('docs/packaging.md', r'`ProductVersion` (\d+\.\d+\.\d+\.\d+)\)', 1, WINDOWS_FORM, 'section 6 says what the Windows file properties carry'),
    ('docs/apple.md', r'the version \(`(\d+\.\d+\.\d+)`\)', 1, THE_VERSION, 'section 10 lists the version among the decisions that are the owner\'s'),
    ('docs/android.md', r'MagicPortals-(\d+\.\d+\.\d+)-debug\.apk', 2, THE_VERSION, 'the example commands name the APK the build script writes'),
)

# ---- NOT SPELLED -------------------------------------------------------------------------------------------------------------------------
MANIFEST = 'game/android/AndroidManifest.xml'
MANIFEST_MUST_NOT_SPELL = ('versionName', 'versionCode')

# ---- ALLOWED: hits of the literal that are not this program's version. (A file, or the start of a path; the reason is the decision.) --------------
ALLOWED = (
    ('tools/converter/', "the converter's own version (pyproject.toml, __init__.py): another product, released on its own"),
    ('tools/apple/test_', 'invented test data of the Apple scripts\' tests (a stand-in plutil, a template filled with several versions)'),
    ('tools/package/test_', 'invented file names and fixtures of the packager\'s tests'),
    ('tools/package/check_version.py', 'this file'),
    ('tools/package/version.py', 'the rules, shown by example (0.1.0 -> 100); the code reads the definition'),
    ('game/windows/MagicPortalsWindows.cmake', 'a comment that shows the padding by example ("0.1.0" -> 0,1,0,0); the code reads its argument'),
)

# ---- the scan's exclusions: records of what was true when written, other people's trees, caches -------------------------------------------------
SKIP_DIRS = ('.git', 'engine', 'out', 'reference', 'node_modules', '__pycache__', '.venv', '.mypy_cache', '.ruff_cache', '.pytest_cache', '.idea', '.vscode')
# A build tree or an environment is `build` / `venv` or `build-<kind>` / `venv-<kind>` (build-rel, venv-apple); a plain prefix would also skip tests/build_pins/ and any builder/ folder.
SKIP_DIR_PATTERN = re.compile(r'^(?:build|venv)(?:-.*)?$')
SKIP_PATHS = ('DEVLOG.md', 'docs/planning/', 'docs/parity-backlog.md')
MOST_BYTES = 5 * 1024 * 1024


def read_text(path):
    try:
        with open(path, 'rb') as f:
            data = f.read(MOST_BYTES + 1)
    except OSError:
        return None
    if len(data) > MOST_BYTES or b'\0' in data[:4096]:
        return None
    try:
        return data.decode('utf-8').replace('\r\n', '\n')
    except UnicodeDecodeError:
        return None


def scan_files(repo):
    """Relative '/' paths of the text files the literal scan reads."""
    found = []
    for folder, dirs, files in os.walk(repo):
        rel_dir = os.path.relpath(folder, repo).replace(os.sep, '/')
        dirs[:] = sorted(d for d in dirs if d not in SKIP_DIRS and not SKIP_DIR_PATTERN.match(d))
        for name in sorted(files):
            rel = name if rel_dir == '.' else rel_dir + '/' + name
            if any(rel == p or (p.endswith('/') and rel.startswith(p)) for p in SKIP_PATHS):
                continue
            found.append(rel)
    return found


def check(repo):
    """The list of problems (each names the place); empty when every place agrees."""
    repo = os.path.abspath(repo)
    problems = []
    try:
        version = vs.read_version(repo)
        windows = vs.windows_version(version)
        vs.version_code(version)
    except vs.VersionError as e:
        return ['CMakeLists.txt (the definition): %s' % e]

    for path, needed, forbidden in READS:
        text = read_text(os.path.join(repo, *path.split('/')))
        if text is None:
            problems.append('%s: cannot be read (it must hand the version on)' % path)
            continue
        for snippet, why in needed:
            if snippet not in text:
                problems.append('%s: no longer has %r -- %s' % (path, snippet.split('\n')[0], why))
        for pattern, why in forbidden:
            if re.search(pattern, text, re.M):
                problems.append('%s: %s (it must be given the version, not spell one)' % (path, why))

    for path, pattern, count, form, why in FOLLOWS:
        text = read_text(os.path.join(repo, *path.split('/')))
        if text is None:
            problems.append('%s: cannot be read (%s)' % (path, why))
            continue
        hits = re.findall(pattern, text)
        want = windows if form == WINDOWS_FORM else version
        if len(hits) != count:
            problems.append('%s: %d place(s) say what the version is where %d were expected (%s)' % (path, len(hits), count, why))
        for hit in hits:
            if hit != want:
                problems.append('%s: says %s, %s is %s (%s)' % (path, hit, form, want, why))  # THE_VERSION and WINDOWS_FORM already start with "the"

    manifest_path = os.path.join(repo, *MANIFEST.split('/'))
    try:
        root = ET.parse(manifest_path).getroot()
        for attr in MANIFEST_MUST_NOT_SPELL:
            if root.get(ANDROID_NS + attr) is not None:
                problems.append('%s: spells %s (the packager hands it to aapt2; the manifest must not)' % (MANIFEST, attr))
    except (OSError, ET.ParseError) as e:
        problems.append('%s: cannot be read: %s' % (MANIFEST, e))

    literal = re.compile(r'(?<![\d.])%s(?!\d)' % re.escape(version))
    followed = {path for path, *_ in FOLLOWS}
    for rel in scan_files(repo):
        if rel in followed:
            continue
        text = read_text(os.path.join(repo, *rel.split('/')))
        if text is None or not literal.search(text):
            continue
        if rel == 'CMakeLists.txt':
            continue  # the definition itself, and the comment that says it is a placeholder
        if any(rel.startswith(a) for a, _ in ALLOWED):
            continue
        line = next(n for n, l in enumerate(text.split('\n'), 1) if literal.search(l))
        problems.append('%s:%d: spells the version %s and is in no table of check_version.py: decide it (a place that must follow the version, or one that '
                        'is not this program\'s) and add it' % (rel, line, version))
    return problems


def main(argv=None):
    args = list(sys.argv[1:] if argv is None else argv)
    repo = vs.default_repo()
    if args[:1] == ['--repo'] and len(args) == 2:
        repo = args[1]
    elif args:
        print('usage: check_version.py [--repo <repository>]', file=sys.stderr)
        return 2
    problems = check(repo)
    if problems:
        for p in problems:
            print('VERSION: ' + p)
        print('check_version: %d place(s) disagree with project(MagicPortals VERSION ...)' % len(problems))
        return 1
    version = vs.read_version(repo)
    print('check_version: ok -- %s (versionCode %d, Windows %s); %d handed-on places, %d sentences, the Android manifest spells none'
          % (version, vs.version_code(version), vs.windows_version(version), len(READS), len(FOLLOWS)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
