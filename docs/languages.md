# Languages

The original game is in English only. Magic Portals Enhanced shows its interface in eleven languages, in the order of the Magic Rampage
Companion app's picker: **English** (the default and the original's), **Deutsch, Español, Français, Italiano, Português, Русский, Türkçe,
Українська, 日本語, العربية** (`en de es fr it pt ru tr uk ja ar`). This page is the design, how to add a language, where the translations came from,
and what is not translated. The record of how it was built, step by step, with its gates, is Steps 236-239 of
[the remaster plan](planning/2026-09-11-magic-portals-remaster.md); the open questions are `LANG-1` to `LANG-5` in
[parity-backlog.md](parity-backlog.md) section 7.

## What a player sees

* **Choosing.** A small globe sits on the main menu, under the gear (the Options button), and another in the top-right corner of the Options
  screen. It opens a screen of eleven plates, each the language's own name in its own script; the one in use is lit. A tap changes the words and the
  fonts at once, on that screen, and the choice is kept. The back button, or `Esc`, returns to the screen that opened it. The globe is a picture the
  game draws itself, in the same style as the gear; nothing is stored.
* **English is the default, and it is what it always was.** There is no detection of the system's language: a player who never chooses has English,
  and English is drawn as before (35 recorded frames of every menu screen and level text, kept as numbers and compared within a tolerance, hold that: `tests/golden/menu_screens.json`;
  one character of one achievement's description is the exception, see *Known limits*). The 15 frames that draw the achievements' words are kept WITHOUT their glyph quads, because a
  glyph's place on the original's font page spells the character: the committed file holds their other quads, their glyph counts, the rectangle their glyphs fill and one fingerprint of
  all their glyphs together, and the glyphs themselves are compared with the gitignored `out/data/menu_screens_text_golden.json` when it is there (below, *What is committed*).
* **Kept in `language.json`** in the same folder as the medals (`scores.json`) and the switches (`audio.json`): `{"version":1,"language":"de"}`.
  Nothing is written until the player chooses; a file that will not read is English, with the reason in the log, and is left alone.
* **`--lang <code>`** forces a language for one run and neither reads nor writes `language.json` (a capture's flag, like `--controls modern`). A choice
  made in the picker during such a run is kept in memory only. A developer's or parity run (`--fixed-step`, `--route`, `--visit-levels`) **without** `--lang` is
  forced to English the same way (`Language::ResolvePolicy`): the saved choice of whoever runs a capture or a route is never read, so a capture compared with the
  original does not change with the language the machine's player chose. Those three flags and no other: the capture helpers (`--press`, `--hold`, `--tap`, `--drag`, `--place`,
  `--burn-dragon`) do not make a run a developer's, so a capture without `--fixed-step` reads the saved language and is offered the picker, and a capture passes `--fixed-step` or `--lang`.
* **Where the picker is not offered:** a developer's run (`--fixed-step`, `--route`, `--visit-levels`) and every test's bare layer, as they have no gear:
  a capture of the original's menus is the original's menus. It does not depend on the modern controls' tables, so a run that has no Options still has it.

## How it is built

| What | Where |
|---|---|
| The eleven codes, their own names, direction, which fonts, and how a generated font is drawn | `game/data/lang/languages.json` |
| The fourteen interface strings of each language (`hud.part`, `dashboard.chapter`, `options.title`, `language.title`, ...), `{n}`, `{percent}`, `{title}` placeholders | `game/data/lang/<code>.json` (UTF-8 JSON); `en.json` is the reference every other file is held to |
| The achievements' titles and descriptions in each language, and the secret achievement's title and sentence (with `{world}` and `{level}`) as a `secret` block | `out/data/achievements.<code>.json` (**not in git**, see below) |
| The picker and its two globes: places, sizes, colours | `game/data/picker.json` |
| The four open-licence faces and their licence texts | `game/data/fonts/` (`README.md` there says where each came from) |
| The string tables, the saved choice, UTF-8, the picker's layout | `game/sim/Strings`, `Language`, `Utf8`, `Picker` |
| The fonts made at run time, and the Arabic shaper | `game/sim/FontBake`, `ArabicShaping` |

* **Text is UTF-8, in JSON only.** No non-ASCII character is written in C++ (MSVC is built without `/utf-8`). The layout decodes UTF-8 itself
  (`Hud::LayOutText`, `Hud::MeasureText`); the engine's bitmap font is byte-wise and is not patched.
* **English passes no table.** Every module that composes a string takes an optional string table; English passes none and runs the code it always ran,
  and `en.json` is tied to the English still in the rule tables by a test.
* **Fonts.** The original draws in two bitmap fonts that hold Latin-1 only (and are not ours to copy or extend). English, German, Spanish, French,
  Italian and Portuguese stay inside Latin-1 and use them. Russian, Turkish, Ukrainian, Japanese and Arabic draw **every** font slot from an atlas
  `FontBake` makes at run time into the game's scratch folder from the four faces in `game/data/fonts/` (Kurale in the place of the swash capitals,
  Noto Sans Bold in the place of the achievements' bold, Noto Sans JP and Noto Sans Arabic for their scripts): white glyphs with a baked soft shadow, the
  same line height and baseline as the originals, so no placement moves. No `.png` or `.fnt` is ever committed, and no table names a generated file.
  The picker's names are one more atlas of the same kind, baked the first time the screen opens, with every script in it, so the plates do not change
  when a language is chosen.
* **Fit to width.** A translation can be longer than the English a slot was laid out for, and nothing wraps. A slot may name a `max_width_units`;
  a longer text is scaled down to it, never up, and English is never scaled. A test holds that no shipped translation of the interface strings or the achievements needs less than
  three quarters of its slot's size; the picker's plates are small, and the longest name needs 0.71 of the plate's size (a test holds them to a half).
* **If the fonts cannot be made** (no scratch folder, a face that will not read) the game speaks English and says why, once; the choice is kept for a machine that can.

## How to add a language

1. **The code and its place.** Add it to `Strings::Lang` and `kLangCodes` (`game/sim/Strings.hpp`) and to `languages.json`'s `languages`, in the same order: the loader
   refuses a table whose order is not the enum's. Give its own name in its own script, `rtl`, and `font_set`: `original` only if every character of its strings is in
   Latin-1 (the loader checks), otherwise `generated`.
2. **The strings.** Copy `en.json` to `<code>.json` and translate the fourteen values; keep every placeholder. Keep the `_about` that says who translated it and how
   (a test requires the machine-translation statement on every file but English's, and a native reader's review is the thing to add there).
   A language set in the original's fonts must not use a capital `I` in a Matura-font string (the font draws it like a `J`).
3. **A new script.** Add the face to `game/data/fonts/` through `tools/l10n/make_fonts.py` (pinned download, subset, licence text, `NOTICE.md`), its code point ranges as a
   `script` in `languages.json`, and route it in each slot's `script_faces`. `tests/test_mp_fontbake.cpp` fails for a character a face lacks.
4. **The achievements.** Put the translation in `out/data/lang_src/<code>.json` (`achievements`, and a `secret` block `{title, description}` whose description holds
   `{world}` and `{level}` once each) and run `tools/l10n/make_achievement_languages.py` (it refuses a file whose ids, numbers or line counts differ from the English, a
   secret sentence that lacks a placeholder or holds a digit, and, for a language set in the original's fonts, a secret text outside U+0020-007E and U+00A0-00FF). The result is `out/data/achievements.<code>.json`; the release's content package ships it
   (`tools/package/closure.py` takes every `achievements.<two letters>.json`).
5. **The picker** lays out its plates from `picker.json`; a twelfth plate fills the fourth row. `tests/test_mp_picker.cpp` checks the grid at the nine window shapes
   and that every name fits its plate, and `test_mp_strings`, `test_mp_fontbake` and `test_mp_overflow` check the strings, the glyphs and the fit of the new language.

## The translations

The fourteen interface strings, the 82 achievements' titles and descriptions and the secret achievement's two texts, in each of the ten other languages, were **machine-translated by a language
model from the English, with the terms aligned with the Magic Rampage Companion app, and reviewed by a second model pass. They have NOT been read by a native speaker.**
Each file says so. Expect wrong register, awkward phrasing and the occasional mistranslation; a reviewer's corrections go straight into the JSON (`LANG-1`).

## What is not translated

* **Text baked into the original's pictures** cannot be changed without redrawing them: TAP START, *Achievements*, *Select chapter*, the *level finished* and *game over*
  screens, *portals created*, *golden score*, *current score*, *points*, the game-finished card, and the names on the credits strip. They stay English in every language
  (`LANG-2`).
* **Proper nouns:** the game's name, the developer's, the engine's, the names of the people in the credits.
* **Developer and failure text:** the start-up error dialog (it runs before any language is known), the log, `--help`, and the failure labels. These are English.
* **A card already on screen** when the language changes keeps its words until it goes.
* **No system-language default** (`LANG-3`): a first run is English by design.

## Arabic

Arabic is written right to left with letters that change shape by position. `Strings::Display` runs the shaper (`game/sim/ArabicShaping`, adapted from the maker's
Penumbra port) once, after the placeholders are filled: Presentation Forms-B joining, the lam-alef ligature, reordering by line with digits and Latin kept left to right and
brackets mirrored. Its limits (`LANG-5`): no vowel marks (harakat) are handled; the reordering is a simplified version of the Unicode algorithm, not all of it; a
digit run such as `2-14` keeps its written order; a block of several lines (the achievements' rows and popup) is still anchored at the left edge, and only 1-21's hint is
mirrored to the right edge. A native reader should check how it looks before it is called right.

## The achievements' translations are not in git

The achievements' titles and descriptions are Asantee's words, so a translation of them is derivative of text this repository does not hold. They are kept in
the gitignored `out/data/` beside the English list, made by `tools/l10n/make_achievement_languages.py` from the translators' files, and shipped only in the release's
content package (the same rule as every other piece of the original). A checkout without them shows the English achievements in any language and says so once.

**What is committed, exactly.** The **fourteen** short interface strings, in `game/data/lang/<code>.json`. **What is not:** the achievements' titles and descriptions
**and the secret achievement's title and sentence** (the words a locked secret achievement shows, with its chapter and level in them), in every language: they are the
overlay `out/data/achievements.<code>.json` (a `secret` block beside `achievements`), made from `out/data/lang_src/<code>.json`. The English of the secret achievement is
what it was before the languages: the original's, composed from `out/data/achievements.json` (`secret`), so a checkout without the converted data holds none of it either.
(An earlier, uncommitted form of this work held the secret achievement's two strings in `en.json` and in the ten language files; the review of 2026-11-08 found that they
are Asantee's words and took them out before anything was committed.)

**The golden of English is committed with the achievements' glyphs left out.** A glyph quad's uv rectangle is a character's cell on a page of the original's font, so the quads of a
frame that draws the achievements' words spell them to anyone who has the original's `.fnt` files. A first form of the golden (format 2) held them; the verification round of
2026-11-10 found it, and the commit that held it was never pushed (planning step 239, F12). `tests/golden/menu_screens.json` is now format 3: the 15 frames that draw those
words (the dashboard's 13 stops, the achievements popup and the unlock card) keep their other quads, their glyph count, the rectangle their glyphs fill and ONE fingerprint of all
their glyphs together; the glyphs themselves are in `out/data/menu_screens_text_golden.json` (gitignored), which `test_mp test_mp_langlayer --record-golden <dir>` writes with the
committed file and the suite compares when it is there (when it is not, the suite says so once and passes). The other 20 frames keep their glyph quads, which decode to digits, to
strings already in git and to percent strings and the loading dots: the original's cell geometry for this port's own characters, and no word of Asantee's. A content guard in the suite
lays every line of the achievements out through the original's fonts and fails when a frame that spells one is not flagged hidden, and when a flagged frame spells none.

Seven of the fourteen are short interface words of the original's own, already in the repository in English since before the languages (`Part N`, `Chapter N`, `new!`,
`requires N%`, `Achievement unlocked! {title}`, `This level is locked!`, `move the camera around`); **their ten translations are new in git**, and whether short
translated UI phrases count as the original's text is the owner's call (`LANG-1`). The rest (`Controls`, `Modern`, `Classic`, `Adjust controls`, `Language`, and the
two credits lines) are this port's own words.

## Known limits, and the owner's calls

* **English is as before, with one character excepted.** The description of achievement 60 holds, in the original, one byte that is not valid UTF-8 (a lone Latin-1 byte;
  `docs/ethanon-formats.md` section 24). The achievements table used to carry U+FFFD for it, which the byte-wise text drew as three Latin-1 glyphs (the three bytes of U+FFFD's
  UTF-8 form); it now carries U+00BA, one glyph. So that one description is not byte for byte what it was before the languages, and nothing else in English moved.
* **Narrow screens (5:4, 1280 x 1024) are the weak spot of the longer languages** (`LANG-6`; found by the verification round of 2026-11-10, which looked at 11 languages x 16 screens x
  2 shapes, 2340 x 1080 and 1280 x 1024). At 5:4 German, Spanish, French, Italian, Portuguese, Russian and Ukrainian overflow: the achievements' descriptions on the dashboard run past
  the bar to the edge of the screen (the fit width of 207 units assumes a bar that is not there at 5:4); the *new!* label is clipped in Spanish and Italian; the pause's title (`Niveau 1/32`,
  `Livello`, `Nivel`, `Seviye`, ...) touches or overlaps the golden-score plaque. English touches the edge once at 5:4 as well (documented). At 20:9
  everything fits. The picker's grid also assumes a screen no narrower than 5:4. This is a known limit and is not fixed.
* **A right-to-left hint can run under the right walk arrow.** 1-21's hint, mirrored to the right edge in Arabic, can pass under the right walk arrow at the larger sizes of the modern
  controls. Cosmetic, one hint (`LANG-7`).
* **After a failed picker bake the globe does nothing.** If the picker's atlas cannot be made, the globe stays drawn and stays clickable, and the tap does nothing (`LANG-7`).
* **An Arabic string that falls back to English is drawn through the Arabic display path**, so its punctuation jumps to the wrong side. Only on a fault path: a key or a file that
  did not read (`LANG-7`).
* **Wording a native reader should look at first** (noted by the verification round of 2026-11-10, whose reading was not a native speaker's; `LANG-1`): Russian *new!* (a short form, `Nov!` transliterated) and *Modern*; Japanese *new!*
  ends in an ASCII `!` where the other Japanese strings use full-width marks, and the credits line for the engine adds a word the English does not have; Italian *requires N%* has `il`
  written in (wrong for a number such as 80); French has no space before `!` and `%`; Arabic mixes the indefinite and the definite form and has a doubled *min*.
* **Packaging hardening that was not done** (`LANG-8`). `tools/package/closure.py` ships an overlay `achievements.<code>.json` only if it is there: a release made from an incomplete
  `out/data` carries English-only achievements for that language and does not say so (the release recipe, `docs/releasing.md` step 5, now says to check the ten overlays). CI runs no
  test of `tools/l10n` or `tools/asbc`. The overlays' names are "two lowercase letters" in the closure's pattern, not derived from `languages.json`.
* **The bake is synchronous on the tap.** Choosing a language that needs generated fonts (ru tr uk ja ar) bakes its two atlases on the frame of the tap
  (64-100 ms measured on the PC, from an empty scratch folder; a language already baked is read from the cache). It has **never been timed on a phone**: a slow phone may
  stall for a visible moment on the first tap of each such language.
* **Arabic multi-line blocks are left-anchored.** The dashboard's rows and the achievements popup are several lines; they are anchored at the left edge in Arabic too.
  Only 1-21's hint is mirrored (see Arabic, above).
* **`NotoSansJP-Bold.ttf` is 328,820 of the 434,432 bytes of fonts** the game ships. Most of it is the 1,026 elementary-school kanji kept so that a native speaker's edit
  has room; dropping grades 5-6 would save about 100 KB. That is the owner's call.
* **The hard rule's mechanical check was widened, on purpose.** `tools/ci/check_public.py` (rules H1 and H2) refuses every `.ttf` in the repository except a
  `game/data/fonts/<Family>-<Style>.ttf` of at most 400,000 bytes that has `OFL-<Family>.txt` beside it. That check **cannot tell an OFL font from a Windows font or
  an Asantee one**: it goes by the name. It is a deliberate widening of the check that guards the hard rule (no extracted asset and no Asantee-origin file is ever
  committed), and **the owner must accept it**. `tools/ci/test_fonts_pinned.py` closes the hole from the other side: every font file under `game/data/fonts` must be one
  the README's table lists, with that size and that sha256, so a swapped or an added face fails the tools' tests; it cannot say that the listed source is what the
  file is (that is `make_fonts.py`'s pinned downloads, and a reviewer reading the README in a diff).

## Checking it

`test_mp_strings` (the files and their keys and placeholders), `test_mp_utf8`, `test_mp_language` (the saved choice), `test_mp_fontbake` and `test_mp_arabic`, `test_mp_langsim` and
`test_mp_langlayer` (the layer in each language, and the golden of English), `test_mp_overflow` (every translation against its box), and for the picker `test_mp_picker` and
`test_mp_pickerlayer`. `tools/package/test_package.py` and `test_android_package.py` hold that the tables, the faces and the translations travel, and
`tests/test_mp_unpack.cpp` that the Android unpack accepts their paths. `tools/l10n/test_make_achievement_languages.py` holds the overlay's maker (the secret block's
placeholders and character set among its checks), `tools/ci/test_fonts_pinned.py` the pinned faces, and `tools/asbc/test_asbc.py` the decoder of the original's strings.
