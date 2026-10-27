"""Tests of tools/ci/check_public.py (planning step 210, PORT-12): the public-readiness checker.

Run:   python -m unittest discover -s tools/ci -p "test_*.py" -v           (standard library and git; no network, no global git configuration is read or written)

WHAT THIS PROVES. Every test builds a SYNTHETIC repository with `git init` in a temporary folder (an author the test sets, the user's own git configuration out of the way) and runs the
checker on it. A repository that is as it should be passes every rule, and for EACH rule (H1 to H12) there are copies edited in one place that the rule must refuse, each
with a case that must stay quiet beside it (a name in a longer word, a placeholder, a link in code font). The history mode is run on synthetic histories: a picture added and removed again,
a key in a commit that no later commit holds, a branch that is not main. The account H4 looks for is an INVENTED name, given as an argument: no real account name is written here.
WHAT IT CANNOT PROVE. That a file a stranger reads is free of everything the rules cannot name. The last test runs the checker on this repository with the files not yet added included; it
is the same run the gates make.
"""
import contextlib
import hashlib
import io
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)

import check_public as cp  # noqa: E402
import running_account as ra  # noqa: E402

ACCOUNT = 'zorbgar'  # invented: the account whose name H4 looks for in these tests
VERSION_OK = lambda repo: (0, 'check_version: ok')  # noqa: E731  (the stand-in for H12's script, where a test is about another rule)
GITLINK_SHA = '0123456789abcdef0123456789abcdef01234567'
# The README's one sentence about the owner's leave to make this version is the owner's wording, and the README is the only file meant to hold it: this file pins it by the sha256 of its
# line and finds it in the README at run time (`readme_sentence`), so that it is not written here a second time.
PERMITTED_DIGEST = 'e0bcbe317859ab4ccffc8198c71718501fea58a0918df8d258603e6937d0d648'


def readme_sentence(text):
    """The line of `text` whose sha256 is PERMITTED_DIGEST, or None."""
    found = [l for l in text.split('\n') if hashlib.sha256(l.strip().encode('utf-8')).hexdigest() == PERMITTED_DIGEST]
    return found[0].strip() if found else None

# A block of the size the rule expects (cp.H6_BLOCK_FLOOR comment lines at least) with its two anchor phrases, as the real file's is.
GITIGNORE = ('# ' + '=' * 70 + '\n#  reference/ IS DELIBERATELY EXCLUDED.\n' + '\n'.join('#  line %d of the block that explains the reference/ line' % i for i in range(1, 26))
             + '\n#  DELIBERATELY UNANCHORED: it matches at any depth.\n# ' + '=' * 70 + '\nreference/\n\nout/\nbuild/\n')


def good_files():
    """The files of a repository that every rule passes. A test copies this dict, edits one entry, and builds the repository from the copy."""
    return {
        'README.md': '# Name\n\nFront.\n\n## Local setup after cloning\n\nSteps.\n\n## Run it\n\nGo. See [the build](docs/building.md#building-it) and [platforms](docs/platforms.md).\n\n## Parity work\n\nMore.\n',
        'LICENSE': 'MIT No Attribution\n\nCopyright (c) 2026 Somebody\n\nPermission is hereby granted ... to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies.\n',
        'NOTICE.md': '# Notices\n\nThird-party code: a library, MIT, with its notice.\n',
        'CONTRIBUTING.md': '# Contributing\n\nBuild it. Test it.\n',
        'SECURITY.md': '# Security\n\nReport a vulnerability privately.\n\n' + cp.PLACEHOLDER + '\n',
        'docs/platforms.md': '# Platforms\n\nWindows, Linux.\n',
        'docs/building.md': '# Building\n\n## Building it\n\nRun cmake.\n',
        '.gitignore': GITIGNORE,
        '.gitmodules': '[submodule "engine"]\n\tpath = engine\n\turl = https://github.com/someone/engine.git\n',
        'tools/build.sh': '#!/bin/sh\necho hi\n',
        'tools/package/check_version.py': 'print("stand-in")\n',
        'game/main.cpp': 'int main() { return 0; }\n',
        'data/table.json': '{"a": 1}\n',
    }


def run_git(repo, *args, env=None):
    done = subprocess.run(['git', '-C', repo] + list(args), capture_output=True, text=True, env=env)
    if done.returncode != 0:
        raise RuntimeError('git %s: %s' % (' '.join(args), done.stderr))
    return done.stdout


class SyntheticRepos(unittest.TestCase):
    """Makes repositories in temporary folders; git is isolated from the user's configuration."""

    def setUp(self):
        self.folders = []
        self.env = {'GIT_CONFIG_GLOBAL': os.devnull, 'GIT_CONFIG_NOSYSTEM': '1', 'GIT_AUTHOR_NAME': 'Test Author', 'GIT_AUTHOR_EMAIL': 'author@example.invalid',
                    'GIT_COMMITTER_NAME': 'Test Author', 'GIT_COMMITTER_EMAIL': 'author@example.invalid'}
        patcher = mock.patch.dict(os.environ, self.env)
        patcher.start()
        self.addCleanup(patcher.stop)
        self.addCleanup(self.cleanup)

    def cleanup(self):
        for folder in self.folders:
            shutil.rmtree(folder, ignore_errors=True)

    def make(self, files, executable=('tools/build.sh',), gitlink='engine', links=None, commit=True, message='first'):
        folder = tempfile.mkdtemp(prefix='check_public_test_')
        self.folders.append(folder)
        run_git(folder, 'init', '-q')
        run_git(folder, 'config', 'core.autocrlf', 'false')
        run_git(folder, 'config', 'core.fileMode', 'false')
        self.write(folder, files)
        run_git(folder, 'add', '-A')
        for path in executable:
            if path in files:
                run_git(folder, 'update-index', '--chmod=+x', path)
        if gitlink:
            run_git(folder, 'update-index', '--add', '--cacheinfo', '160000,%s,%s' % (GITLINK_SHA, gitlink))
        for path, target in (links or {}).items():
            blob = subprocess.run(['git', '-C', folder, 'hash-object', '-w', '--stdin'], input=target, capture_output=True, text=True).stdout.strip()
            run_git(folder, 'update-index', '--add', '--cacheinfo', '120000,%s,%s' % (blob, path))
        if commit:
            run_git(folder, 'commit', '-q', '-m', message)
        return folder

    def write(self, folder, files):
        for path, content in files.items():
            full = os.path.join(folder, *path.split('/'))
            os.makedirs(os.path.dirname(full), exist_ok=True)
            with open(full, 'wb') as f:
                f.write(content if isinstance(content, bytes) else content.encode('utf-8'))

    def commit_more(self, folder, files, message='more', remove=()):
        self.write(folder, files)
        for path in remove:
            os.remove(os.path.join(folder, *path.split('/')))
        run_git(folder, 'add', '-A')
        run_git(folder, 'commit', '-q', '-m', message)

    def findings(self, files, rules=None, accounts=(ACCOUNT,), version_check=VERSION_OK, **make_args):
        repo = self.make(files, **make_args)
        return cp.check(repo, rules=rules, accounts=list(accounts), version_check=version_check)

    @staticmethod
    def by(findings, cls=None, rule=None):
        return [f for f in findings if (cls is None or f.cls == cls) and (rule is None or f.rule == rule)]

    def errors(self, findings, rule):
        return self.by(findings, cp.ERROR, rule)

    def assertRed(self, files, rule, count=None, **kw):
        found = self.errors(self.findings(files, **kw), rule)
        self.assertTrue(found, 'rule %s did not refuse the edited copy' % rule)
        if count is not None:
            self.assertEqual(len(found), count, [repr(f) for f in found])
        return found

    def assertGreen(self, files, rule, **kw):
        found = self.errors(self.findings(files, **kw), rule)
        self.assertEqual(found, [], [repr(f) for f in found])

    def edited(self, **changes):
        files = good_files()
        for path, content in changes.items():
            path = path.replace('__', '/').replace('_dot_', '.')
            if content is None:
                files.pop(path, None)
            else:
                files[path] = content
        return files


