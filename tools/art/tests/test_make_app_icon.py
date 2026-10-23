"""make_app_icon.py: the icon is drawn, deterministic, a decodable PNG at every size, an .ico that parses, and written nowhere it must not be.

Standard library only (unittest), like the tool. No image is stored anywhere: every picture is made by the tool under test, in a temporary folder,
and compared with itself or with the arithmetic that describes it. Run from the repository root:

    tools/parity/.venv/Scripts/python -I -B -m unittest discover -s tools/art/tests -p test_make_app_icon.py -v
"""

from __future__ import annotations

import ast
import hashlib
import importlib.util
import math
import struct
import sys
import tempfile
import unittest
import zlib
from pathlib import Path

TOOL = Path(__file__).resolve().parents[1] / "make_app_icon.py"
_spec = importlib.util.spec_from_file_location("make_app_icon", TOOL)
icon = importlib.util.module_from_spec(_spec)
sys.modules[_spec.name] = icon
_spec.loader.exec_module(icon)


def decode_png(data: bytes) -> tuple[int, int, bytes]:
    """A small PNG decoder of the test's own (8-bit RGBA, any of the five row filters): width, height and the rows' pixels, filters undone."""
    assert data[:8] == b"\x89PNG\r\n\x1a\n", "the PNG signature"
    pos = 8
    width = height = 0
    idat = b""
    ended = False
    while pos < len(data):
        (length,) = struct.unpack_from(">I", data, pos)
        kind = data[pos + 4:pos + 8]
        payload = data[pos + 8:pos + 8 + length]
        (crc,) = struct.unpack_from(">I", data, pos + 8 + length)
        assert crc == zlib.crc32(kind + payload) & 0xFFFFFFFF, f"the {kind!r} chunk's CRC"
        if kind == b"IHDR":
            width, height, depth, colour, comp, filt, interlace = struct.unpack(">IIBBBBB", payload)
            assert (depth, colour, comp, filt, interlace) == (8, 6, 0, 0, 0), "8-bit RGBA, no interlace"
        elif kind == b"IDAT":
            idat += payload
        elif kind == b"IEND":
            ended = True
        else:
            raise AssertionError(f"an unexpected chunk {kind!r}: the files carry no text, time or colour-space chunk")
        pos += 12 + length
    assert ended and pos == len(data), "IEND is the last thing in the file"
    raw = zlib.decompress(idat)
    stride = width * 4
    assert len(raw) == height * (stride + 1), "one filter byte and one row of pixels per row"
    rows = bytearray()
    prev = bytearray(stride)
    for y in range(height):
        kind = raw[y * (stride + 1)]
        line = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for i in range(stride):
            left = line[i - 4] if i >= 4 else 0
            up = prev[i]
            upleft = prev[i - 4] if i >= 4 else 0
            if kind == 1:
                line[i] = (line[i] + left) & 255
            elif kind == 2:
                line[i] = (line[i] + up) & 255
            elif kind == 3:
                line[i] = (line[i] + ((left + up) >> 1)) & 255
            elif kind == 4:
                p = left + up - upleft
                pa, pb, pc = abs(p - left), abs(p - up), abs(p - upleft)
                pred = left if pa <= pb and pa <= pc else up if pb <= pc else upleft
                line[i] = (line[i] + pred) & 255
            else:
                assert kind == 0, "a row filter of 0 to 4"
        rows += line
        prev = line
    return width, height, bytes(rows)


def pixel(rgba: bytes, size: int, x: int, y: int) -> tuple[int, int, int, int]:
    at = (y * size + x) * 4
    return rgba[at], rgba[at + 1], rgba[at + 2], rgba[at + 3]


