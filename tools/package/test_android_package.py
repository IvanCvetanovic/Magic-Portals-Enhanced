"""Unit tests of the Android packager (tools/package/android_package.py, planning step 203, PORT-5), on a SYNTHETIC package the tests build.

Run:   python -m unittest discover -s tools/package -p "test_*.py" -v        (standard library only; the Android SDK is NOT needed)

Every byte and name below is invented (`tile_a.png`, `b"level-a"`, a hand-made ELF): no file of the original, no name from it, nothing copied, and no
binary is committed -- the ELF files and the zips are made by the tests, in a temporary folder, from structs and strings. What needs the SDK (aapt2,
zipalign, apksigner, javac, d8) is not exercised here; the Step's gates run the packager on the real tools.

What is pinned: the unpack list (format, stamp, determinism); the path rules, the same as the C++ `SafePath` (a table); the package loader's refusals
(a missing, short, altered or surplus file, a link, a name that climbs, the never-package list, a package under reference/apk); the manifest's policy
(no permission, no spelled version, Vulkan 1.1 required, landscape, all 18 configuration changes handled, and the SDK platform's own list); the text ties to the C++ that spells the
same names; the build-tree cache check; the ELF check on hand-made ELF files; the byte-for-byte determinism of the assembled zip; the comparison of an
APK's list with the package's manifest; the output-folder rule; and the command line's refusals, each with a message and a non-zero exit.
"""
import contextlib
import hashlib
import io
import json
import os
import re
import shutil
import struct
import sys
import tempfile
import unittest
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import android_package as ap  # noqa: E402
import package as pk  # noqa: E402

REPO = os.path.dirname(os.path.dirname(HERE))


def put(path, data):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'wb') as f:
        f.write(data if isinstance(data, bytes) else data.encode('utf-8'))


def sha(data):
    return hashlib.sha256(data).hexdigest()


def make_package(root, files=None):
    """A content folder with a manifest.json as tools/package/package.py writes it, over invented files."""
    files = files or {
        'levels/level_a.tscn': b'[gd_scene]\n',
        'assets/entities/tile_a.png': b'tile-a',
        'tables/chapters.json': b'{"chapters": []}',
        'data/roles.json': b'{}',
        'port-data/engine.json': b'{"mobile": {}}',
        'original/soundfx/boom_a.mp3': b'boom-a',
        'original/sprites/hd/menu_a.png': b'menu-a-hd',
    }
    entries = []
    for rel, data in sorted(files.items()):
        put(os.path.join(root, *rel.split('/')), data)
        entries.append({'path': rel, 'bytes': len(data), 'sha256': sha(data), 'kind': 'invented', 'role': 'required'})
    put(os.path.join(root, 'manifest.json'), json.dumps({'tool': pk.TOOL, 'format': 1, 'files': entries, 'absent': []}, indent=1))
    put(os.path.join(root, 'SHA256SUMS.txt'), ''.join('%s  %s\n' % (e['sha256'], e['path']) for e in entries))
    return root


def make_shaders(root, names=('a_vert.spv', 'a_frag.spv')):
    for n in names:
        put(os.path.join(root, n), b'spirv-' + n.encode())
    put(os.path.join(root, 'a.frag'), b'glsl source: never packaged')
    return root


def make_elf(machine=62, etype=3, aligns=(0x4000, 0x4000), flags=(5, 6), needed=('libc.so', 'libm.so'), textrel=False, stack_flags=6, bits=2):
    """A minimal ELF64 shared object: header, program headers (two LOADs, DYNAMIC, GNU_STACK), a string table and a dynamic section."""
    phnum = 4
    strtab = b'\0' + b''.join(n.encode() + b'\0' for n in needed)
    name_offsets, at = [], 1
    for n in needed:
        name_offsets.append(at)
        at += len(n) + 1
    off_ph = 64
    off_str = off_ph + phnum * 56
    off_dyn = (off_str + len(strtab) + 7) // 8 * 8
    dyn = [(1, o) for o in name_offsets] + [(5, off_str)] + ([(22, 0)] if textrel else []) + [(0, 0)]
    dyn_bytes = b''.join(struct.pack('<qQ', t, v) for t, v in dyn)
    total = off_dyn + len(dyn_bytes)
    ehdr = b'\x7fELF' + bytes([bits, 1, 1, 0]) + b'\0' * 8
    ehdr += struct.pack('<HHIQQQIHHHHHH', etype, machine, 1, 0, off_ph, 0, 0, 64, 56, phnum, 0, 0, 0)
    ph = struct.pack('<IIQQQQQQ', 1, flags[0], 0, 0, 0, total, total, aligns[0])           # LOAD over the whole file, vaddr 0
    ph += struct.pack('<IIQQQQQQ', 1, flags[1], 0, 0x100000, 0x100000, 0, 0, aligns[1])    # a second LOAD, no bytes
    ph += struct.pack('<IIQQQQQQ', 2, 6, off_dyn, off_dyn, off_dyn, len(dyn_bytes), len(dyn_bytes), 8)
    ph += struct.pack('<IIQQQQQQ', 0x6474E551, stack_flags, 0, 0, 0, 0, 0, 16)
    data = ehdr + ph
    data += b'\0' * (off_str - len(data)) + strtab
    data += b'\0' * (off_dyn - len(data)) + dyn_bytes
    return data


class TempCase(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix='mp-android-pack-')
        self.addCleanup(shutil.rmtree, self.tmp, ignore_errors=True)

    def path(self, *parts):
        return os.path.join(self.tmp, *parts)


# ---- the list ------------------------------------------------------------------------------------------------------------------------

class TheList(unittest.TestCase):
    ENTRIES = [('GameData/levels/level_a.tscn', 11, sha(b'a')), ('GameData/assets/tile_a.png', 6, sha(b'b')), ('engine/assets/shaders/a_vert.spv', 12, sha(b'c'))]

    def test_format_and_round_trip(self):
        stamp, text = ap.make_list(self.ENTRIES)
        lines = text.split('\n')
        self.assertEqual(lines[0], stamp)
        self.assertRegex(stamp, r'^[0-9a-f]{32}$')
        self.assertEqual(lines[1:4], ['6\tGameData/assets/tile_a.png', '11\tGameData/levels/level_a.tscn', '12\tengine/assets/shaders/a_vert.spv'])
        self.assertEqual(lines[4], '')
        self.assertTrue(text.endswith('\n') and '\r' not in text)
        back_stamp, back = ap.parse_list(text)
        self.assertEqual(back_stamp, stamp)
        self.assertEqual(back, [('GameData/assets/tile_a.png', 6), ('GameData/levels/level_a.tscn', 11), ('engine/assets/shaders/a_vert.spv', 12)])

    def test_the_stamp_moves_with_any_byte_and_with_nothing_else(self):
        stamp, _ = ap.make_list(self.ENTRIES)
        self.assertEqual(ap.make_list(list(reversed(self.ENTRIES)))[0], stamp, 'the order the entries are given in is not the stamp')
        changed_hash = [(p, s, sha(b'x') if i == 1 else h) for i, (p, s, h) in enumerate(self.ENTRIES)]
        self.assertNotEqual(ap.make_list(changed_hash)[0], stamp, 'a byte of a file that keeps its size and name')
        changed_size = [(p, s + 1 if i == 0 else s, h) for i, (p, s, h) in enumerate(self.ENTRIES)]
        self.assertNotEqual(ap.make_list(changed_size)[0], stamp)
        renamed = [(p + 'x' if i == 2 else p, s, h) for i, (p, s, h) in enumerate(self.ENTRIES)]
        self.assertNotEqual(ap.make_list(renamed)[0], stamp)
        self.assertNotEqual(ap.make_list(self.ENTRIES[:2])[0], stamp, 'a file fewer')

    def test_a_bad_entry_is_refused(self):
        for bad in ('../x', '/abs', 'GameData/../x', 'Magic Portals/scores.json', 'GameData', 'GameData//a', 'GameData/a\tb', 'C:/x', 'GameData\\a', ''):
            with self.assertRaises(ap.PackError, msg=repr(bad)):
                ap.make_list([(bad, 1, sha(b'a'))])
        with self.assertRaises(ap.PackError):
            ap.make_list([('GameData/a', 1, sha(b'a')), ('GameData/a', 1, sha(b'a'))])
        with self.assertRaises(ap.PackError):
            ap.make_list([('GameData/a', 10 ** ap.MOST_SIZE_DIGITS, sha(b'a'))])

    def test_the_reader_is_as_strict_as_the_cpp_one(self):
        for text in ('', '\n1\tGameData/a\n', 'a b\n1\tGameData/a\n', 's\n1 GameData/a\n', 's\n-1\tGameData/a\n', 's\n1\t../a\n', 's\n1\tMagic Portals/x\n', 's\nx\tGameData/a\n'):
            with self.assertRaises(ap.PackError, msg=repr(text)):
                ap.parse_list(text)
        self.assertEqual(ap.parse_list('s\r\n'.replace('\r', '') + '\n3\tGameData/a\n'), ('s', [('GameData/a', 3)]), 'a blank line is skipped')


