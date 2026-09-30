"""Can the player reach each level's door in the port? A static check over the converted levels.

A 4 px free-space flood from the player's spawn to the door's trigger, read from the `.tscn`
files the converter writes and the port plays (game/sim/Tscn.cpp). It is optimistic about
everything that moves or opens and strict about rock: the question it answers is "is the door
sealed off", not "is the level easy". A door it calls reachable may still need a route no
player can find (that is what the win routes in tests/routes/ are for); a door it calls sealed
cannot be reached however the level is played in the port, short of a mechanism the port does
not build.

What each body is, by the port's own rules (read from data/ and game/data/, never restated):
- ROCK: a static body with no role that moves it and no flag that removes it. The fall bound
  (hazards.json `bounds`: the `max` node plus margin_px, where the port kills the player) is
  rock too.
- FREE: every body that is not static (a crate, a stone, a barrel), every sensor (Area2D), and
  the static bodies whose role moves or opens them: lifts, live moving platforms, switched and
  locked doors, hinges (FREE_ROLES below).
- CONDITIONAL: a static body the port can take away, but only by a mechanism - a breakable
  (flagged, or named in demolish.json) and a crate that fades when it burns (fire.json
  `fade_names`). Each is OPENED only where the level holds a breaker the port has built for it
  (BREAKERS below); otherwise it stays solid.
- A static portal pair (entity_roles.json `static_portal`, index -> destiny) joins the flood
  across: a cell of one's trigger reaches every cell of the other's.

The verdict per level:
- `reachable`: the door is reached with every conditional body solid;
- `opened`: only through conditional bodies the port opens (`opened_by`: breaker -> nodes);
- `sealed`: only through a conditional body nothing in the port opens (`sealed_by`: nodes);
- `unreached`: not even with every conditional body open - a gap in this model, never a pin.
`sealed_by` and `opened_by` are the bodies on the cheapest path, where crossing a body the
port cannot open costs far more than one it can: the fewest walls that have to go.

Usage, from the repository root with the parity venv (README, *Parity work*):
    tools/parity/.venv/Scripts/python tools/parity/reachability.py --check
        rerun over out/levels and compare with the pinned tools/parity/reachability.json;
        exit 1 and print each difference when one is found
    tools/parity/.venv/Scripts/python tools/parity/reachability.py --write
        rerun and rewrite the pinned JSON (a step that changes a verdict says why)
    tools/parity/.venv/Scripts/python tools/parity/reachability.py level11a level8a
        print the verdict for these levels
A tree with no out/ of its own (a worktree) passes --levels-dir and --chapters for the checkout
that has them; ctest runs --check as parity_reachability with the build's configured paths.
Without the converted levels it exits 77, which ctest reports SKIPPED.
"""

from __future__ import annotations

import argparse
import heapq
import json
import math
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PINNED = Path(__file__).resolve().with_name("reachability.json")

# The flood's cell. The audit's (out/parity/audit_2026-09-29/check_levels/flood.py) and fine
# enough for every wall the 128 levels place (the thinnest is 16 px); a gap narrower than a cell
# can leak, which is the optimistic side of the question.
CELL_PX = 4.0
# How near the door's trigger box a reached cell must lie: a point flood carries no body, and the
# player's own half-width is 10 px (data/player.json), so 8 keeps the test inside it.
DOOR_TOLERANCE_PX = 8.0
# The cost of crossing a conditional body on the way: one the port opens, one it does not.
COST_OPENED = 1
COST_SEALED = 1000
# What the command exits with when there are no converted levels to read.
EXIT_SKIPPED = 77


class Kind:
    ROCK = "rock"
    FREE = "free"
    BREAKABLE = "breakable"
    BURNABLE = "burnable"


class Status:
    REACHABLE = "reachable"
    OPENED = "opened"
    SEALED = "sealed"
    UNREACHED = "unreached"


class Breaker:
    STONE = "stone"
    BARREL = "barrel"
    TORCH = "torch"
    DARK_DRAGON = "dark_dragon"
    FIRE = "fire"
    RED_KEY = "red_key"


