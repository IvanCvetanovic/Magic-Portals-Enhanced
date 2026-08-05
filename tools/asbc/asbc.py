"""Reader for AngelScript saved bytecode (`android_game.bin`).

WHY THIS EXISTS
---------------
Magic Portals' gameplay logic is compiled AngelScript. The `.esc` level data told us
*what is placed where*; the script holds *how it behaves* — including the named constants
MILESTONE_1.md §3 asks about (`FIRST_PORTAL_MIN_TIME`, `NEXT_PORTAL_MIN_TIME`,
`g_portalCollisionRadius`). Decoding beats frame-stepping video when it works, because it
yields cited values instead of eyeballed ones.

WHAT THIS IS NOT
----------------
**Not a decompiler, and deliberately not a translator.** CLAUDE.md is explicit that
Ethanon gameplay logic "is *not* converted — it is read as design reference and
reimplemented". This tool's output is a findings document, never GDScript. It is also
kept out of the `.esc` → `.tscn` pipeline: that pipeline converts *data*, and this reads
*code*.

FORMAT
------
Ported from AngelScript's own deserialiser, `asCReader` in
`reference/ethanon/toolkit/Source/src/angelscript/source/as_restore.cpp` (MIT, vendored by
Ethanon at version 2.26.1). Section order is `asCReader::ReadInner` (`:107`):

    noDebugInfo (1 byte)
    enums          count + declarations      <- decoded here
    classTypes     count + declarations
    funcdefs, interface methods, class methods/behaviours, class properties, typedefs
    scriptGlobals  <- the named constants live here (ReadGlobalProperty, :1349)
    scriptFunctions, globalFunctions, bindInformations, usedTypes, usedTypeIds,
    usedFunctions, usedGlobalProperties, usedStringConstants, usedObjectProperties

VERSION COMPATIBILITY is not asserted from a version byte — the format has none. The test
is structural: a varint/length-prefixed format desynchronises catastrophically within a
few hundred bytes of any mismatch, so "parsed N sections without an implausible length or
count" is itself the evidence. `strict` mode makes every such implausibility fatal.
"""

from __future__ import annotations

from dataclasses import dataclass, field

# asOBJ_ENUM, as_objecttype.h / angelscript.h. Checked against the real file: the first
# declared type has flags 0x10 and four integer members, which is DIRECTION.
ASOBJ_ENUM = 0x10

# Sanity bounds. Nothing in a real module comes close, so exceeding one means the reader
# has desynchronised and is interpreting arbitrary bytes as a length.
MAX_SANE_COUNT = 100_000
MAX_SANE_STRING = 4096


class BytecodeError(Exception):
    """Raised when the stream stops making sense — i.e. the reader has desynced."""


@dataclass
class EnumType:
    name: str
    namespace: str
    values: list[tuple[str, int]] = field(default_factory=list)


class Reader:
    """Sequential reader mirroring `asCReader`'s primitives."""

    def __init__(self, data: bytes, *, strict: bool = True) -> None:
        self.data = data
        self.pos = 0
        self.strict = strict
        # asCReader keeps a running table of previously-written strings; a later
        # reference is an INDEX into it, not a repeat of the text. Skipping this table
        # desyncs the moment any string repeats.
        self.saved_strings: list[str] = []

    # --- primitives -------------------------------------------------------

    def read(self, count: int) -> bytes:
        if self.pos + count > len(self.data):
            raise BytecodeError(f"read past end at {self.pos} (+{count})")
        out = self.data[self.pos : self.pos + count]
        self.pos += count
        return out

    def u8(self) -> int:
        return self.read(1)[0]

    def encoded_uint(self) -> int:
        """Port of `asCReader::ReadEncodedUInt64` (`as_restore.cpp:1249`).

        Variable-length big-endian-ish scheme: the top bit is a sign flag and the run of
        following set bits selects how many extra bytes follow.
        """
        b = self.u8()
        negative = bool(b & 0x80)
        b &= 0x7F
        if b == 0x7F:
            n = 8
            value = 0
        elif (b & 0x7E) == 0x7E:
            n, value = 6, (b & 0x01) << 48
        elif (b & 0x7C) == 0x7C:
            n, value = 5, (b & 0x03) << 40
        elif (b & 0x78) == 0x78:
            n, value = 4, (b & 0x07) << 32
        elif (b & 0x70) == 0x70:
            n, value = 3, (b & 0x0F) << 24
        elif (b & 0x60) == 0x60:
            n, value = 2, (b & 0x1F) << 16
        elif (b & 0x40) == 0x40:
            n, value = 1, (b & 0x3F) << 8
        else:
            n, value = 0, b
        for _ in range(n):
            value = (value << 8) + self.u8()
        return -value if negative else value

    def string(self) -> str:
        """Port of `asCReader::ReadString` (`as_restore.cpp:1323`).

        A leading `\\0` is the empty string, `n` introduces a new string that joins the
        back-reference table, and anything else is an index into that table.
        """
        tag = self.u8()
        if tag == 0:
            return ""
        if tag == ord("n"):
            length = self.encoded_uint()
            if self.strict and length > MAX_SANE_STRING:
                raise BytecodeError(f"implausible string length {length} at {self.pos}")
            text = self.read(length).decode("utf-8", "replace")
            self.saved_strings.append(text)
            return text
        index = self.encoded_uint()
        if index >= len(self.saved_strings):
            raise BytecodeError(f"string back-reference {index} out of range at {self.pos}")
        return self.saved_strings[index]

    def count(self, what: str) -> int:
        value = self.encoded_uint()
        if self.strict and value > MAX_SANE_COUNT:
            raise BytecodeError(f"implausible {what} count {value} at {self.pos}")
        return value


@dataclass
class Module:
    no_debug_info: bool = False
    enums: list[EnumType] = field(default_factory=list)
    class_names: list[str] = field(default_factory=list)
    stopped_at: int = 0
    stopped_because: str = ""


def read_module(data: bytes, *, strict: bool = True) -> Module:
    """Decode as far as the format is confidently understood, then stop and say where.

    Stopping is a first-class result, not a failure: reporting "decoded the enum section
    cleanly, then stopped at byte N in classTypes" is honest and useful, whereas guessing
    onwards would produce plausible garbage. That is the same discipline the `.esc` work
    used — no silent guesses.
    """
    r = Reader(data, strict=strict)
    module = Module()

    module.no_debug_info = bool(r.u8())

    # --- enums (ReadInner :120) ---
    enum_count = r.count("enum")
    for _ in range(enum_count):
        name = r.string()
        flags = int.from_bytes(r.read(4), "little")
        r.encoded_uint()  # size
        namespace = r.string()
        if not flags & ASOBJ_ENUM:
            raise BytecodeError(f"type {name!r} in the enum section lacks asOBJ_ENUM ({flags:#x})")
        enum_type = EnumType(name=name, namespace=namespace)
        # Phase 2 for an enum: count, then (name, int32) pairs.
        for _ in range(r.count("enum value")):
            value_name = r.string()
            # `ReadData(&e->value, 4)` is a raw memcpy of a host int. The engine writes on
            # the same architecture it reads, and the file is big-endian here — verified
            # against DIRECTION, whose members come out 0,1,2,3 big-endian and as absurd
            # values little-endian.
            value = int.from_bytes(r.read(4), "big", signed=True)
            enum_type.values.append((value_name, value))
        module.enums.append(enum_type)

    module.stopped_at = r.pos
    module.stopped_because = (
        "classTypes onwards needs ReadObjectTypeDeclaration phase 2, "
        "ReadDataType and ReadFunctionSignature — not yet ported"
    )
    return module
