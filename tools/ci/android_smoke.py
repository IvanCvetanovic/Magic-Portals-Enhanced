#!/usr/bin/env python3
"""The Android smoke flows of planning step 209 (PORT-11): the crash-report dialog and the start of the game, driven through ONE emulator or device. Standard library only.

    python tools/ci/android_smoke.py --apk APK --runner raw --serial emulator-5554 --out DIR [--adb PATH]           a CI runner's emulator (raw adb, always `-s <serial>`)
    python tools/ci/android_smoke.py --apk APK --runner guarded [--slot impl] --out DIR                             this repository's emulator on the maker's PC
    options: [--manifest FILE] [--package P] [--activity A] [--flows F1,F2,...] [--content full|fixture|none] [--json FILE] [--shots DIR]

STATUS, PLAIN. This file was WRITTEN AND TESTED AGAINST INVENTED OUTPUT (tools/ci/test_android_smoke.py: invented uiautomator XML and invented adb text; no recorded real
output of `uiautomator dump` or `dumpsys activity exit-info` exists in this repository). NO FLOW HAS RUN ON ANY EMULATOR OR PHONE. The first real run is a dispatch of
.github/workflows/android-emulator.yml, or a run on the maker's PC when 6 GiB of memory is free (tools/android_emulator.py refuses to start an emulator below that).
Every parser here is tolerant of what it was not shown, and says NOT RUN, not PASS, when it cannot read what a flow asks.

WHY. The engine's Java glue (PostMortem.java, SupersonicActivity.java) shows, at the start after a run that ended unexpectedly, a dialog titled `<name> closed unexpectedly last time`
(Portuguese `O <name> fechou sozinho da ultima vez`). It has NEVER run on a device: this PC has too little memory for the emulator, and the unit tests cover the wording
(ReportText.java) and not the glue. These flows make the glue show itself and READ the dialog from the UI hierarchy, so a title that is wrong, an accent that is lost, a dialog
that never comes or one that comes twice is a FAIL with the text it found.

THE FLOWS. Each reports PASS, FAIL or NOT RUN with its reason, and none raises:
  F1  install the APK, start it, the first-run unpack completes and the game reaches its main menu (the game's own log says `the main menu is next`)
  F2  end the game unexpectedly while it is on the screen, start it again WITHOUT the scripted-run marker file, dump the UI hierarchy and ASSERT the dialog: its title is exactly
      `<name> closed unexpectedly last time`, the Share button and the OK button exist, OK closes it and the game starts
  F3  a second start, after that dialog was dismissed, shows no dialog
  F4  a user's Home, Back (on the main menu) and a force-stop show no dialog at the next start
  F5  with the app's language Portuguese, the dialog's title is `O <name> fechou sozinho da ultima vez` (with the accent) and its button reads Compartilhar
  F6  a level can be started from the menu by taps, and the game's log says `the level is up`
`--content none` (an APK with no game in it) runs F1's install and start only: such a game ends itself with an error at once, which is a report of its own and not what F2 to F6 test.

HOW THE GAME IS ENDED (F2, F5): the methods are tried in this order (CRASH_METHODS) and the first that leaves an exit record the report OFFERS is used: a native crash of the
foreground game (`run-as <package> kill -SEGV <pid>`, the way step 204 measured on API 30), a Java crash (`am crash <package>`), a SIGKILL of a process on the screen
(`run-as <package> kill -9 <pid>`) and, last, `am kill <package>`, which only kills a process that is safe to kill and so ends nothing of a game on the screen. A method that
leaves no record the report offers is NOT used, and the flow says which methods it tried. If none does, the flow is NOT RUN (never FAIL: no record means the report was never
asked). Whether a record is offered is the glue's rule, copied in offers_report and tied by a test to PostMortem.java.

EACH DIALOG CONSUMES ITS RECORD (`markShown`, at most three offers), so F2 and F5 each induce an end of their own, and F3 and F4 start only after the dialog was dismissed.

THE GAME'S LOG IS EVIDENCE OF THIS START ONLY IF IT IS NEW (fix round 1 of step 209, defect D1). The glue builds the report in onCreate and holds the native main until the dialog is
closed (engine AndroidApp.cpp: g_holdStart before SupersonicMain), and the game moves its log to `magicportals.prev.log` only when main starts (game/main.cpp KeepPreviousLog). So from
`am start` until the dialog is closed the log on the device is still the PREVIOUS run's, `the main menu is next` included. Therefore: every start that is judged by the log removes the log
and the run before's first (Context.start, only when no process of the app exists); the one start that must show the dialog leaves the log alone (it is the report's input) and removes it
once the dialog has been read, before OK is tapped (main has not run, nothing has the file open); and the UI dump file is removed before each dump, so that a dump that fails is an error and
not the last dump again. A start that is judged by a log that could not be cleared, or by a UI that could not be read, is NOT RUN. What this rests on, and has not been seen on a device:
the hold. tools/ci/test_android_smoke.py ties it to the engine's source text.

THE TWO RUNNERS. `Runner.run(args)` is `adb -s <serial> args` and nothing else. RawRunner is plain adb for a runner whose only device is the one it started. GuardedRunner goes through
tools/android_emulator.py (imported when it is used), which names only its own two slots, refuses the adb verbs that act on the server or on every device, and logs every call.
"""
from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
import time
import xml.etree.ElementTree as ET
from collections import namedtuple

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS = os.path.dirname(HERE)
REPO = os.path.dirname(TOOLS)

# ---- results ---------------------------------------------------------------------------------------------------------------------------
PASS, FAIL, NOT_RUN = 'PASS', 'FAIL', 'NOT RUN'
EXIT_PASS, EXIT_FAIL, EXIT_NOT_RUN = 0, 1, 3  # 3: no flow failed and at least one did not run, which is never a pass

Result = namedtuple('Result', 'flow status reason evidence')

# ---- the words of the dialog (engine/src/platform/android/java/.../ReportText.java and PostMortem.java) -----------------------------------
TITLE_ENGLISH = '%s closed unexpectedly last time'
TITLE_PORTUGUESE = 'O %s fechou sozinho da \u00faltima vez'  # the accent is U+00FA, written as an escape so that no encoding of this file can lose it
SHARE_ENGLISH, SHARE_PORTUGUESE = 'Share', 'Compartilhar'
BUTTON_OK = 'OK'
# The ids a standard AlertDialog gives its parts (android.R.id): the glue builds one with setTitle, setMessage, setPositiveButton (OK) and setNeutralButton (Share).
ID_TITLE, ID_MESSAGE, ID_OK, ID_SHARE = 'android:id/alertTitle', 'android:id/message', 'android:id/button1', 'android:id/button3'
TITLE_TAIL_ENGLISH, TITLE_TAIL_PORTUGUESE = ' closed unexpectedly last time', ' fechou sozinho da \u00faltima vez'

