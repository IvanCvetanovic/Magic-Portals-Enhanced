# Languages

The original game is in English only. Magic Portals Enhanced shows its interface in eleven languages, in the order of the Magic Rampage
Companion app's picker: **English** (the original's, and what a system that speaks none of the others gets), **Deutsch, Español, Français, Italiano,
Português, Русский, Türkçe, Українська, 日本語, العربية** (`en de es fr it pt ru tr uk ja ar`). A first run speaks the system's language when it is one of
the eleven (below); the globe on the main menu changes it. This page is the design, how to add a language, where the translations came from,
and what is not translated. The record of how it was built, step by step, with its gates, is Steps 236-239, 245 and 247-249 of
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
| The eleven codes, their own names, direction, which fonts, how a generated font is drawn, and the two icons a generated atlas draws itself (`fonts.icons`, step 249) | `game/data/lang/languages.json` |
| The twenty-nine short strings of each language: fourteen interface strings (`hud.part`, `dashboard.chapter`, `options.title`, `language.title`, ...) with `{n}`, `{percent}`, `{title}` placeholders, and, since steps 247 and 248, fifteen picture words (`picture.tap_start`, `picture.achievements`, `picture.select_chapter`, `picture.points`, `picture.level_finished`, `picture.portals_created`, `picture.game_over`, and of step 248 `picture.current_score`, `picture.golden_score`, `picture.game_finished` and the credits strip's five bands `credits.role.design`, `credits.role.illustration`, `credits.role.illustration_at`, `credits.role.modelling`, `credits.thanks`) | `game/data/lang/<code>.json` (UTF-8 JSON); `en.json` is the reference every other file is held to |
| Where the English word of each of eleven pictures was, the box the translated word is drawn in, its smallest scale, its shadow (numbers about geometry, and a digest of each sprite so another version is refused; no pixel) | `game/data/picture_recipes.json` |
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
2. **The strings.** Copy `en.json` to `<code>.json` and translate the twenty-nine values; keep every placeholder. The fifteen `picture.*` and `credits.*` picture values are drawn INTO the original's
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

The fourteen interface strings, the fifteen picture words (steps 247 and 248), the 82 achievements' titles and descriptions and the secret achievement's two texts, in each of the ten other languages, were **machine-translated by a language
model from the English, with the terms aligned with the Magic Rampage Companion app, and reviewed by a second model pass. They have NOT been read by a native speaker.**
Each file says so. Expect wrong register, awkward phrasing and the occasional mistranslation; a reviewer's corrections go straight into the JSON (`LANG-1`).

**The seven picture words are machine translations too, with no native reader, and the shipped wording is the machine's first or second choice, not a settled one.** They were made
for this step from the Magic Rampage Companion app's terms (*tap*, *skill points*, *achievements*, *choose the chapter*, *portal*) and checked only for fit and for the characters the faces hold. Five of them are
a SHORTER wording chosen only so that the phrase fits its picture at a readable size: *game over* in Russian, Ukrainian, French and Italian (the longer, more usual phrases needed a
scale under the floor), and *portals created* in Turkish (`yaratılan portallar` for the longer `oluşturulan portallar`). In Japanese a space inside *level finished*, *portals created* and *game over*
is a place the line may break and is **never drawn**; whether those three are what a Japanese player expects (and whether *Select chapter*, which uses *chapter* written as a loanword,
should say *stage* or *chapter*) is a native reader's call. The Ukrainian *Select chapter* says `розділ` (the game's own dashboard word) where the Companion says `главу`, and the Ukrainian *points* is `очки`, the Russian word letter for letter: a native Ukrainian reader should decide whether it is Ukrainian enough.

