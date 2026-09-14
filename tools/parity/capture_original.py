"""Capture the ORIGINAL's level-start frames into the reference library.

    tools/parity/.venv/Scripts/python tools/parity/capture_original.py [--levels all|1-1,2-5] [--out DIR] [--redo]

For each level: relaunch the app, walk the menus to the level, and screenshot at several
times after the level appears. Output is Asantee-origin imagery: DIR must be under out/.

EVERY STEP IS VERIFIED, NOT TIMED. The first version slept fixed intervals between taps and
lost most of chapters 2-4: when the machine was busy the emulator fell behind, taps landed on
the splash or the wrong menu, and the "level" frames were menus. Now each tap waits until a
reference patch proves the expected screen is up (markers.py), and a level whose settled frame
is not a level is retried.

Tutorial popups (1-2, 1-3, ...) cover the level and hide the HUD. The popup is captured as
`<tag>_popup.png`, dismissed with its arrow button, and the level frames are timed from the
moment the HUD appears.

Requires the all-gold save (docs/parity.md §3). Resumable: a level with a `.json` sidecar is
skipped unless --redo.
"""

from __future__ import annotations

import argparse
import json
import time
from pathlib import Path

import emu
import markers

LEVELS_PER_WORLD = 32
LEVELS_PER_PAGE = 16
CHAPTERS_PER_PAGE = 2
ATTEMPTS = 3

# Seconds after the level's HUD first appears (the fade-in). ~0.3 s shows the "Part N"
# caption and score plaque; by ~6 s the caption and plaque are gone and the player is in.
CAPTURE_TIMES_S = (0.3, 2.8, 6.3)
POPUP_NEXT = (1177, 360)


def parse_levels(spec: str) -> list[tuple[int, int]]:
    if spec == "all":
        return [(w, l) for w in range(4) for l in range(LEVELS_PER_WORLD)]
    out: list[tuple[int, int]] = []
    for item in spec.split(","):
        w, l = item.strip().split("-")
        out.append((int(w) - 1, int(l) - 1))
    return out


class NavigationError(RuntimeError):
    pass


def expect(name: str, timeout_s: float) -> None:
    if not markers.wait_for(name, timeout_s):
        raise NavigationError(f"expected '{name}', screen is '{markers.current_screen()}'")


def open_level(world: int, level: int) -> None:
    s = emu.Screen
    emu.launch(settle_s=3.0)
    expect("main", 60)
    emu.tap(*s.MAIN_TAP_START)
    expect("chapters", 20)
    if world // CHAPTERS_PER_PAGE == 1:
        emu.tap(*s.CHAPTER_NEXT)
        emu.wait(2.5)
        expect("chapters", 10)
    emu.tap(*s.CHAPTER_TILES[world % CHAPTERS_PER_PAGE])
    expect("grid", 20)
    want_page = 1 if level < LEVELS_PER_PAGE else 2
    for _ in range(3):
        page = markers.grid_page()
        if page == want_page:
            break
        emu.tap(*(s.LEVEL_PREV if want_page < (page or 2) else s.LEVEL_NEXT))
        emu.wait(2.5)
    if markers.grid_page() != want_page:
        raise NavigationError(f"could not reach grid page {want_page}")
    index = level % LEVELS_PER_PAGE
    emu.tap(s.LEVEL_COLS[index % 4], s.LEVEL_ROWS[index // 4])


def capture_level(world: int, level: int, out: Path) -> dict:
    tag = f"{world + 1}-{level + 1:02d}"
    open_level(world, level)
    tapped = time.monotonic()
    info: dict = {"level": tag, "popup_pages": 0}

    # Wait for either the HUD (the level is up) or a popup covering it.
    deadline = tapped + 25
    while time.monotonic() < deadline:
        screen = markers.current_screen()
        if screen == "level":
            break
        if screen == "popup":
            if info["popup_pages"] == 0:
                emu.capture(out / f"{tag}_popup.png")
            info["popup_pages"] += 1
            emu.tap(*POPUP_NEXT)
            emu.wait(1.5)
            if info["popup_pages"] > 8:
                raise NavigationError("popup would not dismiss")
            continue
        emu.wait(0.25)
    else:
        raise NavigationError(f"level never appeared; screen is '{markers.current_screen()}'")

    appeared = time.monotonic()
    info["hud_after_tap_s"] = round(appeared - tapped, 2)
    for t in CAPTURE_TIMES_S:
        delay = appeared + t - time.monotonic()
        if delay > 0:
            emu.wait(delay)
        emu.capture(out / f"{tag}_h{t:.1f}.png")
    if markers.current_screen() != "level":
        raise NavigationError("settled frame is not a level")
    return info


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--levels", default="all")
    parser.add_argument("--out", type=Path, default=Path("out/parity/original/levels"))
    parser.add_argument("--redo", action="store_true")
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)

    failed: list[str] = []
    for world, level in parse_levels(args.levels):
        tag = f"{world + 1}-{level + 1:02d}"
        sidecar = args.out / f"{tag}.json"
        if sidecar.exists() and not args.redo:
            continue
        for attempt in range(1, ATTEMPTS + 1):
            try:
                info = capture_level(world, level, args.out)
                info["attempts"] = attempt
                sidecar.write_text(json.dumps(info, indent=1))
                print(f"captured {tag} {info}", flush=True)
                break
            except NavigationError as err:
                print(f"retry {tag} attempt {attempt}: {err}", flush=True)
        else:
            failed.append(tag)
    print(f"done; failed: {failed}", flush=True)


if __name__ == "__main__":
    main()
