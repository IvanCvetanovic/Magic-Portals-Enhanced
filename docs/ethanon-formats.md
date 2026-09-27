<!--
  HOW THIS DOCUMENT WAS PRODUCED, AND HOW MUCH TO TRUST IT
  ────────────────────────────────────────────────────────
  Derived on 2026-07-25 by reading the MIT-licensed Ethanon engine source at
  reference/ethanon (shallow clone, HEAD c140bd3, "iOS and Android SDK versions
  cleanup", 21 Dec 2013) together with the 141 sample data files that ship with it.
  Six parallel decoders (scene / entity / ENML+config / particles / sprites+animation /
  ground-truth-from-data), each output then adversarially re-checked against the cited
  source lines by a separate pass, then reconciled.

  The verification pass DID find real errors in the first-pass notes and corrected them,
  which is the point — but it is not a proof. Treat this as a very good starting spec,
  not as gospel:

    * The engine source is the authority. Where this document and the C++ disagree,
      the C++ wins and this document gets fixed (CLAUDE.md).
    * This describes the Dec 2013 engine revision. Magic Portals may have shipped on a
      later one. §"Open questions" lists a concrete check to run once the APK is
      extracted to confirm the revision matches.
    * Items marked UNKNOWN / DERIVED / low-confidence are exactly that. Do not quietly
      promote them to fact.
    * NOTHING HERE WAS VALIDATED AGAINST A REAL MAGIC PORTALS FILE — no APK was
      available. Every claim is validated against Ethanon's own sample data only.

  Spot-checked by hand after generation: see the "Hand-verified citations" note at the
  bottom of this file.
-->

# Ethanon Engine File Formats — source-verified reference for the Godot 4 converter

**Purpose.** Everything a Python 3 converter needs to parse Ethanon `.esc` / `.ent` / `.par` / `.enml`
data and emit Godot 4 `.tscn`. Every claim below is traced to engine source or to shipped sample data.
Where the source could not settle a question, it is marked **UNKNOWN** or **DERIVED** rather than filled in.

**Snapshot.** Read-only clone of `asantee/ethanon` (MIT, © 2008–2013 Andre Santee) at commit
`c140bd360092dcdf11b242a11dee67e79fbbd025` — *"iOS and Android SDK versions cleanup"*, 21 Dec 2013.
`git rev-list --count HEAD` == 1 and `.git/shallow` exists, so **this is a depth-1 clone**: format
evolution cannot be diffed across history. All "legacy vs current" statements below are inferred from
*co-existing sample data vs. the reader/writer in this tree*, not from commits.

**Path convention.** All paths are relative to the clone root (`reference/ethanon/`). Line numbers refer
to this commit.

**Sample corpus** (the ground truth the format tables were checked against): 89 `.ent`, **11 `.esc`**
(362 scene-entity instances), 33 `.par`, 8 `.enml`, across `testbed/GeneralTests`, `testbed/ShaderTests`,
`testbed/KeyboardTests`, `toolkit/Ethanon Toolkit`, `documentation/Sample-Projects/Sample-project`.

---

## ⚠️ 2026-08-04 — READ THIS BEFORE USING ANY NUMBER IN THIS DOCUMENT

The APK arrived. A six-way adversarial audit re-checked this document against the engine
source **and, for the first time, against the real game data** (133 `.esc`, 190 `.ent` in
`reference/apk/`). 320 claims checked. The result splits cleanly in two:

**The C++ layer is excellent — trust it.** Roughly 200 line citations were resolved and
essentially none had drifted. Blocks quoted as "verbatim" are character-for-character
correct. The subtle claims (empty ENML entity names are legal, `}` is not a name
terminator, a stray backslash aborts the whole file, fixture order is alphabetical) are
right, and are the kind of thing only source-reading produces.

**The empirical layer was measured against the wrong corpus — do not trust any frequency.**
Every "shipped" / "corpus-wide" / "all N scenes" statistic in this document describes the
**11-scene MIT testbed**, not Magic Portals. The header above says so honestly (line 22),
but §§4, 5, 11 and 12 use the word *shipped* for the testbed, which reads as the game.
Reproduced exactly against the testbed — and materially wrong for the target:

| # | Doc says | Magic Portals actually | Converter consequence |
|---|---|---|---|
| 1 | `blendMode=1` (AM_ADD) **"never appears in any shipped entity"** (§5.5, §12.2) | **111 occurrences** — 11 `.ent` + 100 `.esc` tags: `portal`, `portal_static`, `antiportal`, `fireball`, `halo`, `projectile`, `spiral`, `satellite`, `shock_area`, `door_highlight`, `red_key_state`. Plus `black_halo.ent` = `4` (AM_MODULATE), absent from the doc entirely. **Chapter 1 (world 0): 2 entities, 22 instances** — `antiportal.ent` ×15, `portal_static.ent` ×7; no AM_MODULATE | **The headline defect.** Treat blendMode as always-alpha and every portal and glow renders as a flat quad — the additive glow *is* the game's visual identity. Needs `BLEND_MODE_ADD`. `BLEND_MODE_MUL` (value 4) is **not** needed for milestone 1 |
| 2 | `ZAxisDirection` is `(0,-1)`; `(0,0)` is a degenerate one-off in `physicsTest.esc` (§4.2, §12.2) | `(0,0)` in **all 133 scenes**; `(0,-1)` never occurs | Pseudo-3D z-projection is **fully disabled**. `ToScreenPos` (`engine/Util/ETHASUtil.cpp:123-126`) multiplies the z term out, so Godot position = `(x, y)` directly and `z` is depth-ordering only. Building the documented `(0,-1)` path misplaces every entity with non-zero z |
| 3 | `enableLimit` / `upperAngle` / `lowerAngle` "source-verified, never exercised by data" (§12.2) | Exercised by 14 files — `seesaw.ent`, `spinning_platform.ent`, `spinning_platform_no_emissive.ent`, `spinning_cross.ent` + 10 scenes. `enableMotor` and `motorSpeed=-5.0` too. **⚠ NOT chapter 1:** all 10 scenes are suffixed (`level10c`, `14b`, `18b`, `19b`, `21b`, `27a`, `28a`, `29a`, `30a`, `30c`) = worlds 1–3. Measured: **zero** world-0 scenes contain `revoluteJoint` and **zero** of the 49 world-0 `.ent` carry `<Joints>` | Seesaws and swinging platforms are built from joint limits — but they are **out of scope for milestone 1**. Defer the joint path entirely. When it is built: `±1.57075` = `±π/2`, so **radians**, mapping onto Godot angular limits directly |
| 4 | `shape=3` (BS_POLYGON) ↔ one file, `half_metal_crate.ent` (§5.4) | **24** `.ent` files; **1049 polygons** vs 19 compounds in the APK. **Chapter 1:** `shape="3"` on **220 of 934** inline instances (24%), and **all 220 carry `<Polygon>`** | Priority is *inverted* — polygon is the main collision path, not the exotic one, and it is required for milestone 1. `<Polygon>` nests **inside `<Collision>`**, not under `<Entity>` — see below |
| 5 | `layerDepth` is "legacy writer output", `type=5` (ET_LAYERABLE) unused (§5.2, §5.3) | `layerDepth` on **169 of 190** `.ent`; `type=5` present. **Chapter 1 (corrected): absent from all 934** inline instances | Live code path *for the game as a whole*, but **not reachable in chapter 1** — the earlier "46 of 49" measured `.ent` files that no chapter-1 level reads. Defer. When built: `ComputeDepth` returns `Max(layrableMinimumDepth, layerDepth)` and **ignores z entirely** |
| 6 | Outer `<Entity>` attributes are exhaustively `id`, `spriteFrame`, `shadowZ` (§4.3) | A fourth, **`hide`**, in `level_select0-3.esc` + `loading_screen.esc`. **In chapter 1's path:** no world-0 *level* uses it, but `level_select0.esc` — chapter 1's own menu — does | An assert-on-unknown-attribute parser hard-fails on 5 of 133 scenes, including the chapter-1 level select |
| 7 | `hdDensityValue` 2.0 / `fullHdDensityValue` **4.0**; thresholds 720 / 1080 (§8.3, §8.5) | Both density values are **2**; thresholds **480 / 600** | Using 4.0 halves the world size of the 7 `fullhd/` sprites. With 480/600 essentially every Android device lands in the FULL_HD branch |
| 8 | "The sprite namespace is deliberately flat" (§8.6); audio is "the one exception" (§8.1) | `LoadAndGetSprite` (`ETHScriptWrapper.Drawing.cpp:60-65`) does the same `resourceDirectory + file` join as audio | Flattening to basenames is right for `.ent`/`.esc` fields but **wrong** for the 157 files under `assets/sprites/` and 18 under `ETHFramework/`, which are addressed by full relative path — flattening collides |
| 9 | "An HD asset occupies the same world size as its 1x counterpart" (§8.5) | Broken by 4 of 7 `fullhd/` files: `sky`, `icy_sky`, `red_sky`, `sky_purple` are 455×256 at 1x but 1024×512 at fullhd → 512×256 virtual, a **1.125×** change | Never mix tiers per sprite. Pick one tier and derive world size from *that* tier, or four sky backgrounds are 12.5% off |

**Two corrections that are good news:**

- **There is no engine fork.** §12.1 called this undeterminable. It isn't: the 190 shipped
  `.ent` use exactly 23 `<Entity>` attributes and 33 element names, and *every one* is read
  by `ETHEntityProperties::ReadFromXMLFile` (`engine/Entity/ETHEntityProperties.cpp:226-265`)
  or is one of the three legacy fields already flagged dead. No unknown token in either
  vocabulary. Nothing in the shipped corpus needs format knowledge outside this checkout.
- **Element/attribute order never matters on read.** §4.3's child-order finding is a *writer*
  artifact. Every read is name-keyed (`ETHEntity.cpp:212-248`, `ETHSceneProperties.cpp:33-61`).
  Look children up by name; do not build a positional parser.

**Chapter-1 inventory (answers `MILESTONE_1.md` §7.2), measured 2026-08-04, CORRECTED 22:40.**

> ⚠️ **This subsection was published wrong at 22:05 and is restated here.** The first version
> claimed "49 of 66 names resolve to a file in `assets/entities/`, because the engine appends
> `.ent` when the name lacks it", and that branch B covered "236 of 934 instances (25%)".
> **Both are false.** §4.5 of this document was right all along — *nothing appends `.ent`* — and
> the real branch split is the opposite of what I published. Kept visible rather than silently
> overwritten, because the wrong version was inside a block that claims authority over the rest
> of the doc.

World 0 = `level0.esc`..`level31.esc`: **32 scenes, 934 outer `<Entity>` instances, 66 distinct
`EntityName`.**

**Chapter-1 levels are fully self-contained. `<FileName>` occurs ZERO times across all 32
scenes; all 934 instances carry an inline `<Entity>` body — branch B, 100%.** Branch A (the
`.ent` cache lookup) is *never exercised by any chapter-1 level*. **The converter does not need
to read a single `.ent` file to load chapter 1.** This is the largest single simplification
found so far, and it inverts the earlier claim that both branches were needed up front.

The 66 distinct names split as: **35** end in `.ent` and the file exists · **3** end in `.ent`
and the file is **absent** (`collision_128_sqr.ent`, `moving_platform_single.ent`,
`single_block.ent`) · **28** are written bare. Those counts are inventory only — *the name is a
label, not a reference*. Fourteen of the bare names do have a same-named `.ent` on disk
(`crate`, `crystal`, `sky`, `antiportal`, `portal_static`, …), and pairing them would be a
**bug**: the inline bodies are materially different entities. Verified — `crate.ent` is
`density=0.4 friction=0.96`, while inline `crate` occurs as both `0.4/0.96` *and*
`0.2/1.0`; `crate_small` inline uses `smaller_crate.png`; bare `sky` has at least four sprite
variants (`sky.png`, `dark_sky.png`, `icy_sky.png`, `red_sky.png`) against `sky.ent`'s one.
Resolving a bare name to its `.ent` twin silently loads the wrong physics and the wrong art.

Two rows in the table above were also derived from the wrong source — they measured the `.ent`
files rather than the inline blocks chapter 1 actually uses. **Measured over the 934 inline
bodies:** `blendMode` = `{0: 838, 2: 74, 1: 22}` — the 22 AM_ADD are `antiportal` ×15 and
`portal_static` ×7, so **row 1's chapter-1 figure stands**. `layerDepth` is **absent from all
934**, so row 5 does *not* apply to chapter 1.

**`<Polygon>` nests inside `<Collision>`, not under `<Entity>` — and chapter 1 is polygon-heavy.**
An earlier version of this block reported "`<Polygon>` 0 of 934" and raised it as an open
question. That was a measurement error: it looked for `<Polygon>` as a direct child of the inline
`<Entity>`. The writer puts it under the collision root
(`ETHEntityProperties.cpp:490-505`, `pCollisionRoot->LinkEndChild(pElement)`). Re-measured at the
correct depth: **all 220 `shape="3"` instances carry `<Polygon>` inside `<Collision>` — 220/220,
no exceptions.** There is no fallback path and no missing data. (17 `shape="1"` circle instances
also carry a `<Polygon>`; it is vestigial — the switch at `ETHPhysicsSimulator.cpp:85-98`
dispatches on `shape` and calls `GetCircleShape` for those, so their polygon is never read.)

Payload is ENML in **normalised −1..1 coordinates**, 8 vertices being the common case, e.g.
`v0{x=-0.95;y=1;} v1{x=-1;y=0.8;} … v7{x=0.95;y=1;}`. `GetPolygonShape`
(`ETHPhysicsSimulator.cpp:40-53`) maps each vertex to world space as
`(box.size * 0.5) * vertex + box.pos` — so **the polygon is a unit-square-relative outline scaled
by the `<Collision>` box**, which is exactly the shape a Godot `CollisionPolygon2D` wants.
Note `GetPolygonShape` dereferences the polygon pointer with **no null check**, which is
consistent with the data: a `shape=3` entity without polygon data would crash the original, so
the 220/220 result is what the engine requires, not a coincidence.

*Count reconciliation:* an audit agent independently reported "41 world-0 names resolve" using a
bare→`.ent` pairing convention. That convention is invalid per the paragraph above; the
authoritative split is **35 / 3 / 28**, and resolved-file counts are inventory trivia rather than
a reference list, since chapter 1 reads no `.ent` file at all.

Most-placed chapter-1 entities: `crystal.ent` (106, in 28/32), `platform.ent` (67),
`collision_128x768.ent` (64, in **32/32**), `arch.ent` (48), `wall_w3.ent` (46, in 32/32),
`block00.ent` (45), `double_block_plat.ent` (42). Present in all 32 scenes: `camera_start`,
`max`, `main_char`, `door.ent`, `door_bg.ent`, `collision_128x768.ent`, `collision_768x128.ent`,
`wall_w3.ent` — that octet is the minimum viable entity set for a level to load at all.
Note 155 of the 190 `.ent` files are referenced by no world-0 level at all, so this is the
**static placement list**, not the complete chapter-1 asset list — script-spawned entities
(`portal.ent` itself among them) are not visible in `.esc` data.

**Progression (answers §7.3): LINEAR, not star-gated.** State is two persisted integers,
`net.asantee.magicportals.lastWorldAllowed` / `lastLevelAllowed` — a single high-water mark, and
`sprites/skip_level_button.png` exists, so a level can be skipped. Medals are cosmetic. A
*second, orthogonal* gate is purchase (`net.asantee.magicportals.free`, `BuyButton.angelscript`,
`LastFreeLevelPopup.angelscript`) — free vs paid, not performance. **A "golden score" is a
per-level MAXIMUM PORTAL COUNT** to earn gold: seven thresholds are literally `0`, which is
incoherent as a time or par score and coherent only as "finish using zero portals".

**World↔chapter mapping is positively pinned**, not inferred, from achievement strings compiled
into `android_game.bin` cross-referenced against entity placement: *"Finish level 2-32 (dragon
boss)"* ↔ `dragon.ent` occurs in exactly one scene of 128, `level31a.esc`; *"Get the red key on
level 4-19"* ↔ `red_key_state.ent` only in `level18c.esc`; *"Score gold on level 1-9"* / *"1-23"*
↔ `rolling_stone.ent` in `level8.esc` and `level22.esc`. So suffix `''`/`a`/`b`/`c` = chapters
1–4, indices 0-based on disk and displayed 1-based. `Chapter 1..4` badge strings exist in the
bytecode; **no Chapter 5 string does.**

⚠️ **`assets/achievements.en.lang` is stale — do not use it.** It covers only chapters 1–3 and
disagrees with the bytecode (*"18 portals or less"* vs bytecode `22`; *"at least 9 boulders"* vs
`8`). The compiled bytecode is authoritative.

**Also newly established (was §7.4 in `MILESTONE_1.md`):**

- ENML duplicate keys: **last wins** — `Entity::Add` is `m_map[key] = value`
  (`gs2d/src/Enml/Enml.cpp:148-153`). On the real `app.enml` that yields `width=1136, height=640`.
- …but those are **desktop-only**. `width`/`height` are consumed solely by `machine/main.cpp:143-144`
  and `editor/main.cpp:171`. On Android the surface size arrives from JNI
  (`gs2d/src/Platform/android/main.cpp:91`).
- The shipped `app.enml` has **no `android` block** and its `fixedHeight` is commented out, and
  `LoadProperties` bails on a missing entity (`ETHAppEnmlFile.cpp:113-114`). **So `app.enml` does
  not determine the Android viewport at all.** Anything treating 1136×640, 640 or 512 as "the"
  virtual resolution is unsupported.
- **Orientation IS settled: locked landscape** (`AndroidManifest` `screenOrientation=0`) — so
  a remake locks it too. (The retired Godot project had `window/handheld/orientation=4`,
  Sensor Landscape, which should have been `0`; that project was removed on 2026-09-19.)
- Best-supported viewport inference is a **~512-tall landscape design space**, from `.esc`
  geometry (ymin −64 in 92 of 133 levels, modal ymax 384, median vertical span 448) agreeing
  with the commented-out `//fixedHeight = 512;`. **PROVISIONAL** — settle it by playtesting,
  not by reading more files.
- ⚠ **New constraint on §7.4, added 2026-08-05:** the original draws an on-screen movement
  pad — the `SCREEN_PAD_BUTTON` enum decodes to exactly `NO_SCREEN_BUTTON`,
  `LEFT_SCREEN_BUTTON`, `RIGHT_SCREEN_BUTTON` (see `docs/original-gameplay.md` §1). Those
  buttons occupy viewport space and must clear the ≥96 px touch-target floor
  (CLAUDE.md rule 4), so pad size and viewport height have to be resolved *together*.
  Whoever closes §7.4 should read that section first.
- Undocumented mechanism: **`ETHGlobalScaleManager`** (`engine/Util/ETHGlobalScaleManager.cpp:26-64`)
  is the engine's other viewport-adaptation path. For Magic Portals it is *not* engaged — the
  compiled bytecode never calls the setters, so the factor stays 1.0.

**Rule of thumb going forward:** treat every count in this document as an **existence proof,
never as a range**, and re-measure against `reference/apk/` before sizing any table, enum or
assertion. Corrections above are not yet folded into the section bodies — where a section and
this block disagree, **this block wins**.

---

## ⚠️ 2026-09-14 — measured on the RUNNING original

The original now runs in an emulator (`docs/parity.md`). These are observations of the live game,
not readings of data or source, and where they disagree with anything above, **they win**.
Captures stay in `out/parity/`; only the measurements are recorded here.

**The visible world is 256 units tall, not ~512.** Level 1-1 at 1280×720: the bottom floor row's
mortar gaps sit at x = 174, 355, 532, 714, 894, 1074 — pitches 181, 177, 182, 180, 180, mean
**180 px**. The same stones are 64 units wide in the level data (`single_stone_contrast.png` is
64×32), so the scale is 180 ÷ 64 = **2.8125 = 720 ÷ 256**. The upper ledge agrees (169, 178). This
closes MILESTONE_1 §7.4 by measurement and supersedes the "~512-tall" inference above. The 1×
`sky.png` is 455×256, exactly one 16:9 screen (the `fullhd/` sky the original actually draws is wider —
see the tier paragraph below). Whether the rule is height-locked
or width-locked cannot be told from a 16:9 screen; an 1184×720 capture of the menu (nav bar
present) showed menu buttons at the same pixel size as at 1280×720, which points to
**height-locked**, but that is one observation.

**At 720 px the original draws the best tier present PER FILE: `fullhd/`, else `hd/`, else 1×.**
⚠ *Corrected the same day.* This paragraph first said "`hd/`, because
`minScreenHeightForHdVersion` = 720" — that is the engine's default, and the game overrides it. The
shipped `app.enml` sets `minScreenHeightForHdVersion = 480`, `minScreenHeightForFullHdVersion = 600`,
`hdDensityValue = 2`, `fullHdDensityValue = 2`, so at 720 px `ETHSpriteDensityManager` (§8.5) tries
`fullhd/` first. Both tiers use density 2: a file's world size is its pixel size ÷ 2. The APK ships 53
`entities/hd/`, 7 `entities/fullhd/`, 69 `sprites/hd/` and a `particles/hd/` set. `hd/` is an exact 2× of
1× (e.g. `single_stone_contrast.png` 64×32 → 128×64), so it changes no size; `fullhd/` is NOT — the skies
go 455×256 → 1024×512, i.e. **512×256 world units, wider than the 1× file**. Measured, not assumed: a
pixel fit of the running original (`out/parity/specs/lighting/fit.md`, local) matches
`fullhd/arch_with_base_blur.png` and `fullhd/sky.png` but `hd/STONE03A4x10_contrast.png` in the same
frame, and forcing 1× everywhere worsens the fit on every sprite class. A remake drawing the 1× files at
2.8125× is visibly softer than the original, and one using the 1× skies draws them 11% too narrow.

**Progress is `scores.enml` in global external storage, and medals gate chapters.** File:
`<GetGlobalExternalStorageDirectory()>/scores.enml` = `/sdcard/.ethanon/<package>/files/`
(`gs2d/projects/Android/Ethanon/src/net/asantee/gs2d/GS2DActivity.java:219-220`); `achievements.enml`
sits beside it. Grammar: `version { number = 1_4; }` then one entity per world,
`world<W> { level<L> = <medal>; }`. Finishing 1-1 with zero portals wrote `level0 = 3` (gold).
Setting every level of every world to `1` unlocked all 32 chapter-1 levels and showed **33 %**
(bronze) — but chapter 2 stayed locked, captioned **"requires 60%"**; setting all to `3` showed
100 % and unlocked all four chapters. This **corrects §7.3 as recorded above**: medals are *not*
cosmetic, they drive the chapter gate (identifier `MINIMUM_ACCOMPLISHMENT_TO_UNLOCK`), and no
file holds the `lastWorldAllowed` / `lastLevelAllowed` integers — whatever those shared-data keys
do, they are not the persisted store. `2` = silver is expected but was not observed.

### 2026-09-14 (lighting) — what the converter reads, emits and copies: the `eth_*` keys

