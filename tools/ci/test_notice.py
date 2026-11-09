"""Tests of NOTICE.md and LICENSE (planning step 210, PORT-12): the licence texts in NOTICE.md are the texts of the files they say they come from.

Run:   python -m unittest discover -s tools/ci -p "test_*.py" -v           (standard library and git; reads the files of the engine submodule, and skips, saying so, where it is not checked out or git is absent)

WHAT THIS PROVES. Each licence text in NOTICE.md is announced by a comment `<!-- verbatim: <path> [first-last] -->` and a fenced block; the text of the block is compared, line by line,
with the whole file or with those lines (1-based, inclusive) of the file in the engine's tree, comment marks and a common indent removed on both sides. So a licence text cannot be
mistyped, and a move of the engine pin that changes a licence file turns this test red until NOTICE.md is brought up to date. Every component in the engine's own list
(engine/THIRD_PARTY_LICENSES.md) is in NOTICE.md's table. LICENSE is the engine's licence text, and a clone made with core.autocrlf=true (a Windows machine) still has LICENSE and NOTICE.md with Unix line ends. NOTICE.md stands alone: it links to no file of the repository.
WHAT IT CANNOT PROVE. That the table says the right thing about a program it was not made from (the table says which entries are from a link line and which are from the engine's list);
the blocks that come from outside the repository (the NDK, the GCC runtime exception, AngelScript's header, Ethanon's source) are named `external:` and are not compared with a file, because those files are not in a clone (the GCC exception is pinned by its parts).
The licence texts of the fonts this repository ships (game/data/fonts/OFL-<Name>.txt, planning step 237) are compared with those files, which are in a clone; no other path of the repository is a source.
"""
import os
import re
import shutil
import subprocess
import tempfile
import textwrap
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
ENGINE = os.path.join(REPO, 'engine')
# git out of the way of the user's configuration, as tools/ci/test_check_public.py has it
GIT_ENV = dict(os.environ, GIT_CONFIG_GLOBAL=os.devnull, GIT_CONFIG_NOSYSTEM='1')

MARKER = re.compile(r'^<!-- verbatim: (?P<path>.+?)(?: (?P<first>\d+)-(?P<last>\d+))? -->$')
# The licence text of a font this repository ships (planning step 237): game/data/fonts/OFL-<Name>.txt, compared with the file in the clone. Exactly that folder and that
# name, and no other path of the repository: a block may not point at a file the tests have no reason to trust as a licence text.
FONT_LICENCE = re.compile(r'^game/data/fonts/OFL-[A-Za-z]+\.txt$')


def source_of(path):
    """Where a verbatim block's text is compared with: 'engine' (a file of the engine's tree), 'font' (a font licence of this repository), 'external' (not in a clone: not compared),
    or None (a path nothing may be compared with: the block is refused)."""
    if path.startswith('external:'):
        return 'external'
    if path.startswith('engine/') and '..' not in path.split('/'):
        return 'engine'
    if FONT_LICENCE.match(path):
        return 'font'
    return None


def read(path):
    with open(path, encoding='utf-8-sig', newline='') as f:
        return f.read().replace('\r\n', '\n')


def normal(lines):
    """The lines with trailing white space removed, a comment mark or the common indent taken off, and blank lines at either end dropped."""
    lines = [l.rstrip() for l in lines]
    body = [l for l in lines if l]
    if body and all(l.startswith('//') for l in body):
        lines = [(l[3:] if l.startswith('// ') else l[2:]) if l else l for l in lines]
    lines = textwrap.dedent('\n'.join(lines)).split('\n')
    while lines and not lines[0].strip():
        lines.pop(0)
    while lines and not lines[-1].strip():
        lines.pop()
    return [l.rstrip() for l in lines]


def blocks(text):
    """[(line number of the marker, path, first, last, [lines of the fenced block])] of NOTICE.md."""
    lines = text.split('\n')
    found = []
    for n, line in enumerate(lines):
        m = MARKER.match(line)
        if not m:
            continue
        if n + 1 >= len(lines) or not lines[n + 1].startswith('```'):
            found.append((n + 1, m.group('path'), None, None, None))
            continue
        end = n + 2
        while not lines[end].startswith('```'):
            end += 1
        first = int(m.group('first')) if m.group('first') else None
        last = int(m.group('last')) if m.group('last') else None
        found.append((n + 1, m.group('path'), first, last, lines[n + 2:end]))
    return found


