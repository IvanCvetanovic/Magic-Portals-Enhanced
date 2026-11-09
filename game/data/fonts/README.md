# Bundled fonts: the interface languages' four faces

Magic Portals draws its text from the original's own bitmap fonts (Matura MT Script Capitals and Verdana, in the original's
assets; they are the original's and are never copied here). Those hold Latin-1 only, so a language written in another script -
Russian, Ukrainian, Turkish (its g-breve, dotless i, s-cedilla), Japanese, Arabic - is drawn from an atlas the game makes at run
time from the four faces in this folder (`game/sim/FontBake`). The atlases are never committed: they are made into the scratch
folder. Which face draws which script in which slot is data: `game/data/lang/languages.json`, `fonts`.

| Slot | Latin and Cyrillic | Japanese | Arabic |
|---|---|---|---|
| Matura's (the words) | `Kurale-Regular.ttf` | `NotoSansJP-Bold.ttf` | `NotoSansArabic-Bold.ttf` |
| Verdana's (achievements, "new!") | `NotoSans-Bold.ttf` | `NotoSansJP-Bold.ttf` | `NotoSansArabic-Bold.ttf` |

No Windows font and nothing derived from Matura or Verdana ships. Kurale is the nearest open-licence look to Matura's swash
capitals that Penumbra's study of twelve faces found (its `tools/art/make_localized_art.py`), and it has the Cyrillic and Turkish
letters Matura lacks; Noto Sans Bold stands in for the original's bold Verdana.

Each file is a subset (and, for the three Noto variable fonts, an instance at bold) of the upstream file under the SIL Open Font
License 1.1, which allows it as a Modified Version provided no Reserved Font Name is used: none of the four declares one its own
name uses (Noto Sans JP's OFL text names 'Source', Adobe's, which "Noto Sans JP" does not contain; `make_fonts.py` checks the
subset's name table for it). Each licence text travels with its file: `OFL-<name>.txt`, verbatim from the upstream repository.
`NOTICE.md` lists them.

`tools/l10n/make_fonts.py` makes all of it from pinned downloads (README there); this section below is its own and is rewritten.

<!-- make_fonts.py: begin -->
| File | Source | Made | Licence |
|---|---|---|---|
| `Kurale-Regular.ttf` | google/fonts `ofl/kurale/Kurale-Regular.ttf` at `66a36c8c94b1a5d992ee4e7f392fccfe4945767c`, sha256 `6055aaca6870ca64210dd81fa47f8bb71595a9c8fbe570351ed45a15f1a0820d` | subset, instance: none (static); 316 characters, 323 glyphs, 44,880 bytes, sha256 `d2dbcd25c5928fe010bd24923f9e0f7d38ddbf411a0035915afcc7df71d6c6f8`; name table `Kurale Regular` | SIL Open Font License 1.1 (`OFL-Kurale.txt`) |
| `NotoSans-Bold.ttf` | google/fonts `ofl/notosans/NotoSans[wdth,wght].ttf` at `66a36c8c94b1a5d992ee4e7f392fccfe4945767c`, sha256 `bfb7bb691513f12e734dc346c03a03f784912432d7e3fa8e56efcf906fe86b3d` | subset, instance: wght 700, wdth 100; 321 characters, 333 glyphs, 26,084 bytes, sha256 `aff264058074faaaa5d458ff1d500c191f42bcb8d7a8b905a42abc97f41b4341`; name table `Noto Sans Bold` | SIL Open Font License 1.1 (`OFL-NotoSans.txt`) |
| `NotoSansJP-Bold.ttf` | google/fonts `ofl/notosansjp/NotoSansJP[wght].ttf` at `66a36c8c94b1a5d992ee4e7f392fccfe4945767c`, sha256 `c2f3b4d463500a2ddcd3849cded1fceeb9fd6d1c32e6cbecd568453ba50fc68f` | subset, instance: wght 700; 1383 characters, 1383 glyphs, 328,820 bytes, sha256 `75f2a890e687f6dbc1905d9b6941c1cee7a5dab6f3f5d7ad790b393d931485a4`; name table `Noto Sans JP Bold` | SIL Open Font License 1.1 (`OFL-NotoSansJP.txt`) |
| `NotoSansArabic-Bold.ttf` | google/fonts `ofl/notosansarabic/NotoSansArabic[wdth,wght].ttf` at `66a36c8c94b1a5d992ee4e7f392fccfe4945767c`, sha256 `63111b5b2e074dd48cc67692e0a2726d86ee94c1c37fe8598257b7b4e87e869e` | subset, instance: wght 700, wdth 100; 453 characters, 481 glyphs, 34,648 bytes, sha256 `55a1d0016491db6d73237a7877c27c7019e11a4ddcfe4809f45edc72f3c5faf5`; name table `Noto Sans Arabic Bold` | SIL Open Font License 1.1 (`OFL-NotoSansArabic.txt`) |

Characters kept from the language files (12 files in game/data/lang) and from the translations of --texts (10 files, the achievements' translations, which are not in git): 386 distinct, of them 156 for the text faces, 196 for Japanese (101 kanji) and 34 for Arabic.
Japanese also keeps the 1026 elementary-school kanji (00fd7079c389 of davidluzgouveia/kanji-data, grades 1-6 of KANJIDIC2, plus the twenty prefecture-name kanji of the 2017 revision; sha256 `561b72ea9df703c58fae0c68585812e172aaa7611d2fcee6719d55fd2dfd4fa4`).
Made with fontTools 4.68.0.
<!-- make_fonts.py: end -->
