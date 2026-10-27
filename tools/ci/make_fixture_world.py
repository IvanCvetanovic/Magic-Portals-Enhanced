#!/usr/bin/env python3
"""A SYNTHETIC world for `package.py content`, so that a CI run with none of the game's data can build a content package the game starts from (planning step 209, PORT-11).

    python tools/ci/make_fixture_world.py OUT_FOLDER
    python tools/package/package.py content --repo OUT_FOLDER/repo --reference OUT_FOLDER/ref --out CONTENT_FOLDER

WHY. The content packager (tools/package/package.py) reads the converter's output and the extracted assets, both the owner's and neither in this repository. The Android
packager and the release packager take its result. A CI job that wants to build an APK, or an archive, that the game can start from (the fixture level) has no such result, so
this writes the miniature of one: the CI fixture level and its tables (tests/fixtures/ci, hand-written), Enhanced's own tables the fixture carries, the port's own 45 tables
(game/data) as the package's port-data, and one INVENTED stub file in each tree the packager requires to be non-empty. Nothing of the original is read or copied, and the
stubs are text that says so (their names end in .bin_png so that no tool takes them for a picture). The result is a package the game starts from with no flag but --level, and
which shows the fixture level and a one-world menu; it is no part of the game and says so in its name.

Standard library only. Writes only under OUT_FOLDER, which must not exist or be empty, and never into this repository's tracked tree.
"""
import argparse
import os
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
FIXTURE = os.path.join(REPO, 'tests', 'fixtures', 'ci')
PORT_DATA = os.path.join(REPO, 'game', 'data')
STUBS = (('repo', ('out', 'assets', 'entities', 'stub_a.bin_png')), ('ref', ('extracted', 'assets', 'sprites', 'stub_b.bin_png')))
STUB_TEXT = b'an invented stub that stands where a picture would be, so that a package of the fixture is not empty; it is not a picture and not from the original'


class WorldError(Exception):
    pass


def put(path, data):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'wb') as f:
        f.write(data)


def inside(path, folder):
    path, folder = os.path.realpath(path), os.path.realpath(folder)
    return path == folder or path.startswith(folder + os.sep)


def build(out, repo=REPO):
    """Writes OUT/repo and OUT/ref and returns the two paths. Raises WorldError for an output folder that is not empty or lies in the repository's tracked sources."""
    out = os.path.abspath(out)
    tracked_roots = [os.path.join(repo, n) for n in ('game', 'tests', 'tools', 'docs', 'data', 'engine', '.github')]
    if any(inside(out, root) for root in tracked_roots) or out == os.path.realpath(repo):
        raise WorldError('%s is inside the repository\'s tracked sources: a world is written under out/ or a temporary folder' % out)
    if os.path.exists(out) and (not os.path.isdir(out) or os.listdir(out)):
        raise WorldError('%s exists and is not an empty folder: nothing is written over it' % out)
    fixture, port_data = os.path.join(repo, 'tests', 'fixtures', 'ci'), os.path.join(repo, 'game', 'data')
    for folder in (os.path.join(fixture, 'levels'), os.path.join(fixture, 'tables'), os.path.join(fixture, 'data'), port_data):
        if not os.path.isdir(folder):
            raise WorldError('%s is missing: this is not a checkout of the repository' % folder)
    world_repo, world_ref = os.path.join(out, 'repo'), os.path.join(out, 'ref')
    shutil.copytree(os.path.join(fixture, 'levels'), os.path.join(world_repo, 'out', 'levels'))
    shutil.copytree(os.path.join(fixture, 'tables'), os.path.join(world_repo, 'out', 'data'))
    shutil.copytree(os.path.join(fixture, 'data'), os.path.join(world_repo, 'data'))
    shutil.copytree(port_data, os.path.join(world_repo, 'game', 'data'))
    for root, parts in STUBS:
        put(os.path.join(world_repo if root == 'repo' else world_ref, *parts), STUB_TEXT)
    return world_repo, world_ref


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    p.add_argument('out')
    args = p.parse_args(argv)
    try:
        world_repo, world_ref = build(args.out)
    except (WorldError, OSError) as e:
        print('make_fixture_world: %s' % e, file=sys.stderr)
        return 2
    print('world: %s (repo %s, ref %s)' % (os.path.abspath(args.out), world_repo, world_ref))
    return 0


if __name__ == '__main__':
    sys.exit(main())
