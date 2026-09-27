"""The entity `<Scale>`: a Sprite2D `scale` line, and every collider written with it baked in.

Fixture rule (see ../README.md): everything here is HAND-WRITTEN. The .esc below has the
shape of a level's scaled placements - an inline <Entity> with a <Scale>, a <Sprite> and a
<Collision> - with invented names. The barrel's numbers (Scale 1.3 on a 22 x 29 box) are the
plan's own fixture (00_order 9.2); the rest are chosen to separate x from y.

The engine rule under test (tscn.py's docstrings, docs/ethanon-formats.md "2026-09-27
(scale)"): the picture is the frame times the scale about the pivot times the scale
(ETHSpriteEntity.cpp:682, ETHEntity.cpp:296), which is a Godot Sprite2D's own `scale` over
its `offset`; the body is built from `box.Scale(scale)` (ETHPhysicsSimulator.cpp:72-73).
"""

from __future__ import annotations

import difflib
import re

from ethanon2godot import scene, tscn
from ethanon2godot.__main__ import main

# Where a body takes its <Scale> line, if it has one.
_SCALE_SLOT = "<!-- scale -->"


def _instance(iid: int, name: str, body: str, *, scale: tuple[str, str] | None) -> str:
    scale_line = f'      <Scale x="{scale[0]}" y="{scale[1]}" />\n' if scale else ""
    return (
        f'  <Entity id="{iid}" spriteFrame="0">\n'
        f"    <EntityName>{name}</EntityName>\n"
        f'    <Position x="{iid}.000000" y="10.000000" z="0.000000" angle="0.000000" />\n'
        f"{body.replace(_SCALE_SLOT, scale_line)}"
        "  </Entity>\n"
    )


def _box(w: str, h: str, *, pos: tuple[str, str] = ("0.0", "0.0"), extra: str = "") -> str:
    return (
        "      <Collision>\n"
        f'        <Position x="{pos[0]}" y="{pos[1]}" z="0.000000" />\n'
        f'        <Size x="{w}" y="{h}" z="0.000000" />\n'
        f"{extra}"
        "      </Collision>\n"
    )


def _body(shape: int, sprite: str, collision: str, *, static: int = 0, pivot: str = "") -> str:
    return (
        f'    <Entity shape="{shape}" applyLight="0" type="0" static="{static}" blendMode="0">\n'
        f"{_SCALE_SLOT}"
        f"      <Sprite>{sprite}</Sprite>\n"
        f"{pivot}{collision}"
        "      <CustomData />\n"
        "    </Entity>\n"
    )


_POLYGON = "        <Polygon><![CDATA[v0{x=-1;y=-1;} v1{x=1;y=-1;} v2{x=1;y=1;}]]></Polygon>\n"
_COMPOUND = (
    "        <Compound><![CDATA["
    "hull{shape=box;posX=0.5;posY=-0.5;sizeX=1.0;sizeY=0.5;} "
    "knob{shape=circle;posX=-0.5;posY=0.25;radius=0.5;}"
    "]]></Compound>\n"
)
_JOINT = (
    "        <Joints><![CDATA[revoluteJoint{attachPointAX=1.0;attachPointAY=-1.0;"
    "lowerAngle=0.5;}]]></Joints>\n"
)

# (id, name, body) - each body with a slot for the <Scale> line.
_PLACEMENTS = (
    # The plan's fixture: a dynamic box at 1.3.
    (71, "keg.ent", _body(1, "keg.png", _box("22.000000", "29.000000"))),
    # A circle at 0.8 on a 60 x 60 box: radius 30 -> 24.
    (72, "boulder", _body(2, "boulder.png", _box("60.000000", "60.000000"))),
    # A non-uniform circle: (sx * w + sy * h) / 4, not a mean of the scales.
    (73, "pebble", _body(2, "pebble.png", _box("40.000000", "20.000000"))),
    # A static box with an offset, non-uniform.
    (74, "slab.ent", _body(1, "slab.png", _box("10.0", "20.0", pos=("4.0", "-6.0")), static=1)),
    # A polygon: (size * scale * 0.5) * v + pos * scale.
    (75, "ramp", _body(3, "ramp.png", _box("64.0", "32.0", pos=("2.0", "1.0"), extra=_POLYGON),
                       static=1)),
    # A compound: its fixtures against the scaled box; the circle's radius on x alone.
    (76, "anchor", _body(4, "anchor.png", _box("40.0", "20.0", extra=_COMPOUND), static=1)),
    # A jointed box: its anchor against the scaled box, its angles untouched.
    (77, "gate", _body(1, "gate.png", _box("20.0", "10.0", extra=_JOINT))),
    # Shape 0 with a box and a pivot: a trigger, unscaled, and an offset, unscaled.
    (78, "disc.ent", _body(0, "disc.png", _box("30.0", "12.0", pos=("1.0", "2.0")), static=1,
                           pivot='      <PivotAdjust x="0.000000" y="15.000000" />\n')),
)
_SCALES = {
    71: ("1.300000", "1.300000"),
    72: ("0.800000", "0.800000"),
    73: ("2.000000", "0.500000"),
    74: ("2.000000", "0.500000"),
    75: ("2.000000", "3.000000"),
    76: ("2.000000", "3.000000"),
    77: ("2.000000", "3.000000"),
    78: ("2.000000", "2.000000"),
}


