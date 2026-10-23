#!/usr/bin/env python3
"""The content packager: builds the folder Magic Portals Enhanced reads its data from at run time (planning step 200, PORT-2).

    package.py content --repo <repo> --reference <reference dir> [--out <dir>] [--with-pad-art]
    package.py shell   --content <content dir> --exe <program> --out <dir> [--shaders <dir>] [--repo <repo>]
    package.py verify  <content dir>
    package.py same    <dir> <dir>

WHAT IT IS FOR. The game finds its data at run time (game/sim/AssetRoot.hpp). A released game ships the original's art and sound, and the
owner builds that package on his own machine, from his own data: this tool reads his converted levels (<repo>/out) and the original's
extracted assets (<reference>/extracted/assets), and writes the folder the game reads. CI never holds the data; nothing this tool writes
goes into the repository: it refuses an output folder inside the tracked tree (the default is <repo>/out/dist/content, which is under
the gitignored out/).

WHAT IT PACKAGES is derived from the owner's data every time (closure.py): the converted levels, the two tables beside them, the
committed tables, and exactly the files those name, followed through the tiers the game follows. It names no file of the original; a name
in this repository's tracked files would be the hard rule broken (CLAUDE.md). The lightmaps the converter writes are NOT packaged: the
layer bakes them in memory and never opens them.

WHAT IT REFUSES, each with a message that names the cause:
  - an output folder inside the tracked tree, inside a source tree, or on top of one;
  - an output folder that already holds something this tool did not write (the folder is replaced only when it holds our manifest);
  - a symbolic link among the sources it would copy;
  - a reference that matches only ignoring case (the package would work here and fail on Linux and Android);
  - anything on the never-package list (the original's scenes, .pvr/.pkm/.par/.enml, ETHFramework, the APK, the lightmaps, the Android bundle (a .bin),
    .git, build trees...), even if a table were to name it.

THE LAYOUT (docs/packaging.md; game/sim/AssetRoot.cpp Layout::Packaged is the same list):
    levels/            the converted levels             (<repo>/out/levels)
    assets/            what res:// stands for           (<repo>/out/assets: entities/ only; lightmaps/ is left out)
    tables/            chapters.json, achievements.json (<repo>/out/data)
    data/              Enhanced's tables                (<repo>/data)
    port-data/         the port's own tables            (<repo>/game/data)
    original/          the original's files the game opens (<reference>/extracted/assets: sprites, entities, particles, data, soundfx)
    pad-art/           Magic Rampage's control pictures, ONLY with --with-pad-art (<reference>/magic-rampage: controls/, editor/)
    manifest.json      every file with its size, sha256 and kind, and the references that name nothing
    SHA256SUMS.txt     the same hashes in `sha256sum -c` form
`shell` lays out a runnable desktop folder: the program, assets/shaders/*.spv beside it (the engine opens its 17 shaders by relative path)
and content/ -- it only proves the layout; the release archives are a later step.

Deterministic: the same input gives a byte-identical folder (sorted order, fixed times, no clock in the manifest). Standard library only.
"""
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import uuid

import closure as cl

MANIFEST = 'manifest.json'
SUMS = 'SHA256SUMS.txt'
TOOL = 'tools/package/package.py'
FIXED_TIME = 946684800  # 2000-01-01T00:00:00Z: every file and folder gets it, so two runs are the same bytes AND the same times

# Where each source tree lands in the package (the Packaged() layout).
DEST = {
    cl.ORIGINAL: 'original',
    cl.OUT_ASSETS: 'assets',
    cl.OUT_LEVELS: 'levels',
    cl.OUT_DATA: 'tables',
    cl.RAMPAGE: 'pad-art',
    cl.PORT_DATA: 'port-data',
    cl.DATA: 'data',
}

