# MAGIC PORTALS — Remake

## What this is

A ground-up remake, in Godot 4, of **Magic Portals** — a 2D mobile puzzle-platformer
originally published by **Asantee Games** and delisted in 2025. The original was built
on the **Ethanon Engine** (Asantee's own 2D engine, MIT-licensed).

- **Engine:** Godot 4.6 target (see Environment below — installed editor is 4.7.1)
- **Language:** GDScript only. No C#. No GDExtension unless explicitly requested.
- **Targets:** Android (primary). Desktop is the development/playtest surface.
- **Renderer:** Compatibility (`gl_compatibility`) — broadest Android device support.
- **Original scope:** ~120 levels. **Current scope: chapter 1 only** — see `MILESTONE_1.md`.
  Do not build ahead of the current milestone.

This is a *remake*, not an emulator or a repack. Ethanon's runtime is not being ported;
its **file formats** are being decoded so that level and entity data can be *translated*
into native Godot scenes. The gameplay is being rebuilt in idiomatic GDScript.

---

## ⛔ THE HARD RULE — read this before every commit

**No extracted asset and no Asantee-origin file is EVER committed to this repository.**

Not textures. Not audio. Not `.ent`/`.esc`/`.enml` data files. Not the APK. Not a
"small excerpt for a test fixture." Not "temporarily, I'll remove it later."

Why this is absolute:
- The permission granted covers **rebuilding the game and reusing assets in it**. It
  does **not** grant redistribution rights. A public git repository *is* redistribution.
- Git history is effectively permanent. A later `git rm` does not undo the commit; it
  takes a history rewrite plus a force-push, and any clone or fork made in between keeps
  the file forever.
- The Ethanon engine source is MIT and *could* legally be vendored — it still isn't. It
  is read-only decoding reference with its own upstream, so it is cloned, not committed.

Mechanically enforced by `/reference/` in the root `.gitignore`. **That line and its
comment block must not be removed, narrowed, or given a negation exception.** If a
future task seems to need one, the answer is "put the file somewhere outside the repo,"
not "add `!reference/something`."

Converter *output* is under the same rule: anything the converter emits that still
carries original art, audio, or level data goes to `/out/` (gitignored) and stays there
until a human has reviewed what it actually contains.

**What IS committed:** our own GDScript, our own scenes, the converter source, docs,
and hand-authored test fixtures.

---

## Legal basis

| Thing | Status |
|---|---|
| Remaking the game | **Permitted** — written permission from the rights holder |
| Reusing the original assets in the remake | **Permitted** — same grant |
| Redistributing the original assets (incl. committing them) | **NOT permitted** |
| Ethanon Engine source | **MIT** — free to read, modify, vendor; we only read it |
| Shipping the remake | Governed by the grant — see scope in `docs/permission.md` |

`docs/permission.md` is the authoritative record. If a question about what is allowed
comes up, that file governs — not this summary, and not anyone's recollection.
**If `docs/permission.md` still contains its `PASTE THE ACTUAL GRANT HERE` placeholder,
the record is incomplete: say so and stop rather than assuming scope.**

---

## The pipeline

```
  reference/apk/           original APK           (manual drop — never committed)
        │
        │  unzip / apktool
        ▼
  reference/extracted/     assets, scenes/, entities/, data/, app.enml
        │
        │  decode: what do these bytes MEAN?
        │  ← reference/ethanon/  (engine source = the format spec)
        │  → docs/ethanon-formats.md  (the written-down answer)
        ▼
  tools/converter/         ethanon2godot (Python 3)
        │
        │  .esc / .ent  →  Godot 4 .tscn + resources
        ▼
  game/                    the actual remake (GDScript, hand-written gameplay)
```

Each arrow is a separate, checkable step. The rule of thumb: **the engine source is the
only trustworthy spec.** Where `docs/ethanon-formats.md` and the C++ disagree, the C++
wins and the doc gets fixed.

The converter is a *data* pipeline, not a code pipeline. Ethanon gameplay logic lives in
AngelScript and is **not** converted — it is read as design reference and reimplemented.

---

## Environment (Ivan's machine)

- OS: Windows. Project lives on `D:` in a path without spaces — keep it so.
- Godot is the **Steam install**:
  `D:\SteamLibrary\steamapps\common\Godot Engine\godot.windows.opt.tools.64.exe`
  - Verified 2026-07-25: reports **`4.7.1.stable.steam.a13da4feb`**.
  - The path contains spaces — always quote it, or add `tools\godot.bat` forwarding
    all args to it.
  - **Version mismatch, stated plainly:** the spec says Godot 4.6; the only installed
    editor is 4.7.1. `project.godot` declares `config/features=("4.6", ...)` as
    specified. 4.7.1 opens it without complaint and will rewrite that tag to `"4.7"` the
    first time the project is saved from the editor GUI. Nothing breaks. If 4.6 is a
    hard requirement (e.g. to match a collaborator), install 4.6 explicitly — don't rely
    on the feature tag to pin it.
  - Steam auto-updates Godot. Recommended: Steam → Godot Engine → Properties → Updates →
    "Only update the game when I launch it".
- Python 3.11.9 on PATH as `python`. The converter targets `>=3.11`.
- The Godot project root is **`game/`**, not the repo root. All `--path` invocations and
  `res://` paths are relative to `game/`.

---

## Directory layout

```
game/                  Godot 4 project  (res:// starts HERE)
  project.godot
  scenes/              minimal .tscn — root node + structure only
  scripts/             core/ (autoloads) · entities/ · systems/ · input/ · ui/
  data/                ALL gameplay numbers, as JSON
  assets/              our own + converted art (see the hard rule)
tools/converter/       ethanon2godot — Python 3, src layout, venv-friendly
docs/                  format notes, permission record
reference/             ⛔ GITIGNORED IN FULL — see .gitignore
  apk/                 original APK, dropped in manually
  extracted/           unpacked APK contents
  ethanon/             engine source, shallow clone, READ-ONLY
out/                   converter output staging (gitignored)
```

`reference/` does not exist in a fresh clone. Recreate it locally — README.md says how.

---

## Architecture rules

1. **Data-driven everything.** Level layouts, portal timings, entity stats, chapter
   structure — `game/data/*.json`, loaded once by a `DataLoader` autoload. A designer
   must be able to retune without touching a `.gd` file. Hardcoded gameplay values are
   bugs. This matters more than usual here: the converter *generates* this data, so it
   has to be data in the first place.
2. **Typed GDScript everywhere.** `var hp: int`, `func take_damage(amount: int) -> void`.
   Typed code is faster on the VM and catches mistakes at parse time.
3. **Few autoloads.** Start with `EventBus` (global signals), `GameState`, `DataLoader`.
   Do not add a fourth without asking.
4. **Input abstraction.** Gameplay code never reads `InputEvent` directly. One
   `InputController` translates touch AND mouse into semantic intents. Touch-first:
   touch targets ≥ 96 px. Touch and desktop share one code path.
5. **Entities are one generic scene + data**, not a scene per entity type — this mirrors
   how Ethanon works (`.ent` file + per-instance overrides) and keeps the converter's
   job a data mapping rather than a scene-authoring job.
6. **Performance discipline** (mobile, 60fps target):
   - Never scan for targets in `_process`. AI/logic ticks on a staggered timer.
   - Movement and animation interpolate every frame; *thinking* does not.
   - Pool anything spawned in bulk (particles, projectiles, floating numbers).
   - Compatibility renderer: watch draw calls and texture-state changes. Batch by atlas.
7. **No magic strings.** `enum` or `const` for ids, states, signal names.
8. **Do not reimplement Ethanon's engine.** If a behaviour is awkward in Godot, do the
   Godot-native thing that *feels* the same. Fidelity target is the player's experience,
   not the engine's internals.

---

## Working with the reference material

- `reference/ethanon/` is **read-only**. Never build it, never modify it, never commit it.
  It is a shallow clone; `git -C reference/ethanon pull` if it ever needs refreshing.
- When decoding a format, **read the C++, then verify against a real sample file.** Both.
  The engine ships sample projects under `reference/ethanon/testbed/` and
  `reference/ethanon/documentation/Sample-Projects/` — those are MIT and are the safe
  files to experiment on before the APK ever gets involved.
- Record every format finding in `docs/ethanon-formats.md` with a
  `path/to/File.cpp:line` citation. An uncited claim about a byte layout is a guess.
- The original game is delisted. **Do not attempt to download the APK** — mirrors are
  behind Cloudflare and it is Ivan's to supply. If `reference/apk/` is empty, say so and
  work on something that doesn't need it.

---

## Workflow rules for the agent

1. **One system per task.** If the milestone spec is missing something, note it under
   "Open questions" rather than inventing behaviour.
2. **Verify before finishing, every time:**
   ```
   godot --headless --path game --import
   godot --headless --path game --quit-after 10
   ```
   Both must complete with zero script errors. If a flag errors, check `godot --help` —
   do not guess replacements silently.
3. **Append a `DEVLOG.md` entry** every session. Append-only, brutally honest, follow the
   template at the top of that file. Failures and dead ends are the point.
4. Keep diffs small and commit-sized. Prefer editing over rewriting.
5. If unsure about a Godot 4 API, check `--help` or the installed docs over guessing.
   Godot 3.x patterns (`yield`, `KinematicBody2D`, `instance()`) are wrong here.
6. Comments explain *why*, not *what*.

---

## Definition of done (any task)

- [ ] Headless import + run pass with zero errors
- [ ] `git status` shows nothing from `reference/` or `out/` — **check this literally**
- [ ] New tunable values are in `game/data/*.json`, not in scripts
- [ ] New GDScript is fully typed
- [ ] Works on both the mouse and touch input path
- [ ] Any new format knowledge is written into `docs/ethanon-formats.md` **with citations**
- [ ] `DEVLOG.md` entry appended
