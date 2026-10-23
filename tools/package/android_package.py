#!/usr/bin/env python3
"""The Android packager (planning step 203, PORT-5): assembles, signs and checks Magic Portals Enhanced's APK.

NOT RUN ON A PHONE OR AN EMULATOR as of this step: this builds, packages and statically checks. Nothing it makes has ever been installed.

    android_package.py build   --repo <repo> --sdk <sdk> --ndk <ndk> --lib <abi>=<libMagicPortals.so> [--lib ...]
                               (--package <content folder> | --no-data) [--out <dir>] [--build-tools <ver>] [--version-code N]
                               [--verification] [--release --keystore F --keystore-pass-file F --key-alias A [--allow-data-less-release]]
                               [--build-tree <dir>]... [--shaders <dir>] [--java-src <dir>] [--icons <dir>]
    android_package.py inspect <apk> --sdk <sdk> [--build-tools <ver>] [--repo <repo>] [--release --keystore F --keystore-pass-file F --key-alias A]
    android_package.py compare <apk> --package <content folder> [--shaders <dir>] [--repo <repo>]
    android_package.py check-out --repo <repo> --out <dir>

tools/build_android.sh is the front door: it finds the SDK and NDK, builds the libraries and calls `build`. Python, standard library only.

WHAT THE APK HOLDS, and where each part comes from (nothing is read from outside these):
  AndroidManifest.xml, resources     game/android/AndroidManifest.xml and the launcher icon set (tools/art/make_app_icon.py draws it at package
                                     time into the work folder; nothing of the kind is committed), linked by aapt2. versionName is the project VERSION in the
                                     top-level CMakeLists.txt (the one definition) and versionCode is --version-code: aapt2 writes both in.
  classes.dex                        the engine's Java activity (engine/src/platform/android/java), javac then d8.
  lib/<abi>/libMagicPortals.so       the libraries tools/build_android.sh built, each checked as an ELF file (machine, NEEDED, 16 KB alignment...).
  assets/gamedata.list               the list the first-run unpack reads (game/sim/Unpack.hpp): a stamp, then `size TAB path` per file.
  assets/GameData/**                 the content packager's files (tools/package/package.py), EXACTLY the files its manifest.json lists, each
                                     re-hashed against it. The only input folder that holds the owner's data: --package. Nothing else is read from
                                     outside the repository, and no file is taken from it that is not listed.
  assets/engine/assets/shaders/*.spv the engine's compiled shaders (--shaders, default <repo>/engine/assets/shaders), plain .spv files only.
  Without --package (--no-data) there is no GameData and the APK says so on the phone.

WHAT IT REFUSES, each with a message that names the cause and a non-zero exit:
  a package path that is a symbolic link or resolves outside the package folder; a listed file that is missing, of another size or hash, or a
  link; a file in the folder the manifest does not list; a name that is not a safe relative path; anything on the never-package list (the original's
  scenes, .pvr/.pkm/.par/.enml, .exe/.dll/.so/.bin/.as, the APK, build folders ...) and a package that is itself under reference/apk or
  reference/ethanon; a build tree whose cache says Vulkan validation ON, baked data paths ON, the phone profile OFF or developer tools in a player's
  build; a missing or unreadable library, or one that is not an ELF shared object of its ABI with only the allowed NEEDED libraries, 16 KB LOAD
  alignment, no executable-and-writable segment and no text relocation; --release with the verification variant, with no key or with the debug key;
  an output folder inside the repository other than under out/.

DETERMINISTIC: the same inputs give a byte-identical APK (sorted entries, a fixed date and attributes, no clock, the debug key's signature is
deterministic RSA). The bytes the tools make are those of the Python zlib and the JDK's javac/d8 of the machine; two runs on one machine are equal.
"""
import argparse
import hashlib
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import xml.etree.ElementTree as ET
import zipfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import package as pk  # noqa: E402  (the content packager: never_reason, sha256_of, check_output)

# ---- Names that game/android/AndroidMain.cpp spells too (test_android_package.py reads that file and ties them) -----------------------------
LIST_ASSET = 'gamedata.list'
CONTENT_FOLDER = 'GameData'
ENGINE_FOLDER = 'engine'
ARGS_FILE = 'magicportals_args.txt'
SHADERS_DEST = ENGINE_FOLDER + '/assets/shaders'
OWNED_ROOTS = (CONTENT_FOLDER, ENGINE_FOLDER)
LIB_NAME = 'libMagicPortals.so'
STAMP_CHARS = 32  # hex digits of the stamp: 128 bits of sha256

ABI_MACHINE = {'arm64-v8a': 183, 'x86_64': 62}  # ELF e_machine: EM_AARCH64, EM_X86_64
# Libraries the game may need from the phone (the C++ runtime is linked in statically, ANDROID_STL=c++_static).
ALLOWED_NEEDED = ('libandroid.so', 'liblog.so', 'libvulkan.so', 'libaaudio.so', 'libc.so', 'libm.so', 'libdl.so')
MIN_LOAD_ALIGN = 0x4000  # 16 KB pages

# Stored, not deflated, in the APK: formats that are compressed already (aapt2's own list, shortened to what the game has).
STORED_EXTENSIONS = {'.png', '.jpg', '.jpeg', '.mp3', '.ogg', '.wav'}
ZIP_DATE = (1980, 1, 1, 0, 0, 0)  # the earliest a zip can say: no clock enters the APK
SIZE_SEPARATOR = '\t'
MOST_SIZE_DIGITS = 15

# Paths that must never be taken as the package or as a source: the original's APK and the engine of the original (CLAUDE.md, the hard rule).
FORBIDDEN_INPUT_PAIRS = (('reference', 'apk'), ('reference', 'ethanon'), ('reference', 'apk_work'))
FORBIDDEN_NAME_EXTENSIONS = ('.exe', '.dll', '.as', '.pvr', '.pkm', '.apk', '.aab', '.jks', '.keystore', '.idsig')


class PackError(Exception):
    pass


# ---- paths ----------------------------------------------------------------------------------------------------------------------------

def real(path):
    return os.path.normcase(os.path.realpath(path))


def inside(path, folder):
    a, b = path.rstrip(os.sep) + os.sep, folder.rstrip(os.sep) + os.sep
    return a.startswith(b)


def forbidden_input_reason(path):
    """Why a folder may not be used as an input, or None: the original's APK and the engine source are read-only references, never packaged."""
    parts = [p.lower() for p in real(path).replace('\\', '/').split('/')]
    for first, second in FORBIDDEN_INPUT_PAIRS:
        for i in range(len(parts) - 1):
            if parts[i] == first and parts[i + 1] == second:
                return 'it is inside %s/%s, a reference the project reads and never packages' % (first, second)
    return None


