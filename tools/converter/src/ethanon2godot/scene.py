"""Parser for Ethanon ``.esc`` scene files.

A ``.esc`` holds scene-level properties (ambient light, z-axis direction, parallax)
plus the list of placed entity *instances*. For this project a ``.esc`` is a level.

Structure, confirmed against all 32 chapter-1 scenes:

    <Ethanon>
      <SceneProperties lightIntensity=… parallaxIntensity=…>
        <Ambient r= g= b=/>
        <ZAxisDirection x= y=/>
      </SceneProperties>
      <EntitiesInScene>
        <Entity id=… spriteFrame=…>          <- the INSTANCE
          <EntityName>door_bg.ent</EntityName>
          <Position x= y= z= angle=/>
          <Entity …>…</Entity>                <- the inline DEFINITION (see entity.py)
        </Entity>
        …

``<EntityName>`` is a **label, not a path**. Nothing appends ``.ent`` to it
(``docs/ethanon-formats.md`` §4.5), so a bare name never resolves to a file — and
pairing one with a same-named ``.ent`` is a bug: chapter 1's inline ``crate`` and
``crate.ent`` carry different density, friction and art.

Reference: ``engine/Scene/ETHScene.cpp``, ``ETHSceneProperties.cpp``,
``ETHEntity::ReadInSceneDataFromXMLFile`` (``ETHEntity.cpp:210-253``).
"""

from __future__ import annotations

import xml.etree.ElementTree as ET
from dataclasses import dataclass, field
from pathlib import Path

from .entity import Entity, _f, _i, parse_element
from .textio import read_ethanon_xml


@dataclass
class SceneProperties:
    """Scene-level render settings.

    ``z_axis_direction`` is ``(0, 0)`` in all 133 shipped scenes, which fully disables
    the engine's pseudo-3D z projection: ``ToScreenPos`` multiplies the z term out
    (``engine/Util/ETHASUtil.cpp:123-126``). It is kept as data rather than assumed,
    because the engine default is ``(0, -1)`` and a non-zero value would displace every
    entity vertically.
    """

    ambient: tuple[float, float, float] = (1.0, 1.0, 1.0)
    light_intensity: float = 2.0
    parallax_intensity: float = 0.0
    z_axis_direction: tuple[float, float] = (0.0, -1.0)


@dataclass
class Instance:
    """One placed entity: where it goes, plus the definition of what it is."""

    id: int
    entity_name: str
    position: tuple[float, float, float] = (0.0, 0.0, 0.0)
    angle: float = 0.0
    sprite_frame: int = 0
    entity: Entity | None = None

    def screen_pos(self, z_axis_direction: tuple[float, float]) -> tuple[float, float]:
        """World position projected to screen space.

        ``ToScreenPos`` (``ETHASUtil.cpp:123-126``):
        ``(pos.x, pos.y) + zAxisDirection * pos.z``. With the shipped ``(0, 0)`` this
        is the identity on x/y and ``z`` becomes pure depth ordering.
        """
        x, y, z = self.position
        return (x + z_axis_direction[0] * z, y + z_axis_direction[1] * z)


@dataclass
class Scene:
    """An Ethanon scene (``.esc``) — one level."""

    name: str = ""
    properties: SceneProperties = field(default_factory=SceneProperties)
    instances: list[Instance] = field(default_factory=list)


def parse(text: str, *, name: str = "") -> Scene:
    """Parse the contents of a ``.esc`` file."""
    root = ET.fromstring(text)
    scene = Scene(name=name)

    props_el = root.find("SceneProperties")
    if props_el is not None:
        ambient = props_el.find("Ambient")
        zaxis = props_el.find("ZAxisDirection")
        scene.properties = SceneProperties(
            ambient=(
                _f(ambient, "r", 1.0),
                _f(ambient, "g", 1.0),
                _f(ambient, "b", 1.0),
            ),
            light_intensity=_f(props_el, "lightIntensity", 2.0),
            parallax_intensity=_f(props_el, "parallaxIntensity", 0.0),
            # ETHSceneProperties.cpp:92 — Reset() default is (0, -1), not (0, 0).
            z_axis_direction=(_f(zaxis, "x", 0.0), _f(zaxis, "y", -1.0))
            if zaxis is not None
            else (0.0, -1.0),
        )

    container = root.find("EntitiesInScene")
    # Only direct children: a nested <Entity> is the inline definition of its parent
    # instance, not a second instance. iter() would double-count every one of them.
    for inst_el in [] if container is None else container.findall("Entity"):
        name_el = inst_el.find("EntityName")
        pos_el = inst_el.find("Position")
        inst = Instance(
            id=_i(inst_el, "id"),
            entity_name=(name_el.text or "").strip() if name_el is not None else "",
            position=(_f(pos_el, "x"), _f(pos_el, "y"), _f(pos_el, "z")),
            angle=_f(pos_el, "angle"),
            sprite_frame=_i(inst_el, "spriteFrame"),
        )
        # Branch B: an inline <Entity> child IS the definition. Branch A (a <FileName>
        # child, meaning "look this up in the entity cache") never occurs in chapter 1,
        # so resolving it is left to the caller that has an entity directory to hand.
        inline = inst_el.find("Entity")
        if inline is not None and inline.find("FileName") is None:
            inst.entity = parse_element(inline, name=inst.entity_name)
        scene.instances.append(inst)

    return scene


def parse_file(path: Path) -> Scene:
    """Parse a ``.esc`` file from disk.

    Encoding is sniffed, not assumed — ``.esc`` files are canonically UTF-16LE.
    See ``textio.read_ethanon_xml``.
    """
    p = Path(path)
    return parse(read_ethanon_xml(p), name=p.stem)
