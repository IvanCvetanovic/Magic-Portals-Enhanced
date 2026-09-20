# Magic Portals parity — backlog

State at 2026-09-19 (first written 2026-09-15). What remains to make the **Supersonic port** (`game/` here since 2026-09-19; before, `Desktop/Supersonic-Engine/games/magicportals`)
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

- [ ] G6 leftovers: the screen overlay (HUD, pause, menus) is not quantised; the torch light's z is a `_guess`;
      `light_from_projectile.ent`'s own sprite and flame are not built; no capture of 4-22 lit (footage L1 missing).
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
- [ ] Footage to record: L1 4-22 torch lit then put out (3 s each); L2 1-2 shot along a wall; L3 a W2 explosion near crates.

## 2. Entity visuals (plan: `visuals/plan_port.md`, ranked)

**The batch order, `visuals/briefs/00_order.md` §7 — batches 0-4 closed 2026-09-19.** None shelved. The commit is on SE
main and pushed, and "Step" is the SE planning doc's number since the merge. The commits for 3.1-4.3 were made on
`visuals-b` and their messages still say steps 80-85. "Recorded deviations" are the step's own RECORDED DEVIATIONS
section. Where they have rows below, those rows stay open. Next: batches 5-13 (about 25 steps).
**3.4 is out of batch order on purpose**: §7 had dropped it for "no data, no footage, no decode", and §8.13 proposed
it back from decode D10 after batch 3 had closed, so it is built during batch 5's run and filed in its own place here.

| Order | What | Outcome | Commit | Step |
|---|---|---|---|---|
| 0.1 | JPEG size probe | committed | `12a10f3` | 60 |
| 0.2 | `sim/Tiers`, one cut rule | committed | `823e3d7` | 61 |
| 1.1 | facing, stride, column carry-over | committed with six recorded deviations (the six "Step 62" rows below) | `441ce6e` | 62 |
| 1.2 | space suit on the 18 `no_gravity` levels | committed with three recorded deviations, none a failing gate row: the brief's "zero-g never walks" decoded otherwise (the column turns while standing), the `no_gravity_motion` block, the hover is 1.2 screen px | `a0207e1` | 63 |
| 1.3 | pushing rows | committed with five recorded deviations: the wall gate ran on 1-22's wall (level0 has none on its right); row 2 appears the step after the release; the ray as decoded, not as briefed; data shape; the airborne aim follows the port's `air_control` `_guess` | `9c454a7` | 64 |
| 2.1 | converter copies tier files, writes a manifest | committed with five recorded deviations, all on code and file layout: `tiers.py`, manifest shape, densities read from `app.enml`, a tier file with no 1x file copied, staging run | MPR `e9611e4`, SE `5de8509` | 65 |
| 2.2 | level art through the resolver | committed with seven recorded deviations; the bare-ground row is met as amended and re-baselined at 5,312 px | `c404615` | 66 |
| 2.3 | port art and particles through the resolver | committed with three recorded deviations: a cut particle sheet is still split evenly (`tesla_shock_black_bg`, `shock_strike`; for 13.1); the UI fold-in was not taken; the level1c pin was rewritten | `494f6e2` | 67 |
| 3.1 | timer dial, no fade | committed | `0be2725` | 68 |
| 3.2 | crystal and key bob, `sim/Motion` | committed | `5ba5b86` | 69 |
| 3.3 | sway, alpha 0.55, dashed-circle spin | committed | `a32d356` | 70 |
| 3.4 | the button sinks six units under what stands on it | committed with four recorded deviations, no gate row unmet (6.0 u wanted, 5.957 measured; 4.24 wanted at 150 ms, 4.285 measured): 6 `.esc` units are taken as 6 port units (the 0.976 ratio would make the original's own sink 5.86 u, and `movers.json` says not to tighten the gates on it); the >= 300 ms row rests on a box capture, because 1-31's floor picture buries the button at full press; the bucket-visibility clock is not built (D10 records it as mechanism only); the port's sensor is still the remake's 16 px fallback against the original's 26 x 16. §8.13's "a button branch in `syncSprites`" is **superseded by R21**, which moves the body the picture already hangs on. **Gate level substituted**: level2a's button cannot be stood on (walled spawn pocket, reached only with portals), so 1-31 `level30`'s `button_975` is measured instead - same node y 220 over a floor whose top is 224. **Sweep baseline**: `5.2/verify1/sweep` still stands, EXCEPT `3-15 level14b`, now `7a3553fa` - a knight of step 76 stands on `button_1666` and sinks it. The PORT-WIDE interpolation row below was found verifying this step | `0f0142a` | 78 |
| 4.1 | antiportal drawn at 64×scale | committed with one recorded deviation (the 2-26 edge scan), closed by 4.2 | `bbb3c78` | 71 |
| 4.2 | blink and spin | committed with five recorded deviations (the four "Step 72" rows and the PORT-WIDE clock row below) | `5ae1b6d`, `5a544ac` | 72 |
| 4.3 | gravity-well green ring | committed with two recorded deviations, neither a failing gate row: the `test_mp_wells` gate is met across two suites, and the ring exists from the first frame | `6949cc3` | 73 |
| 5.1 | minions and knights on their sheets | committed with deviations, and **two sim changes applied under no adopted ruling - proposed R24 (the patrol's arrival point) and R25 (a carrier's point), THE OWNER'S TO RULE** (the "Step 76" rows below). Two gate rows NOT MET as written and recorded, not tuned: the non-regression sweep on 10 of the 128 f420 frames, where the repaired patrols carry keys, press 3-15's button and weigh 3-22's seesaw (each checked against the original's own frame and moving toward it), and G5a-5's lit R on one widened-series detection that sits on the line. Three deviations left to their owners: the lone guards' coast, the player's carrier point, a shock diamond's prey point | `9d25691` | 76 |
| 5.2 | the 2-32 dragon and its claw follow the sim | committed with deviations. One gate row NOT MET as written and recorded, not tuned: `test_mp_dragon` "stays at 151" reads **230 checks, 0 failures** - **R19** and 8.13 ("fireball direction pending R19") rewrite the aim the suite pinned, and the diff removes no assertion. Two deviations left to the owner (the "Step 77" rows below): two clock bases where the original keeps one, and the `2-32_h2.8`-versus-f168 flight-timing gap, which predates the step | `cbbf169` | 77 |