# Roles (data/entity_roles.json) whose static body the port moves or opens, with where it does.
FREE_ROLES = {
    "lift": "game/sim/Mover.cpp: a lift runs between its two markers",
    "moving_platform": "game/sim/Mover.cpp: the one live moving platform",
    "switched_door": (
        "game/sim/Mover.cpp: a door_lift rises all the way from one press of its button and "
        "falls only from fully raised (step 133)"
    ),
    "locked_door": "game/sim/Keys.cpp: a locked door goes when its key reaches the keyhole",
    "hinge": "game/sim/Hinge.cpp: a seesaw or a spinning platform turns on its joint",
}

# What the port breaks or burns a conditional body with, and where. A step that builds another
# breaker adds its row here and reruns --write.
BREAKERS = {
    Breaker.STONE: (
        "every breakable on a level that holds a demolisher: a rolling stone, a launcher that "
        "throws one, or the beholder's rocks (game/sim/Demolish.cpp:87-101 State::Tick, "
        ":195-201 the stones, Game.cpp:311 a thrown one, Game.cpp:833 the boss's rock)"
    ),
    Breaker.BARREL: (
        "a flagged breakable whose entity demolish.json blast.entities lists, on a level with a "
        "barrel (metadata/explosive) and fire to set one off: a fire agent, a carranca or a fire "
        "diamond (game/sim/Fire.cpp:44-73 the blast's grab, Game.cpp:353-354 Demolish's "
        "Grab and Destroyed on the blast's tick, Demolish.cpp:129-140, :152-166)"
    ),
    Breaker.TORCH: (
        "the torch's light wall (torch.json wall.entity_name) on a level with a torch "
        "(game/sim/Game.cpp:702-706, Demolish.cpp:103-115 Break)"
    ),
    Breaker.DARK_DRAGON: (
        "the dark dragon's wall (darkdragon.json summon.wall, the last node of that name) on a "
        "level with its spawn (game/sim/Game.cpp:733-737, DarkDragon.cpp:797-799)"
    ),
    Breaker.FIRE: (
        "a crate that fades when it burns (fire.json burn.fade_names), on a level with fire: a "
        "fire agent, a carranca or a fire diamond (game/sim/Fire.cpp:206-210 a fire agent, "
        "Game.cpp:417-422 a fireball, Game.cpp:539 a fire diamond's; Fire.cpp:241 the fade)"
    ),
    Breaker.RED_KEY: (
        "the first breakable whose entity is exactly keys.json red_key_state.wall_entity, on a "
        "level that places a red_key_state (entity_roles.json) and a node whose entity is exactly "
        "red_key_state.key_entity: 6000 ms after that key is taken (game/sim/RedKey.cpp:104-134 "
        "State::Tick, Game.cpp:491-495 Demolish's SeekAndDestroy and Destroyed, "
        "Demolish.cpp:142-150)"
    ),
}
# The roles and entities that put fire in a level, for Breaker.FIRE.
FIRE_ROLES = ("fire_source", "turret")
FIRE_ENTITIES = ("fire_diamond.ent",)
# The role of 4-19's red key state (entity_roles.json; game/sim/Roles.hpp kRedKeyState).
RED_KEY_ROLE = "red_key_state"
BOSS_ROCK_SPAWN = "adder"


# ---- the level file ------------------------------------------------------------------------


@dataclass
class Node:
    name: str
    entity: str
    pos: tuple[float, float]
    rot: float
    meta: dict[str, str]
    body: str | None = None
    # Shapes in the node's own frame: ("poly", [(x, y), ...]) or ("circle", ((x, y), r)).
    shapes: list[tuple[str, object]] = field(default_factory=list)

    def flag(self, key: str) -> bool:
        """A flag as the port reads one (Demolish.cpp Flagged): present, not "" and not 0."""
        value = self.meta.get(key)
        if value is None:
            return False
        text = value.strip().strip('"')
        if text == "":
            return False
        try:
            return float(text) != 0.0
        except ValueError:
            return True

    def text(self, key: str, default: str = "") -> str:
        value = self.meta.get(key)
        return default if value is None else value.strip().strip('"')

    def vector(self, key: str) -> tuple[float, float] | None:
        match = re.search(r"Vector2\(([^)]*)\)", self.meta.get(key, ""))
        if not match:
            return None
        x, y = (float(v) for v in match.group(1).split(","))
        return (x, y)

    def world(self, point: tuple[float, float]) -> tuple[float, float]:
        c, s = math.cos(self.rot), math.sin(self.rot)
        x, y = point
        return (self.pos[0] + x * c - y * s, self.pos[1] + x * s + y * c)


