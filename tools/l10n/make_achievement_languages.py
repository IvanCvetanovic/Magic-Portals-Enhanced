"""The achievements in the interface languages: out/data/achievements.<code>.json, from out/data/lang_src/<code>.json.

    python -I tools/l10n/make_achievement_languages.py [--src DIR] [--english FILE] [--languages FILE] [--out DIR] [--check]

Magic Portals' 82 achievements (titles and descriptions) and its secret achievement's two texts (its title, and the sentence with the
chapter and the level) are Asantee's words, so their translations are derivative of them and are NOT in git: the sources and the files this writes
live in the gitignored out/ (and ship only in a release's content package, tools/package/closure.py). The 14 short interface strings are the port's
own and ARE committed (game/data/lang/<code>.json); they are not touched here.

  SOURCE OF TRUTH   out/data/lang_src/<code>.json, one per language but English:
                      {"ui": {...}, "secret": {"title": "...", "description": "... {world}-{level}"},
                       "achievements": {"<id>": {"title": "...", "description": "..."}}, "glossary": ..., "notes": ...}
                    as the translators and the reviewers handed them over (machine-translated from English by one model and reviewed by a second;
                    NOT reviewed by a native speaker). Only "secret" and "achievements" are read here.
  ENGLISH           out/data/achievements.json, tools/asbc/achievements.py's reading of the original: the ids, and the digits each text carries,
                    and how many lines its secret sentence has.
  WRITES            out/data/achievements.<code>.json for each language game/data/lang/languages.json lists after English:
                      {"format": 1, "_about": [...], "language": "<code>", "secret": {"title": "...", "description": "..."},
                       "achievements": {"<id>": {"title": "...", "description": "..."}, ...}}
                    keyed by id, no English in it, ids ascending, UTF-8 without a byte-order mark and without \\u escapes, one trailing newline.
                    The same inputs give the same bytes. sim/Achievements.cpp's LoadTranslation reads it.

A language is written only if EVERY check passes (nothing is half written, and a failing run leaves the files that were there):
  - the ids are exactly the English file's (all 82, none missing, none extra);
  - every title and description is a non-empty string with no control character but a newline in a description, and no leading or trailing space;
  - the digits are the English's: the same digit runs, in any order (a translation may move "level 2-14" to the front of the sentence), all ASCII
    digits - a translation that wrote Arabic-Indic digits, or dropped a number, or invented one, is refused;
  - a description is no more lines than the longest English one (the dashboard and the popup do not wrap: a hard line break is the only one);
  - the secret block is exactly {title, description}, both whole texts as above; the title holds no brace and no line break, the description
    holds {world} and {level} once each and no other brace, neither holds a digit (the numbers are the placeholders), and the description is no
    more lines than the English secret sentence;
  - for a language whose font_set in game/data/lang/languages.json is "original" (it is drawn with the original's bitmap fonts), every character of
    the secret block is in the original fonts' set, U+0020-007E and U+00A0-00FF (a description's line break excepted): the same rule
    tests/test_mp_strings.cpp holds the committed interface strings to. The achievements are not held to it here.
Exit status 0 when every language passed, 1 otherwise; --check validates and writes nothing.
"""
import argparse
import json
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DIGITS = re.compile(r"[0-9]+")
ANY_DIGITS = re.compile(r"\d+")
BRACES = re.compile(r"[{}]")
PLACEHOLDER = re.compile(r"\{([a-z_]+)\}")
SECRET_PLACEHOLDERS = ["level", "world"]  # sorted: each once in the secret sentence (sim/Strings.hpp's Placeholder::kWorld and kLevel)
# The original's fonts hold exactly these code points (sim/Strings.hpp's InOriginalFonts: ids 32-126 and 160-255 of every .fnt).
ORIGINAL_FONT_RANGES = ((0x20, 0x7E), (0xA0, 0xFF))

