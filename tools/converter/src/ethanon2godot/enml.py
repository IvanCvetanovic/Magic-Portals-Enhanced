"""Parser for ENML, Ethanon's own text config format.

Used by ``app.enml`` (window size, title, density settings) and — the reason this
matters for level conversion — by the text payloads of ``<Polygon>``, ``<Compound>``
and ``<Joints>`` embedded inside ``.ent`` and ``.esc`` files
(``engine/Entity/ETHEntityProperties.cpp:373-407``).

Grammar is ``docs/ethanon-formats.md`` §6.1, itself derived from ``File::ParseString``
(``gs2d/src/Enml/Enml.cpp:313-448``). This is a port of that state machine rather than
a regex, because several of its behaviours are load-bearing and a regex quietly gets
them wrong:

* A **single** ``/`` starts a line comment (not ``//``).
* Only ``\\;`` and ``\\\\`` are escapes. **Any other backslash aborts the whole parse**
  — a raw Windows path in a value is an error, not a quirk (``Enml.cpp:645-671``).
* Trailing whitespace before ``;`` is preserved; leading whitespace after ``=`` is not.
* An empty value (``key = ;``) is an error, which is why every engine consumer can
  treat ``Get() == ""`` as "absent" unambiguously.
* On **any** error the engine calls ``Clear()`` and discards the entire file. We raise
  instead — same information, but it cannot be ignored by accident.
"""

from __future__ import annotations

from pathlib import Path

# Enml.cpp:91-94 — exactly these four bytes are "neutral". Note \v and \f are NOT.
_NEUTRAL = " \t\n\r"

# Enml.cpp:97-101 — IsValid() accepts only these. An empty name is *also* valid
# upstream and `}` does not terminate a name, but neither occurs in real data, so we
# reject empty names. Documented deviation, per §6.1's "a strict parser may reject".
_NAME_CHARS = frozenset(
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_"
)


class EnmlError(ValueError):
    """Raised where the engine would ``Clear()`` and report a line number.

    Carries the 1-based line number, matching ``File::ParseString``'s return value.
    """

    def __init__(self, message: str, line: int) -> None:
        super().__init__(f"line {line}: {message}")
        self.line = line


def _skip_comment(text: str, i: int) -> int:
    """Advance past a comment starting at ``text[i] == '/'``; returns the new index.

    One slash starts a line comment. ``/*`` starts a block comment, and a newline does
    NOT close it. An unterminated comment runs to end-of-buffer, which the engine
    treats as success — running off the end is only an error inside a value.
    """
    if text.startswith("/*", i):
        end = text.find("*/", i + 2)
        return len(text) if end < 0 else end + 2
    end = text.find("\n", i)
    return len(text) if end < 0 else end + 1


def _read_value(text: str, i: int, line: int) -> tuple[str, int]:
    """Read a value from ``i`` up to the next unescaped ``;``.

    Comments are NOT recognised inside a value, and ``}``, ``{``, ``/`` and ``=`` are
    all ordinary value bytes (``Enml.cpp:645-671``).
    """
    out: list[str] = []
    while i < len(text):
        c = text[i]
        if c == "\\":
            nxt = text[i + 1] if i + 1 < len(text) else ""
            if nxt not in (";", "\\"):
                # Deliberately fatal — mirrors the engine discarding the whole file.
                raise EnmlError(f"invalid escape '\\{nxt}' in value", line)
            out.append(nxt)
            i += 2
            continue
        if c == ";":
            return "".join(out), i + 1
        out.append(c)
        i += 1
    # Enml.cpp:670 — running off the end is an error *only* here, inside a value.
    raise EnmlError("unterminated value (missing ';')", line)


def parse(text: str) -> dict[str, dict[str, str]]:
    """Parse ENML source into ``{entity_name: {attribute: value}}``.

    The engine uses ``std::map``, so its key order is alphabetical and on-disk order is
    lost. We preserve insertion order instead — strictly more information, never less.
    Do not rely on either: §6 is explicit that order is not meaningful.
    """
    out: dict[str, dict[str, str]] = {}
    i = 0
    line = 1
    name = ""
    key = ""
    entity: dict[str, str] = {}
    # SS_ENTITY -> SS_BRACKET -> SS_ATTRIBUTE_KEY -> SS_ASSIGN -> SS_READ_VALUE
    state = "ENTITY"

    while i < len(text):
        c = text[i]
        if c in _NEUTRAL:
            if c == "\n":
                line += 1
            i += 1
            continue
        # A comment is legal everywhere except inside a value (see _read_value).
        if c == "/":
            j = _skip_comment(text, i)
            line += text.count("\n", i, j)
            i = j
            continue

        if state == "ENTITY":
            start = i
            while i < len(text) and text[i] in _NAME_CHARS:
                i += 1
            name = text[start:i]
            if not name:
                raise EnmlError(f"invalid entity name at {text[start:start + 12]!r}", line)
            entity = {}
            state = "BRACKET"

        elif state == "BRACKET":
            if c != "{":
                raise EnmlError(f"expected '{{' after entity {name!r}", line)
            i += 1
            state = "ATTRIBUTE_KEY"

        elif state == "ATTRIBUTE_KEY":
            if c == "}":
                # AddEntity (Enml.cpp:253-265) clears any same-named entity first, so a
                # later duplicate with an empty body EMPTIES the earlier one rather than
                # merging into it. Plain assignment reproduces that.
                out[name] = entity
                name = ""
                i += 1
                state = "ENTITY"
                continue
            start = i
            while i < len(text) and text[i] in _NAME_CHARS:
                i += 1
            key = text[start:i]
            if not key:
                raise EnmlError(f"invalid attribute name at {text[start:start + 12]!r}", line)
            state = "ASSIGN"

        elif state == "ASSIGN":
            if c != "=":
                raise EnmlError(f"expected '=' after attribute {key!r}", line)
            i += 1
            state = "READ_VALUE"

        else:  # READ_VALUE — leading neutrals already skipped by the loop head.
            value, i = _read_value(text, i, line)
            if not value:
                raise EnmlError(f"empty value for attribute {key!r}", line)
            line += value.count("\n")
            entity[key] = value  # duplicate key: last wins (Enml.cpp:152, m_map[k] = v)
            state = "ATTRIBUTE_KEY"

    # A pending uncommitted entity is silently lost upstream; match that rather than
    # inventing a half-parsed entity the engine would never have produced.
    return out


def parse_file(path: Path) -> dict[str, dict[str, str]]:
    """Parse an ENML file from disk.

    UTF-8 here is deliberate and is NOT the same rule as ``.ent``/``.esc``. Those are
    BOM-sniffed and canonically UTF-16LE (``textio.read_ethanon_xml``); ENML goes
    through the engine's separate ``FileManager::GetAnsiFileString``
    (``gs2d/src/Platform/FileManager.cpp:122-142``), which does not sniff.
    Do not "make this consistent" with the XML readers — they are different paths.

    ``utf-8`` and not ``utf-8-sig``: a BOM would make the first entity name invalid
    and kill the parse upstream, so accepting one here would be a lie about the format.
    """
    return parse(Path(path).read_text(encoding="utf-8"))
