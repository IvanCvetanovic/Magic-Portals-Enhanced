"""Parser for Ethanon entity definitions.

An entity definition describes an entity *type*: sprite, entity type/render mode,
collision shape, lighting, particle systems, and a bag of user-defined "custom data".

It appears in two places, and **the grammar is identical in both**:

* as a standalone ``.ent`` file, referenced by name from a scene — "branch A"; or
* inlined directly inside a ``.esc`` as a nested ``<Entity>`` element — "branch B".

The discriminator is a ``<FileName>`` child (``ETHEntityProperties.cpp:202-224``):
present ⇒ look the name up in the entity cache; absent ⇒ read this element inline.

**Chapter 1 is 100% branch B.** ``<FileName>`` occurs zero times across all 32 world-0
scenes, so converting chapter 1 needs no ``.ent`` file at all. See the 2026-08-04 block
in ``docs/ethanon-formats.md``. Branch A is still supported here because later chapters
were not measured and the grammar is shared anyway.

Reference: ``engine/Entity/ETHEntityProperties.cpp`` (the deserializer) and
``ETHCustomDataManager.cpp``. Spec: ``docs/ethanon-formats.md`` §5.
"""

from __future__ import annotations

import xml.etree.ElementTree as ET
from dataclasses import dataclass, field
from pathlib import Path

from . import enml
from .textio import read_ethanon_xml

# ETHEntityProperties.h:92-99. Note BS_NONE is 0 — "no collision", not "box".
BS_NONE = 0
BS_BOX = 1
BS_CIRCLE = 2
BS_POLYGON = 3
BS_COMPOUND = 4

# ETHEntityProperties::Reset (:167-181) and ETHEntityMaterial::Reset (:85-106).
# Only the defaults the converter actually consumes are listed; adding more means
# citing more.
_DEFAULT_FRICTION = 1.0
_DEFAULT_DENSITY = 1.0
_DEFAULT_RESTITUTION = 0.0
_DEFAULT_GRAVITY_SCALE = 1.0


def _f(element: ET.Element | None, attr: str, default: float = 0.0) -> float:
    """Read a float attribute, falling back to ``default`` when absent or malformed.

    Malformed values fall back rather than raise because that is what the engine does:
    ``QueryFloatAttribute`` leaves the out-parameter untouched on TIXML_WRONG_TYPE, so
    the field keeps its ``Reset()`` default.
    """
    if element is None:
        return default
    raw = element.get(attr)
    if raw is None:
        return default
    try:
        return float(raw)
    except ValueError:
        return default


def _i(element: ET.Element | None, attr: str, default: int = 0) -> int:
    """Read an int attribute. Same fallback rule as :func:`_f`.

    Booleans go through here too: ``ReadBooleanPropertyFromXmlElement`` (:594-604) uses
    ``QueryIntAttribute``, so ``"true"``/``"false"`` would NOT parse — they silently
    fall back to the default. Storing ints keeps that visible instead of hiding it
    behind a Python ``bool``.
    """
    if element is None:
        return default
    raw = element.get(attr)
    if raw is None:
        return default
    try:
        return int(raw)
    except ValueError:
        return default


def _vec(element: ET.Element | None, tag: str) -> tuple[float, float] | None:
    """Read a ``<tag x=... y=.../>`` child as a 2-vector, or ``None`` if absent."""
    if element is None:
        return None
    child = element.find(tag)
    if child is None:
        return None
    return (_f(child, "x"), _f(child, "y"))


@dataclass
class Collision:
    """A ``<Collision>`` block: an axis-aligned box, plus optional polygon outline."""

    offset: tuple[float, float] = (0.0, 0.0)
    size: tuple[float, float] = (0.0, 0.0)
    # Normalised -1..1 outline, present iff shape == BS_POLYGON. Scaled to world space
    # as (size * 0.5) * vertex + offset — ETHPhysicsSimulator.cpp:40-53.
    polygon: list[tuple[float, float]] = field(default_factory=list)
    # One entry per Box2D fixture, present iff shape == BS_COMPOUND. Each is
    # (kind, offset, size_or_radius) already scaled into world units. Ordered
    # ALPHABETICALLY by ENML entity name, because ETHCompoundShape copies out of a
    # std::map — fixture index order follows the map, not the file.
    compound: list[tuple[str, tuple[float, float], tuple[float, float]]] = field(
        default_factory=list
    )
    # A revolute joint, present iff the entity ships <Joints>. Keys are the ENML ones,
    # values already converted: anchor in world units, angles in radians.
    joint: dict[str, float] = field(default_factory=dict)