class ThePathRules(unittest.TestCase):
    """The same table as test_mp_unpack's EveryRefusalOfAPathStandsOnItsOwn: the packager and the game refuse the same names."""
    REFUSED = ['', 'GameData/a\0b', 'GameData/a\nb', 'GameData/a\tb', 'GameData/a\x7f', 'GameData\\a', 'C:/GameData/a', 'C:GameData/a', 'GameData/a:s', '/GameData/a',
               '/etc/passwd', 'GameData/a/', 'GameData//a', 'GameData/./a', './GameData/a', 'GameData/../a', '../GameData/a', 'GameData/a/..', 'GameData/..',
               'Magic Portals/scores.json', 'magicportals_gamedata.stamp', 'magicportals_args.txt', 'gamedata/a', 'GameDataX/a', 'engine2/a', 'GameData', 'engine']
    ACCEPTED = ['GameData/a', 'GameData/levels/level0.tscn', 'GameData/odd name/b c.png', 'engine/assets/shaders/x.spv', 'GameData/a.b.c', 'GameData/.hidden',
                'GameData/caf\u00e9.png', 'GameData/..a', 'GameData/a..b']

    def test_refused(self):
        for p in self.REFUSED:
            self.assertIsNotNone(ap.safe_rel_path(p, ap.OWNED_ROOTS), repr(p))

    def test_accepted(self):
        for p in self.ACCEPTED:
            self.assertIsNone(ap.safe_rel_path(p, ap.OWNED_ROOTS), repr(p))

    def test_the_cpp_table_is_this_table(self):
        """The suite's refusals, read out of tests/test_mp_unpack.cpp, are all refused here too: the two lists cannot drift apart unseen."""
        import re
        with open(os.path.join(REPO, 'tests', 'test_mp_unpack.cpp'), encoding='utf-8') as f:
            text = f.read()
        body = text[text.index('void EveryRefusalOfAPathStandsOnItsOwn'):text.index('// ---- The run')]
        quoted = re.findall(r'refused\("((?:[^"\\]|\\.)*)"', body)
        self.assertGreater(len(quoted), 20)
        for q in quoted:
            if '\\0' in q or '"' in q or q.startswith('std::'):
                continue
            path = q.encode('latin-1').decode('unicode_escape')
            self.assertIsNotNone(ap.safe_rel_path(path, ap.OWNED_ROOTS), repr(path))


# ---- the package folder --------------------------------------------------------------------------------------------------------------

class ThePackage(TempCase):
    def test_a_good_package_loads_sorted(self):
        folder = make_package(self.path('pkg'))
        entries = ap.load_package(folder)
        self.assertEqual([e[0] for e in entries], sorted(e[0] for e in entries))
        self.assertEqual(len(entries), 7)
        self.assertEqual(entries[0][0], 'assets/entities/tile_a.png')
        self.assertEqual(entries[0][1:], (6, sha(b'tile-a')))

    def refuses(self, folder, part):
        with self.assertRaises(ap.PackError) as ctx:
            ap.load_package(folder)
        self.assertIn(part, str(ctx.exception))

    def test_a_missing_short_or_altered_file(self):
        folder = make_package(self.path('pkg'))
        os.remove(os.path.join(folder, 'levels', 'level_a.tscn'))
        self.refuses(folder, 'listed but missing')
        folder = make_package(self.path('pkg2'))
        put(os.path.join(folder, 'tables', 'chapters.json'), b'{"chapters": [1]}')
        self.refuses(folder, 'size differs')
        folder = make_package(self.path('pkg3'))
        put(os.path.join(folder, 'tables', 'chapters.json'), b'{"chapters": []}'.replace(b'c', b'd'))
        self.refuses(folder, 'sha256 differs')

    def test_a_file_the_manifest_does_not_list(self):
        folder = make_package(self.path('pkg'))
        put(os.path.join(folder, 'original', 'sprites', 'stray_a.png'), b'stray')
        self.refuses(folder, 'not in its manifest')

    def test_a_name_that_climbs_or_is_on_the_never_list(self):
        for bad in ('../escape.png', 'levels/../../escape', '/etc/x', 'assets/a\\b'):
            folder = self.path('bad')
            shutil.rmtree(folder, ignore_errors=True)
            make_package(folder)
            with open(os.path.join(folder, 'manifest.json')) as f:
                manifest = json.load(f)
            manifest['files'].append({'path': bad, 'bytes': 1, 'sha256': sha(b'x')})
            put(os.path.join(folder, 'manifest.json'), json.dumps(manifest))
            self.refuses(folder, 'manifest lists')
        for rel in ('original/sprites/x.pvr', 'original/sprites/x.pkm', 'original/scenes/s.esc', 'original/x.exe', 'original/x.dll', 'original/a.as', 'levels/x.apk',
                    'original/build/x.png', 'original/sprites/x.enml'):
            folder = self.path('never')
            shutil.rmtree(folder, ignore_errors=True)
            make_package(folder, {'levels/level_a.tscn': b'x', rel: b'forbidden'})
            self.refuses(folder, 'never-package')

    def test_a_name_outside_the_layout(self):
        folder = make_package(self.path('pkg'), {'levels/level_a.tscn': b'x', 'elsewhere/a.png': b'y'})
        self.refuses(folder, 'outside the package layout')

    def test_a_symbolic_link(self):
        folder = make_package(self.path('pkg'))
        target = self.path('outside.png')
        put(target, b'tile-a')
        link = os.path.join(folder, 'assets', 'entities', 'tile_a.png')
        os.remove(link)
        try:
            os.symlink(target, link)
        except (OSError, NotImplementedError):
            self.skipTest('this system will not make a symbolic link')
        self.refuses(folder, 'symbolic link')

    def test_a_package_under_a_reference_is_refused(self):
        for pair in ('apk', 'ethanon'):
            folder = make_package(self.path('reference', pair, 'pkg'))
            self.refuses(folder, 'reference')

    def test_no_manifest_and_a_manifest_of_another_tool(self):
        os.makedirs(self.path('empty'))
        self.refuses(self.path('empty'), 'cannot be read')
        folder = make_package(self.path('pkg'))
        with open(os.path.join(folder, 'manifest.json')) as f:
            manifest = json.load(f)
        manifest['tool'] = 'somebody else'
        put(os.path.join(folder, 'manifest.json'), json.dumps(manifest))
        self.refuses(folder, 'not written by')

    def test_shaders_are_plain_spv_files_only(self):
        folder = make_shaders(self.path('shaders'))
        names = [s[0] for s in ap.load_shaders(folder)]
        self.assertEqual(names, ['a_frag.spv', 'a_vert.spv'], 'the GLSL source beside them is not taken')
        with self.assertRaises(ap.PackError):
            ap.load_shaders(self.path('nowhere'))
        os.makedirs(self.path('none'))
        put(os.path.join(self.path('none'), 'a.frag'), b'x')
        with self.assertRaises(ap.PackError):
            ap.load_shaders(self.path('none'))


# ---- the manifest and the ties ----------------------------------------------------------------------------------------------------------

