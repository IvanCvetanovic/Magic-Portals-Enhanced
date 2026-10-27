#!/usr/bin/env python3
"""The release packager: from a built program and the owner's content package to the file a player downloads (planning step 208, PORT-10).

    release.py check-program FILE --platform windows|linux|macos|ios|android-lib [--allow-dev] [--also-forbid TEXT]... [--repo R]
    release.py assemble --platform windows|linux --program FILE (--content DIR | --no-data) --out DIR [--vc-redist DIR] [--notices DIR] [--shaders DIR]
                        [--allow-dev] [--repo R]
    release.py assemble --platform macos|ios --shell ZIP|FOLDER [--content DIR] [--notices DIR] --out DIR [--allow-dev] [--repo R]
    release.py android  --apk FILE --sdk SDK [--build-tools VER] [--notices DIR] --out DIR [--version-code N] [--repo R]
    release.py sums     DIR [--check] [--allow-dev]
    release.py check-version [--repo R]

WHAT IT IS FOR. Until this step there was no single, checked way from "a built program and a content folder" to "the archive a player gets", for any of the five
platforms, and nothing stopped a developer's build (baked paths with an account name in them, the developer's flags, the performance readout) from being shipped
by mistake. This tool is that way. It runs on the owner's PC (Python 3.14 on Windows, 3.12 on Linux; macOS has not been run), reads the program and the content
package, and writes ONE archive per platform with the whole game in it. CI holds no game data, so an archive with the game is assembled on the owner's PC from a program
built there or by CI, and from his content package (tools/package/package.py content). Nothing it writes goes into the repository: it refuses an output folder in
the tracked tree.

WHAT IT DECIDES NOTHING OF. The licence text and the notices are the repository's own LICENSE and NOTICE.md (planning step 210): this tool takes them from the repository root or
from the folder --notices names, copies them unchanged, and refuses to write a release archive without them. The version (a placeholder today) is read from the one definition
(tools/package/version.py) and never set here. The application id, the signing identities, the Android release keystore, and whether the game's data goes into the macOS
and iOS packages are the owner's; this tool only offers the mechanisms. For the C++ runtime of a Windows archive it offers --vc-redist (below); without it the archive
carries no runtime and its PLAYING.txt says what a PC without it shows.

WHAT IT REFUSES, each with a message that names every cause (not the first):
  a program that is a developer's build (check-program), content that fails `package.py verify`, a content folder that holds a file outside its manifest, a missing
  notice, an output folder inside the tracked tree, an archive that already exists, an APK that is not a signed release of the one version.

THE ARCHIVES ARE THE SAME BYTES TWICE: sorted entries, a fixed date, fixed modes by rule (the program 0755, everything else 0644), no owner names, no clock. (The
compressed bytes are those of the Python zlib of the machine: two runs on one machine are equal, and another Python's zlib may differ.)

check-program READS A BUILT PROGRAM WITHOUT STARTING IT and checks exactly the facts a built program carries as bytes. Each fact says where it comes from:
  format and machine   the PE machine, the ELF class and e_machine, the Mach-O cputype (thin or fat) must be what the platform name says.
  no baked path        FORBIDDEN_STRINGS below, the checkout's own absolute path, the name of the account that RUNS THE CHECK (read from the system when it runs, so no file holds it;
                       a program built by another account is checked for that one's name through --also-forbid) and any --also-forbid text must not occur, in ASCII or in
                       UTF-16, in any case. A build
                       made with MAGICPORTALS_BAKED_PATHS=ON, or by MSVC with its program database path and __FILE__ text, has them.
  developer tools OFF  none of DEV_TOOLS_SENTINELS occurs. They are whole lines of --help that main.cpp prints only inside `#if MAGICPORTALS_DEV_TOOLS`, so a program
                       made with the tools OFF cannot hold them. The OTHER direction (the refusal sentence "is a developer's flag, and this build was made without the
                       developer tools") is NOT used: it is compiled in StartChecks.cpp for every build, and whether a linker keeps it in a program made with the tools ON is
                       the linker's choice (the Windows developer program does not carry it, measured), so its absence would prove nothing.
  no readout           the string PERFORMANCE-DIAGNOSTIC BUILD (android_package.PERF_DIAG_MARKER) is in a diagnostic program and in no other.
  it is the game       two strings every build of this program carries (GAME_MARKERS), so that a file that is nothing at all does not pass the absences above.
  version (Windows)    the VERSIONINFO ProductVersion and FileVersion equal the one version in its four-field form. It is READ BY SCANNING for the UTF-16 key and its
                       value (and the fixed-size block's binary quad), not by walking the resource directory; a program with none (one made without
                       MAGICPORTALS_WIN_GUI=ON) is refused for it.
--allow-dev turns the refusals about the build's kind into printed warnings and writes DEVELOPER BUILD into whatever archive is then made (its name carries -dev): for a
smoke check only. A wrong format or machine is never allowed.

Standard library only.
"""
import argparse
import contextlib
import getpass
import gzip
import hashlib
import io
import os
import plistlib
import re
import shutil
import struct
import sys
import tarfile
import tempfile
import types
import zipfile
from xml.parsers.expat import ExpatError

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import android_package as ap  # noqa: E402  (inspect, the perf-diag marker, the zip date and entry attributes, the output-folder rule)
import check_version as cv  # noqa: E402
import package as pk  # noqa: E402  (the shell layout, verify, the hash)
import version as vs  # noqa: E402  (the one version)

# ---- the platforms ----------------------------------------------------------------------------------------------------------------------
WINDOWS, LINUX, MACOS, IOS, ANDROID_LIB = 'windows', 'linux', 'macos', 'ios', 'android-lib'
ANDROID = 'android'  # the APK's platform name in an archive name (check-program reads its libraries as android-lib)
CHECKED_PLATFORMS = (WINDOWS, LINUX, MACOS, IOS, ANDROID_LIB)
DESKTOP_PLATFORMS = (WINDOWS, LINUX)
APPLE_PLATFORMS = (MACOS, IOS)

# What each platform's program may be. The name of the machine is the one the archive's name carries.
PE_MACHINES = {0x8664: 'x64'}
ELF_MACHINES = {62: 'x86_64', 183: 'arm64'}
MACHO_CPUS = {0x01000007: 'x86_64', 0x0100000C: 'arm64'}
ACCEPTED_ARCHS = {WINDOWS: ('x64',), LINUX: ('x86_64',), MACOS: ('universal', 'arm64', 'x86_64'), IOS: ('arm64',), ANDROID_LIB: ('x86_64', 'arm64')}
MACHO_LE_MAGIC, MACHO_FAT_MAGIC, MACHO_FAT64_MAGIC = b'\xcf\xfa\xed\xfe', b'\xca\xfe\xba\xbe', b'\xca\xfe\xba\xbf'
ELF_SECTION_HEADER_BYTES = 64  # sizeof(Elf64_Shdr)
SYMBOL_TABLE_SECTION = '.symtab'
MACHO_EXECUTE, MOST_FAT_ARCHS = 2, 30  # a Java class file starts with the fat magic too, and has no 30 architectures

# Two strings every build of this program carries, whatever it was made with: the log file's name (game/sim/Scratch.hpp) and a line of --help (game/main.cpp).
GAME_MARKERS = (b'magicportals.log', b'ignore the paths this build was configured with')

# THE FORBIDDEN BYTE STRINGS, searched in ASCII and in UTF-16LE, in any case. Each says why it is there. A built program that holds one has the builder's machine in it.
# The name of the ACCOUNT is not in this table: it is read from the system when the check runs (running_accounts, account_forbidden below), so that no file of the project spells it
# (planning step 210).
FORBIDDEN_STRINGS = (
    # THE CHECKOUT'S FOLDER NAME, AS A PATH AND NOT AS A NAME (planning step 209, PORT10-2 g). The folder name is also the project's public name, so a line that ENDS at the name (a
    # URL such as https://github.com/<owner>/Magic-Portals-Enhanced, or a credits line that stops there) must not be refused: only the name BETWEEN TWO SEPARATORS is a path of a build
    # tree (`D:\a\Magic-Portals-Enhanced\...` on a Windows runner, which carries neither /home/ nor /Users/). A URL with a path after the name, such as .../Magic-Portals-Enhanced/releases,
    # holds the name between two separators and is refused; so is any line that has a separator on both sides of the name.
    (b'/Magic-Portals-Enhanced/', "the checkout's folder name inside a path written with slashes: any absolute path of the build tree, whoever's account it was in"),
    (b'\\Magic-Portals-Enhanced\\', "the checkout's folder name inside a path written with backslashes (a Windows runner's workspace is D:\\a\\<repository>\\<repository>)"),
    (b'/Magic-Portals-Remake/', "the checkout's former folder name (until 2026-10-01) inside a path written with slashes: a tree configured before the rename"),
    (b'\\Magic-Portals-Remake\\', "the checkout's former folder name (until 2026-10-01) inside a path written with backslashes: a tree configured before the rename"),
    (b'/mnt/', "a Windows drive mounted into WSL (a /mnt/<drive>/... path): a build made there baked that drive's paths"),
    (b'/home/', "a Linux home folder: the builder's account"),
    (b'/Users/', "a macOS home folder or a Windows one written with slashes (<drive>:/Users/...): the builder's account"),
    (b'\\Users\\', "a Windows home folder written with backslashes: the builder's account (a program database's path is written so)"),
)

# THE ACCOUNT THAT RUNS THE CHECK (planning step 210). A program is built by an account and checked by an account, and on the owner's PC they are one. The name is read from the system now and
# searched like the table above, in three kinds. ANYWHERE, as a substring (a path, a build label with a digit glued to it, the local part of an address), when it has
# ACCOUNT_SUBSTRING_LETTERS letters or more and is not the name of an ordinary folder: that is how the literal this lookup replaced was searched. As a WHOLE WORD (no letter or digit touching
# it) when it has ACCOUNT_MIN_LETTERS letters, because four letters are too few to be found inside longer words without refusing honest text. As a path component (/name/ and \name\) when it
# is shorter or is the name of an ordinary folder, because a bare "bo" or "runner" would refuse every program that holds the word. A program built by ANOTHER account is checked for that
# account's name through --also-forbid. tools/ci/running_account.py keeps the same three kinds for the checkers (a test holds the two together).
ACCOUNT_MIN_LETTERS = 4
ACCOUNT_SUBSTRING_LETTERS = 5
GENERIC_ACCOUNTS = frozenset(('runner', 'runneradmin', 'public', 'default', 'administrator', 'user', 'username', 'name', 'yourname', 'you', 'me', 'example', 'someone', 'shared', 'guest', 'root',
                              'ubuntu', 'admin', 'vagrant', 'home', 'all', 'users', 'account', 'owner'))  # the same words as tools/ci/running_account.py (a test holds the two together)
WHY_ACCOUNT = ("the name of the account that runs this check, read from the system now: a baked data path, a checkout path in an assertion or a log text and a program database's path all "
               "carry the builder's account")
WHY_ACCOUNT_PATH = WHY_ACCOUNT + " (a short or ordinary name, so only as a path component)"
WHY_ACCOUNT_WORD = WHY_ACCOUNT + " (a name of four letters, so only as a whole word)"


def running_accounts():
    """The names of the account that runs this check, from the system when it runs: the user name and the name of the home folder (the two can differ on Windows). Nothing is stored."""
    names = []
    for getter in (getpass.getuser, lambda: os.path.basename(os.path.expanduser('~'))):
        try:
            names.append(getter())
        except Exception:  # getpass raises when no variable and no password database names a user
            pass
    names += [os.environ[v] for v in ('USERNAME', 'USER', 'LOGNAME') if os.environ.get(v)]
    return names