**The eight picture words of step 248 are machine translations too, with no native reader** (`picture.current_score`, `picture.golden_score`, `picture.game_finished` and the credits strip's `credits.role.design`, `credits.role.illustration`, `credits.role.illustration_at`, `credits.role.modelling`, `credits.thanks`): terms aligned with the Companion's strings where it has them (*Punkte*, *puntos*, *pontos*, *очки*, *puan*, *ポイント*, *نقاط*; *Gold*, *oro*, *ouro*, *altın*, *ゴールド*, *الذهب*; the credits' roles and *thanks*), checked only for fit and for the characters the faces hold. The golden plaque's *portals for golden score* carries *score* by *gold* (the Companion's wording; a literal *goldene Wertung* is longer and drops the lettering to 14 to 16 pixels). The credits' *illustration - 2d art at* has to lead into the company's name on the next line of the picture, so Russian, Turkish, Ukrainian and Japanese say *studio* (`студия`, `stüdyo`, `студія`, `スタジオ`) where a preposition or a locative could not stand before a name. German nouns are capitalised in the credits (*Illustration - 2D-Grafik*); the other languages keep the picture's lower case. Japanese uses an ASCII space where the prototype used U+200B as a place to break in the three picture phrases (the card's, the two plaques'): the bake treats the two alike (a test lays both out glyph for glyph) and an ASCII space is what the step 247 phrases already use. In the credits the spaces round the dashes are real spaces in every language, Japanese included.

## What is not translated

* **Text baked into the original's pictures**: all eleven of the pictures that carry words are drawn again in the language on the screen since steps 247 and 248 (the next section): TAP START, *Achievements*,
  *Select chapter*, *points*, *level finished*, *portals created*, *game over*, the *current score* and *golden score* plaques, the first paragraph of the game-finished card and the five bands of the credits strip's roles and heading.
  **What stays English in them** (`LANG-2`): only what a picture says in proper nouns - the names on the credits strip, the company's name (*panda vermelho*), the logos. **The card's *Like us on facebook!* line is not kept in English: a language other than English shows NO Facebook line at all** (option A, the owner's choice: the Facebook button was omitted on purpose, so the card shows the translated first paragraph alone; English keeps the shipped picture, line and all). Option B (translate the line) and C (a thank-you in its place) are the owner's, below. **And English itself is never redrawn**: the shipped picture is used, byte for byte.
* **Proper nouns:** the game's name, the developer's, the engine's, the names of the people in the credits.
* **Developer and failure text:** the start-up error dialog (it runs before any language is known), the log, `--help`, and the failure labels. These are English.
* **A card already on screen** when the language changes keeps its words until it goes.
* **A system language the game does not have** (`LANG-3`, closed by step 245 for the eleven it has): Chinese, Hebrew, Polish, Korean and every other language get English.
  Only the FIRST of Windows' preferred UI languages is read (a Windows whose list is `zh-CN`, `de-DE` gets English, not German), and macOS' and iOS' first too.

## The pictures that carry words (`LANG-2`, planning step 247)

Eleven of the original's pictures carry their words as pixels, so a player who chose German still read TAP START: that was the cause of the first report ("the text on the screen and
in the buttons always has English"). All eleven are now drawn again in the language on the screen (step 247 built seven, step 248 the other four: the sections *The other four pictures* below). The first seven: TAP START (`main_play_game_button.png`), *Achievements* (`scores.png`, on the main
menu and in the pause), *Select chapter*, *points* (`achievement_points.png`, the dashboard's), *level finished*, *portals created* (`portals_created_plaque.png`) and *game over*.

* **What is done.** `game/sim/PictureBake` decodes the sprite's hd twin and, by the sprite's recipe in `game/data/picture_recipes.json`: **erases** the English word and draws the phrase in
  its place (TAP START, *Achievements*, *Select chapter*: the plate under the word is a model made of the picture's OWN text-free rows and columns, `row_lerp` or `separable`, with no donor
  picture and no ink detection; *points* is `level_button.png` moved by (-2, -3), which is bit-exactly the same plate without the word, `twin`); or **generates** the picture (*level finished*,
  *portals created*, *game over*: the picture is only words on a transparent ground, so none of the original's pixels reaches the output (the sprite is still decoded and checked by its size and digest, so a machine without it, or with another version of it, keeps the English picture), and the phrase is drawn with the look of the original's lettering,
  white with a soft shadow). The phrase is drawn with the committed faces (Kurale, Noto Sans, Noto Sans JP and Noto Sans Arabic, by the slot's routing; Arabic shaped by `Strings::Display`),
  with a drop shadow like the original's, and shrunk step by step from its natural size to the recipe's box, never below the recipe's floor. The result is a same-size PNG in the scratch folder
  (`<scratch>/pictures/<id>_<code>_<hash>.png`, the hash over the algorithm, the table, the phrase, the language and the faces' bytes), and the layer draws it instead of the shipped file.
  **A generated picture still needs the original's sprite on the machine**: it is decoded and checked by its size and digest on every ask, though none of its pixels is used, so a machine without the
  sprite, or with another version of it, keeps the English picture (and logs why). The sprite is the original's, which the game needs anyway.
* **English is never baked.** The hook is in `MagicPortalsLayer::menuImage` and `originalAsset`, and the first thing it does is return the shipped path for English (and for any picture no
  recipe names), so the English path is what it always was: the tests' English frames (`tests/golden/menu_screens.json`) are unchanged and a run in English makes no folder of baked pictures.
  Switching back to English gives the shipped files back.
* **Where the words live.** The fifteen phrases (ten `picture.*` keys and the five `credits.role.*` and `credits.thanks` keys of the strip's bands) are in `game/data/lang/<code>.json`, as text, like every other string (upper case where the picture is upper case, Arabic in
  logical order, a space a line may break at; in Japanese a space only breaks and is not drawn, except round the credits' dashes, where the recipe's `japanese_spaces` draws it; U+200B only breaks; U+00A0 is a space that does not). `en.json` holds the English words of the pictures. **Nothing
  derived from the original's pictures is committed**: `picture_recipes.json` holds numbers about geometry (rectangles, rows, floors, a look) and, for each sprite, its size and a DIGEST of its
  decoded pixels, so that another version of the sprite is refused; the baked pictures exist only in the scratch folder at run time. (The test suites write them out only when `MP_PICTUREBAKE_DUMP=<dir>` is set, so that a reviewer can look.)
* **Fail closed.** Each bake checks the sprite's size and digest, the recipe's guards (the reference rows and columns the model reads agree with each other and with the picture in a 3-pixel collar;
  a twin is bit-exact outside the word's window and differs inside it), that every code point is in its face, that the fit reaches the floor, that no ink falls outside the rectangle, and that the
  PNG reaches the disk whole (written under a temporary name and renamed). If any check fails the shipped picture is used for that sprite in that language and ONE log line says which sprite, which
  language and why (`picture <sprite> in <code> stays English: ...`); nothing throws and no half-written file is left. A language file without the key, a missing scratch folder and a missing or
  refused `picture_recipes.json` (an older package folder, say) do the same: the pictures stay English, the game starts, and the log says why once.
* **The floors.** TAP START 0.55, *Select chapter* and *Achievements* 0.52, *points* 0.66, *level finished*, *portals created* and *game over* 0.60 (a fraction of the size at which the original's
  word is drawn). The tightest shipped phrase of each, measured with stb_truetype on the Linux machine: TAP START French 0.634, *Select chapter* French 0.790, *Achievements* Arabic 0.942, *points*
  German 0.918, *level finished* Turkish 0.715, *portals created* Russian 0.784, *game over* Spanish 0.715. A phrase that would need less is a defect of the TRANSLATION, not a run-time fall-back:
  `tests/test_mp_overflow.cpp` fails it. French TAP START has the least margin (0.634 against 0.55, about 14 percent; it is also the smallest word, about 17 screen pixels of cap height at 720p).
* **The lost screen.** *Game over* is placed at (640, 252) of the 1280 x 720 view and the restart and list buttons at (512, 432) and (768, 432), 1.405 screen px per hd px (`game/data/ui.json`,
  `level_end/lost`), so the buttons' visible top is at hd row about 201 of the 256-pixel picture and the shadow reaches about 12 rows past the ink. The recipe's safe rectangle therefore ends at row
  186 and is symmetric about the anchor (it ended at 204 at first, and a French title touched the restart button); `test_mp_picturebake_original` fails a title of any language, shadow included, that
  reaches row 201 (the longest shipped one ends at row 193). The other two stacked pictures keep the original's own room.
* **The cost.** A switch bakes nothing: a picture is made at its first draw, and one already made is found again in the scratch folder. Measured on the Linux machine (bake and PNG encode, least of
  three, Russian): TAP START 3.9 ms, *Achievements* 1.9, *Select chapter* 2.0, *points* 1.2, *portals created* 2.4, and **12 to 19 ms for *level finished* and *game over*** (their canvases are large).
  **Never timed on a phone** (the caveat of *The bake is synchronous on the tap*, in the known limits below, applies here too).
* **Every cache is remade by a switch.** `SetLanguage` takes out of the menu's picture cache every picture a recipe names (the menu puts each back at its first draw), and makes again the pause's, the
  end screens' and the popups' (a level that is loaded holds them) and, since step 248, the level HUD's plaque (`m_plaqueImage`, one string, set when a level with a medal loads). A test reads the rule tables and fails if one of the eleven pictures is named anywhere else (the HUD's pads, the
  achievement cards): a cache the switch does not reach would leave it in the old language. It lists the places it allows (the main menu, the selector, the dashboard, the pause, the level HUD, the end screens, the popups and the credits) and counts the 14 places the tables name the pictures in.
* **What it looks like.** The baked pictures were compared on contact sheets with the prototype's (the C++ differs by the glyph edges of another rasteriser and by a few pixels in the size of
  a word), and a residual test on the real sprites holds that the English word leaves no edge in the erased window (the sharpest edge of the window is 0 grey levels where the original's was over 200) and
  that nothing outside the rectangle changes by a byte. The one ground truth an erase has is the *Achievements* plate's unshipped sister picture (the erase agrees with it to a mean of 1.16 grey levels,
  worst 3 on 3,434 text-free pixels); *points* is exact. **Not compared with the original on a screen**: no emulator run, no phone, and no native reader has looked.
  In *points* the word sits where the original's did, on the plate's lower bevel (the picture's design, not a defect of the bake).
* **Open, and the owner's** (`LANG-1`, `LANG-2`): whether these machine translations ship before a native reader has seen them; the Japanese wording (`レベル クリア`, `作成した ポータル`, `ゲーム オーバー`
  and the loanword *chapter*); and whether English should ever get the same erase (it does not: the shipped picture is the original's).

## The other four pictures (`LANG-2`, planning step 248)

The golden and current score plaques, the game-finished card and the credits strip, on the same core (`game/sim/PictureBake`, `picture_recipes.json`): English never baked, the sprite's size and digest checked on every ask, every check failing closed to the shipped picture with one log line, the result a same-size PNG in the scratch folder. What the step added to the core, each with a synthetic fixture in `test_mp_picturebake` and the real sprite in `test_mp_picturebake_original`:

* **The plaques (rule `plaque`).** A plate with a straight frame: a frame column is the same colour down the plate (`F(x)`, the median of the text-free rows), an interior pixel is a vertical profile times a horizontal vignette (`V(y) H(x)` per channel, `H` from the text-free rows, `V` a line through the text-free rows above and the margin columns beside the old lettering, interpolated and never extrapolated: a cubic fitted to the clean rows rises under the lettering while the real pixels fall). The old lettering covered the bevel at both ends of the plaque, so the first rectangle spans the whole width and the frame columns are filled from `F(x)`; the lower band is interior only (the corners round). The golden plaque's medal is part of its sprite: a pixel within 2 px of a chromatic one is never replaced (2,007 pixels; none of the medal changes, and in all ten languages the baked plaque's medal is the sprite's, bit for bit). The translated phrase is a **paragraph**: at each scale from the recipe's size down the arrangement with the fewest lines whose widest line fits (then the narrowest widest line), the lines centred on the plaque by their ink and the block's ink centred in its box, a halo like the original's (`min(0.9, 1.8 * Gauss(1.6))` moved one pixel down), the line pitch a multiple of the size set per face (Kurale 1.40, Noto Sans JP 1.30, Noto Sans Arabic 1.50). The current plaque's ink starts at row 115: the gold medal, drawn over it 8 hd rows down, has its last row of alpha 128 or more at plaque row 114, and an ink box that began at 114 had a few pixels of a letter under the ribbon's tip (found by compositing the three medals over every language). The medals' soft shadows (alpha under 128, to row 123) still dim the first rows of the ink a little (at most 206 ink pixels in the test's count); they hide none.
* **The card (rule `panel`).** The same model without the frame columns: `V(y)` is read for EVERY row of the rectangle from the LEFT margin columns alone (the right margin carries a corner effect in rows 150..195 and is only a check), `H(x)` from the two bands of ink-free rows. **The owner's choice, taken before step 248 (option A):** a language other than English draws ONLY the translation of *Well done! Stay tuned for more*, centred as one paragraph (up to four lines, never grown), and shows NO Facebook line: the Facebook button was omitted on purpose (dead services), and no replacement words are invented. English keeps the untouched picture, Facebook line and all. **Left open for the owner** (`LANG-2`): option B (also translate *Like us on facebook!*, which advertises a button the port does not have) and option C (replace the second paragraph with a thank-you, words that are not in the original); neither is built and their keys do not exist.
* **The credits strip (rule `clear_rows`, layout `bands`).** The strip is words and names on a transparent ground. The rows of the five bands (two role lines, three single role lines and the big heading) are cleared, all columns, each asserted to be bounded above and below by a fully transparent row, and each band's ink window asserted to hold no opaque pixel outside the cleared rows, so nothing can cut a name or a logo; then each band's phrase is drawn again, left-aligned by the ink's edge on hd x 64 where every original role starts, in plain black with alpha equal to the coverage and no shadow (the strip is drawn at alpha 230 and the baked ink rides it, so it is not multiplied twice), at the fewest lines that fit the largest size (wrapping wins over shrinking), a dash cutting a line only AFTER it (the original does the same), the block slid up where a descender would pass the window's last row, ASCII digits drawn from Noto Sans Bold (Kurale's emboldened 3 reads as an 8). **Two rules of step 249, from the real-game look at step 248's strips** (the next section, *The credits strip's leading and heading*): the lines of a band keep 0.15 of its size apart, as the original's own two-line design band does, and a heading that wraps to two lines is moved to between the role line above it and the first name below it. The names, *Daniel and Carol Santee* with its *and*, the company line, the logos and every row outside the five windows are the original's, pixel for pixel, in all ten languages (asserted). `Credits::TailWords` does not draw any of the five phrases a second time: it draws only the closing card's six lines. **Arabic is left-aligned like the other bands** (as the prototype had it), so the lines' ragged edge is on the right; a right-aligned Arabic layout, to a common right edge, would look better beside the names but is not built.
* **What the lettering is.** The original's lettering is heavy hand-drawn blackletter (the plaques), a pixel font (the card) and angular capitals with a swashed *S* (the strip); the shipped faces (Kurale, Noto Sans Bold, Noto Sans JP, Noto Sans Arabic) are lighter and rounder, with the embolden the numbers say. It is legible and clean, and it does not look like the original's: **that is the owner's eye** (`LANG-2`).
* **The floors**, chosen from legibility alone before any fit was looked at: the plaques and the card 0.60 of the size at which the lettering is drawn at scale 1 (a capital of 20 and 24 hd px: 12 and 14.4 hd px, 8.4 and 10 screen px at 720p), the strip 0.70 of a role's x-height. The tightest shipped phrases, measured with stb_truetype on the Linux machine: *current score* Arabic 0.668, *golden score* Russian 0.724, the card Japanese 0.817, the strip Spanish 0.859 (a band's own scale; the least over the bands).
* **The cost**, bake and PNG encode, least of three, Russian, Linux machine: current plaque 2.5 ms, golden plaque 2.8, card 33.5, strip 54.5 (39 and 69 ms in step 249's run, which shared the machine). Step 248 baked the card at every first level load and the strip on the main menu's first frame, about 50 ms each; **since step 249 both are baked when they are shown** (the section *When the pictures are baked*). **Never timed on a phone.**
* **Not reached in a capture:** the game-finished card itself (it needs the last level of the last chapter won): its path is tested through the layer (`PopupPictureFile("sprites/game_finished.png")` is the language's, remade by a switch, shipped in English), and the baked picture was looked at on a contact sheet. The credits strip, the level HUD's plaque with a medal and the pause's two plaques were captured in the real game in German, Japanese and Arabic (headless, under a virtual display).

## The achievements popup's check mark and padlock (planning step 249)

The pause's achievements popup writes a mark before each achievement: a check mark when it is unlocked and a padlock when it is not. The original does it with its Verdana bitmap font, whose glyph codes `@` and
`#` ARE those two icons (`game/data/achievements.json`, `popup.unlocked_mark` and `locked_mark`, decoded from `AchievementsPopup`). English and the five languages set in the original's fonts (German, Spanish,
French, Italian, Portuguese) keep `@` and `#` and so get the icons, byte for byte as before. A language drawn from a generated atlas (Russian, Turkish, Ukrainian, Japanese, Arabic) has no such glyph: its
Noto Sans draws an `@` and a `#` as themselves, and the real-game look at step 248 showed a literal `@` before an unlocked title and a `#` before a locked one.

* **The fix draws two icons of our own.** `languages.json`'s `fonts.icons` names a check mark (`U+E000`) and a padlock (`U+E001`) in the private-use area, where no text and no face has a glyph, each with
  `stands_for` (the original's glyph code it takes the place of: `@` and `#`), the slots that carry it (only Verdana's), its advance and bearing and its shape numbers, in cap heights of the slot. `FontBake` rasterises
  them from vector shapes by 8 x 8 supersampling (a check mark is two round-capped strokes; a padlock a rounded body, an arch and a keyhole cut out of the body), as tall as a capital and standing on the baseline, through the same cell,
  padding, shadow and composition as a letter. **Nothing is read, copied or derived from the original's fonts or pictures** (they are Asantee's and Microsoft's); the numbers are AUTHORED, chosen by eye against a capture of the English
  popup in the real game, and say so. The shapes use `+ - *` and `sqrt` only. A slot that carries no icon bakes exactly what it did (Matura's atlases and the picker's keep their names: the icons are in the hash of a slot that carries them
  and of no other), and every other glyph of Verdana's atlas has the same metrics and pixels as before (a test bakes both ways and compares).
* **Where the choice is made, once.** `Strings::Table::OriginalGlyph(code)` returns the icon's code point (UTF-8) for a language drawn from a generated atlas and the original's own code for every other; the layer asks it for the two marks and
  `Achievements::PopupText` is given them (`PopupMarks`), then the whole text is shaped once as before. `achievements.json` keeps `@` and `#` untouched: they are the original's decoded fact, and the code points are named in one place only.
  The loader reads `icons` as strictly as the rest (an unknown member, a code point outside U+E000-F8FF or in a script's ranges, a glyph code that is a letter or a digit, a slot that is not the plan's, a number outside its box: each refused with the member's place). No string of any language file holds a private-use character (a test).
* **Arabic.** The popup's text is shaped once, in visual order; the icon is a neutral character like the `@` it replaces, so in Arabic it ends up at the right end of the first line, where the `@` was.
* **Any other glyph code?** A scan of every table in `game/data` and `data`, every language file and every achievement translation for `@ # ^ ~ |`, a backslash, a control character or a private-use character in a string the fonts could draw found
  none but the two marks. (`#` and `@` in the converter's names and the command line are not drawn.)
* **The size is the capital's.** Both icons are as tall as a capital H of the slot (24 px in Verdana's) and about a capital wide; the English popup's check mark is a heavier brush stroke and its padlock plainer than ours (no keyhole);
  ours read as the same two things at the same size and weight. `test_mp_overflow` measures the popup with the original's Verdana for every language, so it did not change and does not measure the icons' advance (1.2 capitals).

## The credits strip's leading and heading (planning step 249)

Two things the real-game look at step 248's strips found, both only in `game/data/picture_recipes.json` and `PictureBake`:

* **The lines of a band were too close.** The recipe's `gap` (the least clear distance between two lines, a fraction of the band's size, scaled with the fit) was 0.05: 0.66 hd px between the lines of the design band at its size and 1.3 between the heading's two (the g-loop of
  *Agradecimientos* and the p of *especiales* 1.7 apart, in Spanish and Portuguese), and in Portuguese and German some lines nearly touched. It is **0.15**, measured on the original's own English design band (read-only on the extracted sprite, nothing kept): its two lines of x-height 13.2 hd px are at least 3 free rows apart in a column both reach and at
  least 2.0 hd px apart by distance, and 2.0 / 13.2 = 0.15. The pitch still starts at the original's 26.2 (the heading's 40) and grows only where the ink needs it, in the half-pixel steps `FitPitch` takes; where the grown block no longer fits the band's ink rows the fit takes the next
  scale (0.97 of it), as it always did, down to the floor 0.70. **In five languages the heading takes one step** (Spanish 0.859 to 0.833, French 0.913 to 0.885, Italian 0.885 to 0.859, Portuguese 0.885 to 0.859, Russian 0.859 to 0.833) and no other band changed its scale. The least free rows between the lines of any band are now 2.0 (they were 0.5).
* **A two-line heading read as belonging to the wrong block.** It was anchored by its LAST baseline (565), so it grew upward: in es, fr, it, pt, ru and uk it began 11 to 18 rows under the *3D modelling* line and ended 22 to 26 above the first name (*Bruno Fernandes*, row 600). A band's new `balance` member (`above_band`, `below_row`, `min_lines`) says that
  once its size and lines are fitted as before, a band of two lines is MOVED, the same lines at the same size, to where the free rows above it (to the ink of the band above, as drawn in the language) equal the free rows below it (to row 600); a heading of one line (English, German, Turkish, Japanese, Arabic) keeps its baseline, byte for byte. The free rows above and below are now equal in the six languages (Spanish 20 and 20, French 19, Italian 19, Portuguese 15, Russian 17, Ukrainian 19). The move is made after the fit, not in it, so that the room it frees does not turn a heading of one line into two
  (German's would) or make a heading larger than it was. The ink still lies inside the band's ink rows (`[488, 598)` for the heading, `[39, 129)` for the design band), which the sprite holds free of any other opaque pixel; the recipe's `clear` rows (`[524, 581)` and `[43, 101)`) are what is erased and are not changed.
* **What the 10 pictures became** (the strip's content md5 before and after, from the test suite's dump): Japanese is the same picture; the other nine changed (the gap moves the lines of the design band of de, es, fr, pt, tr and ar, and the six two-line headings of es, fr, it, pt, ru and uk move). The other 100 baked pictures of the ten languages are byte for byte what they were.
* **Checked on the real sprite in every language** (`test_mp_picturebake_original`): the lines of a multi-line band are at least 1.5 hd rows apart (and at least 0.15 of the band's scaled size less half a pixel); a wrapped heading has as many free rows above it as below it to within 3, and at least 10 on each side; a heading of one line stands on baseline 565.

## When the pictures are baked (planning step 249)

A picture that carries words is baked for a language that is not English at the first moment it is needed, and English never bakes. Since step 249 two of them are not baked before they are shown:

* **The credits strip** (512 x 1024, about 50 ms on a desktop CPU) is baked when the credits screen opens, not on the main menu's first frame. **The game-finished card** (680 x 512, about 50 ms) is baked when it is raised (chapter select after 4-32) or first drawn, not at every level load. Both
  are kept out of their caches (`m_pictureLazy`, `m_popupLazy`) until asked for; a language switch moves a baked one back there instead of remaking it, so a switch still bakes nothing by itself, and one already made is found again in the scratch folder.
* **What a cache hit costs.** `PictureBake::Ensure` decodes the sprite, checks its size and digest and reads every face before it looks for the file: step 247's safety, **not changed**. On the Linux machine, least of three, Russian: 1.0 to 1.5 ms for a small picture, **5.9 ms for the strip and 6.4 ms for the card**. A language's first use of a picture pays it once per run (the answer is remembered), so a warm
  first level load finds its nine pictures in 12 (Russian) to 19 (German) ms in all.
* **A first level load still bakes pictures a level does not need yet**, on a cold scratch folder (German, this machine): the golden and current plaques (7.1 and 9.9 ms) and *Achievements* (4.1) of the pause, and the end screens' *level finished* (39.6), *portals created* (4.0) and *game over* (46.2), a total of about 111 ms, of which 86 ms are
  the two end screens, needed only when the level is won or lost. They were not made lazy: the end screens' sizes are read at the level's load (`m_endTexels`), the pause's pictures are one tap from being drawn. Doing it is a larger change than the two caches above (`LANG-10`). **Never timed on a phone.**

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

**What is committed, exactly.** The **twenty-nine** short strings (the fourteen interface strings and the fifteen picture words), in `game/data/lang/<code>.json`, and `game/data/picture_recipes.json` (geometry and digests, no pixel). **What is not:** the achievements' titles and descriptions
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

The fifteen picture words are the words of the original's pictures (TAP START, *Achievements*, ...): their English is committed as text in `en.json`, as the owner decided for this step, and their ten translations are new in git too. Seven of the fourteen interface strings are short interface words of the original's own, already in the repository in English since before the languages (`Part N`, `Chapter N`, `new!`,
`requires N%`, `Achievement unlocked! {title}`, `This level is locked!`, `move the camera around`); **their ten translations are new in git**, and whether short
translated UI phrases count as the original's text is the owner's call (`LANG-1`). The rest (`Controls`, `Modern`, `Classic`, `Adjust controls`, `Language`, and the
two credits lines) are this port's own words.

## Known limits, and the owner's calls

* **A first run now meets the ten machine translations without choosing them** (`LANG-1`, step 245). A German, Spanish, French, Italian, Portuguese, Russian, Turkish, Ukrainian,
  Japanese or Arabic system now starts in words that no native reader has read, where before a player met them only by opening the globe. Leaving the system's language out is one
  line (`main.cpp`'s call of `Language::ApplySystemLocale`) and the tests that pin it say what would change; whether to keep it until a reviewer has read them is the owner's call.
* **A first run in another language is no longer a mixed screen** (`LANG-2`, closed by step 248 but for the owner's decisions about the card's Facebook line, which is gone from the card in these languages, not English). The main menu's TAP START and *Achievements*, chapter select's title, the dashboard's *points*, the end screens' *level finished*, *portals created*, *game over* and *golden score*, the pause's and the level HUD's *current score* and *golden score* plaques, the game-finished card's first paragraph and the credits strip's roles and heading are the language's. What stays English: every proper noun; and the card has no Facebook line at all in these languages (option A: it is not drawn, not drawn in English). The translated surface is the 14 interface strings, the 15 picture words and the 82 achievements.
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

`test_mp_strings` (the files and their keys and placeholders, the icons' loader, `OriginalGlyph`), `test_mp_utf8`, `test_mp_language` (the saved choice), `test_mp_fontbake` (the faces' glyphs, and the two icons: their size, their shape by a stated metric, their hash) and `test_mp_arabic`, `test_mp_langsim` and
`test_mp_langlayer` (the layer in each language, and the golden of English), `test_mp_overflow` (every translation against its box, the fifteen picture words against theirs and their floors, band by band for the credits strip), `test_mp_picturebake` (the bake, on synthetic pictures, and the table's loader) and
`test_mp_picturebake_original` (the same on the original's eleven sprites; skipped without them), and for the picker `test_mp_picker` and
`test_mp_pickerlayer`. `tools/package/test_package.py` and `test_android_package.py` hold that the tables, the faces and the translations travel, and
`tests/test_mp_unpack.cpp` that the Android unpack accepts their paths. `tools/l10n/test_make_achievement_languages.py` holds the overlay's maker (the secret block's
placeholders and character set among its checks), `tools/ci/test_fonts_pinned.py` the pinned faces, and `tools/asbc/test_asbc.py` the decoder of the original's strings.