Source citations below are `toolkit/Source/src/engine/…` in `reference/ethanon/` unless a path says
otherwise. The research behind them is local and gitignored: `out/parity/specs/lighting/engine_math.md`
(the engine's maths, from source), `fit.md` (a numerical fit of that maths against the running
original's pixels) and `design_port.md` §3 (the key vocabulary). **Where the fit and the source
disagree, the fit wins** — that is the only way the colour-space question below could be settled.

**The model the keys serve, per sprite pixel** (every value an 8-bit *encoded* value ÷ 255):
`base = clamp(T·C·min(1, A+E) + LM)`, then one additive `clamp(T·C·light.color·LI·att·dl)` per live light,
and halos added on top. `A` = scene ambient, `E` = emissive rgb, `C` = instance colour, `LM` =
`add<id>.png` sampled at the sprite's UV, `LI` = `lightIntensity`.

| Question | Verdict | Source | MEASURED (fit, block MAE /255 unless stated) |
|---|---|---|---|
| Lightmap added or multiplied? | **Added**, in the ambient pass, unscaled by `C`, `A` or `LI` | `ETHRenderEntity.cpp:83-92`, `add1.ps` (`gs2d/src/Video/glslShaderCode.h:19-30`) | added 0.92 vs multiplied 26.95 (1,042 blocks); `C` blink `(actual−LM)/T` = 0.853/0.850/0.853 (uniform) |
| `lightIntensity` at draw time? | **No** — already baked into the file | `ETHPixelLightDiffuseSpecular.cpp:146`, `ETHLightmapGen.cpp:121` | 0.92 vs ×LI 26.36 vs ÷LI 6.81 |
| Ambient on a lightmapped sprite? | **Yes**, `min(1, A+E)` per channel, clamped before `C` | `ETHRenderEntity.cpp:112-117` | 0.92 vs no-ambient 54.67; fitted gain = scene ambient ±0.02 |
| Sprite without a lightmap: full bright? | **No**: `T·min(1, A+E)`; brightness comes only from `E` | same | 0.28 vs full-bright 45.30 (1,359 blocks) |
| Blend in linear light? | **No** — encoded values are multiplied and blended directly | the source is silent; the fit decides | 0.28 vs `gamma_linear` 13.59 (no LM), 0.92 vs 11.90 (LM) |
| PNG or the ETC1 `.pkm` beside it? | **PNG** | `gs2d/src/Video/GLES2/GLES2Texture.cpp:241-281` would pick `.pkm` on a non-PowerVR GPU | PNG better on 11/11 entities (pooled 0.92 vs 1.23) — margin small |
| Live light on a moving receiver | Yes, per pixel, from static lights too | `ETHEntitySpriteRenderer.cpp:70` | player past the 1-1 torch: residual flat +6.1 ± 1.3 R with the term, +11→+27 without; fitted scale 1.20 R / 0.96 G / 0.86 B |
| Halo units and brightness | World units; brightness ≈ 0.5–1 × `color·haloBrightness` | `ETHRenderEntity.cpp:354-388` (× owner particle ratio, `:371-376`) | 300 units fits (bMAE 4.06), 300 px rejected (9.70); ×0.5 on the strongest-signal entity, ×1 on weak ones — **not a measured constant** |
| Framebuffer | RGB565, no dither | `gs2d/projects/Android/Ethanon/src/net/asantee/gs2d/GL2JNIView.java:94`, `ConfigChooser.java:48-67` | dark no-LM sprites 0.28 with 5/6/5 vs 0.62 without |

This refines §5.6: `<EmissiveColor>` is not an additive glow; it can only raise the ambient multiplier
to 1.

**What is parsed** (`tools/converter/src/ethanon2godot/entity.py`, `scene.py`). Defaults are the
engine's, never 0-by-omission (§3.4):

| Field | XML | Default | Read at |
|---|---|---|---|
| `apply_light` | `<Entity applyLight>` | 0 | `ETHEntityProperties.cpp:231`; reset `:89` |
| `emissive` | `<EmissiveColor r g b a>`, all four read, rgb consumed | (0,0,0,0) | `:265` via `:658-675`; reset `:79`, `:87` |
| `scale` | `<Scale x y>` on the entity block | (1,1) | `:268`; reset `:82`, `:173` |
| `normal` | `<Normal>` text, a basename under `entities/normalmaps/` | "" | `:284-292`; loaded only with rich lighting, `ETHSpriteEntity.cpp:97-101` |
| `light` | `<Light range haloBrightness haloSize>` + `<Position x y z>`, `<Color r g b>`, `<HaloBitmap>` | range 256, haloSize 64, haloBrightness 1, pos 0, colour 1, `active` 1 | `ETHLight.cpp:40-63`; defaults `:27-38`; built as `ETHLight(true)`, `ETHEntityProperties.cpp:341` |
| `color` | **instance** `<Color r g b a>` | (1,1,1,1) | `ETHEntity.cpp:236`; `ETH_DEFAULT_COLOR` `:96` |
| `declared` | whether `<SceneProperties>` exists | — | the ambient/`lightIntensity` reads are `ETHSceneProperties.cpp:37-41`, reset `:88-94` |

A `<Light>` is kept only if `active && range > 0` (`ETHEntityProperties.cpp:343`); its own `static` is
then overwritten with the owner's at **`:347`** (§5.7 above says `:349`; the source says 347), so the
converter stores no light `static` and no `castShadows` (no shadow pass can run, `engine_math` §6.4).

⚠ **Correction — `<Color>` is an instance property, not an entity-block one.** `design_port.md` §3.1
lists it with the entity fields. `ETHEntityProperties::ReadFromXMLFile` (`:226-413`) has no `<Color>`
read; `ETHEntity::ReadInSceneDataFromXMLFile` reads it off the outer element (`ETHEntity.cpp:236`). An
inline body's `<Color>` means nothing. No level instance carries one, so no key is affected.

**What is emitted into the `.tscn`** (`tscn.py`). Every value is a **quoted string** of space-separated
numbers or a `res://` path — no `ExtResource`, no `Vector2`, no bare number — so the keys are inert in
Godot and no `load_steps` header changes. Keys are appended **after every existing line** of their
node, which makes the diff against pre-lighting output pure insertions.

| Key | Node | Value | Written when | Citation |
|---|---|---|---|---|
| `eth_ambient` | root | `"r g b"` | **always**, if `<SceneProperties>` exists | `ETHSceneProperties.cpp:88-94` |
| `eth_light_intensity` | root | `"LI"` | **always**, if `<SceneProperties>` exists | `ETHTypes.h:55` (default 2.0) |
| `eth_z` | entity | unrounded z | z ≠ 0, on **every** node (`z_index` is unchanged, rounded) | light geometry uses the receiver's world z, `engine_math` §4.4 |
| `eth_static` | entity | `"1"` | static | `ETHEntityProperties.cpp:230` |
| `eth_apply_light` | entity | `"1"` | applyLight | `:231` |
| `eth_emissive` | entity | `"r g b"` | rgb ≠ 0 (alpha ignored), on **every** node | `ETHRenderEntity.cpp:113-116` |
| `eth_color` | entity | `"r g b a"` | ≠ (1,1,1,1) | `ETHEntity.cpp:236`, `ETHRenderEntity.cpp:117` |
| `eth_normal` | entity | `res://assets/entities/normalmaps/<name>` | `<Normal>` present | `ETHDirectories.cpp:29` |
| `eth_lightmap` | entity | `res://assets/lightmaps/<scene>/add<id>.png` | static **and** applyLight **and** a sprite **and** the file exists | `ETHLightmapGen.cpp:42`; path `ETHScene.cpp:429-443`, `ETHSpriteEntity.cpp:434-439` |
| `eth_light_offset` | entity | `light.pos × scale.y`, 3 numbers | ≠ (0,0,0) | `ETHEntityRenderingManager.cpp:180` |
| `eth_light_range` | entity | `range × scale.y` | **always**, when a light is kept | `:181` |
| `eth_light_color` | entity | `"r g b"`, may exceed 1 | ≠ (1,1,1) | `ETHLight.cpp:51`; × LI at draw, `ETHPixelLightDiffuseSpecular.cpp:146` |
| `eth_halo` | entity | `res://assets/entities/<HaloBitmap>` | the light has a `<HaloBitmap>` | `ETHSpriteEntity.cpp:103-104`, `ETHDirectories.cpp:30` |
| `eth_halo_offset` | entity | `light.pos` x y, **unscaled** | halo present and ≠ (0,0) | `ETHRenderEntity.cpp:378` |
| `eth_halo_size` | entity | `haloSize × scale`, per axis | halo present and ≠ (64,64) | `:381-385` |
| `eth_halo_brightness` | entity | `haloBrightness` | halo present and ≠ 1 | `:380` |

Rules the table implies, and why:
- **Entity keys go only on nodes that draw a sprite or own a light** (`design_port.md` §3.2), **except
  `eth_z` and `eth_emissive`, which go on every node.** The gate trims `eth_static`/`eth_apply_light` to
  the drawn nodes (the port draws nothing else) and would trim `eth_normal`, which shades nothing without
  a sprite (no shipped spriteless instance names one). It must not trim `eth_z`: spriteless nodes are
  where the port's own actors take their lighting height (`design_port.md` §5.4 reads the player's z from
  the `main_char` node, which has no sprite), and `main_char` is **not** always at z 0 — 2-09
  (`level8a.esc`, instance 29) places it at z 2; the other 127 are at 0. Nor `eth_emissive`: the design's
  totals and the pins it plans for `test_mp_lighting`/`test_mp_start` (§3.5) count it over all instances
  (2,140).
  Invariant: a node carries `eth_z` exactly when it carries `z_index` (1,533 of each over the 128 levels,
  0 mismatches).
- **Three exceptions to "no key at the default".** The root keys are always written when the file has
  `<SceneProperties>`, so a reader can refuse a level without one instead of inheriting invented values
  (all 128 levels have it). `eth_light_range` is always written because a reader identifies a light by
  it, and `fire_agent.ent` really does use the default 256 (13 placements). Halo keys are written only when a
  `<HaloBitmap>` exists: without one no halo piece is created (`ETHEntityRenderingManager.cpp:78`), so the
  numbers have no consumer (the one such light, 1-2's `portal_static`, is at the defaults anyway).
- **Scaling differs per consumer**: lighting uses `scale.y` for position and range; the halo is centred
  on the unscaled offset and sized per axis. No shipped light owner has a `<Scale>`, so only the hand-written
  fixture exercises this (`tests/test_lighting.py`: `Scale 2×3` → offset `"0 -30 60"`, range `"750"`, halo
  offset `"0 -10"`, halo size `"560 840"`).
- **A lightmap file on an instance that fails the gate is reported, not attached.** The engine's
  `LoadLightmapsFromBitmapFiles` tries every entity (`ETHScene.cpp:315-334`) and `DrawAmbientPass` would
  apply one to any static entity, but none of the 1,554 `static=1 applyLight=0` instances in the 128 levels has a file, and
  the converter's report on all 128 levels lists none. A `.bmp`-only lightmap (the engine's fallback,
  `:327-331`) is also reported; none exists.

