# ethanon2godot

Converts the original game's Ethanon Engine data (scenes, entities, `goldenScores.enml`)
into the files the Magic Portals port on the Supersonic Engine reads: one `.tscn` text
scene per level, the chapter manifest, and copies of the art and lighting files the levels
name, and the particle systems each placement carries. The port parses the `.tscn` with its own reader (`game/sim/Tscn.cpp` in this
repository); the package keeps its name from when the target was Godot 4, whose text
scene format it still writes.

The format spec is [`../../docs/ethanon-formats.md`](../../docs/ethanon-formats.md), derived
from the engine source and checked against the real files.

## Setup and use

The repository's [`README.md`](../../README.md) has the setup: step 2 creates this
directory's `.venv/` (gitignored) and installs the package with its `[dev]` extra (pytest,
ruff); step 3 converts all four worlds. Nothing here needs network access at runtime.

```bat
tools\converter\.venv\Scripts\python -m ethanon2godot --help
```

Run it from the repository root: input comes from `reference/extracted/` (the unpacked APK)
and output goes to `out/` by default, both relative to the working directory. **Both are
gitignored** — see the note at the top of the repo `.gitignore`. The converter's output
contains Asantee-origin data and must not be committed.

Checks, from `tools/converter`:

```bat
.venv\Scripts\python -m pytest -q
.venv\Scripts\python -m ruff check .
```

## Layout

```
src/ethanon2godot/
  __main__.py   CLI entry point
  enml.py       ENML parser (Ethanon's text config format — app.enml, goldenScores.enml)
  entity.py     .ent entity-definition parser
  scene.py      .esc scene parser
  textio.py     reads Ethanon's text files in the right encoding
  tscn.py       .tscn writer (the level format the port reads)
  manifest.py   out/data/chapters.json: level order and gold thresholds
  imagesize.py  image dimensions from file headers
  tiers.py      --copy-textures: sprites plus the density tier file the original draws
  lighting.py   --copy-lighting: lightmaps, normal maps, halo bitmaps
  particles.py  --copy-particles: each placement's <Particles> as a UTF-16 .ent sidecar
  uiassets.py   --copy-ui: UI sprites, fonts, sound effects
tests/          pytest; fixtures must be hand-authored, never copied from the APK
```

## Test-fixture rule

Test fixtures are **hand-written minimal examples** of each format. Do not copy a
real Magic Portals `.ent`/`.esc` into `tests/` — that file would get committed, and
that is exactly what the repo rule forbids. Small excerpts from the Ethanon engine's
own MIT-licensed sample projects are fine as a format reference.
