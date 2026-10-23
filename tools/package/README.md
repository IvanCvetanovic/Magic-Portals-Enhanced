# The content packager

Builds the folder Magic Portals Enhanced reads its data from at run time, from the data that is on **your** machine. It is how a
release package gets its art, its sound and its levels; the repository holds none of them (`CLAUDE.md`, the hard rule), CI never holds
them, and nothing this tool writes goes into the tracked tree: it refuses to. Python 3, standard library only, one tool for
Windows, Linux and macOS (run on Python 3.14 on Windows and 3.12 on Linux; macOS has not been run). The folder it writes and how the game finds it are described in [`docs/packaging.md`](../../docs/packaging.md).

```
python tools/package/package.py content --repo <this repository> --reference <the reference folder> [--out <folder>] [--with-pad-art]
python tools/package/package.py verify  <content folder>
python tools/package/package.py shell   --content <content folder> --exe <the program> --out <folder> [--repo <this repository>]
python tools/package/package.py same    <folder> <folder>
```

**Before you run it**: the original's APK unpacked into `reference/extracted/`, and the converter run so that `out/levels`, `out/data` and
`out/assets` exist (README, *Local setup after cloning*, steps 1-8; `docs/packaging.md` §3 lists them); `reference/magic-rampage/` only for
`--with-pad-art`. None of it is in a fresh clone.

* `--repo` is this repository after the converter has run (it reads `out/levels`, `out/data`, `out/assets`, and the committed `data/` and
  `game/data/`).
* `--reference` is the folder with the original's extracted assets (`<reference>/extracted/assets`) and, optionally,
  `<reference>/magic-rampage`.
* `--out` defaults to `<repo>/out/dist/content`. Give a full path; any folder outside the repository, or under its gitignored `out/`, is
  allowed.
* `--with-pad-art` also packages Magic Rampage's control pictures. They are optional: without them the game draws the original's corner
  sprites and then its own glyphs. Off by default.

## What goes in, and how that is decided

Nothing is listed in this repository. `closure.py` follows the chain the game follows, from the owner's own tables: the converted levels
name their textures; the committed JSON tables name images, sounds, fonts and entities; an entity names its particle bitmaps; a
picture is looked for in its density tiers (`fullhd/`, `hd/`, from `game/data/tiers.json`) and then as itself. An entity file also names
its own picture, its normal map and its light's halo, and a table that spells a suffix (`_blue.png`) means the entity's own name in front
of it: those are followed too. Every file those names land on is packaged, with the 1x file under a picked tier twin (what the game falls
back to), and nothing else. The folders the converter
or the extraction leave that nothing names (the original's scenes, `.pvr` and `.pkm` pictures, `.par` effects, the framework folder, the
Android bundle, the baked lightmaps) are left out, and a table that were to name one is an error, not a quiet inclusion.

The lightmap pictures the converter writes (`out/assets/lightmaps`) are **not** packaged: the layer bakes a lit sprite's lightmap in
memory (planning step 196) and never opens them. The 128-level sweep run from a package without them is what proves it.

## What it refuses (exit 2, with the cause)

* an output folder inside the repository's tracked tree, on top of a source folder, or inside the reference folder;
* a non-empty output folder that holds no `manifest.json` written by this tool (a package it wrote is replaced whole; anything else is not touched);
* a symbolic link to a file it would copy, or as the root of a source tree. **Not yet refused: a symbolic link to a sub-folder of a source
  tree** (the folder is not walked, so its files are missing from the package, and a tier twin in it silently falls back to the 1x file; a
  dangling link ends in a traceback). A link is not something the converter or the extraction makes; the sweep from the package alone is
  what would show the loss (planning step 200, Left For Later);
* a name that matches a file only when case is ignored (the package would work on Windows and macOS and fail on Linux and Android), also for a tier
  twin: a wrong-case `hd/` file would otherwise quietly fall back to the 1x one;
* anything on the never-package list (`NEVER_*` in `package.py`).

## What it writes

`manifest.json` (every file with its size, sha256, kind and role, plus the references that name nothing in the data) and `SHA256SUMS.txt`
(`sha256sum -c` form) sit at the top of the output. The same input gives the same bytes in the same order with the same times, so two runs
can be compared with `same`. `verify` re-checks a folder against its manifest and then runs the same model over **the package itself**:
every name the game can open must resolve, in exact case, inside it, and nothing in it may be surplus. A finished `content` run does that
before it moves the folder into place.

## Tests

```
python -m unittest discover -s tools/package -p "test_*.py" -v
```

The tests build a miniature tree of invented names and bytes (`tile_a.png`, `boom_a.mp3`, a hand-made UTF-16 `.ent`): no file of the original
and no name from it is in this repository. They cover the closure, the refusals, the manifest, the never-package list, determinism and the
`shell` layout. Two of them make a symbolic link and skip themselves where the system will not.
