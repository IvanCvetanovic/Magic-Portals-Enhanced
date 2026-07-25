"""Parser for Ethanon ``.esc`` scene files.

A ``.esc`` holds scene-level properties (ambient light, z-axis direction, parallax,
bucket size) plus the list of placed entity *instances* — each referencing a ``.ent``
by name and carrying its own position, angle, scale, colour and custom-data overrides.

For this project a ``.esc`` is a level. Chapter 1's scenes are the input to
MILESTONE_1.md §4.

Reference: ``reference/ethanon/toolkit/Source/src/engine/Scene/ETHScene.cpp`` and
``ETHSceneProperties.cpp``. Spec: ``docs/ethanon-formats.md``.

STATUS: not implemented. See module ``entity`` for why.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

from .textio import read_ethanon_xml

_NOT_YET = (
    "Scene parsing is not implemented yet. Implement against docs/ethanon-formats.md, "
    "then validate on reference/ethanon/testbed/GeneralTests/scenes/."
)


@dataclass
class Scene:
    """An Ethanon scene (``.esc``) — one level."""

    name: str
    properties: dict[str, Any] = field(default_factory=dict)
    instances: list[dict[str, Any]] = field(default_factory=list)


def parse(text: str, *, name: str = "") -> Scene:
    """Parse the contents of a ``.esc`` file."""
    raise NotImplementedError(_NOT_YET)


def parse_file(path: Path) -> Scene:
    """Parse a ``.esc`` file from disk.

    Encoding is sniffed, not assumed — ``.esc`` files are canonically UTF-16LE.
    See ``textio.read_ethanon_xml``.
    """
    p = Path(path)
    return parse(read_ethanon_xml(p), name=p.stem)
