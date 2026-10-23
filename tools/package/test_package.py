"""Unit tests of the content packager, on a SYNTHETIC miniature tree the tests build themselves.

Run:   python -m unittest discover -s tools/package -p "test_*.py" -v        (standard library only)

Every byte and every name below is invented: no file of the original, no name from it, nothing copied. The hard rule (CLAUDE.md) forbids
the original's names in tracked files, so the fixture's art is `tile_a.png`, `boom_a.mp3` and the like, with bytes such as b"tile-a".
The formats are the real ones where the packager reads them (a UTF-16LE `.ent` with <Bitmap>, `res://` lines in a scene, BMFont pages,
density tiers) so that what is tested is the code that reads the real data.
"""
import contextlib
import io
import json
import os
import shutil
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import closure as cl  # noqa: E402
import package as pk  # noqa: E402


def put(path, data):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    mode = 'wb' if isinstance(data, bytes) else 'w'
    with open(path, mode, **({} if isinstance(data, bytes) else {'encoding': 'utf-8', 'newline': '\n'})) as f:
        f.write(data)


def put_json(path, doc):
    put(path, json.dumps(doc, indent=1))


def ent(bitmaps):
    """A UTF-16LE .ent with a BOM, as the engine's entity files are, naming particle bitmaps."""
    text = '<Entity>' + ''.join('<ParticleSystem><Bitmap>%s</Bitmap></ParticleSystem>' % b for b in bitmaps) + '</Entity>'
    return b'\xff\xfe' + text.encode('utf-16-le')


def ent_pictures(sprite, normal, halo):
    """A UTF-16LE .ent that names its own picture (<Sprite>), its normal map (<Normal>) and its light's halo (<HaloBitmap>)."""
    text = ('<?xml version="1.0" ?><Ethanon><Entity type="1"><Sprite>%s</Sprite><Normal>%s</Normal><Light><HaloBitmap>%s</HaloBitmap></Light>'
            '<Particles /></Entity></Ethanon>' % (sprite, normal, halo))
    return b'\xff\xfe' + text.encode('utf-16-le')


FNT = 'info face="x"\ncommon lineHeight=10\npage id=0 file="%s"\n'


