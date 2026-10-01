# Magic Portals — how the original behaves

Design reference for Magic Portals Enhanced. **Separate from `ethanon-formats.md` on purpose:** that
file documents Ethanon's *file formats*, this one documents *this game's* behaviour.

CLAUDE.md: Ethanon gameplay logic "is **not** converted — it is read as design reference
and reimplemented." Nothing here is translated code. It is observations, each carrying its
provenance:

| Marker | Means |
|---|---|
| **DATA** | Read from the original's own level/entity files. Highest confidence. |
| **DECODED** | Decoded from `android_game.bin` with `tools/asbc/`. Cited, verified. |
| **IDENTIFIER** | An identifier string exists in the compiled script. Proves the *concept* exists; says nothing about values. |
| **ABSENCE** | An exhaustive search found nothing. Weak but real evidence. |
| **OWNER** | Stated by Ivan from having played the original. Not measured, but primary testimony from the project owner — outranks IDENTIFIER and ABSENCE, and is the best source available for behaviour that leaves no trace in the data. |
| **GUESS** | Invented. Must be measured. |

Established 2026-08-05.

---

## 1. Controls — the character is player-driven

**DECODED.** `tools/asbc/asbc.py` decodes the module's three enums cleanly
(239 bytes, no desync). Two matter:

```
enum DIRECTION           MOVING_DOWN=0  MOVING_LEFT=1  MOVING_RIGHT=2  MOVING_UP=3
enum SCREEN_PAD_BUTTON   NO_SCREEN_BUTTON=0  LEFT_SCREEN_BUTTON=240  RIGHT_SCREEN_BUTTON=255
```

**`SCREEN_PAD_BUTTON` contains only LEFT and RIGHT.** There is no up, down, jump or action
button in the on-screen pad. So the touch control scheme for *movement* is two buttons,
horizontal only — and everything else the player does (placing portals) must happen
through direct taps on the world rather than through the pad.

`DIRECTION` carries all four compass values, but that is a general-purpose enum also used
by `anti_portal_agent` (whose custom data includes `direction: horizontal`) — it is not
evidence of four-way player movement.

**IDENTIFIER.** `ScreenPad` (`assets/ScreenPad.angelscript`), `setDirection`,
`MainCharacter` (`assets/MainCharacter.angelscript`), `getMainCharacter`,
`killMainCharacter`, `isCharacterDead`.

**IDENTIFIER.** Ground contact is a *sensor entity*, not a raycast: `char_feet.ent` with
`ETHBeginContactCallback_char_feet` / `ETHEndContactCallback_char_feet`.

**ABSENCE.** `jumpForth` is the **only** jump-related identifier in the entire binary.

**OWNER (2026-08-05).** Ivan confirms the original had only move-left and move-right, plus
screen taps for portals. **There is no jump**, and Enhanced now ships with it disabled
(`player.json` → `jump.enabled: false`). Three independent sources agree — a decoded enum,
an exhaustive absence, and testimony — which makes this the best-established fact in this
document.

**ABSENCE.** Zero hits for coyote-time or input-buffering identifiers. Evidence the
original has no forgiveness window. `player.json` sets both to 0 on this basis.

---

## 2. Portals

### 2.0 The core loop — OWNER, 2026-08-05

Stated by Ivan, and it resolves the questions the data could not:

1. **Movement is two buttons, left and right.** Independently confirmed by the decoded
   `SCREEN_PAD_BUTTON` enum (§1) — testimony and decode agree.
2. **Portals are created by tapping the screen.** So the pad moves, taps place. This is why
   there is no third pad button.
3. **At most TWO portals exist at once** — one pair.
4. **Passing through a portal destroys the pair.** They are consumed on use, not permanent.
5. **The same applies to other items**, not just the player — a crate or rolling stone
   travelling through consumes the pair too. This meshes exactly with the `teleportable`
   flag in §2.4.

**This makes the mechanic legible.** A level is a puzzle about *where* to spend a limited
number of one-shot portal pairs, and the golden score (a maximum portal count, §3) is a
budget on how many pairs you spend getting through. That also reconciles the achievement
text — "Finish level 1-20 with 8 portals or less" is a running total across the attempt,
not a simultaneous cap.

