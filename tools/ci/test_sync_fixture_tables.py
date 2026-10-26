"""Tests of tools/ci/sync_fixture_tables.py (planning step 208, PORT-10; the row APPLE-20): the tool that keeps the CI fixture's two copies of Enhanced's tables equal to the originals.

Run:   python -m unittest discover -s tools/ci -p "test_*.py" -v        (standard library only)

WHAT KEEPS THE TWO COPIES EQUAL, and what each part does not do:
  * `sync_fixture_tables.py` (write mode) copies data/player.json and data/portals.json over the fixture's, removes a surplus table, leaves the fixture's own role table alone;
  * `sync_fixture_tables.py --check` changes nothing and exits 1 naming every table that differs, is missing or is surplus (what these tests pin);
  * the C++ suite test_mp_cifixture compares the two copies byte for byte at every run of the suites and fails with this tool's name when they drift (tied below: the suite
    names the same two tables).
Nothing runs the tool by itself: the first two are the person's, the third is a suite that runs where the suites run.
Write mode is only ever run here against a temporary tree: the module's paths are pointed there first, and each test asserts it.
"""
import contextlib
import hashlib
import importlib.util
import io
import os
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))


def load_tool():
    spec = importlib.util.spec_from_file_location('sync_fixture_tables_under_test', os.path.join(HERE, 'sync_fixture_tables.py'))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def tree_hashes(root):
    found = {}
    for folder, _, files in os.walk(root):
        for name in files:
            with open(os.path.join(folder, name), 'rb') as f:
                found[os.path.relpath(os.path.join(folder, name), root).replace(os.sep, '/')] = hashlib.sha256(f.read()).hexdigest()
    return found


class Tool(unittest.TestCase):
    def setUp(self):
        self.tool = load_tool()
        self.tmp = tempfile.mkdtemp(prefix='mp-sync-fixture-test-')
        self.addCleanup(shutil.rmtree, self.tmp, True)
        # a temporary repository; the tool's three module-level paths point into it, and nowhere else
        self.tool.REPO = Path(self.tmp)
        self.tool.SOURCE = Path(self.tmp) / 'data'
        self.tool.FIXTURE_DATA = Path(self.tmp) / 'tests' / 'fixtures' / 'ci' / 'data'
        self.put(os.path.join(self.tmp, 'data', 'player.json'), b'{"player": "invented"}\n')
        self.put(os.path.join(self.tmp, 'data', 'portals.json'), b'{"portals": "invented"}\n')
        self.put(os.path.join(self.tmp, 'data', 'game.json'), b'{"not": "copied"}\n')
        self.put(self.fixture('entity_roles.json'), b'{"roles": "the fixture\'s own"}\n')
        self.put(self.fixture('player.json'), b'{"player": "invented"}\n')
        self.put(self.fixture('portals.json'), b'{"portals": "invented"}\n')

    def put(self, path, data):
        self.assertTrue(os.path.abspath(path).startswith(os.path.abspath(self.tmp)), 'a test writes only inside its temporary tree: ' + path)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, 'wb') as f:
            f.write(data)

    def run_tool(self, *argv):
        self.assertTrue(os.path.abspath(str(self.tool.FIXTURE_DATA)).startswith(os.path.abspath(self.tmp)), 'write mode is never run against the tracked folder')
        out = io.StringIO()
        old = sys.argv
        sys.argv = ['sync_fixture_tables.py'] + list(argv)
        try:
            with contextlib.redirect_stdout(out):
                code = self.tool.main()
        finally:
            sys.argv = old
        return code, out.getvalue()

    def fixture(self, name):
        return os.path.join(str(self.tool.FIXTURE_DATA), name)


