"""Extract the original's achievement list, and the unlock rules a save alone decides.

    python tools/asbc/achievements.py --listing <all_functions.txt> [--out out/data/achievements.json]

WHY THIS EXISTS
---------------
The main menu's Achievements dashboard (Enhanced's ui3 spec, section 4) draws 82 rows
whose titles, descriptions, icons and points are ASANTEE-AUTHORED CONTENT. They may not be
committed to this repository or to the engine's (CLAUDE.md's hard rule; the owner's ruling
for the dashboard). So they are not written into any committed data file: this tool reads
them out of the decode and writes them to a GITIGNORED file under out/, beside the
converter's chapters.json, and the Supersonic port reads that file at run time the way it
reads the levels - and draws no rows when it is absent.

This source carries no achievement text. It knows only the SHAPE of the bytecode.

INPUT
-----
The disassembly listing every ui2/ui3 spec cites (`asbc/all_functions.txt`: one
`== Class::name  (section)  bytes a..b` header per function, then one instruction a line,
string constants in Python repr). It is produced outside this repository from
reference/extracted/assets/android_game.bin; the committed module.py does not yet reach
these functions (see README.md).

WHAT IS READ
------------
1. `AchievementManager::loadAchievementList` (bytes 250572..270969). One `Achievement`
   value is filled field by field and appended with `array::insertLast`, 82 times:
       STR 'x'   ... ADDSi Achievement.title | .description | .iconSprite   (strings)
       SetV4 vN, value ... LoadRObjR ..., Achievement.points | .id | .level | .world
       SetV1 vN, vB    ... LoadRObjR ..., Achievement.isNew | .isSecret     (bools)
   The list order is the dashboard's row order (ScoreDashboard::loop walks it by index).

2. The unlock rules that depend on nothing but the save, as the original's own main menu
   re-derives them from one (PortalMainMenu::checkPreviouslyUnlockAchievements, bytes
   202712..202943): for every world `registerMedalAchievements(world, false)`, and for every
   level `checkForLevelAchievements(world, level, getScore, 99999, 0, false)`.
   - registerMedalAchievements (bytes 399746..400758): the ids a chapter's completion
     percentage unlocks.
   - checkForLevelAchievements (bytes 401641..408734): the ids a level's medal unlocks.
     Every other condition in it needs the Game state (a string compare against 'Game'),
     an entity count or a portal count; the menu's call can never reach those, and neither
     can a save.
   Both are read by walking the bytecode's branches symbolically over the small domains
   the menu calls them with - world 0..3, level 0..31, medal 0..3, completion 0..100 - with
   numPortals 99999, gameTime 0 and the current state not 'Game', exactly as the menu's
   call has them. Any other call result is unknown, and a dispatch that needs one is not
   a save rule.

OUTPUT
------
JSON: `achievements` (the list, in order) and `unlock.from_completion` /
`unlock.from_level_medal`, each rule naming the byte range it came from.
"""

from __future__ import annotations

import argparse
import ast
import json
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
DEFAULT_OUT = REPO / "out" / "data" / "achievements.json"

LIST_FN = "AchievementManager::loadAchievementList"
MEDAL_FN = "registerMedalAchievements"
LEVEL_FN = "checkForLevelAchievements"
MENU_FN = "PortalMainMenu::checkPreviouslyUnlockAchievements"
TITLE_FN = "Achievement::getTitle"
DESCRIPTION_FN = "Achievement::getDescription"
ICON_FN = "Achievement::getIconSprite"

HEADER = re.compile(r"^== (\S+)\s+\((.*?)\)\s+bytes (\d+)\.\.(\d+)")
INSTR = re.compile(r"^\s*(\d+)\s+(\S+)\s*(.*?)\s*$")


@dataclass
class Function:
    name: str
    start: int
    end: int
    code: list[tuple[str, str]] = field(default_factory=list)


