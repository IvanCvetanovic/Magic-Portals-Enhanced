# DEVLOG — Magic Portals Remake

Append-only build diary. Same convention as the Wolf Brigade project: this file is BOTH
the procedure manual and the log, the agent owns capture end to end, and executing the
procedure is part of the definition of done for every session.

**Append-only. Never edit or delete an earlier entry.** If something recorded turns out
to be wrong, say so in a *later* entry. The value of this file is that it's an honest
record of what actually happened, including the parts that were wrong at the time.

---

## Agent procedure — SESSION START (before touching anything)

1. Run `Get-Date` and note the session start time. All `[CLIP]` timestamps are wall-clock
   so footage can be matched to a recording later.
2. Check `reference/apk/` — is the original APK present? Many tasks are blocked without it
   (see `MILESTONE_1.md` §7). If it's absent, say so up front rather than working around it.
3. **Video capture — see the block below. Confirm whether it applies to this project.**

### ⚠️ Video-capture procedure — CARRIED OVER FROM WOLF BRIGADE, NOT YET CONFIRMED HERE

Wolf Brigade is a YouTube devlog series, so its DEVLOG mandates OBS recording at session
start and Godot Movie Maker B-roll renders at session end. That was requested as "same
convention", so it's reproduced here — but **nobody has confirmed Magic Portals is also
being filmed**, and silently committing this project to a video pipeline would be wrong.

**Ivan: is this project being filmed? If yes, delete this warning and keep the block. If
no, delete the block and the `[CLIP]`/`[BROLL]` lines from the entry template.**

If filming:
- **Session start:** launch OBS with `--startrecording --minimize-to-tray` (`obs64.exe`
  from the OBS install dir). Verify the process is actually running. If OBS is missing or
  the launch fails, print a loud, unmissable `⏺ START OBS RECORDING NOW` and get
  confirmation before proceeding — do not silently skip it. Ivan's playtests happen
  mid-session; the rolling capture IS the playtest footage.
- **During:** the moment something visually interesting happens — a feature working for
  the first time, a funny failure, a dramatic bug — note the wall-clock time immediately.
  If the state is worth revisiting on camera, `git tag` it right then
  (`m1-first-portal-transit`, `m1-portal-softlock`). Tags are cheap now and gold later.
- **Session end:** if a visible feature landed, render B-roll via Godot Movie Maker —
  maintain a `showcase/` scene per milestone that drives the feature through the *normal*
  gameplay path (never a parallel code path), then
  `godot --write-movie broll/<milestone>_<feature>.avi --fixed-fps 60`. Keep clips 15–45 s.
  Log each render under **[BROLL]**.

**Note for this project specifically:** `MILESTONE_1.md` §7.6 requires recording the
ORIGINAL game running, for frame-by-frame portal analysis. That is a *reference* artefact,
not devlog footage, and it is required regardless of whether this project is filmed. It is
also the one artefact that can become impossible to obtain — do it first.

## Agent procedure — SESSION END (never skip)

1. Append an entry using the template below, at the BOTTOM of this file.
2. Include one **Inferred mood** line: an honest guess at how the session felt, derived
   from the facts (time lost, retries, wins). Always marked as inferred — Ivan corrects it.
3. **Run `git status` and confirm nothing from `reference/` or `out/` appears.** Record
   that you checked. This is the one check that cannot be skipped (see `CLAUDE.md`).
4. Style: facts, numbers, errors, decisions. Brutally honest — failures are the best
   material. No marketing fluff; narration voice is written later and is not your job.

---

## Entry template (copy, fill, append at bottom)

```
### YYYY-MM-DD HH:MM — Session N: <goal in ~5 words>
**Built:** what now exists that didn't before (1–3 lines, concrete)
**Broke / dead ends:** errors hit, wrong approaches, how long they ate
**Decision:** any choice between alternatives, and why
**Assets check:** `git status` clean of reference/ and out/? (yes/no + what you ran)
**[CLIP] HH:MM** — what to show + `git tag` (one line per moment)
**[BROLL]** — rendered clip filename(s) + one line on what each shows
**Numbers:** anything countable — files, FPS, level count, ms
**Inferred mood:** one line, explicitly a guess from the facts above
```

---

## Entries

<!-- Agent: append below this line. -->

### 2026-07-25 11:11 — Session 1: Repo setup, format-decode groundwork

**Built:** Greenfield → the full project skeleton. Root `.gitignore` (with a long comment
block explaining *why* `reference/` is excluded, so it doesn't get "helpfully" cleaned up
later), `.gitattributes` (CRLF for `.bat`, LF for everything else — carried over from Wolf
Brigade, where a LF batch file mis-parsed in cmd.exe). `game/` as a Godot 4 project:
Compatibility renderer for both default and `.mobile`, `canvas_items`/`expand` stretch,
pixel snap, ETC2/ASTC compression, both pointer-emulation flags off, plus a one-node
`scenes/main.tscn` so the project actually runs. `tools/converter/` as a `src`-layout
Python 3.11+ package (`ethanon2godot`) with zero runtime dependencies, a CLI entry point,
and a pytest/ruff dev extra. Docs: `CLAUDE.md`, `README.md`, `MILESTONE_1.md`,
`docs/permission.md`, this file. Ethanon engine source shallow-cloned to
`reference/ethanon/` (1757 files, HEAD `c140bd3`) — read-only, never built.

**Broke / dead ends:**
- **The one-off segfault (~15 min).** First-ever `godot --headless --path game --import`
  printed every step through `[ DONE ]` and then exited `-1073741819` (0xC0000005,
  access violation) — a crash during shutdown, after the work completed. Did not
  reproduce: deleted `game/.godot/` and cold-imported 3 more times, all exit 0, plus a
  clean `--quit-after 10`. A cold import of a stock empty project also exits 0. So it is
  **not** something in our `project.godot` — most likely a shutdown race in the headless
  editor. Recording it here rather than pretending the first run was clean; if it ever
  recurs, this entry is the prior.
- **Two real defects caught by review, both self-inflicted, both shipped-and-fixed
  within the session:**
  1. **`/reference/` and `/out/` were root-anchored** — so the one rule this repo
     actually has didn't hold where it mattered. The converter's default input path is
     *cwd-relative*, so running `ethanon2godot` from `tools/converter/` would create
     `tools/converter/reference/extracted/`, which a leading-slash pattern does **not**
     match. Extracted assets would have been staged into a tracked directory by the tool
     whose whole job is to touch them. Dropped the anchors (`reference/`, `out/`,
     `.godot/`), added a comment saying why, and extended the `check-ignore` battery with
     the nested cases. The original battery passed 7/7 and proved nothing about this —
     every case I tested was at the repo root. A test that only covers the happy path is
     how a guard rail ends up guarding nothing.
  2. **The parser stubs hardcoded UTF-8** — directly contradicting the decode that had
     just established `.ent`/`.esc` are canonically UTF-16LE-with-BOM (88 of 89 samples).
     The API shape was baking in the exact assumption the research refuted. Added
     `textio.read_ethanon_xml()` (sniffs `FF FE` like `FileManager::GetUTFFileString`
     does, falls back to `utf-8-sig`) and pointed `entity`/`scene` at it. Left `enml.py`
     on UTF-8 with a comment, because ENML genuinely goes through a different engine
     function (`GetAnsiFileString`) that doesn't sniff — a future "consistency" fix there
     would be a regression. Tests went 13 → 18, now covering `parse_file` rather than
     only `parse("")`, and asserting on decoded *content* (a UTF-16 file read as UTF-8
     can decode without raising and still be garbage).
- **PowerShell ate `$LASTEXITCODE` twice.** `& $exe ... *>$null` and
  `$null = & $exe ... 2>&1` both left `$LASTEXITCODE` empty, so two rounds of
  crash-reproduction testing reported nothing at all. Switched to
  `Start-Process -Wait -PassThru` and read `.ExitCode`. Worth remembering — a silent
  empty exit code reads exactly like a pass.

**Decision:**
- **`config/features=("4.6", ...)` as specified, despite the installed editor being
  `4.7.1.stable.steam`.** The spec said Godot 4.6; the machine has 4.7.1 (Steam, shared
  with Wolf Brigade). 4.7.1 opens the project fine and will rewrite the tag to `"4.7"` on
  the first GUI save. Wrote what was asked and flagged the mismatch in `CLAUDE.md` rather
  than silently "correcting" it to 4.7 — if 4.6 is a hard requirement, that needs a real
  4.6 install, not a feature tag.
- **No negation exceptions in `.gitignore`.** `/reference/` is excluded flat, with no
  `!reference/keepme` to make the directory survive a clone. A negation is exactly the
  crack that gets widened later, and the rule here is absolute. Consequence: a fresh clone
  has no `reference/` at all — README documents the three commands to recreate it.
- **Parsers deliberately NOT written yet.** `tools/converter/` is scaffolding with stubs.
  Writing a parser against a guessed format produces code that looks right and is wrong,
  and there is no sample Magic Portals file to validate against yet. The format spec comes
  from the engine source first.
- **`docs/permission.md` is a fill-in-the-blanks stub, not a document.** The grant text
  wasn't available, and a plausible-looking fabricated permission record is actively
  dangerous — someone downstream would rely on it. It carries a loud
  `PASTE THE ACTUAL GRANT HERE` marker, and `CLAUDE.md` instructs agents to stop rather
  than infer scope while that marker is present.
- **`MILESTONE_1.md` written with holes on purpose.** Nobody involved has played the
  original recently enough to specify the portal mechanic, and it's the entire point of
  the milestone. Rather than invent timings, §3 is ten specific questions, each with a
  method for answering it and the JSON key its answer becomes. §7 is the blocker list that
  closes them. Invented level layouts would have been worse than blanks.
- **Ran a 6-dimension format-decode workflow** over the engine source (scene format,
  entity format, ENML/config, particles, sprites/animation, plus a ground-truth agent
  reading only the shipped sample data, as an empirical check on the other five), each
  output then adversarially re-checked against its cited source lines, then reconciled.
  14 agents, 2.28M tokens, 740 tool calls, ~97 min, 0 failures. Result:
  `docs/ethanon-formats.md`, 1800+ lines.

**What the decode actually found** (the parts that change the converter's design):
- **Everything is XML.** `.esc`, `.ent` and `.par` are all TinyXML documents under an
  `<Ethanon>` root — no JSON, no binary, no version header anywhere. `libjson` is
  vendored but referenced by zero engine files. So the converter needs **no third-party
  dependency**: `xml.etree.ElementTree` covers it. `pyproject.toml` stays at zero deps.
- **Canonically UTF-16LE with a BOM** (88 of 89 sample `.ent`s), though the loader sniffs
  the BOM and falls back to UTF-8. Read as binary and sniff — assuming UTF-8 fails on
  most real files.
- **A `.esc` embeds the whole entity definition inline**, *alongside* an
  `<EntityName>foo.ent</EntityName>` reference. Presence of `<FileName>` is what decides
  which one is authoritative. Scenes may therefore be self-contained — worth knowing
  before writing a resolver that assumes the `.ent` must be loaded.
- **Asset paths are flattened.** `ETHResourceManager` builds `resourceDir + fixedSubdir +
  basename` and discards every directory component written in the file. A converter that
  honours the stored path will not find the textures.
- **Entity order in a scene file is bucket-hash order**, not authoring or draw order.
  Depth is recomputed per frame and never stored.
- **A real engine bug that must be reproduced, not fixed:** `ETHEntity.cpp:217` reads
  `flipY` using `m_flipX` as its fallback default. An instance with `flipX="true"` and no
  `flipY` renders flipped on both axes in the original. Reading it "correctly" would
  silently mis-render those entities — hand-verified, not taken on trust.
- **Animation is entirely AngelScript-side.** No frame ranges, no timing, nothing in any
  data file. The converter can never produce animations; they have to be recovered by
  reading the game's scripts and reimplemented. Worth knowing now rather than in month two.
- **Honest caveat:** none of this was validated against a real Magic Portals file, only
  against Ethanon's own samples, and the clone is the Dec 2013 revision. The doc carries
  a concrete check to confirm the shipped game matches that revision once the APK lands.

**Assets check:** yes — `git status --porcelain --untracked-files=all` shows nothing under
`reference/`, and `git check-ignore -v` confirms an explicit rule match for
`reference/ethanon/README.md`, `reference/apk/*.apk`, `reference/extracted/*.ent`,
`game/.godot/*` and `out/*`. Verified *before* running anything that writes to `game/.godot/`.

**[CLIP] —** None. Entirely headless; no game window opened. Nothing to show yet beyond a
terminal. First footage will be `MILESTONE_1.md` §7.6 — recording the ORIGINAL running,
which is Ivan's to capture and is the highest-priority artefact on the project.

**[BROLL]** — None (no visible feature landed).

**Numbers:** 1757 files in the Ethanon clone (HEAD `c140bd3`, 21 Dec 2013). 20 files
authored. 13 converter tests green, ruff clean. `docs/ethanon-formats.md` ≈1800 lines from
14 agents / 2.28M tokens / 740 tool calls / ~97 min. 13 of 13 hand-spot-checked citations
correct. 0 commits (setup stops before committing, by request). 0 lines of gameplay code.
0 of ~120 levels. Godot 4.7.1.stable.steam.a13da4feb, Python 3.11.9, git 2.51.0.windows.1.

**Inferred mood:** guess — steady and slightly pedantic, then genuinely interesting at the
end. No real obstacles; most of the session's actual work was deciding *what not to write
down* (portal timings, permission text, level counts) rather than producing volume. The
segfault was a real 15-minute detour that resolved into "not ours". The format decode is
the one part that produced more than was asked for — the `flipY`/`flipX` bug and the
"animation is script-only" finding are both things that would have cost days if discovered
in month two instead of hour two. One nagging thread remains: none of it can be validated
against the actual game until the APK lands.

---

### 2026-07-25 15:07 — Session 1b: First commit + push

**Correction to the entry above:** it says "0 commits (setup stops before committing, by
request)". That was true when written; Ivan then asked for the commit and push. Recording
it here rather than editing the entry, per the append-only rule.

**Built:** Nothing new — packaging only. Root commit `0cfb733`, 21 files, 3336 insertions,
pushed to `origin/main` (`github.com/IvanCvetanovic/Magic-Portals-Remake`, **private** —
confirmed by an unauthenticated API fetch returning 404 before pushing, since a public
push would have been the irreversible version of this action). Authored and committed as
Ivan Cvetanovic, no co-author trailers, matching the Wolf Brigade convention.

