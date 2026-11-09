"""The four faces of the interface languages: game/data/fonts/*.ttf, subset from their OFL upstreams.

    python tools/l10n/make_fonts.py [--cache DIR] [--texts PATH ...]

Run it with the venv of tools/l10n/README.md (fontTools). Magic Portals' own fonts (Matura MT and Verdana, bitmaps in the
original's assets) hold Latin-1 only, so a language whose text is outside it - Russian, Ukrainian, Turkish, Japanese,
Arabic - is drawn from an atlas the game makes at run time (game/sim/FontBake) from these faces. The routing, which face
draws which script in which slot, is DATA: game/data/lang/languages.json, `fonts`.

  Kurale-Regular.ttf     Matura's slot: Latin (Latin-1, Latin Extended-A, so Turkish) and Cyrillic, upright calligraphic
                         serif, the nearest open-licence look to Matura's swash capitals Penumbra's study found
                         (its tools/art/make_localized_art.py rejected twelve others).
  NotoSans-Bold.ttf      Verdana's slot: the same scripts, instanced at wght 700 (the original's Verdana is bold).
  NotoSansJP-Bold.ttf    both slots: kana, CJK punctuation (no ASCII: Latin routes to the slot's text face), the full-width forms and kanji - every kanji of the Japanese
                         translations plus the 1,026 elementary-school (kyoiku) kanji, so that a native speaker's edit
                         has room - instanced at wght 700.
  NotoSansArabic-Bold.ttf  both slots: the Arabic letters and the Presentation Forms the shaper (game/sim/ArabicShaping)
                         turns them into, instanced at wght 700, wdth 100.

All four come from Google's fonts repository at one pinned commit, every download checked against its sha256. A subset or
an instance is an OFL "Modified Version": each file keeps its upstream name (none of the four declares a Reserved Font
Name its name uses; Noto Sans JP's OFL text names 'Source', which the family name does not contain - checked below), and
its OFL text goes beside it as OFL-<name>.txt. The layout tables go (GSUB, GPOS, GDEF: stb_truetype reads none of them,
and the shaping is the game's own) and so does hinting; name IDs 0-6 stay so the licence and the names survive.

Rerun it whenever a language file changes: the font coverage check of tests/test_mp_fontbake.cpp fails for a character a
face lacks. Reads game/data/lang/*.json and every JSON in --texts (default out/data, achievements.<code>.json), keeping
every character a string of theirs routes to a face. The timestamps are left as upstream set them, so a rerun on the same
inputs and the same fontTools writes the same bytes.
"""
import argparse
import hashlib
import io
import json
import os
import sys
import urllib.request

from fontTools import subset
from fontTools import version as fonttools_version
from fontTools.ttLib import TTFont
from fontTools.varLib import instancer

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
FONTS = os.path.join(REPO, "game", "data", "fonts")
LANG = os.path.join(REPO, "game", "data", "lang")
DEFAULT_CACHE = os.path.join(REPO, "out", "l10n-cache")
DEFAULT_TEXTS = os.path.join(REPO, "out", "data")

COMMIT = "66a36c8c94b1a5d992ee4e7f392fccfe4945767c"  # google/fonts, 2026-03-25 (the commit Penumbra's tools/l10n pins)
RAW = f"https://raw.githubusercontent.com/google/fonts/{COMMIT}"