_BLOCK = re.compile(r"^\[(\w+)([^\]]*)\]\s*$")
_ATTR = re.compile(r'(\w+)="([^"]*)"')
_BODY_TYPES = ("StaticBody2D", "RigidBody2D", "Area2D", "CharacterBody2D", "AnimatableBody2D")


def _numbers(text: str) -> list[float]:
    return [float(v) for v in text.split(",") if v.strip()]


def parse_tscn(text: str) -> list[Node]:
    """The top-level nodes of a converted level, each with its body type and shapes."""
    blocks: list[tuple[str, dict[str, str], list[str]]] = []
    for line in text.splitlines():
        header = _BLOCK.match(line)
        if header:
            blocks.append((header.group(1), dict(_ATTR.findall(header.group(2))), []))
        elif blocks:
            blocks[-1][2].append(line)

    subs: dict[str, tuple[str, dict[str, str]]] = {}
    nodes: dict[str, Node] = {}
    order: list[str] = []
    for kind, attrs, lines in blocks:
        props = {}
        for line in lines:
            if " = " in line:
                key, value = line.split(" = ", 1)
                props[key.strip()] = value.strip()
        if kind == "sub_resource":
            subs[attrs.get("id", "")] = (attrs.get("type", ""), props)
            continue
        if kind != "node" or "parent" not in attrs:
            continue
        parent, node_type, name = attrs["parent"], attrs.get("type", ""), attrs.get("name", "")
        if parent == ".":
            pos = re.search(r"Vector2\(([^)]*)\)", props.get("position", ""))
            meta = {k[len("metadata/"):]: v for k, v in props.items() if k.startswith("metadata/")}
            nodes[name] = Node(
                name=name,
                entity=meta.get("entity_name", "").strip('"'),
                pos=tuple(_numbers(pos.group(1))) if pos else (0.0, 0.0),
                rot=float(props.get("rotation", "0")),
                meta=meta,
            )
            order.append(name)
            continue
        owner = nodes.get(parent.split("/")[0])
        if owner is None:
            continue
        if node_type in _BODY_TYPES:
            owner.body = node_type
            continue
        offset = re.search(r"Vector2\(([^)]*)\)", props.get("position", ""))
        ox, oy = _numbers(offset.group(1)) if offset else (0.0, 0.0)
        if node_type == "CollisionPolygon2D":
            pts = _numbers(re.search(r"\(([^)]*)\)", props.get("polygon", "()")).group(1))
            owner.shapes.append(
                ("poly", [(pts[i] + ox, pts[i + 1] + oy) for i in range(0, len(pts) - 1, 2)])
            )
        elif node_type == "CollisionShape2D":
            ref = re.search(r'SubResource\("([^"]+)"\)', props.get("shape", ""))
            shape_type, shape = subs.get(ref.group(1), ("", {})) if ref else ("", {})
            if shape_type == "RectangleShape2D":
                w, h = _numbers(re.search(r"\(([^)]*)\)", shape["size"]).group(1))
                owner.shapes.append(
                    ("poly", [(ox - w / 2, oy - h / 2), (ox + w / 2, oy - h / 2),
                              (ox + w / 2, oy + h / 2), (ox - w / 2, oy + h / 2)])
                )
            elif shape_type == "CircleShape2D":
                owner.shapes.append(("circle", ((ox, oy), float(shape["radius"]))))
    return [nodes[name] for name in order]


# ---- the port's rules ----------------------------------------------------------------------


@dataclass
class Rules:
    roles: dict[str, str]
    breakable_names: list[str]
    fade_names: list[str]
    demolisher_throwables: list[str]
    torch_wall: str
    dark_dragon_spawn: str
    dark_dragon_wall: str
    margin_px: tuple[float, float]
    blast_entities: list[str] = field(default_factory=list)
    red_key_key: str = ""
    red_key_wall: str = ""


