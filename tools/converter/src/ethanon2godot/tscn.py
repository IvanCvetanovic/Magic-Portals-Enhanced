"""Writer for Godot 4 ``.tscn`` scene files.

Godot's text scene format is plain text and stable enough to emit directly — no Godot
install is needed to run the converter, which keeps it CI-friendly and fast.

THE MAPPING
-----------
Every decision below is measured against chapter 1 (all 32 world-0 scenes, 934
instances), not assumed. See the 2026-08-04 block in ``docs/ethanon-formats.md``.

*Position.* ``screenPos = (x, y) + zAxisDirection * z``. Shipped scenes all carry
``zAxisDirection = (0, 0)``, so this is just ``(x, y)`` and ``z`` is pure depth.
Ethanon is origin-top-left, +x right, **+y down** — the same convention as Godot 2D, so
**no Y sign flip** (``docs/ethanon-formats.md`` §9.4).

*Origin.* ``absoluteOrigin = typeAnchor(size) + pivotAdjust * scale``, and the quad is
drawn at ``screenPos - absoluteOrigin`` (``ETHEntity.cpp:270-297``). Every chapter-1
entity is ``type=0`` ⇒ ``EO_CENTER`` ⇒ ``typeAnchor = size/2``, and no scene carries a
``<Scale>``, so ``scale = 1``. Godot's ``Sprite2D`` is centred by default, which means
the type anchor cancels exactly and all that survives is ``offset = -pivotAdjust``.
**That is why no texture dimensions are needed.** A ``type=2`` (``ET_VERTICAL``,
anchor at ``size.y``) entity would need the sprite height and is rejected rather than
silently misplaced — none occur in chapter 1.

*Rotation.* The Android/GLES2 path rotates by ``-angle``
(``GLES2Sprite.cpp:207,270``: ``RotateZ(DegreeToRadian(-angle))``), matching
``ETHSpriteEntity.cpp:703``. Emitted as ``-radians(angle)``. Desktop GL uses ``+angle``
— a real, uncompensated backend inversion (§11.5); Android is the target, so the
negated form is correct here. **47 chapter-1 instances have a non-zero angle; this
sign is the one part of the mapping still worth confirming against video.**

*Blend.* ``blendMode`` 1 = ``AM_ADD`` ⇒ ``CanvasItemMaterial.blend_mode = 1``. It is
the portals' additive glow, and the doc previously claimed it never occurs on entities.

*Collision.* ``shape`` selects the ``b2Shape`` builder
(``ETHPhysicsSimulator.cpp:85-98``) and ``<Collision>`` supplies the dimensions;
polygon vertices are normalised −1..1 and map as ``(size * 0.5) * v + offset``
(``:40-53``), which is exactly a ``CollisionPolygon2D``.
"""

from __future__ import annotations

import math
from dataclasses import dataclass, field
from pathlib import Path

from .entity import BS_BOX, BS_CIRCLE, BS_COMPOUND, BS_POLYGON
from .imagesize import read_size
from .scene import Instance, Scene

# Godot 4 text scene format version. `format=3` is what Godot 4.x writes.
TSCN_FORMAT = 3

# CanvasItemMaterial.BlendMode — 0=Mix, 1=Add, 2=Sub, 3=Mul, 4=PremultAlpha.
_GODOT_BLEND_ADD = 1
_GODOT_BLEND_MUL = 3
_ETH_BLEND_TO_GODOT = {1: _GODOT_BLEND_ADD, 4: _GODOT_BLEND_MUL}


def quote(value: str) -> str:
    """Escape a Python string as a Godot ``.tscn`` string literal.

    Godot's text parser uses C-style escapes inside double quotes. Backslash must be
    escaped first, or escaping the quote character would double-escape it.
    """
    escaped = (
        value.replace("\\", "\\\\")
        .replace('"', '\\"')
        .replace("\n", "\\n")
        .replace("\r", "\\r")
        .replace("\t", "\\t")
    )
    return f'"{escaped}"'


