"""Tests of the release packager (tools/package/release.py, planning step 208, PORT-10), on SYNTHETIC programs, packages and bundles the tests build themselves.

Run:   python -m unittest discover -s tools/package -p "test_*.py" -v        (standard library only; no SDK, no compiler, no Apple tool, no Windows program is started)

Every byte and name is invented: a PE file is a header, one section and a few strings; an ELF or Mach-O file is a header and a payload; a content package is the miniature
world of test_package.py (invented names, no file of the original); the notices are three invented lines. No program of the game is read, copied or started by any test here.
What is pinned: the facts check-program reads out of a built program and the way each makes it refuse (one named mutation per forbidden string, per sentinel, per machine
type, per version field), the ties of its strings to the C++ that prints them, the archives (layout, modes, determinism, refusals that leave nothing behind), the Apple
layouts, the Android release check, and the sums.
"""
import contextlib
import hashlib
import io
import os
import re
import shutil
import struct
import sys
import tarfile
import tempfile
import unittest
import zipfile
from unittest import mock

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import android_package as ap  # noqa: E402
import package as pk  # noqa: E402
import release as rl  # noqa: E402
import version as vs  # noqa: E402
from test_android_package import find_ndk_library  # noqa: E402  (a real ELF32 file from the NDK, for the one test that reads it)

REPO = os.path.dirname(os.path.dirname(HERE))
VERSION = vs.read_version(REPO)
WINDOWS_VERSION = vs.windows_version(VERSION)
# An INVENTED account name stands for the account that runs the check: the packager reads the real one from the system when it runs (planning step 210), so no file of the project
# spells it. Every test in this module sees this name as the running account (setUpModule); the tests of the lookup itself put the real function back. The path is the one a
# builder's machine would bake into a program.
ACCOUNT = 'zorbgar'
CHECKOUT_PATH_OF_A_BUILDER = ('C:/Users/%s/Desktop/Magic-Portals-Enhanced/out/levels' % ACCOUNT).encode()
REAL_RUNNING_ACCOUNTS = rl.running_accounts


def setUpModule():
    rl.running_accounts = lambda: [ACCOUNT]


def tearDownModule():
    rl.running_accounts = REAL_RUNNING_ACCOUNTS


def phrase(*parts):
    """A phrase from its parts, so that a list of words a text must NOT hold does not hold them whole itself."""
    return ' '.join(parts)


# ---- synthetic programs ---------------------------------------------------------------------------------------------------------------------

def game_payload(dev=False, perf=False, extra=()):
    """The strings every build of the game carries, and the ones that only some builds carry. Nothing else of a program is needed by the reader."""
    parts = list(rl.GAME_MARKERS) + [b'--route', b'--tap', b'is a developer\'s flag, and this build was made without the developer tools', b'DEV: keep the medals']
    if dev:
        parts += list(rl.DEV_TOOLS_SENTINELS)
    if perf:
        parts.append(rl.PERF_DIAG_MARKER + b': a player\'s run of it has the engine\'s frame readout')
    parts += list(extra)
    return b'\0'.join(parts) + b'\0'


def version_block(product=WINDOWS_VERSION, file=WINDOWS_VERSION, quad=None, fixed=True):
    """The pieces of a VERSIONINFO the reader scans for: the fixed block (signature 0xFEEF04BD) and the UTF-16 key/value pairs, padded as Windows pads them."""
    quad = quad if quad is not None else tuple(int(x) for x in WINDOWS_VERSION.split('.'))
    out = b''
    if fixed:
        ms, ls = (quad[0] << 16) | quad[1], (quad[2] << 16) | quad[3]
        out += struct.pack('<IIIIIIII', 0xFEEF04BD, 0x10000, ms, ls, ms, ls, 0x3F, 0)
    for key, value in (('FileVersion', file), ('ProductVersion', product)):
        if value is None:
            continue
        item = (key + '\0').encode('utf-16-le')
        item += b'\0' * (-len(item) % 4)
        out += item + (value + '\0').encode('utf-16-le') + b'\0\0'
    return out


def make_pe(machine=0x8664, subsystem=2, imports=('KERNEL32.dll', 'USER32.dll'), delay=('vulkan-1.dll',), plus=True, dll=False, payload=None, version=True):
    """A minimal PE: DOS header, PE header, one section holding the import tables and the payload. Enough for read_pe, and for nothing else (it cannot be started)."""
    payload = game_payload() if payload is None else payload
    section_at, rva_base = 0x200, 0x1000
    names = b''
    name_off = {}
    for n in list(imports) + list(delay):
        name_off[n] = len(names)
        names += n.encode() + b'\0'
    imp_size = 20 * (len(imports) + 1)
    del_size = 32 * (len(delay) + 1)
    names_at = imp_size + del_size
    rva = lambda off: rva_base + off  # noqa: E731
    imp = b''.join(struct.pack('<IIIII', 0, 0, 0, rva(names_at + name_off[n]), 0) for n in imports) + b'\0' * 20
    dly = b''.join(struct.pack('<IIIIIIII', 1, rva(names_at + name_off[n]), 0, 0, 0, 0, 0, 0) for n in delay) + b'\0' * 32
    body = imp + dly + names
    body += b'\0' * (-len(body) % 16) + payload + (version_block() if version is True else (version or b''))
    optsize = 240 if plus else 224
    header = bytearray(0x200)
    header[0:2] = b'MZ'
    struct.pack_into('<I', header, 0x3C, 0x80)
    header[0x80:0x84] = b'PE\0\0'
    struct.pack_into('<HHIIIHH', header, 0x84, machine, 1, 0, 0, 0, optsize, 0x22 | (0x2000 if dll else 0))
    opt = 0x98
    struct.pack_into('<H', header, opt, 0x20B if plus else 0x10B)
    struct.pack_into('<H', header, opt + 68, subsystem)
    count_at, dirs_at = (opt + 108, opt + 112) if plus else (opt + 92, opt + 96)
    struct.pack_into('<I', header, count_at, 16)
    struct.pack_into('<II', header, dirs_at + 8 * 1, rva(0), imp_size)
    struct.pack_into('<II', header, dirs_at + 8 * 13, rva(imp_size), del_size)
    table = opt + optsize
    struct.pack_into('<8sIIII', header, table, b'.rdata', len(body), rva_base, len(body), section_at)
    return bytes(header) + body


def make_elf_program(machine=62, etype=3, cls=None, little=1, payload=None, glibc=('2.34', '2.38'), glibcxx=('3.4.29', '3.4.32'), sections=None, table_past_the_end=False, width=64):
    """`sections`: the names of the sections the program has (None: no section header table at all, which reads as a program stripped to the bone). The table is written
    after the payload: a null section, one section per name, and the section-name string table last. `width` is the layout: 64 (Elf64, the default) or 32 (Elf32: the header is
    52 bytes and a section header 40, a library of the ABI armeabi-v7a, machine 40). `cls` is the class BYTE the header carries; it follows `width` unless a test wants a lie."""
    elf32 = width == 32
    head_size, section_size = (52, 40) if elf32 else (64, 64)
    cls = cls if cls is not None else (1 if elf32 else 2)
    payload = game_payload() if payload is None else payload
    names = b''.join(b'GLIBC_%s\0' % v.encode() for v in glibc) + b''.join(b'GLIBCXX_%s\0' % v.encode() for v in glibcxx)
    body = b'\0' * 64 + names + payload
    shoff = shnum = 0
    if sections is not None:
        strings = b'\0'
        offsets = {}
        for n in list(sections) + ['.shstrtab']:
            offsets[n] = len(strings)
            strings += n.encode() + b'\0'
        body += strings
        string_at = head_size + len(body) - len(strings)
        body += b'\0' * (-len(body) % 8)
        shoff = head_size + len(body)
        headers = b'\0' * section_size  # the null section
        for n in list(sections) + ['.shstrtab']:
            at, size = (string_at, len(strings)) if n == '.shstrtab' else (0, 0)
            # name, type, flags, address, file offset, size, link, info, alignment, entry size: ten words of 4 bytes in Elf32, and the 8-byte ones in Elf64
            headers += struct.pack('<10I' if elf32 else '<IIQQQQIIQQ', offsets[n], 3 if n == '.shstrtab' else 1, 0, 0, at, size, 0, 0, 1, 0)
        shnum = len(sections) + 2
        body += headers
        if table_past_the_end:
            shoff += 10_000
    head = b'\x7fELF' + bytes([cls, little, 1, 0]) + b'\0' * 8
    if elf32:
        head += struct.pack('<HHIIIIIHHHHHH', etype, machine, 1, 0, head_size, shoff, 0, head_size, 32, 0, section_size if sections is not None else 0, shnum, shnum - 1 if shnum else 0)
    else:
        head += struct.pack('<HHIQQQIHHHHHH', etype, machine, 1, 0, head_size, shoff, 0, head_size, 56, 0, section_size if sections is not None else 0, shnum, shnum - 1 if shnum else 0)
    return head + body


def make_macho(cputype=0x0100000C, filetype=2, payload=None):
    payload = game_payload() if payload is None else payload
    return MACHO_MAGIC + struct.pack('<IIIIIII', cputype, 0, filetype, 0, 0, 0, 0) + payload


MACHO_MAGIC = rl.MACHO_LE_MAGIC


def make_fat(cputypes=(0x01000007, 0x0100000C), payload=None, magic=rl.MACHO_FAT_MAGIC, count=None):
    payload = game_payload() if payload is None else payload
    count = len(cputypes) if count is None else count
    out = magic + struct.pack('>I', count)
    for c in cputypes:
        out += struct.pack('>IIIII', c, 0, 4096, len(payload), 12)
    return out.ljust(4096, b'\0') + payload


def put(path, data):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'wb') as f:
        f.write(data if isinstance(data, bytes) else data.encode('utf-8'))


def run_cli(*argv):
    out, err = io.StringIO(), io.StringIO()
    with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
        try:
            code = rl.main(list(argv))
        except SystemExit as e:  # argparse's own exit
            code = e.code
    return code, out.getvalue(), err.getvalue()


def checked(data, platform, **kw):
    kw.setdefault('version', VERSION)
    kw.setdefault('repo', REPO)
    return rl.check_program_bytes(data, platform, **kw)