class TheGoodRepository(SyntheticRepos):
    def test_a_repository_as_it_should_be_has_no_error_and_no_owner_file_finding(self):
        found = self.findings(good_files())
        self.assertEqual(self.by(found, cp.ERROR), [], [repr(f) for f in found])
        self.assertEqual(self.by(found, cp.OWNER_FILE), [])
        self.assertEqual(self.by(found, cp.RECORD_PATH), [])

    def test_the_only_notes_of_a_good_repository_are_the_placeholder_and_the_largest_files(self):
        notes = self.by(self.findings(good_files()), cp.NOTE)
        self.assertTrue(any(f.rule == 'H5' and f.path == 'SECURITY.md' for f in notes))
        self.assertTrue(all(f.rule in ('H2', 'H4', 'H5') for f in notes), [repr(f) for f in notes])

    def test_every_rule_is_listed_and_can_be_chosen_alone(self):
        self.assertEqual([r for r, _ in cp.RULES], ['H%d' % i for i in range(1, 13)])
        repo = self.make(good_files())
        for rule, _ in cp.RULES:
            cp.check(repo, rules=[rule], accounts=[ACCOUNT], version_check=VERSION_OK)
        with self.assertRaises(cp.GitError):
            cp.check(repo, rules=['H99'])

    def test_a_folder_that_is_not_a_git_work_tree_is_unreadable_not_clean(self):
        folder = tempfile.mkdtemp(prefix='check_public_test_')
        self.folders.append(folder)
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stderr(err):
            self.assertEqual(cp.main(['--repo', folder], out=out), cp.EXIT_UNREADABLE)
        self.assertIn('not a git work tree', err.getvalue())


class H1Names(SyntheticRepos):
    def test_each_extension_of_the_list_is_refused_in_any_case_and_depth(self):
        for name in ('a/b/pic.png', 'x/SOUND.MP3', 'level.esc', 'a.keystore', 'bundle.tar.gz', 'tool.exe', 'lib.a', 'icon.ICO', 'font.ttf', 'k.pem', 'x.jks', 'app.aab', 'x.pkm'):
            files = self.edited()
            files[name] = 'x'
            found = self.assertRed(files, 'H1', 1)
            self.assertEqual(found[0].path, name)

    def test_names_that_only_look_like_one_pass(self):
        files = self.edited()
        files.update({'notes.png.md': 'x', 'png': 'x', 'docs/png/readme.txt': 'x', 'a.pngx': 'x', '.png': 'x', 'dll': 'x', 'src/a.cpp': 'x'})
        self.assertGreen(files, 'H1')

    def test_the_list_is_the_one_the_step_names(self):
        for ext in 'png bmp jpg jpeg gif webp ico icns mp3 ogg wav esc ent enml par fnt pvr pkm exe dll lib obj so a o apk aab dex jks keystore idsig p12 pfx key pem zip jar class gz tar tga dds psd ttf otf'.split():
            self.assertIn(ext, cp.EXTENSIONS)


class H2Binaries(SyntheticRepos):
    def test_a_file_with_a_nul_byte_is_refused(self):
        files = self.edited()
        files['data/blob.dat'] = b'abc\0def'
        found = self.assertRed(files, 'H2', 1)
        self.assertIn('NUL', found[0].text)

    def test_a_file_over_the_cap_is_refused_and_one_at_the_cap_is_not(self):
        with mock.patch.object(cp, 'MOST_BYTES', 2000):
            files = self.edited()
            files['docs/big.md'] = 'x' * 2001
            self.assertRed(files, 'H2', 1)
            files['docs/big.md'] = 'x' * 2000
            self.assertGreen(files, 'H2')

    def test_text_in_utf16_with_a_byte_order_mark_is_not_a_binary(self):
        files = self.edited()
        files['docs/wide.txt'] = '\ufeffhello'.encode('utf-16-le')
        self.assertGreen(files, 'H2')

    def test_the_largest_files_are_printed_as_notes_at_most_ten(self):
        files = self.edited()
        for i in range(15):
            files['docs/f%02d.md' % i] = 'x' * (200 + i)
        notes = self.by(self.findings(files, rules=['H2']), cp.NOTE, 'H2')
        self.assertEqual(len(notes), 10)
        self.assertIn('f14.md', ' '.join(f.path for f in notes))


class H3Secrets(SyntheticRepos):
    SECRETS = {
        'a pem key': '-----BEGIN RSA PRIVATE KEY-----\nMIIE\n-----END RSA PRIVATE KEY-----\n',
        'a bare pem key': '-----BEGIN PRIVATE KEY-----\n',
        'an openssh key': '-----BEGIN OPENSSH PRIVATE KEY-----\n',
        'a fine-grained github token': 'token = github_pat_11ABCDEFG0abcdefghijklmnopqrstuvwxyz\n',
        'a classic github token': 'x = ghp_' + 'a1B2c3D4e5F6g7H8i9J0k1L2m3N4o5P6q7R8' + '\n',
        'an aws key': 'AWS=AKIA' + 'ABCDEFGHIJKLMNOP' + '\n',
        'a slack token': 'SLACK=xoxb-1234567890-abcdefghij\n',
        'storepass': 'keytool -storepass hunter22 -list\n',
        'ks-pass literal': 'apksigner sign --ks-pass pass:hunter22 a.apk\n',
        'ks-pass equals': 'apksigner sign --ks-pass=pass:hunter22 a.apk\n',
        'keypass': 'keytool -keypass sekret99 x\n',
    }

    def test_each_secret_shaped_text_is_refused(self):
        for what, text in self.SECRETS.items():
            files = self.edited()
            files['docs/notes.txt'] = 'line one\n' + text
            found = self.errors(self.findings(files, rules=['H3']), 'H3')
            self.assertEqual(len(found), 1, what)
            self.assertEqual((found[0].path, found[0].line), ('docs/notes.txt', 2), what)

    def test_a_password_taken_from_somewhere_else_is_not_a_literal(self):
        for text in ('--ks-pass file:/tmp/pw', '--ks-pass env:KS_PASS', '--ks-pass stdin', '-storepass "$PASSWORD"', '-storepass $PW', '--ks-pass pass:$PW', '-storepass <password>',
                     '--ks-pass pass:%PW%', '--keystore-pass-file /x/pw', '-keypass ...', '--ks-pass'):
            files = self.edited()
            files['docs/notes.txt'] = text + '\n'
            self.assertGreen(files, 'H3')

    def test_a_public_key_and_ordinary_words_are_not_secrets(self):
        files = self.edited()
        files['docs/notes.txt'] = '-----BEGIN PUBLIC KEY-----\nghp_short\nAKIAshort\nxox-nothing\n'
        self.assertGreen(files, 'H3')

    def test_the_checkers_own_source_and_tests_hold_the_patterns_and_are_not_read(self):
        files = self.edited()
        files['tools/ci/check_public.py'] = self.SECRETS['a pem key']
        files['tools/ci/test_check_public.py'] = self.SECRETS['storepass']
        self.assertGreen(files, 'H3')

    def test_an_allow_listed_literal_is_a_note_with_its_reason_in_its_own_file_only(self):
        text = 'apksigner sign --ks-pass pass:public1 a.apk\n'
        files = self.edited()
        files['tools/build_android.sh'] = text
        found = self.findings(files, rules=['H3'])
        self.assertEqual(self.errors(found, 'H3'), [])
        notes = self.by(found, cp.NOTE, 'H3')
        self.assertEqual(len(notes), 1)
        self.assertIn('debug keystore', notes[0].text)
        files['tools/other.sh'] = text
        self.assertRed(files, 'H3', 1)


