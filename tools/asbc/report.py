"""CLI: decode `android_game.bin` as far as the port currently reaches, and say where.

    python tools/asbc/report.py reference/extracted/assets/android_game.bin

Reporting the stopping point precisely is the point. A partial decode that says
"consumed 13,028 of 523,826 bytes, stopped in class `Button`'s method bytecode" is a
usable result; one that guesses onward produces plausible garbage, which is the failure
mode this whole project is built to avoid.
"""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

import module  # noqa: E402
from asbc import BytecodeError  # noqa: E402


def main(argv: list[str]) -> int:
    if len(argv) < 2:
        print(__doc__)
        return 2
    data = Path(argv[1]).read_bytes()

    for bits in (32, 64):
        reader = module.ModuleReader(data, pointer_bits=bits)
        try:
            mod = reader.read()
        except BytecodeError as error:
            print(f"pointer_bits={bits}: stopped at byte {reader.r.pos} of {len(data)} — {error}")
            continue
        print(f"pointer_bits={bits}: COMPLETE to scriptGlobals")
        print(f"  enums    : {len(mod.enums)}")
        print(f"  classes  : {len(mod.class_types)}")
        print(f"  globals  : {len(mod.globals)}")
        for prop in mod.globals:
            value = _constant_of(prop)
            if value is not None:
                print(f"    {prop.name:32s} = {value}")
        return 0
    return 1


def _constant_of(prop: module.GlobalProperty) -> float | int | None:
    """Pull a literal out of a global's initialiser, if it is a simple assignment.

    A `const float X = 0.35;` compiles to a `SetV4` whose DWORD operand is the IEEE-754
    bit pattern. Anything more complex than one Set instruction is left alone rather than
    guessed at.
    """
    if prop.init is None:
        return None
    import struct

    for mnemonic, operands in prop.init.code:
        if mnemonic in ("SetV4", "SetV1", "SetV2") and len(operands) >= 2:
            raw = operands[-1] & 0xFFFFFFFF
            as_float = struct.unpack("<f", struct.pack("<I", raw))[0]
            # A plausible gameplay constant rather than a bit pattern that happens to
            # decode as a denormal or an astronomically large float.
            if 1e-4 < abs(as_float) < 1e6:
                return round(as_float, 6)
            return raw
    return None


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
