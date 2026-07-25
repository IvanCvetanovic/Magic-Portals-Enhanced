# Permission record

The authoritative record of the rights grant this project depends on. `CLAUDE.md` and
`README.md` summarise it; **this file governs.** If the two ever disagree, fix the summary.

> ## ⚠️ INCOMPLETE — PASTE THE ACTUAL GRANT HERE
>
> This file is a **stub**. The fields below have not been filled in and the grant text
> has not been pasted. Until that happens, nobody — human or agent — should assert what
> this project is or isn't allowed to do beyond the bare fact that permission was given.
>
> **Agents:** if you are reading this and this warning is still present, treat the scope
> as unestablished. Say so and stop; do not infer scope from the summaries elsewhere in
> the repo.
>
> Ivan: fill in §1, paste the message verbatim into §2, delete this block.

---

## 1. The grant — facts

| Field | Value |
|---|---|
| Rights holder (person/entity) | *TBD — who actually holds the rights to Magic Portals?* |
| Granted to | *TBD* |
| Date granted | *TBD* |
| Medium | *TBD — email / DM / forum post / signed document?* |
| Original archived at | *TBD — where does the untouched original live? See §4* |

**Scope — what was explicitly granted:**

- *TBD* (e.g. rebuild the game / reuse art / reuse audio / reuse level data / use the name)

**Scope — what was explicitly excluded or not addressed:**

- *TBD*

**Commercial terms:** *TBD — may the remake be sold? Revenue share? Free only?*

**Attribution required:** *TBD — exact wording, and where it must appear (splash? credits? store page?)*

**Trademark / naming:** *TBD — may the remake be called "Magic Portals"? May "Asantee"
appear? A rights grant over game content does not automatically cover trademarks.*

**Duration / revocability:** *TBD*

---

## 2. Grant text — verbatim

Paste the message **unedited**, including headers/timestamps/sender. Do not paraphrase.

```
(paste here)
```

---

## 3. Standing constraint (independent of the above)

Whatever §1 turns out to say, this holds unless the grant explicitly says otherwise:

**Redistribution of original assets is not permitted, and a public git repository is
redistribution.** No original asset, data file, or APK is committed. Enforced by
`/reference/` in `.gitignore`. See `CLAUDE.md` → "THE HARD RULE".

Reusing assets *inside the shipped game* is a different act from *publishing them in a
source repo*. The grant covers the first. Nothing here assumes it covers the second.

---

## 4. Where the original is archived

The grant message should exist somewhere durable and outside this repo — a saved email,
a PDF export, a screenshot with visible sender and date. A repo can be lost or rewritten;
the evidence should not live only here.

- Archive location: *TBD*
- Backed up: *TBD*

---

## 5. Third-party licences relied on

| Component | Licence | How it is used |
|---|---|---|
| [Ethanon Engine](https://github.com/asantee/ethanon) | MIT | Read-only reference for decoding file formats. Not vendored, not built, not committed. MIT would permit vendoring; we simply don't. |
| Godot 4 | MIT | The remake's engine. |

If any Ethanon code is ever *copied* into this repo (currently: none), the MIT licence
text and copyright notice must be included alongside it — that is what MIT requires.

---

## 6. Open questions to resolve with the rights holder

- [ ] Does the grant cover the **name** "Magic Portals" and any logo/wordmark?
- [ ] Commercial release, or non-commercial only?
- [ ] Required attribution wording and placement?
- [ ] Is the grant assignable / does it survive if the rights change hands?
- [ ] Is there any obligation *not* to publish the remake's source publicly?
- [ ] Are original source files (Ethanon project, art sources) available directly, which
      would remove the need to extract from the APK at all?