# ---- the game's own log lines (game/MagicPortalsLayer.cpp, game/android/AndroidMain.cpp, engine/src/core/SupersonicApp.cpp) ------------------
LOG_MENU = re.compile(r'the main menu is next')
LOG_LEVEL_UP = re.compile(r'level start: the load ran on tick \d+ .*the level is up')
LOG_LEVEL_REFUSED = re.compile(r'level start: the load ran on tick \d+ .*the level was refused: (.*)')
LOGCAT_UNPACKED = re.compile(r'Unpacked \d+ files|already unpacked')
LOGCAT_FIRST_FRAME = re.compile(r'First DrawFrame returned')

# The run before's log sits beside the log as `<stem>.prev.log` (game/sim/Scratch.hpp kPreviousLogFileName, tied by a test); the file `uiautomator dump` writes is removed before each dump.
PREVIOUS_LOG_SUFFIX = '.prev.log'
UI_DUMP_FILE = '/sdcard/mp_smoke_ui.xml'

KEY_HOME, KEY_BACK = 'KEYCODE_HOME', 'KEYCODE_BACK'
API_FOR_THE_REPORT = 30  # ApplicationExitInfo: Android 11

# Where a tap goes to start a level from the main menu of the FULL game at 1280x720 landscape (the coordinates step 205's flows used on the emulator: Play, chapter 1's door,
# the first tile of the grid). The fixture's menu may lay these out otherwise: a tap route that does not reach a level is then a FAIL of F6 that says so.
MENU_TAPS = ((640, 475), (520, 350), (370, 90))
TAP_HOLD_MS = 250  # a zero-length tap is missed under software GL (learned on the parity rig)
TAP_GAP_SECONDS = 3.5

# Every wait of a flow, in seconds: one table, so that a test can shrink them and a reader can see them.
TIMEOUTS = {'install': 300, 'unpack': 180, 'menu': 120, 'process_gone': 25, 'exit_record': 30, 'dialog': 90, 'no_dialog': 60, 'level': 90, 'poll': 2.0, 'settle': 4.0}

# What a NOT RUN says (constants, so that the tests and the documents quote the same words).
NR_NO_DEVICE = 'the device does not answer'
NR_API_TOO_OLD = 'the report needs Android 11 (API 30) or later and this device is API %s'
NR_NOT_DEBUGGABLE = 'run-as is refused for the package: the APK is not debuggable, so the flags file, the marker and the game log cannot be reached'
NR_NO_CONTENT = 'the APK has no game in it (--content none): the game ends itself with an error at once, which is a report of its own and not what this flow tests'
NR_NO_RECORD = 'no way of ending the game left an exit record the report offers: %s'
NR_NO_UI = 'the UI hierarchy could not be read: %s'
NR_NO_LOCALE = 'the app\'s language could not be set to Portuguese: %s'
NR_PRIOR = 'flow %s did not pass, so the game is not known to be running at its menu'
NR_NO_EXIT_INFO = 'the exit records could not be read (dumpsys activity exit-info answered nothing parseable)'
NR_LOG_STAYS = 'the game\'s log could not be removed before the start (%s), so a line in it cannot be told from the run before\'s'
NR_MARKER_STAYS = 'the scripted-run marker file %s could not be removed, and a start with it never shows the dialog'
NR_UI_UNREAD = 'the start reached its menu by the game\'s fresh log, but the UI hierarchy could not be read once, so "no dialog" was not looked at: %s'


class AdbError(Exception):
    """The runner could not run a command (the guard refused it, adb is missing, the call timed out). A flow turns it into NOT RUN, never into a traceback."""


class UiError(Exception):
    """The UI hierarchy text could not be read."""


class CannotJudge(Exception):
    """The flow cannot tell this start from the one before (the log or the marker file would not go), so whatever it saw would not be evidence. run_flows turns it into NOT RUN."""


# ---- the runners -----------------------------------------------------------------------------------------------------------------------

class Runner:
    """What a flow may ask of a device. `run(args)` is the one primitive: `adb -s <serial> args...`. Everything else is built on it, so a stub that implements `run` stands in
    for either runner."""
    name = 'runner'

    def run(self, args, timeout=120, binary=False, stdin=None):  # pragma: no cover - the subclasses
        raise NotImplementedError

    def shell(self, command, timeout=60):
        return self.run(['shell', command], timeout=timeout)

    def run_as(self, package, command, timeout=60):
        """`command` as the app's own user, in its data folder (a debuggable APK only)."""
        return self.shell("run-as %s sh -c '%s'" % (package, command.replace("'", "'\\''")), timeout=timeout)

    def key(self, name):
        return self.shell('input keyevent %s' % name, timeout=30)

    def tap(self, x, y, ms=TAP_HOLD_MS):
        return self.shell('input swipe %d %d %d %d %d' % (x, y, x, y, ms), timeout=30)

    def screenshot_png(self):
        rc, out, err = self.run(['exec-out', 'screencap', '-p'], timeout=60, binary=True)
        if rc != 0 or not out.startswith(b'\x89PNG'):
            raise AdbError('screencap failed (%s)' % rc)
        return out


class RawRunner(Runner):
    """Plain adb, always with `-s <serial>`. For a CI runner, where the emulator started by the job is the only device."""
    name = 'raw'

    def __init__(self, serial, adb='adb'):
        if not serial:
            raise AdbError('a raw runner needs a serial (--serial): it never talks to "the only device"')
        self.serial, self.adb = serial, adb

    def run(self, args, timeout=120, binary=False, stdin=None):
        argv = [self.adb, '-s', self.serial] + [str(a) for a in args]
        try:
            done = subprocess.run(argv, input=stdin, capture_output=True, timeout=timeout)
        except subprocess.TimeoutExpired:
            return 124, b'' if binary else '', b'' if binary else 'timeout after %ss' % timeout
        except OSError as e:
            raise AdbError('cannot run %s: %s' % (self.adb, e))
        if binary:
            return done.returncode, done.stdout, done.stderr
        return done.returncode, done.stdout.decode('utf-8', 'replace').replace('\r\n', '\n'), done.stderr.decode('utf-8', 'replace').replace('\r\n', '\n')


class GuardedRunner(Runner):
    """This repository's tools/android_emulator.py: the only way an emulator is driven on the maker's PC. Imported when this runner is made, so that the other runner (and the
    tests) need nothing of it. It names two slots and refuses the verbs that act on the adb server, and a refusal reaches the flow as an AdbError."""
    name = 'guarded'

    def __init__(self, slot='impl', module=None):
        if module is None:
            sys.path.insert(0, TOOLS)
            try:
                import android_emulator as module  # noqa: E402  (tools/android_emulator.py)
            except ImportError as e:
                raise AdbError('tools/android_emulator.py cannot be imported: %s' % e)
        self.emu = module
        try:
            self.emu.use_slot(slot)
        except self.emu.EmuError as e:
            raise AdbError(str(e))
        self.slot = slot

    def run(self, args, timeout=120, binary=False, stdin=None):
        try:
            return self.emu.adb(*args, timeout=timeout, binary=binary, stdin=stdin)
        except self.emu.EmuError as e:
            raise AdbError(str(e))


# ---- the app's facts, from its manifest -------------------------------------------------------------------------------------------------

