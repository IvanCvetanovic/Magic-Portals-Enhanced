# Magic Portals parity — backlog

State at 2026-09-16 (first written 2026-09-15). What remains to make the **Supersonic port** (`Desktop/Supersonic-Engine/games/magicportals`)
match the original, in the order it should be done. Every spec named here lives under
`Magic-Portals-Remake/out/parity/specs/` (gitignored, local only — re-creatable from the reference library).

How to work on any item: `docs/parity.md` (rig, captures), `tools/parity/` (emu, capture_original, capture_screens,
markers, locate, compare, frames, port_build.bat). Port captures: Bash only,
`build/games/magicportals/MagicPortals.exe --level <name> --window 1280x720 --fixed-step --frames N --screenshot <png>`.
Windows Smart App Control sometimes blocks a freshly linked exe ("Not Run" / "Permission denied" / "An Application Control
policy has blocked this file"): delete the exe and relink. Touching the source is not enough when the rebuild produces the
same bytes. **Every blocked launch shows Ivan a Windows notification: never loop relink/ctest retries** — run the
suites a change touches once each, and report what stayed blocked.

---

## 0. Housekeeping left from this session

- [x] Merge branch `ui2-screens` into `main` — done 2026-09-16 (e6a3cca, pushed): its steps became 50-54, ctest 118/118,
      the worktree and branch removed. See the planning doc's step 54 merge note.
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
- [ ] Godot runtime in MPR (`game/scripts`, `project.godot`) is no longer the target. Ivan allowed deleting it, but the
      port still reads MPR `game/data/*.json` and the converter's `out/` — move those (or re-point the port) first.

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

- [ ] **Step 0** — engine `--screenshot-every N` (every temporal check depends on it).
- [ ] System 1 colour transfer — largely done by lighting E1/G2; re-check its remaining rows.
- [ ] **System 2 particles**: emitter origin at the entity position (torch flame 13.4 u too low), spin sign
      (counter-clockwise ~108°/s), alpha, depth (particles 2.5 layers in front of their sprite), square quads,
      following moving owners; AM_MODULATE blend (engine has alpha + additive only).
- [ ] **System 3 player**: start frame 8 (facing right; `startFrame` is a dead field), walk 10.8 fps, space-suit sheet on
      the 19 zero-g levels, eye glow; 2.8 u rest-height difference is a body/collider question.
- [ ] System 4 antiportals: drawn 64×scale u (not a flat 128 u), red tint; note the refusal radius (16×scale in the port
      vs decoded 64×scale) is an owner gameplay call.
- [ ] **System 5 actors with sheets**: minions/knights (63 spawns in 35 levels) have no sprites; dragon draws the whole sheet.
- [ ] **System 6 texture tiers**: hd/fullhd per file (fullhd skies are 512 u wide). Converter + port together, or sprites
      draw twice too big; the Sprite offset × scale trap applies.
- [ ] System 7 spawned effect entities: teleport effects, player-placed portal spiral, pickup/explosion effects,
      `portal_fail*`, key/keyhole effects; carranca fireballs are tan placeholder boxes today.
- [x] System 8 light halos — landed in G5 (step 49); magnitude still open under §1.
- [ ] System 9 converter-carried per-placement data: scale, sprite frames, the levels' own particle systems
      (45 placements load the wrong data).
- [ ] System 10 small scripted motions: crystal bob, arrow sway (2.5 u, 1.52 s), timed-crystal fade.
- [ ] **StaticSky** (camera-pinned sky) not ported → **34 levels show black ground** (found at G2).
- [ ] Placeholder boxes draw darker since the display-encoded scene.
- [ ] Footage to record (plan §4): F1 walk left, F2 place + enter a portal, F3 failed shot / antiportal, F4 crystal bob,
      F5 knight patrol, F6 carranca fire + barrel, F7 2-32/4-32 in-level frames, F8 antiportal pulse, F9 key pickup,
      F10 zero-g recoil.
- [ ] Cannot be known from footage (needs the bytecode decode): exact red/blue portal tint (`portalColorA/B`), shot frame
      timer, which script spawns `appear_effect.ent`.

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