class TheManifest(TempCase):
    MANIFEST = os.path.join(REPO, 'game', 'android', 'AndroidManifest.xml')

    def variant(self, old, new):
        with open(self.MANIFEST, encoding='utf-8') as f:
            text = f.read()
        self.assertEqual(text.count(old), 1, old)
        path = self.path('AndroidManifest.xml')
        put(path, text.replace(old, new))
        return path

    def test_the_committed_manifest_keeps_its_policy(self):
        facts = ap.read_manifest(self.MANIFEST)
        self.assertEqual(facts['minSdk'], 26)
        self.assertEqual(facts['targetSdk'], 35)
        self.assertEqual(facts['package'], 'com.ivancvetanovic.magicportals')
        self.assertEqual(facts['lib_name'], 'MagicPortals')
        self.assertNotIn(facts['package'], ('com.ivancvetanovic.penumbra',), "not the sibling port's")

    def test_each_breach_is_refused(self):
        cases = [
            ('<uses-sdk android:minSdkVersion="26" android:targetSdkVersion="35" />', '<uses-sdk android:minSdkVersion="26" android:targetSdkVersion="35" />\n<uses-permission android:name="android.permission.INTERNET" />', 'permissions'),
            ('package="com.ivancvetanovic.magicportals">', 'package="com.ivancvetanovic.magicportals" android:versionName="9.9">', 'versionName'),
            ('android:version="0x401000"', 'android:version="0x402000"', 'vulkan'),
            ('android:screenOrientation="landscape"', 'android:screenOrientation="portrait"', 'landscape'),
            ('android:allowBackup="false"', 'android:allowBackup="true"', 'allowBackup'),
            ('android:hasCode="true"', 'android:hasCode="false"', 'hasCode'),
            ('|fontWeightAdjustment"', '"', 'configuration change'),
            ('<uses-feature android:name="android.hardware.touchscreen" android:required="false" />', '<uses-feature android:name="android.hardware.touchscreen" android:required="true" />', 'touchscreen'),
            ('<meta-data android:name="android.app.lib_name" android:value="MagicPortals" />', '<meta-data android:name="android.app.lib_name" android:value="Other" />', 'lib_name'),
        ]
        for old, new, part in cases:
            with self.assertRaises(ap.PackError, msg=part) as ctx:
                ap.read_manifest(self.variant(old, new))
            self.assertIn(part, str(ctx.exception))

    def test_every_configuration_change_the_platform_defines_is_handled(self):
        # Step 203 listed 14 of the 18 kinds and missed mcc, mnc, fontWeightAdjustment and grammaticalGender (PORT5-9 (a)).
        facts = ap.read_manifest(self.MANIFEST)
        self.assertEqual(len(ap.REQUIRED_CONFIG_CHANGES), 18)
        self.assertEqual(facts['configChanges'], set(ap.REQUIRED_CONFIG_CHANGES), 'the manifest names exactly the 18, no other word')
        with open(self.MANIFEST, encoding='utf-8') as f:
            text = f.read()
        for kind in sorted(ap.REQUIRED_CONFIG_CHANGES):
            start = text.index('android:configChanges="') + len('android:configChanges="')
            end = text.index('"', start)
            kept = [k for k in text[start:end].split('|') if k != kind]
            path = self.path('AndroidManifest.xml')
            put(path, text[:start] + '|'.join(kept) + text[end:])
            with self.assertRaises(ap.PackError, msg=kind) as ctx:
                ap.read_manifest(path)
            self.assertIn(kind, str(ctx.exception), 'the message names the kind that is missing')

    def test_the_sdks_own_platform_file_is_the_same_18_and_a_new_kind_stops_the_build(self):
        sdk = os.environ.get('ANDROID_HOME') or os.environ.get('ANDROID_SDK_ROOT')
        kinds = ap.platform_config_changes(sdk, 35) if sdk else None
        if kinds is not None:
            self.assertEqual(set(kinds), set(ap.REQUIRED_CONFIG_CHANGES), 'the android-35 platform defines exactly the kinds this packager requires')
            self.assertEqual(sorted(kinds.values())[-1], 0x40000000)
        # A made-up platform with a kind the manifest does not name: refused, and the kind is named.
        fake = self.path('sdk')
        values = os.path.join(fake, 'platforms', 'android-35', 'data', 'res', 'values')
        os.makedirs(values)
        flags = ''.join('<flag name="%s" value="0x%x"/>' % (name, 1 << i) for i, name in enumerate(sorted(ap.REQUIRED_CONFIG_CHANGES) + ['newKind']))
        put(os.path.join(values, 'attrs_manifest.xml'), '<resources><attr name="configChanges">%s</attr></resources>' % flags)
        facts = dict(ap.read_manifest(self.MANIFEST))
        with self.assertRaises(ap.PackError) as ctx:
            ap.check_config_changes(facts, fake)
        self.assertIn('newKind', str(ctx.exception))
        put(os.path.join(values, 'attrs_manifest.xml'), '<resources><attr name="configChanges">%s</attr></resources>' % flags.replace('<flag name="newKind"', '<flag name="locale"'))
        self.assertEqual(len(ap.check_config_changes(facts, fake)), 18, 'the same kinds: accepted')
        self.assertIsNone(ap.check_config_changes(facts, self.path('no-sdk-here')), 'an SDK with no platform file is the offline list only')
        put(os.path.join(values, 'attrs_manifest.xml'), '<resources/>')
        with self.assertRaises(ap.PackError):
            ap.check_config_changes(facts, fake)

    def test_the_version_is_the_one_cmake_states(self):
        version = ap.read_version(REPO)
        self.assertRegex(version, r'^\d+\.\d+\.\d+$')
        with open(os.path.join(REPO, 'CMakeLists.txt'), encoding='utf-8') as f:
            self.assertEqual(f.read().count('project(MagicPortals VERSION %s' % version), 1)
        bad = self.path('repo')
        put(os.path.join(bad, 'CMakeLists.txt'), 'project(MagicPortals VERSION 1.0.0 LANGUAGES C)\nproject(MagicPortals VERSION 1.0.1 LANGUAGES C)\n')
        with self.assertRaises(ap.PackError):
            ap.read_version(bad)
        put(os.path.join(bad, 'CMakeLists.txt'), '# no project line\n')
        with self.assertRaises(ap.PackError):
            ap.read_version(bad)

    def test_the_ties_to_the_cpp_hold_today(self):
        ap.check_ties(REPO, ap.read_manifest(self.MANIFEST))

    def test_a_drifted_log_path_or_name_is_refused(self):
        facts = dict(ap.read_manifest(self.MANIFEST))
        facts['report_log'] = 'Magic Portals/magicportals.log'
        with self.assertRaises(ap.PackError) as ctx:
            ap.check_ties(REPO, facts)
        self.assertIn('supersonic.reportLog', str(ctx.exception))
        facts = dict(ap.read_manifest(self.MANIFEST))
        facts['report_skip'] = 'penumbra_args.txt'
        with self.assertRaises(ap.PackError):
            ap.check_ties(REPO, facts)
        facts = dict(ap.read_manifest(self.MANIFEST))
        facts['report_enabled'] = 'false'
        with self.assertRaises(ap.PackError):
            ap.check_ties(REPO, facts)

    def test_the_log_path_is_the_one_the_engine_makes(self):
        # The title goes through the engine's allow-list (a space stays, a dot or a slash does not).
        self.assertEqual(ap.sanitise_title('Magic Portals'), 'Magic Portals')
        self.assertEqual(ap.sanitise_title('  ..Game/Name..  '), 'Game_Name')


def find_aapt2():
    """The newest aapt2 of an Android SDK this machine has with its platform android.jar, or None (then the test that needs it is skipped, and says so)."""
    roots = [os.environ.get('ANDROID_HOME'), os.environ.get('ANDROID_SDK_ROOT'), os.path.join(os.environ.get('LOCALAPPDATA', ''), 'Android', 'Sdk')]
    for root in roots:
        build_tools = os.path.join(root or '', 'build-tools')
        if os.path.isdir(build_tools):
            for version in sorted(os.listdir(build_tools), reverse=True):
                for exe in ('aapt2.exe', 'aapt2'):
                    candidate = os.path.join(build_tools, version, exe)
                    jar = os.path.join(root, 'platforms', 'android-35', 'android.jar')
                    if os.path.isfile(candidate) and os.path.isfile(jar):
                        return candidate, jar
    return None


class ManifestVariants(TempCase):
    """The committed manifest with one thing changed, written to a temporary folder (no test of its own)."""
    MANIFEST = os.path.join(REPO, 'game', 'android', 'AndroidManifest.xml')
    ENGINE = os.path.join(REPO, 'engine')
    KEY_LINE = '            <meta-data android:name="supersonic.reportTitle" android:value="%s" />\n'
    APP_LABEL = '        android:label="Magic Portals Enhanced"\n        android:icon'
    ACTIVITY_LABEL = '            android:name="com.ivancvetanovic.supersonic.SupersonicActivity"\n            android:label="Magic Portals Enhanced"'

    def manifest_with(self, key_value=None, old=None, new=None):
        """The committed manifest, optionally with the key added to the activity and one literal replaced; the path of the copy."""
        with open(self.MANIFEST, encoding='utf-8') as f:
            text = f.read()
        if key_value is not None:
            anchor = '            <meta-data android:name="android.app.lib_name" android:value="MagicPortals" />\n'
            self.assertEqual(text.count(anchor), 1)
            quoted = key_value.replace('&', '&amp;').replace('"', '&quot;').replace('<', '&lt;')
            text = text.replace(anchor, anchor + self.KEY_LINE % quoted)
        if old is not None:
            self.assertEqual(text.count(old), 1, old)
            text = text.replace(old, new)
        path = self.path('AndroidManifest.xml')
        put(path, text)
        return path


