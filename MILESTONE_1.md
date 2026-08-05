# MILESTONE 1 — Vertical Slice

**Goal:** chapter 1, playable end to end, with the portal mechanic *feeling right*,
holding 60fps on a modern Android device.

**Not** the full ~120 levels. Not chapters 2+. Not the meta layer. If it isn't needed to
play chapter 1 and have it feel like the original, it is out of scope — put it in
"Open questions" instead of building it.

Read `CLAUDE.md` first, especially the hard rule about committed assets.

---

## ⚠️ This spec has holes on purpose

The original APK is not in `reference/apk/` yet, and nobody on this project has played
the original recently enough to specify it from memory. **Every number and behaviour
below that is marked `TBD` is genuinely unknown, and inventing one would be worse than
leaving it blank** — a plausible-looking wrong spec gets built, and the "feel" target is
exactly what silently fails.

The `TBD`s are not vague; each names a specific question and how to answer it. §7 is the
checklist that closes them. **Do not start building §3–§5 until §7 is done.**

Everything resolved goes into `game/data/*.json`, not into a `.gd` file (`CLAUDE.md` rule 1).

---

## 1. Definition of "done" for this milestone

One sentence: **a player can open the game, play every level of chapter 1 in order,
finish it, and the portal mechanic feels like the original — at 60fps on the target
device.**

Three independent things have to be true. They're separated below because they fail
separately and are tested separately.

---

## 2. Scope boundary

**In:**
- Chapter 1's levels only (count: `TBD` — §7.1)
- Player movement + the portal mechanic
- Whatever hazards/objects chapter 1 actually contains (`TBD` — §7.2)
- Level load from converted data, level complete, level fail, retry, next level
- A minimal level-select or straight-through progression — whichever the original does
- Touch controls (primary) and a keyboard/mouse path for desktop playtesting
- Enough audio to judge feel: portal SFX, jump/land, level complete

**Out (do not build):**
Chapters 2+, the full level set, save/cloud sync, achievements, ads/IAP, settings screens
beyond a volume slider, localisation, menus beyond what chapter 1 needs, particle-perfect
VFX parity, leaderboards, any level editor.

> ⚠ **SCOPE BOUNDARY SUPERSEDED 2026-08-05.** Ivan asked for the full game and to proceed
> without check-ins, so chapters 2-4, menus and audio are now IN scope and built. See the
> note in `CLAUDE.md` → "What this is".
>
> **The rest of this document still governs.** In particular §3's rule stands and is the
> reason the milestone is not closeable yet: portal values must be MEASURED, and a guessed
> value must be marked. Widening what gets built does not lower the bar for calling it
> done. Still genuinely out of scope: ads/IAP, cloud sync, leaderboards, level editor.

---

## 3. The portal mechanic — the part that matters

This is the milestone. Everything else is scaffolding around it.

Portals are the game's identity, and "feels correct" is not a checkbox — it's a set of
small timing and momentum decisions that are individually invisible and collectively the
whole game. Each question below must be answered **by observing the original**, then
encoded as a named, tunable value in `game/data/portals.json`.

| # | Question | How to answer | Lands in |
|---|---|---|---|
| 3.1 | What actually *is* the portal action — place/link a pair? enter one and exit another fixed one? cast one? | Play the original; record video | design note |
| 3.2 | Is momentum preserved through a portal? Rotated with the exit orientation? Clamped? | Frame-step recorded video | `momentum_mode`, `exit_speed_scale` |
| 3.3 | Exit placement: does the player emerge *at* the portal plane, offset by their own radius, or snapped to the surface? | Frame-step | `exit_offset_px` |
| 3.4 | Is there a cooldown / re-entry lockout preventing instant re-entry? How long? | Frame-step; count frames | `reentry_lockout_s` |
| 3.5 | Transit time — instantaneous, or a visible travel/fade with duration? Is the player controllable during it? | Frame-step | `transit_time_s`, `control_during_transit` |
| 3.6 | Any input buffering or coyote-time around portal entry? | Deliberate late/early inputs on the original | `input_buffer_s`, `coyote_s` |
| 3.7 | Camera behaviour on transit: hard cut, snap, or lerp? Over what duration? | Frame-step | `camera_transit_mode`, `camera_lerp_s` |
| 3.8 | What can and cannot pass through — player only? enemies? projectiles? objects? | Play the original | `portal_passable_layers` |
| 3.9 | Failure/edge cases: portal on a moving surface, two portals overlapping, portal into a wall, entering while already in transit | Try to break the original | edge-case table |
| 3.10 | Sound + VFX timing relative to the transit (lead-in? on arrival?) | Video with audio | `sfx_lead_s` |

