"""The reachability check on hand-authored scenes (fixtures/), never on a converted level.

Every fixture is the same 256 x 128 room: the spawn at (48, 96) on the left, the door's trigger at
(208, 96) on the right, and - in all but open.tscn - one body across the whole fall bound at x 128,
so the door is reached only through it, round it within the bound, or by a static portal. The
rules are the port's own committed tables (data/, game/data/), read as the tool reads them.
"""

from __future__ import annotations

import dataclasses
import json
import math
import shutil
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import reachability as r  # noqa: E402

FIXTURES = Path(__file__).resolve().with_name("fixtures")


@pytest.fixture(scope="module")
def rules() -> r.Rules:
    return r.load_rules()


def scene(name: str) -> list[r.Node]:
    return r.parse_tscn((FIXTURES / f"{name}.tscn").read_text(encoding="utf-8"))


def node(nodes: list[r.Node], name: str) -> r.Node:
    return next(n for n in nodes if n.name == name)


def judge(nodes: list[r.Node], rules: r.Rules) -> r.Verdict:
    return r.verdict("fixture", nodes, rules)


# ---- the reader ----------------------------------------------------------------------------


def test_the_reader_keeps_what_the_flood_needs():
    nodes = scene("breakable_stone")
    assert [n.name for n in nodes] == [
        "max_1", "main_char_2", "door_ent_3", "breakable_wall_4", "rolling_stone_ent_5",
    ]
    wall = node(nodes, "breakable_wall_4")
    assert (wall.entity, wall.pos, wall.body) == ("breakable_wall", (128.0, 64.0), "StaticBody2D")
    assert wall.flag("breakable") and not wall.flag("burnable")
    assert wall.shapes == [("poly", [(-8.0, -160.0), (8.0, -160.0), (8.0, 160.0), (-8.0, 160.0)])]
    stone = node(nodes, "rolling_stone_ent_5")
    assert (stone.body, stone.shapes) == ("RigidBody2D", [("circle", ((0.0, 0.0), 12.0))])
    door = node(nodes, "door_ent_3")
    assert door.vector("trigger_offset") == (0.0, 32.0)
    assert door.vector("trigger_size") == (8.0, 24.0)


def test_a_flag_is_read_as_the_port_reads_it():
    n = r.Node("n", "e", (0.0, 0.0), 0.0, {"a": '"1"', "b": '"0"', "c": '""', "d": '"yes"'})
    assert [n.flag(k) for k in "abcde"] == [True, False, False, True, False]


def test_a_turned_node_turns_its_shape():
    pillar = node(scene("rotated"), "pillar_ent_4")
    x, y = pillar.world((160.0, 0.0))
    assert math.isclose(x, 128.0, abs_tol=0.01) and math.isclose(y, 224.0, abs_tol=0.01)


# ---- what each body is -----------------------------------------------------------------------


def test_the_classes_follow_the_ports_tables(rules):
    wall = node(scene("breakable"), "breakable_wall_4")

    def kind(**changes) -> str:
        changes.setdefault("meta", dict(wall.meta))
        return r.classify(dataclasses.replace(wall, **changes), rules)

    assert kind() == r.Kind.BREAKABLE
    # demolish.json names breakable_wall: a wall without the flag is still one.
    assert kind(meta={}) == r.Kind.BREAKABLE
    # The flag alone makes any static body breakable.
    assert kind(entity="scenery.ent") == r.Kind.BREAKABLE
    assert kind(entity="scenery.ent", meta={}) == r.Kind.ROCK
    # Only a crate fades when it burns (fire.json fade_names); a burnable anything else holds.
    assert kind(entity="crate_no_emissive", meta={"burnable": '"1"'}) == r.Kind.BURNABLE
    assert kind(entity="shock_agent.ent", meta={"burnable": '"1"'}) == r.Kind.ROCK
    # A static body the port moves or opens, and every body that is not static.
    for moving in ("lift.ent", "door_lift", "door_locked.ent", "moving_platform", "seesaw"):
        assert kind(entity=moving, meta={}) == r.Kind.FREE, moving
    assert kind(entity="moving_platform_single.ent", meta={}) == r.Kind.ROCK
    for body in ("RigidBody2D", "Area2D"):
        assert kind(body=body) == r.Kind.FREE, body
    assert kind(shapes=[]) == r.Kind.FREE


# ---- the verdicts --------------------------------------------------------------------------


def test_an_open_room_is_reachable(rules):
    v = judge(scene("open"), rules)
    assert v.as_json() == {"label": "", "status": "reachable"}


def test_a_breakable_with_no_breaker_seals_the_door(rules):
    v = judge(scene("breakable"), rules)
    assert (v.status, v.sealed_by, v.opened_by) == ("sealed", ["breakable_wall_4"], {})


def test_a_stone_opens_every_breakable(rules):
    v = judge(scene("breakable_stone"), rules)
    assert (v.status, v.opened_by, v.sealed_by) == (
        "opened", {"stone": ["breakable_wall_4"]}, [])


def test_a_launcher_that_throws_a_stone_is_a_breaker_and_one_that_throws_else_is_not(rules):
    nodes = scene("breakable")
    launcher = r.Node("entity_launcher_9", "entity_launcher", (40.0, 20.0), 0.0,
                      {"entity": '"rolling_stone.ent"'})
    assert judge([*nodes, launcher], rules).status == "opened"
    other = dataclasses.replace(launcher, meta={"entity": '"barrel_bomb.ent"'})
    assert judge([*nodes, other], rules).status == "sealed"