AppFacts = namedtuple('AppFacts', 'package activity label marker log_path title_name')
ANDROID_NS = '{http://schemas.android.com/apk/res/android}'
META_MARKER, META_LOG, META_TITLE = 'supersonic.reportSkipWhenFile', 'supersonic.reportLog', 'supersonic.reportTitle'


def parse_manifest(text):
    """Package, launcher activity, label, the scripted-run marker file, the game's log (relative to the files folder) and the name the dialog's title carries, from the
    AndroidManifest.xml SOURCE (not the compiled one): the same rule as ReportText.chooseName (the key, else the application's label; None when it is a resource reference)."""
    try:
        root = ET.fromstring(text)
    except ET.ParseError as e:
        raise ValueError('the manifest is not XML: %s' % e)
    package = root.get('package')
    app = root.find('application')
    if not package or app is None:
        raise ValueError('the manifest names no package or no application')
    label = (app.get(ANDROID_NS + 'label') or '').strip()
    activity, meta = None, {}
    for node in app.findall('activity'):
        launcher = any(any(c.get(ANDROID_NS + 'name') == 'android.intent.category.LAUNCHER' for c in f.findall('category')) for f in node.findall('intent-filter'))
        if launcher and activity is None:
            activity = node.get(ANDROID_NS + 'name')
            for m in node.findall('meta-data'):
                meta[m.get(ANDROID_NS + 'name')] = m.get(ANDROID_NS + 'value')
    if not activity:
        raise ValueError('the manifest has no launcher activity')
    declared = (meta.get(META_TITLE) or '').strip()
    name = declared or (None if label.startswith('@') or not label else label)
    return AppFacts(package, activity, label, meta.get(META_MARKER) or '', meta.get(META_LOG) or '', name)


def previous_log_path(log_path):
    """The run before's log for a log path relative to the files folder: beside it, `<stem>.prev.log` (Scratch.hpp kPreviousLogFileName)."""
    return os.path.splitext(log_path)[0] + PREVIOUS_LOG_SUFFIX


def expected_title(name, portuguese):
    """The dialog's title (ReportText.title): plain concatenation, the article in front of the name in Portuguese."""
    if not name:
        raise ValueError('the game has no name to put in the title')
    return (TITLE_PORTUGUESE if portuguese else TITLE_ENGLISH) % name


# ---- the UI hierarchy ------------------------------------------------------------------------------------------------------------------

Node = namedtuple('Node', 'text resource_id class_name package content_desc clickable bounds')
BOUNDS = re.compile(r'\[(-?\d+),(-?\d+)\]\[(-?\d+),(-?\d+)\]')


def parse_ui_nodes(text):
    """The nodes of a `uiautomator dump`: the text may start with noise and end with `UI hierchary dumped to: ...` (the tool's own spelling), so only what lies between
    <hierarchy and </hierarchy> is read. Raises UiError for text that holds no hierarchy or one that is not XML."""
    start, end = text.find('<hierarchy'), text.rfind('</hierarchy>')
    if start < 0 or end < 0:
        raise UiError('no <hierarchy> in the text (%d characters; it starts: %r)' % (len(text), text[:60]))
    try:
        root = ET.fromstring(text[start:end + len('</hierarchy>')])
    except ET.ParseError as e:
        raise UiError('the hierarchy is not XML: %s' % e)
    nodes = []
    for element in root.iter('node'):
        m = BOUNDS.fullmatch(element.get('bounds') or '')
        nodes.append(Node(element.get('text') or '', element.get('resource-id') or '', element.get('class') or '', element.get('package') or '',
                          element.get('content-desc') or '', element.get('clickable') == 'true', tuple(int(g) for g in m.groups()) if m else None))
    return nodes


def node_center(node):
    if node.bounds is None:
        raise ValueError('the node has no bounds')
    x1, y1, x2, y2 = node.bounds
    return (x1 + x2) // 2, (y1 + y2) // 2


Dialog = namedtuple('Dialog', 'title message ok share found_by')


def find_report_dialog(nodes, package=None):
    """The crash-report dialog among the nodes, or None. By the id the platform gives an AlertDialog's title; failing that, by a title text that ends the way a report's does (so a
    dialog whose id is not what is assumed here is still read, and `found_by` says so)."""
    scope = [n for n in nodes if not package or n.package == package]
    title = next((n for n in scope if n.resource_id == ID_TITLE), None)
    found_by = 'id %s' % ID_TITLE
    if title is None:
        title = next((n for n in scope if n.text.endswith((TITLE_TAIL_ENGLISH, TITLE_TAIL_PORTUGUESE))), None)
        found_by = 'its text (no node has the id %s)' % ID_TITLE
    if title is None:
        return None
    message = next((n for n in scope if n.resource_id == ID_MESSAGE), None)
    ok = next((n for n in scope if n.resource_id == ID_OK), None)
    share = next((n for n in scope if n.resource_id == ID_SHARE), None)
    return Dialog(title, message, ok, share, found_by)


def check_dialog(dialog, want_title, want_share):
    """The problems of a dialog against what the flow expects (an empty list is a pass). The TITLE is compared exactly; the buttons' texts are compared without regard to case,
    because a button may be shown in capitals and what the accessibility text then holds was not seen."""
    problems = []
    if dialog is None:
        return ['there is no crash-report dialog on the screen']
    if dialog.title.text != want_title:
        problems.append('the title is %r and the title of the report is %r' % (dialog.title.text, want_title))
    if dialog.ok is None:
        problems.append('no OK button (%s)' % ID_OK)
    elif dialog.ok.text.strip().lower() != BUTTON_OK.lower():
        problems.append('the OK button reads %r' % dialog.ok.text)
    if dialog.share is None:
        problems.append('no Share button (%s)' % ID_SHARE)
    elif dialog.share.text.strip().lower() != want_share.lower():
        problems.append('the Share button reads %r and %r was expected' % (dialog.share.text, want_share))
    return problems


# ---- the exit records ------------------------------------------------------------------------------------------------------------------

ExitRecord = namedtuple('ExitRecord', 'index reason reason_name status importance timestamp pid')
# android.app.ApplicationExitInfo's reason numbers (the constants PostMortem.isUnexpected switches on).
REASON_UNKNOWN, REASON_EXIT_SELF, REASON_SIGNALED, REASON_LOW_MEMORY, REASON_CRASH, REASON_CRASH_NATIVE, REASON_ANR, REASON_INITIALIZATION_FAILURE = 0, 1, 2, 3, 4, 5, 6, 7
REASON_PERMISSION_CHANGE, REASON_EXCESSIVE_RESOURCE_USAGE, REASON_USER_REQUESTED, REASON_USER_STOPPED, REASON_DEPENDENCY_DIED, REASON_OTHER = 8, 9, 10, 11, 12, 13
ALWAYS_OFFERED = (REASON_CRASH, REASON_CRASH_NATIVE, REASON_ANR, REASON_INITIALIZATION_FAILURE, REASON_EXCESSIVE_RESOURCE_USAGE)
OFFERED_WHEN_ON_THE_SCREEN = (REASON_UNKNOWN, REASON_LOW_MEMORY, REASON_SIGNALED, REASON_OTHER, REASON_DEPENDENCY_DIED, REASON_PERMISSION_CHANGE)
FOREGROUND_AT_MOST = 125  # PostMortem.FOREGROUND_AT_MOST: IMPORTANCE_FOREGROUND_SERVICE; a process on the screen is 100