def build_world(root):
    """repo/ and reference/ with every kind of reference the packager follows, and files it must leave out."""
    repo = os.path.join(root, 'repo')
    ref = os.path.join(root, 'reference')
    a = os.path.join(ref, 'extracted', 'assets')
    # ---- the original's extracted tree
    put(os.path.join(a, 'sprites', 'hd', 'tile_a.png'), b'tile-a-hd')
    put(os.path.join(a, 'sprites', 'tile_a.png'), b'tile-a-1x')
    put(os.path.join(a, 'sprites', 'hd', 'menu_a.png'), b'menu-a-hd')
    put(os.path.join(a, 'sprites', 'menu_a.png'), b'menu-a-1x')
    put(os.path.join(a, 'sprites', 'achievements', 'ach_a.png'), b'ach-a')
    put(os.path.join(a, 'sprites', 'achievements', 'hd', 'ach_s.png'), b'ach-s-hd')
    put(os.path.join(a, 'sprites', 'globe_0.png'), b'world-0')
    put(os.path.join(a, 'sprites', 'unused_a.png'), b'unused')
    put(os.path.join(a, 'entities', 'thing_a.ent'), ent(['spark_a.png']))
    put(os.path.join(a, 'entities', 'thing_unused.ent'), ent(['spark_unused.png']))
    put(os.path.join(a, 'entities', 'item_a.png'), b'item-a-1x')
    put(os.path.join(a, 'entities', 'hd', 'item_a.png'), b'item-a-hd')
    # an entity file that names its own picture, a normal map and a halo; its colour variants are named by no table, only by a suffix
    put(os.path.join(a, 'entities', 'door_a.ent'), ent_pictures(sprite='door_a.png', normal='door_a_n.png', halo='glow_a.png'))
    put(os.path.join(a, 'entities', 'door_a.png'), b'door-a-1x')
    put(os.path.join(a, 'entities', 'hd', 'door_a.png'), b'door-a-hd')
    put(os.path.join(a, 'entities', 'door_a_blue.png'), b'door-a-blue-1x')
    put(os.path.join(a, 'entities', 'hd', 'door_a_blue.png'), b'door-a-blue-hd')
    put(os.path.join(a, 'entities', 'door_a_green.png'), b'a colour no table spells')
    put(os.path.join(a, 'entities', 'door_unused_blue.png'), b'a variant of an entity nothing names')
    put(os.path.join(a, 'entities', 'normalmaps', 'door_a_n.png'), b'door-a-normal')
    put(os.path.join(a, 'entities', 'glow_a.png'), b'glow-a')
    put(os.path.join(a, 'particles', 'hd', 'spark_a.png'), b'spark-a-hd')
    put(os.path.join(a, 'particles', 'spark_a.png'), b'spark-a-1x')
    put(os.path.join(a, 'particles', 'spark_unused.png'), b'spark-unused')
    put(os.path.join(a, 'data', 'face_a.fnt'), FNT % 'face_a_0.png')
    put(os.path.join(a, 'data', 'face_a_0.png'), b'face-a-page')
    put(os.path.join(a, 'data', 'face_b.fnt'), FNT % 'face_b_0.png')  # no table names it
    put(os.path.join(a, 'data', 'face_b_0.png'), b'face-b-page')
    put(os.path.join(a, 'soundfx', 'boom_a.mp3'), b'boom-a')
    put(os.path.join(a, 'soundfx', 'tune_a.mp3'), b'tune-a')
    put(os.path.join(a, 'soundfx', 'unused_a.mp3'), b'unused-sound')
    # what must never travel: the original's scenes and platform leftovers
    put(os.path.join(a, 'scenes', 'lvl_a-esc', 'scene.esc'), b'scene')
    put(os.path.join(a, 'scenes', 'lvl_a-esc', 'scene.pvr'), b'pvr')
    put(os.path.join(a, 'effects', 'spark_a.par'), b'par')
    put(os.path.join(a, 'ETHFramework', 'sprites', 'fw_a.png'), b'framework')
    put(os.path.join(a, 'bundle_a.bin'), b'bin')
    put(os.path.join(a, 'settings_a.enml'), b'enml')
    # ---- Magic Rampage's pictures (optional)
    put(os.path.join(ref, 'magic-rampage', 'controls', 'pad_a.png'), b'pad-a')
    put(os.path.join(ref, 'magic-rampage', 'editor', 'ed_a.png'), b'ed-a')
    put(os.path.join(ref, 'ethanon', 'src', 'engine.cpp'), b'engine source')
    # ---- the converter's output
    put(os.path.join(repo, 'out', 'levels', 'lvl_a.tscn'),
        '[gd_scene format=3]\n'
        '[ext_resource type="Texture2D" path="res://assets/entities/sheet_a.png" id="1"]\n'
        '[node name="n1" type="Sprite2D"]\n'
        'metadata/entity_name = "thing_a"\n'
        'metadata/eth_lightmap = "res://assets/lightmaps/lm_a.png"\n'
        '[node name="marker" type="Node2D"]\n'
        'metadata/entity_name = "marker_b"\n')
    put(os.path.join(repo, 'out', 'levels', 'lvl_b.tscn'), '[gd_scene format=3]\n')
    put_json(os.path.join(repo, 'out', 'data', 'chapters.json'),
             {'worlds': [{'levels': [{'name': 'lvl_a', 'scene': 'res://assets/levels/lvl_a.tscn'},
                                     {'name': 'lvl_b', 'scene': 'res://assets/levels/lvl_b.tscn'}]}]})
    put_json(os.path.join(repo, 'out', 'data', 'achievements.json'),
             {'achievements': [{'icon': 'ach_a.png'}], 'secret': {'icon': 'ach_s.png'}})
    put(os.path.join(repo, 'out', 'assets', 'entities', 'sheet_a.png'), b'sheet-a-1x')
    put(os.path.join(repo, 'out', 'assets', 'entities', 'hd', 'sheet_a.png'), b'sheet-a-hd')
    put(os.path.join(repo, 'out', 'assets', 'entities', 'unused_sheet.png'), b'unused-sheet')
    put(os.path.join(repo, 'out', 'assets', 'lightmaps', 'lm_a.png'), b'lightmap-a')
    put(os.path.join(repo, 'out', 'assets', 'sounds_copy', 'boom_a.mp3'), b'converter copy')
    # ---- the committed tables
    put_json(os.path.join(repo, 'data', 'items.json'), {
        '_about': ['a note: not_a_file.png'],
        'tile': 'tile_a.png',
        'menu': 'sprites/menu_a.png',
        'item': 'item_a.png',
        'entity': 'thing_a.ent',
        'door': 'door_a.ent',
        'sheet': 'res://assets/entities/sheet_a.png',
    })
    put_json(os.path.join(repo, 'game', 'data', 'sounds.json'),
             {'sounds': {'boom': {'file': 'boom_a.mp3'}}, 'music': {'menu': {'file': 'tune_a.mp3'}}})
    put_json(os.path.join(repo, 'game', 'data', 'ui.json'), {'menu': {'icon_prefix': 'globe_', 'font': 'face_a.fnt'}})
    put_json(os.path.join(repo, 'game', 'data', 'tiers.json'), {'density_tiers': {'search': ['fullhd', 'hd']}})
    put_json(os.path.join(repo, 'game', 'data', 'art.json'), {'sprites': {'blue': '_blue.png'}, '_about': ['_green.png']})
    put_json(os.path.join(repo, 'game', 'data', 'controls.json'), {'art': {'mr': {'file': 'pad_a.png'}, 'generated': {'file': 'made_a.png'}}})
    # tracked files of the repository the packager must never write into
    put(os.path.join(repo, 'tools', 'readme.txt'), b'tracked')
    put(os.path.join(repo, 'engine', 'assets', 'shaders', 'one.spv'), b'spv-one')
    put(os.path.join(repo, 'engine', 'assets', 'shaders', 'two.spv'), b'spv-two')
    put(os.path.join(repo, 'engine', 'assets', 'shaders', 'one.vert'), b'glsl source')
    return repo, ref


