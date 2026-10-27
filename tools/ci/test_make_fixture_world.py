"""Tests of tools/ci/make_fixture_world.py (planning step 209, PORT-11): the synthetic world a CI run builds a content package from.

Run:   python -m unittest discover -s tools/ci -p "test_*.py" -v        (standard library only)

What is pinned: the world holds the fixture level, its tables, the port's own tables and two invented stubs and nothing else; no file in it has the name of a picture, a sound or
a level of the original; the content packager accepts it and its own verify passes on the result; and the tool refuses an output folder that is not empty or that lies in the
repository's tracked sources.
"""
import contextlib
import importlib.util
import io
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))


def load_tool():
    spec = importlib.util.spec_from_file_location('make_fixture_world_under_test', os.path.join(HERE, 'make_fixture_world.py'))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


tool = load_tool()
# What a file of the original, or of anyone's art, is called (the hard rule's list): none may be a name in the world.
FORBIDDEN_SUFFIXES = ('.png', '.bmp', '.jpg', '.jpeg', '.gif', '.webp', '.ico', '.mp3', '.ogg', '.wav', '.esc', '.ent', '.enml', '.par', '.fnt', '.pvr', '.pkm')


class TheWorld(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix='mp-world-test-')
        cls.out = os.path.join(cls.tmp, 'world')
        cls.repo, cls.ref = tool.build(cls.out)

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def files(self, root):
        found = []
        for folder, _, names in os.walk(root):
            for n in names:
                found.append(os.path.relpath(os.path.join(folder, n), root).replace(os.sep, '/'))
        return sorted(found)

    def test_it_holds_the_fixture_the_port_tables_and_two_stubs(self):
        repo_files = self.files(self.repo)
        self.assertIn('out/levels/ci_level.tscn', repo_files)
        self.assertIn('out/data/chapters.json', repo_files)
        for table in ('entity_roles.json', 'player.json', 'portals.json'):
            self.assertIn('data/' + table, repo_files)
        port = sorted('game/data/' + n for n in os.listdir(os.path.join(REPO, 'game', 'data')))
        self.assertEqual([n for n in repo_files if n.startswith('game/data/')], port, 'the port\'s own tables, all of them and nothing else')
        self.assertEqual(self.files(self.ref), ['extracted/assets/sprites/stub_b.bin_png'])
        self.assertIn('out/assets/entities/stub_a.bin_png', repo_files)

    def test_the_stubs_say_they_are_invented(self):
        for path in (os.path.join(self.repo, 'out', 'assets', 'entities', 'stub_a.bin_png'), os.path.join(self.ref, 'extracted', 'assets', 'sprites', 'stub_b.bin_png')):
            with open(path, 'rb') as f:
                data = f.read()
            self.assertIn(b'invented', data)
            self.assertIn(b'not from the original', data)

    def test_no_name_in_it_is_a_picture_a_sound_or_a_file_of_the_original(self):
        for root in (self.repo, self.ref):
            for name in self.files(root):
                self.assertFalse(name.lower().endswith(FORBIDDEN_SUFFIXES), name)

    def test_the_content_packager_accepts_it_and_verifies_the_result(self):
        content = os.path.join(self.tmp, 'content')
        script = os.path.join(REPO, 'tools', 'package', 'package.py')
        done = subprocess.run([sys.executable, '-I', '-B', script, 'content', '--repo', self.repo, '--reference', self.ref, '--out', content], capture_output=True, text=True)
        self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
        done = subprocess.run([sys.executable, '-I', '-B', script, 'verify', content], capture_output=True, text=True)
        self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
        self.assertTrue(os.path.isfile(os.path.join(content, 'levels', 'ci_level.tscn')))
        self.assertTrue(os.path.isfile(os.path.join(content, 'manifest.json')))


class TheRefusals(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix='mp-world-test-')
        self.addCleanup(shutil.rmtree, self.tmp, True)

    def test_an_output_folder_that_is_not_empty_is_refused_and_left_alone(self):
        out = os.path.join(self.tmp, 'x')
        os.makedirs(out)
        with open(os.path.join(out, 'mine.txt'), 'w') as f:
            f.write('keep')
        with self.assertRaises(tool.WorldError):
            tool.build(out)
        self.assertEqual(os.listdir(out), ['mine.txt'])

    def test_an_empty_existing_folder_is_fine(self):
        out = os.path.join(self.tmp, 'empty')
        os.makedirs(out)
        tool.build(out)
        self.assertTrue(os.path.isdir(os.path.join(out, 'repo')))

    def test_the_trackeds_sources_are_never_written_into(self):
        for sub in ('game', 'tests', 'tools', 'docs', 'data', 'engine', os.path.join('tools', 'ci', 'world')):
            with self.assertRaises(tool.WorldError, msg=sub):
                tool.build(os.path.join(REPO, sub, 'zz_world_must_not_exist'))
            self.assertFalse(os.path.exists(os.path.join(REPO, sub, 'zz_world_must_not_exist')), sub)

    def test_a_folder_that_is_not_a_checkout_is_refused(self):
        with self.assertRaises(tool.WorldError):
            tool.build(os.path.join(self.tmp, 'w'), repo=self.tmp)

    def test_the_command_line_says_so_and_exits_2(self):
        err = io.StringIO()
        with contextlib.redirect_stderr(err):
            code = tool.main([os.path.join(REPO, 'tests', 'zz_world')])
        self.assertEqual(code, 2)
        self.assertIn('make_fixture_world:', err.getvalue())
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = tool.main([os.path.join(self.tmp, 'ok')])
        self.assertEqual(code, 0)
        self.assertIn('world:', out.getvalue())


if __name__ == '__main__':
    unittest.main()