def safe_rel_path(path, owned=None):
    """The C++ SafePath (game/sim/Unpack.cpp) in Python: None if `path` may be written under the app's files folder, else the reason."""
    if not path:
        return 'the path is empty'
    for c in path:
        if ord(c) < 0x20 or ord(c) == 0x7F:
            return 'the path holds a control character'
        if c == '\\':
            return 'the path holds a backslash'
        if c == ':':
            return 'the path holds a colon'
    if path[0] == '/':
        return 'the path is absolute'
    if path[-1] == '/':
        return 'the path names a folder'
    segments = path.split('/')
    for seg in segments:
        if seg == '':
            return 'the path has an empty segment'
        if seg == '.':
            return "the path has a '.' segment"
        if seg == '..':
            return "the path has a '..' segment"
    if owned is not None:
        if segments[0] not in owned:
            return 'the path is not under an owned folder'
        if len(segments) < 2:
            return "the path is an owned folder's bare name"
    return None


def check_out(out, repo):
    """The output folder is under <repo>/out/ or outside the repository altogether (the same rule as the content packager)."""
    try:
        pk.check_output(os.path.abspath(out), os.path.abspath(repo), os.path.join(os.path.abspath(repo), 'reference'))
    except pk.PackageError as e:
        raise PackError(str(e))


# ---- the project's facts, each read from its one definition ---------------------------------------------------------------------------

def read_version(repo):
    """versionName: the project VERSION of the top-level CMakeLists.txt, exactly one definition."""
    with open(os.path.join(repo, 'CMakeLists.txt'), encoding='utf-8') as f:
        text = f.read()
    found = re.findall(r'^\s*project\s*\(\s*MagicPortals\s+VERSION\s+(\d+\.\d+\.\d+)\b', text, re.M)
    if len(found) != 1:
        raise PackError('CMakeLists.txt must state project(MagicPortals VERSION x.y.z ...) exactly once (found %d)' % len(found))
    return found[0]


ANDROID_NS = '{http://schemas.android.com/apk/res/android}'


def read_manifest(path):
    """The facts of game/android/AndroidManifest.xml that the build needs, and the policy it must keep. Raises PackError on a breach."""
    try:
        root = ET.parse(path).getroot()
    except (OSError, ET.ParseError) as e:
        raise PackError('%s cannot be read: %s' % (path, e))
    facts = {'package': root.get('package')}
    if not facts['package']:
        raise PackError('the manifest names no package')
    for attr in ('versionName', 'versionCode'):
        if root.get(ANDROID_NS + attr) is not None:
            raise PackError('the manifest spells %s: it is written in by the packager (aapt2 --version-name/--version-code), so that it is spelled once' % attr)
    sdk = root.find('uses-sdk')
    if sdk is None or sdk.get(ANDROID_NS + 'minSdkVersion') is None or sdk.get(ANDROID_NS + 'targetSdkVersion') is None:
        raise PackError('the manifest must state uses-sdk minSdkVersion and targetSdkVersion')
    facts['minSdk'] = int(sdk.get(ANDROID_NS + 'minSdkVersion'))
    facts['targetSdk'] = int(sdk.get(ANDROID_NS + 'targetSdkVersion'))
    permissions = [e.get(ANDROID_NS + 'name') for e in root.iter() if e.tag in ('uses-permission', 'uses-permission-sdk-23', 'permission')]
    if permissions:
        raise PackError('the manifest asks for permissions the game does not need: %s' % ', '.join(map(str, permissions)))
    features = {f.get(ANDROID_NS + 'name'): (f.get(ANDROID_NS + 'version'), f.get(ANDROID_NS + 'required')) for f in root.findall('uses-feature')}
    if features.get('android.hardware.vulkan.version') != ('0x401000', 'true'):
        raise PackError('the manifest must require android.hardware.vulkan.version 0x401000 (Vulkan 1.1)')
    for optional in ('android.hardware.touchscreen', 'android.hardware.gamepad'):
        if features.get(optional, (None, None))[1] != 'false':
            raise PackError('%s must be declared and not required' % optional)
    app = root.find('application')
    if app is None or app.get(ANDROID_NS + 'hasCode') != 'true':
        raise PackError('the application must declare hasCode="true" (the engine ships a Java activity)')
    if app.get(ANDROID_NS + 'allowBackup') != 'false':
        raise PackError('the application must declare allowBackup="false"')
    activity = app.find('activity')
    if activity is None:
        raise PackError('the manifest has no activity')
    if activity.get(ANDROID_NS + 'screenOrientation') != 'landscape':
        raise PackError('the activity must be locked to landscape')
    changes = set((activity.get(ANDROID_NS + 'configChanges') or '').split('|'))
    needed = {'orientation', 'screenSize', 'smallestScreenSize', 'screenLayout', 'keyboard', 'keyboardHidden', 'navigation', 'uiMode', 'density',
              'fontScale', 'layoutDirection', 'locale', 'touchscreen', 'colorMode'}
    if not needed <= changes:
        raise PackError('the activity must handle every configuration change in place; missing %s' % ', '.join(sorted(needed - changes)))
    meta = {m.get(ANDROID_NS + 'name'): m.get(ANDROID_NS + 'value') for m in activity.findall('meta-data')}
    facts['activity'] = activity.get(ANDROID_NS + 'name')
    facts['lib_name'] = meta.get('android.app.lib_name')
    facts['report_log'] = meta.get('supersonic.reportLog')
    facts['report_skip'] = meta.get('supersonic.reportSkipWhenFile')
    facts['report_enabled'] = meta.get('supersonic.reportUnexpectedExit')
    if facts['lib_name'] is None or 'lib%s.so' % facts['lib_name'] != LIB_NAME:
        raise PackError('android.app.lib_name must name the library %s' % LIB_NAME)
    return facts


def cpp_string_constants(path):
    """{name: value} of the `constexpr const char* kName = "value";` lines of a C++ file, for the text ties."""
    with open(path, encoding='utf-8') as f:
        text = f.read()
    return dict(re.findall(r'\bconst(?:expr)?\s+char\s*\*\s*(k\w+)\s*=\s*"([^"]*)"\s*;', text))


def inline_string_constants(path):
    with open(path, encoding='utf-8') as f:
        text = f.read()
    return dict(re.findall(r'\binline\s+constexpr\s+const\s+char\s*\*\s*(k\w+)\s*=\s*"([^"]*)"\s*;', text))


def sanitise_title(name):
    """The engine's SanitiseForPathComponent (engine/src/platform/ExecutablePath.cpp): trim spaces and dots, map the rest through an allow-list."""
    name = name.strip(' .')
    return ''.join(c if (c.isascii() and (c.isalnum() or c in ' -_')) else '_' for c in name)


