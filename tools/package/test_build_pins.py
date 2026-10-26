"""Tests of the two build pins' own machinery (tests/build_pins/, planning step 208, PORT-10; ctest test_mp_build_dev_tools_option and test_mp_build_profile_header).

Run:   python -m unittest discover -s tools/package -p "test_*.py" -v        (standard library only)

The pins themselves run CMake and a C++ compiler on scratch copies of two files, and are run by ctest in the Linux tree. What is tested HERE, on any machine, is what they read
out of the real files: the option's FATAL_ERROR with its line, the engine check's, the header's two #error messages with theirs, the named mutations (each must match the real
file exactly once, or a change of the file would turn the pin's own mutation into a no-op), and the exit code 77 where the pin cannot run. Where cmake and g++ are present (the
Linux subsystem) the header pin is also run end to end, on the real header and on mutated copies: it takes a fraction of a second. On Windows those runs are SKIPPED, and
the skip is printed.
"""
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
PINS = os.path.join(REPO, 'tests', 'build_pins')
sys.path.insert(0, PINS)

import build_profile_header as c2  # noqa: E402
import dev_tools_option as c1  # noqa: E402
import pin_common as pc  # noqa: E402


def read(path):
    return pc.read_text(os.path.join(REPO, path))


def say_skip(case, reason):
    print('\nSKIPPED %s: %s' % (case.id(), reason), file=sys.stderr)
    case.skipTest(reason)


class TheOptionPinReadsTheRealFile(unittest.TestCase):
    def test_the_options_message_and_its_line(self):
        text = read('CMakeLists.txt')
        line, template = c1.option_message(text)
        self.assertEqual(text.split('\n')[line - 1].strip()[:23], 'message(FATAL_ERROR "MA')
        self.assertTrue(template.startswith('MAGICPORTALS_DEV_TOOLS is "${MAGICPORTALS_DEV_TOOLS}", which is not one of its three values'), template)
        self.assertIn('Pass -DMAGICPORTALS_DEV_TOOLS=OFF for a player\'s program.', template)

    def test_the_engine_checks_message_comes_after_it(self):
        text = read('CMakeLists.txt')
        opt_line, _ = c1.option_message(text)
        eng_line, message = c1.engine_message(text)
        self.assertGreater(eng_line, opt_line)
        self.assertTrue(message.startswith('engine/ is empty'), message)
        self.assertIn('git submodule update --init', message)

    def test_the_message_parser_unescapes_what_cmake_does(self):
        calls = c1.cmake_message_calls('message(FATAL_ERROR "a \\"b\\"\\n" "c ${X}")\nmessage(STATUS "s")\n')
        self.assertEqual(calls[0], (1, 'FATAL_ERROR', ['a "b"\n', 'c ${X}']))
        self.assertEqual(calls[1][1:], ('STATUS', ['s']))

    def test_a_file_that_lost_the_guard_is_a_failure_of_the_pin_and_not_a_traceback(self):
        with self.assertRaises(pc.Fail):
            c1.option_message('project(x)\n')
        with self.assertRaises(pc.Fail):
            c1.engine_message('project(x)\n')
        text = read('CMakeLists.txt').replace('message(FATAL_ERROR "MAGICPORTALS_DEV_TOOLS is', 'message(WARNING "MAGICPORTALS_DEV_TOOLS is')
        with self.assertRaises(pc.Fail) as ctx:
            c1.option_message(text)
        self.assertIn('WARNING', str(ctx.exception))

    def test_each_named_mutation_matches_the_real_file_exactly_once(self):
        text = read('CMakeLists.txt')
        names = []
        for name, old, new in c1.mutations_of(text):
            names.append(name)
            self.assertEqual(text.count(old), 1, name)
            self.assertNotEqual(old, new)
            pc.apply_mutation(text, old, new)
        self.assertEqual(names, ['accept-anything', 'refuse-OFF', 'accept-the-typo-OF', 'case-sensitive', 'send-error-not-fatal', 'message-forgets-the-value'])

    def test_the_value_lists_are_the_contract_of_the_documentation(self):
        # docs/packaging.md section 6: exactly AUTO, ON or OFF, in any case; OF, 0, NO and an empty value are refused by name
        for word in ('OF', '0', 'NO', ''):
            self.assertIn(word, c1.BAD_VALUES)
        for word in ('AUTO', 'ON', 'OFF'):
            self.assertIn(word, c1.GOOD_VALUES)
            self.assertNotIn(word, c1.BAD_VALUES)
        self.assertTrue(any(w != w.upper() for w in c1.GOOD_VALUES), 'any case')
        doc = read('docs/packaging.md')
        self.assertIn('**Exactly these three words** (in any case)', doc)


