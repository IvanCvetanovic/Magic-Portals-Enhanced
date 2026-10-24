"""Tests of tools/android_emulator.py's GUARD (planning step 204): the one script that touches an emulator must never be able to name a serial that is not its slot's,
nor an adb verb that acts on the adb server the owner's parity rig shares. No emulator, no adb and no SDK is needed: the guard raises before a process is made.

Run:   python -m unittest discover -s tools/package -p "test_*.py" -v
"""
import os
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))

import android_emulator as emu  # noqa: E402


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


if __name__ == '__main__':
    unittest.main()