ABOUT = [
    "The achievements' titles and descriptions, and the secret achievement's title and sentence, of Magic Portals (Asantee Games) in one of Magic Portals Enhanced's interface languages.",
    "Derivative of Asantee's English text: NOT to be committed to git (out/ is gitignored); it ships only in a release's content package.",
    "Machine-translated by a language model from the English, terms aligned with the Magic Rampage Companion app's, reviewed by a second model pass,",
    "NOT reviewed by a native speaker. Written by tools/l10n/make_achievement_languages.py from out/data/lang_src/<code>.json.",
]


def load_json(path):
    with open(path, encoding="utf-8") as handle:
        return json.load(handle)


def english_ids(path):
    """{id: (title, description)} of the English file, or exits with the reason."""
    english = load_json(path)
    entries = english.get("achievements")
    if not isinstance(entries, list) or not entries:
        raise SystemExit(f"{path}: no achievements list (tools/asbc/achievements.py writes format 1)")
    return {int(entry["id"]): (entry["title"], entry["description"]) for entry in entries}


def english_secret_lines(path):
    """How many lines the English secret sentence has (its prefix holds the line break), or exits with the reason."""
    secret = load_json(path).get("secret")
    if not isinstance(secret, dict) or not isinstance(secret.get("description_prefix"), str):
        raise SystemExit(f"{path}: no secret block with a description_prefix (tools/asbc/achievements.py writes format 1)")
    return secret["description_prefix"].count("\n") + 1


def language_fonts(path):
    """[(code, font_set)] after the first (English) in the committed table of languages, in its order."""
    table = load_json(path)
    entries = table["languages"]
    if not entries or entries[0]["code"] != "en":
        raise SystemExit(f"{path}: the first language must be en")
    return [(entry["code"], entry.get("font_set", "")) for entry in entries[1:]]


def language_codes(path):
    """The codes after the first (English) in the committed table of languages, in its order."""
    return [code for code, _ in language_fonts(path)]


def check_text(kind, id_, text, errors, code):
    where = f"{code}: " + ("the " if id_ is None else f"achievement {id_} ") + kind
    if not isinstance(text, str) or not text.strip():
        errors.append(f"{where} is not a non-empty string")
        return False
    if text != text.strip():
        errors.append(f"{where} has a leading or trailing space or line break")
    for char in text:
        if ord(char) < 0x20 and not (char == "\n" and kind.endswith("description")):
            errors.append(f"{where} holds the control character U+{ord(char):04X}")
            break
        if char == "�":
            errors.append(f"{where} holds U+FFFD, a damaged character")
            break
    return True


def digit_runs(text):
    return sorted(DIGITS.findall(text))


def in_original_fonts(char):
    return any(low <= ord(char) <= high for low, high in ORIGINAL_FONT_RANGES)


def validate_secret(code, secret, max_lines, font_set, errors):
    """Appends the problems of one language's `secret` block to `errors`."""
    if not isinstance(secret, dict) or set(secret) != {"title", "description"}:
        errors.append(f"{code}: the secret block is not exactly {{title, description}}")
        return
    title, description = secret["title"], secret["description"]
    whole = check_text("secret title", None, title, errors, code)
    whole = check_text("secret description", None, description, errors, code) and whole
    if not whole:
        return
    where = f"{code}: the secret"
    if "\n" in title:
        errors.append(f"{where} title has a line break")
    if BRACES.search(title):
        errors.append(f"{where} title holds a brace")
    holds = sorted(PLACEHOLDER.findall(description))
    if holds != SECRET_PLACEHOLDERS or len(BRACES.findall(description)) != 2 * len(SECRET_PLACEHOLDERS):
        errors.append(f"{where} description must hold {{world}} and {{level}} once each and no other brace: {description!r}")
    for kind, text in (("title", title), ("description", description)):
        if ANY_DIGITS.search(text):
            errors.append(f"{where} {kind} holds a digit (the numbers are the placeholders): {text!r}")
    if description.count("\n") + 1 > max_lines:
        errors.append(f"{where} description is {description.count(chr(10)) + 1} lines, the English has {max_lines}")
    if font_set == "original":
        for kind, text in (("title", title), ("description", description)):
            bad = sorted({char for char in text if not in_original_fonts(char) and not (char == "\n" and kind == "description")})
            if bad:
                listed = " ".join(f"U+{ord(char):04X}" for char in bad)
                errors.append(f"{where} {kind} holds {listed}, outside the original fonts' set (an original-font language): {text!r}")