def _num(value: float) -> str:
    """Format a float the way Godot writes them — no trailing ``.0`` noise.

    Ethanon stores everything as ``"384.000000"``; emitting that verbatim produces
    diffs full of meaningless precision.
    """
    if value == int(value) and abs(value) < 1e15:
        return str(int(value))
    return repr(round(value, 4))


def _vec2(x: float, y: float) -> str:
    return f"Vector2({_num(x)}, {_num(y)})"


def _node_name(raw: str, node_id: int) -> str:
    """A unique, Godot-legal node name.

    Godot forbids ``. : @ / " %`` in node names, and ``EntityName`` routinely ends in
    ``.ent``. The instance id is appended because names repeat heavily — ``crystal.ent``
    alone is placed 106 times in chapter 1 — and Godot silently renames duplicate
    siblings, which would break any later lookup by name.
    """
    cleaned = "".join("_" if c in '.:@/"%' else c for c in raw).strip("_")
    return f"{cleaned or 'entity'}_{node_id}"


@dataclass
class ConversionReport:
    """What the converter did and, more importantly, what it refused to do."""

    nodes: int = 0
    sprites: int = 0
    colliders: int = 0
    textures: set[str] = field(default_factory=set)
    skipped: list[str] = field(default_factory=list)


def _collider_lines(inst: Instance, parent: str, sub_ids: list[str]) -> tuple[list[str], str]:
    """Emit the physics body + shape for one instance.

    Returns ``(lines, kind)``. ``kind`` is ``""`` when the instance has no collision.
    """
    ent = inst.entity
    if ent is None or not ent.is_collidable or ent.collision is None:
        return [], ""

    col = ent.collision
    # A sensor participates in overlap tests but not in collision response — Area2D is
    # Godot's equivalent. Otherwise static bodies are the overwhelming majority in
    # chapter 1 (level geometry); anything non-static becomes a RigidBody2D so its
    # density/friction/restitution actually mean something.
    if ent.sensor:
        body_type = "Area2D"
    elif ent.static:
        body_type = "StaticBody2D"
    else:
        body_type = "RigidBody2D"

    body = f"{parent}/Body"
    lines = [f'[node name="Body" type="{body_type}" parent="{quote(parent)[1:-1]}"]', ""]

    if ent.shape == BS_POLYGON and col.polygon:
        # (size * 0.5) * v + offset — ETHPhysicsSimulator.cpp:44-47.
        pts = [
            (col.size[0] * 0.5 * vx + col.offset[0], col.size[1] * 0.5 * vy + col.offset[1])
            for vx, vy in col.polygon
        ]
        packed = ", ".join(f"{_num(x)}, {_num(y)}" for x, y in pts)
        lines += [
            f'[node name="Shape" type="CollisionPolygon2D" parent="{body}"]',
            f"polygon = PackedVector2Array({packed})",
            "",
        ]
        return lines, "polygon"

    if ent.shape == BS_COMPOUND and col.compound:
        # One CollisionShape2D per Box2D fixture. Sizes/offsets were already scaled out of
        # the normalised -1..1 ENML in entity._parse_compound.
        for index, (kind, offset, extent) in enumerate(col.compound):
            sub = f"Compound{index}_{inst.id}"
            if kind == "circle":
                sub_ids.append(
                    f'[sub_resource type="CircleShape2D" id="{sub}"]\n'
                    f"radius = {_num(extent[0])}\n"
                )
            else:
                sub_ids.append(
                    f'[sub_resource type="RectangleShape2D" id="{sub}"]\n'
                    f"size = {_vec2(*extent)}\n"
                )
            lines += [
                f'[node name="Shape{index}" type="CollisionShape2D" parent="{body}"]',
                f"shape = SubResource({quote(sub)})",
            ]
            total = (offset[0] + col.offset[0], offset[1] + col.offset[1])
            if total != (0.0, 0.0):
                lines.append(f"position = {_vec2(*total)}")
            lines.append("")
        return lines, "compound"

    if ent.shape == BS_CIRCLE:
        # GetCircleShape uses (size.x + size.y) / 4 as the radius — i.e. the mean of the
        # two half-extents, not an inscribed or circumscribed circle.
        radius = (col.size[0] + col.size[1]) / 4.0
        sub = f"CircleShape2D_{inst.id}"
        sub_ids.append(f'[sub_resource type="CircleShape2D" id="{sub}"]\nradius = {_num(radius)}\n')
    elif ent.shape == BS_BOX:
        sub = f"RectangleShape2D_{inst.id}"
        sub_ids.append(
            f'[sub_resource type="RectangleShape2D" id="{sub}"]\n'
            f"size = {_vec2(*col.size)}\n"
        )
    else:
        return [], ""

    lines += [
        f'[node name="Shape" type="CollisionShape2D" parent="{body}"]',
        f"shape = SubResource({quote(sub)})",
    ]
    if col.offset != (0.0, 0.0):
        lines.append(f"position = {_vec2(*col.offset)}")
    lines.append("")
    return lines, "shape"


