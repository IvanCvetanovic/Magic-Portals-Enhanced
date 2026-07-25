"""Smoke tests — the package imports, the CLI parses, escaping is correct.

Fixture rule (see ../README.md): test fixtures are HAND-WRITTEN minimal examples.
Never copy a real Magic Portals .ent/.esc in here — it would get committed, which
is exactly what the repo rule forbids. Small excerpts from Ethanon's own
MIT-licensed sample projects are fine.
"""

from __future__ import annotations

import pytest

import ethanon2godot
from ethanon2godot import enml, entity, scene, textio, tscn
from ethanon2godot.__main__ import build_parser, main

# Minimal hand-authored .ent, matching the shape of the real ones. Written by hand
# from the format spec, NOT copied from any game data — see the fixture rule above.
_MINIMAL_ENT = (
    '<?xml version="1.0" ?>\n'
    "<Ethanon>\n"
    '     <Entity type="0">\n'
    "          <Sprite>hero.png</Sprite>\n"
    "    </Entity>\n"
    "</Ethanon>\n"
)


def test_version_present() -> None:
    assert ethanon2godot.__version__


def test_cli_defaults_are_gitignored_paths() -> None:
    args = build_parser().parse_args([])
    # Guard the legal rule mechanically: the defaults must stay inside the
    # gitignored staging dirs, so a careless run cannot drop extracted assets
    # into a tracked directory.
    assert args.input.as_posix() == "reference/extracted"
    assert args.output.as_posix() == "out"


def test_cli_missing_input_exits_2(tmp_path) -> None:
    assert main(["--input", str(tmp_path / "nope")]) == 2


@pytest.mark.parametrize(
    ("raw", "expected"),
    [
        ("plain", '"plain"'),
        ('say "hi"', '"say \\"hi\\""'),
        ("back\\slash", '"back\\\\slash"'),
        ("two\nlines", '"two\\nlines"'),
        ("tab\there", '"tab\\there"'),
        # Backslash must be escaped BEFORE the quote, or this double-escapes.
        ('\\"', '"\\\\\\""'),
    ],
)
def test_tscn_quote(raw: str, expected: str) -> None:
    assert tscn.quote(raw) == expected


@pytest.mark.parametrize(
    "call",
    [
        lambda: enml.parse(""),
        lambda: entity.parse(""),
        lambda: scene.parse(""),
        lambda: tscn.write_scene(),
    ],
)
def test_parsers_are_honest_stubs(call) -> None:
    # They must fail loudly rather than return plausible empty results — a silently
    # empty parse would look like "the level had no entities".
    with pytest.raises(NotImplementedError):
        call()


# --- encoding -------------------------------------------------------------
# Ethanon's .ent/.esc/.par are canonically UTF-16LE with a BOM (88 of 89 shipped
# samples), NOT UTF-8. Reading them as UTF-8 fails or — worse — yields mojibake.
# These tests exist because an earlier version of this package assumed UTF-8 in
# parse_file() and nothing caught it: the stub tests only ever called parse("").


def test_reads_utf16le_with_bom(tmp_path) -> None:
    p = tmp_path / "hero.ent"
    p.write_bytes(b"\xff\xfe" + _MINIMAL_ENT.encode("utf-16-le"))
    text = textio.read_ethanon_xml(p)
    assert text == _MINIMAL_ENT
    assert not text.startswith("﻿"), "BOM must be stripped before XML parsing"


def test_reads_utf8_with_and_without_bom(tmp_path) -> None:
    plain = tmp_path / "plain.ent"
    plain.write_bytes(_MINIMAL_ENT.encode("utf-8"))
    assert textio.read_ethanon_xml(plain) == _MINIMAL_ENT

    bom = tmp_path / "bom.ent"
    bom.write_bytes(b"\xef\xbb\xbf" + _MINIMAL_ENT.encode("utf-8"))
    assert textio.read_ethanon_xml(bom) == _MINIMAL_ENT


def test_non_ascii_survives_utf16_roundtrip(tmp_path) -> None:
    # A UTF-16 file read as UTF-8 can decode without raising and produce garbage.
    # Assert on content, not on "it didn't throw".
    src = '<?xml version="1.0" ?>\n<Ethanon>\n<Sprite>caça-níquel.png</Sprite>\n</Ethanon>\n'
    p = tmp_path / "accented.ent"
    p.write_bytes(b"\xff\xfe" + src.encode("utf-16-le"))
    assert textio.read_ethanon_xml(p) == src


@pytest.mark.parametrize("loader", [entity.parse_file, scene.parse_file])
def test_parse_file_decodes_utf16_before_failing(loader, tmp_path) -> None:
    # The stub must fail at *parsing* (NotImplementedError), not at *decoding*.
    # A UnicodeDecodeError here would mean parse_file assumed the wrong encoding.
    p = tmp_path / "sample.dat"
    p.write_bytes(b"\xff\xfe" + _MINIMAL_ENT.encode("utf-16-le"))
    with pytest.raises(NotImplementedError):
        loader(p)