def validate(code, source, english, max_lines, secret_max_lines=2, font_set="generated"):
    """The problems of one language's source, as a list of lines (empty: it passes), and its entries by id."""
    errors = []
    validate_secret(code, source.get("secret"), secret_max_lines, font_set, errors)
    entries = source.get("achievements")
    if not isinstance(entries, dict):
        return errors + [f"{code}: no \"achievements\" object"], {}
    ids = {}
    for key, value in entries.items():
        try:
            ids[int(key)] = value
        except ValueError:
            errors.append(f"{code}: the key {key!r} is no achievement id")
    missing = sorted(set(english) - set(ids))
    extra = sorted(set(ids) - set(english))
    if missing:
        errors.append(f"{code}: missing ids {missing}")
    if extra:
        errors.append(f"{code}: ids the English file does not have {extra}")
    for id_ in sorted(set(ids) & set(english)):
        value = ids[id_]
        if not isinstance(value, dict) or set(value) != {"title", "description"}:
            errors.append(f"{code}: achievement {id_} is not exactly {{title, description}}")
            continue
        title, description = value["title"], value["description"]
        ok = check_text("title", id_, title, errors, code)
        ok = check_text("description", id_, description, errors, code) and ok
        if not ok:
            continue
        for kind, text, want in (("title", title, english[id_][0]), ("description", description, english[id_][1])):
            if ANY_DIGITS.findall(text) != DIGITS.findall(text):
                errors.append(f"{code}: achievement {id_} {kind} has digits that are not ASCII 0-9: {text!r}")
            elif digit_runs(text) != digit_runs(want):
                errors.append(f"{code}: achievement {id_} {kind} has the numbers {digit_runs(text)} where the English has {digit_runs(want)}")
        if description.count("\n") + 1 > max_lines:
            errors.append(f"{code}: achievement {id_} description is {description.count(chr(10)) + 1} lines, the English has at most {max_lines}")
        if "\n" in title:
            errors.append(f"{code}: achievement {id_} title has a line break")
    return errors, ids


def render(code, ids, secret):
    body = {str(id_): {"title": ids[id_]["title"], "description": ids[id_]["description"]} for id_ in sorted(ids)}
    document = {"format": 1, "_about": ABOUT, "language": code,
                "secret": {"title": secret["title"], "description": secret["description"]}, "achievements": body}
    return json.dumps(document, ensure_ascii=False, indent=1) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--src", default=os.path.join(REPO, "out", "data", "lang_src"))
    parser.add_argument("--english", default=os.path.join(REPO, "out", "data", "achievements.json"))
    parser.add_argument("--languages", default=os.path.join(REPO, "game", "data", "lang", "languages.json"))
    parser.add_argument("--out", default=os.path.join(REPO, "out", "data"))
    parser.add_argument("--check", action="store_true", help="validate only, write nothing")
    args = parser.parse_args()

    english = english_ids(args.english)
    max_lines = max(description.count("\n") + 1 for _, description in english.values())
    secret_lines = english_secret_lines(args.english)
    failed = 0
    for code, font_set in language_fonts(args.languages):
        path = os.path.join(args.src, code + ".json")
        if not os.path.isfile(path):
            print(f"{code}: no source at {path}")
            failed += 1
            continue
        source = load_json(path)
        errors, ids = validate(code, source, english, max_lines, secret_lines, font_set)
        if errors:
            failed += 1
            print(f"{code}: {len(errors)} problem(s), nothing written")
            for line in errors[:40]:
                print("  " + line)
            continue
        text = render(code, ids, source["secret"])
        target = os.path.join(args.out, f"achievements.{code}.json")
        verb = "checked"
        if not args.check:
            os.makedirs(args.out, exist_ok=True)
            with open(target, "w", encoding="utf-8", newline="\n") as handle:
                handle.write(text)
            verb = "wrote"
        print(f"{code}: {verb} {len(ids)} achievements" + ("" if args.check else f" -> {target}"))
    if failed:
        print(f"{failed} language(s) failed")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