⚠ **Not covered by this testimony, still open:** whether `portal_static` entities are also
consumed. Enhanced assumes **they persist** — the name says static, the engine has a
dedicated `teleportToFirstStaticPortal`, and level 1-1 would become unsolvable after two
uses if its four static portals vanished. Reasoned, not confirmed.

### 2.1 The player places them, against a per-level budget

**DATA.** A `properties` entity carries `max_portals` in its custom data. Chapter 1 values:
level 1-1 → `0`, level 1-2 → `1`, level 1-15 → `0`. Only 6 of 32 chapter-1 scenes place a
`properties` entity at all; **absent is not the same as zero** — 1-1 stores an explicit 0.

**DECODED-ADJACENT (achievement text in the binary).** "Finish level 1-20 with 8 portals or
less", "Finish level 1-21 with 4 portals or less", "Finish level 2-14 with seven portals".
So portal count is a scored resource the player spends.

**IDENTIFIER.** `insertPortal`, `managePortalInsertion`, `computePortalFinalPos`,
`incrementPortalCounter`, `decrementPortalCounter`, `getPortalCounter`, `getNumPortals`,
`_maxPortals`, `insertionTime`, `killPortals`, `blockPortalEffects`.

### 2.2 Static portals are explicitly paired in the level data

**DATA.** Each `portal_static` carries `index`, `destiny`, `color` and `active`.
`destiny` is the index it sends the traveller to. All seven in chapter 1:

| Level | index → destiny | colour |
|---|---|---|
| 1-1 | 1 → 0 | blue |
| 1-1 | 0 → 1 | red |
| 1-1 | 2 → 3 | red |
| 1-1 | 3 → 2 | blue |
| 1-2 | 0 → **1** | blue |
| 1-15 | 2 → 3 | red |
| 1-15 | 3 → 2 | blue |

Pairs are mutual (0↔1, 2↔3) and always one red + one blue. **DECODED:** `PORTAL_RED` and
`PORTAL_BLUE` exist as identifiers.

### 2.3 Player-placed portals share the static index space

**DATA, and the most useful inference here.** Level 1-2 ships **one** static portal —
`index 0`, `destiny 1` — and grants `max_portals: 1`. There is no static portal with index
1. The destination is the portal *the player places*.

So static and placed portals occupy **one index space**, and 1-2 is the tutorial for
placement: a fixed blue portal already pointing at the red one you are about to create.

This was found by an assertion failing in `verify_gameplay.gd` — the test asserted every
`destiny` resolved to a static portal, and 1-2 disproved it.

### 2.4 Not everything can travel

**DATA.** A `teleportable` flag in per-instance custom data:

| `teleportable = 1` | `teleportable = 0` |
|---|---|
| `crate.ent`, `crate_small.ent`, `rolling_stone.ent`, `rolling_stone` | `crate` (inline), `crate_small` (inline), `wood_piece_small` |

Per-instance and designer-controlled — the same logical object is teleportable in one
placement and not in another. This answers MILESTONE_1 §3.8 from data.

**DECODED** (`PortalManager`, planning step 135). A **fireball** is teleportable too, and the
portal manager names `fireball.ent` twice on its way through a placed pair: `mayTeleport` lets
one through only while its `teleportedOnce` is clear, and `doesItDestroyPortals` is false for it.
So a fireball goes through a pair **once**, comes out of the partner at its point **flying back
the way it came** (its `dir` times -1), and **the pair stays** for the next one; both ends are
then marked as having carried something, so the clear-portals button refunds neither. It is in
a portal while its centre is within 22 u of the portal's (half its unscaled 16 u box plus the
portal's 14). A lone portal carries nothing. This is how a pair turns a carranca's stream back
(the help popup on 2-03 and 2-06 shows it) or sends it onto a crate or a shock agent.

### 2.5 Portals can be blocked

**DATA.** `antiportal` entities carry a `scale` multiplier (chapter-1 values 1.8 … 9).
`no_portal` and `no_portal_sign.ent` also appear.
**IDENTIFIER.** `AntiPortalManager`, `isPointInAntiPortalField`, `inAntiportalField`,
`anti_portal_agent` (custom data: `speed`, `stride`, `direction: horizontal`).

### 2.6 Two distinct cooldowns exist — values unknown

