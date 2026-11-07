"""Tests of tools/package/original_icon.py (planning step 212): the original's launcher icon as the Android set, from a SYNTHETIC reference folder.

No file of the original game is used or needed: the three source icons here are drawn by the test, in the format the original's are (RGBA, 8 bits).
"""

import os
import shutil
import struct
import sys
import tempfile
import unittest
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import original_icon as oi  # noqa: E402

REPO = os.path.dirname(os.path.dirname(HERE))


def picture(size, pixel):
    """size x size rows of RGBA bytes, pixel(x, y) -> (r, g, b, a)."""
    return [bytearray(b''.join(bytes(pixel(x, y)) for x in range(size))) for y in range(size)]


def png_bytes(size, rows, mode=0, colour=6, depth=8, interlace=0):
    """A PNG with every row filtered by the same filter `mode` (so the decoder's five filters can each be tried)."""
    channels = 4 if colour == 6 else 3
    stride = size * channels
    out = bytearray()
    previous = bytearray(stride)
    for row in rows:
        line = bytearray(row) if channels == 4 else bytearray(b''.join(bytes(row[x * 4:x * 4 + 3]) for x in range(size)))
        enc = bytearray(stride)
        for i in range(stride):
            a = line[i - channels] if i >= channels else 0
            b = previous[i]
            c = previous[i - channels] if i >= channels else 0
            if mode == 0:
                predictor = 0
            elif mode == 1:
                predictor = a
            elif mode == 2:
                predictor = b
            elif mode == 3:
                predictor = (a + b) >> 1
            else:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                predictor = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
            enc[i] = (line[i] - predictor) & 255
        out += bytes([mode]) + enc
        previous = line
    return oi.PNG_SIGNATURE + oi.chunk(b'IHDR', struct.pack('>IIBBBBB', size, size, depth, colour, 0, 0, interlace)) + oi.chunk(b'IDAT', zlib.compress(bytes(out))) + oi.chunk(b'IEND', b'')


def gradient(x, y):
    return (x * 5 % 256, y * 3 % 256, (x + y) % 256, 255)


def rounded(size):
    """A tile whose four corners are transparent, like the original's."""
    def pixel(x, y):
        corner = min(x, size - 1 - x) + min(y, size - 1 - y) < size // 8
        return (30, 120, 220, 0) if corner else (30, 120, 220, 255)
    return pixel


class Reference:
    def __init__(self, test, sources):
        self.folder = tempfile.mkdtemp(prefix='original_icon_ref_')
        test.addCleanup(shutil.rmtree, self.folder, True)
        for density, data in sources.items():
            folder = os.path.join(self.folder, 'apk_work', 'decoded', 'res', 'drawable-%s-v4' % density)
            os.makedirs(folder)
            with open(os.path.join(folder, 'icon.png'), 'wb') as f:
                f.write(data)


class TheDecoder(unittest.TestCase):
    def test_every_png_filter_is_read_back_exactly(self):
        rows = picture(16, gradient)
        for mode in range(5):
            width, height, back = oi.read_png(png_bytes(16, rows, mode=mode), 'synthetic')
            self.assertEqual((width, height), (16, 16), mode)
            self.assertEqual([bytes(r) for r in back], [bytes(r) for r in rows], 'filter %d' % mode)

    def test_an_rgb_png_gets_an_opaque_alpha(self):
        rows = picture(8, gradient)
        _, _, back = oi.read_png(png_bytes(8, rows, colour=2), 'synthetic')
        self.assertTrue(all(row[x * 4 + 3] == 255 for row in back for x in range(8)))
        self.assertEqual(bytes(back[3][:3]), bytes(rows[3][:3]))

    def test_what_it_cannot_read_is_refused_by_name(self):
        rows = picture(8, gradient)
        with self.assertRaisesRegex(oi.IconError, 'not a PNG'):
            oi.read_png(b'GIF89a' + bytes(30), 'x')
        with self.assertRaisesRegex(oi.IconError, 'depth 16'):
            oi.read_png(png_bytes(8, rows, depth=16), 'x')
        with self.assertRaisesRegex(oi.IconError, 'interlace 1'):
            oi.read_png(png_bytes(8, rows, interlace=1), 'x')
        with self.assertRaisesRegex(oi.IconError, 'colour type 0'):
            oi.read_png(png_bytes(8, rows, colour=0), 'x')


