# MAGIC PORTALS — Remake

## What this is

A ground-up remake of **Magic Portals** — a 2D mobile puzzle-platformer originally
published by **Asantee Games** and delisted in 2025. The original was built on the
**Ethanon Engine** (Asantee's own 2D engine, MIT-licensed).

**The remake is the Magic Portals port on Ivan's own Supersonic Engine** (C++20, Vulkan,
EnTT), and it lives here: `game/` (the port), `tests/` (its `test_mp_*` suites) and the
top-level `CMakeLists.txt`, built against the engine at `engine/` — a git submodule of
`https://github.com/IvanCvetanovic/Supersonic-Engine.git` pinned to a commit, "SE" below.
The port moved here from SE's `games/magicportals` on 2026-09-19, with its history. Its
record, every step with its gates, is `docs/planning/2026-09-11-magic-portals-remaster.md`.
Around it, this repository is the port's data source and parity rig: the converter
(`tools/converter`), the remake's gameplay tables (`data/*.json`), the AngelScript bytecode
reader (`tools/asbc`), the parity tools (`tools/parity`) and the docs. The Godot 4 project
that first lived in `game/` stopped being the target on 2026-09-14 and was removed on
2026-09-19. `C:\Users\icvet\Desktop\Game-Engine` is **not** Ivan's engine: never touch it.

- **Original scope:** 128 levels (4 worlds × 32 — pinned exactly, not "~120").
- **SCOPE WIDENED 2026-08-05, by Ivan, explicitly.** The previous rule here was "chapter 1
  only — do not build ahead of the milestone", and `MILESTONE_1.md` excluded menus, audio
  and chapters 2+. Ivan has asked for the full game and to proceed without check-ins, so
  that restriction no longer applies. `MILESTONE_1.md` is kept as the **quality bar** —
  especially §3's rule that portal values must be measured, not guessed — but its scope
  boundary is superseded. Where the two disagree, this note wins.
- All 128 levels convert, and all 128 start and play on the port. The remaining work is
  parity with the original: `docs/parity-backlog.md`, in order.

This is a *remake*, not an emulator or a repack. Ethanon's runtime is not being ported;
its **file formats** are being decoded so that level and entity data can be *translated*.
The converter writes each level as a `.tscn` text scene, the format the port's own reader
(`game/sim/Tscn.cpp`) parses. The gameplay is rebuilt in C++ in the port and measured
against the original running in an emulator.

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