**What `--copy-lighting` copies** (`lighting.py`), into `<output>/assets/`:
`scenes/<scene>-esc/add<id>.png` → `lightmaps/<scene>/add<id>.png` (only attached ones; never `.pkm`/`.pvr`);
every file in `entities/normalmaps/` → `entities/normalmaps/` (44, because script-spawned entities use
files no level names, e.g. the player's `normalmap_77.png`); each named `<HaloBitmap>` → `entities/`.
**Tiers:** all three kinds load through `ETHGraphicResourceManager::AddFile` →
`ETHSpriteDensityManager::ChooseSpriteVersion` (`Resource/ETHResourceManager.cpp:133-134`,
`Resource/ETHSpriteDensityManager.cpp:88-113`), which prefers `fullhd/<name>` then `hd/<name>` at the
original's 720 px, exactly like a sprite. MEASURED over the APK: no `hd/` or `fullhd/` directory exists
under any of the 67 `-esc/` directories or under `normalmaps/`, and no halo bitmap has one in
`entities/hd/` or `entities/fullhd/` — so every copied lighting file is the base (1×) tier, which the fit
also confirms for lightmaps (0.5× the **1×** frame, 730/730). The copier checks this per file rather than
assuming it (a tier sibling makes the file SKIPPED and reported) and records `"tier": "1x"` for each copied
file in `<output>/assets/lighting_files.json`. (`ld/`/`xld/` apply only below the density thresholds and do
not exist in the APK.)

**Counts over all 128 levels** (converter report, 2026-09-14, re-run after the gate fix above and
recounted from the emitted `.tscn` files; "all instances" ignores the sprite-or-light gate; "design" is
`design_port.md` §3.2, measured by an independent parser):

| Key | Emitted | All instances | Design | |
|---|---|---|---|---|
| `eth_ambient`, `eth_light_intensity` | 128 each | — | 128 | = |
| `eth_lightmap` | **730** | 730 files match an instance | 730 | = (in 67 levels; level0 has 9) |
| `eth_light_range` (= lights kept) | **72**, 68 on static owners | 72 | 72 / 68 | = |
| `eth_halo` | **71** (`spark_halo.bmp`, `halo.bmp`, `portal_halo.png`) | 71 | 71 | = |
| `eth_normal` | **1,720** (27 distinct files, all present) | 1,720 | 1,720 | = |
| `eth_emissive` | **2,140** | 2,140 | 2,140 | = (6 of them on spriteless, lightless triggers: `destroyier.ent` ×4, `death_area.ent` ×2) |
| `eth_z` | **1,533** | 1,533 | 1,533 | = (204 of them on spriteless nodes: `camera_start` ×128, `max` ×39, `liftA` ×12, `liftB` ×5, `minion_spawn` ×4, `anti_sight_wall.ent` ×2, 14 singletons incl. `main_char` in `level8a`) |
| `eth_static` | 2,676 | 3,725 | 3,725 | gate (`collision_*` bodies, `main_char`, `properties`, …) |
| `eth_apply_light` | 1,719 | 2,452 | 2,452 | gate; 1,719 = every lit sprite, all of which have a `<Normal>` (+1 unlit `static_sphere`) |
| `eth_color` | 0 | 0 | 0 | = |
| `eth_light_offset` / `eth_light_color` | 71 / 72 | | | `portal_static`'s offset is (0,0,0) |
| `eth_halo_offset` / `eth_halo_size` | 54 / 54 | | | the 17 `fire_agent` halos sit at (0,0) and 64 |
| `eth_halo_brightness` | 66 | | | the 5 `blue_light` halos are at 1.0 |
| **all `metadata` strings added** | **11,234** | | | the port's `metaStrings` pin rises by exactly this |

Level 1-1's torch (`light_ent_696`) comes out as `eth_z "-18"`, `eth_light_offset "0 -12 24"`,
`eth_light_range "300"`, `eth_light_color "1 0.5 0.1"`, `eth_halo_size "300 300"`,
`eth_halo_brightness "0.7"`, `eth_lightmap ".../level0/add696.png"`; the root carries
`eth_ambient "0.35 0.3 0.35"`, `eth_light_intensity "3"`.

Not carried by any key, deliberately: `castShadows` (no effect), `specularPower`/`Brightness` (GLES2 has
no specular), `type = 2`'s vertical light variant (the 10 `light_off` instances), script-driven runtime
emissive (`design_port.md` §5.8). An absent `eth_z` means z 0 on every node, spriteless ones included.

### 2026-09-17 (tiers) — the entity texture tier copy (`--copy-textures`, `tiers.py`)

Source citations are `toolkit/Source/src/…` in `reference/ethanon/`. The research is local and gitignored:
`out/parity/visuals/briefs/system_6.md` §4.1 C2 and `00_order.md` step 2.1. This block **supersedes** §8.5's
thresholds (1080 / 720), its "an HD asset occupies the same world size as its 1× counterpart", and its
"pick one density tier" for this game.

**Which file the original draws.** Every image goes through `ETHGraphicResourceManager::AddFile`
(`engine/Resource/ETHResourceManager.cpp:133-144`), which asks `ChooseSpriteVersion`
(`engine/Resource/ETHSpriteDensityManager.cpp:88-139`) for `<dir>/fullhd/<file>` when the screen height is
`>= minScreenHeightForFullHdVersion`, then `<dir>/hd/<file>` when it is `>= minScreenHeightForHdVersion`
**or the fullhd test passed** (`:105`), then `xld/` and `ld/` on short screens (`:116-135`), each guarded
by `FileExists`, else the named file at density 1 (`:137-138`). Only the tier names are tried: **whether
the 1× file exists is never asked**, so a tier file with no 1× file beside it would still be drawn.

**Which numbers.** `ETHAppEnmlFile`'s constructor defaults are hd density 2, fullhd density **4**, ld 0.5,
xld 0.25, thresholds 720 / 1080 / 480 / 320 (`engine/Platform/ETHAppEnmlFile.cpp:39-52`). It then reads
the `default`, `window` and platform blocks in that order (`:59-66`; names `engine/ETHTypes.h:41-42`), each
key overriding only when present (`LoadProperties` `:111-148`, `GetFloat`/`GetUInt`
`gs2d/src/Enml/Enml.cpp:523-549`), and hands them to the density manager (`:68`,
`ETHSpriteDensityManager.cpp:47-59`). The platform on Android is `android`
(`gs2d/src/Platform/android/Platform.android.cpp:31-34`); the shipped `app.enml` has only `default` and
`ios`. Its `default` block sets `minScreenHeightForHdVersion = 480`, `minScreenHeightForFullHdVersion = 600`,
`hdDensityValue = 2`, `fullHdDensityValue = 2`. **At 720 px the search is fullhd (D 2), then hd (D 2)**;
`ld/` and `xld/` need a screen at most 480 / 320 px tall and the APK ships neither folder.

**Which size.** `SetSpriteDensityValue` sets the bitmap to `texels / D` as a float
(`gs2d/src/Video/GLES2/GLES2Sprite.cpp:429-438`) and `GetBitmapSize` casts it to int (`:389-392`), so an
uncut image is `int(texels / D)` units: an odd 973-texel hd width is 486 units, not 486.5.

**What the converter does** (owner rulings R12 and R13, 2026-09-16: the choice is fixed at the 720 px one,
and the Supersonic port resolves the tier from the file name, so no `.tscn` changes). For every texture a
level names, `--copy-textures` copies the 1× file to `<output>/assets/entities/<name>` as before, then the
first tier file the 720 px search finds to `<output>/assets/entities/<tier>/<name>`, reading the numbers
from `assets/app.enml` as above (a missing or unparsable file falls back to the constructor defaults, with a
warning, as the engine does). A destination that already holds the same bytes is not rewritten, and nothing
is deleted. It records the choice in `<output>/assets/entity_tiers.json`, merged across `--world` runs:
`{"format": 1, "choice": {"screen_height_px": 720, "search": [{"tier", "density"}…]}, "files":
{"entities/<name>": {"drawn", "tier", "density", "texels": [w, h], "units": [int(w/D), int(h/D)]}}}`.
Unlike `uiassets._copy_hd` it does **not** refuse a tier file of another size (the original draws it): it
prints each one as `tier size change`, and a tier file with no 1× file as `tier file without a 1x file`.

**Measured over the 128 levels** (four `--world` runs, 2026-09-17; `out/parity/visuals/2.1/run/`):
- 66 distinct textures: **7 fullhd, 38 hd, 21 1×**, so **45 tier files** copied; over the 2,883 sprite
  placements that is **2,248 hd / 146 fullhd / 489 1×**. All 45 are byte-identical to their APK files.
- **Exactly four change size**, all fullhd skies: `sky`, `icy_sky`, `red_sky`, `sky_purple`, 1024×512 texels
  = **512×256 units** against their 455×256 1× files. The other 41 tier files are exactly 2× (their units
  equal the 1× size); `arch_with_base_blur`, `satellite_sky` and `spiral` are the fullhd ones among them.
- No texture lacks a 1× file, and none has a size the header reader cannot read.
- The `.tscn` output is byte-identical with and without the copy (128 of 128 md5s). `tscn.py`'s
  `_vertical_offset` reads the **1×** height; that equals the tier's units for all 10 `type = 2` placements
  (`torch_small.png`, hd exactly 2×), but would not for a vertical fullhd sky (none exists).

**Not copied, deliberately.** Particle bitmaps: the port reads `particles/` from the extracted APK. Normal
maps, lightmaps and halo bitmaps: none has a tier file (the 2026-09-14 lighting block; `lighting.py` refuses
a tier sibling). **Orphan tier files** that no name reaches: `particles/hd/exclamation_mark_.png` and
`particles/hd/tesla_shock_.png` have no 1× file, and neither name occurs in any of the APK's 413
`.ent`/`.par`/`.esc`/`.enml` files (levels reference `tesla_shock_black_bg.png`, a different name), so the
original never picks them; the same holds for `sprites/hd/game_main_title_.png` and
`sprites/hd/world_icon3_.png`. `entities/hd/` has no orphan: all 53 files have a 1× name.

### 2026-09-27 (entity type) — `type` and `layerDepth` over the game's 190 `.ent` files

Source citations are `toolkit/Source/src/engine/…` in `reference/ethanon/`. This block replaces the
2026-08-04 table's row 5 ("`type=5` present") with the game's own count; §5.2 and §5.3 stand.

**How they are read.** `ETHEntityProperties::ReadFromXMLFile` reads `type` with `QueryIntAttribute`
(`Entity/ETHEntityProperties.cpp:228`) and `layerDepth` with `QueryFloatAttribute` (`:237`), each left at
`Reset`'s value when absent: `ET_HORIZONTAL` and `0.0` (`:176-177`). Nothing checks the type's range: an
integer outside `ENTITY_TYPE` (`Entity/ETHEntityProperties.h:82-89`) falls through `ComputeDepth`'s switch,
which has no default, to depth 0 (`Entity/ETHSpriteEntity.cpp:611-632`), and through `ConvertToGSSO` to
`EO_DEFAULT` (`Entity/ETHEntity.cpp:27-44`).

**What `ET_LAYERABLE` means for depth.** `ComputeDepth` returns `Max(m_layrableMinimumDepth, layerDepth)`
with the minimum `0.001` (`Entity/ETHSpriteEntity.cpp:34`, `:626-628`), whatever the entity's z. Every other
piece's depth is normalised between the scene's lowest and highest heights, so a layerable entity at 1.0
sits at the front of that range, in front of every sprite, halo and particle system the scene maps
(`Renderer/ETHEntityRenderingManager.cpp:37`, `:70-71`), except a vertical or decal piece at the very top of
the range: `ComputeDrawHash` adds a shift to those types' hash and not to a layerable's (`:119-144`), so one
at depth 1.0 can sort past a layerable 1.0. The game's one vertical entity is `light_off.ent`, and no file
uses a decal type (1 or 4).

**The census** (a grep of all 190 decoded `.ent` files, 2026-09-27, planning step 122; pinned in the
port's `tests/test_mp_sprites.cpp`):
- `type` is stated on all 190: **0 on 188, 2 on `light_off.ent` alone, 5 on `black_bar.ent` alone**. No
  file uses 1, 3 or 4.
- `layerDepth` is stated on 169: **1.0 on `black_bar.ent`**, `0.000000` on the other 168. The 21 that omit
  it (`portal.ent`, `projectile.ent`, `fireball.ent`, `minion.ent`, the skies and others) read `Reset`'s 0.
- So the one layerable entity is `black_bar.ent` (512×32 `black_bar.png`, sd only, opaque black), which
  `Game::manageSceneBounds` adds four times round the scene, and the one vertical entity is `light_off.ent`,
  whose particle systems sort `INDIVIDUAL_OFFSET` (`ETHEntityProperties::ResolveDepthSortingMode`,
  `Entity/ETHEntityProperties.cpp:108-122`). The bars are added by the script and placed by no level;
  `light_off` is placed by levels (its 10 instances, the 2026-09-14 lighting block above, which leaves its
  vertical variant out deliberately). This block changes nothing the converter emits.

---

## 1. Orientation: where to look in the engine source

Curated to the ~25 files that answer >90% of format questions.

| What you need | File · line | Why it is the authority |
|---|---|---|
| Scene parse loop | `toolkit/Source/src/engine/Scene/ETHScene.cpp:200` (`ReadFromXMLFile`), `:160` (`LoadFromFile`) | Root name never validated; `<EntitiesInScene>` walked with unfiltered `NextSiblingElement()`; failed entities silently dropped (`:229-244`) |
| Scene property defaults | `engine/Scene/ETHSceneProperties.cpp:30` (read), `:88` (`Reset`) | The only four scene-level fields on disk, and their true defaults |
| Scene-instance fields | `engine/Entity/ETHEntity.cpp:210` (`ReadInSceneDataFromXMLFile`), `:121` (`WriteToXMLFile`) | Every per-instance attribute/child; **`:217` is the `flipY`-inherits-`flipX` bug**; `:121-176` shows which fields are conditionally written |
| Reference-vs-inline discriminator | `engine/Entity/ETHEntityProperties.cpp:202-224` | Presence of `<FileName>` decides whether the `.ent` file is loaded or the inline block is authoritative |
| **THE `.ent` deserializer** | `engine/Entity/ETHEntityProperties.cpp:226-413` | All attributes, the *gated* reads (`:240-263`), all child elements, collision subtree (`:352-410`) |
| `.ent` defaults | `engine/Entity/ETHEntityProperties.cpp:79-83` (macros), `:85-106` (`ETHEntityMaterial::Reset`), `:167-181` (`ETHEntityProperties::Reset`) | Authoritative default table — absent attribute ⇒ these values, never 0 |
| Enums | `engine/Entity/ETHEntityProperties.h:82-90` (`ENTITY_TYPE`), `:92-99` (`BODY_SHAPE`); `gs2d/src/Video.h:60-68` (`ALPHA_MODE`) | Integer encodings a converter must hard-code |
| Custom data | `engine/Entity/ETHCustomDataManager.cpp:27-36` (type strings), `:510-608` (read), `:610-676` (write) | Scalars in element text, vectors in attributes; alphabetical write order |
| Light | `engine/Entity/ETHLight.cpp:27-38` (defaults), `:40-63` (read); acceptance test at `ETHEntityProperties.cpp:335-350` | Light dropped unless `active && range > 0`; `static` force-overwritten from the entity |
| Encoding sniff | `gs2d/src/Platform/FileManager.cpp:106-120` (`GetUTFFileString`), `:69-78` (BOM test), `:122-142` (`GetAnsiFileString`, the `.enml` path) | UTF-16LE iff first two bytes are `FF FE`, else UTF-8 with optional BOM |
| Which TinyXML | `engine/ETHTypes.h:28` → `../vendors/tinyxml_ansi/tinyxml.h` | **ANSI build is the compiled one.** `QueryIntValue` = `sscanf("%d")` at `vendors/tinyxml_ansi/tinyxml.cpp:1186-1191`; `SetDoubleValue` = `"%g"` at `:1211-1220` |
| Sprite-sheet cutting | `gs2d/src/Sprite.cpp:104-136` (`SetupSpriteRects`), `:143-148` (`SetRect` clamp), `:150-153` (`SetRect(col,row)`) | Integer strides, row-major, frame 0 top-left |
| Pivot math | `engine/Entity/ETHEntity.cpp:270-302` (`ComputeAbsoluteOrigin`/`ComputeOrigin`), `:27-44` (`ConvertToGSSO`), `:1019-1027` | `typeAnchor + pivotAdjust*scale`, normalized by size |
| Sprite creation / cut clamp | `engine/Entity/ETHSpriteEntity.cpp:108-116` (`Create`), `:375-383` (`ValidateSpriteCut`), `:652-683` (`GetCurrentSize`) | Cut is per-entity but sprites are cached per-basename ⇒ re-cut every frame |
| Asset path flattening | `engine/Resource/ETHResourceManager.cpp:93` + `:184-190`; role→dir binding at `engine/Entity/ETHSpriteEntity.cpp:95-104` | `resourceDir + fixedSubdir + basename` — **all directory components in the file are discarded** |
| Project directories | `engine/Resource/ETHDirectories.cpp:25-33` | `data/ scenes/ entities/ entities/normalmaps/ particles/ effects/ soundfx/` |
| Density variants | `engine/Resource/ETHSpriteDensityManager.cpp:30-33`, `:61-79`, `:81-86`, `:88-139`, `:141-144` | `hd/ fullhd/ ld/ xld/` inserted before the basename; per-file, per-screen-height |
| Density side effect | `gs2d/src/Video/GLES2/GLES2Sprite.cpp:429-438` (identical in `GLSprite.cpp:219`, `D3D9Sprite.cpp:574`) | Divides `bitmapSize` by density ⇒ **all rect math happens in virtual 1× pixels** |
| Vertex/UV math | `toolkit/Source/src/shaders/GLSL/default/optimal.vs:26-49` | `quad*size − center + entityPos`; proves quad top-left = `entityPos − absoluteOrigin` |
| Z projection | `engine/Util/ETHASUtil.cpp:123-126` (`ToScreenPos`) | `screen = (x,y) + zAxisDirection * z` |
| Depth / draw order | `engine/Entity/ETHSpriteEntity.cpp:611-632` (`ComputeDepth`), `engine/Renderer/ETHEntityRenderingManager.cpp:119-142` (`ComputeDrawHash`) | Per-type depth semantics; computed per frame, **not** stored |
| Bucket ordering | `engine/Scene/ETHBucketManager.cpp:42-50`; `engine/Scene/ETHScene.cpp:136-154` | Entity order in the file is bucket-hash order — **never** draw or authoring order |
| Physics body build | `engine/Physics/ETHPhysicsSimulator.cpp:26-27` (gravity/scale), `:31-63` (shape builders), `:65-120` (`CreateBody`) | Full extents → half-extents, `Scale` baked in, angle negated, 50 px per Box2D unit |
| ENML grammar | `gs2d/src/Enml/Enml.cpp:91-123` (char classes), `:313-448` (state machine), `:645-671` (`ReadValue`) | Used by `app.enml` **and** by `<Polygon>/<Compound>/<Joints>` blobs |
| `app.enml` keys/defaults | `engine/Platform/ETHAppEnmlFile.cpp:39-52` (defaults), `:53-74` (merge order), `:111-148` (key list) | `default` → `window` → `<platform>`; `definedWords` unions |
| Virtual resolution | `engine/Shader/ETHBackBufferTargetManager.cpp:55-84`, `:100-136` | `fixedWidth`/`fixedHeight` define the coordinate space scene positions live in |
| **THE `.par` deserializer** | `engine/Particles/ETHParticleSystem.cpp:89-136` (read), `:67-87` (`Reset` defaults), `:27-40` (`Scale`) | Same function serves standalone `.par` and `.ent`-embedded systems |
| Particle runtime units | `engine/Particles/ETHParticleManager.cpp:188-267` (integrator), `:473-523` (birth randomization) | `frameSpeed = min(dt,250ms)/1000*60` ⇒ ×60 / ×3600 unit conversions |

Best complete ground-truth samples: `testbed/GeneralTests/scenes/shadowTests.esc` (69 lines, whole scene),
`testbed/GeneralTests/entities/barrel_light.ent` (every optional feature), `testbed/GeneralTests/effects/fire_wsound.par`,
`testbed/GeneralTests/app.enml`, `testbed/GeneralTests/entities/invisible_entity.ent` (382 B, the minimal `.ent`).

---

## 2. Converter read order (follow this literally)

```
load_project(root):                       # root = dir containing app.enml (APK: assets/)
    cfg      = parse_enml(root/'app.enml')            # §8
    virtual  = compute_backbuffer(cfg)                # §8.4 — the design-time coord space
    density  = pick_density_tier(cfg)                 # §8.5 — CHOOSE ONE TIER for the port

for each scene file root/'scenes/*.esc':
    text = decode(bytes)                              # §3.1 BOM sniff
    xml  = parse(text)                                # tolerate CDATA
    doc_root  = first ELEMENT child of document       # name NOT validated
    props     = read_scene_properties(doc_root.find('SceneProperties'))   # §4.2, defaults if absent
    container = doc_root.find('EntitiesInScene')
    for child in container element-children:          # ANY tag name, not just <Entity>
        inst = read_instance(child)                   # §4.3 outer layer
        inner = child.find('Entity')                  # DIRECT child
        if inner.find('FileName') is not None:        # §4.4 branch A — reference
            props = load_ent(root/'entities'/inst.entity_name)   # keyed on <EntityName>, NOT <FileName>
            if props is None: DROP this entity        # engine drops it silently
            merge_custom_data(props, inner.find('CustomData'))    # per key, scene wins
        else:                                          # branch B — inline
            props = read_entity_block(inner)           # §5, full .ent schema
        emit_godot_node(inst, props, cfg, density)
```

`load_ent(path)`: decode → first element child of document → **its** first element child →
`read_entity_block`. Only the *first* `<Entity>` under the root is loaded; siblings are ignored
(`ETHEntityProperties.cpp:136-165`).

---

## 3. Rules that apply to every Ethanon data file

These recur in every format below. Implement them once.

### 3.1 Encoding — sniff, never assume
`FileManager::GetUTFFileString` (`gs2d/src/Platform/FileManager.cpp:106-120`):
```cpp
if (HasUTF16LEBOM(buffer)) return GetUTF16FileString(buffer, out);   // first 2 bytes == FF FE
else                       return GetUTF8FileString(buffer, out);     // strips EF BB BF if present
```
UTF-16**BE** is not supported (`:69-78` compares against the little-endian `0xFEFF`).

```python
text = data.decode('utf-16') if data[:2] == b'\xff\xfe' else data.decode('utf-8-sig')
```

Observed: **132 of 133** `.ent`/`.esc`/`.par` are UTF-16LE+BOM. The single exception,
`testbed/GeneralTests/entities/bird_utf8_bom.ent`, is UTF-8+BOM — a deliberate loader-tolerance fixture
(it would *fail* the toolkit's own `verify_text_formats.bat:17`). `.enml` and `.fnt` are 7-bit ASCII,
no BOM (`verify_text_formats.bat:20`; `toolkit/Utilites/source/TextFormatVerifier/AsciiVerifier.cpp:9-16`).

Line endings are mixed across the corpus (114 CRLF / 19 LF among the 133), never mixed within one file.
The XML prolog is byte-identical `<?xml version="1.0" ?>` in **all 133** files and carries **no `encoding=`
attribute** — the BOM is the only encoding signal. Root element is `<Ethanon>` in all 133 (but see §3.6).

### 3.2 Numbers — always `int(float(s))`, never `int(s)`
`TiXmlAttribute::QueryIntValue` is `sscanf(value, "%d", ival)` (`vendors/tinyxml_ansi/tinyxml.cpp:1186-1191`),
which stops at the `.`, so `"4.000000"` parses as `4` **and returns success**.

Two writer generations left two spellings on disk, and they **coexist inside the same directory** — number
style is *not* a version discriminator:

| writer | `SetDoubleValue` format | on disk |
|---|---|---|
| `tinyxml_ansi` (compiled in this snapshot, `ETHTypes.h:28`) | `"%g"` (`tinyxml_ansi/tinyxml.cpp:1211-1220`) | `4`, `402`, `0.8`, `-12` |
| `tinyxml_utf16` (older toolkit; not `#include`d anywhere in `toolkit/Source/src`) | `L"%lf"` (`tinyxml_utf16/tinyxml.cpp:1144-1153`) | `4.000000`, `402.000000` |

Real pair: `testbed/GeneralTests/entities/character.ent` → `<SpriteCut x="4.000000" y="4.000000"/>`;
`documentation/Sample-Projects/Sample-project/entities/blue_light.ent` → `<SpriteCut x="1" y="1"/>`.

Also present: **scientific notation with a 3-digit Windows exponent** —
`documentation/Sample-Projects/Sample-project/entities/plant.ent:3` has `layerDepth="-1.07374e+008"`.
Python's `float()` handles it; a hand-rolled fixed-point parser will not.

Fields to read as `int(float(s))`: `id`, `spriteFrame`, `type`, `shape`, `blendMode`, `SpriteCut/@x`,
`SpriteCut/@y`, `particles`, `allAtOnce`, `alphaMode`, `repeat`, `animationMode`, and all booleans.

Two theoretical divergences from `%d`, absent from all shipped data: a non-numeric string makes `sscanf`
fail so the engine **keeps the current default** (Python raises), and `"1e3"` is `1` under `%d` but `1000`
under `int(float(...))`. Guard if you care about bit-exact parity.

### 3.3 Booleans are integers
`ReadBooleanPropertyFromXmlElement` (`ETHEntityProperties.cpp:594-604`) does `QueryIntAttribute` and returns
`value != 0` — but **only when the query returns `TIXML_SUCCESS`** (`:600`). On a missing attribute *or* an
unparseable one it returns the supplied default. So `flipX="true"` reads as the **default**, not as true.
Any non-zero integer is true. Shipped data always writes `0`/`1`. (`.enml` is different — it uses
`true`/`false`; see §8.2.)

### 3.4 Missing attribute ⇒ the `Reset()` value, never 0
TinyXML's `QueryFloatAttribute`/`QueryIntAttribute` leave the target untouched unless parsing succeeds
(`tinyxml_ansi/tinyxml.h:998-1005`). Every field is queried independently, so a partial
`<Ambient r="0.5"/>` yields `(0.5, 1, 1)` and a partial `<Position x="10"/>` yields `(10,0,0)`.
Traps to remember: per-entity `parallaxIntensity` defaults to **1.0** (scene-level defaults to 0);
`specularPower` to **50**; `friction`/`density`/`gravityScale` to **1.0**; `maxSize` to **99999**.

### 3.5 Element lookups must be DIRECT-CHILD only
The engine uses `pElement->FirstChild(name)` everywhere. Element names are reused at different nesting
levels, so `elem.find('.//Name')` in Python will silently grab the wrong node:

| element | appears as child of |
|---|---|
| `<SpriteCut>` | `<Entity>` **and** `<ParticleSystem>` |
| `<Position>` | scene-instance `<Entity>` (with `@angle`), `<Light>` (no angle), `<Collision>` (no angle) |
| `<Color>` | scene-instance `<Entity>` (`r g b a`) and `<Light>` (`r g b`, **no alpha**) |
| `<Size>` | `<Collision>` only |
| `<Entity>` | itself — the scene nests instance `<Entity>` around template `<Entity>` |

Concrete failure: in `documentation/Sample-Projects/Sample-project/scenes/level1.esc` **all three**
`<SpriteCut>` elements belong to particle systems and none to an entity. A `.//SpriteCut` parse would
destroy every animation in `explosion.ent` (entity-level `1×1` shadows the particle-level `4×3`).

### 3.6 Structural looseness the engine tolerates
- Root element name is **never checked** — the loader takes "first element child of the document"
  (`ETHScene.cpp:190`, `ETHEntityProperties.cpp:164`). `<Ethanon>` is convention only.
- `<EntitiesInScene>` children are walked with `NextSiblingElement()` with **no name filter**
  (`ETHScene.cpp:218-247`); same for `<Variable>` under `<CustomData>` (`ETHCustomDataManager.cpp:601`)
  and `<ParticleSystem>` under `<Particles>`.
- `ETHScene::ReadFromXMLFile` and `ETHSceneProperties::ReadFromXMLFile` **always return true** — a file
  with no `<SceneProperties>` loads as an all-defaults scene.
- **Crash hazard in the engine, not in you:** text is fetched with `GetText()`, which returns `NULL` for an
  empty element (`ETHEntity.cpp:232`, `ETHEntityProperties.cpp:280/290/300`, `ETHCustomDataManager.cpp:577`,
  `ETHLight.cpp:59`, `ETHParticleSystem.cpp:120`). `<EntityName />` or `<Value />` is undefined behaviour.
  Real data never contains them; treat empty text as the field default.

### 3.7 CDATA is legal in physics blobs
`<Polygon>` and `<Compound>` are written wrapped in CDATA by the current writer
(`ETHEntityProperties.cpp:492-522`, `text->SetCDATA(true)`); `<Joints>` is plain text (`:528-530`).
Both raw and CDATA forms exist on disk: raw in `half_metal_crate.ent`, `lollipop.ent`,
`spinning_cross.ent`, `ushape_joint.ent`; CDATA in `ushape.ent` and ×4 in
`testbed/GeneralTests/scenes/physicsTest.esc` (first at `:117`). An XML parser handles both transparently;
a regex parser will not. `TiXmlElement::GetText()` returns the CDATA content verbatim
(`tinyxml_ansi/tinyxml.cpp:900-910`), so the engine is agnostic.

### 3.8 There is no JSON and no binary format
`grep -ril json` over `engine/`, `editor/`, `gs2d/` returns **zero hits**; `vendors/libjson` is vendored but
unreferenced. There are zero `.json` and zero `.as` files repo-wide. No magic number, no version attribute,
no format-discriminating byte exists in any `.esc`/`.ent`/`.par`. **Do not build a format-sniffing branch**
beyond the BOM check. If a later Ethanon revision added a JSON path, it is not in this snapshot.

---

## 4. `.esc` — scene format

XML. Top-level shape:

```xml
<?xml version="1.0" ?>
<Ethanon>                                     <!-- name not validated -->
  <SceneProperties lightIntensity parallaxIntensity>
    <Ambient r g b/>
    <ZAxisDirection x y/>
  </SceneProperties>
  <EntitiesInScene>
    <Entity …/> <Entity …/> …
  </EntitiesInScene>
</Ethanon>
```

`<SceneProperties>` and `<EntitiesInScene>` are looked up by name as direct children, in any order.
There is **no** scene-level `<CustomData>`, no scene name/version, and no bucket metadata — the top-level
children in all 11 shipped scenes are exactly those two.

### 4.1 Load path
`ETHScene::LoadFromFile` (`ETHScene.cpp:160-198`) → `ReadFromXMLFile` (`:200-259`) →
`ETHSceneProperties::ReadFromXMLFile` (`ETHSceneProperties.cpp:30-64`) →
per entity `new ETHRenderEntity(…)` → `ETHEntity::ReadInSceneDataFromXMLFile` (`ETHEntity.cpp:210-253`) →
`ETHEntityProperties::ReadFromXMLFile(pElement, entityCache, …)` (`ETHEntityProperties.cpp:202-224`).
An entity whose `IsSuccessfullyLoaded()` is false is **silently dropped** (`ETHScene.cpp:229-244`).

### 4.2 Scene properties — the complete on-disk set

`ETHSceneProperties` has exactly four members (`ETHSceneProperties.h:35-38`).

| XML | C++ | Type | Default (`Reset()`, `ETHSceneProperties.cpp:88-94`) | Meaning / units |
|---|---|---|---|---|
| `@lightIntensity` | `lightIntensity` | float | **2.0** (`_ETH_DEFAULT_LIGHT_INTENSITY`, `ETHTypes.h:55`) | Global multiplier on every dynamic light colour: `lightColor = Vector4(light->color,1) * lightIntensity` (`engine/Shader/ETHPixelLightDiffuseSpecular.cpp:146`). Unitless. Editor clamps 0..100. |
| `@parallaxIntensity` | `parallaxIntensity` | float | **0.0** | Scene-wide parallax strength. Offset formula `engine/Shader/ETHParallaxManager.cpp:99-106`: `offset = ((worldXY − cameraPos − originPx)/screenWidth) * z * intensity * entityParallaxIntensity`. Unitless. |
| `Ambient/@r,@g,@b` | `ambient` (Vector3) | float×3 | **(1,1,1)** — the `0.3` `_ETH_DEFAULT_AMBIENT_LIGHT` is commented out on `:90` | Linear 0..1 ambient multiplier: `diffuse = Vector4(ambient,1); diffuse += emissiveColor (clamped to 1); diffuse *= entityColor` (`ETHRenderEntity.cpp:113-117`). No alpha. |
| `ZAxisDirection/@x,@y` | `zAxisDirection` (Vector2) | float×2 | **(0,−1)** | Screen direction the world +z axis projects onto (§9.4). Screen pixels per z unit. |

Observed values: `lightIntensity` `2` (10 scenes) / `3` (`physicsTest.esc`); `parallaxIntensity` `0` (9) /
`2` / `2.25`; `Ambient` 8 distinct triples in 0.1–1.0, two non-grey (`level1.esc` `(0.2,0,0.2)`,
`Sample-project/scenes/scene.esc` `(0.4,0,0.4)`); `ZAxisDirection` `(0,−1)` in 10 scenes and **`(0,0)` in
`physicsTest.esc`** (semantics UNKNOWN — see §12).

**Not in the `.esc`** (all runtime-only, do not look for them): bucket size (constructor arg;
`_ETH_DEFAULT_BUCKET_SIZE = 256.0f` scalar at `ETHTypes.h:58`, editor uses `(512,512)`), parallax **origin**
(normalized, default `(0.5,0.5)`, `ETHParallaxManager.cpp:32-35`), parallax vertical intensity (`0.1`),
z-buffer enable, lightmap enable, camera position.

### 4.3 Outer `<Entity>` — the scene instance

Read `ETHEntity.cpp:210-253`; write `:121-176`.

| XML | C++ | Type | Default if absent | Written when | Meaning |
|---|---|---|---|---|---|
| `@id` | `m_id` | int | **−1** (ctor `:66`) | always (`:153`) | Unique within the file. If `< 0` after read, `ETHScene::AddEntity` assigns a counter (`ETHScene.cpp:269-275`). Ids are **sparse, non-contiguous, and not in document order** (observed min 0, max 385; first id per scene ranges 1…78). **Do not renumber** — the lightmap sidecar is keyed on it (§4.7). |
| `@spriteFrame` | `m_spriteFrame` | int→unsigned (`:219-221`) | **0** | always (`:154`) | Index into the sprite-cut grid (§9.1). Only `0` and `8` occur in shipped data. |
| `@shadowZ` | `m_shadowZ` | float | **0.0** | only if `castShadow` (`:159-160`) | Z used for the projected fake shadow. Present on 91/362 instances, and **`0` in all 91** ⇒ semantics not inferable from data (§12). |
| `@hide` | `m_hide` | int-bool | **0** | only if ≠ 0 (`:162`) | Skip rendering. |
| `@flipX` | `m_flipX` | int-bool | **0** | only if ≠ 0 (`:163`) | Horizontal flip. |
| `@flipY` | `m_flipY` | int-bool | **inherits `flipX` — see below** | only if ≠ 0 (`:164`) | Vertical flip. |
| `<EntityName>` | `m_properties.entityName` | string | `""` | only if non-empty | The cache key for branch A (§4.4). See §4.5. |
| `<Color r g b a>` | `m_v4Color` | Vector4 | **(1,1,1,1)** (`ETH_DEFAULT_COLOR`, `:96`) | only if ≠ default | Per-instance tint, multiplied into ambient diffuse (`ETHRenderEntity.cpp:117`). Linear 0..1. Present on 7/362 instances, **all in `customDataTesting.esc` and all at `(1,1,1,1)`** ⇒ semantics from source only. |
| `<Position x y z angle>` | controller pos + angle | 4 floats | `(0,0,0)` / `0` | always (`:143-152`) | `x,y` world pixels; `z` depth (§9.4); `angle` **degrees** (§9.5). |

That list is exhaustive: across all 362 shipped instances the only outer attributes are `id` (362),
`spriteFrame` (362), `shadowZ` (91), and the only outer children are `EntityName` (362), `Position` (362),
`Entity` (362), `Color` (7). Child order is `EntityName > Position > Entity` (355) or
`EntityName > Color > Position > Entity` (7) — the inner `<Entity>` is always last.

> **Engine bug you must replicate — `flipY` inherits `flipX`.** `ETHEntity.cpp:215-217`, in execution order:
> ```cpp
> m_hide  = ReadBooleanPropertyFromXmlElement(pElement, GS_L("hide"),  m_hide);
> m_flipX = ReadBooleanPropertyFromXmlElement(pElement, GS_L("flipX"), m_flipX);
> m_flipY = ReadBooleanPropertyFromXmlElement(pElement, GS_L("flipY"), m_flipX);   // :217
> ```
> Line 216 has already overwritten `m_flipX` with the file's value by the time line 217 uses it as the
> *default* for `flipY`. So `<Entity flipX="1">` with no `flipY` loads as **flipX=1, flipY=1**, and since the
> writer emits each flag only when non-default (`:162-164`), a flipX-only entity round-trips double-flipped.
> **Rule:** `flip_x = int(attr("flipX", 0)); flip_y = int(attr("flipY", flip_x))`.
> Zero shipped scenes use flip attributes at all, so testbed data will not surface this — real game data may.

**There is no per-instance scale.** `<Scale x y>` is a property of the *entity block*
(`ETHEntityProperties.cpp:268` read, `:441-442` write), default `(1,1)`, and appears in **0 of 362**
scene instances. Take scale from the nested `<Entity>` / `.ent`, never from the instance.

### 4.4 Nested `<Entity>` — two mutually exclusive forms

Discriminator, `ETHEntityProperties::ReadFromXMLFile(pElement, entityCache, entityPath, fileManager)`
(`ETHEntityProperties.cpp:202-224`), verbatim:

```cpp
TiXmlNode *pNode = pElement->FirstChild(GS_L("FileName"));
if (pNode)
{
    const ETHEntityProperties* props = entityCache.Get(entityName, entityPath, fileManager);
    if (props)
    {
        *this = *props;                  // the .ent file WINS, wholesale
        ReadDataFromXMLFile(pElement);   // then scene <CustomData> merged on top, per key
        return (successfullyLoaded = true);
    }
}
else
{
    return ReadFromXMLFile(pElement);    // inline: this block IS the definition
}
return false;                            // <FileName> present but .ent missing -> entity DROPPED
```

**Branch A — reference** (`<FileName>` present). Written by `WriteEntityNameToXMLFile` (`:415-426`), which
emits exactly `<Entity><FileName>name.ent</FileName><CustomData>…</CustomData></Entity>` and nothing else.
Critical detail: **the cache key is the text of the OUTER `<EntityName>`, not the text of `<FileName>`.**
`GS_L("FileName")` occurs exactly twice in `toolkit/Source/src` — the existence test at `:208` and the writer
at `:420` — so **`<FileName>`'s text is never read by any load path**. Lookup is
`ETHEntityCache::Get` → literal `entityPath + entityName` with **no extension fix-up**
(`ETHEntityCache.cpp:27-49`), where `entityPath = <resourceDir> + "entities/"` (`ETHScene.cpp:59`).
`*this = *props` uses the implicit `operator=`, which memberwise-copies the `shared_ptr` members (`light`,
`collision`, `polygon`, `compoundShape`, `particleSystems`) — instances **share** those objects with the
cached prototype; only the custom-data map is deep-copied (`ETHCustomDataManager::operator=` → `CopyMap`,
`ETHCustomDataManager.cpp:38-42`).

**Branch B — inline** (`<FileName>` absent). The nested `<Entity>` carries the full `.ent` property set
(§5) and is parsed by `ETHEntityProperties::ReadFromXMLFile(pElement)` (`:226-413`). **The `.ent` file is
never opened.** If a same-named `.ent` exists on disk with different values, the inline block still wins —
this actually happens: `TestEntity.esc`'s `character` instance declares `type="2"` / `PivotAdjust y="−6"`
inline while `entities/character.ent` declares `type="0"` / `PivotAdjust (0,0)`.

**All 11 shipped scenes are 100% inline** (zero `<FileName>` elements corpus-wide), but the writer emits the
reference form whenever the entity cache resolves the name (`ETHEntity.cpp:167-174`). **Implement both.**

### 4.5 `<EntityName>` is a label, not reliably a filename
347 of 362 values end in `.ent`; **15 do not** — `anchor`, `tower`, `tower2`, `motor`, `meteor`,
`one_sided_platform`, `sick_spinning_cross` (physicsTest), `character`, `character2`, `cursor` (TestEntity),
`custom-barrel` ×2, `barrie` (customDataTesting), `cursor` (temporary_entity_test),
`cursor` (verticalParallaxTest). **14 of the 15 have no matching file on disk; `character` does**
(`testbed/GeneralTests/entities/character.ent` exists, `character2` does not) — that inconsistency is the
trap. All 15 carry a fully self-contained inline block. Nothing appends `.ent`, so an extension-less name
in branch A can never resolve and its entity would be dropped.

Scripts use `<EntityName>` for lookup — e.g. `spinning_cross.ent:44-50` stores a custom-data
`string revoluteJoint = "tower"` naming the joint partner by entity name (§6.3).

**Rule:** never resolve `<EntityName>` as a required file reference, and never prefer an on-disk `.ent`
over an inline block.

### 4.6 File order carries no meaning
`ETHScene::SaveToFile` (`ETHScene.cpp:136-154`) walks an `ETHOrderedBucketMap` =
`std::map<Vector2, ETHEntityList, BUCKET_COMP>` (`ETHBucketManager.h:52`) whose comparator orders by
`boost::hash_value(Vector2)` (`ETHBucketManager.cpp:42-45`). Entity order in the file is therefore
**grouped by spatial bucket in hash order** — not authoring order, not spatial order, not draw order.
Buckets themselves are runtime-only (`GetBucket = floor(pos / bucketSize)`, `ETHBucketManager.cpp:47-50`);
**nothing bucket-related is stored on disk and all coordinates are absolute world pixels.**
Real draw order is computed per frame from depth (§10.3).

### 4.7 Sidecar: baked lightmap directory
Static lighting is not in the `.esc`. `ETHScene::ConvertFileNameToLightmapDirectory` (`ETHScene.cpp:429-443`)
replaces every `.` in the scene filename with `-` and appends `/`:
`scenes/temporary_entity_test.esc` → `scenes/temporary_entity_test-esc/`. Per-entity file is
`add<entityID>.<ext>` (`ETHSpriteEntity.cpp:434-439`), and PNG is preferred over BMP
(`ETHScene.cpp:315-334`). Observed: `testbed/GeneralTests/scenes/temporary_entity_test-esc/` holds
`add20.bmp add21.bmp add22.bmp add24.bmp add32.bmp add34.bmp` — uncompressed 24-bit BMPs whose dimensions
match the source sprites, and whose numbers are all ids present in the scene. Derived data; a converter
should ignore it but **must not renumber instance ids** if it ever wants to use it.

### 4.8 Annotated real sample
`testbed/GeneralTests/scenes/shadowTests.esc` — the complete 69-line file, abbreviated only where marked:

```xml
<?xml version="1.0" ?>
<Ethanon>
     <SceneProperties lightIntensity="2.000000" parallaxIntensity="0.000000">
          <Ambient r="0.200000" g="0.200000" b="0.200000" />      <!-- overrides default (1,1,1) -->
          <ZAxisDirection x="0.000000" y="-1.000000" />           <!-- == the default -->
    </SceneProperties>
     <EntitiesInScene>
          <Entity id="1" spriteFrame="0">
               <EntityName>bump_test_floor.ent</EntityName>       <!-- label; block below is authoritative -->
               <Position x="128.000000" y="128.000000" z="0.000000" angle="0.000000" />
               <Entity shape="0" applyLight="1" specularPower="50.000000" specularBrightness="1.000000"
                       castShadow="0" type="0" static="1" startFrame="0" blendMode="0">
                    <!-- no <FileName> => INLINE. startFrame is legacy and unread. -->
                    <Sprite>white_ground.jpg</Sprite>             <!-- resolved to entities/white_ground.jpg -->
                    <Normal>bump_test.png</Normal>                <!-- entities/normalmaps/bump_test.png -->
                    <CustomData />                                <!-- always written, even empty -->
            </Entity>
        </Entity>
          <!-- ids 2 and 3: same block, different <Position> only -->
          <Entity id="5" spriteFrame="0">                          <!-- id 4 absent: ids need not be dense -->
               <EntityName>red_light.ent</EntityName>
               <Position x="112.000000" y="38.000000" z="26.000000" angle="0.000000" />
               <!-- z=26 with zAxisDirection (0,-1) => drawn 26 px UP the screen from y=38 -->
               <Entity shape="0" applyLight="1" … type="0" static="1" blendMode="0">
                    <Light active="1" static="1" castShadows="1" range="265.000000"
                           haloBrightness="1.000000" haloSize="72.000000">
                         <Position x="0" y="0" z="0" />           <!-- offset RELATIVE to the entity -->
                         <Color r="1.000000" g="0.000000" b="0.000000" />   <!-- rgb only, no alpha -->
                         <HaloBitmap>halo1.bmp</HaloBitmap>       <!-- entities/halo1.bmp -->
                </Light>
                    <CustomData />                                <!-- no <Sprite>: light + halo only -->
            </Entity>
        </Entity>
          <Entity id="6" spriteFrame="0" shadowZ="0.000000">       <!-- shadowZ written: castShadow=1 -->
               <EntityName>tall_shadow_barrel.ent</EntityName>
               <Position x="116.000000" y="189.000000" z="0.000000" angle="0.000000" />
               <Entity shape="0" applyLight="1" specularPower="50.000000" specularBrightness="1.000000"
                       castShadow="1" shadowScale="0.900000" shadowLengthScale="3.300000"
                       shadowOpacity="1.000000" type="2" static="0" startFrame="0" blendMode="0">
                    <!-- type=2 == ET_VERTICAL => centre-bottom origin, rotation forced to 0 -->
                    <PivotAdjust x="0.000000" y="-12.000000" />
                    <Sprite>barril.png</Sprite>
                    <Normal>barril_nm.bmp</Normal>
                    <CustomData>
                         <Variable><Type>vector2</Type><Name>direction</Name>
                                   <Value x="0.000000" y="0.000000" /></Variable>   <!-- ATTRIBUTES -->
                         <Variable><Type>int</Type><Name>resistance</Name>
                                   <Value>100</Value></Variable>                    <!-- TEXT -->
                </CustomData>
            </Entity>
        </Entity>
    </EntitiesInScene>
</Ethanon>
```

Contrast files worth testing against: `customDataTesting.esc` (older writer — unconditional `<Color>`,
`collidable`/`soundVolume`/`layerDepth`, `<EmissiveColor>`, `<SpriteCut>`, `<Particles/>`, the corpus's only
XML entity `&apos;` at `:31`, and its only non-ASCII text `André&apos;s tree ãõíàìçöäêâîôû`);
`TestEntity.esc` (only non-zero `spriteFrame`, value `8`, with `<SpriteCut x="4.000000" y="4.000000"/>`);
`physicsTest.esc` (CDATA `<Compound>` at `:117` and raw `<Joints>` at `:119`, `ZAxisDirection (0,0)`,
`angle="90"`); `level1.esc` and `Sample-project/scenes/scene.esc` (compact numerics, LF line endings).

---

## 5. `.ent` — entity definition format

XML, one entity per file. Skeleton (`ETHEntityProperties::SaveToFile`, `ETHEntityProperties.cpp:188-200`):

```xml
<?xml version="1.0" ?>
<Ethanon>
    <Entity  ...attributes... >  ...child elements...  </Entity>
</Ethanon>
```

Load: `ETHEntityProperties::ETHEntityProperties(filePath, fileManager)` (`:136-165`) — decode, first element
child of the document, then **its** first element child. Only the *first* `<Entity>` is loaded; siblings are
ignored (all 89 samples have exactly one).

`entityName` for a standalone `.ent` is `Platform::GetFileName(filePath)` (`:163`), i.e. **including the
`.ent` extension** (`"barrel.ent"`). Inside a scene it comes from `<EntityName>` (§4.5).

### 5.1 The single most important parsing rule: gated attribute reads

Attribute parsing is **sequential and conditional** (`ETHEntityProperties.cpp:240-263`):

```cpp
if (applyLight)        { specularPower; specularBrightness; }
if (castShadow)        { shadowScale; shadowLengthScale; shadowOpacity; }
if (shape != BS_NONE)  { sensor; fixedRotation; bullet; friction; density; gravityScale; restitution; }
```

A file that says `applyLight="0" specularPower="999"` loads `specularPower = 50`. A file that says
`shape="0" density="0.000000"` loads `density = 1.0`. **Reproduce this or you will emit non-default
physics/lighting for entities the engine treats as defaults.** The writer mirrors it (`:535-560`).

### 5.2 `<Entity>` attributes — complete

| Attribute | Type | Default | Read at | Meaning |
|---|---|---|---|---|
| `type` | int enum | `0` `ET_HORIZONTAL` | `:228` | §5.3 |
| `static` | int-bool | `0` | `:230` | Never updated/simulated; eligible for lightmap baking; forces `light->staticLight`; makes the Box2D body `b2_staticBody` |
| `applyLight` | int-bool | `0` | `:231` | Participates in the dynamic lighting pass |
| `castShadow` | int-bool | `0` | `:232` | Casts fake/real-time shadows |
| `hideFromSceneEditor` | int-bool | `0` | `:233` | Editor-only (`editor/SceneEditor.cpp:1671`). Written only when true |
| `shape` | int enum | `0` `BS_NONE` | `:235` | §5.4 |
| `blendMode` | int enum | `0` `AM_PIXEL` | `:236` | §5.5 |
| `layerDepth` | float | `0.0` | `:237` | Only meaningful for `ET_LAYERABLE`; `0` = furthest, `1` = closest; clamped 0..1 in `ETHEntity::SetLayerDepth` (`ETHEntity.cpp:540-544`). **Written only if `type == ET_LAYERABLE`** (`:563-564`), yet 69/89 samples carry it and **no sample uses `type="5"`** — legacy writer output |
| `parallaxIntensity` | float | **`1.0`** | `:238` | Per-entity multiplier on scene parallax. Written only if ≠ 1.0 |
| `specularPower` | float | `50.0` | `:242` | **gated on `applyLight`**. Observed 40/50/80/90/200 |
| `specularBrightness` | float | `1.0` | `:243` | **gated on `applyLight`** |
| `shadowScale` | float | `0.0` | `:248` | **gated on `castShadow`**; `<= 0` treated as `1.0` at render time (`ETHRenderEntity.cpp:249`) |
| `shadowLengthScale` | float | `1.0` | `:249` | **gated**; multiplies the shadow quad height (`ETHRenderEntity.cpp:305`) |
| `shadowOpacity` | float | `1.0` | `:250` | **gated**; `<= 0` also treated as `1.0` (`ETHRenderEntity.cpp:250`) — a fully transparent shadow is not representable |
| `sensor` | int-bool | `0` | `:255` | **gated on `shape != 0`**; Box2D `isSensor` |
| `fixedRotation` | int-bool | `0` | `:256` | **gated**; Box2D `fixedRotation` |
| `bullet` | int-bool | `0` | `:257` | **gated**; Box2D CCD |
| `friction` | float | `1.0` | `:259` | **gated** |
| `density` | float | `1.0` | `:260` | **gated**; forced to `0` for static entities (`ETHPhysicsSimulator.cpp:107`) |
| `gravityScale` | float | `1.0` | `:261` | **gated** |
| `restitution` | float | `0.0` | `:262` | **gated** |
| `collidable` | int | **`1` when absent** | `:354` | **Legacy, still read.** If present and `0`, the entire `<Collision>` subtree is skipped. Never written by current code. Distribution over 89 files: absent 44, `1` 14, `0` 31 |
| `startFrame` | int | — | — | **Dead.** No XML read or write path exists (`grep` over `engine/`). Only an editor GUI widget survives (`editor/EntityEditor.cpp:1664` live, `:335-340/:592/:1646` and `SceneEditor.cpp:1050/:1353` commented). Superseded by the instance attribute `spriteFrame`. Present in 75/89 `.ent` and 6/11 `.esc` |
| `soundVolume` | float | — | — | **Dead.** Zero references anywhere in `toolkit/Source/src`. Present in 73/89 |

**Only four attributes are universal** across the 89 samples: `type`, `static`, `applyLight`, `castShadow`.
Everything else can be absent and needs the default above. (A converter that treats `specularPower` as
required will crash on `testbed/GeneralTests/entities/invisible_entity.ent`, the 382-byte minimal file:
`<Entity shape="0" applyLight="0" castShadow="0" type="0" static="0" blendMode="0"><Particles /><CustomData /></Entity>`.)

### 5.3 `ENTITY_TYPE` (`ETHEntityProperties.h:82-90`)

```cpp
ET_HORIZONTAL = 0, ET_GROUND_DECAL = 1 /*deprecated*/, ET_VERTICAL = 2,
ET_OVERALL = 3 /*deprecated*/, ET_OPAQUE_DECAL = 4 /*deprecated*/, ET_LAYERABLE = 5
```
Independently confirmed by the name table at `ETHTypes.h:89-97`:
`{"horizontal","ground decal","vertical","overall","opaque decal","layerable"}`.

| # | Name | Origin (`ETHEntity::ConvertToGSSO`, `:27-44`) | Depth (`ETHSpriteEntity::ComputeDepth`, `:611-632`) | Draw hash (`ETHEntityRenderingManager.cpp:119-142`) | Rotatable | Particle depth mode |
|---|---|---|---|---|---|---|
| 0 | `ET_HORIZONTAL` | `EO_CENTER` | `(z − minH)/(maxH − minH)` | `hashDepth` | yes | `SAME_DEPTH_AS_OWNER` |
| 1 | `ET_GROUND_DECAL` | `EO_CENTER` | `(z + 0.1 − minH)/(maxH − minH)` | `hashDepth + 0.1` | yes | `SAME_DEPTH_AS_OWNER` |
| 2 | `ET_VERTICAL` | **`EO_CENTER_BOTTOM`** | `(z − minH)/(maxH − minH)` | `hashDepth + (posY − camY)/screenSize.y + 0.1` | **NO — angle forced to 0** | `INDIVIDUAL_OFFSET` |
| 3 | `ET_OVERALL` | `EO_CENTER` | constant **`1.0`** (always in front) | `hashDepth` | yes | `SAME_DEPTH_AS_OWNER` |
| 4 | `ET_OPAQUE_DECAL` | `EO_CENTER` | `(z + 0.1 − minH)/(maxH − minH)` | `hashDepth + 0.1` | yes | `SAME_DEPTH_AS_OWNER` |
| 5 | `ET_LAYERABLE` | `EO_CENTER` | **`max(0.001, layerDepth)` — ignores z entirely** | `hashDepth` | yes | `LAYERABLE` |

`+0.1` is `ETH_SMALL_NUMBER` (`ETHTypes.h:36`); `0.001` is `ETHSpriteEntity::m_layrableMinimumDepth` (`:34`).
The `ET_VERTICAL` hash shift is written with a `precisionScale = 100` that cancels, reducing to
`(posY − camY) / screenSize.y`.

**`ET_VERTICAL` never rotates.** Hard override in both render passes, not a property:
`ETHRenderEntity.cpp:105` (`angle = (type == ET_VERTICAL) ? 0.0f : GetAngle()`) and `:151`
(`(!IsRotatable() || drawToTarget) ? 0.0f : GetAngle()`), with `ETHEntity::IsRotatable()` =
`GetType() != ET_VERTICAL` (`:621-624`). A `type="2"` entity with `angle="45"` renders unrotated — the angle
is still read, still visible to scripts, and still affects `ComputeInScreenSpriteCenter`, but does not
rotate the sprite.

`type` is read with `QueryIntAttribute` into the enum with **no range validation**; an out-of-range value
falls through every switch and lands on `default: EO_DEFAULT` in `ConvertToGSSO` — origin `(0,0)`,
**top-left**, not centre.

Type census over all 813 `<Entity>` tags: `0` ×259, `1` ×1 (`explosion_decal.ent`), `2` ×179, `3` ×7
(`black.ent`), `4` ×5 (`blood_decal.ent`). Per-file over 89 `.ent`: `0`→47, `1`→1, `2`→39, `3`→1, `4`→1,
**`5`→0**. Semantics from the data: `0` = flat/ground (floors, walls, light-only emitters, physics bodies);
`2` = upright prop/actor with its position at the ground contact point (barrels, trees, characters);
`5` = pure 2D layer. `type` is **orthogonal** to whether the entity has a `<Light>`.

### 5.4 `BODY_SHAPE` (`ETHEntityProperties.h:92-99`)
`BS_NONE = 0, BS_BOX = 1, BS_CIRCLE = 2, BS_POLYGON = 3, BS_COMPOUND = 4`.
String forms (used only inside compound ENML): `"box" "circle" "polygon" "compound"`, anything else →
`BS_NONE` (`ETHPhysicsSimulator::StringToShape`, `:309-321`).

`shape == BS_NONE` means "no Box2D body", but the entity can still be collidable in the legacy AABB sense:
`ETHEntity::IsCollidable()` merely tests `collision != null` (`ETHEntity.cpp:657-660`), while
`ETHEntity::IsBody()` requires `shape != BS_NONE && IsCollidable()` (`:729-733`).
(`ETHEntityProperties::IsCollidable()` is declared at `ETHEntityProperties.h:109` and has **no definition
anywhere** — dead.)

Corpus: `shape` values `0` ×336, `1` ×48, `2` ×2, `3` ×1, `4` ×8. `1` ↔ box bodies; `2` ↔ `asteroid_body.ent`;
`3` ↔ `half_metal_crate.ent` (the only `<Polygon>`); `4` ↔ `lollipop`, `spinning_cross`, `ushape`,
`ushape_joint` (all the `<Compound>`/`<Joints>` files).

### 5.5 `blendMode` = `gs2d::Video::ALPHA_MODE` (`gs2d/src/Video.h:60-68`)

| # | Name | GL state (`gs2d/src/Video/GL/GLVideo.cpp:226-263`) |
|---|---|---|
| 0 | `AM_PIXEL` | `glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)` + alpha test — standard alpha blend (default) |
| 1 | `AM_ADD` | `glBlendFunc(GL_ONE, GL_ONE)` — additive |
| 2 | `AM_ALPHA_TEST` | blending disabled, alpha test only — 1-bit cutout |
| 3 | `AM_NONE` | blending and alpha test disabled — fully opaque |
| 4 | `AM_MODULATE` | `glBlendFunc(GL_ZERO, GL_SRC_COLOR)` — multiply |

Plus `AM_UNKNOWN = 0xFFFFFFFF` (`:67`). Entity census: attribute absent 29 files, `"0"` 52, `"2"` 8
(`barrel_not_found`, `barrel_super_high_parallax`, `barrel_with_data`, `bird_utf8_bom`, `horizontalBumpOdd`,
`pillar_fire`, `raw_barrel`, `small_bump_test`). Across all 813 `<Entity>` tags: `0` ×346, `2` ×76.
**Value `1` never appears in any shipped entity** — its behaviour is source-only.

### 5.6 Child elements

| Element | Content | C++ | Default | Read at | Notes |
|---|---|---|---|---|---|
| `<Sprite>` | text | `spriteFile` | `""` | `:274-282` | Basename; resolved under `entities/` (§8.6) |
| `<Normal>` | text | `normalFile` | `""` | `:284-292` | `entities/normalmaps/`; **loaded only if `richLighting`** (`ETHSpriteEntity.cpp:96-101`) |
| `<Gloss>` | text | `glossFile` | `""` | `:294-302` | `entities/`; **also gated on `richLighting`** (same block) |
| `<SpriteCut x y>` | **int** ×2 | `spriteCut` (`Vector2i`) | `(1,1)` | `:267` | **x = columns, y = rows** (`editor/EntityEditor.cpp:345/351`: "Sprite columns"/"Sprite rows") |
| `<Scale x y>` | float ×2 | `scale` | `(1,1)` | `:268` | Dimensionless multiplier; scales frame size, pivot **and** the physics box |
| `<PivotAdjust x y>` | float ×2 | `pivotAdjust` | `(0,0)` | `:269` | **Pixels, pre-scale** (§9.3) |
| `<EmissiveColor r g b a>` | float ×4 | `emissiveColor` (Vector4) | `(0,0,0,0)` | `:265` | Additive on the ambient diffuse; **only r,g,b are consumed** (`ETHRenderEntity.cpp:114-116`) — `a` is stored and round-tripped but no consumer was found |
| `<Light …>` | see §5.7 | `light` | none | `:335-350` | |
| `<Collision>` | see §5.8 | `collision`/`polygon`/`compoundShape`/`enmlJointDefinitions` | none | `:352-410` | |
| `<Particles>` | see §7.2 | `particleSystems` | empty | `:304-333` | Max 2 |
| `<CustomData>` | see §5.9 | | empty | `:411` | Always written, even empty |

Only `<Particles>` and `<CustomData>` appear in all 89 samples (as `<Particles />` / `<CustomData />` when
empty). Everything else is optional. Observed presence as a **direct child of `<Entity>`**: `PivotAdjust` 78,
`EmissiveColor` 71, `SpriteCut` **70** (85 total occurrences — 15 are inside `<ParticleSystem>`),
`Sprite` 64, `Collision` 64, `Light` 55, `Normal` 51, `Scale` 20, `Gloss` 18.

Canonical child order (a superset every observed sequence is a subsequence of):
`EmissiveColor > SpriteCut > Scale > PivotAdjust > Sprite > Normal > Gloss > Particles > Light > Collision > CustomData`.
27 distinct sequences occur; do not depend on order.

### 5.7 `<Light>`
Read `ETHLight::ReadFromXMLFile` (`ETHLight.cpp:40-63`); defaults from the ctor (`:27-38`), called as
`new ETHLight(true)` (`ETHEntityProperties.cpp:341`) — so **`active` defaults to true when absent**.

| Name | Where | Type | Default | Meaning |
|---|---|---|---|---|
| `active` | attr | int-bool | **`1`** | If false the whole light is **discarded** |
| `static` | attr | int-bool | `0` | **Read then unconditionally overwritten** with the entity's `static` (`ETHEntityProperties.cpp:349`) |
| `castShadows` | attr | int-bool | `0` | This light generates shadows from other entities |
| `range` | attr | float | `256.0` | Radius in world pixels; `<= 0` ⇒ light discarded |
| `haloBrightness` | attr | float | `1.0` | Halo alpha/intensity multiplier |
| `haloSize` | attr | float | `64.0` | Halo sprite size in pixels |
| `<Position x y z>` | child | float ×3 | `(0,0,0)` | **Offset relative to the entity origin** (`ETHEntity::GetLightPosition()` = `GetPosition() + light->pos`, `:636-642`) |
| `<Color r g b>` | child | float ×3 | `(1,1,1)` | Linear RGB, **no alpha** |
| `<HaloBitmap>` | child | text | `""` | Resolved under `entities/`; empty ⇒ `HasHalo()` false. **Not** gated on rich lighting |

Acceptance test (`ETHEntityProperties.cpp:335-350`): the light is kept only if `IsActive() && range > 0.0f`,
then `light->staticLight = staticEntity`. `documentation/Sample-Projects/Sample-project/entities/jazag.ent`
has `<Light active="0" …>` — parsed and thrown away.

All six attributes are present on all 91 `<Light>` elements corpus-wide; `<Position>` and `<Color>` on all 91;
`<HaloBitmap>` on 46.

### 5.8 `<Collision>`
Gated by the legacy `collidable` attribute (`ETHEntityProperties.cpp:352-356`):
```cpp
int nCollidable = 1;
const int r = pElement->QueryIntAttribute("collidable", &nCollidable);
if (nCollidable || r == TIXML_NO_ATTRIBUTE) { ... parse <Collision> ... }
```

`ETHCollisionBox::ReadFromXMLFile` (`engine/Physics/ETHCollisionBox.cpp:37-42`):

| Element | Attrs | Field | Default | Meaning |
|---|---|---|---|---|
| `<Position x y z>` | float ×3 | `collision.pos` | `(0,0,0)` | Box **centre** offset from the entity origin, world pixels |
| `<Size x y z>` | float ×3 | `collision.size` | `(0,0,0)` | **FULL extents, not half**, world pixels. `z` is used only by the legacy AABB test (`ETHCollisionBox::Collide`, `:74-103`); Box2D ignores it |

Both present on all 198 `<Collision>` elements corpus-wide.

**Zero-size discard** (`:367-371`): if `collision->size == (0,0,0)` the box is reset to null — *but*
`<Polygon>`, `<Compound>` and `<Joints>` are parsed **after** this reset, inside the same `if`. So
`collision` may be null while the XML still had a `<Collision>` element, and you must still walk the
sub-elements. (Corpus caveat: this branch is never actually reached — all 31 files with a zero-size
`<Collision>` also carry `collidable="0"`, and vice versa, so the `collidable` gate short-circuits first
every time.)

Sub-elements (`:373-407`), all ENML text (§6): `<Polygon>`, `<Compound>` — **mutually exclusive, `<Polygon>`
wins** (it is the `if`, `<Compound>` the `else`) — and `<Joints>`, stored verbatim in `enmlJointDefinitions`
and parsed lazily at runtime (`ETHPhysicsEntityController.cpp:254-260`).

**Box → Box2D** (`ETHPhysicsSimulator::CreateBody`, `:65-120`, verified in-source):
```cpp
bodyDef.type          = entity->IsStatic() ? b2_staticBody : b2_dynamicBody;   // :68
bodyDef.fixedRotation = entity->IsFixedRotation();                            // :70
ETHCollisionBox box   = entity->GetCollisionBox();
box.Scale(entity->GetScale());                     // :73  <- <Scale> IS baked into the body
bodyDef.position      = ScaleToBox2D(entity->GetPositionXY());                // :74-75
bodyDef.angle         = -(DegreeToRadian(entity->GetAngle()));                // :76
bodyDef.bullet        = entity->IsBullet();                                   // :77
bodyDef.gravityScale  = properties->gravityScale;                             // :78
fixtureDef.density    = entity->IsStatic() ? 0.0f : entity->GetDensity(idx);  // :107
```
- `ETHCollisionBox::Scale` multiplies `pos.x/y` and `size.x/y` by the entity `Scale` — **`z` untouched**
  (`ETHCollisionBox.cpp:105-111`).
- `DEFAULT_SCALE = 50.0f` px per Box2D unit (`:27`); `DEFAULT_GRAVITY = b2Vec2(0, 10)` (`:26`) — **+y is
  down in Box2D space too**, i.e. the mapping is a straight 1/50 scale with no axis flip.
- `BS_BOX`: half-extents `size/2` centred on `pos` (`:31-38`).
- `BS_CIRCLE`: radius `(size.x + size.y) / 4` — the average of the two half-extents (`:56-63`).
- `BS_POLYGON`: `vertex * (size/2) + pos` (`:40-54`).
- `BS_COMPOUND`: `GetShapes(ToVector2(box.size))` on the already-scaled box (`:92`).

### 5.9 `<CustomData>` — the gameplay-parameter store
`ETHCustomDataManager::ReadDataFromXMLFile` (`:510-608`) / `WriteDataToFile` (`:610-676`).

| enum (`ETHCustomDataManager.h:34-44`) | int | `<Type>` text | `<Value>` encoding | Parse |
|---|---|---|---|---|
| `DT_NODATA` | 0 | `""` | — | — |
| `DT_FLOAT` | 1 | `float` | element **text** | `sscanf("%f")` |
| `DT_INT` | 2 | `int` | element **text** | `sscanf("%d")` |
| `DT_UINT` | 3 | `uint` | element **text** | `sscanf("%u")` — **wraps negatives**: `"-1"` → `4294967295` |
| `DT_STRING` | 4 | `string` | element **text**, verbatim | — |
| `DT_VECTOR2` | 5 | `vector2` | attributes `x`,`y` (missing ⇒ 0) | `QueryFloatAttribute` |
| `DT_VECTOR3` | 6 | `vector3` | attributes `x`,`y`,`z` | `QueryFloatAttribute` |

**No bool, no colour, no array, no nested object.** The integer enum values never appear on disk — only the
lowercase names. Type-name table at `ETHCustomDataManager.cpp:27-36`; `ETHGlobal::Parse*` at
`engine/Util/ETHASUtil.cpp:143-162`.

```xml
<CustomData>
  <Variable><Type>float</Type>  <Name>mass</Name>       <Value>100.67</Value></Variable>
  <Variable><Type>int</Type>    <Name>pieces</Name>     <Value>-4</Value></Variable>
  <Variable><Type>uint</Type>   <Name>time</Name>       <Value>2000</Value></Variable>
  <Variable><Type>string</Type> <Name>material</Name>   <Value>wood</Value></Variable>
  <Variable><Type>vector2</Type><Name>testVector</Name> <Value x="1.000000" y="2.000000" /></Variable>
  <Variable><Type>vector3</Type><Name>gravity_center</Name>
                                <Value x="12.300000" y="3.000000" z="0.000001" /></Variable>
</CustomData>
```
(First five verbatim from `barrel_light.ent`; the `vector3` from `customDataTesting.esc` — the **only**
`vector3` in the corpus. The scalar-in-text / vector-in-attributes split holds across all 57 `<Variable>`
elements with zero exceptions.)

Reader details: iteration starts at the first `<Variable>` then walks `NextSiblingElement()` with **no name
filter**. A variable is stored only if `<Type>` and `<Name>` are both non-empty **and** a `<Value>` child
exists (`:552-556`). An unrecognised `<Type>` is silently discarded. Storage is
`std::map<string, ETHCustomDataPtr>` — **keys are unique (last wins) and the write path emits them in ASCII
alphabetical order**, not authoring order.

**Scene-level override:** in branch A (§4.4), the `.ent`'s custom data is deep-copied and the scene's
`<CustomData>` is merged **per key** — keys defined only in the `.ent` survive.

Script-side, custom data is the entity's variable bag (`ETHEntity.cpp:821-941`), with type-strict getters
(`GetFloat` on an `int` returns false). AngelScript objects set via `SetAngelScriptObject` live in a separate
dictionary and are **never serialized**.

### 5.10 Annotated real sample
`testbed/GeneralTests/entities/barrel_light.ent` (UTF-16LE+BOM; shown decoded, attribute order verbatim):

```xml
<?xml version="1.0" ?>
<Ethanon>
  <Entity type="2"             <!-- ET_VERTICAL: centre-bottom origin, y-sorted, non-rotatable -->
     static="0" shape="0"      <!-- BS_NONE => NO Box2D body -->
     startFrame="0"            <!-- LEGACY, ignored -->
     applyLight="1"            <!-- => specular* below ARE read -->
     castShadow="0"            <!-- => shadow* below are NOT read: shadowScale stays 0.0, not 0.9 -->
     sensor="0" bullet="0" fixedRotation="0"        <!-- shape==0 => NOT read -->
     blendMode="0"
     friction="1.000000" density="0.000000"          <!-- NOT read; effective friction 1.0, DENSITY 1.0 -->
     layerDepth="0.000000"
     soundVolume="1.000000"                          <!-- LEGACY, ignored -->
     shadowScale="0.900000" shadowLengthScale="1.000000" shadowOpacity="1.000000"   <!-- skipped -->
     specularPower="50.000000" specularBrightness="1.000000"                        <!-- READ -->
     restitution="0.000000" parallaxIntensity="1.000000" gravityScale="1.000000">   <!-- NOT read -->
    <EmissiveColor r="0.000000" g="0.000000" b="0.000000" a="0.000000" />
    <SpriteCut x="1.000000" y="1.000000" />   <!-- float spelling, int semantics => (1,1) -->
    <Scale x="1.000000" y="1.000000" />
    <PivotAdjust x="0.000000" y="-12.000000" />  <!-- px, PRE-scale; +y is down => moves art DOWN 12px -->
    <Sprite>barril.png</Sprite>               <!-- entities/barril.png -->
    <Normal>barril_nm.bmp</Normal>            <!-- entities/normalmaps/barril_nm.bmp -->
    <Particles>
      <ParticleSystem particles="14" …>       <!-- kept: nParticles > 0 -->
        <Bitmap>explosion.JPG</Bitmap>        <!-- particles/explosion.JPG -->
        <StartPoint x="0.000000" y="10.000000" z="48.000000" /> …
      </ParticleSystem>
    </Particles>
    <Light active="1" static="0"              <!-- static: read then DISCARDED, forced to entity's -->
           castShadows="0" range="116.500000" haloBrightness="1.000000" haloSize="64.000000">
      <Position x="0" y="0" z="40.000000" />  <!-- RELATIVE to the entity -->
      <Color r="1.000000" g="0.600000" b="0.400000" />
    </Light>                                  <!-- no <HaloBitmap> => HasHalo() false -->
    <!-- no <Collision> at all => collision null, IsCollidable() false -->
    <CustomData>… mass/material/pieces/testVector/time, alphabetical …</CustomData>
  </Entity>
</Ethanon>
```

Resulting in-memory state — note the four fields whose on-disk values are **discarded** by the gates:
`shadowScale = 0.0` (not 0.9), `friction = 1.0`, `density = 1.0` (not 0.0), `restitution = 0.0`,
`gravityScale = 1.0`, `sensor = bullet = fixedRotation = false`.

---

## 6. ENML — the embedded physics blobs (and `app.enml`)

ENML ("Easy Non-Markup Language", `gs2d/src/Enml/Enml.h:43`) is a flat `name { key = value; … }` format.
Two classes: `enml::Entity` (`map<string,string>`) and `enml::File` (`map<string, Entity>`), both
`std::map` ⇒ **on-disk order is lost, keys sort alphabetically**.

The same parser serves `app.enml` (§8) and the text nodes of `<Polygon>`, `<Compound>`, `<Joints>` inside
**both** `.ent` and `.esc` files (`ETHEntityProperties.cpp:373-407`).

### 6.1 Grammar (`File::ParseString`, `Enml.cpp:313-448`)

```
file      := ( ws | comment | entity )*
entity    := name ws* '{' body '}'
body      := ( ws | comment | attribute )*
attribute := name ws* '=' ws* value ';'
name      := [A-Za-z0-9_]*                      # Enml.cpp:97-101 — ONLY these bytes
value     := 1+ bytes up to the next unescaped ';'
escape    := '\;' -> ';'  |  '\\' -> '\'        # THE ONLY TWO
comment   := '/' … '\n'   (ONE slash starts it) |  '/*' … '*/'  (newline does NOT close it)
ws        := 0x20 0x09 0x0A 0x0D                # Enml.cpp:91-94
```

Rules a Python reader must replicate (`File::ReadValue`, `Enml.cpp:645-671`):
- Value runs from the first non-neutral byte after `=` to the next unescaped `;`. Newlines, `}`, `{`, `/`,
  `=` are all ordinary value bytes. Comments are **not** recognised inside a value.
- **Leading** whitespace after `=` is skipped; **trailing** whitespace before `;` is **preserved**
  (`title = Foo ;` yields `"Foo "`).
- Any backslash not followed by `;` or `\` **aborts the whole parse** — a raw Windows path in a value is a
  parse error, not a quirk.
- **Empty values are impossible** (`key = ;` is an error), which is why `Get() == ""` unambiguously means
  "attribute absent" and why every engine consumer tests `value != ""`.
- On **any** error the parser calls `Clear()` — the entire file is discarded — and returns the 1-based line
  number. `ETHAppEnmlFile` treats that as "use all defaults" (`ETHAppEnmlFile.cpp:57-73`).
- Running off the end of the buffer is **not** an error, except **inside a value** (`:670`). A pending
  uncommitted entity is silently lost.
- `AddEntity` (`:253-265`) runs `if (Exists(key)) m_entities[key].Clear();` **unconditionally**, then copies
  attributes. So a first-time entity with an **empty body creates no entity at all**, and a later duplicate
  with an empty body **empties** the earlier one.
- Duplicate attribute in one entity: last wins.
- Names may legally be **empty** (`IsValid("")` returns valid), and `}` is not a name terminator, so `foo}`
  parses as a name. Neither occurs in real data; a strict parser may reject non-`[A-Za-z0-9_]` names.
- `.enml` files are ASCII with **no BOM** — a BOM would make the first entity name invalid and kill the parse.

Typed getters (`Enml.cpp:495-549`) use `sscanf` `%lf`/`%d`/`%u`/`%f`, return `false` on missing or
unparseable input and leave the out-parameter untouched. `"1.5abc"` parses as `1.5` and succeeds.

### 6.2 `<Polygon>` (`engine/Physics/ETHPolygon.cpp:26-41`)
Vertices must be named `v0`, `v1`, `v2`, … **contiguously** (`GetEntityName`, `:63-67` — literally `"v" << v`);
the loop stops at the first gap. Keys `x`, `y`, `%f`, default 0. Coordinates are **normalized to −1..1** of
the collision box; the engine computes `vertex * (size/2) + pos`. Valid only with ≥ 3 vertices (`:43-46`);
must be convex and clockwise (comment in the editor template, `ETHEntityProperties.cpp:60-62`).

The corpus's only `<Polygon>`, `testbed/GeneralTests/entities/half_metal_crate.ent:15` (`shape="3"`),
proving newlines are not syntax:
```
v0{x=1;y=-1;} v1{x=1;y=1;} v2{x=-1;y=1;} 
```

### 6.3 `<Compound>` (`engine/Physics/ETHCompoundShape.cpp:26-116`)
One ENML entity per Box2D fixture. `scale` below is the **already-`<Scale>`-multiplied** collision-box size
passed in from `CreateBody`.

| Key | Type | Used when | Meaning |
|---|---|---|---|
| `shape` | string | always | `box` or `circle`. Anything else ⇒ `GetShape` returns `0` |
| `posX`, `posY` | float | both | Centre offset, **normalized −1..1** → `posX * boxSize.x * 0.5`, `posY * boxSize.y * 0.5` |
| `sizeX`, `sizeY` | float | `box` | Full extents, normalized → `sizeX * boxSize.x * 0.5`, `sizeY * boxSize.y * 0.5` (halved again inside `GetBoxShape`) |
| `radius` | float | `circle` | Normalized; radius = `radius * boxSize.x * 0.5` (**x only**, not an average), then used as a `radius*2` square box |
| `angle` | float | `box` | Passed **raw and unnegated** to `b2PolygonShape::SetAsBox(hx,hy,center,angle)` ⇒ **Box2D radians — INFERRED from the API signature, never exercised: all five shipped blocks use `angle = 0`** |
| `density`, `friction`, `restitution` | float | optional | Per-fixture override; falls back to the entity value only when the key is absent or empty (`GetIndividualProperty`, `:89-102`) |

**Ordering caveat:** the ctor copies out of a `std::map`, so fixture index order is **alphabetical by ENML
entity name**, not file order. `ushape.ent`'s `shape_bottom` / `shape_left` / `shape_right` become indices
0/1/2 in that order — which is what `GetIndividualFriction(idx)` keys on.

**Crash hazard:** an unsupported `shape` key makes `GetShape` return `0`, and `GetShapes` (`:79-87`) still
pushes that null into the vector, which `CreateBody` assigns to `b2FixtureDef::shape`.

Real sample, `testbed/GeneralTests/entities/lollipop.ent:13-31` (CRLF and tabs preserved from the XML text node):
```
candy { shape = circle; posX = 0.0; posY =-0.5; radius = 1.0; angle = 0; }
stick { shape = box; sizeX = 0.1; sizeY = 2.0; posX = 0.0; posY = 0.0; angle = 0; }
```
(shown collapsed; the file is multi-line). `ushape.ent` uses the CDATA + per-shape-override variant:
`shape_left {… density = 1.0; friction = 1.0; }`, `shape_right {… density = 0.2; friction = 0.0; }`,
`shape_bottom {… restitution = 1.0; }`. `physicsTest.esc:117` is the one-line CDATA variant.
Note `posY =-0.5;` — no space after `=`, negative number — is normal.

### 6.4 `<Joints>` (`engine/Physics/ETHJoint.cpp`)
Only revolute joints exist. The ENML entity name must **start with** `"revoluteJoint"` (prefix test,
`ETHJoint::IsRevoluteJoint`, `:65-68`); shipped names are `revoluteJoint`, `revoluteJoint0`, `revoluteJoint1`,
`revoluteJointA`.

| Key | Type | Default | Source |
|---|---|---|---|
| `otherEntityName` | string | `""` | `:60-63` |
| `attachPointAX`, `attachPointAY` | float | `0.0` | `:44-50`; normalized −1..1 of entity A's scaled collision box (`ETHRevoluteJoint.cpp:60-64`: `boxSize = collisionBox.size.xy * scale * 0.5`, anchor = `anchorPoint * boxSize`) |
| `attachPointBX`, `attachPointBY` | float | `0.0` | `:52-58` |
| `enableLimit` | bool-string | false | `:70-73` → `ETHGlobal::IsTrue` |
| `enableMotor` | bool-string | false | `:75-78` |
| `lowerAngle` | float (radians) | `0.0` | `:87-92` |
| `upperAngle` | float (radians) | **`PI2`** | `:80-85` |
| `maxMotorTorque` | float | `100.0` | `:94-99` |
| `motorSpeed` | float | `0.0` | `:101-106` |

`ETHGlobal::IsTrue` (`engine/Util/ETHASUtil.cpp:96-102`) accepts exactly `"true" "TRUE" "yes" "YES" "1"` —
`"True"` and `"on"` are false.

**Cross-link to custom data:** if `otherEntityName` is absent, the engine looks up a *custom-data variable
whose key equals the joint's ENML entity name* (`ETHPhysicsEntityController.cpp:266-283`): a `string` value
names the other entity, an `int` value gives its scene id; neither ⇒ the joint is skipped. Both forms ship:
`spinning_cross.ent` uses the fallback (`<Joints>revoluteJoint{…}` + custom `string revoluteJoint = "tower"`),
`ushape_joint.ent` uses the direct form (`otherEntityName = anchor;` / `= meteor;`).

Keys exercised by shipped data: `attachPointAX/AY/BX/BY`, `otherEntityName`, `enableMotor`, `motorSpeed`,
`maxMotorTorque`. **`enableLimit`, `upperAngle`, `lowerAngle` are source-verified but never used in any
sample.** No sample uses a key the source does not read.

---

## 7. `.par` — particle systems

### 7.1 Container and the two carriers
UTF-16LE+BOM XML, all 33 shipped files. `ETHParticleSystem::ReadFromXMLFile` (`:89-136`) is the **only**
deserializer, fed from two places:

**(a) Standalone `.par`** — `ReadFromFile` (`:138-156`): first element child of the document, then its first
element child. **No sibling loop — a `.par` holds exactly one system**, even if more are present.
Loaded at runtime from `effects/` (`ETHParticleDrawer.cpp:43`), but that path **rejects endless systems**:
`IsAlive()` (`:68-76`) logs *"Only temporary particle effects are allowed"* when `repeat <= 0`.

**(b) Embedded in a `.ent`/`.esc`** — `<Entity><Particles><ParticleSystem …>`
(`ETHEntityProperties.cpp:304-333`). Max **`ETH_MAX_PARTICLE_SYS_PER_ENTITY` = 2** (`ETHTypes.h:33`); a third
is silently dropped. Systems are **appended** to `particleSystems`, which `Reset()` does not clear.

**There is no path reference from a `.ent` to a `.par`.** The editor loads a `.par` through a file dialog
(`editor/EntityEditor.cpp:765/777/1561`) and copies the data into the entity; on save it is re-serialized
inline (`ETHEntityProperties.cpp:468-477`). Verified: no `.par` basename appears anywhere inside any `.ent`
or `.esc`. **However `.par` files are still runtime assets** — AngelScript loads them by name:
`testbed/GeneralTests/Test/TestSceneScale.angelscript:93` `PlayParticleEffect("fireball.par", …)`.
A converter that only ports scenes/entities can skip `effects/`; one that also ports scripts cannot.

Corpus: 70 `<ParticleSystem>` total — 33 standalone, 21 in `.ent` (13 files ×1, **4 files ×2**:
`Sample-project/entities/explosion.ent`, `.../fire_ct.ent`, `testbed/.../explosion1.ent`, `explosion2.ent`),
16 in `.esc`.

### 7.2 Complete field table
Defaults from `ETHParticleSystem::Reset()` (`:67-87`), called from the ctor before every read — **every field
is optional**. "Scaled?" marks the eleven fields multiplied by entity scale (§7.4).

**Attributes on `<ParticleSystem>` (17):**

| XML | C++ | Type | Default | Scaled | Units / meaning |
|---|---|---|---|---|---|
| `particles` | `nParticles` | int | `0` | no | **Fixed pool size, not a spawn rate.** `<= 0` ⇒ system dropped (at load `:323`, at write `:474`, at runtime `ETHParticleManager.cpp:78-83`). Shipped 1…300 |
| `allAtOnce` | `allAtOnce` | `ETH_BOOL` | `false` | no | `1` ⇒ all released frame 1; `0` ⇒ staggered (§7.3). Read via `ReadBooleanPropertyFromXmlElement` ⇒ any non-zero is true |
| `alphaMode` | `alphaMode` | int enum | `0` `AM_PIXEL` | no | §5.5. Shipped: `1` ×55, `0` ×15 only |
| `repeat` | `repeat` | int | `0` | no | Life cycles per particle. **`0` = endless** (`IsEndless()` is `repeat == 0`). Shipped: `0` ×36, `1` ×13, **`8` ×5** |
| `animationMode` | `animationMode` | int enum | **`1`** `PLAY_ANIMATION` | no | `1` = play sheet once over lifetime; `2` = `PICK_RANDOM_FRAME` (`ETHParticleSystem.h:33-37`). Shipped `1` ×40, `2` ×2 |
| `boundingSphere` | `boundingSphere` | float | `512.0` | **yes** | **Diameter**, px. Culling only (`GetBoundingRadius() = /2`); no visual effect |
| `lifeTime` | `lifeTime` | float | `0.0` | no | **Milliseconds**, mean |
| `randomLifeTime` | `randomizeLifeTime` | float | `0.0` | no | **ms, FULL peak-to-peak width** ⇒ actual = `lifeTime ± r/2` |
| `angleDir` | `angleDir` | float | **NONE — `Reset()` never assigns it** | no | Angular velocity, deg/frame @60 ⇒ ×60 = deg/s. Use 0.0 and warn if absent |
| `randAngle` | `randAngle` | float | **NONE — same** | no | Full width of angular-velocity jitter |
| `size` | `size` | float | `1.0` | **yes** | **Absolute on-screen pixels**, applied to *both* axes (square quad, §7.5) |
| `randomizeSize` | `randomizeSize` | float | `0.0` | **yes** | Full width of start-size jitter, px |
| `growth` | `growth` | float | `0.0` | **yes** | px/frame @60 ⇒ ×60 = px/s. Negative shrinks |
| `minSize` | `minSize` | float | `0.0` | **yes** | Lower clamp, applied every frame |
| `maxSize` | `maxSize` | float | **`99999.0`** | **yes** | Upper clamp. (The editor habitually writes `1000`, which is *not* the default) |
| `angleStart` | `angleStart` | float | `0.0` | no | Initial sprite rotation, degrees |
| `randAngleStart` | `randAngleStart` | float | `0.0` | no | **ASYMMETRIC** jitter `U(0, randAngleStart)`, degrees |

**Child elements (10):**

| Element | Attrs | C++ | Default | Scaled | Units / meaning |
|---|---|---|---|---|---|
| `<Bitmap>` | text | `bitmapFile` | `""` ⇒ `"particle.png"` (`ETHTypes.h:38`) | no | Basename, resolved under `particles/` |
| `<Gravity>` | `x y` | `gravityVector` | `(0,0)` | **yes** | px/frame² @60 ⇒ **×3600 = px/s²** |
| `<Direction>` | `x y` | `directionVector` | `(0,0)` | **yes** | Mean initial velocity, px/frame @60 ⇒ ×60 = px/s |
| `<RandomizeDir>` | `x y` | `randomizeDir` | `(0,0)` | **yes** | **Full width of a rectangular, per-axis-independent velocity box** (§10.5) |
| `<StartPoint>` | `x y z` | `startPoint` | `(0,0,0)` | **yes** | Emitter offset from the entity origin, px. `z` also drives depth sorting and screen projection |
| `<RandStartPoint>` | `x y` | `randStartPoint` | `(0,0)` | **yes** | Full width of the emitter-position box, px (**no z**) |
| `<SpriteCut>` | `x y` | `spriteCut` (`Vector2i`) | `(1,1)` | no | x = columns, y = rows; `GetNumFrames() = x*y` |
| `<Color0>` | `r g b a` | `color0` | **`(0,0,0,0)`** | no | Colour at birth. Note the default is transparent black — a file omitting it renders nothing (all 33 samples provide both) |
| `<Color1>` | `r g b a` | `color1` | **`(0,0,0,0)`** | no | Colour at death, lerped linearly over normalized age |
| `<Luminance>` | `r g b` | `emissive` | `(1,1,1)` | no | **Self-illumination — not inert.** See §7.6 |

Presence over the 33 `.par`: all 16 core attributes and all 9 core children at 33/33; `animationMode` and
`<SpriteCut>` at 11/33 (the *same* 11 files — the current writer emits both unconditionally);
`<SoundEffect>` at 4/33.

**Legacy `<SoundEffect>`** (e.g. `testbed/GeneralTests/effects/fire_wsound.par` →
`<SoundEffect>fire.ogg</SoundEffect>`): there is **no `soundFX` member on `ETHParticleSystem`** and
`ReadFromXMLFile` never looks for it. Dead data; retain only as a designer hint.

### 7.3 Runtime semantics needed to interpret the numbers
**Emission:** `nParticles` is a fixed recycled pool. Particle `id = t` is released when
(`ETHParticleManager.cpp:222-230`):
```cpp
releaseTime = (lifeTime + randomizeLifeTime) * (particle.id / (float)nParticles);
if (particle.elapsed > releaseTime || allAtOnce) { … release … }
```
so the pool fills evenly over **`lifeTime + randomLifeTime`** (the *sum*), then recycles.

**Birth randomization** (`ResetParticle` `:473-505`, `PositionParticle` `:507-523`):
```cpp
angleDir  = system.angleDir  + Rand(-randAngle/2,          +randAngle/2);
lifeTime  = system.lifeTime  + Rand(-randomizeLifeTime/2,  +randomizeLifeTime/2);
size      = system.size      + Rand(-randomizeSize/2,      +randomizeSize/2);
dir.x     = directionVector.x + Rand(-randomizeDir.x/2, +randomizeDir.x/2);   // per-axis, independent
dir.y     = directionVector.y + Rand(-randomizeDir.y/2, +randomizeDir.y/2);
dir       = Multiply(dir, rotMatrix);                                   // :489
angle     = angleStart + Rand(randAngleStart) + entityAngle;            // :517
pos       = startPoint + Rand(±randStartPoint/2);
pos       = Multiply(pos, rotMatrix);                                   // :520
```
`Randomizer::Float(min,max)` is uniform inclusive; `Float(max)` is `[0,max]`
(`gs2d/src/Math/Randomizer.cpp:44-55`). Hence **`randAngle`, `randomizeLifeTime`, `randomizeSize`,
`randomizeDir`, `randStartPoint` are FULL peak-to-peak widths** (the code halves them) while
**`randAngleStart` is asymmetric** `U(0,max)` — `angleStart=0, randAngleStart=350` spans 0…350°, not ±175°.

**The owning entity's angle rotates the emitter, not just the sprite.**
`rotMatrix = RotateZ(DegreeToRadian(entityAngle))` is applied to the jittered velocity (`:489`) *and* the
jittered start offset (`:520`), and the raw angle is added to each particle's sprite rotation (`:517`).

**Unit derivation** (`UpdateParticleSystem`, `:188-267`):
```cpp
frameSpeed = (min(lastFrameElapsedTime, 250.0f) / 1000.0) * 60.0;   // == 1.0 at 60 fps
particle.elapsed += lastFrameElapsedTime;         // NOTE: UNCAPPED
dir   += gravityVector * frameSpeed;
pos   += dir          * frameSpeed;
angle += angleDir     * frameSpeed;
size  += growth       * frameSpeed;
color  = color0 + (color1 - color0) * (elapsed / lifeTime);
size   = clamp(size, minSize, maxSize);
```
⇒ **velocity = `directionVector` × 60 px/s; acceleration = `gravityVector` × 3600 px/s²;
growth × 60 px/s; angular = `angleDir` × 60 deg/s; lifetimes in ms.**
Two subtleties worth preserving as comments: `elapsed` accumulates the *uncapped* delta while motion uses the
250 ms-capped one (on a hitch, particles age faster than they move); and `w = elapsed/lifeTime` is unclamped
but the particle resets as soon as `elapsed > lifeTime`.

**Sprite-sheet animation:** `PLAY_ANIMATION` plays the sheet exactly once over the lifetime
(`currentFrame = min(int(numFrames * w), numFrames-1)`, `:247-249`), reset to 0 at birth.
`PICK_RANDOM_FRAME` picks once at birth via `Randomizer::Int(nFrames-1)` and holds it (`:500-503`).
Both are skipped unless `spriteCut.x > 1 || spriteCut.y > 1`.

**Mirroring** (`ETHParticleSystem::MirrorX/MirrorY`, `:42-60`): negates `startPoint`, `directionVector`,
`randomizeDir`, `randStartPoint` on the chosen axis, and `gravityVector` only if `mirrorGravity`.
`ETHParticleManager::MirrorX/Y` (`:453-471`) also negates each live particle's `dir` and `pos`.
If the game mirrors entities, replicate by negating these fields — **not** by setting a node `scale.x = -1`.

### 7.4 Entity scale propagates into particle values
`ETHSpriteEntity::LoadParticleSystem` (`:293-322`, scale at `:316`):
`particleScale = (GetScale().x + GetScale().y) / 2.0f` → `m_system.Scale(scale)`
(`ETHParticleManager.cpp:90` → `ETHParticleSystem.cpp:27-40`), which multiplies exactly:
`boundingSphere, gravityVector, directionVector, randomizeDir, startPoint, randStartPoint, size,
randomizeSize, growth, minSize, maxSize`.

**Not scaled:** `lifeTime, randomizeLifeTime, angleDir, randAngle, angleStart, randAngleStart, color0,
color1, emissive, nParticles, repeat, spriteCut, animationMode, allAtOnce`.

Consequences: non-uniform entity scale is **flattened to a scalar mean**; the `.ent` stores **pre-scale**
values (the properties copy is never mutated — the mutating lines are commented out at
`ETHSpriteEntity.cpp:516` and `ETHEntity.cpp:601-608`), so a converter reading static files must
pre-multiply itself. **Currently a no-op in every sample** — no shipped `.ent` that owns a particle system has
a non-`(1,1)` `<Scale>` — but implement it anyway.

### 7.5 `size` is absolute pixels on a square quad
`DrawParticleSystem` sets `Sprite::EO_CENTER` (`:366`), selects `SetRect(currentFrame)` when the cut is
> 1×1 (`:397-402`), and draws with `Vector2(particle.size, particle.size)` (`:413`), which backends pass
through as the **absolute pixel quad size**. So a non-square **frame** (not bitmap) gets squashed.

Measured across all 54 systems in shipped data, every non-square texture cuts to square frames
(`fire.png` 512×128 ÷ 4×1 = 128×128; `explosion.png` 256×192 ÷ 4×3 = 64×64). **The only genuinely non-square
frame is `flash.bmp` (265×253, no `<SpriteCut>`)**, used by `bluegeiser.par` and `purple_haze.par` — a ~4.7%
horizontal squeeze. A uniform `scale = size / frame_dimension` is therefore correct for everything else.

### 7.6 `Luminance` is live self-illumination
`ETHParticleManager.cpp:381-389`:
```cpp
Vector3 finalAmbient(1,1,1);
if (alphaMode == AM_PIXEL || alphaMode == AM_ALPHA_TEST)
    finalAmbient = min(emissive + sceneAmbient, 1);
finalColor = particle.color * Vector4(finalAmbient, 1);
```
`(1,1,1)` ⇒ ignores scene ambient (full bright); `(0,0,0)` ⇒ fully ambient-modulated. **Dead under `AM_ADD`.**
Cross-tab over 54 shipped systems: `(1,1,1)`+ADD 38 (inert), **`(0,0,0)`+PIXEL 14 (LIVE)**, `(1,1,1)`+PIXEL 1,
`(0,0,0)`+ADD 1. So it cannot be dropped — 14 systems genuinely darken with scene ambient.

### 7.7 Texture side effects and shaders
- `alphaMode == AM_ADD` changes how the texture is loaded: `graphics->GetPointer(…, cutOutBlackPixels =
  (alphaMode == AM_ADD))` (`ETHParticleManager.cpp:107-108`, `ETHSpriteEntity.cpp:313`). This is why
  `explosion.JPG` works as a particle — a JPEG has no alpha, and additive blending makes black transparent.
- **Missing texture ⇒ the whole system is skipped**, leaving a null slot that still consumes its index
  (`ETHSpriteEntity.cpp:313-314`). Deliberate fixture: `effects/bitmap_not_to_be_found.par`.
- **The converter does not need to model shaders.** The only path from particle data to a shader is
  `ShouldUseHighlightPS()` (`ETHParticleSystem.cpp:204-214`) = any `color0`/`color1` channel > 1.0, which
  swaps in a trivial `oColor = tex2D(diffuse, uv) * highlight` shader. **Verified false for all 54 shipped
  systems** (no channel exceeds 1.0). Shaders live only in `toolkit/Source/src/shaders/{Cg,GLSL}/` and are
  compiled into the executable as string literals (`ETHShaderManager.cpp:47`) — **`effects/` contains only
  `.par` files, and no shader source ships in any project directory.**

### 7.8 Complete real sample
`testbed/GeneralTests/effects/fire_wsound.par`:
```xml
<?xml version="1.0" ?>
<Ethanon>
     <ParticleSystem particles="14" allAtOnce="0" alphaMode="1" repeat="0" animationMode="1"
        boundingSphere="512.000000" lifeTime="500.000000" randomLifeTime="350.000000"
        angleDir="0.000000" randAngle="0.000000" size="30.000000" randomizeSize="0.000000"
        growth="-1.000000" minSize="0.000000" maxSize="1000.000000" angleStart="0.000000"
        randAngleStart="350.000000">
          <SoundEffect>fire.ogg</SoundEffect>       <!-- LEGACY, no reader -->
          <Bitmap>explosion.JPG</Bitmap>
          <Gravity x="0.000000" y="0.000000" />
          <Direction x="0.000000" y="-1.400000" />  <!-- y-down space => -1.4 rises -->
          <RandomizeDir x="0.600000" y="0.200000" />
          <SpriteCut x="1.000000" y="1.000000" />
          <StartPoint x="0.000000" y="0.000000" z="0.000000" />
          <RandStartPoint x="0.000000" y="0.000000" />
          <Color0 r="1.000000" g="0.700000" b="0.700000" a="1.000000" />
          <Color1 r="1.000000" g="0.000000" b="0.000000" a="1.000000" />
          <Luminance r="1.000000" g="1.000000" b="1.000000" />
    </ParticleSystem>
</Ethanon>
```

---

## 8. Project layout, `app.enml`, asset resolution, density

### 8.1 Canonical directories (`engine/Resource/ETHDirectories.cpp:25-33`)
```cpp
#define ETH_SHADER_DIRECTORY       GS_L("data/")
#define ETH_BITMAP_FONT_DIRECTORY  GS_L("data/")
#define ETH_SCENES_DIRECTORY       GS_L("scenes/")
#define ETH_ENTITY_DIRECTORY       GS_L("entities/")
#define ETH_NORMAL_DIRECTORY       GS_L("entities/normalmaps/")
#define ETH_HALOS_DIRECTORY        GS_L("entities/")     // NOT a separate folder
#define ETH_PARTICLES_DIRECTORY    GS_L("particles/")
#define ETH_EFFECTS_DIRECTORY      GS_L("effects/")
#define ETH_SOUNDFX_DIRECTORY      GS_L("soundfx/")
```
All relative with a trailing slash, concatenated onto the resource root.

**Do not invert `particles/` and `effects/`: `.par` definition files live in `effects/`; particle *textures*
live in `particles/`.** Confirmed by the shipped readmes (`effects/readme.txt`: "All \*.PAR effect files must
be in this folder."; `particles/readme.txt`: "All particle bitmaps must be in this folder.").

`GetSoundFXDirectory()` has **zero call sites** anywhere in `toolkit/Source/src` — audio loads from a
script-supplied path concatenated onto the resource root (`ETHScriptWrapper.Audio.cpp:30-33, 46-49`), so the
sound folder is a script convention. Audio is the **one exception** to §8.6: subdirectories are preserved.

**Resource root** (`gs2d/src/Platform/FileIOHub.cpp:45-48`): the project directory holding `app.enml`.
On Windows it defaults to the `.exe` folder (`WindowsFileIOHub.cpp:44-57`; `machine.exe` accepts `dir=<path>`).
**On Android it is the literal string `assets/` inside the APK** (`AndroidFileIOHub.cpp:28-37`, with a
`ZipFileManager` over the APK). So in an extracted APK look in `assets/app.enml`, `assets/scenes/*.esc`,
`assets/entities/`, `assets/particles/`, `assets/effects/`, `assets/data/`. Android also disables the
encrypted zip-pack path (`IsResourcePackingSupported()` false; `EnablePackLoading` is `#ifdef _MSC_VER`), but
lightmaps may live in an OBB expansion file (`EnableLightmapsFromExpansionPack`,
`ETHScriptWrapper.System.cpp:290-318`).

`*.ethproj` is a **marker file with no payload** — 27 bytes containing exactly
`Ethanon Engine project file` (or 0 bytes). `ProjectManager::Save` is a stub. Project configuration lives in
`app.enml`.

### 8.2 `app.enml` — load order
`ETHAppEnmlFile` (`engine/Platform/ETHAppEnmlFile.cpp:53-74`) merges, each pass overriding the previous:
**`default` → `window` (legacy alias) → `<platformName>`**, where the platform entity is
`windows` / `android` / `ios` / `macosx` (`gs2d/src/Platform/*/Platform.*.cpp`). A missing entity is skipped.
The file is read with `GetAnsiFileString` — raw bytes with **every `0x0D` stripped**, no charset conversion
(`FileManager.cpp:122-142`). Python equivalent:
```python
text = open(path,'rb').read().replace(b'\r', b'').decode('latin-1')
```

### 8.3 `app.enml` key table (defaults from the ctor initialiser list, `:39-52`)

| Key | Parse | Units | Default | Meaning |
|---|---|---|---|---|
| `width` | `boost::lexical_cast<unsigned>` (`:92-109`) | px | `640` | **On cast failure the field is set to `0`, not the default** |
| `height` | same | px | `480` | Same failure mode |
| `windowed` | `IsTrue` | bool | `true` | |
| `vsync` | `IsTrue` | bool | `true` | |
| `richLighting` | `IsTrue` | bool | `true` | **Gates `<Normal>`/`<Gloss>` loading entirely** |
| `title` | string | — | `"Ethanon Engine"` | Only overwritten when non-empty |
| `fixedWidth` | string (`"auto"`/empty allowed) | px | `""` = auto | §8.4 |
| `fixedHeight` | string | px | `""` = auto | §8.4 |
| `hdDensityValue` | `%f` | factor | `2.0` | Density stamped on sprites from `hd/` |
| `fullHdDensityValue` | `%f` | factor | `4.0` | `fullhd/` |
| `ldDensityValue` | `%f` | factor | `0.5` | `ld/` |
| `xldDensityValue` | `%f` | factor | `0.25` | `xld/` |
| `minScreenHeightForHdVersion` | `%u` | px | `720` | |
| `minScreenHeightForFullHdVersion` | `%u` | px | `1080` | |
| `maxScreenHeightBeforeNdVersion` | `%u` | px | `480` | drives the **LD** test |
| `maxScreenHeightBeforeLdVersion` | `%u` | px | `320` | drives the **XLD** test |
| `definedWords` | comma-split | — | `[]` | **UNION across all three passes**, sorted and de-duplicated (`:142-147`) — every other key is override-only |

Numeric parsing is **not uniform**: `width`/`height` use `lexical_cast` (whole-string, zeroes on failure);
all density/threshold keys use `sscanf` (lenient, keeps the default). Since ENML preserves trailing
whitespace before `;`, `width = 800 ;` behaves differently from `hdDensityValue = 2.0 ;`.
`Platform::SplitString("", ",")` returns `[""]`, so an entity lacking `definedWords` still contributes an
empty string — drop empties.

Across all 8 shipped `.enml` the only keys used are `title richLighting width height windowed vsync
definedWords fixedHeight` (plus editor-only `lightmapsEnabled renderMode showCustomData`), and the only
entities are `default windows macosx android ios window rendering status`.
**No shipped file uses `fixedWidth` or any density key.**

### 8.4 `fixedWidth` / `fixedHeight` — the virtual coordinate space
These define the backbuffer the whole game renders into, i.e. **the coordinate space scene entity positions
live in**. `ETHBackBufferTargetManager` (`:55-84`, `:100-136`); `IsAuto(str)` = `str == "auto" || empty`.

| `fixedWidth` | `fixedHeight` | `bufferSize` |
|---|---|---|
| auto | auto | `screenSize` (1:1) |
| auto | `H` | `( ceil(H * screenW/screenH), H )` — height-locked |
| `W` | auto | `( W, ceil(W * screenH/screenW) )` — width-locked |
| `W` | `H` | `(W, H)` literally |

Then the componentwise clamp `bufferSize = Min(screenSize, bufferSize)` runs **unconditionally** (`:64-65`) —
the requested virtual resolution is not guaranteed on a smaller physical screen. `m_targetScale =
bufferSize.x / screenSize.x` (`:79`). Parse asymmetry: the "this side" value is read with `%i` (so `0x`/`0`
prefixes are hex/octal) and the "other side" with `%f`; both fall back to `-1`.

The canonical mobile config, `documentation/Sample-Projects/Sample-project/app.enml`:
`default { width = 1024; height = 768; }`, `android { fixedHeight = 480; }`, `ios { fixedHeight = 320; }`.
**Note:** `toolkit/Source/src/gs2d/projects/Android/Ethanon/assets/app.enml` is a *different, 12-line* file
with only `default` and `windows` — no `fixedHeight` at all. The value must come from the game's own `app.enml`.

### 8.5 Density variants — a folder inserted before the basename
⚠ **2026-09-17: three statements below are wrong for Magic Portals** — the thresholds 1080 / 720 are
the engine's defaults, not the game's (480 / 600); "the same world size as its 1× counterpart" fails for
four `fullhd/` skies; and the converter now copies the tier file per texture. See "2026-09-17 (tiers)"
near the top of this document; where the two disagree, that block wins.

`ETHSpriteDensityManager` (`:30-33`, `:81-86`, `:88-139`). **There is no filename suffix convention.**
```cpp
AssembleResourceName(fullPath, versionPathName) = GetFileDirectory(fullPath) + versionPathName + GetFileName(fullPath)
// "hd/" "fullhd/" "ld/" "xld/"
```
`entities/barril.png` → `entities/hd/barril.png`; `entities/normalmaps/x.png` → `entities/normalmaps/hd/x.png`.

Selection uses the **physical** `video->GetScreenSize().y`, each tier guarded by a per-file `FileExists`:
`>= minScreenHeightForFullHdVersion` (1080) → try `fullhd/` then `hd/`; `>= minScreenHeightForHdVersion`
(720) → `hd/`; `<= maxScreenHeightBeforeLdVersion` (320) → `xld/` then `ld/`;
`<= maxScreenHeightBeforeNdVersion` (480) → `ld/`; otherwise NORMAL. Enum values (`.h:39-46`):
`NORMAL=0, HD=1, FULL_HD=2, LD=3, XLD=4`. (Note the confusing pairing — the XLD test uses
`maxScreenHeightBeforeLdVersion` and the LD test uses `maxScreenHeightBeforeNdVersion`; that is what the code
says at `:71-79`.)

**Density is resolved per file, not per project** — a project can ship `entities/hd/a.png` but no
`entities/hd/b.png`, and the same scene then mixes D=2 and D=1 sprites.

**Critical side effect:** `SetSpriteDensityValue` (`GLES2Sprite.cpp:429-438`, identical in the other two
backends) sets `m_bitmapSize = textureSize / density` and re-runs `SetupSpriteRects(1,1)`. So **all rect
arithmetic happens in virtual 1× pixels** and an HD asset occupies the same world size as its 1× counterpart.
UVs still come out right because `optimal.vs:45-49` divides rect by bitmapSize, both virtual.

**A converter should pick one density tier and must not treat `hd/ fullhd/ ld/ xld/` as content directories.**

### 8.6 Asset path resolution — directories are STRIPPED
`ETHGraphicResourceManager::GetPointer` (`ETHResourceManager.cpp:82-113`) does
`fileName = Platform::GetFileName(fileRelativePath)` then
`AssembleResourceFullPath(resourceDirectory, searchPath, fileName) = programPath + searchPath + fileName`
(`:184-190`). `GetFileName` normalises both slash kinds and returns everything after the last slash.

```
finalPath = resourceDirectory + <fixed subdirectory> + basename(pathStoredInFile)
```

**Any directory component written in the `.ent`/`.esc` is discarded.** `sprites/enemies/bat.png` resolves to
`<res>entities/bat.png`. Paths are never relative to the file, never relative to the project root, never
absolute. The sprite namespace is deliberately flat — the cache is keyed on the bare basename (`:150`, `:164`)
and `FindSprite` logs "Duplicate resource name found" when two origin paths collide (`:170-175`).

| Field | Resolves to | Colour key (`cutOutBlackPixels`) | Loaded when |
|---|---|---|---|
| `<Sprite>` | `<res>entities/<basename>` | `false` → magenta `0xFFFF00FF` | always |
| `<Normal>` | `<res>entities/normalmaps/<basename>` | magenta | **only if `richLighting`** |
| `<Gloss>` | `<res>entities/<basename>` | magenta | **only if `richLighting`** |
| `<Light><HaloBitmap>` | `<res>entities/<basename>` | **`true` → black `0xFF000000`** (halos are additive art) | only if the entity has a light |
| particle `<Bitmap>` | `<res>particles/<basename>` | `alphaMode == AM_ADD` | always |
| `.par` file | `<res>effects/<name>` | — | script-driven |
| `.esc` entity references | `<res>entities/<EntityName>` | — | branch A only |

All shipped asset references are **bare filenames with no separator**, and every one resolves case-exactly —
including upper-case extensions (`explosion.JPG`, `water.PNG`) — except three deliberate negative-path
fixtures in `barrel_not_found.ent` and `bitmap_not_to_be_found.par`.

**There is no glow map.** `grep -ril glow toolkit/Source/src` returns zero hits. The only emissive mechanism
is the scalar `<EmissiveColor>` added to the ambient term (`ETHRenderEntity.cpp:113-117`). `<Gloss>` feeds
specular, not emission — do not map "glow" onto it.

### 8.7 Real `app.enml`
```
default
{
	title = Ethanon Engine testbed;
	richLighting = true;
	width = 800;
	height = 800;
	definedWords = IN_TESTBED,DUPLICATE_DIRECTIVE,ETHANON_SAMPLE;
	windowed = true;
	vsync = true;
}

windows
{
	width = 1024;
	height = 768;
	windowed = true;
	definedWords = DUPLICATE_DIRECTIVE,RUNNING_ON_WINDOWS,DUPLICATE_DIRECTIVE,DUPLICATE_DIRECTIVE;
}

macosx
{
	height = 600;
	definedWords = DUPLICATE_DIRECTIVE,RUNNING_ON_MACOSX,DUPLICATE_DIRECTIVE,DUPLICATE_DIRECTIVE;
	//richLighting = false;
}
```
(`testbed/GeneralTests/app.enml`, complete.) `toolkit/Ethanon Toolkit/editor.enml` has
`renderMode = Pixel shaded;` — proof that values may contain spaces and are not whitespace-delimited.

---

## 9. Sprite cutting, pivot, and the coordinate system

### 9.1 Frame layout — row-major, integer strides
`gs2d::Sprite::SetupSpriteRects(columns, rows)` (`gs2d/src/Sprite.cpp:104-136`):
```cpp
if (columns <= 0 || rows <= 0) { ShowMessage(...); return false; }   // :108-112, rects left NULL
const unsigned strideX = size.x / columns, strideY = size.y / rows;  // INTEGER division
unsigned index = 0;
for (unsigned y = 0; y < rows; y++)
    for (unsigned x = 0; x < columns; x++)
    { rects[index].pos = (x*strideX, y*strideY); rects[index].size = (strideX, strideY); index++; }
```
So **`frameIndex = row * spriteCut.x + column`**, frame 0 top-left, advancing left→right then top→bottom.
Confirmed twice more: `Sprite::SetRect(col,row) → SetRect(row*m_nColumns + column)` (`:150-153`) and
`ETHEntity::SetFrame(col,row) → m_spriteFrame = row*cutX + column` (`ETHEntity.cpp:765-780`).

- `SetRect(index)` clamps: `m_currentRect = Min(rect, GetNumRects()-1)` and returns false (`:143-148`).
- Values `<= 0` are clamped up to 1 in `ETHSpriteEntity::Create` (`:111-112`) — that clamp, not
  `SetupSpriteRects`, is what prevents the null-rects early-out. Same block duplicated at `:190-193`.
- `strideX * columns` can be **less** than the bitmap width; the remainder pixels belong to no rect.
  Do not redistribute them.
- **Off-by-one in `ETHEntity::SetFrame(uint)`** (`:745-758`): the bound is `frame > numFrames` (should be
  `>=`), so `frame == numFrames` is accepted and then clamped down by `Sprite::SetRect`; `frame > numFrames`
  resets to 0. Replicate only if you emulate script calls.
- **The cut belongs to the entity, not the texture.** Sprites are cached by bare basename, so two entities
  sharing one PNG share one `gs2d::Sprite` and clobber each other's rects — hence
  `ETHSpriteEntity::ValidateSpriteCut` (`:375-383`) re-runs `SetupSpriteRects` **every frame**.

**Two truncations, in this order:** `GetBitmapSize()` returns `Vector2i(int(m_bitmapSize.x), …)` where
`m_bitmapSize` is already the *virtual* (density-divided) float size; then `strideX = unsigned(size.x)/columns`.

```python
def frame_rect(tex_w, tex_h, cols, rows, frame, density=1.0):
    """(x, y, w, h) in TEXTURE pixels for a 0-based frame index."""
    cols, rows = max(1, cols), max(1, rows)
    bw, bh = int(tex_w / density), int(tex_h / density)   # virtual, float->int truncation
    sx, sy = bw // cols, bh // rows                       # integer strides, virtual units
    frame  = min(frame, cols * rows - 1)                  # Sprite::SetRect clamp
    col, row = frame % cols, frame // cols                # row-major
    d = density
    return (int(col * sx * d), int(row * sy * d), int(sx * d), int(sy * d))
```
**Truncation trap:** a 100 px-wide texture at density 2 cut into 3 columns → virtual width `int(100/2)=50`,
`strideX = 50//3 = 16`, texture-space frame width `16*2 = 32` — **not** 33. Compute in virtual space, then
scale up.

### 9.2 Frame size and current size
`ETHSpriteEntity::GetCurrentSize()` (`:652-683`) = `Sprite::GetFrameSize() * properties.scale`.
With no sprite the fallback is the light's `haloSize` (if `> 16`), else the collision box XY (if collidable),
else `(32,32)`.

### 9.3 Pivot / origin
`ETHEntity::ComputeAbsoluteOrigin(size)` (`ETHEntity.cpp:270-297`):
```
typeAnchor = (size.x/2, size.y/2)   for EO_CENTER          (types 0,1,3,4,5)
           = (size.x/2, size.y)     for EO_CENTER_BOTTOM   (type 2, ET_VERTICAL)
           = (0, 0)                 for EO_DEFAULT         (out-of-range type only — TOP-LEFT)
absoluteOrigin = typeAnchor + (pivotAdjust * scale)
```
`ComputeOrigin(size) = absoluteOrigin / size` (`:299-302`) is the normalized origin handed to
`Sprite::SetOrigin` (`ETHSpriteEntity.cpp:367-373`). `size` is `GetCurrentSize()`, i.e. already
`frameSize * scale` — so both terms scale, and in **unscaled sprite pixels**:
```
originPx = typeAnchorPx + pivotAdjust
```
`pivotAdjust` is stored **pre-scale**; the script API confirms it (`GetPivotAdjust()` returns
`pivotAdjust * scale`, `SetPivotAdjust(p)` stores `p / scale`, `:1019-1027`).

**Sign convention: local +y is DOWN.** Proof from source, not convention: `EO_CENTER_BOTTOM` anchors at
`v2Center.y = size.y` (max-y), and the screen rect is built by *subtracting* the origin
(`box.min = pos − absoluteOrigin`, `ETHSpriteEntity.cpp:749-750`), so the sprite extends *upward* from the
entity position — only consistent with y-down. Therefore a **negative `PivotAdjust.y` moves the drawn image
DOWN** relative to the entity position (`barrel_light.ent`'s `y="-12"` pushes the barrel 12 px down).

### 9.4 World → screen
`ETHGlobal::ToScreenPos` (`engine/Util/ETHASUtil.cpp:123-126`):
```cpp
Vector2 ToScreenPos(const Vector3 &v3Pos, const Vector2 &zAxisDirection)
{ return Vector2(v3Pos.x, v3Pos.y) + (zAxisDirection * v3Pos.z); }
```
- Origin top-left, **+x right, +y down** — the same convention as Godot 2D, so **no Y sign flip is needed**.
- Units: pixels at global scale 1. `ETHScene::ScaleEntities` multiplies at runtime; the file always stores
  unscaled values.
- **`z` is not a layer index.** With the default `zAxisDirection = (0,−1)` it is a fake height in pixels: an
  entity at `z=26` is drawn 26 px higher on screen than its world y. It additionally feeds depth sorting
  (via scene min/max height), the parallax offset, and light positions.
- Parallax and camera are added on top in `ETHSpriteEntity::ComputeInScreenPosition` (`:685-689`).

Full chain, exactly:
```
screenPos      = (pos.x, pos.y) + zAxisDirection * pos.z          # + parallax + camera at runtime
displaySize    = frameSize * scale
absoluteOrigin = typeAnchor(displaySize) + pivotAdjust * scale
quadTopLeft    = screenPos - absoluteOrigin
quadCenter     = quadTopLeft + displaySize/2
```
Vertex math backing it (`shaders/GLSL/default/optimal.vs:26-43`, `center = normalizedOrigin * size`):
```glsl
newPos = (unitQuad * flipMul + flipAdd) * size - center;   // flip applied to the [0,1] quad FIRST
… rotate … ; newPos += entityPos; newPos -= halfScreenSize; newPos -= cameraPos;
newPos *= vec4(1,-1,1,1);      // Y flips only here, for GL clip space
```
Note the flip params are `flipMul.x = -1, flipAdd.x = 1` applied to the **unit quad** — Ethanon mirrors
*within the frame box*, then applies the origin. It does **not** mirror about the origin point.

### 9.5 Angle
Degrees, unwrapped. Of 362 shipped instances 336 have `angle="0"`; the 26 non-zero values include `90`,
`466` and `-425` — magnitudes impossible in radians and exceeding a full turn, so the editor accumulates
without wrapping.

**Sign — DERIVED, not measured.** Two independent derivations agree that **positive `angle` is
counter-clockwise on screen**:
1. Render: `RotateZ(DegreeToRadian(-angle))` (`GLES2Sprite.cpp:207, 270`) → `optimal.vs:31-34` gives
   `x' = x·cos a + y·sin a`, `y' = −x·sin a + y·cos a`, i.e. CCW in a y-down space for positive `angle`.
2. Physics: `bodyDef.angle = -(DegreeToRadian(GetAngle()))` (`ETHPhysicsSimulator.cpp:76`) into a Box2D
   world whose gravity is `(0,+10)` and whose positions are a straight 1/50 scale with no axis flip — i.e.
   Box2D space is also y-down, where Box2D's own CCW-positive convention appears clockwise on screen; the
   negation therefore makes positive on-disk angles CCW on screen.

⇒ Godot (y-down, positive = clockwise) needs `rotation = -deg_to_rad(angle)` — **except** for `type == 2`,
where rotation is forced to 0 (§5.3). See §11 for the one section that asserts the opposite sign, and §12
for the desktop-vs-GLES2 backend caveat.

---

## 10. Converter implications — Ethanon → Godot 4

Everything in this section is engineering judgement layered on the verified facts above. **Godot 4 property
spellings were written from memory.** *2026-09-19: the Godot runtime this section was written for is removed.
The node types and properties below are the `.tscn` vocabulary the converter writes; what they mean at run time is
now what the Supersonic port's reader (`game/sim/Tscn.cpp`) makes of them. Statements about how Godot
itself behaves (its physics units, its particle model, `flip_h`) are kept as written, as history.*

### 10.1 Node mapping

| Ethanon | Godot 4 | Notes |
|---|---|---|
| `.esc` scene | `Node2D` root + one child per instance | Scene props → ambient/light setup (§10.7) |
| Entity instance | `Node2D` (or `Sprite2D` directly if it has no children) named from `<EntityName>` + `id` | Keep `id` in metadata — the lightmap sidecar is keyed on it |
| `<Sprite>` + `<SpriteCut>` + `spriteFrame` | `Sprite2D` with an `AtlasTexture` | One `AtlasTexture` per **(texture, cut, frame)** triple — §10.2 |
| `type = 0/1/3/4` | `Sprite2D` centered, rotation from `angle` | |
| **`type = 2` (`ET_VERTICAL`)** | `Sprite2D` anchored centre-bottom, **`rotation = 0`**, parent has `y_sort_enabled = true` | Three consequences, not one |
| `type = 5` (`ET_LAYERABLE`) | `z_index` derived from `layerDepth` (0..1), **z ignored** | No shipped sample uses it |
| `type = 3` (`ET_OVERALL`) | Highest `z_index` / a top `CanvasLayer` | Depth is the constant 1.0 |
| `<Light>` | `PointLight2D` (+ a separate `Sprite2D` for `<HaloBitmap>`) | Position is entity-relative; `range` is a radius in px; colour has no alpha |
| `<Collision>` + `shape` | `StaticBody2D`/`RigidBody2D`/`Area2D` + `CollisionShape2D` — §10.4 | |
| `<Particles>` | `CPUParticles2D`, one per system, in index order — §10.5 | |
| `<CustomData>` | Node metadata (`set_meta`) or an exported script dict | Typed; keys are unique and alphabetical |
| `blendMode` | `CanvasItemMaterial.blend_mode` | 0→MIX, 1→ADD, 4→MUL; **2 (`AM_ALPHA_TEST`) needs a `discard` shader; 3 (`AM_NONE`) is an opaque draw** |
| `<EmissiveColor>` / `<Luminance>` | Bake into `modulate`, or a `CanvasItemMaterial` emission path | Godot 2D has no direct equivalent to Ethanon's ambient model |
| `.par` in `effects/` | Reusable `PackedScene` with a `CPUParticles2D` | Only needed if you also port scripts |
| `<Normal>` | `CanvasTexture.normal_texture` | Only if you keep 2D lighting; gated on `richLighting` in the original |
| `<Gloss>`, `shadowScale`/`shadowLengthScale`/`shadowOpacity`, `specular*`, `parallaxIntensity`, lightmap `add<id>.bmp` | **No mapping** — Ethanon-specific pseudo-3D lighting | Preserve as metadata/comments if the art depends on them |

### 10.2 Sprite cut → `AtlasTexture`
```
region = frame_rect(tex_w, tex_h, spriteCut.x, spriteCut.y, spriteFrame, D)     # §9.1
```
**One `AtlasTexture` per (texture, cut, frame)** — not per texture. The same PNG cut 4×4 by one entity and
1×1 by another yields two different region sets (this is exactly why the engine re-cuts every frame).

Recipe, with `D` = the density of the chosen tier, `(sx,sy) = <Scale>`, `(px,py) = <PivotAdjust>`,
`(rw,rh)` = the texture-space region size:
```
Sprite2D.centered = true
Sprite2D.texture  = AtlasTexture(region = frame_rect(...))
Sprite2D.scale    = (sx / D, sy / D)                     # per-node, because D is per-texture
Sprite2D.position = ToScreenPos(pos3, zAxisDirection)    # y-down already matches
Sprite2D.flip_h   = flipX
Sprite2D.flip_v   = flipY                                # remember flipY defaults to flipX!
Sprite2D.rotation = 0.0 if type == 2 else -deg_to_rad(angle)
Sprite2D.offset.x = -px * D
Sprite2D.offset.y = -py * D                    # types 0,1,3,4,5
Sprite2D.offset.y = -py * D - rh / 2           # type 2 (ET_VERTICAL): shift up by half the frame
```
Rendered size checks out: `(strideX * D) * (sx / D) = strideX * sx` = the Ethanon display size.

Derivation of the offset: the required region **centre** in world space is
`entityPos − pivotAdjust*scale + (displaySize/2 − typeAnchor)`. The x-component of
`(displaySize/2 − typeAnchor)` is 0 for every type; the y-component is 0 for `EO_CENTER` and
`−displaySize.y/2` for `ET_VERTICAL`. Dividing by the node scale gives the formulas above.

**Worked check** (real data): `testbed/GeneralTests/entities/hunter03recolourxi8.png` is 128×192; the
`character` instance in `TestEntity.esc` has `SpriteCut 4×4`, `spriteFrame="8"`, `type="2"`,
`PivotAdjust y="−6"`, `Position (402, 367)`, no `<Scale>` ⇒ `(1,1)`, D=1.
strides 32×48 → frame 8 = col 0, row 2 → region `(0, 96, 32, 48)`. typeAnchor `(16,48)`;
absoluteOrigin `(16, 42)`; quad top-left `(402−16, 367−42) = (386, 325)`; bottom edge `373` = entity y **+6**.
Godot: `offset = (0, 6 − 24) = (0, −18)`; region centre `(402, 349)`; region top-left `(386, 325)`. ✔

**Two Godot-side assumptions, unverified:** that `Sprite2D.offset` is in texture pixels applied *before* the
node scale, and that `flip_h` does **not** mirror `offset`. If Godot negates `offset.x` under `flip_h`,
pre-negate to compensate.

### 10.3 Depth / draw order
**File order carries no information** (§4.6). Ethanon's true depth is computed per frame from
`ComputeDepth(z, sceneMaxHeight, sceneMinHeight)` normalized against the *current scene's* min/max z, plus
type-specific offsets and a draw hash (§5.3) — **not statically reproducible**. Pragmatic mapping,
**approximate by construction**:

| Ethanon | Godot |
|---|---|
| `type = 2` | `y_sort_enabled = true` on the common parent; let Y sorting do the work |
| `type = 5` | `z_index = round(layerDepth * K)` for some spread `K`; ignore `z` |
| `type = 3` | Highest `z_index`, or its own `CanvasLayer` |
| `type = 0/1/4` | `z_index` from the entity's `z` (rank-ordered across the scene), with `1`/`4` getting a small `+ε` bump over a same-z type 0 |

Do **not** try to reproduce the hash arithmetic; sanity-check visually against the original.

### 10.4 Collision
```
size_full  = <Collision><Size x y>   * <Scale>        # FULL extents — halve for Godot's RectangleShape2D
center     = <Collision><Position x y> * <Scale>      # offset from the entity origin
```
| `shape` | Godot |
|---|---|
| 0 `BS_NONE` | no body (but `collision != null` still means "collidable" in the legacy AABB sense) |
| 1 `BS_BOX` | `RectangleShape2D(size = size_full)` — Godot's `size` *is* full extents; Box2D took half |
| 2 `BS_CIRCLE` | `CircleShape2D(radius = (size_full.x + size_full.y) / 4)` |
| 3 `BS_POLYGON` | `ConvexPolygonShape2D`, vertices `v_i * size_full/2 + center` (ENML `-1..1`, convex, CW) |
| 4 `BS_COMPOUND` | Multiple `CollisionShape2D` children, one per ENML fixture, **in alphabetical ENML-name order** |

Body type: `static="1"` → `StaticBody2D`; else `RigidBody2D` (`sensor="1"` → `Area2D`).
Map `friction`/`restitution` to a `PhysicsMaterial`, `density` → `mass` (density is forced to 0 when static),
`fixedRotation` → `lock_rotation`, `bullet` → `continuous_cd`, `gravityScale` → `gravity_scale`.
`<Joints>` `revoluteJoint*` → `PinJoint2D`, anchors `attachPointA/B * (collisionBox.size.xy * scale * 0.5)`,
partner resolved from `otherEntityName` or from the custom-data fallback (§6.4).

⚠️ **`DEFAULT_SCALE = 50 px per Box2D unit` is an internal Box2D artifact. Do NOT apply it to Godot 2D
physics** — Godot works in pixels. Likewise the ENML `-1..1` normalization is relative to the collision box,
not to the sprite.

### 10.5 Particles → `CPUParticles2D`
Prefer `CPUParticles2D`: counts are small (1–300), properties are diffable in the `.tscn`, and you avoid a
`ParticleProcessMaterial` sub-resource per system.

| Ethanon | Godot | Conversion |
|---|---|---|
| `nParticles` | `amount` | direct |
| `lifeTime` L, `randomLifeTime` R | `lifetime`, `lifetime_randomness` | `lifetime = (L + R/2)/1000`; `lifetime_randomness = R/(L + R/2)` — matches Ethanon's `[L−R/2, L+R/2]` exactly. A naive `L/1000` + `R/L` is badly wrong |
| `repeat == 0` | `one_shot = false` | endless |
| `repeat == 1` | `one_shot = true` | single burst |
| **`repeat > 1`** | **no equivalent** | `repeat = 8` in **5 shipped systems**, all *fireball* variants. Emulate with `one_shot = false` + a timer clearing `emitting` after `N * lifetime` |
| `allAtOnce = 1` | `explosiveness = 1.0` | |
| `allAtOnce = 0` | `explosiveness = 0.0` | **Approximate:** Ethanon fills over `L + R`, Godot over `lifetime` (1.7× difference for `fire.par`) |
| `startPoint.x/y` | node `position` | y-down matches; rotates with the entity angle |
| `randStartPoint` | `EMISSION_SHAPE_RECTANGLE`, `emission_rect_extents = randStartPoint / 2` | |
| `angleStart`, `randAngleStart` | `angle_min = angleStart`, `angle_max = angleStart + randAngleStart` | asymmetric, matching `U(0,max)` |
| `angleDir`, `randAngle` | `angular_velocity_min/max = (angleDir ∓ randAngle/2) * 60` | deg/s, symmetric |
| `gravityVector` | `gravity = v * 3600` | px/s², y-down matches |
| `size`, `randomizeSize` | `scale_amount_min/max = (size ∓ randomizeSize/2) / frame_dimension` | frame, not bitmap (§7.5) |
| `color0` → `color1` | 2-stop `Gradient` in `color_ramp` at offsets 0 and 1 | linear lerp in both |
| `alphaMode` | `CanvasItemMaterial.blend_mode` | §5.5 |
| `spriteCut` + `animationMode` | `particles_animation = true`, `particles_anim_h_frames = cut.x`, `particles_anim_v_frames = cut.y`, `particles_anim_loop = false` | row-major matches exactly |
| `PLAY_ANIMATION` | `anim_speed = 1.0`, `anim_offset = 0` | one pass over lifetime |
| `PICK_RANDOM_FRAME` | `anim_speed = 0`, `anim_offset_min/max = 0/1` | frozen random frame |
| `minSize`/`maxSize` + linear `growth` | `scale_amount_curve` (approximate) | Ethanon clamps a *per-particle-randomized* start size, which a shared curve cannot express. **18 of 54 systems have non-`(0,1000)` clamps** — check per file |
| `boundingSphere` | optionally `visibility_rect` | Culling only; safe to drop |
| `Luminance` | bake: `final = color * min(emissive + sceneAmbient, 1)` | Live in 14 systems (§7.6) |

**The one real mismatch — rectangular velocity box vs polar cone.** Ethanon samples an axis-aligned rectangle
in velocity space with *independent* x and y jitter; Godot's model is `direction` + angular `spread` +
scalar `initial_velocity_min/max`. These are different shapes and cannot be made equivalent.
Example, `fire.par` (`Direction (0,−1.4)`, `RandomizeDir (0.6,0.2)`): the true box is
`vx ∈ [−18,+18]`, `vy ∈ [−90,−78]` px/s; the best polar fit is `direction (0,−1)`, `spread ≈ 12°`,
`initial_velocity ≈ 80…92`. Good when `randomizeDir.x ≈ randomizeDir.y` (and for the omnidirectional case
`direction = (0,0)`), poor for strongly anisotropic jitter. For a signature effect, use `GPUParticles2D`
with a ~10-line custom process shader that samples the box directly and reproduces Ethanon exactly.

Also: preserve **index order** (scripts address systems by index via `PlayParticleSystem(n, …)`), and note
that a system dropped for `nParticles <= 0` or a missing texture shifts nothing on disk but does occupy its
slot at runtime.

### 10.6 Assets
1. Resolve every `<Sprite>/<Normal>/<Gloss>/<HaloBitmap>/<Bitmap>` as
   `basename()` + the fixed subdirectory (§8.6).
2. Pick **one** density tier; treat `hd/ fullhd/ ld/ xld/` as variants, not content dirs; record `D` per
   texture and set node `scale = <Scale> / D`.
3. Import magenta-keyed art (`0xFFFF00FF`) and black-keyed halos (`0xFF000000`) with the key converted to
   alpha, or re-author the art.
4. `.JPG`/`.PNG` upper-case extensions are real and case-exact.

### 10.7 Scene properties
`ambient` → a `CanvasModulate` colour (its `lightIntensity` multiplier applies to lights, not ambient);
`lightIntensity` → multiply every `PointLight2D.energy`; `parallaxIntensity` → a `Parallax2D`/`ParallaxLayer`
configuration (Ethanon's formula also needs the runtime-only normalized origin, default `(0.5,0.5)`, and
per-entity `parallaxIntensity` defaulting to 1.0); `zAxisDirection` → the `z` projection in §9.4.

### 10.8 Two scoping facts that shape the whole project
1. **No animation data exists in any Ethanon data file.** The only writes to `m_spriteFrame` in the entire
   source are: ctor zero (`ETHEntity.cpp:101`), the XML read (`:221`), `SetFrame(uint)` (`:750/:755`),
   `SetFrame(col,row)` (`:772/:777`), and a reset in `SetSpriteCut` (`ETHSpriteEntity.cpp:343`).
   `ETHSpriteEntity::Update` (`:453-460`) has no elapsed-time path. `ETHFrameTimer` exists but is **not**
   registered with AngelScript — it is editor-preview only. Games ship their own timer
   (`eth_util.angelscript:196-235`, `frameTimer.Set(first, last, stride)` with `stride` in **ms per frame**)
   and the canonical idiom is `character.SetFrame(charaTimer.Set(0,3,stride), directionRow)` with
   `stride = 100` (`Sample-project/main.angelscript:74, 100-102`) — i.e. **column = animation frame, row =
   facing direction**. ⇒ The converter emits a *static* frame. Frame ranges, rates, loop flags and
   direction-row conventions must be recovered from the game's `.angelscript` and hand-authored as
   `SpriteFrames`/`AnimationPlayer`. Budget this separately.
2. **Asset paths are basename-flattened** (§8.6). This is the highest-consequence rule for actually locating
   PNGs, and it is easy to miss because the field values look like relative paths.

### 10.9 Effort estimate
Mechanical field conversion: a few hours. Velocity-box approximation: half a day (or ~1 hour via the
GPU-shader route). `repeat > 1` wrapper: ~1 hour. Per-effect artistic tuning: 15–30 min each. Auto-converting
is clearly worth it — it preserves the designer's numeric intent (colours, lifetimes, counts, blend modes,
sheet layouts) that would be tedious and error-prone to re-derive by eye.

---

## 11. Conflicts between the verified source sections

Reported honestly; where I resolved one myself, the resolving read is cited.

### 11.1 RESOLVED — which TinyXML build is compiled
The scene-format section states the scene loader uses `vendors/tinyxml_utf16/` ("Windows/wide builds"); the
entity, particle and sprite sections all state `vendors/tinyxml_ansi/`.

**Resolution: `tinyxml_ansi`, unconditionally.** `toolkit/Source/src/engine/ETHTypes.h:**28**` (not `:27`,
which two sections cite) is `#include "../vendors/tinyxml_ansi/tinyxml.h"`, and `tinyxml_utf16` is
`#include`d nowhere in `toolkit/Source/src` (verified by grep).

**Downstream correction:** the scene section therefore over-claims when it says this snapshot's writer emits
`spriteFrame="3.000000"`. The mechanism is real (`ETHEntity.cpp:154` writes `SetAttribute`, `:156-157`
overwrites with `SetDoubleAttribute` when `> 0`), but the `tinyxml_ansi` `SetDoubleValue` is `"%g"`
(`tinyxml_ansi/tinyxml.cpp:1211-1220`), so it would emit `"3"`. The `%f`-padded values on disk are legacy
`tinyxml_utf16` (`L"%lf"`) output. **The `int(float(s))` rule survives unchanged** because both spellings
exist in the shipped corpus — do not "simplify" it away.

### 11.2 RESOLVED — scene corpus size and every frequency derived from it
The scene-format section reports **10** `.esc` files and **336** instances; the ground-truth section reports
**11** and **362**.

**Resolution: 11 files, 362 outer instances** (724 `<Entity ` tags total = 362 outer + 362 inner), verified
by direct enumeration. The scene section omitted `testbed/ShaderTests/scenes/scene.esc` (26 instances;
336 + 26 = 362). **Consequently every per-attribute frequency in that section is a floor, not a total.**
The two load-bearing negatives survive and are strengthened: **0 `<FileName>` and 0 `flipX`/`flipY`
occurrences across all 362 instances.**

### 11.3 RESOLVED — is the `flipY` inheritance harmful?
Scene-format calls `ETHEntity.cpp:217` a bug that a converter must replicate; sprites-animation calls it
"harmless in practice because both are initialised to `ETH_FALSE` at `:103`".

**Resolution: scene-format is right.** I read `:216-217`: line 216 has already assigned the file's `flipX`
value to `m_flipX` before line 217 uses `m_flipX` as the *default* for `flipY`. The "harmless" reading only
holds when `flipX` is also absent. Rule: `flip_y = int(attr("flipY", flip_x))`.

### 11.4 OPEN — the sign of the on-disk `angle` in screen space
- **scene-format §6** and **sprites-animation §5**: positive `angle` = **counter-clockwise on screen**
  (derived from `RotateZ(DegreeToRadian(-angle))` at `GLES2Sprite.cpp:207/270` through `optimal.vs:31-34`,
  and independently from `Sprite::Stretch`/`GetAngle` at `gs2d/src/Sprite.cpp:70-101` +
  `GameMath.h:711-715`).
- **entity-format §8.4**: *"On-disk entity angles are therefore clockwise-positive in screen space"*, from
  `bodyDef.angle = -(DegreeToRadian(entity->GetAngle()))` (`ETHPhysicsSimulator.cpp:76`).

**My reading — I am rejecting the entity-format claim, and here is why, so you can re-check it.** I verified
that the Ethanon→Box2D mapping preserves the axes: gravity is `b2Vec2(0, +10)` (`:26`) and position is a
straight `ScaleToBox2D` (1/50) with no y flip. So Box2D space here is **also y-down**, and Box2D's own
CCW-positive-in-its-own-axes convention appears *clockwise on screen*. Negating the on-disk degrees to get
the Box2D angle therefore makes positive on-disk angles **counter-clockwise on screen** — agreeing with the
two render derivations. The entity-format sentence looks like an inversion.

**Confidence: DERIVED, not measured. No empirical test was run in any section.** Put the sign behind one
flippable constant in the converter and validate against a rotated `physicsTest.esc` entity (`angle="90"`).

### 11.5 OPEN — desktop vs GLES2 rotation sign (separate from 11.4)
`GLES2Sprite.cpp:207, 270` uses `RotateZ(DegreeToRadian(-angle))`; `GLSprite.cpp:140` and
`D3D9Sprite.cpp:402` use `RotateZ(DegreeToRadian(+angle))` with *different* shader files that were not
traced by any section. Entities take the `DrawOptimal` path, and Magic Portals was a mobile title, so the
GLES2 derivation is the relevant one — but if a rotated non-vertical entity comes out mirrored, this is the
first thing to flip.

### 11.6 Not conflicts, just different denominators
Several counts differ only because sections counted different units. Reconciled here:

| Item | Total occurrences (all documents) | Files with it as a direct `<Entity>` child |
|---|---|---|
| `<SpriteCut>` | 85 | **70** (15 occurrences are inside `<ParticleSystem>`) |
| `<Variable>` | 57 (25 in `.ent`) | only **8** `.ent` files have any custom data |
| `<ParticleSystem>` | 70 (33 `.par` + 21 `.ent` + 16 `.esc`) | **17** `.ent` files (4 have two) |
| `blendMode` | 422 `<Entity>` tags (`0` ×346, `2` ×76) | 60 of 89 `.ent` files |

Similarly, entity-format's `.ent`-only frequencies and ground-truth's all-tags frequencies are both correct
for their scope.

### 11.7 Superseded framings that must not survive into a converter
- *"`.par` files are design-time only."* **Refuted** — `PlayParticleEffect("fireball.par", …)` at
  `testbed/GeneralTests/Test/TestSceneScale.angelscript:93, 95`. The narrower true claim: no `.par` basename
  appears inside any `.ent`/`.esc`.
- *"`<Particles />` is what the current writer emits when there are none."* **Refuted** — the writer guards
  with `if (!particleSystems.empty())` (`ETHEntityProperties.cpp:468`), so it emits nothing. Yet **89/89**
  samples contain `<Particles>`, 72 of them empty. Same story for `layerDepth` (written only when
  `type == ET_LAYERABLE`, present in 69/89 samples, **zero** of which are type 5) and for identity-valued
  `<EmissiveColor>/<SpriteCut>/<Scale>/<PivotAdjust>`. **This is independent proof that the shipped sample
  data was produced by a different editor build than this checkout** — accept both the present-and-empty and
  the absent forms everywhere.
- *"`.ent` files are always UTF-16."* One deliberate UTF-8+BOM fixture exists (§3.1).
- *"`<EntityName>` is the `.ent` filename with extension."* 15/362 have no extension and 14 of those have no
  file (§4.5).
- *"The halo directory is a separate folder."* It is literally `entities/` (§8.1).

---

## 12. Open questions / UNKNOWN

Grouped by whether they block conversion.

### 12.1 Cannot be answered from this repository at all
- **Whether Magic Portals shipped on this engine revision.** The clone is depth-1 at Dec 2013. **Concrete
  check:** extract the APK and list `assets/entities/` — if its only subdirectories are
  `normalmaps/ hd/ fullhd/ ld/ xld/` (i.e. sprites are flat), §8.6's flattening rule holds. If `.ent` files
  reference sprites that exist only under other subdirectories, the game used a later revision and §8 must
  be re-derived.
- **Which nested-entity form Magic Portals' `.esc` files use** (reference vs inline). All 11 shipped scenes
  are inline; the writer in this tree emits the reference form whenever the `.ent` resolves. Implement both.
- **Whether any real scene sets `flipX`** (which would trigger §11.3). Zero occurrences in shipped data.
- **Which density tier the APK ships**, and whether it is complete (§8.5 resolves per file).
- **Whether the game's assets contain a `Color0`/`Color1` channel > 1.0** (would activate `highlightPS`), an
  `alphaMode` other than 0/1, or `repeat > 1` beyond the fireball family. Assert-and-warn on all three.
- **All animation timing** — frame ranges, ms/frame, loop flags, direction-row conventions. Lives only in
  `.angelscript` (§10.8).
- **Whether Magic Portals used a modified engine fork.** Unknown attributes are ignored silently by every
  reader, so a fork-added field would be invisible here.
- `documentation/Manual/` is a **71-byte stub** pointing at the now-dead `doc.ethanonengine.com`. There is
  **no human-written format documentation in the repo**; a web-archive fetch is the only avenue.

### 12.2 Semantics not inferable because the data is degenerate
- **`shadowZ`** — present on 91/362 instances and **`0` in all 91** (52× `"0"`, 39× `"0.000000"`). Its
  relationship to the template-level shadow attributes is unknown.
- **Outer `<Color r g b a>`** — present on 7/362 instances and **`(1,1,1,1)` in all 7**. Read into
  `m_v4Color` and multiplied into the ambient diffuse (`ETHRenderEntity.cpp:117`), but no sample varies it.
- **`blendMode = 1` (`AM_ADD`) on an entity** — never appears in shipped entity data (only in particles).
- **`ZAxisDirection = (0,0)`** in `physicsTest.esc` (all other scenes use `(0,−1)`) — whether this disables
  the pseudo-3D z projection or is degenerate/unsupported input.
- **Compound-shape ENML `angle` units** — inferred to be Box2D **radians** from the
  `b2PolygonShape::SetAsBox` signature only; all five shipped `<Compound>` blocks plus the editor template
  use `angle = 0`.
- **`enableLimit`, `upperAngle`, `lowerAngle`** in `<Joints>` — source-verified, never exercised by data.
- **The zero-size `<Collision>` discard branch** — never reached by any sample (the `collidable` gate always
  short-circuits first).

### 12.3 Fields with no consumer found
- **`emissiveColor.w`** (the `a` of `<EmissiveColor>`) — parsed, stored, compared against the default and
  round-tripped, but the render path uses only `.x/.y/.z` (`ETHRenderEntity.cpp:114-116`) and the editor
  exposes only three RGB sliders (`EntityEditor.cpp:234-253`). Whether the alpha was ever meaningful: UNKNOWN.
- **`startFrame`** and **`soundVolume`** — no XML read/write path in this snapshot. Intended semantics
  ("starting frame in the sprite sheet" per a commented editor label; per-entity particle-sound volume) are
  inferred from names, not proven.
- **`ETHEntityProperties::IsCollidable()`** — declared at `ETHEntityProperties.h:109`, **no definition
  anywhere** in `toolkit/Source/src`. Dead; use `ETHEntity::IsCollidable`.
- **`ETHDirectories::GetSoundFXDirectory()`** — zero call sites; the actual sound folder is a script
  convention.

### 12.4 Godot-side assumptions to verify before trusting the recipes
*2026-09-19: written for the Godot runtime, which is removed. They matter now only where the Supersonic port's
`.tscn` reader interprets the same properties.*
- `Sprite2D.offset` in texture pixels, applied **before** node scale (§10.2).
- `Sprite2D.flip_h` does **not** mirror `offset` (§10.2).
- `CPUParticles2D.lifetime_randomness` formula `lifetime * (1 − randf()*lr)` — the derivation in §10.5
  depends on it.
- Godot 4.x property spellings generally (several were renamed from 3.x, e.g.
  `scale_amount` → `scale_amount_min`/`_max`).

### 12.5 Minor / cosmetic unknowns
- Exact `boost::lexical_cast<unsigned int>` behaviour on a value with preserved trailing whitespace
  (`width = 800 ;`) — Boost is not vendored in this tree.
- The Linux/SDL `GetPlatformName()` string (`gs2d/src/Platform/sdl/SDLWindow.h:105` declares it; no
  implementation exists here), i.e. which `app.enml` entity a Linux build would read.
- Whether `Video::IsRoundingUpPosition()` was enabled in the shipped game — when true,
  `GLES2Sprite.cpp:253-262` floors entity position, camera position and origin before drawing, shifting
  sprites by up to 1 px relative to the exact formulas in §9.4.
- What encoding the shipped editor binary actually wrote for `.ent` (`SaveToFile` uses the ANSI TinyXML,
  which writes UTF-8 without a BOM, yet 88/89 files on disk are UTF-16LE+BOM). Only matters if the converter
  ever writes `.ent` files back.

---

# Appendix A — Entry points: where the scene loader and entity parser live

*(Answer to "point me at the scene loader and entity parser". Verified independently of
the main document above.)*

# Ethanon: where the scene loader and entity parser live

Paths relative to `D:/Magic-Portals-Remake/reference/ethanon`. This revision is **TiXml/XML only** — a case-insensitive `grep -rni json` over `toolkit/Source/src/engine` and `toolkit/Source/src/gs2d/src` returns nothing; libjson exists in `vendors/` but is unreferenced. There is no binary or ENML scene format (ENML appears only *inside* `<Polygon>`/`<Compound>`/`<Joints>` CDATA text).

## Read this first — the `.esc` path
1. `toolkit/Source/src/engine/Scene/ETHScene.cpp:160` — `ETHScene::LoadFromFile()`. Exists-check, `GetUTFFileString`, `doc.LoadFile(content, TIXML_ENCODING_LEGACY)` at :180. **Best single breakpoint.**
2. `.../ETHScene.cpp:200` — `ETHScene::ReadFromXMLFile()`. Scene props at :209, then walks `<EntitiesInScene>` → `<Entity>` siblings; :226 builds one `ETHRenderEntity` per element, :229 drops entities whose props failed.
3. `.../Scene/ETHSceneProperties.cpp:30` — `ReadFromXMLFile()`: `lightIntensity`, `parallaxIntensity`, `<Ambient r/g/b>`, `<ZAxisDirection x/y>`. Defaults in `Reset()` at :88.

## The fork that defines the format
4. `.../Entity/ETHEntityProperties.cpp:202` — `ReadFromXMLFile(pElement, entityCache, entityPath, fileManager)`. If the inner `<Entity>` has a `<FileName>` child (:208), properties come from the `.ent` on disk via `ETHEntityCache::Get` (`.../Entity/ETHEntityCache.cpp:27`) and only `<CustomData>` is re-read (:215). Otherwise (:221) the whole entity body is **inline** in the `.esc`.
   - Caveat: the cache key is `entityName`, taken from the *outer* `<EntityName>` text (`ETHEntity.cpp:232`), not from `<FileName>`'s text; :208 only tests presence.
   - Caveat: all 11 shipped `.esc` samples use the **inline** form — none contains `<FileName>` — although the writer prefers the reference form. A parser must accept both.

## The `.ent` parser
5. `.../Entity/ETHEntityProperties.cpp:226` — `ReadFromXMLFile(TiXmlElement*)`, lines 226–413: the real field-by-field parser (type/static/shape/blendMode/layerDepth/parallaxIntensity, conditional light+shadow+physics blocks, `SpriteCut`/`Scale`/`PivotAdjust`, `<Sprite>`/`<Normal>`/`<Gloss>`, `<Particles>`, `<Light>`, `<Collision>`).
6. `.../ETHEntityProperties.cpp:136` — `ETHEntityProperties(filePath, fileManager)`: standalone `.ent` entry point; sets `entityName` from the filename (:163) and descends `<Ethanon>` → first child `<Entity>` (:164). Defaults in `Reset()` at :167.
7. `.../Entity/ETHEntity.cpp:210` — `ReadInSceneDataFromXMLFile()`: the per-instance overlay (`id`, `shadowZ`, `hide`/`flipX`/`flipY`, `spriteFrame`, `<EntityName>`, `<Color>`, `<Position x/y/z/angle>`).
8. Constructor chain to trace: `ETHScene.cpp:226` → `ETHRenderEntity.cpp:31` → `ETHSpriteEntity.cpp:44` → `ETHEntity.cpp:60` → `ETHEntity::ReadFromXMLFile` at `ETHEntity.cpp:192`.
9. Sub-parsers: `Entity/ETHLight.cpp:40`, `Physics/ETHCollisionBox.cpp:37`, `Particles/ETHParticleSystem.cpp:89`, `Entity/ETHCustomDataManager.cpp:510`.

## Writers (often the clearest spec)
10. `.../Scene/ETHScene.cpp:111` — `ETHScene::SaveToFile()`: emits `<Ethanon>` → SceneProperties → `<EntitiesInScene>`.
11. `.../Entity/ETHEntity.cpp:121` — `WriteToXMLFile()`. Note :140/:162: `Color`, `hide`, `flipX`, `flipY` are **omitted when default**. The reference-vs-inline decision is at :167.
12. `.../ETHEntityProperties.cpp:415` `WriteEntityNameToXMLFile()` (reference form), `:428` `WriteContentToXMLFile()` (inline form, also omits defaults), `:188` `SaveToFile()` (the `.ent` writer). Counterparts: `ETHSceneProperties.cpp:66`, `ETHLight.cpp:65`, `ETHCollisionBox.cpp:44`, `ETHParticleSystem.cpp:158`, `ETHCustomDataManager.cpp:610`.
13. Editor side (thin wrappers over the above): `editor/SceneEditor.cpp:1573` `SaveAs()`, `:1591` `Save()`, `:75` `RebuildScene()`, `:1610` `OpenByFilename()`; `editor/EntityEditor.cpp:1288` `Save()`, `:1277` `SaveAs()`, `:1298` `OpenEntity()`.

## Gotcha before you write any parser
14. `.esc`/`.ent` files are **UTF-16LE with BOM** on disk. `gs2d/src/Platform/FileManager.cpp:106` (`GetUTFFileString`) sniffs the BOM and falls back to UTF-8 — so both encodings are legal input, and the editor's change detector assumes `UTF16_WITH_BOM` (`EntityEditor.cpp:1272`).

---

# Appendix B — Hand-verified citations

The following citations from §1 were re-opened by hand after the document was generated,
as a sanity check on the automated verification pass. This is a spot check, not full
coverage — an uncited or unchecked claim is still a claim, not a fact.

| Citation | Checked | Result |
|---|---|---|
| `engine/Scene/ETHScene.cpp:200` — `ETHScene::ReadFromXMLFile` | yes | correct |
| `engine/Scene/ETHScene.cpp:160` — `ETHScene::LoadFromFile` | yes | correct |
| `engine/Entity/ETHEntityProperties.cpp:226` — `.ent` deserializer | yes | correct |
| `engine/Entity/ETHCustomDataManager.cpp:510` — `ReadDataFromXMLFile` | yes | correct |
| `engine/Scene/ETHSceneProperties.cpp:30` — `ReadFromXMLFile` | yes | correct |
| `engine/Entity/ETHLight.cpp:40` — `ReadFromXMLFile` | yes | correct |
| `engine/Particles/ETHParticleSystem.cpp:89` — `ReadFromXMLFile` | yes | correct |
| `engine/Physics/ETHCollisionBox.cpp:37` — `ReadFromXMLFile` | yes | correct |
| `engine/Scene/ETHSceneProperties.cpp:30` — `ReadFromXMLFile` | yes | correct |
| `engine/Entity/ETHEntity.cpp:210` — `ReadInSceneDataFromXMLFile` | yes | correct |
| `engine/Resource/ETHDirectories.cpp:25` — directory constants | yes | correct |
| `gs2d/src/Sprite.cpp:104` — `SetupSpriteRects` | yes | correct |
| `engine/Util/ETHASUtil.cpp:123` — `ToScreenPos` | yes | correct |

The three highest-consequence claims — the ones that would silently produce a
wrong-looking conversion rather than an obvious error — were checked in full, not just
at the line number:

| Claim | Verified |
|---|---|
| **`flipY` inherits `flipX` as its default** (`engine/Entity/ETHEntity.cpp:217`) | **CONFIRMED.** The line literally reads `ReadBooleanPropertyFromXmlElement(pElement, GS_L("flipY"), m_flipX)` — the fallback passed for `flipY` is `m_flipX`. This is an engine **bug**, and the converter must **reproduce** it: an instance with `flipX="true"` and no `flipY` attribute renders flipped on *both* axes in the original. Reading it "correctly" would silently mis-render those entities. |
| **`ENTITY_TYPE` integer encodings** (`engine/Entity/ETHEntityProperties.h:82-90`) | **CONFIRMED.** `ET_HORIZONTAL=0, ET_GROUND_DECAL=1 (deprecated), ET_VERTICAL=2, ET_OVERALL=3 (deprecated), ET_OPAQUE_DECAL=4 (deprecated), ET_LAYERABLE=5`. Three of the six are marked deprecated in the header comments. |
| **Rotation is suppressed for `ET_VERTICAL`** (`engine/Renderer/ETHRenderEntity.cpp:105`) | **CONFIRMED.** `const float angle = (m_properties.type == ETHEntityProperties::ET_VERTICAL) ? 0.0f : GetAngle();` — a `type="2"` entity ignores its stored `angle` entirely when drawn. Applying it in Godot would tilt sprites the original renders upright. |

Independently confirmed against real sample data: `.esc` and `.ent` are both XML with an
`<Ethanon>` root; a `.esc` entity instance embeds the full entity definition inline
*alongside* an `<EntityName>foo.ent</EntityName>` reference
(`testbed/GeneralTests/scenes/customDataTesting.esc`).
