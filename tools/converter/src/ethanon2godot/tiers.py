"""Copy the entity textures the levels name, with the density tier file the original draws.

Output is Asantee-origin content and stays under the gitignored output directory.

WHICH FILE. Every image goes through ``ETHGraphicResourceManager::AddFile``
(``ETHResourceManager.cpp:133-144``), which asks ``ETHSpriteDensityManager::ChooseSpriteVersion``
(``ETHSpriteDensityManager.cpp:88-139``) for ``<dir>/fullhd/<file>`` when the screen is at least
``minScreenHeightForFullHdVersion`` tall, then ``<dir>/hd/<file>`` when it is at least
``minScreenHeightForHdVersion`` tall or the fullhd test passed, then (short screens only)
``xld/`` and ``ld/``, each guarded by ``FileExists``, else the named file at density 1. Only the
tier names are tried: whether the 1x file exists is never asked.

WHICH NUMBERS. ``app.enml``, read the way ``ETHAppEnmlFile`` reads it: constructor defaults
(``ETHAppEnmlFile.cpp:39-52``), then the ``default``, ``window`` and platform blocks in that order
(``:59-66``, ``LoadProperties`` ``:111-148``), platform ``android``
(``gs2d/src/Platform/android/Platform.android.cpp:31-34``). The engine's own defaults are NOT the
game's: fullhd density 4 and thresholds 720 / 1080. The shipped file sets 480 / 600 and density 2
for both tiers.

WHICH SCREEN. The original re-derives the choice from the device's screen height. Enhanced fixes
it at 720 px, where every library frame of the original was measured (owner ruling R12), and the
port resolves the file itself (route B, ruling R13): the converter copies the tier files beside the
1x copies and ``out/levels`` does not change.

WHICH SIZE. ``GLES2Sprite::SetSpriteDensityValue`` (``GLES2Sprite.cpp:429-438``) makes the bitmap
``texels / D`` and the frame cut reads it through an int cast (``:389-392``), so the size in world
units is ``int(texels / D)``. The ``hd/`` entity files are exactly 2x, but four ``fullhd/`` skies
are 1024x512 over a 455x256 1x file: 512x256 units. Unlike ``uiassets._copy_hd``, a tier file of
another size is NOT refused (the original draws it); it is copied and reported.
"""

from __future__ import annotations

import filecmp
import json
import shutil
from dataclasses import dataclass, field, replace
from pathlib import Path

from . import enml
from .imagesize import read_size

TIER_BASE = "1x"
MANIFEST_NAME = "entity_tiers.json"
# Owner ruling R12: the choice the original makes on a 720 px tall screen, for every window.
FIXED_SCREEN_HEIGHT_PX = 720
# gs2d::Application::GetPlatformName() on Android (Platform.android.cpp:31-34).
PLATFORM_NAME = "android"
# ETH_DEFAULT_PROPERTY_APP_ENML_ENTITY and ETH_OLDER_DEFAULT_PROPERTY_APP_ENML_ENTITY
# (ETHTypes.h:41-42), then the platform's own block.
_BLOCKS = ("default", "window", PLATFORM_NAME)


@dataclass(frozen=True)
class DensitySettings:
    """The ``ETHAppEnmlFile`` fields the density manager takes.

    ``ETHSpriteDensityManager::FillParametersFromFile``, ``ETHSpriteDensityManager.cpp:47-59``.

    Defaults are the ``ETHAppEnmlFile`` constructor's (``ETHAppEnmlFile.cpp:39-52``).
    """

    hd_density: float = 2.0
    fullhd_density: float = 4.0
    ld_density: float = 0.5
    xld_density: float = 0.25
    min_height_for_hd: int = 720
    min_height_for_fullhd: int = 1080
    max_height_before_nd: int = 480
    max_height_before_ld: int = 320


_FLOAT_KEYS = {
    "hdDensityValue": "hd_density",
    "fullHdDensityValue": "fullhd_density",
    "ldDensityValue": "ld_density",
    "xldDensityValue": "xld_density",
}
_UINT_KEYS = {
    "minScreenHeightForHdVersion": "min_height_for_hd",
    "minScreenHeightForFullHdVersion": "min_height_for_fullhd",
    "maxScreenHeightBeforeNdVersion": "max_height_before_nd",
    "maxScreenHeightBeforeLdVersion": "max_height_before_ld",
}