**IDENTIFIER.** `FIRST_PORTAL_MIN_TIME` and `NEXT_PORTAL_MIN_TIME`, plus `lastPortalTimer`
and `insertionTime`. That there are **two** separate constants is itself a design fact: the
first portal and subsequent portals are rate-limited differently.

⚠ **The numbers are not extracted.** See §5.

### 2.7 Edge cases the original explicitly handles

**IDENTIFIER.** `hasMovingPlatformInAnyPortal`, `isThereAnythingInPortal`,
`doesItDestroyPortals`, `teleportToFirstStaticPortal`, `teleportToOther`,
`computePortalFinalPos`, `g_portalCollisionRadius`. Also
`addClearPortalsButton` / `removeClearPortalsButton` — the player can clear placed portals.

**DECODED** (`hasMovingPlatformInAnyPortal`, planning step 136). **Nothing goes through a
placed pair while a moving platform or a lift is at any placed portal.** `teleportToOther` asks
it before it moves anything, and it is true while an entity named `moving_platform`, `lift` or
`lift.ent` reaches within 0.4 x the portal's 14 u (5.6 u) of a placed portal's point, by the
entity's collision box about its own point with its corners cut at 45 degrees. The traveller
waits in its portal and goes as soon as the platform has moved on. Static portals never ask.

**IDENTIFIER.** Audio hooks: `playPortalCreatedSound`, `playPortalLaunchSound`,
`playPortalKilledSound`, `playPortalFailedSound`. A "failed" sound implies placement can be
rejected — consistent with the antiportal fields.

---

## 3. Level objectives

**DATA.** Every one of the 32 chapter-1 scenes contains exactly one `main_char`, one
`camera_start`, one `max` (bounds) and one `door.ent` (exit).

**DATA.** 114 crystals across chapter 1 (106 `crystal.ent` + 8 inline `crystal`), in 28 of
32 levels. **IDENTIFIER.** `crystalCounter`, `_numCrystals`, `_maxCrystals`,
`getCrystalCount`, `crystalPicker`, `crystal_pick.ent`, `crystal_vanish.ent`.

⚠ **UNRESOLVED:** whether the exit requires all crystals. No identifier settles it.
Modelled as a data switch (`portals.json` → `level.exit_requires_all_crystals`, default
`false`).

**IDENTIFIER.** `DoorLockedChooser`, and an achievement "Close all doors before finishing" —
doors have locked states, and `door_lift` entities carry `switchIdx`/`idx` custom data
pairing them with `button` entities. Not implemented in Enhanced yet.

**IDENTIFIER.** `death_area.ent` kills on contact (`ETHBeginContactCallback_death_area`,
`sensor=1`). Only one placement in chapter 1; far more in later worlds.
**DECODED** (planning step 136): the callback sets `hp` 0 on a character whose body meets any part
of the sensor's own polygon (level5's 60 x 16 octagon, level25a's 52 x 16), and the only
character is the player (`isCharacter`, set by `GameCharacter`, built only for the main
character). The levels place two, on 1-06 and 2-26 - not "far more".

**MEASURED (§7.3).** Progression is a linear high-water mark — two persisted integers
`lastWorldAllowed`/`lastLevelAllowed`. Not star-gated; a skip-level button exists and
medals are cosmetic. A separate free-vs-paid purchase gate exists and is out of scope.

**DATA.** A "golden score" is a per-level **maximum portal count**; seven levels across the
game ship a threshold of `0`, which only parses as "finish using zero portals".

---

## 4. Camera

**IDENTIFIER.** `CameraController`, `AutoCameraController` (`assets/AutoCameraController.angelscript`),
`ICameraController`, `clampCameraPos`, `readCameraStartPos`, `cameraHoldTime`,
`cameraHoldElapsedTime`, `dragMoveCamera`, `autoCamera`.

Two readings supported: the camera follows automatically, **and** the player can drag it to
look around (`dragMoveCamera`) — which makes sense for a puzzle game where you must survey
the level before placing portals. `cameraHoldTime` implies a hold-then-resume behaviour.

**DATA.** Some levels carry `minX`/`maxX`/`minY`/`maxY` custom data — explicit camera
clamps, matching `clampCameraPos`.

⚠ All camera *durations* are **GUESS**.

