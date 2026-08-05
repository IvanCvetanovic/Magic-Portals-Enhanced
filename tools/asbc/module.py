"""Full AngelScript module decoder — enough of it to reach the named global constants.

A faithful port of ``asCReader`` (``as_restore.cpp``) as far as ``scriptGlobals``, plus
bytecode decoding of each global's initialisation function. That is the whole point: the
values MILESTONE_1 §3 needs (``FIRST_PORTAL_MIN_TIME``, ``NEXT_PORTAL_MIN_TIME``,
``g_portalCollisionRadius``) are not stored as data anywhere. ``ReadGlobalProperty``
(``:1349``) stores a name, a namespace, a type and an **init function** — so the value is
an immediate operand inside that function's bytecode, and getting it means decoding the
instruction stream.

BYTE ORDER — the one genuine surprise. Every raw 4-byte ``ReadData`` field in this file is
**big-endian**, not host order. Two independent confirmations:

* the first declared type's flags read ``10 00 00 00``, which is ``asOBJ_ENUM``
  (``0x10000000``, ``as_objecttype.h:59``) only when read big-endian; and
* ``DIRECTION``'s members then come out ``0,1,2,3`` rather than ``0, 16777216, …``.

Both fields would be nonsense under little-endian, so this is not a coin-flip.

VALIDATION. There is no version field, and AngelScript bytecode is not backward
compatible, so compatibility is established structurally: a length-prefixed format
desynchronises catastrophically within a few hundred bytes of any mismatch. "Decoded N
sections without an implausible count, string length or opcode" IS the evidence. Every
such implausibility raises rather than being clamped.

CURRENT STATE — 2026-08-05. **Incomplete, and precisely so.**

Decodes cleanly to byte **13,028 of 523,826**, which covers:

* the header and all 3 enums;
* all **132** class-type declarations (`SPortalStateFactory`, `SGlobalScale`,
  `STimeManager`, … — real names, so this is genuinely in sync, not accidentally);
* all 5 funcdefs;
* all 9 interfaces; and
* **6 complete classes** with every method's signature AND decoded bytecode.

It then desynchronises inside class `Button`'s method bytecode. The remaining bug is in
either an instruction-operand layout or a per-function trailer; it is NOT the pointer
width, because 32- and 64-bit runs fail at the identical byte, which they could not do if
a pointer-sized instruction had already been misread.

Four real bugs were found and fixed getting this far, each of which had produced a
confident-looking wrong parse:

1. ``ttIdentifier`` is **5**, not 14 — an unnumbered C enum, so its value is its ordinal
   (``as_tokendef.h:46-58``). Guessing it misparses every object-typed parameter.
2. ``asFUNC_SCRIPT`` is **1**, not 2 (``angelscript.h:318-327``).
3. **Every** function reads a trailing script-section name when debug info is present —
   not only script functions.
4. Class phase 2 reads ``derivedFrom``, interfaces, then ONE unconditional destructor,
   then constructor/factory **pairs**, then methods, then the virtual function table — and
   the destructor and constructors are **skipped entirely for interfaces**
   (``if(!IsInterface() && flags != TYPEDEF && flags != ENUM)``).

Until it reaches ``scriptGlobals``, the named constants are still unextracted and every
timing value in ``game/data/*.json`` remains a marked guess.
"""

from __future__ import annotations

import struct
from dataclasses import dataclass, field

from asbc import BytecodeError, Reader
from opcodes import MAX_BYTECODE, TABLE, TYPE_SIZE, resolve

# as_objecttype.h:58-60
ASOBJ_ENUM = 0x10000000
ASOBJ_TYPEDEF = 0x40000000
# angelscript.h:152
ASOBJ_SCRIPT_OBJECT = 0x80000

# asEFuncType — angelscript.h:318-327.
ASFUNC_SCRIPT = 1
ASFUNC_VIRTUAL = 3

# eTokenType (as_tokendef.h:46-58) — an unnumbered C enum, so the value is its ordinal:
# ttUnrecognizedToken=0, ttEnd=1, ttWhiteSpace=2, ttOnelineComment=3, ttMultilineComment=4,
# ttIdentifier=5. Marks a data type that names an object rather than a primitive. Guessing
# this wrong misparses every object-typed parameter and desyncs on the third funcdef.
TT_IDENTIFIER = 5