class TheReportTitle(ManifestVariants):
    """Planning step 207 (PORT-9): the name the engine's crash report calls the game by. The engine's rule (ReportText.chooseName) is copied into the packager, so that
    inspect can say what the dialog will call the game, and the packager's check refuses a key the engine would read as absent. These pin the copy, hold it to the
    engine's own source and README where the engine is checked out, and hold the typing rule to the real aapt2 where an SDK is present."""
    # ---- the engine's rule, copied -------------------------------------------------------------------------------------------------------------

    def test_the_committed_manifest_names_the_game_by_its_label(self):
        facts = ap.read_manifest(self.MANIFEST)
        self.assertIsNone(facts['report_title'], 'the shipped manifest sets no key: its label already is the name')
        self.assertEqual(facts['label'], 'Magic Portals Enhanced')
        self.assertEqual(ap.report_title_choice(facts), ('Magic Portals Enhanced', "from the application's android:label"))
        self.assertEqual(ap.report_title_line(facts), "the crash report will call the game: Magic Portals Enhanced (from the application's android:label)")

    def test_a_key_wins_over_the_label(self):
        facts = ap.read_manifest(self.manifest_with('Portals Deluxe'))
        self.assertEqual(facts['report_title'], 'Portals Deluxe')
        name, how = ap.report_title_choice(facts)
        self.assertEqual(name, 'Portals Deluxe')
        self.assertIn(ap.REPORT_TITLE_KEY, how)
        self.assertIn('Portals Deluxe', ap.report_title_line(facts))

    def test_names_that_are_plain_strings_pass_the_check_and_are_kept_verbatim(self):
        for name in ('100% "Real" {Game}', '7 Wonders', '3D Pinball', 'Zelda: 2', '2 Fast 2 Furious', '1,5', '5dp', 'yes', 'null', 'tRue', '1.5.', '12abc', "It's",
                     'Ünïcode ✓', ' padded ', '#ab', '#abcde', '0xZZ'):
            facts = ap.read_manifest(self.manifest_with(name))
            self.assertEqual(facts['report_title'], name, name)
            self.assertEqual(ap.report_title_choice(facts)[0], ap.java_trim(name), name)

    def test_the_pure_choice_is_the_engines_chooseName(self):
        c = ap.choose_report_name
        self.assertEqual(c('Key', 'Label'), 'Key')
        self.assertEqual(c(None, 'Label'), 'Label')
        self.assertEqual(c('', 'Label'), 'Label', 'a blank key falls back to the label')
        self.assertEqual(c('   \t', 'Label'), 'Label')
        self.assertEqual(c('\x01\x1f', 'Label'), 'Label', 'Java trim removes every character up to U+0020')
        self.assertEqual(c('  Key  ', 'Label'), 'Key', 'both are trimmed')
        self.assertEqual(c(None, '  Label  '), 'Label')
        self.assertEqual(c(None, None), '', 'nothing names the game: the engine answers with its neutral word')
        self.assertEqual(c('', ''), '')
        self.assertEqual(c(None, '   '), '')
        self.assertEqual(c('  ', None), '')
        self.assertEqual(c(' A  B ', None), 'A  B', 'the inner spacing is kept')

    def test_java_trim_is_not_pythons_strip(self):
        # Python's strip() removes a no-break space and U+3000; Java's trim() does not: a key of those alone is a NAME to the engine (not blank).
        self.assertEqual(ap.java_trim(' '), ' ')
        self.assertEqual(ap.java_trim('　x　'), '　x　')
        self.assertEqual(ap.choose_report_name(' ', 'Label'), ' ')
        self.assertEqual(ap.java_trim(' \x00x\x20'), 'x')
        self.assertEqual(ap.java_trim(None), '')

    def test_the_application_label_is_the_one_asked_not_the_activitys(self):
        path = self.manifest_with(old=self.ACTIVITY_LABEL, new=self.ACTIVITY_LABEL.replace('android:label="Magic Portals Enhanced"', 'android:label="The Activity Label"'))
        self.assertEqual(ap.report_title_choice(ap.read_manifest(path))[0], 'Magic Portals Enhanced', "ApplicationInfo.loadLabel answers the application element's label")
        path = self.manifest_with(old=self.APP_LABEL, new=self.APP_LABEL.replace('Magic Portals Enhanced', 'Application Label'))
        self.assertEqual(ap.report_title_choice(ap.read_manifest(path))[0], 'Application Label')

    def test_a_missing_or_unreadable_label_is_said_and_names_nothing(self):
        facts = ap.read_manifest(self.manifest_with(old=self.APP_LABEL, new='        android:icon'))
        self.assertIsNone(facts['label'])
        name, how = ap.report_title_choice(facts)
        self.assertEqual(name, '')
        self.assertIn('class or package name', how)
        self.assertIn('(no name)', ap.report_title_line(facts))
        for label in ('@string/app_name', '?attr/label'):
            name, how = ap.report_title_choice(ap.read_manifest(self.manifest_with(old=self.APP_LABEL, new=self.APP_LABEL.replace('Magic Portals Enhanced', label))))
            self.assertEqual(name, '', label)
            self.assertIn('resource reference', how, 'a reference is not printed as if it were the name')
        name, how = ap.report_title_choice(ap.read_manifest(self.manifest_with(old=self.APP_LABEL, new=self.APP_LABEL.replace('Magic Portals Enhanced', '   '))))
        self.assertEqual(name, '')
        self.assertIn('blank', how)
        # A blank KEY does not name the game either: the pure rule falls back to the label (the build refuses such a key before it gets here, below).
        self.assertEqual(ap.report_title_choice({'report_title': '  ', 'label': 'L'})[0], 'L')

    # ---- the check -----------------------------------------------------------------------------------------------------------------------------

    def test_each_value_the_compiler_would_store_as_another_type_is_refused_with_the_way_out(self):
        cases = {'12345': 'an integer', '0': 'an integer', '-3': 'an integer', '+7': 'an integer', '0x1F': 'an integer', '0X1f': 'an integer', ' 42 ': 'an integer',
                 'true': 'a boolean', 'false': 'a boolean', 'TRUE': 'a boolean', 'True': 'a boolean', 'FALSE': 'a boolean', ' true ': 'a boolean',
                 '1.5': 'a floating-point number', '.5': 'a floating-point number', '1e3': 'a floating-point number', '1.': 'a floating-point number',
                 '-1.5e-3': 'a floating-point number', '0x1.8p1': 'a floating-point number',
                 '#abc': 'a colour', '#aabbccdd': 'a colour', '#abcdef': 'a colour', '#ABCD': 'a colour',
                 '@null': 'a resource reference', '@string/title': 'a resource reference', '?attr/x': 'a resource reference', '@android:string/ok': 'a resource reference',
                 '99999999999': 'an integer'}  # the last: aapt2 keeps it a string; the rule is deliberately wider (digits only, any length)
        for value, kind in cases.items():
            self.assertEqual(ap.manifest_value_type(value), kind, value)
            with self.assertRaises(ap.PackError, msg=value) as ctx:
                ap.read_manifest(self.manifest_with(value))
            message = str(ctx.exception)
            self.assertIn(ap.REPORT_TITLE_KEY, message)
            self.assertIn(kind, message, 'the message says what the value is read as')
            self.assertIn('ABSENT', message)
            self.assertIn('@string/name', message, 'the string resource is named as a way out')
            self.assertIn('Rename', message, 'and so is a new name')
            self.assertIn('README', message, 'and the engine document that states the rule')

    def test_a_blank_value_is_refused(self):
        for value in ('', ' ', '   ', '\t'):  # a control character below U+0020 other than tab is not well-formed XML, so it cannot reach the packager
            with self.assertRaises(ap.PackError, msg=repr(value)) as ctx:
                ap.read_manifest(self.manifest_with(value))
            self.assertIn('blank', str(ctx.exception))
            self.assertIn(ap.REPORT_TITLE_KEY, str(ctx.exception))

    def test_a_no_break_space_alone_is_a_name_not_a_blank(self):
        # The engine's trim() keeps it, so the engine would show it as the name; the packager does not call it blank.
        self.assertEqual(ap.read_manifest(self.manifest_with(' '))['report_title'], ' ')

    # ---- the built file ------------------------------------------------------------------------------------------------------------------------

    XML_HEAD = 'N: android=http://schemas.android.com/apk/res/android\n  E: manifest (line=2)\n    E: application (line=5)\n      E: activity (line=9)\n'

    def xml(self, *attrs):
        return self.XML_HEAD + '        E: meta-data (line=14)\n' + ''.join('          A: http://schemas.android.com/apk/res/android:%s\n' % a for a in attrs)

    def test_the_built_manifest_is_held_to_the_source(self):
        key = ap.REPORT_TITLE_KEY
        name = 'name(0x01010003)="%s" (Raw: "%s")' % (key, key)

        def string(v):
            return 'value(0x01010024)="%s" (Raw: "%s")' % (v, v)
        absent = {'report_title': None}
        line, problems = ap.check_built_report_title(self.xml('name(0x01010003)="android.app.lib_name" (Raw: "x")', string('MagicPortals')), absent)
        self.assertEqual(problems, [])
        self.assertIn('not set in the APK', line)
        line, problems = ap.check_built_report_title(self.xml(name, string('My Game')), {'report_title': 'My Game'})
        self.assertEqual(problems, [])
        self.assertIn("a string in the APK, 'My Game'", line)
        # The compiler stored it as a number: the engine would read it as absent.
        for stored in ('value(0x01010024)=12345', 'value(0x01010024)=true', 'value(0x01010024)=1.5', 'value(0x01010024)=#ffaabbcc'):
            line, problems = ap.check_built_report_title(self.xml(name, stored), {'report_title': '12345'})
            self.assertEqual(len(problems), 1, stored)
            self.assertIn('not as a string', problems[0])
            self.assertIn('absent', problems[0])
        # The attributes may come in either order.
        self.assertEqual(ap.check_built_report_title(self.xml(string('My Game'), name), {'report_title': 'My Game'})[1], [])
        # The APK holds another name than the source, or one the source does not set, or lacks the one it sets.
        self.assertEqual(len(ap.check_built_report_title(self.xml(name, string('Other')), {'report_title': 'My Game'})[1]), 1)
        self.assertEqual(len(ap.check_built_report_title(self.xml(name, string('Other')), absent)[1]), 1)
        self.assertEqual(len(ap.check_built_report_title(self.xml('name(0x01010003)="x" (Raw: "x")', string('y')), {'report_title': 'My Game'})[1]), 1)
        # A name with quotes, a percent sign and braces is read back whole.
        hard = '100% "Real" {Game}'
        self.assertEqual(ap.check_built_report_title(self.xml(name, string(hard)), {'report_title': hard})[1], [])
        # The value of ANOTHER meta-data is not the key's.
        other = self.xml('name(0x01010003)="android.app.lib_name" (Raw: "x")', 'value(0x01010024)=12345')
        self.assertEqual(ap.check_built_report_title(other, absent)[1], [])

    # ---- the ties to the engine's own words ----------------------------------------------------------------------------------------------------

    def test_the_key_is_the_one_the_engines_activity_reads_and_its_readme_states_the_rule(self):
        folder = os.path.join(self.ENGINE, 'src', 'platform', 'android', 'java', 'com', 'ivancvetanovic', 'supersonic')
        readme = os.path.join(self.ENGINE, 'README.md')
        if not (os.path.isfile(os.path.join(folder, 'ReportText.java')) and os.path.isfile(readme)):
            self.skipTest('the engine submodule is not checked out here, or is at a commit before ReportText')
        with open(os.path.join(folder, 'SupersonicActivity.java'), encoding='utf-8') as f:
            source = f.read()
        self.assertIn('info.metaData.getString("%s")' % ap.REPORT_TITLE_KEY, source, 'the engine reads the key with getString, as a STRING (the check is written for that)')
        with open(os.path.join(folder, 'ReportText.java'), encoding='utf-8') as f:
            rule = f.read()
        self.assertIn('String fromManifest = trimmed(declared);', rule)
        self.assertIn('return trimmed(label);', rule, 'the engine falls back to the label')
        self.assertIn('text == null ? "" : text.trim()', rule, 'and trims with String.trim, as java_trim does')
        with open(readme, encoding='utf-8') as f:
            doc = ' '.join(f.read().split())
        for phrase in ('**The title names the game**', 'manifest meta-data `%s`' % ap.REPORT_TITLE_KEY, 'The key is read as a string',
                       'a value the manifest compiler stores as another type - digits, `true`, `1.5`, a colour such as `#abc` - reads as absent',
                       'android:value="@string/title"'):
            self.assertIn(phrase, doc, 'the engine README no longer says: %s' % phrase)

    def test_the_packagers_copy_names_the_readme_it_copies(self):
        with open(os.path.join(HERE, 'android_package.py'), encoding='utf-8') as f:
            source = f.read()
        self.assertIn('engine/README.md', source)
        self.assertIn('The title names the game', source)
        self.assertIn("THIS PACKAGER'S OWN COPY", source)

    def test_the_typing_rule_is_held_to_the_real_aapt2(self):
        found = find_aapt2()
        if found is None:
            self.skipTest('no Android SDK with aapt2 and platform android-35 here: the typing rule was NOT compared with the real compiler')
        import subprocess
        aapt2, jar = found
        # One manifest with one meta-data per value: each value goes in, and what aapt2 stored is read back out of the one dump.
        values = ['Magic Portals Enhanced', '100% "Real" {Game}', '7 Wonders', '3D Pinball', 'Zelda: 2', 'yes', '1,5', '5dp', '10%', 'null', '1_000', '0x', '0xZZ', '-', '+', '12abc',
                  '1f', '1.0f', '1.5.', '#ab', '#abcde', '# abc', 'tRue', 'fALSE', '0b101', '  padded name  ', ' 42', '42 ', '　42', 'Ünïcode ✓',
                  '12345', '0', '007', '-3', '+7', '0x1F', '0X1f', '-0x1F', ' 123 ', '\t42', 'true', 'TRUE', 'True', 'false', 'False', 'FALSE', ' true ', '1.5', '.5', '1e3', '1.',
                  '1E-3', '-.5', '1.5e-3', '0x1p3', '0x1.8p1', '#abc', '#ABCD', '#aabbccdd', '#abcdef', '@null']
        elements = ''.join('<meta-data android:name="k%d" android:value="%s" />\n' % (i, v.replace('&', '&amp;').replace('"', '&quot;')) for i, v in enumerate(values))
        manifest = self.path('probe.xml')
        put(manifest, '<?xml version="1.0" encoding="utf-8"?>\n<manifest xmlns:android="http://schemas.android.com/apk/res/android" package="com.example.probe">\n'
                      '<uses-sdk android:minSdkVersion="26" android:targetSdkVersion="35" /><application android:label="P"><activity android:name=".A" android:exported="true">\n'
                      + elements + '</activity></application></manifest>\n')
        apk = self.path('probe.apk')
        done = subprocess.run([aapt2, 'link', '-o', apk, '-I', jar, '--manifest', manifest], capture_output=True, text=True, errors='replace')
        self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
        tree = subprocess.run([aapt2, 'dump', 'xmltree', '--file', 'AndroidManifest.xml', apk], capture_output=True, text=True, encoding='utf-8', errors='replace').stdout
        stored = {}
        name = None
        for line in tree.split('\n'):
            m = re.search(r'android:name\(0x01010003\)="(k\d+)"', line)
            if m:
                name = m.group(1)
            if name and 'android:value(0x01010024)=' in line:
                stored[name] = line.split('android:value(0x01010024)=', 1)[1].strip()
                name = None
        self.assertEqual(len(stored), len(values))
        for i, value in enumerate(values):
            aapt_string = ' (Raw: "' in stored['k%d' % i]
            ours = ap.manifest_value_type(value) == 'a string'
            self.assertEqual(ours, aapt_string, '%r: aapt2 stored %s, the packager says %s' % (value, stored['k%d' % i], ap.manifest_value_type(value)))