class H4Paths(SyntheticRepos):
    CASES = {
        'windows backslashes': 'see C:\\Users\\bobsmith\\Desktop\\x',
        'windows slashes': 'see D:/Users/bobsmith/Desktop/x',
        'windows doubled backslashes (a string literal)': 'path = "C:\\\\Users\\\\bobsmith\\\\x"',
        'linux home': 'cd /home/bobsmith/work',
        'macos home': 'open /Users/bobsmith/Library',
        'a drive mounted into WSL': 'cd /mnt/c/stuff',
        'the account': 'owner %s was here' % ACCOUNT,
        'the account in another case': 'owner %s was here' % ACCOUNT.upper(),
        # step 210, fix round 2: a name of five letters or more is a SUBSTRING search (as the literal it replaced was), so the shapes a word search missed are refused too
        'the account glued to letters before': 'owner xx%s was here' % ACCOUNT,
        'the account glued to letters after': 'owner %sage door' % ACCOUNT,
        'the account glued to a digit': 'owner %s2 built it' % ACCOUNT,
        'the account in front of more letters, digits and an at-sign': 'owner %srest99@example.invalid wrote' % ACCOUNT,
        'the account with an underscore': 'owner %s_build was here' % ACCOUNT,
    }

    def test_each_path_of_a_pc_is_refused_outside_the_records(self):
        for what, text in self.CASES.items():
            files = self.edited()
            files['docs/guide.md'] = 'first line\n' + text + '\n'
            found = self.errors(self.findings(files, rules=['H4']), 'H4')
            self.assertEqual(len(found), 1, what)
            self.assertEqual((found[0].path, found[0].line), ('docs/guide.md', 2), what)

    def test_placeholders_and_system_folders_pass(self):
        for text in ('C:\\Users\\<name>\\x', 'C:\\Users\\%USERNAME%\\x', '/home/<you>/x', '/Users/runner/work/x', 'D:\\Users\\Public\\x', '/home/user/x', '/mnt/<drive>/x', '/mnt/',
                     'C:\\Users\\x\\y', 'the zorbing company', 'xzorbx', '/usr/home/', 'a/Users/b', '/mnt/cdrom/x', '/home/', '~/Library/Application Support'):
            files = self.edited()
            files['docs/guide.md'] = text + '\n'
            self.assertGreen(files, 'H4')

    def test_a_name_of_four_letters_is_a_whole_word_search_and_a_longer_one_is_not(self):
        # four letters would be found inside too many honest words ("zorbing"): that name is a word. Five letters or more are looked for anywhere.
        for text, red in (('the zorbing company', False), ('xzorbx', False), ('zorb2', False), ('owner zorb was here', True), ('owner "zorb" was here', True), ('owner ZORB_x', True)):
            files = self.edited()
            files['docs/guide.md'] = text + '\n'
            self.assertEqual(bool(self.errors(self.findings(files, rules=['H4'], accounts=['zorb']), 'H4')), red, text)
        for text in ('the zorbgaring company', 'xzorbgarx', 'zorbgar2'):
            files = self.edited()
            files['docs/guide.md'] = text + '\n'
            self.assertEqual(len(self.errors(self.findings(files, rules=['H4']), 'H4')), 1, text)

    def test_the_name_inside_an_email_address_is_the_email_note_and_nothing_else_is_forgiven(self):
        # a maintainer's public contact commonly starts with the account's name (SECURITY.md is where it will be written): that is one deliberate publication, reported by the e-mail note
        # of H4 and not a leak of the owner's PC. The exemption is the span of the ADDRESS only.
        files = self.edited()
        files['SECURITY.md'] = 'Report to %srest99@mail.example.net please.\n' % ACCOUNT
        found = self.findings(files, rules=['H4'])
        self.assertEqual(self.errors(found, 'H4'), [])
        self.assertTrue([f for f in self.by(found, cp.NOTE, 'H4') if f.path == 'SECURITY.md' and 'e-mail address' in f.text])
        for text in ('Report to %s mail.example.net please.' % ACCOUNT,                      # the name beside the address
                     'Report to %s or %srest99@mail.example.net' % (ACCOUNT, ACCOUNT),         # the name beside an address that also holds it
                     'C:/Users/bobsmith/x%srest99@mail.example.net' % ACCOUNT,                 # a path, then an address
                     'Report to %srest99@example.invalid please.' % ACCOUNT,                    # a placeholder address is not an address of a person
                     'Report to 1234+%s@users.noreply.github.com please.' % ACCOUNT,            # nor is a GitHub noreply address
                     '/work/%s/notes' % ACCOUNT):                                               # a path
            files = self.edited()
            files['SECURITY.md'] = text + '\n'
            self.assertTrue(self.errors(self.findings(files, rules=['H4']), 'H4'), text)

    def test_the_hit_is_reported_by_kind_and_never_spells_the_account(self):
        files = self.edited()
        files['docs/guide.md'] = 'owner %s was here\n' % ACCOUNT
        found = self.errors(self.findings(files, rules=['H4']), 'H4')
        self.assertNotIn(ACCOUNT, found[0].text.lower())
        self.assertIn("account's name", found[0].text)

    def test_an_email_address_of_a_person_is_a_note_with_its_lines_and_never_printed(self):
        files = self.edited()
        files['docs/guide.md'] = ('one\nwrite to bob@bobsmith.org or alice.b@mail.example.net\n'
                                  'three: git@github.com:x/y.git, a@example.com, b@host.invalid, 1234+c@users.noreply.github.com\n')
        found = self.findings(files, rules=['H4'])
        self.assertEqual(self.errors(found, 'H4'), [])
        notes = [f for f in self.by(found, cp.NOTE, 'H4') if f.path == 'docs/guide.md']
        self.assertEqual(len(notes), 1)
        self.assertIn('1 line(s) (2)', notes[0].text)
        self.assertNotIn('bobsmith', notes[0].text)

    def test_two_kinds_on_one_line_are_one_finding(self):
        files = self.edited()
        files['docs/guide.md'] = 'C:/Users/bobsmith/x\n'
        self.assertEqual(len(self.errors(self.findings(files, rules=['H4']), 'H4')), 1)

    def test_in_a_record_file_the_same_hits_are_counted_per_file_with_their_lines(self):
        text = 'one\nC:\\Users\\bobsmith\\x\nthree\n/home/bobsmith/y\n' + ACCOUNT + '\n'
        for record in ('DEVLOG.md', 'docs/parity-backlog.md', 'CLAUDE.md', 'docs/planning/plan.md'):
            files = self.edited()
            files[record] = text
            found = self.findings(files, rules=['H4'])
            self.assertEqual(self.errors(found, 'H4'), [], record)
            rec = self.by(found, cp.RECORD_PATH, 'H4')
            self.assertEqual([f.path for f in rec], [record])
            self.assertIn('lines 2, 4, 5', rec[0].text)
            self.assertIn('3 line(s)', rec[0].text)

    def test_a_file_that_probes_for_the_name_is_allowed_by_name_with_its_reason(self):
        files = self.edited()
        files['tests/test_mp_applelayout.cpp'] = '// ' + ACCOUNT + '\n'
        found = self.findings(files, rules=['H4'])
        self.assertEqual(self.errors(found, 'H4'), [])
        notes = [f for f in self.by(found, cp.NOTE, 'H4') if f.path == 'tests/test_mp_applelayout.cpp']
        self.assertEqual(len(notes), 1)
        self.assertIn('allow-listed by name', notes[0].text)
        files['tests/test_other.cpp'] = '// ' + ACCOUNT + '\n'
        self.assertRed(files, 'H4', 1)

    def test_a_short_or_generic_account_is_not_searched_and_the_note_says_so(self):
        for name in ('bo', 'runner', 'USER', 'root'):
            files = self.edited()
            files['docs/guide.md'] = 'we are %s here, runner and root and user\n' % name
            found = self.findings(files, rules=['H4'], accounts=[name])
            self.assertEqual(self.errors(found, 'H4'), [], name)
            note = [f for f in self.by(found, cp.NOTE, 'H4') if not f.path]
            self.assertIn('0 name(s) searched', note[0].text)
            self.assertIn('too short or generic', note[0].text)

    def test_the_running_account_is_read_from_the_system_not_from_a_file(self):
        with mock.patch.object(ra.getpass, 'getuser', return_value=ACCOUNT), mock.patch.dict(os.environ, {'USERNAME': '', 'USER': '', 'LOGNAME': ''}), \
                mock.patch.object(ra.os.path, 'expanduser', return_value='/nowhere/' + ACCOUNT):
            self.assertEqual(cp.running_accounts()[0], ACCOUNT)
            self.assertEqual(cp.running_accounts()[1], ACCOUNT)
            files = self.edited()
            files['docs/guide.md'] = 'it is %s\n' % ACCOUNT
            repo = self.make(files)
            found = cp.check(repo, rules=['H4'], version_check=VERSION_OK)
            self.assertEqual(len(self.errors(found, 'H4')), 1)

    def test_this_file_and_the_checker_hold_no_name_of_the_account_that_runs_them(self):
        searched, _ = cp.searchable_accounts(cp.running_accounts())
        account = cp.account_regex(searched)
        for name in ('check_public.py', 'test_check_public.py', 'check_workflows.py'):
            with open(os.path.join(HERE, name), encoding='utf-8') as f:
                text = f.read()
            self.assertEqual([] if account is None else [m.group(0) for m in account.finditer(text)], [], name)


