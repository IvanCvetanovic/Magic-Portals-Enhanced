# Magic Portals parity — backlog

State at 2026-10-15, after wave 4d-1: the final comparison re-taken on the fixed build (first written 2026-09-15). What remains to make the **Supersonic port** (`game/` here since 2026-09-19; before, `Desktop/Supersonic-Engine/games/magicportals`)
match the original, in the order it should be done. Every spec named here lives under
`Magic-Portals-Enhanced/out/parity/specs/` (gitignored, local only — re-creatable from the reference library).

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
- [x] Merge the program's wave 1 (branches `w1-b` and `w1-c`) into MPR `main` — done 2026-09-30 (`5e13f54` for
      `w1-b`, `187b41b` for `w1-c`; pushed, `origin/main` = `187b41b`). Track B's steps 150-155 became 119-124 and
      track C's 170-172 became 125-127 (the branches' own commit messages keep the old numbers); the merge note is
      planning step 128, whose table is the map. Five conflicts, each resolved keeping both behaviours; one
      earthquake table (32 rows) and one route trace. `test_mp_routes` 70 routes, 215 checks, 0 failures after two
      route fixes (2-09 one tap more, 3-31 re-routed); reachability 108 / 20 / 0, 0 differ; the 128-level f420 sweep
      128 of 128 rc=0, every change attributed to a track; the full ctest once, **26 of 47 passed, 0 failed, 21 Not
      Run**, and 30 suites ran on the merged code in all. **41 Smart App Control refusals.** An independent check
      (`merge_w1/verify1.json`) passed every gate and re-took the sweep byte-identical. Engine pin `4bfcf67`,
      unmoved. Worktrees `Magic-Portals-Remake-w1b` and `-w1c` and both branches removed. **The standing sweep
      baseline is `out/parity/program/merge_w1/sweep`.** Section 6's "Wave 1" paragraph has the steps. *Step 130
      (P0) moved the engine pin to `a733ca0` and re-took the sweep: 128 of 128 byte-identical, so the baseline
      stands.* *Step 131 (P1, clamp-to-edge) moves the outer half-texel of every picture whose opposite borders differ,
      so the standing baseline becomes `out/parity/program/P1/clamp/after/sweep` (the same bytes as
      `P1/clamp/fix1/sweep`); every earlier byte-identical baseline is stale.* *Step 133 (LD, the lift door) re-took
      the sweep: 128 of 128 byte-identical to it, so it stands.*
- [x] Merge the program's wave 2 (branches `w2-b` and `w2-c`) into MPR `main` — done 2026-10-01 (`c2d7682` for `w2-b`,
      `4d9ee56` for `w2-c`; pushed, `b25a426..4d9ee56`). Track B's steps 150-153 became 137-140 and track C's 170-172
      became 141-143 (the branches' own commit messages keep the old numbers, and the routes' marker key keeps its
      written name, `_step150`); the merge note is planning step 144, whose table is the map. 19 conflicts (14 and 5),
      each resolved keeping both behaviours, and one rule of the merge's own (GAMEPLAY on the handle levels: the flame
      a shot meets stands where its agent burns from, before the follow). Track A's 14 routes moved into the release
      meaning (78 shot taps a tick earlier); 2-06 and 2-18 re-authored. `test_mp_routes` 86 routes, 1,513 checks, 0
      failures; the runner 49 of 49 suites, 18,059 checks; reachability 108 / 20 / 0, 0 differ; the 128-level f420
      sweep 128 of 128 rc 0, the 17 levels only track B moved equal to B, 4-20 (only track C) equal to C, the other
      110 equal to the base - nothing unexplained. **3 Smart App Control refusals** (two runners and one game, each
      relinked once). An independent check (`merge_w2/verify1.json`) passed all 18 gates and re-took the sweep
      byte-identical. Engine pin `710bffc`, unmoved. Worktrees `Magic-Portals-Remake-w2b` and `-w2c` and both branches
      removed (`merge_w2/finalize.md`). The standing sweep baseline became `out/parity/program/merge_w2/sweep`; *step
      145 (EC) moved 41 dark-sky levels' edge columns and step 146 (UI) 24 levels' pictures, so **the standing
      baseline is `out/parity/program/UI/after/sweep`**.* Section 6's "Wave 2" paragraph has the steps.
- [x] **`test_mp_launchers` and `test_mp_keys` have not run on the merged binaries** (step 112). The merge relinked
      both; Smart App Control refused each twice (the first launch or the ctest, then the one relink), and the merge's
      independent check did not launch them. Their last runs: `test_mp_launchers` 105 on track B (step 109; 98 on
      track A at step 95) and `test_mp_keys` 124 on track A (step 98; track B did not run it), 0 failures each. Next:
      the next step that relinks them launches each once and records the count. *Step 118 ran `test_mp_keys` (169) on
      track A; both are again among the 17 suites NOT RUN on wave 1's merged code (step 128, the row "17 suites" in
      section 6's Tests).* *Closed by step 129 (T.2), on wave 1's merged code in the one runner `test_mp.exe`
      (`6c5f2626`), each in the one-process run and under ctest, the same count in both: `test_mp_launchers` 105 and
      `test_mp_keys` 169, 0 failures, first launch, 0 refusals.*
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
- [ ] **The folder's and the repository's new name** (2026-10-01, the owner;
      `out/parity/program/resume_2026-10-01.md`, `merge_w2/finalize.md`). The checkout is
      `C:\Users\icvet\Desktop\Magic-Portals-Enhanced` (`b25a426` named it in `CLAUDE.md`, `docs/parity.md` and here),
      and GitHub answers a push with "This repository moved" to `IvanCvetanovic/Magic-Portals-Enhanced`, through whose
      redirect wave 2's pushes went. **Done after the close:** `origin` is set to `.../Magic-Portals-Enhanced.git`
      (the commit that wrote this line was pushed to it) and `README.md`'s clone line names it. Still open: both
      venvs' console-script shims (`pytest.exe`, `pip.exe` and the others pip wrote) name the old folder: run
      `.venv/Scripts/python -m pytest` and `-m ruff` (`ruff.exe` is ruff's own binary, not a shim; it does not name
      the old folder). Step 132's identifiers and keys still say Remake
      (`Units::kRemakeWorldGravityPx`, `Game::UseRemakeGravity`, the `_remake_*_not_applied` keys, `compare.py`'s
      positional `remake`): a code and data step of their own.
- [ ] **For the owner, his file: `CLAUDE.md`'s workflow rule 2 still runs a suite as
      `build/tests/test_mp_<suite>.exe`** (step 129's LEFT FOR THE OWNER). Since step 129 every suite is in one
      runner, `build/tests/test_mp.exe test_mp_<suite>` (`test_mp.exe` alone runs them all, `--list` names them); the
      old executables are deleted, so the stale line fails loudly instead of running stale code.

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
      *Partly closed by step 166 (CMP-1, wave 4c-1), 2026-10-11*: `ignore_emissive`, `door_emissive` and the character's emissive in `Game::preLoop` (#355-393) are decoded,
      built and measured (`game/data/lighting.json` `script_emissive`); the crate callbacks (CMP-9) and the rest of this list are not touched by it, so the row stays open.
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
| 3.4 | the button sinks six units under what stands on it | committed with four recorded deviations, no gate row unmet (6.0 u wanted, 5.957 measured; 4.24 wanted at 150 ms, 4.285 measured): 6 `.esc` units are taken as 6 port units (the 0.976 ratio would make the original's own sink 5.86 u, and `movers.json` says not to tighten the gates on it); the >= 300 ms row rests on a box capture, because 1-31's floor picture buries the button at full press; the bucket-visibility clock is not built (D10 records it as mechanism only); the port's sensor is still the Godot project's 16 px fallback against the original's 26 x 16. §8.13's "a button branch in `syncSprites`" is **superseded by R21**, which moves the body the picture already hangs on. **Gate level substituted**: level2a's button cannot be stood on (walled spawn pocket, reached only with portals), so 1-31 `level30`'s `button_975` is measured instead - same node y 220 over a floor whose top is 224. **Sweep baseline**: `5.2/verify1/sweep` still stands, EXCEPT `3-15 level14b`, now `7a3553fa` - a knight of step 76 stands on `button_1666` and sinks it. The PORT-WIDE interpolation row below was found verifying this step | `0f0142a` | 78 |
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
  (10.4); `antiportal_explosion.ent` (8.13's unowned row, which also needs R23's in-flight kill; *built in step 126
  with R17's kill*); `fire32.ent` on a
  burning character (11.3) and on the burned knight (12.2); 2-32's claw's `wall_destroy` (11.1); a boss-dropped key's
  `key_pick` (the merge); the key's fly-in into its keyhole (10.3); the carried shock diamond at alpha 0.1 (10.4); the
  shock agent's pulse (13.1); the unlit torch's spin (11.2); the dark dragon's scattered `small_explosion_no_light`,
  its death's `explode` and `ghost_utility_spawn` (12.1); the ghost's DAMAGE and DEATH explosions (12.2); and the
  sounds 10.4, 11.2, 12.1, 12.2 and 13.1 name. *Step 146 (UI) built `fire32` on a burning character and on a burned
  minion, a boss-dropped key's `key_pick`, the carried diamond at 0.1, the shock agent's pulse, the unlit torch's
  spin, the dark dragon's scattered `small_explosion_no_light`, its death's `explode` (the picture; the grab is not
  built) and `ghost_utility_spawn`, and the ghost's DAMAGE and DEATH explosions (with the beholder's, `carranca_destroy`
  and the tutorial hands: section 6's "Smaller" row). Left: `shock_agent`'s `small_explosion.ent`, 2-32's claw's
  `wall_destroy` and the key's fly-in; the sounds those rows name are step 142's.*
- **The earthquake's follow-ups**: Q.1's gate 4 NOT MET (F16's +-10 opening; the owner accepts it or C52 is settled);
  whether the generator resets at a load; the shaken-camera readers the port keeps at rest, the skull among them
  (Q.1's deviation 6 and the merge's finding); D11 rows 3, 13, 21, 30 and 10 / 8's kill unwired (*rows 3 and 30
  wired in wave 1: `agent_killed` in step 126, `rocket_launch` in step 123*), row 9 dead on 3-20;
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
  *Wave 1 of the program built three of them, each as the original does: R2 in step 125, R17 (the anti-portal agent's
  kill, one of R23's in-flight kills) in step 126, R1 in step 127. R23's other items and R22 are still unbuilt.*
  *Wave 3b-2's step 156 (local, not pushed) built R23's tap: the order of its tests and the 400 ms clock (only the tap that
  reaches the hourglass restarts it; the cooldown and budget-0 taps do not, by the listing), the destination at the ray's
  `finalPos`, the head-height retry as the shot's own, a reflected leg that lands 10 short of a wall, the blocker read at the
  shot's position, the field flag read at the shot's position (G4-8), and the zero-g recoil aimed at `finalPos`; `light_off`'s
  kill, G8's flame and R17's agent were already built. What is left of it: the "Step 156's recorded deviations" rows.*
  *Wave 3b-2's close: R23's tap is built (steps 156-157) and its six routes are re-authored (step 161); R22 is the one still unbuilt here.*
- **The owner's calls still open**: **R24 and R25**, applied by step 76, still the owner's to rule (the "Step 76, THE
  OWNER'S TO RULE" row; the player's point is R25's other half, and 7.3, 10.1, 10.1b and 10.4 each record it again);
  **the two PORT-WIDE rows** - the script clock the original floors to whole milliseconds, and every body's picture
  drawn a tick behind the sim (both built under the program's rule: steps 147 and 150); **11.2's fire signal**, applied and the owner's to rule, and step 30's put-out test at
  the ball, which the original makes at the torch; and the gameplay rows this run filed for the owner without
  changing them - 12.1's barrel-bomb restock (4-32 cannot be won without it; built in step 116 under the program's
  rule, the original's), its summon clock and its wound's shot
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
- [x] **Step 62 deviation, facing a portal tap** *(built in step 148: `Portals::State::Shoot` returns whether the tap was taken and the layer turns the player, pinned by `test_mp_layer`'s `APlayerTurnsToTheTapItTakes`)*: the original turns the player toward a portal tap and the port never
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
- [x] **Step 72 deviation (84 on `visuals-b`), the port draws 1-07's antiportal ring BEHIND art the original draws
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
      **Its premises changed in step 118's fix round** (`docs/ethanon-formats.md`, the bucket paragraphs; not
      re-derived here): `Game::start` passes its own bucket, 128 level units (the five-argument `LoadScene`);
      `scaleEntities` moves every entity that does not carry `scalable = 0`, so the engine buckets the drawn
      coordinates; and a moved type-0 entity goes to the FRONT of its new bucket, so a bucket's order after the load is
      not its load order.
      *Draw order settled by step 167 (CMP3, wave 4c-1), 2026-10-11; ticked with its limit: the ring's gate as written was NOT re-read.* The port now draws pictures
      at one depth in the original's bucket order (`Sprites::Rank`; the bucket is 128 level units, `game/data/art.json` `draw_order`): `antiportal_687` (row 1, column 2)
      is over `platform_ent_7` and `block00_ent_702` (1, 1), as the original's stills show; 1-07's box reads 4.54 -> 3.20 mean abs against h6.3 (the stills' spread
      3.22) and the pair image has the red disc over all four blocks. This row's gate is a `c_R` blink reading (0.36-0.42 and 0.18-0.24, measured 0.2875 / 0.1786), and
      step 167 read the box and the pair image, not that estimator, so the row's "re-read the row as written" is still undone. Next: run the 4.2
      estimator once on 1-07 when the effects' phase is next measured (CMP-low a); nothing else of this row is open.
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
      *Step 147 (CK, wave 3a; committed locally, not pushed) builds the floor: `ScriptClock::Step` is the one script-facing
      delta (16 of the tick's 16.667 ms), the 70 functions that read the clock are classified in a table, and the physics step
      and the wall clock stay where they were, so every scripted timer, blink and fade now runs 4.17 % slower in wall time.
      Built bar one site, so the row stays open: the player's ground walk-cell flipbook is still advanced by the engine's tick
      (CK-1, section 6, G10's). 41 of the 86 routes still win on that build; the routes are re-authored by the wave's route
      phase after PIC.*
      *Wave 3a's close: the row stays open for that one site. G10 did not take CK-1 and PIC declined it (PIC-5), so the walk
      cycle still runs on the engine's tick until the final comparison or a step of its own. The 86 routes were re-authored on
      the floored clock in step 151 (`af008a9`): 84 win.*
      *Wave 3b-1: unchanged - CK-1 is still the one open site. The time base carries every body through the same floor
      (`ScriptClock::ScriptSpeed`, step 152) and the 86 routes were re-timed on it in step 155.*
      *Wave 3b-2: unchanged - no step took CK-1, which is still the one open site.*
- [x] **PORT-WIDE, found verifying step 78 (order 3.4): the picture of any body is drawn one tick behind the sim.**
      ***BUILT by step 150 (program wave 3a, PIC, local, not pushed): see the paragraph at the end of this row.***
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
      ***Built in step 150*** (program wave 3a, PIC; the premise above that a debug box carries no interpolation did not
      hold - `buildDrawables` has given every body's box and the player's one since step 7 - so the gate was measured against
      the simulation's own body, which the engine never interpolates): `MagicPortalsLayer::drawPicturesAtTick` ends every
      tick by making the tick's end both ends of every interpolated picture and of the camera, so `InterpolationSystem::Apply`
      returns the tick's own place at any alpha; the picture's lag is **0.0000 ticks** at alpha 0 / 0.25 / 0.5 / 0.75 / 1 (the
      control on a smoothed display reads 1 - alpha), and on the real binary the player's picture is 7 px (one tick of the
      walk, 6.75) ahead of the old build's at the same frame. **In real time above 80 Hz the engine's smoothing is kept**
      (`game/data/pictures.json` `smoothing.above_hz`, marked `_guess`; 0 draws at the tick's place at every rate; always off
      under `--fixed-step`) - the owner's to confirm (planning step 150, LEFT FOR THE OWNER). No gameplay, no route and no tap
      moves: the route census is G4's, route by route, and three traces agree tick for tick. The f420 sweep: 68 of 128 levels byte-identical against G4's, 60 changed and every one explained by a picture or the camera on the view changing on tick 420, the capture's own - the 17 space levels' turning spiral (0.12 u a tick), 13 minions' 0.65 u, three thrown stones, the beholder, 2-32's sky cover and the camera of 1-24 and 2-32 - and 59 of the identical ones had nothing on the view change at all (`PIC/explain_sweep.txt`). **The routes are re-authored by the wave's route phase after PIC**: this step moves none (8 of the 86 win, the same 8 on the same ticks as at step 149). The step's deviations are the rows PIC-1 to PIC-9 in section 6. *Committed `d17e7d3`; the routes were re-authored in step
      151 (`af008a9`) with the picture at the tick's own place: 84 of 86 win.*
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
- [x] System 4 antiportals: drawn 64×scale u (not a flat 128 u) — **built, SE step 71**; the red blink (0.2 → 0.4 → 0.2
      on 300 ms legs) and the 10°/s turn — **built, SE step 72** (order 4.2), with the five deviations above. What is
      left of the system: the refusal radius stays 16×scale in the port against the decoded **32×scale**
      (128 u × the manager's `Scale(0.5)` × the node's scale, halved; footage F3 brackets the original's
      refusal at (31.6, 32.4]×scale). Ruled R1 — what the original does — but no step in 00_order §7
      plays it yet. ***R1 built in step 127*** (program wave 1, track C, `3e51904`): an antiportal refuses a portal
      within 32 x its node's scale (`isPointInAntiPortalField`'s half `GetSize().x` over the manager's `Scale(0.5)`;
      F3's bracket (31.64, 32.36] x scale; the window on 1-07: 91.2 u refused, 100.0 u accepted), and the drawn ring
      is the played circle. `placement.json`'s "note of 64" corrected: 64 is the diameter. GAMEPLAY on the 52 levels
      that place the 71 antiportals. 44 of them won by new or re-authored routes in the step, 6 by track A's; 4-28
      and 4-14 not won (section 6, Tests). The f420 sweep 128 of 128 byte-identical (no level taps by f420). Left:
      the strict `<` at the edge (`placement.json` `_not_modelled`), `no_portal_705` a port-only zone inside
      `antiportal_687`'s field (*gone in step 156*), and stale comments (`game/sim/Portals.hpp:61-63, 66-69, 149-150, 463-466`,
      `game/MagicPortalsLayer.cpp:5141-5143`) for the next code edit of those files.
- [x] **Gravity wells, the refusal (R2)** — the green ring is **built, SE step 73** (order 4.3, `6949cc3`, made as
      step 85 on `visuals-b`; drawn from `hd/white_ring.png` since step 74); the gameplay half is not. Decided for the
      original: its well refuses no portal and its shot does not die on one. `'gravity_area'` is in exactly two functions of the listing (`ETHCallback_gravity_agent` ins 29,
      `checkForLevelAchievements` ins 1542), and `AntiPortalManager` fills its `fields` from `Game::preLoop` before
      any agent has added its area (00_order §8.4, §8.14). The port still refuses: `Game::Start` hands each well to
      `Portals::State::zones`, and `test_mp_wells.cpp:312-313` assert it. 00_order §6 lists it as a gameplay hand-off
      in no batch. Next: drop the well zone, let a shot pass, flip the two assertions, with its own captures. It
      cannot move the ring: the picture is sized by `Well::RingSizePx` and the zone is derived from it.
      ***Built in step 125*** (program wave 1, track C, `ce65260`): a portal opens anywhere inside a well's ring, and
      the shot flies through a well's agent (`FirstBodyPassing`). GAMEPLAY on the 5 well levels (10 wells), as the
      original does. Won by routes: 4-17 (tick 127), 4-18 (125); 4-28's route of that step was later refused by R1
      (step 127). 4-21 stays unwon, on the lift door (section 6, B7). The f420 sweep 128 of 128 byte-identical. Left
      (tidy, each a relink): `AShotFliesThroughAWell` pins `shotPasses` on one well of ten; `NoPortalZone::radiusPx`
      is set by nothing; `gravitywell.json`'s `zone.shrink_px` is the ring's number under the zone's name;
      `Portals::State::Tick` copies `shotPasses` every tick a shot flies.
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
      capsule's centre, **22 u over its feet** (`data/player.json`'s 20 x 44 body, the Godot project's guess), where the
      original's entity point stands **18.1** over them (`dark_mage.ent`'s compound, `ETHCompoundShape.cpp:66-69`).
      So a key or a diamond the player carries rides 3.9 u low, and both its 26 and the diamond's 30 are polled from
      3.9 u low. Not applied here: the capsule is the player's body to settle, not the carry's. Next: the player's
      body, with `Carry::Carrier`'s point already in place to take it.
      *Closed by step 153 (CAP, local, not pushed): the body is `dark_mage.ent`'s own (36.2 tall), so its centre IS the original's entity point, 18.1 over the
      feet, and `Carry::Carrier`'s (0, 0) is right; the picture's residual is no longer the 3.9 u but the engine's rest skin (CAP-2, 1.6-1.8 px low). R25 itself,
      the carrier point as the decode has it, is still the owner's.*
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
- [x] **Step 76 deviation, the lone guards' coast** (beside R5; CLOSED by step 152, text only here: `minion.ent` friction 0 in `materials.json`, and `test_mp_layer` pins the guard of 3-01 holding on column 0 of row 2 after coasting in to its post): the original's guard is frictionless (`minion.ent`
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
      *Step 147 (CK) did the unification: `Dragon::State::Tick` takes the script's step, so `elapsedMs`, the claw's sweep, the
      flight and the follower count one 16 ms. The row stays open for what it also asks, re-gating 5.2's schedule on it (CK-8,
      section 6).*
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
- [x] **Order 11.3 deviation, the character's point is its capsule centre** (step 96): the door's sparkles and a
      character's blood sit about 3.9 u high (22 u over the feet against the original's 18.1). R25's player half. *Gone for the player in step 153 (CAP): his capsule's centre is 18.1 over his feet.*
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
- [x] **Order 11.3 deviation, `fire32.ent` has no step** (step 96): 00_order section 5 names F13 for "11.3 `skull`,
      `blood`, `fire32`", section 7's 11.3 and `system_7` 7f do not. F13's fire death shows flames the port does not
      draw. Next: the owner assigns it. *Built in step 146 (UI)*: `fire32.ent` on the character a fireball or a fire
      agent's flame kills, at its frame's width over 64 (0.625), riding its body. On 2-10 its onset and size match
      F13's and its life the decode's; where it burns (in place, where F13's sink with the shrunken body) is step 146's
      deviation 5, in section 6's row "Step 146's recorded deviations".
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
      `_guess`; fix round 2; the decoded 18.2 since step 153) - where the original's is `scaledCollide(finalDoor, character)`, a plain overlap of the two
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
      *Joined by step 167 (CMP3, wave 4c-1), not closed*: step 167 deals the level's PICTURES at one depth by the original's bucket order; this row's flame quads are
      effects, ordered by the engine's transparent sort and K13's table, and no measurement of step 167 reads these 17 pixels.
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
      *The `holdTime` cap (#12-55) was built in step 159 (G1a-10, G1b: a touch caps a knight's hold at 1000 ms).*
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
- [x] **Order 12.2, THE OWNER'S TO RULE: fire against the ghost** (step 98; F15 finding 1). The original's fireball passes
      through `ghost.ent` (a sensor: `ETHBeginContactCallback_fireball` #46-121 returns on one) and only a barrel's
      `explode()` burns it; the port keeps its fireball test (within `radius_px` 64) because the barrel restock
      (`addEntityIfItCantBeFound('barrel_bomb')`) is not built - explode-only would leave the port's ghost unkillable.
      Also unpinned: a fireball while the ghost is APPEARING sets `burned`, counted on its first SHOOTING tick. Rage's
      picture and luminance (step 98) were reached only through this port-only rule. *Built in step 117, as the
      original does it (the program's rule)*: the 64 px rule is gone, a fireball flies through the ghost, and a
      barrel's blast burns it (`Ghost::BlastReaches`: the grabber's 80 against `ghost.ent`'s 128 x 139, then the ray
      to its body, 38.4 x 41.7 at the add's scale, INFERRED); the ghost puts the barrel back itself (the row below).
      A blast in any arm leaves `burned` set, counted on the next SHOOTING update (pinned for a blast now, not a
      fireball). GAMEPLAY, 3-32 only. **Won by a route: 3-32**, tick 2724.
- [x] **Order 12.2 finding, `no_music` and `isBossFight`** (step 98). `Game::preLoop` #485-509 plays
      `playGameMusic(m_levelIndex == 31)` unless the level's properties carry `no_music` (`readProperties` #166-181): the
      original is silent at load on level0c, level18c, level31b and level31c, and plays the boss's music from load on
      2-32. The port plays "game" on all of them (and "boss" on 1-32); step 98 switches 3-32 to the boss's music from the
      ghost's init and to silence from its DEATH. Audio only. Next: the sound's owner. *Built in step 121 (section 6's
      P7): silent at load on the four, `boss` from load on 2-32 (and 1-32), each world's last level read from the
      chapters table.*
- [ ] **Order 12.2, not built** (step 98, deviation 6): the ghost's and its shot's sounds (the shot's
      `startEarthquake(10)`, #331-333, is `earthquake.json`'s `enemy_shot_end` since the merge, step 112); the DAMAGE
      and DEATH explosions and the death blast's 2.0; `ghost.ent`'s static box, which fails a
      portal ray across it (F15 finding 2, `computePortalFinalPos` #30, #136-175; step 117 infers that body at 38.4 x
      41.7, the `.ent`'s box at the add's scale 0.3, where F15 read 128 x 139 - and 3-32's route crosses it with two
      taps a cycle, so this row's step re-authors them); the static bucket clock (a ghost off the
      view does not advance in the original; a knight killed on the floor at y 768 adds it far below the view); an
      explosion's burn of a minion (level18b's bombs). 4-32's dropped yellow key's picture stays 12.1's.
      *Step 142 plays the ghost's and its shot's sounds (its appear, item, damage, death and small explosions,
      `addEnemyShoot` #172 and `ETHCallback_enemy_shoot` #354-356), and step 146 draws DAMAGE's and DEATH's scattered
      `small_explosion_no_light` and the death's `explode` at x2 (its picture; its grab is not built). The static box,
      the bucket clock and an explosion's burn of a minion stand.*
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
- [x] **Order 12.2 deviation, no `fire32` flames on the burned knight** (step 98, deviation 9; found by the independent
      check). F15 r1 #157-#171 shows about 1.3 s of flames on the knight's spot - the minion's burn's `addFlames`
      (`ETHCallback_minion` #1456-1471); the port draws the explosion and the blood only (`12.2/verify1/f15_vs_port_0.png`
      at +0.73 and +1.26 s). `addFlames` is not built (`Minions.hpp`, `Death::byFire`). The same entity as the "Order
      11.3 deviation, `fire32.ent` has no step" row above. Next: the owner assigns `fire32`. *Built in step 146 (UI)*:
      `fire32` on a burned minion (3-32's knight and the ghost's escorts, 4-28's knight) at its frame's width over 64
      (0.59375), before its blood, left where the minion was deleted. Seen on 3-32's escort (burned on 1032) and 4-28's
      knight (605); the knight's own burn on 264 is off its route's view.
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
      *The restock is built in step 116* (with the lava's contact, B3), and 4-32 is won by a route; the rest of this
      row stands, `ghost_utility_spawn.ent` at each restock with it (the "restock's pictures" row). *Step 146 draws the
      scattered `small_explosion_no_light.ent`, the death's `explode` (its picture; the grab is not built) and
      `ghost_utility_spawn.ent` at each restock and at key_pos.*
      *Step 142 plays the dark dragon's sounds: its appear, the open mouth, the wounds, the item with `killAll`'s
      portal sound, the death and its adder's scattered explosions. What is left of the row is the explode's grab
      (gameplay).*
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
      *Now three: step 138 added 1-21's hand, which step 139 made `Hint::Round`, shared with 2-20's lesson
      (`Ghost::Appear`, `DarkDragon::Path`, `Hint::Round`; the third repeats).*
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
      *The second half is answered by step 137 (RF): the original fires on touch-up (`hasPortalRequestInput` #1-9
      tests `GSKS_RELEASE`; footage F14, F15 and F16), and the port fires a press and a scheduled tap alike on the
      release since. The pause's buttons still read the mouse on the press (step 137's deviation 2); the first half
      stands.*
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
      *The lift's (step 140), the arrows' (step 139) and `hand_tap`'s (step 146) hit tests are built since; whether
      each reads the shaken or the rest camera was not checked (step 139's note).*
- [ ] **Order Q.1, D11 rows still unwired** (step 101's NOT WIRED list, as 10.1 and Q.2 left it): row 3
      (`antiportal_explosion`, a shot an `anti_portal_agent` kills: R23); row 10 (a stone crushing a minion: the port's
      minions are not breakable) and row 8's other half (a character killed above hitIntensity 2.5) - gameplay; row 13
      (`shaker`, placed by no level); row 21 (a burned carranca) and row 30 (4-01's rocket), systems the port lacks; row 9
      on 3-20, whose placed stone the port never culls. And three wired rows no case drives: `beholder_death`,
      `ghost_death`, `dark_dragon_death` (a boss beaten). Next: their owners; a boss-win case for the three.
      *Wave 1 wired two: row 3 as `agent_killed` (intensity 20, after_update; step 126, R17) and row 30 as
      `rocket_launch` (3, after_update, re-armed past 100 ms; step 123, 4-01's win). One table of 32 rows since the
      merge (step 128), 28 of them wired. Rows 8's other half, 9, 10, 13 and 21 remain.*
- [ ] **Order Q.1 deviations, recorded** (step 101; none a gate failure): the 50 px cap and the 1 px stop taken at R12's
      720 px screen (deviation 2; a 1080 px phone's cap is 11.85 u); the light wall goes on its own tick, the original's
      with its first spike (5); two calls on one side of one tick in the port's order (8); the fireball's 5 a frame early,
      with its explosion (9); `SpaceSky` not ported, so the 19 `space_bg` levels shake their whole background (10;
      *closed by step 122*: `shakeView` moves the space sky, spiral, planet and eclipse with the shaken camera); no
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
      is R23's, R1's, R2's or R17's gameplay. Next: those rulings built. *Wave 1 built R2 (step 125), R17 (126, the
      agent's in-flight kill) and R1 (127): the fields are now the original's for antiportals, wells and agents. Open
      for R23: the hourglass's clock, the sensor and teleportable-body disagreements, the head-height retry, the
      reflected shot at a wall, and the flame's in-flight snuff (G8's `burnProjectile`); and `no_portal_705`.* *Step 156 (R23) built
      them: the shot flies by the ray, so the mark and the shot's end are one answer; `no_portal_705` is no zone (it is the sign's picture
      and `AntiPortalManager` gathers only `antiportal` and `antiportal.ent`; it lay inside `antiportal_687`'s field, so no tap's answer moved);
      the in-flight kills not played before are G8's and R17's (built) and `light_off`'s (built, step 30; its `portal_death` at the shot, step 110).*
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
      player's body overlaps its trigger box (Goals, the Godot project's rule); `MainCharacter::crystalPicker` takes it when the
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
      header comment is stale. Next: the key's picture owner. *A boss-dropped key's `key_pick` is built in step 146,
      where the runtime last drew its `key.ent`.*
- [ ] **Order 10.4, not built, and recorded** (step 108): `shock_agent`'s `small_explosion.ent` where a fireball destroys
      an agent (#331-345; deviation 2 - a one-row addition beside `shock_death_at_agent`, owned by no step); the carried
      shock diamond at alpha 0.1 (its lightning still draws at full strength while carried; `diamonds.json`
      `_not_built`); the sounds (`playShockDiamondPickSound`, `playShockStrikeSound`, `playBurnSound`,
      `playFireExtinguishSound`, `playLightOffSound`); F14's standoff (its knight struck 27 u off, where the port's route
      walks the knight into the player); the effects' onset - the strike's white peaks about 0.04 s after F14's, the
      water's plateau starts about 0.12 s after L1's; `Torch.hpp:42`'s stale comment (`Fields.hpp`'s was fixed by 13.1)
      and `system_7.md`'s "unreachable" for `ETHCallback_shock_agent`. Next: their owners. *The carried diamond at 0.1
      is built in step 146 (the sprite only; its lightning stays at full strength, as the original's).*
      *The sounds are played since step 142: the shock diamond's pick and strike, a burn, a fire diamond's extinguish
      and the light's off.*
- [ ] **Order 11.1, GAMEPLAY rows for the owner** (step 109, deviations 1-4; each moves the effects the step draws with
      it): `destroyable` stones are not destroyed by the wall they break (`ETHBeginContactCallback_breakable_wall`
      #42-60) - 12 placed stones on 10 levels, and every stone an `entity_launcher` throws (#200-209 set the flag), so on
      1-24 and 1-25 the original's thrown stone dies at the wall it breaks while the port's rolls on to the despawner;
      3-20's placed stone is never taken by its destroyier; the port's lift door (`Mover::Door`) rises 126 u where the
      script rises 0.8 x h = 102.4, and turns wherever its channel turns it where the script opens it only from shut and
      closes it only from fully raised; the beholder breaks any demolisher stone rising into it, the original only
      `rolling_stone.ent`. And two wall breaks the port does not do: a bomb's blast destroys every breakable it grabs
      (`explode` #111-122; `Fire.hpp:52-56`, nine chapter-2 levels - *the walls built in step 115*, B1), and 4-19's
      `red_key_state` destroys `breakable_wall_2756` 6000 ms after its key is destroyed (recorded nowhere else; *built in
      step 118*, B5 - its key TAKEN, the destroyed being the state's own datum). Next:
      the owner. *Program wave 1 (steps 125-127): the lift door's latch now blocks a level - 4-21 cannot be won in the
      port (a portal reset over `button_1723` raises `door_lift_1721` 5.6 px at most, against more than 15 px for a
      shot line and 43 px to walk under; section 6, B7). Decided for the original by the 2026-09-29 rulings (11.1's
      four), so it needs only a step spec that names it: the door as `ETHCallback_door_lift` #330-546 runs it (0.8 x h
      eased by smoothEnd, opened only from shut, closed only from fully raised), 46 lift doors on 37 levels. That step
      re-verifies the routes of 1-14, 1-15, 1-16, 1-32 and 1-21 (step 126: its shot threads `door_lift_831` as it
      closes) and 4-19's (the row "4-19's route rides the port's shot and lift door"), re-pins `test_mp_play`'s
      `kDoorRisePx` checks and `ALiftDoorLeavesItsDustAsItOpensAndCloses`, and adds 4-21's route (the candidate: a
      portal beyond the raised door, one on the player). Routes that pass through a wall broken by touch: 3-12 and 3-14
      (step 127).* *The lift-door clause is built in step 133 (LD): 102.4 u eased by smoothEnd, the status machine,
      and a closing door crushes the player (`ETHBeginContactCallback_door_lift`); 4-21 is won and the six routes are
      re-verified on their old ticks (section 6, B7). `kDoorRisePx` is gone and `test_mp_play`'s door checks are re-pinned
      on 102.4; `ALiftDoorLeavesItsDustAsItOpensAndCloses` is NOT re-pinned (its comment only; section 6's row). The
      other three clauses stay open.*
      *Step 164 (wave 4a): the routes of 1-19 (`level18`, won 271) and 1-20 (`level19`, 596) win BECAUSE the first clause is not built -
      the destroyable stone survives the wall it breaks (R4a-1); each is re-flown when it is built.*
      *Step 175 (WALL, wave 4c-2), 2026-10-15: THE FIRST CLAUSE IS BUILT. GAMEPLAY: YES. A stone whose `destroyable` is exactly 1 dies at the first wall it breaks (12
      placed stones on 10 levels: 1-13, 1-19, 1-20, 1-22, 3-10, 3-12, 3-13, 3-14, 3-18, 4-21; and every stone an `entity_launcher` throws, 1-24 and 1-25), and a stone
      breaks a wall only if its first touch is faster than 75 u/s (the original's 1.5 m/s, `ETHBeginContactCallback_breakable_wall` #16-33, #42-60). It moves eight routes
      (1-19, 1-20, 1-22, 2-29, 3-10, 3-12, 3-14, 4-21: no longer won) which the route phase (step 176) re-flies. The row stays open: its other clauses are not this step's
      (the notes on steps 133, 115 and 118 above say which are built), and 1-19's and 1-20's re-flight is step 176's. Step 175's recorded deviations WALL-1 to WALL-11 are
      below, after step 174's.*
      *Step 176 (the route phase, `45f6c05`), 2026-10-15: the first clause is built AND its eight moved routes are re-flown and win (1-19, 1-20, 1-22, 2-29, 3-10, 3-12, 3-14,
      4-21; 1-24 and 1-25 and the other five clause levels win on their census ticks). The row stays open for its other clauses (3-20's placed stone and its destroyier,
      and the beholder breaking any demolisher stone: the row's own list); the first clause is done.*
- [ ] **Order 11.1, recorded, and tidy** (step 109): the wall's angle is the converter's 1.5708 rad (-90.00021 deg, 0.0002
      u over the half line); static callbacks add their dust only while their bucket is drawn in the original
      (deviation 6, with steps 78-80's rows); 2-32's claw's `wall_destroy` at angle 90 has no owner in 00_order section 7;
      the wall watch (`m_wallEffects`) is one-way and its comment says a wall does not come back (harmless: 11.2 gave the
      light wall a watch of its own); `ALiftDoorLeavesItsDustAsItOpensAndCloses` names `button_1666` where the door that
      turns is `button_1656`'s; and step 118's `game/sim/RedKey.hpp` (new, committed with them): lines 41-43 have the
      wall's callback "that frame or the next" in a bucket of its own, lines 63-64 the bucket "256 / 2.8125" - its fix
      round has 128 and the same frame - and lines 5-6 call the wall "the level's only way to the rocket", unproven
      (the row "4-19's way under the key's room"); and `game/sim/Game.cpp` 488-489 still send a reader to
      `RedKey.hpp` for the wall "a frame later" (step 118's withdrawn deviation 3; its verifier found both). The
      comments wait for an edit that recompiles them anyway. *Step 133 fixed the `button_1666` / `button_1656` name
      in the dust test's comment; the rest stays.*
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
- [x] **Order 11.2, the zero-g recoil's speed** (step 110, deviation 7; found there, not its own): the port's 1.4 m/s
      recoil is 70 u/s (`zerog.json`; `kPixelsPerMetre` 50, Ethanon's `DEFAULT_SCALE`), where F10 measured one at 40-44
      u/s. *Closed in step 148: no conflict. The original's physics steps a fixed 1/30 s a frame, so scale(1.4) is 70 u per
      physics second and 70 x fps / 30 of wall time: F10's emulator ran 17-19 frames a second (39.7-44.3 u/s), a 60 Hz
      device gets 140. The port gives the shooter the decoded 1.4 at `SimRatio` (2): 140 u/s. The "m_absoluteSize 480"
      note of `zerog.json` is corrected (256: the recoil is the same in units on every screen). "No conflict" holds
      at the matched 60 Hz, which no clip shows: the rate itself is the owner's open call (G10-2, section 6).* *The route phase (step 151) re-flew
      chapter 4 at the 140 u/s recoil: all 19 of its routes win, the zero-g levels among them.* *Step 155 re-flew them on the time-base build (the capsule, the weights): all 19 still win, the weightless 4-xx routes' failed shots, where a shot's recoil is the move, each named in its `_about`.*
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
      a lighting (15). Next: their owners. *(4) is built in step 146: AddToAngle(190 deg/s) on the unlit torch's
      systems, its picture upright, measured against footage L1.*
      *(13) is built in step 142: `light_off`'s light on (#145), `fire_signal`'s light off (#275) and the light wall's
      switch 800 ms into its fade (#84-86).*
- [ ] **Order 13.1, the off-screen kill** (step 111; GAMEPLAY, older than it): `Fields::Tick` polls every ring every
      tick, where the original's `ETHCallback_shock_area` kills only while its bucket is drawn (D6_check 5.7). Next: the
      owner, with the bucket-visibility rows.
- [ ] **Order 13.1, not built, and recorded** (step 111, deviations 1-7): the agent's own pulse (`bounce(V2_ONE,
      vector2(0.95, 0.95), 300)` on the agent's sprite; its argument order not re-derived) and `playShockStrikeSound`;
      the ring turned and jittered from the level's first tick (the original's only while its bucket is drawn) and there
      from the first frame (the original's a frame later), on the port's 4.17 %-fast clock (the PORT-WIDE row), with
      `randF`'s inclusive 1.007 never drawn; paused frames now write the rings' two quads. None shows at 1 %. Next: a
      small step for the pulse. *The pulse is built in step 146 (1 -> 0.95 -> 1 on 300 ms legs, its picture only; on the
      level's age, which the pause holds: step 146's deviation 2).*
      *`playShockStrikeSound` is played since step 142, as a destroyed agent goes (#289-299) and as a ring kills the
      character (`ETHCallback_shock_area` #136-151).*

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
      *The AchievementsPopup is built from the decode in step 143, on the 59 levels an achievement names; still
      unmeasured, as no footage of it exists.*
- [ ] UI3 leftovers (spec §8.3): achievement unlock conditions not decoded (`AchievementManager`); locked-tile denial;
      credits drag/fling and dashboard bottom band not recorded; silver-medal states not captured; switch actions not decoded.
      *The unlock conditions are decoded and built in step 143: all 82, from `checkForLevelAchievements`,
      `registerMedalAchievements`, `ETHCallback_key` and `HelpBlockController`.*
- [ ] Achievement text/icons are Asantee content: only in gitignored `out/data/achievements.json`, produced by the committed
      `tools/asbc/achievements.py --listing <all_functions.txt>`; the port draws no rows without it.
      *Step 143 commits two of the original's on-screen strings in `game/data/achievements.json` ('Achievement
      unlocked! ', 'This level is locked!') and two suites: the owner's call (section 6, "Left by step 143").*

## 4. Gameplay feel (MILESTONE_1 §3 — still guesses)

- [x] Player walk speed, acceleration, gravity (player.json all `_guess`) *(decoded and built in step 148: `data/player.json`)* — measurable now from footage (rec11 walk).
- [ ] Camera follow lag / hold (portals.json camera `_guess`) — measurable from rec11 (the 11.25-12.0 s scroll).
      *The hold is decoded and built in step 138: 4000 ms after a drag away, behind `camera.json`'s switch, and no
      opening hold (`data/portals.json`'s guessed `hold_time_s` is gone). The lag is still the port's `_guess`
      (`follow_lag_s` 0.12, against the original's `Follower(1000, 100)`; step 138's deviations).*
- [ ] Portal timings `FIRST_PORTAL_MIN_TIME`, `NEXT_PORTAL_MIN_TIME`, collision radius, transit — decode or frame-step.

## 5. Final check

- [ ] 128-level sweep: capture the port at the library's `_h6.3` moment for every level, compare with
      `out/parity/original/levels/`, list residual differences per level, fix upstream (converter / port), repeat.
      *MEASURED 2026-10-11 (wave 4b; NOT ticked, and not ticked until its structural groups are fixed).* The port at f420 (the build `a5013ee6`,
      step 165's) against the library's `_h6.3` stills, and the `_h0.3`, `_h2.8` and t-series stills at their own ticks, read by 16 readers of
      8 levels each, then triaged by one rule: **26 `match`, 44 `minor`, 58 `differs`** of 128 (the readers' own tally was 26 / 47 / 55; five
      levels re-graded). `match` means no residual above low (phase, clock, tone), not pixel-equal; `minor` that the worst is medium;
      `differs` that one is high. 468 residuals (82 high, 125 medium, 261 low) fall in 18 causes, CMP-1 to CMP-18, and CMP-low (the rows
      after step 165's, before "Blockers"); 52 of the 58 `differs` levels differ by CMP-1 alone (the close's count from the per-level
      table). The pairing is measured only in part (h6.3 = f420 is the record's convention, early by 0.3 to 1.2 s on the script clock;
      calibrated on chapters 1-3 only), the seed is the all-gold save the library's stills were taken on (`09268640`, not the owner's
      `40bfef3a`), and the readers' audit covers 16 of 128 levels with 0 missed: see "Wave 4b" in section 6 and row CMP-R1 for the limits.
      Evidence: `out/parity/program/R4b/CMP/` (`pairing.md`, `per_level.md`, `residuals.md`, `groups.json`, `audit.json`; the pair and overlay
      images `pairs/<id>_h63_pair.png` and `_overlay.png`, and the port's frames `port_f420/`, were purged at wave 4d-1's close, 2026-10-15; the text files stay).
      *RE-TAKEN 2026-10-15 (wave 4d-1; NOT ticked, and not ticked until its structural groups are fixed: CMP-4, CMP-5, CMP-11, CMP-17 and CMP-18 are all unchanged).* The
      port at f420 on the fixed build (`fe37df6`, `MagicPortals.exe` `f27dde1a`, steps 166-176) against the same stills at the same pairings, read the same way (16
      readers of 8 levels, one rule): **61 `match`, 65 `minor`, 2 `differs`** of 128 (the readers' own tally 58 / 68 / 2) against 26 / 44 / 58. By the old verdict: of the
      58 that differed, 22 now `match`, 34 are `minor` and **2 still `differs` (1-30, CMP-4's pile; 3-05, CMP-5's knight A)**; of the 44 `minor`, 14 now `match` and 30
      stay; of the 26 `match`, 25 stay and 1 (1-15) reads `minor`, a re-grade of a cause no step touched, not a regression of the build. The 52 levels that differed by
      CMP-1 alone read 19 `match` and 33 `minor`: no level shows a lit player or arch any more. 490 residuals (2 high, 74 medium, 414 low; the old pass 82, 125, 261),
      every high and medium in a group: 8 groups fixed (CMP-1, -2, -3, -6, -7, -10, -15, -16), 1 partly (CMP-9), 9 open and unchanged plus CMP-low, 2 new (CMP-19 and
      CMP-20: new to the list, not new in the build). The audit found 15 of 16 levels agree and one miss (2-28's fireball halo), against 16 of 16 the first time, so the
      counts are the readers' and a lower bound on medium residuals (R4d1-4). The pairing is unchanged (h6.3 = f420 is still a convention, CMP-R1) and the seed is the
      all-gold save. Evidence: `out/parity/program/R4d/CMP/` (`pairing.md`, `old_vs_new.md`, `per_level.md`, `residuals.md`, `groups.json`, `audit.json`, `numbers.json`),
      the verifier `out/parity/program/R4d/verify1.json`, and the kept images `port_f420/` and `pairs/` there, named in `out/parity/BASELINE.txt`.

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
  suite, 23 with the window's. Step 116 adds 4-32, and its `test_mp_routes` run replays all 24 on the window's ticks:
  24 of 128 replayed by the suite. Step 117 adds 3-32: 25 of 128, all replayed by the suite. Step 118 adds 4-19, through
  the red key and its wall, replayed by the suite on tick 581 and in the window (its verifier's, `cbdb4004`): 26 of 128.
  Track C, on its own branch: step 125 adds 4-17, 4-18 and 4-28; step 126 adds 1-10, 1-11, 1-12, 1-21, 1-23 and 3-13;
  step 127 adds 36 (4-01, 1-07, 1-27, 1-28, 1-29, 2-15, 2-18, 2-23, 2-25, 2-26, 2-30, 2-31, 2-32, 3-02, 3-03, 3-07,
  3-09, 3-12, 3-14, 3-15, 3-19, 3-20, 3-22, 3-27, 3-29, 3-31, 4-02, 4-06, 4-07, 4-08, 4-11, 4-15, 4-25, 4-29, 4-30,
  4-31), re-routes 1-08, 1-11, 1-12 and 1-23, and removes 4-28's (refused under R1). Track B's steps 119-124 add none.
  **Step 128's merge: 70 of 128 won by a route `test_mp_routes` replays on the merged build** (215 checks, 0
  failures; main's 26 and track C's 44), after two route fixes: 2-09 one tap more (it kills
  `anti_portal_agent_ent_1095` first, R1 having refused the old last pair; won on the same tick 1319) and 3-31
  re-routed (G5 spent its first fireballs on a platform corner; won on 1046). Chapter 1: 20 won, chapter 2: 19,
  chapter 3: 15, chapter 4: 16. Three of the 70 are not yet ways the original is shown to play: 2-31 (two shots
  through a fire agent's flame, which the original snuffs: G8), 3-19 (an unmeasured hinge creep) and 3-31 (the port's
  self-running lift, G2); and every route leans on G4 and G10. **The 58 not won:** 1-06, 1-13, 1-17, 1-18, 1-19,
  1-20, 1-22, 1-24, 1-25, 1-26, 1-30, 1-31; 2-06, 2-10, 2-11, 2-16, 2-17, 2-19, 2-20, 2-21, 2-22, 2-24, 2-27, 2-28,
  2-29; 3-01, 3-04, 3-05, 3-06, 3-08, 3-10, 3-11, 3-16, 3-17, 3-18, 3-21, 3-23, 3-24, 3-25, 3-26, 3-28, 3-30; 4-03,
  4-04, 4-05, 4-09, 4-10, 4-12, 4-13, 4-14, 4-20, 4-21, 4-22, 4-23, 4-24, 4-26, 4-27, 4-28. Twelve carry a recorded
  cause: 1-17 (a pocket sealed to shots until its crates are cleared), 2-06 and 4-14 (G6), 2-10, 2-11, 2-16, 2-17
  and 2-19 (G8), 2-21 and 2-24 (B4), 4-21 (the lift door's latch, B7), 4-28 (no route found under R1; G8 or the
  minion burn, not proven unwinnable). No step has recorded an attempt at the other 46. **Step 133 (LD, the lift
  door) adds 4-21** (`level20c`, tick 587, by the door's latch; B7): **71 of 128** won by a route `test_mp_routes`
  replays (218 checks, 0 failures), chapter 4: 17. It re-authors 2-18 (1963 -> 1965) and 4-31 (997 -> 1167), and 2-07
  and 4-29 move to 1008 and 1501. The 57 not won lose 4-21, and eleven of them carry a recorded cause. Eleven of the
  57 hold a lift door, whose win the latch and the crush now shape with no route to show it either way: 1-13, 1-18,
  1-26, 1-31, 2-17, 2-24, 2-29, 3-05, 3-16, 4-22, 4-24. **Step 134 (G8 and the flame's burn of a minion) adds 2-10,
  2-11, 2-16, 2-17, 2-19 and 4-28** (`level9a` 393, `level10a` 314, `level15a` 260, `level16a` 459, `level18a` 342,
  `level27c` 861, all won by play), re-authors 2-23 (773 -> 758; its shot to the platform's right end is now burned)
  and, in its fix round, 2-31 (298 -> 248): step 127's kill shot passes 13.8 px from `fire_agent_963` and is burned,
  as the original does, so the agent is now shot from under its pocket, which the player reaches over `seesaw_915` in
  flight from a portal outside the ring. **77 of 128** won by a route `test_mp_routes` replays (77 files, 236 checks,
  0 failures), chapter 1: 20, chapter 2: 24, chapter 3: 15, chapter 4: 18. The 51 not won lose the six; five carry a
  recorded cause (1-17; 2-06 and 4-14, G6; 2-21 and 2-24, B4), and ten of the lift-door levels above remain (2-17 is
  won). **Step 135 (G6, a fireball through a portal pair) adds 2-06 and 4-14** (`level5a` 570, `level13c` 354, both
  won by play: a pair sends a carranca's fireball onto 2-06's crate and onto 4-14's shock agent) and re-authors 2-03
  (1458 -> 911): step 114's hops through pairs along the floor now carried the fireball onto the player, so the player
  holds the button behind a pair laid in the lane, which turns each fireball back, the way the original's help popup
  W2P3 draws it. **79 of 128** won by a route `test_mp_routes` replays (79 files, 242 checks, 0 failures), chapter 1:
  20, chapter 2: 25, chapter 3: 15, chapter 4: 19. The 49 not won lose 2-06 and 4-14; three carry a recorded cause
  (1-17; 2-21 and 2-24, B4). **Step 136 (HZ: a death area kills on its whole shape, and the platform gate) adds
  1-06** (`level5` 457, won by play: a pair onto `block00_ent_761`, down under `wall04_ent_762`, a pair onto
  `moving_platform_764`, and from it a shot through the slot `death_area_ent_800` fills to the door's block, then a
  portal on the player). **80 of 128** won by a route `test_mp_routes` replays (80 files, 245 checks, 0 failures),
  chapter 1: 21, chapter 2: 25, chapter 3: 15, chapter 4: 19. The other 79 win on their step-135 ticks; the platform
  gate held entries on one of them (1-28: 9 ticks, its small crate riding `lift_ent_694` through a portal, won on the
  same tick). The 48 not won lose 1-06; three carry a recorded cause (1-17; 2-21 and 2-24, B4).
      **Wave 2's tracks B and C and the merge (steps 137-144)**: step 137 (release-to-fire) moves every route's shot
      taps a tick earlier, each still firing on its old tick, and re-authors 2-09 and 2-32 on their old ticks; step
      139 (B4's handle) adds **2-20, 2-21, 2-22 and 2-24** (`level19a` 344, `level20a` 815, `level21a` 274, `level23a`
      722); step 140 (G2, the waiting lift) re-authors the nine lift routes and adds **3-08 and 3-10** (`level7b`
      1643, `level9b` 1085); the merge (step 144) moves track A's 14 routes into the release meaning, re-authors 2-06
      (575) and 2-18 (1191) and replays all: **86 of 128** won by a route `test_mp_routes` replays on the merged build
      (86 files, 1,513 checks, 0 failures, each route's sound and unlock census included), chapter 1: 21, chapter 2:
      29, chapter 3: 17, chapter 4: 19. Steps 145 and 146 replay all 86 on the same ticks. **The 42 not won:** 1-13,
      1-17, 1-18, 1-19, 1-20, 1-22, 1-24, 1-25, 1-26, 1-30, 1-31; 2-27, 2-28, 2-29; 3-01, 3-04, 3-05, 3-06, 3-11,
      3-16, 3-17, 3-18, 3-21, 3-23, 3-24, 3-25, 3-26, 3-28, 3-30; 4-03, 4-04, 4-05, 4-09, 4-10, 4-12, 4-13, 4-20,
      4-22, 4-23, 4-24, 4-26, 4-27. Three carry a recorded cause: 1-17 (the pocket sealed to shots), 2-27 (step 139's
      finding: only a shot its fixed flame turns into a fireball, aimed by the handle's reflector, reaches the door
      room's crates) and 3-23 (step 140's finding: both keys are carried by knights; its route waits on step 134's
      burn of a minion by a flame, with no recorded attempt yet) - the last two with their mechanisms on one build
      since the merge. Nine of the 42 hold a lift door: 1-13, 1-18, 1-26, 1-31, 2-29, 3-05, 3-16, 4-22, 4-24. The
      other 39 have no recorded attempt. Open until all 128.)* *Wave 3a, step 147 (CK, local, not pushed): 41 of the 86 win on its
      build and 45 do not (CK-2); the routes are re-authored by the wave's route phase after PIC.* *Step 148 (G10, local, not
      pushed): 23 of the 86 win on its build and 63 do not (the 45, and 18 that G10's movement moved); the routes are
      re-authored by the wave's route phase after PIC.* *Step 149 (G4): 8 of the 86 win and 78 do not; step 150 (PIC): the
      same 8. **Step 151 (the route phase, `af008a9`, local, not pushed) re-authors the routes on the final build: 84 of 128
      levels are won by a route `test_mp_routes` replays** (1,486 checks, 4 failures). The two routes that do not win are
      1-11 and 2-30, which won before wave 3a and are sealed on this build (R3-1 and R3-2 below). Chapter 1: 20, chapter 2:
      28, chapter 3: 17, chapter 4: 19. Of the 86 files, 76 were re-authored, 4 that already won (1-02, 2-20, 2-22, 4-06) had
      their `win_by` tightened, 4 win as committed (1-01, 1-15, 2-02, 2-07) and the two sealed ones are untouched. **The 44
      not won:** the 42 listed above, and 1-11 and 2-30. Open until all 128.* *Step 152 (WT, wave 3b-1, local, not pushed): 62 of
      the 86 win on its build and 24 do not (1-11 and 2-30 as before, and 22 that won at wave 3a; 56 win on the same tick, 6 on
      another); `test_mp_routes` is red by design inside the time-base group, and the routes are re-authored by the wave's route
      phase after PIC (step 150, built) and after the group's CAP and G3 - no route file was touched.* *Step 153 (CAP): 53 of the 86 win
      and 33 do not.* *Step 154 (G3, local, not pushed): 49 of the 86 win on its build and 37 do not (the 33 that did not win on CAP's build,
      unchanged, and 1-06, 2-26, 3-03 and 3-14, the four whose platform or patrolling agent G3 moved); every one of the 49 wins on the same
      tick as on CAP's build; the routes are re-authored by the wave's route phase after PIC (step 150, built), CAP and G3.*
      *Step 155 (the route phase of wave 3b-1, `7c843c7`, local, not pushed) re-authors the routes on the time-base group's final
      build: **86 of 128 levels are won by a route `test_mp_routes` replays** (86 files, 1,514 checks, 0 failures; chapter 1: 21,
      chapter 2: 29, chapter 3: 17, chapter 4: 19), **2-30 on a creep a port artifact makes (R3-2: its route is a stop-gap)**. 49
      won and 37 did not at the start of the phase; 1-11 (835) and 2-30 (1,386) are won again. **The 42 not won** are the 42 listed
      at step 144's tail, unchanged. Open until all 128.*
      *Wave 3b-2 (steps 156-161), pushed as `00de2b1..53d8db2`: step 156 (R23) leaves 80 of the 86 winning, 157 (G9) 74, 158 (G1a) 69, 159 (G1b) 61 and
      160 (STK) 61 (3-12 now dies under its stone), each with no route file touched. **Step 161 (the route phase, `53d8db2`) re-authors the 25
      that did not win: 86 of 128 levels are won again by a route `test_mp_routes` replays** (86 files, 1,598 checks, 0 failures; chapter 1: 21,
      chapter 2: 29, chapter 3: 17, chapter 4: 19; the independent verifier passed 11 of 11 gates). Of the 61 routes nobody edited 60 win on the
      same tick and 3-02 on 400 (was 388); of the 25 re-authored 21 win on another tick (16 later, 5 earlier) and 19 carry a new `win_by`.
      **The 42 not won** are the 42 listed at step 144's tail, unchanged. Open until all 128.*
      *Wave 4a (steps 162-164), pushed as `36a52e6..8fe8374`: steps 162 (STK9) and 163 (WAKE) leave the 86 winning on the same ticks but
      3-10, which step 163's wake takes (not won by 1,460). **Step 164 (the route phase, `8fe8374`) adds the 42 levels that had none and
      re-flies 3-10: 128 of 128 levels are won by a route `test_mp_routes` replays** (128 files, 2,366 checks, 0 failures; chapter 1: 32,
      chapter 2: 32, chapter 3: 32, chapter 4: 32; the independent verifier's second round passed 13 gate rows). **No level is without a
      route and no finding file was needed: this bullet is MET**, with the caveat that several routes stand on a port behaviour the
      original is not shown to have (R4a-1 to R4a-8), each stated in its own `_about`.*
      *Wave 4c-1: 128 of 128 routes win on exactly the census's ticks (83 to 3,343) after each of steps 166-170 and in all three verifications; no route file was touched.*
      *Wave 4c-2 (steps 171-176, pushed `7e708e3..45f6c05`, 2026-10-15): the five gameplay steps broke 13 routes and moved a 14th (`test_mp_routes` was red from step 171 to step
      175), and step 176 re-flew them: **128 of 128 levels are won again by a route `test_mp_routes` replays** (128 files, 2,365 checks, 0 failures; chapter 1: 32, chapter 2: 32,
      chapter 3: 32, chapter 4: 32; the independent verifier passed 9 gates, replayed all 128 twice with identical win lines). 117 win on exactly the census tick of
      `R4/census_after_round1.md` (114 untouched files and 3 rewritten on the same tick), 11 on another tick (8 earlier, 3 later), none late, **none unwon**. The 14 files:
      13 re-flown and 3-19 tightened (`win_by` only). **Not as before**: no route now unlocks achievement 48 (1-20: the new route needs 10 portals; R4c2-1), so the
      "achievements as before" clause of the route gate is NOT MET literally; the four stealth achievements (26, 28, 32, 42) are unchanged. Two of the re-flown routes (1-22 and 4-21) and the
      untouched 3-13 stand on something not measured on the original (R4c2-2 to R4c2-4). The census file future route gates compare against is
      `R4c2/census_after_round1.md`.*
- A static reachability check over the converted levels runs as a test and shows no door sealed by the port.
  *(Step 113: `tools/parity/reachability.py`, pinned in `reachability.json`, runs as ctest `parity_reachability`;
  it shows 108 reachable, 11 opened, 9 sealed - B1's nine. Step 115's `barrel` breaker turns the nine to opened:
  108 reachable, 20 opened, 0 sealed. The flood is optimistic about barrels (a barrel and any fire on the level opens
  a wall), so five of the nine read opened with no port route yet: G8's and B4's rows. Open until those are won.
  Step 118 adds the `red_key` breaker (4-19's wall); no verdict moves: 4-19 reads `reachable` by a way under the key's
  room that a point takes and the player would have to clear a crate for - the row "4-19's way under the key's room".
  Steps 125-127 (track C) and the merge (step 128): 108 reachable, 20 opened, 0 sealed, 0 differ from the pin,
  which no wave-1 step after 118 rewrote. That is no evidence for R1, R2 or R17: the flood models no no-portal
  zone, so a zone that grew or went cannot move a verdict (step 127, deviation 4; the Tests row "The reachability
  flood is optimistic"). The routes are their evidence. Step 133 (LD): 108 / 20 / 0, 0 differ. The flood treats every
  lift door as open (`FREE_ROLES` `switched_door`), so it sees neither the latch nor the crush; that step's evidence is
  its routes. Step 134 (G8): 108 / 20 / 0, 0 differ; the flood lights a level's barrels by any fire on it and models
  no portal shot at all (no sight line, no flame), so a burned shot cannot move a verdict; its routes are its evidence.
  Step 135 (G6): 108 / 20 / 0, 0 differ; the flood models no portal and no fireball's flight, so a fireball sent
  through a pair cannot move a verdict either; its routes are its evidence. Step 136 (HZ): 108 / 20 / 0, 0 differ;
  the flood frees every sensor, death areas included, and models no portal, so neither of that step's changes can move
  a verdict (with the two death areas made rock, in an experiment outside the tool, 1-06 and 2-26 still read
  reachable: a point passes the 6 u over 1-06's octagon); its routes are its evidence.
      Wave 2: 108 / 20 / 0 with 0 differ at every step that ran it, 129-131 and 133-146 (step 132 changed wording only
      and did not run it); tracks B and C ran the tool with main's venv on `out/snap_w2`. The flood models no touch,
      camera, handle, lift send, medal, sound or achievement, so none of wave 2's changes can move a verdict, and
      their routes are their evidence. Its `FREE_ROLES['lift']` text is stale since step 140 (the merge's verifier;
      the row "Step 144's recorded deviations").)*
      *Wave 3a: 108 / 20 / 0 with 0 differ at each of steps 147-150 and in the third verifier's run on the final build. The flood
      models no timing, no movement and no shot, so none of the timing group's changes can move a verdict, and it cannot see
      the two routes that do not win (1-11, 2-30); the routes are the evidence there.*
      *Wave 3b-1: 108 / 20 / 0 with 0 differ at steps 152-154, after both amendments of step 152 and in the third verifier's run on
      the final build. The flood models no weight, no capsule, no platform's swing and no timing, so none of the group's changes can
      move a verdict, and the routes (1-11 among them, now won) are the evidence.*
      *Wave 3b-2: 108 / 20 / 0 with 0 differ at steps 156-160 and in the route phase's verifier on the final build. The flood models no tap-time
      ray, no reflector, no knight's sight or shot and no stone's kill of the player, so none of the wave's changes can move a verdict; the routes
      are the evidence.*
      *Wave 4a: 108 / 20 / 0 with 0 differ at steps 162-164 and in both rounds of the route phase's verifier. The flood models no stone fall,
      no sleeper and no route, so neither mechanics step can move a verdict; **and it has no `destroyier.ent` term**: 1-24's door reads
      reachable, but the port's walk to it crosses `destroyier_ent_1051`'s 16 px kill box and the route reaches it only by a drop onto the
      trigger (R4a-4, STK-8), so 108 / 20 / 0 does not speak to that box. The routes are the evidence.*
      *Wave 4c-1: 108 / 20 / 0 with 0 differ at each of steps 166-170 and in the three verifications. The five steps change colours, a draw order, a light, a blink and
      a picture's alpha and scale: nothing the flood reads, so none can move a verdict. It still has no `destroyier.ent` term.*
      *Wave 4c-2: 108 / 20 / 0 with 0 differ at each of steps 171-175 and in the route phase's verifier. The flood reads no knight marker, no body's scale, no slab's
      velocity, no joint and no stone's speed, so none of the five steps can move a verdict; **it has no speed term and no one-wall-per-destroyable-stone term, so it is no
      evidence for step 175's rule (WALL-4): the 128 routes are**, and no level is sealed by it. It still has no `destroyier.ent` term.*
- Every blocker and major below is built as the original does it, or its ruling is recorded.
- The final 128-level comparison against the original's library (section 5) has run and its residuals are recorded,
  the largest fixed.
  *Wave 4b, 2026-10-11: the first half is done - the comparison has run and its residuals are recorded (CMP-1 to CMP-18, CMP-low and
  CMP-R1, rows after step 165's). "The largest fixed" is NOT met: the largest cause, CMP-1 (the player and the exit arch drawn
  lit on 78 levels), is decoded and unbuilt, and so are eight more groups; wave 4c takes them.*
  *Wave 4c-1, 2026-10-12 (pushed after the ruling R4c1-1): the comparison's five drawing groups are built and committed - CMP-1 (the player and the exit arch lit on
  78 levels, 52 of the 58 `differs` levels), CMP-3, CMP-7, CMP-9 and CMP-10 (steps 166-170) - so the largest cause is fixed; "the largest fixed" is still NOT met as a
  whole: the four groups that change gameplay (CMP-2, CMP-6, CMP-15, CMP-16) are decoded and unbuilt and wave 4c-2 takes them, and the five fixes have not yet been
  re-compared against the stills (the next measuring pass does, against `CMP/port_f420` and `CMP/pairs`).*
  *Wave 4c-2, 2026-10-15 (pushed): the four groups that change gameplay are built and committed - CMP-6 (step 171), CMP-16 (172), CMP-15 (173) and CMP-2 (174) - with the
  breakable wall's contact (175) and the route phase (176); the f420 sweep changes 11 levels (3-09, 4-29, 2-01, 4-02, 2-28, 2-29, 2-30 by one pixel, 2-31, 3-20, 4-11,
  4-31), each toward the stills where measured. "The largest fixed" is met for the nine decoded groups (CMP-1, -2, -3, -6, -7, -9, -10, -15, -16); it is NOT met as a whole:
  the comparison has not been re-taken on the fixed build, and CMP-4, -5, -8, -11, -12, -13, -14, -17, -18 and CMP-low stay open (the rig clips, the owner's calls and the
  untraced). This bullet stays open until the re-take.*
  *Wave 4d-1, 2026-10-15: the comparison is RE-TAKEN on the fixed build (`R4d/CMP/`): 61 `match`, 65 `minor`, 2 `differs` against 26 / 44 / 58, no regression caused by
  the build, eight decoded groups confirmed fixed on the stills (CMP-1, -2, -3, -6, -7, -10, -15, -16) and CMP-9 fixed in kind with low residuals left. The bullet STAYS
  OPEN: 1-30 (CMP-4) and 3-05 (CMP-5) still differ, CMP-8, -11, -12, -13, -14, -17, -18 and the two new groups CMP-19 and CMP-20 are open (the rig clips, the owner's
  calls, one decoded drawing group and the untraced), and the section-5 box is not ticked.*
- The owner's open calls are listed in one place.

**Wave 1 (2026-09-29 14:51 to 2026-09-30 06:25; closed 2026-09-30).** Three tracks at once: A on `main` (steps
113-118), B in the worktree `w1b` (written 150-155, renumbered 119-124) and C in `w1c` (170-172, renumbered 125-127),
merged in step 128 and pushed (`187b41b`). The engine pin stayed `4bfcf67`; no converter change, so `out/levels`,
`out/assets` and `out/data` are the ones the wave started from. "Deviations" are each step's own RECORDED DEVIATIONS.

| Step | Row | Track | Commit | Outcome | GAMEPLAY | Routes added |
|---|---|---|---|---|---|---|
| 113 | W.1 | A | `37c4ece` | built: `reachability.py` (ctest `parity_reachability`) and `test_mp_routes`; 3 deviations | none | 11: 1-01 to 1-05, 1-08, 1-09, 1-14, 1-15, 1-16, 1-32 |
| 114 | G5 | A | `cf52da3` | built, deviations recorded; a fix round won 2-09 | **yes**: a fireball dies on the first solid body it touches (every fireball level; 2-09's crate burns only while its button is held) | 8: 2-01 to 2-05, 2-07, 2-08, 2-09 |
| 115 | B1 | A | `0ab9609` | built; B2 a finding (not cleared) | **yes**: a barrel's blast breaks the walls it grabs (nine chapter-2 levels and 4-16; possibly 3-19, 4-10, 2-29) | 4: 2-12, 2-13, 2-14, 4-16 |
| 116 | B3 | A | `f72d4e3` | built, 4 deviations | **yes**, 4-32 only: the barrel restock and the lava | 1: 4-32 |
| 117 | G11 | A | `fe656cf` | built, 6 deviations | **yes**, 3-32 only: fire passes the ghost, a blast burns it, its barrel restock | 1: 3-32 |
| 118 | B5 | A | `e276018` | built, deviations 1, 2, 4-6 | **yes**, 4-19 only: the red key's fade and wall | 1: 4-19 |
| 119 | P1 | B | `e1dab6e` | built, no gate row unmet | none | - |
| 120 | P3, P4 | B | `105ca1c` | built, no gate row unmet | none (pictures) | - |
| 121 | P7 | B | `1656d8f` | built, no gate row unmet | none (music) | - |
| 122 | P5 | B | `691d6de` | built, no gate row unmet | none (scenery) | - |
| 123 | P2 | B | `e2277b8` | built; gate 9 NOT MET as written and explained (2-32's knight), gate 10's routes not run in the worktree | **yes, narrowly**: the door-to-medal time on 4-01, 4-19, 2-32 (5000 / 14500 / 6600 ms) | - |
| 124 | S1 | B | `cc8be2e` | built | none | - |
| 125 | R2 | C | `ce65260` | built with 4 deviations; 4-21 not won (the lift door) | **yes**: a well refuses no portal and stops no shot (5 levels) | 3: 4-17, 4-18, 4-28 (the last removed in 127) |
| 126 | R17 | C | `596862e` | built with 4 deviations | **yes**: a shot kills an anti-portal agent and its ring (17 levels, 21 agents) | 6: 1-10, 1-11, 1-12, 1-21, 1-23, 3-13 |
| 127 | R1 | C | `3e51904` | built with 10 deviations; 4-28 and 4-14 not won | **yes**: an antiportal refuses at 32 x scale (52 levels, 71 rings) | 36 (the list above); 4 re-routed; 4-28's removed |
| 128 | merge | - | `5e13f54`, `187b41b` | 70 of 70 routes won; 17 suites not run (Smart App Control) | none of its own | 2-09 one tap more; 3-31 re-routed |

**Routes**: 70 of 128 won by a route `test_mp_routes` replays on the merged build (26 before track C's, 44 from it).
**Reachability**: 108 reachable, 11 opened, 9 sealed (B1's nine) at step 113 and on tracks B and C, which lacked
step 115; 108 / 20 / 0 from step 115's barrel breaker to the merge, 0 differ from the pin. No step sealed a door the
flood can see, and the flood cannot see zones (the Tests row). **Smart App Control**: 81 refused launches recorded by
the steps and the merge (A 21, B 12, C 7, the merge 41), plus 2 at track C's setup - 83 notifications, assembled from
the records (step 122's verifier recorded no count).

**What the next wave should take**, in this order: (1) the lift door's latch (the "Order 11.1, GAMEPLAY rows for the
owner" row; B7): it wins 4-21 and re-verifies 1-14, 1-15, 1-16, 1-21, 1-32 and 4-19; (2) G8 and the fire agent's burn
of a minion: 2-10, 2-11, 2-16, 2-17, 2-19, 4-28, and 2-31 re-verified; (3) G6: 2-06, 4-14, and 2-03 re-authored;
(4) routes for the 46 levels no step has tried, chapter by chapter, which is most of what is left of the definition of
done; (5) G2, then G1 and G9, each re-verifying its route list above; G4 and G10 last, as they re-play every route; (6)
step 123's two MEDIUM rows (the medal saved at the door; 4-19's key sparkles) before the owner plays the three
cutscene levels; (7) the engine pin-bump step, on its own with a 128-level byte-identity sweep, carrying a
clamp-to-edge sampler for 2-32's claw lines (the Tests row) - it re-takes every baseline; (8) the 17 suites not run on
the merged code, each once, as the first step that links them. *(8) is done by step 129 (T.2): the suites are one
runner, `test_mp.exe`, so a relink makes one new executable, not 47. All 47 ran on the merged code with 0 failures,
once in one process and once under ctest (48 of 48, 0 Not Run), with 0 Smart App Control refusals.* *(7) is done in
part by step 130 (P0): `engine/` moved from `4bfcf67` to `a733ca0`, the engine's `main` (a fast-forward of 20 commits,
another game's opt-in work), and all 128 levels are byte-identical to `merge_w1/sweep` at f420, with every suite's
count and output, the 70 routes and reachability 108 / 20 / 0 unchanged, and 0 Smart App Control refusals. The clamp
it was to carry is not built: the engine now has one per texture (`a915514`), and the port opting in is a game change
that re-takes every baseline, left for its own step (the Tests row "2-32's claw draws a line").* *(7) is completed by step
131 (P1): the engine's clamp chosen per material (`710bffc`, reviewed by the other game's session before its push) and
the port's pictures and overlay quads opted in; 2-32's claw line, 2-01's ramp line and the sky's edge rows are gone.*
*(1) is done by step 133 (LD): the lift door as `ETHCallback_door_lift` runs it, and a closing door crushes the player;
4-21 is won (tick 587) and 1-14, 1-15, 1-16, 1-21, 1-32 and 4-19 are re-verified on their old ticks (B7).*
*(2) is done by step 134 (G8): a shot through a flame becomes a fireball and a flame burns a minion; 2-10, 2-11, 2-16,
2-17, 2-19 and 4-28 are won, and 2-23 and 2-31 re-authored (each had a shot the flame now burns).*
*(3) is done by step 135 (G6): a fireball goes through a placed pair once, flying back the way it came, and the pair
stays; 2-06 and 4-14 are won and 2-03 re-authored.*
*(6) is done by step 141 (MS): the medal is saved on the door's tick, and 4-19's key sparkles shrink with the key.*
*(5) is done for G2 by step 140 (the nine lift routes re-authored on the waiting lift, 3-08 and 3-10 won); G1, G9, G4
and G10 remain, in the Wave 2 paragraph's order.* *(4) is done in part: of the 46 levels no step had tried, wave 2 won
1-06, 2-20, 2-22, 3-08 and 3-10 and recorded findings on 2-27 and 3-23, each in the step that built its mechanism; 39
have no recorded attempt.*

**Wave 2 (2026-09-30 07:27 to 2026-10-01 18:56; closed 2026-10-01).** Two parts in one session, cut by a power loss
and finished by the next. **2a** on `main`, one step at a time (steps 129-132). **2b** on three tracks at once: A on
`main` (steps 133-136), B in the worktree `w2-b` (written 150-153, renumbered 137-140) and C in `w2-c` (170-172,
renumbered 141-143). The laptop lost power at about 03:50 on 2026-10-01, while step 143's first verifier had just
started; nothing committed was lost, and step 143 was found whole (`out/parity/program/resume_2026-10-01.md`). That
morning the owner renamed the checkout to `Magic-Portals-Enhanced`; the next session repaired the worktrees' links,
verified and committed step 143, merged both tracks in step 144 (pushed, `4d9ee56`) and ran EC and UI on `main` (steps
145-146). The engine pin moved in 2a only (`4bfcf67` -> `a733ca0` in step 130, -> `710bffc` in step 131); the
converter changed only in two docstring lines (step 132's wording, in `__init__.py` and `tiers.py`), so `out/levels`,
`out/assets` and `out/data` are the ones the wave started from (`out/snap_w2` by its manifest).

| Step | Row | Track | Commit | Outcome | GAMEPLAY | Routes added |
|---|---|---|---|---|---|---|
| 129 | T.2 | 2a | `e1826f3` | built: one runner, `test_mp.exe`; all 47 suites ran on wave 1's merged code (48 of 48 under ctest); 3 deviations | none | - |
| 130 | P0 | 2a | `8ce9668` | built: engine pin `4bfcf67` -> `a733ca0`, 128 of 128 levels byte-identical | none | - |
| 131 | P1 | 2a | `02e4d10` | built: clamp-to-edge (engine `710bffc`); three verifications failed it (the engine test refused, then gate 5 and the engine's rewritten `origin/main`), it was shelved and finished by hand (gate 5 decided as the original; the engine commit reviewed by the other game's session, the engine's ctest 57 of 57 under WSL); 8 deviations | none (every level's edge pixels move) | - |
| 132 | wording | 2a | `fd80e82` | renamed: "Remake" -> "Enhanced", 476 occurrences in 138 files; compile-only | none | - |
| 133 | LD | A | `5ee1c66` | built, 6 deviations | **yes**: the 46 lift doors on 37 levels (0.8 x h, eased, latched; a closing door crushes the player) | 1: 4-21 |
| 134 | G8 | A | `fdc100d` | built after a fix round (2-31 re-routed), 6 deviations | **yes**: a shot through a flame becomes a fireball (20 levels); a flame burns a minion (3-19, 3-23, 4-28) | 6: 2-10, 2-11, 2-16, 2-17, 2-19, 4-28 |
| 135 | G6 | A | `eb2143c` | built, 5 deviations; the carranca's speed a finding | **yes**: a fireball goes through a placed pair once (43 levels) | 2: 2-06, 4-14 |
| 136 | HZ | A | `d7e8322` | built, 5 deviations | **yes**: a death area kills on its polygon (1-06, 2-26); nothing goes through a placed pair while a platform or a lift is at a portal (29 bodies, 24 levels) | 1: 1-06 |
| 137 | RF | B | `7aee28b` | built, 4 deviations; every route's shot taps a tick earlier | **yes**: a shot leaves on the touch's release, from a touch that strayed no more than scale(18) (every level) | - |
| 138 | CD | B | `93eb2a1` | built, 7 deviations; 1-21's lesson | **yes**, behind `camera.json`'s switch: a drag pans at twice the swipe, holds 4000 ms, a held touch stops the follow (127 levels) | - |
| 139 | AR | B | `0e76e6d` | built, 7 deviations; 2-27 not won (a finding) | **yes**: the draggable handle and the flame or reflector it pulls (2-20 to 2-24, 2-27) | 4: 2-20, 2-21, 2-22, 2-24 |
| 140 | G2 | B | `056e1cd` | built, 7 deviations; nine lift routes re-authored; 3-23 not won (a finding) | **yes**: a lift waits on its marker and a touch sends it at 60 u/s (13 levels) | 2: 3-08, 3-10 |
| 141 | MS | C | `6462ba9` | built, 5 deviations; 4-19's key sparkles shrink with the key | **yes, narrowly**: the medal saved on the door's tick, from that tick's counts (every level) | - |
| 142 | P6 | C | `90df118` | built, 15 deviations; verified on its third round | none (sound) | - |
| 143 | P8 | C | `b45c740` | built, 8 deviations (one closed at the merge); verified after the power loss | none (achievements) | - |
| 144 | merge | - | `c2d7682`, `4d9ee56` | 86 of 86 routes won; 49 suites, 18,059 checks; the sweep attributed to the tracks | **yes**, its own: the flame a shot meets stands where its agent burns from (the handle levels) | 2-06, 2-18 re-authored |
| 145 | EC | main | `12eb200` | built, 2 deviations (2-05's right edge open) | none (41 dark-sky levels' edge columns) | - |
| 146 | UI | main | `aa374d4` | built, 6 deviations | none (pictures; 24 levels at f420) | - |

**Routes**: 86 of 128 won by a route `test_mp_routes` replays (1,513 checks, 0 failures) on the merged build and after
steps 145 and 146: chapter 1 21, chapter 2 29, chapter 3 17, chapter 4 19 - 16 more than wave 1's 70, each through the
step that built its mechanism; every route in the release meaning since step 137 (`tick` the down, `release` the up,
the shot on the up). **Reachability**: 108 reachable, 20 opened, 0 sealed, 0 differ at every step that ran it; no step
sealed a door the flood can see, and the flood sees none of this wave's mechanisms. **Suites**: one runner since step
129; 49 suites, 18,224 checks, 0 failures after step 146 (47 and 14,359 at step 129). **Smart App Control**: 19
refused launches, assembled from the records (2a: P1 2; 2b: LD 1, G6 1, CD 1, AR 1, P6 5, P8 1; after the power loss:
P8's verification 1, the merge 3, UI 3; every other role 0), against wave 1's 83.

**What the next wave should take**, in the program's order: (1) the timing group - G4 (the shot's 200 u/s and its
first-frame jump), G10 (the player's decoded movement), the script clock's floor and the pictures a tick behind
(section 2's PORT-WIDE rows) - with this wave's timing findings beside it (a carranca's fireball at its dir's length x
130, step 135; the zero-g recoil, on which 4-14's route drifts; the follow's lag, step 138); it re-plays all 86
routes, the thin ones first (2-11, 2-16, 2-17 and 2-23 within 3 px of a flame; 2-18's 1.2 px window on one release
tick; 2-14, 4-19 and 4-32 tick-exact); (2) G1, the knights - its fifteen routes, 3-08's and 3-10's, and the seven
stealth achievements; (3) G3 (R7), with 1-06, 1-11, 2-26 and the platform gate; (4) G9, with 2-27's reflector in mind;
(5) R23's rest; (6) routes for the 42 levels without one, 2-27 and 3-23 first (their mechanisms are on one build now);
(7) the final comparison (section 5), with the pixel-centre rule for every picture at the frame's edge (step 145's
deviation 1) before it, and the ship preparation: the grant in `docs/permission.md`, the play build, the owner's calls
below. *(1) is done by wave 3a (steps 147-151, below); (2) onwards are the next wave's.*

**Wave 3a (the timing group and its route phase; 2026-10-03 15:38 to 2026-10-04 17:00; closed 2026-10-04, committed locally,
NOT pushed).** Five steps on `main`, run alone because each moves every frame and every route tick: the script clock's floor
(CK), the player's decoded movement (G10), the portal shot's decoded flight (G4) and the picture of a body drawn at its tick's
own position (PIC), in that order, then the route phase (step 151), which re-authored the routes on the final build. The steps
committed locally; inside a timing step `test_mp_routes` was expected to fail on the routes the new timing moved, and each step
recorded a census instead of editing a route. Three independent verifier rounds checked the whole build after the route phase;
none passed (`R3/verify1.json` to `verify3.json`: the same two routes red, and 2-32's route out of the view), so **nothing is
pushed: `origin/main` is still `0b8d3c5` and `main` is nine commits ahead** (the wave's eight and the closing docs commit). The
engine pin stays `710bffc`; no converter change, so `out/levels`, `out/assets` and `out/data` are the ones the wave started from.

| Step | Row | Track | Commit | Outcome | GAMEPLAY | Routes added |
|---|---|---|---|---|---|---|
| 147 | CK | main | `2cd0675` (a docs follow-up, `9d3fd6b`); local, not pushed | built and committed with 11 recorded deviations (owner-delegated); the third verifier passed 12 of 13 gates, and the one it failed, the player's ground walk-cell flipbook, is cosmetic and G10's; 49 of 50 suites, 17,717 checks (`test_mp_routes` fails by design); reachability 108 / 20 / 0 | **yes**: every scripted motion, timer, blink and fade runs 16 ms a tick where it ran 16.667, 4.17 % slower in wall time (the 400 ms between taps takes 26 ticks, not 24; a 10 s crystal runs out on the 626th tick; the lost screen on the 88th) | none: 41 of the 86 routes still win on this build and 45 do not; re-authored in step 151 (84 of 86 win) |
| 148 | G10 | main | `d33d14d`, and the amendment of the R3 fix round (G10-5, the standing creep) `e54fa9d` and `972cdaf`; local, not pushed | built and committed with 13 recorded deviations (owner-delegated); the verifier passed all ten gates and listed six problems, three medium (the standing creep, a confirmed regression against the library; the unruled 60 Hz rate; the world's mixed time base) and three low, whose text findings the commit applied; 50 of 51 suites, 17,654 checks (`test_mp_routes` fails by design); reachability 108 / 20 / 0. The amendment (built in the R3 fix round, after the route phase's second verifier flagged that the routes had been re-authored on a build that still crept): 52 suites, 51 passed, 18,945 checks, `test_mp_player` 164 -> 274; 1-24 and 2-24 change in the sweep | **yes**: the player is the decode's - walk 144 u/s set outright, an air brake of 0.86 a tick and no steering, gravity 4000 u/s^2 under the original's fixed 1/30 s physics step, a 14-unit ground sensor, the zero-g recoil 140 u/s a shot, a turn toward a tap; and, in the amendment, an idle player on a slope stands still (it was pressed down into the slope and crept 0.28 units a tick on a 15 degree ramp) | none: **23 of the 86 routes still win** (CK: 41), 63 do not; re-authored in step 151 (84 of 86 win) |
| 149 | G4 | main | `012dd1a`; local, not pushed | built and committed with 9 recorded deviations (owner-delegated); the verifier passed all twelve gates and listed five low problems, all text only, which the commit applied (`game/data/shot.json`'s `_source` gained two comment facts after the executables were built: notes only, its keys unchanged, run by the verifier's full launch and sweep); 50 of 51 suites, 17,574 checks (`test_mp_routes` fails by design); reachability 108 / 20 / 0; the f420 sweep changes exactly the seven carranca levels (121 of 128 byte-identical), each explained by its fireballs' bursts and shakes | **yes**: every portal shot flies the original's 3.2 units a tick after a 24-unit jump (192 a second of wall time, about three times slower than before: 300 units 32 -> 87 ticks), opens its portal at its destiny when it has gone past it, and is seen by an anti-portal agent and a flame by its position once a frame; seven levels' fireballs fly at their carranca's |dir| x 130 (2-01 156, 2-03 162.5, 2-07 208, 2-15 169 and 260, 2-18 156, 2-28 156, 4-13 91) | none: **8 of the 86 routes still win** (G10: 23), 78 do not - 15 that won fail now, 14 with a second tap refused by a flight and 2-12 with a pair opened 27 ticks late; re-authored in step 151 (84 of 86 win) |
| 150 | PIC | main | `d17e7d3`; local, not pushed | built and committed with 9 recorded deviations (owner-delegated); the verifier passed all fourteen gates and listed six low problems (no medium), which the commit applied as far as they are text (the moved `test_mp_layer` line told in the step's Suites row, the stale comments counted as five sites, this row's own outcome, 4-22 recorded as inferred) and recorded as open rows where they are not (the aura pin that tests only the smoothed mode, `Pictures::Rules`'s two default numbers, the unlogged smoothing flip); no gate row unmet: the picture's lag 0.0000 ticks at every alpha (the control on a smoothed display 1 - alpha), 7 px on the real binary against one tick of the walk's 6.75; the f420 sweep 68 of 128 byte-identical and the 60 changed all explained by a picture or the camera on the view moving on the capture's tick; 51 of 52 suites, 17,730 checks (`test_mp_routes` fails by design; every earlier suite's counts unchanged, `test_mp_pictures` adds 156); reachability 108 / 20 / 0 | **none**: pictures only - a body's picture and the camera are drawn at the tick's own position (16.7 ms sooner), and the lights, effects, tutorial hands and shot marks that always read the sim's point now agree with them; in real time above 80 Hz the engine's smoothing is kept (`pictures.json`, 0 turns it off) | none: **8 of the 86 routes still win**, the same 8 and the same 78 as at step 149, route by route; re-authored in step 151 (84 of 86 win) |
| 151 | routes | main | `af008a9` (its Record line reads step 150; the commit adds step 151 and says so in its body); local, not pushed | the route phase: every route re-authored on the PIC build in three rounds (81, 83, then 84 of 86 won), a first fix round (four `win_by` tightened, 14 CRLF files, 2-32's first stretch) and a second (the standing creep, five routes re-flown); 84 of 86 win and the two sealed ones, 1-11 and 2-30, stay open with their causes (R3-1, R3-2); three verifiers, none passed: the third (`verify3.json`) passed 6 of its 8 gates and failed `test_mp_routes` (84 of 86) and route sanity (2-32's route is outside the view for 194 ticks, R3-3); 52 suites, 51 passed, 18,945 checks; reachability 108 / 20 / 0; the f420 sweep 128 of 128 byte-identical to the fix round's, and 126 of 128 to PIC's (1-24 and 2-24, the amendment's) | **none**: test data only (holds and taps) | **86 files: 76 re-authored, 4 tightened, 4 as committed, 2 untouched; 84 of 86 win** (23 earlier and 60 later than before the wave, median move 42 ticks; slack 13 to 62, median 37) |

**Routes** (step 151, `af008a9`): 84 of 128 levels are won by a route `test_mp_routes` replays on the final build (1,486 checks,
4 failures): chapter 1 20 of 21, chapter 2 28 of 29, chapter 3 17 of 17, chapter 4 19 of 19. The census went 86 of 86 (wave 2) ->
41 (CK) -> 23 (G10) -> 8 (G4) -> 8 (PIC), and the re-author's rounds took it 8 -> 81 -> 83 -> 84, with the creep fix of step 148's
amendment re-flying five routes on the way. Of the 86 files, 76 were re-authored on the final build, 4 that already won (1-02,
2-20, 2-22, 4-06) had their `win_by` tightened, 4 win as committed (1-01, 1-15, 2-02, 2-07) and 1-11 and 2-30 are untouched.
**Ticks moved**: against each route's own tick before the wave, 83 of the 84 winners win on another tick (23 earlier, 60 later,
median move 42 ticks; 2-04 on 534 is the one unmoved); the extremes are 3-19 3,207 -> 997, 2-26 827 -> 367, 3-10 1,085 -> 778
and 3-32 2,724 -> 3,358, 4-11 248 -> 529. Win ticks run 87 (4-02) to 3,358 (3-32) and every win is inside its `win_by`, with 13 to
62 ticks of slack (median 37); **the thin ones** are 4-02 (13), 4-08 (15), 3-08 (17) and 2-06 (20), and **the knife-edge ones**
3-20 (a second walk that must end on tick 238) and 3-22 (a walk of 428-430), which say so in their own notes. Wave 2's thin set
all win, each on another tick (2-11 on 354, 2-14 540, 2-16 396, 2-17 519, 2-18 983, 2-23 727, 4-19 662, 4-32 3,263); their
margins in pixels were not re-measured. **Failed, and why**: 1-11 (sealed at its first crossing: the spawn island lies inside
`antiportal_709`'s 192 px ring and no input reaches nearer than 28.15 px to the nearest legal portal against a reach of 24; it
needs G3 / R7) and 2-30 (burned by `fire_agent_ent_918` on tick 259: the air carry is capped at 14.74 u by the 0.86 brake and
the plank sags once the stone leaves its arm; it needs G10-1, G10-2, G10-4 and the owner). 4-31, called sealed in rounds 1 and
2, was won in round 3 (tick 1,262: the torch is a toggle, and a shot near its flame puts it out, which brings the burnt light wall
back for the climb). 2-32's route runs ahead of the dragon's camera for its last 194 ticks (R3-3).
**Suites**: 52 suites, 51 passed, 18,945 checks, against PIC's 17,730: `test_mp_routes` 381 -> 1,486 (a failing route stops
checking early, so the count grew with the wins), `test_mp_player` 164 -> 274 (the amendment), every other suite's count equal.
**Reachability**: 108 / 20 / 0, 0 differ at every step. **Smart App Control**: 2 refused launches in the wave, both the first
launch of a fresh `test_mp.exe` (CK's at 16:02:38 and G10's at 21:45:24 on 2026-10-03, each deleted and relinked once and then
run), against wave 1's 83 and wave 2's 19; every other role of the wave and this close: 0. **The standing f420 baseline** is
now `out/parity/program/R3/fix2/after/sweep` (`MagicPortals.exe` `f291bb42`, the binary `build/` holds; the third verifier's own
sweep matched it 128 of 128 and was deleted after hashing); `out/parity/BASELINE.txt` names it, and the images of every earlier
step were purged at the close.
**GAMEPLAY changed in wave 3a**, each as the original does and each in its planning step: CK (all scripted time runs 16 ms a tick
where it ran 16.667), G10 (the player's walk, air, fall, feet, recoil and turn; and, in the amendment, an idle player on a slope
stands still), G4 (every portal shot about three times slower, the portal opened at its destiny, seven levels' fireballs at their
carranca's |dir| x 130). None in PIC (pictures and the camera) or in the route phase (test data). R24 and R25 were not touched.

**What the next wave should take**, in the program's order: (1) G1, the knights - its fifteen routes, 3-08's and 3-10's, and the
seven stealth achievements; (2) G3 (R7), with 1-06, 1-11, 2-26 and the platform gate (it should also re-check 2-31, 3-03, 3-09
and 3-14, and the knife-edge 3-20 and 3-22); (3) G9, with 2-27's reflector in mind; (4) R23's rest and R22; (5) routes for the 44
levels not won - the 42 with no route, and 1-11 and 2-30, whose routes are sealed on this build; (6) the final comparison, with
the pixel-centre rule for every picture at the frame's edge (step 145's deviation 1), and the ship preparation: the grant in
`docs/permission.md`, the play build, the owner's calls below. **Before any of them**, the owner's rulings on G10-2 (the rate
the program matches), and with G10-1 (the world's time base) and G10-4 (the capsule) they gate 2-30 and re-time every route
again; and the decision whether wave 3a may be pushed with `test_mp_routes` red on 1-11 and 2-30 (R3-8).
*G3 (2) and the time base the rulings above were to gate (G10-1, G10-4) are done by wave 3b-1 (steps 152-155, below); G10-2 and the
push stand, and the order for what remains is at the end of the Wave 3b-1 paragraph.*

**Wave 3b-1 (the time-base group and its route phase: WT, CAP, G3, then step 155; 2026-10-04 17:28 to 2026-10-05 07:37; closed
2026-10-05, committed locally, NOT pushed).** Four steps on `main`, run alone on top of wave 3a because each re-times the routes
again: every dynamic body in the original's time base with each entity's own weight (WT, step 152), the player's real capsule, mass
and ground sensor (CAP, step 153) and the original's moving platforms and patrolling agents (G3, step 154), in that order, each
recording a census instead of editing a route; then the one route phase (step 155), which re-authored the routes on the final build.
Two amendments to step 152 (R3B FIX 1 and R3B FIX 2) came out of the independent verification. Three verifier rounds checked the
whole build after the route phase and **none passed** (`R3b/verify1.json` to `verify3.json`, `pass: false` each): the first on 3-13
(not won, high) and on 2-30 and 3-12 (won on loose-body motion nobody had established as the original's), the second and third on
2-30 alone, which is won on a creep the held friction makes (R3-2) and waits on the owner. **Nothing is pushed: `origin/main` is
still `0b8d3c5` and `main` is 18 commits ahead with this close** (wave 3a's nine, wave 3b-1's eight and this docs commit). The
engine pin stays `710bffc`; no converter change, so `out/levels`, `out/assets` and `out/data` are the ones the wave started from.
17:28 is the creation time of `out/parity/program/WT/`, the wave's first record, not a measured start.
*Pushed on 2026-10-05, with wave 3a, as `0b8d3c5..00de2b1` (the 18 commits above): the suite was green at 86 of 86 and the one failed verification
gate (2-30's creep, R3-2) was judged an owner question and not a defect. The "local, not pushed" in this paragraph and in wave 3a's is the state at
each wave's own close.*

| Step | Row | Track | Commit | Outcome | GAMEPLAY | Routes added |
|---|---|---|---|---|---|---|
| 152 | WT | main | `df5c542`; local, not pushed | built and committed with 9 recorded deviations (owner-delegated); the first verifier failed it (the decoded frictions walked 13 resting crates and barrels along their floors at 5.65 to 8.78 units a second and solved 2-08 by itself), the fix held the frictions at 1 (G10-14) and the second verifier passed all thirteen gates and listed seven problems, three medium and four low, whose text findings the commit applied and whose source findings are rows WT-2, WT-10, WT-11 and WT-12; 53 suites, 52 passed, 19,026 checks (`test_mp_routes` fails by design); reachability 108 / 20 / 0; the f420 sweep 46 of 128 byte-identical to the standing baseline, the 82 changed explained (the 33 levels with no loose body 33 of 33 identical; 2-29 and 3-12 only in part, WT-10); the falling crate 258.333 units in 30 ticks as the original's, a minion 1.0400 units a tick, 2-24's idle player 0.000 units in 420 ticks | **yes**: the world falls at the original's 2000 u/s^2 at the layer's tick (a crate 258 units in 30 ticks where it fell 127), a minion walks 62.4 u/s (was 39) and a 2-32 knight 81.6 (51), a well pulls every body, every body weighs its `.ent`'s weight (a stone or crate landing on the player embeds him, level30's two big crates cannot be pushed: WT-2, WT-3), no body has damping, a beholder's rock kills on the hardest approach of the last 30 ticks, Box2D's clamp holds every body; friction and restitution are NOT changed (held: G10-14, WT-1) | none: **62 of the 86 routes win** (84 before: 56 on the same tick, 6 on another), 24 do not (1-11 and 2-30 as before, and 22 that won); no route file touched, the routes are re-authored by the wave's route phase after PIC, CAP and G3 (done in step 155: 86 of 86 win) |
| 153 | CAP | main | `0e0c046`; local, not pushed | built, owner-delegated, 11 recorded deviations (CAP-1 to CAP-10 below, WT-10 to WT-12 not taken); **the spec's "a minion's body is not ground" gate is reversed by the listing and was not built** (a minion is not an `isCharacter`: only the player is; the feet refuse a sensor, the player's own body, `fireball.ent` and `anti_sight_wall*`); 53 suites, 52 passed, 19,030 checks (`test_mp_routes` fails by design); reachability 108 / 20 / 0; the f420 sweep 16 of 128 byte-identical to the WT build (14 weightless levels and the two popup levels), the 112 changed explained (98 by the player's picture 10.98 px lower, 11 by the camera following him, 4-16, 4-18 and 2-32 named); the player's picture against the library stills on 13 flat floors -9.19 -> +1.75 px, on 1-24's ramp -9.84 -> +1.56, on 2-24's barrel -6.48 -> +4.53 (+1.58 against the barrel, which is itself 2.95 px low) | **yes**: the player is the original's two circles as a capsule 18.2 wide and 36.2 tall (20 x 44 before), 3.9 units lower on whatever he stands on, his ground sensor 18 below the entity, triggers judge the real shape, every level opens with a 3.9-unit fall and a 0.3-unit landing tremble, and a fall is held to 100 screen pixels a step on the window it is played on (23.7 units at 1080); his weight, friction, walk and fall are unchanged | none: **53 of the 86 routes win** (62 before: 25 on the same tick, 20 later by 1 to 3, 8 earlier), 33 do not (1-11, 2-30 and 22 as before, and 9 that won: 1-12, 2-09, 2-11, 2-12, 2-17, 3-31, 4-07, 4-18, 4-25); no route file touched: the routes are re-authored by the wave's route phase after PIC, CAP and G3 (the time-base group's last step; PIC is wave 3a's, done), not by this step; done in step 155 (86 of 86 win) |
| 154 | G3 | main | `5cc4a09`; local, not pushed | built, owner-delegated, 8 recorded deviations (G3-1 to G3-8 below); **the spec's "carried by friction" premise is refuted by the decode and was not built** (a platform is a `b2_staticBody` moved by `SetTransform`: no velocity, vertical only; G10-5c(3) closes); 53 suites, 52 passed, 28,137 checks (`test_mp_routes` fails by design); reachability 108 / 20 / 0; the f420 sweep 120 of 128 byte-identical to CAP's, the 8 changed (1-06, 1-11, 2-26, 2-28, 2-31, 3-14, 3-28, 4-12) each explained by the formula at tick 420, 4-22's moved platform not visible in its identical frame; the 15 platforms and 3 agents within 0.0001 unit of `cos(angle) x stride` on every tick of 600; the library's stills fit 1-11's block and agent to 1.75 u rms on one phase (the old swing 30.8) | **yes**: every moving platform on 11 levels swings twice as far (the full stride either side, was half) at half the rate (`speed` rad/s, was twice) and starts a stride down the screen from its placed point (1-11's block y 114 to 254, was 149 to 219); the three patrolling anti-portal agents likewise (1-11's two, 3-14's); a shot meets an agent where its last frame put it; a rider is carried as before, by velocity both ways (the original pushes him up and drops him down: G3-1, G3-4) | none: **49 of the 86 routes win** (53 before: all 49 on the same tick), 37 do not (1-06, 2-26, 3-03 and 3-14 lost; the 33 that did not win on CAP's build unchanged); no route file touched: the routes are re-authored by the wave's route phase after PIC, CAP and G3 (the time-base group's last step; PIC is wave 3a's, done), not by this step; done in step 155 (86 of 86 win) |
| 152 (amend. 1) | R3b fix 1 | main | `95aee3f`, and a notes-only follow-up `cf37763`; local, not pushed | fixes the 3a + 3b-1 verification's round 1 (`R3b/verify1.json`: 3-13 not won, high; 2-30 and 3-12 won on solver artifacts, medium): `crate_low_emissive` held at 0.08 (a guess; the wedge lets go for every release from tick 180 at 0.079 to 0.082), 3-12's stone and crate asleep from tick 0 as the original's stills show them (`resting.json`) and a portal's teleport waking what lay on the traveller (Box2D's own rule), 3-12 and 3-13 re-authored, **2-30's creep traced to the held friction and left as it is** (R3-2); 53 suites, 53 passed, 28,723 checks; **86 of 86 routes win**; reachability 108 / 20 / 0; the f420 sweep 125 of 128 byte-identical to G3's, the 3 changed (3-03, 3-05, 3-12) explained | **yes**: the crates of 3-13, 3-03, 3-05 and 4-09 push at 1.89 u/tick, 3-12's pile does not move until touched, and a body a portal takes away wakes what lies on it | 3-12, 3-13 re-authored |
| 152 (amend. 2) | R3b fix 2 | main | `3994867`, and a notes-only follow-up `556bcec`; local, not pushed | the second verifier's round (`R3b/verify2.json`: 9 of 10 gates, one medium finding, 2-30, and six low): **2-30 NOT FIXED, it waits on the owner**; `held_friction` 0.08 gets its `"_guess": true`, 1-11's and 3-09's routes say they are authored on the port's tick-1 phase, 2-30's route says in capitals that it is a stop-gap on a port artifact, and the owner's three choices are measured on this build (the counterweight leap wins 2-30 on tick 284 at `air_decay` 1.0 and at `air_decay_fps` 45 and 30 and dies on 270 at the shipped 0.86; the stone's decoded friction fails 2-30 and 3-12 and moves 1-09 by +2 and 2-07 by -1); 53 suites, 53 passed, 28,723 checks; 86 of 86 routes win | **none** (one data note, route notes) | none |
| 155 | routes | main | `7c843c7` ("Magic Portals: routes re-authored on the time-base group's build"; its Record line reads step 155, the step it adds, where the brief's N was 154); local, not pushed | the route phase: every route re-authored on the final build in four rounds (83, then 85 of 86 won; R3B FIX 1's 3-12 and 3-13 made 86), 37 route files in the commit (36 holds or taps changed, 3-09's a note); **86 of 86 win, 2-30 on a creep**; three whole-build verifiers, **none passed** (`verify3.json`: 9 of 10 gates ok, the failed one the spot replay on 2-30); 53 suites, 53 passed, 28,723 checks, `test_mp_routes` 1,514; reachability 108 / 20 / 0; the f420 sweep 128 of 128 byte-identical to R3B FIX 1's and 125 of 128 to G3's (3-03, 3-05 and 3-12, which the amendment explains) | **none**: test data only (holds and taps) | **86 files: 35 of the 37 that lost on G3's build re-authored, 2-32 re-flown to stay in view, 3-09 a note, 3-12 and 3-13 earlier (`95aee3f`); 86 of 86 win** (of wave 3a's 84 winners 56 win on another tick - 16 earlier, 40 later, median move 3 - and 28 on the same; slack 9 to 70, median 31); 1-11 (835) and 2-30 (1,386) are won now |

**Routes** (step 155, `7c843c7`): 86 of 128 levels are won by a route `test_mp_routes` replays on the final build (86 files, 1,514
checks, 0 failures; chapter 1 21 of 21, chapter 2 29 of 29, chapter 3 17 of 17, chapter 4 19 of 19), **2-30 on a creep a port
artifact makes** (R3-2: its route is a stop-gap and says so; the designed counterweight leap dies at the shipped 0.86 air brake on
tick 270 and wins on tick 284 at an air brake of 1.0 or at 45 or 30 frames a second). The census went 84 of 86 (wave 3a) -> 62 (WT)
-> 53 (CAP) -> 49 (G3) with no route file touched, then 83 after the phase's first re-author round, 85 after the fourth and 86 after
R3B FIX 1's 3-13 and 3-12. **At the start of the phase 37 did not win**: 35 lost to the group, and 1-11 and 2-30 stayed red from
wave 3a; their first problems were 15 not won by `win_by`, 9 deaths (4 falls, 2 burns, 2 shocks, 1 enemy shot), 6 taps refused for a
shot still in flight, 6 taps off the view and 1 tap that fired nothing. **Ticks moved**, against wave 3a's 84 winners: **56 win on
another tick (16 earlier, 40 later, median move 3 ticks) and 28 on the same**; the largest are 2-32 2,718 -> 3,205 (+487, re-flown
to stay in view), 3-22 1,470 -> 1,181, 1-06 523 -> 808, 1-15 527 -> 762 and 4-30 1,650 -> 1,458; 29 files carry a new `win_by`; and
**24 of the 47 files nobody edited moved on their own** (median 2 ticks, at most 17: 4-21 387 -> 370, 1-09 388 -> 404), so a route
that needed no edit is still re-timed by the time base. Win ticks run 87 (4-02) to 3,376 (3-32) and every win is inside its
`win_by`, with 9 to 70 ticks of slack (median 31; 43 routes above 30 and 7 above 50). **The thin ones** are 4-18 (9), 3-12 (12),
3-13 (13), 4-02 (13) and 4-08 (15); **the knife-edges** are 3-20 (a second walk that must end on tick 238: 237 and 239 do not win),
3-22 (a drop good for one tick; 7 of its 56 +-3 variants die) and 3-13 (its crate's held friction wins only at 0.079 to 0.082, a
guess; its input windows are at least 4 ticks a side). **Failed, and why**: none lost at the end, but **2-30's win is not the
level's** (R3-2); 1-11, sealed on wave 3a's build, is won because G3's platforms swing as the original's do (its route boards the
moving platform, kills three anti-portal agents and takes the door), and its route and 3-03's and 3-09's rest on the port's tick-1
phase for their platforms (G3-2; 3-06 has no route). The first verifier found 3-13 not won (a high finding: the held friction of 1
wedged its crate) and 2-30 and 3-12 won on artifacts; R3B FIX 1 held one crate at a guessed 0.08, put 3-12's pile to sleep as the
original's stills show it and had a portal's teleport wake what lay on the traveller.

**Suites**: 53 suites, 53 passed, 28,723 checks at the end (19,026 after WT, 19,030 after CAP, 28,137 after G3 (`test_mp_movers`
alone is 9,478 of them, 9,000 a per-tick velocity check) and 28,723 after R3B FIX 1: `test_mp_timebase` 381 -> 437, `test_mp_routes`
1,500 -> 1,514); `test_mp_routes` was red inside WT, CAP and G3 by design and is green now. **Reachability**: 108 / 20 / 0, 0 differ
at every step and in the third verifier's run. **Verification**: WT's first verifier failed it (8 of 12 gates: the decoded frictions
walked 13 resting bodies along their floors) and its second passed all 13; CAP's (11 of 11) and G3's (10 of 10) passed with six
low-to-medium problems each, whose text findings their commits applied; the three whole-build rounds after the route phase did not
pass. **Smart App Control**: 4 refused launches in the wave, from a CodeIntegrity 3033/3077 query this close ran from 2026-10-04
17:00: WT's first `MagicPortals.exe` (18:32:07), CAP's `test_mp.exe` (22:21:20), G3's `MagicPortals.exe` (00:15:32) and R3B FIX 1's
first `MagicPortals.exe` (05:21:13 on 10-05), each deleted and relinked once and then run; none from a verifier, the route phase, a
commit pass or this close (wave 1: 83, wave 2: 19, wave 3a: 2). Zero warnings rests on the fix builds' logs
(`WT/fix1/runs/build_fix*.log`, `CAP/commit_compile_objs.log`, `R3b/fix1/build1-7.log`): the close built nothing, and R3b-8 lists
what the last verifier did not re-measure.

**The standing f420 baseline** is now `out/parity/program/R3b/fix1/after/sweep` (128 PNGs, taken by R3B FIX 1 on `MagicPortals.exe`
`814f08b5`, the committed build's sources before a comment-only edit): the wave did not pass, so it is the last sweep that matches
the committed build - the third verifier's own sweep (`098e51b4`, HEAD `556bcec`, 128 launches, every rc 0) was byte-identical to it
128 of 128 before that verifier deleted its PNGs, and the close re-checked all 128 md5s against that list and against the capture log, and the PNGs'
signatures and 1280 x 720 size, before the purge, and the md5s again after it. `out/parity/BASELINE.txt` names it, replacing the wave 3a baseline
`R3/fix2/after/sweep`; the f420 sweeps of WT, CAP and G3 and 21 cited crops were purged at the close (533 images, 0.39 GB).

**GAMEPLAY changed in wave 3b-1**, each as the original does and each in its planning step: WT (the world falls at the original's
2000 u/s^2 at the layer's tick where it fell at 980; a minion walks 62.4 u/s, was 39, and a 2-32 knight 81.6, was 51; a gravity well
pulls every body; every body weighs its `.ent`'s weight; no body has damping; a beholder's rock kills on the hardest approach of the
last 30 ticks; Box2D's clamp holds every body; friction and restitution are decoded and HELD, not applied), CAP (the player is the
original's two circles as a capsule 18.2 x 36.2, 3.9 units lower on whatever he stands on, his ground sensor 18 below the entity,
triggers judge the real shape, every level opens with a 3.9-unit fall, and a fall is held to 100 screen pixels a step on the window
it is played on), G3 (every moving platform on 11 levels and the three patrolling agents swing twice as far at half the rate, from a
stride down the screen) and R3B FIX 1 (the one crate `crate_low_emissive` of 3-13, 3-03, 3-05 and 4-09 is pushed at 1.89 u/tick and
slides a few units on release; 3-12's pile does not move until touched; on every level a body a portal takes away wakes what lay on
it). None in the route phase (test data) or in R3B FIX 2 (notes). R24 and R25 were not touched and G10-2 was not reopened
(`clock.json` `matched_hz` stays 60).

**What the next wave should take**, in the program's order: (1) G1, the knights' sight, shooting and portal-killing, with its routes
(3-02, 3-03, 3-07, 3-08, 3-09, 3-10, 3-12, 3-15, 3-19, 3-20, 3-22, 3-27, 3-29, 3-31, 4-25, 4-29, 4-30) re-verified - 3-22's drop and
3-20's second walk assume a knight that never sees - and the seven stealth achievements (24, 26, 28, 32, 42, 66, 76); (2) G9, with
2-27's reflector in mind; (3) R23's rest and R22; (4) routes for the levels not won: the 42 with no route (1-13, 1-17, 1-18, 1-19,
1-20, 1-22, 1-24, 1-25, 1-26, 1-30, 1-31, 2-27, 2-28, 2-29, 3-01, 3-04, 3-05, 3-06, 3-11, 3-16, 3-17, 3-18, 3-21, 3-23, 3-24, 3-25,
3-26, 3-28, 3-30, 4-03, 4-04, 4-05, 4-09, 4-10, 4-12, 4-13, 4-20, 4-22, 4-23, 4-24, 4-26, 4-27), and 2-30, whose route stands on a
creep until the owner settles R3-2 (1-11 is won); (5) the final comparison, with the pixel-centre rule for every picture at the
frame's edge (step 145's deviation 1), and the ship preparation: the grant in `docs/permission.md`, the play build, the owner's
calls below. **Before any of them**, the owner's answers: whether waves 3a and 3b-1 (17 local commits, 18 with this close) are
pushed with 86 of 86 routes won but the one failed verification gate, or 2-30 is settled first - by G10-2 lower, the stone's decoded
friction, or the creep accepted until the engine's friction step (R3-2, R3-8) - and whether the engine's friction and restitution
steps (G10-14, WT-1) are queued with the Penumbra port's engine work. **With the wave**: R3-3 (2-32 ahead of the dragon's camera)
and R3b-6 (`win_by` tightened, 3-20 widened) once G1 has moved the routes it moves.

**Wave 3b-2 (the portal tap, the reflector, the knights and the stone: R23, G9, G1a, G1b, STK, then the route phase; 2026-10-05 08:11 to
2026-10-11 01:50; closed 2026-10-11, PUSHED).** Six steps, each a local commit while it ran, pushed together once the routes were re-authored and an
independent verifier had checked the whole build: `origin/main` is `53d8db2` (`00de2b1..53d8db2`, a fast-forward, 75 files, +6,134 / -1,099).
The wave ran in two parts: R23 and a first try at G9 on 2026-10-05 (stopped for the owner's usage at a clean point, the unverified G9 tree kept on
`wip/wave3b2-g9-interrupted`), and G9 again, G1a, G1b, STK and the route phase on 2026-10-10 and 11 (the run's brief cites the owner's standing order of 2026-10-10 to continue until
finished or told to stop). The step paragraphs below were written at each step's commit and say "local, NOT pushed"; that was true then. The
closing paragraphs after step 161 are the wave's close. **Step 156 (R23,
the portal tap as the original handles it) is built, verified and committed locally**: the independent verifier passed all thirteen
gates (`out/parity/program/R23/verify1.json`; five low problems, whose text findings the commit applied); 53 suites, 52 passed,
29,723 checks, `test_mp_routes` red on six routes by design (1-23, 2-10, 2-17, 2-19, 3-13 and 3-15; 80 of 86 win, each on step 155's
tick) - **the routes are re-authored by the wave's route phase, which follows G1b (PIC's pictures are already in this build)**, no route
file was touched; reachability 108 / 20 / 0; the f420 sweep 128 of 128 byte-identical to the standing baseline; zero warnings; 2 Smart
App Control refusals (the runner's two first launches, both relinked). **GAMEPLAY changed, plainly, on every tap**: the shot flies to
the tap-time ray's `finalPos` and opens a portal there or fails there as the ray said (a crate, a stone or a minion takes the portal 10
units in; a button, a `destroyier.ent` and the anchor doom the shot at their face; the head-height retry is the shot's own; a reflected
leg lands 10 short of a wall); a blocker reads the shot's position; a portal can open inside a no-portal field when the ray's end is 10
units inside a body whose contact lay outside it; only the tap that reaches the hourglass restarts the 400 ms (the program's spec said
every refusal; the listing says not); the zero-g recoil is away from `finalPos`; `no_portal_705` is no zone. The recorded deviations are
the rows "Step 156's recorded deviations" (R23-1 to R23-11), under G4's. R24 and R25 were not touched.
**Step 157 (G9, the reflector as the original has it) is built, measured, independently verified (13 gates passed, three low findings, text applied) and committed locally, NOT pushed (re-done from an interrupted first attempt, `wip/wave3b2-g9-interrupted`)**: a shot is caught by a position
sample STRICTLY within 32 units of a reflector's point (the sprite's `GetSize().x`; the port's 16 was a guess, swept along the way), mirrored on the plane's axis
- or, for a placement with no plane (2-27's), on the side of the reflector it came from - and sent on from the REFLECTOR'S point for `min(the length it was fired, the way from where its
leg began to the reflector)`, by one reflector at most; no clip shows a reflected shot, so it rests on the listing. **GAMEPLAY changed, plainly, on the 19 levels with a reflector**
(1-12, 1-13, 1-20, 1-21, 1-23, 1-29, 2-10, 2-11, 2-16, 2-18, 2-25, 2-27, 3-03, 3-05, 3-14, 3-25, 3-26, 4-10, 4-23): a shot passing 16 to 32 units from one is now turned, and a turned shot's leg is
the whole length it was fired or the way it came (much longer where the tap was at or short of the reflector, shorter where it was far beyond it) from the reflector's own point, so its
portal opens elsewhere and a longer leg keeps it in the air (a second tap meets the hourglass). `test_mp_routes` is red on 12 routes (R23's six
and six more: 1-12, 1-21, 2-11, 2-16, 2-25, 3-14; 74 of 86 win, every one on its step-155 tick) - the wave's route phase re-authors them; reachability 108 / 20 / 0; the f420 sweep is in
planning step 157 (128 of 128 byte-identical to R23's). The rows are "Step 157's recorded deviations" (G9-1 to G9-7), under R23's. The Build Tools' MSVC moved from 14.50 to 14.51 on 2026-10-05 (the old toolset is gone), so `build/` was
deleted and reconfigured by `port_build.bat`; the three-level check against the R23 sweep (1-01, 1-12, 2-25) was byte-identical before the step's first edit.
**Step 158 (G1a, the knights see) is built, measured, independently verified (13 gates passed, six low findings, the text ones applied) and committed locally, NOT pushed**: the perception half of G1 (above), 35 levels' worth of GAMEPLAY change, said plainly in planning step 158: a knight that sees stands (its
dwell clock zeroed on each sighting and each shot gate, so a hold of 2000 ms or more is never served out while the sighting lasts), shows its `!` and plays its sound, searches when its way is blocked (a knight lifted off its floor by a portal no
longer walks on toward its waypoint's x), and is blind to the player on the dark levels until a torch is lit; `anti_sight_wall.ent` blocks sight. `test_mp_routes` is red on 17 routes (the twelve of step 157 with the same first problems and five new -
3-07, 3-22, 3-32, 4-28, 4-29: routes written for knights that never see or never stop; 3-02 wins 13 ticks later, on 401; 69 of 86 win) - the wave's route phase re-authors them after G1b; reachability 108 / 20 / 0; the f420 sweep is 125 of 128
byte-identical to step 157's (3-16, 3-23 and 3-29 differ: a knight that sees the player at tick 372 to 398 stands, and 3-29's `!` is on screen, within about 6 px of the original's still); 54 suites (the new `test_mp_sight`, 383 checks), 30,125
checks, zero warnings (read from the build logs by the verifier, not rebuilt); one Smart App Control refusal (`MagicPortals.exe`'s first launch, relinked once). The rows are "Step 158's recorded deviations" (G1a-1 to G1a-11), under G9's.
**Step 159 (G1b, the knights shoot) is built, measured, independently verified (11 gates passed, one medium and five low findings, the text ones applied) and committed locally, NOT pushed (the wave's route phase, its verifier and the push follow)**: the action half of G1 (above), the GAMEPLAY change said plainly in planning step 159 - knights fire an
`enemy_shoot.ent` that kills the player or a portal by its stored id (the player in the line of a shot at a portal is not hurt), every shot ends in a burst and a shake of 10, the player's touch caps a knight's hold at 1000 ms, a barrel's blast and a stone's crush kill a knight, and the
seven stealth achievements read the real `SpotCount()`. `test_mp_routes` is red on 25 routes (61 of 86 win: G1a's 17 and eight new; 3-02 wins on 400) - **the routes are re-authored by the wave's route phase, after G1b**; the stone's kill of the player (G1b-5) is open and is to be built or ruled out before it; reachability 108 / 20 / 0; the f420 sweep is 127 of 128 byte-identical to step 158's (3-16 differs: its first shot is in the air at tick 420);
55 suites, 54 passed (the new `test_mp_shoot`, 100 checks), 30,225 checks, zero warnings (read from the build logs by the verifier, not rebuilt), no Smart App Control refusal. The rows are "Step 159's recorded deviations" (G1b-1 to G1b-10), under G1a's.
**Step 160 (STK, a rolling stone kills the player it hits fast enough) is built, measured, independently verified three times (round 1: 12 of 15 gates passed, three not run behind Smart App Control, one high, one medium and four low findings, fixed: the unrun suites now run and pass; round 2: 13 of 14; round 3: all 12 gates, one medium and five low, whose text findings the commit applied) and committed locally, not pushed**: the verifier's one MEDIUM of step 159 (G1b-5), built before the route phase so the routes are re-authored against the final rules. `ETHBeginContactCallback_rolling_stone` #107-129 sets the player's hp to 0 where the stone's own speed along the line to his point, over 50 px a unit, is above 2.5 (125 level units a second): the layer's impact watch reads the begin as it reads the stone's shake and the knights' crush, `Game::StoneHitsPlayer` asks `demolish.json` `stone.kills_player_above`, and `AfterStep` folds the kill into Hazards as a death area's death; the beholder's rocks stay Boss's. **GAMEPLAY changed, plainly, on the 26 levels with a stone that has the callback** (planning step 160). `test_mp_routes` is red on the same 25 routes (61 of 86 win, every one on its step-159 tick; **3-12 now dies on tick 200 under its stone**, where it did not win before) - **the routes are re-authored by the wave's route phase, after G1b and this step, on this build; no route file was touched**; reachability 108 / 20 / 0; the f420 sweep is 128 of 128 byte-identical to step 159's (no stone begins on the player by tick 420 on any level); 55 suites, of which **54 pass on the fix's whole run** (`test_mp.exe` `1aee46d3`: 30,329 checks; `test_mp_demolish` 178, `test_mp_layer` 3,706, only `test_mp_routes` red; the first run's three failures in the step's own new cases were corrected and the G1b-10 test passes), zero warnings. The rows are "Step 160's recorded deviations" (STK-1 to STK-9), under G1b's; **STK-9, the verifier's one medium of round 3 (the watch reads a free fall a step or two low), is open and is not text**.
**Step 161 (the route phase of wave 3b-2, `53d8db2`, "Magic Portals: routes re-authored on the portal-tap and knights build") re-authors the routes on the wave's final build: 86 of 86 win** (`test_mp_routes` 1,598 checks, 0 failures; at its start 61 won and 25 did not, 1,213 checks and 50 failures: `R3c/census_start.md`). Seven roles re-authored the 25 in one round, each on its own route files and the replay `test_mp.exe test_mp_routes --only <level>`; the commit holds 25 files of `tests/routes/` (+533 / -443) and the planning step, and no source, test, data, converter or engine file. The independent verifier (`out/parity/program/R3c/verify1.json`) passed all 11 gates on its first round, with one medium (STK-9, open, not a route problem) and five low findings. **GAMEPLAY: none** (routes are test data). The finalize role then pushed the wave (`git push origin main` printed `00de2b1..53d8db2`; after a fresh fetch `git rev-parse HEAD origin/main` agree). The rows are "The route phase's recorded deviations of wave 3b-2" (R3c-1 to R3c-13), after R3b-8.

| Step | Row | Commit | Outcome | GAMEPLAY | Routes win |
|---|---|---|---|---|---|
| 156 | R23 | `8e81ccf` | built; independent verifier 13 of 13 gates, 5 low; 53 suites, 52 passed, 29,723 checks (`test_mp_routes` red by design); reachability 108 / 20 / 0; f420 sweep 128 of 128 byte-identical to the standing baseline; 2 Smart App Control refusals (the runner's) | **yes**, on every tap: the shot flies to the tap-time ray's end and opens its portal there or fails there (a crate, a stone or a knight takes it 10 units in; a button, a `destroyier.ent` and the anchor doom it at their face; the head-height retry is the shot's own; a reflected leg lands 10 short of a wall); only the tap that reaches the hourglass restarts the 400 ms; the zero-g recoil is away from the ray's end; `no_portal_705` is no zone | 80 of 86 (1-23, 2-10, 2-17, 2-19, 3-13, 3-15 red) |
| 157 | G9 | `1faf09b` | built, re-done from an interrupted first attempt (`wip/wave3b2-g9-interrupted`); verifier 13 of 13, 3 low; 53 suites, 52 passed, 29,788; 108 / 20 / 0; sweep 128 of 128 identical to R23's; 2 refusals (the runner's and `MagicPortals.exe`'s) | **yes**, on the 19 levels with a reflector: it catches a shot strictly within 32 units (the port's 16 was a guess), mirrors it on its plane's axis (a plane-less one on the side it came from) and sends it on from its own point for `min(the length it was fired, the way its leg came)`, by one reflector at most | 74 of 86 (+ 1-12, 1-21, 2-11, 2-16, 2-25, 3-14 red) |
| 158 | G1a | `d4a094e` | built; verifier 13 of 13, 6 low; 54 suites, 53 passed, 30,125 (the new `test_mp_sight`, 383 checks); 108 / 20 / 0; sweep 125 of 128 identical to G9's (3-16, 3-23, 3-29 differ, each explained); 1 refusal | **yes**, on the 35 levels with a knight: a knight sees (a 120 degree cone through three 128-unit buckets of its own row, past every body but the knights' and the player's), stands while it does, shows `!` with its sound, searches (`?`) when its way is blocked, and is blind on the six darkest levels until a torch is lit; `anti_sight_wall.ent` blocks sight | 69 of 86 (+ 3-07, 3-22, 3-32, 4-28, 4-29 red; 3-02 on 401) |
| 159 | G1b | `a682880` | built; verifier 11 gates, 1 medium (the stone's kill of the player: STK) and 5 low; 55 suites, 54 passed, 30,225 (the new `test_mp_shoot`, 100); 108 / 20 / 0; sweep 127 of 128 identical to G1a's (3-16 differs); 0 refusals | **yes**, on the 35 knight levels: a knight fires an `enemy_shoot.ent` that kills the player or, by its stored id, a portal; every shot ends in a burst and a shake of 10; a touch caps a knight's hold at 1000 ms; a barrel's blast and a stone's crush kill a knight; the seven stealth achievements read the real `SpotCount()` | 61 of 86 (25 red; 3-02 on 400) |
| 160 | STK | `356c32e` | built; verified three times (12 of 15 gates with 3 not run behind Smart App Control and one high; one failed gate, a stale gate row of the planning step, 10 of 11 in `verify2.json` and 13 of 14 in the step's own text; 12 of 12 with one medium, STK-9, and 5 low); 55 suites, 54 passed, 30,329; 108 / 20 / 0; sweep 128 of 128 identical to G1b's; 4 refusals (the runner's three, a verifier's one) | **yes**, on the 26 levels with a stone that has the callback: a stone whose own speed along the line to the player's point is above 2.5 hitIntensity kills him | 61 of 86 (3-12 now dies on tick 200 under its stone) |
| 161 | routes | `53d8db2` | the route phase: 25 route files re-authored, 61 untouched; verifier 11 of 11; 55 suites, 55 passed, 30,714; 108 / 20 / 0; the verifier's sweep 128 of 128 identical to STK's; 0 refusals; pushed | **none** (test data) | **86 of 86** |
| 162 | STK9 | `11d0be4` (pushed in wave 4a) | the impact watch reads a rolling stone's free fall as Box2D's step does; verified twice (round 1: 9 of 10 gates, one medium - two guesses in code, now in `earthquake.json` - and four low, applied by round 1; round 2: 10 of 10 gates and four low, the text one, the world's iteration count 8 and 5 and not 5 and 2, applied by the commit); 55 suites, 55 passed, 30,812 checks (`test_mp_camera` +40, `test_mp_layer` +58); 108 / 20 / 0; the f420 sweep 128 of 128 byte-identical to the standing baseline (14 levels' logs differ in a stone's reading before tick 420, each explained); 2 Smart App Control refusals (the runner's, one in each round, each relinked once; none in the verifiers') | **yes**, on the 26 stone levels: a stone that falls on the player kills him from a drop over 15.56 u (it was 25 to 30), one that falls on a knight crushes it from over 3.33 u, the stone's shake and hit sound read one to two steps higher | 86 of 86, on the same ticks (no route file moved and none is re-authored: the wave's route phase has nothing to redo for this step, and the routes of the 42 levels that have none are authored on this build) |
| 163 | WAKE | `f20f0a4` (pushed in wave 4a) | a sleeper whose support is destroyed, stops being solid or is moved by the port is woken before the next physics step (`Resting::Watch`, Box2D's contact rule: `b2Contact::Destroy` wakes both bodies of a contact that had points, `b2Contact.cpp:101-109`); `wake_reach_px` is gone for `touch_units` 0.5 (a measured plateau, 0.178 to 1.0), `moved_m` 1e-4 and the level's `rests_on`; 56 suites, 55 passed, 1 failed (`test_mp_routes`: 3-10, the census), 31,005 checks (`test_mp_wake` new, 200; `test_mp_timebase` +8); 108 / 20 / 0; the f420 sweep 128 of 128 byte-identical to step 162's (no sleeper of any level loses its support in the first 420 idle ticks: no `woken` line in the 128 logs); 1 Smart App Control refusal (the runner's, relinked once); verified once (`verify1.json`: 10 of 10 gates, 6 low, 0 refusals; the text ones applied by the commit, the two comments of `game/sim/Resting.hpp` left for its next real edit on purpose, a widely included header) | **yes**, wherever a body sleeps on something the port removes or moves: 3-10's knight B falls on tick 484 where it hung 730 ticks, and the census names 2-23, 2-24, 2-32, 4-07 and 3-12 (their routes win on the same ticks), the verifier's adds 1-30 and two more 2-32 pairs as the same exposure, and 1-09 does not change (its platform is inert) | 85 of 86 on the same ticks; **3-10 not won by 1,460** (the knight falls to the stone's landing and is crushed on 519): WAKE-1; the routes are re-authored by the wave's route phase (as after G1b, step 159, for the previous wave: the phase that follows the mechanics steps), which re-flies 3-10 - **done by step 164 (3-10 won on 1,017)** |

**Routes** (step 161, `53d8db2`): **86 of 128 levels are won by a route `test_mp_routes` replays on the final build** (86 files, 1,598 checks, 0 failures; chapter 1: 21 of 21, chapter 2: 29 of 29, chapter 3: 17 of 17, chapter 4: 19 of 19). The census went 86 -> 80 (R23) -> 74 (G9) -> 69 (G1a) -> 61 (G1b) -> 61 (STK: 3-12's failure changed from crushed knights to the player's death under its stone) with no route file touched, then 86 after the phase's one round. **At the start of the phase 25 did not win**: R23's six, G9's six, G1a's five, G1b's six and 3-10 and 3-12; their first problems were a tap whose ray ended short of or behind the tap (a doomed shot, a body 10 units in), a reflected leg still in the air at the next tap or opening elsewhere, a knight's sighting that stood it, shot the player or never let it walk, a stone's crush of the knight the route needs (3-10) and a stone's kill of the player (3-12). **Ticks moved**, against the last census of the previous wave (`R3b/verify3/all_t1.log`): of the 25 re-authored **21 win on another tick (16 later, 5 earlier) and 4 on the same** (2-16 397, 2-17 519, 3-14 1,141, 3-15 976), and 19 carry a new `win_by`; the largest moves are 3-10 804 -> 1,435 (+631), 4-29 1,541 -> 1,023 (-518), 3-07 732 -> 351 (-381), 3-31 1,458 -> 1,668 (+210) and 3-03 607 -> 776 (+169). **Of the 61 routes nobody edited, 60 win on the same tick and 1 on another**: 3-02 (`level1b`) 388 -> 400, moved under steps 156-160 with the cause not isolated (R3c-7). The median move of the 22 that moved is 46.5 ticks. Win ticks run 87 (4-02) to 3,343 (3-32), every win inside its `win_by` with 9 to 70 ticks of slack (median 30; the 25 re-authored 14 to 42). **The thin ones** are 4-18 (9), 4-02 (13), 3-13 (14) and 4-08 (15); **the knife-edges** are 3-20 (a second walk that must end on tick 238), 3-22 (a rush on a margin of about one tick, R3c-3), 3-13 (its crate's guessed friction, R3b-3) and 2-25's tap 313 (it works only while the camera still leads him, R3c-6). **Failed, and why**: no route was lost at the end; what the re-authoring found is in planning step 161. The knights see only the three 128-unit buckets of their own row, so 3-03, 3-31, 3-32, 4-28, 4-29 and 4-30 now win with no sighting at all, 3-09 is never sighted, 3-27 once and 3-08 twice (its player walks on out of both shots); 3-22 is won by a rush past its knight and 3-07 on foot by shoving one (R3c-2, R3c-3); a knight lifted off its floor never walks again, which 4-28 and 4-29 are won around (R3c-4); 3-10's knight does not fall when its wall breaks (R3c-5); R23's ray ends at the first body, so 2-10, 2-19, 1-23 and 3-13 wait for a burning or falling body to go before a tap; G9's reflector catches within 32, so 2-16, 1-21 and 2-25 end their shots outside it and 3-14 moves the player; 3-12 stands at x 45.2 out of its stone's way; 3-15's entry portal lies inside an `anti_sight_wall`. The stealth achievements: 4-30's unseen route unlocks 76 at the door, the seen routes of 3-02 and 4-25 unlock none, and 26, 28, 32 and 42 (3-04, 3-05, 3-11, 3-24: no route) are pinned only by `test_mp_layer` and `test_mp_achievements` (R3c-8).

**Suites**: 55 suites, 55 passed, 30,714 checks at the end (29,723 after R23, 29,788 after G9, 30,125 after G1a with the new `test_mp_sight`, 30,225 after G1b with `test_mp_shoot`, 30,329 after STK, and 30,714 with the routes' 1,598 checks in place of 1,213); `test_mp_routes` was red by design inside every step and is green now. **Reachability**: 108 / 20 / 0, 0 differ at every step and in the verifier's run. **Verification**: R23, G9 and G1a each passed 13 of 13 gates (5, 3 and 6 low findings, the text ones applied at their commits), G1b 11 of 11 (one medium, which STK built, and five low), STK's three rounds 12 of 15 (three not run behind Smart App Control, one high), one failed gate (a stale gate row of the planning step: 10 of 11 in `verify2.json`, 13 of 14 in the step's own text) and 12 of 12 (one medium, STK-9, and five low), and the route phase's whole-build verifier 11 of 11 on its first round. **Smart App Control**: 9 refused launches in the wave, counted at this close from a CodeIntegrity 3033/3077 query from 2026-10-05 07:38 (its other events are other programs'): R23's runner twice (10-05 08:28 and 08:36), G9's runner (10-10 16:32) and `MagicPortals.exe` (16:35), G1a's `MagicPortals.exe` (18:15), STK's runner three times (21:42, 21:46, 21:46) and its first verifier's once (22:18); none in G1b, the route phase, the finalize or this close, and each refused executable was deleted and relinked and the relinked one ran (R23's runner twice; wave 1: 83, wave 2: 19, wave 3a: 2, wave 3b-1: 4). Zero warnings rests on each step's build logs and the verifier's `port_build.bat` (MagicPortalsSpike only, R3c-12); the close built nothing.

**The standing f420 baseline** is now `out/parity/program/STK/after/sweep` (128 PNGs, 1280 x 720, taken by STK on `MagicPortals.exe` `4ae49bca`, the executable the committed sources build: the route phase changed no source, and its verifier's 128 launches were byte-identical to this folder before it deleted its own PNGs). The close checked all 128 md5s against `STK/after/capture.txt`, the PNG signatures and the size, before the purge and again after it. `out/parity/BASELINE.txt` names it alone, replacing `R3b/fix1/after/sweep` and the four intermediate sweeps (R23, G9, G1a, G1b) that session 26 had kept as each next step's "before"; the purge deleted the other 653 images under `out/parity/program` (0.49 GB; free disk 114.58 -> 115.04 GB).

**GAMEPLAY changed in wave 3b-2**, each as the original does and each in its planning step: R23 (every tap: the shot flies to the tap-time ray's end and opens its portal there or fails there; only a tap that reaches the hourglass restarts the 400 ms; the zero-g recoil is away from the ray's end; `no_portal_705` is no zone), G9 (on the 19 levels with a reflector: it catches within 32 units and turns the shot on its own point, by one reflector at most), G1a (on the 35 knight levels: a knight sees through a 120 degree cone in its own row, stands while it does, shows `!` and `?` with their sounds, searches when its way is blocked, and is blind on the six darkest levels until a torch is lit), G1b (the knights shoot and kill the player and portals, every shot ends in a burst and a shake of 10, a touch caps a knight's hold at 1000 ms, a barrel's blast and a stone's crush kill a knight, the seven stealth achievements are real) and STK (on the 26 stone levels, a stone that hits the player fast enough kills him). None in the route phase (test data). R24 and R25 were not touched and G10-2 was not reopened (`clock.json` `matched_hz` stays 60); the engine pin stays `710bffc` and no converter file changed, so `out/levels`, `out/assets` and `out/data` are the ones the wave started from.

**The wave's rows**: R23-1 to R23-11, G9-1 to G9-7, G1a-1 to G1a-11, G1b-1 to G1b-10, STK-1 to STK-9 and R3c-1 to R3c-13 (61 rows), of which 12 are closed: R23-1, R23-9, G9-6, G1a-1, G1a-7, G1a-9, G1a-10, G1b-5, G1b-8, G1b-10, STK-5, R3c-12. The open ones are each an owner call, a measurement on the original, or a small source change that waits for the next edit of its file (the comment-only ones were left on purpose, because an edit of a `.cpp` relinks the executables and every refused launch notifies the owner).

**What the next wave should take (wave 4)** *[wave 4a took (1), STK-9's step and the sleepers' wake: its record is the "Wave 4a" paragraph that follows, and (2) and (3) are re-listed there as wave 4b's]*, in the program's order: (1) **routes for the levels not won**: the 42 with no route (1-13, 1-17, 1-18, 1-19, 1-20, 1-22, 1-24, 1-25, 1-26, 1-30, 1-31; 2-27, 2-28, 2-29; 3-01, 3-04, 3-05, 3-06, 3-11, 3-16, 3-17, 3-18, 3-21, 3-23, 3-24, 3-25, 3-26, 3-28, 3-30; 4-03, 4-04, 4-05, 4-09, 4-10, 4-12, 4-13, 4-20, 4-22, 4-23, 4-24, 4-26, 4-27), with the causes already recorded: 1-17 (a pocket sealed to shots until its crates are cleared), 2-27 (step 139's finding: only a shot its fixed flame turns into a fireball, aimed by the handle's reflector, reaches the door room's crates; G9's reflector now catches within 32, so the aim has to be found again on this build) and 3-23 (step 140's finding: both keys are carried by knights; step 134's flame can burn a minion, and the knights now see and shoot, so it is a G1 route too); nine of the 42 hold a lift door (1-13, 1-18, 1-26, 1-31, 2-29, 3-05, 3-16, 4-22, 4-24); four are the stealth levels 3-04, 3-05, 3-11 and 3-24, whose routes would also show achievements 26, 28, 32 and 42 (R3c-8); the rest have no recorded attempt. The rules a route author works under now are in planning step 161 and rows R3c-2 to R3c-5 and R3c-10. **Before the routes or beside them, STK-9's own step** (the impact watch reads a free fall 0.33 to 0.67 low: measure a stone on the player or take the unbraked free-flight arithmetic, re-pin the falls, re-run the routes; no winning route is near the line, R3c-1) - **DONE as step 162 (STK9, wave 4a): the arithmetic, replayed in the watch; the 86 routes win on the same ticks; the player's, a knight's and a crate's begins are STK9-1**; and **the sleepers' wake (R3b-4, R3c-5) - DONE as step 163 (WAKE, wave 4a): a sleeper whose support is destroyed, stops being solid or is moved is woken before the next step; 85 of the 86 routes win on the same ticks and 3-10's knight, which now falls, takes its route with it (WAKE-1: the route phase re-flies it)**; (2) **the final comparison** against the original's library (section 5), with the pixel-centre rule for every picture at the frame's edge (step 145's deviation 1) done first, and its residuals recorded: G1a-11 (the player drawn lit on the six darkest levels), G1a-8 (the marks' place and the `?`), the rig checks of R3c-11 and the rows that already wait on it; (3) **the ship preparation**: the grant in `docs/permission.md` (which still holds its "INCOMPLETE - PASTE THE ACTUAL GRANT HERE" placeholder, so the record of the scope is incomplete), the play build (`tools\parity\play_build.bat`; there is none built) and the owner's calls, listed here once: 2-30's creep (R3-2: the stone's decoded friction, G10-2 lower, or the creep accepted until the engine's friction step), the engine's friction and restitution steps (G10-14, WT-1: queued with the Penumbra port's engine work or not), G10-2's matched 60 Hz, R24 and R25, `crate_low_emissive`'s guessed 0.08 (R3b-3), 2-32 ahead of the dragon's camera (R3-3), STK-8 (what he saw near a launcher's drop: a sensor, or a thrown stone), PIC-1's smoothing above 80 Hz, and the standing ones in "The owner's calls" below. **With the wave**: R3b-6 and R3c-9 (`win_by` tightened when those files are next touched) and 3-20's knife-edge (R3-4); R3-3 is unchanged (2-32's route was not touched and wins on 3,205).

**Wave 4a (the impact watch's free fall, the sleepers' wake and the routes of the 42 levels that had none: STK9, WAKE, then the route phase; 2026-10-11 02:29 to 09:09; closed
2026-10-11, PUSHED).** Three steps, each a local commit while it ran, pushed together once the routes were won and an independent verifier had checked the whole build:
`origin/main` is `8fe8374` (`36a52e6..8fe8374`, a fast-forward, 62 files, +3,404 / -145). The wave ran on the owner's standing order of 2026-10-10 (continue until finished or
told to stop); 02:29 is the creation time of `out/parity/program/STK9/start_time.txt`, the first record of the run and not a measured start, and 09:09 is the route commit's time.
The engine pin stayed `710bffc` and no converter file changed, so `out/levels`, `out/assets` and `out/data` are the ones the wave started from.

| Step | Row | Commit | Outcome | GAMEPLAY | Routes win |
|---|---|---|---|---|---|
| 162 | STK9 | `11d0be4` | the impact watch reads a rolling stone's free fall as Box2D's step does (the velocity after the first step whose free position is within the pair's skin, replayed from the last step the engine had not braked: `Impacts::FreeFlightVelocity`); closes STK-9 and R3c-1, opens STK9-1 to STK9-8; verified twice (round 1: 9 of 10 gates, one medium - two guesses in code, now in `earthquake.json` - and four low; round 2: 10 of 10 gates and four low); 55 suites, 55 passed, 30,812 checks (`test_mp_camera` +40, `test_mp_layer` +58); 108 / 20 / 0; the f420 sweep 128 of 128 byte-identical to the standing baseline; 2 Smart App Control refusals (the runner's, one in each round, each relinked once) | **yes**, on the 26 stone levels: a stone that falls on the player kills him from a drop over 15.56 u (it was 25 to 30), one that falls on a knight crushes it from over 3.33 u, and the stone's shake and hit sound read one to two steps higher | 86 of 86, on the same ticks (no route file touched) |
| 163 | WAKE | `f20f0a4` | a sleeper whose support is destroyed, stops being solid or is moved by the port is woken before the next physics step (`Resting::Watch`, Box2D's contact rule: `b2Contact.cpp:101-109`); closes R3b-4 and, as to the port's gap, R3c-5; opens WAKE-1 to WAKE-7; verified once (10 of 10 gates, 6 low); 56 suites, 55 passed, 1 failed (`test_mp_routes`: 3-10, the census), 31,005 checks (`test_mp_wake` new, 200); 108 / 20 / 0; the f420 sweep 128 of 128 byte-identical; 1 refusal (the runner's, relinked once) | **yes**, wherever a body sleeps on something the port removes or moves: 3-10's knight B falls on tick 484 where it hung 730 ticks; the census names 2-23, 2-24, 2-32, 4-07 and 3-12 (their routes win on the same ticks) and the verifier's adds 1-30 and two more 2-32 pairs | 85 of 86 on the same ticks; 3-10 not won by 1,460 |
| 164 | routes | `8fe8374` | the route phase: **42 new route files and 3-10 re-flown on the sleeper-wake rule: 128 of 128 levels won**; the first verifier round failed one gate (two untracked scratch directories in `tests/routes`, moved out of the tree), the second passed 13 gate rows; 56 suites, 56 passed, 31,788 checks; 108 / 20 / 0; the verifier's sweep 128 of 128 byte-identical to WAKE's; 0 refusals; pushed | **none** (test data) | **128 of 128** |

**Routes** (step 164, `8fe8374`): **128 of 128 levels are won by a route `test_mp_routes` replays on the final build** (128 files, 2,366 checks, 0 failures, no SKIP, no late win;
chapter 1: 21 -> 32 of 32, chapter 2: 29 -> 32, chapter 3: 17 -> 32, chapter 4: 19 -> 32). The commit holds 43 files of `tests/routes/` (42 new, `level9b.json` edited) and the
planning step, and no source, test, data, converter or engine file. At the phase's start 85 of the 86 won and 3-10 did not; **the 42 new routes win on ticks 83 (4-05) to 1,136
(3-05), median 379, each with `win_by` = won tick + 11 to 15, and 3-10 wins on 1,017** (was 1,435; `win_by` 1,030); the other 85 win on exactly the ticks of step 163's census (0
differences), and across all 128 the win ticks run 83 to 3,343 (3-32). **New files: 11 in chapter 1** (1-13, 1-17 to 1-20, 1-22, 1-24 to 1-26, 1-30, 1-31), **3 in chapter 2**
(2-27, 2-28, 2-29), **15 in chapter 3** (3-01, 3-04, 3-05, 3-06, 3-11, 3-16, 3-17, 3-18, 3-21, 3-23 to 3-26, 3-28, 3-30) and **13 in chapter 4** (4-03, 4-04, 4-05, 4-09, 4-10,
4-12, 4-13, 4-20, 4-22, 4-23, 4-24, 4-26, 4-27). **Levels still without a route: none, and no `finding_*.md` was needed.** **What the 42 needed** (chiefly; each `_about` is the full
list): portal hops of a player who cannot jump (nearly all; 1-17 costs exactly its golden 10 portals, 1-13 is four pairs past a lift door, 3-01 three hops); rolling stones, launchers
and breakable walls (1-13, 1-19, 1-20, 1-22, 1-24, 1-25, 1-26, 2-29, 4-22, 4-24 and 3-10); lift doors and buttons (the nine - 1-13, 1-18, 1-26, 1-31, 2-29, 3-05, 3-16, 4-22, 4-24 -
and the lift a touch sends, 3-10 and 3-23); the reflector (G9: 1-13, 1-20, 2-27, 3-25, 3-26, 4-23); fire (G8: 2-27, 2-28, 3-30, 4-10, 4-12); the weightless levels' recoil (R23:
4-03, 4-04, 4-05, 4-09, 4-10, 4-12, 4-13); the dark levels (4-20, 4-24, 4-26, 4-27); knights and keys (G1a and G1b: 3-01, 3-06, 3-16, 3-17, 3-18, 3-21, 3-23, 3-30); shock rings
(3-24, 3-25, 3-26, 3-28, 4-04, 4-05); on foot only: 2-28 (its budget is 0) and 3-04. **Sighted**: ten of the 42 are seen by a knight at least once (3-01, 3-06, 3-16, 3-17, 3-18,
3-21, 3-23, 3-26, 4-26, 4-27) and in five a knight shoots (3-06, 3-16, 3-23, 3-26, 4-27) without hitting him; the other 32, the four stealth levels among them, are never sighted.
**The stealth achievements are shown** (R3c-8, closed): 3-04 unlocks 26 on tick 297, 3-05 unlocks 27 and 28 on 1,136, 3-11 unlocks 32 on 215 and 3-24 unlocks 42 on 142, each unseen
with no `enemy_spotted` and no `enemy_shot` in its sounds; the other unlocks the new routes show are 33 (3-16), 40 (3-23), 48 (1-20), 61 (4-20), 64 (4-22) and 75 (4-27).
**Findings** (none blocking; rows R4a-1 to R4a-12 above): 1-19 and 1-20 win because the destroyable stone survives the wall it breaks (Order 11.1), 4-10 and 4-12 rest on the
friction-0 player's wall-slide, 1-22 on a stone creeping below the wall's gate (R3b-2, R3-2), 1-24 reaches its door only over destroyier's kill box (STK-8; reachability does not
cover it), 1-30 on a solver shove out of a pile (R3b-5), 2-29 on the port's stone clock (WT-10), 3-10 as a workaround for the crushed knight (R3c-4), and a list of routes that
rest on port behaviour no clip shows. No tunnel was found: the verifier's audit of all 128 traces hit 7 routes, each with the body gone or broken or on a seam, and the longest run
outside the camera's view is 5 ticks (3-18, 4-18, 2-27).

**Suites**: 56 suites, 56 passed, 31,788 checks at the end (30,714 after step 161; 30,812 after STK9; 31,005 after WAKE with `test_mp_routes` red on 3-10; 31,788 with the routes' 2,366
checks in place of 1,583). **Reachability**: 108 / 20 / 0, 0 differ at every step and in both verifier rounds (it has no destroyier term: R4a-4). **Verification**: STK9 twice (9 of
10, then 10 of 10 gates), WAKE once (10 of 10), the route phase twice (round 1 failed one gate of the git state, two scratch directories in `tests/routes` that the fix round moved
out; round 2 passed 13 gate rows with 11 problems, none blocking). **Smart App Control**: 3 refused launches in the wave, counted at this close from a CodeIntegrity 3033/3077 query
from 2026-10-11 00:00 (its only events are these): STK9's runner twice (02:53:49 and 03:59:17) and WAKE's runner (05:36:06); none in the route phase, the verifiers, the finalize or
this close, and each refused executable was deleted and relinked once and the relinked one ran (wave 1: 83, wave 2: 19, wave 3a: 2, wave 3b-1: 4, wave 3b-2: 9). Zero warnings rests
on the steps' build logs; the route phase and this close built nothing.

**The standing f420 baseline** is now `out/parity/program/WAKE/after/sweep` (128 PNGs, 1280 x 720, taken by WAKE on `MagicPortals.exe` `1c3c00a2`, the executable the committed
sources build: STK9 and the route phase changed no source after it, and the route phase's verifier took 128 fresh launches whose md5s equal this folder's 128 of 128). The close
checked all 128 (PNG signature, the closing IEND chunk with no NUL padding, 1280 x 720, md5 equal to its line of `WAKE/after/capture.txt` and to the verifier's list) before the purge
and again after it. `out/parity/BASELINE.txt` names it alone, replacing `program/STK/after/sweep`; the purge deleted the other 256 images under `out/parity` (the STK and STK9 sweeps,
0.19 GB; every role had deleted its own scratch; free disk 113.99 -> 114.17 GiB, `out/parity` 17.85 -> 17.67 GiB). About 11,000 older images outside `program/` stay by the purge
tool's design and are not this wave's. `R4/verify2/saves/` (128 scratch save copies, JSON, no images) was left in place.

**GAMEPLAY changed in wave 4a**, each as the original does and each in its planning step: STK9 (on the 26 stone levels, a stone kills the player from a drop over 15.56 u and a
knight from over 3.33 u, and the stone's shake and hit sound read one to two steps higher) and WAKE (a body asleep on something the port removes, stops being solid or moves falls at
once: 3-10's knight, and the census's 2-23, 2-24, 2-32, 4-07, 3-12 and 1-30). None in the route phase (test data). R24 and R25 were not touched, G10-2 was not reopened
(`clock.json` `matched_hz` stays 60), 2-30's creep (R3-2) and 3-13's 0.08 (R3b-3) stand.

**The wave's rows**: STK9-1 to STK9-8, WAKE-1 to WAKE-7 and R4a-1 to R4a-12 (27 rows), of which 1 is closed (WAKE-1); the older rows the wave closed are STK-9 and R3c-1 (step 162), R3b-4
and R3c-5 as to the port's gap (step 163) and R3c-8 (step 164). The open ones are each an owner call, a measurement on the original, a re-fly that waits for its mechanism, or a small
source change that waits for the next edit of its file.

**What the next wave should take (wave 4b)** *[wave 4b took PX and (2)'s comparison, its record is the "Wave 4b" paragraph that follows; (1), (3) and what is left of (2) are re-listed there as wave 4c's]*, in the program's order: (1) **the levels still without a route: none** (128 of 128). What is left of the routes is the re-flies each row
names when its mechanism is built or its rig check is run: 1-19 and 1-20 (Order 11.1, R4a-1), 1-22 (R3b-2 and R3-2, R4a-3), 1-24 (STK-8, R4a-4), 1-30 (R3b-5, R4a-5), 2-29 (WT-10,
R4a-6), 3-10 and 3-16 (R3c-4, R4a-7), 4-10 and 4-12 (a zero-g wall-slide measured on the original, R4a-2) and the routes of R4a-8; (2) **the final comparison** against the original's
library (section 5), with the pixel-centre rule for every picture at the frame's edge (step 145's deviation 1) done first, and its residuals recorded: G1a-11 (the player drawn lit on
the six darkest levels), G1a-8 (the marks' place and the `?`), R3c-11's rig checks and the routes' unmeasured items (R4a-8); (3) **the ship preparation**: the grant in
`docs/permission.md` (which still holds its "INCOMPLETE - PASTE THE ACTUAL GRANT HERE" placeholder, so the record of the scope is incomplete: the owner's to fill), the play build
(`tools\parity\play_build.bat` exists; no play build has been made) and the owner's calls, listed here once: 2-30's creep (R3-2), the engine's friction and restitution steps
(G10-14, WT-1), G10-2's matched 60 Hz, R24 and R25, `crate_low_emissive`'s guessed 0.08 (R3b-3), 2-32 ahead of the dragon's camera (R3-3), STK-8, PIC-1's smoothing above 80 Hz and
the standing ones in "The owner's calls" below; and **the repository's going public, which still needs the owner's explicit word**. **With the wave**: R3b-6 and R3c-9 (`win_by` of
the 85 older routes tightened when those files are next touched), 3-20's knife-edge (R3-4), the comment-only fixes that wait for their file's next real edit (the two comments of
`game/sim/Resting.hpp` among them: a widely included header, so a comment-only edit would relink both executables and risk a refused launch) and WAKE-7's tests, with R4a-10's one check.

**Wave 4b (the pixel-centre rule at the frame's edge, then the final comparison's measuring pass: PX, then CMP; 2026-10-11 09:31 to the push, whose report is stamped
18:48; closed 2026-10-11, PUSHED).** One rendering step and one read-only measuring pass, each checked by independent verification, on the owner's standing order of
2026-10-10. `origin/main` is `7eae0e4` (`f240af9..7eae0e4`, a fast-forward, one commit, 12 files, +2,296 / -3, pushed by the finalize role); 09:31 is the earliest file
left under `out/parity/program/PX/` (the step's first spot check), not a measured start. The engine pin stayed `710bffc` and no converter file changed, so `out/levels`,
`out/assets` and `out/data` are the ones the wave started from.

| Step | Row | Commit | Outcome | GAMEPLAY | Routes win |
|---|---|---|---|---|---|
| 165 | PX | `7eae0e4` | the pixel-centre rule for every level picture at the frame's edge: a strip of the picture's own material over the gap its edge leaves in the outer pixel (`EdgeCover::StripsOf`, `MagicPortalsLayer::syncEdgeCovers`, `game/data/edge_cover.json`), the complement for a picture whose inward edge is in the outer pixel (a strip, or the quad left out of the frame where it covers no pixel centre), from a pool of 24 made before any other entity; gate 1 met (2-05's column 1279, rows 421-586, (8.92, 14.63, 16.66) -> (0.98, 1.16, 1.29), the original's (1.04, 1.16, 1.34), 164 of 166 rows equal), gate 2 NOT met (edge rows above 12: 777 -> 614 on the left and right columns, 2,210 -> 2,047 on all four sides, the whole fall 2-05's), gate 4 PARTIAL; verified three times (round 1: 1 high, 1 medium; round 2: 1 high, 2 medium; round 3: 10 gates, 5 low) and once more by the comparison's verifier (14 gates, 6 low); 57 suites, 57 passed, 32,314 checks (`test_mp_edgecover` new, 414; `test_mp_layer` 3,876); 108 / 20 / 0; the level walk 134 / 1,460 / 1,100 / peak 109 / 0 failures; the f420 sweep 127 of 128 byte-identical to the standing baseline (2-05's 211 px aside), 0 px outside the ring on all 128 | none (a second quad in the outer pixel; nothing the simulation reads moves) | 128 of 128 on the census's ticks (no route file touched) |
| - | CMP | none (read-only) | the final comparison's measuring pass: 128 levels against the library's stills, 468 residuals in 18 causes and CMP-low; **26 `match`, 44 `minor`, 58 `differs`**; independently verified (14 gates, 6 low, 0 of 6 spot-checked pair images wrong) | none (no source edit) | - |

**PX** (planning step 165, `7eae0e4`). The original draws one sample a pixel, at its centre, so a picture whose edge lies in the frame's outer half pixel fills that pixel
whole; the port's 4x MSAA left it a blend with what lies behind. The first build made its strips on demand and moved 133 px of additive glow outside the ring on five
levels (the renderer breaks depth ties by store order); the pool made first fixed it (the cause is INFERRED: the gather order was never dumped). Round 1's verification
found the level walk failing (259 failures: a hidden strip kept its picture's lightmap paths) and the shake's top row popping (the complement was added); round 2's found
the walk still failing on 4 (a strip's ids a frame behind its picture's, fixed in `LevelVisit::measure`); round 3 passed 10 gate rows. **Of step 145's deviation 1 only
2-05 is closed** (the row stays `[ ]`, with PX-1 to PX-8 below its block). The final build is `MagicPortals.exe` `a5013ee6` and `test_mp.exe` `32f4a14b`;
`game/data/edge_cover.json`'s notes were edited after the gates (md5 `4b313617` -> `d7484711`, no key changed, PX-8).

**CMP** (the comparison's measuring pass, `out/parity/program/R4b/CMP/`; no source edit, no relink, no route edit). The port at f420 (and, as extra sets, f75, f135, f240,
f450 and each level's own HUD tick) against the library's `_h6.3`, `_h0.3`, `_h2.8` and t-series stills of all 128 levels (140 launches of `MagicPortals.exe`, 0 refusals;
2,282 images, each opening at its size with its IEND chunk), 16 readers of 8 levels each, then one triage rule: `differs` = a real residual of severity high, `minor` =
the worst is medium, `match` = only low (**no residual above low: phase, clock, tone; not pixel-equal**), the same cause graded the same on every level. **Counts: 26
`match`, 44 `minor`, 58 `differs`** (the readers' own tally: 26 / 47 / 55; five levels re-graded); by chapter 1: 11 / 20 / 1, chapter 2: 0 / 0 / 32, chapter 3: 0 / 19 /
13, chapter 4: 15 / 5 / 12. **52 of the 58 `differs` levels differ by CMP-1 alone** (the close's count from `per_level.md`: all their high residuals are the player or the
exit arch drawn lit); the other six have their own: 1-30 (CMP-4), 2-31 (CMP-2), 3-05 (CMP-5), 4-20 (CMP-7), 4-29 (CMP-6), 4-31 (CMP-3 and CMP-2). 468 residuals (82 high,
125 medium, 261 low), every one of the 207 high and medium in a group (the verification parsed all 303 evidence lines). **The section-5 box is not ticked** and will not
be until the structural groups are fixed.

**How far to trust it.** (a) **Pairing**: `h6.3` against f420 is the record's convention and not a measurement; the original runs two clocks the port runs as one (its HUD
0.49-0.94 s ahead of its world, mean 0.76 s), so no single port tick matches a still's HUD and its movers; the script clock was fitted on four platform levels in chapters
1-3 (+-0.4 s level to level) and not on chapter 4, whose "match" levels carry the highest mean abs (4-18 11.38, 4-12 10.30) on a phase explanation nobody measured. (b)
**Seed**: every capture used the all-gold save the library's stills were taken on (`09268640`), not the owner's (`40bfef3a`): the plaque is the only difference at tick 90
(1-01 0 px, 3-16 42,165 px, 2-32 42,202 px) and the 128 f420 frames equal the owner's-seed sweep byte for byte except 1-02 and 1-03 (popup closed by a scheduled tap). (c)
**Readers**: the audit re-measured 16 of 128 levels (one per chunk): 16 of 16 agree, 0 missed, 0 not real, and its own first pass missed items the readers had; it covers
12.5 percent of the levels and re-measured pixels, not causes, and the readers did miss the help block on 2-03 and 2-06 outside the sample (CMP-10). The triage checked 11
cause attributions against the listing (md5 `48fe65ce`) and the Ethanon source: 10 hold, the draw-order one only on 7 levels. The verification re-derived the CMP-1 census
(78 levels carry `ignore_emissive`) and re-read six pair images, and re-ran every suite. The limits are row CMP-R1.

The groups (one open row each below, the block before "Blockers"; `movesRoutes` = the fix changes a physics or sim number a winning route leans on, so GAMEPLAY):

| Id | Levels | Severity | Kind | Fixability | Moves routes | Cause in one line |
|---|---|---|---|---|---|---|
| CMP-1 | 78 | high | light | decoded | no | `ignore_emissive` and `door_emissive` are read by neither the port nor the converter: the player and the exit arch are drawn lit |
| CMP-2 | 10 | high | structural | decoded | yes | the port drops the revolute joint's attach point B: planks stand where placed, not where the joint pulls them (2-31 29 u) |
| CMP-3 | 9 | high | structural | decoded | no | equal-z pictures are drawn in file order; the original orders them by 128-unit bucket |
| CMP-4 | 4 | high | structural | needs-rig | yes | piles at rest in the original topple or creep in the port (1-30, 2-13, 2-05) |
| CMP-5 | 2 | high | structural | needs-rig | yes | 3-05's knight A absent for 8 s in the original; 3-22's knight leaves its plank early |
| CMP-6 | 2 | high | structural | decoded | yes | `minion_spawn6` (4-29) and `minion_spawn_` (3-09) have no `ETHCallback`: the original spawns no knight |
| CMP-7 | 2 | high | light | decoded | no | the red key's own `AddLight` is not built (4-20; 4-19 holds the other red key) |
| CMP-8 | 50 | medium | light | owner-call | no | `halo_brightness_scale` 0.6 is a `_guess`: halos read weaker (torches 0.75-0.9, blue lamp 0.5-0.86) |
| CMP-9 | 32 | medium | light | decoded | no | script emissive blinks not built: barrels, the wall's `blink`, teleportable crates |
| CMP-10 | 13 | medium | structural | decoded | no | the help block is plain and opaque; the original draws alpha 0.76 breathing 1.05-1.13x |
| CMP-11 | 5 | medium | structural | owner-call | yes | crates and barrels creep or rest elsewhere (friction and restitution decoded, held: G10-14, WT-1) |
| CMP-12 | 4 | medium | particles | unknown | no | the blue lamp's orb is smaller and bluer, its bracket dark |
| CMP-13 | 4 | medium | particles | needs-rig | no | the static portals' rings cover the original's grey spoked iris |
| CMP-14 | 3 | medium | light | unknown | no | lit stone reads softer on lightmapped levels (67 carry `eth_lightmap`, 3 judged) |
| CMP-15 | 1 | medium | structural | decoded | yes | 4-02's rider drifts off a bobbing slab (`MoveKinematic` sets a velocity the original's `SetTransform` does not) |
| CMP-16 | 1 | medium | structural | decoded | yes | 2-01's crate carries `scale` 0.75 the port ignores (178 x 175 px against 134 x 130) |
| CMP-17 | 1 | medium | structural | owner-call | yes | 3-08's lone guard rests 30 px right, astride a seam (R24) |
| CMP-18 | 1 | medium | structural | unknown | yes | 2-29's stone 125-135 px behind in every still (WT-10) |
| CMP-low | 120 | low | phase | unknown | if CAP-2 | clock and phase, the 2 px rest skin, opening camera, bob centre, tone: no step |

**Suites and checks**: 57 suites, 57 passed, 32,314 checks (the runner's no-argument launch, run in the second and third PX verifications and by the comparison's
verifier; 31,788 at the start of the wave); `test_mp_routes` 128 of 128 on the census's ticks, 2,366 checks. **Reachability** 108 / 20 / 0, 0 differ, at every step and in
every verification (it still has no destroyier term: R4a-4). **Zero warnings** rests on PX's build logs (0 lines containing `warning`); the comparison's verifier did not
rebuild, and the executables are newer than every compiled source (CMP-R1). **Smart App Control**: 13 refusals (each a pair of CodeIntegrity 3033/3077 events) from 09:21,
the wave's start, counted at this close, of which 12 are this repository's executables and 1 is python's: PX's implementer 6 (`test_mp.exe` 4 times, `MagicPortals.exe`
twice), its first verification 1 (the game exe, 12:22:18, after 130 good launches), the fix round 4 (the game exe on one set of sources: three links and a retry) and the
harness fix 1 (14:52:54); the comparison's own 140 launches and the verifier's 130 game launches and one runner launch had none, and one `python.exe` launch was refused
at 17:14:08 loading scipy's `_flapack` extension (a triage tool; not a wave executable). Wave 1: 83, wave 2: 19, wave 3a: 2, wave 3b-1: 4, wave 3b-2: 9, wave 4a: 3. The
owner's folder holds `scores.json` alone (164 bytes, md5 `40bfef3a`) before and after every role.

**The standing f420 baseline** is now `out/parity/program/PX/after/sweep` (128 PNGs, 1280 x 720; `out/parity/BASELINE.txt` names it, with `program/R4b/CMP/port_f420` (128
PNGs and the two popup-up frames of 1-02 and 1-03 in `popup_up/`) and `program/R4b/CMP/pairs` (256: 128 pairs and 128 overlays), which the next wave measures its fixes
against). Its caveat: it was taken at 10:22-10:40 on `MagicPortals.exe` `195f5bfa` (PX's round 1, the pool build), not on the committed `a5013ee6`; the committed build's
output equals it byte for byte on all 128 (the comparison's verifier's own 128 launches on `a5013ee6`, md5 for md5) and on 126 of the comparison's frames (1-02 and 1-03
differ by the popup tap), and the close checked all 128 (PNG signature, closing IEND chunk, 1280 x 720, md5 equal to its line of `PX/after/capture.txt`,
`R4b/verify1/sweep_md5.txt`) before and after the purge. Against the previous baseline (`WAKE/after/sweep`, deleted) it differs on 2-05 alone. **The wave's image purge**
(the owner's standing order of 2026-10-03): the comparison's scratch (`CMP/pairs_other`, `CMP/port_other`, `CMP/crops`) 1,896 images, 1.263 GB, and the purge tool's `all`
tier 744 images, 0.445 GB (PX's moves, crops and `verify2/after` sweep, and WAKE's old baseline; 0 refused), **2,640 images, 1.708 GB in all; `out/parity` 19.571 ->
17.980 GiB, free disk 113.441 -> 114.822 GiB** (the disk gained less than `out/parity` lost; INFERRED: other processes write to C:). Also deleted: 97 scratch `--saves`
folders under `PX/`, `R4b/` and `R4/verify2/` (1,994 `scores.json` / `achievements.json` copies; nothing under `merge_w1/sweep_saves`, `ui1/saves_gold` or `saves/`).
Kept: the three folders above, `original/`, `smoke/`, `markers/`, `saves/`, `asbc/`, the audit, every mp4 and every text, json, script and log file; about 11,000 older
images outside `program/` stay by the purge tool's design.

**GAMEPLAY changed in wave 4b: none.** PX adds a second quad in the frame's outer pixel and the comparison changed nothing. The groups' fixes are each stated in their
row: CMP-2, CMP-4, CMP-5, CMP-6, CMP-15, CMP-16 and CMP-18 would change gameplay (each as the original does) and wave 4c takes them in one batch with one re-fly of the
routes; CMP-1, CMP-3, CMP-7, CMP-9, CMP-10 and CMP-12 to CMP-14 change drawing only. R24, R25, G10-2's matched 60 Hz, 2-30's creep (R3-2), 3-13's 0.08 (R3b-3) and the
engine pin were not touched.

**The wave's rows**: PX-1 to PX-8 (8; PX-1 is settled history, closed at birth) and CMP-1 to CMP-18, CMP-low and CMP-R1 (20): 28 opened, 1 closed; no older row was closed
(step 145's deviation 1 is partly closed, 2-05). G1a-11 is the general case of CMP-1.

**What the next wave should take (wave 4c)** *[wave 4c-1 took (1)'s five drawing groups, CMP-1, CMP-3, CMP-7, CMP-9 and CMP-10: its record is the "Wave 4c-1" paragraph that follows, and what is left is re-listed there as wave 4c-2's]*, in this order: (1) **the groups whose fixability is `decoded`, in order of severity, each its own step with its gates** (a
drawing step's gate: the f420 sweep changes only the levels its group names, the rest byte-identical to `PX/after/sweep`, or each change explained against it (CMP-3 also
reviews the other 119 levels for flips); the group's boxes read the original's; the group's pair images re-taken): CMP-1 first (high, 78 levels, 52 of the 58 `differs`
levels, no route moves), CMP-3 (high, 9 levels), CMP-7 (high, 4-20 and 4-19), then CMP-9 (32 levels) and CMP-10 (13): all drawing only. Then the four that change gameplay
and move routes: CMP-2 (high: the converter carries attach point B, the hinge uses it; a change on both sides of the `.tscn`, re-converted into a staging directory under
`out/` and compared before it replaces anything), CMP-6 (high: two names out of `entity_roles.json`), CMP-15 (4-02's slab) and CMP-16 (2-01's crate scale). **The routes
they move are re-flown once, after all four**: 2-28, 2-29, 2-31, 3-15, 3-19, 3-20, 3-22, 4-11 and 4-31 (CMP-2; 2-30 is predicted unchanged; 3-20's knife-edge, R3-4), 4-29
and a re-check of 3-09 (CMP-6), 4-02 (CMP-15) and 2-01 (CMP-16), each against `R4/census_after_round1.md`. (2) **The open mechanics rows, each at the status it holds**:
the breakable wall's contact as the original has it, one callback (`ETHBeginContactCallback_breakable_wall`): Order 11.1's first clause (a `destroyable` stone dies at the
first wall it breaks, #42-60: decoded, GAMEPLAY, an owner row the standing ruling decides for the original) and R3b-2's gate (a stone must be faster than scale(1.5),
#16-33: decoded as R4a-3 reads it; R3b-2's own row still says the callback is not decoded), re-flying 1-19 and 1-20 (R4a-1) and 1-22 (R4a-3), re-checking 3-12's and
3-14's breaks, and the thrown stones of 1-24 and 1-25, in the same re-fly as (1)'s. **Not decoded, so not a step yet**: R3b-5 (the piles: a clip first, CMP-4; re-fly 1-30
(R4a-5), 2-13 and 2-05 after it), WT-10 (2-29's stone clock: unexplained, CMP-18, traced after CMP-2; re-fly 2-29 from tap 320, R4a-6) and STK-8 (the owner's ruling, and
1-24's door with it, R4a-4). (3) **The rig checks and the owner's calls the groups name**: rig clips (`docs/parity.md` sections 1-2): the arrival at 1-30's pile (CMP-4),
3-05 for 10 s without moving and 3-22 for 14 s (CMP-5), the static portals' ring brightness against the 58 footage frames (CMP-13), R3c-11's list and R4a-2's zero-g
wall-slide; one experiment on the port alone: 2-17 with the lightmap off and at full resolution (CMP-14); and the owner's calls: the halo scale (CMP-8: one global scale,
per-entity scales, or leave; CMP-12 follows it), the engine's friction and restitution steps (CMP-11: G10-14, WT-1), R24 (CMP-17), whether the HUD / world clock offset is
modelled (CMP-low a) and the engine's contact margin (CAP-2, CMP-low b: it would move every route's rest heights). The edge rows and slivers (PX-2 to PX-4) and the pan
and tremble against footage (PX-3) were not reached by the comparison and stay the next measuring pass's, which re-takes the comparison on the fixed build against
`CMP/port_f420` and `CMP/pairs`, at f420 and at the level's own HUD tick or f450 where a mover or the spiral is in frame (CMP-R1); the section-5 box ticks only when the
structural groups are fixed. (4) **The ship preparation**: the grant in `docs/permission.md` (still its "INCOMPLETE - PASTE THE ACTUAL GRANT HERE" placeholder, so the
record of the scope is incomplete: the owner's to fill), the play build (`tools\parity\play_build.bat` exists; no play build has been made) and the owner's calls, listed
here once: 2-30's creep (R3-2), the engine's friction and restitution steps (G10-14, WT-1), G10-2's matched 60 Hz, R24 and R25, `crate_low_emissive`'s guessed 0.08
(R3b-3), 2-32 ahead of the dragon's camera (R3-3), STK-8, PIC-1's smoothing above 80 Hz, the halo scale (CMP-8) and the standing ones in "The owner's calls" below; and
**the repository's going public, which still needs the owner's explicit word**. **With the wave**: R3b-6 and R3c-9 (`win_by` of the 85 older routes tightened when those
files are next touched), 3-20's knife-edge (R3-4), the comment-only fixes that wait for their file's next real edit (the two comments of `game/sim/Resting.hpp` among
them: a widely included header) and WAKE-7's tests, with R4a-10's one check.


**Wave 4c-1 (the comparison's five drawing groups, CMP-1, CMP-3, CMP-7, CMP-9 and CMP-10; 2026-10-11 19:14 to 2026-10-12 12:39; closed 2026-10-12, NOT PUSHED).** Five
drawing-only steps, one per decoded group of the final comparison, each independently verified, then three rounds of an independent verification of the whole wave and a
finalize that did not push; on the owner's standing order of 2026-10-10. 19:14 is `out/parity/program/CMP1/start.txt`, the first record of the run (not a measured start); 12:39 is
the finalize report's stamp. **`origin/main` was `16b78e1` when the wave closed: the wave was seven local commits on `main` (`16b78e1..753e7d0`: 20 files, +3,523 / -65) and the
close's own, and it was not pushed then, because the wave's own gate 4 (the sweep chain) failed as worded on one level, 4-12, in all three verification rounds. The finalize role
had no authority to rule on it and did not push, and neither had this close. The orchestrator ruled afterwards (R4c1-1, option (a): CMP-9's scope is 42 levels) and pushed the
wave as built, a fast-forward from `16b78e1`, with one docs commit that records the ruling.** The engine pin stayed `710bffc`; no converter, route, collision, physics, level or `data/` file changed, so
`out/levels`, `out/assets` and `out/data` are the ones the wave started from.

| Step | Row | Commit | Outcome | f420 sweep changes | GAMEPLAY | Routes win |
|---|---|---|---|---|---|---|
| 166 | CMP-1 | `baaa651` | the script's emissive override: `ignore_emissive` and `door_emissive` are read from the level's properties and the chapter's world, and on the 78 flag levels the exit door is drawn at `door_emissive` (0 when absent) and the player at 0 (0.2 in chapter 3); the readers' boxes, mean abs luminance error, player 43.8 -> 0.7 and arch 31.6 -> 0.7; `compare.py` against h6.3 fell on 77 of 78 levels (4.64 -> 3.43), rose on none; verified once (9 gates, 4 low); closes G1a-11 | 77 (all inside the 78; 2-32's player is dead before f420 and no arch is in frame) | none | 128 of 128 |
| 167 | CMP-3 | `ca93e40` | pictures at one depth are drawn in the original's bucket order (`Sprites::Rank`: the later 128-unit bucket, the lower row then the higher column, over the earlier); all 9 group levels read the original's order (1-20 57.67 -> 0.77, 4-16 49.75 -> 1.03, 4-11 11.09 -> 3.35 mean abs to h6.3); verified once (9 gates, 7 low); the gate's "only the nine" is NOT met as written: 40 other levels change too, each explained (33 toward the stills, 5 flat, 2 slightly away); the in-bucket move-to-front is not built (the stills contradict it, CMP3-1) | 49 (the 9 and 40 more) | none | 128 of 128 |
| 168 | CMP-7 | `7ebe83b` | the red key's own `AddLight` ((1, 0, 0), range 48, 6 over the key's z, one tick behind its picture): 4-20's group box R 17.3 -> 48.4 against the original's 49.4, box mean abs 11.24 -> 1.27; committed with recorded deviations: gate 2 NOT met to the letter (the port 1 to 2 percent low in every lit box, the same size as the unlit baseline's 0.979, cause untraced); verified three times | 1 (4-20; 4-19 is byte-identical by geometry) | none | 128 of 128 |
| 169 | CMP-9 | `4216f32`, text fixes `1379eca` and `753e7d0` | the script's emissive blinks: a barrel's, a breakable wall's with `blink` (a doubled clock, period 300 ms) and a teleportable crate's, and the fire agent's pulse (1.02 -> 0.98 on 300 ms legs); the barrel's and the crate's periods fitted on footage (0.605 s and 1.210 s against the decode's 0.600 and 1.200); committed with recorded deviations: **gate 1 NOT met as first written** (10 levels outside the 32 change, the fire agent's), its text corrected to 42 levels, the scope NOT ruled (R4c1-1); verified three times | 40 (30 of the 32 and the fire agent's 10; 2-32 and 3-15 unchanged, their crate is off the frame) | none | 128 of 128 |
| 170 | CMP-10 | `5c1949b` | the help block at the script's alpha 0.76, breathing 1.13 -> 1.05 on 250 ms legs, on the 13 levels that place one (71 to 77 px against the stills' 71-75, the opaque 213-grey square gone); committed with recorded deviations: gate 2's face tone NOT met to the letter (the port 1.1 to 3.0 grey of 255 under the stills', cause on the original's side where fitted); verified three times | 9 (the 13 less the four whose block is off the f420 frame) | none | 128 of 128 |

**Verification.** Steps 166 and 167 passed their one independent verification (9 gates each; 4 and 7 low). Steps 168, 169 and 170 each took three rounds and a disposition that
committed them with recorded deviations (168 gate 2, 169 gate 1, 170 gate 2); each deviation is a row in the step's block above. The wave's own verification ran three times
(`R4c1/verify1.json` at `5c1949b`, `verify2.json` at `1379eca`, `verify3.json` at `753e7d0`) and found the same thing each time: gates 1, 2, 3, 5, 6 and 7 hold and **gate 4
fails on 4-12** (R4c1-1: a scope ruling, not a defect; ruled (a) after the close). Fix 1 (`1379eca`) and fix 2 (`753e7d0`) were text-only corrections of step 169's record (its gate 1 to 42 levels; its
"nothing got worse at f420" to its own table; the word "owner-delegated" there to "a delegated disposition"); no source, data, test, route or engine file moved after step 170's
commit, and the finalize role checked it (git only, no launch). Verify3's measurements: 128 launches of `MagicPortals.exe` `29166f75`, each with a fresh copy of the owner's
`scores.json`, 128 of 128 byte-identical to `CMP10/after/sweep`, validation active and silent in all; 20 gold-seed re-takes read against the stills (the claimed improvements are
real: CMP-1 77 of 77 levels toward the stills over their changed pixels, pixel-weighted 31.4 -> 2.2; CMP-3 45 toward, 1 flat, 3 away; CMP-9 30 toward, 2 flat, 8 away; CMP-10 8
toward, 1-02 away; CMP-7 4-20 23.1 -> 1.6), four controls byte-identical.

**The sweep chain.** Against the old baseline (`PX/after/sweep`) the five steps change **106 levels and 22 are byte-identical** (1-04, 1-06, 1-14, 1-16, 1-21, 1-26, 1-28,
1-29, 1-31, 1-32, 2-32, 4-02, 4-03, 4-05, 4-07, 4-08, 4-09, 4-14, 4-15, 4-17, 4-18, 4-19); the steps overlap, 176 level-changes in all (77 + 49 + 1 + 40 + 9). By whole-frame
mean abs to h6.3 (verify1): 93 nearer the original's still, 1 slightly away, 12 flat; by changed pixels (verify3) seven read slightly away, each recorded by its step (R4c1-3).

**Suites and checks**: 57 suites, 57 passed, **33,132 checks** (the runner's no-argument launch, run once per step and once in each of the three verifications; 32,314 at the
start of the wave: +818 = `test_mp_lighting` 548 -> 588, `test_mp_keys` 169 -> 182, `test_mp_sprites` 1,571 -> 2,061, `test_mp_layer` 3,876 -> 4,151); `test_mp_routes` 2,366 checks
unchanged, 128 of 128 on exactly the census's ticks after every step. **Reachability** 108 / 20 / 0, 0 differ, at every step and in every verification (still no destroyier
term: R4a-4). **Zero warnings** rests on each step's build logs (0 lines containing `warning`) and on the verifiers' /W4 scratch compiles (the 55 units with a changed
dependency twice, all 136 port units once; every unit rc 0, 0 warnings); `port_build.bat` was not run by a verify or close role (CMP7-6). The final build is `MagicPortals.exe` `29166f75`
(relinked once by step 170's third verifier from the committed sources, launched alone, no refusal) and `test_mp.exe` `9f5865e7`; the close launched neither.

**Smart App Control**: **5 refusals of this repository's executables** (each a pair of CodeIntegrity 3033/3077 events, counted at this close from 2026-10-11 19:00): step 167's
implementer had `test_mp.exe` refused twice (21:15:22 and :23, one loop over two suites on the runner rebuilt for the column-major mutation) and the first `MagicPortals.exe`
once (21:15:51); step 168's implementer the first `MagicPortals.exe` (23:13:18); step 170's the first `MagicPortals.exe` (06:23:25); each was deleted and relinked once and the
relinked one ran. None in steps 166 and 169, the fix rounds, the three verifications (step 170's third verifier's one relink launched at once), the finalize and this close. Other events in the
window, not this repository's: 14 pairs of `python.exe` loading scipy, pyarrow and pyproj from another project's virtual environment (`Desktop\Urban Heat Study`, 10:50 to
11:25), one `python.exe` loading scipy from the user's Roaming site-packages at 07:50:16 (unattributed), and MiKTeX's `pdftoppm.exe` (3 events, 12:40:19 to 12:48:36 at the time of counting). Wave 1: 83, wave 2: 19, wave
3a: 2, wave 3b-1: 4, wave 3b-2: 9, wave 4a: 3, wave 4b: 13. The owner's folder holds `scores.json` alone (164 bytes, md5 `40bfef3a`) before and after every role.

**The standing f420 baseline** is now `out/parity/program/CMP10/after/sweep` (128 PNGs, 1280 x 720, owner's seed; `out/parity/BASELINE.txt` names it with `program/R4b/CMP/port_f420`
and `program/R4b/CMP/pairs`, which the next wave measures its fixes against). Its caveat: it was taken on `MagicPortals.exe` `aecd9c1a` (step 170's implementer's relink, after its first link was refused), not on the
committed `29166f75`; the committed build's output equals it byte for byte on all 128 (the three verifiers' own launches, md5 for md5; verify3's list is kept as
`R4c1/verify3/sweep_md5.txt`). The close checked it by level id, 128 compared and 128 unique (PNG signature, closing IEND chunk with no NUL padding, 1280 x 720, md5 equal to
its line of `CMP10/after/capture.txt` and to verify3's list) before and after the purge. **The wave's image purge** (the owner's standing order of 2026-10-03): the 64 scratch
`--saves` folders the steps and verifiers left (4,510 `scores.json` / `achievements.json` copies, 2.3 MB, each checked to hold nothing else and to parse; nothing under
`merge_w1/sweep_saves`, `ui1/saves_gold` or `saves/`), then `purge_images.py all`: **800 images, 0.53 GB, 0 refused** (the old baseline `PX/after/sweep` and the sweeps, golds,
pairs and crops of steps 166 to 169 and the leftovers of 170 and the verifications); `out/parity` 18.743 -> 18.248 GiB, free disk 104.982 -> 105.485 GiB. Kept: the baseline
and the two comparison folders (128, 130 and 256 images, all opening), `original/`, `smoke/`, `markers/`, `saves/`, `asbc/`, the audit, every mp4 and every text, json, script and
log file. **A consequence**: the pre-wave baseline and each step's own after-sweep no longer exist, so a later veto of the fire agent's pulse (R4c1-1's option (b), not taken) would compare its new sweep with
`CMP10/after/sweep`.

**GAMEPLAY changed in wave 4c-1: none.** Each step is presentation: colours, a draw order, a light, blinks, a picture's alpha and scale; no body, shape, sensor, tick or number
the simulation reads moved, and all 128 routes win on exactly the census's ticks. R24, R25, G10-2's matched 60 Hz, 2-30's creep (R3-2), 3-13's 0.08 (R3b-3) and the engine pin
were not touched. **"Owner-delegated" in steps 168 and 170 and their rows means what step 169's fix 1 spelled out for 169: a disposition made by the commit role under the owner's
standing order of 2026-10-10, not an owner message on that row.**

**The wave's rows**: R4c1-1 to R4c1-3 (3) and the five steps' blocks CMP1-1 to CMP1-6, CMP3-1 to CMP3-6, CMP7-1 to CMP7-6, CMP9-1 to CMP9-7 and CMP10-1 to CMP10-7 (32): 35
opened, 10 of them closed or settled (CMP1-1, CMP3-5, CMP7-2, CMP7-6 by this close, CMP9-4, CMP9-7, CMP10-2, CMP10-3, CMP10-6, CMP10-7), 25 open. Older rows closed: CMP-1,
CMP-3, CMP-7, CMP-9 and CMP-10 (5 of the comparison's 20), G1a-11, and step 72's 1-07 ring deviation (with its limit); "Script-driven emissive" (section 1), Order 11.3's
deviation 9 and G1a-2 are annotated and stay open.

**What the next wave should take (wave 4c-2)** *[wave 4c-2 took (1)'s gameplay steps and (3)'s code mutations: its record is the "Wave 4c-2" paragraph that follows, and what is left is re-listed there as wave 4d's]*, in this order. **R4c1-1 is ruled** (option (a), its row: the pulse stays, CMP-9's scope is 42 levels, the wave is pushed); the wave's `CMP10/after/sweep` is the
baseline every step below measures against. Then: (1) **the groups that change gameplay,
each its own step with its gates** (the f420 sweep changes only the levels the group names, each change explained against `CMP10/after/sweep`; the group's boxes read the
original's; the group's pair images re-taken): **CMP-2** (high: the revolute joint's attach point B: the converter carries it and the hinge uses it, a change on both sides of the
`.tscn`, re-converted into a staging directory under `out/` and compared with `out/levels` before it replaces anything, `tools/converter`'s `pytest` and `ruff check` clean),
**CMP-6** (high: `minion_spawn6` in 4-29 and `minion_spawn_` in 3-09 out of `entity_roles.json`: the original has no `ETHCallback` for either), **CMP-15** (4-02's slab:
`MoveKinematic` sets a velocity the original's `SetTransform` does not), **CMP-16** (2-01's crate carries `scale` 0.75 the port ignores) and **the breakable wall's contact rows**
(`ETHBeginContactCallback_breakable_wall`, one callback: Order 11.1's first clause, a `destroyable` stone dies at the first wall it breaks (#42-60), and R3b-2's gate, a stone
must be faster than scale(1.5) (#16-33); R3b-2's own row still says the callback is not decoded and R4a-3 reads it as decoded). **The routes they move are re-flown once, after
all of them**, each against `R4/census_after_round1.md`: 2-28, 2-29, 2-31, 3-15, 3-19, 3-20, 3-22, 4-11 and 4-31 (CMP-2; 2-30 is predicted unchanged; 3-20's knife-edge, R3-4), 4-29
and a re-check of 3-09 (CMP-6), 4-02 (CMP-15), 2-01 (CMP-16), 1-19 and 1-20 (R4a-1), 1-22 (R4a-3), 3-12's and 3-14's breaks and the thrown stones of 1-24 and 1-25 (the wall).
(2) **What stays**: the rig clips (`docs/parity.md` sections 1-2): the arrival at 1-30's pile (CMP-4), 3-05 for 10 s without moving and 3-22 for 14 s (CMP-5), the static
portals' ring brightness against the 58 footage frames (CMP-13), R3c-11's list and R4a-2's zero-g wall-slide; the port-only experiment, 2-17 with the lightmap off and at full
resolution (CMP-14); and the owner's calls: the halo scale (CMP-8: one global scale, per-entity scales, or leave; CMP-12 follows it), the engine's friction and restitution steps
(CMP-11: G10-14, WT-1), R24 (CMP-17), whether the HUD / world clock offset is modelled (CMP-low a), the engine's contact margin (CAP-2, CMP-low b: it would move every route's rest
heights), and, not decoded, R3b-5 (the piles), WT-10 (CMP-18, traced after CMP-2) and STK-8. (3) **This wave's loose ends, taken when a step touches the file**: the two code
mutations of step 168 and step 167's column-major one, at the first relink (CMP7-3, CMP3-6; wave 4c-2 relinks anyway); the two clips that settle three rows, one of 1-09 or 2-14
(the wall's period and depth, CMP9-3 and CMP9-6, with the bucket clock G3-2) and one of 1-02 or 1-10 from the load frame (the block's phase and popup hold, CMP10-4); the
literal and the clear that two steps left in source (CMP1-4, CMP3-3); and the second measuring pass on the fixed build against the kept `CMP/port_f420` and `CMP/pairs`, which
reads a blink, a pulse or a bounce as a RANGE across the six stills and the port's frames around f420-f504, not as one tick (h6.3 is not exactly f420: step 165). The
section-5 box ticks only when the structural groups are fixed. (4) **The ship preparation**: the grant in `docs/permission.md` (still its "INCOMPLETE - PASTE THE ACTUAL GRANT
HERE" placeholder, so the record of the scope is incomplete: the owner's to fill), the play build (`tools\parity\play_build.bat` exists; no play build has been made), the
owner's calls listed here once (2-30's creep R3-2, G10-14 and WT-1, G10-2's matched 60 Hz, R24 and R25, `crate_low_emissive`'s guessed 0.08 R3b-3, 2-32 ahead of the dragon's
camera R3-3, STK-8, PIC-1's smoothing above 80 Hz, the halo scale CMP-8, CMP-9's pulse scope (ruled in R4c1-1; the owner may overrule it) and the standing ones in "The owner's calls" below) and **the repository's going
public, which still needs the owner's explicit word**. **With the wave**: R3b-6 and R3c-9 (`win_by` of the 85 older routes tightened when those files are next touched), 3-20's
knife-edge (R3-4), the comment-only fixes that wait for their file's next real edit (the two comments of `game/sim/Resting.hpp` among them: a widely included header) and
WAKE-7's tests, with R4a-10's one check.


**Wave 4c-2 (the comparison's four gameplay groups, the breakable wall's contact and one route phase: CMP6, CMP16, CMP15, CMP2, WALL, then the re-fly; 2026-10-12 13:11 to 2026-10-15
02:46; closed and PUSHED 2026-10-15).** A mutation check (no commit), five gameplay steps (planning steps 171-175), one route phase (176), an independent verification of the whole wave
and a finalize that pushed, on the owner's standing order of 2026-10-10. The run was **paused on 2026-10-12 at about 20:50 on the owner's order (usage limit) and resumed on 2026-10-14
on his "continue where you left off"** (CMP2's work was half built and uncommitted; WALL had not started). 13:11 is the mutation role's first Code Integrity query
(`R4c2/mutations.md`), not a measured start; 02:46 is the route-phase commit. **`origin/main` is `45f6c05`: six commits, `7e708e3..45f6c05`, a fast-forward pushed by the finalize role
(54 files; every author and committer IvanCvetanovic, no trailer, each message ending `Record: ... step N`); the engine pin stayed `710bffc`.** Every step's scope was a CENSUS of the
converted scenes and the original's raw files, written first (wave 4c-1's lesson): the hint was exact for CMP-6, CMP-16, CMP-15 and CMP-2, and WALL's census named eight levels the route hint
did not (1-09, 1-13, 2-04, 2-29, 3-10, 3-13, 3-18, 4-21).

| Step | Row | Commit | Outcome | f420 sweep changes | GAMEPLAY | Routes |
|---|---|---|---|---|---|---|
| 171 | CMP-6 | `9797699` | the two markers the script never runs (`minion_spawn_`, `minion_spawn6`) spawn no knight: `enemy_spawn.names` is `["minion_spawn"]`, 60 knights from 60 markers (62 before); 4-29's box mean abs to the stills 18.0 / 18.5 / 18.4 -> 1.1 / 1.7 / 1.5 (h0.3 / h2.8 / h6.3), dark pixels 7,383 -> 380 (the original's 380); verified once (11 gates, 1 medium, the route, 6 low) | 2 (3-09: 80 px, the shared effects stream; 4-29: 7,779 px) | **YES**: 3-09 two knights, not three (the guard's death on tick 58 gone, achievement 31 now counts 2 from tick 1); 4-29 one, not two | 4-29 unwon until 176 |
| 172 | CMP-16 | `9d643c9` | a node's `scale` datum scales its picture, body and mass: 2-01's crate 58 -> 43.5 units, mass 0.5382 -> 0.3028 (5.17 -> 2.91 players), 180 -> 135 px wide (the original's 135); mean abs h6.3 **4.20 -> 3.31** (corrected, see CMP-16); verified once (10 gates; gates 2 and 3 were taken by the verifier after three refused links) | 1 (2-01: 27,328 px; the 52 antiportal levels byte-identical) | **YES**: 2-01's crate | 2-01 unwon until 176 |
| 173 | CMP-15 | `12ac5d5` | a bounce slab is placed (`PlaceKinematic`), not moved with a velocity: the slab's velocity 60 px/s on tick 1 -> 0 on every tick; 4-02's astronaut eye 364.5 (15.5 px high) -> 380.4 at f420 against the original's 380.06; committed with recorded deviations: **the gate's f75-f135 frames NOT met** (the start offset, 383.10 at f75 before and after: CMP15-1); three verifications, none a pass (2 medium each) | 1 (4-02: 10,321 px) | **YES**: 4-02's astronaut is shoved, no longer carried | 4-07 unwon until 176 |
| 174 | CMP-2 | `cad4a2f` | a revolute joint has two ends: the converter carries attach point B (`joint_other`, `joint_anchor_b_x/y`; 10 of 128 `out/levels` files, 3 lines each) and the hinged body stands where its joint holds it: 10 joints in 10 levels, 8 move, 2-31's seesaw 31.8 px; the plank against the stills (-14.22, +28.44) -> (0, 0), 2-31 mean abs 5.51 -> 2.56; converter `pytest` 121 passed (9 new), `ruff` clean; verified once (11 gates, 1 medium: the route clause) | 7 (2-28, 2-29, **2-30: one pixel, cause inferred**, 2-31, 3-20, 4-11, 4-31) | **YES**: ten levels' hinged bodies | 2-31, 3-20 unwon; 3-19 on 827 |
| 175 | WALL | `ab18b1b` | the breakable wall's contact as the original has it: a stone breaks a wall only if it BEGINS to touch it above 75 u/s (1.5 m/s, the whole velocity, strictly), judged at up to two instants with the larger reading (fix 2), and a `destroyable` stone dies at the first wall it breaks; 15 census levels; three verifications, none a pass (the second found 3-13's wall holding at 1.333 m/s, fixed by a second look), committed with 11 recorded deviations | **0** (no stone touches a wall in 420 ticks: the sweep cannot show the rule, WALL-11) | **YES**: 15 levels | 8 unwon until 176 |
| 176 | the route phase | `45f6c05` | 13 routes re-flown and a 14th tightened by 14 authors side by side, one round; 128 of 128 won; verifier pass (9 gates, 2 medium, 5 low); **achievement 48 no longer demonstrated** (R4c2-1) | 0 (128 of 128 byte-identical to WALL's) | none | **128 of 128** |

**Verification.** Steps 171, 172 and 174 passed their one verification; 173 and 175 took three each and none passed (CMP15: gate 3's f75-f135 frames and gate 6's route clause; WALL: the
route clause, the unmeasured speeds and 3-13's inference, after the second round's finding was fixed), both committed with recorded deviations by the standing order's delegation, no gate's
text rewritten to read as met. **The route clause ("every route outside the census wins on EXACTLY its tick") was NOT MET literally at the commit of steps 173, 174 and 175** (and step 172's
cell that said "met" is corrected to the same words); the route phase met it on the final tree (planning step 176, deviation 5). The route phase's independent verifier (`R4c2/verify1.json`,
`pass: true`, 9 gates) re-derived everything it could: its own launch of `test_mp.exe` with no argument, all 128 routes replayed twice with identical win lines, 14 + 1 routes read with
`--trace 1`, a fresh re-conversion of all four worlds byte-identical to `out/levels`, 128 launches of `MagicPortals.exe` `f27dde1a` byte-identical to `WALL/after/sweep` and exactly 11 levels
changed against `CMP10/after/sweep`, and gold-seed re-takes of six census levels and one control read against the stills (4-29 1.31 -> 0.85, 2-01 4.20 -> 3.31, 4-02 7.51 -> 7.17, 2-31 5.51 -> 2.56, 3-20 4.26
-> 3.88; 3-09 and the 3-13 control unchanged; WALL claims no f420 change).

**The mutation check** (`R4c2/mutations.md`, 2026-10-12, no commit): wave 4c-1's three unrun code mutations (R4c1-3 (b)) applied together to ONE runner build (`81127657`) and launched once each,
0 Smart App Control refusals: A (the key's light not a tick behind) and B (a particle share on it) are each caught by `test_mp_layer` by a pin of their own, C (the bucket order column-major)
by `test_mp_sprites` and not by `test_mp_layer`, whose order test cannot tell the axes apart (CMP6-9, with the three pairs that can). **No mutation survived the suite that owns its rule.**
CMP7-3, CMP3-6 and R4c1-3 (b) are settled on it (R4c2-13).

**The route phase (step 176).** At its start 115 of 128 won and 13 did not (4-29; 2-01; 4-07; 2-31 and 3-20; 1-19, 1-20, 1-22, 2-29, 3-10, 3-12, 3-14 and 4-21; 3-19 won 5 ticks early); one round of fourteen
authors later **128 of 128 win**: **117 on exactly their census tick** (114 untouched files and 3 rewritten on the same tick: 3-20 555, 4-07 446, 4-29 1,023), **11 on another** (8 earlier: 1-19 271 -> 184,
2-01 444 -> 307, 3-10 1,017 -> 943, 3-14 1,141 -> 1,087, 4-21 370 -> 360, 2-29 569 -> 564, 3-19 832 -> 827, 3-12 639 -> 636; 3 later: 1-20 596 -> 825, 1-22 535 -> 666, 2-31 405 -> 446), 0 late, 0 unwon. 14 files
changed in `tests/routes/` (+384 / -286), `win_by` 12 to 15 over each; by cause CMP6 1, CMP16 1, CMP15 1, CMP2 3, WALL 8. Shots changed on 1-19 (6 -> 4), 1-20 (8 -> 10), 3-10 (2 -> 4) and 4-29 (8 -> 6); medals at the door
on 3-12 (2 -> 3), 3-10 (2 -> 3) and 2-31 (2 -> 1). **No level is sealed by the wall's rule**, but 1-20, 1-22, 3-10, 3-12 and 4-21 are won by a different idea than the old route's and three of them (1-22, 4-21, and 3-13 untouched)
stand on something not measured on the original (R4c2-2 to R4c2-4); **1-20's achievement 48 is not demonstrated by any route** (R4c2-1). The four stealth achievements (26, 28, 32, 42) are unchanged.

**Suites and checks**: 57 suites, 57 passed, **33,730 checks** (the verifier's launch of `test_mp.exe` `2c74ad04` with no argument; 33,132 at the start of the wave: +598 = `test_mp_demolish` 178 -> 424 (+246), `test_mp_minions` 294 -> 448 (+154),
`test_mp_hinge` 28 -> 102 (+74), `test_mp_timebase` 445 -> 495 (+50), `test_mp_layer` 4,151 -> 4,181 (+30), `test_mp_levels` 129 -> 150 (+21), `test_mp_bounce` 189 -> 203 (+14), `test_mp_launchers` 105 -> 115 (+10), `test_mp_routes`
2,366 -> 2,365 (-1: the unlock check of achievement 48)). **Reachability** 108 / 20 / 0, 0 differ, at every step and in the verifier's run (no speed term for step 175: WALL-4). **Zero warnings** rests on each step's build logs and the verifiers'
scratch compiles; **no full `port_build.bat` of this tree ran in the wave** (R4c2-5, WALL-7). The final pair is `MagicPortals.exe` `f27dde1a` (the route phase's verifier's one relink from unchanged sources, ran first time) and `test_mp.exe` `2c74ad04`;
this close launched neither.

**Smart App Control**: **10 refusals of this repository's executables** (each a pair of CodeIntegrity 3033/3077 events, counted at this close from 2026-10-12 13:00): step 172's implementer 5 (`test_mp.exe` 16:01:53 and the mutated one
16:10:02, `MagicPortals.exe` 16:02:42, 16:02:54 and 16:03:21 on 12 October: three links of the game each refused on its first launch, the last one ran when launched again at 16:29 with no relink), step 173's 1 (`MagicPortals.exe` 17:21:55),
step 174's first role 2 (`MagicPortals.exe` 20:31:43 and 20:31:55, the relink at 20:32:03 ran) and step 175's 2 (`test_mp.exe` 21:44:07 on 14 October and 00:22:53 on 15 October, each relinked once). None in the mutation check, step 171, step 174's resumed role,
the verifiers, the route phase, the finalize and this close. Other events in the window, not this repository's: MiKTeX's `pdftoppm.exe` (19) and `pdftotext.exe` (8) on 13 October, 10:17 to 11:08, 12 events of `python.exe` (10) and `powershell.exe` (2) loading from another
project's scratchpad (`Urban Heat Study`, 13 October 10:21 to 10:36) and one of an Office process (`aimgr.exe`, 14 October 10:24). Wave 1: 83, wave 2: 19, 3a: 2, 3b-1: 4, 3b-2: 9, 4a: 3, 4b: 13, 4c-1: 5. The owner's folder holds `scores.json` alone (164 bytes,
md5 `40bfef3a`) before and after every role, and at this close.

**The standing f420 baseline** is now `out/parity/program/WALL/after/sweep` (128 PNGs, 1280 x 720, owner's seed; `out/parity/BASELINE.txt` names it with `program/R4b/CMP/port_f420` and `program/R4b/CMP/pairs`, which the next wave measures its fixes against).
Its caveat: it was taken on `MagicPortals.exe` `357fae09` (step 175's fix 2), not on the committed `f27dde1a`; the route phase's verifier's 128 launches on the committed build equal it byte for byte (`R4c2/verify1/sweep_md5.txt`). The close checked it by level id (128 compared, PNG
signature, closing IEND chunk with no NUL padding, 1280 x 720, md5 equal to that list) before and after the purge. **The wave's image purge** (the owner's standing order of 2026-10-03): the 87 scratch `--saves` folders the steps and verifiers left (2,735 `scores.json` /
`achievements.json` copies, 0.6 MB, each checked to hold nothing else and to parse; nothing under `merge_w1/sweep_saves`, `ui1/saves_gold` or `saves/`; the other 299 saves folders of earlier waves were not touched) and step 174's `levels_before` backup (128 `.tscn`, 2.4 MB),
then `purge_images.py all`: **835 images, 0.60 GB, 0 refused** (the old baseline `CMP10/after/sweep` 128, and the sweeps, golds, pairs and crops of CMP6 147, CMP16 130, CMP15 270, CMP2 134, WALL 18 and R4c2 8); free disk by `df` 100.55 -> 101.12 GiB. Kept: the baseline
and the two comparison folders (128, 130 and 256 images by a recursive count, all opening), `original/`, `smoke/`, `markers/`, `saves/`, `asbc/`, the audit, every mp4 and every text, json, script and log file. **A consequence**: the pre-wave baseline (`CMP10/after/sweep`) and each
step's own after-sweep no longer exist, so a later veto of the fire agent's pulse (R4c1-1) would compare its new sweep with `WALL/after/sweep`, which already carries this wave.

**GAMEPLAY changed in wave 4c-2: YES, in steps 171 to 175, and none in step 176.** 3-09 and 4-29 lose a knight each (171); 2-01's crate is 0.75 of its size and 0.5625 of its mass (172); 4-02's astronaut stands where the slab shoves him (173); ten levels' hinged bodies stand where their
joints hold them, 2-31's seesaw 31.8 px away (174); a stone breaks a breakable wall only above 75 u/s at its first touch and a `destroyable` stone dies with the first wall it breaks, on 15 levels (175). The routes were re-flown for all of it (176). R24, R25, G10-2's matched 60 Hz, 2-30's creep (R3-2: it still wins on 1,386),
3-13's 0.08 friction (R3b-3: it still wins on 1,043) and the engine pin were not touched. "Committed with recorded deviations" in steps 173 and 175 means what wave 4c-1's close spelled out: a disposition made by the commit role under the owner's standing order of 2026-10-10, not an owner message on that row.

**The wave's rows**: the five steps' blocks CMP6-1 to CMP6-9, CMP16-1 to CMP16-7, CMP15-1 to CMP15-6, CMP2-1 to CMP2-7 and WALL-1 to WALL-11 (40) and R4c2-1 to R4c2-13 (13): 53 opened, 24 closed or settled by this close (CMP6-1, -2, -6, -7; CMP16-1, -3, -4, -7; CMP15-2, -4, -6; CMP2-1, -4, -5; WALL-1, -2, -4, -8, -9, -11; R4c2-10 to R4c2-13), 29 open. Older rows closed: CMP-2, CMP-6,
CMP-15 and CMP-16 (4 of the comparison's 20; with wave 4c-1's five, 9), R4a-1, R4a-3 and R3b-2 (by the re-flown routes), CMP7-3 and CMP3-6 (by the mutations); Order 11.1 (first clause built and re-flown, the row stays open for its others), STK9-2 (narrowed), CMP-18 (CMP-2 is not the cause) and R4c1-3 (b) are annotated.

**What the next wave should take (wave 4d)** *[wave 4d-1 took (1)'s re-taken comparison: its record is the "Wave 4d-1" paragraph that follows, and what is left is re-listed there as wave 4d-2's]*, in this order. (1) **The final comparison RE-TAKEN on the fixed build** (a read-only measuring pass, wave 4b's over again, from `tools/parity` and `out/parity/program/wave_scripts/gen4b.py`): which of the 58 levels that differed still differ, and with which CMP groups; against the kept
`R4b/CMP/port_f420` and `R4b/CMP/pairs` (the "before"), at f420 and at the level's own HUD tick or f450 where a mover or the spiral is in frame (CMP-R1), reading a blink, a pulse or a bounce as a RANGE across the six stills and the port's frames around f420-f504 (h6.3 is not exactly f420: step 165); the capture seed is the ALL-GOLD one (`out/parity/ui1/saves_gold/scores.json`,
md5 `09268640`; 1-02 and 1-03 need `--tap 1178,358@300-305`) and the standing sweep stays on the owner's seed. The section-5 box ticks only when the structural groups are fixed: CMP-4, CMP-5, CMP-11, CMP-17 and CMP-18 are not. (2) **What stays**: the rig clips (`docs/parity.md` sections 1-2): the arrival at 1-30's pile (CMP-4), 3-05 for 10 s without moving and 3-22 for 14 s (CMP-5), the static portals' ring
brightness against the 58 footage frames (CMP-13), R3c-11's list and R4a-2's zero-g wall-slide, **and now one clip of a stone rolling and falling into a breakable wall (WALL-3, WALL-5, R4c2-2) and footage of 1-20 (R4c2-1)**; the port-only experiment (CMP-14: 2-17 with the lightmap off and at full resolution); **CMP-18 / WT-10** (2-29's stone 125-135 px behind: CMP-2 is not the cause, so the trace of the stone against the plank and the hinge is the next step on it,
then re-fly 2-29 from tap 320, R4a-6); and the owner's calls: the halo scale (CMP-8: one global scale, per-entity scales, or leave; CMP-12 follows it), the engine's friction and restitution steps (CMP-11: G10-14, WT-1; 1-22's coast re-times with them: R4c2-2), R24 (CMP-17), whether the HUD / world clock offset is modelled (CMP-low a), the engine's contact margin (CAP-2, CMP-low b; CMP16-5's 2 px), 2-30's creep (R3-2), STK-8, G10-2's matched 60 Hz (4-21's aim band moves with it:
R4c2-3), R25, `crate_low_emissive`'s guessed 0.08 (R3b-3), 2-32 ahead of the dragon's camera (R3-3), PIC-1's smoothing above 80 Hz and CMP-9's pulse scope (R4c1-1). (3) **Loose ends, taken when a step touches the file**: **a full `port_build.bat` with zero warnings** (R4c2-5, WALL-7); the three discriminating pairs for `test_mp_layer`'s order test (CMP6-9); the mutation witnesses nobody ran (CMP6-8, CMP2-2, CMP15-3, CMP16-2, WALL-6);
`kDestroyableKey` and the `== 1.0` literal into `demolish.json` (WALL-10); `Hinged::placedPx` / `heldPx` (CMP2-3) and the converter's datum-type pin (CMP2-7); G3-4's `PlaceKinematic` for platforms, lifts and doors (CMP15-5); `win_by` of the 114 untouched routes (R4c2-9, R3b-6, R3c-9); 3-20's knife-edge (R3-4); the comment-only fixes that wait for their file's next real edit (the two comments of `game/sim/Resting.hpp` among them: a widely included header), WAKE-7's tests with R4a-10's one check,
the literal and the clear that two steps left in source (CMP1-4, CMP3-3), and the two clips that settle three rows (1-09 or 2-14 for the wall's period and depth, CMP9-3 and CMP9-6; 1-02 or 1-10 from the load frame for the block's phase, CMP10-4). (4) **The ship preparation**: the grant in `docs/permission.md` (still its "INCOMPLETE - PASTE THE ACTUAL GRANT HERE" placeholder, so the record of the scope is incomplete: the owner's to fill), the play build
(`tools\parity\play_build.bat` exists; no play build has been made), the README and LICENSE, the release zip, and **the repository's going public, which still needs the owner's explicit word**. For the next wave's gates: future route gates compare against `R4c2/census_after_round1.md` (128 won; 117 exact, 11 on another tick), and a step's scope is derived by a census of `out/levels` and the original's files, never typed.

**Wave 4d-1 (the final comparison re-taken on the fixed build: a read-only measuring pass; 2026-10-15 11:08 to 13:31; closed 2026-10-15; the close's two docs files are its only commit).** One
capture role, sixteen readers and an audit, a triage and an independent verification, on the owner's order of 2026-10-15 ("continue with the next step"). It edited no
source, data, test or route file, relinked nothing, launched no test runner and committed nothing: `HEAD` stayed `fe37df6`, `git status` was clean before and after, the
engine pin is `710bffc`. 11:08 is the capture role's first query (`R4d/CMP/start_time.txt`), 13:31 the verifier's report; the evidence is `out/parity/program/R4d/CMP/`
(`pairing.md`, `old_vs_new.md`, `per_level.md`, `residuals.md`, `groups.json`, `audit.json`, `numbers.json`, the 16 chunks) and `out/parity/program/R4d/verify1.json`, and
the close's own report is `R4d/close.md`. The build is the committed one of step 176: `MagicPortals.exe` `f27dde1a` (checked by md5 at the start and the end of the sweep)
and `test_mp.exe` `2c74ad04` (not launched). **The pass repeats wave 4b's**: the same pairings (the ticks, the sets and the save rule were not re-derived: h6.3 against
f420, with f450, f75, f135, each level's own HUD tick and f240 as extra sets), the same all-gold seed (`09268640`), the same one rule for the verdict. 134 launches of
`MagicPortals.exe` (129 for the 128-level sweep, 1-12 twice; 2 popup runs; 3 runs of the owner's seed at f90), **0 Smart App Control refusals**, validation active and
silent in all 128 sweep logs. All 128 f420 frames are byte-identical to `WALL/after/sweep` (126 as taken; 1-02 and 1-03 are taken with the popup closed and their popup-up
frames equal the sweep), so the committed exe renders the baseline.

**What it found: 61 `match`, 65 `minor`, 2 `differs`** of 128, against 26 / 44 / 58 (the readers' own labels read 58 / 68 / 2; the rule re-grades 3-17, 3-19 and 3-23 to
`match`, every residual of them low, as the old pass did for its five). By the old verdict:

| old \ new | match | minor | differs | total |
|---|---:|---:|---:|---:|
| match | 25 | 1 | 0 | 26 |
| minor | 14 | 30 | 0 | 44 |
| differs | 22 | 34 | 2 | 58 |
| total | 61 | 65 | 2 | 128 |

Better 70 levels, the same 57, worse 1. **Of the 58 levels that differed, 22 now match, 34 are minor and 2 still differ: 1-30 (CMP-4, the pile) and 3-05 (CMP-5, knight
A).** The 52 that differed by CMP-1 alone read 19 `match` and 33 `minor`; of the six with their own cause, 1-30 and 3-05 still differ, 2-31 reads `minor` and 4-20, 4-29
and 4-31 `match`. The 34 that are minor keep only medium causes: CMP-14 on 10 (chapter 2's lightmapped levels), CMP-19 on 9, CMP-8 on 9, CMP-20 on 3, CMP-4 on 2, CMP-11
on 2, CMP-18 on 1, CMP-17 on 1 (a level can hold two); 30 of the 65 minor levels are minor by CMP-8 alone. By chapter (match / minor / differs, old -> new): chapter 1 11
/ 20 / 1 -> 13 / 18 / 1, chapter 2 0 / 0 / 32 -> 7 / 25 / 0, chapter 3 0 / 19 / 13 -> 9 / 22 / 1, chapter 4 15 / 5 / 12 -> 32 / 0 / 0. 490 residuals (2 high, 74 medium,
414 low; the old pass's 468 were 82, 125 and 261), every high and medium in a group. **No level shows a lit player or exit arch any more.** The section-5 box is not
ticked: CMP-4, CMP-5, CMP-11, CMP-17 and CMP-18, the structural groups, are all unchanged.

**No regression caused by the build was found.** The one level whose verdict got worse is 1-15 (`match` -> `minor`), a re-grade: its portal box holds 0 changed pixels
against the old frame and the pair images, old and new, show the pink disc against the original's grey-white iris plainly, so the old `match` was an under-grade (R4d1-1,
CMP-13). The one level whose f420 `mean_abs` rose is 1-22 (4.73 -> 4.77, a blink at its dim end, CMP9-6), and 22 levels show a medium the old reading did not list at
medium (23 residuals: CMP-19 on 9, CMP-14 on 7, CMP-8 on 3, CMP-20 on 3, CMP-13 on 1), every one in a box that holds the old frame's pixels or only the player, arch and
blink pixels that steps 166 and 169 changed (R4d1-2). **The f420 pixels against the old pass: 107 levels differ and 21 are byte-for-byte equal, exactly the union of the
levels the record lists as changed by steps 166-175 and exactly the levels it says nothing changed** (none differs that the record does not list, none is equal that it
does). `compare.py`'s ranking (not proof): the `mean_abs` medians fell, h6.3 / f420 4.645 -> 3.62, f450 4.49 -> 3.435, h0.3 / f75 6.805 -> 6.18, f135 6.105 -> 5.265, the
level's own HUD tick 4.485 -> 3.34 and h2.8 / f240 4.665 -> 3.885, on 97, 99, 96, 101, 100 and 99 levels (higher on 1, 3, 5, 3, 2 and 3); the largest falls at f420 are
2-01 7.04 -> 3.31, 2-31 5.86 -> 2.56, 2-28 8.47 -> 5.36, 2-14 5.32 -> 2.39, 2-06 6.02 -> 3.13, 2-23 5.42 -> 2.57 and 2-13 9.30 -> 6.50; the top five now are 2-32 14.97
(the game-over timing, unchanged), 4-18 11.38, 4-12 10.30, 4-10 8.45 and 4-15 8.32.

**How far to trust it.** (a) The audit (16 levels, one per chunk) found 15 agree and **one miss, 2-28's fireball halo, medium** (the first audit: 16 of 16), and 0
residuals not real; the second reading of chunk 8 (2-25 to 2-32), taken after the audit, found a medium the first reading missed on 5 of its 8 levels (CMP-14 on 2-26,
2-27, 2-30 and 2-31, the fireball on 2-28), and a chunk chosen because of the audit's miss makes that an upper bound; the other 15 chunks were not re-read and the audit's
sample holds no lightmapped chapter-2 level. So **61 / 65 / 2 are the rule's counts on the readers' verdicts and a lower bound on medium residuals**; the misses are
likely of two classes (lit-stone softness, a light effect on a moving object) on the chunks not re-read (R4d1-4). On the 21 levels no step touched, the two readings agree
on the verdict on 20 of 20 (the 21st, 2-32, is a fix seen at another tick), so the differences elsewhere are the build or a sharper look, not reader drift. (b) The
pairing is as wave 4b left it: the script-clock fit (four chapter 1-3 platform levels) and the two-clock reading were NOT re-run, h6.3 is not exactly f420, and a blink, a
pulse or a mover reads as a range (CMP-R1). (c) The independent verification (`R4d/verify1.json`: 10 gates, pass, 4 low) re-derived the counts and the matrix from the
chunks, checked the 128 frames against the sweep by md5, re-ran `compare.py` on three levels and read 19 of the 128 pair images (4-20, 1-13, 1-30, 3-05, 2-28, 4-12, 1-15,
1-22, 2-01, 2-31, 4-29, 1-05, 2-29, 2-32, 4-18, 3-19, 4-31, 3-28, 3-09): the claims it read held; the other readers' claims were checked structurally, not re-read. Its lows: the
counts quoted without the lower-bound sentence (R4d1-4); the grading of one light cause not uniform across readers; three stills' HUD reads moved by an untraced cause
(R4d1-3); two of the group status checks cannot show the fix on the group's first listed level (the verifier used 2-31 and 4-29).

The groups after the re-take (21: 8 fixed, 1 partly, 9 open and unchanged plus CMP-low, 2 new; one row each in the block before "Blockers", each annotated; `movesRoutes`
= the fix changes a physics or sim number a winning route leans on, so GAMEPLAY; the fixed rows show their old reach):

| Id | Levels | Kind | Fixability | Moves routes | Status | Title |
|---|---|---|---|---|---|---|
| CMP-4 | 4 | structural | needs-rig | yes | unchanged | piles at rest in the original topple or creep in the port (1-30 high; 2-13, 2-05; 3-12 low) |
| CMP-5 | 2 | structural | needs-rig | yes | unchanged | 3-05's knight A absent for 8 s in the original; 3-22's knight leaves its plank early |
| CMP-8 | 57 | light | owner-call | no | unchanged | lights and halos read weaker (`halo_brightness_scale` 0.6 a `_guess`; 50 levels before, 37 medium) |
| CMP-14 | 12 | light | unknown | no | unchanged | lit stone reads softer on lightmapped levels (all 10 of chapter 2 read it; 3 before) |
| CMP-19 | 10 | light | unknown | no | **new** | the carranca fireball's glow reads weaker beyond its core (2-01 to 2-08, 2-28, 2-32) |
| CMP-11 | 5 | structural | owner-call | yes | unchanged | crates and barrels creep or rest elsewhere (friction, restitution: G10-14, WT-1) |
| CMP-17 | 5 | structural | owner-call | yes | unchanged | knights rest off the original's rest: 3-08 30 px; 4-30, 4-28, 3-02, 3-20 6-12 px (R24) |
| CMP-12 | 4 | particles | unknown | no | unchanged | the blue lamp's orb smaller and bluer, its bracket dark |
| CMP-13 | 4 | particles | needs-rig | no | unchanged | static portals' rings cover the original's grey spoked iris (1-15 raised to medium) |
| CMP-20 | 3 | particles | decoded | no | **new** | a burned crate draws no flame and no fade (`fire32`, 1000 ms) |
| CMP-18 | 1 | structural | unknown | yes | unchanged | 2-29's stone 222 px behind at f420 (WT-10) |
| CMP-low | 128 | phase | unknown | if CAP-2 | unchanged | clock and phase, the 2 px rest skin, opening camera, bob centre, tone: no step |
| CMP-9 | 11 | light | needs-rig | no | partly | script blinks built; low residuals left (walls at the dim end at f420, static crates' clock) |
| CMP-1 | 0 (was 78) | light | decoded | no | fixed, 166 | the player and the exit arch drawn lit |
| CMP-2 | 0 (was 10) | structural | decoded | yes | fixed, 174 | the revolute joint's attach point B |
| CMP-3 | 0 (was 9) | structural | decoded | no | fixed, 167 | equal-z pictures in the 128-unit bucket order |
| CMP-6 | 0 (was 2) | structural | decoded | yes | fixed, 171 | the two markers that spawn no knight |
| CMP-7 | 0 (was 2) | light | decoded | no | fixed, 168 | the red key's own light |
| CMP-10 | 0 (was 13) | structural | decoded | no | fixed, 170 | the help block at alpha 0.76 breathing 1.05-1.13x |
| CMP-15 | 0 (was 1) | structural | decoded | yes | fixed, 173 | 4-02's bounce slab placed, not moved with a velocity |
| CMP-16 | 0 (was 1) | structural | decoded | yes | fixed, 172 | 2-01's crate carries scale 0.75 |

**Suites and checks**: none launched (the pass is read-only and the close launched nothing); the committed numbers of wave 4c-2 stand: 57 suites, 33,730 checks,
`test_mp_routes` 128 of 128 on 2,365 checks, reachability 108 / 20 / 0. **Zero warnings** rests on wave 4c-2's records (no build ran). **Smart App Control**: **0
refusals** in the wave (CodeIntegrity 3033/3077 queried from 11:00 by the verifier and again by this close: none; the log is readable, its last pair is 2026-10-15 00:22,
wave 4c-2's last refusal). Earlier waves: 1: 83, 2: 19, 3a: 2, 3b-1: 4, 3b-2: 9, 4a: 3, 4b: 13, 4c-1: 5, 4c-2: 10. The owner's folder holds `scores.json` alone (164
bytes, md5 `40bfef3a`) before and after every role and at this close; every launch of the pass had its own `--saves` folder holding a copy of its seed, and none is left.
The wave ran beside a read-only design workflow for the owner's modernised controls (item (4) below): it wrote only under `out/parity/program/CTL/`, touched no tracked
file and nothing under `build/`.

**The standing f420 baselines** are now `out/parity/program/WALL/after/sweep` (128 PNGs, 1280 x 720, the owner's seed; its caveat, that it was taken on `357fae09`, is
closed by this wave: the committed exe `f27dde1a` renders all 128 frames byte for byte), and the re-take's `R4d/CMP/port_f420` (130 PNGs: the 128 frames, 1-02 and 1-03
with the popup closed, and their popup-up frames in `popup_up/`) and `R4d/CMP/pairs` (256: 128 pairs of 640 x 720 and 128 overlays); `out/parity/BASELINE.txt` names
exactly those three. **The next measurements (the controls step, then the fixes) compare against this re-take**, as this one compared against `R4b/CMP`. **The wave's
image purge** (the owner's standing order of 2026-10-03): the re-take's scratch `R4d/CMP/pairs_other` (1,280), `port_other` (616) and the earlier pass's
`R4b/CMP/port_f420` and `pairs` (386), **2,282 images, 1.491 GB (1,490,599,825 bytes); free disk 99.07 -> 100.48 GiB**. Before and after, the 514 kept images (the
baseline, `port_f420`, `pairs`) have the same md5 and all open at their size with the IEND chunk and no NUL padding. The capture role had already deleted its own
`--saves` folders (no `scores.json` or `achievements.json` newer than 10:50 is left anywhere under `out/`) and its frame dumps. **One image was left on purpose**:
`program/CTL/CTL1/sheet.png` (35 KB), the contact sheet of the modernised controls' art tool, written at 13:18 by a concurrent workflow that is not this wave's; the purge
tool's dry run still lists it, and the next purge takes it once that work has committed. The R4b `pairs_other`, `port_other` and crops were purged at wave 4b's close, so
what is quoted from the old pass's non-f420 sets here comes from its reports, not from a picture.

**GAMEPLAY changed in wave 4d-1: none.** R24, R25, G10-2's matched 60 Hz, 2-30's creep (R3-2), 3-13's 0.08 (R3b-3) and the engine pin were not touched. **The wave's
rows**: CMP-19 and CMP-20 (2 new groups) and R4d1-1 to R4d1-4 (4): 6 opened, none closed (CMP-1, -2, -3, -6, -7, -10, -15 and -16 were already ticked by their steps and
are annotated with the re-take's evidence; CMP-9 stays ticked, with its low residuals named). The CMP-4, -5, -8, -11 to -14, -17, -18, CMP-low and CMP-R1 rows are
annotated and stay open.

**What the next wave should take (wave 4d-2)**, in this order. (1) **The groups whose fixability is `decoded`: one is open, CMP-20** (a burned crate's flame and its fade
over 1000 ms: drawing only, moves no route; the gates are in its row: the f420-f504 frames of 2-22 and 2-08, a sweep that changes only levels where a crate burns by f420,
a pin in `test_mp_effects`, 3-30's 63-tick stand unchanged). The eight fixed groups need nothing (keep their pins). Beside it, the groups that need neither the rig nor an
owner's call but a measurement first: **CMP-19** (a read-only reconciliation on one pair against step 85's rows first, then `lighting.json` `fireball`), **CMP-14** (the
port-only experiment: 2-17 with the lightmap off and at full resolution) and **CMP-18 / WT-10** (the trace of the stone against the plank and the hinge; its fix would
move 2-29's route), with CMP-12 after CMP-8's call, and the cheapest low item, CMP-low (d), one bob-centre fit (about 60-70 levels, 3-8 px, drawing only). **No group of
this list moves a route except CMP-18's, and a route a fix moves is re-flown ONCE after it** (2-29 from tap 320, R4a-6), not after each step. (2) **The rig checks** that
need the emulator or a clip (`docs/parity.md` sections 1-2): the arrival at 1-30's pile (CMP-4), 3-05 for 10 s without moving and 3-22 for 14 s (CMP-5), the static
portals' rings against the 58 footage frames (CMP-13; 1-15's grey iris is the plain case), the two clips of CMP-9 (1-09 or 2-14 for the wall's period and depth, G3-2's
bucket clock for the static crates; 1-02 or 1-10 from the load frame for the help block's phase, CMP10-4), R3c-11's list, R4a-2's zero-g wall-slide, one clip of a stone
rolling and falling into a breakable wall (WALL-3, WALL-5, R4c2-2) and footage of 1-20 (R4c2-1); and the re-take's own, the pairing's clock on chapter 4 with the mover
fit re-run (CMP-R1, R4d1-3) and one more chunk of lit levels re-read before 61 is quoted outside this file (R4d1-4). (3) **The owner's calls the groups name**: the halo
scale (CMP-8: one global scale, per-entity scales or leave; this re-take's per-entity numbers favour per-entity; CMP-12 follows it and CMP-19 only if the scale is the
knob), the engine's friction and restitution steps (CMP-11: G10-14, WT-1; 1-22's coast re-times with them: R4c2-2), R24 (CMP-17: five knights, 3-08's 30 px), CMP-low (a)
whether the HUD / world clock offset is modelled and (b) the engine's contact margin (CAP-2: it would move every route's rest heights; CMP16-5's 2 px), and the standing
ones: 2-30's creep (R3-2), STK-8, G10-2's matched 60 Hz (4-21's aim band moves with it: R4c2-3), R25, `crate_low_emissive`'s guessed 0.08 (R3b-3), 2-32 ahead of the
dragon's camera (R3-3), PIC-1's smoothing above 80 Hz and CMP-9's pulse scope (R4c1-1). The section-5 box ticks only when CMP-4, CMP-5, CMP-11, CMP-17 and CMP-18 are
fixed, and each of those waits on a clip or on a call above. (4) **The owner's new ask (2026-10-15, with "continue with the next step"): modernised on-screen controls
taken from Magic Rampage.** Today the walk arrows are flush in the bottom corners (64 design units, the hit a quarter circle about the corner), restart and pause flush
top-right and clear-portals flush top-left (`game/data/ui.json` `hud`, `game/sim/Hud.*`); he wants a Magic Rampage layout away from the corners. **It moves the HUD on
every frame**, which is why the classic HUD was measured first (this re-take) and why **the classic mode is kept, as the mode of the parity rig**, every suite and every
route. Its own steps from planning step 177 (each takes the next free number when it lands): the design is `out/parity/program/CTL/design.md` (a read-only design
workflow, second pass after two reviews): 14 steps CTL-1 to CTL-14, natural stops after CTL-3, CTL-6, CTL-9 and CTL-13, decided with the owner on 2026-10-15: both walk
arrows grouped bottom-left, inset from the corner (the right thumb stays free to fire), restart / pause / clear-portals inset by the same margin, a toggle between modern
and classic in an Options screen (not a developer flag), Magic Rampage's own button art, and its adjustment screen (size 0.4-1.4, opacity, lock and drag, restore) as a
later step. Defaults: `--controls classic|modern` (a developer run, a route and every suite are classic; modern becomes the default for a player only at CTL-13), the
`uniform` preset (size 0.7, a 44 px margin on every edge). **The gate that holds the rest: classic reproduces `R4d/CMP/port_f420` and `WALL/after/sweep` byte for byte at
every step.** The art is read at run time from a gitignored folder, made by a committed tool that holds no Asantee bytes, with a drawn fallback (the hard rule); Magic
Rampage is another Asantee game that `docs/permission.md` (still the placeholder) does not record, so shipping its art is the owner's question (design Q2). CTL-1 (the art
tool: Python and docs only) was staged under `out/parity/program/CTL/CTL1/stage/` during the re-take and is NOT committed; CTL-2 onward relinks `MagicPortals.exe` and
`test_mp.exe` under Smart App Control. The census of the 128 routes against the modern hit rectangles names five levels whose recorded shots fall inside them (1-20 and
3-27 for the pair, 2-06 for restart, 1-09 and 3-14 for clear-portals), accepted as data (CTL-4). The owner's open questions Q0 to Q16 are in `design.md` section 6 with
recommendations (Q0 touch device or desktop, Q2 the art's rights, Q3 the default look, Q4 when to flip the default). GAMEPLAY: none (no gameplay number, one pointer
path). (5) **The ship preparation**: the grant in `docs/permission.md` (still its "INCOMPLETE - PASTE THE ACTUAL GRANT HERE" placeholder, so the record of the scope is
incomplete: the owner's to fill), the play build (`tools\parity\play_build.bat` exists; no play build has been made), the README and LICENSE, the release zip, and **the
repository's going public, which still needs the owner's explicit word**. (6) **Loose ends, taken when a step touches the file**: unchanged from item (3) of the wave-4d
list above (a full `port_build.bat` with zero warnings, R4c2-5 and WALL-7, among them). For the next wave's gates: future route gates compare against
`R4c2/census_after_round1.md` (128 won; 117 exact, 11 on another tick), a step's scope is derived by a census of `out/levels` and the original's files, never typed, and
the sweep a step is measured against is `WALL/after/sweep` or this re-take's `port_f420`.


**CK's recorded deviations**, each an open row for the step named (planning step 147 has the evidence, `RECORDED DEVIATIONS` 1-11):
- [ ] **CK-1. The player's ground walk-cell flipbook runs on the engine's unfloored 16.667 ms tick** (the one failed gate of the
      third verifier; cosmetic, no physics, route or sweep frame depends on it). The original's `FrameTimer::set(0, 3, 90)`
      (`SideScrollerCharacter::updateFrame` ins 80) adds the floored 16 ms; the port's engine flipbook
      (`MagicPortalsLayer.cpp:4175-4181`, `:6266`) is 4.17 % fast in wall time. Next: **G10** (it relinks anyway; fallback
      PIC) steps the cell with the `FrameTimer` logic on `Ms(step)` in `stepLevel`, as `m_shotFrames` does, and re-pins
      `test_mp_layer`'s walk case (6 ticks add 96 ms, 6 over).
- [x] **CK-2. 45 of the 86 routes no longer win on CK's build** (41 do: 34 on the same tick; 1-03 +3, 2-24 -1, 3-08 +4, 3-13 +6,
      4-18 +2, 4-28 +4; 4-21 on 438 against 587). 38 are refused by the 400 ms between taps (DECODED: strictly above 400 of the
      script clock, so 26 ticks, not 24); seven first causes are INFERRED and untraced: 2-18 (a shot in flight), 3-09 and 2-32 (the
      tap off the view: the camera's follow share), 1-11 (a moving no-portal zone), 1-32, 3-32 and 4-32 (the bosses' timers); 4-21's
      149 ticks sooner is the gravity agent's 0.96 pull (INFERRED). Next: **the wave's route re-author phase after PIC**, each route
      replayed on the final build so every cause is traced or the route replaced (G4 moves the shot-flight and view-edge rows).
      *Done in step 151 (`af008a9`): every route was replayed on the final build and 84 of 86 win; the two that do not are
      carried with their causes as R3-1 (1-11) and R3-2 (2-30).*
- [x] **CK-3. Three velocity readers carry no 0.96**: `applyForces`, `ETHCallback_minion` and `ETHCallback_dragon_knight` scale the
      velocity they set by `elapsedTime / 33.33` (the physics' fixed 1/30 s), x 0.96 with the floor; the port leaves x 1.0. CK
      exposes it for motion bounded by a script timer (a patrol leg, a stone's flight: about 4.17 % longer than the original's), the
      INFERRED cause of the sweep residuals 1-30, 3-05, 3-23 and 3-24. Next: **G10** (the player: its row under the majors) and
      **G1** (minions and the knight). *Step 148 did the player's (applyForces: `speed x min(66, ms) / step`, in the port's units
      through `SimRatio`). Left for G1, and found by it: `minions.json` reads `scale(1.3) x elapsed / 33.33` as 1.3 px per
      33.3 ms (39 px/s), where the same form in applyForces is metres of the original's physics, 50 units each: 62 u/s of
      wall time at 60 Hz.* *Built in step 152 (WT, local, not pushed): the minion's walk (and the 2-32 knight's, scale(1.7)) is
      `ScriptClock::ScriptSpeed` carried at `SimRatio` - 62.4 u/s, 1.04 a tick - measured on F9's two 3-18 patrols at 63.2 and
      62.3 u/s.*
      *Closed at wave 3b-1's close: all three readers are built - the player's (step 148) and the minion's and the 2-32 knight's
      (step 152, `ScriptClock::ScriptSpeed`: `scale(k) x min(66, the floored script ms) / the physics step`, pinned by
      `test_mp_timebase` at 1.0400 u a tick for a minion, 1.36 for the knight and 2.4 for the player). The INFERRED link to the
      sweep residuals 1-30, 3-05, 3-23 and 3-24 was not re-checked; G1 keeps the knights' sight and shooting.*
- [ ] **CK-4. Looser pins**: `TheRocketLandsAfter4_19sDoor`'s door tolerance 1.0; the beholder case lost its hard branch
      (`ABeholderRockBrokenOnTheLevelIsHeard`: the first two static breaks are soft in the new timeline, so the heard-above-2 branch
      is not exercised; it needs a rock placed over the floor); the dark dragon's flash colour within 0.02; the pad-alpha check's
      second clause 6/255; `test_mp_clock`'s antiportal check is arithmetic. Next: **the next timing step that edits the same suite**
      (G4 for the shot and the doors, G10 for the player), else the wave's verifier or close role.
- [ ] **CK-5. The launcher's strict test and reset-to-zero**: `ETHCallback_entity_launcher` throws at `elapsedTime > stride` and
      resets the sum to 0; `Launchers` carries the remainder and throws at `leftMs <= 0`, so its period is 125 ticks where the
      original's is 126 (2000 ms). Unchanged by CK. Next: **the first step that edits `Launchers.cpp`** (G1, or the re-author's first
      launcher level), measured by the sweep's "stones destroyed 94 ticks apart, not 90" lines.
- [ ] **CK-6. The timed crystal's origin: the 626th or the 627th tick.** The strict expiry is built (126 / 626 / 751 ticks for 2 /
      10 / 12 s); whether the timer entity's first add lands a tick after the crystal's is not traced. Next: **the route re-author
      and the final comparison**, against footage of a timed crystal.
- [ ] **CK-7. The loading screen's, the menus' and the popups' timings were not re-measured against the UI footage** after the clock
      moved them (the opening blacks are `GetTime` and stay on the footage fit). Next: **the final comparison** (section 5), with
      `out/parity/footage/INDEX.md`.
- [ ] **CK-8. 2-32's dragon schedule runs on one clock basis and is not re-gated** (the "Step 77 deviation, two clock bases" row
      above, done as to the unification): step 77's f180 / f240 / f420 / f444 frames are not re-measured. Next: **the route
      re-author phase** (2-32's route is among the 45) **and the final comparison**.
      *Step 151 re-authored 2-32's route (it wins on 2,718, 42 ticks of slack, and runs ahead of the view for its last 194 ticks,
      R3-3); the schedule's frames are still not re-gated, so the row stays open.*
- [ ] **CK-9. The per-frame paths scale a frame's milliseconds by 16 / 16.667** where the original floors each frame's own: exact at
      the 60 Hz tick, a limit at any other rate. No owner unless the port gains a variable-rate mode.
- [ ] **CK-10. The sweep: four frames (1-24, 2-04, 3-16, 4-06) carry random-driven particle detail one tick off**, the levels whose
      x 16 ms crystal expires before tick 420 (its expiry 250 -> 251, 375 -> 376). The shared effects stream is the INFERRED
      mechanism. Next: **PIC**, which re-takes the sweep; no action unless a deterministic motion differs there.
      *PIC re-took the sweep (step 150): 1-24, 3-16 and 4-06 change for a thrown stone and the camera, a patrolling minion and the spiral's turn (each explained in step 150's table), so the sweep does not separate the particle detail from the pictures' own shift, and 2-04 is byte-identical; no deterministic motion differs there. Still open for the final comparison.**
- [x] **CK-11. `readers.md` named `ETHTextDrawer` as the only C++ reader of the floored uint**; `ETHParticleDrawer::Draw` is the
      other, never created by this game. Closed in CK's commit (one sentence in the readers table and in step 147).

**G10's recorded deviations**, each an open row for the step named (planning step 148 has the evidence, `RECORDED DEVIATIONS` 1-13):
- [x] **G10-1. Only the player is in the original's time base** - *BUILT in step 152 (WT, local, not pushed).* The original's Box2D
      world steps a fixed 1/30 s a frame, twice the 60 Hz tick, so every dynamic body is now carried at `SimRatio` times the
      original's velocity and the world's gravity at its square - `ScriptClock::CarriedVelocity`, `OriginalVelocity` and
      `CarriedAcceleration`, the only places the factor is named: the world falls at the original's 10 m/s^2, 2000 units a second
      squared at the layer's tick (a bare-float tick, SimRatio 1, gets 500; the Godot 980 is deleted), a minion's, a knight's and a
      well's velocity is written at the ratio, and every callback's `hitIntensity` and a beholder's rock's crush read the body's
      velocity over it. The carried frame's crate falls exactly the original's `g (1/30)^2 N (N+1)/2` (258.333 units in 30 ticks), and
      a stone's roll on a ramp equals the same solver stepped at 1/30 s to 0.000 units. GAMEPLAY on every level with a loose body
      (82 of 128 changed in the sweep, the 33 with none byte-identical). Its companions: each entity's own weight
      (`game/data/materials.json`; the decoded restitution and friction are held, not applied: the engine bounces a pair before it
      touches and its friction solve walks a resting box, G10-14), no damping, and Box2D's clamp on every body. The routes: 62 of 86
      win (84 before). *The first build applied the decoded frictions and its verifier failed it (13 resting crates and barrels walked
      at a constant 5.65 to 8.78 units a second); this is the fixed row.*
- [ ] **G10-2. `GetFPSRate` is the matched 60** (the original's is frames x interval, 60-63 on a 60 Hz device, 30 for the first second, 17 on the
      emulator): the air brake's one rate-dependent number. The owner's call on the rate. **The verifier (medium) widens it: the 140 u/s
      recoil, the 4000 u/s^2 fall, the 0.86 brake and the 144 u/s walk all rest on `matched_hz` = 60, which no clip shows (F10 ran 17-19
      frames a second, F13 about 30; at 30 the same decode gives 70, 1000, 0.43 and 148.5).** Put to the owner as its own question
      (`clock.json` `matched_hz` / `physics_step_s`) **before PIC and the route re-author**: every route is replayed on the rate chosen. *Wave 3b-1 kept 60 and did not reopen it (the order); the owner has not ruled; 2-30's designed leap wins at `air_decay` 1.0 or at 45 or 30 frames a second (R3-2), so the ruling now also decides whether 2-30 is won honestly.*
- [x] **G10-3. The ground sensor** - *BUILT in step 153 (CAP, local, not pushed): the octagon stands where `addFeetOffset` puts it, 18 below the
      entity (`collisionBox.size.y x 0.5`, not the capsule's 18.1), and counts what `shouldBeIgnoredByFeet` lets through. **The row's premise was wrong
      and its gate was not built as written: the feet refuse a SENSOR, `isCharacter`, `fireball.ent` and `anti_sight_wall*`, and `isCharacter` is the
      player alone (set only by `GameCharacter::GameCharacter`, which only `MainCharacter` reaches; a minion is `AddEntity('minion.ent')`, `sensor="0"`), so a
      minion's or a knight's body IS ground, as the port already had it.** The audit: ten `.ent` files are `sensor="1"`, the 128 levels place six names of
      them 90 times and every one is an `Area2D` (a trigger), none solid; `fireball.ent` has no body. Pinned by `test_mp_player`
      (`AMinionsBodyIsGroundAsACrateAndAPlatformAre`, `NoSensorOfTheOriginalIsBuiltAsASolidBody`). Still the port's: the octagon is tested by shape each
      tick, not by Box2D's contact begin and end.*
- [x] **G10-4. The capsule is the original's** - *BUILT in step 153 (CAP, local, not pushed): `dark_mage.ent`'s two circles of radius 1.3 x 7 = 9.1 at
      +-0.5 x 18 = +-9 (`ETHCompoundShape.cpp:65-71`) as their hull, a capsule 18.2 wide and 36.2 tall, the entity's point its middle; the mass is step 152's
      0.104 and the friction the port's 0. Its numbers are `data/player.json` `body` (the box and the two shares; `LoadTuning` refuses a width or height
      that disagrees with them) and `test_mp_player` reads them back from `dark_mage.ent`. GAMEPLAY on every level: 20 x 44 -> 18.2 x 36.2, 3.9 units lower at
      rest (the sprite follows), triggers judge the real shape. Open as CAP-1 (the waist) and CAP-2 (the engine's rest skin).*
- [x] **G10-5. A standing player creeps down a slope: a CONFIRMED REGRESSION against the original** - *BUILT in step 148's amendment (R3 fix
      round 2, local, not pushed).* The cause was not the friction this row named: the engine integrates a body's velocity BEFORE it solves its
      contacts, and `Player::Steer` adds a tick of gravity to a grounded player, so an idle player sinks 1.111 units straight down into a slope each
      tick and the contact pushes him out along its normal - 1.111 x sin 15 x cos 15 = 0.278 units a tick, to the digit what 1-24's ramp showed - and
      a friction on the body cannot touch it (a first try, 0.9 on every idle tick, left 112.5 of 114.2 units on 1-24). An idle player on a plane he holds on (`data/player.json`
      `hold`: dark_mage.ent's 0.9, so up to 43.5 degrees) is now pressed along the plane's normal, not straight down: 1-24's ramp 0.000 units in 420 ticks
      (114.2 before), a 15 and a 30-degree floor 0.001 and 0.000 (129.5 and 358.9 before), the walk unchanged. GAMEPLAY on every level where a player
      stands on a slope. The five routes that leaned on the creep (2-25, 2-31, 3-19, 3-20, 3-22) were re-flown, each
      on its old win tick, and six more win on another tick (1-09 +13, 1-12 +2, 1-21 -1, 2-24 -1, 3-15 +2, 4-29 +1). Its deviations are the rows below.
- [x] **G10-5b. 2-24's idle player still leaves `barrel_bomb_ent_1135`** - *BUILT in step 152 (WT, local, not pushed): it was the
      MASS, not the time base.* The world's time base moved the drift from 36.03 to 36.35 units in 420 ticks and nothing else. The
      player starts on the barrel's edge, and the engine weighed every body 1, so the barrel tipped (0.022 -> 0.075 rad) and crept
      out from under him; the original's barrel (density 1 over 28.6 x 37.7 units, 0.431) is 4.15 times the player (density 0.5 over two
      circles of radius 9.1, 0.104). With the .ent weights **0.000 units for 420 ticks** (pinned in `test_mp_player`; 36.1 with every
      body at 1, the control). The mass half of G10-4 (the player's weight) was taken here for it; CAP keeps the capsule's shape.
- [ ] **G10-5c. The hold's other limits.** (1) Within 3 units of a platform's chamfered corner, or on a round top, the three probes are not one line and
      the player is steered as before (he creeps off a corner as he did; at the boundary he can shimmer 0.3 units a tick, 3-20's first ledge). (2) The
      port's floors all carry friction 1 (`LevelBuilder::kStaticFriction`; step 152 holds the decoded ones at 1, G10-14), so the hold's mix is 0.949, not the original's per-entity mixes (0.47 on
      ramp_right's 0.25, 0.42 on a platform's 0.2, 0.6 on block00's 0.4): a slope between 25 and 43.5 degrees holds the player here and not there.
      (3) ~~The original's friction also carries an idle player along a platform that moves sideways under him; the hold does not (G3's step).~~ *Refuted by step 154 (G3): a platform is a static body moved by
      `SetTransform` (no velocity), its axis is vertical, and a lift is the same: no friction carries a rider sideways in the original (INFERRED from the decode; no clip of a rider). Nothing to build; see G3-1.* (4) The
      mechanism is the port's, not Box2D's: why the original's walk shows no loss to the same friction is still not decoded.
      *Step 153 (CAP) scanned (2) (`out/parity/program/CAP/slope_scan.py`, the 128 levels' sloped static tops against `sqrt(0.9 x the decoded floor
      friction)`: platform 23.0 degrees, ramps 25.4, bridge01 33.9, block00 31.0) and built nothing: the 16 ramps (14.0 degrees) and bridge01's 46 at 14 hold either
      way; **five levels have a surface between 30 and 40 degrees the decoded mix would not hold and the port's 0.949 does - 1-29 (block00 40, 50), 2-11
      (single_block_plat 35, 55), 2-22 (bridge01 34, 0.1 over), 3-20 (block00 40, 50) and 3-29 (single_block_plat 30, 60)**; 1-02's 24 block00s at 45 degrees
      and 4-15's two double_block_plats are past 43.5 either way. Beyond the limit the player is steered as before, the frictionless creep (0.48 units a tick at
      30 degrees), which is not the original's slow slide (its idle x is set to 0 every update and friction acts on the rest; no footage of it): swapping a
      too-sticky hold for a too-slippery creep is not a gain, so the row stays open with both numbers. (1), (3) and (4) are unchanged.*
- [x] **G10-6. Box2D's `b2_maxTranslation` is 100 screen pixels** - *BUILT in step 153 (CAP, local, not pushed). Step 152 applied it to EVERY dynamic body at
      the 720-line reference (35.6 units a tick); the original's limit is 100 x 256 / H units (`ETHPhysicsSimulator.cpp:27`: a metre is 50 pixels, a unit
      H / 256), 35.6 at 720, 23.7 at 1080, 17.8 at 1440, 11.9 at 2160. `Tuning::screenPxPerUnit` stands at the reference (`player.json` `reference_px_per_unit`) until
      `MagicPortalsLayer::stepLevel` sets it from the viewport's height over the view's 256 lines, so every suite and route replay (1280 x 720 or no viewport) flies
      as before and a player's own window sets his. Pinned at 480 to 2160 lines through the solver and through the layer. Route-neutral at 720 (a fall of 590 units);
      on a 4K window a fall of 63 units reaches it. Open as CAP-4 (whether the port should be fixed at the rig's 720).*
- [ ] **G10-7. 1-09's playthrough puts the player on its two shelf crystals** (the decoded player cannot steer out of the far portal onto
      them), and **G10-8** 1-32's rock case asks no pair twice, not exactly one begin. Next: the route phase, which restores the strict pins
      (a real solve that reaches 1-09's shelf, with its three crystals confirmed reachable under the decoded movement; a rock placed over
      the floor for 1-32).
      *Step 151 won 1-09 (388) and 1-32 (2,758) on the decoded movement but edited no test, so `test_mp_layer`'s two looser
      pins were not restored and both rows stay open.* *Step 153 (CAP) loosened the rock case further for the new body (the begin is bounded by the last velocity read and the fastest fall,
      where it asserted equality) and re-pinned 4-01's pillar and a shot seam: CAP-10 holds the restore.* *Step 155 edited no test either: both pins stay loose.*
- [ ] **G10-9..12**: the tap's turn the tick it is taken; `air_velocity_share` removed from `art.json`; **CK-1 left to PIC** (PIC did not take it: PIC-5); the landing's
      hit 8.0 for a 100-unit drop (8.94 continuous, 8.67 on the original's own frames).
- [x] **G10-13. `Player.cpp`'s clamp comment says a fall of 570 units reaches Box2D's clamp**; the exact figure is 586.7 (about 590, as the
      step and `player.json` say). Corrected in step 148's amendment (R3 fix round 2), which relinked anyway.
- [ ] **G10-14. Friction is decoded and HELD, not applied: the engine's friction solve walks a resting box** (planning step 152's fix, `materials.json`
      `friction_applied: false`, `held_friction: 1`). A flat 30 x 30 crate on a flat floor at the decoded pair crate-on-platform (0.96 and 0.2,
      0.438 combined) accelerates to 5.745 units a second in 30 ticks and keeps it for ever, awake, at the two masses (0.144 and 1) and two gravities (40 and 19.6 m/s^2)
      tried (a 29 x 38 barrel 8.779); at 1 against 1 it sleeps from tick 30; a scan of the combined friction over 2-08 shows 0.44 and 0.5 creep, 0.6 and above hold.
      The cause, as a hypothesis: `PhysicsSystem`'s friction block re-derives the tangent each pass but clamps the accumulated impulse as along
      a fixed axis, and the port's z-deep boxes may give a face four points, two to a corner (a Python model with that assumed point order,
      `out/parity/program/WT/fix1/solver_sim.py`, walks the box 5.55 units a second and 0.00 at every friction with the tangent fixed per contact;
      with another order it does not, and the engine's manifold was not inspected). Not built: the original's crates, barrels
      and stones on their own frictions (a pushed crate slides on). Next: **an engine step on the friction tangent**, the
      tripwire case in `test_mp_timebase` failing, then the flag, then every suite and the routes. The zero frictions (a minion's, a ghost
      minion's, a gravity agent's) are applied. *R3b fix 1 (amendment to step 152, local): 3-13 was sealed by this row (the held 1 pushes the slot's crate at 0.58
      u/tick where the decoded pair gives 1.6), and the engine step alone may not unseal it: with the decoded combined friction (a body friction of 0.19) the
      route's walk reaches the far side on ONE release tick of 75. The stop-gap is `materials.json`'s `crate_low_emissive` group with its own `held_friction`
      of 0.08 (a guess; the wedge lets go and the held walk pushes the crate flat for every release from tick 180 at 0.079 to 0.082, on isolated ticks at every other
      value): 3-13, and the crates of 3-03, 3-05 and 4-09, are pushed at 1.89 u/tick. Drop it, and re-fly 3-13, when `friction_applied` goes true.* *R3b fix 2 measured the stone's decoded friction on the 86 current routes: 84 win, 2-30 (burned on tick 1346) and 3-12 (falls on 493) do not, 1-09 +2, 2-07 -1; R3b-3 and R3-2 carry the rest.*

**G4's recorded deviations**, each an open row for the step named (planning step 149 has the evidence, `RECORDED DEVIATIONS` 1-9):
- [x] **G4-1. The destination is the tap, not the tap-time ray's `finalPos`** (R23): the original's distance is `|finalPos - casterPos|` (the contact for
      a wall or a body a portal may not open on, 10 units past it for one it may); the port's shot flew to the tap and failed on the first body along the
      way. *Built in step 156 (wave 3b-2, local): `Portals::State::Shoot` runs the tap-time ray once and the shot flies to its `finalPos`, doomed or not as the
      ray said, and opens a portal there (`insertPortal`, no field test) or fails there (`portal_fail` at the destiny). The sensor and teleportable-body
      disagreements are gone with it (a button, a `destroyier.ent` or the anchor dooms the shot at its face; a crate, a stone or a minion takes the portal 10
      units in), and so are the head-height retry's and the reflected shot's.*
- [x] **G4-2. Bodies, blockers and reflectors are swept along the tick's way, not sampled** (the original has no body in flight; its blocker and reflector
      callbacks sample a position): a graze of at most 3.2 units at a blocker (64 x 128) or a reflector (16; G9's 32), and on an arrival tick they are
      still swept up to the destination where the original's static callbacks find nothing. *Step 156 (R23) built two of the three: the shot meets no body
      at all, and an `anti_projectile_wall` reads the shot's POSITION once a frame (`isPointInRect`, closed, a shot that arrives is gone before it looks).
      The reflector is still swept, with the port's 16 and its own leg: Next: **G9**.* *Built in step 157: a reflector reads the shot's POSITION once a frame, strictly within 32 x its
      scale, and never a shot that has arrived: all three are sampled now.*
- [ ] **G4-3. The spawn pass runs on the first tick after the tap**, one tick after the original's static frame (the layer reads a tap after `AfterStep`): an
      agent within 22.4 of the player dies, and a flame within 32 burns the shot, 16 ms later. No owner unless the layer's order changes (PIC).
- [x] **G4-4. A reflected leg starts at the catch point**, not at the reflector (the original's new projectile spawns at the agent's point and jumps 24 from
      there). Next: **G9**. *Built in step 157: the new shot is spawned at the reflector's point, its first callback jumps 24 from it, and its `projectile_shoot` flash is 24 out from the reflector;
      the OLD shot's flash is where it was caught (#245-260).*
- [ ] **G4-5. The sampled test is in double**; the original's squared distances are float32, so an exact tie is not pinned.
- [ ] **G4-6. The shot's cell timer steps on a tick the callback ran** (the tap's tick and a reflected leg's creation tick call it in no original frame);
      its stride, its one global timer and the ghost's shots are as they were. Two cosmetic one-tick differences of the picture ride with it: the original sets
      the timer (#89-100) before its arrival test, so it steps on the arrival callback too (the port's only while a flight remains), and on the tap's own tick
      the port draws the shot already turned along its way where the callback has not yet run.
- [ ] **G4-7. The footage is at 17-30 frames a second** (the pts are presentation times; the per-shot slope scatters 174-207 u/s): it supports the decode to
      about +-5 % pooled and cannot separate 192 from 200; F2 shot A and F3 take a1's second shot have too few clean frames to fit a slope. No clip shows the
      shot at 60 Hz. The arrival agreement is -0.064..+0.030 s (mean +0.002) on the continuous `(D - 24) / 192` and -0.049..+0.045 s (mean +0.011) against the
      discrete 60 Hz decode: both inside one clip gap. Next: **the final comparison** (section 5).
- [x] **G4-8. The failure picture's anti-portal-field flag reads the destiny, not the shot's overshot position**: `ETHCallback_projectile` #201-211 asks
      `isPointInAntiPortalField(GetPositionXY())` of the position after the last advance, up to 3.2 units past the destiny; `Portals.cpp` asked `InField` of the
      destiny. A tap within 3.2 units of a field's edge could draw `portal_fail_in_antiportal` (and queue the field's shake) where the original draws
      `portal_fail`, or the reverse; the picture and shake only. *Built in step 156: the flag is `InField` of the position the last advance left, pinned in
      `test_mp_shot` (a wall face 1.5 units short of a field's edge, the shot 2.7 past it).*
- [ ] **G4-9. An agent is tested at its end-of-tick centre; the original tests before it moves**: `ETHCallback_anti_portal_agent` runs its kill test (#37-199)
      before its own `linearMotion` (#201-250), while `Portals::Tick` advances the agent's motion at its top and `flyOnePass` reads its centre after. About a
      unit of the agent's own motion; pre-existing, no pinned scenario flips. No owner assigned: a row for the final comparison (section 5) to weigh.

**Step 156's recorded deviations (R23: the portal tap as the original), each an open row** (planning step 156 has the evidence):
- [x] **R23-1. The reflector is still swept, with the port's 16 and its own leg** (G4-2's third part, G4-4): `flyOnePass` finds a reflector's reach along the tick's way
      from the point after the jump, turns the shot at the entry, and the new leg's ray (`hasBeenReflected`) now decides where that leg goes and whether it is
      doomed. The original's catch is a position sample within `size.x` (32) of the agent and the new projectile starts at the agent. Next: **G9**,
      which also pins 1-12's new leg's end and verdict (red_cross or portal_mark) as numbers from the listing's `newDest` / clip arithmetic:
      `test_mp_layer`'s reflector case (`AShotTurnedByAReflectorMarksItsNewEnd`) now follows the sim's own verdict (`turn.said.willDie`), so it holds layer = sim
      and not the geometry (the verifier's low finding). *Built in step 157: 1-12's reflect_agent_761 with the player at (73, 204) and a tap on the line through it sends the shot to
      (287, 204), worked out by hand in `OneTwelvesReflectorSendsTheShotToNumbersWorkedOutByHand` (the first leg aimed at the wall's underside (200.0625, 128), the reflector 124.68 from the
      player, newDest the reflector's point + (107, 64), clipped to itself: portal_mark, a portal at (287, 204)), and the layer case computes the same arithmetic itself instead of reading
      `turn.said` back.*
- [ ] **R23-2. A static callback (a blocker, an agent, a flame) runs only while its bucket is drawn**, and the port's always do (section 15 of `docs/ethanon-formats.md`; step 154,
      deviation 2): a long shot past the screen's drawn buckets meets a blocker the original's callback never looks for.
- [ ] **R23-3. `light_off` reads the shot's position before the tick's move**, one tick later than the original's static look (`Game::AfterStep` runs the torch before
      `Portals::Tick`); the effects and the torch's state land one tick (16 ms) late. Not changed: no route or test moves by it.
- [ ] **R23-4. A tap exactly on the player's point**: the original's `normalize` of a zero vector is zero, so its projectile never moves and never arrives and holds the hourglass up
      for ever; the port's shot arrives on its first callback and opens a portal there. A measure-zero tap; not built.
- [ ] **R23-5. `preferedTeleportEntity` is stored by nothing**: `getObjectInPortal` (357583..358053 #25-50) teleports first, of the bodies in a portal, the one the shot landed on; the
      port teleports every traveller in a portal in its own order.
- [ ] **R23-6. `blockPortalEffects` tests the button with the world point** (`absTouchPos`, #62-78), the port with the screen point; the vibration (`vibrate 30` on a doomed tap,
      `vibrate 40` on a budget-0 tap) has no motor on the desktop; `no_portal_sign.ent`'s `stride = 4` has no reader.
- [ ] **R23-7. The ray's world is the registry's**: the bodies the port keeps as modules (the ghost's static sensor box, `dark_dragon.ent`'s sensor circle, `dragon_knight.ent`,
      `ghost_minion.ent`) are not in it, so a tap through the boss of 3-32 or 4-32 is not doomed there where the original's would be (the old flight did not meet them either).
- [ ] **R23-8. The field test is `<=` and in double** (`Holds`; R1's `_not_modelled`): the original's is strict and in float32, so a tap or a position exactly 32 x scale from a ring's centre is a
      refusal here and an acceptance there.
- [x] **R23-9. Six routes lose their win on this build** (the census in step 156: 1-23, 2-10, 2-17, 2-19, 3-13, 3-15; no route file was touched): each has a tap the ray ends short of or
      behind the tap (a doomed shot at a field-held stone, a burning crate, a teleportable body 10 units in), and 80 of 86 win on the same tick. **The routes are
      re-authored by the wave's route phase, after G1b** (PIC's pictures are already in this build), not in a step. Residual risk to carry into it (the verifier's low): the
      head-height retry now moves shots (126 of the 180 grid taps ran it) and four of the six (2-17, 2-19, 3-13, 3-15) end "after the retry"; its trigger rests on step 103's
      grabber reading and on the registry as the ray's world (R23-7), and whether those taps were wrong in the original cannot be checked without the emulator. Prefer taps
      whose ray end is the intended point (the log's `to (x, y)`), and spot-check one on the original in the emulator rig if a route hinges on a retry's end.
      *CLOSED by step 161 (`53d8db2`): all six are re-authored and win (1-23 597, 2-10 463, 2-17 519, 2-19 478, 3-13 1,043, 3-15 976). The retry risk
      stands as R3c-10; the rig as R3c-11.*
- [ ] **R23-10. The clock's rule rests on the listing alone**: the program's spec said every refusal restarts the 400 ms, the decode says only the tap that reaches the hourglass
      (or fires) does; built as decoded. No clip shows the restart after an hourglass tap or budget 0's tap (F3's take a2 follows by 2.8 s) and the emulator rig was not run for
      either. Next: the final comparison (section 5), one capture of two taps within 400 ms of an hourglass tap.
- [ ] **R23-11. A stale comment in `game/sim/Game.cpp` (about lines 655-666)** still says the port has "nothing to scale" for a fire-diamond shot, where `Shoot` now aims it
      at `final_pos.fire_diamond_reach` x the aim (harmless: the layer's conversion still runs before `Tick`). Left to keep the tested binaries current (a comment edit relinks both
      executables and every refused launch notifies the owner). Next: the next edit of `Game.cpp`.

**Step 157's recorded deviations (G9: the reflector as the original), each an open row** (planning step 157 has the evidence):
- [ ] **G9-1. A tie between two static callbacks that catch the same sample is settled by the port's order**: a blocker, then a reflector, then an anti-portal agent, then a flame. The original's is
      the scene walk's (its buckets' order, then the entities' in each), undecoded; the same sample is held by two only where two static callbacks stand within 64 units of each other, and in the 19 levels with a reflector none does: no `anti_projectile_wall` stands in any of them and no anti-portal agent or fire agent
      lies within 64 units of any of the 23 reflectors by node point (`G9/scan_near.py`; 1-12's reflector 761 and its co-located antiportal ring are a field, not a callback). Of two reflectors both catching one sample the port takes the first in the level's order.
- [ ] **G9-2. A static callback after the reflector's own, in the same frame, sees the new shot at the reflector's point** in the original (and no longer the old one: `DeleteEntity` erased it from its
      bucket at once); the port runs no look between the new shot's creation and its first callback on the next tick. Only an anti-portal agent, a flame, a blocker or a second reflector standing at a
      reflector's point could tell the two apart: none lies within 64 units of any of the 23 placements (a scan of the converted levels, `out/parity/program/G9/scan_near.py`).
- [ ] **G9-3. R23-2 holds for reflectors too**: the original's callback runs only while its bucket is drawn; the port's always does, so a shot far off the view can pass a reflector the original does
      not look at.
- [ ] **G9-4. No clip shows a reflected shot**, so the 32, the leg and the mirror rest on the listing and on the sprite's frame size (`reflect_agent.png` is 32 x 32, Scale 1.0; the hd copy is the same
      entity at the device's scale): the footage gate is NOT RUN. Next: the final comparison (section 5), one capture of a shot passing 24 units from a reflector on the emulator rig (1-12's
      reflect_agent_761, or 1-21's reflect_agent_826, which the route of 1-21 passed at 24 u). The radius is not a free number: the route shot of 1-21's tick 333 sits on the boundary (its
      callback-20 sample is 31.92-32.04 u from reflect_agent_826 across the 0.05 rounding of the printed player point), so a true radius a little over or under 32 would flip that one catch.
- [ ] **G9-5. The plane-less reflector's snap is at exact multiples of PI/4 in double**; the original's constants are float32 (`PIb` 1.570796327f), so a shot exactly on a diagonal of a plane-less
      reflector can go either way. The one such placement is 2-27's, which no route reaches yet (its reflector follows the handle, step 139; a route for 2-27 will find which side its shot comes from).
- [x] **G9-6. Twelve routes fail on this build and no route file was touched**: R23's six (R23-9) and 1-12, 1-21, 2-11, 2-16, 2-25, 3-14 (planning step 157's census, `G9/impl.md`): in the first four the
      route's reflected shot is still flying at the next tap (the new leg starts at the reflector and is as long as the old one was fired, or as far as it came: the hourglass), in 2-25 the reflected shot
      opens its portal at (319.6, 144) where R23's opened at the tap (524.7, 190), and the player is burned by a carranca's fireball on tick 383, and in 3-14 the shot of tick 637 toward (175, 262)
      passes reflect_agent_1244 at 25.3 units (over the old 16, under 32), is turned, and opens its portal at (313.3, 252.9): the two later taps fail. The other 74 win on their step-155 ticks with the same shots. **The wave's route phase re-authors all 86
      after G1b**; prefer a shot that passes a reflector at 32 or more, or one the route means to turn (the log's "sound shot_reflected").
      *CLOSED by step 161 (`53d8db2`): all six are re-authored and win (1-12 473, 1-21 498, 2-11 433, 2-16 397, 2-25 538, 3-14 1,141).*
- [ ] **G9-7. A turn on the spawn pass queues its effects in the wrong order inside the tick** (found by the verifier, `G9/verify1.json` problem 3, by code reading only; low, cosmetic): when a shooter
      stands within 32 of a reflector, `flyOnePass(false)` turns the shot and `flyOnePass(true)` launches the new leg in the same `Tick`, and `MagicPortalsLayer` queues the launches (the new leg's
      `projectile_shoot`) before the reflected record's waves, mark and flash; the original adds the reflector's effects on the tap's frame and the new leg's flash on the frame after. Not measured,
      and none of the 23 placements or the 86 routes was observed to reach it. Next: the next edit of `MagicPortalsLayer.cpp`, ordering a `Reflection` recorded before a `Launch` of the same `Tick` first.

**Step 158's recorded deviations (G1a: the knights see), each an open row** (planning step 158 has the evidence; `out/parity/program/G1a/decode.md` the machine):
- [x] **G1a-1. Which target `findEntityInSight` returns when two are in sight** (CLOSED by step 159, decode.md section 5 and `docs/ethanon-formats.md` section 23: `Add` pushes BACK, `MoveEntity` pushes FRONT for an `ET_HORIZONTAL` entity, and the player and `portal.ent` are both type 0, so the player is before a later portal in one bucket, older portals before newer, a nearer bucket before a farther; the port's order was right. The one reversal, a portal whose hover crosses into the player's bucket, is G1b-3.) (the player and a portal): the port walks the buckets in the original's order (the agent's, the next, the one after) and puts the player before the
      portals inside one; the original's order inside a bucket is the bucket list's (a mover is moved to the FRONT of its new bucket, an added static to the back), undecoded. It decides G1b's shot target, not G1a's sighting. 
- [ ] **G1a-2. The bucket a mover is stored in lags its position by a frame** (`ETHBucketManager::MoveEntity` queues the move); the port uses the tick's own position. At most one frame at a bucket line.
      *Built for the pictures' draw order only, by step 167 (CMP3, wave 4c-1), 2026-10-11; the row stays open*: a picture's bucket is the one its entity stood in after the
      previous tick (`ownerPx`), so a body that crosses a line is drawn with the bucket it left for one frame (`ETHScene.cpp:513`); the sim's own uses of the bucket
      (G1a-1, G1b-3) are not changed by it.
- [ ] **G1a-3. `char_feet.ent` is not a body here**: the original's look, which does not ignore it, can meet the feet octagon (14 x 10, 18 under the player's point) of a player more than 13 units below the ray's end; the port's
      ray cannot. Only a steep ray from below passes it.
- [ ] **G1a-4. `isFinished` is `goals.completed || hazards.playerDied`**, both set a tick later than the original's `finishGame` (the level's end and a death). It matters on the tick a sighting begins.
- [ ] **G1a-5. The cone and the ray are in doubles**, the original's in floats: a target within a float's rounding of the cone's edge, or a ray ending within one of a body's edge, can go either way.
- [ ] **G1a-6. The torch gives the sight back one frame early** (`Lighting.hpp`: the port has the lit ambient from the tick the torch is shot, the original on the next frame), and a torch put out takes it at once.
- [x] **G1a-7. The seven stealth achievements are still wrong** (CLOSED by step 159: `SpottedCount()` returns `Minions::State::SpotCount()`; test_mp_achievements pins each of the seven true and false): `MagicPortalsLayer`'s `SpottedCount()` returns 0, so 24, 26, 28, 32, 42, 66 and 76 unlock whenever the knights all stand, though a knight may have spotted the player
      (3-02's route unlocks 24 on 401 with a sighting on 194). `Minions::State::SpotCount()` (the sum of every sighting the `minion.ent` knights alive have made; the black knight is not counted, as `spottedCount` asks for 'minion.ent' alone) was the hook.
- [ ] **G1a-8. No footage shows a `?` or the sound of either mark** (the F14/still first-shot question is RECONCILED by step 159, decode.md section 8: F14's console-polled take r1 has the knight turn at HUD + 6.05 s against the rule's 6.0 s, with no constant tuned; the screencap-polled p1 and the still need late markers, G1b-1), and the one library still with a `!` (3-29's `h6.3`) fixes its place to within about 6 px at 1280x720 (the implementer's figure, 3.4 px in x and 0.0 in y, is the difference of the two sweeps' pixels above the knight's head; the independent check's,
      4 to 6 px in x and 1 to 2 px in y, is the port's mark's bounding-box centre and intensity centroid against the still's white-pixel centre; the knights are not at the same spot of their walk) and not its size
      or age; the 128-unit bucket rests on the listing and the formats doc, corroborated by that still (the `!` stands at x 382.0, one step inside the bucket line at 384; 256-unit buckets would put it near 398). The library stills `3-16_h6.3` and `3-23_t8.0` show the original's knight
      standing where the port stands it, with its first shot's flash (G1b). `F14.md`'s "about 4 s after the HUD" for 3-16's first shot and that still's 6.3 s are two shots of the 2000 ms interval seen from two markers (step 159). The mark's depth is the effects' stream's (knight z - 32), seen above the knight; its order against other sprites is not
      measured. Next: the final comparison (section 5), one capture of a knight turning to face the player across a bucket line and of a blocked way's `?` on the emulator rig.
- [x] **G1a-9. Seventeen routes failed after step 158 and 25 fail after step 159 (see G1b-8); no route file was touched** (planning step 158's census, `G1a/census.md`): the twelve of step 157 and 3-07, 3-22, 3-32, 4-28 and 4-29 (3-02 wins 13 ticks later). Their first problems: 3-07 knights
      that see and stand, with knight A walking ahead of the player; 3-22 the key knight's dwell zeroed by its sighting; 3-32 the boss's escort drops straight down because its ray to its waypoint is blocked by the double block (the old
      port drifted it onto the block); 4-28 the key knight standing at the wall while it sees the player; 4-29 a knight lifted onto the upper platform that searches instead of walking on to `button_2037`. 3-02's 13 ticks (388 to 401) are INFERRED from the traces of this build (no step-157 build
      remained to retrace them); re-measure them when the route is looked at. **The wave's route phase re-authors all 86 after G1b.**
      *CLOSED by step 161 (`53d8db2`): all 17 are re-authored and win (3-07 351, 3-22 1,219, 3-32 3,343, 4-28 791, 4-29 1,023 among them); 3-02's 400
      stays INFERRED (R3c-7).*
- [x] **G1a-10. The contact's hold cap is not built** (BUILT by step 159: `Minions::State::Tick` judges the begin of the contact with the player's body and caps `holdMs` at 1000; test_mp_shoot pins it on a rig and on 3-16's knight B) (found by the independent check; decoded here from the listing, `docs/ethanon-formats.md` section 22's last bullet): `ETHBeginContactCallback_minion` #12-55 caps a
      knight's own `holdTime` at `min(holdTime, 1000)` when it begins contact with a non-sensor `dark_mage.ent`. It has been open since step 97 (Order 11.4's "the dark mage's `holdTime` cap (#12-55)" row above) and interacts with the sight's
      dwell reset: a knight the player touches while it sees him would walk on after at most 1000 ms of dwell where the port stands it through a long hold, which moves the routes that brush a knight (3-02's "held up by a
      standing body", 3-07). Not built in G1a (one system a step). 
- [x] **G1a-11. The player is drawn lit on the six `darkest` levels** (found by the independent check, which read the library stills h6.3 of 4-25, 4-26, 4-27, 4-29, 4-30 and 4-31 against the port's frames; 4-25's pair was
      looked at again at the commit): the original shows the player as a black silhouette with its red eye (or not at all), and the port draws him purple and lit; 4-25's exit arch is drawn lit in the port too. Unchanged by
      this step (125 of 128 byte-identical to step 157's) and not G1's. Next: the final comparison (section 5).
      *Closed by step 166 (CMP1, wave 4c-1), 2026-10-11, as the general case CMP-1*: the six levels' players read the original's to 0.0-0.7 of 255 per channel (4-25 (2.4, 0.9, 1.0)
      against (2.9, 1.5, 1.6); 4-26 (2.0, 0.4, 0.4) against (2.0, 0.4, 0.4); 4-27, 4-29, 4-30 and 4-31 the same), 4-25's arch (9.6, 8.5, 10.0) against (9.8, 8.6, 10.1), and 4-22's
      and 4-27's crops show the black figure with its red eye as bright as the original's. 4-20's player stays red-lit in the original and not in the port (CMP-7).

**Step 159's recorded deviations (G1b: the knights shoot), each an open row** (planning step 159 has the evidence; `out/parity/program/G1b/decode.md` the machine):
- [ ] **G1b-1. The footage's two screencap-polled HUD markers are INFERRED to be late, never measured.** F14's take r1 (console-polled every ~75 ms, the OLD HUD awaited) has knight B turn left at HUD + 6.05 s where the rule says script 6.0 s: **0.05 s, no offset, nothing tuned**.
      F14's probe p1 (shot left at marker + 4.7 s, the rule's 6.65) and the library still `3-16_h6.3.png` (shot fired at marker + 5.9 s) would need their markers 1.95 and 0.75 s late - a screencap poll can only be late - which the footage cannot confirm.
      Only r1 (one console-polled point, about 75 ms of jitter) supports the first-shot rule; the first shot of the 35 levels is not otherwise measured. F9 (3-18), which the program names as a cross-check, gives no timing: its notes have the player landing at pts 24.403 (#387), at the corridor's right wall at 26.31 to 26.39 (#419-#421), knight B's red shot flying right along the corridor at 26.67 (#425) and still in flight when he teleports away at 27.47 to 27.70 (#437-#441), which is what the decode says (a shot flies on to its destiny once its target has left), but no row of its timeline marks the sighting, B's patrol phase is sampled from two cape grabs, and the port has no 3-18 route to replay the take's inputs: no error can be stated, so F9 is not a timing cross-check of the first shot. Next: the final comparison, one capture of 3-16 that reads the level's first script frame directly (a log line from the game, or the marker polled against the OLD HUD as `f14r.py` does), and a capture of a `?`.
- [ ] **G1b-2. The shot's timing conventions are INFERRED.** The knight's callback is the tick's BEFORE-step half, so the shot is made there, first flies on the next tick (the original's first callback is the next frame's), and its sound, pictures and shake are latched by the layer after the NEXT step
      (16 ms after the sim made it; the ghost's, made in the after-step half, are latched on their own tick); the order among the dynamic entities' callbacks (knight, shot, portal, player) is the scene walk's, undecoded. The knights' shots step `_projectileFrameTimer` after the ghost's.
- [ ] **G1b-3. A killed portal's entity lives about 400 ms in the original and the port drops it at once.** `killPortal` kills its two particle systems and `PortalManager::update` pops it from the array, but the ENTITY - still named `portal.ent` - fades 400 ms, so a knight could still see it (its
      `targetSpotted` would not drop) and a shot aimed at it would still find it by id and be spent on it. The port drops it from sight and from the shot's lookup in the same tick it is killed. And the residual of G1a-1: a portal whose hover (`linearMotion`) carries it across a 128-unit bucket line INTO the player's
      bucket is MOVED to the front of it and would be the first candidate; the port keeps the player first (and a bucket move is queued to the frame's end, G1a-2).
- [ ] **G1b-4. Two copies of the shot's flight.** The ghost's (`Ghost::State::shots`, `>=` on the distance flown) and the knights' (`Minions::State::shots`, the listing's strict `>` on squared distances from `initialPos`) are the same entity and callback in the original; `test_mp_shoot` pins the two JSON blocks equal. Fold them into one module when the ghost's suite is next
      touched (the ghost is made in the after-step half and the knights' in the before-step half, G1b-2).
- [x] **G1b-5. The stone's other kill is not built (a side finding of this step).** (CLOSED by step 160: `Game::StoneHitsPlayer`, demolish.json `stone.kills_player_above` 2.5, the fold into Hazards; the decode is `out/parity/program/STK/decode.md`; the numbers' margin is STK-1.) `ETHBeginContactCallback_rolling_stone` #107-129 sets an `isCharacter` other's hp to 0 when hitIntensity is above 2.5 - a rolling stone kills the PLAYER it hits fast enough. No stone-to-player kill was found in `Demolish`, `Hazards` or `Launchers` (`Demolish.hpp` says "a stone is not a hazard: it kills nothing"; launchers.json's
      destroyier note says the owner saw one kill the player on 11 September, which is that sensor); `Boss::Contacts` does kill the player with the BEHOLDER's rocks, the one stone the port lets crush him. Not built (the step is the knights'); it is GAMEPLAY on the seven levels with stones and it moves routes that cross a stone. **The independent check's one MEDIUM finding is this row**: on the seven stone levels (3-07, 3-10, 3-12, 3-15, 3-18, 3-20, 3-22) a route can stand under or against a stone that kills the player in the original, so the routes the wave's route phase re-authors would be written without that rule; and the crush that IS built has a thin margin for them (3-12's first crush is at hitIntensity 1.52 against 1.23, about 24 % over, and the port's hitIntensity is the impact watch's, its equality with the callback's `dp2(velocity, normal) / getScale()` INFERRED, G1b-6). **Before the route phase, one of two**: build it as its own small step (a Stone begin whose other is the player and whose hitIntensity is above 2.5 folds into `Hazards` as the eighth death; `crushMinions`' impact begins are the hook), or the owner rules it out; the wave's close says which. Not decided here (text only).
- [ ] **G1b-6. The knight is one circle where the original's is two.** The hold cap's contact and a stone's crush begin when the port's 7-unit circle touches the player's capsule or the stone, where the original's capsule (14 x 36, two circles of radius 9.1 by its own scale) may touch earlier at the head;
      a crush's hitIntensity is the impact watch's approach velocity along the line between the two entity points, the same number the stone's own impact row reads (INFERRED equal to the callback's `dp2(velocity, normal) / getScale()`).
- [ ] **G1b-7. A barrel's blast kills a knight one tick after it.** The grabbed knights go through the flame's one-tick-later burn (`Fire::State::minionsAlight`); whether the original's minion callback runs before or after the barrel's on the blast's own frame is unknown. The blast's reach is the port's sphere against the knight's one circle, with `Fire::Blast`'s ray to its centre.
- [x] **G1b-8. 25 routes fail and no route file was touched** (planning step 159's census, `G1b/census.md`): G1a's 17 (the twelve of step 157 unchanged; 3-07, 3-32 and 4-29 now die to a shot, 3-22 and 4-28 as before) and eight new: **3-03, 3-08, 3-09, 3-27, 3-31, 4-30** (a knight's shot ends on the player) and **3-10, 3-12** (a stone crushes the knights the route needs).
      3-02 wins on 400 (was 401; INFERRED to be the contact's hold cap, no cap-less build was retraced: if it ever matters, retrace once with `contact.hold_cap_ms` set very large) and no longer unlocks 24. **The routes are re-authored by the wave's route phase, after G1b, on this build.** (Step 160 moved only 3-12, which now dies on tick 200 under `rolling_stone_1198` at hitIntensity 4.28 instead of failing on the crushed knights; the other 24 fail as before. The same phase re-authors it.)
      *CLOSED by step 161 (`53d8db2`): all 25 are re-authored and win (3-03 776, 3-08 1,666, 3-09 838, 3-27 1,214, 3-31 1,668, 4-30 1,337, 3-10 1,435,
      3-12 639); 3-02 wins on 400 (R3c-7).*
- [ ] **G1b-9. The shake of every shot's end is global.** `startEarthquake(10, 0)` runs wherever the shot is; a knight's shot a screen away still shakes the view (the original's, and the ghost's).
- [x] **G1b-10. The layer's `SpottedCount()` has no test of its own** (BUILT by step 160 as `AKnightsSightingKeepsTheStealthAchievementFromTheDoor` and PASSING on `test_mp.exe` `1aee46d3` after the verifier's round-1 fix: `test_mp_layer` 3,706 checks, 0 failures. Three controls that wait 20, 30 and 40 ticks reach 3-02's door with no knight having sighted him and 24 is dispatched in each; the seen run (two sightings, both knights alive) does not dispatch it, so a `SpottedCount()` that returned 0 fails the seen run - shown by construction, not by a mutation run; the number's equality with `SimLevel()->minions.SpotCount()` beyond zero and non-zero is STK-6) (the independent check's, low; a test, not text, so not built by the commit). `MagicPortalsLayer::SpottedCount()` is `return m_level.minions.SpotCount()`; the seven stealth conditions are pinned against a fake world (`test_mp_achievements`) and the one end-to-end evidence is the route census (3-02: `achievements: 24@401` before, `none` now), so, before this case, reverting that one line to `return 0` failed no suite (it now fails the seen run, by construction).

**Step 160's recorded deviations (STK: a rolling stone kills the player it hits fast enough), each an open row** (planning step 160 has the evidence; `out/parity/program/STK/decode.md` the callback):
- [ ] **STK-1. The kill's number is the impact watch's, INFERRED equal to the callback's** (the same row as G1b-6, which stays open for the knights' crush). hitIntensity is `Begin::Approaching(along)` over 50: the fastest the stone came along the line to the player's point since it last met or left something (at most 60 steps), the speed the original's Box2D would read at the begin because the engine's solver brakes a closing pair before it meets. No footage shows a stone against the player (INDEX.md and every F*.md/L1.md searched; the 'destroyier' note of launchers.json is the sensor, not a stone), so the margin of any contact is not measured; 3-12's route is 71 % over (4.28), a fall of 24 px reads 2.33 and one of 40 px 3.33. Round 3 also looked at the F9 (3-18) and L1 (4-22) sheets, a 1 fps tile of F9 and the `_h6.3` stills of all 26 stone levels: every stone is at rest and the player alive at his spawn, none shows a contact; the direction of the offset is STK-9. Next: one capture of a stone against the player in the emulator rig, or the owner's play.
- [ ] **STK-2. Two copies of one callback for the stone's kill (G1b-4's kind).** The beholder's rocks are killed by `Boss::Contacts` (its own 30-tick window of the rock's velocity over the SimRatio, a 2 px margin and its own `Intensity`; boss.json `rock.crush_intensity`), every other stone by the impact watch (demolish.json `stone.kills_player_above`); `test_mp_demolish` pins the two numbers equal and `Game::StoneHitsPlayer` leaves a body in `boss.rocks` to Boss. Fold them into one when the boss suite is next touched.
- [ ] **STK-3. The kill is the tick's AfterStep, not the physics step's.** The original writes hp inside the step and `checkGameLost` runs the same or the next frame; the port's blood, hide and lost screen start on the begin's own tick. At most one tick, unmeasured.
- [ ] **STK-4. Only solid bodies' contacts.** The original also runs the callback for the player's feet sensor `char_feet.ent` (no `isCharacter`: no kill) and any sensor; the port's contacts, here as for the stone's other rows, are its solid bodies'. A stone that touches only his feet sensor makes no difference to the kill.
- [x] **STK-5. The corrected `test_mp_layer` had not run on the final runner.** (CLOSED by the fix of step 160's verifier round 1: Smart App Control had refused `test_mp.exe` three times in the step (`0eb2fa00`, `e8e38ef3`, `6ccd4b4b`) and once for the verifier (`fa4467d6`); the runner was deleted and relinked once with the test edits (`1aee46d3`, 0 warnings) and ran at its first launch, with no refusal: `test_mp_layer` 3,706 checks, 0 failures, then the whole runner once: 55 suites, 54 passed, 30,329 checks, only `test_mp_routes` red, on the same 25 routes.)
      The first run (`7e521035`) had 3 failures in the step's own new cases: the resting stone's speed threshold (500 px/s where the engine carries it at 466: now 300) and the stealth control, in which a knight sighted the player in the first ticks of the crystal sequence (which knight was never logged; the case now runs controls that wait 20, 30 and 40 ticks, judges each by the rule whatever it sees, and prints each knight's place and sightings). The G1b-10 case's comment ("the second of them holds beside the third crystal looking at it") and the resting case's ("what it carried before is spent") still state causes that were not established; a comment-only edit relinks the runner, so they wait for the next real edit of `tests/test_mp_layer.cpp`.
- [ ] **STK-6. G1b-10's case pins the layer's `SpottedCount()` through the door's dispatch only.** The world is `LevelWorld` in an anonymous namespace of `MagicPortalsLayer.cpp` and the layer has no public `SpottedCount()`, so the case cannot read the number; it shows that a knight's sighting keeps achievement 24 from the door (and that its absence does not), which a world answering 0 would not. Since the fix every run also asserts both knights alive at the door (so 24 is decided by the sightings and not by the knights' count) and the rule is judged on four runs. Its equality with `SimLevel()->minions.SpotCount()` beyond zero and non-zero is not observable without an accessor, which a test-only change should not add.
- [ ] **STK-7. The resting-stone case is weaker than its comment says.** `AStoneRestingAgainstThePlayerKillsNothing` waits about 125 ticks after the stone's landing before the player touches it, past the watch's 60 steps, so it would pass even if both the 60-step window and the reset on a contact change were broken: it shows that a stone long at rest kills nothing, not that a recently fast stone is read afresh (the claim its comment makes). Next: a case that lets the stone rest a few ticks only after a fast fall and touches it then. Round 3 adds that the layer's falls read 0.33 to 2.33 and 3.33 and nothing between (the per-tick quantum is 1/3), so the strict edge itself is pinned only at `Demolish::KillsPlayer`/`HitPlayer` by `nextafter`: pin 2.33 and 2.67 at the pipeline if a gap gives them, in the same edit that touches the case's comment (and the G1b-10 case's, STK-5). *Step 162 did the first half: the falls read k / 3 now and `AStoneDroppedFifteenPxDoesNotKillAndSeventeenDoes` pins 2.333 and 2.667 at the pipeline; the resting case's comment and its weakness stay.*
- [ ] **STK-8. Should `destroyier.ent` still kill the player? (a question for the owner; the verifier's low finding.)** The listing has no `ETHBeginContactCallback_destroyier` and no reader of `destroyier.ent` but the stone's callback (`ETHBeginContactCallback_rolling_stone` #73-105 destroys the stone that touches it; L68589), yet `data/entity_roles.json` lists `destroyier.ent` among the hazards that kill the player and the layer builds a 16 px trigger for it (`launchers.json`'s note: the owner confirmed on 11 September that it "also kills the player"). With the stone's own kill built (STK) what he saw near a launcher's drop may have been a thrown stone landing on the player at hitIntensity above 2.5 (1-26's launched stones read 8.1 on the player in the verifier's replay), not a sensor. No footage shows either (INDEX.md and the F*.md notes searched; the mp4s were not decoded, no ffmpeg on PATH). Not changed here. Next: ask the owner what he saw; if it was a stone, drop the player-kill of `destroyier.ent` (`hazards.json`, `entity_roles.json`) and keep it a despawner of thrown stones. *Step 164 (wave 4a): 1-24's route (`level23`, won 343) reaches the door only by a drop onto its trigger over `destroyier_ent_1051`'s box, which kills a walking player on tick 366 in the port; it wins either way, and `reachability.py` has no destroyier term, so 108 / 20 / 0 does not cover the box (R4a-4).*
- [x] **STK-9. The impact watch reads a free fall one to two gravity quanta low, so the port kills from a higher drop than the original (the verifier's one MEDIUM of round 3; DERIVED, not measured; NOT text, so not changed by the commit).** (CLOSED by step 162, `11d0be4`, pushed in wave 4a (`36a52e6..8fe8374`): a rolling stone's begin in free flight is read as Box2D's own step reads it - the velocity after the first step whose FREE position is within the pair's skin, `Impacts::FreeFlightVelocity` replaying the flight from the last step the engine had not braked - and the seven falls read 1.000, 1.667, 2.000, 2.333, 2.667, 3.000 and 4.000, where the row's figures for 2 and 6 px, 0.667 and 1.333, took the polygon's skin as 0.5 level units and the stone against the player is circle against circle with none; 15 px does not kill and 17 px does. GAMEPLAY: a stone kills from a drop over 15.56 u, a knight is crushed from over 3.33 u. The 86 routes win on the same ticks. What stays open is STK9-1 to STK9-8 below.) The original's physics has no damping and `rolling_stone.ent` no gravity scale, so a stone free-falls at 500 u/s^2 in 1/30 s steps; the port's per-tick quantum is exactly 1/3 and its falls read 0.333, 1.0, 1.333, 1.667, 2.0, 2.333 and 3.333 for gaps of 2, 6, 10, 14, 18, 24 and 40 px, where Box2D's own timing (BeginContact in the next step's collide with the velocity the previous step left, no speculative braking, a 0.5 px skin) reads 0.667, 1.333, 2.0, 2.333, 2.667, 3.0 and 4.0: 0.33 to 0.67 higher. The original crosses 2.5 at a drop over about 16 px, the port only over about 25 to 30 (24 px reads 2.33, no kill; 40 px reads 3.33, kill). Cause: the engine's speculative contact brakes a closing pair a step or two early (`Impacts.hpp`, planning step 152) and `Approaching()` recovers the speed at the start of the braking, not the speed gravity would have given at the touch; step 160's deviation 1 said the watch "reconstructs" the callback's speed, which it does only to within that offset. No winning route has a stone begin near the threshold (at most 0.0), 3-12's kill is 71 % over, and the knights' crush (1.23) errs the same strict way, so no route is thin; **until this is settled a re-authored route should keep any stone's begin on the player under about 1.5** (the offset is at most 0.67, so that reads under 2.2 on the original). Next, before the route phase is called final or at latest before shipping: measure one stone landing on the player on the original (STK-1's capture) or settle it by arithmetic on the decoded gravity; if it holds, make the stone rows (the kill, the knights' crush, the shake, the hit sound) read the unbraked free flight (the peak velocity plus g dt along the line for each braked step between the peak and the begin), re-pin the falls of `AStoneThatBeginsToTouchThePlayerKillsHimAboveTwoAndAHalfAlone` and record it as a gameplay change of the watch (steps 142 and 152's reading).
      *Step 161's verifier checked it against all 86 routes: none is near the line, and the wave pushed with it open (R3c-1).*

**PIC's recorded deviations**, each an open row for the step named (planning step 150 has the evidence, `RECORDED DEVIATIONS` 1-9):
- [ ] **PIC-1. The smoothing's line is a guess and unmeasured on a real fast display.** `smoothing.above_hz` 80 / `release_below_hz` 72 /
      30 frames / 2 ticks are the port's own (`_guess`), unit-tested against simulated frame lengths and a layer fed 300 Hz frames, but no 90 /
      120 / 144 Hz window was run: above the line the behaviour is the engine's own interpolation, exactly the build before step 150. Next: one
      real-time look on such a screen; the owner's call whether the smoothing stays (`above_hz` 0 draws at the tick's place at every rate).
      Nothing logs when `RateWatch` flips, so a fast screen cannot be checked by its log: log one line when `Smoothing()` flips, with the
      measured Hz, at the next relink (the verifier's fifth problem; a source change, not applied by the commit).
- [ ] **PIC-2. Above the line the pictures trail the sim by up to a tick again**, and the lights and effects that read the sim's point lead them
      (2.9 px at the player's walk): the price of the smoothing, stated in `pictures.json`.
- [ ] **PIC-3. Five comments are stale**: "between ticks" - true only above the line, in the smoothed mode - in `MagicPortalsLayer.hpp` (lines 85,
      349 and 3305) and in `MagicPortalsLayer.cpp` (line 9825, the earthquake block's "WHAT MOVES"; the verifier's find, not on the step's first list),
      and `drawPicturesAtTick`'s first comment (`.cpp` line 10923), which lists the HUD among what reads the sim's point; the HUD is the screen's own
      overlay. Left to keep the tested binaries current (the G10-13 precedent). Next: correct all five at the next relink.
- [ ] **PIC-4. The first ~20 frames of a real-time run on a fast display draw at the tick's place**: the watch starts at the matched 60 Hz and
      walks to the rate it measures.
- [ ] **PIC-5. CK-1 is not taken by PIC** (G10's fallback): the player's ground walk-cell flipbook still runs on the engine's unfloored tick. It
      changes the cells of every walking frame and would mix a second cause into a one-cause sweep; no route, tick or position depends on it.
      Next: the route phase or the final comparison.
- [ ] **PIC-6. The 128-level explanation reads a no-input run of the suite, not the captures' own state**: the capture's 420th frame is taken as the
      layer's 420th tick (the table keeps 418 to 422; 3-10, 3-17 and 3-21 move on 419 and 421, stand still on 420 and are byte-identical, which fixes it).
- [ ] **PIC-7. `test_mp_layer`'s level31b aura pin measures the smoothed mode** (the verifier's first problem): the case feeds 120 Hz frames, so the
      smoothing engages and `worstBody > 0.5` holds on a knight drawn one patrol tick (0.650 u) behind its body; the printed figure moved from 2.013 to
      0.650. In the default mode it would read about 0 and the pin would not hold. Next, at the next test edit: feed the case whole-tick frames, or
      assert both modes.
- [ ] **PIC-8. `Pictures::Rules` carries two default members that duplicate `pictures.json`** (`windowFrames` 30, `hitchTicks` 2.0 in
      `game/sim/Pictures.hpp`; the verifier's third problem): inert (a default `Rules` never smooths and `LoadRules` overwrites all four) but a tunable
      number in code. Next, at the next relink: default them to 1 and 1.5 (or leave them unset) with a comment that the JSON is the one source.
- [ ] **PIC-9. 4-22's byte-identical f420 frame is INFERRED, not confirmed, to be an unlit moving platform** (`moving_platform_1716` in a `darkest`
      level, a picture moving 0.43 u a tick drawn black on black; the verifier's sixth problem): the frame is not static (f420 against f421 differ by
      15,627 px), so the sweep cannot separate "invisible" from "not drawn". Next: capture 4-22 with the platform lit once, or leave it for the
      final comparison.

**The route phase's recorded deviations** (step 151), each an open row for the step named (planning step 151 has the evidence,
`RECORDED DEVIATIONS` 1-8; `out/parity/program/R3/finalize.md` the rest):
- [x] **R3-1. 1-11 is not won by a route** (`level10`; sealed on the timing build: the island lies in antiportal_709's 192 px
      ring and no input reaches nearer than 28.15 px to the nearest legal portal against a reach of 24). Needs G3 / R7 (the
      platforms' full cosine swing), then a route; geometry derived, not measured on a G3 build. (planning step 151, deviation 1)
      *Step 154 (G3, local, not pushed) built the full swing: `moving_platform_707` now runs y 254 -> 114 (top 238 -> 98), pinned by
      `test_mp_movers`; its top starts 14 u under the island floor (y 224) and ends 14 u over `double_block_plat_ent_708`'s top (112),
      the step round 3 hypothesised, so the 28.15 px geometry (computed on the half swing, which never reached the island floor) no longer
      describes the level. The committed route still fails on tick 59 (a shot still in flight, as on CAP's build); a route that uses the
      block is the route phase's, and `anti_portal_agent_639` starts its swing only once the camera has come near in the original (G3-2).*
      *Closed by step 155 (`7c843c7`, local, not pushed): 1-11 is won by a route on tick 835 (`win_by` 880). The route boards the
      rising platform G3 built, kills three anti-portal agents with 11 shots and 4 traversals and takes the door; the third verifier
      replayed it with `--trace 30` and read it as designed. The platform's phase is the port's tick-1 convention, not the
      original's (the bucket clock is not modelled, G3-2), and the route's notes say so; the library stills fit the block and the
      horizontal agent to one phase at 1.75 u rms (G3's fit, 1-11 only).*
- [ ] **R3-2. 2-30 is not won by a route** (`level29a`; the player is burned on tick 259 by fire_agent_ent_918). The air carry is
      capped at 14.74 u (AirShare 0.86 at the matched 60 Hz), the plank sags once the stone leaves its arm (INFERRED: G10-1 time
      base, G10-4 capsule mass). Needs the owner's rulings on G10-2 / G10-1 / G10-4 and a source step or two, then every route
      re-timed. (planning step 151, deviation 2) *Step 152 (WT) built G10-1 and the mass half of G10-4: the route still dies, on
      tick 270 (259 before); G10-2's ruling and CAP's capsule stand, and the route phase after G3 re-authors it.* *Step 153 (CAP) built the capsule
      and measured the plank (`CAP/plank_route.py`): the committed route still dies on tick 270, the stone lying at (356, 175) on the right arm; the idle player on the plank
      (x 263) holds at y 166.3 from tick 100 to 420, and walking left he is carried up the raised left end and off it (y 133 at x 135 on tick 260), landing at (115, 187) in
      `fire_agent_ent_918`'s circle. The plank does not sag under him with the stone on its arm; the airborne carry (G10-2's AirShare) is what is left.* *R3b round 2
      re-authored it on the stone's creep back from the wall (a win on tick 1386). R3b fix 1 established the creep's cause by a data edit of the run: the stone's
      decoded friction (0.025, mixing to 0.158 against the held floor) leaves it resting at x 30.1 for the 1,100 ticks that remain, so the creep is the HELD friction's
      traction (G10-14), and with the decoded friction applied the census moves four routes only (2-30 and the old 3-12 stop winning, 2-07 -1 tick, 1-09 +2). Not applied:
      2-30 has no other route under the shipped air brake (the stone dropped on the right arm counterweights the plank left-up; the player walks up it to (143, 136) and off its raised end at x 134 into
      the flame's circle on tick 264 - the leap that is 0.4 u short). DIAGNOSTIC, data edits for the run: that same walk WINS on tick 278 with `air_decay` 1.0 and with `air_decay_fps`
      30 or 45, and dies at 0.93 and at the shipped 0.86, so the designed solution exists and G10-2's matched rate is what stops it. Open; the owner's to rule on (planning step 152's amendment, 3).*
      *R3b fix 2 (the second verifier's one medium finding, NOT fixed: it waits on the owner; planning step 152's R3B FIX 2): re-measured on the current 86 routes
      and round 1's counterweight route (`R3b/fix2/cw_r1.json`, outside `tests/routes`; its win tick 284 and the 278 above are two variants of the same walk): it dies on tick 270 at `air_decay` 0.86, 0.90 and 0.93 and wins on tick
      284 at 1.0, `air_decay_fps` 45 and 30; the stone's decoded friction fails 2-30 (burned on 1346) and 3-12 (falls on 493) and nothing else (1-09 +2, 2-07 -1).
      The route's `_about` now says all of this. The three choices are in the planning step's table: settle G10-2, apply the stone's friction, or keep the creep
      until the engine's friction step.*
      *Step 155 and the third verifier (`verify3.json`, medium, unchanged since round 2): **the one failed gate of the wave's last
      verification** (4a, the player walks to the door by the level's own mechanics). 2-30 is won on tick 1,386 (`win_by` 1,420)
      while the player stands at (268.8, 167.5) from tick 270 to 1,290 with no input and the stone creeps 0.0867 u a tick on a flat
      floor (x 32.1 to 113.4); the route's own `_about` says so in capitals. A player who finds the designed counterweight leap
      meets a sealed door. No route was found in five earlier rounds and none was searched for in R3B FIX 2, which is not a proof
      that none exists. The owner's three choices, none taken (planning step 152's R3B FIX 2 table): G10-2 lower (every route is
      re-timed), the stone's decoded friction (2-30 and 3-12 red until re-authored; 1-09 +2, 2-07 -1) or the creep accepted until
      the engine's friction step.*
      *Step 164 (wave 4a): 1-22's route also rides this creep (its tick-434 arrival moves 400 to 446 under a 1-tick change of any earlier
      input: R4a-3); 2-30's one-tick displacement at tick 159 (14.8 px) is noted as R4a-9.*
- [ ] **R3-3. 2-32's route runs ahead of the view for its last 194 ticks** (2525-2718, up to 293 u past the right edge; the
      dragon's camera, 45 u/s, shows the door at x 2554 only from about tick 3126, extrapolated). The original has no rule against
      it (ETHCallback_dragon_claw takes only what lies left of the view and never the player; AutoCameraController follows the
      dragon between camMin and camMax; checkGameLost reads minPos / maxPos). A route that waits in view dies to the dragon's
      fireballs on tick 2643 (low block) or 2845 (high block). Owner or footage: would a person play it so? If not, the route
      needs a dodging author. (planning step 151, deviation 3)
      *Step 155 re-flew 2-32 (won on 3,205 where 2,718, `win_by` 3,235). The third verifier measured the player's centre never
      outside the view (0 ticks; the earlier 194-tick stretch is gone) and up to 221.3 u ahead of the dragon's camera, of a
      half-view of 227.55. Still the owner's: would a person play it so? If not, its fix is a walk lagged by about 20 u.*
      *Step 161 (wave 3b-2) did not touch 2-32's route: it still wins on 3,205 (`win_by` 3,235).*
- [ ] **R3-4. Two routes are knife-edge**: 3-20 (`level19b`) wins only for a second walk ending on tick 238 (237 does not win,
      239 dies on tick 240) and 3-22 (`level21b`) only for a walk of 428-430 (its neighbours do not win). Both replay
      deterministically and say so in their notes. Next: re-flow them when the time base or G3 moves them, or give each a walk
      with margin. (deviation 4) *On step 152's build 3-20 dies (tick 386) and 3-22 does not win (by 1,500); both are re-flown by the
      route phase after G3.*
      *Step 155 re-flew both: 3-20 is won on 555 with its second walk ending on 238 only (237 and 239 do not win), 3-22 on 1,181
      with a drop good for one tick (7 of its 56 +-3 variants die); 3-13's crate friction is a third (R3b-3), the looser `win_by`
      R3b-6.*
      *Step 161: 3-22 is re-flown again, now a rush past its knight on a margin of about one tick (R3c-3), and 3-20 is untouched (555, `win_by` 585,
      a second walk that must still end on tick 238).*
- [ ] **R3-5. Brief camera lags outside the view after a portal exit**: 4-18 (ticks 190-194, up to 292.6 u from the camera centre,
      65 u past the edge), 1-21 (429-431) and 3-09 (322-325); the follow catches up in 3 to 5 ticks. Not a route fault. Next: the
      final comparison against footage. (deviation 5)
      *Step 155's third verifier: the longest centre-out runs are 4-18 5 ticks (from tick 190; the body fully out 4), 1-21 4 and
      3-09 4; the largest |dx| is 4-18's portal exit, 292.6 u.*
- [x] **R3-6. G10-5b stays open**: an idle player still leaves 2-24's barrel (36 units in 420 ticks; the route wins on 446). See
      G10-5b above; the world's time base (G10-1) is the next. (deviation 6) *Closed by step 152 (WT, local, not pushed): the cause was the
      player's weight, not the time base, and the idle player now holds 0.000 units for 420 ticks; 2-24's route does not win on that build
      (not won by 480) and is re-authored by the wave's route phase after G3.*
- [ ] **R3-7. Every route is timed at the matched 60 Hz** (`clock.json` `matched_hz`), the rate the owner has not ruled (G10-2): a
      ruling that moves it re-times all 84 again. (deviation 7)
      *Step 155 re-timed all 86 again at 60 Hz; the 2-30 leap is the route that rate decides (G10-2).*
- [ ] **R3-8. `test_mp_routes` is red on 1-11 and 2-30, and no owner ruling to push with it red is on record.** The second
      verifier (`verify2.json`) quoted one as recorded in this section as two open rows; there was none (searched by the fix
      round and by the finalize role). Wave 3a is committed locally, `origin/main` is still `0b8d3c5`. Owner: push now with
      R3-1 and R3-2 open, or build G3 and settle the 2-30 rows first. (deviation 8)
      *Updated at wave 3b-1's close: G3 is built and `test_mp_routes` is green (86 of 86, 1,514 checks, 0 failures; 1-11 won), but
      the three verifiers after the route phase (`verify1.json` to `verify3.json`) all returned `pass: false` and the last failed
      one gate, 2-30 (R3-2). Nothing is pushed: 17 local commits are ahead of `origin/main` (`0b8d3c5`), 18 with this close. The
      decision stands with the owner: push waves 3a and 3b-1 together with 2-30 on a stop-gap, or settle it first.*
      *Superseded at wave 3b-2's close: waves 3a and 3b-1 were pushed on 2026-10-05 as `0b8d3c5..00de2b1` (the lead's judgment that the failed gate was an
      owner question and not a defect, DEVLOG session 26), and wave 3b-2 on 2026-10-11 as `00de2b1..53d8db2`; `test_mp_routes` is green at 86 of 86 with 2-30 on its
      stop-gap. The row stays open only as the owner's answer: 2-30 itself is R3-2.*

**WT's recorded deviations** (step 152), each an open row for the step named (planning step 152 has the evidence, `RECORDED DEVIATIONS` 1-9,
and `THE SECOND VERIFIER AND THE COMMIT` the findings of `out/parity/program/WT/verify2.json`; G10-14 above is deviation 9). The routes the new
physics moved are re-authored by the wave's route phase after PIC, CAP and G3:
- [ ] **WT-1. Restitution is decoded and held, not applied** (`materials.json` `restitution_applied: false`; deviation 1). The engine applies a body's
      restitution to a speculative contact - a pair apart and closing within a step's travel (`PhysicsSystem.cpp:1574-1631`) - so a stone of 0.15
      turns round 5.00 units over a floor (`TheEngineBouncesAStoneBeforeItTouches`) where Box2D turns it at the touch; with the decoded values applied
      `test_mp_demolish`'s stone broke no wall and one of `test_mp_pictures`' walks changed. Next: an engine step (restitution at the touch), the tripwire
      case failing, then the flag and the suites that read a touch. Not this repository's to change alone (the pin stays).
- [ ] **WT-2. A heavy body on the light player embeds him in the floor, and a launcher's stone does not kill him** (deviation 2; the second verifier's
      third problem, medium). A stone (1.131) let go over the player (0.104) sank him 37.6 units and held (no fall-through in 240 ticks): **a diagnostic of
      the first build, not kept as a case and not retaken on the final one** (the verifier could not provoke a launcher stone onto an idle player in 1,500
      ticks of 1-23 and 1-24). The original kills a character above `hitIntensity` 2.5; the port builds that for the beholder's rocks only. Next: a case in
      `test_mp_timebase` that drops the stone on the player on the final build and pins the sink depth or the kill (at the next test edit), the stones' kill
      with R23, and the owner's look at 1-23 to 1-26 and 2-29 in play.
- [ ] **WT-3. A walking player cannot push level30's two touching big crates** (deviation 3): 0.538 each, 5 times the player, 8.9 units and a stall;
      a single small crate is pushed at 81 % of the walk. Not measured against the original (no clip pushes a crate), and the floor friction the original's
      pushes also fought is held at 1 (G10-14). Next: the owner's look in play; re-read after the engine's friction step.
- [ ] **WT-4. Twelve entity names have no `.ent` among the extracted assets and keep friction 1 and weight 1** (`moving_platform`, `moving_platform_single`,
      `enemy_killer`, `lava`, `bounce`, `no_restitution`, `collision_128_sqr`, `single_block`, `double_block`, `square_platform_22`, `rocket_bridge`,
      `motor_seesaw`; deviation 4), and **G10-5c(2) stays open**: `Player::HoldTan` still reads `kStaticFriction` 1. Next: their own values where the
      original's data can be found; `HoldTan` from the body under the player.
- [x] **WT-5. The clamp holds every body but at the 720-line reference** (35.6 units a tick; deviation 5; G10-6 above): the original's limit is 100
      screen pixels, fewer units on a taller screen. *Built in step 153 (CAP): the layer tells the sim the screen's pixels a unit.*
- [x] **WT-6. The first build's sweep was re-taken and then replaced** (deviation 6): a control run overwrote `materials.json` for about a minute while it
      ran; every sweep, table and census cited is the fixed build's. Record only, nothing to build.
- [ ] **WT-7. Awake piles never sleep, and 2-20's stacked crate creeps** (deviation 7; the second verifier's seventh problem, low): 32 loose bodies are awake at
      tick 420 against the baseline's 29 (spin up to 1.7 degrees a second, each under 1 unit in 120 ticks), and 2-20's 30 x 30 crate on the big one moves 0.3
      units a second (named in `NoLooseBodyOfAnyLevelCreepsAtRest`; cause not diagnosed). Next: diagnose 2-20's overhanging crate when the engine's friction
      step is done.
- [ ] **WT-8. Statics' restitution and density are held and unused, and `BuildStaticBox` (the dark dragon's platform) builds a body with the engine's
      default material** (deviation 8). Next: with WT-1.
- [ ] **WT-9. The decoded friction is held at 1** (deviation 9): G10-14 above carries its evidence and its next step; the two flags and the tripwire are named there.
- [ ] **WT-10. 2-29 and 3-12 changed levels are only partly explained** (the second verifier's second problem, medium; 2-19 and 2-20 are explained by the original's
      own stills, the verifier's reading: the baseline was the outlier). 2-29: the stone sits on the hinged spinning plank until it tips at about tick 340 and is still rolling at 61 u/s at
      tick 420 at x 209, where the baseline's had stopped at the barrel at 152.6 - a LATER release despite the faster time base, unexplained. 3-12: the crate falls to
      the floor in the first second as at the baseline and the stone leaves the ledge by itself between ticks 120 and 150, before the walking minion arrives, so the
      record's "a minion pushes them against the wall" holds at most for the vibration at tick 420 (corrected in step 152). Next, in CAP or G3: trace the plank's
      tipping time (the stone 1.131 against the plank's own weight and the hinge) and the stone's leaving the ledge (the well's doubled per-frame pull on a body at the
      ledge's edge is the first thing to test); 3-12 does not win now and both routes are re-authored after G3 anyway. *R3b fix 1: 3-12 is explained and fixed. The original's
      stills at 2 and 8 seconds show the stone cradled between the crate's and the column's corners and the crate on its ledge, at rest from the first frame; the engine's
      eight solver iterations cannot hold a stone of 1.131 on two corners over a crate of 0.144 (the pair drifts at equal weights and at a fifth of the gravity too), so
      `game/data/resting.json` puts the two to sleep where the level places them (`sim/Resting`), and a portal's teleport wakes what lay on the traveller (Box2D's own rule,
      `b2Contact.cpp:105-109, 210-214`, which the engine lacks). 3-12 is re-authored on the real start (won on 624). 2-29's plank and stone are still unexplained.*
      *Step 164 (wave 4a): 2-29's route (`level28a`, won 569) runs on the port's stone clock - the library's stills put the original's stone at
      x about 316 at 2.8 s and on the floor at x about 130 at 6.3 s, the port's at x 379 and 250 - so tap 320 and the traversal on 387 are the
      port's (R4a-6); if this row moves the stone's clock, re-fly 2-29 from tap 320.*
- [ ] **WT-11. `LevelBuilder::MaterialOf(registry, "dark_mage")` in `Player::Spawn` (`game/sim/Player.cpp`) is a string literal** (the verifier's sixth problem, low;
      repo rule 3 asks for a constant): a source edit, so not applied by the commit (the G10-13 precedent: it would relink). Next, at the next relink: a named constant, or
      the player's entity read from `player.json`.
- [x] **WT-12. `test_mp_timebase`'s `kStillMoving` strings for 3-12 carry the inference the second verifier's trace contradicts** (`tests/test_mp_timebase.cpp`, the
      comment and the two reasons for `crate_small_ent_1372` and `rolling_stone_1198`: "pushed by the walking minion behind it", "pushed by that crate against the left
      wall"; the comment says INFERRED, not traced). Left, to keep the tested runner current. Next, at the next test edit: say "awake, cause not known" and cite WT-10.
      *Done by R3b fix 1 (`95aee3f`): the two entries are gone, since `resting.json` holds the pile asleep (checked at the wave's close: `kStillMoving` has no 3-12 entry and its comment says the inference did not bear out).*

**CAP's recorded deviations**, each an open row for the step named (planning step 153 has the evidence, `RECORDED DEVIATIONS` 1-11):
- [ ] **CAP-1. The waist of the original's body is filled.** The engine has one collider per entity and no compound, so the two circles of radius 9.1 at +-9 are built as
      their hull, a capsule 18.2 x 36.2; their union is 2.69 units wide at the middle, so a corner at the player's waist can nest in the original's notch up to 7.75
      units and cannot here. Next: an engine step for a compound collider (two spheres on one body), then the capsule becomes them.
- [ ] **CAP-2. The engine's contact skin: the player at rest is 1.6 to 1.8 px (0.6 units) lower than the library stills'.** The engine rests the capsule 0.46 units
      into a floor, 0.55 into a ramp and 0.73 into a small crate; Box2D rests a circle 0.25 units above a polygon (`b2_polygonRadius` less `b2_linearSlop`,
      `b2ContactSolver.cpp:712`), a polygon on a polygon 0.75 above and a circle on a circle 0.25 into it. Measured on 13 flat floors (+1.75, +1.53 to +1.86), a ramp
      (+1.56) and a barrel (+1.58 against the barrel, which is itself 2.95 px low). Every body shares it (the crates' sprites too), so a picture lift for the player alone
      was not built. **The same skin at a wall, and a footage gap that is NOT settled**: 4-01's pillar (face at x 143) stops him at 151.85 (0.25 into the wall), Box2D's decoded
      rest is 152.35, and F17's "about 153" (hood-mask median 432 px / 2.8125; the same mask on the port's frame, `CAP/pillar/4-01_left_f300.png`, has its median 1.9 px
      right of the entity) implies about 152.9: 1.05 units (3 px) more than the port, 0.5 of it the wall skin, the rest inside a hood median's accuracy (the two masks may
      differ; not verified). The old 20-wide capsule matched it exactly (10 + 143). Next: an engine step for a contact margin (`PhysicsSettings` has none), then a recorded
      probe of the pillar stop in the final comparison.
- [ ] **CAP-3. Every level opens with a fall of 3.9 units more, and a level's first quake begins with a back.** The markers stand where the old capsule rested; the
      original's body is 3.9 units shorter and falls to the floor (level30: lands on tick 3, `player_impact` 1.333, a 0.28-unit tremble), and the decoded generator's
      `forth` flag survives a quake that stopped before its back, so the next one starts with the back of that forth. Nothing to build: the original does it; four layer cases
      run one quake out first (`LetTheLandingPass`). Next: the final comparison against a level start's footage.
- [ ] **CAP-4. The sim reads the window's height through the layer** (G10-6): a 1080 window holds a fall at 23.7 units a tick, a 720 one at 35.6, a suite or a headless run is
      at 720. The owner's call whether the port should be fixed at the capture rig's 720 instead.
- [ ] **CAP-5. G10-5c(2) is open, with its scan**: five levels (1-29, 2-11, 2-22, 3-20, 3-29) have a surface between 30 and 40 degrees the decoded floor friction would not hold.
      Next: the original's slide on such a slope, measured, then `HoldTan` from the body under the player (the row above).
- [ ] **CAP-6. Triggers and portals judge the hull.** A death area, a goal, a portal's entry and the other overlap tests read the 18.2 x 36.2 capsule; the original's portal has no
      body (`portal.ent` is `shape="0"`) and tests the player by script, and how it measures his compound is not decoded here: whether the hull's width matches is NOT checked. It moved
      2-12's walk (the entry edge before the pair exists). Next: the portals' entry decode (G3's neighbour), with 2-12's route.
- [x] **CAP-7. The sources are newer than the executables by comment and note edits, and the executables are not relinked.** *Closed by step 154 (G3): its relink compiled them (`MagicPortals.exe` `aa85e2b9`, `test_mp.exe` `a72d460d`, one launch with no argument: 52 of 53 suites pass, the one failure `test_mp_routes` by design).* The commit corrected `game/sim/Player.cpp` (`ETHCompoundShape.cpp: 66-71` is `:65-71`),
      `tests/test_mp_player.cpp` (`GetCircleShape (:41-47)` is `ETHPhysicsSimulator.cpp:56-62`) and four stale "22 over his feet" notes (`game/sim/Portals.hpp`, `game/sim/Puzzle.hpp`, `game/data/keys.json`,
      `game/data/gravitywell.json`: 18.1 since step 153), comments and note strings only, no line added or removed. The four touched objects compile with 0 warnings; `MagicPortals.exe` `106a38c4` and `test_mp.exe`
      `f25cc632` (what the gates measured) predate the edits. Next, G3's relink compiles them: re-run `test_mp.exe` once then, and close this row. `data/player.json`'s `_source` carries the right
      citations since 22:34:06, three minutes into the sweep (keys and numbers identical).
- [ ] **CAP-8. The spec's "a minion's body is not ground" gate was not built** (the listing says the feet count it: G10-3 above). Record only; if the owner reads the original
      differently, the one test that pins it is `AMinionsBodyIsGroundAsACrateAndAPlatformAre`.
- [ ] **CAP-9. The player's friction is the port's 0, not `dark_mage.ent`'s 0.9** (found by step 153's verifier; the spec said "mass and friction"). The mass is decoded (0.104069, step 152) and the friction is not
      applied: `Player::kFriction = 0` stays in code, as since step 3, because a gripping player fights his own steering here, and what the original's friction does (an idle player stands still on a slope) is `Steer`'s hold.
      Next: with CAP-5 and G10-5c, `HoldTan` from the body under him once the original's slide on a slope its friction cannot hold is measured; then the friction can be the .ent's.
- [ ] **CAP-10. Three pins were loosened or re-pinned to the new body, and 4-01's pillar disagrees with footage** (found by step 153's verifier): `test_mp_shot` accepts a seam within 0.5 as either body, `test_mp_layer`'s 1-32 rock
      case asserts `impact.hit` between the last velocity read and the fastest fall (G10-8's equality is gone), and 4-01's pillar pin is the computed 152.1 +-0.6, not F17's 153.0 +-1.0 (port 151.85, Box2D's decoded rest
      152.35, footage-implied about 152.9: CAP-2). Next: restore the strict equality and the footage pin in the route phase (G10-8) or after an engine contact-margin step, and re-measure F17 against the pillar by a
      second method before deciding that the capsule's width is exact. *Step 155 touched route files only: the strict pins are still not restored.*

**G3's recorded deviations** (step 154), each an open row for the step named (planning step 154 has the evidence, `RECORDED DEVIATIONS` 1-8):
- [ ] **G3-1. The spec's "carried by friction" is refuted, not built.** A moving platform is `static="1"`: `CreateBody` makes it a `b2_staticBody` (`ETHPhysicsSimulator.cpp:68`) and
      `linearMotion`'s `SetPosition` is `b2Body::SetTransform` (`ETHPhysicsEntityController.cpp:98-103`), which leaves it no velocity; friction acts on relative tangent velocity, so nothing is
      carried sideways, and what stands on it is pushed out along the contact normal (a rising platform) or falls (a falling one). Its axis is vertical, so it has no sideways travel; `lift.ent` is
      static too. The port carries a rider by the velocity that gets the body there (the player's friction is 0, the platform's held at 1): a rider on 1-11's block keeps his feet within 0.01 unit
      of its top through a swing. INFERRED from the decode: no clip shows a rider, so the original's push-out lag (about 1.5 units into a rising platform at the fastest, Box2D's solver) is
      not compared. Next: a clip of a rider on a platform or a lift.
- [ ] **G3-2. The bucket clock is not modelled, and four movers are affected.** A static entity's callback runs only from the visible-bucket loop (buckets of 128 units intersecting the screen
      plus one ring: `ETHScene.cpp:54`, `:69`, `:541-574`, `ETHBucketManager.cpp:52-90`), and the original's camera opens on `camera_start`, near the exit, and eases to the player
      (`MagicPortalsLayer.cpp:657-665`; the port opens on the player, the owner's choice), so a mover runs while the camera is near it and stops after. With the camera on the player at the spawn four of
      the 18 movers stand outside the drawn buckets (computed from the level files): **3-03's and 3-06's `moving_platform_1011` (x 672), 1-11's `anti_portal_agent_639` (x 674) and 3-09's
      `moving_platform_1429` (x 96, the spawn at x 617)**. The port runs every mover from tick 1 and never pauses it (every static callback is in the same project-wide row: crystals, the ghost, the lifts,
      the handle). Next: one step for the row, with the camera's rectangle in the sim and the opening pan, if the route phase finds the four phases matter; the route phase should know it for those levels: the routes of 3-03, 3-06, 3-09 and 1-11 are authored on the port's convention (every static callback from tick 1), not on the original's phase, and are marked so, so that the owner does not take their timing as the original's (the second verifier's finding, `out/parity/program/G3/verify1.json`). *Step 155 authored 1-11 (835), 3-03 (607) and 3-09 (747) on that phase and marked them (3-09's mark came with R3B FIX 2); 3-06 has no route.*
- [ ] **G3-3. A platform moves before its step, a frame ahead of the original's contact.** The original's callbacks run after its physics step (`ETHScene.cpp:464-490`), so a step meets the
      position the frame before wrote; every mover here moves its body before the step (door, slab, lift). At most one tick's travel (1.15 units); the picture drawn after the tick is the original's.
- [ ] **G3-4. The port carries by velocity, the original by push-out.** A body pressed against a platform's SIDE would be dragged by the port's contact friction where the original's static body
      drags nothing; the player's friction is 0 so he is not, a crate's (held at 1) would be; not measured on any body but the player's. A rider on a DESCENDING platform is carried down by the port and, by the decode, would fall in the original
      (the second verifier's arithmetic, not measured: gravity adds 0.556 unit a tick of speed each tick against the block's fastest descent of 1.17 units a tick, so he is airborne a few ticks of each
      landing). **Next:** a throwaway build that places a platform's body with no velocity every tick (`PlaceKinematic`), the player measured on 1-11's block: if the engine's push-out holds him,
      adopt the original's behaviour; otherwise record an engine gap with the number (a source edit and one relink).
- [ ] **G3-5. The phase is not confirmed by the stills.** The rate and the amplitude are (`out/parity/program/G3/library_fit.md`); a still's zero is the HUD's, not the script clock's, and the fitted
      phases differ by level (0.75 to 1.57 s; 1-06's 1.57 s unexplained). The tick-1-is-a-stride-along phase follows from the decode. *Softened by the second verifier (`verify1.json`, problem 3): its fit of 1-11's t-series stills (t2.0, t4.5, t8.0; block centres
      163.2, 157.2, 187.9 units) gives the full swing at 2.13 u rms with a phase of -0.17 s, about the port's level-start phase (the old swing 18.2), so on 1-11 the phase is confirmed to about 0.2 s; only 1-11
      is fitted that way.*
- [ ] **G3-6. `awakeEntitiesAround` is not built**: the engine wakes what rests on a kinematic body itself (`engine/src/core/PhysicsSystem.cpp:1089-1095`). *Step 163: the decode is in `docs/ethanon-formats.md` (the section on sleeping): the original's three scripted movers wake every non-static body in the 3 x 3 buckets of 128 units round them every frame they move (listing L69717, called from L83504 #321, L84120 #619 and L74234 #17), touching or not; what touches a mover is awake in the port (the engine's kinematic rule, and `Resting::Watch` for what rests within the touch of one outside the engine's contact margin - 4-07's crate, 0.03 u off the bobbing slab - and for what a level placed asleep against one), what is only near is not woken: WAKE-2.*
- [ ] **G3-7. The first call's overlap is scanned for dynamic bodies only**: none of the 11 levels' dynamic bodies stands within 20 units of a platform's tick-1 box; the player's start is not a
      node and was not scanned. A body inside it would be shoved out by the engine's solver, where the original's position solver pushes it over a few frames.
- [ ] **G3-8. `test_mp_movers` carries 9,478 checks, 9,000 of them one velocity check per platform per tick.** Aggregating them is a test edit that relinks the runner (done at the next one).

**The time-base route phase's recorded deviations and findings** (step 155 and step 152's two amendments; the evidence is
`out/parity/program/R3b/`: `verify1.json` to `verify3.json`, `fix1.md`, `fix2.md`, `finalize.md`), each an open row for the step
named. R3-1 to R3-8 above carry the route phase's own (2-30, 2-32's camera, the knife-edges, the 60 Hz); these are the rest:
- [ ] **R3b-1. The held-friction creep is not unique to 2-30: 3-22's stone also creeps** (step 155, deviation 2; the third
      verifier's low). A windowed scan of the all-routes `--trace 1` log finds a body creeping at 0.03-0.6 u a tick for 300 or more
      consecutive ticks in three routes: 2-30 (961 ticks), 3-22's `rolling_stone_1679` (777 ticks from tick 375, x 33 to 83+ at y
      418.4, 0.069 u a tick, after it fell off the level on tick 333) and 1-15's `crate_ent_654` (306 ticks from tick 1, a crate the
      walking player pushes ahead of him: legitimate). 3-22's route does not use the stone after tick 288, so its win is unaffected.
      Next: re-check 3-22 with 2-30, 3-12, 1-09 and 2-07 when the engine's friction step makes `friction_applied` true (G10-14).
- [x] **R3b-2. 3-12's win breaks a slab with a stone that exits a portal at rest touching it** (step 152's amendment, deviation 13;
      step 155, deviation 3; the second and third verifiers' low). The port's `demolish.json` rule is that a stone breaks what it
      touches; the original's `ETHBeginContactCallback_breakable_wall` (#16-33) reads the stone's speed against scale(1.5) and is
      not decoded here, so a stone that appears at rest against a slab might not break it there. The route replays and wins (624);
      the match with the original is at risk, not the replay. Next: decode the callback into the break rule and re-check 3-12, or
      keep it on the owner's list.
      *Step 164 (wave 4a): 1-22's route (`level21`, won 535) also breaks wall 2 with a stone creeping at hit 0.088, below the decoded gate
      (R4a-3); when the gate is built, re-fly it with a fast second stone (`rolling_stone_727`).*
      *Step 175 (WALL, wave 4c-2), 2026-10-15: THE GATE IS BUILT and this row's decode is settled (`ETHBeginContactCallback_breakable_wall`, listing L68341, bytes
      345591..345993: strictly above `scale(1.5)` = 75 u/s, the whole velocity, at the begin of the touch). GAMEPLAY: YES. 3-12's slab-stone exits the portal at rest
      against the slab and reads 0.117 m/s: the slab HOLDS (tick 269) and the committed route's player dies on tick 449. Left open: the route phase (step 176) re-flies
      3-12 with a stone above 75 u/s at the slab, and the speeds the port reads are unmeasured (WALL-3). Not ticked: the row ends in that re-flight.*
      *CLOSED by step 176 (`45f6c05`), 2026-10-15: `level11b.json` is re-flown and wins on tick 636 (was 639; `win_by` 651; medal 2 -> 3): the slab breaks by a FALL, not a
      touch at rest - he goes up to the slab and the pocket stone is dropped through a pair 39 units onto it, reading 3.33 m/s (2.2 times the gate) on tick 408; the knight is
      dealt with by the clock. It rests on a fall of that size breaking the slab in the original too, and on a floor player being unable to place a portal above the slab (the
      route's `_about`): WALL-3 and WALL-5 stay open for the unit.*
- [ ] **R3b-3. `crate_low_emissive`'s held friction of 0.08 is a guess** (the amendment, deviation 10; marked `"_guess": true` since
      R3B FIX 2). Four levels' crates (3-13, 3-03, 3-05, 4-09) are held at 0.08 where every other body is held at 1: a knife-edge in
      the friction (the wedge in 3-13's slot lets go for every release from tick 180 only at 0.079 to 0.082, a combined 0.281-0.286,
      and on isolated ticks at every other value tried) that the engine's contact solve makes, not a property of the level; with the
      decoded combined friction the route's walk reaches the far side on at most one release tick of 75, so the engine's friction
      step alone may not unseal 3-13. Next: drop it and re-fly 3-13 when `friction_applied` goes true (G10-14); the owner's call
      whether it stays until then.
- [x] **R3b-4. `wake_reach_px` (6 units) is a guess, and only a portal's teleport wakes the sleepers** (CLOSED by step 163, WAKE, `f20f0a4`, pushed in wave 4a (`36a52e6..8fe8374`): `Resting::Watch` wakes a sleeper when a body it touches is destroyed, stops being solid or is moved by the port, at the end of `Game::BeforeStep`; `WakeAround` asks the same contacts; `wake_reach_px` is gone for `touch_units` 0.5 (measured: the same wakes for 0.178 to 1.0 on the 86 routes), `moved_m` 1e-4 and the level's `rests_on`; 5 wakes in the 86 replays, 3-10's the only one that moves a route; what stays open is WAKE-1 to WAKE-7) (the amendment, deviation
      11). `resting.json`'s reach is measured to be no knife-edge (3-12 wins on tick 624 for every reach from 2 to the 40 tried and
      not for 1.8 or less; the other 85 routes keep their ticks at 6). A body that leaves a sleeper's side any other way - a
      breakable wall broken, a lift that moves, a crate that burns - does not wake it, where Box2D would (`b2Contact.cpp:105-109,
      210-214`). Next: wake on those events if a level shows the hang (G3-6 covers what rests on a moving platform).
      *Step 161: 3-10 is the level that shows the hang (R3c-5): its knight stays over a broken wall until its hold ends.*
- [ ] **R3b-5. Only one pile is named from a still** (the amendment, deviation 12). 3-12's pile is asleep from tick 0 because the
      library's stills at 2 and 8 seconds show it at rest; 1-30's pile, 2-20's stacked crate and 2-29's stone are awake at tick 420
      (the suite's named list) and are not in `resting.json`, since no still of theirs has been compared. Next: compare those three
      levels' stills (with WT-7 and WT-10).
      *Step 164 (wave 4a): 1-30's route (`level29`, won 228) needs its pile awake at the start - the portal exit (202.9, 183.9) lies inside
      `crate_small`'s column and the solver shoves him 8.7 px on tick 74 (R4a-5); if the original's pile sleeps and the arrival wakes it,
      re-fly 1-30.*
- [ ] **R3b-6. `win_by` is looser than it need be** (step 155, deviation 5; the third verifier's low). 43 of the 86 routes have more
      than 30 ticks of slack and 7 more than 50 (4-21 370 of 440, 2-07 1,126 of 1,186, 4-32 56, 1-21 54, 3-09 53, 2-03 53, 3-27 51;
      slack 9 (4-18) to 70, median 31), and 3-20's second walk is a knife-edge (R3-4). Not touched in step 155: tightening a
      `win_by` changes a file the verifier measured. Next: tighten to the won tick plus 10 to 15 and widen 3-20's second walk to at
      least 3 ticks a side, once the time base stops moving (wave 3b-2's G1, G9, R23 and R22 move routes again).
      *Step 161: 19 `win_by` values changed in the route phase (the 25 re-authored routes carry 14 to 42 ticks of slack) and the 61 untouched ones keep
      theirs (R3c-9).*
- [ ] **R3b-7. The route phase's commit names step 155, not 154, on its Record line** (step 155, deviation 7): the brief's N was the
      last time-base step's number, on the assumption that no step was added, and the commit adds step 155. `af008a9`'s Record line
      names 150 though it adds 151 (the wave 3a table says so). Record only; a message-only amend of an unpushed commit if the owner
      wants it.
- [ ] **R3b-8. The last verification's limits** (the third verifier's `notRun`, and the first verifier's low 6). Zero warnings was
      not re-measured on the final build (nothing was compiled: 0 stale of 262 objects by `ninja -t deps`; it rests on
      `R3b/fix1/build1-7.log`, whose last two links are the current executables); tunnelling was judged by a per-tick displacement
      proxy (at most 19.9 u outside a traversal against the clamp's 35.6), not by an overlap test against each level's geometry;
      2-30's behaviour was not measured against the original and no footage was consulted; the planning steps' footage cross-checks
      and library-still fits were not re-derived; and no fresh build was made, so that a rebuild equals the executables rests on
      timestamps and dependency data. The first verifier's `port_build.bat` run relinked a stale `MagicPortalsSpike.exe` (never
      launched). Next: the final comparison; a verifier that must not relink builds named targets.

**The route phase's recorded deviations of wave 3b-2** (step 161), each an open row for the step named (planning step 161 has the evidence,
`RECORDED DEVIATIONS` 1-5; `out/parity/program/R3c/` the round notes `round1_*.md`, `verify1.json` and `finalize.md` the rest):
- [x] **R3c-1. STK-9 is the verifier's one medium, and no route is near it.** (CLOSED by step 162: the stone rows now read the callback's number, and the 86 replays confirm it on the new build - every stone begin on the player or a knight is the number it was but 3-12's crush of the knight at tick 302, 5.00 to 5.62 against 1.23, none near 2.5, the same 86 win ticks.) The impact watch reads a free fall 0.33 to 0.67 low (STK-9), so the
      port kills from a higher drop than the original. Checked against all 86 routes: every stone-on-player begin reads 0.106 or less (3-13 tick 766
      0.106, 4-21 tick 258 -0.40, 3-12 tick 474 0.0; 2-04, 2-07 and 3-10 about 0) and the only stone begin on a knight is 3-12's crush at 5.0 against
      1.23, so the offset cannot move a route (`R3c/verify1.json`). Not a blocker for the push (the verifier's word) and not text. Next: STK-9's own step
      before anything is shipped (measure a stone on the player, or take the unbraked free-flight arithmetic and re-pin the falls: the step's own
      estimate is that 24 px would read 3.0 and kill, 40 px 4.0; then re-run `test_mp_routes`); until then a re-authored route keeps a stone's begin
      on the player under about 1.5.
- [ ] **R3c-2. 3-07 is won on foot with no taps, by shoving a holding knight** (`level6b`, tick 351, `win_by` 380; the verifier's low). It walks left,
      drops at x 89.6 (about tick 150) behind both lane knights while they face right (until about tick 405) and walks right from tick 160, pushing
      knight B ahead of him along the lane at about 1.7 units a tick. A walk begun on tick 300 is sighted as knight A turns (410) and killed on 456, one
      begun on 430 on 506; starts 215 to about 260 win and every edge of +-3 wins. The push is the port's solid contact with the knight's hold-brake
      (`kBodyFriction` 1): the original's `ETHBeginContactCallback_minion` #12-55 does fire on a non-sensor `dark_mage.ent` begin, so the bodies touch
      there, but that the player moves the knight ahead at that rate is NOT measured. The level's portal puzzle is not exercised. Next: the rig (R3c-11).
- [ ] **R3c-3. 3-22 is won by a rush past its knight, on a knife-edge** (`level21b`, tick 1,219, `win_by` 1,255; R3-4's other half). The knight at wayB0
      sees the player once (tick 1,010) and makes no shot: he runs on without stopping, is within 42 units of it by about tick 1,050 (the 650 ms far gate
      is up on 1,051, so the wait is already the 1,000 ms near one) and touches it on 1,061, which caps its hold, so it walks on pushed ahead of him. A
      pause of 1 to 3 ticks in the last walk gets him shot in all 12 variants tried; the margin is about one tick. A portal hop past the knight was NOT
      tried (no other way past was found; the player cannot jump). It rests on the contact's hold cap (G1a-10, built) and on the push (R3c-2). Next: the
      rig (R3c-11); re-fly it with a margin if the original's hold differs.
- [ ] **R3c-4. A knight lifted off its floor never walks again under G1a's blocked-way rule, and two routes are won around it** (4-28 `level27c` 791,
      4-29 `level28c` 1,023). The way ray to the knight's next waypoint meets the floor it stands on, so it searches (`?`) instead of walking. Both levels
      look designed for the knight to walk on from the portal's exit (4-28: the flame stands over wayA1's x; 4-29: to `button_2037` and the keyhole) and
      neither has another way for the carried key to reach its keyhole. 4-29 pushes the key knight along the floor with the player's body (the knight in
      its hold brake, x velocity x 0.3 a tick; the port's bodies grip, `kBodyFriction` 1, where the original's minion is frictionless, so the original
      would push at least as easily and where it stops would differ); 4-28 burns it with the flame's fireball after the gravity well parks it at (255, 40).
      If the ORIGINAL's knight does walk there, G1a over-blocks and both routes must be re-flown (a knight that walks on its own, or into the flame).
      Next: the rig (R3c-11: 4-29 with a knight sent up by a portal at (400, 60); 4-28 with a portal at (220, 70)).
      *Step 164 (wave 4a): 3-10 (`level9b`, won 1,017) and 3-16 (`level15b`, 1,090) lean on the same rule. 3-10's designed solution (a knight
      carries the key through a portal) cannot be flown because the stone lands on the knight, so the player takes the key himself (R4a-7);
      3-16 sets a knight down on a block, where it stays (it asks on tick 951 and never walks again). If the original's knight walks on,
      re-fly both.*
- [x] **R3c-5. 3-10's knight does not fall when its wall breaks: the sleeping-body wake after a breakable wall's destruction** (CLOSED by step 163 as to the port's gap: the knight is woken on tick 483, its wall's own, and its y is 128.4, 128.9, 130.1, 131.7 on 483 to 486 (`out/parity/program/WAKE/trace1_level9b.txt`); THE ROUTE was open as WAKE-1 and is re-flown by step 164: won on 1,017) (`level9b`, tick 1,435,
      `win_by` 1,460; R3b-4's condition "if a level shows the hang" is met). Knight B takes the key on tick 78, walks the wall (x 256 to 384, y 134 to 166)
      and holds at wayB1 for 15 s at (276.6, 128.4) from about tick 290; the route breaks that wall with its stone on tick 483 and the knight stays
      hanging with its floor gone until its hold ends (about tick 1,215), because it is asleep (`kSleepTime` 0.5 s) and `Resting::WakeAround` wakes a
      sleeper only for a portal's traveller; the original's Box2D wakes it when the contact is destroyed (`b2Contact.cpp:105-109`). The route idles about
      600 ticks; the knight then walks on, falls through the portal at (277, 335), shoots once at the player's old spot (he walks right from tick 1,320
      at 2.4 units a tick and is gone) and its key opens the keyhole at 1,320. A port gap, observed and NOT fixed. Next: wake on a destroyed breakable
      wall (and on a lift's move or a crate's burn, R3b-4), then re-fly 3-10 (the knight would fall at tick 483 and the later timings assume its arrival
      at the keyhole near 1,280).
- [ ] **R3c-6. Three re-authored routes aim with more drift than any earlier one** (the verifier's low): 1-21 5.33 px, 3-12 5.05 and 2-25 4.01, against
      2.4 (1-08) at most in the 61 untouched; the camera moves between a tap's down and up ticks. The driver confirms the shot goes where the finger is
      (0.05 px), so they win; only the point the file names is that far from the real aim. 2-25's tap 313, named (505, 222) and aimed at (501, 222), is 38 px
      from the reflector against its catch of 32 and works only while the camera still leads the player by about 30 units (a tap on 316 or later is
      inside the 64-unit walk pad and fires nothing); a camera retune or a change of the 400 ms between taps breaks it. Next: move the tap's ticks to a
      pair where the camera is still, or write the real point in the file, when the route is next touched.
- [ ] **R3c-7. 3-02 (`level1b`) wins on 400 where it won on 388** (inside its `win_by` 420; the one untouched route that moved). It moved under steps
      156-160 and the cause was not isolated to one step: its run has one knight sighting, at tick 377, and no kill; G1a-9 INFERS +13 (to 401) from
      the stand and G1b-8 INFERS one tick back from the contact's hold cap, with no cap-less retrace. It no longer unlocks 24. Next: retrace once with
      `contact.hold_cap_ms` very large if it ever matters.
- [x] **R3c-8. The stealth achievements are shown by one unseen route and two seen ones.** (CLOSED by step 164: the four routes of wave 4a play
      them - 3-04 unlocks 26 on tick 297 (no taps, both knights alive), 3-05 unlocks 27 ('four portals or less') and 28 (never noticed) on 1,136, 3-11
      unlocks 32 on 215 (two shots, no kill) and 3-24 unlocks 42 on 142 (the ring hop, never seen); the four runs' sound lines hold no `enemy_spotted`
      and no `enemy_shot`, the rule being `unnoticed N` = exactly N minions alive and a spotted count of 0, `Achievements.cpp:296-298`.) 4-30's unseen
      route unlocks 76 at the door; the seen routes of 3-02 (one sighting) and 4-25 unlock none; 26, 28, 32 and 42 sat on 3-04, 3-05, 3-11 and 3-24,
      which had no route, so they were pinned only by `test_mp_layer` and `test_mp_achievements` against a fake world.
- [ ] **R3c-9. `win_by` is still looser than it need be in the untouched routes** (the verifier's low; R3b-6 continued): 4-21 70 ticks, 2-07 60, 4-32 56,
      2-03 53, 2-14 50 (slack 9 to 70, median 30 over the 86; the 25 re-authored 14 to 42, 19 with a new `win_by`); the thin ones are 4-18 (9), 4-02
      (13), 3-13 (14) and 4-08 (15). Not touched: tightening one changes a file the verifier measured. Next: tighten to the won tick plus 10 to 15 when
      the rules stop moving (R3b-6).
      *Step 164 (wave 4a): the 42 new route files carry `win_by` = won tick + 11 to 15 (3-10, re-flown: 13); the 85 older files are untouched, so
      this row stands for them.*
- [ ] **R3c-10. Routes lean on rules no clip shows** (planning step 161, `RULINGS APPLIED`): 1-12 and 2-11 use the reflector's leg as the level's own tool
      (G9-4: the listing's arithmetic, no reflected shot is filmed); the taps of 2-17, 2-19, 3-13 and 3-15 whose ray ends "after the head-height retry"
      (R23-9's residual risk: the retry now moves shots); 3-15's entry portal is a 3-unit aim (x 194 to 196, y 88 to 95) inside `anti_sight_wall_ent_1699`;
      3-12 stands at x 45.2 out of its stone's way on STK's rule (a player from x 47.6 right is struck) and unlocks no achievement; 4-30 toggles the torch
      twice more than its old route; 3-07 and 3-22 are R3c-2 and R3c-3; and every route waits for a burning or falling body to go before the tap whose ray
      would end on it (2-10 until tick 321, 2-19 338, 1-23 375, 3-13's corridor portal tap 836). Next: the final comparison and the rig (R3c-11).
- [ ] **R3c-11. Rig checks on the original, one list** (the finalize's "left for later"): 3-07's shove of a holding knight (R3c-2); 4-28 and 4-29 with a
      knight sent up by a portal, does it walk (R3c-4); 3-22's rush, and a portal hop past it (R3c-3); 3-10's knight when its wall breaks (R3c-5); a retry
      tap's end (R23-9) and two taps within 400 ms of an hourglass tap (R23-10); a stone against the player (STK-1); a `!` and a `?` (G1a-8). All need the
      emulator rig (`docs/parity.md`); none was run in this wave.
- [x] **R3c-12. `MagicPortalsSpike` was stale in the default target** (the verifier's low). CLOSED inside the route phase: the verifier's `port_build.bat`
      recompiled `spike/main.cpp` and relinked `MagicPortalsSpike.exe` (`R3c/verify1/build.log`: 2 steps, rc 0, no warning line; never launched, so no Smart
      App Control event), and no source has changed since (a `ninja -n` at the wave's close shows only the CMake glob re-check). The steps build named
      targets only.
- [ ] **R3c-13. The last verification's limits** (`R3c/verify1.json` `notRun`; R3b-8's kind): a forced rebuild of `MagicPortals.exe` and `test_mp.exe` was
      not done (they were current, so zero warnings for them rests on the implementers' build logs; only MagicPortalsSpike's build was the verifier's
      own); the full ctest was not run (the brief forbids it: one launch of the one runner instead); there was no comparison against the original in the
      emulator rig and no visual read of captures (the sweep was compared byte for byte, not looked at); and the verifier read `origin/main` from the
      local tracking ref (the finalize fetched before the push: it read `00de2b1`). Next: the final comparison.

**Step 162's recorded deviations** (planning step 162 has the evidence, `RECORDED DEVIATIONS` 1-8; `out/parity/program/STK9/` the decode, the arithmetic and the census):
- [ ] **STK9-1. The player's, a knight's and a crate's begins keep the history's reading.** The same early braking offsets them by one to two steps. The player's gravity is the layer's own (`useGravity` is off, applied in `Player::Steer`, so the replay has no gravity step to add) and his landing shake was matched to F14 on the history's reading (step 104: 8.4 against the footage's 5 px); a knight is steered and a crate pushed. Next: hand each its own gravity step (the layer knows them), set `Watched::freeFlight` for them, and re-run F14's landing and the crates' sounds; the Box2D reading is the same (a player or a knight against a floor is circle against polygon, a crate polygon against polygon, two skins).
- [ ] **STK9-2. A stone that breaks a wall is read from the history.** `Watch::Carried` serves `Demolish`'s strike (`latchImpacts`): the wall's body has gone before the watch can see it, so there is no shape to replay against. Its shake and hit sound keep the offset (not the kill, not the crush). Next: hand the replay the breakable's box (`Demolish::Breakable::box`) as the other. *Step 175 (WALL, wave 4c-2): NARROWED, not closed.* A stone whose wall breaks on the first look still has its shake and sound read from the history by `latchImpacts` (the wall is gone before the layer's own watch sees the pair); one whose wall breaks on the second look (3-13) is read by the layer's watch itself (WALL-9).
- [ ] **STK9-3. The begin still comes when the engine lets the stone arrive.** The reading is the original's; the TIME of the kill and the crush is the port's, up to eight ticks after the original's where the staged braking parks the stone (STK-3's kind; step 104's deviation 3).
- [ ] **STK9-4. The replay is judged against where the other stands now, and two of its judgments are guesses.** A walking other is allowed its travel in the reach test (its velocity into the step) but the replay's touching test uses its current shape: an error of its travel over the replayed steps. The two judgments that sort the engine's steps into "this pair's brake" and "something else" are **guesses, now in `game/data/earthquake.json` `impacts.free_flight` (`_guess: true`)** - the verifier's one medium of round 1: they were `kBrakeReachSlackPx` (1 px) in `Impacts.cpp` and a friction coefficient of 1 written into a comparison. `brake_reach_slack_units` (1.0 level unit, not screen px) is what a change of velocity may stand past the engine's own reach (`PhysicsSystem.cpp:1164`, the two bodies' sweeps; the replay's reach is the stone's travel after gravity and the other's CURRENT speed over the steps since, an estimate), and `brake_friction_cone` (1.0) how much of that change may lie along the surfaces per unit taken out of the approach. `AFreeFallIsReadAsTheOriginalsStepReadsIt` holds the slack's lower edge (its stop at step 5, a gap of 3.43 against a reach of 2.78, is a brake only with more than 0.65) and lets each guess decide one case. **Measured (`fix1/sens_report.txt`, `sens_report2.txt`): over the 86 route replays and `test_mp_layer` the readings are identical for a slack of 0 to 1.5 and a cone of 0.5 to 1.0** (a slack of 2 or 4 moves one begin, the old 70 px knight case on a still-settling knight, 2.68 to 4.15; a cone of 0.25 moves two, 1.5 or 2 two, and 0 about 45 and five checks of the layer), so 1.0 / 1.0 is inside a plateau - and nothing measured on the original fixes either. `kFreePositionPx` (0.01 px) and `kFreeVelocityPxS` (0.05 px/s) stay in `Impacts.cpp`: the float state of the engine's integration, which is no game rule. Next: retune only against STK-1's capture.
- [ ] **STK9-5. A circle pair's free flight that stops 0 to 0.178 u short of touching is read one step early.** The begin is found within `touch_px` of any pair, as before; for two circles the original's skin is none, so a flight whose free position stops in that band is read on the begin's own step, one quantum (0.333) low. The table's drops keep 0.3 u clear of every step.
- [ ] **STK9-6. The beholder's rocks' kill is Boss's and their shake and sound are the watch's** (STK-2 widened): the two estimators differ by up to a quantum more than they did; the two thresholds are still equal (`boss.json` `rock.crush_intensity`, `demolish.json` `stone.kills_player_above`).
- [ ] **STK9-7. Not measured on the original** (STK-1 and G1b-6 stay open). The reading is the listing's and Box2D's arithmetic, agreeing with the port's engine on 15 falls (nine over the player's head, four onto a floor, two onto a settled knight) and 61 stone begins in the route replays, not a capture of a stone landing: no footage shows one. **The knights' crush edge (a drop over 3.33 u) is played now** (`AStoneThatFallsOnAKnightReadsTheFreeFlightAndCrushesAbove1Point23`, round 1: a stone let go 2.5 px over a settled knight reads 0.9885 and crushes nothing, one let go 4.5 px reads 1.3105 and crushes it, each k / 3 times the vertical share of the line between the entity points, 0.9885 and 0.9829), **on the port's knight, one 7 px circle against the original's two of 9.1**, so the crown the stone meets stands lower in the port than in the original. A reading can now be LOWER than the history's (3-14's stone on `platform_low_emissive.ent`, 2.96 to 1.12; 4-21's on a gravity agent, 2.20 to 1.21): the original's `along` is the line between the two entity points (C55) and leans against the fall there (3-14's is (0.94, 0.34)), **and 3-14's old maximum was a stale sample**: the stone's speed some ticks before it glanced a ramp (tick 533: its sideways speed went from +1.3 to -2.3 u a step), which no contact the watch counted cleared from the history; the callback reads the velocity AT the touch (1.0 to 1.1 along that line, the replay's 1.12). A later reader must not retune the `along` rule to the lean alone.
- [ ] **STK9-8. A push NEWER than the oldest brake is not looked at, and a gravity well's pull is not in the replay** (the verifier's low of round 1). `FreeFlightVelocity` scans a stone's history oldest to newest, stops at the first brake and flies the stone free from there with the engine's gravity alone, so anything that moved it after that brake - a gravity agent's pull (4-17, 4-18, 4-19, 4-21 and 4-28 place them, and only 4-21, `level20c`, also has a stone: the verifier's grep over `out/levels`), a glancing contact the watch never counted - is replayed away. 4-21's stone at tick 159 (pulled toward `gravity_agent` (460, 36) at 1 to 4 u a step, then braked) reads 1.21 where the history read 2.20; neither is checked against the well's script. No route is affected (the same win ticks) and none is near a kill. Next: give the replay the well's pull, or fall back to the history for a stone inside an agent's radius, once a capture or the well's script says which reading the original has.

**Step 163's recorded deviations** (planning step 163 has the evidence, `RECORDED DEVIATIONS` 1-8; `out/parity/program/WAKE/` the census, the sweeps of the numbers and the traces):
- [x] **WAKE-1. 3-10's route no longer wins: the knight it needs falls to the stone's landing and is crushed** (CLOSED by step 164, `8fe8374`: 3-10 is re-flown and won on 1,017 (`win_by` 1,030, was 1,435) as a different solution - two portals, the player takes the key himself from the knight the stone crushes - because the designed one, a knight that carries the key through a portal, cannot be flown: the stone lands on the knight in every variant tried; that is R4a-7 and R3c-4.) (`level9b`, `win_by` 1,460; step 163's route census). With the knight woken on tick 483 (the wall's break) it falls from 484, lands on `ramp_left` on 515 and the stone the route broke the wall with comes down on it on 519 (`minion_smashed`, the key on the ground at (277.8, 314.1)); everything the route does after tick 483 assumes the knight hangs until about 1,215 and reaches the keyhole near 1,280 (the portal at (277, 335), the keyhole at 1,320, the walk from 1,320). The first thing wrong is the crush on 519. Next: the route phase re-flies 3-10 on this build (the stone's landing must miss the knight, or the knight must be brought to the keyhole alive another way; the wall portal's x scan in `R3c/round1_level9b.md` finding 3 is the starting point).
- [ ] **WAKE-2. The original's bucket wake (`awakeEntitiesAround`) is not built** (G3-6). A scan of the placements finds a loose body in the 3 x 3 buckets (128 units) of a lift, door lift or moving platform in 26 levels: 1-15, 1-16, 1-27, 1-28, 1-31; 2-06, 2-07, 2-08, 2-12, 2-15, 2-17, 2-18, 2-24, 2-25, 2-28, 2-29, 2-31; 3-05, 3-07, 3-10, 3-14, 3-15; 4-07, 4-19, 4-22, 4-31. The original wakes it every frame the mover moves; the port wakes what touches the mover and leaves what is only near asleep on whatever holds it. No route shows a difference; a pile that the engine's solver cannot hold (R3b-5's kind) next to a mover would. Next: build it (a distance of the mover's bucket, from `Level::built` and the mover list) only if a level shows it.
- [ ] **WAKE-3. A body that arrives beside a sleeper without waking it is not looked at, and the record of what a sleeper touches is made once.** Box2D wakes both bodies of a new pair (`b2ContactManager::AddPair`, `b2ContactManager.cpp:289-290`); the engine wakes the sleeper only when the body that arrives is moving at 0.05 m/s or more, or is kinematic. A body that creeps into contact with a sleeper more slowly is not in its `Supports`, so it going later wakes nothing. Not seen in the 86 replays (205 looks). Next: refresh a sleeper's record when something is within the touch of it (cheap: the watch already visits every sleeper each tick) if a level shows it. **The verifier's low (round 1), reasoned from the code and not reproduced: a sleeper first seen after its support went in the same frame is never woken** - the frame is the engine's step, then `AfterStep` (a wall breaks), then `BeforeStep` and the watch, so a body that dozes off in the very step before its wall breaks is first scanned with the wall already gone, records nothing to lose and hangs. Not seen in the 86 replays or the 216 looks. Next: run the scan half of `Watch` once more right after the physics step, before `AfterStep` (it visits only sleepers without a record), the wake half staying where it is.
- [ ] **WAKE-4. An awake loose support is the engine's.** `Resting::Watch` keeps an awake loose body's place up to date and wakes what rests on it only by the engine's stirring rule (0.05 m/s); Box2D keeps the whole island awake while any body of it is. A support creeping away under 0.05 m/s for up to half a second would leave its sleeper hanging until the support sleeps, and then nothing wakes it (the support's place was refreshed while it crept). Not seen in the 86 replays. Next: wake on an awake support's travel past `moved_m` between looks if a level shows it (the run at `touch_units` 8 shows what a wider net costs: 18 wakes more). **The verifier's low (round 1), reasoned from the code and not reproduced, the same hang through the one support a level names**: `Resting::Watch` only refreshes an awake support's place, and 3-12's named crate (`rests_on`, 1.95 u from the stone, so no engine contact ever forms) pushed off by anything but a portal (which goes through `WakeAround`) slides away and leaves the stone asleep in the air, as 3-10's knight hung; the route and the census never push it. Next: for a support the level names, compare its place with the recorded one even when it is awake loose (and refresh only after waking), with one test: a named awake crate moved by more than `moved_m` wakes the stone.
- [ ] **WAKE-5. The port's touch is a distance, `touch_units` 0.5, measured on the 86 replays and not on the original.** Box2D's is a manifold within the shapes' radii (0.178 u a circle against a polygon, 0.356 u two polygons, none two circles, at the original's 720-line screen, `resting.json` `_touch_units`); the port's engine rests a body overlapping its support by about 0.39 u. The same wakes for 0.178 to 1.0; 2.0 and above add wakes of bodies merely near a mover. The one sleeper the level places (3-12's stone, 1.95 u off its crate) has its crate named in `rests_on`, which is the still's reading. Next: the other three piles R3b-5 names, if they are put to sleep, need their own `rests_on`.
- [ ] **WAKE-6. A support that is turned is not watched** (nothing in the port turns a static or kinematic body after the level is built: `Hinge` and the seesaws turn dynamic bodies through the engine's joints), **and `moved_m` is the engine's own distance** (1e-4 m: the same wakes at 1e-5; at 1e-3 the 4-07 slab's slow bob no longer wakes the crate on it). Next: nothing, unless a mover that turns is added.
- [ ] **WAKE-7. No test builds the real lift, door-lift or platform scripts over a sleeper, kills a knight under one through `Minions::Take`, or runs `Launchers` and the boss's crushes** (the verifier's low 6, round 1). `test_mp_wake` drives `Mover::MoveKinematic` and `PlaceKinematic`, the primitives those scripts call, and `Demolish`, `Fire` and `Keys` for the removals; the rule is generic (nothing in it depends on who destroys or moves), and the live cases are 3-10's wall and stone, 2-24's and 2-32's removals and 4-07's bobbing slab. Next (optional, at the next edit of `tests/test_mp_wake.cpp`): one test per mover kind that builds the real `Mover::Door`, `Lift` and `Platform` state and ticks it over a sleeper, and one that kills a knight under one.

**Step 164's recorded deviations** (the route phase of wave 4a; planning step 164 has the evidence, `RECORDED DEVIATIONS` 1-11; `out/parity/program/R4/` the
census files, the round notes `round1_*.md`, `fix1.md`, `verify1.json`, `verify2.json` and `finalize.md`; none blocks, and each route states its own in its `_about`).
**No level is without a route and no `finding_*.md` exists, so there is no row for an unwon level.**
- [x] **R4a-1. 1-19 and 1-20 win because the destroyable stone survives the wall it breaks** (`level18` won 271, `level19` 596). The original's `rolling_stone_728`
      carries `destroyable` 1 and dies at the first breakable wall it breaks (`ETHBeginContactCallback_breakable_wall` #42-60: Order 11.1's first clause, an owner row, not
      built); the port keeps it, so 1-19's third pair and 1-20's single stone for both walls exist only because of that (`R4/note_1-20_embedded_exit_trace.txt` traces 1-20's
      exit). Next, when that clause is built: 1-19 drops its last two taps (173, 208) and its second walk half; 1-20 is re-flown from tap 232 on with the level's second stone
      (`rolling_stone_1048` on plank 1058).
      *Step 175 (WALL, wave 4c-2), 2026-10-15: THE CLAUSE IS BUILT. `rolling_stone_728` dies at the first wall it breaks (1-19: wall 731 on tick 109 at 4.000 m/s, the
      stone destroyed at (450, 236.1); 1-20: wall 1061 on tick 159 at 2.667 m/s, destroyed at (539.9, 130.2), and wall 1056 stands) and both routes no longer win (1-19
      not won by 285, 1-20 by 610). The re-flight is the route phase's (step 176); the row closes when the re-flown routes win.*
      *CLOSED by step 176 (the route phase, `45f6c05`), 2026-10-15, with one cost: both routes are re-flown and win. **1-19** (`level18.json`) drops the third pair and the second
      walk half (6 shots -> 4): the stone breaks wall 731 at 4.0 m/s on tick 109 and dies, and the route wins on tick 184 (was 271). **1-20** (`level19.json`) uses both
      stones, one per wall (10 shots, was 8): the first breaks its wall at 4.67 m/s on tick 277 and dies, the second is fed through a pair to the other wall (3.10 m/s, tick
      584), and the second stone is reachable only from the block's top; it wins on tick 825 (was 596). **No route now unlocks achievement 48** (1-20, "8 portals or fewer": the
      old route had one stone break both walls, which this row's rule took away): R4c2-1.*
- [ ] **R4a-2. 4-10 and 4-12 rest on a wall-slide the port's friction-0 player makes** (`level9c` 329, `level11c` 311). `Player.hpp` `kFriction` is 0 where `dark_mage.ent`
      has 0.9. 4-10 climbs a block face at 42 and 36 u/s (taps 4 to 6) and says plainly that it is a KNIFE-EDGE (taps 1 and 2 move together -4 to +2 only, taps 5 and 6 are not
      re-aimed); 4-12's tap 4 rises along a plat's face at 110 u/s for 28 ticks. Not measured on the original (`zerog.json` is silent). Next: measure a body kicked along a wall
      in a zero-g room on the original (R3c-11's rig), then re-fly both if the rise decays.
- [x] **R4a-3. 1-22 breaks wall 2 with a stone creeping at hit 0.088, below the decoded gate** (`level21` 535). `ETHBeginContactCallback_breakable_wall` #16-33 needs a
      stone faster than scale(1.5); the port breaks a wall on touch (R3b-2), and the tick-434 arrival moves 400 to 446 under a 1-tick change of any earlier input (the held-friction
      creep, R3-2). Next: when R3b-2's gate is built, re-fly 1-22 with a fast second stone (`rolling_stone_727`).
      *Step 175 (WALL, wave 4c-2), 2026-10-15: THE GATE IS BUILT. 1-22's first stone (753) breaks wall 719 at 3.162 m/s on tick 308 and is destroyed; the second (727)
      reads 0.119 m/s against wall 755 on tick 356 and HOLDS, so the route no longer wins (not won by 548). The re-flight with a fast second stone is the route phase's
      (step 176); the row closes when it wins.*
      *CLOSED by step 176 (`45f6c05`), 2026-10-15, WITH A LIMIT: `level21.json` is re-flown and wins on tick 666 (was 535; `win_by` 678): the stones coast off the platform, the first
      breaks wall 719 at 3.17 m/s on tick 318, and the second is put through a pair on the ramp, its velocity inverted, and breaks wall 755 at 3.28 m/s on tick 581. The route
      rests on two things the original is not shown to have: **a stone put at a portal against a breakable wall breaks it, and stones coast at about 0.21 u/tick once the player
      stops pushing** (observed, cause not established; G10-14's friction step would re-time it): R4c2-2.*
- [ ] **R4a-4. 1-24's door is reached only by a drop onto it over a kill box, and the reachability result does not cover the box** (`level23` 343). `destroyier_ent_1051`'s
      16 px box lies across the door's trigger and kills a walking player on tick 366 (STK-8: no listing callback makes a destroyier kill a player, the owner's testimony says it
      does); the route lands from above with his feet 5 units over the box (`R4/portgap_1-24_destroyier.md`). **`reachability.py` has no destroyier term, so 108 / 20 / 0 does
      not speak to this box.** The route wins either way. Next: STK-8's ruling; if the owner says it does not kill, the plain walk opens.
- [ ] **R4a-5. 1-30 wins through the solver pushing him out of a pile he arrives inside** (`level29` 228). The portal exit (202.9, 183.9) lies inside `crate_small`'s column and
      the solver shoves him 8.7 px on tick 74 (the only unexplained one-tick displacement of a new route), after which the pile collapses; the route needs the pile awake at the
      start (R3b-5). Next: if the original's pile sleeps and the arrival wakes it, re-fly 1-30.
- [ ] **R4a-6. 2-29's stone, and the taps that wait for it, run on the port's clock** (`level28a` 569). The library's stills put the original's stone at x about 316 at 2.8 s and
      on the floor at x about 130 at 6.3 s, the port's at x 379 and 250 (WT-10: the plank and the stone are not explained); tap 320 and the traversal on 387 are the port's. Next: if
      WT-10 moves the stone's clock, re-fly 2-29 from tap 320.
- [ ] **R4a-7. 3-10 is a workaround for a port gap** (`level9b` won 1,017, was 1,435; closes WAKE-1). The designed solution (a knight carries the key through a portal) cannot be
      flown: the stone lands on the knight in every variant tried and crushes it 4 ticks after it lands, and a lifted knight never walks again (R3c-4); the player takes the key
      himself from the knight the stone crushes, with two portals. 3-16 sets a knight down on a block under the same rule. Next: R3c-4's rig check; if the original's knight walks on,
      re-fly both as designed.
- [ ] **R4a-8. Routes that rest on port behaviour not measured on the original** (each says so in its `_about`; the final comparison takes them, the rig list is R3c-11's): 1-13
      (it passes under the rising lift door with 18 units of head room, the port's door body), 1-17 (the pair's entry is an edge in the port, the original asks
      `isThereAnythingInPortal` every frame), 1-25 and 1-26 (a stone stopped 9 to 13 units short of a surface by the engine's contact brake), 2-27 (G9's 32-unit catch: no footage of
      a reflected shot), 2-28 (capsule step-ups and the hinge), 3-16 and 3-23 (a knight carried through a portal, a key opening by position), 4-26 and 4-27 (a knight's shot ends
      where he stood, the wall rebuilt behind him), 3-30 (he stands on a burning crate for 63 ticks unhurt), 3-01 (the knight's cone, read from the geometry) and 3-05 (the knights'
      sight and facing, the tap-time ray, the reflector's catch and the door's stride: a sighting would end its claim to achievement 28, not its win). Tight ones in their own notes:
      3-25's hop (entry on the key's tick + 3, the last before the ring kills) and 3-28's stand under a swinging ring (10 ticks only). 3-26 is sighted on tick 206 as he drops into
      the knight's row, which cannot be avoided on that level, and no achievement unlocks there. Next: the final comparison; none changes a win now.
- [ ] **R4a-9. Two older routes, unchanged, shake once after a portal exit**: 2-30 tick 159 (14.8 px, no portal exit on that tick: the R3-2 creep) and 2-25 tick 387 (13.3 px, one
      tick after the 242.6 px exit of 386), the 1-30 kind of solver shove; both win on the ticks of steps 161 and 163 (1,386; 538). The larger one-tick jumps of 1-01 and 1-15 are the
      levels' own static portals. Next: 2-25's with 1-30's (R3b-5); 2-30's is the owner's call (R3-2).
- [ ] **R4a-10. No test compares the shipped `resting.json` `touch_units` and `moved_m` with `test_mp_wake`'s rig** (its lines 72-73 and 369-370 hard-code 0.5 and 0.0001;
      `test_mp_timebase` pins the parser with 2.0 and 0.001 only). Next: one check against the loaded file when WAKE-7's tests are written.
- [ ] **R4a-11. The first build after the wave re-runs CMake** (its glob sees the 42 new route files) and may relink the runner: one Smart App Control risk. Nothing here needs a
      rebuild; the next real source edit takes it.
- [ ] **R4a-12. The last verification's limits** (`R4/verify2.json` `notRun`; R3c-13's kind): no rebuild and no re-measure of zero warnings (the executables were newer than every
      compiled source; the claim rests on the WAKE and STK9 build logs, 0 `warning C` and `warning LNK` lines); no full ctest (one launch of the one runner, and two route-only
      launches); no comparison against the original in the emulator rig; no capsule-level overlap proof (the tunnel audit is the player's centre against static polygons and rectangles,
      removable and moving bodies skipped by name; it hit 7 routes and none is a tunnel: new 4-12 and 4-27 and old 2-11, 4-29, 4-30 and 4-31 with the body gone or broken, and 2-25
      one tick at depth 0.0 on a seam); no route was perturbed by the verifier (the margins are the authors' own); `game/data/resting.json` is newer than the executables (06:41,
      notes only: step 163's statement, not re-derived); `R4/verify2/saves/` holds 128 scratch save copies (JSON, no images; the verifier's bulk remove was blocked and the close did
      not delete them: its disk rule is for images). Planning steps 162 and 163 still say "committed locally, not pushed (the hash goes in at the wave's close)": the hashes are
      `11d0be4` and `f20f0a4`, pushed as `36a52e6..8fe8374`, and the doc is left as the commits wrote it. Next: the final comparison.

**Step 165's recorded deviations** (the pixel-centre rule at the frame's edge, wave 4b; planning step 165 has the evidence, `RECORDED DEVIATIONS` 1-10; `out/parity/program/PX/` the reports and `verify1.json` to `verify3.json`, the three verifications; GAMEPLAY: none; the row "Step 145's recorded deviations" above is partly closed by it):
- [x] **PX-1. Settled history** (deviations 1, 2, 3, 5 and 8). The first build, made on demand, failed gate 3 (133 px outside the ring on five levels) and was replaced by
      the pool build; round 1 reported "suites not run" where they pass, and the level walk failed on two causes in turn (a hidden strip keeping its picture's lightmaps:
      fixed in the code; a strip's ids a frame behind its picture's in the harness: fixed in `LevelVisit::measure`); the fix's game gates were first measured by the
      second verification; the complement is on as shipped, measured on the game; the second verification's own disturbances (the JSON toggled in place and restored, its
      window minimised twice, 2-02's stray shot). Nothing is left to do.
- [ ] **PX-2. A picture that goes on inward from an edge in the pixel's inner half stays an MSAA blend** (deviation 6; the "slivers"). 1,631 `edge sliver` log lines in
      113 of the 128 route logs and 206 in 35 sweep logs (3-03 ticks 280-287, 3-14 frames 604-607 and 3-16 frame 44 among them). A strip adds samples and a cull takes a
      whole quad, so neither takes those samples away; no f420 pixel and no pixel outside the ring is affected. Next: the final comparison attributes the pixels (it did not: row CMP-R1); if a
      later step wants them, move the sliver's quad by a fraction of a pixel with its uv carried (its own byte-identical gate), or give a strip the picture's own gather
      position so it stops losing same-depth ties to its neighbour.
- [ ] **PX-3. 1-12's right columns at frames 113-115 are unattributed, and the code does not move them** (deviation 6). They are byte-identical with the switch off, the first half
      alone and the complement, and the log names no picture with an edge in the band there. The third verification reads them as scene content, a soft dark band sliding
      in under the horizontal shake (column means 69, 66, 56, 41 across columns 1276-1279 at frame 113): INFERRED, not proven. Next: the final comparison (it read stills at fixed ticks, no tremble or pan: CMP-R1); gate 4 was read
      against the rule, never against the original's own tremble or pan footage.
- [ ] **PX-4. Of step 145's deviation 1 only 2-05 is closed; 2,047 edge rows above 12 remain** (deviation 6). Left and right 614 (777 before), all four sides 2,047 (left
      260, right 354, top 515, bottom 918; 2,210 before); 398 have the blend's signature (left 83, right 82, top 62, bottom 171). The rows the backlog named (1-21 and
      1-23 right, 4-01 left, 4-09 right) are single 5/6/5 steps with equal inner pixels, not a quad edge in the band; the first suspect is a sub-pixel difference in where
      a picture's edge falls (INFERRED, not measured). Next: the final comparison groups the rows by cause. *Wave 4b: it did not (CMP-R1): the readers' tools are blurred difference maps and boxes on the pair images, which do not show a one-pixel column's 5/6/5 step (INFERRED); the rows are the next measuring pass's.*
- [ ] **PX-5. Two pixel-neutral effects, no code change** (deviation 7). A strip loses a same-depth, same-key tie to its neighbour because the pool stands first in the
      stores (3-14 frames 604-607: no pixel changes), and a quad whose edge sits at the frame's boundary within 1e-5 px is culled every frame (2-23: 633 cull lines per
      bar), which no pixel shows. Next: a guard that leaves such a quad alone, only if a later step wants the log quiet.
- [ ] **PX-6. The rule always runs against 1280 x 720 in the shipped game** (deviation 9; the third verification's low 1). The frame is 1280 x 720 unless a `ViewportInfo`
      says otherwise and nothing in `game/` sets one (the editor does), so at 1920 x 1080 a strip still lands (columns 1918 and 1919 equal) but the band, the first sample
      and the cull's outer pixel are 1280-pixel units: a bar's edge 0.6 real pixels in would be culled though it covers its pixel's centre. A flipbook on its own clock is
      read at the component's frame (no level has one). Only 2-05 was captured at another size, and no original frame exists there. Next: when a step touches other window
      sizes, read the window's real pixel size; nothing at 1280 x 720 moves.
- [ ] **PX-7. What the verifications did not run** (deviation 10; the third verification's `notRun`). The peak number of strips on one tick against the pool of 24 (the
      second verification's "at most 10: 3-21 tick 8 and 3-15 tick 106" stands), a strips-off A/B for 1-12, 3-03 and 3-19 (the switch is read from
      `game/data/edge_cover.json` by absolute path and `--data` moves only Enhanced's `data/`, so a clean A/B needs a launch option for the port's own data directory),
      pixel parity at any window size but 1280 x 720, 112 of the 128 route replays through the window (the runner replayed all 128; round 1 and the second verification
      did all 128 through the window) and the first frame a lightmapped strip is drawn (3-14 tick 6, under the fade-in). Next: the launch option, if a later step needs an
      A/B.
- [ ] **PX-8. The pool's size and its explanation are the port's own, and `edge_cover.json`'s notes were edited after the gates** (deviation 4). The pool of 24 is a
      `_guess` (the most any replay needed was 10); why a late strip moved 133 px (EnTT's swap-and-pop moving the renderer's gather-order tie between one particle
      system's draws) is INFERRED, the gather order was never dumped; `turn_rad` is marked `_guess` too since the third verification's low 5. The file's `_` notes were
      edited after the second verification (md5 `4b313617`, the file every gate measured) and again by the commit (`d7484711`: the `_about` frame sentence and
      `turn_rad`'s `_guess` line); the non-underscore keys are equal throughout and no launch followed the last edit. Next: dump the gather order if the pool is ever
      questioned; the next launch of the runner or the game reads `d7484711`.

**Step 166's recorded deviations** (the script's emissive override, CMP-1, wave 4c-1; planning step 166 has the evidence, `RECORDED DEVIATIONS` 1-5; `out/parity/program/CMP1/` the reports and `verify1.json`; GAMEPLAY: none; the rows CMP-1 and G1a-11 are closed by it):
- [x] **CMP1-1. Settled history** (deviations 1, 3 and 4). The group's gate figure "chapter 2 player RGB 8-35, arch 5-12 grey" is a summary, not a bound (see CMP-1's note
      below: the gate used the six stills' spread); the sweep's own image of 2-32 cannot show the rule (its player is dead at tick 301 and no arch is in frame: the f200
      capture and the layer's pinned colours are the evidence); the pairs and crops are kept for the named levels only (the overlays, the f200 frames, the montages and the gold
      frames, equal to `after/sweep` byte for byte, were deleted). Nothing is left to do.
- [ ] **CMP1-2. Step 45's 4-22 door reading disagrees with the rule and with the comparison** (deviation 2). Step 45 read 3.3 / 3.9 / 4.6 of 255 over four unnamed blocks
      ("about 0.01 of the texel"); the rule gives, and the comparison's reader box on 4-22 (x 1128-1212, y 120-390) reads, (11.1, 11.2, 14.1) in the original against
      (11.8, 11.8, 14.4) after. Step 45's four blocks are not named, so the difference is not traced; nothing was tuned to either. Next: nothing, unless the blocks are named
      and re-read on the original; no code.
- [ ] **CMP1-3. The flag is tested by presence, the original tests the uint type** (the verification's low 1). `Game::readProperties` #61-66 asks `CheckCustomData` for the
      uint type and never reads the value; the port asks `Meta(key) != nullptr`. Identical on all 128 shipped levels (the census of the original's `.esc` files: 78 flag
      levels, each `<Type>uint</Type><Value>1</Value>`), not in general. Next: test the value's type if a level ever carries the key as another type.
- [ ] **CMP1-4. Two small source leftovers, neither moves a pixel** (the verification's low 2 and the implementer's note 3). The absent `door_emissive`'s 0.0 is a literal in
      `ScriptEmissive` (the listing's start value, #53, cited in `_source`, not a tunable key), and `m_scriptEmissive` is not cleared by `unloadLevel` (the next `buildSprites`
      resets it and the menu reads nothing from it). Next: a `script_emissive` key for the default and a clear in `unloadLevel`, when `game/sim/Lighting.cpp` and the layer
      are next edited for a real change (an edit relinks both executables, and every refused launch notifies the owner).
- [ ] **CMP1-5. Five boxes the step does not bring to the original's, each another cause** (planning step 166's gate 2; the verification's low 4). 2-28's arch (20.3 against 8.3:
      an orange particle cloud over the door at f420, INFERRED from the pair image), 3-25's and 3-26's arches (28.0 against 30.5: the door's red halo, CMP-8, INFERRED from the
      pair images and the reds of the readings, not separately measured), 4-20's player (1.3 against 4.4: the red key's own light, CMP-7) and, inside its stills' range, 2-02's
      player ((46.4, 27.1, 33.0) against (29.6, 24.0, 30.1), luminance error 7.6, the largest player error left: a light on the figure). Next: read them again after CMP-7,
      CMP-8 and the effects' phase (CMP-low a); nothing of CMP-1's rule moves them.
- [ ] **CMP1-6. What the verification did not run** (the verification's `notRun`). The mutation launches (a verifier does not edit sources: it read the implementer's logs,
      `flag` renamed 13 failures, chapter 3's `lit_value` 0 -> 2, `player.value` 1 -> 6, `flag` = `door_emissive` 7) and a rebuild for the zero-warnings gate (a relink under
      Smart App Control: read from `build1.log` and `build2.log`, 0 `warning` lines; the executables `4240c9bd` and `b9430d66` are newer than every compiled source). Next: the
      next build of these sources reads the second; nothing for the first unless the pins are questioned.

**Step 167's recorded deviations** (pictures at one depth in the original's bucket order, CMP-3, wave 4c-1; planning step 167 has the evidence, `RECORDED DEVIATIONS` 1-8; `out/parity/program/CMP3/` the reports and `verify1.json`; GAMEPLAY: none; the row CMP-3 is closed by it):
- [ ] **CMP3-1. The step-118 note's in-bucket rule is not built** (deviation 1; the verification's low 2). The note says `MoveEntity` puts a moved type-0 entity at the FRONT of its
      new bucket, so a bucket's moved pictures would be drawn in reverse file order. None of the nine pairs stands in one bucket; the original draws the same-bucket pairs it
      shows in the FILE's order: 4-16's `block00_ent_2393` and `double_block_plat_ent_2439` (file places 12 and 24, the later over the earlier; 0.81 mean abs from the port's order against 68.9 from the other, on 2,448 px; the
      verification re-measured it, 0.81 on 2,449 px, the still correlating +0.95 with the over-drawn block's texture) and 1-21's `antiportal_822` under `wall04_ent_813`
      (|still - port| 0.86 over 9,223 px, correlation +0.79 / -0.37), plus one consistent pair, 1-21's ring over `single_block_plat_ent_827` (1.60 against 11.2 reversed; another
      load bucket, so not decisive). The chain is open at the engine's walk of `GetEntityArray` (a `boost::unordered_map`: the order across load buckets is the hash's) and at the
      depth normalisation's running range (`ETHScene.cpp:531-532`). Building it would reverse same-bucket pairs on 19 more levels by the static model with nothing to settle
      them (`order_pixels.json`, `v1_a_first` against `v2_a_first`, `sameBucket`); 2-05's overlap is under the touch arrow, 4-26's level is black, 4-19's pair could not be located.
      Next: a still that shows a same-bucket pair reversed reopens it; or dump the engine's walk. Nothing otherwise.
- [ ] **CMP3-2. `line_tolerance_units` 0.001 is the port's own number** (deviation 3; the verification's low 7). The levels put entities exactly on a bucket line (x or y a
      multiple of 128) and the port's float32 bodies read back 127.99999; 0.001 makes a position that close under a line ON it (ten to a hundred times the noise). The commit marks
      it `_guess` in `art.json`'s `_line_tolerance_units` note (the file is `37140679` in the commit, `77728052` in every gate; only that `_` note differs and no launch followed).
      Next: nothing, unless a body is seen drawn in the wrong bucket at a line.
- [ ] **CMP3-3. Two source leftovers, neither moves a pixel on the shipped levels** (deviations 4 and 5). `light_off.ent` (ET_VERTICAL, 4-22 to 4-27) and `black_bar.ent` are dealt as
      type 0 (the port is not told a placement's type; a vertical entity's draw hash adds its y, so it never ties with a type-0 picture, and no pair of a `light_off` picture changes
      order); pictures the layer adds after the level's own (a restocked barrel, the dark dragon's platform, a well's ring, a shock agent's ring, the door's glow) keep the slot they
      are given; `m_rankedSprites` is not cleared by `unloadSprites` (the next build sets it first). Next: tell the reader a placement's type if a vertical entity's pair ever
      matters; clear `m_rankedSprites` when the layer is next edited for a real change (an edit relinks both executables, and every refused launch notifies the owner).
- [ ] **CMP3-4. Causes INFERRED, not measured** (deviation 6; the verification's low 5 and 6). 1-01's slightly worse piece (3.30 -> 3.96 over 157 px), 2-13's and 4-28's flat
      pieces (4-28's is a faint crescent of a wave ring beside the crystal, not the bridge01 / ramp_right pair `explain_changes.md` names), the 11 "dynamic" levels (1-10, 1-17,
      1-22, 1-23, 1-30, 2-18, 2-19, 3-03, 3-07, 3-12, 3-13: camera solves with inlier shares 0.18 to 0.63), 2-30's remaining box error (red-channel; only about 26 % of it in the
      seven blocks over 10 grey levels, so "flame and ring-glow phase" is a reading of the rest), and the nine predicted levels left unchanged (2-26, 2-32 and 3-16 on weak
      solves, 0.14 to 0.21). The verification's estimator reads 2-13 slightly away (29.0 -> 32.1 over 126 px) where the implementer's reads it flat (22.4 -> 23.2): both stand.
      Next: read them again after CMP-9 (the barrels), CMP-4 (the piles) and the effects' phase (CMP-low a).
- [x] **CMP3-5. Settled history** (deviations 2 and 7; the verification's low 1 and 4). The bucket a picture is drawn in is the one the last frame left (G1a-2's lag, built for
      pictures; the original resolves a move after the draw, `ETHScene.cpp:513`); the three Smart App Control refusals (the runner rebuilt for the column-major mutation, launched
      twice in one loop, and the first `MagicPortals.exe`); the literal "only the nine" sweep clause is not met and the 40 are explained, the owner told the count; `test_mp_pictures`
      is not where the order lives. Nothing is left to do.
- [x] **CMP3-6. What the verification did not run** (deviation 8; the verification's `notRun`). The mutation re-runs (`bucket_units` 1e9: 5 failures in `test_mp_sprites` and 36 in
      `test_mp_layer`; `line_tolerance_units` 0: 4 and 5; read from the implementer's logs), **the column-major mutation (never run by anyone; replaced by 2-27, 3-05 and 1-05 on the
      stills)**, a fresh compile for the zero-warnings gate (read from `build6.log`: 0 warning lines, every source older than the executables `ab08b2e8` and `842da3d8`), a full ctest
      loop (the repository's rule) and the reconciliation of its static predictor (34 levels) with the implementer's (47). Next: run the column-major mutation once when the runner
      is next relinked for a real change; nothing for the others.
      *SETTLED 2026-10-12 by wave 4c-2's mutation role (`R4c2/mutations.md`): the column-major `Rank` comparator (`BucketOf` and `Bucket` untouched) is **caught by
      `test_mp_sprites` only**, 3 failures of 2,061 (:4107 "A over C over B", :4112 "(row 1, column 0) is over (row 0, column 4)", the planning step's named pin, and :4162, `Rank`
      ranked twice); **it SURVIVES in `test_mp_layer`** (4,151 checks, its 6 failures are A's and B's): `PicturesAtOneDepthAreDrawnInTheOriginalsBucketOrder`'s nine observed pairs
      cannot tell the axes apart. That gap is row CMP6-9's.*

**Step 168's recorded deviations** (the red key's own light, CMP-7, wave 4c-1; planning step 168 has the evidence, `RECORDED DEVIATIONS` 1-6; `out/parity/program/CMP7/` the reports, `deviations.md` (the owner-delegated disposition after the third verification) and `verify1.json` to `verify3.json`; GAMEPLAY: none; the row CMP-7 is closed by it):
- [ ] **CMP7-1. Gate 2: the raw reading is 1 to 2 percent low in every lit box of 4-20** (deviation 1; the third verification's medium). Required: the group's box R 49.4 (the reader's
      box 57 / 19 / 22; the strip 86.3) to the tolerance of the six stills' spread, which is nil (0 to 0.05 grey). Measured: group box (760,255)-(1040,465) 48.36 / 21.13 / 23.64
      against 49.42 / 21.31 / 24.02; reader's box 55.90 / 18.84 / 21.08 against 56.87 / 19.03 / 21.49; strip 84.50 against 86.32; player box 34.74 against 35.14; disc 34.79
      against 35.43. The part of the same frame the light cannot reach (r >= 180 px from the light's centre) reads 0.979 / 0.992 / 0.982 of the original's, and each lit box divided
      by it reads 0.9995 to 1.004: **the same size, not a shown cause** (the unlit sky's median pixel ratio is 1.000 and the strip's shortfall is spatially structured, rows y300-315
      0.868 rising to 1.006 at y435-450, so the two are not shown to be one gain). Position is exact to 0.7 px (centroids); every whole-pixel shift is worse. The stated bar is
      unreachable by any renderer; waived at the commit as "within 2 percent of the original's added red, equal to the same-frame unlit ratio 0.979" (owner-delegated). Nothing was
      tuned: colour (1, 0, 0), range 48, height 6 and the one tick behind are decoded, and must not be moved to this. Next: nothing for the light; the unlit shortfall is CMP-low (e)
      TONE ("1-1.5 percent darker blue and grey on 4-09 to 4-16", cause unknown), a footage read when a step next touches the sky or the tone.
- [x] **CMP7-2. Settled at the commit: the record said as fact that the shortfall "is the frame's own baseline and not the light's amplitude"** (deviation 2; the third verification's
      lows 2 and 4). The data show only the same size (CMP7-1). Reworded in step 168's intro, gate 2 row, deviation 1 and LEFT FOR LATER (and fix round 2's note), docs only. The
      `hidden` census wording ("only `level_select0-3` and `loading_screen` hold the word", in step 168 and `keys.json` `red_light._source`) is corrected to what the 133 scenes hold:
      none contains the word `hidden`, `hide="0"` stands only in `level_select0-3` and `loading_screen`, no key entity has `hide="1"` (39 keys, 133 scenes, recounted at the commit);
      no key is hidden, as before. `keys.json` is `74099cc1` after the text edit (parses, equal to `0d223d73` apart from `red_light._source`; `test_mp_keys` 182 checks, 0 failures
      on the same runner). Nothing is left to do.
- [x] **CMP7-3. Gate 3: the two rules of the code were never mutation-tested** (deviation 3; the verification's low 3). The one tick behind and the no-particle-share rules fail by
      construction (297 of 300 and 24 of 300 moving ticks; 300 colour ticks), as the verifier read them; `keys.json`'s four values were mutated (`colour_name` 2 and 2 failures in
      `test_mp_keys` and `test_mp_layer`, `height_units` 1 and 2, `range_units` 1 and 2, `colour` 1 and 4). A code mutation relinks the runner under Smart App Control and a refused
      launch notifies the owner. Next: run the two mutations once when a relink is allowed anyway (the next step that must relink the runner).
      *SETTLED 2026-10-12 by wave 4c-2's mutation role (`R4c2/mutations.md`, no commit; one mutated runner `81127657`, 0 Smart App Control refusals): **both caught by
      `test_mp_layer`, each by a pin of its own**. A (the key's light at this tick's picture instead of one tick behind): `test_mp_layer.cpp:21728` `behind > 0`, "level18c: 0
      of 300 ticks a tick behind its key" and the same for level19c, and `wrong == 0` (:21726) on the position half. B (the key's light given its emitter's particle share):
      :21687, the load-time colour (0, 0, 0) where (1, 0, 0) x the level's intensity is required, on both levels, and `wrong == 0` on the colour half (level19c's first wrong
      tick has the position equal, so that one is B alone). The "297 and 24 of 300" are the step's record's, not re-measured (the committed runner was not launched).*
- [ ] **CMP7-4. 4-19's light is shown by no frame** (deviation 4; the verification's low 5). The f420 frame is byte-identical by geometry (the breakable wall, the one lit
      neighbour within 48 units, is off the frame); no original frame near the wall exists; `test_mp_layer` pins 4-19's position, height 4, range and colour on 300 ticks; at f640 on
      its route the carried key lights the booster red, like footage frame #371. One capture with `--hold right` did not move the player in 260 frames (cause not looked into).
      Next: take a frame with the wall in view when the level can be played to there on a route, if the owner wants the second level shown.
- [ ] **CMP7-5. The key's fly-in is not built** (deviation 5; `keys.json` `_not_built`, `_deviation`). A key that found its keyhole flies in and is deleted at the fly-in's last
      frame (#646), taking its light; the port holds the key's picture where it opened its keyhole until that keyhole is gone, so a spent key's light lasts as long as that picture.
      Next: the light ends at frame #646 when the fly-in's own step builds it.
- [x] **CMP7-6. Gate 6 (zero warnings) rests on scratch compiles and the build logs** (deviation 6). The implementer's four build logs hold 0 warning lines; the verifier recompiled
      the four changed units (`Keys.cpp`, `MagicPortalsLayer.cpp`, `test_mp_keys.cpp`, `test_mp_layer.cpp`) with the build's own /W4 lines (0 warnings) and did not re-run
      `port_build.bat` (it could relink and notify the owner; `ninja -n` showed a CMake re-check step, cause not investigated). Every source is older than the executables
      `08469442` and `23a7d823`. Next: the wave's verify role runs `port_build.bat` once.
      *Ticked by wave 4c-1's close on other evidence, not on the build the row asked for: each of the wave's three verifiers recompiled translation units with the build's own
      /W4 command lines (no link, outputs in a scratch folder), every unit rc 0 with no warning line: verify1 the 55 units whose dependencies include a changed file (9 in
      `R4c1/verify1/compile_scratch.log` and 46 in `compile_scratch2.log`), verify2 all 136 port units (`verify2/compile_scratch.log`), verify3 the same 55
      (`verify3/compile_scratch.log`); and the steps' own build logs hold 0 warning lines (CMP1 `build1.log`, `build2.log`; CMP3 `build6.log`, 53 objects; CMP7 `build0.log`,
      `build0b.log`, `build1.log`, `build2.log`; CMP9 `build1.log` to `build3.log`; CMP10 `build1.log`, `build2.log`, `build_final.log`, `build_final2.log`; the logs show the
      build's ninja steps and do not say whether `port_build.bat` or the MSVC environment alone ran them). `port_build.bat` itself was NOT run by any verify or close role: with
      every object current by ninja's dependency records it re-runs CMake's glob check and compiles nothing, so it would show nothing about warnings, and a relink can notify
      the owner.*

**Step 169's recorded deviations** (the script's emissive blinks, CMP-9, wave 4c-1; planning step 169 has the evidence, `RECORDED DEVIATIONS` 1-7; `out/parity/program/CMP9/` the reports, `deviations.md` (the delegated disposition after the third verification; its own numbering differs from the record's, which these rows follow) and `verify1.json` to `verify3.json`; GAMEPLAY: none; the row CMP-9 is closed by it):
- [ ] **CMP9-1. Gate 1: the f420 sweep changed 40 levels, 10 of them outside the group's 32, and 2 of the 32 did not change** (deviation 1; the first, second and third
      verifications' medium). Required (as first written; corrected to 42 levels by wave 4c-1's fix1, below): only the group's 32 levels differ from `CMP7/after/sweep`, the rest byte-identical, each change explained. Measured: 128 launches, rc 0,
      validation silent; 88 byte-identical, 40 changed = 30 of the 32 and 10 outside them: 2-10, 2-11, 2-20, 2-26, 2-27, 2-30, 2-31, 3-23, 4-12, 4-28, exactly the fire-agent
      levels outside the 32 (22 agents on 20 levels, 10 inside the 32 and 10 outside, from the scenes), 445 to 5,125 changed pixels, at most 25 grey levels (2-31's agents are
      off the frame and only their range-256 light reaches it). The pulse stands at 0.9965 of the size at f420, and the whole frame's mean absolute difference to h6.3 moves by
      0.006 or less on each of the 10. 2-32 and 3-15 are byte-identical: their one teleportable crate is off the f420 frame (2-32's at x 1312 of a scrolling level; 3-15's at x
      496, 770 px right of the frame's edge). The spec contradicts itself (its BUILD line and CMP-9's own text, "also decoded, not built: the fire agent's bounce", ask for the
      pulse; its sweep gate names only the 32). **Scope 42 levels ACCEPTED at the commit (a delegated disposition under the standing order of 2026-10-10: no owner message) and the gate's text CORRECTED to the 42 by wave 4c-1's fix1, after its verifier failed the first text on 4-12, the one fire-agent level that no CMP group of the wave names (the other nine are inside CMP-1's 78; planning step 169's fix paragraph has the recomputation, `out/parity/program/R4c1/fix1/changed_vs_named.txt`)**, because the ruling is the original and the pulse is what the
      original does; the gate is not recorded as passed. Revert if the owner vetoes it: the pulse line in `syncSprites`, the owner-scale pass in `syncLights`,
      `fire_agent.pulse` (`art.json`, `Art.cpp`, `Art.hpp`) and its pins, then a re-sweep (only the 30 drawn levels then move). Next: 2-32 and 3-15 each get a capture at a tick
      with the crate on screen, in the wave 4c-2 re-fly or the final comparison; the pulse was swept at f420 only, not at its ends (0.98, 1.02). The scope of 42 is the ruling R4c1-1 (accepted after the wave's close: its gate 4 failed on 4-12 as worded).
- [ ] **CMP9-2. Gate 2: 16 of the 93 stills lie outside the port's f420-f504 range after alignment (20 before it)** (deviation 2; the verifications' lows). Measured: 77 of 93
      stills over the 22 boxes inside the range after the camera offsets of four stills are aligned (73 before), 66 of 69 in the 17 boxes whose content is the blinking thing
      alone. Outside, with the cause looked at: 2-25's h0.3 (the "Part 2" title over the crate), 2-08's pyramid (burning in the later stills), 2-05's small crates (a pile
      elsewhere), 2-24's 700 x 300 px box, 3-19's h0.3 (its static sky 20 percent brighter in that still), 2-29's h6.3 (blue above red: another object). The stills never sample
      the port's troughs on 4-32 (43.3 to 67.2 against 15.7 to 65.2), 4-25 (the aligned floor 9.7 against 2.4) and 2-21 (20.7 to 22.9 against 10.9 to 22.4); footage F16's
      amplitude on 4-32 supports the port's. 3-14's h0.3 and 4-25's h0.3 and t2.0 were camera offsets (closed). Cause: confounded boxes and the stills' phase sampling (wall
      seconds at 15 to 29 fps, the HUD 0.5 to 0.9 s ahead of the world), not a wrong rule. Next: trace the ambient and the lights on 4-25, 4-32 and 2-21, or capture a clip of
      them, in the wave 4c-2 re-fly or the final comparison.
- [ ] **CMP9-3. The flagged wall's period and the static entities' phase are decode only** (deviation 3; the verifications' low). No footage clip holds a wall with `blink` (8
      levels), so its 300 ms period on the doubled clock is the decode's, not measured (the barrel's 0.605 s and the crate's 1.210 s are measured, against the decode's 0.600
      and 1.200); the fire agent's pulse (+-2 percent of a 32-unit picture) is below pixel resolution. The 33 breakable walls and the fire agents are static, so the original
      starts their clock when their bucket is first drawn (G3-2) and the port at level tick 1: the ends and the period are the original's, the phase is not known to match; a
      restocked barrel is drawn one frame early. Next: a clip of 1-09 or 2-14 (the same clip as CMP9-6), then G3-2.
- [x] **CMP9-4. Settled before the commit: the restocked barrel's blink had no test and the code rules were never mutated** (deviation 4; the first verification's medium).
      Fixed (fix1): `ARestockedBarrelBlinksFromItsOwnBirth` drives 4-32 to the dragon's next shot (12 checks, the barrel drawn from its birth tick), and the four code rules
      (the wall colour's one add behind, `For`'s teleportable branch, the light's and halo's owner scale, the restock's born clock) were mutated in one relinked runner, each
      failing the checks that name it. The restock's floor assignment is an equivalent mutant on the shipped data (the placed barrel it is copied from already carries the
      floor); the test pins the value. Nothing is left to do.
- [ ] **CMP9-5. The `blink`-flag crate branch is not built: 4-19's two flagged crates** (deviation 5). `crateCallback` #95-121, `blinkEmissive(1 - GetAmbientLight(), V3_ZERO,
      200)` for a teleportable crate that carries the uint `blink`: `crate_2494` and `crate_2495` are the only two of the 128 levels; building it moves 4-19, which is neither
      in CMP-9's group nor in the step's build list (4-19 is byte-identical to before). `art.json` `non_teleportable._not_built` says so. Next: build it with the step that
      takes 4-19.
- [ ] **CMP9-6. The whole frame's MAD against h6.3 is slightly worse on three changed levels; the chapter-1 walls read dimmer than the stills** (deviation 6; the
      delegated disposition, `CMP9/deviations.md`). h6.3: 1-22 5.205 -> 5.260, 2-19 2.576 -> 2.651, 1-13 4.597 -> 4.607; six more (2-11, 2-26, 2-29, 2-31, 3-23, 4-10)
      move by 0.006 or less. Summed over ALL the kept stills the frame MAD is flat or better on all three (1-22 55.26 -> 55.16, six stills; 1-13 49.46 -> 49.30, six; 2-19 11.97
      -> 11.95, three) and the MAD over the changed pixels is better (100.0 -> 98.4, 117.8 -> 110.9, 27.5 -> 27.1), so the h6.3 reading is one phase; 2-19's port value now sits
      inside the stills' spread. **Open:** on the chapter-1 walls (1-22, 1-13) the port's f420 value (0.85 to 0.875 of the old peak) is dimmer than all six stills on the
      changed pixels (port minus still -3.5 to -12.1 on 1-22), where before it was brighter than four. The stills read 0.72 to 1.01 of the peak (mean about 0.91) against the
      decode's 0.64 to 1.0 cycle, sampled at multiples of 0.1 s on a 0.3 s period, which can explain it; a shallower original blink (the colour and emissive floors not both
      reaching 0.8 in the picture) cannot be ruled out without a clip. Cause: probably the phase, not traced to a depth difference. Next: the clip of CMP9-3, then the wall's
      depth and phase measured; the wave 4c-2 re-measure reads them as ranges.
- [x] **CMP9-7. Housekeeping: dead default initialisers in `Art.hpp`, and `MagicPortals.exe` older than the libraries** (deviation 7; the third verification's lows). (a)
      `BarrelBomb::floorPresent{1.0}` and `ceiling{0.2}` and `BreakableWall::callsPerFrame = 1` repeat `art.json`'s values and never take effect (`LoadRules` refuses the file
      without each key); code, and `Art.hpp` is included by ten files, so the edit relinks both executables: not done at the commit. Next: value-initialise them to 0 the next
      time `Art.hpp` is touched. (b) `build/game/MagicPortals.exe` `90a48b79` (linked 02:57) is older than the libraries rebuilt for `test_mp` after the first fix (04:35); the
      sources are byte-identical (md5s equal `fix2/md5_end.txt`) and the sweep was taken with this executable, so nothing was rebuilt at the commit. Next: the wave's verify
      role runs `port_build.bat` once (`ninja -n` shows a CMake re-check step, so it may relink and a refused launch notifies the owner), launches the new exe once alone, and
      re-checks three levels against `CMP9/after/sweep`.
      *Settled by step 170 (CMP10, wave 4c-1), 2026-10-12: (a) the three dead default initialisers (`BarrelBomb::floorPresent`, `ceiling`, `BreakableWall::callsPerFrame`)
      are value-initialised to 0 in `Art.hpp` (planning step 170, THE CHANGE; nothing reads them: the full run and the sweep equal the before); (b) the third verifier of
      step 170 relinked `MagicPortals.exe` once (`29166f75`), launched it alone with no refusal, and its 128 launches equal `CMP10/after/sweep` byte for byte (the other 119
      levels equal `CMP9/after/sweep`; CMP10-7).*

**Step 170's recorded deviations** (the help block's alpha and bounce, CMP-10, wave 4c-1; planning step 170 has the evidence, `RECORDED DEVIATIONS` 1-7;
`out/parity/program/CMP10/` the reports, `deviations.md` (the owner-delegated disposition after the third verification; its numbering is the record's) and `verify1.json` to
`verify3.json`; GAMEPLAY: none; the row CMP-10 is closed by it):
- [ ] **CMP10-1. Gate 2: the face reads 1.1 to 3.0 grey of 255 under the original's on all eight levels in frame; one calibration experiment, not a fix** (deviation 1; the
      third verification's low). Required: the face 171-173 on the readers' statistic (1-10, 1-11, 1-13, 1-25) to the group's tolerance or the spread of the six stills.
      Measured (`CMP10/verify3/gate2.md`: one finder on the stills and the port, gold seed): the port minus the same-wall stills' mean 1-02 -1.1, 1-10 -1.8, 1-11 -1.4, 1-13
      -2.2, 1-25 -1.4, 1-19 -1.6, 2-03 -2.7, 2-06 -3.0 grey, 0.5 to 1.2 percent, against the stills' own spread of 0.5 to 0.8; the readers' statistic moves about 1 grey
      with the threshold and the phase (the port 173.6-175.2 / 169.1-170.2 / 170.3-171.5 / 171.7-173.2). The widths are met (the port 71-77 px over a period, the stills
      71-75). Cause: not the port: on 1-10, 1-11 and 1-13 the port draws the PNG at the decoded 0.76 (fits 0.7587 to 0.7593) and the original reads 0.7661 to 0.7670 on 16
      fits, a source weight 0.9 to 1.1 percent heavier; the mechanism is unknown (alpha and face tone are not separable) and 1-25, 2-06, 1-02 and 1-19 are not fitted. A
      lead, not established: the HUD's pause button reads +2.8 / +1.5 / +2.9 grey (R / G / B, the median of 31 readings) above the port's, so the excess may be general to
      the original's alpha-blended pictures. **Next, an experiment:** run `fix2/fit2.py` on a second alpha-blended picture with a decoded alpha (a fading handle, the hint,
      the HUD button) and calibrate only if it says the excess is general (a blend calibration, a row of its own); do not retune the block alone (an alpha of about 0.767 or
      a face x 1.01 would contradict the decoded 0.76 and could mask a general effect). The lighting rows may take the 1-19 and 2-03 wall (+2.7 to +4.0 grey, 42 to 80 px
      from the block).
- [x] **CMP10-2. Settled at the commit: 1-25's "150" is not reproducible** (deviation 2). The readers' patch coordinates were never recorded (`chunk_4.json` holds only the
      box) and no 5 x 5 patch reproduces their 150 / 147 / 152; the port reads 148 / 146 / 149 on patches chosen by the original's value, which is not an independent
      reproduction. Dropped from CMP-10's text at the commit; nothing left.
- [x] **CMP10-3. Settled at the commit: the group's and the spec's order of the two scales is the listing's push order** (deviation 3). The call is `bounce(thisEntity,
      1.13, 1.05, 250)` (a call pushes its last argument first; the fire agent's #282-300 and bounce's `interpolate(scaleA, scaleB, ...)` agree): 1.13 at 0 ms, 1.05 at 250,
      period 500. The range, 71.4 to 76.8 px, is the same either way; only the phase, which no still fixes, would differ. Built as decoded; CMP-10's wording of the call and
      of the triangle corrected at the commit; nothing left.
- [ ] **CMP10-4. The block's phase against the original's, and what a paused time does to it** (deviation 4). The stills are timed by wall seconds and read as a range, so
      the phase is not measured. `help_block.ent` is static, so the original runs the callback only while its bucket is drawn (G3-2) and the port from tick 1 on the level's
      age; 1-02 and 1-03 hold the block at 1.13 and alpha 0.76 under their level-start popup (the age stands at 0 under it; what f420 shows on the owner's seed). Whether
      the original's paused game time stops the callback's `blinkElapsedTime` the same way is the pause row's. Next: a clip of 1-02 or 1-10 from the load frame pins the
      phase and the popup's hold; the bucket clock (G3-2), with the fire agent's (CMP9-3), the antiportal's and the walls' rows.
- [ ] **CMP10-5. Four blocks are on no frame the comparison holds: 1-32, 3-09, 4-01 and 4-02** (deviation 5). Unchanged at f420 because the block is off the frame (with
      `--place` each shows the translucent block); the 13-level layer pin (`EveryHelpBlockOfTheThirteenLevelsIsDrawnByItsCallback`) is what reaches them; the original's
      stills never show 4-01's block either. 1-03's block is a 25 px sliver at the frame's edge, its face and width not measured. Next: nothing owned; the wave 4c-2 re-fly
      or the final comparison may capture them with the block on screen.
- [x] **CMP10-6. Settled at the commit: the planning record's gate 2 row quoted the readers' statistic to a precision it lacks** (deviation 6; the third verification's
      low). The single values 172.8 / 169.3 / 169.8 / 172.0 and "1-10 and 1-25 inside 171-173, 1-11 and 1-13 1.2 to 1.7 under" now read as the series ranges 173.6-175.2 /
      169.1-170.2 / 170.3-171.5 / 171.7-173.2, good to about 1 grey; no conclusion changes. Nothing left.
- [x] **CMP10-7. Settled by the third verifier: step 169's deviation 7b, the executable older than the libraries** (deviation 7). `MagicPortals.exe` `29166f75`, relinked
      once and launched alone with no refusal; its 128 launches equal `CMP10/after/sweep` byte for byte. Part (a) of the same row is closed by this step's `Art.hpp` (see
      CMP9-7). Nothing left.

**Step 171's recorded deviations** (the two spawn markers the script never runs, CMP-6, wave 4c-2; planning step 171 has the evidence, `RECORDED DEVIATIONS` 1-9;
`out/parity/program/CMP6/` the reports, `impl.md`, `census.md` and `verify1.json`; GAMEPLAY: YES, 3-09 and 4-29; the row CMP-6 is closed by it):
- [x] **CMP6-1. Settled at the commit: the row "3-09 minion_killed" was fed by the knight that is gone** (deviation 1; the first full run's failure). The guard died on the
      killer floor on tick 58 of every run, which `EventsALevelMakesOnItsOwnShakeTheView` waited for; the row now carries a knight 20 px over the floor, as a portal would,
      and the callback's own numbers (the 20, the one-tick order of the shake) still pass. The original's 3-09 never raises that event in play (the verifier's low): the row
      proves the callback's numbers, not a death the level produces. Nothing left.
- [x] **CMP6-2. Settled at the commit: the high-knight pin moved from 3-09 to 3-03 and its tolerance was corrected** (deviation 2). 3-09's 157.9 px knight was the pin's
      only high marker; the next is 3-03's `minion_spawn_913` (29.9 px over its floor). The old pin read the step's travel from the begin's velocity in the original's units
      where the port's body falls SimRatio times that (5.0 px a tick at the matched 60 Hz); it now reads the body's own two transforms and passes for a step of at least 4.4
      px. Nothing left.
- [ ] **CMP6-3. 4-30's id 76 ("unnoticed 1") asks for exactly 1 knight alive and unseen, and both of its markers are `minion_spawn` with a callback** (deviation 3; the
      verifier's low; GAMEPLAY: yes if real). Level29c: ids 1766 at (360, 208) and 1902 at (112, 208). Ten of the eleven achievement counts that name `minion.ent` equal the
      port's knights; this is the one difference, pinned by level in `test_mp_minions`. Candidates, unproven: a knight the original loses on its own (as 3-09's guard did in
      the port), a bucket-gated spawn (a static marker's callback runs only in a visible bucket, G3-2), or an achievement the designers never made winnable. The level is
      dark and no footage exists. Next: a clip of 4-30 from the load frame or the bucket trace (G3-2); the owner's call if neither decides it.
- [ ] **CMP6-4. A minimised capture window stalled the sweeps** (deviation 4; both the implementer's and the verifier's sweeps; the verifier's low). Something on the
      desktop minimised `MagicPortals.exe`'s window on a launch (7 minutes lost in each sweep; cause unknown, not either role's doing as far as the logs show: no
      portal_launch, tap or Escape input in the verifier's 132 launches' logs); each role restored its own window only, by `ShowWindow(SW_SHOWNOACTIVATE)` from a hidden watchdog.
      The runs are fixed-step: the two sweeps' 128 PNGs are byte-identical. Next: as R4c1-3 (d): allow 40 minutes for a 128-level sweep, keep a restore loop for the sweep's
      own window, or sweep with the laptop awake; the later steps' sweeps will meet it.
- [ ] **CMP6-5. 3-09's 80 px at f420 are attributed to the shared effects random stream, not proven** (deviation 5; the verifier's low). The two clusters (68 px at
      (870-879, 86-94), the red eye of the knight on the block; 12 px at (1063-1118, 446-521), the red flame; at most 16 grey levels) were confirmed, and the baseline log
      carries the guard's death, its 19.69 px shake and the skull and blood particles that the after log lacks; no frame with a differing stream was taken. Next: nothing
      owned; a frame with the old stream (the guard's draws replayed) would prove it.
- [x] **CMP6-6. The DEVLOG entry is the wave's close role's** (deviation 6; written by wave 4c-2's close, 2026-10-15: DEVLOG Session 31). The backlog row CMP-6 is ticked by this commit; `test_mp_minions`' new test names 4-30 and the
      two silent levels by file, as the suite's other census does. Next: the close role's DEVLOG entry.
- [x] **CMP6-7. Disclosed: the launches beyond "once each"** (deviation 7; the verifier's low). `test_mp_minions` 7, `test_mp_layer` 4, `test_mp_routes` 3, the no-argument
      run 2: the fresh-runner launch, a first full run that found deviations 1 and 2, the relinked runner's confirmation and the gate 4 mutations; zero Smart App Control
      refusals. Nothing to fix; later steps keep one link and one launch per runner.
- [ ] **CMP6-8. The gate 4 mutation has no second witness** (deviation 8; the verifier's low). The verifier may not edit a tracked data file in place and read the four
      mutation logs (12 + 1, 9, 6 failures) instead. Next, if the wave's close wants one: put `minion_spawn_` and `minion_spawn6` back into `enemy_spawn.names`, run
      `test_mp.exe test_mp_minions` once (expected: 12 failures), restore the file (md5 `5bc2e4f1`). *Wave 4c-2's close did not take it (it launched nothing: a tracked-file
      edit and a runner launch are for a role that needs them); the verifier of the route phase read the four logs again and did not run it either.*
- [ ] **CMP6-9. `test_mp_layer`'s order test does not tell row-major from column-major** (deviation 9; a low carried from wave 4c-2's mutation role, not this step's). Next:
      a pin that does, when the layer's order is next touched. *PROVEN by the mutation role (`R4c2/mutations.md`): the column-major comparator fails `test_mp_sprites` (3) and
      no `test_mp_layer` check; `test_mp_layer.cpp:10617`'s nine `observed` pairs each differ on one axis, or on both in the same direction. The three pairs that DO discriminate
      (each from the stills, planning step 167 gate 3; node names and positions read from `out/levels`; every node at depth 0, none carries `z_index` or `eth_z`), to add to
      `observed` with their levels to the level loop: `level26a` (2-27) `platform_no_emissive_ent_962` (78, 160: row 1, column 0) over `crate_no_emissive_1000` (172, 115: row 0,
      column 1); `level4b` (3-05) `single_block_plat_low_emissive_ent_1903` (322, 144: row 1, column 2) over `wall04_low_emissive_ent_1899` (384, 48: row 0, column 3); `level4`
      (1-05) `platform_ent_658` (64, 142: row 1, column 0) over `double_block_plat_ent_702` (176, 64: row 0, column 1). Row-major puts the first of each over the second; column-major
      the reverse. A test-only edit.*

**Step 172's recorded deviations** (a node's own `scale` datum scales its picture, its body's box and its mass, CMP-16, wave 4c-2; planning step 172 has the evidence,
`RECORDED DEVIATIONS` 1-6; `out/parity/program/CMP16/` the reports, `impl.md`, `census.md` and `verify1.json`; GAMEPLAY: YES, 2-01's crate; the row CMP-16 is closed by
it):
- [x] **CMP16-1. Settled at the commit: gates 2 and 3 were NOT RUN by the implementer, and the verifier took them** (deviation 1). Smart App Control refused the three
      links of `MagicPortals.exe` on their first launches (16:02-16:03); the verifier launched the same `7dd3a7de` at 16:29, no refusal, no relink, and measured the sweep
      (1 of 128 changed, 2-01) and the crate (135 x 128 against 135 x 130). The implementer's "135 x 131 COMPUTED" was wrong by 3 rows (it used the spec's 175, which
      reads 6 rows into the platform). Planning step 172 now carries the measured rows. Nothing left.
- [ ] **CMP16-2. The gate 4 mutations were one build and could not tell the box from the mass** (deviation 2; the verifier's low). Five removals were applied together (to
      limit Smart App Control exposure) and the box, circle and hull unscaled with the geometry-derived mass fell together (17 + 2 checks); only the picture (4),
      `ShapeBoundsPx` (7), the refusal (2 + 2) and the material's stated-area override (1) were each shown to fail alone. The verifier did not re-run them (each costs a
      `test_mp.exe` relink) and read the implementer's three logs, which match. Next, if the wave's close wants the split: a box-only and a mass-only build (two links),
      `test_mp.exe test_mp_timebase` once each.
- [x] **CMP16-3. Disclosed: 5 Smart App Control refusals, and the process note** (deviation 3). `test_mp.exe` 44fcd318 and the mutated 29c679de (each relinked once),
      `MagicPortals.exe` bbaa1bd1, cee70780 and 7dd3a7de (three links, each refused on its first launch). `7dd3a7de` then ran unchanged at 16:29, so the refusal was
      transient: launch a refused exe once more before relinking (a relink is a new file, checked again, and every refusal notifies the owner). Nothing to fix.
- [x] **CMP16-4. Settled: `ReadCustomScale` refuses a datum that is not a number above 0** (deviation 4), where `scaleEntity` would pass a 0 or a negative number to
      `Scale()`. No node carries one (the census), the refusal names the datum, and `test_mp_levels` and `test_mp_timebase` pin it. Nothing left.
- [ ] **CMP16-5. 2-01's crate is 2 px lower in height than the original's, which is not the scale** (deviation 5; the verifier's low; GAMEPLAY: none). 128 px tall against
      the original's 130 at f420, width exact (135 = 135). The same 2-row drop is measured on 3-13's UNSCALED crate (top and bottom edges both 2 rows lower in the port,
      left and right equal), so it is a pre-existing resting offset of the engine's contact, not this step's. Hypothesis, unproven: Box2D's polygon skin
      (`b2Settings.h:76`, `b2_polygonRadius` = 2 x `b2_linearSlop` = 0.01 m, a rest separation of about 0.015 m = 0.75 unit = 2.1 px) which the engine's resting contact
      does not reproduce. Next: measure a crate at rest on a flat floor in the port against the original's (a footage frame or the library stills), and record an engine
      gap with the number if it is the contact (an engine step, not this one).
- [ ] **CMP16-6. `ShapeBoundsPx` returns the scaled box, which the original's script-side box is not** (deviation 6; the verifier's low; GAMEPLAY: none today).
      `GetCollisionBox` (`ETHEntity.cpp:314-321`) and `getAbsoluteCollisionBox` (listing bytes 6337..6722: it multiplies by `getScale()`, 1 in the port, and by the angle)
      are not scaled by the entity's own scale; only `CreateBody`'s box is. The port's consumers (lava, breakables, lifts, killer floors, minions, the dark dragon's
      platform, restock barrels, the drawn box) read no node that carries the datum except 2-01's crate, which only the drawn box reads, so nothing a script would read
      differs. The function's comment named a burning crate's flames, a consumer that does not exist (`Fire.cpp` reads lava only); the commit rewords it, text only, and
      the binaries were not relinked for it. Next: nothing owned; a node with the datum and a script-side box consumer would have to read the unscaled box.
- [x] **CMP16-7. 2-01's and 4-29's routes fail on this build** (the route text of planning step 172; the verifier's low). 2-01: the lighter crate is pushed at 1.0-1.8 px
      a tick of 2.4, the player steps off the top floor on tick ~175 (route: ~308) and is burned by the fireball spat on tick 283. 4-29: step 171's, unchanged (the tap on
      tick 525 is off the view). 126 of 128 routes win on exactly the census's ticks. Next: the route phase (planning step 176) re-flies both; 2-01's header text is
      stale.
      *CLOSED by step 176 (`45f6c05`), 2026-10-15: 2-01 (307) and 4-29 (1,023) are re-flown and win; the stale header text is rewritten in the new route.*

**Step 173's recorded deviations** (a bounce slab is placed, not moved with a velocity, CMP-15, wave 4c-2; planning step 173 has the evidence, `RECORDED DEVIATIONS` 1-6;
`out/parity/program/CMP15/` the reports, `impl.md`, `census.md`, `deviations.md`, `fix1.md`, `fix2.md` and `verify3.json`; GAMEPLAY: YES, 4-02's astronaut, and it moves 4-07's
route; the row CMP-15 is closed by it, committed with these deviations by the owner's delegation):
- [ ] **CMP15-1. Gate 3, the eye within +-1 px of 380 at f75, f150, f300, f420 and f450, is met from f150 and NOT at f75-f135** (deviation 1; the verifier's medium; the gate's text
      is not rewritten). 383.10 at f75 (+3.1 px; 383.18 on the verifier's reader) is the same before and after the fix, and f90-f135 (383.07, 382.38, 381.94, 381.12) sit 0.5-1.1 px
      FARTHER from 380 than before (the old drift carried him up toward 380 early); f150 380.75, f300 380.81, f420 380.42, f450 380.85, and all 21 frames f150-f450 read
      380.31-381.08 (two of them 0.02-0.08 px over the band's edge: reader noise on a pulsing glow). The drift the row opened for (3.2 px/s, 16 px at f420) IS cured. Cause: the astronaut's spawn rest is about one stride (1 px = 2.8 screen px) lower than the
      original's stills read until the slab's first rise shoves him 0.85 px near tick 79. No original frame exists at world tick 75 (the stills date themselves at f127-f208 on the
      slab's own top-y clock), and the decode (`level1c.esc`: `main_char` (138, 152), slab (140, 202), collision 64 x 64; the slab's top below his feet to call 78) puts the original's
      astronaut at his start on tick 75 too (an INFERENCE, labelled: about 382.8 on screen). Next: G3-5 (the bob's phase and the spawn offset) and CMP-low (a); nothing tuned.
- [x] **CMP15-2. Gate 6's route clause: three routes fail on this build, and the wave enters the route phase with three** (deviation 2; the verifier's medium). 125 of 128 win on
      exactly their census tick, 0 on another tick, 0 missing. **4-07** (`level6c`, census 446) is not won by 470, by design: its route leaned on the rider the slab no longer
      carries (the player and the crate rest on the top of the slab at (128, 208) from tick 44; 0.4-0.9 px less carry, the tick-313 shot flies 2.1 px lower and grazes the crate
      on tick 344, the door shot's recoil leaves him at (-10, -11) u/s, the tap of tick 443 misses him). **2-01** (`level0a`, census 444: died on tick 283) and **4-29** (`level28c`,
      census 1023: the tap on tick 525 is off the view) are inherited, not this step's: the same signatures in step 172's own run on `9d643c9` (`CMP16/runs/all1.log`), no
      bounce node in either level, f420 frames byte-identical to the before-sweep (CMP16-7). `test_mp_routes` 2,321 checks, 6 failures. Next: the route phase (planning step 176)
      re-flies 4-07, 2-01 and 4-29; read 4-07's exact vy only if a tick-sensitive tap fails again.
      *CLOSED by step 176 (`45f6c05`), 2026-10-15: 4-07 (446, the census tick), 2-01 (307) and 4-29 (1,023) are re-flown and win; the step's own gate text stays "NOT MET
      literally" as it was at its commit.*
- [ ] **CMP15-3. The gate 4 mutation of the kinematic rule was run by nobody** (deviation 3; the verifier's low). The placement rule's removal IS the red run of the new pins on the
      committed `Bounce.cpp` (runner `e830640d`: 9 failures of 203, the velocity pin on the three levels, the rider's travel 7.501 px against 2.25, vy -1.1959..-1.2002 at the
      five samples; green 203 / 0), not a separate removal build. "The body stays kinematic" (`kBouncer` out of `Roles::Moves`) has only the older `EverySlabIsKinematic` pin
      (and `checked > 0`). Next: a throwaway build with it removed, restored before committing, when a step next touches the bounce slab (it costs a runner link).
- [x] **CMP15-4. Settled at the commit: the executables were relinked for a comment-only change** (deviation 4; the verifier's low). `MagicPortals.exe` `877b51e6` and `test_mp.exe`
      `7b6a91f7`, on which every gate was measured, predate the last comment of `Bounce.cpp` and the note of `bounce.json` (compiled alone, 0 warnings). The commit's build
      relinked both (0 warning lines; `79814fd0` and `80560e19`), the runner ran once alone (`test_mp_bounce` 203 / 0) and 4-02's f420 on the relinked exe is `daf646c9`, the
      sweep's. 0 Smart App Control refusals. Nothing left.
- [ ] **CMP15-5. The same velocity-writing call remains for moving platforms' later calls, lifts and lift doors, and their riders were not measured** (deviation 5). They still
      call `Mover::MoveKinematic`. Next: G3-4's throwaway build (`PlaceKinematic` for them), read each rider against the original, and record an engine gap with the number if
      the push-out does not hold it.
- [x] **CMP15-6. Disclosed: no engine gap** (deviation 6). The engine's push-out holds 4-02's rider (0.850 px once, vy 0.0000 on every sample); 4-07's rider drift of 1.2 u/s is the
      recoil of the tick-183 shot (the fit: 0.000 u/s over ticks 156-181, -1.202 u/s from 186, predicted -1.182), not a push-out residue; the trace prints v as integers, so it is
      a fit, not an exact vy. Nothing to fix.

**Step 174's recorded deviations** (a hinged body stands where its joint holds it, CMP-2, wave 4c-2; planning step 174 has the evidence, `RECORDED DEVIATIONS` 1-7;
`out/parity/program/CMP2/` the reports, `impl.md`, `census.md`, `route_census.md` and `verify1.json`; GAMEPLAY: YES, ten levels' hinged bodies, and it moves the routes
of 2-31, 3-19 and 3-20; the row CMP-2 is closed by it):
- [x] **CMP2-1. Settled by the verifier: the sweep was taken on an executable the implementer relinked afterwards** (deviation 1). The sweep is on `MagicPortals.exe`
      `3c818a2d`; the implementer relinked it (`3ea23c59`, then `83d5c069`) with no source change (`Hinge.cpp` `274a41ea`), and 15 launches were byte-identical with the
      sweep. The verifier re-took the whole sweep on `83d5c069`: 128 of 128 PNGs equal. The commit relinked nothing (both exes are newer than every source; its edits are
      record text). Nothing left.
- [ ] **CMP2-2. No port-side mutation of this step was independently re-run, and one was never run** (deviation 2; the verifier's low). The first two (the body not placed,
      the name check removed; B's attach point not turned by B's angle) were built by the role that was interrupted and the implementer's three (A's attach point not
      subtracted; `connectedAnchor` 0; attach B's numbers optional: 16 failures of 102) by the resumed one; the verifier read the logs and re-ran none (each needs a
      relinked runner, a Smart App Control exposure). Never run: the removal of "a missing `joint_other` is refused" alone (the next line dereferences it). Next: a role
      that relinks the runner anyway runs that one.
- [ ] **CMP2-3. `Hinged::placedPx` and `heldPx` have no reader outside `Hinge.cpp`** (deviation 3; the verifier's low; GAMEPLAY: none). The tests do their own arithmetic;
      removing them rebuilds every translation unit that includes `Hinge.hpp`. Next: remove them with the next change that touches `Hinge.hpp`.
- [x] **CMP2-4. Disclosed: the census file was overwritten by the implementer's re-run** (deviation 4). `census.md` now shows the three new keys in its section 4; what it
      said before the re-conversion is `census_pre_conversion.md`. Nothing left.
- [x] **CMP2-5. Gate 6's route clause is NOT met as worded: five routes do not win and one moves, three of the five outside the census** (deviation 5; the verifier's
      medium; the gate's text is not rewritten). 122 of 128 routes win on exactly their census tick, 3-19 wins on 827 (census 832) and **2-31 (died on tick 170) and 3-20
      (not won by 585)** do not win: this step's. **2-01 (died on tick 283), 4-07 (not won by 470) and 4-29 (the tap of tick 525 is off the view)** do not win and the census
      does not name them: inherited, the same signatures as in step 173's own run (`CMP15/route_census.md`), no joint in any of the three, their f420 frames byte-identical
      before and after this step. The clause is met only against the route state step 173 recorded for HEAD `12ac5d5`, which was not rebuilt to re-measure it. `test_mp_routes`
      2,291 checks, 10 failures. Next: the route phase (planning step 176) re-flies 2-31, 3-20, 2-01, 4-07 and 4-29 and re-checks 3-19; the push gate "128 of 128 routes win"
      cannot be met before it.
      *CLOSED by step 176 (`45f6c05`), 2026-10-15: 2-31 (446), 3-20 (555) and 2-01, 4-07, 4-29 are re-flown and win, 3-19 is tightened (827); 128 of 128 win. The step's own
      gate text stays "NOT MET literally" as it was at its commit.*
- [ ] **CMP2-6. Evidence not taken or not measurable at f420** (deviation 6; GAMEPLAY: none). 3-15 and 3-19: the hinged body is off the view (about x 17-444 u and 330-757
      u, read from the player's start against where he stands on the frame; the frames are byte-identical before and after and the original's stills show neither body), so
      their pulls (+4, 0) and (0, +8) are pinned by `test_mp_hinge` only. 2-29: not measurable by the edge-shift method (the stone's phase fills the plank's mask); the stone's
      numbers are the sim's. 2-30: ONE pixel differs at f420 (294, 516; 4/255) where the spec predicted none; the cause (the old anchor derived through the rounded angle
      1.5708, 1.2e-4 px) is INFERRED, not measured. Next: nothing owned; a clip with 3-15's seesaw or 3-19's plank in view would decide the first two.
- [ ] **CMP2-7. The converter's datum-type pin does not detect the check's removal** (deviation 7; the verifier's low; GAMEPLAY: none). The verifier's mutation M10, a `float`
      datum accepted as a string, passes all 121 converter tests: the parametrised "a datum the engine does not read" fixture in `test_joint.py` gives it the Value `'2'`,
      which names no entity, so the joint is skipped either way. Not applied at the commit (a test change, not record text). Next: give the float and the other-typed datum a
      Value that names an entity existing in the fixture (e.g. `'pivot'`), the next time the converter's joint code is touched.

**Step 175's recorded deviations** (the breakable wall's contact, WALL, wave 4c-2; planning step 175 has the evidence, `RECORDED DEVIATIONS` 1-11 and the notes N1-N6;
`out/parity/program/WALL/` the reports, `impl.md`, `census.md`, `fix1.md`, `fix2.md`, `deviations.md` and `verify1.json` to `verify3.json`; GAMEPLAY: YES, 15 census
levels, and it moves eight routes (1-19, 1-20, 1-22, 2-29, 3-10, 3-12, 3-14, 4-21); there is no CMP-n row for it, and the rows it works on, Order 11.1's first clause,
R3b-2, R4a-1, R4a-3 and STK9-2, are annotated above and none is ticked, each ending in the route phase's re-flight; `test_mp_routes` fails 13 routes, the other 56 suites
pass, 33,535 checks; committed with these deviations by the owner's delegation):
- [x] **WALL-1. Gate 6's route clause is NOT met as worded: six routes outside the census's levels are off their census tick** (deviation 1; the verifier's medium; the
      gate's text is not rewritten). 2-01 dies on tick 283, 2-31 on tick 170, 3-20 is not won by 585, 4-07 by 470, 4-29's tap of tick 525 is off the view, and 3-19 wins
      on 827 for 832: the same lines in `CMP2/routes_after.log` and in `WALL/routes_before.log` (the committed tree replayed before this step's first edit), inherited
      from steps 171-174 and moved by none of this step. Required 0 off-tick, measured 6. It extends CMP2-5. The push gate "128 of 128 routes win" stays unmet until the
      route phase. Next: planning step 176.
      *CLOSED by step 176 (`45f6c05`), 2026-10-15: all six are re-flown or tightened and win (2-01 307, 2-31 446, 3-20 555, 4-07 446, 4-29 1,023, 3-19 827 with its `win_by`
      tightened); no route outside the census's levels is off its tick any more (planning step 176, deviation 5). The gate's text stays NOT MET at the step's commit.*
- [x] **WALL-2. The route census is 114 exact / 1 other tick / 13 not won, against the committed tree's 122 / 1 / 5** (deviation 2). The delta is eight routes, all on
      census levels, each preceded by its `wall contact` line, failing by design (the route leaned on the behaviour the step corrects): 1-19, 1-20, 1-22 (its second
      stone, 727, must be fast), 2-29, 3-10, 3-12 (R3b-2: a stone above 75 u/s at the slab), 3-14 and 4-21. 3-13, first thought broken, wins on its census tick 1043 since
      fix 2 and needs no re-flight. Next: the route phase re-flies the eight and the six inherited ones.
      *CLOSED by step 176 (`45f6c05`), 2026-10-15: the eight are re-flown (1-19 184, 1-20 825, 1-22 666, 2-29 564, 3-10 943, 3-12 636, 3-14 1,087, 4-21 360) and the census is
      now 117 exact / 11 other tick / 0 not won.*
- [ ] **WALL-3. The speeds the port reads at the walls are unmeasured** (deviation 3; the verifier's medium; GAMEPLAY: the routes). The gate (75 u/s) and the death are
      decoded; no clip shows a stone breaking a wall (D11_check C40: decode-only). Several natural stones read far under the gate on the port: 2-29 0.621 m/s, 4-21 0.470,
      3-12 0.117, 1-22's second stone 0.119, 1-09's hand play 0.697. Cause: the port's physics reads, not the rule (planning step 175, note N1). Next: the route phase
      proves a win on each with a stone clearly above 1.5 m/s (not a creep) and reports any level the rule seals; the owner may settle the unit with one rig clip of a
      stone rolling or falling into a breakable wall.
      *STEP 176 (the route phase), 2026-10-15, did what the row asked, and the row STAYS OPEN: every level has a win with a stone clearly above 1.5 m/s - 1-19 4.0 m/s, 1-20
      4.67 and 3.10, 1-22 3.17 and 3.28 (the second portal-fed), 2-29 2.96 (a portal in the stone's fall under the plank's end), 3-10 3.67, 3-12 3.33 (a 39-unit fall), 3-14
      3.33, 4-21 1.94 (a stone lifted into the wall's underside by the gravity agent: an aim band of 2 units, R4c2-3) - and **no level is sealed**. The speeds are the port's;
      none is measured on the original, and one rig clip of a stone rolling and falling into a breakable wall would settle this row, WALL-5 and 1-22's gap together.*
- [x] **WALL-4. `reachability.py`'s 108 / 20 / 0 is not evidence for this rule** (deviation 4; GAMEPLAY: none). The script has no speed term and no
      one-wall-per-destroyable-stone term (1-20 and 1-22 each have two walls and two destroyable stones), so it cannot show a level the gate newly seals. Next: the route
      phase (a fly proves it); optionally give the script both terms, or record that it cannot.
      *CLOSED by step 176, 2026-10-15: the fly proved it - 128 of 128 routes win, so no level is sealed by the rule. The script still has neither term (optional, not built).*
- [ ] **WALL-5. 3-13's wall verdict rests on a port inference and two marked guesses** (deviation 5; the verifier's medium). The history reads 1.333 m/s there (89 percent
      of the gate, holds on tick 689) and the free-flight replay 2.000 (breaks on tick 690); "the larger of two readings judges" is the port's, the original's callback
      runs once. It also rests on `earthquake.json` `impacts.free_flight` slack 1.0 and cone 1.0 (marked guesses, step 162's plateau edge). Cause: the port's engine
      brakes a stone before the contact; no capture. Marked as inference in `demolish.json` `_note`. Next: the owner's rig clip; the route phase flies 3-13 with a throw
      clearly above 1.5 m/s if it is re-flown anyway.
      *STEP 176, 2026-10-15: 3-13 was NOT re-flown (it wins on its census tick 1,043, so no author touched it) and the throw was not made faster; the row stands unchanged
      (R4c2-4).*
- [ ] **WALL-6. Gate 4's pins have one hole, and the verifier ran none of its own mutations** (deviation 6; the verifier's low). The `Game::AfterStep` -> `Demolish::Tick`
      hand-over (the stones' begins, `FreeFlightRules` from `Data::earthquake`) has no unit pin: an empty hand-over would be caught only by route 3-13, one of the red
      routes, and only in a tick-by-tick census compare. Each mutation needs a relinked runner (a Smart App Control exposure), so gate 4 rests on the fixer's per-rule
      mutation logs, audited by the verifier, whose counts equal the planning step's. Next: one non-route check on `Game::Tick` (3-13 or 1-24), or keep the route-census
      compare as an explicit gate of step 176.
      *STEP 176, 2026-10-15: the route-census compare WAS a gate of the route phase (its gate 4: the verifier's run agrees with `census_after_round1.md` on all 128 rows, and 3-13
      wins on 1,043), so a broken hand-over would have shown; the non-route check on `Game::Tick` is not written, and the verifier ran none of the mutations again. Open.*
- [ ] **WALL-7. Gate 7 (zero warnings) was not proven by a full rebuild after the sources were restored** (deviation 7). Fix 2's four builds read 0 warnings (`build_1` 53
      steps, `build_2` 47, `build_3_final_runner`, which recompiled the restored `Demolish.cpp` and `Game.cpp`, and `build_4_relink_runner`) and the verifier compiled the
      7 changed translation units at /W4 with 0 warnings; the commit role did not rebuild (no source changed: 16 of 16 md5s equal `WALL/fix2/final_source_md5.txt`). Next:
      the next role that builds runs `port_build.bat` and reads zero warnings.
      *STEP 176, 2026-10-15: still open. The route phase's verifier compiled the 8 changed translation units at /W4 with the build's own commands (0 warnings, 0 errors) and
      relinked the game once with no recompile (0 warnings); **no full `port_build.bat` of this tree has run in the whole of wave 4c-2** (R4c2-5). Not run by this close either:
      it would relink and notify the owner.*
- [x] **WALL-8. Binary housekeeping: `MagicPortals.exe` `357fae09` predates the restored sources by modification time** (deviation 8; the verifier's low). It was linked
      before the mutation runs from the same sources (the string `WALL_MUT` of the harness is absent from it; its `--route` replays print the final code's lines) and is
      older than `Demolish.cpp` and `Game.cpp` only because the restore touched them; `test_mp.exe` `2c74ad04` was linked after it. The first build after the commit
      relinks the game with a new md5 and the same code; no spot frame was taken (nothing was launched). Next: the first role that builds records the md5 and takes one
      spot frame.
      *CLOSED by step 176's verifier, 2026-10-15: it relinked once from the unchanged sources (`MagicPortals.exe` `f27dde1a`, no recompile, ran first time) and took the whole f420
      sweep on it: 128 of 128 byte-identical to `WALL/after/sweep`, the sweep taken on `357fae09`. The committed pair is `MagicPortals.exe` `f27dde1a` and `test_mp.exe` `2c74ad04`.*
- [x] **WALL-9. STK9-2 is narrowed, not closed** (deviation 9; GAMEPLAY: none). A stone whose wall breaks on the first look still has its shake and sound read from the
      history by `latchImpacts`; one that breaks on the second look (3-13) is read by the layer's own watch. The row STK9-2 carries a step-175 note. Nothing left in this
      row.
- [ ] **WALL-10. Two decoded constants live in `Demolish.cpp`, not in `demolish.json`** (deviation 10; the verifier's low; GAMEPLAY: none). `kDestroyableKey`
      ("destroyable") and the `== 1.0` literal (the `CMPIu` immediate of #52-53), both cited; every tunable is in JSON. Non-wall breakables keep the touch rule (the
      existing row on a carranca; no live case in the 128 scenes; planning step 175, note N4). Next: move both into `demolish.json` `wall` at the next change that relinks
      anyway.
- [x] **WALL-11. Disclosed: no f420 frame changes** (deviation 11; GAMEPLAY: none). 128 of 128 PNGs are byte-identical to `CMP2/after/sweep`: the gate and the death are
      play-time, so the sweep cannot show them, and the pair images of 1-24, 3-13, 4-21 and 1-09 read by eye show the same state as before. Not a failure; recorded so no
      later role takes the byte-identity as proof of the rule. Nothing left.

**Wave 4c-2's own rows** (the route phase, planning step 176, and the wave's close; `out/parity/program/R4c2/` holds the reports: `mutations.md`, the censuses
`census_after_steps.md` and `census_after_round1.md`, the round notes `round1_<level>.md`, `verify1.json`, `finalize.md` and `close.md`; GAMEPLAY of step 176: none, of the wave:
steps 171-175, each in its own block above). **No level is without a route and no `finding_*.md` exists, so there is no row for an unwon level.**
- [ ] **R4c2-1. 1-20 no longer unlocks achievement 48 (Be smarter I, 8 portals or fewer; `game/data/achievements.json` id 48, world 0 level 19), and no route demonstrates it**
      (medium; the verifier's first problem; step 176's deviation 1). The old route unlocked it on tick 596 with 8 shots because ONE stone broke BOTH walls, which step 175
      (R4a-1, Order 11.1's first clause) took away: the stone dies at the first wall. The new route needs 10 portals (`achievements: none (door: medal 2, 10 portals, 13200 ms)`):
      the ledge, wall 1056, the ramp and the mirror each cost a pair, and the second stone costs one more because no ray from the ledge, the floor or the plank reaches it
      (only the block's top does). The author found no 8-portal way on this build. `test_mp_routes` went from 2,366 to 2,365 checks (the unlock check is gone). Required: the
      route gate's "achievements as before"; measured: four stealth ones (26, 28, 32, 42) unchanged, 48 gone: **NOT MET literally**. Whether 8 portals is reachable under the
      75 u/s gate is unproven, and if the original's players do it in 8 then a rule of the original differs from the port's here (the shot's ray reaching the stone over the
      block, or the stone's reach). No clip of 1-20 exists (`footage/INDEX.md`). Next: an 8-portal route, or footage of 1-20, or the owner's call.
- [ ] **R4c2-2. 1-22's natural two-portal solution is a port gap, and the shipped route stands on two unmeasured things** (medium; the verifier's second problem; step 176's
      deviation 2; `round1_1-22.md` "PORT GAP"). Pushing both stones off the left end and stepping after them wins on this build on tick 449 (medal 3, 2 portals, all 5 crystals;
      kept as `R4c2/round1_1-22_intended_2portals_NOT_A_ROUTE.json`, not in `tests/routes/`) but only in a narrow chaotic cell (3 to 5 of 10 aim points win, scattered): the engine
      brakes a closing stone early (`PhysicsSystem.cpp:1142-1584`, about 6.2 units, leaving it 2.2 to 3.8 units short of wall 755's face), the wall's first look needs the stone
      within 1 unit (`demolish.json` `contact.margin_px`, a `_guess`), so the stone sits short, lands, creeps at 0.12 m/s and the wall holds; it breaks only if the stone's last
      free step ends within [286.0, 287]. INFERRED, not measured (no clip shows a stone breaking a wall, D11_check C40): Box2D does not brake early, so the original's callback
      would read the roll speed. The shipped route feeds the second stone through a pair (3.28 m/s, velocity inverted: **a stone put at a portal against a breakable wall breaks it
      is unmeasured on the original**) and **depends on stones coasting at about 0.21 u/tick once the player stops pushing** (observed, cause not established): the engine's
      friction step (G10-14) would re-time it. Next: read the callback's velocity at the stone's last free step where the brake stops a pair short (the wall's second look), or a
      wall margin of 4 units (a `_guess`), or the owner's rig clip of a stone rolling and falling into a breakable wall (settles this, WALL-3 and WALL-5 together).
- [ ] **R4c2-3. 4-21 is a knife-edge in its aim, on the owner's 60 Hz** (medium; the verifier's second problem; step 176's deviation 2; `round1_4-21.md`). A stone can only be fast
      against that wall from under it: the gravity agent's pull (`gravitywell.json`, 0.5 m/s a reference frame) beats gravity only within about 190 units of the agent, so a stone set
      above the wall is lifted away. The exit pair's aim wins for y 198.5 to 200.5 at x 395 (x 392 to 401 at y 199.5; speeds 1.5 to 2.2; a stone set a little lower reads 0.36 to 0.54 m/s
      and the wall holds), and the band moves with the frame-rate factor `fpsFix` (G10-2, the owner's call: INFERRED, at 30 fps the original's per-second pull is twice the port's).
      Timing edges are 2 to 16 ticks. The route is deterministic; its fragility is in the aim. Next: G10-2's ruling; re-aim if the rate changes.
- [ ] **R4c2-4. 3-13 was not re-flown and still stands on WALL-5's inference** (low; step 176's findings). It wins on its census tick 1,043, so no author touched it; its wall verdict
      (history 1.333 m/s holds, the free-flight replay 2.000 m/s breaks, "the larger of two readings judges") is the port's, and step 175 said the route phase would fly it with a
      clearly faster throw "if it is re-flown anyway". Next: WALL-5's rig clip; re-fly with a faster throw then.
- [ ] **R4c2-5. No full `port_build.bat` has run on this tree in the whole of wave 4c-2** (low; step 176's deviation 4; WALL-7). The steps' build logs, the verifiers' scratch
      compiles (the 8 changed units at /W4: 0 warnings) and one relink (`f27dde1a`) are the evidence for zero warnings; the committed pair is `MagicPortals.exe` `f27dde1a` and
      `test_mp.exe` `2c74ad04`. Not run by this close (a relink notifies the owner). Next: the next role that builds runs `tools\parity\port_build.bat` and reads zero warnings;
      the default target's stale `MagicPortalsSpike` (step 161's deviation 5) is still there.
- [ ] **R4c2-6. Routes that rest on port behaviour not measured on the original** (low; step 176; R4a-8's kind; each says so in its `_about`): 3-10 (a stone out of a portal over
      the knight crushes him before it breaks the wall; a key whose carrier dies stays in the air where he stood; the knight still at `wayB1` at about tick 460 is the port's
      timing), 3-12 (the slab breaks by a 39-unit fall at 3.33 m/s, 2.2 times the gate; a floor player cannot place a portal above the slab), 2-31 (the plank's crossing rests on
      CMP-2's pose and the plank's density 1.42 against the player's 0.104; the agent window is chaotic past tick 230), 2-29 (the stone's clock is the port's: WT-10, CMP-18) and
      3-20 (**no cell had a +-1 margin on every axis**: the ceiling aim x wins from 252.6 to 253.4, the right hold must end on 316 or 317, the knight's throw is chaotic to about 0.5 px
      of landing x; the two walks 125-168 and 175-238 fail on any +-1, inherited: R3-4). 3-14's tail is a new idea forced by the stone's death (the crate presses both buttons in
      turn). Two achievements are exercised by no route: 48 on 1-20 (R4c2-1) and 31 on 3-09 (gold with exactly 2 knights alive: its route earns none, `achievements: none`, medal
      2). Next: the final comparison and the rig list (R3c-11).
- [ ] **R4c2-7. Five unmoved routes' sounds changed and no single step is traced** (low; step 176's findings). They still win on exactly their ticks and the suite does not pin
      sounds: 1-13 (`stone_hit` x2 -> x1), 3-13 (`stone_hit` x2 -> none, `player_landed` x5 -> x4), 1-24 (`stone_broken` x3 -> x2, `stone_hit` x2 -> x1), 1-25 (`stone_hit` x2
      -> x1) and 3-09 (one `minion_fell` and one `minion_landed` gone). The stone lines are most likely step 175's death and speed gate; 3-09's may be step 171's, 172's or 174's.
      Next: nothing owned; a per-step replay would attribute them.
- [ ] **R4c2-8. What the route phase's verifier did not run** (low; step 176's deviation 7; `verify1.json` `notRun`). Any step's port-side mutation (each needs a relinked runner: see
      CMP2-2, CMP15-3, CMP16-2 and WALL-6), CMP6's data-rule second witness (CMP6-8: a tracked-file edit), any measurement on the original (the speeds at the walls, 2-31's plank
      crossing, 3-15's and 3-19's pulls: decode or inference only), an in-window replay of the other 126 routes (the headless runner replays all 128 twice with identical win lines;
      only 1-25 and 1-19 were replayed with `--route` in `MagicPortals.exe`), and the gold-seed pair images of 4-07 and 3-13 read by eye (their numbers equal the before's).
- [ ] **R4c2-9. `win_by` of the 114 untouched routes is still looser than 10 to 15 on some (up to 60; median 21 over the 128)** (low; step 176's left-for-later; R3b-6 and R3c-9's
      kind). The 14 files of step 176 are 12 to 15 over their tick. Next: tighten when a file is next touched.
- [x] **R4c2-10. Settled at the commit and this close: step 172's record said 7.04 -> 3.31 where the crate's own gain is 4.20 -> 3.31, and "met" where its gate 6 was not** (the
      verifier's two lows; step 176's deviation 3). The two cells of planning step 172 are corrected in `45f6c05` (the original numbers stay visible, marked corrected); the
      backlog's CMP-16 note is corrected by this close; **the pushed commit message of `9d643c9` still says 7.04** and is not amended (history is not rewritten). Nothing left.
- [x] **R4c2-11. Disclosed: the 14 route authors shared `%TEMP%` and the scratchpad** (step 176's deviation 6). Two authors (2-29, 3-14) found their scratch files overwritten, and
      4-07's author saw ONE launch print `test_mp_routes: 3 checks, 1 failures` that never played the route, most likely another author's runner using the same
      `%TEMP%\supersonic-test-mp-routes` files at that moment (inferred; the identical command repeated gave 20 checks, 0 failures and never recurred in several hundred launches
      with a private `TEMP`). It is no evidence that a route is nondeterministic: the verifier replayed all 128 twice, identical. Next, for any phase that runs authors side by side:
      give each its own `TEMP` and scratch folder.
- [x] **R4c2-12. Disclosed: five routes leave the 455 x 256 view for a few ticks** (the verifier's low). The player's centre is outside it for at most 5 consecutive ticks: 3-18
      (5), 4-18 (5), 2-27 (5), 1-21 (3), 3-09 (3); none of the 14 files of step 176 does. Nothing to fix.
- [x] **R4c2-13. The three code mutations of R4c1-3 (b) ran, and the wave's close settles CMP7-3, CMP3-6 and R4c1-3 (b) on them** (`R4c2/mutations.md`, 2026-10-12, no commit; one
      mutated runner `81127657`, 0 Smart App Control refusals; `test_mp_keys` 182 / 0 as the control, `test_mp_sprites` 2,061 / 3, `test_mp_layer` 4,151 / 6). A (the key's light not
      a tick behind) and B (a particle share on the key's light) are each caught by `test_mp_layer` by a pin of their own (:21728 and :21687); C (a column-major bucket order) is
      caught by `test_mp_sprites` only (:4107, :4112, :4162) and **survives in `test_mp_layer`**: no mutation survived the suite that owns its rule, and the layer's own order test has
      a gap, CMP6-9 (the three pairs that close it are in that row). Nothing left here.

**Wave 4c-1's own rows** (the wave's three verifications, two fix rounds and the finalize; `out/parity/program/R4c1/` holds the reports: `verify1.json` to `verify3.json`,
`fix1.md`, `fix2.md`, `finalize.md` and `close.md`; GAMEPLAY: none):
- [x] **R4c1-1. Gate 4 (the sweep chain) failed as worded on one level, 4-12, and needed a scope ruling: RULED (a), 2026-10-12, and the wave pushed** (medium; the one finding of all three
      verifications: `verify1.json` at `5c1949b`, `verify2.json` at `1379eca`, `verify3.json` at `753e7d0`). Required (the wave's gate 4): the owner-seed f420 sweep of the final
      build equals `CMP10/after/sweep` byte for byte, and against `PX/after/sweep` every changed level lies inside the union of the five groups' named lists (CMP-1 78; CMP-3
      its 9 and its 40-level table; CMP-7 4-19 and 4-20; CMP-9 "the 32"; CMP-10 13) and in some step's explained table. Measured (verify3): the first half holds (128 launches,
      128 of 128 byte-identical, validation silent); 106 levels differ from the old baseline (CMP-1 77, CMP-3 49, CMP-7 1, CMP-9 40, CMP-10 9: 176 level-changes, the steps
      overlap) and **one lies outside the union: 4-12**, which changes only through step 169's fire-agent pulse (2,203 px in the agent's box, at most 25 grey levels; its
      whole-frame mean abs to h6.3 11.109 -> 11.106). The fire agent is on 20 levels; 10 are outside CMP-9's 32 (2-10, 2-11, 2-20, 2-26, 2-27, 2-30, 2-31, 3-23, 4-12, 4-28), nine
      of them inside CMP-1's 78 and so counted as explained there; 4-12 is in no group's list (only CMP-low's). **There is no port defect**: `ETHCallback_fire_agent`
      (`out/parity/asbc/all_functions.txt`, md5 `48fe65ce`) runs `bounce(this, 1.02, 0.98, 300)` (#282-300) on every fire agent with no branch before it (every jump lands at #268
      or earlier, the only RET is #304; re-read by verify2 and verify3), and 4-12 holds `fire_agent_2236` (`out/levels/level11c.tscn` lines 42-48), so the original pulses it as the
      port does. The spec contradicted itself (its BUILD line and CMP-9's own text ask for the pulse; its sweep gate says the 32) and fix 1 (`1379eca`) corrected step 169's gate 1
      to 42 levels; a step rewriting its own gate is not the wave's gate changing, so each verifier kept failing it. The ruling is neither the finalize role's nor this close's.
      Options: **(a)** accept CMP-9's scope as 42 levels (the 32 and the ten above): gate 4 then reads 106 changed levels, each step's changed set inside its own scope, and the
      wave pushes as built with nothing rebuilt (verify2 and verify3 note that it agrees with the standing rule that rulings follow the original, and fix 2 recommends it); **(b)** veto the pulse: the
      revert is the recipe in step 169's deviation 1 (the pulse line in `syncSprites`, the owner-scale pass in `syncLights`, `fire_agent.pulse` with its loader requirement in
      `Art.cpp`, `Art.hpp` and `art.json`, its pins in `test_mp_sprites` and `test_mp_layer`), then a re-sweep of the 128 levels and a relink of both executables (a Smart App
      Control risk), compared with `CMP10/after/sweep`, the one sweep this close keeps (only the fire-agent levels' frames should move).
      **RULING (2026-10-12, the orchestrator, on the owner's standing order of 2026-10-10, "just continue doing each step until you are finished or until i tell you to stop"):
      option (a).** Why: the original pulses every fire agent (`ETHCallback_fire_agent` #282-300, no branch before it, re-read by two verifiers), so the port's 20 are the
      original's 20 and a veto would make the port differ from the original, besides costing a relink of both executables (a Smart App Control notification each); the spec
      contradicted itself (its BUILD line asks for the pulse, its gate 4 names 32 levels) and the scope it should have named is the census of fire agents; three rounds found no
      other defect. **Gate 4 as written FAILED on 4-12 and stays recorded as failed as written; it holds under the 42-level scope** (106 changed levels, each step's changed set
      inside its own scope). The wave was pushed as built, a fast-forward from `16b78e1`, its last commit the docs commit that carries this ruling. Not one of the owner's calls
      (no engine change, no pin bump, no gameplay) and reversible by an ordinary commit (the recipe is step 169's deviation 1); the owner may overrule it.
- [ ] **R4c1-2. Step 169's "over their changed pixels alone 8 of the 10 read slightly away" does not reproduce, and the pulse is not shown to help any frame at f420** (low;
      verify3's problem 2; the sentence is in planning step 169's deviation 1 as `753e7d0` wrote it, and this close does not edit the planning document). On verify3's estimator
      (luma mean abs to h6.3 over each level's changed pixels, owner seed, `CMP7` -> `CMP9`) the fire agent's ten outside the 32 read: away 2-11 18.9 -> 20.5, 2-26 32.2 -> 32.9,
      2-27 15.5 -> 15.8, 2-31 55.2 -> 57.8, 3-23 17.8 -> 19.2 (5); flat 2-10 20.37 -> 20.41 (1); toward 2-20 24.4 -> 24.2, 2-30 9.1 -> 8.6, 4-12 34.1 -> 33.3, 4-28 21.7 -> 21.2 (4);
      the whole frame moves 0.006 or less either way. The pattern is step 169's (a 0.35 percent scale change moves edge pixels) and no gain is claimed: read it as "about half read
      slightly away". 4-12's six stills lie inside the port's f420-f504 range as a phase picture (1 of 6 before the pulse; CMP9-1). Next: nothing; the final comparison reads it
      as a phase picture.
- [ ] **R4c1-3. What the verifications left or noted** (the three verifiers' `notRun` and lows; low). (a) **Seven changed levels read slightly away from the stills over their
      changed pixels**, each recorded by its step: CMP-3's 1-01 (+2.4 over 157 px, at most 16 grey levels), 2-13 (+3.6 over 126 px) and 3-12 (+0.3 over 24 px), CMP-9's 1-13
      (+0.3), 1-22 (+0.7) and 2-19 (+1.1) (the walls' depth and phase) and CMP-10's 1-02 (+0.3 over 8,399 px; its level-start popup is open at f420); rows CMP3-4, CMP9-6 and
      CMP10-4 own the causes, and the final comparison reads the walls and the block as phase pictures. (b) **The code mutations nobody ran**: step 168's two (one tick behind, no
      particle share: CMP7-3) and step 167's column-major axis (CMP3-6); a code mutation relinks the runner, and wave 4c-2 relinks anyway: run them at its first relink.
      **DONE 2026-10-12 (`R4c2/mutations.md`): A and B caught by `test_mp_layer`, C by `test_mp_sprites` and not by `test_mp_layer` (CMP6-9); CMP7-3 and CMP3-6 are ticked on it.** (c) **No
      frame of 4-19 with its breakable wall in view, no clip of a flagged wall, none of 1-02 or 1-10 from the load frame, and 1-03's gold frame was not retaken** (CMP7-4, CMP9-3,
      CMP10-4): the wall's and the block's phase against the original is decoded, not measured. (d) **Capture sweeps stall when the capture window is minimised on a desktop in
      use** (verify1's low, which advises): allow 40 minutes for a 128-level sweep, or add a 10 s watcher that restores `MagicPortals.exe`'s own window (never another process's).
      (e) **Records left as written**: `R4b/CMP/groups.json` (under `out/`) still says "the 32" for CMP-9; step 169's gate 1 in the planning document carries the corrected scope
      and the planning document names no commit hash for steps 166 to 170 (this file does: the table in the "Wave 4c-1" paragraph).

**The final comparison's rows** (wave 4b; 128 levels, 18 causes and CMP-low; GAMEPLAY: none in the comparison itself, each row says its fix's). Evidence is under
`out/parity/program/R4b/CMP/` (`residuals.md` has every reader's line with its box, `groups.json` the causes and proposals, `per_level.md` the table): the pair images
`pairs/<id>_h63_pair.png` (the original above the port, half scale) and `pairs/<id>_h63_overlay.png`, port frames `port_f420/<id>_f420.png`, are kept (named in
`out/parity/BASELINE.txt`); the `_h0.3`, `_h2.8` and f450 pair images and the readers' crops were deleted at the wave's close and are re-taken by `capture_CMP.sh` and
`pairs_run.py`. A box is `x1,y1,x2,y2` (or a pair of corners) in the 1280 x 720 frame. The listing is `out/parity/asbc/all_functions.txt` (md5 `48fe65ce`). Every cause
below was CHECKED by the comparison's triage against the listing or the Ethanon source (its own re-derivation of 11 reader attributions: 10 held); the independent
verification re-derived only CMP-1's census (78 levels carry `ignore_emissive`, 0 hits in `game/`, the converter, `data/` and `tests/`) and re-read six pair images.
**Re-taken 2026-10-15 (wave 4d-1):** every row below carries a 'Re-taken' note (status, levels, numbers, what is left); the re-take's evidence is under
`out/parity/program/R4d/CMP/` with the same file names (`old_vs_new.md` sets the new result against the old level by level); the old pass's pair images and port frames
named above were purged at that wave's close and `out/parity/BASELINE.txt` now names the R4d folders (`port_f420/`, `pairs/`); CMP-19 and CMP-20 are new rows and R4d1-1
to R4d1-4 follow CMP-R1.
Fixability: `decoded` (the mechanism is read from the listing and a fix can be specified), `needs-rig` (a clip of the original is needed), `owner-call`, `unknown`.
- [x] **CMP-1. Script-driven emissive: the player and the exit arch are drawn lit on the 78 levels that carry `ignore_emissive`** (high; light; `decoded`; moves no
      route). Levels: 2-01 to 2-32, 3-01 to 3-32, 4-01 and 4-20 to 4-32 (the player in all 78; the arch in frame on 45). Evidence: 2-01 player box [965,60,1060,195] and
      arch [915,275,1190,645] in `pairs/2-01_h63_pair.png`; 2-05 arch [1030,0,1280,330]; 2-03 arch [0,260,205,640]; the player is a black silhouette in the original on
      chapter 2 and on 4-20 to 4-27 and 4-29 to 4-32 (high), dim and matte on chapter 3, 4-01 and 4-28 (medium). Measured ratios fit the rule: 4-01's player
      0.70/0.70/1.00 = its ambient (0.7, 0.7, 1) with emissive 0; 3-05's 0.33/0.31/0.41 = ambient (0.1, 0.1, 0.2) + 0.2; 3-09's arch 0.373/0.368/0.559 = (ambient + 0.1)
      / min(1, ambient + 0.7). Cause (listing): `Game::readProperties` #55-82 reads the properties node's `ignore_emissive` and `door_emissive` (absent = 0), #285-300
      call `door.SetEmissiveColor(door_emissive)`, and `Game::preLoop` #355-393 sets the character's emissive to 0, or 0.2 when `g_currentWorld == 2` (chapter 3), on
      such levels. The port and the converter read neither key (the converter already writes both as metadata); `art.json`'s character (1,1,1) and `door.ent`'s 0.7 are
      drawn as the files say. The arch is `door.ent` (window01.png), not `door_bg.ent`, which the chapter-4 readers named. Chapter 1 and 4-02 to 4-19 carry neither key
      and read clean (the control). Joins: "Script-driven emissive" (section 1), G1a-11 (the six darkest levels: this is its general case), planning step 45. Next: read
      both keys and the chapter index where `art.json`'s emissives are applied (player 0.2 on world 2, else 0; `door.ent` = `door_emissive`, on `ignore_emissive` levels
      only), the rule and its citations in `game/data/lighting.json`. Gate: the f420 sweep changes only the 78 levels (the other 50 byte-identical to `PX/after/sweep`),
      the player and arch boxes read the original's (chapter 2 player RGB 8-35, arch 5-12 grey); pin 2-01, 3-05, 4-01 and 4-22 in `test_mp_art` or `test_mp_layer`.
      GAMEPLAY: none. Of the 58 `differs` levels, 52 differ by this cause alone.
      *Closed by step 166 (CMP1, wave 4c-1), 2026-10-11; committed as `baaa651` (wave 4c-1's close; pushed on 2026-10-12 after R4c1-1's ruling). GAMEPLAY: none (the 128 routes win on exactly the census's
      ticks).* Built as the Next says: the port reads `ignore_emissive` and `door_emissive` from the level's first `properties` node and the chapter's 0-based world; the flag
      gates both writes (4-02 to 4-19's `door_emissive` 0 is never read); on a flag level `door.ent` is drawn at `door_emissive` (0 when absent) and the player at 0 (0.2 in
      chapter 3). The rule and its citations: `game/data/lighting.json` `script_emissive`; pins in `test_mp_lighting` (548 -> 588 checks) and `test_mp_layer` (3,876 -> 3,905).
      Measured: the f420 sweep changed 77 levels, all inside the 78, and 51 are byte-identical (the 50 outside and 2-32: its player burns at tick 301 and no arch is in frame,
      so its rule is shown at f200 only); the readers' boxes read the original's, mean abs luminance error player 43.8 -> 0.7 and arch 31.6 -> 0.7, `compare.py` `mean_abs`
      against h6.3 down on 77 of 78 (4.64 -> 3.43), up on none; 57 suites, 32,383 checks, `test_mp_routes` 128 of 128. Independently verified (`CMP1/verify1.json`: 9 gates,
      4 low). **What it leaves**: (a) the gate's "chapter 2 player RGB 8-35, arch 5-12 grey" was a summary, not a bound (the original's own arch boxes read 70.9 on 2-16, 69.1
      on 2-30, 25.0 on 2-05; 26 of 31 chapter-2 player boxes read 35 or under), so the gate used the six stills' spread; (b) outside that spread, each another cause: 2-28's
      arch (an orange particle cloud over the door at f420, INFERRED from the pair image), 3-25's and 3-26's arches (the door's red halo, CMP-8, INFERRED), 4-20's player (the
      red key's light, CMP-7); (c) 2-02's player is redder than the original's, (46.4, 27.1, 33.0) against (29.6, 24.0, 30.1), inside its stills' range 27.7-41.8: a light on the
      figure, read with CMP-7 and CMP-8; (d) the 52 `differs` levels that differed by this cause alone are read again after the groups that remain. The deviations are rows
      CMP1-1 to CMP1-6 above. Closes G1a-11; section 1's "Script-driven emissive" stays open for its other items.
      *Re-taken 2026-10-15 (wave 4d-1, `out/parity/program/R4d/CMP/`; the build of step 176, exe `f27dde1a`): FIXED on 78 of 78, by step 166. 77 levels read the
      original's player (and the arch where in frame) at h6.3 / f420 with port-over-original ratios of 0.93 to 1.01 (2-17 player 75.2/38.6/43.3 against 77.1/38.9/43.5,
      arch 33.7/13.8/12.6 against 36.6/13.9/12.1; 2-22 player 26.7/24.6/30.4 against 26.2/24.2/30.0; 4-26 player 1.2/0.2/0.3 against 1.2/0.3/0.2; the lowest, 3-07's
      player, 0.93 to 1.00). 2-32 is not readable at f420 (its player is dead behind the game-over card); its h0.3 pair shows the player dark with the red eye in both. A
      scan of all 490 residual texts finds no lit player or arch left and no level where the fix overshoots. Of the 58 `differs` levels, the 52 that differed by this
      cause alone now read 19 `match` and 33 `minor`, none `differs`. Nothing is open here; keep the step's pins.*
- [x] **CMP-2. Revolute-joint pose: the port drops attach point B, so a plank or arm stands where it was placed, not where the original's joint pulls it** (high;
      structural; `decoded`; MOVES ROUTES). Seen: 2-31 (box [0,480,520,720], `pairs/2-31_h63_pair.png`, identical in all three original stills and every port frame, so
      a static pose), 3-20 (box x720-1090 y395-505; best shift (+6, 0) on the plank, box x845-960 y395-505), 4-31 (box [960,190,1170,285], the bar's edges 9 px off,
      measured on columns 1100 and 1140). In scope, 10 hinge levels: 2-28, 2-29, 2-30, 2-31, 3-15, 3-19, 3-20, 3-22, 4-11, 4-31. Cause: `ETHRevoluteJoint.cpp:35-44`
      sets both local anchors (A = attach point A x half of A's box, B = attach point B x half of B's box, B the first entity the joint's custom data names) and Box2D
      pulls the dynamic body so the two world points meet; the converter writes only `joint_anchor_x/y` (A's) and `game/sim/Hinge.cpp:197-205` says B's dropped attach
      point "costs nothing". Predicted pulls of the plank centre: 2-31 (-14.0, +28.5) u [measured (-14, +29)], 3-20 (-2.0, 0) [2.1 u], 4-31 (0, -3.0) [3 u = 9 px], 2-30
      (0, 0) [matches]: all four to 0.5 u; unmeasured: 3-19 (0, +8), 3-15 (+4, 0), 2-28 (+0.6, +2), 2-29 (+0.6, 0), 4-11 (-0.4, 0), 3-22 (0, 0). Neighbours: step 134's
      note that the 2-31 route rests on the port's seesaw, R3-4, WT-10 (CMP-18). Next: the converter carries `attachPointB` (x, y) per instance beside
      `joint_anchor_x/y` (a change on both sides of the `.tscn`: convert into a staging directory under `out/`, compare with what the port reads, then replace;
      `tests/test_mp_levels` counts), `Hinge.cpp` sets `connectedAnchor` = attach B x half-box of B (rotated by B's angle) and places the body where the joint puts it.
      Gate: the four measured shifts reproduced to 0.5 u on f420 frames of 2-31, 3-20 and 4-31 with 2-30 unchanged; `test_mp_hinge` pins; then re-fly the nine routes
      (2-28, 2-29, 2-31, 3-15, 3-19, 3-20, 3-22, 4-11, 4-31), once, after the wave's other gameplay fixes. GAMEPLAY: yes (2-31's seesaw moves 29 u down and 14 u left,
      3-19's plank 8 u).
      *Closed by step 174 (CMP2, wave 4c-2), 2026-10-12, resumed and finished 2026-10-14, WITH RECORDED DEVIATIONS (gate 6's route clause is NOT met as worded: CMP2-5); committed in wave 4c-2 (the commit titled "Magic Portals: a hinged body stands
      where its joint holds it", `cad4a2f`; pushed with the wave, `7e708e3..45f6c05`, 2026-10-15). GAMEPLAY: YES, and it
      MOVES ROUTES: eight of the ten hinged bodies start somewhere else, 2-31's seesaw 31.8 px (89 px on screen) from where the level places it; 2-31's route no longer wins
      (the player lands on the moved seesaw on tick 119 and dies on tick 170; census 405), 3-20's no longer wins (the shot of tick 255 ends on the plank; not won by 585;
      census 555) and 3-19's wins on tick 827 (census 832); the other census levels' routes (2-28, 2-29, 2-30, 3-15, 3-22, 4-11, 4-31) win on exactly their ticks. So
      `test_mp_routes` fails on FIVE routes until the route phase (planning step 176): 2-31 and 3-20 (this step) and 2-01, 4-07 and 4-29 (inherited from steps 171-173,
      outside the census; CMP2-5), and 3-19 is to be re-checked.* Built as the Next says, on both sides of the `.tscn`: the converter reads `attachPointBX/BY`, the joint's
      name, `otherEntityName` and the datum's type and writes, beside the eight `joint_*` keys, `metadata/joint_other` (B, found as `ResolveJoints` finds it, the first matching
      instance) and `metadata/joint_anchor_b_x/y` (attach point B x half of B's box x B's `<Scale>`, in B's own frame); `Hinge::Find` requires the three keys (a level
      converted earlier is refused and told to convert again), checks `joint_other` against the entity the datum names, puts the world point on B's centre plus attach point B
      turned by B's `rotation`, places the body where that point less A's own attach point leaves it and gives the engine's hinge `anchor` = A's attach point and
      `connectedAnchor` = attach point B (`ETHRevoluteJoint.cpp:35-44` and `:60-64`; `docs/ethanon-formats.md` has the citations). No tunable moved (`hinge.json`'s notes
      cite the decode). The census (derived from the original's raw `.esc`, the four `.ent` that ship `<Joints>`, the 128 converted scenes and the listing; the hint exact):
      10 joint instances in 10 levels, all `revoluteJoint` through a `string` datum, one B candidate each, A dynamic and B static, no motor, attach point A (0, 0), attach point B
      nonzero. Pulls (units; 1 u = 2.8125 px): 2-28 (+0.6, +2.0), 2-29 (+0.6, 0), 2-30 none, **2-31 (-14.04, +28.49)**, 3-15 (+4, 0), 3-19 (0, +8), 3-20 (-2, 0), 3-22 none,
      4-11 (-0.4, 0), 4-31 (0, -3). The re-conversion changed 10 of 128 `out/levels` files by exactly three added lines each (a fresh conversion is byte-identical to it; the
      verifier's own is too). Measured: the f420 sweep (owner's seed, 128 launches, validation silent) changed 7 levels and left 121 byte-identical: 2-28, 2-29, 2-31, 3-20,
      4-11, 4-31 and **2-30 by ONE pixel** (294, 516), 4/255, rounding of the old derived anchor, INFERRED (CMP2-6); 3-22 (pull 0) and 3-15 and 3-19 (the bodies are off
      the view at f420) are unchanged, rightly. The plank against the original's stills, edge-shift, before -> after: 2-31 (-14.22, +28.44) -> (0, 0), 3-20 (-2.13, 0) -> (0, 0),
      4-31 (0, -2.84) -> (0, 0) (predicted (-14.04, +28.49), (-2.0, 0), (0, -3.0)), 2-28 and 4-11 -> 0, 2-30 unchanged; `compare.py` ALL-GOLD h6.3 edge_iou / mean_abs 2-31
      0.5191 / 5.51 -> 0.6369 / 2.56, 3-20 0.7427 / 4.26 -> 0.7830 / 3.88, 4-31 0.8278 / 1.64 -> 0.8605 / 1.31, 2-28 0.5166 / 5.65 -> 0.5674 / 5.36, 4-11 0.7627 / 6.46 ->
      0.7790 / 6.32; the pair images of 2-31, 3-20 and 4-31 read by eye (the seesaw with its ball and diamonds, the plank with its pivot, the bar with its pivot, where the
      original's are). Pins: `test_mp_hinge` 28 -> 102, `test_mp_levels` (`metaStrings` 17,774 -> 17,784, `metaNumbers` 80 -> 100), the converter's `test_joint.py` (9 tests;
      121 passed, `ruff` clean); eight converter mutations and six port mutations (three builds; the last three together: 16 failures of 102) each fail a named pin. 57 suites, 56 pass, 33,390 checks
      (`test_mp_routes` 2,291, 10 failures = 5 routes x 2), reachability 108 / 20 / 0, 0 warnings. Independently verified once (`CMP2/verify1.json`: pass; ten gates and the housekeeping check ok; its
      medium, the route clause, and its lows are the rows below). 2-29's stone: the plank's first turn past 0.05 rad is tick 254 before and after and the stone is 1.04 u
      farther along at tick 420, so the new pose does not by itself explain WT-10's / CMP-18's 125-135 px lag. **What it leaves**: (a) the five routes (CMP2-5; planning step 176); (b) WT-10
      / CMP-18 stays open (the pose moves the stone 1 u of the 125-135 px; its trace, the stone against the plank and the hinge, is the next step on it); (c) 3-15, 3-19 and 2-29 not measurable at f420 and 2-30's pixel inferred (CMP2-6); (d) the
      port-side mutations nobody re-ran (CMP2-2); (e) the converter's datum-type pin (CMP2-7); (f) `placedPx` / `heldPx` (CMP2-3). The deviations are rows CMP2-1 to CMP2-7
      above the wave 4c-1 rows.
      *Step 176 (the route phase), 2026-10-15: 2-31 is re-flown (`level30a.json`: portal at (418, 247) and one on himself, the plank's right half, wins on tick 446, was 405),
      3-20 re-aimed (tap 254 at (253, 40), not (258, 40): the plank's tip moved 2 units left; wins on 555, the same tick, no cell has a +-1 margin on every axis) and 3-19
      tightened (inputs unchanged, wins on 827 for 832; `win_by` 865 -> 840); the other six census routes (2-28, 2-30, 3-15, 3-22, 4-11, 4-31) win on exactly
      their census ticks (2-29 was re-flown for the wall, step 175, not for the joint). 2-30 still wins on its creep, 1,386 (R3-2).*
      *Re-taken 2026-10-15 (wave 4d-1): FIXED where in frame, 8 of 10, by step 174 (`cad4a2f`): 2-28 the plank and its red pivot ball agree in outline and brightness;
      2-29 the pose agrees; 2-30 plank, pivot and pillar within 2 px (R3-2 untouched); 2-31 the seesaw agrees (the old pair hangs the plank about 29 u low, the new equals
      the original); 3-20 the plank 0 px (error 1.57 at shift 0; the old reading 6 px); 3-22 the plank placed as the original's (error 4.25 at shift 0); 4-11 the seesaw
      and its red ball at the original's place; 4-31 the red ball at (988.3, 202.5) against (988.4, 202.6) and the bar rows 200-243 against 202-242. 3-15 and 3-19 hold
      the hinge off frame (predicted +4 u and +8 u): NOT OBSERVABLE, not fixed by reading. Left over, both low: 3-20's knight 6 px (it stayed when the plank moved:
      CMP-17) and 2-29's hinged plank, which leans 1.3 degrees less than the original's, 3 px at the ends (not traced: R4d1-3).*
- [x] **CMP-3. Equal-z pictures are drawn in scene-file order; the original orders them by 128-unit bucket** (high on 4-31, medium on the rest; structural; `decoded`;
      moves no route). Levels: 1-07, 1-11, 1-12, 1-20, 2-14, 2-30, 4-11, 4-16, 4-31. Evidence: 4-31 box [810,170,985,285] (`pairs/4-31_h63_pair.png` and `_overlay.png`:
      the arm's left half and pivot are hidden behind the anchor block in the port); 1-07 box (545,270)-(1080,715); 1-20 box 384,452,470,497 (the pillar drawn over the
      cracked stone's first tile); 2-14 box 103,367,193,547; 2-30 box 0,380,482,720; 4-11 box 1045,395,1110,600 (the port's disc covers the door's left frame); 4-16 box
      1095,445,1190,495; and 1-11 box 250,200,470,340 and 1-12 box 430,305,590,470 (the rings). Cause: Ethanon keys one multimap by the draw hash, equal depths tie, and
      a tie falls to insertion order = visible buckets row by row (`ETHBucketManager.cpp:81-91`), then the bucket's list (`ETHScene.cpp:541-576`,
      `ETHEntityRenderingManager.cpp:119-131`); the bucket is 128 level units (`Game::start`, `SGlobalScale::scaleEntities`: floor(x x 2.8125 / 360) = floor(x / 128));
      the later bucket draws on top. It predicts all 9 static orders on the 7 levels that show one (1-07, 1-20, 2-14, 2-30, 4-11, 4-16, 4-31) and none is contradicted;
      1-11 and 1-12 fit only if the ring is the agent's own particle at z 0, so the readers' attribution to a z -4 antiportal or a `shock_area` is probably wrong there
      (the step must confirm which entity the ring is). The port orders by `z_index`, then file order (`Sprites::Find`, `MagicPortalsLayer.cpp:2302`). Within one bucket
      step 118's note (an entity the scale moved to another bucket goes to the front) applies and is untested. Joins: step 72's deviation (1-07's ring behind art the
      original draws it in front of), Order 11.3's deviation 9. Next: break z ties by (floor(y/128), floor(x/128)) of the node's current position, then file order
      (`Sprites::Find`, `buildSprites`). Gate: the 9 static orders on f420 frames, 1-11's and 1-12's rings as the original's, the other 119 levels reviewed for flips,
      `test_mp_pictures` pin. GAMEPLAY: none.
      *Closed by step 167 (CMP3, wave 4c-1), 2026-10-11; committed as `ca93e40` (wave 4c-1's close; pushed on 2026-10-12 after R4c1-1's ruling). GAMEPLAY: none (the 128 routes win on exactly the census's
      ticks).* Built as the Next says, with the bucket's own order: `Sprites::Rank` deals the slots of each `z_index` by (exact depth, bucket row, bucket column, place in
      the file), the later bucket (the lower row, then the higher column) over the earlier; the bucket is 128 level units and the line tolerance 0.001, both in
      `game/data/art.json` `draw_order` with their citations; the layer deals at build and again a frame late when a picture's bucket changes. Pins: `test_mp_sprites`
      1,571 -> 1,675 and `test_mp_layer` 3,905 -> 3,987 (`test_mp_pictures` holds step 150's interpolation, not which picture is over which, so the Gate's pin sits in the two
      suites that own the API). Measured: the f420 sweep changed 49 levels (79 byte-identical), all 9 of the group among them; the nine boxes, mean abs against h6.3 before
      -> after (the stills' spread): 1-07 4.54 -> 3.20 (3.22), 1-11 1.62 -> 1.31 (5.82), 1-12 16.48 -> 14.34 (35.48), 1-20 57.67 -> 0.77 (0.00), 2-14 2.52 -> 0.23 (4.27),
      2-30 6.51 -> 5.03 (3.45), 4-11 11.09 -> 3.35 (13.93), 4-16 49.75 -> 1.03 (0.29), 4-31 9.74 -> 5.23 (0.00); 57 suites, 32,569 checks, `test_mp_routes` 128 of 128 on the
      census's ticks, reachability 108 / 20 / 0, 0 warnings. Independently verified (`CMP3/verify1.json`: 9 gates ok, 7 low). **What it leaves**: (a) the Gate's "changes only
      the levels the group names" is NOT met as written: 40 levels outside the nine change at f420 too (edge contacts; on their changed pixels 33 move toward the stills,
      5 are flat and 2 slightly away, 1-01 (157 px, at most 16 grey levels) and 2-13), each explained in `out/parity/program/CMP3/explain_changes.md`, the causes of 11 of
      them and of 4-28's piece INFERRED; (b) two of the nine boxes are not to the spread, each another cause: 2-30 (5.03 against 3.45: the doorway flame and the fire diamond's
      glow, phase, INFERRED) and 4-31 (5.23 against 0.00: R3's hinge pose, the arm 9 px low, CMP-2's); 1-20 and 4-16 are at the port's own floor, their stills agreeing;
      (c) the step-118 note's in-bucket rule is NOT built, and the stills contradict it on two decisive pairs (CMP3-1); (d) 1-11's and 1-12's rings are the agents' own
      particle system at z 0 (`onda.png`), not a z -4 antiportal or a `shock_area` as the group suspected; (e) the row-major axis is settled on 2-27, 3-05 and 1-05 beside
      the source, the column-major mutation never run (CMP3-6). The deviations are rows CMP3-1 to CMP3-6 above. Settles the draw order of step 72's deviation (ticked there,
      with its limit) and joins Order 11.3's deviation 9 and G1a-2 without closing either.
      *Re-taken 2026-10-15 (wave 4d-1): FIXED in frame on 8 of 9, by step 167 (`ca93e40`): 1-07 the no-portal disc tints the blocks, 1-11 the left agent's ring is cut by
      the two-stone platform, 1-12 the cyan ring is drawn over the blocks above it, 1-20 the pillar over the cracked wall (mean_abs 1.63 -> 1.38), 2-14 the tall block
      stands behind the slab, 4-11 the door in front of the disc (minor -> match), 4-16 the stone pair behind the 2x2 block, 4-31 the arm over the ledge with its whole
      bar and pivot (box mean abs 0.42). 2-30 has no reader line on it either way (not measured). No level shows a draw-order residual; 1-21's red disc over a plank (R +4
      to +8, byte-identical to the old frame) is the one unexplained neighbour and stays in CMP-low (g).*
- [ ] **CMP-4. Piles that stand at rest in the original fall or never settle in the port** (high on 1-30, medium on 2-13 and 2-05, low on 3-12; structural; `needs-rig`;
      MOVES ROUTES). Evidence: 1-30 box 360,180,1000,660 (`pairs/1-30_h63_pair.png`: the crate pile at rest in all six original stills, toppled by f75 in the port;
      pile-region mean abs 3-4 between the original's own stills against 12-17); 2-13 box 1100,195,1280,360 (the barrel pile slides about 105 px, 30 px by f75); 2-05
      original right pile [670,455,810,630] against the port's [670,535,880,630] (stacks tilt 1-4 degrees and lose the top crate); 3-12 box 385,105,595,360 (held asleep
      by `resting.json`, low). Cause as recorded: the engine's eight solver iterations cannot hold a heavy body on lighter ones and awake piles never sleep (G10-14,
      WT-7); only 3-12's pile is put to sleep. The stills answer R3b-5's "compare those levels' stills" for 1-30, 2-13 and 2-05 (at rest from the first frame in every
      still); they cannot say asleep from merely stable, and that decides what a player's arrival does (1-30's route enters the pile: R4a-5, the portal exit (202.9,
      183.9) lies inside `crate_small`'s column and the solver shoves him 8.7 px on tick 74). Next: a clip of the original with the player arriving at 1-30's pile; then
      `game/data/resting.json` entries for the piles with the stills as `_measured` (as 3-12's), and re-fly 1-30, 2-13 and 2-05; or the engine's friction step (CMP-11,
      owner's call). Gate: 1-30's pile region from mean abs 12-17 to the original's 3-4, `test_mp_resting` pins, `test_mp_routes` 128 of 128. GAMEPLAY: yes (a pile a
      body meets is asleep until touched).
      *Re-taken 2026-10-15 (wave 4d-1): UNCHANGED. 1-30 still `differs` (the only high of this row) and the same piles on 2-13 and 2-05 (medium) and 3-12 (low); no step
      of waves 4c-1 and 4c-2 touched piles or the solver. 1-30: the pile (a big crate on a plank across two stacks of two small crates) is at rest in all six stills and
      has fallen by f75, toppled by f120-f135, a crate lying at about 45 degrees at f420; its box (360,180)-(1000,660) holds 90 changed pixels against the old frame (a
      seam strip at x 906-962, y 630-635) and is otherwise byte-identical, so the fall is the old fall. 2-13: the barrel pile at the right edge collapses by f135 and the
      upright barrel stands 105 px left of its place at f420. 2-05: the top crate has fallen and the left crate crept 4 px. 3-12: the stone is 4 px right and 5 px high,
      the crate 3 px right and 1 px low. Cause as recorded (G10-14, WT-7), not re-derived. Still needs the rig clip; GAMEPLAY if taken.*
- [ ] **CMP-5. Knight timing the stills contradict: 3-05's knight A is absent for 8 s in the original; 3-22's knight leaves its plank before its 10 s hold ends** (high
      on 3-05, medium on 3-22; structural; `needs-rig`; MOVES ROUTES). Evidence: 3-05 box x1130-1280 y490-640 (`pairs/3-05_h63_pair.png`, lower right: the port's knight
      A (spawn (760,208), wayA1 (622,208) hold 4000, wayA0 (420,208) hold 2000) walks into view at the lift door by f420; none of the original's six stills, 0.3 to 8 s,
      shows it, so a clock offset cannot hide it; the reader put it at about three chances in four that it is real); 3-22 box x880-1100 y360-670 (wayB1's holdTime 10000
      ms from arrival, about f200, cannot end by f420: the port's knight stands on the plank at f240 as the original does and is on the floor by f420, the original's on
      the plank in four stills). Candidate causes, unproven: a bucket-gated spawn (the marker is static = 1 and a static entity's callback runs only in a visible
      bucket: G3-2, though the opening pan sweeps that bucket), the port's minion hold, a frictionless slide down the 19-degree plank (R24/R25, R3c-3). No footage of
      either level exists (`out/parity/footage/INDEX.md`). Next: record 3-05 for 10 s without moving and 3-22 for 14 s (`docs/parity.md` sections 1-2), then fix in
      `sim/Minions` or the spawn gate. Gate: knight A's first frame and 3-22's departure tick against the clip; the stealth routes (3-04, 3-05, 3-11, 3-24: achievements
      26-28, 32, 42) and 3-22's re-flown. GAMEPLAY: yes if real. Joins: G3-2, R24/R25, R3c-3, R3c-8.
      *Re-taken 2026-10-15 (wave 4d-1): UNCHANGED. 3-05 still `differs`: the port's knight A stands beside the lift door at f420 (red-cape centroid x 1232) and f450 (x
      1176) and is in none of the original's six stills, the last of them 8 s after the tap; the box (1130,490)-(1280,640) holds 0 changed pixels against the old frame.
      3-22 (medium): the knight is on the plank at f240 and has stepped onto the floor ledge by f420 and f450 (x 950-1010 against the stills' 905-985), while the original
      has it on the plank in all four post-opening stills. No footage of either level exists. The candidate for 3-05 (the marker is static = 1 and a static entity's
      callback runs only in a visible bucket, G3-2, though the opening pan sweeps that bucket) is still not proven.*
- [x] **CMP-6. A knight the original never spawns: the markers `minion_spawn6` (4-29) and `minion_spawn_` (3-09) have no `ETHCallback`** (high; structural; `decoded`;
      MOVES ROUTES). Evidence: 4-29 box [860,495,1050,632] (`pairs/4-29_h63_pair.png`, lower right: sky, gem and a black knight in the port, empty in the original; six
      original stills and 12 footage frames, `out/parity/footage/L1_out_4-29_sheet.png`, show none); 3-09's lone guard (`minion_spawn_` id 908 at (410,48), wayA, hold
      9999999) is in neither still. Cause: Ethanon runs `ETHCallback_` + the entity's exact name without extension (`ETHASUtil.cpp:63-79`); the listing has
      `ETHCallback_minion_spawn` (bytes 426511..426876) and no `_minion_spawn6` or `_minion_spawn_`, so those markers spawn nothing; `data/entity_roles.json` lists all
      three as `enemy_spawn` and `Minions.cpp:928` spawns from each. The original's scenes: 60 `minion_spawn`, one `minion_spawn6` (level28c id 1766 at (414,208),
      static), one `minion_spawn_`. Next: remove the two names from `enemy_spawn` (or mark them no-spawn with the citation); `test_mp_minions` pins the census (60
      spawning markers); re-fly 4-29 (its route was flown for the knights A and B, B being `minion_spawn6_1766`) and re-check 3-09's. Gate: 4-29 f420 shows no knight at
      the gem, reachability 108 / 20 / 0, `test_mp_routes` 128 of 128. GAMEPLAY: yes (one knight fewer on 4-29, the 3-09 guard gone).
      *Closed by step 171 (CMP6, wave 4c-2), 2026-10-12; committed in wave 4c-2 (the commit titled "Magic Portals: two spawn markers the original never runs spawn no
      knight", `9797699`; pushed with the wave, `7e708e3..45f6c05`, 2026-10-15). GAMEPLAY: YES, and it MOVES A ROUTE: 4-29's
      route no longer wins (its first problem: the tap on tick 525 is off the view), the other 127 routes win on exactly the census's ticks (3-09 still on 838), so
      `test_mp_routes` fails on 4-29 until the route phase (planning step 176) re-flies it.* Built as the Next says, as data and no code: `data/entity_roles.json`
      `enemy_spawn.names` is `["minion_spawn"]`, its `_note` carrying the citations (`ETHASUtil.cpp:63-79`, `ETHScene.cpp:707-708`, `Platform.cpp:90-110`, the listing's
      bytes 426511..426876, md5 `48fe65ce`); no line under `game/` changed and `MagicPortals.exe` is still `29166f75`. The census (derived from the 128 converted scenes and
      the 128 raw `.esc`: 60 `minion_spawn` in 35 levels, one `minion_spawn_`, one `minion_spawn6`) names exactly 3-09 and 4-29, the hint; knights over the 128 levels 62 ->
      60 (3-09 3 -> 2, 4-29 2 -> 1). Measured: the f420 sweep (owner's seed, 128 launches, validation silent) changed 2 levels and left 126 byte-identical: 4-29 (7,779 px;
      the black knight beside the gem is gone: its box's mean absolute difference to the original's three stills 18.0 / 18.5 / 18.4 -> 1.13 / 1.65 / 1.50, pixels darker
      than 30 7,383 -> 380, the original's 380) and 3-09 (80 px, at most 16 grey levels: the lone guard that dropped on tick 24 and died on the killer floor on tick 58 with
      a 19.7 px shake is not made, so the effects' shared random stream differs; achievement 31's count of exactly 2 now stands from tick 1 instead of by accident). Pins:
      `test_mp_minions` 294 -> 448 (the census pin 62 -> 60 and `aMarkerTheScriptHasNoCallbackForSpawnsNothing`), `test_mp_layer` 4,151 -> 4,157; the old table put back
      fails 12 + 1 checks (both names), 9 (`minion_spawn_` alone) and 6 (`minion_spawn6` alone). 57 suites, 56 pass, 33,277 checks (`test_mp_routes` 2,351, failing on 4-29
      only), reachability 108 / 20 / 0, 0 warnings. Independently verified once (`CMP6/verify1.json`: pass). **What it leaves**: (a) **4-29's route**, re-flown by the route
      phase (planning step 176) for knight A only (`minion_spawn_1902`), dropping knight B's tap on tick 336 and the wait it needed; (b) 4-30's id 76, whose two callback
      markers disagree with the script's count of 1 (CMP6-3); (c) achievement 31 on 3-09 (gold, no knight killed) is exercised by no route. The deviations are rows CMP6-1
      to CMP6-9 above.
      *Step 176 (the route phase), 2026-10-15: 4-29 is re-flown (`level28c.json`: the two taps spent on the deleted knight B, ticks 56 and 335, go; four taps and holds are
      centred; 8 shots -> 6 and 6 -> 4 portals at the door; it still wins on tick 1,023, set by the key knight's patrol); 3-09 still wins on 838 and is not edited.*
      *Re-taken 2026-10-15 (wave 4d-1): FIXED on 4-29, by step 171 (`9797699`): the near-black pixel count in the sky region (760-1060 x 380-625) is 874 at f75, f135,
      f240, f420 and f450 against 953 in the three stills, so no figure stands where the black knight stood. 3-09 is NOT OBSERVABLE: its guard was never visible in either
      still, and the level reads `match`.*
- [x] **CMP-7. The red key's own light is not built** (high; light; `decoded`; moves no route). Levels: 4-20 (seen), 4-19 (holds the other red key; reads `match`, its
      light not separated from `red_key_state`'s halo). Evidence: 4-20 box 760,255,1040,465 (`pairs/4-20_h63_pair.png`: RGB 57,19,22 in the original against 26,25,35 in
      the port; the strip beside him 86,0,0 against 0,0,0; the key lies at the spawn and is taken on tick 1). Cause (listing): `ETHCallback_key` #119-186 (bytes
      438490..442683): for a visible key whose `color` is `red`, every frame `AddLight(position + (0,0,6 x scale), (1,0,0), range 48 x scale)`; `game/data/keys.json`
      `_not_built` names it. Joins planning step 69; no backlog row. Next: `sim/Keys` and the layer add the point light while a red key is visible (the torches' own
      `AddLight` path), the numbers in `keys.json` with the citation. Gate: 4-20 f420 box from (26,25,35) to the original's (57,19,22), 4-19 unchanged within noise, the
      lighting suites. GAMEPLAY: none.
      *Closed by step 168 (CMP7, wave 4c-1), 2026-10-12; committed as `7ebe83b` (wave 4c-1's close; pushed on 2026-10-12 after R4c1-1's ruling). GAMEPLAY: none (the 128 routes win on exactly the census's
      ticks). Committed WITH RECORDED DEVIATIONS (owner-delegated): gate 2 is not met to the letter.* Built as the Next says: a key whose `color` datum is `red` adds, on every
      call, a point light (1, 0, 0), range 48, 6 over the key's own z, at the key's place of the PREVIOUS tick, so it follows a carried key and stays while the key's
      picture does (`Keys::Rules::OwnLight` and `HasOwnLight`, `game/data/keys.json` `red_light` with its citations, the layer's `KeyLight`; a script's light, so no owner scale and
      no particle share). **The offset is a height, `(0, 0, 6 x scale)`: planning step 69's "(6 x scale, 0, 0)" and the old `_not_built` note had it as an x offset, and were
      wrong** (the listing pushes the arguments last-first, #168-173; step 69's text is not rewritten). Pins: `test_mp_keys` 169 -> 182, `test_mp_layer` 3,987 -> 4,017.
      Measured: the f420 sweep changed 1 level of 128, 4-20 (24,611 px, the light's reach, red channel only), 127 byte-identical; 4-20's group box (760,255)-(1040,465) R
      17.3 -> 48.4 against the original's 49.4 (mean abs against h6.3 11.24 -> 1.27), the reader's box 15.5 -> 55.9 against 56.9, the strip 0 -> 84.5 against 86.3, the player's
      box 9.2 -> 34.7 against 35.1; 4-19 is byte-identical by geometry (its one lit neighbour, the breakable wall, is off the frame); 57 suites, 32,612 checks, `test_mp_routes`
      128 of 128 on the census's ticks, reachability 108 / 20 / 0, 0 warnings (scratch compiles). Independently verified three times (`CMP7/verify1.json` to `verify3.json`:
      the third read 7 of 8 gates ok, gate 2 not met to the letter). **What it leaves**: (a) the group's "57,19,22 against 26,25,35" is the reader's mean box and the port's
      figure a frame from before steps 166-167; the box the group names reads 49.4 / 21.3 / 24.0 in the original against 17.3 / 21.1 / 23.6 before, G and B already the original's;
      (b) the raw 1 to 2 percent low in every lit box is the same size as the unlit part of the same frame reads (0.979), cause untraced (CMP7-1; the record's wording of it is
      CMP7-2); (c) 4-19's light is shown by no frame (CMP7-4); (d) the key's fly-in is not built, so a spent key's light lasts as long as its held picture (CMP7-5); (e) the two code
      mutations were never run (CMP7-3) and the full build's zero warnings rests on scratch compiles (CMP7-6); (f) 4-20's player (CMP1-5's piece, "1.3 against 4.4: CMP-7") now reads R 34.7
      against the original's 35.1 in its box (855,330)-(945,465), G and B 0.96 of the original's before and after the light (the silhouette's dark pixels and the carried key's
      sliver, not traced); the carried key's resting side of the player (CMP-low g) and the achievement card (a capture condition) stand. The deviations are rows CMP7-1 to
      CMP7-6 above.
      *Re-taken 2026-10-15 (wave 4d-1): FIXED on 4-20, by step 168 (`7ebe83b`): the red area reads 40.0/13.2/14.8 against the original's 40.8/13.3/15.0 (within 2 percent;
      the old pass read the port 26/25/35 against 57/19/22) and the door, dome and ground are painted red; 4-19: key, halo and haze equal. 4-20's one remaining residual
      is the key resting on the other side of the player (its tip at his front edge x 929-936 in the original, a faint rim at his back x 877-881 in the port: CMP-low g).
      The achievement card the old pass read there is a fresh-save capture condition and is dropped.*
- [ ] **CMP-8. Lights and halos read weaker than the original on about 50 levels** (medium, mostly low-to-medium per level; light; `owner-call`; moves no route).
      Levels: 1-01, 1-02, 1-04 to 1-07, 1-09 to 1-12, 1-14 to 1-18, 1-22, 1-28, 1-29, 1-31, 2-10, 2-11, 2-16, 2-17, 2-19, 2-24, 3-01, 3-02, 3-04 to 3-08, 3-10 to 3-12,
      3-14, 3-17 to 3-29, 3-31. Evidence: 1-01 box (700,30)-(900,380) (`pairs/1-01_h63_overlay.png`; torch box R 190 against 221); 1-12 box 560,0,900,330 (R 189-205
      against 218-238); 1-11 box 1040,0,1280,340 (block means at (1200,80) 171,99,51 against 214,119,53); 1-28 box 1100,0,1280,330 (signed grid -7/-12/-19); 3-01 box
      x990-1280 y0-440; 3-25 to 3-27 right edge, cells x1120-1280 y0-270 121/113, 144/132, 69/54 (original / port). Cause: `game/data/lighting.json`
      `halo_brightness_scale` 0.6 is a marked `_guess` (Ethanon has no such factor; its own formula is 1), tuned to one frame's flame (step 49); the flame is 0.7265
      live on average, so 0.6 draws the halo at 0.44 of the formula and 0.69 would match the fit's 0.5. The ratios differ by entity (torches 0.75-0.9, blue lamp 0.5-0.6
      near and 0.8-0.86 far, orange crystals 0.55-0.9, fire agents 0.85-0.9), so one knob may not close them; the port also flickers less (13 against 32 grey levels on
      1-01). Constant across the six stills, so not a pulse. Joins: G5's missed gates (step 49), "Found by 11.2 (step 110)", "Halo magnitude". Next, the owner's call:
      (a) one global scale fitted on the 128 x 6 stills this comparison holds, (b) per-entity scales (torch, `blue_light`, `diamond_light_*`, fire agent), or (c) leave.
      Gate if taken: the box ratios to 1.0 +-0.05 on a held-out half of the levels, G5's `LM_E<1_halo` rows still pass at hd art, the flicker amplitude re-read.
      GAMEPLAY: none.
      *Re-taken 2026-10-15 (wave 4d-1): UNCHANGED, and the cause stands: `game/data/lighting.json` `halo_brightness_scale` 0.6 is still a `_guess` (steps 166-175 touched
      the file only for `script_emissive`). The level set grew from 50 to 57 and lost none (seven added by a sharper reading: corner slivers on 1-03, 1-30, 1-32 and 3-16,
      fire-agent halos on 2-18 and 2-30, 1-08's lamp wash; 1-10 and 3-25 raised from low to medium), which is what a cause no step touched should do. 37 of the 57 carry
      it at medium; it is the only cause of 30 of the 65 `minor` levels. It is present on every level that carries the entity, graded medium or low by magnitude, so a fix
      is not scoped to the medium levels. New data for the owner's call: the shortfall differs by entity (torch wall luminance 0.81-0.97 of the original's, orange-crystal
      wash R 0.77-0.89, blue lamp 0.62-0.87, fire-agent halos 9-20 percent weaker at 32-64 px and equal beyond 80 px), which favours option (b), per-entity scales; the
      original's concentric light bands around the flame are absent in the port's torch halos on 1-01, 1-02, 1-04, 1-06, 1-07 and 1-18. Still the owner's call; CMP-12
      follows it, and CMP-19 joins it only if the scale is the knob.*
- [x] **CMP-9. Script emissive blinks not built: barrels, the breakable wall's `blink` flag and teleportable crates** (medium; light; `decoded`; moves no route). 32
      levels in scope: 1-09, 1-13, 1-19, 1-22, 1-24, 1-25, 2-04, 2-05, 2-08, 2-12 to 2-19, 2-21 to 2-25, 2-28, 2-29, 2-32, 3-14, 3-15, 3-19, 4-10, 4-16, 4-25, 4-32 (the
      readers filed it on 17; wave 4c-1's fix1 adds the fire agent's 10 outside them, which the cause text below names (the pulse): 2-10, 2-11, 2-20, 2-26, 2-27, 2-30, 2-31, 3-23, 4-12 and 4-28, 42 levels in all). Evidence: 2-12 barrel box 778,520,866,630 (R 15.9-23.8 over the stills against a constant 10.5); 2-14 wall box 800,358,890,508 (72-88
      against 50) and barrel box 205,395,292,508; 3-14 wall box 560,410,915,495 (78-101 against 66); 2-05 crates [710,156,890,330] (the original shows their grain, the
      port's are dark); 4-25 crate [678,92,770,182] (textured in the original, black in the port); the walls' pulse 1.18-1.53x. Cause (listing):
      `ETHCallback_barrel_bomb` #1-30 `blinkEmissive(thisEntity, 0.2, 300 ms, minEmissive)` every frame (16 barrel levels; the readers called it "unknown, not decoded"
      on 2-17, 2-21, 2-23 and 2-24); `ETHCallback_breakable_wall` #88-151, for a wall with the uint `blink`, `blinkColor` and `blinkEmissive` 0.8 <-> 1 on 300 ms legs
      (8 levels: 1-09, 1-13, 1-19, 1-22, 1-24, 1-25, 2-14, 3-14); `crateCallback` #38-121, a teleportable `_no_emissive` crate's emissive 0.2 -> 0 over 600 ms (14
      levels; `art.json` `non_teleportable._not_built`); also decoded, not built: the fire agent's `bounce(1.02 -> 0.98, 300 ms)`. The blink triangle already exists
      (`Art.hpp`, built for the non-teleportable crate and the antiportal). Joins: "Found by step 120, decoded, not built", "The restock's pictures" (the barrel's
      blink); the wall's blink is in no row. Next: three more callers of the existing blink in `sim/Art` and the layer (data in `art.json` with the listing citations)
      and the fire agent's pulse. Gate: the readers' boxes inside the original's range at the right phase, pins in `test_mp_sprites`, the f420 sweep changes only the 32
      levels (corrected by wave 4c-1's fix1 to the 32 and the fire agent's 10, 42). GAMEPLAY: none.
      *Closed by step 169 (CMP9, wave 4c-1), 2026-10-12; committed as `4216f32` (and two text-only commits, `1379eca` and `753e7d0`, which correct its record) (wave 4c-1's close; pushed on 2026-10-12 after R4c1-1's ruling). GAMEPLAY: none (the 128 routes win on exactly the census's
      ticks, 83 to 3,343). Committed WITH RECORDED DEVIATIONS (a delegated disposition): gate 1 as first written is not met; its text is corrected to 42 levels (fix1).* Built as the Next says, from the listing (md5 `48fe65ce`),
      every number in `game/data/art.json` with its citations: a barrel's emissive from its `minEmissive` datum to 0.2 on 300 ms legs, every frame, over the node's own
      (`ETHCallback_barrel_bomb` #1-30; `GetVector3` of an absent datum is (0, 0, 0), so 15 of the 16 barrel levels blink 0 -> 0.2, chapter 2's barrels get brighter and 3-19's,
      4-10's and 4-16's, which carried 0.45, get darker; 4-32's one barrel carries (1, 1, 1), as does every barrel the dark dragon restocks); a breakable wall with the uint
      `blink`, colour and emissive 1 -> 0.8 on 300 ms legs, **both calls adding the frame to ONE clock, so it runs twice as fast: a period of 300 ms, the colour one add
      behind** (`ETHCallback_breakable_wall` #87-151; the group's row did not state it); a teleportable `_no_emissive` crate's emissive 0.2 -> 0 on 600 ms legs (`crateCallback`
      #70-93); and the fire agent's pulse, its scale 1.02 -> 0.98 on 300 ms legs, which draws its picture, its light's range and offset and its halo's size, not its flames
      (`ETHCallback_fire_agent` #282-300). Pins: `test_mp_sprites` 1,675 -> 1,763, `test_mp_layer` 4,017 -> 4,090; 13 `art.json` mutations and four code mutations each fail
      named checks. Measured: the f420 sweep changed 40 levels of 128 (88 byte-identical): 30 of the group's 32 and the fire agent's 10 outside it; the readers' boxes 77 of 93
      stills inside the port's f420-f504 range once the camera offsets are aligned (73 unaligned), 66 of 69 where the box holds only the blinking thing: 2-12's barrel 10.5 ->
      16.3 against the stills' 15.9-23.8, 2-14's wall 50.0 -> 76.7 against 71.8-87.8, 3-14's walls 66.0 -> 89.6 against 78.1-100.9 (the 78.1, h0.3, reads 99.4 once aligned),
      3-19's barrel, which was too bright, 50.0 -> 27.8 against 26.5-32.6, 4-25's crate no longer a black square and 2-05's crates showing their grain; the periods fitted on
      the original's footage, 0.605 s for the barrel and 1.210 s for the crate, against the decode's 0.600 and 1.200; 57 suites, 32,773 checks, `test_mp_routes` 2,366 checks
      with 128 of 128 on the census's ticks, reachability 108 / 20 / 0, 0 warnings. Independently verified three times (`CMP9/verify1.json` to `verify3.json`: the third read 8
      of 9 gates ok, gate 1's letter not met). **What it leaves**: (a) gate 1's scope of 42 levels (accepted by the commit's delegated disposition, NOT ruled by the owner, and the wave's third verification still fails gate 4 as worded on
      4-12: R4c1-1) and the unchanged 2-32 and 3-15 (CMP9-1); (b) the stills never
      sample the port's troughs on 4-25, 4-32 and 2-21 and five stills lie outside for stated causes (CMP9-2); (c) the flagged wall's period and the static entities' phase are
      decode only (CMP9-3), and the chapter-1 walls read dimmer than all six stills at f420 on the changed pixels, 1-22, 1-13 and 2-19 slightly worse against h6.3 alone
      (CMP9-6); (d) 4-19's two flagged crates, the 200 ms branch, are not built (CMP9-5); (e) the dead initialisers and the older executable (CMP9-7). The deviations are rows
      CMP9-1 to CMP9-7 above; the older rows that name the barrel's blink, the crate's emissive and the pulse as unbuilt ("Found by step 120", "The restock's pictures") carry a
      note.
      *Re-taken 2026-10-15 (wave 4d-1): FIXED in kind, left `[x]`; the re-take reads the group as PARTLY, because low residuals stay (R4d1-2). Every case a reader
      measured lies inside the original's range: barrels on 2-12 (original R 15-23, port 10-21), 2-14, 2-15, 2-21 (steady 20.4 against 20.7), 2-23, 2-29, 3-19, 4-10,
      4-16, 4-32; breakable walls on 1-09 (pillar luma 75.5-86.7 against 76.0-86.2), 1-19, 1-22, 2-14 (74-89 against 71-87), 3-14 (75.1-86.0 against 74.4-84.3); crates on
      2-01, 2-05, 2-06, 2-08, 2-25, 2-28, 4-12, 4-25. Left, all low and all known: (a) CMP9-6: at f420 the chapter-1 walls sit at the dim end of the blink (1-13 68.0
      against the stills' 71.5-84.5, 1-22 0.92 of the original, 1-24 8-9 percent, 1-25 113/101/84/126 against 114-124/100-109/84-92/127-138); (b) the static crates'
      bucket clock (2-11's crate is static = 1 and reads the saturated red wash at four of five port ticks, R 77/58/57/75 against the stills' 31-45; 2-10's two crates
      pulse 55-91 against a steady 75-87); (c) 1-31's non-teleportable crate, deep red in all six stills (G 22-32) and cycling in the port; (d) amplitudes the stills
      cannot settle (2-17's and 2-24's barrels dip at f420, 4-16's barrels 4 percent darker, 4-28's orange mine 9 percent brighter in the original). Two clips settle (a)
      and (b): 1-09 or 2-14 for the wall's period and depth, and G3-2's bucket clock for the static crates. Drawing only.*
- [x] **CMP-10. The help block ("?") is drawn plain and opaque; the original draws it at alpha 0.76 breathing 1.05-1.13x** (medium; structural; `decoded`; moves no
      route). 13 levels in scope: 1-02, 1-03, 1-10, 1-11, 1-13, 1-19, 1-25, 1-32, 2-03, 2-06, 3-09, 4-01, 4-02; in frame and filed on 1-02, 1-10, 1-11, 1-13, 1-25; in
      frame and unfiled on 2-03 and 2-06 (their pair images show the same greyer face, about 16 percent dimmer over the dark sky: the readers missed a decoded
      difference). Evidence: 1-10 box 520,386,605,470 (`pairs/1-10_h63_pair.png`: face 171-173 against the port's 213, width 72-76 against 68 px; the same on 1-11 box
      490,120,580,215 and 1-13 box 85,236,165,318); 1-25 box 785,280,875,370; 1-02 box (1152,184)-(1226,261). Cause (listing): `ETHCallback_help_block` (bytes
      371476..371655) `SetAlpha(0.76)` and `bounce(thisEntity, 1.13, 1.05, 250 ms)` every frame (`utilEntityEffect` bounce, bytes 327057..327527); `helpBlockInput`
      (`Popup.cpp`) only reads the touch. Alpha 0.76 over the dark wall predicts the measured 171-173. No backlog row names it. Next: draw alpha 0.76 and the smoothEnd
      triangle 1.13 -> 1.05 over 250 ms on `help_block.ent` in the layer (numbers in `art.json` or `ui.json` with the citation; the tap rect unchanged, it uses
      `box_scale`). Gate: the face reads 171-173 and 72-76 px on 1-10, 1-11, 1-13; a `test_mp_sprites` pin. GAMEPLAY: none.
      *Closed by step 170 (CMP10, wave 4c-1), 2026-10-12; committed as `5c1949b` (wave 4c-1's close; pushed on 2026-10-12 after R4c1-1's ruling). GAMEPLAY: none (the 128 routes win on exactly the
      census's ticks, 83 to 3,343). Committed WITH RECORDED DEVIATIONS (owner-delegated): gate 2's face tolerance is not met to the letter.* Built as the Next says, from
      the listing (md5 `48fe65ce`), every number in `game/data/art.json` `help_block` with its citations: the block is drawn at alpha 0.76 (`ETHCallback_help_block` #1-3
      `SetAlpha(0.76f)`) and breathes 1.13 -> 1.05 -> 1.13 on 250 ms legs, a period of 500 ms (#5-23, `bounce(thisEntity, 1.13, 1.05, 250)`: a call pushes its last argument
      first, and the group's text and the spec had the push order, CMP10-3), on all 13 levels; its tap rect is unchanged (`box_scale`; pinned fixed over 80 ticks, a touch
      at its corner opens the popup). This row's text is corrected: the `(150)` of 1-25's box is dropped (not reproducible, CMP10-2) and the call and the triangle are
      written in the argument order. Pins: `test_mp_sprites` 1,763 -> 2,061, `test_mp_layer` 4,090 -> 4,151; three code rules mutated in a hooked runner and five `art.json`
      values mutated, each failing the checks that name it. Measured: the f420 sweep (owner's seed, 128 launches, validation silent) changed 9 levels and left 119
      byte-identical: 1-02, 1-03, 1-10, 1-11, 1-13, 1-19, 1-25, 2-03 and 2-06, all in the group; 1-32, 3-09, 4-01 and 4-02 are unchanged because their block is off the f420
      frame (the 13-level layer pin reaches them). The widths are met: 71-77 px over a period on the eight levels in frame against the stills' 71-75 and the old block's 68;
      the opaque near-white square (face about 213, 40 grey off) is gone and the face reads 1.1 to 3.0 grey under the stills'. 57 suites, 33,132 checks (32,773 before),
      `test_mp_routes` 2,366 checks with 128 of 128 on the census's ticks, reachability 108 / 20 / 0, 0 warnings. Independently verified three times (`CMP10/verify1.json`
      to `verify3.json`: the third read gates 1 and 3 to 9 ok and gate 2 not met on the face). **What it leaves**: (a) the face gap, cause open and on the original's side
      where fitted, one calibration experiment (CMP10-1); (b) the block's phase and the popup's hold (CMP10-4); (c) four blocks on no comparison frame (CMP10-5). The
      deviations are rows CMP10-1 to CMP10-7 above.
      *Re-taken 2026-10-15 (wave 4d-1): FIXED on the 8 levels where a reader measured or read the block, by step 170 (`5c1949b`): 1-02 73 x 76 px at grey 181.6 against 75
      x 78 at 182.2 (old 68 x 70, 213); 1-10 face 166.6 against 164.5-167.6; 1-11 71-73 px; 1-13; 1-25 172-175 against 172-205 and 70-72 px against 72-76; 2-03
      155/157/161 against 154/157/159; 2-06 168/172/174 against 169/173/175. The only residual is the known CMP10-1 (the face 1.1 to 7 grey under the stills; low). 1-03,
      1-32, 3-09, 4-01 and 4-02 are NOT OBSERVABLE (no reader line on the block); 1-19's reader reads it as matching.*
- [ ] **CMP-11. Crates and barrels that creep or rest elsewhere than the original's** (medium; structural; `owner-call`; MOVES ROUTES if taken). Levels: 2-20 (box
      880,195,1000,295: the small crate on the big one slides and tilts, (-5,+5) px at f75, (-14,+6) at f450), 2-24 (box 625,45,725,175: the top barrel 6 px low and
      creeping), 2-22 (box 945,100,1250,290: a crate rotated 2 degrees), 3-03 (box x1072-1280 y448-640, `pairs/3-03_h63_overlay.png`: the crate rests 11 px (4 u) off
      the 2x2 block), 3-05 (box x820-995 y190-360: 3 px). Cause as recorded: friction is decoded and held at 1 (G10-14: the engine's friction solve walks a resting
      box), restitution decoded and held (WT-1), `crate_low_emissive` held at friction 0.08, a guess, with no restitution applied (R3b-3); WT-7 names 2-20's creep. The
      comparison adds 2-24 and 2-22 and the original's stills as the reference. Next, the owner's call: queue the engine's friction and restitution steps with the
      Penumbra port's engine work, or not. Until then nothing here changes: R3b-3's 0.08 and 2-30's creep (R3-2) stand. GAMEPLAY: yes, every route that pushes a crate
      (3-03's does).
      *Re-taken 2026-10-15 (wave 4d-1): UNCHANGED, the same five levels with the same numbers: 2-20 the small crate on the big one slid (-12,+5) px at f420 and tilted
      (old pass: (-5,+5) at f75, (-10,+6) at f240, (-14,+6) at f450); 2-24 the top barrel (6,+5) px and tilted 1.5 degrees; 2-22 the crate on the tilted block rotated
      about 2 degrees and (-3,+2) px; 3-03 the crate rests 11 px (4 u) further right and 2 px low, the same at h2.8 / f240, h6.3 / f420 and f450 (a rest, not a phase);
      3-05's crate 3 px right and 2 low. 3-03 and 3-05 are the `crate_low_emissive` crates (R3b-3, friction 0.08, a guess). Still the owner's call.*
- [ ] **CMP-12. The blue lamp's orb is smaller and bluer than the original's, its bracket dark** (medium; particles; `unknown`; moves no route). Levels: 1-05 (box
      (770,70)-(860,190), window mean R 204.0 against 216.1), 1-08 (box (120,40)-(400,240), 199.3 against 212.6), 1-19 (box 420,0,700,200), 1-22 (box 590,40,730,140,
      core R 233 in the original against 211). Known and open (steps 91-92: not the depth; size and brightness left). The original's orb is a white core with concentric
      rings reaching the bowl's lip; the port's is smaller and bluer with the rim showing through its top arc; the wall it lights is 15-50 percent darker (shared with
      CMP-8). Next: settle CMP-8 first (the same pixels), then the orb's draw (added colour, `hard_halo.png` tier) against the five `blue_light` levels' stills. Gate:
      orb window R within 3 of the stills. GAMEPLAY: none.
      *Re-taken 2026-10-15 (wave 4d-1): UNCHANGED, the same four levels (their boxes hold 0 changed pixels except 1-19's 1,612 of 60,000, the wall blink). Original
      against port: 1-05 white core 1,826 px (59 x 54) against 1,185 (46 x 44), bracket 123/201/251 against 67/105/152; 1-08 1,918 (61 x 54) against 1,139 (40 x 43),
      bracket 147/217/254 against 97/140/185; 1-19 the original's white orb with ring structure at the top edge is absent in the port at every tick, with a 20-40 percent
      darker wash; 1-22 core R 223 against 204. Known and open (step 91 found the orb behind the bowl's rim, step 92 measured it is not the depth). Shares pixels with
      CMP-8: settle that first.*
- [ ] **CMP-13. Static portals read differently: the port's rings cover the original's grey spoked iris** (medium on 1-01 and 1-02, low on 1-15 and 3-03; particles;
      `needs-rig`; moves no route). Evidence: 1-01 portals (350,140)-(500,280), (140,385)-(290,550), (880,470)-(1010,610) (`pairs/1-01_h63_pair.png`); 1-02 box
      (360,380)-(510,510); 1-15 box 820,40,1100,620; 3-03 box x0-112 y336-448. Cause (known: step 59's two recorded particle deviations, R22): the ring particles are
      brighter over their life than the original's (1.62x on 1-01's 582, its pulsing part 3.02x; the original's pulse s.d. peaks at 49-57 px, the port's rings alone at
      23-33); tint, 0.8 scale, inline systems and the `AM_ADD` blend are ruled out; the original cycles whitish-pink / red / grey-ring-with-pink / pink across its six
      stills and the port never shows the grey iris phase in five frames; the lower-left portal reads larger and bluer (150 px against 105). Next: the rings'
      life-versus-brightness curve against the 58 footage frames (`out/parity/visuals/particles/verify3/measure_v3.txt`), then the particles in `effects.json`; R22 is
      the gate's re-baseline choice. Gate: ring excess r 25-45 px on 1-01's 582 inside 65.6 +-25 percent. GAMEPLAY: none.
      *Re-taken 2026-10-15 (wave 4d-1): UNCHANGED (no step touched the rings), and 1-15 is RAISED from low to medium on six stills against six frames with no overlap: the
      original's iris is a neutral grey-white disc (RGB 143-159 / 130-160 / 165-181, luma 138-162), the port's a pink-magenta one (182-206 / 151-182 / 214-239, luma
      167-194). Its portal box (880,500)-(1000,620) holds 0 changed pixels against the old frame and the pair image shows the colour difference plainly, so the old
      `match` of 1-15 was an under-grade (R4d1-1). 1-01: the ring reads saturation 0.03-0.48 in the original (under 0.2 in 12 of 18 readings, channels capped at 165) and
      0.25-0.47 in the port (never under 0.24, channels to 253, about 1.5x brighter); 1-02's lower-left portal bluer in the port (127/105/179 against 131/108/160); 3-03 a
      thicker cyan ring and a dark spoked iris against a thinner, brighter ring (low). Cause as recorded: the ring particles are brighter over their life (1.62x on 1-01's
      582); tint, 0.8 scale, inline systems and the AM_ADD blend are ruled out.*
- [ ] **CMP-14. Lit stone reads softer than the original on lightmapped levels** (medium; light; `unknown`; moves no route). Levels judged: 2-17 (box 100,630,1180,715),
      2-18 (box 180,630,1080,715), 2-19 (box 910,95,985,260); 67 levels carry `eth_lightmap`, 2-16 has one and was not tested. Evidence: the original shows crisp
      relief, cracks and bright grit on floors and cracked pillars, the port smoothed stones; the Laplacian s.d. of the cracked `breakable_wall` is 17.2-17.8 against
      12.4-12.6 on 2-17 to 2-19, but 11.9 against 11.8 on 2-21 and 2-24, which have no lightmap. Cause: unexplained; untested candidates are a lightmap (a quarter of
      the hd art per axis) stretched and filtered where the original keeps detail, or a weaker normal-mapped relief. Not in the backlog. Next: render 2-17 with the
      lightmap off and at full resolution and read the Laplacian against `h6.3`; if it explains the gap, widen to the other 64 lightmapped levels. Gate: the wall's
      Laplacian s.d. 12.4 -> 17.2 on 2-17, no change on the unlightmapped levels. GAMEPLAY: none.
      *Re-taken 2026-10-15 (wave 4d-1): UNCHANGED in the build, WIDER in the evidence. The old pass judged 3 levels (2-17 to 2-19); this one reads it on all 10
      lightmapped levels of chapter 2 (2-10, 2-11, 2-16, 2-17, 2-18, 2-19, 2-26, 2-27, 2-30, 2-31; the first reading of chunk 8 had missed it on four of them) and as a
      softer lit rim on 3-17 and 3-19. The chapter's unlit levels read equal in the same measure (2-24, lit by a fire agent and carrying no lightmap: cracked pillar 8.30
      against 8.24; 2-25 floor 1.80 against 1.78; 2-29 pillar 6.55 against 6.49), and the high-pass spread of lit stone is 0.5-0.64 of the original's with the mean
      luminance equal (2-10 ledge 4.24 against 7.66, 2-11 floor 4.89 against 9.72, 2-30 floor 2.06 against 3.33 at luminance 23.0 against 22.9): it is the grain, not a
      dimmer light. A screening census (the median over 32 x 32 blocks of a contrast ratio, h6.3 against f420; it is consistent with the lightmap path and does not
      establish it): the 67 lightmapped levels read 0.977 against 0.995 on the 60 unlit; chapter 1 separates cleanly (24 lightmapped, 0.974, against 8 unlit, 0.998),
      chapter 3 has no unlit control and chapter 4 one. The cause is NOT isolated, no step touched the lightmap path, and the experiment stands as written (2-17 with the
      lightmap off and at full resolution); the other 57 lightmapped levels need their own boxes read before any gate is set on them. Present on every lightmapped level,
      medium or low by magnitude.*
- [x] **CMP-15. A rider drifts off a bobbing slab: the astronaut on 4-02's bounce slab floats away at the slab's peak speed** (medium; structural; `decoded` but NOT
      RUN; MOVES A ROUTE). Evidence: 4-02 box 350,330,450,520 (`pairs/4-02_h63_pair.png` and `_overlay.png`: the astronaut doubled, the port's about 17 px higher): the
      original's eye stays at y 380 +-0.5 in all six stills while the slab bobs 475-481 under him; the port's drifts up 3.2 px/s (383.1 at f75, 364.3 at f420) = the
      slab's peak speed (1 u x 1.2 rad/s = 3.4 px/s). Cause (code reading and the speed match, not run): the slab is `static` 1, density 0 in `level1c.esc` and
      `ETHCallback_bounce` calls `linearMotion` (bytes 353694..353862), so the original's `SetPosition` is `b2Body::SetTransform` with no velocity (G3-1);
      `Bounce.cpp:119-122` calls `Mover::MoveKinematic`, which sets the velocity the contact solve reads, so a zero-gravity rider keeps it; `Mover::PlaceKinematic` (no
      velocity) exists. 4-07's slabs carry no rider and 4-17 shows no drift. Next: `PlaceKinematic` in place of `MoveKinematic` (G3-4's throwaway build), read the rider
      on 4-02; if the engine's push-out does not hold him, record an engine gap with the number (not this step). Gate: the astronaut's eye y within +-1 px of 380
      through f450, a `test_mp_bounce` pin, the 4-02 and 4-17 routes re-run. GAMEPLAY: yes (an idle astronaut stays on his block). Joins: G3-1, G3-4.
      *Closed by step 173 (CMP15, wave 4c-2), 2026-10-12, WITH RECORDED DEVIATIONS (owner-delegated); committed in wave 4c-2 (the commit titled "Magic Portals: a bounce slab is
      placed, not moved with a velocity", `12ac5d5`; pushed with the wave, `7e708e3..45f6c05`, 2026-10-15). GAMEPLAY: YES, and it
      MOVES A ROUTE: 4-07's route no longer wins (not won by tick 470; census 446), so `test_mp_routes` fails on THREE routes, 4-07 (this step), 2-01 (step 172) and 4-29 (step 171),
      until the route phase (planning step 176) re-flies them; the other 125 win on exactly the census's ticks, 4-02 on 87 and 4-17 on 163 among them. The gate is MET FROM f150:
      the eye is within +-1 px of 380 at four of the five sampled frames and every frame f150-f450 reads 380.31-381.08 (two 0.02-0.08 px over the band's edge: reader noise on a pulsing glow); f75-f135 stay open under G3-5 and CMP-low (a) (CMP15-1).* Built as the
      Next says, port-side and no data: `Bounce::State::Tick` calls `Mover::PlaceKinematic` where it called `Mover::MoveKinematic`, the decoded rule (`ETHCallback_bounce`,
      bytes 353694..353862, `linearMotion`'s `SetPosition` on a `static` 1, density 0 body = `b2Body::SetTransform` + `SetAwake(true)`, `ETHPhysicsEntityController.cpp:98-107`);
      the body stays kinematic (`Roles::Moves(bouncer)`), so the engine still shoves what the slab rises into and carries nothing. No tunable moved (`bounce.json`'s notes
      cite the decode). The census (derived from the converted scenes, the raw `.esc` and the listing; the hint was exact): 4 slabs in 3 levels, 4-02 (`level1c`, one),
      4-07 (`level6c`, two) and 4-17 (`level16c`, one). **It corrects this row's "4-07's slabs carry no rider"**: idle only 4-02 has a rider (within 3 px on 450 of 450 ticks),
      but in 4-07's ROUTE the player and the crate rest on the top of the slab at (128, 208) from tick 44 (the player also at the other slab's face), and in 4-17's the
      recoils press the player into the slab's face (a side contact). Measured: the slab's rigid velocity (1.2 px/s at its peak, 60 px/s on tick 1) is zero on every tick;
      4-02's astronaut was 7.501 px up from tick 75 to 450 at vy -1.2002 px/s and is 0.850 px up in all (the slab's first rise into him) at vy 0; his eye on screen 383.10 /
      379.11 / 370.96 / 364.48 / 362.96 -> 383.10 / 380.75 / 380.81 / 380.42 / 380.85 at f75 / f150 / f300 / f420 / f450 against the original's six stills (379.75-380.60). The f420
      sweep (owner's seed, 128 launches, validation silent) changes ONE level, 4-02 (10,321 px, the astronaut 16 px lower, on his block) and leaves 127 byte-identical, 4-07 and
      4-17 among them (no body is at a slab at f420); box 350,330,450,520 against h6.3 mean abs 28.76 -> 8.53, whole frame h6.3 mean_abs 7.17 and edge_iou 0.6795; the doubled
      astronaut of the 50/50 overlay is gone. Pins: `test_mp_bounce` 189 -> 203 (the new pins on the COMMITTED `Bounce.cpp` fail 9 of 203; green 203 / 0). 57 suites, 56 pass,
      33,346 checks (`test_mp_routes` 2,321, 6 failures), reachability 108 / 20 / 0, 0 warnings. Independently verified in three rounds (`CMP15/verify3.json`: not a pass as
      worded, its two mediums being gate 3 at f75-f135 and gate 6's route clause, recorded as deviations 1 and 2; no gate's text was rewritten). **What it leaves**: (a) the eye at
      f75-f135, 1.1-3.1 px out of the band, the same start offset before and after (CMP15-1; G3-5); (b) the three routes (CMP15-2), 4-07's header text's positions being the CAP
      era's; (c) the kinematic rule's mutation, run by nobody (CMP15-3); (d) the same velocity-writing call for platforms, lifts and lift doors (CMP15-5; G3-4). The deviations are
      rows CMP15-1 to CMP15-6 above the wave 4c-1 rows.
      *Step 176 (the route phase), 2026-10-15: 4-07 is re-flown (`level6c.json`: second lift tap 282 -> 279, the left-edge shot (5, 60) -> (5, 80), the last tap (155, 125) ->
      (148, 98): the slab no longer carries him, so he stands 1 px lower; it wins on tick 446 again, the census tick); 4-02 (87) and 4-17 (163) were never moved.*
      *Re-taken 2026-10-15 (wave 4d-1): FIXED at f420, by step 173 (`12ac5d5`): the astronaut's eye is at y 380.3 (380.8 at f240 and f450) against the original's
      379.7-380.5 over all six stills (old: 17 px above the slab at f420, drifting 3.2 px/s). Left: f75 reads 383.1 (CMP15-1, known: the spawn rest, one stride low until
      the slab's first rise) and the slab sits 2 px low at f420 (bob phase).*
- [x] **CMP-16. 2-01's crate is drawn 33 percent too big and, in the original, is smaller and lighter** (medium; structural; `decoded`; MOVES A ROUTE). Evidence: 2-01
      original crate [443,57,577,187] against the port's [420,15,598,190] (`pairs/2-01_h63_pair.png` and `_overlay.png`: a crate inside a larger crate; the same at
      `h0.3` and `h2.8`): the original's is 134 x 130 px, the port's 178 x 175. Cause: `level0a.esc` id 796 (`crate_no_emissive`) carries the custom data `scale` 0.75,
      the only custom scale on a non-agent node in the 128 levels; `SGlobalScale::scaleEntity` multiplies it through `ETHEntity::Scale(float)`
      (`ETHEntity.cpp:591-599`), which rebuilds the body from the scaled box (`ETHPhysicsEntityController.cpp:186-193`), so the original's body is 0.75 x 58 = 43.5 u
      and the port keeps 58 u. Planning step 71 names it "an unowned scaleEntities gap". The 2-01 route pushes this crate from tick 61 ("five times the player's
      weight"). Next: read `metadata/scale` on a crate into picture, collision box and mass; re-fly 2-01. Gate: the crate 134 x 130 px at f420, a `test_mp_levels` pin,
      `test_mp_routes` 128 of 128. GAMEPLAY: yes (a smaller, lighter crate on 2-01).
      *Closed by step 172 (CMP16, wave 4c-2), 2026-10-12; committed in wave 4c-2 (the commit titled "Magic Portals: a node's own scale scales its picture, its body and
      its mass", `9d643c9`; pushed with the wave, `7e708e3..45f6c05`, 2026-10-15). GAMEPLAY: YES, and it MOVES A ROUTE:
      2-01's route no longer wins (the crate it pushes now weighs 2.91 players and not 5.17, so the player reaches the top floor's end on tick ~175 where the route has
      ~308 and is burned by the fireball spat on tick 283), and 4-29's fails as it did after step 171; the other 126 routes win on exactly the census's ticks, so
      `test_mp_routes` fails on 2-01 and 4-29 until the route phase (planning step 176) re-flies them.* Built as the Next says, port-side and no data:
      `LevelBuilder::ReadCustomScale` (`kScaleDatum`) is the one reader; `BuildNode` scales the box, the circle and the hull (size AND offset) and a material's stated
      original area by it, so the mass goes with the square; `ShapeBoundsPx` is scaled and `buildSprites` puts the datum in `drawn.scale` (the static portal's 0.8 and the
      antiportal manager's 0.5 now multiply it: the same double). The census (derived from the 128 converted scenes, the raw `.esc` and the listing): 72 nodes carry the
      datum in 53 levels, converted = raw: 64 `antiportal` and 7 `antiportal.ent` (no body, honoured since step 71) and ONE other, 2-01's `crate_no_emissive_796`, 0.75,
      the hint exact and the only node with a body. 2-01's crate: body 58 -> 43.5 units, mass 0.5382 -> 0.3028 (x 0.5625; 5.17 -> 2.91 times the player's 0.1041), picture
      64 -> 48 units (180 -> 135 px). Measured by the verifier on `MagicPortals.exe` `7dd3a7de` (the implementer's three links were refused by Smart App Control; the same
      exe ran when launched again): the f420 sweep (owner's seed, 128 launches, validation silent) changed ONE level and left 127 byte-identical, the 52 other levels that
      carry the datum (the antiportals) among them: 2-01, 27,328 px, the crate 180 -> 135 px wide; the crate visible 135 x 128 px against the original's 135 x 130 (the
      spec's 134 x 130, +-2: met at the edge); `compare.py` ALL-GOLD against the original's stills, h6.3 mean_abs **4.20 -> 3.31** and edge_iou **0.6167 -> 0.6308**, h2.8 **4.26 -> 3.34** and **0.6556 -> 0.6686**
      (**corrected 2026-10-15 by step 176 and this close**: this note, planning step 172's gate 3 and the commit message of `9d643c9` first said 7.04 -> 3.31, 0.4187 -> 0.6308,
      7.14 -> 3.34 and 0.4212 -> 0.6686, which are R4b's frame from before wave 4c-1's five drawing steps: they had already gained about 2.8 of those 3.7 points, so the crate's
      own gain is 0.9 on h6.3; the step's real before is `CMP10/after/sweep`, measured by the wave's verifier; the pushed commit message still says 7.04 and is not amended). Pins: `test_mp_levels` 129 -> 150, `test_mp_timebase` 445 -> 495, `test_mp_layer` 4,157 -> 4,171; the rules put back fail 27 + 6 + 2
      checks (the box and the geometry-derived mass fell together: CMP16-2). 57 suites, 56 pass, 33,347 checks (`test_mp_routes` 2,336, failing on 2-01 and 4-29),
      reachability 108 / 20 / 0, 0 warnings. Independently verified once (`CMP16/verify1.json`: pass; its medium, the record's stale rows, is corrected in this commit,
      and its lows are the rows below). **What it leaves**: (a) 2-01's route and 4-29's, re-flown by the route phase (planning step 176; CMP16-7), the 2-01 route's header
      text ("five times the player's weight", "a fifth of his pace") being stale; (b) the crate's 2 rows of height, a pre-existing resting offset (CMP16-5); (c) the
      unsplit mutation (CMP16-2); (d) `ShapeBoundsPx`'s scaled box against the script's unscaled one (CMP16-6). The deviations are rows CMP16-1 to CMP16-6 above the wave
      4c-1 rows, and CMP16-7 is the routes.
      *Step 176 (the route phase), 2026-10-15: 2-01 is re-flown (`level0a.json`: wins on tick 307, was 444; it stops pushing the lighter crate at tick 182 and the fireball burns
      the crate on 197) and 4-29 with it; the row's "MOVES A ROUTE" is done.*
      *Re-taken 2026-10-15 (wave 4d-1): FIXED, by step 172 (`9d643c9`): the crate is 135 px wide at x 442-576 as the original's (old 178 x 175 against 134 x 130); its top
      edge is 2 px low (CAP-2); the player and the arch read the original's. 2-01's f420 mean_abs 7.04 -> 3.31 against the old pass is the largest fall of the re-take,
      and most of it is wave 4c-1's lit player and arch; the crate's own share is 4.20 -> 3.31 (step 172's record).*
- [ ] **CMP-17. 3-08's lone guard rests 30 px right of and 7 px below the original's, astride the ledge seam** (medium; structural; `owner-call`; MOVES A ROUTE if R24
      changes). Evidence: 3-08 box x144-256 y48-192 (`pairs/3-08_h63_pair.png`, top left). The guard (spawn (410,48), wayA0 (372,48), hold 9999999) stops about 11 u
      earlier than the original's (`minions.json` `arrive_px` 8, `settle_px` 16) and ends over the seam of two ledge blocks; the same row was closed for 3-01 by step
      152 and 3-01 matches here at dx 0; why 3-08 stops at the seam is not isolated. Next: nothing until R24 (the patrol's arrival point, the owner's) is ruled. Joins:
      R24, step 76's lone guards' coast. GAMEPLAY: yes if R24 changes.
      *Re-taken 2026-10-15 (wave 4d-1): UNCHANGED for 3-08 (the guard 30 px right and 7 px low in every still; its box holds 0 changed pixels), WIDENED to four more
      knights found at rest off the original's, constant across ticks and so not phase: 4-30's wayB knight 12 px left of the original's (the old reading called it phase),
      4-28 9 px (original x 89-117, port 98-126 at f135-f450 and h2.8-h6.3), 3-02 8 px (red cape x 546 against 554), 3-20's knight on the spinning plank 6 px right and 1
      px low. 3-20's knight stayed 6 px when step 174 moved the plank to 0 px, so it was not carried with the plank. Scale (arithmetic from `game/data/minions.json`): the
      patrol speed is 62.4 u/s = 175.5 px/s, so one frame of the original's 30 Hz script is 5.85 px and `arrive_px` 8 u is 22.5 px: offsets of 6-12 px are one to two
      script frames inside the decoded arrival radius, which fits a brake / settle / hold ordering difference (`minions.json` brake 0.3, `settle_px` 16) but is not shown;
      3-08's 30 px is more. The signs differ (the port is right of the original on 3-02, 3-08, 3-20, 4-28 and left on 4-30), as a stop short of a waypoint approached from
      either side would. R24 (the patrol's arrival point) is still the owner's open ruling; the five knights are the check if it is ruled.*
- [ ] **CMP-18. 2-29's rolling stone is 125-135 px behind the original's in every still (WT-10)** (medium; structural; `unknown`; MOVES A ROUTE). Evidence: 2-29 box
      35,455,520,632 (`pairs/2-29_h63_pair.png`; the same lag at `h2.8` and at f450): in the original's h6.3 the stone is against the barrel at x 37-122, the port's at
      f420 at x 342-515 with the barrel unmoved; a constant 125-135 px (0.75 s at its 61 u/s). So the release from the hinged plank is late: the port tips at about tick
      340 and is still rolling at 61 u/s at tick 420 at x 209. Unexplained by the recorded row; the hinge pose (CMP-2) is predicted to move only 0.6 u here. Next, as
      WT-10 says: trace the plank's tipping time (the stone 1.131 against the plank and the hinge) after CMP-2 lands, then re-fly 2-29 from tap 320 (R4a-6). GAMEPLAY:
      yes.
      *CMP-2 landed (step 174, wave 4c-2) and is NOT the cause: the plank's first turn past 0.05 rad is tick 254 before and after, and the stone is 1.04 u (about 3 px) farther
      along at tick 420, of the 125-135 px. The trace of the stone against the plank and the hinge (WT-10's own) is therefore still the next step on this row; 2-29 was
      re-flown by step 176 for the wall's gate (564, was 569), on the port's stone clock, so R4a-6 is unchanged.*
      *Re-taken 2026-10-15 (wave 4d-1): UNCHANGED. The stone is 222 px behind the original's at f420 (right edge x 512 against 289; the barrel at x 103-190 against
      37-123, because the original's stone has already pushed it), 136 px at f450 and 124 px at h2.8 (the old 125-135 is the f450 / h2.8 gap). CMP-2 is ruled out as the
      cause (step 174's hinge moves the stone 1.04 u, about 3 px, and the plank's first turn past 0.05 rad is tick 254 before and after); the late release from the plank
      is unexplained and WT-10's trace of the stone against the plank and the hinge is the next step on this row.*
- [ ] **CMP-19. The carranca fireball's glow reads weaker than the original's beyond its core** (medium; light; `unknown`; moves no route). NEW TO THE LIST by the
      2026-10-15 re-take (wave 4d-1), not new in the build: the fireball boxes hold 0 changed pixels against the old frame (2-28's 3,319 are the platform at the box's
      foot), and the old readers filed the fireballs only as phase (where the ball is), never its light. Levels: 2-01 to 2-08, 2-28 and 2-32's dragon (10 read; 21 levels
      carry a carranca). 2-01 to 2-08 are one reader's entries with shared wording and pooled numbers (original core 1,450-2,180 px and R-B glow 15-21, port 800-1,450 px
      and 8-13): count them as eight levels where a fireball was in frame, not eight readings; 2-28 (the second reading of chunk 8, and the audit's only miss) and 2-32
      (the dragon lit by two fireballs, a second reader) are independent. Evidence, the triage's own radial profile (luminance and R-B excess over each image's 130-200 px
      baseline, annuli round the core's centroid, checked on the pair images; `R4d/CMP/work_triage/fireball_profile.py`): on 2-02, where the positions match (original
      h0.3 against port f105), the core (0-12 px) is equal, +161.3 against +161.9 luminance, then +88.2 against +35.8 at 24-36 px (8.5-12.8 u), +38.6 against +11.4 at
      36-48 px (12.8-17.1 u), +8.2 against -3.8 at 48-60 px; R-B +119.2 against +71.9, +71.0 against +39.0, +36.2 against +20.6; 2-04 (h2.8 against f240) and 2-07 read
      the same direction (at 36-48 px luminance +72.4 against +63.4 and +77.8 against +46.8, R-B +60.5 against +33.5 and +70.1 against +42.6). So on the three
      near-matched cases the port's halo is weaker from about 8 u outward and loses its warm tint from about 13 u. A CONFLICT NOT RESOLVED: the Order 7.2 rows (steps 85
      and 94) record the opposite sign on 2-01's pair, the port's tail LONGER than the original's (R excess to 10 at 40.0 u against 27.6 +-6 u; at 16.9 u the port reads R
      112 against the original's 81 and 83), and the readers cite step 85's core mean of 6.28 u against the original's 7.7-8.3: different baselines, one pair against
      three, and both cannot hold on the same effect. It is NOT shown to be `halo_brightness_scale` (CMP-8): the fireball's light is read from `fireball.ent` (range 50,
      colour (1, 0.5, 0.2), Scale 1.2; `lighting.json` `fireball`) and its falloff is Order 7.2's separate question; which knob moves the profile was not tested. Present
      on every level that carries a carranca, medium or low by magnitude (the re-take read 10 of the 21), so a fix is not scoped to the ten. Next: FIRST a read-only
      reconciliation on ONE pair (re-measure step 85's tail rows on 2-01 with this profile on the same frames); then, if the weakness holds, the fireball's light falloff
      and halo size in `lighting.json` `fireball` and `effects.json` against the 8 matched stills on 2 sides (and the dragon's two mouth fireballs on 2-32). Gate: the
      profile at 12-26 u within 15 percent on 2-02, 2-04, 2-07 and 2-28, the core unchanged (6-10 u), step 85's gates (8.94 u core, R 226 +-20) still met. Joins CMP-8's
      owner decision only if the scale is the knob. GAMEPLAY: none.
- [ ] **CMP-20. A crate the fireball burns draws no flame and no fade: the original puts `fire32` on it and fades it over 1000 ms** (medium; particles; `decoded`; moves
      no route). NEW TO THE LIST by the re-take, not new in the build: the old readers filed it as low phase ('the crate is intact in the port, the shot still in
      flight'); it is the recorded, decoded and unbuilt part of 'Found by step 120' and step 146's `fire32` ('a burnable crate's not drawn'). Levels: 2-05 (h2.8 an orange
      flame over the right crate, t8.0 a flame column), 2-08 (h6.3 the last crate burning and translucent), 2-22 (h6.3 the left crate in a flame cloud with ember blobs,
      h2.8 the right one); the port frame shows an intact crate with the fireball still in flight (a phase) or an explosion and no flame, and the changed pixels in those
      boxes are the crates' blink (step 169), not a flame. Scope by census of `out/levels`: 29 levels carry a crate with `burnable` = 1, 12 of them also a carranca (2-01,
      2-04, 2-05, 2-06, 2-08, 2-09, 2-14, 2-18, 2-22, 2-23, 2-25, 2-28), where a fireball can burn it. Cause (the listing, md5 `48fe65ce`): `crateCallback` (bytes
      421145..421695) calls `manageBurnable` (420664..421145) at #14; for a burned crate `burnTime` starts at 0 and `addFlames` (420120..420459) puts `fire32.ent` at the
      crate's position, sized by its width, with `targetID`; until `burnTime` reaches 1000 ms the colour and alpha bias is 1 - burnTime/1000, then the crate is deleted.
      The port has `fire32` for characters and burned minions (step 146, `effects.json` `fire32`) and takes a burnt crate away whole at 1000 ms, with no flame and no
      fade. The group is DRAWING ONLY, checked in the listing: `ETHCallback_fire32` (bytes 420459..420664) only follows its target (`SeekEntity(targetID)`,
      `SetPositionXY` while it finds it: no damage, no spread, no light, no delete), the original's `fire32.ent` holds a particle system only (`applyLight` 0, `sensor` 0,
      `density` 0, no Light element; `fire.png`, 32 particles, life 450 +- 300 ms), and the only other things `manageBurnable` does are the colour and alpha fade and the
      delete at 1000 ms, which the port already does. Next: `addFlames` as decoded (`fire32` at the crate, sized by its width, following it) and the bias fade 1 -> 0 over
      1000 ms on the crate's colour and alpha, in the code that today deletes the crate at 1000 ms; numbers in `effects.json` with the listing citation. The crate still
      leaves at 1000 ms and no damage rule is added. Gate: the f420-f504 frames of 2-22 and 2-08 show the flame and the fade at the ticks the carranca's decoded schedule
      burns the crate; the sweep changes only levels where a crate burns by f420; a pin in `test_mp_effects`; 3-30's 63-tick stand on a burning crate must not change.
      GAMEPLAY: none.
- [ ] **CMP-low. Residuals no play would notice** (low; 120 levels, 227 residuals; `unknown`, no step proposed). (a) PHASE (about 100 levels): animation, particle,
      flame, blink, mover and knight phases. Not defects of the port: the stills were timed by wall seconds on an emulator at 15-29 fps and the original runs two clocks
      the port runs as one (the HUD 0.49-0.94 s ahead of its world: `ui.json` `level_start._start_after`); a level-by-level fit puts `h6.3` at f440-f492, and chapter
      4's `h0.3` matches the port's f180-195, not f75 (4-09 at 1.98 against 11.40 at f420), so chapter 4's clock is later than the f75 / f240 / f450 sets assume
      (unmeasured: the calibration is chapters 1-3). (b) CAP-2 and FRAME: the engine's contact skin rests the player and crates 1.6-1.8 px low and the camera follows
      the player, so the world reads 2 px high on 15 levels (11 at (0,2): 2-07, 2-09, 2-13, 1-13, 1-19, 1-25, 1-26, 3-10, 3-14, 3-18, 3-30; the rest 1-24 (1,3), 2-25
      (0,-5, the camera), 3-07 (0,-2, a shake of the original's still), 2-32 (-94,0, the game-over fade)); a 2 px offset alone drops `edge_iou` to 0.33-0.46 with
      nothing visible. (c) CAMERA: the original opens on `camera_start` and eases to the player, the port opens on the player by the owner's choice (G3-2): `h0.3`
      offsets of 13-46 px on 11 levels; 2-25's camera is 5 px different at h6.3 (the follow lag `_guess` 0.12 against `Follower(1000,100)`: R3c-6). (d) BOB: the
      original's diamonds sit about 3.6 px lower than the port's centre (+3.6 mean on 22 chapter-4 diamonds; the same sign on 1-05, 1-06, 1-08, 1-25, 1-26, 1-28 and
      4-04; 3-01 reads the other way; the amplitude 4.2 px matches the decode). (e) TONE: 1-1.5 percent darker blue and grey on 4-09 to 4-16. (f) the space sky's spiral
      turns 1.0-2.9 percent under its decoded rate (P5, "Left by step 122"). (g) unexplained singletons: 1-21's pink tint on a plank, 2-11's redder crate by the door,
      4-18's player 6 px farther from the well, 4-20's key resting on the other side. (h) known rows: 2-32's dragon timing and game-over fade (step 77: f420 reads mean
      abs 14.97, f450 2.71), 3-29's `!` mark age (G1a-8), 4-22's sparkle depth (Order 8.4). Dropped as not defects: 4-20's achievement card (a fresh-save capture
      condition), 1-02 and 1-03's pad pulse (the anchored popup tick), 3-07's 2 px shake of the original's still, 1-03's 2 px edge tab. Next: no step. (a) needs the rig
      only if the owner wants the HUD / world clock offset modelled; (b) is an engine step (CAP-2: a contact margin, `PhysicsSettings` has none) that would move every
      route's rest heights; (c) is decided; (d) and (e) cost a footage read when a step next touches motions or sky. Joins: CAP-2, G3-2, G3-5, "Camera at 1-13", P5,
      step 77, G1a-8, Order 8.4, R3c-6.
      *Re-taken 2026-10-15 (wave 4d-1): UNCHANGED in kind, re-read with this pass's numbers (every one of the 128 levels carries a low item). (d) BOB CENTRE is now the
      most consistent low item: chapter 4's crystals read 3.1 px higher in the port on 23 of 24 (4-04 4.8), at f420 the original's diamonds are 5-8 px lower on 4-09 to
      4-16 (4-10 8 and 7, 4-12 8, 1, 6, 4-13 7, 8, 7), 1-18 and 1-20 the same sign, 3-17 the original 3.6 px lower, 3-18 and 3-20 the port's gems 3-6 px higher, 3-01 and
      3-02 the other way: about 60-70 levels at 3-8 px, part of it phase (the ranges overlap), and the cheapest to close with one bob-centre fit. (e) TONE: chapter 4
      reads 1-2 percent darker (4-01 to 4-08 whole-frame 0.95-0.99 per channel, 4-09 to 4-16 0.9866 on static stone; not measured by the chapter 1-3 readers). (b) CAP-2
      rest skin: the player 1.6-2.0 px low on 7 of chapter 4's last 8 levels and on chapters 1-3, crates and barrels 2 px, a stack adding up (2-10 crates 3-6 px, 2-16
      barrels 4-6, 2-24 the player on his barrel 5). 4-22's sparkle depth (Order 8.4) is not reproduced on this build. Dropped as not defects: 4-20's achievement card,
      3-07's 2 px shake of the original's still, 1-02 and 1-03's pad pulse and popup moment. Next: still no step.*
- [ ] **CMP-R1. The comparison's own limits** (the verification's six low findings, `R4b/verify1.json`, and the triage's). (1) **The pairing is the record's convention,
      measured in part**: `h6.3` against f420 is not a measurement (f420 is 0.3-1.2 s early on the script clock; f450 is the extra set, +-0.4 s level to level); the
      script clock was fitted on four chapter 1-3 platform levels and 2-32's dragon; `h0.3` and `h2.8` use f75, f135, the level's own HUD tick (`CMP/hud_ticks.json`,
      +-8 ticks, 17 levels "gone") or f240. A reader judging a mover, a dragon or a knight off f420 judged the clock first. (2) **`match` is not pixel-equal**: the 26
      `match` levels have no residual above low (phase, clock, tone); 4-09 reads mean abs 7.50 at f420 (1.57 at its own HUD tick), 4-18 11.38 and 4-12 10.30 (a spiral
      nebula and two platforms at the other end of their swing, by the readers), and chapter 4, where the HUDs are oldest, was not calibrated. Take the next comparison
      at the level's own HUD tick or f450 where a mover or the spiral is in frame. (3) **The seed is the all-gold save** `out/parity/ui1/saves_gold/scores.json` (md5
      `09268640`), not the owner's `40bfef3a`, because the library's stills were taken on an all-gold save and show the plaque on every level; the owner's save differs
      only in the plaque card (at tick 90: 1-01 0 px, 3-16 42,165 px, 2-32 42,202 px, in x 23-201, y 13-250); at f420 the plaque is gone and the 128 gold-seed frames
      equal the owner's-seed sweep byte for byte except 1-02 and 1-03, which are captured with the popup closed (`--tap 1178,358@300-305`; their popup-up frames, kept
      in `port_f420/popup_up/`, equal the sweep). (4) **The readers' audit**: 16 of 128 levels (levels 4, 12, 20 and 28 of each chapter), 16 agree, 0 missed, 0 not
      real; it re-measured pixels, not causes; 0 of 16 bounds a per-level miss rate at about 17-19 percent (95 percent); the readers did miss the help block on 2-03 and
      2-06 outside the sample (CMP-10), and the structural single-level findings (1-30, 2-01, 2-31, 3-05, 4-02, 4-29, 4-31) are all outside it. The verification re-read
      six pair images (1-23, 4-09, 4-26, 3-10, 3-20, 1-20): 0 wrong. (5) **Causes**: the triage checked the mechanisms against the listing and source (10 of 11
      attributions held); nobody re-derived the halo magnitudes, the lightmap softness, the pile physics or any phase attribution, and 3-05's and 3-22's knights have no
      footage. (6) **Not compared**: edge rows (PX-2 to PX-4: the 2,047 rows above 12 and the slivers were not attributed), any pan or tremble against the original's
      footage (PX-3), a window other than 1280 x 720 (PX-6). (7) **Build currency and zero warnings** rest on mtimes (the executables `a5013ee6` and `32f4a14b` are
      newer than every compiled source) and on PX's build logs (0 `warning` lines), not on a rebuild at the close or a ninja up-to-date check. (8) One python launch,
      `python.exe` loading scipy's `_flapack` at 17:14:08, was refused by Smart App Control in the comparison phase (not a wave executable; the triage tools should
      avoid scipy). Next: the next measuring pass, after wave 4c's fixes, on the fixed build, with (1), (2) and (6) in hand.
      *Re-taken 2026-10-15 (wave 4d-1): the limits stand, and the re-take adds its own in R4d1-3 and R4d1-4. (1) The pairing was NOT re-derived (the ticks, the sets and
      the save rule are wave 4b's; `hud_ticks.json` is carried unchanged, md5 equal) and the script-clock fit on four platform levels and 2-32's dragon was NOT re-run.
      (3) The seed is the same all-gold save; the 128 gold-seed f420 frames equal `WALL/after/sweep` (the owner's seed) byte for byte, 1-02 and 1-03 in their popup-up
      form. (4) The audit this time found 15 of 16 levels agree and one miss (2-28's fireball halo, medium; the first audit found 16 of 16); the second reading of chunk 8
      found a medium the first missed on 5 of 8 levels; the audit's sample holds no lightmapped level of chapter 2. (6) Edge rows, any pan or tremble and a window other
      than 1280 x 720 are still not compared. (7) Build currency: `MagicPortals.exe` `f27dde1a` was checked by md5 at the start and the end of the sweep and nothing was
      rebuilt. (8) No python launch was refused (0 CodeIntegrity events from 11:00). The row stays open.*
- [ ] **R4d1-1. 1-15: `match` -> `minor` is a re-grade of a cause no step touched, not a regression of the build** (the one level whose verdict got worse; CMP-13; low).
      The lower static portal over the pale sky: the original's iris is a neutral grey-white disc (mean RGB 143-159 / 130-160 / 165-181, luma 138-162 over six stills),
      the port's a pink-magenta disc (182-206 / 151-182 / 214-239, luma 167-194 over six frames), no overlap; the old reading filed it on this level as low phase. The
      portal's box (880,500)-(1000,620) holds 0 changed pixels against the old frame (step 167 changed only a strip at y 633-684, 2,334 px, the largest channel change 9)
      and mean_abs is 3.07 both times; the verifier read the old and the new pair image and the same pink disc stands against the same grey-white iris in both, so the old
      `match` was an under-grade and the old count of 26 held at least one level that was not. Likely step: none. Closes with CMP-13 (the rings' life-versus-brightness
      curve against the 58 footage frames).
- [ ] **R4d1-2. Numbers that rose, and mediums the old reading did not list** (the re-take's regression check; low; no build-caused regression found). (a) 1-22's f420
      mean_abs 4.73 -> 4.77 (+0.04) is the only level whose f420 mean_abs rose: the cracked breakable column now blinks (step 169, CMP-9) and the port reads 0.92 of the
      original at f420 and 1.01-1.03 at f75 and f240, so f420 sits at the dim end of the blink (8,908 of the 122,100 px of the lamp-wall box changed); the same dim end on
      1-13 (68.0 against the stills' 71.5-84.5), 1-24 (8-9 percent) and 1-25. It is CMP9-6, now measured on three more levels. At the other sets the rises are small: 3-14
      f75 +0.15 and own HUD tick +0.15 (the four cracked stones' pulse), 2-15 f75 +0.11, 1-09 f240 +0.06, 4-10 f450 +0.05, 4-02 f135 +0.04; none changes a verdict. (b) 22
      levels show a medium the old reading did not list at medium (23 residuals: CMP-19 on 2-01, 2-02, 2-03, 2-04, 2-06, 2-07, 2-08, 2-28 and 2-32; CMP-14 on 2-10, 2-11,
      2-16, 2-26, 2-27, 2-30 and 2-31; CMP-8 on 1-08, 1-10 and 3-25; CMP-20 on 2-05, 2-08 and 2-22; CMP-13 on 1-15). The test, for every residual with a box (490): the
      pixels of the port's f420 frame inside the box that differ between the old pass and this one (`R4d/CMP/work_triage/box_changed.json`). Every one of those boxes
      holds the same pixels as the old frame, or only the player, arch and blink pixels that steps 166 and 169 changed (the CMP-14 boxes are wide: 2-16 45,896 changed px
      of 228,330, 2-30 102,184 of 371,100, which are the player, arch, blinks and hinge pose): they were there before and are named now that the lit player and arch no
      longer fill the readers' attention. Next: nothing of its own; the rows it names carry the fixes.
- [ ] **R4d1-3. What the re-take left untraced** (low). (a) 2-29's hinged plank leans 1.3 degrees in the original and stands vertical in the port (3 px at the ends): the
      left edge x of the plank at y 240 / 270 / 480 / 500 is 813 / 815 / 818 / 819 on the original's h6.3, 815 / 815 / 815 / 815 on the old port f420 and 817 / 817 / 817
      / 816 on the new (and f450), vertical in both ports, so the lean was never built and step 174 only moved the plank 2 px; no build-caused regression, and the lean is
      not tied to the stone's clock (CMP-18). (b) Three stills' HUD reads moved with a cause not traced: 2-09's h0.3 caption 0.377 -> 0.461 and its h2.8 and h6.3 caption
      -0.156 / -0.151 -> 0.0; 2-29's h2.8 and h6.3 plaque -0.185 / -0.188 -> 0.0; 4-30's -0.149 / -0.147 -> 0.0 (in each the old reading was negative, a box darker than
      the estimator's background, and is now 0.0 = gone, as in every other still; the old pass's non-f420 frames were purged, so there is no before / after). (c) Two
      levels' own HUD tick would change on the new reading (2-09 f120 -> f105, 2-25 f90 -> f105); `hud_ticks.json` was kept as the old pass wrote it so that old and new
      numbers sit against the same ticks, and the other tick moves compare.py's h0.3 mean_abs by at most 0.4 grey levels. (d) 1-12's first launch of the sweep exited 0
      after `Window close requested` at about tick 50 with no input the script scheduled (no `DEV tap`, `portal_launch` or `Escape` in its log); the one retry gave f420
      `ab373cd0`, equal to the baseline. (e) The mover fit (the script clock) and the pairing were not re-run (CMP-R1). Next: nothing until a step touches those boxes.
- [ ] **R4d1-4. The re-take's counts are the readers' verdicts under the old pass's one rule, and a lower bound on medium residuals** (the verifier's first two lows;
      low). Quote 61 / 65 / 2 with: the readers' own labels are 58 / 68 / 2 (3-17, 3-19 and 3-23 are re-graded to `match` by the rule, every residual of them low); the
      audit found 1 miss in 16 levels (6 percent) and the second reading of chunk 8 found a medium the first missed on 5 of its 8 levels (an upper bound: the chunk was
      chosen because of the audit's miss); the other 15 chunks were not re-read, so medium misses of two classes (lit-stone softness on lightmapped levels, a light effect
      on a moving object) are likely under-reported there, and the audit's sample holds no lightmapped chapter-2 level. The grading of one light cause is not uniform
      across readers: CMP-8 is medium on 37 of its 57 levels and low on 20, and 3-19's fire-agent halo (very faint in the port) and lit stones (7-11 percent brighter) are
      low where the same kinds of cause are medium on chapter 2 (CMP-19, CMP-14); so CMP-8, CMP-14 and CMP-19 are present on every level that carries the entity and a fix
      is not scoped to the medium levels. Next, if the count is to be quoted outside this file: re-read one more chunk of lit levels of chapter 1 or 3 (read-only; the
      verifier's advice), or say the sentence above. Open until then.

**Blockers: levels that cannot be won** (each built as the original does, and the level won by a route).
- [x] **B1. A barrel's blast breaks no wall** - 2-12, 2-13, 2-14, 2-16, 2-17, 2-19, 2-21, 2-24 and 4-16 cannot be
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
      2-24. *Step 134 (G8) wins 2-16, 2-17 and 2-19 (260, 459, 342), each by a barrel its fireball or a flame sets off
      after the barrel is moved by a portal pair; 2-21 and 2-24 remain (B4).*
      *Step 139 (AR, B4's handle) wins 2-21 (815) and 2-24 (722), so all nine are won by routes `test_mp_routes`
      replays on the merged build (step 144): 2-12 576, 2-13 500, 2-14 518, 2-16 260, 2-17 459, 2-19 342, 2-21 815,
      2-24 722, 4-16 764. Closed by its own measure.*
- [ ] **B2. 84 of 128 levels never unlock from the menus** on a fresh save, because 2-12 cannot be won (chapter 2
      tops out at 34 % of its medals, under the 60 % chapters 3 and 4 need). Goes with B1. *Not cleared by step 115*:
      2-12 is won, but a level opens only when the one before it scored, and chapter 2's order also stops at 2-06 (G6)
      and 2-10 and 2-11 (G8: 2-10's crates stand 133 u from its fire agent and cannot be sent through a portal, 2-11's
      static crate 206 u). Measured with a save of chapter 1 gold (a fixture) and every chapter-2 route's medal: 2-13
      to 2-15 open behind 2-12, 2-07, 2-11 and 2-12 locked, chapter 2 at 23 % (`out/parity/program/B1/b2/`). Open
      until G6 and G8 let those three be won. *Unchanged by wave 1*: track C's chapter-2 routes (2-15, 2-18, 2-23,
      2-25, 2-26, 2-30, 2-31, 2-32) lie behind 2-06, 2-10 and 2-11 in the menus' order, and none of those three has a
      route. Not re-measured. *Step 134 (G8) wins 2-10 and 2-11, re-routes 2-31, and re-measures it as step 115
      did (`out/parity/program/G8/fix1/b2/`): chapter 1 gold (the fixture) and chapter 2's window-replay medals, 47 of
      96 (2-31 silver, 2-32 gold); chapter select reads 48 %, under the 60 % chapters 3 and 4 need; 2-11, 2-12, 2-16
      and 2-32 now open, 2-07 still locked behind 2-06 (G6), 2-21 to 2-23 behind 2-20 (B4), 2-25 behind 2-24, 2-28 to
      2-30 behind 2-27. Open until G6, B4 and the unrouted levels are won.* *Step 135 (G6) gives 2-06 a route, so
      2-07's lock behind it can clear (not re-measured). Open until B4 and the unrouted levels are won.*
      *Wave 2 adds routes for 2-20, 2-21, 2-22 and 2-24 (step 139); chapter 2's levels with no route are now 2-27,
      2-28 and 2-29, so the menus' order still stops at 2-27 on a fresh save (not re-measured). Open until they are
      won.*
- [x] **B3. 4-32 cannot be won** - the barrel restock (`addEntityIfItCantBeFound('barrel_bomb')`, `ETHCallback_dark_dragon`
      ins 997-1064) and the lava's explode contact (`ETHBeginContactCallback_lava` L68416 #21-30) must land together,
      or a missed barrel softlocks. KNOWN ("Order 12.1, not built"). *Built in step 116*, both: after every shot at the
      player that leaves (never a refused one, never the wound's torch shot) a barrel at `barrel_spawn` when the level
      holds none (`sim/Restock`, general; the dragon's caller wired, the ghost's three left to 3-32's step), and
      whatever touches the lava is burned if burnable and asked to go off (`Fire`'s lava). GAMEPLAY, 4-32 only (the one
      level with either). **Won by a route: 4-32**, tick 3421, three burns by barrels its own fireballs set off, no DEV
      flag, replayed by `test_mp_routes` and in the window on the same tick.
- [ ] **B4. 2-20, 2-22, 2-23, 2-27 at risk: the draggable handle** (`arrows.ent`, `ETHCallback_arrows` L67305, `doFollow`
      from `fire_agent` #280 and `reflect_agent` #6) is filed as cosmetic and not built; nothing reads `follow`.
      VERIFIED mechanism, INFERRED unwinnable. Needs release-to-fire (below) and the camera ruling. **Blocks 2-21 and
      2-24** (step 115, both `follow` agents; no port route found): 2-21's fixed agent is 103 u from its wall at best
      and the level has one barrel; on 2-24 one barrel in the fixed agent's zone sets off all seven (measured, ticks
      68-72), leaving nothing to hold `button_1144` in front of the door (that the player alone cannot then pass the
      door lift is reasoned from the geometry, not played; a barrel kept out of the chain to weigh the button, which
      works in the original too, was not tried - so 2-24's attribution to B4 is partly reasoned). *Step 127 wins 2-23
      by a route without the handle (tick 773; replayed on the merged build); 2-20, 2-22 and 2-27 have no recorded
      attempt.*
      *Built in step 139 (AR, wave 2's track B, `0e76e6d`): `ETHCallback_arrows` - a touch down on the handle and held
      drags it one for one, at most scale(64) a tick; faint (0.3) at rest and whole while held (the audit's "alpha 0.3
      while moving" is inverted against #190-200 and the 2-20 stills), pulsing (1.1, 1) <-> (1, 1.1) every 300 ms;
      `doFollow` - the fire agent or reflector that names it eases after it (followUp 600 / 100); `shouldLockCamera` -
      the camera's finger pans and holds nothing while a handle moves; 2-20's `hand_waypoint0` lesson. GAMEPLAY. The
      merge (step 144) puts the flame a shot meets where the last tick's follow left its agent, as
      `ETHCallback_fire_agent` burns (#253-265) before it follows (#280). **Won by routes: 2-20 (344), 2-21 (815),
      2-22 (274), 2-24 (722)**; 2-23 by step 127's route, re-authored in step 134 (758). **2-27 is open** (a finding):
      its door room's crates are burnable and not teleportable, 63 u from the only flame, which is fixed (reach 32);
      only a shot that flame turns into a fireball, aimed by the handle's reflector, reaches them. G8 and the handle
      are on one build since the merge, and no route has been tried. Open until 2-27 is won.*
- [x] **B5. 4-19 at risk: the red key breaks the wall** - `ETHCallback_red_key_state` (L97171): the key carried ->
      its sound, a 6000 ms fade of light and ambient, then `SeekEntity('breakable_wall')` destroyed (F17 #216). Nothing
      reads `red_key_state`. (The "Order 11.1" row says "destroyed"; the trigger is the key carried.) *Built in step 118*
      (`sim/RedKey`): on the tick the key in its bucket has an owner, `red_key_got`; from the next, the halo's colour and
      the level's ambient x smoothBeginning(smoothEnd(1 - t/6000)); at 6000 ms the halo goes and Demolish takes
      `breakable_wall_2756` (`SeekAndDestroy`, then `Destroyed`), with its dust, quake and sound. The halo is drawn red
      (1, 0.1, 0.1) and stretched (0.8, 1.5) from its first call. `entity_roles.json` gives `red_key_state` its own role.
      F17's fade fits the port's rule on the original's frames with RMS 0.59 grey levels (as its free fit). GAMEPLAY, 4-19
      only. **Won by a route: 4-19**, tick 581 (the key 129, the wall 489), replayed by `test_mp_routes` and in the window
      (`MagicPortals.exe` `cbdb4004`); the after-sweep 127 of 128 byte-identical, 4-19 its halo. Its bucket is 128 level
      units, `Game::start`'s own (fix round 1; the first record's 91.02 was the four-string `LoadScene` misread).
- [x] **B6. 3-13 at risk: an anti-portal agent should die to a shot**, taking its ring with it
      (`ETHCallback_anti_portal_agent` L66304 #109-187, `deleteAntiPortalInThisBucket`); the port keeps it a permanent
      zone (R17). 21 agents on 17 levels. Build before or with R1, which alone closes 4-01's route. (The audit's G7
      is this row.) *Built in step 126* (program wave 1, track C, `596862e`; R17): the agent is a role of its own and
      no zone; a portal shot in flight within 0.7 x 32 = 22.4 px kills it and deletes the one bare `antiportal` ring
      round it (the bucket search), with `antiportal_explosion`, a shake of 20 and `playExplosionSound(true)`.
      GAMEPLAY on the 17 agent levels (21 agents). **Won by a route: 3-13**, tick 1027 (the kill on 350); with it 1-10,
      1-11, 1-12, 1-21 and 1-23. R1 followed in step 127, and 4-01 is won by that step's route (tick 351). Left: 1-23's agent sits inside `rolling_stone_ent_877` (r 30 > 22.4), where the port's shot dies on
      the stone and the original's tap-time ray ends 10 u inside a teleportable body and would kill it (R23; the route
      wins without the kill); the static callback runs only while the agent's bucket is drawn (not modelled); a shot
      fired from within 22.4 kills at once (G4's row). *Step 156 (R23, wave 3b-2, local) built the ray's end as the shot's
      destination: a shot at a teleportable stone now flies 10 u inside it and does not die on it. Whether it then kills
      the agent inside `rolling_stone_ent_877` was not re-measured; 1-23's route is one of the six that lose their win on
      that build (R23-9).*
- [x] **B7. 4-21 at risk: R2** - the well's port-only zone (248 px) covers the button, the wall and the door. KNOWN.
      *The port-only zone is gone (step 125, R2), and 4-21 (`level20c`) is still not won.* The cause is now the port's
      lift door, which has no latch: portal resets over `button_1723` raise `door_lift_1721` 5.6 px at most (y 106 to
      100.4, 16 ticks a pair), against more than 15 px for a shot line under it and 43 px to walk under. The
      original's door latches (`ETHCallback_door_lift` #330-546), so one press opens it fully. Blocked on the "Order
      11.1, GAMEPLAY rows for the owner" row (the lift door's rise and status machine, step 109 deviation 3), which is
      decided for the original and needs only a step spec that names it; that step adds 4-21's route. *Built in step
      133 (LD)*: `Mover::Door` runs `ETHCallback_door_lift` #1-638 (a rise of 0.8 x 128 = 102.4 u eased by smoothEnd;
      opened only from shut, closed only from fully raised, a press during the fall landing it and rising again) and
      `Channels::Crush` its begin-contact callback #1-117 (a door closing onto the player kills it). GAMEPLAY, all 46
      lift doors on 37 levels. **Won by a route: 4-21** (`level20c`), tick 587: two taps and the walk, no DEV flag; the
      drift over `button_1723` (218-222, again about 229-233) lifts `door_lift_1721` all the way over its 6000 ms and
      the player passes under it (490-575). Replayed by `test_mp_routes` and in the window (`LD/verify1`) on the same
      tick.

**Majors with no row above** (VERIFIED unless marked).
- [x] **G1. Knights never see, shoot or destroy portals** (BUILT by steps 158-159, each independently verified and committed locally, not pushed; the stone's kill of the player, G1b-5, is built by step 160) (`ETHCallback_minion` L87616: 120 deg cone + occlusion ray,
      `addEnemyShoot` every 2000 ms, `killPortal` within scale(16), `!`/`?` marks, `cantReach`, the six darkest levels
      see only while a torch is lit). 34 levels play easier; `anti_sight_wall` has no purpose. *Routes that lean on it
      (wave 1), which its step re-verifies*: 3-02, 3-03, 3-07, 3-09, 3-12, 3-15 (knight B wedged on `button_1666`),
      3-19, 3-20, 3-22, 3-27, 3-29, 3-31, 4-25, 4-29, 4-30 (step 127).
      *Wave 2*: step 140's routes for 3-08 and 3-10 send knights off a floor's end (the original's walk has no
      floor probe) and lean on G1 unbuilt; G1's step replays them. The seven stealth achievements (24, 26, 28, 32,
      42, 66, 76) unlock whenever the minions all stand, because sight is not built (step 143; 3-02's route unlocks
      24 and 4-30's 76), and the minions' spotted, question and shot sounds wait on it (step 142).
      *Wave 3b-1: step 152 (WT) built the walk's speed (a minion 62.4 u/s, a 2-32 knight 81.6: CK-3's G1 half) and not the sight,
      the shot or the kill, so the row stays open. Step 155's routes of 3-22 (1,181, a drop good for one tick) and 3-20 (555) assume
      a knight that never sees; G1 moves them.*
      *Wave 3b-2, step 158 (G1a, local, not pushed) built the PERCEPTION half: a knight looks every frame through the three
      128-unit buckets of its row along its facing, inside a 120 degree cone, past a ray that every body but the knights'
      and the player's blocks (an `anti_sight_wall.ent` sensor first among them); a sighting zeroes its dwell clock, shows
      `exclamation.ent` (once its clock is past 4000) with `enemy_spotted`, and counts; a way to its waypoint that the ray finds
      blocked is searched (`question.ent`, `minion_question`), not walked; on the six `darkest` levels it is blind to the
      player (not to a portal) until a torch is lit. The row stays OPEN for G1b: the shot (`addEnemyShoot`,
      `ETHCallback_enemy_shoot`, `killPortal`, `playEnemyShootSound`), the wiring of `Minions::State::SpotCount()` to the seven
      stealth achievements (still unlocked whenever the knights all stand), the consumer of the `ShotDue` records the gate
      already writes, and the contact's hold cap (G1a-10, `ETHBeginContactCallback_minion` #12-55). The step's GAMEPLAY change is
      on the 35 levels that place a knight (planning step 158); the independent check passed 13 gates. **`test_mp_routes` is red
      on 17 routes (the twelve of steps 156-157 and 3-07, 3-22, 3-32, 4-28, 4-29; 3-02 wins 13 ticks later; 69 of 86 win) and no
      route file was touched: the routes are re-authored by the wave's route phase, after G1b.** The rows are G1a-1 to G1a-11.*
      *Wave 3b-2, step 159 (G1b, local, not pushed; the independent check passed 11 gates) built the ACTION half and closes the row: a knight whose gate opens fires an `enemy_shoot.ent` (the ghost's entity, the
      same numbers: 120 units a second of the script's clock, 16 out to begin with, no body) at where the sighted entity stood; it is judged by its STORED target's id alone, and within 16 units of it a
      player's hp is set to 0 and a placed portal is `killPortal`'d (it leaves the pair, fades 400 ms; nothing is refunded); every shot ends in a burst, a shake of 10 and `enemy_shot_ended`. The contact's hold cap (G1a-10) is built: the
      begin of a contact with the player caps a knight's hold at 1000 ms. A barrel's blast and a rolling stone's crush now kill a knight (the original's `explode` and `ETHBeginContactCallback_rolling_stone`); there is no
      sword in the original. `SpottedCount()` is wired, so the seven stealth achievements are real. **GAMEPLAY changed, plainly: knights shoot and kill the player and portals on 35 levels** (planning step 159). **`test_mp_routes` is red on 25 routes
      (61 of 86 win; G1a's 17 and eight new: 3-03, 3-08, 3-09, 3-27, 3-31 and 4-30 shot, 3-10 and 3-12 with their knights crushed) and no route file was touched: the routes are re-authored by the wave's route phase, after G1b.** The rows are G1b-1 to G1b-10.*
      *Wave 3b-2's close: the row is built (G1a, G1b, and STK's stone) and its routes are re-authored by step 161 (3-07, 3-22, 3-32, 4-28 and 4-29
      for G1a's sight; 3-03, 3-08, 3-09, 3-27, 3-31, 4-30, 3-10 and 3-12 for G1b's shot, the crush and the stone; 3-02 wins on 400; 86 of 86 win).
      The wave's open rows on it are listed in "What the next wave should take"; the rig checks are R3c-11.*
- [x] **G2. Lifts run on their own**; the original's idle at marker `a`, and a tap inside their rect sends them at
      60 u/s (`ETHCallback_lift` L83504, `sendLiftToOtherPoint`) and fires no portal. 14 lifts on 13 levels. *Routes
      that ride the self-running lifts, which its step re-authors*: 2-02, 2-07, 2-09 (step 114; the Tests row), 1-27,
      1-28, 1-29, 2-18, 2-25 (step 127) and 3-31 (the merge's re-route rides `lift_ent_2438`; step 128, deviation 2).
      *Step 136's platform gate reads the port's lift*: on 1-28 it holds `crate_small_ent_708`, riding
      `lift_ent_694` down through the portal at (88, 175), for 9 ticks (868-876), so G2's step re-times that too.
      *Built in step 140 (G2, wave 2's track B, `056e1cd`): `ETHCallback_lift` / `sendLiftToOtherPoint` - a lift
      stands on its marker `a` from the first tick and waits; a touch down inside its picture (64 x 32 units, both
      edges in), off the walk pads, sends it (a -> b, b -> a, between -> a) at 60 units a second (the guessed 120 and
      2-02's unread `speed` gone), and it stops on the far marker; that touch fires no portal shot;
      `playElevatorSound` once per send, none on arrival; the 0.6 <-> 1 blink on 300 ms legs. GAMEPLAY on the 13 lift
      levels. Routes re-authored with lift touches: 1-27 (660), 1-28 (1088), 1-29 (589), 2-02 (568), 2-07 (1126), 2-09
      (1552), 2-18 (1194, then 1191 at the merge), 2-25 (510), 3-31 (1248); **won by new routes: 3-08 (1643) and 3-10
      (1085)**. The merge (step 144) re-authored 2-06 (575: two touches send `lift_ent_832` down and up). No route is
      held by step 136's gate on the merged build. 3-23 is not won (the row "Step 140's recorded deviations").*
- [x] **G3. Moving platforms swing half as far at twice the rate** (R7: `linearMotion` cos(angle) x stride from
      +stride; the port sin(2 x speed x t) x stride / 2). 15 platforms on 11 levels, 3 patrolling agents. *Routes that
      lean on it (wave 1)*: 1-11 (step 126: the kill of `anti_portal_agent_599` works at any point of the port's half
      swing and is phase-dependent under the original's; its door-block shot to (740, 108) passes 0.6 px from the
      lowest point of `anti_portal_agent_639`'s original swing, where it would kill that agent and open nothing) and
      2-26 (step 127: a shot through the slot from the moving platform's edge; the full swing would widen it).
      *Built in step 154 (wave 3b-1, local, not pushed): `Mover::Oscillation` is `Motion::Linear` over the node's own `speed` and
      `stride` (movers.json `linear_motion`, decoded: `ETHCallback_moving_platform` ins 1-17, `ETHCallback_anti_portal_agent` ins
      200-250): the node's point plus `cos(angle) x stride`, the angle stepped by `speed x 16 ms` BEFORE the cosine, from 0 - the
      FULL stride either side at `speed` rad/s, the first call already a stride along (down the screen), a bare teleport with no
      velocity; every later tick `MoveKinematic`. An agent is tested before it moves (its callback's order) and the spawn pass of a
      tap's own frame reads where it stood before its last move. The dead zone-patrol branch is removed (no antiportal callback
      moves a ring). GAMEPLAY on 12 levels (1-06, 1-11, 2-26, 2-28, 2-31, 3-03, 3-06, 3-09, 3-28, 4-12, 4-22; 3-14's agent): every
      platform swings twice as far at half the rate from a stride down, the three agents likewise (1-11's block y 114 to 254, was 149
      to 219). Measured on the original's own stills: 1-11's block and horizontal agent fit ONE phase to 1.75 u rms (the old swing
      30.8); 1-06, 2-26 and 3-28 to 1.34, 0.70 and 1.64 u. **The spec's friction carry is refuted by the decode** (a static body moved
      by `SetTransform`, vertical only: G3-1). Routes: 49 of 86 win, 1-06, 2-26, 3-03 and 3-14 now fail on the new geometry (the wave's route
      phase re-authors them after PIC, CAP and G3); 1-11 now has the step R3-1 hypothesised (the block starts 14 u under the island floor and ends 14 over
      `double_block_plat_ent_708`) and no route yet. The shock agents already swung as `cos x stride` (`Fields`).*
      *Step 155 (`7c843c7`, local, not pushed) re-authored the routes G3 moved: 1-11 is won (835, by the rising platform and its
      three agents' kills), 1-06 on 808, 2-26 on 495, 3-14 on 1,141 and 3-03 on 607; 3-09 (747) names the tick-1 phase; 49 -> 86 of
      86 win. The bucket clock (G3-2) and the rider's push-out (G3-1, G3-4) are still not modelled.*
- [x] **G4. The portal shot flies 600 u/s against 200** (`ETHCallback_projectile` L71109 #113-125, with a 24 u jump
      on its first frame, #29-55). *Every route leans on it* (with G10), and 2-14's, 4-19's and 4-32's are tick-exact
      on it (their rows). *And a first-tick kill* (step 126): a shot fired from within 22.4 u of an anti-portal agent
      kills it at once, even a self-tap that arrives on its first tick, where the original's projectile jumps 24 u out on
      its first frame and one that arrives on that frame is never in flight (`placement.json`
      `anti_portal_agent._not_modelled`). No route and no suite fires from there. *Built in step 149 (wave 3a, local,
      not pushed): the shot is the original's callback - a jump of 24 on its first run, an advance of `200 x
      min(200, the script's ms) / 1000` on every run (3.2 units a tick, 192 a second of wall time at 60 Hz), arrived
      when it has gone STRICTLY past its destiny, the portal opened at the destiny, never at the shot - read by an
      agent's and a flame's static callback by its POSITION once a frame (at its spawn on the tap's own frame, 27.2 out
      on its first callback, then every 3.2), and gone before they look on the frame it arrives. No tick is added to
      the tap (the layer reads it after `AfterStep`). GAMEPLAY on every level with a shot: a shot takes about three times
      longer (100 units 11 -> 24 ticks, 300: 32 -> 87, 500: 53 -> 149), so a second tap inside a flight is the hourglass
      far more often. **The first-tick kill row, ruled from the decode**: an agent within 22.4 of the player does die on
      the first tick with any shot that has a way to fly (the static callback of the tap's own frame sees it at its
      spawn), a self-tap with none spares it, and an agent only the first hop's line passes is spared. Footage F2 and F3
      (nine clean shots, 87 frames): speed 193 +- 3 u/s against 200 (192), the 600 out by 82 u rms against 6.7; F3's
      nine arrivals within -0.064..+0.030 s of `(D - 24) / 192`. **8 of the 86 routes still win** (G10: 23): 15 that won
      fail now (14 on a second tap refused by a flight, 2-12 on a pair opened 27 ticks late) and 38 routes in all are refused
      by a flight (`census.md`); no route file was edited, **the routes are re-authored by the wave's route phase after PIC**.
      Commit `012dd1a`. The routes were re-authored in step 151 (`af008a9`): 84 of 86 win, every second tap spaced past the
      flight and past CK's 26 ticks. Its recorded deviations are the block "G4's recorded deviations".* *Step 155 re-flew the routes again on the time-base build: 86 of 86 win.*
- [x] **G5. Fireballs pass walls and closed doors** (`ETHBeginContactCallback_fireball` L85862 #152 dies on anything
      solid that is not a carranca, a sensor or a spared character). On 2-12 the player dies at the spawn (tick 180).
      *Built in step 114*: the fireball's body is fireball.ent's circle, radius 9.6; every solid body it touches spends
      it, and is burned or set off when it is burnable or explosive; 2-12's player lives at the spawn, the fire
      stopping on the closed door. Routes for 2-01 to 2-09 but 2-06 (row below). GAMEPLAY: 2-09's crate now burns
      only while `button_1022` is held as a fireball passes, the original's rule - until step 114 the port's fire burned
      `crate_no_emissive_1004` through the closed `door_lift_1007` on tick 167 with no input.
- [x] **G6. Fireballs never pass through portals** (`mayTeleport` lets one through once; the pair survives). *A
      blocker for 2-06 (step 114) and for 4-14 (step 127, deviation 2)*: every player centre that touches 4-14's door
      is at most 64 px from `shock_agent_2285` (areaRadius 90), and the carranca's lane passes the agent at 100.2 px
      where contact needs 41.3; a fireball flies through a portal in its lane unmoved (`R1/fix2/cap/l13c`). G6's step
      routes 2-06 and 4-14, and re-authors 2-03's route (it hops each fireball through a portal pair).
      *Built in step 135*: a fireball whose centre comes within 22 u of a placed portal with a partner (half its
      unscaled 16 u box and the portal's 14, `EntityGrabber`) goes to the partner's point flying back the way it came
      (`teleportToOther` #152-176 turns its 'dir'), once in its life (`teleportedOnce`, #178-207; `mayTeleport`); the
      pair is not spent (`doesItDestroyPortals`) and neither end is refunded after (#125-150); the traversal is
      recorded as any traveller's, so the layer draws the teleport's effects and plays its sound (seen in the window
      by the step's verifier on 2-06, 4-14 and 2-03: the appear ring on the exit, the suck and sparkles at the entry,
      the pair still drawn, `teleport.mp3` loaded after the transit tick; `out/parity/program/G6/verify1/window/`). GAMEPLAY on 43 of the 45 fireball-source levels (2-07 and 2-28 have a budget of 0). **Won by routes: 2-06 (570) and 4-14 (354)**;
      2-03 re-authored (911). Its deviations are the row "Step 135's recorded deviations".
- [x] **G8. A shot through a fire agent's flame becomes a fireball** (`ETHCallback_fire_agent` #254-265
      `burnProjectile`); the port's shot flies on. 20 levels. **A blocker since step 115**: no port route for 2-16
      (the ledge shadows the wall and the pit barrels from the fire agent's zone), 2-17 (its second wall 87 u from the
      zone, `door_lift_1111` between) and 2-19 (108 u), nor for 2-10 and 2-11 (B2). The search was direct ignition and
      one hop through barrels where the level places them; relays through barrels moved first were not searched.
      *Wave 1*: **4-28 is not won** (step 127, deviation 1): step 125's route was refused three times under R1 and no
      new route was found - the yellow key's only carrier is the knight, which walks only toward x 22 / 172 and
      out-pushes the player at about 8 px/s, and every point within 26 px of the keyhole lies inside
      `antiportal_1940`'s 192 px. NOT proven unwinnable (seven such findings in that step were later won; the blind
      spot untried: portal pairs placed at different times and places, floors reached only through one). The
      original's own way needs G8 or the minion burn (the row below). And **2-31's route passes two shots through
      `fire_agent_963`'s flame** (13.8 and 2.2 px; step 127, deviation 6), which the original snuffs (`fire.json`
      `_burn_projectile_not_built`): 2-31 is won in the port but not as the original plays it, and G8's step
      re-verifies it (unless another way is found, it goes back to a finding). *Built in step 134*: a shot still in
      flight within 32 u of a flame is deleted where it first comes within reach and a fireball goes on along its
      heading, killMainCharacter clear (`Portals::Flame`, `Game::AddBurnedShot`, the fire diamond's conversion now
      through it). GAMEPLAY on the 20 fire-agent levels. **Won by routes: 2-10, 2-11, 2-16, 2-17, 2-19, 4-28**; 2-23
      re-authored, and **2-31 re-authored** in the step's fix round (298 -> 248): its old kill shot is burned, and the
      new one is fired from under the agent's pocket, which the player reaches over `seesaw_915` in flight from a
      portal outside the ring (his walk up the plank tips it left-down; the port's hinge solver and G10). Tight margins
      for G4 and G10 to move are listed there.
- [x] **The fire agent burns a minion** (new, step 125 deviation 2 and step 127 deviation 1; beside G8): the
      original's fire agent burns a burnable minion in its flame, which the port does not wire (`minions.json`
      `_burnable_breakable_not_wired`). With G8 it is the original's way through 4-28 (the knight that carries the
      yellow key). Whichever step builds either re-attacks 4-28, blind spot included, and re-adds its route. *Built in
      step 134*: a minion whose entity point comes within 32 u of a flame is set alight and destroyed on the next tick
      (`Fire::State::minionsAlight`, `Minions::Burn`); a minion is no character, so the burn is all a flame does to
      it. GAMEPLAY on 3-19, 3-23 and 4-28. **4-28 won** (861): the knight, portalled beside the flame, walks into it,
      and the player takes the key it drops.
- [x] **G9. The reflector catches within `size.x` (32) and its leg is `min(|caster - agent|, remaining)`**
      (`ETHCallback_reflect_agent` L69063 #56-84, #148-196); the port's 16 is a guess and it mirrors the whole rest.
      *Built in step 157 (wave 3b-2, local, not pushed). The listing says the leg is `min(sqrt(squaredDistance), |casterPos - agent|)` - the WHOLE length the shot was fired, which the callback
      never decrements, not what is left of it - so that is what is built; a shot is caught by a position sample strictly within 32 x the placement's scale of the reflector (never on the tick it
      arrives, and on the tap's frame if the shooter stands within 32 of it), turned on the plane's axis (a plane-less placement: the side it came from), spawned at the reflector's point, and turned by
      one reflector at most (reflect 0 on the shot a reflector adds: "two reflectors in a row" pins that the second does not catch it). `docs/ethanon-formats.md` section 21 has the citations.
      GAMEPLAY on the 19 levels with a reflector; 12 routes now fail (R23's six and 1-12, 1-21, 2-11, 2-16, 2-25, 3-14: each has a shot passing 16 to 32 units from a reflector or a
      reflected leg longer than the old one): no route file was touched, and **the routes are re-authored by the wave's route phase after G1b**. The rows are G9-1 to G9-7, under R23's.*
      *Routes that lean on the port's 16 (wave 1)*: 1-12 (the landing over the post is `reflect_agent_675`'s), 1-21
      (the high portal (230, 142) 24 u from `reflect_agent_826`, the bounce's shot leaving 26 u from it), 1-23 (the
      shot to (150, 80) 29.9 u from `reflect_agent_826`) - all inside 32 (step 126) - and 3-14, 2-25 (step 127).
      *Measured in step 157: of the 86 routes exactly six have a reflected shot on this build - 1-12, 1-21, 2-11, 2-16, 2-25 and 3-14 (2-11 and 2-16 were not on this list) - and 1-23 has none: its tick-110 shot
      from (62.6, 206.4) toward (150, 80) passes 36.1 u from level22's `reflect_agent_826` at (84, 112), outside 32 (the step-126 figure of 29.9 u does not hold for the route as it is now). 2-27 has no route.*
      *Wave 3b-2's close: the six routes with a reflected shot are re-authored by step 161 (1-12 473, 1-21 498, 2-11 433, 2-16 397, 2-25 538, 3-14
      1,141); 2-27 still has no route.*
- [x] **G10. The player moves as the decode says** - `speed 3`, `SetGravityScale(2)`, velocity set outright, in the
      air `vel.x x min(1, 0.86 x fps/60)`; the port's `move_toward` numbers are Godot-era guesses. *Every route leans on
      it* (with G4); its step re-plays all 70 (86 since wave 2). *Built in step 148 (wave 3a, local, not pushed): the
      walk set outright at 144 u/s (3 x 16 ms / 33.33 ms of the original's FIXED 1/30 s physics step, carried at
      `ScriptClock::SimRatio` 2), the air brake 0.86 a tick and no steering, gravity 20 m/s^2 = 4000 u/s^2 of wall time
      with Box2D's clamp only, the 14-unit ground sensor (`char_feet.ent`), the zero-g recoil and a well's pull at the
      ratio (140 u/s a shot), the turn toward a tap, and the landing's `hitIntensity` in the original's units. GAMEPLAY on
      every level. Cross-checked on F10 (70 x fps / 30 at 17-19 fps), F13's walk (144.4 u/s), fall per frame (2.75-2.96
      px/frame^2 against the decode's 3.125, which does not depend on the frame rate) and walk-off (84.3-84.6 against
      80.8-84.9 at 30 to 24 fps). 23 of the 86 routes still win (CK: 41); **the routes are re-authored by the wave's route
      phase after PIC**. Its deviations are the rows "G10's recorded deviations" in the wave 3a list above; the verifier
      (`G10/verify1.json`, ten gates ok) wants three of them settled BEFORE the re-author: G10-5 (the standing creep, a
      confirmed regression against the library), G10-2 (the owner's ruling on the rate) and G10-1 (the world's time base). G10-5 was built
      after the re-author, in the R3 fix round (step 148's amendment); the other two stand. Commits `d33d14d`, and `e54fa9d` /
      `972cdaf` for G10-5. The routes were re-authored in step 151: 84 of 86 win; 2-30 stays unwon on G10-1, G10-2 and G10-4
      (R3-2), the rows still to settle.* *G10-1 and the mass half of G10-4 are built in step 152 (WT, local, not pushed); G10-2
      stands; the capsule's shape, the sensor and the clamp's screen are built in step 153 (CAP, local, not pushed).*
      *Wave 3b-1's close: G10-1, G10-3, G10-4, G10-5b and G10-6 are ticked; G10-2 (the rate) stands and G10-5c, G10-7 and G10-14
      stay open; the 86 routes were re-timed on the whole group in step 155 (86 of 86 win; 2-30 on a creep, R3-2, which G10-2
      decides).*
- [x] **G11. 3-32's ghost can be killed only through a rule the original lacks** (the audit's G11; KNOWN as "Order 12.2,
      THE OWNER'S TO RULE: fire against the ghost"). *Built in step 117*: a fireball passes through the ghost, a
      barrel's blast burns it, and the ghost puts the barrel back itself on every beat (the two "Order 12.2" rows and
      "3-32's ghost restocks too"). GAMEPLAY, 3-32 only. **Won by a route: 3-32**, tick 2724, three wounds by barrels
      the fire diamond's fireballs set off, the knight burned first (R4), replayed by `test_mp_routes` and in the
      window on the same tick. Its route crosses the ghost's body with two portal shots a cycle, which the port fails
      no shot on (F15 finding 2, the "Order 12.2, not built" row): the step that builds that re-authors them. Its six
      recorded deviations are open rows under **Tests** (step 117, deviations 1-6).
- [x] **Release-to-fire**: the original fires on release, if the pointer moved no more than scale(18)
      (`hasPortalRequestInput`); the port fires on press.
      *Built in step 137 (RF, wave 2's track B, `7aee28b`): a portal shot leaves on the touch's release, only if the
      touch never strayed more than scale(18) (inclusive) from where it went down, aimed at the last held point
      (`getReleasePos`) through the release tick's camera (`hasPortalRequestInput` #1-22, `TouchInputManager::update`,
      `managePortalInsertion` #16-41; footage F14, F15 and F16 fire on touch-up). GAMEPLAY on every level: a tap fires
      a tick later, a held touch fires nothing until it is let go, a drag over scale(18) fires nothing. Every route's
      shot taps moved a tick earlier, each still firing on its old tick (424 on the track, 78 more at the merge); 2-09
      and 2-32 re-authored on their old ticks; seven won on another tick by the aim's drift (at most 2.58 px; 4-16
      774 -> 764 the most). Its deviations: the row "Step 137's recorded deviations".*

**Presentation** (VERIFIED unless marked).
- [x] **P1. A chapter's last level ends on a dead "Chapter N complete" line** (only Esc leaves); the original goes to
      the chapter select, and after 4-32 opens `GameFinishedPopup`. **Built in step 119** (program wave 1, track B,
      `e1dab6e`): N, the pause's Skip and the medal screen's go-on open chapter select on the chapter's page (1-32 and
      2-32 page 1, 3-32 page 2); after 4-32, page 2 with GameFinishedPopup (the card's top-left at (162, 0) px as
      decoded, its close button at (0.92, 0.88), a 204 / 255 press tint, closed only by a press or the back key, the
      PageManager still under it). The 128-level sweep byte-identical. GAMEPLAY: none.
- [ ] **Left by step 119**: no footage of the card (on the rig with the all-gold save: 4-32, pause, Skip - a capture
      settles its placement and whether the title and arrows hide under it); `TheChapterEnds` and
      `TheGameFinishedCardComesUpAfter4_32` pin no D7 count (the chapter-end path holds by the code today); the pause's
      and the medal screen's buttons act on the press where `Button::update` acts on the release inside (pre-existing),
      and read the input directly, so a scheduled `--tap` does not reach them; a HelpPopup tapped on its own close
      button fades out tinted in the original (`Open::closeTinted` is the hook; not built); the level-end flow's own
      capture (a win on an x-32, then the go-on) can now be driven by the routes of 1-32, 2-32, 3-32 and 4-32. **For the
      owner**: the card's art reads "Like us ... on facebook!", and the Facebook button the dead-services ruling omits
      (#121) leaves a hole after "Like us" (about (556, 385) px).
- [x] **P2. The win cutscenes**: 4-01's rocket launch, 4-19's landing, 2-32's dragon knight, with `end_delay` (5000 /
      14500 / 6600 ms) and their music; the port holds a flat 1400 ms. **Built in step 123** (program wave 1, track B,
      `e2277b8`): the medal comes at the level's end_delay after the door (`GameStateController` #143-175; the lost
      screen keeps 1400), measured 5.000 / 14.500 / 6.600 s (F17 5.10 / 14.65 s on a 1-1.5 % long clock).
      `ETHCallback_rocket_launch`: the door up (0, -390) by smoothBeginning(t / 7600), the bridge (-32, 0) by
      smoothBeginning(t / 3800) unclamped, `startEarthquake(3, 0)` (59 calls, +-2.95 px), the rocket's track; 7 of 7
      F17 points read. `ETHCallback_rocket_landing`: the door's originalPos (256, 400) by smoothBeginning(t / 12500),
      the eclipse up to rest at 610 px, the spiral to 0.1, the key x 0.9 a frame, `back_home`. The dragon knight: added
      once 2-32 is finished (a death too), falls, holds 800 ms, walks in 4.85 s after the door at 51 px/s, with the
      door's suck, sparkles and sound. **GAMEPLAY: the door-to-medal time on those three levels only.** All three are
      won by routes on the merged build (step 128: 4-01 351, 4-19 581, 2-32 2577); the step captured each cutscene
      after a DEV-placed win (`--place`), and none has been captured after a route's win.
- [ ] **Left by step 123** (the verifier's severities): **(MEDIUM) the medal is saved when the screen comes up, not on
      the door's frame** (`checkGameEnd` #75 `writeScore`): R, N or closing the window in the 5-14.5 s beat loses it - on
      4-19 for 14.5 s, and a mouse-only player cannot press anything then. Fix: `m_scores.Record` and `Save` into the
      `goals.completed` branch, with a layer case. **(MEDIUM) 4-19's key sparkles are not shrunk with the key**: its
      two `sparkles.bmp` systems glow on at its point after the picture is gone (from f72), and in real play they would
      hang in the doorway for all 14.5 s and under the medal. Fix: scale the key's emitters as `ETHEntity::Scale` does
      (`ETHEntity.cpp:591-599`). **(LOW) a dark 2-3 px line along the eclipse's quad top edge** on 4-19 from about 6 s
      (y 545-547 px; absent from F17 r5 #630): the sampler wraps `eclipse.png`'s opaque bottom row onto its top - the
      same REPEAT cause as the claw's lines (the row "2-32's claw draws a line" under Tests). (LOW) 4-19's camera x
      follows the rocket more tightly than F17 (the porthole at 636-643 px against F17's 650 -> 671, mean -17 px;
      INFERRED: the original's follow lags more). The shake re-arms every sixth tick on the port's clock where whole
      milliseconds give seven (the PORT-WIDE clock row). The knight's add and first update are INFERRED to the tick; the
      record's ~11.5 s for the knight's walk read in Box2D m/s wants its derivation (`ETHPhysicsController.cpp:150`
      multiplies by the global scale). **New on the merged build**: 4-19's landing now plays under step 118's 6 s red-key
      fade with the wall broken, which step 123's worktree lacked; it has not been compared with F17 since the merge.
      *The two MEDIUM items are done by step 141 (MS, wave 2's track C, `6462ba9`): the medal is saved on the door's
      tick, where `checkGameEnd` #75 `writeScore` -> `setScore` #139 saves it (`recordWin` in the `goals.completed`
      branch; the screen still comes at the delay), and 4-19's key sparkles are scaled with the key by each
      `Scale(0.9)`, as `ETHEntity::Scale` scales them (`ETHEntity.cpp:591-599`): 0.5227 u drawn 30 frames after the
      door, and no glow in the doorway or under the medal from f660. GAMEPLAY: the medal is saved on the door's tick
      on every level, from that tick's counts. The LOW items stay open; the eclipse's line is the REPEAT cause step
      131's clamp removes, not re-measured on the landing.*
- [x] **P3. 20 of 22 fire agents wear the anti-portal agent's face** (`ETHCallback_fire_agent` #1-7 sets
      `fire_agent.png` every update). **Built in step 120** (program wave 1, track B, `105ca1c`): every fire agent is
      drawn from `fire_agent.png` - 22 on 20 levels; on the stills 19 of 19 readings fit. GAMEPLAY: none.
- [x] **P4. Blue and red keys, keyholes and doors draw yellow** (`setKeyColor` L90240 swaps to `_blue`/`_red`
      sprites); the non-teleportable crate's red pulse (`manageNonTeleportable`, 600 ms) is not drawn. **Built in step
      120**: 24 blue or red pieces on 9 levels drawn from their `_blue` / `_red` files, white (9 of 9 readings fit); 23
      blinking crates on 15 levels, not the audit's 22 (level7a's `crate_small_no_emissive_826` spells `telepotable`),
      colour (1, 0.3, 0.3) <-> white and emissive (0.5, 0, 0) <-> 0 on 600 ms legs (24 of 24 readings fit). On the
      merged build G5 (step 114) keeps 2-08's two crates and 2-09's `crate_no_emissive_1004` standing at f420, so the
      blink shows there (step 128's sweep attribution). (Step 120's fifth finding, 4-19's halo drawn white, was B5's:
      step 118 draws it red and stretched.)
- [ ] **Found by step 120, decoded, not built**: the fire agent's pulse (`ETHCallback_fire_agent` #282-300,
      `bounce(1.02 -> 0.98, 300 ms)`; the port draws it still); the teleportable crate's own blink (`crateCallback`
      #38-121: an unburnt `_no_emissive` crate's emissive (0.2, 0.2, 0.2) -> 0 over 600 ms, one with the uint `blink`
      (1 - ambient) -> 0 over 200 ms - on the dark levels it lights every crate a portal carries); a burning crate's
      fade (`manageBurnable` #41-117: `SetColor(bias)` and `SetAlpha(bias)`, bias 1 - burnTime / 1000, then
      `DeleteEntity`; the port takes a burnt crate away whole at 1000 ms); and the four static crates' blink clock
      (2-09's, 2-11's, 2-23's and 4-12's crates are `static="1"`, so the original blinks them only while their bucket
      is drawn, 00_order 8.12c; the port from the level's first tick - the bucket-limited clock, not decodable from
      stills).
      *Step 169 (CMP-9) builds the fire agent's pulse (its picture, its light's range and offset and its halo) and the teleportable `_no_emissive` crate's emissive (0.2 -> 0 on
      600 ms legs, `crateCallback` #70-93); the `blink`-flag branch (#95-121, 4-19's two crates; CMP9-5), the burning crate's fade and the static crates' bucket clock stay open
      here, so the row stays open.*
- [x] **P5. SpaceSky** (spiral, planet, eclipse) on the 19 space levels. KNOWN only as a cause. **Built in step 122**
      (program wave 1, track B, `691d6de`), as `SpaceSky` (Sky.angelscript L30669-31104): the space sky at the view's
      centre at its own size; the spiral at the view's bottom-centre, a square as wide as the view's diagonal, coloured
      0.7 (4-01's `alpha` 0.6) and turned -1.7 deg/s; the planet's point at the bottom-centre with linearMotion's 1 u
      bob from PI at 0.5 rad/s; the eclipse across at the centre and down its own y plus the corner's. Measured on the
      original's 57 stills: spiral (640, 720), side 1468.6 px, -1.65..-1.68 deg/s; planet x 640. 19 levels change,
      109 identical. GAMEPLAY: none.
- [ ] **Left by step 122**: the spiral turns 1.0-2.9 % under its decoded 1.7 deg/s on the stills, and Ethanon's
      whole-millisecond frame time (`ETHEngine.cpp:147`) is a candidate cause that would reach every timed motion - a
      cross-cutting frame-time question for the owner, needing the emulator's frame time measured first; the space
      pictures between ticks under a PANNING camera are pinned by no test (4-03's case never pans); the space levels'
      shadow cascades are re-recorded more often (`Shadow passes saved` 18 -> 14 on 11 levels, and 4-08's worst):
      render cost only, keeping the sky's bodiless quads out of the shadow casters is not built; the eclipse's y takes
      the rest corner's y under a shake where the original keeps the tremble (deviation 1; no 4-19 frame differs); the
      spiral's side is fixed at load (a window resized mid-level keeps it, as the original).
- [x] **P6. The sound pass**: the landing thump, keys, crates, stones, blasts and fireball impacts, the timed crystal's
      alert, the bosses' cues; `playExplosionSound` plays ONE sample chosen by its argument, and
      `test_mp_sounds.cpp:193` pins the misdecode "both".
      *Built in step 142 (P6, wave 2's track C, `90df118`), as the listing plays them: `playExplosionSound(true)` is
      `explosion_huge` alone behind `hugeExplosionSoundTimer` 150, `(false)` `explosion_small` alone behind
      `smallExplosionSoundTimer` 150 (#3 CpyVtoR4, #4 JLowZ), re-pinned; 42 new events and 4 re-pointed, each at its
      call site with its volume, speed and timer, and twelve hooks' parameters corrected. Contacts heard as their
      callbacks hear them: the landing thump above 3.5 at hit / 18, a stone's at hit / 3 above 2 (a beholder rock
      broken on the level included), a crate's at hit / 30 above 2 and `crate_dragged` on the player, a minion's;
      keys, the keyhole, the timed crystal's alert on the first tick, shock, the light wall's 800 ms switch, the fire
      diamond, the bosses' cues (the beholder's first sight, the dragon's 2000 ms, the adders' 400 ms clocks), the
      level's buttons. Measured: 47 of 47 suites (`test_mp_layer` 3,343, `test_mp_routes` 1,143, `test_mp_sounds`
      242); 70 routes won on the same ticks, 9,538 non-sound lines 0 differ; reachability 108 / 20 / 0; the 128-level
      sweep 128 of 128 byte-identical, `crystal_alert` 35 on the 20 timed levels. GAMEPLAY: none. On the merged build
      the lift's elevator sound is one per send (step 140; `lift_turned` is gone), every shot a flame burns plays its
      fireball, and a flame's burn of a minion is heard a tick late (step 144). Its 15 deviations and what it left:
      the row "Step 142's recorded deviations".*
- [x] **P7. Music**: `no_music` levels, boss tracks starting and stopping, the level's track kept in the selectors.
      **Built in step 121** (program wave 1, track B, `1656d8f`), as `Game::preLoop` #485-509: `no_music` (presence,
      `readProperties` #166-181) opens level0c, level18c, level31b and level31c silent; each world's last level opens on
      `boss` (1-32 and 2-32), the rest on `game`. `PortalMainMenu::preLoop` is the only `playMenuMusic`, so the level's
      track - or its silence - carries under the medal, the grid, chapter select and the pause (the audit's "menu music
      plays after a level" goes with it). The ghost's init and the dark dragon's summon play `boss`; the beholder's DEAD,
      the ghost's DEATH, the dragon's take-off and the dark dragon's DYING stop it. The rocket's and the landing's
      tracks came with step 123's cutscenes. Measured: the 128-level sweep logs boss 2, none 4, game 122; the
      beholder's stop on tick 2115, the dragon's on 3100; sweep 128 of 128 byte-identical. GAMEPLAY: none.
- [ ] **Left by step 121**: **for the owner** - a switch turned back on resumes `currentTrack` even where the game had
      stopped it (4-19 reached by a skip from 4-18 opens silent, but the music switch off and on in its pause plays
      4-18's track; after a boss has gone, the same brings its track back): the original's behaviour, built; the owner
      may prefer silence to stay silence. Not heard against the original (none of the 78 mp4s under `footage/` has a
      sound track). The track under the lost screen and on the chapter-end path is pinned by no test (it holds by
      construction).
- [x] **P8. Achievements**: about 60 conditions in `checkForLevelAchievements` (27 of 82 can unlock today), the saved
      set, the "Achievement unlocked!" card, the "This level is locked!" card, the AchievementsPopup. *Step 126*:
      achievement 10, "Red Agent spare" (1-10, `checkForLevelAchievements` #190-222), counts
      `'anti_portal_agent;anti_portal_agent.ent'` and dispatches while an agent is left - meaningful now agents can die;
      the step that wires the level achievements counts the live agents, not the placed ones.
      *Built in step 143 (P8, wave 2's track C, `b45c740`): `checkForLevelAchievements` (67 dispatches),
      `registerMedalAchievements` (12 chapter badges at 50, 75 and 100 %), `ETHCallback_key` (61 for any red key, 5000
      ms) and `HelpBlockController` (62 and 63, 1500 ms) - all 82 ids by their original unlock path, each rule in
      `game/data/achievements.json` with its instruction range; where the list's descriptions disagree the bytecode is
      built (20 also wants gold, 42 and 75 test no medal; 10 counts the live agents). The rules are checked on the
      door's tick, at chapter select, in the main menu's silent pass (and a named `--level`'s start), on the key's
      pick and on the blocks' release. The saved set is `achievements.json` beside `scores.json`, with `saveList`'s
      list and hash (the formula gives the original's own saved 48140). The cards are ETHFramework's Notificator
      ported: bottom centre, elastic in, faded out, 3400 ms, with the unlock's sound; the locked card 1500 ms with no
      icon; its music callbacks set 0.15 under a card and 0.6 or 0.7 after. The pause's AchievementsPopup is built on
      the 59 levels an achievement names. Measured: 70 routes on the same ticks (12 unlock); 48 of 48 suites, 16,812
      checks; the f420 sweep 127 of 128 identical to step 142's (4-20 by 61's card: its red key is taken on tick 1);
      reachability 108 / 20 / 0; re-verified after the power loss and the folder's rename. On the merged build the
      same twelve routes unlock the same ids, and achievement 13 reads the lift door's latched status (step 144).
      GAMEPLAY: none.*
- [ ] **Left by step 143** (for the owner): the seven stealth achievements (24, 26, 28, 32, 42, 66, 76) unlocked
      whenever the minions all stood, because sight was not built (3-02's route unlocked 24 and 4-30's 76; CLOSED by steps 158-159); the
      minion-count achievements waited on kill paths (a stone's crush and an explosion's burn: BUILT by step 159; there is no sword), and 13 reads the lift doors and 60 the wells' laps by reading only (no route reaches them); the pause's
      button on the 69 levels no achievement names opens nothing (his ruling, kept); two of the original's on-screen
      strings ('Achievement unlocked! ', 'This level is locked!') are committed in `game/data/achievements.json` and
      two suites while `docs/permission.md` is a stub - extract them into `out/data/achievements.json` with
      `tools/asbc/achievements.py`, or rule that short UI strings may be committed; `mergeOtherAchievementFiles` and
      the analytics line are not built; no footage of the original's cards or popup exists, so their placement is
      checked against the decode only and the music under a card by its values, not heard.
- [ ] **Smaller**: sound switches not saved; the log erased at each launch (*both built in step 124*, program wave
      1, track B, `cc8be2e`: both switches saved on each toggle and read at the start as the original does, in
      `audio.json` beside `scores.json`; the previous run's log kept as `magicportals.prev.log`; and
      `tools\parity\play_build.bat` builds `build-play\` without validation for playing. GAMEPLAY: none); tutorial
      helpers (1-21's camera hint, the tap-away hands - *the hands built in step 146*: a release strictly within 30 u
      of a hand, off the walk pads, takes it with its `smoke_vanish`, read from the hands' own touch record, so 1-27's
      two go at its route's lift touches); the beholder's death explosion (*built in step 146*: DEAD's scattered
      `small_explosion_no_light` and its `explode` at x2); `fire32` (*built in step 146*, on the character and on a
      burned minion; a burnable crate's not drawn), `antiportal_explosion` (*built in step 126*), `carranca_destroy`
      (*built in step 146*, drawn wherever the port takes a carranca, which no level's play does),
      `small_explosion_no_light` (*built in step 146*, the three bosses'); the closing lift door
      crushes nothing (*built in step 133*: it kills the player it closes on, the one character); `death_area`
      kills only the player in a 16 px box (*built in step 136*: it kills on its whole polygon, level5's 60 x 16 and
      level25a's 52 x 16, and the player is the only character, `isCharacter`); no teleport while a platform sits in
      a portal (*built in step 136*: `hasMovingPlatformInAnyPortal`, nothing through a placed pair while a
      `moving_platform`, `lift` or `lift.ent` reaches within 5.6 u of any placed portal by its box with the corners
      cut; the entry waits and goes when it has gone; static pairs never ask; 29 bodies on 24 levels);
      stale notes (`end_delay` - *corrected in step 123*, `Camera.hpp:6-10`, `entity_roles.json`'s cosmetic `arrows`
      (its `red_key_state` a role of its own since step 118, and its `rocket_launch` / `_landing` note rewritten in step
      123); `Turrets.hpp:17-22`'s and `diamonds.json`'s `_fireball_note` were corrected in step 114). And, from step
      124: `--saves`'s `--help` line stops mid-sentence ("...without changing the player's", `game/main.cpp:416-419`) -
      the next step that edits `main.cpp` finishes it; and a capture without `--saves` now reads the owner's
      `%APPDATA%\Magic Portals\audio.json` (step 124, deviation 2), so a capture that presses `sound` or `music` must
      pass `--saves` - the hermetic sweep pattern is `out/parity/program/merge_w1/capture_merge_w1.sh` (every run given
      `--saves`, a scratch copy of `scores.json`), not `visuals/merge_c/capture_merge_c.sh`, which passes none. And,
      from step 136 (its verifier's finding): `game/MagicPortalsLayer.cpp:5707-5708` still calls the hazards' box "the
      box that kills: the Godot project's trigger, not the shape the converter gives them"; since step 136 a death
      area kills on its polygon and its box is the polygon's bounds (drawn only in the debug box view). The next step
      that edits the layer (which relinks `MagicPortals.exe` and `test_mp.exe`) rewords it: "Hazards at their box: a
      destroyier's 16 px trigger, a death area's polygon bounds (it kills on the polygon itself, Hazards.hpp)".
      *Done in step 146, in those words.*
      *Wave 2 also built 1-21's camera hint (step 138) and 2-20's lesson (step 139), corrected `Camera.hpp:6-10` (step
      138: there is no opening hold) and `entity_roles.json`'s cosmetic `arrows` (step 139: a `handle` role of its
      own), and finished `--saves`'s `--help` line (step 143's commit). Left in the row: a burnable crate's `fire32`
      (`manageBurnable` #34-40, noted in step 146, not built), and `--drag`'s help and `ScheduleDevDrag`'s comment,
      which still describe a menu drag only (steps 138 and 144).*

**Tests.** Step 113 built both: the audit's flood is `tools/parity/reachability.py` (the port's own rules read from
`data/` and `game/data/`, verdicts pinned, ctest `parity_reachability`), and `test_mp_routes` replays every route in
`tests/routes/` (11 chapter-1 levels won by play). Each gameplay step adds a route for every level it unblocks, and a
step that builds a breaker adds its `BREAKERS` row and rewrites the pin (B1: a `barrel` row, the nine to opened).
- [ ] **No route yet for 1-07, 1-10, 1-17** (step 113 left them: 1-17 tried headless, the pocket sealed to shots
      until its crates are cleared; 1-07 and 1-10 read from their maps only). Puzzles needing authoring time, no port
      defect seen. *1-10 won by step 126's route (tick 292) and 1-07 by step 127's (180); 1-17 remains.*
- [ ] **Ruff in `tools/parity` is scoped by an `include` list** (step 113, deviation 1): a new script is not checked
      until listed. Fix: `extend-exclude` naming the old capture scripts in place of `include`.
- [ ] **`test_mp_routes` runs `OnUpdate` every tick** as the app does under `--fixed-step`, where `test_mp_layer`
      mostly does not (step 113, deviation 2); the 11-of-11 match with the window is its only evidence.
- [ ] **The reachability flood is optimistic** (step 113, deviation 3): a point, not the 20 x 44 body; no no-portal
      zones; locked and switched doors and dynamic bodies free - so 4-19, 3-13, 4-21 and 4-32 read reachable and
      are settled only by their routes (4-19's and 4-32's are won; 4-19's flood path is the row "4-19's way under the
      key's room", step 118). *And about barrels* (step 115, deviation 1): a barrel and any fire on the
      level open a wall, whether or not that fire can set the barrel off within the wall's reach - so 2-16, 2-17,
      2-19, 2-21 and 2-24 read opened with no port route (B1's row). *And about zones* (step 127, deviation 4): with no
      no-portal zone term the check was vacuous for R1, R2 and R17 - its verdicts could not move whatever a zone did
      - so their steps' evidence that no door was newly sealed is their routes and scripted checks
      (`R1/fix2/check_3_15.txt`, `check_4_14.txt`, `check_2_26.txt`, `check_2_31b.txt`, `check_2_31c.txt`). 3-13 and
      4-21 now rest on that: 3-13 is won (step 126), 4-21 is not (B7). A tools step gives the flood a zone term.
      *4-21 is won by step 133's route (tick 587). The flood also treats every lift door as open, so it cannot see
      that step's latch or crush either.*
- [x] **`--route` without `--saves` writes the player's own save** when the run goes past the level's end: say so in
      its `--help` line, or give it a throwaway saves directory (code; the next step touching `main.cpp`). Step 113's
      runs left gold for 1-02 and 1-32 in `%APPDATA%/Magic Portals/scores.json` - the owner deletes it for a fresh
      save (below). *Step 114's `--help` says it; its own window runs used a throwaway `--saves`.*
- [x] **2-06 (`level5a`) has no route: it needs G6** (VERIFIED from its geometry and flags). The door's pocket (x 300-454 above `platform_no_emissive_ent_824`)
      is sealed by `block00_no_emissive_ent_823` and `crate_no_emissive_796` on it (teleportable 0, burnable 1), up to
      the ceiling. Fire alone removes the crate, and the level's one carranca fires along y 202, under the platform:
      only a portal pair that carries its fireball (G6) reaches the crate (step 114). *Won in step 135* (tick 570): a
      portal left of the crate at (215, 32) and one in the lane at (300, 202), put there from `lift_ent_832` as it
      goes down; fireball #2 comes out of (215, 32) flying right and burns the crate (390).
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
      cannot both bring the player to the button and wait at the door. *Re-authored at the merge (step 128)*: under R1
      (step 127) the route's taps on ticks 1265 and 1290 were refused, so one tap more on tick 1238 at (226, 208) shoots
      `anti_portal_agent_ent_1095`, which dies on 1244 with its ring, as the original's player does; won on tick 1319,
      the same tick as before (15 shots).
- [x] **Routes that ride the port's self-running lifts (G2)**: 2-02 (`level1a`) and 2-07 (`level6a`) board their lift
      as it comes back on its own, and 2-09 (`level8a`) rides `lift_ent_1016` up to the button and down from it on the
      lift's own clock; G2's step re-authors all three. 2-03 (`level2a`) hops each fireball through a portal
      pair, which G6 would carry the fire through: G6's step re-authors it. *Wave 1 adds* 1-27, 1-28, 1-29, 2-18 and
      2-25 (step 127) and 3-31 (step 128: the merge's re-route rides `lift_ent_2438`, where the old route leaned on
      G1, G10 and G4); nine routes in all for G2's step. *Step 135 (G6)* re-authors 2-03 (911: a pair in the lane
      turns the fire back; no lift) and adds 2-06 (`level5a`, 570), whose player rides `lift_ent_832` down and up on
      the lift's own clock and is put in a portal as it goes down: ten routes in all for G2's step.
      *Done: step 140 (G2) re-authored the nine on the waiting lift with lift touches (1-27 660, 1-28 1088, 1-29 589,
      2-02 568, 2-07 1126, 2-09 1552, 2-18 1194, 2-25 510, 3-31 1248), and the merge (step 144) the tenth: 2-06 (575)
      sends `lift_ent_832` down and up with two touches.*
- [x] **`test_mp_dragon` NOT RUN in step 114** (Smart App Control refused it twice); the next step that links it runs
      it. (`test_mp_routes`, refused twice in the step, was relinked by its verifier and ran: 59 / 0, 18 of 18; the fix
      round's run, with 2-09, 62 checks, 0 failures, 19 of 19 won.) *Run at the merge (step 128) on the merged code:
      230 checks, 0 failures.*
- [x] **4-32's torch shot under G5's rule is not played** (step 114): the dark dragon's fireball at the torch
      (`Game.cpp:702-714`) now dies on any solid body in its way; `light_off` stands at (210, 34) beside
      `single_block_plat_no_emissive_ent_2278`, and `test_mp_darkdragon` checks only the shot's origin and aim. Static
      only (step 114's verifier, `out/parity/program/G5/verify2/lanes_and_torch.txt`): from `test_mp_darkdragon`'s
      torch-shot origin (110.89, 43.94) the 9.6 circle reaches `light_off_1689` at 99.61 u and first touches
      `single_block_plat_no_emissive_ent_2278` at 110.10 u, clear by 10.5 u. For B3's step, which must play 4-32 anyway.
      *Played in step 116* (the 4-32 route): both of the route's wound-end shots reach the torch, none meets the block -
      tick 1403 from (110.8, 42.1), spent at the lit torch's fire signal on 1431, the torch out and the light wall back;
      tick 2066 from (110.6, 46.2), spent at the unlit torch on 2102, which it relights (the wall goes 1500 ms later).
      The rule it plays is still the port's (the "Order 12.1 finding, the wound's shot" row: the original fires only at
      an unlit torch); the route does not depend on it (row "4-32's route" below).
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
- [x] **2-06's fire lane ends on the self-running lift** (step 114, deviation 4): `lift_ent_832` comes down into
      `carranca_ent_827`'s lane on its own and the first fireball goes off on it (x 192.3, tick 200), where the
      original's lift idles at its marker until tapped. `test_mp_turrets` prints this lane and pins the other 19; G2's
      step pins it.
      *Closed by step 140 (G2): the lift waits at (160, 112), and `carranca_ent_827`'s first fireball ends on the left
      wall at (8.17, 202.00) against the lane's 9.5; `test_mp_turrets` pins 20 of 20 lanes.*
- [x] **`test_mp_routes` NOT RUN in step 115** (Smart App Control refused `6090631b`, after its one relink
      `c8d1403d`, and the verifier's relink `bd6544ad`); its evidence is the window replay of all 23 routes on
      `MagicPortals.exe` `ff41bf4e`, 23 of 23 won twice with the same ticks, the 19 old ones on step 114's. The next
      step that links it runs it once; that run is the first headless replay of 2-12, 2-13, 2-14 and 4-16 (authored in
      the window), and a mismatch there re-authors them, not a regression. Until then those four are won by a route
      the window replays, not yet by one a suite replays. Nor were `test_mp_launchers`, `test_mp_boss` and
      `test_mp_start` relinked, which read `demolish.json` through the loader that now requires `blast.entities`
      (their sources unchanged; the 128-level sweep starts every level through it). *Run in step 116*
      (`2467fcfd`, first launch): 23 of 23 won on the window's ticks, 2-12 576, 2-13 500, 2-14 518 and 4-16 774 among
      them (74 checks, 0 failures); with 4-32's route, 24 of 24 (77 checks). `test_mp_launchers`, `test_mp_boss`,
      `test_mp_start` still not relinked (they now also read `fire.json`'s required `lava` block; sources unchanged).
- [ ] **The rest of `explode`'s destroy set** (step 115): a blast also sets `destroy` on a carranca (flagged breakable;
      `carrancaCallback` L85350 #128-183: `carranca_destroy.ent`, `startEarthquake(20, 0)`, `DeleteEntity` - 2-18's and
      2-23's barrels stand 54 and 56 u from one), a shock agent and a minion (burnable; `ETHCallback_shock_agent` L92534
      #294, `ETHCallback_minion` L87616 #1478) and a light wall (`ETHCallback_light_wall` L98075 #32). The port's blast
      breaks walls alone (`demolish.json` `blast._not_built`); each needs its own module's death. *Step 146 draws
      `carranca_destroy.ent` wherever the port takes a carranca (only its stone rule, the row below); the blast's
      taking stays unbuilt (step 146, deviation 3).*
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
      are off them). *Step 117 (deviation 2)*: the ghost's blast test casts the same solid-only ray; harmless for
      3-32's route (`anti_sight_wall_1430` goes with the knight, `anti_sight_wall_1438` is left of any blast that can
      reach the ghost, and at the route's three blast ticks no shot, minion or bodied portal lies on the ray).
- [x] **2-24's kept-out barrel is untried** (step 115 (d)): a barrel left out of the seven-barrel chain to weigh
      `button_1144` while the rest break the walls; if it wins, 2-24 leaves B4.
      *Ticked as moot, not as built: since step 139 2-24 is won with B4's handle (722), whose route moves barrels off
      the chain and sends one onto `button_1144`; a kept-out barrel with no handle was never tried, and no row needs
      it now.*

- [ ] **4-32's route rides port-only numbers** (step 116): the summon 7 s after the light (the "Order 12.1 finding,
      the summon's clock" row: the original's 4 s once 3 s of level time have passed), the portal shot's 600 u/s (G4:
      each barrel's pair is timed on a 20-tick flight to the portal beside it) and the player's walk (G10: the dodges
      and the seam at x 255.4 the player settles in). The step whose rule moves one re-authors the route's ticks. Not
      port-only: its two portals stand 152 u and 358 u from the antiportal's centre (outside R1's 144) and each barrel
      goes through its pair before the fireball reaches the portal (G6 would otherwise carry the fireball to the
      barrel's side); the wound-end torch shots do not touch it (it never goes left of x 192 before the key, and the
      torch is lit at the end under either rule, so its last tap puts it out and brings the light wall back).
      *Step 128 replays it with R1 built: won on tick 3421, as before.* *Step 135 replays it with G6 built: no
      fireball goes through either pair (the route trace marks none `through`); won on 3421.*
- [ ] **The restock's pictures** (step 116; 3-32's too since step 117, deviation 5, at `barrel_spawn` and
      `fire_diamond_spawn`, with `playGhostItemAppearSound`): `addEntityAtSpawnPoint` adds `ghost_utility_spawn.ent` (explosion.JPG and
      onda.png bursts) at `barrel_spawn` with each barrel, and sets the new barrel's `minEmissive` to (1, 1, 1), the
      floor of `ETHCallback_barrel_bomb`'s blink (#1-30), which the port draws for no barrel. The dying dragon's
      `ghost_utility_spawn.ent` beside the key is the same entity. One effects-runtime row and the blink, for the
      effects' owner. *The effects-runtime row is built in step 146*: `ghost_utility_spawn.ent` on the spawn marker's
      own point and depth before each barrel and fire diamond a boss puts back, at the ghost's escorts and at the dark
      dragon's key_pos. The barrel's `minEmissive` blink is still not drawn.
      *Step 169 (CMP-9) draws the blink: a barrel the dragon puts back runs the barrel's triangle from the tick it is made, at the floor (1, 1, 1)
      (`ARestockedBarrelBlinksFromItsOwnBirth`, 4-32).*
- [ ] **The lava is filed as a hazard, and kills nobody in the original** (step 116, finding):
      `ETHBeginContactCallback_lava` (L68416) only burns and sets `explode`; `entity_roles.json` puts `lava` under
      `hazard`, so `Hazards` gives level31c's `lava_1140` a 16 px box that kills the player. Unreachable: the player
      dies of the level's edge at y 352 (hazards.json bounds), 46 u above the lava's top at 398. No change made; the next
      step touching `hazards.json` can add `lava` to `not_hazard_names`, as `enemy_killer` is.
- [x] **3-32's ghost restocks too** (step 116, for the ghost's step): `ETHCallback_ghost` ins 594-630 put a
      `barrel_bomb_low_emissive.ent` (named `barrel_bomb`) back at `barrel_spawn` after every shot, and ins 632-667 and
      1047-1083 a `fire_diamond.ent` at `fire_diamond_spawn`. `sim/Restock` is the mechanism, general (a rule of five
      names and a template node; `Found` by exact name; `Add` builds the template's body at the marker); the ghost's
      rule and its template check against `barrel_bomb_low_emissive.ent` are that step's, and its fire diamond's
      restock stays Game's own lines (a diamond is not a body). *Not a wiring job* (step 116's verifier): level31b
      places no `barrel_bomb` and no explosive node - only `barrel_spawn_1435`, `fire_diamond_spawn_1434` and
      `fire_diamond_ent_1256` - so `Add` has no template node for the ghost's barrel. That step first gives
      `sim/Restock` a build source that needs no placed node (a port JSON template for
      `barrel_bomb_low_emissive.ent` with its `.ent` provenance, or a converter-emitted entity table), and has a
      refused restock say so (the row "A refused restock is silent" below). *Built in step 117*: the port's template
      table (`game/data/templates.json`, read through `Tscn::Parse`; `barrel_bomb_low_emissive.ent` is byte-identical
      to `barrel_bomb.ent`), the barrel on EVERY SHOOTING beat - not after every shot: the red line gates the shot
      alone (#536, #556 jump to #593; F15 finding 3) - and RAGE_MODE's fire diamond on every update while the player
      lives (#1013-1083; the row above's "RAGE_MODE's own restock"). GAMEPLAY, 3-32 only.
- [x] **A refused restock is silent** (step 116, deviation 1, and its verifier): the restock is built from a placed
      node, not from the `.ent` (the simulation reads none; 4-32's `barrel_bomb_2225` was checked field by field
      against `barrel_bomb.ent`, friction 0.7 aside, which no rigid body reads). A caller whose entity no node carries
      cannot be built: `Restock::Add` adds nothing and fills its `why`, and its one caller (`Game.cpp:723-727`) drops
      it, so the level quietly goes without its weapon. Print `why` once, or refuse an unbuildable rule when the
      level loads. 4-32 never meets it. *Closed in step 117*: `Restock::State` counts refusals and keeps the first
      `why`; the layer says it once a level, and the route trace prints it. No level refuses (both callers' templates
      exist), and no test drives one: a rule whose template no node carries would pin it.
- [ ] **`Restock::Found` counts bodies only** (step 116, deviation 2): the original's `SeekEntity` finds any entity by
      its exact name; the port's search runs over the level's bodies and what a restock added, so a search for a
      marker (no body, not in `built`) finds nothing. Every caller the port wires searches for a body; a future caller
      that searches for a marker needs it.
- [ ] **A restocked barrel starts inside 4-32's ceiling** (step 116, deviation 3, INFERRED): `barrel_spawn` stands at
      (430, 0), the ceiling's face, so each barrel is added 18.85 u inside `collision_768x128_ent_1952`; the port's
      solver puts it out below over three ticks (y 14.9, 18.0, 18.8) and it rests on block 2219 at (429.9, 63.4)
      24 ticks after the restock. That the original's Box2D does the same is inferred, not measured: no footage shows
      a restock. Needs a capture of the original's 4-32 fight.
- [ ] **The lava's contact is an overlap** (step 116, deviation 4): a body's collider against the lava's box grown by
      `fire.json` `lava.margin_px` 1.0 (`_guess`, as `demolish.json`'s stone contact), not Box2D's begin-contact. The
      barrel's going off a tick after its touch is the row "A fireball's touch and its barrel's blast are a tick
      apart" above (Fire's flag-and-poll split).
- [ ] **The lava's burn branch is untested** (step 116's verifier): `Fire::Tick` burns a burnable that touches the lava
      (`ETHBeginContactCallback_lava` #1-19), and no test covers it; no level puts a burnable where it can reach the
      lava. Entering `test_mp_fire`'s lava barrel as a `Burnable` too and checking `burned` would.

- [ ] **The ghost's body, which the blast's ray meets, is inferred** (step 117, deviation 1): 38.4 x 41.7 about its point
      plus (1.5, -2.1) - `ghost.ent`'s 128 x 139 at (5, -7) times the add's scale 0.3, the body built when the entity is
      added (`ETHPhysicsSimulator.cpp:72-73`) and never rebuilt by `SetScale` (`ETHEntity.cpp:601-608`). Read from the
      engine source, not measured: no footage shows a barrel burning the ghost. F15 finding 2 read the box at 128 x 139;
      3-32's route's blasts lie outside both.
- [ ] **3-32's barrel is built from the port's template table, not the `.ent`** (step 117, deviation 3):
      `game/data/templates.json`'s node for `barrel_bomb_low_emissive.ent`, checked against the `.ent` (byte-identical
      to `barrel_bomb.ent`) field by field - a claim about two files, as step 116's. Friction 0.7 and density 1 are read
      for no rigid body. A converter-emitted entity table would carry it from the file itself.
- [ ] **The restocked barrel's picture slot is unmeasured** (step 117, deviation 4): `Slot::Effect` at the spawn's
      round(z) 0; where the original draws an added z-0 entity among level31b's z-0 art is not measured.
- [ ] **The ghost's body fails no portal shot in the port** (step 117, deviation 5; F15 finding 2, the "Order 12.2, not
      built" row in section 2): the original's `computePortalFinalPos` (#30, #136-175) fails a portal ray across
      `ghost.ent`'s static box. 3-32's route shoots two portals a cycle across it (at (590, 190) and above the diamond,
      from x 389 and 495); under F15's 128 x 139 every shot from the floor across x 429-557 fails, so the step that
      builds it re-authors the route (F15 section 7's pushed barrel is the original player's way).
- [ ] **The blast's reach has rounded corners; the original's is an octagon** (step 117, deviation 6, its verifier):
      `EntityGrabber` builds its `Shape` with `edgePlanes` true (#149-150), four 45-degree planes through the box's
      corners, so the original reaches where both overshoots a and b are below 80 and (a + b) / sqrt 2 is too;
      `Ghost::BlastReaches` (and `Fire::Blast` for every entity) takes the Euclidean distance, up to 6.6 px short in the
      corner bands (70 past one side and 40 past the other reaches in the original, not in the port). Fix: that test in
      `BlastReaches`, a test point there, and `test_mp_ghost.cpp:671`'s "a sphere against a box, not a box" reworded.
      Behaviour: its own step. 3-32's route's blasts lie past one side only, unaffected.
- [x] **22 suites include step 117's changed headers and have not run on its code** (step 117's verifier): boss,
      bounce, demolish, diamonds, dragon, fields, fire, hazards, hinge, keys, launchers, minions, movers, play, shot,
      start, statics, timed, torch, turrets, wells, zerog (`test_mp_*`), and every suite links the changed
      `MagicPortalsSim`. All 36 affected objects compile with 0 warnings (the verifier's compile-only check); the next
      build of any of them relinks it, so up to 22 first launches Smart App Control may refuse - no ctest loop.
      `test_mp_diamonds`, `test_mp_minions`, `test_mp_start` and `test_mp_dragon` open level31b: run those first. Once
      every suite is relinked, `ghost.json`'s unread `body.radius_px` (kept for them) goes.
      *Step 118* ran `test_mp_keys` (169) and `test_mp_start` (753) on its code, which carries step 117's: 20 of the
      22 remain. It adds its own: 28 suites include a header it changed (boss, bounce, camera, darkdragon, demolish,
      diamonds, dragon, fields, fire, geometry, ghost, hazards, hinge, launchers, lighting, minions, motion, movers,
      play, shot, sky, sprites, statics, timed, torch, turrets, wells, zerog), and all 41 it did not run link its sim
      (its record, THE SUITES): the next build relinks all 41, up to 41 first launches.
      *Wave 1's merge (step 128) relinked every suite, and 30 of the 47 ran on the merged code with 0 failures (among
      them boss, demolish, dragon, fields, ghost, minions, movers, play, shot, statics, timed, torch, turrets and zerog).
      **17 suites have not run on the merged code**: camera, geometry, scores, start, hazards, fire, hinge, bounce,
      wells, darkdragon, keys, diamonds, launchers, lighting, sky, tiers and select (`test_mp_*`) - each refused by
      Smart App Control on its first launch and again after its one relink (step 128, deviation 1). Partial cover: the
      track that changed each ran it on its own code (track A fire, darkdragon, keys; B sky, scores; C wells; the other
      ten no track changed), their code merged without a conflict, the sweep started all 128 levels, and
      `test_mp_camera`'s table matches `earthquake.json`, the enum and `kKeys` row by row (the verifier's static check).
      Next: the next step that links them runs each once and records its count; no ctest loop.*
      *Closed by step 129 (T.2).* The suites are one runner, `test_mp.exe` (`6c5f2626`). Its one link ran on its first
      launch, with 0 refusals. It ran every suite on the merged code, once in one process and once under ctest (48 of
      48 passed, 0 Not Run), with 0 failures and the same count in both, 47 of 47. The 17: camera 214, geometry 64,
      scores 63, start 753, hazards 43, fire 94, hinge 28, bounce 189, wells 321, darkdragon 259, keys 169, diamonds
      134, launchers 105, lighting 548, sky 630, tiers 527, select 112. Every suite has now run on step 117's and step
      118's code, and the other 30 are at step 128's counts. What the row left, `ghost.json`'s unread
      `body.radius_px`, is the next row.
- [ ] **`ghost.json`'s unread `body.radius_px` can go** (step 117 kept it, and its `_radius_px_not_read` note, "because
      a binary built before that step still requires the key"). Since step 129 every suite is one binary built after
      step 117 and run on the merged code, and `MagicPortals.exe` `755b8a9d` is step 128's. Removing the key is a data
      change: the step that does it runs `test_mp_ghost` and whichever suites read the table, once each.
- [ ] **`test_mp` alone counts every exit 77 as skipped, marked or not** (step 129, deviation 3; its verifier's LOW
      finding). ctest gives `SKIP_RETURN_CODE 77` only to the suites whose `add_mp_test` line carries `SKIPS_WITH_77`,
      so an unmarked suite's 77 is FAILED under ctest. In the one-process tally it is "skipped", and the run exits 0.
      The path was reviewed, not exercised: on the owner's machine every data path exists, so no suite returns 77.
      Fix (code, the next step that touches `tests/test_mp.cpp`): hand the runner the marks (a list generated from
      `add_mp_test`) or report an unmarked 77 as FAILED. A fresh-clone run, without `out/`, exercises the path. Until
      then ctest's verdict is the one that counts.

- [x] **Step 118's sweep and window replay NOT RUN** (Smart App Control refused `MagicPortals.exe` `57630962`, `e78d335b`
      and `f5aacc1f`; both relinks spent). *Run by its verifier* on `cbdb4004` (a link-only relink): the sweep 127 of 128
      byte-identical to `G11/after/sweep`, 4-19 its halo red and stretched (`B5/verify1/after/sweep`); the route won on
      581, the key 129 and the wall 489, as the suite. Its fix round replayed the route again on the final data, frame
      for frame the same (`B5/fix1/window18c`).
- [ ] **The red key's wall comes 0.15-0.25 s early against F17** (step 118, deviation 4): the port's rule on the
      original's frames breaks it on #212 (13.601 s); F17 has it whole on #214 and dust on #216 (6.15-6.25 s after the
      fade's start). The uint milliseconds account for about 55 ms; the wall's callback runs in the state's own frame
      (fix round 1: they share bucket (3, 1) and the state is ahead in it, inferred from the engine source), so about
      0.1-0.2 s is not explained. Nor is it a clamp or smoothing of the original's frame time (its verifier): the
      script's `getLastFrameElapsedTime` is the uint of the engine's delta, truncated to whole milliseconds
      (`ETHEngine.cpp:147`) and capped only at 1000 ms (gs2d `android/main.cpp:180-181`).
- [ ] **4-19's key bobs across its state's bucket edge** (step 118, deviation 6, found by its fix round; INFERRED, not
      built): the state and the key stand on y 128, the top edge of bucket row 1 at 128 u. The original's free key bobs
      1.5 u about that point and is in row 0 while above it, so a key taken on the upper half of its swing is seen only
      once it trails back to y 128 or below - at once from below (the route), never while a carrier holds it above. The
      port reads the key at its node (the pick-up-on-the-node divergence, `motions.json` `_about`), so its trigger is
      always the taking tick. Building the key's bob into the sim would settle it.
- [ ] **Step 118's other recorded deviations** (1, 2 and 5; its 3 was withdrawn by the fix round: the wall and the
      state share bucket (3, 1), the state ahead in it). (1) `red_key_state` and `breakable_wall_2756` are static, so
      the original runs their callbacks only while their bucket is visible (00_order 8.12c); the port's fade clock runs
      every tick, as its other static clocks do (section 2's bucket-visibility rows: "Order 10.3, gameplay rows",
      "Order 13.1, the off-screen kill"). (2) The fade adds the tick's 16.67 ms where the original adds a frame's
      whole milliseconds (section 2's PORT-WIDE clock row). (5) A key a boss drops (`key.ent`, on 3-32 and 4-32) is
      never the state's candidate: `seekEntityFromBucket` matches the name exactly. No level places both.
- [ ] **4-19's way under the key's room** (step 118, finding): the reachability flood reaches the rocket through the
      3-unit gap between `crate_2494` and `block00_no_emissive_ent_2504`, under the room at y 256-260 and up past x 512,
      inside the fall bound, so 4-19 reads `reachable` with the wall standing. The player would have to move `crate_2494`
      first; whether it can, in the port or the original, is not tried. If it can, 4-19 has a way round the key. The
      check itself (its verifier): flooding with the player's 20 x 44 box rather than a point would not pass a 3-u slit.
- [ ] **4-19's route rides the port's shot and lift door** (step 118): its third to fifth taps put a shot under a door that
      the crate on the button holds up for 15 ticks, and turns back as soon as it is let go (step 109's row); the step that
      builds G4 or the door's open-from-shut rule re-authors them. The original's route (F17 r5) opened a portal inside
      a crate, which the port's shot, failing on a crate, cannot. *Step 133 built the door's open-from-shut rule: the
      crate's 15-tick press now raises `door_lift_2498` all the way (to 25.6) and it falls from there on 113 (98
      before, from 65); the route wins on its old tick 581 unchanged. The shot's half (G4) stays.*
- [ ] **Step 133's recorded deviations** (the lift door, 1-6). (1) The crush's point is the player's capsule centre,
      22 over the feet (18.1 since step 153, the original's), where the original's entity point stands 18.1 over; under a falling door either is far below
      the 70.4 the callback asks. (2) A contact begins on the first tick of an overlap, with no Box2D skin. (3) The
      door's clock is the fixed step in milliseconds (a float step summed in double), where the original adds whole
      milliseconds a frame (section 2's PORT-WIDE clock row). (4) `awakeEntitiesAround` is not modelled. (5) The door's
      callback runs off screen, where the original's static callback runs only while its bucket is drawn (00_order
      8.12c; section 2's bucket-visibility rows, "Order 10.3, gameplay rows" and "Order 13.1, the off-screen kill").
      The latch makes it matter more: a door pressed off screen now starts a whole rise. **3-15's `door_lift_1677`**
      (a knight on `button_1656`, tick 290) is fully raised by f420 with its bottom above the view (y -6.4): the
      original's callback may stop there and leave it up, where the port lowers it on tick 443; no route depends on
      it, and any door that leaves the view while raised is the same case. 4-21's door stays on screen through its
      press (camera (424, 128)). (6) `test_mp_play`'s floor stays 171 (it runs 223): the next step that relinks the
      runner anyway raises it. *Step 146 relinked the runner and did not raise it (it runs 234 there): (6) is still
      open.*
- [ ] **2-18's re-authored kill threads about 0.4 px** (step 133, left for the owner): `level17a.json`'s shot toward
      (54, 72) passes about 0.4 px under the raised `door_lift_1146`'s bottom (56.6) and about 0.3 px over the barrel's
      left corner (120.7, 65.0), 21.6 px from the agent (R17's 22.4). Deterministic, won on 1965, and the geometry's (no
      line clears the door's corner, the barrel's corner and the agent's 22.4 each by more than about 0.5 px), but maybe
      not how the original is played; its reflector (`reflect_agent_1180`) is unused. Whichever step next touches the
      shot (G4), the door or the player's jump (G10) re-verifies 2-18 first.
      *Re-authored twice since: by step 140 for the waiting lift (1194), and by the merge (step 144, 1191), where step
      133's raised door stopped step 140's kill: the kill passes under the door's bottom and over the barrel's corner
      within 22.4 of the agent, released on one tick only (1120); aim y 71.4-72.6 and x 54-58 all win, a release a
      tick later fails on the door - a 1.2 px window in the aim (step 144's deviation 2). The row's question, whether
      this is the original's way (its reflector unused), stands.*
- [x] **`ALiftDoorLeavesItsDustAsItOpensAndCloses` does not pin the rise** (step 133's verifier): it compares the dust
      against mix(closed, open, the watch's progress), so it passes at any rise; its print shows the closing dust at
      (430, -6.4), which is 32 - 102.4 + 64. The 102.4 itself is pinned in `test_mp_play` (`DoorsTakeTheirStrideFromTheLevel`,
      `ADoorRisesEasedAndLatches`, `ACrateOnAButtonOpensItsDoor`). The next step that relinks `test_mp.exe` anyway adds
      an absolute check that the closing dust sits at y 32 - RisePx() + 64. *Done in step 146*: the case now checks
      the dust absolutely, the opening dust at (430, 32 - 64) and the closing dust at (430, 32 - RisePx() + 64), with
      the rise 102.4.
- [ ] **Step 134's recorded deviations** (the flame's burn, G8 and the minion, 1-6). (1) The burn point is where the
      shot first enters the 32 on the tick's whole stretch (`Shot::EntersCircle`), where the original tests the shot's
      point once a frame after moving it 200 u/s; a shot grazing the circle within about 0.4 px can differ (the
      anti-portal agent's reading, step 126). (2) The agent's bucket gather (its own bucket and the four beside it,
      never the diagonals, `ETHCallback_fire_agent` #10-79) is not modelled: at the port's 256 u bucket no agent of the
      22 stands within 32 of a bucket corner; at 256 / 1.5 and 256 / 2.8125 six and twelve would, the bucket-size
      question placement.json leaves open. None of the step's six route shot burns enters in a diagonal bucket at any
      of the three sizes (`out/parity/program/G8/verify2/bucket/burn_buckets.txt`; 4-28's knight not in that check).
      (3) A flame's burn of a minion is seen a tick later, as the original's dynamic minion reads it on its next frame.
      (4) The converted fireball's launch sound (`playFireballSound`) is not played, as for the fire diamond's (the
      audio audit's row 16, the sound pass P6). (5) The flame's callback runs off screen, where the original's static
      callback runs only while its bucket is drawn (00_order 8.12c; section 2's bucket-visibility rows, "Order 10.3,
      gameplay rows" and "Order 13.1, the off-screen kill"): a shot or a knight burned off screen here may not be
      there. (6) A flame-burned minion shows no flames and makes no sound: `ETHCallback_minion` #1471's `addFlames` is
      not drawn and `burn()` #16's `playBurnSound` is not played (the "Order 12.2 deviation, no `fire32` flames on the
      burned knight" row; the audio audit's `playBurnSound` row, missing). *(6)'s flames are drawn since step 146
      (`fire32` before the blood, at 0.59375).*
      *(4) is closed by the merge (step 144): a burned shot's fireball plays `playFireballSound` on the burn's tick,
      as `burnProjectile` #54's `addFireball` #87 does. (6)'s sound half is played since step 142, a tick late (step
      144's deviation 1).*
- [ ] **Step 134's routes lean on the port where no clip shows the original** (step 134, left for the owner).
      **2-31** (`level30a.json`, 248) rests on the port's seesaw: the plank tips left-down under a player who walks up
      it from x 389 with his flight's speed (the hinge solver, and G10's walk), and its first portal, (445, 200), is 7 u
      outside `antiportal_958`'s 192. **Tight margins**, deterministic but for G4 (the shot's speed and its 24 u start)
      and G10 to move: 2-11's first portal clears the flame by 0.8 px, 2-17's portal left of the flame by 1.1 px,
      2-23's new shot by 2.8 px, 2-16's last portal by 2.7 px. 2-16, 2-17 and 2-19 teleport a barrel or a crate by a
      portal pair before the burn, which no clip shows, and 2-11's crate may be the original's G6 way (a fireball sent
      through portals). Whichever step builds G4, G6 or G10, or measures the seesaw, re-verifies these first.
      *Step 135 (G6) replays all six on their step-134 ticks and no fireball of theirs goes through a pair (the trace
      marks none `through`), so none leans on G6; 2-11's static crate is still burned by its flame's fireball.*
- [x] **A carranca's fireball flies at |dir| x 130 in the original** (step 135's finding, left for the owner;
      GAMEPLAY when built). *Built in step 149 (G4): `Turrets` keeps the node's (dirX, dirY) as written and a fireball's
      velocity is that times 130; pinned level by level in `test_mp_turrets` (`TheEightCarrancasFlyAtTheirDirsLength`);
      the other 20 carrancas, every converted shot and the dragons' fireballs are unit, 130 as before; the routes of
      2-01, 2-03, 2-07, 2-15 and 2-18 were re-authored in step 151 and win on 302, 907, 1128, 532 and 983 (2-28 and 4-13 have
      no route). Commit `012dd1a`.* *Step 155 re-timed them on the time-base build: 2-01 wins on 444, 2-03 on 907, 2-07 on 1,126, 2-15 on 499 and 2-18 on 983.* `ETHCallback_fireball` (418524..418930, #1-43) moves the fireball by
      `GetVector2('dir')` x `scaledUnitsPerSecond(speed)` every frame, and a carranca's 'dir' is its node's
      (dirX, dirY) as written (`carrancaCallback`, `addFireball` `SetVector2`), never normalised. Eight of the 28
      carrancas carry one that is not 1 (`out/parity/program/G6/carrancas.txt`): `level0a` 1.2 (2-01, 156 u/s),
      `level2a` -1.25 (2-03, 162.5), `level6a` -1.6 (2-07, 208), `level14a` -1.3 and 2 (2-15, 169 and 260), `level17a`
      1.2 (2-18, 156), `level27a` -1.2 (2-28, 156), `level12c` 0.7 (4-13, 91). The port normalises to 130
      (`turrets.json`'s direction note, corrected in step 135). Building it retimes those levels' lanes, the lane
      pins in `test_mp_turrets` and the routes of 2-01, 2-03, 2-07, 2-15 and 2-18.
- [ ] **Step 135's recorded deviations** (G6, the fireball through a pair, 1-5). (1) The exit's `active` flag is not
      modelled for a fireball (`setActive(other, false)` until nothing is in any placed portal; an inactive end sends
      nothing): a traveller entering the exit within about 10 ticks of a fireball coming out of it differs; the
      shortest carranca stride is 90 ticks and no route has one. (2) One object per portal per frame
      (`getObjectInPortal`) and the second portal's first (`doTeleporting` #37-98) are not modelled: a fireball and
      another traveller in a pair's two ends on one tick differ; no route has one. (3) *Closed by step 136*:
      `hasMovingPlatformInAnyPortal` is built for every traveller, a fireball included, and 2-06's route replays with
      nothing held. (4) The static portals' callback and the one-placement
      path carry a fireball with no once test and no turn, and are not built for it: no level places a fireball
      source with a static portal. (5) A fireball is carried after its tick's contact test, so it meets bodies from
      its next tick, 2.17 u along, as Box2D sees the original's `SetPositionXY` on its next step.
- [ ] **Step 135's routes lean on the port where no clip shows the original** (left for the owner). 2-06 (570)
      rides `lift_ent_832` on its own clock (G2) and puts a portal on the player; 4-14 (354) drifts on the port's
      zero-gravity recoil (70 u/s a tap, added); 2-03 (911) is timed on the port's 130 u/s fireball (the |dir| row).
      No clip shows a fireball go through a pair; the decode and the help popup W2P3 are the evidence.
      *2-06's route was re-authored at the merge (step 144, 575): two touches send `lift_ent_832` down and up, so it
      no longer rides the lift's own clock.*
- [ ] **Step 136's recorded deviations** (HZ, the death area and the platform gate). (1) The gate reads the port's
      platforms and lifts: G3's half swing and G2's self-running lifts, not the original's; each of those steps
      re-verifies the gate's routes (1-28 is held today). (2) The death area meets the port's player capsule, 20 x 44
      (player.json, a guess; R25; since step 153 the hull of the original's two circles, 18.2 x 36.2), not the original's character body, and Box2D's polygon skin (`b2_polygonRadius`)
      is not added: a first overlap kills, not a touch. (3) A held entry is the port's per-traveller
      entry offered again each tick; the original's `isThereAnythingInPortal` takes one object per portal per frame
      (step 135, deviation 2), which differs only with two travellers at one portal on one tick. (4) The reachability
      flood cannot see either change (the row above). (5) A gate body turned or scaled is refused by `Portals::Find`
      (the grabber's corner planes do not turn); none of the 29 is.
      *(1)'s G2 half: lifts wait and are sent since step 140, and no route is held on the merged build (1-28's route
      re-authored for the waiting lift, 1088); G3's half swing stands.*
- [ ] **1-06's route leans on the port** (step 136): the platform's swing (G3, R7), the shot's speed (G4) and the walk
      (G10). The one clip of 1-06 (F13) shows only its fall death off the spawn block, which the port's walk off it
      matches (tick 82); none shows the way through.

- [ ] **Wave 1's routes lean on the port where the original is not shown** (step 127, deviation 8; step 125, deviation
      3), each for the step that builds the row to re-verify: body-overlap traversal, where the original tests a point
      (1-27, 1-28, 1-29, 2-15, 2-18, 2-30, 2-32, 3-07, 3-12, 3-14, 3-15 - entered about 25 px off - 3-22, 4-07, 4-25);
      the wall broken by touch (11.1's rows: 3-12, 3-14); an exit 6 px into `wall_low_emissive_1688` pushed out by
      physics and a portal opened on a falling player (3-15); a portal opened on the player itself (4-18; and 4-28's
      removed route) - that the original opens it is read from the decode (its ray starts 8 units out along a
      zero-length aim and meets nothing), not measured: the owner's testing and the final comparison settle it. G1,
      G2, G3, G9, G4 and G10 have their own lists above.
- [ ] **3-19's route rests on an unmeasured hinge creep** (step 127, deviation 7): `spinning_platform_1554` is pushed
      for about 2,600 ticks and the route wins on 3207 against a win_by of 3270. The step that measures the spinning
      platform re-verifies it; if the original's platform does not yield, 3-19 becomes a finding. (Authored without B1,
      whose blast can take `breakable_wall_1556` there, it wins on the merged build with B1 on the same tick.) *Step 155 re-flew it on the time-base build (997 -> 832: the spinning plank no longer turned under the player's push, so the route was re-flown); the hinge's yield is still not measured against the original.*
- [ ] **3-13's crate** (step 126): pushed too far it slides over the 64 px gap and jams under the column (the port's
      crate and its friction are the Godot project's); the route releases it in time. Not measured against the original. *R3B FIX 1 holds that crate at 0.08, a guess (R3b-3), and re-authored 3-13: won on 1,035.*
- [x] **2-32's claw draws a line beside the dragon** (the owner's report, 2026-09-29; `out/parity/program/dragon_lines/`
      `investigation.md` and its skeptic's `check.md`, cause CONFIRMED): `dragon_claw.png`'s opaque left column (the
      arm, rows 10-40) is wrapped onto the quad's right edge by the engine's REPEAT sampler (`VulkanImage.hpp:82`'s
      default; `TextureRegistry.cpp:211` passes no address mode) - a dark line about 1.4 px wide and 87 px long, f396-f443
      on 2-32 (24-45 grey undimmed). The original clamps (`GLES2Texture.cpp:306` through SOIL's `GL_CLAMP_TO_EDGE`, and
      `:166-167`, `:375-376`). The same cause darkens 2-01's ramp (row 461: port 70.6, bottom row cleared 85.8, the
      original 87.3), the sky's first and last screen rows on 2-32 (+3.8 / -4.5 grey where the original is flat) and step
      123's eclipse edge line on 4-19; the left and right screen columns (0.65-0.71 and 0.91 of their neighbours) are
      partly this and partly an untested MSAA edge. **Needs an engine change**: a clamp-to-edge sampler beside the REPEAT
      one, chosen per material (the check's option A; the default stays REPEAT), then the port opts its sprites and
      overlay quads in - so it belongs in the pin-bump step, and it moves the outer half-texel of every texture whose
      opposite borders differ, so every byte-identical baseline is re-taken after it. Its gates are in `check.md`
      ("The fix I would build", section 3). *Step 130 (P0) moved the pin to `a733ca0`, whose `a915514` gives
      `TextureRegistry::UploadRGBA` a trailing `vk::SamplerAddressMode` (default `eRepeat`): a clamp chosen per
      texture where the check's option A proposed one per material, and it does the same job. The engine side is
      done. The pin-bump step carried no clamp, because opting in is a game change and would have broken its
      byte-identity proof, so "it belongs in the pin-bump step" no longer holds. What is left is a port step: the port
      opts its sprites and overlay quads in, and re-takes every byte-identical baseline. Caution: `ReplaceRGBA`
      rebuilds a texture with the defaults, so a replaced texture goes back to repeat.*
      *Closed by step 131 (P1), 2026-09-30*: engine `710bffc` (`MaterialComponent::clampToEdge` and
      `ScreenOverlay::Quad::clampToEdge`, off by default; a material's default keeps each texture's own sampler, so an
      image uploaded clamped stays clamped - pinned in `test_materials`, 442 checks on Linux) and the port opting in on
      every sprite and overlay quad. Measured: the claw strip gone on f396-f443 (the same exe draws the unchanged claw
      and the arm-cleared claw byte-identical over 450 frames); 2-01's row 461 71.5 -> 86.8 grey (the original 88.5);
      2-32's sky edge rows flat (+2.05 / -1.78 -> -0.03 / -0.04). The dark dragon's normal map clamps too, as the
      original's does (deviation 8, accepted as the original).
- [ ] **Step 131's leftovers** (P1, clamp-to-edge; GAMEPLAY: none). `test_materials` has run only as a Linux binary
      (GCC 13.3 under WSL: 442 checks, 0 failures, its parent 419; and in the engine's ctest, 57 of 57, before
      `710bffc` was pushed); its MSVC binary was refused twice by Smart App Control and has never run on Windows
      (deviation 1). 4-32's dark dragon is lit through a normal map whose borders differ, so it changes at its spike
      tips (7,412 px on 92 of 348 sampled frames, measured on a scratch build); gate 5 was decided as the original,
      which clamps its normal maps too, and the exact change from step 130 is not measured (deviation 8). One
      `VkSampler` more per image read both ways (deviation 6).
- [ ] **Step 137's recorded deviations** (RF, release-to-fire; GAMEPLAY). (1) The corner controls (restart, pause,
      clear) still act on the press and spend the touch, where `Button::update` acts on the release: a touch that goes
      down on one and is dragged off it within scale(18) presses it in the port and fires a shot in the original. (2)
      The touch is not followed under the pause, whose buttons read the mouse on the press. (3) A down and an up on
      one tick fire: a click shorter than a tick, which the original's per-frame `KeyStateManager` would never see as
      a hit (INFERRED). (4) A route's aim drifts with the camera while the finger is down - at most 2.58 px on the
      track, 6.80 px on the merged build (2-11) - by design. Left: a touch held through a popup fires on its release,
      by the code only (no case pins it); the bound equals scale(18) only while `view.json`'s `height_px` stays 256
      (`_inferred`), which `test_mp_layer`'s `unitPx` pins.
- [ ] **Step 138's recorded deviations** (CD, the camera drag; GAMEPLAY behind `camera.json` `drag.drag_pan_enabled`).
      The follow is still the port's `_guess` lag, not `Follower(1000, 100)`; the drag's view is clamped at once,
      where the original's virtual camera can sit one frame's pan past an edge; 1-21's hand runs its round on level
      ticks (the original's FrameTimer on engine time, through a pause) and the static arrow's callback from tick 1;
      no touch is read under the pause or after a door or a death; (6) a released shot is aimed through the camera
      before the release tick's follow, where the original follows once first (`addController` order, L6011 and
      L6026); (7) both walk keys held do not spend the hold (the key sum against `ScreenPad::arrowKeysMove`'s per-key
      rule; the on-screen pads are right). Its (3), `shouldLockCamera` reading nothing until the handle, is closed by
      step 139. Left: `--drag`'s `--help` and `ScheduleDevDrag`'s comment still describe a menu drag only (step 144
      too); the decoded line "move the camera around" is committed in `game/data/camera.json`, as `ui.json`'s "Part "
      and "new!" are (the owner's call, with step 143's two strings).
- [ ] **Step 139's recorded deviations** (AR, the draggable handle; GAMEPLAY). One pointer; a static follower moves
      every tick (bucket visibility is not modelled); the handle is read only in play; the follow lands half a tick
      after the drag; a click shorter than a tick is no hit; 2-20's small crate falls off the big one in the port (a
      physics divergence older than the step; the route burns it first). Its (7), 2-24's lift door rising through the
      platform in the track's unlatched tree, is moot on the merged build, where 2-24 wins with the latch (722). Left:
      two claims by the code only, for the next step that relinks the runner - the handle works with the camera's
      switch off, and a tap on a handle fires its shot; the routes of 2-20, 2-21, 2-22 and 2-24 start their first drag
      on tick 10, inside the black opening caption (move it past about tick 90 when next touched); `main.cpp`'s
      `--help` does not mention a route's drags.
- [ ] **Step 140's recorded deviations** (G2, the waiting lift; GAMEPLAY). (1) A lift moves and blinks every tick,
      where its static callback runs only while its bucket is drawn (no route leans on it: no lift moves while wholly
      outside the view); (2) one pointer; (3) the camera a tick apart (the same at rest); (4) riders carried by
      `MoveKinematic` where the original sets the position; (5) 1 u a tick, where the original's whole-millisecond
      frames give 0.96; (6) the elevator sound latched a tick later; (7) a touch on a control or a help block over a
      lift does both, by the code only. Left: the pad exclusion (a press on a walk pad over a lift sends nothing) is
      pinned by no test - the next step that links `test_mp_layer` or `test_mp_movers` adds the case; a knight holding
      at a waypoint stays in the air after its floor breaks until its hold ends (a 3-10 draft, about 830 ticks; not
      measured on the original); 2-02's window of good send ticks (about 90-229 mod 240) is reasoned from the lanes,
      not measured. **3-23 is not won** (a finding, NOT proven unwinnable): both keys are carried by knights, the
      yellow keyhole is seen only from the right floor, and nothing in the track's tree took a key from a living
      knight; step 134's burn of a minion by a flame, on the same build since the merge, is the mechanism its route
      waits on (no recorded attempt with it yet). 1-27's two hand hints, which the lift touches left standing, are
      taken since step 146.
- [ ] **Step 141's recorded deviations** (MS, the medal at the door; GAMEPLAY: when the medal is saved, from that
      tick's counts). The key's scalings follow the tick clock, as its picture does; the first `Scale(0.9)` on the
      door's tick is step 123's inference; a portal a shot opens after the door no longer counts, and the other
      routes' old past-door medals were not measured; one log line per win. Found, not changed:
      `ghost_appear_effect`'s scale is applied after its quads are placed (a frame behind, x 1.005). The `--hold` and
      `--route` help lines it left stale were rewritten in step 142.
- [ ] **Step 142's recorded deviations** (P6, the sound pass; GAMEPLAY: none), 1-15: a timed crystal's alert on the
      level's first tick (the original's static crystal inits when its bucket is first processed); a burn latched once
      per burnable (the original's sounds again on a re-burn); a shock agent a fireball destroys strikes on the same
      tick (the original's on its next frame); the dark dragon's torch shot sounds whenever the port fires it (step
      99's divergence); the beholder's first sight on the port's camera; an argument volume caps at 1 (the original's
      0.99); no sample-state bleed; a crate's contacts are its solid ones; the adders' clock on the port's tick; a
      spike sounds on after the kill, as the original's does; a log line per latched sound (the shipped game's too);
      an edge `Game::BeforeStep` makes is latched a tick late (2-32's dragon's take-off, claw crushes and fireballs, a
      carranca's fireball; 16.7 ms); a crate's begin with a moving body takes the other where the step left it; a
      beholder rock's break is heard on the port's break tick; a thrown stone on a destroyier plays
      `playStoneHitSound(1)` alone. Left: `boss_spike_hit` and `minion_landed` are pinned to their hook, not their
      moment; the minions' spotted, question and shot sounds and a stone crushing a minion (built by steps 158 and 159) and 3-20's placed stone
      on its destroyier wait on their mechanisms; the log's `sound` line counts latches, not plays.
- [ ] **Step 143's recorded deviations** (P8, achievements; GAMEPLAY: none). The game's time is the level's age on the
      tick, a pause's time excluded; a named `--level` runs the main menu's silent pass at its start; the Notificator
      runs on the layer's tick (strict comparisons kept); the music's volume is kept per track (INFERRED); a card's
      log line names the id; no card without Enhanced's list (the unlock is still made and saved); a save shared
      across runs carries an unlock into the next (so every sweep launch takes a fresh copy). Its (3), a lift door's
      status read from `opening` and `progress`, is closed by the merge: `LevelWorld::LiftDoorsShut` reads `status ==
      Shut` (step 144).
- [ ] **Step 144's recorded deviations** (the merge). (1) A minion a flame burns is heard a tick late: `burn()` plays
      `playBurnSound` on the alight frame, and the port latches `burned` when Minions takes it, 16.7 ms later (the
      record says the knight's flames stay undrawn; step 146 draws them since). (2) 2-18's kill is released on one
      tick only (1120): the window is in the aim (1.2 px), not the tick (the row "2-18's re-authored kill"). Its
      verifier's low note (`merge_w2/verify1.json`): `tools/parity/reachability.py`'s `FREE_ROLES['lift']` text still
      says "a lift runs between its two markers", where a lift waits at its first marker since step 140 - text, no
      verdict moves; a tools step rewords it.
- [x] **The screen's left and right columns draw dark** (found with the claw's lines, 2026-09-30; `dragon_lines/check.md`):
      the leftmost column about 0.65x its neighbour, the rightmost about 0.91x, from a level's first frame; the original
      is flat. Not the wrap (clamping leaves them): one untested model is a sky 1279.69 px wide over 1280 with 4x MSAA
      showing the black clear at the edges. Next: its own step, measured against the library frames.
      *Closed by step 145 (EC), 2026-10-01*: the model holds, on the 44 levels that draw `dark_sky.png` (1-32, all of
      chapter 2, 3-13, 4-20 to 4-22, 4-25 and 4-27 to 4-32), which has no fullhd twin: 455 u across a 455.11 u view,
      0.156 px short of each side, so 4x MSAA left one sample of each edge pixel the black clear (rows of sky alone at
      0.76 of their neighbour); the 84 others matched the original already. The port now draws such a sky's picture a
      second time just behind it, out to the frame's pixel edges by the pixel-centre rule (`sky.json` `pixel_cover`;
      `Sky::PixelCover`), its uv stretched so no texel moves. GAMEPLAY: none. Measured on the f420 sweep: the 84 others
      byte-identical; of the 44, 41 change and only in columns 0 and 1279 (38,648 px); medians L 0.817 -> 1.002 and
      R 0.838 -> 0.986 (the original 1.002 / 0.988); edge rows equal to the original's 48.1% -> 97.4% (left) and
      32.2% -> 96.5% (right); flat in play from the first visible frame, through the camera's pan and the trembles (the
      verifier's frames). Left: 2-05's right edge, the row below.
- [ ] **Step 145's recorded deviations** (EC, the screen's edge columns). (1) **2-05's right edge moved away from the
      original.** 166 edge rows of the 41 changed levels did, 161 of them 2-05's column 1279, y 421-586: there
      `bar4_contrast.png` (its right edge at 455 u) ends at 1279.69 px, covering pixel 1279's centre but not its x 0.875
      sample. That sample was the black clear, as black as the wall's edge texels, and is now the sky's cover: a mean of
      (0.8, 0.8, 1.0) -> (8.9, 14.6, 16.7) against the original's (1.0, 1.2, 1.3), the right edge's ratio 0.974 -> 1.049
      (the original's 0.964). The same wall draws so on any level whose sky covers the frame, as any quad edge does
      against the sky (the general MSAA blend, step 102's note). The other 5 rows are one or two 5/6/5 steps at a
      picture's edge (2-04, 2-19, 2-25 and 2-32's two). Next was its own step (step 165, PX: partly closed, below) since it touches the 84 levels' frames:
      the pixel-centre rule for every level picture at the frame's edge (a quad whose edge lies in the frame's outer
      half pixel drawn out to it, or given a cover like the sky's); gates: 2-05's column 1279, rows 421-586, back to
      (0-1, 0-1, 0-1), and the 84 levels' 566 scattered edge rows above 12 (1-21 and 1-23 right, 4-01 left and 4-09
      right among them).
      *Partly closed by step 165 (PX, wave 4b), 2026-10-11*: **2-05 only.** (Commit `7eae0e4`, pushed in `f240af9..7eae0e4`. The row stays `[ ]`: 2,047 edge rows remain (PX-4) and the final comparison did not attribute them, row CMP-R1.) The pixel-centre rule is built: a strip of a
      picture's own material over the gap its edge leaves in the frame's outer pixel (`edge_cover.json`;
      `EdgeCover::StripsOf`, `MagicPortalsLayer::syncEdgeCovers`), from a pool made before any other entity so the
      renderer's tie order does not move (the first build, made on demand, moved 133 px outside the ring on five levels),
      and its complement (a bar's inward edge in the outer pixel: a strip, or a cull of a picture that covers no pixel
      centre of the frame). Gate 1 is met: 2-05's column 1279, rows 421-586, (8.92, 14.63, 16.66) -> (0.98, 1.16, 1.29)
      against the original's (1.04, 1.16, 1.34), 164 of 166 rows equal. The f420 sweep is byte-identical to the baseline
      on 127 of 128 levels (2-05's 211 px in column 1279 aside), 0 px outside the ring on all 128, and 0 px outside it on
      every frame of the moves taken (the second verification's 2,161, the third's 240 of 2-24). **The second gate is NOT
      met**: edge rows above 12 fall 777 -> 614 on the left and right columns (2,210 -> 2,047 on all four sides), and the
      whole fall is 2-05's right column; the 84 levels' 572 rows, the ones named here (1-21 and 1-23 right, 4-01 left,
      4-09 right) among them, did not move: their inner pixels equal the original's and the edge column is one 5/6/5 step
      apart, so they are not a quad edge in the band. GAMEPLAY: none (128 of 128 routes win on the census's ticks; 57
      suites, 32,314 checks; the level walk 134 / 1,460 / 1,100 / peak 109 / 0 failures; reachability 108 / 20 / 0). A pan
      and a shake flat through the move is PARTIAL (the third verification read the shipped build's frames against the
      rule, not against the original's own footage). Left, as rows PX-1 to PX-8 (the block before "Blockers"): the 2,047 rows (398 with the
      blend's signature), the slivers, 1-12's right columns, the window sizes; the final comparison groups them by cause.
      (2) `sky.json`'s two `_` notes were reworded after the implementer's sweep: *settled by the
      verification*, whose sweep on the file as it is matched the implementer's 128 of 128.
- [ ] **Step 146's recorded deviations** (UI, the smaller pictures the original adds; GAMEPLAY: none). (1) The
      implementer's sweep and probes ran on `564f657e`, a link without the hands' own touch record (Smart App Control
      refused the final code's): *settled by the verification*, whose sweep on the final binary `2cc4a50e` is 128 of
      128 byte-identical to `UI/after/sweep/`, and whose 1-27 capture takes both hands at the route's lift touches
      (`hand_tap_ent_1160` on 46, `hand_tap_ent_1195` on 521). (2) The shock agent's pulse runs on the level's age,
      which the port's pause holds, where the original's getLastFrameElapsedTime runs through a pause (as the
      antiportal's blink reads it); only the paused frames' share can tell. (3) `carranca_destroy` is drawn wherever
      the port takes a carranca, which no level's play does; the original's taker, explode()'s grab, is not built (the
      row "The rest of `explode`'s destroy set"). (4) The dark dragon's explode is at its last DYING waypoint, (28,
      334.4), below the view, as decoded; not checked against footage. (5) **`fire32` on a dead character burns where it
      died, where the original's sinks with the body**: on 2-10, F13's flames' centroid descends from 567 to 668 px and
      leaves the screen by +0.92 s, the port's stays near (540, 550) px for about 1.45 s (its onset and size match
      F13's, its life the decode's).
      The cause is outside the step: `checkGameLost` (#47-58) hides the character and calls Scale(0.05) on it, so the
      original's shrunken body falls off the platform's edge and `ETHCallback_fire32` follows it; the port keeps the
      hidden body full size on the platform. Next: a step that builds that Scale(0.05) (after the death, so no win can
      change; it checks the lost screen's timing), then re-measures 2-10's flames against F13 by time. (6) The hand's
      reach test and smoke read the swing a tick behind: `tapHandsAway` reads where the last frame drew the hand, where
      `ETHCallback_hand_tap` moves it first (`linearMotion` #1-14); at most one tick of a 4 u, 2.09 s swing, about 0.2
      u. Optional fix: advance the swing to the tick before the test (a behaviour change). Also left by the step: 3-32's
      black knight's own burn was not seen in view (off its route's camera; the same picture on the level's escort and
      on 4-28's knight was), and `test_mp_play`'s floor (step 133's (6)).

**Before the owner's testing:** delete `%APPDATA%/Magic Portals/scores.json` for a fresh save - it holds gold for
1-01, 1-02 and 1-32, two of them from step 113's automated runs. *Wave 1*: step 123's three cutscene captures ran
without `--saves` and wrote 4-01 and 4-19 bronze and 2-32 gold into that file; exactly those three entries were
removed the same night (md5 `40bfef3a` since, unchanged through both waves' sweeps, which ran on scratch copies; at
wave 2's close the folder holds `scores.json` alone). *Wave 2*: since step 143 an unlock writes `achievements.json`
beside `scores.json` (the original's `saveList` on every unlock). On this save the first launch makes it - the main
menu's silent pass, or a named `--level`'s start, unlocks what the saved medals have earned, achievements 45, 6 and
14, with no card - and a later launch leaves it alone until something new unlocks. A switch pressed writes
`audio.json` (step 124): a fresh save deletes all three. Since
step 141 the medal is saved on the door's tick on every level, so quitting in 4-01's, 4-19's or 2-32's cutscene no
longer loses it, and a run that reaches a door writes its medal into the player's own save when it beats the saved one
(or the level has none), unless it is given `--saves`. In play since wave 2: a shot leaves when the mouse button comes
up (step 137); a drag pans the camera (step
138; `game/data/camera.json` `drag.drag_pan_enabled` false gives back his 2026-09-11 ruling); a handle is dragged
(step 139); a press inside a lift sends it (step 140). In play since wave 3a (local, not pushed): the script clock's 16 ms floor, the player's decoded movement (a walk of 144 u/s, no
air steering and a brake of 0.86 a tick, a fall of 4000 u/s^2, a 14-unit ground sensor, the zero-g recoil at 140 u/s, a turn
toward a tap, and standing still on a slope), the portal shot's decoded flight (about three times slower than before; seven
levels' fireballs at their carranca's |dir| x 130) and every body's picture and the camera at the tick's own place. In play since step 152 (WT, local, not pushed):
every body falls at the original's pace (the world's 2000 u/s^2 where it was 980), a minion walks 62.4 u/s (was 39) and a 2-32 knight 81.6 (51), a gravity well pulls every body
and not the player alone, and every body weighs its own `.ent`'s weight (a barrel 4 times the player, a big crate 5, a stone 11); the decoded frictions and restitution are NOT yet in
the game (G10-14, WT-1). In play since wave 3b-1's later steps (local, not pushed): the player is a capsule 18.2 x 36.2 standing 3.9 units lower, with a ground sensor 18 below him (step 153); every moving platform on 11 levels and the three patrolling agents swing twice as far at half the rate (step 154); the crates of 3-13, 3-03, 3-05 and 4-09 are pushed at 1.89 u/tick, 3-12's pile does not move until touched and a body a portal takes away wakes what lay on it (R3B FIX 1). 2-30 is won only on a stone's creep: the designed leap dies at the shipped air brake (R3-2). In play since wave 3b-2 (pushed, `origin/main` `53d8db2`): a tap's shot
flies to where the tap-time ray ends and opens its portal there or fails there, and only a tap that reaches the hourglass restarts the 400 ms (step 156); a reflector catches a
shot within 32 units and sends it on from its own point (step 157); the knights see (a 120 degree cone in their own row, a ray, `!` and `?`, blind on the six darkest levels
until a torch is lit), shoot and kill the player or a portal, a touch caps their hold at 1000 ms, and a barrel's blast and a stone's crush kill them (steps 158-159); a rolling
stone that hits the player fast enough kills him (step 160); and the seven stealth achievements are real. 86 of 128 levels are won by a replayed route. There is no play build: `build-play\` went with `w1b` and wave
2 made none; run `tools\parity\play_build.bat` in `C:\Users\icvet\Desktop\Magic-Portals-Enhanced` to build one from
`main` (a fresh link, whose first launch Smart App Control may refuse once).

**The owner's calls** (gathered here; each decided as the original does unless he says otherwise): the grant in
`docs/permission.md` (gates handing a build to anyone else); camera dragging (his 2026-09-11 ruling "no dragging"
against the decode's 2x-swipe pan on 127 of 128 levels); R24/R25; step 110's fire-signal orbit and the put-out test at
the ball; the port-wide floored clock and the picture a tick behind; 12.1's summon clock; 11.3's death before the door;
10.2's pickup rule and 10.3's carriers; 11.1's four; Q.2's spawn point; 13.1's off-screen kill; the zero-g recoil
speed; the pause's Achievements button on every level; 4-02's easter-egg popup; target platforms. *Added by wave 1*:
the hole the omitted Facebook button leaves in GameFinishedPopup's art (step 119); a music switch turned back on
resuming the track the game had stopped, as the original does (step 121); whether the original's timed motions run
1-3 % slow on whole-millisecond frames, which would reach every `unitsPerSecond` motion (step 122).
*Added by wave 2*: camera dragging is built as the original behind `game/data/camera.json` `drag.drag_pan_enabled`
(step 138; false restores his ruling, pinned bit for bit), so his call is now that switch; short decoded UI strings
committed under `game/data/` - "move the camera around" (step 138), 'Achievement unlocked! ' and 'This level is
locked!' (step 143) - while `docs/permission.md` is a stub; the seven stealth achievements that unlock while sight is
not built (step 143); his ruling on the pause's Achievements button, kept on the 69 levels where it opens nothing
(step 143); a carranca's fireball at its dir's length x 130 (step 135's finding; built in step 149); and the routes
that lean on the port where no clip shows the original (the Tests rows on the routes' leans).
*Added by wave 3a*: the rate the program matches (`clock.json` `matched_hz` 60; the 144 u/s walk, the 4000 u/s^2 fall, the 140 u/s
recoil, the 0.86 brake, the 3.2-unit shot and every route's timing rest on it; no clip shows 60 Hz: G10-2); whether the smoothing
above 80 Hz stays (`game/data/pictures.json`, `smoothing.above_hz`, `_guess`; 0 draws at the tick's place at every rate: PIC-1);
whether wave 3a may be pushed with `test_mp_routes` red on 1-11 and 2-30 (R3-8); the rulings that gate 2-30 (G10-1, G10-2,
G10-4); and whether a person would play 2-32 ahead of the dragon's camera, as its route does (R3-3).
*Added by step 152 (WT)*: whether the engine step on the friction solve (G10-14) and the one on restitution (WT-1) are queued with the Penumbra port's engine work, since the decoded
frictions are not in the game until they are done; what to look for in play, now that every body has its own weight - a stone or barrel landing on the player and level30's two big crates (WT-2, WT-3);
and the push question it asked (whether waves 3a and 3b-1 may be pushed with `test_mp_routes` red on 24 of 86 routes) is answered by wave 3b-1's close: the routes are green, and the question is now R3-8's.
*Added by wave 3b-1's close*: whether waves 3a and 3b-1 are pushed (17 local commits, 18 with the close) with 86 of 86 routes won
and every suite green but the last independent verification failed one gate, 2-30 (R3-2, R3-8), or 2-30 is settled first, by one of
three ways: G10-2 lower, the stone's decoded friction, or the creep accepted until the engine's friction step; whether
`crate_low_emissive`'s 0.08 stays until that step (R3b-3); whether a person would play 2-32 ahead of the dragon's camera (R3-3); and
whether the engine's friction solve and restitution at the touch (G10-14, WT-1) are queued with the Penumbra port's engine work.
*Added by wave 3b-2's close*: the push question is answered - waves 3a, 3b-1 and 3b-2 are pushed (`origin/main` `53d8db2`), 86 of 86 routes won, 2-30 on its creep (R3-2) still the owner's;
whether `destroyier.ent` still kills the player or only despawns a thrown stone, now that a stone's own kill is built (STK-8: what he saw near a launcher's drop); STK-9's step before
anything is shipped (the stone's kill reads a free fall a little low); the rig checks that decide whether G1a's blocked-way rule over-blocks (does a knight lifted by a portal walk: 4-28,
4-29) and whether the player can shove a holding knight (3-07, 3-22) (R3c-4, R3c-2, R3c-3); the sleeping-body wake after a breakable wall's destruction (3-10, R3c-5); and the grant in
`docs/permission.md`, still its placeholder. The seven stealth achievements are real now: step 143's call is closed by steps 158-159.
*Added by wave 4a's close*: waves 3b-2 and 4a are pushed (`origin/main` `8fe8374`), all 128 levels are won by a route and every suite is green; STK-8 is now also the question
of 1-24's door (its route drops onto it over `destroyier_ent_1051`'s box, and the reachability check does not cover the box: R4a-4); what to look for in play now that a stone kills
from a drop of about 16 units and crushes a knight from about 3.3 (STK9: 3-12, 1-09, 1-23, 1-24 to 1-26) and that a body asleep on something you break, burn or move falls at once
(WAKE: 3-10's knight, 2-23, 2-24); the levels whose routes rest on a port behaviour no clip shows (1-19 and 1-20: the stone outlives its wall; 1-22: a creeping stone; 4-10 and 4-12:
the wall-slide; 2-29: the stone's timing; 3-10: the knight whose wall breaks); the grant in `docs/permission.md`, still its placeholder; and the repository's going public, which needs
his explicit word.
*Added by wave 4b's close*: wave 4b is pushed (`origin/main` `7eae0e4`; PX, the pixel-centre rule, GAMEPLAY none) and the final comparison has run (26 `match`, 44
`minor`, 58 `differs`; 52 of the 58 by one cause, CMP-1, decoded and unbuilt); the halo scale (CMP-8: `halo_brightness_scale` 0.6 is a guess; one global scale, per-entity
scales, or leave); whether the engine's friction and restitution steps are queued (CMP-11: five levels' crates and barrels creep or rest elsewhere, and every route that
pushes a crate rests on it); R24, now also 3-08's lone guard (CMP-17); whether the original's two clocks (its HUD 0.5-0.9 s ahead of its world) are modelled (CMP-low a);
the engine's contact margin (CAP-2: the player and crates at rest 1.6-1.8 px low, a 2 px offset on 15 levels; it would move every route's rest heights); what to look for
in play now that the comparison names the differences a player could see: the player and the exit arch lit on chapters 2-4 (CMP-1), a plank or a seesaw standing where it
was placed (2-31, 3-19, 3-20, 4-31: CMP-2), the knight beside 4-29's gem that the original never spawns (CMP-6), 3-05's first knight arriving early (CMP-5) and 1-30's
pile toppling (CMP-4); the grant in `docs/permission.md`, still its placeholder; and the repository's going public, which needs his explicit word.
*Added by wave 4c-1's close*: wave 4c-1 is pushed after the ruling R4c1-1 (the five drawing groups CMP-1, CMP-3, CMP-7, CMP-9 and CMP-10, GAMEPLAY
none): the fire agent's pulse is accepted on all 20 fire-agent levels (10 are outside CMP-9's named 32; the original pulses every fire agent,
`ETHCallback_fire_agent` #282-300 has no branch before it), ruled in R4c1-1 and open to the owner's veto (a revert, a re-sweep and a relink of both executables); the help block's face tone, 1 to 3
grey of 255 under the original's (CMP10-1: one calibration experiment on a second alpha-blended picture, not a retune of the block); what to look for in play now that these are
drawn as the original draws them: the player and the exit arch dark on chapters 2-4 and 4-01 (CMP-1), pictures at one depth stacked as the original stacks them (CMP-3), 4-20's
red wash from the key's own light (CMP-7), barrels, cracked walls and crates breathing in brightness and the fire agents' small pulse (CMP-9), and the help block translucent
and breathing (CMP-10); the grant in `docs/permission.md`, still its placeholder; and the repository's going public, which needs his explicit word.
*Added by wave 4c-2's close*: wave 4c-2 is pushed (`origin/main` `45f6c05`; GAMEPLAY changed in steps 171-175, all 128 routes re-flown and won, every suite green): 1-20's achievement 48
("Be smarter I", 8 portals or fewer) is no longer unlocked by any route, because a stone now dies at the first wall it breaks and the second wall costs a second stone - whether 8 portals is
possible under the 75 u/s gate, or the original's players reach the second stone more cheaply, is yours or a clip's to say (R4c2-1); one clip of a stone rolling and falling into a breakable
wall would settle the gate's unit, the "larger of two readings" rule for 3-13's wall and 1-22's two-portal solution together (WALL-3, WALL-5, R4c2-2); 4-21's route stands on a 2-unit aim band that
moves with G10-2's 60 Hz (R4c2-3); what to look for in play now that these are built as the original builds them: **3-09 with two knights and 4-29 with one** (CMP-6), **2-01's smaller, lighter
crate** (CMP-16), **4-02's astronaut standing on his slab** (CMP-15), **2-31's seesaw 31.8 px down and left of where the level places it, and the planks and bars of 2-28, 3-15, 3-19, 3-20, 4-11
and 4-31 a few units off** (CMP-2), and **stones that must be fast to break a wall and die with the first one**: 1-19, 1-20, 1-22, 2-29, 3-10, 3-12 (drop the stone on the slab from above), 3-14
(the crate on both buttons), 4-21 (the stone lifted into the wall from under it) and the thrown stones of 1-24 and 1-25; the grant in `docs/permission.md`, still its placeholder; and the
repository's going public, which needs his explicit word.
