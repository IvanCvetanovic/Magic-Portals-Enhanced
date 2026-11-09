"""Tests that every font face committed in game/data/fonts is exactly the one its README pins (planning step 237, review fix F5).

Run:   python -m unittest discover -s tools/ci -p "test_*.py" -v           (standard library only; reads game/data/fonts)

WHY. tools/ci/check_public.py lets a `game/data/fonts/<Family>-<Style>.ttf` of at most 400,000 bytes through its rules H1 (no binary extensions) and H2 (no binary
files) when an `OFL-<Family>.txt` is beside it. That is a DELIBERATE WIDENING of the hard rule's mechanical check (no extracted asset and no Asantee-origin file is ever
committed), and the check cannot tell an OFL font from a Windows font or an Asantee one: a file only has to be NAMED like a face. The widening is accepted only because THIS
test closes the hole from the other side: the README of that folder pins each face by size and sha256 (a table that tools/l10n/make_fonts.py writes from the upstream
files it checks against hashes of its own), and this test fails for a .ttf that the table does not list, for one whose bytes are not the pinned ones, for a pinned face that is
missing, and for a face with no licence text. A font swapped in under a pinned name changes its hash; a font added under a new name is unlisted; and the table itself
is reviewed in a diff of a README, where a font of Microsoft's or Asantee's would have to be written down as a source. The owner must accept the widening (docs/languages.md).

WHAT IT PROVES. The faces in the working tree are the ones the README lists, byte for byte. WHAT IT CANNOT PROVE. That the listed source is what the file is: the sha256 of the
upstream file and the licence are claims of that table, checked by make_fonts.py against hashes it carries, and the committed README is what a reviewer reads.
"""
import hashlib
import os
import re
import shutil
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
FONTS = os.path.join(REPO, 'game', 'data', 'fonts')

# Every kind of font file there is: only the .ttf of the folder is exempt in check_public.py, and a font of another kind here is refused outright.
FONT_EXTENSIONS = ('.ttf', '.otf', '.ttc', '.woff', '.woff2', '.eot', '.pfb', '.fon')
# A row of the generated table: | `File.ttf` | source ... | made ...; 328,820 bytes, sha256 `<64 hex>`; ... | licence |  - the SECOND sha256 of the row, the committed file's own.
ROW = re.compile(r'^\| `(?P<file>[A-Za-z0-9]+-[A-Za-z]+\.ttf)` \|.*?(?P<size>\d[\d,]*) bytes, sha256 `(?P<sha>[0-9a-f]{64})`', re.MULTILINE)
FAMILY = re.compile(r'^(?P<family>[A-Za-z0-9]+)-[A-Za-z]+\.ttf$')


def sha256_of(path):
    with open(path, 'rb') as handle:
        return hashlib.sha256(handle.read()).hexdigest()


def pinned_table(readme_text):
    """{file: (size, sha256)} of the README's table of faces."""
    return {m.group('file'): (int(m.group('size').replace(',', '')), m.group('sha')) for m in ROW.finditer(readme_text)}


def problems(fonts_dir):
    """What is wrong with the font faces of `fonts_dir`, one line each (empty: every face is pinned, whole, and has its licence text)."""
    out = []
    readme = os.path.join(fonts_dir, 'README.md')
    if not os.path.isfile(readme):
        return ['%s has no README.md to pin the faces' % fonts_dir]
    with open(readme, encoding='utf-8') as handle:
        table = pinned_table(handle.read())
    if not table:
        out.append('README.md pins no face: its table has no row of the shape | `Family-Style.ttf` | ... bytes, sha256 `...` |')
    present = []
    for root, _, names in os.walk(fonts_dir):
        for name in names:
            if name.lower().endswith(FONT_EXTENSIONS):
                present.append(os.path.relpath(os.path.join(root, name), fonts_dir).replace(os.sep, '/'))
    for rel in sorted(present):
        if rel not in table:
            out.append('%s is a font file the README does not list: only the pinned faces may be committed' % rel)
            continue
        path = os.path.join(fonts_dir, rel)
        size, sha = table[rel]
        if os.path.getsize(path) != size:
            out.append('%s is %d bytes, the README pins %d' % (rel, os.path.getsize(path), size))
        elif sha256_of(path) != sha:
            out.append('%s has sha256 %s, the README pins %s' % (rel, sha256_of(path), sha))
        family = FAMILY.match(rel)
        licence = 'OFL-%s.txt' % family.group('family') if family else None
        if licence is None or not os.path.isfile(os.path.join(fonts_dir, licence)):
            out.append('%s has no SIL OFL text %s beside it' % (rel, licence))
    for rel in sorted(table):
        if rel not in present:
            out.append('%s is pinned by the README and is not there' % rel)
    return out


class TheCommittedFaces(unittest.TestCase):
    def test_the_real_folder_is_exactly_what_its_readme_pins(self):
        self.assertEqual(problems(FONTS), [])

    def test_the_table_pins_the_four_faces_the_languages_json_names(self):
        with open(os.path.join(FONTS, 'README.md'), encoding='utf-8') as handle:
            table = pinned_table(handle.read())
        self.assertEqual(sorted(table), ['Kurale-Regular.ttf', 'NotoSans-Bold.ttf', 'NotoSansArabic-Bold.ttf', 'NotoSansJP-Bold.ttf'])
        for size, sha in table.values():
            self.assertTrue(0 < size <= 400 * 1000, 'check_public.py lets a face of at most 400,000 bytes through; a pinned one is within it')
            self.assertEqual(len(sha), 64)