**What IS committed:** our own code — the port (`game/`, `tests/`), the converter and the
tools — the engine's pin (`engine/` is a submodule; the engine's files live in its own
repository), docs, and hand-authored test fixtures.

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
        │  .esc / .ent  →  out/levels/*.tscn, out/data/chapters.json, out/assets/
        ▼
  game/                    the actual remake (C++20, hand-written gameplay) on engine/;
                           reads out/, data/ and reference/extracted/assets/ where they are
```

Each arrow is a separate, checkable step. The rule of thumb: **the engine source is the
only trustworthy spec.** Where `docs/ethanon-formats.md` and the C++ disagree, the C++
wins and the doc gets fixed.

The converter is a *data* pipeline, not a code pipeline. Ethanon gameplay logic lives in
AngelScript and is **not** converted — it is read as design reference and reimplemented.

The port finds its inputs through absolute paths baked into `build/CMakeCache.txt` when
`build/` is configured, each defaulting to its place in this repository:
`SUPERSONIC_MAGICPORTALS_LEVELS` (`out/levels`), `_CHAPTERS` (`out/data/chapters.json`),
`_DATA` (`data`), `_ORIGINAL` (`reference/extracted/assets`) and `_ACHIEVEMENTS`
(`out/data/achievements.json`); the port's own `game/data` is read from the source tree.
Moving or renaming any of them breaks the port's runs and turns its level suites into skips
(exit 77, which ctest reports as SKIPPED, never a pass). The build still compiles: the paths
are only compile definitions (`game/sim/CMakeLists.txt`).

---

## Environment (Ivan's machine)

**Checked 2026-09-19.**

- OS: Windows 11. This repo is `C:\Users\icvet\Desktop\Magic-Portals-Remake`, with the
  engine checked out inside it at `engine/` (`git submodule update --init`). SE's own clone,
  `C:\Users\icvet\Desktop\Supersonic-Engine`, is where engine changes are made and pushed;
  moving `engine/` to a new engine commit is then a commit here. **There is no `D:` drive** —
  `C:` is the only filesystem drive. Neither path has spaces; keep it that way.
- **Compiler: MSVC from Visual Studio Build Tools 18** (toolset 14.50, x64).
  `tools\parity\port_build.bat` calls its `vcvars64.bat`, configures this repo's `build/` if
  it has no cache yet, and runs `cmake --build` on it, so the Build Tools path lives in that
  one file.
- **CMake 3.29.2, Ninja 1.12.0 and ctest are Strawberry Perl's** (`C:\Strawberry\c\bin`, on
  PATH). `build/` is Ninja, Release, with validation on; the engine is a subproject there, so
  it builds `SupersonicCore` only — no editor, script plugin or engine suites.
- **Vulkan SDK 1.4.357.0** (`C:\VulkanSDK\1.4.357.0`): the validation layers and `glslc`.
  `port_build.bat` configures with `GLSL_COMPILER=C:/VulkanSDK/1.4.357.0/Bin/glslc.exe`. The
  port's build never recompiles the engine's shaders: only the engine's editor depends on its
  `Shaders` target, so `engine/assets/shaders/*.spv` stay as the engine commits them.
- **Windows Smart App Control is in enforce mode.** It sometimes refuses the first launch of
  a freshly linked exe ("An Application Control policy has blocked this file", "Permission
  denied", ctest "Not Run"), and **every refused launch shows Ivan a Windows notification.**
  Relink nothing you do not need, launch a suite at most once, never loop ctest or relinks,
  and report a refused exe as not run. From this repo's root, `ctest --test-dir build -N`
  lists tests without launching one.
- Python **3.14.3** on PATH as `python`. The converter's venv is `tools\converter\.venv`
  (it targets `>=3.11` and has zero runtime deps; `[dev]` adds pytest and ruff). The parity
  scripts have their own, `tools\parity\.venv` (README, *Parity work*).
- The original runs only in the Android emulator rig — the API 30 x86 image, a patched APK,
  adb: `docs/parity.md`. Read its §1-2 before running any script in `tools/parity/`.

---

## Directory layout

```
CMakeLists.txt         the build: engine/ as a subproject, then game/ and tests/
engine/                Supersonic Engine, git submodule pinned to a commit; never edited here
game/                  the port: main.cpp, MagicPortalsLayer, LevelVisit, sim/, spike/, data/
  data/                the port's own JSON: decoded, measured or authored numbers, with provenance
tests/                 the port's test_mp_* suites and their registration
data/                  the remake's gameplay JSON, committed; the port reads it in place (_DATA above)
tools/converter/       ethanon2godot — Python 3, src layout, venv-friendly
tools/asbc/            AngelScript bytecode reader; achievements.py → out/data/achievements.json
tools/parity/          emulator driver, reference-library capture, frame comparison, port_build.bat
docs/                  format notes, the original's behaviour, parity rig and backlog, permission record
  planning/            the port's planning records; the remaster doc is its live step record
build/                 CMake build tree (gitignored): build/game/MagicPortals.exe, build/tests/
reference/             ⛔ GITIGNORED IN FULL — see .gitignore
  apk/                 original APK, dropped in manually
  extracted/           unpacked APK contents
  ethanon/             engine source, shallow clone, READ-ONLY
out/                   converter output and parity captures (gitignored); the port reads
                       out/levels, out/data and out/assets
```

There is no Godot project any more: its `game/data/` tables are now `data/`, and `game/`
is the port. `reference/` does not exist in a fresh clone. Recreate it locally — README.md
says how.

---

## Architecture rules (this repository)

1. **Data-driven, with provenance.** Gameplay numbers live in JSON, never in code:
   `data/*.json` for the remake's tables, `game/data/*.json` for the port's own. A
   designer must be able to retune without touching code. Every number says where it came
   from — decoded (with its citation), measured on the original (with the capture), or
   guessed and marked (`"_guess": true` beside it, as `player.json` does). No silent
   guesses. This matters more than usual here: the converter *generates* part of this data,
   so it has to be data in the first place.
2. **The converter is a data mapping**, not a code translator and not a scene author.
   Entities stay one generic node plus data, as Ethanon has them (`.ent` file + per-instance
   overrides). What it emits is what the port's reader parses: a change to the `.tscn` or a
   manifest is a change on both sides, and `tests/test_mp_levels.cpp` pins counts taken
   from the converted files.
3. **No magic strings.** `enum` or `const` for ids, states, keys — in the converter and in
   the port.
4. **Do not reimplement Ethanon's engine.** The fidelity target is the player's experience,
   and the original on screen is the ground truth: measure it (`docs/parity.md`), then build
   what it shows.
5. **Port code follows the engine's own conventions**, not this file: `engine/AGENTS.md`
   (build, test, run), `engine/CONTRIBUTING.md` (what a change carries, style, commits) and
   `engine/ARCHITECTURE.md`. Every port change is a Step in
   `docs/planning/2026-09-11-magic-portals-remaster.md`, with its gates. An engine gap found
   on the way is fixed in the engine's own repository first; `engine/` then moves to that
   commit here.

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

1. **One system per task.** If the spec is missing something, note it under "Open
   questions" (in the planning doc, the step's LEFT FOR THE OWNER) rather than inventing
   behaviour.
2. **Verify before finishing, every time**, for what the change touched:
   - **Converter** — from `tools/converter`, both clean (Git Bash or PowerShell; cmd.exe
     needs backslashes, `.venv\Scripts\python`, as in the converter README):
     ```
     .venv/Scripts/python -m pytest -q
     .venv/Scripts/python -m ruff check .
     ```
     Convert into a staging directory under the gitignored `out/`, and compare it with what
     the port reads (`out/levels`, `out/data`, `out/assets`) before replacing anything there.
   - **Port** — `tools\parity\port_build.bat [--target X]` must finish with **zero
     warnings** (from Git Bash: `cmd //c "tools\parity\port_build.bat --target MagicPortals"`).
     Then run the `test_mp_*` suites the change touches **directly, once each**, from this
     repo's root: `build/tests/test_mp_<suite>.exe`. **Not a full ctest loop** — see Smart App
     Control above. Record each suite's check count, and name any refused one as not run.
   - **Captures** — from **Bash** only (PowerShell 5.1 mangles the arguments), from this
     repo's root, into its gitignored `out/parity/port/` (`--screenshot` writes into an
     existing directory only). **Give every path argument as an absolute path:** the game
     finds the engine's shaders in `engine/` and makes that its working directory before it
     opens anything, so a relative `--screenshot` would land inside the submodule:
     ```
     mkdir -p out/parity/port
     build/game/MagicPortals.exe --level <name> --window 1280x720 --fixed-step --frames N --screenshot "$PWD/out/parity/port/<name>.png"
     ```
3. **Append a `DEVLOG.md` entry** every session. Append-only, brutally honest, follow the
   template at the top of that file. Failures and dead ends are the point.
4. Keep diffs small and commit-sized. Prefer editing over rewriting.
5. **Commits, here and in the engine's repository, as `IvanCvetanovic <icvetanovic99@gmail.com>`**
   with no AI trailers: no `Co-Authored-By`, no session line, no mention of AI. A port commit is
   titled `Magic Portals: <title>`, and its last line is
   `Record: docs/planning/2026-09-11-magic-portals-remaster.md, step N`. Before any commit
   here, `git diff --cached --name-only | grep -iE '\.(png|bmp|jpg|jpeg|mp3|ogg|wav|esc|ent|enml|par|fnt)$'`
   must print nothing, and nothing from `reference/` or `out/` may be staged. Never
   force-push, rewrite history, or `reset --hard` / `checkout --` over uncommitted work.
6. Comments explain *why*, not *what*.

---

## Definition of done (any task)

- [ ] Converter change: `pytest` and `ruff check` clean in `tools/converter/.venv`
- [ ] Port change: `port_build.bat` with zero warnings, and the `test_mp_*` suites it
      touches run once each — counts recorded, refused launches reported as not run
- [ ] `git status` shows nothing from `reference/` or `out/` — **check this literally**
- [ ] New tunable values are in JSON (`data/*.json` or `game/data/*.json`) with their
      provenance, not in code
- [ ] A port change has its Step in `docs/planning/2026-09-11-magic-portals-remaster.md`,
      with its gates
- [ ] Input goes through the port's one pointer path: the mouse stands in for the
      original's touch (README, *Run it*)
- [ ] Any new format knowledge is written into `docs/ethanon-formats.md` **with citations**
- [ ] `DEVLOG.md` entry appended