**Broke / dead ends:**
- **`git add` warned "LF will be replaced by CRLF"** on `.gitattributes`, `.gitignore` and
  `tools/converter/.gitignore`. Dotfiles have no extension, so every `*.ext text eol=lf`
  rule missed them and the catch-all `* text=auto` took over. This is the exact annoyance
  Wolf Brigade logged as firing "on every git touch all loop" — pinned
  `.gitignore`/`.gitattributes` to `eol=lf` explicitly before committing, so it never
  starts. Cost ~2 minutes because the fix was already known from the other project.

**Decision:**
- **One root commit, not five.** CLAUDE.md says keep diffs commit-sized, but a greenfield
  scaffold is genuinely atomic: the `.gitignore` is what protects the `reference/` tree
  that was *already on disk* before anything else existed, so splitting it into
  "hygiene → project → converter → docs" would create intermediate commits where the
  protection isn't in place yet. The small-commits rule starts applying next session.
- **Checked repo visibility before pushing, not after.** `gh` isn't installed, so used an
  unauthenticated `api.github.com` fetch — 404 means private. Worth the extra call: the
  push is the publish, and "it's private, right?" is not a question worth answering
  optimistically when the project's central rule is about redistribution.

**Assets check:** yes — three gates run against the actual commit tree (`git ls-tree -r
HEAD`), not the working directory: (1) no path under `reference/`, `out/`, `.godot/`,
`.venv/`, `__pycache__/`; (2) no file with an asset or game-data extension
(`.apk .png .ogg .ent .esc .par .enml .as` …); (3) pack size 0.00 MB, which is itself
evidence no binary slipped in. All 21 files are hand-authored text.

**[CLIP] —** None. Still headless.

**[BROLL]** — None.

**Numbers:** commit `0cfb733`, 21 files, 3336 insertions, 0 deletions, 0.00 MB packed.
Remote had 0 refs before, 2 after (`HEAD`, `refs/heads/main`). 18 converter tests green,
ruff clean, Godot import + headless run both exit 0 at time of commit.

**Inferred mood:** guess — brisk and uneventful, which is the correct feeling for a push.
The only judgement call worth anything was spending one extra API call to confirm the repo
was private before making the history public-shaped. Everything else was mechanical.

### 2026-08-04 21:41 — Session 2: APK arrived; formats doc audited against real data

**Built:** Nothing in `game/` — this was a verification session and it changed what we think
we know. The APK landed (`MagicPortals_3_6_3_public_archive.apk`, 25 MB, sha256 `3753fdf9…`),
so for the first time every "TBD pending APK" in `MILESTONE_1.md` §7 became answerable.
Recreated `reference/` from scratch (it did not exist on this machine at all): `apk/`,
`extracted/`, and a fresh `git clone --depth 1` of `asantee/ethanon` (108 MB). Ran a six-way
adversarial audit of `docs/ethanon-formats.md` against the engine source *and* the real game
data — 320 claims checked — and wrote the corrections into that file as a dated block.

**Chapter structure solved (§7.1).** Three independent files agree: `goldenScores.enml` has
exactly 4 blocks (`goldenScores0..3`) each keyed `level0..level31`; the `.esc` files partition
into exactly 4 groups of 32 (unsuffixed, `a`, `b`, `c`) with zero gaps; and there are
`level_select0..3.esc`. So **4 worlds × 32 levels = 128**, plus 4 level-selects and
`loading_screen` = 133 `.esc`. **Chapter 1 = world 0 = `level0.esc`..`level31.esc`.**
The "~120 levels" in `CLAUDE.md` is now pinned exactly at 128.

**Broke / dead ends:**
- **The APK was sitting in the repo root.** It *was* ignored — but by `*.apk` (`.gitignore:43`),
  a rule written for *export output*, which caught it by luck. The actual hazard was one step
  ahead: unpack anywhere outside `reference/` and none of the 3,115 asset entries match any
  rule, so a single `git add -A` puts the whole game in history permanently. Moved it to
  `reference/apk/`; `git check-ignore -v` now cites `.gitignore:31` (`reference/`), the
  intentional rule. Hash identical before and after.
- **`CLAUDE.md`'s Environment block was entirely stale and would have wasted a session.** It
  describes a Steam Godot at `D:\SteamLibrary\…` "verified 2026-07-25". There is **no `D:`
  drive on this machine** — `C:` is the only one, Steam has no `libraryfolders.vdf`, and
  `godot` is not on `PATH` or anywhere under `Program Files`/`LOCALAPPDATA`. **Godot is not
  installed.** So the mandatory verify (`--headless --path game --import`, `--quit-after 10`)
  **could not be run this session** — recording that plainly rather than skipping it quietly.
  Python is 3.14.3, not the documented 3.11.9. Rewrote the block with the real facts.
- **Three of nine audit agents died on network errors** (2× ECONNRESET, 1× ENOTFOUND) after
  ~74 min of wall-clock. Not a work failure. Re-ran via `resumeFromRunId`, which replayed the
  six good audits from cache.

**Decision — the audit's finding, and it is the important part of this entry.** The formats
doc splits cleanly in two. Its **C++ layer is excellent**: ~200 citations resolved, essentially
zero drift, and blocks quoted "verbatim" are character-for-character right. Its **empirical
layer was measured against the wrong corpus** — every "shipped"/"corpus-wide" statistic
describes the 11-scene MIT testbed, not Magic Portals. The header did disclose this (line 22,
"NOTHING HERE WAS VALIDATED AGAINST A REAL MAGIC PORTALS FILE"), but §§4/5/11/12 then use the
word *shipped* for the testbed, which reads as the game. Nine numbers are materially wrong for
the target. Chose to append a dated correction block that **overrides** the section bodies
rather than rewrite ~130 KB in place — the sections stay as the honest record of what was
believed, and the block says where it wins.

Worst of the nine: the doc says `blendMode=1` (AM_ADD) **"never appears in any shipped
entity."** In the real game it appears **111 times**, on `portal`, `portal_static`,
`antiportal`, `fireball`, `halo`, `projectile`, `spiral`… A converter built on that sentence
renders every portal and glow as a flat alpha quad — it would have deleted the game's visual
identity and looked like a subtle art bug, not a parser bug. Runner-up: `ZAxisDirection` is
`(0,0)` in **all 133** scenes where the doc calls it a degenerate one-off — which actually
*simplifies* things (z never displaces a sprite, Godot position = `(x,y)`), but building the
documented `(0,-1)` path would have misplaced every entity with non-zero z.

Two findings went the other way: there is **no engine fork** (all 23 shipped `<Entity>`
attributes and 33 element names are read by this checkout's parser — §12.1 called this
undeterminable), and element/attribute order never matters on read.

**§7.4 partially closed, honestly.** ENML duplicate keys are last-wins (`Enml.cpp:148-153`,
`m_map[key] = value`), giving `width=1136,height=640` from the real `app.enml` — but those keys
are **desktop-only**; on Android the surface comes from JNI. The shipped `app.enml` has no
`android` block and its `fixedHeight` is commented out, so **`app.enml` does not determine the
Android viewport at all.** Orientation *is* settled — locked landscape per `AndroidManifest`,
so `project.godot`'s `orientation=4` should be `0`. Viewport size stays PROVISIONAL (~512-tall
design space, inferred from `.esc` geometry). Did **not** write a guessed number into
`project.godot`; leaving 7.4 open beats a plausible wrong value, which is the exact failure
mode `MILESTONE_1.md` §"holes on purpose" warns about.

**Assets check:** yes. `git status --porcelain` after every mutating step; final state is three
modified tracked files (`CLAUDE.md`, `docs/ethanon-formats.md`, `DEVLOG.md`) and nothing else.
`git status --porcelain -- reference/` empty. `git check-ignore -v` confirms the APK is caught
by `.gitignore:31`. Nothing from `reference/` or `out/` appears anywhere.

**[CLIP] —** None. Still no Godot installed, so nothing was on screen to capture.

**[BROLL]** — None.

**Numbers:** APK 25,209,080 bytes / 3,126 entries — 133 `.esc`, 404 `.ent` (190 distinct used),
157 sprites, 46 soundfx, 47 particles, 88 effects. 128 levels = 4 worlds × 32. Audit: 9 agents,
6 completed + 3 network-failed then resumed, 320 claims checked, 26 WRONG/UNVERIFIED, all six
sections graded MOSTLY_SOLID, 1.12 M subagent tokens, 414 tool calls, 74 min. Converter under
Python 3.14.3: 18 tests pass, ruff clean. APK corpus vs doc corpus: 4,072 scene instances vs
362; 190 `.ent` vs 89. Godot runs this session: **0 — not installed.**

**Inferred mood:** guess — the good kind of unsettling. Two sessions of format notes turned out
to be right about the engine and wrong about the game, and the one sentence that would have
done the most damage ("AM_ADD never appears on entities") was also the most confidently worded.
Cheap to catch now, expensive to catch after 32 levels had been converted and the art "just
looked a bit flat". The APK finally arriving is the actual headline; the audit is what made it
worth more than a file listing.

### 2026-08-04 22:05 — Session 2b: world-0 inventory; corrected my own correction block

**Built:** The chapter-1 build list (`MILESTONE_1.md` §7.2), measured directly rather than via
an agent: world 0 is **32 scenes, 934 entity instances, 66 distinct `EntityName`**. 49 resolve
to `assets/entities/` (the engine appends `.ent` when the name is bare — 28 of 66 are); 17 do
not, and all 17 carry inline `<Entity>` bodies with no `<FileName>`, i.e. **branch B of §4.4,
now confirmed against real data**. Branch B covers 236 of 934 instances (25%), so both branches
are required before any chapter-1 level loads. Three of the 17 are written *with* an explicit
`.ent` suffix and still have no file (`collision_128_sqr.ent`, `moving_platform_single.ent`,
`single_block.ent`) — so "ends in `.ent`" is not a safe test for "external file". Wrote all of
it into the correction block in `docs/ethanon-formats.md`.

Also promoted one setting out of PROVISIONAL: `window/handheld/orientation` `4` → **`0`**. The
original's `AndroidManifest` declares `screenOrientation="0"` — locked landscape, not sensor.
That is independent of the unresolved viewport *size*, so it stands on its own.

**Broke / dead ends:**
- **I shipped a wrong claim in the block that declares itself authoritative over the rest of the
  doc.** The correction table said joint limits were "load-bearing chapter-1 gameplay". They are
  not: all ten joint-using scenes are suffixed (`level10c`, `14b`, `18b`, `19b`, `21b`, `27a`,
  `28a`, `29a`, `30a`, `30c`) = worlds 1–3. Measured after the fact: **zero** world-0 scenes
  contain `revoluteJoint`, **zero** of the 49 world-0 `.ent` carry `<Joints>`. I had actually
  noticed the suffixes while reading the agent output, and published the agent's phrasing
  anyway. Wrong scoping inside a block whose whole premise is "where this and a section
  disagree, this wins" is worse than the original error it was fixing. Corrected, and the joint
  path is now explicitly deferred out of milestone 1.
- **My first blendMode check for world 0 returned zero AM_ADD entities, which was wrong.** I
  only matched `EntityName` values that literally end in `.ent`, so every bare name was skipped
  — including `antiportal` and `portal_static`, which are precisely the additive ones. Real
  answer after resolving bare names: **2 entities, 22 instances** (`antiportal.ent` ×15,
  `portal_static.ent` ×7). The corpus-wide figure is 111, so the chapter-1 exposure is much
  smaller than the headline — but non-zero, and still the portal FX class. Caught it only
  because the audit's named list and my result contradicted each other.

**Decision:** scoped every corpus-wide row in the correction table to world 0 rather than
leaving them as whole-game counts. Milestone 1 is chapter 1; an audit that reports 133-scene
statistics against a 32-scene milestone silently overstates the work. Net effect: AM_ADD shrinks
to 2 entities, AM_MODULATE drops out entirely, joints drop out entirely, while `<Polygon>`
(14/49) and `layerDepth` (46/49) stay firmly in scope.

Also rewrote `project.godot`'s viewport comment, which was instructing future sessions to "read
app.enml and replace these". That instruction is now known to be a dead end and would have cost
somebody a session — recorded *why* (desktop-only keys, no `android` block, commented-out
`fixedHeight`, `ETHGlobalScaleManager` never engaged) so it doesn't get re-attempted.

**Assets check:** yes — `git status --porcelain` shows exactly four modified tracked files
(`CLAUDE.md`, `DEVLOG.md`, `docs/ethanon-formats.md`, `game/project.godot`) and nothing else.
`git status --porcelain -- reference/` empty. Everything read out of the APK this session was
read **in memory** via `zipfile`; `reference/extracted/` is still empty and nothing was unpacked
to disk.

**[CLIP] —** None. **[BROLL]** — None.

**Numbers:** world 0 — 32 scenes, 934 instances, 66 distinct names, 49 resolved / 17 inline,
25% branch-B. AM_ADD 2 entities / 22 instances. `<Polygon>` 14/49 `.ent`. `layerDepth` 46/49.
Entities present in all 32 scenes: 8. Most-placed: `crystal.ent` ×106 across 28/32 scenes.

**Inferred mood:** guess — mildly humbling in a useful way. Two of my own errors in one hour,
both the same shape: trusting a confident sentence (the agent's "chapter-1", my own `.ent`
suffix test) instead of measuring. That is exactly the failure the audit had just finished
documenting in someone else's work, which is funny, and worth writing down so the next session
reads the correction block skeptically too.

### 2026-08-04 22:20 — Session 2c: asset guard rail, and the anchoring bug (again)

**Built:** `**/game/assets/` in `.gitignore`, default-deny, per Ivan's call. This closes the
hole that `MILESTONE_1.md` §7.7 would have opened on its very next action: `out/` is where the
converter *stages*, `game/assets/` is where reviewed output *lands*, and that promotion step was
a review gate with nothing behind it — the directory was tracked, so one `git add -A` after a
conversion run commits original art permanently. Escape hatch is an explicit
`git add -f game/assets/<file>`; the friction is the point. `game/data/*.json` is deliberately
left tracked (CLAUDE.md rule 1 requires it) with a note that generated JSON still needs a human
read, since level geometry derived from a `.esc` is Asantee level data in another encoding.

**Broke / dead ends:**
- **I wrote the rule as `game/assets/` and asserted in the comment that it was unanchored. It
  is not, and the comment was wrong.** A gitignore pattern containing a slash anywhere but the
  end is anchored to the directory holding the `.gitignore`, so `game/assets/` matches only the
  repo-root one. `reference/` and `out/` are single-segment patterns and get any-depth matching
  for free; this one has an internal slash and does not. Caught by the check-ignore battery:
  `tools/converter/game/assets/z.png` came back **NOT IGNORED** while the root case passed.
  Fixed to `**/game/assets/` and re-verified at three depths (root, `tools/converter/…`,
  `deep/nest/…`), plus confirmed `git add -f` still overrides.

  **This is the same defect session 1 fixed on `/reference/`, re-introduced by a pattern that
  merely looked unanchored.** Session 1's entry says a guard rail tested only at the repo root
  "is how a guard rail ends up guarding nothing" — and I then shipped exactly that, in the rule
  written to prevent it, having just read the warning. Worth stating plainly: the lesson from
  session 1 was recorded but not *applied*; what caught it was running the nested case, not
  remembering. Test the nested case every time — reading about it is not sufficient.

