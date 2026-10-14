"""A revolute joint's other end: attach point B, and which node it is on.

Fixture rule (see ../README.md): everything here is HAND-WRITTEN, invented names and numbers,
chosen so that A's box and scale cannot stand in for B's.

The engine rule under test (docs/ethanon-formats.md 6.4): ``ETHRevoluteJoint.cpp:35-44`` sets
``localAnchorA = attachPointA x half of A's scaled box`` and ``localAnchorB = attachPointB x
half of B's scaled box`` (``:60-64``, ``boxSize = collisionBox.size * scale * 0.5``), and Box2D
pulls the dynamic body until the two world points meet. B is the first entity the joint's
definition (``otherEntityName``) or its custom datum names (``ETHPhysicsEntityController.cpp:
252-298``): a ``string`` by name, an ``int`` by scene id, anything else no joint at all.
"""

from __future__ import annotations

import pytest

from ethanon2godot import scene, tscn

_PLANK_SCALE = '      <Scale x="2.000000" y="3.000000" />\n'
_PIVOT_SCALE = '      <Scale x="0.500000" y="4.000000" />\n'


def _joint(a: str, b: str, *, other: str = "", name: str = "revoluteJoint") -> str:
    # <Joints> is plain text in the files (ETHEntityProperties.cpp:528-530), a CDATA in some.
    extra = f"otherEntityName = {other}; " if other else ""
    return (
        f"        <Joints><![CDATA[{name}{{attachPointAX = {a[0]}; attachPointAY = {a[1]}; "
        f"attachPointBX = {b[0]}; attachPointBY = {b[1]}; {extra}"
        "enableLimit = true; lowerAngle = -1.0; upperAngle = 2.0;}]]></Joints>\n"
    )


def _datum(kind: str, name: str, value: str) -> str:
    return (
        "      <CustomData>\n"
        f"        <Variable><Type>{kind}</Type><Name>{name}</Name>"
        f"<Value>{value}</Value></Variable>\n"
        "      </CustomData>\n"
    )


def _instance(iid: int, name: str, body: str) -> str:
    return (
        f'  <Entity id="{iid}" spriteFrame="0">\n'
        f"    <EntityName>{name}</EntityName>\n"
        f'    <Position x="{iid}.000000" y="10.000000" z="0.000000" angle="0.000000" />\n'
        f"{body}"
        "  </Entity>\n"
    )


def _body(
    box: tuple[str, str] | None, *, static: int, scale: str = "", joint: str = "", data: str = ""
) -> str:
    collision = (
        ""
        if box is None
        else (
            "      <Collision>\n"
            '        <Position x="0.0" y="0.0" z="0.000000" />\n'
            f'        <Size x="{box[0]}" y="{box[1]}" z="1.000000" />\n'
            f"{joint}"
            "      </Collision>\n"
        )
    )
    return (
        f'    <Entity shape="1" applyLight="0" type="0" static="{static}" blendMode="0">\n'
        f"{scale}      <Sprite>wood.png</Sprite>\n"
        f"{collision}{data or '      <CustomData />' + chr(10)}"
        "    </Entity>\n"
    )


def _plank(
    iid: int,
    *,
    a: tuple[str, str] = ("1.0", "-1.0"),
    b: tuple[str, str] = ("-0.5", "0.25"),
    other: str = "",
    name: str = "revoluteJoint",
    data: str | None = None,
) -> str:
    # 40 x 10 at scale (2, 3): A's half box is (40, 15).
    data = _datum("string", name, "pivot") if data is None else data
    joint = _joint(a, b, other=other, name=name)
    box = ("40.0", "10.0")
    return _instance(
        iid, "plank", _body(box, static=0, scale=_PLANK_SCALE, joint=joint, data=data)
    )


def _pivot(iid: int, name: str = "pivot", box: tuple[str, str] | None = ("60.0", "20.0")) -> str:
    # 60 x 20 at scale (0.5, 4): B's half box is (15, 40).
    return _instance(iid, name, _body(box, static=1, scale=_PIVOT_SCALE))


def _convert(*instances: str) -> tuple[str, tscn.ConversionReport]:
    esc = (
        '<?xml version="1.0" ?>\n<Ethanon>\n  <EntitiesInScene>\n'
        + "".join(instances)
        + "  </EntitiesInScene>\n</Ethanon>\n"
    )
    return tscn.write_scene(scene.parse(esc, name="joint"), root_name="joint")


def _node(text: str, name: str) -> list[str]:
    lines = text.splitlines() + [""]
    start = lines.index(f'[node name="{name}" type="Node2D" parent="."]')
    return lines[start + 1 : lines.index("", start)]


