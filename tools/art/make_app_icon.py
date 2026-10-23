#!/usr/bin/env python3
"""Magic Portals Enhanced's application icon, DRAWN: two concentric rings, one blue and one orange, on a dark disc (planning step 201, PORT-3).

Standard library only, and nothing is read: the icon is arithmetic. No file of the original game, no screenshot and no picture of any kind is
opened, sampled or traced, so no byte of anyone's art is in the output, and the two colours below are this tool's own choice (the original's
portals are blue and red; the orange is a pick for legibility on a dark disc, not a measurement of anything). The icon is never part of this
repository (CLAUDE.md, the hard rule: no image file is committed, an icon included): the tool writes into the gitignored out/ and refuses to write
anywhere else inside the repository.

    python tools/art/make_app_icon.py [--out DIR]          (default <repository>/out/dist/icons)

WHAT IT WRITES into --out:
  icon_<size>.png       1024 512 256 192 128 64 48 32 16, each DRAWN at its own size (not shrunk from a larger one), RGBA 8-bit
  magic_portals.ico     one file with the sizes Windows asks for: 16 32 48 64 128 256, PNG-compressed entries (Vista and later)
  android/mipmap-{mdpi,hdpi,xhdpi,xxhdpi,xxxhdpi}/ic_launcher.png     48 72 96 144 192, the Android launcher set a later step reuses
  android/play_store_512.png                                          the store listing's 512
  SHA256SUMS.txt        the hash of every file above, in `sha256sum -c` form

DETERMINISTIC. The same inputs give the same bytes on one machine: the drawing uses only + - * / and square root (IEEE-exact), the PNGs carry no
timestamp and no text chunk, and zlib is run at a fixed level. (The compressed bytes can differ between zlib builds; the decoded pixels cannot.)

Windows: MAGICPORTALS_ICON=<out>/magic_portals.ico on the CMake configure of the release build embeds it (docs/packaging.md).
"""

from __future__ import annotations

import argparse
import hashlib
import math
import struct
import sys
import zlib
from pathlib import Path

# ---- the drawing -------------------------------------------------------------------------------------------------------------------------
# Everything is in units of half the canvas (1.0 reaches an edge), so one description serves every size.

DISC_RADIUS = 0.94                          # the dark disc; the rest of the canvas is transparent
DISC_CENTRE = (22.0, 30.0, 56.0)            # its colour at the middle ...
DISC_EDGE = (10.0, 14.0, 28.0)              # ... and at its edge (a radial blend by (r / R) squared)
RIM_RADIUS = 0.925                          # a faint lighter edge, so the disc stands out on a dark taskbar
RIM_HALF_WIDTH = 0.014
RIM_COLOUR = (78.0, 94.0, 140.0)
RIM_ALPHA = 0.65

BLUE = (47.0, 140.0, 255.0)                 # the outer ring
ORANGE = (255.0, 138.0, 30.0)               # the inner ring
OUTER_RADIUS = 0.72                         # the ring's middle line
INNER_RADIUS = 0.40
HALF_WIDTH = 0.09                           # half the ring's thickness
GLOW_REACH = 0.20                           # how far a ring's glow spreads past its edge
GLOW_ALPHA = 0.42

ICON_SIZES = (1024, 512, 256, 192, 128, 64, 48, 32, 16)
ICO_SIZES = (16, 32, 48, 64, 128, 256)      # ICO's directory has one byte for a side, 0 meaning 256: nothing larger fits
ANDROID_MIPMAPS = (("mdpi", 48), ("hdpi", 72), ("xhdpi", 96), ("xxhdpi", 144), ("xxxhdpi", 192))
STORE_SIZE = 512
ICO_NAME = "magic_portals.ico"


def _clamp01(value: float) -> float:
    return 0.0 if value < 0.0 else 1.0 if value > 1.0 else value