def read_listing(path: Path, wanted: set[str]) -> dict[str, Function]:
    found: dict[str, Function] = {}
    current: Function | None = None
    with path.open(encoding="utf-8") as listing:
        for line in listing:
            header = HEADER.match(line)
            if header:
                current = None
                if header.group(1) in wanted and header.group(1) not in found:
                    current = Function(header.group(1), int(header.group(3)), int(header.group(4)))
                    found[current.name] = current
                continue
            if current is None:
                continue
            instr = INSTR.match(line)
            if instr:
                if int(instr.group(1)) != len(current.code):
                    raise SystemExit(f"{current.name}: instruction {instr.group(1)} out of order")
                current.code.append((instr.group(2), instr.group(3)))
    missing = wanted - set(found)
    if missing:
        raise SystemExit(f"{path}: no {', '.join(sorted(missing))}")
    return found


def first_int(text: str) -> int:
    match = re.match(r"v?(-?\d+)", text)
    if not match:
        raise ValueError(text)
    return int(match.group(1))


# ---- 1. the list ------------------------------------------------------------

STRING_FIELDS = {"Achievement.title": "title", "Achievement.description": "description",
                 "Achievement.iconSprite": "icon"}
INT_FIELDS = {"Achievement.points": "points", "Achievement.id": "id", "Achievement.level": "level",
              "Achievement.world": "world"}
BOOL_FIELDS = {"Achievement.isNew": "is_new", "Achievement.isSecret": "is_secret"}


def read_list(fn: Function) -> list[dict]:
    entries: list[dict] = []
    record: dict = {}
    last_string: str | None = None
    last_value: int | None = None
    for op, arg in fn.code:
        if op == "STR":
            last_string = ast.literal_eval(arg)
        elif op in ("SetV4", "SetV1"):
            # "SetV4 v4, 15 ()" / "SetV1 v4, v1": the value is the second operand.
            last_value = first_int(arg.split(",", 1)[1].strip())
        elif op == "ADDSi" and arg in STRING_FIELDS:
            if last_string is None:
                raise SystemExit(f"{fn.name}: {arg} with no string before it")
            record[STRING_FIELDS[arg]] = last_string
            last_string = None
        elif op == "LoadRObjR":
            member = arg.split(",", 1)[1].strip()
            if member in INT_FIELDS or member in BOOL_FIELDS:
                if last_value is None:
                    raise SystemExit(f"{fn.name}: {member} with no value before it")
                if member in INT_FIELDS:
                    record[INT_FIELDS[member]] = last_value
                else:
                    record[BOOL_FIELDS[member]] = last_value != 0
                last_value = None
        elif op == "CALLSYS" and arg == "sys:array::insertLast":
            keys = set(STRING_FIELDS.values()) | set(INT_FIELDS.values()) | set(BOOL_FIELDS.values())
            # The value object is reused: a field not written again keeps its last value,
            # which is what copying it into the array does in the original.
            merged = {**(entries[-1] if entries else {}), **record}
            if set(merged) != keys:
                raise SystemExit(f"{fn.name}: entry {len(entries)} lacks {sorted(keys - set(merged))}")
            entries.append(merged)
            record = {}
    return entries


def strings_of(fn: Function) -> list[str]:
    return [ast.literal_eval(arg) for op, arg in fn.code if op == "STR"]


def read_secret(functions: dict[str, Function]) -> dict:
    """What a secret achievement shows while locked (Achievement::getTitle, getDescription,
    getIconSprite: `isSecret && !unlocked`).

    getDescription builds prefix + (world + 1) + separator + (level + 1): it loads the
    separator first (instructions 31..34, pushed with level + 1) and the prefix after
    (41..45, with world + 1), so the listing's two strings come in that order.
    """
    title = strings_of(functions[TITLE_FN])
    description = strings_of(functions[DESCRIPTION_FN])
    icon = strings_of(functions[ICON_FN])
    if len(title) != 1 or len(description) != 2 or len(icon) != 1:
        raise SystemExit("the secret achievement's getters are not the shape this reads")
    separator, prefix = description
    return {
        "_source": f"{TITLE_FN} bytes {functions[TITLE_FN].start}..{functions[TITLE_FN].end}, "
                   f"{DESCRIPTION_FN} bytes {functions[DESCRIPTION_FN].start}..{functions[DESCRIPTION_FN].end}, "
                   f"{ICON_FN} bytes {functions[ICON_FN].start}..{functions[ICON_FN].end}",
        "title": title[0],
        "description_prefix": prefix,
        "description_separator": separator,
        "icon": icon[0],
    }