def parse_exit_info(text):
    """The records of `dumpsys activity exit-info <package>`, newest first as the platform prints them. Each is found by its `ApplicationExitInfo #n:` header and read for
    `reason=<n> (<name>)`, `status=`, `importance=`, `timestamp=` and `pid=`; a block with no reason is skipped. The layout was not shown to the author (invented text in the
    tests), so every field is optional and an empty list means "nothing parseable", which the flows say aloud."""
    records = []
    for block in re.split(r'(?=ApplicationExitInfo #\d+:)', text)[1:]:
        reason = re.search(r'reason=(\d+)(?:\s*\(([^)]*)\))?', block)
        if not reason:
            continue

        def number(pattern):
            m = re.search(pattern, block)
            return int(m.group(1)) if m else None
        stamp = re.search(r'timestamp=(\d{4}-\d\d-\d\d \d\d:\d\d:\d\d(?:\.\d+)?|\d+)', block)
        records.append(ExitRecord(int(re.match(r'ApplicationExitInfo #(\d+):', block).group(1)), int(reason.group(1)), (reason.group(2) or '').strip(),
                                  number(r'\bstatus=(-?\d+)'), number(r'\bimportance=(\d+)'), stamp.group(1) if stamp else '', number(r'\bpid=(\d+)')))
    return records


def offers_report(record):
    """Whether the engine's report offers this end at the next start: PostMortem.isUnexpected, copied (a test ties the numbers and the 125 to the Java source)."""
    if record.reason in ALWAYS_OFFERED:
        return True
    if record.reason == REASON_EXIT_SELF:
        return record.status is not None and record.status != 0
    if record.reason in OFFERED_WHEN_ON_THE_SCREEN:
        return record.importance is not None and record.importance <= FOREGROUND_AT_MOST
    return False


def record_key(record):
    return (record.timestamp, record.pid, record.reason)


def new_records(before, after):
    """The records of `after` that `before` did not have (by timestamp, pid and reason: the index shifts as new ones are put in front)."""
    seen = {record_key(r) for r in before}
    return [r for r in after if record_key(r) not in seen]


CrashMethod = namedtuple('CrashMethod', 'name command needs_run_as why')
# THE ORDER IS A DECISION and a test pins it: the one measured (a native crash, step 204), then the other ways the platform records an end of a process on the screen, and
# `am kill` last because it only ends a process that is safe to kill, which a game on the screen is not.
CRASH_METHODS = (
    CrashMethod('native-crash', 'run-as {package} kill -SEGV {pid}', True, 'a native crash of the foreground game (REASON_CRASH_NATIVE): the way step 204 measured on API 30'),
    CrashMethod('java-crash', 'am crash {package}', False, 'a Java crash raised in the app (REASON_CRASH)'),
    CrashMethod('signal-kill', 'run-as {package} kill -9 {pid}', True, 'SIGKILL of a process on the screen (REASON_SIGNALED, importance foreground)'),
    CrashMethod('am-kill', 'am kill {package}', False, 'a kill the platform allows only for a process that is safe to kill: expected to end nothing of a game on the screen'),
)


def crash_command(method, package, pid):
    return method.command.format(package=package, pid=pid)


# ---- the context a flow runs in --------------------------------------------------------------------------------------------------------

class Clock:
    def __init__(self, now=time.monotonic, sleep=time.sleep):
        self.now, self.sleep = now, sleep


class Context:
    """The device, the app's facts and what a run needs: one object, so that a flow is a function of it and a test can hand it a stub runner and a clock that does not wait."""

    def __init__(self, runner, facts, apk=None, content='full', clock=None, timeouts=None, out_dir=None, shots_dir=None, say=None):
        self.runner, self.facts, self.apk, self.content = runner, facts, apk, content
        self.clock = clock or Clock()
        self.timeouts = dict(TIMEOUTS, **(timeouts or {}))
        self.out_dir, self.shots_dir = out_dir, shots_dir
        self.said = []
        self._say = say
        self.running = False  # whether the game is known to be on the screen at its menu (F1 sets it, a flow that ends the game clears it)
        self.crash_method = None  # the method that worked (F2 sets it, F5 uses it)
        self.dismissed = False  # whether a report dialog was shown and closed with OK (F2 sets it; F3 needs it)
        self.dumps = 0
        self.api = None
        self.debuggable = None

    # --- small things
    def say(self, text):
        self.said.append(text)
        if self._say:
            self._say(text)

    def wait(self, seconds):
        self.clock.sleep(seconds)

    def poll(self, check, timeout, interval=None):
        """check() until it returns something true (that is the result) or `timeout` seconds have passed (None)."""
        deadline = self.clock.now() + timeout
        step = self.timeouts['poll'] if interval is None else interval
        while True:
            value = check()
            if value:
                return value
            if self.clock.now() >= deadline:
                return None
            self.clock.sleep(step)

    # --- the device
    def sh(self, command, timeout=60):
        return self.runner.shell(command, timeout=timeout)

    def as_app(self, command, timeout=60):
        return self.runner.run_as(self.facts.package, command, timeout=timeout)

    def pid(self):
        rc, out, _ = self.sh('pidof %s' % self.facts.package, timeout=20)
        m = re.search(r'\d+', out or '')
        return int(m.group(0)) if rc == 0 and m else None

    def start(self, clear_log=True):
        """`am start`. With `clear_log` (the default) the game's log and the run before's are removed FIRST when no process of the app exists, so that a line found in the log
        afterwards was written by THIS start (see the header: until the glue's dialog is closed, the log on the device is the previous run's). The one start that must show the
        dialog passes clear_log=False: the log is the report's input. A log that will not go raises CannotJudge. Not debuggable: nothing can be read or removed, and every flow that
        reads the log says so itself."""
        if clear_log and self.debuggable is not False and self.pid() is None:
            self.must_clear_game_log()
        rc, out, err = self.sh('am start -n %s/%s' % (self.facts.package, self.facts.activity), timeout=60)
        text = (out + err).strip()
        if rc != 0 or 'Error' in text:
            raise AdbError('am start failed (%s): %s' % (rc, text[:200]))
        return text

    def stop(self):
        """Ends the app the way a person leaves it: Home first (the emulator once died when an app presenting frames was force-stopped, step 204), then a force-stop."""
        self.runner.key(KEY_HOME)
        self.wait(self.timeouts['settle'])
        self.sh('am force-stop %s' % self.facts.package, timeout=60)
        self.wait(2.0)

    def game_log(self):
        rc, out, _ = self.as_app("cat '%s'" % ('files/' + self.facts.log_path), timeout=60)
        return out if rc == 0 else ''

    def logcat(self):
        rc, out, _ = self.runner.run(['logcat', '-d', '-v', 'threadtime'], timeout=120)
        return out if rc == 0 else ''

    def marker_files(self):
        base = 'files/' + self.facts.marker
        return [base, os.path.splitext(base)[0] + '.used']

    def log_files(self):
        """The game's log and the run before's, relative to the app's data folder (none when the manifest names no log)."""
        if not self.facts.log_path:
            return []
        return ['files/' + self.facts.log_path, 'files/' + previous_log_path(self.facts.log_path)]

    def present(self, paths):
        rc, out, _ = self.as_app('ls ' + ' '.join("'%s'" % p for p in paths) + ' 2>/dev/null', timeout=30)
        return bool(out.strip())

    def remove(self, paths):
        """`rm -f` as the app, then whether none of the files is left."""
        self.as_app('rm -f ' + ' '.join("'%s'" % p for p in paths), timeout=30)
        return not self.present(paths)

    def marker_present(self):
        return self.present(self.marker_files())

    def remove_marker(self):
        return self.remove(self.marker_files())

    def clear_game_log(self):
        """Removes the game's log and the run before's. True when none is left (also when there is no log to name)."""
        files = self.log_files()
        return self.remove(files) if files else True

    def must_clear_game_log(self):
        if not self.clear_game_log():
            raise CannotJudge(NR_LOG_STAYS % ', '.join(self.log_files()))

    def ui_nodes(self):
        """The nodes on the screen now. Raises UiError. The dump file is removed first: a dump that fails must be an error, and not the last dump read again."""
        rc, out, err = self.sh('rm -f %s; uiautomator dump %s > /dev/null 2>&1; cat %s' % (UI_DUMP_FILE, UI_DUMP_FILE, UI_DUMP_FILE), timeout=90)
        text = out if rc == 0 else ''
        if self.out_dir and text:
            self.dumps += 1
            self.keep('ui_dump_%03d.xml' % self.dumps, text)
        if not text.strip():
            raise UiError('uiautomator dump gave nothing (rc %s): %s' % (rc, err.strip()[:120]))
        return parse_ui_nodes(text)

    def exit_records(self):
        rc, out, _ = self.sh('dumpsys activity exit-info %s' % self.facts.package, timeout=60)
        return parse_exit_info(out) if rc == 0 else []

    def keep(self, name, text):
        if not self.out_dir:
            return
        try:
            os.makedirs(self.out_dir, exist_ok=True)
            with open(os.path.join(self.out_dir, name), 'w', encoding='utf-8', newline='\n') as f:
                f.write(text)
        except OSError:
            pass

    def shot(self, name):
        if not self.shots_dir:
            return
        try:
            png = self.runner.screenshot_png()
            os.makedirs(self.shots_dir, exist_ok=True)
            with open(os.path.join(self.shots_dir, name + '.png'), 'wb') as f:
                f.write(png)
        except (AdbError, OSError):
            pass


