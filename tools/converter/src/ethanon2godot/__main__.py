"""CLI entry point: ``ethanon2godot``."""

from __future__ import annotations

import argparse
import sys
from collections import Counter
from pathlib import Path

from . import __version__, lighting, manifest, tiers, tscn, uiassets
from .scene import lightmap_directory, parse_file

# Defaults are repo-relative. Both are gitignored — see the repo .gitignore.
# reference/extracted/ holds unpacked APK contents; out/ is conversion staging.
DEFAULT_INPUT = Path("reference/extracted")
DEFAULT_OUTPUT = Path("out")

# Chapter 1 is world 0, and world N is the .esc suffix ''/'a'/'b'/'c'
# (docs/ethanon-formats.md, 2026-08-04 block). Pinned from achievement strings in
# android_game.bin cross-referenced against entity placement, not guessed.
WORLD_SUFFIX = ("", "a", "b", "c")
LEVELS_PER_WORLD = 32


def level_names(world: int) -> list[str]:
    """The ``.esc`` stem for every level of ``world``, in play order."""
    suffix = WORLD_SUFFIX[world]
    return [f"level{i}{suffix}" for i in range(LEVELS_PER_WORLD)]


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
        "--world",
        type=int,
        default=0,
        choices=range(len(WORLD_SUFFIX)),
        help="world index to convert; 0 == chapter 1 (default: %(default)s)",
    )
    p.add_argument(
        "--level",
        type=int,
        metavar="N",
        help="convert only level N of the world, instead of all 32",
    )
    p.add_argument(
        "--copy-textures",
        action="store_true",
        help=(
            "also copy every referenced sprite into <output>/assets/entities/, and the "
            "density tier file the original draws at 720 px into entities/<tier>/, recording "
            "the choice in <output>/assets/entity_tiers.json. Output stays gitignored; see "
            "the hard rule in CLAUDE.md."
        ),
    )
    p.add_argument(
        "--copy-ui",
        action="store_true",
        help=(
            "also copy assets/sprites/ (UI, menus, medals) with its hd/ tier, the bitmap fonts "
            "from assets/data/, and assets/soundfx/ into "
            "<output>/assets/. Output stays gitignored."
        ),
    )
    p.add_argument(
        "--copy-lighting",
        action="store_true",
        help=(
            "also copy the files the eth_* lighting keys name: attached lightmaps into "
            "<output>/assets/lightmaps/<scene>/, every normal map into "
            "<output>/assets/entities/normalmaps/, and the named halo bitmaps into "
            "<output>/assets/entities/, recording each file's density tier in "
            "<output>/assets/lighting_files.json. Output stays gitignored."
        ),
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

    scenes_dir = args.input / "assets" / "scenes"
    if not scenes_dir.is_dir():
        print(
            f"error: no scenes directory at {scenes_dir}\n"
            "Drop the original APK in reference/apk/ and unpack it into "
            "reference/extracted/ first (see MILESTONE_1.md §7).",
            file=sys.stderr,
        )
        return 2

    names = level_names(args.world)
    if args.level is not None:
        if not 0 <= args.level < LEVELS_PER_WORLD:
            print(f"error: --level must be 0..{LEVELS_PER_WORLD - 1}", file=sys.stderr)
            return 2
        names = [names[args.level]]

    out_dir = args.output / "levels"
    tex_dir = args.output / "assets" / "entities"
    if not args.dry_run:
        out_dir.mkdir(parents=True, exist_ok=True)

    total_nodes = total_sprites = total_colliders = 0
    all_textures: set[str] = set()
    all_skipped: list[str] = []
    converted = 0
    lighting_keys: Counter[str] = Counter()
    lighting_ungated: Counter[str] = Counter()
    static_lights = 0
    all_lightmaps: list[tuple[Path, str]] = []
    all_normals: set[str] = set()
    all_halos: set[str] = set()

    for name in names:
        src = scenes_dir / f"{name}.esc"
        if not src.is_file():
            print(f"error: missing scene {src}", file=sys.stderr)
            return 2

        scene = parse_file(src)
        text, report = tscn.write_scene(
            scene,
            root_name=name,
            sprite_dir=args.input / "assets" / "entities",
            lightmap_dir=lightmap_directory(src),
        )

        total_nodes += report.nodes
        total_sprites += report.sprites
        total_colliders += report.colliders
        all_textures |= report.textures
        all_skipped += report.skipped
        converted += 1
        lighting_keys += report.lighting
        lighting_ungated += report.lighting_ungated
        static_lights += report.static_lights
        all_lightmaps += report.lightmaps
        all_normals |= report.normals
        all_halos |= report.halos

        if not args.dry_run:
            (out_dir / f"{name}.tscn").write_text(text, encoding="utf-8")
        if args.verbose:
            print(
                f"{name}: {report.nodes} nodes, {report.sprites} sprites, "
                f"{report.colliders} colliders, {len(report.skipped)} skipped"
            )

    # The manifest is what turns loose .tscn files into an ordered chapter with gold
    # thresholds. Emitted for the whole world even when --level converted only one, so a
    # single-level run does not silently produce a one-level chapter.
    if not args.dry_run:
        manifest_path = args.output / "data" / "chapters.json"
        scores = manifest.read_golden_scores(args.input / "assets" / "goldenScores.enml")
        if not scores:
            print(
                "warning: no goldenScores.enml — gold thresholds will be unknown (-1)",
                file=sys.stderr,
            )
        manifest.write(
            manifest_path,
            manifest.build(
                args.world,
                level_names(args.world),
                scores,
                existing=manifest.read(manifest_path),
            ),
        )
        print(f"manifest: {manifest_path}")

    if args.copy_textures and not args.dry_run:
        # The 1x file and the density tier file the original draws at 720 px (tiers.py; owner
        # rulings R12, R13). A tier file never changes the .tscn: the port resolves it.
        tex_dir.mkdir(parents=True, exist_ok=True)
        settings, warning = tiers.read_density_settings(args.input / "assets" / "app.enml")
        if warning:
            print(f"warning: {warning}", file=sys.stderr)
        tex_report = tiers.copy_entity_textures(args.input, args.output, all_textures, settings)
        print(
            f"entity textures: 1x copied {tex_report.copied[tiers.TIER_BASE]}, "
            f"unchanged {tex_report.unchanged[tiers.TIER_BASE]}; tier files copied "
            f"{tex_report.copied['tier']}, unchanged {tex_report.unchanged['tier']} (to {tex_dir})"
        )
        print(
            "entity texture tiers: "
            + ", ".join(f"{name} {count}" for name, count in sorted(tex_report.tiers.items()))
            + f" (recorded in {args.output / 'assets' / tiers.MANIFEST_NAME})"
        )
        for line in tex_report.size_changes:
            print(f"tier size change: {line}")
        for line in tex_report.no_base:
            print(f"tier file without a 1x file: {line}")
        for line in tex_report.skipped:
            print(f"SKIPPED {line}", file=sys.stderr)

    if args.copy_ui and not args.dry_run:
        # Wholesale, and tier-aware: see uiassets.py for why `hd/` is safe and `fullhd/`
        # is not.
        ui = uiassets.copy_ui_assets(args.input, args.output)
        for group, count in ui.copied.items():
            print(f"copied {count} {group} file(s) to {args.output / 'assets'}")
        for line in ui.rewritten:
            print(f"rewrote {line}")
        for line in ui.skipped:
            print(f"SKIPPED {line}", file=sys.stderr)

    if args.copy_lighting and not args.dry_run:
        lit = lighting.copy_lighting_assets(
            args.input,
            args.output,
            lightmaps=all_lightmaps,
            normals=all_normals,
            halos=all_halos,
        )
        lit_tiers = Counter(entry["tier"] for entry in lit.files.values())
        for group, count in lit.copied.items():
            print(f"copied {count} {group} file(s) to {args.output / 'assets'}")
        print(
            "lighting file tiers: "
            + ", ".join(f"{tier} {count}" for tier, count in sorted(lit_tiers.items()))
            + f" (recorded in {args.output / 'assets' / lighting.MANIFEST_NAME})"
        )
        for line in lit.skipped:
            print(f"SKIPPED {line}", file=sys.stderr)

    # Every lighting key, so a downstream pin can be set from this output rather than
    # from an inventory. "all instances" ignores the sprite-or-light gate (see tscn.py).
    print("lighting keys (emitted / over all instances):")
    for key in tscn.ETH_SCENE_KEYS:
        print(f"  {key}: {lighting_keys[key]}")
    for key in tscn.ETH_ENTITY_KEYS:
        print(f"  {key}: {lighting_keys[key]} / {lighting_ungated[key]}")
    print(f"  lights on static owners: {static_lights}")
    print(
        f"  metadata strings added: {sum(lighting_keys.values())}; distinct normal maps "
        f"{len(all_normals)}, halo bitmaps {len(all_halos)}"
    )

    verb = "would convert" if args.dry_run else "converted"
    print(
        f"{verb} {converted} scene(s) from world {args.world}: "
        f"{total_nodes} nodes, {total_sprites} sprites, {total_colliders} colliders, "
        f"{len(all_textures)} distinct textures"
    )
    if all_skipped:
        # Never let a partial conversion look complete — CLAUDE.md's "no silent caps".
        print(f"SKIPPED {len(all_skipped)} instance(s):", file=sys.stderr)
        for line in all_skipped[:20]:
            print(f"  {line}", file=sys.stderr)
        if len(all_skipped) > 20:
            print(f"  … and {len(all_skipped) - 20} more", file=sys.stderr)
    if not args.dry_run:
        print(f"output: {out_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