def test_a_torch_opens_its_light_wall_and_nothing_else(rules):
    nodes = scene("breakable")
    wall = node(nodes, "breakable_wall_4")
    torch = r.Node("light_off_9", "light_off", (40.0, 20.0), 0.0, {"switch": '"1"'})
    light_wall = dataclasses.replace(wall, entity="light_wall.ent")
    lit = judge([n if n is not wall else light_wall for n in nodes] + [torch], rules)
    assert (lit.status, lit.opened_by) == ("opened", {"torch": ["breakable_wall_4"]})
    assert judge([*nodes, torch], rules).status == "sealed"


def test_the_dark_dragon_opens_its_wall(rules):
    nodes = scene("breakable")
    spawn = r.Node("dark_dragon_spawn_9", "dark_dragon_spawn", (40.0, 20.0), 0.0, {})
    v = judge([*nodes, spawn], rules)
    assert (v.status, v.opened_by) == ("opened", {"dark_dragon": ["breakable_wall_4"]})


def test_fire_burns_a_crate_that_fades(rules):
    nodes = scene("crate_fire")
    v = judge(nodes, rules)
    assert (v.status, v.opened_by) == ("opened", {"fire": ["crate_no_emissive_4"]})
    no_fire = [n for n in nodes if n.name != "carranca_right_ent_5"]
    assert judge(no_fire, rules).sealed_by == ["crate_no_emissive_4"]


def test_a_static_portal_pair_joins_two_rooms(rules):
    nodes = scene("static_portals")
    v = judge(nodes, rules)
    assert v.as_json() == {"label": "", "status": "reachable", "static_portals": True}
    unpaired = [n for n in nodes if n.name != "portal_static_6"]
    assert judge(unpaired, rules).status == "unreached"


def test_a_turned_pillar_seals_where_an_unturned_one_would_not(rules):
    nodes = scene("rotated")
    assert judge(nodes, rules).status == "unreached"
    pillar = node(nodes, "pillar_ent_4")
    flat = [n if n is not pillar else dataclasses.replace(pillar, rot=0.0) for n in nodes]
    assert judge(flat, rules).status == "reachable"


def test_the_fall_bound_is_rock_at_hazards_margin(rules):
    # The rock ends at y 200 and the bound at max.y + 96 = 224: the flood goes under it.
    nodes = scene("bound_gap")
    assert judge(nodes, rules).status == "reachable"
    tight = dataclasses.replace(rules, margin_px=(rules.margin_px[0], 72.0))
    assert judge(nodes, tight).status == "unreached"


def test_the_fewest_walls_are_named(rules):
    # Two breakables in series, and a third beside the path that nothing has to cross.
    nodes = scene("breakable")
    wall = node(nodes, "breakable_wall_4")
    second = dataclasses.replace(wall, name="breakable_wall_5", pos=(176.0, 64.0))
    aside = dataclasses.replace(wall, name="breakable_wall_6", pos=(24.0, -80.0), rot=math.pi / 2)
    v = judge([*nodes, second, aside], rules)
    assert (v.status, sorted(v.sealed_by)) == ("sealed", ["breakable_wall_4", "breakable_wall_5"])


# ---- the command ---------------------------------------------------------------------------


def write_levels(tmp: Path, names: list[str]) -> tuple[Path, Path]:
    levels = tmp / "levels"
    levels.mkdir()
    entries = []
    for i, name in enumerate(names):
        shutil.copy(FIXTURES / f"{name}.tscn", levels / f"{name}.tscn")
        entries.append({"index": i, "name": name})
    chapters = tmp / "chapters.json"
    chapters.write_text(json.dumps({"worlds": [{"index": 0, "levels": entries}]}), encoding="utf-8")
    return levels, chapters


def test_write_then_check_passes_and_a_moved_verdict_fails(tmp_path, capsys):
    levels, chapters = write_levels(tmp_path, ["open", "breakable", "breakable_stone"])
    pinned = tmp_path / "pinned.json"
    common = ["--levels-dir", str(levels), "--chapters", str(chapters), "--pinned", str(pinned)]
    assert r.main(["--write", *common]) == 0
    document = json.loads(pinned.read_text(encoding="utf-8"))
    assert document["counts"] == {"opened": 1, "reachable": 1, "sealed": 1}
    assert document["levels"]["breakable"] == {
        "label": "1-02", "status": "sealed", "sealed_by": ["breakable_wall_4"]}
    assert set(document["_breakers"]) == {"stone", "torch", "dark_dragon", "fire"}
    assert r.main(["--check", *common]) == 0

    document["levels"]["breakable"]["status"] = "opened"
    pinned.write_text(json.dumps(document), encoding="utf-8")
    capsys.readouterr()
    assert r.main(["--check", *common]) == 1
    out = capsys.readouterr().out
    assert "breakable: pinned" in out and "1 differ" in out


def test_no_levels_is_a_skip_rather_than_a_pass(tmp_path):
    missing = ["--levels-dir", str(tmp_path / "none"), "--chapters", str(tmp_path / "none.json")]
    assert r.main(["--check", *missing]) == r.EXIT_SKIPPED == 77