# id -> what to fetch and make. `location` is the instancer's axis position (None: not a variable font).
FACES = {
    "kurale": {
        "font": (f"{RAW}/ofl/kurale/Kurale-Regular.ttf", "6055aaca6870ca64210dd81fa47f8bb71595a9c8fbe570351ed45a15f1a0820d"),
        "ofl": (f"{RAW}/ofl/kurale/OFL.txt", "b712dacdda5da76c0213f8478886f893b855665c1153ae438bc3e55a7dc525ef"),
        "upstream": "ofl/kurale/Kurale-Regular.ttf",
        "out": "Kurale-Regular.ttf",
        "license": "OFL-Kurale.txt",
        "location": None,
        "family": "Kurale",
    },
    "noto_sans_bold": {
        "font": (f"{RAW}/ofl/notosans/NotoSans%5Bwdth,wght%5D.ttf",
                 "bfb7bb691513f12e734dc346c03a03f784912432d7e3fa8e56efcf906fe86b3d"),
        "ofl": (f"{RAW}/ofl/notosans/OFL.txt", "cee9892f9f0cc8fe882c9e9537ee6a89621d86ee7ceaf70b02e2b2b1c25c061a"),
        "upstream": "ofl/notosans/NotoSans[wdth,wght].ttf",
        "out": "NotoSans-Bold.ttf",
        "license": "OFL-NotoSans.txt",
        "location": {"wght": 700, "wdth": 100},
        "family": "Noto Sans",
    },
    "noto_sans_jp_bold": {
        "font": (f"{RAW}/ofl/notosansjp/NotoSansJP%5Bwght%5D.ttf",
                 "c2f3b4d463500a2ddcd3849cded1fceeb9fd6d1c32e6cbecd568453ba50fc68f"),
        "ofl": (f"{RAW}/ofl/notosansjp/OFL.txt", "1c05c68c34f9708415aada51f17e1b0092d2cea709bf4a94cd38114f9e73d7d9"),
        "upstream": "ofl/notosansjp/NotoSansJP[wght].ttf",
        "out": "NotoSansJP-Bold.ttf",
        "license": "OFL-NotoSansJP.txt",
        "location": {"wght": 700},
        "family": "Noto Sans JP",
        "reserved": "Source",  # the OFL text's Reserved Font Name; no name of the file may contain it
    },
    "noto_sans_arabic_bold": {
        "font": (f"{RAW}/ofl/notosansarabic/NotoSansArabic%5Bwdth,wght%5D.ttf",
                 "63111b5b2e074dd48cc67692e0a2726d86ee94c1c37fe8598257b7b4e87e869e"),
        "ofl": (f"{RAW}/ofl/notosansarabic/OFL.txt", "07fc70bfeb985cc1a87a8587d0a0c80bab11c86c9dc3fd95b6f0cb332f983e96"),
        "upstream": "ofl/notosansarabic/NotoSansArabic[wdth,wght].ttf",
        "out": "NotoSansArabic-Bold.ttf",
        "license": "OFL-NotoSansArabic.txt",
        "location": {"wght": 700, "wdth": 100},
        "family": "Noto Sans Arabic",
    },
}

# Which characters are the elementary-school (kyoiku) kanji: grades 1-6 of KANJIDIC2 as davidluzgouveia/kanji-data (MIT)
# carries them, at a pinned commit. That list is the 2008 one (1,006); the 2017 revision, in force from April 2020, moved
# twenty prefecture-name kanji into grade 4 (MEXT's gakushu shido yoryo), which are added below: 1,026. Only the SELECTION of
# glyphs comes from it; no data of it ships.
KANJI_URL = ("https://raw.githubusercontent.com/davidluzgouveia/kanji-data/00fd7079c3890f430759536f91aa5e854ec0ca4f/kanji.json",
             "561b72ea9df703c58fae0c68585812e172aaa7611d2fcee6719d55fd2dfd4fa4")
KYOIKU_2020 = "茨媛岡潟岐熊香佐埼崎滋鹿縄井沖栃奈梨阪阜"

# Script routing of the game (game/data/lang/languages.json `fonts.scripts`), repeated here for what each face keeps.
LATIN1 = list(range(0x20, 0x7F)) + list(range(0xA0, 0x100))
# Latin Extended-A, only what the eleven languages use: Turkish (G-breve, dotted I, dotless i, S-cedilla), French (OE ligature, Y-diaeresis).
LATIN_EXT_A = [0x11E, 0x11F, 0x130, 0x131, 0x15E, 0x15F, 0x152, 0x153, 0x178]
TEXT_PUNCT = ([0x2010, 0x2011, 0x2012, 0x2013, 0x2014, 0x2015] + list(range(0x2018, 0x201F)) + [0x2020, 0x2021, 0x2022, 0x2026, 0x2030, 0x2039, 0x203A, 0x20AC, 0x2116, 0x2122])
CYRILLIC = list(range(0x0400, 0x0460)) + [0x0490, 0x0491]
JP_BLOCKS = (list(range(0x3000, 0x3100)) + list(range(0xFF01, 0xFF5F)))
AR_LETTERS = (list(range(0x0620, 0x064B)) + [0x060C, 0x061B, 0x061F, 0x0640] + list(range(0x0660, 0x066A)))
AR_FORMS = list(range(0xFB50, 0xFC00)) + list(range(0xFE70, 0xFF00))
AR_ASCII = list(range(0x20, 0x7F)) + [0xA0]

# What must come out mapped whatever the texts say: the shaper's own set (game/sim/ArabicShaping.cpp).
AR_REQUIRED = set(range(0x0621, 0x063B)) | set(range(0x0641, 0x064B)) | set(range(0xFE80, 0xFEFD)) | {0xFBE8, 0xFBE9}

README_BEGIN = "<!-- make_fonts.py: begin -->"
README_END = "<!-- make_fonts.py: end -->"