EXPECTED_REQUIRED = sorted([
    # the converted levels, the tables beside them, the committed tables
    'levels/lvl_a.tscn', 'levels/lvl_b.tscn', 'tables/chapters.json', 'tables/achievements.json',
    'data/items.json', 'port-data/sounds.json', 'port-data/ui.json', 'port-data/tiers.json', 'port-data/controls.json', 'port-data/art.json',
    # the level's texture, picked in its hd tier
    'assets/entities/hd/sheet_a.png',
    # tables: tier twins, entity, particle bitmap, font and its page, sounds, music
    'original/sprites/hd/tile_a.png', 'original/sprites/hd/menu_a.png', 'original/entities/hd/item_a.png',
    'original/entities/thing_a.ent', 'original/particles/hd/spark_a.png', 'original/data/face_a.fnt', 'original/data/face_a_0.png',
    # an entity file brings its own picture (the hd twin is the one picked), its normal map and its halo, and the colour variant a table spells
    'original/entities/door_a.ent', 'original/entities/hd/door_a.png', 'original/entities/normalmaps/door_a_n.png', 'original/entities/glow_a.png',
    'original/entities/hd/door_a_blue.png',
    'original/soundfx/boom_a.mp3', 'original/soundfx/tune_a.mp3',
    # achievements and the world icon
    'original/sprites/achievements/ach_a.png', 'original/sprites/achievements/hd/ach_s.png', 'original/sprites/globe_0.png',
])
EXPECTED_FALLBACK = sorted([
    'assets/entities/sheet_a.png', 'original/sprites/tile_a.png', 'original/sprites/menu_a.png', 'original/entities/item_a.png',
    'original/particles/spark_a.png', 'original/entities/door_a.png', 'original/entities/door_a_blue.png',
])


