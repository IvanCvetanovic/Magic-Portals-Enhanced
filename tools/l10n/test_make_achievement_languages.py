"""Tests of make_achievement_languages.py: the checks a translation must pass and what is written.

    python -I -m unittest discover -s tools/l10n -p "test_make_achievement_languages.py"

Every text here is synthetic: no Asantee word is in this file (the real sources are in the gitignored out/data/lang_src).
"""
import importlib.util
import io
import json
import os
import contextlib
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
SPEC = importlib.util.spec_from_file_location("make_achievement_languages", os.path.join(HERE, "make_achievement_languages.py"))
tool = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(tool)

ENGLISH = {
    0: ("Made-up badge 1", "Made-up 30% done"),
    1: ("Made-up 5-9", "Do a made-up thing in level 5-9\nwith 3 tries or fewer"),
    3: ("Plain", "Do the thing"),
}


def read_bytes(path):
    with open(path, "rb") as handle:
        return handle.read()


def good_source():
    return {
        "ui": {},
        "secret": {"title": "Erfundenes Raetsel", "description": "Erfunden aus\nLevel {world}-{level}"},
        "achievements": {
            "0": {"title": "Erfundenes Abzeichen 1", "description": "Erfundene 30% fertig"},
            "1": {"title": "Erfunden 5-9", "description": "Mit 3 Versuchen oder weniger\nErfundenes in Level 5-9"},
            "3": {"title": "Schlicht", "description": "Tu es"},
        },
    }


def problems(source, code="de", font_set="generated"):
    errors, _ = tool.validate(code, source, ENGLISH, max_lines=2, secret_max_lines=2, font_set=font_set)
    return errors


class Validation(unittest.TestCase):
    def test_a_good_source_passes(self):
        self.assertEqual(problems(good_source()), [])

    def test_numbers_may_change_places_but_not_value(self):
        source = good_source()
        # "3 ... 5-9" in the other order is the language's business.
        source["achievements"]["1"]["description"] = "Level 5-9 mit\n3 Versuchen oder weniger"
        self.assertEqual(problems(source), [])
        source["achievements"]["1"]["description"] = "Level 5-9 mit\n4 Versuchen oder weniger"
        self.assertTrue(any("numbers" in line for line in problems(source)))
        source["achievements"]["1"]["description"] = "Level mit\n3 Versuchen oder weniger"
        self.assertTrue(any("numbers" in line for line in problems(source)))

    def test_digits_must_be_ascii(self):
        source = good_source()
        source["achievements"]["0"]["description"] = "Erfundene ٣٠% fertig"
        self.assertTrue(any("ASCII" in line or "numbers" in line for line in problems(source)))

    def test_ids_are_exactly_the_englishs(self):
        source = good_source()
        del source["achievements"]["3"]
        self.assertTrue(any("missing ids [3]" in line for line in problems(source)))
        source = good_source()
        source["achievements"]["9"] = {"title": "x", "description": "y"}
        self.assertTrue(any("ids the English file does not have [9]" in line for line in problems(source)))
        source = good_source()
        source["achievements"]["first"] = {"title": "x", "description": "y"}
        self.assertTrue(any("no achievement id" in line for line in problems(source)))

    def test_texts_are_whole(self):
        for field in ("title", "description"):
            for bad in ("", "   ", None, 5, " padded", "padded\n"):
                source = good_source()
                source["achievements"]["3"][field] = bad
                self.assertTrue(problems(source), f"{field}={bad!r} must fail")
        source = good_source()
        source["achievements"]["3"]["title"] = "two\nlines"
        self.assertTrue(problems(source))
        source = good_source()
        source["achievements"]["3"]["description"] = "bell\x07"
        self.assertTrue(any("control character" in line for line in problems(source)))
        source = good_source()
        source["achievements"]["3"]["description"] = "damaged �"
        self.assertTrue(any("damaged" in line for line in problems(source)))

    def test_description_lines_are_bounded_by_the_englishs(self):
        source = good_source()
        source["achievements"]["3"]["description"] = "one\ntwo\nthree"
        self.assertTrue(any("lines" in line for line in problems(source)))

    def test_extra_members_and_missing_members_fail(self):
        source = good_source()
        source["achievements"]["3"]["note"] = "x"
        self.assertTrue(problems(source))
        source = good_source()
        del source["achievements"]["3"]["title"]
        self.assertTrue(problems(source))
        self.assertTrue(problems({"ui": {}}))


