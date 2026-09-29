# Magic Portals parity — backlog

State at 2026-09-29 (first written 2026-09-15). What remains to make the **Supersonic port** (`game/` here since 2026-09-19; before, `Desktop/Supersonic-Engine/games/magicportals`)
match the original, in the order it should be done. Every spec named here lives under
`Magic-Portals-Remake/out/parity/specs/` (gitignored, local only — re-creatable from the reference library).

How to work on any item: `docs/parity.md` (rig, captures), `tools/parity/` (emu, capture_original, capture_screens,
markers, locate, compare, frames, port_build.bat). Port captures: Bash only, from this repository's root,
`build/game/MagicPortals.exe --level <name> --window 1280x720 --fixed-step --frames N --screenshot <absolute png>`.
Windows Smart App Control sometimes blocks a freshly linked exe ("Not Run" / "Permission denied" / "An Application Control
policy has blocked this file"): delete the exe and relink. Touching the source is not enough when the rebuild produces the
same bytes. **Every blocked launch shows Ivan a Windows notification: never loop relink/ctest retries** — run the
suites a change touches once each, and report what stayed blocked.

---

## 0. Housekeeping left from this session

- [x] Merge branch `ui2-screens` into `main` — done 2026-09-16 (e6a3cca, pushed): its steps became 50-54, ctest 118/118,
      the worktree and branch removed. See the planning doc's step 54 merge note.
- [x] Merge branch `visuals-b` into `main` — done 2026-09-19 (`e81c21b`; SE main pushed at `7281ca5`): its steps 80-85
      became 68-73 (80→68, 81→69, 82→70, 83→71, 84→72, 85→73; the branch's own commit messages keep 80-85). The one
      tier gap the merge found (the gravity well's ring still loading the 1x `white_ring.png`) is closed by integration
      step 74 (`591e143`, `7281ca5`). Worktree `Supersonic-Engine-vis2` and branch `visuals-b` removed. Build current
      (ninja dry run: no work to do), 0 warnings; merge suites 0 failures; full ctest (with `test_mp_lighting` left
      out) 119 of 121 passed and 0 failed, and `test_mp_levels`, one of the two Smart App Control refusals, passed when run
      directly after one relink (128 checks, 0 failures). Not run, because Smart App Control blocked the launch:
      `test_mp_lighting` (last run 503/0 at step 66) and `test_mp_geometry` (neither track touched it). Both binaries
      are current; they were never launched. See step 73's merge note.
- [x] Merge branch `visuals-c` into MPR `main` — done 2026-09-21 (`a5c9e23`, pushed; `2bf4755..a5c9e23`
      fast-forward). Its steps 90-96 became 81-87 (the branch's own commit messages keep 90-96); the merge note is
      planning step 88. Three files conflicted and each kept both tracks' behaviour. Full ctest 45 of 45 passed, 0
      failed, 17.7 s; the 128-level f420 sweep 128 of 128 rc=0, 0 validation messages; no Smart App Control refusal
      and nothing relinked. Worktree `Magic-Portals-Remake-vis3` and branch `visuals-c` removed. The engine pin did
      not move (`4bfcf67` on both tracks, and it is the engine's `origin/main`). See §2's table and merge note.
      **One inherited caution recorded there**: the merge's own ctest log showed 45/45 in **29,660 s**, 29,618 of
      them in `test_mp_zerog`, because the laptop slept mid-run; the re-run puts that suite at 0.29 s.
- [x] Merge branch `visuals-d` into MPR `main` — done 2026-09-29 (`fc243c6`, parents `848ed92` and `93e3641`, plus
      the docs-only `d1834c4`; pushed, `origin/main` = `d1834c4`). Track B's steps 120-131 became 100-111 in the order
      they were committed (the branch's own commit messages keep 120-131); the merge note is planning step 112, whose
      table is the renumbering map. 14 files conflicted and each kept both tracks' behaviour; three joins git merged as
      text broke in code and were fixed there (`EffectReport`'s two `angleDeg`, the `Minion::entity` rename, the dark
      dragon's path). The merge's own gates: build 0 warnings; the 128-level f420 sweep (`merge_d/sweep`, on
      `MagicPortals.exe` `8b5ab6d2`) 128 of 128 rc=0, validation active and silent, 66 levels neither track moved equal
      to the merge base, 27 equal to track A, 30 equal to track B, and the 5 both moved attributed (2-24's 55 px are
      R9's barrel under track B's stream shift); 22 suites run directly once each, 0 failures (`test_mp_layer` 2901);
      the full ctest once, **40 of 44 passed, 0 failed, 4 Not Run** (Smart App Control). **9 Smart App Control
      refusals.** An independent check (`merge_d/verify1/`) found the build current (a dry run: no work to do), ran 7
      suites once each with 0 failures (`test_mp_layer` 2901, `test_mp_sprites` 1354, `test_mp_effects` 706,
      `test_mp_darkdragon` 216, `test_mp_camera` 212, `test_mp_play` 205, `test_mp_levels` 129), and its own sweep on
      `8b5ab6d2` matched `merge_d/sweep`'s md5 list 128 of 128. The engine pin did not move: `4bfcf67` on both
      tracks, which is on the engine's `origin/main` history but no longer its tip (`6cb4036`, another game's backend
      and renderer work); no engine gap came up, so nothing was bumped. Worktree `Magic-Portals-Remake-vis4` and branch
      `visuals-d` removed. **The standing sweep baseline is `merge_d/sweep`.**
- [ ] **`test_mp_launchers` and `test_mp_keys` have not run on the merged binaries** (step 112). The merge relinked
      both; Smart App Control refused each twice (the first launch or the ctest, then the one relink), and the merge's
      independent check did not launch them. Their last runs: `test_mp_launchers` 105 on track B (step 109; 98 on
      track A at step 95) and `test_mp_keys` 124 on track A (step 98; track B did not run it), 0 failures each. Next:
      the next step that relinks them launches each once and records the count.
- [ ] Planning doc Step 45 (G3) still reports pre-amendment verdicts; add a note citing `design_port.md` §10 (gate
      amendments) and relabel the two attribution rows as the gates.
- [ ] `test_mp_launchers`: add a refusal case for a missing/bad throwable emissive.
- [ ] `test_jobs` is timing-flaky (failed on thread distribution on 2026-09-15 and again under load on 2026-09-16) —
      unrelated, but make it robust.
- [ ] GCC build never run for any of this session's work — run it once (the repo claims zero warnings on both).
- [x] MPR `DEVLOG.md`: entries for 2026-09-15 and 2026-09-16 appended.
- [ ] MPR `docs/permission.md` is still the unfilled stub — Ivan to paste the grant.
- [ ] MPR converter low items: `_i()` should parse `int(float(raw))` (TinyXML `%d` reads "1.000000" as 1);
      the CLI lighting report is not asserted by a test.
- [x] Godot runtime in MPR removed 2026-09-19: `game/project.godot`, `game/scenes/`, `game/scripts/`, `tools/godot.bat`,
      and locally the `game/.godot` cache and the `game/assets` copies. Nothing had to move: `game/data/*.json` and the
      converter's `out/` stay where they are, and the port reads them through the paths baked into SE's CMake cache.
- [ ] The port still cites the removed Godot files as provenance: 77 lines in 37 files under `game/` (SE's
      `games/magicportals` when this was counted) and `tests/test_mp_*` (e.g. `portal_system.gd:230-263` in
      `sim/Portal.hpp`, `verify_gameplay.gd:259` in `tests/test_mp_play.cpp`, `project.godot` in `sim/Units.hpp:27` and `data/view.json:18`, the gone local
      `game/assets/levels` copy in `tests/test_mp_levels.cpp:5`), and 29 lines in its three Magic Portals planning docs.
      None reads a file. The scripts stay readable at MPR `368d46c`; a port step should pin that commit where they are
      cited, or reword the lines. See MPR `DEVLOG.md`, session 18.

## 1. Lighting (design: `lighting/design_port.md`, gates amended in its §10)

Done and pushed: G1 reader, E0 descriptor sets, E1 scene encoding, G2 display-encoded scene, G3 ambient/emissive,
E2 overlay map, G4 lightmaps (1-01 mean_abs vs original 29.7 → 15.9), E3 2D point lights (step 48), G5 lights placed,
the player and the shot lit, halos drawn (step 49; 1-01 15.86 → 12.72), G6 the 5/6/5 surface and the runtime bake
after a torch is lit (step 55; 4-22 noLM_E<1 vs original 0.95 → 0.13). The design's plan is complete.

- [ ] G6 leftovers: the screen overlay (HUD, pause, menus) is not quantised. ~~The torch light's z is a `_guess`;
      `light_from_projectile.ent`'s own sprite and flame are not built; no capture of 4-22 lit (footage L1 missing).~~
      **Closed by 11.2 (step 110, `e30489a`)**: the lit torch is `light_from_projectile.ent` itself, added at the
      torch's own `GetPosition()` (`ETHCallback_light_off` #69-101) - `torch_small.png` on its pivot, 12 `fire.png`, its
      own `<Light>` at (0, -12, 24) - and `art.json`'s `torch_light` copy, with its z `_guess`, is gone; 4-22 lit is
      captured on L1's tap and compared with footage `L1_light_4-22` (gate 3). The overlay item stays open.
- [ ] **Found by 11.2 (step 110): the lit room near the torch reads further from L1 than before.** With the halo now
      at the light's point (step 82's 12 u, closed) and at its flame's live share, both as decoded
      (`ETHRenderEntity.cpp:354-388`), 4-22 lit reads 7-19 luma further BELOW the footage within 60-200 px of the torch
      and down its column than step 109's build did (mean luma, footage / before / after: rings 60-100 px 168.5 / 140.9
      / 132.6, column 100-130 px below 135.4 / 109.0 / 90.5, L1 section 3.1's pillar 83.5 / 60.7 / 53.3). The halo
      fix is correct by the code and does NOT improve this comparison. The knob is the global `lighting.json`
      `halo_brightness_scale` 0.6 `_guess` (the G5 row below); not retuned, because it moves every halo on every level.
      Next: with G5's row.
- [ ] **Found by 11.2's second verification (step 110), older than it: the player stays lit after a put-out.** On
      4-29 the player's box is mean RGB (7.2, 4.2, 8.7) before the lighting and (25.7, 14.8, 31.1) at f300 and f400
      after the put-out, where L1 returns him to (0.8, 0.2-0.3, 0.2-0.3); step 109's build gives the same, so step 110
      did not bring it (`11.2/commit/player_box.txt`). Next: the put-out's ambient and the run-time bake (steps 45, 55).
- [ ] **G5's missed gates** (step 49, never independently verified — the session that built it died before its verify):
      1-09 `LM_E<1_halo` 6.17 against ≤ 6.0 (no single halo scale passes both LM_E<1 rows with 1x art); the orange R−B
      excess beside the 1-01 torch at 60/90/150 u is 47/18/18 below the original (asked ±15); "red rises monotonically"
      fails at one frame where the formula's prediction dips too (a gate-definition question).
      `lighting.json halo_brightness_scale` 0.6 is a `_guess` tuned to one frame's flame.

Open after the plan (design §8):
- [ ] **Script-driven emissive** — the largest remaining lighting error in chapters 2-4. Decode `ignore_emissive`,
      `door_emissive`, the crate callbacks, the player spawn and three background functions with `tools/asbc`.
- [ ] **Camera at 1-13**: port offset (0,−10) px against the original; the lighting gate refuses this level until fixed.
- [ ] Halo magnitude: formula × particle ratio; the fit's ×0.5 on one entity may need a `_guess` scale.
- [ ] Vertical entities (`vPixelLight`, XZ plane) not ported (10 `light_off` instances).
- [ ] Per-light 3D cull (`range + max side`) not ported; light existence by visible bucket not ported.
- [ ] Footage to record: L1 4-22 torch lit then put out (3 s each); L2 1-2 shot along a wall; L3 a W2 explosion near crates;
      L4 4-26 or 4-29 — light the torch, then watch the wall it takes away (no clip puts a torch out on a level that
      places a light wall, so step 80's ghost is the decode's alone; §5's footage list wants this row too).
      **L1 is recorded** (`out/parity/footage/INDEX.md`: `L1_light_4-22`, and `L1_out_4-29`, a torch lit and put out on
      a wall level with its wall's shake), and it covers L4's purpose: 11.2 (step 110, gate 7) confirmed step 80's ghost
      against it - position, alpha pulse and scale pulse, the scale pulse's amplitude not resolved. L2 and L3 are still
      to record.

## 2. Entity visuals (plan: `visuals/plan_port.md`, ranked)

**The batch order, `visuals/briefs/00_order.md` §7 — batches 0-4 closed 2026-09-19, batches 5-7 closed 2026-09-21,
batches 8-13 closed 2026-09-29.** None shelved, none skipped, none blocked. Every commit named here is an ancestor of
MPR `main` `d1834c4`, which is pushed. "Step" is `docs/planning/2026-09-11-magic-portals-remaster.md`'s number as it
stands NOW: batches 0-4 were renumbered by the `visuals-b` merge (their own commit messages still say 80-85), batch
6-7's seven steps were renumbered 90-96 → 81-87 by the `visuals-c` merge (`a5c9e23`, step 88; their commit messages
still say 90-96), and track B's twelve steps of the last run were renumbered 120-131 → 100-111 by the `visuals-d` merge
(`fc243c6`, step 112; their commit messages still say 120-131). That run also added four steps §7 does not list, each
filed under its own id: **T.1** (a scheduled `--tap` is a real press), **Q.1** and **Q.2** (the earthquake, which
13.3 and 8.13's call-site rows needed) and **10.1b** (10.1 re-measured, no change). "Recorded deviations" are the
step's own RECORDED DEVIATIONS section. Where they have rows below, those rows stay open. **Every step of §7 is now
built; what is left is under the table.**
**3.4 is out of batch order on purpose**: §7 had dropped it for "no data, no footage, no decode", and §8.13 proposed
it back from decode D10 after batch 3 had closed, so it is built during batch 5's run and filed in its own place here.
**13.2 and 13.4 are out of batch order on purpose too**: §8.13 named them ready from decode D5 - 13.2's sheet swap,
glow and bob are one script gate, and 13.4's ghost is the other half of the same decode - so both are built during
batch 5's run and filed last here.

| Order | What | Outcome | Commit | Step |
|---|---|---|---|---|
| 0.1 | JPEG size probe | committed | `12a10f3` | 60 |
| 0.2 | `sim/Tiers`, one cut rule | committed | `823e3d7` | 61 |
| 1.1 | facing, stride, column carry-over | committed with six recorded deviations (the six "Step 62" rows below) | `441ce6e` | 62 |
| 1.2 | space suit on the 18 `no_gravity` levels | committed with three recorded deviations, none a failing gate row: the brief's "zero-g never walks" decoded otherwise (the column turns while standing), the `no_gravity_motion` block, the hover is 1.2 screen px | `a0207e1` | 63 |
| 1.3 | pushing rows | committed with five recorded deviations: the wall gate ran on 1-22's wall (level0 has none on its right); row 2 appears the step after the release; the ray as decoded, not as briefed; data shape; the airborne aim follows the port's `air_control` `_guess` | `9c454a7` | 64 |
| 2.1 | converter copies tier files, writes a manifest | committed with five recorded deviations, all on code and file layout: `tiers.py`, manifest shape, densities read from `app.enml`, a tier file with no 1x file copied, staging run | MPR `e9611e4`, SE `5de8509` | 65 |
| 2.2 | level art through the resolver | committed with seven recorded deviations; the bare-ground row is met as amended and re-baselined at 5,312 px | `c404615` | 66 |
| 2.3 | port art and particles through the resolver | committed with three recorded deviations: a cut particle sheet is still split evenly (`tesla_shock_black_bg`, `shock_strike`; for 13.1); the UI fold-in was not taken; the level1c pin was rewritten. **The split sheets are closed**: `shock_strike.png` by 10.4 (step 108: an effect's emitter on it samples `int(512 / 5)` = 102 units a column, `SetupSpriteRects`' whole stride) and `tesla_shock_black_bg.png` by 13.1 (step 111: one whole-unit cut rule, `WholeUnitCut`, for the level's own emitters, the effects and the loading screen alike; 51 of 256, `EmitterReport::cellPx` 51 x 32) | `494f6e2` | 67 |
| 3.1 | timer dial, no fade | committed | `0be2725` | 68 |
| 3.2 | crystal and key bob, `sim/Motion` | committed | `5ba5b86` | 69 |
| 3.3 | sway, alpha 0.55, dashed-circle spin | committed | `a32d356` | 70 |
| 3.4 | the button sinks six units under what stands on it | committed with four recorded deviations, no gate row unmet (6.0 u wanted, 5.957 measured; 4.24 wanted at 150 ms, 4.285 measured): 6 `.esc` units are taken as 6 port units (the 0.976 ratio would make the original's own sink 5.86 u, and `movers.json` says not to tighten the gates on it); the >= 300 ms row rests on a box capture, because 1-31's floor picture buries the button at full press; the bucket-visibility clock is not built (D10 records it as mechanism only); the port's sensor is still the remake's 16 px fallback against the original's 26 x 16. §8.13's "a button branch in `syncSprites`" is **superseded by R21**, which moves the body the picture already hangs on. **Gate level substituted**: level2a's button cannot be stood on (walled spawn pocket, reached only with portals), so 1-31 `level30`'s `button_975` is measured instead - same node y 220 over a floor whose top is 224. **Sweep baseline**: `5.2/verify1/sweep` still stands, EXCEPT `3-15 level14b`, now `7a3553fa` - a knight of step 76 stands on `button_1666` and sinks it. The PORT-WIDE interpolation row below was found verifying this step | `0f0142a` | 78 |
| 4.1 | antiportal drawn at 64×scale | committed with one recorded deviation (the 2-26 edge scan), closed by 4.2 | `bbb3c78` | 71 |
| 4.2 | blink and spin | committed with five recorded deviations (the four "Step 72" rows and the PORT-WIDE clock row below) | `5ae1b6d`, `5a544ac` | 72 |
| 4.3 | gravity-well green ring | committed with two recorded deviations, neither a failing gate row: the `test_mp_wells` gate is met across two suites, and the ring exists from the first frame | `6949cc3` | 73 |
| 5.1 | minions and knights on their sheets | committed with deviations, and **two sim changes applied under no adopted ruling - proposed R24 (the patrol's arrival point) and R25 (a carrier's point), THE OWNER'S TO RULE** (the "Step 76" rows below). Two gate rows NOT MET as written and recorded, not tuned: the non-regression sweep on 10 of the 128 f420 frames, where the repaired patrols carry keys, press 3-15's button and weigh 3-22's seesaw (each checked against the original's own frame and moving toward it), and G5a-5's lit R on one widened-series detection that sits on the line. Three deviations left to their owners: the lone guards' coast, the player's carrier point, a shock diamond's prey point | `9d25691` | 76 |
| 5.2 | the 2-32 dragon and its claw follow the sim | committed with deviations. One gate row NOT MET as written and recorded, not tuned: `test_mp_dragon` "stays at 151" reads **230 checks, 0 failures** - **R19** and 8.13 ("fireball direction pending R19") rewrite the aim the suite pinned, and the diff removes no assertion. Two deviations left to the owner (the "Step 77" rows below): two clock bases where the original keeps one, and the `2-32_h2.8`-versus-f168 flight-timing gap, which predates the step | `cbbf169` | 77 |
| 6.1 | the shot as its script adds it, and the owner-scale halo rule | committed, no recorded deviation and no gate row unmet. The portal shot is drawn as `addProjectile` and `ETHCallback_projectile` draw it: its 64 u cell at the script's `Scale(0.8)` = **51.2 u**, turned every frame so the bolt's head leads along its flight, standing on its pivot 3.2 u behind the shot's point, and stepped by ONE global 30 ms timer (the script's `_projectileFrameTimer`) rather than the per-shot 15 fps `_guess` it had. `Lighting::OwnerScaled` carries the same scale into its light: halo 50 -> 40 u, range 70 -> 56, height -12 -> -9.6. A level placement's halo stays unscaled (K7) and the 128 f420 frames are byte-identical to the baseline; step 49's "portal shot on level1" gate, silently disarmed by step 52's popup, is re-armed and passes on all three rows; `art.json`'s shot has no `_guess` left. **K14 is left open and the merge did not close it - the row below** | `e620810` | 81 |
| 6.2 | the effects entity reader and the pure pieces | committed, no recorded deviation and no gate row unmet. `Art::LoadEnt` reads any of the original's 190 `.ent` files as `ETHEntityProperties::ReadFromXMLFile` does - the engine's own default where a file states nothing, a refusal where it states something unreadable - into an `Art::Entity`; `Particles::Scaled` and `Particles::Finished` are the engine's system scale and `m_finished`; `sim/Effects` and `effects.json` hold the teleport's three `AddEntity` calls and the arithmetic of where they land. Nothing draws with any of it yet (6.3 wires it), so the 128-level f420 sweep is byte-identical. New suite `test_mp_effects`, 78 checks. **Two engine facts found here and owed to later steps**: a lit torch's halo is drawn **12 u low** (`light_from_projectile.ent`'s light sits at (0, -12, 24) and `DrawHalo` uses that (x, y), where `art.json torch_light.light.halo_offset` is (0, 0)) - for 00_order 7f-5 or a follow-up, with a lit-torch capture; and `LoadEnt` drops each file's `type` and `layerDepth`, which is exactly what 13.3's `black_bar` (type 5, layerDepth 1.0, drawn in front of all) and `light_off.ent`'s INDIVIDUAL_OFFSET particle sorting need. **Both closed in the batches 8-13 run**: `LoadEnt` reads `type` and `layerDepth` since 13.3 (step 102; type 0 on 188 files, 2 on `light_off.ent`, 5 on `black_bar.ent`; `ET_VERTICAL`'s drawing and `INDIVIDUAL_OFFSET` sorting stay with the "Order 8.4" vertical-shift row), and the lit torch's halo stands at its light's (0, -12) since 11.2 (step 110, by the code: `haloOffset` from the light's own `<Position>`, as `DrawHalo` draws it) - which moves the lit room FURTHER from L1, §1's row | `449fb4c` | 82 |
| 6.3 | the effects runtime, a stream of its own (K8) and one between-slot table (K13) | committed, no recorded deviation and no gate row unmet; an independent check failed it on one row and the fix is in the commit. `spawnEffect`/`updateEffects`/`unloadEffects` add an entity of the original's by name - its picture on its pivot, its light and halo at the owner's scale, one pooled emitter per `<ParticleSystem>` - and free it the way the engine frees it; **`m_effectRandom`** keeps every non-placement draw off the level's placement stream (K8); and `slotAfter` is promoted to a member over ONE table of between-slot fractions with a stated order at equal depth (K13), which every user in the tree now reads. Every existing fraction is carried over exactly, so the step moves no pixel: the 128-level f420 sweep is byte-identical and `test_mp_layer` reads 1,339 checks. **A stated exception to K8's wording**: the loading screen's entities stay on the placement stream. **What the renderer does with an exact tie is not pinned** - the table STATES the order and the suite pins the table, and two pairs can still tie (a dial with a ring; the minion/escort pair step 76's census found), so a step that needs a tie broken on the screen gives those rows fractions of their own. Six Smart App Control refusals while it was built, and a seventh in the second independent check | `d36abef` | 83 |
| 7.1 | the red eyes the character adds on every level | committed, no recorded deviation and no gate row unmet - the first thing the effects runtime actually draws. `MainCharacter`'s constructor adds `red_eyes.ent` at the character and `updateEyes` puts it on the face every frame: 7 u to the side the DRAWN CELL faces, 14.25 u above the character's point (15.65 where gravity is off), 1 u in front of it, hidden with the character; 12 red specks living 120 +- 100 ms in world space, so the eyes glimmer and a walk leaves a short wake. They draw on `m_effectRandom`, so no particle of any level's own emitters moves: of the 128 f420 frames **123 change 87-97 px inside a 4.3 u box at the face, 4 do not change at all**, and one zero-g drift leaves two lone trail pixels. E7's gate change is applied (peak R >= 240 dropped for the MEAN R inside the mask: an unmasked frame-8 cell reaches R 245 on hood highlights alone). The hold-left gate's whole-cell locator failed and was re-read rather than tuned. `test_mp_effects` 179 checks. One Smart App Control refusal, one relink, no loop | `602b3af` | 84 |
| 7.2 | the fireball, and the small and large explosions | committed **with seven recorded deviations and THREE gate rows NOT MET as written, under the owner's delegated decision, none of them tuned**. Two independent checks failed the step on its RECORD (not on its build); a third re-measured every gate and reproduced every number, and it was committed on that third round. NOT MET: (1) the light's **R-excess tail falls to 10 at 40.0 u against 27.6 +- 6 u**, 6.4 u past the band, with the same scripts reading the ORIGINAL's own pair at the two numbers the band is made of - the knob is the falloff's SHAPE, not the decoded range 50; (2) the **core is MET on the brief's licensed sample** (8.94 u, RGB (242.0, 201.6, 145.2)) **and NOT MET as a population** - 300 readings over a 210-frame series give 4.12-9.95 u, mean 6.39, **42.7 % under the 6.0 floor**, and only 54 % hold all three channels within +-20 (blue 59.7 %), against the original's 7.71 and 8.30 u by the same code; (3) **K8's byte-identity over the sweep** - 107 of 128 identical, and **20 of the 21 that changed redraw the PLAYER'S EYES: 1,499 px, of which 1,448 lie beyond the fireball's own box** on 19 levels. Also recorded: the comparison box is **60 u, derived from `fireball.ent`'s own `<Light>` (range 50 x `<Scale>` 1.2)**, not `system_7` G.2's 40 u, and that substitution is what turns 2-01 from a failure into a pass (22,611 px outside the box at 40 u). The four rows below stay open. **The K8 row carries an inherited ruling**: from 7.2 onward an effect step's K8 row means "the placement stream is untouched" (6.3's own case, 1,200 emitter values, 0 different) rather than "the frame is identical", so 7.3, 7.4 and 8.x do not each re-litigate it | `47564f2` | 85 |
| 7.3 | the teleport's three effects, and the depth a bare literal means | committed with four recorded deviations, no gate row unmet. The suck effect, the sparkles and the flash land where `effects.json`'s arithmetic puts them, and `Effects::Spawn::zScaled` separates a script's `g_scale.scale()` from a BARE literal, which is divided by R12's 720 / 256 = 2.8125 (the trap step 85 recorded for this step). **Deviations**: (1) the traveller's point is the port's body centre, 22 u against the original's 18.1 over the feet, so the sparkles and the suck direction sit about 4 u high on a teleporting player - exactly the carrier-point divergence §8.15 **R25** leaves with the player's owner, recorded rather than fixed because fixing it moves the player's own picture; (2) the three draw from the same `m_effectRandom` as the eyes, which is 7.2's inherited K8 reading and no worse; (3) **the suck effect's turn is the first non-zero `angleDeg` the runtime has ever drawn and no frame measures it** - `Effects::AngleDegrees` is pinned at 202.1286 deg in `test_mp_effects`, and the capture confirms the PLACEMENT only; (4) the queue's order within one tick is the port's, the original's callback order being undecoded (it can move the stream, never a placement). `test_mp_effects` 249, `test_mp_statics` 83, `test_mp_layer` 1419, 0 failures each. **A system's own `<StartPoint>` z is the same raw literal and is NOT converted here** - left for the K13 owner. **Corrected by 8.4 (step 92): it is NOT a raw literal** - `ETHParticleSystem::Scale` scales the start point's z with the entity (`ETHParticleSystem.cpp:27-33`), and every call site passes `g_scale.getScale()` or a multiple as the entity's scale, so the effects runtime (`Particles::Scaled`) already had it right and nothing moved; only its two comments changed. Two Smart App Control refusals, one relink each | `5161692` | 86 |
| 7.4 | the placed portal as the two entities it is | committed with seven recorded deviations and decisions, no gate row unmet; a verifier failed it on DEFECTS (not on gates), and the fix round is in the commit. A portal the player opens is no longer a white disc the port draws for it: it is `portal.ent` spawned whole through the effects runtime, with the spiral and the iris the entity carries, the colour `PortalManager` gives it, the 2 u hover its callback gives it, **twenty seconds to live (R14, applied as the original plays it)** and `portal_death.ent` where it dies - `m_portalQuads` is gone. **Decisions recorded**: the placed portal's TINT is applied although §4 gives it to no batch, because the kill fade multiplies exactly that colour; the hover and the perish are gameplay and are built in the sim; **the port's clock for both is its TICK where the original's is its rendered frame, which costs ONE FRAME on a teleport and nothing anywhere else** (every other kill path is called before the pops in the original too); the summon bubble does not follow its portal, which is what a separate `AddEntity` with no owner does. **Left open, and the step says it plainly: no automated evidence in this tree covers the other 125 levels' golden scores under a twenty-second portal** - the three `test_mp_play` replays are three levels' worth, and `test_mp_levels`' 128 checks read converted `.tscn` files and replay nothing. What would answer it is one scripted replay sweep of the 128 with `perish_ms` 20000 against 0. Twelve suites run once each, 0 failures (`test_mp_layer` 1463, `test_mp_start` 752, `test_mp_effects` 285); `test_mp_boss` and `test_mp_start` refused once each, one relink each | `5a60733` | 87 |
| 8.1 | the port reads `eth_particles`; `buildEmitters` prefers it, the `.ent` as fallback | committed with one recorded deviation, no gate row unmet - K10's reader half, committed before 8.2 lets the converter emit the key. `Lighting::Read` knows the key as it knows `eth_normal`: a `res://` path naming a file, only on a node that draws a sprite or owns a light, or the level's lighting is refused naming the line; the path goes into `Look::particles` and on to `DrawnSprite::particles`. `buildEmitters` draws the systems of the sidecar a node names - one that will not load, or holds none, is logged and NOT replaced by the `.ent` - and, with no key, its entity's `.ent` exactly as before (same `Load`, `Scale`, `MakePool` on `m_particleRandom`, same order). The sidecar is read under `m_paths.art`, its bitmaps still from the original's `particles/`. No converted level carries the key yet, so **the 128-level f420 sweep is 128 of 128 byte-identical to `merge_c/sweep`** (1-01 `e2da5131`), 128 rc=0, validation active and silent. **Gameplay: none.** A probe gave 1-08's blue light a sidecar cut from `level7.esc` (a check, not a gate): 4,404 px changed, all in one 74 x 75 px box on the lamp, orb pixels over 230 0 -> 2,299 against the original's 1,340-2,672. **Deviation**: the key is read into the `Look` rather than let through for `buildEmitters` to resolve again (row below). **Recorded, not built on**: K10's wording - an unknown `eth_` key draws the level UNLIT (`MagicPortalsLayer.cpp:435-440`), it does not refuse it; `scale` does. **Latent** (row below): the key is accepted on a light-only node that `buildEmitters` never visits. `test_mp_lighting` 533 (34 refusals, 31 before), `test_mp_start` 752, `test_mp_torch` 123, `test_mp_layer` 1593, once each, 0 failures; no Smart App Control refusal in 133 launches; engine untouched at `4bfcf67`. **Sweep baseline**: `8.1/after/sweep` | `25b0f89` | 89 |
| 8.2 | the converter writes each placement's own `<Particles>` as a sidecar and names it; the levels regenerated | committed with four recorded deviations (the four "Step 90" rows below), no gate row unmet - K10's converter half. `entity.py` reads `<Particles>` as `ETHEntityProperties.cpp:304-333` does (two systems at most, `particles > 0` by `sscanf("%d")`); `particles.py` writes each distinct block, canonical and on one line, as a UTF-16 `.ent` named by its SHA-1 under `assets/entities/inline_particles/` (`--copy-particles`); `tscn.py` appends `metadata/eth_particles` as the last line of every carrier that draws a sprite. Re-derived: **840 carriers, 897 systems (887 / 10), 24 blocks** - the plan's numbers. The HEAD converter first reproduced `out/` byte for byte; the new one's staging differs from it by 840 inserted key lines and 24 new files ONLY, and replaced `out/levels` and added the sidecars; `snap_pre8` untouched. `census9.py`: 840 of 840 keyed, each sidecar holding its carrier's inline systems. pytest 99 (14 new, two identical blocks -> one file), ruff clean. `test_mp_levels` `metaStrings` **16,934 -> 17,774** (re-pinned here, not in 8.3: deviation); 31 level-reading suites, 6,867 checks, 0 failures; `test_mp_lighting` "840 nodes name an eth_particles sidecar, 0 of them unreadable". **Sweep: 114 of 128 byte-identical** (1-01 `e2da5131`; all 110 levels whose carriers match their `.ent` or differ only in `StartPoint.z`), **14 changed, all predicted**: the `blue_light` orb on 1-05, 1-08 (= step 89's probe frame), 1-19, 1-22 (orb pixels over 230: 0 -> 2,442 / 2,299 / 1,877, a preview of 8.3's gate); the keys' and keyholes' `Color0.b` 0.7 on seven chapter-3 levels, every changed pixel a blue-only decrease; 3-14's keyhole spread; 4-01 and 4-19, where the rocket door (off view) adds systems whose draws shift the placement stream for the other emitters. 128 rc=0, validation silent, 840 placements drawn from sidecars. **Gameplay: none.** No Smart App Control refusal in 161 launches; `game/` and the engine (`4bfcf67`) untouched. **Deviations**: the `metaStrings` re-pin; the sidecar holds the systems the engine keeps; `--copy-particles` is its own flag (9.2 must pass it); the name hashes the block. An independent check passed every gate, its own sweep 128 of 128 byte-identical to the step's, and its two low findings went in at commit as text only: the hash-clash refusal holds within one converter run (a file an earlier `--world` run left under the same name is overwritten), and the spriteless-carrier report marks a PORT gap - the original draws such an entity's particles (`ETHSpriteEntity.cpp:106`). **Sweep baseline**: `8.2/after/sweep` | `db14d3a` | 90 |
| 8.3 | a placement draws only the systems its own `<Particles>` states; the `.ent` fallback dropped | committed with four recorded deviations (the four "Order 8.3 deviation" rows below), no gate row unmet - the end of batch 8's reader/converter/pins split (K10). `buildEmitters` reads no `.ent` for a node that names no sidecar (the engine reads a placement's own body, and the `.ent` only under a `<FileName>`, which none of the 4,072 placements has: `ETHEntityProperties.cpp:202-224`). Step 89's unlit question ruled: a level whose lighting read is refused draws no placed particles either, and its warning says so. `Lighting::Read` refuses `eth_particles` on a node that draws no sprite (35 refusals). Pinned in `test_mp_sprites`: **840 carriers, 897 systems (887 added / 10 mixed)**, 57 with two, 24 sidecars, and the four spot carriers (level7 #930 4 `hard_halo`; level1 #582 4 rings + a 1 ms iris; level7b #1335 two systems at `Color0.b` 0.7; level0c #2079 two systems); `test_mp_layer` proves the fallback gone on a copy of level0 with one static portal's key cut. Re-derived first: 840 / 897 / 887 / 10, each sidecar = its inline block 840 of 840, and **0 of 2,043 unkeyed sprite nodes with an `.ent` holding a system** - so **the sweep is 128 of 128 byte-identical to `8.2/after/sweep`** (the plan's "moves pixels" happened at 8.2: deviation). `bluelight.py` **2,442 / 2,299 / 1,877 >= 1,000** (plan's today 0 / 0 / 0; original 1,340-2,826); G5's four gate JSONs and `halo_g5`'s `halo.json` byte-identical; `metaStrings` 17,774. **Gameplay: none.** `test_mp_sprites` 1,060, `test_mp_lighting` 539, `test_mp_levels` 128, `test_mp_start` 752, `test_mp_torch` 123, `test_mp_layer` 1,603, 0 failures (the layer suite's first run failed once on the step's own new test, a pointer into a temporary; fixed). **Smart App Control refused 3 first launches** (`test_mp_start`, `test_mp_torch`, `MagicPortals.exe` `e09e8c33`), each relinked once and run; engine untouched at `4bfcf67`. An independent check passed every gate, its own sweep 128 of 128 byte-identical to the step's and its census 0 `<FileName>` in 4,065 level placements (4,072 in all 133 `.esc`); four of its six low findings went in at commit as text only (wording and citations), the fifth - the port draws the blue lamp's orb behind the bowl's rim - is left for 8.4 (row below), and the sixth - fixing the six dangling `test_mp_layer` reads in this commit - was not made, since it costs a relink (row below). **Sweep baseline**: `8.3/after/sweep` (its `capture.txt` also logs the 1-01 series, whose png is in `series/`, not `sweep/`: a sweep check reads 128 of 128 `sweep/` entries matching and that one extra entry "missing" there) | `e71b697` | 91 |
| 8.4 | a placement's system drawn at its owner's z plus its own `<StartPoint>` z | committed with six recorded deviations (the "Order 8.4" rows below; the fourth, the suite measured in the independent check rather than the build, is closed), no gate row unmet. `buildEmitters` lifts a level system whose start point z is not 0 to its owner's `eth_z` plus that z - after every picture at or below it and after the player where 1.42 is at or below it - through one new between-slot row, `Slot::LiftedSystems` (-1/2, spreading, so it ties with nothing; K13); every other system keeps its owner's slot. **The run's premise is corrected, not applied**: the start point's z is NOT a bare literal (`ETHParticleSystem::Scale` scales it with the entity, `ETHParticleSystem.cpp:27-33`; `scaleEntity` scales systems and position by one factor, 6072..6337 #30-53), so nothing is divided by 2.8125 and the effects runtime already had it right (7.3's note, corrected). Census: **15 carriers, 25 of 897 systems** - the five `blue_light` orbs (z 24) and the ten unlit `light_off` torches' sparkles (z 4), the plan's "5 levels" missing the torches; the torches are the only `ET_VERTICAL` carriers and their +10 raw depth shift is NOT built (no type key; a K10 pair for 9.2). **Sweep: 128 of 128 byte-identical to `8.3/after/sweep`** (1-01 `e2da5131`): at f420 nothing but added light lies between a lifted system and what it passes. **`test_mp_layer`: 1,872 checks, 0 failures**, in the independent check on a rebuild of the same source (Smart App Control refused the build's binary twice, launch and its one relink): 15 carrier lines, 25 of 897 systems, each carrier's pictures passed equal to the level files' census (61 in all). The lift is also measured on the running game - probes on copies of 1-05 and 4-22 (the build's) and of 1-08 and 4-22 (the check's, owners not at z 0), one picture moved onto the orb / the sparkles at a z at or below their depth and then above it: **2,628, 1,307, 4,429 and 2,741 px differ, every one brighter with the system on top**. The bowl's-rim row is answered: the orb windows are byte-identical, so it is the dim orb showing the rim through, not a depth. A thin vertical line above the orb on 1-08 and 1-22, older than this step, is filed (row below). The six dangling reads fixed and compile-guarded (a deleted `ReportOf` overload). Build 0 warnings (four builds); 128 rc=0, validation silent, 840 placements drawn. **Gameplay: none.** Engine untouched at `4bfcf67`. **Sweep baseline**: `8.4/after/sweep` | `bd44717` | 92 |
| 9.1 | the level reader takes a Sprite2D's `scale`; `Find` draws its units and offset times it; the lightmap checked before it | committed with four recorded deviations (the "Order 9.1" rows below), no gate row unmet - K10's reader half, to be committed before 9.2's converter writes the key. `Tscn::Parse` takes `scale` on a Sprite2D alone, as a Vector2 of two numbers above 0, and refuses it anywhere else or in any other form, naming the line; `Sprites::Find` sizes a sprite `Tiers::Units(texels, density) x scale` with its offset x scale (K9: density, then truncation, then scale; never the density on the offset); `Lighting::Read` checks a lightmap against the units BEFORE the scale (the engine bakes it at the bitmap size with the entity's scale set to 1, `ETHLightmapGen.cpp:57`, `:117-120`; formats doc); `buildEmitters` makes a placement's systems at `drawn.scale x (sx + sy) / 2`, the engine's mean (`ETHSpriteEntity.cpp:316-319`), in one call. No converted level carries the key, so **the 128-level f420 sweep is 128 of 128 byte-identical to `8.4/after/sweep`** (1-01 `e2da5131`). **Gameplay: none** - no body reads the sprite's scale; the barrels and the stone change with 9.2's colliders (R9). **6.1's latent K7 fixture is built**: a copy of level0 with 1-1's torch at scale 2 reads lit, draws the torch 64 x 128 u at (288, 96), keeps its light (range 300, at (288, 52)) and its halo (300 u square, at (288, 52): the engine centres a halo on the UNSCALED light position, `ETHRenderEntity.cpp:378`) as `eth_*` says, and makes its flame at twice the size, born at (288, 40). `test_mp_tscn` 204, `test_mp_sprites` 1,080, `test_mp_lighting` 548, `test_mp_layer` 1,890, `test_mp_levels` 128, once each, 0 failures. **Smart App Control refused 3** (the first launch of `test_mp_tscn`, `test_mp_levels`, `test_mp_layer`); each relinked once and ran. **Not taken**: step 92's entity-type pair (the "Order 8.4" vertical-shift row). Engine untouched at `4bfcf67`. An independent check passed every gate (`9.1/verify1/`): the rule re-derived from the engine first, the 8 changed sources recompiled with the build's own commands at 0 warnings, the five suites 0 failures at the same counts, its own sweep on `865c2d4b` 128 of 128 byte-identical to `8.4/after/sweep` and to the step's, 0 refusals in 133 launches; its one low finding - the 8.4 row's commit still "(this commit)" - went in at commit as text only. **Sweep baseline**: `9.1/after/sweep` | `aa68d17` | 93 |
| 9.2 | the converter writes each `<Scale>` on its Sprite2D and bakes it into the colliders; the levels regenerated | committed with five recorded deviations (the "Order 9.2" rows below; the fifth, the `test_mp_levels` gate measured in the independent check rather than the build, is closed), no gate row unmet - K10's converter half. `tscn.py` writes `scale = Vector2(sx, sy)` on the Sprite2D only where `<Scale>` is not (1, 1), the offset left `-pivot`; every collider is written from the box after `box.Scale` (`ETHPhysicsSimulator.cpp:72-73`): size and position per axis, the polygon and compound on the scaled box, a circle `(sx * w + sy * h) / 4`, a joint's anchor too; `trigger_size` stays unscaled (a script's `GetCollisionBox()`, `ETHEntity.cpp:314-320`); a scale not above 0 is refused. pytest 110 (11 new; the plan's fixture: 1.3 on 22 x 29 -> `scale 1.3`, 28.6 x 37.7), ruff clean. The HEAD converter first reproduced all 1,042 files it writes in `out/`; the new one's staging (`--copy-textures --copy-lighting --copy-particles`) differs from it by **33 inserted `scale` lines, 31 box sizes 22 x 29 -> 28.6 x 37.7 and 1 circle 30 -> 24, in 18 levels, and nothing else** - the plan's counts; the 18 replaced; `snap_pre8` untouched. `census9.py` 33 of 33. **GAMEPLAY CHANGED (R9, as the original)**: 31 barrel bodies 1.3 times larger per axis, the 2-22 stone 0.8 times. **Sweep: 111 of 128 byte-identical, 17 changed, all 17 among the 18 scaled levels** (4-19's eclipse is below the view; 1-01 `e2da5131`): barrels drawn 41.6 u, the stone 51.2 u; at f420 2-24's player stands on a barrel (36.8 u higher), as the original's h6.3 shows it, and 2-19's dropped crate (the port drops it, the original does not: filed) lands upright, with the player 18.7 u further right, at the x where the original's player stands; 2-29's scale-1 stone rests 3.4 u further right. Probes for 9.3: `barrel_width.py` on 2-15 **31.3 / 31.6 u** at the original's own pixel spans; `stone.py` 30.16 u, the disc clipped (the script reads the original at 31.6-37.1 u too); mean_abs against h6.3 falls on 15 of 17. `test_mp_start` read 3 failures - its tier checks used `sizePx`, which step 93 made units x scale - and now reads units (753 checks, 0 failures, 128 start and play, 33 scaled); 14 level-reading suites, 5,672 checks, 0 failures. `test_mp_levels` re-pinned (`keys["scale"]` 33, 12 keys): **129 checks, 0 failures**, in the independent check on its relink of the same source (`56084a70`; Smart App Control refused the build's binary twice, launch and its one relink). 128 rc=0, validation silent, 840 sidecars drawn. 145 launches in the build, 2 refused. Engine untouched at `4bfcf67`; `MagicPortals.exe` `865c2d4b` unchanged. An independent check passed every gate (`9.2/verify1/`): the rule re-derived from the engine first, the HEAD converter reproducing `out/` and the new one's staging equal to it, its own diff and `.esc` census 33 / 31 / 1 and nothing else, pytest 110 and ruff clean, the two changed test sources compiled at 0 warnings, the 14 suites 0 failures at the same counts, its own sweep on `865c2d4b` 128 of 128 byte-identical to the step's, 0 refusals in 143 launches; its four low findings went in at commit as text only - `test_mp_levels` recorded as not run (it ran there), 2-19 described as matching the original where only the player's x does, a formats-doc citation one line off (`ETHSpriteEntity.cpp:316-319`), and 2-29's positions against the original, older than this step (row below). **Sweep baseline**: `9.2/after/sweep` | `916d0b5` | 94 |
| 9.3 | the `<Scale>` pins and gates: the three sprites the plan names, R9's bodies as built, the barrel and stone pictures against the original | committed with four recorded deviations (the "Order 9.3" rows below), no gate row unmet but the brief's `stone.py` as written - TESTS ONLY, closing batch 9. `test_mp_sprites` pins on the converted levels 2-15's barrel **41.6 x 41.6** (hd, 64 texels at 1.3), 4-19's eclipse **512 x 64 about (0, -30)** (1x, 256 x 32 at 2) and 2-22's stone **51.2 x 51.2** (hd, 128 texels at 0.8), and over 128 levels **33 scaled sprites (31 / 1 / 1), each its units times its scale**; `test_mp_fire` level12a's three barrel bodies **28.6 x 37.7 u** and `test_mp_demolish` 2-22's stone **a circle of 24 u** (level8's, at 1, 30), as the engine builds them (`ETHPhysicsSimulator.cpp:59`, `:72-73`). `test_mp_levels` 129 (`keys["scale"]` 33, re-counted by grep: 31 / 1 / 1). `barrel_width.py` on 2-15 **31.3 / 31.6 u at centres 1092.5 / 1188 px** - the original's widths to the pixel. The stone: the brief's `stone.py` reads 30.16 u on the port and **31.6-37.1 u on the original** (its sky rows hold the disc's own top), so it is re-instrumented (`9.3/tools/stone93.py`, parameters fixed before a port frame was read, validated on the original first): **port 24.65 u, original 24.62-24.64 u**, the drawn file's prediction 24.55 u; gate 25.3 +- 1.5 u. 4-19's eclipse, never on screen in the port, is checked against footage F17 (section 5 names it for 9.3): the rim's curvature at rest **r 707.2 px** against `eclipse.png` at 2, **713.7 px** (0.991; at 1 it would be 356.9). **Gameplay: none changed by this step** (it pins step 94's R9 change); R9's suites green - `test_mp_play` 205 and `test_mp_launchers` 98 print step 90's output line for line, `test_mp_fire` 48, `test_mp_demolish` 138; `test_mp_sprites` 1,224. `MagicPortals.exe` `865c2d4b` unchanged (the build relinked the three suites alone, 0 warnings) and `out/` unchanged, so **the sweep is 128 of 128 byte-identical to `9.2/after/sweep`** (1-01 `e2da5131`), 128 rc=0, validation silent. 134 launches, **0 refused**. Engine untouched at `4bfcf67`. An independent check passed every gate (`9.3/verify1/`): the rule re-derived from the engine first (`ETHSpriteEntity.cpp:682`, `ETHEntity.cpp:296`, `ETHPhysicsSimulator.cpp:59`, `:73`), the three changed sources compiled with the build's own `cl` commands at 0 warnings, seven suites once each at the step's counts and `test_mp_start` **753 checks, 0 failures, 2,883 sprites, 33 scaled**, its own `.esc` census 33 / 31 / 1 / 1 in 18 levels, its own sweep on `865c2d4b` 128 of 128 byte-identical to `9.2/after/sweep` and to the step's, the barrels 31.29 / 31.29 u by a gradient-edge reading on the port and the original alike, the stone by its own instrument **port 24.57 u, original 24.58 u** (step 93's unscaled stone 30.71 u against 30.69 predicted), the brief's `stone.py` reading the original at 27.9-68.5 u over thresholds 0.55-0.75, `out/` and `snap_pre8` unchanged, 0 refusals in 135 launches; its two findings went in at commit as text only - the stone instrument's corroboration, handed to the owner (row below), and `test_mp_start` recorded as not run where it ran there. The build confirmed current at commit (a ninja dry run: no work to do). **Sweep baseline**: `9.3/after/sweep` | `97ea5c6` | 95 |
| 11.3 | the level's end and a death: the red suck and sparkles at the door, blood, the skull, and `AM_MODULATE` as a multiply | committed **with nine recorded deviations and one gate row NOT MET as written - the F12 red light - under the owner's delegated decision, not tuned** (the "Order 11.3" rows below; the fifth closed in its fix round); no required gate row unmet (the unit, the test counts). No change inside the step can meet the light row: it is re-taken when the exit test, R25 or the effects runtime's frame order lands. A fit in fix round 1 shows the light is the original's in brightness (gain 1,517 against 1,522) and in rule (eighths of the sparks' live share); it stands 7.75 u left (the port's exit test, filed) and 4.0 u high (R25's player half). And the door frame's light is missing, because the effects runtime draws what `GameStateController` adds a frame early (filed for the runtime, recorded not fixed). `fire32` is left for the owner. Taken over from a first implementer that died at the login expiry: its diff reviewed against the decode and kept unchanged, every capture re-taken on the final binary `7adf5f3b` and byte-identical to its first round. `levelFinishedEffect`'s `red_suck_effect` at the door + (0, 16) and `red_sparkles` (light 68, (2, 0, 0)) at the character on the door tick; `checkGameLost`'s `blood.ent` at the character's vector3 (z 1.422, R10) or, for a fall, `skull.ent` 12 over the view's foot (both branches); a minion's blood at its entity point, and a killer floor's skull under it (`Minions::State::deaths`, read by nothing in the sim). `blood.ent`'s burst drawn as a multiply: `modulateImage` generalised from the loading halo (whose image is byte-identical), `particleTexture` in all three builders, `DrawColour` (0, 0, 0, 1) for `alphaMode 4` (K12). Unit: **0 of 65,536 (g, d) past +-1, worst 0.498**; 4 of 4 of the original's multiplies exact. F12: the disc peaks at 13,190 red px at +150 ms (original 13,228 at +202), the sparks' count clause holding (a pool of 8) but the sparks about 8 u left and 4 u high, fewer distinct points (deviation 1's cause), the light **12.1 against 16.9 R - G on the right-hand stones and a gain of 27.2 against 17.3 on the left** (it stands 7.75 u left), in eighths in both; F13: the skull at y 685.5-686.0 px (original 685.5), 48 x 40 px, fade to zero at +0.813 s (+0.808); blood's burst peaking at +300 ms (+331), the puff rising 80.5 px/s (84.3). **Gameplay: none.** Suites once each, 0 failures: `test_mp_effects` 339, `test_mp_sprites` 1,256, `test_mp_minions` 115, `test_mp_layer` 1,928 (**1,937** after fix round 2), `test_mp_diamonds` 126, `test_mp_keys` 124 (run in the independent check); `test_mp_ghost` NOT RUN (Smart App Control: 3 refusals in the takeover). **Sweep: 125 of 128 byte-identical to `9.3/after/sweep`**; 2-12 and 2-32 (the character dies with no input; 2-32's smoke still alive at f420) and 3-09 (a minion on a killer floor) change in effect-stream pixels, plus 17 door-flame pixels one RGB565 level off (equal-depth tie order, INFERRED; deviation 9, filed below). Engine untouched at `4bfcf67`. An independent check (`11.3/verify1/`) failed the light row and four text points. Fix round 1 (`11.3/fix1/`) changed no source and launched nothing: it added the light fit, the door frames' order (F12 #140 on, #141 off, #142 on, as the engine source gives) and the text. A second check (`11.3/verify2/`) failed the light row again, finding with this record that no change to the step's code can meet it. Fix round 2 (`11.3/fix2/`) showed the light's x is the port's exit test on a guessed body (679.011 u, the first tick past 693 - 4 - 10 = 679, the capsule a `_guess`), reworded the sparkles row, added `test_mp_layer`'s strike case (a shock diamond's strike on 3-16: one `blood.ent`, no skull; the suite alone relinked, `fdefe196`, 1 launch, 0 refused) and left the blood row's z guard for the next step that relinks the simulation. `MagicPortals.exe` `7adf5f3b` unchanged, so the sweep stands. A third check (`11.3/verify3/`) reproduced every number (the light to the hundredth: f192-f195 at 0.65 / 0.67 / 0.67 / 12.14 against #139-#142 at 1.86 / 16.64 / 2.31 / 16.66; the six suites at the same counts; the sweep and the four clips byte-identical; a compile-only /W4 run of the 8 changed units, 0 warnings; 138 launches, 0 refused) and failed the light row alone; it also found that the original checks a death before the door on one frame (a gameplay row filed below). Committed with the nine deviations under the owner's delegated decision; the commit changed text only. **Sweep baseline**: `11.3/after/sweep` | `df638f1` | 96 |
| 11.4 | the knights' aura, and the sim honouring the marker's `entityName` (R16) | committed with five recorded deviations (the "Order 11.4" rows below; the first, `test_mp_layer` NOT RUN, closed by the independent check), no gate row unmet. R16 as the owner's standing ruling: `Minions::Minion::entity` holds what the marker adds (`minions.json` `spawn`: `minion.ent` by default, `ghost_minion.ent` on level31b, anything else refused at load); the two `.ent` bodies are equal to the attribute, so **gameplay: none changes in play** - what the original does differently by the entity's name (the ghost on the knight's death, R4, 12.2's; `ETHBeginContactCallback_minion`'s arms, the dark mage's `holdTime` cap, the impact tremble and `playFallSound`, which the knight never gets, `ETHPhysicsEntityController.cpp:59`, `:66-70`; `ScoreDashboard`'s `minion.ent`-only counts) is not built on `main` (corrected after the check: the first record said the callbacks did not tell the two apart). The aura: `ghost_minion.ent`'s 24 `black_portal_particle.png` at `alphaMode` 4, added once through the effects runtime with no picture of its own (`EffectSpawn::picture`), carried by the knight's DRAWN point (`EffectOwner::Kind::Minion`), on the multiply (`modulateImage`), at `EffectSystems` of round(0.711 - 2) - behind the platforms, the knight and the player, as the original sorts it; K8 kept (`m_effectRandom`). Unit: 0 of 65,536 past 1 on the generic case (`test_mp_sprites` 1,256), on the multiply the app wrote (4,096 of 4,096 texels) and on the drawn quads (`test_mp_layer`); follow within 0.5 u: **0.000000 u** from the knight's drawn point on 600 of 600 frames over 80.7 u of patrol, the tick body 2.013 u off, 214 released at the owner's x - read through a dangling pointer in the new case (deviation 5, whose depth check against the knight is vacuous; the order measured from `level31b.tscn` instead: door -1.452 < aura -1.4485 < platforms -1.448 to -1.432 < knight -1.430 < player -1.428). Suites: `test_mp_minions` 278, `test_mp_effects` 343 (11.3's blood z guard), `test_mp_ghost` 75, `test_mp_keys` 124, `test_mp_start` 753, `test_mp_sprites` 1,256, `test_mp_layer` 1,959 (the check's one launch of `03d1f439`), 0 failures; `test_mp_diamonds` NOT RUN (refused twice). **Sweep: 127 of 128 byte-identical to `11.3/after/sweep`**; 3-32's eyes (89 px, the effects' stream) and 23 door-flame pixels, each in one channel by at most 9 levels. F15: matches in kind. `MagicPortals.exe` `b87639e7`, 0 warnings; 3 refusals in 141 launches. The independent check passed every gate (`11.4/verify1/`): the 8 changed units compiled with the build's own `cl` commands at 0 warnings, the suites once each at the step's counts, its own sweep 128 of 128 byte-identical to the step's, its retake of the 3-32 clip 201 of 201, the player dying on tick 260 as before; 1 refusal in 137 launches. The commit changed text only (a dry run on a copy of `build.ninja`: no work to do). **Sweep baseline**: `11.4/after/sweep` | `3affd2f` | 97 |
| 12.2 | the ghost, `enemy_shoot` and its dropped key, and R4 | committed with ten recorded deviations (the "Order 12.2" rows below; the first two - every capture gate, and `test_mp_motion` and `test_mp_diamonds`, NOT RUN at the build under Smart App Control - closed by the independent check) and **one gate row NOT MET as written, by design: `test_mp_ghost` "stays at 73"** (75 after step 97; re-pinned from the decode under R4, 141). R4 as the owner's standing ruling: no ghost, shot or restock on 3-32 until the black knight dies (fire, the killer floor or a strike), the ghost added at its point one level unit behind it and at scale 0.3, entity 1430 deleted, its init the tick after (INFERRED), five waypoints 1200 / 2500 / 50 / 0 ms to ghost_pos black to white, SHOOTING on its 227th update (226 after its init), the boss music from the init. **GAMEPLAY CHANGES**: R4; a fireball burns the minion it reaches on 3-29 to 3-32 (`Minions::Burn`, the escorts included); the ghost's point hovers 8 u (`motions.json` `ghost`), which moves its shots' origin and its key; a ghost shot starts 16 u out (`shot.nudge_px`), 0.13 s sooner. Pictures: `syncGhost` on `hd/ghost.png` cut 121 x 128 (`Slot::Ghost`, -0.2889: behind the z-0 art, in front of the locked door), cell 3 at alpha 0.5 raging (on the quad uv 0.7461 and albedo alpha 0.5; on the picture cell 3's eye slit); `ghost_appear_effect.ent` with its `followUp` and `Scale(1.005)`; `enemy_shoot.ent` (a 42 x 32 cell of six, light 64, halo 146, at scale(10), on the one `_projectileFrameTimer`), `portal_mark.ent` at scale(32), `projectile_shoot.ent`, `portal_fail_in_antiportal.ent`; the dropped `key.ent`, bobbing. F15: in the simulation scale within 0.028, path within 4.52 u, hover 8.000 u moving up from the centre; on the picture (the independent check's fire route: the knight burned on tick 468, the ghost added that tick, SHOOTING on tick 695, the player dead to the first shot on tick 721) it **matches in kind** - the black silhouette grows and flies up-left with its puffs, greys with the red face and arrives at ghost_pos at the same size and place; the back patch's R at 0.47 / 0.73 / 0.95 / 0.99 of its arrival value against the original's 0.54 / 0.65 / 0.90 / 1.00; the hood's hover within 0.42 u of the model. Two differences filed (no `fire32` flames on the knight; the first shot on SHOOTING's first tick, 0.2-0.3 s before the original's). Rage's luminance: the ghost's contrast over the same background 0.54-0.56 of SHOOTING's, the raw mean 0.814 - exactly alpha 0.5 over its L 31 background, 0.5 + 0.5 x 31.1 / 49.5; rage reached only by the port's fireball rule (the owner's open row). Suites, 0 failures: `test_mp_ghost` 141, `test_mp_minions` 288, `test_mp_effects` 382, `test_mp_sprites` 1,266, `test_mp_layer` 2,011, `test_mp_boss` 126, `test_mp_keys` 124, `test_mp_start` 753, and on the check's relinks `test_mp_motion` 169 (the build's binary had failed 14 of 167, fixtures without the `ghost` row) and `test_mp_diamonds` 126. `MagicPortals.exe` `7e43f9b3` (the check's link-only relink), 0 warnings; 1-01 f420 `e2da5131`. **Sweep: 128 of 128 byte-identical to `11.4/after/sweep`**, as predicted. Smart App Control: 11 refusals in 24 launches at the build, 0 in 147 in the check. Engine untouched at `4bfcf67`. The commit changed text only (a dry run on a copy of `build.ninja`: no work to do). **Sweep baseline**: `12.2/verify1/sweep` | `1d05b59` | 98 |
| 12.1 | the dark dragon and its dropped yellow key's picture, and R20 | committed with ten recorded deviations (the "Order 12.1" rows below; the fourth, the wound read before the shot, closed in fix round 1) and **one gate row NOT MET as written, by design: `test_mp_darkdragon` "stays at 97"** (99 after step 85; re-pinned from the decode under R20, 209). R20 as the owner's standing ruling: a wound is 1000 + 8 x 300 = 3400 ms. `ETHCallback_dark_dragon` read in full and built in its own order: the init on the first update after the summon; `linearMotion(this, true, 0, PIb)` every update on the LAST update's `originalPos` (speed 1.6, stride 7); a WaypointManager (`DarkDragon::Path`: one waypoint an update, the current waypoint's filter on position, angle and colour; `sim/Ease` for smoothEnd / smoothBeginning / smoothBothSides); APPEARING's seven waypoints (14, 0, 0, 10, 28, 27, -12 degrees) with the jaw opening on waypoint 3 (`smoothBeginning(bias) x 14`, restarted every update on cell 3); FIGHTING on the Follower (800 / 150, forced to -12 + 360, 9 updates at 348 then 3.14 % on the 10th, toward `getAngle(normalize(dragon - char)) + PIb x 1.2`, unclamped) with the neck's lift on its point and the mouth killed below interval / 2 and restarted above 3 / 4; DAMAGE red / 3x-bright on eight 300 ms legs, the head thrown +27, the jaw shut and reopened; DYING white, +30, the jaw shut. **GAMEPLAY CHANGES (4-32 only)**: R20 (the torch shot and the return 205 updates after a wound, not 168); the eased paths and DAMAGE / DYING from where it stands; its point hovers 7 u and the neck moves it up to 18 u; the shots leave from the mouth (`MuzzlePx`) - the torch shot along its own point's line to the torch, 1.8 u from it from F16's floor (the shot itself is still the port's: the original fires only at an unlit `light_off.ent` and relights it, filed below); the first shot 1.3 s into the fight, not 4.2 s (the fire clock runs on from the jaw's waypoint); and (fix round 1) the wound read after the mouth and the shot, all three inside `!isFinished`, as ins 1067 orders it. Pictures: `dark_dragon.ent` whole through the effects runtime (`EffectOwner::Kind::DarkDragon`: dragon_boss.png cut 2 x 2 on (-48, 7), dragon_normal.png, the mouth's `explosion.JPG` on `m_effectRandom`), its cell, angle, emissive and mouth from the sim's `Look` every frame; at z 0, not the plan's 8 (the hover's SetPosition writes originalPos's z 0, `ETHPhysicsEntityController.cpp:92-96`); the dropped `key.ent` at key_pos, z 6, bobbing. DEV: `--burn-dragon <tick>`. Gates: the replay's cells on the pixels **105 of 105** frames from f738, 107 of 107 from f726 (0 through legs 0-2, 0 -> 1 -> 2 -> 3 on f906-f924, 3 on to f1362; a silhouette fit against the committed build's frames, byte-identical to f684; 5.2's luminance fit agrees on 96 of 99, missing only while the head enters the view); the flash **R / G 3.61 / 3.61 / 3.58 / 3.53** at the red legs (bright 1.68, fighting 1.94), on a DEV-burn capture; F16 agrees (at shout 2.52 s, FIGHTING 7.42 s, the jaw within a frame of F16's on the decoded anchors but about 0.2 s late counted from F16's burst frame, where F16's own leg 3 runs 0.15 s early, the mouth's fire out 3.13 s after leg 3 and the shot 4.20 s after, F16 3.09 / 4.20) - and, since fix round 1, none of the fire drawn on an update that restarts it, as the engine updates, then runs the callback, then draws: 0 px of lum > 200 in the mouth's box on 17 of 17 such frames against 900-2,353 free-running, F16 none on #238-#250 and #319-#340 (the first round drew a ball there, 952-1,014 px). Suites, 0 failures: `test_mp_darkdragon` **216**, `test_mp_layer` **2,042**, `test_mp_start` 753 (fix round 1; 209, 2,041 and 753 before it). `MagicPortals.exe` **`753dc103`** (fix round 1; `a70bb8c5` before), 0 warnings; 1-01 f420 `e2da5131`. **Sweep: 128 of 128 byte-identical to `12.2/verify1/sweep`**, as predicted (nothing of it exists until a torch is shot); fix round 1 re-swept on `753dc103`: again 128 of 128 byte-identical to it and to `12.1/after/sweep`, 128 rc 0, validation silent. Smart App Control: 2 refusals in 141 launches, and 0 in 134 in fix round 1. Engine untouched at `4bfcf67`. An independent check (`12.1/verify1/`) reproduced every number and failed the step on the mouth's fire (drawn while Play restarts it every update, where F16 shows none) and on this record's reading of the torch shot (backwards); fix round 1 (`12.1/fix1/`) fixed the first in `updateEffects`, corrected the second in the text and filed it, and closed deviation 4. A second check (`12.1/verify2/`) passed every gate: the rule re-derived from the listing before the code, the 7 changed units compiled with the build's own commands at 0 warnings, the route and the burn run re-captured byte-identical, its own fit putting 79 of 79 judged frames on the cells (least margin 0.023), the red legs' R / G 3.49 / 3.49 / 3.45 / 3.40 on its own mask, the three suites at 216 / 2,042 / 753, its sweep 128 of 128 byte-identical; its four low findings - `darkdragon.json`'s hover note, the cells counted from f726, step 98's commit cell filled in this row's diff, a candidate for the jaw's 0.2 s - went in at commit as text only. **Sweep baseline**: `12.1/fix1/sweep` | `848ed92` | 99 |
| T.1 | a scheduled `--tap` is a real press: the controls read the tick's touch, and its pixels are the viewport's | committed, no recorded deviation and no gate row unmet - a step this run added (not in §7). Its first measurement: the backlog row's mechanism was already gone - step 87 had moved `readInput` onto the tick's `Touch`, so on the committed build a `--tap` already fired a shot and lit 4-29's torch. What was left, and is built: a scheduled tap on a HUD control is spent on the control as a finger's is (it had fired a shot under 1-04's restart button), and its pixels are the viewport's (in a 960 x 540 window it had aimed a third further out and lit nothing). **Gameplay: none** - a real press reads the same expressions. `test_mp_layer` 1606 (+13: `AScheduledTapFiresTheShotARealPressFires`, `AScheduledTapOnAControlIsSpentOnIt`), `test_mp_play` 205, 0 failures. Sweep 128 of 128 byte-identical; 7.4's, 1.1's and the help-block levels' `--tap` captures byte-identical. 0 refusals. With step 87, closes the "`--tap` cannot fire a portal shot" row | `87aa67f` | 100 |
| Q.1 | the earthquake the original shakes its camera with (decode D11 and D11_check), and every call of it whose event the port had | committed with four recorded deviations, **one of them a gate row NOT MET under the owner's delegated decision, not tuned: F16's 4-32 light wall opens at +-10 px, the port's at +-19.7** (D11_check C52 unsettled; the "Order Q.1" row below). `sim/Earthquake` is `g_earthquakeGen` built literally - a call overwrites the amplitude with clamp(0.35 x intensity, 0, 50) screen px at R12's 720; a tremble every 70 ms, forth then back, halving, stopped under 1 px; y only; no random draw - on the layer's tick clock, drawn on the frame's camera after the interpolation and never on the tick's camera, the taps or the HUD; 24 call sites wired from counters the sims already kept. L1 on 4-29 met: -19.78, +19.79, -9.90, +9.91 ... +1.14 px against -20, +20, -10, +10 ... +1. `test_mp_camera` 100, `test_mp_launchers` 101, `test_mp_layer` 1719. Sweep 128 of 128 byte-identical (12 levels fire calls before f420, none on tick 420). **Gameplay: none.** 0 refusals. Since closed: deviation 1 (a frame of several ticks draws only the last one's move) by Q.2, deviation 4 (the pinned sky where the original draws black bars) by 13.3; D11 rows 2, 6 and 14 wired by 10.1, rows 4, 8 and 25 by Q.2 | `b822a14` | 101 |
| 13.3 | the four black bars round the scene, and the `type` and `layerDepth` the entity reader dropped | committed, no recorded deviation and no gate row unmet. `Game::manageSceneBounds`' four `black_bar.ent` at (256, -16), (512, -16), (256, maxY + 16), (512, maxY + 16), 512 x 32 each, layerable at 1.0 and drawn at an explicit topmost `kLayerableTopZ` = 1.0 (K13: a slot of its own, not a table row), under the HUD. On 4-29's light wall every uncovered pixel outside the HUD is (0, 0, 0) on all ten shaken frames (19 / 9 / 5 / 2 / 1 whole rows each way, plus the row the edge crosses, a 4x MSAA blend); the other 210 of 220 frames byte-identical. 10 levels are wider than the 768 u the bars cover (re-counted); no capture shows a band stopping at 768. `test_mp_sprites` 965, `test_mp_layer` 1765, and three more at their counts. Sweep 128 of 128 byte-identical. **Gameplay: none.** 1 refusal | `3003e04` | 102 |
| 10.1 | shot marks and failures (`portal_mark`, `red_cross`, `hourglass`, `red_waves`, `portal_fail`, `portal_fail_in_antiportal`, `projectile_shoot`), and K14 | committed with ten recorded deviations and decisions (rows below), no gate row NOT MET. The mark chosen at the tap by `computePortalFinalPos` run on the port's world (R15, the owner's standing ruling: a picture only); a shot's end drawn where the port's simulation ends it; D11 rows 2, 6 and 14 wired. **K14 closed**: the shot and its halo are the table's first two rows (-0.515625, -0.5078125), behind the player (1-07: 5,518 and 5,502 px inside the player's box, every one darker, as F3 #27 and #206 show the original). F3 matched at every event the port can play. **Gameplay: none** - `Shoot` returns what it returned and moves no clock on a refusal; the R23 overlap was decided up front (the hourglass does not restart the 400 ms clock). `test_mp_layer` 1865, `test_mp_shot` 181, `test_mp_effects` 308, `test_mp_camera` 103, and nine more at their counts. Sweep 128 of 128 byte-identical. 5 refusals | `7ac69b1` | 103 |
| Q.2 | every tremble a frame's ticks make drawn, and the impacts of the player, a stone and a minion (D11 rows 4, 8, 25) | committed with ten recorded deviations and decisions, no gate row NOT MET; its first verification failed it (4-29 opened at F16's +-9.8, from knights landing off their spawners - calls the original never makes, because it makes each knight standing), a fix round removed those calls, and the third verification passed it. A held frame move draws every tremble whatever the ticks per frame; `sim/Impacts` computes a contact's approach speed port-side (the fastest along the row's line since the body last met or left anything, at most 60 steps), with no engine change; a new minion is looked at where the original's stands. F14's landing: -4.96, +4.98, -2.44, +2.45, -1.15, +1.16 px against F14's -5, +5, -2, +2, -1, +1; L1 on 4-29 met again, all 220 frames byte-identical to the committed build's. 27 of the 30 wired rows pinned by a case. **Corrects D11 section 4 row 4**: the player's contact normal is negated against a box or a polygon, never against a circle or a compound (#27-40; read from the listing, not RAW-checked). `test_mp_camera` 212, `test_mp_layer` 1924. Sweep 128 of 128 byte-identical (25 levels make impact calls by tick 420, none on it). **Gameplay: none.** 4 refusals | `7465279` | 104 |
| 10.1b | 10.1 read again after the level's opening, and `projectile_shoot` / `portal_mark` against F2 | measured, no change - a step this run added; the diff is its record and its corrections of step 103's (the opening ends at f70, not "about f100"). K14 behind the player after the opening; the wall shot's colours at full brightness match F3 #28-#45; `projectile_shoot` grows 162.8 / 161.2 u/s (the `.ent`'s 162; F2 152.9 / 151.6), `portal_mark` 11 dots, +90 deg/s, 32.08 u, visible 0.983 s against F3's 0.981. One picture deviation recorded, not built: a tap's particle-only pictures start one tick early (row below). Found: the standing player is drawn 9.75 px (3.47 u) higher than the original's (filed on the "Step 76 deviation, the player's carrier point" row). Nothing built; 133 launches of the committed binary, 0 refusals | `224196f` | 105 |
| 10.2 | `crystal_pick` where a crystal is taken, `crystal_vanish` x 1.3 where a timed one runs out | committed with five recorded deviations and decisions, no gate row NOT MET. Both at the crystal's drawn point, on its own depth + 20 (`Effects::OnEntity`, the vector3 ADDED to `GetPosition()`); 5 + 2 particles for the pick, 1 + 2 at x 1.3 for the vanish (first-frame box 73 x 76 px against the pick's 57 x 58). F14 consistent in kind (22 fps resolves no single frame); 2-04's library still nearest the port's +83 ms frame (16.0 against 49.1 and 74.7). `test_mp_effects` 324, `test_mp_layer` 1958. Sweep 121 of 128 byte-identical, the 7 exactly those with a crystal event by tick 420. **Gameplay: none.** 0 refusals | `d072502` | 106 |
| 10.3 | `key_pick`, `key_unlock_halo`, and a door's `wall_destroy`, `door_locked_vanish` and `keyhole_vanish` | committed with eight recorded deviations and decisions, no gate row NOT MET. A pick for any carrier, a knight's too, at the key + 32; the halo on the 41st tick after the unlock, at a z of 32 of its own; the three at the keyhole's alpha 0 in the script's order, beside `door_unlocked`'s 20; the door's sparkles at door z + 2 / 2.8125 (`OnEntityBare`: only the bare literal divided). 41 locked doors in 34 levels, D4_check's census. The unlock's pictures against F9 NOT RUN (no route in the port reaches a keyhole without a warp); its timings consistent. `test_mp_effects` 473, `test_mp_layer` 1996. Sweep 113 of 128 byte-identical, the 15 exactly those with a key pick by tick 420. **Gameplay: none.** 0 refusals | `0da3ac5` | 107 |
| 10.4 | a shock diamond's pick and strike, a fire diamond's burns and its water, the death a ring or a dead agent adds, the put-out signal's water | committed with ten recorded deviations and decisions, no gate row NOT MET. Eight call sites: `shock_diamond_pick`; `shock_death` at a struck minion's entity point, a bare 10 in front of a dead agent, and at the player a ring kills; `shock_strike` turned by getAngle's RADIANS read as degrees (at most 6.28 deg); `burn_projectile` a bare 10 in front of a taken fire diamond and at a burned shot; `extinguish_fire` at a drained diamond and at a put-out signal. `shock_strike.png` cut in whole units (step 67's deviation, for this sheet). F14's strike: its white peaks +0.13 s after the strike against F14's +0.09 s and is gone by +0.72-0.76 s in both; L1's water has the same plateau, fade and end, starting 0.12 s later. `test_mp_effects` 512, `test_mp_diamonds` 134, `test_mp_layer` 2065. Sweep 128 of 128 byte-identical. **Gameplay: none** (two write-only records). 0 refusals | `b3f1861` | 108 |
| 11.1 | a breakable wall's dust, turned by it and at its size; a destroyed stone's puff; a lift door's dust | committed with nine recorded deviations and decisions (four of them gameplay the owner has, rows below), no gate row NOT MET. `wall_destroy` at the wall's node and depth, turned by it (13 of the 33 walls lie a quarter turn, so their dust falls from a horizontal line), at `destroyEffectScale` (2 on 4-32's wall 2217 alone); `rolling_stone_destroy` at a z of 32 of its own, x 1.3; `door_lift_particles` at the door's edge where the tick began. 1-24's lying wall measured on screen (the band across the screen, not down it); 1-28's door dust falls within 12 px of the decode's arc. `test_mp_effects` 555, `test_mp_launchers` 105, `test_mp_boss` 133, `test_mp_layer` 2121. Sweep 125 of 128 byte-identical, the 3 exactly those with an event by tick 420. **Gameplay: none** (three write-only records). 1 refusal | `3a2d5fc` | 109 |
| 11.2 | the lit torch as the entity its shot adds, the fire signal, the torch put back and its wall faded in, the weightless trail, the well's lap | committed with fifteen recorded deviations and decisions, no gate row NOT MET; its first verification failed it on its RECORD (the signal's put-out test misread, the ghost's scale pulse called unmeasurable), a text-only fix round answered both, and a second verification passed it. **GAMEPLAY CHANGED, THE OWNER'S TO RULE: the fire signal orbits clockwise from above, added on its torch** (decoded; L1: 296 deg/s at r 49.6 px against 296-300 at 48-54) - the original's PICTURE, and gameplay here only because the port tests the put-out at the orbiting ball (step 30's rule), where the original tests at the torch's point and never counts a fireball (the "Order 11.2" row below); on L1's taps the put-out moves from tick 261 to 260. Built: `light_from_projectile.ent` whole (its sprite, 12 flames, its light, its halo at (0, -12), bouncing 1.04 / 0.96 where a light wall is), `light_on_sparkles`, `portal_death` at the shot, the signal, `light_off` put back at a bare 10, `light_wall_create` and the rebuilt wall fading in over a second, the light wall's own `wall_destroy`, `movement_trail` on the weightless levels (F10: +6 to +17 luma over 69-90 px), `roundabout_effect` and the agent's `elastic` turn (a lap captured on 4-17 by the second verification). Closes §1's G6 `light_from_projectile` item and step 82's 12 u halo (by the code; the room reads further from L1, §1's row), and answers 13.4's deviation 4. `test_mp_torch` 134, `test_mp_wells` 308, `test_mp_effects` 611, `test_mp_layer` 2382, `test_mp_sprites` 964, `test_mp_sounds` 107. Sweep 128 of 128 byte-identical. 1 refusal | `e30489a` | 110 |
| 13.1 | the ring a shock agent adds round itself, and the lightning's sheet cut as the original cuts it | committed with seven recorded deviations and decisions, no gate row NOT MET. `shock_area.ent` at the ABSOLUTE `scale(-10)`, 2 x areaRadius across, (0.4, 0.5, 0.5) added, turned 20 deg/s of game time (held by a pause), scaled by a fresh factor in [0.993, 1.007] every frame on `m_effectRandom` (running under a pause), gone with its agent; a new table row `Slot::ShockRing` -0.53125 (K13). 3-24 against the library `h6.3`: rim (107, 130, 124) against (107, 130, 125.8) and (99, 129, 124); outer radius 0.9964 and 0.9959 of areaRadius. `tesla_shock_black_bg.png` cut 51 of 256 (step 67's deviation closed; step 108's `cellPx` too). `test_mp_sprites` 1000, `test_mp_fields` 63, `test_mp_layer` 2446. Sweep: 20 of 128 differ, every changed pixel named (the 15 ring levels with a ring in view, the eyes on all 16 ring levels, 4-14's fireball, four diamonds' lightning). **Gameplay: none.** 1 refusal | `93e3641` | 111 |
| 13.2 | the weightless rooms' door: shadowless sheet, glow, bob | committed with four recorded deviations, no gate row unmet. Built from decode D5 as §8.13 amends §7: `window01_shadowless.png` on the 17 levels that carry `shadowless_door` (4-02 to 4-18), `door_highlight.ent` at the door's point, 128 x 192 u, additive, at depth -22 (a depth in the LEVEL's units - `readProperties` runs before `scaleEntities`), and the 1 u / 1.2 rad/s bob of door, `door_bg` and glow on the 18 `no_gravity` levels. Measured: the glow on exactly 17 of 128 levels; its quad inside the node's prediction to a pixel; least change 0 (additive); mean |after - before - texel| 1.080 of 255; mean |change| 0.047 on the stone in front of it against the 216.0 it would add were it not behind; the bob 0.9996 u at 1.2000 rad/s, period 5.2359 s, its zero crossing at frame 78.52 against 78.54. **Deviations**: (1) the exit's SENSOR does not move with the picture - `linearMotion` ends in `SetPosition` on a body `door.ent` carries (8 x 57 u at (0,20)); the port bobs pictures alone, as `sim/Motion` does everywhere, and it is proposed to the owner beside R21 (row below); (2) the glow's band magnitudes are NOT gated against the original, by §8.13's own instruction (D5 measured them about 40 % under the 1x prediction); (3) 4-19, the one weightless level with no swap and no glow, is not a capture gate - its rocket door sits outside the f420 camera - and is pinned by the 128-level census and the layer's negative case instead; (4) the bucket-visibility clock is not built, as for 3.4. **K13**: the glow is a new between-slot user at `slotAfter(-22)`, pushed last in `buildSprites` so no dial or ring slot moves; §8.13 already lists the row for 6.3's table. **K8** does not arise: the start angle is the constant 0, and the 111 byte-identical levels of the sweep say no phase moved. **Sweep baseline**: `13.2/after/sweep` now stands for all 128 levels, superseding `5.2/verify1/sweep` and 3.4's one-row `3-15` exception | `dc8d3cc` | 79 |
| 13.4 | the ghost a lit torch leaves where its light wall stood | committed with four recorded deviations, no gate row unmet. Built from decode D5 as §8.13 amends §7: `dashed_door.ent`, named `light_wall_ghost`, at the wall's own point on the six levels that place a light wall (4-26, 4-27, 4-29 to 4-32), from the tick the torch is lit until its orbiting fire signal is shot. `hd/dashed_door.png` at the 720 selection (R12), 32 x 128 u, the wall's angle (1.5708 rad on 4-32), alpha-blended at full brightness 1.4222 u behind the wall - 4 RAW pixels, because `seekLightWallToDestroy` adds them AFTER `scaleEntities`, the opposite of 13.2's -22 - with alpha 0.2 + 0.1 cos on an angle of 300 deg/s and `bounceLightEntity`'s pulse 1.006 -> 0.994 -> 1.006 on 160 ms legs. Measured: 6 of 128 levels resolve it and 4-22 (a torch, no wall) and 1-01 do not; the six points are the wall nodes' own and `lookZ` is 0.578, 0.578, 0.578, -3.422, -1.422, -5.422, each behind the wall's quad; nothing is drawn on the tick the torch is lit (§8.0 rule 6), first drawn alpha **0.299619** and pulse **1.004045**; low 0.100000 and high 0.300000 at ticks 36 and 72, worst departure from the closed form under 1e-6 over 120 ticks, and the quad tinted and scaled on 120 of 120; the wall's own picture gone on the 90th tick after the flag with the ghost still drawn; the quad gone on the tick the wall returns, both clocks at 0, and a relit torch starting again at 0.299619 / 1.004045. **Deviations**: (1) the bucket-visibility clock is not built, as for 3.4 and 13.2 - both clocks run from the tick the ghost is added, and it is reachable in play on 4-32, whose wall stands 146 u left of its torch against a 455 u view; (2) a pause holds both clocks, where the original's `bounce` would go on pulsing; (3) the 4 px the original's rebuilt wall drifts back each light/put-out cycle, which the port's - rebuilt from the level's node - does not; (4) **nothing confirms the ghost against the original**: no footage puts a torch out on a wall level (L1 is 4-22, which has none) and no capture can light one, so every number is the decode's (the `--tap` row below, and §1's L4). **K13**: a new between-slot user at `slotAfter(wall eth_z - 1.4222)`, the row §8.13 already lists for 6.3's table. **K8** does not arise: no random draw. The gates are `test_mp_layer` (1361 checks) and `test_mp_sprites` (762) plus the 128-level sweep, not a capture. **Sweep baseline**: `13.4/after/sweep`, which reproduces `13.2/after/sweep` row for row and now stands for all 128 levels. **Deviation (4) answered by 11.2 (step 110, gate 7)**: footage `L1_out_4-29` confirms the ghost's position (to 0.1 px), its alpha pulse (period 1.208 s against 1.200, max/min 3.27 against 2.92) and its scale pulse's shape (profile r 0.999 on the second verification's reader); the scale pulse's AMPLITUDE is not resolved (0.60, 0.87 and 0.90 of the port's by three readers) | `f2e0e47` | 80 |

Merge `e81c21b` (§0). Integration step 74 (`591e143`, `7281ca5`) sends the well ring and the crystal dial through the
tier resolver. The ten rings now draw `hd/white_ring.png`. The dial keeps `timer.png`, which has no tier file. It
records no deviation of its own. It re-read 4.1's and 4.2's rows on the merged build, which gave the two open
instrument items below.

**Batch 5 opened after that merge.** Its first step, 5.1 (planning step 76), is the first step committed in this
repository rather than the engine's; 5.2 (planning step 77) follows it.

**Merge `a5c9e23` (planning step 88) closes batches 5-7**, on 2026-09-21. The run went on two tracks at once: track A
was `main` (5.1, 5.2, then 3.4, 13.2 and 13.4 out of batch order, steps 76-80), track B the branch `visuals-c` in the
worktree `Magic-Portals-Remake-vis3` (6.1 to 7.4, written as steps 90-96 and renumbered 81-87 by the merge). Twelve
step commits, all on `main` and pushed; the worktree and the branch are gone. Three files conflicted and each was
resolved by keeping both behaviours. **K13**: the merge migrated `Slot::DoorHighlight` (79) and `Slot::WallGhost` (80)
into 6.3's between-slot table, both at -0.5, exactly what the lambda they were written against returned, so neither
moves a unit; 19 rows now, 11 at -0.5. **K8**: 2-32's dragon-mouth re-pool moved off the placement stream onto
`m_effectRandom`. Merge gates: build 0 warnings; full `ctest` 45 of 45, 0 failed; the 128-level f420 sweep 128 of 128
rc=0 with Vulkan validation active and no message; M == B inside B's pictures on all 128, and == A outside them on 126
(the two exceptions are 2-32's 3,616 px mouth blob, which is the K8 move, and 15 RGB565-step pixels on 4-14 where A's
door glow meets B's effect quads, which neither track produces alone). No engine change on either track: the submodule
stays pinned at `4bfcf67`, which is the engine's own `origin/main`.

**Merge `fc243c6` (planning step 112) closes batches 8-13**, on 2026-09-29, with a docs-only `d1834c4` after it. The
run went on two tracks at once from `f1769ed`: track A was `main` (8.1-8.4, 9.1-9.3, 11.3, 11.4, 12.2 and 12.1, steps
89-99, pushed as they went; it regenerated `out/levels` and `out/assets` in 8.2 and 9.2), track B the branch
`visuals-d` in the worktree `Magic-Portals-Remake-vis4` (T.1, Q.1, 13.3, 10.1, Q.2, 10.1b, 10.2, 10.3, 10.4, 11.1,
11.2 and 13.1, written as steps 120-131 on the FROZEN level data `out/snap_pre8` and renumbered 100-111 by the merge).
23 step commits and the two merge commits, all on `main` and pushed; the worktree and the branch are gone. 14 files
conflicted and each kept both behaviours. **K13**: the table has 22 rows - B's `ShockRing` (-0.53125), `ShotPicture`
(-0.515625) and `ShotHalo` (-0.5078125) first, A's `Ghost` between `Escort` and `WallGhost`, A's `LiftedSystems` after
`Effect` - 12 classes at -0.5, 3 spreading, no row twice, no lambda. **K8**: no draw moved; the one order chosen is the
tick's effects queue, B's additions then A's, so a struck minion's `blood` follows the diamond's own two effects.
**`portal_mark`**: one spawn rule - A's `enemy_shoot_mark` (the same numbers) folded into it and added once at
`Ghost::Shot::destinyPx`, overriding step 98's note to keep two rows. **The earthquake meets track A's sims**: row 16
(the dark dragon's mouth) rewired to A's `DarkDragon::Path` (waypoint 3 past raw bias 0.2: 48 ticks from tick 721 on
4-32); rows 1 and 24 (a fall, a minion on the killer floor), which step 96 left for the merge, are live; row 25 reads
`Minion::entity`, so the black knight makes no impact tremble (`TheBlackKnightsLandingMakesNoTremble`). Merge gates in
§0. **Gameplay: the merge changes none of its own**; it puts both tracks' gameplay in one build - A's R9 bodies (31
barrels at 1.3, 2-22's stone at 0.8), R4 (3-29 to 3-32: the ghost comes with the black knight's death) and R20 (4-32:
DAMAGE 3400 ms), and B's 11.2 fire signal (clockwise from above, the owner's to rule). No engine change on either
track: the submodule stays at `4bfcf67`, on the engine's `origin/main` history but no longer its tip.

**WHAT IS LEFT OF THE VISUALS PLAN (00_order §7), after the batches 8-13 run.** Every step §7 lists is built - 0.1 to
13.4 - and none was shelved, blocked or skipped; 6.1's latent K7 fixture is built too (9.1, step 93). What remains is
work no step of §7 owns, and the owner's calls:

- **Left inside systems the plan built**: 8.4's vertical shift - an `ET_VERTICAL` owner's particle pieces sort
  +10 raw deeper (the "Order 8.4 deviation, the vertical shift" row); 13.3 now reads `light_off.ent`'s type 2, so the
  port has the fact, but nothing uses it. And the effects runtime's clock: a particle-only picture added from a
  controller or a tap is drawn a tick early (`portal_mark`, `red_cross`, `crystal_pick`; the "Order 10.1b" row), and
  what `GameStateController` adds a frame early (the "Order 11.3" row) - one runtime step could take both.
- **Pictures the original adds that no step owns** (each in its row below): `shock_agent`'s `small_explosion.ent`
  (10.4); `antiportal_explosion.ent` (8.13's unowned row, which also needs R23's in-flight kill); `fire32.ent` on a
  burning character (11.3) and on the burned knight (12.2); 2-32's claw's `wall_destroy` (11.1); a boss-dropped key's
  `key_pick` (the merge); the key's fly-in into its keyhole (10.3); the carried shock diamond at alpha 0.1 (10.4); the
  shock agent's pulse (13.1); the unlit torch's spin (11.2); the dark dragon's scattered `small_explosion_no_light`,
  its death's `explode` and `ghost_utility_spawn` (12.1); the ghost's DAMAGE and DEATH explosions (12.2); and the
  sounds 10.4, 11.2, 12.1, 12.2 and 13.1 name.
- **The earthquake's follow-ups**: Q.1's gate 4 NOT MET (F16's +-10 opening; the owner accepts it or C52 is settled);
  whether the generator resets at a load; the shaken-camera readers the port keeps at rest, the skull among them
  (Q.1's deviation 6 and the merge's finding); D11 rows 3, 13, 21, 30 and 10 / 8's kill unwired, row 9 dead on 3-20;
  `beholder_death`, `ghost_death` and `dark_dragon_death` wired but driven by no case; `Impacts::kRecentSteps` still
  a number in code.
- **Gameplay rulings decided for what the original does, and not built** (no batch owns them; they were kept out of
  this run by instruction): **R1** - an antiportal refuses at 32 x scale, the port at 16 (the "System 4" row); **R2** -
  the gravity well refuses nothing, the port still refuses and `test_mp_wells` asserts it (the "Gravity wells" row);
  **R23** - §8.14's portal-tap list: a refused tap restarts the 400 ms clock, budget-0 feedback, a reflected shot never
  dies on a wall, the in-flight kills (`anti_portal_agent`, a `fire_agent`'s flame, `light_off`), no death on a well,
  the zero-g recoil aimed at finalPos. 10.1's pictures now show each of these wherever it bites (the "Order 10.1"
  rows). **R22** - how to re-baseline 1-01's static portal 582's ring excess (1.62x, its pulsing part 3.02x), which no
  script lever explains - is a gate choice for System 2's follow-up (the "Step 59" row), also unbuilt.
- **The owner's calls still open**: **R24 and R25**, applied by step 76, still the owner's to rule (the "Step 76, THE
  OWNER'S TO RULE" row; the player's point is R25's other half, and 7.3, 10.1, 10.1b and 10.4 each record it again);
  **the two PORT-WIDE rows** - the script clock the original floors to whole milliseconds, and every body's picture
  drawn a tick behind the sim; **11.2's fire signal**, applied and the owner's to rule, and step 30's put-out test at
  the ball, which the original makes at the torch; and the gameplay rows this run filed for the owner without
  changing them - 12.1's barrel-bomb restock (4-32 cannot be won without it), its summon clock and its wound's shot
  at the torch, 12.2's fire against the ghost, 11.3's death before the door, 10.2's pickup rule, 10.3's carriers and
  the keyhole's visible clock, 11.1's four (`destroyable` stones, 3-20's destroyier, the lift door's rise and status
  machine, the beholder's filter) and its two unbuilt wall breaks, Q.2's minion spawn point and the engine's staged
  braking, 13.1's off-screen kill, 11.2's zero-g recoil speed, 13.2's exit sensor (beside R21) and step 73's ring
  depth.
- **Parked, owned by no brief** (§4 of 00_order), carried over and not re-checked in this run: the static portal's
  tint source - 8.2/D1 settled the `_portalColorA/B` globals as the ANTIPORTAL's - and the loading-screen suck burst
  against `launch_gold` frame 160.
- **The standing sweep baseline** is `merge_d/sweep` (step 112, `MagicPortals.exe` `8b5ab6d2`, the regenerated
  levels; 1-01 f420 `e2da5131`).

- [x] **Step 0** — engine `--screenshot-every N` (every temporal check depends on it). Done: SE step 57 (`29448a8`).
- [ ] System 1 colour transfer — largely done by lighting E1/G2; re-check its remaining rows.
- [ ] **System 2 particles**: emitter origin at the entity position (torch flame 13.4 u too low), spin sign
      (counter-clockwise ~108°/s), alpha, depth (particles 2.5 layers in front of their sprite), square quads,
      following moving owners; AM_MODULATE blend (engine has alpha + additive only). **AM_MODULATE is built as a
      multiply** by 11.3 (step 96: `modulateImage`, `DrawColour` (0, 0, 0, 1) for `alphaMode` 4; 0 of 65,536 (g, d)
      pairs past +-1) and used by 11.4's aura (step 97); the other items are not re-checked here.
- [ ] **Step 59's two recorded particle deviations** (SE step 59, committed with them, not tuned): on 1-01's static
      portals, the ring excess r 25-45 px on 582 is 106.5 against the original's 65.6 +-25% (1.62x; 1.33x against the
      58 footage frames), and the iris core's temporal s.d. on 604 and 582 is 3.41 / 3.19 against <= 3 on 51 frames
      (before the step 8.1 / 9.9 on 6). Both are the rings' brightness over their life (the original's pulse s.d. peaks at
      49-57 px, the port's rings alone at 23-33 px); tint, 0.8 scale, inline systems and the shipped AM_ADD blend
      (`libApplication.so` `SetAlphaMode` at 0x11eac0: `glBlendFunc(GL_ONE, GL_ONE)`) are ruled out. Next: root-cause
      the rings' life-versus-brightness curve against footage (`out/parity/visuals/particles/verify3/measure_v3.txt`).
- [ ] **System 3 player**: start frame 8 (facing right; `startFrame` is a dead field), walk 10.8 fps, space-suit sheet on
      the 19 zero-g levels, eye glow; 2.8 u rest-height difference is a body/collider question. (SE step 62, order 1.1:
      frame 8, the decoded 90 ms stride and the column carried across a stop are built. SE step 63, order 1.2: the
      space-suit sheet on the 18 `no_gravity` levels, its 280 ms standing column and its hover. SE step 64, order 1.3:
      the pushing rows. Still open: the eye glow (7.1) and the 2.8 u rest height.)
- [ ] **Step 62 deviation, the sweep gate** (SE step 62 committed with it; 00_order section 7 step 1.1): required normal f8
      wins in all 54 levels where the original's h6.3 f8 >= 0.85; measured 52 of 54 at f420. Misses 1-02 (best space f8
      0.7967, lead 0.004; normal f8 0.7927) and 1-03 (best space f8 0.8123, lead 0.0025; normal f8 0.8098): the
      level-start tutorial popup covers the player in the port's f420 frame, while the library's h6.3 frames of exactly
      these two were taken after the popup was closed (sidecars `popup_pages 1`; `tools/parity/capture_original.py:86-113`).
      Popup closed in the port (`--tap 1178,358@300-305`): 54 of 54, 0.894 at x 1100 and 0.903 at x 287 against the
      original's 0.906 and 0.894; popup up on both sides: 52 of 52; lowest winner 1-29 at 0.8575, lead 0.050. Next: amend
      the 00_order gate wording so 1-02 and 1-03 are captured with the popup closed, as the library did. Only those two
      chapter-1 levels, not every sweep gate; 1.2's gates are chapter 4 and unaffected.
- [ ] **Step 62 deviation, the stride's data shape**: systems_3_4 3a asked for `character.animation.frames_per_second
      11.111111`; the port stores the decoded `stride_ms 90` (`SideScrollerCharacter` ctor 9-11), rate derived as 1000 /
      stride, `idle_column 0`. Same decode, different block shape. Next: 1.2 builds its 280 ms zero-gravity column on
      `stride_ms`. (1.2, SE step 63, built that column as a separate block, `no_gravity_motion` `column_stride_ms 280`.
      The row stays open for the owner.)
- [ ] **Step 62 deviation, the zero-gravity player**: `MainCharacter::linearMotion` runs every update only when
      `noGravity` is set (3-6) and calls `FrameTimer` `set(0,3,280,true)` (163-171). On the 18 `no_gravity` levels the
      standing player cycles columns every 280 ms, a walking update adds its time twice, and the player hovers by
      1.2*cos(a) on y; step 62's timer holds only with gravity (recorded in `art.json` `_source`). Next: step 1.2.
      (SE step 63 built it: the standing column turns every 280 ms, and the pivot hovers 1.2 screen px, which is
      0.4267 u at 720 px. The row stays open, as step 62's deviation, together with step 63's owner items: the hover
      at other window heights and the eyes, which stay put.)
- [ ] **Step 62 deviation, facing a portal tap**: the original turns the player toward a portal tap and the port never
      does. `PortalManager::managePortalInsertion` #101-124 (listing L:27784) calls `setDirection(1)` if the player is
      right of the tap, else `setDirection(2)`, on an accepted request with no `projectile.ent` alive. Not a 1.1
      regression. Next: step 1.3 or a new 1.x: `m_facingRight = !(playerX > tapX)` under the sim's acceptance, pinned
      in `test_mp_layer` (a tap left of the standing level0 player gives `firstFrame` 4). Step 1.3 (SE step 64) did
      not build it, because a step covers one system. It still needs a new 1.x step.
- [ ] **Step 62 deviation, evidence for the carried column**: no clip of the original resolves a column across a stop.
      F12 holds right once; F9 r7b (`F9_flyin_3-18.mp4`) walks right, stops and steps back left, but at about 17 fps
      (median gap 50 ms) it cannot resolve a 90 ms column. Carry-over rests on the decode plus the port's tick-for-tick
      stop-and-turn capture (step 62's wording corrected at commit). Next: a 60 fps stop-and-resume clip.
- [ ] **Step 62 deviation, a fragile invariant**: carry-over depends on `syncDrawables` (`MagicPortalsLayer.cpp:2611-2614`)
      writing `playing` and `frameCount` together, because `SpriteAnimationSystem::Advance` resets `frame` to 0 when a
      playing flipbook's count is 1. Cannot happen today; `TheWalkKeepsItsColumnAndItsTime` guards it on level8. Next:
      1.2 (280 ms zero-gravity column) and 1.3 (pushing rows) keep that test running, or comment the coupling, if they
      touch that block. (`TheWalkKeepsItsColumnAndItsTime` still passes at steps 63 and 64. Step 63 adds a still
      flipbook on `no_gravity` levels only, and step 64 leaves the coupling as it was.)
- [x] **Step 71 deviation (83 on `visuals-b`), 2-26's edge scan** — CLOSED 2026-09-18 (SE step 71 committed with it;
      00_order §7 step 4.1,
      `work_s34/antiportal_edge.py`): required half/scale 31.5-34.0 on `antiportal_955` at node scale 3.6; measured
      **30.50** (17.08 before, so the row moved 13.4 of the 14.4 it was short). NOT MET as written. Attributed cause,
      measured on both sides and reproduced by the verifier on its own capture, its own re-implementation and both grids
      (the brief's fixed 40-70.5 u port grid and the per-scale 24s-40s grid): saturation, not size. The port's rim is
      still additive white and clips at 255 over that level's bright ground (inside-band median (255, 255, 255), 54-66 %
      of its pixels at 255 in all three channels, outside band summing 217-221), so the summed step is a plateau of 509.5
      tied at halves 30.5 / 30.75 / 31.0 / 31.25 / 31.5 / 33.25 and the estimator's strict `>` keeps the first; the
      original's same ring does not clip over 30.5-33.5 ×scale (≤ 0.5 % of the band at 255 in all three channels) and
      reads 33.25. The ring's drawn size is correct: a clipping-immune estimator reads 32.10 ×scale there against the
      original's 31.82-32.03 on six library frames, and the outermost-changed-pixel reading gives 32.06. The gate band
      also misses ground truth — 4 of the original's 30 frame readings by the same estimator fall below its own 31.5
      floor (1-10 at t2.0/t4.5/t8.0, 2-26 at h6.3). Next: step 4.2 (Blink and spin) removes the cause, because its own
      gates put c_G and c_B ≤ 0.03 so the rim stops being additive white — but 4.2's §7 gate list names only 3-12, 4-02,
      4-07 and 1-07 and contains no size re-read, so an explicit 2-26 `antiportal_955` edge-scan re-read has to be added
      to 4.2's gates (or the row's wording amended in 00_order §7: last maximum, tie midpoint, or a brightness-independent
      reading). Without that addition this NOT MET row has no owner and would silently disappear.
      **Closed by SE step 72 (order 4.2) as a measurement, not an amendment**: with the ring drawn red its rim
      stops being additive white and the same scan reads **33.25 ×scale** on `antiportal_955` — inside 31.5-34.0
      and the original's own reading on that ring (33.25 in five of its six library frames), `c` there now
      (0.305, -0.009, 0.000). The other four readable rings of that gate read 33.25 / 32.00 / 32.89 / 33.00,
      all in band; 1-10 `638` still has fewer than 8 sectors on screen. `out/parity/visuals/4.2/work/edge_after_42.json`,
      reproduced independently by 4.2's verifier at 33.26 on its own capture. No amendment to 00_order §7 4.1 needed.
- [ ] **Step 72 deviation (84 on `visuals-b`), the port draws 1-07's antiportal ring BEHIND art the original draws
      it in front of**
      (SE step 72, order 4.2; 00_order §7 step 4.2's 1-07 `antiportal_687` row, required max `c_R` 0.36-0.42 and min
      0.18-0.24, measured **0.2875 / 0.1786** — NOT MET as written). **The row fails on draw order, not on colour.**
      Measured three ways on the step's own frames. (a) Sector by sector: 15 of the ring's 28 readable sectors carry no
      blink step in the port and 4 carry none in the original — **12 dead in the port alone**, the 10° sectors 130-150
      and 200-280. The rule the count comes from, since it moves with the rule: the port's f378 minus f360 against the
      original's `1-07_t8.0.png` minus `1-07_h2.8.png`, in the gate estimator's inside band ([0.90, 0.95] × the decoded
      half) about the node at camera (0, 0), read as the red excess `R − (G + B) / 2`; readable = both pairs put ≥ 6 px
      in the sector (28 of 36), dead = a step under 3 grey levels (the gate's stricter readable rule gives 26 sectors
      and the same 12). The discriminating test — the original's spread over all six of that level's library frames, per
      channel — puts 8 of the port-dead sectors (140, 150, 210-260) at 16-25 grey levels in RED with green and blue at
      exactly 0; the same test finds 0 such sectors on 4-07. (b) Four single points in the band, over
      `single_block_plat_ent_784` (sector 270), `block00_ent_702` (225, 240) and `platform_ent_7` (140), are byte-equal
      across the port's f378, its f360 and its white before-frame, while the original's two frames differ there by +8.9,
      +8.7, +9.0 and +9.6 in red alone — the step its clear sectors show (+8.2). (c) By eye
      (`4.2/verify1/crop_107.png`): a complete annulus in the original, an annulus with the blocks cut out of it in the
      port. The loss predates step 72 — the same estimator read this ring's WHITE ring at 0.3964 where an unoccluded one
      reads 0.93-1.00 — and what the port draws is not low: the ring's own blink amplitude on the difference frame is
      0.2135, 107 % of the decoded 0.200. **The port's side is traced**: `Sprites::Find` orders a level's pictures by
      `z_index` then by the scene file and `buildSprites` draws each at `SlotZ(sprite.order)`
      (`MagicPortalsLayer.cpp:2302`), so at `z_index` 0 the file decides and the ring (12th node of `level6.tscn`) goes
      under `platform_ent_7` (16th), `block00_ent_702` (20th) and `single_block_plat_ent_784` (23rd). **The original's
      side is named and NOT settled**: Ethanon keys one multimap by a draw hash that for a type-0 entity is its z
      normalised over the level's height range (`ETHEntityRenderingManager.cpp:68-73`, `:119-131`,
      `ETHSpriteEntity.cpp:611-630`, `ETHScene.cpp:277-279`, `:531-532`), so equal z ties and the tie falls to insertion
      order — visible buckets row by row then the bucket's load order (`ETHScene.cpp:542-576`,
      `ETHBucketManager.cpp:81-91`, `:277`). On the `.esc`'s own coordinates with the default 256 u bucket that predicts
      two of the three blocks; on positions scaled to the screen it predicts all three, but no entity of `level6.esc`
      carries the `scalable` custom data `SGlobalScale::scaleEntities` needs. It is not systematic: 4-02 loses no sector
      in either build, 3-12 `1376` loses ten in each (two sectors apart, its 4 % larger ring), 4-07's 11 dead sectors are
      a subset of the original's 14. The brief's fallback ratio carries **no verdict** here: 4b(5) offers it "if the
      port's max reads about 10 % low on a ring whose white ring read 0.9 today", and this one's white ring read 0.3964
      and its max is 28 % low, so its 1.61 is a reading and not a gate measurement. Note also that the 0.36-0.42 band was
      set from "the rings that reached the ceiling in the original" — 3-12, 4-02 and 4-07 — with 1-07 added "plus", so it
      was never established on this ring. Next: settle what the original orders two type-0 entities at z 0 by (and
      whether the engine buckets the file's coordinates or the drawn ones), then re-read the row **as written**; sweep
      the other 51 levels that place a ring for the same divergence. Owned outside 4.2 — a colour step cannot move a
      draw order, and the row is not to be re-specified to a cover-immune reading, which would hide exactly this signal.
- [ ] **Step 72 deviation (84 on `visuals-b`), 4-07's gate ring reads low under art BOTH builds draw in front of it**
      (SE step 72;
      00_order §7 step 4.2's 4-07 `antiportal_2166` row): required max `c_R` 0.36-0.42 and min 0.18-0.24, measured
      **0.2679 / 0.1089** — NOT MET as written. **An offset, not a gain**: the ring's blink amplitude on the difference
      frame is 0.1786, 89 % of the decoded 0.200, and the f378−f360 read there is R +24.59 with G/B at −0.03/−0.06. The
      cover is faithful, not a divergence — that node carries `eth_z` **−12** (`level6c.esc`, `out/levels/level6c.tscn`
      `z_index = -12`), so the ring sits behind that level's z 0 art in the original by depth and in the port by slot,
      and **0 of its 36 sectors is dead in the port alone** (the port's 11 dead sectors are a subset of the original's
      14). The gate rings that pass read 0.3964 / 0.2135 (3-12 `1376`) and 0.3964 / 0.2178 (4-02 `2074`). The brief's
      fallback ratio carries **no verdict** here either: its precondition wants a white ring that read 0.9 and a max
      about 10 % low, and this one's white ring read 0.8604 and its max is 33 % low, so its 2.46 is a reading and not a
      gate measurement. **What is NOT explained**: the brief's census reads this same ring at 0.377 on the ORIGINAL's
      frames with the same estimator — the same art in front of the same ring — so the port's background around it is
      not yet the original's. Next: re-read the row once chapter 4's lighting, its effects and the texture tier (steps
      2.2, 7.x, 11.x) have landed, since all three move that background; do not re-specify it to a cover-immune reading
      while 1-07's draw order is open.
- [ ] **Step 72 deviation (84 on `visuals-b`), `c_G`/`c_B` over its bound** (SE step 72; same gate): required `c_G`,
      `c_B` ≤ 0.03, read on
      **magnitude** (the decode gives green and blue exactly 0, so either sign is a colour the original does not draw).
      **NOT MET on 2 of the 4 rings the gate names** — 3-12 `1376` `c_B` **+0.0349** and 4-07 `c_G`
      **−0.0436 / −0.0349 / −0.0349** (min/median/max) with `c_B` −0.0349; 1-07 and 4-02 are inside. Of the six rings
      read, 3-12 `1533` `c_B` **+0.0392** also misses, leaving 3 inside. One grey level is 1/229.55 = **0.00436 in `c`**,
      so these are 1.1 to 3.1 grey levels over. **Cause: the background's own step at that radius, not the ring**, and
      measured three ways rather than asserted — on the f378−f360 difference the ring's green and blue are exactly
      0.0000 on all six rings; 3-12 `1375` and 4-02 read `c_G` = `c_B` = 0.0000 exactly although their rings are drawn
      there; and `test_mp_layer` reads the quad's own `albedoColor` on 80 ticks and fails on any non-zero g or b. All
      three residuals are constant over a whole period (green moves ≤ 0.0087, blue 0.0000, against a red that moves
      0.074-0.183). Next: say in 00_order §7 4.2 whether the bound is signed or on magnitude, and bound the channel's
      **movement** over a period (0.0087 and 0.0000 here) rather than its absolute level, which the background sets.
- [ ] **Step 72 deviation (84 on `visuals-b`), the census sanity check's band** (SE step 72; 00_order §7 4.2 carries
      systems_3_4 4b(5) as a
      "sanity check only"): required port `c_R` in [0.15, 0.42] and `c_G`, `c_B` ≤ 0.06 over the camera-confirmed rings.
      Measured **17 of 22 in band** under the step's matched-filter rule (median 0.259, p90 0.322, max 0.322, G/B within
      0.06 on 21 of 22) and **22 of 33 rings on 27 levels** under the wider quad-containment rule (median 0.233, G/B over
      0.06 on 2) — NOT MET, **every miss low, none high**. The counts move with the admission rule and must be quoted
      with it: the 22 are the rings whose matched-filter peak sits within 12 px of their node (`census_42.py`); the 33
      are every level whose ring quads at the nodes contain every changed pixel (`4.2/fix1/work/census_f1.py`). Cause:
      the estimator and the art in front of the rings — at f420 every ring is drawn at the same `c_R` 0.3333 (7,000 ms,
      leg 23, bias 0.667), the best-read rings reach 97 % of it, and four of the five low rings read 0.000-0.196 on the
      WHITE ring too (1-21 `822` and 4-30 `ent_2005` read exactly 0 in BOTH builds). **On 1-07 that art is art the
      ORIGINAL draws the ring in front of** (the first item above), so an unknown share of the low rings may be that same
      divergence rather than the estimator — that item's sweep of the other 51 ring levels is what separates them. The
      brief itself calls this check "too loose to catch a wrong implementation" and the original's own census p10 is
      0.115, under the band's floor. Next: state the admission rule inside 4b(5), and replace the per-ring band with a
      check on the readable rings' upper envelope (or exclude rings that read ~0 on a white ring). **It stays a sanity
      check and must not be promoted to a §7 gate.**
- [ ] **Step 72 follow-up (SE main; step 84 on `visuals-b`), the 4.1 rim row's estimator**: `antiportal3.py`'s rim /
      scale (systems_3_4 4a(5)), required 28-33, reads **15.56** on 1-10 `574` and **23.20** on 1-12 `574` on the
      merged build (15.56 and 19.20 on step 72's own sweep, `4.2/work/edge_after_42.json`): its median-luminance peak
      stopped landing on the ring once step 72 drew it a dim red. The 00_order 4.1 gate, the edge scan, passes on all
      five rings (33.25 / 31.75 / 33.61 / 34.00 / 33.25). Recorded in SE step 74's RECORDED DEVIATIONS. Next: point
      the rim estimator at the red channel, or retire it in favour of the edge scan.
- [ ] **Step 72's gate instrument, `blink_42.py`, reads about the node's pixel index** (found by SE step 74's third
      verification). On 4-02 `antiportal_2074` it reads `c_R` max 0.4269 there and 0.3964 / 0.2178, in band, about
      the ring's drawn centre, (+0.40, -1.20) px from the index; and its matched-filter camera check returns corr 0,
      unnoticed, when before = after. Next: the step that re-reads 4.2 locates each ring's drawn centre from its own
      blink (point symmetry of the red channel's temporal s.d., `visuals/merge_tiers/verify3/centre_refine_v3.py`),
      refuses a corr-0 camera, and re-reads step 72's rows there. 1-07 and 4-07 stay out of band at every centre
      within 1 px (max ≤ 0.285), so their deviations above stand.
- [ ] **PORT-WIDE: the original's script clock is a FLOOR of each frame's milliseconds, and the port's is not**
      (decoded in SE step 72; it also answers step 68's "the whole milliseconds `getLastFrameElapsedTime` returns … are
      not decoded"). The Android loop measures a frame as a float of milliseconds (`ComputeElapsedTimeF`, gs2d
      `Application.cpp:36-50`, off `clock_gettime(CLOCK_MONOTONIC)`: `AndroidGLES2Video.cpp:66-69`), caps it at 1000 ms
      (`Min(1000.0f, …)`, `android/main.cpp:180-181`), and `ETHEngine::Update` stores `static_cast<unsigned long>` of it
      (`ETHEngine.cpp:147`) — confirmed in the SHIPPED library: `gs2d::ETHEngine::Update(float)` at `0xb4100` calls
      `ETHDrawableManager::RemoveTheDead` (+0x0e), then `__aeabi_f2uiz` at `0x1cb364` (+0x14) and
      `ETHScriptWrapper::SetLastFrameElapsedTime(unsigned long)` at `0xf2038` (+0x18). The C++ scene keeps the float
      (`:166`); the SCRIPT only ever sees the truncated `uint`, which both `STimeManager::getLastFrameElapsedTime`
      (listing bytes 7894..7961) and `STimeManager::unitsPerSecond` (8045..8162) read. **So at a steady 60 Hz every
      scripted motion in the original advances 16 ms a frame where the port advances 1000/60: the port runs 4.17 % fast**
      (a 600 ms antiportal period against 625 ms of wall clock; opposite phase after 450 ticks, 7.5 s), and the rate is
      frame-rate dependent in the original (`floor(33.3)` loses 1 %, `floor(16.7)` 4 %). It touches the antiportal blink
      and turn, every `Motion` the port steps, and the timed crystal's dial. **Not changed in step 72 on purpose**:
      flooring the one clock every scripted motion reads moves every committed step's captures. Next: the owner's call on
      whether the port floors its script-facing delta and at which frame rate the original is to be matched, then one
      re-capture of the committed baselines.
- [ ] **PORT-WIDE, found verifying step 78 (order 3.4): the picture of any body is drawn one tick behind the sim.**
      `buildSprites` gives every body-linked quad an `InterpolatedTransformComponent` (`game/MagicPortalsLayer.cpp:2590`)
      and `InterpolationSystem::Apply` draws `previousPosition -> currentPosition` at the frame's alpha
      (`engine/src/core/InterpolationSystem.cpp:140-149`), which is 0 under `--fixed-step` - so the picture shows where
      the body stood a tick ago, while a debug box (`makeBox`, `:2134`) carries no such component and shows the tick's
      own state. **The original draws the moved position in the SAME frame**: `SetPositionXY` is a `b2Body::SetTransform`
      inside `Update` and the draw reads `GetPosition` at draw time (`ETHRenderEntity.cpp:106`; D10_check §4a).
      MEASURED on step 78's two captures of one run schedule (the player's drawn left edge holds a constant -9..-11 px
      offset between art and box mode, walking and at rest, so the two runs map ticks to frames identically): the
      picture equals the box exactly one frame late in both directions - unchanged at f90 and 0.516 u at f91 against
      the box's 0.527 u at f90 on the press, at rest at f180 against f178/179 on the rise. It is 16.7 ms on EVERY
      body-linked picture in the port, not the button's alone, which is why step 78 recorded it rather than changing
      it. Next: the owner's call on whether a body's picture is drawn at its tick's own position, then a re-capture of
      the committed baselines - it moves any frame taken while a body is moving.
- [ ] **Step 79 deviation (order 13.2): the exit's sensor does not move with the bobbing door.** `linearMotion` ends in
      `SetPosition` (ins 180-208), and `door.ent` is `static="1"` with a `<Collision>` (Position (0,20), Size (8,57)),
      so the original moves the exit's sensor body with its picture on all 18 `no_gravity` levels. The port bobs
      pictures alone - `syncSprites` adds the offset to the quads and `Goals::State::exit` stays where the level put it
      - which is what `sim/Motion` does for every other `linearMotion` (the crystal's and the key's pick-up ranges,
      step 69). One unit either way on a box 57 units tall changes by a frame or two WHEN the exit is reached, never
      WHETHER, so it fails no gate of step 79 and was recorded rather than built. Recorded in `motions.json`
      `door._not_built`. Next: the owner's ruling, proposed beside R21 (which moved the button's body under its
      picture) - either offset the exit trigger by the bob each tick, as `SetPosition` does, or rule that the port's
      scripted motions move pictures only.
- [x] **Found building step 80 (order 13.4): `--tap` cannot fire a portal shot, so no capture can light a torch.**
      **CLOSED by step 87 (`5a60733`) with T.1 (step 100, `87aa67f`).** Step 87 had already moved `readInput` onto the
      tick's `Touch`, read before `helpBlockInput`, so a `--tap` fired the shot on every level - T.1's first
      measurement, on the committed build: `--level level28c --tap 247,141@60` lights 4-29's torch (the wall going on
      tick 71). T.1 then made the last two roads one: a scheduled tap on a HUD control is spent on the control, and its
      pixels are the viewport's in any window. So 7.2's, 7.4's and 10.1-10.4's capture gates no longer have to rest on
      the suites, and 10.1 to 11.2 took `--tap` captures. What a `--tap` still cannot press: the pause's and the end
      screens' buttons (the "Left by T.1" row). The row as it was:
      `readInput`'s shot reads `Input::MousePosition()` and `ViewportInfo::pointerOverGame`
      (`game/MagicPortalsLayer.cpp:4996-4999`), which a scheduled `--tap` never sets - it fills this layer's own
      `Touch` alone - and during play that `Touch` is not even built on a level with no help block: `helpBlockInput`
      returns before `touchThisTick` when `m_helpBlocks` is empty (`:5831-5834`). So `--tap` is inert on 115 of the
      128 levels, all six with a light wall among them. MEASURED: five runs of 4-26, 4-29 and 4-31 that tapped the
      torch's own screen point at ticks 27, 60 and 100 produced no `DEV tap down` line and no lit torch. Step 78 met
      it as "a `--fixed-step` run cannot tap"; step 80 has the mechanism. It blocks the capture gates of 00_order §7's
      7.2, 7.4 and 10.1 to 10.4 as surely as it blocked 13.4's, which is why those steps' gates must rest on the
      suites until it is fixed. Next: carry the scheduled tap's `Touch` into the shot and call `touchThisTick` during
      play on a level with no help block - a change to `readInput`, the path every level's play goes through, so its
      own step with its own re-capture.
- [ ] System 4 antiportals: drawn 64×scale u (not a flat 128 u) — **built, SE step 71**; the red blink (0.2 → 0.4 → 0.2
      on 300 ms legs) and the 10°/s turn — **built, SE step 72** (order 4.2), with the five deviations above. What is
      left of the system: the refusal radius stays 16×scale in the port against the decoded **32×scale**
      (128 u × the manager's `Scale(0.5)` × the node's scale, halved; footage F3 brackets the original's
      refusal at (31.6, 32.4]×scale). Ruled R1 — what the original does — but no step in 00_order §7
      plays it yet.
- [ ] **Gravity wells, the refusal (R2)** — the green ring is **built, SE step 73** (order 4.3, `6949cc3`, made as
      step 85 on `visuals-b`; drawn from `hd/white_ring.png` since step 74); the gameplay half is not. Decided for the
      original: its well refuses no portal and its shot does not die on one. `'gravity_area'` is in exactly two functions of the listing (`ETHCallback_gravity_agent` ins 29,
      `checkForLevelAchievements` ins 1542), and `AntiPortalManager` fills its `fields` from `Game::preLoop` before
      any agent has added its area (00_order §8.4, §8.14). The port still refuses: `Game::Start` hands each well to
      `Portals::State::zones`, and `test_mp_wells.cpp:312-313` assert it. 00_order §6 lists it as a gameplay hand-off
      in no batch. Next: drop the well zone, let a shot pass, flip the two assertions, with its own captures. It
      cannot move the ring: the picture is sized by `Well::RingSizePx` and the zone is derived from it.
- [ ] **Latent, found verifying SE step 73 (85 on `visuals-b`): `buildLights` counts a between-slot picture as a
      slot.** Its branch for
      a light with no picture of its own (`MagicPortalsLayer.cpp:3406`) counts every `m_sprites` entry at or below the
      light's z, and the gravity-well rings are the first between-slot pictures in `m_sprites`. No pixel moves today
      (four of the five well levels place no light; level27c's `fire_agent_ent_1919` has its own picture), but such a
      light on a well level would sit a slot too deep. Next: step 6.3's slot table leaves between-slot pictures out of
      that count, pinned in `test_mp_layer`.
- [x] **System 5 actors with sheets**: the minions and knights (62 markers in 35 levels, and the chapter-3 boss's
      escorts) are drawn on `hd/minion.png` cut as the original cuts it, on the decoded rows 1/2, behind the player at
      its decoded depth 4 (R10), on patrols that now arrive where the original's do - **built, planning step 76**
      (order 5.1; committed 2026-09-20 as MPR `9d25691`), with the "Step 76" rows below.
      The 2-32 dragon follows it: its own cell, its flight, its `Follower`-driven angle and R19's fireball, and
      its claw - **built, planning step 77** (order 5.2; committed 2026-09-20 as MPR `cbbf169`), with the
      "Step 77" rows below.
      The knights' dark aura, and the sim honouring the marker's `entityName` (R16), are built in planning step 97
      (order 11.4), the ghost that the knight's death adds (R4), its shot and its dropped key in planning step 98
      (order 12.2), and the dark dragon and its dropped key (R20) in planning step 99 (order 12.1, `848ed92`), each
      with its "Order" rows below. **Every actor of the system is now drawn**; what is left is in those rows and in
      R24 / R25's.
- [ ] **Step 76, THE OWNER'S TO RULE: two sim changes applied under no adopted ruling** (proposed as **R24** the
      patrol's arrival point and **R25** a carrier's point, appended to `00_order` §8.15 by 5.1's third round;
      neither is one of R1-R23, and the step's first record called them "the same standing ruling", which was an
      extension by analogy). Both are the decode's (R24: `ETHCallback_minion` ins 369-374, 413-425, 198-210;
      R25: `ETHCallback_key` ins 744-753, 932-957, 959-976), and both change gameplay, not only pictures.
      **Why they were applied anyway:** 5.1's own gates G5a-4 ("holds on column 0") and half of G5a-8 ("MAE <= 10 at
      the holds") are unmeasurable without R24 - with the arrival measured from the body's centre the port serves no
      hold on any level - and R25 was forced by R24 (a knight reached a key for the first time and carried it at its
      boots). **What refusing them costs:** R24 returns those gates to NOT MEASURED and puts the 30 Hz row strobe
      back; R25 leaves the key at the boots on 11 levels; it moves 11 f420 frames but moves none of them
      across the non-regression rule (MEASURED: the sweep of the binary with R24 in and R25 out has the same 61
      changed frames and the same 15 carrying an out-of-cell change; that sweep's 128 pngs open and match their
      logged md5s). The two rows below record what each one did.
- [x] **Step 76 finding, fixed in step 76: no patrolling minion arrived at a waypoint.** `Minions::Tick` measured the
      8 u arrival from the body's centre, 9 below the waypoints the levels place at the original's entity height, so
      every patroller crossed its first waypoint and turned on every tick after: once drawn, a 30 Hz row 1/2 strobe
      (MEASURED: 81 flips in 81 frames on 3-23, `5.1/verify1/turn`). Now measured from the entity point, the body's
      centre less 11.1 (`minions.json` `body.entity_above_feet_px` 18.1 less `radius_px` 7): over 1,083 tracked
      knight-frames the facing changes only where a hold ends, and G5a-4 and G5a-8 are measured on captures. The
      patrols' gameplay changes with it, each as the original plays it: knights carry the keys on their routes (11
      levels), press 3-15's button and weigh 3-22's seesaw. Step 76's non-regression row (changes only in knights'
      and the player's cells) is therefore NOT MET as written on 10 of those levels' f420 frames, recorded there.
- [x] **Step 76 finding (the second verification), fixed in step 76: a knight's carried key rode at its boots.**
      `Carry::Acquire` and `Carry::Trail` measured the pickup and led the key from the carrier's body centre;
      `ETHCallback_key` measures it from the carrier's `GetPositionXY` (ins 744-753) and leads the key to that same
      point (ins 932-957 the leash and `forceFollowUpPosition`, 959-976 `followUp`) - the ENTITY's point, 11.1 above
      a minion's body centre (`dark_mage.ent` carries `minion.ent`'s compound). Invisible until the arrival fix let
      a knight reach a key; then the key stood at its boots on 11 f420 frames. Now `Carry::Carrier` carries a body
      AND its point, and `Game` offers each minion at its centre less `Minions::Rules::EntityAboveCentrePx()`.
      MEASURED (`out/parity/visuals/5.1/fix2/work/key_fix2.py`): the key's yellow inside the knight's cell falls
      45 -> 26 px on 3-10 and 136 -> 28 px on 3-17, and its centroid from 19.5 and 23.1 u below the cell's centre to
      2.4 and 3.5 u above it, against 1.5 u below in the original's `3-10_t8.0` and `_h6.3` - the edge behind the
      shoulder the original draws, with a residual of 3.9-5.0 u in height where the error was 21.0-24.6 u.
      `test_mp_keys` pins the point (124 checks).
- [ ] **Step 76 deviation, the player's carrier point**: the player is offered to keys and shock diamonds at its
      capsule's centre, **22 u over its feet** (`data/player.json`'s 20 x 44 body, the remake's guess), where the
      original's entity point stands **18.1** over them (`dark_mage.ent`'s compound, `ETHCompoundShape.cpp:66-69`).
      So a key or a diamond the player carries rides 3.9 u low, and both its 26 and the diamond's 30 are polled from
      3.9 u low. Not applied here: the capsule is the player's body to settle, not the carry's. Next: the player's
      body, with `Carry::Carrier`'s point already in place to take it.
      **Evidence from the batches 8-13 run, nothing changed**: 10.1b (step 105) finds the standing player DRAWN 9.75 px
      (3.47 u) higher on the screen than the original's on 1-03 and 1-07 (cell 8 placed by correlation at top-left y
      484.75 against 494.5 on nine lossless library stills and F2 / F3 #20; the scene aligned at (0, 0)), against the
      3.9 u (11 px) the capsule predicts, a residual of about 1.2 px not investigated. The same point also places
      `projectile_shoot` and 10.1's tap ray (step 103, deviation 2), the ghost's aim (step 103, deviation 7), the ring's
      `shock_death` and the strike's midpoint (step 108, deviation 9), 11.3's sparkles and blood, and 7.3's
      teleport effects: each moves with the player's body when it is settled.
- [ ] **Step 76 deviation, a shock diamond's prey point**: `Diamonds::Tick` measures its 30 to a minion from that
      minion's body centre; `ETHCallback_shock_diamond` ins 421-430 measure it from the minion's `GetPositionXY`,
      11.1 higher, which is about 3.7 u more reach on a shared floor. Next: Diamonds, with the same point.
      (10.4, step 108, deviation 1: the strike's effects now go at the minion's ENTITY point, as `GetPosition()` is,
      while the reach is still measured to its body centre - gameplay, unchanged.)
- [ ] **Step 76 deviation, the lone guards' coast** (beside R5): the original's guard is frictionless (`minion.ent`
      friction 0), so once `shouldntMove` stops driving it at the wider 16 it coasts on to its 8, arrives, adopts the
      999999999 hold and is braked to a stop on column 0. The port's body grips (`kBodyFriction` 1) and stops about
      where the drive stops: the guards of 3-01 and 3-29 stand **15.1 and 15.2 u short of their posts at f420, on a
      stride cell (cell 11)**, and never serve a hold (measured on 2 of the 6 lone guards; the other four stand where
      the capture, read at camera x 0, cannot place them). Next: a frictionless minion body, after reading what the
      engine's friction combine gives against a floor of 1.
- [ ] **Step 76 deviation, G5a-5 lit R >= 60**: required R >= 60 inside the mask of every lit knight on 3-05 and 3-23.
      On the brief's frames (f270-f420) 18 of 18 pass (R 60.2-67.9 on this round's captures, 60.2-68.0 before
      them) - **by two tenths of a grey level at the bottom, so the row is not headroom in its own window
      either** (the third verification reads that lowest detection at 60.16); 6.3 and 11.4 must not read
      >= 60 as slack. On the widened series (f120-f840 every 30),
      **one detection sits ON the line and the result flips with the sub-pixel centre the mask is placed at**:
      3-05 f210 reads **59.9** at the step's centre (79 of 80) and **60.02** at a render-model best centre
      (80 of 80 - the verification's reading, re-measured by the third round: 60.02,
      `out/parity/visuals/5.1/fix3/work/g5_ratio3.py`). It is a stride cell 5 (row 1, column 1), whose own texels
      average **60.0** inside the same alpha >= 200 mask (columns 0 and 2 67.2-67.3, column 3 63.7), and **the
      original's own frames never show a column-1 cell** - its eight lit knights on 3-05 and 3-23 stand on columns
      0 and 2, texels 67.17-67.38 - so the 60 was set where nothing can fail it. **The measured residual, which the
      gate does not capture:** the original draws its lit knight at **1.019-1.023x** its cell's own texel mean
      (drawn R 68.61-68.87, 8 of 8 library knights) where the port draws at **1.000-1.001x**, about 2% dimmer on
      every cell - which is the whole reason a column-1 cell lands on 60. Next: the owner's call whether the row
      reads standing cells only, or R at or above 0.98x the drawn cell's own texel mean, and whether the 2% is
      worth chasing - a gate re-spec, not a tuned threshold.
- [ ] **Step 77 deviation, two clock bases where the original keeps one.** The port feeds
      `Dragon::State::elapsedMs` the tick's 16.667 ms while `Follower::Update` takes `Follower::FrameMs`'s truncated
      16, so the two drift about 4% apart - at 2-32's f240 shot the follower has counted 3,840 ms against
      `elapsedMs`'s 4,000. The original has ONE basis: `elapsedTime` is advanced by
      `g_timeManager.getLastFrameElapsedTime` (`ETHCallback_dragon` ins 277-289, `AddToUInt`), and that function
      (`all_functions.txt` bytes 7894..7961, ins 3-9) is `uint(GetLastFrameElapsedTime() x m_factor)` - the same
      truncated engine millisecond the follower gets, at factor 1, itself about 4% slow against the device's wall
      clock. Step 77's first draft recorded the split as "the original's own arrangement"; that was wrong, and its
      record and `dragon.json` now say so. **No gate measured in step 77 moves with it.** NOT applied there because
      it is not local: feeding `elapsedMs` the truncated frame shifts level31a's whole schedule about 4% later and
      moves the f180 / f240 / f420 / f444 frames that step quotes, and the claw's sweep and the flight run on the
      tick too. Next: the owner's call on one basis for the module, then re-gate 5.2's schedule on it.
- [ ] **Step 77 deviation, the dragon's flight timing against the original's library frame.** On `2-32_h2.8` the
      original's dragon sits lower and further right than the port's at the nominal same time (f168), with no mouth
      fire - a descent/timing gap that predates step 5.2, whose flight numbers (45, -60, 25 px/s and the 48 ceiling)
      it does not touch; the clock row above is one candidate. Recorded so the next role does not rediscover it as a
      5.2 regression. That frame also does not discriminate the angle: with its flame muted its four cells score
      0.434-0.541 and a per-angle search gives 340 -> 0.462, 360 -> 0.510, 380 -> 0.581
      (`out/parity/visuals/5.2/verify1`), which is why the decode and not the frame is what the port's angle rests
      on. Next: measure the descent against the footage rather than the library frame.
- [x] **System 6 texture tiers**: hd/fullhd per file (fullhd skies are 512 u wide). Converter + port together, or sprites
      draw twice too big; the Sprite offset × scale trap applies. Built by orders 0.2, 2.1, 2.2 and 2.3 (SE steps 61, 65,
      66 and 67; MPR `e9611e4`) and step 74 for track B's two pictures. What is left is in those steps' RECORDED
      DEVIATIONS and LEFT FOR THE OWNER, for example the evenly split particle sheet (step 67, for 13.1) - **closed
      by 10.4 (step 108, `shock_strike.png`) and 13.1 (step 111, `tesla_shock_black_bg.png`)**.
- [x] System 7 spawned effect entities: teleport effects, player-placed portal spiral, pickup/explosion effects,
      `portal_fail*`, key/keyhole effects; carranca fireballs are tan placeholder boxes today. **Every item named here
      is built**: the effects runtime (6.2-6.3, steps 82-83), the fireball and its explosions (7.2, step 85), the
      teleport's three (7.3, 86), the placed portal (7.4, 87), the level end and death (11.3, 96), the marks and
      `portal_fail*` (10.1, 103), the crystals' (10.2, 106), the keys' and keyholes' (10.3, 107), the diamonds', rings'
      and signal's (10.4, 108), the walls', stones' and doors' (11.1, 109), the torch's, trail and lap (11.2, 110), the
      bosses' (12.1, 12.2). The effects the original adds that no step owns are listed under "WHAT IS LEFT" above.
- [x] System 8 light halos — landed in G5 (step 49); magnitude still open under §1.
- [ ] System 9 converter-carried per-placement data: scale, sprite frames, the levels' own particle systems
      (45 placements load the wrong data). **The particle systems are built by batch 8** (steps 89-92: 840 carriers,
      897 systems, each placement drawing only what its own `<Particles>` states) **and the scale by batch 9** (steps
      93-95: 33 scaled sprites, R9's bodies). "Sprite frames" is not re-checked here and keeps the row open.
- [x] System 10 small scripted motions: crystal bob, arrow sway (2.5 u, 1.52 s), timed-crystal fade. Built by orders
      3.1-3.3: the timed crystal's dial with no fade (SE step 68), the crystal and free-key bob in `sim/Motion` (69),
      and the sway of the arrow, dashed circle and tapping hand at alpha 0.55 (70). No recorded deviations; owner items
      are in each step's LEFT FOR THE OWNER, and the script clock is the PORT-WIDE row above.
- [ ] **StaticSky** (camera-pinned sky) not ported → **34 levels show black ground** (found at G2).
- [ ] Placeholder boxes draw darker since the display-encoded scene.
- [ ] Footage to record (plan §4): F1 walk left, F2 place + enter a portal, F3 failed shot / antiportal, F4 crystal bob,
      F5 knight patrol, F6 carranca fire + barrel, F7 2-32/4-32 in-level frames, F9 key pickup, F10 zero-g recoil.
      (F8 antiportal pulse is RETIRED — SE step 72 decoded the blink instruction by instruction, and no still can show
      the phase the original starts at anyway.)
- [ ] Cannot be known from footage (needs the bytecode decode): the shot frame timer, which script spawns
      `appear_effect.ent`. The portal tint is DONE — `ETHCallback_portal_static` sets vector3(1, 0.3, 0.3) or
      vector3(0.3, 0.3, 1) by the placement's `color` string (SE step 59's `art.json static_portal`), and the
      `_portalColorA/B` globals this row named turn out to be the ANTIPORTAL's — vector3(0.2, 0, 0) and
      vector3(0.4, 0, 0), the blink's two ends (SE step 72).

- [x] **Order 6.1 deviation, K14: the shot is still drawn IN FRONT of the player, and the merge did not re-derive it**
      (step 81). **CLOSED by 10.1 (step 103, `7ac69b1`), re-taken after the level's opening by 10.1b (step 105).**
      `Slot::ShotPicture` -0.515625 and `Slot::ShotHalo` -0.5078125 are now the between-slot table's first two rows,
      drawn at `slotAfter(shot.z = 0)`: behind the player (R10's 1.42) and behind a minion where they share a gap.
      Measured: with the shot at the player on 1-07 (taps of ticks 200 and 290), 5,518 and 5,502 px inside the
      player's box differ, every one darker - the body now covers the shot and only its blue sparks show at his back,
      as F3 #27 and #206 show the original; 10.1b reads the same after the opening on 1-07 and 1-03, and the launch's
      colours against F3 #27 and F2 #26 (4,014 px, 77 % behind the player's middle, against F3's 4,021, 75 %). Its
      price is recorded (the "Order 10.1" rows): the table orders a gap by class, so a -0.5 class with its own z below
      0 is drawn over the shot where both share a gap. The row as it was: 00_order's K14 asks where the shot sits against the player. `addProjectile` adds it at **z 0**, and
      R10 puts the player at **z 4**, so the original draws the shot BEHIND the player. Step 81 knowingly kept the
      port's order — a quarter slot in front of a player the port still placed at z 0 on track B's tree, which is
      Ethanon's order at an equal z for what is added later — and left it to "whoever lands second", naming 6.3's
      between-slot table as where it belongs. Track A then landed R10: step 76 put the player at the decoded z 4, and
      6.3 built the table. **Neither closed K14.** Measured in the merged source, and nothing else was measured: the
      call site is still `slotAt(m_playerSlot, Slot::ShotPicture)` (`game/MagicPortalsLayer.cpp:4349`) with
      `Slot::ShotPicture` at **+0.25** and `Slot::ShotHalo` at +0.5 in `kSlotTable`, so the shot is drawn a quarter
      slot in FRONT of the player's slot whatever the player's z now is; step 88's merge record does not mention K14
      at all. **NOT measured here**: whether any frame shows the difference (a shot only overlaps the player for the
      first frames of its flight), and nothing was rebuilt or captured to find out — a code edit would have relinked
      the binary the merge's 128-level sweep is measured on. Next: the step that owns the shot's depth re-derives it
      from the table, with a capture of a shot leaving the player on a level where the two overlap.
- [ ] **Order 7.2 deviation, the fireball light's tail** (step 85, committed on `visuals-c` as step 94 and renumbered by the merge). 00_order §7
      and system_7 G.2 require the R excess to fall to 10 by **27.6 +- 6 u**; measured **40.0 u** on 2-01, 6.4 u past
      the band, on the one sample of the pair whose rings no neighbour reaches. The method is not the difference: the
      same scripts read the ORIGINAL's own pair at r_half 16.9 u and <= 10 by 27.6 u, the numbers the band is made of
      (`out/parity/visuals/7.2/fix1/tail.json`, `7.2/fix2/tail2.json`). **The knob is the falloff's SHAPE, not the
      decoded range 50**: range 50, the colour (1, 0.5, 0.2) and `<Scale>` 1.2 are all read from `fireball.ent`, the
      step adds no light maths of its own (its light goes through `placeLight`, the rule every placement and torch
      uses), and in the 28-45 u ring the excess is (14.1, 7.7, 3.4) = 1 : 0.55 : 0.24, the `<Light>` colour itself.
      At 16.9 u the port reads R 112 against the original's 81 and 83, and at 27.6 u it still holds 30 % of that
      against the original's 9-11 %, so brightness alone does not account for it. **Owner: 2.2 / G5**, which already
      holds `lighting.json`'s `halo_brightness_scale 0.6 _guess`. Not tuned.
- [ ] **Order 7.2 deviation, K8's byte-identity and the player's eyes** (step 85). K8 asks that a step's frames be
      byte-identical outside the new picture's box. Measured over the 128-level f420 sweep: 107 frames identical, 21
      changed, and **20 of the 21 redraw the player's eyes - 1,499 px, of which 1,448 lie beyond the fireball's own
      60 u box** on 19 levels (14-92 px each), classified by overlap with the pixels step 84 itself moved, not by
      eye (`7.2/fix1/eyes_all.json`, `7.2/fix2/identity2.json`). The cause is the single `m_effectRandom` that K8
      adopted in 6.3: effects are built before the eyes' renewals are stepped, so the eyes take different draws.
      Left as it is on purpose - the original re-seeds ONE shared MTRand per frame (00_order §3 K8, D6), and a
      generator per effect would re-open 6.3's committed design and move 7.1's committed baseline. **THE OWNER'S to
      rule, and the ruling is inherited**: from 7.2 onward every effect step's K8 row means "the placement stream is
      untouched" (6.3's own K8 case - 1,200 emitter values across two levels, 0 different - still passes) rather
      than "the frame is identical", so 7.3, 7.4 and 8.x do not each re-litigate it.
- [ ] **Order 7.2 deviation, the comparison box is 60 u, not the brief's 40 u** (step 85). `system_7` G.2 gate (5)
      reads "each fireball +- 40 u and its explosions"; 00_order §7's own 7.2 row sets no radius, so this replaces
      the brief only. The 60 u is derived, not fitted: `fireball.ent`'s widest number is its `<Light>`, range 50 x
      `<Scale>` 1.2, which is wider than the 72 u halo quad (36 u half) and than the particles (20.4 u). The frames
      agree - on 2-01, 10,115 changed px sit in the 50-60 u ring about the nearer fireball and **1** in the next,
      and 2-05 has zero changed px in every 5 u ring from 15 u to 80 u. **It is the substitution that turns 2-01
      from a failure into a pass**: at 40 u that frame has 22,611 px outside the box. Recorded so the swap is not
      read later as a quietly different number.
- [ ] **Order 7.2 deviation, the fireball's core is met on its licensed sample and not as a population** (step 85).
      G.2 gate (2) wants a core equivalent radius of **6.0-10.0 u** and an RGB within +-20 of (226, 196, 165) from
      one run on a pair with "a fireball in neither frame's rings"; on that sample the port reads 8.94 u and (242.0,
      201.6, 145.2), so the gate is MET as written. Read wider it is not: the core is read on the RAW frame, where
      no neighbour can reach it (the nearest fireball in the same frame is 544-554 px away), and over a 210-frame
      series of 2-01 on the same binary one fireball's core moves **4.47-8.82 u, mean 6.28, s.d. 0.95, 39 % under
      the 6.0 floor**, with all 300 instance readings at 4.12-9.95, mean 6.39, 42.7 % under it, against the
      original's 7.71 and 8.30 u by the same code; and only 54 % of those readings hold all three RGB channels in
      band (blue 59.7 %). Not fragmentation - summing every bright piece within 45 px moves the mean 6.39 -> 6.50.
      **No decoded number is the knob** (`n5 a1 rep0`, size 17, growth -1.8 are all the `.ent`'s): what differs is
      how the port's particle runtime spreads five particles' phases and sizes, so this belongs with **System 2's
      particle row** above and the next effects step. `7.2/fix2/core_series.json`. Not tuned.
- [ ] **Order 8.1 deviation, `eth_particles` is read into the Look** (step 89). `systems_9_10` 9c (4) has the lighting
      reader know the key and not use it, and `buildEmitters` resolve it a second time against `m_paths.art`. Built
      instead: `Lighting::Read` resolves it once into `Look::particles` (`game/sim/Lighting.hpp:104`), `buildSprites`
      copies it into `DrawnSprite::particles`, and the layer loads exactly the file the reader checked - one parser of
      the key rather than two that could resolve it differently, on the same art root. A consequence 8.3 must rule on:
      a level whose lighting fails to read has no `Look`, so every node there takes the `.ent`; once 8.3 drops the
      fallback such a level would lose its particles with its lighting. No shipped level is unlit (128 of 128 read).
      **Ruled in 8.3 (step 91): it does.** One reader of the key stays; a level whose lighting read is refused draws
      none of its placements' particle systems either, and the warning says so ("drawn unlit, and without its
      placements' particle systems", `game/MagicPortalsLayer.cpp` `loadLevel`). It is a fault state the log names,
      not a play mode, and none ships.
- [x] **Latent, found verifying step 89 (order 8.1): the key is accepted on a node `buildEmitters` never visits.**
      `Place::Drawn` (`game/sim/Lighting.cpp:87`) takes a node that draws a sprite OR owns a light, while
      `buildEmitters` walks `m_sprites` only (`game/MagicPortalsLayer.cpp:749`), so a light-only node carrying the key
      would read and never draw - a skip, against the reader's "refused, never skipped" (`Lighting.cpp:62-63`). Nothing
      hits it: all 840 inline-particle placements in the level `.esc` files have a `<Sprite>`. Next, in 8.2: either
      the reader refuses `eth_particles` on a node with no sprite (and `test_mp_lighting` pins the refusal), or the
      converter writes the key only on a node with a `Sprite2D`. **8.2 (step 90) took the second: the converter writes
      the key only on a node it writes a `Sprite2D` for, and reports a carrier without a sprite ("draws no sprite;
      not carried", pytest `test_a_carrier_without_a_sprite_is_reported_and_not_keyed`) instead of keying it; none
      ships, and the regenerated levels carry 840 keys, all on sprite nodes.** That report marks a PORT gap, not an
      engine rule: the original draws a spriteless entity's particles (`ETHSpriteEntity.cpp:106` calls
      `LoadParticleSystem()` whatever `m_pSprite` is). The reader still accepts the key on a light-only node, which
      no converted level has; refusing it there too stays open for 8.3, which owns the reader. **Closed in 8.3
      (step 91): `Lighting::Read` now refuses `eth_particles` on a node that draws no sprite ("eth_particles on a
      node that draws no sprite, whose particles the port does not draw"), pinned in `test_mp_lighting` (35
      refusals, 34 before); the port gap the converter's report marks is unchanged.**
- [ ] **Order 8.2 deviation, `metaStrings` is re-pinned in 8.2, not 8.3** (step 90). 00_order §7 lists
      "`test_mp_levels` `metaStrings` 17,774 (recompute per K10)" among 8.3's gates, but the count is read from the
      converted files, which 8.2 regenerates, so the suite would fail from 8.2 on if it waited. Re-pinned 16,934 ->
      17,774 (840 keys) in step 90; K10's "whichever lands second recomputes" is met there. Next: 8.3 re-confirms it.
      **Re-confirmed in 8.3 (step 91): `test_mp_levels` 128 checks, 0 failures, `metaStrings` 17,774.**
- [ ] **Order 8.2 deviation, the sidecar holds the systems the engine KEEPS, not the inline element as written**
      (step 90). `systems_9_10` 9c (4) says "keep the inline `<Particles>` element"; the converter writes only what
      `ETHEntityProperties.cpp:304-333` keeps (two systems at most, `particles > 0`), because the port's
      `Particles::Parse` reads every `<ParticleSystem>` and refuses one with no particles. On the shipped levels
      nothing is dropped (899 raw systems in 133 `.esc` = 897 in the levels + 2 in `loading_screen`), so the 897 are
      the same either way; a level that stated a third or an empty system would differ from the element as written.
- [ ] **Order 8.2 deviation, `--copy-particles` is a flag of its own** (step 90). 9c (4) offers it or
      `--copy-lighting`; particles are not lighting. The cost: the key is written with or without the flag, and a key
      naming no file is refused, which draws the whole level UNLIT (`game/sim/Lighting.cpp:87`, `:178-190`), so every
      run for the port must pass it - both READMEs say so. **9.2's regeneration must pass `--copy-particles`**, or
      every level with a carrier draws unlit.
- [ ] **Order 8.2 deviation, the sidecar's name hashes the block, not the whole file** (step 90). 9c (4) names the
      scheme (`<sha1-12>.ent`) and not what it hashes; the converter hashes the canonical block (one line,
      whitespace-only text dropped, attributes in the file's order). Found verifying it: the clash refusal
      (`ParticleBlocks.add`) holds within ONE converter run only - each `--world` run is its own process, and
      `write_sidecars` overwrites a same-named file an earlier run left with other bytes. At 48 bits and 24 blocks a
      clash is not expected; guarding it (refuse when the file exists with other bytes) is a converter change for a
      later step, recorded rather than built.
- [ ] **Order 8.3 deviation, the pixels this batch moves moved at 8.2, not 8.3** (step 91). 00_order §7's baseline
      rule has 8.3 move pixels on many levels. Once step 90 keyed all 840 carriers, the 14 frames batch 8 predicts
      moved there; what 8.3 dropped, the `.ent` fallback, reached no shipped level (0 of the 2,043 unkeyed sprite
      nodes has an `.ent` holding a system), so its sweep is 128 of 128 byte-identical to `8.2/after/sweep`.
      Superseded, not missed: `8.3/after/sweep` is still the next step's before, and no pixel is owed.
- [ ] **Order 8.3 deviation, `bluelight.py`'s gate was first crossed at step 90** (step 91). 00_order §7 gives the
      gate (orb-window pixels over 230 >= 1,000 on 1-05, 1-08, 1-22; 0 / 0 / 0 on 2026-09-16) to 8.3; step 90's
      sidecars already drew the orb, and its numbers were recorded there as a preview. Measured and gated in 8.3 on
      the same numbers: 2,442 / 2,299 / 1,877, against the original's 1,340-2,826.
- [ ] **Order 8.3 deviation, the rocket door's flames start at y 22, not 118** (step 91). `systems_9_10` 9c (3) gives
      level0c #2079 "sparkles x24 size 99 + fire x12 at StartPoint y 118"; in the placement's own `<Particles>` 118
      is the sparkles' start, and the 12 `fire.png` particles (a 4 x 1 cut) start at y 22. Pinned as the file says
      (`test_mp_sprites`); the brief's line is what is wrong.
- [ ] **Order 8.3 deviation, level7b #1335 carries two systems** (step 91). 00_order §7 names only the key's
      `Color0.b` 0.7; its block holds two `sparkles.bmp` systems, each `Color0` (1, 1, 0.7, 1). Both are pinned.
- [ ] **Found verifying 8.3 (step 91), for 8.4: the port draws the blue lamp's orb behind the bowl's rim.** On 1-05,
      1-08 and 1-22 the lamp bowl's front rim crosses the port's orb at f420, where the original draws the orb whole
      in front of it (`out/parity/visuals/8.3/verify1/orb_context.png`); the port's orb also reads a little smaller
      and bluer (window mean R 204.0 against 216.1 on 1-05; 199.3 against 212.6 on 1-08's t8.0 frame, its original
      frames reading 181.5-212.6). It is the `StartPoint.z` 24 depth 8.4 owns. Next: 8.4's gate compares the rim /
      orb draw order with the original's crop on the five `blue_light` levels, not only "the sweep changes only the
      5 `blue_light` levels". **Measured in 8.4 (step 92): not the depth.** The lift puts all five orbs after every
      picture at or below 24 (or 6, 10, 8) and after the player, and the orb windows on 1-05, 1-08 and 1-22 are
      byte-identical before and after it (0 changed pixels in the 60 x 60 window and in a 160 x 160 crop; 1-19's orb
      sits at the frame's top edge and 1-28's is off it, and those two frames are byte-identical too;
      `8.4/orb_84.png`, `orb_84.json`). No picture but the lamp's own bowl lies under any orb, and the orb already
      drew after the bowl, its own picture: what reads as the rim crossing the orb is the bowl's dark rim SHOWING
      THROUGH an added orb that is not bright enough there to wash it out - the original's orb is larger and whiter
      and does (its rings reach the bowl's lip; window mean R 204.0 / 199.3 / 188.7 against the original's 216.1 /
      212.6 / 194.9 on its t8.0 frames). The row stays open for that size and brightness, which is the orb's own draw (2c's added colour,
      `hard_halo.png`'s tier, the lamp's halo), not a depth: for the particles owner.
- [x] **Found in 8.3 (step 91), test-only: six `test_mp_layer` reads keep a pointer into a destroyed vector.**
      `ReportOf(layer.EmitterReports(), ...)` at `tests/test_mp_layer.cpp:3001, 3010, 3052, 3140, 3379, 6078` (the
      one at `:3379` in an `if` initialiser) points into the temporary `EmitterReports()` returns, which is destroyed at the
      end of the statement, and reads it afterwards. They pass because the freed memory is not reused first; step
      91's own new test did the same and read an emptied string (its first `test_mp_layer` run, 1 failure), and holds
      the reports in a local now. Not folded into step 91's commit, which changed text only (the fix costs a relink
      and a launch under Smart App Control). Next: the same fix in the six, as the first change of the next step that
      rebuilds `test_mp_layer`. **Fixed in 8.4 (step 92), its first change**: each of the six holds the reports in a
      local, and a deleted overload `ReportOf(std::vector<EmitterReport>&&, ...)` makes a seventh a compile error -
      the suite compiled with it, 0 warnings, so none is left. **Run in 8.4's independent check**: Smart App
      Control refused the build's `test_mp_layer.exe` twice, and the check's rebuild of the same source ran it, 1,872
      checks, 0 failures.
- [ ] **Order 8.4 deviation, the ten unlit torches are lifted too** (step 92). 00_order §7 gates 8.4 on "the sweep
      changes only the 5 `blue_light` levels", after `systems_9_10` 9c (4): "the only non-zero z among level placements
      is blue_light's 24". The census (`8.4/census/types84.py`) finds 15 carriers and 25 systems: the five lamps, and
      the ten `light_off` torches of 4-22 to 4-27 and 4-29 to 4-32, whose two sparkle systems stand at z 4 inline AND in
      their `.ent`, which is why 9c's inline-vs-`.ent` table never listed them. All 15 are lifted by the same rule; the
      sweep is read against the 15 (the table row).
- [ ] **Order 8.4 deviation, the vertical shift is not built** (step 92). An `ET_VERTICAL` owner's particle pieces sort
      a raw 10 world units deeper (`_ETH_PARTICLE_DEPTH_SHIFT`, `ETHParticleManager.h:30`, `.cpp:332-335`), 3.556 on
      the level's axis, and each such particle sorts on its own (`ComputeParticleDepth`, `:305-330`). The ten torches
      are the only vertical carriers (830 of the 840 are type 0), so their sparkles belong at `eth_z` + 4 + 3.556; the
      port draws them at `eth_z` + 4, because the converter writes no entity type. On 6 of the 10 levels a picture
      stands in between (-4 on 4-22, 4-24, 4-25; -6 on 4-29, 4-30, 4-31; `8.4/census/lift84.py`), which the original
      draws the sparkles over and the port does not. Next: a K10 pair with 9.2's regeneration - the reader accepts a
      type key (e.g. `metadata/eth_vertical`), then the converter writes it - and `buildEmitters` adds the shift. No
      name-keyed substitute is built.
      **Looked at in 9.1 (step 93) and not taken.** `ET_VERTICAL` is six rules, not one depth (formats doc 5.3):
      the particle shift, the draw hash's y term (`ETHEntityRenderingManager.cpp:132-134`), the halo a sprite's
      height deeper (`:80`), no rotation (`ETHRenderEntity.cpp:105`), the centre-bottom origin (already in the
      converter's `_vertical_offset`) and the vertical light and ambient shaders (`ETHShaderManager.cpp:110`,
      `ETHPixelLightDiffuseSpecular.cpp:62`, `ETHVertexLightDiffuse.cpp:57`). All ten torches apply light, so a type
      key read for the shift alone would be a lighting input the reader knowingly skips on every one of them, and
      refusing it on a lit node would refuse all ten levels. Next: the owner, or the owner of the torches'
      lighting, says how much of `ET_VERTICAL` the port draws; the key follows that. **Not taken in 9.2 (step 94)
      either**: its converter writes no type key.
- [ ] **Order 8.4 deviation, "after sprites with z <= 24" is owner-relative** (step 92). It holds as written where the
      lamp stands at `eth_z` 0 (1-05, 1-28); the rule is the owner's `eth_z` plus 24, so the orbs of 1-08, 1-19 and 1-22
      stand at 6, 10 and 8, and `test_mp_layer` pins each carrier at its own depth.
- [ ] **Order 8.4 deviation, the sweep moves no pixel** (step 92). 00_order §7 expects the lamp levels to change; at
      f420 nothing but added light lies between any of the 25 lifted systems and the 61 pictures (and the player) they
      now pass, so `8.4/after/sweep` is 128 of 128 byte-identical to `8.3/after/sweep`. The order is measured by the
      probe instead (`8.4/probe`: 2,628 px on 1-05, 1,307 px on 4-22, all brighter with the system on top; the
      independent check's, owners not at z 0, `8.4/verify1/probe`: 4,429 px on 1-08, 2,741 px on 4-22).
- [ ] **Order 8.4 deviation, the run's "bare literal" premise is not applied** (step 92). The instruction and step 86's
      hand-off had the start point's z divided by 2.8125; the engine scales it with the entity
      (`ETHParticleSystem.cpp:27-33`, `ETHParticleManager.cpp:90`, `ETHSpriteEntity.cpp:316-319`; the script's
      `scaleEntity` scales systems and position by one factor), so it is on the level's axis as the file states it.
      Dividing would have put the orbs at `eth_z` + 8.53 and the sparkles at `eth_z` + 1.42. `docs/ethanon-formats.md`
      7.4a.
- [x] **Found in 8.4 (step 92): `test_mp_layer` has not run on this step's source.** Smart App Control refused the
      freshly linked `test_mp_layer.exe` (exit 126) and refused it again after its one delete-and-relink, so the new
      `AStartPointLiftsAPlacementsSystems`, the widened table test and the six fixed reads are compiled (0 warnings)
      but not run; the lift was measured instead on the running game (`8.4/probe`, planning step 92). Next: the next
      step that relinks `test_mp_layer` runs it, and step 92's pins are read there (expected: the 15 carriers' lines,
      25 of 897 systems, 0 failures). **Closed in 8.4's independent check (step 92)**: the suite, rebuilt from the
      same source (`test_mp_layer.exe` `4a5748bd`), was accepted on its first launch and ran **1,872 checks, 0
      failures** (`8.4/verify1/test_mp_layer_run1.txt`): "15 carriers, 25 of the levels' 897 systems", one line per
      carrier at its own depth with the pictures it passes equal to the level files' census (61 in all), and "20
      classes, 11 of them between two slots, 3 spreading".
- [ ] **Found in 8.4 (step 92): every suite was relinked with no source change of its own.** A comment edit to
      `game/sim/Particles.hpp`, reverted with `git checkout`, left the header newer than its objects, so the step's
      build recompiled `Particles.cpp`, `Art.cpp` and the six suites' sources that include it (effects, lighting,
      zerog, timed, minions, sprites), and relinked `MagicPortalsSim`, all 45 `test_mp_*` executables and
      `MagicPortalsSpike.exe`. None but `test_mp_layer` was
      launched, so no refusal came of it here, but each is now a freshly linked binary: the next run of any of them
      may be refused on its first launch. `Particles.hpp`'s `startPointZ` comment ("not yet drawn with" for
      placements) is stale and left for 9.1, which rebuilds the sim library anyway.
      **9.1 (step 93) fixed the comment**, and its build relinked every suite again; its first launches of three
      of them were refused (planning step 93).
- [ ] **Found verifying 8.4 (step 92), older than it: a thin vertical line above the blue lamp's orb on 1-08 and
      1-22.** In the port's f420 frames a thin dark vertical line runs down to the orb on 1-08 and 1-22; the
      original's t8.0 and h6.3 crops show none (`out/parity/visuals/8.4/verify1/orb_v.png` and `8.4/orb_84.png`,
      rows 2 and 3, the port's tiles on the left). The frames are byte-identical to step 91's (`8.3/after/sweep`), so
      step 92 did not introduce it; its cause is not identified and no launch was spent on it. Low. Next: the owner
      of the lamp's and the background's pictures names the picture that draws it.
- [ ] **Order 9.1 deviation, the scale is validated in the reader** (step 93). `systems_9_10` 9a (4) checks it in
      `Sprites::Find` alone; 00_order §7 gates "zero or negative refused" in `test_mp_tscn`, which has no images.
      `Tscn::Parse` refuses a `scale` that is not two numbers above 0, or that is on any node but a Sprite2D, naming
      the line; `Find` repeats the form check for a scene built by hand. For 9.2: the converter writes it on the
      Sprite2D only.
- [ ] **Order 9.1 deviation, a placement's systems are made at the mean of its scale** (step 93). 9a (4) left
      particle owner scale to the effects; the engine makes a placement's managers at `(sx + sy) / 2`
      (`ETHSpriteEntity.cpp:316-319`, `ETHParticleManager.cpp:90`), and the converter writes the sidecars unscaled,
      so the port applies it (`buildEmitters`, one `Particles::Scale` call with the script's factor). None of the
      840 carriers has a `<Scale>`; pinned on the K7 fixture (the flame at twice its size, born at (288, 40)).
- [ ] **Order 9.1 deviation, no `imageSizePx` field** (step 93). 9a (4) proposed one beside `sizePx`; the
      lightmap check reads `Tiers::Units(texels, density)` from the sprite's own fields instead - the same number,
      and no field a picture the layer makes itself would have to fill.
- [ ] **Order 9.1 deviation, the entity-type pair is not taken** (step 93): the "Order 8.4" vertical-shift row.
- [x] **Found in 9.1 (step 93), for 9.2: `test_mp_levels` fails from the regeneration until re-pinned.** 00_order
      §7 puts `keys["scale"] == 33` in 9.3, but the counts are read from the converted files, so the step that
      regenerates (9.2) re-pins `keys.size()` 11 -> 12 and adds `keys["scale"]` 33, as 8.2 re-pinned
      `metaStrings`; 9.3 re-confirms.
      **Re-pinned in 9.2 (step 94) and compiled at 0 warnings, NOT RUN**: Smart App Control refused the freshly
      linked `test_mp_levels.exe` and again after its one delete-and-relink (exit 126 both times). The counts were
      taken instead from the regenerated files by a grep tally (`^scale = ` 33, every other key and `metaStrings`
      17,774 unchanged). Next: the next step that launches `test_mp_levels` runs it (expected 129 checks, 0
      failures). **Closed in 9.2's independent check (step 94)**: the suite, relinked from the same source
      (`test_mp_levels.exe` `56084a70`, 0 warnings), was accepted on its first launch and ran **129 checks, 0
      failures** (`9.2/verify1/logs/test_mp_levels.txt`).
- [ ] **Found in 9.1 (step 93): pictures the layer re-cuts do not take the node's scale.** 2-32's dragon is sized
      from `m_dragonCut` (and so is its report), and the minions and knights are drawn from their own state. None of
      the 33 scaled placements is one of them (census `9.1/census/types91.json`). Next: the step that scales one.
- [ ] **Order 9.2 deviation, joint anchors are scaled too** (step 94). `systems_9_10` 9a (4) lists size, offset,
      polygon, compound and radius; the engine normalises a revolute joint's anchor against the scaled box as well
      (`ETHRevoluteJoint.cpp:62`), so `joint_anchor_x/y` take the scale. No scaled placement has a joint; pinned in
      `tools/converter/tests/test_scale.py`.
- [ ] **Order 9.2 deviation, `trigger_size` stays unscaled for a stated reason** (step 94): a script's
      `GetCollisionBox()` returns the box as read (`ETHEntity.cpp:314-320`). 9a (4) left it unscaled as "untested". No
      scaled placement has a trigger box.
- [ ] **Order 9.2 deviation, a `<Scale>` not above 0 is refused** (step 94): the instance is skipped with a report
      line, since the port's reader would refuse the whole level. None ships.
- [ ] **Order 9.2 deviation, `test_mp_start` changed** (step 94). Its two tier checks in `EveryLevelsArtReads`
      compared `sizePx` - the units times the scale since step 93 - with the 1x file and the converter's
      `entity_tiers.json` units; on the regenerated levels they read 60 resized placements (not 28) and 33
      disagreements, 3 failures. They now read `Tiers::Units(texels, density)`, and a new pin counts the 33 scaled
      placements: 753 checks, 0 failures. Step 93 had not run this suite. No behaviour changed.
- [x] **Order 9.2 deviation, the `test_mp_levels` gate measured in the independent check, not in the build** (step
      94): Smart App Control refused the build's binary twice; closed in the row "Found in 9.1 ... `test_mp_levels`"
      above (129 checks, 0 failures).
- [x] **Found in 9.2 (step 94), for 9.3: `test_mp_play` reads `level30` alone.** R9's "`test_mp_play` must stay green"
      (00_order section 6) assumed a walkthrough of the barrel levels; the suite loads `level30.tscn` only, which no
      scale touches. `test_mp_start`'s "128 of 128 levels start, 128 play" is what loads every barrel level (it
      starts each and lets the player land); no suite plays a chapter-2 level to its end. Next: 9.3 says which suite
      carries R9's gate. **Answered in 9.3 (step 95)**: R9 is carried by `test_mp_fire` (level12a's three barrel
      bodies pinned at 28.6 x 37.7 u, and the chain it plays), `test_mp_demolish` (2-22's stone pinned at a circle of
      24 u) and `test_mp_start` (every level starts and plays; step 94's run, and again in 9.3's independent check:
      753 checks, 0 failures, 2,883 sprites, 33 scaled). `test_mp_play` stays in the gate as non-regression: 205
      checks, its output step 90's line for line. A walkthrough of a barrel level is not built.
- [ ] **Found in 9.2 (step 94): the engine's compound path ignores the collision box's position.** `CreateBody`
      passes `box.size` alone to `GetShapes` (`ETHPhysicsSimulator.cpp:92`), while the converter adds the box's
      position to each fixture's offset. Moot on the shipped data: 0 of the 10 compound placements has a position
      other than (0, 0). Recorded in the formats doc; not changed.
- [ ] **Found in 9.2 (step 94), older than it: 2-19's upper crate falls at the start in the port.** In the original's
      `2-19_h6.3` one crate stands on the ledge at the upper right; in the port's f420 frames (before and after step
      94) both crates are on the floor. Not investigated; no launch spent on it. Next: the owner of the crates'
      bodies.
- [ ] **Found in 9.2's independent check (step 94), older than it: 2-29's barrel and stone stand right of the
      original's.** In the port's f420 frames, before and after step 94 alike, the barrel and the scale-1
      `rolling_stone` beside it stand to the right of where the original's `2-29_h6.3` shows them; the original's
      barrel is partly past the left wall's edge (`9.2/verify1/trip/2-29_level28a.png`). Step 94 moves the stone a
      further 3.4 u right (the barrel's half-width grows 3.3 u); the older divergence is not investigated. Next: the
      owner of the dynamic bodies.
- [x] **Found in 9.2 (step 94), for 9.3: `stone.py` cannot read the port's 2-22 stone at f420.** The disc is clipped by
      the frame's right edge and touches the dark crate and blocks: 30.16 u after (28.68 before), and the same script
      reads the original's frames at 31.6, 36.9 and 37.1 u, not the 25.2-25.6 u `systems_9_10` 9a (5) quotes. The
      drawn size is 51.2 u (the capture log) and the body 24 u (the file). Next: 9.3 measures on a frame with the
      whole arc in view, as 9a (5) allows. **Closed in 9.3 (step 95)**: at f420 the scaled disc is whole in the
      frame; the fault is the script's, whose sky rows (100-130) hold the disc's own top. Re-instrumented
      (`9.3/tools/stone93.py`: ray edges against sky, HUD band excluded, a trimmed circle fit; parameters fixed and
      validated on the original before a port frame was read): original 24.62 / 24.63 / 24.64 u, port 24.65 u, the
      drawn file's prediction 24.55 u - inside 9a (5)'s 25.3 +- 1.5 u. Deviation row "Order 9.3" below.
- [ ] **Order 9.3 deviation, the stone gate measured by a rebuilt instrument** (step 95). `briefs/s9_10/stone.py`
      as written reads the port at 30.16 u and the original at 31.6-37.1 u (its saved `stone.json`), so its row is
      recorded NOT MET as written; `9.3/tools/stone93.py` reads the original 24.62-24.64 u and the port 24.65 u. The
      brief's 25.2-25.6 u for the original is reproduced by neither script. The independent check's own instrument
      agrees (port 24.57 u, original 24.58 u; step 93's unscaled stone 30.71 u against 30.69 predicted), and the
      brief's script swept over thresholds 0.55-0.75 reads the original anywhere from 27.9 to 68.5 u. Next: the
      owner accepts the substitution or not; 00_order section 7 (9.3) and `systems_9_10` 9a (5) should name a
      calibrated instrument and strike the 25.2-25.6 u baseline.
- [ ] **Order 9.3 deviation, body pins added** (step 95). Section 7 names the suites R9 must keep green; the
      bodies themselves were pinned only in the converted files. `test_mp_fire` (level12a's barrels, 28.6 x 37.7 u)
      and `test_mp_demolish` (2-22's stone, 24 u; level8's, 30) now pin them as `LevelBuilder` builds them.
- [ ] **Order 9.3 deviation, the eclipse checked against footage** (step 95). Section 5 names F17 for "9.3 the
      eclipse" and section 7 lists no gate: F17 r5's rim at rest has a curvature radius of 707.2 px, `eclipse.png`
      at `<Scale>` 2 predicts 713.7 px (at 1, 356.9). Measured on the original's frames against the file, since the
      port never draws it risen.
- [ ] **Order 9.3 deviation, `test_mp_play` is not R9's gate** (step 95): it reads `level30` alone. R9 is carried
      by `test_mp_fire`, `test_mp_demolish` and `test_mp_start` (the row above, answered; `test_mp_start` 753 checks,
      0 failures, in the independent check).
- [ ] **Left by 9.3 (step 95): no suite plays a barrel level to its end.** 2-24's player starts on a barrel since
      step 94, as the original's; a chapter-2 walkthrough is the play tests' owner's. And 4-19's eclipse is never on
      screen in the port: it stands below the start view, and the port has no `rocket_landing` to slide it up after
      the win (F17). Next: the level-end owner.
- [ ] **Order 11.3 deviation, the door's red light stands 7.75 u left of the original's and 4.0 u high** (step 96;
      the F12 light row, NOT MET as written, committed with it under the owner's delegated decision). Required: on at
      the door frame D, the right-hand stones' R - G 16.6-17.1 flat for about 0.47 s, gone by about +1.23 s. Measured
      (`11.3/verify3/f12_v3.json`): f192-f195 at 0.65 / 0.67 / 0.67 / 12.14 against F12 #139-#142 at 1.86 / 16.64 /
      2.31 / 16.66; the right patch flat at 12.13-12.14 to +533 ms, then eighths (10.69 to 2.15), baseline 0.67 at
      +1,117 ms; the left patch 27.96 against about 17.3. Its brightness and rule are the original's: a fit of both
      engines' light rule to window01's normal map (`11.3/fix1/light_fit3.py`) gives a gain of 1,517 against 1,522 at
      correlation 0.9996 / 0.9973, and both step in eighths. F12's "thirds" are its eight sparks dying in three clumps.
      The point is (679.0, 186.25) u against (686.75, 190.25); the x is the whole right-hand shortfall, and the
      character's cloak is 8.2 u left of F12's on its last frame (`11.3/verify3/charx_v3.json`). The same cause gives
      the F12 sparkles their dim, left-shifted look. Next: nothing for the lights; the 4.0 u is R25's player half, the
      7.75 u is the exit row below, and the door frame is the frame-order row. Re-take the row when any of the three
      lands.
- [ ] **Order 11.3 deviation, the character's point is its capsule centre** (step 96): the door's sparkles and a
      character's blood sit about 3.9 u high (22 u over the feet against the original's 18.1). R25's player half.
- [ ] **Merge finding, the skull reads the rest camera** (step 112): step 96 places both skulls from `cameraCornerPx()`,
      the follow's view, which step 101's shake never moves; the original reads the shaken camera for both
      (`checkGameLost` #257 this frame's, `ETHBeginContactCallback_enemy_killer` #35 the previous frame's; D11 section 2,
      D11_check section 2). A skull added during a shake stands up to one tremble off, where it stays. Not measured.
      Next: with step 101's deviation 6, the shaken-camera readers the port keeps at rest.
- [x] **Order 11.3 deviation, no `startEarthquake`** (step 96): the fall's shake (20, or 5 weightless) and the killer
      floor's (20) were not wired on `main`. **Closed by the merge of the two tracks (step 112):** both are
      `earthquake.json`'s `fall` / `fall_no_gravity` and `minion_killed`, track B's step 101 (written as 121 on
      `visuals-d`), live on `main` since the merge and checked through the layer there by `AFallShakesTheView` and
      `EventsALevelMakesOnItsOwnShakeTheView` (`test_mp_layer` 2901 checks, 0 failures).
- [ ] **Order 11.3 deviation, `fire32.ent` has no step** (step 96): 00_order section 5 names F13 for "11.3 `skull`,
      `blood`, `fire32`", section 7's 11.3 and `system_7` 7f do not. F13's fire death shows flames the port does not
      draw. Next: the owner assigns it.
- [x] **Order 11.3 deviation, the suck's smear sits 9 px (3.2 u) higher than F12's** (step 96). Closed in its fix
      round: the whole disc lands on the original's within 2 px in a shift search (`11.3/verify1/disc_offset.json`).
      The 9 px came from a centroid box that took in the doorway's sparks. A smear-only search differs by 13-14 px in x,
      which depends on the instrument.
- [ ] **Order 11.3 deviation, order within a tick** (step 96): a killer floor's skull and blood come on one tick (the
      original's a frame apart), and a tick's finish or death effects are queued after its other effects - the port's
      order. Only the effect stream can move.
- [ ] **Order 11.3 deviation, two skull points pinned in tests only** (step 96): the weightless fall's and the killer
      floor's have no footage.
- [ ] **Order 11.3 deviation, what `GameStateController` adds draws a frame early, and the door frame's light is
      missing** (step 96, fix round 1: found, confirmed on F12, NOT fixed). In the original, an entity the
      scene-update callback adds is first updated on the next frame (`ETHScene.cpp:467-474`,
      `ETHActiveEntityHandler.cpp:56-61`). An `allAtOnce` count starts full (`ETHParticleManager.cpp:110-117`), and a
      light reads the frame's own count (`ETHScene.cpp:482`, `ETHEntityRenderingManager.cpp:110-116`). So the door's
      light is on at D (F12 #140), off at D+1 (#141, the disc's first frame) and on from D+2. The port's runtime steps
      an effect on its spawn frame and places its light before the step. So its disc, sparks, fall skull and character
      blood draw from D, and its door light is off on D. Next: the effects runtime's owner (step 83), with the recipe in
      step 96's deviation 8: (a) light and halo after the step; (b) an `allAtOnce` count starting full; (c) the first
      update by where the adder runs, which is INFERRED for callbacks. Part (a) moves every fireball's and explosion's
      light by a frame, so that change needs its own sweep.
- [ ] **Order 11.3 finding, the character goes in at the door 7.75 u left of the original's** (step 96, fix round 1;
      GAMEPLAY, not built). This is measured through the door light's fit on 1-01: the character's point on the door
      tick is x 679.0 u in the port against 686.75 in the original. The cloak's centroid agrees: the port's is 1,012
      px one tick before the door, and the original's is still walking at 1,035 px 52 ms before its door frame. The
      cause is INFERRED, not compared box for box: the port's exit is its player body overlapping the level's trigger
      box (`Goals::State::Tick`) - on 1-01 the character is at 679.011 u on the door tick, the first tick past 693 - 4 - 10 = 679 (a tick's walk is about 2.7 u):
      the trigger's left edge (`trigger_size` 8 wide) less the capsule's radius (`player.json` `body.width_px` 20, a
      `_guess`; fix round 2) - where the original's is `scaledCollide(finalDoor, character)`, a plain overlap of the two
      entities' absolute collision boxes (`checkGameEnd` #15-32; `utilCollide.angelscript` 330795..331560;
      `door.ent`'s box 8 x 57 at (0, 20)). It moves where the door's sparks, light and a finished level's last
      frame stand. Next: the owner of the exit (Goals).
- [ ] **Order 11.3 deviation, door-flame pixels one RGB565 level off** (step 96, deviation 9). Required: every
      changed pixel of the sweep explained. Measured: 17 pixels, 9 on 2-12 and 8 on 3-09, inside the final door's
      additive flame, each exactly one RGB565 level off in one channel (`11.3/verify3/door_flame_px_v3.json`). Cause
      INFERRED from the code, not measured: effect quads coming and going reshuffle the packed registry, so the
      flame's equal-depth additive quads sum in a different order at the transparent sort's tie-break
      (`RenderSystem::SortTransparentDraws`, engine `src/core/RenderSystem.cpp:158-176`); the flame's own particles do
      not move. The same class as the step-88 merge's 15 pixels on 4-14. Quantisation-level noise; it blocks nothing.
      Next: the effects runtime's owner or the engine's transparent sort.
- [ ] **Order 11.3 finding, the original checks a death before the door** (step 96, the third check; GAMEPLAY, not
      built). `GameStateController::update` calls #110 `checkGameLost`, then #113 `checkGameEnd`, whose #21-32 refuse
      once `finishGame` (122645..122773 #3-8) has set `gameLost` and `gameFinished`: a character that dies and reaches
      the door on one frame loses, with blood or the skull. The port's `OnFixedUpdate` takes the `goals.completed`
      branch first, so the door wins and draws the suck and the sparks; its comment (`MagicPortalsLayer.cpp:6541-6546`)
      says the order of two triggers in a frame is not defined. The behaviour predates step 96, which only makes the
      difference visible. Next: the owner of Goals and Hazards; the comment changes when this row is built.
- [ ] **Left by 11.3 (step 96)**: 2-32's victory cutscene (`ETHCallback_dragon_knight`'s own `levelFinishedEffect`)
      with the knight; the minion's `playMinionFallSound`.
- [x] **Order 11.4 NOT RUN, `test_mp_layer`** (step 97, deviation 1). Closed by the independent check: its one launch
      of `03d1f439` (the build's two launches were refused) read 1,959 checks, 0 failures - the unit on the drawn quads
      and the follow, 0.000000 u from the knight's drawn point over 80.7 u of patrol (the tick body 2.013 u off), with
      the caveat of the dangling-pointer row below.
- [x] **Order 11.4 NOT RUN, `test_mp_diamonds`** (step 97). Smart App Control refused `6f603a35` on the build's launch
      and again on the check's; not relinked, to spare a notification. Its only contact with the step is the renamed
      field (`Minion::entity`). Closed by step 98's independent check: its relink `4241e7ee`, which carries step 98 as
      well, read 126 checks, 0 failures (recorded in step 98).
- [x] **Order 11.4 deviation, the aura case reads a dangling pointer** (step 97, deviation 5; found by the independent
      check). Fixed in step 98 at both call sites (the reports held in a local, then `MinionOf`), and run once on
      `test_mp_layer` `f6e6f2cf`: 2,011 checks, 0 failures, the aura 0.000000 u from its drawn knight as before. `tests/test_mp_layer.cpp`, `TheBlackKnightCarriesItsAuraAsAMultiply`, calls
      `MinionOf(layer.MinionReports(), "minion_spawn_908")` in the follow loop and in the depth check; `MinionReports()`
      returns its vector by value, so the pointer outlives the temporary. The follow's 0.000000 u is credible but
      undefined; the depth check's comparison with the knight is vacuous and the knight z it prints is freed memory. The
      order itself holds, measured from `out/levels/level31b.tscn` (door -1.452 < aura -1.4485 < platforms -1.448 to
      -1.432 < knight -1.430 < player -1.428). Not fixed at the commit: a test edit relinks the suite and needs a launch
      under Smart App Control. Next: at both call sites `const auto minionReports = layer.MinionReports();` then
      `MinionOf(minionReports, ...)`; relink `test_mp_layer` once, launch it once, record the count in step 97.
- [x] **Order 11.4 merge note, Q.2's `minion_impact` must still leave the black knight out** (step 97). **Closed by the
      merge (step 112):** `impactBodies` reads `Minion::entity` (`!Earthquake::IsEntity(minion.entity, ...)`), and the
      new `TheBlackKnightsLandingMakesNoTremble` drops level31b's knight 30 units onto its floor and pins 0 begins of
      its own (`test_mp_layer`). The fall sound, the `holdTime` cap and `ScoreDashboard` counts below stay open. On `visuals-d`,
      Q.2 (`7465279`) skips the knight with `!minion.entityName.empty() && !Earthquake::IsEntity(minion.entityName, ...)`;
      step 97 renames the field `Minion::entity` and it is never empty (`minion.ent` by default). When `visuals-d`
      merges, re-point that test at `minion.entity` so `ghost_minion.ent` stays out of `minion_impact`, as the
      original's missing `ETHBeginContactCallback_ghost_minion` leaves it (`ETHPhysicsEntityController.cpp:59`,
      `:66-70`; `ETHBeginContactCallback_minion` 437083..437789 #124-130). A rename that merely compiles is not enough:
      pin it (a `ghost_minion.ent` landing makes no tremble). The same holds for any later minion fall sound
      (#132-140), the dark mage's `holdTime` cap (#12-55) and `ScoreDashboard`'s `minion.ent`-only counts. Next: the
      merge agent of `visuals-d`.
- [ ] **Order 11.4 finding, the eyes follow the player's tick point, not its drawn one** (step 97; INFERRED from the
      code, not measured). `effectOwnerPoint`'s `Kind::Player` reads the sim body, which has no
      `InterpolatedTransformComponent`, while the player's picture is drawn between ticks; under `--fixed-step` every
      frame is drawn at `Apply(0)`, the tick before, so the red eyes would lead the drawn face by one tick's walk while
      walking - up to 2.7 u at `player.json`'s 160 px/s. Step 97's `Kind::Minion` reads the drawn quad for this reason.
      Next: the effects runtime's owner (step 84's eyes); measure on a walking capture first.
- [ ] **Order 11.4 deviation, placed systems a few levels off on 3-32** (step 97): 23 px in the final door's flame at
      f420, each in one channel only, by at most 9 levels, and on the gate clip the gutter's steam (f60-f129, 54-172 px a frame, every
      channel within 9 levels, some by 1-3) - the class of "Order 11.3 deviation,
      door-flame pixels" above (equal-depth tie order, INFERRED); the placement stream is untouched. Next: with that row.
- [ ] **Left by 11.4 (step 97)**: `ghost_minion.ent`'s `startFrame` 4 (never drawn: the knight walks on its first
      update); `dragon_knight.ent`'s aura, with 2-32's victory cutscene. (Its R4 hook is used by step 98.)
- [x] **Order 12.2 NOT RUN, every capture gate** (step 98, deviation 1). Smart App Control refused `MagicPortals.exe`
      `701876e8`, `a514d2a4` and `b5deb246` at the build. Closed by the independent check (`12.2/verify1/`): one
      link-only relink, `7e43f9b3`, accepted; 1-01 f420 `e2da5131`; the 128-level f420 sweep 128 of 128 byte-identical
      to `11.4/after/sweep`; F15's picture matches in kind on a fire route it calibrated (the gate rows of step 98);
      rage's luminance a contrast of 0.54-0.56 of SHOOTING's, the raw mean 0.814 (alpha 0.5 over L 31).
- [x] **Order 12.2 NOT RUN, `test_mp_motion` FAILED on its last launch** (step 98, deviation 2). `71b2bb20` read 167
      checks with 14 failures, every one a fixture without the `ghost` row `LoadRules` now requires; the fixture edit
      (`kGhostRow`) was built and unlaunched (`343cc592` and its relink `46219146` refused). Closed by the independent
      check: its relink `c8ae473e` read 169 checks, 0 failures.
- [x] **Order 12.2 NOT RUN, `test_mp_diamonds`** (step 98, deviation 2; also step 97's row above). `9df89061` and its
      relink `62e5f813` refused at the build. Closed by the independent check: its relink `4241e7ee` read 126 checks, 0
      failures (`holdingOneTurnsAPortalShotIntoAFireball`'s fireball now burns level28b's wayA patroller, which no
      check there reads).
- [ ] **Order 12.2, THE OWNER'S TO RULE: fire against the ghost** (step 98; F15 finding 1). The original's fireball passes
      through `ghost.ent` (a sensor: `ETHBeginContactCallback_fireball` #46-121 returns on one) and only a barrel's
      `explode()` burns it; the port keeps its fireball test (within `radius_px` 64) because the barrel restock
      (`addEntityIfItCantBeFound('barrel_bomb')`) is not built - explode-only would leave the port's ghost unkillable.
      Also unpinned: a fireball while the ghost is APPEARING sets `burned`, counted on its first SHOOTING tick. Rage's
      picture and luminance (step 98) were reached only through this port-only rule.
- [ ] **Order 12.2 finding, `no_music` and `isBossFight`** (step 98). `Game::preLoop` #485-509 plays
      `playGameMusic(m_levelIndex == 31)` unless the level's properties carry `no_music` (`readProperties` #166-181): the
      original is silent at load on level0c, level18c, level31b and level31c, and plays the boss's music from load on
      2-32. The port plays "game" on all of them (and "boss" on 1-32); step 98 switches 3-32 to the boss's music from the
      ghost's init and to silence from its DEATH. Audio only. Next: the sound's owner.
- [ ] **Order 12.2, not built** (step 98, deviation 6): the ghost's and its shot's sounds (the shot's
      `startEarthquake(10)`, #331-333, is `earthquake.json`'s `enemy_shot_end` since the merge, step 112); RAGE_MODE's own fire-diamond restock every frame while the player
      lives (#1034-1071); the DAMAGE and DEATH explosions and the death blast's 2.0; `ghost.ent`'s static box, which fails a
      portal ray across it (F15 finding 2, `computePortalFinalPos` #30, #136-175); the static bucket clock (a ghost off the
      view does not advance in the original; a knight killed on the floor at y 768 adds it far below the view); an
      explosion's burn of a minion (level18b's bombs). 4-32's dropped yellow key's picture stays 12.1's.
- [ ] **Order 12.2 deviation, the dropped key draws in front of level31b's z-0 art** (step 98, deviation 8): its depth is
      the ghost's -0.2889, but the effects runtime places a picture at `slotAfter(round(z))` and round(-0.2889) is 0, so
      the key and its sparkles stand after the five platforms, where the original's (world 2 - f) stands behind them.
      Picture-only, on 3-32 after the ghost dies. Next: an unrounded picture slot for such a depth in the runtime.
- [ ] **Order 12.2 INFERRED, the ghost drawn white on the tick the knight dies** (step 98, deviation 3): the static
      `ghost.ent` added in a frame's update is drawn in that frame at the `.ent`'s white, alpha 1, scale 0.3, and its
      callback runs in the next (`ETHScene.cpp:547-572`). Pinned in the suites; no footage shows it (F15 r1 hides the
      contact in a 216 ms gap). Seen on the port's picture in the independent check (`12.2/verify1/route/deathtick`:
      f468 white, its pale tail beside the explosion; f469 black); the original's frame at that tick is still unseen.
      Next: a 60 fps clip of the kill.
- [ ] **Order 12.2 deviation, no `fire32` flames on the burned knight** (step 98, deviation 9; found by the independent
      check). F15 r1 #157-#171 shows about 1.3 s of flames on the knight's spot - the minion's burn's `addFlames`
      (`ETHCallback_minion` #1456-1471); the port draws the explosion and the blood only (`12.2/verify1/f15_vs_port_0.png`
      at +0.73 and +1.26 s). `addFlames` is not built (`Minions.hpp`, `Death::byFire`). The same entity as the "Order
      11.3 deviation, `fire32.ent` has no step" row above. Next: the owner assigns `fire32`.
- [ ] **Order 12.2 deviation, the first ghost shot leaves sooner than the original's, unexplained** (step 98, deviation
      10; found by the independent check). The port's first shot leaves on SHOOTING's first tick (tick 695 on the
      check's fire route); F15 section 3 has the original's 0.2-0.3 s after the hover starts, not explained there. With
      the 16 u nudge, the port's standing player died on tick 721, about 0.5 s before the original's first shot is even
      visible at the same moment after the kill. Next: find in the listing what delays the original's first shot, then
      measure it on F15.
- [ ] **Order 12.2 finding, the ghost's hover is simulation state on `std::cos`** (step 98; found by the independent
      check). `Ghost.cpp:122` moves the ghost's simulation point - its shots' origin and its key - by
      `Motion::OffsetPx`, which calls `std::cos` (`Motion.cpp:245`, `:252`); the simulation's trig goes through
      `DetMath` so replays reproduce (`engine/src/core/DetMath.hpp`; `Ghost.cpp`'s own `SmoothEnd` does). Next: a
      `DetMath` version of the offset for the simulation's row, leaving the layer's picture motions (and so the sweep)
      as they are. A simulation change.
- [ ] **Order 12.2 tidy, a header comment parted from its member** (step 98; found by the independent check).
      `game/MagicPortalsLayer.hpp`: the `GhostQuad` block now sits between 2-32's dragon comment ("dragon_boss.png cut
      2 x 2 ...") and `m_dragonCut`, which it describes. Next: move the block above that comment in a step that relinks
      anyway - a header edit relinks every suite under Smart App Control.
- [x] **Order 12.2 merge notes (`visuals-d`)** (step 98): the effects runtime now cuts a picture of more than one cell with
      `Tiers::FrameCut` and turns its pivot with its angle - any track-B effect with more cells, or a pivot and an angle,
      moves; `effects.json` `enemy_shoot_mark` and track B's 10.1 `portal_mark` row for the player's shot are two call
      sites, keep both; `Slot::Ghost` joins the between-slot table; `Minions::Death::byFire` is new; the enemy shot's
      `startEarthquake(10)` is Q.1's to wire. **Done by the merge (step 112)**, with one note not followed: the run's
      instruction for the merge was ONE spawn rule for `portal_mark`, so `enemy_shoot_mark` is folded into
      `portal_mark` (the same numbers) and the ghost's shot adds it at `Ghost::Shot::destinyPx` once, where both tracks
      would otherwise have added one each. The pivot and the cut apply to every effect; `Slot::Ghost` is row 13 of 22;
      `enemy_shot_end` reads the ghost's shots as track B wired it. Step 112 has each.
- [ ] **Order 12.1 deviation, the dark dragon at z 0 where the plan said 8** (step 99, deviation 1): `originalPos` is
      written as `vector3(waypoint.pos, 0)` (`ETHCallback_dark_dragon` ins 529-537) and the hover's `SetPosition` on
      this physics entity writes z (`linearMotion` ins 180-208, `ETHPhysicsEntityController.cpp:92-96`), so from its
      second update the original stands at z 0 - behind the player, in front of level31c's art; 00_order K13's
      `slotAfter(8)` came from system_5's INFERRED "AddScaledEntity keeps z". The summon's frame and the first update
      (z 8, at the spawn off the view's left edge) are drawn at 0 too, and at 0 it ties with its own z-0 fireballs in
      the engine's gather order. Next: nothing unless a clip shows the order.
- [ ] **Order 12.1 INFERRED, the summon's frame draws the `.ent` before its first update** (step 99, deviation 2):
      startFrame 2, angle 0, its system playing, at the spawn - as step 98's ghost on the tick its knight dies. Next: a
      60 fps clip of the summon.
- [x] **Order 12.1 deviation, the wound read before the shot** (step 99, deviation 4) - **closed in step 99's fix round 1**:
      FIGHTING now reads `burned` after the mouth and the shot, all three inside `!isFinished` (ins 821-827, 1067), as the
      callback orders them. Gameplay: a blast on the update it fires lets that shot leave; a wound below half the interval
      enters DAMAGE with its mouth killed (the first round's DEV burns kept it burning through the second wound and the
      death); a burn after the level is finished is not read.
- [ ] **Order 12.1, not built** (step 99, deviation 5): the dark dragon's sounds (its earthquakes - every update of the
      jaw's waypoint past bias 0.2 at 10, ins 573-578, and 15 per shot, ins 980-982 - are wired since the merge, step
      112: `dark_dragon_mouth` reads this path's waypoint and bias); the scattered
      `small_explosion_no_light.ent` of DAMAGE and DYING (`darkDragonOvertimeSmallExplosionAdder`, ins 1558-1563,
      1813-1818) and the death's `explode`; `ghost_utility_spawn.ent`; the barrel bomb's restock after each shot
      (`addEntityIfItCantBeFound`, ins 997-1064) - gameplay, the level's only weapon comes back only through it: level31c
      places ONE `barrel_bomb` and the dragon takes three burns, so **4-32 cannot be won in the port** without it.
- [ ] **Order 12.1 finding, the summon's clock** (step 99; F16 section 3): the original's `dark_dragon_spawn` counts
      `arm_ms` 3000 from the level's start, gated on the light, so a torch lit late summons 4 s after it; the port counts
      from the light, 7 s. Gameplay. Next: the owner.
- [ ] **Order 12.1, the wound and the death captured with a DEV input** (step 99, deviation 6): `--burn-dragon` sets the
      flag a barrel's blast sets; the original's only burn is its own fireball setting off the level's one barrel within
      190 u of it, which F16 section 6 could not set up either. Next: F16 section 6's interception clip, which would put
      the flash against the original.
- [ ] **Order 12.1 deviation, the mouth's restart on the frame and the dragon at its tick point** (step 99, deviations 9
      and 10): the pool is remade on every frame the tick's `systemRestarted` stands (step 77's row, a second consumer) -
      since fix round 1 each such frame draws none of the fire, as the original's update does, but each moves the
      effects' stream - and the dragon is drawn with no interpolation between ticks; both show only on a display faster
      than it ticks. Picture only. Next: consume the restart once per tick, and interpolate the effect's point, in one
      runtime step.
- [ ] **Order 12.1 finding, the wound's shot at the torch is the reverse of the original's** (step 99's fix round 1;
      GAMEPLAY, the owner's): `ETHCallback_dark_dragon` fires at the torch as a wound ends only if
      `SeekEntity('light_off.ent')` finds one (ins 1675-1694). SeekEntity compares names exactly
      (`ETHBucketManager.cpp:394-406`), level31c's own torch is bare `light_off`, a lit torch deletes itself
      (`ETHCallback_light_off` ins 1-35, 105-114), and only `ETHCallback_fire_signal` adds a `light_off.ent` - the torch
      put out again. So the original never fires at a lit torch, and at one its fire signal put out the fireball relights
      it (`hasProjectileAround` counts `fireball.ent` within scale(24); decode plus geometry, not measured). The port
      fires whenever the level has a torch (`Game.cpp` `hasTorch`), and against a lit one Torch puts it out and brings
      the light wall back. Predates step 99; no batch owns it. Next: the owner.
- [ ] **Order 12.1 finding, 2-32's mouth drawn while Play restarts it** (step 99's fix round 1, for step 77's owner):
      Ethanon updates an entity's particles, THEN runs its callback, THEN draws (`ETHActiveEntityHandler.cpp:134-138`,
      `ETHSpriteEntity.cpp:455-459`, `ETHScene.cpp:459-497`), and Play leaves every particle unreleased
      (`ETHParticleManager.cpp:274-286`), which the draw skips (`:375`). `updateEmitters` remakes 2-32's dragon's pool
      BEFORE its step, so it draws particle 0 at the mouth on every update the callback calls `PlayParticleSystem(0)`,
      where the original would draw none - what step 99's first round did on 4-32, which F16 showed wrong. INFERRED for
      2-32 from the same order; no footage of 2-32's mouth was checked. Picture only. Next: step 77's owner, with a
      clip of 2-32's mouth.
- [ ] **Order 12.1, F16's jaw against its burst frame** (step 99; confirmation only, not blocking): counted from F16's
      #152 the port's jaw is about 0.2 s late (cell 1 / 3 at 3.77 / 3.95 s after the summon against 3.57 / 3.78 s), while
      F16's own shout and FIGHTING agree with 2500 / 7400 ms and its leg 3 begins 0.15-0.19 s early against them. Not
      explained. A candidate (the independent check's, not measured): F16 has a 228 ms recording gap at #150 -> #151
      just before #152, its FIGHTING anchor is an upper bound ("by #316"), and from the port's own fire-on the later
      intervals agree (fire out 3.13 against 3.09 s, the shot 4.20 against 4.20 s) - a uniform offset of every later
      event from #152 fits #152's timestamp standing off the true summon, not a timing difference in the dragon; against
      it, #151, 36 ms before #152, still shows the wall intact. Next: a 60 fps clip of the original's fly-in.
- [ ] **Order 12.1 deviations under rows that exist** (step 99, deviations 3, 7 and 8): the Follower on the script's
      whole 16 ms and the path, fire clock, neck and hover on the tick's 16.667 (the PORT-WIDE clock row; the fly-in is
      444 updates after the init here, about 463 on a 16 ms clock); the aim at the player's body centre (`PxOf`) where
      the original's `GetPositionXY` is the entity point (R25's player half, the owner's); the `!isPaused` guards met by
      the port not ticking a paused level, as step 77's. Next: those rows.
- [ ] **Order 12.1 tidy, two WaypointManagers** (step 99): `Ghost::Appear` and `DarkDragon::Path` are the same
      `FrameTimer` arithmetic twice. Next: one `sim` WaypointManager both use, in a step that touches the ghost anyway.
- [x] **Order 12.1 merge notes (`visuals-d`)** (step 99), **done by the merge (step 112)**: the mouth's shake reads
      `DarkDragon::State::path` (its `frame` and `Bias()`) and the shot's reads `fired` in FIGHTING, from the layer as
      every other call is, so neither `Look` nor `Turn` grew a field; the emissive, the owner kind and the DEV entries
      merged as they were. The note: Q.1's earthquake - the dark dragon asks for
      `startEarthquake(10)` on every update of the jaw's waypoint past bias 0.2 (ins 573-578) and `startEarthquake(15)`
      per shot (ins 980-982), carried by neither `DarkDragon::Look` nor `Turn` yet; `EffectOwner::Kind::DarkDragon` and
      the effect's own `emissive` are new in the runtime, and `tint` now reads it (the `.ent`'s unless a script writes
      one), so a track-B effect is unchanged unless it writes it; `--burn-dragon` / `ScheduleDevBurn` / `LayerTicks` are
      new DEV entries; the dragon's mouth lands its Kill / Play in `updateEffects` after the effect's emitter loop.
      Next: the merge agent.
- [ ] **Left by T.1 (step 100)**: a scheduled `--tap` still cannot press the pause's buttons (they have
      `ScheduleDevPress`) or the end screens' (nothing), which read the mouse alone; and whether the original fires on
      touch-down or touch-up is unresolved by L1 (section 2) - the port fires on the down edge, for a press and a
      scheduled tap alike. Harness and timing only. Next: a capture that needs either.
- [ ] **Order Q.1, NOT MET: F16's 4-32 light wall opens at +-10 px, the port's at +-19.7** (step 101, gate 4 and
      deviation 15; committed under the owner's delegated decision, not tuned). The decode and L1 (4-29) open at
      19.69 px and so does the port (-19.78 / +19.79 on 4-29; +7 u = 19.69 px on 4-32's tick 170); from its second pair
      on, the port is F16's whole sequence within 0.6 px. Why F16 has no first pair is D11_check C52, UNSETTLED: (a) a
      phase leak - an earlier sub-pixel call (rows 4 and 25, wired since Q.2) left the generator on a back; or (b) two
      dropped capture frames. F16's frame times lean to (a) and do not settle it (`Q.1/fix2/f16_timing.txt`). The phase
      is not seeded to fake it. Q.2 adds that the port now CAN open at +-10 where soft calls come before the wall; no
      4-32 capture was run (its torch needs the player walked to it). Next: the owner accepts it, or a light-wall clip
      with a visible frame cadence settles C52.
- [ ] **Order Q.1, the generator is kept across a restart and into the next level** (step 101, deviation 3), as the
      original's script module is never rebuilt (C3): a shake running when a level ends plays out over the next one's
      first frames; a menu freezes it. Next: the owner's call whether a load resets it.
- [ ] **Order Q.1, the original's other readers of the SHAKEN camera stay at rest in the port** (step 101, deviation 6;
      D11_check section 2, C36): the help blocks' tap rects; the lift's, `hand_tap`'s and the arrows' hit tests; the
      beholder's appear test and its spikes' screen cull; `no_portal_sign`'s 100 ms re-aim; 2-32's `dragon_claw` y echo;
      4-19's `rocket_landing` - each at most one frame and one spike off. The two skulls are the merge's row above ("the
      skull reads the rest camera"). Next: one step for the shaken camera's readers.
- [ ] **Order Q.1, D11 rows still unwired** (step 101's NOT WIRED list, as 10.1 and Q.2 left it): row 3
      (`antiportal_explosion`, a shot an `anti_portal_agent` kills: R23); row 10 (a stone crushing a minion: the port's
      minions are not breakable) and row 8's other half (a character killed above hitIntensity 2.5) - gameplay; row 13
      (`shaker`, placed by no level); row 21 (a burned carranca) and row 30 (4-01's rocket), systems the port lacks; row 9
      on 3-20, whose placed stone the port never culls. And three wired rows no case drives: `beholder_death`,
      `ghost_death`, `dark_dragon_death` (a boss beaten). Next: their owners; a boss-win case for the three.
- [ ] **Order Q.1 deviations, recorded** (step 101; none a gate failure): the 50 px cap and the 1 px stop taken at R12's
      720 px screen (deviation 2; a 1080 px phone's cap is 11.85 u); the light wall goes on its own tick, the original's
      with its first spike (5); two calls on one side of one tick in the port's order (8); the fireball's 5 a frame early,
      with its explosion (9); `SpaceSky` not ported, so the 19 `space_bg` levels shake their whole background (10); no
      vibration (11); F6's barrel on 2-12 not reproduced, because the port's blast takes no wall away (12; gameplay,
      `Fire.hpp`, nine chapter-2 levels - *closed by step 115*: the wall goes on the blast's tick and 2-12 trembles as
      F6's, at most 9.84 px); a static callback's call not limited to visible buckets (14). And the merge's:
      4-32's mouth leg starts on tick 721 where step 101 measured 720 (the path's 0 ms waypoint takes an update of its
      own; the same 48 ticks). Next: their owners.
- [ ] **Left by 13.3 (step 102)**: the band stopping at x 768 on a wide level (10 of 128) rests on
      `TheBlackBarsStopAt768OnAWideScene` and F17 alone - no capture has a quake while such a view is past 768; the
      no-portal sign, drawn in the HUD overlay, is over the bars where the original's entity is under them (1-15, 2-07,
      2-28; not measured); `view.json`'s note says 73 levels are 256 u tall where the `max` markers give 94 (00_order
      8.14's owner); a reverted comment in `game/sim/Art.cpp` ("every named type has a centre", where `ET_VERTICAL` is
      `EO_CENTER_BOTTOM`) waits for a rebuild. Next: their owners.
- [ ] **Order 10.1, the pictures show R23, R1, R2 and R17 wherever they bite** (step 103, deviations 1-4): the hourglass
      does not restart the 400 ms clock (R23); the mark follows the original's tap-time ray while the shot follows the
      port's flight, and the two disagree on 46 buttons (37 levels), 4 `destroyier.ent` and one anchor (sensors the
      original's ray stops on and the port's flight passes), on 116 placed teleportable bodies (62 levels) and 60 minion
      spawns (35) (the reverse), on the head-height retry, and on a reflected shot meeting a wall (R23); the fields are
      the port's - antiportals at 16 x scale (R1; 71 placements in 52 levels), wells (R2; 10 in 5) and
      `anti_portal_agent`s (R17; 21 in 17) as fields, and 1-07's `no_portal_705`; the in-flight kills the port does not
      play (an `anti_portal_agent`, a `fire_agent`'s flame). The pictures are right for the port's play; each difference
      is R23's, R1's, R2's or R17's gameplay. Next: those rulings built.
- [ ] **Order 10.1 deviations, recorded** (step 103, 5-10): no vibration; the fire-diamond tap's mark follows the
      leftover byte of `absTouchPos.x`, which the port's camera cannot reproduce (D3's unsettled item); the ghost's mark
      at the player's body centre (R25); `red_cross` looks UNDER `red_waves` in F3 where the add order puts it over (an
      unpinned tie, not measured); K14's price - a -0.5 class with its own z below 0 (a dial, a ring, a door glow, a
      spike, a wall's ghost) is drawn over the shot where both share a gap, and at an equal z 0 a placed portal and a z-0
      effect are ordered by class, not by when each was added (no gate frame shows one); the flash on a fire-diamond shot
      is undecoded (the port converts the shot before its first leg). Deviation 7's "the ghost exists from the level's
      start" is stale on `main`: R4 is built there. No footage shows `red_waves` (B) (1-15, 2-07, 2-28). Next: their
      owners.
- [ ] **Order Q.2, where the port makes a minion** (step 104, deviation 10; GAMEPLAY, the minions' owner's): the port
      centres a minion's 7-unit circle on its marker, 11.1 higher than the original makes its knight (the entity point
      on the marker: its feet into the floor on 51 of the 62 markers, 44 of them by 2.1), so every knight falls about 9 at its level's
      start where the original's is made standing. The impact watch reads around it; the sim is unchanged. Next: the
      minions' owner.
- [ ] **Order Q.2, the engine brakes a landing in stages** (step 104, deviation 3; the engine's): its speculative
      contacts stop a fall early and in stages (3-16: a 460 px/s fall stopped dead 7.7 px over the floor, falling again,
      touching 8 ticks later at 20 px/s) - a hover of a few frames on a hard landing, with the landing's shake when the
      body touches. Gameplay and the engine's; read around, not changed. Next: the engine's owner, if a capture shows the
      hover against the original.
- [ ] **Order Q.2 deviations, recorded, and tidy** (step 104): the port's contacts are its own bodies' - one capsule for
      the player where `dark_mage.ent` has two circles, a 7 px circle for a minion where `minion.ent` has two of 9.1 - so
      a knight's seam begin can erase a running landing shake where the original's would not, and the reverse
      (deviations 2, 8); two impact calls on one tick in the port's order (9; no tick of the sweep has two); row 4's sign
      against a round other not traced (4); no sensor in row 8 (5); a body first looked at begins nothing (6). Deferred,
      because each relinks every suite and the game: `Impacts::kRecentSteps` (60, `game/sim/Impacts.hpp:83`) into
      `earthquake.json` with its provenance, and `APlayerLandingShakesTheView`'s header comment (it lands on a back on
      3-16). Next: the next step that touches `sim/Impacts`.
- [ ] **Order 10.1b, a particle-only picture added from a controller or a tap starts one tick early** (step 105,
      deviation 1; step 106, deviation 1): the port draws `portal_mark`, `red_cross` and `crystal_pick` on the frame of
      the tick that adds them; the original adds them after its always-active update loop (a controller, the portal
      manager), so their particles are first released and drawn a frame later (`ETHScene.cpp:467`, `:473-474`, `:482`;
      `ETHParticleManager.cpp:125`, `:219-228`, `:375`). F2 and F3 show no mark on each of their four tap frames and one on
      the next. `crystal_vanish`, added from `ETHCallback_timer` inside the loop, is on time, as the port draws it;
      `red_waves` and the hourglass would start a tick late too (no footage frame). 16.7 ms, picture only. Next: the
      effects runtime - such a picture first drawn on its tick + 1 at age 0, with a test; beside 11.3's "what
      `GameStateController` adds draws a frame early".
- [ ] **Order 10.2, the pickup rule** (step 106, deviation 3; GAMEPLAY no batch owns): the port takes a crystal when the
      player's body overlaps its trigger box (Goals, the remake's rule); `MainCharacter::crystalPicker` takes it when the
      character's point is within `squaredRange` of the crystal's point shifted `scale(14)` either way (#111-177). The
      burst follows whichever takes it. Next: the owner.
- [ ] **Order 10.2 deviations, and tidy** (step 106): the expiry tick is the port's (Goals' `leftS` reaching 0 against
      the timer's strict `>` on floored frame ms, step 68's note; deviation 2); the crystal's point is the tick before's,
      at most 0.05 u of bob (4). For later, from its verification: a per-system pool count in `EffectReport` (so the
      suites pin {5, 2} and {1, 2}, not the sum); the effect's depth read from the node's `eth_z` rather than the lighting
      look's `lookZ` (a level whose lighting read failed would put the burst at a flat 20; all 128 read today; 10.3 and
      10.4 read the same field).
- [ ] **Order 10.3, gameplay rows** (step 107, deviations 3 and 4): several carriers in range on one frame - the
      original's loop adds a `key_pick` for each and the last owns the key; `Carry::Acquire` takes the first. And the
      keyhole's 1500 ms counts only while it is on screen in the original (`keyhole.ent` is static, so its callback runs
      only in visible buckets, 00_order 8.12c); the port counts it everywhere, as it does the button's, the door's and
      the wall ghost's clocks (steps 78-80). Next: the owner, with the bucket-visibility rows.
- [ ] **Order 10.3, not built, and tidy** (step 107): the key's fly-in (F9: swung 20 u right of the keyhole, slid in,
      faded and deleted about 1.27 s after the unlock; the port's spent key holds until the keyhole goes; only the
      fly-in's clock is built, because it times the halo); a boss-dropped key's `key_pick` (the merge's finding: 10.3
      adds it only for a key a level node pictures, and 12.1 / 12.2 draw the bosses' keys as effects); the unlock's
      pictures against F9 need a route to a keyhole or a capture-only warp; `latchSimEffects` drops all three door
      effects when the keyhole's OR the door's picture is missing (no frozen level has either missing); `Keys.hpp`'s
      header comment is stale. Next: the key's picture owner.
- [ ] **Order 10.4, not built, and recorded** (step 108): `shock_agent`'s `small_explosion.ent` where a fireball destroys
      an agent (#331-345; deviation 2 - a one-row addition beside `shock_death_at_agent`, owned by no step); the carried
      shock diamond at alpha 0.1 (its lightning still draws at full strength while carried; `diamonds.json`
      `_not_built`); the sounds (`playShockDiamondPickSound`, `playShockStrikeSound`, `playBurnSound`,
      `playFireExtinguishSound`, `playLightOffSound`); F14's standoff (its knight struck 27 u off, where the port's route
      walks the knight into the player); the effects' onset - the strike's white peaks about 0.04 s after F14's, the
      water's plateau starts about 0.12 s after L1's; `Torch.hpp:42`'s stale comment (`Fields.hpp`'s was fixed by 13.1)
      and `system_7.md`'s "unreachable" for `ETHCallback_shock_agent`. Next: their owners.
- [ ] **Order 11.1, GAMEPLAY rows for the owner** (step 109, deviations 1-4; each moves the effects the step draws with
      it): `destroyable` stones are not destroyed by the wall they break (`ETHBeginContactCallback_breakable_wall`
      #42-60) - 12 placed stones on 10 levels, and every stone an `entity_launcher` throws (#200-209 set the flag), so on
      1-24 and 1-25 the original's thrown stone dies at the wall it breaks while the port's rolls on to the despawner;
      3-20's placed stone is never taken by its destroyier; the port's lift door (`Mover::Door`) rises 126 u where the
      script rises 0.8 x h = 102.4, and turns wherever its channel turns it where the script opens it only from shut and
      closes it only from fully raised; the beholder breaks any demolisher stone rising into it, the original only
      `rolling_stone.ent`. And two wall breaks the port does not do: a bomb's blast destroys every breakable it grabs
      (`explode` #111-122; `Fire.hpp:52-56`, nine chapter-2 levels - *the walls built in step 115*, B1), and 4-19's
      `red_key_state` destroys `breakable_wall_2756` 6000 ms after its key is destroyed (recorded nowhere else). Next:
      the owner.
- [ ] **Order 11.1, recorded, and tidy** (step 109): the wall's angle is the converter's 1.5708 rad (-90.00021 deg, 0.0002
      u over the half line); static callbacks add their dust only while their bucket is drawn in the original
      (deviation 6, with steps 78-80's rows); 2-32's claw's `wall_destroy` at angle 90 has no owner in 00_order section 7;
      the wall watch (`m_wallEffects`) is one-way and its comment says a wall does not come back (harmless: 11.2 gave the
      light wall a watch of its own); `ALiftDoorLeavesItsDustAsItOpensAndCloses` names `button_1666` where the door that
      turns is `button_1656`'s. The comments wait for an edit that recompiles them anyway.
- [ ] **Order 11.2, THE OWNER'S TO RULE: the fire signal's orbit** (step 110, deviation 1; GAMEPLAY, applied): clockwise
      from above and added on its torch (decoded #42-108; L1: 296-300 deg/s at 48-54 px), where the port had it
      counter-clockwise from a full radius to the torch's left. In the original this is the picture alone; it changes
      the port's play only because the port's put-out test stands on the ball (the row below): on L1's taps the put-out
      moves from tick 261 to 260, which is also the tick the original's torch-centred test gives on them, by their
      geometry. Next: the owner rules on it with the row below.
- [ ] **Order 11.2, GAMEPLAY from step 30: the fire signal's put-out test** (found by step 110's fix round, not
      rebuilt): `ETHCallback_fire_signal` #111-117 test `hasProjectileAround(originalPos, scale(24), includeFireballs 0)` -
      at the TORCH's point, `projectile.ent` alone, in the torch point's own bucket alone (`hasProjectileAround` #4-13).
      The port (`Torch::State::Tick`, since step 30) tests at the orbiting BALL and counts fireballs, so a shot through
      the torch's point but not near the ball goes on, and a fireball near the ball puts the torch out - reachable on
      4-32 alone, where the dark dragon's fireballs fly. `game/sim/Torch.cpp:155`'s comment, `test_mp_torch`'s "it MOVES,
      which is what makes it a shot worth aiming" and step 30's record read it the port's way; the data notes are
      corrected. Next: the owner; gameplay no batch owns.
- [ ] **Order 11.2, the zero-g recoil's speed** (step 110, deviation 7; found there, not its own): the port's 1.4 m/s
      recoil is 70 u/s (`zerog.json`; `kPixelsPerMetre` 50, Ethanon's `DEFAULT_SCALE`), where F10 measured one at 40-44
      u/s. Next: `zerog.json`'s owner.
- [ ] **Order 11.2, step 73's ring depth** (step 110, deviation 9): `gravitywell.json` `ring.z` draws the well's ring at
      -5 port units, where the area it stands for is at the bare -5 = -1.778 (step 86's rule, which `roundabout_effect`
      follows). Next: a step that touches the ring: -5 -> -1.778.
- [ ] **Order 11.2 deviations, recorded** (step 110): the lit torch's light does not bounce with its picture and halo
      (the original bakes a static light a frame after the lighting; 2); the rebuilt wall is an effect at the ghost,
      drawn at full brightness, which does not drift the original's 4 raw px a cycle (3); the unlit torch's 190 deg/s spin
      is not built (4); one frame shows both torches (5); the trail every 24th tick at 60 Hz, where the original's floored
      clock passes 400 on its 24th or 25th frame (6); every dynamic body swept from the first tick, and the agent's clock
      off the bucket clock (8); the trail hides with the player (10); the light-on, light-off and switch sounds (13); a
      lit torch takes its level emitters off the placement stream, so later level emitters draw a shifted sequence after
      a lighting (15). Next: their owners.
- [ ] **Order 13.1, the off-screen kill** (step 111; GAMEPLAY, older than it): `Fields::Tick` polls every ring every
      tick, where the original's `ETHCallback_shock_area` kills only while its bucket is drawn (D6_check 5.7). Next: the
      owner, with the bucket-visibility rows.
- [ ] **Order 13.1, not built, and recorded** (step 111, deviations 1-7): the agent's own pulse (`bounce(V2_ONE,
      vector2(0.95, 0.95), 300)` on the agent's sprite; its argument order not re-derived) and `playShockStrikeSound`;
      the ring turned and jittered from the level's first tick (the original's only while its bucket is drawn) and there
      from the first frame (the original's a frame later), on the port's 4.17 %-fast clock (the PORT-WIDE row), with
      `randF`'s inclusive 1.007 never drawn; paused frames now write the rings' two quads. None shows at 1 %. Next: a
      small step for the pulse.

## 3. UI

- [x] HUD + level start (steps 38-40), pause / finished / game over / popups (steps 50-52).
- [x] UI3 loading screen + main menu (step 53); credits, achievements dashboard and locking (step 54). Rulings applied:
      two chapters per page, back quits, locking as the original, loading screen.
- [x] UI3 chapter select + level grid (step 56, `sim/Selector`): vs the stills 1.72-2.86 mean_abs at offset (0, 0).
- [ ] Step 56 leftovers: spec 7.5/7.6 rows needing footage (slide frame by frame vs nav_gold, arrow entrance, swipe
      paths); silver chapter medal uncaptured (U3); the page counter's dot fits worse than its digit (U4).
- [ ] Step 54 was never independently verified and its §7.3 / §7.4 acceptance rows were not each measured: only
      compare.py on two frames (credits 6.79, dashboard 2.89) and the suites. The gold-save dashboard shows 315 points
      against the original's 325 (an unlock the port's medals cannot decide).
- [ ] UI2 leftovers: finished screen counter/medal past golden+2 not captured on a real play; ~~door suck/sparkle effect
      and death burst/skull not built~~ (built by 11.3, step 96: `red_suck_effect`, `red_sparkles`, `blood`, `skull`, gated
      on footage F12 and F13); 3-09 and 4-01 popups never raised at runtime; the pause's AchievementsPopup unmeasured.
- [ ] UI3 leftovers (spec §8.3): achievement unlock conditions not decoded (`AchievementManager`); locked-tile denial;
      credits drag/fling and dashboard bottom band not recorded; silver-medal states not captured; switch actions not decoded.
- [ ] Achievement text/icons are Asantee content: only in gitignored `out/data/achievements.json`, produced by the committed
      `tools/asbc/achievements.py --listing <all_functions.txt>`; the port draws no rows without it.

## 4. Gameplay feel (MILESTONE_1 §3 — still guesses)

- [ ] Player walk speed, acceleration, gravity (player.json all `_guess`) — measurable now from footage (rec11 walk).
- [ ] Camera follow lag / hold (portals.json camera `_guess`) — measurable from rec11 (the 11.25-12.0 s scroll).
- [ ] Portal timings `FIRST_PORTAL_MIN_TIME`, `NEXT_PORTAL_MIN_TIME`, collision radius, transit — decode or frame-step.

## 5. Final check

- [ ] 128-level sweep: capture the port at the library's `_h6.3` moment for every level, compare with
      `out/parity/original/levels/`, list residual differences per level, fix upstream (converter / port), repeat.

## 6. After the 2026-09-29 audit: the program to "only the owner's testing left"

A read-only audit on 2026-09-29 read the original's script listing, its assets and the converted levels against the
port, in six lenses, and had each lens checked by a skeptic. It found gaps the rows above do not carry. Its report and
evidence stay local, under the gitignored `out/parity/audit_2026-09-29/` (`REPORT.md`, `levels_completability.md`,
`check_levels/flood2.py`). Tags as there: VERIFIED (both sides read), INFERRED (reasoned), KNOWN (a row above).
On 2026-09-29 the owner said: "proceed with improvements until the only thing left is my testing".

**Definition of done for this program.**
- Every level is won by a scripted route that a suite replays, from its start to its door. *(Step 113: the harness
  is built - `tests/routes/`, replayed by `test_mp_routes` - and 11 of 128 levels are won by a route: 1-01, 1-02,
  1-03, 1-04, 1-05, 1-08, 1-09, 1-14, 1-15, 1-16, 1-32. Step 114 adds eight: 2-01, 2-02, 2-03, 2-04, 2-05, 2-07,
  2-08, 2-09 - 19 of 128. Step 115 adds four, 2-12, 2-13, 2-14 and 4-16, so far won only by the window's replay of
  their routes: `test_mp_routes` has not run on them (Smart App Control, three refusals) - 19 of 128 replayed by the
  suite, 23 with the window's. Open until all 128.)*
- A static reachability check over the converted levels runs as a test and shows no door sealed by the port.
  *(Step 113: `tools/parity/reachability.py`, pinned in `reachability.json`, runs as ctest `parity_reachability`;
  it shows 108 reachable, 11 opened, 9 sealed - B1's nine. Step 115's `barrel` breaker turns the nine to opened:
  108 reachable, 20 opened, 0 sealed. The flood is optimistic about barrels (a barrel and any fire on the level opens
  a wall), so five of the nine read opened with no port route yet: G8's and B4's rows. Open until those are won.)*
- Every blocker and major below is built as the original does it, or its ruling is recorded.
- The final 128-level comparison against the original's library (section 5) has run and its residuals are recorded,
  the largest fixed.
- The owner's open calls are listed in one place.

**Blockers: levels that cannot be won** (each built as the original does, and the level won by a route).
- [ ] **B1. A barrel's blast breaks no wall** - 2-12, 2-13, 2-14, 2-16, 2-17, 2-19, 2-21, 2-24 and 4-16 cannot be
      won (VERIFIED by a free-space flood; the mechanism was KNOWN in the "Order 11.1" rows). `explode` (listing L67590)
      #111-122 sets `destroy` on every breakable it grabs; the port breaks walls only for a stone, a torch or the dark
      dragon (`Fire.hpp:52-56`, `Demolish.cpp:73-86`). Build it after or with G5, or a fireball through 2-12's closed
      door sets off its barrel. *Built in step 115*: Demolish is the one owner of a breakable's breaking (a stone, the
      torch, the dark dragon and now the blast), and a blast takes every flagged `breakable_wall` (either spelling) it
      reaches with a clear line, on its own tick, with `wall_destroy` and its quake - F6's 2-12 to the tremble.
      GAMEPLAY. The same rule can also take a wall on 3-19 (`breakable_wall_1556`) and 4-10 (`breakable_wall_ent_2098`,
      34 u from its barrel), whose doors were reachable before, and possibly 2-29 (`breakable_wall_883`, 72 u): play
      there can differ (not played). **Won by routes: 2-12, 2-13, 2-14, 4-16** (in the window; `test_mp_routes` has not
      replayed them yet, row below). The other five have no port route (step 115 (d), measured by
      `out/parity/program/B1/scan_b1.py`): 2-16, 2-17, 2-19 need G8; 2-21, 2-24 need B4 (their rows). **Mechanism
      built; open until those five are won**: G8 (under Majors) is a blocker for 2-16, 2-17 and 2-19, B4 for 2-21 and
      2-24.
- [ ] **B2. 84 of 128 levels never unlock from the menus** on a fresh save, because 2-12 cannot be won (chapter 2
      tops out at 34 % of its medals, under the 60 % chapters 3 and 4 need). Goes with B1. *Not cleared by step 115*:
      2-12 is won, but a level opens only when the one before it scored, and chapter 2's order also stops at 2-06 (G6)
      and 2-10 and 2-11 (G8: 2-10's crates stand 133 u from its fire agent and cannot be sent through a portal, 2-11's
      static crate 206 u). Measured with a save of chapter 1 gold (a fixture) and every chapter-2 route's medal: 2-13
      to 2-15 open behind 2-12, 2-07, 2-11 and 2-12 locked, chapter 2 at 23 % (`out/parity/program/B1/b2/`). Open
      until G6 and G8 let those three be won.
- [ ] **B3. 4-32 cannot be won** - the barrel restock (`addEntityIfItCantBeFound('barrel_bomb')`, `ETHCallback_dark_dragon`
      ins 997-1064) and the lava's explode contact (`ETHBeginContactCallback_lava` L68416 #21-30) must land together,
      or a missed barrel softlocks. KNOWN ("Order 12.1, not built").
- [ ] **B4. 2-20, 2-22, 2-23, 2-27 at risk: the draggable handle** (`arrows.ent`, `ETHCallback_arrows` L67305, `doFollow`
      from `fire_agent` #280 and `reflect_agent` #6) is filed as cosmetic and not built; nothing reads `follow`.
      VERIFIED mechanism, INFERRED unwinnable. Needs release-to-fire (below) and the camera ruling. **Blocks 2-21 and
      2-24** (step 115, both `follow` agents; no port route found): 2-21's fixed agent is 103 u from its wall at best
      and the level has one barrel; on 2-24 one barrel in the fixed agent's zone sets off all seven (measured, ticks
      68-72), leaving nothing to hold `button_1144` in front of the door (that the player alone cannot then pass the
      door lift is reasoned from the geometry, not played; a barrel kept out of the chain to weigh the button, which
      works in the original too, was not tried - so 2-24's attribution to B4 is partly reasoned).
- [ ] **B5. 4-19 at risk: the red key breaks the wall** - `ETHCallback_red_key_state` (L97171): the key carried ->
      its sound, a 6000 ms fade of light and ambient, then `SeekEntity('breakable_wall')` destroyed (F17 #216). Nothing
      reads `red_key_state`. (The "Order 11.1" row says "destroyed"; the trigger is the key carried.)
- [ ] **B6. 3-13 at risk: an anti-portal agent should die to a shot**, taking its ring with it
      (`ETHCallback_anti_portal_agent` L66304 #109-187, `deleteAntiPortalInThisBucket`); the port keeps it a permanent
      zone (R17). 21 agents on 17 levels. Build before or with R1, which alone closes 4-01's route.
- [ ] **B7. 4-21 at risk: R2** - the well's port-only zone (248 px) covers the button, the wall and the door. KNOWN.

**Majors with no row above** (VERIFIED unless marked).
- [ ] **G1. Knights never see, shoot or destroy portals** (`ETHCallback_minion` L87616: 120 deg cone + occlusion ray,
      `addEnemyShoot` every 2000 ms, `killPortal` within scale(16), `!`/`?` marks, `cantReach`, the six darkest levels
      see only while a torch is lit). 34 levels play easier; `anti_sight_wall` has no purpose.
- [ ] **G2. Lifts run on their own**; the original's idle at marker `a`, and a tap inside their rect sends them at
      60 u/s (`ETHCallback_lift` L83504, `sendLiftToOtherPoint`) and fires no portal. 14 lifts on 13 levels.
- [ ] **G3. Moving platforms swing half as far at twice the rate** (R7: `linearMotion` cos(angle) x stride from
      +stride; the port sin(2 x speed x t) x stride / 2). 15 platforms on 11 levels, 3 patrolling agents.
- [ ] **G4. The portal shot flies 600 u/s against 200** (`ETHCallback_projectile` L71109 #113-125, with a 24 u jump
      on its first frame, #29-55).
- [x] **G5. Fireballs pass walls and closed doors** (`ETHBeginContactCallback_fireball` L85862 #152 dies on anything
      solid that is not a carranca, a sensor or a spared character). On 2-12 the player dies at the spawn (tick 180).
      *Built in step 114*: the fireball's body is fireball.ent's circle, radius 9.6; every solid body it touches spends
      it, and is burned or set off when it is burnable or explosive; 2-12's player lives at the spawn, the fire
      stopping on the closed door. Routes for 2-01 to 2-09 but 2-06 (row below). GAMEPLAY: 2-09's crate now burns
      only while `button_1022` is held as a fireball passes, the original's rule - until step 114 the port's fire burned
      `crate_no_emissive_1004` through the closed `door_lift_1007` on tick 167 with no input.
- [ ] **G6. Fireballs never pass through portals** (`mayTeleport` lets one through once; the pair survives).
- [ ] **G8. A shot through a fire agent's flame becomes a fireball** (`ETHCallback_fire_agent` #254-265
      `burnProjectile`); the port's shot flies on. 20 levels. **A blocker since step 115**: no port route for 2-16
      (the ledge shadows the wall and the pit barrels from the fire agent's zone), 2-17 (its second wall 87 u from the
      zone, `door_lift_1111` between) and 2-19 (108 u), nor for 2-10 and 2-11 (B2). The search was direct ignition and
      one hop through barrels where the level places them; relays through barrels moved first were not searched.
- [ ] **G9. The reflector catches within `size.x` (32) and its leg is `min(|caster - agent|, remaining)`**
      (`ETHCallback_reflect_agent` L69063 #56-84, #148-196); the port's 16 is a guess and it mirrors the whole rest.
- [ ] **G10. The player moves as the decode says** - `speed 3`, `SetGravityScale(2)`, velocity set outright, in the
      air `vel.x x min(1, 0.86 x fps/60)`; the port's `move_toward` numbers are Godot-era guesses.
- [ ] **Release-to-fire**: the original fires on release, if the pointer moved no more than scale(18)
      (`hasPortalRequestInput`); the port fires on press.

**Presentation** (VERIFIED unless marked).
- [ ] **P1. A chapter's last level ends on a dead "Chapter N complete" line** (only Esc leaves); the original goes to
      the chapter select, and after 4-32 opens `GameFinishedPopup`.
- [ ] **P2. The win cutscenes**: 4-01's rocket launch, 4-19's landing, 2-32's dragon knight, with `end_delay` (5000 /
      14500 / 6600 ms) and their music; the port holds a flat 1400 ms.
- [ ] **P3. 20 of 22 fire agents wear the anti-portal agent's face** (`ETHCallback_fire_agent` #1-7 sets
      `fire_agent.png` every update).
- [ ] **P4. Blue and red keys, keyholes and doors draw yellow** (`setKeyColor` L90240 swaps to `_blue`/`_red`
      sprites); the non-teleportable crate's red pulse (`manageNonTeleportable`, 600 ms) is not drawn.
- [ ] **P5. SpaceSky** (spiral, planet, eclipse) on the 19 space levels. KNOWN only as a cause.
- [ ] **P6. The sound pass**: the landing thump, keys, crates, stones, blasts and fireball impacts, the timed crystal's
      alert, the bosses' cues; `playExplosionSound` plays ONE sample chosen by its argument, and
      `test_mp_sounds.cpp:193` pins the misdecode "both".
- [ ] **P7. Music**: `no_music` levels, boss tracks starting and stopping, the level's track kept in the selectors.
- [ ] **P8. Achievements**: about 60 conditions in `checkForLevelAchievements` (27 of 82 can unlock today), the saved
      set, the "Achievement unlocked!" card, the "This level is locked!" card, the AchievementsPopup.
- [ ] **Smaller**: sound switches not saved; the log erased at each launch; tutorial helpers (1-21's camera hint,
      the tap-away hands); the beholder's death explosion; `fire32`, `antiportal_explosion`, `carranca_destroy`,
      `small_explosion_no_light`; the closing lift door crushes nothing; `death_area` kills only the player in a 16 px
      box; no teleport while a platform sits in a portal; stale notes (`end_delay`, `Camera.hpp:6-10`,
      `entity_roles.json`'s cosmetic `arrows` and `red_key_state`; `Turrets.hpp:17-22`'s and `diamonds.json`'s
      `_fireball_note` were corrected in step 114).

**Tests.** Step 113 built both: the audit's flood is `tools/parity/reachability.py` (the port's own rules read from
`data/` and `game/data/`, verdicts pinned, ctest `parity_reachability`), and `test_mp_routes` replays every route in
`tests/routes/` (11 chapter-1 levels won by play). Each gameplay step adds a route for every level it unblocks, and a
step that builds a breaker adds its `BREAKERS` row and rewrites the pin (B1: a `barrel` row, the nine to opened).
- [ ] **No route yet for 1-07, 1-10, 1-17** (step 113 left them: 1-17 tried headless, the pocket sealed to shots
      until its crates are cleared; 1-07 and 1-10 read from their maps only). Puzzles needing authoring time, no port
      defect seen.
- [ ] **Ruff in `tools/parity` is scoped by an `include` list** (step 113, deviation 1): a new script is not checked
      until listed. Fix: `extend-exclude` naming the old capture scripts in place of `include`.
- [ ] **`test_mp_routes` runs `OnUpdate` every tick** as the app does under `--fixed-step`, where `test_mp_layer`
      mostly does not (step 113, deviation 2); the 11-of-11 match with the window is its only evidence.
- [ ] **The reachability flood is optimistic** (step 113, deviation 3): a point, not the 20 x 44 body; no no-portal
      zones; locked and switched doors and dynamic bodies free - so 4-19, 3-13, 4-21 and 4-32 read reachable and
      are settled only by their routes. *And about barrels* (step 115, deviation 1): a barrel and any fire on the
      level open a wall, whether or not that fire can set the barrel off within the wall's reach - so 2-16, 2-17,
      2-19, 2-21 and 2-24 read opened with no port route (B1's row).
- [x] **`--route` without `--saves` writes the player's own save** when the run goes past the level's end: say so in
      its `--help` line, or give it a throwaway saves directory (code; the next step touching `main.cpp`). Step 113's
      runs left gold for 1-02 and 1-32 in `%APPDATA%/Magic Portals/scores.json` - the owner deletes it for a fresh
      save (below). *Step 114's `--help` says it; its own window runs used a throwaway `--saves`.*
- [ ] **2-06 (`level5a`) has no route: it needs G6** (VERIFIED from its geometry and flags). The door's pocket (x 300-454 above `platform_no_emissive_ent_824`)
      is sealed by `block00_no_emissive_ent_823` and `crate_no_emissive_796` on it (teleportable 0, burnable 1), up to
      the ceiling. Fire alone removes the crate, and the level's one carranca fires along y 202, under the platform:
      only a portal pair that carries its fireball (G6) reaches the crate (step 114).
- [x] **2-09 (`level8a`) is won by a route** (step 114's fix round: tick 1319, 14 shots). The fire burns
      `crate_no_emissive_1004` only while `button_1022` holds `door_lift_1007` open (before step 114 the port burned it
      through the closed door on tick 167 with no input), and nothing dynamic in the level can hold the button, so the
      player holds it and goes back round: down with the self-running lift (G2), up the right side a pair at a time, and
      from under `wall04_no_emissive_ent_1006` a pair into the hole the crate left. Step 114's first account - the pocket
      "within 64 u of (226, 208)", a permanent zone, the level unwinnable without B6 - was wrong for the port: the port
      plays `antiportal_1096` at 32 u (`placement.json` `radius_px` 16 x its scale 2) and `anti_portal_agent_ent_1095`
      at 16 u; 64 u is the original's (R1, not built). The route's last tap lies 42 u from the fields' centre, so R1's or
      B6's step (in the original the agent and its ring are shot away first) re-authors that pair. The door-dash (1-14's
      pattern) does not exist in the port: the fire room and the pocket lie under the button's platform or behind
      `block00_no_emissive_ent_1008`, so a tap into either from the button ends on the platform (the fix round's probe:
      both taps end on `single_block_plat_no_emissive_ent_1021`), and one pair at a time, spent by its first crossing,
      cannot both bring the player to the button and wait at the door.
- [ ] **Routes that ride the port's self-running lifts (G2)**: 2-02 (`level1a`) and 2-07 (`level6a`) board their lift
      as it comes back on its own, and 2-09 (`level8a`) rides `lift_ent_1016` up to the button and down from it on the
      lift's own clock; G2's step re-authors all three. 2-03 (`level2a`) hops each fireball through a portal
      pair, which G6 would carry the fire through: G6's step re-authors it.
- [ ] **`test_mp_dragon` NOT RUN in step 114** (Smart App Control refused it twice); the next step that links it runs
      it. (`test_mp_routes`, refused twice in the step, was relinked by its verifier and ran: 59 / 0, 18 of 18; the fix
      round's run, with 2-09, 62 checks, 0 failures, 19 of 19 won.)
- [ ] **4-32's torch shot under G5's rule is not played** (step 114): the dark dragon's fireball at the torch
      (`Game.cpp:702-714`) now dies on any solid body in its way; `light_off` stands at (210, 34) beside
      `single_block_plat_no_emissive_ent_2278`, and `test_mp_darkdragon` checks only the shot's origin and aim. Static
      only (step 114's verifier, `out/parity/program/G5/verify2/lanes_and_torch.txt`): from `test_mp_darkdragon`'s
      torch-shot origin (110.89, 43.94) the 9.6 circle reaches `light_off_1689` at 99.61 u and first touches
      `single_block_plat_no_emissive_ent_2278` at 110.10 u, clear by 10.5 u. For B3's step, which must play 4-32 anyway.
- [ ] **`Turrets.hpp`'s shock-agent bullet stops at #119** (step 114's verifier): the listing goes on to #146, a second
      destroy, the small explosion's sound and `burn()` of the agent. Text only, when the header is next touched.
- [ ] **2-05's crate stack parts from footage F6 after its second fire** (step 114, measured, not this step's rule):
      the port's `crate_small_no_emissive_ent_918`, stacked across 916 and 917, slides into 916's burnt place and takes
      the next left fireball at 5.27 s, where F6's L1 reaches the crate in front of the player at about 5.7 s; the idle
      player lives past 12 s where F6's dies at about 10.05 s. F6's crate is also gone about 0.6 s after its
      `small_explosion`, where `fire.json` `burn.time_ms` is 1000 (decoded). For the fire and crates' owner.
- [x] **The reachability `BREAKERS` citations name `Game.cpp` lines** that step 114 moved (the fireball block was
      rewritten): `:285`, `:364-370`, `:495`, `:636-641`, `:685-690`, `:775` are now `:286`, `:374-379`, `:490`,
      `:630-`, `:679-`, `:770`, in the pinned `_breakers` too. B1's step rewrites them with its `barrel` row; the
      verdicts did not move (`--check` 0 differ). *Rewritten in step 115* with the `barrel` row, against the code the
      step leaves.
- [ ] **A fireball that kills the player is gone before it burns what it also touches** (step 114, deviation 1):
      `Turrets::Tick` judges the player before the solid test; in the original both contacts begin on one Box2D step
      and both callbacks run. Only a player pressed against a crate or a barrel in the lane can tell.
- [ ] **A fireball's explosion lands one tick of travel (2.17 u) short** (step 114, deviation 2): the port places it
      where the contact is found; the original moves the fireball once more before it reads `destroy`.
- [ ] **Box2D's polygon skin is not modelled** (step 114, deviation 3): `b2_polygonRadius`, about 0.18 level units,
      so the port's fireball touches a polygon about 0.18 u late.
- [ ] **2-06's fire lane ends on the self-running lift** (step 114, deviation 4): `lift_ent_832` comes down into
      `carranca_ent_827`'s lane on its own and the first fireball goes off on it (x 192.3, tick 200), where the
      original's lift idles at its marker until tapped. `test_mp_turrets` prints this lane and pins the other 19; G2's
      step pins it.
- [ ] **`test_mp_routes` NOT RUN in step 115** (Smart App Control refused `6090631b`, after its one relink
      `c8d1403d`, and the verifier's relink `bd6544ad`); its evidence is the window replay of all 23 routes on
      `MagicPortals.exe` `ff41bf4e`, 23 of 23 won twice with the same ticks, the 19 old ones on step 114's. The next
      step that links it runs it once; that run is the first headless replay of 2-12, 2-13, 2-14 and 4-16 (authored in
      the window), and a mismatch there re-authors them, not a regression. Until then those four are won by a route
      the window replays, not yet by one a suite replays. Nor were `test_mp_launchers`, `test_mp_boss` and
      `test_mp_start` relinked, which read `demolish.json` through the loader that now requires `blast.entities`
      (their sources unchanged; the 128-level sweep starts every level through it).
- [ ] **The rest of `explode`'s destroy set** (step 115): a blast also sets `destroy` on a carranca (flagged breakable;
      `carrancaCallback` L85350 #128-183: `carranca_destroy.ent`, `startEarthquake(20, 0)`, `DeleteEntity` - 2-18's and
      2-23's barrels stand 54 and 56 u from one), a shock agent and a minion (burnable; `ETHCallback_shock_agent` L92534
      #294, `ETHCallback_minion` L87616 #1478) and a light wall (`ETHCallback_light_wall` L98075 #32). The port's blast
      breaks walls alone (`demolish.json` `blast._not_built`); each needs its own module's death.
- [ ] **A stone breaks a carranca in the port only** (step 115): `Demolish::Find` holds every flagged breakable,
      carrancas included, and a stone's touch breaks any of them; the original's stone callback,
      `ETHBeginContactCallback_breakable_wall` (L68341), is `breakable_wall`'s alone. And it breaks only above a speed
      (#16-33: faster than `scale(1.5)`), where the port breaks on touch.
- [ ] **A fireball's touch and its barrel's blast are a tick apart** in the port (Fire's flag-and-poll split), where
      `ETHScene::Update` runs the barrel's callback after the physics step that set its flag, on the same frame
      (`ETHScene.cpp:464-467`). F6's frames (50-100 ms) cannot tell. (A chain ripples a tick at a time in the port;
      in the original a barrel asked by another's blast goes the same frame or the next, by the update order.)
- [ ] **2-14's route is tick-exact** (step 115): its barrel falls past a passing fireball at the one height its blast
      reaches the wall from; a step that moves the fire (G4), the shot or a body's fall (G10) re-authors taps 3-4.
- [ ] **A blast's grab differs from `explode`'s in four small ways** (step 115, deviations 3 and 5-7): the port's
      sphere meets a hull's bounding box, the original's the turned collision box (the same box for every breakable
      wall, all at quarter turns); the original's line-of-sight ray stops at a sensor (`b2World.cpp:976-995`), the
      port's `Raycast(..., includeTriggers=false)` passes it; `explode` #86-96 skips an entity when the ray reports no
      contact (the blast inside its shape), the port grabs it; `EntityGrabber` #13-23 grabs only from the 3x3
      neighbouring buckets (L63238), the port anywhere in 80 u. All predate the step (burnables and bombs), and none
      lies on the four routes' blast lines (the only sensors there, `button_1050` on 2-12 and `button_1112` on 2-14,
      are off them).
- [ ] **2-24's kept-out barrel is untried** (step 115 (d)): a barrel left out of the seven-barrel chain to weigh
      `button_1144` while the rest break the walls; if it wins, 2-24 leaves B4.

**Before the owner's testing:** delete `%APPDATA%/Magic Portals/scores.json` for a fresh save - it holds gold for
1-01, 1-02 and 1-32, two of them from step 113's automated runs.

**The owner's calls** (gathered here; each decided as the original does unless he says otherwise): the grant in
`docs/permission.md` (gates handing a build to anyone else); camera dragging (his 2026-09-11 ruling "no dragging"
against the decode's 2x-swipe pan on 127 of 128 levels); R24/R25; step 110's fire-signal orbit and the put-out test at
the ball; the port-wide floored clock and the picture a tick behind; 12.1's summon clock; 11.3's death before the door;
10.2's pickup rule and 10.3's carriers; 11.1's four; Q.2's spawn point; 13.1's off-screen kill; the zero-g recoil
speed; the pause's Achievements button on every level; 4-02's easter-egg popup; target platforms.