def _read(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def load_rules(root: Path = ROOT) -> Rules:
    """The port's own tables, from the files it reads them from."""
    roles = {}
    for role, entry in _read(root / "data" / "entity_roles.json")["roles"].items():
        for name in entry["names"]:
            roles[name] = role
    game = root / "game" / "data"
    summon = next(
        block for block in _read(game / "darkdragon.json").values()
        if isinstance(block, dict) and "wall" in block and "spawn" in block
    )
    throwables = _read(game / "launchers.json")["throwables"]
    margin = _read(game / "hazards.json")["bounds"]["margin_px"]
    red_key = _read(game / "keys.json")["red_key_state"]
    return Rules(
        roles=roles,
        breakable_names=list(_read(game / "demolish.json")["breakable"]["names"]),
        fade_names=list(_read(game / "fire.json")["burn"]["fade_names"]),
        demolisher_throwables=[
            name for name, entry in throwables.items()
            if not name.startswith("_") and entry.get("demolisher") is True
        ],
        torch_wall=_read(game / "torch.json")["wall"]["entity_name"],
        dark_dragon_spawn=summon["spawn"],
        dark_dragon_wall=summon["wall"],
        margin_px=(float(margin[0]), float(margin[1])),
        blast_entities=list(_read(game / "demolish.json")["blast"]["entities"]),
        red_key_key=red_key["key_entity"],
        red_key_wall=red_key["wall_entity"],
    )


def classify(node: Node, rules: Rules) -> str:
    if node.body is None or not node.shapes:
        return Kind.FREE
    if node.body != "StaticBody2D":
        return Kind.FREE
    if rules.roles.get(node.entity) in FREE_ROLES:
        return Kind.FREE
    if node.flag("breakable") or node.entity in rules.breakable_names:
        return Kind.BREAKABLE
    if node.flag("burnable") and node.entity in rules.fade_names:
        return Kind.BURNABLE
    return Kind.ROCK


def openers(nodes: list[Node], rules: Rules) -> dict[str, str]:
    """Each conditional body the port can open on this level, and by what."""
    role_of = {n.name: rules.roles.get(n.entity, "") for n in nodes}
    roles_here = set(role_of.values())
    stone = "demolisher" in roles_here or any(
        role_of[n.name] == "launcher" and n.text("entity") in rules.demolisher_throwables
        for n in nodes
    )
    if any(role_of[n.name] == "boss_spawn" and n.entity == BOSS_ROCK_SPAWN for n in nodes):
        stone = True
    fire = bool(roles_here.intersection(FIRE_ROLES)) or any(
        n.entity in FIRE_ENTITIES for n in nodes
    )
    barrel = fire and any(n.flag("explosive") for n in nodes)
    torch = "torch" in roles_here
    # SeekEntity finds the first entity of the exact name; the port's Demolish, the first
    # breakable standing in the level's order.
    red_key_wall = None
    if RED_KEY_ROLE in roles_here and any(n.entity == rules.red_key_key for n in nodes):
        red_key_wall = next(
            (n.name for n in nodes
             if n.entity == rules.red_key_wall and classify(n, rules) == Kind.BREAKABLE),
            None,
        )
    dragon_wall = None
    if any(n.entity == rules.dark_dragon_spawn for n in nodes):
        for n in nodes:
            if n.entity == rules.dark_dragon_wall:
                dragon_wall = n.name
    out: dict[str, str] = {}
    for n in nodes:
        kind = classify(n, rules)
        if kind == Kind.BREAKABLE:
            if n.name == dragon_wall:
                out[n.name] = Breaker.DARK_DRAGON
            elif torch and n.entity == rules.torch_wall:
                out[n.name] = Breaker.TORCH
            elif n.name == red_key_wall:
                out[n.name] = Breaker.RED_KEY
            elif stone:
                out[n.name] = Breaker.STONE
            elif barrel and n.flag("breakable") and n.entity in rules.blast_entities:
                out[n.name] = Breaker.BARREL
        elif kind == Kind.BURNABLE and fire:
            out[n.name] = Breaker.FIRE
    return out


# ---- the flood -----------------------------------------------------------------------------


def _inside_poly(x: float, y: float, pts: list[tuple[float, float]]) -> bool:
    inside = False
    j = len(pts) - 1
    for i in range(len(pts)):
        xi, yi = pts[i]
        xj, yj = pts[j]
        if (yi > y) != (yj > y) and x < (xj - xi) * (y - yi) / (yj - yi) + xi:
            inside = not inside
        j = i
    return inside


@dataclass
class Grid:
    x0: float
    y0: float
    w: int
    h: int
    # 0 free, 1 rock, 2 + k the conditional body bodies[k].
    cells: list[int]
    bodies: list[str]

    def index(self, x: float, y: float) -> int | None:
        cx, cy = int((x - self.x0) // CELL_PX), int((y - self.y0) // CELL_PX)
        if 0 <= cx < self.w and 0 <= cy < self.h:
            return cy * self.w + cx
        return None

    def centre(self, i: int) -> tuple[float, float]:
        return (self.x0 + (i % self.w + 0.5) * CELL_PX, self.y0 + (i // self.w + 0.5) * CELL_PX)

    def box(self, lo: tuple[float, float], hi: tuple[float, float]) -> list[int]:
        """Every cell whose centre lies inside the box."""
        out = []
        cx0 = max(0, math.ceil((lo[0] - self.x0) / CELL_PX - 0.5))
        cx1 = min(self.w - 1, math.floor((hi[0] - self.x0) / CELL_PX - 0.5))
        cy0 = max(0, math.ceil((lo[1] - self.y0) / CELL_PX - 0.5))
        cy1 = min(self.h - 1, math.floor((hi[1] - self.y0) / CELL_PX - 0.5))
        for cy in range(cy0, cy1 + 1):
            out.extend(range(cy * self.w + cx0, cy * self.w + cx1 + 1))
        return out


def rasterise(nodes: list[Node], rules: Rules, bound_max: tuple[float, float]) -> Grid:
    mx, my = rules.margin_px
    x0, y0 = -mx, -my
    w = math.ceil((bound_max[0] + mx - x0) / CELL_PX)
    h = math.ceil((bound_max[1] + my - y0) / CELL_PX)
    grid = Grid(x0, y0, w, h, [0] * (w * h), [])
    for node in nodes:
        kind = classify(node, rules)
        if kind == Kind.FREE:
            continue
        value = 1
        if kind != Kind.ROCK:
            grid.bodies.append(node.name)
            value = len(grid.bodies) + 1
        for shape_kind, data in node.shapes:
            for i in _covered(grid, node, shape_kind, data):
                current = grid.cells[i]
                # Rock over anything; a conditional body over free space; the first body kept.
                if current == 1 or (current > 1 and value > 1):
                    continue
                grid.cells[i] = value
    return grid


def _covered(grid: Grid, node: Node, shape_kind: str, data) -> list[int]:
    """The cells whose centre one of a node's shapes covers, in the level's frame."""
    if shape_kind == "poly":
        pts = [node.world(p) for p in data]
        lo = (min(p[0] for p in pts), min(p[1] for p in pts))
        hi = (max(p[0] for p in pts), max(p[1] for p in pts))
        return [i for i in grid.box(lo, hi) if _inside_poly(*grid.centre(i), pts)]
    (cx, cy), radius = node.world(data[0]), data[1]
    out = []
    for i in grid.box((cx - radius, cy - radius), (cx + radius, cy + radius)):
        x, y = grid.centre(i)
        if (x - cx) ** 2 + (y - cy) ** 2 <= radius * radius:
            out.append(i)
    return out


@dataclass
class Verdict:
    label: str
    status: str
    opened_by: dict[str, list[str]] = field(default_factory=dict)
    sealed_by: list[str] = field(default_factory=list)
    static_portals: bool = False

    def as_json(self) -> dict:
        out: dict = {"label": self.label, "status": self.status}
        if self.opened_by:
            out["opened_by"] = {k: sorted(v) for k, v in sorted(self.opened_by.items())}
        if self.sealed_by:
            out["sealed_by"] = sorted(self.sealed_by)
        if self.static_portals:
            out["static_portals"] = True
        return out


def _portal_links(nodes: list[Node], rules: Rules, grid: Grid) -> dict[int, list[int]]:
    """Cell -> the cells of the paired portal's trigger, for every static portal."""
    portals = {}
    for n in nodes:
        if rules.roles.get(n.entity) != "static_portal":
            continue
        size = n.vector("trigger_size") or (32.0, 32.0)
        cells = grid.box((n.pos[0] - size[0] / 2, n.pos[1] - size[1] / 2),
                         (n.pos[0] + size[0] / 2, n.pos[1] + size[1] / 2))
        portals[n.text("index")] = (n.text("destiny"), [c for c in cells if grid.cells[c] == 0])
    links: dict[int, list[int]] = {}
    for destiny, cells in portals.values():
        if destiny not in portals:
            continue
        for c in cells:
            links.setdefault(c, []).extend(portals[destiny][1])
    return links


def verdict(name: str, nodes: list[Node], rules: Rules, label: str = "") -> Verdict:
    by_role = {rules.roles.get(n.entity, ""): n for n in nodes}
    bound = next((n for n in nodes if n.entity == "max"), None)
    spawn = by_role.get("player_spawn")
    doors = [n for n in nodes if rules.roles.get(n.entity) == "exit_door"]
    if bound is None or spawn is None or not doors:
        raise ValueError(f"{name}: no max, no main_char or no door")
    grid = rasterise(nodes, rules, bound.pos)
    links = _portal_links(nodes, rules, grid)
    opened = openers(nodes, rules)
    cost = [0, None]
    for body in grid.bodies:
        cost.append(COST_OPENED if body in opened else COST_SEALED)

    goal = set()
    for door in doors:
        off = door.vector("trigger_offset") or (0.0, 0.0)
        size = door.vector("trigger_size") or (0.0, 0.0)
        cx, cy = door.world(off)
        half = (size[0] / 2 + DOOR_TOLERANCE_PX, size[1] / 2 + DOOR_TOLERANCE_PX)
        goal.update(grid.box((cx - half[0], cy - half[1]), (cx + half[0], cy + half[1])))

    start = grid.index(*spawn.pos)
    if start is None or grid.cells[start] == 1:
        raise ValueError(f"{name}: the spawn is outside the bound or inside rock")

    # Dijkstra over cells, weights 0 / COST_OPENED / COST_SEALED on entering a conditional body.
    dist = {start: 0}
    parent: dict[int, int] = {start: -1}
    jumped: set[int] = set()
    heap = [(0, start)]
    found = None
    while heap:
        d, c = heapq.heappop(heap)
        if d != dist.get(c):
            continue
        if c in goal:
            found = c
            break
        cx, cy = c % grid.w, c // grid.w
        here = grid.cells[c]
        steps = []
        if cx > 0:
            steps.append(c - 1)
        if cx < grid.w - 1:
            steps.append(c + 1)
        if cy > 0:
            steps.append(c - grid.w)
        if cy < grid.h - 1:
            steps.append(c + grid.w)
        for n in steps:
            value = grid.cells[n]
            if value == 1:
                continue
            nd = d + (cost[value] if value > 1 and value != here else 0)
            if nd < dist.get(n, math.inf):
                dist[n] = nd
                parent[n] = c
                heapq.heappush(heap, (nd, n))
        for n in links.get(c, ()):
            if d < dist.get(n, math.inf):
                dist[n] = d
                parent[n] = c
                jumped.add(n)
                heapq.heappush(heap, (d, n))

    if found is None:
        return Verdict(label, Status.UNREACHED)
    crossed: list[str] = []
    through_portal = False
    c = found
    while c != -1:
        value = grid.cells[c]
        if value > 1 and grid.bodies[value - 2] not in crossed:
            crossed.append(grid.bodies[value - 2])
        if c in jumped and parent[c] in links and c in links[parent[c]]:
            through_portal = True
        c = parent[c]
    out = Verdict(label, Status.REACHABLE, static_portals=through_portal)
    for body in crossed:
        if body in opened:
            out.opened_by.setdefault(opened[body], []).append(body)
        else:
            out.sealed_by.append(body)
    if out.sealed_by:
        out.status = Status.SEALED
    elif out.opened_by:
        out.status = Status.OPENED
    return out


# ---- the command ---------------------------------------------------------------------------


def level_labels(chapters: Path) -> dict[str, str]:
    """Level name -> "W-LL", in chapters.json's order."""
    labels = {}
    for world in _read(chapters)["worlds"]:
        for level in world["levels"]:
            labels[level["name"]] = f"{world['index'] + 1}-{level['index'] + 1:02d}"
    return labels


def run(levels_dir: Path, chapters: Path, names: list[str] | None = None,
        root: Path = ROOT) -> dict[str, dict]:
    rules = load_rules(root)
    labels = level_labels(chapters)
    out = {}
    for name in names or list(labels):
        nodes = parse_tscn((levels_dir / f"{name}.tscn").read_text(encoding="utf-8"))
        out[name] = verdict(name, nodes, rules, labels.get(name, "")).as_json()
    return out


def pinned_document(levels: dict[str, dict]) -> dict:
    counts: dict[str, int] = {}
    for entry in levels.values():
        counts[entry["status"]] = counts.get(entry["status"], 0) + 1
    return {
        "_about": [
            "Whether each level's door can be reached in the port: tools/parity/reachability.py",
            "over the converted levels (out/levels), with the port's rules read from data/ and",
            "game/data/. Derived by the tool, never hand-edited: rerun it with --check after any",
            "change to the converter, the levels or a mechanism that opens a wall, and with",
            "--write only in a step that says why a verdict moved. The node names are the",
            "converter's; nothing of the original's files is here.",
        ],
        "_breakers": BREAKERS,
        "counts": dict(sorted(counts.items())),
        "levels": levels,
    }


def compare(fresh: dict[str, dict], pinned: dict[str, dict]) -> list[str]:
    diffs = []
    for name in sorted(set(fresh) | set(pinned)):
        a, b = pinned.get(name), fresh.get(name)
        if a != b:
            diffs.append(f"{name}: pinned {json.dumps(a, sort_keys=True)}\n"
                         f"{' ' * len(name)}  now    {json.dumps(b, sort_keys=True)}")
    return diffs


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("levels", nargs="*", help="level names (default: all, as chapters.json)")
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--check", action="store_true", help="compare with the pinned JSON")
    mode.add_argument("--write", action="store_true", help="rewrite the pinned JSON")
    parser.add_argument("--levels-dir", type=Path, default=ROOT / "out" / "levels")
    parser.add_argument("--chapters", type=Path, default=ROOT / "out" / "data" / "chapters.json")
    parser.add_argument("--pinned", type=Path, default=PINNED)
    args = parser.parse_args(argv)
    if not args.levels_dir.is_dir() or not args.chapters.is_file():
        # 77, as the port's suites skip without the converted levels: ctest reports it SKIPPED.
        print(f"reachability: SKIPPED - no converted levels at {args.levels_dir}"
              f" or no {args.chapters}")
        return EXIT_SKIPPED
    fresh = run(args.levels_dir, args.chapters, args.levels or None)
    if args.write:
        text = json.dumps(pinned_document(fresh), indent=2) + "\n"
        args.pinned.write_text(text, encoding="utf-8", newline="\n")
        print(f"reachability: wrote {len(fresh)} levels to {args.pinned}")
        return 0
    if args.check:
        pinned = json.loads(args.pinned.read_text(encoding="utf-8"))["levels"]
        if args.levels:
            pinned = {k: v for k, v in pinned.items() if k in fresh}
        diffs = compare(fresh, pinned)
        counts = pinned_document(fresh)["counts"]
        for line in diffs:
            print(line)
        print(f"reachability: {len(fresh)} levels, {counts}; "
              f"{len(diffs)} differ from {args.pinned.name}")
        return 1 if diffs else 0
    for name, entry in fresh.items():
        print(f"{name} {json.dumps(entry, sort_keys=True)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