class RenderTest(unittest.TestCase):
    def test_same_arguments_same_bytes(self):
        for size in (16, 32, 256):
            self.assertEqual(icon.render(size), icon.render(size), f"{size}")
            self.assertEqual(icon.png_bytes(size, icon.render(size)), icon.png_bytes(size, icon.render(size)), f"png {size}")

    def test_the_shape_at_256(self):
        size = 256
        rgba = icon.render(size)
        half = size / 2

        def at(radius: float, angle: float = 0.0):
            x = int(half + radius * half * math.cos(angle))
            y = int(half + radius * half * math.sin(angle))
            return pixel(rgba, size, x, y)

        self.assertEqual(pixel(rgba, size, 0, 0)[3], 0, "the corner is transparent")
        self.assertEqual(pixel(rgba, size, size - 1, size - 1)[3], 0, "and the other one")
        self.assertEqual(at(0.0)[3], 255, "the middle is on the disc")
        r, g, b, a = at(0.0)
        self.assertLess(max(r, g, b), 70, "and dark")
        r, g, b, a = at(icon.OUTER_RADIUS)
        self.assertEqual(a, 255)
        self.assertTrue(b > 200 and b > r + 100 and b > g, f"the outer ring is blue: {(r, g, b)}")
        r, g, b, a = at(icon.INNER_RADIUS, 1.0)
        self.assertEqual(a, 255)
        self.assertTrue(r > 200 and r > b + 100 and g > b, f"the inner ring is orange: {(r, g, b)}")
        # Between the rings, and between the outer ring and the edge, it is the dark disc again.
        r, g, b, _ = at((icon.OUTER_RADIUS + icon.INNER_RADIUS) / 2)
        self.assertLess(max(r, g, b), 120, "the gap between the rings is dark")
        self.assertEqual(at(icon.DISC_RADIUS + 0.03)[3], 0, "past the disc is transparent")
        # Every edge is anti-aliased: across the disc's edge the alpha takes values other than 0 and 255.
        edge = {pixel(rgba, size, x, size // 2)[3] for x in range(size)}
        self.assertTrue(edge - {0, 255}, "the disc's edge has partly covered pixels")
        # Four-fold symmetry of the drawing (it is a function of the radius): the pixel to the left of the middle and the one above agree up to a pixel.
        self.assertEqual(pixel(rgba, size, 10, size // 2)[3], pixel(rgba, size, size // 2, 10)[3])

    def test_it_stays_legible_small(self):
        # At 16 pixels both rings are still there: a blue-ish pixel and an orange-ish pixel on one row through the middle.
        size = 16
        rgba = icon.render(size)
        row = [pixel(rgba, size, x, size // 2) for x in range(size)]
        self.assertTrue(any(b > 180 and b > r + 60 for r, g, b, a in row if a > 200), "a blue pixel")
        self.assertTrue(any(r > 180 and r > b + 90 for r, g, b, a in row if a > 200), "an orange pixel")

    def test_too_small_is_refused(self):
        with self.assertRaises(ValueError):
            icon.render(4)


class FilesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.files = icon.build(Path("unused"))

    def test_every_png_decodes_to_what_was_drawn(self):
        names = [f"icon_{s}.png" for s in icon.ICON_SIZES]
        for name in names:
            width, height, pixels = decode_png(self.files[name])
            side = int(name[5:-4])
            self.assertEqual((width, height), (side, side), name)
            if side <= 256:  # the larger ones are the same drawing at a larger size; the decode of the smaller ones is compared pixel for pixel
                self.assertEqual(pixels, icon.render(side), name)

    def test_the_set_is_the_set_the_tool_says(self):
        expect = {f"icon_{s}.png" for s in (1024, 512, 256, 192, 128, 64, 48, 32, 16)}
        expect |= {icon.ICO_NAME, "SHA256SUMS.txt", "android/play_store_512.png"}
        expect |= {f"android/mipmap-{d}/ic_launcher.png" for d in ("mdpi", "hdpi", "xhdpi", "xxhdpi", "xxxhdpi")}
        self.assertEqual(set(self.files), expect)

    def test_the_android_set_has_the_sizes_of_its_densities(self):
        sizes = {"mdpi": 48, "hdpi": 72, "xhdpi": 96, "xxhdpi": 144, "xxxhdpi": 192}
        for density, side in sizes.items():
            width, height, _ = decode_png(self.files[f"android/mipmap-{density}/ic_launcher.png"])
            self.assertEqual((width, height), (side, side), density)
        width, height, _ = decode_png(self.files["android/play_store_512.png"])
        self.assertEqual((width, height), (512, 512))
        self.assertEqual(self.files["android/play_store_512.png"], self.files["icon_512.png"])

    def test_the_ico_parses(self):
        data = self.files[icon.ICO_NAME]
        reserved, kind, count = struct.unpack_from("<HHH", data, 0)
        self.assertEqual((reserved, kind), (0, 1), "an icon file, not a cursor")
        self.assertEqual(count, 6)
        sides = []
        end = 6 + 16 * count
        for i in range(count):
            w, h, colours, res, planes, bits, size, offset = struct.unpack_from("<BBBBHHII", data, 6 + 16 * i)
            side = w or 256
            self.assertEqual(w, h, "a square entry")
            self.assertEqual((colours, res, planes, bits), (0, 0, 1, 32))
            self.assertEqual(offset, end, "entries follow one another")
            self.assertEqual(data[offset:offset + 8], b"\x89PNG\r\n\x1a\n", "PNG-compressed")
            png = data[offset:offset + size]
            width, height, pixels = decode_png(png)
            self.assertEqual((width, height), (side, side))
            self.assertEqual(png, self.files[f"icon_{side}.png"], "the entry is the same file as the PNG of its size")
            sides.append(side)
            end = offset + size
        self.assertEqual(sides, [16, 32, 48, 64, 128, 256], "smallest first; 256 is written as 0")
        self.assertEqual(end, len(data), "nothing after the last entry")
        w256 = struct.unpack_from("<B", data, 6 + 16 * 5)[0]
        self.assertEqual(w256, 0, "the directory's one byte says 256 as 0")

    def test_the_ico_is_smallest_first_whatever_order_it_is_given(self):
        small = icon.png_bytes(16, icon.render(16))
        large = icon.png_bytes(32, icon.render(32))
        data = icon.ico_bytes([(32, large), (16, small)])
        self.assertEqual((data[6], data[22]), (16, 32), "the directory lists 16 then 32")
        self.assertEqual(data[6 + 32:6 + 32 + 8], b"\x89PNG\r\n\x1a\n")
        self.assertEqual(data[6 + 32:6 + 32 + len(small)], small, "and the first entry's data is the smaller picture")
        with self.assertRaises(ValueError):
            icon.ico_bytes([])
        with self.assertRaises(ValueError):
            icon.ico_bytes([(257, small)])

    def test_the_sums_file_is_the_hash_of_each_file(self):
        lines = self.files["SHA256SUMS.txt"].decode("ascii").splitlines()
        listed = {line.split("  ", 1)[1]: line.split("  ", 1)[0] for line in lines}
        self.assertEqual(set(listed), set(self.files) - {"SHA256SUMS.txt"})
        for name, digest in listed.items():
            self.assertEqual(hashlib.sha256(self.files[name]).hexdigest(), digest, name)

    def test_two_runs_write_the_same_bytes(self):
        with tempfile.TemporaryDirectory() as one, tempfile.TemporaryDirectory() as two:
            # Outside the repository, so the guard allows it; the two folders are the two runs.
            self.assertEqual(icon.main(["--out", one]), 0)
            self.assertEqual(icon.main(["--out", two]), 0)
            names_one = sorted(p.relative_to(one).as_posix() for p in Path(one).rglob("*") if p.is_file())
            names_two = sorted(p.relative_to(two).as_posix() for p in Path(two).rglob("*") if p.is_file())
            self.assertEqual(names_one, names_two)
            self.assertEqual(len(names_one), 17)
            for name in names_one:
                self.assertEqual((Path(one) / name).read_bytes(), (Path(two) / name).read_bytes(), name)
                self.assertEqual((Path(one) / name).read_bytes(), self.files[name], f"{name}: the files written are the files built")


class RulesTest(unittest.TestCase):
    def test_the_tracked_tree_is_refused(self):
        repo = TOOL.parents[2]
        for folder in (repo / "tools" / "art", repo / "game", repo, repo / "docs" / "icons"):
            with self.assertRaises(SystemExit, msg=str(folder)):
                icon.check_out_folder(folder, repo)

    def test_out_and_outside_are_allowed(self):
        repo = TOOL.parents[2]
        icon.check_out_folder(repo / "out" / "dist" / "icons", repo)
        icon.check_out_folder(repo / "out", repo)
        with tempfile.TemporaryDirectory() as elsewhere:
            icon.check_out_folder(Path(elsewhere), repo)

    def test_only_the_standard_library_is_imported(self):
        allowed = {"__future__", "argparse", "hashlib", "math", "struct", "sys", "zlib", "pathlib"}
        tree = ast.parse(TOOL.read_text(encoding="utf-8"))
        imported = set()
        for node in ast.walk(tree):
            if isinstance(node, ast.Import):
                imported |= {a.name.split(".")[0] for a in node.names}
            elif isinstance(node, ast.ImportFrom):
                imported.add((node.module or "").split(".")[0])
        self.assertTrue(imported <= allowed, f"imports beyond the standard library's few: {imported - allowed}")

    def test_the_tool_opens_no_file_to_read(self):
        # Nothing is read: the only file operations are the writes in main(). (A read of an original file would be a call to open, read_bytes or
        # read_text, or a numpy or PIL import, which the test above refuses.)
        source = TOOL.read_text(encoding="utf-8")
        for call in ("open(", ".read_bytes(", ".read_text(", ".read("):
            self.assertNotIn(call, source, f"the tool must not read a file: {call}")


if __name__ == "__main__":
    unittest.main()
