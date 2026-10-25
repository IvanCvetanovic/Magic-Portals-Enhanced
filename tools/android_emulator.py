#!/usr/bin/env python3
"""The ONE way this project drives an Android emulator (planning step 204, PORT-6). Python 3, standard library only; Windows is what it was run on.

WHY IT EXISTS. The owner's machine holds more than this game's emulator: the parity rig (the AVD MP_Original_API30, console port 5554), the sibling port's
AVDs (Penumbra_API30_x86_64, Penumbra_API33_x86_64, serial emulator-5560) and Medium_Phone_API_37.1. They share ONE adb server, and a bare `adb`, an
`adb kill-server`, a `pkill adb`, a start of the wrong AVD or an install to "the only device" costs days of the owner's work. So nothing of this project
talks to adb or the emulator except through this file, and this file can only ever name the two slots below:

    impl    AVD MP_Android_API33_x86_64     console port 5564   serial emulator-5564     (the implementer's)
    verify  AVD MP_Android_API33_x86_64_v   console port 5566   serial emulator-5566     (an independent verifier's)

Everything else is refused: any other serial, any `-s` of the caller's, and the adb verbs that act on the server or on every device (kill-server,
start-server, reconnect, devices, connect, disconnect, usb, tcpip, root, unroot, mdns, pair, forward, reverse). EVERY adb call is written as
`adb -s emulator-556x ...`, and every call, every emulator launch and every avdmanager call is appended to one log (--log, else $MP_EMU_LOG, else
<repo>/out/android_emulator/calls.log) BEFORE it runs, so the log is the evidence that no other serial was addressed (grep the log for a line that begins
`adb` and does not continue `-s emulator-556`). The adb server itself is started by adb on its first command, as adb does; this file never starts or
stops it.

It is also a module: `import android_emulator as emu; emu.use_slot('impl'); emu.adb('shell', 'getprop sys.boot_completed')`, so a driver script of
a step shares the guard and the log.

WHAT AN EMULATOR PROVES, AND WHAT IT DOES NOT. Startup, assets, input, lifecycle and the Android code paths. It NEVER proves phone performance, GPU behaviour,
thermals, memory pressure, audio output, real multi-touch (`adb shell input` injects one pointer), notches, edge back-gestures or arm64 execution. The
numbers a run takes here are "SwiftShader x86_64 emulator on this PC" and nothing more.

Commands (`tools/android_emulator.py [--slot impl|verify] [--log FILE] <command> ...`; Git Bash rewrites an argument that begins with / unless
MSYS_NO_PATHCONV=1 is set, so set it when a device path is an argument):
    create                      the AVD of the slot with avdmanager WITHOUT --force (stops if the name exists), configured like the sibling's API 33 AVD
    start [--timeout S]         refuses below 6 GB of free memory ("NOT RUN" with the numbers), starts the AVD headless and waits for sys.boot_completed
    stop                        `adb -s <serial> emu kill`, then checks that the process is gone
    status                      the state of the slot, the free memory and the emulator-related processes (all of them, untouched)
    install APK [--fresh]       adb install -r (--fresh: uninstall first)           uninstall
    launch                      am start of the activity                            force-stop
    args "FLAGS" | args-clear   leave the developer's flags file for the next launch (needs a debuggable APK)
    shell CMD                   adb shell CMD                                       run-as CMD    (the app's own user, debuggable APK)
    pull REMOTE LOCAL           the file at REMOTE under the app's folder, through run-as, written to LOCAL
    tree-hash                   size and sha256 of every file under the app's files folder, in one pass
    screenshot PNG | tap X Y [MS] | swipe X1 Y1 X2 Y2 [MS] | key NAME
    logcat OUT [--clear]        the buffer, dumped to OUT (or --clear it)
    adb VERB ARGS...            any other allowed adb verb, always with -s <serial>
"""
import argparse
import ctypes
import datetime
import hashlib
import os
import re
import subprocess
import sys
import time

PACKAGE = 'com.ivancvetanovic.magicportals'
ACTIVITY = 'com.ivancvetanovic.supersonic.SupersonicActivity'
SYSTEM_IMAGE = 'system-images;android-33;google_apis;x86_64'
MIN_FREE_MEMORY_BYTES = 6 * 1024 ** 3