---

## 5. What is still unknown, and the one way to get it

Every numeric constant. The structure above is well established; the numbers are not.

`tools/asbc/` now decodes to byte **13,028 of 523,826** — up from 239 — covering the
header, all 3 enums, all **132** class declarations, 5 funcdefs, 9 interfaces and **6
complete classes with every method's decoded bytecode**. It then desynchronises inside
class `Button`.

Four real bugs were found and fixed getting there, each of which had produced a
confident-looking wrong parse: `ttIdentifier` is 5 not 14 (an unnumbered C enum, so its
value is its ordinal); `asFUNC_SCRIPT` is 1 not 2; **every** function reads a trailing
script-section name when debug info is present, not just script functions; and class
phase 2 skips the destructor and constructors entirely for interfaces.

It is NOT the pointer width — 32- and 64-bit runs fail at the identical byte, which they
could not if a pointer-sized instruction had already been misread.

The original stopping point was byte 239. Reaching the named constants means reaching
`scriptGlobals` (`as_restore.cpp:1349`), which sits behind `classTypes`, funcdefs,
interface methods, class methods, class properties and typedefs — so it needs ports of
`ReadObjectTypeDeclaration` phase 2, `ReadDataType`, `ReadFunctionSignature` and
`ReadFunction`.

And even arriving there is not enough: `ReadGlobalProperty` stores a name, a namespace, a
type and an **initialisation function** — not a value. `FIRST_PORTAL_MIN_TIME = x` is
computed by bytecode, so the value is an immediate operand inside that function and needs
`ReadByteCode` (`:1621`) plus instruction decoding.

**Feasible, and clearly the right path — but a substantial piece of work, not a quick win.**
The clean 239-byte decode already proves the format matches this AngelScript version
(2.26.1), which was the main risk.

Until then: values live in `data/*.json` marked `_guess`, and
`MILESTONE_1.md` §3 stays open.

---

## 6. Entity behaviours — the authoritative test

**DECODED.** The engine builds each entity's script hook as
`"ETHCallback_" + RemoveExtension(entityName)`. So **an entity has behaviour if and only
if a matching `ETHCallback_*` symbol exists in `android_game.bin`.** 96 do.

That turns "which of these 178 entities actually do something?" from judgement into an
exact byte search — and it immediately rules three out. `moving_platform_single.ent`
carries a `moveStride`, and inline `platform` and `anchor` carry `speed`/`stride`, that
**nothing reads**. Dead copy-paste. Animating them would invent motion the original never
had; `verify_gameplay.gd` asserted they stayed inert (it went with the Godot runtime on
2026-09-19).

### 6.1 The `stride` disambiguation rule

**DATA.** Applied throughout the converter and the runtime:

| Condition | Meaning |
|---|---|
| paired with a `speed` float | `stride` is a **distance in pixels** |
| a bare uint, no `speed` | `stride` is a **duration in milliseconds** |

Corroborated twice: `carranca` carries both `stride` and `startStride` (an unambiguous
initial delay), and a `button`'s 300 on a 26×16 body cannot be a distance.

### 6.2 The destruction economy

**DATA.** Four flags, read through one predicate the binary names
`isBreakableOrExplosiveOrBurnable`:

| Flag | Only on | Effect |
|---|---|---|
| `demolisher` | rolling stones (27/27) | destroys anything `breakable` on contact |
| `explosive` | barrel bombs (31/31) | detonates and chains — **on request, not on contact** |
| `burnable` | crates, shock agents (65) | burns for a duration, then is **removed entirely** |
| `breakable` | walls, carrancas | marks what can be smashed |

Almost all of it is also `teleportable`, which is the whole puzzle: **route a destructive
object onto a destructible one through a portal pair.** Burning removes collision too, so
torching a crate deletes a platform.

Notes that change implementation:
- A rolling stone is **not** a touch-kill hazard — in level 1-9 the player spawns 58 px
  from one on the same platform.
- A barrel bomb is **inert on contact**: no `ETHBeginContactCallback_barrel_bomb` exists,
  only a per-frame callback whose one local is `brightness` — a "live bomb" emissive tell.
- A shock field's ring is **drawn** at radius `areaRadius`; hiding it makes it unfair.
- `blink` is **purely cosmetic** and must never gate anything.