# ---- 2. the unlock rules: a branch walk ----------------------------------------

JUMP_TAKEN = {
    "JZ": lambda v, n: v == n, "JNZ": lambda v, n: v != n,
    "JS": lambda v, n: v < n, "JNS": lambda v, n: v >= n,
    "JP": lambda v, n: v > n, "JNP": lambda v, n: v <= n,
}
TEST_TRUE = {
    "TZ": lambda v, n: v == n, "TNZ": lambda v, n: v != n,
    "TS": lambda v, n: v < n, "TNS": lambda v, n: v >= n,
    "TP": lambda v, n: v > n, "TNP": lambda v, n: v <= n,
}


class Unknown:
    """A value the walk cannot know - a call's result - which makes a rule depend on the game."""


UNKNOWN = Unknown()


@dataclass(frozen=True)
class Sym:
    name: str
    offset: int = 0  # the slot holds name - offset


@dataclass(frozen=True)
class Cond:
    sym: Sym
    test: str
    const: int


def target(arg: str) -> int:
    return int(arg.split("->")[1])


def slot(text: str) -> str:
    return re.match(r"(v-?\d+)", text.strip()).group(1)


def value_in(slots: dict[str, object], text: str) -> object:
    return slots.get(slot(text), UNKNOWN)


def split_domains(doms: dict[str, frozenset[int]], value: object, test: str, const: int,
                  table: dict) -> tuple[dict | None, dict | None]:
    """The domains where the test holds, and where it does not; both None when unknown."""
    if isinstance(value, int):
        return (doms, None) if table[test](value, const) else (None, doms)
    if isinstance(value, Sym):
        values = doms[value.name]
        yes = frozenset(v for v in values if table[test](v - value.offset, const))
        no = values - yes
        return ({**doms, value.name: yes} if yes else None, {**doms, value.name: no} if no else None)
    return None, None


