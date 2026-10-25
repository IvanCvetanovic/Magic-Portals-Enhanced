#!/usr/bin/env python3
"""Refreshes the two tables of Enhanced's own that the CI fixture folder holds copies of (planning step 206, PORT-8), or checks that they are fresh.

    python tools/ci/sync_fixture_tables.py            copy data/player.json and data/portals.json over the fixture's
    python tools/ci/sync_fixture_tables.py --check    change nothing; exit 1 and name every table that differs (what test_mp_cifixture asks too)

WHY THE FIXTURE HOLDS COPIES OF THESE TWO. `MagicPortals --root tests/fixtures/ci --port-data game/data --level ci_level ...` starts from the fixture folder, and --root fills every path the game
reads from the root alone: `data/` is Enhanced's tables, of which the game reads three (Game::LoadData): the role table (the fixture's own, written for it, with invented names), the player's
tuning and the portals' rules. Those two are Enhanced's, committed in this repository, and a --data flag would replace the whole folder (and with it the fixture's role table), so the fixture holds them,
byte for byte. The port's own 45 tables (game/data) are NOT copied: the run names them with --port-data, so that nothing of theirs (their notes name other projects) is committed a second time.
Copies drift, so the suite compares them to the originals and fails with this command's name when they differ.

Standard library only. Writes only under tests/fixtures/ci/ and refuses any other target.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
FIXTURE_DATA = REPO / "tests" / "fixtures" / "ci" / "data"
SOURCE = REPO / "data"
TABLES = ("player.json", "portals.json")
# What the fixture writes itself and this tool must leave alone.
OWN = {"entity_roles.json"}


def _same(a: bytes, b: bytes) -> bool:
    # The working tree may hold CRLF (core.autocrlf) where the repository holds LF: the same table either way.
    return a.replace(b"\r\n", b"\n") == b.replace(b"\r\n", b"\n")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--check", action="store_true", help="change nothing; exit 1 when a table differs")
    args = parser.parse_args()

    problems: list[str] = []
    for name in TABLES:
        src = (SOURCE / name).read_bytes()
        dst = FIXTURE_DATA / name
        if not dst.is_file():
            problems.append(f"missing: {dst.relative_to(REPO).as_posix()}")
        elif not _same(src, dst.read_bytes()):
            problems.append(f"differs: {dst.relative_to(REPO).as_posix()}")
        else:
            continue
        if not args.check:
            FIXTURE_DATA.mkdir(parents=True, exist_ok=True)
            dst.write_bytes(src)
    present = sorted(p.name for p in FIXTURE_DATA.glob("*.json")) if FIXTURE_DATA.is_dir() else []
    for name in present:
        if name not in TABLES and name not in OWN:
            problems.append(f"surplus: {(FIXTURE_DATA / name).relative_to(REPO).as_posix()}")
            if not args.check:
                (FIXTURE_DATA / name).unlink()

    for line in problems:
        print(line)
    if args.check:
        print("the fixture's tables are the repository's" if not problems else f"{len(problems)} difference(s): run python tools/ci/sync_fixture_tables.py")
        return 1 if problems else 0
    print(f"{len(problems)} table(s) written or removed" if problems else "nothing to do")
    return 0


if __name__ == "__main__":
    sys.exit(main())