class H5Wording(SyntheticRepos):
    # (a text, the name of the phrase of KEPT_OUT_PHRASES that must be among the ones found in it)
    PHRASES = (
        ('the data may travel', 'data may travel'), ('whether the data may leave his machine', 'whether the data may'), ("it may leave the owner's machine", 'may leave a machine'),
        ('it may leave the machine', 'may leave a machine'), ('we are allowed to ship it', 'allowed to ship'), ('the files may be shipped', 'may be shipped'), ('he may ship the game', 'may ship'),
        ('the right to ship', 'right to ship'), ('the right to distribute', 'right to ship'), ('a doubt about the right to ship', 'a doubt about the right'),
        ('this does not cover redistribution', 'redistribution'), ('permission does not cover it', 'permission does not cover'), ('we have no permission to publish', 'no permission to'),
        ('see docs/permission.md', 'permission.md'), ('the rights holder', 'rights holder'), ('the rights-holder', 'rights holder'), ('the grant of permission', 'the grant'),
        ('the legal basis', 'legal basis'), ('the rights caveat', 'rights caveat'), ('the rights question', 'rights caveat'), ('written permission', 'written permission'),
        ('permission to rebuild it', 'permission to rebuild'), ('Redistribute them', 'redistribution'), ("whether the game's data is allowed", 'whether the data may'),
        ('they leave his machine', 'leave his machine'), ('the rights grant', 'rights grant'),
    )

    def test_each_phrase_is_refused_outside_the_records_and_by_its_own_name(self):
        for phrase, name in self.PHRASES:
            files = self.edited()
            files['docs/guide.md'] = 'one\nwe say: ' + phrase + '.\n'
            found = self.errors(self.findings(files, rules=['H5']), 'H5')
            self.assertTrue(found, phrase)
            self.assertEqual({f.line for f in found}, {2}, phrase)
            self.assertIn('(%s)' % name, ' '.join(f.text for f in found), phrase)

    def test_every_phrase_of_the_list_has_a_case_above_that_names_it(self):
        named = {name for _, name in self.PHRASES}
        for name, _ in cp.KEPT_OUT_PHRASES:
            self.assertIn(name, named, 'no test case catches the phrase %r by its own name' % name)

    def test_a_phrase_broken_over_two_lines_or_a_comment_mark_is_still_one(self):
        files = self.edited()
        files['game/x.cpp'] = '// whether the data\n// may leave his\n// machine\n'
        self.assertRed(files, 'H5')

    def test_the_one_sentence_the_readme_may_say_and_ordinary_words_pass(self):
        with open(os.path.join(REPO, 'README.md'), encoding='utf-8') as f:
            sentence = readme_sentence(f.read())
        self.assertIsNotNone(sentence, "the README no longer holds the owner's one sentence (its digest is pinned in this file)")
        for text in (sentence, 'the Visual C++ Redistributable', 'a redistributable folder', 'the right question', 'right to left', 'file permissions and mode bits', 'the user grants access',
                     'Permission is hereby granted, free of charge', 'the correct way to distribute load', 'a touch may travel and still open a popup', 'the grant total'):
            files = self.edited()
            files['README.md'] = good_files()['README.md'] + '\n' + text + '\n'
            self.assertGreen(files, 'H5')

    def test_the_licence_texts_are_exempt_because_the_standard_text_says_distribute(self):
        files = self.edited()
        files['LICENSE'] = 'Redistribution and use in source and binary forms, with or without modification, are permitted.\n'
        files['NOTICE.md'] = 'Permission is hereby granted ... the right to distribute\n'
        self.assertGreen(files, 'H5')

    def test_the_checkers_own_source_and_tests_are_exempt(self):
        files = self.edited()
        files['tools/ci/check_public.py'] = 'the right to ship\n'
        files['tools/ci/test_check_public.py'] = 'the right to distribute\n'
        self.assertGreen(files, 'H5')

    def test_in_a_record_file_the_words_are_a_note_with_a_count(self):
        for record in ('DEVLOG.md', 'docs/parity-backlog.md', 'docs/planning/plan.md'):
            files = self.edited()
            files[record] = 'a\nthe right to ship\nb\nthe rights holder\n'
            found = self.findings(files, rules=['H5'])
            self.assertEqual(self.errors(found, 'H5'), [])
            notes = [f for f in self.by(found, cp.NOTE, 'H5') if f.path == record]
            self.assertEqual(len(notes), 1)
            self.assertIn('2 line(s)', notes[0].text)

    def test_in_a_file_only_the_owner_changes_the_words_are_an_owner_file_finding_with_the_lines(self):
        for owner in ('CLAUDE.md', 'docs/permission.md', '.gitignore'):
            files = self.edited()
            files[owner] = (files.get(owner) or '') + '\nline\nthe right to ship\n'
            found = self.findings(files, rules=['H5'])
            self.assertEqual(self.errors(found, 'H5'), [], owner)
            own = [f for f in self.by(found, cp.OWNER_FILE, 'H5') if f.path == owner]
            self.assertEqual(len(own), 1, owner)
            self.assertIn('not edited', own[0].text)

    def test_the_placeholder_line_is_allowed_in_security_md_alone(self):
        found = self.findings(self.edited(), rules=['H5'])
        self.assertEqual(self.errors(found, 'H5'), [])
        files = self.edited()
        files['README.md'] += '\n' + cp.PLACEHOLDER + '\n'
        found = self.assertRed(files, 'H5', 1)
        self.assertEqual(found[0].path, 'README.md')
        self.assertIn('belongs in SECURITY.md alone', found[0].text)

    def test_the_phrase_list_carries_what_this_step_found(self):
        names = {name for name, _ in cp.KEPT_OUT_PHRASES}
        for needed in ('whether the data may', 'may leave a machine', 'leave his machine', 'allowed to ship', 'may be shipped', 'right to ship', 'a doubt about the right', 'permission.md'):
            self.assertIn(needed, names)


