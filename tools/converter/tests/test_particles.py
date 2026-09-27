"""The particle sidecars (`particles.py`) and the `metadata/eth_particles` key.

Fixture rule (see ../README.md): everything here is HAND-WRITTEN. The .esc below has the
shape of a level's particle carriers - an inline <Entity> whose <Particles> holds one or more
<ParticleSystem> with the seventeen attributes and ten children the port's reader demands -
but every bitmap name and number in it is invented.
"""

from __future__ import annotations

import re
from pathlib import Path

import pytest

from ethanon2godot import entity, particles, scene, tscn
from ethanon2godot.__main__ import main

_SYSTEM_ATTRS = (
    'allAtOnce="0" alphaMode="1" repeat="0" animationMode="1" boundingSphere="64.000000" '
    'lifeTime="900.000000" randomLifeTime="40.000000" angleDir="0.700000" randAngle="0.000000" '
    'size="21.000000" randomizeSize="3.000000" growth="-0.200000" minSize="0.000000" '
    'maxSize="700.000000" angleStart="0.000000" randAngleStart="30.000000"'
)


def _system(count: str, bitmap: str, indent: str) -> str:
    inner = indent + "     "
    return (
        f'{indent}<ParticleSystem particles="{count}" {_SYSTEM_ATTRS}>\n'
        f"{inner}<Bitmap>{bitmap}</Bitmap>\n"
        f'{inner}<Gravity x="0.000000" y="0.010000" />\n'
        f'{inner}<Direction x="0.000000" y="-0.900000" />\n'
        f'{inner}<RandomizeDir x="0.300000" y="0.000000" />\n'
        f'{inner}<SpriteCut x="1" y="1" />\n'
        f'{inner}<StartPoint x="0.000000" y="-7.000000" z="3.000000" />\n'
        f'{inner}<RandStartPoint x="9.000000" y="9.000000" />\n'
        f'{inner}<Color0 r="0.900000" g="0.600000" b="0.200000" a="1.000000" />\n'
        f'{inner}<Color1 r="0.400000" g="0.100000" b="0.000000" a="0.000000" />\n'
        f'{inner}<Luminance r="1.000000" g="1.000000" b="1.000000" />\n'
        f"{indent}</ParticleSystem>\n"
    )


def _instance(
    iid: int, name: str, body: str, *, sprite: str | None = "carrier.png", x: float = 0.0
) -> str:
    sprite_line = f"      <Sprite>{sprite}</Sprite>\n" if sprite else ""
    return (
        f'  <Entity id="{iid}" spriteFrame="0">\n'
        f"    <EntityName>{name}</EntityName>\n"
        f'    <Position x="{x:.6f}" y="10.000000" z="0.000000" angle="0.000000" />\n'
        '    <Entity shape="0" applyLight="0" type="0" static="1" blendMode="0">\n'
        f"{sprite_line}{body}"
        "      <CustomData />\n"
        "    </Entity>\n"
        "  </Entity>\n"
    )


def _particles(*systems: str, indent: str = "      ") -> str:
    return f"{indent}<Particles>\n{''.join(systems)}{indent}</Particles>\n"


_ONE = _particles(_system("5", "ember_dot.png", "        "))
# The same systems at another indentation: the same block, so the same file.
_ONE_REINDENTED = _particles(_system("5", "ember_dot.png", "  "), indent="    ")

_ESC = (
    '<?xml version="1.0" ?>\n'
    "<Ethanon>\n"
    '  <SceneProperties lightIntensity="2.000000" parallaxIntensity="0.000000">\n'
    '    <Ambient r="1.000000" g="1.000000" b="1.000000" />\n'
    '    <ZAxisDirection x="0.000000" y="0.000000" />\n'
    "  </SceneProperties>\n"
    "  <EntitiesInScene>\n"
    # Two placements stating the same block: one file, two keys.
    + _instance(60, "brazier.ent", _ONE, x=10)
    + _instance(61, "brazier.ent", _ONE, x=20)
    + _instance(62, "brazier", _ONE_REINDENTED, x=30)
    # Three systems: the first holds no particles and the third is past the engine's two,
    # so only the second is kept.
    + _instance(
        63,
        "fountain.ent",
        _particles(
            _system("0", "unused_a.png", "        "),
            _system("7", "droplet.png", "        "),
            _system("9", "unused_b.png", "        "),
        ),
    )
    # A carrier that draws no sprite: reported, not keyed.
    + _instance(64, "vent", _ONE, sprite=None)
    # A sprite with no particles, and one whose <Particles> is empty: no key on either.
    + _instance(65, "plain.ent", "")
    + _instance(66, "empty.ent", "      <Particles />\n")
    # `particles` in the older writer's spelling, which sscanf("%d") reads as 3.
    + _instance(67, "lamp.ent", _particles(_system("3.000000", "glint.png", "        ")))
    + "  </EntitiesInScene>\n"
    "</Ethanon>\n"
)