**DECODED** (`ETHCallback_fire_agent`, planning step 134). A fire agent reaches whatever stands
within its sprite's width, 32 u, centre to centre, and runs four tests on each: it asks a
barrel to go off, burns what is burnable and not yet burned, kills a character (the player
alone), and turns a portal shot still in flight into a fireball (`burnProjectile`: the shot is
deleted and a fireball goes on from where it was, along its heading). So **a shot fired
through a flame opens no portal**: it becomes a fireball that burns the first crate or sets
off the first barrel it meets, and cannot hurt the player. A minion is burnable and no
character: a flame sets it alight and it destroys itself on the next frame, dropping what it
carried.

### 6.3 Switches, keys and locks

**DATA.** `button.idx` ↔ `door_lift.switchIdx` — identical multisets in all 37 levels that
use them, zero mismatches across 46 buttons and 46 doors, never more than one button per
`(level, idx)`. Not a latch: the original counts touching entities and has both begin- and
end-contact callbacks, so a plate releases the instant the last body leaves.

**DECODED** (`ETHCallback_door_lift`, planning step 133). **The door it opens is the latch.**
It rises 0.8 × its height (102.4 u for `metal_door.png`'s 128), eased by smoothEnd over its
`stride`, and a status (0 shut, 1 opening, 2 open, −1 closing) lets its button start a rise
only from shut and a fall only from fully raised: a one-frame press lifts it all the way,
and a release lowers it all the way once it is up. A door closing onto the player kills it
(`ETHBeginContactCallback_door_lift`: the player's centre more than 0.55 × the door's height
below the door's centre and within 0.55 × its width across).

Keys pair with keyholes and locked doors **by colour** (yellow 33 / blue 6 / red 2 per
role, identical multisets per level) — **never by proximity**, since a keyhole and its
door can sit 200–340 px apart.

### 6.4 Minions

**DATA.** Worlds 3–4 only; 62 spawners, none in chapters 1–2. **No enemy is ever placed
directly in a scene** — every one is script-spawned from an invisible marker.
`waypointName` is a **PREFIX**: `wayA` patrols `wayA0` ↔ `wayA1`, holding `holdTime`
**milliseconds** at each. A lone `wayX0` with a sentinel hold of 9999999 is an authored
**stationary guard**, not missing data.

Minions carry `breakable`, `burnable` **and** `teleportable`: they are puzzle objects as
much as threats, crushable or portallable onto `enemy_killer` — the invisible kill floor
below every world-3 level. That is what the "Kill all minions" achievements are for.

### 6.5 Hinges

**DATA.** `<Joints>` is nested **inside `<Collision>`**, alongside `<Polygon>` and
`<Compound>`. The general rule in this format: *every ENML payload lives under the
collision root* — there is no counter-example, and looking one level higher returns
nothing while raising no error.

**Angles are RADIANS**, now settled from data rather than inferred: the shipped `±1.57075`
is `±π/2`. Only 10 instances exist, all in worlds 2–4.

### 6.6 Level properties

**DATA.** The `properties` entity, in 102 of 128 levels. Chapter 4 is where they matter:

| Levels | Flag | Effect |
|---|---|---|
| 4-2 … 4-19 | `no_gravity` | **the zero-gravity chapter.** Applies to EVERY dynamic body, not just the player — a crate falling under normal gravity shoves the player out of the level |
| 4-20 … 4-32 | `darkest` | the unlit levels the "Alone in the dark" achievements name |
| 4-1, 4-19, 4-32 | `no_music` | silent by design |

### 6.7 Bosses

**DATA.** Exactly one per chapter, always level 32: `adder` carrying `entity: beholder.ent`
(1-32), `dragon.ent` (2-32), `ghost_pos` (3-32), `dark_dragon_spawn` (4-32).

⚠ **Only the structure is implemented.** That a boss exists, is killable, and ends the
level is supported by data and achievement text. The **attack patterns are not** — each is
a bespoke AngelScript class whose logic is in bytecode we can name but not read
(`beholder_spike`, `dragon_claw`, `enemy_shoot` are identifiers, not decoded behaviour).
Inventing four boss fights would be exactly the plausible-but-wrong content §3 warns about.
