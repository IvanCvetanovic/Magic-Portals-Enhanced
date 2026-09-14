"""Which screen is the original showing? Answered by comparing reference patches.

Each marker is a small greyscale crop of a static part of one screen (the main menu's info
button, the "Select chapter" plaque, the level grid's back arrow, the in-level pause button,
the first grid tile's number, a tutorial popup's arrow). A crop matches when its zero-mean
normalised correlation with the same box of the live frame is at least MATCH_MIN.

Correlation, not a pixel difference: the HUD buttons are translucent, so on a torch-lit level
(1-2) the pause button's mean absolute difference rose to 28.8 and read as "not a level" while
the correlation stayed 0.79. Measured 2026-09-14 over 15 known frames: true matches 0.79-1.00,
mismatches <= 0.26, except the pause screen's facebook button against the menu's info button at
0.51. MATCH_MIN sits above that.

The reference crops are cut from captures of the original, so they are Asantee imagery and live
in out/parity/markers/ (gitignored). Rebuild them with `markers.py --build` from the captures
named in REFERENCES if that directory is missing.
"""

from __future__ import annotations

import argparse
import io
import time
from pathlib import Path

import numpy as np
from PIL import Image

import emu

MARKER_DIR = Path(__file__).resolve().parents[2] / "out" / "parity" / "markers"
MATCH_MIN = 0.6

BOXES: dict[str, tuple[int, int, int, int]] = {
    "main_info": (0, 0, 90, 90),
    "chapter_title": (460, 15, 820, 105),
    "grid_back_arrow": (47, 315, 137, 405),
    "grid_tile_p1": (310, 30, 400, 130),
    "grid_tile_p2": (310, 30, 400, 130),
    "level_pause_btn": (1190, 0, 1280, 90),
    "level_restart_btn": (1100, 0, 1190, 90),
    "popup_next": (1132, 315, 1222, 405),
}

# marker -> (capture it was cut from, relative to the repo root). Used by --build only.
REFERENCES: dict[str, str] = {
    "main_info": "out/parity/smoke/06_menu_settled.png",
    "chapter_title": "out/parity/smoke/17_chapters_gold_p1.png",
    "grid_back_arrow": "out/parity/smoke/rec11/enter_1-1_frames/0000_000000ms.png",
    "grid_tile_p1": "out/parity/smoke/rec11/enter_1-1_frames/0000_000000ms.png",
    "grid_tile_p2": "out/parity/original/levels_v1/2-22_t8.0.png",
    "level_pause_btn": "out/parity/original/levels_v1/1-01_t8.0.png",
    "level_restart_btn": "out/parity/original/levels_v1/1-01_t8.0.png",
    "popup_next": "out/parity/original/levels_v1/1-02_t8.0.png",
}

# A screen can have several markers; any one matching is enough. The level needs two because
# an effect can sit on one button: 3-25's corner fire washes out the pause button entirely.
SCREEN_MARKER: dict[str, tuple[str, ...]] = {
    "level": ("level_pause_btn", "level_restart_btn"),
    "popup": ("popup_next",),
    "grid": ("grid_back_arrow",),
    "chapters": ("chapter_title",),
    "main": ("main_info",),
}

_cache: dict[str, np.ndarray] = {}


def _ref(name: str) -> np.ndarray:
    if name not in _cache:
        _cache[name] = np.asarray(Image.open(MARKER_DIR / f"{name}.png"), dtype=np.float32)
    return _cache[name]


def grab() -> Image.Image:
    return Image.open(io.BytesIO(emu._adb("exec-out", "screencap", "-p").stdout)).convert("L")


def similarity(frame: Image.Image, name: str) -> float:
    crop = np.asarray(frame.crop(BOXES[name]), dtype=np.float32)
    ref = _ref(name)
    a, b = crop - crop.mean(), ref - ref.mean()
    # A flat patch (a black screen) has no structure to correlate; it matches nothing.
    return float((a * b).sum() / (np.sqrt((a * a).sum() * (b * b).sum()) + 1e-6))


def current_screen(frame: Image.Image | None = None) -> str:
    frame = frame or grab()
    for screen, names in SCREEN_MARKER.items():
        if any(similarity(frame, name) >= MATCH_MIN for name in names):
            return screen
    return "unknown"


def wait_for(screen: str, timeout_s: float, poll_s: float = 0.4) -> bool:
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        if current_screen() == screen:
            return True
        time.sleep(poll_s)
    return False


def grid_page(frame: Image.Image | None = None) -> int | None:
    frame = frame or grab()
    s1, s2 = similarity(frame, "grid_tile_p1"), similarity(frame, "grid_tile_p2")
    if max(s1, s2) < MATCH_MIN:
        return None
    return 1 if s1 > s2 else 2


def build() -> None:
    root = Path(__file__).resolve().parents[2]
    MARKER_DIR.mkdir(parents=True, exist_ok=True)
    for name, source in REFERENCES.items():
        Image.open(root / source).convert("L").crop(BOXES[name]).save(MARKER_DIR / f"{name}.png")
        print(f"built {name} from {source}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", action="store_true")
    parser.add_argument("--which", action="store_true", help="print the screen showing now")
    args = parser.parse_args()
    if args.build:
        build()
    if args.which:
        print(current_screen())