def _meta(node: list[str], key: str) -> str | None:
    prefix = f"metadata/{key} = "
    found = [line[len(prefix) :] for line in node if line.startswith(prefix)]
    return found[0] if found else None


def test_attach_b_takes_entity_bs_box_and_scale_not_as() -> None:
    text, report = _convert(_plank(1), _pivot(2))
    node = _node(text, "plank_1")
    # A: (1, -1) x (40 * 2 * 0.5, 10 * 3 * 0.5) = (40, -15), as before this step.
    assert _meta(node, "joint_anchor_x") == "40"
    assert _meta(node, "joint_anchor_y") == "-15"
    # B: (-0.5, 0.25) x (60 * 0.5 * 0.5, 20 * 4 * 0.5) = (-7.5, 10). A's box and scale would
    # give (-10, 3.75) with A's box on B's scale, and (-30, 11.25) with B's box on A's.
    assert _meta(node, "joint_anchor_b_x") == "-7.5"
    assert _meta(node, "joint_anchor_b_y") == "10"
    assert _meta(node, "joint_other") == '"pivot_2"'
    assert not report.skipped


def test_the_node_named_is_the_first_of_that_entity_in_the_file() -> None:
    # ETHPhysicsEntityController.cpp:287-298 takes the first entity that matches and `break`s.
    text, _ = _convert(_plank(1), _pivot(5), _pivot(3))
    assert _meta(_node(text, "plank_1"), "joint_other") == '"pivot_5"'


def test_an_int_datum_is_a_scene_id_and_names_whatever_has_it() -> None:
    data = _datum("int", "revoluteJoint", "9")
    text, report = _convert(_plank(1, data=data), _pivot(2), _pivot(9, name="boulder"))
    node = _node(text, "plank_1")
    assert _meta(node, "joint_other") == '"boulder_9"'
    # B's box and scale are the NAMED entity's: boulder is the same 60 x 20 at (0.5, 4) here.
    assert _meta(node, "joint_anchor_b_x") == "-7.5"
    assert not report.skipped


def test_other_entity_name_in_the_joint_beats_the_custom_datum() -> None:
    # ETHPhysicsEntityController.cpp:266-268 reads the definition first; the datum is the fallback.
    text, _ = _convert(_plank(1, other="boulder"), _pivot(2), _pivot(3, name="boulder"))
    assert _meta(_node(text, "plank_1"), "joint_other") == '"boulder_3"'


def test_no_partner_means_no_joint_keys_and_a_report_line() -> None:
    text, report = _convert(_plank(1), _pivot(2, name="somebody_else"))
    node = _node(text, "plank_1")
    # A's half is unchanged, so the port can still say what is missing; B's is not guessed.
    assert _meta(node, "joint_anchor_x") == "40"
    for key in ("joint_other", "joint_anchor_b_x", "joint_anchor_b_y"):
        assert _meta(node, key) is None
    assert len(report.skipped) == 1 and report.skipped[0].startswith("plank#1: its revolute joint")


@pytest.mark.parametrize(
    "data",
    [
        _datum("float", "revoluteJoint", "2"),  # a type the engine does not read: `continue`
        _datum("string", "revoluteJoint0", "pivot"),  # keyed like another joint, not this one
        "      <CustomData />\n",  # nothing at all
    ],
)
def test_a_datum_the_engine_does_not_read_names_nobody(data: str) -> None:
    _, report = _convert(_plank(1, data=data), _pivot(2))
    assert len(report.skipped) == 1


def test_the_datum_is_keyed_like_the_joint() -> None:
    data = _datum("string", "revoluteJointA", "pivot")
    text, report = _convert(_plank(1, name="revoluteJointA", data=data), _pivot(2))
    assert _meta(_node(text, "plank_1"), "joint_other") == '"pivot_2"'
    assert not report.skipped


def test_an_absent_attach_point_b_is_zero_and_a_partner_without_a_box_has_none() -> None:
    plank = _plank(1).replace("attachPointBX = -0.5; attachPointBY = 0.25; ", "")
    text, _ = _convert(plank, _pivot(2))
    node = _node(text, "plank_1")
    assert _meta(node, "joint_anchor_b_x") == "0" and _meta(node, "joint_anchor_b_y") == "0"
    # A partner with no <Collision> has a box of 0 x 0 (the engine's default), so B is its centre.
    text, _ = _convert(_plank(1), _pivot(2, box=None))
    node = _node(text, "plank_1")
    assert _meta(node, "joint_other") == '"pivot_2"'
    assert _meta(node, "joint_anchor_b_x") == "0" and _meta(node, "joint_anchor_b_y") == "0"


def test_a_scene_without_a_joint_gains_no_key() -> None:
    text, report = _convert(_pivot(2), _pivot(3, name="other"))
    assert "joint_" not in text
    assert not report.skipped