def _convert() -> tuple[str, tscn.ConversionReport]:
    return tscn.write_scene(scene.parse(_ESC, name="carriers"), root_name="carriers")


def _key(text: str, node: str) -> str | None:
    block = text.split(f'[node name="{node}" type="Node2D" parent="."]\n', 1)[1].split("\n\n", 1)[0]
    found = re.search(r'^metadata/eth_particles = "(.*)"$', block, re.M)
    return found.group(1) if found else None


def _decode(path: Path) -> str:
    data = path.read_bytes()
    assert data[:2] == b"\xff\xfe"
    return data[2:].decode("utf-16-le")


# --- parsing -----------------------------------------------------------------------------


def test_no_particles_means_no_block() -> None:
    ent = entity.parse("<Entity><Sprite>a.png</Sprite></Entity>")
    assert (ent.particles_xml, ent.particle_systems, ent.particle_systems_dropped) == ("", 0, 0)
    assert entity.parse("<Entity><Particles /></Entity>").particles_xml == ""


def test_the_engine_keeps_at_most_two_systems_and_none_without_particles() -> None:
    # ETHEntityProperties.cpp:317-325 (ETH_MAX_PARTICLE_SYS_PER_ENTITY, ETHTypes.h:33).
    sc = scene.parse(_ESC)
    fountain = next(i for i in sc.instances if i.id == 63).entity
    assert fountain is not None
    assert fountain.particle_systems == 1
    assert fountain.particle_systems_dropped == 2
    assert "droplet.png" in fountain.particles_xml
    assert "unused_a.png" not in fountain.particles_xml
    assert "unused_b.png" not in fountain.particles_xml


def test_particles_is_read_as_sscanf_reads_it() -> None:
    lamp = next(i for i in scene.parse(_ESC).instances if i.id == 67).entity
    assert lamp is not None and lamp.particle_systems == 1
    # Not a number at all keeps the Reset() value 0, so the system is dropped.
    ent = entity.parse('<Entity><Particles><ParticleSystem particles="many"/></Particles></Entity>')
    assert (ent.particles_xml, ent.particle_systems_dropped) == ("", 1)


def test_the_block_is_the_files_own_text_without_its_indentation() -> None:
    sc = scene.parse(_ESC)
    by_id = {i.id: i.entity for i in sc.instances}
    assert by_id[60].particles_xml == by_id[62].particles_xml
    block = by_id[60].particles_xml
    assert block.startswith('<Particles><ParticleSystem particles="5" allAtOnce="0" alphaMode="1"')
    assert "\n" not in block
    assert '<StartPoint x="0.000000" y="-7.000000" z="3.000000" />' in block
    assert block.endswith("</ParticleSystem></Particles>")


# --- the key -----------------------------------------------------------------------------


def test_two_identical_blocks_name_one_file() -> None:
    text, report = _convert()
    first = _key(text, "brazier_ent_60")
    assert first is not None
    assert first == _key(text, "brazier_ent_61") == _key(text, "brazier_62")
    assert re.fullmatch(r"res://assets/entities/inline_particles/[0-9a-f]{12}\.ent", first)
    # brazier, fountain and lamp: three blocks for five keyed nodes.
    assert len(report.particle_blocks.blocks) == 3
    assert report.lighting["eth_particles"] == 5


def test_a_carrier_without_a_sprite_is_reported_and_not_keyed() -> None:
    text, report = _convert()
    assert _key(text, "vent_64") is None
    assert report.lighting_ungated["eth_particles"] == 6
    assert any("vent#64" in line and "draws no sprite" in line for line in report.skipped)


def test_no_key_without_a_kept_system() -> None:
    text, report = _convert()
    assert _key(text, "plain_ent_65") is None
    assert _key(text, "empty_ent_66") is None
    assert report.particle_systems_dropped == 2  # the fountain's first and third