class H6Ignore(SyntheticRepos):
    def test_the_line_with_its_block_and_no_negation_passes(self):
        self.assertGreen(good_files(), 'H6')
        files = self.edited()
        files['.gitignore'] = GITIGNORE.replace('reference/\n', '/reference/\n')
        self.assertGreen(files, 'H6')

    def test_the_line_gone_is_refused(self):
        files = self.edited()
        files['.gitignore'] = GITIGNORE.replace('reference/\n', '')
        self.assertRed(files, 'H6', 1)

    def test_the_line_commented_out_is_refused(self):
        files = self.edited()
        files['.gitignore'] = GITIGNORE.replace('\nreference/\n', '\n# reference/\n')
        self.assertRed(files, 'H6', 1)

    def test_the_comment_block_gone_is_refused(self):
        files = self.edited()
        files['.gitignore'] = 'reference/\nout/\n'
        found = self.assertRed(files, 'H6', 1)
        self.assertIn('comment block', found[0].text)

    def test_a_block_cut_to_a_stub_that_keeps_the_anchor_words_is_refused_by_its_size(self):
        files = self.edited()
        stub = '# DELIBERATELY EXCLUDED\n# DELIBERATELY UNANCHORED\n#\n#\n#\n'  # five comment lines: it still names both, and it is no longer the block
        files['.gitignore'] = stub + GITIGNORE[GITIGNORE.index('reference/\n'):]
        found = self.assertRed(files, 'H6', 1)
        self.assertIn('narrowed', found[0].text)
        self.assertIn('5 comment line(s)', found[0].text)

    def test_a_block_of_the_right_size_without_an_anchor_phrase_is_refused_by_name(self):
        for anchor in cp.H6_ANCHORS:
            files = self.edited()
            files['.gitignore'] = GITIGNORE.replace(anchor, 'something else')
            found = self.assertRed(files, 'H6', 1)
            self.assertIn('"%s"' % anchor, found[0].text)

    def test_a_block_at_the_floor_passes_so_that_rewording_a_line_of_the_real_one_does_not_turn_it_red(self):
        words = list(cp.H6_ANCHORS) + ['filler %d' % i for i in range(cp.H6_BLOCK_FLOOR - len(cp.H6_ANCHORS))]
        files = self.edited()
        files['.gitignore'] = '\n'.join('# ' + w for w in words) + '\nreference/\nout/\n'
        self.assertGreen(files, 'H6')
        files['.gitignore'] = '\n'.join('# ' + w for w in words[1:]) + '\nreference/\nout/\n'  # one line fewer than the floor
        self.assertRed(files, 'H6', 1)

    def test_a_negation_below_that_unignores_anything_under_reference_is_refused(self):
        for line in ('!reference/keep.txt', '!/reference/a/b', '!**/reference/x', '  !reference/x'):
            files = self.edited()
            files['.gitignore'] += line + '\n'
            self.assertRed(files, 'H6', 1)

    def test_a_negation_of_something_else_passes_and_a_missing_file_is_refused(self):
        files = self.edited()
        files['.gitignore'] += '!keep.txt\n'
        self.assertGreen(files, 'H6')
        self.assertRed(self.edited(_dot_gitignore=None), 'H6', 1)


class H7Required(SyntheticRepos):
    def test_each_required_file_missing_or_empty_is_refused(self):
        for path in cp.REQUIRED_FILES:
            files = self.edited()
            files.pop(path)
            found = self.errors(self.findings(files, rules=['H7']), 'H7')
            self.assertEqual([f.path for f in found], [path])
            files[path] = '  \n\n'
            found = self.errors(self.findings(files, rules=['H7']), 'H7')
            self.assertEqual([f.path for f in found], [path])
            self.assertIn('empty', found[0].text)

    def test_the_list_is_the_one_the_step_names(self):
        self.assertEqual(set(cp.REQUIRED_FILES), {'LICENSE', 'NOTICE.md', 'README.md', 'CONTRIBUTING.md', 'SECURITY.md', 'docs/platforms.md', 'docs/building.md'})

    def test_a_file_that_exists_but_is_not_tracked_does_not_count_until_it_is_asked_for(self):
        repo = self.make(self.edited(LICENSE=None))
        self.write(repo, {'LICENSE': 'text'})
        self.assertEqual([f.path for f in self.errors(cp.check(repo, rules=['H7'], version_check=VERSION_OK), 'H7')], ['LICENSE'])
        self.assertEqual(self.errors(cp.check(repo, include_untracked=True, rules=['H7'], version_check=VERSION_OK), 'H7'), [])


class H8Submodules(SyntheticRepos):
    def test_an_https_url_without_credentials_and_a_gitlink_pass(self):
        self.assertGreen(good_files(), 'H8')

    def test_a_url_that_is_not_https_is_refused(self):
        for url in ('git@github.com:someone/engine.git', 'http://github.com/someone/engine.git', 'ssh://git@github.com/x/y.git', 'git://github.com/x/y.git', '../engine'):
            files = self.edited(_dot_gitmodules='[submodule "engine"]\n\tpath = engine\n\turl = %s\n' % url)
            self.assertRed(files, 'H8', 1)

    def test_credentials_in_the_url_are_refused_and_a_port_is_not_a_credential(self):
        for url in ('https://user:token@github.com/someone/engine.git', 'https://token@github.com/someone/engine.git'):
            self.assertRed(self.edited(_dot_gitmodules='[submodule "engine"]\n\tpath = engine\n\turl = %s\n' % url), 'H8', 1)
        self.assertGreen(self.edited(_dot_gitmodules='[submodule "engine"]\n\tpath = engine\n\turl = https://example.org:8443/x/engine.git\n'), 'H8')

    def test_a_path_that_is_not_a_gitlink_is_refused(self):
        files = self.edited()
        files['engine/readme.txt'] = 'a plain folder where the module should be'
        found = self.assertRed(files, 'H8', 1, gitlink=None)
        self.assertIn('not a gitlink', found[0].text)

    def test_a_gitlink_with_no_gitmodules_is_refused(self):
        self.assertRed(self.edited(_dot_gitmodules=None), 'H8', 1)

    def test_no_submodule_at_all_passes(self):
        self.assertGreen(self.edited(_dot_gitmodules=None), 'H8', gitlink=None)