class TheInspectLine(ManifestVariants):
    """Planning step 207: `inspect` says what the crash report will call the game and holds the built manifest to it. inspect_apk is run on a synthetic APK with the
    SDK's tools stubbed (the real run on the real APKs is the Step's gate), so the line, the built-key check and the label check are pinned where inspect calls them."""
    BADGING = ("package: name='com.ivancvetanovic.magicportals' versionCode='1' versionName='%s'\nsdkVersion:'26'\ntargetSdkVersion:'35'\n"
               "uses-feature: name='android.hardware.vulkan.version' version='4198400'\nuses-feature-not-required: name='android.hardware.touchscreen'\n"
               "uses-feature-not-required: name='android.hardware.gamepad'\nuses-feature: name='android.hardware.screen.landscape'\napplication-debuggable\n"
               "native-code: 'x86_64'\napplication-label:'%s'\n")

    def inspect(self, label='Magic Portals Enhanced', tree_extra='', repo_manifest=None):
        apk = self.path('MagicPortals-0.1.0-debug.apk')
        with zipfile.ZipFile(apk, 'w') as z:
            z.writestr('lib/x86_64/libMagicPortals.so', make_elf())
            z.writestr('assets/gamedata.list', ap.make_list([])[1])
        mask = 0
        for bit in range(32):
            mask |= 1 << bit
        tree = 'E: manifest\n  A: screenOrientation(0x0101001e)=0\n  A: configChanges(0x0101001f)=0x%x\n%s' % (mask, tree_extra)
        answers = {'aapt2 dump badging': self.BADGING % (ap.read_version(REPO), label), 'aapt2 dump xmltree': tree, 'zipalign -c': 'Verification successful\n',
                   'apksigner verify': 'Verifies\nVerified using v2 scheme (APK Signature Scheme v2): true\nSigner #1 certificate DN: CN=Android Debug\n'}
        repo = self.path('repo')
        os.makedirs(os.path.join(repo, 'game', 'android'))
        shutil.copyfile(repo_manifest or self.MANIFEST, os.path.join(repo, 'game', 'android', 'AndroidManifest.xml'))
        shutil.copyfile(os.path.join(REPO, 'CMakeLists.txt'), os.path.join(repo, 'CMakeLists.txt'))

        class FakeTools:
            sdk = self.path('no-sdk')
            aapt2, zipalign, java, apksigner_jar = 'aapt2', 'zipalign', 'java', 'apksigner.jar'
        original = ap.run
        ap.run = lambda cmd, what: answers[what]
        try:
            return ap.inspect_apk(apk, FakeTools(), repo, False)
        finally:
            ap.run = original

    def test_the_line_names_the_game_by_its_label(self):
        lines, problems = self.inspect()
        self.assertIn("the crash report will call the game: Magic Portals Enhanced (from the application's android:label)", lines)
        self.assertEqual(problems, [])

    def test_a_key_is_the_name_and_a_string_in_the_apk_passes(self):
        manifest = self.manifest_with('Portals Deluxe')
        tree = ('E: meta-data\n  A: http://schemas.android.com/apk/res/android:name(0x01010003)="supersonic.reportTitle" (Raw: "supersonic.reportTitle")\n'
                '  A: http://schemas.android.com/apk/res/android:value(0x01010024)="Portals Deluxe" (Raw: "Portals Deluxe")\n')
        lines, problems = self.inspect(tree_extra=tree, repo_manifest=manifest)
        self.assertEqual(problems, [])
        self.assertTrue(any(l.startswith('the crash report will call the game: Portals Deluxe (from the manifest meta-data supersonic.reportTitle)') for l in lines), lines)
        self.assertIn("supersonic.reportTitle: a string in the APK, 'Portals Deluxe'", lines)

    def test_an_apk_that_stores_the_key_as_a_number_is_a_problem(self):
        manifest = self.manifest_with('Portals Deluxe')
        tree = ('E: meta-data\n  A: http://schemas.android.com/apk/res/android:name(0x01010003)="supersonic.reportTitle" (Raw: "supersonic.reportTitle")\n'
                '  A: http://schemas.android.com/apk/res/android:value(0x01010024)=12345\n')
        lines, problems = self.inspect(tree_extra=tree, repo_manifest=manifest)
        self.assertEqual(len([p for p in problems if 'not as a string' in p]), 1, problems)

    def test_an_apk_whose_label_is_not_the_manifests_is_a_problem(self):
        lines, problems = self.inspect(label='Penumbra')
        self.assertEqual(len([p for p in problems if 'application-label' in p]), 1, problems)


class TheSigning(unittest.TestCase):
    def test_no_jar_signature_is_asked_for_and_the_other_options_stay(self):
        # PORT5-9 (b): the minimum SDK is 26 and v2 is read from 24, so v1 is switched off; v4 stays off (no .idsig).
        import types
        seen = []
        real_run, real_keystore = ap.run, ap.debug_keystore
        ap.run = lambda cmd, what: seen.append(list(cmd)) or ''
        ap.debug_keystore = lambda out: 'debug.keystore'
        try:
            tools = types.SimpleNamespace(java='java', apksigner_jar='apksigner.jar')
            ap.sign(tools, 'aligned.apk', 'signed.apk', types.SimpleNamespace(release=False), 'out')
        finally:
            ap.run, ap.debug_keystore = real_run, real_keystore
        self.assertEqual(len(seen), 1)
        cmd = seen[0]
        self.assertEqual(cmd[cmd.index('--v1-signing-enabled') + 1], 'false')
        self.assertEqual(cmd[cmd.index('--v4-signing-enabled') + 1], 'false')
        self.assertEqual(cmd[-1], 'aligned.apk')
        self.assertIn('--ks-key-alias', cmd)


