"""Writer for Godot 4 ``.tscn`` scene files.

Godot's text scene format is plain text and stable enough to emit directly — no
Godot install is needed to run the converter, which keeps it CI-friendly and fast.

The only piece here that is safe to build ahead of the Ethanon format spec is the
*escaping*, because it depends on Godot's format, not Ethanon's. Everything that
maps Ethanon concepts onto Godot nodes waits for ``docs/ethanon-formats.md``.

STATUS: escaping implemented; node emission not implemented.
"""

from __future__ import annotations

_NOT_YET = (
    "Scene emission is not implemented yet. The Ethanon -> Godot node mapping is "
    "defined in docs/ethanon-formats.md (Converter implications)."
)

# Godot 4 text scene format version. `format=3` is what Godot 4.x writes.
TSCN_FORMAT = 3


def quote(value: str) -> str:
    """Escape a Python string as a Godot ``.tscn`` string literal.

    Godot's text parser uses C-style escapes inside double quotes. Backslash must
    be escaped first, or escaping the quote character would double-escape it.
    """
    escaped = (
        value.replace("\\", "\\\\")
        .replace('"', '\\"')
        .replace("\n", "\\n")
        .replace("\r", "\\r")
        .replace("\t", "\\t")
    )
    return f'"{escaped}"'


def write_scene(*args: object, **kwargs: object) -> str:
    """Render a converted scene to ``.tscn`` source."""
    raise NotImplementedError(_NOT_YET)
