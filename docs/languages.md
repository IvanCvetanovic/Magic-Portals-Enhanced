# Languages

The original game is in English only. Magic Portals Enhanced shows its interface in eleven languages, in the order of the Magic Rampage
Companion app's picker: **English** (the original's, and what a system that speaks none of the others gets), **Deutsch, Español, Français, Italiano,
Português, Русский, Türkçe, Українська, 日本語, العربية** (`en de es fr it pt ru tr uk ja ar`). A first run speaks the system's language when it is one of
the eleven (below); the globe on the main menu changes it. This page is the design, how to add a language, where the translations came from,
and what is not translated. The record of how it was built, step by step, with its gates, is Steps 236-239, 245 and 247 of
[the remaster plan](planning/2026-09-11-magic-portals-remaster.md); the open questions are `LANG-1` to `LANG-8` in
[parity-backlog.md](parity-backlog.md) section 7.

## What a player sees

* **Choosing.** A small globe sits on the main menu, under the gear (the Options button), and another in the top-right corner of the Options
  screen. It opens a screen of eleven plates, each the language's own name in its own script; the one in use is lit. A tap changes the words and the
  fonts at once, on that screen, and the choice is kept. The back button, or `Esc`, returns to the screen that opened it. The globe is a picture the
  game draws itself, in the same style as the gear; nothing is stored.
* **A first run speaks the system's language when the game has it, and follows it until the player picks** (`LANG-3`, planning step 245). A player's run
  that has no `language.json` and no `--lang` asks the system which language it speaks and, if that is one of the eleven, speaks it. Any other language
  (Chinese, Hebrew, Polish, Korean ...), no answer, and a system that says `C` or `POSIX` give **English, as it always was.** **Nothing is written for the system's
  language**: the store holds no choice, so a phone whose language changes changes the game's with it, and the first pick in the picker writes `language.json`
  and ends that. A saved choice always wins, a saved English included (a player who picked English on a Portuguese phone has English, on every later run). Picking
  the language the system already gave is a choice too and is written, so it outlives a change of the system's. A `language.json` that will not read is English, as
  before, and **not** the system's: the file is there and is left alone, with its reason in the log. The log says what the system answered and what became of it
  (`system language: 'pt_BR.UTF-8' -> pt`, then `language: pt (the system's language ...)`).
* **Where the system's language is read** (the text is turned into one of the eleven codes by `Language::CodeForLocale`, which makes a pure function of every spelling the
  platforms use: the language subtag before the first of `_ - . @`, compared whole and ignoring case, so `pt_BR`, `pt-PT`, `pt_BR.UTF-8`, `PT_br` and a bare `pt` are
  Portuguese, `ukr` and `zh_CN` are none; the region is not read, so Portuguese from Portugal gets the one Portuguese there is, `LANG-4`): **Windows** the first of the
  user's preferred UI languages (`GetUserPreferredUILanguages`), the regional-format locale only when that list cannot be had; **Android** `AConfiguration_getLanguage`
  in `game/android/AndroidMain.cpp`, handed to the game as a locale text through `MagicPortalsSetSystemLocale` and **never as `--lang`** (a forced run neither reads nor
  writes `language.json`, so it would hide every saved choice); **macOS and iOS** `[NSLocale preferredLanguages]`, first entry, in `MacMain.mm` and `IOSMain.mm` (Foundation,
  which both already import; **written, never compiled**); **Linux and the other POSIX systems** `LC_ALL`, then `LC_MESSAGES`, then `LANG`: the first that is set and not
  empty decides, even when it says `C` (a player who set `LC_ALL=C` did not ask for their `LANG`'s language); `LANGUAGE` is not read.
* **Only a player's run follows the system.** `Language::ApplySystemLocale` gives a default to an attended run (`RunKind.hpp`'s `AttendedRun`: no `--frames`, `--screenshot`,
  `--record`, `--replay`, `--hidden`, `--saves`, script flag or `--light-masks-off`) with no forced language. `--lang`, `--fixed-step`, `--route` and `--visit-levels` (English,
  forced) and every capture and script get none, whatever the system says, and the system is not even asked: a capture or a route is the same pictures on every machine, and the
  suites (whose bare `Paths` carry no default) do not depend on the machine's language.
