#!/usr/bin/env python3
"""Whether a capture the game wrote is a picture of something (planning step 206, PORT-8): a PNG of the size asked for that is not one flat colour.

    python tools/apple/check_png.py FILE [--size WxH] [--min-size WxH] [--min-colours N]

Why: a run of the fixture level on a runner that nobody watches ends with an exit code and a file, and a file's existing says little: a swapchain that drew
nothing writes a black picture as readily as a level. This decodes the PNG (8-bit grey, grey with alpha, RGB or RGBA, not interlaced, all five row filters: what
the engine's writer produces) and says how many distinct pixel values it holds. Exit 0: it is the size asked for and holds at least --min-colours (default 2)
distinct values; exit 1 says which part failed. It cannot say the picture is RIGHT: only that something was drawn.
--min-size is for a window the system may make smaller than asked (macOS keeps a window on the screen: the first run on GitHub's macOS runner asked for 1280x720 and drew 1024x653).

Standard library only; reads the one file it is given.
"""

from __future__ import annotations

import argparse
import struct
import sys
import zlib
from pathlib import Path

CHANNELS = {0: 1, 2: 3, 4: 2, 6: 4}  # colour type -> samples per pixel


class NotAPng(Exception):
    pass


def decode(data: bytes) -> tuple[int, int, int, bytes]:
    """(width, height, bytes per pixel, the rows' pixels with every filter undone). Raises NotAPng with the reason."""
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise NotAPng("not a PNG: the signature is wrong")
    pos = 8
    header = None
    idat = bytearray()
    while pos + 8 <= len(data):
        (length,) = struct.unpack_from(">I", data, pos)
        kind = data[pos + 4:pos + 8]
        payload = data[pos + 8:pos + 8 + length]
        if len(payload) != length:
            raise NotAPng("the file ends inside a chunk")
        if kind == b"IHDR":
            header = struct.unpack(">IIBBBBB", payload)
        elif kind == b"IDAT":
            idat += payload
        elif kind == b"IEND":
            break
        pos += 12 + length
    if header is None:
        raise NotAPng("no IHDR chunk")
    width, height, depth, colour, _compression, _filter, interlace = header
    if depth != 8 or colour not in CHANNELS or interlace != 0:
        raise NotAPng(f"unsupported PNG (bit depth {depth}, colour type {colour}, interlace {interlace}): this reads 8-bit, not interlaced")
    bpp = CHANNELS[colour]
    stride = width * bpp
    try:
        raw = zlib.decompress(bytes(idat))
    except zlib.error as error:
        raise NotAPng(f"the picture's data does not decompress: {error}") from error
    if len(raw) != height * (stride + 1):
        raise NotAPng(f"the picture's data is {len(raw)} bytes, {height * (stride + 1)} expected")
    out = bytearray(height * stride)
    prev = bytearray(stride)
    for y in range(height):
        kind = raw[y * (stride + 1)]
        line = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        if kind == 0:
            pass
        elif kind == 1:
            for i in range(bpp, stride):
                line[i] = (line[i] + line[i - bpp]) & 255
        elif kind == 2:
            for i in range(stride):
                line[i] = (line[i] + prev[i]) & 255
        elif kind == 3:
            for i in range(stride):
                left = line[i - bpp] if i >= bpp else 0
                line[i] = (line[i] + ((left + prev[i]) >> 1)) & 255
        elif kind == 4:
            for i in range(stride):
                a = line[i - bpp] if i >= bpp else 0
                b = prev[i]
                c = prev[i - bpp] if i >= bpp else 0
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                predictor = a if pa <= pb and pa <= pc else (b if pb <= pc else c)
                line[i] = (line[i] + predictor) & 255
        else:
            raise NotAPng(f"row {y} has the filter type {kind}, which PNG does not have")
        out[y * stride:(y + 1) * stride] = line
        prev = line
    return width, height, bpp, bytes(out)


def distinct_pixels(width: int, height: int, bpp: int, pixels: bytes) -> int:
    seen = set()
    for i in range(0, width * height * bpp, bpp):
        seen.add(pixels[i:i + bpp])
        if len(seen) > 4096:
            break  # far past any threshold a caller sets
    return len(seen)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Check that a capture is a picture of the size asked for and not one flat colour.")
    parser.add_argument("file", type=Path)
    parser.add_argument("--size", help="WxH the picture must be")
    parser.add_argument("--min-size", help="WxH the picture must be at least as wide and as high as")
    parser.add_argument("--min-colours", type=int, default=2, help="the fewest distinct pixel values it may hold (default 2)")
    args = parser.parse_args(argv)
    try:
        data = args.file.read_bytes()
    except OSError as error:
        print(f"{args.file}: cannot be read: {error}")
        return 1
    if not data:
        print(f"{args.file}: is empty")
        return 1
    try:
        width, height, bpp, pixels = decode(data)
    except NotAPng as error:
        print(f"{args.file}: {error}")
        return 1
    colours = distinct_pixels(width, height, bpp, pixels)
    print(f"{args.file}: {width}x{height}, {colours} distinct pixel value(s){'+' if colours > 4096 else ''}, {len(data)} bytes")
    if args.size and args.size != f"{width}x{height}":
        print(f"the size is {width}x{height}, not {args.size}")
        return 1
    if args.min_size:
        try:
            least_width, least_height = (int(part) for part in args.min_size.split("x"))
        except ValueError:
            parser.error(f"--min-size wants WxH, not {args.min_size!r}")
        if width < least_width or height < least_height:
            print(f"the size is {width}x{height}, smaller than the {args.min_size} asked for")
            return 1
    if colours < args.min_colours:
        print(f"the picture holds {colours} distinct value(s), fewer than the {args.min_colours} asked for: nothing was drawn")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