def script_of(cp):
    """The face a character routes to, as languages.json `fonts.scripts` says: 'jp', 'ar', or the slot's text face."""
    if 0x3000 <= cp <= 0x30FF or 0x31F0 <= cp <= 0x31FF or 0x3400 <= cp <= 0x4DBF or 0x4E00 <= cp <= 0x9FFF or 0xFF00 <= cp <= 0xFFEF:
        return "jp"
    if 0x0600 <= cp <= 0x06FF or 0x0750 <= cp <= 0x077F or 0xFB50 <= cp <= 0xFDFF or 0xFE70 <= cp <= 0xFEFF:
        return "ar"
    return "text"


def fetch(url, sha256, cache):
    name = hashlib.sha256(url.encode()).hexdigest()[:16] + "-" + os.path.basename(url).replace("%5B", "[").replace("%5D", "]")
    path = os.path.join(cache, name)
    if not os.path.exists(path):
        print(f"downloading {url}")
        with urllib.request.urlopen(url, timeout=600) as response:
            data = response.read()
        with open(path, "wb") as f:
            f.write(data)
    with open(path, "rb") as f:
        data = f.read()
    digest = hashlib.sha256(data).hexdigest()
    if digest != sha256:
        sys.exit(f"{url}: sha256 {digest}, expected {sha256}")
    return data


def strings_of(value, out):
    if isinstance(value, str):
        out.append(value)
    elif isinstance(value, dict):
        for k, v in value.items():
            if not k.startswith("_"):
                strings_of(v, out)
    elif isinstance(value, list):
        for v in value:
            strings_of(v, out)


def texts_characters(paths):
    """Every character of every string (notes, keys starting with '_', excepted) of the JSON files at `paths`."""
    files = []
    for path in paths:
        if os.path.isdir(path):
            files += [os.path.join(path, n) for n in sorted(os.listdir(path)) if n.endswith(".json")]
        elif os.path.isfile(path):
            files.append(path)
    chars = set()
    used = []
    for path in files:
        with open(path, encoding="utf-8") as f:
            doc = json.load(f)
        found = []
        strings_of(doc, found)
        chars.update(ord(c) for text in found for c in text)
        # Relative to the repository, with forward slashes; a file outside it by its name alone (the README is committed).
        used.append(os.path.relpath(path, REPO).replace(os.sep, "/") if path.startswith(REPO) else os.path.basename(path))
    return chars, used


def kyoiku(cache):
    data = json.loads(fetch(*KANJI_URL, cache).decode("utf-8"))
    kanji = {ord(ch) for ch, entry in data.items() if entry.get("grade") in (1, 2, 3, 4, 5, 6)}
    kanji |= {ord(ch) for ch in KYOIKU_2020}
    if len(kanji) != 1026:
        sys.exit(f"the kyoiku list has {len(kanji)} kanji, expected 1026")
    return kanji


def build(key, unicodes, required, cache):
    source = FACES[key]
    raw = fetch(*source["font"], cache)
    font = TTFont(io.BytesIO(raw), recalcTimestamp=False)
    options = subset.Options()
    options.layout_features = []
    options.drop_tables += ["GSUB", "GPOS", "GDEF", "BASE", "JSTF", "MATH", "DSIG", "vhea", "vmtx", "VORG"]
    options.hinting = False
    options.notdef_outline = True
    options.name_IDs = [0, 1, 2, 3, 4, 5, 6, 13, 14, 16, 17]
    options.name_legacy = True
    options.recalc_timestamp = False
    subsetter = subset.Subsetter(options)
    subsetter.populate(unicodes=sorted(unicodes))
    subsetter.subset(font)
    if source["location"]:
        # Named for the instance ("Noto Sans Bold"), not the default one, from the STAT table.
        font = instancer.instantiateVariableFont(font, source["location"], updateFontNames=True)
    font.recalcTimestamp = False
    out = os.path.join(FONTS, source["out"])
    font.save(out)
    check = TTFont(out)
    cmap = check.getBestCmap()
    missing = sorted(cp for cp in required if cp not in cmap)
    if "fvar" in check:
        sys.exit(f"{out}: still variable")
    names = {n.nameID: n.toUnicode() for n in check["name"].names if n.platformID == 3}
    reserved = source.get("reserved")
    if reserved:
        for name_id in (1, 4, 6, 16):
            if name_id in names and reserved.lower() in names[name_id].lower():
                sys.exit(f"{out}: name {name_id} {names[name_id]!r} contains the Reserved Font Name {reserved!r}")
    ofl = fetch(*source["ofl"], cache)
    with open(os.path.join(FONTS, source["license"]), "wb") as f:
        f.write(ofl)
    with open(out, "rb") as f:
        data = f.read()
    return {
        "key": key,
        "file": source["out"],
        "license": source["license"],
        "upstream": source["upstream"],
        "upstream_sha256": source["font"][1],
        "sha256": hashlib.sha256(data).hexdigest(),
        "bytes": len(data),
        "glyphs": check["maxp"].numGlyphs,
        "mapped": len(cmap),
        "missing": missing,
        "location": ", ".join(f"{k} {v}" for k, v in source["location"].items()) if source["location"] else "none (static)",
        "full_name": names.get(4, ""),
    }


