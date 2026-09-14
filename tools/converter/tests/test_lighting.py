"""Lighting keys (`metadata/eth_*`) and the lighting file copy.

Fixture rule (see ../README.md): everything here is HAND-WRITTEN. The .esc below has the
shape of a real level — a torch-like lamp, an emissive sprite, a z of -18.5 and a <Scale> —
but its names and numbers are invented, not taken from game data: every <Light>, ambient,
lightIntensity, emissive colour and position that carries a distinctive value was checked to
occur in no shipped .esc/.ent (generic ones such as static="1" or a zero alpha of course do).
The lightmap is a PNG generated in tmp_path.

Pinned numbers follow the engine, cited in tscn.py / entity.py and in the 2026-09-14
"lighting keys" subsection of docs/ethanon-formats.md.
"""

from __future__ import annotations

import json
import re
import struct
import zlib
from pathlib import Path

from ethanon2godot import entity, scene, tscn
from ethanon2godot.__main__ import main
from ethanon2godot.imagesize import read_size
from ethanon2godot.lighting import MANIFEST_NAME, copy_lighting_assets

_LIGHTING_ESC = (
    '<?xml version="1.0" ?>\n'
    "<Ethanon>\n"
    '  <SceneProperties lightIntensity="2.250000" parallaxIntensity="0.000000">\n'
    '    <Ambient r="0.400000" g="0.250000" b="0.300000" />\n'
    '    <ZAxisDirection x="0.000000" y="0.000000" />\n'
    "  </SceneProperties>\n"
    "  <EntitiesInScene>\n"
    # A torch-like lamp: static, lit, sprite, normal map, a light with a halo. The Scale is
    # deliberately x != y: it is the only thing that separates the lighting's scale.y from
    # the halo's per-axis scale, and no shipped light owner carries one.
    '    <Entity id="42" spriteFrame="0">\n'
    "      <EntityName>lamp.ent</EntityName>\n"
    '      <Position x="120.000000" y="48.000000" z="-18.500000" angle="0.000000" />\n'
    '      <Entity shape="0" applyLight="1" castShadow="0" type="0" static="1" blendMode="0">\n'
    '        <Scale x="2.000000" y="3.000000" />\n'
    "        <Sprite>lamp.png</Sprite>\n"
    "        <Normal>lamp_nm.png</Normal>\n"
    '        <Light active="1" static="0" castShadows="1" range="250.000000"'
    ' haloBrightness="0.600000" haloSize="280.000000">\n'
    '          <Position x="0.000000" y="-10.000000" z="20.000000" />\n'
    '          <Color r="0.900000" g="0.400000" b="0.200000" />\n'
    "          <HaloBitmap>glow.bmp</HaloBitmap>\n"
    "        </Light>\n"
    "        <CustomData />\n"
    "      </Entity>\n"
    "    </Entity>\n"
    # An emissive, static, UNLIT sprite with an instance colour on the OUTER element, and a
    # lightmap file beside it that the engine could never have baked.
    '    <Entity id="43" spriteFrame="0">\n'
    "      <EntityName>gem.ent</EntityName>\n"
    '      <Color r="1.000000" g="1.000000" b="1.000000" a="0.500000" />\n'
    '      <Position x="10.000000" y="20.000000" z="0.000000" angle="0.000000" />\n'
    '      <Entity shape="0" applyLight="0" type="0" static="1" blendMode="0">\n'
    '        <EmissiveColor r="0.550000" g="0.800000" b="0.650000" a="0.000000" />\n'
    "        <Sprite>gem.png</Sprite>\n"
    "        <CustomData />\n"
    "      </Entity>\n"
    "    </Entity>\n"
    # Spriteless and lightless, like a player-start marker: z and emissive still reach the
    # node (a consumer takes an actor's lighting height from such a node), but static,
    # applyLight and the normal map do not.
    '    <Entity id="44" spriteFrame="0">\n'
    "      <EntityName>marker</EntityName>\n"
    '      <Position x="0.000000" y="0.000000" z="5.000000" angle="0.000000" />\n'
    '      <Entity shape="0" applyLight="1" type="0" static="1" blendMode="0">\n'
    '        <EmissiveColor r="1.000000" g="1.000000" b="1.000000" a="0.000000" />\n'
    "        <Normal>marker_nm.png</Normal>\n"
    "        <CustomData />\n"
    "      </Entity>\n"
    "    </Entity>\n"
    # A sprite at every default (emissive rgb 0 with a non-zero alpha, Scale 1): no key.
    '    <Entity id="45" spriteFrame="0">\n'
    "      <EntityName>plain.ent</EntityName>\n"
    '      <Position x="30.000000" y="40.000000" z="0.000000" angle="0.000000" />\n'
    '      <Entity shape="0" applyLight="0" type="0" static="0" blendMode="0">\n'
    '        <EmissiveColor r="0.000000" g="0.000000" b="0.000000" a="1.000000" />\n'
    '        <Scale x="1.000000" y="1.000000" />\n'
    "        <Sprite>plain.png</Sprite>\n"
    "        <CustomData />\n"
    "      </Entity>\n"
    "    </Entity>\n"
    # A DYNAMIC lit sprite with a lightmap file beside it: not static, so not attached.
    '    <Entity id="46" spriteFrame="0">\n'
    "      <EntityName>crate.ent</EntityName>\n"
    '      <Position x="50.000000" y="60.000000" z="0.000000" angle="0.000000" />\n'
    '      <Entity shape="0" applyLight="1" type="0" static="0" blendMode="0">\n'
    "        <Sprite>crate.png</Sprite>\n"
    "        <Normal>crate_nm.png</Normal>\n"
    "        <CustomData />\n"
    "      </Entity>\n"
    "    </Entity>\n"
    # A spriteless light at every <Light> default except that it exists: range 256 is still
    # written, because a reader identifies a light by that key.
    '    <Entity id="47" spriteFrame="0">\n'
    "      <EntityName>spark</EntityName>\n"
    '      <Position x="70.000000" y="80.000000" z="0.000000" angle="0.000000" />\n'
    '      <Entity shape="0" applyLight="0" type="0" static="0" blendMode="0">\n'
    "        <Light />\n"
    "        <CustomData />\n"
    "      </Entity>\n"
    "    </Entity>\n"
    # A halo whose offset, size and brightness are all at their defaults: the halo itself
    # is written, its three numbers are not.
    '    <Entity id="48" spriteFrame="0">\n'
    "      <EntityName>ember.ent</EntityName>\n"
    '      <Position x="90.000000" y="100.000000" z="0.000000" angle="0.000000" />\n'
    '      <Entity shape="0" applyLight="0" type="0" static="0" blendMode="0">\n'
    "        <Sprite>ember.png</Sprite>\n"
    '        <Light range="180.000000">\n'
    '          <Position x="0.000000" y="0.000000" z="7.000000" />\n'
    "          <HaloBitmap>ember_halo.png</HaloBitmap>\n"
    "        </Light>\n"
    "        <CustomData />\n"
    "      </Entity>\n"
    "    </Entity>\n"
    # A light with NON-default halo numbers but no <HaloBitmap>: no halo piece exists, so
    # no halo key is written at all.
    '    <Entity id="49" spriteFrame="0">\n'
    "      <EntityName>beacon</EntityName>\n"
    '      <Position x="110.000000" y="130.000000" z="0.000000" angle="0.000000" />\n'
    '      <Entity shape="0" applyLight="0" type="0" static="1" blendMode="0">\n'
    '        <Light range="120.000000" haloBrightness="0.850000" haloSize="90.000000">\n'
    '          <Position x="6.000000" y="-4.000000" z="8.000000" />\n'
    '          <Color r="0.300000" g="0.600000" b="0.900000" />\n'
    "        </Light>\n"
    "        <CustomData />\n"
    "      </Entity>\n"
    "    </Entity>\n"
    "  </EntitiesInScene>\n"
    "</Ethanon>\n"
)


