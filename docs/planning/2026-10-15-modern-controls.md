<!-- Provenance: the final (second-pass) design of the modern on-screen controls, written 2026-10-15 by a read-only design workflow
     (three maps, a designer, two adversarial reviewers) and copied here unchanged below this note. Planning step numbers inside it are
     provisional ("CTL-n" is the working name; each step takes the next free number in 2026-09-11-magic-portals-remaster.md when it
     lands, CTL-1 = step 177). The "CTL/..." and "out/parity/..." paths it names are working files in the gitignored out/ and may be gone.
     The questions in its section 6 are the owner's and are OPEN until he answers them. -->

> **STATUS, 2026-10-22 (planning Step 198, CTL-13): DONE.** The owner answered Q4 (section 6) in chat on 2026-10-22: "ctl13 - modern controls should be default contorls."
> `default_mode` in `game/data/controls.json` is `"modern"` (one data value; its note says OWNER 2026-10-22), so a player's run with nothing saved plays Modern - a new install and an
> existing save with no `controls.json` alike, no migration and no prompt - while every developer's run (`--fixed-step`, `--route`, `--visit-levels`), `--controls classic` and every suite
> stay classic and byte-identical. The census list of 1.9 (after Step 186's split: `tests/golden/controls_census.json`) was **accepted by the owner's instruction, not reviewed item by
> item**. The Options toggle is the way back. Section 5's CTL-13 row and section 2.1's "classic until CTL-13" read as history from here.

> **AMENDMENT, 2026-10-16 (planning Step 186, CTL-6b): THE OWNER SPLIT THE WALK ARROWS.** After he saw the layout on his screen the owner decided, in his words:
> "moving stick should not be only on the left side. you should put one button to the left and one to the right like the classic ones." **This overrides his
> 2026-10-15 answer that the two walk arrows be grouped in the bottom-left.** The arrows now stand **split**: `walk_left` inset in the bottom-LEFT corner and `walk_right`
> inset in the bottom-RIGHT corner, each by the same margin (44 px to the box, on both edges of its corner), as the classic pair stands but inset. Restart and pause (top
> right) and clear-portals (top left), the margin and the three looks are unchanged. The grouped arrangement of planning steps 180 to 185 is kept as an ALTERNATIVE that a
> capture or a test asks for (`--controls modern,arrows=grouped`, data key `layout.default_arrows`, never saved), and `kLayoutVersion` and `layout.version` are 2 (a
> `controls.json` saved at layout 1 keeps its mode, size and opacity and drops its moves).
>
> **Superseded by Step 186** (everything below describes the GROUPED arrangement unless Step 186 says otherwise): **1.2** (the default places: at 16:9 and size 0.7 `walk_right`'s box
> is (407.6089, 208.4978)-(439.4667, 240.3556) and its hit (407.6089, 208.4978)-(455.1111, 256), 47.50 x 47.50 units, as the left arrow's hit is (0, 208.4978)-(47.5022, 256));
> **1.4** (the pair's seam: there is none; each arrow's hit is its box plus the strips to its own two view edges, half-open, growth 0); **1.6** (the hint clears the LEFT arrow only,
> and where the text cannot clear both it is drawn over the right arrow); **1.9** (the census: the split arrows cost NO recorded winning shot at any of the nine window shapes, so
> 1-20 and 3-27 leave the list; the accepted lists of both arrangements are in `tests/golden/controls_census.json`); **2.3** (the Options thumbnails show one arrow in each
> bottom corner); **3.2 and 3.3** (the adjustment screen: the 5.83 and the 22.86 clearances against the pair's top at size 1.4, and the pair top 176.64, are the GROUPED pair's. **With the split
> arrows the editor has a REAL collision the grouped pair did not have: its Back button is at the bottom-RIGHT, box 38.68, inset 15.64 (3.2), which is where `walk_right` now
> stands - at 16:9 the preview's walk_right box (407.6, 208.5)-(439.5, 240.4) lies wholly inside Back's (400.8, 201.7)-(439.5, 240.4) - the "tiles over controls" failure design
> L5 forbids.** The widget column (W/2 +- 54.47) is clear of both arrows at every supported aspect and size (the left arrow's hit ends at 79.36 at size 1.4, the right one's begins at
> W - 79.36, 240.64 at 5:4 against the column's 214.47), so the Restore-tile clearances against the pair's top do not arise.) **CTL-11 must re-derive the editor's clearance numbers
> with `CTL/arith2/editor_clearance.py` first, for the split arrows, and must place Back (or the preview) so that no widget covers a control.**

> **AMENDMENT, 2026-10-17 (planning Step 192, CTL-11): THE ADJUSTMENT SCREEN'S BACK BUTTON STANDS AT THE TOP LEFT, UNDER THE CLEAR-PORTALS PREVIEW.** Step 186 split the walk arrows and put `walk_right` where this
> design's Back was (bottom-right); the owner's assistant proposed, and on 2026-10-16 the owner accepted in chat ("proceed with your suggestion"), that **Back stands in the TOP-LEFT corner, BELOW the clear-portals preview control**, not at the design's
> bottom-right. Back is a **fixed place**: its left is the layout's side margin (44 px = 15.644 units) and its top is the layout's top margin plus the LARGEST corner box (size 1.4: 57.344) plus Magic Rampage's own 16 px clearance (5.689), all from the safe
> rectangle's corner: **top 78.677, box (15.644, 78.677)-(54.329, 117.362), 38.684 square (0.85 x 128 px)**, at 16:9 and every other aspect, and it never moves as the size is stepped. It clears clear_portals' box by **5.689 units (16 px) at size 1.4, the
> least, and by 22.073 at the default size 0.7**, at every size, aspect and safe area (nine aspects x eleven sizes x four inset sets: the least is exactly 5.689). With the arrows in the corners no other default box meets Back at any size or aspect (the left
> arrow's top, 176.64 at size 1.4, is 59.28 below Back's bottom, which a top and a bottom inset of less than 59.28 units between them keep).
>
> **Superseded by Step 192** (everything below describes the design as first written unless Step 192 says otherwise): **0.1**'s row "B 5 W1: editor Back at MR's top-left | overlaps the clear-portals preview ... Back goes to the bottom-right" (the overlap is real and it
> is avoided by standing UNDER the preview, not by the bottom-right corner); **3.2**'s "Back bottom-right, box 38.68, inset 15.64", its table of clearances and its three numbers (the **5.83** of Restore against the pair's top, the **22.86** of `walk_right` at 16:9 and size 1.4,
> and the pair's top **176.64**: they are the GROUPED pair's) and the paragraph under it; **3.5** L5's "Back moved off the clear preview"; **3.6** L-2's "Adjust's Back is bottom-right and returns to Options, which has nothing there" (Back is now inside Options' own credits-back
> button's rectangle, so the return press is pinned to act on exactly one screen: `BackLeavesAdjustAndOptionsTakesNothingFromThePress`); **CTL-11**'s tests `AtSixteenNineTheLeastClearanceIsTwentyTwoPointEightSixAtSizeOnePointFour` and `RestoreClearsThePairByFivePointEightThreeAtSizeOnePointFour` (renamed
> `AtSixteenNineTheLeastClearanceToTheTilesIsThirtyOnePointThreeSevenAtSizeOnePointFour` and `RestoreAndTheColumnClearTheSplitArrowsAtEverySizeAndAspect`), its G2 ("the clearance table of 3.2 equals the measured one to 0.01": the table is re-derived below and `out/parity/program/CTL/CTL11/editor_clearance.py` is
> the contract) and its "fails if" line about Back at the top-left (it is now the DECISION: Back is moved off the clear button, under it); **6**'s **Q11** (answered by the owner's assistant's proposal, accepted: top-left, under the preview); and **Appendix A**'s last paragraph (its 5.83 and 22.86).
>
> **The re-derived clearances, the split arrows and Back at the top left** (`CTL/CTL11/editor_clearance.py`, Python arithmetic; a widget has a BOX, Magic Rampage's, and a FACE, the box shrunk by its frame's margin, which is what a tap hits; the table is of the BOXES, the larger). The column is W/2 +- 54.471 across and 50.916 to
> 170.809 down at every aspect (rows at 0.27, 0.40 and 0.57 of the height; two rows' boxes overlap by 3.129 units and their faces are 7.111 apart). The least distance from a default control's box to the six tiles (Back apart), size 0.7 / size 1.4: 5:4 **overlaps** (restart against the size row's plus tile: -3.41 / -22.07); 4:3 7.25 / overlaps; 3:2 28.59 / overlaps (-4.18);
> 16:10 41.39 / 8.62; **16:9 64.14 / 31.37**; 20:9 121.03 / 88.26; 21:9 135.25 / 102.49; 32:9 291.70 / 258.93; 2:1 92.59 / 59.82. The meetings: **restart's box meets the size row's plus tile at 5:4 at every size, from size 1.1 at 4:3, at size 1.4 at 3:2, and never at 16:10 and wider, and no other control's box meets any widget's box at any
> size or aspect**; restart's box meets a widget's FACE (what a tap hits) from size 1.1 at 5:4 and from 1.2 at 4:3 only. No default control is wholly under a widget anywhere. The arrows stand in the corners: 26.169 units from the column at 5:4 and size 1.4 (93.72 at 16:9), so Restore's old "5.83 against the pair" does not arise (the column's bottom, 170.809, is 5.831 above the arrows' top
> at size 1.4, in the middle of the screen against the corners). Widgets win every hit test in Adjust, and Restore recovers anything a widget covers.

> **AMENDMENT, 2026-10-17 (planning Step 193, CTL-12): THE DRAG.** What 3.3 left to the step is decided here, and the code wins where this and 3.3 differ. **(1) The forbidden zones are the seven widgets' BOXES, Back included** (not
> the column's bounding box, which is the six tiles' hull and covers the readouts and the gaps between them, and not the faces: a press is won by a widget's FACE, a control is kept off its BOX); the clearance of 16 px (5.689 units,
> `clearance_px`) is kept from the other four controls' boxes and none from a widget. **"Meets"** is one predicate everywhere (the walk, the five boxes' test, a size step): apart by less than the clearance on both axes, and a gap that is the clearance
> to a billionth of a unit is clear (at size 1.0 an arrow and the clearance are 144 px, 51.2 units, a multiple of the tenth a move is kept to, so the exact gap happens; the 10,000-gesture fuzz found it). **(2) The walk:** the control goes toward the
> pointer minus its offset, clamped by the one shared clamp, x first and then y, in steps of at most `adjust.drag.step_units` (0.5), every step validated on the LAID-OUT box (`MoveFor` then `Boxes`: the clamp and the tenth are applied), a refused
> step bisected to the farthest tenth. So a control slides along an obstacle (the refused component stops, the other goes on), stops within a tenth of it, **cannot jump a neighbour at any pace** (for a drag blocked along one axis, or sliding along one face, a pointer at 1 unit a tick and one at 40 end on the same
> place, the review's F9; a diagonal drag that ends inside an obstacle can end beside it or above it depending on how coarsely the pointer is sampled: Step 193's LEFT FOR THE OWNER 1), and a control below a neighbour that the pointer has passed catches up with the pointer's x at once. **(3) A control that STARTS meeting an obstacle** (restart at 5:4 and the larger sizes meets the size row's plus tile) takes a step that
> makes it meet that obstacle LESS and refuses one that makes it meet more: it can leave, and never goes deeper. **(4) Kept once:** the held control's place lives in the drag, not in the tuning in force; the release says "keep" iff the tuning is not the one at the
> press, and the layer keeps it through `editTuning` (one save). `Back` or `Esc` while a finger holds a control **drops** the gesture (nothing kept); a pointer that leaves the window ends it where it stands (and keeps it). **(5) A size step** sends home both of every
> meeting pair, and if a pair still meets (a control sent home stands where a third, moved one stands) everyone goes home; widgets are not asked. **(6) Not changed:** the layout number stays 2 (a move's meaning, units from the anchor, is what Step 187 made it);
> a moved control keeps its original anchor (3.3.3's limit, recorded and not built).

# CTL design: Magic Rampage's modernised controls for Magic Portals Enhanced (FINAL, second pass)

Designer's final step plan, 2026-10-15, written after two independent reviews (`review1.md`: parity and regression; `review2.md`: player experience and design fidelity). Read-only role:
nothing in the repository, in Penumbra or in the engines was edited, built or run; the scripts I ran are arithmetic over the committed routes and the gitignored converted levels
(`CTL/arith2/*.py`, `python -I`, no game and no test executable). Sources: the three maps (`CTL/mapA_port.md`, `mapB_magic_rampage.md`, `mapC_art.md`), re-checked against the code wherever this plan leans on them
(`MagicPortalsLayer.cpp` 7885-8108, 8552-8790, 9985-10011, 12200-12277; `sim/Hud.*`, `sim/LevelEnd.cpp`, `sim/UiLayer.cpp`, `sim/Switches.hpp`, `main.cpp` 60-345, `game/CMakeLists.txt`, `game/sim/CMakeLists.txt`,
`tests/CMakeLists.txt`, `tests/test_mp_layer.cpp` 100-135 and 6231-6330, `ui.json`, `camera.json`, the font `Matura84_shadow.fnt`) and against Penumbra's `docs/spec/42-magic-rampage-screenpad.md`
(read only). Planning step numbers below are **provisional**: the planning doc's last step is 176, the next free number is 177, and each step takes the number that is free *when it lands* (finding 3 of review 1).

> **PERMISSION RECORD: STILL THE STUB.** `docs/permission.md` still carries its "PASTE THE ACTUAL GRANT HERE" placeholder and says nothing
> about Magic Rampage (another Asantee game). CLAUDE.md says to state that and to assume no scope; I restate no verbal grant. What that
> blocks, and only that: a **release package that contains Magic Rampage's art**, and public screenshots of it. It blocks no step beyond a recorded
> go-ahead: the art is read at run time from a gitignored folder, never committed, and every control has a fallback that holds no Asantee pixel.
> The two steps that touch Magic Rampage's pixels (CTL-1, CTL-8) carry a **precondition**: the step doc records that the owner's 2026-10-15 request
> for Magic Rampage's own buttons is the go-ahead for local, gitignored use, and that shipping stays gated by Q2. Either step can be dropped
> without changing any other step.

> **ONE POINTER.** CLAUDE.md and README fix one pointer path (the mouse stands in for touch). The owner's "the right thumb stays free to fire
> portals" is a property of the layout only on a device with two contacts. Under one pointer a held walk arrow still blocks a shot, exactly as
> a held classic pad does; on the desktop the keyboard (A/D, arrows) already walks while the mouse fires. **Every core step (CTL-1 to CTL-13)
> is complete under one pointer and the grouping is, until CTL-14, a placement and a margin, not a second hand.** CTL-14 (engine contacts) amends
> a standing rule and is gated on an owner ruling (Q1); whether it matters at all depends on Q0 (does the port ship on a touch device).

> **PRECONDITIONS (review 1, finding 3).** The DEVLOG's last entry ends "Next is wave 4d: the final comparison re-taken on the fixed build", and a comparison run is using `build/` and the desktop now.
> **P1.** CTL-1 (Python and docs: no build, no launch, no file the other session reads) may run now.
> **P2.** CTL-2 onward relinks `build/game/MagicPortals.exe` or `build/tests/test_mp.exe` and launches them: they start **when wave 4d has reported done**. The alternative is a second tree `build-ctl/` (a copy of `play_build.bat`'s recipe with validation ON and the
> data-path cache variables copied from `build/CMakeCache.txt`; every gate and every capture script's `EXE` then names that tree); I recommend waiting, because a second tree is another full engine build and a second set of binaries for Smart App Control to judge.
> **P3.** The port reads its own data from the source tree at run time (`MAGICPORTALS_PORT_DATA_DIR`, `game/sim/CMakeLists.txt:135`): **no step mutates an existing `game/data/*.json` in place**; mutation tests read a scratch copy through a test-only override (CTL-2).
> **P4.** The planning record is shared: a step takes the next free number when it lands, working documents say "CTL-n", and commit order on `main` is the order of numbers. If CTL lands before 4d's own close, 4d's record names the exe md5 it measured.

---

## Changes after review

Every high and medium finding, checked against the code (or the repository's data) before it was accepted; the reviewer who is partly wrong is said to be. "Where" names the section of this file.

| Finding | Sev | Verdict and evidence | What the design now does | Where |
|---|---|---|---|---|
| R1-1 the zero-gravity level | H | **Right.** `out/levels/level0c.tscn` has no `no_gravity` (count 0); `level1c`, `level17c`, `level18c` have it; `test_mp_zerog.cpp:1, 65` say level1c to level18c; the weightless levels are 4-02 to 4-19. | K4, CTL-2's quirk pin and CTL-6 use `level1c`; a negative control (`level0c` shows the pair) sits in the same pin; 1.6 says 4-02 to 4-19. | 1.6, K4, CTL-2, CTL-6 |
| R1-2 S3 sees only f420 | H | **Right.** `capture_CMP2.sh` is `--level <n> --frames 420`, no input: the slide, pulse, decay, claims and gates never run. | S3 says what it proves (the settled frame). New evidence: an **A-side set** (A1-A9) and a **menu set** (M1-M5) taken at CTL-2 and md5-compared; an in-process **digest test** of every HUD quad of 128 levels plus dynamic snapshots, run at every C++ step; CTL-3's gate is the CTL-2 pins plus the named existing layer cases with check counts. | S3, CTL-2, CTL-3 |
| R1-3 collision with wave 4d | H | **Right.** Every gate runs in `build/`; the port reads its data from the source tree; the record is shared. | P1-P4 above; planning numbers provisional; "CTL-n" in working documents. | header, P1-P4, S8 |
| R1-4 vacuous modern gates | H | **Right.** D9 makes "modern never engaged" a quiet failure (a typo in `controls.json` forces Classic, and the census would pass). | `MagicPortalsLayer::ControlsStyle()` seam (CTL-5); every modern gate prints `style=modern` and fails otherwise; the modern sweep must differ **inside** the union on all 128 levels; every capture log is grepped for `controls: modern`; the swallowed-tap set overall must be non-empty. | N1, CTL-6 |
| R1-5 / R2-F3 census excuse | M | **Right.** `Route.cpp:331` has four causes; one swallowed tap excused any drift. | Identical per-tick records **up to and including the first swallowed tap**; a second variant (modern layout and draw, classic claims) must be identical on all 128 with no exemption; each swallowed tap is classified through `ControlAt` and counted per route and per control. | CTL-6 G3 |
| R1-6 / R2-F5 K9 cannot save | M | **Right.** The flag that shows the entry forbids saving; a dev run hides the entry. | A forced style (`--controls modern`) starts the run in that style with a **memory-only store**: tiles and Adjust work for the run, nothing is written. The persistence captures (K9, K12) are **player-style runs** (no `--controls`, no `--fixed-step`) on a scratch `--saves`; S7 names that directory as the one place a `controls.json` may appear. R2's `--controls ui` is **rejected**: a third mode and a truth-table row for evidence the player-style run already gives. | 2.1, K9, S7 |
| R1-7 the `--data` lever | M | **Right.** `--data` sets `paths.data`; `portData` is a compile-time default (`MagicPortalsLayer.hpp:162`); `TestPaths()` sets only `prisms`. | A **test-only** environment override `MP_TEST_PORTDATA` read by `TestPaths()`; mutations run the same binary against `CTL/mut/<name>/` copies of `game/data`; expectations are hard-coded (63.99 / 64.01), never read from the mutated rules. | CTL-2, CTL-4 |
| R1-8 S9 budget | M | **Right.** | S9 counts **launched new binaries**; compile-only iterations build the static libraries (`MagicPortalsSim`, `MagicPortalsGame`: no exe, nothing for Smart App Control to judge); golden constants live in `tests/golden/*.json` read at run time (a correction relinks nothing); a per-step launch table. | S9, 5.1 |
| R1-9 forty sweeps | M | **Right.** `--controls classic` takes the same branch as no flag. | Digest at every step; the full 128-launch sweep only at the four steps whose code classic can reach (CTL-3, CTL-6, CTL-9, CTL-13) and once at the end, **verifier only**; `--controls classic` is a six-level spot. | S3 |
| R1-10 / R2-F2 corner hits and destructive presses | H/M | **Right.** Restart is `loadLevel`, clear is `KillAll(true)`, both on the press (`8717-8746`); the first design's corner hits were 3.9-4.5 times the classic ones. | Growth is **per control and per side, 0 by default**; corner buttons are claimed on box plus edge reach plus half of a seam, and **act on the release inside the hit rectangle after a press inside it** (the original's own rule, data `corner.act_on`); a dead-zone budget test. | D2, D11, 1.4, 1.9 |
| R1-11 `onPad` gated by `shown` | M | **Right** (`Camera.cpp:134, 186, 202` read it; classic's is ungated, `8622-8623`). | Modern's `onWalkControl` is **ungated like classic's**; both sides pinned on `level1c`. | D12, CTL-6 |
| R1-12 menu baselines | M | **Right.** `UI/after/sweep` is purged; `capture_UI.sh` is a level sweep, not a menu set. | M1-M5 (main menu, Credits, Achievements, Worlds, Levels) and A6 (the pause over a level) captured at CTL-2, byte equality at CTL-9 and CTL-13. | CTL-2, CTL-9 |
| R1-13 `--trace` | M/L | **Right.** `--route-trace` exists on the exe only with `--route`; `--trace` only on `test_mp test_mp_routes`. | An in-process pin: a scheduled tap on the modern right arrow against `ScheduleDevHold`, position and camera compared every tick. K6 stays a picture. | CTL-6 |
| R1-14 developer predicate | L | **Partly.** True that a windowed `--tap` run is a player-style run; but counting `--tap` as a developer run would hide the Options entry from the K9 capture it must run (R1-6). | Predicate unchanged (the three rig flags); every capture recipe greps its log for `controls: modern`; README says `--controls classic` for a windowed scheduled-input run after CTL-13. | 2.1, CTL-13 |
| R1-15..21 | L | All **right** (15: `ResolvePolicy` is in `sim/Controls`, main only calls it; 16: S5 compares with `HEAD`, the engine pin joins S6; 17: PadArt moved into CTL-4 and the flag plumbing into its own CTL-5; 18: S10 purge and stopping points; 19: hint clamp, width measured; 20: a test-only camera seam; 21: validation clause and window-size check for modern captures). | as listed | S5, S6, S10, 1.6 |
| R2-F1 placement vs the levels | H | **Right, reproduced to the digit** (`CTL/arith2/deadzone_census.py`: 25 of 110 levels at the first design's size 1.0, 37 at 1.4; classic 0), then extended (`census2.py`, `final_layout.py`, `census_aspects.py`). Caveat the reviewer states: recorded routes, not proof that no other aim exists. | The default is chosen **from the census**: one margin of 44 px for all five (the owner's "same margin"), **default size 0.7** (the pair's top at 208.5, under the character's row at 206), **no growth toward the world**. Result at 16:9: **2 levels** (1-20, 3-27) for the pair, 2-06 for restart, 1-09 and 3-14 for clear (classic: 1-09). The census is a **pure test in CTL-4**; the flip (CTL-13) waits on it. Three presets in data for K2 (uniform, balanced, mr). | D1, D13, 1.2, 1.9, Q3 |
| R2-F4 the character under the pair | M | **Right in kind, smaller in size.** By the reviewer's estimate 16 of 110 levels at the first design; at the new default **1 (2-02)** (`final_layout.py`, section "the character at level start"). | Recorded in CTL-6; `idle_scale` in data (default 1.0, MR's 0.25 rule is the option); Q10. | 1.5, Q10 |
| R2-F6 hint overflow | M | **Right.** The text is 639 font px x 0.4 = 255.6 units of advance, **258.4 to its last glyph's edge** (measured from `Matura84_shadow.fnt` and `camera.json` `units_per_font_px`); at 4:3 and 5:4 the first design ran off the screen. The lesson's arrow (y 64 to 197) clears the new pair (top 208.5). | `HintLeft` clamps so the text ends 8 units inside the view and measures the width from the font at run time. | 1.6, CTL-7 |
| R2-F7 the platform question | M | **Right.** Contacts come only from the Android backend (`engine/src/platform/android/AndroidApp.cpp:771-778`); the port builds for Windows. | New **Q0** ahead of Q1; the growth defaults to 0 (right for a mouse and a thumb on a margin); CTL-14 stays last unless Q0 says "phones". | Q0 |
| R2-F8 .. F14 | L | F8 right (adopted); F9 half (slide-along adopted, the nearest-edge anchor recorded as a limit); F10 right (tags split); F11 = R1-2; F12 right; F13 right; F14 right. | F8: while unlocked the editor draws each hit rectangle (alpha 0x22) and a character-row guide; F9: the drag tries x and y separately; F10: tags split into value / application / magnitude, MR's alpha rule listed; F12: loader checks the PNG size against the manifest, editor tiers named; F13: the flip depends on CTL-12 and the census sign-off; F14: taps from a level to Options recorded in CTL-9. | 1.1, 3.3, 4.2, CTL-13 |

---

## 0. The decisions in one page

| # | Decision | Where argued |
|---|---|---|
| D1 | Modern default (16:9, view 256 units tall): **one margin M = 44 px = 15.64 units** from each control's two corner edges, for all five controls (the owner's "inset by the same margin"). The two walk arrows are one pair in the bottom-left, 11.38 apart (MR's 32 px), box = 45.51 x **size**; **default size 0.7** (box 31.86, solid face 24.9, pair top at y 208.5, below the character's row at 206), chosen **from the census** (D13), not from Magic Rampage's 1.0. Restart, pause and clear-portals are boxes of 0.9 x 128 px x max(size, `corner.min_size` 1.0) = **40.96**, whose solid body is exactly 32.0, the classic corner button's size (so a small arrow does not shrink the corner buttons). Three presets in data (`uniform` 0.7 / margin 44 all round, the default; `balanced` 0.8 / bottom 34 px; `mr` 1.0 / bottom 11 px) so K2 can show the owner the three looks (Q3). | 1.1, 1.2, 1.9 |
| D2 | **Hit areas = the box, plus the reach to the view edge, plus the middle of a seam; growth toward the world is 0 by default, per control, in data.** A side whose box edge is within 48 px (`edge_reach_px`, = margin + 4 px) of the view's (safe-area) edge extends to the edge; a gap of at most 32 px (`seam_px`) to a neighbour's box is split at its middle; `hit.grow_px.<id>` (0) is the only growth toward the world and is the answer to Q0/Q6. Rectangles, half-open, replace the classic quarter circle only in the modern style. | 1.4 |
| D3 | The classic style is **the existing code, literally** (`Hud::PadRect/Place/OnPad` called, not re-expressed). A new pure module `sim/Controls` is the one place every consumer of control geometry asks (draw, claim, camera `onPad`, lift gate, hand gate, ring); its classic branch forwards. `Hud.*`, `ui.json`, `LevelEnd.*` and `Camera.*` are not edited by any step. | CTL-3 |
| D4 | **Which style a run plays, in this order:** `--controls <classic\|modern>` flag; else a developer run (`--fixed-step`, `--route`, `--visit-levels`) is classic; else the player's saved `controls.json`; else the shipped default (data; **classic until CTL-13 flips it to modern**). A bare `Paths` (every suite, every route) is classic. So the unedited f420 sweep script reproduces `WALL/after/sweep` at every step, including after the flip, and no capture script is edited. | 2.1 |
| D5 | The toggle lives in a new, smallest-possible **Options screen** (a menu state, the Credits template) opened from a main-menu button in the top-right slot the original's omitted Facebook button had. Not in the pause (the pause switches are drivable only by new `--press` values and a mid-level rebuild would call `unloadControls`, which clears the pause, end and popup image maps). | 2.3 |
| D6 | The setting is **`controls.json`** in `Paths::saveDir`, its own file and store (not a third key of `audio.json`, which `Switches::Store` rewrites from two members). It stores **deltas only** (a size, an opacity or a move equal to the default is absent), so changing a default never strands a player on the old one. Layout number in the file from day one. | 2.4 |
| D7 | Adjustment (size, opacity, lock, drag, restore) is its own later pair of steps, a menu state off Options (so `--tap`/`--drag` drive it through `touchThisTick`). Size and opacity are Magic Rampage's grids (0.4 to 1.4 by 0.1; 0.2 to 1.8 by 0.2); a move is in design units from the control's anchor, never scaled by size, clamped whole inside the view. **Static rules, no per-frame push-apart.** While unlocked the editor draws every hit rectangle and a character-row guide, so the player sees what the placement costs. | 3 |
| D8 | Art: Magic Rampage's buttons, read at run time from `reference/magic-rampage/controls/` (gitignored by the existing unanchored `reference/` line, no `.gitignore` edit), made by a committed tool that holds no Asantee bytes. Fallback per control: Magic Rampage file (size checked), then the original's own sprite (restart, pause, clear only), then a **drawn** glyph written to `prisms`, then a flat quad. The walk arrows are never dropped by the "unreadable picture, no control" rule. | 4 |
| D9 | `controls.json` (game data) is read **outside** the fatal attach chain and only when a run can play modern: a typo there makes modern unavailable with one error line and cannot touch a classic run, the parity rig or a suite; and because that is a quiet failure, **every modern gate proves that modern engaged** (N1). | 2.5 |
| D10 | Magic Rampage's 1498 px row cap is dropped (it centres a five-button row; the owner grouped the pair left). Its attack, jump and rune buttons have no counterpart. | 1.7 |
| D11 | **Modern restart, pause and clear act on the release inside their hit rectangle, after a press inside it** (a press that slides off cancels), which is the original's own Button rule (`MagicPortalsLayer.cpp:8741`); the press still spends the shot track. Classic keeps its recorded deviation (the press). Data `corner.act_on` (`release` or `press`). | 1.4, CTL-6 |
| D12 | Modern's `onWalkControl` (camera `onPad`, lift gate, hand gate) is **ungated by `shown`, like classic's** (`readInput` 8622-8623 asks `Hud::OnPad` whatever `pads` says), so no camera behaviour differs on 4-02 to 4-19 beyond the geometry itself. | 1.6, CTL-6 |
| D13 | **The census is a pure test in CTL-4** (`test_mp_census`): the 128 recorded routes against the default hit rectangles at eight window shapes. The accepted list is data (`tests/golden/controls_census.json`: 1-20 and 3-27 for the pair, 2-06 for restart, 1-09 and 3-14 for clear); a level that joins it fails the build, and the default flip (CTL-13) waits on it. | 1.9, CTL-4 |
| D14 | **Evidence is built to discriminate** (review 1): an in-process digest of every classic HUD quad at every C++ step, an A-side set and a menu set of captures, a style seam so no modern gate can pass with modern off, golden constants as run-time data, mutation by a test-only data override, a launch budget counted in new binaries. | 5.0 |

### 0.1 Where this plan departs from, or corrects, the maps and its own first pass

| Claim | What I found, and what the plan does |
|---|---|
| B 1.3: hit area = box + 16 px per side; first pass: 32 px per side and a 16 px edge snap | The first pass's rectangles blocked **25 of 110** levels' recorded winning shots (**37** at size 1.4) against the classic discs' 0 (review 2 F1, reproduced). Growth toward the world is the expensive part (box only at the first pass's size 1.0 and margin: 20); the box's own top above the character's row is the rest. The plan: growth 0 (data), reach to the edge within 48 px, seam splitting, and a placement whose top is below the character's row. 14 levels (balanced) or 8 (uniform) if a 16 px growth is ever wanted for a thumb: that is Q0/Q6. |
| B 1.3: nothing at the view edge | Without a rule the 15.64-unit strip beside a box fires portals into the corner. The edge reach claims it (checked by the census: it adds no level for the pair, restart or pause, and for clear only 1-09 and 2-24, which the classic button blocks too; 3-14 is the box's own at EM 16). |
| B 5 W1: editor Back at MR's top-left | **Overlaps the clear-portals preview**: checked, true. Back goes to the bottom-right, inset 15.64. |
| B 1.3 vertical-budget note | Re-derived with the *corner buttons* (3.2): MR's editor column meets **restart** at 5:4 at every size, from size 1.1 at 4:3, 1.4 at 3:2, never at 16:10 and wider; Restore clears the pair by **5.83** at size 1.4. No title frame in v1. |
| B/C: the pause art is MR's pill 128x86 | A pill inside a square layout box moves its visible top below restart's. The layout is **art-independent** (every control has a square box); the pause is a framed square (owner's call, Q5). |
| B vs C: shadow margin 13 vs 14 px | Thresholds, not a disagreement (alpha > 200: 13..115; alpha >= 250: 14..114). The data carries the **solid body** (14, i.e. 0.109375 of the box). |
| A and C 4.3 disagree on the art path | A's: a compile definition on the **game target / main.cpp only** and an empty `Paths` default; `test_mp` does not relink for it. The cache name (`SUPERSONIC_MAGICPORTALS_PADART`) still satisfies `play_build.bat`'s regex. |
| A 3.4: "classic via data until the owner flips it" | Kept, and the precedence chain (D4) makes the flip harmless to the rig: `--fixed-step` is classic whatever the data says. |
| A 2.5 row 4: camera `onPad` | Classic's `onPad` is **not gated by the pads being shown**; modern keeps that (D12), and the quirk is pinned on both sides (CTL-2, CTL-6). |
| A 2.5 row 4 and B 3.3 G3 | A menu state entered from the main menu has **no level loaded** (`openMenu` unloads it, `:3128`): the in-level touch tracks cannot be armed when Options or Adjust opens. The leak that remains in play is a resuming or closing press held over a moved control (CTL-7). |
| First pass, 1.1: "corner body is exactly 32.0" at size 1.0 | True only at size 1.0 (40.96 x 0.78125). With a default size of 0.7 a size-scaled corner would be 28.7 (body 22.4, 30 percent under the classic): hence `corner.min_size` 1.0. |

---

## 1. The layout (the modern default) in design units

Units: the view is 256 units tall (`view.json` `height_px`) and `256 x aspect` wide: 455.11 at 16:9; 2.8125 px per unit at 1280x720
(`ui.json` `_about`). Magic Rampage's reference is 1280x720, converted **by height** (equal to MR at 16:9; an assumption elsewhere, B 1.1).
Every number below is regenerated by `CTL/arith2/final_layout.py` (`python -I`, repository root; reads `tests/routes` and `out/levels`, writes nothing).

### 1.1 The constants (all become `game/data/controls.json`, each with its tag)

A tag says *what kind of fact* each number is (review 2 F10): **value** (Magic Rampage's own number), **application** (where this port applies it) and **magnitude** (the number the port chose). DECODED = spec 42; MEASURED = a tool on a file; OWNER = the 2026-10-15 decisions; PORT = a port decision, `"_guess": true` in the file.

| Constant | MR (1280x720 px) | Units | Tag | Used for |
|---|---|---|---|---|
| margin M | 44 | 15.6444 | value DECODED (spec 42 l.78: the side margin of the bottom row); application OWNER ("inset from the corner, the same margin": the same 44 on **both** edges of every corner, bottom included, where MR's bottom row is flush); magnitude kept | the two inset edges of each of the five controls |
| gap G | 32 | 11.3778 | DECODED (l.78) | between the two arrows; between restart and pause |
| box B at size 1 | 128 | 45.5111 | DECODED (l.78) | walk arrow side = B x size |
| **default size** | 1.0 | **0.7** | value DECODED (l.74-76); magnitude **PORT** (census: the pair's top must sit below the character's row; 0.7 is the largest grid value with M = 44 px that does) | the shipped default; one data number |
| corner factor | 0.9 | | value DECODED (l.77: the pause is 0.9 x scale); application PORT (restart and clear as well) | corner box C = 0.9 x B x max(size, `corner.min_size`) |
| `corner.min_size` | | 1.0 | PORT | a corner button never shrinks below 40.96 (body 32.0 = the classic's) |
| hit growth toward the world | 4 to 8 steps of 16 (l.80) | **0** per control | PORT `_guess` | `hit.grow_px.<id>`; Q0/Q6 |
| seam | | 11.38 (= G) | PORT | a gap of at most `seam_px` = 32 px is split at its middle between the two boxes |
| edge reach | | 17.07 (48 px = M + 4) | PORT `_guess` | a side whose box edge is within it of the view's (safe-area) edge extends to the edge |
| clearance | 16 | 5.6889 | DECODED (`isOverlapping(b, 16)`, l.119) | the adjust screen's overlap rule (3.3) |
| body inset phi | 14 of 128 | 0.109375 of the box | MEASURED (Pillow, alpha >= 250, `dpad-left/right/frame`, mapC) | 1.3 |
| size | MR default 1.0; grid 0.4 to 1.4 step 0.1 | | DECODED (l.74-76) | CTL-4, CTL-11 |
| opacity | default 1.0, 0.2 to 1.8 by 0.2 | | DECODED (l.86-87) | 1.5 |
| base alpha | 120 of 255 | 0.4706 | DECODED+MEASURED (`ui.json` `hud.opacity`); **kept** (MR's 0.25 is the option, `idle_scale`, Q10) | so toggling does not change brightness |

Inset means the **BOX** (MR's own rule: "sprite box flush", margin to the box), never the visible face. The face is 3.5 units further in for an arrow of size 0.7 (4.5 for a corner button): 19.1 units (54 px at 720p)
from the edge for an arrow, 20.1 units (57 px) for a corner button. Those are the numbers to quote to the owner.

**Presets** (data `layout.presets`, selectable by `--controls modern,preset=<name>` for a capture, never saved; `default_preset` is `uniform`):

| Preset | Size | Bottom margin | Pair top (units) | Arrow face | Why it exists |
|---|---|---|---|---|---|
| `uniform` (default) | 0.7 | 44 px (= M) | 208.50 | 24.9 units (70 px) | the owner's "same margin" literally; ties the other two at growth 0 (2 levels), wins at any growth above it |
| `balanced` | 0.8 | 34 px | 207.50 | 28.4 units (80 px) | a larger arrow; the bottom margin is 10 px under M (breaks the literal decision; owner may overrule) |
| `mr` | 1.0 | 11 px | 206.58 | 35.6 units (100 px) | Magic Rampage's own arrow size; the face is 25 px from the bottom edge |

All three keep the pair top at or below the character's row (about 206: the standard waist line, review 2 F1); `uniform` is recommended because it is the only one that satisfies the owner's words and costs the fewest levels once any growth is asked for (8 against 14 and 16 at 16 px of growth, 1 against 5 levels with the character behind the pair at level start).

### 1.2 The default places, 16:9, size 0.7, preset `uniform` (units, then px at 1280x720)

| Control | Anchor | Inset (units) | Box side | Box rect, units | Box, px | Centre px | **Hit** rect, units | **Hit** rect, px | Hit area, u^2 |
|---|---|---|---|---|---|---|---|---|---|
| walk_left | bottom_left | (15.6444, 15.6444) | 31.8578 | (15.6444, 208.4978)-(47.5022, 240.3556) | (44.0, 586.4)-(133.6, 676.0) | (88.8, 631.2) | (0, 208.4978)-(53.1911, 256) | (0, 586.4)-(149.6, 720) | 2,526.7 |
| walk_right | bottom_left | (58.8800, 15.6444) | 31.8578 | (58.8800, 208.4978)-(90.7378, 240.3556) | (165.6, 586.4)-(255.2, 676.0) | (210.4, 631.2) | (53.1911, 208.4978)-(90.7378, 256) | (149.6, 586.4)-(255.2, 720) | 1,783.6 |
| pause | top_right | (15.6444, 15.6444) | 40.96 | (398.5067, 15.6444)-(439.4667, 56.6044) | (1120.8, 44.0)-(1236.0, 159.2) | (1178.4, 101.6) | (392.8178, 0)-(455.1111, 56.6044) | (1104.8, 0)-(1280, 159.2) | 3,526.1 |
| restart | top_right | (67.9822, 15.6444) | 40.96 | (346.1689, 15.6444)-(387.1289, 56.6044) | (973.6, 44.0)-(1088.8, 159.2) | (1031.2, 101.6) | (346.1689, 0)-(392.8178, 56.6044) | (973.6, 0)-(1104.8, 159.2) | 2,640.5 |
| clear_portals | top_left | (15.6444, 15.6444) | 40.96 | (15.6444, 15.6444)-(56.6044, 56.6044) | (44.0, 44.0)-(159.2, 159.2) | (101.6, 101.6) | (0, 0)-(56.6044, 56.6044) | (0, 0)-(159.2, 159.2) | 3,204.1 |

Order of the walk pair: left then right (MR's order); the pair's right edge 90.74, its top 208.50. The right arrow's right side faces the world, so its hit stops at its box (growth 0). The classic positions for comparison: pads flush in the bottom
corners 64 units wide (0 to 64 and W-64 to W), restart (W-64..W-32, 0..32), pause (W-32..W), clear (0..32, 0..32).

The corner rows depend only on W and the corner size; pause.x / restart.x at the default: 5:4 (W 320) 263.396 / 211.058; 4:3 (341.333) 284.729 / 232.391; 3:2 (384) 327.396 / 275.058; 16:10 (409.6) 352.996 / 300.658; 16:9 398.507 / 346.169;
21:9 (597.333) 540.729 / 488.391; 32:9 (910.222) 853.618 / 801.280. The bottom-left group never moves with the aspect.

| Size | 0.4 | 0.7 (default) | 1.0 | 1.2 | 1.4 |
|---|---|---|---|---|---|
| arrow box B | 18.2044 | 31.8578 | 45.5111 | 54.6133 | 63.7156 (= the classic 64) |
| corner box C = 0.9 B max(size, 1) | 40.96 | 40.96 | 40.96 | 49.152 | 57.344 |
| pair right edge | 63.4311 | 90.7378 | 118.0444 | 136.2489 | 154.4533 |
| pair top | 222.1511 | 208.4978 | 194.8444 | 185.7422 | 176.6400 |
| pair hit area, u^2 (x classic 6,434.0) | 2,147.1 (0.33) | 4,310.2 (0.67) | 7,219.1 (1.12) | 9,572.5 (1.49) | 12,257.4 (1.91) |

Margins and gap are constants: **the arrows never overlap each other at any size**; only the user's moves can make controls meet. The pair's top passes the character's row (206) between sizes 0.7 and 0.8 at M = 44 px: the player can choose a bigger
arrow in Adjust, and the editor then shows the rectangle it covers (3.3); the census prices the choice (1.9).

**Why size 0.7 is the shipped default** (Q3): the owner asked for "inset by the same margin" and for the right thumb to stay free; with M = 44 px on the bottom edge too, an arrow of 1.0 would put the pair's top at 194.8, on the character's row, and 20 of 110 levels' recorded winning shots would sit under it (25 with the first pass's growth);
at 0.7 the same measure gives 2. The arrow's solid face is then 24.9 units (70 px at 720p): small for a thumb, so Q3 offers `balanced` and `mr`. It is one data number; the owner picks after seeing K2 (5.0).

### 1.3 Box, face and the art's own margin (so the fallback sprites sit where MR's do)

Layout, claims and clamps are all measured on the **box**. Every picture is drawn into a *draw rect* derived from the box: the canonical solid face is the box shrunk by phi
(0.109375) on each side; an image whose own transparent margin is mu is drawn so that its body fills that face:
`drawRect = face grown by mu / (1 - 2 mu)` of its size each side. Per tier, in data: Magic Rampage's files and our generated glyphs mu = phi (drawn exactly into the box);
the original's three corner sprites mu = 0 (body = box, drawn into the face, i.e. 22 percent smaller than their box). Pinned by `TheDrawRectPutsEveryTiersBodyOnTheSameFace`.

### 1.4 Hit areas: the box, the reach to the edge, the seam; no growth toward the world

Classic: a quarter disc of radius 64 about the screen corner, strictly inside (`Hud::OnPad`), 3,217 units^2 each, wider than the drawn disc (opaque to 46); restart, pause and clear
test their drawn rectangle (closed edges), 1,024 each. Both stay exactly as they are.

Modern, one rule for all five (`Controls::HitRects`, the census assumes exactly this):
1. start from the **box**, each side grown by `hit.grow_px.<id>` (**0 by default**, per control);
2. a side that **faces another control's box** across a gap of at most `seam_px` (32 px = 11.38 units), when their extents on the other axis overlap, goes to the **middle of the gap**: the two arrows (and restart/pause) split their gap exactly and never overlap, and no seam is dead ground;
3. a side whose box edge is within `edge_reach_px` (48 px = 17.07 units) of the view's edge (or the safe-area edge) **extends to the view's edge** (the strip beside the box is the control's: a missed corner tap must not become a portal);
4. rectangles are **half-open** [min, max): a point on a shared edge belongs to the later control, so no point is claimed twice; where two rectangles still overlap (diagonal neighbours, only after a user move) the control earlier in the claim order Left, Right, Reset, Menu, Clear wins. Classic's `Rect::Contains` (closed) is not touched.

What it costs (the numbers of 1.9): the pair's hit area is **0.67 times** the classic quarter circles; restart 2.58, pause 3.44 and clear 3.13 times the classic 1,024 (most of it the margin strip a player does not aim portals at); the whole HUD, clear shown, is 1.44 times the classic dead zone
(13,681 against 9,506 u^2 at 16:9: 11.7 against 8.2 percent of the view; 15.7 against 10.9 at 4:3; 16.7 against 11.6 at 5:4). `TheModernHudIsNoMoreThanOneAndAHalfTimesTheClassicsAtDefaultSize` (per aspect, in total; the pair alone at most 1.0) fails loudly if a data change breaks the budget.
**Claims.** The two walk arrows are claimed (held) on their hit rectangle; restart, pause and clear are claimed on theirs and **act on the release** (D11). A touch that goes down on a control spends the whole touch (`m_shotTouch = {}`), as classic.

### 1.5 What the classic pads' decoded behaviours become (the brief's question)

| Behaviour (classic, decoded) | Modern | Why |
|---|---|---|
| Slide-in, 128 units out, 700 ms, smoothEnd, under the opening black | **Kept** for the pair, per arrow from the nearer vertical edge (centre left of the view's middle: from the left); the two numbers (128 units, 700 ms) and the pulse, ring-timing and decay numbers are read **straight from `Hud::Rules`** (`ui.json`), the module takes it as an argument, so they cannot drift apart; the hit rectangle moves with the drawn one | Invisible in a still, free, and the corner buttons already get their entrance as `Hud::Placement`s through `LevelEnd::HudEntered/HudDismissed` (the ray from the view's centre through the anchor, 32 units, 700 ms) |
| Corner buttons' entrance and death-dismissal (UIButton) | **Kept, by being Placements**: `Controls::PlacementFor` returns the anchor, inset and size, so `HudEntered/HudDismissed` run unchanged (pinned at the inset corners) | The restart/pause at a door are CUT, at a death dismissed (decoded/measured) |
| The 14-stride pulse (+40 byte) | **Kept**, same bytes from `Hud::PadAlphaByte`, then x `idle_scale` x opacity | One function, so the end-of-level decay start byte (`m_padEndByte`) is identical in both styles (pinned) |
| level0's tutorial pulse (48 strides, +90) and the ring | **Kept**: the pulse as above; the ring centred on **each arrow's box centre**, size 3.125 x box (classic 200 over its 64) = 99.6 at size 0.7 (142.2 at 1.0) (`_guess`, derived), alpha as classic | It is the original's teaching cue for "walk here"; it follows the pads |
| End-of-level decay x0.98 per tick | **Kept** unchanged; opacity multiplies at draw | Continuous to 0 |
| Pads between the two blacks, restart/pause/clear under them | **Kept** (draw order pinned, quad order) | Decoded |
| No pads on a weightless level | **Kept** (modern pair `shown` false; the corner buttons stay) | Decoded (`MainCharacter` skips them) |
| Not drawn under a pause or a popup | **Kept** | Decoded |
| Corner buttons act on the press (a recorded deviation, `8741-8742`) | **Changed, modern only**: on the release inside the hit rectangle after a press inside it (D11) | The original's own rule; a miss is cheap, a restart is not |
| No pressed look | **New, modern only**: a held arrow or button draws +38 bytes (Penumbra +0.15, `_guess`) | A mouse has no haptics; touch devices expect it |
| Classic alpha 120, no multiplier | modern alpha = min(255, round(byte x `idle_scale` x opacity)) / 255; at `idle_scale` 1.0 and opacity 1.0 the byte is **unchanged**; opacity steps 0.2 x 120 = 24k are whole bytes | Bit-for-bit default |
| MR's size-dependent base alpha `min(0.25 / s^2, 0.25)` and pause `min(t x 0.6, 1)` (spec 42 l.88-89) | **Not copied** (a flat classic byte at every size, so a toggle does not change brightness); `idle_scale` in data (`_guess`, 1.0) is the lever if the owner prefers MR's ghost look (0.53 = 0.25 / 0.4706), and the 1-level estimate of a character behind the pair at level start (2-02) is in Q10 | Review 2 F4/F10: a decision, recorded, not a silent difference |

### 1.6 Around the controls

* **1-21's hint** ("move the camera around", `camera.json` `hint`: text from x 50, y 256-32 = 224, drawn last) runs straight through the pair (y 208.5 to 240.4). `Hint` is not edited; the layer asks
  `Controls::HintLeft(classicLeft, textExtent, view, shownControls)`: modern only; the text starts at the pair's **hit right edge** (90.74 at size 0.7, 154.45 at 1.4) when the row meets a shown control, **clamped so the text ends 8 units inside the view**:
  `left = min(max(classicLeft, pairHitRight), W - textExtent - 8)`. The extent is measured from the font at run time (639 px of advance x `units_per_font_px` 0.4 = 255.6 units, **258.4 to the last glyph's edge**, both checked on `Matura84_shadow.fnt`; the extent is used, so the glyph's edge and not its advance stays inside),
  so a font change cannot break it; the pure function takes it as an argument.
  Results at size 0.7: left 90.74 at 16:9, 16:10 and 3:2 (the text ends 349.1); **74.93 at 4:3** (ends 333.3 of 341.3); **53.60 at 5:4** (ends 312.0 of 320); at size 1.4 the left is 154.45 / 143.20 / 117.60 / 74.93 / 53.60 for 16:9, 16:10, 3:2, 4:3, 5:4. The clamped text still crosses the arrows' rows (drawn last, over them, as classic's text crosses its disc).
  The camera lesson's arrow (view x 85 to 110, y 64 to 197 at the start) ends 11.5 units above the pair's hit top (208.5): `TheHintsArrowEndsAboveThePairsHitRect` pins it (a drag that followed the arrow down must not slide onto walk_right and stop the pan). Classic 50, untouched.
* **The level-start plaque** (x 8 to 72, y 4 to 132, only on a level with a medal, appears 1000 ms, dismissed after 2000 ms, out in 1000 ms) sits under the clear button's box in the top-left. Clear draws over it
  for at most 3 s, and only while a portal exists, as the classic button does over the plaque's top-left corner. Accepted and recorded; no move.
* **The no-portal sign** (64 units at the top-left, 1-15, 2-07, 2-28) is under the HUD; clear is not shown on a budget-0 level. No change.
* **Weightless levels are 4-02 to 4-19 (`level1c` to `level18c`)**, eighteen of them (not 4-01: `level0c` has gravity, `test_mp_zerog.cpp:1, 65, 411-433`): no pair, and the bottom-left stays world (a tap there fires a portal) **except for the camera, the lift gate and the hand gate**, which ask the arrows' rectangles
  ungated like classic's (D12). Pinned both ways on `level1c` (the camera is not dragged by a touch resting there in either style) with `level0c` as the negative control (its pair is shown).
* **Safe area.** `Controls::Insets{left, top, right, bottom}` in units, default zero, is an argument of the layout: the margin M is measured from the safe-area rectangle's edges, boxes are clamped inside it, the edge reach goes to the view edge. The layer fills it from `Supersonic::SafeArea::Get()`
  (window pixels, all zero on a desktop; `engine/src/platform/SafeArea.hpp`, the engine's own answer to rounded corners and notches) x 256 / window height, **modern only**. Verified by pure tests with injected insets; **not verifiable on this machine** (recorded).
  On a device the safe area is the real answer to "too close to the device corners"; the 44 px margin is the aesthetic inset on top of it.

### 1.7 Narrow and very wide windows

Everything is anchored to a corner, so nothing drifts with the width. Supported aspects 5:4 (320 units) to 32:9 (910): at 5:4 and size 1.4 the bottom-left group is 154.45 wide, the top-right pair 141.71 and the top-left button 73.0 (the top row sums to 214.7 of 320), no box meets another
(`NoTwoDefaultBoxesOverlapAtAnyAspectOrSize`, 9 aspects x 11 sizes: 0 overlaps, `final_layout.py`). Below 5:4 the layout is unsupported (the game is landscape; recorded, not clamped). On a wide window the groups sit in the corners and
the hint clears the pair. MR's cap (532.62 units) is **not** applied (D10).

### 1.8 What the mouse player sees

The pair bottom-left with the cursor over it, a brighter arrow while the button is held on it, the three corner buttons inset from the corners, a left arrow whose target is 53.2 x 47.5 units (the box plus the strips to the left and bottom edges and half the seam) and a right one 37.5 x 47.5. While a
walk arrow is held the mouse cannot fire (one pointer); A/D and the arrow keys walk as before and add to the pad as before, so the realistic desktop walk is the keyboard. A tap on a corner button acts when the button comes up, so a press that slides off cancels. No hover effect in play. The Options entry sits top-right
on the main menu; Options shows two tiles, each a small picture of its layout, and the chosen one is lit.

### 1.9 What the layout costs the levels (the census, review 2 F1/F2)

**Method** (pure geometry, no game; `CTL/arith2/census2.py` and `final_layout.py`, becoming `test_mp_census` in CTL-4). Portals open at the tapped point (`Portals::Shoot(registry, atPx ...)`, `MagicPortalsLayer.cpp:8760-8782`). Take every shot tap of the 128 recorded winning routes (`tests/routes/*.json`, 691 taps on 110 gravity levels)
and the level's bounds (`max` marker; `Camera::Clamp`, `sim/Camera.cpp:109`: a level larger than the view scrolls between its edges, one no larger is centred). A tap is **unreachable** when *every* camera position the clamp allows puts it inside the control's hit rectangle (with the rectangle shrunk by `slack` units on its sides inside the view, so a player may aim a few units off a recorded point; slack 0 here)
and *EM* units in from the view edge is the nearest a player parks a target (16 here; the result at the default does not change with EM from 0 to 16). The census **assumes the hit rule of 1.4** exactly. Caveat the reviewer states and I keep: these are recorded routes, not proof that no other aim exists.

**Result at the first pass's numbers** (review 2, reproduced): size 1.0 at margin 44 px with 32 px growth and a snap: **25 of 110** levels (37 at size 1.4); the drawn box alone: 20; classic discs: 0.

| Candidate (16:9, EM 16, slack 0) | Levels blocked (growth 0) | with 16 px growth toward the world | character behind the pair box at level start |
|---|---|---|---|
| `uniform` 0.7, bottom 44 px (**default**) | **2** (1-20, 3-27) | 8 | 1 (2-02) |
| `balanced` 0.8, bottom 34 px | 2 | 14 | 5 (2-02, 2-25, 3-16, 3-24, 3-29) |
| `mr` 1.0, bottom 11 px | 2 | 16 | 5 |
| first pass: 1.0, bottom 44 px | 20 | 21 (25 with 32 px) | 16 |

Why 1-20 and 3-27 stay: the recorded tap (72, 236) of 1-20 and (80, 217.4) of 3-27 sit at the very bottom-left of a level whose camera cannot scroll left of 0; any inset pair of 0.7 or more reaches x 90 there. The classic disc reaches x 60 at that height. It is the one cost of *any* inset pair, named, not hidden.

**At the default across window shapes** (`census_aspects.py`, EM 16, slack 0): the pair blocks 1-20 and 3-27 from 5:4 to 18:9 (512 units), 1-20 only at 20:9 and 21:9, nothing at 32:9; restart blocks 2-06 at 16:9 (the route shoots (380, 30), inside restart's box 346.2 to 387.1) and 2-06 and 4-23 at 18:9; pause blocks nothing; clear blocks 1-09 (the classic button's own false positive) and 3-14 from 5:4 to 18:9 (an upper bound: clear exists only while a portal is alive), and 2-24 at 18:9.
The accepted list is `tests/golden/controls_census.json`; **a level that joins it fails `test_mp_census`**; the owner signs the list before the flip (CTL-13). The census shows only what the *default* costs: a size or a move chosen in Adjust moves it, which is why the editor draws the rectangles (3.3).

---

## 2. The mode

### 2.1 Which style a run plays (pinned by a truth table)

```
Policy ResolvePolicy(optional<string> controlsFlag, bool fixedStep, bool route, bool visitLevels)   // sim/Controls, pure, tested; main.cpp calls it
    -> { bool available; optional<Style> forced; bool saveAllowed; }
Style  ResolveStyle(const Policy&, optional<Style> saved, Style shippedDefault)                   // sim/Controls, pure, tested
```

`ResolvePolicy` lives in `sim/Controls` (review 1, finding 15): `main.cpp` is not linked into `test_mp`, so only the module can be tested; the exe's own call is checked by two launches (`--controls bogus` exits 1, `--help` lists the flag).

| `--controls` | dev run (`--fixed-step`, `--route`, `--visit-levels`) | `controls.json` | available | style | save allowed | Options entry |
|---|---|---|---|---|---|---|
| classic | any | any | **no** | Classic | no | hidden |
| modern (with or without tuning keys) | any | any (ignored) | yes | Modern **as the run's starting style** | **no** (memory-only store: the tiles and Adjust work for the run, nothing is written) | shown |
| none | **yes** | any (ignored, not read) | **no** | Classic | no | hidden |
| none | no | modern | yes | Modern | yes | shown |
| none | no | classic | yes | Classic | yes | shown |
| none | no | absent or unreadable | yes | the shipped default | yes | shown |
| bare `Paths` (suites, routes, `test_mp`) | n/a | never opened | **no** | Classic | no | hidden |

`available == false` means *the feature does not exist in this run*: no store is opened, `controls.json` (game data) is not read, the Options entry is not drawn, the main menu and the pause are byte-for-byte the
original's. That is what makes "classic byte-identical" true for pictures that are not levels too. The shipped default is `controls.json` (data) `default_mode`, **classic from CTL-9 to CTL-12 and modern from CTL-13**.
For an existing save (a folder with `scores.json`, `audio.json`, `achievements.json` and no `controls.json`) the answer is the shipped default: no migration, no prompt; the owner's own play changes only when CTL-13 lands, and
the Options toggle (kept in `controls.json`) overrides it for good. Precedent and its trap: a capture without `--saves` already reads the player's own switches (README 410); the chain makes the *rig* immune because every
capture passes `--fixed-step`.

**What a tile does under a force.** `--controls modern` is a *starting* style for the run, not a lock: tapping the Classic tile changes the run's style for the next level load and writes nothing (the store is memory-only). That is what lets K8, K10 and K11 (which run under the flag, deterministic under `--fixed-step`) drive Options and Adjust.
**How persistence is captured** (review 1 finding 6, review 2 F5): the two persistence captures K9 and K12 are **player-style runs** (no `--controls`, no `--fixed-step`) on a scratch `--saves` directory, which is the one place a `controls.json` may appear (S7). Their `--tap` schedules are tick-scheduled, so not tick-exact, which a toggle does not need.
**Why the developer predicate stays the three rig flags** (review 1 finding 14): counting `--tap`/`--hold` as developer flags would hide the Options entry from the K9 capture; the cost is that a *windowed* scheduled-input run on a player's account plays the saved or default style after CTL-13. The guard is loud instead: every capture recipe greps its log for `controls: modern` (N1), and the README says `--controls classic` for such runs.

### 2.2 The flag

`--controls <classic|modern>[,key=value...]` (main.cpp strips it before `LaunchOptions`, which refuses unknown flags; `--help` and the README flag table get a line). Keys: `preset=uniform|balanced|mr` (CTL-5: which look, for K2), and from CTL-10 `size=1.2`, `opacity=0.6`,
`walk_left=dx:dy` (also `walk_right`, `restart`, `pause`, `clear_portals`), Penumbra's grammar (`TouchTuning::ParseFlag`: `key=value,...`, replaces rather than merges, a bad item prints the usage and exits 1). Any use of the
flag **writes nothing** (Penumbra's `noSave`, L7). `--press`/`--tap` need no new value: the menus are driven by `--tap x,y@t[-t]` and `--drag`, the back key by `--press back@t`.

### 2.3 The Options screen (build the smallest one that does the job)

* **Entry**: one 32 x 32 unit button on the main menu, anchor and origin (1, 0), px (1190, 0)-(1280, 90) at 720p, the slot of the omitted Facebook button (ui3 spec 9; `ui.json:1628`, checked: the main menu has no top-right button; at 5:4 the title's box ends at x = 288 where the entry starts, touching and not overlapping); glyph = a drawn gear (`PadArt`); plays the original's
  `menu_button` press sound like Credits. Drawn and claimed only when `available`. It is **not** inside `MainMenu::Pieces` (whose counts `test_mp_frontdoor` pins): a new `Options::EntryPiece`, drawn by `EmitMenu` after the
  menu's pieces and tested before `MainMenu::ButtonsAt`. **Findability** (review 2 F14): the entry is a drawn gear flush in a corner of a menu whose other buttons are the original's art, and from a level it takes pause, Levels and the main menu to reach it; CTL-9 records the number of taps, and Options from the pause is the first post-flip step (Q7).
* **Screen**: `Screen::Options`, the Credits template (`Screen` enum, `menuStateUp`, `openMenu`, `menuTick` dispatch, `PendingMenu` routing, `layOutMenu`, `EmitMenu`, a `sim/Options.{hpp,cpp}` of pure `Rules/Pieces/ButtonAt`):
  the credits' background and **back** button (the credits' rules: `back_button_credits_screen.png`, 64 x 64 at (0.05, 0.5), the left edge's middle, a place no main-menu button occupies, so the return press cannot also press a main-menu button); the Escape / back key and
  `--press back@t` leave it. Contents: a title (the original's `Matura84_shadow` font, 0.5 units per font px, a word from data) and two tiles **Modern** and **Classic** stacked in the middle (each: a thumbnail of its layout,
  the *pure `LayOut` of that style at 455 x 256 scaled into 64 x 36 units, flat quads*, and its word), the chosen tile lit. From CTL-11: the "Adjust controls" button, drawn only while Modern is chosen.
* Geometry (authored, `_guess: true`, fractions of the screen like `main_menu`; chosen so that **no two Options widgets meet at any supported aspect**, the back button's rectangle ends at x 76.8 at 5:4): title (0.5, 0.14); Modern tile centre (0.5, 0.38), Classic (0.5, 0.62), each 160 x 48 units (thumbnail inset 8 at the left, the word to its right; left edge W/2 - 80 = 80 at 5:4); Adjust button (0.5, 0.84), 160 x 36. Pinned by `NoOptionsWidgetOverlapsAnotherAtAnySupportedAspect`.
* Buttons follow `MenuState`'s touch rule (`TouchDown`/`TouchHeld` as the other menu states use it, the held tint), so a press that begins on another screen's button and slides onto a tile does nothing; the entry press is not the next screen's press (tested).
* Choosing a tile sets the setting, saves (2.4) unless the store is memory-only, plays `menu_button`, and takes effect at the **next level load** (`buildControls` reads the style; no level is loaded while Options is up, so `unloadControls` is never called by a toggle).
* Words come from the original's font; with no original assets there is no menu at all (as today), so no fallback text is built.

### 2.4 Persistence

`controls.json` in `Paths::saveDir` (`%APPDATA%\Magic Portals\`, moved by `--saves`), beside `scores.json`, `audio.json`, `achievements.json`; its own `Controls::Store` (Open, Save by temp file and rename, memory-only when the
directory is empty **or the policy forbids saving**, `Persists`, `FromFile`), opened in `OnAttach` next to the switches, saved on a toggle and **once per gesture** for tuning (not per frame; not if equal to what was last saved: Penumbra L9).

```json
{ "version": 1,
  "controls": { "mode": "modern", "layout": 1,
                "size": 1.2, "opacity": 0.6,
                "move": { "walk_left": [dx, dy], "walk_right": [dx, dy], "restart": [dx, dy], "pause": [dx, dy], "clear_portals": [dx, dy] } } }
```

**Deltas only** (D6): `size`, `opacity` and each `move` entry are written only when they differ from the data's default, so the file of a player who never touched a slider is `{"version":1,"controls":{"mode":"modern","layout":1}}` and a later change of the default size or preset reaches that player. `size` is MR's `screenPadScale`, `opacity` its `screenPadTransparency`, `move` its
`dpad-custom-offset-<name>` (absolute px there; **design units from the control's anchor, screen direction, non-zero entries only** here, so any window shape keeps the place: B 2.4 L10; a control dragged across the screen keeps its original anchor, a limit recorded in 3.3).
CTL-9's store already carries `size`, `opacity` and `move` (read, kept and written back unchanged) and only `mode` is edited; CTL-10 edits the tuning; the format does not change between them. `layout` is the number of the default arrangement
(`kLayoutVersion = 1`): a file with another number keeps size and opacity and **drops the moves silently** (an expected migration, Penumbra L1/E34); whoever changes a default place bumps it, and
`TheDefaultPlacesArePinnedBesideTheLayoutNumber` fails otherwise. Reading is tolerant (per-field defaults, non-finite and out-of-grid values snapped or defaulted, unknown keys ignored); an unreadable file gives the defaults,
one warning, and is overwritten at the next save (as `audio.json` is). The presets are not saved.

### 2.5 The game's own data and the fatal chain

`game/data/controls.json` (the table, 1.1, and the Options layout) has its own strict `Controls::LoadRules`, called in `OnAttach` **after and outside** the `&&` chain of `ui.json` loaders (`:319-348`), and only when
`available`. Failure: `m_modernReady = false`, one `SUPERSONIC_LOG_ERROR`, the Options entry hidden, style forced Classic. A broken file therefore cannot stop a classic run, the parity rig or a suite
(`ABrokenControlsDataLeavesClassicAlone`, with a scratch copy of the data directory read through the test-only `MP_TEST_PORTDATA`).

---

## 3. The adjustment screen (CTL-11, CTL-12)

### 3.1 The data model (`Controls::Tuning`, pure, built in CTL-4, kept in CTL-10)

* `sizeSteps` 4..14 and `opacitySteps` 1..9 as **integers** (the default size steps are the active preset's: 7 for `uniform`'s 0.7, so a step is the grid's 0.1 whatever the default); the values are `n / 10.0` and `n / 5.0` by one division, so a chain of steps ends exactly on 0.4/1.4 and 0.2/1.8 and a step at a limit returns the same
  value (Penumbra `TouchTuning.cpp:22-46, 139-152`, the idea, not the code).
* `move[5]` in design units, screen direction (+x right, +y down), relative to the default place, **not scaled by size** (spec 42 l.78), bound +-1024 and snapped to 0.1.
* `IsDefault()`; `Clamped()` (NaN to default, grids, bounds); `ReadFrom` / `ToJson` (one line, non-zero moves only, written from the clamped value so NaN is never written); `ParseFlag`.
* **Default tuning is the base bit for bit**: `LayOut(..., Tuning{})` equals the literal formula table of 1.2 on every rect, hit and alpha (compared by `memcmp` on the doubles).
* **One placement function used backwards** (Penumbra E28): `Place(id, size, move, view, insets)` forward in the layout, `MoveFor(id, wantedBoxMin, ...)` backward for the drag, **one shared clamp** (the box wholly inside the safe rectangle:
  "a lost button is a bug", where MR clamps nothing), 10,000 round trips tested. A saved move that no longer fits (another aspect) is pulled in at layout and left as stored.

### 3.2 The screen (modal menu state `Screen::Adjust`, opaque flat backdrop (22, 19, 33), widgets in units at 16:9)

Centre x = W/2. MR's rows converted (spec 42 l.52-63): size row y 69.12, opacity row y 102.40, lock row y 145.92; tiles 36.41 square at W/2 -+ 36.27; the number between (the original's font, 20 units tall, `- 1.0 +`
shown as digits only); lock (W/2 - 26.11) and restore (W/2 + 26.11), 49.78 square; **Back bottom-right, box 38.68, inset 15.64**; no title frame. The preview is the five controls at their real size, alpha and place through
the **same** `Controls::LayOut` (G5 of B 3.3: one derived rect set), `clear_portals` forced shown, the pair shown. Widgets' rectangles are the only forbidden zones (a control cannot be dropped on them), the analogue of MR's two.

Clearances of the default controls against the widget column (column: x W/2 +-54.47 for the size and opacity rows, +-51.00 for the lock row; y 50.92 to 170.81), from `CTL/arith2/editor_clearance.py` (the corner box is 40.96 up to size 1.0, so restart's numbers are the first pass's; the pair is the default's):

| aspect | size 0.7 (default) | size 1.4 | restart's box first meets the size/opacity rows at size |
|---|---|---|---|
| 5:4 | restart meets the column (covered partly): its box is 40.96 at every size up to 1.0 | same | every size |
| 4:3 | 7.25 clear (restart) | restart meets it (partly) | 1.1 |
| 3:2 | 28.59 clear | restart meets it (partly) | 1.4 |
| 16:10 | 41.39 | 5.83 (Restore against the pair's top) | never |
| 16:9 | 64.14 (restart) | 22.86 (walk_right) | never |
| 21:9 | 135.25 | 93.39 | never |
| 32:9 | 291.70 | 249.72 | never |

Restore's bottom (170.81) against the pair's top at size 1.4 (176.64): **5.83 units** wherever the column and the pair share x, which is 16:10 and narrower (the E32 "tiles over controls" failure waiting to recur: pinned). No control is ever **wholly** under a widget at any aspect
or size; phone aspects are 16:9 and wider, so the partial meetings are desktop windows (Restore recovers anything covered).

### 3.3 Gestures on a mouse, and the rules

1. **Tap a tile** (all seven widgets): down inside and up strictly inside the same tile, on the release; a slide off cancels; at a limit a tile is inert (alpha 90/255); several taps in one tick act in order; the original's `menu_button` sound.
2. **Unlock** (the lock tile) arms dragging; the screen **always opens locked**; the lock is never saved. Unlocked: a red frame and a thin outline round each box (0.7 unit, alpha 110 +- 70 over 48 ticks, 1 unit and solid while held; no rotation needed, B 2.2), **each control's hit rectangle as a faint white frame (alpha 0x22, from `LayOut`'s own rectangles: spec 42 l.80 draws them too)** and **a thin guide across the preview at the character's row (view y 206, `adjust.character_row_y`, the standard waist line)**, so the player sees where a tap stops being a portal shot and what a bigger arrow will cover (review 2 F8; the editor prices a choice by showing it, it does not count levels).
3. **Drag** (unlocked only): a press on a control's **drawn box** that is not under a widget grabs it; it keeps the pointer's offset (`grabOffset`); the candidate box is the pointer minus the offset, clamped by the shared
   clamp; **a candidate that meets another control's box (+5.69 clearance) or a widget is refused: the pointer's x and y components are tried separately and the control keeps the one that stays valid, so it slides along an obstacle** instead of sticking to it, and a fast move that would jump a neighbour ends on the same valid place as a slow one (pinned with `--drag` at 1 and 40 units a tick; review 2 F9); no tunnelling push, no animation; release commits once, if different from what was saved.
   **A limit, recorded and not built:** a moved control keeps its original anchor (2.4), so a pair dragged across the screen stays attached to the left edge when the window widens, and the clamp pulls it in; MR has neither case because it stores absolute pixels and never re-lays out. Q15's hand placement settles it.
4. **A size step** re-locks and clears the grab (MR l.79), rebuilds the layout, then **sends home** any control whose new box meets another (move to 0); if any pair still meets, all go home. An **opacity step does not re-lock** (MR l.92).
   This replaces MR's per-frame push-apart at 128/256 px/s (stateful, animated): same guarantee (no overlap), no state.
5. **Restore** (enabled only when a move is non-zero): all moves to 0, **keeps size and opacity**, re-locks, no confirmation (MR l.134-138).
6. **Back / Escape** leaves, committing what is unsaved; a key held while the screen opens is not a close (Penumbra's close edge).
7. The press that opened the screen (the Options button's) is **dead until it lifts**; no portal, no walk, no shot can come from any press while Adjust is up (no level is loaded).
8. A press on empty space does nothing. The window resized while open: the layout is recomputed from the stored tuning.
9. DEV: `--tap` and `--drag` drive all of it (`touchThisTick`); `--press back@t` leaves.

### 3.4 Magic Rampage's behaviours: copy, adapt, drop

| MR | Plan |
|---|---|
| One global scale and one opacity, no per-button size | **Copy** |
| Grids 0.4-1.4 / 0.2-1.8, step per completed tap, no hold-repeat | **Copy** |
| Lock opens locked, never saved; a size step re-locks, an opacity step does not | **Copy** |
| Grab the drawn area, not the hit area | **Copy** (the drawn box) |
| Move by summed finger deltas; saved every frame | **Adapt**: one pointer, pointer-offset, saved once per gesture |
| Pad buttons pushed apart every frame; pause `ghost` | **Drop**: refuse at the drag, send home after a size step |
| Two forbidden zones (400x170 and 300x486 px) | **Drop** (MR's HUD): the widgets are the zones in the editor |
| No screen clamp | **Drop**: whole-box clamp inside the safe area |
| Restore keeps size and opacity | **Copy** |
| Absolute-pixel offsets | **Drop**: units from the anchor |
| Wiggle (rotation) | **Drop**: outline cue |
| Reached only from the main menu, only with touch | **Copy the entry** (main menu, via Options); **drop the touch-only gate** (the mouse is the touch here) |
| Sounds (click, grab 0.8, drop 1.2, lock) | **Adapt**: the original's `menu_button` on tiles; drag silent (MR's mp3s are not read) |

### 3.5 Penumbra's E32 to E34 lessons, applied

L1 layout number from day one (`kLayoutVersion`, pinned beside the places); L2 margins that do not grow with size (rule-based layout, not a table scaled by size); L3 a test that no default control box meets a widget at every aspect and
size (3.2's table); L4 ship the census-chosen default (0.7, not MR's 1.0) and fold the owner's own hand placement in later, as data with provenance (Q3, Q15); L5 never put an editor widget over an underlying screen's button without a leak test (Back moved off the clear
preview; the main menu is not drawn under Adjust); L6 pure idempotent layout and hit functions, one clock; L7 `--controls` writes nothing; L8 measure both insets if a layout is ever derived from a screenshot (not done); L9 one save per gesture;
L10 units from the anchor.

### 3.6 Leak guards this port needs (adapted from B 3.3 to the real tick)

* **L-1 Modal menu states.** Options and Adjust are menu states entered from the main menu: no level is loaded (`openMenu`), `fixedTick` returns early to `menuTick`, and `readInput` is not called; the in-level tracks cannot be armed.
* **L-2 Dead entry press** (above, 3.3.7) and a press that **returns** from Adjust to Options or Options to the main menu acts on exactly one screen (the credits' back is at the left edge's middle and the main menu has no button there; the same
  test is written for Options; Adjust's Back is bottom-right and returns to Options, which has nothing there; the return press of each is tested to press nothing on the screen it returns to).
* **L-3 Resume / close swallow (CTL-7, modern only).** The pause's Resume and a popup's close card act on a press; after they close, `readInput` runs on the next tick with the same mouse still down. If a moved control sits under that
  point, `held` would walk the player. A flag `m_swallowUntilUp`, set on the tick game time resumes while the pointer is down and cleared when it is up, makes `controlUnderPointer` return nothing meanwhile. Classic does not set it (byte-identical).
  The closing tick itself never calls `readInput` (existing contract: `pauseTick` returns true and `stepLevel(keyDirection())` runs).
* **L-4 Widgets win** every hit test in Adjust; a control under a widget is grabbable only by its uncovered part.
* **L-5 Hit padding is the control's** (a press in the padding is a pad or button press and arms nothing that fires: the classic rule for walk pads, `m_shotTouch` untouched, release judged at the last held point) and **corner presses spend the whole touch** (`m_shotTouch = {}`), as classic.
* **L-6 Controls beat help blocks**, as now (`helpBlockInput` asked only when `under == nullptr`): a moved control may sit over a block, which then cannot be tapped from there (recorded; the census lists levels where a block's view rectangle meets a hit rectangle).

---

## 4. The art

### 4.1 The state of things

* The `.xapk` is **gone** from `Downloads`; what survives is Penumbra's extract `...\Penumbra-...\out\mr_extract\assets\` (205 MB, **another session's gitignored scratch, which that session may purge**) and Penumbra's finished PNGs (git-tracked there, filed
  in its `LICENSE.md` as "Asantee Games' property"). Penumbra's route (commit the PNGs) is **forbidden here** (THE HARD RULE, no exception for placeholders or fixtures). Nothing is downloaded: the owner re-supplies the package.
* Pixel-exact for "as shipped" images (Penumbra's `dpad_left/right` equal the sprites, max difference 0). Straight alpha is confirmed (`VulkanRenderer.cpp:561-563`, `VulkanPipeline.cpp:45-67`). No magenta key exists in this port.
* Per control (mapC 2.1): walk_left/right = `dpad-left/right.png` as shipped; restart = `dpad-frame.png` + `icon-restart-64.png`; pause = `dpad-frame.png` + two bars (a glyph the tool draws); clear_portals = `dpad-frame.png` + a ring-with-X glyph the
  tool draws (MR has none; Q12). All 128 x 128 RGBA, 128 px only (drawn 1:1 at 720p, 1.5x at 1080p).

### 4.2 On disk, in code, and the fallback

```
reference/magic-rampage/        (ignored by the existing unanchored `reference/` line; no .gitignore edit)
    assets/sprites/   the ~7 source sprites, copied once (so Penumbra's scratch is no longer needed)       <- CTL-1 --extract
    controls/         walk_left walk_right restart pause clear_portals (.png) + manifest.json             <- CTL-1
    editor/           the seven editor glyphs (no panel)                                                  <- CTL-11 (tool --stage editor)
```

* `tools/art/make_mr_controls.py` (new, **code only**, Ivan's own style; helpers copied from Penumbra's `make_mr_touch_art.py`, never imported; run with `tools\parity\.venv` which has Pillow and numpy): `--assets <folder>` or `--xapk <file>`, `--stage controls|editor|all`,
  `--out` default `reference/magic-rampage/controls`, **refuses an `--out` that `git check-ignore` does not say is ignored**, asserts zero exact-magenta and the sizes, `--check` re-derives and compares sha256, writes the ignored `manifest.json`
  (source member, sha256, size, measured body rect). Committed provenance: `docs/magic-rampage-controls.md` (names, recipes, numbers; no pixels).
* Loader (CTL-8): `Paths::controls.artDir`, set in `main.cpp` from a definition `MAGICPORTALS_PADART_DIR` carried **by `main.cpp` alone** (`set_source_files_properties(main.cpp ...)`, so `test_mp` does not relink), cache variable
  `SUPERSONIC_MAGICPORTALS_PADART` (one uppercase word: `play_build.bat`'s `findstr` copies it; default `${repo}/reference/magic-rampage/controls`), flag `--padart <absolute dir>`. **The loader checks each PNG's size against the manifest (or 128 x 128) and drops to the next tier with a WARN when it differs** (a hand-dropped Penumbra canvas of 406 x 406 would otherwise draw mis-scaled with no log line: review 2 F12); the editor's seven glyphs (CTL-11) have the same tiers: Magic Rampage's `editor/` file, a drawn glyph, a flat quad.
* Fallback per control, resolved once per run into `m_modernImages` (not per level, so Adjust can draw them with no level loaded and `unloadControls` never touches them):

| Control | 1st | 2nd | 3rd | 4th |
|---|---|---|---|---|
| walk_left, walk_right | `<padart>/walk_*.png` | (no usable original sprite: quarter discs) | **drawn glyph** (`PadArt`, SDF, 4x supersample, `stbi_write_png` into `<prisms>/modern_<id>.png`, the `modulateImage` precedent) | a flat quad (`prisms` empty: every suite) |
| restart, pause, clear_portals | `<padart>/<id>.png` | the original's own `restart_level_button.png`, `main_menu_shortcut.png`, `clear_portals_button.png` through `menuImage` | drawn glyph | flat quad |

  The classic rule "no picture, no control" does **not** apply to modern: all five controls always exist and claim, whatever is on disk. A mixed look (MR's grey face beside the original's white) is logged at INFO with the tier of each control.
* **Never**: commit any generated PNG (the pre-commit grep would catch it), import from Penumbra's repository, point a definition at Penumbra's `out/`, vendor `Averia48.fnt`, add a negation to `.gitignore`, write art next to `engine/`.

### 4.3 What the owner must decide or provide

1. Whether Magic Rampage's art may be used on a machine other than his, in a release package, in public screenshots (Q2); until the stub is filled the package ships with the fallback.
2. **Run CTL-1 first (it may run now: P1), once the go-ahead line (its precondition) is recorded**, while Penumbra's extract exists; if it is already purged, re-supply `Magic+Rampage_7.8.7_APKPure.xapk` (to `reference/magic-rampage/package/`).
3. Pause as framed square (recommended) or MR's pill (Q5); clear-portals glyph: our drawn ring-and-X on MR's frame (recommended) or the original's own button (Q12).
4. The default size after he has seen K2 (Q3); optionally one hand placement of the five controls on a real device, to be folded into the data once, with provenance (Penumbra L4).

---

## 5. The steps

Planning numbers are **provisional**: each step takes the next free number in `docs/planning/2026-09-11-magic-portals-remaster.md` when it lands (the last today is 176); the CTL id is the working name.

| Id | Planning step | Title | Size | Owner sees |
|---|---|---|---|---|
| CTL-1 | next free (177 today) | Magic Rampage art tool and its first run (Python, docs) | M | nothing yet (files in `reference/`) |
| CTL-2 | next | The classic pointer path, pinned (tests, two seams, baselines) | M | nothing |
| CTL-3 | next | One home for control geometry, classic as a forward (refactor) | S-M | nothing (byte-identical) |
| CTL-4 | next | The modern layout as data and arithmetic, the pad art, and the census (pure) | L | nothing |
| CTL-5 | next | The policy and the flag, classic only (plumbing) | S | `--controls` is accepted and logged |
| CTL-6 | next | The layer plays under the modern layout (`--controls modern`) | L | the modern pair and corners, drawn and pressable |
| CTL-7 | next | The modern controls' own input behaviour (pressed look, swallow, hint, safe area) | M | pressed arrows, the 1-21 hint clear of the pair |
| CTL-8 | next | Magic Rampage's art read at run time (loader, fallback order) | M | MR's buttons |
| CTL-9 | next | The setting and the Options screen | L | the toggle in Options, kept between runs |
| CTL-10 | next | The tuning is kept and applied | M | size, opacity and moves by flag and file |
| CTL-11 | next | The adjustment screen, part 1 (screen, preview, size, opacity) | L | Adjust controls |
| CTL-12 | next | The adjustment screen, part 2 (lock, drag, clamp, restore) | L | drag the buttons |
| CTL-13 | next | Modern is the default for a player | S | modern on first launch |
| CTL-14 | next | OPTIONAL, owner-gated: two contacts | L | a thumb on the pair and a shot at once, on a touch device |

Order and stopping points: strictly sequential except **CTL-1, which has no dependency and no relink and may run now (P1)**. The natural stops, each leaving `main` committed locally and green, are **after CTL-3, CTL-6, CTL-9 and CTL-13** (S10).

### 5.0 How every step runs, and the gates they share

**Looks are the owner's.** A verifier checks the numbers (rects, bytes, counts, the world-pixel gate) and reports the pictures; it does not rule on taste (the art, the size, the glyphs).

**Roles** (as waves 4c-1 and 4c-2): an *implementer* (code, tests, data, planning Step N, DEVLOG entry, README rows; builds, runs each touched suite once, writes `out/parity/program/CTL/<id>/implement.md`); an *independent verifier*
(retakes every gate with its own launches, reads the sources, writes `verify1.json` with `pass`; for a step with a mutation gate, owns the one hooked runner); a *committer* (local commit by `IvanCvetanovic <icvetanovic99@gmail.com>`, no trailer of any kind,
title `Magic Portals: <title>`, last line `Record: docs/planning/2026-09-11-magic-portals-remaster.md, step N`; **no push** without the owner).

**N1. A modern gate proves that modern engaged** (review 1 finding 4; D9 makes "modern off" a quiet failure). Every pin that claims modern behaviour reads `MagicPortalsLayer::ControlsStyle()` (CTL-5) and fails unless it is `Modern`; the route census prints `style=modern` per route and **fails if any route ran classic**; every modern capture's log must contain the `controls: modern` line (the verifier greps; a log without it fails the capture);
the modern sweep (CTL-6) must differ from the classic one **inside** the union on all 128 levels (restart and pause move on every level, so zero differences means modern did not engage); the census must report a non-empty swallowed-tap set overall.

**Standard gates (every step unless it says "n/a"):**

| Gate | Required |
|---|---|
| **S1 build** | Compile iterations are free and unlimited on the **static libraries** (`cmake --build build --target MagicPortalsSim` / `MagicPortalsGame`: no exe is linked, so nothing new for Smart App Control to judge). Then **one launching build**: `cmd //c "tools\parity\port_build.bat --target MagicPortals test_mp"` from Git Bash (`--target test_mp` alone for a step whose gates are pure): **0 lines containing "warning"**. A *full* `port_build.bat` (default target) once at CTL-6 and once at CTL-13 (step 176's deviation 4 is still open). |
| **S2 suites** | each touched suite run **once**, `build/tests/test_mp.exe <suite>`, check count recorded and at least the minimum named in the step, 0 failures; CTL-13 runs `test_mp.exe` once with no argument: **62 suites (57 + `test_mp_controls`, `test_mp_padart`, `test_mp_census`, `test_mp_options`, `test_mp_adjust`), all passed, 0 skipped** (baseline before the wave: 57 suites, 33,730 checks; `test_mp_routes` 2,365; `test_mp_layer` 4,181). A refused exe is "not run", never a pass. |
| **S3 classic evidence, in three layers, each saying what it proves** | **(a) the digest**, in-process, zero launches, **every C++ step**: `TheClassicHudQuadsOfEveryLevelArePinned` ticks all 128 levels to tick 420 through the layer (as `test_mp_pictures --sweep` does) and the dynamic snapshots below, hashes every `ScreenOverlay::Quad` field `EmitHud` produces (FNV-1a) and compares with `tests/golden/classic_hud.json`. It proves the classic HUD's whole draw path, in the states it lists; it says nothing about world pixels. **(b) the A-side, menu and K sets**: A1-A9, M1-M5, K1, K7 (5.0 catalogue), md5 **equal** to the baselines `CTL/baseline/` took at CTL-2, at CTL-3 and at every later step that relinks the exe (CTL-5 to CTL-13); it proves pixel equality of the states a settled frame never reaches (the slide, the pulse and ring, clear shown, the decay, the death dismissal, the pause, the hint, the menus). **(c) the f420 sweep**: 128 launches with the **unedited** `capture_CMP2.sh` pattern (`--window 1280x720 --fixed-step --level <n> --frames 420 --saves <fresh dir holding only the owner's scores.json>`, a copy with only `OUT` changed), **128 of 128 PNGs byte-identical (md5) to `out/parity/program/WALL/after/sweep`**, rc 0, validation ACTIVE and silent in all 128 logs, exe md5 unchanged at the end; **by the verifier only, at CTL-3, CTL-6, CTL-9, CTL-13 and once more after the last step** (about 19 minutes and 0.7 MB a launch: the implementer's evidence is (a) and (b)). It proves that the world at the settled frame and the HUD's final rectangles and alpha did not move (no input is given, so no claim, camera, lift or hand gate runs: those are pinned by CTL-2's pins and the digest). From CTL-6 a **six-level spot** (1-01, 1-02, 1-15, 1-21, 2-02, 4-02) with `--controls classic` added is equal to the first. |
| **S4 routes** | `test_mp_routes`: **128 routes won, each on exactly its census tick** (`out/parity/program/R4c2/census_after_round1.md`), 2,365 checks, 0 failures. |
| **S5 classic arithmetic untouched** | `git diff --stat HEAD -- game/data/ui.json game/sim/Hud.hpp game/sim/Hud.cpp game/sim/LevelEnd.* game/sim/Camera.*` is **empty in every step** (the verifier runs before the committer, so the comparison is against `HEAD`; the committer repeats it against `HEAD~1` after the commit). |
| **S6 hard rule** | `git diff --cached --name-only \| grep -iE '\.(png\|bmp\|jpg\|jpeg\|mp3\|ogg\|wav\|esc\|ent\|enml\|par\|fnt)$'` prints nothing; nothing from `reference/` or `out/` staged; `git status --porcelain` shows no `reference/`/`out/` path; `git ls-files \| grep -ciE '\.(png\|bmp\|jpg\|jpeg\|mp3\|ogg\|wav)$'` prints **0**; `git submodule status` shows the engine at the pin the latest Step records (`5781518` since Step 188; engine/ is never edited, and is re-pinned only by a Step that says so). |
| **S7 the owner's save** | the owner's `scores.json` is md5 `40bfef3ac396b4e94bdd97990401435a` (164 bytes) after every launch (every launch passes `--saves`); **no `controls.json` appears** in any `--saves` directory or in `%APPDATA%\Magic Portals` except the **scratch directory of K9 and K12** (player-style runs, 2.1) and a unit test's own temp directory. |
| **S8 record** | a planning-doc Step with its gates (rows Required / Measured; its number the next free one when it lands, P4), a DEVLOG entry (failures and dead ends included), README rows where the step names them, commit identity and last line as above. |
| **S9 Smart App Control** | counted in **launched new binaries**, not links: per step at most one `test_mp.exe`, one `MagicPortals.exe` and, where the step says so, one hooked mutation runner (5.1); launch each suite once; no loops; a refused exe (exit 126, "Permission denied", ctest "Not Run") is reported as not run, deleted and relinked once from the same sources. **Golden constants are run-time data** (`tests/golden/*.json`, read the way `tests/routes/*.json` is: a per-source compile definition, no relink when a constant is corrected). The owner should expect refusals: the last wave drew 10 in six steps. |
| **S10 housekeeping** | each step ends with `python out/parity/purge_images.py all --delete` (Ivan's standing order) and lists its baselines in `out/parity/BASELINE.txt` (the CTL baseline sets, the sweep it compares against); at about 3.7 GB for a set of sweeps the disk matters. Stop at a boundary (after CTL-3, CTL-6, CTL-9, CTL-13), never mid-step, and save WIP on a branch (Ivan's usage is limited). |

**Capture catalogue** (Git Bash, from the repo root; every path absolute; `mkdir -p out/parity/program/CTL/cap` first; names are `chapters.json`'s and the routes' file names: 1-01 `level0`, 1-02 `level1`, 1-21 `level20`, 2-02 `level1a`, 4-01 `level0c`, 4-02 `level1c`). Pixel positions are for 1280x720.

```
EXE=build/game/MagicPortals.exe;  C="$PWD/out/parity/program/CTL/cap";  SV="$PWD/out/parity/program/CTL/saves"   # SV: a fresh dir with scores.json only
# --- CLASSIC BASELINES: taken once, at CTL-2, from CTL-2's own exe; md5-compared at CTL-3 and at every later step that relinks the exe (S3b). Every one: --window 1280x720 --fixed-step --saves $SV
K1  classic tutorial f420   --level level0 --frames 420
A1  inside the slide        --level level1 --frames 20
A2  ring and pulse          --level level0 --frames 120
A3  clear shown             --level level28c --frames 240 --tap 247,141@60          (the recipe of the earlier clear-portals captures)
A4  the end decay           --level level0 --hold right@1-<door tick> --frames <door tick + 30>   (the implementer measures the door tick on the unmodified build and records it)
A5  a death dismissal       a level with a reachable hazard, walked into by --hold, frames after the death (the end-screen steps' recipe; recorded)
A6  the pause over a level  --level level0 --press pause@120 --frames 160
A7  1-21's hint             --level level20 --frames 90
A8  a weightless level      --level level1c --frames 420
A9  window shapes           --level level0 --frames 420 at --window 960x720 and at --window 1680x720
K7  main menu f300          (no --level) --frames 300
M1-M5 the screens           main menu f300 (= K7), Credits, Achievements, Worlds, Levels: each by --tap of the button's px read from ui.json main_menu (pinned by test_mp_frontdoor), then a settled frame
# --- MODERN (every log must contain "controls: modern", N1; every capture has validation ACTIVE and silent, review 1 finding 21)
K2  modern tutorial         --controls modern --level level0 --frames 120 and --frames 420; K2b/K2c: --controls modern,preset=balanced and ,preset=mr (the owner's look comparison, Q3)
K3  modern 1-21 hint        --controls modern --level level20 --frames 90 at 1280x720, and at --window 960x720 (4:3) and 900x720 (5:4)
K4  weightless              --controls modern --level level1c --frames 420 (corners, no pair)   and   --level level0c (the pair is there: the negative control)
K5  window shapes           K2 at --window 900x720 (5:4), 960x720 (4:3), 1680x720 (21:9), 2560x720 (32:9)   (the INFO line's view size is read first: a display may clamp the window)
K6  a pad held (a picture)  --controls modern --level level0 --tap 210,631@60-160 --frames 200   against   --level level0 --hold right@60-160 --controls modern   (the right arrow's centre is (210.4, 631.2); the equality of the two walks is the in-process pin of CTL-6, not this picture)
K8  options                 --controls modern --fixed-step --frames 420 --tap 1235,45@300
K9  toggle, kept            PLAYER-STYLE (no --controls, no --fixed-step) on a scratch --saves: --frames 600 --tap 1235,45@300 --tap <Modern tile px>@360 ; then a second launch, same --saves, --frames 30: INFO "controls: modern (saved)"
K10 adjust                  K8 plus --tap <Adjust px>@360 --frames 480;   K11 drag: plus --tap <lock px>@400 --drag <x1,y1>:<x2,y2>@420-450 --frames 520
K12 a moved layout, kept    PLAYER-STYLE on a scratch --saves: K11's gesture, then a second launch in a level (--level level0 --frames 60): the INFO line prints the moved rects
```

Each capture's `INFO` log prints the five control rects once per level start (CTL-6), which the verifier compares with the table of 1.2. `--tap` is in 1280x720 pixels whatever the window's size.

### 5.1 What each step launches (the expectation to give the owner)

| Step | `test_mp.exe` (new binary) | `MagicPortals.exe` (new binary) | hooked runner | Full sweeps (verifier) |
|---|---|---|---|---|
| CTL-1 | 0 | 0 | 0 | 0 |
| CTL-2 | 1 (goldens are recorded to data by the same binary) | 1 (the baselines come from it) | 1 | 0 |
| CTL-3 | 1 | 1 | 0 | 1 |
| CTL-4 | 1 (`--target test_mp`) | 0 | 0 | 0 |
| CTL-5 | 1 | 1 | 0 | 0 |
| CTL-6 | 1 | 1 | 1 | 1 + 1 modern |
| CTL-7, 8 | 1 each | 1 each | 0 | 0 |
| CTL-9 | 1 | 1 | 0 | 1 |
| CTL-10, 11 | 1 each | 1 each | 0 | 0 |
| CTL-12 | 1 | 1 | 1 | 0 |
| CTL-13 | 1 | 1 | 0 | 1 (+ one at the end) |
| CTL-14 | 1 | 1 | 1 | 0 |

About 13 `test_mp.exe`, 12 `MagicPortals.exe` and 4 hooked runners if nothing goes wrong, plus a relink for every verification round; at the last wave's rate expect **a handful to a dozen refusals**, each reported and relinked once.

**Shared seams** (added once, used by later steps): `MagicPortalsLayer::ControlAt(viewPoint)` and `PlaceCameraForTest(centrePx)` (CTL-2, public, documented test seams; the second is *not* read-only and has no production caller); `MagicPortalsLayer::ControlsStyle()` (CTL-5); `Paths::controls` (CTL-5: `{available, forced, saveAllowed, artDir, tuning, testInsets, classicClaims}`; all default to "classic, nothing"); `test_mp test_mp_routes --controls modern` (CTL-6); `MP_TEST_PORTDATA` (CTL-2: test code only, read by `TestPaths()`).

---

### CTL-1 (planning step: next free): the Magic Rampage art tool and its first run (Python and docs only)

**Size** M (about 450 lines Python, 250 tests, 100 doc). **Relinks** 0. **Depends** nothing; **may run now (P1)**, and first, because the only source of the art is another session's scratch. Skippable (Q2).
**Precondition (written into the step doc before the first copy):** the owner's request of 2026-10-15 for Magic Rampage's own control buttons is the go-ahead for copying its sprites into the gitignored `reference/magic-rampage/` for local use; `docs/permission.md` is still the stub, nothing is committed or shipped, and any release package containing that folder stays gated by Q2. If the owner has not given that go-ahead, the step stops here (CLAUDE.md: stop rather than assume scope).

* **Files**: `tools/art/make_mr_controls.py`, `tools/art/tests/test_make_mr_controls.py`, `docs/magic-rampage-controls.md`, planning Step, `DEVLOG.md`, README (directory table, one line on the tool and venv).
* **Data**: none in the repository. The ignored output: `reference/magic-rampage/{assets/sprites,controls}/`, `manifest.json` (`tool_version`, `package`, per output: source member, recipe, sha256, size, `body` rect (14, 14, 114, 114)).
* **Tests (pytest in `tools\parity\.venv`; install pytest by hand if absent and record it)**: `test_straight_rgba_from_palette_and_trns`, `test_straight_rgba_from_la`, `test_colour_under_alpha_zero_is_black`, `test_an_exact_magenta_pixel_is_refused`,
  `test_as_shipped_sprites_are_not_resampled`, `test_refuses_an_out_that_git_does_not_ignore` (a temp git repo with `reference/` ignored), `test_manifest_records_sha256_size_and_body_rect`, `test_check_reproduces_the_bytes`,
  `test_composing_a_glyph_on_the_frame_is_centred_on_the_body`, `test_the_tool_imports_nothing_from_penumbra`. **Fixtures are made inside the tests; no Magic Rampage file is ever opened by a test.** At least 10 tests.
* **Gates**: **G1** pytest >= 10 passed and `ruff check tools/art` clean (run with the converter venv's ruff). **G2** first run exits 0 and writes five 128 x 128 RGBA files, `walk_left`/`walk_right` equal to their source sprites pixel for pixel (max diff 0), 0 exact-magenta pixels, `manifest.json` present; `--check` exits 0. **G3** `git check-ignore -v reference/magic-rampage/controls/walk_left.png`
  names the `reference/` rule; `--out docs` exits non-zero and writes nothing. **G4** S6 and S8. S1 to S5, S7, S9 are **n/a** (no C++ touched; the build and sweeps are untouched by construction: `git diff --stat` shows only `tools/art`, `docs`).
* **Fails if / recorded as**: Penumbra's extract already purged (then the xapk must be re-supplied: the step stops and says so); Pillow/numpy missing from the parity venv; palette+tRNS colour under alpha 0 not black (a halo); composed glyph looks wrong (owner's eyes at CTL-8); the tool's default `--out` pointing at a tracked path (G3).

### CTL-2 (planning step: next): the classic pointer path, pinned (tests, two seams, baselines)

**Size** M (about 800 test lines, about 20 source lines). **Relinks** 1 + one hooked mutation runner. **Depends** nothing; **starts after wave 4d (P2)**. The point: **nothing exercises a held pad today** (only `Hud::OnPad` pure geometry), and the lift gate (`!onPad`, `:8655`) is a standing "left for later" (backlog 140). These pins go in **before** the refactor, so
CTL-3's evidence is "the same tests, green on the old code and on the new".

* **Files**: `game/MagicPortalsLayer.hpp/.cpp` (two public seams: the const `std::optional<Control> ControlAt(const glm::dvec2& viewPoint) const`, an 8-line wrapper of `controlUnderPointer`; and `void PlaceCameraForTest(const glm::dvec2& centrePx)`, 4 lines, for the lift and hand pins: `CameraCentrePx()` is a getter only today and the follow lags the player; nothing else), `tests/test_mp_layer.cpp`, `tests/test_mp_hud.cpp`, `tests/test_mp_camera.cpp`, `tests/CMakeLists.txt` (`MAGICPORTALS_GOLDEN_DIR` as a per-source definition on `test_mp_layer.cpp`, the pattern of `MAGICPORTALS_ROUTES_DIR`), `tests/golden/classic_claim.json` and `classic_hud.json` (hashes and counts, no pixels). `TestPaths()` (test code only) reads `MP_TEST_PORTDATA` and sets `paths.portData` to it when present.
* **Tests (names)**, `test_mp_layer`: `AHeldTouchOnTheLeftPadWalksThePlayerLeft`, `AHeldTouchOnTheRightPadWalksThePlayerRight`, `ThePadHitCircleIsStrictlyInside` (**hard-coded** 63.99 / 64.01 units from each corner along both axes and the diagonal, never read from `rules.pads.hitRadiusUnits`, and never exactly 64.0 on the diagonal: inside walks and fires nothing, outside fires on release),
  `ATouchSlidOntoAPadWalksAndOffItStops` (a `--drag` world to pad and back), `AHeldTouchOnAPadKeepsTheCameraFollowing` and `AHeldTouchInTheWorldStopsIt`, `APressOnAPadOverALiftSendsNothingAndAPressOutsideSendsIt` (a level whose lift can be reached: 2-02 `level1a`/2-24 `level23a`, the camera placed by `PlaceCameraForTest`; if no level qualifies the pin is pure-geometry only and the hooked mutation on the three consumers is the evidence, recorded), `AReleaseOnAPadTakesNoTutorialHand` (a level that places `hand_tap`: 1-02 `level1`, 1-03 `level2`, 1-16 `level15`, 1-27 `level26`; the hand's rectangle tapped from a point on a pad and from a point outside it),
  `TheClaimOrderIsLeftRightResetMenuClear` (a point exactly on the edge shared by restart and pause: restart; the closed rectangles), `RestartPauseAndClearActOnThePressAndSpendTheTouch` (reload, pause opens, portals cleared with a portal placed; no shot afterwards), **`ARestingTouchInAWeightlessCornerStillCountsAsOnAPad`**
  (the ungated quirk, on **`level1c`** = 4-02, the first weightless level, with **`level0c` (4-01, which has gravity and shows its pair) as the negative control**: both sides pinned, so a refactor that gates `onPad` by `shown` fails),
  `NothingClaimsUnderAPauseAPopupOrAfterTheDoorOrADeath`, `TheClassicClaimMapIsPinned` (every 4 units of the view at 16:9 and 4:3, ages 0, 100, 350, 700, 5000 ms, clear hidden and shown: FNV-1a of the claim per point against `classic_claim.json`),
  **`TheClassicHudQuadsOfEveryLevelArePinned`** (S3a: all 128 levels at tick 420, plus the dynamic snapshots 1-02 at tick 20, 1-01 at tick 120 (ring and pulse), clear shown (A3), the end decay (A4), a death dismissal (A5), the pause over a level (A6), 1-21's hint (A7), level1c (A8), against `classic_hud.json`; skipped with 77 and a message without the original art, as the other HUD cases are);
  `test_mp_hud`: `ThePadRectAndHitCircleAtEverySlideAge`; `test_mp_camera`: the `onPad` flag's pure cases are already there (`:93, 223, 283-301`), none added. A `--record-golden <file>` argument of `test_mp test_mp_layer` writes the two golden files on the **unmodified** source, so a first run and a correction need no relink.
* **Byte-identical**: everything (S3, S4); `Hud.*`, `ui.json` untouched (S5).
* **Gates**: **G1** new checks pass on the **unmodified** claim code: `test_mp_layer` 4,181 + >= 150, `test_mp_hud` + >= 40, `test_mp_camera` unchanged. **G2** each pin **bites**: (a) **data mutation without a relink and without touching the repository's data**: the harness copies `game/data` to `out/parity/program/CTL/mut/<name>/`, changes one number there (hit radius 64 to 32; pad size 64 to 48), and runs the *same unchanged binary* with `MP_TEST_PORTDATA` pointing at the copy: the named pins fail, and `game/data/ui.json`'s md5 is unchanged before and after (the other session reads that file);
  (b) one hooked runner with three environment-switched code rules (camera `onPad`, the lift gate, the hand gate) removed by a test-only seam that is **deleted before the commit**: the three named pins fail. **G3** `git diff --stat` for `game/` lists only the two accessors (<= 20 added lines). **G4** the baselines **K1, A1-A9, K7, M1-M5** are captured twice (md5-equal: determinism) from this step's own exe, kept in `out/parity/program/CTL/baseline/`, and listed in `out/parity/BASELINE.txt`. S1 to S10.
* **Fails if / recorded as**: a pin finds an existing defect (record it as a backlog row, do **not** fix it here); no reachable lift level (pure-geometry pin, said so); the claim-map or HUD digests not reproducible on a second launch (a nondeterminism: stop).

### CTL-3 (planning step: next): one home for control geometry, classic as a forward (refactor, byte-identical)

**Size** S to M. **Relinks** 1. **Depends** CTL-2.

* **Files**: new `game/sim/Controls.hpp/.cpp` (`Id`, `HitShape {Rect|Disc, half-open flag, Contains}`, `Frame`, and the **classic** functions only: each calls the existing `Hud::PadRect/PadCorner/PadSlideUnits/OnPad/PadOpacity/PadAlphaByte/Place` and returns their result), `game/sim/CMakeLists.txt`,
  `game/MagicPortalsLayer.hpp/.cpp` (`ControlButton` gains `hit`; `layOutControls`, `controlUnderPointer`, one `onWalkControl(viewPoint)` replacing the four `Hud::OnPad` pairs (camera `:8622`, lift gate `:8655`, hand gate `:9988`), the ring centre `:8389`, `endHud`'s byte, and the `corner()` lambda's pointer-identity test `&placement != &m_hudRules.clearPortals` replaced by the `Control` kind), `tests/test_mp_controls.cpp` (new, with the classic cases only here), `tests/CMakeLists.txt`.
* **Tests**: `test_mp_controls` (pure, never skips): `ClassicHitEqualsTheHudFunctionsEverywhere` (a 0.25-unit grid over three aspects x eight slide ages: the decision of `HitShape::Contains` equals `Hud::OnPad` / `Rect::Contains`), `ClassicFramesAreTheHudsRectsExactly` (doubles equal), `ClassicWalkClaimEqualsTheTwoOnPadCalls`,
  `TheEndByteIsThePulsesByte`. `test_mp_layer`: **CTL-2's pins unchanged**, the claim-map and HUD digests **equal**.
* **Gates**: **G1** every CTL-2 pin, the claim-map hash and the HUD digest green, constants unedited. **G2** `grep -nE "Hud::(OnPad|PadRect|PadCorner|PadSlideUnits|PadAlphaByte|PadOpacity)" game/MagicPortalsLayer.cpp` prints **nothing** (before: `OnPad` x6, `PadRect` x2, `PadCorner` x1, plus `PadAlphaByte/PadOpacity` at the layout and `endHud`); the same calls now sit only in `sim/Controls.cpp`. **G3 the dynamic half**, named (review 1 finding 2): the existing layer cases that pin it, with their **check counts recorded before and after and equal**: `TheHudIsTheOriginals` (`test_mp_layer.cpp:6231`), the end-HUD cases (`:12875-13035`), `AClearPortalsButtonComesWithAPortal` (`:9337`), `TheTutorialRingsAndAWeightlessLevelHasNoPads` (`:9473`); `test_mp_hud`, `test_mp_levelend`, `test_mp_camera` check counts **unchanged**.
  **G4** S3 (a) green, (b) the A-side, menu and K sets **md5-equal** to CTL-2's, (c) the sweep 128 of 128 (verifier); S4; **S5 (strict: `sim/Hud.*` is not touched)**. `test_mp_controls` >= 120 checks. S1, S2, S6 to S10.
* **Fails if / recorded as**: a re-expressed classic expression flips one claim-map cell by an ulp (the classic path is **forwarded, not copied**); `onPad` gated by `shown` by accident (CTL-2's quirk pin fails on `level1c`); the ring centre or `endHud` read from a modern branch that does not exist yet (there is none: `Style` is introduced in CTL-4).

### CTL-4 (planning step: next): the modern layout as data and arithmetic, the pad art, and the census (pure)

**Size** L. **Relinks** 1 (`test_mp` only: `--target test_mp`; the layer does not use the module yet and `MagicPortals.exe` is not relinked). **Depends** CTL-3.

* **Files**: `game/data/controls.json` (new: `_about`; `reference.height_px` 720; `layout {version 1, margin_px {side 44, top 44, bottom 44}, gap_px 32, box_px 128, corner_box_factor 0.9, corner_min_size 1.0, presets {uniform {size 0.7, margin_bottom_px 44}, balanced {size 0.8, margin_bottom_px 34}, mr {size 1.0, margin_bottom_px 11}}, default_preset "uniform"}`;
  `hit {grow_px {walk_left 0, walk_right 0, restart 0, pause 0, clear_portals 0} (PORT, `_guess`, Q0/Q6), seam_px 32, edge_reach_px 48}`; `corner {act_on "release"}`; `clearance_px 16`; `size {min 0.4, max 1.4, step 0.1}`; `opacity {min 0.2, max 1.8, step 0.2, base_alpha_byte 120, idle_scale 1.0}`;
  (no `slide_in`, pulse or decay keys: those come from `Hud::Rules`, 1.5); `pressed {boost_byte 38}` (`_guess`); `ring {size_over_box 3.125}` (`_guess`, derived); `hint {margin_units 8}`; `art {<id>: {file, body_inset 0.109375}}` per tier (`mr`, `original`, `generated`); `census {em_units 16, slack_units 0}`; `default_mode "classic"`),
  **every number with a `_source` array: DECODED (spec 42 section and line), MEASURED, OWNER 2026-10-15, PORT, or `"_guess": true`**; `game/sim/Controls.hpp/.cpp` (`Style`, `Rules` and its **strict** `LoadRules`, `Insets`, `Tuning` and its grids/clamps/`ReadFrom`/`ToJson`/`ParseFlag`, `LayOut(style, rules, tuning, view, insets, ageMs)`, `HitRects`, `DrawRect`, `PlacementFor`, `MoveFor`, `HintLeft`, `RingFor`, `ResolveStyle`, `ResolvePolicy`);
  `game/sim/PadArt.hpp/.cpp` (pure, deterministic glyph bitmaps: two arrows (mirror images), restart, pause, clear, gear; moved here from the layer step so nothing new needs bisecting there: review 1 finding 17); `tests/test_mp_controls.cpp`, `tests/test_mp_padart.cpp`, `tests/test_mp_census.cpp` (**SKIPS_WITH_77**: it reads `tests/routes/*.json` and `out/levels`), `tests/golden/controls_census.json` (the accepted list, 1.9), `tests/CMakeLists.txt` (a per-source definition for the golden directory, and the suite names).
* **Tests** (`test_mp_controls`, >= 700 checks, reading only the port's own committed data, never skipping): `TheModernRulesAreTheFilesNumbers`, `TheDefaultPlacesArePinnedBesideTheLayoutNumber` (the five rects, hits and 1.2's px at 16:9, **and `kLayoutVersion == 1`**), `ThePresetsKeepThePairTopAtOrBelowTheCharacterRow` (206: 208.50, 207.50, 206.58), `TheSizeStepsEndExactlyOnTheLimits` (chains up and down; a step at a limit returns the same value),
  `MarginsAndGapsDoNotGrowWithSize`, `ACornerButtonNeverShrinksBelowItsMinimumSize` (40.96 at 0.4, 0.7, 1.0; 49.15 at 1.2), `NoTwoDefaultBoxesOverlapAtAnyAspectOrSize` (5:4, 4:3, 3:2, 16:10, 16:9, 20:9, 21:9, 32:9 and 2:1 x 11 sizes), `TheSeamIsSplitAtItsMiddleAndNothingOverlaps` (half-open: a point on the shared edge is the later control's),
  `TheEdgeReachGoesToTheViewEdgeOnlyWithinTheReach`, `ThereIsNoGrowthTowardTheWorldByDefault`, `AHitRectNeverLeavesTheView`, **`TheModernHudIsNoMoreThanOneAndAHalfTimesTheClassicsAtDefaultSize`** (total at 16:9, 4:3, 5:4: 1.44 at the default; the pair alone at most 1.0 (0.67); it **fails loudly**, not as a note, where a data change exceeds the budget),
  `TheSlideMovesEachArrowFromItsNearerEdgeAndTheHitFollows` (ages 0, 350, 700, 701), `TheCornerPlacementsGiveTheOriginalsEntranceAtTheInsetCorners` (`HudEntered/HudDismissed` on `PlacementFor`: the start along the ray from the view's centre, 32 units, alpha linear), `DefaultTuningIsTheBaseBitForBit` (`memcmp`),
  `AMoveIsInUnitsFromTheAnchorAndNotScaledBySize`, `MoveForIsTheInverseOfPlace` (10,000 round trips), `ClampKeepsEveryBoxWhollyInsideTheSafeRectangle`, `SafeInsetsMoveTheDefaultsInward`, `TheDrawRectPutsEveryTiersBodyOnTheSameFace`,
  `TheOpacityIsAMultiplierOnTheByteAndTheDefaultIsUnchanged` (t = 1.0 and `idle_scale` 1.0 equals `Hud::PadAlphaByte` for every age; t in {0.2..1.8} x 120 whole bytes; capped at 255), `TheRingFollowsTheArrowsCentre`, **`HintLeftClearsThePairAndLeavesClassicAlone`** (90.74 and 154.45; the clamp at 4:3 and 5:4 with the font's measured extent 258.4 as a fixture: 74.93 and 53.60; classic 50), `TheHintsArrowEndsAboveThePairsHitRect` (the arrow's geometry from `camera.json`, 197 against 208.50),
  `ResolveStyleTruthTable` and `ResolvePolicyTruthTable` (2.1's table, every row), `WhatIsRefused` (each key of `controls.json` removed, one at a time, in a scratch copy: refused; a typo in `_source` is not), `TheDefaultsOfAnAbsentTuningFileAreTheBase`.
  `test_mp_census` (>= 40): `TheCensusAtTheDefaultIsTheAcceptedList` (eight window shapes: the pair blocks only 1-20 and 3-27, restart 2-06 and 4-23, clear 1-09, 3-14 and 2-24, pause nothing: **a level joining the list fails**, one leaving it fails too, so the golden is kept honest), `TheFirstPassPlacementWouldHaveFailed` (a **negative control**: size 1.0, margin 44, 32 px growth and a snap give 25 levels, not the accepted list), `AGrowthOfSixteenPxCostsWhatQ0Says` (8 / 14 / 16 for the three presets: pinned as the number the owner decides on), `ThePairIsBelowTheCharacterRowOrTheCensusFails`.
  `test_mp_padart` (>= 60): `AGlyphIsDeterministic`, `TheTwoArrowsAreMirrorImages`, `ABodyKeepsTheStatedMarginAndNoOtherAlpha`, `NoPixelIsExactMagenta`, `EveryGlyphIsAsLargeAsAsked`.
* **Byte-identical**: all of S3/S4 (the layer does not read the module; `MagicPortals.exe` is not relinked); `ui.json` untouched (S5, `controls.json` the only new data file).
* **Gates**: **G1** `test_mp_controls` >= 700, `test_mp_census` >= 40 (or 77 and named, without `out/levels`), `test_mp_padart` >= 60 checks, 0 failures; the pinned table equals 1.2 and Appendix A to 1e-9. **G2** `controls.json` md5 recorded; every numeric leaf has a sibling `_source` or `_guess` (a test walks the tree). **G3** a **mutation by data** through `MP_TEST_PORTDATA` (CTL-2's override): `margin_px.side` 44 to 40, `hit.edge_reach_px` 48 to 0, `hit.grow_px.walk_left` 0 to 16 in a scratch copy: the named pins and the census fail; the repository file is untouched (md5). **G4** S3(a) digest equal (nothing classic changed). S1 (`--target test_mp`), S2, S5 to S10.
* **Fails if / recorded as**: float drift in the size chain (the integer-step model is the guard); half-open vs closed at the shared edge; a hit rect partition that depends on iteration order; an aspect where two boxes touch (a test row); `controls.json` keys colliding with a later step's; the census list disagreeing with the reviewer's script (it must reproduce `CTL/arith2/final_layout.py`).

### CTL-5 (planning step: next): the policy and the flag, classic only (plumbing)

**Size** S. **Relinks** 1 (+1 `MagicPortals.exe`). **Depends** CTL-4. Split out of the layer step (review 1 finding 17) so that "the layer plays modern" has nothing else to bisect against.

* **Files**: `game/MagicPortalsLayer.hpp/.cpp` (`Paths::controls {available, forced, saveAllowed, artDir, tuning, testInsets, classicClaims}`, all defaulting to classic; `ControlsStyle() const`; `Controls::LoadRules` in `OnAttach` **after and outside** the `&&` chain of `ui.json` loaders (`:319-348`) and only when `available`; the INFO line `controls: <style> (<why>)`; **until CTL-6 the layer plays Classic whatever resolves** and says so: `controls: modern requested, drawn classic until the layer plays it`), `game/main.cpp` (`--controls`, the call of `Controls::ResolvePolicy`, help text), `tests/test_mp_layer.cpp`, README (flags row).
* **Tests**: `ABareLayerIsClassicAndHasNoStore`, `ControlsStyleReportsTheResolvedStyle`, `AForcedModernResolvesModernAndThePolicyIsRecorded`, `ABrokenControlsDataLeavesClassicAlone` (a scratch copy of the data directory through `MP_TEST_PORTDATA`: one error line, style Classic, nothing fatal), `APresetKeyIsParsedAndAnUnknownOneIsRefused` (in `test_mp_controls`).
* **Gates**: **G1** the pins. **G2** main's own call, by launch: `MagicPortals.exe --controls bogus` exits 1 with the usage, `--help` lists the flag, `--controls modern --level level0 --fixed-step --frames 30` logs the line above. **G3** S3(a) and (b) equal (classic only), no sweep; S4. S1, S2, S5 to S10.
* **Fails if / recorded as**: `controls.json` (data) fatal in the attach chain; a store opened in a dev run; a classic launch whose log or output changes by a byte (S3b).

### CTL-6 (planning step: next): the layer plays under the modern layout (`--controls modern`), **the riskiest step: most pins**

**Size** L (the largest). **Relinks** 1 + one hooked mutation runner. **Depends** CTL-5.

* **Files**: `game/MagicPortalsLayer.hpp/.cpp` (`m_modernImages`; `buildControls`/`layOutControls`/`EmitHud` modern branches **beside** the untouched classic code: `if (style == Classic) { the existing text }`; claims through `ControlButton::hit`; `onWalkControl` **ungated by `shown`** as classic's (D12); the corner buttons' **act-on-release** in `readInput` (a record of the control a press began on, cleared when the touch is up: modern only); ring and `endHud` through the module; modern art tiers 3 and 4 only here), `game/main.cpp` (the layer now plays what resolves), `tests/test_mp_layer.cpp`, `tests/test_mp_routes.cpp` (`--controls <style>`, the census print), `tests/CMakeLists.txt`, README (Input table).
* **What it does**: five controls always built in modern; rects, hits, alphas (pulse, opacity 1.0, decay), slide, ring, no-gravity and time-stopped hiding, the corner entrances and dismissals via `PlacementFor`; the INFO line `controls: modern, walk_left (x,y)-(x,y) px ...` once per level start; drawn from `m_modernImages` (generated tier or flat quads when `prisms` is empty). **Not yet**: pressed look, swallow, hint clearance, safe-area wiring, MR art (CTL-7, CTL-8).
* **Tests**, `test_mp_layer` (the modern twins of CTL-2's pins, **same bodies re-aimed at the modern hit rects**; **each reads `ControlsStyle()` and fails unless Modern**, N1): `AHeldTouchOnTheModernLeftArrowWalksLeft` (inside every corner of its hit rect minus 0.01, and the exact centre), `...Right...`, `ATouchJustOutsideAModernHitRectFiresAPortal` (plus 0.01), `TheSharedSeamBetweenTheArrowsIsTheRightArrows`,
  `TheCornerEdgeOfTheViewIsAControlPressInModern` (the edge reach), `ATouchSlidOntoAModernArrowWalksAndOffItStops`, `TheModernPadsHitRectFollowsItsSlide`, `AHeldTouchOnAModernArrowKeepsTheCameraFollowing`, `APressOnAModernArrowOverALiftSendsNothing`, `AReleaseOnAModernArrowTakesNoTutorialHand`,
  **`ModernRestartPauseAndClearActOnTheReleaseInsideTheirHitRect`** (a press and release inside: acts once; a press inside that slides off and releases outside: nothing acts and **no shot fires** (the press spent the touch); a press outside that slides onto a button: a world touch, nothing acts), `AModernCornerPressInTheEdgeStripActs` (a press 5 units above the drawn box, in the strip), `ModernClearHidesAsItClearsAndItsTouchFiresNothing`,
  `TheModernClaimMapIsPinned` (hash against `tests/golden/classic_claim.json`'s sibling `modern_claim.json`, as CTL-2's),
  **`TheModernPairIsHiddenAndUnclaimedOnAWeightlessLevelAndTheCornerIsWorld`** (on **`level1c`**, with `level0c` showing its pair), **`TheModernOnWalkControlIsUngatedLikeClassics`** (a touch resting in the bottom-left corner of `level1c` stops the camera's follow in both styles: both sides pinned, D12), `NothingModernClaimsUnderAPauseAPopupOrAfterTheEnd`, `TheModernPadsAreDrawnBetweenTheTwoBlacks` (quad order), `TheModernEndByteDecaysFromTheSameByte`, `AModernControlIsNeverDroppedForAMissingPicture`, `ModernRunsWithNoOriginalArtDrawFlatQuads`,
  `AClassicLayerIgnoresEverythingModern` (every CTL-2 pin re-run with `Paths::controls` set but `forced = Classic`), **`AHeldTouchOnTheModernRightArrowWalksLikeAHeldKey`** (two layers: one with a scheduled tap on the arrow's centre for ticks 60 to 160, one with `ScheduleDevHold(60, 160, +1)`; the player's position and `CameraCentrePx()` compared on **every** tick for exact equality: the in-process replacement of the first pass's `--trace` gate).
* **Byte-identical**: **S3 (a), (b) and (c) flagless**, equal; the six-level `--controls classic` spot equal; S4; S5; `test_mp_hud`, `test_mp_levelend`, `test_mp_camera`, `test_mp_frontdoor`, `test_mp_popup` counts unchanged.
* **Captures**: K2 (three presets), K3, K4, K5, K6; the PNGs are read by eye and the INFO lines against 1.2's px table; all with `controls: modern` in the log (N1).
* **Gates**: **G1** all new pins green; `test_mp_layer` >= 4,181 + 150 (CTL-2) + 240, `test_mp_controls` >= 700, `test_mp_padart` >= 60. **G2 World-pixels gate**: a modern f420 sweep (`--controls modern`) equals the classic sweep **outside the union of both layouts' drawn boxes, their rings and 1-21's hint row, padded 2 px, on 128 of 128 levels** (the world, hence the simulation at f420, is unchanged) **and differs inside it on 128 of 128** (N1: restart and pause move on every level); the differences inside are listed per level; modern captures carry the validation clause.
  **G3 Route census** (`test_mp test_mp_routes --controls modern`), two variants: **(a)** every route either wins on exactly its classic tick with an identical per-tick `--trace 1` record **up to and including the tick of its first swallowed tap**, or is identical to its end if it has none; a swallowed tap is one `ControlAt` assigns to a modern control (classified per tap: which control, the tick, the camera's clamp state) and the table **per route and per control** goes into the step, with the forced list of 1.9 as its lower bound (taps the census proves unavoidable must appear); `style=modern` is printed per route and a route that ran classic **fails**;
  **(b)** modern layout and draw with **classic claims** (`Paths::controls.classicClaims`, a test flag) must be **identical on all 128 routes with no exemption**: it separates "layout, draw and policy moved a simulation number" from "a claim took a tap". Both variants report a non-empty swallowed set overall (N1).
  **G4** a **mutation**: one hooked runner switches off the modern hit (every control claims by the classic code), the camera, lift and hand gates, the ring centre, and the act-on-release: the named modern pins fail; removed before the commit. **G5** a full `port_build.bat` once: 0 warnings. S1 to S10.
* **Fails if / recorded as**: one of the six consumers (draw, claim, camera `onPad`, lift gate, hand gate, ring) still reading the classic circle (a pad that draws in one place and presses in another, a camera that drags under a pad); the drawn rect and the hit rect disagreeing while sliding; `buildControls` dropping arrows when a picture is unreadable; calling `unloadControls` on any modern path; a hit rect covering a help block (listed by the census, not a failure); the generated PNG not written when `prisms` is empty (flat quads: tested); a modern HUD on a clean clone with no original art changing today's "no art, no HUD" (documented exception: modern always draws its controls); an unexpected `Hud::` classic edit (S5); a modern gate that passes with `ControlsStyle()` classic (N1).

### CTL-7 (planning step: next): the modern controls' own input behaviour

**Size** M. **Relinks** 1. **Depends** CTL-6.

* **Files**: layer (pressed flag from the previous tick's claim, the boost in `EmitHud`; `m_swallowUntilUp`; `HintLeft` at the hint draw with the font's measured extent; `SafeArea::Get()` to `Insets` modern-only; the slide direction by half), `tests/test_mp_layer.cpp`, README.
* **Tests**: `APressedModernControlDrawsTheBoost` (classic never does), `TheSwallowHoldsAClaimUntilTheTouchIsUp` (a tuned layout via `Paths::controls.tuning` puts the pair under the pause's Resume press: after resume the held mouse walks nothing; a fresh press walks), `TheSwallowIsNotSetInClassic`, `ThePopupCloseSwallowsToo`, `TheHintClearsTheModernPair` (1-21: left 90.74 at 16:9, classic 50; the text row unmoved), `TheHintStaysOnTheScreenAtEverySupportedAspectAndSize` (the font's extent through the layer, 5:4 to 32:9 x sizes 0.4, 0.7, 1.0, 1.4), `TheModernCornerButtonsDrawOverThePlaqueForItsLifeOnly`, `SafeInsetsReachTheLayoutOnlyInModern` (injected), `AControlOnTheRightHalfSlidesInFromTheRight`.
* **Gates**: **G1** the named pins; `test_mp_layer` + >= 60. **G2** S3 (a), (b) equal; the six-level `--controls classic` spot; S4. **G3** CTL-6's census **repeated, unchanged** (the swallow does not alter a route: no route resumes a pause under a held pointer). **G4** K3 at 16:9, 4:3 and 5:4: the 1-21 hint's first glyph is right of the pair's right edge at 16:9 (90.74 units = 255 px at 720p) and the whole text is on the screen at 4:3 and 5:4 (read by eye and by the INFO line). S1, S2, S5 to S10.
* **Fails if / recorded as**: the swallow set in classic (CTL-2's pins catch it); a swallow that never clears (a held button after a window focus loss: cleared when the pointer is not over the game); the hint moving in classic.

### CTL-8 (planning step: next): Magic Rampage's art read at run time (loader and the fallback order)

**Size** M. **Relinks** 1 (`game/CMakeLists.txt` touches only `main.cpp`'s compile; the layer is recompiled; `test_mp` relinks for the layer change only). **Depends** CTL-6 (CTL-1 for real files; the step is fully testable without them).
**Precondition:** the same as CTL-1's, recorded again in this step's doc: local, gitignored use on the owner's 2026-10-15 request; no Magic Rampage file is read by any test, committed or shipped; shipping stays gated by Q2. Without it the step stops, and modern keeps running on the fallback tiers.

* **Files**: `game/CMakeLists.txt` (per-source definition `MAGICPORTALS_PADART_DIR` for `main.cpp`; cache variable `SUPERSONIC_MAGICPORTALS_PADART`), `game/main.cpp` (`paths.controls.artDir`, `--padart <abs dir>`, help), layer (`padImage(id)` through the four tiers into `m_modernImages`, the INFO line `modern controls art: walk_left=mr walk_right=mr restart=mr pause=mr clear_portals=mr`, a WARN when the tiers are mixed; **a file whose PNG size is not the manifest's (or 128 x 128) is skipped to the next tier with a WARN**: a hand-dropped Penumbra canvas of 406 x 406 would otherwise draw mis-scaled, review 2 F12), `tests/test_mp_layer.cpp`, README (cache-variable row, flag row), `docs/magic-rampage-controls.md`.
* **Tests** (synthetic PNGs the test writes into a scratch directory with the engine's stb writer; never a Magic Rampage file): `TheMagicRampageFileWinsWhenItIsReadable`, `AnUnreadableFileFallsToTheOriginalsSpriteForTheThreeCorners`, `AFileOfTheWrongSizeFallsToTheNextTierWithAWarning`, `TheArrowsFallToTheDrawnGlyphWithNoFile`, `TheLastTierIsAFlatQuadWithNoPrisms`, `TheTiersAreResolvedOncePerRunAndNeverClearedByALevelLoad`, `TheDrawRectUsesTheTiersMargin` (a margin-less sprite is drawn 22 percent smaller than its box).
* **Gates**: **G1** the pins. **G2** with `reference/magic-rampage/controls/` present: the INFO line reads five `mr`; with `--padart <an empty dir>`: `walk_*=generated`, three corners `original`; with `--padart` and no original art: `generated` x 2 and flat quads x 3; K2, K5 retaken each way (read by eye; the owner's call on the look). **G3** S6 above all: `git status` and the extension grep clean with `reference/magic-rampage` full of PNGs. **G4** S3 (a), (b) and S4 (classic never touches the tiers), `test_mp` does not relink for the definition (after the CMake and `main.cpp`-only edit, made first, `cmake --build build --target test_mp -- -n` reports no work to do). S1, S2, S5, S7 to S10.
* **Fails if / recorded as**: a PUBLIC definition on `MagicPortalsSim` (would rebuild every suite: rejected here); the cache name with an underscore or digit after `MAGICPORTALS_` (`play_build.bat` would not copy it); any path into Penumbra's tree; art resolved per level (it would be cleared by `unloadControls`).

### CTL-9 (planning step: next): the setting and the Options screen

**Size** L. **Relinks** 1. **Depends** CTL-6 (CTL-8 only for looks). It edits the shared menu dispatch, so it is a code-touching step for classic (S3 b and c).

* **Files**: `game/sim/Controls.{hpp,cpp}` (`Store`), `game/sim/Options.{hpp,cpp}` (pure `Rules` (strict, from `controls.json`'s `options` block), `Pieces`, `ButtonAt`, `EntryPiece`, thumbnail geometry), `game/data/controls.json` (`options`, `default_mode` stays `"classic"`), layer (`Screen::Options` through the Credits wiring sites `:2901, 3012, 3158, 3348, 3779, 3837, 3929`; `PendingMenu`; the main-menu entry; the store opened in `OnAttach` next to the switches, memory-only when the policy forbids saving; `saveControls()`), `game/main.cpp` (the final `--controls` semantics, `--saves` shared), `tests/test_mp_options.cpp` (new), `tests/test_mp_layer.cpp`, `tests/test_mp_controls.cpp`, README (Input table, flags, the Options screen), `docs/original-gameplay.md` (a note under the pads: "modern controls are an enhancement; classic is the original").
* **Tests**: `test_mp_options` (>= 150): `TheEntryIsTheFacebookSlot`, `TheTilesAndTheBackButtonLayOutAtEveryAspect`, `NoOptionsWidgetOverlapsAnotherAtAnySupportedAspect`, `ATileIsPressedByADownAndUpInsideOnly`, `TheChosenTileIsLit`, `TheThumbnailsAreTheTwoLayoutsScaled`. `test_mp_controls`: `ControlsStoreRoundTrips`, `TheFileHoldsDeltasOnly` (a default size and opacity write no key), `AMissingFileIsTheShippedDefault`, `AnUnreadableFileIsDefaultedAndOverwrittenAtTheNextSave`, `AMemoryOnlyStoreWritesNothing`, `ALayoutNumberFromAnotherVersionDropsMovesAndKeepsSizeAndOpacity`, `TheSwitchesStoreIsNotTouched` (audio.json bytes equal before and after a toggle). `test_mp_layer`: `TheOptionsEntryOpensOptionsAndItsPressIsSpent`, `TappingAModernTileSavesAndTheNextLayerStartsModern`, `TheFlagBeatsTheSavedValueAndWritesNothing`, `UnderAForcedStyleATileChangesTheRunNotTheFile`, `AFixedStepRunIsClassicWhateverIsSaved`, `ABareLayerHasNoEntryAndNoStore`, `BackLeavesOptionsAndTheMainMenuTakesNothingFromThePress`, `TheStyleChosenInOptionsIsPlayedAtTheNextLevel` (a toggle never calls `unloadControls`), `TheNumberOfTapsFromALevelToOptionsIsRecorded` (the pause's Levels, then the screens; written into the step: review 2 F14).
* **Byte-identical**: S3 (a), (b) and (c): **the main menu `k7` and the M1-M5 screens equal CTL-2's baselines byte for byte** (the entry is absent without the flag), and equal under `--controls classic`; `test_mp_frontdoor`, `test_mp_levelend`, `test_mp_info` counts unchanged.
* **Captures**: K7 (with the entry), K8 (under the flag), **K9 as a player-style run** (2.1: no `--controls`, no `--fixed-step`, a scratch `--saves`; a toggle, then a second launch reading it back: INFO `controls: modern (saved)`).
* **Gates**: **G1** the pins; **G2** `ResolveStyle`/`ResolvePolicy` truth tables (CTL-4's) now exercised **through `main`'s own call** (CTL-5's launches repeated: `--controls bogus` exits 1, `--help`). **G3** the saved file reads back `{"version":1,"controls":{"mode":"modern","layout":1}}` byte-for-byte in a unit-test scratch directory (the byte-for-byte check stays a **unit test**; K9 is the real-path capture); `audio.json` untouched. **G4** **S7 strict**: no `controls.json` created by any sweep or capture that did not toggle, and only K9's scratch directory holds one. **G5** the corrupt-file cases. S1, S2, S5 to S10.
* **Fails if / recorded as**: a third key written into `audio.json` (rejected); the entry added inside `MainMenu::Pieces` (breaks `test_mp_frontdoor`'s pins); `controls.json` (data) fatal in the attach chain; a toggle during a level (not reachable); text with no original font (no labels; accepted, same as every menu); any of M1-M5 differing by a byte.

### CTL-10 (planning step: next): the tuning is kept and applied

**Size** M. **Relinks** 1. **Depends** CTL-9. No new screen: size, opacity and moves are reachable by the dev flag and the file.

* **Files**: `sim/Controls.*` (`Store` keeps `size`, `opacity`, `move`; `Tuning` already exists), layer (the store's tuning into `Paths::controls.tuning` and `layOutControls`; `m_modernImages` unchanged; opacity at draw), `main.cpp` (`--controls modern,size=1.2,opacity=0.6,walk_left=-10:5,preset=mr`), tests, README.
* **Tests**: `TheTuningRoundTripsThroughTheFile`, `TheDefaultTuningWritesNoMoveAndNoSurprise`, `AFlagTuningReplacesNeverMergesAndWritesNothing`, `ASizeOfOnePointFourDrawsTheClassicSizeOfArrow` (63.72), `OpacityMultipliesTheByteAndCapsAtOne` (t = 1.8: the pulse top 160 x 1.8 caps at 255), `AMovedControlIsClaimedWhereItIsDrawn` (pair moved 60 units right: hit follows; the old place is world), `ASavedMoveThatNoLongerFitsIsPulledInAtLayoutAndLeftAsStored`, `AFileWrittenUnderAnotherLayoutNumberKeepsSizeAndOpacity`, `ADefaultThatChangesReachesAPlayerWhoNeverMovedASlider`.
* **Gates**: **G1** the pins (>= 120 new checks). **G2** K2 with `--controls modern,size=1.4` shows the arrows at the classic 64-unit size (box 179 px: measured from the INFO line), `opacity=0.2` and `1.8` shown. **G3** S3 (a), (b), S4, the CTL-6 census with default tuning unchanged. S1, S2, S5 to S10.
* **Fails if / recorded as**: a size that multiplies the offsets (Penumbra L2: not here, constants M and G); moves scaled by size; the tuning applied in classic (CTL-2's pins).

### CTL-11 (planning step: next): the adjustment screen, part 1: the screen, the live preview, size and opacity

**Size** L. **Relinks** 1. **Depends** CTL-10, CTL-8 for real glyphs.

* **Files**: `sim/Adjust.{hpp,cpp}` (pure `Rules` from `controls.json`'s `adjust` block (3.2's numbers, each with `_source`), `Pieces`, `WidgetAt`, tile arithmetic with the readouts, **the hit-rectangle frames and the character-row guide**), `sim/PadArt` (the seven drawn editor glyphs), layer (`Screen::Adjust`, the Options button, `EmitMenu` draws the preview through `LayOut`, readouts through `LayOutText` at 20 units; the editor glyph tiers with the same size check as CTL-8: Magic Rampage's `editor/` file, then the drawn glyph, then a flat quad), `tools/art/make_mr_controls.py --stage editor` (+ tests, + the doc), tests, README, `docs/original-gameplay.md`.
* **Tests**: `test_mp_adjust` (>= 200): `TheWidgetColumnIsMagicRampagesInUnits`, `NoDefaultControlIsWhollyUnderAWidgetAtAnyAspectOrSize`, `TheNarrowAspectsMeetAsRecorded` (restart meets the size/opacity rows at 5:4 at every size, from size 1.1 at 4:3, 1.4 at 3:2, never at 16:10 and wider), `AtSixteenNineTheLeastClearanceIsTwentyTwoPointEightSixAtSizeOnePointFour`, `RestoreClearsThePairByFivePointEightThreeAtSizeOnePointFour` (pair top 176.64), `ATileActsOnTheReleaseStrictlyInside`, `AtALimitATileIsInert`, `TheSizeChainEndsOnTheLimits`, `AnOpacityStepDoesNotRelock`, `ASizeStepRelocks`, `WhileUnlockedTheEditorDrawsEveryHitRectAndTheCharacterRowGuide`, `TheEditorGlyphsFallBackTierByTier`.
  `test_mp_layer`: `AdjustOpensFromOptionsAndTheEntryPressIsDead`, `ASizeTapMovesTheLiveButtonsAndIsKept` (once per gesture, not per frame), `TheBackButtonAndTheBackKeyLeaveAndSaveWhatIsUnsaved`, `BackLeavesAdjustAndOptionsTakesNothingFromThePress`, `AdjustDrawsAllFiveControlsWhateverTheLastLevelHad` (clear forced shown; weightless irrelevant), `NoPointerReachesALevelWhileAdjustIsUp`, `TheAdjustButtonIsHiddenInClassic`.
* **Captures**: K10 at 16:9 and 4:3; the widget px read against 3.2.
* **Gates**: **G1** the pins. **G2** the clearance table of 3.2 equals the measured one (`CTL/arith2/editor_clearance.py`) to 0.01. **G3** `--press back@t` and `--tap` drive the whole screen with no real input (a capture with no mouse). **G4** S3 (a), (b), S4, the CTL-6 census unchanged (the editor is not in play). S1, S2, S5 to S10.
* **Fails if / recorded as**: a widget over an underlying button (Back at the top-left would meet the preview's clear button: moved); MR's `title` frame (meets restart at 5:4: dropped); the preview forgotten under a weightless level's no-pad rule; readouts with no original font (no digits: accepted).

### CTL-12 (planning step: next): the adjustment screen, part 2: lock, drag, clamp, restore

**Size** L. **Relinks** 1 + one hooked runner. **Depends** CTL-11.

* **Files**: `sim/Adjust.*` (the state machine as a pure class fed one pointer and the layout: `Use {Dead, Widget, Control, None}`, grab, candidate, **slide-along**, refusal, commit-once), layer wiring, tests, README.
* **Tests** (`test_mp_adjust` + `test_mp_layer`): `TheScreenAlwaysOpensLockedAndTheLockIsNeverSaved`, `ADragMovesTheBoxWithThePointersOffset`, `ADragIsClampedWhollyInsideTheView`, `ACandidateMeetingAnotherBoxIsRefusedAndTheControlStays`, **`ADragSlidesAlongAnObstacleInsteadOfStickingToIt`** (x and y tried separately; with `--drag` at 1 and 40 units a tick the end states agree: review 2 F9), `ACandidateMeetingAWidgetIsRefused`, `AGrabIsTheDrawnBoxNotTheHitRect`, `AWidgetWinsWhereItCoversAControl`, `ASizeStepSendsHomeOnlyTheControlsThatNowMeet`, `IfAnyPairStillMeetsEveryoneGoesHome`, `RestoreIsEnabledOnlyWhenAMoveExistsAndKeepsSizeAndOpacity`, `ADragCommitsOnceAtTheRelease` (the store's save counter), `NothingIsSavedIfTheSameAsLastSaved`, `AMovedControlIsClaimedInPlayWhereTheDragLeftIt`, `TheResumePressOverAMovedControlWalksNothing` (CTL-7's swallow, on a layout the editor produced), `AControlDroppedOnTheBackButtonIsRefused`, `ADeadEntryPressDoesNotGrab`, `OutlinesBreatheInTicksNotWallClock`.
* **Captures**: K11 (`--tap` unlock, `--drag` the pair 60 units up, the Restore tile lit); **K12 as a player-style run** (2.1): the same gesture, then a second launch in a level reading the moved layout back.
* **Gates**: **G1** pins. **G2** a **mutation**: one hooked runner removes the clamp, the overlap refusal and the commit-once rule: the three named pins fail; removed. **G3** 10,000 random gestures (seeded, in the suite) never leave a box outside the safe rectangle, two boxes meeting, or a file that fails to read back to the same tuning. **G4** S3 (a), (b), S4, census unchanged. S1, S2, S5 to S10.
* **Fails if / recorded as**: a drag that leaks a portal or a walk (no level is loaded: tested); a press that grabs through a tile; a push-apart creeping back (Penumbra L2 and MR l.119: refused by design); saving per frame (L9); a control lost off-screen (the clamp); the clear-portals preview forced shown but claimed in play when no portal exists (the forcing is a preview flag, pinned); a control dragged across the screen keeping its original anchor (recorded as a limit, not built: Q15's hand placement would settle it).

### CTL-13 (planning step: next): modern is the default for a player

**Size** S. **Relinks** 1. **Depends** CTL-12 (the editor: otherwise the default flips to a layout nobody can move, with a toggle reachable only from the main menu: review 2 F13) **and the owner's signed census list** (1.9: the pair's 1-20 and 3-27, restart's 2-06, clear's 1-09 and 3-14, or a shorter one) **and the owner's word** (Q4).

* **Files**: `game/data/controls.json` (`default_mode` to `"modern"`, `_source` OWNER 2026-10-15), tests (`TheShippedDefaultIsModern`, the truth table's last rows), README (the Input section, the capture recipes: "`--fixed-step` is classic; give `--controls modern` to capture the new HUD; a **windowed** dev run that uses `--tap`/`--hold` without `--fixed-step` should say `--controls classic`"), `docs/original-gameplay.md`, planning/DEVLOG, **no CLAUDE.md edit** (the owner's file; one suggested sentence in the step doc).
* **Gates**: **G1** the **unedited** sweep script (flagless, `--fixed-step`) still reproduces `WALL/after/sweep` 128 of 128 (D4), and with `--controls classic` the same; S3 (a), (b) equal. **G2** a no-`--fixed-step` smoke on a fresh `--saves` directory: INFO `controls: modern (default)`, a screenshot K2-style with the pair; the same on a directory holding `scores.json`, `audio.json`, `achievements.json` and no `controls.json` (an existing save) reads modern; a directory with `controls.json` saying classic reads classic. **G3** `test_mp.exe` with no argument once: all suites passed, 0 skipped, counts recorded against the baseline. **G4** a full `port_build.bat`, 0 warnings. **G5** a grep that no existing capture script's log contains `controls: modern` (N1 inverted). S5 to S10.
* **Fails if / recorded as**: any capture script not carrying `--fixed-step` (it would now be modern: loud, not silent: the byte gate fails); the owner's real save changing without a toggle (S7); the owner not wanting it yet (the step is one data value and can wait: Q4).

### CTL-14 (planning step: next, OPTIONAL, owner-gated Q0/Q1): two contacts, so a thumb on the pair does not stop a shot

**Size** L. **Relinks** 1 + one hooked runner. **Depends** CTL-6. **Do not start without the owner's word**: it amends CLAUDE.md's "Input goes through the port's one pointer path" and README 378-379. If Q0 says "phones", it belongs **before** CTL-11 (it is the only step that delivers "the right thumb stays free"); the default order assumes a desktop.

* **Design sketch**: `touchThisTick` keeps returning the world `Touch`; a second function returns the *controls contact*. Contacts come from `Input::GetContact` (the desktop mouse is contact 0, so the mouse path is unchanged; the engine fills real contacts on Android: `engine/src/platform/android/AndroidApp.cpp:771-778`) and from the replay's `TickInput::contacts`; DEV `--tap` entries may run **concurrently only when `Paths::controls.twoContacts`**, otherwise they serialise exactly as today (routes unchanged). The *controls contact* is the contact on a shown control's hit shape or one that began on one and is down; the world contact is the first other contact; with one contact both are the same contact (today's behaviour). The five single-touch records (`m_shotTouch`, `m_handTouch`, `m_cameraTouch`, the handle track, `MenuTouch`) are fed from the world contact and **reset when the feeding contact changes**.
* **Tests**: `AHeldPairAndASecondContactFiresAPortal`, `TheFirstContactsReleaseOnAPadFiresNothing`, `TheWorldContactChangingMidHoldResetsTheRecords`, `OneContactBehavesExactlyAsToday` (the whole classic pin set re-run), `TwoDevTapsSerialiseWhenTheFlagIsOff`, `ARouteIsUnchangedUnderTheFlag` (128 routes).
* **Gates**: S3, S4 unchanged with the flag off; `--controls modern` + `twoContacts`: the K6 pair with a second `--tap` shot fires one portal while the player keeps walking. **Not verifiable here**: real multi-touch hardware (replay only).

---

## 6. Open questions for the owner, ranked, each with my recommendation

| # | Question | Recommendation |
|---|---|---|
| **Q0** | **Does the port ship on a touch device?** The engine has an Android backend that fills real contacts (`AndroidApp.cpp:771-778`); the port builds and is tested for Windows, where the pointer is the mouse. The answer sets the **hit growth** (`hit.grow_px`: 0 for a mouse; up to 16 px for a thumb, which costs 8 levels' recorded shots at `uniform`, 14 at `balanced`, 16 at `mr`) and whether CTL-14 comes before the editor. | **Desktop now, touch later**: ship growth 0 (right for a mouse, and the strips to the edges already give a thumb a margin), and revisit with Q15's hand placement on a real device. If "phones": do CTL-14 first and re-run the census with the growth you choose. |
| **Q1** | Amend the one-pointer rule and read engine contacts (CTL-14)? Without it the grouping does not let a thumb fire while the other walks; on the desktop the keyboard already does. | **Yes, but last**, after the modern layout is in and you have seen it, and only if Q0 says touch; the core is complete without it. |
| **Q2** | Does your word with the rights holder cover Magic Rampage's control art, locally, in a release package, in screenshots? `docs/permission.md` is still the stub. | Local use as now (read at run time, never committed); decide the release content later; fill the stub before the repository goes public. If "no": drop CTL-1 and CTL-8; nothing else changes. |
| **Q3** | **The default look.** `uniform` (size 0.7, 44 px on every edge: your "same margin" literally; arrow face 24.9 units = 70 px), `balanced` (0.8, bottom 34 px), `mr` (Magic Rampage's own 1.0, bottom 11 px). All three keep the pair under the character's row; the first pass's 1.0 at 44 px would block **20 of 110** levels' recorded shots (25 with its growth). Look at K2 and K2b/K2c. | **`uniform`**: it is the only one that satisfies your words, and the cheapest once any growth is asked for. If the arrow is too small for a thumb, `balanced` is one data line (it breaks the literal "same margin" at the bottom by 10 px). The corner buttons keep the classic's 32-unit body whatever you choose (`corner.min_size` 1.0). |
| **Q4** | When to flip the default to modern, and for existing saves? | At CTL-13, **after the editor (CTL-12) and after you have signed the census list** (1-20, 3-27, 2-06, 1-09, 3-14); no prompt, no migration (the file is new); the Options toggle is the way back. It can wait a release. |
| **Q5** | Pause control art: a framed square (recommended) or Magic Rampage's pill? | Framed square (the layout is square-boxed; a pill sits 17.6 units low). |
| **Q6** | Hit growth: none toward the world (recommended), 16 px, or MR's 64 to 128 px? | **None** (Q0). The corner buttons and the arrows still claim the strips to the screen edges and half of every seam. |
| **Q7** | Options also from the pause? | Not in v1 (needs new `--press` values, a mid-level rebuild, and the leak guards G3/G4). It is reached from the main menu; the way from a level to the main menu (the pause's Levels button, then the level screens) is checked in CTL-9 and its taps recorded; **Options from the pause is the first step after the flip** if the taps are many. |
| **Q8** | Restart/pause/clear act on the press (the port's recorded deviation, classic keeps it) or on the release inside their rectangle (the original's rule)? Restart is irreversible within the attempt and clear takes the portal pair. | **The release (D11)**: a missed or slid-off press cancels; the press still spends the shot. `corner.act_on` is data. |
| **Q9** | After a size step, controls that now meet go home (recommended) or are pushed apart as MR does? | Go home: same guarantee, no state, no animation. |
| **Q10** | Modern base alpha: the classic 0.47 (120/255, recommended) or MR's ghost 0.25 (`idle_scale` 0.53, falling as the pad grows)? At the default **1** level starts with the character behind the pair (2-02, by the spawn-marker estimate; 16 at the first pass, 5 at `balanced`). | Classic's, so a toggle does not change brightness; look at K2 and change `idle_scale` if the arrow is too loud. A fade while the character is behind the pair (MR's `ScreenPadButtonAlphaController` idea) is a later option. |
| **Q11** | Editor Back bottom-right (recommended) or MR's top-left (meets the clear preview)? | Bottom-right. |
| **Q12** | Clear-portals art: our drawn ring-and-X on MR's frame (recommended) or the original's own button? | The drawn glyph; the original's own is the fallback anyway. |
| **Q13** | Pulses and ring in modern: keep (recommended) or drop? | Keep; they are the original's tutorial cue and cost nothing. |
| **Q14** | Mirror / left-handed layout? | None (the owner said bottom-left). |
| **Q15** | A hand placement of the five controls on a real device, folded into the data once? | Yes, after CTL-12, as Penumbra's L4 says; it would also settle where a control dragged across the screen anchors. |
| **Q16** | Wave 4d first, or a second build tree now (P2)? | Wait for 4d; CTL-1 runs now. |

### 6.1 Deliberately NOT in scope

Magic Rampage's attack, jump, rune, down and combo buttons (no counterpart); its forbidden zones, per-frame push-apart and 1498 px row cap; a screen-edge-free drag; the pause as an entry to Options; a left-handed mirror;
gamepad or keyboard navigation of Options and Adjust; translations of the Options words; Magic Rampage's sounds and fonts; any Android build or on-device safe-area verification; a second contact before CTL-14; any change to
`Hud.*`, `ui.json`, `LevelEnd.*`, `Camera.*` or any gameplay number; pushing to `origin`; editing CLAUDE.md or `docs/permission.md` (the owner's); a count of blocked levels shown to the player in the editor (the editor draws the rectangles and the character row instead).

---

## Appendix A. The arithmetic (`CTL/arith2/final_layout.py`, `census2.py`, `census_aspects.py`, `editor_clearance.py`, `deadzone_census.py` (the reviewer's, reproduced); Python only, no game was run)

`ppu = 2.8125`; `M = 15.644444` (44 px), `G = 11.377778` (32 px), `B(1) = 45.511111`, `C(1) = 40.96`, `phi = 0.109375`, `seam = 11.377778`, `edge reach = 17.066667` (48 px), classic discs `2 x pi/4 x 64^2 = 6433.98`, classic corner buttons `3 x 32^2 = 3072`, classic HUD `9505.98` (8.2 percent of the 16:9 view).
Default (`uniform`, size 0.7, bottom 44 px, corner box 40.96): the five boxes, hits and px are the table of 1.2. Hit areas at 16:9 (4:3 and 5:4 are the same areas): walk_left 2,526.7, walk_right 1,783.6 (pair 4,310.2 = 0.67 x classic), restart 2,640.5, pause 3,526.1, clear 3,204.1; total 13,680.9 u^2 = 11.7 percent of 16:9 (15.7 of 4:3, 16.7 of 5:4) against the classic 9,506 (8.2, 10.9, 11.6): 1.44 times.
The two other presets (same corner boxes, the pair differs): `balanced` pair top 207.50, pair hit 4,842.0 (0.75), total 14,212.7; `mr` pair top 206.58, pair hit 5,834.0 (0.91), total 15,204.7.

Census (110 gravity levels, 16:9, EM 16, slack 0; `final_layout.py`, `census_aspects.py`): blocked recorded shots at the default: **1-20 and 3-27** (pair, 5:4 to 18:9), 1-20 alone at 20:9 and 21:9, none at 32:9; restart 2-06 (16:9) and 2-06, 4-23 (18:9); clear 1-09, 3-14 (and 2-24 at 18:9); pause none. With 16 px of growth toward the world: 8 (`uniform`), 14 (`balanced`), 16 (`mr`). The first pass (size 1.0, 44 px, growth 32 px, snap 16 px): 25 (37 at size 1.4); its drawn box alone: 20 (32 at size 1.4); classic discs: 0.
The character at level start behind the pair's box (spawn-marker estimate): `uniform` 1 (2-02); `balanced` and `mr` 5 (2-02, 2-25, 3-16, 3-24, 3-29); the first pass 16.

Corner boxes and the pair at the narrowest supported shapes (5:4, W 320): size 1.4 pause.x 247.012, restart.x 178.290, clear right edge 72.99, pair right edge 154.45, pair top 176.64. 4:3 (W 341.333) size 1.4: pause.x 268.345, restart.x 199.623.

MR's adjust column in units: rows 69.12 / 102.40 / 145.92; tiles 36.409, offsets 36.267 (column half width 54.471 for the size and opacity rows, 51.001 for the lock row; y 50.916 to 170.809); lock frame 49.778, offsets 26.112; Back (0.85 x 128 px) 38.684. Clearance of the default controls against the column (`editor_clearance.py`; the corner box is 40.96 up to size 1.0, so restart's numbers are the first pass's):
5:4 restart overlaps the column at every size; 4:3 7.25 clear at 0.7, overlaps from 1.1; 3:2 28.59, overlaps from 1.4; 16:10 41.39 at 0.7 and **5.83 (Restore against the pair's top) at 1.4**; 16:9 64.14 at 0.7, **22.86 (walk_right) at 1.4**; 21:9 135.25 / 93.39; 32:9 291.70 / 249.72. Restore's bottom 170.81 against the pair's top at 1.4 (176.64): **5.83**.
The hint (`camera.json`, `Matura84_shadow.fnt`): 639 px of advance x 0.4 = 255.6 units; 258.4 to the last glyph's edge; left at size 0.7: 90.74 (16:9, 16:10, 3:2), 74.93 (4:3), 53.60 (5:4); at 1.4: 154.45 / 143.20 / 117.60 / 74.93 / 53.60.

## Appendix B. File and test index

New: `game/sim/Controls.{hpp,cpp}`, `Options.{hpp,cpp}`, `Adjust.{hpp,cpp}`, `PadArt.{hpp,cpp}`; `game/data/controls.json`; `tests/test_mp_controls.cpp`, `test_mp_padart.cpp`, `test_mp_census.cpp`, `test_mp_options.cpp`, `test_mp_adjust.cpp`; `tests/golden/{classic_claim,classic_hud,modern_claim,controls_census}.json`; `tools/art/make_mr_controls.py` and its tests; `docs/magic-rampage-controls.md`.
Edited: `game/MagicPortalsLayer.{hpp,cpp}`, `game/main.cpp`, `game/CMakeLists.txt`, `game/sim/CMakeLists.txt`, `tests/CMakeLists.txt`, `tests/test_mp_layer.cpp`, `test_mp_hud.cpp`, `test_mp_camera.cpp`, `test_mp_routes.cpp`, `README.md`, `docs/original-gameplay.md`, the planning doc (one Step per CTL step, numbered when it lands), `DEVLOG.md`.
Never edited: `game/sim/Hud.*`, `game/data/ui.json`, `game/sim/LevelEnd.*`, `game/sim/Camera.*`, `game/sim/Switches.*`, `engine/` (pin `710bffc`), `.gitignore`, `docs/permission.md`, `CLAUDE.md`.
Files read at run time and never committed: `reference/magic-rampage/**`; the player's `controls.json`.
Test-only environment: `MP_TEST_PORTDATA` (CTL-2), read by `TestPaths()` and by nothing in the shipped game.
