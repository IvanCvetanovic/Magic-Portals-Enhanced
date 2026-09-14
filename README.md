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

**All 128 levels convert and play.** Four chapters, zero converter skips across 4,065
entity instances. Move, place portals, travel through them, collect crystals, ride lifts
and platforms, press switches, break walls, burn crates, carry keys, dodge minions, die,
retry instantly, reach the exit, advance.

Verified in-engine: a player dropped into every one of the 128 levels lands on geometry
decoded from the original's own bytes — and floats, correctly, in chapter 4's
zero-gravity levels.

**It does not yet play *like* Magic Portals.** The mechanic's structure is established, but
every timing, speed and distance is a marked guess. `MILESTONE_1.md` §3 requires those
measured, and its acceptance is the owner signing off on feel.

Scope stays narrow — chapter 1 only, not the full 128 levels.

## Layout

```
game/              Godot 4 project (res:// starts here) — Compatibility renderer, mobile 2D
tools/converter/   ethanon2godot — Python 3, parses Ethanon .esc/.ent → Godot .tscn
docs/              format notes, permission record
reference/         ⛔ gitignored in full — not present in a fresh clone (see below)
```

## Local setup after cloning

> **A fresh clone has no game content and cannot be played.** That is by design, not an
> oversight. Levels are converted from the original APK, and a converted level is the
> original's level geometry in a different encoding — so converted output is gitignored
> for the same reason the APK is. **Everyone who builds this project supplies their own
> copy of the original and runs the converter locally.** There is no path around this
> that does not involve redistributing content we have no permission to redistribute.

**1. Recreate `reference/`** — intentionally absent from the repo:

```bash
mkdir -p reference/apk reference/extracted
git clone --depth 1 https://github.com/asantee/ethanon.git reference/ethanon
```

Drop the original APK into `reference/apk/`, then unpack its `assets/` into
`reference/extracted/`. Nothing you put there will be committed.

**2. Install the toolchain.** Godot **4.6** (Standard, not .NET) and Python **3.11+**:

```bash
winget install GodotEngine.GodotEngine --version 4.6.3
cd tools/converter && python -m venv .venv && .venv\Scripts\activate && pip install -e ".[dev]"
```

`tools\godot.bat` forwards to the installed editor so the path lives in one place.

**3. Convert chapter 1** (world 0 — `level0.esc`…`level31.esc`, 32 levels):

```bash
python -m ethanon2godot --world 0 --copy-textures
```

Output lands in `out/`. Stage all three pieces into the Godot project — every
destination is gitignored:

```bash
cp out/levels/*.tscn      game/assets/levels/
cp out/assets/entities/*  game/assets/entities/
cp out/data/chapters.json game/assets/data/
```

UI art, its `hd/` tier, the bitmap fonts and audio come from `--copy-ui` (no level
references them, so they are copied wholesale):

```bash
python -m ethanon2godot --world 0 --copy-ui
cp -r out/assets/sprites out/assets/fonts out/assets/soundfx game/assets/
```

`chapters.json` is the level order plus each level's gold threshold, read from the
original's `goldenScores.enml`. Without it the game has levels but no chapter.

**4. Verify** — both exit non-zero on failure, so they work as build gates:

```bash
tools\godot.bat --headless --path game --import
tools\godot.bat --headless --path game --script res://scripts/dev/verify_levels.gd
tools\godot.bat --headless --path game res://scenes/dev/verify_core.tscn
tools\godot.bat --headless --path game res://scenes/dev/verify_gameplay.tscn
```

`verify_levels` loads every converted level. `verify_core` tests the data layer,
progression and level manager. `verify_gameplay` is the strongest check in the project: it
drops a real physics body into all 32 levels and requires it to **land** — if the collision
polygons converted wrongly, the player falls through and this fails.

(The last two run as *scenes*, not `--script`: a custom main loop is installed before
autoloads register, so `DataLoader` isn't resolvable there.)

On a clone with no converted levels both report "none found" and exit 0 — that is
correct, not a failure.

**5. Run it:**

```bash
tools\godot.bat --path game
```

Chapter 1 level 1 loads and plays.

| Input | Action |
|---|---|
| `A`/`D` or `←`/`→` | move (the original has exactly two movement buttons) |
| click / tap | place a portal |
| `R` | retry |
| `N` | skip level (dev) |
| `Esc` | quit |

**There is no jump** — three independent sources agree the original has none: the decoded
`SCREEN_PAD_BUTTON` enum contains only LEFT and RIGHT, `jumpForth` is the sole jump
identifier in the binary, and the project owner confirms it from play.

> **The mechanic's *structure* is right; its *numbers* are not.** Placement, the two-portal
> cap, single-use consumption and static-portal pairing all come from the original's data
> or from owner testimony. Every timing, speed and distance is a marked guess — see the
> `_guess` markers in `game/data/*.json` and `docs/original-gameplay.md`. `MILESTONE_1.md`
> §3 stays open until those are measured.

## Docs

- `CLAUDE.md` — goal, legal basis, pipeline, architecture rules, definition of done
- `MILESTONE_1.md` — the vertical slice being built now
- `DEVLOG.md` — append-only build diary
- `docs/ethanon-formats.md` — Ethanon file formats, decoded from the engine source
- `docs/original-gameplay.md` — how the ORIGINAL behaves, every claim tagged with its
  provenance (DATA / DECODED / IDENTIFIER / ABSENCE / OWNER / GUESS)
- `tools/asbc/` — AngelScript bytecode reader, ported from the engine's own deserialiser