# How the emulator is started (Windows creation flags). NOT DETACHED_PROCESS: a process with no console that starts a console program (emulator.exe starts
# qemu-system-x86_64-headless.exe) gives that program a console of its own, which the default terminal (Windows Terminal) shows as a tab titled with the
# path of qemu, and an OpenConsole.exe -Embedding process starts with it (step 204: the owner saw the tab). CREATE_NO_WINDOW gives the emulator a console
# with no window, which qemu inherits, so nothing is shown; CREATE_NEW_PROCESS_GROUP keeps a Ctrl+C aimed at the script from reaching the emulator. Neither
# ties the emulator to the script: it goes on running after the script has exited (the slot is ended by `stop`, never by the script's end).
CREATE_NO_WINDOW = 0x08000000
CREATE_NEW_PROCESS_GROUP = 0x00000200
DETACHED_PROCESS = 0x00000008  # named only so that the test can say it is NOT used
EMULATOR_CREATIONFLAGS = CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP

# The two slots, and the ONLY serials this file will ever name. Anything else is a bug of the caller, so it stops.
SLOTS = {
    'impl': {'avd': 'MP_Android_API33_x86_64', 'port': 5564},
    'verify': {'avd': 'MP_Android_API33_x86_64_v', 'port': 5566},
}
# Not this project's: named here only to be refused loudly, and to be looked at (never touched) by `status`.
OTHERS_UNTOUCHED = ('MP_Original_API30', 'Penumbra_API30_x86_64', 'Penumbra_API33_x86_64', 'Medium_Phone_API_37.1')

# The adb verbs a caller may use. `shell`, `exec-out` and `exec-in` run a command ON the one device the serial names. Everything that acts on the adb
# server, on every device, or on the device's adbd is absent on purpose.
ALLOWED_VERBS = {'shell', 'install', 'uninstall', 'push', 'pull', 'logcat', 'exec-out', 'exec-in', 'get-state', 'get-serialno', 'wait-for-device', 'emu', 'bugreport'}
# `adb emu` talks to this emulator's console: only these console commands.
ALLOWED_EMU = {'kill', 'rotate', 'screenrecord', 'avd', 'gsm', 'network', 'power', 'sms', 'geo', 'event', 'redir'}
DENIED_WORDS = {'kill-server', 'start-server', 'reconnect', 'devices', 'connect', 'disconnect', 'usb', 'tcpip', 'root', 'unroot', 'mdns', 'pair', 'forward', 'reverse', 'disable-verity', 'remount'}

_slot = {'name': 'impl'}
_log_path = {'path': None}


class EmuError(Exception):
    pass


# ---- the guard -------------------------------------------------------------------------------------------------------------------------

def repo_root():
    return os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def use_slot(name):
    if name not in SLOTS:
        raise EmuError('unknown slot %r: this file knows %s' % (name, ', '.join(sorted(SLOTS))))
    _slot['name'] = name


def slot():
    return SLOTS[_slot['name']]


def slot_name():
    return _slot['name']


def serial():
    return 'emulator-%d' % slot()['port']


def set_log(path):
    _log_path['path'] = path


def log_path():
    if _log_path['path']:
        return _log_path['path']
    return os.environ.get('MP_EMU_LOG') or os.path.join(repo_root(), 'out', 'android_emulator', 'calls.log')


def note(line):
    path = os.path.abspath(log_path())
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'a', encoding='utf-8', newline='\n') as f:
        f.write('%s %s\n' % (datetime.datetime.now().isoformat(timespec='milliseconds'), line))


def sdk_root():
    for key in ('ANDROID_HOME', 'ANDROID_SDK_ROOT', 'ANDROID_SDK'):
        if os.environ.get(key) and os.path.isdir(os.environ[key]):
            return os.environ[key]
    guess = os.path.join(os.environ.get('LOCALAPPDATA', ''), 'Android', 'Sdk')
    if os.path.isdir(guess):
        return guess
    raise EmuError('no Android SDK found (set ANDROID_HOME)')


def tool(*parts):
    path = os.path.join(sdk_root(), *parts)
    for ext in ('.exe', '.bat', ''):
        if os.path.isfile(path + ext):
            return path + ext
    raise EmuError('%s not found in the SDK' % os.path.join(*parts))