* **English is drawn as before** (35 recorded frames of every menu screen and level text, kept as numbers and compared within a tolerance, hold that: `tests/golden/menu_screens.json`;
  one character of one achievement's description is the exception, see *Known limits*). The 15 frames that draw the achievements' words are kept WITHOUT their glyph quads, because a
  glyph's place on the original's font page spells the character: the committed file holds their other quads, their glyph counts, the rectangle their glyphs fill and one fingerprint of
  all their glyphs together, and the glyphs themselves are compared with the gitignored `out/data/menu_screens_text_golden.json` when it is there (below, *What is committed*).
* **Kept in `language.json`** in the same folder as the medals (`scores.json`) and the switches (`audio.json`): `{"version":1,"language":"de"}`.
  Nothing is written until the player chooses (the system's language is not a choice); a file that will not read is English, with the reason in the log, and is left alone.
* **`--lang <code>`** forces a language for one run and neither reads nor writes `language.json` (a capture's flag, like `--controls modern`). A choice
  made in the picker during such a run is kept in memory only. A developer's or parity run (`--fixed-step`, `--route`, `--visit-levels`) **without** `--lang` is
  forced to English the same way (`Language::ResolvePolicy`): the saved choice of whoever runs a capture or a route is never read, so a capture compared with the
  original does not change with the language the machine's player chose. Those three flags and no other: the capture helpers (`--press`, `--hold`, `--tap`, `--drag`, `--place`,
  `--burn-dragon`) do not make a run a developer's, so a capture without `--fixed-step` reads the saved language and is offered the picker, and a capture passes `--fixed-step` or `--lang`.
  Such a capture does not take the SYSTEM's language, though (it is not an attended run, above): only a language somebody saved.
* **Where the picker is not offered:** a developer's run (`--fixed-step`, `--route`, `--visit-levels`) and every test's bare layer, as they have no gear:
  a capture of the original's menus is the original's menus. It does not depend on the modern controls' tables, so a run that has no Options still has it.

## How it is built

| What | Where |
|---|---|
| The eleven codes, their own names, direction, which fonts, and how a generated font is drawn | `game/data/lang/languages.json` |
| The twenty-one short strings of each language: fourteen interface strings (`hud.part`, `dashboard.chapter`, `options.title`, `language.title`, ...) with `{n}`, `{percent}`, `{title}` placeholders, and, since step 247, seven picture words (`picture.tap_start`, `picture.achievements`, `picture.select_chapter`, `picture.points`, `picture.level_finished`, `picture.portals_created`, `picture.game_over`) | `game/data/lang/<code>.json` (UTF-8 JSON); `en.json` is the reference every other file is held to |
| Where the English word of each of seven pictures was, the box the translated word is drawn in, its smallest scale, its shadow (numbers about geometry, and a digest of each sprite so another version is refused; no pixel) | `game/data/picture_recipes.json` |
| The achievements' titles and descriptions in each language, and the secret achievement's title and sentence (with `{world}` and `{level}`) as a `secret` block | `out/data/achievements.<code>.json` (**not in git**, see below) |
| The picker and its two globes: places, sizes, colours | `game/data/picker.json` |
| The four open-licence faces and their licence texts | `game/data/fonts/` (`README.md` there says where each came from) |
| The string tables, the saved choice, UTF-8, the picker's layout | `game/sim/Strings`, `Language`, `Utf8`, `Picker` |
| The words of the original's pictures, drawn again in the language on the screen | `game/sim/PictureBake` (pure, no renderer); the layer only asks it for a file (`MagicPortalsLayer::wordedPicture`) |
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
2. **The strings.** Copy `en.json` to `<code>.json` and translate the twenty-one values; keep every placeholder. The seven `picture.*` values are drawn INTO the original's
   pictures at run time and must fit their boxes: keep them short, and run `test_mp_overflow`, which fails a phrase that would need less than its recipe's floor (and
   `test_mp_picturebake`, which fails one with a character the faces lack). Keep the `_about` that says who translated it and how
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

The fourteen interface strings, the seven picture words (step 247), the 82 achievements' titles and descriptions and the secret achievement's two texts, in each of the ten other languages, were **machine-translated by a language
model from the English, with the terms aligned with the Magic Rampage Companion app, and reviewed by a second model pass. They have NOT been read by a native speaker.**
Each file says so. Expect wrong register, awkward phrasing and the occasional mistranslation; a reviewer's corrections go straight into the JSON (`LANG-1`).

**The seven picture words are machine translations too, with no native reader, and the shipped wording is the machine's first or second choice, not a settled one.** They were made
for this step from the Magic Rampage Companion app's terms (*tap*, *skill points*, *achievements*, *choose the chapter*, *portal*) and checked only for fit and for the characters the faces hold. Five of them are
a SHORTER wording chosen only so that the phrase fits its picture at a readable size: *game over* in Russian, Ukrainian, French and Italian (the longer, more usual phrases needed a
scale under the floor), and *portals created* in Turkish (`yaratılan portallar` for the longer `oluşturulan portallar`). In Japanese a space inside *level finished*, *portals created* and *game over*
is a place the line may break and is **never drawn**; whether those three are what a Japanese player expects (and whether *Select chapter*, which uses *chapter* written as a loanword,
should say *stage* or *chapter*) is a native reader's call. The Ukrainian *Select chapter* says `розділ` (the game's own dashboard word) where the Companion says `главу`.