def _vertical_offset(ent, sprite_dir: Path | None) -> tuple[float, float] | None:
    """Sprite offset for a ``type=2`` (``ET_VERTICAL``) entity, or ``None`` if unknowable.

    ``ET_VERTICAL`` anchors at ``(size.x/2, size.y)`` — bottom-centre — instead of the
    centre (``ETHEntity.cpp:270-297``). Godot's ``Sprite2D`` is centre-anchored, so the
    difference is half the sprite height, and that is the ONLY case in the whole converter
    that needs real image dimensions.

        quadCenter = screenPos - absoluteOrigin + displaySize/2
                   = screenPos - (w/2, h) - pivot + (w/2, h/2)
                   = screenPos - (0, h/2) - pivot
    """
    if sprite_dir is None or not ent.sprite:
        return None
    size = read_size(sprite_dir / ent.sprite)
    if size is None:
        return None
    return (-ent.pivot_adjust[0], -(size[1] * 0.5) - ent.pivot_adjust[1])


def write_scene(
    scene: Scene,
    *,
    texture_prefix: str = "res://assets/entities",
    root_name: str = "",
    sprite_dir: Path | None = None,
) -> tuple[str, ConversionReport]:
    """Render a parsed :class:`Scene` to ``.tscn`` source.

    ``texture_prefix`` is where the converted sprites will live inside the Godot
    project. Sprite references in ``.ent``/``.esc`` are bare filenames resolved against
    the project's entity directory — all 38 distinct chapter-1 sprites resolve under
    ``assets/entities/``. Note this flat rule does NOT hold for ``assets/sprites/``,
    which the game addresses by full relative path (§8.1).
    """
    report = ConversionReport()
    zdir = scene.properties.z_axis_direction

    ext: dict[str, str] = {}
    subs: list[str] = []
    body: list[str] = []

    root = root_name or _node_name(scene.name or "Level", 0).rstrip("_0") or "Level"
    body.append(f'[node name="{root}" type="Node2D"]')
    body.append("")

    for inst in scene.instances:
        ent = inst.entity
        if ent is None:
            # Branch A (a <FileName> lookup into the entity cache). Never occurs in
            # chapter 1; recorded rather than guessed at.
            report.skipped.append(f"{inst.entity_name}#{inst.id}: unresolved entity definition")
            continue
        sprite_offset: tuple[float, float] = (-ent.pivot_adjust[0], -ent.pivot_adjust[1])
        if ent.type == 2:
            # ET_VERTICAL anchors bottom-centre, so it needs the sprite height. Refuse
            # rather than misplace by half a sprite if the image cannot be measured.
            vertical = _vertical_offset(ent, sprite_dir)
            if vertical is None:
                report.skipped.append(
                    f"{inst.entity_name}#{inst.id}: type=2 (ET_VERTICAL), sprite size unknown"
                )
                continue
            sprite_offset = vertical
        if ent.shape == BS_COMPOUND and (ent.collision is None or not ent.collision.compound):
            report.skipped.append(
                f"{inst.entity_name}#{inst.id}: shape=4 (BS_COMPOUND) with no <Compound> data"
            )

        name = _node_name(inst.entity_name, inst.id)
        sx, sy = inst.screen_pos(zdir)

        body.append(f'[node name="{name}" type="Node2D" parent="."]')
        body.append(f"position = {_vec2(sx, sy)}")
        # The original's EntityName is the ONLY thing identifying what a node is — the
        # converter deliberately does not decide roles, because that is a game-design
        # mapping and belongs in committed JSON (game/data/entity_roles.json), not in
        # the data pipeline. Emit the raw name and let the game interpret it.
        body.append(f"metadata/entity_name = {quote(inst.entity_name)}")
        # Entities with shape=BS_NONE still carry a <Collision> box, and it is not dead
        # data: the original does its own overlap tests against it. door.ent, crystal.ent
        # and portal_static are all shape=0 with a real box. Dropping it (because it
        # produces no physics fixture) would throw away every trigger region in the game.
        if ent.collision is not None and ent.collision.joint:
            # A revolute joint. Emitted as metadata rather than as a Godot joint node
            # because the pivot has to be attached at runtime, once the body exists.
            for key in sorted(ent.collision.joint):
                body.append(f"metadata/joint_{key} = {_num(ent.collision.joint[key])}")
        if ent.collision is not None and not ent.is_collidable:
            body.append(f"metadata/trigger_size = {_vec2(*ent.collision.size)}")
            if ent.collision.offset != (0.0, 0.0):
                body.append(f"metadata/trigger_offset = {_vec2(*ent.collision.offset)}")
        for key in sorted(ent.custom_data):
            # Per-instance designer parameters: teleportable, max_portals, portal
            # index/destiny/color, platform speed/stride… Values stay STRINGS because
            # that is how the original stores them and the <Type> tag is advisory —
            # the game parses with the type it expects.
            body.append(f"metadata/{key} = {quote(ent.custom_data[key])}")
        if inst.angle:
            body.append(f"rotation = {_num(-math.radians(inst.angle))}")
        if inst.position[2]:
            # z is depth-only here (zAxisDirection is (0,0)); z_index preserves the
            # draw order without displacing anything. Chapter-1 z spans -100..30, well
            # inside Godot's -4096..4095.
            body.append("z_index = " + _num(round(inst.position[2])))
        body.append("")
        report.nodes += 1

        if ent.sprite:
            res = f"{texture_prefix}/{ent.sprite}"
            if res not in ext:
                ext[res] = f"tex_{len(ext)}"
            report.textures.add(ent.sprite)
            body.append(f'[node name="Sprite" type="Sprite2D" parent="{name}"]')
            body.append(f"texture = ExtResource({quote(ext[res])})")
            if sprite_offset != (0.0, 0.0):
                # Sprite2D is centred, so for the usual EO_CENTER type the anchor cancels
                # and only the pivot survives (negated — Ethanon SUBTRACTS the origin).
                # ET_VERTICAL additionally carries half the sprite height.
                body.append(f"offset = {_vec2(*sprite_offset)}")
            godot_blend = _ETH_BLEND_TO_GODOT.get(ent.blend_mode)
            if godot_blend is not None:
                mat = f"CanvasItemMaterial_blend{godot_blend}"
                decl = (
                    f'[sub_resource type="CanvasItemMaterial" id="{mat}"]\n'
                    f"blend_mode = {godot_blend}\n"
                )
                if decl not in subs:
                    subs.append(decl)
                body.append(f"material = SubResource({quote(mat)})")
            body.append("")
            report.sprites += 1

        lines, kind = _collider_lines(inst, name, subs)
        if kind:
            body.extend(lines)
            report.colliders += 1

    # load_steps = ext_resources + sub_resources + 1 (the scene itself).
    header = [f"[gd_scene load_steps={len(ext) + len(subs) + 1} format={TSCN_FORMAT}]", ""]
    for res, rid in ext.items():
        header.append(f'[ext_resource type="Texture2D" path={quote(res)} id={quote(rid)}]')
    if ext:
        header.append("")
    header.extend(subs)

    return "\n".join(header + body).rstrip() + "\n", report