def account_forbidden(names):
    """[(bytes, why, label, whole_word)] to look for: each account name once (case does not matter); anywhere in the bytes when it has ACCOUNT_SUBSTRING_LETTERS letters or more, as a
    whole word when it has ACCOUNT_MIN_LETTERS, else between two separators. The label never spells the name."""
    found, seen = [], set()
    for name in names:
        name = (name or '').strip()
        if not name or name.lower() in seen:
            continue
        seen.add(name.lower())
        raw = name.encode('utf-8')
        if len(name) >= ACCOUNT_SUBSTRING_LETTERS and name.lower() not in GENERIC_ACCOUNTS:
            found.append((raw, WHY_ACCOUNT, "the name of the account that runs this check (%d letters, read from the system now; not printed)" % len(name), False))
        elif len(name) >= ACCOUNT_MIN_LETTERS and name.lower() not in GENERIC_ACCOUNTS:
            found.append((raw, WHY_ACCOUNT_WORD, "the name of the account that runs this check as a whole word (%d letters, read from the system now; not printed)" % len(name), True))
        else:
            for form in (b'/' + raw + b'/', b'\\' + raw + b'\\'):
                found.append((form, WHY_ACCOUNT_PATH, "the name of the account that runs this check as a path component (%d letters; not printed)" % len(name), False))
    return found


def hide_names(text, names):
    """The text with every account name that is a name (long enough, and not an ordinary folder's) replaced by <account>, so that a refusal does not repeat it: not in the bytes
    it quotes round a hit, and not in the checkout's path it names. A short or ordinary name is a word of the text and stays."""
    for name in sorted({n.strip() for n in names if n and len(n.strip()) >= ACCOUNT_MIN_LETTERS and n.strip().lower() not in GENERIC_ACCOUNTS}, key=len, reverse=True):
        text = re.sub(re.escape(name), '<account>', text, flags=re.I)
    return text


# THE DEVELOPER TOOLS' SENTINELS: whole lines of `MagicPortals --help` that game/main.cpp prints only inside `#if MAGICPORTALS_DEV_TOOLS`. test_release.py ties each one to
# that region of main.cpp, so a reworded help text cannot quietly make the check vacuous. NOT the flags' names (a program with the tools OFF holds them, to refuse them by
# name) and NOT a bare "DEV:" (the --saves entry keeps its DEV: wording in a program with the tools OFF, which docs/packaging.md says).
DEV_TOOLS_SENTINELS = (
    b"DEV: replay a win route (tests/routes/<level>.json, given",
    b"DEV: visit these levels in one process, taking the material set",
    b"DEV: burn 4-32's dark dragon on the game's tick <tick>, as a",
    b"DEV: hold a walk from the game's tick <from> to <to>, for a",
)
PERF_DIAG_MARKER = ap.PERF_DIAG_MARKER
VULKAN_LOADER = 'vulkan-1.dll'  # kVulkanLoaderName in game/sim/StartChecks.hpp, which test_release.py ties this to

# Facts of a refusal are of two kinds: the file is not the program of this platform (never allowed through), or it is a developer's build (--allow-dev).
STRUCTURE, DEVELOPER = 'structure', 'developer'
# A third kind (planning step 209): the program asks the system for more than the floor the caller named (--max-glibc). It is neither a wrong file nor a developer's build, so
# --allow-dev does not waive it.
FLOOR = 'floor'


class ReleaseError(Exception):
    pass


class BinaryError(Exception):
    pass


# ---- reading the three executable formats ----------------------------------------------------------------------------------------------------

def u16(data, at):
    return struct.unpack_from('<H', data, at)[0]


def u32(data, at):
    return struct.unpack_from('<I', data, at)[0]


def read_pe(data):
    """Facts of a PE file: machine, arch name, subsystem, DLL or not, imported DLL names (normal and delay-load). Raises BinaryError for anything that is not one."""
    if len(data) < 0x40 or data[:2] != b'MZ':
        raise BinaryError('not a PE file: no MZ header')
    lfanew = u32(data, 0x3C)
    if lfanew + 24 > len(data) or data[lfanew:lfanew + 4] != b'PE\0\0':
        raise BinaryError('not a PE file: no PE signature where the MZ header points')
    machine, sections, _, _, _, optsize, characteristics = struct.unpack_from('<HHIIIHH', data, lfanew + 4)
    opt = lfanew + 24
    magic = u16(data, opt)
    if magic not in (0x10B, 0x20B):
        raise BinaryError('not a PE file: optional header magic 0x%x' % magic)
    plus = magic == 0x20B
    subsystem = u16(data, opt + 68)
    count_at, dirs_at = (opt + 108, opt + 112) if plus else (opt + 92, opt + 96)
    dir_count = u32(data, count_at)
    table = opt + optsize
    spans = []
    for i in range(sections):
        _, vsize, vaddr, rawsize, rawptr = struct.unpack_from('<8sIIII', data, table + 40 * i)
        spans.append((vaddr, max(vsize, rawsize), rawptr))

    def offset(rva):
        for vaddr, size, rawptr in spans:
            if vaddr <= rva < vaddr + size:
                return rawptr + rva - vaddr
        raise BinaryError('an RVA (0x%x) in no section' % rva)

    def cstring(at):
        end = data.index(b'\0', at)
        return data[at:end].decode('ascii', 'replace')

    def directory(index):
        if index >= dir_count:
            return 0, 0
        return struct.unpack_from('<II', data, dirs_at + 8 * index)

    imports, delay = [], []
    rva, size = directory(1)
    if rva:
        at = offset(rva)
        while at + 20 <= len(data):
            name_rva = u32(data, at + 12)
            if name_rva == 0 and u32(data, at) == 0:
                break
            imports.append(cstring(offset(name_rva)))
            at += 20
    rva, size = directory(13)
    if rva:
        at = offset(rva)
        while at + 32 <= len(data):
            attrs, name_rva = struct.unpack_from('<II', data, at)
            if name_rva == 0 and attrs == 0:
                break
            delay.append(cstring(offset(name_rva) if attrs & 1 else name_rva))
            at += 32
    return {'format': 'PE', 'machine': machine, 'arch': PE_MACHINES.get(machine), 'subsystem': subsystem, 'dll': bool(characteristics & 0x2000),
            'plus': plus, 'imports': imports, 'delay_imports': delay}


def elf_section_names(data):
    """The names of the sections of an ELF64 little-endian file, read from its section header table and the section-name string table; [] for a file with no table (a
    program stripped to the bone has none). Raises BinaryError for a table that runs past the end of the file."""
    shoff = struct.unpack_from('<Q', data, 0x28)[0]
    shentsize, shnum, shstrndx = struct.unpack_from('<HHH', data, 0x3A)
    if shoff == 0 or shnum == 0 or shentsize < ELF_SECTION_HEADER_BYTES or shstrndx >= shnum:
        return []
    if shoff + shentsize * shnum > len(data):
        raise BinaryError('the section header table runs past the end of the file')

    def header(index):
        return struct.unpack_from('<IIQQQQ', data, shoff + index * shentsize)  # name, type, flags, address, file offset, size

    _, _, _, _, names_at, names_size = header(shstrndx)
    table = data[names_at:names_at + names_size]
    names = []
    for index in range(shnum):
        start = header(index)[0]
        end = table.find(b'\0', start)
        names.append(table[start:end if end != -1 else len(table)].decode('latin-1'))
    return names


def read_elf(data):
    if len(data) < 64 or data[:4] != b'\x7fELF':
        raise BinaryError('not an ELF file')
    cls, little = data[4], data[5]
    etype, machine = struct.unpack_from('<HH', data, 0x10) if little == 1 else struct.unpack_from('>HH', data, 0x10)
    sections = elf_section_names(data) if cls == 2 and little == 1 else []
    return {'format': 'ELF', 'class': cls, 'little': little == 1, 'type': etype, 'machine': machine, 'arch': ELF_MACHINES.get(machine), 'sections': sections}


def read_macho(data):
    """Facts of a Mach-O file, thin or fat: the slices' architectures and the file type of a thin one."""
    if len(data) >= 32 and data[:4] == MACHO_LE_MAGIC:
        cputype, _, filetype = struct.unpack_from('<III', data, 4)
        arch = MACHO_CPUS.get(cputype)
        return {'format': 'Mach-O', 'fat': False, 'archs': [arch] if arch else [], 'cputypes': [cputype], 'filetype': filetype}
    if len(data) >= 8 and data[:4] in (MACHO_FAT_MAGIC, MACHO_FAT64_MAGIC):
        count = struct.unpack_from('>I', data, 4)[0]
        if not 0 < count <= MOST_FAT_ARCHS:
            raise BinaryError('not a Mach-O file: %d "architectures" after the fat magic (a Java class file starts the same way)' % count)
        entry = 32 if data[:4] == MACHO_FAT64_MAGIC else 20
        archs, cputypes = [], []
        for i in range(count):
            at = 8 + entry * i
            if at + entry > len(data):
                raise BinaryError('a fat Mach-O file cut short')
            cputype = struct.unpack_from('>I', data, at)[0]
            cputypes.append(cputype)
            archs.append(MACHO_CPUS.get(cputype))
        return {'format': 'Mach-O', 'fat': True, 'archs': [a for a in archs if a], 'cputypes': cputypes, 'filetype': None}
    raise BinaryError('not a Mach-O file')


def utf16(text):
    return text.encode('utf-16-le')


def find_all(data, needle):
    """Offsets of `needle` in `data`, both already lower case."""
    at, found = data.find(needle), []
    while at != -1:
        found.append(at)
        at = data.find(needle, at + 1)
    return found


def printable(chunk):
    return re.sub(rb'[^\x20-\x7e]', b'.', chunk).decode('ascii')


LETTER_OR_DIGIT = re.compile(rb'[a-z0-9]')


def standalone(lowered, at, length, wide):
    """Whether the match at `at` is a whole word: the character before and the one after it are not an ASCII letter or digit (in UTF-16 the characters are two bytes)."""
    unit = 2 if wide else 1
    before, after = lowered[max(0, at - unit):at], lowered[at + length:at + length + unit]
    for side in (before, after):
        if len(side) == unit and LETTER_OR_DIGIT.fullmatch(side[:1]) and (not wide or side[1:] == b'\0'):
            return False
    return True


def occurrences(data, lowered, needle, word=False):
    """[(offset, 'ascii'|'utf-16', context)] of `needle` in `data`, in any case, in ASCII/UTF-8 and in UTF-16LE. `lowered` is data.lower(). With `word`, only a match that is a whole
    word (no letter or digit touches it) counts: that is how an account name of four letters is searched, so that it is found in a path and not in the middle of a longer word (a name of
    five letters or more is searched without it)."""
    found = []
    low = needle.lower()
    for at in find_all(lowered, low):
        if not word or standalone(lowered, at, len(low), False):
            found.append((at, 'ascii', printable(data[max(0, at - 24):at + len(needle) + 40])))
    wide = utf16(low.decode('latin-1'))
    for at in find_all(lowered, wide):
        if not word or standalone(lowered, at, len(wide), True):
            found.append((at, 'utf-16', printable(data[max(0, at - 48):at + len(wide) + 80]).replace('.', '')))
    return found


def pe_version_strings(data):
    """{'ProductVersion': '1.2.3.0', 'FileVersion': ...} from the UTF-16 keys of the version resource, and the binary quads (file, product) of its fixed block or None.
    BY SCANNING, not by walking the resource directory: the key is found, the zero padding after it skipped, the value read to its terminator."""
    strings = {}
    for key in ('ProductVersion', 'FileVersion'):
        needle = utf16(key) + b'\0\0'
        at = data.find(needle)
        if at == -1:
            continue
        at += len(needle)
        while at + 1 < len(data) and data[at:at + 2] == b'\0\0':
            at += 2
        end = at
        while end + 1 < len(data) and data[end:end + 2] != b'\0\0':
            end += 2
        strings[key] = data[at:end].decode('utf-16-le', 'replace')
    quads = None
    at = data.find(b'\xbd\x04\xef\xfe')
    if at != -1 and at + 24 <= len(data):
        file_ms, file_ls, prod_ms, prod_ls = struct.unpack_from('<IIII', data, at + 8)
        quads = (tuple(x for w in (file_ms, file_ls) for x in (w >> 16, w & 0xFFFF)), tuple(x for w in (prod_ms, prod_ls) for x in (w >> 16, w & 0xFFFF)))
    return strings, quads