class Temp(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.mkdtemp(prefix='mp-release-test-')
        self.addCleanup(shutil.rmtree, self._tmp, True)

    def path(self, *parts):
        return os.path.join(self._tmp, *parts)


# ---- the ties to the C++ -------------------------------------------------------------------------------------------------------------------

def guarded_regions(lines, macro):
    """The line numbers (1-based) that lie inside `#if <macro>` (positive, up to its #else or #endif) of a C++ file, nesting understood."""
    inside, stack = set(), []
    for n, line in enumerate(lines, 1):
        s = line.strip()
        if re.match(r'#\s*(if|ifdef|ifndef)\b', s):
            stack.append(bool(re.fullmatch(r'#\s*if\s+%s(\s*//.*)?' % re.escape(macro), s)))
        elif re.match(r'#\s*else\b', s) and stack:
            stack[-1] = False
        elif re.match(r'#\s*endif\b', s) and stack:
            stack.pop()
        elif any(stack):
            inside.add(n)
    return inside


class TheTiesToTheCpp(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        with open(os.path.join(REPO, 'game', 'main.cpp'), encoding='utf-8') as f:
            cls.lines = f.read().split('\n')
        cls.dev = guarded_regions(cls.lines, 'MAGICPORTALS_DEV_TOOLS')
        cls.perf = guarded_regions(cls.lines, 'MAGICPORTALS_PERF_DIAG')

    def lines_with(self, text):
        return [n for n, line in enumerate(self.lines, 1) if text in line]

    def test_the_region_finder_is_not_vacuous(self):
        self.assertGreater(len(self.dev), 100, 'main.cpp has hundreds of lines under #if MAGICPORTALS_DEV_TOOLS')
        self.assertEqual(guarded_regions(['#if A', 'x', '#else', 'y', '#endif', 'z'], 'A'), {2})
        self.assertEqual(guarded_regions(['#if !A', 'x', '#endif'], 'A'), set())
        self.assertEqual(guarded_regions(['#if A', '#ifdef B', 'x', '#endif', 'y', '#endif // A', 'z'], 'A'), {3, 5})

    def test_every_sentinel_is_a_help_line_inside_the_developer_tools_region_and_nowhere_else(self):
        self.assertGreaterEqual(len(rl.DEV_TOOLS_SENTINELS), 4, 'the check reads the tools from several of their help lines, not from one')
        self.assertEqual(len(set(rl.DEV_TOOLS_SENTINELS)), len(rl.DEV_TOOLS_SENTINELS))
        for sentinel in rl.DEV_TOOLS_SENTINELS:
            text = sentinel.decode('utf-8')
            where = self.lines_with(text)
            self.assertEqual(len(where), 1, '%r must be one line of game/main.cpp (it is on %s)' % (text, where))
            self.assertIn(where[0], self.dev, '%r is not inside #if MAGICPORTALS_DEV_TOOLS: a program made without the tools could carry it' % text)
            self.assertIn('<<', self.lines[where[0] - 1], 'it must be a line of --help')
            # and no other source of the game spells it (a program made with the tools OFF links every one of them)
            for folder, _, files in os.walk(os.path.join(REPO, 'game')):
                for name in files:
                    if name.endswith(('.cpp', '.hpp', '.mm', '.in', '.json')) and not (folder == os.path.join(REPO, 'game') and name == 'main.cpp'):
                        with open(os.path.join(folder, name), encoding='utf-8', errors='replace') as f:
                            self.assertNotIn(text, f.read(), '%s also spells %r' % (os.path.join(folder, name), text))

    def test_the_sentinels_are_not_the_refusal_sentence_nor_the_saves_entry(self):
        for sentinel in rl.DEV_TOOLS_SENTINELS:
            self.assertNotIn(b'developer\'s flag', sentinel)
            self.assertNotIn(b'keep the medals', sentinel)
        # --saves keeps its DEV: wording in a program with the tools OFF (docs/packaging.md section 6): its line is outside the region
        saves = self.lines_with('DEV: keep the medals')
        self.assertEqual(len(saves), 1)
        self.assertNotIn(saves[0], self.dev)

    def test_the_performance_marker_is_a_log_line_inside_the_diagnostic_region(self):
        where = self.lines_with(rl.PERF_DIAG_MARKER.decode())
        self.assertEqual(len(where), 1, where)
        self.assertIn(where[0], self.perf)
        self.assertIs(rl.PERF_DIAG_MARKER, ap.PERF_DIAG_MARKER, 'one definition, the Android packager\'s')

    def test_the_game_markers_are_in_every_build_of_the_program(self):
        with open(os.path.join(REPO, 'game', 'sim', 'Scratch.hpp'), encoding='utf-8') as f:
            self.assertIn('kLogFileName = "%s"' % rl.GAME_MARKERS[0].decode(), f.read())
        text = '\n'.join(self.lines)
        self.assertIn(rl.GAME_MARKERS[1].decode(), text)
        self.assertNotIn(self.lines[self.lines_with(rl.GAME_MARKERS[1].decode())[0] - 1].strip()[:1], ('#',))
        self.assertNotIn(self.lines_with(rl.GAME_MARKERS[1].decode())[0], self.dev, 'the second marker must not be a developer-tools-only line')

    def test_the_list_of_forbidden_strings_is_the_one_the_documentation_names(self):
        # the list is a decision, and the loops below run over it: this is what pins its contents (docs/packaging.md section 8 names the same ones; the checkout's own path and the name of the
        # account that runs the check are added at run time, planning step 210).
        # Planning step 209 (PORT10-2 g): the two folder names are listed as PATHS (between two separators), not as bare names, because the folder name is also the project's public name.
        self.assertEqual([b for b, _ in rl.FORBIDDEN_STRINGS], [b'/Magic-Portals-Enhanced/', b'\\Magic-Portals-Enhanced\\', b'/Magic-Portals-Remake/', b'\\Magic-Portals-Remake\\',
                                                                 b'/mnt/', b'/home/', b'/Users/', b'\\Users\\'])
        with open(os.path.join(REPO, 'docs', 'packaging.md'), encoding='utf-8') as f:
            doc = f.read()
        for word in ('`/mnt/`', '`/home/`', '`/Users/`', '`\\Users\\`', "the checkout's two folder names as paths", "this checkout's own path", 'the name of the account that runs the check'):
            self.assertIn(word, doc)

    def test_every_forbidden_string_says_why_and_the_list_has_no_duplicate(self):
        names = [b for b, _ in rl.FORBIDDEN_STRINGS]
        self.assertEqual(len(names), len(set(n.lower() for n in names)))
        for b, why in rl.FORBIDDEN_STRINGS:
            self.assertGreater(len(why), 20, b)

    def test_the_windows_form_of_the_version_is_what_the_resource_template_makes(self):
        self.assertEqual(WINDOWS_VERSION, VERSION + '.0')
        with open(os.path.join(REPO, 'game', 'windows', 'MagicPortalsWindows.cmake'), encoding='utf-8') as f:
            self.assertIn('set(MAGICPORTALS_RC_VERSION "${_major}.${_minor}.${_patch}.${_build}")', f.read())


# ---- check-program --------------------------------------------------------------------------------------------------------------------------

class TheFormatsAndMachines(Temp):
    def test_a_clean_windows_program_passes_with_every_fact_said(self):
        r = checked(make_pe(), rl.WINDOWS)
        self.assertEqual(r.problems, [])
        text = '\n'.join(t for _, t in r.facts)
        for word in ('PE, machine 0x8664 (x64), subsystem 2 (GUI)', 'no baked path', 'developer tools OFF', 'no performance readout', 'it is the game',
                     'ProductVersion %s' % WINDOWS_VERSION, 'FileVersion %s' % WINDOWS_VERSION):
            self.assertIn(word, text)
        self.assertEqual(r.arch, 'x64')

    def test_the_machine_types_each_platform_accepts(self):
        self.assertEqual(checked(make_pe(), rl.WINDOWS).arch, 'x64')
        self.assertEqual(checked(make_elf_program(), rl.LINUX).arch, 'x86_64')
        self.assertEqual(checked(make_macho(0x01000007), rl.MACOS).arch, 'x86_64')
        self.assertEqual(checked(make_macho(0x0100000C), rl.MACOS).arch, 'arm64')
        self.assertEqual(checked(make_fat(), rl.MACOS).arch, 'universal')
        self.assertEqual(checked(make_macho(0x0100000C), rl.IOS).arch, 'arm64')
        self.assertEqual(checked(make_elf_program(machine=183), rl.ANDROID_LIB).arch, 'arm64')
        self.assertEqual(checked(make_elf_program(machine=62), rl.ANDROID_LIB).arch, 'x86_64')
        self.assertEqual(checked(make_elf_program(machine=40, width=32), rl.ANDROID_LIB).arch, 'armv7')
        for platform, data in ((rl.WINDOWS, make_pe()), (rl.LINUX, make_elf_program()), (rl.MACOS, make_fat()), (rl.IOS, make_macho()), (rl.ANDROID_LIB, make_elf_program()),
                               (rl.ANDROID_LIB, make_elf_program(machine=183)), (rl.ANDROID_LIB, make_elf_program(machine=40, width=32))):
            self.assertEqual(checked(data, platform).problems, [], platform)

    def test_a_wrong_machine_is_refused_and_never_allowed_through(self):
        cases = ((rl.WINDOWS, make_pe(machine=0x14C, plus=False)), (rl.WINDOWS, make_pe(machine=0xAA64)), (rl.WINDOWS, make_pe(plus=False)),
                 (rl.LINUX, make_elf_program(machine=183)), (rl.LINUX, make_elf_program(machine=3)), (rl.LINUX, make_elf_program(cls=1)),
                 (rl.LINUX, make_elf_program(little=2)), (rl.LINUX, make_elf_program(etype=1)),
                 (rl.MACOS, make_macho(cputype=7)), (rl.MACOS, make_macho(filetype=1)), (rl.IOS, make_fat()), (rl.IOS, make_macho(0x01000007)),
                 (rl.ANDROID_LIB, make_elf_program(etype=2)), (rl.ANDROID_LIB, make_elf_program(machine=3)))
        for platform, data in cases:
            r = checked(data, platform)
            self.assertTrue([t for k, t in r.problems if k == rl.STRUCTURE], (platform, r.problems))
            self.assertTrue(r.blocking(allow_dev=True), 'a wrong machine or format is never allowed through (%s)' % platform)

    def test_an_armeabi_v7a_library_is_an_elf32_file_of_the_arm_machine_and_only_an_android_library_may_be(self):
        r = checked(make_elf_program(machine=40, width=32), rl.ANDROID_LIB)
        self.assertEqual((r.problems, r.arch), ([], 'armv7'))
        self.assertTrue(any('format: ELF32, little-endian, machine 40 (armv7), type 3' in t for _, t in r.facts), r.facts)
        self.assertTrue(any('format: ELF64, little-endian, machine 183 (arm64), type 3' in t for _, t in checked(make_elf_program(machine=183), rl.ANDROID_LIB).facts))
        # The other platforms take no 32-bit ARM program: the machine is refused, and the class is right for it, so the machine is the only reason.
        r = checked(make_elf_program(machine=40, width=32), rl.LINUX)
        self.assertEqual([t for _, t in r.problems], ['ELF machine armv7 is not one for linux (x86_64)'])
        self.assertTrue(r.blocking(allow_dev=True))

    def test_the_class_must_be_the_machines_and_the_refusal_names_both(self):
        # Each file breaks ONE rule: the machine is one the platform takes, so the only problem is the class/machine combination, and it names the class, the machine and its class.
        cases = (('an Elf64 file with the arm machine', make_elf_program(machine=40), 'the file is a 64-bit ELF file and its machine 40 (armv7) is a 32-bit one'),
                 ('an Elf32 file with the aarch64 machine', make_elf_program(machine=183, width=32), 'the file is a 32-bit ELF file and its machine 183 (arm64) is a 64-bit one'),
                 ('an Elf32 file with the x86_64 machine', make_elf_program(machine=62, width=32), 'the file is a 32-bit ELF file and its machine 62 (x86_64) is a 64-bit one'),
                 ('the Elf32 layout under a 64-bit class byte', make_elf_program(machine=40, width=32, cls=2), 'the file is a 64-bit ELF file and its machine 40 (armv7) is a 32-bit one'),
                 ('the Elf64 layout under a 32-bit class byte', make_elf_program(machine=183, cls=1), 'the file is a 32-bit ELF file and its machine 183 (arm64) is a 64-bit one'))
        for what, data, message in cases:
            r = checked(data, rl.ANDROID_LIB)
            self.assertEqual([t for k, t in r.problems if k == rl.STRUCTURE], [message], what)
            self.assertTrue(r.blocking(allow_dev=True), 'never allowed through: %s' % what)
        r = checked(make_elf_program(machine=62, cls=1), rl.LINUX)
        self.assertEqual([t for _, t in r.problems], ['the file is a 32-bit ELF file and its machine 62 (x86_64) is a 64-bit one'], 'a Linux program too')
        # What is not a class at all, or not little-endian, is said as it is.
        self.assertEqual([t for _, t in checked(make_elf_program(cls=3), rl.ANDROID_LIB).problems], ['ELF class 3 is neither 32-bit (1) nor 64-bit (2)'])
        self.assertEqual([t for _, t in checked(make_elf_program(little=2), rl.ANDROID_LIB).problems][0], 'not a little-endian ELF file')

    def test_a_file_that_lies_about_its_class_gets_the_named_refusal_not_a_section_table_error(self):
        # An Elf32 library whose header says 64-bit has a section table that would be read at the wrong offsets; the answer is the class/machine refusal, not "not the program".
        # The fixtures carry bytes where the OTHER class's header keeps its table fields, so that reading the table by the class byte would run past the end of the file.
        elf32_called_64 = bytearray(make_elf_program(machine=40, width=32, cls=2, sections=['.text', '.symtab']))
        struct.pack_into('<HHH', elf32_called_64, 0x3A, 64, 3, 1)  # Elf64's e_shentsize, e_shnum, e_shstrndx
        elf64_called_32 = bytearray(make_elf_program(machine=183, cls=1, sections=['.text', '.symtab']))
        struct.pack_into('<I', elf64_called_32, 0x20, 100_000)  # Elf32's e_shoff (the low half of Elf64's e_phoff)
        for data, message in ((bytes(elf32_called_64), 'the file is a 64-bit ELF file and its machine 40 (armv7) is a 32-bit one'),
                              (bytes(elf64_called_32), 'the file is a 32-bit ELF file and its machine 183 (arm64) is a 64-bit one')):
            r = checked(data, rl.ANDROID_LIB)
            self.assertEqual([t for _, t in r.problems], [message])
            self.assertEqual(rl.read_elf(data)['sections'], [], 'a table that cannot be trusted is not read')

    def test_the_section_names_of_an_elf32_file_are_read_by_its_own_table_layout(self):
        names = rl.read_elf(make_elf_program(machine=40, width=32, sections=['.text', '.symtab', '.strtab']))['sections']
        self.assertEqual(names, ['', '.text', '.symtab', '.strtab', '.shstrtab'], 'a section header of 40 bytes is a whole one in Elf32')
        self.assertEqual(rl.read_elf(make_elf_program(machine=40, width=32))['sections'], [])
        with self.assertRaises(rl.BinaryError):
            rl.read_elf(make_elf_program(machine=40, width=32, sections=['.text'], table_past_the_end=True))
        self.assertEqual(rl.read_elf(make_elf_program(machine=40, width=32))['arch'], 'armv7')

    def test_an_elf32_header_alone_is_a_whole_header_and_a_cut_one_is_not_a_file(self):
        header = make_elf_program(machine=40, width=32)[:52]  # sizeof(Elf32_Ehdr); an Elf64 header is 64 bytes, so these 52 would not be one
        elf = rl.read_elf(header)
        self.assertEqual((elf['class'], elf['machine'], elf['arch'], elf['sections']), (1, 40, 'armv7', []))
        with self.assertRaises(rl.BinaryError):
            rl.read_elf(header[:51])
        with self.assertRaises(rl.BinaryError):
            rl.read_elf(make_elf_program(machine=183)[:63])

    def test_a_real_elf32_library_from_the_ndk_is_read_as_one(self):
        # The hand-made files above come from the hands that wrote the reader; the NDK's libc++_shared.so for armeabi-v7a does not.
        found = find_ndk_library('arm-linux-androideabi', 'libc++_shared.so')
        if found is None:
            skip_loudly(self, 'no Android NDK with an armeabi-v7a libc++_shared.so here: read_elf was NOT compared with a real ELF32 file')
        with open(found, 'rb') as f:
            data = f.read()
        elf = rl.read_elf(data)
        self.assertEqual((elf['class'], elf['machine'], elf['arch'], elf['type']), (1, 40, 'armv7', 3))
        self.assertEqual(len(elf['sections']), struct.unpack_from('<H', data, 0x30)[0], 'one name per section header (e_shnum, at 0x30 in an Elf32 header)')
        for name in ('.text', '.dynamic', '.dynsym', '.dynstr', '.shstrtab'):
            self.assertIn(name, elf['sections'])
        facts = [t for _, t in checked(data, rl.ANDROID_LIB).facts]
        self.assertIn('format: ELF32, little-endian, machine 40 (armv7), type 3', facts, 'the format and the machine of an armeabi-v7a library (it is not the game, which is another fact)')

    def test_the_machine_tables_agree_with_the_packagers(self):
        # Two tables describe one set of ABIs: android_package.py's (ABI -> machine, class) and this tool's (machine -> arch, class). A new ABI in one and not the other is a library
        # that is built and then refused, or one that is accepted here and never built.
        arch_of = {abi: rl.ELF_MACHINES[machine] for abi, machine in ap.ABI_MACHINE.items()}
        self.assertEqual(sorted(arch_of.values()), sorted(rl.ACCEPTED_ARCHS[rl.ANDROID_LIB]), 'the architectures an Android library may have are the ABIs')
        for abi, arch in arch_of.items():
            self.assertEqual(rl.ELF_ARCH_CLASS[arch], ap.ABI_ELF_CLASS[abi], abi)
        self.assertEqual(rl.ACCEPTED_ARCHS[rl.LINUX], ('x86_64',), 'a Linux program is still x86_64 only')
        self.assertEqual(sorted(rl.ELF_ARCH_CLASS), sorted(rl.ELF_MACHINES.values()))
        for abi in ap.ABI_MACHINE:
            data = make_elf_program(machine=ap.ABI_MACHINE[abi], width=rl.ELF_CLASS_BITS[ap.ABI_ELF_CLASS[abi]])
            self.assertEqual(checked(data, rl.ANDROID_LIB).problems, [], abi)

    def test_a_file_of_another_format_is_not_the_program(self):
        for platform in rl.CHECKED_PLATFORMS:
            for data in (b'', b'plain text', make_elf_program() if platform in (rl.WINDOWS, rl.MACOS, rl.IOS) else make_pe()):
                r = checked(data, platform)
                self.assertTrue(r.blocking(allow_dev=True), (platform, data[:8]))

    def test_a_dll_is_not_a_program(self):
        r = checked(make_pe(dll=True), rl.WINDOWS)
        self.assertTrue(any('DLL' in t for _, t in r.problems))

    def test_a_java_class_file_is_not_a_fat_mach_o_file(self):
        data = rl.MACHO_FAT_MAGIC + struct.pack('>I', 52) + b'\0' * 64 + game_payload()
        r = checked(data, rl.MACOS)
        self.assertTrue(any('Java class file' in t for _, t in r.problems), r.problems)

    def test_a_truncated_pe_is_a_fact_and_not_a_traceback(self):
        whole = make_pe()
        for cut in (0, 1, 0x3E, 0x7F, 0x90, 0x150):
            r = checked(whole[:cut], rl.WINDOWS)
            self.assertTrue(r.blocking(True), cut)

    def test_the_pe_reader_finds_the_imports_and_the_delay_loaded_library(self):
        pe = rl.read_pe(make_pe(imports=('KERNEL32.dll', 'VCRUNTIME140.dll', 'MSVCP140.dll'), delay=('vulkan-1.dll',)))
        self.assertEqual(pe['imports'], ['KERNEL32.dll', 'VCRUNTIME140.dll', 'MSVCP140.dll'])
        self.assertEqual(pe['delay_imports'], ['vulkan-1.dll'])
        self.assertEqual((pe['subsystem'], pe['machine'], pe['plus']), (2, 0x8664, True))
        self.assertEqual(rl.read_pe(make_pe(subsystem=3))['subsystem'], 3)

    def test_the_gnu_version_names_an_elf_program_asks_for_are_read(self):
        needs = rl.glibc_needs(make_elf_program(glibc=('2.2.5', '2.34', '2.38'), glibcxx=('3.4', '3.4.32')))
        self.assertEqual(needs['GLIBC'], (2, 38))
        self.assertEqual(needs['GLIBCXX'], (3, 4, 32))
        r = checked(make_elf_program(), rl.LINUX)
        self.assertTrue(any('GLIBC 2.38' in t for s, t in r.facts if s == 'info'))
        self.assertEqual(rl.glibc_needs(b'nothing here'), {})


class TheForbiddenStrings(Temp):
    def mutated(self, needle, form='ascii'):
        piece = needle if form == 'ascii' else needle.decode('latin-1').encode('utf-16-le')
        return make_pe(payload=game_payload(extra=[b'before ' + piece + b' after']))

    def test_each_forbidden_string_turns_a_clean_program_red_and_is_named(self):
        for needle, why in rl.FORBIDDEN_STRINGS:
            for form in ('ascii', 'utf-16'):
                r = checked(self.mutated(needle, form), rl.WINDOWS)
                bad = [t for k, t in r.problems if k == rl.DEVELOPER and repr(needle.decode()) in t]
                self.assertEqual(len(bad), 1, (needle, form, r.problems))
                self.assertIn(form, bad[0])

    def test_the_search_ignores_case(self):
        for needle, _ in rl.FORBIDDEN_STRINGS:
            r = checked(self.mutated(needle.upper()), rl.WINDOWS)
            self.assertTrue([t for _, t in r.problems if repr(needle.decode()) in t], needle)

    def test_a_program_made_with_baked_paths_is_refused_whatever_the_platform(self):
        baked = game_payload(extra=[b'/home/someone/Magic-Portals-Enhanced/out/levels', b'C:/Users/someone/Magic-Portals-Enhanced/data'])
        for platform, data in ((rl.LINUX, make_elf_program(payload=baked)), (rl.ANDROID_LIB, make_elf_program(payload=baked)), (rl.MACOS, make_macho(payload=baked)),
                               (rl.WINDOWS, make_pe(payload=baked))):
            r = checked(data, platform)
            names = ' '.join(t for _, t in r.problems)
            for word in ("'/home/'", "'/Magic-Portals-Enhanced/'", "'/Users/'"):
                self.assertIn(word, names, (platform, word))

    def test_the_checkouts_own_path_and_an_extra_text_are_forbidden_too(self):
        repo = self.path('somewhere', 'a-long-checkout-name')
        os.makedirs(repo)
        data = make_pe(payload=game_payload(extra=[repo.replace('\\', '/').encode()]))
        r = checked(data, rl.WINDOWS, repo=repo)
        self.assertTrue(any("this checkout's own path" in t for _, t in r.problems), r.problems)
        r = checked(make_pe(payload=game_payload(extra=[b'/srv/ci/work/mp'])), rl.WINDOWS, also_forbid=('/srv/ci/work',))
        self.assertTrue(any('--also-forbid' in t for _, t in r.problems), r.problems)
        self.assertEqual(checked(make_pe(), rl.WINDOWS, also_forbid=('/srv/ci/work',)).problems, [])

    def test_a_short_repo_path_is_not_a_forbidden_string(self):
        self.assertEqual(rl.checkout_forbidden('/'), [])
        self.assertEqual(rl.checkout_forbidden(None), [])

    def test_a_refusal_lists_every_failed_fact_not_the_first(self):
        data = make_pe(payload=game_payload(dev=True, perf=True, extra=[b'C:\\Users\\someone\\x']), version=version_block(product='9.9.9.9', file='9.9.9.9'))
        r = checked(data, rl.WINDOWS)
        kinds = ' | '.join(t for _, t in r.problems)
        for word in (repr('\\Users\\'), 'developer tools are compiled in', 'PERFORMANCE-DIAGNOSTIC', 'ProductVersion is', 'FileVersion is'):
            self.assertIn(word, kinds)
        self.assertGreaterEqual(len(r.problems), 5)


class TheDeveloperToolsAndTheReadout(Temp):
    def test_each_sentinel_alone_is_enough_to_refuse(self):
        for sentinel in rl.DEV_TOOLS_SENTINELS:
            r = checked(make_pe(payload=game_payload(extra=[sentinel])), rl.WINDOWS)
            self.assertEqual(len([t for k, t in r.problems if 'developer tools are compiled in' in t]), 1, sentinel)
            self.assertIn('1 of the %d' % len(rl.DEV_TOOLS_SENTINELS), '\n'.join(t for _, t in r.problems))

    def test_the_flag_names_and_the_refusal_sentence_do_not_make_a_program_a_developers(self):
        # a program with the tools OFF holds all of these (to refuse them by name): the check must not read them as the tools
        r = checked(make_pe(payload=game_payload(extra=[b'--visit-levels', b'--route-trace', b'--burn-dragon', b'--tap', b'--hold'])), rl.WINDOWS)
        self.assertEqual(r.problems, [])

    def test_the_performance_readout_is_refused_and_its_absence_is_said(self):
        r = checked(make_elf_program(payload=game_payload(perf=True)), rl.LINUX)
        self.assertTrue(any('PERFORMANCE-DIAGNOSTIC build' in t for _, t in r.problems))
        r = checked(make_elf_program(payload=game_payload()), rl.LINUX)
        self.assertTrue(any('no performance readout' in t for _, t in r.facts))

    def test_a_file_that_is_nothing_at_all_does_not_pass_the_absences(self):
        r = checked(make_pe(payload=b'nothing of the game'), rl.WINDOWS)
        self.assertTrue([t for k, t in r.problems if k == rl.STRUCTURE and 'not this game' in t], r.problems)
        self.assertTrue(r.blocking(allow_dev=True))


class TheVulkanLoader(Temp):
    def test_the_loader_name_is_the_one_the_program_asks_for(self):
        with open(os.path.join(REPO, 'game', 'sim', 'StartChecks.hpp'), encoding='utf-8') as f:
            self.assertIn('inline constexpr const char* kVulkanLoaderName = "%s";' % rl.VULKAN_LOADER, f.read())

    def test_a_delay_loaded_loader_passes_and_is_said(self):
        r = checked(make_pe(delay=('vulkan-1.dll',)), rl.WINDOWS)
        self.assertEqual(r.problems, [])
        self.assertTrue(any('delay-loaded' in t for _, t in r.facts))

    def test_a_loader_imported_at_start_is_refused_for_the_dialog_a_player_would_get(self):
        for name in ('vulkan-1.dll', 'VULKAN-1.DLL'):
            r = checked(make_pe(imports=('KERNEL32.dll', name), delay=()), rl.WINDOWS)
            self.assertTrue(any('is loaded when the program starts' in t and 'MAGICPORTALS_WIN_GUI=ON' in t for _, t in r.problems), (name, r.problems))
            self.assertTrue(r.blocking(False))
            self.assertFalse([t for k, t in r.problems if k != rl.DEVELOPER], 'a kind-of-build fact: --allow-dev may print it')

    def test_a_program_that_delay_loads_no_loader_is_refused(self):
        r = checked(make_pe(delay=('something_else.dll',)), rl.WINDOWS)
        self.assertTrue(any('delay-loads no vulkan-1.dll' in t for _, t in r.problems), r.problems)


class TheVersionInformation(Temp):
    def test_a_program_with_the_one_version_passes(self):
        self.assertEqual(checked(make_pe(), rl.WINDOWS).problems, [])

    def test_each_field_that_differs_is_red(self):
        cases = {'ProductVersion': dict(product='0.0.9.0'), 'FileVersion': dict(file='0.0.9.0'),
                 'the binary version block': dict(quad=(9, 9, 9, 9)), 'ProductVersion is': dict(product=VERSION)}
        for word, kw in cases.items():
            r = checked(make_pe(version=version_block(**kw)), rl.WINDOWS)
            self.assertTrue(any(word in t for _, t in r.problems), (word, r.problems))

    def test_a_program_with_no_version_information_is_refused_for_it(self):
        r = checked(make_pe(version=b''), rl.WINDOWS)
        self.assertTrue(any('no version information' in t and 'MAGICPORTALS_WIN_GUI=ON' in t for _, t in r.problems), r.problems)

    def test_a_key_without_a_value_is_a_fact(self):
        r = checked(make_pe(version=version_block(product=None, fixed=False)), rl.WINDOWS)
        self.assertTrue(any('has no ProductVersion' in t for _, t in r.problems), r.problems)

    def test_the_scan_reads_the_padding_windows_writes(self):
        for pad in (0, 2):  # a block is padded to a 32-bit boundary and every field is a whole number of 16-bit units, so the padding is 0 or 2 bytes
            block = ('FileVersion\0'.encode('utf-16-le') + b'\0' * pad + (WINDOWS_VERSION + '\0').encode('utf-16-le')
                     + 'ProductVersion\0'.encode('utf-16-le') + b'\0' * pad + (WINDOWS_VERSION + '\0').encode('utf-16-le'))
            strings, _ = rl.pe_version_strings(b'xx' + block)
            self.assertEqual(strings, {'ProductVersion': WINDOWS_VERSION, 'FileVersion': WINDOWS_VERSION}, pad)

    def test_a_version_checks_against_the_repository_it_is_given(self):
        r = checked(make_pe(), rl.WINDOWS, version='1.2.3')
        self.assertTrue(any('ProductVersion is' in t and '1.2.3' in t for _, t in r.problems))
        self.assertEqual(checked(make_pe(version=version_block(product='1.2.3.0', file='1.2.3.0', quad=(1, 2, 3, 0))), rl.WINDOWS, version='1.2.3').problems, [])


class TheCommandLine(Temp):
    def test_a_clean_program_exits_0_and_a_developers_exits_1_with_every_reason(self):
        clean = self.path('clean.exe')
        put(clean, make_pe())
        code, out, err = run_cli('check-program', clean, '--platform', 'windows')
        self.assertEqual(code, 0, err)
        self.assertIn("a player's build", out)
        dev = self.path('dev.exe')
        put(dev, make_pe(payload=game_payload(dev=True, extra=[CHECKOUT_PATH_OF_A_BUILDER]), version=b''))
        code, out, err = run_cli('check-program', dev, '--platform', 'windows')
        self.assertEqual(code, 1)
        self.assertIn('REFUSED', err)
        for word in ('the name of the account that runs this check', "'/Magic-Portals-Enhanced/'", 'developer tools are compiled in', 'no version information'):
            self.assertIn(word, err)

    def test_allow_dev_turns_the_refusals_into_warnings_and_not_a_wrong_machine(self):
        dev = self.path('dev.exe')
        put(dev, make_pe(payload=game_payload(dev=True), version=b''))
        code, out, err = run_cli('check-program', dev, '--platform', 'windows', '--allow-dev')
        self.assertEqual(code, 0, err)
        self.assertIn('WARNING (--allow-dev)', out)
        self.assertIn('DEVELOPER BUILD', out)
        wrong = self.path('wrong.exe')
        put(wrong, make_pe(machine=0xAA64, payload=game_payload(dev=True)))
        code, out, err = run_cli('check-program', wrong, '--platform', 'windows', '--allow-dev')
        self.assertEqual(code, 1)

    def test_an_armeabi_v7a_library_passes_check_program_as_an_android_library_and_a_wrong_class_does_not(self):
        good = self.path('v7.so')
        put(good, make_elf_program(machine=40, width=32))
        code, out, err = run_cli('check-program', good, '--platform', 'android-lib')
        self.assertEqual(code, 0, err)
        self.assertIn('format: ELF32, little-endian, machine 40 (armv7)', out)
        self.assertIn("android-lib, armv7, a player's build", out)
        for what, data, words in (('an Elf64 file with the arm machine', make_elf_program(machine=40), ('64-bit', 'machine 40 (armv7)', '32-bit')),
                                  ('an Elf32 file with the aarch64 machine', make_elf_program(machine=183, width=32), ('32-bit', 'machine 183 (arm64)', '64-bit'))):
            wrong = self.path('wrong.so')
            put(wrong, data)
            for extra in ((), ('--allow-dev',)):
                code, out, err = run_cli('check-program', wrong, '--platform', 'android-lib', *extra)
                self.assertEqual(code, 1, '%s %s' % (what, extra))
                self.assertIn('REFUSED', err)
                for word in words:
                    self.assertIn(word, err, what)
        code, out, err = run_cli('check-program', good, '--platform', 'linux')
        self.assertEqual(code, 1, 'a 32-bit ARM library is not a Linux program')
        self.assertIn('ELF machine armv7 is not one for linux', err)

    def test_help_says_what_each_subcommand_refuses(self):
        for argv, words in ((['check-program'], ('forbidden string', 'developer tools', 'machine')), (['sums'], ('developer', '-dev')), (['assemble'], ('refuses',)),
                            (['android'], ('refuses', 'debug'))):
            code, out, err = run_cli(*(argv + ['--help']))
            self.assertEqual(code, 0)
            for w in words:
                self.assertIn(w, out, (argv, w))

    def test_not_a_file_and_an_unknown_platform_are_refused(self):
        self.assertEqual(run_cli('check-program', self.path('missing.exe'), '--platform', 'windows')[0], 2)
        code, out, err = run_cli('check-program', self.path('x'), '--platform', 'beos')
        self.assertEqual(code, 2)


# ---- the archives of Windows and Linux ------------------------------------------------------------------------------------------------------

import test_package as tp  # noqa: E402  (the miniature world and its content package: invented names, no file of the original)

NOTICES = {'LICENSE': b'An invented licence text, for a test.\nLine two.\n', 'NOTICE.md': b'# An invented notice\n\nNothing of anyone.\n'}
OTHER_VERSION = '3.4.5'  # the archive follows the repository it is given, not this one


def other_version_block():
    return version_block(product=OTHER_VERSION + '.0', file=OTHER_VERSION + '.0', quad=(3, 4, 5, 0))


def staging_leftovers():
    return sorted(n for n in os.listdir(tempfile.gettempdir()) if n.startswith(('mp-release-stage-', 'mp-release-verify-')))


class ArchiveCase(Temp):
    """A synthetic repository (the miniature world, a CMakeLists.txt that states another version, the engine's shaders), its content package and invented notices."""

    @classmethod
    def setUpClass(cls):
        cls.world = tempfile.mkdtemp(prefix='mp-release-world-')
        cls.repo, cls.ref = tp.build_world(cls.world)
        put(os.path.join(cls.repo, 'CMakeLists.txt'), 'project(MagicPortals VERSION %s LANGUAGES C CXX)\n' % OTHER_VERSION)
        cls.content = os.path.join(cls.repo, 'out', 'dist', 'content')
        with contextlib.redirect_stdout(io.StringIO()):
            code = pk.main(['content', '--repo', cls.repo, '--reference', cls.ref, '--out', cls.content])
        assert code == 0
        cls.notices = os.path.join(cls.world, 'notices')
        for n, d in NOTICES.items():
            put(os.path.join(cls.notices, n), d)

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.world, True)

    def setUp(self):
        super().setUp()
        self.out = self.path('release')
        self.before = staging_leftovers()

    def program(self, name, data):
        path = self.path('programs', name)
        put(path, data)
        return path

    def assemble(self, platform, program, *extra, content=True, out=None):
        argv = ['assemble', '--platform', platform, '--program', program, '--out', out or self.out, '--repo', self.repo, '--notices', self.notices]
        argv += ['--content', self.content] if content else ['--no-data']
        return run_cli(*(argv + list(extra)))

    def windows_program(self, **kw):
        kw.setdefault('version', other_version_block())
        return self.program('MagicPortals.exe', make_pe(**kw))

    def linux_program(self, **kw):
        return self.program('MagicPortals', make_elf_program(**kw))

    def assert_nothing_left(self):
        self.assertFalse(os.path.exists(self.out) and [n for n in os.listdir(self.out) if n != 'SHA256SUMS.txt'], 'no archive and no .part where the refusal happened')
        self.assertEqual(staging_leftovers(), self.before, 'no staging folder is left in the temporary folder')

    def tree_bytes(self, folder):
        got = {}
        for d, _, files in os.walk(folder):
            for n in files:
                with open(os.path.join(d, n), 'rb') as f:
                    got[os.path.relpath(os.path.join(d, n), folder)] = f.read()
        return got


class TheWindowsArchive(ArchiveCase):
    def test_the_layout_the_names_and_the_modes(self):
        program = self.windows_program()
        code, out, err = self.assemble('windows', program)
        self.assertEqual(code, 0, err)
        name = 'MagicPortals-%s-windows-x64.zip' % OTHER_VERSION
        archive = os.path.join(self.out, name)
        self.assertTrue(os.path.isfile(archive), os.listdir(self.out))
        files = rl.read_archive(archive)
        top = name[:-len('.zip')] + '/'
        self.assertTrue(all(n.startswith(top) for n in files))
        rel = sorted(n[len(top):] for n in files)
        self.assertIn('MagicPortals.exe', rel)
        self.assertEqual([n for n in rel if n.startswith('assets/')], ['assets/shaders/one.spv', 'assets/shaders/two.spv'])
        for n in ('PLAYING.txt', 'LICENSE', 'NOTICE.md', 'content/manifest.json', 'content/SHA256SUMS.txt', 'content/levels/lvl_a.tscn'):
            self.assertIn(n, rel)
        self.assertNotIn('shell.json', rel, 'the layout proof of package.py shell is not part of an archive')
        self.assertFalse([n for n in rel if n.endswith(('.vert', '.frag', '.part'))])
        modes = {n[len(top):]: m for n, (_, _, m) in files.items()}
        self.assertEqual(modes['MagicPortals.exe'], 0o755)
        self.assertEqual({m for n, m in modes.items() if n != 'MagicPortals.exe'}, {0o644}, 'every other file is 0644, by rule')
        # the notices are the ones given, byte for byte
        with zipfile.ZipFile(archive) as z:
            self.assertEqual(z.namelist(), sorted(z.namelist()), 'sorted entries')
            for n, d in NOTICES.items():
                self.assertEqual(z.read(top + n), d)
            self.assertEqual({i.date_time for i in z.infolist()}, {ap.ZIP_DATE})
            self.assertEqual({i.create_system for i in z.infolist()}, {3})
        self.assertIn('archive: ' + archive, out)
        self.assertIn("a player's build", out)
        self.assertEqual(rl.verify_sums(self.out), [])
        with open(os.path.join(self.out, 'SHA256SUMS.txt'), encoding='utf-8') as f:
            self.assertEqual(f.read(), '%s  %s\n' % (pk.sha256_of(archive), name))

    def test_the_program_keeps_the_name_the_archive_gives_it_whatever_the_input_was_called(self):
        program = self.program('MagicPortals-built-on-a-friday.exe', make_pe(version=other_version_block()))
        self.assertEqual(self.assemble('windows', program)[0], 0)
        files = rl.read_archive(os.path.join(self.out, 'MagicPortals-%s-windows-x64.zip' % OTHER_VERSION))
        self.assertTrue([n for n in files if n.endswith('/MagicPortals.exe')])
        self.assertFalse([n for n in files if 'friday' in n])

    def test_the_content_inside_is_the_package_and_passes_package_verify(self):
        self.assertEqual(self.assemble('windows', self.windows_program())[0], 0)
        archive = os.path.join(self.out, 'MagicPortals-%s-windows-x64.zip' % OTHER_VERSION)
        scratch = self.path('unpacked')
        top = 'MagicPortals-%s-windows-x64/' % OTHER_VERSION
        self.assertGreater(rl.extract_prefix(archive, top + 'content/', scratch), 10)
        self.assertEqual(pk.verify_content(scratch)['problems'], [])
        self.assertEqual(self.tree_bytes(scratch), self.tree_bytes(self.content), 'the package inside is the package given, byte for byte')

    def test_the_inputs_are_not_changed(self):
        program = self.windows_program()
        with open(program, 'rb') as f:
            program_before = f.read()
        content_before = self.tree_bytes(self.content)
        self.assertEqual(self.assemble('windows', program)[0], 0)
        self.assertEqual(self.tree_bytes(self.content), content_before)
        with open(program, 'rb') as f:
            self.assertEqual(f.read(), program_before)

    def test_the_same_inputs_give_the_same_bytes_from_other_folders_at_other_times(self):
        program = self.windows_program()
        self.assertEqual(self.assemble('windows', program, out=self.path('first'))[0], 0)
        # touch every source: a time or a path that leaked into the archive would show
        for folder in (self.content, os.path.dirname(program), self.notices):
            for d, _, files in os.walk(folder):
                for n in files:
                    os.utime(os.path.join(d, n), (1_700_000_000, 1_700_000_000))
        self.assertEqual(self.assemble('windows', program, out=self.path('second', 'deeper', 'folder'))[0], 0)
        name = 'MagicPortals-%s-windows-x64.zip' % OTHER_VERSION
        a, b = os.path.join(self.path('first'), name), os.path.join(self.path('second', 'deeper', 'folder'), name)
        self.assertEqual(pk.sha256_of(a), pk.sha256_of(b))
        with open(a, 'rb') as fa, open(b, 'rb') as fb:
            self.assertEqual(fa.read(), fb.read())
        self.assertEqual(self.read_text(os.path.join(self.path('first'), 'SHA256SUMS.txt')), self.read_text(os.path.join(self.path('second', 'deeper', 'folder'), 'SHA256SUMS.txt')))

    def read_text(self, path):
        with open(path, encoding='utf-8') as f:
            return f.read()

    def test_the_version_in_the_name_is_the_repositorys(self):
        put(os.path.join(self.repo, 'CMakeLists.txt'), 'project(MagicPortals VERSION 5.6.7 LANGUAGES C CXX)\n')
        try:
            program = self.windows_program(version=version_block(product='5.6.7.0', file='5.6.7.0', quad=(5, 6, 7, 0)))
            self.assertEqual(self.assemble('windows', program)[0], 0)
            self.assertTrue(os.path.isfile(os.path.join(self.out, 'MagicPortals-5.6.7-windows-x64.zip')))
            # and a program that carries the other version is refused for it
            self.assertEqual(self.assemble('windows', self.windows_program(), out=self.path('again'))[0], 2)
        finally:
            put(os.path.join(self.repo, 'CMakeLists.txt'), 'project(MagicPortals VERSION %s LANGUAGES C CXX)\n' % OTHER_VERSION)

    def test_a_release_beside_a_developers_smoke_archive_has_sums_without_it(self):
        dev = self.windows_program(payload=game_payload(dev=True), version=b'')
        self.assertEqual(self.assemble('windows', dev, '--allow-dev')[0], 0)
        code, out, err = self.assemble('windows', self.windows_program())
        self.assertEqual(code, 0, err)
        with open(os.path.join(self.out, 'SHA256SUMS.txt'), encoding='utf-8') as f:
            text = f.read()
        self.assertIn('MagicPortals-%s-windows-x64.zip' % OTHER_VERSION, text)
        self.assertNotIn('-dev', text)
        self.assertEqual(rl.verify_sums(self.out), [])

    def test_no_data_makes_an_archive_without_the_game_and_says_so(self):
        code, out, err = self.assemble('windows', self.windows_program(), content=False)
        self.assertEqual(code, 0, err)
        archive = os.path.join(self.out, 'MagicPortals-%s-windows-x64-nodata.zip' % OTHER_VERSION)
        files = rl.read_archive(archive)
        self.assertFalse([n for n in files if '/content/' in n])
        with zipfile.ZipFile(archive) as z:
            text = z.read([n for n in files if n.endswith('PLAYING.txt')][0]).decode('utf-8')
        self.assertIn('NO GAME DATA', text)
        self.assertIn('none (--no-data)', out)

    def test_exactly_one_of_content_and_no_data(self):
        program = self.windows_program()
        self.assertEqual(run_cli('assemble', '--platform', 'windows', '--program', program, '--out', self.out, '--repo', self.repo, '--notices', self.notices)[0], 2)
        code, out, err = run_cli('assemble', '--platform', 'windows', '--program', program, '--content', self.content, '--no-data', '--out', self.out, '--repo', self.repo,
                                 '--notices', self.notices)
        self.assertEqual(code, 2)
        self.assertIn('exactly one', err)
        self.assert_nothing_left()

    def test_playing_text_says_what_to_run_the_driver_the_saves_and_the_runtime(self):
        self.assertEqual(self.assemble('windows', self.windows_program())[0], 0)
        archive = os.path.join(self.out, 'MagicPortals-%s-windows-x64.zip' % OTHER_VERSION)
        with zipfile.ZipFile(archive) as z:
            text = z.read('MagicPortals-%s-windows-x64/PLAYING.txt' % OTHER_VERSION).decode('utf-8')
        for word in ('Start MagicPortals.exe', 'Vulkan 1.2', 'No Vulkan graphics driver was found', 'vulkan-1.dll', '%APPDATA%\\Magic Portals', 'language.json', 'your language choice', '%TEMP%\\magicportals.log',
                     'magicportals.prev.log', 'does NOT carry it', 'VCRUNTIME140.dll', 'Media Feature Pack', 'has not been seen on a PC without a driver',
                     'were not seen on a PC that lacked', 'LICENSE and NOTICE.md', OTHER_VERSION):
            self.assertIn(word, text)

    def test_the_texts_of_an_archive_hold_none_of_the_words_of_the_list(self):
        # an archive's texts say what it holds and how to use it. The phrases are built from parts (phrase), so that this file holds none of them whole.
        texts = [rl.playing_windows('1.0.0', 'x64', [], True, False), rl.playing_windows('1.0.0', 'x64', ['MSVCP140.dll'], False, True),
                 rl.playing_linux('1.0.0', 'x86_64', {'GLIBC': (2, 38)}, True, False), rl.playing_linux('1.0.0', 'x86_64', {}, False, False),
                 rl.seal_text(rl.MACOS, '1.0.0', 'Contents/Resources/GameData', True), rl.seal_text(rl.IOS, '1.0.0', 'GameData', True),
                 rl.seal_text(rl.MACOS, '1.0.0', 'Contents/Resources/GameData', True, True),
                 rl.intro_text(rl.MACOS, '1.0.0', 'universal', False), rl.intro_text(rl.IOS, '1.0.0', 'arm64', True), rl.developer_note(rl.Report('windows'))]
        words = ('permission', 'permitted', 'grant', phrase('rights', 'holder'), phrase('allowed', 'to', 'ship'), phrase('may', 'be', 'shipped'), phrase('may', 'ship'), 'asantee',
                 'original game', phrase('may', 'leave'), phrase('may', 'travel'), phrase('whether', 'the', 'data'))
        for text in texts:
            for word in words:
                self.assertNotIn(word, text.lower(), word)
        # the tool's own source: the same words are not in it ("permissions" there is a file's mode)
        with open(os.path.join(HERE, 'release.py'), encoding='utf-8') as f:
            source = f.read().lower()
        for word in ('permission to', 'permission from', 'permitted to', 'granted', phrase('rights', 'holder'), phrase('allowed', 'to', 'ship'), phrase('may', 'be', 'shipped'),
                     phrase('may', 'ship'), 'asantee', 'original game', phrase('may', 'leave'), phrase('may', 'travel')):
            self.assertNotIn(word, source, word)
        # what the Apple texts DO say of the owner's decision: the route
        for text in (rl.seal_text(rl.MACOS, '1.0.0', 'Contents/Resources/GameData', True),):
            self.assertIn("Which route", text)
            self.assertIn("is the owner's decision", text)
        # the iOS text says what nobody has done, and no longer leaves the route to the owner (the route was taken: docs/apple.md section 7)
        ios = rl.seal_text(rl.IOS, '1.0.0', 'GameData', True)
        self.assertIn('no sideloading', ios)
        self.assertNotIn("owner's decision", ios)

    def test_the_text_of_an_archive_with_no_game_does_not_promise_a_manifest_or_sums(self):
        for text in (rl.playing_windows('1.0.0', 'x64', [], False, False), rl.playing_linux('1.0.0', 'x86_64', {'GLIBC': (2, 38)}, False, False)):
            self.assertNotIn('manifest.json', text)
            self.assertIn('no content folder and no SHA256SUMS.txt', text)
            self.assertIn('LICENSE and NOTICE.md', text)
        for text in (rl.playing_windows('1.0.0', 'x64', [], True, False), rl.playing_linux('1.0.0', 'x86_64', {'GLIBC': (2, 38)}, True, False)):
            self.assertIn('manifest.json lists every file of the game', text)
            self.assertNotIn('no content folder', text)

    def test_the_texts_say_only_what_is_read_from_the_program_or_a_history_of_runs(self):
        # an archive's text must be true of ANY program it is made for: how a build was made (its distribution, its compiler) is not read from the program, so it is not said
        texts = [rl.playing_windows('1.0.0', 'x64', [], True, False), rl.playing_linux('1.0.0', 'x86_64', {'GLIBC': (2, 38), 'GLIBCXX': (3, 4, 32)}, True, False),
                 rl.playing_linux('1.0.0', 'x86_64', {}, True, False), rl.intro_text(rl.MACOS, '1.0.0', 'universal', False), rl.intro_text(rl.IOS, '1.0.0', 'arm64', False),
                 rl.seal_text(rl.MACOS, '1.0.0', 'Contents/Resources/GameData', True), rl.seal_text(rl.IOS, '1.0.0', 'GameData', True),
                 rl.seal_text(rl.MACOS, '1.0.0', 'Contents/Resources/GameData', True, True)]
        for text in texts:
            for phrase in ('Ubuntu 24.04', 'g++', 'GCC', 'built on', 'was built', 'were built', 'built with', 'notarised and not signed'):
                self.assertNotIn(phrase, text)
        self.assertIn('read from the program', texts[1])
        self.assertIn('has been run on Windows 11', texts[0])

    def test_a_dll_beside_the_program_is_named_in_the_text_and_checked(self):
        redist = self.path('redist')
        for n in ('MSVCP140.dll', 'VCRUNTIME140.dll', 'vcruntime140_1.dll'):
            put(os.path.join(redist, n), make_pe(dll=True, imports=('KERNEL32.dll',), delay=(), payload=b'a runtime'))
        program = self.windows_program(imports=('KERNEL32.dll', 'MSVCP140.dll', 'VCRUNTIME140.dll', 'VCRUNTIME140_1.dll'))
        code, out, err = self.assemble('windows', program, '--vc-redist', redist)
        self.assertEqual(code, 0, err)
        archive = os.path.join(self.out, 'MagicPortals-%s-windows-x64.zip' % OTHER_VERSION)
        top = 'MagicPortals-%s-windows-x64/' % OTHER_VERSION
        files = rl.read_archive(archive)
        for n in ('MSVCP140.dll', 'VCRUNTIME140.dll', 'VCRUNTIME140_1.dll'):
            self.assertIn(top + n, files, 'copied under the name the program imports')
        with zipfile.ZipFile(archive) as z:
            text = z.read(top + 'PLAYING.txt').decode('utf-8')
        self.assertIn('this folder carries the runtime DLLs', text)
        self.assertNotIn('does NOT carry it', text)
        self.assertIn('VCRUNTIME140_1.dll', out)

    def test_the_runtime_folder_is_refused_when_a_dll_is_missing_or_wrong(self):
        program = self.windows_program(imports=('KERNEL32.dll', 'MSVCP140.dll', 'VCRUNTIME140.dll'))
        redist = self.path('redist')
        put(os.path.join(redist, 'MSVCP140.dll'), make_pe(dll=True, imports=(), delay=(), payload=b'x'))
        code, out, err = self.assemble('windows', program, '--vc-redist', redist)
        self.assertEqual(code, 2)
        self.assertIn('VCRUNTIME140.dll is imported by the program and is not in', err)
        self.assert_nothing_left()
        put(os.path.join(redist, 'VCRUNTIME140.dll'), make_pe(machine=0xAA64, dll=True, imports=(), delay=(), payload=b'x'))
        code, out, err = self.assemble('windows', program, '--vc-redist', redist)
        self.assertEqual(code, 2)
        self.assertIn('VCRUNTIME140.dll is for machine 0xaa64', err)
        put(os.path.join(redist, 'VCRUNTIME140.dll'), make_pe(dll=False, imports=(), delay=(), payload=b'x'))
        self.assertIn('is a program, not a DLL', self.assemble('windows', program, '--vc-redist', redist)[2])
        put(os.path.join(redist, 'VCRUNTIME140.dll'), b'not a PE file at all')
        self.assertIn('is not a PE file', self.assemble('windows', program, '--vc-redist', redist)[2])
        self.assert_nothing_left()

    def test_a_runtime_folder_for_a_program_with_a_static_runtime_or_for_linux_is_refused(self):
        redist = self.path('redist')
        os.makedirs(redist)
        code, out, err = self.assemble('windows', self.windows_program(imports=('KERNEL32.dll',)), '--vc-redist', redist)
        self.assertEqual(code, 2)
        self.assertIn('imports no Visual C++ runtime DLL', err)
        code, out, err = self.assemble('linux', self.linux_program(), '--vc-redist', redist)
        self.assertEqual(code, 2)
        self.assertIn('Windows matter', err)

    def test_the_runtime_dll_names(self):
        for good in ('MSVCP140.dll', 'VCRUNTIME140.dll', 'VCRUNTIME140_1.dll', 'vcruntime140.DLL', 'MSVCP140_1.dll', 'CONCRT140.dll', 'VCOMP140.DLL',
                     # the members of the redistributable that carry a word after the number (a C++20 program may import them)
                     'msvcp140_atomic_wait.dll', 'MSVCP140_CODECVT_IDS.dll', 'msvcp140_2.dll', 'vcruntime140_threads.dll', 'VCRUNTIME140_1.dll'):
            self.assertTrue(rl.RUNTIME_DLL.match(good), good)
        for bad in ('KERNEL32.dll', 'api-ms-win-crt-runtime-l1-1-0.dll', 'vulkan-1.dll', 'MFPlat.DLL', 'MSVCRT.dll', 'UCRTBASE.dll', 'msvcp_win.dll', 'MSVCP.dll', 'msvcp140_atomic_wait.exe',
                    'msvcp140.dll.bak', 'my_msvcp140.dll'):
            self.assertFalse(rl.RUNTIME_DLL.match(bad), bad)

    def test_a_runtime_dll_with_a_word_after_its_number_is_copied_named_and_checked(self):
        # msvcp140_atomic_wait.dll is in Microsoft's redistributable and a program that uses std::atomic::wait imports it: packaged without it, PLAYING.txt would say
        # that the folder carries the runtime the program loads and the player's PC would show Windows' own message
        redist = self.path('redist')
        names = ('MSVCP140.dll', 'msvcp140_atomic_wait.dll', 'VCRUNTIME140.dll')
        program = self.windows_program(imports=('KERNEL32.dll', 'msvcp140_atomic_wait.dll', 'MSVCP140.dll', 'VCRUNTIME140.dll'))
        for n in names[:2]:
            put(os.path.join(redist, n), make_pe(dll=True, imports=('KERNEL32.dll',), delay=(), payload=b'a runtime ' + n.encode()))
        code, out, err = self.assemble('windows', program, '--vc-redist', redist)
        self.assertEqual(code, 2, 'a runtime DLL the program imports and the folder lacks is refused')
        self.assertIn('VCRUNTIME140.dll is imported by the program and is not in', err)
        self.assert_nothing_left()
        put(os.path.join(redist, 'VCRUNTIME140.dll'), make_pe(dll=True, imports=('KERNEL32.dll',), delay=(), payload=b'a runtime'))
        os.remove(os.path.join(redist, 'msvcp140_atomic_wait.dll'))
        code, out, err = self.assemble('windows', program, '--vc-redist', redist)
        self.assertEqual(code, 2)
        self.assertIn('msvcp140_atomic_wait.dll is imported by the program and is not in', err, 'the new name is looked for, and named when missing')
        self.assert_nothing_left()
        put(os.path.join(redist, 'msvcp140_atomic_wait.dll'), make_pe(dll=True, imports=('KERNEL32.dll',), delay=(), payload=b'an atomic wait'))
        code, out, err = self.assemble('windows', program, '--vc-redist', redist)
        self.assertEqual(code, 0, err)
        archive = os.path.join(self.out, 'MagicPortals-%s-windows-x64.zip' % OTHER_VERSION)
        top = 'MagicPortals-%s-windows-x64/' % OTHER_VERSION
        files = rl.read_archive(archive)
        for n in names:
            self.assertIn(top + n, files, '%s is in the archive' % n)
        with zipfile.ZipFile(archive) as z:
            text = z.read(top + 'PLAYING.txt').decode('utf-8')
        self.assertIn('msvcp140_atomic_wait.dll', text, 'and PLAYING.txt names it')


class TheLinuxArchive(ArchiveCase):
    def archive(self):
        return os.path.join(self.out, 'MagicPortals-%s-linux-x86_64.tar.gz' % OTHER_VERSION)

    def test_the_layout_the_modes_and_no_owner(self):
        code, out, err = self.assemble('linux', self.linux_program())
        self.assertEqual(code, 0, err)
        top = 'MagicPortals-%s-linux-x86_64/' % OTHER_VERSION
        with tarfile.open(self.archive(), 'r:gz') as tf:
            members = tf.getmembers()
            names = [m.name for m in members]
            self.assertEqual(names[0], top.rstrip('/'), 'the top folder comes first')
            self.assertEqual([m.name for m in members if m.isdir()], sorted(m.name for m in members if m.isdir()))
            for m in members:
                self.assertEqual((m.uid, m.gid, m.uname, m.gname, m.mtime), (0, 0, '', '', pk.FIXED_TIME), m.name)
                self.assertEqual(m.mode, 0o755 if (m.isdir() or m.name == top + 'MagicPortals') else 0o644, m.name)
            self.assertIn(top + 'MagicPortals', names)
            self.assertIn(top + 'assets/shaders/one.spv', names)
            self.assertIn(top + 'content/manifest.json', names)
            self.assertIn(top + 'LICENSE', names)
            self.assertFalse([n for n in names if n.endswith('shell.json')])
        with open(self.archive(), 'rb') as f:
            header = f.read(10)
        self.assertEqual(header[:4], b'\x1f\x8b\x08\x00', 'gzip, with no file-name flag')
        self.assertEqual(header[4:8], b'\0\0\0\0', 'and no time in the gzip header')

    def test_the_same_bytes_from_other_folders_at_other_times(self):
        program = self.linux_program()
        self.assertEqual(self.assemble('linux', program, out=self.path('first'))[0], 0)
        for d, _, files in os.walk(self.content):
            for n in files:
                os.utime(os.path.join(d, n), (1_650_000_000, 1_650_000_000))
        os.utime(program, (1_650_000_000, 1_650_000_000))
        self.assertEqual(self.assemble('linux', program, out=self.path('x', 'y'))[0], 0)
        name = 'MagicPortals-%s-linux-x86_64.tar.gz' % OTHER_VERSION
        with open(os.path.join(self.path('first'), name), 'rb') as fa, open(os.path.join(self.path('x', 'y'), name), 'rb') as fb:
            self.assertEqual(fa.read(), fb.read())

    def test_playing_text_names_the_glibc_the_program_asks_for_and_the_executable_bit(self):
        self.assertEqual(self.assemble('linux', self.linux_program(glibc=('2.34', '2.38'), glibcxx=('3.4.32',)))[0], 0)
        with tarfile.open(self.archive(), 'r:gz') as tf:
            text = tf.extractfile('MagicPortals-%s-linux-x86_64/PLAYING.txt' % OTHER_VERSION).read().decode('utf-8')
        for word in ('tar xzf', 'chmod +x MagicPortals', 'glibc 2.38 or newer', 'GLIBCXX 3.4.32', 'libvulkan.so.1', 'libasound.so.2', '~/.local/share/Magic Portals', 'your language choice',
                     '$XDG_DATA_HOME', 'magicportals.log', 'GLFW could not report the required Vulkan instance extensions', 'VK_DRIVER_FILES', 'not on a PC without one', 'X11'):
            self.assertIn(word, text)

    def test_a_windows_program_is_not_a_linux_archive(self):
        code, out, err = self.assemble('linux', self.windows_program())
        self.assertEqual(code, 2)
        self.assertIn('not the program of this platform (linux)', err)
        self.assert_nothing_left()


class TheRefusalsOfAnArchive(ArchiveCase):
    def test_a_developers_program_is_refused_with_every_reason_and_nothing_is_written(self):
        program = self.windows_program(payload=game_payload(dev=True, extra=[CHECKOUT_PATH_OF_A_BUILDER]), version=b'')
        code, out, err = self.assemble('windows', program)
        self.assertEqual(code, 2)
        for word in ('check-program refuses', 'the name of the account that runs this check', 'developer tools are compiled in', 'no version information'):
            self.assertIn(word, err)
        self.assert_nothing_left()

    def test_allow_dev_makes_a_dev_archive_that_says_so_everywhere(self):
        program = self.windows_program(payload=game_payload(dev=True), version=b'')
        code, out, err = self.assemble('windows', program, '--allow-dev')
        self.assertEqual(code, 0, err)
        name = 'MagicPortals-%s-windows-x64-dev.zip' % OTHER_VERSION
        archive = os.path.join(self.out, name)
        self.assertTrue(os.path.isfile(archive))
        top = name[:-4] + '/'
        files = rl.read_archive(archive)
        self.assertIn(top + 'DEVELOPER BUILD.txt', files)
        with zipfile.ZipFile(archive) as z:
            self.assertIn('DEVELOPER BUILD', z.read(top + 'DEVELOPER BUILD.txt').decode())
            self.assertIn('developer tools are compiled in', z.read(top + 'DEVELOPER BUILD.txt').decode())
            self.assertTrue(z.read(top + 'PLAYING.txt').decode().startswith('*** THIS IS A DEVELOPER BUILD'))
        self.assertIn('DEVELOPER BUILD', out)
        self.assertFalse(os.path.exists(os.path.join(self.out, 'SHA256SUMS.txt')), 'a developer\'s archive is not in a release folder\'s sums')

    def test_a_wrong_machine_is_refused_even_with_allow_dev(self):
        code, out, err = self.assemble('windows', self.windows_program(machine=0xAA64), '--allow-dev')
        self.assertEqual(code, 2)
        self.assert_nothing_left()

    def test_content_that_fails_verify_is_refused(self):
        copy = self.path('content-copy')
        shutil.copytree(self.content, copy)
        with open(os.path.join(copy, 'levels', 'lvl_b.tscn'), 'ab') as f:
            f.write(b'tampered')
        code, out, err = run_cli('assemble', '--platform', 'windows', '--program', self.windows_program(), '--content', copy, '--out', self.out, '--repo', self.repo,
                                 '--notices', self.notices)
        self.assertEqual(code, 2)
        self.assertIn('fails package.py verify', err)
        self.assertIn('size differs: levels/lvl_b.tscn', err)
        self.assert_nothing_left()

    def test_a_content_package_with_a_file_outside_its_manifest_is_refused(self):
        copy = self.path('content-copy')
        shutil.copytree(self.content, copy)
        put(os.path.join(copy, 'original', 'sprites', 'stray_a.png'), b'a stray file')
        code, out, err = run_cli('assemble', '--platform', 'linux', '--program', self.linux_program(), '--content', copy, '--out', self.out, '--repo', self.repo,
                                 '--notices', self.notices)
        self.assertEqual(code, 2)
        self.assertIn('on disk but not in the manifest: original/sprites/stray_a.png', err)
        self.assert_nothing_left()

    def test_a_missing_or_empty_notice_is_refused_naming_it(self):
        for missing in NOTICES:
            folder = self.path('notices-' + missing)
            for n, d in NOTICES.items():
                if n != missing:
                    put(os.path.join(folder, n), d)
            code, out, err = run_cli('assemble', '--platform', 'windows', '--program', self.windows_program(), '--content', self.content, '--out', self.out, '--repo', self.repo,
                                     '--notices', folder)
            self.assertEqual(code, 2)
            self.assertIn('no ' + missing, err)
            self.assertIn('LICENSE and NOTICE.md are at the root of the repository', err)
        empty = self.path('notices-empty')
        put(os.path.join(empty, 'LICENSE'), b'  \n')
        put(os.path.join(empty, 'NOTICE.md'), NOTICES['NOTICE.md'])
        code, out, err = run_cli('assemble', '--platform', 'windows', '--program', self.windows_program(), '--content', self.content, '--out', self.out, '--repo', self.repo,
                                 '--notices', empty)
        self.assertEqual(code, 2)
        self.assertIn('LICENSE (it is empty)', err)
        self.assert_nothing_left()

    def test_the_repository_root_is_where_the_notices_are_looked_for_by_default(self):
        code, out, err = run_cli('assemble', '--platform', 'windows', '--program', self.windows_program(), '--content', self.content, '--out', self.out, '--repo', self.repo)
        self.assertEqual(code, 2)
        self.assertIn('has no LICENSE and no NOTICE.md', err)
        self.assertIn(self.repo, err)
        for n, d in NOTICES.items():
            put(os.path.join(self.repo, n), d)
        try:
            code, out, err = run_cli('assemble', '--platform', 'windows', '--program', self.windows_program(), '--content', self.content, '--out', self.out, '--repo', self.repo)
            self.assertEqual(code, 0, err)
        finally:
            for n in NOTICES:
                os.remove(os.path.join(self.repo, n))

    def test_an_output_in_the_tracked_tree_or_in_a_folder_it_reads_is_refused(self):
        program = self.windows_program()
        for bad in (os.path.join(self.repo, 'tools', 'release'), os.path.join(self.repo, 'data'), self.repo, os.path.join(self.content, 'inside'), self.content):
            code, out, err = self.assemble('windows', program, out=bad)
            self.assertEqual(code, 2, bad)
            self.assertFalse([n for d, _, files in os.walk(bad) for n in files if n.startswith('MagicPortals-')], bad)
        self.assertFalse([n for n in os.listdir(os.path.join(self.repo, 'tools')) if n.startswith('MagicPortals')])
        under_out = os.path.join(self.repo, 'out', 'release')
        try:
            self.assertEqual(self.assemble('windows', program, out=under_out)[0], 0, 'under the gitignored out/ is fine')
        finally:
            shutil.rmtree(under_out, True)

    def test_an_archive_that_exists_is_never_written_over(self):
        program = self.windows_program()
        self.assertEqual(self.assemble('windows', program)[0], 0)
        archive = os.path.join(self.out, 'MagicPortals-%s-windows-x64.zip' % OTHER_VERSION)
        before = pk.sha256_of(archive)
        code, out, err = self.assemble('windows', program)
        self.assertEqual(code, 2)
        self.assertIn('already exists', err)
        self.assertEqual(pk.sha256_of(archive), before)
        self.assertEqual(sorted(os.listdir(self.out)), ['MagicPortals-%s-windows-x64.zip' % OTHER_VERSION, 'SHA256SUMS.txt'])

    def test_a_linked_program_is_refused(self):
        program = self.windows_program()
        link = self.path('programs', 'linked.exe')
        try:
            os.symlink(program, link)
        except (OSError, NotImplementedError):
            self.skipTest('this system will not make a symbolic link for the test: the refusal of a linked program is NOT checked on it')
        code, out, err = self.assemble('windows', link)
        self.assertEqual(code, 2)
        self.assertIn('not a plain file', err)

    def test_platform_and_input_mismatches_are_refused_by_name(self):
        code, out, err = run_cli('assemble', '--platform', 'windows', '--shell', self.path('x.zip'), '--out', self.out, '--repo', self.repo, '--notices', self.notices)
        self.assertEqual(code, 2)
        self.assertIn('--shell is for macos and ios', err)
        code, out, err = run_cli('assemble', '--platform', 'windows', '--content', self.content, '--out', self.out, '--repo', self.repo, '--notices', self.notices)
        self.assertEqual(code, 2)
        self.assertIn('--program', err)
        code, out, err = run_cli('assemble', '--platform', 'macos', '--program', self.windows_program(), '--out', self.out, '--repo', self.repo)
        self.assertEqual(code, 2)
        self.assertIn('windows and linux', err)

    def test_the_release_is_refused_when_the_shaders_are_missing(self):
        code, out, err = self.assemble('windows', self.windows_program(), '--shaders', self.path('no-shaders'))
        self.assertEqual(code, 2)
        self.assert_nothing_left()

    def test_the_reread_catches_an_archive_that_is_not_what_was_meant(self):
        original = rl.write_zip

        def short(path, entries, **kw):
            original(path, entries[:-1], **kw)  # the last entry is lost on the way to the disk
        rl.write_zip = short
        try:
            code, out, err = self.assemble('windows', self.windows_program())
        finally:
            rl.write_zip = original
        self.assertEqual(code, 2)
        self.assertIn('fails its own re-read', err)
        self.assertIn('missing from the archive', err)
        self.assert_nothing_left()

    def test_a_failure_after_staging_leaves_no_staging_folder_and_no_part_file(self):
        original = rl.write_zip

        def broken(path, entries, **kw):
            original(path, entries, **kw)
            raise ReleaseErrorForTest('the disk filled up')
        rl.write_zip = broken
        try:
            with self.assertRaises(ReleaseErrorForTest):
                rl.assemble_desktop(rl.build_parser().parse_args(['assemble', '--platform', 'windows', '--program', self.windows_program(), '--content', self.content,
                                                                  '--out', self.out, '--repo', self.repo, '--notices', self.notices]))
        finally:
            rl.write_zip = original
        self.assertEqual(staging_leftovers(), self.before)
        self.assertEqual(os.listdir(self.out), [], 'neither the .part file nor a finished archive')


class ReleaseErrorForTest(Exception):
    pass


class TheNamesAndTheSums(Temp):
    def test_archive_names(self):
        self.assertEqual(rl.archive_name('1.2.3', 'windows', 'x64'), 'MagicPortals-1.2.3-windows-x64.zip')
        self.assertEqual(rl.archive_name('1.2.3', 'linux', 'x86_64', ext='.tar.gz'), 'MagicPortals-1.2.3-linux-x86_64.tar.gz')
        self.assertEqual(rl.archive_name('1.2.3', 'linux', 'x86_64', 'nodata', True, '.tar.gz'), 'MagicPortals-1.2.3-linux-x86_64-nodata-dev.tar.gz')
        self.assertEqual(rl.archive_name('1.2.3', 'android', None, ext='.apk'), 'MagicPortals-1.2.3-android.apk')
        for name in ('MagicPortals-1.2.3-windows-x64.zip', 'MagicPortals-1.2.3-linux-x86_64.tar.gz', 'MagicPortals-1.2.3-ios-arm64.ipa', 'MagicPortals-1.2.3-android.apk',
                     'MagicPortals-1.2.3-ios-arm64-simulator-shell.zip'):
            self.assertTrue(rl.ARTIFACT.match(name), name)
        for name in ('MagicPortals-1.2-windows-x64.zip', 'readme.txt', 'MagicPortals-1.2.3-windows-x64.exe', 'SHA256SUMS.txt'):
            self.assertFalse(rl.ARTIFACT.match(name), name)

    def fill(self, *names):
        for n in names:
            put(self.path(n), ('bytes of ' + n).encode())

    def test_sums_are_written_in_sha256sum_form_and_verified(self):
        self.fill('MagicPortals-1.2.3-windows-x64.zip', 'MagicPortals-1.2.3-linux-x86_64.tar.gz', 'LICENSE', 'notes.txt')
        code, out, err = run_cli('sums', self._tmp)
        self.assertEqual(code, 0, err)
        with open(self.path('SHA256SUMS.txt'), encoding='utf-8') as f:
            lines = f.read().splitlines()
        self.assertEqual([l.split('  ')[1] for l in lines], ['LICENSE', 'MagicPortals-1.2.3-linux-x86_64.tar.gz', 'MagicPortals-1.2.3-windows-x64.zip'], 'sorted, and notes.txt is no release file')
        for l in lines:
            digest, name = l.split('  ')
            self.assertEqual(digest, hashlib.sha256(('bytes of ' + name).encode()).hexdigest())
        self.assertEqual(run_cli('sums', self._tmp, '--check')[0], 0)

    def test_check_finds_a_changed_a_missing_and_an_unlisted_file(self):
        self.fill('MagicPortals-1.2.3-windows-x64.zip', 'MagicPortals-1.2.3-linux-x86_64.tar.gz')
        run_cli('sums', self._tmp)
        put(self.path('MagicPortals-1.2.3-windows-x64.zip'), b'changed')
        os.remove(self.path('MagicPortals-1.2.3-linux-x86_64.tar.gz'))
        self.fill('MagicPortals-1.2.3-android.apk')
        problems = rl.verify_sums(self._tmp)
        text = '\n'.join(problems)
        self.assertIn('the sha256 of MagicPortals-1.2.3-windows-x64.zip differs', text)
        self.assertIn('MagicPortals-1.2.3-linux-x86_64.tar.gz is listed and is not a file', text)
        self.assertIn('MagicPortals-1.2.3-android.apk is in the folder and not in SHA256SUMS.txt', text)
        code, out, err = run_cli('sums', self._tmp, '--check')
        self.assertEqual(code, 1)
        self.assertIn('FAILED, 3 problem(s)', out)

    def test_a_developers_archive_is_not_in_a_release_folders_sums(self):
        self.fill('MagicPortals-1.2.3-windows-x64-dev.zip')
        code, out, err = run_cli('sums', self._tmp)
        self.assertEqual(code, 2)
        self.assertIn('developer', err)
        self.assertFalse(os.path.exists(self.path('SHA256SUMS.txt')))
        self.assertEqual(run_cli('sums', self._tmp, '--allow-dev')[0], 0)

    def test_a_shell_is_not_a_release_either_and_an_assembly_leaves_both_out_of_its_sums(self):
        self.fill('MagicPortals-1.2.3-macos-universal-shell.zip')
        self.assertEqual(run_cli('sums', self._tmp)[0], 2)
        self.fill('MagicPortals-1.2.3-linux-x86_64-dev.tar.gz', 'MagicPortals-1.2.3-linux-x86_64.tar.gz')
        self.assertEqual(rl.write_sums(self._tmp, skip_dev=True), ['MagicPortals-1.2.3-linux-x86_64.tar.gz'])
        self.assertEqual(rl.verify_sums(self._tmp), [], 'and the check does not ask for the ones that are not releases')

    def test_an_empty_folder_and_a_missing_one_are_refused(self):
        self.assertEqual(run_cli('sums', self._tmp)[0], 2)
        self.assertEqual(run_cli('sums', self.path('nowhere'))[0], 2)
        self.assertEqual(run_cli('sums', self._tmp, '--check')[0], 1)

    def test_a_listed_name_with_a_path_is_not_followed(self):
        self.fill('MagicPortals-1.2.3-windows-x64.zip')
        put(self.path('SHA256SUMS.txt'), '%s  ../outside.txt\nnot a line\n' % ('0' * 64))
        text = '\n'.join(rl.verify_sums(self._tmp))
        self.assertIn('../outside.txt is listed and is not a file of this folder', text)
        self.assertIn('a line that is not', text)


# ---- the Apple layouts ------------------------------------------------------------------------------------------------------------------------

import plistlib  # noqa: E402


def bundle_plist(executable='MagicPortals', platforms=None):
    doc = {'CFBundleExecutable': executable, 'CFBundleIdentifier': 'com.example.invented', 'CFBundleShortVersionString': OTHER_VERSION}
    if platforms:
        doc['CFBundleSupportedPlatforms'] = platforms
    return plistlib.dumps(doc)


def shell_files(kind, program=None, readme=True):
    """The files of a shell as make_app.sh's zips hold them (invented bytes, a real plist, a header-only executable): {name: (bytes, mode)}."""
    if kind == 'macos':
        base = 'MagicPortals/MagicPortals.app/'
        files = {base + 'Contents/Info.plist': (bundle_plist(), 0o644), base + 'Contents/MacOS/MagicPortals': (program or make_fat(), 0o755),
                 base + 'Contents/Resources/engine/assets/shaders/a.spv': (b'spv-a', 0o644), base + 'Contents/Resources/licenses/README.txt': (b'the shell\'s own note', 0o644)}
        if readme:
            files['MagicPortals/READ ME FIRST.txt'] = (b'This is the Magic Portals program WITHOUT the game: no levels.\n', 0o644)
        return files
    base = 'Payload/MagicPortals.app/' if kind == 'ios' else 'MagicPortals.app/'
    return {base + 'Info.plist': (bundle_plist(platforms=['iPhoneOS'] if kind == 'ios' else ['iPhoneSimulator']), 0o644),
            base + 'MagicPortals': (program or make_macho(0x0100000C), 0o755), base + 'engine/assets/shaders/a.spv': (b'spv-a', 0o644)}


def write_shell_zip(path, files):
    """A zip the way ditto and zip -r write one: folder entries first, the mode word with the file type, the executable bit kept."""
    folders = sorted({'/'.join(n.split('/')[:i]) + '/' for n in files for i in range(1, len(n.split('/')))})
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with zipfile.ZipFile(path, 'w') as z:
        for f in folders:
            info = zipfile.ZipInfo(f, date_time=(2026, 10, 25, 22, 0, 0))
            info.create_system, info.external_attr = 3, (0o040755 << 16) | 0x10
            z.writestr(info, b'')
        for name in sorted(files):
            data, mode = files[name]
            info = zipfile.ZipInfo(name, date_time=(2026, 10, 25, 22, 0, 0))
            info.create_system, info.external_attr = 3, (0o100000 | mode) << 16
            z.writestr(info, data)
    return path


class TheAppleLayouts(ArchiveCase):
    def shell(self, kind, **kw):
        return write_shell_zip(self.path('shells', kind + '.zip'), shell_files(kind, **kw))

    def apple(self, platform, shell, *extra, content=True, out=None):
        argv = ['assemble', '--platform', platform, '--shell', shell, '--out', out or self.out, '--repo', self.repo, '--notices', self.notices]
        argv += ['--content', self.content] if content else []
        return run_cli(*(argv + list(extra)))

    def test_the_data_goes_into_the_folder_the_game_looks_in_on_a_mac(self):
        code, out, err = self.apple('macos', self.shell('macos'))
        self.assertEqual(code, 0, err)
        archive = os.path.join(self.out, 'MagicPortals-%s-macos-universal.zip' % OTHER_VERSION)
        files = rl.read_archive(archive)
        base = 'MagicPortals/MagicPortals.app/Contents/Resources/GameData/'
        for n in ('manifest.json', 'levels/lvl_a.tscn', 'tables/chapters.json', 'original/soundfx/boom_a.mp3', 'data/items.json', 'port-data/engine.json' if False else 'port-data/ui.json'):
            self.assertIn(base + n, files, n)
        self.assertEqual({n for n in files if '/GameData/' in n}, {base + n[len('content/'):] for n in
                                                                    ['content/' + p for p, _ in pk.listing(self.content)]}, 'exactly the package, nothing else, nothing left out')
        # the shell is otherwise what it was: the program with its exec bit, the plist, the shaders, the licences folder
        modes = {n: m for n, (_, _, m) in files.items()}
        self.assertEqual(modes['MagicPortals/MagicPortals.app/Contents/MacOS/MagicPortals'], 0o755)
        self.assertEqual(modes[base + 'levels/lvl_a.tscn'], 0o644)
        for n in ('MagicPortals/MagicPortals.app/Contents/Info.plist', 'MagicPortals/MagicPortals.app/Contents/Resources/engine/assets/shaders/a.spv',
                  'MagicPortals/MagicPortals.app/Contents/Resources/licenses/README.txt'):
            self.assertIn(n, files)
        # the notices are beside the app, not inside it
        self.assertIn('MagicPortals/LICENSE', files)
        self.assertIn('MagicPortals/NOTICE.md', files)
        self.assertFalse([n for n in files if n.startswith('MagicPortals/MagicPortals.app/') and n.endswith(('LICENSE', 'NOTICE.md')) and 'licenses/' not in n])
        self.assertEqual(rl.verify_sums(self.out), [])

    def test_the_seal_is_said_in_the_archive_and_in_the_output_and_names_only_what_section_7_names(self):
        code, out, err = self.apple('macos', self.shell('macos'))
        self.assertEqual(code, 0, err)
        archive = os.path.join(self.out, 'MagicPortals-%s-macos-universal.zip' % OTHER_VERSION)
        with zipfile.ZipFile(archive) as z:
            text = z.read('MagicPortals/READ ME FIRST.txt').decode('utf-8')
        for word in ('SEAL IS NO LONGER VALID', 'INVALID', 'must be made again', 'CANNOT sign', 'has ever been run on a Mac', 'docs/apple.md section 7', 'rcodesign',
                     'Application Support/Magic Portals/GameData', "the owner's decision", 'Contents/Resources/GameData', 'is damaged'):
            self.assertIn(word, text, word)
            self.assertIn(word.split('\n')[0], out, 'the same words are printed: ' + word) if word in ('INVALID', 'rcodesign', 'CANNOT sign') else None
        self.assertNotIn('WITHOUT the game', text, 'the shell\'s own READ ME is replaced: it says there is no game')
        self.assertIn('with the game inside the app', text.lower())
        for invented in ('notarytool', 'productsign', 'altool', 'stapler', 'spctl', 'xattr', 'Developer ID', 'fastlane'):
            self.assertNotIn(invented, text, 'only the routes docs/apple.md section 7 names')
        self.assertIn('This text is also in the archive', out)

    def test_a_resealed_mac_archive_says_it_was_signed_again_and_says_no_more_than_was_checked(self):
        # --resealed is what .github/workflows/apple-release.yml passes: it signs the app again on a Mac and fails unless codesign is satisfied, so the archive may say the seal was remade
        code, out, err = self.apple('macos', self.shell('macos'), '--resealed')
        self.assertEqual(code, 0, err)
        archive = os.path.join(self.out, 'MagicPortals-%s-macos-universal.zip' % OTHER_VERSION)
        with zipfile.ZipFile(archive) as z:
            text = z.read('MagicPortals/READ ME FIRST.txt').decode('utf-8')
        for word in ('SIGNED AGAIN AFTER THE DATA WENT IN', 'codesign --force --sign - --timestamp=none', 'codesign --verify --deep --strict --verbose=2', 'WHAT THIS IS NOT',
                     'not notarised', 'Open Anyway', 'xattr -dr com.apple.quarantine MagicPortals.app', 'None of this has been tried with this app', 'real Mac',
                     'macOS 13.3 or newer', 'Contents/Resources/GameData'):
            self.assertIn(word, text, word)
            self.assertIn(word, out, 'the same words are printed: ' + word) if word == 'WHAT THIS IS NOT' else None
        for gone in ('INVALID', 'SEAL IS NO LONGER VALID', 'CANNOT sign', 'is damaged', "the owner's decision", 'WITHOUT the game'):
            self.assertNotIn(gone, text, gone)
        # the intro of that archive does not say that the packager made it on a PC, and the text says what the workflow does after the tool has run
        for word in ('assembled on a GitHub macOS machine', "person's Mac", 'is expected to stop it the first time', 'starts the program inside the finished app'):
            self.assertIn(word, text, word)
        self.assertNotIn('cannot sign or notarise (it runs on a PC)', text)
        # the words of the list that no archive text may hold (the same ones the other Apple texts are held to)
        for word in ('permission', 'permitted', 'grant', 'asantee', 'original game', 'built on', 'was built', 'were built', 'built with', 'Ubuntu', 'g++', 'GCC'):
            self.assertNotIn(word, text.lower() if word.islower() else text, word)
        # the same bytes otherwise: the archive is the one the plain assembly writes, the READ ME apart
        plain = os.path.join(self.path('plain'), 'MagicPortals-%s-macos-universal.zip' % OTHER_VERSION)
        code2, _, err2 = self.apple('macos', self.shell('macos'), out=self.path('plain'))
        self.assertEqual(code2, 0, err2)
        a, b = rl.read_archive(archive), rl.read_archive(plain)
        self.assertEqual(sorted(a), sorted(b))
        self.assertEqual([n for n in a if a[n][0] != b[n][0]], ['MagicPortals/READ ME FIRST.txt'])

    def test_resealed_is_refused_for_ios_and_for_a_shell_with_no_data(self):
        code, _, err = self.apple('ios', self.shell('ios'), '--resealed')
        self.assertEqual(code, 2)
        self.assertIn('--resealed is for a macos archive with the game in it', err)
        code, _, err = self.apple('macos', self.shell('macos'), '--resealed', content=False)
        self.assertEqual(code, 2)
        self.assertIn('--resealed is for a macos archive with the game in it', err)
        self.assertFalse(os.path.exists(os.path.join(self.out, 'MagicPortals-%s-macos-universal.zip' % OTHER_VERSION)), 'nothing is written on a refusal')

    def test_the_ios_data_goes_to_the_root_of_the_bundle_and_the_ipa_starts_with_payload(self):
        code, out, err = self.apple('ios', self.shell('ios'))
        self.assertEqual(code, 0, err)
        archive = os.path.join(self.out, 'MagicPortals-%s-ios-arm64.ipa' % OTHER_VERSION)
        with zipfile.ZipFile(archive) as z:
            names = z.namelist()
            self.assertEqual(names[0], 'Payload/', 'the first entry, as make_app.sh checks of its own ipa')
            self.assertIn('Payload/MagicPortals.app/', names)
            text = z.read('READ ME FIRST.txt').decode('utf-8')
        files = rl.read_archive(archive)
        base = 'Payload/MagicPortals.app/GameData/'
        self.assertIn(base + 'levels/lvl_a.tscn', files)
        self.assertIn(base + 'manifest.json', files)
        self.assertEqual(files['Payload/MagicPortals.app/MagicPortals'][2], 0o755)
        self.assertIn('LICENSE', files)
        self.assertIn('NOTICE.md', files)
        self.assertIn('signs the whole', text)
        self.assertIn('never run', text)
        self.assertIn('inferred', text)
        self.assertNotIn('seal is', text.lower().replace('ad-hoc signature', ''), 'iOS has no seal to break: a sideloading tool signs the whole bundle')

    def test_a_simulator_bundle_is_named_for_it(self):
        code, out, err = self.apple('ios', self.shell('sim'))
        self.assertEqual(code, 0, err)
        archive = os.path.join(self.out, 'MagicPortals-%s-ios-arm64-simulator.zip' % OTHER_VERSION)
        files = rl.read_archive(archive)
        self.assertIn('MagicPortals.app/GameData/levels/lvl_a.tscn', files)

    def test_the_apple_archives_are_the_same_bytes_twice(self):
        for kind, platform, name in (('macos', 'macos', 'MagicPortals-%s-macos-universal.zip'), ('ios', 'ios', 'MagicPortals-%s-ios-arm64.ipa')):
            shell = self.shell(kind)
            self.assertEqual(self.apple(platform, shell, out=self.path('a', kind))[0], 0)
            for d, _, files in os.walk(self.content):
                for n in files:
                    os.utime(os.path.join(d, n), (1_600_000_000, 1_600_000_000))
            os.utime(shell, (1_600_000_000, 1_600_000_000))
            self.assertEqual(self.apple(platform, shell, out=self.path('b', 'deeper', kind))[0], 0)
            with open(os.path.join(self.path('a', kind), name % OTHER_VERSION), 'rb') as fa, open(os.path.join(self.path('b', 'deeper', kind), name % OTHER_VERSION), 'rb') as fb:
                self.assertEqual(fa.read(), fb.read(), kind)

    def test_the_content_inside_the_apple_archive_is_the_package_and_verifies(self):
        self.assertEqual(self.apple('macos', self.shell('macos'))[0], 0)
        archive = os.path.join(self.out, 'MagicPortals-%s-macos-universal.zip' % OTHER_VERSION)
        scratch = self.path('unpacked')
        self.assertGreater(rl.extract_prefix(archive, 'MagicPortals/MagicPortals.app/Contents/Resources/GameData/', scratch), 10)
        self.assertEqual(pk.verify_content(scratch)['problems'], [])
        self.assertEqual(self.tree_bytes(scratch), self.tree_bytes(self.content))

    def test_without_content_the_shell_passes_through_byte_for_byte_under_its_new_name(self):
        shell = self.shell('macos')
        with open(shell, 'rb') as f:
            original = f.read()
        code, out, err = self.apple('macos', shell, content=False, out=self.out)
        self.assertEqual(code, 0, err)
        archive = os.path.join(self.out, 'MagicPortals-%s-macos-universal-shell.zip' % OTHER_VERSION)
        with open(archive, 'rb') as f:
            self.assertEqual(f.read(), original)
        self.assertIn('NO game data', out)
        self.assertIn('byte for byte', out)
        # no notices are asked for, and none is added: a shell is not a release
        code, out, err = run_cli('assemble', '--platform', 'ios', '--shell', self.shell('ios'), '--out', self.out, '--repo', self.repo)
        self.assertEqual(code, 0, err)
        self.assertTrue(os.path.isfile(os.path.join(self.out, 'MagicPortals-%s-ios-arm64-shell.ipa' % OTHER_VERSION)))

    def test_a_shell_whose_program_is_a_developers_build_is_refused(self):
        shell = write_shell_zip(self.path('shells', 'dev.zip'), shell_files('macos', program=make_fat(payload=game_payload(dev=True, extra=[b'/Users/someone/work/x']))))
        code, out, err = self.apple('macos', shell)
        self.assertEqual(code, 2)
        self.assertIn('developer tools are compiled in', err)
        self.assertIn("'/Users/'", err)
        self.assert_nothing_left()
        code, out, err = self.apple('macos', shell, '--allow-dev')
        self.assertEqual(code, 0, err)
        self.assertTrue(os.path.isfile(os.path.join(self.out, 'MagicPortals-%s-macos-universal-dev.zip' % OTHER_VERSION)))

    def test_a_shell_entry_that_is_a_link_is_refused_and_not_written_as_a_file_holding_its_target(self):
        # a symbolic link in a zip is an entry of mode 0120777 whose bytes are the target's path: read as a plain file it would be written into the archive as 0644 text
        for platform in ('macos', 'ios'):
            link = write_shell_zip(self.path('shells', 'link-%s.zip' % platform), shell_files(platform))
            with zipfile.ZipFile(link, 'a') as z:
                info = zipfile.ZipInfo('MagicPortals/MagicPortals.app/Contents/Resources/escape' if platform == 'macos' else 'Payload/MagicPortals.app/escape',
                                       date_time=(2026, 10, 25, 22, 0, 0))
                info.create_system, info.external_attr = 3, 0o120777 << 16
                z.writestr(info, '../../../../etc/passwd')
            for content in (True, False):
                code, out, err = self.apple(platform, link, content=content)
                self.assertEqual(code, 2, (platform, content))
                self.assertIn('is not a plain file', err)
                self.assertIn('escape (type bits 120000)', err, 'named, with what it is')
                self.assert_nothing_left()

    def test_a_zip_with_no_unix_mode_word_is_still_a_shell(self):
        # zips written on Windows carry no mode word (the upper 16 bits of the attributes are 0): the type is then unknown, and an entry is a plain file
        files = shell_files('ios')
        path = self.path('shells', 'nomode.zip')
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with zipfile.ZipFile(path, 'w') as z:
            for name in sorted(files):
                info = zipfile.ZipInfo(name, date_time=(2026, 10, 25, 22, 0, 0))
                info.create_system, info.external_attr = 0, 0
                z.writestr(info, files[name][0])
        self.assertEqual(sorted(rl.shell_files_from_zip(path)), sorted(files))
        code, out, err = self.apple('ios', path)
        self.assertEqual(code, 0, err)

    def test_a_bundle_folder_with_a_link_in_it_is_refused(self):
        folder = self.path('bundle')
        for name, (data, _) in shell_files('sim').items():
            put(os.path.join(folder, *name.split('/')), data)
        try:
            os.symlink(self.path('bundle', 'MagicPortals.app', 'MagicPortals'), os.path.join(folder, 'MagicPortals.app', 'link'))
        except (OSError, NotImplementedError):
            self.skipTest('this system will not make a symbolic link for the test: the refusal of a link in a bundle folder is NOT checked on it')
        code, out, err = self.apple('ios', folder)
        self.assertEqual(code, 2)
        self.assertIn('symbolic link', err)
        self.assertIn('(link)', err, 'named relative to the bundle')
        self.assert_nothing_left()

    def test_a_program_of_the_wrong_machine_is_refused(self):
        shell = write_shell_zip(self.path('shells', 'wrong.zip'), shell_files('ios', program=make_macho(0x01000007)))
        code, out, err = self.apple('ios', shell)
        self.assertEqual(code, 2)
        self.assertIn('not one for ios', err)

    def test_a_shell_of_the_other_platform_or_no_bundle_is_refused_by_name(self):
        code, out, err = self.apple('macos', self.shell('ios'))
        self.assertEqual(code, 2)
        self.assertIn('an iOS bundle', err)
        code, out, err = self.apple('ios', self.shell('macos'))
        self.assertEqual(code, 2)
        self.assertIn('a macOS bundle', err)
        empty = write_shell_zip(self.path('shells', 'empty.zip'), {'readme.txt': (b'nothing', 0o644)})
        code, out, err = self.apple('macos', empty)
        self.assertEqual(code, 2)
        self.assertIn('0 application bundles', err)
        code, out, err = self.apple('macos', self.path('shells', 'missing.zip'))
        self.assertEqual(code, 2)

    def test_notices_content_and_output_are_checked_before_anything_is_written(self):
        shell = self.shell('macos')
        code, out, err = run_cli('assemble', '--platform', 'macos', '--shell', shell, '--content', self.content, '--out', self.out, '--repo', self.repo)
        self.assertEqual(code, 2)
        self.assertIn('no LICENSE', err)
        copy = self.path('content-copy')
        shutil.copytree(self.content, copy)
        os.remove(os.path.join(copy, 'levels', 'lvl_b.tscn'))
        code, out, err = run_cli('assemble', '--platform', 'macos', '--shell', shell, '--content', copy, '--out', self.out, '--repo', self.repo, '--notices', self.notices)
        self.assertEqual(code, 2)
        self.assertIn('fails package.py verify', err)
        code, out, err = self.apple('macos', shell, out=os.path.join(self.repo, 'tools', 'x'))
        self.assertEqual(code, 2)
        self.assert_nothing_left()
        self.assertEqual(self.apple('macos', shell)[0], 0)
        code, out, err = self.apple('macos', shell)
        self.assertEqual(code, 2)
        self.assertIn('already exists', err)

    def test_a_folder_the_script_wrote_is_a_shell_too(self):
        folder = self.path('make_app_out')
        for name, (data, mode) in shell_files('macos').items():
            if name.startswith('MagicPortals/MagicPortals.app/'):
                put(os.path.join(folder, 'MagicPortals.app', *name[len('MagicPortals/MagicPortals.app/'):].split('/')), data)
        self.assertEqual(self.apple('macos', folder)[0], 0)
        files = rl.read_archive(os.path.join(self.out, 'MagicPortals-%s-macos-universal.zip' % OTHER_VERSION))
        self.assertIn('MagicPortals/MagicPortals.app/Contents/Resources/GameData/levels/lvl_a.tscn', files)
        self.assertEqual(files['MagicPortals/MagicPortals.app/Contents/MacOS/MagicPortals'][2], 0o755, 'the program is the one the plist names, executable by rule')
        # a folder with one shell zip in it passes that zip through
        folder2 = self.path('make_app_out2')
        os.makedirs(folder2)
        shutil.copyfile(self.shell('ios'), os.path.join(folder2, 'MagicPortals-iOS-shell.ipa'))
        self.assertEqual(self.apple('ios', folder2, content=False, out=self.path('r2'))[0], 0)
        os.makedirs(self.path('empty-folder'))
        self.assertEqual(self.apple('ios', self.path('empty-folder'), content=False, out=self.path('r3'))[0], 2)

    def test_a_bundle_folder_with_no_content_makes_a_shell_zip_named_for_it(self):
        folder = self.path('make_app_out3')
        for name, (data, mode) in shell_files('macos').items():
            if name.startswith('MagicPortals/MagicPortals.app/'):
                put(os.path.join(folder, 'MagicPortals.app', *name[len('MagicPortals/MagicPortals.app/'):].split('/')), data)
        code, out, err = self.apple('macos', folder, content=False)
        self.assertEqual(code, 0, err)
        files = rl.read_archive(os.path.join(self.out, 'MagicPortals-%s-macos-universal-shell.zip' % OTHER_VERSION))
        self.assertTrue(files)
        self.assertFalse([n for n in files if 'GameData' in n])
        self.assertIn('a shell: it has NO game data', out)
        self.assertFalse(os.path.exists(os.path.join(self.out, 'SHA256SUMS.txt')), 'a shell is not a release: no sums')

    def test_the_names_are_the_ones_the_game_looks_for(self):
        with open(os.path.join(REPO, 'game', 'sim', 'AppleLayout.hpp'), encoding='utf-8') as f:
            text = f.read()
        self.assertIn('kGameDataFolder = "GameData"', text)
        self.assertIn('<bundle>/Contents/Resources/GameData', text)
        self.assertIn('<bundle>/GameData', text)
        self.assertEqual(rl.GAME_DATA_IN_BUNDLE, {rl.MACOS: 'Contents/Resources/GameData', rl.IOS: 'GameData'})
        with open(os.path.join(REPO, 'docs', 'packaging.md'), encoding='utf-8') as f:
            doc = f.read()
        self.assertIn('`<MagicPortals.app>/Contents/Resources/GameData`', doc)
        self.assertIn('`<MagicPortals.app>/GameData`', doc)
        with open(os.path.join(REPO, 'game', 'macos', 'MacMain.mm'), encoding='utf-8') as f:
            self.assertIn('Resources', f.read(), 'the Mac entry hands the Resources folder to the lookup')

    def test_every_command_the_text_names_is_one_the_docs_name(self):
        with open(os.path.join(REPO, 'docs', 'apple.md'), encoding='utf-8') as f:
            doc = f.read()
        section7 = doc[doc.index('## 7.'):doc.index('## 8.')]
        text = rl.seal_text(rl.MACOS, '1.0.0', 'Contents/Resources/GameData', True)
        self.assertIn('rcodesign', section7)
        self.assertIn('rcodesign', text)
        self.assertIn('~/Library/Application Support/Magic Portals/GameData', section7)
        self.assertIn('Application Support/Magic Portals/GameData', text)
        with open(os.path.join(REPO, 'tools', 'apple', 'make_app.sh'), encoding='utf-8') as f:
            self.assertIn('codesign --force --sign - --timestamp=none', f.read())
        self.assertIn('codesign --force --sign - --timestamp=none', text)


# ---- the Android release check --------------------------------------------------------------------------------------------------------------

class FakeTools:
    aapt2, zipalign, java, apksigner_jar = 'aapt2', 'zipalign', 'java', 'apksigner.jar'

    def __init__(self, sdk, build_tools):
        self.sdk, self.build_tools = sdk, build_tools


GOOD_LINES = ["the crash report will call the game: Magic Portals Enhanced (from the application's android:label)", 'supersonic.reportTitle: not set in the APK (the application\'s label names the game)',
              'native-code ABIs: arm64-v8a armeabi-v7a x86_64']


# The libraries of an APK that carries all three ABIs, as the APK holds them (entry ABI -> the library's bytes).
THREE_ABIS = {'arm64-v8a': make_elf_program(machine=183), 'armeabi-v7a': make_elf_program(machine=40, width=32), 'x86_64': make_elf_program(machine=62)}


class TheAndroidRelease(ArchiveCase):
    def apk(self, name='MagicPortals-%s-release.apk' % OTHER_VERSION, libs=None):
        libs = libs if libs is not None else dict(THREE_ABIS)
        path = self.path('apks', name)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with zipfile.ZipFile(path, 'w') as z:
            for abi, data in sorted(libs.items()):
                z.writestr('lib/%s/libMagicPortals.so' % abi, data)
            z.writestr('assets/gamedata.list', b'stamp\n')
        return path

    def release(self, apk, lines=None, problems=None, package=None, extra=(), raises=None):
        calls = []

        def inspect(path, tools, repo, release, **kw):
            calls.append((path, release, kw))
            if raises:
                raise raises
            return list(GOOD_LINES if lines is None else lines), list(problems or [])
        args = rl.build_parser().parse_args(['android', '--apk', apk, '--sdk', self.path('sdk'), '--build-tools', '35.0.0', '--out', self.out, '--repo', self.repo,
                                             '--notices', self.notices] + list(extra))
        try:
            return rl.android_release(args, make_tools=FakeTools, inspect=inspect, package_line=lambda t, a: package or (OTHER_VERSION, vs.version_code(OTHER_VERSION))), calls
        except rl.ReleaseError as e:
            return str(e), calls

    def test_a_signed_release_of_the_one_version_is_placed_with_its_notices_and_sums(self):
        apk = self.apk()
        lines, calls = self.release(apk)
        self.assertIsInstance(lines, list, lines)
        self.assertEqual(len(calls), 1)
        self.assertTrue(calls[0][1], 'inspect is asked as a RELEASE')
        self.assertTrue(calls[0][2].get('expect_data'), 'and for a build that has the game in it')
        name = 'MagicPortals-%s-android.apk' % OTHER_VERSION
        self.assertEqual(sorted(os.listdir(self.out)), sorted(['LICENSE', 'NOTICE.md', 'SHA256SUMS.txt', name]))
        with open(apk, 'rb') as fa, open(os.path.join(self.out, name), 'rb') as fb:
            self.assertEqual(fa.read(), fb.read(), 'the APK is copied unchanged: it is signed')
        for n, d in NOTICES.items():
            with open(os.path.join(self.out, n), 'rb') as f:
                self.assertEqual(f.read(), d)
        self.assertEqual(rl.verify_sums(self.out), [])
        text = '\n'.join(lines)
        for word in ('a signed release of version ' + OTHER_VERSION, 'versionCode %d' % vs.version_code(OTHER_VERSION), 'not inside', 'no key was read or made'):
            self.assertIn(word, text)

    def test_an_apk_with_all_three_abis_is_a_release_and_each_library_is_a_players_build(self):
        # android_release does not read the native-code line itself (inspect_apk does, and test_android_package pins it for three ABIs); it is handed here as the line a real inspect prints.
        apk = self.apk()
        with zipfile.ZipFile(apk) as z:
            self.assertEqual(sorted(n for n in z.namelist() if n.startswith('lib/')),
                             ['lib/arm64-v8a/libMagicPortals.so', 'lib/armeabi-v7a/libMagicPortals.so', 'lib/x86_64/libMagicPortals.so'])
        lines, calls = self.release(apk, lines=GOOD_LINES[:2] + ['native-code ABIs: arm64-v8a armeabi-v7a x86_64'])
        self.assertIsInstance(lines, list, lines)
        self.assertIn('3 libraries checked as a player\'s build', '\n'.join(lines))
        self.assertEqual(os.listdir(self.out).count('MagicPortals-%s-android.apk' % OTHER_VERSION), 1)

    def test_no_particular_set_of_abis_is_required_today(self):
        # Pinned so that a change is a decision: the refusals are "no library at all" and what inspect says; a release of two ABIs (as every APK was before the third) is still one.
        lines, _ = self.release(self.apk(libs={'arm64-v8a': THREE_ABIS['arm64-v8a'], 'x86_64': THREE_ABIS['x86_64']}), lines=GOOD_LINES[:2] + ['native-code ABIs: arm64-v8a x86_64'])
        self.assertIsInstance(lines, list, lines)
        self.assertIn('2 libraries checked', '\n'.join(lines))

    def test_a_wrong_library_in_a_three_abi_apk_is_named_by_its_entry_and_nothing_is_placed(self):
        entry = 'lib/armeabi-v7a/libMagicPortals.so'
        cases = (('a developer build', make_elf_program(machine=40, width=32, payload=game_payload(dev=True)), 'the developer tools are compiled in'),
                 ('a diagnostic build', make_elf_program(machine=40, width=32, payload=game_payload(perf=True)), 'the program is a PERFORMANCE-DIAGNOSTIC build'),
                 ('a baked path', make_elf_program(machine=40, width=32, payload=game_payload(extra=[b'/home/someone/x'])), "the program holds '/home/'"),
                 ('an executable, not a shared object', make_elf_program(machine=40, width=32, etype=2), 'ELF type 2'),
                 ('an Elf64 file with the arm machine', make_elf_program(machine=40), 'the file is a 64-bit ELF file and its machine 40 (armv7) is a 32-bit one'),
                 ('a 32-bit machine nobody builds', make_elf_program(machine=3, width=32), 'ELF e_machine 3 is not one this packager has a layout for'))
        for what, data, message in cases:
            text, _ = self.release(self.apk(libs=dict(THREE_ABIS, **{'armeabi-v7a': data})))
            self.assertIn('%s: %s' % (entry, message), text, what)
            self.assertIn('(1 fact(s) fail)', text, '%s: the other two libraries are fine and not named' % what)
            self.assertEqual(os.listdir(self.out) if os.path.isdir(self.out) else [], [], 'nothing is placed (%s)' % what)

    def test_an_apk_that_declares_a_library_it_lacks_or_has_none_is_refused(self):
        # What inspect_apk says when badging lists an ABI the lib/ folder does not hold is its problem, carried into the refusal with the rest; an APK with no library is refused here.
        three = self.apk()
        text, _ = self.release(three, problems=["native-code ['arm64-v8a', 'armeabi-v7a', 'x86_64'] does not match the lib/ entries ['arm64-v8a', 'x86_64']"])
        self.assertIn('does not match the lib/ entries', text)
        self.assertEqual(os.listdir(self.out) if os.path.isdir(self.out) else [], [])
        text, _ = self.release(self.apk(libs={}))
        self.assertIn('the APK has no lib/<abi>/libMagicPortals.so', text)

    def test_the_version_code_rule_and_its_override(self):
        wrong, _ = self.release(self.apk(), package=(OTHER_VERSION, 1))
        self.assertIn('versionCode is 1 and the rule of version.py gives %d' % vs.version_code(OTHER_VERSION), wrong)
        again, _ = self.release(self.apk(), package=(OTHER_VERSION, 77), extra=['--version-code', '77'])
        self.assertIsInstance(again, list, again)
        self.assertEqual(vs.version_code('3.4.5'), 30405, 'the rule, pinned for the version these tests use')

    def test_a_debug_apk_is_refused_with_every_reason(self):
        debug = self.apk('MagicPortals-%s-debug.apk' % OTHER_VERSION)
        text, _ = self.release(debug, problems=['a release APK is debuggable', 'a release APK is signed with the debug key'], package=(OTHER_VERSION, 1))
        for word in ('is named MagicPortals-%s-debug.apk' % OTHER_VERSION, 'a debug APK', 'a release APK is debuggable', 'signed with the debug key', 'versionCode is 1'):
            self.assertIn(word, text)
        self.assertEqual(os.listdir(self.out) if os.path.isdir(self.out) else [], [], 'nothing is placed')

    def test_the_variants_are_refused_by_name_and_by_what_their_libraries_hold(self):
        for tag in ('-VERIFICATION', '-PERFDIAG', '-nodata'):
            text, _ = self.release(self.apk('MagicPortals-%s%s-release.apk' % (OTHER_VERSION, tag)))
            self.assertIn('is never a release', text, tag)
        # a file renamed to look like a release is still caught by what is inside
        dev = self.apk(libs={'x86_64': make_elf_program(payload=game_payload(dev=True))})
        text, _ = self.release(dev)
        self.assertIn('lib/x86_64/libMagicPortals.so: the developer tools are compiled in', text)
        diag = self.apk(libs={'arm64-v8a': make_elf_program(machine=183, payload=game_payload(perf=True))})
        text, _ = self.release(diag)
        self.assertIn('PERFORMANCE-DIAGNOSTIC build', text)
        baked = self.apk(libs={'x86_64': make_elf_program(payload=game_payload(extra=[b'/home/someone/x']))})
        text, _ = self.release(baked)
        self.assertIn("'/home/'", text)
        wrong = self.apk(libs={'x86_64': make_elf_program(etype=2)})
        text, _ = self.release(wrong)
        self.assertIn('ELF type 2', text)
        none, _ = self.release(self.apk(libs={}))
        self.assertIn('has no lib/<abi>/libMagicPortals.so', none)

    def test_the_report_fact_and_the_built_manifest_read_back_are_required(self):
        text, _ = self.release(self.apk(), lines=['native-code ABIs: x86_64'])
        self.assertIn('the report fact is missing', text)
        self.assertIn('supersonic.reportTitle line', text)
        text, _ = self.release(self.apk(), lines=['the crash report will call the game: (no name) (the application declares no android:label)', GOOD_LINES[1]])
        self.assertIn('by no name', text)

    def test_a_version_name_that_is_not_the_one_version_is_refused(self):
        text, _ = self.release(self.apk(), package=('9.9.9', vs.version_code(OTHER_VERSION)))
        self.assertIn("versionName is '9.9.9' and the one version is %s" % OTHER_VERSION, text)

    def test_an_apk_apksigner_cannot_verify_is_refused_with_the_tools_words(self):
        text, _ = self.release(self.apk(), raises=ap.PackError('apksigner verify failed (exit 1): DOES NOT VERIFY'))
        self.assertIn('an unsigned APK', text)
        self.assertIn('DOES NOT VERIFY', text)

    def test_the_notices_are_required_and_the_output_and_the_name_are_checked(self):
        args = rl.build_parser().parse_args(['android', '--apk', self.apk(), '--sdk', self.path('sdk'), '--build-tools', 'x', '--out', self.out, '--repo', self.repo])
        with self.assertRaises(rl.ReleaseError) as caught:
            rl.android_release(args, make_tools=FakeTools, inspect=lambda *a, **k: (GOOD_LINES, []), package_line=lambda t, a: (OTHER_VERSION, 30405))
        self.assertIn('no LICENSE', str(caught.exception))
        bad = rl.build_parser().parse_args(['android', '--apk', self.apk(), '--sdk', self.path('sdk'), '--build-tools', 'x', '--out', os.path.join(self.repo, 'tools', 'rel'),
                                            '--repo', self.repo, '--notices', self.notices])
        with self.assertRaises(rl.ReleaseError):
            rl.android_release(bad, make_tools=FakeTools, inspect=lambda *a, **k: (GOOD_LINES, []), package_line=lambda t, a: (OTHER_VERSION, 30405))
        first, _ = self.release(self.apk())
        self.assertIsInstance(first, list)
        again, _ = self.release(self.apk())
        self.assertIn('already exists', again)

    def test_notices_beside_an_apk_must_be_the_ones_already_there(self):
        os.makedirs(self.out)
        put(os.path.join(self.out, 'LICENSE'), b'another licence')
        text, _ = self.release(self.apk())
        self.assertIn('a release folder holds one set of notices', text)
        # the refusal comes BEFORE anything is placed (as assemble's does): no APK, no NOTICE.md, no sums, the licence there untouched
        self.assertEqual(os.listdir(self.out), ['LICENSE'])
        with open(os.path.join(self.out, 'LICENSE'), 'rb') as f:
            self.assertEqual(f.read(), b'another licence')
        # so the owner can put the right notice there and run it again: the name is not taken by a half-made release
        os.remove(os.path.join(self.out, 'LICENSE'))
        again, _ = self.release(self.apk())
        self.assertIsInstance(again, list, again)
        self.assertEqual(sorted(os.listdir(self.out)), sorted(['LICENSE', 'NOTICE.md', 'SHA256SUMS.txt', 'MagicPortals-%s-android.apk' % OTHER_VERSION]))

    def test_notices_beside_an_apk_that_are_a_link_or_a_folder_are_refused_before_anything_is_placed(self):
        os.makedirs(os.path.join(self.out, 'NOTICE.md'))
        text, _ = self.release(self.apk())
        self.assertIn('is not a plain file', text)
        self.assertEqual(os.listdir(self.out), ['NOTICE.md'], 'only the folder that was already there')

    def test_the_packager_never_asks_for_or_reads_a_key(self):
        code, out, err = run_cli('android', '--apk', 'x', '--sdk', 'y', '--out', 'z', '--keystore', 'k')
        self.assertEqual(code, 2, 'argparse refuses an option the tool does not have')
        with open(os.path.join(HERE, 'release.py'), encoding='utf-8') as f:
            source = f.read()
        self.assertNotIn("add_argument('--keystore", source)
        self.assertNotIn('keytool', source)
        self.assertNotIn("'sign'", source)

    def test_a_missing_apk_and_a_plain_file_that_is_no_apk_are_refused(self):
        code, out, err = run_cli('android', '--apk', self.path('nowhere.apk'), '--sdk', self.path('sdk'), '--build-tools', 'x', '--out', self.out, '--repo', self.repo,
                                 '--notices', self.notices)
        self.assertEqual(code, 2)
        self.assertIn('not a plain file', err)


# ---- planning step 209 (PORT-11): the checkout's path and the project's name, the Linux program's facts, the archive with no game, and the three behaviours of step 208 -----------



def skip_loudly(case, reason):
    """Skips the test AND says so on standard error, where a CI log shows it (unittest's own summary says only "skipped=1"): a check that did not run must not look like one that did."""
    print('\nSKIPPED %s: %s' % (case.id(), reason), file=sys.stderr)
    case.skipTest(reason)


class TheCheckoutPathAndTheProjectName(Temp):
    """PORT10-2 (g): `Magic-Portals-Enhanced` is the checkout's folder name AND the project's public name. A path of a build tree is refused; a line that ends at the name (a URL or a
    credits line that stops there) is not, and a URL with a path after the name holds the name between two separators and is refused (planning step 210 corrects the earlier wording)."""
    NAMES = (b'Magic-Portals-Enhanced', b'Magic-Portals-Remake')

    def refused_for(self, text, name_form):
        r = checked(make_elf_program(payload=game_payload(extra=[text])), rl.LINUX)
        return [t for k, t in r.problems if name_form in t]

    def test_no_bare_name_is_in_the_forbidden_list_and_every_path_form_is(self):
        listed = [needle for needle, _ in rl.FORBIDDEN_STRINGS]
        for name in self.NAMES:
            self.assertNotIn(name, listed, 'the bare name would refuse a credits line')
            for form in (b'/' + name + b'/', b'\\' + name + b'\\'):
                self.assertIn(form, listed)

    def test_a_path_that_holds_the_name_between_two_separators_is_refused_in_both_slash_forms(self):
        for text in (b'/srv/ci/Magic-Portals-Enhanced/game/main.cpp', b'D:\\a\\Magic-Portals-Enhanced\\Magic-Portals-Enhanced\\game\\main.cpp', b'D:/a/Magic-Portals-Enhanced/Magic-Portals-Enhanced/game',
                     b'/opt/Magic-Portals-Remake/out/levels', b'E:\\old\\Magic-Portals-Remake\\data'):
            r = checked(make_elf_program(payload=game_payload(extra=[text])), rl.LINUX)
            self.assertTrue([t for k, t in r.problems if k == rl.DEVELOPER and 'Magic-Portals-' in t], (text, r.problems))

    def test_the_runner_workspace_is_refused_though_it_carries_neither_home_folder_nor_account(self):
        # a Windows runner builds in D:\a\<repository>\<repository>: nothing else of the list sees it
        r = checked(make_pe(payload=game_payload(extra=[b'D:\\a\\Magic-Portals-Enhanced\\Magic-Portals-Enhanced\\game\\sim\\Files.cpp'])), rl.WINDOWS)
        self.assertEqual([t for k, t in r.problems if k == rl.DEVELOPER and 'holds' in t and 'Magic-Portals-Enhanced' in t].__len__(), 1, r.problems)
        self.assertFalse([t for _, t in r.problems if "'/home/'" in t or "'/Users/'" in t or ACCOUNT in t], r.problems)

    def test_the_utf16_form_of_a_path_is_refused_too(self):
        text = '\\Magic-Portals-Enhanced\\'.encode('utf-16-le')
        r = checked(make_pe(payload=game_payload(extra=[b'before ' + text + b' after'])), rl.WINDOWS)
        self.assertTrue([t for k, t in r.problems if 'utf-16' in t and 'Magic-Portals-Enhanced' in t], r.problems)

    def test_a_credits_line_or_a_url_that_names_the_project_is_accepted(self):
        for text in (b'Magic-Portals-Enhanced', b'Magic-Portals-Enhanced by someone', b'Credits: Magic-Portals-Enhanced', b'Magic-Portals-Enhanced.exe', b'https://example.org/someone/Magic-Portals-Enhanced',
                     b'The Magic-Portals-Enhanced project', b'Magic-Portals-Remake', b'Magic-Portals-Enhanced\\', b'/Magic-Portals-Enhanced', b'\\Magic-Portals-Enhanced'):
            for platform, make in ((rl.LINUX, make_elf_program), (rl.WINDOWS, make_pe)):
                r = checked(make(payload=game_payload(extra=[text])), platform)
                self.assertEqual([t for _, t in r.problems if 'Magic-Portals-' in t], [], (text, platform))

    def test_a_credits_line_in_utf16_is_accepted_too(self):
        r = checked(make_pe(payload=game_payload(extra=['Magic-Portals-Enhanced by someone'.encode('utf-16-le')])), rl.WINDOWS)
        self.assertEqual(r.problems, [])

    def test_the_other_forbidden_strings_still_refuse(self):
        # the account name, a home folder and the checkout's own path are not touched by the change
        r = checked(make_elf_program(payload=game_payload(extra=[b'/home/someone/x', ACCOUNT.encode(), b'/mnt/' + b'c/x'])), rl.LINUX)
        text = ' '.join(t for _, t in r.problems)
        for word in ("'/home/'", 'the name of the account that runs this check', "'/mnt/'"):
            self.assertIn(word, text)


class TheAccountOfTheCheck(Temp):
    """Planning step 210: the name of the account that RUNS the check is refused, read from the system when the check runs, and no file of the project spells it. Every test names an
    INVENTED account (through the system's own lookups, patched, or through the argument the function takes)."""
    OTHER = 'quuxbert'

    def holds(self, text, platform=rl.LINUX, **kw):
        make = make_pe if platform == rl.WINDOWS else make_elf_program
        r = checked(make(payload=game_payload(extra=[text])), platform, **kw)
        return [t for k, t in r.problems if k == rl.DEVELOPER and 'the account that runs this check' in t]

    def test_the_running_users_name_is_refused_in_both_encodings_and_any_case(self):
        for text in (('C:/Users/%s/x' % ACCOUNT).encode(), ('owner %s here' % ACCOUNT.upper()).encode(), ACCOUNT.encode(), ('before %s after' % ACCOUNT).encode('utf-16-le')):
            self.assertEqual(len(self.holds(text)), 1, text)
            self.assertEqual(len(self.holds(text, rl.WINDOWS)), 1, text)

    def test_the_refusal_names_the_rule_and_never_spells_the_name_or_quotes_it_back(self):
        found = self.holds(('C:/Users/%s/Desktop/x' % ACCOUNT).encode())
        self.assertEqual(len(found), 1)
        self.assertNotIn(ACCOUNT, found[0].lower())
        self.assertIn('<account>', found[0])
        self.assertIn('%d letters' % len(ACCOUNT), found[0])
        wide = self.holds(('before %s after' % ACCOUNT).encode('utf-16-le'), rl.WINDOWS)
        self.assertNotIn(ACCOUNT, wide[0].lower())

    def test_a_long_name_is_refused_wherever_it_stands_even_glued_to_letters_and_digits(self):
        # step 210, fix round 2: the literal this lookup replaced was a SUBSTRING, so a name of five letters or more is searched as one. These are the shapes an independent probe
        # (16 byte strings that hold a name) found missed when the search wanted a word: all of them must be refused, in ASCII and in UTF-16, whatever the case.
        name = ACCOUNT
        shapes = [('C:\\Users\\%s\\Desktop' % name).encode(), ('/home/%s/x' % name).encode(), ('built by %s today' % name).encode(), ('built by %s today' % name.upper()).encode(),
                  ('xx%s ' % name).encode(), (' %szz ' % name).encode(), (' %s2 ' % name).encode(), (' %s_build ' % name).encode(), (' %s-build ' % name).encode(),
                  ('%srest99@example.invalid' % name).encode(), ('x.%s.y' % name).encode(), ('"%s"' % name).encode(), (b'zorbgarage door'), (b'xzorbgar'), (b'azorbgarx')]
        shapes += [text.encode('utf-16-le') for text in ('built by %s ok' % name, 'C:\\Users\\%s\\x' % name, '%srest99@example.invalid' % name, ' %s2 ' % name, 'xx%szz' % name)]
        for text in shapes:
            self.assertEqual(len(self.holds(text)), 1, text)
            self.assertEqual(len(self.holds(text, rl.WINDOWS)), 1, text)

    def test_a_name_of_four_letters_is_refused_as_a_whole_word_only(self):
        # four letters are found inside too many honest words ("zorb" in "zorbing"): such a name is a word, a path component, a quoted or dotted name, and not a part of a longer one
        name = 'zorb'
        self.assertEqual(rl.ACCOUNT_SUBSTRING_LETTERS, len(name) + 1)
        for text in (b'zorbgarage door', b'xzorb', b'zorb2', b'azorbx', b'zorbing'):
            self.assertEqual(self.holds(text, accounts=[name]), [], text)
        for text in (b'/zorb/', b'(zorb)', b'zorb.DESKTOP-1A2B', b'zorb_backup', b'"zorb"', b'built by ZORB today', 'a zorb b'.encode('utf-16-le')):
            self.assertEqual(len(self.holds(text, accounts=[name])), 1, text)

    def test_the_home_folders_name_is_searched_too_when_it_differs_from_the_user_name(self):
        with mock.patch.object(rl.getpass, 'getuser', return_value=self.OTHER), mock.patch.object(rl.os.path, 'expanduser', return_value='/somewhere/' + ACCOUNT), \
                mock.patch.dict(os.environ, {'USERNAME': '', 'USER': '', 'LOGNAME': ''}):
            names = REAL_RUNNING_ACCOUNTS()
            self.assertEqual(sorted(names), sorted([self.OTHER, ACCOUNT]))
            for name in (self.OTHER, ACCOUNT):
                r = checked(make_elf_program(payload=game_payload(extra=[('x/%s/y' % name).encode()])), rl.LINUX, accounts=names)
                self.assertTrue([t for k, t in r.problems if 'the account that runs this check' in t], name)

    def test_the_running_account_is_what_the_default_check_reads(self):
        with mock.patch.object(rl, 'running_accounts', return_value=[self.OTHER]):
            self.assertEqual(len(self.holds(('C:/Users/%s/x' % self.OTHER).encode())), 1)
            self.assertEqual(self.holds(('owner %s here' % ACCOUNT).encode()), [])  # another account's name is not this check's business (--also-forbid is)

    def test_a_short_or_ordinary_name_is_searched_only_as_a_path_component(self):
        for name in ('bo', 'Runner', 'root', 'USER'):
            found = rl.account_forbidden([name])
            self.assertEqual([item for item, *_ in found], [b'/' + name.encode() + b'/', b'\\' + name.encode() + b'\\'], name)
            self.assertTrue(all(not word for *_, word in found))
            self.assertEqual(self.holds(('the %s of the matter, bobo, a runner and root and users' % name).encode(), accounts=[name]), [], name)
            self.assertEqual(len(self.holds(('/home/x/%s/y' % name).encode(), accounts=[name])), 1, name)
            self.assertEqual(len(self.holds(('C:\\x\\%s\\y' % name).encode(), accounts=[name])), 1, name)

    def test_a_long_name_is_searched_anywhere_and_once_however_many_lookups_give_it(self):
        found = rl.account_forbidden([ACCOUNT, ACCOUNT.upper(), ' ' + ACCOUNT, ''])
        self.assertEqual([item for item, *_ in found], [ACCOUNT.encode()])
        self.assertFalse(found[0][3], 'a name of five letters or more is a substring search, not a whole-word one')
        four = rl.account_forbidden(['zorb', 'ZORB'])
        self.assertEqual([(item, word) for item, _why, _label, word in four], [(b'zorb', True)])

    def test_no_name_known_means_nothing_extra_is_searched(self):
        self.assertEqual(rl.account_forbidden([]), [])
        self.assertEqual(rl.account_forbidden(['', None]), [])

    def test_the_lookup_gives_the_user_name_the_home_folder_and_the_variables(self):
        with mock.patch.object(rl.getpass, 'getuser', return_value='aaaa1'), mock.patch.object(rl.os.path, 'expanduser', return_value='/x/bbbb2'), \
                mock.patch.dict(os.environ, {'USERNAME': 'cccc3', 'USER': '', 'LOGNAME': 'dddd4'}):
            self.assertEqual(REAL_RUNNING_ACCOUNTS(), ['aaaa1', 'bbbb2', 'cccc3', 'dddd4'])

    def test_a_lookup_that_raises_is_skipped_not_fatal(self):
        with mock.patch.object(rl.getpass, 'getuser', side_effect=OSError('no user')), mock.patch.object(rl.os.path, 'expanduser', return_value='/x/home-name'), \
                mock.patch.dict(os.environ, {'USERNAME': '', 'USER': '', 'LOGNAME': ''}):
            self.assertEqual(REAL_RUNNING_ACCOUNTS(), ['home-name'])

    def test_also_forbid_is_how_a_program_built_by_another_account_is_checked(self):
        data = make_elf_program(payload=game_payload(extra=[('x/%s/y' % self.OTHER).encode()]))
        self.assertEqual([t for k, t in checked(data, rl.LINUX).problems if self.OTHER in t], [])
        r = checked(data, rl.LINUX, also_forbid=[self.OTHER])
        self.assertTrue([t for k, t in r.problems if "'%s'" % self.OTHER in t])

    def test_the_command_line_refuses_a_program_that_holds_the_running_account(self):
        program = self.path('built.exe')
        put(program, make_pe(payload=game_payload(extra=[CHECKOUT_PATH_OF_A_BUILDER])))
        code, out, err = run_cli('check-program', program, '--platform', 'windows')
        self.assertEqual(code, 1)
        self.assertIn('the name of the account that runs this check', err)
        self.assertNotIn(ACCOUNT, err.lower())
        self.assertNotIn(ACCOUNT, out.lower())

    def test_the_name_is_hidden_in_the_whole_refusal_and_not_only_in_the_bytes_it_quotes(self):
        # the checkout's own path is a forbidden string, and it names the account when the checkout lives in the account's folder
        repo = self.path('Users', ACCOUNT, 'a-long-checkout-name')
        os.makedirs(repo)
        data = make_elf_program(payload=game_payload(extra=[repo.replace(os.sep, '/').encode()]))
        r = checked(data, rl.LINUX, repo=repo)
        texts = [t for k, t in r.problems if k == rl.DEVELOPER]
        self.assertTrue([t for t in texts if "this checkout's own path" in t], texts)
        for text in texts:
            self.assertNotIn(ACCOUNT, text.lower(), text)
        self.assertIn('<account>', ' '.join(texts))
        for fact in (t for _, t in r.facts):
            self.assertNotIn(ACCOUNT, fact.lower(), fact)

    def test_the_header_of_the_check_hides_the_name_when_the_program_is_given_by_an_absolute_path(self):
        # the first line of check-program echoes the argument, and an absolute path on a builder's PC holds the account's folder; the line is copied into logs, so it is hidden like the refusal
        folder = self.path('Users', ACCOUNT, 'built')
        os.makedirs(folder)
        program = os.path.join(folder, 'MagicPortals.exe')
        put(program, make_pe(payload=game_payload(dev=True), version=b''))
        code, out, err = run_cli('check-program', program, '--platform', 'windows')
        self.assertEqual(code, 1)
        header = out.splitlines()[0]
        self.assertTrue(header.startswith('check-program '), header)
        self.assertIn('<account>', header)
        self.assertIn('(windows, ', header)  # the rest of the line is the same as before
        self.assertNotIn(ACCOUNT, (out + err).lower())

    def test_an_error_line_hides_the_name_of_a_path_it_was_given(self):
        folder = self.path('Users', ACCOUNT, 'built')
        os.makedirs(folder)
        code, out, err = run_cli('check-program', os.path.join(folder, 'missing.exe'), '--platform', 'windows')
        self.assertEqual(code, 2)
        self.assertTrue(err.startswith('release.py: '), err)
        self.assertIn('<account>', err)
        self.assertIn('is not a plain file', err)
        self.assertNotIn(ACCOUNT, (out + err).lower())

    def test_hide_names_replaces_a_name_and_leaves_a_short_or_ordinary_one_alone(self):
        self.assertEqual(rl.hide_names('x %s y %s z' % (ACCOUNT, ACCOUNT.upper()), [ACCOUNT]), 'x <account> y <account> z')
        self.assertEqual(rl.hide_names('a robot at the bo of a runner', ['bo', 'runner', '']), 'a robot at the bo of a runner')
        self.assertEqual(rl.hide_names('nothing here', []), 'nothing here')

    def test_the_two_lookups_of_the_account_agree_on_what_a_name_is(self):
        # tools/ci/running_account.py (the checkers) and release.py (the packager, which runs on its own) each hold the rule; they must not drift apart
        sys.path.insert(0, os.path.join(REPO, 'tools', 'ci'))
        try:
            import running_account as ra
        finally:
            sys.path.pop(0)
        self.assertEqual(ra.ACCOUNT_MIN_LETTERS, rl.ACCOUNT_MIN_LETTERS)
        self.assertEqual(ra.ACCOUNT_SUBSTRING_LETTERS, rl.ACCOUNT_SUBSTRING_LETTERS)
        self.assertEqual(set(ra.GENERIC_NAMES), set(rl.GENERIC_ACCOUNTS))
        for names in (['abcd', 'ab', 'runner', 'Alice1', 'alice1'], ['root', 'USER', 'zorbgar'], ['zorb', 'x'], []):
            # which names are searched bare (a path component is the packager's way of searching a short name; the checkers skip such a name)
            packager = sorted(item.decode().lower() for item, _why, _label, _word in rl.account_forbidden(names) if not item.startswith((b'/', b'\\')))
            checkers = sorted(n.lower() for n in ra.searchable_accounts(names)[0])
            self.assertEqual(packager, checkers, names)
        # and HOW: the same strings are a hit for both, a name of five letters anywhere, a name of four letters only as a whole word
        for name in ('zorbgar', 'Alice1', 'zorb', 'ABCD'):
            regex = ra.account_regex([name])
            bare = [(item, word) for item, _why, _label, word in rl.account_forbidden([name])]
            for text in (name, 'x' + name, name + 'x', name + '2', '2' + name, '/' + name + '/', '"' + name + '"', name + '_build', 'rest-' + name, name.lower() + 'rest99@example.invalid'):
                data = text.encode()
                in_packager = any(rl.occurrences(data, data.lower(), item, word) for item, word in bare)
                self.assertEqual(in_packager, bool(regex.search(text)), (name, text))

    def test_no_file_of_the_project_but_the_tests_spells_the_real_accounts_name(self):
        names = [n for n in REAL_RUNNING_ACCOUNTS() if len(n) >= rl.ACCOUNT_MIN_LETTERS and n.lower() not in rl.GENERIC_ACCOUNTS]
        if not names:
            self.skipTest('the account that runs this test has a short or ordinary name: nothing to look for')
        # the same three kinds as the lookup itself (a name of five letters or more anywhere, one of four letters as a word), so that a file cannot hold the name glued to something
        pattern = re.compile('|'.join(re.escape(n) if len(n) >= rl.ACCOUNT_SUBSTRING_LETTERS else r'(?<![A-Za-z0-9])%s(?![A-Za-z0-9])' % re.escape(n) for n in names), re.I)
        for name in ('release.py', 'test_release.py', 'package.py', 'android_package.py', 'check_version.py'):
            with open(os.path.join(HERE, name), encoding='utf-8') as f:
                self.assertEqual(pattern.findall(f.read()), [], name)
        with open(os.path.join(REPO, 'docs', 'packaging.md'), encoding='utf-8') as f:
            self.assertEqual(pattern.findall(f.read()), [], 'docs/packaging.md')


class TheLinuxProgramsFacts(Temp):
    """PORT10-3 (planning step 209): whether the program carries its symbols, what it asks of the system, and the floor a caller names."""

    def test_a_program_with_no_section_table_or_no_symbol_table_is_stripped(self):
        for sections in (None, ['.text', '.dynsym', '.dynstr']):
            r = checked(make_elf_program(sections=sections), rl.LINUX)
            self.assertTrue(r.stripped, sections)
            self.assertTrue(any('stripped (it has no symbol table)' in t for _, t in r.facts), r.facts)
            self.assertFalse(any('NOT stripped' in t for _, t in r.facts))

    def test_a_program_with_a_symbol_table_is_not_stripped_and_a_debug_section_is_said(self):
        r = checked(make_elf_program(sections=['.text', '.symtab', '.strtab']), rl.LINUX)
        self.assertFalse(r.stripped)
        self.assertFalse(r.has_debug)
        self.assertTrue(any('NOT stripped (it carries a symbol table)' in t for _, t in r.facts), r.facts)
        r = checked(make_elf_program(sections=['.text', '.symtab', '.debug_info', '.debug_line']), rl.LINUX)
        self.assertFalse(r.stripped)
        self.assertTrue(r.has_debug)
        self.assertTrue(any('symbol table and debug sections' in t for _, t in r.facts), r.facts)

    def test_a_section_table_that_runs_past_the_end_is_a_structure_fact_not_a_traceback(self):
        r = checked(make_elf_program(sections=['.text'], table_past_the_end=True), rl.LINUX)
        self.assertTrue([t for k, t in r.problems if k == rl.STRUCTURE and 'section header table runs past the end' in t], r.problems)

    def test_only_a_linux_program_is_asked_for_its_symbols(self):
        self.assertIsNone(checked(make_elf_program(machine=183, sections=['.text', '.symtab']), rl.ANDROID_LIB).stripped)

    def test_the_section_names_are_read_by_the_reader(self):
        names = rl.read_elf(make_elf_program(sections=['.text', '.symtab', '.strtab']))['sections']
        self.assertEqual(names, ['', '.text', '.symtab', '.strtab', '.shstrtab'])
        self.assertEqual(rl.read_elf(make_elf_program())['sections'], [])

    def test_a_program_that_names_no_libstdcxx_version_is_told_so(self):
        r = checked(make_elf_program(glibcxx=()), rl.LINUX)
        self.assertTrue(any('asks the system for no libstdc++ version' in t for _, t in r.facts), r.facts)
        r = checked(make_elf_program(), rl.LINUX)
        self.assertFalse(any('asks the system for no libstdc++ version' in t for _, t in r.facts), 'a program that names GLIBCXX_3.4.32 asks for it')

    def test_the_glibc_floor_passes_below_and_at_it_and_refuses_above_it(self):
        data = make_elf_program(glibc=('2.17', '2.35'))
        for floor, ok in (((2, 35), True), ((2, 36), True), ((2, 34), False), ((3, 0), True), ((2, 35, 1), True)):
            r = checked(data, rl.LINUX, max_glibc=floor)
            self.assertEqual(not [t for k, t in r.problems if k == rl.FLOOR], ok, floor)
        r = checked(data, rl.LINUX, max_glibc=(2, 34))
        self.assertIn('the program asks for glibc 2.35 and the floor is 2.34', r.problems[0][1])
        self.assertTrue(any('glibc floor: the program asks for glibc 2.35' in t for _, t in checked(data, rl.LINUX, max_glibc=(2, 35)).facts))

    def test_the_glibc_floor_compares_numbers_not_text(self):
        # "2.9" is after "2.35" as text and before it as a version
        r = checked(make_elf_program(glibc=('2.9',)), rl.LINUX, max_glibc=(2, 35))
        self.assertEqual([t for k, t in r.problems if k == rl.FLOOR], [])
        r = checked(make_elf_program(glibc=('2.38',)), rl.LINUX, max_glibc=(2, 4))
        self.assertTrue([t for k, t in r.problems if k == rl.FLOOR])

    def test_a_program_that_names_no_glibc_cannot_be_checked_against_a_floor_and_is_refused(self):
        r = checked(make_elf_program(glibc=()), rl.LINUX, max_glibc=(2, 35))
        self.assertTrue([t for k, t in r.problems if k == rl.FLOOR and 'cannot be checked' in t], r.problems)

    def test_the_floor_is_not_waived_by_allow_dev(self):
        data = make_elf_program(glibc=('2.38',), payload=game_payload(dev=True))
        r = checked(data, rl.LINUX, max_glibc=(2, 35))
        self.assertEqual(len(r.blocking(True)), 1, r.problems)
        self.assertIn('glibc 2.38', r.blocking(True)[0])

    def test_a_floor_for_another_platform_is_a_bad_question(self):
        with self.assertRaises(rl.ReleaseError):
            checked(make_pe(), rl.WINDOWS, max_glibc=(2, 35))

    def test_the_command_line_takes_the_floor(self):
        program = self.path('MagicPortals')
        put(program, make_elf_program(glibc=('2.17', '2.38')))
        code, out, err = run_cli('check-program', program, '--platform', 'linux', '--repo', REPO, '--max-glibc', '2.35')
        self.assertEqual(code, 1, out + err)
        self.assertIn('the program asks for glibc 2.38 and the floor is 2.35', err)
        code, out, err = run_cli('check-program', program, '--platform', 'linux', '--repo', REPO, '--max-glibc', '2.38')
        self.assertEqual(code, 0, out + err)
        for bad in ('2', '2.x', 'glibc 2.35', '2.35.', '-2.35'):
            code, out, err = run_cli('check-program', program, '--platform', 'linux', '--repo', REPO, '--max-glibc', bad)
            self.assertEqual(code, 2, bad)
            self.assertIn('--max-glibc wants a glibc version', err)
        code, out, err = run_cli('check-program', self.path('x.exe'), '--platform', 'windows', '--repo', REPO, '--max-glibc', '2.35')
        self.assertEqual(code, 2)

    def test_the_parser_of_the_floor(self):
        self.assertIsNone(rl.parse_glibc_floor(None))
        self.assertEqual(rl.parse_glibc_floor('2.35'), (2, 35))
        self.assertEqual(rl.parse_glibc_floor('2.9'), (2, 9))
        self.assertEqual(rl.parse_glibc_floor('2.31.1'), (2, 31, 1))


class TheLinuxArchiveFacts(ArchiveCase):
    def archive(self):
        return os.path.join(self.out, 'MagicPortals-%s-linux-x86_64.tar.gz' % OTHER_VERSION)

    def playing(self):
        with tarfile.open(self.archive(), 'r:gz') as tf:
            return tf.extractfile('MagicPortals-%s-linux-x86_64/PLAYING.txt' % OTHER_VERSION).read().decode('utf-8')

    def test_playing_says_stripped_when_it_is_and_not_when_it_is_not(self):
        self.assertEqual(self.assemble('linux', self.linux_program(sections=None))[0], 0)
        text = self.playing()
        self.assertIn('The program is stripped (it carries no symbol table)', text)
        self.assertNotIn('is not stripped', text)
        shutil.rmtree(self.out)
        self.assertEqual(self.assemble('linux', self.linux_program(sections=['.text', '.symtab']))[0], 0)
        self.assertIn('The program is not stripped', self.playing())

    def test_playing_says_the_cxx_runtime_is_inside_when_no_glibcxx_is_asked_for(self):
        self.assertEqual(self.assemble('linux', self.linux_program(glibcxx=()))[0], 0)
        text = self.playing()
        self.assertIn('glibc 2.38 or newer:', text)
        self.assertNotIn('GLIBCXX', text)
        self.assertIn('the C++ runtime is inside the program', text)
        shutil.rmtree(self.out)
        self.assertEqual(self.assemble('linux', self.linux_program(glibcxx=('3.4.32',)))[0], 0)
        text = self.playing()
        self.assertIn('GLIBCXX 3.4.32', text)
        self.assertNotIn('the C++ runtime is inside the program', text)

    def test_a_floor_is_passed_to_the_check_and_a_refusal_leaves_nothing(self):
        code, out, err = self.assemble('linux', self.linux_program(glibc=('2.17', '2.38')), '--max-glibc', '2.35')
        self.assertEqual(code, 2)
        self.assertIn('the program asks for glibc 2.38 and the floor is 2.35', err)
        self.assert_nothing_left()
        code, out, err = self.assemble('linux', self.linux_program(glibc=('2.17', '2.38')), '--max-glibc', '2.38')
        self.assertEqual(code, 0, err)

    def test_a_floor_for_a_windows_archive_or_an_apple_shell_is_refused(self):
        code, out, err = self.assemble('windows', self.windows_program(), '--max-glibc', '2.35')
        self.assertEqual(code, 2)
        self.assertIn('--max-glibc is for a Linux program', err)
        code, out, err = run_cli('assemble', '--platform', 'macos', '--shell', self.path('x.zip'), '--max-glibc', '2.35', '--out', self.out, '--repo', self.repo)
        self.assertEqual(code, 2)
        self.assertIn('--max-glibc', err)

    def test_the_stripped_archive_is_the_same_bytes_twice(self):
        program = self.linux_program(sections=None)
        self.assertEqual(self.assemble('linux', program, out=self.path('first'))[0], 0)
        os.utime(program, (1_650_000_000, 1_650_000_000))
        self.assertEqual(self.assemble('linux', program, out=self.path('second', 'deeper'))[0], 0)
        name = 'MagicPortals-%s-linux-x86_64.tar.gz' % OTHER_VERSION
        self.assertEqual(pk.sha256_of(os.path.join(self.path('first'), name)), pk.sha256_of(os.path.join(self.path('second', 'deeper'), name)))


class TheArchiveWithNoGame(ArchiveCase):
    """PORT10-6 (b): -nodata is no release. It is named like -dev and -shell, left out of the sums, and an assembly of it writes no sums."""
    NODATA = 'MagicPortals-1.2.3-linux-x86_64-nodata.tar.gz'

    def test_the_name_is_a_developers_name(self):
        for name in (self.NODATA, 'MagicPortals-1.2.3-windows-x64-nodata.zip', 'MagicPortals-1.2.3-windows-x64-nodata-dev.zip', 'MagicPortals-1.2.3-linux-x86_64-dev.tar.gz',
                     'MagicPortals-1.2.3-macos-universal-shell.zip'):
            self.assertTrue(rl.is_developer_name(name), name)
        for name in ('MagicPortals-1.2.3-linux-x86_64.tar.gz', 'MagicPortals-1.2.3-android.apk', 'MagicPortals-1.2.3-windows-x64.zip', 'MagicPortals-1.2.3-ios-arm64.ipa'):
            self.assertFalse(rl.is_developer_name(name), name)

    def test_sums_refuse_it_unless_allowed_and_leave_it_out_of_an_assemblys(self):
        put(self.path('f', self.NODATA), b'a program, no game')
        code, out, err = run_cli('sums', self.path('f'))
        self.assertEqual(code, 2)
        self.assertIn('-nodata', err)
        put(self.path('f', 'MagicPortals-1.2.3-linux-x86_64.tar.gz'), b'the whole game')
        self.assertEqual(rl.write_sums(self.path('f'), skip_dev=True), ['MagicPortals-1.2.3-linux-x86_64.tar.gz'])
        self.assertEqual(rl.verify_sums(self.path('f')), [], 'and the check does not ask for it')

    def test_an_assembly_with_no_data_writes_no_sums_and_says_why(self):
        code, out, err = self.assemble('linux', self.linux_program(), content=False)
        self.assertEqual(code, 0, err)
        self.assertEqual(os.listdir(self.out), ['MagicPortals-%s-linux-x86_64-nodata.tar.gz' % OTHER_VERSION], 'the archive alone: no sums over a folder that holds no release')
        self.assertIn('no SHA256SUMS.txt written for an archive with no game', out)

    def test_a_release_assembled_beside_it_lists_only_the_whole_game(self):
        self.assertEqual(self.assemble('linux', self.linux_program(), content=False)[0], 0)
        self.assertEqual(self.assemble('linux', self.linux_program())[0], 0)
        with open(os.path.join(self.out, 'SHA256SUMS.txt'), encoding='utf-8') as f:
            lines = f.read().splitlines()
        self.assertEqual([l.split('  ')[1] for l in lines], ['MagicPortals-%s-linux-x86_64.tar.gz' % OTHER_VERSION])
        self.assertEqual(rl.verify_sums(self.out), [])

    def test_a_windows_archive_with_no_data_writes_no_sums_either(self):
        self.assertEqual(self.assemble('windows', self.windows_program(), content=False)[0], 0)
        self.assertEqual(os.listdir(self.out), ['MagicPortals-%s-windows-x64-nodata.zip' % OTHER_VERSION])


class TheThreeBehavioursNoTestPinned(ArchiveCase):
    """PORT10-6 (c): the three behaviours of step 208's fix round 1 that the round-2 verifier's own mutations of an isolated copy left green."""

    def test_a_dangling_link_where_a_notice_goes_beside_an_apk_is_refused_before_anything_is_placed(self):
        # mutation: `os.path.lexists` -> `os.path.exists` in android_release: a dangling link "does not exist", the APK is placed and the link is then left beside it
        os.makedirs(self.out)
        try:
            os.symlink(self.path('nowhere', 'at', 'all'), os.path.join(self.out, 'LICENSE'))
        except (OSError, NotImplementedError) as e:
            skip_loudly(self, 'this system will not make a symbolic link (%s): the refusal of a dangling link beside an APK is NOT checked on it' % e)
        case = TheAndroidRelease('test_a_signed_release_of_the_one_version_is_placed_with_its_notices_and_sums')
        case.out, case.repo, case.notices, case.path = self.out, self.repo, self.notices, self.path
        text, calls = case.release(case.apk())
        self.assertIn('is not a plain file', text, 'refused, and in words')
        self.assertEqual(sorted(os.listdir(self.out)), ['LICENSE'], 'no APK, no notice, no sums: only the link that was there')
        self.assertTrue(os.path.islink(os.path.join(self.out, 'LICENSE')))

    def test_a_link_to_a_folder_inside_a_bundle_folder_is_refused(self):
        # mutation: `ds + fs` -> `fs` in refuse_links_in_bundle: only links to files were seen
        folder = self.path('bundle')
        for name, (data, _) in shell_files('sim').items():
            put(os.path.join(folder, *name.split('/')), data)
        try:
            os.symlink(self.path('bundle', 'MagicPortals.app', 'engine'), os.path.join(folder, 'MagicPortals.app', 'folderlink'), target_is_directory=True)
        except (OSError, NotImplementedError) as e:
            skip_loudly(self, 'this system will not make a symbolic link (%s): the refusal of a link to a FOLDER inside a bundle is NOT checked on it' % e)
        code, out, err = run_cli('assemble', '--platform', 'ios', '--shell', folder, '--content', self.content, '--out', self.out, '--repo', self.repo, '--notices', self.notices)
        self.assertEqual(code, 2, out + err)
        self.assertIn('symbolic link', err)
        self.assertIn('folderlink', err, 'and named')
        self.assertFalse(os.path.exists(self.out) and os.listdir(self.out), 'nothing written')

    def test_a_runtime_dll_the_program_only_delay_loads_is_asked_of_the_redistributable(self):
        # mutation: `pe['imports'] + pe['delay_imports']` -> `pe['imports']` in vc_redist_entries: the DLL a delay-load names would be left out of the archive while
        # PLAYING.txt says the folder carries the runtime the program loads
        redist = self.path('redist')
        for n in ('VCRUNTIME140.dll',):
            put(os.path.join(redist, n), make_pe(dll=True, imports=('KERNEL32.dll',), delay=(), payload=b'a runtime'))
        program = self.windows_program(imports=('KERNEL32.dll', 'VCRUNTIME140.dll'), delay=('vulkan-1.dll', 'msvcp140_atomic_wait.dll'))
        code, out, err = self.assemble('windows', program, '--vc-redist', redist)
        self.assertEqual(code, 2, out + err)
        self.assertIn('msvcp140_atomic_wait.dll is imported by the program and is not in', err)
        self.assert_nothing_left()
        put(os.path.join(redist, 'msvcp140_atomic_wait.dll'), make_pe(dll=True, imports=('KERNEL32.dll',), delay=(), payload=b'an atomic wait'))
        code, out, err = self.assemble('windows', program, '--vc-redist', redist)
        self.assertEqual(code, 0, err)
        files = rl.read_archive(os.path.join(self.out, 'MagicPortals-%s-windows-x64.zip' % OTHER_VERSION))
        self.assertIn('MagicPortals-%s-windows-x64/msvcp140_atomic_wait.dll' % OTHER_VERSION, files)


class TheHelpNamesTheRefusals(Temp):
    """PORT10-6 (d): two refusals were added in fix round 1 and the --help lines did not say so."""

    def help_of(self, *command):
        code, out, err = run_cli(*(list(command) + ['--help']))
        self.assertEqual(code, 0)
        return ' '.join(out.split())

    def test_assemble_help_names_the_shell_entry_refusal_the_floor_and_the_sums_rule(self):
        text = self.help_of('assemble')
        for phrase in ('an entry that is not a plain file (a symbolic link', 'not even a link to a file inside a bundle folder', '--max-glibc', '-dev, -nodata, -shell'):
            self.assertIn(phrase, text)

    def test_android_help_names_the_notice_beside_the_apk_refusal(self):
        text = self.help_of('android')
        for phrase in ('already beside the place for the APK', 'differs from the one given, is a folder, or is a link, dangling or not', 'before anything is placed'):
            self.assertIn(phrase, text)

    def test_check_program_help_names_the_floor(self):
        self.assertIn('--max-glibc', self.help_of('check-program'))


class TheVersionCheckThroughTheRelease(Temp):
    def test_the_subcommand_is_the_check_and_passes_on_the_real_tree(self):
        code, out, err = run_cli('check-version')
        self.assertEqual(code, 0, out + err)
        self.assertIn('check_version: ok', out)
        code, out, err = run_cli('check-version', '--repo', self._tmp)
        self.assertEqual(code, 1)
        self.assertIn('VERSION:', out)


if __name__ == '__main__':
    unittest.main()
