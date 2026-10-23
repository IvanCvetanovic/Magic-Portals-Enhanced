"""The file set the Magic Portals port opens, worked out from the owner's data at packaging time.

WHY IT IS A MODEL OF THE GAME AND NOT A LIST. The game opens its data by names it finds in tables: the converted levels name their
textures, the committed JSON tables name images, sounds, fonts and entities, the entities name their particle bitmaps. A hand-written list
of those files would be a list of the original's file names kept in this repository, which the hard rule (CLAUDE.md) forbids, and it would
go stale the day a table changes. So the list is DERIVED here, each time, from the tables themselves, by following the chain the game
follows (game/sim/Tiers.cpp, MagicPortalsLayer.cpp's originalAsset/menuImage): a name is looked for in its density tiers first
(`fullhd/`, `hd/`, from game/data/tiers.json), then as itself.

FAIL CLOSED ON CASE. The owner's machine is NTFS and case-insensitive, so a reference whose case differs from the file's loads there and
fails on Linux and Android. Every name is therefore compared with the EXACT names a directory listing returns. A reference that only
matches ignoring case is an error here (CASE_MISS_*), never a quietly dropped file: a package that works on the machine that built it and
nowhere else is the failure this exists to prevent.

THE LIGHTMAPS ARE NOT READ. Since CMP-14 the layer bakes a lit sprite's lightmap in memory; `eth_lightmap` in a level is only the key it
is held under (Lighting.cpp ResPath does not require the file), so the converter's lightmap pictures are never in the plan. Gate G3(c) of
planning step 200 (the 128-level sweep run from the package alone) is what proves that.

This module is pure: it reads the trees it is given and writes nothing. Standard library only.
"""
import collections
import json
import os
import re

# The tiers when game/data/tiers.json cannot say (it always can; this is the default the game itself falls back to).
DEFAULT_TIERS = ('fullhd', 'hd')

NAME_RE = re.compile(r'^(res://)?[A-Za-z0-9_@+\-./ ]+\.(png|bmp|jpg|jpeg|mp3|ogg|wav|fnt|ent|par|dds|tscn)$', re.I)
RES_RE = re.compile(r'res://[^"\s\]]+')
ENTITY_RE = re.compile(r'metadata/entity_name\s*=\s*"([^"]*)"')
BITMAP_RE = re.compile(r'<Bitmap>(.*?)</Bitmap>')
# What an entity file names beyond its particle bitmaps (game/sim/Art.cpp LoadEnt reads <Sprite> and <Normal>, ReadEntLight <HaloBitmap>).
ENT_PICTURE_RE = {
    'sprite': re.compile(r'<Sprite>([^<]+)</Sprite>'),
    'normal': re.compile(r'<Normal>([^<]+)</Normal>'),
    'halo': re.compile(r'<HaloBitmap>([^<]+)</HaloBitmap>'),
}
FONT_PAGE_RE = re.compile(r'page id=\d+ file="([^"]+)"')

OK = 'OK'
ABSENT = 'ABSENT'
CASE_MISS_FATAL = 'CASE_MISS_FATAL'  # Windows finds it, Linux does not
CASE_MISS_TIER = 'CASE_MISS_TIER'    # Windows picks one tier's file, Linux silently falls to a lower tier

# The seven places a reference can resolve in, by the names the plan uses.
ORIGINAL = 'original'          # reference/extracted/assets
OUT_ASSETS = 'out/assets'      # the converter's art
OUT_LEVELS = 'out/levels'
OUT_DATA = 'out/data'          # chapters.json, achievements.json
RAMPAGE = 'magic-rampage'      # reference/magic-rampage (controls/, editor/): optional
PORT_DATA = 'game/data'
DATA = 'data'
TREE_NAMES = (ORIGINAL, OUT_ASSETS, OUT_LEVELS, OUT_DATA, RAMPAGE, PORT_DATA, DATA)


class ClosureError(Exception):
    """The data cannot be packaged as it stands: the message says what and where."""


