"""Tests of the two options planning step 209 (PORT-11) gave tools/build_linux.sh: --portable and --strip (the row PORT10-3: a Linux program that runs on more than the machine that built it).

Run:   python -m unittest discover -s tools/ci -p "test_*.py" -v        (needs bash; the configure test needs a Linux host with cmake, ninja, a compiler and the engine checkout)

What is pinned: the options are in the script's help and say what they do; a sanitizer build refuses both, before it looks for a tool; a configure with them puts the linker flags in the
CMake cache and a configure without them does not (the control); nothing else about the configure changes. The program the flags make is measured in the Step (readelf), not here.
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
SCRIPT = os.path.join(REPO, 'tools', 'build_linux.sh')


def bash():
    found = shutil.which('bash')
    if found is None or 'system32' in found.lower():
        for candidate in (r'C:\Program Files\Git\bin\bash.exe', r'C:\Program Files (x86)\Git\bin\bash.exe'):
            if os.path.isfile(candidate):
                return candidate
        return None
    return found


def run(*args, timeout=300):
    return subprocess.run([bash(), SCRIPT] + list(args), capture_output=True, text=True, timeout=timeout)


def skip_loudly(case, reason):
    print('\nSKIPPED %s: %s' % (case.id(), reason), file=sys.stderr)
    case.skipTest(reason)


class TheOptions(unittest.TestCase):
    def setUp(self):
        if bash() is None:
            skip_loudly(self, 'no bash: build_linux.sh was NOT run')

    def test_the_help_lists_both_and_says_what_they_do(self):
        done = run('--help')
        self.assertEqual(done.returncode, 0, done.stderr)
        text = ' '.join(done.stdout.split())
        for phrase in ('--portable', '-static-libstdc++ -static-libgcc', 'no GLIBCXX or CXXABI version', 'the floor, so build a release on the OLDEST system', '--strip', '-Wl,--strip-all'):
            self.assertIn(phrase, text)

    def test_a_sanitizer_build_refuses_both_before_it_looks_for_a_tool(self):
        tmp = tempfile.mkdtemp(prefix='mp-build-linux-refusal-')
        self.addCleanup(shutil.rmtree, tmp, True)
        for flag, words in (('--portable', 'a sanitizer needs the shared one'), ('--strip', 'the symbols a sanitizer')):
            # --configure-only and a build folder of the test's own: were the refusal ever lost (the mutation B01 of the Step does exactly that), the script would configure into a scratch
            # folder and stop. Without them it would BUILD THE WHOLE PROJECT, at -j4, in this repository's build folder, and outlive the test's timeout.
            done = run('--sanitize', flag, '--configure-only', '--build-dir', os.path.join(tmp, 'build').replace(os.sep, '/'))
            self.assertEqual(done.returncode, 1, flag)
            self.assertIn(words, done.stderr)
            self.assertNotIn('cmake', done.stderr.replace('build_linux.sh', ''), 'refused before the tools are looked for')

    def test_an_unknown_option_is_still_refused(self):
        done = run('--portabel')
        self.assertEqual(done.returncode, 1)
        self.assertIn('unknown option --portabel', done.stderr)


class TheBuildingDocument(unittest.TestCase):
    """docs/building.md is for a stranger who pastes its code blocks: each must be valid bash (the first version of the document ended lines with a parenthetical, which bash rejects)."""

    def test_every_code_block_of_the_document_is_valid_bash(self):
        if bash() is None:
            skip_loudly(self, 'no bash: the code blocks of docs/building.md were NOT checked')
        with open(os.path.join(REPO, 'docs', 'building.md'), encoding='utf-8') as f:
            blocks = re.findall(r'```\n(.*?)```', f.read(), re.S)
        self.assertGreaterEqual(len(blocks), 5)
        tmp = tempfile.mkdtemp(prefix='mp-building-doc-')
        self.addCleanup(shutil.rmtree, tmp, True)
        for number, block in enumerate(blocks, 1):
            path = os.path.join(tmp, 'block%d.sh' % number)
            with open(path, 'w', encoding='utf-8', newline='\n') as f:
                f.write(block)
            done = subprocess.run([bash(), '-n', path.replace(os.sep, '/')], capture_output=True, text=True)
            self.assertEqual(done.returncode, 0, 'block %d of docs/building.md is not valid bash: %s' % (number, done.stderr.strip()[:200]))


class TheConfigure(unittest.TestCase):
    """A configure only (--configure-only): it writes a build folder of its own under a temporary folder and builds nothing."""

    def setUp(self):
        if not sys.platform.startswith('linux'):
            skip_loudly(self, 'not a Linux host: the configure of build_linux.sh was NOT run')
        for tool in ('cmake', 'ninja'):
            if shutil.which(tool) is None:
                skip_loudly(self, '%s is not installed: the configure was NOT run' % tool)
        if shutil.which('g++') is None and shutil.which('clang++') is None:
            skip_loudly(self, 'no C++ compiler: the configure was NOT run')
        if not os.path.isfile(os.path.join(REPO, 'engine', 'CMakeLists.txt')):
            skip_loudly(self, 'engine/ is not checked out: the configure was NOT run')
        # The engine's GLFW stops its configure without the X11 headers, and Vulkan's loader is found by name: the packages of the script's header. A machine without them gets a loud
        # skip here and not a red test (the tools job of ci.yml installs them, and a test in test_check_workflows.py ties its list to the header's).
        libraries = ['x11', 'xrandr', 'xinerama', 'xcursor', 'xi', 'vulkan']
        if shutil.which('pkg-config') is None or subprocess.run(['pkg-config', '--exists'] + libraries, capture_output=True).returncode != 0:
            skip_loudly(self, 'pkg-config does not find %s (the X11 and Vulkan development packages in build_linux.sh\'s header): the configure was NOT run' % ', '.join(libraries))
        self.tmp = tempfile.mkdtemp(prefix='mp-build-linux-test-')
        self.addCleanup(shutil.rmtree, self.tmp, True)

    def linker_flags(self, *options):
        build = os.path.join(self.tmp, 'b%d' % len(os.listdir(self.tmp)))
        done = run('--configure-only', '--no-baked-paths', '--dev-tools', 'OFF', '--build-dir', build, *options, timeout=600)
        self.assertEqual(done.returncode, 0, done.stdout[-600:] + done.stderr[-600:])
        with open(os.path.join(build, 'CMakeCache.txt'), encoding='utf-8') as f:
            m = re.search(r'^CMAKE_EXE_LINKER_FLAGS:STRING=(.*)$', f.read(), re.M)
        return m.group(1) if m else None, done.stdout

    def test_portable_and_strip_reach_the_linker_flags_and_nothing_does_without_them(self):
        control, _ = self.linker_flags()
        self.assertNotIn('static-libstdc++', control or '')
        self.assertNotIn('strip', control or '')
        both, out = self.linker_flags('--portable', '--strip')
        self.assertIn('-static-libstdc++', both)
        self.assertIn('-static-libgcc', both)
        self.assertIn('-Wl,--strip-all', both)
        self.assertIn('linker flags of the program:', out)
        only_portable, _ = self.linker_flags('--portable')
        self.assertIn('-static-libstdc++', only_portable)
        self.assertNotIn('strip', only_portable)
        only_strip, _ = self.linker_flags('--strip')
        self.assertIn('-Wl,--strip-all', only_strip)
        self.assertNotIn('static-libstdc++', only_strip)


if __name__ == '__main__':
    unittest.main()