def glibc_needs(data):
    """The highest GLIBC_x.y, GLIBCXX_x.y.z and CXXABI_x.y.z version names an ELF program asks of the system, as ((major, minor, ...)) tuples or None."""
    out = {}
    for tag in (b'GLIBC', b'GLIBCXX', b'CXXABI'):
        versions = set(re.findall(rb'(?<![A-Za-z_])' + tag + rb'_(\d+(?:\.\d+)+)\x00', data))
        if versions:
            out[tag.decode()] = max((tuple(int(p) for p in v.split(b'.')) for v in versions))
    return out


# ---- check-program --------------------------------------------------------------------------------------------------------------------------

class Report:
    """What check-program found: the facts it printed, the problems (each with its kind), and the program's arch."""

    def __init__(self, platform):
        self.platform = platform
        self.arch = None
        self.facts = []      # (status, text) with status 'ok', 'info' or 'FAIL'
        self.problems = []   # (kind, text)
        self.needs = {}
        self.pe = None
        self.stripped = None  # a Linux program: whether it carries no symbol table (None when it was not read as one)
        self.has_debug = False

    def ok(self, text):
        self.facts.append(('ok', text))

    def info(self, text):
        self.facts.append(('info', text))

    def fail(self, kind, text):
        self.facts.append(('FAIL', text))
        self.problems.append((kind, text))

    def blocking(self, allow_dev):
        """The problems that stop an archive: all of them, or with --allow-dev only the ones about the file not being this platform's program."""
        return [t for k, t in self.problems if not (allow_dev and k == DEVELOPER)]

    def developer(self):
        return [t for k, t in self.problems if k == DEVELOPER]


def checkout_forbidden(repo):
    """The checkout's own absolute path in both slash forms and its real path: whoever's account and wherever, a path of THIS tree in a program is a baked path."""
    found = []
    if repo:
        for path in {os.path.abspath(repo), os.path.realpath(repo)}:
            if len(path.rstrip('/\\')) > 8:
                for form in (path, path.replace('\\', '/'), path.replace('/', '\\')):
                    item = form.rstrip('/\\').encode('utf-8')
                    if item not in found:
                        found.append(item)
    return [(item, "this checkout's own path (%s)" % item.decode('utf-8', 'replace')) for item in found]


def check_program_bytes(data, platform, version=None, repo=None, also_forbid=(), max_glibc=None, accounts=None):
    """Everything check-program asks of a built program given as bytes. Returns a Report; never raises for a bad program (that is a fact), only for a bad question.
    `max_glibc` (a Linux program only, a tuple such as (2, 35)): the highest glibc the program may ask of the system. `accounts`: the names of the account that runs the check
    (default: read from the system now); a test names an invented one."""
    if platform not in CHECKED_PLATFORMS:
        raise ReleaseError('unknown platform %r (%s)' % (platform, ', '.join(CHECKED_PLATFORMS)))
    if max_glibc is not None and platform != LINUX:
        raise ReleaseError('--max-glibc is for a Linux program: %s has no glibc to name' % platform)
    version = version or vs.read_version(repo or vs.default_repo())
    r = Report(platform)

    # 1. the format and the machine
    wanted = ACCEPTED_ARCHS[platform]
    try:
        if platform == WINDOWS:
            pe = read_pe(data)
            r.arch = pe['arch']
            if pe['dll']:
                r.fail(STRUCTURE, 'the file is a DLL, not a program')
            if not pe['plus'] or pe['arch'] is None:
                r.fail(STRUCTURE, 'PE machine 0x%x%s is not one this packager has a layout for (%s)' % (pe['machine'], '' if pe['plus'] else ', 32-bit', ', '.join(wanted)))
            else:
                r.ok('format: PE, machine 0x%x (%s), subsystem %d (%s)' % (pe['machine'], pe['arch'], pe['subsystem'], {2: 'GUI', 3: 'console'}.get(pe['subsystem'], 'other')))
            r.pe = pe
        elif platform in (LINUX, ANDROID_LIB):
            elf = read_elf(data)
            r.arch = elf['arch']
            problems = []
            if elf['class'] != 2 or not elf['little']:
                problems.append('not a 64-bit little-endian ELF file')
            if elf['arch'] is None:
                problems.append('ELF e_machine %d is not one this packager has a layout for' % elf['machine'])
            elif elf['arch'] not in wanted:
                problems.append('ELF machine %s is not one for %s (%s)' % (elf['arch'], platform, ', '.join(wanted)))
            want_types = (3,) if platform == ANDROID_LIB else (2, 3)
            if elf['type'] not in want_types:
                problems.append('ELF type %d is not %s' % (elf['type'], 'a shared object (ET_DYN)' if platform == ANDROID_LIB else 'an executable (ET_EXEC or ET_DYN)'))
            if problems:
                for p in problems:
                    r.fail(STRUCTURE, p)
            else:
                r.ok('format: ELF64, little-endian, machine %d (%s), type %d' % (elf['machine'], elf['arch'], elf['type']))
                if platform == LINUX:
                    r.stripped = SYMBOL_TABLE_SECTION not in elf['sections']
                    r.has_debug = any(n.startswith('.debug') for n in elf['sections'])
        else:
            macho = read_macho(data)
            archs = macho['archs']
            if len(macho['cputypes']) != len(archs) or not archs:
                r.fail(STRUCTURE, 'Mach-O cputype %s is not one this packager has a layout for' % ', '.join('0x%x' % c for c in macho['cputypes']))
            else:
                r.arch = 'universal' if sorted(archs) == ['arm64', 'x86_64'] else archs[0] if len(archs) == 1 else '+'.join(sorted(archs))
                if r.arch not in wanted:
                    r.fail(STRUCTURE, 'Mach-O %s (%s) is not one for %s (%s)' % ('fat' if macho['fat'] else 'thin', '+'.join(archs), platform, ', '.join(wanted)))
                elif platform == IOS and macho['fat']:
                    r.fail(STRUCTURE, 'a fat Mach-O file is not an iOS program (arm64 only)')
                elif macho['filetype'] not in (None, MACHO_EXECUTE):
                    r.fail(STRUCTURE, 'Mach-O file type %d is not an executable (2)' % macho['filetype'])
                else:
                    r.ok('format: Mach-O, %s, %s' % ('fat' if macho['fat'] else 'thin', '+'.join(archs)))
    except (BinaryError, struct.error, ValueError, IndexError) as e:
        r.fail(STRUCTURE, 'not the program of this platform (%s): %s' % (platform, e))
    structure_failed = bool(r.problems)

    lowered = data.lower()
    # 2. no baked path, no checkout path, no builder's account
    names = list(running_accounts() if accounts is None else accounts)
    forbidden = ([(item, why, repr(item.decode('utf-8', 'replace')), False) for item, why in FORBIDDEN_STRINGS]
                 + [(i, w, repr(i.decode('utf-8', 'replace')), False) for i, w in checkout_forbidden(repo)]
                 + account_forbidden(names) + [(t.encode('utf-8'), '--also-forbid', repr(t), False) for t in also_forbid])
    clean = True
    for item, why, label, word in forbidden:
        hits = occurrences(data, lowered, item, word)
        if hits:
            clean = False
            at, form, context = hits[0]
            r.fail(DEVELOPER, hide_names('the program holds %s %d time(s) (first at byte %d, %s: ...%s...): %s' % (label, len(hits), at, form, context.strip(), why), names))
    if clean:
        r.ok('no baked path and no builder\'s account: none of %d forbidden strings occurs (ASCII or UTF-16, any case)' % len(forbidden))

    # 3. the developer tools are OFF
    found = [s for s in DEV_TOOLS_SENTINELS if s in data]
    if found:
        r.fail(DEVELOPER, 'the developer tools are compiled in (MAGICPORTALS_DEV_TOOLS is ON): the program holds %d of the %d help lines that exist only then, for example %r'
               % (len(found), len(DEV_TOOLS_SENTINELS), found[0].decode('utf-8', 'replace')))
    else:
        r.ok('developer tools OFF: none of the %d help lines that exist only with them ON is in the program' % len(DEV_TOOLS_SENTINELS))

    # 4. no performance readout
    if PERF_DIAG_MARKER in data:
        r.fail(DEVELOPER, 'the program is a PERFORMANCE-DIAGNOSTIC build (it holds %r): a tester\'s build, never a release' % PERF_DIAG_MARKER.decode())
    else:
        r.ok('no performance readout: the string %s is not in the program' % PERF_DIAG_MARKER.decode())

    # 5. it is the game (so that the absences above mean something)
    missing = [m for m in GAME_MARKERS if m not in data]
    if missing and not structure_failed:
        r.fail(STRUCTURE, 'the file is not this game\'s program: it lacks %s' % ', '.join(repr(m.decode()) for m in missing))
    elif not missing:
        r.ok('it is the game: it holds both of the strings every build of it carries')

    # 5b. the Vulkan loader is loaded late (a Windows program): with MAGICPORTALS_WIN_GUI=ON it is delay-loaded, so that a PC with no graphics driver is told in words
    # (docs/packaging.md section 6); a program that imports it at start gets Windows' own dialog, which names a DLL and nothing else
    if platform == WINDOWS and r.pe is not None and not structure_failed:
        started = [n for n in r.pe['imports'] if n.lower() == VULKAN_LOADER]
        late = [n for n in r.pe['delay_imports'] if n.lower() == VULKAN_LOADER]
        if started:
            r.fail(DEVELOPER, "%s is loaded when the program starts (an import, not a delay-load): a PC with no graphics driver would get Windows' own dialog and not the "
                              "game's words; a Windows release is made with MAGICPORTALS_WIN_GUI=ON" % started[0])
        elif not late:
            r.fail(DEVELOPER, 'the program delay-loads no %s: the check that says in words what is missing cannot be there' % VULKAN_LOADER)
        else:
            r.ok('%s is delay-loaded (loaded at the first Vulkan call, behind the check that says in words what is missing)' % late[0])

    # 6. the version (a Windows program)
    if platform == WINDOWS and not structure_failed:
        strings, quads = pe_version_strings(data)
        want = vs.windows_version(version)
        if not strings and quads is None:
            r.fail(DEVELOPER, 'the program has no version information (VERSIONINFO): a Windows release is made with MAGICPORTALS_WIN_GUI=ON, which writes it from the one version')
        else:
            for key in ('ProductVersion', 'FileVersion'):
                got = strings.get(key)
                if got is None:
                    r.fail(DEVELOPER, 'the version information has no %s' % key)
                elif got != want:
                    r.fail(DEVELOPER, '%s is %r and the one version is %s (its Windows form %s)' % (key, got, version, want))
                else:
                    r.ok('%s %s = the one version, in its four-field form' % (key, got))
            if quads is not None:
                wanted_quad = tuple(int(x) for x in want.split('.'))
                if quads != (wanted_quad, wanted_quad):
                    r.fail(DEVELOPER, 'the binary version block says file %s and product %s, not %s' % ('.'.join(map(str, quads[0])), '.'.join(map(str, quads[1])), want))

    # 7. what a Linux program asks of the system (information, for PLAYING.txt) and whether it carries its symbols
    if platform == LINUX and not structure_failed:
        r.needs = glibc_needs(data)
        if r.needs:
            r.info('the system must provide ' + ', '.join('%s %s' % (k, '.'.join(map(str, v))) for k, v in sorted(r.needs.items())) + ' or newer (the highest names the program asks for)')
        if 'GLIBCXX' not in r.needs and 'CXXABI' not in r.needs:
            r.info('the program asks the system for no libstdc++ version (no GLIBCXX or CXXABI name): the C++ runtime is not taken from the system, which is what linking it in gives')
        r.info('the program is %s' % ('stripped (it has no symbol table)' if r.stripped else 'NOT stripped (it carries a symbol table%s)' % (' and debug sections' if r.has_debug else '')))
        # 7b. the floor the caller named (--max-glibc): a program built on a newer system than the oldest one it should run on asks for a newer glibc than that
        if max_glibc is not None:
            got = r.needs.get('GLIBC')
            if got is None:
                r.fail(FLOOR, 'the program names no GLIBC version, so the floor of glibc %s cannot be checked' % '.'.join(map(str, max_glibc)))
            elif got > max_glibc:
                r.fail(FLOOR, 'the program asks for glibc %s and the floor is %s: it was built on a system newer than the oldest one it should run on (build it on that one)'
                       % ('.'.join(map(str, got)), '.'.join(map(str, max_glibc))))
            else:
                r.ok('glibc floor: the program asks for glibc %s, at most the %s asked for' % ('.'.join(map(str, got)), '.'.join(map(str, max_glibc))))
    return r