@dataclass
class ObjectType:
    name: str = ""
    flags: int = 0
    size: int = 0
    namespace: str = ""

    @property
    def is_enum(self) -> bool:
        return bool(self.flags & ASOBJ_ENUM)

    @property
    def is_typedef(self) -> bool:
        return bool(self.flags & ASOBJ_TYPEDEF)

    @property
    def is_interface(self) -> bool:
        # asCObjectType::IsInterface — as_objecttype.cpp:406.
        return bool(self.flags & ASOBJ_SCRIPT_OBJECT) and self.size == 0


@dataclass
class Function:
    name: str = ""
    func_type: int = 0
    # (mnemonic, [operands]) per instruction, in order.
    code: list[tuple[str, list[int]]] = field(default_factory=list)


@dataclass
class GlobalProperty:
    name: str = ""
    namespace: str = ""
    init: Function | None = None


@dataclass
class Module:
    no_debug_info: bool = False
    enums: list[tuple[ObjectType, list[tuple[str, int]]]] = field(default_factory=list)
    class_types: list[ObjectType] = field(default_factory=list)
    globals: list[GlobalProperty] = field(default_factory=list)
    consumed: int = 0
    total: int = 0


class ModuleReader:
    """Sequential decoder. Mirrors ``asCReader``'s state, including its back-ref tables."""

    def __init__(self, data: bytes, *, pointer_bits: int = 32) -> None:
        self.r = Reader(data, strict=True)
        self.pointer_bits = pointer_bits
        self.no_debug_info = False
        # asCReader keeps these; back-references index into them, so skipping any one
        # desyncs the moment a repeat appears.
        self.saved_data_types: list[object] = []
        self.saved_functions: list[Function] = []

    # --- primitives -------------------------------------------------------

    def _u32_be(self) -> int:
        return struct.unpack(">I", self.r.read(4))[0]

    def _i32_be(self) -> int:
        return struct.unpack(">i", self.r.read(4))[0]

    def _bool(self) -> bool:
        return self.r.u8() != 0

    def _encoded16(self) -> int:
        # ReadEncodedUInt16 (:1225) — a full encoded uint whose top half must be 0 or
        # 0xFFFF. Anything else is an error upstream, so it is one here too.
        value = self.r.encoded_uint()
        high = (value >> 16) & 0xFFFF
        if high not in (0, 0xFFFF):
            raise BytecodeError(f"encoded uint16 out of range ({value})", 0)
        return value & 0xFFFF

    # --- structures -------------------------------------------------------

    def read_object_type(self) -> str:
        """``ReadObjectType`` (:1485). Returns the type name, or "" for none."""
        tag = self.r.u8()
        if tag == ord("a"):
            name = self.r.string()
            for _ in range(self.r.count("template subtype")):
                if self.r.u8() == ord("s"):
                    self.read_data_type()
                else:
                    self.r.encoded_uint()
            return name
        if tag == ord("s"):
            # A template SUBTYPE, named by a plain string — not a nested data type.
            return self.r.string()
        if tag == ord("o"):
            name = self.r.string()
            self.r.string()  # namespace
            return name
        # '\0' — no type.
        return ""

    def read_data_type(self) -> None:
        """``ReadDataType`` (:1398). Result is discarded; only the stream position matters."""
        token = self.r.encoded_uint()
        if token == 0:
            # A back-reference into savedDataTypes.
            index = self.r.encoded_uint()
            if index >= len(self.saved_data_types):
                raise BytecodeError(f"data type back-ref {index} out of range", 0)
            return
        slot = len(self.saved_data_types)
        self.saved_data_types.append(None)

        object_name = ""
        if token == TT_IDENTIFIER:
            object_name = self.read_object_type()
            self.r.u8()  # isObjectHandle
            self.r.u8()  # isHandleToConst
        self.r.u8()  # isReference
        self.r.u8()  # isReadOnly

        if token == TT_IDENTIFIER and object_name == "_builtin_function_":
            # A funcdef data type carries an entire nested signature.
            self.read_function_signature()
        self.saved_data_types[slot] = object_name or token

    def read_function_signature(self) -> Function:
        """``ReadFunctionSignature`` (:576)."""
        func = Function()
        func.name = self.r.string()
        self.read_data_type()  # return type
        for _ in range(self.r.count("parameter")):
            self.read_data_type()
        for _ in range(self.r.count("inout flag")):
            self.r.encoded_uint()
        func.func_type = self.r.encoded_uint()
        default_args = self.r.count("default arg")
        for _ in range(default_args):
            self.r.string()
        object_type = self.read_object_type()
        if object_type:
            self.r.u8()  # isReadOnly / isPrivate bitfield
        else:
            self.r.string()  # namespace
        return func

    def read_bytecode(self, func: Function) -> None:
        """``ReadByteCode`` (:1621). Decodes instructions, keeping their operands."""
        count = self.r.count("instruction")
        for _ in range(count):
            opcode = self.r.u8()
            if opcode >= MAX_BYTECODE:
                raise BytecodeError(f"opcode {opcode} past asBC_MAXBYTECODE", 0)
            mnemonic, raw_type = TABLE[opcode]
            arg_type = resolve(raw_type, self.pointer_bits)
            if arg_type not in TYPE_SIZE:
                raise BytecodeError(f"unknown instruction type {arg_type}", 0)
            func.code.append((mnemonic, self._read_operands(arg_type)))

    def _read_operands(self, arg_type: str) -> list[int]:
        """Read one instruction's operands, in the exact order ReadByteCode does."""
        if arg_type == "NO_ARG":
            return []
        if arg_type in ("W_ARG", "wW_ARG", "rW_ARG"):
            return [self._encoded16()]
        if arg_type in ("rW_DW_ARG", "wW_DW_ARG", "W_DW_ARG"):
            return [self._encoded16(), self.r.encoded_uint()]
        if arg_type == "DW_ARG":
            return [self.r.encoded_uint()]
        if arg_type == "DW_DW_ARG":
            return [self.r.encoded_uint(), self.r.encoded_uint()]
        if arg_type == "wW_rW_rW_ARG":
            return [self._encoded16(), self._encoded16(), self._encoded16()]
        if arg_type in ("wW_rW_ARG", "rW_rW_ARG", "wW_W_ARG"):
            return [self._encoded16(), self._encoded16()]
        if arg_type in ("wW_rW_DW_ARG", "rW_W_DW_ARG"):
            return [self._encoded16(), self._encoded16(), self.r.encoded_uint()]
        if arg_type == "QW_ARG":
            return [self.r.encoded_uint64()]
        if arg_type in ("wW_QW_ARG", "rW_QW_ARG"):
            return [self._encoded16(), self.r.encoded_uint64()]
        if arg_type == "QW_DW_ARG":
            return [self.r.encoded_uint64(), self.r.encoded_uint()]
        if arg_type == "INFO":
            return []
        raise BytecodeError(f"unhandled instruction type {arg_type}", 0)

    def read_function(self) -> Function | None:
        """``ReadFunction`` (:640)."""
        tag = self.r.u8()
        if tag == 0:
            return None
        if tag == ord("r"):
            index = self.r.encoded_uint()
            if index >= len(self.saved_functions):
                raise BytecodeError(f"function back-ref {index} out of range", 0)
            return self.saved_functions[index]

        func = Function()
        self.saved_functions.append(func)
        signature = self.read_function_signature()
        func.name = signature.name
        func.func_type = signature.func_type

        if func.func_type == ASFUNC_VIRTUAL:
            self.r.encoded_uint()  # vfTableIdx
        if func.func_type != ASFUNC_SCRIPT:
            # EVERY function reads the trailing section name when debug info is present —
            # not just script ones. Missing this desyncs on the first funcdef.
            if not self.no_debug_info:
                self.r.string()
            return func

        self.read_bytecode(func)
        self.r.encoded_uint()  # variableSpace
        obj_vars = self.r.count("object variable")
        for _ in range(obj_vars):
            self.read_object_type()
            self.r.encoded_uint()
            self.r.encoded_uint()
        if obj_vars > 0:
            self.r.encoded_uint()  # objVariablesOnHeap
        for _ in range(self.r.count("object variable info")):
            self.r.encoded_uint()
            self.r.encoded_uint()
            self.r.encoded_uint()

        if not self.no_debug_info:
            for _ in range(self.r.count("line number")):
                self.r.encoded_uint()
            section_count = self.r.count("section index")
            for i in range(section_count):
                if (i & 1) == 0:
                    self.r.encoded_uint()
                else:
                    self.r.string()

        self.r.u8()  # isShared

        if not self.no_debug_info:
            for _ in range(self.r.count("variable")):
                self.r.encoded_uint()  # declaredAtProgramPos
                self.r.encoded_uint()  # stackOffset
                self.r.string()  # name
                self.read_data_type()

        self.r.u8()  # dontCleanUpOnException
        # Trailing section name, for every function type.
        if not self.no_debug_info:
            self.r.string()
        return func

    def read_object_type_declaration(self, ot: ObjectType, phase: int) -> list[tuple[str, int]]:
        """``ReadObjectTypeDeclaration`` (:798). Returns enum members when phase 2 reads one."""
        if phase == 1:
            ot.name = self.r.string()
            # BIG-ENDIAN — see the module docstring.
            ot.flags = self._u32_be()
            ot.size = self.r.encoded_uint()
            ot.namespace = self.r.string()
            return []

        if phase == 2:
            if ot.is_enum:
                members: list[tuple[str, int]] = []
                for _ in range(self.r.count("enum value")):
                    name = self.r.string()
                    members.append((name, self._i32_be()))
                return members
            if ot.is_typedef:
                self.r.encoded_uint()
                return []
            # The exact read order matters and is easy to get subtly wrong:
            #   derivedFrom, interfaces[], ONE destructor, then constructor/factory PAIRS,
            #   then methods[], then the virtual function table[].
            # The destructor is a single unconditional read, not a loop, and each entry in
            # the constructor loop reads TWO functions — a constructor and its factory.
            self.read_object_type()  # derivedFrom
            for _ in range(self.r.count("interface")):
                self.read_object_type()
            # GUARDED: `if(!IsInterface() && flags != TYPEDEF && flags != ENUM)`. An
            # interface has neither a destructor nor constructors, so reading them for one
            # desyncs on the very first interface.
            if not ot.is_interface and not ot.is_typedef and not ot.is_enum:
                self.read_function()  # beh.destruct — one, unconditional
                for _ in range(self.r.count("constructor")):
                    self.read_function()  # constructor
                    self.read_function()  # matching factory
            for _ in range(self.r.count("method")):
                self.read_function()
            for _ in range(self.r.count("virtual function")):
                self.read_function()
            return []

        # phase 3 — object properties.
        for _ in range(self.r.count("property")):
            self.r.string()
            self.read_data_type()
            self.r.u8()
        return []

    def read_global_property(self) -> GlobalProperty:
        """``ReadGlobalProperty`` (:1349). The init function is where the VALUE lives."""
        prop = GlobalProperty()
        prop.name = self.r.string()
        prop.namespace = self.r.string()
        self.read_data_type()
        if self._bool():
            prop.init = self.read_function()
        return prop

    # --- top level --------------------------------------------------------

    def read(self) -> Module:
        """``ReadInner`` (:107), in its exact section order, as far as scriptGlobals."""
        module = Module(total=len(self.r.data))
        self.no_debug_info = self._bool()
        module.no_debug_info = self.no_debug_info

        for _ in range(self.r.count("enum")):
            ot = ObjectType()
            self.read_object_type_declaration(ot, 1)
            members = self.read_object_type_declaration(ot, 2)
            module.enums.append((ot, members))

        for _ in range(self.r.count("class type")):
            ot = ObjectType()
            self.read_object_type_declaration(ot, 1)
            module.class_types.append(ot)

        for _ in range(self.r.count("funcdef")):
            self.read_function()

        # Interfaces first, then classes, then class properties — three separate passes,
        # in this order (:200-215). Getting the order wrong desyncs immediately.
        for ot in module.class_types:
            if ot.is_interface:
                self.read_object_type_declaration(ot, 2)
        for ot in module.class_types:
            if not ot.is_interface:
                self.read_object_type_declaration(ot, 2)
        for ot in module.class_types:
            if not ot.is_interface:
                self.read_object_type_declaration(ot, 3)

        for _ in range(self.r.count("typedef")):
            ot = ObjectType()
            self.read_object_type_declaration(ot, 1)
            self.read_object_type_declaration(ot, 2)

        for _ in range(self.r.count("global")):
            module.globals.append(self.read_global_property())

        module.consumed = self.r.pos
        return module
