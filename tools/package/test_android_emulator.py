"""Tests of tools/android_emulator.py's GUARD (planning step 204) and of what it builds and prints (planning step 205): the one script that touches an emulator
must never be able to name a serial that is not its slot's, nor an adb verb that acts on the adb server the owner's parity rig shares. No emulator, no adb and
no SDK is needed: the guard raises before a process is made, and the tool paths and the processes the script makes are stubbed.

Run:   python -m unittest discover -s tools/package -p "test_*.py" -v
"""
import contextlib
import io
import os
import sys
import tempfile
import unittest
from unittest import mock

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
sys.path.insert(0, HERE)

import android_emulator as emu  # noqa: E402
import android_package as ap  # noqa: E402

REPO = os.path.dirname(os.path.dirname(HERE))


class TheGuard(unittest.TestCase):
    def setUp(self):
        emu.use_slot('impl')

    def argv(self, *args):
        # adb_argv resolves the SDK's adb first; a machine with no SDK has nothing to run, so the check is made on the guard alone.
        emu._check_args(list(args))
        return ['-s', emu.serial()] + list(args)

    def test_only_two_slots_and_their_serials_exist(self):
        self.assertEqual({name: s['port'] for name, s in emu.SLOTS.items()}, {'impl': 5564, 'verify': 5566})
        self.assertEqual({s['avd'] for s in emu.SLOTS.values()}, {'MP_Android_API33_x86_64', 'MP_Android_API33_x86_64_v'})
        emu.use_slot('verify')
        self.assertEqual(emu.serial(), 'emulator-5566')
        emu.use_slot('impl')
        self.assertEqual(emu.serial(), 'emulator-5564')
        for other in ('rig', 'penumbra', 'emulator-5554', 'emulator-5560', ''):
            with self.assertRaises(emu.EmuError, msg=other):
                emu.use_slot(other)

    def test_the_rigs_and_the_siblings_are_never_a_slot(self):
        for name in emu.OTHERS_UNTOUCHED:
            self.assertNotIn(name, {s['avd'] for s in emu.SLOTS.values()})
        self.assertNotIn(5554, {s['port'] for s in emu.SLOTS.values()})
        self.assertNotIn(5560, {s['port'] for s in emu.SLOTS.values()})

    def test_a_caller_may_not_name_a_serial(self):
        for args in (('-s', 'emulator-5554', 'shell', 'ls'), ('shell', '-s'), ('-d', 'shell'), ('-e', 'shell'), ('-t', '1', 'shell')):
            with self.assertRaises(emu.EmuError, msg=repr(args)):
                self.argv(*args)

    def test_the_verbs_that_touch_the_server_or_every_device_are_refused(self):
        for verb in ('kill-server', 'start-server', 'reconnect', 'devices', 'connect', 'disconnect', 'usb', 'tcpip', 'root', 'unroot', 'mdns', 'pair',
                     'forward', 'reverse', 'remount', 'disable-verity', 'version', 'help', 'keys'):
            with self.assertRaises(emu.EmuError, msg=verb):
                self.argv(verb)
        for verb in emu.DENIED_WORDS:
            self.assertNotIn(verb, emu.ALLOWED_VERBS)

    def test_a_shell_is_allowed_and_always_carries_the_slots_serial(self):
        self.assertEqual(self.argv('shell', 'echo hi')[:2], ['-s', 'emulator-5564'])
        emu.use_slot('verify')
        self.assertEqual(self.argv('install', '-r', 'x.apk')[:2], ['-s', 'emulator-5566'])
        for verb in ('shell', 'install', 'uninstall', 'push', 'pull', 'logcat', 'exec-out', 'exec-in', 'get-state', 'wait-for-device'):
            self.assertEqual(self.argv(verb)[:2], ['-s', 'emulator-5566'], verb)

    def test_only_the_named_console_commands_pass_emu(self):
        self.assertEqual(self.argv('emu', 'kill')[:2], ['-s', 'emulator-5564'])
        for bad in ('quit', 'exit', 'rm', 'avd stop', ''):
            with self.assertRaises(emu.EmuError, msg=bad):
                self.argv('emu', bad)
        with self.assertRaises(emu.EmuError):
            self.argv('emu')

    def test_no_adb_means_no_verb(self):
        with self.assertRaises(emu.EmuError):
            self.argv()

    def test_the_memory_floor_is_six_gigabytes(self):
        self.assertEqual(emu.MIN_FREE_MEMORY_BYTES, 6 * 1024 ** 3)