class TheListTie(unittest.TestCase):
    """The unpack list's format is spelled in game/sim/Unpack.hpp and in the packager. Nothing but this ties them (step 203's D2)."""
    HEADER = os.path.join(REPO, 'game', 'sim', 'Unpack.hpp')
    TEST = os.path.join(REPO, 'tests', 'test_mp_unpack.cpp')
    # Four invented files; the list the packager makes for them is in tests/test_mp_unpack.cpp between BEGIN and END PACKAGER LIST.
    FILES = [('GameData/levels/level0.tscn', 1234, sha(b'synthetic level0')), ('GameData/odd name/b c.png', 0, sha(b'')),
             ('engine/assets/shaders/x.spv', 4096, sha(b'synthetic shader')), ('GameData/a/#b', 7, sha(b'synthetic hash'))]

    def constants(self):
        with open(self.HEADER, encoding='utf-8') as f:
            text = f.read()
        import re
        found = {}
        found['separator'] = re.search(r"kSizeSeparator\s*=\s*'(.*)';", text).group(1)
        found['digits'] = int(re.search(r'kMostSizeDigits\s*=\s*(\d+)\s*;', text).group(1))
        found['stamp'] = int(re.search(r'kMostStampChars\s*=\s*(\d+)\s*;', text).group(1))
        # Step 205 (PORT6-8 (a)): the two limits a package must not break. The header spells them as products ("4 * 1024 * 1024"), read here as the product.
        for key, name in (('list_text', 'kMostListTextBytes'), ('block', 'kBlockBytes')):
            expression = re.search(name + r'\s*=\s*([0-9 *]+?)\s*;', text).group(1)
            value = 1
            for factor in expression.split('*'):
                value *= int(factor.strip())
            found[key] = value
        return found

    def test_the_constants_are_the_headers(self):
        found = self.constants()
        self.assertEqual(found['separator'], '\\t', 'the header spells the TAB as a C++ escape')
        self.assertEqual(ap.SIZE_SEPARATOR, '\t')
        self.assertEqual(ap.MOST_SIZE_DIGITS, found['digits'])
        self.assertEqual(ap.MOST_STAMP_CHARS, found['stamp'])
        # The stamp the packager makes is inside the parser's alphabet and length.
        stamp, text = ap.make_list(self.FILES)
        self.assertLessEqual(len(stamp), found['stamp'])
        self.assertRegex(stamp, r'^[0-9A-Za-z._-]+$')
        self.assertEqual(len(stamp), ap.STAMP_CHARS)
        # The reader's limits are the header's: a size of digits+1 digits and a stamp of stamp+1 characters are refused, the longest ones are not.
        ap.parse_list('s\n%s\tGameData/a\n' % ('9' * found['digits']))
        with self.assertRaises(ap.PackError):
            ap.parse_list('s\n%s\tGameData/a\n' % ('9' * (found['digits'] + 1)))
        ap.parse_list('%s\n1\tGameData/a\n' % ('a' * found['stamp']))
        with self.assertRaises(ap.PackError):
            ap.parse_list('%s\n1\tGameData/a\n' % ('a' * (found['stamp'] + 1)))

    def test_the_limits_the_app_holds_a_list_to_are_the_headers(self):
        found = self.constants()
        self.assertEqual(ap.MOST_LIST_TEXT_BYTES, found['list_text'])
        self.assertEqual(ap.BLOCK_BYTES, found['block'])
        self.assertEqual(found['list_text'], 4 * 1024 * 1024)
        self.assertEqual(ap.MOST_UINT64, 2 ** 64 - 1)

    def test_the_packagers_own_synthetic_list_passes_both_limits_and_round_trips(self):
        stamp, text = ap.make_list(self.FILES)
        self.assertEqual(ap.parse_list(text)[0], stamp)
        ap.check_list_limits([size for _, size, _ in self.FILES], len(text.encode('utf-8')), 'the synthetic list')

    def test_a_list_whose_text_passes_the_apps_cap_is_not_built_and_not_read(self):
        # One entry's line is '1\tGameData/' + name + '\n'; names are made as long as a safe path allows, so a few entries fill the cap quickly.
        cap = ap.MOST_LIST_TEXT_BYTES
        base = len(('s' * ap.STAMP_CHARS) + '\n')
        line = lambda pad: '1\tGameData/' + 'a' * pad + '\n'  # noqa: E731
        pad = 4000
        count = (cap - base) // len(line(pad))
        text = ('s' * ap.STAMP_CHARS) + '\n' + ''.join('1\tGameData/%s%d\n' % ('a' * (pad - 6), n) for n in range(count))
        # Top it up to the byte: the cap itself is accepted, one byte past it is refused, on the packager's reader (parse_list) and its limit function.
        short = cap - len(text.encode('utf-8'))
        self.assertGreaterEqual(short, 0)
        exact = text + '1\tGameData/' + 'b' * (short - len('1\tGameData/\n')) + '\n' if short > len('1\tGameData/\n') else text
        self.assertEqual(len(exact.encode('utf-8')), cap)
        ap.parse_list(exact)  # at the cap: taken
        with self.assertRaises(ap.PackError) as caught:
            ap.parse_list(exact + '\n')  # a blank line is ignored by the format, but it is a byte past the cap
        self.assertIn('kMostListTextBytes', str(caught.exception))
        ap.check_list_limits([1], cap, 'x')
        with self.assertRaises(ap.PackError):
            ap.check_list_limits([1], cap + 1, 'x')

    def test_make_list_refuses_a_list_the_app_would_refuse_for_its_text(self):
        # Entries with the longest safe path (the packager has no length limit of its own): 5,000 of them of 900 bytes are over 4 MiB.
        entries = [('GameData/' + ('d%d/' % n) + 'f' * 880 + '%d' % n, 1, sha(b'x%d' % n)) for n in range(5000)]
        with self.assertRaises(ap.PackError) as caught:
            ap.make_list(entries)
        self.assertIn('kMostListTextBytes', str(caught.exception))
        ap.make_list(entries[:100])  # the same shape, far under the cap

    def test_sizes_that_round_up_past_sixty_four_bits_are_not_built_and_not_read(self):
        # The verifier's own probe of step 204 (D4), at the packager: 18,446 files of 999,999,999,999,999 bytes and one of 744,073,709,570,061 add to exactly
        # 2^64 - 1 as they are and to 2^64 + 20,480 once each is rounded up to a block, which the app refuses.
        sizes = [999999999999999] * 18446 + [744073709570061]
        self.assertEqual(sum(sizes), 2 ** 64 - 1)
        with self.assertRaises(ap.PackError) as caught:
            ap.check_list_limits(sizes, 100, 'the probe')
        self.assertIn('64 bits', str(caught.exception))
        ap.check_list_limits(sizes[:-1], 100, 'one less')  # 18,446 of them: under
        text = 's\n' + ''.join('%d\tGameData/f%d\n' % (size, n) for n, size in enumerate(sizes))
        with self.assertRaises(ap.PackError):
            ap.parse_list(text)
        entries = [('GameData/f%d' % n, 999999999999999, sha(b'x')) for n in range(18446)] + [('GameData/g', 744073709570061, sha(b'x'))]
        with self.assertRaises(ap.PackError):
            ap.make_list(entries)

    def test_the_rounding_is_the_apps(self):
        for size, rounded in ((0, 0), (1, 4096), (4096, 4096), (4097, 8192), (999999999999999, 1000000000000000 + (4096 - 1000000000000000 % 4096) % 4096)):
            self.assertEqual(ap.round_up_to_blocks(size), rounded, size)

    def test_the_list_in_the_cpp_suite_is_the_list_the_packager_makes_today(self):
        import re
        with open(self.TEST, encoding='utf-8') as f:
            text = f.read()
        block = text.split('// BEGIN PACKAGER LIST', 1)[1].split('// END PACKAGER LIST', 1)[0]
        pieces = re.findall(r'"((?:[^"\\]|\\.)*)"', block)
        self.assertGreaterEqual(len(pieces), 5)
        literal = ''.join(pieces).replace('\\n', '\n').replace('\\t', '\t')
        self.assertNotIn('\\', literal, 'only the two escapes the packager can make')
        stamp, made = ap.make_list(self.FILES)
        self.assertEqual(literal, made, 'tests/test_mp_unpack.cpp parses what the packager makes: regenerate the literal if the format moved')
        self.assertTrue(literal.startswith(stamp + '\n'))


class TheBuildTree(TempCase):
    GOOD = {'CMAKE_BUILD_TYPE': 'Release', 'SUPERSONIC_ENABLE_VALIDATION': 'OFF', 'MAGICPORTALS_BAKED_PATHS': 'OFF', 'MAGICPORTALS_MOBILE': 'ON',
            'MAGICPORTALS_DEV_TOOLS': 'OFF'}

    def tree(self, name, **changes):
        values = dict(self.GOOD, **changes)
        text = '# a cache\n' + ''.join('%s:%s=%s\n' % (k, 'STRING' if k in ('CMAKE_BUILD_TYPE', 'MAGICPORTALS_DEV_TOOLS') else 'BOOL', v) for k, v in values.items())
        put(self.path(name, 'CMakeCache.txt'), text)
        return self.path(name)

    def test_good_and_each_refusal(self):
        ap.check_build_tree(self.tree('good'), verification=False)
        for key, value, part in (('SUPERSONIC_ENABLE_VALIDATION', 'ON', 'validation'), ('MAGICPORTALS_BAKED_PATHS', 'ON', 'baked'), ('MAGICPORTALS_MOBILE', 'OFF', 'phone profile'),
                                 ('MAGICPORTALS_DEV_TOOLS', 'ON', 'developer tools'), ('CMAKE_BUILD_TYPE', 'Debug', 'Release')):
            with self.assertRaises(ap.PackError, msg=key) as ctx:
                ap.check_build_tree(self.tree('bad-' + key, **{key: value}), verification=False)
            self.assertIn(part, str(ctx.exception))

    def test_the_verification_variant_is_the_other_way_round(self):
        ap.check_build_tree(self.tree('v', MAGICPORTALS_DEV_TOOLS='ON'), verification=True)
        with self.assertRaises(ap.PackError):
            ap.check_build_tree(self.tree('v2'), verification=True)
        with self.assertRaises(ap.PackError):
            ap.check_build_tree(self.tree('v3', SUPERSONIC_ENABLE_VALIDATION='ON', MAGICPORTALS_DEV_TOOLS='ON'), verification=True)

    def test_not_a_build_tree(self):
        os.makedirs(self.path('nothing'))
        with self.assertRaises(ap.PackError):
            ap.check_build_tree(self.path('nothing'), verification=False)

    def test_the_diagnostic_variant_is_its_own_tree_either_way(self):
        # Planning step 205: a tree configured with MAGICPORTALS_PERF_DIAG=ON is a diagnostic build and only that; a tree with it OFF, or from before the option, is not.
        ap.check_build_tree(self.tree('d', MAGICPORTALS_PERF_DIAG='ON'), verification=False, perf_diag=True)
        ap.check_build_tree(self.tree('s', MAGICPORTALS_PERF_DIAG='OFF'), verification=False, perf_diag=False)
        ap.check_build_tree(self.tree('old'), verification=False, perf_diag=False)  # no such key in the cache: the shipped build
        with self.assertRaises(ap.PackError) as ctx:
            ap.check_build_tree(self.tree('d2', MAGICPORTALS_PERF_DIAG='ON'), verification=False, perf_diag=False)
        self.assertIn('MAGICPORTALS_PERF_DIAG', str(ctx.exception))
        with self.assertRaises(ap.PackError):
            ap.check_build_tree(self.tree('s2', MAGICPORTALS_PERF_DIAG='OFF'), verification=False, perf_diag=True)
        with self.assertRaises(ap.PackError):
            ap.check_build_tree(self.tree('old2'), verification=False, perf_diag=True)  # an old tree is not a diagnostic build


