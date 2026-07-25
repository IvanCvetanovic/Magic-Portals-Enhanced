"""CLI entry point: ``ethanon2godot``."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from . import __version__

# Defaults are repo-relative. Both are gitignored — see the repo .gitignore.
# reference/extracted/ holds unpacked APK contents; out/ is conversion staging.
DEFAULT_INPUT = Path("reference/extracted")
DEFAULT_OUTPUT = Path("out")


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        prog="ethanon2godot",
        description="Convert Ethanon Engine scene/entity data into Godot 4 .tscn scenes.",
        epilog=(
            "Neither the input nor the output of this tool may be committed to the "
            "repository. See CLAUDE.md."
        ),
    )
    p.add_argument("--version", action="version", version=f"%(prog)s {__version__}")
    p.add_argument(
        "-i",
        "--input",
        type=Path,
        default=DEFAULT_INPUT,
        metavar="DIR",
        help="unpacked Ethanon project root (default: %(default)s)",
    )
    p.add_argument(
        "-o",
        "--output",
        type=Path,
        default=DEFAULT_OUTPUT,
        metavar="DIR",
        help="where to write converted scenes (default: %(default)s)",
    )
    p.add_argument(
        "--dry-run",
        action="store_true",
        help="parse and report what would be converted, writing nothing",
    )
    p.add_argument("-v", "--verbose", action="store_true", help="verbose output")
    return p


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)

    if not args.input.exists():
        print(
            f"error: input directory not found: {args.input}\n"
            "Drop the original APK in reference/apk/ and unpack it into "
            "reference/extracted/ first (see MILESTONE_1.md §7).",
            file=sys.stderr,
        )
        return 2

    print(
        "ethanon2godot is scaffolding — the parsers are not implemented yet.\n"
        "The format spec is docs/ethanon-formats.md; implement scene.py / entity.py /\n"
        "enml.py against it, validating on reference/ethanon/testbed/ before touching\n"
        "extracted game data.",
        file=sys.stderr,
    )
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