def _png(path: Path, width: int, height: int) -> None:
    """A real (decodable) RGB PNG, built with the stdlib."""

    def chunk(tag: bytes, data: bytes) -> bytes:
        crc = zlib.crc32(tag + data) & 0xFFFFFFFF
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", crc)

    rows = b"".join(b"\x00" + b"\x20\x10\x03" * width for _ in range(height))
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(rows))
        + chunk(b"IEND", b"")
    )


def _node_block(text: str, name: str) -> list[str]:
    """The lines of one node block, header included, up to the blank line."""
    lines = text.splitlines()
    start = lines.index(next(line for line in lines if line.startswith(f'[node name="{name}"')))
    block: list[str] = []
    for line in lines[start:]:
        if not line:
            break
        block.append(line)
    return block


def _instance_blocks(text: str) -> list[list[str]]:
    """Every instance node block (direct children of the root), header included."""
    blocks = [chunk.splitlines() for chunk in text.split("\n\n")]
    return [b for b in blocks if b and b[0].startswith("[node ") and 'parent="."' in b[0]]


def _eth(block: list[str]) -> dict[str, str]:
    found = (re.match(r'^metadata/(eth_[a-z_]+) = "(.*)"$', line) for line in block)
    return {m.group(1): m.group(2) for m in found if m}