# ---- the flows -------------------------------------------------------------------------------------------------------------------------

def need_a_running_game(ctx, flow, prior='F1'):
    if not ctx.running:
        return Result(flow, NOT_RUN, NR_PRIOR % prior, [])
    return None


def need_content(ctx, flow):
    if ctx.content == 'none':
        return Result(flow, NOT_RUN, NR_NO_CONTENT, [])
    return None


def need_the_report(ctx, flow):
    if ctx.api is not None and ctx.api < API_FOR_THE_REPORT:
        return Result(flow, NOT_RUN, NR_API_TOO_OLD % ctx.api, [])
    if ctx.debuggable is False:
        return Result(flow, NOT_RUN, NR_NOT_DEBUGGABLE, [])
    return None


def menu_line(log_text):
    """The line of a log's text that says the main menu is next, or None."""
    for line in log_text.split('\n'):
        if LOG_MENU.search(line):
            return line.strip()
    return None


def menu_seen(ctx):
    """One look at the game's own log for the line that says the main menu is next: the line, or None. It is the evidence of THIS start only if the log was removed before the
    start (Context.start does it) or the game's main has run since (the dialog's OK was tapped after the log was removed)."""
    return menu_line(ctx.game_log())


def wait_for_menu(ctx, timeout=None):
    """Polls the game's own log for that line. Returns it or None."""
    return ctx.poll(lambda: menu_seen(ctx), timeout or ctx.timeouts['menu'])


def flow_f1(ctx):
    """F1: install, start, the first-run unpack completes, the menu."""
    flow = 'F1'
    ev = []
    if ctx.apk is None or not os.path.isfile(ctx.apk):
        return Result(flow, NOT_RUN, 'no APK to install (%s)' % ctx.apk, ev)
    ctx.sh('am force-stop %s' % ctx.facts.package, timeout=60)
    ctx.runner.run(['uninstall', ctx.facts.package], timeout=120)
    rc, out, err = ctx.runner.run(['install', '-r', ctx.apk], timeout=ctx.timeouts['install'])
    ev.append('install: %s' % (out + err).strip().replace('\n', ' | ')[:200])
    if rc != 0 or 'Success' not in out:
        return Result(flow, FAIL, 'the APK did not install: %s' % (out + err).strip()[:200], ev)
    rc, out, _ = ctx.as_app('id', timeout=30)
    ctx.debuggable = rc == 0 and 'uid=' in out
    ctx.runner.run(['logcat', '-c'], timeout=30)
    ctx.remove_marker()
    try:
        ctx.start()
    except AdbError as e:
        return Result(flow, FAIL, 'the installed app did not start: %s' % e, ev)
    if ctx.content == 'none':
        return Result(flow, PASS, 'installed and started (am start accepted); the APK has no game in it, so the unpack and the menu were not asked for', ev)
    unpacked = ctx.poll(lambda: next((l for l in ctx.logcat().split('\n') if LOGCAT_UNPACKED.search(l)), None), ctx.timeouts['unpack'])
    ev.append('unpack: %s' % (unpacked.strip()[:160] if unpacked else 'no "Unpacked N files" or "already unpacked" line in the log'))
    if not unpacked:
        return Result(flow, FAIL, 'the first-run unpack did not finish within %ds' % ctx.timeouts['unpack'], ev)
    menu = wait_for_menu(ctx)
    ev.append('menu: %s' % (menu[:160] if menu else 'the game\'s log never said the main menu is next'))
    if not menu:
        return Result(flow, FAIL, 'the game did not reach its main menu within %ds' % ctx.timeouts['menu'], ev)
    ctx.shot('f1_menu')
    ctx.running = True
    return Result(flow, PASS, 'installed, unpacked, and the log says the main menu is next', ev)