def write_readme(results, notes):
    path = os.path.join(FONTS, "README.md")
    rows = []
    for r in results:
        rows.append(f"| `{r['file']}` | google/fonts `{r['upstream']}` at `{COMMIT}`, sha256 `{r['upstream_sha256']}` | "
                    f"subset, instance: {r['location']}; {r['mapped']} characters, {r['glyphs']} glyphs, {r['bytes']:,} bytes, "
                    f"sha256 `{r['sha256']}`; name table `{r['full_name']}` | SIL Open Font License 1.1 (`{r['license']}`) |")
    block = "\n".join([README_BEGIN, "| File | Source | Made | Licence |", "|---|---|---|---|", *rows, "", *notes, README_END])
    with open(path, encoding="utf-8") as f:
        text = f.read()
    if README_BEGIN not in text or README_END not in text:
        sys.exit(f"{path}: the generated block's markers are missing")
    text = text[:text.index(README_BEGIN)] + block + text[text.index(README_END) + len(README_END):]
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--cache", default=DEFAULT_CACHE, help="where the downloads are kept between runs (default out/l10n-cache)")
    parser.add_argument("--texts", nargs="*", default=[DEFAULT_TEXTS],
                        help="more JSON files or folders whose strings decide the glyphs, besides game/data/lang "
                             "(default out/data: the achievements' translations, which are not in git)")
    args = parser.parse_args(argv)
    os.makedirs(args.cache, exist_ok=True)

    lang_chars, lang_used = texts_characters([LANG])
    more_chars, more_used = texts_characters(args.texts)
    drawn = {cp for cp in lang_chars | more_chars if cp >= 0x20}
    by_face = {"text": set(), "jp": set(), "ar": set()}
    for cp in drawn:
        by_face[script_of(cp)].add(cp)
    # A native name in the picker, language file or not, is in the language table, which texts_characters read.

    text_set = set(LATIN1) | set(LATIN_EXT_A) | set(TEXT_PUNCT) | set(CYRILLIC) | by_face["text"]
    kyoiku_set = kyoiku(args.cache)
    kanji_used = {cp for cp in by_face["jp"] if 0x3400 <= cp <= 0x9FFF}
    jp_set = set(JP_BLOCKS) | by_face["jp"] | kyoiku_set
    ar_set = set(AR_LETTERS) | set(AR_FORMS) | set(AR_ASCII) | by_face["ar"]
    results = [
        build("kurale", text_set, by_face["text"], args.cache),
        build("noto_sans_bold", text_set, by_face["text"], args.cache),
        build("noto_sans_jp_bold", jp_set, by_face["jp"] | kyoiku_set | set(range(0x3041, 0x3097)) | set(range(0x30A1, 0x30FB)), args.cache),
        build("noto_sans_arabic_bold", ar_set, by_face["ar"] | AR_REQUIRED, args.cache),
    ]
    bad = False
    for r in results:
        print(f"{r['file']}: {r['mapped']} characters, {r['glyphs']} glyphs, {r['bytes']:,} bytes, sha256 {r['sha256']}")
        if r["missing"]:
            bad = True
            print(f"  NOT in the upstream face: {' '.join(f'U+{cp:04X}' for cp in r['missing'][:60])}")
    notes = [
        f"Characters kept from the language files ({len(lang_used)} files in game/data/lang) and from the translations of --texts "
        f"({len(more_used)} files, the achievements' translations, which are not in git): {len(drawn)} distinct, of them {len(by_face['text'])} for the text faces, "
        f"{len(by_face['jp'])} for Japanese ({len(kanji_used)} kanji) and {len(by_face['ar'])} for Arabic.",
        f"Japanese also keeps the {len(kyoiku_set)} elementary-school kanji ({KANJI_URL[0].split('/')[-2][:12]} of davidluzgouveia/kanji-data, "
        f"grades 1-6 of KANJIDIC2, plus the twenty prefecture-name kanji of the 2017 revision; sha256 `{KANJI_URL[1]}`).",
        f"Made with fontTools {fonttools_version}.",
    ]
    write_readme(results, notes)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
