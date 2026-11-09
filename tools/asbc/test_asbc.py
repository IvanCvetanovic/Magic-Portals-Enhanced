"""Tests of the string constants asbc reads out of the original's saved bytecode (planning step 236, PORT-32).

Run:   python -I -m unittest discover -s tools/asbc -p "test_*.py" -v        (standard library only; reads no game file)

There was no test in tools/asbc before this one: the reader was checked against the real android_game.bin by hand, and its output by the
extractors that read it. What is pinned here is the one thing step 236 changed - how a string constant's BYTES become text - because the
change was found by accident: one real description (a number of degrees and then the byte 0xBA) had a U+FFFD in it, which
`decode("utf-8", "replace")` makes of any byte that is not UTF-8, and the port drew it as three Latin-1 glyphs. Every text below is synthetic -
only the byte 0xBA between ASCII letters matters to the decoder - so no word of the original is in this file.

WHAT IT PROVES. A constant that is valid UTF-8 is UTF-8, and one that is not is Latin-1, so no byte value is ever lost to U+FFFD; the reader's string table (a repeated string is an index into the ones read) returns the decoded text
each time; and the extractor's listing reader passes such a character through untouched.
WHAT IT CANNOT PROVE. That the real module has only the one such constant (it was found by regenerating the listing and comparing it with
the old one: one line of 98,778 differed), nor what the original draws for a lone 0xBA (Video/BitmapFont.cpp:314-352 hands it to a checked UTF-8 decoder that throws), nor that U+00BA, and not the degree sign, is what the author meant.
"""
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import achievements  # noqa: E402
import asbc  # noqa: E402

REPLACEMENT = "�"


def reader(*constants: bytes) -> "asbc.Reader":
    """A Reader over string constants written as asCWriter writes them: 'n', the length, the bytes."""
    data = b""
    for raw in constants:
        assert len(raw) < 0x40  # one byte of length
        data += b"n" + bytes([len(raw)]) + raw
    return asbc.Reader(data)


class DecodeString(unittest.TestCase):
    def test_ascii_is_itself(self):
        self.assertEqual(asbc.decode_string(b"Do a flip"), "Do a flip")
        self.assertEqual(asbc.decode_string(b""), "")
        self.assertEqual(asbc.decode_string(b"a\nb"), "a\nb")

    def test_valid_utf8_is_utf8(self):
        self.assertEqual(asbc.decode_string("café".encode("utf-8")), "café")
        self.assertEqual(asbc.decode_string("Жあ".encode("utf-8")), "Жあ")
        self.assertEqual(asbc.decode_string(b"\xef\xbf\xbd"), REPLACEMENT)  # a literal U+FFFD is valid UTF-8 and stays

    def test_the_achievement_that_was_lost(self):
        # The constant as the module holds it, with a plainly synthetic text: 0xBA after a number, between ASCII letters.
        text = asbc.decode_string(b"A 360\xba B")
        self.assertEqual(text, "A 360º B")
        self.assertNotIn(REPLACEMENT, text)

    def test_no_single_byte_is_ever_lost(self):
        for byte in range(256):
            text = asbc.decode_string(bytes([byte]))
            self.assertNotIn(REPLACEMENT, text, hex(byte))
            if byte < 0x80:
                self.assertEqual(text, chr(byte))
            else:
                self.assertEqual(text, chr(byte), hex(byte))  # Latin-1: the byte is the code point
        # Within a string, too.
        for byte in range(0x80, 0x100):
            self.assertEqual(asbc.decode_string(b"a" + bytes([byte]) + b"b"), "a" + chr(byte) + "b")

    def test_a_constant_is_one_encoding_throughout(self):
        # A valid UTF-8 sequence next to a stray byte: the string is not valid UTF-8, so it is Latin-1 throughout, never half and half.
        self.assertEqual(asbc.decode_string(b"\xc3\xa9\xba"), "Ã©º")

    def test_truncated_utf8_is_latin1_not_an_error(self):
        self.assertEqual(asbc.decode_string(b"a\xe2\x82"), "aâ\u0082")


class ReaderStrings(unittest.TestCase):
    def test_new_strings_are_decoded(self):
        r = reader(b"abc", b"1080\xba", "é".encode("utf-8"))
        self.assertEqual(r.string(), "abc")
        self.assertEqual(r.string(), "1080º")
        self.assertEqual(r.string(), "é")
        self.assertEqual(r.pos, len(r.data))

    def test_a_back_reference_returns_the_decoded_text(self):
        # 'r' + index is a reference to the nth string already read; the table holds the DECODED text.
        r = asbc.Reader(b"n\x05" + b"1080\xba" + b"r\x00" + b"\x00")
        self.assertEqual(r.string(), "1080º")
        self.assertEqual(r.string(), "1080º")
        self.assertEqual(r.string(), "")

    def test_the_empty_string_and_a_bad_reference(self):
        self.assertEqual(asbc.Reader(b"\x00").string(), "")
        with self.assertRaises(asbc.BytecodeError):
            asbc.Reader(b"r\x05").string()


class ListingReader(unittest.TestCase):
    def test_the_extractor_passes_the_character_through(self):
        # The listing is Python's repr of each constant, written as UTF-8: the extractor reads it as UTF-8 and literal_eval's the argument.
        listing = (
            "== Achievement::getTitle  (assets/x.angelscript)  bytes 1..9\n"
            "     0 STR       'A 360º B\\nC'\n"
            "     1 RET       \n"
        )
        with tempfile.TemporaryDirectory() as folder:
            path = os.path.join(folder, "all_functions.txt")
            with open(path, "w", encoding="utf-8", newline="\n") as handle:
                handle.write(listing)
            functions = achievements.read_listing(__import__("pathlib").Path(path), {"Achievement::getTitle"})
        code = functions["Achievement::getTitle"].code
        self.assertEqual(code[0][0], "STR")
        self.assertEqual(achievements.ast.literal_eval(code[0][1]), "A 360º B\nC")
        self.assertEqual(achievements.ast.literal_eval(repr("A 360º B")), "A 360º B")  # repr keeps it printable


if __name__ == "__main__":
    unittest.main()