**Assets check:** yes — `git status --porcelain` shows five modified tracked files
(`.gitignore`, `CLAUDE.md`, `DEVLOG.md`, `docs/ethanon-formats.md`, `game/project.godot`).
Nothing from `reference/` or `out/`. All battery fixtures removed after testing.

**[CLIP] —** None. **[BROLL]** — None.

**Numbers:** guard-rail battery 3/3 depths ignored after the fix (1/3 before), force-add
override confirmed. Five tracked files modified this session, zero added, zero binary.

**Inferred mood:** guess — the good kind of annoyed. Three self-caught errors in one session,
all found by running a check rather than by thinking harder, which is the useful pattern to
notice. The `.gitignore` one is the keeper: it is the third time this exact anchoring bug has
appeared in this repo, and the only reason it did not survive is a four-line shell loop.

### 2026-08-04 22:40 — Session 2d: the resumed agents caught me; errata corrected

**Built:** The three network-failed agents finished on the resume (9/9, 0 errors). One of them
audited *my own* 22:05 errata block and found it wrong. Corrected, with the wrong version left
visible above the fix rather than silently overwritten — it lived inside a block that tells
readers to trust it over the rest of the document, so quietly swapping it would be the same
mistake twice.

**Broke / dead ends — two errors, both mine, both from measuring the wrong thing:**

1. **"The engine appends `.ent` when the name lacks it."** False, and §4.5 of the doc I was
   correcting already said so ("Nothing appends .ent, so an extension-less name in branch A can
   never resolve"). I overrode a correct section with a guess. Real split of the 66 distinct
   world-0 names: **35** end in `.ent` and exist, **3** end in `.ent` and do not exist, **28**
   are bare. My "49 resolve" was 35 + 14 bare names that happen to have a same-named file — and
   pairing those is a *bug*, not a resolution. Verified structurally: `crate.ent` is
   `density=0.4 friction=0.96` while inline `crate` appears as both that and `0.2/1.0`;
   `crate_small` inline uses `smaller_crate.png`; bare `sky` has four sprite variants against
   `sky.ent`'s one. Resolving bare→`.ent` silently loads the wrong physics and the wrong art.

2. **"Branch B covers 236 of 934 instances (25%)."** Off by 4x in the wrong direction. Measured:
   `<FileName>` occurs **zero** times across all 32 world-0 scenes — **934/934 are branch B.**
   Branch A is never exercised by any chapter-1 level.

**The correction is good news, and bigger than the errors.** **Chapter-1 levels are fully
self-contained: the converter does not need to read a single `.ent` file to load chapter 1.**
I had written the opposite ("must implement both branches before any chapter-1 level loads").

Knock-on: two more table rows were derived from `.ent` files that chapter 1 never opens.
Re-measured over the 934 inline bodies — `blendMode` `{0:838, 2:74, 1:22}`, so the AM_ADD figure
(22 instances, `antiportal` ×15 + `portal_static` ×7) **survives**; but `layerDepth` is absent
from all 934, so that row does not apply to chapter 1 at all, and `<Polygon>` child elements are
0/934 even though `shape="3"` is on **220/934 (24%)**. That last one is a new open question —
where a `shape=3` inline entity with no `<Polygon>` child gets its vertices — logged rather than
guessed.

**§7.3 answered, and the world↔chapter map is now positively pinned rather than inferred.**
Progression is **linear** — two persisted integers (`lastWorldAllowed`/`lastLevelAllowed`), a
skip-level button exists, medals are cosmetic; the only other gate is free-vs-paid purchase. A
**"golden score" is a maximum portal count** for gold: seven thresholds are literally `0`, which
only parses as "finish using zero portals". The chapter mapping was pinned by cross-referencing
achievement strings in `android_game.bin` against entity placement — *"level 2-32 (dragon
boss)"* ↔ `dragon.ent` in exactly one scene of 128 (`level31a.esc`); *"red key on level 4-19"* ↔
`red_key_state.ent` only in `level18c.esc`; *"level 1-9"/"1-23"* ↔ `rolling_stone.ent` in
`level8.esc`/`level22.esc`. Chapter 1–4 badge strings exist; no Chapter 5 string does.
Also flagged: `achievements.en.lang` is **stale** (covers only ch.1–3, disagrees with the
bytecode) — reading it instead of the bytecode would produce wrong numbers.

**Decision:** stopped deriving chapter-1 facts from `.ent` files entirely. For world 0 the
`.esc` inline body is the only source of truth; the `.ent` tree is for later chapters. Every
chapter-1 statistic in the errata block is now measured over the 934 inline blocks.

**Assets check:** yes — `git status --porcelain` shows five modified tracked files
(`.gitignore`, `CLAUDE.md`, `DEVLOG.md`, `docs/ethanon-formats.md`, `game/project.godot`).
Nothing from `reference/` or `out/`; `reference/extracted/` still empty; all APK reads in memory.

**[CLIP] —** None. **[BROLL]** — None.

**Numbers:** resumed run 9/9 agents, 0 errors, 374 k tokens, 81 tool calls, 24 min (vs 1.12 M /
414 / 74 min for the first). Branch B 934/934 (100%), was published as 236/934. Name split
35/3/28, was published as 49/17. `blendMode` inline `{0:838, 2:74, 1:22}`. `shape=3` 220/934
with `<Polygon>` 0/934. `layerDepth` 0/934. Errors found in my own errata: 2 hard, 2 knock-on.

**Inferred mood:** guess — genuinely glad I re-ran the failed agents instead of writing them off
as a network blip. The resumed run's only unique job was checking work I had already published
as authoritative, and it found four defects in it. Five self-caught errors across the session
now, every one found by measuring rather than reasoning, which is the pattern worth keeping.
The `.ent`-append mistake is the one that stings: the correct answer was already written in the
document, in the section my own block was overriding.

### 2026-08-04 22:55 — Session 2e: the polygon "open question" was my own bad query

**Built:** Closed the `shape=3` collision question instead of shipping it as an open blocker —
it gates 24% of chapter-1 entities, so leaving it unresolved would have blocked the collision
converter. **All 220 `shape="3"` instances carry `<Polygon>`, 220/220, no exceptions.**

**Broke / dead ends:**
- **There was never an open question.** I reported "`<Polygon>` 0 of 934" and wrote it up as an
  engine mystery. I had queried `<Polygon>` as a *direct child of `<Entity>`*. The writer puts it
  under the collision root — `ETHEntityProperties.cpp:490-505`,
  `pCollisionRoot->LinkEndChild(pElement)` — i.e. **inside `<Collision>`**. Re-queried at the
  right depth: 220/220. Same failure mode as the `.ent`-append error an hour earlier: I asserted
  a negative from a query I hadn't validated could ever return a positive. A "0 of N" result
  deserves the same suspicion as a surprising positive — the null hypothesis should have been
  "my selector is wrong", not "the engine is strange".
- Confirmed it wasn't a lucky guess by reading the consumer: `GetPolygonShape`
  (`ETHPhysicsSimulator.cpp:40-53`) dereferences the polygon pointer with **no null check**, so a
  `shape=3` entity without polygon data would crash the original outright. 220/220 is what the
  engine *requires*, not a coincidence of this dataset.

**Useful specifics for the converter:** polygon payload is ENML in normalised −1..1 coordinates,
8 vertices typical (`v0{x=-0.95;y=1;} … v7{x=0.95;y=1;}`), mapped to world space as
`(box.size * 0.5) * vertex + box.pos`. That is a unit-square-relative outline scaled by the
`<Collision>` box — which is exactly the form a Godot `CollisionPolygon2D` takes, so this maps
across with no geometry rework. Also noted: 17 `shape="1"` circle instances carry a vestigial
`<Polygon>` the engine never reads, because the switch at `ETHPhysicsSimulator.cpp:85-98`
dispatches on `shape` and calls `GetCircleShape` for those. A converter that keys off "has a
`<Polygon>`" instead of off `shape` would give 17 circles the wrong collider.

**Also reconciled** the one number left dangling: an agent reported "41 world-0 names resolve"
against my 49 and another agent's 35. It comes from a bare→`.ent` pairing convention that the
inline-body finding invalidates. Authoritative split is 35 / 3 / 28, noted in the errata so a
future session doesn't re-litigate a number sitting in the journal.

**Assets check:** yes — five modified tracked files, nothing from `reference/` or `out/`,
`reference/extracted/` still empty, all APK reads in memory. Converter still 18/18 green.

**[CLIP] —** None. **[BROLL]** — None.

**Numbers:** `shape` over 934 chapter-1 inline bodies: `{0: 513, 1: 192, 2: 9, 3: 220}`.
`<Polygon>` inside `<Collision>`: 220/220 for shape=3, plus 17 vestigial on shape=1.
Open questions closed this session: 3 (§7.1 chapter structure, §7.3 progression, polygon
source). Open questions remaining: viewport size (§7.4, needs playtest), and §7.6 video capture.

**Inferred mood:** guess — six self-caught errors now, and the shape of them has been identical
every time: a confident negative derived from a query I never proved could return a positive.
Worth naming as a rule rather than a mood, since it is the only recurring defect of the session.
The session still nets out strongly positive — three milestone blockers closed and the converter
target simplified to "read no `.ent` files at all" — but the error rate on my own measurements
was high enough that the errata block earns its "read this skeptically" framing.

### 2026-08-04 23:30 — Session 3: the converter works; §7.7 closed

**Built:** The pipeline end to end. `ethanon2godot` now actually converts — all four
stub modules implemented against the format spec, and **all 32 chapter-1 levels convert
to Godot 4 `.tscn` and load in the engine.** That is `MILESTONE_1.md` §7.7, the real
go/no-go for the whole approach, and it passes.