class Tree:
    """An exact-case index of a directory: every file and directory as a '/'-relative path."""

    def __init__(self, name, root):
        self.name = name
        self.root = root
        self.files = {}      # rel -> size
        self.dirs = set()
        self.symlinks = []   # rel paths (files or directories) that are symbolic links
        self.lower = collections.defaultdict(set)
        self.present = bool(root) and os.path.isdir(root)
        if not self.present:
            return
        if os.path.islink(root):
            self.symlinks.append('.')
        for d, ds, fs in os.walk(root):
            rel = os.path.relpath(d, root).replace(os.sep, '/')
            rel = '' if rel == '.' else rel
            for n in sorted(ds):
                p = rel + '/' + n if rel else n
                self.dirs.add(p)
                self.lower[p.lower()].add(p)
                if os.path.islink(os.path.join(d, n)):
                    self.symlinks.append(p)
            for n in sorted(fs):
                p = rel + '/' + n if rel else n
                full = os.path.join(d, n)
                self.files[p] = os.path.getsize(full)
                self.lower[p.lower()].add(p)
                if os.path.islink(full):
                    self.symlinks.append(p)

    def hit(self, rel):
        """(exact, [names that match ignoring case])"""
        ci = sorted(self.lower.get(rel.lower(), ()))
        return (rel in self.files or rel in self.dirs), ci

    def is_symlinked(self, rel):
        """Whether `rel`, or a folder above it, or the tree's own root, is a symbolic link."""
        if '.' in self.symlinks:
            return True
        parts = rel.split('/')
        marked = set(self.symlinks)
        return any('/'.join(parts[:i]) in marked for i in range(1, len(parts) + 1))

    def abspath(self, rel):
        return os.path.join(self.root, *rel.split('/'))


class Sources:
    """The seven trees the plan reads."""

    def __init__(self, trees):
        self.trees = dict(trees)
        for name in TREE_NAMES:
            self.trees.setdefault(name, Tree(name, ''))

    def __getitem__(self, name):
        return self.trees[name]

    @classmethod
    def from_disk(cls, repo, reference):
        return cls({
            ORIGINAL: Tree(ORIGINAL, os.path.join(reference, 'extracted', 'assets')),
            OUT_ASSETS: Tree(OUT_ASSETS, os.path.join(repo, 'out', 'assets')),
            OUT_LEVELS: Tree(OUT_LEVELS, os.path.join(repo, 'out', 'levels')),
            OUT_DATA: Tree(OUT_DATA, os.path.join(repo, 'out', 'data')),
            RAMPAGE: Tree(RAMPAGE, os.path.join(reference, 'magic-rampage')),
            PORT_DATA: Tree(PORT_DATA, os.path.join(repo, 'game', 'data')),
            DATA: Tree(DATA, os.path.join(repo, 'data')),
        })

    @classmethod
    def from_package(cls, content):
        """The same seven trees, read back from a Packaged() folder (the layout docs/packaging.md describes)."""
        return cls({
            ORIGINAL: Tree(ORIGINAL, os.path.join(content, 'original')),
            OUT_ASSETS: Tree(OUT_ASSETS, os.path.join(content, 'assets')),
            OUT_LEVELS: Tree(OUT_LEVELS, os.path.join(content, 'levels')),
            OUT_DATA: Tree(OUT_DATA, os.path.join(content, 'tables')),
            RAMPAGE: Tree(RAMPAGE, os.path.join(content, 'pad-art')),
            PORT_DATA: Tree(PORT_DATA, os.path.join(content, 'port-data')),
            DATA: Tree(DATA, os.path.join(content, 'data')),
        })


def tier_candidates(rel, tiers):
    d, _, b = rel.rpartition('/')
    out = []
    for t in tiers:
        out.append((d + '/' + t + '/' + b) if d else (t + '/' + b))
    out.append(rel)
    return out