def avd_home():
    return os.environ.get('ANDROID_AVD_HOME') or os.path.join(os.path.expanduser('~'), '.android', 'avd')


def _check_args(args):
    """The arguments of an adb call, held to the guard. Raises EmuError."""
    if not args:
        raise EmuError('no adb verb')
    verb = args[0]
    if verb == '-s' or any(a == '-s' for a in args[:2]):
        raise EmuError('the caller may not name a serial: the slot does (%s)' % serial())
    if verb.startswith('-'):
        raise EmuError('adb options before the verb are refused: %r' % verb)
    if verb in DENIED_WORDS or verb not in ALLOWED_VERBS:
        raise EmuError('the adb verb %r is not allowed here' % verb)
    if verb == 'emu' and (len(args) < 2 or args[1] not in ALLOWED_EMU):
        raise EmuError('this emulator-console command is not allowed: %r' % (args[1:2],))
    for a in args[1:]:
        if a in ('kill-server', 'start-server'):
            raise EmuError('%r is refused' % a)


def adb_argv(*args):
    _check_args(list(args))
    return [tool('platform-tools', 'adb'), '-s', serial()] + [str(a) for a in args]


def adb(*args, timeout=120, check=False, binary=False, stdin=None, log=True):
    """Run `adb -s <this slot's serial> args...`. Returns (returncode, stdout, stderr); text unless binary."""
    argv = adb_argv(*args)
    # EVERY call is logged, before it runs; a poll (log=False) only gets a marker, so a grep of the log for an `adb` line that does not continue
    # `-s emulator-556x` sees all of them.
    note('adb -s %s %s%s' % (serial(), ' '.join(str(a) for a in args), '' if log else '   [poll]'))
    env = dict(os.environ, MSYS_NO_PATHCONV='1')
    try:
        p = subprocess.run(argv, input=stdin, capture_output=True, timeout=timeout, env=env)
    except subprocess.TimeoutExpired:
        note('  -> timeout after %ss' % timeout)
        if check:
            raise EmuError('adb %s timed out after %ss' % (args[0], timeout))
        return 124, b'' if binary else '', b'' if binary else 'timeout'
    out, err = p.stdout, p.stderr
    if not binary:
        out = out.decode('utf-8', 'replace').replace('\r\n', '\n')
        err = err.decode('utf-8', 'replace').replace('\r\n', '\n')
    if check and p.returncode != 0:
        raise EmuError('adb %s failed (%d): %s' % (' '.join(map(str, args)), p.returncode, err.strip() if not binary else err[:200]))
    return p.returncode, out, err


def shell(command, **kw):
    return adb('shell', command, **kw)


def run_as(command, **kw):
    """`command` as the app's own user (a debuggable APK), in the app's data folder."""
    return adb('shell', "run-as %s sh -c '%s'" % (PACKAGE, command.replace("'", "'\\''")), **kw)


def is_up():
    rc, out, _ = adb('get-state', timeout=15, log=False)
    return rc == 0 and out.strip() == 'device'


def boot_completed():
    rc, out, _ = adb('shell', 'getprop sys.boot_completed', timeout=20, log=False)
    return rc == 0 and out.strip() == '1'


# ---- machine facts ---------------------------------------------------------------------------------------------------------------------

def free_memory_bytes():
    class MemoryStatus(ctypes.Structure):
        _fields_ = [('dwLength', ctypes.c_ulong), ('dwMemoryLoad', ctypes.c_ulong), ('ullTotalPhys', ctypes.c_ulonglong), ('ullAvailPhys', ctypes.c_ulonglong),
                    ('ullTotalPageFile', ctypes.c_ulonglong), ('ullAvailPageFile', ctypes.c_ulonglong), ('ullTotalVirtual', ctypes.c_ulonglong),
                    ('ullAvailVirtual', ctypes.c_ulonglong), ('sullAvailExtendedVirtual', ctypes.c_ulonglong)]
    status = MemoryStatus()
    status.dwLength = ctypes.sizeof(MemoryStatus)
    if not ctypes.windll.kernel32.GlobalMemoryStatusEx(ctypes.byref(status)):
        raise EmuError('cannot read the free memory')
    return status.ullAvailPhys, status.ullTotalPhys