def print_report(report, allow_dev, out=None):
    out = out or sys.stdout
    for status, text in report.facts:
        shown = status
        if status == 'FAIL' and allow_dev and any(t == text and k == DEVELOPER for k, t in report.problems):
            shown = 'WARNING (--allow-dev)'
        print('  %-21s %s' % (shown, text), file=out)


def read_file(path):
    if not os.path.isfile(path) or os.path.islink(path):
        raise ReleaseError('%s is not a plain file' % path)
    with open(path, 'rb') as f:
        return f.read()


def parse_glibc_floor(text):
    """`--max-glibc 2.35` as the tuple (2, 35); None for no floor. Refuses anything that is not two or three numbers joined by dots."""
    if text is None:
        return None
    if not re.fullmatch(r'\d+\.\d+(?:\.\d+)?', text):
        raise ReleaseError('--max-glibc wants a glibc version such as 2.35, not %r' % text)
    return tuple(int(part) for part in text.split('.'))


def cmd_check_program(args):
    repo = os.path.abspath(args.repo) if args.repo else vs.default_repo()
    data = read_file(args.file)
    report = check_program_bytes(data, args.platform, repo=repo, also_forbid=args.also_forbid or (), max_glibc=parse_glibc_floor(args.max_glibc))
    # The program's path is the caller's own, and on a builder's PC it holds the account's folder: the header is the line that gets copied into a log, so it is hidden like the refusal.
    print(hide_names('check-program %s (%s, %d bytes, sha256 %s)' % (args.file, args.platform, len(data), hashlib.sha256(data).hexdigest()), running_accounts()))
    print_report(report, args.allow_dev)
    blocking = report.blocking(args.allow_dev)
    sys.stdout.flush()  # the facts above, then the refusal: one stream's order, even when both go to a file
    if blocking:
        print('check-program: REFUSED, %d fact(s) fail:' % len(blocking), file=sys.stderr)
        for text in blocking:
            print('  - ' + text, file=sys.stderr)
        return 1
    if report.developer():
        print('check-program: a DEVELOPER BUILD, allowed by --allow-dev (%d warning(s)): any archive made of it is named -dev' % len(report.developer()))
    else:
        print('check-program: ok -- %s, %s, a player\'s build' % (args.platform, report.arch))
    return 0


# ---- sums -----------------------------------------------------------------------------------------------------------------------------------

SUMS_FILE = 'SHA256SUMS.txt'
NOTICE_FILES = ('LICENSE', 'NOTICE.md')
# The files of a release folder: the archives of the five platforms and the notices that sit beside an APK. -dev marks a developer's archive, -shell a shell without the
# game and -nodata an archive made with `assemble --no-data` (planning step 209, PORT10-6 b): none of the three is a release, and none is listed in a release's sums.
ARTIFACT = re.compile(r'^MagicPortals-\d+\.\d+\.\d+(?:-[A-Za-z0-9_]+)*\.(?:zip|tar\.gz|apk|ipa)$')


def release_files(folder):
    """The names a release folder's sums cover: the archives and the notice files, sorted."""
    return sorted(n for n in os.listdir(folder) if os.path.isfile(os.path.join(folder, n)) and (ARTIFACT.match(n) or n in NOTICE_FILES))


def is_developer_name(name):
    """Whether an archive's name says it is not a release: -dev (a developer's build, a smoke archive), -shell (a shell without the game) or -nodata (an archive made with
    --no-data, a program without the game). A release folder is the whole game, so the last two are left out of its sums the way the first is."""
    return bool(re.search(r'-(?:dev|shell|nodata)(?:\.|-)', name))


def sums_text(folder, names):
    return ''.join('%s  %s\n' % (pk.sha256_of(os.path.join(folder, n)), n) for n in names)


def write_sums(folder, allow_dev=False, skip_dev=False):
    """SHA256SUMS.txt over the release files of `folder`. A developer's archive in it is an error for `sums`, and is simply left out by an assembly, which writes a release's
    sums beside whatever an earlier smoke run left (skip_dev)."""
    names = release_files(folder)
    if skip_dev:
        names = [n for n in names if not is_developer_name(n)]
    dev = [n for n in names if is_developer_name(n)]
    if dev and not allow_dev:
        raise ReleaseError('%s is a developer\'s archive, a shell or an archive with no game (-dev, -shell or -nodata in its name) and a release folder\'s sums do not list one '
                           '(--allow-dev for a smoke check)' % dev[0])
    if not names:
        raise ReleaseError('%s holds no release archive (MagicPortals-<version>-<platform>...)' % folder)
    pk.write_text(os.path.join(folder, SUMS_FILE), sums_text(folder, names))
    return names


def verify_sums(folder):
    """Problems (list of text) of `folder` against its SHA256SUMS.txt in `sha256sum -c` form: a changed file, a missing one, an unlisted release file."""
    path = os.path.join(folder, SUMS_FILE)
    try:
        with open(path, encoding='utf-8') as f:
            lines = [l.rstrip('\n') for l in f if l.strip()]
    except OSError as e:
        return ['%s cannot be read: %s' % (SUMS_FILE, e)]
    problems, listed = [], set()
    for line in lines:
        m = re.fullmatch(r'([0-9a-f]{64})  (.+)', line)
        if not m:
            problems.append('%s: a line that is not `<sha256>  <name>`: %r' % (SUMS_FILE, line))
            continue
        digest, name = m.groups()
        listed.add(name)
        full = os.path.join(folder, name)
        if '/' in name or '\\' in name or not os.path.isfile(full):
            problems.append('%s: %s is listed and is not a file of this folder' % (SUMS_FILE, name))
        elif pk.sha256_of(full) != digest:
            problems.append('%s: the sha256 of %s differs' % (SUMS_FILE, name))
    for name in release_files(folder):
        if name not in listed and not is_developer_name(name):
            problems.append('%s is in the folder and not in %s' % (name, SUMS_FILE))
    return problems


def cmd_sums(args):
    folder = os.path.abspath(args.dir)
    if not os.path.isdir(folder):
        raise ReleaseError('%s is not a folder' % folder)
    if args.check:
        problems = verify_sums(folder)
        for p in problems:
            print('SUMS: ' + p)
        print('sums --check: %s' % ('FAILED, %d problem(s)' % len(problems) if problems else 'ok (%d files)' % len(release_files(folder))))
        return 1 if problems else 0
    names = write_sums(folder, args.allow_dev)
    print('%s: %d file(s): %s' % (os.path.join(folder, SUMS_FILE), len(names), ', '.join(names)))
    return 0


# ---- check-version ---------------------------------------------------------------------------------------------------------------------------

def cmd_check_version(args):
    return cv.main(['--repo', os.path.abspath(args.repo)] if args.repo else [])


# ---- the pieces of an archive -------------------------------------------------------------------------------------------------------------------

PLAYING_FILE = 'PLAYING.txt'
DEV_MARKER_FILE = 'DEVELOPER BUILD.txt'
READ_ME_FILE = 'READ ME FIRST.txt'
SEAL_FILE = 'GAME DATA AND THE SEAL.txt'
PROGRAM_NAME = {WINDOWS: 'MagicPortals.exe', LINUX: 'MagicPortals'}
PROGRAM_MODE, FILE_MODE, DIR_MODE = 0o755, 0o644, 0o755
# The Visual C++ runtime's DLL names: the ones the program's import tables name are the ones --vc-redist must supply. The redistributable holds more than the numbered ones
# (MSVCP140_1.dll, MSVCP140_ATOMIC_WAIT.dll, MSVCP140_CODECVT_IDS.dll, VCRUNTIME140_1.dll, VCRUNTIME140_THREADS.dll), so MSVCP and VCRUNTIME take any word after the number:
# a program that imported one of them would otherwise be packaged without it while PLAYING.txt says the folder carries the runtime the program loads.
RUNTIME_DLL = re.compile(r'^(?:MSVCP\d+(?:_[A-Za-z0-9_]+)?|VCRUNTIME\d+(?:_[A-Za-z0-9_]+)?|CONCRT\d+|VCOMP\d+)\.dll$', re.I)
PE_DLL_FLAG = 0x2000
FILE_TYPE_BITS, FOLDER_TYPE_BITS, ZIP_TYPE_MASK = 0o100000, 0o040000, 0o170000  # S_IFREG, S_IFDIR, S_IFMT: a zip entry's mode word carries the file type with the permissions
STORED_SUFFIXES = tuple(ap.STORED_EXTENSIONS)
TAR_MTIME = pk.FIXED_TIME


def archive_name(version, platform, arch, kind=None, dev=False, ext='.zip'):
    """MagicPortals-<version>-<platform>-<arch>[-<kind>][-dev].<ext>; kind is nodata, shell or simulator. (An APK has no arch: android.)"""
    parts = ['MagicPortals', version, platform] + ([arch] if arch else []) + ([kind] if kind else []) + (['dev'] if dev else [])
    return '-'.join(parts) + ext


def entry_bytes(source):
    if isinstance(source, bytes):
        return source
    with open(source, 'rb') as f:
        return f.read()


def tree_entries(root, prefix, program_name=None):
    """[(archive name, source path, mode)] for every file under `root`, sorted. The mode is by rule and not from the file system (Git Bash shows 755 for everything)."""
    out = []
    for rel, _ in pk.listing(root):
        out.append((prefix + rel, os.path.join(root, *rel.split('/')), PROGRAM_MODE if rel == program_name else FILE_MODE))
    return out


def check_entries(entries):
    names = [n for n, _, _ in entries]
    if len(set(names)) != len(names):
        dup = sorted(n for n in set(names) if names.count(n) > 1)
        raise ReleaseError('the archive would hold %s twice' % dup[0])
    for n in names:
        why = ap.safe_rel_path(n)
        if why:
            raise ReleaseError('%r cannot go in an archive: %s' % (n, why))


def write_zip(path, entries, first=None, folders=False):
    """A zip over [(name, source, mode)]: sorted, one fixed date, mode words by rule, deflated (stored for pictures and sounds, as the APK does). Same bytes twice."""
    check_entries(entries)
    rows = [(name, source, mode) for name, source, mode in entries]
    if folders:
        # a folder entry for every parent, as `zip -r` and ditto write them (an .ipa is expected to start with Payload/)
        parents = sorted({'/'.join(n.split('/')[:i]) + '/' for n, _, _ in entries for i in range(1, len(n.split('/')))})
        rows += [(p, None, DIR_MODE) for p in parents]
    key = (lambda e: (0 if first and e[0].startswith(first) else 1, e[0]))
    with zipfile.ZipFile(path, 'w') as z:
        for name, source, mode in sorted(rows, key=key):
            if source is None:
                info = zipfile.ZipInfo(name, date_time=ap.ZIP_DATE)
                info.create_system = 3
                info.external_attr = ((FOLDER_TYPE_BITS | mode) << 16) | 0x10
                z.writestr(info, b'')
                continue
            info = ap.zip_info(name, ap.is_stored(name))
            info.external_attr = (FILE_TYPE_BITS | mode) << 16
            z.writestr(info, entry_bytes(source), compresslevel=9)