def walk(fn: Function, params: dict[str, object], domains: dict[str, range],
         call_symbol: dict[str, str]) -> dict[int, list[dict[str, frozenset[int]]]]:
    """Every dispatch or unlock the function can reach, with the domains that reach it.

    `params` gives the value each slot starts with: a Sym over `domains`, or a constant.
    `call_symbol` names a call whose result is a symbol rather than unknown.
    """
    hits: dict[int, list[dict[str, frozenset[int]]]] = {}
    code = fn.code
    start_domains = {name: frozenset(values) for name, values in domains.items()}
    stack = [(0, dict(params), start_domains, None, None, None, None)]
    seen: set = set()
    steps = 0
    while stack:
        pc, slots, doms, flag, register, last_str, last_push = stack.pop()
        while True:
            steps += 1
            if steps > 2_000_000:
                raise SystemExit(f"{fn.name}: the walk did not end")
            if pc >= len(code):
                break
            key = (pc, tuple(sorted((k, repr(v)) for k, v in slots.items())),
                   tuple(sorted(doms.items())), repr(flag), repr(register))
            if key in seen:
                break
            seen.add(key)
            op, arg = code[pc]

            if op in ("CMPIu", "CMPIi"):
                left, right = arg.split(",", 1)
                flag = (value_in(slots, left), first_int(right.strip()))
                pc += 1
            elif op in ("CMPi", "CMPu", "CMPf", "CMPd", "CMPIf", "CmpPtr"):
                flag = (UNKNOWN, 0)
                pc += 1
            elif op in JUMP_TAKEN:
                to = target(arg)
                if flag is None or isinstance(flag[0], Unknown):
                    raise SystemExit(f"{fn.name}: {op} at {pc} on an unknown compare")
                yes, no = split_domains(doms, flag[0], op, flag[1], JUMP_TAKEN)
                if yes is not None:
                    stack.append((to, dict(slots), yes, flag, register, last_str, last_push))
                if no is None:
                    break
                doms = no
                pc += 1
            elif op in TEST_TRUE:
                if flag is None or isinstance(flag[0], Unknown):
                    register = UNKNOWN
                elif isinstance(flag[0], int):
                    register = int(TEST_TRUE[op](flag[0], flag[1]))
                else:
                    register = Cond(flag[0], op, flag[1])
                pc += 1
            elif op in ("JLowZ", "JLowNZ"):
                to = target(arg)
                if isinstance(register, Unknown) or register is None:
                    # A call's result decides it: neither side is a save rule.
                    break
                if isinstance(register, int):
                    taken = (register == 0) if op == "JLowZ" else (register != 0)
                    if taken:
                        pc = to
                    else:
                        pc += 1
                    continue
                true_doms, false_doms = split_domains(doms, register.sym, register.test, register.const, TEST_TRUE)
                zero_doms, nonzero_doms = false_doms, true_doms
                taken_doms, fall_doms = (zero_doms, nonzero_doms) if op == "JLowZ" else (nonzero_doms, zero_doms)
                if taken_doms is not None:
                    stack.append((to, dict(slots), taken_doms, flag, register, last_str, last_push))
                if fall_doms is None:
                    break
                doms = fall_doms
                pc += 1
            elif op == "JMP":
                pc = target(arg)
            elif op == "JMPP":
                index = value_in(slots, arg)
                if not isinstance(index, Sym):
                    raise SystemExit(f"{fn.name}: JMPP at {pc} on no symbol")
                for value in sorted(doms[index.name]):
                    case = value - index.offset
                    entry = pc + 1 + case
                    if case < 0 or entry >= len(code) or code[entry][0] != "JMP":
                        raise SystemExit(f"{fn.name}: JMPP at {pc} has no case {case}")
                    stack.append((entry, dict(slots), {**doms, index.name: frozenset([value])}, flag,
                                  register, last_str, last_push))
                break
            elif op == "SetV4" or op == "SetV1":
                dst, value = arg.split(",", 1)
                slots[slot(dst)] = first_int(value.strip())
                pc += 1
            elif op == "CpyRtoV4":
                slots[slot(arg)] = register if register is not None else UNKNOWN
                pc += 1
            elif op == "CpyVtoV4":
                dst, src = arg.split(",", 1)
                slots[slot(dst)] = value_in(slots, src)
                pc += 1
            elif op == "CpyVtoR4":
                register = value_in(slots, arg)
                pc += 1
            elif op == "SUBIi" or op == "ADDIi":
                dst, src, const = (part.strip() for part in arg.split(","))
                source = value_in(slots, src)
                amount = first_int(const)
                sign = 1 if op == "SUBIi" else -1
                if isinstance(source, Sym):
                    slots[slot(dst)] = Sym(source.name, source.offset + sign * amount)
                elif isinstance(source, int):
                    slots[slot(dst)] = source - sign * amount
                else:
                    slots[slot(dst)] = UNKNOWN
                pc += 1
            elif op == "STR":
                last_str = ast.literal_eval(arg)
                pc += 1
            elif op == "PshC4":
                last_push = first_int(arg)
                pc += 1
            elif op in ("CALL", "CALLSYS", "CALLINTF"):
                if arg in ("dispatchAchievement", "AchievementManager::unlock"):
                    hits.setdefault(last_push, []).append(dict(doms))
                    register = UNKNOWN
                elif arg == "sys:string::opEquals" and last_str == "Game":
                    register = 0  # the menu's call: the current state is not Game
                elif arg in call_symbol:
                    register = Sym(call_symbol[arg])
                else:
                    register = UNKNOWN
                pc += 1
            elif op == "RET":
                break
            else:
                pc += 1
    return hits


def upward(values: frozenset[int], domain: range) -> int | None:
    """The least value of a set that is every value from there to the domain's top."""
    if not values:
        return None
    least = min(values)
    return least if values == frozenset(range(least, domain.stop)) else None