class H9Modes(SyntheticRepos):
    def test_scripts_executable_and_nothing_else_passes(self):
        self.assertGreen(good_files(), 'H9')

    def test_a_script_that_is_not_executable_is_refused(self):
        found = self.assertRed(good_files(), 'H9', 1, executable=())
        self.assertEqual(found[0].path, 'tools/build.sh')
        self.assertIn('100755', found[0].text)

    def test_another_file_that_is_executable_is_refused(self):
        files = self.edited()
        files['tools/run.py'] = 'print(1)\n'
        found = self.assertRed(files, 'H9', 1, executable=('tools/build.sh', 'tools/run.py'))
        self.assertEqual(found[0].path, 'tools/run.py')

    def test_a_script_not_added_yet_has_no_mode_to_check_and_is_not_refused(self):
        repo = self.make(good_files())
        self.write(repo, {'tools/new.sh': '#!/bin/sh\n'})
        self.assertEqual(self.errors(cp.check(repo, include_untracked=True, rules=['H9'], version_check=VERSION_OK), 'H9'), [])
        run_git(repo, 'add', 'tools/new.sh')
        self.assertEqual([f.path for f in self.errors(cp.check(repo, rules=['H9'], version_check=VERSION_OK), 'H9')], ['tools/new.sh'])

    def test_a_symbolic_link_is_a_note_and_the_gitlink_is_not_an_executable(self):
        found = self.findings(self.edited(), rules=['H9'], links={'docs/link.md': 'platforms.md'})
        self.assertEqual(self.errors(found, 'H9'), [])
        self.assertEqual([f.path for f in self.by(found, cp.NOTE, 'H9')], ['docs/link.md'])


class H10Links(SyntheticRepos):
    def readme(self, extra):
        return good_files()['README.md'] + '\n' + extra + '\n'

    def test_working_links_pass(self):
        extra = ('[a](docs/platforms.md) [b](./docs/building.md#building-it) [c](docs/) [d](<docs/platforms.md>) ![img](docs/platforms.md "title") [e](docs/building.md?plain=1) '
                 '[f](#run-it) [g](https://example.org/x) [h](mailto:a@b.c) [i](engine/README.md) [j](engine) [k](docs/platforms.md "a title") [l](%64ocs/platforms.md)\n\n[ref]: docs/platforms.md\n')
        self.assertGreen(self.edited(README_dot_md=self.readme(extra)), 'H10')

    def test_a_link_to_a_file_that_is_not_there_is_refused(self):
        found = self.assertRed(self.edited(README_dot_md=self.readme('[x](docs/missing.md)')), 'H10', 1)
        self.assertIn('docs/missing.md', found[0].text)

    def test_a_link_into_out_or_reference_is_refused_and_says_why(self):
        for target in ('out/levels/level0.tscn', 'reference/extracted/x', 'build/game/MagicPortals'):
            found = self.assertRed(self.edited(README_dot_md=self.readme('[x](%s)' % target)), 'H10', 1)
            self.assertIn('git ignores', found[0].text)

    def test_a_link_relative_to_the_file_resolves_from_its_folder(self):
        files = self.edited()
        files['docs/a.md'] = '[up](../README.md) [side](platforms.md) [sub](sub/b.md)\n'
        files['docs/sub/b.md'] = '[back](../a.md)\n'
        self.assertGreen(files, 'H10')
        files['docs/sub/b.md'] = '[back](a.md)\n'
        found = self.assertRed(files, 'H10', 1)
        self.assertEqual(found[0].path, 'docs/sub/b.md')

    def test_a_link_that_leaves_the_repository_is_refused(self):
        found = self.assertRed(self.edited(README_dot_md=self.readme('[x](../../etc/passwd)')), 'H10', 1)
        self.assertIn('leaves the repository', found[0].text)

    def test_a_link_in_code_is_not_a_link(self):
        extra = 'in code font `[x](docs/missing.md)` and\n\n```\n[y](docs/also-missing.md)\n```\n\n~~~text\n[z](nope.md)\n~~~\n\n``[w](nope2.md)``\n'
        self.assertGreen(self.edited(README_dot_md=self.readme(extra)), 'H10')

    def test_a_fragment_must_name_a_heading_of_its_target(self):
        self.assertRed(self.edited(README_dot_md=self.readme('[x](#no-such-heading)')), 'H10', 1)
        self.assertRed(self.edited(README_dot_md=self.readme('[x](docs/building.md#no-such)')), 'H10', 1)
        self.assertGreen(self.edited(README_dot_md=self.readme('[x](#local-setup-after-cloning) [y](docs/building.md#building-it) [z](#RUN-IT)')), 'H10')

    def test_headings_with_marks_and_repeats_have_the_anchors_github_gives(self):
        files = self.edited()
        files['docs/a.md'] = '# The `data` & *the* tools (2026)\n\n## Same\n\n## Same\n\n## Same\n'
        files['README.md'] = self.readme('[a](docs/a.md#the-data--the-tools-2026) [b](docs/a.md#same) [c](docs/a.md#same-1) [d](docs/a.md#same-2)')
        self.assertGreen(files, 'H10')
        files['README.md'] = self.readme('[e](docs/a.md#same-3)')
        self.assertRed(files, 'H10', 1)

    def test_a_reference_style_definition_is_read(self):
        self.assertRed(self.edited(README_dot_md=self.readme('[ref][1]\n\n[1]: docs/missing.md')), 'H10', 1)

    def test_a_file_that_is_not_markdown_is_not_read_for_links(self):
        files = self.edited()
        files['docs/notes.txt'] = '[x](docs/missing.md)'
        self.assertGreen(files, 'H10')


class H11Sections(SyntheticRepos):
    def test_a_reference_that_names_a_heading_passes(self):
        files = self.edited()
        files['tools/a.py'] = '# see README, Parity work, for the venv\n# (README, *Run it*) and (README.md, *Local setup after cloning*, steps 1-8) and README, Parity work);\n'
        self.assertGreen(files, 'H11')

    def test_a_quoted_title_that_names_a_heading_passes_in_straight_and_curly_quotes(self):
        files = self.edited()
        files['tools/a.py'] = '# (README, "Run it") and README.md, "Local setup after cloning", and (README, “Parity work”); also `README`, "Run it"\n'
        self.assertGreen(files, 'H11')

    def test_a_reference_to_a_heading_that_is_not_there_is_refused(self):
        for text in ('(README, *The data*)', '(README, The data)', 'README.md, *No such section*', '# README, Capture flags.', '`README.md`, *Elsewhere*'):
            files = self.edited()
            files['tools/a.py'] = text + '\n'
            found = self.assertRed(files, 'H11', 1)
            self.assertEqual(found[0].path, 'tools/a.py')

    def test_a_quoted_title_that_is_not_a_heading_is_refused_in_straight_and_curly_quotes(self):
        # the form tools/build_linux.sh had: (README, "The data"), a title the README has never had
        for text in ('# The data is not here (README, "The data")', 'README.md, "No such section"', '(README, “The data”)', '`README`, "Elsewhere", and more'):
            files = self.edited()
            files['tools/a.sh'] = text + '\n'
            found = self.assertRed(files, 'H11', 1)
            self.assertEqual(found[0].path, 'tools/a.sh')
            self.assertEqual(found[0].line, 1)

    def test_other_readmes_and_loose_words_are_not_references(self):
        files = self.edited()
        files['tools/a.py'] = 'engine/README.md, *Requirements*\nREADME, step 3\nthe README says it\nsee the README for more\ntools/package/README.md, *X*\n'
        files['tools/b.py'] = 'engine/README.md, "The title names the game"\nthe engine README, "The title names the game"\nthe engine\'s README, "Requirements"\nthe engine’s README, “X”\nREADME, "x"\n'
        self.assertGreen(files, 'H11')

    def test_in_claude_md_it_is_an_owner_file_finding_and_in_a_record_a_note(self):
        files = self.edited()
        files['CLAUDE.md'] = 'one\n(README, *Gone section*)\n'
        files['DEVLOG.md'] = '(README, *Old section*)\n'
        found = self.findings(files, rules=['H11'])
        self.assertEqual(self.errors(found, 'H11'), [])
        owner = self.by(found, cp.OWNER_FILE, 'H11')
        self.assertEqual([(f.path, f.line) for f in owner], [('CLAUDE.md', 2)])
        notes = self.by(found, cp.NOTE, 'H11')
        self.assertEqual([f.path for f in notes], ['DEVLOG.md'])

    def test_with_no_readme_the_rule_has_nothing_to_compare_with(self):
        files = self.edited(README_dot_md=None)
        files['tools/a.py'] = '(README, *Run it*)\n'
        self.assertGreen(files, 'H11')