def induce_an_unexpected_end(ctx):
    """Ends the game while it is on the screen, trying CRASH_METHODS in order. Returns (method or None, a list of what each attempt gave)."""
    tried = []
    for method in CRASH_METHODS:
        if method.needs_run_as and ctx.debuggable is False:
            tried.append('%s: needs run-as, and the APK is not debuggable' % method.name)
            continue
        if not ctx.running or ctx.pid() is None:
            ctx.remove_marker()
            ctx.start()
            if not wait_for_menu(ctx):
                tried.append('%s: the game could not be put on the screen to end it' % method.name)
                continue
            ctx.running = True
        pid = ctx.pid()
        if pid is None and '{pid}' in method.command:
            tried.append('%s: the game\'s process id could not be read (pidof)' % method.name)
            continue
        before = ctx.exit_records()
        command = crash_command(method, ctx.facts.package, pid)
        rc, out, err = ctx.sh(command, timeout=60)
        gone = ctx.poll(lambda: ctx.pid() is None, ctx.timeouts['process_gone'])
        ctx.running = False
        if not gone:
            tried.append('%s (%s): the process %s was still there after %ds' % (method.name, command, pid, ctx.timeouts['process_gone']))
            continue
        fresh = ctx.poll(lambda: new_records(before, ctx.exit_records()), ctx.timeouts['exit_record'])
        if not fresh:
            tried.append('%s (%s): the process ended and no new exit record appeared' % (method.name, command))
            continue
        offered = [r for r in fresh if offers_report(r)]
        if not offered:
            tried.append('%s (%s): the record is %s (status %s, importance %s), which the report does not offer' % (method.name, command, fresh[0].reason_name or fresh[0].reason,
                                                                                                          fresh[0].status, fresh[0].importance))
            continue
        tried.append('%s (%s): record %s, status %s, importance %s: offered' % (method.name, command, offered[0].reason_name or offered[0].reason, offered[0].status,
                                                                             offered[0].importance))
        return method, tried
    return None, tried


def show_the_dialog(ctx, want_title, want_share, flow, restart=True):
    """Starts the game WITHOUT the scripted-run marker, waits for the dialog, reads it from the UI hierarchy, checks it, and taps OK. Returns (problems, evidence, hard_reason) where
    `hard_reason` is a NOT RUN reason (the UI could not be read) or None."""
    ev = []
    if not ctx.remove_marker():
        # a start WITH the marker lets the end go as seen and shows no dialog: the flow would pass or fail for the wrong reason
        return None, ev, NR_MARKER_STAYS % ctx.facts.marker
    ev.append('the marker %s is absent at the start' % ctx.facts.marker)
    log_before = ''
    if restart:
        ctx.runner.run(['logcat', '-c'], timeout=30)
        log_before = ctx.game_log()
        ctx.start(clear_log=False)  # the log is the report's input (collectReport reads it in onCreate): it is removed below, once the dialog was read
    state = {'error': None, 'nodes': None}

    def look():
        try:
            nodes = ctx.ui_nodes()
        except UiError as e:
            state['error'] = str(e)
            return None
        state['nodes'] = nodes
        return find_report_dialog(nodes, ctx.facts.package)
    dialog = ctx.poll(look, ctx.timeouts['dialog'])
    if dialog is None:
        if state['nodes'] is None:
            return None, ev, NR_NO_UI % (state['error'] or 'no dump was made')
        # the log was not removed before this start, so a menu line counts only if the log is not the one it was before the start (the game moves it aside when its main starts)
        log_now = ctx.game_log()
        started = log_now != log_before and menu_line(log_now)
        return ['no dialog appeared within %ds%s' % (ctx.timeouts['dialog'], '; the game started without one (its log is new and says the main menu is next)' if started else
                                                      '; the game did not reach its menu either (a held start with a dialog the dump cannot see?)')], ev, None
    ev.append('dialog found by %s: title %r, message %d characters' % (dialog.found_by, dialog.title.text, len(dialog.message.text) if dialog.message else 0))
    ctx.shot(flow.lower() + '_dialog')
    problems = check_dialog(dialog, want_title, want_share)
    # The report has been read, and the game's main has not run (the glue holds it while the dialog is open): the log is removed NOW, so that the menu line looked for after OK can only
    # be one that the game writes once the dialog is gone.
    ctx.must_clear_game_log()
    ev.append('the game\'s log was removed while the dialog was open, so the menu line after OK is the game\'s own')
    if dialog.ok is not None and dialog.ok.bounds is not None:
        x, y = node_center(dialog.ok)
        ctx.runner.tap(x, y)
        ctx.wait(ctx.timeouts['settle'])
        try:
            after = find_report_dialog(ctx.ui_nodes(), ctx.facts.package)
        except UiError as e:
            after = None
            ev.append('after OK the UI could not be read (%s); the game\'s new log decides (a menu line in it means the game\'s main ran, which the glue allows only once the dialog is closed)' % e)
        if after is not None:
            problems.append('OK was tapped at (%d, %d) and the dialog is still on the screen' % (x, y))
        else:
            ctx.dismissed = True
            menu = wait_for_menu(ctx, ctx.timeouts['menu'])
            if menu:
                ev.append('after OK: the game started (%s)' % menu[:80])
                ctx.running = True
            else:
                problems.append('OK closed the dialog and the game did not reach its menu within %ds' % ctx.timeouts['menu'])
    elif not problems:
        problems.append('the OK button has no position to tap')
    return problems, ev, None


def flow_f2(ctx):
    flow = 'F2'
    for check in (need_content(ctx, flow), need_the_report(ctx, flow), need_a_running_game(ctx, flow)):
        if check:
            return check
    if not ctx.facts.title_name:
        return Result(flow, NOT_RUN, 'the manifest gives the game no name to read the title against (a label that is a resource reference)', [])
    method, tried = induce_an_unexpected_end(ctx)
    if method is None:
        return Result(flow, NOT_RUN, NR_NO_RECORD % ' | '.join(tried), tried)
    ctx.crash_method = method
    problems, ev, hard = show_the_dialog(ctx, expected_title(ctx.facts.title_name, False), SHARE_ENGLISH, flow)
    ev = tried + ev
    if hard:
        return Result(flow, NOT_RUN, hard, ev)
    if problems:
        return Result(flow, FAIL, '; '.join(problems), ev)
    return Result(flow, PASS, 'the dialog read %r with Share and OK; OK closed it and the game started (the end was made by %s)' % (expected_title(ctx.facts.title_name, False), method.name), ev)


Quiet = namedtuple('Quiet', 'dialog menu ui_read evidence')


def quiet_start(ctx):
    """Starts the game (marker absent, the game's log removed first) and says whether a report dialog showed. Returns Quiet(dialog or None, the menu line or None, whether the UI
    hierarchy was read at least once, evidence). The log's menu line is the main evidence and it is a NEW log's (Context.start removed the old one): a dialog holds the native start
    back, so a game whose new log says the menu is next was not held. The UI dump is the second: a start whose UI could not be read even once is not reported as "no dialog" (the
    callers say NOT RUN). Stops at the first of a dialog and a menu line."""
    if not ctx.remove_marker():
        raise CannotJudge(NR_MARKER_STAYS % ctx.facts.marker)
    ctx.runner.run(['logcat', '-c'], timeout=30)
    ctx.start()
    seen = {'dialog': None, 'error': None, 'menu': None, 'reads': 0}

    def look():
        try:
            seen['dialog'] = find_report_dialog(ctx.ui_nodes(), ctx.facts.package)
            seen['reads'] += 1
        except UiError as e:
            seen['error'] = str(e)
        if seen['dialog'] is not None:
            return True
        seen['menu'] = menu_seen(ctx)
        return bool(seen['menu'])
    ctx.poll(look, ctx.timeouts['no_dialog'])
    ev = []
    if seen['reads'] == 0:
        ev.append('the UI hierarchy could not be read once (%s)' % (seen['error'] or 'no dump was tried')[:100])
    if seen['dialog'] is not None:
        ev.append('a dialog titled %r is on the screen' % seen['dialog'].title.text)
    return Quiet(seen['dialog'], seen['menu'], seen['reads'] > 0, ev)