class TheArgvThatIsBuilt(unittest.TestCase):
    """PORT6-6: the guard test above looks at _check_args and never at what adb_argv BUILDS, so an adb_argv that dropped the serial (a bare `adb`, which would
    address the owner's parity rig if it were the only device) passed all eight tests. These hold the real list, with the SDK's adb stubbed out."""
    STUB = 'STUB-ADB'

    def tearDown(self):
        emu.use_slot('impl')

    def test_every_call_is_the_tool_then_dash_s_then_the_slots_serial_then_the_args(self):
        with mock.patch.object(emu, 'tool', return_value=self.STUB):
            for slot, serial in (('impl', 'emulator-5564'), ('verify', 'emulator-5566')):
                emu.use_slot(slot)
                self.assertEqual(emu.adb_argv('shell', 'x'), [self.STUB, '-s', serial, 'shell', 'x'], slot)
                self.assertEqual(emu.adb_argv('emu', 'kill'), [self.STUB, '-s', serial, 'emu', 'kill'], slot)
                self.assertEqual(emu.adb_argv('install', '-r', 'a.apk')[:3], [self.STUB, '-s', serial], slot)

    def test_a_number_argument_is_a_string_and_the_serial_is_still_there(self):
        with mock.patch.object(emu, 'tool', return_value=self.STUB):
            emu.use_slot('impl')
            self.assertEqual(emu.adb_argv('shell', 5), [self.STUB, '-s', 'emulator-5564', 'shell', '5'])

    def test_a_refused_call_builds_nothing(self):
        with mock.patch.object(emu, 'tool', return_value=self.STUB):
            emu.use_slot('impl')
            with self.assertRaises(emu.EmuError):
                emu.adb_argv('devices')

    def test_the_call_that_runs_is_the_list_that_was_built_and_is_logged_with_its_serial(self):
        calls = []

        def fake_run(argv, **kw):
            calls.append(argv)
            return mock.Mock(returncode=0, stdout=b'ok\r\n', stderr=b'')
        notes = []
        with mock.patch.object(emu, 'tool', return_value=self.STUB), mock.patch.object(emu.subprocess, 'run', fake_run), \
                mock.patch.object(emu, 'note', notes.append):
            emu.use_slot('verify')
            self.assertEqual(emu.adb('shell', 'echo hi')[:2], (0, 'ok\n'))
        self.assertEqual(calls, [[self.STUB, '-s', 'emulator-5566', 'shell', 'echo hi']])
        self.assertTrue(notes and notes[0].startswith('adb -s emulator-5566 shell'), notes)


class TheEmulatorsStart(unittest.TestCase):
    """The owner's report after step 204: a Windows Terminal tab titled with the path of qemu-system-x86_64-headless.exe opened at `start`."""

    def tearDown(self):
        emu.use_slot('impl')

    def test_the_flags_give_a_console_with_no_window_and_never_detach(self):
        self.assertTrue(emu.EMULATOR_CREATIONFLAGS & emu.CREATE_NO_WINDOW)
        self.assertTrue(emu.EMULATOR_CREATIONFLAGS & emu.CREATE_NEW_PROCESS_GROUP)
        self.assertFalse(emu.EMULATOR_CREATIONFLAGS & emu.DETACHED_PROCESS,
                         'DETACHED_PROCESS makes qemu allocate a console of its own, which the terminal shows as a tab')
        self.assertEqual(emu.CREATE_NO_WINDOW, 0x08000000)
        self.assertEqual(emu.CREATE_NEW_PROCESS_GROUP, 0x00000200)
        self.assertEqual(emu.DETACHED_PROCESS, 0x00000008)

    def test_start_hands_exactly_those_flags_to_the_process_it_makes(self):
        seen = {}

        class FakePopen:
            pid = 4242

            def __init__(self, argv, **kw):
                seen['argv'] = argv
                seen.update(kw)
        with tempfile.TemporaryDirectory() as folder:
            patches = [mock.patch.object(emu, 'avd_exists', return_value=True), mock.patch.object(emu, 'is_up', return_value=False),
                       mock.patch.object(emu, 'our_processes', return_value=[]), mock.patch.object(emu, 'processes', return_value=[]),
                       mock.patch.object(emu, 'free_memory_bytes', return_value=(8 * 1024 ** 3, 16 * 1024 ** 3)),
                       mock.patch.object(emu, 'tool', return_value='STUB-EMULATOR'), mock.patch.object(emu, 'wait_boot', return_value=True),
                       mock.patch.object(emu, 'log_path', return_value=os.path.join(folder, 'calls.log')), mock.patch.object(emu.subprocess, 'Popen', FakePopen)]
            for p in patches:
                p.start()
            try:
                emu.use_slot('impl')
                with contextlib.redirect_stdout(io.StringIO()):
                    self.assertEqual(emu.cmd_start(mock.Mock(timeout=1)), 0)
            finally:
                for p in patches:
                    p.stop()
        self.assertEqual(seen['creationflags'], emu.EMULATOR_CREATIONFLAGS)
        self.assertEqual(seen['argv'][:5], ['STUB-EMULATOR', '-avd', 'MP_Android_API33_x86_64', '-port', '5564'])

    def test_a_start_below_six_gigabytes_makes_no_process(self):
        made = []
        with mock.patch.object(emu, 'avd_exists', return_value=True), mock.patch.object(emu, 'is_up', return_value=False), \
                mock.patch.object(emu, 'our_processes', return_value=[]), mock.patch.object(emu, 'note'), \
                mock.patch.object(emu, 'free_memory_bytes', return_value=(4 * 1024 ** 3, 16 * 1024 ** 3)), \
                mock.patch.object(emu.subprocess, 'Popen', lambda *a, **k: made.append(a)):
            emu.use_slot('impl')
            with contextlib.redirect_stdout(io.StringIO()) as said:
                self.assertEqual(emu.cmd_start(mock.Mock(timeout=1)), 3)
            self.assertIn('NOT RUN', said.getvalue())
        self.assertEqual(made, [])