def write_targz(path, entries):
    """A gzip'd GNU tar over [(name, source, mode)]: directories first (0755), then files, sorted; uid and gid 0, no owner names, one fixed time; the gzip header holds
    no name and no time. The executable bit is the mode word, kept by tar (a zip tool would not)."""
    check_entries(entries)
    folders = sorted({'/'.join(n.split('/')[:i]) for n, _, _ in entries for i in range(1, len(n.split('/')))})
    raw = io.BytesIO()
    with tarfile.open(fileobj=raw, mode='w', format=tarfile.GNU_FORMAT) as tf:
        for folder in folders:
            info = tarfile.TarInfo(folder)
            info.type, info.mode, info.mtime = tarfile.DIRTYPE, DIR_MODE, TAR_MTIME
            tf.addfile(info)
        for name, source, mode in sorted(entries, key=lambda e: e[0]):
            data = entry_bytes(source)
            info = tarfile.TarInfo(name)
            info.size, info.mode, info.mtime = len(data), mode, TAR_MTIME
            tf.addfile(info, io.BytesIO(data))
    with open(path, 'wb') as f:
        with gzip.GzipFile(filename='', mode='wb', fileobj=f, mtime=0, compresslevel=9) as g:
            g.write(raw.getvalue())


def is_zip(path):
    """Whether the file is a zip (an .ipa is one), by its first bytes: the archive is re-read under a temporary name, so the extension cannot say."""
    with open(path, 'rb') as f:
        return f.read(2) == b'PK'


def read_archive(path):
    """{name: (size, sha256, mode)} of the files of a .zip, .ipa or .tar.gz, read back from the bytes on disk."""
    found = {}
    if is_zip(path):
        with zipfile.ZipFile(path) as z:
            for info in z.infolist():
                if not info.filename.endswith('/'):
                    data = z.read(info)
                    found[info.filename] = (len(data), hashlib.sha256(data).hexdigest(), (info.external_attr >> 16) & 0o777)
    else:
        with tarfile.open(path, 'r:gz') as tf:
            for member in tf.getmembers():
                if member.isfile():
                    data = tf.extractfile(member).read()
                    found[member.name] = (len(data), hashlib.sha256(data).hexdigest(), member.mode & 0o777)
    return found


def extract_prefix(path, prefix, dest):
    """Writes the files of the archive under `prefix` into `dest` (without the prefix). Used to hand the content inside an archive to package.py verify."""
    count = 0
    members = []
    if is_zip(path):
        with zipfile.ZipFile(path) as z:
            members = [(i.filename, z.read(i)) for i in z.infolist() if i.filename.startswith(prefix) and not i.filename.endswith('/')]
    else:
        with tarfile.open(path, 'r:gz') as tf:
            members = [(m.name, tf.extractfile(m).read()) for m in tf.getmembers() if m.isfile() and m.name.startswith(prefix)]
    for name, data in members:
        target = os.path.join(dest, *name[len(prefix):].split('/'))
        os.makedirs(os.path.dirname(target), exist_ok=True)
        with open(target, 'wb') as f:
            f.write(data)
        count += 1
    return count


def verify_archive(path, entries, content_prefix=None):
    """Re-reads the archive and compares it, file by file, with what was meant to go in (name, size, sha256, mode); then, when it holds a content folder, runs
    `package.py verify` on that folder as unpacked from the archive. Returns the problems."""
    problems = []
    want = {}
    for name, source, mode in entries:
        data = entry_bytes(source)
        want[name] = (len(data), hashlib.sha256(data).hexdigest(), mode)
    got = read_archive(path)
    for name in sorted(set(want) - set(got)):
        problems.append('missing from the archive: ' + name)
    for name in sorted(set(got) - set(want)):
        problems.append('in the archive and not meant to be: ' + name)
    for name in sorted(set(want) & set(got)):
        if want[name][:2] != got[name][:2]:
            problems.append('differs from what was written: ' + name)
        elif want[name][2] != got[name][2]:
            problems.append('mode %o, meant %o: %s' % (got[name][2], want[name][2], name))
    if content_prefix and not problems:
        scratch = tempfile.mkdtemp(prefix='mp-release-verify-')
        try:
            if extract_prefix(path, content_prefix, scratch) == 0:
                problems.append('the archive holds no %s' % content_prefix)
            else:
                problems += ['content inside the archive: ' + p for p in pk.verify_content(scratch)['problems']]
        finally:
            shutil.rmtree(scratch, ignore_errors=True)
    return problems


# ---- inputs every archive needs ---------------------------------------------------------------------------------------------------------------

def find_notices(repo, notices):
    """{name: bytes} of LICENSE and NOTICE.md, from the folder --notices names or the repository root. A release archive is never written without them, and they are
    copied unchanged: what they say is the repository's own text (planning step 210), not this tool's."""
    folder = os.path.abspath(notices) if notices else repo
    out, missing = {}, []
    for name in NOTICE_FILES:
        path = os.path.join(folder, name)
        if not os.path.isfile(path) or os.path.islink(path):
            missing.append(name)
            continue
        data = read_file(path)
        if not data.strip():
            missing.append(name + ' (it is empty)')
        out[name] = data
    if missing:
        raise ReleaseError('a release archive carries the licence and the notices, and %s has no %s. LICENSE and NOTICE.md are at the root of the repository; name the folder '
                           'that holds them with --notices' % (folder, ' and no '.join(missing)))
    return out


def check_output_folder(out, repo, *inputs):
    """The archive goes outside the tracked tree (or under the gitignored out/), and not into a folder it reads."""
    out = os.path.abspath(out)
    try:
        ap.check_out(out, repo)
    except ap.PackError as e:
        raise ReleaseError(str(e))
    for folder in inputs:
        if folder and (pk.inside(pk.real(out), pk.real(folder)) or pk.inside(pk.real(folder), pk.real(out))):
            raise ReleaseError('the output folder %s overlaps %s, which the archive is made from' % (out, folder))
    return out


def refuse_if_present(out, name):
    target = os.path.join(out, name)
    if os.path.lexists(target):
        raise ReleaseError('%s already exists: an archive is never written over another (remove it, or give another --out)' % target)
    return target


def vc_redist_entries(redist, pe, prefix):
    """The runtime DLLs the program imports, taken from the folder `redist` (Microsoft's redistributable): each must be there and be a PE DLL of the program's machine type.
    Returns [(archive name, bytes, mode)]; raises with every problem."""
    needed = sorted({n for n in pe['imports'] + pe['delay_imports'] if RUNTIME_DLL.match(n)}, key=str.lower)
    if not needed:
        raise ReleaseError('--vc-redist was given and the program imports no Visual C++ runtime DLL (it has a static runtime?): there is nothing to copy')
    if not os.path.isdir(redist):
        raise ReleaseError('--vc-redist %s is not a folder' % redist)
    names = {n.lower(): n for n in os.listdir(redist)}
    out, problems = [], []
    for dll in needed:
        actual = names.get(dll.lower())
        if actual is None:
            problems.append('%s is imported by the program and is not in %s' % (dll, redist))
            continue
        data = read_file(os.path.join(redist, actual))
        try:
            facts = read_pe(data)
        except BinaryError as e:
            problems.append('%s is not a PE file (%s)' % (actual, e))
            continue
        if not facts['dll']:
            problems.append('%s is a program, not a DLL' % actual)
        elif facts['machine'] != pe['machine'] or facts['plus'] != pe['plus']:
            problems.append('%s is for machine 0x%x and the program is for 0x%x' % (actual, facts['machine'], pe['machine']))
        else:
            out.append((prefix + dll, data, FILE_MODE))
    if problems:
        raise ReleaseError('the Visual C++ runtime for the program is not in --vc-redist:\n  ' + '\n  '.join(problems))
    return out


# ---- the words the player is given -----------------------------------------------------------------------------------------------------------------

def version_line(version, platform, arch):
    return 'Magic Portals Enhanced %s - %s (%s)' % (version, platform, arch)


def runtime_paragraph(redist_names):
    if redist_names:
        return ('  * The Microsoft Visual C++ runtime: this folder carries the runtime DLLs the program loads (%s), copied unchanged from Microsoft\'s redistributable, so a PC that\n'
                '    lacks the redistributable can still start the game. (Windows\' own C and system libraries are not in this folder; they are part of Windows.)' % ', '.join(redist_names))
    return ('  * The Microsoft Visual C++ runtime. This archive does NOT carry it. A PC without the "Microsoft Visual C++ Redistributable for Visual Studio 2015-2022\n'
            '    (x64)" shows Windows\' own message ("the code execution cannot proceed because VCRUNTIME140.dll was not found", or MSVCP140.dll) BEFORE the game starts, and no\n'
            '    message of the game can help it. Install the redistributable from Microsoft, then start the game again.')


def playing_windows(version, arch, redist_names, data, dev):
    lines = [version_line(version, 'Windows', arch), '=' * 60, '',
             'TO PLAY',
             '  1. Unpack the whole zip into a folder you may write to (for example inside Documents). Do not run the game from inside the zip.',
             '  2. Start MagicPortals.exe. It must stay next to the folders "assets" (the engine\'s shaders) and "content" (the game).' if data else
             '  2. Start MagicPortals.exe. It must stay next to the folder "assets" (the engine\'s shaders). THIS ARCHIVE HAS NO GAME DATA: the game will say so and stop.',
             '  The engine keeps a small cache beside the program (assets\\shaders); a folder you cannot write to loses it.', '',
             'WHAT YOUR PC NEEDS',
             '  * 64-bit Windows. The game has been run on Windows 11; whether it starts on Windows 10 was not tried.',
             '  * A graphics driver that provides Vulkan 1.2 or newer (NVIDIA, AMD or Intel: from the maker, or Windows Update).',
             runtime_paragraph(redist_names),
             '  * On a Windows "N" edition: the Media Feature Pack. The game decodes its sounds with Windows Media Foundation, and without the pack Windows shows a message about',
             '    MFPlat.DLL before the game starts.',
             '  The last two points are a reading of the program\'s import table; they were not seen on a PC that lacked the runtime or the pack.', '',
             'IF IT DOES NOT START',
             '  * With no usable Vulkan driver the game shows ONE message box, beginning "No Vulkan graphics driver was found" (or "The Vulkan graphics driver cannot be used"),',
             '    and names vulkan-1.dll; the same words are in the log. They are written in the program and a test checks them; the box has not been seen on a PC without a driver.',
             '  * Read the log (next section) and send it with a report.', '',
             'WHERE THE GAME WRITES',
             '  * Your medals, the sound and music switches, the achievements and your control choices: %APPDATA%\\Magic Portals (scores.json, audio.json, achievements.json,',
             '    controls.json). Delete the folder to start from nothing.',
             '  * The log of the last run: %TEMP%\\magicportals.log (the run before it: magicportals.prev.log), and files the game can make again in %TEMP%\\supersonic-magicportals.', '',
             'THIS ARCHIVE',
             ('  content\\manifest.json lists every file of the game with its size and SHA-256; SHA256SUMS.txt, beside the archive, has the archive\'s own.' if data else
              '  This archive has no content folder and no SHA256SUMS.txt: it is a test archive of the program alone.'),
             '  LICENSE and NOTICE.md are the licence and the notices. The program is MagicPortals.exe; it has no installer and changes nothing outside the places above.']
    return finish(lines, dev)