def flow_f3(ctx):
    """F3: after the dialog was dismissed, the next start shows none (the end it reported was marked seen)."""
    flow = 'F3'
    for check in (need_content(ctx, flow), need_the_report(ctx, flow)):
        if check:
            return check
    if not ctx.dismissed:
        return Result(flow, NOT_RUN, 'F2 did not show a dialog and close it with OK, so there is no dismissed report for a second start to be compared with', [])
    ctx.stop()
    ctx.running = False
    quiet = quiet_start(ctx)
    dialog, menu, ev = quiet.dialog, quiet.menu, quiet.evidence
    if dialog is not None:
        return Result(flow, FAIL, 'the same end was offered a second time: a dialog titled %r came at the second start' % dialog.title.text, ev)
    if not menu:
        return Result(flow, FAIL, 'the second start did not reach the main menu within %ds (a dialog holds the start back)' % ctx.timeouts['no_dialog'], ev)
    ctx.running = True  # the game's new log says the menu is next: it is on its menu whether or not the UI could be read
    if not quiet.ui_read:
        return Result(flow, NOT_RUN, NR_UI_UNREAD % 'the second start', ev + ['menu: ' + menu[:100]])
    return Result(flow, PASS, 'the second start reached the main menu with no dialog', ev + ['menu: ' + menu[:100]])


def leave_by_home(ctx):
    """A user's Home, and then the system ending the process it no longer needs (`am kill` ends only a process that is safe to kill: one in the background)."""
    ctx.runner.key(KEY_HOME)
    ctx.wait(ctx.timeouts['settle'])
    ctx.sh('am kill %s' % ctx.facts.package, timeout=60)


def leave_by_back(ctx):
    """Back on the main menu: the game leaves (game/MagicPortalsLayer.cpp, OWNER RULING R2)."""
    ctx.runner.key(KEY_BACK)
    ctx.wait(ctx.timeouts['settle'])


def leave_by_force_stop(ctx):
    ctx.stop()


USER_LEAVES = (('Home', leave_by_home), ('Back on the main menu', leave_by_back), ('a force-stop', leave_by_force_stop))


def flow_f4(ctx):
    """F4: Home, Back on the main menu and a force-stop are a user's own ends (or the system's tidying of a background process) and show no dialog at the next start."""
    flow = 'F4'
    for check in (need_content(ctx, flow), need_the_report(ctx, flow), need_a_running_game(ctx, flow, 'F1 or F3')):
        if check:
            return check
    ev, problems, unread = [], [], []
    for label, leave in USER_LEAVES:
        if ctx.pid() is None:
            ctx.remove_marker()
            ctx.start()
            if not wait_for_menu(ctx):
                problems.append('%s: the game could not be put on the screen first' % label)
                continue
        leave(ctx)
        if not ctx.poll(lambda: ctx.pid() is None, ctx.timeouts['process_gone']):
            ctx.sh('am force-stop %s' % ctx.facts.package, timeout=60)
            ev.append('%s: the process stayed (the system keeps a background process), so it was force-stopped to make the next start a new one' % label)
            ctx.wait(2.0)
        ctx.running = False
        quiet = quiet_start(ctx)
        ev += ['%s: %s' % (label, m) for m in quiet.evidence]
        if quiet.dialog is not None:
            problems.append('%s: a dialog titled %r came at the next start' % (label, quiet.dialog.title.text))
        elif not quiet.menu:
            problems.append('%s: the next start did not reach the main menu within %ds' % (label, ctx.timeouts['no_dialog']))
        else:
            ctx.running = True
            if quiet.ui_read:
                ev.append('%s: the next start reached the main menu with no dialog' % label)
            else:
                unread.append(label)
    if problems:
        return Result(flow, FAIL, '; '.join(problems), ev)
    if unread:
        return Result(flow, NOT_RUN, NR_UI_UNREAD % ('the next start after ' + ', '.join(unread)), ev)
    return Result(flow, PASS, 'Home, Back and a force-stop each left a start with no dialog', ev)


def set_app_language(ctx, tag):
    """The app's language through `cmd locale set-app-locales` (API 33+); falls back to the system locale setting. Returns (how, problem)."""
    package = ctx.facts.package
    rc, out, err = ctx.sh("cmd locale set-app-locales %s --locales '%s'" % (package, tag), timeout=60)
    if rc == 0 and 'rror' not in out + err and 'nknown' not in out + err:
        rc, got, _ = ctx.sh('cmd locale get-app-locales %s' % package, timeout=30)
        if tag.split('-')[0] in got:
            return 'app locale', None
    rc, out, err = ctx.sh('settings put system system_locales %s' % (tag + '-BR' if tag == 'pt' else tag), timeout=30)
    if rc == 0:
        return 'system locale (unverified: the setting was written; the dialog\'s language tells)', None
    return None, 'neither `cmd locale set-app-locales` nor the system locale setting took (%s)' % (out + err).strip()[:100]


def reset_language(ctx):
    ctx.sh("cmd locale set-app-locales %s --locales ''" % ctx.facts.package, timeout=60)
    ctx.sh('settings delete system system_locales', timeout=30)


def flow_f5(ctx):
    flow = 'F5'
    for check in (need_content(ctx, flow), need_the_report(ctx, flow)):
        if check:
            return check
    if not ctx.facts.title_name:
        return Result(flow, NOT_RUN, 'the manifest gives the game no name to read the title against', [])
    if ctx.crash_method is None:
        return Result(flow, NOT_RUN, 'F2 found no way to end the game that the report offers, so there is no end to read in Portuguese', [])
    if not ctx.running:
        ctx.remove_marker()
        ctx.start()
        if not wait_for_menu(ctx):
            return Result(flow, NOT_RUN, NR_PRIOR % 'F1 or F3', [])
        ctx.running = True
    how, problem = set_app_language(ctx, 'pt')
    if how is None:
        return Result(flow, NOT_RUN, NR_NO_LOCALE % problem, [])
    ev = ['language set through the %s' % how]
    try:
        # the app is ended and started again so that its process takes the language from its first moment
        ctx.stop()
        ctx.running = False
        ctx.remove_marker()
        ctx.start()
        if not wait_for_menu(ctx):
            return Result(flow, NOT_RUN, 'the game did not reach its menu with the language set, so no end could be made', ev)
        ctx.running = True
        pid = ctx.pid()
        before = ctx.exit_records()
        ctx.sh(crash_command(ctx.crash_method, ctx.facts.package, pid), timeout=60)
        ctx.running = False
        if not ctx.poll(lambda: ctx.pid() is None, ctx.timeouts['process_gone']):
            return Result(flow, NOT_RUN, 'the process did not end with %s' % ctx.crash_method.name, ev)
        fresh = ctx.poll(lambda: [r for r in new_records(before, ctx.exit_records()) if offers_report(r)], ctx.timeouts['exit_record'])
        if not fresh:
            return Result(flow, NOT_RUN, 'ending the game with %s left no record the report offers this time' % ctx.crash_method.name, ev)
        problems, more, hard = show_the_dialog(ctx, expected_title(ctx.facts.title_name, True), SHARE_PORTUGUESE, flow)
        ev += more
        if hard:
            return Result(flow, NOT_RUN, hard, ev)
        if problems:
            if how.startswith('system locale') and any('title is' in p for p in problems):
                problems.append('(the language was set through the system setting, which is unverified: an English title may mean it did not take effect)')
            return Result(flow, FAIL, '; '.join(problems), ev)
        return Result(flow, PASS, 'the dialog read %r with the accent and %s' % (expected_title(ctx.facts.title_name, True), SHARE_PORTUGUESE), ev)
    finally:
        reset_language(ctx)