def _convert(tmp_path: Path) -> tuple[str, tscn.ConversionReport]:
    esc_dir = tmp_path / "scenes"
    lightmaps = scene.lightmap_directory(esc_dir / "lamps.esc")
    _png(lightmaps / "add42.png", 16, 32)
    _png(lightmaps / "add43.png", 16, 32)
    _png(lightmaps / "add46.png", 16, 32)
    return tscn.write_scene(
        scene.parse(_LIGHTING_ESC, name="lamps"), root_name="lamps", lightmap_dir=lightmaps
    )


# --- parsing ----------------------------------------------------------------------------


def test_entity_lighting_fields_default_to_the_engine_reset() -> None:
    ent = entity.parse("<Entity/>")
    assert ent.apply_light == 0
    assert ent.emissive == (0.0, 0.0, 0.0, 0.0)
    assert ent.scale == (1.0, 1.0)
    assert ent.normal == ""
    assert ent.light is None


def test_light_is_parsed_with_constructor_defaults_per_component() -> None:
    # ETHLight.cpp:27-38; every field is queried independently, so a partial <Color>
    # keeps 1.0 in the missing channels.
    ent = entity.parse("<Entity><Light><Color r='0.2'/></Light></Entity>")
    assert ent.light == entity.Light(color=(0.2, 1.0, 1.0))
    assert ent.light.range == 256.0
    assert ent.light.halo_size == 64.0
    assert ent.light.halo_brightness == 1.0


def test_light_is_discarded_unless_active_and_in_range() -> None:
    # ETHEntityProperties.cpp:343 — `active` defaults to true (ETHLight(true), :341).
    assert entity.parse("<Entity><Light active='0'/></Entity>").light is None
    assert entity.parse("<Entity><Light range='0'/></Entity>").light is None
    assert entity.parse("<Entity><Light range='-5'/></Entity>").light is None
    assert entity.parse("<Entity><Light active='1' range='1'/></Entity>").light is not None


def test_instance_color_is_read_from_the_outer_element_only() -> None:
    # ETHEntity.cpp:236 reads <Color> off the instance; ETHEntityProperties has no <Color>.
    sc = scene.parse(_LIGHTING_ESC)
    assert sc.instances[1].color == (1.0, 1.0, 1.0, 0.5)
    inner_only = (
        "<Ethanon><EntitiesInScene><Entity id='1'><Position x='0' y='0' z='0'/>"
        "<Entity><Color r='0' g='0' b='0' a='0'/></Entity></Entity></EntitiesInScene></Ethanon>"
    )
    assert scene.parse(inner_only).instances[0].color == (1.0, 1.0, 1.0, 1.0)


def test_lightmap_directory_replaces_every_dot() -> None:
    # ETHScene.cpp:429-443.
    assert scene.lightmap_directory(Path("scenes/level0.esc")) == Path("scenes/level0-esc")
    assert scene.lightmap_directory(Path("a/x.y.esc")) == Path("a/x-y-esc")


# --- emission -----------------------------------------------------------------------------


def test_scene_keys_go_on_the_root_node(tmp_path) -> None:
    text, report = _convert(tmp_path)
    assert _node_block(text, "lamps") == [
        '[node name="lamps" type="Node2D"]',
        'metadata/eth_ambient = "0.4 0.25 0.3"',
        'metadata/eth_light_intensity = "2.25"',
    ]
    assert report.lighting["eth_ambient"] == 1


def test_scene_keys_are_not_invented_without_scene_properties() -> None:
    text, _ = tscn.write_scene(scene.parse("<Ethanon><EntitiesInScene/></Ethanon>"))
    assert "eth_ambient" not in text
    assert "eth_light_intensity" not in text


