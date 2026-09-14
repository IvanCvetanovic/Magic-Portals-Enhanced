"""Drive the ORIGINAL Magic Portals running in the Android emulator, over adb.

Why this exists: the remake is tuned by comparing its frames against the original's, and
the original only runs inside an API 30 x86 emulator (its one native library is 32-bit
ARM; only that image translates it). See docs/parity.md for the full setup.

Everything captured through here is Asantee-origin imagery. Callers must write it under
out/ (gitignored) — never anywhere git can see.

Coordinates are screen pixels at 1280x720 landscape, the resolution the parity AVD uses.
"""

from __future__ import annotations

import os
import subprocess
import time
from pathlib import Path

SERIAL = os.environ.get("PARITY_SERIAL", "emulator-5554")
PACKAGE = "net.asantee.magicportals"
ACTIVITY = f"{PACKAGE}/.PortalActivity"

# The engine polls touch state once per frame, and under software GL + ARM translation a
# frame can take longer than adb's zero-length `input tap`. A held press is always seen.
TAP_HOLD_MS = 250


def _adb(*args: str, capture: bool = True) -> subprocess.CompletedProcess[bytes]:
    return subprocess.run(["adb", "-s", SERIAL, *args], check=True, capture_output=capture)


def shell(command: str) -> str:
    return _adb("shell", command).stdout.decode("utf-8", "replace")


def tap(x: int, y: int, hold_ms: int = TAP_HOLD_MS) -> None:
    shell(f"input swipe {x} {y} {x} {y} {hold_ms}")


def hold(x: int, y: int, ms: int) -> None:
    """Press and keep pressed for `ms` — how the movement pad is driven."""
    shell(f"input swipe {x} {y} {x} {y} {ms}")


def wait(seconds: float) -> None:
    time.sleep(seconds)


def capture(path: Path) -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    data = _adb("exec-out", "screencap", "-p").stdout
    path.write_bytes(data)
    return path


class Recording:
    """`with Recording(path, seconds):` — screenrecord on the device, pulled on exit.

    Still captures cost ~0.3 s each, which is too slow for camera pans, transitions and
    motion. The device-side encoder gives real frame timing instead.
    """

    DEVICE_PATH = "/sdcard/parity_rec.mp4"

    def __init__(self, path: Path, seconds: int) -> None:
        self.path = path
        self.seconds = seconds
        self._proc: subprocess.Popen[bytes] | None = None

    def __enter__(self) -> Recording:
        shell(f"rm -f {self.DEVICE_PATH}")
        self._proc = subprocess.Popen(
            ["adb", "-s", SERIAL, "shell",
             f"screenrecord --bit-rate 12000000 --time-limit {self.seconds} {self.DEVICE_PATH}"],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )
        wait(0.5)  # the encoder needs a moment before the first frame is real
        return self

    def __exit__(self, *_exc: object) -> None:
        assert self._proc is not None
        self._proc.wait(timeout=self.seconds + 30)
        wait(1.0)  # let the muxer finalise the file before pulling it
        self.path.parent.mkdir(parents=True, exist_ok=True)
        _adb("pull", self.DEVICE_PATH, str(self.path))


def is_running() -> bool:
    return bool(shell(f"pidof {PACKAGE}").strip())


def launch(settle_s: float = 12.0) -> None:
    shell(f"am force-stop {PACKAGE}")
    shell(f"am start -W -n {ACTIVITY}")
    wait(settle_s)
    if not is_running():
        raise RuntimeError("original did not stay running — check `adb logcat` for a crash")


class Screen:
    """Tap targets, measured from captures of the original at 1280x720."""

    MAIN_TAP_START = (640, 475)

    CHAPTER_TILES = ((520, 350), (757, 350))  # two chapters per page
    CHAPTER_PREV = (147, 360)
    CHAPTER_NEXT = (1133, 360)

    LEVEL_COLS = (370, 550, 730, 910)  # 4x4 grid, 16 levels per page
    LEVEL_ROWS = (90, 270, 450, 630)
    LEVEL_PREV = (92, 360)
    LEVEL_NEXT = (1188, 360)

    GAME_RESTART = (1143, 45)
    GAME_PAUSE = (1232, 45)
    PAD_LEFT = (60, 680)
    PAD_RIGHT = (1230, 680)

    PAUSE_LEVEL_LIST = (535, 313)
    PAUSE_RESUME = (742, 313)
