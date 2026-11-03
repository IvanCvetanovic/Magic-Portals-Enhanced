#!/usr/bin/env python3
"""The original game's launcher icon as the Android icon set of a release (planning step 212).

The first release's APK carried an icon this project drew itself (tools/art/make_app_icon.py: two rings on a dark disc), and the owner asked for the
original's: a blue sunburst tile with the dark portal gear in its middle. The original's icon is a file of the original game, so it is the owner's data
and never part of this repository (CLAUDE.md, the hard rule): this tool reads it from the owner's own reference folder, writes the set into a folder
under out/ (or outside the repository), and refuses to write anywhere else inside the repository. The packager takes the set with --icons
(tools/build_android.sh --icons <folder>).

    python tools/package/original_icon.py --reference <reference folder> --out <folder> [--repo <this repository>]

WHAT IT READS: <reference>/apk_work/decoded/res/drawable-{mdpi,hdpi,xhdpi}-v4/icon.png, the original APK's own launcher icons at 48, 72 and 96 pixels
(RGBA, 8 bits). WHAT IT WRITES: android/mipmap-{mdpi,hdpi,xhdpi,xxhdpi,xxxhdpi}/ic_launcher.png. The first three are the original's files, BYTE FOR
BYTE. The original has nothing larger than 96 pixels, so xxhdpi (144) and xxxhdpi (192) are made from the 96 by bicubic (Catmull-Rom) resampling
on premultiplied alpha: they are as sharp as 96 pixels allow and no sharper. Standard library only; the same inputs give the same bytes.
"""

from __future__ import annotations

import argparse
import os
import struct
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import package as pk  # noqa: E402  (the content packager's output-folder rule)

PNG_SIGNATURE = b'\x89PNG\r\n\x1a\n'
# density -> (size in pixels, where the original keeps it or None when it is made from the largest)
SET = (('mdpi', 48, 'mdpi'), ('hdpi', 72, 'hdpi'), ('xhdpi', 96, 'xhdpi'), ('xxhdpi', 144, None), ('xxxhdpi', 192, None))
SOURCE_FOLDER = 'drawable-%s-v4'
SOURCE_NAME = 'icon.png'


class IconError(Exception):
    pass


def read_png(data, where):
    """(width, height, rows of RGBA bytes) of an 8-bit RGBA or RGB PNG, not interlaced. Anything else is refused by name."""
    if data[:8] != PNG_SIGNATURE:
        raise IconError('%s is not a PNG' % where)
    pos = 8
    ihdr = None
    idat = []
    while pos + 8 <= len(data):
        length, kind = struct.unpack('>I4s', data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if kind == b'IHDR':
            ihdr = struct.unpack('>IIBBBBB', body)
        elif kind == b'IDAT':
            idat.append(body)
        elif kind == b'IEND':
            break
    if ihdr is None or not idat:
        raise IconError('%s has no image data' % where)
    width, height, depth, colour, _compression, _filter, interlace = ihdr
    if depth != 8 or colour not in (2, 6) or interlace != 0:
        raise IconError('%s is not an 8-bit RGB or RGBA PNG without interlacing (depth %d, colour type %d, interlace %d)' % (where, depth, colour, interlace))
    channels = 4 if colour == 6 else 3
    stride = width * channels
    raw = zlib.decompress(b''.join(idat))
    if len(raw) != height * (stride + 1):
        raise IconError('%s: the image data is %d bytes, %d were expected' % (where, len(raw), height * (stride + 1)))
    rows = []
    previous = bytearray(stride)
    for y in range(height):
        base = y * (stride + 1)
        mode = raw[base]
        line = bytearray(raw[base + 1:base + 1 + stride])
        if mode == 1:
            for i in range(channels, stride):
                line[i] = (line[i] + line[i - channels]) & 255
        elif mode == 2:
            for i in range(stride):
                line[i] = (line[i] + previous[i]) & 255
        elif mode == 3:
            for i in range(stride):
                left = line[i - channels] if i >= channels else 0
                line[i] = (line[i] + ((left + previous[i]) >> 1)) & 255
        elif mode == 4:
            for i in range(stride):
                a = line[i - channels] if i >= channels else 0
                b = previous[i]
                c = previous[i - channels] if i >= channels else 0
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                predictor = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[i] = (line[i] + predictor) & 255
        elif mode != 0:
            raise IconError('%s: filter type %d is not a PNG filter' % (where, mode))
        if channels == 3:
            rgba = bytearray(width * 4)
            for x in range(width):
                rgba[x * 4:x * 4 + 3] = line[x * 3:x * 3 + 3]
                rgba[x * 4 + 3] = 255
            rows.append(rgba)
        else:
            rows.append(line)
        previous = line
    return width, height, rows


def chunk(kind, body):
    return struct.pack('>I', len(body)) + kind + body + struct.pack('>I', zlib.crc32(kind + body) & 0xFFFFFFFF)


def write_png(width, height, rows):
    """An RGBA 8-bit PNG: no text chunk, no timestamp, zlib at a fixed level."""
    raw = b''.join(b'\x00' + bytes(row) for row in rows)
    return PNG_SIGNATURE + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b'')