def classify(chain):
    """chain: ordered [(tree, rel)]. Returns (class, windows_pick, linux_pick, detail).

    Windows' pick is the first candidate that exists in ANY case; Linux's the first that exists in EXACT case."""
    win = lin = None
    notes = []
    for tree, rel in chain:
        exact, ci = tree.hit(rel)
        if ci and win is None:
            win = (tree.name, rel, ci)
        if exact and lin is None:
            lin = (tree.name, rel)
        if ci and not exact:
            notes.append('%s:%s exists only as %s' % (tree.name, rel, ci))
    if win is None:
        return ABSENT, None, None, '; '.join(notes)
    if lin is not None and lin[0] == win[0] and lin[1] == win[1]:
        return OK, win, lin, '; '.join(notes)
    if lin is None:
        return CASE_MISS_FATAL, win, None, '; '.join(notes)
    return CASE_MISS_TIER, win, lin, '; '.join(notes)


def walk_json(o, path=''):
    if isinstance(o, dict):
        for k, v in o.items():
            yield from walk_json(v, path + '.' + k)
    elif isinstance(o, list):
        for i, v in enumerate(o):
            yield from walk_json(v, path + '[%d]' % i)
    elif isinstance(o, str):
        yield path, o


def is_note_path(path):
    """Keys that start with '_' are the tables' notes."""
    return any(seg.startswith('_') for seg in re.split(r'[.\[]', path))


def read_json(tree, rel):
    with open(tree.abspath(rel), encoding='utf-8') as f:
        return json.load(f)


def read_text(tree, rel):
    with open(tree.abspath(rel), encoding='utf-8') as f:
        return f.read()


class Ref:
    __slots__ = ('cat', 'src', 'ref', 'chain', 'optional')

    def __init__(self, cat, src, ref, chain, optional=False):
        self.cat, self.src, self.ref, self.chain, self.optional = cat, src, ref, chain, optional


def sprite_groups(src):
    orig, out_assets, ramp = src[ORIGINAL], src[OUT_ASSETS], src[RAMPAGE]
    return [
        ('sprites', [(orig, 'sprites/hd/%s'), (orig, 'sprites/%s')]),
        ('entities', [(orig, 'entities/fullhd/%s'), (orig, 'entities/hd/%s'), (orig, 'entities/%s')]),
        ('particles', [(orig, 'particles/fullhd/%s'), (orig, 'particles/hd/%s'), (orig, 'particles/%s')]),
        ('normalmaps', [(orig, 'entities/normalmaps/hd/%s'), (orig, 'entities/normalmaps/%s')]),
        ('out-entities', [(out_assets, 'entities/fullhd/%s'), (out_assets, 'entities/hd/%s'), (out_assets, 'entities/%s')]),
        ('rampage', [(ramp, 'controls/%s'), (ramp, 'editor/%s')]),
        ('data', [(orig, 'data/%s')]),
    ]