def test_torch_like_lamp_emits_every_key_with_the_engine_scaling(tmp_path) -> None:
    text, _ = _convert(tmp_path)
    block = _node_block(text, "lamp_ent_42")
    assert _eth(block) == {
        "eth_z": "-18.5",
        "eth_static": "1",
        "eth_apply_light": "1",
        "eth_normal": "res://assets/entities/normalmaps/lamp_nm.png",
        "eth_lightmap": "res://assets/lightmaps/lamps/add42.png",
        # light.pos * scale.y and range * scale.y (ETHEntityRenderingManager.cpp:180-181)
        "eth_light_offset": "0 -30 60",
        "eth_light_range": "750",
        "eth_light_color": "0.9 0.4 0.2",
        "eth_halo": "res://assets/entities/glow.bmp",
        # light.pos UNSCALED (ETHRenderEntity.cpp:378); haloSize * scale per axis (:385)
        "eth_halo_offset": "0 -10",
        "eth_halo_size": "560 840",
        "eth_halo_brightness": "0.6",
    }
    # The rounded draw order and the unrounded lighting height are separate lines.
    assert "z_index = -18" in block
    assert 'metadata/eth_z = "-18.5"' in block


def test_keys_come_out_in_the_documented_order(tmp_path) -> None:
    text, _ = _convert(tmp_path)
    keys = list(_eth(_node_block(text, "lamp_ent_42")))
    assert keys == [k for k in tscn.ETH_ENTITY_KEYS if k in keys]


def test_emissive_and_instance_colour_but_no_lightmap_on_an_unlit_sprite(tmp_path) -> None:
    text, report = _convert(tmp_path)
    assert _eth(_node_block(text, "gem_ent_43")) == {
        "eth_static": "1",
        "eth_emissive": "0.55 0.8 0.65",
        "eth_color": "1 1 1 0.5",
    }
    assert any("gem.ent#43" in line and "not attached" in line for line in report.skipped)


def test_lightmap_needs_static_and_apply_light_and_a_sprite(tmp_path) -> None:
    text, report = _convert(tmp_path)
    assert text.count("metadata/eth_lightmap") == 1
    assert "eth_lightmap" not in _eth(_node_block(text, "crate_ent_46"))
    assert any("crate.ent#46" in line for line in report.skipped)
    # Instance ids are the join: the one attached file is the lamp's own add<id>.png.
    assert [rel for _, rel in report.lightmaps] == ["lamps/add42.png"]
    assert report.lighting_ungated["eth_lightmap"] == 3


def test_a_bmp_only_lightmap_is_reported_not_attached(tmp_path) -> None:
    # The engine falls back to add<id>.bmp (ETHScene.cpp:327-331); the converter does not
    # copy or attach one, and says so.
    lightmaps = tmp_path / "scenes" / "lamps-esc"
    lightmaps.mkdir(parents=True)
    (lightmaps / "add42.bmp").write_bytes(b"BM")
    text, report = tscn.write_scene(
        scene.parse(_LIGHTING_ESC, name="lamps"), root_name="lamps", lightmap_dir=lightmaps
    )
    assert "eth_lightmap" not in text
    assert report.lightmaps == []
    assert [line for line in report.skipped if "add" in line or ".bmp" in line] == [
        "lamp.ent#42: only a .bmp lightmap exists; not handled"
    ]


def test_no_key_when_every_value_is_the_default(tmp_path) -> None:
    text, _ = _convert(tmp_path)
    assert _eth(_node_block(text, "plain_ent_45")) == {}


def test_only_z_and_emissive_reach_a_node_with_neither_sprite_nor_light(tmp_path) -> None:
    # A spriteless marker keeps its lighting height: the port places its own actors (the
    # player, from main_char) at such nodes. static, applyLight and <Normal> are all
    # non-default here too, and are still not written.
    text, report = _convert(tmp_path)
    assert _eth(_node_block(text, "marker_44")) == {"eth_z": "5", "eth_emissive": "1 1 1"}
    assert report.lighting["eth_z"] == report.lighting_ungated["eth_z"] == 2
    assert report.lighting["eth_emissive"] == report.lighting_ungated["eth_emissive"] == 2
    # ...while the gated keys differ, so an inventory over all instances can be reconciled.
    assert (report.lighting["eth_static"], report.lighting_ungated["eth_static"]) == (3, 4)
    assert (report.lighting["eth_apply_light"], report.lighting_ungated["eth_apply_light"]) == (
        2,
        3,
    )
    assert "marker_nm.png" not in report.normals