class TheResampler(unittest.TestCase):
    def test_a_flat_picture_stays_flat_with_no_ringing(self):
        rows = picture(96, lambda x, y: (10, 200, 90, 255))
        big = oi.resize(96, 96, rows, 192)
        self.assertEqual({bytes(r[x * 4:x * 4 + 4]) for r in big for x in range(192)}, {bytes((10, 200, 90, 255))})

    def test_a_transparent_corner_stays_transparent_and_does_not_bleed_its_colour(self):
        rows = picture(96, rounded(96))
        for size in (144, 192):
            big = oi.resize(96, 96, rows, size)
            self.assertEqual(bytes(big[0][0:4]), bytes((0, 0, 0, 0)), size)
            self.assertEqual(bytes(big[size // 2][(size // 2) * 4:(size // 2) * 4 + 4]), bytes((30, 120, 220, 255)), size)

    def test_the_output_is_the_asked_size_and_is_deterministic(self):
        rows = picture(96, gradient)
        a = oi.write_png(144, 144, oi.resize(96, 96, rows, 144))
        b = oi.write_png(144, 144, oi.resize(96, 96, rows, 144))
        self.assertEqual(a, b)
        self.assertEqual(oi.read_png(a, 'x')[:2], (144, 144))


class TheSet(unittest.TestCase):
    def sources(self):
        return {d: png_bytes(s, picture(s, rounded(s)), mode=4) for d, s in (('mdpi', 48), ('hdpi', 72), ('xhdpi', 96))}

    def out_folder(self):
        folder = os.path.join(REPO, 'out', 'dist', 'test_original_icon_%d' % os.getpid())
        self.addCleanup(shutil.rmtree, folder, True)
        return folder

    def test_the_three_originals_are_copied_byte_for_byte_and_the_two_larger_are_made(self):
        src = self.sources()
        ref = Reference(self, src)
        out = self.out_folder()
        written = oi.build(ref.folder, out, REPO)
        self.assertEqual([n for n in written if n.endswith('/ic_launcher.png')],
                         ['android/mipmap-%s/ic_launcher.png' % d for d in ('mdpi', 'hdpi', 'xhdpi', 'xxhdpi', 'xxxhdpi')])
        for density in ('mdpi', 'hdpi', 'xhdpi'):
            with open(os.path.join(out, 'android', 'mipmap-' + density, 'ic_launcher.png'), 'rb') as f:
                self.assertEqual(f.read(), src[density], density)
        for density, size in (('xxhdpi', 144), ('xxxhdpi', 192)):
            with open(os.path.join(out, 'android', 'mipmap-' + density, 'ic_launcher.png'), 'rb') as f:
                self.assertEqual(oi.read_png(f.read(), density)[:2], (size, size))

    def test_the_set_is_what_the_packager_asks_for(self):
        import android_package as ap
        ref = Reference(self, self.sources())
        out = self.out_folder()
        oi.build(ref.folder, out, REPO)
        icons, store = ap.make_icons(REPO, out, out)
        self.assertEqual(sorted(icons), ['hdpi', 'mdpi', 'xhdpi', 'xxhdpi', 'xxxhdpi'])
        self.assertEqual(sorted(icons['xhdpi']), ['background', 'foreground', 'legacy'], 'the adaptive icon\'s two layers come with the legacy picture (planning step 225)')
        self.assertIsNone(store)

    def test_an_output_folder_in_the_tracked_tree_is_refused_and_nothing_is_written(self):
        # A synthetic repository, so that a guard that stopped working would litter a temporary folder and not this checkout.
        ref = Reference(self, self.sources())
        repo = tempfile.mkdtemp(prefix='original_icon_repo_')
        self.addCleanup(shutil.rmtree, repo, True)
        for name in ('out', 'tools', 'docs'):
            os.makedirs(os.path.join(repo, name))
        for bad in (os.path.join(repo, 'tools', 'icons'), os.path.join(repo, 'docs'), repo, os.path.join(repo, 'out')):
            with self.assertRaises(oi.IconError, msg=bad):
                oi.build(ref.folder, bad, repo)
            self.assertFalse(os.path.exists(os.path.join(bad, 'android')), bad)
        oi.build(ref.folder, os.path.join(repo, 'out', 'dist', 'icons'), repo)  # under the repository's own out/ it is allowed
        self.assertTrue(os.path.isfile(os.path.join(repo, 'out', 'dist', 'icons', 'android', 'mipmap-xhdpi', 'ic_launcher.png')))

    def test_an_output_folder_inside_the_reference_is_refused(self):
        ref = Reference(self, self.sources())
        with self.assertRaises(oi.IconError):
            oi.build(ref.folder, os.path.join(ref.folder, 'icons'), REPO)

    def test_a_missing_or_wrong_sized_source_is_refused_by_name(self):
        src = self.sources()
        del src['hdpi']
        ref = Reference(self, src)
        with self.assertRaisesRegex(oi.IconError, 'hdpi icon is not at'):
            oi.build(ref.folder, self.out_folder(), REPO)
        src = self.sources()
        src['xhdpi'] = png_bytes(64, picture(64, gradient))
        ref = Reference(self, src)
        with self.assertRaisesRegex(oi.IconError, 'is 64 x 64, the xhdpi icon is 96 x 96'):
            oi.build(ref.folder, self.out_folder(), REPO)

    def test_the_command_says_what_went_wrong_and_exits_2(self):
        ref = Reference(self, {})
        self.assertEqual(oi.main(['--reference', ref.folder, '--out', self.out_folder()]), 2)


def tile(size=96, interior=(30, 120, 220), bezel=(200, 200, 205)):
    """The original's shape, measured on its xhdpi icon: a silver bezel 4 pixels wide at the middle of an edge, the corners rounded (transparent while x + y from
    the corner is under 8, bezel under 16 along the diagonal: the diagonal's alpha is 0 for 0..3, 91 at 4, the bezel from 5 to 7, blue from 8), a flat interior."""
    def pixel(x, y):
        edge = min(x, size - 1 - x, y, size - 1 - y)
        corner = min(x, size - 1 - x) + min(y, size - 1 - y)
        if corner < 8:
            return (0, 0, 0, 0)
        if edge < 4 or corner < 16:
            return bezel + (255,)
        return interior + (255,)
    return pixel


class TheAdaptiveLayers(unittest.TestCase):
    """Planning step 225. A launcher puts a legacy icon (a rounded tile with transparent corners) on a white plate: the owner saw white bars. The adaptive icon is a
    foreground of the tile's interior in the middle of a 108 dp canvas and a flat background, which a launcher's mask (72 dp of the 108) can never show past."""
    DENSITIES = (('mdpi', 108), ('hdpi', 162), ('xhdpi', 216), ('xxhdpi', 324), ('xxxhdpi', 432))

    def rows(self, **kw):
        return picture(96, tile(**kw))

    def decode(self, data):
        width, height, rows = oi.read_png(data, 'layer')
        self.assertEqual(width, height)
        return width, rows

    def test_the_foreground_is_a_square_of_78_dp_in_the_middle_of_a_transparent_canvas(self):
        layers = oi.adaptive_layers(self.rows())
        self.assertEqual(sorted(layers), sorted(d for d, _ in self.DENSITIES))
        for density, canvas in self.DENSITIES:
            side, rows = self.decode(layers[density][0])
            self.assertEqual(side, canvas, density)
            face = oi.face_pixels(canvas)
            self.assertEqual((canvas - face) % 2, 0, 'it centres on whole pixels: ' + density)
            self.assertAlmostEqual(face / canvas, 78 / 108, delta=2.0 / canvas, msg=density)
            offset = (canvas - face) // 2
            for y in range(canvas):
                for x in range(canvas):
                    inside = offset <= x < offset + face and offset <= y < offset + face
                    alpha = rows[y][x * 4 + 3]
                    self.assertEqual(alpha, 255 if inside else 0, '%s (%d, %d)' % (density, x, y))

    def test_the_square_is_the_interior_alone_no_bezel_and_no_corner_shows_in_it(self):
        # The bezel here is light grey and the interior blue: not one pixel of the foreground may be the bezel's colour, and every corner of the square is the interior's.
        layers = oi.adaptive_layers(self.rows())
        for density, canvas in self.DENSITIES:
            _, rows = self.decode(layers[density][0])
            face = oi.face_pixels(canvas)
            offset = (canvas - face) // 2
            seen = {bytes(rows[y][x * 4:x * 4 + 4]) for y in range(offset, offset + face) for x in range(offset, offset + face)}
            self.assertEqual(seen, {bytes((30, 120, 220, 255))}, density)

    def test_the_square_covers_every_mask_a_launcher_applies(self):
        # Android shows the middle 72 dp of the 108: a circle of that diameter, or a rounded square inside it. The square must contain the whole 72 dp.
        for density, canvas in self.DENSITIES:
            face = oi.face_pixels(canvas)
            visible = canvas * 72 / 108
            self.assertGreaterEqual(face, visible, density)
            self.assertLessEqual(face, canvas, density)

    def test_the_background_is_flat_opaque_and_the_mean_of_the_interiors_border(self):
        # Not flat: a ring of 3 pixels of one colour and a middle of another, so that the mean is the ring's alone.
        def pixel(x, y):
            base = tile()(x, y)
            if base[3] == 255 and base[:3] == (30, 120, 220) and min(x - 9, 86 - x, y - 9, 86 - y) >= oi.BORDER:
                return (250, 20, 20, 255)  # the gear
            return base
        layers = oi.adaptive_layers(picture(96, pixel))
        for density, canvas in self.DENSITIES:
            side, rows = self.decode(layers[density][1])
            self.assertEqual(side, canvas)
            self.assertEqual({bytes(r[x * 4:x * 4 + 4]) for r in rows for x in range(side)}, {bytes((30, 120, 220, 255))}, density)

    def test_the_border_colour_is_a_mean_rounded_half_up(self):
        crop = [bytearray(b'\x00\x00\x00\xff' * 10) for _ in range(10)]
        # one border pixel at 1 in the red channel of 36 border pixels (10x10 less the 4x4 middle): mean 1/36 -> 0; four of them: 4/36 -> 0; 18 of them: 0.5 -> 1
        border = [(x, y) for y in range(10) for x in range(10) if min(x, y, 9 - x, 9 - y) < oi.BORDER]
        self.assertEqual(len(border), 100 - 16)
        for x, y in border[:42]:
            crop[y][x * 4] = 1
        self.assertEqual(oi.border_colour(crop), (1, 0, 0))
        for x, y in border[:41]:
            crop[y][x * 4] = 1
        for x, y in border[41:]:
            crop[y][x * 4] = 0
        self.assertEqual(oi.border_colour(crop), (0, 0, 0))

    def test_a_tile_that_is_not_opaque_inside_the_bezel_is_refused(self):
        rows = self.rows()
        rows[40][40 * 4 + 3] = 0  # a hole in the middle of the picture
        with self.assertRaisesRegex(oi.IconError, r'not opaque at pixel \(40, 40\)'):
            oi.adaptive_layers(rows)
        rows = self.rows()
        rows[oi.INTERIOR[0]][oi.INTERIOR[0] * 4 + 3] = 128  # the corner of the interior: the bezel's rounded corner leaks in
        with self.assertRaisesRegex(oi.IconError, 'not the tile the adaptive layers were measured on'):
            oi.adaptive_layers(rows)

    def test_the_layers_are_deterministic(self):
        a = oi.adaptive_layers(self.rows())
        b = oi.adaptive_layers(self.rows())
        self.assertEqual(a, b)

    def test_the_measured_interior_clears_the_bezel_of_a_tile_shaped_like_the_originals(self):
        # tile(): bezel 4 pixels at an edge's middle and 8 along the diagonal; INTERIOR must lie wholly inside what is left.
        first, past = oi.INTERIOR
        self.assertEqual((first, past), (9, 87))
        self.assertEqual(first + past - 1, 95, 'the interior is centred on the tile: 9 from one edge, 9 from the other')
        pixel = tile()
        for y in range(first, past):
            for x in range(first, past):
                self.assertEqual(pixel(x, y), (30, 120, 220, 255), (x, y))
        self.assertGreater(first, 8, 'the bezel is 8 pixels along a diagonal in the original')

    def test_the_written_set_carries_both_layers_at_every_density_and_the_legacy_icon_unchanged(self):
        folder = tempfile.mkdtemp(prefix='original_icon_ref_')
        self.addCleanup(shutil.rmtree, folder, True)
        for density, size in (('mdpi', 48), ('hdpi', 72), ('xhdpi', 96)):
            os.makedirs(os.path.join(folder, 'apk_work', 'decoded', 'res', 'drawable-%s-v4' % density))
            with open(os.path.join(folder, 'apk_work', 'decoded', 'res', 'drawable-%s-v4' % density, 'icon.png'), 'wb') as f:
                f.write(png_bytes(size, picture(size, tile(size))))
        out = os.path.join(REPO, 'out', 'dist', 'test_original_icon_layers_%d' % os.getpid())
        self.addCleanup(shutil.rmtree, out, True)
        written = oi.build(folder, out, REPO)
        for density, canvas in self.DENSITIES:
            for name in ('ic_launcher_foreground.png', 'ic_launcher_background.png'):
                path = os.path.join(out, 'android', 'mipmap-' + density, name)
                with open(path, 'rb') as f:
                    data = f.read()
                self.assertEqual(data, written['android/mipmap-%s/%s' % (density, name)])
                self.assertEqual(oi.read_png(data, path)[:2], (canvas, canvas), path)
        self.assertEqual(len(written), 5 + 10)


if __name__ == '__main__':
    unittest.main()