class TheCheckCanFail(unittest.TestCase):
    """The same check on a copy of the folder with one thing changed: each change is found, and the unchanged copy is not."""

    def setUp(self):
        self.scratch = tempfile.mkdtemp(prefix='fonts-pinned-')
        self.copy = os.path.join(self.scratch, 'fonts')
        shutil.copytree(FONTS, self.copy)

    def tearDown(self):
        shutil.rmtree(self.scratch, ignore_errors=True)

    def read_readme(self):
        with open(os.path.join(self.copy, 'README.md'), encoding='utf-8', newline='') as handle:
            return handle.read()

    def write_readme(self, text):
        with open(os.path.join(self.copy, 'README.md'), 'w', encoding='utf-8', newline='') as handle:
            handle.write(text)

    def test_an_unchanged_copy_is_clean(self):
        self.assertEqual(problems(self.copy), [])

    def test_a_face_with_one_byte_changed_is_found(self):
        path = os.path.join(self.copy, 'NotoSans-Bold.ttf')
        with open(path, 'rb') as handle:
            data = bytearray(handle.read())
        data[len(data) // 2] ^= 0x01
        with open(path, 'wb') as handle:
            handle.write(bytes(data))
        found = problems(self.copy)
        self.assertEqual(len(found), 1, found)
        self.assertIn('NotoSans-Bold.ttf has sha256', found[0])

    def test_a_face_of_another_size_is_found(self):
        path = os.path.join(self.copy, 'Kurale-Regular.ttf')
        with open(path, 'ab') as handle:
            handle.write(b'\0')
        found = problems(self.copy)
        self.assertEqual(len(found), 1, found)
        self.assertIn('bytes, the README pins', found[0])

    def test_an_unlisted_face_is_refused_whatever_its_name_and_licence(self):
        # Named like a face, with a licence text beside it: exactly what check_public.py would let through.
        shutil.copyfile(os.path.join(self.copy, 'Kurale-Regular.ttf'), os.path.join(self.copy, 'Verdana-Bold.ttf'))
        shutil.copyfile(os.path.join(self.copy, 'OFL-Kurale.txt'), os.path.join(self.copy, 'OFL-Verdana.txt'))
        found = problems(self.copy)
        self.assertEqual(len(found), 1, found)
        self.assertIn('Verdana-Bold.ttf is a font file the README does not list', found[0])

    def test_a_font_of_another_kind_or_in_a_sub_folder_is_refused(self):
        for rel in ('Kurale-Regular.otf', 'Matura.woff2', os.path.join('sub', 'Kurale-Regular.ttf')):
            target = os.path.join(self.copy, rel)
            os.makedirs(os.path.dirname(target), exist_ok=True)
            shutil.copyfile(os.path.join(self.copy, 'Kurale-Regular.ttf'), target)
            found = problems(self.copy)
            self.assertTrue(any('does not list' in line for line in found), (rel, found))
            os.remove(target)

    def test_a_face_whose_row_was_dropped_from_the_readme_is_unlisted(self):
        text = self.read_readme()
        kept = [line for line in text.splitlines(keepends=True) if not line.startswith('| `NotoSansJP-Bold.ttf`')]
        self.assertEqual(len(kept), len(text.splitlines(keepends=True)) - 1)
        self.write_readme(''.join(kept))
        found = problems(self.copy)
        self.assertEqual(len(found), 1, found)
        self.assertIn('NotoSansJP-Bold.ttf is a font file the README does not list', found[0])

    def test_a_pin_that_was_edited_is_found(self):
        text = self.read_readme()
        sha = pinned_table(text)['NotoSansJP-Bold.ttf'][1]
        self.write_readme(text.replace(sha, '0' * 64))
        found = problems(self.copy)
        self.assertEqual(len(found), 1, found)
        self.assertIn('NotoSansJP-Bold.ttf has sha256', found[0])

    def test_a_pinned_face_that_is_gone_is_found(self):
        os.remove(os.path.join(self.copy, 'NotoSansArabic-Bold.ttf'))
        found = problems(self.copy)
        self.assertTrue(any('is pinned by the README and is not there' in line for line in found), found)

    def test_a_face_without_its_licence_text_is_found(self):
        os.remove(os.path.join(self.copy, 'OFL-NotoSansJP.txt'))
        found = problems(self.copy)
        self.assertEqual(len(found), 1, found)
        self.assertIn('OFL-NotoSansJP.txt', found[0])

    def test_no_readme_and_an_empty_table_are_findings(self):
        os.remove(os.path.join(self.copy, 'README.md'))
        self.assertTrue(problems(self.copy))
        self.write_readme('# nothing pinned\n')
        self.assertTrue(any('pins no face' in line for line in problems(self.copy)))


if __name__ == '__main__':
    unittest.main()