def processes():
    """Name, pid and command line of every emulator-, qemu- and adb-related process (read only)."""
    script = ("Get-CimInstance Win32_Process | Where-Object { $_.Name -match 'emulator|qemu|adb|crashpad' } | "
              "ForEach-Object { '{0}|{1}|{2}' -f $_.Name, $_.ProcessId, $_.CommandLine }")
    p = subprocess.run(['powershell', '-NoProfile', '-NonInteractive', '-Command', script], capture_output=True, text=True, timeout=60)
    rows = []
    for line in p.stdout.splitlines():
        parts = line.split('|', 2)
        if len(parts) == 3:
            rows.append((parts[0], int(parts[1]) if parts[1].isdigit() else -1, parts[2]))
    return rows


def our_processes():
    """The emulator processes of THIS slot's AVD (by its -avd argument): never anybody else's."""
    mine = []
    for name, pid, cmd in processes():
        if re.search(r'-avd\s+%s(\s|$)' % re.escape(slot()['avd']), cmd or ''):
            mine.append((name, pid, cmd))
    return mine


# ---- the AVD ---------------------------------------------------------------------------------------------------------------------------

# Like the sibling's Penumbra_API33_x86_64 (read, never written): 720x1280, density 320, 2 GB, 2 cores, SwiftShader, no snapshots. The data partition is
# 2 GB: the data-bearing APK is about 30 MB and the unpacked tree about 25 MB.
AVD_CONFIG = {
    'hw.lcd.width': '720',
    'hw.lcd.height': '1280',
    'hw.lcd.density': '320',
    'hw.ramSize': '2048',
    'hw.cpu.ncore': '2',
    'hw.gpu.enabled': 'yes',
    'hw.gpu.mode': 'swiftshader_indirect',
    'hw.initialOrientation': 'landscape',
    'disk.dataPartition.size': '2048M',
    'fastboot.forceColdBoot': 'yes',
    'fastboot.forceFastBoot': 'no',
    'fastboot.chosenSnapshotFile': '',
    'snapshot.present': 'no',
    'showDeviceFrame': 'no',
    'hw.keyboard': 'yes',
    'hw.audioInput': 'no',
    'hw.camera.back': 'none',
    'hw.camera.front': 'none',
}


def avd_exists():
    return os.path.exists(os.path.join(avd_home(), slot()['avd'] + '.ini'))


def apply_config(path, values):
    with open(path, encoding='utf-8') as f:
        lines = f.read().replace('\r\n', '\n').split('\n')
    seen = set()
    out = []
    for line in lines:
        key = line.split('=', 1)[0].strip() if '=' in line else None
        if key in values:
            out.append('%s=%s' % (key, values[key]))
            seen.add(key)
        else:
            out.append(line)
    while out and out[-1] == '':
        out.pop()
    for key, value in values.items():
        if key not in seen:
            out.append('%s=%s' % (key, value))
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(out) + '\n')


def cmd_create(args):
    name = slot()['avd']
    if avd_exists():
        raise EmuError('the AVD %s already exists: stopped (the AVD is created without --force, and never over an existing one)' % name)
    argv = [tool('cmdline-tools', 'latest', 'bin', 'avdmanager'), 'create', 'avd', '-n', name, '-k', SYSTEM_IMAGE, '-d', 'pixel']
    note('avdmanager create avd -n %s -k %s -d pixel   (no --force)' % (name, SYSTEM_IMAGE))
    p = subprocess.run(argv, input='no\n', capture_output=True, text=True, timeout=300, env=dict(os.environ))
    print(p.stdout.strip())
    if p.returncode != 0:
        raise EmuError('avdmanager failed: %s' % p.stderr.strip())
    config = os.path.join(avd_home(), name + '.avd', 'config.ini')
    apply_config(config, AVD_CONFIG)
    note('config.ini of %s written: %s' % (name, ', '.join('%s=%s' % kv for kv in AVD_CONFIG.items())))
    print('created %s; config.ini set to: %s' % (name, ', '.join('%s=%s' % kv for kv in AVD_CONFIG.items())))
    return 0