# THE NEVER-PACKAGE LIST, by kind and not by file name: a tracked file may not hold the original's names.
NEVER_FOLDERS = ('scenes', 'ETHFramework', 'effects', 'lightmaps', 'reference', 'node_modules')
NEVER_SEGMENTS = ('.git', '__pycache__')
NEVER_EXTENSIONS = ('.pvr', '.pkm', '.par', '.enml', '.apk', '.exe', '.dll', '.so', '.dylib', '.bin', '.list', '.pspimage', '.as')

REQUIRED_TREES = (cl.OUT_LEVELS, cl.OUT_DATA, cl.OUT_ASSETS, cl.ORIGINAL, cl.DATA, cl.PORT_DATA)


class PackageError(Exception):
    pass


# ---- small helpers -------------------------------------------------------------------------------------------------------------

def sha256_of(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for block in iter(lambda: f.read(1 << 20), b''):
            h.update(block)
    return h.hexdigest()


def real(path):
    return os.path.normcase(os.path.realpath(path))


def inside(path, folder):
    """Whether `path` is `folder` or below it (both already real paths)."""
    a, b = path.rstrip(os.sep) + os.sep, folder.rstrip(os.sep) + os.sep
    return a.startswith(b)


def never_reason(rel):
    """Why `rel` (a path inside a source tree) must not be packaged, or None."""
    parts = rel.replace('\\', '/').split('/')
    for seg in parts[:-1]:
        if seg in NEVER_FOLDERS or seg in NEVER_SEGMENTS or seg.startswith('build'):
            return 'a file under %s/' % seg
    low = parts[-1].lower()
    for ext in NEVER_EXTENSIONS:
        if low.endswith(ext):
            return 'a %s file' % ext
    if parts[-1] in NEVER_SEGMENTS:
        return 'a %s entry' % parts[-1]
    return None


def check_output(out, repo, reference):
    """Refuse an output folder that would put the package where it must not go. Real paths, so a link cannot hide the answer."""
    out_r, repo_r, ref_r = real(out), real(repo), real(reference)
    allowed_root = real(os.path.join(repo, 'out'))
    if inside(out_r, repo_r):
        if not inside(out_r, allowed_root) or out_r == allowed_root:
            raise PackageError('%s is inside the repository %s but not under its gitignored out/: the package must never be written '
                               'into the tracked tree (CLAUDE.md, the hard rule)' % (out, repo))
        for name in ('levels', 'data', 'assets'):
            tree = real(os.path.join(repo, 'out', name))
            if inside(out_r, tree) or inside(tree, out_r):
                raise PackageError('%s overlaps the converter output %s' % (out, tree))
        # best effort: the tracked tree is git's to say
        try:
            tracked = subprocess.run(['git', '-C', repo, 'ls-files', '--', out], capture_output=True, text=True, timeout=60)
            if tracked.returncode == 0 and tracked.stdout.strip():
                raise PackageError('%s holds files git tracks: %s' % (out, tracked.stdout.split()[0]))
        except (OSError, subprocess.SubprocessError):
            pass
    if inside(out_r, ref_r) or inside(ref_r, out_r):
        raise PackageError('%s overlaps the reference folder %s: the package would be written into (or over) the originals' % (out, reference))
    for name in ('data', os.path.join('game', 'data'), 'engine', 'tools', 'tests', 'docs'):
        tree = real(os.path.join(repo, name))
        if inside(out_r, tree) or inside(tree, out_r):
            raise PackageError('%s overlaps the source folder %s' % (out, tree))


def prepare_destination(out):
    """Where the package is built: a sibling staging folder; `out` itself is replaced only when it holds a manifest this tool wrote."""
    if os.path.lexists(out):
        if not os.path.isdir(out) or os.path.islink(out):
            raise PackageError('%s exists and is not a plain folder' % out)
        entries = os.listdir(out)
        if entries:
            try:
                with open(os.path.join(out, MANIFEST), encoding='utf-8') as f:
                    ours = json.load(f).get('tool') == TOOL
            except (OSError, ValueError):
                ours = False
            if not ours:
                raise PackageError('%s is not empty and holds no %s written by this tool: it is not replaced' % (out, MANIFEST))
    stage = '%s.staging-%s' % (out, uuid.uuid4().hex[:8])
    os.makedirs(stage)
    return stage


def finish_destination(stage, out):
    if os.path.lexists(out):
        shutil.rmtree(out)
    os.rename(stage, out)


def stamp_tree(root):
    """The fixed time on every file and folder, deepest first."""
    for d, ds, fs in os.walk(root, topdown=False):
        for n in fs:
            os.utime(os.path.join(d, n), (FIXED_TIME, FIXED_TIME))
        os.utime(d, (FIXED_TIME, FIXED_TIME))


def write_text(path, text):
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(text)


def listing(root):
    """[(relpath, size)] of every file under root, sorted, '/'-separated."""
    out = []
    for d, ds, fs in os.walk(root):
        for n in fs:
            full = os.path.join(d, n)
            out.append((os.path.relpath(full, root).replace(os.sep, '/'), os.path.getsize(full)))
    return sorted(out)


# ---- content ---------------------------------------------------------------------------------------------------------------------

def plan_content(repo, reference, with_pad_art):
    src = cl.Sources.from_disk(repo, reference)
    missing = [name for name in REQUIRED_TREES if not src[name].present]
    if missing:
        raise PackageError('these source folders are not there: %s (the converter writes <repo>/out; the original is extracted into '
                           '<reference>/extracted/assets)' % ', '.join('%s (%s)' % (n, src[n].root) for n in missing))
    for n in REQUIRED_TREES:
        if not src[n].files:
            raise PackageError('the source folder %s is empty: %s' % (n, src[n].root))
    plan = cl.build_plan(src)
    if plan.problems:
        lines = ['%s %s %r %s' % p for p in plan.problems[:20]]
        raise PackageError('%d reference(s) match a file only ignoring case; the package would fail on Linux and Android. Fix the name '
                           'or the file:\n  %s' % (len(plan.problems), '\n  '.join(lines)))
    entries = plan.entries(with_pad_art)
    for tname, rel, role, why in entries:
        reason = never_reason(rel)
        if reason:
            raise PackageError('%s:%s is %s -- on the never-package list (named by: %s)' % (tname, rel, reason, why))
        if src[tname].is_symlinked(rel):
            raise PackageError('%s:%s is, or lies under, a symbolic link: a package is built only from plain files (%s)' %
                               (tname, rel, src[tname].abspath(rel)))
    return src, plan, entries


def dest_of(tname, rel):
    return DEST[tname] + '/' + rel


def build_manifest(plan, entries, hashes, with_pad_art):
    files = []
    for tname, rel, role, why in entries:
        path = dest_of(tname, rel)
        files.append({'path': path, 'bytes': plan.sizes[(tname, rel)], 'sha256': hashes[path], 'kind': plan.kinds[(tname, rel)], 'role': role})
    files.sort(key=lambda e: e['path'])
    manifest = {
        'format': 1,
        'tool': TOOL,
        'layout': 'packaged',
        'pad_art': bool(with_pad_art),
        'totals': {'files': len(files), 'bytes': sum(e['bytes'] for e in files)},
        'files': files,
        # names the data does not hold: markers and waypoints with no entity file, pictures the game draws itself, a lightmap key
        'absent': sorted([c, r, bool(o)] for c, r, o in (plan.absent + ([] if with_pad_art else plan.pad_refs))),
    }
    return manifest


def write_manifest(root, manifest):
    write_text(os.path.join(root, MANIFEST), json.dumps(manifest, indent=1, sort_keys=True) + '\n')
    write_text(os.path.join(root, SUMS), ''.join('%s  %s\n' % (e['sha256'], e['path']) for e in manifest['files']))


def cmd_content(args):
    repo = os.path.abspath(args.repo)
    reference = os.path.abspath(args.reference)
    out = os.path.abspath(args.out) if args.out else os.path.join(repo, 'out', 'dist', 'content')
    check_output(out, repo, reference)
    src, plan, entries = plan_content(repo, reference, args.with_pad_art)
    stage = prepare_destination(out)
    try:
        hashes = {}
        for tname, rel, role, why in entries:
            dest = os.path.join(stage, *dest_of(tname, rel).split('/'))
            os.makedirs(os.path.dirname(dest), exist_ok=True)
            shutil.copyfile(src[tname].abspath(rel), dest)
            hashes[dest_of(tname, rel)] = sha256_of(dest)
        manifest = build_manifest(plan, entries, hashes, args.with_pad_art)
        write_manifest(stage, manifest)
        if not args.no_verify:
            report = verify_content(stage)
            if report['problems']:
                raise PackageError('the package fails its own check:\n  ' + '\n  '.join(report['problems'][:20]))
        stamp_tree(stage)
        finish_destination(stage, out)
    except BaseException:
        shutil.rmtree(stage, ignore_errors=True)
        raise
    t = manifest['totals']
    by_role = {}
    for e in manifest['files']:
        by_role.setdefault(e['role'], [0, 0])
        by_role[e['role']][0] += 1
        by_role[e['role']][1] += e['bytes']
    print('package: %s' % out)
    print('files %d, bytes %d (%.2f MB)' % (t['files'], t['bytes'], t['bytes'] / 1e6))
    for role in sorted(by_role):
        print('  %-18s %5d files %10d bytes' % (role, by_role[role][0], by_role[role][1]))
    print('references that name nothing in the data: %d (see %s)' % (len(manifest['absent']), MANIFEST))
    return 0


# ---- verify ----------------------------------------------------------------------------------------------------------------------

def verify_content(content):
    """Re-check a package against its own manifest and against the model of what the game opens. Returns {'problems': [...], 'summary': {...}}."""
    problems = []
    summary = {}
    try:
        with open(os.path.join(content, MANIFEST), encoding='utf-8') as f:
            manifest = json.load(f)
    except (OSError, ValueError) as e:
        return {'problems': ['%s cannot be read: %s' % (MANIFEST, e)], 'summary': summary}
    if manifest.get('tool') != TOOL or manifest.get('format') != 1:
        problems.append('%s was not written by this tool (tool=%r format=%r)' % (MANIFEST, manifest.get('tool'), manifest.get('format')))
    listed = {e['path']: e for e in manifest.get('files', [])}
    on_disk = {rel: size for rel, size in listing(content) if rel not in (MANIFEST, SUMS)}
    for path in sorted(set(listed) - set(on_disk)):
        problems.append('listed but missing: ' + path)
    for path in sorted(set(on_disk) - set(listed)):
        problems.append('on disk but not in the manifest: ' + path)
    bad_hash = 0
    for path in sorted(set(listed) & set(on_disk)):
        e = listed[path]
        full = os.path.join(content, *path.split('/'))
        if on_disk[path] != e['bytes']:
            problems.append('size differs: %s (%d, manifest %d)' % (path, on_disk[path], e['bytes']))
        elif sha256_of(full) != e['sha256']:
            bad_hash += 1
            problems.append('sha256 differs: ' + path)
        if os.path.islink(full):
            problems.append('a symbolic link: ' + path)
    for path in sorted(on_disk):
        top = path.split('/')[0]
        if top not in DEST.values():
            problems.append('outside the layout: ' + path)
        tname = next((n for n, d in DEST.items() if d == top), None)
        reason = never_reason(path[len(top) + 1:]) if tname else None
        if reason:
            problems.append('never-package: %s is %s' % (path, reason))
    try:
        with open(os.path.join(content, SUMS), encoding='utf-8') as f:
            sums = dict((line.split('  ', 1)[1].rstrip('\n'), line.split('  ', 1)[0]) for line in f if '  ' in line)
        if sums != {p: e['sha256'] for p, e in listed.items()}:
            problems.append('%s does not match %s' % (SUMS, MANIFEST))
    except OSError as e:
        problems.append('%s cannot be read: %s' % (SUMS, e))

    # the model, run on the package: every name the game can open resolves, exactly, inside it
    src = cl.Sources.from_package(content)
    refs = cl.collect_references(src)
    rows = cl.resolve(src, refs)
    classes = {}
    for r, cls, win, lin, detail in rows:
        classes[cls] = classes.get(cls, 0) + 1
        if cls in (cl.CASE_MISS_FATAL, cl.CASE_MISS_TIER):
            problems.append('%s: %s (%s)' % (cls, r.ref, r.cat))
    # A lightmap key names a file the game does not open (it bakes the picture in memory): all of them must be absent here.
    lightmap_keys = [r for r, cls, *_ in rows if r.cat == 'tscn:eth_lightmap']
    present_lightmaps = [r.ref for r, cls, *_ in rows if r.cat == 'tscn:eth_lightmap' and cls != cl.ABSENT]
    if present_lightmaps:
        problems.append('a lightmap is in the package: ' + present_lightmaps[0])
    absent_now = sorted([r.cat, r.ref, bool(r.optional)] for r, cls, *_ in rows if cls == cl.ABSENT and r.cat != 'tscn:eth_lightmap')
    absent_then = sorted(a for a in manifest.get('absent', []) if a[0] != 'tscn:eth_lightmap')
    # the pad art is optional; a reference to it is absent when the package was built without it
    if absent_now != absent_then:
        extra = [a for a in absent_now if a not in absent_then][:3]
        gone = [a for a in absent_then if a not in absent_now][:3]
        problems.append('the references that name nothing differ from the source run: now-only %s, then-only %s' % (extra, gone))
    # and the plan the model draws from the package is the package: nothing in it is surplus, nothing the game names is missing
    plan = cl.build_plan(src)
    planned = {dest_of(t, r) for (t, r) in list(plan.required) + list(plan.fallback)}
    if manifest.get('pad_art'):
        planned |= {dest_of(t, r) for (t, r) in plan.optional}
    for path in sorted(set(on_disk) - planned):
        problems.append('in the package but not needed by anything the model follows: ' + path)
    summary = {
        'references': len(rows),
        'classes': classes,
        'lightmap_keys_absent': len(lightmap_keys) - len(present_lightmaps),
        'absent_other': len(absent_now),
        'files': len(on_disk),
        'bytes': sum(on_disk.values()),
        'bad_hashes': bad_hash,
    }
    return {'problems': problems, 'summary': summary}


def cmd_verify(args):
    report = verify_content(os.path.abspath(args.content))
    print(json.dumps(report['summary'], indent=1, sort_keys=True))
    for p in report['problems']:
        print('PROBLEM: ' + p)
    print('verify: %s' % ('FAILED, %d problem(s)' % len(report['problems']) if report['problems'] else 'ok'))
    return 1 if report['problems'] else 0


# ---- shell -----------------------------------------------------------------------------------------------------------------------

def cmd_shell(args):
    content = os.path.abspath(args.content)
    exe = os.path.abspath(args.exe)
    repo = os.path.abspath(args.repo) if args.repo else None
    shaders = os.path.abspath(args.shaders) if args.shaders else (os.path.join(repo, 'engine', 'assets', 'shaders') if repo else None)
    out = os.path.abspath(args.out)
    if shaders is None:
        raise PackageError('give --shaders <folder of .spv> or --repo <repo> (engine/assets/shaders)')
    if not os.path.isfile(exe) or os.path.islink(exe):
        raise PackageError('--exe %s is not a plain file' % exe)
    spv = sorted(n for n in os.listdir(shaders) if n.endswith('.spv'))
    if not spv:
        raise PackageError('no .spv in %s' % shaders)
    for n in spv:
        if os.path.islink(os.path.join(shaders, n)):
            raise PackageError('%s is a symbolic link' % os.path.join(shaders, n))
    if repo:
        # the shell is a runnable copy for testing: outside the repository, or under its gitignored out/
        reference = os.path.join(repo, 'reference')
        check_output(out, repo, reference)
    if inside(real(out), real(content)) or inside(real(content), real(out)):
        raise PackageError('--out overlaps --content')
    report = verify_content(content)
    if report['problems']:
        raise PackageError('the content folder fails its check, so it is not laid out:\n  ' + '\n  '.join(report['problems'][:10]))
    stage = prepare_destination(out)
    try:
        shutil.copy2(exe, os.path.join(stage, os.path.basename(exe)))
        os.makedirs(os.path.join(stage, 'assets', 'shaders'))
        for n in spv:
            shutil.copyfile(os.path.join(shaders, n), os.path.join(stage, 'assets', 'shaders', n))
        shutil.copytree(content, os.path.join(stage, 'content'))
        entries = []
        for rel, size in listing(stage):
            if rel not in (MANIFEST, SUMS):
                entries.append({'path': rel, 'bytes': size, 'sha256': sha256_of(os.path.join(stage, *rel.split('/')))})
        write_text(os.path.join(stage, 'shell.json'), json.dumps(
            {'format': 1, 'tool': TOOL, 'shell': True, 'exe': os.path.basename(exe), 'shaders': len(spv), 'files': entries},
            indent=1, sort_keys=True) + '\n')
        stamp_tree(os.path.join(stage, 'content'))
        finish_destination(stage, out)
    except BaseException:
        shutil.rmtree(stage, ignore_errors=True)
        raise
    print('shell: %s (%s, %d shaders, content %d files)' % (out, os.path.basename(exe), len(spv), report['summary']['files']))
    return 0


# ---- same ------------------------------------------------------------------------------------------------------------------------

def cmd_same(args):
    a, b = os.path.abspath(args.a), os.path.abspath(args.b)
    la, lb = dict(listing(a)), dict(listing(b))
    diffs = []
    for p in sorted(set(la) | set(lb)):
        if p not in la or p not in lb:
            diffs.append('only in %s: %s' % ('b' if p not in la else 'a', p))
        elif la[p] != lb[p] or sha256_of(os.path.join(a, *p.split('/'))) != sha256_of(os.path.join(b, *p.split('/'))):
            diffs.append('differs: ' + p)
    print('%d files in a, %d in b, %d differences' % (len(la), len(lb), len(diffs)))
    for d in diffs[:20]:
        print('  ' + d)
    return 1 if diffs else 0


def main(argv=None):
    ap = argparse.ArgumentParser(description='Build the Magic Portals content package from the owner\'s data (see the module text).')
    sub = ap.add_subparsers(dest='command', required=True)
    c = sub.add_parser('content', help='derive the closure from the data and write the package')
    c.add_argument('--repo', required=True, help='this repository (reads data/, game/data/, out/levels, out/data, out/assets)')
    c.add_argument('--reference', required=True, help='the reference folder (reads extracted/assets and magic-rampage/)')
    c.add_argument('--out', help='the output folder (default <repo>/out/dist/content)')
    c.add_argument('--with-pad-art', action='store_true', help="also package Magic Rampage's control pictures (optional; the game draws its own without)")
    c.add_argument('--no-verify', action='store_true', help='skip the self-check of the finished package')
    c.set_defaults(func=cmd_content)
    s = sub.add_parser('shell', help='lay out program + assets/shaders + content/ (proves the layout)')
    s.add_argument('--content', required=True)
    s.add_argument('--exe', required=True)
    s.add_argument('--out', required=True)
    s.add_argument('--shaders', help="folder of the engine's .spv (default <repo>/engine/assets/shaders)")
    s.add_argument('--repo', help='this repository (for the shaders and the output-location check)')
    s.set_defaults(func=cmd_shell)
    v = sub.add_parser('verify', help='check a package against its manifest and the model of what the game opens')
    v.add_argument('content')
    v.set_defaults(func=cmd_verify)
    m = sub.add_parser('same', help='compare two folders byte for byte')
    m.add_argument('a')
    m.add_argument('b')
    m.set_defaults(func=cmd_same)
    args = ap.parse_args(argv)
    try:
        return args.func(args)
    except (PackageError, cl.ClosureError) as e:
        print('package.py: %s' % e, file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
