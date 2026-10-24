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
