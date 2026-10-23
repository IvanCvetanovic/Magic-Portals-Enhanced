# Where the game finds its data, and how a package is built

Magic Portals Enhanced reads three kinds of data at run time: the levels the converter wrote, the original's art and sound, and its own
tables (`data/`, `game/data/`). None of the first two is in this repository (`CLAUDE.md`, the hard rule), so the game has to be told where
they are. Since planning step 200 it works that out when it starts; it no longer depends on the paths of the machine that built it.

This page is the record of that rule, of the folder a package holds, and of the tool that builds it. The step is in
[`planning/2026-09-11-magic-portals-remaster.md`](planning/2026-09-11-magic-portals-remaster.md) (Step 200); the code is
`game/sim/AssetRoot.{hpp,cpp}`, `game/sim/Scratch.{hpp,cpp}` and `tools/package/`.

## 1. How the game finds its data

At start the game looks for a *root*: the first folder from this list that holds the game, meaning it has the converted levels and the
chapters table. A folder is read in either of the two shapes in section 2.

| # | Tier | Folder |
|---|---|---|
| 1 | `--root <absolute folder>` | the folder you name. **Strict**: one that does not hold the game ends the run with a message that names it (exit 1); it is never skipped for a later tier. A relative path is refused. |
| 2 | next to the program | `<program folder>/content`, then `<program folder>` itself |
| 3 | the application bundle | a mobile or macOS entry point passes it (no desktop does) |
| 4 | the user's data folder | `<data folder>/Magic Portals/content`: `%APPDATA%\Magic Portals\content` on Windows, `~/.local/share/Magic Portals/content` (or under `$XDG_DATA_HOME`) on Linux, `~/Library/Application Support/Magic Portals/content` on macOS. `content` is only looked at, never made. The `Magic Portals` folder above it is the player's data folder and the engine makes it when it names it (`Supersonic::UserDataDirectory`), which the game asks whenever tiers 1-3 did not answer: so **a developer's run, and a failed start, leave an empty `Magic Portals` folder where there was none** (it is where the saves go anyway; the engine has no variant that only looks, and the resolver reads no environment variable to do without it: planning step 200, Recorded Deviation 2). |
| 5 | the paths the build was configured with | what a developer's build has always used (`SUPERSONIC_MAGICPORTALS_*` in CMake). Taken as they are, without asking whether they hold the game, so a build with no data still starts and draws boxes, as before. |

Nothing reads an environment variable for this. A tier that has no answer is skipped; when none answers the game says where it looked and
exits 1.

The first lines of the game's log name the root and the tier (`asset root: ... (tier: ..., layout: ...)`), then the scratch folder
(section 5), so a report from another machine says what it found.

**Flags** (beside the existing `--levels`, `--art`, `--data`, `--padart`): `--root`, `--original`, `--chapters`, `--achievements`,
`--port-data`. Each replaces the answer for that one path and nothing else. `--levels` brings the art with it (the folder above the levels,
as it always has) unless `--art` is given, and does not move the chapters table. **`--no-baked-paths`** ignores tier 5 for the run, so a
package can be proven to hold everything the game opens.

**A release build.** `-DMAGICPORTALS_BAKED_PATHS=OFF` (default ON) compiles every data path to an empty string, sets the engine's own
baked checkout path to empty, and maps the checkout and build folders out of the compiler's source paths, so the program carries no
account name and no absolute repository path (measured with g++ on Linux: the binary holds no `icvet`, no `/mnt/c` and no `/home`; the
MSVC build with the option off has not been made and has no `/d1trimfile` yet, which the Windows release step owns). It then has no tier 5: a run with no root fails with the message above, never with a stray
relative open. The suites read the baked paths, so the tests are not built by default in that configuration. Use a build folder of its own
(`tools/build_linux.sh --no-baked-paths --build-dir <folder>` does).

Every path handed on to the game is spelled out: no `..` segment is left in a resolved path.

## 2. The two shapes of a root

`Layout::Repo()` is this repository, relative to its root: `out/levels`, `out/data/chapters.json`, `out/data/achievements.json`, `data`,
`game/data`, `reference/extracted/assets`, `reference/magic-rampage/controls`, and `out` as the folder `res://` stands for (the converted
art is `out/assets/...`). It is what a developer's checkout, or a copy of one, looks like.

`Layout::Packaged()` is the folder the packager writes and a release ships. A folder is tried as Packaged first, then as Repo.

```
content/
  levels/              the converted levels (*.tscn)
  assets/              what res:// stands for: assets/entities/** (the converted art, tier folders included)
  tables/              chapters.json, achievements.json
  data/                Enhanced's tables
  port-data/           the port's own tables
  original/            the original's files the game opens: sprites/, entities/, particles/, data/, soundfx/
  pad-art/             Magic Rampage's control pictures (controls/, editor/): only with the packager's --with-pad-art
  manifest.json        every file, with size, sha256, kind and role; and the references that name nothing in the data
  SHA256SUMS.txt       the same hashes in `sha256sum -c` form
```

A desktop package puts the program beside it (`package.py shell` lays this out and proves it):

```
MagicPortals(.exe)
assets/shaders/*.spv   the engine opens its 17 shaders by relative path from the program's own folder
content/               the folder above
```

The engine makes the program's folder its working directory when it holds `assets/shaders`. It writes there (a pipeline cache), so it
belongs in a folder the player may write to: a portable folder, or a per-user install; a read-only install location loses the cache.