Merge `e81c21b` (§0). Integration step 74 (`591e143`, `7281ca5`) sends the well ring and the crystal dial through the
tier resolver. The ten rings now draw `hd/white_ring.png`. The dial keeps `timer.png`, which has no tier file. It
records no deviation of its own. It re-read 4.1's and 4.2's rows on the merged build, which gave the two open
instrument items below.

**Batch 5 opens after that merge.** Its first step, 5.1 (planning step 76), is the first step committed in this
repository rather than the engine's; 5.2 (planning step 77) follows it.

- [x] **Step 0** — engine `--screenshot-every N` (every temporal check depends on it). Done: SE step 57 (`29448a8`).
- [ ] System 1 colour transfer — largely done by lighting E1/G2; re-check its remaining rows.
- [ ] **System 2 particles**: emitter origin at the entity position (torch flame 13.4 u too low), spin sign
      (counter-clockwise ~108°/s), alpha, depth (particles 2.5 layers in front of their sprite), square quads,
      following moving owners; AM_MODULATE blend (engine has alpha + additive only).
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
- [ ] **System 5 actors with sheets**: the minions and knights (62 markers in 35 levels, and the chapter-3 boss's
      escorts) are drawn on `hd/minion.png` cut as the original cuts it, on the decoded rows 1/2, behind the player at
      its decoded depth 4 (R10), on patrols that now arrive where the original's do - **built, planning step 76**
      (order 5.1; committed 2026-09-20 as MPR `9d25691`), with the "Step 76" rows below.
      The 2-32 dragon follows it: its own cell, its flight, its `Follower`-driven angle and R19's fireball, and
      its claw - **built, planning step 77** (order 5.2; committed 2026-09-20 as MPR `cbbf169`), with the
      "Step 77" rows below.
      What is left of the system: the dark dragon (12.1), the ghost (12.2), the knights' dark aura (11.4).
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
- [ ] **Step 76 deviation, a shock diamond's prey point**: `Diamonds::Tick` measures its 30 to a minion from that
      minion's body centre; `ETHCallback_shock_diamond` ins 421-430 measure it from the minion's `GetPositionXY`,
      11.1 higher, which is about 3.7 u more reach on a shared floor. Next: Diamonds, with the same point.
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
      DEVIATIONS and LEFT FOR THE OWNER, for example the evenly split particle sheet (step 67, for 13.1).
- [ ] System 7 spawned effect entities: teleport effects, player-placed portal spiral, pickup/explosion effects,
      `portal_fail*`, key/keyhole effects; carranca fireballs are tan placeholder boxes today.
- [x] System 8 light halos — landed in G5 (step 49); magnitude still open under §1.
- [ ] System 9 converter-carried per-placement data: scale, sprite frames, the levels' own particle systems
      (45 placements load the wrong data).
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
- [ ] UI2 leftovers: finished screen counter/medal past golden+2 not captured on a real play; door suck/sparkle effect and
      death burst/skull not built; 3-09 and 4-01 popups never raised at runtime; the pause's AchievementsPopup unmeasured.
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