def test_every_node_with_a_z_index_carries_eth_z_and_no_other_does(tmp_path) -> None:
    text, _ = _convert(tmp_path)
    blocks = _instance_blocks(text)
    assert len(blocks) == 8
    for block in blocks:
        has_z_index = any(line.startswith("z_index = ") for line in block)
        assert has_z_index == ("eth_z" in _eth(block)), block[0]


def test_default_light_still_writes_its_range_and_nothing_else(tmp_path) -> None:
    text, report = _convert(tmp_path)
    assert _eth(_node_block(text, "spark_47")) == {"eth_light_range": "256"}
    assert report.lighting["eth_light_range"] == 4
    assert report.static_lights == 2
    assert report.halos == {"glow.bmp", "ember_halo.png"}
    assert report.normals == {"lamp_nm.png", "crate_nm.png"}


def test_a_halo_at_its_defaults_writes_the_halo_and_none_of_its_numbers(tmp_path) -> None:
    text, _ = _convert(tmp_path)
    assert _eth(_node_block(text, "ember_ent_48")) == {
        "eth_light_offset": "0 0 7",
        "eth_light_range": "180",
        "eth_halo": "res://assets/entities/ember_halo.png",
    }


def test_halo_numbers_without_a_halo_bitmap_write_no_halo_key(tmp_path) -> None:
    # ETHEntityRenderingManager.cpp:78: no <HaloBitmap>, no halo piece, no consumer.
    text, report = _convert(tmp_path)
    assert _eth(_node_block(text, "beacon_49")) == {
        "eth_static": "1",
        "eth_light_offset": "6 -4 8",
        "eth_light_range": "120",
        "eth_light_color": "0.3 0.6 0.9",
    }
    assert report.lighting["eth_halo"] == 2
    assert report.lighting["eth_halo_offset"] == report.lighting["eth_halo_size"] == 1
    assert report.lighting["eth_halo_brightness"] == 1


def test_every_lighting_value_is_a_quoted_string(tmp_path) -> None:
    # The port pins "no bare numbers outside joints"; a Vector2 or a bare 3 would break it.
    text, _ = _convert(tmp_path)
    lines = [line for line in text.splitlines() if "eth_" in line]
    assert lines
    assert all(re.fullmatch(r'metadata/eth_[a-z_]+ = "[^"]*"', line) for line in lines)


def test_lighting_keys_are_appended_after_every_existing_line(tmp_path) -> None:
    # This is what makes a diff against the pre-lighting output pure insertions.
    text, _ = _convert(tmp_path)
    for block in _instance_blocks(text):
        flags = ["metadata/eth_" in line for line in block]
        first = flags.index(True) if True in flags else len(flags)
        assert all(flags[first:]), block[0]


def test_no_lighting_argument_means_no_lightmap_key() -> None:
    text, report = tscn.write_scene(scene.parse(_LIGHTING_ESC, name="lamps"))
    assert "eth_lightmap" not in text
    assert report.lightmaps == []


# --- copying -------------------------------------------------------------------------------


def test_copy_lighting_records_the_tier_and_refuses_a_tier_sibling(tmp_path) -> None:
    src = tmp_path / "in" / "assets"
    _png(src / "scenes" / "lamps-esc" / "add42.png", 16, 32)
    _png(src / "entities" / "normalmaps" / "lamp_nm.png", 32, 64)
    _png(src / "entities" / "normalmaps" / "script_only_nm.png", 8, 8)
    (src / "entities" / "glow.bmp").write_bytes(b"BM")
    (src / "entities" / "tiered.bmp").write_bytes(b"BM")
    (src / "entities" / "hd").mkdir()
    (src / "entities" / "hd" / "tiered.bmp").write_bytes(b"BM")  # the original would draw this

    report = copy_lighting_assets(
        tmp_path / "in",
        tmp_path / "out",
        lightmaps=[(src / "scenes" / "lamps-esc" / "add42.png", "lamps/add42.png")],
        normals={"lamp_nm.png", "missing_nm.png"},
        halos={"glow.bmp", "tiered.bmp"},
    )

    out = tmp_path / "out" / "assets"
    copied = out / "lightmaps" / "lamps" / "add42.png"
    assert copied.read_bytes() == (src / "scenes" / "lamps-esc" / "add42.png").read_bytes()
    assert read_size(copied) == (16, 32)
    # All normal maps, not only the referenced ones: scripts spawn entities too.
    assert (out / "entities" / "normalmaps" / "script_only_nm.png").is_file()
    assert (out / "entities" / "glow.bmp").is_file()
    assert not (out / "entities" / "tiered.bmp").exists()
    assert report.copied == {"lightmaps": 1, "normalmaps": 2, "halos": 1}
    assert any("tiered.bmp" in line and "hd/tiered.bmp" in line for line in report.skipped)
    assert any("missing_nm.png" in line for line in report.skipped)

    manifest = json.loads((out / MANIFEST_NAME).read_text(encoding="utf-8"))
    assert manifest["files"]["lightmaps/lamps/add42.png"] == {
        "source": "scenes/lamps-esc/add42.png",
        "tier": "1x",
    }
    assert {entry["tier"] for entry in manifest["files"].values()} == {"1x"}
    assert "entities/tiered.bmp" not in manifest["files"]


