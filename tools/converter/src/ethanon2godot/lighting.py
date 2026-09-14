"""Copy the files the ``eth_*`` lighting keys point at: lightmaps, normal maps, halo bitmaps.

Output is Asantee-origin content and stays under the gitignored output directory.

What goes where (the ``res://`` paths ``tscn.py`` emits, relative to ``<output>/assets/``):

* ``scenes/<scene>-esc/add<id>.png`` -> ``lightmaps/<scene>/add<id>.png``, only for the
  lightmaps a level actually attached. Never the ``.pkm``/``.pvr`` beside them: a pixel fit
  of the running original prefers the PNG on 11 of 11 lightmapped entities.
* ``entities/normalmaps/*`` -> ``entities/normalmaps/``, ALL of them, because
  script-spawned entities (the player's ``normalmap_77.png``) use files no level names.
* ``entities/<HaloBitmap>`` -> ``entities/<HaloBitmap>``, the ones levels name.

TIERS. All three kinds are loaded through ``ETHGraphicResourceManager::AddFile``, which asks
``ETHSpriteDensityManager::ChooseSpriteVersion`` for ``fullhd/<name>``, then ``hd/<name>``,
then the base file (``ETHResourceManager.cpp:133-134``, ``ETHSpriteDensityManager.cpp:88-113``).
At the original's 720 px both HD tiers are eligible, so a tier file beside any of these would
be what the original draws. None exists in the shipped APK (no ``hd/`` or ``fullhd/`` under any
``-esc`` directory or under ``normalmaps/``, and no halo in ``entities/hd|fullhd/``), so every
copied file is the base (1x) tier. That is checked per file, not assumed: a tier sibling makes
the file SKIPPED and reported, because copying the base file would silently be the wrong
content. The tier of every copied file is written to ``<output>/assets/lighting_files.json``.
"""

from __future__ import annotations

import json
import shutil
from dataclasses import dataclass, field
from pathlib import Path

TIER_BASE = "1x"
# ETHSpriteDensityManager's search order (FULL_HD_VERSION_PATH_NAME, HD_VERSION_PATH_NAME).
_TIER_DIRS = ("fullhd", "hd")
MANIFEST_NAME = "lighting_files.json"


@dataclass
class LightingCopyReport:
    copied: dict[str, int] = field(default_factory=dict)
    skipped: list[str] = field(default_factory=list)
    # Destination (relative to <output>/assets/) -> {"source": ..., "tier": ...}.
    files: dict[str, dict[str, str]] = field(default_factory=dict)


def _tier_sibling(source: Path) -> Path | None:
    for tier in _TIER_DIRS:
        candidate = source.parent / tier / source.name
        if candidate.is_file():
            return candidate
    return None


def _copy_one(
    source: Path, assets_root: Path, dest_rel: str, input_assets: Path, report: LightingCopyReport
) -> bool:
    if not source.is_file():
        report.skipped.append(f"{dest_rel}: source not found ({source})")
        return False
    sibling = _tier_sibling(source)
    if sibling is not None:
        report.skipped.append(
            f"{dest_rel}: a density tier exists ({sibling.relative_to(input_assets).as_posix()});"
            " the original would draw that, so the base file is not copied"
        )
        return False
    dest = assets_root / dest_rel
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, dest)
    report.files[dest_rel] = {
        "source": source.relative_to(input_assets).as_posix(),
        "tier": TIER_BASE,
    }
    return True


def copy_lighting_assets(
    input_root: Path,
    output_root: Path,
    *,
    lightmaps: list[tuple[Path, str]],
    normals: set[str],
    halos: set[str],
) -> LightingCopyReport:
    """Copy from ``<input_root>/assets/`` into ``<output_root>/assets/``.

    ``lightmaps`` is ``ConversionReport.lightmaps`` (source file, path under
    ``lightmaps/``); ``normals`` and ``halos`` are the names the levels referenced, used to
    report a reference with no file behind it.
    """
    src = input_root / "assets"
    dst = output_root / "assets"
    report = LightingCopyReport()

    count = 0
    for source, rel in sorted(lightmaps, key=lambda item: item[1]):
        count += _copy_one(source, dst, f"lightmaps/{rel}", src, report)
    report.copied["lightmaps"] = count

    normal_dir = src / "entities" / "normalmaps"
    count = 0
    if normal_dir.is_dir():
        for found in sorted(p for p in normal_dir.iterdir() if p.is_file()):
            count += _copy_one(found, dst, f"entities/normalmaps/{found.name}", src, report)
    for name in sorted(normals):
        if not (normal_dir / name).is_file():
            report.skipped.append(f"entities/normalmaps/{name}: referenced but not found")
    report.copied["normalmaps"] = count

    count = 0
    for name in sorted(halos):
        count += _copy_one(src / "entities" / name, dst, f"entities/{name}", src, report)
    report.copied["halos"] = count

    _merge_manifest(dst / MANIFEST_NAME, report.files)
    return report


def _merge_manifest(path: Path, files: dict[str, dict[str, str]]) -> None:
    """Merge over an existing manifest, so four ``--world`` runs build one record."""
    existing: dict[str, dict[str, str]] = {}
    if path.is_file():
        try:
            loaded = json.loads(path.read_text(encoding="utf-8"))
            if isinstance(loaded, dict) and isinstance(loaded.get("files"), dict):
                existing = loaded["files"]
        except (json.JSONDecodeError, OSError):
            existing = {}
    existing.update(files)
    path.parent.mkdir(parents=True, exist_ok=True)
    payload = {"format": 1, "files": dict(sorted(existing.items()))}
    path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