def check_ties(repo, facts):
    """The packager's names and the manifest's against the C++ that spells the same names: a drift stops the build."""
    shim = cpp_string_constants(os.path.join(repo, 'game', 'android', 'AndroidMain.cpp'))
    expected = {'kListAsset': LIST_ASSET, 'kContentFolder': CONTENT_FOLDER, 'kEngineFolder': ENGINE_FOLDER, 'kArgsFile': ARGS_FILE}
    for name, value in expected.items():
        if shim.get(name) != value:
            raise PackError('game/android/AndroidMain.cpp says %s = %r and the packager says %r' % (name, shim.get(name), value))
    scratch = inline_string_constants(os.path.join(repo, 'game', 'sim', 'Scratch.hpp'))
    main = cpp_string_constants(os.path.join(repo, 'game', 'main.cpp'))
    for name in ('kUserScratchFolder', 'kLogFileName'):
        if name not in scratch:
            raise PackError('game/sim/Scratch.hpp does not define %s' % name)
    if 'kTitle' not in main:
        raise PackError('game/main.cpp does not define kTitle')
    log = '%s/%s/%s' % (sanitise_title(main['kTitle']), scratch['kUserScratchFolder'], scratch['kLogFileName'])
    if facts['report_log'] != log:
        raise PackError('the manifest names the log %r (supersonic.reportLog) and the game writes %r on a phone' % (facts['report_log'], log))
    if facts['report_skip'] != shim.get('kArgsFile'):
        raise PackError('supersonic.reportSkipWhenFile is %r and the shim reads %r' % (facts['report_skip'], shim.get('kArgsFile')))
    if facts['report_enabled'] != 'true':
        raise PackError('supersonic.reportUnexpectedExit must be true')


# ---- build trees ---------------------------------------------------------------------------------------------------------------------

def read_cache(tree):
    cache = os.path.join(tree, 'CMakeCache.txt')
    if not os.path.isfile(cache):
        raise PackError('%s has no CMakeCache.txt: not a CMake build tree' % tree)
    values = {}
    with open(cache, encoding='utf-8', errors='replace') as f:
        for line in f:
            m = re.match(r'^([A-Za-z0-9_\-]+):[A-Z]+=(.*)$', line.rstrip('\n'))
            if m:
                values[m.group(1)] = m.group(2)
    return values


def check_build_tree(tree, verification):
    """A tree whose cache does not say what a phone's program needs is refused."""
    v = read_cache(tree)
    want = [
        ('CMAKE_BUILD_TYPE', 'Release', 'the native code must be a Release build'),
        ('SUPERSONIC_ENABLE_VALIDATION', 'OFF', 'Vulkan validation is ON: a phone has no validation layers'),
        ('MAGICPORTALS_BAKED_PATHS', 'OFF', 'baked data paths are ON: a program for a phone must carry no checkout path'),
        ('MAGICPORTALS_MOBILE', 'ON', "the phone profile is OFF: the program would ask for Vulkan 1.2, which many phones' stock drivers do not offer"),
        ('MAGICPORTALS_DEV_TOOLS', 'ON' if verification else 'OFF',
         'the developer tools are %s' % ('OFF in a verification build' if verification else 'ON in a player\'s build: that is the verification variant')),
    ]
    for key, value, why in want:
        if v.get(key, '').upper() != value.upper():
            raise PackError('%s: %s=%r, wanted %s (%s)' % (tree, key, v.get(key), value, why))
    if v.get('CMAKE_SYSTEM_NAME', 'Android') != 'Android':
        raise PackError('%s is not an Android build tree (CMAKE_SYSTEM_NAME=%s)' % (tree, v.get('CMAKE_SYSTEM_NAME')))
    return v


# ---- ELF -----------------------------------------------------------------------------------------------------------------------------

def check_elf(path, abi):
    """Facts of an ELF64 shared object, and the problems a phone would have with it. Pure Python: llvm-readelf is the cross-check, not the tool."""
    with open(path, 'rb') as f:
        data = f.read()
    problems = []
    if data[:4] != b'\x7fELF':
        raise PackError('%s is not an ELF file' % path)
    if len(data) < 64 or data[4] != 2 or data[5] != 1:
        raise PackError('%s is not a 64-bit little-endian ELF file' % path)
    e_type, e_machine = struct.unpack_from('<HH', data, 0x10)
    e_phoff, = struct.unpack_from('<Q', data, 0x20)
    e_phentsize, e_phnum = struct.unpack_from('<HH', data, 0x36)
    facts = {'machine': e_machine, 'type': e_type, 'needed': [], 'load_aligns': [], 'rwx': 0, 'textrel': False, 'exec_stack': False}
    if e_type != 3:
        problems.append('not a shared object (ET_DYN): type %d' % e_type)
    if e_machine != ABI_MACHINE[abi]:
        problems.append('machine %d, the ABI %s wants %d' % (e_machine, abi, ABI_MACHINE[abi]))
    loads = []
    dynamic = None
    for i in range(e_phnum):
        off = e_phoff + i * e_phentsize
        p_type, p_flags, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align = struct.unpack_from('<IIQQQQQQ', data, off)
        if p_type == 1:  # PT_LOAD
            loads.append((p_vaddr, p_offset, p_filesz))
            facts['load_aligns'].append(p_align)
            if p_align < MIN_LOAD_ALIGN:
                problems.append('a LOAD segment is aligned to 0x%x, below 0x%x (16 KB pages)' % (p_align, MIN_LOAD_ALIGN))
            if p_flags & 7 == 7:
                facts['rwx'] += 1
                problems.append('a LOAD segment is readable, writable and executable')
        elif p_type == 2:  # PT_DYNAMIC
            dynamic = (p_offset, p_filesz)
        elif p_type == 0x6474E551:  # PT_GNU_STACK
            if p_flags & 1:
                facts['exec_stack'] = True
                problems.append('the stack is executable')
    if dynamic is None:
        problems.append('no dynamic section')
        return facts, problems
    needed_offsets = []
    strtab = None
    off, size = dynamic
    for pos in range(off, off + size, 16):
        tag, val = struct.unpack_from('<qQ', data, pos)
        if tag == 0:
            break
        if tag == 1:  # DT_NEEDED
            needed_offsets.append(val)
        elif tag == 5:  # DT_STRTAB (a virtual address)
            strtab = val
        elif tag == 22:  # DT_TEXTREL
            facts['textrel'] = True
        elif tag == 30 and val & 4:  # DT_FLAGS, DF_TEXTREL
            facts['textrel'] = True
    if facts['textrel']:
        problems.append('text relocations')
    if needed_offsets and strtab is None:
        problems.append('DT_NEEDED without DT_STRTAB')
    elif needed_offsets:
        base = None
        for vaddr, p_offset, filesz in loads:
            if vaddr <= strtab < vaddr + max(filesz, 1):
                base = strtab - vaddr + p_offset
                break
        if base is None:
            problems.append('DT_STRTAB is in no LOAD segment')
        else:
            for n in needed_offsets:
                end = data.index(b'\0', base + n)
                facts['needed'].append(data[base + n:end].decode('ascii', 'replace'))
    for name in facts['needed']:
        if name not in ALLOWED_NEEDED:
            problems.append('NEEDED %s is not on the allow-list %s' % (name, ', '.join(ALLOWED_NEEDED)))
    return facts, problems


# ---- the package folder --------------------------------------------------------------------------------------------------------------

