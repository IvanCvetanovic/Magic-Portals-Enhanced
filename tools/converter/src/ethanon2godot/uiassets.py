"""Copy the assets no level references: UI sprites, their `hd/` tier, fonts and audio.

These are copied wholesale, unlike entity textures, because there is no level data naming
a menu button to filter on. Output is Asantee-origin content and stays gitignored.

TIERS. The original picks the best density tier present per file by physical screen height
(`ETHSpriteDensityManager`, docs/ethanon-formats.md §8.5, and the 2026-09-14 block for the game's
own thresholds: hd >= 480 px, fullhd >= 600 px, both density 2). The UI has no `fullhd/` folder,
so for sprites `hd/` is what the original draws. Measured over the APK, `hd/` is a clean 2x of
the 1x art — 66/69 UI sprites exactly — so it changes no layout; an `hd/` file that is not
exactly 2x its 1x counterpart is skipped and reported rather than allowed to resize something
silently. (Entity textures are a different case: their `fullhd/` files ARE drawn and change
world size, e.g. the skies become 512 units wide. That belongs to the entity texture copy.)
"""

from __future__ import annotations

import re
import shutil
from dataclasses import dataclass, field
from pathlib import Path

from .imagesize import read_size

HD_FACTOR = 2
_FONT_PAGE = re.compile(r'^page\s+id=\d+\s+file="([^"]+)"', re.MULTILINE)
_ALPHA_GLYPH_AND_OUTLINE = re.compile(r"\balphaChnl=2\b")


@dataclass
class UiCopyReport:
    copied: dict[str, int] = field(default_factory=dict)
    skipped: list[str] = field(default_factory=list)
    rewritten: list[str] = field(default_factory=list)


def copy_ui_assets(input_root: Path, output_root: Path) -> UiCopyReport:
    """Copy from ``<input_root>/assets/`` into ``<output_root>/assets/``."""
    src = input_root / "assets"
    dst = output_root / "assets"
    report = UiCopyReport()

    report.copied["sprites"] = _copy_glob(src / "sprites", dst / "sprites", ("*.png", "*.bmp"))
    report.copied["soundfx"] = _copy_glob(src / "soundfx", dst / "soundfx", ("*.mp3",))
    report.copied["sprites/hd"] = _copy_hd(src / "sprites", dst / "sprites", report)
    report.copied["fonts"] = _copy_fonts(src / "data", dst / "fonts", report)
    return report


def _copy_glob(src_dir: Path, dst_dir: Path, patterns: tuple[str, ...]) -> int:
    if not src_dir.is_dir():
        return 0
    dst_dir.mkdir(parents=True, exist_ok=True)
    copied = 0
    for pattern in patterns:
        for found in sorted(src_dir.glob(pattern)):
            shutil.copy2(found, dst_dir / found.name)
            copied += 1
    return copied


def _copy_hd(base_src: Path, base_dst: Path, report: UiCopyReport) -> int:
    hd_src = base_src / "hd"
    if not hd_src.is_dir():
        return 0
    hd_dst = base_dst / "hd"
    hd_dst.mkdir(parents=True, exist_ok=True)
    copied = 0
    for found in sorted(hd_src.glob("*.png")):
        one_x = base_src / found.name
        if not one_x.is_file():
            report.skipped.append(f"sprites/hd/{found.name}: no 1x counterpart")
            continue
        small, big = read_size(one_x), read_size(found)
        if small is None or big is None:
            report.skipped.append(f"sprites/hd/{found.name}: size unreadable")
            continue
        if big != (small[0] * HD_FACTOR, small[1] * HD_FACTOR):
            report.skipped.append(
                f"sprites/hd/{found.name}: {small} -> {big} is not exactly x{HD_FACTOR}"
            )
            continue
        shutil.copy2(found, hd_dst / found.name)
        copied += 1
    return copied


def _copy_fonts(data_src: Path, fonts_dst: Path, report: UiCopyReport) -> int:
    """Each AngelCode `.fnt` plus the page images it names, kept side by side."""
    if not data_src.is_dir():
        return 0
    fonts_dst.mkdir(parents=True, exist_ok=True)
    copied = 0
    for fnt in sorted(data_src.glob("*.fnt")):
        pages = _FONT_PAGE.findall(fnt.read_text(encoding="ascii", errors="replace"))
        missing = [p for p in pages if not (data_src / p).is_file()]
        if missing:
            report.skipped.append(f"fonts/{fnt.name}: missing page(s) {missing}")
            continue
        text = fnt.read_text(encoding="ascii", errors="replace")
        # AngelCode channel code 2 is "glyph + outline". Godot's loader rejects it outright
        # ("Unsupported BMFont texture format", scene/resources/font.cpp), which also fails
        # the whole `--import`. Only Verdana20_shadow uses it; its alpha still holds the
        # glyph, so declaring it as plain glyph (0) renders the same pixels.
        if _ALPHA_GLYPH_AND_OUTLINE.search(text):
            text = _ALPHA_GLYPH_AND_OUTLINE.sub("alphaChnl=0", text)
            report.rewritten.append(f"fonts/{fnt.name}: alphaChnl=2 -> 0 for Godot")
        (fonts_dst / fnt.name).write_text(text, encoding="ascii", newline="")
        for page in pages:
            shutil.copy2(data_src / page, fonts_dst / page)
        copied += 1
    return copied