def flow_f6(ctx):
    flow = 'F6'
    check = need_content(ctx, flow)
    if check:
        return check
    if not ctx.running or ctx.pid() is None:
        ctx.remove_marker()
        ctx.start()
        if not wait_for_menu(ctx):
            return Result(flow, NOT_RUN, NR_PRIOR % 'F1', [])
        ctx.running = True
    ev = []
    for x, y in MENU_TAPS:
        ctx.runner.tap(x, y)
        ctx.wait(TAP_GAP_SECONDS)
        ev.append('tap (%d, %d)' % (x, y))

    def up():
        for line in ctx.game_log().split('\n'):
            if LOG_LEVEL_UP.search(line):
                return line.strip()
            refused = LOG_LEVEL_REFUSED.search(line)
            if refused:
                return 'REFUSED: ' + refused.group(1)
        return None
    line = ctx.poll(up, ctx.timeouts['level'])
    ctx.shot('f6_level')
    if line and not line.startswith('REFUSED'):
        ev.append(line[:200])
        return Result(flow, PASS, 'the taps started a level and the log says the level is up', ev)
    reason = ('the game refused the level: %s' % line[len('REFUSED: '):]) if line else 'no "the level is up" line within %ds of the taps' % ctx.timeouts['level']
    return Result(flow, FAIL, reason + ' (the taps are the full game\'s menu at 1280x720, step 205: another content may lay the menu out otherwise)', ev)


FLOWS = (('F1', flow_f1), ('F2', flow_f2), ('F3', flow_f3), ('F4', flow_f4), ('F5', flow_f5), ('F6', flow_f6))


def device_facts(ctx):
    """API level and whether the device answers. Returns a NOT RUN reason or None."""
    rc, out, err = ctx.sh('getprop ro.build.version.sdk', timeout=30)
    if rc != 0 or not out.strip().isdigit():
        return '%s (getprop answered %r)' % (NR_NO_DEVICE, (out + err).strip()[:80])
    ctx.api = int(out.strip())
    return None


def run_flows(ctx, selected=None):
    """Every selected flow, in order, each turned into a Result; nothing raises. A flow's AdbError is a NOT RUN, any other exception a FAIL that says what it was."""
    results = []
    try:
        reason = device_facts(ctx)
    except AdbError as e:
        reason = '%s: %s' % (NR_NO_DEVICE, e)
    for name, flow in FLOWS:
        if selected and name not in selected:
            continue
        if reason:
            results.append(Result(name, NOT_RUN, reason, []))
            continue
        try:
            result = flow(ctx)
        except AdbError as e:
            result = Result(name, NOT_RUN, 'the runner failed: %s' % e, [])
        except UiError as e:
            result = Result(name, NOT_RUN, NR_NO_UI % e, [])
        except CannotJudge as e:
            result = Result(name, NOT_RUN, str(e), [])
        except Exception as e:  # a flow must not take the others down; a bug of this file is a FAIL that names itself
            result = Result(name, FAIL, 'internal error in the flow: %s: %s' % (type(e).__name__, e), [])
        ctx.say('%s %s: %s' % (name, result.status, result.reason))
        results.append(result)
        if name == 'F1' and result.status != PASS:
            ctx.running = False
    return results


def exit_code(results):
    if any(r.status == FAIL for r in results):
        return EXIT_FAIL
    if any(r.status == NOT_RUN for r in results) or not results:
        return EXIT_NOT_RUN
    return EXIT_PASS


def render(results):
    lines = ['%-4s %-8s %s' % ('flow', 'result', 'reason')]
    for r in results:
        lines.append('%-4s %-8s %s' % (r.flow, r.status, r.reason))
        for e in r.evidence:
            lines.append('       %s' % e)
    counts = {s: sum(1 for r in results if r.status == s) for s in (PASS, FAIL, NOT_RUN)}
    lines.append('%d PASS, %d FAIL, %d NOT RUN' % (counts[PASS], counts[FAIL], counts[NOT_RUN]))
    return '\n'.join(lines)


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__.split('\n')[0], formatter_class=argparse.RawDescriptionHelpFormatter, epilog=__doc__)
    p.add_argument('--apk', required=True)
    p.add_argument('--runner', choices=('raw', 'guarded'), required=True)
    p.add_argument('--serial', help='raw: the device (always passed as adb -s)')
    p.add_argument('--adb', default='adb', help='raw: the adb program')
    p.add_argument('--slot', default='impl', help='guarded: the slot of tools/android_emulator.py')
    p.add_argument('--manifest', default=os.path.join(REPO, 'game', 'android', 'AndroidManifest.xml'))
    p.add_argument('--package')
    p.add_argument('--activity')
    p.add_argument('--flows', help='comma-separated, e.g. F1,F2 (default: all)')
    p.add_argument('--content', choices=('full', 'fixture', 'none'), default='full', help='what the APK carries (none: F2 to F6 are NOT RUN)')
    p.add_argument('--out', help='a folder for the text, the UI dumps and the JSON')
    p.add_argument('--json', help='write the results here')
    p.add_argument('--shots', help='a folder for screenshots (they show the game\'s own imagery: keep them out of the repository)')
    args = p.parse_args(argv)
    try:
        with open(args.manifest, encoding='utf-8') as f:
            facts = parse_manifest(f.read())
        if args.package or args.activity:
            facts = facts._replace(package=args.package or facts.package, activity=args.activity or facts.activity)
        runner = RawRunner(args.serial, args.adb) if args.runner == 'raw' else GuardedRunner(args.slot)
    except (OSError, ValueError, AdbError) as e:
        print('android_smoke: %s' % e, file=sys.stderr)
        return EXIT_NOT_RUN
    ctx = Context(runner, facts, apk=args.apk, content=args.content, out_dir=args.out, shots_dir=args.shots, say=lambda t: print(t, flush=True))
    selected = set(x.strip() for x in args.flows.split(',')) if args.flows else None
    results = run_flows(ctx, selected)
    text = render(results)
    print(text)
    if args.out:
        ctx.keep('smoke_result.txt', text + '\n')
    if args.json:
        with open(args.json, 'w', encoding='utf-8', newline='\n') as f:
            json.dump([r._asdict() for r in results], f, indent=2)
    return exit_code(results)


if __name__ == '__main__':
    sys.exit(main())