def cmd_start(args):
    name = slot()['avd']
    if not avd_exists():
        raise EmuError('the AVD %s does not exist: run `create` first' % name)
    if is_up():
        print('%s is already up' % serial())
        return 0
    if our_processes():
        raise EmuError('a process of AVD %s is already running but %s is not answering: stopped without touching it' % (name, serial()))
    free, total = free_memory_bytes()
    print('free memory %.2f GB of %.2f GB' % (free / 1024 ** 3, total / 1024 ** 3))
    note('free memory before start: %d bytes of %d' % (free, total))
    if free < MIN_FREE_MEMORY_BYTES:
        print('NOT RUN: free memory is under 6 GB (%.2f GB)' % (free / 1024 ** 3))
        return 3
    others = [(n, pid) for n, pid, cmd in processes() if n.lower().startswith(('emulator', 'qemu')) and name not in (cmd or '')]
    if others:
        print('other emulator processes are running (left alone): %s' % others)
        note('other emulator processes at start (untouched): %s' % others)
    argv = [tool('emulator', 'emulator'), '-avd', name, '-port', str(slot()['port']), '-no-window', '-no-audio', '-no-snapshot', '-no-boot-anim',
            '-gpu', 'swiftshader_indirect']
    note('emulator ' + ' '.join(argv[1:]))
    out_dir = os.path.dirname(os.path.abspath(log_path()))
    os.makedirs(out_dir, exist_ok=True)
    emulator_log = open(os.path.join(out_dir, 'emulator_%s.log' % _slot['name']), 'ab')
    proc = subprocess.Popen(argv, stdout=emulator_log, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL, creationflags=EMULATOR_CREATIONFLAGS,
                            close_fds=True)
    emulator_log.close()  # the child holds its own copy of the handle; the parent's would only lock the file
    with open(os.path.join(out_dir, 'emulator_%s.pid' % _slot['name']), 'w') as f:
        f.write(str(proc.pid))
    started = time.time()
    return 0 if wait_boot(float(getattr(args, 'timeout', 300) or 300), started) else 1


def wait_boot(timeout, started=None):
    started = started or time.time()
    deadline = started + timeout
    while time.time() < deadline:
        if is_up() and boot_completed():
            rc, out, _ = adb('shell', 'pm path android', timeout=20, log=False)
            if rc == 0 and 'package:' in out:
                print('%s booted in %.1f s' % (serial(), time.time() - started))
                note('boot completed after %.1f s' % (time.time() - started))
                return True
        time.sleep(3)
    print('%s did not finish booting within %.0f s' % (serial(), timeout))
    note('boot timeout')
    return False


def kill_ours():
    """Ends THIS slot's emulator processes (found by their -avd argument, never anybody else's) when the console no longer answers: a wedged emulator
    answers no adb command, not even `emu kill`."""
    for name, pid, cmd in our_processes():
        note('taskkill of %s pid %d (a process of AVD %s, and only that)' % (name, pid, slot()['avd']))
        subprocess.run(['taskkill', '/PID', str(pid), '/F'], capture_output=True, timeout=60)


def cmd_stop(args):
    if is_up():
        adb('emu', 'kill', timeout=60)
    deadline = time.time() + 60
    while time.time() < deadline and (is_up() or our_processes()):
        time.sleep(2)
    if our_processes():
        print('the emulator did not leave after `emu kill`: ending the processes of AVD %s' % slot()['avd'])
        kill_ours()
        time.sleep(5)
    left = our_processes()
    print('%s: %s; processes of AVD %s left: %s' % (serial(), 'still answering' if is_up() else 'gone', slot()['avd'], left or 'none'))
    note('stop: processes left %s' % (left or 'none'))
    return 0 if not left and not is_up() else 1


def cmd_status(args):
    free, total = free_memory_bytes()
    print('slot %s: AVD %s, serial %s, exists=%s, up=%s' % (_slot['name'], slot()['avd'], serial(), avd_exists(), is_up()))
    print('free memory %.2f GB of %.2f GB' % (free / 1024 ** 3, total / 1024 ** 3))
    for name, pid, cmd in processes():
        print('  %s %s %s' % (name, pid, (cmd or '')[:150]))
    return 0


# ---- the app ---------------------------------------------------------------------------------------------------------------------------