class TheHeaderPinReadsTheRealFile(unittest.TestCase):
    def test_the_two_messages_and_their_lines(self):
        text = read('game/BuildProfile.hpp')
        found = c2.error_texts(text)
        self.assertEqual(len(found), 2)
        (l1, conflict), (l2, missing) = found
        self.assertLess(l1, l2)
        self.assertIn('MAGICPORTALS_BUILD_PROFILE_DESKTOP', conflict)
        self.assertIn('neither as all three', missing)
        for line, message in found:
            self.assertTrue(text.split('\n')[line - 1].startswith('#error "'), 'the real header carries them as #error lines')

    def test_a_message_whose_directive_was_taken_away_is_still_found(self):
        self.assertEqual(c2.error_texts('// "BuildProfile.hpp: gone"\n#warning "BuildProfile.hpp: warned"\nint x;\n'), [(1, 'BuildProfile.hpp: gone'), (2, 'BuildProfile.hpp: warned')])

    def test_each_named_mutation_matches_the_real_header_exactly_once(self):
        text = read('game/BuildProfile.hpp')
        names = []
        for name, old, new in c2.mutations_of(text):
            names.append(name)
            self.assertEqual(text.count(old), 1, name)
            pc.apply_mutation(text, old, new)
        self.assertEqual(names, ['delete-the-missing-constants-error', 'delete-the-conflict-error', 'two-of-three-is-enough', 'conflict-forgets-the-readout',
                                 'the-desktop-declaration-is-gone', 'the-desktop-means-no-tools'])

    def test_the_unit_asks_for_the_constants_it_expects(self):
        self.assertEqual(c2.unit(), '#include "BuildProfile.hpp"\nint main() { return 0; }\n')
        self.assertIn('static_assert(MagicPortals::kDevTools == true', c2.unit((0, 1, 0)))
        self.assertIn('static_assert(MagicPortals::kMobileBuild == false', c2.unit((0, 1, 0)))

    def test_a_mutation_that_matches_nothing_or_twice_is_an_error_of_the_pin(self):
        with self.assertRaises(pc.Fail):
            pc.apply_mutation('abc', 'x', 'y')
        with self.assertRaises(pc.Fail):
            pc.apply_mutation('abab', 'ab', 'y')
        self.assertEqual(pc.apply_mutation('abc', 'b', 'y'), 'ayc')