def playing_linux(version, arch, needs, data, dev, stripped=None):
    glibc, glibcxx = needs.get('GLIBC'), needs.get('GLIBCXX')
    if glibc:
        # The floor is whatever the machine that built the program had (planning step 209, PORT10-3): it is read from the program and said here, with the version. A program
        # with libstdc++ and libgcc linked in (build_linux.sh --portable) names no GLIBCXX, and the line then says that the C++ runtime is not asked of the system.
        need_line = ('  * glibc %s or newer%s: the highest versions the program asks for (read from the program). A system with an older glibc will\n'
                     '    refuse it with a "version `GLIBC_...\' not found" message.%s'
                     % ('.'.join(map(str, glibc)), (' and a libstdc++ with GLIBCXX %s' % '.'.join(map(str, glibcxx))) if glibcxx else '',
                        '' if glibcxx else '\n    It asks the system for no libstdc++ version: the C++ runtime is inside the program.'))
    else:
        need_line = '  * The system libraries the program names (libc, libstdc++, libgcc_s, libm).'
    stripped_line = ('  The program is stripped (it carries no symbol table); the packager does not change it.' if stripped else
                     '  The program is not stripped (the packager does not change it).')
    lines = [version_line(version, 'Linux', arch), '=' * 60, '',
             'TO PLAY',
             '  1. Unpack with tar, which keeps the program\'s executable bit:   tar xzf <this archive>   If you unpacked it some other way, restore the bit:   chmod +x MagicPortals',
             '  2. Start ./MagicPortals from a terminal, or from your file manager (in a desktop session). It must stay next to the folders "assets" (the engine\'s shaders) and',
             '     "content" (the game).' if data else
             '     "content" (the game). THIS ARCHIVE HAS NO GAME DATA: the game will say so and stop.',
             '  The engine keeps a small cache beside the program (assets/shaders); a folder you cannot write to loses it.', '',
             'WHAT YOUR PC NEEDS',
             '  * 64-bit Linux on an x86-64 CPU, in a desktop session with an X11 display. The program opens libX11 at run time; on a Wayland desktop it needs XWayland, which',
             '    was not tried.',
             '  * A Vulkan 1.2 driver and the Vulkan loader, libvulkan.so.1 (Mesa\'s radv, anv or lavapipe, or the vendor\'s driver; Debian and Ubuntu: libvulkan1).',
             '  * libasound.so.2 (ALSA) for sound (Debian and Ubuntu: libasound2t64).',
             need_line,
             stripped_line, '',
             'IF IT DOES NOT START',
             '  * With the Vulkan loader but no driver the game ends at once, exit code 1, with one line on the terminal and in the log: "fatal: GLFW could not report the required Vulkan instance extensions. This usually means no Vulkan loader or ICD is installed."',
             '    That was measured by telling the loader to look for no driver files (VK_DRIVER_FILES), not on a PC without one. Without libvulkan.so.1 itself your system answers before the game starts ("error while loading shared libraries"; expected, not run). The friendly check the Windows program has does not exist on Linux.',
             '  * Read the log (next section) and send it with a report.', '',
             'WHERE THE GAME WRITES',
             '  * Your medals, the sound and music switches, the achievements and your control choices: ~/.local/share/Magic Portals (or under $XDG_DATA_HOME).',
             '    Delete the folder to start from nothing.',
             '  * The log of the last run: <the temporary folder, usually /tmp>/magicportals.log (the run before it: magicportals.prev.log).', '',
             'THIS ARCHIVE',
             ('  content/manifest.json lists every file of the game with its size and SHA-256; SHA256SUMS.txt, beside the archive, has the archive\'s own.' if data else
              '  This archive has no content folder and no SHA256SUMS.txt: it is a test archive of the program alone.'),
             '  LICENSE and NOTICE.md are the licence and the notices. The program is MagicPortals; it has no installer and changes nothing outside the places above.']
    return finish(lines, dev)


def finish(lines, dev):
    if dev:
        lines = ['*** THIS IS A DEVELOPER BUILD (see %s). It is not a release. ***' % DEV_MARKER_FILE, ''] + lines
    return '\n'.join(lines) + '\n'


def developer_note(report):
    return ('DEVELOPER BUILD\n\nThis archive was made with --allow-dev from a program that is a developer\'s build. It is a smoke check of the packager and not a release; its name\n'
            'carries -dev, and a release folder\'s sums refuse it. check-program said:\n\n' + '\n'.join('  - ' + t for t in report.developer()) + '\n')


def seal_text(platform, version, with_data_dir, notices_beside):
    """What the archive says about the data and the seal. Written into the archive, and printed. Names only what docs/apple.md section 7 names."""
    if platform == MACOS:
        return ('THE GAME\'S DATA IS IN THIS APP, AND THE APP\'S SEAL IS NO LONGER VALID\n\n'
                'tools/apple/make_app.sh signs MagicPortals.app ad hoc as the LAST step of making the shell ("codesign --force --sign - --timestamp=none"). A signature covers\n'
                'every file in the bundle, and the packager has since added the folder %s to it. The seal is therefore INVALID, and a Mac may say that the app "is damaged\n'
                'and can\'t be opened".\n\n'
                'The seal must be made again, now that the data is in: on a Mac, or by a signer run on another machine. The packager that wrote this archive CANNOT sign (it runs on a\n'
                'PC), and nothing here has ever been run on a Mac. docs/apple.md section 7 lists the three routes, none proven: (1) a Mac assembles and signs the app; (2) a\n'
                'cross-platform ad-hoc signer such as rcodesign, run on the PC that holds the data, signs it again; (3) the shell WITHOUT data, plus a folder the player puts into\n'
                '~/Library/Application Support/Magic Portals/GameData. Which route is the owner\'s decision.\n\n'
                'The LICENSE and NOTICE.md beside the app are this archive\'s licence and notices; the bundle\'s own Resources/licenses folder holds them too, with the engine\'s.\n'
                % with_data_dir)
    return ('THE GAME\'S DATA IS IN THIS BUNDLE\n\n'
            'The folder %s was added at the root of the app after the shell was made and signed (ad hoc only). A sideloading tool (Sideloadly, AltStore, SideStore) signs the whole\n'
            'bundle again with the player\'s own Apple ID when it installs it, so the ad-hoc signature not covering the new files is expected to be harmless (docs/apple.md section 7:\n'
            'inferred, never run). The packager that wrote this archive cannot sign, and nothing iOS has ever run on a device or a simulator. Which route the\n'
            'data takes to a player is the owner\'s decision.\n' % with_data_dir)


# ---- assemble: Windows and Linux ---------------------------------------------------------------------------------------------------------------

def swallow(function, *a, **kw):
    """Runs one of package.py's commands without its progress line (this tool says its own)."""
    with contextlib.redirect_stdout(io.StringIO()):
        return function(*a, **kw)


def refusal_text(report, allow_dev):
    return '\n  - '.join([''] + report.blocking(allow_dev))


def assemble_desktop(args):
    platform = args.platform
    repo = os.path.abspath(args.repo) if args.repo else vs.default_repo()
    version = vs.read_version(repo)
    if args.shell:
        raise ReleaseError('--shell is for macos and ios; a %s archive is made of --program' % platform)
    if not args.program:
        raise ReleaseError('give --program <the built program>')
    if bool(args.content) == bool(args.no_data):
        raise ReleaseError('give exactly one of --content <content package> and --no-data (an archive with no game data in it)')
    if args.vc_redist and platform != WINDOWS:
        raise ReleaseError('--vc-redist is for a Windows archive: the Visual C++ runtime is a Windows matter')
    program = os.path.abspath(args.program)
    data = read_file(program)

    # Every refusal comes before anything is staged or written.
    report = check_program_bytes(data, platform, version=version, repo=repo, also_forbid=args.also_forbid or (), max_glibc=parse_glibc_floor(args.max_glibc))
    if report.blocking(args.allow_dev):
        raise ReleaseError('check-program refuses %s (%d fact(s) fail):%s' % (program, len(report.blocking(args.allow_dev)), refusal_text(report, args.allow_dev)))
    dev = bool(report.developer())
    notices = find_notices(repo, args.notices)
    content = os.path.abspath(args.content) if args.content else None
    shaders = os.path.abspath(args.shaders) if args.shaders else os.path.join(repo, 'engine', 'assets', 'shaders')
    spv = pk.shader_files(shaders)
    content_report = None
    if content:
        content_report = pk.verify_content(content)
        if content_report['problems']:
            problems = content_report['problems']
            raise ReleaseError('the content package fails package.py verify (%d problem(s)):\n  %s%s' % (len(problems), '\n  '.join(problems[:10]),
                                                                                                    '\n  ...' if len(problems) > 10 else ''))
    out = check_output_folder(args.out, repo, content)
    ext = '.zip' if platform == WINDOWS else '.tar.gz'
    name = archive_name(version, platform, report.arch, None if content else 'nodata', dev, ext)
    target = refuse_if_present(out, name)
    prefix = name[:-len(ext)] + '/'
    redist = args.vc_redist and os.path.abspath(args.vc_redist)
    redist_entries = vc_redist_entries(redist, report.pe, prefix) if redist else []

    stage = tempfile.mkdtemp(prefix='mp-release-stage-')
    part = target + '.part'
    try:
        shell = os.path.join(stage, 'shell')
        if content:
            swallow(pk.cmd_shell, types.SimpleNamespace(content=content, exe=program, out=shell, shaders=shaders, repo=None))
        else:
            os.makedirs(os.path.join(shell, 'assets', 'shaders'))
            shutil.copyfile(program, os.path.join(shell, os.path.basename(program)))
            for n in spv:
                shutil.copyfile(os.path.join(shaders, n), os.path.join(shell, 'assets', 'shaders', n))
        entries = []
        for rel, _ in pk.listing(shell):
            if rel == 'shell.json':
                continue  # the layout proof of package.py shell: it lists the folder before the notices go in, and an archive has its own record
            source = os.path.join(shell, *rel.split('/'))
            if rel == os.path.basename(program):
                entries.append((prefix + PROGRAM_NAME[platform], source, PROGRAM_MODE))
            else:
                entries.append((prefix + rel, source, FILE_MODE))
        entries += [(prefix + n, d, FILE_MODE) for n, d in sorted(notices.items())]
        entries += redist_entries
        redist_names = sorted(n[len(prefix):] for n, _, _ in redist_entries)
        text = (playing_windows(version, report.arch, redist_names, bool(content), dev) if platform == WINDOWS
                else playing_linux(version, report.arch, report.needs, bool(content), dev, report.stripped))
        entries.append((prefix + PLAYING_FILE, text.encode('utf-8'), FILE_MODE))
        if dev:
            entries.append((prefix + DEV_MARKER_FILE, developer_note(report).encode('utf-8'), FILE_MODE))
        os.makedirs(out, exist_ok=True)
        if platform == WINDOWS:
            write_zip(part, entries)
        else:
            write_targz(part, entries)
        problems = verify_archive(part, entries, prefix + 'content/' if content else None)
        if problems:
            raise ReleaseError('the archive fails its own re-read (%d problem(s)):\n  %s' % (len(problems), '\n  '.join(problems[:10])))
        os.replace(part, target)
    finally:
        shutil.rmtree(stage, ignore_errors=True)
        if os.path.exists(part):
            os.remove(part)
    # An archive with no game in it (--no-data) is no release and is left out of the sums as the Apple shell without content is (PORT10-6 b): writing the sums over a folder
    # that holds only that archive would also stop at "holds no release archive" after the archive was written.
    sums = None if (dev or not content) else write_sums(out, skip_dev=True)
    print('archive: %s' % target)
    print('  %d bytes, sha256 %s' % (os.path.getsize(target), pk.sha256_of(target)))
    print('  program: %s %s%s' % (platform, report.arch, ' -- a DEVELOPER BUILD (--allow-dev), the name carries -dev' if dev else ", a player's build"))
    print('  content: %s' % ('%d files, %d bytes (package.py verify ok, re-run on the copy inside the archive)' % (content_report['summary']['files'], content_report['summary']['bytes'])
                             if content else 'none (--no-data)'))
    print('  runtime: %s' % ('%s copied from %s' % (', '.join(redist_names), redist) if redist_names else ('the Visual C++ runtime is NOT carried (PLAYING.txt says what a PC without it shows)'
                                                                                                         if platform == WINDOWS else 'the system\'s')))
    print('  %s' % ('no %s written for a developer\'s archive' % SUMS_FILE if dev else
                    'no %s written for an archive with no game (-nodata is no release)' % SUMS_FILE if not content else '%s: %s' % (SUMS_FILE, ', '.join(sums))))
    return 0


# ---- assemble: macOS and iOS ----------------------------------------------------------------------------------------------------------------