def cmd_install(args):
    if args.fresh:
        adb('uninstall', PACKAGE, timeout=120)
    rc, out, err = adb('install', '-r', args.apk, timeout=300)
    print((out + err).strip())
    return rc


def cmd_launch(args):
    rc, out, err = adb('shell', 'am start -n %s/%s' % (PACKAGE, ACTIVITY), timeout=60)
    print((out + err).strip())
    return rc


def leave_args(flags):
    """The developer's flags for the next launch, through run-as (a debuggable APK only)."""
    return adb('exec-in', 'run-as %s sh -c %s' % (PACKAGE, "'cat > files/magicportals_args.txt'"), stdin=(flags + '\n').encode('utf-8'), timeout=60)


def pull_file(remote, local):
    """files/<remote> of the app, binary-safe, through run-as; returns the byte count."""
    rc, out, err = adb('exec-out', "run-as %s cat '%s'" % (PACKAGE, remote), binary=True, timeout=300)
    if rc != 0:
        raise EmuError('cannot read %s: %s' % (remote, err[:200]))
    os.makedirs(os.path.dirname(os.path.abspath(local)), exist_ok=True)
    with open(local, 'wb') as f:
        f.write(out)
    return len(out)


def tree_hash(folder='files'):
    """{relative path: (size, sha256)} of every file under the app's `folder`, in one pass on the device (toybox sha256sum and stat)."""
    rc, out, err = run_as("cd %s && find . -type f -exec sha256sum {} + && echo ---SIZES--- && find . -type f -exec stat -c '%%s %%n' {} +" % folder, timeout=600)
    if rc != 0 or '---SIZES---' not in out:
        raise EmuError('tree-hash failed: %s' % (err or out)[:300])
    sums_text, sizes_text = out.split('---SIZES---', 1)
    sums = {}
    for line in sums_text.strip().splitlines():
        digest, _, path = line.partition('  ')
        sums[path[2:] if path.startswith('./') else path] = digest
    result = {}
    for line in sizes_text.strip().splitlines():
        size, _, path = line.partition(' ')
        path = path[2:] if path.startswith('./') else path
        if path in sums and size.isdigit():
            result[path] = (int(size), sums[path])
    return result


def screenshot(png):
    rc, out, err = adb('exec-out', 'screencap -p', binary=True, timeout=60)
    if rc != 0 or not out.startswith(b'\x89PNG'):
        raise EmuError('screencap failed (%d): %s' % (rc, err[:200]))
    os.makedirs(os.path.dirname(os.path.abspath(png)), exist_ok=True)
    with open(png, 'wb') as f:
        f.write(out)
    return len(out)


def tap(x, y, ms=250):
    """A hold of `ms`: a zero-length tap is missed under software GL (learned on the parity rig)."""
    return adb('shell', 'input swipe %d %d %d %d %d' % (x, y, x, y, ms), timeout=30)


def swipe(x1, y1, x2, y2, ms=300):
    return adb('shell', 'input swipe %d %d %d %d %d' % (x1, y1, x2, y2, ms), timeout=30)


def key(name):
    if not re.fullmatch(r'KEYCODE_[A-Z0-9_]+|[0-9]+', name):
        raise EmuError('not a key name: %r' % name)
    return adb('shell', 'input keyevent %s' % name, timeout=30)


def logcat_dump(out_path, clear=False):
    if clear:
        return adb('logcat', '-c', timeout=30)
    rc, out, err = adb('logcat', '-d', '-v', 'threadtime', timeout=120, binary=True)
    os.makedirs(os.path.dirname(os.path.abspath(out_path)), exist_ok=True)
    with open(out_path, 'wb') as f:
        f.write(out.replace(b'\r\n', b'\n'))
    return rc, len(out)


# ---- the command line ------------------------------------------------------------------------------------------------------------------

def make_output_safe(*streams):
    """A console's code page may lack a character the device printed (a cp1252 console and a name in another script): the verbs that write the device's
    text to the console (shell, run-as, adb, the failure messages) must not die on it (PORT6-7). The text is written with `?` for what the stream cannot encode.
    This is a tool for a Windows host; on any other host the streams are UTF-8 and nothing changes."""
    for stream in streams or (sys.stdout, sys.stderr):
        try:
            stream.reconfigure(errors='replace')
        except (AttributeError, ValueError, OSError):
            pass


