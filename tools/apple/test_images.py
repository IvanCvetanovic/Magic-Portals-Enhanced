"""tools/apple/check_png.py and tools/apple/ios_icons.py (planning step 206, PORT-8), on pictures the tests make themselves.

Run:   python -m unittest discover -s tools/apple -p "test_*.py" -v        (standard library only)

No image of anyone's is read: every PNG here is made by the code under test or by a few lines of this file, in a temporary folder, from arithmetic.
"""
import contextlib
import io
import os
import struct
import sys
import tempfile
import unittest
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import check_png  # noqa: E402
import ios_icons  # noqa: E402


def chunk(kind, payload):
    return struct.pack('>I', len(payload)) + kind + payload + struct.pack('>I', zlib.crc32(kind + payload) & 0xFFFFFFFF)


def png(width, height, colour, rows, filters=None):
    """A PNG of 8-bit samples; `rows` are lists of bytes (unfiltered), `filters` the filter type of each row (they are applied, so decoding must undo them)."""
    bpp = {0: 1, 2: 3, 4: 2, 6: 4}[colour]
    raw = bytearray()
    prev = bytes(width * bpp)
    for y, row in enumerate(rows):
        kind = (filters or [0] * height)[y]
        line = bytearray(row)
        out = bytearray()
        for i in range(len(line)):
            a = line[i - bpp] if i >= bpp else 0
            b = prev[i]
            c = prev[i - bpp] if i >= bpp else 0
            if kind == 0:
                p = 0
            elif kind == 1:
                p = a
            elif kind == 2:
                p = b
            elif kind == 3:
                p = (a + b) >> 1
            else:
                pa, pb, pc = abs(a + b - c - a), abs(a + b - c - b), abs(a + b - c - c)
                p = a if pa <= pb and pa <= pc else (b if pb <= pc else c)
            out.append((line[i] - p) & 255)
        raw.append(kind)
        raw += out
        prev = bytes(line)
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, colour, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(bytes(raw))) + chunk(b'IEND', b'')


class CheckPng(unittest.TestCase):
    def check(self, data, *args):
        with tempfile.TemporaryDirectory() as folder:
            path = os.path.join(folder, 'x.png')
            with open(path, 'wb') as f:
                f.write(data)
            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                code = check_png.main([path, *args])
            return code, out.getvalue()

    def test_a_picture_with_two_colours_passes(self):
        rows = [[0, 0, 0, 255] * 4, [200, 100, 50, 255] * 4, [0, 0, 0, 255] * 4, [0, 0, 0, 255] * 4]
        code, text = self.check(png(4, 4, 6, rows), '--size', '4x4')
        self.assertEqual(code, 0, text)
        self.assertIn('2 distinct', text)

    def test_one_flat_colour_is_nothing_drawn(self):
        rows = [[0, 0, 0] * 8] * 8
        code, text = self.check(png(8, 8, 2, rows))
        self.assertEqual(code, 1)
        self.assertIn('nothing was drawn', text)

    def test_the_size_is_checked(self):
        rows = [[0, 0, 0, 255, 9, 9, 9, 255]] * 2
        code, text = self.check(png(2, 2, 6, rows), '--size', '1280x720')
        self.assertEqual(code, 1)
        self.assertIn('not 1280x720', text)

    def test_every_row_filter_is_undone(self):
        width, height = 5, 5
        rows = [[(x * 37 + y * 11 + c * 3) & 255 for x in range(width) for c in range(3)] for y in range(height)]
        for kind in range(5):
            decoded = check_png.decode(png(width, height, 2, rows, [kind] * height))
            self.assertEqual(decoded[3], bytes(sum(rows, [])), 'filter %d' % kind)
        mixed = check_png.decode(png(width, height, 2, rows, [0, 1, 2, 3, 4]))
        self.assertEqual(mixed[3], bytes(sum(rows, [])), 'one of each in one picture')

    def test_a_file_that_is_not_a_picture_is_refused_in_words(self):
        for data, words in ((b'', 'is empty'), (b'not a png at all', 'signature'), (png(2, 2, 6, [[0] * 8] * 2)[:-30], '')):
            code, text = self.check(data)
            self.assertEqual(code, 1, text)
            self.assertIn(words, text)

    def test_a_missing_file_is_said(self):
        with contextlib.redirect_stdout(io.StringIO()) as out:
            code = check_png.main([os.path.join(tempfile.gettempdir(), 'no-such-capture-0.png')])
        self.assertEqual(code, 1)
        self.assertIn('cannot be read', out.getvalue())


class IosIcons(unittest.TestCase):
    def test_four_opaque_pictures_of_the_sizes_the_plist_names(self):
        files = ios_icons.build()
        self.assertEqual(sorted(files), sorted(['AppIcon60x60@2x.png', 'AppIcon60x60@3x.png', 'AppIcon76x76@2x~ipad.png', 'AppIcon83.5x83.5@2x~ipad.png']))
        for name, size in (('AppIcon60x60@2x.png', 120), ('AppIcon60x60@3x.png', 180), ('AppIcon76x76@2x~ipad.png', 152), ('AppIcon83.5x83.5@2x~ipad.png', 167)):
            data = files[name]
            width, height = struct.unpack('>II', data[16:24])
            self.assertEqual((width, height), (size, size), name)
            self.assertEqual(data[25], 2, name + ' is colour type 2: RGB, no alpha channel (the App Store refuses an icon with one)')
            decoded = check_png.decode(data)
            self.assertEqual(decoded[2], 3, name)
            self.assertGreater(check_png.distinct_pixels(*decoded), 50, name + ' is a picture, not a tile')

    def test_the_corners_are_the_tile_not_transparent_or_black(self):
        data = ios_icons.build()['AppIcon60x60@2x.png']
        width, height, bpp, pixels = check_png.decode(data)
        corner = tuple(pixels[0:3])
        self.assertEqual(corner, ios_icons.TILE)
        self.assertNotEqual(corner, (0, 0, 0))

    def test_it_is_deterministic_and_draws_from_arithmetic_alone(self):
        self.assertEqual(ios_icons.build(), ios_icons.build())
        with open(os.path.join(HERE, 'ios_icons.py'), encoding='utf-8') as f:
            text = f.read()
        for word in ('open(', 'read_bytes', 'read_text', 'import PIL', 'from PIL'):
            self.assertNotIn(word, text, 'it reads no file and uses no imaging library: ' + word)

    def test_it_refuses_to_write_into_the_tracked_tree(self):
        with self.assertRaises(SystemExit):
            ios_icons.main(['--out', os.path.join(ios_icons.REPO, 'docs', 'icons')])

    def test_it_writes_what_it_builds(self):
        with tempfile.TemporaryDirectory() as folder:
            self.assertEqual(ios_icons.main(['--out', folder]), 0)
            for name, data in ios_icons.build().items():
                with open(os.path.join(folder, name), 'rb') as f:
                    self.assertEqual(f.read(), data)


if __name__ == '__main__':
    unittest.main()