class ThePinsRun(unittest.TestCase):
    def pin(self, script, *extra):
        return subprocess.run([sys.executable, os.path.join(PINS, script), '--repo', REPO] + list(extra), capture_output=True, text=True, timeout=300)

    def test_on_windows_both_pins_skip_with_77_and_say_why(self):
        if os.name != 'nt':
            say_skip(self, 'not Windows: the Windows skip is not exercised here')
        for script in ('dev_tools_option.py', 'build_profile_header.py'):
            done = self.pin(script)
            self.assertEqual(done.returncode, 77, done.stdout + done.stderr)
            self.assertIn('SKIPPED (exit 77, never a pass)', done.stdout)
            self.assertIn('Linux tree', done.stdout)

    def test_without_a_compiler_or_cmake_both_pins_skip_with_77(self):
        if os.name == 'nt':
            say_skip(self, 'Windows: the pins skip before they look for a compiler (the previous test)')
        done = self.pin('build_profile_header.py', '--cxx-compiler', '/no/such/compiler')
        self.assertEqual(done.returncode, 77, done.stdout)
        self.assertIn('no C++ compiler', done.stdout)
        done = self.pin('dev_tools_option.py', '--cmake', '/no/such/cmake')
        self.assertEqual(done.returncode, 77, done.stdout)
        self.assertIn('no cmake', done.stdout)
        done = self.pin('dev_tools_option.py', '--c-compiler', '/no/such/gcc', '--cxx-compiler', '/no/such/g++')
        self.assertEqual(done.returncode, 77, done.stdout)
        self.assertIn('no C and C++ compiler', done.stdout)

    def test_the_header_pin_end_to_end_where_a_compiler_is_present(self):
        if os.name == 'nt' or not (shutil.which('g++') or shutil.which('clang++')):
            say_skip(self, 'no Linux compiler here: the header pin was NOT run end to end by this test (ctest runs it in the Linux tree)')
        done = self.pin('build_profile_header.py')
        self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
        self.assertIn('6 named mutations', done.stdout)
        self.assertIn('the real file is unchanged', done.stdout)
        self.assertNotIn('GREEN', done.stdout)

    def test_the_header_pin_goes_red_on_a_header_that_lost_its_guard(self):
        if os.name == 'nt' or not (shutil.which('g++') or shutil.which('clang++')):
            say_skip(self, 'no Linux compiler here: a mutated header was NOT compiled by this test')
        tmp = tempfile.mkdtemp(prefix='mp-pin-test-')
        self.addCleanup(shutil.rmtree, tmp, True)
        text = read('game/BuildProfile.hpp').replace('#error "BuildProfile.hpp: this translation unit was given', '// "BuildProfile.hpp: this translation unit was given')
        path = os.path.join(tmp, 'BuildProfile.hpp')
        with open(path, 'w', encoding='utf-8', newline='\n') as f:
            f.write(text)
        done = self.pin('build_profile_header.py', '--file', path)
        self.assertEqual(done.returncode, 1, done.stdout + done.stderr)
        self.assertIn('not with the header\'s missing-constants #error', done.stdout)

    def test_the_option_pin_goes_red_on_a_cmake_file_that_accepts_a_typo(self):
        if os.name == 'nt' or not shutil.which('cmake') or not (shutil.which('g++') or shutil.which('clang++')):
            say_skip(self, 'no cmake and Linux compiler here: a mutated CMakeLists.txt was NOT configured by this test')
        tmp = tempfile.mkdtemp(prefix='mp-pin-test-')
        self.addCleanup(shutil.rmtree, tmp, True)
        text = pc.apply_mutation(read('CMakeLists.txt'), 'elseif (_magicPortalsDevTools STREQUAL "ON")',
                                 'elseif (_magicPortalsDevTools STREQUAL "ON" OR _magicPortalsDevTools STREQUAL "OF")')
        path = os.path.join(tmp, 'CMakeLists.txt')
        with open(path, 'w', encoding='utf-8', newline='\n') as f:
            f.write(text)
        done = self.pin('dev_tools_option.py', '--file', path)
        self.assertEqual(done.returncode, 1, done.stdout + done.stderr)
        self.assertIn("'OF'", done.stdout)


class TheRegistration(unittest.TestCase):
    def test_both_pins_are_registered_in_ctest_with_the_77_mark_and_the_tree_s_tools(self):
        text = read('tests/CMakeLists.txt')
        for name, script in (('test_mp_build_dev_tools_option', 'dev_tools_option.py'), ('test_mp_build_profile_header', 'build_profile_header.py')):
            self.assertIn('add_build_pin(%s %s)' % (name, script), text)
            self.assertTrue(os.path.isfile(os.path.join(PINS, script)))
        body = text[text.index('function(add_build_pin'):text.index('add_build_pin(test_mp_build_dev_tools_option')]
        for word in ('SKIP_RETURN_CODE 77', '${CMAKE_COMMAND}', '${CMAKE_C_COMPILER}', '${CMAKE_CXX_COMPILER}', '${CMAKE_CXX_COMPILER_ID}', '--repo "${CMAKE_SOURCE_DIR}"'):
            self.assertIn(word, body)
        # they are not suites of test_mp: no .cpp, no add_mp_test
        self.assertNotIn('add_mp_test(test_mp_build', text)

    def test_no_c_plus_plus_file_or_game_file_was_added_for_them(self):
        self.assertEqual(sorted(n for n in os.listdir(PINS) if n != '__pycache__'), ['build_profile_header.py', 'dev_tools_option.py', 'pin_common.py'])


if __name__ == '__main__':
    unittest.main()