## 3. What the packager puts in

**Before you run it** you need the owner's inputs on the machine; none of them is in this repository (README, *Local setup after cloning*,
has the steps):

1. the original's APK unpacked into `reference/extracted/` (its `assets/` folder is `<reference>/extracted/assets`);
2. the converter run, so that `out/levels`, `out/data` and `out/assets` exist (`tools/converter`, and `tools/asbc` for `out/data/achievements.json`);
3. optionally `reference/magic-rampage/` (the modern controls' pictures; `docs/magic-rampage-controls.md` says where they come from), for
   `--with-pad-art`.

```
python tools/package/package.py content --repo <this repository> --reference <the reference folder> [--out <folder>] [--with-pad-art]
```

It runs on the owner's machine (run on Windows and Linux; macOS has not been run), reads the owner's data, and writes by default to `<repo>/out/dist/content`.
It is not run in CI, and nothing it writes can go into the repository: it refuses an output folder in the tracked tree. See
[`tools/package/README.md`](../tools/package/README.md) for what it refuses and how it is tested.

The set of files is **derived from the tables each time**, not listed: the converted levels, the two tables beside them, the committed
tables, and exactly the files those name (a texture in its density tiers `fullhd/`, `hd/`, then as itself; the entity files; the particle
bitmaps each entity names; the fonts and their pages; the sounds and the music), then what the entity files themselves name (the picture
in `<Sprite>`, the normal map in `<Normal>`, the light's `<HaloBitmap>`, and the colour variants: the entity's own name in front of a
suffix a table spells, such as `_blue.png`), plus the 1x file under a tier twin that was picked.
On the owner's data (2026-10-23) that is 883 files, 24,584,458 bytes (725 required, 158 fallback: 472 pictures, 180 entity files, 54 tables,
128 levels, 41 sounds, 5 music tracks, 3 fonts); 896 files and 24,704,515 bytes with `--with-pad-art`; `package.py shell` adds the 17
shaders (118,716 bytes). Compared with the research's list (`PLAT/case_audit/package_files.csv`, 757 files and the 17 shaders): 5 page
pictures of fonts that no table names are left out (651,321 bytes), and 131 files are added (4,298,588 bytes): 55 sprite pictures, 27
normal maps, 1 halo and 6 colour variants of entities the closure already held, and the 1x fallbacks under them (42). **The first run of the
sweep from a package built without them found the gap**: eight pictures the game opens (the black bars' own, a shooter's bolt, the
coloured keys, keyholes and locked doors) are named by no table, only by an entity file or by a suffix; every other file, size and role is
equal to the research's.

**Left out, on purpose:** the converter's lightmap pictures (`out/assets/lightmaps`, 730 files): the layer bakes a lit sprite's lightmap in
memory and never opens them (planning step 196), which the 128-level sweep run from a package without them proves; the original's
`scenes/` (the converter's input), `.pvr`, `.pkm`, `.par`, the framework folder, the Android bundle, `.enml` files, the APK, the engine
checkout, build folders, the converter's other output. A table that named one would be an error.

**Not yet:** sound off Windows (the engine decodes MP3 only there), the Android, iOS and macOS entry points, release archives, icons and
licence texts. They are later steps of the platform program.

## 4. Proving a package

* `package.py verify <folder>`: the manifest, every hash, nothing surplus, nothing on the never-package list, and the model of what the
  game opens run over the package itself: 0 references that match only ignoring case, the same references that name nothing as the
  source run, every lightmap key absent. `content` runs it before it moves the folder into place.
* `package.py same <a> <b>`: two runs are the same bytes in the same order.
* The 128-level sweep from the package alone: `MagicPortals --root <folder> --no-baked-paths --level <name> --fixed-step --frames 420
  ...` for every level, byte for byte against the sweep from the repository's own data. This proves the levels; the menu, the HUD and the
  dashboard are proved by `verify`'s model and by captures of the screens. **The proof is the byte comparison, not the game's log**: the
  game does not always say when a picture it wanted is missing (the verifier removed two pictures the first level draws from a copy of a package:
  the log did not change, the picture did), so "no log line says `could not be read`" is not evidence that nothing is missing. The
  comparison's power was measured that way, and the closure was cross-checked by tracing every file a game process opens over 150 planned
  Linux runs (the 128 win routes, the 18 screens, 4 modern-controls runs): 504 data files, all in the package except the optional pad art.
  Sound is not traced (Linux cannot decode MP3); the package holds every sound file of the original.

## 5. Where the game writes

The log (`magicportals.log`, and the last run's `.prev.log`) and the files the game generates and could make again (the collision
prisms the levels' polygons become, the control glyphs, the multiplies) go to a *scratch folder*, chosen when the game starts as the first
of these where a file can really be made (it is tried, not assumed):

1. the system's temporary folder (`%TEMP%` on Windows; the log is `<it>\magicportals.log`, the rest in `<it>\supersonic-magicportals`);
2. `<user data folder>/scratch`, the one place that is made if it is not there, and only if the first was refused;
3. the older last resorts: the user data folder, the program's folder, the working directory.

The second matters on Android, where the temporary folder the C++ library names is one an app cannot write, and on a shared `/tmp` whose
prism folder belongs to another account. The half-written prism file has a name of its own per call, so two windows building one level
cannot take each other's file.

The player's saves (`scores.json`, `audio.json`, `achievements.json`, `controls.json`) stay where they were: the user data folder itself.