def _esc(scales: dict[int, tuple[str, str]]) -> str:
    return (
        '<?xml version="1.0" ?>\n'
        "<Ethanon>\n"
        "  <EntitiesInScene>\n"
        + "".join(_instance(i, n, b, scale=scales.get(i)) for i, n, b in _PLACEMENTS)
        + "  </EntitiesInScene>\n"
        "</Ethanon>\n"
    )


def _convert(scales: dict[int, tuple[str, str]]) -> tuple[str, tscn.ConversionReport]:
    return tscn.write_scene(scene.parse(_esc(scales), name="scaled"), root_name="scaled")


def _block(text: str, header: str) -> list[str]:
    """The lines of the block that starts with ``header``, up to the blank line or the end."""
    lines = text.splitlines() + [""]
    start = lines.index(header)
    end = lines.index("", start)
    return lines[start + 1 : end]


def _sprite(text: str, node: str) -> list[str]:
    return _block(text, f'[node name="Sprite" type="Sprite2D" parent="{node}"]')


def _resource(text: str, kind: str, rid: str) -> list[str]:
    return _block(text, f'[sub_resource type="{kind}" id="{rid}"]')


def test_a_barrel_at_1_3_gets_a_sprite_scale_and_a_box_28_6_by_37_7() -> None:
    text, report = _convert(_SCALES)
    sprite = _sprite(text, "keg_ent_71")
    assert sprite == ['texture = ExtResource("tex_0")', "scale = Vector2(1.3, 1.3)"]
    box = _resource(text, "RectangleShape2D", "RectangleShape2D_71")
    assert box == ["size = Vector2(28.6, 37.7)"]
    # The node and the body carry no scale: the engine scales the box it builds a body from.
    assert _block(text, '[node name="Body" type="RigidBody2D" parent="keg_ent_71"]') == []
    node = _block(text, '[node name="keg_ent_71" type="Node2D" parent="."]')
    assert not any(line.startswith("scale") for line in node)
    assert report.scaled_sprites == 8
    assert report.scaled_colliders == 7


def test_a_circle_takes_its_scale_on_both_half_extents() -> None:
    text, _ = _convert(_SCALES)
    # (0.8 * 60 + 0.8 * 60) / 4 = 24; (2 * 40 + 0.5 * 20) / 4 = 22.5 (ETHPhysicsSimulator.cpp:59)
    assert _resource(text, "CircleShape2D", "CircleShape2D_72") == ["radius = 24"]
    assert _resource(text, "CircleShape2D", "CircleShape2D_73") == ["radius = 22.5"]
    assert "scale = Vector2(0.8, 0.8)" in _sprite(text, "boulder_72")
    assert "scale = Vector2(2, 0.5)" in _sprite(text, "pebble_73")


def test_a_box_offset_is_scaled_per_axis() -> None:
    text, _ = _convert(_SCALES)
    assert _resource(text, "RectangleShape2D", "RectangleShape2D_74") == ["size = Vector2(20, 10)"]
    shape = _block(text, '[node name="Shape" type="CollisionShape2D" parent="slab_ent_74/Body"]')
    assert shape == ['shape = SubResource("RectangleShape2D_74")', "position = Vector2(8, -3)"]


def test_a_polygon_is_built_on_the_scaled_box() -> None:
    text, _ = _convert(_SCALES)
    shape = _block(text, '[node name="Shape" type="CollisionPolygon2D" parent="ramp_75/Body"]')
    # (64 * 2 * 0.5) * v + 2 * 2 = +-64 + 4; (32 * 3 * 0.5) * v + 1 * 3 = +-48 + 3
    assert shape == ["polygon = PackedVector2Array(-60, -45, 68, -45, 68, 51)"]


