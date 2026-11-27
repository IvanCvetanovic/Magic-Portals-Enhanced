#!/usr/bin/env python3
"""The original game's launcher icon on the Windows release program (planning step 278).

The program release.yml builds carries the icon this project draws itself (tools/art/make_app_icon.py: two rings on a dark disc), because the original's icon
is a file of the original game and never part of this repository (CLAUDE.md, the hard rule), so no runner has it. The Android file has carried the original's
since planning step 212 (original_icon.py), and the owner asked for the same on Windows. This tool does that on the owner's PC, to the program the workflow
built: it changes the program's icon resources and nothing else.

    python tools/package/windows_icon.py ico   --reference <reference folder> --out <folder>/magic_portals_original.ico
    python tools/package/windows_icon.py apply --program <MagicPortals.exe> --ico <that .ico> --out <folder>/MagicPortals.exe

`ico` reads the original APK's own icons (the 48 and 96 pixel ones, <reference>/apk_work/decoded/res/drawable-{mdpi,xhdpi}-v4/icon.png). The 48 and 96 entries
are those pictures' pixels; the smaller ones and 64 are area averages of the 96; 128 and 256 are resampled up from it (the original has nothing larger).
Entries below 256 are device-independent bitmaps and 256 is a PNG, the form older Windows readers take. `apply` needs Windows (it uses the system's resource
editing calls): it copies the program, replaces every icon resource of the copy with two groups of these pictures - 1, the file's icon in Explorer, and
GLFW_ICON, the one the window library gives the game's window and taskbar button (without it Windows shows its generic program icon there) - and reads them
back. Both outputs go under the repository's out/ or outside the repository. Standard library only; the same inputs give the same icon file.
"""

from __future__ import annotations

import argparse
import ctypes
import os
import shutil
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import original_icon as oi  # noqa: E402
import package as pk  # noqa: E402  (the content packager's output-folder rule)

SIZES = (16, 20, 24, 32, 40, 48, 64, 96, 128, 256)
SOURCES = ((48, 'mdpi'), (96, 'xhdpi'))  # the original's own pictures this tool takes: size -> density folder
PNG_FROM = 256  # entries of this size and larger are stored as PNG; smaller ones as bitmaps

# The Windows resource types and names this tool touches.
RT_ICON = 3
RT_GROUP_ICON = 14
RT_VERSION = 16
RT_MANIFEST = 24
GROUPS = ('GLFW_ICON', 1)  # named entries sort first in a resource directory; both groups name the same pictures
LANG_EN_US = 0x0409
LOAD_AS_DATA = 0x00000002 | 0x00000020  # LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE


class WindowsIconError(Exception):
    pass


# ---- the icon file ----------------------------------------------------------------------------------------------------------------------------

def shrink(width, rows, size):
    """Rows of the square picture at size x size, smaller than it: each new pixel is the area it covers of the old ones, averaged on premultiplied alpha."""
    step = width / size

    def spans(d):
        low, high = d * step, (d + 1) * step
        first = int(low)
        found = []
        i = first
        while i < width and i < high:
            cover = min(high, i + 1) - max(low, i)
            if cover > 1e-9:
                found.append((i, cover))
            i += 1
        return found

    axis = [spans(d) for d in range(size)]
    result = []
    for y in range(size):
        line = bytearray(size * 4)
        for x in range(size):
            total = [0.0, 0.0, 0.0, 0.0]
            area = 0.0
            for sy, wy in axis[y]:
                row = rows[sy]
                for sx, wx in axis[x]:
                    w = wx * wy
                    r, g, b, a = row[sx * 4:sx * 4 + 4]
                    total[0] += r * a / 255.0 * w
                    total[1] += g * a / 255.0 * w
                    total[2] += b * a / 255.0 * w
                    total[3] += a * w
                    area += w
            a = total[3] / area
            if a > 0.0:
                colour = [min(max(v / area * 255.0 / a, 0.0), 255.0) for v in total[:3]]
            else:
                colour = [0.0, 0.0, 0.0]
            line[x * 4:x * 4 + 4] = bytes([int(round(v)) for v in colour] + [int(round(min(a, 255.0)))])
        result.append(line)
    return result