def catmull_rom(t):
    """The cubic's four weights at fraction t between sample 1 and sample 2 (a = -0.5)."""
    t2, t3 = t * t, t * t * t
    return (-0.5 * t3 + t2 - 0.5 * t, 1.5 * t3 - 2.5 * t2 + 1.0, -1.5 * t3 + 2.0 * t2 + 0.5 * t, 0.5 * t3 - 0.5 * t2)


def resize(width, height, rows, size):
    """Rows of the picture at size x size, by Catmull-Rom on premultiplied alpha (so a transparent corner does not bleed its colour into the edge), edges clamped."""
    pre = []
    for row in rows:
        out = []
        for x in range(width):
            r, g, b, a = row[x * 4:x * 4 + 4]
            out.append((r * a / 255.0, g * a / 255.0, b * a / 255.0, float(a)))
        pre.append(out)

    def taps(destination, source_size, dest_size):
        position = (destination + 0.5) * source_size / dest_size - 0.5
        base = int(position // 1)
        weights = catmull_rom(position - base)
        return [(min(max(base - 1 + k, 0), source_size - 1), weights[k]) for k in range(4)]

    horizontal = [taps(x, width, size) for x in range(size)]
    vertical = [taps(y, height, size) for y in range(size)]
    wide = []
    for row in pre:
        wide.append([tuple(sum(row[sx][c] * w for sx, w in horizontal[x]) for c in range(4)) for x in range(size)])
    result = []
    for y in range(size):
        line = bytearray(size * 4)
        for x in range(size):
            r, g, b, a = (sum(wide[sy][x][c] * w for sy, w in vertical[y]) for c in range(4))
            a = min(max(a, 0.0), 255.0)
            if a > 0.0:
                r, g, b = (min(max(v * 255.0 / a, 0.0), 255.0) for v in (r, g, b))
            else:
                r = g = b = 0.0
            line[x * 4:x * 4 + 4] = bytes((int(round(r)), int(round(g)), int(round(b)), int(round(a))))
        result.append(line)
    return result


def build(reference, out, repo):
    reference = os.path.abspath(reference)
    out = os.path.abspath(out)
    repo = os.path.abspath(repo)
    try:
        pk.check_output(out, repo, reference)
    except pk.PackageError as e:
        raise IconError(str(e))
    sources = {}
    for density, size, source in SET:
        if source is None:
            continue
        path = os.path.join(reference, 'apk_work', 'decoded', 'res', SOURCE_FOLDER % source, SOURCE_NAME)
        if not os.path.isfile(path):
            raise IconError('the original\'s %s icon is not at %s (the reference folder is the one README, Local setup after cloning, makes)' % (density, path))
        with open(path, 'rb') as f:
            data = f.read()
        width, height, rows = read_png(data, path)
        if (width, height) != (size, size):
            raise IconError('%s is %d x %d, the %s icon is %d x %d' % (path, width, height, density, size, size))
        sources[density] = (data, rows)
    largest = sources['xhdpi'][1]
    written = {}
    for density, size, source in SET:
        if source is not None:
            data = sources[density][0]
        else:
            data = write_png(size, size, resize(96, 96, largest, size))
        written[density] = data
    os.makedirs(out, exist_ok=True)
    for density, data in written.items():
        folder = os.path.join(out, 'android', 'mipmap-' + density)
        os.makedirs(folder, exist_ok=True)
        with open(os.path.join(folder, 'ic_launcher.png'), 'wb') as f:
            f.write(data)
    return written


def main(argv=None):
    parser = argparse.ArgumentParser(description='The original game\'s launcher icon as the Android icon set of a release (the owner\'s data: never committed).')
    parser.add_argument('--reference', required=True, help='the reference folder (it holds apk_work/decoded/res)')
    parser.add_argument('--out', required=True, help='where the set goes (under the repository\'s out/, or outside the repository)')
    parser.add_argument('--repo', default=os.path.dirname(os.path.dirname(HERE)), help='this repository (default: the one this file is in)')
    args = parser.parse_args(argv)
    try:
        written = build(args.reference, args.out, args.repo)
    except IconError as e:
        print('original_icon: %s' % e, file=sys.stderr)
        return 2
    for density, data in written.items():
        print('android/mipmap-%s/ic_launcher.png: %d bytes' % (density, len(data)))
    print('written to %s' % os.path.abspath(args.out))
    return 0


if __name__ == '__main__':
    sys.exit(main())
