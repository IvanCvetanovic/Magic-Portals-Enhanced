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
