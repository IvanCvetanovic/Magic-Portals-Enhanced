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
        self.assertEqual(list(written), ['mdpi', 'hdpi', 'xhdpi', 'xxhdpi', 'xxxhdpi'])
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


if __name__ == '__main__':
    unittest.main()