- `enml.py` — a port of `File::ParseString`'s state machine (`Enml.cpp:313-448`), not a
  regex, because several behaviours are load-bearing and a regex silently gets them
  wrong: a **single** `/` starts a line comment; only `\;` and `\` are escapes and any
  other backslash aborts the whole file; trailing whitespace before `;` is preserved but
  leading whitespace after `=` is not; an empty value is an error. Needed because
  `<Polygon>` payloads are ENML, not XML.
- `entity.py` — `<Entity>` parsing, shared by `.ent` files and inline blocks (same
  grammar). Engine-accurate defaults, and booleans go through int parsing so `"true"`
  falls back to the default exactly as `QueryIntAttribute` does upstream.
- `scene.py` — `.esc` parsing. Uses `findall('Entity')` on `<EntitiesInScene>`, **not**
  `iter()`: the nested `<Entity>` is the parent instance's inline *definition*, and
  `iter()` would report every instance twice.
- `tscn.py` — the Ethanon→Godot mapping.

**The mapping, and why it came out simple.** Every chapter-1 entity is `type=0`
(`EO_CENTER`, anchor `size/2`) and no scene carries a `<Scale>`. Godot's `Sprite2D` is
centred by default, so the type anchor **cancels exactly** and all that survives is
`offset = -pivotAdjust`. That is why the converter needs no texture dimensions at all.
`zAxisDirection` is `(0,0)`, so position is `(x, y)` straight through and `z` becomes
`z_index`. Ethanon is +y-down like Godot 2D, so no sign flip. Rotation is emitted as
`-radians(angle)` to match the Android/GLES2 path (`GLES2Sprite.cpp:207`
`RotateZ(DegreeToRadian(-angle))`) — desktop GL uses `+angle`, a real uncompensated
backend inversion, and Android is the target.

**Installed Godot 4.6.3** (`winget install GodotEngine.GodotEngine --version 4.6.3`),
which unblocks the mandatory verify and retires the old 4.6-vs-4.7.1 wart entirely —
4.6.3 matches `project.godot`'s feature tag, so the editor will no longer rewrite it.
Added `tools\godot.bat` so the path lives in one place. Deliberately not the Steam build:
Steam auto-updates and would reintroduce the drift this just removed.

**Broke / dead ends:**
- **`.ent` files have `<Ethanon>` as their root, not `<Entity>`.** My first `entity.parse`
  did `ET.fromstring(text)` and handed the root straight to the field reader, which would
  have returned an entity with every field at its default — a silent all-defaults parse,
  not a crash. Caught while writing the fixture, not by running. Now accepts either shape.
  Chapter 1 never reads a `.ent` so nothing downstream would have noticed.
- **The `<Polygon>` nesting bit me a second time.** Having already documented that it
  lives inside `<Collision>`, I still had to re-check while writing `_parse_polygon`.
  Wrote a test that pins it (`test_entity_polygon_is_read_from_inside_collision`) so the
  third occurrence gets caught by CI instead of by me.
- Polygon vertex keys needed **numeric** sorting, not lexicographic — ENML is an
  unordered map upstream, and `v10` sorting between `v1` and `v2` would silently scramble
  the winding order Box2D depends on. Pinned by a 12-vertex test.
- Two ruff line-length failures on the first pass; trivial, but they mean the pre-existing
  18-test suite never covered anything I wrote today.

**Decision — where converted levels live.** `game/assets/levels/`, i.e. behind the
`**/game/assets/` default-deny gate added earlier today. A converted `.tscn` is Asantee
level geometry in a different encoding, so it belongs behind the same gate as converted
art, not in tracked `game/scenes/`. Consequence, by design: a fresh clone has no levels
and `verify_levels.gd` reports zero found. That is correct, and the script says so rather
than failing.

**Verification — three independent counts agree.** The converter reports 711 sprites and
421 colliders; an audit agent's independent scan of the raw `.esc` counted `Sprite 711`;
and Godot, instantiating the emitted scenes, walked 711 `Sprite2D` and 421 collision
nodes. Different code paths, same numbers. Also **zero skipped instances** — every one of
the 934 converted.

**Assets check:** yes. `git status --porcelain` shows only hand-written source:
`.gitignore`, `CLAUDE.md`, `DEVLOG.md`, `docs/ethanon-formats.md`, `game/project.godot`,
the five converter modules, the test file, plus new `game/scripts/dev/verify_levels.gd`
and `tools/godot.bat`. Converter output (`out/`), staged levels and textures
(`game/assets/**`) and everything under `reference/` are all invisible to git —
`git check-ignore -v` confirms `game/assets/levels/level0.tscn` hits `.gitignore:96`.

**[CLIP] 23:28** — first frame of real converted content loading in Godot; 32 levels,
0 failures. Worth a tag if this project is being filmed (`DEVLOG` §video is still
unconfirmed — Ivan). **[BROLL]** — None; nothing rendered to screen yet, headless only.

**Numbers:** 32 scenes → 32 `.tscn`, 934 nodes, 711 sprites, 421 colliders, 38 textures,
**0 skipped**. Godot instantiation: 2519 nodes total across the 32 levels (the extra over
934 is the Sprite/Body/Shape children). Converter suite 18 → **41 tests**, ruff clean.
`--import` exit 0, `--quit-after 10` exit 0, `verify_levels.gd` exit 0. Godot 4.6.3.

**Inferred mood:** guess — the best session of the three by a distance, and notably the
one with the fewest self-inflicted errors, because the format work was already done and
measured. The `type=0` / no-`<Scale>` discovery is the quiet win: it collapsed the whole
origin problem into `offset = -pivotAdjust` and removed any need to read image headers.
Worth being explicit that this is a *data* milestone, not a game — nothing is playable,
nothing moves, and the portal mechanic is still entirely unbuilt pending §7.6 video.

### 2026-08-04 23:45 — Session 3b: a test that tested nothing; one correction

**Broke / dead ends:**
- **`test_write_scene_negates_rotation_for_the_android_path` asserted nothing.** It read
  `assert f"rotation = {-math.radians(90.0):.4f}"[:12] in text or …`. The f-string is
  `"rotation = -1.5708"` and `[:12]` truncates it to **`"rotation = -"`** — so the test
  passed for any negative rotation whatsoever, and the `or` branch masked it further. It
  would have passed if the emitter produced `-0.001`. This was guarding the single part
  of the mapping I had explicitly flagged as unconfirmed, on the value that silently
  mirrors 47 chapter-1 instances. Now asserts the exact string, asserts the desktop-GL
  `+angle` form is *absent*, and pins `_num` directly.
  **Proved the fix by mutation:** flipped `-math.radians` to `+math.radians` in the
  emitter and confirmed the test fails, then restored. A test I have not seen fail is a
  test I have not verified — that is the rule this should have followed the first time.

**Correction to the 23:30 entry.** I wrote that handing `<Ethanon>` to the field reader
"would have returned an entity with every field at its default — a silent all-defaults
parse, not a crash." The all-defaults part is right; **"silent" is wrong.** With
`shape` defaulted to `BS_NONE`, `is_collidable` is False for every entity, so **all 421
colliders would have vanished from all 32 levels** — loudly wrong on the first run, not
something that would have survived to bite later. The lesson I drew from it was therefore
the wrong one. The real lesson is the same as everything else this session: I found it by
writing a fixture and looking at actual bytes, not by reasoning about the code.

**Assets check:** yes — `git status --porcelain` unchanged in shape; only hand-written
source visible. Nothing from `reference/`, `out/` or `game/assets/`.

**[CLIP] —** None. **[BROLL]** — None.

**Numbers:** 41 tests, ruff clean, mutation check 1/1 (sign flip correctly detected).

**Inferred mood:** guess — a good catch to end on, and an uncomfortable one, since it was
my own test guarding my own known-uncertain value and it was decorative. Seven self-caught
errors across the day; this is the only one that would have shipped as false confidence
rather than a visible break.

### 2026-08-05 00:40 — Session 4: core layer — autoloads, input, progression

**Built:** The game-side layer that consumes converter output. `MILESTONE_1.md` §4 minus
the gameplay itself.

- **Three autoloads, exactly the three CLAUDE.md rule 3 pre-authorises.** `EventBus`
  (signal declarations only — no state, no logic, deliberately dumb so it cannot become
  a god object), `DataLoader`, `GameState`. Load order is *not* alphabetical and matters:
  DataLoader before GameState, because progression validates against level counts.
- **`InputController`** — CLAUDE.md rule 4. Touch and mouse converge here and nowhere
  else. Deliberately **not** a fourth autoload: it is a normal node that publishes
  through EventBus, which respects the three-autoload cap without asking to break it.
  `project.godot` already disables both pointer-emulation flags, so no event is ever
  processed twice and the two paths can be treated symmetrically.
- **`LevelManager`** — load / complete / fail / retry / advance, plus the golden-score
  check.
- **`manifest.py`** in the converter — emits `chapters.json` (level order + per-level gold
  threshold) by parsing the original's `goldenScores.enml` **with this package's own ENML
  parser**. Good dogfooding: thresholds came out matching the file exactly (level0=0,
  level1=1, level2=2, level3=2…), which is independent evidence the ENML port is right.
- `game/data/game.json` for our tunables, and two headless test entry points.

**The data split is legal, not stylistic.** `res://data/*.json` is ours and committed;
`res://assets/data/chapters.json` is converter output derived from the original's design
data and is gitignored with the levels and art. `DataLoader` loads both and exposes
`is_ready` + `missing_reason`, so a fresh clone gets "run the converter" instead of
reading zeros and looking like it works.

**Broke / dead ends:**
- **Autoloads do not exist in `--script` mode.** `verify_core.gd` failed at *compile*
  time with `Identifier not found: DataLoader`. `--script` installs a custom main loop
  before autoloads are registered, so they are not even resolvable identifiers. Converted
  it to a scene run as `--headless --path game res://scenes/dev/verify_core.tscn`, which
  is the more faithful test anyway — it exercises the real autoload path. `verify_levels`
  only worked as a script because it touches no autoload.
- **Four test failures that were the test's fault, not the code's.** I asserted
  `complete_level()` advances to the next level. It does not, by design: it records the
  result and emits, and *advancing is policy* that lives in `Main` behind the
  `advance_on_complete` config flag, so a results screen or level select can sit between
  the two later. Fixed the test to call `advance_level()` explicitly and added an
  assertion pinning the non-advance, so the separation is now specified rather than
  incidental.
- One ruff line-length failure again. Third session running. Worth wiring ruff into a
  pre-commit hook rather than continuing to trip over it.

**Decision — `free()` not `queue_free()` on level swap.** §4 requires retry to be instant
with no load screen. Two things make that true: the `PackedScene` is cached so a retry is
an instantiate rather than a disk read, and the old root is freed *immediately*. Deferred
deletion would leave both levels alive for a frame — double physics bodies, double input
handlers, and a visible flicker on every retry. Pinned by a test that restarts five times
and asserts the container still holds exactly one child.

**Deliberately NOT built: the portal mechanic (§3).** Every value it needs —
`momentum_mode`, `exit_offset_px`, `reentry_lockout_s`, `transit_time_s`, `camera_lerp_s`
— has to be measured by frame-stepping recorded footage of the original, and that footage
does not exist (blocker §7.6). `game.json` carries an explicit `_about` block saying so
and instructing whoever reads it next not to create `portals.json` from estimates. There
is no player character either; `Main` is honestly labelled a harness.

**Assets check:** yes — `git status --porcelain` shows only hand-written source. Converter
output (`out/`), staged levels/textures/data (`game/assets/**`) and `reference/` are all
invisible to git. Verified `game/assets/data/chapters.json` hits `.gitignore:96`.

**[CLIP] 00:37** — `verify_core` going 46/46 with a real converted level loading,
restarting and walking all 32 levels of chapter 1. **[BROLL]** — None; still headless.

**Numbers:** converter 41 → **48 tests**, ruff clean. Godot side: `verify_levels` 32/32
levels, `verify_core` **46/46 checks**, `--import` exit 0, main scene runs 30 frames with
zero errors or warnings. 3 autoloads (the cap), 0 magic strings (Intent is an enum), all
GDScript typed. Chapter manifest: 4 worlds, 32 levels populated for world 0, gold
thresholds spanning {0, 1, 2, 4, 6, 8, 10}.

**Inferred mood:** guess — steady and unglamorous, which is right for plumbing. The
satisfying part was the ENML parser round-tripping the original's own score file without
special-casing; the useful part was the test catching that I had assumed a coupling
(`complete` → `advance`) the design deliberately avoids. Still nothing playable, and I
want to be clear-eyed that the project remains one blocker — §7.6 video — away from the
thing that actually makes it Magic Portals rather than a level viewer.

### 2026-08-05 01:15 — Session 4b: a passing test that proved nothing (again)

**Broke / dead ends:**
- **The assertion carrying the whole "retry is instant" claim was decorative.** It read
  `container.get_child_count() == before` after five restarts. **Mutation-tested it:
  swapped `free()` for `queue_free()` and it still passed 46/46.** Of course it did —
  `_swap_in` calls `remove_child()` *before* freeing, so the container holds exactly one
  child either way. Child count can never detect deferred deletion. The leak I claimed to
  be guarding is an orphaned node staying alive to end-of-frame, which that assertion
  cannot see.
  Replaced with `not is_instance_valid(first)` immediately after one restart, which does
  discriminate: baseline 47/47 pass, `queue_free()` mutation fails on exactly that line.
  **Second session running that a test I wrote to guard a known-risky decision turned out
  to assert nothing.** Same shape as the rotation-sign test. The rule I keep re-learning:
  a test written to protect a specific choice must be run against the wrong choice, or it
  is decoration. Mutation-testing is now the default for any assertion I would cite as
  evidence.
- **Also verified the harness can fail at all.** Forced a `_check(..., false)` and
  confirmed exit code 1 and the FAIL line. Previously I had only ever seen it exit 0, and
  in one run I filtered output through `grep`, so `$?` was grep's status and not Godot's.
  A gate never observed failing is not a verified gate.
- **`LevelManager._init(container)` had a silent wrong-tree trap.** `_init` args are not
  passed when Godot instantiates a node from a `.tscn`, so `_container` would be null, and
  `_ready` fell back to `_container = self` — levels would parent to the manager instead of
  the `Levels` node, with no error. My own fallback was the tell that I knew null was
  reachable and chose to paper over it. Now: no fallback, `push_error` on a null container,
  `load_level()` refuses, and a `set_container()` for post-construction use. Tested both
  the refusal and the recovery path.

**Assets check:** yes — only hand-written source visible to git; `reference/`, `out/` and
`game/assets/**` all invisible. Full pipeline re-run from clean afterwards.

**[CLIP] —** None. **[BROLL]** — None.

**Numbers:** verify_core 46 → **51 checks**, all passing; mutation checks 2/2 discriminating
(queue_free caught, deliberate-failure exit 1 confirmed). Converter 48 tests, ruff clean.
verify_levels 32/32. Main scene 30 frames, 0 error lines. Every gate exit 0.

**Inferred mood:** guess — mildly irritated at myself in a productive way. The session's
real output was not the 5 extra checks, it was noticing that two of my last three
"safety" tests were theatre. Three of the four things fixed today were mine, and all
three were the same failure: writing an assertion that describes the intent rather than
one that can fail when the intent is violated.

### 2026-08-05 03:10 — Session 5: it plays; bytecode decoded; portal mechanic mapped

**Built:** The game actually plays. Character spawns, moves, falls, collects crystals,
dies to hazards, retries instantly, reaches the exit, advances. Static portals teleport it.
32 chapter-1 levels, all verified in-engine.

- **`LevelRuntime`** — classifies converted nodes by role from `entity_roles.json` and
  wires triggers. One-time scan at load, Area2D signals after (rule 6: never scan per frame).
- **`Player`** (CharacterBody2D), **`PortalSystem`** (static portals), camera follow.
- **`tools/asbc/`** — an AngelScript bytecode reader ported from `as_restore.cpp`.
- **`docs/original-gameplay.md`** — a new doc, separate from `ethanon-formats.md` because
  that one is about *file formats* and this is about *this game's behaviour*. Every claim
  carries a provenance marker: DATA / DECODED / IDENTIFIER / ABSENCE / GUESS.

**Broke / dead ends — two silent parser bugs, both found by looking at bytes:**

1. **`<CustomData>` had been parsing as empty for every entity, always.** The code read
   `var.get("name")`/`var.get("value")`, but the real shape is child *elements*:
   `<Variable><Type>float</Type><Name>speed</Name><Value>4</Value></Variable>`. No error,
   no exception — just an empty dict every time. This was the single most costly bug of the
   project so far, because **custom data is where the original's designers put the
   gameplay**: `max_portals`, `teleportable`, portal `index`/`destiny`/`color`, platform
   `speed`/`stride`. Fixing it unlocked most of what follows.

2. **The converter was discarding every trigger region in the game.** `door.ent`,
   `crystal.ent` and `portal_static` are `shape=BS_NONE` — no physics fixture — but they
   still ship a `<Collision>` box that the original tests overlap against itself. My
   emitter dropped collision data whenever it produced no collider. Now preserved as
   `metadata/trigger_size`/`trigger_offset`. Without this there is no way to touch a door.

**THE BIG STRUCTURAL FINDING, and a test failure earned it.** `verify_gameplay` asserted
that every static portal's `destiny` resolves to another static portal. Level 1-2 failed:
one portal, `index 0`, `destiny 1`, and no portal with index 1 — but `max_portals: 1`.
**Static and player-placed portals share one index space.** 1-2 is the placement tutorial:
a fixed blue portal already pointing at the red one you are about to create. I would not
have found that by reading; the assertion found it by being wrong.

**Bytecode decode — worked, and stopped honestly.** Ported `ReadEncodedUInt64`,
`ReadString` (including the back-reference table) and the enum section from
`as_restore.cpp`. Decodes the header and all three enums cleanly, then stops at byte 239 of
523,826 and says why. The clean parse is the real result: it proves the file matches the
AngelScript 2.26.1 the Ethanon clone vendors, which was the main risk.

Best find: **`SCREEN_PAD_BUTTON` has exactly `NO_SCREEN_BUTTON=0`, `LEFT_SCREEN_BUTTON=240`,
`RIGHT_SCREEN_BUTTON=255`. There is no jump button.** Combined with `jumpForth` being the
only jump identifier in the binary, **the jump the remake currently implements is almost
certainly wrong.** Left enabled so the harness is playable, with a loud note in
`player.json` saying to expect to delete it rather than tune it.

**Why the constants are still not extracted, precisely.** `ReadGlobalProperty` stores a
name, namespace, type and an *initialisation function* — not a value. So
`FIRST_PORTAL_MIN_TIME` is an immediate operand inside bytecode, and getting it needs
`ReadObjectTypeDeclaration` phase 2, `ReadDataType`, `ReadFunctionSignature`, `ReadFunction`
*and* `ReadByteCode` with instruction decoding. Feasible, clearly the right path, not a
quick win. Written up in `docs/original-gameplay.md` §5 rather than left as a vague TODO.

**Decision — build with marked guesses rather than not build.** MILESTONE_1 §3 does not
forbid guessing; it forbids *silent* guessing ("gets a `# GUESS` comment… No silent
guesses") and gates *acceptance* on none remaining. So `player.json` and `portals.json`
carry `_guess: true` per block plus an `_about` block stating what is known structurally
versus invented numerically. The milestone stays legitimately open; the game exists.

**Also decided:** `portals.json` and `player.json` are committed, unlike `chapters.json`.
`chapters.json` is a per-level table mechanically derived from a shipped data file;
these are a dozen behavioural facts and our own tuning values, closer to
`docs/ethanon-formats.md` (committed, full of cited observations) than to extracted art.
Stating the reasoning once here rather than relitigating per file.

**Assets check:** yes — `git status --porcelain` shows only hand-written source.
`reference/`, `out/` and `game/assets/**` all invisible to git. Full pipeline re-run from
clean afterwards.

**[CLIP] 02:52** — `verify_gameplay` 136/136: a converted level, a real physics body,
and the player landing on geometry decoded from the original's `.esc` bytes.
**[BROLL]** — None; still headless. Nothing has been seen on screen yet by anyone.

**Numbers:** converter 48 → **52 tests**, ruff clean. Godot: verify_levels 32/32,
verify_core 51/51, verify_gameplay **136/136**, main scene 60 frames 0 errors, all exit 0.
Chapter 1: 934 instances, 114 crystals, 7 static portals, 4 portal pairs. Bytecode: 3 enums,
239 bytes clean, `noDebugInfo=false` (not stripped). CustomData keys recovered: 27 distinct.

**Inferred mood:** guess — the best session yet, and the one where the method paid off most
visibly. Two silent parser bugs found by dumping bytes rather than reasoning; one genuine
design discovery (shared portal index space) delivered by a test being wrong; and a decode
that answered a real question — no jump button — that would have taken a person a playthrough
to notice and me a lot of guessing to get wrong. The honest caveat stands: this plays, but
it does not yet play like Magic Portals, and I have no way to know how far off it is.

### 2026-08-05 03:40 — Session 5b: the fall-through test was decoration too

**Broke / dead ends:**
- **`verify_gameplay`'s headline assertion did not discriminate.** It asserted the player
  had not fallen past `bounds_max.y + 2000` — about 2256 px below spawn. But 120 frames at
  the guessed gravity covers only ~1500 px, so a player falling through the entire world
  never reaches the threshold: **the test passed by timing out.** Proved it by mutation —
  stripped every collider from the converter output, reconverted, and the suite still
  reported **136/136, exit 0**, while quietly listing all 32 levels as "never landed".
  Changed the assertion to require `landed`. Re-mutated: **32 failures, exit 1**; correct
  output **136/136**. Baseline `_no_ground` is empty across all 32 levels, so requiring a
  landing is safe as well as strictly stronger.

  Third time this exact defect has appeared — an assertion that describes the intent
  instead of one that can fail when the intent is violated. The first two I caught by
  mutation-testing on purpose; this one I only caught because it was pointed out that the
  arithmetic didn't work. The lesson is not "mutation-test the risky ones", it is
  **mutation-test the ones whose result you intend to quote.**

- **`PortalSystem` was emitting a counterfeit input event.** On teleport it fired
  `EventBus.intent_released(...)` from a physics callback — synthesising player input from
  a collision. Nothing consumed it yet, but `Player` is already connected to the press side
  of that pair, so any listener tracking press/release symmetry would have seen a release
  with no press. It also breaks the single rule `InputController` exists to enforce.
  Replaced with a real `EventBus.portal_traversed(traveller, entry_index, exit_index)`.

**Also:** cross-referenced the new `SCREEN_PAD_BUTTON` finding into the §7.4 errata block —
the on-screen pad occupies viewport space and must clear the 96 px touch floor, so pad size
and viewport height have to be settled together rather than separately.

**Assets check:** yes — only hand-written source visible; `reference/`, `out/`,
`game/assets/**` invisible. Full pipeline re-run from clean.

**[CLIP] —** None. **[BROLL]** — None.

**Numbers:** mutation checks now 4/4 discriminating across the project. verify_gameplay
136/136 (mutant 104/136, 32 fails). All gates exit 0.

**Inferred mood:** guess — chastened but in the useful direction. The session's real
deliverable is a game that plays; its real lesson is that three of my own "proof" tests have
now turned out to prove nothing until mutated, and I should stop treating a green suite as
evidence until I have seen it go red on purpose.

### 2026-08-05 05:20 — Session 6: the portal mechanic, from owner testimony

**Built:** Player-placed portals. The game is now recognisably the game.

Ivan supplied the piece no amount of decoding was going to give up cheaply: **movement is
two buttons, portals are created by tapping the screen, at most two exist at once, and
passing through destroys them — for any teleportable object, not just the player.**

That single paragraph resolved most of §3's open structure, and it *agrees with the
decode*: `SCREEN_PAD_BUTTON` decodes to exactly `NO_SCREEN_BUTTON`, `LEFT_SCREEN_BUTTON`,
`RIGHT_SCREEN_BUTTON`. Testimony and bytecode independently arriving at "two buttons, no
jump" is the strongest evidence in the project so far.

It also retroactively explains three things I already had but had not connected:
- `default_max_portals: 2` — I guessed 2. It is 2. Now marked OWNER-confirmed, not a guess.
- The `teleportable` per-instance flag exists precisely because *items* travel too.
- The achievement budgets ("Finish 1-20 with 8 portals or less") are a **running total**,
  not a simultaneous cap — which only makes sense if portals are consumed and re-placed.

Added an **OWNER** provenance marker to `docs/original-gameplay.md`, ranked above
IDENTIFIER and ABSENCE: it is primary testimony, and it is the only available source for
behaviour that leaves no trace in the data.

**Broke / dead ends:**
- **A tap was bound to jump.** `Player` connected to `intent_pressed` and jumped on
  PRIMARY. With taps now placing portals, every portal placement would also have fired a
  jump. Unbound entirely; jump is now reachable only via an explicitly-named
  `request_jump()` that nothing calls in normal play, and `player.json` sets
  `enabled: false`. Three independent sources say the original has no jump, so this went
  from "probably wrong" to "off".
- **Placed portals were parented to the game root**, not the level — so a retry would have
  left them floating in the next attempt. `LevelRuntime` now exposes `level_root` and
  anything spawned during play parents there, which makes cleanup a consequence of the
  existing level-free rather than a thing to remember.

**Decision — model the confirmed behaviour as data, not as code.** `consume_on_traverse`,
`static_portals_persist` and `on_cap_recycle_oldest` are JSON switches in `portals.json`
rather than hardcoded rules, even though the first is owner-confirmed. Two of the three are
still inferred (statics persisting is reasoned from `teleportToFirstStaticPortal` and from
1-1 being unsolvable otherwise; recycling-the-oldest at the cap is a pure guess), and
playtesting should be able to flip them without a code change.

**Numbers:** verify_gameplay 136 → **146 checks**, all passing. verify_core 51/51,
verify_levels 32/32, converter 52 tests, ruff clean, main scene 0 errors, every gate exit 0.
Chapter 1: 934 instances, 114 crystals, 7 static portals in 4 pairs, 2-portal player cap.

**Assets check:** yes — only hand-written source visible to git; `reference/`, `out/` and
`game/assets/**` invisible. Full pipeline re-run from clean.

**[CLIP] 05:14** — placing two portals, walking into one, coming out the other, and both
vanishing. First moment the project looks like Magic Portals rather than a level viewer.
**[BROLL]** — None; still headless. Nobody has watched this run.

**Inferred mood:** guess — the most efficient session by far, and not because of anything I
did: thirty seconds of the owner's recollection unlocked more of §3 than several hours of
decoding. Worth recording as a process lesson rather than a fact — I had been treating the
binary as the only remaining source of truth and had not thought to just ask. The decode
was still worth it (it independently corroborated the testimony and killed the jump), but
asking first would have been cheaper.

### 2026-08-05 07:30 — Session 7: all four worlds, menus, HUD, audio

**Scope widened by Ivan**, explicitly: full game, no check-ins. `CLAUDE.md` said "chapter 1
only, do not build ahead of the milestone" and `MILESTONE_1.md` excluded menus, audio and
chapters 2+. Rather than leave the docs contradicting the code, both now carry a dated note
saying the scope boundary is superseded — while keeping §3's quality bar, which is the
actual reason the milestone is not closeable. Widening what gets built must not lower the
bar for calling it done.

**Built:**
- **All 128 levels convert with ZERO skips.** Previously 20 instances were refused. Both
  causes fixed:
  - **`BS_COMPOUND`** (8 instances — `spinning_platform`, `seesaw`, `motor_seesaw`): the
    `<Compound>` payload is ENML inside `<Collision>`, one entity per Box2D fixture, all
    values normalised −1..1 against the collision box. Emits one `CollisionShape2D` per
    fixture. Sorted ALPHABETICALLY by ENML name, because the engine copies out of a
    `std::map` — index order follows the map, not the file, and per-fixture friction keys
    on that index.
  - **`ET_VERTICAL` (`type=2`)** (12 instances, `light_off.ent`): anchors bottom-centre
    rather than centre, so placing one needs the sprite height. Added `imagesize.py`, a
    24-byte PNG/BMP header read — no Pillow, because the converter's zero-dependency rule
    deserves better than "I wanted image dimensions".
- **Menus:** main menu and level select. Four chapter tabs, 32 buttons each, locked
  per `GameState`'s high-water mark, `★` for a golden score and `✓` for cleared.
- **HUD:** level label, crystal and portal counters, and the on-screen pad — **two buttons,
  left and right**, matching the decoded `SCREEN_PAD_BUTTON` enum. The pad publishes the
  same EventBus signal the keyboard does, so gameplay cannot tell them apart.
- **Audio:** 46 cues mapped in `audio.json`. The four portal cues are confident — the
  original has `playPortalCreatedSound` / `Launch` / `Killed` / `Failed` as identifiers.
  Music-to-world mapping is a marked guess.
- `--copy-ui` stages the 80 UI sprites and 46 sounds. The `hd/`/`fullhd/` tiers are
  skipped deliberately: §8.5 records that four fullhd sky files have a different aspect
  ratio from their 1x counterparts, so mixing tiers changes world sizes.

**Decisions:**
- **`AudioManager` is a node under `Main`, not a fourth autoload.** Rule 3 caps autoloads
  at three and says not to add one without asking. `Main` outlives levels, so a node there
  has the same lifetime an autoload would — the rule cost nothing to keep.
- **Menu suspends `Levels` and `PortalSystem` rather than pausing the tree**, because
  `get_tree().paused` would also freeze the menu's own input handling.
- Missing audio is a **silent no-op**, not an error: audio is converter output and
  gitignored, so a fresh clone has none and must still run.

**Broke / dead ends:**
- Patched `tscn.py` through a shell heredoc and the `\n` escapes came out as real newlines,
  producing 29 syntax errors inside f-strings. Fixed with a direct edit. Lesson: stop
  editing Python source through shell heredocs when the content contains escapes — use the
  editing tools, which do not re-interpret anything.

**Still running:** a 7-agent workflow researching entity behaviour semantics (platforms,
switches, hazards, breakables, enemies, level properties, portal agents) across all 128
levels. That is the input for the remaining ~80 entity behaviours. Not yet returned.

**Assets check:** yes — only hand-written source visible to git; `reference/`, `out/` and
`game/assets/**` invisible. Full pipeline re-run from clean.

**[CLIP] 07:22** — main menu → level select → chapter 1 with all four chapter tabs visible.
**[BROLL]** — None; still headless.

**Numbers:** 128/128 levels, **0 skips** (was 20), 4065 instances, 10752 nodes, 2883
sprites, 1838 colliders, 66 entity textures + 80 UI sprites + 46 sounds. Gates: levels
128/128, core **57/57**, gameplay 146/146, converter 52 tests, ruff clean, main 0 errors.

**Inferred mood:** guess — productive but stretched. The compound-shape and vertical-anchor
work was satisfying because both were real format gaps rather than bugs, and closing them
took the converter from 99.5% to 100% of the game's data. The honest caveat is unchanged
and now larger: there are 4065 entity instances converted and only a handful of them
actually *do* anything yet.

### 2026-08-05 10:15 — Session 8: behaviour components; the whole game verified

**The single most useful finding of the project.** The engine builds each entity's script
callback as `"ETHCallback_" + RemoveExtension(entityName)`, so **an entity has behaviour
if and only if a matching `ETHCallback_*` symbol exists in `android_game.bin`.** 96 do.
That converts "which of these 178 entities actually do something?" from guesswork into an
exact-match byte search.

It immediately killed three things I would otherwise have built: `moving_platform_single.ent`
carries a `moveStride` that nothing reads, and inline `platform` and `anchor` carry
`speed`/`stride` that nothing reads. No callback exists for any of them. Their custom data
is **dead copy-paste**, and animating them would have invented motion the original never
had. `verify_gameplay` now asserts they stay inert.

**The `stride` disambiguation rule**, derived from the corpus and applied throughout:
paired with a `speed` float ⇒ `stride` is **pixels**; a bare uint with no `speed` ⇒
**milliseconds**. Corroborated by `carranca` carrying both `stride` and `startStride`, and
by a `button`'s 300 on a 26×16 body — not a distance.

**Built:** `Behaviours` — one parameterised component set, not a class per entity
(rule 5): `Mover` (oscillate / shuttle / gated), `Switch`, `TimedCollectible`, `Reflector`.
Wired: **29 movers, 46 switches, 46 doors** across the four chapters. Buttons and doors
pair by channel (`idx` ↔ `switchIdx`) through EventBus, so neither has to find the other.

Moving bodies are re-parented onto `AnimatableBody2D` with `sync_to_physics`. A teleported
`StaticBody2D` does not carry riders — the player would slide off a moving platform.

**Broke / dead ends — and the tests earned their keep three times:**
1. **The 128-level sweep timed out** at 10 minutes because it loaded every scene twice.
   Folded the behaviour counting into the single existing pass and cut settle frames from
   120 to 45 (the loop breaks on landing, so it is a ceiling not a per-level cost).
2. **A nested loop I inserted produced a tab/space mix** that Godot rejects outright.
   Flattened it to one loop over a precomputed list rather than fighting the indentation.
3. **Chapter 4 is a ZERO-GRAVITY chapter and I did not know.** Five levels failed the
   landing assertion. `properties` on 4-2..4-19 sets `no_gravity`, and 4-20..4-32 sets
   `darkest` — which is exactly what the "Alone in the dark, finish level 4-25" achievement
   refers to. The test was right and I was wrong. Implemented `no_gravity`, `darkest` and
   `no_music`.
4. **…and then 4-9 still failed, at 142 px.** Not depenetration. The spawn sits between two
   `crate_low_emissive` rigid bodies, and I had disabled gravity on the *player* only — so
   a crate fell under Godot's project gravity and shoved him. **`no_gravity` is a LEVEL
   property, not a player one**: every dynamic body has to float, which is also what makes
   those levels puzzles about pushing objects through portals. Fixed, and 535/535.

The zero-g assertion inverts deliberately: in those levels the player must **stay put**,
not land. Tolerance is 16 px — collision depenetration is a few pixels, gravity over the
same window would be ~340, so the two magnitudes are twenty times apart and the bound
discriminates rather than merely permitting.

**Assets check:** yes — only hand-written source visible to git; `reference/`, `out/` and
`game/assets/**` invisible. Full pipeline re-run from clean.

**[CLIP] 10:02** — 535/535 across all 128 levels, including the zero-g chapter behaving
as zero-g. **[BROLL]** — None; still headless.

**Numbers:** 128 levels, 0 skips, 4065 instances. Gates: levels 128/128, core 57/57,
gameplay **535/535**, converter 52 tests, ruff clean, main 0 errors — all exit 0.
470 crystals, 29 movers, 46 switches, 46 doors. 96 ETHCallback symbols catalogued.

**Inferred mood:** guess — genuinely good. The callback-name discovery is the kind of
finding that makes the rest of the work smaller rather than larger, and the zero-gravity
chain (test fails → wrong hypothesis → still fails → right hypothesis) is the clearest
example yet of the tests doing real work rather than confirming what I already believed.
Two of those three failures I would have shipped without noticing.

### 2026-08-05 12:40 — Session 9: pushed; destruction economy

**Pushed** `036590b` to `origin/main` under Ivan's identity (already the configured git
user). Pre-commit gates run against the staged tree, not the working directory: no path
under `reference/`, `out/` or `game/assets/`, no asset extension, and re-confirmed the
repo is private with an unauthenticated API probe (404) before publishing — the push IS
the publish, and this project's central rule is about redistribution. 58 files, 86 KiB
pack, zero assets in `git ls-tree -r HEAD`.

**Built — the destruction economy.** The original reads four per-instance flags through a
single predicate it literally names `isBreakableOrExplosiveOrBurnable`:
`demolisher` destroys `breakable`; `explosive` detonates and chains; `burnable` enters a
timed burning state; `breakable` marks what can be smashed. Every hazard is a source or a
target, and almost all of it is `teleportable` — so the puzzle is always *route a
destructive object onto a destructible one through a portal pair*.

Components: `Demolisher`, `Explosive`, `Burnable`, `FireSource`, `ShockField`,
`GravityWell`, `Turret` (carranca) + `Projectile`, `Launcher`, `Blink`.

Counts attached, and they corroborate the research independently: **demolisher 27**
(research: 27/27 placements), **explosive 31** (31/31), turret 28, fire 22, shock 21,
gravity wells 10, launchers 3, burnable 65.

Notes worth keeping:
- A rolling stone is **not** a touch-kill hazard. In level 1-9 — the level the "Like a
  rolling stone" achievement names — the player spawns 58 px from one on the same
  platform, so it must be safe to stand beside. It is a physics prop that breaks things.
- A barrel bomb is **inert on contact**: no `ETHBeginContactCallback_barrel_bomb` exists,
  only a per-frame callback whose single local is `brightness` — an emissive "live bomb"
  tell. Detonation is *requested* by something else, so `detonate()` is exposed rather
  than self-triggered. Chains are deferred a frame so a long chain cannot recurse.
- A shock field's ring is **drawn**, not invisible. The original scales `white_ring.png`
  so its radius equals `areaRadius`; hiding it would make the hazard unfair.
- `blink` is **purely cosmetic** — a designer telegraph. It must never gate anything, and
  it yields `modulate` to a burning crate rather than fighting it for the property.

**Broke / dead ends:**
- **Flags were only applied to role-classified nodes.** A crate has no gameplay role — it
  is a physics prop — but it still carries `burnable`, so only **11 of 65** burnable
  objects were flagged and the rest silently were not. Found because the count looked too
  small against the corpus, not because anything failed. Split the pass: roles to
  role-classified nodes, flags to *every* entity. Now 65, cross-checked against the raw
  data.
- The corpus also carries exactly one `burnable="0"`, which must NOT count. `0` is a real
  "off" value, not absence — the same trap as the golden score, where 0 means "finish
  using zero portals". The flag reader handles it and the test pins the exact number
  rather than `> 0`, so a regression to the 11-behaviour cannot pass.

**Assets check:** yes — post-commit audit on the real tree: 0 asset-extension files, 0
paths under a gitignored dir.

**[CLIP] 12:31** — a rolling stone smashing a breakable wall it was routed into through a
portal. **[BROLL]** — None; still headless.

**Numbers:** gates levels 128/128, core 57/57, gameplay **541/541**, converter 52 tests,
ruff clean, main 0 errors. Commit `036590b`, 6773 insertions.

**Inferred mood:** guess — steady. The flag-coverage bug is the interesting one: nothing
failed, nothing errored, and the only reason I caught it was that 11 looked wrong next to
a corpus I had already measured. Having the counts in the test as exact numbers rather
than `> 0` is what turns that from a lucky glance into a permanent guard.

### 2026-08-05 15:20 — Session 10: keys, locks, minions, hinges

**Built:** the actor layer and hinges. Attached counts all match the corpus exactly:
keys **39**, keyholes **41**, locked doors **41**, minion spawners **62**, hinges **10**.

- **Keys/locks** pair BY COLOUR, never by proximity — per level the colour multisets of
  key, keyhole and door_locked are identical, but a keyhole and its door can sit 200-340 px
  apart, so a nearest-neighbour heuristic would mis-pair them. There is no physics contact
  callback for a key anywhere in the binary; pickup is distance-polled, so it is a range
  test rather than an overlap.
- **Minions.** No enemy is ever placed directly in a scene — every one is script-spawned
  from an invisible marker. `waypointName` is a **PREFIX**: `wayA` means patrol `wayA0` ↔
  `wayA1`, holding `holdTime` MILLISECONDS at each. A lone `wayX0` with a sentinel hold of
  9999999 is how the designers authored a stationary guard, so one resolved waypoint means
  "stand still", not "data missing" — clamped rather than taken literally.
  Minions carry `breakable`, `burnable` AND `teleportable`: they are puzzle objects as much
  as threats, crushable or portallable onto `enemy_killer`, the invisible kill floor under
  every world-3 level. Line of sight is a raycast, standing in for the original's
  `anti_sight_wall.ent` occluder bars.
- **Hinges** — seesaws, spinning platforms, motor seesaws. `PinJoint2D` with angular
  limits.

**Broke / dead ends:**
- **`<Joints>` is nested inside `<Collision>`**, alongside `<Polygon>` and `<Compound>` —
  and I queried it as a direct child of `<Entity>` and got zero. **Exactly the same nesting
  trap `<Polygon>` set two sessions ago**, and I walked into it again despite having written
  the warning myself. Worth stating the general rule rather than the instance: in this
  format, *every ENML payload lives under the collision root*. There is no counter-example.
- **`Actors.attach` was never called.** A string replacement did not match and I did not
  check, so keys, keyholes and spawners all reported 0 while `locked_door` reported 41 —
  the role table was fine, the attach pass simply was not running for it. Caught because
  0 next to a measured 39 is obviously wrong. Patching source by string-replace keeps
  failing silently; the edit tools error instead, which is the whole point of using them.

**Angles are RADIANS, now settled from data.** The shipped `±1.57075` is `±π/2` — the
format notes had left the units open by inference; the corpus closes it.

**Assets check:** yes — only hand-written source visible; `reference/`, `out/` and
`game/assets/**` invisible.

**[CLIP] 15:11** — a motor seesaw pivoting, and a minion patrolling between two waypoints.
**[BROLL]** — None; still headless.

**Numbers:** gates levels 128/128, core 57/57, gameplay **546/546**, converter 52 tests,
ruff clean, main 0 errors. Still zero converter skips across 4065 instances.

**Inferred mood:** guess — mildly embarrassed by the joint-nesting repeat. The measured
counts are doing real work now: every one of the five new numbers was checked against the
corpus before being asserted, and two bugs surfaced purely because a count looked wrong
rather than because anything threw.

### 2026-08-05 16:40 — Session 11: bosses; the behaviour layer is complete

**Built:** boss spawns, and with them every entity family the corpus contains. Exactly one
boss per chapter, always level 32 — `adder` carrying `entity: beholder.ent` (1-32),
`dragon.ent` (2-32), `ghost_pos` (3-32), `dark_dragon_spawn` (4-32), plus
`dragon_knight_spawn` sharing 2-32. Five markers, and the test asserts that count.

**Deliberately only the STRUCTURE.** That a boss exists in each level 32, is killable, and
ends the level is supported by data and by achievement text ("Killing the dragon", "Defeat
the dragon and finish level with 12 portals or less"). The **attack patterns are not**:
each boss is a bespoke AngelScript class whose logic sits in bytecode we can name but not
read — `beholder_spike`, `dragon_claw`, `enemy_shoot`, `ghost_appear_effect` are
identifiers, not decoded behaviour. Inventing four boss fights would be exactly the
plausible-but-wrong content §3 warns against, so the code says placeholder and the doc
says placeholder.

**Documented.** `docs/original-gameplay.md` now carries a §6 covering the whole behaviour
layer with per-claim provenance: the `ETHCallback_` test, the `stride` disambiguation rule,
the destruction economy, switches/keys/locks, minions, hinges, level properties and bosses.
Also corrected a claim that had gone stale in it — §1 still said "the remake currently
implements a jump", which stopped being true two sessions ago.

**Final state of the behaviour layer**, all counts measured against the corpus before being
asserted: 29 movers · 46 switches · 46 doors · 27 demolishers · 31 explosives · 65 burnable
· 28 turrets · 22 fire sources · 21 shock fields · 10 gravity wells · 3 launchers · 39 keys
· 41 keyholes · 41 locked doors · 62 minion spawners · 10 hinges · 5 bosses.

**Assets check:** yes — full pipeline re-run from clean; only hand-written source visible
to git.

**[CLIP] 16:31** — the chapter-1 beholder spawning in 1-32. **[BROLL]** — None; headless.

**Numbers:** levels 128/128, core 57/57, gameplay **547/547**, converter 52 tests, ruff
clean, main 0 errors. 4065 instances, 0 converter skips.

**Inferred mood:** guess — satisfied, with one reservation I want on the record. The
behaviour layer is complete in the sense that every entity family in the data now has a
component, and every count is pinned to the corpus. It is NOT complete in the sense of
matching the original: the bosses are placeholders, and every timing and speed value in
the game is still a marked guess. Those are different claims and I would rather say so
plainly than let "finished" blur them.

### 2026-08-05 19:10 — Session 12: the bytecode decoder — 54x further, still short

**Built:** a real AngelScript module decoder. `tools/asbc/` now ports `asCReader` far
enough to read **13,028 of 523,826 bytes** — up from 239 — covering the header, all 3
enums, all **132** class declarations, 5 funcdefs, 9 interfaces, and **6 complete classes
with every method's signature and decoded bytecode**. `opcodes.py` carries the full
256-entry instruction table, generated from the engine's own header.

**It still does not reach `scriptGlobals`.** It desynchronises inside class `Button`'s
method bytecode, so the constants — `FIRST_PORTAL_MIN_TIME`, `NEXT_PORTAL_MIN_TIME`,
`g_portalCollisionRadius` — remain unextracted, and every timing value in the game is
still a marked guess. Saying that plainly matters more than the distance covered.

**Four real bugs, each of which had produced a confident-looking wrong parse:**
1. **`ttIdentifier` is 5, not 14.** It is an unnumbered C enum (`as_tokendef.h:46-58`), so
   its value is its ordinal. I had guessed from the token's position in the *documentation*
   rather than counting the enum. This misparses every object-typed parameter.
2. **`asFUNC_SCRIPT` is 1, not 2.**
3. **Every function reads a trailing script-section name** when debug info is present —
   not only script functions. Missing it desyncs on the first funcdef.
4. **Class phase 2 skips the destructor and constructors entirely for interfaces**
   (`if(!IsInterface() && flags != TYPEDEF && flags != ENUM)`), and the constructor loop
   reads function **pairs** — a constructor and its factory — not one each.

**Two findings worth keeping regardless of whether the decode ever finishes:**
- **All raw 4-byte fields in this file are big-endian**, which is not what the engine's
  `ReadData` memcpy implies. Confirmed twice over: the first type's flags are `10 00 00 00`,
  which equals `asOBJ_ENUM` (`0x10000000`) only big-endian, and `DIRECTION`'s members then
  come out `0,1,2,3` rather than `0, 16777216, …`. Either alone would be a coincidence;
  both together are not.
- **It is not the pointer width.** 32- and 64-bit runs fail at the *identical* byte, which
  they could not do if a pointer-sized instruction had already been misread.

**Broke / dead ends:**
- **I hand-wrote the 256-entry opcode table from memory** before deciding to generate it
  from the header. The generated version disagreed — `CpyVtoG4` is `rW_PTR_ARG`, not
  `wW_PTR_ARG`. Writing a lookup table from recall in a project whose entire method is
  "read the source" was the wrong instinct, and it would have desynced somewhere subtle.

**Assets check:** yes — only hand-written source; gitignored dirs invisible.

**[CLIP] —** None. **[BROLL]** — None.

**Numbers:** decoder 239 → **13,028 bytes** (2.5% of the file), 132 classes named, 4 bugs
fixed. Game gates unchanged and green: levels 128/128, core 57/57, gameplay 547/547,
converter 52 tests, ruff clean.

**Inferred mood:** guess — genuinely torn. The decoder is 54x further along and every step
was earned from the C++ rather than guessed, which is the right way to be stuck. But the
thing it was built to produce — actual numbers — is still not produced, and I would rather
say "13,028 of 523,826, stopped in `Button`" than let the distance travelled imply the
destination was reached.

### 2026-08-05 21:05 — Session 13: closing the coverage gap to zero

**Asked "is anything left?" and answered it from the data**, not from memory: cross-referenced
every entity PLACED in the 128 levels against the `ETHCallback_` liveness list and against
the role table. **29 placed entities had a live callback and no handling.** Now zero.

Real gameplay gaps closed:
- **`antiportal.ent` was simply missing from the role list** — 7 placements that did not
  block portal placement at all. **`anti_portal_agent`** (21) is the same field but MOBILE:
  the 5 inline ones carry `direction`/`speed`/`stride` and now patrol, so the forbidden
  region moves.
- **`torch`** (10) — chapter 4's `darkest` levels, lit by proximity. Those are the levels
  the "Alone in the dark" achievements name, and without a light source they are unplayable.
- **`pickup`** (13) — shock/fire diamonds. Distinct from crystals: `shock_diamond_pick.mp3`
  is its own cue and the callbacks are separate. Deliberately grants nothing — what
  collecting one DOES is not established, so it collects and announces, and that is all.
- **`projectile_blocker`** (4) — invisible 64×128 barrier, `shape=0`, no sprite.

**Broke / dead ends — and the test caught both:**
1. **I implemented `bounce` as a trampoline and it threw the player out of level 4-7.**
   Wrong. The identifier set settles it: `m_bounceScaleA`, `m_bounceScaleB`,
   `m_bounceStride`, `m_currentBounceScale`, `enableBounceEffects`, plus
   `highlightButtonBounceMin/Max` and `setCreditButtonBounceEffect` — every one is about
   SCALE, and two are about menu buttons. There is `ETHCallback_bounce` but **no**
   `ETHBeginContactCallback_bounce`, the identical evidence that showed barrel bombs are
   not contact-triggered. It is a cosmetic pulse; the entity was already solid geometry and
   works as a platform without help. I invented a mechanic and a level failure found it.
2. **A name in two role lists silently resolved to one.** `no_portal_sign.ent` was listed
   under both `no_portal_zone` and `scenery_hint`, so three portal zones vanished and
   nothing errored — the count came out 93 against a measured 96. Resolved in favour of
   `scenery_hint` (a sign is signage; the field is the `antiportal`), and `LevelRuntime`
   now **warns** on any duplicate rather than quietly picking the last one.

**Also classified the final 7** — `arrows`, `gutter_mouth`, `hand_waypoint0`, `dragon_claw`,
`rocket_launch`/`_landing`, `red_key_state` — as `scenery_fx` with a note on each, so the
sweep reports zero *unexplained* entities rather than a list someone has to re-triage.

**Assets check:** yes — only hand-written source; gitignored dirs invisible.

**[CLIP] 20:58** — a mobile antiportal patrolling, so a placement that was legal a moment
ago is refused. **[BROLL]** — None.

**Numbers:** coverage gap 29 → **0**. Gates: levels 128/128, core 57/57, gameplay
**552/552**, converter 52 tests, ruff clean, main 0 errors, 0 duplicate-role warnings.

**Inferred mood:** guess — the right way to end a build phase. Two of the three things found
were mine, both were found by a test rather than by review, and the `bounce` one is the
clearest example in the whole project of the rule that keeps paying: when the data does not
say a mechanic exists, do not add it. I had a plausible story (a 64×64 solid block called
"bounce") and the identifiers said "no, it pulses".

### 2026-09-14 11:47 — Session 14: the original runs; parity rig

**Built:** the original Magic Portals running in an Android emulator, driven and screenshotted over
adb (`tools/parity/emu.py`), plus a remake capture mode (`--parity-level/--parity-out`,
`game/scripts/dev/parity_capture.gd`). Every one of the 128 original levels is reachable. Full
setup in `docs/parity.md`. Ivan's goal: side-by-side every screen and level, and close the gaps.

**Broke / dead ends:** four walls in a row, each diagnosed rather than guessed.
1. Only one of 8 installed images can run it — the APK is `armeabi` only, and every x86_64 image
   has an empty `abilist32`. The existing API 37 AVD would have failed outright.
2. `INSTALL_PARSE_FAILED_NO_CERTIFICATES`: the "public archive" had all 128 `.angelscript`
   sources stripped after signing (3251 manifest entries, 128 missing). Re-signed a local copy.
3. Crash in `eglCreateContext`. Read `ConfigChooser.java:48-66`: exact-RGB565-or-null, and null is
   passed straight on. The host GPU path offers no 565 config; `-gpu swiftshader_indirect` does.
4. `adb shell input tap` did nothing — shorter than one frame at this speed. Held swipes work.
Also a wasted detour: rebuilt the APK debuggable with apktool to reach app-private saves, then
found progress is not there at all — it is in `/sdcard/.ethanon/`, which the shell can write.

**Decision:** traverse levels by writing `scores.enml`, not by skip buttons or play-through — it is
deterministic and reaches all 128 in one push. Ivan's rulings this session: proceed although
`docs/permission.md` is still a stub (local-only, captures never leave `out/`), license-patching a
local APK is allowed, dead store/social buttons are omitted. Commits go out under his name only.

**Measured, and it overturns an inference:** the original's visible world is **256 units tall**, not
~512. Floor mortar pitch 181/177/182/180/180 px against 64-unit stones → 2.8125 = 720/256. And
medals are NOT cosmetic: all-bronze leaves chapter 2 locked, "requires 60%". Both recorded in the
new 2026-09-14 block of `docs/ethanon-formats.md`.

**First side-by-side (1-1):** remake is ~2.8x zoomed out, camera centred instead of clamped with
the player far left, unlit (original has per-pixel normal-mapped torchlight — the log loads `*_nm.png`),
no torch particles, portal art wrong, placeholder HUD, no "Part 1" caption, no pause or
level-finished screens, 1x textures where the original draws the `hd/` tier.

**Assets check:** yes — `git status --short | grep -E "reference/|out/|game/assets/"` returned
nothing. Captures, saves, rebuilt APKs and the keystore are all under gitignored paths.

**[CLIP] 12:06** — the original's level 1-1 finished by holding the right pad, "level finished" gold.
**[BROLL]** — None.

**Numbers:** gates: import 0, quit-after 0, levels 128/128, core 57/57, gameplay 552/552.
`verify_gameplay` prints 40× `SCRIPT ERROR: Invalid call. Nonexistent 'String' constructor` —
**pre-existing**: identical count on a clean stash of HEAD, and it still reports 552 passed.

**Inferred mood:** guess — a long unblocking grind that paid out completely: every wall had a
readable cause, and the scale measurement alone retires a question three sessions left open.

### 2026-09-14 17:40 — Session 14b: the target moves to Supersonic; lighting decoded to 1/255

**Built:** the verified reference library of the original (all 128 levels, every menu, save states
fresh/bronze/gold), and the converter now emits the original's lighting as `metadata/eth_*` keys plus
`--copy-lighting` (lightmaps, normal maps, halo bitmaps). Staged only in `out/staging_lighting`; `out/levels`
is untouched until the Supersonic port can read the keys.

**Decision — the game being compared changed.** Mid-session Ivan clarified that the remake now lives in
his own engine, `Desktop/Supersonic-Engine/games/magicportals`, a C++ port that reads this repo's
converter output and `game/data`. The Godot runtime is no longer the target; this repo stays as the port's
data source. A HUD rebuild aimed at Godot's `hud.gd` was stopped before it edited anything and re-aimed at
the port. Captures of the port are bit-identical run to run under `--fixed-step`.

**Broke / dead ends:**
1. The first library capture slept fixed intervals and lost **47 of 128** levels to menus and the splash
   when the machine was busy. Rebuilt on screen markers (normalised correlation; a pixel difference read
   torch-lit 1-2 as "not a level" because the HUD buttons are translucent).
2. I wrote this morning that the original draws `hd/` at 720 px. Wrong: that was the engine default; the
   game's `app.enml` puts fullhd at 600 px. Corrected and pushed the same day (f654ff2).
3. The converter's first pass under-counted `eth_z` by 204 and `eth_emissive` by 6, and its docs said the
   player's z is 0 everywhere — 2-09 has it at 2. Both caught by an adversarial reviewer, both fixed.

**Measured:** a numerical fit of the original's pixels settles the lighting model to about 1/255 per block:
`min(1, ambient + emissive) * texture + lightmap`, added on encoded (not linear) values; `lightIntensity`
is already baked into `add<id>.png`; there is no "unlit = full bright" rule; the moving player IS lit by
the torch per pixel; halos are additive in world units. Linear-light blending scores 13.59 against the
model's 0.28. The port today draws no lighting at all and tone-maps every sprite (whites cap near 186).

**Assets check:** yes — `git status --short` shows only tools/converter, docs, README, DEVLOG. Captures,
lightmaps and staged output are under `out/`.

**[CLIP] 17:10** — the library contact sheet: 128 original levels, chapter 4's space and pitch-black sets.
**[BROLL]** — None.

**Numbers:** converter 80 tests, ruff clean. Emitted over 128 levels: 730 lightmaps, 72 lights (68 static
owners), 71 halos, 1,720 normal maps, 2,140 emissive, 1,533 z — every figure equal to the design's census;
11,234 metadata lines added, every other byte of every `.tscn` identical. Library: 128/128 levels, 26
screens. Port suites: 34/34 against the data changed today.

**Inferred mood:** guess — a day that changed direction twice and still ended with the hardest unknown
(how the original lights a pixel) answered by numbers rather than by reading.

### 2026-09-15 21:29 — Session 14c: lighting and menu screens in the port (written 2026-09-16)

*Written the next day from git, the workflow journals and the files on disk: the session ended
before its own SESSION END procedure ran.*

**Built (Supersonic port, pushed):** lighting steps G3 ambient + emissive (e5cb31d), E2 the engine's
overlay map, 2D sprite record and premultiplied blend (0656311, 6d55f68), G4 lightmaps drawn (827ea6c).
Level 1-01 compare.py mean_abs against the original: 29.7 before any lighting step → 15.86 after G4; 1-09 8.21.
**Built (committed, not pushed that day):** E3, the engine's 2D point light with a height and the
shader loop that adds it (1aa34b9, e1c64f3) — every existing frame byte-identical, nothing places a light.
**Built (branch `ui2-screens`, a separate git worktree so UI work could run beside lighting):** the
pause over a level, level finished + game over, the tutorial/help popups, overlay quads that rotate
(engine), and the loading screen + main menu (fa2c17f, 84b61ab, 04ff978, 6fb5f15, b541898).

**Broke / dead ends:**
1. G3 was verified three times and failed each time on the *gate definitions*, not the code: a
   placeholder fireball box under one frame's blocks, a "tier moves detail not the mean" premise the
   numbers refuted (+3.3 on 1-09 LM_E=1), and a torch check that belonged to G4. The design owner amended
   the gates (design_port.md §10) instead of dimming the placeholder to pass.
2. Windows Smart App Control refused freshly linked test exes in every build — 28, 7, 2, 1 suites in one
   round. Each needs delete + relink; it cost minutes per build all day.
3. Two port captures of level0a exited 1 with no PNG and no message during E3's gates; six reruns were
   identical and clean. Cause unknown.
4. **The session died mid-step.** The machine went idle ~22:57, the agents woke at 03:04 and the process
   ended: G5 (lights placed, halos) was uncommitted with 23/128 levels swept, the credits + achievements
   step uncommitted and its suite never compiled. The G5 sweep's background shell then failed with
   `fork: Resource temporarily unavailable` at 03:32.

**Decision:** menus follow the original exactly (rulings R1–R4: two chapters per page, back quits,
locking from the save, the Asantee loading screen). Achievement titles/descriptions are Asantee content:
extracted by a committed tool into gitignored `out/data/achievements.json`, read at run time.
At 21:28 Ivan asked for no new agents (usage credits); the backlog went to `out/parity/BACKLOG.md`.

**Assets check:** checked the next day — `git status --short` in this repo showed only the untracked
`tools/asbc/achievements.py` (tool source, no achievement text: grep for the titles returns 0).

**[CLIP] —** None recorded.
**[BROLL]** — None.

**Numbers:** Supersonic ctest 113 → 114 suites (test_light2d). 1-01 mean_abs 15.86 after G4. E3's
predicted-pixel scene: 30,256 pixels, none more than 1/255 off. 128/128 levels byte-identical across E3.

**Inferred mood:** guess — productive but brittle: two tracks landed a lot of verified work, and then
the night ate both in-flight steps.

### 2026-09-16 11:30 — Session 15: recover the dead session's work, merge, push

**Built:** nothing new — finished what session 14c left half-done. Supersonic: G5 lights + halos committed
(e68a386), credits + achievements dashboard + locking committed on the branch (bd03f70), the `ui2-screens` branch
merged into main (e6a3cca) and pushed with E3; worktree and branch removed. This repo: `tools/asbc/achievements.py`
committed, backlog committed as `docs/parity-backlog.md`. No new agents — Ivan's standing instruction (credits).

**Broke / dead ends:**
1. The dead session's G5 sweep had reached 23/128; rerun in full on the same source: 128/128 exit 0.
2. The credits/achievements suite had never compiled: one check of its own making was wrong (it asked a fling to peak
   above the 48 u it was released at; the decoded decay → move → band order lands the next tick at 41.16), three
   double comparisons went through the harness's float `nearly` and MSVC warned. Fixed in the test, not the code.
3. Merge: 5 conflicted files, and 11 failing checks in main's lighting cases — not conflicts: since the branch, the back
   key in a level opens the pause and 1-02 raises a tutorial popup, so "back to the grid" and "tap on level1" no longer
   did what the tests assumed. Added `OutToTheGrid` (popup, back, pause's Levels) instead of weakening the checks.
4. Smart App Control refused 32 → 11 → 3 → 2 → 1 suites after the merge build, then `test_mp_layer.exe` five relinks
   in a row ("An Application Control policy has blocked this file"); the sixth link ran.

**Decision:** G5 committed with its misses recorded rather than tuned: 1-09's halo class 6.17 vs ≤ 6.0 and the orange
excess beside the torch are halo-magnitude questions the backlog already held, and tuning them is new work. Steps
50-54 renumbered in the planning doc; commit bodies keep the branch's numbers.

**Assets check:** yes — `git status --short` here before committing listed only `DEVLOG.md`,
`docs/parity-backlog.md` and `tools/asbc/achievements.py`; `grep -ciE "badge|enter the portal|apprentice|cleared"` on
the tool returns 0; `out/data/achievements.json` is ignored by `.gitignore:66`.

**[CLIP] —** None.
**[BROLL]** — None.

**Numbers:** Supersonic ctest 118/118 on the merged build (test_mp_layer 765 checks, test_mp_info 389). G5: 1-01
mean_abs 15.86 → 12.72, torch pass R scale 0.969. Credits vs original 6.79, dashboard 2.89. Merged build byte-identical
to G5 on 18 gate frames + MainScene + Wolf Brigade, and to the branch on 4 menu frames.

**Inferred mood:** guess — tidy-up with a tight budget: no discoveries, but nothing is at risk any more.

### 2026-09-16 15:30 — Session 15b: the last lighting step, chapter select and the level grid

**Built (Supersonic, pushed):** step 55 G6 — the 5/6/5 surface and the runtime bake a lit torch starts, with
`light_from_projectile.ent`'s light and halo (ad1e97c); step 56 — chapter select and the level grid rebuilt as the
original pages them, `sim/Selector` over `sim/Locking` (87886c6). No agents (Ivan's standing instruction).

**Broke / dead ends:**
1. Ivan was being flooded with "C:\Strawberry\c\bin\ctest.exe has been blocked" notifications: Smart App Control
   refuses freshly linked test exes and every refusal notifies. My relink-and-retry ctest loops fired dozens (37 in
   one round). Stopped them; now only the touched suites run, once. Ivan chose to turn SAC off; Windows still
   reported it on afterwards, and 4 untouched suites stayed refused in the last ctest.
2. MagicPortals.exe was refused four relinks running (rc 126), then ran. A probe with rc 127 was my own shell
   quoting, not a block.
3. First lighting-rules path used the remake's data dir, not the port's: the quantise never applied until the
   layer test caught it.
4. The torch test first used 4-25 (level25c), where the shot hits a platform before the torch; 4-22 works.
5. The G6 sweep showed one new diagnostic on 2-32 (a player death, then a stone breaking under the lost screen).
   Traced to the UI merge (84b61ab added the death log and the lost screen over a running level), not to G6.
6. `requires` as a member name — a C++20 keyword — caught before compiling.

**Decision:** G6 and the selector were in the same working tree; G6 was committed alone by staging its hunks from
a classified `git diff -U0` patch (`git apply --cached --unidiff-zero`), so each step is its own commit.

**Assets check:** yes — `git status --short` here lists only DEVLOG.md and docs/parity-backlog.md; captures stay
in `out/parity/ui3/select/` and `out/parity/specs/lighting/work/g6/`.

**[CLIP] —** None.
**[BROLL]** — None.

**Numbers:** G6: 2-05 port vs engine 2.14 / vs engine_no565 2.32; 4-22 noLM_E<1 vs original 0.95 → 0.13; 128/128
levels. Selector vs original stills: 1.72, 2.08, 1.93, 2.55, 2.86 mean_abs, edge IoU 0.82-0.89. test_mp_select 112,
test_mp_layer 783, test_mp_lighting 495. ctest 115/119 (4 refused, untouched).

**Inferred mood:** guess — steady progress under a tight budget, with the notification flood the one real irritant.

### 2026-09-16 15:40 — Session 16: sky, particles, the visuals briefs, batches 0-4 started; the battery ran out (written 2026-09-17)

*Written the next day from git, the planning doc, the workflow scripts, journals and role reports, and the files on disk:
at 02:43 the laptop's battery ran out and the session ended before its own SESSION END procedure. Agents were back on
(workflows), after 15 and 15b ran without them.*

**Built (Supersonic, committed locally, 6 commits):** step 57 `29448a8` (16:04, the only engine commit): `--screenshot-every N`
writes every Nth frame of one capture run beside the screenshot. Step 58 `e891731` (19:38) + docs-only `1d2468c` (23:39): the
sky the original pins to the camera (`StaticSky` → `sim/Sky`, `data/sky.json`; SpaceSky for the 19 space levels recorded, not
ported). Step 59 `c9f500b` (23:43): particles at their entity's node and angle, in Ethanon's random-number order, added whole,
square, following moving owners (`sim/Particles`), and static portals scaled 0.8 and tinted as `ETHCallback_portal_static`
does. Step 60 `12a10f3` (01:02): a JPEG's size read from its frame header, so 2-32's dragon builds its `explosion.JPG` fire.
Step 61 `823e3d7` (01:58): `sim/Tiers`, the hd/fullhd file the original picks, sized in whole units, one sheet-cut rule;
nothing draws through it yet. SE origin/main stood at `e891731` when track B's worktree was set up just before midnight;
`1d2468c` to `823e3d7` were local only when the power went.
**Built (this repo):** `0e6daec`, step 59's two recorded deviations as one open item in `docs/parity-backlog.md` §2.
**Built (gitignored, `out/parity/`):** five system briefs and `visuals/briefs/00_order.md` (1,541 lines, batches 0-13), from a
read-only workflow run beside the building one (20:12, 70 min); decodes D1-D8 and D10, each checked by a second reader,
appended as `00_order.md` §8 (23:32, 62 min); footage of the original on the emulator for F12, F13, L1 and F3, with notes.
**In flight at 02:43:** 1.1 (player facing, stride, column carry-over) built and measured in SE main, uncommitted, not yet
written up as a planning step (`out/parity/visuals/1.1/`, its sweep result at 02:42); 3.1 (the timer dial) uncommitted as
Step 80 in the worktree `Supersonic-Engine-vis2` (branch `visuals-b`), its third verifier mid-run (last file 02:38); F2
(1-03, a portal pair) recorded at 02:17, its files copied out at 02:43, `F2.md` never written; the footage `INDEX.md` never
written; the merge-and-push phase never reached. Session 17 found this state, pushed SE main through step 61 and resumed.

**Broke / dead ends:**
1. **Smart App Control, all night**, still in enforce mode (`VerifiedAndReputablePolicyState` = 1, step 58's record). The step
   records and role reports count at least 101 refused launches, each a notification on Ivan's screen: step 57's ctest 26;
   step 58's ctest 26 plus four links of `MagicPortals.exe` in a row; step 59 14; step 60 10 (`test_mp_sky` refused again after
   its one relink, so not run); step 61 2; 3.1's ctest 18 plus one exe. The roles kept to one relink and one ctest each.
2. **StaticSky (58) failed verification twice for want of a frame.** r1: no capture gate had an after, and the record said
   `test_mp_layer` had not run when it had (790/0). r2: bare ground, compare, lightgate, byte-identity and the 128-level sweep
   all NOT RUN (the fourth refused link, `6b2d0d04`). The second fix round deleted the exe and relinked once; `5a435cad` ran,
   every capture was taken on it (69 min), r3 passed. Two numbers got worse and stay: 1-01 mean_abs 12.72 → 14.01 and its halo
   rings, both from the 1x sky picture's tier, not the placement (the fullhd picture pinned there scores 10.97).
3. **Particles (59) failed all three rounds, and batch 1 stopped itself at 23:28** so nothing built on it. r1: torch top 14.2 u
   on the plan's 6 frames (1.3 short; one flame tip decides it; 18.1 on 51 frames), ring excess pushed 604 out of band
   (1.22 / 1.44 / 2.08x), and two random draws inside one `glm::dvec2` whose order C++ leaves open (MSVC drew y first; putting
   them in order moved 1-01 f420 `b19ce71a` → `7138a7bc`). r2: ring excess unchanged, square quads never pinned on a
   non-square cell, step 49's halo profile worse at every band. That verifier guessed the static portals' tint was
   `portalColorA/B` (the antiportal's); the fix round found the portal's own script, `Scale(0.8)` then `SetColor`. r3: 582's
   ring excess 1.62x, and the iris core's temporal s.d. newly failing, 3.41 / 3.19, because of that 0.8 scale.
4. **3.1 (Step 80) failed two rounds.** r1: the dial began shrinking on the pickup tick, one tick before the original (whose
   frame still runs on its stale local elapsedTime); and 00_order's level23 f270 → f420 gate cannot be measured as written,
   both crystals expire on ticks 300 and 360. r2: `test_mp_layer` failed at :4456 on the binary the fix round had reported
   966/0: `DialOf` returned a pointer into the vector `TimerReports()` returns by value, six use-after-free reads, so that pass
   was undefined behaviour. Fixed to return a copy, 966/0 on `ac056aaa`. The third verifier was running when the power went.
5. **L1's put-out is impossible on 4-22.** The orbiting fire signal spawns only on a level with a `light_wall.ent`
   (`addFireSignalIfNecessary`, `all_functions.txt` line 97829); level21c has none. Lighting recorded on 4-22, lighting then
   putting out on 4-29 (4-26 had no clear shot). Every lighting take hides the moment in a ~500 ms recording gap (503 / 500 /
   536 ms). F3's first hourglass take missed: two taps in one adb call ran 1.24 s apart. The emulator records 16-29 fps, never 60.
6. **Step 58's wording corrections, twice.** Its verifier asked for four text fixes; the commit agent applied them and amended
   its commit once (`57fed4c` → `e891731`). The finish-59 role was then handed the same four: three were already in, only the
   two pivot citations still ran a line long (`ETHSpriteEntity.cpp:742-752` → `742-751`, `ETHEntity.cpp:270-297` → `270-296`),
   so `1d2468c` went in under another title than the one given. Step 61's commit agent refused a verifier's citation change
   (`:652-684`) with evidence: the closing brace is line 683.
7. **The workflow gave the main-tree roles the worktree's build script.** `tree.includes('worktree')` matched the main tree's
   own description ("Another track works in a separate worktree"), so they were told to use `vis2_build.bat`, which the
   worktree setup had just rewritten to build `Supersonic-Engine-vis2`. 0.1 and 0.2 noticed and built main through copies.
8. **00_order was wrong in places.** PRE-3's baseline, `particles/after/` (`b19ce71a`), is the superseded first build's; the
   committed build draws `a1f8d7ae` (`fix2/`), which 0.1 and 3.1 each found on their own. K16 names four entities on
   `explosion.JPG`; there are five. D3 came back NOT CONFIRMED (two supporting claims refuted, one branch missed; its four
   direct answers hold).
9. **The footage contradicted three of our records**, none edited yet. `specs/ui2/pause_finished.md` says restart and pause
   vanish in one frame; F12 and the library's `finish_1-1.mp4` show them sliding out and fading over ~0.5 s. The port's
   `data/torch.json` `signal._note` puts the signal at (-cos, sin), counter-clockwise; the footage and the bytecode read
   last-argument-first give (sin, -cos), clockwise (one clip). F3: the antiportal refuses a tap 267 px from its centre and
   accepts one at 273 px, at scale 3: a radius of 32 × scale, not the port's 16 nor `placement.json`'s 64, which misses the
   script's `Scale(0.5)` at load.

**Decision:**
- Step 59 committed with two RECORDED DEVIATIONS, not tuned, under Ivan's delegation toward what the original does: portal
  582's ring excess (106.5 against 65.6 ± 25%, 1.62x) and the iris core's temporal s.d. on 604 and 582 (3.41 / 3.19 against
  ≤ 3). Both are the rings' brightness over their life; no script lever reaches a static portal's particles, and the shipped
  `libApplication.so` rules out the blend (`SetAlphaMode` at 0x11eac0: AM_ADD is `glBlendFunc(GL_ONE, GL_ONE)`).
- Two tracks for batches 0-4: track A (0.1, 0.2, 1.1-1.3, 2.1-2.3) in SE main, track B (3.1-3.3, 4.1-4.3) in a worktree on
  `visuals-b`, its planning steps numbered from 80 so the two cannot collide; only a merge agent at the end pushes. A step
  failing three verify rounds goes to a disposition agent: commit with recorded deviations, or shelve to a stash.
- Research beside the builds, read-only: the briefs and decodes wrote only under `out/parity/visuals/briefs/`; the footage
  workflow drove the emulator and launched nothing of Supersonic.
- Rulings R1-R18 taken toward the original; step 61 records R10 (player at z 4), R12 (tier fixed at the 720 px choice) and
  R13 (route B: the port resolves tiers, `out/levels` stays byte-identical). The decodes' R19-R23 are proposed, not adopted.
- 3.1 deletes the remake's guessed crystal fade and replaces its test pin on purpose; its level23 gate is kept in the closed
  form int(t2/T·8) − int(t1/T·8), and amending 00_order is left to Ivan.

**Assets check:** yes — `git status --short` here, run after appending this entry, lists only ` M DEVLOG.md`. Everything else
this session made on this side is under `out/`: 4.1 GB of footage (8 delivered `.mp4` clips of the original plus extracted frames) and
4.0 GB of captures, briefs and decodes under `out/parity/visuals/`; `git check-ignore -v` gives `.gitignore:66:out/` for both.

**[CLIP] —** None.
**[BROLL]** — None.

**Numbers:** step 57: 14 stamped files for 420 frames at N = 30, `_f420` byte-identical to a plain run, 10.9 s against 7.9 s
(14 launches ≈ 110 s). Step 58: bare ground 3,634,104 px on 34 levels → 1,593 px on 3; the original's sky centre (640, 360)
± 1 px on 16 of 16 library levels; 2-02 mean_abs 34.16 → 9.30, 4-20 19.95 → 1.87, 1-21 50.08 → 19.37. Step 59: torch flame
centroid (288.9, 62.5) → (288.8, 46.9) u against the original's (288.0, 47.5), top 35.9 → 18.5 against 18.5; ring spin +100 →
-120 / -120 / -110 °/s against -95 to -119; ring reach 81 / 77 / 79 px against 81 / 79 / 83; 1-01 mean_abs 14.01 → 14.76, f420
md5 `a1f8d7ae`. Step 60: 1,182 original images, 0 sized otherwise than stb_image reads them; "particle image could not be
read" 1 → 0 of 128 logs; 128/128 f420 byte-identical. Step 61: 249 1x images, 5 draw at another size. Suites: test_mp_sky
361, test_mp_tiers 527, test_mp_layer 790 → 854 → 860, test_mp_sprites 336 → 372; ctest 93/119 (57, 58), 111 then 117/120
(59), 112/120 (60), 120/121 (61), 0 failed. 1.1 (uncommitted): normal f8 0.970 vs f4 0.807 on 1-01, 0.926 vs 0.796 on 1-09,
centres exactly x 101 and 396; B-onset period 21.6 frames (gate 21.6 ± 1.0); f8 wins 52 of the 54 gate levels at f420, 1-02
and 1-03 losing by 0.004 and 0.002 under their help popups and winning at 0.894 / 0.903 with the popup tapped away; 125 of
128 frames change, 122 only inside the player quad, 1-02, 2-15 and 2-28 outside it, unexplained at the crash; test_mp_sprites
392, test_mp_layer 875, test_mp_start 726. 3.1 (uncommitted): 35 dials in 20 levels, cell from the tick count on 17/17
frames, alpha 0.459-0.528, test_mp_layer 966, test_mp_timed 80. Workflows: batch 1 16 agents, 7 h 51 min, 4.20 M tokens;
briefs 6 agents, 70 min, 2.67 M; decodes 19 agents, 62 min, 4.34 M; batches 0-4 had started 15 agents and the footage
workflow 5 when the power went. Session length 11 h 12 min.

**Inferred mood:** guess — a long night on autopilot that mostly worked: five steps landed and the verifiers caught real
faults (a test that passed on freed memory), but Smart App Control notified all night and the battery left two steps and a
clip half-done, uncommitted and unlogged.