# Where the game looks for its data inside a bundle (game/sim/AppleLayout.hpp, docs/packaging.md section 7): macOS in Resources, iOS at the bundle's root.
GAME_DATA_IN_BUNDLE = {MACOS: 'Contents/Resources/GameData', IOS: 'GameData'}


class Shell:
    """A shell as make_app.sh made it, read into memory: its files (name -> (bytes, mode)), where its bundle, program and GameData go, and what kind it is."""

    def __init__(self, files, bundle, exe, data_dir, top, platform, ipa, simulator):
        self.files, self.bundle, self.exe, self.data_dir, self.top = files, bundle, exe, data_dir, top
        self.platform, self.ipa, self.simulator = platform, ipa, simulator


def shell_files_from_zip(path):
    """The files of a shell zip. An entry that is not a plain file (a symbolic link, a device) is refused: it would be written into the archive as a plain file holding its target's
    text, and make_app.sh writes none."""
    files, odd = {}, []
    with zipfile.ZipFile(path) as z:
        for info in z.infolist():
            if info.filename.endswith('/'):
                continue
            kind = (info.external_attr >> 16) & ZIP_TYPE_MASK
            if kind not in (0, FILE_TYPE_BITS):  # 0: a zip written on Windows has no Unix mode word at all
                odd.append('%s (type bits %o)' % (info.filename, kind))
                continue
            files[info.filename] = (z.read(info), (info.external_attr >> 16) & 0o777)
    if odd:
        raise ReleaseError('%s holds %d entr%s that %s not a plain file, which a shell made by tools/apple/make_app.sh never does: %s' %
                           (path, len(odd), 'y' if len(odd) == 1 else 'ies', 'is' if len(odd) == 1 else 'are', ', '.join(odd)))
    return files


def refuse_links_in_bundle(bundle):
    """A bundle folder is read as plain files only: a link in it (to a file, which open() would follow, or to a folder, which os.walk would not enter) is refused."""
    links = []
    for d, ds, fs in os.walk(bundle):
        links += [os.path.relpath(os.path.join(d, n), bundle).replace(os.sep, '/') for n in ds + fs if os.path.islink(os.path.join(d, n))]
    if links:
        raise ReleaseError('%s holds %d symbolic link(s) (%s): a bundle is read as plain files only' % (bundle, len(links), ', '.join(sorted(links))))


def shell_files_from_folder(folder):
    """A folder make_app.sh wrote: either it holds MagicPortals.app (the bundle) or one shell zip/ipa. Returns (files, was_zip_path or None)."""
    bundle = os.path.join(folder, 'MagicPortals.app')
    if os.path.isdir(bundle):
        refuse_links_in_bundle(bundle)
        files = {}
        for rel, _ in pk.listing(bundle):
            with open(os.path.join(bundle, *rel.split('/')), 'rb') as f:
                files['MagicPortals.app/' + rel] = (f.read(), 0o644)
        return files, None
    archives = sorted(n for n in os.listdir(folder) if n.endswith(('.zip', '.ipa')))
    if len(archives) != 1:
        raise ReleaseError('%s holds neither MagicPortals.app nor exactly one shell zip or ipa (it holds %d)' % (folder, len(archives)))
    return shell_files_from_zip(os.path.join(folder, archives[0])), os.path.join(folder, archives[0])


def classify_shell(files, platform):
    plists = sorted(n for n in files if re.fullmatch(r'(?:.*/)?[^/]+\.app/(?:Contents/)?Info\.plist', n))
    if len(plists) != 1:
        raise ReleaseError('the shell holds %d application bundles (Info.plist files); one is expected' % len(plists))
    plist = plists[0]
    mac = plist.endswith('/Contents/Info.plist')
    bundle = plist[:-len('/Contents/Info.plist')] if mac else plist[:-len('/Info.plist')]
    try:
        info = plistlib.loads(files[plist][0])
        exe_name = info['CFBundleExecutable']
    except (plistlib.InvalidFileException, ValueError, KeyError, ExpatError) as e:
        raise ReleaseError('%s cannot be read as a property list with a CFBundleExecutable: %s' % (plist, e))
    if mac:
        exe, kind = '%s/Contents/MacOS/%s' % (bundle, exe_name), MACOS
    else:
        exe, kind = '%s/%s' % (bundle, exe_name), IOS
    data_dir = bundle + '/' + GAME_DATA_IN_BUNDLE[kind]
    if kind != platform:
        raise ReleaseError('this shell is %s bundle and --platform says %s' % ('a macOS' if mac else 'an iOS', platform))
    if exe not in files:
        raise ReleaseError('the shell has no program at %s' % exe)
    simulator = 'iPhoneSimulator' in (info.get('CFBundleSupportedPlatforms') or [])
    top = bundle.rsplit('/', 1)[0] + '/' if '/' in bundle and not bundle.startswith('Payload/') else ''
    return Shell(files, bundle, exe, data_dir, top, platform, bundle.startswith('Payload/'), simulator)


def intro_text(platform, version, arch, simulator):
    if platform == MACOS:
        return ('Magic Portals Enhanced %s - macOS (%s), WITH the game inside the app\n\n'
                'The app is MagicPortals.app; the game\'s levels, pictures and sounds are in its Contents/Resources/GameData. Nothing here has ever been run on a Mac.\n'
                'The packager that wrote this archive cannot sign or notarise (it runs on a PC). LICENSE and NOTICE.md are the licence and the notices.\n\n' % (version, arch))
    return ('Magic Portals Enhanced %s - iOS (%s%s), WITH the game inside the app\n\n'
            'The app is MagicPortals.app; the game\'s levels, pictures and sounds are in its GameData folder. Nothing iOS has ever been run on a device or a simulator.\n'
            'LICENSE and NOTICE.md are the licence and the notices.\n\n' % (version, arch, ', simulator' if simulator else ''))


def assemble_apple(args):
    platform = args.platform
    repo = os.path.abspath(args.repo) if args.repo else vs.default_repo()
    version = vs.read_version(repo)
    if args.program or args.vc_redist or args.no_data or args.max_glibc:
        raise ReleaseError('--program, --vc-redist, --no-data and --max-glibc are for windows and linux; a %s archive is made of --shell (and --content)' % platform)
    if not args.shell:
        raise ReleaseError('give --shell <the zip or ipa make_app.sh made, or the folder it wrote>')
    shell_path = os.path.abspath(args.shell)
    shell_zip = None
    if os.path.isdir(shell_path):
        files, shell_zip = shell_files_from_folder(shell_path)
    elif os.path.isfile(shell_path):
        files, shell_zip = shell_files_from_zip(shell_path), shell_path
    else:
        raise ReleaseError('--shell %s is not a file or a folder' % shell_path)
    shell = classify_shell(files, platform)
    if shell_zip is None:
        # a bundle folder, laid out the way make_app.sh's own zips are: a MagicPortals/ folder on a Mac, Payload/ in an ipa, the bundle alone in a simulator zip
        prefix = 'MagicPortals/' if platform == MACOS else ('' if shell.simulator else 'Payload/')
        if prefix:
            files = {prefix + n: v for n, v in files.items()}
            shell = classify_shell(files, platform)
    report = check_program_bytes(files[shell.exe][0], platform, version=version, repo=repo, also_forbid=args.also_forbid or ())
    if report.blocking(args.allow_dev):
        raise ReleaseError('check-program refuses the program in the shell (%s; %d fact(s) fail):%s' % (shell.exe, len(report.blocking(args.allow_dev)),
                                                                                                         refusal_text(report, args.allow_dev)))
    dev = bool(report.developer())
    content = os.path.abspath(args.content) if args.content else None
    content_report = None
    notices = {}
    if content:
        notices = find_notices(repo, args.notices)
        content_report = pk.verify_content(content)
        if content_report['problems']:
            problems = content_report['problems']
            raise ReleaseError('the content package fails package.py verify (%d problem(s)):\n  %s' % (len(problems), '\n  '.join(problems[:10])))
    out = check_output_folder(args.out, repo, content)
    ext = '.ipa' if shell.ipa else '.zip'
    kinds = [k for k, on in (('simulator', shell.simulator), ('shell', not content)) if on]
    name = archive_name(version, platform, report.arch if platform == MACOS else 'arm64', '-'.join(kinds) or None, dev, ext)
    target = refuse_if_present(out, name)

    if not content and shell_zip:
        # The shell as it is, under the archive's name: byte for byte, so it is exactly what make_app.sh made and signed.
        os.makedirs(out, exist_ok=True)
        part = target + '.part'
        try:
            shutil.copyfile(shell_zip, part)
            os.replace(part, target)
        finally:
            if os.path.exists(part):
                os.remove(part)
        print('archive: %s' % target)
        print('  the shell passed through, byte for byte (%d bytes, sha256 %s), renamed; it has NO game data and no notices (it is a shell, not a release)' %
              (os.path.getsize(target), pk.sha256_of(target)))
        print('  program: %s %s%s' % (platform, report.arch, ' -- a DEVELOPER BUILD (--allow-dev)' if dev else ", a player's build"))
        return 0

    entries = []
    for fname, (data, _) in files.items():
        if platform == MACOS and fname == shell.top + READ_ME_FILE and content:
            continue  # the shell's READ ME says "WITHOUT the game"; with the data in, it is replaced below
        entries.append((fname, data, PROGRAM_MODE if fname == shell.exe else FILE_MODE))
    prefix_data = shell.data_dir + '/'
    if content:
        for rel, _ in pk.listing(content):
            entries.append((prefix_data + rel, os.path.join(content, *rel.split('/')), FILE_MODE))
        for n, d in sorted(notices.items()):
            entries.append((shell.top + n, d, FILE_MODE))
        text = (intro_text(platform, version, report.arch if platform == MACOS else 'arm64', shell.simulator)
                + seal_text(platform, version, GAME_DATA_IN_BUNDLE[platform], bool(notices)))
        entries.append((shell.top + READ_ME_FILE, text.encode('utf-8'), FILE_MODE))
    if dev:
        entries.append((shell.top + DEV_MARKER_FILE, developer_note(report).encode('utf-8'), FILE_MODE))
    os.makedirs(out, exist_ok=True)
    part = target + '.part'
    try:
        write_zip(part, entries, first='Payload/' if shell.ipa else None, folders=True)
        problems = verify_archive(part, entries, prefix_data if content else None)
        if problems:
            raise ReleaseError('the archive fails its own re-read (%d problem(s)):\n  %s' % (len(problems), '\n  '.join(problems[:10])))
        os.replace(part, target)
    finally:
        if os.path.exists(part):
            os.remove(part)
    sums = None if (dev or not content) else write_sums(out, skip_dev=True)
    print('archive: %s' % target)
    print('  %d bytes, sha256 %s' % (os.path.getsize(target), pk.sha256_of(target)))
    print('  program: %s %s%s' % (platform, report.arch, ' -- a DEVELOPER BUILD (--allow-dev), the name carries -dev' if dev else ", a player's build"))
    if not content:
        print('  a shell: it has NO game data and no notices, and no %s is written for it (a shell is not a release)' % SUMS_FILE)
        return 0
    print('  content: %d files, %d bytes, in %s (package.py verify ok, re-run on the copy inside the archive)' % (content_report['summary']['files'], content_report['summary']['bytes'],
                                                                                                                   shell.data_dir))
    print('  %s' % ('no %s written for a developer\'s archive' % SUMS_FILE if dev else '%s: %s' % (SUMS_FILE, ', '.join(sums))))
    print('')
    for line in seal_text(platform, version, GAME_DATA_IN_BUNDLE[platform], bool(notices)).rstrip().split('\n'):
        print('  ' + line)
    print('  (This text is also in the archive, as %s.)' % READ_ME_FILE)
    return 0


# ---- android ------------------------------------------------------------------------------------------------------------------------------

