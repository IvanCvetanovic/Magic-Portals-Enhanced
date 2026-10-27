"""Tests of tools/ci/android_smoke.py (planning step 209, PORT-11): the Android smoke flows, run against INVENTED uiautomator XML, INVENTED `dumpsys` text and a FAKE DEVICE.

Run:   python -m unittest discover -s tools/ci -p "test_*.py" -v        (standard library only; no adb, no emulator, no SDK)

WHAT THIS IS, PLAINLY. NO RECORDED REAL OUTPUT of `uiautomator dump`, of `dumpsys activity exit-info` or of any other adb command exists in this repository: the author could
not start an emulator (too little memory on the maker's PC) and nothing has been run on a phone. Every XML and every line of text below was made up from what the platform's
documentation and the engine's Java say, and the FakeDevice is a model of the engine's report written for these tests. So these tests prove that the SCRIPT does what it says
when the device behaves the way they model it: they cannot prove that a device behaves so, and the first run of the flows on a real emulator may find a layout they do not
cover (the parsers are tolerant, and a flow that cannot read what it needs says NOT RUN, which a test below pins).

What is pinned: the dump parsing, the title and the accent, the order in which the ways of ending the game are tried, the NOT RUN reasons, the marker-file rule (the dialog is
asked for only with the scripted-run marker absent), the second-start rule, the timeouts as one table, the two runners (raw adb always `-s <serial>`; the guarded one only
through tools/android_emulator.py), and the ties to the engine's Java (the report's rule and wording).
"""
import contextlib
import io
import os
import re
import subprocess
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)

import android_smoke as sm  # noqa: E402

PACKAGE = 'com.example.invented'
ACTIVITY = 'com.example.engine.InventedActivity'
NAME = 'Invented Game'
MARKER = 'scripted_marker.txt'
LOG = 'Invented Game/scratch/game.log'
PREV_LOG = 'Invented Game/scratch/game.prev.log'
MENU_LINE = '[Magic Portals] loading held on tick 3 (frame 4); the main menu is next'
MANIFEST = """<?xml version="1.0" encoding="utf-8"?>
<!-- an invented manifest -->
<manifest xmlns:android="http://schemas.android.com/apk/res/android" package="%s">
    <application android:label="%s">
        <activity android:name="%s" android:label="%s">
            <meta-data android:name="android.app.lib_name" android:value="x" />
            <meta-data android:name="supersonic.reportLog" android:value="%s" />
            <meta-data android:name="supersonic.reportSkipWhenFile" android:value="%s" />
            %s
            <intent-filter><action android:name="android.intent.action.MAIN" /><category android:name="android.intent.category.LAUNCHER" /></intent-filter>
        </activity>
    </application>
</manifest>
"""


def manifest(label=NAME, title=None):
    extra = '<meta-data android:name="supersonic.reportTitle" android:value="%s" />' % title if title is not None else ''
    return MANIFEST % (PACKAGE, label, ACTIVITY, label, LOG, MARKER, extra)


FACTS = sm.parse_manifest(manifest())

SPELLINGS = ('Success', )


def skip_loudly(case, reason):
    print('\nSKIPPED %s: %s' % (case.id(), reason), file=sys.stderr)
    case.skipTest(reason)


# ---- invented UI hierarchies ------------------------------------------------------------------------------------------------------------

def hierarchy(*nodes, noise_before='', noise_after='UI hierchary dumped to: /sdcard/mp_smoke_ui.xml'):
    body = ''.join(nodes)
    return '%s<?xml version=\'1.0\' encoding=\'UTF-8\' standalone=\'yes\' ?>\n<hierarchy rotation="1">%s</hierarchy>%s' % (noise_before, body, noise_after)


def node(text='', rid='', cls='android.widget.TextView', package=PACKAGE, bounds='[0,0][10,10]', clickable='false', desc=''):
    return ('<node index="0" text="%s" resource-id="%s" class="%s" package="%s" content-desc="%s" checkable="false" checked="false" clickable="%s" enabled="true" '
            'focusable="false" focused="false" scrollable="false" long-clickable="false" password="false" selected="false" bounds="%s" />'
            % (text, rid, cls, package, desc, clickable, bounds))


def dialog_xml(title, ok='OK', share='Share', ids=True, message='Tap Share, or take screenshots'):
    return hierarchy(
        node(cls='android.widget.FrameLayout', bounds='[0,0][1280,720]'),
        node(title, sm.ID_TITLE if ids else '', bounds='[300,100][980,160]'),
        node(message, sm.ID_MESSAGE if ids else '', bounds='[300,170][980,560]'),
        node(share, sm.ID_SHARE if ids else '', 'android.widget.Button', bounds='[300,580][600,660]', clickable='true') if share is not None else '',
        node(ok, sm.ID_OK if ids else '', 'android.widget.Button', bounds='[700,580][980,660]', clickable='true') if ok is not None else '')


GAME_XML = hierarchy(node(cls='android.view.SurfaceView', bounds='[0,0][1280,720]'))


# ---- the fake device -------------------------------------------------------------------------------------------------------------------

class FakeClock:
    def __init__(self):
        self.t = 0.0
        self.slept = 0.0

    def now(self):
        return self.t

    def sleep(self, seconds):
        self.t += seconds
        self.slept += seconds