## What is not translated

* **Text baked into the original's pictures**: seven of them are drawn again in the language on the screen since step 247 (the next section): TAP START, *Achievements*,
  *Select chapter*, *points*, *level finished*, *portals created* and *game over*. **Four still stay English in every language** (`LANG-2`): the *golden score* and *current score*
  plaques, the game-finished card and the names on the credits strip (prototyped, built by the next step). **And English itself is never redrawn**: the shipped picture is used, byte for byte.
* **Proper nouns:** the game's name, the developer's, the engine's, the names of the people in the credits.
* **Developer and failure text:** the start-up error dialog (it runs before any language is known), the log, `--help`, and the failure labels. These are English.
* **A card already on screen** when the language changes keeps its words until it goes.
* **A system language the game does not have** (`LANG-3`, closed by step 245 for the eleven it has): Chinese, Hebrew, Polish, Korean and every other language get English.
  Only the FIRST of Windows' preferred UI languages is read (a Windows whose list is `zh-CN`, `de-DE` gets English, not German), and macOS' and iOS' first too.

## The pictures that carry words (`LANG-2`, planning step 247)

Eleven of the original's pictures carry their words as pixels, so a player who chose German still read TAP START: that was the cause of the first report ("the text on the screen and
in the buttons always has English"). Seven of them are now drawn again in the language on the screen: TAP START (`main_play_game_button.png`), *Achievements* (`scores.png`, on the main
menu and in the pause), *Select chapter*, *points* (`achievement_points.png`, the dashboard's), *level finished*, *portals created* (`portals_created_plaque.png`) and *game over*.

* **What is done.** `game/sim/PictureBake` decodes the sprite's hd twin and, by the sprite's recipe in `game/data/picture_recipes.json`: **erases** the English word and draws the phrase in
  its place (TAP START, *Achievements*, *Select chapter*: the plate under the word is a model made of the picture's OWN text-free rows and columns, `row_lerp` or `separable`, with no donor
  picture and no ink detection; *points* is `level_button.png` moved by (-2, -3), which is bit-exactly the same plate without the word, `twin`); or **generates** the picture (*level finished*,
  *portals created*, *game over*: the picture is only words on a transparent ground, so no pixel of the original is read, and the phrase is drawn with the look of the original's lettering,
  white with a soft shadow). The phrase is drawn with the committed faces (Kurale, Noto Sans, Noto Sans JP and Noto Sans Arabic, by the slot's routing; Arabic shaped by `Strings::Display`),
  with a drop shadow like the original's, and shrunk step by step from its natural size to the recipe's box, never below the recipe's floor. The result is a same-size PNG in the scratch folder
  (`<scratch>/pictures/<id>_<code>_<hash>.png`, the hash over the algorithm, the table, the phrase, the language and the faces' bytes), and the layer draws it instead of the shipped file.
* **English is never baked.** The hook is in `MagicPortalsLayer::menuImage` and `originalAsset`, and the first thing it does is return the shipped path for English (and for any picture no
  recipe names), so the English path is what it always was: the tests' English frames (`tests/golden/menu_screens.json`) are unchanged and a run in English makes no folder of baked pictures.
  Switching back to English gives the shipped files back.
* **Where the words live.** The seven phrases are the `picture.*` keys of `game/data/lang/<code>.json`, as text, like every other string (upper case where the picture is upper case, Arabic in
  logical order, a space a line may break at; in Japanese a space is never drawn; U+200B only breaks; U+00A0 is a space that does not). `en.json` holds the English words of the pictures. **Nothing
  derived from the original's pictures is committed**: `picture_recipes.json` holds numbers about geometry (rectangles, rows, floors, a look) and, for each sprite, its size and a DIGEST of its
  decoded pixels, so that another version of the sprite is refused; the baked pictures exist only in the scratch folder at run time. (The test suites write them out only when `MP_PICTUREBAKE_DUMP=<dir>` is set, so that a reviewer can look.)
* **Fail closed.** Each bake checks the sprite's size and digest, the recipe's guards (the reference rows and columns the model reads agree with each other and with the picture in a 3-pixel collar;
  a twin is bit-exact outside the word's window and differs inside it), that every code point is in its face, that the fit reaches the floor, that no ink falls outside the rectangle, and that the
  PNG reaches the disk whole (written under a temporary name and renamed). If any check fails the shipped picture is used for that sprite in that language and ONE log line says which sprite, which
  language and why (`picture <sprite> in <code> stays English: ...`); nothing throws and no half-written file is left. A language file without the key, a missing scratch folder and a missing or
  refused `picture_recipes.json` (an older package folder, say) do the same: the pictures stay English, the game starts, and the log says why once.
* **The floors.** TAP START 0.55, *Select chapter* and *Achievements* 0.52, *points* 0.66, *level finished*, *portals created* and *game over* 0.60 (a fraction of the size at which the original's
  word is drawn). The tightest shipped phrase of each, measured with stb_truetype on the Linux machine: TAP START French 0.634, *Select chapter* French 0.790, *Achievements* Arabic 0.942, *points*
  German 0.918, *level finished* Turkish 0.715, *portals created* Russian 0.784, *game over* Ukrainian 0.808. A phrase that would need less is a defect of the TRANSLATION, not a run-time fall-back:
  `tests/test_mp_overflow.cpp` fails it. French TAP START has the least margin (0.634 against 0.55, about 14 percent; it is also the smallest word, about 17 screen pixels of cap height at 720p).
* **The cost.** A switch bakes nothing: a picture is made at its first draw, and one already made is found again in the scratch folder. Measured on the Linux machine (bake and PNG encode, least of
  three, Russian): TAP START 3.9 ms, *Achievements* 1.9, *Select chapter* 2.0, *points* 1.2, *portals created* 2.4, and **12 to 19 ms for *level finished* and *game over*** (their canvases are large).
  **Never timed on a phone** (the caveat of *The bake is synchronous on the tap*, in the known limits below, applies here too).
* **Every cache is remade by a switch.** `SetLanguage` takes out of the menu's picture cache every picture a recipe names (the menu puts each back at its first draw), and makes again the pause's, the
  end screens' and the popups' (a level that is loaded holds them). A test reads the rule tables and fails if one of the seven pictures is named anywhere else (the HUD's pads and plaque, the
  achievement cards): a cache the switch does not reach would leave it in the old language.
* **What it looks like.** The baked pictures were compared on contact sheets with the prototype's (the C++ differs by the glyph edges of another rasteriser and by a few pixels in the size of
  a word), and a residual test on the real sprites holds that the English word leaves no edge in the erased window (the sharpest edge of the window is 0 grey levels where the original's was over 200) and
  that nothing outside the rectangle changes by a byte. The one ground truth an erase has is the *Achievements* plate's unshipped sister picture (the erase agrees with it to a mean of 1.16 grey levels,
  worst 3 on 3,434 text-free pixels); *points* is exact. **Not compared with the original on a screen**: no emulator run, no phone, and no native reader has looked.
  In *points* the word sits where the original's did, on the plate's lower bevel (the picture's design, not a defect of the bake).
* **Open, and the owner's** (`LANG-1`, `LANG-2`): whether these machine translations ship before a native reader has seen them; the Japanese wording (`レベル クリア`, `作成した ポータル`, `ゲーム オーバー`
  and the loanword *chapter*); and whether English should ever get the same erase (it does not: the shipped picture is the original's).

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

**What is committed, exactly.** The **twenty-one** short strings (the fourteen interface strings and the seven picture words), in `game/data/lang/<code>.json`, and `game/data/picture_recipes.json` (geometry and digests, no pixel). **What is not:** the achievements' titles and descriptions
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

The seven picture words are the words of the original's pictures (TAP START, *Achievements*, ...): their English is committed as text in `en.json`, as the owner decided for this step, and their ten translations are new in git too. Seven of the fourteen interface strings are short interface words of the original's own, already in the repository in English since before the languages (`Part N`, `Chapter N`, `new!`,
`requires N%`, `Achievement unlocked! {title}`, `This level is locked!`, `move the camera around`); **their ten translations are new in git**, and whether short
translated UI phrases count as the original's text is the owner's call (`LANG-1`). The rest (`Controls`, `Modern`, `Classic`, `Adjust controls`, `Language`, and the
two credits lines) are this port's own words.

## Known limits, and the owner's calls

* **A first run now meets the ten machine translations without choosing them** (`LANG-1`, step 245). A German, Spanish, French, Italian, Portuguese, Russian, Turkish, Ukrainian,
  Japanese or Arabic system now starts in words that no native reader has read, where before a player met them only by opening the globe. Leaving the system's language out is one
  line (`main.cpp`'s call of `Language::ApplySystemLocale`) and the tests that pin it say what would change; whether to keep it until a reviewer has read them is the owner's call.
* **A first run in another language is still a partly mixed screen** (`LANG-2`). Since step 247 the main menu's TAP START and *Achievements*, chapter select's title, the dashboard's *points* and the
  end screens' *level finished*, *portals created* and *game over* are the language's. What stays English: the *golden score* and *current score* plaques (the pause, the end screens), the
  game-finished card and the credits strip's names (the next step), and every proper noun. The translated surface is the 14 interface strings, the 7 picture words and the 82 achievements.
* **A first run in Russian, Turkish, Ukrainian, Japanese or Arabic bakes its fonts before the first frame.** The two atlases of a generated language are made while the game starts
  (60-90 ms on the Linux machine of step 245's launches, from an empty scratch folder; a language already baked is read from the cache). Never timed on a phone. If they cannot be made,
  the run speaks English and says why in the log, as a pick does (`LANG-7`), and the next start tries again.
* **None of the system reads was run on a phone, a Mac, an iPhone or Windows.** Linux's was run on the built program (`LC_ALL`, `LC_MESSAGES`, `LANG`, a saved file, `--lang`,
  `--fixed-step`, `--frames`, `--saves`, `--screenshot`, `C`, `zh_CN`, an unset environment); the Android, Windows and Apple reads are written and unproved. Android's two files were
  parsed (not built) against NDK 28.2's headers, whose glue sets the configuration before the game starts; Windows' calls were checked by reading the SDK's headers, not by building;
  the Apple ones were never compiled (no Mac).
* **English is as before, with one character excepted.** The description of achievement 60 holds, in the original, one byte that is not valid UTF-8 (a lone Latin-1 byte;
  `docs/ethanon-formats.md` section 24). The achievements table used to carry U+FFFD for it, which the byte-wise text drew as three Latin-1 glyphs (the three bytes of U+FFFD's
  UTF-8 form); it now carries U+00BA, one glyph. So that one description is not byte for byte what it was before the languages, and nothing else in English moved.
* **Narrow screens (5:4, 1280 x 1024) are the weak spot of the longer languages** (`LANG-6`; found by the verification round of 2026-11-10, which looked at 11 languages x 16 screens x
  2 shapes, 2340 x 1080 and 1280 x 1024). At 5:4 German, Spanish, French, Italian, Portuguese, Russian and Ukrainian overflow: the achievements' descriptions on the dashboard run past
  the bar to the edge of the screen (the fit width of 207 units assumes a bar that is not there at 5:4); the *new!* label is clipped in Spanish and Italian; the pause's title (`Niveau 1/32`,
  `Livello`, `Nivel`, `Seviye`, ...) touches or overlaps the golden-score plaque. English touches the edge once at 5:4 as well (documented). At 20:9
  everything fits, and so it did at 16:9 (1280 x 720) when the release audit of the release that brought the languages clicked through the options, the picker, the credits strip, the chapter select, the whole
  achievements list, the pause menu and 1-21's hint in all eleven languages on the shipped Linux build: no text past the list's bar, the plaque or another box. That look did not
  reach the unlock popup, the game-finished card or Adjust controls, and no shape between 16:9 and 5:4. The picker's grid also assumes a screen no narrower than 5:4. This is a known limit and is not fixed.
* **In Arabic a locked chapter's "requires 60%" is drawn with the percent sign left of the number (`%60`).** Found by the release audit of the release that brought the languages (a missing case of the
  bidirectional rule for a percent sign next to digits in `ArabicShaping`); cosmetic, one label (`LANG-5`).
* **A right-to-left hint can run under the right walk arrow.** 1-21's hint, mirrored to the right edge in Arabic, can pass under the right walk arrow at the larger sizes of the modern
  controls. Cosmetic, one hint (`LANG-7`).
* **After a failed picker bake the globe does nothing.** If the picker's atlas cannot be made, the globe stays drawn and stays clickable, and the tap does nothing (`LANG-7`).
* **An Arabic string that falls back to English is drawn through the Arabic display path**, so its punctuation jumps to the wrong side. Only on a fault path: a key or a file that
  did not read (`LANG-7`).
* **Wording a native reader should look at first** (noted by the verification round of 2026-11-10, whose reading was not a native speaker's; `LANG-1`): Russian *new!* (a short form, `Nov!` transliterated) and *Modern*; Japanese *new!*
  ends in an ASCII `!` where the other Japanese strings use full-width marks, and the credits line for the engine adds a word the English does not have; Italian *requires N%* has `il`
  written in (wrong for a number such as 80); French has no space before `!` and `%`; Arabic mixes the indefinite and the definite form and has a doubled *min*; Italian leaves the achievement title *Like a Rolling Stone* in English (it may be a song title on purpose).
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
`test_mp_langlayer` (the layer in each language, and the golden of English), `test_mp_overflow` (every translation against its box, the seven picture words against theirs and their floors), `test_mp_picturebake` (the bake, on synthetic pictures, and the table's loader) and
`test_mp_picturebake_original` (the same on the original's seven sprites; skipped without them), and for the picker `test_mp_picker` and
`test_mp_pickerlayer`. `tools/package/test_package.py` and `test_android_package.py` hold that the tables, the faces and the translations travel, and
`tests/test_mp_unpack.cpp` that the Android unpack accepts their paths. `tools/l10n/test_make_achievement_languages.py` holds the overlay's maker (the secret block's
placeholders and character set among its checks), `tools/ci/test_fonts_pinned.py` the pinned faces, and `tools/asbc/test_asbc.py` the decoder of the original's strings.