def render(size: int) -> bytes:
    """The icon at `size` x `size` as straight-alpha RGBA bytes, row by row from the top. Anti-aliased by the signed distance to each shape's
    edge, in pixels: a pixel is covered by what the edge leaves of it."""
    if size < 8:
        raise ValueError("an icon smaller than 8 pixels is not drawn")
    half = size / 2.0
    out = bytearray(size * size * 4)
    # Never thinner than a pixel and a bit, or the rings drop out at 16: the rule is in pixels, so it only acts where the canvas is small.
    half_width = max(HALF_WIDTH, 0.9 / half)
    rim_half = max(RIM_HALF_WIDTH, 0.5 / half)
    at = 0
    for y in range(size):
        py = (y + 0.5 - half) / half
        py2 = py * py
        for x in range(size):
            px = (x + 0.5 - half) / half
            r = math.sqrt(px * px + py2)
            disc = _clamp01(0.5 - (r - DISC_RADIUS) * half)
            if disc <= 0.0:
                at += 4
                continue  # transparent: the four bytes are already 0
            # premultiplied accumulation, painted back to front
            blend = r / DISC_RADIUS
            blend *= blend
            if blend > 1.0:
                blend = 1.0
            cr = DISC_CENTRE[0] + (DISC_EDGE[0] - DISC_CENTRE[0]) * blend
            cg = DISC_CENTRE[1] + (DISC_EDGE[1] - DISC_CENTRE[1]) * blend
            cb = DISC_CENTRE[2] + (DISC_EDGE[2] - DISC_CENTRE[2]) * blend
            pr, pg, pb, pa = cr, cg, cb, 1.0
            for radius, colour in ((OUTER_RADIUS, BLUE), (INNER_RADIUS, ORANGE)):
                edge = abs(r - radius) - half_width      # < 0 inside the ring, in units
                if edge < GLOW_REACH:
                    if edge > 0.0:
                        t = 1.0 - edge / GLOW_REACH
                        glow = GLOW_ALPHA * t * t
                        pr = colour[0] * glow + pr * (1.0 - glow)
                        pg = colour[1] * glow + pg * (1.0 - glow)
                        pb = colour[2] * glow + pb * (1.0 - glow)
                    cover = _clamp01(0.5 - edge * half)
                    if cover > 0.0:
                        pr = colour[0] * cover + pr * (1.0 - cover)
                        pg = colour[1] * cover + pg * (1.0 - cover)
                        pb = colour[2] * cover + pb * (1.0 - cover)
            rim = _clamp01(0.5 - (abs(r - RIM_RADIUS) - rim_half) * half) * RIM_ALPHA
            if rim > 0.0:
                pr = RIM_COLOUR[0] * rim + pr * (1.0 - rim)
                pg = RIM_COLOUR[1] * rim + pg * (1.0 - rim)
                pb = RIM_COLOUR[2] * rim + pb * (1.0 - rim)
            # The disc's own coverage is the pixel's alpha; the colour was painted as if opaque, which is straight alpha already.
            out[at] = int(pr + 0.5)
            out[at + 1] = int(pg + 0.5)
            out[at + 2] = int(pb + 0.5)
            out[at + 3] = int(disc * 255.0 + 0.5)
            at += 4
    return bytes(out)


# ---- the files ---------------------------------------------------------------------------------------------------------------------------

PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


def _chunk(kind: bytes, payload: bytes) -> bytes:
    return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF)


def png_bytes(size: int, rgba: bytes) -> bytes:
    """An RGBA 8-bit PNG: IHDR, one IDAT (every row filter 0, zlib level 9), IEND. No timestamp, no text, no colour-space chunk."""
    if len(rgba) != size * size * 4:
        raise ValueError("the pixel data is not size x size RGBA")
    stride = size * 4
    raw = bytearray()
    for y in range(size):
        raw.append(0)
        raw += rgba[y * stride:(y + 1) * stride]
    packer = zlib.compressobj(9, zlib.DEFLATED, 15, 8, zlib.Z_DEFAULT_STRATEGY)
    idat = packer.compress(bytes(raw)) + packer.flush()
    return PNG_SIGNATURE + _chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0)) + _chunk(b"IDAT", idat) + _chunk(b"IEND", b"")


def ico_bytes(images: list[tuple[int, bytes]]) -> bytes:
    """An .ico of PNG-compressed entries, smallest first. `images` is (side, png file bytes). A side of 256 is written as 0, as the format has it."""
    if not images or len(images) > 255:
        raise ValueError("an .ico holds 1 to 255 images here")
    header = struct.pack("<HHH", 0, 1, len(images))
    directory = bytearray()
    offset = 6 + 16 * len(images)
    body = bytearray()
    for side, png in sorted(images):
        if not 1 <= side <= 256:
            raise ValueError("an .ico entry is 1 to 256 pixels a side")
        directory += struct.pack("<BBBBHHII", side % 256, side % 256, 0, 0, 1, 32, len(png), offset)
        body += png
        offset += len(png)
    return header + bytes(directory) + bytes(body)


def _inside(path: Path, folder: Path) -> bool:
    try:
        path.relative_to(folder)
        return True
    except ValueError:
        return False


def check_out_folder(out: Path, repo: Path) -> None:
    """The hard rule, in code: inside the repository only its gitignored out/ may receive an image."""
    out = out.resolve()
    repo = repo.resolve()
    if _inside(out, repo) and not _inside(out, repo / "out"):
        raise SystemExit(f"refusing to write images into the tracked tree: {out} (use a folder under {repo / 'out'}, or one outside the repository)")


def build(out: Path) -> dict[str, bytes]:
    """Every file of the set, by path relative to `out`. Pure: nothing is written."""
    files: dict[str, bytes] = {}
    rendered: dict[int, bytes] = {}

    def png(size: int) -> bytes:
        if size not in rendered:
            rendered[size] = png_bytes(size, render(size))
        return rendered[size]

    for size in ICON_SIZES:
        files[f"icon_{size}.png"] = png(size)
    files[ICO_NAME] = ico_bytes([(size, png(size)) for size in ICO_SIZES])
    for density, size in ANDROID_MIPMAPS:
        files[f"android/mipmap-{density}/ic_launcher.png"] = png(size)
    files["android/play_store_512.png"] = png(STORE_SIZE)
    sums = "".join(f"{hashlib.sha256(data).hexdigest()}  {name}\n" for name, data in sorted(files.items()))
    files["SHA256SUMS.txt"] = sums.encode("ascii")
    return files


def main(argv: list[str] | None = None) -> int:
    repo = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description="Draw Magic Portals Enhanced's application icon (no input is read).")
    parser.add_argument("--out", type=Path, default=repo / "out" / "dist" / "icons", help="where to write (default: <repository>/out/dist/icons)")
    args = parser.parse_args(argv)
    check_out_folder(args.out, repo)
    files = build(args.out)
    for name, data in files.items():
        target = args.out / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
    print(f"wrote {len(files)} files into {args.out.resolve()}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