REPORT_LINE = 'the crash report will call the game:'
DENIED_APK_NAMES = (('-debug', 'a debug APK (debuggable, signed with the debug key)'), ('-VERIFICATION', 'the verification variant (it carries the developer\'s tools)'),
                    (ap.APK_PERF_DIAG_TAG, 'the performance-diagnostic variant (a tester\'s build)'), ('-nodata', 'an APK with no game in it'))


def apk_package_line(tools, apk):
    """(versionName, versionCode) from `aapt2 dump badging`."""
    badging = ap.run([tools.aapt2, 'dump', 'badging', apk], 'aapt2 dump badging').replace('\r', '')
    m = re.search(r"^package: name='([^']*)' versionCode='(\d+)' versionName='([^']*)'", badging, re.M)
    if not m:
        raise ReleaseError('aapt2 dump badging prints no package line for %s' % apk)
    return m.group(3), int(m.group(2))


def android_release(args, make_tools=None, inspect=None, package_line=None):
    """The release path of `android`, with the SDK-backed pieces injectable so that the tests can feed it a release-looking result. Returns the printed lines."""
    repo = os.path.abspath(args.repo) if args.repo else vs.default_repo()
    version = vs.read_version(repo)
    code = args.version_code if args.version_code is not None else vs.version_code(version)
    apk = os.path.abspath(args.apk)
    read_file(apk)
    notices = find_notices(repo, args.notices)
    out = check_output_folder(args.out, repo)
    name = archive_name(version, ANDROID, None, None, False, '.apk')
    target = refuse_if_present(out, name)
    base = os.path.basename(apk)
    problems = []
    for tag, why in DENIED_APK_NAMES:
        if tag in base:
            problems.append('the file is named %s: %s is never a release' % (base, why))
    sdk = os.path.abspath(args.sdk)
    make_tools = make_tools or ap.Tools
    tools = make_tools(sdk, args.build_tools or ap.newest_build_tools(sdk, ap.read_manifest(os.path.join(repo, 'game', 'android', 'AndroidManifest.xml'))['targetSdk']))
    inspect = inspect or ap.inspect_apk
    try:
        lines, found = inspect(apk, tools, repo, True, expect_data=True)
    except ap.PackError as e:
        raise ReleaseError('the APK could not be inspected (an unsigned APK, or one that apksigner cannot verify, ends here): %s' % e)
    problems += found
    report_lines = [l for l in lines if l.startswith(REPORT_LINE)]
    if not report_lines:
        problems.append('inspect printed no "%s" line: the report fact is missing' % REPORT_LINE)
    elif '(no name)' in report_lines[0]:
        problems.append('the crash report would call the game by no name: %s' % report_lines[0])
    if not any(l.startswith(ap.REPORT_TITLE_KEY) for l in lines):
        problems.append('inspect printed no %s line: the built manifest was not read back' % ap.REPORT_TITLE_KEY)
    try:
        got_version, got_code = (package_line or apk_package_line)(tools, apk)
    except ap.PackError as e:
        raise ReleaseError(str(e))
    if got_version != version:
        problems.append('versionName is %r and the one version is %s' % (got_version, version))
    if got_code != code:
        problems.append('versionCode is %d and the rule of version.py gives %d for %s (--version-code if the owner chose another)' % (got_code, code, version))
    with zipfile.ZipFile(apk) as z:
        libs = {n: z.read(n) for n in z.namelist() if n.startswith('lib/') and n.endswith('/' + ap.LIB_NAME)}
    if not libs:
        problems.append('the APK has no lib/<abi>/%s' % ap.LIB_NAME)
    for n, data in sorted(libs.items()):
        lib = check_program_bytes(data, ANDROID_LIB, version=version, repo=repo, also_forbid=args.also_forbid or ())
        problems += ['%s: %s' % (n, t) for _, t in lib.problems]
    if problems:
        raise ReleaseError('%s is not a signed release of version %s (%d fact(s) fail):\n  - %s' % (apk, version, len(problems), '\n  - '.join(problems)))
    # The notices already in the folder are compared BEFORE anything is placed: a refusal leaves the folder as it was (as assemble does), not holding an APK without its sums.
    differ = [n for n in sorted(notices) if os.path.lexists(os.path.join(out, n)) and read_file(os.path.join(out, n)) != notices[n]]  # read_file refuses a link or a folder
    if differ:
        raise ReleaseError('%s exists and differs from the notice given: a release folder holds one set of notices' % ', '.join(os.path.join(out, n) for n in differ))
    os.makedirs(out, exist_ok=True)
    part = target + '.part'
    try:
        shutil.copyfile(apk, part)
        os.replace(part, target)
    finally:
        if os.path.exists(part):
            os.remove(part)
    placed = []
    for n, data in sorted(notices.items()):
        beside = os.path.join(out, n)
        if not os.path.exists(beside):
            with open(beside, 'wb') as f:
                f.write(data)
        placed.append(n)
    sums = write_sums(out, skip_dev=True)
    return ['apk: %s' % target, '  %d bytes, sha256 %s' % (os.path.getsize(target), pk.sha256_of(target)),
            '  a signed release of version %s, versionCode %d; %d librar%s checked as a player\'s build' % (version, got_code, len(libs), 'y' if len(libs) == 1 else 'ies'),
            '  notices beside it (not inside: the APK is signed, and adding a file would be a rebuild): %s' % ', '.join(placed),
            '  %s: %s' % (SUMS_FILE, ', '.join(sums)), '  no key was read or made']


def cmd_android(args):
    for line in android_release(args):
        print(line)
    return 0


def cmd_assemble(args):
    if args.platform in DESKTOP_PLATFORMS:
        return assemble_desktop(args)
    return assemble_apple(args)


# ---- the command line ------------------------------------------------------------------------------------------------------------------------

def build_parser():
    ap_ = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap_.add_subparsers(dest='cmd', required=True)
    c = sub.add_parser('check-program', help='read a built program WITHOUT starting it and refuse a developer\'s build',
                       description='Refuses (exit 1, every failed fact listed): a file that is not the platform\'s program (format, machine, DLL), a program that holds a '
                                   'forbidden string (a baked path, the checkout\'s path, the builder\'s account), that was made with the developer tools ON, that is a '
                                   'performance-diagnostic build, that is not this game at all, or (Windows) whose version information is not the one version. '
                                   '--allow-dev turns the refusals about the kind of build into warnings; a wrong format or machine is never allowed.')
    c.add_argument('file')
    c.add_argument('--platform', required=True, choices=CHECKED_PLATFORMS)
    c.add_argument('--allow-dev', action='store_true', help='print the developer-build refusals as warnings (a smoke check; any archive made of it is named -dev)')
    c.add_argument('--also-forbid', action='append', metavar='TEXT', help='one more string that must not occur (repeatable): a stranger\'s account name, a CI path')
    c.add_argument('--max-glibc', metavar='X.Y', help='linux: refuse a program that asks the system for a glibc newer than this (the oldest system it should run on); the check is on the '
                                                      'GLIBC_x.y names the program holds, and --allow-dev does not waive it')
    c.add_argument('--repo', help='this repository (for the version and the checkout\'s own path); default: the one this file is in')
    c.set_defaults(func=cmd_check_program)

    a = sub.add_parser('assemble', help='write the archive of a platform: a built program (or a shell) and the content package',
                       description='Writes ONE archive with the whole game: windows .zip, linux .tar.gz, macos .zip, ios .ipa. Refuses, naming every cause and leaving nothing behind: '
                                   'a program that check-program refuses (--allow-dev: only the kind-of-build facts), content that fails package.py verify or holds a file outside '
                                   'its manifest, a missing LICENSE or NOTICE.md, an output folder inside the tracked tree or into a folder it reads, an archive that already exists, '
                                   '--vc-redist DLLs that are missing or of the wrong machine type, a macos or ios shell that holds an entry that is not a plain file (a symbolic link: '
                                   'not even a link to a file inside a bundle folder), a linux program that asks for a newer glibc than --max-glibc. The same inputs give the same '
                                   'archive bytes. Writes SHA256SUMS.txt beside it, except for a developer\'s archive and an archive with no game in it (-dev, -nodata, -shell: none is a release).')
    a.add_argument('--platform', required=True, choices=DESKTOP_PLATFORMS + APPLE_PLATFORMS)
    a.add_argument('--program', help='windows, linux: the built program (MagicPortals.exe, MagicPortals)')
    a.add_argument('--shell', help='macos, ios: the zip or ipa tools/apple/make_app.sh made, or the folder it wrote')
    a.add_argument('--content', help='the content package folder (package.py content). macos, ios: without it the shell passes through unchanged')
    a.add_argument('--no-data', action='store_true', help='windows, linux: an archive with no game data (its name carries -nodata)')
    a.add_argument('--out', required=True, help='where the archive and SHA256SUMS.txt go (outside the tracked tree, or under the gitignored out/)')
    a.add_argument('--vc-redist', help='windows: Microsoft\'s redistributable folder; the runtime DLLs the program imports are copied beside it (each checked as a PE DLL of its machine)')
    a.add_argument('--notices', help='the folder holding LICENSE and NOTICE.md (default: the repository root)')
    a.add_argument('--shaders', help='the engine\'s compiled shaders (default <repo>/engine/assets/shaders)')
    a.add_argument('--allow-dev', action='store_true', help='accept a developer\'s build for a smoke check: DEVELOPER BUILD is written into the archive, whose name carries -dev')
    a.add_argument('--also-forbid', action='append', metavar='TEXT')
    a.add_argument('--max-glibc', metavar='X.Y', help='linux: refuse a program that asks the system for a glibc newer than this (as check-program --max-glibc)')
    a.add_argument('--repo')
    a.set_defaults(func=cmd_assemble)

    d = sub.add_parser('android', help='check an APK as a signed release of the one version and place it in a release folder',
                       description='Runs android_package.py inspect on the APK as a RELEASE and refuses (every cause listed): a debug or debuggable APK, one signed with the debug key '
                                   'or unsigned, a verification or performance-diagnostic variant (read out of its libraries), a developer\'s build, a versionName that is not the '
                                   'one version, a versionCode other than the rule of version.py, a missing crash-report fact, no game data, and (before anything is placed) a '
                                   'LICENSE or NOTICE.md already beside the place for the APK that differs from the one given, is a folder, or is a link, dangling or not. It never '
                                   'signs and never reads a key. Copies the APK to MagicPortals-<version>-android.apk, puts LICENSE and NOTICE.md beside it, and writes SHA256SUMS.txt.')
    d.add_argument('--apk', required=True)
    d.add_argument('--sdk', required=True, help='the Android SDK (aapt2, apksigner, zipalign)')
    d.add_argument('--build-tools')
    d.add_argument('--out', required=True)
    d.add_argument('--notices')
    d.add_argument('--version-code', type=int, help='the versionCode the APK was built with, if the owner chose one other than the rule\'s')
    d.add_argument('--also-forbid', action='append', metavar='TEXT')
    d.add_argument('--repo')
    d.set_defaults(func=cmd_android)

    s = sub.add_parser('sums', help='write SHA256SUMS.txt for a release folder, or check one',
                       description='Writes SHA256SUMS.txt (sha256sum -c form) over the archives and notice files of DIR. Refuses a folder with no archive and a developer\'s '
                                   'archive (-dev in its name) unless --allow-dev. --check verifies an existing file: changed, missing and unlisted files.')
    s.add_argument('dir')
    s.add_argument('--check', action='store_true')
    s.add_argument('--allow-dev', action='store_true')
    s.set_defaults(func=cmd_sums)

    v = sub.add_parser('check-version', help='compare the one version with every place it is written (check_version.py)')
    v.add_argument('--repo')
    v.set_defaults(func=cmd_check_version)
    return ap_


def main(argv=None):
    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        return args.func(args)
    except (ReleaseError, pk.PackageError, ap.PackError, vs.VersionError) as e:
        sys.stdout.flush()
        print('release.py: %s' % hide_names(str(e), running_accounts()), file=sys.stderr)  # an error names the paths it was given, and a path may hold the account's folder
        return 2


if __name__ == '__main__':
    sys.exit(main())