def load_package(folder):
    """[(rel, size, sha256)] of the content folder, sorted: exactly the files its manifest.json lists, each re-hashed. Raises PackError."""
    folder = os.path.abspath(folder)
    reason = forbidden_input_reason(folder)
    if reason:
        raise PackError('the package folder %s is refused: %s' % (folder, reason))
    if os.path.islink(folder) or not os.path.isdir(folder):
        raise PackError('%s is not a plain folder' % folder)
    try:
        with open(os.path.join(folder, pk.MANIFEST), encoding='utf-8') as f:
            manifest = json.load(f)
    except (OSError, ValueError) as e:
        raise PackError('%s/%s cannot be read: %s (run tools/package/package.py content first)' % (folder, pk.MANIFEST, e))
    if manifest.get('tool') != pk.TOOL or manifest.get('format') != 1:
        raise PackError('%s was not written by %s' % (pk.MANIFEST, pk.TOOL))
    root = real(folder)
    listed = {}
    for entry in manifest.get('files', []):
        rel, size, sha = entry.get('path'), entry.get('bytes'), entry.get('sha256')
        why = safe_rel_path(rel) if isinstance(rel, str) else 'the path is not text'
        if why:
            raise PackError('the manifest lists %r: %s' % (rel, why))
        if rel in listed:
            raise PackError('the manifest lists %s twice' % rel)
        top = rel.split('/')[0]
        if top not in pk.DEST.values():
            raise PackError('%s is outside the package layout' % rel)
        never = pk.never_reason(rel[len(top) + 1:])
        if never or rel.lower().endswith(FORBIDDEN_NAME_EXTENSIONS):
            raise PackError('%s is on the never-package list (%s)' % (rel, never or 'a forbidden extension'))
        if not isinstance(size, int) or not isinstance(sha, str) or not re.fullmatch(r'[0-9a-f]{64}', sha):
            raise PackError('the manifest entry for %s is malformed' % rel)
        full = os.path.join(folder, *rel.split('/'))
        if os.path.islink(full):
            raise PackError('%s is a symbolic link' % rel)
        if not os.path.isfile(full):
            raise PackError('listed but missing: %s' % rel)
        if not inside(real(full), root):
            raise PackError('%s resolves outside the package folder' % rel)
        if os.path.getsize(full) != size:
            raise PackError('size differs: %s (%d on disk, %d in the manifest)' % (rel, os.path.getsize(full), size))
        if pk.sha256_of(full) != sha:
            raise PackError('sha256 differs: %s' % rel)
        listed[rel] = (rel, size, sha)
    on_disk = set()
    for d, dirs, files in os.walk(folder):
        dirs.sort()
        for n in files:
            on_disk.add(os.path.relpath(os.path.join(d, n), folder).replace(os.sep, '/'))
    surplus = sorted(on_disk - set(listed) - {pk.MANIFEST, pk.SUMS})
    if surplus:
        raise PackError('in the package folder but not in its manifest: %s%s' % (surplus[0], ' (and %d more)' % (len(surplus) - 1) if len(surplus) > 1 else ''))
    if not listed:
        raise PackError('the package lists no file')
    return sorted(listed.values())


def load_shaders(folder):
    """[(name, size, sha256, path)] of the plain .spv files directly in `folder`."""
    folder = os.path.abspath(folder)
    if not os.path.isdir(folder):
        raise PackError('no shader folder at %s' % folder)
    out = []
    for name in sorted(os.listdir(folder)):
        if not name.endswith('.spv'):
            continue
        full = os.path.join(folder, name)
        if os.path.islink(full):
            raise PackError('%s is a symbolic link' % full)
        if not os.path.isfile(full):
            continue
        out.append((name, os.path.getsize(full), pk.sha256_of(full), full))
    if not out:
        raise PackError('no .spv shader in %s: the engine opens its shaders by name and cannot start without them' % folder)
    return out


# ---- the list ------------------------------------------------------------------------------------------------------------------------

def make_list(entries):
    """(stamp, text) for [(path, size, sha256)] with paths relative to the files folder. The stamp covers the hashes, so it moves with any byte."""
    entries = sorted(entries)
    paths = [e[0] for e in entries]
    if len(set(paths)) != len(paths):
        raise PackError('a path is listed twice')
    h = hashlib.sha256()
    lines = []
    for path, size, sha in entries:
        why = safe_rel_path(path, OWNED_ROOTS)
        if why:
            raise PackError('%r cannot go in the unpack list: %s' % (path, why))
        if len(str(size)) > MOST_SIZE_DIGITS:
            raise PackError('%s is too large for the list' % path)
        h.update(('%s\0%d\0%s\n' % (path, size, sha)).encode('utf-8'))
        lines.append('%d%s%s\n' % (size, SIZE_SEPARATOR, path))
    stamp = h.hexdigest()[:STAMP_CHARS]
    return stamp, stamp + '\n' + ''.join(lines)


def parse_list(text):
    """The packager's own reader of the list (the C++ one is game/sim/Unpack.cpp): (stamp, [(path, size)]). Strict."""
    lines = text.split('\n')
    if lines and lines[-1] == '':
        lines.pop()
    if not lines or not re.fullmatch(r'[0-9A-Za-z._\-]{1,128}', lines[0]):
        raise PackError('the list has no usable stamp')
    out = []
    for n, line in enumerate(lines[1:], start=2):
        if not line:
            continue
        size, sep, path = line.partition(SIZE_SEPARATOR)
        if not sep or not re.fullmatch(r'[0-9]{1,%d}' % MOST_SIZE_DIGITS, size):
            raise PackError('list line %d is malformed' % n)
        why = safe_rel_path(path, OWNED_ROOTS)
        if why:
            raise PackError('list line %d: %s' % (n, why))
        out.append((path, int(size)))
    return lines[0], out


# ---- running tools --------------------------------------------------------------------------------------------------------------------

def run(cmd, what):
    try:
        done = subprocess.run(cmd, capture_output=True, text=True, errors='replace')
    except OSError as e:
        raise PackError('%s could not be run: %s' % (what, e))
    if done.returncode != 0:
        tail = '\n'.join((done.stdout + done.stderr).strip().splitlines()[-15:])
        raise PackError('%s failed (exit %d):\n%s' % (what, done.returncode, tail))
    return done.stdout + done.stderr


def find_java_tool(name):
    home = os.environ.get('JAVA_HOME')
    if home:
        for exe in (name + '.exe', name):
            candidate = os.path.join(home, 'bin', exe)
            if os.path.isfile(candidate):
                return candidate
    found = shutil.which(name)
    if not found:
        raise PackError('no %s: set JAVA_HOME to a JDK 17 or newer' % name)
    return found