@dataclass
class Entity:
    """An Ethanon entity definition, from a ``.ent`` file or an inline block."""

    name: str = ""
    type: int = 0
    shape: int = BS_NONE
    static: int = 0
    sensor: int = 0
    blend_mode: int = 0
    friction: float = _DEFAULT_FRICTION
    density: float = _DEFAULT_DENSITY
    restitution: float = _DEFAULT_RESTITUTION
    gravity_scale: float = _DEFAULT_GRAVITY_SCALE
    sprite: str = ""
    # Stored pre-scale; ComputeAbsoluteOrigin multiplies by scale (ETHEntity.cpp:270-297).
    pivot_adjust: tuple[float, float] = (0.0, 0.0)
    collision: Collision | None = None
    custom_data: dict[str, str] = field(default_factory=dict)

    @property
    def is_collidable(self) -> bool:
        """True when this entity contributes a physics fixture.

        Requires BOTH a non-NONE shape and a ``<Collision>`` block — the shape selects
        which ``b2Shape`` builder runs (``ETHPhysicsSimulator.cpp:85-98``) and the
        collision box supplies its dimensions. Either alone produces nothing.
        """
        return self.shape != BS_NONE and self.collision is not None


def _parse_polygon(collision_el: ET.Element) -> list[tuple[float, float]]:
    """Read the ``<Polygon>`` payload, which is ENML text, not XML.

    Nesting matters: ``<Polygon>`` is written under the *collision* root
    (``ETHEntityProperties.cpp:490-505``), NOT as a child of ``<Entity>``. Looking for
    it one level too high silently returns "no polygons anywhere", which is exactly the
    wrong answer — chapter 1 has 220 of them.

    Vertex keys are ``v0``, ``v1``, … and are sorted numerically here because ENML is
    an unordered map upstream; winding order matters to Box2D, and ``v10`` must not
    sort between ``v1`` and ``v2``.
    """
    node = collision_el.find("Polygon")
    if node is None or not (node.text or "").strip():
        return []
    parsed = enml.parse(node.text or "")

    def index_of(key: str) -> int:
        digits = key.lstrip("v")
        return int(digits) if digits.isdigit() else -1

    verts: list[tuple[float, float]] = []
    for key in sorted(parsed, key=index_of):
        if index_of(key) < 0:
            continue
        attrs = parsed[key]
        verts.append((float(attrs.get("x", "0")), float(attrs.get("y", "0"))))
    return verts


def _parse_compound(
    collision_el: ET.Element, box: tuple[float, float]
) -> list[tuple[str, tuple[float, float], tuple[float, float]]]:
    """Read a ``<Compound>`` payload — multiple Box2D fixtures, as ENML.

    Format is `docs/ethanon-formats.md` §6.3 (`ETHCompoundShape.cpp:26-116`). All values
    are normalised −1..1 against the collision box and scaled here:

        offset = (posX, posY) * box * 0.5
        box fixture  full extents = (sizeX, sizeY) * box * 0.5
        circle       radius       = radius * box.x * 0.5   (x only, NOT an average)

    Fixtures come out sorted by ENML entity name because the engine copies them out of a
    ``std::map``, so index order is alphabetical rather than file order — which matters,
    since per-fixture friction is keyed on that index.

    An unrecognised ``shape`` is DROPPED. Upstream it produces a null fixture and crashes;
    dropping is the closest safe behaviour and is reported by the caller.
    """
    node = collision_el.find("Compound")
    if node is None or not (node.text or "").strip():
        return []
    parsed = enml.parse(node.text or "")
    out: list[tuple[str, tuple[float, float], tuple[float, float]]] = []
    for key in sorted(parsed):
        attrs = parsed[key]
        kind = attrs.get("shape", "").strip()
        offset = (
            float(attrs.get("posX", "0")) * box[0] * 0.5,
            float(attrs.get("posY", "0")) * box[1] * 0.5,
        )
        if kind == "box":
            out.append((
                "box",
                offset,
                (
                    float(attrs.get("sizeX", "0")) * box[0] * 0.5,
                    float(attrs.get("sizeY", "0")) * box[1] * 0.5,
                ),
            ))
        elif kind == "circle":
            radius = float(attrs.get("radius", "0")) * box[0] * 0.5
            out.append(("circle", offset, (radius, radius)))
    return out