class TheLicence(unittest.TestCase):
    def test_license_is_the_mit_no_attribution_text_of_the_engine_with_its_holder_and_year(self):
        text = read(os.path.join(REPO, 'LICENSE'))
        self.assertTrue(text.startswith('MIT No Attribution\n\nCopyright (c) 2026 Ivan Cvetanovic\n\nPermission is hereby granted, free of charge,'), text[:120])
        self.assertTrue(text.rstrip().endswith('SOFTWARE.'))
        engine_licence = os.path.join(ENGINE, 'LICENSE')
        if not os.path.isfile(engine_licence):
            self.skipTest('the engine is not checked out: LICENSE was compared with nothing (git submodule update --init)')
        self.assertEqual(text, read(engine_licence), "LICENSE differs from the engine's licence text")

    def test_license_is_a_plain_text_file_with_unix_line_ends(self):
        with open(os.path.join(REPO, 'LICENSE'), 'rb') as f:
            data = f.read()
        self.assertNotIn(b'\r', data)
        self.assertTrue(data.endswith(b'\n'))

    def test_gitattributes_pins_license_and_notice_to_unix_line_ends(self):
        # LICENSE has no extension, so no `*.md`-style line reaches it: only a line of its own keeps a Windows checkout (core.autocrlf=true) from writing it with CRLF
        if not (os.path.isdir(os.path.join(REPO, '.git')) or os.path.isfile(os.path.join(REPO, '.git'))) or shutil.which('git') is None:
            self.skipTest('no git work tree or no git: the attributes were NOT read')
        done = subprocess.run(['git', '-C', REPO, 'check-attr', 'eol', '--', 'LICENSE', 'NOTICE.md'], capture_output=True, text=True, env=GIT_ENV)
        self.assertEqual(done.returncode, 0, done.stderr)
        self.assertEqual(sorted(done.stdout.split('\n')[:2]), ['LICENSE: eol: lf', 'NOTICE.md: eol: lf'], done.stdout)

    def test_a_clone_made_with_core_autocrlf_true_has_license_and_notice_with_unix_line_ends(self):
        # the defect this pins: on a Windows machine (and on GitHub's Windows runners) a plain clone wrote LICENSE with CRLF and NOTICE.md with LF, and release.py copies both into an
        # archive unchanged. The repository's own .gitattributes decides, so it is the file the throwaway repository gets.
        if shutil.which('git') is None:
            self.skipTest('no git: the clone was NOT made')
        with tempfile.TemporaryDirectory(prefix='mp-licence-eol-') as tmp:
            source, clone = os.path.join(tmp, 'source'), os.path.join(tmp, 'clone')
            os.makedirs(source)
            for name in ('.gitattributes', 'LICENSE', 'NOTICE.md'):
                with open(os.path.join(REPO, name), 'rb') as f:
                    data = f.read().replace(b'\r\n', b'\n')  # what the repository stores; the working copy of the one running this test may have CRLF
                with open(os.path.join(source, name), 'wb') as f:
                    f.write(data)
            for args in (['init', '-q'], ['add', '-A'], ['-c', 'user.name=Test', '-c', 'user.email=test@example.invalid', 'commit', '-q', '-m', 'files']):
                done = subprocess.run(['git', '-C', source] + args, capture_output=True, text=True, env=GIT_ENV)
                self.assertEqual(done.returncode, 0, done.stderr)
            done = subprocess.run(['git', '-c', 'core.autocrlf=true', 'clone', '-q', source, clone], capture_output=True, text=True, env=GIT_ENV)
            self.assertEqual(done.returncode, 0, done.stderr)
            for name in ('LICENSE', 'NOTICE.md'):
                with open(os.path.join(clone, name), 'rb') as f:
                    self.assertNotIn(b'\r', f.read(), '%s was checked out with CRLF' % name)