class H12Version(SyntheticRepos):
    def test_the_version_check_failing_is_refused_with_its_words(self):
        found = self.errors(self.findings(good_files(), rules=['H12'], version_check=lambda repo: (1, 'VERSION: docs/x.md says 9.9.9\ncheck_version: 1 place(s) disagree')), 'H12')
        self.assertEqual(len(found), 1)
        self.assertIn('docs/x.md says 9.9.9', found[0].text)

    def test_the_version_check_passing_is_quiet(self):
        self.assertEqual(self.findings(good_files(), rules=['H12']), [])

    def test_the_default_check_runs_the_repositorys_own_script(self):
        for body, wanted in (('import sys\nprint("fine")\nsys.exit(0)\n', 0), ('import sys\nprint("VERSION: a place")\nsys.exit(1)\n', 1)):
            files = self.edited()
            files['tools/package/check_version.py'] = body
            repo = self.make(files)
            found = self.errors(cp.check(repo, rules=['H12']), 'H12')
            self.assertEqual(len(found), wanted)
        files = self.edited(tools__package__check_version_dot_py=None)
        repo = self.make(files)
        found = self.errors(cp.check(repo, rules=['H12']), 'H12')
        self.assertIn('does not exist', found[0].text)


class TheCommandLine(SyntheticRepos):
    def run_main(self, repo, *args):
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stderr(err):
            code = cp.main(['--repo', repo, '--account', ACCOUNT, '--rules', 'H1,H7'] + list(args), out=out)
        return code, out.getvalue(), err.getvalue()

    def test_exit_zero_when_nothing_is_an_error_and_one_when_something_is(self):
        code, out, _ = self.run_main(self.make(good_files()))
        self.assertEqual(code, 0, out)
        self.assertIn('summary:', out)
        files = self.edited()
        files['pic.png'] = 'x'
        code, out, _ = self.run_main(self.make(files))
        self.assertEqual(code, 1)
        self.assertIn('ERROR', out)
        self.assertIn('pic.png', out)

    def test_findings_that_are_not_errors_do_not_change_the_exit(self):
        files = self.edited()
        files['DEVLOG.md'] = 'C:\\Users\\bobsmith\\x\n'
        code, out, _ = self.run_main(self.make(files), '--rules', 'H4')
        self.assertEqual(code, 0, out)
        self.assertIn('RECORD-PATH', out)

    def test_the_table_lists_errors_first_and_has_the_columns_and_a_summary(self):
        files = self.edited()
        files['pic.png'] = 'x'
        files['DEVLOG.md'] = '/home/bobsmith/x\n'
        code, out, _ = self.run_main(self.make(files), '--rules', 'H1,H4')
        lines = out.split('\n')
        self.assertTrue(lines[0].startswith('CLASS'))
        self.assertTrue(lines[1].startswith('ERROR'))
        self.assertLess(out.index('ERROR'), out.index('RECORD-PATH'))
        self.assertRegex(out, r'summary: 1 ERROR, 1 RECORD-PATH')

    def test_list_rules_names_twelve(self):
        out = io.StringIO()
        self.assertEqual(cp.main(['--list-rules'], out=out), 0)
        self.assertEqual(len([l for l in out.getvalue().split('\n') if l.strip()]), 12)

    def test_an_unknown_rule_is_a_usage_error_not_a_pass(self):
        code, _, err = self.run_main(self.make(good_files()), '--rules', 'H77')
        self.assertEqual(code, cp.EXIT_UNREADABLE)

    def test_include_untracked_reads_files_not_added_yet_and_not_ignored(self):
        repo = self.make(self.edited(), commit=True)
        self.write(repo, {'new.png': 'x', 'out/ignored.png': 'x', '.gitignore': GITIGNORE})
        self.assertEqual(self.errors(cp.check(repo, rules=['H1'], accounts=[ACCOUNT]), 'H1'), [])
        found = self.errors(cp.check(repo, include_untracked=True, rules=['H1'], accounts=[ACCOUNT]), 'H1')
        self.assertEqual([f.path for f in found], ['new.png'])

    def test_a_clean_clone_gives_the_same_answer_as_the_checkout(self):
        files = self.edited()
        files['pic.png'] = 'x'
        repo = self.make(files)
        clone = tempfile.mkdtemp(prefix='check_public_test_')
        self.folders.append(clone)
        shutil.rmtree(clone)
        run_git(os.path.dirname(clone), 'clone', '-q', repo, clone)
        a = sorted((f.cls, f.rule, f.path, f.line) for f in cp.check(repo, accounts=[ACCOUNT], version_check=VERSION_OK) if f.cls != cp.NOTE)
        b = sorted((f.cls, f.rule, f.path, f.line) for f in cp.check(clone, accounts=[ACCOUNT], version_check=VERSION_OK) if f.cls != cp.NOTE)
        self.assertEqual(a, b)