class World(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.mkdtemp(prefix='mp-package-test-')
        self.addCleanup(shutil.rmtree, self._tmp, True)
        self.repo, self.ref = build_world(self._tmp)
        self.out = os.path.join(self.repo, 'out', 'dist', 'content')

    def run_main(self, *argv):
        err = io.StringIO()
        out = io.StringIO()
        with contextlib.redirect_stderr(err), contextlib.redirect_stdout(out):
            code = pk.main(list(argv))
        return code, out.getvalue(), err.getvalue()

    def content(self, *extra, out=None):
        return self.run_main('content', '--repo', self.repo, '--reference', self.ref, '--out', out or self.out, *extra)

    def manifest(self, folder=None):
        with open(os.path.join(folder or self.out, pk.MANIFEST), encoding='utf-8') as f:
            return json.load(f)

    def paths(self, folder=None, role=None):
        return sorted(e['path'] for e in self.manifest(folder)['files'] if role is None or e['role'] == role)

    def tree_bytes(self, folder):
        got = {}
        for d, ds, fs in os.walk(folder):
            for n in fs:
                full = os.path.join(d, n)
                with open(full, 'rb') as f:
                    got[os.path.relpath(full, folder).replace(os.sep, '/')] = f.read()
        return got


class TheClosure(World):
    def test_exactly_the_files_the_tables_name_are_packaged(self):
        code, out, err = self.content()
        self.assertEqual(code, 0, err)
        self.assertEqual(self.paths(role='required'), EXPECTED_REQUIRED)
        # the 1x file under a picked tier twin is what Tiers::Resolve falls back to
        self.assertEqual(self.paths(role='fallback-1x'), EXPECTED_FALLBACK)
        self.assertEqual(set(self.paths()), set(EXPECTED_REQUIRED) | set(EXPECTED_FALLBACK))

    def test_what_nothing_names_is_left_out(self):
        self.content()
        have = set(self.paths())
        for gone in ('original/sprites/unused_a.png', 'original/entities/thing_unused.ent', 'original/particles/spark_unused.png',
                     'original/soundfx/unused_a.mp3', 'assets/entities/unused_sheet.png',
                     # a font no table names: neither it nor its page
                     'original/data/face_b.fnt', 'original/data/face_b_0.png',
                     # a colour no table spells (the note's fragment is not one), and a variant of an entity nothing names
                     'original/entities/door_a_green.png', 'original/entities/door_unused_blue.png'):
            self.assertNotIn(gone, have, gone)

    def test_an_entity_file_brings_its_picture_its_normal_map_and_its_halo(self):
        self.content()
        have = set(self.paths())
        why = {}
        src = cl.Sources.from_disk(self.repo, self.ref)
        plan = cl.build_plan(src)
        for (tree, rel), reason in plan.required.items():
            why[rel] = reason
        self.assertIn('the sprite of original:entities/door_a.ent', why['entities/hd/door_a.png'])
        self.assertIn('the normal of original:entities/door_a.ent', why['entities/normalmaps/door_a_n.png'])
        self.assertIn('the halo of original:entities/door_a.ent', why['entities/glow_a.png'])
        self.assertIn('original/entities/door_a.png', have, 'and the 1x file under the picked twin')

    def test_a_colour_variant_is_found_by_the_suffix_a_table_spells_and_the_entity_that_names_it(self):
        self.content()
        have = set(self.paths())
        self.assertIn('original/entities/hd/door_a_blue.png', have)
        self.assertIn('original/entities/door_a_blue.png', have)
        self.assertNotIn('original/entities/door_a_green.png', have)
        self.assertEqual(cl.suffix_fragments(cl.Sources.from_disk(self.repo, self.ref)), ['_blue.png'])

    def test_the_lightmaps_are_never_packaged_though_a_level_names_one(self):
        self.content()
        self.assertFalse([p for p in self.paths() if 'lightmap' in p or 'lm_a' in p])
        self.assertTrue(self.manifest()['absent'])

    def test_nothing_on_the_never_package_list_travels(self):
        self.content()
        for p in self.paths():
            self.assertIsNone(pk.never_reason('/'.join(p.split('/')[1:])), p)
            self.assertFalse(p.endswith(('.esc', '.pvr', '.pkm', '.par', '.enml', '.bin', '.apk', '.exe', '.dll', '.cpp')), p)
            self.assertNotIn('scenes/', p)
            self.assertNotIn('ETHFramework', p)
            self.assertNotIn('ethanon', p)
        on_disk = [p for p, _ in pk.listing(self.out)]
        self.assertFalse([p for p in on_disk if p.endswith(('.esc', '.pvr', '.par', '.enml', '.bin', '.cpp'))])

    def test_the_manifest_names_sizes_and_hashes_of_what_was_copied(self):
        self.content()
        m = self.manifest()
        self.assertEqual(m['tool'], pk.TOOL)
        self.assertEqual(m['layout'], 'packaged')
        self.assertEqual(m['totals']['files'], len(m['files']))
        self.assertEqual(m['totals']['bytes'], sum(e['bytes'] for e in m['files']))
        self.assertEqual([e['path'] for e in m['files']], sorted(e['path'] for e in m['files']), 'sorted, so two runs agree')
        for e in m['files']:
            full = os.path.join(self.out, *e['path'].split('/'))
            self.assertEqual(os.path.getsize(full), e['bytes'], e['path'])
            self.assertEqual(pk.sha256_of(full), e['sha256'], e['path'])
        kinds = {e['path']: e['kind'] for e in m['files']}
        self.assertEqual(kinds['original/soundfx/tune_a.mp3'], 'music')
        self.assertEqual(kinds['original/soundfx/boom_a.mp3'], 'sound')
        self.assertEqual(kinds['levels/lvl_a.tscn'], 'level')
        self.assertEqual(kinds['original/data/face_a.fnt'], 'font')
        with open(os.path.join(self.out, pk.SUMS), encoding='utf-8') as f:
            sums = f.read().splitlines()
        self.assertEqual(len(sums), len(m['files']))
        self.assertTrue(all(len(line.split('  ')[0]) == 64 for line in sums))

    def test_the_references_that_name_nothing_are_recorded(self):
        self.content()
        absent = {(c, r) for c, r, _ in self.manifest()['absent']}
        self.assertIn(('tscn:entity_name', 'marker_b'), absent)            # a marker with no entity file
        self.assertIn(('json:image(generated at run time)', 'made_a.png'), absent)  # drawn by the game when it is not there

    def test_magic_rampages_pictures_only_with_the_flag(self):
        self.content()
        self.assertFalse([p for p in self.paths() if p.startswith('pad-art/')])
        self.assertIn(['json:image(bare)', 'pad_a.png', False], self.manifest()['absent'])
        other = os.path.join(self.repo, 'out', 'dist', 'with-pads')
        code, out, err = self.content('--with-pad-art', out=other)
        self.assertEqual(code, 0, err)
        pads = [e for e in self.manifest(other)['files'] if e['path'].startswith('pad-art/')]
        self.assertEqual(sorted(e['path'] for e in pads), ['pad-art/controls/pad_a.png'])
        self.assertTrue(all(e['role'] == 'optional-pad-art' for e in pads))
        self.assertNotIn(['json:image(bare)', 'pad_a.png', False], self.manifest(other)['absent'])

    def test_the_finished_package_passes_its_own_check(self):
        self.content()
        report = pk.verify_content(self.out)
        self.assertEqual(report['problems'], [])
        self.assertEqual(report['summary']['bad_hashes'], 0)
        self.assertEqual(report['summary']['classes'].get(cl.CASE_MISS_FATAL, 0) + report['summary']['classes'].get(cl.CASE_MISS_TIER, 0), 0)


class Determinism(World):
    def test_two_runs_are_the_same_bytes_in_the_same_order(self):
        a = os.path.join(self.repo, 'out', 'dist', 'a')
        b = os.path.join(self.repo, 'out', 'dist', 'b')
        self.assertEqual(self.content(out=a)[0], 0)
        self.assertEqual(self.content(out=b)[0], 0)
        self.assertEqual(self.tree_bytes(a), self.tree_bytes(b))
        self.assertEqual(self.run_main('same', a, b)[0], 0)
        self.assertEqual([p for p, _ in pk.listing(a)], [p for p, _ in pk.listing(b)])
        # and the times are fixed, so a copy made later is not a different package
        for d, ds, fs in os.walk(a):
            for n in fs:
                self.assertEqual(int(os.path.getmtime(os.path.join(d, n))), pk.FIXED_TIME)

    def test_a_run_is_not_moved_by_the_order_the_system_lists_files_in(self):
        # the plan is sorted, never listing-ordered: the same plan twice
        src = cl.Sources.from_disk(self.repo, self.ref)
        one = cl.build_plan(src)
        two = cl.build_plan(cl.Sources.from_disk(self.repo, self.ref))
        self.assertEqual(one.entries(False), two.entries(False))

    def test_same_reports_a_difference(self):
        a = os.path.join(self.repo, 'out', 'dist', 'a')
        b = os.path.join(self.repo, 'out', 'dist', 'b')
        self.content(out=a)
        self.content(out=b)
        with open(os.path.join(b, 'levels', 'lvl_b.tscn'), 'ab') as f:
            f.write(b'x')
        self.assertEqual(self.run_main('same', a, b)[0], 1)


class TheRefusals(World):
    def test_an_output_inside_the_tracked_tree_is_refused(self):
        for bad in (os.path.join(self.repo, 'tools', 'x'), os.path.join(self.repo, 'data', 'x'), os.path.join(self.repo, 'x'), self.repo):
            code, out, err = self.content(out=bad)
            self.assertEqual(code, 2, bad)
            self.assertIn('never be written into the tracked tree', err)
            self.assertFalse(os.path.exists(os.path.join(bad, pk.MANIFEST)), bad)

    def test_an_output_over_a_source_or_the_reference_is_refused(self):
        for bad in (os.path.join(self.repo, 'out', 'levels', 'x'), os.path.join(self.repo, 'out', 'assets'), os.path.join(self.ref, 'x'),
                    os.path.join(self.repo, 'out')):
            code, out, err = self.content(out=bad)
            self.assertEqual(code, 2, bad)

    def test_an_output_outside_the_repository_is_fine(self):
        elsewhere = os.path.join(self._tmp, 'elsewhere', 'pkg')
        code, out, err = self.content(out=elsewhere)
        self.assertEqual(code, 0, err)
        self.assertTrue(os.path.isfile(os.path.join(elsewhere, pk.MANIFEST)))

    def test_the_default_output_is_under_the_gitignored_out(self):
        code, out, err = self.run_main('content', '--repo', self.repo, '--reference', self.ref)
        self.assertEqual(code, 0, err)
        self.assertTrue(os.path.isfile(os.path.join(self.repo, 'out', 'dist', 'content', pk.MANIFEST)))

    def test_a_folder_this_tool_did_not_write_is_not_replaced(self):
        os.makedirs(self.out)
        put(os.path.join(self.out, 'mine.txt'), 'precious')
        code, out, err = self.content()
        self.assertEqual(code, 2)
        self.assertIn('is not replaced', err)
        with open(os.path.join(self.out, 'mine.txt'), encoding='utf-8') as f:
            self.assertEqual(f.read(), 'precious')
        # an empty folder is fine, and so is the tool's own earlier package
        os.remove(os.path.join(self.out, 'mine.txt'))
        self.assertEqual(self.content()[0], 0)
        put(os.path.join(self.out, 'stale.txt'), 'old')
        self.assertEqual(self.content()[0], 0)
        self.assertFalse(os.path.exists(os.path.join(self.out, 'stale.txt')), 'the tool\'s own package is replaced whole')
        self.assertFalse([n for n in os.listdir(os.path.dirname(self.out)) if '.staging-' in n], 'no staging folder is left')

    def test_a_failed_run_leaves_no_staging_folder_and_keeps_the_old_package(self):
        self.assertEqual(self.content()[0], 0)
        before = self.tree_bytes(self.out)
        os.remove(os.path.join(self.repo, 'out', 'data', 'chapters.json'))
        os.remove(os.path.join(self.repo, 'out', 'data', 'achievements.json'))
        shutil.rmtree(os.path.join(self.repo, 'out', 'levels'))
        code, out, err = self.content()
        self.assertEqual(code, 2)
        self.assertIn('out/levels', err)
        self.assertEqual(self.tree_bytes(self.out), before)
        self.assertFalse([n for n in os.listdir(os.path.dirname(self.out)) if '.staging-' in n])

    def test_a_symbolic_link_among_the_sources_is_refused(self):
        target = os.path.join(self._tmp, 'real_tile.png')
        put(target, b'real')
        link = os.path.join(self.ref, 'extracted', 'assets', 'sprites', 'hd', 'tile_a.png')
        os.remove(link)
        try:
            os.symlink(target, link)
        except (OSError, NotImplementedError):
            self.skipTest('this system will not make a symbolic link for the test')
        code, out, err = self.content()
        self.assertEqual(code, 2)
        self.assertIn('symbolic link', err)
        self.assertFalse(os.path.exists(os.path.join(self.out, pk.MANIFEST)))

    def test_a_link_to_a_whole_source_folder_is_refused_too(self):
        real = os.path.join(self._tmp, 'levels_elsewhere')
        shutil.move(os.path.join(self.repo, 'out', 'levels'), real)
        try:
            os.symlink(real, os.path.join(self.repo, 'out', 'levels'), target_is_directory=True)
        except (OSError, NotImplementedError):
            self.skipTest('this system will not make a symbolic link for the test')
        code, out, err = self.content()
        self.assertEqual(code, 2)
        self.assertIn('symbolic link', err)

    def test_a_reference_that_matches_only_ignoring_case_is_an_error(self):
        # The table says tile_a.png; the file on disk is Tile_A.png. Windows and macOS would open it, Linux and Android would not.
        base = os.path.join(self.ref, 'extracted', 'assets', 'sprites')
        shutil.rmtree(os.path.join(base, 'hd'))
        os.remove(os.path.join(base, 'tile_a.png'))
        put(os.path.join(base, 'Tile_A.png'), b'wrong case')
        put(os.path.join(base, 'menu_a_dummy.png'), b'x')
        code, out, err = self.content()
        self.assertEqual(code, 2, err)
        self.assertIn('ignoring case', err)
        self.assertIn('tile_a.png', err)
        self.assertFalse(os.path.exists(os.path.join(self.out, pk.MANIFEST)))

    def test_a_tier_twin_in_the_wrong_case_is_an_error_not_a_quiet_fall_to_the_1x_file(self):
        base = os.path.join(self.ref, 'extracted', 'assets', 'sprites', 'hd')
        os.remove(os.path.join(base, 'tile_a.png'))
        put(os.path.join(base, 'Tile_A.png'), b'twin in the wrong case')
        code, out, err = self.content()
        self.assertEqual(code, 2, err)
        self.assertIn('CASE_MISS', err)

    def test_a_table_that_names_a_never_package_file_is_refused(self):
        put_json(os.path.join(self.repo, 'data', 'bad.json'), {'x': 'effects/spark_a.par'})
        code, out, err = self.content()
        self.assertEqual(code, 2, err)
        self.assertIn('never-package', err)

    def test_missing_sources_are_named(self):
        shutil.rmtree(os.path.join(self.ref, 'extracted'))
        code, out, err = self.content()
        self.assertEqual(code, 2)
        self.assertIn('original', err)
        self.assertIn('not there', err)

    def test_the_never_package_rules(self):
        for rel in ('scenes/a/b.esc', 'ETHFramework/sprites/a.png', 'effects/a.par', 'lightmaps/a.png', 'a.pvr', 'a.pkm', 'a.enml', 'a.bin',
                    'x/build/a.png', 'build-play/a.png', '.git/config', 'a.exe', 'a.dll', 'a.apk', 'reference/ethanon/a.png', 'a.list'):
            self.assertIsNotNone(pk.never_reason(rel), rel)
        for rel in ('entities/a.png', 'sprites/hd/a.png', 'soundfx/a.mp3', 'data/a.fnt', 'particles/a.png', 'levels/a.tscn', 'a.json',
                    'builder.png', 'scenic/a.png'):
            self.assertIsNone(pk.never_reason(rel), rel)


class TheCheck(World):
    def setUp(self):
        super().setUp()
        self.assertEqual(self.content()[0], 0)

    def test_a_changed_file_a_missing_file_and_an_extra_file_are_found(self):
        with open(os.path.join(self.out, 'levels', 'lvl_b.tscn'), 'ab') as f:
            f.write(b'tampered')
        os.remove(os.path.join(self.out, 'original', 'soundfx', 'boom_a.mp3'))
        put(os.path.join(self.out, 'original', 'sprites', 'stray.png'), b'stray')
        report = pk.verify_content(self.out)
        text = '\n'.join(report['problems'])
        self.assertIn('size differs: levels/lvl_b.tscn', text)
        self.assertIn('listed but missing: original/soundfx/boom_a.mp3', text)
        self.assertIn('on disk but not in the manifest: original/sprites/stray.png', text)
        self.assertEqual(self.run_main('verify', self.out)[0], 1)

    def test_a_changed_byte_is_found_by_the_hash_alone(self):
        path = os.path.join(self.out, 'levels', 'lvl_b.tscn')
        with open(path, 'rb') as f:
            data = f.read()
        with open(path, 'wb') as f:
            f.write(bytes([data[0] ^ 1]) + data[1:])
        self.assertTrue(any('sha256 differs' in p for p in pk.verify_content(self.out)['problems']))

    def test_a_lightmap_in_the_package_is_found(self):
        put(os.path.join(self.out, 'assets', 'lightmaps', 'lm_a.png'), b'lightmap')
        text = '\n'.join(pk.verify_content(self.out)['problems'])
        self.assertIn('lightmap', text)

    def test_a_file_the_game_names_but_the_package_lacks_is_found(self):
        # rebuild the manifest after removing a file a table names (the world's icon), so the manifest and the disk agree and only the
        # model can tell: the reference now resolves to nothing, which the source run did not record
        path = os.path.join(self.out, 'original', 'sprites', 'globe_0.png')
        os.remove(path)
        m = self.manifest()
        m['files'] = [e for e in m['files'] if e['path'] != 'original/sprites/globe_0.png']
        pk.write_manifest(self.out, m)
        text = '\n'.join(pk.verify_content(self.out)['problems'])
        self.assertIn('differ from the source run', text)
        self.assertIn('globe_0.png', text)

    def test_a_case_miss_inside_the_package_is_found(self):
        # a package copied through a case-folding step: the model sees a reference that only matches ignoring case
        old = os.path.join(self.out, 'original', 'soundfx', 'tune_a.mp3')
        new = os.path.join(self.out, 'original', 'soundfx', 'Tune_A.mp3')
        os.rename(old, new)
        m = self.manifest()
        for e in m['files']:
            if e['path'] == 'original/soundfx/tune_a.mp3':
                e['path'] = 'original/soundfx/Tune_A.mp3'
        m['files'].sort(key=lambda e: e['path'])
        pk.write_manifest(self.out, m)
        text = '\n'.join(pk.verify_content(self.out)['problems'])
        self.assertIn('CASE_MISS', text)


class TheShell(World):
    def setUp(self):
        super().setUp()
        self.assertEqual(self.content()[0], 0)
        self.exe = os.path.join(self._tmp, 'MagicPortals.test')
        put(self.exe, b'not a program')
        self.shaders = os.path.join(self.repo, 'engine', 'assets', 'shaders')
        self.shell_out = os.path.join(self._tmp, 'with a space', 'shell')

    def shell(self, **kw):
        args = ['shell', '--content', kw.get('content', self.out), '--exe', kw.get('exe', self.exe), '--out', kw.get('out', self.shell_out),
                '--shaders', self.shaders]
        return self.run_main(*args)

    def test_the_shell_is_the_program_the_shaders_and_the_content(self):
        code, out, err = self.shell()
        self.assertEqual(code, 0, err)
        names = [p for p, _ in pk.listing(self.shell_out)]
        self.assertIn('MagicPortals.test', names)
        self.assertEqual(sorted(n for n in names if n.startswith('assets/shaders/')), ['assets/shaders/one.spv', 'assets/shaders/two.spv'])
        self.assertFalse([n for n in names if n.endswith('.vert')], 'only the compiled shaders')
        self.assertTrue(all(n.startswith('content/') for n in names if n not in ('MagicPortals.test', 'shell.json') and
                                                                 not n.startswith('assets/shaders/')))
        self.assertEqual(self.tree_bytes(os.path.join(self.shell_out, 'content')), self.tree_bytes(self.out))
        self.assertEqual(pk.verify_content(os.path.join(self.shell_out, 'content'))['problems'], [])

    def test_a_content_folder_that_fails_its_check_is_not_laid_out(self):
        os.remove(os.path.join(self.out, 'levels', 'lvl_a.tscn'))
        code, out, err = self.shell()
        self.assertEqual(code, 2)
        self.assertIn('fails its check', err)
        self.assertFalse(os.path.exists(self.shell_out))

    def test_a_linked_program_is_refused(self):
        link = os.path.join(self._tmp, 'linked.exe')
        try:
            os.symlink(self.exe, link)
        except (OSError, NotImplementedError):
            self.skipTest('this system will not make a symbolic link for the test')
        code, out, err = self.shell(exe=link)
        self.assertEqual(code, 2)
        self.assertIn('not a plain file', err)

    def test_the_shell_is_not_written_over_the_content(self):
        code, out, err = self.shell(out=os.path.join(self.out, 'inside'))
        self.assertEqual(code, 2)


if __name__ == '__main__':
    unittest.main()