def test_a_missing_halo_source_is_skipped_and_not_recorded(tmp_path) -> None:
    (tmp_path / "in" / "assets" / "entities").mkdir(parents=True)
    report = copy_lighting_assets(
        tmp_path / "in", tmp_path / "out", lightmaps=[], normals=set(), halos={"absent.bmp"}
    )
    assert report.copied["halos"] == 0
    assert len(report.skipped) == 1
    assert report.skipped[0].startswith("entities/absent.bmp: source not found")
    manifest = json.loads((tmp_path / "out" / "assets" / MANIFEST_NAME).read_text("utf-8"))
    assert manifest["files"] == {}


def test_manifest_merges_across_runs_and_survives_a_corrupt_one(tmp_path) -> None:
    # Four `--world` runs share one output directory, so each run merges into the record.
    src = tmp_path / "in" / "assets"
    _png(src / "scenes" / "a-esc" / "add1.png", 4, 4)
    _png(src / "scenes" / "b-esc" / "add2.png", 4, 4)
    path = tmp_path / "out" / "assets" / MANIFEST_NAME
    path.parent.mkdir(parents=True)
    path.write_text("{not json", encoding="utf-8")

    def run(scene_name: str, instance_id: int) -> None:
        source = src / "scenes" / f"{scene_name}-esc" / f"add{instance_id}.png"
        copy_lighting_assets(
            tmp_path / "in",
            tmp_path / "out",
            lightmaps=[(source, f"{scene_name}/add{instance_id}.png")],
            normals=set(),
            halos=set(),
        )

    run("a", 1)  # the corrupt file is replaced, not fatal
    run("b", 2)
    manifest = json.loads(path.read_text(encoding="utf-8"))
    assert manifest == {
        "format": 1,
        "files": {
            "lightmaps/a/add1.png": {"source": "scenes/a-esc/add1.png", "tier": "1x"},
            "lightmaps/b/add2.png": {"source": "scenes/b-esc/add2.png", "tier": "1x"},
        },
    }


def test_cli_wires_lightmaps_and_copies_them(tmp_path) -> None:
    src = tmp_path / "in" / "assets"
    esc = src / "scenes" / "level0.esc"
    esc.parent.mkdir(parents=True)
    # UTF-16LE with a BOM, like the shipped scenes.
    esc.write_bytes(b"\xff\xfe" + _LIGHTING_ESC.encode("utf-16-le"))
    _png(src / "scenes" / "level0-esc" / "add42.png", 16, 32)
    _png(src / "entities" / "normalmaps" / "lamp_nm.png", 32, 64)
    (src / "entities" / "glow.bmp").write_bytes(b"BM")
    _png(src / "entities" / "ember_halo.png", 8, 8)

    out = tmp_path / "out"
    argv = ["-i", str(tmp_path / "in"), "-o", str(out), "--level", "0", "--copy-lighting"]
    assert main(argv) == 0

    level = (out / "levels" / "level0.tscn").read_text(encoding="utf-8")
    assert 'metadata/eth_lightmap = "res://assets/lightmaps/level0/add42.png"' in level
    assert (out / "assets" / "lightmaps" / "level0" / "add42.png").is_file()
    assert (out / "assets" / "entities" / "glow.bmp").is_file()
    assert (out / "assets" / "entities" / "ember_halo.png").is_file()
