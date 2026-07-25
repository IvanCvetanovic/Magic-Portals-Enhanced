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
