"""Builds the chapter manifest the game loads at runtime.

The manifest is what turns 32 loose ``.tscn`` files into "chapter 1, in order, with a
gold threshold per level". It is derived entirely from the original's own data:

* level ORDER comes from the ``.esc`` naming scheme — world N is the suffix
  ``''``/``'a'``/``'b'``/``'c'``, levels 0..31 within each. Pinned from achievement
  strings in ``android_game.bin`` cross-referenced against entity placement, not guessed
  (``docs/ethanon-formats.md``, 2026-08-04 block).
* GOLD THRESHOLDS come from ``assets/goldenScores.enml``, parsed with this package's own
  ENML reader.

A "golden score" is a per-level **maximum portal count** — finish at or under it to earn
gold. Seven levels ship a threshold of ``0``, which is incoherent as a time or a par
score and coherent only as "finish using zero portals". That is why ``-1`` and not ``0``
is the "unknown" sentinel everywhere downstream.

Because it is original-derived design data, the manifest is written to ``out/`` and
staged into the gitignored ``game/assets/data/``, exactly like converted art and levels.
"""

from __future__ import annotations

import json
from pathlib import Path

from . import enml

WORLD_COUNT = 4
UNKNOWN_GOLDEN_SCORE = -1


def read_golden_scores(enml_path: Path) -> dict[int, dict[int, int]]:
    """Parse ``goldenScores.enml`` into ``{world: {level: max_portals}}``.

    The file's blocks are ``goldenScores0``..``goldenScores3`` and its keys ``level0``..
    ``level31`` — the same 0-based indexing as the ``.esc`` filenames, which is itself a
    third independent confirmation of the world/level numbering.
    """
    if not enml_path.is_file():
        return {}
    parsed = enml.parse_file(enml_path)
    out: dict[int, dict[int, int]] = {}
    for block, attrs in parsed.items():
        if not block.startswith("goldenScores"):
            continue
        suffix = block.removeprefix("goldenScores")
        if not suffix.isdigit():
            continue
        levels: dict[int, int] = {}
        for key, value in attrs.items():
            if not key.startswith("level"):
                continue
            index = key.removeprefix("level")
            if index.isdigit():
                # Values are plain integers in every shipped block, but ENML's typed
                # getters use sscanf and tolerate trailing junk, so be equally lenient
                # rather than throwing away a level over a stray character.
                try:
                    levels[int(index)] = int(float(value))
                except ValueError:
                    continue
        out[int(suffix)] = levels
    return out


def build(
    world: int,
    level_names: list[str],
    golden_scores: dict[int, dict[int, int]],
    *,
    scene_dir: str = "res://assets/levels",
    existing: dict | None = None,
) -> dict:
    """Return a manifest with ``world`` populated, preserving any other worlds.

    ``existing`` lets a later ``--world 1`` run add to what ``--world 0`` produced
    instead of erasing it. Worlds that have never been converted stay present with an
    empty ``levels`` list, so the array index always equals the world index and callers
    never have to search.
    """
    worlds: list[dict] = []
    previous = (existing or {}).get("worlds")
    for index in range(WORLD_COUNT):
        has_previous = (
            isinstance(previous, list)
            and index < len(previous)
            and isinstance(previous[index], dict)
        )
        if has_previous:
            worlds.append(previous[index])
        else:
            worlds.append({"index": index, "levels": []})

    per_world = golden_scores.get(world, {})
    worlds[world] = {
        "index": world,
        "levels": [
            {
                "index": i,
                "name": name,
                "scene": f"{scene_dir}/{name}.tscn",
                "golden_score": per_world.get(i, UNKNOWN_GOLDEN_SCORE),
            }
            for i, name in enumerate(level_names)
        ],
    }
    return {"format": 1, "worlds": worlds}


def write(path: Path, data: dict) -> None:
    """Write the manifest, merging over whatever is already there."""
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")


def read(path: Path) -> dict | None:
    """Read an existing manifest, or ``None`` if absent or unreadable.

    Unreadable is treated the same as absent on purpose: a corrupt manifest should be
    rebuilt from source data, not half-merged into.
    """
    if not path.is_file():
        return None
    try:
        loaded = json.loads(path.read_text(encoding="utf-8"))
    except (json.JSONDecodeError, OSError):
        return None
    return loaded if isinstance(loaded, dict) else None