class TheConsoleMayLackACharacter(unittest.TestCase):
    """PORT6-7: the verbs that write the device's text to the console died on a character the console's code page lacks (a cp1252 console and a name in
    another script). The tool is for a Windows host; its streams are made to replace what they cannot encode."""

    def console(self):
        return io.TextIOWrapper(io.BytesIO(), encoding='cp1252', errors='strict', newline='\n', write_through=True)

    def run_main(self, argv, **patches):
        out, err = self.console(), self.console()
        stack = [mock.patch.object(emu, name, value) for name, value in patches.items()]
        stack += [mock.patch.object(sys, 'stdout', out), mock.patch.object(sys, 'stderr', err), mock.patch.object(emu, 'note')]
        for p in stack:
            p.start()
        try:
            rc = emu.main(argv)
        finally:
            for p in reversed(stack):
                p.stop()
            emu.use_slot('impl')
        return rc, out.buffer.getvalue().decode('cp1252'), err.buffer.getvalue().decode('cp1252')

    def test_a_shell_whose_answer_has_a_character_cp1252_lacks_prints_a_question_mark(self):
        rc, out, _ = self.run_main(['shell', 'ls'], shell=lambda c, **k: (0, 'name Ж and ☃\n', ''))
        self.assertEqual(rc, 0)
        self.assertEqual(out, 'name ? and ?\n')

    def test_run_as_and_the_raw_adb_verb_too(self):
        rc, out, _ = self.run_main(['run-as', 'ls'], run_as=lambda c, **k: (0, 'Ж', ''))
        self.assertEqual((rc, out), (0, '?'))
        rc, out, _ = self.run_main(['adb', 'get-state'], adb=lambda *a, **k: (0, '', '☃'))
        self.assertEqual((rc, out), (0, '?'))

    def test_the_failure_message_on_stderr_too(self):
        rc, _, err = self.run_main(['key', 'Ж'])
        self.assertEqual(rc, 2)
        self.assertIn('not a key name', err)
        self.assertIn('?', err)

    def test_a_character_the_console_has_is_not_changed(self):
        rc, out, _ = self.run_main(['shell', 'ls'], shell=lambda c, **k: (0, 'café\n', ''))
        self.assertEqual((rc, out), (0, 'café\n'))

    def test_the_tool_and_its_documents_say_it_is_for_a_windows_host(self):
        """The limit stays written where a reader looks: the module's own first line and the row of docs/android.md that lists it among the tools (PORT6-7).
        Not make_output_safe's docstring, which says it too: a test that read that one would pass with the module and the document silent."""
        self.assertIn('Windows', emu.__doc__.split('\n')[0])
        with open(os.path.join(REPO, 'docs', 'android.md'), encoding='utf-8') as f:
            self.assertIn('Windows host only', f.read())


class TheNamesTheScriptSpellsAreTheManifests(unittest.TestCase):
    """PORT6-8 (c): PACKAGE and ACTIVITY are spelled in the script and in game/android/AndroidManifest.xml (which the packager reads)."""

    def test_the_application_id_and_the_activity(self):
        facts = ap.read_manifest(os.path.join(REPO, 'game', 'android', 'AndroidManifest.xml'))
        self.assertEqual(emu.PACKAGE, facts['package'])
        self.assertEqual(emu.ACTIVITY, facts['activity'])
        self.assertNotIn('penumbra', emu.PACKAGE.lower(), "not the sibling port's")


if __name__ == '__main__':
    unittest.main()