class TheNotice(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.text = read(os.path.join(REPO, 'NOTICE.md'))

    def test_it_has_the_credit_and_the_third_party_sections_in_that_order(self):
        heads = [l for l in self.text.split('\n') if l.startswith('## ')]
        self.assertEqual(heads[:2], ['## Credit', '## Third-party components in a program'])
        credit = self.text.split('## Third-party components')[0]
        for needle in ('Asantee Games', 'Andre Santee', 'Supersonic Engine', 'Ivan Cvetanovic', 'Ethanon', 'MIT'):
            self.assertIn(needle, credit)

    def test_it_stands_alone_and_links_to_no_file_of_the_repository(self):
        # a reader of an archive has NOTICE.md and nothing else of the repository
        self.assertEqual([m for m in re.findall(r'\]\(([^)]*)\)', self.text) if not re.match(r'[a-z]+://', m)], [])

    def test_every_block_is_announced_and_fenced_and_has_text(self):
        found = blocks(self.text)
        self.assertGreaterEqual(len(found), 18)
        for n, path, first, last, body in found:
            self.assertIsNotNone(body, 'line %d: the marker for %s is not followed by a fenced block' % (n, path))
            self.assertTrue([l for l in body if l.strip()], 'line %d: the block for %s is empty' % (n, path))

    def test_each_block_from_the_engine_is_the_text_of_its_file(self):
        if not os.path.isdir(os.path.join(ENGINE, 'third_party')):
            self.skipTest('the engine is not checked out: no licence text was compared (git submodule update --init)')
        compared = 0
        for n, path, first, last, body in blocks(self.text):
            kind = source_of(path)
            if kind in ('external', 'font'):
                continue
            self.assertEqual(kind, 'engine', 'line %d: %s is neither in the engine, nor a font licence of game/data/fonts, nor marked external' % (n, path))
            lines = read(os.path.join(REPO, *path.split('/'))).split('\n')
            if first is not None:
                lines = lines[first - 1:last]
            self.assertEqual(normal(body), normal(lines), 'line %d: the block is not the text of %s%s' % (n, path, '' if first is None else ' lines %d-%d' % (first, last)))
            compared += 1
        self.assertGreaterEqual(compared, 15)

    def test_each_font_licence_block_is_the_text_of_the_file_beside_its_font(self):
        # the four faces of the interface languages (game/data/fonts) are under the SIL OFL 1.1 and each licence text travels with its font: NOTICE.md reproduces each, whole
        folder = os.path.join(REPO, 'game', 'data', 'fonts')
        files = sorted(f for f in os.listdir(folder) if f.startswith('OFL-') and f.endswith('.txt'))
        self.assertGreaterEqual(len(files), 4, files)
        announced = sorted(path for n, path, first, last, body in blocks(self.text) if source_of(path) == 'font')
        self.assertEqual(announced, ['game/data/fonts/' + f for f in files], 'every OFL text beside a font is in NOTICE.md, and NOTICE.md has no other')
        for n, path, first, last, body in blocks(self.text):
            if source_of(path) != 'font':
                continue
            self.assertIsNone(first, 'line %d: a font licence is reproduced whole' % n)
            self.assertEqual(normal(body), normal(read(os.path.join(REPO, *path.split('/'))).split('\n')), 'line %d: the block is not the text of %s' % (n, path))
        # and each font file has its row in the table and its licence section
        table = '\n'.join(l for l in self.text.split('\n') if l.startswith('| '))
        for needle in ('Kurale', 'Noto Sans (', 'Noto Sans JP', 'Noto Sans Arabic'):
            self.assertIn(needle, table, needle)
        for ttf in sorted(f for f in os.listdir(folder) if f.endswith('.ttf')):
            self.assertTrue(os.path.isfile(os.path.join(folder, 'OFL-' + ttf.split('-')[0] + '.txt')), '%s has no OFL text beside it' % ttf)

    def test_every_component_of_the_engines_own_list_is_in_the_table(self):
        listing = os.path.join(ENGINE, 'THIRD_PARTY_LICENSES.md')
        if not os.path.isfile(listing):
            self.skipTest("the engine is not checked out: its list was not read (git submodule update --init)")
        names = []
        for line in read(listing).split('\n'):
            m = re.match(r'^\| \[([^\]]+)\]\(', line)
            if m:
                names.append(m.group(1))
        self.assertGreaterEqual(len(names), 12)
        table = '\n'.join(l for l in self.text.split('\n') if l.startswith('| '))
        for name in names:
            self.assertIn(name.split(' (')[0].lower().replace('vulkan memory allocator', 'vulkan memory allocator'), table.lower(), '%s is in the engine list and not in the table' % name)

    def test_the_table_says_for_each_platform_how_the_entry_was_established(self):
        rows = [l for l in self.text.split('\n') if l.startswith('| ') and not l.startswith('| ---') and not l.startswith('| Component')]
        self.assertGreaterEqual(len(rows), 14)
        for row in rows:
            cells = [c.strip() for c in row.strip('|').split('|')]
            self.assertEqual(len(cells), 8, row)
        self.assertIn('| Component | Version | Licence taken | Windows | Linux | Android | macOS | iOS |', self.text)
        self.assertIn('engine list', self.text)
        self.assertIn('linked', self.text)

    def test_the_gcc_runtime_library_exception_is_reproduced_for_the_portable_linux_program(self):
        # a Linux program built with --portable has libstdc++ and libgcc inside it (tools/build_linux.sh), and the exception is what lets it be conveyed under its own licence: its text is in the
        # file an archive carries. The block is `external:` (a file of the compiler's package, not of the engine), so it is pinned by its parts and not compared with a file.
        found = [b for b in blocks(self.text) if b[1].startswith('external: GCC RUNTIME LIBRARY EXCEPTION')]
        self.assertEqual(len(found), 1)
        body = '\n'.join(found[0][4])
        self.assertTrue(body.startswith('GCC RUNTIME LIBRARY EXCEPTION\n\nVersion 3.1, 31 March 2009'), body[:80])
        flat = ' '.join(body.split())  # the text is wrapped at 70 columns: compare it as words
        for needle in ('is an additional permission under section 7 of the GNU General Public License', 'A Compilation Process is "Eligible"', '1. Grant of Additional Permission.',
                       'You have permission to propagate a work of Target Code formed by combining the Runtime Library with Independent Modules',
                       'provided that all Target Code was generated by Eligible Compilation Processes', 'You may then convey such a combination under terms of your choice',
                       '2. No Weakening of GCC Copyleft.', 'requirements of the license of GCC.'):
            self.assertIn(needle, flat)
        self.assertGreaterEqual(len(found[0][4]), 70)
        table = '\n'.join(l for l in self.text.split('\n') if l.startswith('| '))
        self.assertIn('libstdc++ and libgcc', table)
        self.assertIn('GCC Runtime Library Exception 3.1', table)
        self.assertIn('### GCC runtime libraries (Linux programs built with `--portable`)', self.text)

    def test_it_says_nothing_about_the_grant_of_permission_or_why_content_may_be_shipped(self):
        # the credit is the whole of what it says about the original game; the standard licence texts below it are exempt (they say "distribute")
        credit = self.text.split('## Third-party components')[0].lower()
        for word in ('permission', 'grant', 'allowed', 'right to', 'rights'):
            self.assertNotIn(word, credit, word)

    def test_it_holds_no_path_of_a_pc_and_no_version_of_the_game(self):
        for needle in ('C:\\', 'C:/', '/home/', '/Users/', '/mnt/'):
            self.assertNotIn(needle, self.text, needle)

    def test_the_two_places_that_follow_other_source_are_the_two_the_tools_say_they_port(self):
        for path, needle in (('tools/asbc/asbc.py', 'Ported from AngelScript'), ('tools/converter/src/ethanon2godot/enml.py', 'a port of that state machine')):
            self.assertIn(needle, read(os.path.join(REPO, *path.split('/'))), path)
        section = self.text.split("## Source in this repository that follows other projects' source")[1]
        self.assertIn('tools/asbc/', section)
        self.assertIn('tools/converter/src/ethanon2godot/enml.py', section)
        self.assertIn('Andreas Jonsson', section)
        self.assertIn('Andre Santee', section)


class TheHelpers(unittest.TestCase):
    def test_a_comment_mark_and_a_common_indent_are_taken_off_and_only_those(self):
        self.assertEqual(normal(['// a', '//', '//   b', '', '']), ['a', '', '  b'])
        self.assertEqual(normal(['   one', '', '     two', '']), ['one', '', '  two'])
        self.assertEqual(normal(['x ', ' y']), ['x', ' y'])

    def test_the_block_reader_finds_the_marker_the_range_and_the_text(self):
        text = 'a\n<!-- verbatim: engine/x/LICENSE 3-9 -->\n```text\nline\n\nline2\n```\n<!-- verbatim: external: y -->\ntext\n'
        self.assertEqual(blocks(text), [(2, 'engine/x/LICENSE', 3, 9, ['line', '', 'line2']), (8, 'external: y', None, None, None)])

    def test_only_the_engine_a_font_licence_and_an_external_marker_are_sources(self):
        self.assertEqual(source_of('engine/third_party/entt-3.13.2/LICENSE'), 'engine')
        self.assertEqual(source_of('game/data/fonts/OFL-Kurale.txt'), 'font')
        self.assertEqual(source_of('game/data/fonts/OFL-NotoSansJP.txt'), 'font')
        self.assertEqual(source_of('external: GCC RUNTIME LIBRARY EXCEPTION, version 3.1'), 'external')
        for refused in ('game/data/fonts/../../LICENSE', 'game/data/fonts/Kurale-Regular.ttf', 'game/data/fonts/sub/OFL-X.txt', 'game/data/fonts/OFL-.txt',
                        'game/data/fonts/OFL-Kurale.TXT', 'game/data/fonts/OFL-Ku-rale.txt', 'game/data/fonts/README.md', 'game/data/lang/en.json', 'game/data/OFL-Kurale.txt',
                        'LICENSE', 'NOTICE.md', 'reference/extracted/x.txt', 'out/OFL-Kurale.txt', 'engine/../LICENSE', '/etc/passwd', ''):
            self.assertIsNone(source_of(refused), refused)

    def test_a_changed_word_is_not_equal(self):
        self.assertNotEqual(normal(['Permission is hereby granted']), normal(['Permission is hereby granted,']))


if __name__ == '__main__':
    unittest.main()
