#!/usr/bin/env python3
"""The program's ONE version number, and the numbers the platforms make of it (planning step 208, PORT-10).

    version.py            print the version (x.y.z)
    version.py code       print the Android versionCode that follows from it
    version.py windows    print the four-field string the Windows file properties carry

THE ONE DEFINITION is `project(MagicPortals VERSION x.y.z ...)` in the top-level CMakeLists.txt. Everything else that carries the number either reads it
(CMake's own VERSIONINFO, the two Info.plist templates, the Android packager, the release packager) or is checked against it by check_version.py. This module is
the one reader: no tool of this folder keeps its own copy of the pattern. It only READS the number; what the first release is called is the owner's, and
0.1.0 is a placeholder (docs/packaging.md section 6).

THE RULES, each pinned by test_version.py:
  versionCode  = major * 10000 + minor * 100 + patch      0.1.0 -> 100, 1.2.3 -> 10203. It goes up with every release that goes up in any field, which is
                 what Android demands of an update. A field of 100 or more would collide with its neighbour (0.100.0 = 1.0.0), so such a version has no code
                 and is refused rather than given a wrong one; so is 0.0.0, whose code would be 0 where Android needs at least 1. The owner may still give a
                 different code on the command line of a build.
  The definition has exactly three fields. CMake accepts one to four after VERSION; this reader refuses 1.2 and 1.2.3.4 by name instead of cutting or padding them
  (the Info.plists, the Windows file properties and the code rule are all made of three).
  Windows      = the version padded to four fields with a zero    0.1.0 -> 0.1.0.0   (game/windows/MagicPortalsWindows.cmake does the same padding).

Standard library only.
"""
import os
import re
import sys

CMAKE_FILE = 'CMakeLists.txt'
# The definition. Anchored at the start of a line (a comment that shows the shape is not the line); the whole run of digits and dots after VERSION is taken, so that a
# number of two or four fields is refused as what it is (SHAPE) and not read as its first three fields: the file has exactly one.
DEFINITION = re.compile(r'^\s*project\s*\(\s*MagicPortals\s+VERSION\s+(\d[\d.]*)', re.M)
SHAPE = re.compile(r'\d+\.\d+\.\d+')
MOST_FIELD = 99  # the largest value of one field the versionCode rule can carry
LEAST_CODE = 1  # Android refuses a versionCode below 1
CODE_MAJOR, CODE_MINOR = 10000, 100


class VersionError(Exception):
    pass


def default_repo():
    """The repository this file is in: tools/package/version.py -> two folders up."""
    return os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def read_version(repo=None):
    """x.y.z, from the top-level CMakeLists.txt of `repo`, which must state it exactly once."""
    path = os.path.join(repo or default_repo(), CMAKE_FILE)
    try:
        with open(path, encoding='utf-8') as f:
            text = f.read()
    except OSError as e:
        raise VersionError('%s cannot be read: %s' % (path, e))
    found = DEFINITION.findall(text)
    if len(found) != 1:
        raise VersionError('CMakeLists.txt must state project(MagicPortals VERSION x.y.z ...) exactly once (found %d)' % len(found))
    if not SHAPE.fullmatch(found[0]):
        raise VersionError('CMakeLists.txt defines the version as %s; it must be exactly three fields, x.y.z (the Info.plists, the Windows file properties and the versionCode '
                           'rule are made of three)' % found[0])
    return found[0]


def fields(version):
    """(major, minor, patch) of an x.y.z string. Refuses anything else, and a field above MOST_FIELD (the versionCode rule could not carry it)."""
    m = re.fullmatch(r'(\d+)\.(\d+)\.(\d+)', version or '')
    if not m:
        raise VersionError('%r is not a version of the form x.y.z' % (version,))
    major, minor, patch = (int(g) for g in m.groups())
    for name, value in (('major', major), ('minor', minor), ('patch', patch)):
        if value > MOST_FIELD:
            raise VersionError('the %s field of %s is %d; the versionCode rule carries at most %d in a field (0.100.0 would be 1.0.0)' % (name, version, value, MOST_FIELD))
    return major, minor, patch


def version_code(version):
    major, minor, patch = fields(version)
    code = major * CODE_MAJOR + minor * CODE_MINOR + patch
    if code < LEAST_CODE:
        raise VersionError('the versionCode of %s would be %d, and Android needs at least %d' % (version, code, LEAST_CODE))
    return code


def windows_version(version):
    """The string of VERSIONINFO's FileVersion and ProductVersion: the version padded to four fields, as MagicPortalsWindows.cmake pads it."""
    fields(version)
    return version + '.0'


def main(argv=None):
    args = sys.argv[1:] if argv is None else list(argv)
    what = args[0] if args else 'version'
    repo = None
    if len(args) == 3 and args[1] == '--repo':
        repo = args[2]
    elif len(args) > 1:
        print('usage: version.py [version|code|windows] [--repo <repository>]', file=sys.stderr)
        return 2
    try:
        version = read_version(repo)
        if what in ('version', 'show'):
            print(version)
        elif what == 'code':
            print(version_code(version))
        elif what == 'windows':
            print(windows_version(version))
        else:
            print('version.py: unknown word %r (version, code or windows)' % what, file=sys.stderr)
            return 2
    except VersionError as e:
        print('version.py: %s' % e, file=sys.stderr)
        return 2
    return 0


if __name__ == '__main__':
    sys.exit(main())