def read_rules(functions: dict[str, Function]) -> dict:
    percent = range(101)
    worlds = range(4)
    levels = range(32)
    medals = range(4)

    medal_fn = functions[MEDAL_FN]
    medal_hits = walk(medal_fn, {"v0": Sym("world"), "v-1": 0}, {"world": worlds, "percent": percent},
                      {"ScoreManager::computeWorldAccomplishment": "percent"})
    from_completion = []
    for aid, reaches in sorted(medal_hits.items()):
        by_world: dict[int, set[int]] = {}
        for doms in reaches:
            for world in doms["world"]:
                by_world.setdefault(world, set()).update(doms["percent"])
        for world, values in sorted(by_world.items()):
            least = upward(frozenset(values), percent)
            if least is None:
                raise SystemExit(f"{MEDAL_FN}: id {aid} in world {world} is not a lower bound")
            from_completion.append({"world": world, "min_percent": least, "id": aid})

    level_fn = functions[LEVEL_FN]
    level_hits = walk(level_fn,
                      {"v0": Sym("world"), "v-1": Sym("level"), "v-2": Sym("medal"), "v-3": 99999, "v-4": 0,
                       "v-5": 0},
                      {"world": worlds, "level": levels, "medal": medals}, {})
    from_level = []
    for aid, reaches in sorted(level_hits.items()):
        cells: dict[tuple[int, int], set[int]] = {}
        for doms in reaches:
            for world in doms["world"]:
                for level in doms["level"]:
                    cells.setdefault((world, level), set()).update(doms["medal"])
        for (world, level), values in sorted(cells.items()):
            least = upward(frozenset(values), medals)
            if least is None or least == 0:
                raise SystemExit(f"{LEVEL_FN}: id {aid} at {world}-{level} is not a medal bound")
            from_level.append({"world": world, "level": level, "min_medal": least, "id": aid})

    return {
        "_about": [
            "What a save alone unlocks, as PortalMainMenu::checkPreviouslyUnlockAchievements re-derives it:",
            "from_completion is registerMedalAchievements' (a chapter's completion percentage,",
            "floor(100 * sum of its medals / (3 * its levels)), at least min_percent); from_level_medal is",
            "checkForLevelAchievements' (a level's medal at least min_medal). Every other rule needs the",
            "game itself and is not here.",
        ],
        "from_completion_source": f"{MEDAL_FN} bytes {medal_fn.start}..{medal_fn.end}",
        "from_completion": from_completion,
        "from_level_medal_source": f"{LEVEL_FN} bytes {level_fn.start}..{level_fn.end}",
        "from_level_medal": from_level,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--listing", type=Path, required=True, help="the asbc all_functions.txt listing")
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT)
    args = parser.parse_args()

    functions = read_listing(args.listing, {LIST_FN, MEDAL_FN, LEVEL_FN, MENU_FN, TITLE_FN, DESCRIPTION_FN, ICON_FN})
    listed = functions[LIST_FN]
    entries = read_list(listed)
    ids = [entry["id"] for entry in entries]
    if len(set(ids)) != len(ids):
        raise SystemExit(f"{LIST_FN}: an id appears twice")
    rules = read_rules(functions)
    unknown = {rule["id"] for key in ("from_completion", "from_level_medal") for rule in rules[key]} - set(ids)
    if unknown:
        raise SystemExit(f"a rule unlocks ids the list does not have: {sorted(unknown)}")

    document = {
        "format": 1,
        "_about": [
            "ASANTEE-AUTHORED CONTENT. Gitignored output of tools/asbc/achievements.py; never commit it.",
            "The original's achievement list in AchievementManager order (the dashboard's row order),",
            "and the unlock rules a save alone decides.",
        ],
        "source": {
            "list": f"{LIST_FN} bytes {listed.start}..{listed.end}",
            "menu_check": f"{MENU_FN} bytes {functions[MENU_FN].start}..{functions[MENU_FN].end}",
        },
        "achievements": entries,
        "secret": read_secret(functions),
        "unlock": rules,
    }
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(document, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    worlds: dict[int, int] = {}
    for entry in entries:
        worlds[entry["world"]] = worlds.get(entry["world"], 0) + 1
    print(f"wrote {args.out}: {len(entries)} achievements (by world {dict(sorted(worlds.items()))}, "
          f"{sum(1 for e in entries if e['is_secret'])} secret), "
          f"{len(rules['from_completion'])} completion rules, {len(rules['from_level_medal'])} medal rules")
    return 0


if __name__ == "__main__":
    sys.exit(main())
