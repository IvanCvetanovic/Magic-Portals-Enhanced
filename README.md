# Magic Portals — Remake

A Godot 4 remake of **Magic Portals**, a 2D mobile puzzle-platformer originally
published by **Asantee Games** and delisted in 2025.

## Permission

This project exists with the **written permission of the rights holder** to rebuild the
game and reuse its assets. The original engine, [Ethanon](https://github.com/asantee/ethanon),
is MIT-licensed and is used here only as read-only reference for decoding the original
file formats.

**That permission covers rebuilding and reuse — it does not cover redistribution.** So:

> **No original asset, data file, or APK is ever committed to this repository.**
> `reference/` is gitignored in full and stays that way. See `CLAUDE.md` and the comment
> block at the top of `.gitignore`.

The authoritative record of the grant and its scope is `docs/permission.md`.

## Status

Project setup. No gameplay yet. Scope is deliberately narrow — see `MILESTONE_1.md`:
**chapter 1 only**, portal mechanic feeling right, 60fps on a modern Android device.
Not the full ~120 levels.

## Layout

```
game/              Godot 4 project (res:// starts here) — Compatibility renderer, mobile 2D
tools/converter/   ethanon2godot — Python 3, parses Ethanon .esc/.ent → Godot .tscn
docs/              format notes, permission record
reference/         ⛔ gitignored in full — not present in a fresh clone (see below)
```

## Local setup after cloning

`reference/` is intentionally absent from the repo. Recreate it yourself:

```bash
mkdir -p reference/apk reference/extracted
git clone --depth 1 https://github.com/asantee/ethanon.git reference/ethanon
```

Then drop the original APK into `reference/apk/` and unpack it into
`reference/extracted/`. Nothing you put there will be committed.

The converter:

```bash
cd tools/converter && python -m venv .venv && .venv\Scripts\activate && pip install -e ".[dev]"
```

## Docs

- `CLAUDE.md` — goal, legal basis, pipeline, architecture rules, definition of done
- `MILESTONE_1.md` — the vertical slice being built now
- `DEVLOG.md` — append-only build diary
- `docs/ethanon-formats.md` — Ethanon file formats, decoded from the engine source