def _parse_joint(collision_el: ET.Element, box: tuple[float, float]) -> dict[str, float]:
    """Read a ``<Joints>`` payload — a revolute (pin) joint, as ENML.

    Nested inside ``<Collision>``, exactly like ``<Polygon>`` and ``<Compound>``. Looking
    one level higher finds nothing, which is the same trap ``<Polygon>`` set earlier.

    Only revolute joints exist: the ENML entity name must START WITH ``revoluteJoint``
    (a prefix test, ``ETHJoint::IsRevoluteJoint``), and shipped names are ``revoluteJoint``,
    ``revoluteJoint0/1`` and ``revoluteJointA``.

    Anchors are normalised −1..1 of the entity's scaled collision half-box
    (``ETHRevoluteJoint.cpp:60-64``) and are converted to world units here. Angles are
    RADIANS — the shipped ``±1.57075`` is ``±π/2``, which is what settles the units
    question the format notes left open.
    """
    node = collision_el.find("Joints")
    if node is None or not (node.text or "").strip():
        return {}
    parsed = enml.parse(node.text or "")
    def number(attrs: dict[str, str], name: str, fallback: float) -> float:
        raw = attrs.get(name, "").strip()
        try:
            return float(raw)
        except ValueError:
            return fallback

    def truthy(attrs: dict[str, str], name: str) -> float:
        return 1.0 if attrs.get(name, "").strip().lower() in ("true", "1") else 0.0

    for key, attrs in parsed.items():
        if not key.startswith("revoluteJoint"):
            continue
        return {
            "anchor_x": number(attrs, "attachPointAX", 0.0) * box[0] * 0.5,
            "anchor_y": number(attrs, "attachPointAY", 0.0) * box[1] * 0.5,
            "enable_limit": truthy(attrs, "enableLimit"),
            # ETHJoint.cpp:80-92 — upper defaults to 2*pi, lower to 0.
            "lower_angle": number(attrs, "lowerAngle", 0.0),
            "upper_angle": number(attrs, "upperAngle", 6.2831853),
            "enable_motor": truthy(attrs, "enableMotor"),
            "motor_speed": number(attrs, "motorSpeed", 0.0),
            "max_motor_torque": number(attrs, "maxMotorTorque", 100.0),
        }
    return {}


def parse_element(element: ET.Element, *, name: str = "") -> Entity:
    """Parse an ``<Entity>`` XML element into an :class:`Entity`.

    Works for both a ``.ent`` file root and a ``.esc``-inlined block, because the
    grammar is the same. Attribute and element ORDER is irrelevant: every engine read
    is name-keyed (``ETHEntity.cpp:212-248``), and the ordering seen in shipped files
    is a writer artifact. Do not build anything order-dependent on top of this.
    """
    ent = Entity(
        name=name,
        type=_i(element, "type"),
        shape=_i(element, "shape", BS_NONE),
        static=_i(element, "static"),
        sensor=_i(element, "sensor"),
        blend_mode=_i(element, "blendMode"),
        friction=_f(element, "friction", _DEFAULT_FRICTION),
        density=_f(element, "density", _DEFAULT_DENSITY),
        restitution=_f(element, "restitution", _DEFAULT_RESTITUTION),
        gravity_scale=_f(element, "gravityScale", _DEFAULT_GRAVITY_SCALE),
    )

    sprite = element.find("Sprite")
    if sprite is not None and sprite.text:
        ent.sprite = sprite.text.strip()

    pivot = _vec(element, "PivotAdjust")
    if pivot is not None:
        ent.pivot_adjust = pivot

    collision_el = element.find("Collision")
    if collision_el is not None:
        pos = collision_el.find("Position")
        size = collision_el.find("Size")
        box = (_f(size, "x"), _f(size, "y"))
        ent.collision = Collision(
            offset=(_f(pos, "x"), _f(pos, "y")),
            size=box,
            polygon=_parse_polygon(collision_el),
            compound=_parse_compound(collision_el, box),
            joint=_parse_joint(collision_el, box),
        )

    custom = element.find("CustomData")
    if custom is not None:
        # <Variable> uses child ELEMENTS, not attributes:
        #     <Variable><Type>float</Type><Name>speed</Name><Value>4</Value></Variable>
        # An earlier version read var.get("name")/var.get("value") and therefore silently
        # parsed every custom-data bag as empty — no error, just nothing. This bag is
        # where the original's designers put per-instance gameplay parameters, so losing
        # it loses `teleportable`, `max_portals`, portal pairing, platform speeds…
        for var in custom.findall("Variable"):
            key = (var.findtext("Name") or "").strip()
            if key:
                ent.custom_data[key] = (var.findtext("Value") or "").strip()

    return ent


def parse(text: str, *, name: str = "") -> Entity:
    """Parse the contents of a ``.ent`` file.

    A ``.ent`` file's root element is ``<Ethanon>`` with the ``<Entity>`` inside it,
    whereas an inlined definition IS the ``<Entity>``. Accept either, so callers do not
    have to care which shape they hold.
    """
    root = ET.fromstring(text)
    element = root if root.tag == "Entity" else root.find("Entity")
    if element is None:
        raise ValueError(f"no <Entity> element found in {name or 'entity data'}")
    return parse_element(element, name=name)


def parse_file(path: Path) -> Entity:
    """Parse a ``.ent`` file from disk.

    Encoding is sniffed, not assumed — ``.ent`` files are canonically UTF-16LE.
    See ``textio.read_ethanon_xml``.
    """
    p = Path(path)
    return parse(read_ethanon_xml(p), name=p.stem)