class Tools:
    """The SDK's tools, found once. Windows has .exe and .bat names; the jars are run with java, so they are the same everywhere."""

    def __init__(self, sdk, build_tools):
        self.sdk = sdk
        bt = os.path.join(sdk, 'build-tools', build_tools)
        if not os.path.isdir(bt):
            raise PackError('no build-tools %s under %s' % (build_tools, sdk))
        self.bt = bt
        self.exe = '.exe' if os.name == 'nt' else ''
        self.aapt2 = self._need(os.path.join(bt, 'aapt2' + self.exe))
        self.zipalign = self._need(os.path.join(bt, 'zipalign' + self.exe))
        self.d8_jar = self._need(os.path.join(bt, 'lib', 'd8.jar'))
        self.apksigner_jar = self._need(os.path.join(bt, 'lib', 'apksigner.jar'))
        self.java = find_java_tool('java')
        self.javac = find_java_tool('javac')

    @staticmethod
    def _need(path):
        if not os.path.isfile(path):
            raise PackError('missing: %s' % path)
        return path

    def android_jar(self, target_sdk):
        return self._need(os.path.join(self.sdk, 'platforms', 'android-%d' % target_sdk, 'android.jar'))


# ---- the pieces of an APK ------------------------------------------------------------------------------------------------------------

def compile_java(tools, java_src, target_sdk, min_sdk, work):
    """The engine's Java activity -> classes.dex (javac for Java 8 bytecode against android.jar, then d8)."""
    sources = []
    for d, dirs, files in os.walk(java_src):
        dirs.sort()
        sources += [os.path.join(d, n) for n in sorted(files) if n.endswith('.java')]
    if not sources:
        raise PackError('no .java under %s (is the engine submodule checked out?)' % java_src)
    classes = os.path.join(work, 'classes')
    dex = os.path.join(work, 'dex')
    for d in (classes, dex):
        shutil.rmtree(d, ignore_errors=True)
        os.makedirs(d)
    jar = tools.android_jar(target_sdk)
    run([tools.javac, '-source', '8', '-target', '8', '-Xlint:-options', '-encoding', 'UTF-8', '-bootclasspath', jar, '-d', classes] + sources, 'javac')
    class_files = []
    for d, dirs, files in os.walk(classes):
        dirs.sort()
        class_files += [os.path.join(d, n) for n in sorted(files) if n.endswith('.class')]
    run([tools.java, '-cp', tools.d8_jar, 'com.android.tools.r8.D8', '--release', '--min-api', str(min_sdk), '--lib', jar, '--output', dex] + class_files, 'd8')
    out = os.path.join(dex, 'classes.dex')
    if not os.path.isfile(out):
        raise PackError('d8 made no classes.dex')
    return out


def make_icons(repo, work, override):
    """{density: png path} of the launcher icon, and the 512 store icon. Drawn at package time by tools/art/make_app_icon.py, or taken from `override`."""
    if override:
        base = os.path.abspath(override)
    else:
        base = os.path.join(work, 'icons')
        shutil.rmtree(base, ignore_errors=True)
        run([sys.executable, os.path.join(repo, 'tools', 'art', 'make_app_icon.py'), '--out', base], 'tools/art/make_app_icon.py')
    icons = {}
    for density in ('mdpi', 'hdpi', 'xhdpi', 'xxhdpi', 'xxxhdpi'):
        path = os.path.join(base, 'android', 'mipmap-' + density, 'ic_launcher.png')
        if not os.path.isfile(path):
            raise PackError('missing launcher icon %s' % path)
        with open(path, 'rb') as f:
            if f.read(8) != b'\x89PNG\r\n\x1a\n':
                raise PackError('%s is not a PNG' % path)
        icons[density] = path
    store = os.path.join(base, 'android', 'play_store_512.png')
    return icons, store if os.path.isfile(store) else None


def link_base(tools, manifest, icons, facts, version, version_code, debug, work):
    """aapt2: the manifest and the icon, with versionName and versionCode written in. No assets here: they are added by this packager, entry by entry."""
    res = os.path.join(work, 'res')
    shutil.rmtree(res, ignore_errors=True)
    for density, path in sorted(icons.items()):
        d = os.path.join(res, 'mipmap-' + density)
        os.makedirs(d)
        shutil.copyfile(path, os.path.join(d, 'ic_launcher.png'))
    res_zip = os.path.join(work, 'res.zip')
    if os.path.exists(res_zip):
        os.remove(res_zip)
    run([tools.aapt2, 'compile', '--dir', res, '-o', res_zip], 'aapt2 compile')
    base = os.path.join(work, 'base.apk')
    if os.path.exists(base):
        os.remove(base)
    cmd = [tools.aapt2, 'link', '-o', base, '-I', tools.android_jar(facts['targetSdk']), '--manifest', manifest, '-R', res_zip,
           '--version-name', version, '--version-code', str(version_code), '--auto-add-overlay']
    if debug:
        cmd.append('--debug-mode')
    run(cmd, 'aapt2 link')
    return base


def zip_info(name, stored):
    info = zipfile.ZipInfo(name, date_time=ZIP_DATE)
    info.compress_type = zipfile.ZIP_STORED if stored else zipfile.ZIP_DEFLATED
    info.create_system = 3
    info.external_attr = 0o644 << 16
    return info


def is_stored(name):
    return os.path.splitext(name)[1].lower() in STORED_EXTENSIONS


def assemble(base, dex, libs, list_text, content, shaders, package_dir, out_path):
    """The APK before alignment and signing: aapt2's entries as they came, then the dex, the libraries, the list and every asset, sorted."""
    with zipfile.ZipFile(base) as src, zipfile.ZipFile(out_path, 'w') as apk:
        for item in src.infolist():
            info = zipfile.ZipInfo(item.filename, date_time=ZIP_DATE)
            info.compress_type = item.compress_type
            info.create_system = 3
            info.external_attr = 0o644 << 16
            apk.writestr(info, src.read(item.filename), compresslevel=9)
        with open(dex, 'rb') as f:
            apk.writestr(zip_info('classes.dex', False), f.read(), compresslevel=9)
        for abi in sorted(libs):
            with open(libs[abi], 'rb') as f:
                apk.writestr(zip_info('lib/%s/%s' % (abi, LIB_NAME), False), f.read(), compresslevel=9)
        apk.writestr(zip_info('assets/' + LIST_ASSET, False), list_text.encode('utf-8'), compresslevel=9)
        for rel, size, sha in content:
            with open(os.path.join(package_dir, *rel.split('/')), 'rb') as f:
                data = f.read()
            if len(data) != size or hashlib.sha256(data).hexdigest() != sha:
                raise PackError('%s changed while it was being packaged' % rel)
            name = 'assets/%s/%s' % (CONTENT_FOLDER, rel)
            apk.writestr(zip_info(name, is_stored(name)), data, compresslevel=9)
        for name, size, sha, path in shaders:
            with open(path, 'rb') as f:
                data = f.read()
            apk.writestr(zip_info('assets/%s/%s' % (SHADERS_DEST, name), False), data, compresslevel=9)


