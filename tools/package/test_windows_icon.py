"""Tests of tools/package/windows_icon.py (planning step 278): the original's icon as a Windows icon file and on a program, from a SYNTHETIC reference folder.

No file of the original game is used or needed. The program whose icon is changed is a COPY of the Python interpreter that runs the test; nothing is started.
"""

import os
import shutil
import struct
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import original_icon as oi  # noqa: E402
import windows_icon as wi  # noqa: E402

REPO = os.path.dirname(os.path.dirname(HERE))


def picture(size, pixel):
    return [bytearray(b''.join(bytes(pixel(x, y)) for x in range(size))) for y in range(size)]


def tile(size):
    """A tile whose four corners are transparent, its colour telling its place (so a flipped or channel-swapped copy cannot pass)."""
    def pixel(x, y):
        if min(x, size - 1 - x) + min(y, size - 1 - y) < size // 8:
            return (0, 0, 0, 0)
        return (x * 255 // size, y * 255 // size, 200, 255)
    return picture(size, pixel)


class Folders(unittest.TestCase):
    def setUp(self):
        self.root = tempfile.mkdtemp(prefix='windows_icon_')
        self.addCleanup(shutil.rmtree, self.root, True)
        self.reference = os.path.join(self.root, 'reference')
        self.sources = {48: tile(48), 96: tile(96)}
        for size, density in wi.SOURCES:
            folder = os.path.join(self.reference, 'apk_work', 'decoded', 'res', 'drawable-%s-v4' % density)
            os.makedirs(folder)
            with open(os.path.join(folder, 'icon.png'), 'wb') as f:
                f.write(oi.write_png(size, size, self.sources[size]))

    def ico(self, name='icon.ico'):
        path = os.path.join(self.root, 'made', name)
        wi.make_ico(self.reference, path, REPO)
        return path


def from_dib(data, size):
    """The rows (RGBA, top-down) and the mask bits of an icon bitmap."""
    header = struct.unpack('<IiiHH', data[:16])
    assert header == (40, size, size * 2, 1, 32), header
    rows = []
    for y in range(size):
        start = 40 + (size - 1 - y) * size * 4
        line = bytearray(data[start:start + size * 4])
        line[0::4], line[2::4] = line[2::4], line[0::4]
        rows.append(line)
    stride = (size + 31) // 32 * 4
    base = 40 + size * size * 4
    mask = [[bool(data[base + (size - 1 - y) * stride + x // 8] & (0x80 >> (x % 8))) for x in range(size)] for y in range(size)]
    assert len(data) == base + stride * size
    return rows, mask


class TheIconFile(Folders):
    def test_it_holds_every_size_in_the_form_windows_reads(self):
        with open(self.ico(), 'rb') as f:
            entries = wi.read_ico(f.read(), 'made')
        self.assertEqual([size for size, _ in entries], list(wi.SIZES))
        for size, data in entries:
            if size >= 256:
                self.assertEqual(data[:8], oi.PNG_SIGNATURE)
                self.assertEqual(oi.read_png(data, 'entry')[:2], (size, size))
            else:
                from_dib(data, size)

    def test_the_48_and_96_pictures_are_the_originals_pixels_and_their_mask_marks_the_corners(self):
        with open(self.ico(), 'rb') as f:
            entries = dict(wi.read_ico(f.read(), 'made'))
        for size in (48, 96):
            rows, mask = from_dib(entries[size], size)
            self.assertEqual(rows, self.sources[size])
            self.assertEqual(mask, [[row[x * 4 + 3] == 0 for x in range(size)] for row in self.sources[size]])
            self.assertTrue(mask[0][0])
            self.assertFalse(mask[size // 2][size // 2])

    def test_a_smaller_picture_is_the_area_it_covers_on_premultiplied_alpha(self):
        # Left half opaque red, right half transparent GREEN: the green must not bleed into the half-covered column.
        rows = picture(96, lambda x, y: (200, 0, 0, 255) if x < 48 else (0, 255, 0, 0))
        small = wi.shrink(96, rows, 3)
        for line in small:
            self.assertEqual(bytes(line), bytes((200, 0, 0, 255)) + bytes((200, 0, 0, 128)) + bytes((0, 0, 0, 0)))
        flat = wi.shrink(96, picture(96, lambda x, y: (10, 20, 30, 255)), 20)
        self.assertEqual({bytes(line) for line in flat}, {bytes((10, 20, 30, 255)) * 20})

    def test_the_same_inputs_give_the_same_file(self):
        with open(self.ico('a.ico'), 'rb') as a, open(self.ico('b.ico'), 'rb') as b:
            self.assertEqual(a.read(), b.read())

    def test_a_missing_or_wrong_source_is_named(self):
        path = os.path.join(self.reference, 'apk_work', 'decoded', 'res', 'drawable-xhdpi-v4', 'icon.png')
        with open(path, 'wb') as f:
            f.write(oi.write_png(48, 48, tile(48)))
        with self.assertRaisesRegex(wi.WindowsIconError, '48 x 48'):
            self.ico()
        os.remove(path)
        with self.assertRaisesRegex(wi.WindowsIconError, 'xhdpi icon is not at'):
            self.ico()

    def test_nothing_is_written_into_the_tracked_tree_or_over_a_file(self):
        for out in (os.path.join(REPO, 'game', 'icon.ico'), os.path.join(REPO, 'icon.ico'), os.path.join(self.reference, 'x', 'icon.ico')):
            with self.assertRaises(wi.WindowsIconError):
                wi.make_ico(self.reference, out, REPO)
            self.assertFalse(os.path.exists(out))
        made = self.ico()
        with self.assertRaisesRegex(wi.WindowsIconError, 'already there'):
            wi.make_ico(self.reference, made, REPO)

    def test_a_file_that_is_no_icon_is_refused(self):
        for data in (b'', b'\x00\x00\x02\x00\x01\x00', struct.pack('<HHH', 0, 1, 1) + struct.pack('<BBBBHHII', 16, 16, 0, 0, 1, 32, 100, 22)):
            with self.assertRaises(wi.WindowsIconError):
                wi.read_ico(data, 'broken')


@unittest.skipUnless(os.name == 'nt', 'the resource calls are Windows\'')
class OnAProgram(Folders):
    def program(self):
        path = os.path.join(self.root, 'program.exe')
        shutil.copyfile(getattr(sys, '_base_executable', sys.executable), path)
        return path

    def test_the_copy_carries_the_pictures_in_both_groups_and_everything_else_as_before(self):
        program, ico, out = self.program(), self.ico(), os.path.join(self.root, 'out', 'MagicPortals.exe')
        with open(program, 'rb') as f:
            original = f.read()
        before = wi.resources(program)
        self.assertTrue(any(kind == wi.RT_VERSION for kind, _n, _l in before), 'the stand-in program has no version resource to keep')
        wi.apply_icon(program, ico, out, REPO)
        after = wi.resources(out)
        with open(ico, 'rb') as f:
            entries = wi.read_ico(f.read(), 'ico')
        groups = {name: data for (kind, name, _l), data in after.items() if kind == wi.RT_GROUP_ICON}
        self.assertEqual(set(groups), {'GLFW_ICON', 1})
        for data in groups.values():
            self.assertEqual(data, wi.group_bytes(entries))
        icons = {name: data for (kind, name, _l), data in after.items() if kind == wi.RT_ICON}
        self.assertEqual(icons, {number: picture_ for number, (_s, picture_) in enumerate(entries, 1)})
        keep = lambda found: {key: data for key, data in found.items() if key[0] not in (wi.RT_ICON, wi.RT_GROUP_ICON)}
        self.assertEqual(keep(after), keep(before))
        with open(program, 'rb') as f:
            self.assertEqual(f.read(), original, 'the input program was changed')

    def test_a_second_apply_replaces_the_first(self):
        program, ico = self.program(), self.ico()
        once = os.path.join(self.root, 'once', 'MagicPortals.exe')
        twice = os.path.join(self.root, 'twice', 'MagicPortals.exe')
        wi.apply_icon(program, ico, once, REPO)
        wi.apply_icon(once, ico, twice, REPO)
        self.assertEqual(wi.resources(once), wi.resources(twice))

    def test_a_file_that_is_no_program_leaves_nothing(self):
        bad = os.path.join(self.root, 'bad.exe')
        with open(bad, 'wb') as f:
            f.write(b'MZ not a program')
        out = os.path.join(self.root, 'out', 'MagicPortals.exe')
        with self.assertRaises(wi.WindowsIconError):
            wi.apply_icon(bad, self.ico(), out, REPO)
        self.assertFalse(os.path.exists(out))


@unittest.skipIf(os.name == 'nt', 'what a machine that is not Windows says')
class ElseWhere(Folders):
    def test_apply_says_it_needs_windows(self):
        with self.assertRaisesRegex(wi.WindowsIconError, 'needs Windows'):
            wi.apply_icon(__file__, self.ico(), os.path.join(self.root, 'out', 'x.exe'), REPO)


if __name__ == '__main__':
    unittest.main()
