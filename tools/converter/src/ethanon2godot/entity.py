"""Parser for Ethanon ``.ent`` entity-definition files.

An ``.ent`` file describes an entity *type*: its sprite, sprite-sheet cut, entity
type/render mode, collision shape, lighting, particle systems, and a bag of
user-defined "custom data" key/value pairs. A ``.esc`` scene then places *instances*
of these, with per-instance overrides.

The custom-data bag is the interesting part for a remake: gameplay parameters the
original designers attached to entities live there.

Reference: ``reference/ethanon/toolkit/Source/src/engine/Entity/ETHEntityProperties.cpp``
(the deserializer) and ``ETHCustomDataManager.cpp``. Spec: ``docs/ethanon-formats.md``.

STATUS: not implemented, on purpose. A parser written against a *guessed* layout
produces code that looks correct and is silently wrong, and there is no real Magic
Portals file to validate against until the APK lands (MILESTONE_1.md §7). Implement
this against the written spec, then validate on the MIT-licensed sample projects in
``reference/ethanon/testbed/`` before ever pointing it at extracted game data.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

from .textio import read_ethanon_xml

_NOT_YET = (
    "Entity parsing is not implemented yet. Implement against docs/ethanon-formats.md, "
    "then validate on reference/ethanon/testbed/GeneralTests/entities/."
)


@dataclass
class Entity:
    """An Ethanon entity definition (``.ent``).

    Fields are intentionally minimal until the format spec is confirmed — do not
    add speculative fields here. Every field added should correspond to something
    observed in the engine source *and* in a real sample file.
    """

    name: str
    raw: dict[str, Any] = field(default_factory=dict)
    custom_data: dict[str, Any] = field(default_factory=dict)


def parse(text: str, *, name: str = "") -> Entity:
    """Parse the contents of a ``.ent`` file."""
    raise NotImplementedError(_NOT_YET)


def parse_file(path: Path) -> Entity:
    """Parse a ``.ent`` file from disk.

    Encoding is sniffed, not assumed — ``.ent`` files are canonically UTF-16LE.
    See ``textio.read_ethanon_xml``.
    """
    p = Path(path)
    return parse(read_ethanon_xml(p), name=p.stem)