class SecretBlock(unittest.TestCase):
    """The secret achievement's two texts: Asantee's words too, so they travel in the overlay and are held to rules of their own."""

    def secret_problems(self, secret, font_set="generated"):
        source = good_source()
        if secret is None:
            del source["secret"]
        else:
            source["secret"] = secret
        return problems(source, font_set=font_set)

    def test_a_good_secret_passes_in_both_font_sets(self):
        self.assertEqual(self.secret_problems(good_source()["secret"]), [])
        self.assertEqual(self.secret_problems(good_source()["secret"], "original"), [])

    def test_the_block_must_be_there_and_exactly_two_members(self):
        self.assertTrue(any("secret block" in line for line in self.secret_problems(None)))
        self.assertTrue(self.secret_problems({"title": "Erfunden"}))
        self.assertTrue(self.secret_problems({**good_source()["secret"], "icon": "x.png"}))
        self.assertTrue(self.secret_problems("a string"))

    def test_the_sentence_holds_both_placeholders_once_and_no_other_brace(self):
        title = "Erfunden"
        for description in ("Level {world}", "Level {level}", "Level {world}-{world}", "Level {world}-{level}-{level}", "Level {world}-{level} {n}",
                            "Level {world}-{level} {", "Level {world}-{level} }", "Level 1-2", "Level {World}-{level}"):
            errors = self.secret_problems({"title": title, "description": description})
            self.assertTrue(any("once each" in line for line in errors), f"{description!r}: {errors}")
        # In the other order is the language's business.
        self.assertEqual(self.secret_problems({"title": title, "description": "{level} Level {world}"}), [])

    def test_the_title_holds_no_brace_no_line_break_and_no_digit(self):
        for title in ("Erfun{world}den", "Erfunden}", "Zwei\nZeilen", "Erfunden 2"):
            self.assertTrue(self.secret_problems({"title": title, "description": "Level {world}-{level}"}), repr(title))

    def test_no_digit_in_the_sentence_and_no_more_lines_than_the_englishs(self):
        self.assertTrue(any("digit" in line for line in self.secret_problems({"title": "Erfunden", "description": "Level 3 {world}-{level}"})))
        self.assertTrue(any("lines" in line for line in self.secret_problems({"title": "Erfunden", "description": "Eins\nZwei\nDrei {world}-{level}"})))

    def test_texts_must_be_whole(self):
        for bad in ("", "  ", " padded", "padded ", None, 5):
            self.assertTrue(self.secret_problems({"title": bad, "description": "Level {world}-{level}"}), repr(bad))
            self.assertTrue(self.secret_problems({"title": "Erfunden", "description": bad}), repr(bad))
        self.assertTrue(any("damaged" in line for line in self.secret_problems({"title": "Erfunden \ufffd", "description": "Level {world}-{level}"})))

    def test_an_original_font_language_keeps_to_the_original_fonts_set(self):
        # Latin-1 is in the set (the umlaut, the accented e), the typographic quote, the ellipsis, the dash and everything outside Latin-1 are not.
        self.assertEqual(self.secret_problems({"title": "Fr\u00fchst\u00fcck \u00e0", "description": "Erfunden \u00e9\nLevel {world}-{level}"}, "original"), [])
        for char in ("\u2019", "\u2026", "\u2013", "\u0153", "\u0131", "\u015f", "\u0410", "\u3042", "\u0627"):
            errors = self.secret_problems({"title": f"Erfunden{char}", "description": "Level {world}-{level}"}, "original")
            self.assertTrue(any("original fonts' set" in line and f"U+{ord(char):04X}" in line for line in errors), f"U+{ord(char):04X}: {errors}")
            errors = self.secret_problems({"title": "Erfunden", "description": f"Level{char}\n{{world}}-{{level}}"}, "original")
            self.assertTrue(any("original fonts' set" in line for line in errors), f"U+{ord(char):04X}: {errors}")
            # a generated-font language draws them from the faces it bakes
            self.assertEqual(self.secret_problems({"title": f"Erfunden{char}", "description": f"Level{char}\n{{world}}-{{level}}"}, "generated"), [])

    def test_a_line_break_is_no_character_outside_the_set(self):
        self.assertEqual(self.secret_problems({"title": "Erfunden", "description": "Eins\n{world}-{level}"}, "original"), [])