def debug_keystore(out):
    """The machine's standard debug keystore (what Android Studio signs debug builds with), else one made in the work folder."""
    standard = os.path.join(os.path.expanduser('~'), '.android', 'debug.keystore')
    if os.path.isfile(standard):
        return standard
    made = os.path.join(out, 'debug.keystore')
    if not os.path.isfile(made):
        run([find_java_tool('keytool'), '-genkeypair', '-keystore', made, '-storepass', 'android', '-keypass', 'android', '-alias', 'androiddebugkey',
             '-keyalg', 'RSA', '-keysize', '2048', '-validity', '10000', '-dname', 'CN=Android Debug,O=Android,C=US'], 'keytool')
    return made


def sign(tools, aligned, signed, args, out):
    cmd = [tools.java, '-jar', tools.apksigner_jar, 'sign']
    if args.release:
        cmd += ['--ks', args.keystore, '--ks-key-alias', args.key_alias, '--ks-pass', 'file:' + args.keystore_pass_file]
    else:
        cmd += ['--ks', debug_keystore(out), '--ks-pass', 'pass:android', '--key-pass', 'pass:android', '--ks-key-alias', 'androiddebugkey']
    # No v4 signature: that is an .idsig file beside the APK, for adb's incremental installs only.
    cmd += ['--v4-signing-enabled', 'false', '--out', signed, aligned]
    run(cmd, 'apksigner sign')
    for stray in (signed + '.idsig',):
        if os.path.exists(stray):
            os.remove(stray)


# ---- checking an APK ----------------------------------------------------------------------------------------------------------------

def inspect_apk(apk, tools, repo, release, keystore=None, pass_file=None, key_alias=None, expect_data=None):
    """Runs the static checks on an APK and returns (lines, problems). Everything it prints is something it asked a tool or the zip."""
    lines, problems = [], []
    facts = read_manifest(os.path.join(repo, 'game', 'android', 'AndroidManifest.xml'))
    version = read_version(repo)

    badging = run([tools.aapt2, 'dump', 'badging', apk], 'aapt2 dump badging').replace('\r', '')
    lines.append('--- aapt2 dump badging (selected lines)')
    for line in badging.split('\n'):
        if re.match(r"^\s*(package|sdkVersion|minSdkVersion|targetSdkVersion|uses-feature|uses-permission|uses-implied|native-code|application-debuggable|launchable-activity|application-label)", line):
            lines.append(line.rstrip())
    m = re.search(r"^package: name='([^']*)' versionCode='(\d+)' versionName='([^']*)'", badging, re.M)
    if not m:
        problems.append('badging has no package line')
    else:
        if m.group(1) != facts['package']:
            problems.append('package id %r, the manifest says %r' % (m.group(1), facts['package']))
        if m.group(3) != version:
            problems.append('versionName %r is not the CMake project version %r' % (m.group(3), version))
    if not re.search(r"^(minSdkVersion|sdkVersion):'%d'" % facts['minSdk'], badging, re.M):
        problems.append('minSdk is not %d' % facts['minSdk'])
    if not re.search(r"^targetSdkVersion:'%d'" % facts['targetSdk'], badging, re.M):
        problems.append('targetSdk is not %d' % facts['targetSdk'])
    # aapt2 prints the Vulkan version as a decimal: 0x401000 is 4198400, Vulkan 1.1.
    if not re.search(r"^\s*uses-feature: name='android\.hardware\.vulkan\.version' version='4198400'", badging, re.M):
        problems.append('no required uses-feature android.hardware.vulkan.version 0x401000 (4198400)')
    for optional in ('android.hardware.touchscreen', 'android.hardware.gamepad'):
        if not re.search(r"^\s*uses-feature-not-required: name='%s'" % re.escape(optional), badging, re.M):
            problems.append('%s is not declared as not required' % optional)
    if re.search(r'^\s*uses-permission', badging, re.M):
        problems.append('the APK asks for a permission')
    if 'application-debuggable' in badging:
        if release:
            problems.append('a release APK is debuggable')
    elif not release:
        problems.append('a debug APK is not debuggable')
    native = re.search(r"^native-code: (.*)$", badging, re.M)
    abis = sorted(re.findall(r"'([^']+)'", native.group(1))) if native else []
    lines.append('native-code ABIs: %s' % ' '.join(abis))

    # screenOrientation 0 is SCREEN_ORIENTATION_LANDSCAPE; badging only implies the landscape feature from it.
    xml = run([tools.aapt2, 'dump', 'xmltree', '--file', 'AndroidManifest.xml', apk], 'aapt2 dump xmltree').replace('\r', '')
    if not re.search(r'screenOrientation\(0x0101001e\)=0\s*$', xml, re.M):
        problems.append('the activity is not locked to landscape (screenOrientation 0)')
    if not re.search(r"uses-feature: name='android\.hardware\.screen\.landscape'", badging):
        problems.append('badging does not say the APK needs a landscape screen')
    lines.append('--- manifest (aapt2 dump xmltree): ' + ' '.join(l.strip() for l in xml.split('\n') if 'screenOrientation' in l))

    with zipfile.ZipFile(apk) as z:
        names = z.namelist()
        bad_zip = z.testzip()
        if bad_zip:
            problems.append('the zip entry %s is damaged' % bad_zip)
        libs = sorted(n for n in names if n.startswith('lib/'))
        lines.append('lib entries: %s' % ' '.join(libs))
        got_abis = sorted(n.split('/')[1] for n in libs)
        if abis != got_abis or any(n != 'lib/%s/%s' % (n.split('/')[1], LIB_NAME) for n in libs):
            problems.append('native-code %s does not match the lib/ entries %s' % (abis, got_abis))
        for n in names:
            if n.lower().endswith(FORBIDDEN_NAME_EXTENSIONS) or n.startswith('assets/scenes/') or '/scenes/' in n:
                problems.append('the APK holds a file that must never be packaged: %s' % n)
        assets = {n[len('assets/'):] for n in names if n.startswith('assets/') and not n.endswith('/')}
        if LIST_ASSET not in assets:
            problems.append('assets/%s is missing' % LIST_ASSET)
        else:
            stamp, listed = parse_list(z.read('assets/' + LIST_ASSET).decode('utf-8'))
            lines.append('unpack list: stamp %s, %d files, %d bytes' % (stamp, len(listed), sum(s for _, s in listed)))
            if assets - {LIST_ASSET} != {p for p, _ in listed}:
                extra = sorted(assets - {LIST_ASSET} - {p for p, _ in listed})[:3]
                gone = sorted({p for p, _ in listed} - assets)[:3]
                problems.append('the APK assets are not the list: only in the APK %s, only in the list %s' % (extra, gone))
            for path, size in listed:
                if z.getinfo('assets/' + path).file_size != size:
                    problems.append('the asset %s is not %d bytes' % (path, size))
                    break
            if expect_data is not None and bool([p for p, _ in listed if p.startswith(CONTENT_FOLDER + '/')]) != expect_data:
                problems.append('the APK %s game data' % ('has no' if expect_data else 'has'))
        for n in libs:
            abi = n.split('/')[1]
            tmp = apk + '.elf.tmp'
            with open(tmp, 'wb') as f:
                f.write(z.read(n))
            try:
                facts_elf, elf_problems = check_elf(tmp, abi)
            finally:
                os.remove(tmp)
            lines.append('%s: machine %d, NEEDED %s, LOAD aligns %s, rwx %d, textrel %s' % (
                n, facts_elf['machine'], ' '.join(facts_elf['needed']), ' '.join(hex(a) for a in facts_elf['load_aligns']), facts_elf['rwx'], facts_elf['textrel']))
            problems += ['%s: %s' % (n, p) for p in elf_problems]

    align = run([tools.zipalign, '-c', '-P', '16', '-v', '4', apk], 'zipalign -c')
    lines.append('--- zipalign -c -P 16 -v 4: %s' % align.strip().splitlines()[-1])
    verify = run([tools.java, '-jar', tools.apksigner_jar, 'verify', '--verbose', '--print-certs', apk], 'apksigner verify').replace('\r', '')
    lines.append('--- apksigner verify --print-certs')
    for line in verify.split('\n'):
        if re.match(r'^(Verifies|Verified using|Number of signers|Signer #1 certificate (DN|SHA-256))', line):
            lines.append(line)
    if not re.search(r'^Verifies$', verify, re.M):
        problems.append('apksigner does not say Verifies')
    cert = re.search(r'^Signer #1 certificate SHA-256 digest: ([0-9a-fA-F]+)', verify, re.M)
    if release:
        if os.path.exists(apk + '.idsig'):
            problems.append('an .idsig sits beside a release APK')
        if cert and keystore:
            digest = keystore_sha256(keystore, pass_file, key_alias)
            lines.append('keystore certificate SHA-256: %s' % digest)
            if cert.group(1).lower() != digest:
                problems.append('the APK is signed with %s, not the release key %s' % (cert.group(1).lower(), digest))
        if 'Android Debug' in verify:
            problems.append('a release APK is signed with the debug key')
    return lines, problems