**Feel acceptance (subjective but recorded):** side-by-side video of the original and the
remake performing the same portal transit, at the same playback speed. Ivan signs off, or
lists what's off and it gets retuned. Do this **before** building the rest of chapter 1 —
if the mechanic is wrong, level content built on top of it is wasted work.

**Hard rule for this section:** if a value is guessed rather than measured, it gets a
`# GUESS` comment in the JSON. No silent guesses.

---

## 4. Chapter 1 content

- Level count: `TBD` (§7.1)
- Levels load from **converted** data (`tools/converter/` → `.tscn`), not hand-rebuilt.
  If the converter can't produce a usable level yet, that is the blocker to fix — do not
  route around it by hand-authoring levels; the whole pipeline exists to avoid that, and
  hand-authoring 120 levels later is not an option.
- Progression: linear through chapter 1, then a "chapter complete" state. Whether the
  original gates by stars/collectibles: `TBD` (§7.3).
- Level failure and retry must be instant — no load screen between retries. Puzzle-platformers
  live or die on retry friction.

---

## 5. Performance target — 60fps, stated measurably

"60fps on a modern Android device" needs a device, a scene, and a method, or it isn't testable.

- **Device:** `TBD` (§7.5) — name the actual phone this is validated on. A mid-range
  device is a more honest target than a flagship; decide and write it down.
- **Scene:** the **worst-case level in chapter 1** — the one with the most simultaneous
  entities, particles and moving geometry. Identified once levels convert.
- **Criterion:** ≥ 60fps sustained for a 60-second play session, with **no frame over
  16.6 ms** during a portal transit. Average FPS is the wrong metric here — a single
  32 ms hitch at the moment of transit destroys the feel that this milestone is about.
- **Method:** on-screen frame-time graph + `Performance.get_monitor()` logging to a file,
  captured on-device from a release-mode export (**not** the editor, **not** a debug
  build — both lie about mobile performance).
- **Renderer:** Compatibility. Watch draw calls and texture-state changes; batch by atlas.

Record the numbers in `DEVLOG.md`. If the target is missed, the entry says by how much
and where the time went — not "needs optimisation".

---

## 6. Acceptance checklist

Portal feel:
- [ ] Every question in §3 answered from observation, values in `game/data/portals.json`
- [ ] No `# GUESS` comments left in `portals.json`
- [ ] Side-by-side transit video recorded; Ivan signs off on feel
- [ ] Edge cases from §3.9 behave sanely (no soft-locks, no falling out of the world)

Chapter 1:
- [ ] All chapter 1 levels load **from converter output** and are completable
- [ ] Level complete → next level; fail → instant retry, no load screen
- [ ] Chapter complete state exists
- [ ] Touch controls work on-device; desktop path works for playtesting

Performance:
- [ ] Named target device recorded in `DEVLOG.md`
- [ ] Release-mode APK, worst-case level, 60 s: ≥60fps sustained, no frame >16.6 ms on transit
- [ ] Frame-time capture saved

Hygiene:
- [ ] `godot --headless --path game --import` and `--quit-after 10` both clean
- [ ] `git status` shows nothing from `reference/` or `out/`
- [ ] `docs/ethanon-formats.md` updated with anything new learned, with citations
- [ ] `DEVLOG.md` entries appended per session

---

## 7. Blockers — resolve these first

All of these need the APK in `reference/apk/`. **Do not download it** (mirrors are behind
Cloudflare; it's Ivan's to supply).

- [ ] **7.1** Unpack the APK → `reference/extracted/`. Find the scene/level files. How
      many levels in chapter 1, and how is "chapter" represented in the data at all?
- [ ] **7.2** Inventory chapter 1's entity types — every distinct `.ent` referenced by
      chapter 1 scenes. That list *is* the build list for §4.
- [ ] **7.3** Determine progression/gating: linear? stars? collectibles?
- [ ] **7.4** Read `app.enml` for the authoritative viewport size and orientation, and
      replace the provisional values in `game/project.godot` (they are marked as such).
- [ ] **7.5** Pick and name the target Android device for §5.
- [ ] **7.6** Record video of the original running, for the §3 frame-stepping. This is
      the only irreplaceable artefact here — if the original ever stops running on
      available hardware, the feel reference is gone. **Do this first.**
- [ ] **7.7** Converter produces one chapter 1 level as a loadable `.tscn`. This is the
      real go/no-go for the pipeline.

**7.6 is the highest-value task in this document.** Everything in §3 depends on being able
to re-watch the original frame by frame, and that capability is the one thing that can
expire.