def main(argv=None):
    make_output_safe()
    parser = argparse.ArgumentParser(prog='android_emulator.py', description=__doc__.split('\n')[0], add_help=True)
    parser.add_argument('--slot', default='impl', choices=sorted(SLOTS))
    parser.add_argument('--log', default=None)
    sub = parser.add_subparsers(dest='command', required=True)
    sub.add_parser('create')
    p = sub.add_parser('start')
    p.add_argument('--timeout', type=float, default=300)
    sub.add_parser('stop')
    sub.add_parser('status')
    p = sub.add_parser('install')
    p.add_argument('apk')
    p.add_argument('--fresh', action='store_true')
    sub.add_parser('uninstall')
    sub.add_parser('launch')
    sub.add_parser('force-stop')
    p = sub.add_parser('args')
    p.add_argument('flags')
    sub.add_parser('args-clear')
    p = sub.add_parser('shell')
    p.add_argument('cmd')
    p = sub.add_parser('run-as')
    p.add_argument('cmd')
    p = sub.add_parser('pull')
    p.add_argument('remote')
    p.add_argument('local')
    sub.add_parser('tree-hash')
    p = sub.add_parser('screenshot')
    p.add_argument('png')
    p = sub.add_parser('tap')
    p.add_argument('x', type=int)
    p.add_argument('y', type=int)
    p.add_argument('ms', type=int, nargs='?', default=250)
    p = sub.add_parser('swipe')
    for n in ('x1', 'y1', 'x2', 'y2'):
        p.add_argument(n, type=int)
    p.add_argument('ms', type=int, nargs='?', default=300)
    p = sub.add_parser('key')
    p.add_argument('name')
    p = sub.add_parser('logcat')
    p.add_argument('out')
    p.add_argument('--clear', action='store_true')
    p = sub.add_parser('adb')
    p.add_argument('rest', nargs=argparse.REMAINDER)
    args = parser.parse_args(argv)
    use_slot(args.slot)
    if args.log:
        set_log(args.log)
    try:
        c = args.command
        if c == 'create':
            return cmd_create(args)
        if c == 'start':
            return cmd_start(args)
        if c == 'stop':
            return cmd_stop(args)
        if c == 'status':
            return cmd_status(args)
        if c == 'install':
            return cmd_install(args)
        if c == 'uninstall':
            return adb('uninstall', PACKAGE, timeout=120)[0]
        if c == 'launch':
            return cmd_launch(args)
        if c == 'force-stop':
            return shell('am force-stop %s' % PACKAGE, timeout=60)[0]
        if c == 'args':
            return leave_args(args.flags)[0]
        if c == 'args-clear':
            return run_as('rm -f files/magicportals_args.txt files/magicportals_args.used')[0]
        if c == 'shell':
            rc, out, err = shell(args.cmd)
            sys.stdout.write(out + err)
            return rc
        if c == 'run-as':
            rc, out, err = run_as(args.cmd)
            sys.stdout.write(out + err)
            return rc
        if c == 'pull':
            print('%d bytes' % pull_file(args.remote, args.local))
            return 0
        if c == 'tree-hash':
            for path, (size, digest) in sorted(tree_hash().items()):
                print('%s %d %s' % (digest, size, path))
            return 0
        if c == 'screenshot':
            print('%d bytes' % screenshot(args.png))
            return 0
        if c == 'tap':
            return tap(args.x, args.y, args.ms)[0]
        if c == 'swipe':
            return swipe(args.x1, args.y1, args.x2, args.y2, args.ms)[0]
        if c == 'key':
            return key(args.name)[0]
        if c == 'logcat':
            rc = logcat_dump(args.out, args.clear)
            return rc[0] if isinstance(rc, tuple) else 0
        if c == 'adb':
            rc, out, err = adb(*args.rest)
            sys.stdout.write(out + err)
            return rc
    except EmuError as e:
        print('android_emulator: %s' % e, file=sys.stderr)
        note('REFUSED/FAILED: %s' % e)
        return 2
    return 0


if __name__ == '__main__':
    sys.exit(main())