def test_the_key_is_the_last_eth_line_and_the_only_one_added() -> None:
    # Converting the same scene with every <Particles> removed must give the same text less
    # exactly the eth_particles lines: what makes the regenerated levels' diff pure insertions.
    text, _ = _convert()
    bare = re.sub(r"\s*<Particles>.*?</Particles>\n|\s*<Particles />\n", "\n", _ESC, flags=re.S)
    bare_text, bare_report = tscn.write_scene(
        scene.parse(bare, name="carriers"), root_name="carriers"
    )
    assert not bare_report.particle_blocks.blocks
    kept = [line for line in text.splitlines() if not line.startswith("metadata/eth_particles")]
    assert kept == bare_text.splitlines()
    lines = text.splitlines()
    for index, line in enumerate(lines):
        if line.startswith("metadata/eth_particles"):
            assert not lines[index + 1].startswith("metadata/")


# --- the sidecar -------------------------------------------------------------------------


def test_the_sidecar_is_an_ent_in_utf16_with_a_bom() -> None:
    block = scene.parse(_ESC).instances[0].entity.particles_xml
    data = particles.sidecar_bytes(block)
    assert data[:2] == b"\xff\xfe"
    text = data[2:].decode("utf-16-le")
    assert text == f'<?xml version="1.0" ?>\n<Ethanon><Entity>{block}</Entity></Ethanon>\n'
    assert particles.sidecar_name(block) == particles.sidecar_name(block)
    assert particles.sidecar_name(block) != particles.sidecar_name(block.replace('"5"', '"6"'))


def test_a_block_outside_ascii_is_refused() -> None:
    with pytest.raises(ValueError, match="outside ASCII"):
        particles.sidecar_bytes("<Particles><ParticleSystem><Bitmap>\u00e9.png</Bitmap>"
                                "</ParticleSystem></Particles>")


def test_two_blocks_under_one_name_are_refused(monkeypatch) -> None:
    blocks = particles.ParticleBlocks()
    monkeypatch.setattr(particles, "sidecar_name", lambda _xml: "clash.ent")
    blocks.add("<Particles>a</Particles>")
    blocks.add("<Particles>a</Particles>")
    with pytest.raises(ValueError, match="clash.ent"):
        blocks.add("<Particles>b</Particles>")


def test_write_sidecars_rewrites_nothing_already_right(tmp_path) -> None:
    _, report = _convert()
    first = particles.write_sidecars(tmp_path, report.particle_blocks)
    assert (first.written, first.unchanged) == (3, 0)
    again = particles.write_sidecars(tmp_path, report.particle_blocks)
    assert (again.written, again.unchanged) == (0, 3)


def test_cli_writes_one_file_for_two_identical_blocks(tmp_path) -> None:
    src = tmp_path / "in" / "assets" / "scenes"
    src.mkdir(parents=True)
    # UTF-16LE with a BOM, like the shipped scenes.
    (src / "level0.esc").write_bytes(b"\xff\xfe" + _ESC.encode("utf-16-le"))
    out = tmp_path / "out"
    argv = ["-i", str(tmp_path / "in"), "-o", str(out), "--level", "0", "--copy-particles"]
    assert main(argv) == 0

    level = (out / "levels" / "level0.tscn").read_text(encoding="utf-8")
    keys = re.findall(r'^metadata/eth_particles = "res://(.*)"$', level, re.M)
    assert len(keys) == 5 and len(set(keys)) == 3
    files = sorted((out / "assets" / particles.SIDECAR_DIR).iterdir())
    assert [f.name for f in files] == sorted({Path(k).name for k in keys})
    brazier = out / keys[0]
    assert "ember_dot.png" in _decode(brazier)
    assert _decode(brazier).count("<ParticleSystem ") == 1


def test_without_the_flag_the_keys_are_written_and_no_file(tmp_path) -> None:
    src = tmp_path / "in" / "assets" / "scenes"
    src.mkdir(parents=True)
    (src / "level0.esc").write_bytes(b"\xff\xfe" + _ESC.encode("utf-16-le"))
    out = tmp_path / "out"
    assert main(["-i", str(tmp_path / "in"), "-o", str(out), "--level", "0"]) == 0
    assert "metadata/eth_particles" in (out / "levels" / "level0.tscn").read_text("utf-8")
    assert not (out / "assets" / particles.SIDECAR_DIR).exists()