def collect_references(src):
    """Every name the game can open, as Ref rows, from the converted levels and the tables."""
    orig, out_assets, out_levels, out_data = src[ORIGINAL], src[OUT_ASSETS], src[OUT_LEVELS], src[OUT_DATA]
    ramp, port_data, data = src[RAMPAGE], src[PORT_DATA], src[DATA]
    refs = []

    def add(cat, source, ref, chain, optional=False):
        refs.append(Ref(cat, source, ref, chain, optional))

    tiers = list(DEFAULT_TIERS)
    if 'tiers.json' in port_data.files:
        try:
            tiers = list(read_json(port_data, 'tiers.json')['density_tiers']['search'])
        except (KeyError, TypeError, ValueError):
            tiers = list(DEFAULT_TIERS)

    # ---- the converted levels --------------------------------------------------------------------------------------------------
    unique_res = {}
    unique_entities = collections.Counter()
    for lf in sorted(f for f in out_levels.files if f.endswith('.tscn')):
        for line in read_text(out_levels, lf).splitlines():
            for m in RES_RE.finditer(line):
                key = 'ext_resource' if line.startswith('[ext_resource') else (line.split('=')[0].strip() if '=' in line else 'other')
                unique_res.setdefault((key, m.group(0)), set()).add(lf)
            m = ENTITY_RE.search(line)
            if m:
                unique_entities[m.group(1)] += 1
    for (key, r), lvls in sorted(unique_res.items()):
        rest = r[len('res://'):]
        if rest.startswith('assets/levels/'):
            tree_, rel_ = out_levels, rest[len('assets/levels/'):]
        elif rest.startswith('assets/'):
            tree_, rel_ = out_assets, rest[len('assets/'):]
        else:
            tree_, rel_ = out_assets, rest
        chain = [(tree_, c) for c in tier_candidates(rel_, tiers)]
        add('tscn:' + ('eth_lightmap' if key.endswith('eth_lightmap') else key), 'out/levels/* (%d levels)' % len(lvls), r, chain,
            optional=key.endswith('eth_lightmap'))
    for name, n in sorted(unique_entities.items()):
        base = name if name.lower().endswith('.ent') else name + '.ent'
        add('tscn:entity_name', 'out/levels/* (%d nodes)' % n, name, [(orig, 'entities/' + base)], optional=True)

    # ---- chapters.json and achievements.json -----------------------------------------------------------------------------------
    worlds = 0
    if 'chapters.json' in out_data.files:
        ch = read_json(out_data, 'chapters.json')
        worlds = len(ch.get('worlds', []))
        for w in ch.get('worlds', []):
            for lv in w.get('levels', []):
                add('chapters:name', 'out/data/chapters.json', lv['name'], [(out_levels, lv['name'] + '.tscn')])
                add('chapters:scene', 'out/data/chapters.json', lv['scene'], [(out_levels, lv['scene'].split('/')[-1])])
    if 'achievements.json' in out_data.files:
        ach = read_json(out_data, 'achievements.json')
        icons = [e['icon'] for e in ach.get('achievements', []) if 'icon' in e]
        if isinstance(ach.get('secret'), dict) and 'icon' in ach['secret']:
            icons.append(ach['secret']['icon'])
        for ic in sorted(set(icons)):
            d = 'sprites/achievements'
            add('achievements:icon', 'out/data/achievements.json', ic, [(orig, d + '/hd/' + ic), (orig, d + '/' + ic)])

    # ---- the committed tables --------------------------------------------------------------------------------------------------
    for tree in (data, port_data):
        for rel in sorted(tree.files):
            if not rel.endswith('.json'):
                continue
            doc = read_json(tree, rel)
            for path, s in walk_json(doc):
                if is_note_path(path) or not NAME_RE.match(s):
                    continue
                source = '%s/%s%s' % (tree.name, rel, path)
                ext = s.lower().rsplit('.', 1)[1]
                if s.startswith('_'):
                    continue  # a suffix fragment (SetSprite(GetEntityName() + '_blue.png')), not a name
                if s.startswith('res://'):
                    rest = s[len('res://'):]
                    rel_ = rest[len('assets/'):] if rest.startswith('assets/') else rest
                    add('json:res://', source, s, [(out_assets, c) for c in tier_candidates(rel_, tiers)])
                elif '.generated.' in path or path.endswith('options.entry.file'):
                    # drawn by the game at run time when the file is not there; Magic Rampage's art is optional
                    add('json:image(generated at run time)', source, s, [(ramp, 'controls/' + s), (ramp, 'editor/' + s)], optional=True)
                elif ext == 'fnt':
                    add('json:font', source, s, [(orig, 'data/' + s)])
                elif ext in ('mp3', 'ogg', 'wav'):
                    add('json:sound', source, s, [(orig, 'soundfx/' + s)])
                elif ext == 'ent':
                    add('json:entity', source, s, [(orig, 'entities/' + s)])
                elif ext in ('png', 'bmp', 'jpg', 'jpeg', 'dds'):
                    if '/' in s:
                        d, _, b = s.rpartition('/')
                        add('json:image(dir/file)', source, s, [(orig, d + '/hd/' + b), (orig, s)])
                    else:
                        add('json:image(bare)', source, s, ('GROUPS', s))
                else:
                    add('json:other', source, s, [(orig, s)])

    # the world icons: ui.json's icon_prefix + the world's number + .png (Selector.cpp, MagicPortalsLayer.cpp)
    if 'ui.json' in port_data.files:
        prefix = None
        for path, s in walk_json(read_json(port_data, 'ui.json')):
            if path.endswith('icon_prefix'):
                prefix = s
        if prefix is not None:
            for w in range(worlds):
                n = '%s%d.png' % (prefix, w)
                add('code:world_icon', 'ui.json icon_prefix + n', n, ('GROUPS', n))
    return refs


