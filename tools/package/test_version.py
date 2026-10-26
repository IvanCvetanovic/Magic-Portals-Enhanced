"""Tests of the one version number (tools/package/version.py) and of the check that it is the same everywhere it is written (check_version.py), planning step 208.

Run:   python -m unittest discover -s tools/package -p "test_*.py" -v        (standard library only)

The versionCode rule is pinned by a table; the check is run on the real tree (it must pass) and on a COPY of the tree, one place at a time changed (each place's
mutation must turn the check red and name the place), and restored.
"""
import contextlib
import io
import os
import shutil
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import check_version as cv  # noqa: E402
import version as vs  # noqa: E402

REPO = os.path.dirname(os.path.dirname(HERE))


def write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(text)


class TheRules(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.mkdtemp(prefix='mp-version-test-')
        self.addCleanup(shutil.rmtree, self._tmp, True)

    def cmake(self, text):
        write(os.path.join(self._tmp, 'CMakeLists.txt'), text)
        return self._tmp

    def test_the_version_is_read_from_the_one_definition(self):
        self.assertEqual(vs.read_version(self.cmake('cmake_minimum_required(VERSION 3.20)\nproject(MagicPortals VERSION 1.2.3 LANGUAGES C CXX)\n')), '1.2.3')

    def test_a_comment_that_shows_the_shape_is_not_the_definition(self):
        text = '# project(MagicPortals VERSION 9.9.9 ...) is the shape\nproject(MagicPortals VERSION 1.2.3 LANGUAGES C)\n'
        self.assertEqual(vs.read_version(self.cmake(text)), '1.2.3')

    def test_none_or_two_definitions_are_refused(self):
        for text in ('# nothing\n', 'project(MagicPortals VERSION 1.0.0 LANGUAGES C)\nproject(MagicPortals VERSION 1.0.1 LANGUAGES C)\n',
                     'project(Other VERSION 1.0.0 LANGUAGES C)\n', 'project(MagicPortals VERSION 1.0 LANGUAGES C)\n'):
            with self.assertRaises(vs.VersionError, msg=text):
                vs.read_version(self.cmake(text))
        with self.assertRaises(vs.VersionError):
            vs.read_version(os.path.join(self._tmp, 'no such folder'))

    def test_the_real_definition_is_one_x_y_z(self):
        self.assertRegex(vs.read_version(REPO), r'^\d+\.\d+\.\d+$')

    def test_the_version_code_rule(self):
        # major * 10000 + minor * 100 + patch: every release that goes up in any field gets a larger code.
        for version, code in (('0.1.0', 100), ('0.1.1', 101), ('0.2.0', 200), ('1.0.0', 10000), ('1.2.3', 10203), ('2.0.0', 20000), ('99.99.99', 999999), ('0.0.1', 1)):
            self.assertEqual(vs.version_code(version), code, version)
        order = ['0.0.1', '0.0.9', '0.1.0', '0.9.9', '1.0.0', '1.0.1', '1.1.0', '10.0.0']
        codes = [vs.version_code(v) for v in order]
        self.assertEqual(codes, sorted(set(codes)), 'a later version always has a larger code')

    def test_a_field_the_rule_cannot_carry_has_no_code(self):
        # 0.100.0 would be the code of 1.0.0, and 0.0.100 that of 0.1.0: refused, never given a wrong number.
        for bad in ('0.100.0', '0.0.100', '100.0.0', '1.2', '1.2.3.4', 'x.y.z', '', '1.2.3-rc1', ' 1.2.3'):
            with self.assertRaises(vs.VersionError, msg=bad):
                vs.version_code(bad)

    def test_zero_zero_zero_has_no_code_because_android_needs_at_least_one(self):
        with self.assertRaises(vs.VersionError) as caught:
            vs.version_code('0.0.0')
        self.assertIn('needs at least 1', str(caught.exception))
        self.assertEqual(vs.version_code('0.0.1'), 1, 'the smallest version that has a code')
        self.assertEqual(vs.windows_version('0.0.0'), '0.0.0.0', 'the Windows form has no such floor')
        with contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(vs.main(['code', '--repo', self.cmake('project(MagicPortals VERSION 0.0.0 LANGUAGES C)\n')]), 2)

    def test_a_definition_of_two_or_four_fields_is_refused_by_name_and_never_cut_or_padded(self):
        # CMake accepts one to four fields; the tools are made of three. '1.2.3.4' used to be read as 1.2.3.
        for text, shown in (('project(MagicPortals VERSION 1.2.3.4 LANGUAGES C)\n', '1.2.3.4'), ('project(MagicPortals VERSION 1.2 LANGUAGES C)\n', '1.2'),
                            ('project(MagicPortals VERSION 7 LANGUAGES C)\n', '7'), ('project(MagicPortals\n    VERSION 0.1.0.7\n    LANGUAGES C)\n', '0.1.0.7')):
            with self.assertRaises(vs.VersionError, msg=text) as caught:
                vs.read_version(self.cmake(text))
            self.assertIn('defines the version as %s' % shown, str(caught.exception))
            self.assertIn('exactly three fields', str(caught.exception))
        err = io.StringIO()
        with contextlib.redirect_stderr(err):
            self.assertEqual(vs.main(['version', '--repo', self.cmake('project(MagicPortals VERSION 1.2.3.4 LANGUAGES C)\n')]), 2)
        self.assertIn('exactly three fields', err.getvalue())

    def test_the_windows_form_is_four_fields(self):
        self.assertEqual(vs.windows_version('0.1.0'), '0.1.0.0')
        self.assertEqual(vs.windows_version('12.34.56'), '12.34.56.0')
        with self.assertRaises(vs.VersionError):
            vs.windows_version('1.2')

    def test_the_command_line(self):
        def run(*argv):
            out, err = io.StringIO(), io.StringIO()
            with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
                code = vs.main(list(argv))
            return code, out.getvalue().strip(), err.getvalue()
        version = vs.read_version(REPO)
        self.assertEqual(run(), (0, version, ''))
        self.assertEqual(run('code'), (0, str(vs.version_code(version)), ''))
        self.assertEqual(run('windows'), (0, version + '.0', ''))
        self.assertEqual(run('code', '--repo', self.cmake('project(MagicPortals VERSION 1.2.3 LANGUAGES C)\n')), (0, '10203', ''))
        self.assertEqual(run('nonsense')[0], 2)
        self.assertEqual(run('code', '--repo', self._tmp + '-missing')[0], 2)

    def test_the_android_packager_follows_the_rule_and_keeps_its_error(self):
        import android_package as ap
        self.assertEqual(ap.read_version(REPO), vs.read_version(REPO))
        self.assertEqual(ap.read_version(self.cmake('project(MagicPortals VERSION 7.8.9 LANGUAGES C)' + chr(10))), '7.8.9', 'the Android packager keeps no number of its own')
        self.assertEqual(ap.default_version_code('1.2.3'), 10203)
        with self.assertRaises(ap.PackError):
            ap.read_version(os.path.join(self._tmp, 'no such repository'))
        with self.assertRaises(ap.PackError):
            ap.default_version_code('0.100.0')


class TheCheck(unittest.TestCase):
    """The check on the real tree, and on a copy of it with one place changed at a time."""

    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix='mp-checkversion-test-')
        # A reduced copy: the definition, every file the tables name, the manifest, and one file of each decided hit (the converter's own number, a test's data).
        # The real tree is scanned whole by test_the_real_tree_passes; a copy of all of it for every mutation would only make the tests slow.
        names = {'CMakeLists.txt', cv.MANIFEST, 'tools/converter/pyproject.toml', 'tools/package/test_android_package.py', 'tools/package/version.py',
                 'tools/package/check_version.py', 'tools/apple/test_plists.py'}
        names |= {path for path, *_ in cv.READS} | {path for path, *_ in cv.FOLLOWS}
        for rel in sorted(names):
            src = os.path.join(REPO, *rel.split('/'))
            if not os.path.isfile(src):
                continue  # a file the tables name that does not exist yet is the check's to report, not the copy's
            dst = os.path.join(cls.tmp, *rel.split('/'))
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            shutil.copyfile(src, dst)

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, True)

    def path(self, rel):
        return os.path.join(self.tmp, *rel.split('/'))

    def read(self, rel):
        with open(self.path(rel), 'rb') as f:
            return f.read()

    def mutate(self, rel, old, new, count=1):
        """Change `old` to `new` in the copy (it must be there `count` times), run the check, put the file back byte for byte; returns the problems."""
        original = self.read(rel)
        text = original.decode('utf-8')
        self.assertEqual(text.count(old), count, '%s: %r is not there %d time(s) (the mutation would test nothing)' % (rel, old, count))
        try:
            with open(self.path(rel), 'wb') as f:
                f.write(text.replace(old, new).encode('utf-8'))
            return cv.check(self.tmp)
        finally:
            with open(self.path(rel), 'wb') as f:
                f.write(original)
            self.assertEqual(self.read(rel), original, '%s was not restored byte for byte' % rel)

    def expect(self, problems, *words):
        text = '\n'.join(problems)
        self.assertTrue(problems, 'the check stayed green')
        for word in words:
            self.assertIn(word, text)

    def test_the_real_tree_passes(self):
        self.assertEqual(cv.check(REPO), [])
        self.assertEqual(cv.check(self.tmp), [], 'the copy the mutations start from is clean')
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            self.assertEqual(cv.main(['--repo', REPO]), 0)
        self.assertIn('check_version: ok', out.getvalue())

    def test_a_disagreeing_sentence_is_said_once_in_clear_words(self):
        version = vs.read_version(REPO)
        problems = self.mutate('docs/apple.md', 'the version (`%s`)' % version, 'the version (`9.9.9`)')
        self.expect(problems, 'docs/apple.md: says 9.9.9, the version is %s (' % version)
        problems = self.mutate('docs/packaging.md', '`ProductVersion` %s.0)' % version, '`ProductVersion` 9.9.9.9)')
        self.expect(problems, 'says 9.9.9.9, the Windows form (four fields) is %s.0 (' % version)
        for problem in problems:
            self.assertNotIn('the the', problem)

    def test_the_command_line_names_the_places_and_exits_1(self):
        original = self.read('docs/apple.md')
        try:
            with open(self.path('docs/apple.md'), 'wb') as f:
                f.write(original.replace(b'the version (`', b'the version (`9'))
            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                code = cv.main(['--repo', self.tmp])
            self.assertEqual(code, 1)
            self.assertIn('VERSION: docs/apple.md', out.getvalue())
        finally:
            with open(self.path('docs/apple.md'), 'wb') as f:
                f.write(original)
        with contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(cv.main(['--bogus']), 2)

    def test_a_changed_definition_turns_every_sentence_that_says_the_version_red(self):
        version = vs.read_version(REPO)
        newer = '0.%d.0' % (int(version.split('.')[1]) + 5)
        problems = self.mutate('CMakeLists.txt', 'project(MagicPortals VERSION %s' % version, 'project(MagicPortals VERSION %s' % newer)
        self.expect(problems, 'docs/packaging.md: says %s' % version, 'docs/apple.md: says %s' % version, 'docs/android.md: says %s' % version,
                    'the Windows form (four fields) is %s.0' % newer)

    def test_a_second_or_no_definition_is_red(self):
        version = vs.read_version(REPO)
        line = 'project(MagicPortals VERSION %s' % version
        self.expect(self.mutate('CMakeLists.txt', line, 'project(MagicPortals VERSION %s LANGUAGES C)\n%s' % (version, line)), 'exactly once')
        self.expect(self.mutate('CMakeLists.txt', line, 'project(Other VERSION %s' % version), 'exactly once')

    def test_the_windows_resource_template_must_hand_the_version_on(self):
        self.expect(self.mutate('game/windows/MagicPortals.rc.in', 'VALUE "ProductVersion", "@MAGICPORTALS_RC_VERSION@"', 'VALUE "ProductVersion", "0.1.0.0"'),
                    'game/windows/MagicPortals.rc.in', 'ProductVersion')
        self.expect(self.mutate('game/windows/MagicPortals.rc.in', 'FILEVERSION @MAGICPORTALS_RC_VERSION_COMMAS@', 'FILEVERSION 0,1,0,0'),
                    'game/windows/MagicPortals.rc.in', 'numeric')

    def test_the_windows_padding_must_stay_four_fields(self):
        self.expect(self.mutate('game/windows/MagicPortalsWindows.cmake', 'while (_count LESS 4)', 'while (_count LESS 3)'), 'game/windows/MagicPortalsWindows.cmake')

    def test_game_cmake_must_hand_the_project_version_to_the_windows_function_and_the_bundles(self):
        self.expect(self.mutate('game/CMakeLists.txt', 'magicportals_windows_release(MagicPortals VERSION "${MagicPortals_VERSION}"',
                                'magicportals_windows_release(MagicPortals VERSION "1.0.0"'), 'game/CMakeLists.txt', 'Windows program')
        self.expect(self.mutate('game/CMakeLists.txt', 'set(MAGICPORTALS_BUNDLE_VERSION "${MagicPortals_VERSION}")', 'set(MAGICPORTALS_BUNDLE_VERSION "1.0.0")'),
                    'game/CMakeLists.txt', 'bundles')

    def test_each_plist_template_must_hand_the_version_on(self):
        for rel in ('game/macos/Info.plist.in', 'game/ios/Info.plist.in'):
            short = '<key>CFBundleShortVersionString</key>\n    <string>@MAGICPORTALS_BUNDLE_VERSION@</string>'
            self.expect(self.mutate(rel, short, short.replace('@MAGICPORTALS_BUNDLE_VERSION@', '1.0.0')), rel, 'short version')
            full = '<key>CFBundleVersion</key>\n    <string>@MAGICPORTALS_BUNDLE_VERSION@</string>'
            self.expect(self.mutate(rel, full, full.replace('@MAGICPORTALS_BUNDLE_VERSION@', '1.0.0')), rel, 'bundle version')

    def test_the_android_manifest_must_spell_neither_the_name_nor_the_code(self):
        root = '<manifest xmlns:android="http://schemas.android.com/apk/res/android"\n    package="com.ivancvetanovic.magicportals">'
        for attr in ('versionName="1.0.0"', 'versionCode="7"'):
            problems = self.mutate('game/android/AndroidManifest.xml', root, root[:-1] + '\n    android:' + attr + '>')
            self.expect(problems, 'game/android/AndroidManifest.xml', attr.split('=')[0])

    def test_the_android_packager_and_the_build_script_must_not_keep_a_version_of_their_own(self):
        self.expect(self.mutate('tools/package/android_package.py', 'import version as vs  # noqa: E402', 'import os as vs  # noqa: E402'),
                    'tools/package/android_package.py', 'import version as vs')
        self.expect(self.mutate('tools/package/android_package.py', 'return vs.version_code(version)', 'return 1'), 'tools/package/android_package.py', 'versionCode')
        self.expect(self.mutate('tools/build_android.sh', 'VERSION_CODE=""\n', 'VERSION_CODE=1\n'), 'tools/build_android.sh')

    def test_the_documents_that_say_what_the_version_is_must_follow_it(self):
        version = vs.read_version(REPO)
        self.expect(self.mutate('docs/packaging.md', '`project(MagicPortals VERSION %s ...)`' % version, '`project(MagicPortals VERSION 9.9.9 ...)`'), 'docs/packaging.md: says 9.9.9')
        self.expect(self.mutate('docs/packaging.md', '`ProductVersion` %s.0)' % version, '`ProductVersion` %s.1)' % version), 'docs/packaging.md', 'Windows form')
        self.expect(self.mutate('docs/apple.md', 'the version (`%s`)' % version, 'the version (`9.9.9`)'), 'docs/apple.md: says 9.9.9')
        self.expect(self.mutate('docs/android.md', 'MagicPortals-%s-debug.apk' % version, 'MagicPortals-9.9.9-debug.apk', count=2), 'docs/android.md: says 9.9.9')
        # a sentence that no longer says it is also red (the table must not go quietly stale)
        self.expect(self.mutate('docs/apple.md', 'the version (`%s`)' % version, 'the version'), 'docs/apple.md: 0 place(s)')

    def test_a_new_file_that_spells_the_version_is_red_until_it_is_decided(self):
        version = vs.read_version(REPO)
        path = self.path('tools/new_tool.py')
        write(path, 'VERSION = "%s"\n' % version)
        try:
            self.expect(cv.check(self.tmp), 'tools/new_tool.py:1', 'decide it')
        finally:
            os.remove(path)
        self.assertEqual(cv.check(self.tmp), [])

    def test_the_records_of_history_and_the_converters_own_number_are_not_places(self):
        version = vs.read_version(REPO)
        records = ('DEVLOG.md', 'docs/planning/a-step.md', 'docs/parity-backlog.md', 'engine/README.md', 'out/x.txt')
        for rel in records:
            self.assertFalse(os.path.exists(self.path(rel)), rel)
            write(self.path(rel), 'it said %s then\n' % version)
        try:
            self.assertEqual(cv.check(self.tmp), [])
        finally:
            for rel in records:
                os.remove(self.path(rel))
            shutil.rmtree(self.path('engine'), True)
            shutil.rmtree(self.path('out'), True)
            shutil.rmtree(self.path('docs/planning'), True)
        self.assertTrue(any(a.startswith('tools/converter/') for a, _ in cv.ALLOWED), 'the converter is a decided hit, with a reason')
        self.assertTrue(all(why for _, why in cv.ALLOWED))
        self.assertEqual(cv.check(self.tmp), [])

    def test_only_a_build_tree_or_an_environment_is_skipped_not_a_folder_that_starts_with_build(self):
        version = vs.read_version(REPO)
        scanned_folders = ('tests/build_pins', 'builder', 'buildx', 'build_tools', 'venvs', 'zz')
        skipped_folders = ('build', 'build-rel', 'build-android', 'venv', 'venv-apple', '.venv')
        for folder in scanned_folders + skipped_folders:
            write(self.path(folder + '/x.txt'), 'it says %s\n' % version)
        try:
            problems = cv.check(self.tmp)
            for folder in scanned_folders:
                self.assertTrue(any(p.startswith(folder + '/x.txt:1') for p in problems), '%s was skipped: %s' % (folder, problems))
            for folder in skipped_folders:
                self.assertFalse(any(p.startswith(folder + '/') for p in problems), folder)
            self.assertIn('tests/build_pins/x.txt', cv.scan_files(self.tmp))
        finally:
            for folder in scanned_folders + skipped_folders:
                shutil.rmtree(self.path(folder.split('/')[0]), True)
        self.assertEqual(cv.check(self.tmp), [])
        self.assertTrue(any(p.startswith('tests/build_pins/') for p in cv.scan_files(REPO)), "the real tree's own folder of build pins is read by the scan")

    def test_the_scan_reads_the_files_the_table_names(self):
        scanned = set(cv.scan_files(REPO))
        for path, *_ in cv.READS:
            self.assertTrue(path in scanned, path)
        for path, *_ in cv.FOLLOWS:
            self.assertTrue(path in scanned, path)
        self.assertTrue(cv.MANIFEST in scanned)
        self.assertFalse('DEVLOG.md' in scanned)
        self.assertFalse([p for p in scanned if p.startswith(('engine/', 'out/', 'reference/', 'docs/planning/'))])


if __name__ == '__main__':
    unittest.main()
