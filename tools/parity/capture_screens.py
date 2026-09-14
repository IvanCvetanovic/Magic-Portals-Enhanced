"""Capture the ORIGINAL's menus and non-level screens into the reference library.

    tools/parity/.venv/Scripts/python tools/parity/capture_screens.py [--only name,name] [--out DIR]

Each screen is captured from a fresh launch with a chosen save state, so no capture depends
on where the previous one left the app. Save states are ENML files pushed to the global
external storage the game reads (docs/parity.md §3):

  fresh      no scores.enml at all — first launch, everything locked but 1-1
  bronze     every level of every world = 1 — chapter 1 open, chapter 2 "requires 60%"
  gold       every level of every world = 3 — everything open, 100%

Output is Asantee-origin imagery: DIR must be under out/.
"""

from __future__ import annotations

import argparse
import subprocess
from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable

import emu
import markers

SAVE_DIR = "/sdcard/.ethanon/net.asantee.magicportals/files"
S = emu.Screen

MAIN_INFO = (45, 45)
MAIN_ACHIEVEMENTS = (1100, 677)
MAIN_SOUND = (45, 677)
MAIN_MUSIC = (135, 677)


def scores_enml(medal: int) -> str:
    lines = ["version", "{", "\tnumber = 1_4;", "}", ""]
    for world in range(4):
        lines += [f"world{world}", "{"] + [f"\tlevel{i} = {medal};" for i in range(32)] + ["}", ""]
    return "\n".join(lines) + "\n"


def set_save(state: str, scratch: Path) -> None:
    emu.shell(f"am force-stop {emu.PACKAGE}")
    emu.shell(f"rm -f {SAVE_DIR}/scores.enml")
    if state == "fresh":
        return
    medal = {"bronze": 1, "gold": 3}[state]
    local = scratch / f"scores_{state}.enml"
    local.parent.mkdir(parents=True, exist_ok=True)
    local.write_text(scores_enml(medal), newline="\n")
    subprocess.run(["adb", "-s", emu.SERIAL, "push", str(local), f"{SAVE_DIR}/scores.enml"],
                   check=True, capture_output=True)


@dataclass
class Shot:
    name: str
    save: str
    steps: Callable[[Path], None]
    notes: list[str] = field(default_factory=list)


def _expect(screen: str, timeout_s: float = 25.0) -> None:
    # Verified, not timed: see capture_original.py for how fixed sleeps went wrong.
    if not markers.wait_for(screen, timeout_s):
        raise RuntimeError(f"expected '{screen}', screen is '{markers.current_screen()}'")


def _start() -> None:
    _expect("main", 60)
    emu.tap(*S.MAIN_TAP_START)
    _expect("chapters")


def _chapter(world: int) -> None:
    _start()
    if world >= 2:
        emu.tap(*S.CHAPTER_NEXT)
        emu.wait(2.5)
        _expect("chapters")
    emu.tap(*S.CHAPTER_TILES[world % 2])
    _expect("grid")


def _grid_page(page: int) -> None:
    for _ in range(3):
        if markers.grid_page() == page:
            return
        emu.tap(*(S.LEVEL_PREV if page == 1 else S.LEVEL_NEXT))
        emu.wait(2.5)
    raise RuntimeError(f"could not reach grid page {page}")