def read_density_settings(app_enml: Path) -> tuple[DensitySettings, str | None]:
    """The density settings ``app_enml`` gives, and a warning when the file could not be used.

    On a missing or unparsable file the engine keeps every default (``ETHAppEnmlFile.cpp:53-74``);
    so does this, but it says so, because the defaults halve the size of a fullhd sprite. A value
    is read with ``float``/``int``, stricter than the engine's ``sscanf`` (``Enml.cpp:523-549``):
    text after a number keeps the default here. No shipped value has any.
    """
    try:
        blocks = enml.parse_file(app_enml)
    except (OSError, enml.EnmlError) as exc:
        return DensitySettings(), f"{app_enml}: {exc}; the engine defaults are used"
    settings = DensitySettings()
    for block in _BLOCKS:
        entity = blocks.get(block)
        if entity is None:
            continue
        for keys, parse in ((_FLOAT_KEYS, float), (_UINT_KEYS, int)):
            for key, name in keys.items():
                if key not in entity:
                    continue
                try:
                    settings = replace(settings, **{name: parse(entity[key])})
                except ValueError:
                    pass
    return settings, None


def search_order(
    settings: DensitySettings, screen_height: int = FIXED_SCREEN_HEIGHT_PX
) -> list[tuple[str, float]]:
    """The tier folders ``ChooseSpriteVersion`` tries on this screen, in order, with densities.

    ``ETHSpriteDensityManager.cpp:61-79`` (the four tests) and ``:94-135`` (their order).
    """
    order: list[tuple[str, float]] = []
    full_hd = screen_height >= settings.min_height_for_fullhd
    if full_hd:
        order.append(("fullhd", settings.fullhd_density))
    hd = screen_height >= settings.min_height_for_hd and not full_hd
    if hd or full_hd:
        order.append(("hd", settings.hd_density))
    xld = screen_height <= settings.max_height_before_ld
    if xld:
        order.append(("xld", settings.xld_density))
    ld = screen_height <= settings.max_height_before_nd and not xld
    if ld or xld:
        order.append(("ld", settings.ld_density))
    return order


def units(texels: tuple[int, int], density: float) -> tuple[int, int]:
    """World size of an uncut image: ``int(texels / D)`` per axis (``GLES2Sprite.cpp:389-392``)."""
    return (int(texels[0] / density), int(texels[1] / density))


@dataclass
class TierCopyReport:
    # Files written, and files whose destination already held the same bytes (left untouched).
    copied: dict[str, int] = field(default_factory=lambda: {TIER_BASE: 0, "tier": 0})
    unchanged: dict[str, int] = field(default_factory=lambda: {TIER_BASE: 0, "tier": 0})
    # Textures by the tier drawn: "fullhd" / "hd" / ... / "1x".
    tiers: dict[str, int] = field(default_factory=dict)
    # A tier file whose size in units is not its 1x file's.
    size_changes: list[str] = field(default_factory=list)
    # A tier file with no 1x file beside it (drawn all the same).
    no_base: list[str] = field(default_factory=list)
    skipped: list[str] = field(default_factory=list)
    # "entities/<tex>" -> manifest row.
    files: dict[str, dict[str, object]] = field(default_factory=dict)


def _copy_if_changed(source: Path, dest: Path) -> bool:
    """Copy unless ``dest`` already holds the same bytes; True when a file was written.

    Other processes read ``out/assets`` while the converter runs (the port's capture runs), so an
    identical file is never truncated and rewritten.
    """
    if dest.is_file() and filecmp.cmp(source, dest, shallow=False):
        return False
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, dest)
    return True


def _number(value: float) -> float | int:
    return int(value) if float(value).is_integer() else value