class TheCheck(Tool):
    def test_fresh_copies_pass_and_nothing_is_written(self):
        before = tree_hashes(self.tmp)
        code, out = self.run_tool('--check')
        self.assertEqual(code, 0, out)
        self.assertIn("the fixture's tables are the repository's", out)
        self.assertEqual(tree_hashes(self.tmp), before)

    def test_a_drifted_copy_is_named_and_the_check_changes_nothing(self):
        self.put(self.fixture('player.json'), b'{"player": "edited by hand"}\n')
        before = tree_hashes(self.tmp)
        code, out = self.run_tool('--check')
        self.assertEqual(code, 1)
        self.assertIn('differs: tests/fixtures/ci/data/player.json', out)
        self.assertNotIn('portals.json', out)
        self.assertIn('1 difference(s): run python tools/ci/sync_fixture_tables.py', out)
        self.assertEqual(tree_hashes(self.tmp), before, '--check never writes')

    def test_a_missing_copy_and_a_surplus_table_are_named_too(self):
        os.remove(self.fixture('portals.json'))
        self.put(self.fixture('stowaway.json'), b'{}\n')
        code, out = self.run_tool('--check')
        self.assertEqual(code, 1)
        self.assertIn('missing: tests/fixtures/ci/data/portals.json', out)
        self.assertIn('surplus: tests/fixtures/ci/data/stowaway.json', out)
        self.assertIn('2 difference(s)', out)

    def test_the_fixtures_own_role_table_is_never_a_surplus_or_a_difference(self):
        self.put(self.fixture('entity_roles.json'), b'{"roles": "changed, and still its own"}\n')
        self.assertEqual(self.run_tool('--check')[0], 0)

    def test_line_endings_are_not_a_difference(self):
        # the working tree may hold CRLF (core.autocrlf) where the repository holds LF: the same table either way
        self.put(self.fixture('player.json'), b'{"player": "invented"}\r\n')
        self.assertEqual(self.run_tool('--check')[0], 0)
        self.put(self.fixture('player.json'), b'{"player": "invented"}\r\n\r\n')
        self.assertEqual(self.run_tool('--check')[0], 1, 'but an extra blank line is a difference')

    def test_a_table_that_is_not_one_of_the_two_in_the_source_is_not_copied(self):
        self.assertEqual(self.run_tool()[0], 0)
        self.assertFalse(os.path.exists(self.fixture('game.json')))


class TheWriteMode(Tool):
    def test_it_refreshes_a_drifted_copy_and_then_the_check_passes(self):
        self.put(self.fixture('player.json'), b'{"player": "edited by hand"}\n')
        code, out = self.run_tool()
        self.assertEqual(code, 0, out)
        self.assertIn('differs: tests/fixtures/ci/data/player.json', out)
        self.assertIn('1 table(s) written or removed', out)
        with open(self.fixture('player.json'), 'rb') as f:
            self.assertEqual(f.read(), b'{"player": "invented"}\n')
        self.assertEqual(self.run_tool('--check')[0], 0)

    def test_it_creates_a_missing_copy_removes_a_surplus_table_and_leaves_the_own_table_alone(self):
        own = tree_hashes(str(self.tool.FIXTURE_DATA))['entity_roles.json']
        os.remove(self.fixture('portals.json'))
        self.put(self.fixture('stowaway.json'), b'{}\n')
        code, out = self.run_tool()
        self.assertEqual(code, 0)
        self.assertTrue(os.path.isfile(self.fixture('portals.json')))
        self.assertFalse(os.path.exists(self.fixture('stowaway.json')))
        self.assertEqual(tree_hashes(str(self.tool.FIXTURE_DATA))['entity_roles.json'], own)
        self.assertEqual(self.run_tool('--check')[0], 0)

    def test_a_run_with_nothing_to_do_says_so_and_writes_nothing(self):
        before = tree_hashes(self.tmp)
        code, out = self.run_tool()
        self.assertEqual((code, out.strip()), (0, 'nothing to do'))
        self.assertEqual(tree_hashes(self.tmp), before)

    def test_it_writes_nowhere_but_the_fixtures_data_folder(self):
        os.remove(self.fixture('player.json'))
        before = {k: v for k, v in tree_hashes(self.tmp).items() if not k.startswith('tests/fixtures/ci/data/')}
        self.assertEqual(self.run_tool()[0], 0)
        after = {k: v for k, v in tree_hashes(self.tmp).items() if not k.startswith('tests/fixtures/ci/data/')}
        self.assertEqual(before, after)


class TheRealTree(unittest.TestCase):
    def test_the_repositorys_fixture_copies_are_the_originals(self):
        tool = load_tool()
        self.assertEqual(os.path.realpath(str(tool.REPO)), os.path.realpath(REPO))
        self.assertTrue(str(tool.FIXTURE_DATA).replace(chr(92), '/').endswith('tests/fixtures/ci/data'))
        out = io.StringIO()
        old = sys.argv
        sys.argv = ['sync_fixture_tables.py', '--check']
        try:
            with contextlib.redirect_stdout(out):
                code = tool.main()
        finally:
            sys.argv = old
        self.assertEqual(code, 0, out.getvalue())

    def test_the_suite_that_compares_them_names_the_same_two_tables(self):
        tool = load_tool()
        with open(os.path.join(REPO, 'tests', 'test_mp_cifixture.cpp'), encoding='utf-8') as f:
            suite = f.read()
        for name in tool.TABLES:
            self.assertIn(name, suite, 'test_mp_cifixture compares %s with the original' % name)
        self.assertIn('sync_fixture_tables.py', suite, 'and fails with this tool\'s name when they drift')
        self.assertEqual(tuple(tool.TABLES), ('player.json', 'portals.json'))
        self.assertEqual(tool.OWN, {'entity_roles.json'})


if __name__ == '__main__':
    unittest.main()
