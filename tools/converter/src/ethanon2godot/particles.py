"""The particle sidecars: each placement's own ``<Particles>``, as a file the port reads.

A level places its particle systems INLINE: every instance's ``<Entity>`` body carries its
own ``<Particles>`` block, which the engine reads and draws (``ETHEntityProperties.cpp:304-333``,
``ETHSpriteEntity.cpp:293-322``). The port's particle reader (``game/sim/Particles.cpp``,
``Particles::Load``) reads an ``.ent``, so each kept block is written as one: UTF-16LE with a
byte-order mark, as every ``.ent`` of the original's is (``textio.py``), the block inside an
``<Ethanon><Entity>`` wrapper that the reader passes over. The node names its file with
``metadata/eth_particles`` (``tscn.py``).

The files are CONTENT-ADDRESSED: named by the SHA-1 of the block's canonical text
(``entity._parse_particles``), so placements that state the same systems share one file,
and a re-run writes the same names. The 897 systems the 128 levels place come to 24 blocks.

This is Asantee data cut out of the scenes, so it goes under the gitignored output
directory only (the hard rule in CLAUDE.md).
"""

from __future__ import annotations

import hashlib
from dataclasses import dataclass, field
from pathlib import Path

from .textio import UTF16LE_BOM

# Inside the project, as `<SIDECAR_DIR>/<hash>.ent` under `assets/`. Fixed rather than
# derived from the output directory, so a staging run and a real run emit the same text.
SIDECAR_DIR = "entities/inline_particles"
SIDECAR_PREFIX = f"res://assets/{SIDECAR_DIR}"
# Twelve hex digits of the SHA-1: 48 bits for two dozen blocks. A clash within one run is
# refused, not assumed away (ParticleBlocks.add); one with a file an earlier run left is not
# checked, and write_sidecars overwrites that file.
_NAME_DIGITS = 12
_XML_DECLARATION = '<?xml version="1.0" ?>'


def sidecar_name(particles_xml: str) -> str:
    """The file name a block is written under: ``<sha1-12>.ent``."""
    digest = hashlib.sha1(particles_xml.encode("utf-8")).hexdigest()
    return f"{digest[:_NAME_DIGITS]}.ent"


def sidecar_text(particles_xml: str) -> str:
    """The sidecar's text: the block in the ``.ent`` shape ``Particles::Load`` reads."""
    return f"{_XML_DECLARATION}\n<Ethanon><Entity>{particles_xml}</Entity></Ethanon>\n"


def sidecar_bytes(particles_xml: str) -> bytes:
    """The sidecar's bytes: ``FF FE`` then UTF-16LE, the only encoding the port's reader takes.

    The reader also refuses any character outside ASCII (``Particles.cpp``, ``ReadUtf16``);
    every shipped block is ASCII, and one that is not is refused here, by name, rather than
    written for the port to refuse at run time.
    """
    text = sidecar_text(particles_xml)
    if not text.isascii():
        name = sidecar_name(particles_xml)
        raise ValueError(f"{name}: the block holds a character outside ASCII")
    return UTF16LE_BOM + text.encode("utf-16-le")


@dataclass
class ParticleBlocks:
    """The distinct blocks a conversion named, by file name."""

    blocks: dict[str, str] = field(default_factory=dict)

    def add(self, particles_xml: str) -> str:
        """Record a block and return its file name. Two different blocks under one name in
        this run is a hash clash and fails the conversion: sharing the file would draw one
        block's systems on the other's placements."""
        name = sidecar_name(particles_xml)
        known = self.blocks.setdefault(name, particles_xml)
        if known != particles_xml:
            raise ValueError(f"{name}: two different <Particles> blocks share this name")
        return name

    def update(self, other: ParticleBlocks) -> None:
        for particles_xml in other.blocks.values():
            self.add(particles_xml)


@dataclass
class SidecarReport:
    written: int = 0
    unchanged: int = 0


def write_sidecars(output_root: Path, blocks: ParticleBlocks) -> SidecarReport:
    """Write every block to ``<output_root>/assets/<SIDECAR_DIR>/``.

    A file already holding the right bytes is left alone, so four ``--world`` runs into one
    output directory (which share blocks) rewrite nothing twice. A file of the same name
    holding other bytes is overwritten: the clash refusal (``ParticleBlocks.add``) holds
    within one run only.
    """
    directory = output_root / "assets" / SIDECAR_DIR
    directory.mkdir(parents=True, exist_ok=True)
    report = SidecarReport()
    for name, particles_xml in sorted(blocks.blocks.items()):
        data = sidecar_bytes(particles_xml)
        path = directory / name
        if path.is_file() and path.read_bytes() == data:
            report.unchanged += 1
            continue
        path.write_bytes(data)
        report.written += 1
    return report