def copy_entity_textures(
    input_root: Path,
    output_root: Path,
    textures: set[str],
    settings: DensitySettings,
    screen_height: int = FIXED_SCREEN_HEIGHT_PX,
) -> TierCopyReport:
    """Copy each texture's 1x file and the tier file drawn, from ``<input_root>/assets/entities/``.

    Nothing under the destination is deleted, and a destination with the same bytes is not
    rewritten. The manifest ``<output_root>/assets/entity_tiers.json`` is merged, not replaced.
    """
    src = input_root / "assets" / "entities"
    dst = output_root / "assets" / "entities"
    order = search_order(settings, screen_height)
    report = TierCopyReport()

    for tex in sorted(textures):
        base = src / tex
        if base.is_file():
            if _copy_if_changed(base, dst / tex):
                report.copied[TIER_BASE] += 1
            else:
                report.unchanged[TIER_BASE] += 1

        drawn, tier, density = base, TIER_BASE, 1.0
        # ``Path.is_file`` ignores letter case on Windows; the original's ``FileExists`` on Android
        # does not. No texture the 128 levels name differs in case from its tier file (0 of 66), so
        # the choice is the same today. The port's tier lookup carries the same caveat.
        for folder, folder_density in order:
            candidate = src / folder / tex
            if candidate.is_file():
                drawn, tier, density = candidate, folder, folder_density
                break
        if not drawn.is_file():
            report.skipped.append(f"entities/{tex}: not found, and no tier file either")
            continue
        drawn_rel = f"entities/{tex}" if tier == TIER_BASE else f"entities/{tier}/{tex}"
        if tier != TIER_BASE:
            if _copy_if_changed(drawn, dst / tier / tex):
                report.copied["tier"] += 1
            else:
                report.unchanged["tier"] += 1
        report.tiers[tier] = report.tiers.get(tier, 0) + 1

        texels = read_size(drawn)
        size = None if texels is None else units(texels, density)
        if size is None:
            report.skipped.append(f"{drawn_rel}: size unreadable; copied, recorded with no size")
        elif tier != TIER_BASE:
            if not base.is_file():
                report.no_base.append(
                    f"{drawn_rel}: no 1x file; drawn at {size[0]}x{size[1]} units"
                )
            else:
                base_size = read_size(base)
                if base_size is None:
                    report.skipped.append(f"entities/{tex}: 1x size unreadable, not compared")
                elif base_size != size:
                    report.size_changes.append(
                        f"{drawn_rel}: {size[0]}x{size[1]} units against the 1x file's "
                        f"{base_size[0]}x{base_size[1]}"
                    )
        report.files[f"entities/{tex}"] = {
            "drawn": drawn_rel,
            "tier": tier,
            "density": _number(density),
            "texels": list(texels) if texels is not None else None,
            "units": list(size) if size is not None else None,
        }

    _merge_manifest(
        output_root / "assets" / MANIFEST_NAME,
        report.files,
        {
            "screen_height_px": screen_height,
            "search": [{"tier": name, "density": _number(d)} for name, d in order],
        },
    )
    return report


def _merge_manifest(
    path: Path, files: dict[str, dict[str, object]], choice: dict[str, object]
) -> None:
    """Merge over an existing manifest, so four ``--world`` runs build one record.

    The same rule as ``lighting._merge_manifest``: a corrupt or foreign file is replaced.
    ``choice`` is written as this run's and is not compared with the stored one: rows from an
    earlier run under another ``app.enml`` or screen height would be kept beside this run's. All
    four ``--world`` runs share one choice today (fullhd D 2, hd D 2 at 720 px).
    """
    existing: dict[str, dict[str, object]] = {}
    if path.is_file():
        try:
            loaded = json.loads(path.read_text(encoding="utf-8"))
            if isinstance(loaded, dict) and isinstance(loaded.get("files"), dict):
                existing = loaded["files"]
        except (ValueError, OSError):  # JSONDecodeError and UnicodeDecodeError are ValueErrors
            existing = {}
    existing.update(files)
    payload = {"format": 1, "choice": choice, "files": dict(sorted(existing.items()))}
    text = json.dumps(payload, indent=2) + "\n"
    try:
        if path.read_text(encoding="utf-8") == text:
            return  # the same record: not rewritten under a reader
    except (ValueError, OSError):
        pass
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")