def resolve(src, refs):
    """Each Ref with its class and the file it resolves to. Returns [(Ref, class, win, lin, detail)]."""
    groups = sprite_groups(src)
    rows = []
    for r in refs:
        chain = r.chain
        if isinstance(chain, tuple) and chain and chain[0] == 'GROUPS':
            name = chain[1]
            results = [(g, classify([(t, pat % name) for t, pat in members])) for g, members in groups]
            oks = [x for x in results if x[1][0] == OK]
            if oks:
                res = oks[0][1]
            else:
                nonabs = [x for x in results if x[1][0] != ABSENT]
                res = nonabs[0][1] if nonabs else (ABSENT, None, None, '')
        else:
            res = classify(chain)
        rows.append((r,) + tuple(res))
    return rows


def suffix_fragments(src):
    """The strings the tables spell as the END of a picture's name ('_blue.png'): the game puts an entity's own name in front of one
    (SetSprite(GetEntityName() + '_blue.png')), so the file is named by no table. They are the strings NAME_RE accepts that start with '_'."""
    found = set()
    for tree in (src[DATA], src[PORT_DATA]):
        for rel in sorted(tree.files):
            if not rel.endswith('.json'):
                continue
            for path, s in walk_json(read_json(tree, rel)):
                if not is_note_path(path) and s.startswith('_') and NAME_RE.match(s) and '/' not in s:
                    found.add(s)
    return sorted(found)


def ent_text(tree, rel):
    with open(tree.abspath(rel), 'rb') as f:
        raw = f.read()
    return raw[2:].decode('utf-16-le', 'replace') if raw[:2] == b'\xff\xfe' else raw.decode('utf-8', 'replace')


def kind_of(tree_name, rel, music_files):
    low = rel.lower()
    if rel.startswith('soundfx/'):
        return 'music' if rel.split('/')[-1] in music_files else 'sound'
    if low.endswith('.tscn'):
        return 'level'
    if low.endswith('.fnt'):
        return 'font'
    if low.endswith('.ent'):
        return 'entity-definition (particles)'
    if low.endswith('.json'):
        return 'table'
    if low.endswith('.spv'):
        return 'shader'
    if low.endswith(('.png', '.bmp', '.jpg', '.dds')):
        return 'image'
    return 'other'


class Plan:
    """What the package holds. `required` and `fallback` are {(tree, rel): why}, `optional` the Magic Rampage pictures."""

    def __init__(self):
        self.required = {}
        self.fallback = {}
        self.optional = {}
        self.absent = []      # (category, reference, optional): names that resolve to nothing, kept for the record
        self.problems = []    # (class, category, reference, detail): case misses
        self.pad_refs = []    # (category, reference, optional) that resolve into Magic Rampage's art: absent from a package built without it

    def entries(self, with_pad_art):
        """[(tree, rel, role, why)] in a fixed order."""
        out = [(t, r, 'required', w) for (t, r), w in sorted(self.required.items())]
        out += [(t, r, 'fallback-1x', w) for (t, r), w in sorted(self.fallback.items()) if (t, r) not in self.required]
        if with_pad_art:
            out += [(t, r, 'optional-pad-art', w) for (t, r), w in sorted(self.optional.items())]
        return out


def music_names(src):
    names = set()
    port = src[PORT_DATA]
    if 'sounds.json' in port.files:
        try:
            for _name, t in read_json(port, 'sounds.json').get('music', {}).items():
                if isinstance(t, dict) and 'file' in t:
                    names.add(t['file'])
        except (OSError, ValueError, AttributeError):
            pass
    return names