class TheHistory(SyntheticRepos):
    def history(self, repo, **kw):
        return cp.run_history(repo, accounts=[ACCOUNT], **kw)

    def test_a_clean_history_has_no_finding_and_counts_its_commits(self):
        repo = self.make(good_files())
        self.commit_more(repo, {'docs/more.md': 'text\n'})
        result = self.history(repo)
        self.assertEqual([f for f in result.findings if f.cls == cp.HISTORY], [])
        self.assertEqual(result.commits, 2)
        self.assertEqual(result.authors, {'author@example.invalid'})
        self.assertEqual(result.not_run, [])

    def test_a_picture_added_and_removed_again_is_found_by_its_name(self):
        repo = self.make(good_files())
        self.commit_more(repo, {'art/pic.png': 'x'}, message='add')
        self.commit_more(repo, {}, remove=['art/pic.png'], message='remove')
        found = [f for f in self.history(repo).findings if f.cls == cp.HISTORY]
        self.assertEqual([(f.rule, f.path) for f in found], [('H1', 'art/pic.png')])
        self.assertIn('commit', found[0].text)
        self.assertEqual(cp.check(repo, rules=['H1'], accounts=[ACCOUNT]), [])  # the tip is clean: only the history is not

    def test_a_key_in_a_commit_no_later_commit_holds_is_found(self):
        repo = self.make(good_files())
        self.commit_more(repo, {'docs/k.txt': '-----BEGIN RSA PRIVATE KEY-----\nabc\n'})
        self.commit_more(repo, {}, remove=['docs/k.txt'])
        found = [f for f in self.history(repo).findings if f.cls == cp.HISTORY]
        self.assertEqual([(f.rule, f.path) for f in found], [('H3', 'docs/k.txt')])
        self.assertIn('first in commit', found[0].text)
        self.assertIn('last in ', found[0].text)

    def test_a_path_of_a_pc_is_found_outside_the_records_and_counted_inside_them(self):
        repo = self.make(good_files())
        self.commit_more(repo, {'docs/g.md': 'C:\\Users\\bobsmith\\x\n', 'DEVLOG.md': '/home/bobsmith/y\n' + ACCOUNT + '\n'})
        result = self.history(repo)
        found = [f for f in result.findings if f.cls == cp.HISTORY]
        self.assertEqual([(f.rule, f.path) for f in found], [('H4', 'docs/g.md')])
        self.assertTrue(any(f.cls == cp.NOTE and 'record' in f.text for f in result.findings))

    def test_the_account_is_found_in_a_past_version_of_a_file(self):
        repo = self.make(good_files())
        self.commit_more(repo, {'docs/g.md': 'owner ' + ACCOUNT + ' was here\n'})
        self.commit_more(repo, {'docs/g.md': 'clean now\n'})
        found = [f for f in self.history(repo).findings if f.cls == cp.HISTORY]
        self.assertEqual([(f.rule, f.path) for f in found], [('H4', 'docs/g.md')])
        self.assertNotIn(ACCOUNT, found[0].text)

    def test_the_account_glued_to_other_letters_is_found_in_the_past_and_inside_an_address_is_not(self):
        # the same two rules as the tip of the tree: five letters or more are a substring search, and an address of a person that starts with the name is the e-mail note's business
        repo = self.make(good_files())
        self.commit_more(repo, {'docs/g.md': 'built by %s2 in the lab\n' % ACCOUNT, 'docs/h.md': 'contact %srest99@mail.example.net\n' % ACCOUNT})
        self.commit_more(repo, {'docs/g.md': 'clean now\n', 'docs/h.md': 'clean now\n'})
        found = [f for f in self.history(repo).findings if f.cls == cp.HISTORY]
        self.assertEqual([(f.rule, f.path) for f in found], [('H4', 'docs/g.md')])

    def test_a_binary_and_a_large_blob_in_the_past_are_found(self):
        repo = self.make(good_files())
        self.commit_more(repo, {'data/b.dat': b'a\0b', 'data/big.txt': 'x' * 300})
        self.commit_more(repo, {}, remove=['data/b.dat', 'data/big.txt'])
        with mock.patch.object(cp, 'MOST_BYTES', 200):
            found = {(f.rule, f.path) for f in self.history(repo).findings if f.cls == cp.HISTORY and f.path.startswith('data/')}
        self.assertEqual(found, {('H2', 'data/b.dat'), ('H2', 'data/big.txt')})

    def test_a_commit_on_a_branch_that_is_not_main_is_read(self):
        repo = self.make(good_files())
        run_git(repo, 'checkout', '-q', '-b', 'wip/experiment')
        self.commit_more(repo, {'art/pic.png': 'x'}, message='on the branch')
        run_git(repo, 'checkout', '-q', '-')
        found = [f for f in self.history(repo).findings if f.cls == cp.HISTORY]
        self.assertEqual([(f.rule, f.path) for f in found], [('H1', 'art/pic.png')])

    def test_a_pattern_past_its_budget_is_reported_not_run_with_its_time_and_never_silently(self):
        repo = self.make(good_files())
        result = self.history(repo, budget=-1.0)
        self.assertTrue(result.not_run)
        self.assertIn('NOT RUN', ' '.join(result.not_run))
        self.assertIn('blobs', result.not_run[0])
        out = io.StringIO()
        code = cp.main(['--repo', repo, '--rules', 'H1', '--history', '--history-budget=-1'], out=out)
        self.assertEqual(code, 1)
        self.assertIn('NOT RUN', out.getvalue())

    def test_history_mode_exits_one_on_a_historical_finding_and_zero_when_there_is_none(self):
        repo = self.make(good_files())
        out = io.StringIO()
        self.assertEqual(cp.main(['--repo', repo, '--rules', 'H1', '--account', ACCOUNT, '--history'], out=out), 0, out.getvalue())
        self.assertIn('HISTORY: 1 commit(s)', out.getvalue())
        self.commit_more(repo, {'art/pic.png': 'x'})
        self.commit_more(repo, {}, remove=['art/pic.png'])
        out = io.StringIO()
        self.assertEqual(cp.main(['--repo', repo, '--rules', 'H1', '--account', ACCOUNT, '--history'], out=out), 1)
        self.assertIn('HISTORY', out.getvalue())

    def test_the_files_of_the_checker_are_not_read_for_their_patterns(self):
        repo = self.make(good_files())
        self.commit_more(repo, {'tools/ci/check_public.py': '-----BEGIN RSA PRIVATE KEY-----\n/home/bobsmith/x\n'})
        self.assertEqual([f for f in self.history(repo).findings if f.cls == cp.HISTORY], [])


class TheReadme(unittest.TestCase):
    """The README says one thing about permission (the owner's sentence) and nothing else about it, and it keeps the headings the rest of the repository points at."""

    @classmethod
    def setUpClass(cls):
        with open(os.path.join(REPO, 'README.md'), encoding='utf-8') as f:
            cls.text = f.read()

    def test_the_one_permitted_sentence_is_there_once_and_the_word_permission_is_nowhere_else(self):
        self.assertEqual(len([l for l in self.text.split('\n') if hashlib.sha256(l.strip().encode('utf-8')).hexdigest() == PERMITTED_DIGEST]), 1)
        prose = self.text.replace('[Permission](#permission)', '').replace('\n## Permission\n', '\n')  # a link to the section and the section's heading are not sentences
        self.assertEqual(len(re.findall('permission', prose, re.I)), 1)

    def test_the_permission_section_names_the_facts_and_the_front_links_to_it(self):
        section = self.text.split('\n## Permission\n')[1].split('\n## ')[0]
        for needle in ('no content of the original game', '`reference/`', '`out/`', '--root', 'docs/building.md', 'Asantee Games', 'Andre Santee'):
            self.assertIn(needle, section)
        self.assertIn('(#permission)', self.text.split('\n## Status')[0])

    def test_the_headings_other_files_point_at_are_still_there_in_their_order(self):
        headings = [l[3:] for l in self.text.split('\n') if l.startswith('## ')]
        names = ('Permission', 'Status', 'The Supersonic port', 'How it fits together', 'Local setup after cloning', 'Run it', 'Parity work', 'Docs')
        for name in names:
            self.assertIn(name, headings)
        order = [headings.index(n) for n in names]
        self.assertEqual(order, sorted(order))

    def test_the_front_has_the_sections_a_stranger_looks_for_before_the_developer_part(self):
        front = self.text.split('\n## For developers and contributors')[0]
        for name in ('Platforms', 'How to play', 'Controls', 'Build it', 'What is where', 'Credits', 'Licence', 'Contributing'):
            self.assertIn('\n## %s\n' % name, front)
        self.assertLess(len(front.split('\n')), 125, 'the front is meant to be read in a minute')

    def test_the_platform_table_names_all_five_platforms_and_says_what_each_status_is_made_of(self):
        rows = [l for l in self.text.split('\n') if l.startswith('| ') and l.split('|')[1].strip() in ('Windows', 'Linux', 'Android', 'macOS', 'iOS')]
        self.assertEqual([r.split('|')[1].strip() for r in rows], ['Windows', 'Linux', 'Android', 'macOS', 'iOS'])
        for row in rows:
            self.assertRegex(row, r'\*\*(Verified|Built)')


class TheRepositoryItself(unittest.TestCase):
    def test_this_repository_has_no_error_finding(self):
        if not os.path.isdir(os.path.join(REPO, '.git')) and not os.path.isfile(os.path.join(REPO, '.git')):
            self.skipTest('this is not a git work tree: the run was NOT made')
        findings = cp.check(REPO, include_untracked=True)
        errors = [f for f in findings if f.cls == cp.ERROR]
        self.assertEqual(errors, [], '\n'.join('%s %s %s' % (f.rule, f.where(), f.text) for f in errors))


if __name__ == '__main__':
    unittest.main()