class ThePerformanceDiagnosticVariant(unittest.TestCase):
    """Planning step 205: `inspect` reads the variant out of the LIBRARY and holds it to the file name, so that the diagnostic build cannot be taken for the shipped one."""
    MARK = ap.PERF_DIAG_MARKER
    PLAIN = b'\x7fELF some library bytes with no diagnostic string'
    DIAG = b'\x7fELF some library bytes ' + ap.PERF_DIAG_MARKER + b' and more'

    def test_the_shipped_apk_is_not_a_diagnostic_build(self):
        line, problems = ap.check_perf_diag_variant('MagicPortals-0.1.0-debug.apk', {'lib/x86_64/libMagicPortals.so': self.PLAIN, 'lib/arm64-v8a/libMagicPortals.so': self.PLAIN})
        self.assertEqual(problems, [])
        self.assertIn('not a diagnostic build', line)

    def test_the_diagnostic_apk_says_so_in_its_name_and_in_its_inspect_line(self):
        line, problems = ap.check_perf_diag_variant('MagicPortals-0.1.0-PERFDIAG-debug.apk', {'lib/x86_64/libMagicPortals.so': self.DIAG, 'lib/arm64-v8a/libMagicPortals.so': self.DIAG})
        self.assertEqual(problems, [])
        self.assertIn('PERFORMANCE-DIAGNOSTIC', line)
        self.assertIn('TESTER', line)

    def test_a_diagnostic_library_in_a_shipped_name_is_refused(self):
        _, problems = ap.check_perf_diag_variant('MagicPortals-0.1.0-debug.apk', {'lib/x86_64/libMagicPortals.so': self.DIAG})
        self.assertEqual(len(problems), 1)
        self.assertIn('taken for the shipped APK', problems[0])

    def test_a_diagnostic_name_over_a_plain_library_is_refused(self):
        _, problems = ap.check_perf_diag_variant('MagicPortals-0.1.0-PERFDIAG-debug.apk', {'lib/x86_64/libMagicPortals.so': self.PLAIN})
        self.assertEqual(len(problems), 1)
        self.assertIn('carries no diagnostic build', problems[0])

    def test_libraries_that_disagree_are_refused(self):
        _, problems = ap.check_perf_diag_variant('MagicPortals-0.1.0-PERFDIAG-debug.apk', {'lib/x86_64/libMagicPortals.so': self.DIAG, 'lib/arm64-v8a/libMagicPortals.so': self.PLAIN})
        self.assertTrue(problems and 'disagree' in problems[0])

    def test_no_library_is_not_a_variant(self):
        line, problems = ap.check_perf_diag_variant('MagicPortals-0.1.0-debug.apk', {})
        self.assertEqual(len(problems), 1)
        self.assertIn('not read', line)

    def test_the_marker_is_the_line_main_writes_and_only_when_the_option_is_on(self):
        with open(os.path.join(REPO, 'game', 'main.cpp'), encoding='utf-8') as f:
            text = f.read().replace('\r\n', '\n')
        marker = ap.PERF_DIAG_MARKER.decode('ascii')
        self.assertEqual(text.count(marker), 1, 'the string is spelled once in main.cpp')
        before = text[:text.index(marker)]
        self.assertIn('#if MAGICPORTALS_PERF_DIAG', before)
        opened = before.rindex('#if MAGICPORTALS_PERF_DIAG')
        self.assertNotIn('#endif', before[opened:], 'and inside the #if, so a build without the option does not carry it')
        self.assertIn('#endif', text[text.index(marker):])
        self.assertEqual(ap.APK_PERF_DIAG_TAG, '-PERFDIAG')


# ---- the ELF check ----------------------------------------------------------------------------------------------------------------------

class TheElfCheck(TempCase):
    def check(self, abi='x86_64', **kw):
        path = self.path('lib.so')
        put(path, make_elf(**kw))
        return ap.check_elf(path, abi)

    def test_a_good_library(self):
        facts, problems = self.check()
        self.assertEqual(problems, [])
        self.assertEqual(facts['needed'], ['libc.so', 'libm.so'])
        self.assertEqual(facts['machine'], 62)
        facts, problems = self.check('arm64-v8a', machine=183)
        self.assertEqual(problems, [])

    def test_each_problem_is_found(self):
        self.assertTrue(any('machine' in p for p in self.check('arm64-v8a', machine=62)[1]), 'an x86_64 library for the arm64 slot')
        self.assertTrue(any('machine' in p for p in self.check('x86_64', machine=183)[1]))
        self.assertTrue(any('16 KB' in p for p in self.check(aligns=(0x1000, 0x4000))[1]), 'a LOAD aligned to 4 KB')
        self.assertTrue(any('16 KB' in p for p in self.check(aligns=(0x4000, 0x1000))[1]))
        self.assertTrue(any('writable and executable' in p for p in self.check(flags=(7, 6))[1]))
        self.assertTrue(any('text relocation' in p for p in self.check(textrel=True)[1]))
        self.assertTrue(any('libc++_shared.so' in p for p in self.check(needed=('libc.so', 'libc++_shared.so'))[1]))
        self.assertTrue(any('executable' in p for p in self.check(stack_flags=7)[1]))
        self.assertTrue(any('shared object' in p for p in self.check(etype=2)[1]))

    def test_the_allow_list_is_what_the_step_states(self):
        for name in ap.ALLOWED_NEEDED:
            self.assertEqual(self.check(needed=(name,))[1], [], name)

    def test_not_an_elf_file(self):
        put(self.path('a.so'), b'MZ\x90\0' + b'\0' * 100)
        with self.assertRaises(ap.PackError):
            ap.check_elf(self.path('a.so'), 'x86_64')
        put(self.path('b.so'), make_elf(bits=1))
        with self.assertRaises(ap.PackError):
            ap.check_elf(self.path('b.so'), 'x86_64')


# ---- the zip -----------------------------------------------------------------------------------------------------------------------------

def make_base_apk(path):
    with zipfile.ZipFile(path, 'w') as z:
        for name, data, kind in (('AndroidManifest.xml', b'binary-manifest', zipfile.ZIP_DEFLATED), ('resources.arsc', b'arsc' * 8, zipfile.ZIP_STORED),
                                 ('res/mipmap-mdpi/ic_launcher.png', b'\x89PNG', zipfile.ZIP_STORED)):
            z.writestr(zipfile.ZipInfo(name, date_time=(2020, 5, 5, 5, 5, 4)), data, compress_type=kind)


