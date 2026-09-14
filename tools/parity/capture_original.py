"""Capture the ORIGINAL's level-start frames into the reference library.

    tools/parity/.venv/Scripts/python tools/parity/capture_original.py [--levels all|1-1,2-5] [--out DIR]

For each level: relaunch the app, walk the menus to the level, and screenshot at several
times after the tap. Relaunching per level is slow (~30 s) but it is the only navigation that
cannot be derailed: some levels open a tutorial popup that swallows taps and the back key,
and one bad state would otherwise cascade through every later capture.

Requires the all-gold save (docs/parity.md §3) so every level is unlocked and the level grid
opens on page 2. Output is Asantee-origin imagery: DIR must be under out/.
Resumable — a level whose final frame already exists is skipped.
"""

from __future__ import annotations

import argparse
import time
from pathlib import Path

import emu

LEVELS_PER_WORLD = 32
LEVELS_PER_PAGE = 16
CHAPTERS_PER_PAGE = 2

# Seconds after tapping the level. ~2 s shows the "Part N" caption and the score plaque;
# by ~8 s the fade-in, caption and plaque are gone and the player has materialised.
CAPTURE_TIMES_S = (2.0, 4.5, 8.0)


def parse_levels(spec: str) -> list[tuple[int, int]]:
    if spec == "all":
        return [(w, l) for w in range(4) for l in range(LEVELS_PER_WORLD)]
    out: list[tuple[int, int]] = []
    for item in spec.split(","):
        w, l = item.strip().split("-")
        out.append((int(w) - 1, int(l) - 1))
    return out


def open_level(world: int, level: int) -> float:
    """Walk from a fresh launch to the level; returns the time the level was tapped."""
    s = emu.Screen
    emu.launch(settle_s=10.0)
    emu.tap(*s.MAIN_TAP_START)
    emu.wait(3.5)
    if world // CHAPTERS_PER_PAGE == 1:
        emu.tap(*s.CHAPTER_NEXT)
        emu.wait(2.5)
    emu.tap(*s.CHAPTER_TILES[world % CHAPTERS_PER_PAGE])
    emu.wait(3.5)
    # With every level unlocked the grid opens on its last page.
    if level < LEVELS_PER_PAGE:
        emu.tap(*s.LEVEL_PREV)
        emu.wait(2.5)
    index = level % LEVELS_PER_PAGE
    x, y = s.LEVEL_COLS[index % 4], s.LEVEL_ROWS[index // 4]
    emu.tap(x, y)
    return time.monotonic()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--levels", default="all")
    parser.add_argument("--out", type=Path, default=Path("out/parity/original/levels"))
    args = parser.parse_args()

    for world, level in parse_levels(args.levels):
        tag = f"{world + 1}-{level + 1:02d}"
        final = args.out / f"{tag}_t{CAPTURE_TIMES_S[-1]:.1f}.png"
        if final.exists():
            continue
        tapped = open_level(world, level)
        for t in CAPTURE_TIMES_S:
            delay = tapped + t - time.monotonic()
            if delay > 0:
                emu.wait(delay)
            emu.capture(args.out / f"{tag}_t{t:.1f}.png")
        print(f"captured {tag}", flush=True)


if __name__ == "__main__":
    main()
