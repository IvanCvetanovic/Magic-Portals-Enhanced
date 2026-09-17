"""Smoke tests — the package imports, the CLI parses, the parsers parse.

Fixture rule (see ../README.md): test fixtures are HAND-WRITTEN minimal examples.
Never copy a real Magic Portals .ent/.esc in here — it would get committed, which
is exactly what the repo rule forbids. Small excerpts from Ethanon's own
MIT-licensed sample projects are fine.

Numbers asserted here come from measuring the real corpus (all 32 chapter-1 scenes,
934 instances) but the *data* under test is hand-authored to match its shape.
"""

from __future__ import annotations

import math

import pytest

import ethanon2godot
from ethanon2godot import enml, entity, manifest, scene, textio, tscn
from ethanon2godot.__main__ import build_parser, level_names, main

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

# Hand-authored .esc in the real structure: instance wrapper carrying <EntityName>,
# <Position> and an INLINE <Entity> definition (branch B — the only branch chapter 1
# uses). The polygon lives inside <Collision>, which is where the engine writes it.
_MINIMAL_ESC = (
    '<?xml version="1.0" ?>\n'
    "<Ethanon>\n"
    '  <SceneProperties lightIntensity="3.0" parallaxIntensity="0.0">\n'
    '    <Ambient r="0.35" g="0.3" b="0.35"/>\n'
    '    <ZAxisDirection x="0.0" y="0.0"/>\n'
    "  </SceneProperties>\n"
    "  <EntitiesInScene>\n"
    '    <Entity id="7" spriteFrame="0">\n'
    "      <EntityName>block.ent</EntityName>\n"
    '      <Position x="100.0" y="50.0" z="-4.0" angle="90.0"/>\n'
    '      <Entity type="0" shape="3" static="1" blendMode="1" friction="0.4">\n'
    "        <Sprite>block.png</Sprite>\n"
    '        <PivotAdjust x="3.0" y="-12.0"/>\n'
    "        <Collision>\n"
    '          <Position x="0.0" y="0.0" z="0.0"/>\n'
    '          <Size x="64.0" y="32.0" z="16.0"/>\n'
    "          <Polygon><![CDATA[v0{x=-1;y=-1;} v1{x=1;y=-1;} v2{x=1;y=1;}]]></Polygon>\n"
    "        </Collision>\n"
    "        <CustomData/>\n"
    "      </Entity>\n"
    "    </Entity>\n"
    "  </EntitiesInScene>\n"
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


def test_world_suffix_mapping() -> None:
    # World N == .esc suffix ''/'a'/'b'/'c'; chapter 1 is world 0. Pinned from
    # achievement strings in android_game.bin, not guessed.
    assert level_names(0)[:2] == ["level0", "level1"]
    assert level_names(1)[31] == "level31a"
    assert level_names(3)[0] == "level0c"
    assert len(level_names(0)) == 32


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


# --- ENML -----------------------------------------------------------------
# Grammar is docs/ethanon-formats.md §6.1 (Enml.cpp:313-448). The edge cases below
# are the ones a regex-based parser gets wrong.


def test_enml_basic() -> None:
    assert enml.parse("foo { a = 1; b = two; }") == {"foo": {"a": "1", "b": "two"}}


def test_enml_duplicate_key_last_wins() -> None:
    # Enml.cpp:152 — Entity::Add is m_map[key] = value. This is what makes the real
    # app.enml resolve to 1136x640 rather than 1280x720.
    assert enml.parse("d { width = 1280; width = 1136; }") == {"d": {"width": "1136"}}


def test_enml_single_slash_starts_a_line_comment() -> None:
    # ONE slash, not two. A parser expecting "//" swallows the rest as data.
    assert enml.parse("f { a = 1; / this is a comment\n b = 2; }") == {
        "f": {"a": "1", "b": "2"}
    }


def test_enml_block_comment_spans_newlines() -> None:
    assert enml.parse("f { /* one\ntwo */ a = 1; }") == {"f": {"a": "1"}}


def test_enml_whitespace_rule() -> None:
    # Leading whitespace after '=' is skipped; trailing before ';' is PRESERVED.
    assert enml.parse("f { title =   Foo ; }") == {"f": {"title": "Foo "}}


def test_enml_escapes() -> None:
    assert enml.parse("f { a = x\\;y; }") == {"f": {"a": "x;y"}}
    assert enml.parse("f { a = x\\\\y; }") == {"f": {"a": "x\\y"}}


def test_enml_stray_backslash_aborts() -> None:
    # A raw Windows path in a value is a parse ERROR upstream, not a quirk.
    with pytest.raises(enml.EnmlError):
        enml.parse("f { path = C:\\Users\\x; }")


def test_enml_empty_value_is_an_error() -> None:
    # This is why the engine can treat Get() == "" as "absent" unambiguously.
    with pytest.raises(enml.EnmlError):
        enml.parse("f { a = ; }")


def test_enml_unterminated_value_is_an_error() -> None:
    with pytest.raises(enml.EnmlError):
        enml.parse("f { a = 1 }")


def test_enml_error_carries_line_number() -> None:
    with pytest.raises(enml.EnmlError) as excinfo:
        enml.parse("f {\n a = 1;\n b = \\q; }")
    assert excinfo.value.line == 3


# --- entity ---------------------------------------------------------------


def test_entity_parses_ethanon_root_and_bare_element() -> None:
    ent = entity.parse(_MINIMAL_ENT, name="hero")
    assert ent.sprite == "hero.png"
    assert ent.type == 0
    # An inline definition IS the <Entity>; both shapes must work.
    assert entity.parse("<Entity type='0'><Sprite>x.png</Sprite></Entity>").sprite == "x.png"


def test_entity_defaults_match_engine_reset() -> None:
    ent = entity.parse("<Entity/>")
    assert ent.shape == entity.BS_NONE
    assert ent.friction == 1.0
    assert ent.density == 1.0
    assert ent.restitution == 0.0
    assert ent.gravity_scale == 1.0
    assert not ent.is_collidable


def test_entity_boolean_spellings_fall_back_like_the_engine() -> None:
    # ReadBooleanPropertyFromXmlElement uses QueryIntAttribute, so "true" does NOT
    # parse — it silently keeps the default. Asserting this keeps the behaviour honest.
    assert entity.parse('<Entity static="true"/>').static == 0
    assert entity.parse('<Entity static="1"/>').static == 1


def test_entity_polygon_is_read_from_inside_collision() -> None:
    # Nesting matters: <Polygon> is written under the COLLISION root, not under
    # <Entity>. Looking one level too high returns "no polygons", which is the wrong
    # answer for 220 of chapter 1's instances.
    ent = scene.parse(_MINIMAL_ESC).instances[0].entity
    assert ent is not None
    assert ent.collision is not None
    assert ent.collision.polygon == [(-1.0, -1.0), (1.0, -1.0), (1.0, 1.0)]
    assert ent.is_collidable


def test_entity_polygon_vertices_sort_numerically() -> None:
    # v10 must not sort between v1 and v2 — winding order matters to the physics.
    xml = (
        "<Entity shape='3'><Collision><Size x='2' y='2'/>"
        "<Polygon>" + " ".join(f"v{i}{{x={i};y=0;}}" for i in range(12)) + "</Polygon>"
        "</Collision></Entity>"
    )
    ent = entity.parse(xml)
    assert ent.collision is not None
    assert [x for x, _ in ent.collision.polygon] == [float(i) for i in range(12)]


# --- scene ----------------------------------------------------------------


def test_scene_parses_properties_and_instance() -> None:
    sc = scene.parse(_MINIMAL_ESC, name="level0")
    assert sc.name == "level0"
    assert sc.properties.z_axis_direction == (0.0, 0.0)
    assert sc.properties.ambient == (0.35, 0.3, 0.35)
    assert len(sc.instances) == 1
    inst = sc.instances[0]
    assert inst.id == 7
    assert inst.entity_name == "block.ent"
    assert inst.position == (100.0, 50.0, -4.0)
    assert inst.angle == 90.0


def test_scene_does_not_double_count_inline_definitions() -> None:
    # The nested <Entity> is the DEFINITION of its parent instance, not a second
    # instance. An iter()-based walk would report two.
    assert len(scene.parse(_MINIMAL_ESC).instances) == 1


def test_scene_z_axis_direction_zero_means_no_displacement() -> None:
    inst = scene.parse(_MINIMAL_ESC).instances[0]
    assert inst.screen_pos((0.0, 0.0)) == (100.0, 50.0)
    # The engine DEFAULT is (0,-1); with it, z would shift the sprite vertically.
    assert inst.screen_pos((0.0, -1.0)) == (100.0, 54.0)


def test_scene_default_z_axis_is_engine_default_not_zero() -> None:
    # ETHSceneProperties.cpp:92 resets to (0,-1). Defaulting to (0,0) would silently
    # agree with the shipped data while being wrong about the format.
    sc = scene.parse('<Ethanon><SceneProperties/><EntitiesInScene/></Ethanon>')
    assert sc.properties.z_axis_direction == (0.0, -1.0)


# --- tscn emission --------------------------------------------------------


def test_write_scene_emits_loadable_structure() -> None:
    text, report = tscn.write_scene(scene.parse(_MINIMAL_ESC, name="level0"))
    assert text.startswith("[gd_scene load_steps=")
    assert "format=3" in text
    assert report.nodes == 1
    assert report.sprites == 1
    assert report.colliders == 1
    assert report.skipped == []
    assert "block.png" in report.textures
    # load_steps must equal ext + sub + 1 or Godot warns on load.
    declared = int(text.split("load_steps=")[1].split()[0])
    assert declared == text.count("[ext_resource ") + text.count("[sub_resource ") + 1


def test_write_scene_negates_rotation_for_the_android_path() -> None:
    # GLES2Sprite.cpp:207 — RotateZ(DegreeToRadian(-angle)). Desktop GL uses +angle;
    # Android is the target, so the negated form is the correct one here.
    #
    # Assert the exact value, not a prefix: this sign is the one part of the mapping
    # still unconfirmed against video, and it silently mirrors 47 chapter-1 instances.
    # An earlier version of this test compared against a truncated string that reduced
    # to "rotation = -", which passed for ANY negative number and tested nothing.
    text, _ = tscn.write_scene(scene.parse(_MINIMAL_ESC))
    assert "rotation = -1.5708\n" in text
    assert "rotation = 1.5708" not in text  # the desktop-GL (+angle) convention
    assert tscn._num(-math.radians(90.0)) == "-1.5708"


def test_write_scene_maps_additive_blend() -> None:
    # blendMode=1 (AM_ADD) is the portals' glow. The doc previously claimed it never
    # occurs on entities; it occurs 22 times in chapter 1 alone.
    text, _ = tscn.write_scene(scene.parse(_MINIMAL_ESC))
    assert 'type="CanvasItemMaterial"' in text
    assert "blend_mode = 1" in text


def test_write_scene_negates_pivot_into_sprite_offset() -> None:
    # Ethanon SUBTRACTS the origin; Sprite2D is centred so the type anchor cancels and
    # only -pivotAdjust survives.
    text, _ = tscn.write_scene(scene.parse(_MINIMAL_ESC))
    assert "offset = Vector2(-3, 12)" in text


def test_write_scene_polygon_scales_by_half_collision_size() -> None:
    # (size * 0.5) * v + offset — ETHPhysicsSimulator.cpp:44-47. Size is 64x32.
    text, _ = tscn.write_scene(scene.parse(_MINIMAL_ESC))
    assert "PackedVector2Array(-32, -16, 32, -16, 32, 16)" in text


def test_write_scene_node_names_are_godot_legal_and_unique() -> None:
    text, _ = tscn.write_scene(scene.parse(_MINIMAL_ESC))
    # "block.ent" would be an illegal Godot node name; the id disambiguates repeats.
    assert 'name="block_ent_7"' in text


def test_write_scene_reports_rather_than_silently_dropping() -> None:
    # An instance with no resolvable definition must be REPORTED, never quietly
    # skipped — CLAUDE.md's "no silent caps".
    xml = (
        "<Ethanon><SceneProperties/><EntitiesInScene>"
        '<Entity id="1"><EntityName>ghost</EntityName>'
        '<Position x="0" y="0" z="0" angle="0"/></Entity>'
        "</EntitiesInScene></Ethanon>"
    )
    _, report = tscn.write_scene(scene.parse(xml))
    assert report.nodes == 0
    assert len(report.skipped) == 1
    assert "ghost" in report.skipped[0]


# --- chapter manifest -----------------------------------------------------


def _scores_file(tmp_path):
    # Hand-authored in the real goldenScores.enml shape: one block per world, keys
    # level0..levelN. Values below include a 0, which is a REAL threshold meaning
    # "finish using zero portals" — seven levels ship one.
    p = tmp_path / "goldenScores.enml"
    p.write_text(
        "goldenScores0 { level0 = 0; level1 = 1; level2 = 2; }\n"
        "goldenScores1 { level0 = 4; level1 = 6; }\n",
        encoding="utf-8",
    )
    return p


def test_manifest_reads_golden_scores(tmp_path) -> None:
    scores = manifest.read_golden_scores(_scores_file(tmp_path))
    assert scores[0] == {0: 0, 1: 1, 2: 2}
    assert scores[1] == {0: 4, 1: 6}


def test_manifest_missing_scores_file_is_not_fatal(tmp_path) -> None:
    # An absent file means "thresholds unknown", not "crash the conversion".
    assert manifest.read_golden_scores(tmp_path / "nope.enml") == {}


def test_manifest_build_shape(tmp_path) -> None:
    scores = manifest.read_golden_scores(_scores_file(tmp_path))
    data = manifest.build(0, ["level0", "level1", "level2"], scores)
    assert data["format"] == 1
    # The array index must always equal the world index, so callers never search.
    assert len(data["worlds"]) == manifest.WORLD_COUNT
    assert [w["index"] for w in data["worlds"]] == [0, 1, 2, 3]
    levels = data["worlds"][0]["levels"]
    assert levels[0]["scene"] == "res://assets/levels/level0.tscn"
    assert levels[0]["golden_score"] == 0
    assert levels[2]["golden_score"] == 2
    # Unconverted worlds stay present but empty.
    assert data["worlds"][2]["levels"] == []


def test_manifest_unknown_threshold_is_minus_one_not_zero(tmp_path) -> None:
    # 0 is a real threshold, so it cannot double as "unknown" — that is why the
    # sentinel is -1 everywhere, including in the Godot-side DataLoader.
    data = manifest.build(0, ["level0", "level1"], {0: {0: 0}})
    assert data["worlds"][0]["levels"][0]["golden_score"] == 0
    assert data["worlds"][0]["levels"][1]["golden_score"] == manifest.UNKNOWN_GOLDEN_SCORE
    assert manifest.UNKNOWN_GOLDEN_SCORE == -1


def test_manifest_merge_preserves_other_worlds(tmp_path) -> None:
    # Converting world 1 later must not erase world 0's levels.
    first = manifest.build(0, ["level0"], {0: {0: 5}})
    second = manifest.build(1, ["level0a"], {1: {0: 7}}, existing=first)
    assert second["worlds"][0]["levels"][0]["name"] == "level0"
    assert second["worlds"][1]["levels"][0]["name"] == "level0a"


def test_manifest_roundtrip(tmp_path) -> None:
    path = tmp_path / "data" / "chapters.json"
    data = manifest.build(0, ["level0"], {0: {0: 3}})
    manifest.write(path, data)
    assert manifest.read(path) == data


def test_manifest_read_corrupt_returns_none(tmp_path) -> None:
    # Corrupt means "rebuild from source", not "half-merge into garbage".
    path = tmp_path / "chapters.json"
    path.write_text("{not json", encoding="utf-8")
    assert manifest.read(path) is None
    assert manifest.read(tmp_path / "absent.json") is None


# --- encoding -------------------------------------------------------------
# Ethanon's .ent/.esc/.par are canonically UTF-16LE with a BOM (88 of 89 shipped
# samples, and 133 of 133 real .esc), NOT UTF-8. Reading them as UTF-8 fails or —
# worse — yields mojibake. These tests exist because an earlier version of this
# package assumed UTF-8 in parse_file() and nothing caught it.


def test_reads_utf16le_with_bom(tmp_path) -> None:
    p = tmp_path / "hero.ent"
    p.write_bytes(b"\xff\xfe" + _MINIMAL_ENT.encode("utf-16-le"))
    text = textio.read_ethanon_xml(p)
    assert text == _MINIMAL_ENT
    assert not text.startswith("\ufeff"), "BOM must be stripped before XML parsing"


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


@pytest.mark.parametrize(
    ("loader", "payload"), [(entity.parse_file, _MINIMAL_ENT), (scene.parse_file, _MINIMAL_ESC)]
)
def test_parse_file_decodes_utf16(loader, payload, tmp_path) -> None:
    # Must decode UTF-16 before parsing. A UnicodeDecodeError or an XML syntax error
    # here would mean parse_file assumed the wrong encoding.
    p = tmp_path / "sample.dat"
    p.write_bytes(b"\xff\xfe" + payload.encode("utf-16-le"))
    assert loader(p) is not None


# --- custom data + trigger geometry ---------------------------------------
# Both of these were silently broken at first and produced no error, only absence.


def test_custom_data_uses_child_elements_not_attributes() -> None:
    # <Variable><Type>..</Type><Name>..</Name><Value>..</Value></Variable>.
    # Reading var.get("name") instead parses every bag as empty — no exception, just
    # nothing — which loses teleportable, max_portals and the portal pairing.
    xml = (
        "<Entity><CustomData>"
        "<Variable><Type>uint</Type><Name>max_portals</Name><Value>3</Value></Variable>"
        "<Variable><Type>float</Type><Name>speed</Name><Value>0.7</Value></Variable>"
        "</CustomData></Entity>"
    )
    ent = entity.parse(xml)
    assert ent.custom_data == {"max_portals": "3", "speed": "0.7"}


def test_custom_data_reaches_the_tscn_as_metadata() -> None:
    text, _ = tscn.write_scene(scene.parse(_MINIMAL_ESC))
    assert 'metadata/entity_name = "block.ent"' in text


def test_trigger_box_survives_for_non_physics_entities() -> None:
    # door.ent / crystal.ent / portal_static are shape=BS_NONE — no physics fixture — but
    # still ship a <Collision> box that the original tests overlap against. Dropping it
    # because it makes no fixture would delete every trigger region in the game.
    xml = (
        "<Ethanon><SceneProperties/><EntitiesInScene>"
        '<Entity id="3"><EntityName>door.ent</EntityName>'
        '<Position x="10" y="20" z="0" angle="0"/>'
        '<Entity type="0" shape="0"><Collision>'
        '<Position x="1" y="2" z="0"/><Size x="8" y="24" z="16"/>'
        "</Collision></Entity></Entity>"
        "</EntitiesInScene></Ethanon>"
    )
    sc = scene.parse(xml)
    ent = sc.instances[0].entity
    assert ent is not None
    assert not ent.is_collidable  # shape=BS_NONE: no physics body
    text, report = tscn.write_scene(sc)
    assert "metadata/trigger_size = Vector2(8, 24)" in text
    assert "metadata/trigger_offset = Vector2(1, 2)" in text
    assert report.colliders == 0  # …and still no collider emitted


def test_collidable_entities_do_not_get_a_trigger_box() -> None:
    # A real physics entity emits a collider, not trigger metadata — otherwise level
    # geometry would sprout pickup-style triggers.
    text, report = tscn.write_scene(scene.parse(_MINIMAL_ESC))
    assert report.colliders == 1
    assert "metadata/trigger_size" not in text


# --- uiassets ------------------------------------------------------------------------------


def _fake_png(path, width: int, height: int) -> None:
    """A header-only PNG: read_size needs the signature and IHDR, nothing else."""
    import struct

    path.parent.mkdir(parents=True, exist_ok=True)
    ihdr = struct.pack(">I", 13) + b"IHDR" + struct.pack(">II", width, height)
    path.write_bytes(b"\x89PNG\r\n\x1a\n" + ihdr)


def test_ui_copy_takes_exact_hd_and_refuses_the_rest(tmp_path) -> None:
    from ethanon2godot.uiassets import copy_ui_assets

    sprites = tmp_path / "in" / "assets" / "sprites"
    _fake_png(sprites / "button.png", 32, 32)
    _fake_png(sprites / "hd" / "button.png", 64, 64)  # exact x2: copied
    _fake_png(sprites / "sky.png", 455, 256)
    _fake_png(sprites / "hd" / "sky.png", 1024, 512)  # aspect change: refused
    _fake_png(sprites / "hd" / "orphan_.png", 64, 64)  # no 1x file: refused

    report = copy_ui_assets(tmp_path / "in", tmp_path / "out")

    out = tmp_path / "out" / "assets" / "sprites"
    assert (out / "hd" / "button.png").is_file()
    assert not (out / "hd" / "sky.png").exists()
    assert not (out / "hd" / "orphan_.png").exists()
    assert report.copied["sprites/hd"] == 1
    assert len(report.skipped) == 2


def test_ui_copy_brings_font_pages_and_refuses_incomplete_fonts(tmp_path) -> None:
    from ethanon2godot.uiassets import copy_ui_assets

    data = tmp_path / "in" / "assets" / "data"
    data.mkdir(parents=True)
    (data / "Good.fnt").write_text(
        'info face="X"\npage id=0 file="Good_0.png"\npage id=1 file="Good_1.png"\n'
    )
    _fake_png(data / "Good_0.png", 8, 8)
    _fake_png(data / "Good_1.png", 8, 8)
    (data / "Broken.fnt").write_text('page id=0 file="Broken_0.png"\n')  # page absent

    report = copy_ui_assets(tmp_path / "in", tmp_path / "out")

    fonts = tmp_path / "out" / "assets" / "fonts"
    assert {p.name for p in fonts.iterdir()} == {"Good.fnt", "Good_0.png", "Good_1.png"}
    assert report.copied["fonts"] == 1
    assert any("Broken.fnt" in line for line in report.skipped)


def test_ui_copy_rewrites_the_one_channel_code_godot_rejects(tmp_path) -> None:
    from ethanon2godot.uiassets import copy_ui_assets

    data = tmp_path / "in" / "assets" / "data"
    data.mkdir(parents=True)
    (data / "V.fnt").write_text('common alphaChnl=2 redChnl=4\npage id=0 file="V.png"\n')
    _fake_png(data / "V.png", 8, 8)

    copy_ui_assets(tmp_path / "in", tmp_path / "out")

    written = (tmp_path / "out" / "assets" / "fonts" / "V.fnt").read_text()
    assert "alphaChnl=0" in written and "alphaChnl=2" not in written

# --- entity texture tiers ------------------------------------------------------------------
# tiers.py. Every image below is a header-only PNG written here, under invented names. The sizes
# have the shapes the original ships: an exact-2x hd file, a 1024x512 fullhd sky over a 455x256 1x
# file, and an odd hd width (973 over 486) whose half is truncated.

_GAME_APP_ENML = (
    "default\n{\n\twidth = 1280; height = 720;\n\tminScreenHeightForHdVersion = 480;\n"
    "\tminScreenHeightForFullHdVersion = 600;\n\thdDensityValue = 2;\n"
    "\tfullHdDensityValue = 2;\n}\n"
)


def _tier_input(root) -> None:
    entities = root / "assets" / "entities"
    _fake_png(entities / "a.png", 32, 32)
    _fake_png(entities / "hd" / "a.png", 64, 64)
    _fake_png(entities / "sky.png", 455, 256)
    _fake_png(entities / "fullhd" / "sky.png", 1024, 512)
    _fake_png(entities / "hd" / "sky.png", 910, 512)  # both tiers: fullhd is tried first
    _fake_png(entities / "hd" / "b.png", 64, 64)  # no 1x file and referenced by nothing
    _fake_png(entities / "wide.png", 486, 128)
    _fake_png(entities / "hd" / "wide.png", 973, 256)
    _fake_png(entities / "plain.png", 16, 8)
    _fake_png(entities / "hd" / "only.png", 40, 20)  # no 1x file: the loader never asks for one
    (root / "assets" / "app.enml").write_text(_GAME_APP_ENML, encoding="utf-8")


def test_tier_search_reads_app_enml_like_the_engine(tmp_path) -> None:
    from ethanon2godot import tiers

    app = tmp_path / "app.enml"
    app.write_text(_GAME_APP_ENML, encoding="utf-8")
    game, warning = tiers.read_density_settings(app)
    assert warning is None
    assert tiers.search_order(game) == [("fullhd", 2.0), ("hd", 2.0)]
    # Below 600 px no fullhd file is tried. At 480 px both the hd and the ld test pass.
    assert tiers.search_order(game, 599) == [("hd", 2.0)]
    assert tiers.search_order(game, 480) == [("hd", 2.0), ("ld", 0.5)]
    assert tiers.search_order(game, 479) == [("ld", 0.5)]
    assert tiers.search_order(game, 320) == [("xld", 0.25), ("ld", 0.5)]

    # No file: the ETHAppEnmlFile constructor's numbers, which are not the game's. At 720 px only
    # hd is tried, and a fullhd file would be density 4.
    defaults, warning = tiers.read_density_settings(tmp_path / "absent.enml")
    assert warning is not None and "defaults" in warning
    assert tiers.search_order(defaults) == [("hd", 2.0)]
    assert tiers.search_order(defaults, 1080) == [("fullhd", 4.0), ("hd", 2.0)]

    # `window`, then the Android block, override `default` key by key; `ios` is not read.
    app.write_text(
        _GAME_APP_ENML + "window { hdDensityValue = 3; }\nandroid { fullHdDensityValue = 1.5; }\n"
        "ios { hdDensityValue = 9; }\n",
        encoding="utf-8",
    )
    merged, _ = tiers.read_density_settings(app)
    assert tiers.search_order(merged) == [("fullhd", 1.5), ("hd", 3.0)]


def test_tier_units_truncate_after_the_divide() -> None:
    from ethanon2godot import tiers

    assert tiers.units((973, 256), 2.0) == (486, 128)
    assert tiers.units((1024, 512), 2.0) == (512, 256)
    assert tiers.units((155, 217), 1.0) == (155, 217)


def test_tier_copy_copies_the_drawn_tier_and_reports_size_changes(tmp_path) -> None:
    import json

    from ethanon2godot import tiers

    _tier_input(tmp_path / "in")
    settings, _ = tiers.read_density_settings(tmp_path / "in" / "assets" / "app.enml")
    textures = {"a.png", "sky.png", "wide.png", "plain.png", "only.png", "missing.png"}

    report = tiers.copy_entity_textures(tmp_path / "in", tmp_path / "out", textures, settings)

    out = tmp_path / "out" / "assets" / "entities"
    assert sorted(p.relative_to(out).as_posix() for p in out.rglob("*.png")) == [
        "a.png",
        "fullhd/sky.png",
        "hd/a.png",
        "hd/only.png",
        "hd/wide.png",
        "plain.png",
        "sky.png",
        "wide.png",
    ]
    assert report.copied == {"1x": 4, "tier": 4}
    assert report.tiers == {"hd": 3, "fullhd": 1, "1x": 1}
    # Only the sky changes size: 973 / 2 truncates to the 1x file's 486.
    assert report.size_changes == [
        "entities/fullhd/sky.png: 512x256 units against the 1x file's 455x256"
    ]
    assert report.no_base == ["entities/hd/only.png: no 1x file; drawn at 20x10 units"]
    assert report.skipped == ["entities/missing.png: not found, and no tier file either"]

    record = json.loads((tmp_path / "out" / "assets" / tiers.MANIFEST_NAME).read_text("utf-8"))
    assert record["format"] == 1
    assert record["choice"] == {
        "screen_height_px": 720,
        "search": [{"tier": "fullhd", "density": 2}, {"tier": "hd", "density": 2}],
    }
    assert record["files"]["entities/sky.png"] == {
        "drawn": "entities/fullhd/sky.png",
        "tier": "fullhd",
        "density": 2,
        "texels": [1024, 512],
        "units": [512, 256],
    }
    assert record["files"]["entities/wide.png"]["units"] == [486, 128]
    assert record["files"]["entities/plain.png"] == {
        "drawn": "entities/plain.png",
        "tier": "1x",
        "density": 1,
        "texels": [16, 8],
        "units": [16, 8],
    }
    assert "entities/missing.png" not in record["files"]
    assert "entities/b.png" not in record["files"]


def test_tier_copy_rewrites_no_identical_file_and_no_identical_manifest(tmp_path) -> None:
    import os

    from ethanon2godot import tiers
    from ethanon2godot.imagesize import read_size

    _tier_input(tmp_path / "in")
    settings, _ = tiers.read_density_settings(tmp_path / "in" / "assets" / "app.enml")
    textures = {"a.png", "sky.png"}
    tiers.copy_entity_textures(tmp_path / "in", tmp_path / "out", textures, settings)

    assets = tmp_path / "out" / "assets"
    kept = [
        assets / "entities" / "a.png",
        assets / "entities" / "fullhd" / "sky.png",
        assets / tiers.MANIFEST_NAME,
    ]
    for path in kept:
        os.utime(path, (1_000_000, 1_000_000))  # a copy or a rewrite would move this
    (assets / "entities" / "hd" / "a.png").write_bytes(b"stale")  # other bytes: replaced

    report = tiers.copy_entity_textures(tmp_path / "in", tmp_path / "out", textures, settings)

    assert report.copied == {"1x": 0, "tier": 1}
    assert report.unchanged == {"1x": 2, "tier": 1}
    assert all(path.stat().st_mtime == 1_000_000 for path in kept)
    assert read_size(assets / "entities" / "hd" / "a.png") == (64, 64)


def test_cli_copies_tiers_and_merges_the_manifest_across_worlds(tmp_path, capsys) -> None:
    import json

    _tier_input(tmp_path / "in")
    scenes = tmp_path / "in" / "assets" / "scenes"
    scenes.mkdir(parents=True)
    (scenes / "level0.esc").write_text(_MINIMAL_ESC.replace("block.png", "a.png"), "utf-8")
    (scenes / "level0a.esc").write_text(_MINIMAL_ESC.replace("block.png", "sky.png"), "utf-8")
    out = tmp_path / "out"
    base = ["-i", str(tmp_path / "in"), "-o", str(out), "--level", "0", "--copy-textures"]

    assert main([*base, "--world", "0"]) == 0
    first = capsys.readouterr().out
    assert "entity texture tiers: hd 1 (" in first
    assert "tier size change" not in first
    assert main([*base, "--world", "1"]) == 0
    second = capsys.readouterr().out
    assert "tier size change: entities/fullhd/sky.png: 512x256 units" in second

    record = json.loads((out / "assets" / "entity_tiers.json").read_text(encoding="utf-8"))
    assert sorted(record["files"]) == ["entities/a.png", "entities/sky.png"]
    assert (out / "assets" / "entities" / "hd" / "a.png").is_file()
    assert (out / "assets" / "entities" / "fullhd" / "sky.png").is_file()
    # The scene does not name the tier: the port resolves it (route B).
    level = (out / "levels" / "level0a.tscn").read_text(encoding="utf-8")
    assert 'path="res://assets/entities/sky.png"' in level
    assert "fullhd" not in level
