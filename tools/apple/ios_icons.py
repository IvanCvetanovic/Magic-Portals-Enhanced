#!/usr/bin/env python3
"""The iOS app icons, DRAWN opaque (planning step 206, PORT-8): the same two rings as tools/art/make_app_icon.py, on a solid tile.

    python tools/apple/ios_icons.py --out DIR

Why a tool of its own: tools/art/make_app_icon.py draws a disc on a TRANSPARENT canvas (right for a taskbar, a launcher and an .icns), and iOS fills what
is transparent with black and the App Store refuses an icon with an alpha channel. So the same drawing (make_app_icon.render, imported, not copied) is laid
over a tile of the disc's own edge colour and written as 8-bit RGB PNGs with no alpha channel, at the four sizes the bundle's Info.plist names
(game/ios/Info.plist.in, CFBundleIconFiles): 60 pt at 2x and 3x (iPhone), 76 pt at 2x and 83.5 pt at 2x (iPad).

Standard library only, and nothing is read: the picture is arithmetic, so no byte of the original game is in it. The icon is a PLACEHOLDER; the real one is
the owner's. Never part of this repository: the tool writes into a folder it is given and refuses a tracked place inside the repository (the same rule as
make_app_icon.py's, which it calls).
"""

from __future__ import annotations

import argparse
import importlib.util
import struct
import sys
import zlib
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
_TOOL = REPO / "tools" / "art" / "make_app_icon.py"
_spec = importlib.util.spec_from_file_location("make_app_icon", _TOOL)
icon = importlib.util.module_from_spec(_spec)
sys.modules[_spec.name] = icon
_spec.loader.exec_module(icon)

# (file name in the bundle, pixels a side). The names are CFBundleIconFiles' base names plus the scale and device suffix iOS looks for.
ICONS = (
    ("AppIcon60x60@2x.png", 120),
    ("AppIcon60x60@3x.png", 180),
    ("AppIcon76x76@2x~ipad.png", 152),
    ("AppIcon83.5x83.5@2x~ipad.png", 167),
)
# The tile: the drawing's own disc edge colour, so the corners the disc leaves are the colour its rim fades to and not a third one.
TILE = tuple(int(round(c)) for c in icon.DISC_EDGE)


def opaque_rgb(size: int) -> bytes:
    """The icon at `size`, laid over the tile: straight-alpha RGBA in, RGB out, row by row from the top."""
    rgba = icon.render(size)
    out = bytearray(size * size * 3)
    for i in range(size * size):
        r, g, b, a = rgba[4 * i:4 * i + 4]
        for channel, (value, tile) in enumerate(zip((r, g, b), TILE)):
            out[3 * i + channel] = (value * a + tile * (255 - a) + 127) // 255
    return bytes(out)


def png_rgb(size: int, rgb: bytes) -> bytes:
    """An RGB 8-bit PNG (colour type 2, so NO alpha channel): IHDR, one IDAT (every row filter 0, zlib level 9), IEND."""
    if len(rgb) != size * size * 3:
        raise ValueError("the pixel data is not size x size RGB")
    stride = size * 3
    raw = bytearray()
    for y in range(size):
        raw.append(0)
        raw += rgb[y * stride:(y + 1) * stride]
    packer = zlib.compressobj(9, zlib.DEFLATED, 15, 8, zlib.Z_DEFAULT_STRATEGY)
    idat = packer.compress(bytes(raw)) + packer.flush()
    return icon.PNG_SIGNATURE + icon._chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 2, 0, 0, 0)) + icon._chunk(b"IDAT", idat) + icon._chunk(b"IEND", b"")


def build() -> dict[str, bytes]:
    """Every file of the set, by name. Pure: nothing is written."""
    return {name: png_rgb(size, opaque_rgb(size)) for name, size in ICONS}


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Draw the iOS app icons, opaque (no input is read).")
    parser.add_argument("--out", type=Path, required=True, help="where to write the four files")
    args = parser.parse_args(argv)
    icon.check_out_folder(args.out, REPO)
    files = build()
    args.out.mkdir(parents=True, exist_ok=True)
    for name, data in files.items():
        (args.out / name).write_bytes(data)
    print(f"wrote {len(files)} files into {args.out.resolve()}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
