# ethanon2godot

Converts Ethanon Engine project data (scenes, entities) into Godot 4 `.tscn` scenes
for the Magic Portals remake.

## Status

**Scaffolding only.** The parsers are stubs. They are deliberately not written yet:
writing a parser against a guessed format produces code that looks right and is
wrong. The format spec is being derived from the engine source — see
[`../../docs/ethanon-formats.md`](../../docs/ethanon-formats.md) — and the parsers
get written against that, then validated on the real files from the APK.

## Setup

```bash
cd tools/converter
python -m venv .venv
.venv\Scripts\activate         # Windows
# source .venv/bin/activate    # Linux/macOS
pip install -e ".[dev]"
```

`.venv/` is gitignored. Nothing here needs network access at runtime.

## Use

```bash
ethanon2godot --help
```

Input comes from `reference/extracted/` (the unpacked APK) and output goes to
`out/` by default. **Both are gitignored** — see the note at the top of the repo
`.gitignore`. The converter's output can contain Asantee-origin data and must not
be committed.

## Layout

```
src/ethanon2godot/
  __main__.py   CLI entry point
  enml.py       ENML parser (Ethanon's text config format — app.enml etc.)
  entity.py     .ent entity-definition parser
  scene.py      .esc scene parser
  tscn.py       Godot 4 .tscn writer
tests/          pytest; fixtures must be hand-authored, never copied from the APK
```

## Test-fixture rule

Test fixtures are **hand-written minimal examples** of each format. Do not copy a
real Magic Portals `.ent`/`.esc` into `tests/` — that file would get committed, and
that is exactly what the repo rule forbids. Small excerpts from the Ethanon engine's
own MIT-licensed sample projects are fine as a format reference.