def dib(size, rows):
    """An icon's bitmap form: a BITMAPINFOHEADER whose height counts the picture and its mask, the rows bottom-up as BGRA, then the 1-bit mask (set where
    the pixel is fully transparent), its rows padded to 32 bits."""
    header = struct.pack('<IiiHHIIiiII', 40, size, size * 2, 1, 32, 0, 0, 0, 0, 0, 0)
    colour = bytearray()
    for row in reversed(rows):
        line = bytearray(row[:size * 4])
        line[0::4], line[2::4] = row[2:size * 4:4], row[0:size * 4:4]
        colour += line
    stride = (size + 31) // 32 * 4
    mask = bytearray()
    for row in reversed(rows):
        bits = bytearray(stride)
        for x in range(size):
            if row[x * 4 + 3] == 0:
                bits[x // 8] |= 0x80 >> (x % 8)
        mask += bits
    return bytes(header + colour + mask)


def pictures(sources):
    """[(size, the entry's bytes)] for SIZES, from {48: rows, 96: rows} of the original."""
    large = sources[96]
    entries = []
    for size in SIZES:
        if size in sources:
            rows = sources[size]
        elif size < 96:
            rows = shrink(96, large, size)
        else:
            rows = oi.resize(96, 96, large, size)
        entries.append((size, oi.write_png(size, size, rows) if size >= PNG_FROM else dib(size, rows)))
    return entries


def ico_bytes(entries):
    """The .ico file: its directory, then the pictures in the directory's order."""
    offset = 6 + 16 * len(entries)
    directory = bytearray(struct.pack('<HHH', 0, 1, len(entries)))
    body = bytearray()
    for size, data in entries:
        directory += struct.pack('<BBBBHHII', size & 255, size & 255, 0, 0, 1, 32, len(data), offset + len(body))
        body += data
    return bytes(directory + body)


def read_ico(data, where):
    """[(size, the entry's bytes)] of an .ico file. Anything that is not one, or an entry outside the file, is refused by name."""
    if len(data) < 6 or struct.unpack('<HH', data[:4]) != (0, 1):
        raise WindowsIconError('%s is not an icon file' % where)
    count = struct.unpack('<H', data[4:6])[0]
    if count == 0 or len(data) < 6 + 16 * count:
        raise WindowsIconError('%s has no icon directory' % where)
    entries = []
    for i in range(count):
        width, height, _colours, _reserved, _planes, _bits, length, offset = struct.unpack('<BBBBHHII', data[6 + 16 * i:22 + 16 * i])
        if width != height or length == 0 or offset + length > len(data):
            raise WindowsIconError('%s: entry %d is not a square picture inside the file' % (where, i))
        entries.append((width or 256, data[offset:offset + length]))
    return entries


def read_sources(reference):
    sources = {}
    for size, density in SOURCES:
        path = os.path.join(reference, 'apk_work', 'decoded', 'res', oi.SOURCE_FOLDER % density, oi.SOURCE_NAME)
        if not os.path.isfile(path):
            raise WindowsIconError('the original\'s %s icon is not at %s (the reference folder is the one docs/developing.md, Local setup after cloning, makes)' % (density, path))
        with open(path, 'rb') as f:
            width, height, rows = oi.read_png(f.read(), path)
        if (width, height) != (size, size):
            raise WindowsIconError('%s is %d x %d, the %s icon is %d x %d' % (path, width, height, density, size, size))
        sources[size] = rows
    return sources


def check_out(out, repo, reference):
    folder = os.path.dirname(os.path.abspath(out))
    try:
        pk.check_output(folder, os.path.abspath(repo), os.path.abspath(reference))
    except pk.PackageError as e:
        raise WindowsIconError(str(e))
    if os.path.exists(out):
        raise WindowsIconError('%s is already there: nothing is written over it' % out)
    os.makedirs(folder, exist_ok=True)


def make_ico(reference, out, repo):
    check_out(out, repo, reference)
    data = ico_bytes(pictures(read_sources(os.path.abspath(reference))))
    with open(out, 'wb') as f:
        f.write(data)
    return data


# ---- the program's resources (Windows) ------------------------------------------------------------------------------------------------------------

def kernel():
    if os.name != 'nt':
        raise WindowsIconError('changing a program\'s icon needs Windows (its resource editing calls)')
    from ctypes import wintypes
    k = ctypes.WinDLL('kernel32', use_last_error=True)
    p = ctypes.c_void_p
    for name, restype, argtypes in (
            ('LoadLibraryExW', p, [wintypes.LPCWSTR, p, wintypes.DWORD]), ('FreeLibrary', wintypes.BOOL, [p]),
            ('EnumResourceNamesW', wintypes.BOOL, [p, p, p, ctypes.c_ssize_t]),
            ('EnumResourceLanguagesW', wintypes.BOOL, [p, p, p, p, ctypes.c_ssize_t]),
            ('FindResourceExW', p, [p, p, p, wintypes.WORD]), ('LoadResource', p, [p, p]), ('LockResource', p, [p]),
            ('SizeofResource', wintypes.DWORD, [p, p]), ('BeginUpdateResourceW', p, [wintypes.LPCWSTR, wintypes.BOOL]),
            ('UpdateResourceW', wintypes.BOOL, [p, p, p, wintypes.WORD, p, wintypes.DWORD]), ('EndUpdateResourceW', wintypes.BOOL, [p, wintypes.BOOL])):
        function = getattr(k, name)
        function.restype, function.argtypes = restype, argtypes
    return k


def failed(what):
    return WindowsIconError('%s failed: %s' % (what, ctypes.WinError(ctypes.get_last_error())))


class Name:
    """A resource name or type as the calls take it: a small integer as itself, a string as a pointer to it (kept alive here)."""

    def __init__(self, value):
        self.value = value
        self.buffer = None if isinstance(value, int) else ctypes.create_unicode_buffer(value)
        self.pointer = ctypes.c_void_p(value) if self.buffer is None else ctypes.cast(self.buffer, ctypes.c_void_p)


def resources(program, types=(RT_ICON, RT_GROUP_ICON, RT_VERSION, RT_MANIFEST)):
    """{(type, name, language): bytes} of the program's resources of those types."""
    k = kernel()
    module = k.LoadLibraryExW(os.path.abspath(program), None, LOAD_AS_DATA)
    if not module:
        raise failed('reading %s' % program)
    found = {}
    try:
        def text(value):
            value = value or 0
            return value if value < 0x10000 else ctypes.wstring_at(value)

        names_type = ctypes.WINFUNCTYPE(ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_ssize_t)
        languages_type = ctypes.WINFUNCTYPE(ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_ushort, ctypes.c_ssize_t)
        for kind in types:
            names = []
            collect = names_type(lambda _m, _t, name, _p: names.append(text(name)) or 1)
            k.EnumResourceNamesW(module, ctypes.c_void_p(kind), ctypes.cast(collect, ctypes.c_void_p), 0)  # false when there is none of the type
            for name in names:
                held = Name(name)
                languages = []
                each = languages_type(lambda _m, _t, _n, language, _p: languages.append(language) or 1)
                if not k.EnumResourceLanguagesW(module, ctypes.c_void_p(kind), held.pointer, ctypes.cast(each, ctypes.c_void_p), 0):
                    raise failed('listing the languages of resource %r of type %d' % (name, kind))
                for language in languages:
                    entry = k.FindResourceExW(module, ctypes.c_void_p(kind), held.pointer, language)
                    if not entry:
                        raise failed('finding resource %r of type %d' % (name, kind))
                    data = k.LockResource(k.LoadResource(module, entry))
                    found[(kind, name, language)] = ctypes.string_at(data, k.SizeofResource(module, entry))
    finally:
        k.FreeLibrary(module)
    return found


def group_bytes(entries):
    """An icon group resource: the .ico directory with each picture's offset replaced by the number of its RT_ICON resource (1, 2, ...)."""
    data = bytearray(struct.pack('<HHH', 0, 1, len(entries)))
    for number, (size, picture) in enumerate(entries, 1):
        data += struct.pack('<BBBBHHIH', size & 255, size & 255, 0, 0, 1, 32, len(picture), number)
    return bytes(data)


def apply_icon(program, ico, out, repo, reference=None):
    """Writes `out`, a copy of `program` whose icon resources are the pictures of `ico`; every other resource stays as it was. Returns the resources read back."""
    kernel()  # refuse before anything is written
    with open(ico, 'rb') as f:
        entries = read_ico(f.read(), ico)
    before = resources(program)
    check_out(out, repo, reference or os.path.join(repo, 'reference'))
    languages = sorted({language for (kind, _n, language) in before if kind == RT_GROUP_ICON})
    language = languages[0] if languages else LANG_EN_US
    wanted = {(RT_ICON, number, language): picture for number, (_size, picture) in enumerate(entries, 1)}
    wanted.update({(RT_GROUP_ICON, group, language): group_bytes(entries) for group in GROUPS})
    shutil.copyfile(program, out)
    done = False
    try:
        k = kernel()
        update = k.BeginUpdateResourceW(os.path.abspath(out), False)
        if not update:
            raise failed('opening %s for its resources' % out)
        changes = [(key, None) for key in before if key[0] in (RT_ICON, RT_GROUP_ICON) and key not in wanted] + sorted(wanted.items(), key=repr)
        for (kind, name, lang), data in changes:
            held = Name(name)
            buffer = None if data is None else ctypes.create_string_buffer(data, len(data))
            if not k.UpdateResourceW(update, ctypes.c_void_p(kind), held.pointer, lang, ctypes.cast(buffer, ctypes.c_void_p) if buffer else None,
                                     0 if data is None else len(data)):
                k.EndUpdateResourceW(update, True)
                raise failed('changing resource %r of type %d' % (name, kind))
        if not k.EndUpdateResourceW(update, False):
            raise failed('writing the resources of %s' % out)
        after = resources(out)
        expected = {key: data for key, data in before.items() if key[0] not in (RT_ICON, RT_GROUP_ICON)}
        expected.update(wanted)
        if after != expected:
            raise WindowsIconError('%s does not read back as written: resources %s differ' % (out, sorted({repr(k_) for k_ in set(after) ^ set(expected)} |
                                   {repr(k_) for k_ in set(after) & set(expected) if after[k_] != expected[k_]})))
        done = True
        return after
    finally:
        if not done and os.path.exists(out):
            os.remove(out)


def main(argv=None):
    parser = argparse.ArgumentParser(description='The original game\'s icon on the Windows release program (the owner\'s data: never committed).')
    sub = parser.add_subparsers(dest='command', required=True)
    ico = sub.add_parser('ico', help='write the original\'s icon as a Windows icon file')
    ico.add_argument('--reference', required=True, help='the reference folder (it holds apk_work/decoded/res)')
    apply_ = sub.add_parser('apply', help='copy a program and give the copy that icon (Windows only)')
    apply_.add_argument('--program', required=True)
    apply_.add_argument('--ico', required=True)
    for each in (ico, apply_):
        each.add_argument('--out', required=True, help='the file to write (under the repository\'s out/, or outside the repository)')
        each.add_argument('--repo', default=os.path.dirname(os.path.dirname(HERE)), help='this repository (default: the one this file is in)')
    args = parser.parse_args(argv)
    try:
        if args.command == 'ico':
            data = make_ico(args.reference, args.out, args.repo)
            print('%s: %d bytes, %d pictures (%s)' % (args.out, len(data), len(SIZES), ', '.join(str(s) for s in SIZES)))
        else:
            after = apply_icon(args.program, args.ico, args.out, args.repo)
            print('%s: %d bytes, sha256 %s' % (args.out, os.path.getsize(args.out), pk.sha256_of(args.out)))
            print('  icon groups: %s; %d pictures; the other resources are the program\'s own' %
                  (', '.join(sorted(str(n) for (kind, n, _l) in after if kind == RT_GROUP_ICON)), sum(1 for key in after if key[0] == RT_ICON)))
    except (WindowsIconError, oi.IconError, OSError) as e:
        print('windows_icon: %s' % e, file=sys.stderr)
        return 2
    return 0


if __name__ == '__main__':
    sys.exit(main())
