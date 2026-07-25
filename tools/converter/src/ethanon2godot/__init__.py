"""ethanon2godot — Ethanon Engine project data -> Godot 4 scenes.

Reads the original game's Ethanon data (scenes, entity definitions, config) and
emits Godot 4 ``.tscn`` files for the Magic Portals remake.

Scope note: this converts *data*, not *code*. Ethanon gameplay logic lives in
AngelScript and is deliberately not translated — it is read as design reference
and reimplemented in GDScript. See ``../../CLAUDE.md``.

Legal note: input comes from ``reference/extracted/`` and output goes to ``out/``.
Both are gitignored. Nothing this package reads or writes may be committed --
see the comment block at the top of the repo ``.gitignore``.

The format spec these parsers implement is ``docs/ethanon-formats.md``, derived
from the MIT-licensed engine source at ``reference/ethanon/``. Where the doc and
the C++ disagree, the C++ wins and the doc gets fixed.
"""

__version__ = "0.1.0"

__all__ = ["__version__"]