def keystore_sha256(keystore, pass_file, alias):
    out = run([find_java_tool('keytool'), '-list', '-v', '-keystore', keystore, '-storepass:file', pass_file, '-alias', alias], 'keytool -list')
    m = re.search(r'SHA256:\s*([0-9A-Fa-f:]+)', out)
    if not m:
        raise PackError('keytool did not print a SHA256 fingerprint')
    return m.group(1).replace(':', '').lower()


def compare_apk(apk, package_dir, shaders_dir):
    """G3: the APK's unpack list EQUALS the package's manifest.json (same files, sizes, sha256) and the shaders. Returns (lines, problems)."""
    content = {rel: (size, sha) for rel, size, sha in load_package(package_dir)}
    shaders = {name: (size, sha) for name, size, sha, _ in load_shaders(shaders_dir)}
    problems, lines = [], []
    with zipfile.ZipFile(apk) as z:
        stamp, listed = parse_list(z.read('assets/' + LIST_ASSET).decode('utf-8'))
        seen_content, seen_shaders, total = set(), set(), 0
        for path, size in listed:
            data = z.read('assets/' + path)
            sha = hashlib.sha256(data).hexdigest()
            total += len(data)
            if len(data) != size:
                problems.append('%s: the list says %d bytes, the APK entry has %d' % (path, size, len(data)))
            if path.startswith(CONTENT_FOLDER + '/'):
                rel = path[len(CONTENT_FOLDER) + 1:]
                seen_content.add(rel)
                if rel not in content:
                    problems.append('%s is in the APK but not in the package manifest' % rel)
                elif content[rel] != (size, sha):
                    problems.append('%s differs from the package manifest (size %d vs %d, sha256 equal: %s)' % (rel, size, content[rel][0], content[rel][1] == sha))
            elif path.startswith(SHADERS_DEST + '/'):
                name = path[len(SHADERS_DEST) + 1:]
                seen_shaders.add(name)
                if shaders.get(name) != (size, sha):
                    problems.append('shader %s differs from %s' % (name, shaders_dir))
            else:
                problems.append('%s is neither game data nor a shader' % path)
        for rel in sorted(set(content) - seen_content):
            problems.append('%s is in the package manifest but not in the APK list' % rel)
        for name in sorted(set(shaders) - seen_shaders):
            problems.append('shader %s is missing from the APK list' % name)
        lines.append('APK list: stamp %s, %d files (%d game files, %d shaders), %d bytes' % (stamp, len(listed), len(seen_content), len(seen_shaders), total))
        lines.append('package manifest: %d files, %d bytes; shaders folder: %d files' % (len(content), sum(s for s, _ in content.values()), len(shaders)))
        lines.append('APK file size: %d bytes' % os.path.getsize(apk))
    return lines, problems


# ---- build ---------------------------------------------------------------------------------------------------------------------------

def write_sums(out):
    names = sorted(n for n in os.listdir(out) if n.endswith('.apk'))
    with open(os.path.join(out, 'SHA256SUMS.txt'), 'w', encoding='utf-8', newline='\n') as f:
        for n in names:
            f.write('%s  %s\n' % (pk.sha256_of(os.path.join(out, n)), n))
    return names


