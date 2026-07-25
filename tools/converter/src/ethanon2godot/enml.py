"""Parser for ENML, Ethanon's own text config format.

Used by ``app.enml`` (window size, title, density settings) and other project
config files. Grammar is documented in ``docs/ethanon-formats.md``; the reference
implementation is the ENML parser in ``reference/ethanon/toolkit/Source/src/gs2d/``.

STATUS: not implemented. See module ``entity`` for why the stubs are empty.
"""

from __future__ import annotations

from pathlib import Path

_NOT_YET = (
    "ENML parsing is not implemented yet. Implement against the grammar in "
    "docs/ethanon-formats.md, verified against the reference parser in "
    "reference/ethanon/toolkit/Source/src/gs2d/."
)


def parse(text: str) -> dict[str, dict[str, str]]:
    """Parse ENML source into ``{entity_name: {attribute: value}}``."""
    raise NotImplementedError(_NOT_YET)


def parse_file(path: Path) -> dict[str, dict[str, str]]:
    """Parse an ENML file from disk.

    UTF-8 here is deliberate and is NOT the same rule as ``.ent``/``.esc``. Those
    are BOM-sniffed and canonically UTF-16LE (``textio.read_ethanon_xml``); ENML
    goes through the engine's separate ``FileManager::GetAnsiFileString``
    (``gs2d/src/Platform/FileManager.cpp:122-142``), which does not sniff.
    Do not "make this consistent" with the XML readers — they are different paths.
    """
    return parse(Path(path).read_text(encoding="utf-8"))