def shots() -> list[Shot]:
    out: list[Shot] = []

    def main_menu(d: Path) -> None:
        _expect("main", 60)
        emu.wait(3.0)  # the buttons slide in after the menu is recognisable
        emu.capture(d / "main_menu.png")
        emu.wait(1.3)
        emu.capture(d / "main_menu_b.png")  # TAP START pulses: a second phase

    out.append(Shot("main_menu", "gold", main_menu))

    def main_menu_intro(d: Path) -> None:
        emu.shell(f"am force-stop {emu.PACKAGE}")
        with emu.Recording(d / "main_menu_intro.mp4", 20):
            emu.shell(f"am start -n {emu.ACTIVITY}")
            emu.wait(18)

    out.append(Shot("main_menu_intro", "gold", main_menu_intro))

    def info(d: Path) -> None:
        _expect("main", 60)
        emu.wait(3.0)
        emu.tap(*MAIN_INFO)
        emu.wait(3)
        emu.capture(d / "info.png")
        emu.wait(3)
        emu.capture(d / "info_b.png")

    out.append(Shot("info", "gold", info))

    def achievements(d: Path) -> None:
        _expect("main", 60)
        emu.wait(3.0)
        emu.tap(*MAIN_ACHIEVEMENTS)
        emu.wait(3.5)
        emu.capture(d / "achievements.png")

    out.append(Shot("achievements", "gold", achievements))

    def toggles(d: Path) -> None:
        _expect("main", 60)
        emu.wait(3.0)
        emu.tap(*MAIN_SOUND)
        emu.wait(1.5)
        emu.tap(*MAIN_MUSIC)
        emu.wait(1.5)
        emu.capture(d / "main_menu_sound_music_off.png")
        emu.tap(*MAIN_SOUND)  # restore, so later shots keep audio state
        emu.wait(1)
        emu.tap(*MAIN_MUSIC)
        emu.wait(1)

    out.append(Shot("toggles", "gold", toggles))

    for state in ("fresh", "bronze", "gold"):
        def chapters(d: Path, state: str = state) -> None:
            _start()
            emu.capture(d / f"chapters_{state}_p1.png")
            emu.tap(*S.CHAPTER_NEXT)
            emu.wait(2.5)
            emu.capture(d / f"chapters_{state}_p2.png")

        out.append(Shot(f"chapters_{state}", state, chapters))

    def levels_fresh(d: Path) -> None:
        _chapter(0)
        emu.capture(d / "levels_fresh_w1_open.png")
        emu.tap(*S.LEVEL_NEXT)
        emu.wait(2.5)
        emu.capture(d / "levels_fresh_w1_next.png")

    out.append(Shot("levels_fresh", "fresh", levels_fresh))

    for world in range(4):
        def levels_gold(d: Path, world: int = world) -> None:
            _chapter(world)
            _grid_page(2)
            emu.capture(d / f"levels_gold_w{world + 1}_p2.png")
            _grid_page(1)
            emu.capture(d / f"levels_gold_w{world + 1}_p1.png")

        out.append(Shot(f"levels_gold_w{world + 1}", "gold", levels_gold))

    def pause_gold(d: Path) -> None:
        _chapter(0)
        _grid_page(1)
        emu.tap(S.LEVEL_COLS[3], S.LEVEL_ROWS[0])  # 1-4: 1-2 and 1-3 open tutorial popups
        _expect("level")
        emu.wait(6)
        emu.capture(d / "pause_before_1-4.png")
        emu.tap(*S.GAME_PAUSE)
        emu.wait(3)
        emu.capture(d / "pause_gold_1-4.png")

    out.append(Shot("pause_gold", "gold", pause_gold))

    def pause_fresh(d: Path) -> None:
        _chapter(0)
        emu.tap(S.LEVEL_COLS[0], S.LEVEL_ROWS[0])
        _expect("level")
        emu.wait(6)
        emu.tap(*S.GAME_PAUSE)
        emu.wait(3)
        emu.capture(d / "pause_fresh_1-1.png")

    out.append(Shot("pause_fresh", "fresh", pause_fresh))

    def finished_fresh(d: Path) -> None:
        _chapter(0)
        emu.tap(S.LEVEL_COLS[0], S.LEVEL_ROWS[0])
        _expect("level")
        emu.wait(6)
        with emu.Recording(d / "finish_1-1.mp4", 18):
            emu.hold(*S.PAD_RIGHT, 5500)
            emu.wait(9)
        emu.capture(d / "finished_1-1.png")

    out.append(Shot("finished_fresh", "fresh", finished_fresh))
    return out


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--only", default="")
    parser.add_argument("--out", type=Path, default=Path("out/parity/original/screens"))
    args = parser.parse_args()
    wanted = {n.strip() for n in args.only.split(",") if n.strip()}

    for shot in shots():
        if wanted and shot.name not in wanted:
            continue
        set_save(shot.save, args.out / "_saves")
        if shot.name != "main_menu_intro":
            emu.launch(settle_s=11.0)
        shot.steps(args.out)
        print(f"captured {shot.name}", flush=True)


if __name__ == "__main__":
    main()