def cmd_build(args):
    repo = os.path.abspath(args.repo)
    out = os.path.abspath(args.out)
    check_out(out, repo)
    if args.release and args.verification:
        raise PackError('--release with --verification: the verification build has the developer tools in and is never signed with a release key')
    if args.release:
        for need in ('keystore', 'keystore_pass_file', 'key_alias'):
            if not getattr(args, need):
                raise PackError('--release needs --%s' % need.replace('_', '-'))
        default_debug = os.path.normcase(os.path.realpath(os.path.join(os.path.expanduser('~'), '.android', 'debug.keystore')))
        if os.path.normcase(os.path.realpath(args.keystore)) == default_debug:
            raise PackError('--keystore is the debug keystore: a release is never signed with the debug key')
        if not os.path.isfile(args.keystore) or not os.path.isfile(args.keystore_pass_file):
            raise PackError('the release keystore or its password file is not there')
        if args.no_data and not args.allow_data_less_release:
            raise PackError('--release with --no-data would publish an APK with no game in it (--allow-data-less-release if that is meant)')
    if bool(args.package) == bool(args.no_data):
        raise PackError('give exactly one of --package <content folder> and --no-data')
    manifest_path = os.path.join(repo, 'game', 'android', 'AndroidManifest.xml')
    facts = read_manifest(manifest_path)
    check_ties(repo, facts)
    version = read_version(repo)
    libs = {}
    for spec in args.lib or []:
        abi, sep, path = spec.partition('=')
        if not sep or abi not in ABI_MACHINE:
            raise PackError('--lib wants <abi>=<path> with the abi one of %s, got %r' % (', '.join(sorted(ABI_MACHINE)), spec))
        if abi in libs:
            raise PackError('--lib %s is given twice' % abi)
        if not os.path.isfile(path):
            raise PackError('the library for %s is missing: %s' % (abi, path))
        libs[abi] = os.path.abspath(path)
    if not libs:
        raise PackError('no library: give --lib <abi>=<libMagicPortals.so>')
    for abi, path in sorted(libs.items()):
        elf, problems = check_elf(path, abi)
        if problems:
            raise PackError('%s (%s) is not fit for a phone:\n  %s' % (path, abi, '\n  '.join(problems)))
    for tree in args.build_tree or []:
        check_build_tree(os.path.abspath(tree), args.verification)

    shaders = load_shaders(args.shaders or os.path.join(repo, 'engine', 'assets', 'shaders'))
    content = [] if args.no_data else load_package(args.package)
    tools = Tools(os.path.abspath(args.sdk), args.build_tools)
    entries = [('%s/%s' % (CONTENT_FOLDER, rel), size, sha) for rel, size, sha in content]
    entries += [('%s/%s' % (SHADERS_DEST, name), size, sha) for name, size, sha, _ in shaders]
    stamp, list_text = make_list(entries)

    work = os.path.join(out, 'work')
    shutil.rmtree(work, ignore_errors=True)
    os.makedirs(work)
    name = 'MagicPortals-%s%s%s-%s.apk' % (version, '-nodata' if args.no_data else '', '-VERIFICATION' if args.verification else '',
                                           'release' if args.release else 'debug')
    final = os.path.join(out, name)

    dex = compile_java(tools, args.java_src or os.path.join(repo, 'engine', 'src', 'platform', 'android', 'java'), facts['targetSdk'], facts['minSdk'], work)
    icons, store = make_icons(repo, work, args.icons)
    if store:
        shutil.copyfile(store, os.path.join(out, 'play_store_512.png'))
    base = link_base(tools, manifest_path, icons, facts, version, args.version_code, not args.release, work)
    assembled = os.path.join(work, 'assembled.apk')
    assemble(base, dex, libs, list_text, content, shaders, os.path.abspath(args.package) if args.package else None, assembled)
    aligned = os.path.join(work, 'aligned.apk')
    run([tools.zipalign, '-f', '-P', '16', '4', assembled, aligned], 'zipalign')
    signed = os.path.join(work, 'signed.apk')
    sign(tools, aligned, signed, args, out)
    if os.path.exists(final):
        os.remove(final)
    shutil.move(signed, final)

    lines, problems = inspect_apk(final, tools, repo, args.release, args.keystore, args.keystore_pass_file, args.key_alias, expect_data=not args.no_data)
    if args.package:
        more, bad = compare_apk(final, args.package, args.shaders or os.path.join(repo, 'engine', 'assets', 'shaders'))
        lines += more
        problems += bad
    print('\n'.join(lines))
    shutil.rmtree(work, ignore_errors=True)
    if problems:
        print('PROBLEMS:\n  ' + '\n  '.join(problems), file=sys.stderr)
        raise PackError('%s failed its checks (%d problem(s)); it is left at %s' % (name, len(problems), final))
    names = write_sums(out)
    print('APK: %s (%d bytes, versionName %s, versionCode %d, stamp %s)' % (final, os.path.getsize(final), version, args.version_code, stamp))
    print('SHA256SUMS.txt: %s' % ', '.join(names))
    return 0


def cmd_inspect(args):
    repo = os.path.abspath(args.repo) if args.repo else os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    facts = read_manifest(os.path.join(repo, 'game', 'android', 'AndroidManifest.xml'))
    build_tools = args.build_tools or newest_build_tools(args.sdk, facts['targetSdk'])
    tools = Tools(os.path.abspath(args.sdk), build_tools)
    lines, problems = inspect_apk(os.path.abspath(args.apk), tools, repo, args.release, args.keystore, args.keystore_pass_file, args.key_alias)
    print('\n'.join(lines))
    if problems:
        print('PROBLEMS:\n  ' + '\n  '.join(problems), file=sys.stderr)
        return 1
    print('inspect: ok')
    return 0


def newest_build_tools(sdk, target):
    wanted = '%d.0.0' % target
    folder = os.path.join(sdk, 'build-tools')
    if os.path.isdir(os.path.join(folder, wanted)):
        return wanted
    names = [n for n in os.listdir(folder) if re.fullmatch(r'\d+(\.\d+)*', n)]
    if not names:
        raise PackError('no build-tools under %s' % folder)
    return sorted(names, key=lambda n: [int(p) for p in n.split('.')])[-1]


def cmd_compare(args):
    repo = os.path.abspath(args.repo) if args.repo else os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    lines, problems = compare_apk(os.path.abspath(args.apk), args.package, args.shaders or os.path.join(repo, 'engine', 'assets', 'shaders'))
    print('\n'.join(lines))
    if problems:
        print('PROBLEMS:\n  ' + '\n  '.join(problems[:20]), file=sys.stderr)
        return 1
    print('compare: the APK list equals the package manifest (and the shaders)')
    return 0


def cmd_check_out(args):
    check_out(args.out, args.repo)
    return 0


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest='cmd', required=True)
    b = sub.add_parser('build', help='assemble, sign and check an APK')
    b.add_argument('--repo', required=True)
    b.add_argument('--sdk', required=True)
    b.add_argument('--ndk', help='recorded for the log; the libraries are checked as ELF files by this tool')
    b.add_argument('--build-tools', required=True)
    b.add_argument('--out', default=None)
    b.add_argument('--lib', action='append')
    b.add_argument('--package')
    b.add_argument('--no-data', action='store_true')
    b.add_argument('--version-code', type=int, default=1)
    b.add_argument('--verification', action='store_true')
    b.add_argument('--release', action='store_true')
    b.add_argument('--keystore')
    b.add_argument('--keystore-pass-file')
    b.add_argument('--key-alias')
    b.add_argument('--allow-data-less-release', action='store_true')
    b.add_argument('--build-tree', action='append')
    b.add_argument('--shaders')
    b.add_argument('--java-src')
    b.add_argument('--icons')
    i = sub.add_parser('inspect', help='the static checks on an APK')
    i.add_argument('apk')
    i.add_argument('--sdk', required=True)
    i.add_argument('--build-tools')
    i.add_argument('--repo')
    i.add_argument('--release', action='store_true')
    i.add_argument('--keystore')
    i.add_argument('--keystore-pass-file')
    i.add_argument('--key-alias')
    c = sub.add_parser('compare', help="an APK's unpack list against the content package's manifest.json")
    c.add_argument('apk')
    c.add_argument('--package', required=True)
    c.add_argument('--shaders')
    c.add_argument('--repo')
    o = sub.add_parser('check-out', help='refuse an output folder in the tracked tree')
    o.add_argument('--repo', required=True)
    o.add_argument('--out', required=True)
    args = parser.parse_args(argv)
    try:
        if args.cmd == 'build':
            if args.out is None:
                args.out = os.path.join(args.repo, 'out', 'android')
            return cmd_build(args)
        if args.cmd == 'inspect':
            return cmd_inspect(args)
        if args.cmd == 'compare':
            return cmd_compare(args)
        return cmd_check_out(args)
    except PackError as e:
        print('android_package.py: %s' % e, file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
