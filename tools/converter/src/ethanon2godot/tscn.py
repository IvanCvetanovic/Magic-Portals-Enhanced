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

*Lighting.* Carried as ``metadata/eth_*`` keys whose values are ALL quoted strings, so
the port's "no bare numbers outside joints" pin holds and the keys are inert in Godot.
Scene keys go on the root node; entity keys go on nodes that draw a sprite or own a
light (``eth_z`` and ``eth_emissive`` on every node), only when the value differs from
the engine default, and are appended LAST in the node block so every pre-existing line
keeps its place. The mapping and its citations are
the 2026-09-14 "lighting keys" subsection of ``docs/ethanon-formats.md``.

*Particles.* A placement's own ``<Particles>`` goes to a sidecar ``.ent`` (``particles.py``)
that ``metadata/eth_particles`` names, the last ``eth_*`` line of a node that draws a sprite.
The 2026-09-27 "particle sidecars" subsection of ``docs/ethanon-formats.md``.
"""

from __future__ import annotations

import math
from collections import Counter
from dataclasses import dataclass, field
from pathlib import Path

from .entity import BS_BOX, BS_CIRCLE, BS_COMPOUND, BS_POLYGON, Entity
from .imagesize import read_size
from .particles import SIDECAR_PREFIX, ParticleBlocks
from .scene import Instance, Scene

# Godot 4 text scene format version. `format=3` is what Godot 4.x writes.
TSCN_FORMAT = 3

# CanvasItemMaterial.BlendMode — 0=Mix, 1=Add, 2=Sub, 3=Mul, 4=PremultAlpha.
_GODOT_BLEND_ADD = 1
_GODOT_BLEND_MUL = 3
_ETH_BLEND_TO_GODOT = {1: _GODOT_BLEND_ADD, 4: _GODOT_BLEND_MUL}

# Where copied lightmaps live inside the project, as `<prefix>/<scene>/add<id>.png`. Fixed
# rather than derived from the output directory, so a staging run and a real run emit the
# same text.
LIGHTMAP_PREFIX = "res://assets/lightmaps"

# Every lighting key, in emission order. The scene keys go on the root node only.
ETH_SCENE_KEYS = ("eth_ambient", "eth_light_intensity")
ETH_ENTITY_KEYS = (
    "eth_z",
    "eth_static",
    "eth_apply_light",
    "eth_emissive",
    "eth_color",
    "eth_normal",
    "eth_lightmap",
    "eth_light_offset",
    "eth_light_range",
    "eth_light_color",
    "eth_halo",
    "eth_halo_offset",
    "eth_halo_size",
    "eth_halo_brightness",
    # Not a lighting input: the node's own <Particles>, as the sidecar file that holds them
    # (particles.py). Last, and only on a node that draws a sprite (write_scene).
    "eth_particles",
)
# Written on EVERY node whose value is non-default, not only on nodes that draw a sprite
# or own a light. eth_z: spriteless nodes are where the port's own actors take their
# lighting height from (design_port.md §5.4: the player's z is the `main_char` node's, and
# level8a's main_char sits at z 2). eth_emissive: the design's totals and the port's pins
# count it over all instances (2,140, six of them on spriteless triggers). The gate stays
# on eth_static / eth_apply_light, where it trims thousands of collision bodies and markers,
# and on eth_normal, which shades nothing without a sprite to draw (and no shipped
# spriteless instance names one).
_UNGATED_ENTITY_KEYS = frozenset({"eth_z", "eth_emissive"})
# A halo's size default is the unscaled haloSize (ETHLight.cpp:33), which is what a reader
# assumes when the key is absent.
_HALO_SIZE_DEFAULT = (64.0, 64.0)


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


def _nums(*values: float) -> str:
    """Space-separated numbers for a string-valued lighting key: ``"0.35 0.3 0.35"``."""
    return " ".join(_num(v) for v in values)


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
    # Nodes carrying each emitted `eth_*` key (the scene keys count once per level).
    lighting: Counter[str] = field(default_factory=Counter)
    # The same entity keys counted over EVERY converted instance, ignoring the "draws a
    # sprite or owns a light" gate, so a total can be reconciled with an inventory taken
    # over all placements. For eth_lightmap: add<id>.png files matching any instance.
    lighting_ungated: Counter[str] = field(default_factory=Counter)
    # Emitted lights whose owner is static (a light's `static` is forced to its owner's).
    static_lights: int = 0
    # (source add<id>.png, destination relative to the lightmap root), per emitted key.
    lightmaps: list[tuple[Path, str]] = field(default_factory=list)
    normals: set[str] = field(default_factory=set)
    halos: set[str] = field(default_factory=set)
    # The distinct <Particles> blocks the eth_particles keys name, for --copy-particles.
    particle_blocks: ParticleBlocks = field(default_factory=ParticleBlocks)
    # Systems the engine would drop (particles <= 0, or past the second), over all instances.
    particle_systems_dropped: int = 0


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


def _entity_lighting(
    inst: Instance, ent: Entity, lightmap: str, texture_prefix: str
) -> list[tuple[str, str]]:
    """The ``eth_*`` entity keys for one instance, defaults omitted, values unquoted.

    Scaling follows the engine per consumer: the light's position and range are
    multiplied by ``scale.y`` only (``BuildChildLight``,
    ``ETHEntityRenderingManager.cpp:180-181``); the halo is centred on the UNSCALED
    ``light.pos`` (``ETHRenderEntity.cpp:378``) and sized ``haloSize * scale`` per axis
    (``:381-385``).
    """
    out: list[tuple[str, str]] = []
    z = inst.position[2]
    if z:
        # Unrounded: lighting needs the real height; z_index keeps doing the ordering.
        out.append(("eth_z", _num(z)))
    if ent.static:
        out.append(("eth_static", "1"))
    if ent.apply_light:
        out.append(("eth_apply_light", "1"))
    if ent.emissive[:3] != (0.0, 0.0, 0.0):
        out.append(("eth_emissive", _nums(*ent.emissive[:3])))
    if inst.color != (1.0, 1.0, 1.0, 1.0):
        out.append(("eth_color", _nums(*inst.color)))
    if ent.normal:
        out.append(("eth_normal", f"{texture_prefix}/normalmaps/{ent.normal}"))
    if lightmap:
        out.append(("eth_lightmap", lightmap))

    light = ent.light
    if light is not None:
        sx, sy = ent.scale
        offset = (light.position[0] * sy, light.position[1] * sy, light.position[2] * sy)
        if offset != (0.0, 0.0, 0.0):
            out.append(("eth_light_offset", _nums(*offset)))
        # Written even at the engine default (256): it is how a reader knows the node owns
        # a light at all, and fire_agent really does use 256.
        out.append(("eth_light_range", _num(light.range * sy)))
        if light.color != (1.0, 1.0, 1.0):
            out.append(("eth_light_color", _nums(*light.color)))
        # A light without <HaloBitmap> gets no halo piece (ETHEntityRenderingManager.cpp:78),
        # so its halo numbers have no consumer and are not written.
        if light.halo_bitmap:
            out.append(("eth_halo", f"{texture_prefix}/{light.halo_bitmap}"))
            halo_offset = (light.position[0], light.position[1])
            if halo_offset != (0.0, 0.0):
                out.append(("eth_halo_offset", _nums(*halo_offset)))
            halo_size = (light.halo_size * sx, light.halo_size * sy)
            if halo_size != _HALO_SIZE_DEFAULT:
                out.append(("eth_halo_size", _nums(*halo_size)))
            if light.halo_brightness != 1.0:
                out.append(("eth_halo_brightness", _num(light.halo_brightness)))
    return out


def write_scene(
    scene: Scene,
    *,
    texture_prefix: str = "res://assets/entities",
    root_name: str = "",
    sprite_dir: Path | None = None,
    lightmap_dir: Path | None = None,
    lightmap_prefix: str = LIGHTMAP_PREFIX,
    particles_prefix: str = SIDECAR_PREFIX,
) -> tuple[str, ConversionReport]:
    """Render a parsed :class:`Scene` to ``.tscn`` source.

    ``texture_prefix`` is where the converted sprites will live inside the Godot
    project. Sprite references in ``.ent``/``.esc`` are bare filenames resolved against
    the project's entity directory — all 38 distinct chapter-1 sprites resolve under
    ``assets/entities/``. Note this flat rule does NOT hold for ``assets/sprites/``,
    which the game addresses by full relative path (§8.1). Normal maps and halo bitmaps
    resolve under the same prefix: ``normalmaps/`` and the entity directory itself
    (``ETHDirectories.cpp:25-33``).

    ``lightmap_dir`` is the scene's ``<scene>-esc/`` directory
    (:func:`scene.lightmap_directory`); ``None`` emits no lightmap key at all.
    ``particles_prefix`` is where the particle sidecars live, like ``lightmap_prefix``; the
    blocks themselves are collected in ``report.particle_blocks`` for ``--copy-particles``.
    """
    report = ConversionReport()
    zdir = scene.properties.z_axis_direction

    ext: dict[str, str] = {}
    subs: list[str] = []
    body: list[str] = []

    root = root_name or _node_name(scene.name or "Level", 0).rstrip("_0") or "Level"
    body.append(f'[node name="{root}" type="Node2D"]')
    if scene.properties.declared:
        # Always written, engine defaults included: a reader must be able to tell "the file
        # said (1,1,1) / 2.0" apart from "the file had no <SceneProperties>".
        props = scene.properties
        body.append(f"metadata/eth_ambient = {quote(_nums(*props.ambient))}")
        body.append(f"metadata/eth_light_intensity = {quote(_num(props.light_intensity))}")
        report.lighting.update(ETH_SCENE_KEYS)
    body.append("")
    lightmap_scene = scene.name or root

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
        # mapping and belongs in committed JSON (data/entity_roles.json), not in
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

        # The lightmap is a file, not a field. It is attached only where the engine can bake
        # one — static, applyLight and a sprite (ETHLightmapGen.cpp:42) — and a file found
        # anywhere else is reported rather than used or silently dropped.
        lightmap = ""
        if lightmap_dir is not None:
            png = lightmap_dir / f"add{inst.id}.png"
            if png.is_file():
                report.lighting_ungated["eth_lightmap"] += 1
                if ent.static and ent.apply_light and ent.sprite:
                    rel = f"{lightmap_scene}/add{inst.id}.png"
                    lightmap = f"{lightmap_prefix}/{rel}"
                    report.lightmaps.append((png, rel))
                else:
                    report.skipped.append(
                        f"{inst.entity_name}#{inst.id}: add{inst.id}.png exists but the "
                        "instance is not static+applyLight+sprite; lightmap not attached"
                    )
            elif (lightmap_dir / f"add{inst.id}.bmp").is_file():
                report.skipped.append(
                    f"{inst.entity_name}#{inst.id}: only a .bmp lightmap exists; not handled"
                )
        lighting = _entity_lighting(inst, ent, lightmap, texture_prefix)
        report.lighting_ungated.update(k for k, _ in lighting if k != "eth_lightmap")
        draws = bool(ent.sprite) or ent.light is not None
        emitted = [(k, v) for k, v in lighting if draws or k in _UNGATED_ENTITY_KEYS]
        for key, value in emitted:
            body.append(f"metadata/{key} = {quote(value)}")
        report.lighting.update(k for k, _ in emitted)
        report.particle_systems_dropped += ent.particle_systems_dropped
        if ent.particles_xml:
            # After every lighting line, so the diff against a level converted without it
            # is one inserted line per carrier. Only where a Sprite2D is written: the port
            # draws a placement's systems from its sprite (buildEmitters walks the drawn
            # sprites), and every one of the 840 shipped carriers draws one. A carrier
            # without a sprite is reported, never keyed where nothing would read it.
            report.lighting_ungated["eth_particles"] += 1
            if ent.sprite:
                sidecar = report.particle_blocks.add(ent.particles_xml)
                body.append(f"metadata/eth_particles = {quote(f'{particles_prefix}/{sidecar}')}")
                report.lighting["eth_particles"] += 1
            else:
                report.skipped.append(
                    f"{inst.entity_name}#{inst.id}: {ent.particle_systems} particle system(s) on "
                    "a node that draws no sprite; not carried"
                )
        if draws:
            if ent.normal:
                report.normals.add(ent.normal)
            if ent.light is not None:
                report.static_lights += 1 if ent.static else 0
                if ent.light.halo_bitmap:
                    report.halos.add(ent.light.halo_bitmap)
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