class FakeDevice(sm.Runner):
    """A model of the engine's report on a device, made for these tests (nothing here was seen on a device). Knobs make it misbehave in the ways the flows must catch."""
    name = 'fake'

    def __init__(self, clock, name=NAME, api=33, debuggable=True, records_from=('native-crash', 'java-crash', 'signal-kill'), title_english=None, drop_accent=False,
                 dialog_every_start=False, no_ids=False, capitals=False, first_unpack=True, locale_works=True, ui_works=True, ok_closes=True, exit_info_works=True,
                 dialog_late=0, release_main=True):
        self.clock, self.name_shown, self.api, self.debuggable = clock, name, api, debuggable
        self.records_from, self.title_english, self.drop_accent = records_from, title_english, drop_accent
        self.dialog_every_start, self.no_ids, self.capitals = dialog_every_start, no_ids, capitals
        self.locale_works, self.ui_works, self.ok_closes, self.exit_info_works = locale_works, ui_works, ok_closes, exit_info_works
        # dialog_late: the dialog of a start is drawn only from the (dialog_late + 1)th UI dump after `am start` on (the first dumps run before it is on the screen);
        # release_main: whether closing the dialog lets the game's main run (the glue holds it until then; the game moves its log aside only when main runs)
        self.dialog_late, self.release_main = dialog_late, release_main
        self.dumps_since_start = 0
        self.ui_file = None      # the file `uiautomator dump` writes and `cat` reads
        self.prev_log = []
        self.installed = False
        self.unpacked = not first_unpack
        self.running = False
        self.foreground = False
        self.pid = None
        self.next_pid = 1000
        self.marker = False
        self.game_log = []
        self.logcat_lines = []
        self.records = []        # newest first
        self.last_shown = 0
        self.dialog = None
        self.language = 'en'
        self.commands = []
        self.dialogs_shown = 0
        self.level_up = False

    # --- the model
    def stamp(self):
        return 1_700_000_000 + int(self.clock.t)

    def add_record(self, reason, name, status, importance):
        self.records.insert(0, {'reason': reason, 'name': name, 'status': status, 'importance': importance, 'pid': self.pid, 'ts': self.stamp()})
        self.running, self.foreground, self.pid, self.dialog = False, False, None, None

    def unexpected(self, rec):
        return sm.offers_report(sm.ExitRecord(0, rec['reason'], rec['name'], rec['status'], rec['importance'], '', rec['pid']))

    def title(self):
        if self.language == 'pt':
            word = 'fechou sozinho da ultima vez' if self.drop_accent else 'fechou sozinho da última vez'
            return 'O %s %s' % (self.name_shown, word)
        return self.title_english or '%s closed unexpectedly last time' % self.name_shown

    def run_main(self):
        """The game's main starts: the log of the run before is moved aside (KeepPreviousLog) and the new one opens with the menu line."""
        self.prev_log, self.game_log = self.game_log, [MENU_LINE]

    def start(self):
        if self.running:
            self.foreground = True
            return
        self.running, self.foreground, self.pid = True, True, self.next_pid
        self.next_pid += 1
        self.dumps_since_start = 0
        self.logcat_lines.append('10-27 12:00:00.000  1  1 I Supersonic: ' + ('Unpacked 900 files, 24707748 bytes (stamp abc).' if not self.unpacked else 'The game is already unpacked.'))
        self.unpacked = True
        newest = next((r for r in self.records if r['ts'] > self.last_shown and self.unexpected(r)), None)
        if self.marker:
            if newest:
                self.last_shown = newest['ts']  # a scripted start lets the end go as seen
            newest = None
        if self.dialog_every_start and self.records:
            newest = self.records[0]
        if newest:
            self.last_shown = newest['ts']
            self.dialogs_shown += 1
            self.dialog = True  # main is held: the log on the device stays the run before's
            return
        self.run_main()

    def dismiss(self):
        self.dialog = None
        if self.release_main:
            self.run_main()

    # --- the adb surface
    def run(self, args, timeout=120, binary=False, stdin=None):
        self.commands.append(' '.join(str(a) for a in args))
        verb = args[0]
        if verb == 'uninstall':
            self.installed = False
            return 0, 'Success\n', ''
        if verb == 'install':
            self.installed = True
            return 0, 'Performing Streamed Install\nSuccess\n', ''
        if verb == 'logcat':
            return 0, ('\n'.join(self.logcat_lines) + '\n') if '-d' in args else '', ''
        if verb == 'exec-out':
            return 0, b'\x89PNG\r\n\x1a\n' + b'fake', b''
        if verb == 'shell':
            return self.shell_command(args[1])
        return 1, '', 'unknown verb %s' % verb

    def screen(self):
        """What `uiautomator dump` would write now."""
        if not (self.dialog and self.dumps_since_start > self.dialog_late):
            return GAME_XML
        share = sm.SHARE_PORTUGUESE if self.language == 'pt' else sm.SHARE_ENGLISH
        return dialog_xml(self.title(), 'OK', share.upper() if self.capitals else share, not self.no_ids)

    def shell_command(self, cmd):
        m = re.match(r"run-as (\S+) (?:sh -c '(.*)'|(.*))$", cmd)
        if m:
            if not self.debuggable:
                return 1, '', 'run-as: package not debuggable: %s' % m.group(1)
            return self.as_app((m.group(2) if m.group(2) is not None else m.group(3)).replace("'\\''", "'"))
        if cmd == 'getprop ro.build.version.sdk':
            return 0, '%d\n' % self.api, ''
        if cmd.startswith('pidof '):
            return (0, '%d\n' % self.pid, '') if self.running else (1, '', '')
        if cmd.startswith('am start -n '):
            self.start()
            return 0, 'Starting: Intent { cmp=%s }\n' % cmd.split()[-1], ''
        if cmd.startswith('am force-stop'):
            if self.running:
                self.add_record(10, 'USER_REQUESTED', 0, 100)
            return 0, '', ''
        if cmd.startswith('am crash'):
            if self.running and 'java-crash' in self.records_from:
                self.add_record(4, 'CRASH', 0, 100)
            return 0, '', ''
        if cmd.startswith('am kill'):
            if self.running and not self.foreground:
                self.add_record(13, 'OTHER', 0, 400)
            return 0, '', ''
        if cmd.startswith('input keyevent'):
            if cmd.endswith('HOME'):
                self.foreground = False
            elif cmd.endswith('BACK') and self.running and not self.dialog:
                self.add_record(1, 'EXIT_SELF', 0, 100)
            return 0, '', ''
        if cmd.startswith('input swipe'):
            x, y = (int(v) for v in cmd.split()[2:4])
            if self.dialog and self.ok_closes and 700 <= x <= 980 and 580 <= y <= 660:
                self.dismiss()
            elif self.running and not self.dialog and (x, y) == sm.MENU_TAPS[-1]:
                self.game_log.append('[Magic Portals] level start: the load ran on tick 90 in 120.0 ms, 130.0 ms after the press was asked for; the level is up')
            return 0, '', ''
        if 'uiautomator dump' in cmd:
            if cmd.startswith('rm -f %s;' % sm.UI_DUMP_FILE):
                self.ui_file = None
            self.dumps_since_start += 1
            if self.ui_works:
                self.ui_file = self.screen()
            if self.ui_file is None:
                return 1, '', 'cat: %s: No such file or directory' % sm.UI_DUMP_FILE
            return 0, self.ui_file, ''
        if cmd.startswith('dumpsys activity exit-info'):
            if not self.exit_info_works:
                return 0, 'Unable to dump\n', ''
            lines = ['Process ExitInfo cache:', '  package: %s' % PACKAGE]
            for i, r in enumerate(self.records):
                t = r['ts'] - 1_700_000_000
                lines += ['      ApplicationExitInfo #%d:' % i, '        timestamp=2026-10-27 12:%02d:%02d.000 pid=%s realUid=10123 packageUid=10123' % (t // 60 % 60, t % 60, r['pid']),
                          '        process=%s reason=%d (%s) subreason=0 (UNKNOWN) status=%d' % (PACKAGE, r['reason'], r['name'], r['status']),
                          '        importance=%d pss=1MB rss=2MB' % r['importance'], '        description=invented', '        state=empty trace=null']
            return 0, '\n'.join(lines) + '\n', ''
        if cmd.startswith('cmd locale set-app-locales'):
            if not self.locale_works:
                return 255, '', 'Unknown command: locale'
            self.language = 'pt' if "'pt'" in cmd else 'en'
            return 0, '', ''
        if cmd.startswith('cmd locale get-app-locales'):
            return 0, 'Locales for %s for user 0 are [%s]\n' % (PACKAGE, 'pt' if self.language == 'pt' else ''), ''
        if cmd.startswith('settings'):
            return 0, '', ''
        if cmd.startswith(('run-as', 'kill')):
            return 1, '', 'bad'
        return 0, '', ''

    def as_app(self, inner):
        if inner == 'id':
            return 0, 'uid=10123(u0_a123) gid=10123 context=u:r:untrusted_app:s0\n', ''
        m = re.match(r"cat 'files/(.*)'$", inner)
        if m:
            files = {LOG: self.game_log, PREV_LOG: self.prev_log}
            return (0, '\n'.join(files[m.group(1)]) + '\n', '') if m.group(1) in files and files[m.group(1)] else (1, '', 'No such file')
        if inner.startswith('rm -f'):
            for path in re.findall(r"'files/([^']*)'", inner):
                if path == MARKER or path == os.path.splitext(MARKER)[0] + '.used':
                    self.marker = False
                elif path == LOG:
                    self.game_log = []
                elif path == PREV_LOG:
                    self.prev_log = []
            return 0, '', ''
        if inner.startswith('ls '):
            there = [p for p in re.findall(r"'files/([^']*)'", inner)
                     if (p == MARKER and self.marker) or (p == LOG and self.game_log) or (p == PREV_LOG and self.prev_log)]
            return (0, ''.join('files/%s\n' % p for p in there), '') if there else (1, '', '')
        m = re.match(r'kill -(\S+) (\d+)', inner)
        if m:
            if self.running and int(m.group(2)) == self.pid:
                if m.group(1) == 'SEGV' and 'native-crash' in self.records_from:
                    self.add_record(5, 'CRASH_NATIVE', 11, 100)
                elif m.group(1) == '9' and 'signal-kill' in self.records_from:
                    self.add_record(2, 'SIGNALED', 9, 100)
            return 0, '', ''
        return 0, '', ''


def make_context(device, clock, content='full', apk=True, **kw):
    path = os.path.join(HERE, 'android_smoke.py') if apk else None   # any file that exists stands in for the APK: the fake never reads it
    return sm.Context(device, FACTS, apk=path, content=content, clock=sm.Clock(clock.now, clock.sleep), say=lambda t: None,
                      timeouts=dict({'install': 5, 'unpack': 20, 'menu': 20, 'process_gone': 10, 'exit_record': 10, 'dialog': 20, 'no_dialog': 20, 'level': 20, 'poll': 1.0, 'settle': 1.0}, **kw))


def run_all(device=None, clock=None, **kw):
    clock = clock or FakeClock()
    device = device or FakeDevice(clock)
    ctx = make_context(device, clock, **kw)
    return sm.run_flows(ctx), ctx, device


def by_flow(results):
    return {r.flow: r for r in results}


# ---- the facts of the manifest --------------------------------------------------------------------------------------------------------

class TheManifest(unittest.TestCase):
    def test_the_invented_manifest_is_read(self):
        self.assertEqual((FACTS.package, FACTS.activity, FACTS.label, FACTS.marker, FACTS.log_path, FACTS.title_name), (PACKAGE, ACTIVITY, NAME, MARKER, LOG, NAME))

    def test_the_real_manifest_gives_the_titles_the_task_names(self):
        path = os.path.join(REPO, 'game', 'android', 'AndroidManifest.xml')
        with open(path, encoding='utf-8') as f:
            facts = sm.parse_manifest(f.read())
        self.assertEqual(facts.title_name, 'Magic Portals Enhanced')
        self.assertEqual(facts.marker, 'magicportals_args.txt')
        self.assertEqual(facts.activity, 'com.ivancvetanovic.supersonic.SupersonicActivity')
        self.assertEqual(sm.expected_title(facts.title_name, False), 'Magic Portals Enhanced closed unexpectedly last time')
        self.assertEqual(sm.expected_title(facts.title_name, True), 'O Magic Portals Enhanced fechou sozinho da última vez')

    def test_the_title_key_wins_over_the_label_and_a_blank_one_does_not(self):
        self.assertEqual(sm.parse_manifest(manifest(title='Portals Deluxe')).title_name, 'Portals Deluxe')
        self.assertEqual(sm.parse_manifest(manifest(title='   ')).title_name, NAME)

    def test_a_label_that_is_a_resource_reference_names_nothing(self):
        self.assertIsNone(sm.parse_manifest(manifest(label='@string/app_name')).title_name)

    def test_a_manifest_without_a_launcher_or_a_package_is_refused(self):
        with self.assertRaises(ValueError):
            sm.parse_manifest(manifest().replace('android.intent.category.LAUNCHER', 'x.y'))
        with self.assertRaises(ValueError):
            sm.parse_manifest(manifest().replace(' package="%s"' % PACKAGE, ''))
        with self.assertRaises(ValueError):
            sm.parse_manifest('not xml at all')


class TheTitle(unittest.TestCase):
    def test_the_english_title_is_exact(self):
        self.assertEqual(sm.expected_title(NAME, False), 'Invented Game closed unexpectedly last time')

    def test_the_portuguese_title_has_the_article_and_the_accent(self):
        title = sm.expected_title(NAME, True)
        self.assertEqual(title, 'O Invented Game fechou sozinho da última vez')
        self.assertIn('ú', title)
        self.assertEqual(ord(title[-10]), 0xFA, 'u with an acute accent, not a plain u')
        self.assertNotEqual(title, 'O Invented Game fechou sozinho da ultima vez')

    def test_the_name_is_joined_not_formatted(self):
        self.assertEqual(sm.expected_title('100% "Real" {Game}', False), '100% "Real" {Game} closed unexpectedly last time')

    def test_no_name_is_refused(self):
        with self.assertRaises(ValueError):
            sm.expected_title('', False)

    def test_the_button_words_are_the_literals_the_dialog_is_checked_against(self):
        # not read from the engine (that tie is skipped without engine/): the fake draws its dialog from these same constants, so only the literals can catch a changed word
        self.assertEqual(sm.SHARE_ENGLISH, 'Share')
        self.assertEqual(sm.SHARE_PORTUGUESE, 'Compartilhar')
        self.assertEqual(sm.BUTTON_OK, 'OK')

    def test_the_words_are_the_engines(self):
        path = os.path.join(REPO, 'engine', 'src', 'platform', 'android', 'java', 'com', 'ivancvetanovic', 'supersonic', 'ReportText.java')
        if not os.path.isfile(path):
            skip_loudly(self, 'engine/ is not checked out: the tie of the title to ReportText.java is NOT checked')
        with open(path, encoding='utf-8') as f:
            java = f.read()
        self.assertIn('" closed unexpectedly last time"', java)
        self.assertIn('" fechou sozinho da \\u00FAltima vez"', java)
        self.assertIn('"O "', java)
        self.assertEqual(sm.TITLE_ENGLISH % 'X', 'X' + sm.TITLE_TAIL_ENGLISH)
        self.assertEqual(sm.TITLE_PORTUGUESE % 'X', 'O X' + sm.TITLE_TAIL_PORTUGUESE)
        with open(os.path.join(os.path.dirname(path), 'PostMortem.java'), encoding='utf-8') as f:
            glue = f.read()
        self.assertIn('"Compartilhar"', glue)
        self.assertIn('"Share"', glue)


# ---- the dump ---------------------------------------------------------------------------------------------------------------------------

class TheDump(unittest.TestCase):
    def test_a_dump_with_noise_round_it_is_read(self):
        nodes = sm.parse_ui_nodes(hierarchy(node('hello', 'x:id/a', bounds='[1,2][3,4]'), noise_before='some noise\n', noise_after='\nUI hierchary dumped to: /dev/tty\n'))
        self.assertEqual(len(nodes), 1)
        self.assertEqual((nodes[0].text, nodes[0].resource_id, nodes[0].bounds), ('hello', 'x:id/a', (1, 2, 3, 4)))

    def test_text_that_is_not_a_hierarchy_is_refused_in_words(self):
        for text in ('', 'ERROR: could not get idle state.', '<hierarchy><node></hierarchy', 'UI hierchary dumped to: /dev/tty'):
            with self.assertRaises(sm.UiError):
                sm.parse_ui_nodes(text)

    def test_the_centre_of_a_node(self):
        self.assertEqual(sm.node_center(sm.parse_ui_nodes(hierarchy(node(bounds='[700,580][980,660]')))[0]), (840, 620))
        with self.assertRaises(ValueError):
            sm.node_center(sm.Node('', '', '', '', '', False, None))

    def test_the_dialog_is_found_by_its_id(self):
        dialog = sm.find_report_dialog(sm.parse_ui_nodes(dialog_xml(sm.expected_title(NAME, False))), PACKAGE)
        self.assertEqual(dialog.title.text, 'Invented Game closed unexpectedly last time')
        self.assertIsNotNone(dialog.ok)
        self.assertIsNotNone(dialog.share)
        self.assertIn(sm.ID_TITLE, dialog.found_by)

    def test_the_dialog_is_found_by_its_text_when_the_ids_are_not_there_and_it_is_said(self):
        dialog = sm.find_report_dialog(sm.parse_ui_nodes(dialog_xml(sm.expected_title(NAME, False), ids=False)), PACKAGE)
        self.assertEqual(dialog.title.text, 'Invented Game closed unexpectedly last time')
        self.assertIn('its text', dialog.found_by)
        self.assertIsNone(dialog.ok, 'and without the ids there is no button to tap: a problem for the check')

    def test_no_dialog_on_a_game_screen_or_in_another_package(self):
        self.assertIsNone(sm.find_report_dialog(sm.parse_ui_nodes(GAME_XML), PACKAGE))
        other = dialog_xml(sm.expected_title(NAME, False)).replace('package="%s"' % PACKAGE, 'package="com.other"')
        self.assertIsNone(sm.find_report_dialog(sm.parse_ui_nodes(other), PACKAGE))

    def good(self, **kw):
        return sm.find_report_dialog(sm.parse_ui_nodes(dialog_xml(sm.expected_title(NAME, False), **kw)), PACKAGE)

    def test_a_good_dialog_has_no_problems(self):
        self.assertEqual(sm.check_dialog(self.good(), sm.expected_title(NAME, False), sm.SHARE_ENGLISH), [])

    def test_the_title_is_compared_exactly(self):
        for wrong in ('Invented Game closed unexpectedly last time ', 'invented game closed unexpectedly last time', 'Penumbra closed unexpectedly last time',
                      'Invented Game closed unexpectedly', 'Invented Game closed unexpectedly last time.'):
            dialog = sm.find_report_dialog(sm.parse_ui_nodes(dialog_xml(wrong)), PACKAGE)
            problems = sm.check_dialog(dialog, sm.expected_title(NAME, False), sm.SHARE_ENGLISH)
            self.assertTrue([p for p in problems if p.startswith('the title is')], wrong)

    def test_a_title_without_the_accent_is_not_the_portuguese_title(self):
        dialog = sm.find_report_dialog(sm.parse_ui_nodes(dialog_xml('O Invented Game fechou sozinho da ultima vez', share='Compartilhar')), PACKAGE)
        problems = sm.check_dialog(dialog, sm.expected_title(NAME, True), sm.SHARE_PORTUGUESE)
        self.assertTrue([p for p in problems if p.startswith('the title is')], problems)

    def test_the_buttons_are_compared_without_regard_to_case(self):
        self.assertEqual(sm.check_dialog(self.good(ok='ok', share='SHARE'), sm.expected_title(NAME, False), sm.SHARE_ENGLISH), [])

    def test_a_missing_button_or_a_wrong_label_is_a_problem(self):
        problems = sm.check_dialog(self.good(ok=None, share=None), sm.expected_title(NAME, False), sm.SHARE_ENGLISH)
        self.assertEqual(len(problems), 2)
        problems = sm.check_dialog(self.good(ok='Close', share='Send'), sm.expected_title(NAME, False), sm.SHARE_ENGLISH)
        self.assertTrue([p for p in problems if 'OK button reads' in p])
        self.assertTrue([p for p in problems if 'Share button reads' in p])

    def test_no_dialog_at_all_is_a_problem(self):
        self.assertEqual(sm.check_dialog(None, 'x', 'y'), ['there is no crash-report dialog on the screen'])


# ---- the exit records -------------------------------------------------------------------------------------------------------------------

RECORDS = """Process ExitInfo cache:
  package: com.example.invented
      ApplicationExitInfo #0:
        timestamp=2026-10-27 12:00:05.000 pid=1002 realUid=10123 packageUid=10123 definingUid=10123 user=0
        process=com.example.invented reason=5 (CRASH_NATIVE) subreason=0 (UNKNOWN) status=11
        importance=100 pss=1MB rss=2MB
        description=invented
        state=empty trace=null
      ApplicationExitInfo #1:
        timestamp=2026-10-27 11:59:00.000 pid=1001 realUid=10123 packageUid=10123 definingUid=10123 user=0
        process=com.example.invented reason=10 (USER_REQUESTED) subreason=0 (UNKNOWN) status=0
        importance=100 pss=1MB rss=2MB
"""


class TheExitRecords(unittest.TestCase):
    def test_invented_records_are_read(self):
        records = sm.parse_exit_info(RECORDS)
        self.assertEqual(len(records), 2)
        self.assertEqual((records[0].reason, records[0].reason_name, records[0].status, records[0].importance, records[0].pid), (5, 'CRASH_NATIVE', 11, 100, 1002))
        self.assertEqual(records[0].timestamp, '2026-10-27 12:00:05.000')
        self.assertEqual((records[1].reason, records[1].reason_name), (10, 'USER_REQUESTED'))

    def test_text_with_no_record_gives_an_empty_list_not_an_error(self):
        for text in ('', 'Unable to dump', 'Process ExitInfo cache:\n  package: x\n'):
            self.assertEqual(sm.parse_exit_info(text), [])

    def test_a_block_without_a_reason_is_skipped_and_the_fields_are_optional(self):
        records = sm.parse_exit_info('ApplicationExitInfo #0:\n  nothing here\nApplicationExitInfo #1:\n  reason=4\n')
        self.assertEqual(len(records), 1)
        self.assertEqual((records[0].reason, records[0].status, records[0].importance, records[0].pid), (4, None, None, None))

    def test_the_report_offers_what_the_engines_rule_offers(self):
        def rec(reason, status=0, importance=100):
            return sm.ExitRecord(0, reason, '', status, importance, '', 1)
        for reason in (sm.REASON_CRASH, sm.REASON_CRASH_NATIVE, sm.REASON_ANR, sm.REASON_INITIALIZATION_FAILURE, sm.REASON_EXCESSIVE_RESOURCE_USAGE):
            self.assertTrue(sm.offers_report(rec(reason, importance=400)), reason)
        self.assertTrue(sm.offers_report(rec(sm.REASON_EXIT_SELF, status=1)))
        self.assertFalse(sm.offers_report(rec(sm.REASON_EXIT_SELF, status=0)), 'a clean quit')
        self.assertFalse(sm.offers_report(rec(sm.REASON_EXIT_SELF, status=None)))
        for reason in (sm.REASON_UNKNOWN, sm.REASON_LOW_MEMORY, sm.REASON_SIGNALED, sm.REASON_OTHER, sm.REASON_DEPENDENCY_DIED, sm.REASON_PERMISSION_CHANGE):
            self.assertTrue(sm.offers_report(rec(reason, importance=100)), reason)
            self.assertTrue(sm.offers_report(rec(reason, importance=125)), reason)
            self.assertFalse(sm.offers_report(rec(reason, importance=200)), 'a process nobody was looking at')
            self.assertFalse(sm.offers_report(rec(reason, importance=None)))
        for reason in (sm.REASON_USER_REQUESTED, sm.REASON_USER_STOPPED, 99):
            self.assertFalse(sm.offers_report(rec(reason)), reason)

    def test_the_rule_is_the_java_sources(self):
        path = os.path.join(REPO, 'engine', 'src', 'platform', 'android', 'java', 'com', 'ivancvetanovic', 'supersonic', 'PostMortem.java')
        if not os.path.isfile(path):
            skip_loudly(self, 'engine/ is not checked out: the tie of the report rule to PostMortem.java is NOT checked')
        with open(path, encoding='utf-8') as f:
            java = f.read()
        self.assertIn('FOREGROUND_AT_MOST = %d' % sm.FOREGROUND_AT_MOST, java)
        body = java[java.index('private static boolean isUnexpected'):java.index('private static Report build')]
        groups = re.findall(r'((?:\s*case ApplicationExitInfo\.REASON_\w+:)+)', body)
        names = [re.findall(r'REASON_(\w+)', g) for g in groups]
        wanted = {'CRASH': sm.REASON_CRASH, 'CRASH_NATIVE': sm.REASON_CRASH_NATIVE, 'ANR': sm.REASON_ANR, 'INITIALIZATION_FAILURE': sm.REASON_INITIALIZATION_FAILURE,
                  'EXCESSIVE_RESOURCE_USAGE': sm.REASON_EXCESSIVE_RESOURCE_USAGE, 'EXIT_SELF': sm.REASON_EXIT_SELF, 'UNKNOWN': sm.REASON_UNKNOWN, 'LOW_MEMORY': sm.REASON_LOW_MEMORY,
                  'SIGNALED': sm.REASON_SIGNALED, 'OTHER': sm.REASON_OTHER, 'DEPENDENCY_DIED': sm.REASON_DEPENDENCY_DIED, 'PERMISSION_CHANGE': sm.REASON_PERMISSION_CHANGE}
        self.assertEqual(sorted(names[0]), sorted(k for k, v in wanted.items() if v in sm.ALWAYS_OFFERED))
        self.assertEqual(names[1], ['EXIT_SELF'])
        self.assertEqual(sorted(names[2]), sorted(k for k, v in wanted.items() if v in sm.OFFERED_WHEN_ON_THE_SCREEN))

    def test_new_records_are_those_the_earlier_listing_did_not_have(self):
        before = sm.parse_exit_info(RECORDS)
        after = sm.parse_exit_info(RECORDS.replace('ApplicationExitInfo #0:', 'ApplicationExitInfo #1:').replace('ApplicationExitInfo #1:\n        timestamp=2026-10-27 11:59',
                                                                                                                 'ApplicationExitInfo #2:\n        timestamp=2026-10-27 11:59'))
        self.assertEqual(sm.new_records(before, after), [])
        added = sm.parse_exit_info('ApplicationExitInfo #0:\n timestamp=2026-10-27 12:05:00.000 pid=1003 reason=4 (CRASH) status=0 importance=100\n') + after
        self.assertEqual([r.pid for r in sm.new_records(before, added)], [1003])


class TheWaysOfEndingTheGame(unittest.TestCase):
    def test_the_order_is_a_decision(self):
        self.assertEqual([m.name for m in sm.CRASH_METHODS], ['native-crash', 'java-crash', 'signal-kill', 'am-kill'])

    def test_the_measured_way_is_first_and_am_kill_is_last(self):
        self.assertEqual(sm.crash_command(sm.CRASH_METHODS[0], PACKAGE, 1234), 'run-as com.example.invented kill -SEGV 1234')
        self.assertEqual(sm.crash_command(sm.CRASH_METHODS[1], PACKAGE, 1234), 'am crash com.example.invented')
        self.assertEqual(sm.crash_command(sm.CRASH_METHODS[2], PACKAGE, 1234), 'run-as com.example.invented kill -9 1234')
        self.assertEqual(sm.crash_command(sm.CRASH_METHODS[3], PACKAGE, 1234), 'am kill com.example.invented')
        self.assertEqual([m.needs_run_as for m in sm.CRASH_METHODS], [True, False, True, False])


# ---- the runners ------------------------------------------------------------------------------------------------------------------------

class TheRunners(unittest.TestCase):
    def test_raw_adb_is_always_given_the_serial(self):
        seen = []

        def fake_run(argv, **kw):
            seen.append(argv)
            return subprocess.CompletedProcess(argv, 0, b'out\r\n', b'')
        original = sm.subprocess.run
        sm.subprocess.run = fake_run
        try:
            runner = sm.RawRunner('emulator-5554', adb='adb-under-test')
            self.assertEqual(runner.run(['shell', 'echo hi']), (0, 'out\n', ''))
            runner.run(['install', '-r', 'x.apk'])
            runner.shell('id')
            runner.run_as(PACKAGE, "echo 'a b'")
            runner.tap(1, 2)
            runner.key('KEYCODE_HOME')
        finally:
            sm.subprocess.run = original
        for argv in seen:
            self.assertEqual(argv[:3], ['adb-under-test', '-s', 'emulator-5554'], argv)
        self.assertIn("run-as com.example.invented sh -c 'echo '\\''a b'\\'''", seen[3][4])
        self.assertEqual(seen[4][3:], ['shell', 'input swipe 1 2 1 2 250'])

    def test_a_raw_runner_without_a_serial_is_refused(self):
        for serial in (None, ''):
            with self.assertRaises(sm.AdbError):
                sm.RawRunner(serial)

    def test_a_missing_adb_is_an_adb_error_and_a_timeout_is_not_a_crash(self):
        original = sm.subprocess.run

        def missing(argv, **kw):
            raise FileNotFoundError('no adb')
        sm.subprocess.run = missing
        try:
            with self.assertRaises(sm.AdbError):
                sm.RawRunner('s').run(['shell', 'x'])
        finally:
            sm.subprocess.run = original

        def slow(argv, **kw):
            raise subprocess.TimeoutExpired(argv, 1)
        sm.subprocess.run = slow
        try:
            rc, out, err = sm.RawRunner('s').run(['shell', 'x'], timeout=1)
        finally:
            sm.subprocess.run = original
        self.assertEqual(rc, 124)
        self.assertIn('timeout', err)

    def test_the_guarded_runner_goes_through_the_emulator_script_and_never_names_a_serial(self):
        class EmuError(Exception):
            pass

        class FakeEmu:
            calls = []

            def use_slot(self, name):
                if name not in ('impl', 'verify'):
                    raise EmuError('unknown slot %r' % name)

            def adb(self, *args, **kw):
                self.calls.append((args, kw))
                if args[0] == 'forbidden':
                    raise EmuError('the adb verb is not allowed here')
                return 0, 'ok', ''
        FakeEmu.EmuError = EmuError
        emu = FakeEmu()
        runner = sm.GuardedRunner('verify', module=emu)
        self.assertEqual(runner.shell('id'), (0, 'ok', ''))
        self.assertEqual(emu.calls[0][0], ('shell', 'id'))
        self.assertNotIn('-s', emu.calls[0][0])
        with self.assertRaises(sm.AdbError):
            runner.run(['forbidden'])
        with self.assertRaises(sm.AdbError):
            sm.GuardedRunner('somebody-elses', module=emu)

    def test_the_guarded_runner_is_the_repositorys_emulator_script(self):
        # the real module, imported: it names two slots and refuses the verbs the flows must never use (the guard is tested by tools/package/test_android_emulator.py)
        runner = sm.GuardedRunner('impl')
        self.assertEqual(runner.emu.serial(), 'emulator-5564')
        self.assertEqual(sm.GuardedRunner('verify').emu.serial(), 'emulator-5566')
        for args in (['kill-server'], ['devices'], ['-s', 'emulator-5554', 'shell', 'id'], ['root']):
            with self.assertRaises(sm.AdbError):
                runner.run(args)
        # every verb the flows use is one the guard allows
        for args in (['shell', 'id'], ['install', '-r', 'x.apk'], ['uninstall', PACKAGE], ['logcat', '-c'], ['logcat', '-d', '-v', 'threadtime'], ['exec-out', 'screencap', '-p']):
            runner.emu._check_args(args)

    def test_the_smoke_script_does_not_import_the_emulator_script_until_a_guarded_runner_is_made(self):
        with open(os.path.join(HERE, 'android_smoke.py'), encoding='utf-8') as f:
            source = f.read()
        top = source[:source.index('class GuardedRunner')]
        self.assertNotIn('import android_emulator', top)


# ---- the flows over the fake device -----------------------------------------------------------------------------------------------------

class TheFlowsOnAWellBehavedDevice(unittest.TestCase):
    def test_every_flow_passes_and_the_exit_code_is_zero(self):
        results, ctx, device = run_all()
        got = {r.flow: (r.status, r.reason) for r in results}
        self.assertEqual({f: s for f, (s, _) in got.items()}, {f: sm.PASS for f in ('F1', 'F2', 'F3', 'F4', 'F5', 'F6')}, got)
        self.assertEqual(sm.exit_code(results), 0)
        self.assertEqual(device.dialogs_shown, 2, 'one for the English dialog of F2 and one for the Portuguese of F5: never twice for one end')

    def test_the_render_names_every_flow_and_counts(self):
        results, _, _ = run_all()
        text = sm.render(results)
        for flow in ('F1', 'F2', 'F3', 'F4', 'F5', 'F6'):
            self.assertIn(flow, text)
        self.assertIn('6 PASS, 0 FAIL, 0 NOT RUN', text)

    def test_f2_ends_the_game_by_the_first_method_that_leaves_a_record_and_says_so(self):
        results, ctx, device = run_all()
        f2 = by_flow(results)['F2']
        self.assertIn('native-crash', f2.reason)
        self.assertTrue(any('kill -SEGV' in c for c in device.commands))
        self.assertFalse(any('am crash' in c for c in device.commands), 'the second way is not tried when the first worked')

    def test_the_marker_is_removed_before_the_restart_that_must_show_the_dialog(self):
        results, ctx, device = run_all()
        commands = device.commands
        crash_at = next(i for i, c in enumerate(commands) if 'kill -SEGV' in c)
        later = commands[crash_at:]
        removed = next(i for i, c in enumerate(later) if 'rm -f' in c and 'files/%s' % MARKER in c)
        started = next(i for i, c in enumerate(later) if c.startswith('shell am start'))
        self.assertLess(removed, started)

    def test_a_start_with_the_marker_present_is_not_what_the_flow_asks_for(self):
        # the device keeps the marker (the file cannot be removed): the dialog never shows, and the flow says the marker is why, not that the dialog is missing
        clock = FakeClock()
        device = FakeDevice(clock)
        original = device.as_app

        def stubborn(inner):
            if inner.startswith('rm -f') and MARKER in inner:
                device.marker = True
                return 0, '', ''
            return original(inner)
        device.as_app = stubborn
        device.marker = True
        results, ctx, _ = run_all(device, clock)
        f2 = by_flow(results)['F2']
        self.assertEqual(f2.status, sm.NOT_RUN, f2)
        self.assertIn('marker file', f2.reason)

    def test_f3_a_second_start_shows_nothing_and_f3_needs_the_dialog_of_f2(self):
        results, _, device = run_all()
        self.assertEqual(by_flow(results)['F3'].status, sm.PASS)
        clock = FakeClock()
        results = sm.run_flows(make_context(FakeDevice(clock), clock), {'F3'})
        self.assertEqual(by_flow(results)['F3'].status, sm.NOT_RUN)
        self.assertIn('no dismissed report', by_flow(results)['F3'].reason)

    def test_f4_covers_home_back_and_a_force_stop(self):
        results, _, device = run_all()
        f4 = by_flow(results)['F4']
        self.assertEqual(f4.status, sm.PASS, f4)
        joined = '\n'.join(f4.evidence)
        for label in ('Home', 'Back on the main menu', 'a force-stop'):
            self.assertIn(label, joined)
        self.assertTrue(any(c.startswith('shell input keyevent KEYCODE_BACK') for c in device.commands))

    def test_f5_reads_the_portuguese_title_with_the_accent_and_resets_the_language(self):
        results, _, device = run_all()
        f5 = by_flow(results)['F5']
        self.assertEqual(f5.status, sm.PASS, f5)
        self.assertIn('fechou sozinho da última vez', f5.reason)
        self.assertEqual(device.language, 'en', 'the language is put back')

    def test_f6_taps_the_menu_and_reads_the_level_line(self):
        results, _, device = run_all()
        f6 = by_flow(results)['F6']
        self.assertEqual(f6.status, sm.PASS, f6)
        self.assertEqual(sum(1 for c in device.commands if c.startswith('shell input swipe') and c.endswith('250')), len(sm.MENU_TAPS) + 2, 'the taps of F6 and the one on OK of each of the two dialogs')


class TheFlowsOnADeviceThatMisbehaves(unittest.TestCase):
    def test_a_wrong_title_is_a_fail_with_the_text_that_was_found(self):
        clock = FakeClock()
        results, _, _ = run_all(FakeDevice(clock, title_english='Penumbra closed unexpectedly last time'), clock)
        f2 = by_flow(results)['F2']
        self.assertEqual(f2.status, sm.FAIL)
        self.assertIn("'Penumbra closed unexpectedly last time'", f2.reason)
        self.assertEqual(sm.exit_code(results), sm.EXIT_FAIL)

    def test_a_lost_accent_is_a_fail_in_f5(self):
        clock = FakeClock()
        results, _, _ = run_all(FakeDevice(clock, drop_accent=True), clock)
        f5 = by_flow(results)['F5']
        self.assertEqual(f5.status, sm.FAIL)
        self.assertIn('ultima vez', f5.reason)
        self.assertEqual(by_flow(results)['F2'].status, sm.PASS)

    def test_a_dialog_shown_at_every_start_fails_the_second_start_rule(self):
        clock = FakeClock()
        results, _, _ = run_all(FakeDevice(clock, dialog_every_start=True), clock)
        by = by_flow(results)
        self.assertEqual(by['F2'].status, sm.PASS)
        self.assertEqual(by['F3'].status, sm.FAIL, by['F3'])
        self.assertIn('second time', by['F3'].reason)

    def test_no_method_that_leaves_a_record_is_a_not_run_never_a_fail(self):
        clock = FakeClock()
        results, _, device = run_all(FakeDevice(clock, records_from=()), clock)
        f2 = by_flow(results)['F2']
        self.assertEqual(f2.status, sm.NOT_RUN, f2)
        self.assertIn('no way of ending the game', f2.reason)
        for name in ('native-crash', 'java-crash', 'signal-kill', 'am-kill'):
            self.assertIn(name, ' '.join(f2.evidence + [f2.reason]), 'every method tried is named')
        self.assertEqual(sm.exit_code(results), sm.EXIT_NOT_RUN)
        for flow in ('F3', 'F5'):
            self.assertEqual(by_flow(results)[flow].status, sm.NOT_RUN, flow)

    def test_the_next_method_is_tried_when_the_first_leaves_nothing(self):
        clock = FakeClock()
        results, _, device = run_all(FakeDevice(clock, records_from=('java-crash',)), clock)
        f2 = by_flow(results)['F2']
        self.assertEqual(f2.status, sm.PASS, f2)
        self.assertIn('java-crash', f2.reason)
        self.assertTrue(any('kill -SEGV' in c for c in device.commands) and any('am crash' in c for c in device.commands))

    def test_a_non_debuggable_apk_cannot_be_reached_and_says_so(self):
        clock = FakeClock()
        results, _, _ = run_all(FakeDevice(clock, debuggable=False), clock)
        by = by_flow(results)
        for flow in ('F2', 'F3', 'F4', 'F5'):
            self.assertEqual(by[flow].status, sm.NOT_RUN, flow)
            self.assertIn('not debuggable', by[flow].reason)

    def test_an_old_android_has_no_report(self):
        clock = FakeClock()
        results, _, _ = run_all(FakeDevice(clock, api=29), clock)
        for flow in ('F2', 'F3', 'F4', 'F5'):
            self.assertEqual(by_flow(results)[flow].status, sm.NOT_RUN, flow)
            self.assertIn('API 29', by_flow(results)[flow].reason)

    def test_an_unreadable_ui_is_not_run_not_pass(self):
        clock = FakeClock()
        results, _, _ = run_all(FakeDevice(clock, ui_works=False), clock)
        f2 = by_flow(results)['F2']
        self.assertEqual(f2.status, sm.NOT_RUN, f2)
        self.assertIn('UI hierarchy could not be read', f2.reason)

    def test_a_dialog_whose_ok_does_not_close_it_is_a_fail(self):
        clock = FakeClock()
        results, _, _ = run_all(FakeDevice(clock, ok_closes=False), clock)
        f2 = by_flow(results)['F2']
        self.assertEqual(f2.status, sm.FAIL)
        self.assertIn('still on the screen', f2.reason)

    def test_a_dialog_without_ids_is_found_by_its_text_but_its_buttons_cannot_be_tapped(self):
        clock = FakeClock()
        results, _, _ = run_all(FakeDevice(clock, no_ids=True), clock)
        f2 = by_flow(results)['F2']
        self.assertEqual(f2.status, sm.FAIL)
        self.assertIn('no OK button', f2.reason)

    def test_capital_letters_on_the_buttons_do_not_fail_the_flow(self):
        clock = FakeClock()
        results, _, _ = run_all(FakeDevice(clock, capitals=True), clock)
        self.assertEqual(by_flow(results)['F2'].status, sm.PASS)

    def test_a_language_that_cannot_be_set_is_not_run_and_the_fallback_is_unverified(self):
        clock = FakeClock()
        results, _, _ = run_all(FakeDevice(clock, locale_works=False), clock)
        f5 = by_flow(results)['F5']
        # the fake accepts the fallback setting without changing the language: the title stays English, and the flow must say the setting was unverified
        self.assertEqual(f5.status, sm.FAIL, f5)
        self.assertIn('unverified', f5.reason)

    def test_an_apk_with_no_game_runs_only_the_install_and_the_start(self):
        results, _, device = run_all(content='none')
        by = by_flow(results)
        self.assertEqual(by['F1'].status, sm.PASS)
        for flow in ('F2', 'F3', 'F4', 'F5', 'F6'):
            self.assertEqual(by[flow].status, sm.NOT_RUN, flow)
            self.assertIn('no game in it', by[flow].reason)

    def test_a_missing_apk_is_not_run(self):
        results, _, _ = run_all(apk=False)
        self.assertEqual(by_flow(results)['F1'].status, sm.NOT_RUN)
        self.assertEqual(by_flow(results)['F2'].status, sm.NOT_RUN)
        self.assertIn('F1 did not pass', by_flow(results)['F2'].reason)

    def test_a_failed_install_is_a_fail_of_f1_and_the_rest_do_not_run(self):
        clock = FakeClock()
        device = FakeDevice(clock)
        original = device.run

        def refuse(args, **kw):
            if args[0] == 'install':
                device.commands.append(' '.join(args))
                return 1, '', 'Failure [INSTALL_FAILED_NO_MATCHING_ABIS]'
            return original(args, **kw)
        device.run = refuse
        results, _, _ = run_all(device, clock)
        self.assertEqual(by_flow(results)['F1'].status, sm.FAIL)
        self.assertIn('NO_MATCHING_ABIS', by_flow(results)['F1'].reason)
        self.assertEqual(by_flow(results)['F2'].status, sm.NOT_RUN)

    def test_a_game_that_never_reaches_its_menu_is_a_fail_of_f1(self):
        clock = FakeClock()
        device = FakeDevice(clock)
        original = device.start

        def silent():
            original()
            device.game_log.clear()
        device.start = silent
        results, _, _ = run_all(device, clock)
        self.assertEqual(by_flow(results)['F1'].status, sm.FAIL)
        self.assertIn('main menu', by_flow(results)['F1'].reason)

    def test_a_device_that_does_not_answer_is_not_run_for_every_flow(self):
        class Dead(sm.Runner):
            def run(self, args, timeout=120, binary=False, stdin=None):
                raise sm.AdbError('adb: device offline')
        clock = FakeClock()
        results = sm.run_flows(make_context(Dead(), clock))
        self.assertEqual([r.status for r in results], [sm.NOT_RUN] * 6)
        self.assertIn('does not answer', results[0].reason)
        self.assertEqual(sm.exit_code(results), sm.EXIT_NOT_RUN)

    def test_an_exception_in_a_flow_is_a_fail_that_names_itself_and_the_others_still_run(self):
        original = sm.flow_f4

        def broken(ctx):
            raise KeyError('boom')
        sm.FLOWS = tuple((n, broken if n == 'F4' else f) for n, f in sm.FLOWS)
        try:
            results, _, _ = run_all()
        finally:
            sm.FLOWS = tuple((n, original if n == 'F4' else f) for n, f in sm.FLOWS)
        by = by_flow(results)
        self.assertEqual(by['F4'].status, sm.FAIL)
        self.assertIn('KeyError', by['F4'].reason)
        self.assertEqual(by['F5'].status, sm.PASS)

    def test_the_exit_codes(self):
        P, F, N = sm.Result('A', sm.PASS, '', []), sm.Result('B', sm.FAIL, '', []), sm.Result('C', sm.NOT_RUN, '', [])
        self.assertEqual(sm.exit_code([P, P]), 0)
        self.assertEqual(sm.exit_code([P, N]), 3)
        self.assertEqual(sm.exit_code([P, F, N]), 1)
        self.assertEqual(sm.exit_code([]), 3, 'no result is never a pass')

    def test_the_flows_can_be_chosen(self):
        results = sm.run_flows(make_context(FakeDevice(FakeClock()), FakeClock()), {'F1'})
        self.assertEqual([r.flow for r in results], ['F1'])


class TheLogOfThisStart(unittest.TestCase):
    """Defect D1 of the verifier of step 209 (fix round 1). The glue builds the report in onCreate and holds the game's main until the dialog is closed; the game moves its log
    aside only when main starts. So until the dialog is closed the log on the device is the RUN BEFORE's, with its `the main menu is next`. The fake models that (run_main), and
    the knobs dialog_late (the dialog is drawn after the first dumps) and release_main (OK does not let main run) are the ways a flow could be fooled by the stale line."""

    def until_f2(self, device, clock):
        ctx = make_context(device, clock)
        return ctx, by_flow(sm.run_flows(ctx, {'F1', 'F2'}))

    def test_the_fake_leaves_the_run_befores_menu_line_in_the_log_while_the_dialog_is_open(self):
        clock = FakeClock()
        device = FakeDevice(clock)
        ctx = make_context(device, clock)
        sm.run_flows(ctx, {'F1'})
        self.assertEqual(device.game_log, [MENU_LINE])
        device.add_record(5, 'CRASH_NATIVE', 11, 100)
        device.start()
        self.assertTrue(device.dialog)
        self.assertEqual(device.game_log, [MENU_LINE], 'main is held: nothing has moved the log')
        self.assertEqual(device.prev_log, [])

    def test_a_dialog_that_is_drawn_a_moment_late_is_still_found(self):
        clock = FakeClock()
        results, _, device = run_all(FakeDevice(clock, dialog_late=2), clock)
        self.assertEqual({f: r.status for f, r in by_flow(results).items()}, {f: sm.PASS for f in ('F1', 'F2', 'F3', 'F4', 'F5', 'F6')}, [r.reason for r in results])
        self.assertEqual(device.dialogs_shown, 2)

    def test_a_dialog_the_glue_wrongly_shows_at_every_start_and_draws_late_fails_f3(self):
        # the verifier's probe: the first dump runs before the dialog is on the screen, and the log still says the menu is next
        clock = FakeClock()
        ctx, by = self.until_f2(FakeDevice(clock, dialog_every_start=True, dialog_late=1), clock)
        self.assertEqual(by['F2'].status, sm.PASS, by['F2'])
        by = by_flow(sm.run_flows(ctx, {'F3'}))
        self.assertEqual(by['F3'].status, sm.FAIL, by['F3'])
        self.assertIn('second time', by['F3'].reason)

    def test_a_dialog_the_glue_wrongly_shows_after_home_back_or_a_force_stop_fails_f4(self):
        clock = FakeClock()
        ctx, by = self.until_f2(FakeDevice(clock, dialog_every_start=True, dialog_late=1), clock)
        by = by_flow(sm.run_flows(ctx, {'F4'}))
        self.assertEqual(by['F4'].status, sm.FAIL, by['F4'])
        self.assertIn('came at the next start', by['F4'].reason)

    def test_an_ok_that_does_not_let_the_game_start_is_not_a_pass(self):
        # the dialog goes away, the game's main does not run, and the log still holds the run before's menu line
        clock = FakeClock()
        device = FakeDevice(clock, release_main=False)
        ctx, by = self.until_f2(device, clock)
        self.assertEqual(by['F2'].status, sm.FAIL, by['F2'])
        self.assertIn('did not reach its menu', by['F2'].reason)

    def test_the_log_is_removed_before_the_second_start_and_the_start_is_judged_by_a_new_one(self):
        clock = FakeClock()
        device = FakeDevice(clock)
        ctx, by = self.until_f2(device, clock)
        mark = len(device.commands)
        by = by_flow(sm.run_flows(ctx, {'F3'}))
        self.assertEqual(by['F3'].status, sm.PASS, by['F3'])
        later = device.commands[mark:]
        start = next(i for i, c in enumerate(later) if c.startswith('shell am start'))
        self.assertTrue(any('rm -f' in c and 'scratch/game.log' in c and 'scratch/game.prev.log' in c for c in later[:start]), 'the log and the run before\'s go before `am start`')

    def test_the_log_is_kept_for_the_start_that_must_show_the_dialog_and_removed_before_the_tap_on_ok(self):
        # the report reads the log in onCreate (it is the report's input); after the dialog was read, nothing has the file open and the game's main has not run
        _, _, device = run_all()
        commands = device.commands
        crash = next(i for i, c in enumerate(commands) if 'kill -SEGV' in c)
        start = next(i for i, c in enumerate(commands) if i > crash and c.startswith('shell am start'))
        removals = [i for i, c in enumerate(commands) if 'rm -f' in c and 'scratch/game.log' in c]
        tap = next(i for i, c in enumerate(commands) if i > start and c.startswith('shell input swipe'))
        self.assertEqual([i for i in removals if crash < i < start], [], 'the start that must show the dialog finds the log of the run that ended')
        self.assertTrue([i for i in removals if start < i < tap], 'removed once the dialog was read, before OK')

    def test_a_dump_that_fails_is_an_error_and_not_the_last_dump_read_again(self):
        clock = FakeClock()
        device = FakeDevice(clock)
        ctx = make_context(device, clock)
        self.assertEqual(len(ctx.ui_nodes()), 1)
        device.ui_works = False
        with self.assertRaises(sm.UiError):
            ctx.ui_nodes()

    def test_a_second_start_whose_ui_was_never_read_is_not_run_and_so_is_f4(self):
        clock = FakeClock()
        device = FakeDevice(clock)
        ctx, by = self.until_f2(device, clock)
        self.assertEqual(by['F2'].status, sm.PASS, by['F2'])
        device.ui_works = False
        by = by_flow(sm.run_flows(ctx, {'F3'}))
        self.assertEqual(by['F3'].status, sm.NOT_RUN, by['F3'])
        self.assertIn('UI hierarchy could not be read', by['F3'].reason)
        self.assertTrue(ctx.running, 'the game\'s new log says it is on its menu')
        by = by_flow(sm.run_flows(ctx, {'F4'}))
        self.assertEqual(by['F4'].status, sm.NOT_RUN, by['F4'])
        self.assertIn('UI hierarchy could not be read', by['F4'].reason)

    def test_a_start_held_by_a_dialog_the_dump_cannot_see_is_a_fail_and_never_a_pass(self):
        clock = FakeClock()
        device = FakeDevice(clock, dialog_every_start=True)
        ctx, by = self.until_f2(device, clock)
        device.ui_works = False
        by = by_flow(sm.run_flows(ctx, {'F3'}))
        self.assertEqual(by['F3'].status, sm.FAIL, by['F3'])
        self.assertIn('did not reach the main menu', by['F3'].reason)

    def test_a_log_that_cannot_be_removed_is_not_run(self):
        clock = FakeClock()
        device = FakeDevice(clock)
        ctx, by = self.until_f2(device, clock)
        original = device.as_app

        def keeps_the_log(inner):
            if inner.startswith('rm -f') and 'scratch/game' in inner:
                return 0, '', ''
            return original(inner)
        device.as_app = keeps_the_log
        by = by_flow(sm.run_flows(ctx, {'F3'}))
        self.assertEqual(by['F3'].status, sm.NOT_RUN, by['F3'])
        self.assertIn('log could not be removed', by['F3'].reason)

    def test_a_marker_that_cannot_be_removed_before_a_quiet_start_is_not_run(self):
        clock = FakeClock()
        device = FakeDevice(clock)
        ctx, by = self.until_f2(device, clock)
        original = device.as_app

        def keeps_the_marker(inner):
            if inner.startswith('rm -f') and MARKER in inner:
                device.marker = True
                return 0, '', ''
            return original(inner)
        device.as_app = keeps_the_marker
        by = by_flow(sm.run_flows(ctx, {'F3'}))
        self.assertEqual(by['F3'].status, sm.NOT_RUN, by['F3'])
        self.assertIn('marker file', by['F3'].reason)

    def test_the_previous_log_is_the_name_the_game_gives_it(self):
        path = os.path.join(REPO, 'game', 'sim', 'Scratch.hpp')
        with open(path, encoding='utf-8') as f:
            header = f.read()
        self.assertIn('kLogFileName = "magicportals.log"', header)
        self.assertIn('kPreviousLogFileName = "magicportals.prev.log"', header)
        with open(os.path.join(REPO, 'game', 'android', 'AndroidManifest.xml'), encoding='utf-8') as f:
            log = sm.parse_manifest(f.read()).log_path
        self.assertEqual(log, 'Magic Portals/scratch/magicportals.log')
        self.assertEqual(sm.previous_log_path(log), 'Magic Portals/scratch/magicportals.prev.log')

    def test_the_glue_holds_the_game_until_the_dialog_is_closed_and_the_report_reads_the_log(self):
        base = os.path.join(REPO, 'engine', 'src', 'platform', 'android')
        app, java = os.path.join(base, 'AndroidApp.cpp'), os.path.join(base, 'java', 'com', 'ivancvetanovic', 'supersonic', 'PostMortem.java')
        if not (os.path.isfile(app) and os.path.isfile(java)):
            skip_loudly(self, 'engine/ is not checked out: the tie of the hold to AndroidApp.cpp is NOT checked')
        with open(app, encoding='utf-8') as f:
            native = f.read()
        self.assertRegex(native, r'while \(!app->destroyRequested && g_holdStart\.load\(\)')
        self.assertLess(native.index('g_holdStart.load()'), native.index('SupersonicMain(1'), 'the hold comes before the game\'s main')
        with open(java, encoding='utf-8') as f:
            glue = f.read()
        self.assertIn('new File(context.getFilesDir(), logPath)', glue, 'the report is built from the game\'s log, so the dialog-expected start leaves it alone')


class TheTimeouts(unittest.TestCase):
    def test_every_wait_is_one_table_and_none_is_unbounded(self):
        self.assertEqual(sorted(sm.TIMEOUTS), ['dialog', 'exit_record', 'install', 'level', 'menu', 'no_dialog', 'poll', 'process_gone', 'settle', 'unpack'])
        for key, value in sm.TIMEOUTS.items():
            self.assertGreater(value, 0, key)
            self.assertLessEqual(value, 300, key)

    def test_a_poll_gives_up_at_its_timeout_on_the_clock_it_was_given(self):
        clock = FakeClock()
        ctx = make_context(FakeDevice(clock), clock)
        self.assertIsNone(ctx.poll(lambda: False, 10))
        self.assertGreaterEqual(clock.t, 10)
        self.assertLess(clock.t, 12)
        self.assertEqual(ctx.poll(lambda: 'yes', 10), 'yes')

    def test_the_menu_taps_are_the_coordinates_of_the_full_games_menu(self):
        self.assertEqual(sm.MENU_TAPS, ((640, 475), (520, 350), (370, 90)))


class TheCommandLine(unittest.TestCase):
    def test_a_missing_manifest_is_not_run_not_a_traceback(self):
        err = io.StringIO()
        with contextlib.redirect_stderr(err):
            code = sm.main(['--apk', 'x.apk', '--runner', 'raw', '--serial', 's', '--manifest', os.path.join(HERE, 'no_such_manifest.xml')])
        self.assertEqual(code, sm.EXIT_NOT_RUN)
        self.assertIn('android_smoke:', err.getvalue())

    def test_a_raw_runner_needs_a_serial(self):
        err = io.StringIO()
        with contextlib.redirect_stderr(err):
            code = sm.main(['--apk', 'x.apk', '--runner', 'raw'])
        self.assertEqual(code, sm.EXIT_NOT_RUN)
        self.assertIn('serial', err.getvalue())

    def test_help_names_the_status_in_plain_words(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out), self.assertRaises(SystemExit):
            sm.main(['--help'])
        self.assertIn('NO FLOW HAS RUN ON ANY EMULATOR OR PHONE', out.getvalue())


if __name__ == '__main__':
    unittest.main()