class TheAssembledZip(TempCase):
    def setUp(self):
        super().setUp()
        self.package = make_package(self.path('pkg'))
        self.shaders = make_shaders(self.path('shaders'))
        self.base = self.path('base.apk')
        make_base_apk(self.base)
        put(self.path('classes.dex'), b'dex\n035\0')
        self.libs = {'x86_64': self.path('x.so'), 'arm64-v8a': self.path('a.so')}
        put(self.libs['x86_64'], make_elf())
        put(self.libs['arm64-v8a'], make_elf(machine=183))
        self.content = ap.load_package(self.package)
        self.shader_list = ap.load_shaders(self.shaders)
        entries = [('GameData/' + r, s, h) for r, s, h in self.content] + [('engine/assets/shaders/' + n, s, h) for n, s, h, _ in self.shader_list]
        self.stamp, self.list_text = ap.make_list(entries)

    def build(self, name):
        out = self.path(name)
        ap.assemble(self.base, self.path('classes.dex'), self.libs, self.list_text, self.content, self.shader_list, self.package, out)
        return out

    def test_two_assemblies_are_the_same_bytes(self):
        a, b = self.build('one.apk'), self.build('two.apk')
        with open(a, 'rb') as fa, open(b, 'rb') as fb:
            self.assertEqual(fa.read(), fb.read())

    def test_the_entries_and_their_order(self):
        with zipfile.ZipFile(self.build('a.apk')) as z:
            names = z.namelist()
            self.assertEqual(names[:3], ['AndroidManifest.xml', 'resources.arsc', 'res/mipmap-mdpi/ic_launcher.png'], "aapt2's entries first, as they came")
            self.assertEqual(names[3], 'classes.dex')
            self.assertEqual(names[4:6], ['lib/arm64-v8a/libMagicPortals.so', 'lib/x86_64/libMagicPortals.so'])
            self.assertEqual(names[6], 'assets/gamedata.list')
            assets = [n[len('assets/'):] for n in names[7:]]
            self.assertEqual(assets, ['GameData/' + r for r, _, _ in self.content] + ['engine/assets/shaders/' + n for n, _, _, _ in self.shader_list])
            self.assertEqual(z.getinfo('resources.arsc').compress_type, zipfile.ZIP_STORED, 'resources.arsc stays stored')
            self.assertEqual(z.getinfo('assets/GameData/assets/entities/tile_a.png').compress_type, zipfile.ZIP_STORED, 'a picture is already compressed')
            self.assertEqual(z.getinfo('assets/GameData/levels/level_a.tscn').compress_type, zipfile.ZIP_DEFLATED)
            for info in z.infolist():
                self.assertEqual(info.date_time, ap.ZIP_DATE)
                self.assertEqual(info.create_system, 3)
            self.assertEqual(z.read('assets/gamedata.list').decode(), self.list_text)

    def test_nothing_unlisted_is_in_the_assets(self):
        put(os.path.join(self.shaders, 'stray.txt'), b'not a shader')
        shaders = ap.load_shaders(self.shaders)
        self.assertNotIn('stray.txt', [s[0] for s in shaders])

    def test_a_file_that_changes_while_packaging_is_refused(self):
        put(os.path.join(self.package, 'tables', 'chapters.json'), b'{"chapters": [1]}')
        with self.assertRaises(ap.PackError):
            self.build('changed.apk')

    def test_compare_equal_and_each_difference(self):
        apk = self.build('a.apk')
        lines, problems = ap.compare_apk(apk, self.package, self.shaders)
        self.assertEqual(problems, [])
        self.assertIn('7 game files, 2 shaders', lines[0])
        # an entry altered in the APK (same size, other bytes)
        altered = self.path('altered.apk')
        with zipfile.ZipFile(apk) as src, zipfile.ZipFile(altered, 'w') as dst:
            for item in src.infolist():
                data = src.read(item.filename)
                if item.filename == 'assets/GameData/tables/chapters.json':
                    data = data.replace(b'chapters', b'chapterz')
                dst.writestr(item, data)
        _, problems = ap.compare_apk(altered, self.package, self.shaders)
        self.assertTrue(any('tables/chapters.json differs' in p for p in problems), problems)
        # a shader that changed on disk
        put(os.path.join(self.shaders, 'a_vert.spv'), b'changed-spirv')
        _, problems = ap.compare_apk(apk, self.package, self.shaders)
        self.assertTrue(any('shader a_vert.spv differs' in p for p in problems), problems)
        # a package file the APK does not have
        put(os.path.join(self.shaders, 'a_vert.spv'), b'spirv-a_vert.spv')
        folder = make_package(self.path('pkg-more'), dict({'levels/level_a.tscn': b'[gd_scene]\n'}, **{'data/extra.json': b'{}'}))
        _, problems = ap.compare_apk(apk, folder, self.shaders)
        self.assertTrue(any('is in the APK but not in the package manifest' in p for p in problems) or any('not in the APK list' in p for p in problems), problems)


# ---- the output folder and the command line -----------------------------------------------------------------------------------------------

class TheCommandLine(TempCase):
    def run_main(self, *argv):
        err = io.StringIO()
        out = io.StringIO()
        with contextlib.redirect_stderr(err), contextlib.redirect_stdout(out):
            code = ap.main(list(argv))
        return code, err.getvalue()

    def test_an_output_folder_in_the_tracked_tree_is_refused(self):
        repo = self.path('repo')
        os.makedirs(os.path.join(repo, 'out'))
        os.makedirs(os.path.join(repo, 'tools'))
        os.makedirs(os.path.join(repo, 'reference'))
        code, err = self.run_main('check-out', '--repo', repo, '--out', os.path.join(repo, 'tools', 'apk'))
        self.assertEqual(code, 2)
        self.assertIn('out/', err)
        code, _ = self.run_main('check-out', '--repo', repo, '--out', os.path.join(repo, 'apk'))
        self.assertEqual(code, 2)
        code, _ = self.run_main('check-out', '--repo', repo, '--out', os.path.join(repo, 'out'))
        self.assertEqual(code, 2, 'out/ itself is the converter\'s home, not a product folder')
        code, err = self.run_main('check-out', '--repo', repo, '--out', os.path.join(repo, 'out', 'android'))
        self.assertEqual((code, err), (0, ''))
        code, _ = self.run_main('check-out', '--repo', repo, '--out', self.path('elsewhere', 'apk'))
        self.assertEqual(code, 0, 'outside the repository altogether is the caller\'s business')

    def build_args(self, *extra):
        pkg = make_package(self.path('pkg'))
        lib = self.path('x.so')
        put(lib, make_elf())
        return ['build', '--repo', REPO, '--sdk', self.path('no-sdk'), '--build-tools', '35.0.0', '--out', os.path.join(REPO, 'out', 'android-unit-test'),
                '--lib', 'x86_64=' + lib] + (['--package', pkg] if '--no-data' not in extra else []) + list(extra)

    def test_the_diagnostic_build_is_never_a_release_and_never_the_verification_build(self):
        code, err = self.run_main(*self.build_args('--perf-diag', '--release', '--keystore', 'k', '--keystore-pass-file', 'p', '--key-alias', 'a'))
        self.assertEqual(code, 2)
        self.assertIn('perf-diag', err)
        code, err = self.run_main(*self.build_args('--perf-diag', '--verification'))
        self.assertEqual(code, 2)
        self.assertIn('developer', err)

    def test_release_and_verification_together(self):
        code, err = self.run_main(*self.build_args('--release', '--verification', '--keystore', 'k', '--keystore-pass-file', 'p', '--key-alias', 'a'))
        self.assertEqual(code, 2)
        self.assertIn('verification', err)

    def test_a_release_needs_its_key_and_never_the_debug_one(self):
        code, err = self.run_main(*self.build_args('--release'))
        self.assertEqual(code, 2)
        self.assertIn('--keystore', err)
        debug = os.path.join(os.path.expanduser('~'), '.android', 'debug.keystore')
        # the debug keystore is named but never read: the refusal is on its name before the file is looked at
        code, err = self.run_main(*self.build_args('--release', '--keystore', debug, '--keystore-pass-file', self.path('p.txt'), '--key-alias', 'androiddebugkey'))
        self.assertEqual(code, 2)
        self.assertIn('debug', err)

    def test_data_or_no_data_but_exactly_one(self):
        code, err = self.run_main('build', '--repo', REPO, '--sdk', self.path('no-sdk'), '--build-tools', '35.0.0', '--out', os.path.join(REPO, 'out', 'android-unit-test'),
                                  '--lib', 'x86_64=' + self.path('x.so'))
        self.assertEqual(code, 2)
        both = self.build_args('--no-data')
        both += ['--package', self.path('pkg')]
        code, err = self.run_main(*both)
        self.assertEqual(code, 2)
        self.assertIn('exactly one', err)

    def test_a_missing_or_unknown_library(self):
        code, err = self.run_main('build', '--repo', REPO, '--sdk', self.path('no-sdk'), '--build-tools', '35.0.0', '--out', os.path.join(REPO, 'out', 'android-unit-test'), '--no-data')
        self.assertEqual(code, 2)
        self.assertIn('no library', err)
        code, err = self.run_main('build', '--repo', REPO, '--sdk', self.path('no-sdk'), '--build-tools', '35.0.0', '--out', os.path.join(REPO, 'out', 'android-unit-test'), '--no-data',
                                  '--lib', 'x86_64=' + self.path('missing.so'))
        self.assertEqual(code, 2)
        self.assertIn('missing', err)
        code, err = self.run_main('build', '--repo', REPO, '--sdk', self.path('no-sdk'), '--build-tools', '35.0.0', '--out', os.path.join(REPO, 'out', 'android-unit-test'), '--no-data',
                                  '--lib', 'mips=' + self.path('x.so'))
        self.assertEqual(code, 2)
        self.assertIn('abi', err)

    def test_a_library_for_the_wrong_machine_is_refused_before_anything_is_built(self):
        wrong = self.path('wrong.so')
        put(wrong, make_elf(machine=3))
        code, err = self.run_main('build', '--repo', REPO, '--sdk', self.path('no-sdk'), '--build-tools', '35.0.0', '--out', os.path.join(REPO, 'out', 'android-unit-test'),
                                  '--no-data', '--lib', 'x86_64=' + wrong)
        self.assertEqual(code, 2)
        self.assertIn('not fit for a phone', err)

    def test_a_tree_with_validation_on_is_refused(self):
        lib = self.path('x.so')
        put(lib, make_elf())
        cache = self.path('tree', 'CMakeCache.txt')
        put(cache, 'CMAKE_BUILD_TYPE:STRING=Release\nSUPERSONIC_ENABLE_VALIDATION:BOOL=ON\nMAGICPORTALS_BAKED_PATHS:BOOL=OFF\nMAGICPORTALS_MOBILE:BOOL=ON\nMAGICPORTALS_DEV_TOOLS:STRING=OFF\n')
        code, err = self.run_main('build', '--repo', REPO, '--sdk', self.path('no-sdk'), '--build-tools', '35.0.0', '--out', os.path.join(REPO, 'out', 'android-unit-test'),
                                  '--no-data', '--lib', 'x86_64=' + lib, '--build-tree', self.path('tree'))
        self.assertEqual(code, 2)
        self.assertIn('validation', err)

    def test_a_package_under_reference_apk_is_refused(self):
        lib = self.path('x.so')
        put(lib, make_elf())
        folder = make_package(self.path('reference', 'apk', 'pkg'))
        code, err = self.run_main('build', '--repo', REPO, '--sdk', self.path('no-sdk'), '--build-tools', '35.0.0', '--out', os.path.join(REPO, 'out', 'android-unit-test'),
                                  '--package', folder, '--lib', 'x86_64=' + lib)
        self.assertEqual(code, 2)
        self.assertIn('reference/apk', err)

    def test_nothing_is_written_into_the_repository_by_the_refusals(self):
        self.assertFalse(os.path.exists(os.path.join(REPO, 'out', 'android-unit-test')), 'the refusals above stopped before they made the output folder')


if __name__ == '__main__':
    unittest.main()