def test_a_compound_is_resolved_against_the_scaled_box() -> None:
    text, _ = _convert(_SCALES)
    # box 40 x 20 at (2, 3): hull offset (0.5 * 80 * 0.5, -0.5 * 60 * 0.5) = (20, -15), extents
    # (1.0 * 80 * 0.5, 0.5 * 60 * 0.5) = (40, 15); knob offset (-20, 7.5), radius 0.5 * 80 * 0.5
    # = 20, on x alone (ETHCompoundShape.cpp:69), not 30.
    assert _resource(text, "RectangleShape2D", "Compound0_76") == ["size = Vector2(40, 15)"]
    assert _resource(text, "CircleShape2D", "Compound1_76") == ["radius = 20"]
    hull = _block(text, '[node name="Shape0" type="CollisionShape2D" parent="anchor_76/Body"]')
    knob = _block(text, '[node name="Shape1" type="CollisionShape2D" parent="anchor_76/Body"]')
    assert hull[-1] == "position = Vector2(20, -15)"
    assert knob[-1] == "position = Vector2(-20, 7.5)"


def test_a_joint_anchor_takes_the_scale_and_its_angles_do_not() -> None:
    text, _ = _convert(_SCALES)
    node = _block(text, '[node name="gate_77" type="Node2D" parent="."]')
    # attachPoint (1, -1) against the scaled half box (20 * 2 * 0.5, 10 * 3 * 0.5) = (20, 15)
    # (ETHRevoluteJoint.cpp:62); the lower angle stays 0.5 rad.
    assert "metadata/joint_anchor_x = 20" in node
    assert "metadata/joint_anchor_y = -15" in node
    assert "metadata/joint_lower_angle = 0.5" in node


def test_the_offset_and_a_trigger_box_stay_unscaled() -> None:
    text, _ = _convert(_SCALES)
    # The pivot is written as it stands; the Sprite2D's scale multiplies it, as the engine's does.
    assert _sprite(text, "disc_ent_78") == [
        'texture = ExtResource("tex_7")',
        "offset = Vector2(0, -15)",
        "scale = Vector2(2, 2)",
    ]
    # A script's GetCollisionBox() returns the box as read (ETHEntity.cpp:314-320).
    node = _block(text, '[node name="disc_ent_78" type="Node2D" parent="."]')
    assert "metadata/trigger_size = Vector2(30, 12)" in node
    assert "metadata/trigger_offset = Vector2(1, 2)" in node


def test_a_scale_of_one_writes_nothing() -> None:
    ones = {i: ("1.000000", "1.000000") for i in _SCALES}
    with_ones, report = _convert(ones)
    without, _ = _convert({})
    assert with_ones == without
    assert "scale = " not in without
    assert report.scaled_sprites == report.scaled_colliders == 0


def test_the_scale_changes_only_its_own_lines() -> None:
    # Converted with every <Scale> removed, the scene is the scaled one less exactly the scale
    # lines, with only collider values changed: the same diff the regeneration is held to.
    scaled, _ = _convert(_SCALES)
    plain, _ = _convert({})
    inserted, changed = [], []
    matcher = difflib.SequenceMatcher(a=plain.splitlines(), b=scaled.splitlines(), autojunk=False)
    for op, _i1, _i2, j1, j2 in matcher.get_opcodes():
        if op == "insert":
            inserted += scaled.splitlines()[j1:j2]
        elif op == "replace":
            changed += scaled.splitlines()[j1:j2]
        else:
            assert op == "equal"
    assert len(inserted) == 8 and all(line.startswith("scale = Vector2(") for line in inserted)
    value = re.compile(r"^(size = |radius = |position = |polygon = |metadata/joint_anchor_[xy] = )")
    assert changed and all(value.match(line) for line in changed)


def test_a_scale_not_above_zero_is_refused_not_written() -> None:
    for bad in (("0.000000", "1.000000"), ("1.000000", "-0.500000")):
        text, report = _convert({71: bad})
        assert "keg_ent_71" not in text
        assert any(line.startswith("keg.ent#71: <Scale>") for line in report.skipped)
        assert "scale = " not in text


def test_the_cli_reports_the_scaled_sprites_and_colliders(tmp_path, capsys) -> None:
    src = tmp_path / "in" / "assets" / "scenes"
    src.mkdir(parents=True)
    # UTF-16LE with a BOM, like the shipped scenes.
    (src / "level0.esc").write_bytes(b"\xff\xfe" + _esc(_SCALES).encode("utf-16-le"))
    argv = ["-i", str(tmp_path / "in"), "-o", str(tmp_path / "out"), "--level", "0"]
    assert main(argv) == 0
    assert "scaled sprites: 8; colliders with the scale baked in: 7" in capsys.readouterr().out
    level = (tmp_path / "out" / "levels" / "level0.tscn").read_text(encoding="utf-8")
    assert level.count("\nscale = Vector2(") == 8
