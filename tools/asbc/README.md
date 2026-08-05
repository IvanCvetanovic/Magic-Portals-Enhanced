# `asbc` — AngelScript bytecode reader

Reads `android_game.bin`, the compiled AngelScript from the original APK.

**Not a decompiler, and deliberately not a translator.** `CLAUDE.md` is explicit that
Ethanon gameplay logic "is *not* converted — it is read as design reference and
reimplemented". This tool's output is findings, never GDScript, and it is kept out of the
`.esc` → `.tscn` pipeline: that pipeline converts *data*, this reads *code*.

```bash
python tools/asbc/report.py reference/extracted/assets/android_game.bin
```

## What it has already established

- **The `ETHCallback_` liveness test.** The engine derives each entity's script hook as
  `"ETHCallback_" + RemoveExtension(entityName)`, so an entity has behaviour **iff** that
  symbol exists in the binary. 96 do. This turned "which of 178 entities actually do
  something?" into an exact byte search and stopped three entities being animated whose
  custom data nothing reads.
- **`SCREEN_PAD_BUTTON` decodes to `NO_SCREEN_BUTTON`, `LEFT_SCREEN_BUTTON`,
  `RIGHT_SCREEN_BUTTON`.** There is no jump button. Independently confirmed by the owner.
- **All raw 4-byte fields in this file are big-endian**, which is not the host order the
  engine's `ReadData` implies. Two independent confirmations, in `module.py`'s docstring.

## What it does NOT yet do

Reach `scriptGlobals`, where the named constants live — `FIRST_PORTAL_MIN_TIME`,
`NEXT_PORTAL_MIN_TIME`, `g_portalCollisionRadius`. It stops at byte 13,028 of 523,826,
inside class `Button`'s method bytecode. See `module.py` for the exact state and the four
bugs already fixed.

Even reaching `scriptGlobals` is not sufficient on its own: `ReadGlobalProperty` stores a
name, a namespace, a type and an **init function**, so each value is an immediate operand
inside bytecode. `report.py` already implements that extraction (`_constant_of`) for the
simple `const float X = …` case; it just has nothing to run on yet.

## Regenerating the opcode table

`opcodes.py`'s `TABLE` is generated from the engine's own header rather than hand-written
— an earlier hand-written version had `CpyVtoG4` wrong:

```python
import re, pathlib
h = pathlib.Path('reference/ethanon/toolkit/Source/src/angelscript/include/angelscript.h').read_text()
body = h[h.index('const asSBCInfo asBCInfo[256]'):]
body = body[:body.index('\n};')]
rows = re.findall(r'asBCINFO(_DUMMY)?\(\s*(\w+)\s*(?:,\s*(\w+)\s*,)?', body)
```