def build_plan(src, skip_top=('lightmaps',)):
    """The closure. `skip_top` are folders of out/assets the game does not read (the baked lightmaps)."""
    plan = Plan()
    tiers = list(DEFAULT_TIERS)
    if 'tiers.json' in src[PORT_DATA].files:
        try:
            tiers = list(read_json(src[PORT_DATA], 'tiers.json')['density_tiers']['search'])
        except (KeyError, TypeError, ValueError):
            pass
    rows = resolve(src, collect_references(src))
    needed = {}
    why = {}
    for r, cls, win, lin, detail in rows:
        if cls in (CASE_MISS_FATAL, CASE_MISS_TIER):
            plan.problems.append((cls, r.cat, r.ref, detail or '%s -> %s' % (win, lin)))
            continue
        if cls == ABSENT:
            plan.absent.append((r.cat, r.ref, r.optional))
            continue
        tname, rel = win[0], win[1]
        if tname == RAMPAGE:
            plan.pad_refs.append((r.cat, r.ref, r.optional))
        if rel in src[tname].files:
            needed[(tname, rel)] = src[tname].files[rel]
            why.setdefault((tname, rel), '%s: %s <- %s' % (r.cat, r.ref, r.src))

    # every .ent a level or a table names is opened for its particle systems: their <Bitmap> names are particles/<name>, tier-searched
    for (tname, rel) in sorted(needed):
        if not rel.endswith('.ent'):
            continue
        txt = ent_text(src[tname], rel)
        for b in BITMAP_RE.findall(txt):
            chain = [(src[ORIGINAL], 'particles/%s/%s' % (t, b)) for t in tiers] + [(src[ORIGINAL], 'particles/' + b)]
            cls, win, lin, detail = classify(chain)
            if cls in (CASE_MISS_FATAL, CASE_MISS_TIER):
                plan.problems.append((cls, 'particle bitmap', '%s of %s:%s' % (b, tname, rel), detail))
            elif cls == ABSENT:
                plan.absent.append(('particle bitmap', '%s of %s:%s' % (b, tname, rel), False))
            else:
                needed[(win[0], win[1])] = src[win[0]].files[win[1]]
                why.setdefault((win[0], win[1]), 'particle bitmap <Bitmap>%s</Bitmap> of %s:%s' % (b, tname, rel))

    # an entity file of the original also names its own picture, its normal map and its halo: the game opens them wherever it draws the
    # entity from the file (the black bars, a shooter's bolt, a lit torch). Looked for where Art.cpp and the layer look: entities/ in its
    # density tiers, normal maps in entities/normalmaps/ in theirs.
    orig = src[ORIGINAL]
    for (tname, rel) in sorted(k for k in needed if k[0] == ORIGINAL and k[1].endswith('.ent')):
        txt = ent_text(orig, rel)
        for kind, pattern in sorted(ENT_PICTURE_RE.items()):
            for name in pattern.findall(txt):
                name = name.strip()
                base = 'entities/normalmaps/' + name if kind == 'normal' else 'entities/' + name
                cls, win, lin, detail = classify([(orig, c) for c in tier_candidates(base, tiers)])
                if cls == ABSENT and kind != 'normal':
                    # a halo may be a menu picture: any group the game reads pictures from
                    for _g, members in sprite_groups(src):
                        cls, win, lin, detail = classify([(t, pat % name) for t, pat in members])
                        if cls != ABSENT:
                            break
                if cls in (CASE_MISS_FATAL, CASE_MISS_TIER):
                    plan.problems.append((cls, 'entity ' + kind, '%s of %s' % (name, rel), detail))
                elif cls == ABSENT:
                    plan.absent.append(('entity ' + kind, '%s of %s' % (name, rel), True))
                else:
                    needed[(win[0], win[1])] = src[win[0]].files[win[1]]
                    why.setdefault((win[0], win[1]), 'the %s of %s:%s' % (kind, tname, rel))

    # the colour variants: an entity's own name in front of a suffix a table spells ('lamp_a' + '_green.png'), in the entity tiers
    fragments = suffix_fragments(src)
    for (tname, rel) in sorted(k for k in needed if k[0] == ORIGINAL and k[1].startswith('entities/') and k[1].endswith('.ent')):
        stem = rel[len('entities/'):-len('.ent')]
        for fragment in fragments:
            name = stem + fragment
            cls, win, lin, detail = classify([(orig, c) for c in tier_candidates('entities/' + name, tiers)])
            if cls in (CASE_MISS_FATAL, CASE_MISS_TIER):
                plan.problems.append((cls, 'colour variant', '%s (%s + %s)' % (name, stem, fragment), detail))
            elif cls == OK:
                needed[(win[0], win[1])] = src[win[0]].files[win[1]]
                why.setdefault((win[0], win[1]), 'the colour variant %s of %s:%s' % (fragment, tname, rel))

    # the fonts: the pages of every .fnt a table names (the game opens a font's pages when it loads it). The pages of a .fnt that nothing
    # names are not opened, and the .fnt itself is not packaged either.
    for f in sorted(rel for (tname, rel) in needed if tname == ORIGINAL and rel.startswith('data/') and rel.endswith('.fnt')):
        with open(orig.abspath(f), encoding='latin-1') as fh:
            text = fh.read()
        for page in FONT_PAGE_RE.findall(text):
            cls, win, lin, detail = classify([(orig, 'data/' + page)])
            if cls == OK:
                needed[(ORIGINAL, 'data/' + page)] = orig.files['data/' + page]
                why.setdefault((ORIGINAL, 'data/' + page), 'page of ' + f)
            elif cls == ABSENT:
                plan.absent.append(('font page', '%s of %s' % (page, f), False))
            else:
                plan.problems.append((cls, 'font page', '%s of %s' % (page, f), detail))

    # all the levels, the two tables the converter writes beside them, and the committed tables
    for rel, size in src[OUT_LEVELS].files.items():
        needed[(OUT_LEVELS, rel)] = size
        why.setdefault((OUT_LEVELS, rel), 'a level (chapters.json)')
    for rel in ('chapters.json', 'achievements.json'):
        if rel in src[OUT_DATA].files:
            needed[(OUT_DATA, rel)] = src[OUT_DATA].files[rel]
            why.setdefault((OUT_DATA, rel), 'the level order / achievements table')
    for rel, size in src[DATA].files.items():
        needed[(DATA, rel)] = size
        why.setdefault((DATA, rel), 'Enhanced table (Game::LoadData)')
    for rel, size in src[PORT_DATA].files.items():
        needed[(PORT_DATA, rel)] = size
        why.setdefault((PORT_DATA, rel), 'the port table (portData)')

    # the 1x file under a picked tier twin: what Tiers::Resolve falls back to
    fallback = {}
    whyf = {}
    for (tname, rel) in sorted(needed):
        parts = rel.split('/')
        for tier in tiers:
            if tier in parts[:-1]:
                one = '/'.join(p for p in parts if p != tier)
                if one in src[tname].files:
                    fallback[(tname, one)] = src[tname].files[one]
                    whyf.setdefault((tname, one), 'the 1x file under the picked tier twin %s' % rel)

    def skipped(tname, rel):
        return tname == OUT_ASSETS and rel.split('/')[0] in skip_top

    for key, size in needed.items():
        if skipped(*key):
            continue
        if key[0] == RAMPAGE:
            plan.optional[key] = why.get(key, '')
        else:
            plan.required[key] = why.get(key, '')
    for key in fallback:
        if skipped(*key) or key in needed:
            continue
        plan.fallback[key] = whyf.get(key, '')
    plan.sizes = {}
    for d in (plan.required, plan.fallback, plan.optional):
        for (tname, rel) in d:
            plan.sizes[(tname, rel)] = src[tname].files[rel]
    music = music_names(src)
    plan.kinds = {k: kind_of(k[0], k[1], music) for k in plan.sizes}
    plan.absent.sort()
    plan.problems.sort()
    return plan
