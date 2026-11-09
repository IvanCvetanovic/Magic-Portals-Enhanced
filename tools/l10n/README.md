# tools/l10n: the interface languages' faces

`make_fonts.py` makes the four `.ttf` files of `game/data/fonts/` (and their licence texts, `OFL-<name>.txt`, and the generated block of that folder's
README) from pinned downloads of the upstream fonts. What the faces are for, and which draws what, is `game/data/fonts/README.md` and
`game/data/lang/languages.json` (`fonts`); the code that bakes them into the atlases the game draws is `game/sim/FontBake`.

## Run it

The venv is this folder's own (`.venv/` is gitignored; `python -m venv` and one package, no other dependency):

```
python -m venv tools/l10n/.venv
tools/l10n/.venv/Scripts/python -m pip install fonttools      # Windows; tools/l10n/.venv/bin/python on Linux and macOS
tools/l10n/.venv/Scripts/python tools/l10n/make_fonts.py
```

It needs the network the first time (about 12 MB: the four variable fonts and the elementary-school kanji list), keeps what it downloaded in
`out/l10n-cache/` (gitignored) and checks every file against a sha256 written in the script, so a changed upstream is refused rather than
trusted. A rerun on the same inputs with the same fontTools writes the same bytes (the README block says which fontTools made the committed files).

`--texts PATH ...` adds JSON files or folders whose strings decide which glyphs are kept, besides `game/data/lang/*.json`. The default is `out/data`:
the achievements' translations (`achievements.<code>.json`) are Asantee's words in other languages, so they are not in git, but the Japanese face has to
hold every kanji they use. Rerun the tool whenever a language file or those files change; `tests/test_mp_fontbake.cpp` fails for a character a face lacks.

## What it keeps

| Face | Kept |
|---|---|
| Kurale Regular, Noto Sans Bold | Latin-1, the Turkish and French letters of Latin Extended-A, typographic punctuation, Cyrillic (U+0400-045F, and Ghe with upturn), and every character of the strings routed there |
| Noto Sans JP Bold | kana and the CJK punctuation (U+3000-30FF), the full-width forms (U+FF01-FF5E), every character of the Japanese strings, and the 1,026 elementary-school (kyoiku) kanji, so that a native speaker's edit has room |
| Noto Sans Arabic Bold | the Arabic letters (U+0620-064A), the Presentation Forms (U+FB50-FBFF, U+FE70-FEFF: what `game/sim/ArabicShaping` turns letters into, the lam-alef ligature among them), the Arabic-Indic digits, ASCII |

The layout tables (GSUB, GPOS, GDEF), hinting and the vertical metrics go: stb_truetype reads none of them and the shaping is the game's own. Name IDs 0-6, 13, 14,
16 and 17 stay, so each file carries its upstream name and licence. The three Noto faces are variable fonts: the tool instances them at bold (weight 700, width 100)
with fontTools' instancer.

The elementary-school kanji list is grades 1-6 of KANJIDIC2 as `davidluzgouveia/kanji-data` (MIT) carries it at a pinned commit, plus the twenty
prefecture-name kanji the 2017 revision of the curriculum moved into grade 4 (1,006 + 20 = 1,026). Only the selection of glyphs comes from it; none of its data ships.

## Reserved Font Names

The OFL text of Noto Sans JP names 'Source' (Adobe's) as a Reserved Font Name; the family name "Noto Sans JP" does not contain it, and the tool refuses to write the
file if any of its family, full, PostScript or typographic family names does. The other three declare none.

## The achievements in the interface languages (`make_achievement_languages.py`)

The 82 achievements' titles and descriptions and the secret achievement's title and sentence are Asantee's words, so their translations are derivative of them and are
NOT in git. The source of truth is `out/data/lang_src/<code>.json` (gitignored; one file per language but English, `{ui, secret, achievements, glossary, notes}` as the
translators and the reviewers handed them over: machine-translated from the English by one model and reviewed by a second, **not by a native speaker**; `secret` is
`{"title": "...", "description": "... {world}-{level} ..."}`). The tool turns the `secret` and the `achievements` of each into `out/data/achievements.<code>.json`, the file
the game reads (beside `achievements.json`, a `secret` block and the achievements keyed by id, no English in it):

```
python -I tools/l10n/make_achievement_languages.py            # writes ten files
python -I tools/l10n/make_achievement_languages.py --check    # validates, writes nothing
python -I -m unittest discover -s tools/l10n -p "test_make_achievement_languages.py"
```

A language is written only if all of it passes: the ids are exactly the English file's; every text is a non-empty string with no control character (a description may hold
line breaks) and no U+FFFD; the numbers are the English's (the same digit runs in any order, ASCII digits only); and no description has more lines than the English's longest.
The `secret` block is exactly `{title, description}`; its title holds no brace and no line break, its description holds `{world}` and `{level}` once each and no other brace,
neither holds a digit, and the description has no more lines than the English secret sentence; and for a language whose `font_set` in `game/data/lang/languages.json` is
`original` (drawn with the original's bitmap fonts) every character of the block is in U+0020-007E or U+00A0-00FF, as the committed interface strings are held to.
Nothing is half written and a failing run leaves the files that were there. The files ship only in a release's content package (`tools/package/closure.py`,
`tables/achievements.<code>.json`). Rerun `make_fonts.py` after changing them: the Japanese face must hold every kanji they use (`tests/test_mp_fontbake.cpp` checks it).