class Writing(unittest.TestCase):
    def make_world(self, root, source):
        os.makedirs(os.path.join(root, "src"))
        os.makedirs(os.path.join(root, "out"))
        with open(os.path.join(root, "english.json"), "w", encoding="utf-8") as handle:
            json.dump({"achievements": [{"id": i, "title": t, "description": d} for i, (t, d) in ENGLISH.items()],
                       "secret": {"description_prefix": "Made-up from\nlevel "}}, handle)
        with open(os.path.join(root, "languages.json"), "w", encoding="utf-8") as handle:
            json.dump({"languages": [{"code": "en", "font_set": "original"}, {"code": "de", "font_set": "original"}]}, handle)
        with open(os.path.join(root, "src", "de.json"), "w", encoding="utf-8") as handle:
            json.dump(source, handle, ensure_ascii=False)
        return ["--src", os.path.join(root, "src"), "--english", os.path.join(root, "english.json"),
                "--languages", os.path.join(root, "languages.json"), "--out", os.path.join(root, "out")]

    def run_tool(self, argv):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            old = tool.sys.argv
            tool.sys.argv = ["make_achievement_languages.py"] + argv
            try:
                status = tool.main()
            finally:
                tool.sys.argv = old
        return status, out.getvalue()

    def test_it_writes_a_sorted_utf8_file_and_the_same_bytes_twice(self):
        with tempfile.TemporaryDirectory() as root:
            argv = self.make_world(root, good_source())
            status, _ = self.run_tool(argv)
            self.assertEqual(status, 0)
            path = os.path.join(root, "out", "achievements.de.json")
            data = read_bytes(path)
            self.assertFalse(data.startswith(b"\xef\xbb\xbf"))
            self.assertTrue(data.endswith(b"\n") and not data.endswith(b"\n\n"))
            self.assertNotIn(b"\\u", data)
            document = json.loads(data.decode("utf-8"))
            self.assertEqual(document["format"], 1)
            self.assertEqual(document["language"], "de")
            self.assertEqual(list(document), ["format", "_about", "language", "secret", "achievements"])
            self.assertEqual(document["secret"], good_source()["secret"])
            self.assertEqual(list(document["achievements"]), ["0", "1", "3"])
            self.assertEqual(set(document["achievements"]["1"]), {"title", "description"})
            self.assertNotIn("ui", document)
            self.run_tool(argv)
            self.assertEqual(read_bytes(path), data)

    def test_a_failing_language_writes_nothing_and_keeps_the_old_file(self):
        with tempfile.TemporaryDirectory() as root:
            source = good_source()
            argv = self.make_world(root, source)
            path = os.path.join(root, "out", "achievements.de.json")
            with open(path, "w", encoding="utf-8") as handle:
                handle.write("old")
            source["achievements"]["3"]["description"] = ""
            with open(os.path.join(root, "src", "de.json"), "w", encoding="utf-8") as handle:
                json.dump(source, handle)
            status, text = self.run_tool(argv)
            self.assertEqual(status, 1)
            self.assertIn("nothing written", text)
            self.assertEqual(read_bytes(path), b"old")

    def test_a_secret_outside_the_original_fonts_fails_an_original_language_and_writes_nothing(self):
        with tempfile.TemporaryDirectory() as root:
            source = good_source()
            source["secret"]["title"] = "Erfunden\u2019s"
            argv = self.make_world(root, source)
            status, text = self.run_tool(argv)
            self.assertEqual(status, 1)
            self.assertIn("U+2019", text)
            self.assertFalse(os.path.exists(os.path.join(root, "out", "achievements.de.json")))

    def test_check_writes_nothing(self):
        with tempfile.TemporaryDirectory() as root:
            argv = self.make_world(root, good_source())
            status, _ = self.run_tool(argv + ["--check"])
            self.assertEqual(status, 0)
            self.assertFalse(os.path.exists(os.path.join(root, "out", "achievements.de.json")))

    def test_a_missing_source_fails(self):
        with tempfile.TemporaryDirectory() as root:
            argv = self.make_world(root, good_source())
            os.remove(os.path.join(root, "src", "de.json"))
            status, text = self.run_tool(argv)
            self.assertEqual(status, 1)
            self.assertIn("no source", text)


if __name__ == "__main__":
    unittest.main()
