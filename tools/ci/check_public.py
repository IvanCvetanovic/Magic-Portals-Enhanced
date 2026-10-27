#!/usr/bin/env python3
"""The public-readiness checker (planning step 210, PORT-12): what a stranger finds in the tracked files of this repository, read before the repository is shown to anyone.

    python tools/ci/check_public.py [--repo R] [--include-untracked] [--rules H1,H4,...] [--account NAME]... [--history] [--history-budget SECONDS] [--list-rules]

    exit 0   no finding of class ERROR (and, with --history, none of class HISTORY)
    exit 1   at least one
    exit 2   the repository could not be read (not a git work tree, git missing, a bad option)

IT READS THE FILES GIT TRACKS (`git ls-files`), so it gives the same answer in a clean clone, where out/, reference/ and build/ do not exist. `--include-untracked` adds the files
a developer has made and not yet added (they are not ignored ones), for a run before a commit; the gate runs do without it. It reads and prints; it changes nothing and fetches nothing.

THE CLASSES. ERROR (exit 1) is a defect of the files a stranger reads. RECORD-PATH is a path of the owner's PC inside a RECORD file (the diary, the backlog, the planning records, CLAUDE.md):
counted per file and listed by line, never an error, because a record says what was true when it was written. OWNER-FILE is a finding inside a file only the owner changes
(CLAUDE.md, docs/permission.md, .gitignore, .gitmodules): reported with its lines, never edited by this tool or by the step that wrote it. NOTE is a fact the owner should see (an
allowed placeholder, an allow-listed file, a record file's words). HISTORY (only with --history) is something in a past commit; nothing is removed by anyone for it.

THE RULES, by id (each has a test that passes a repository and fails an edited copy of it: tools/ci/test_check_public.py):
  H1  no tracked name has an extension of EXTENSIONS: pictures, sounds, original-game data files, binaries, packages, keys and keystores.
  H2  no tracked file holds a NUL byte (a binary), and none is larger than MOST_BYTES; the ten largest are printed as NOTEs.
  H3  no key or token shaped text (a PEM private key, GitHub and AWS tokens, a Slack token, a keystore password given as a literal) in any file but this tool's own source and tests,
      and the allow-listed files of ALLOWED_SECRETS (each with its reason, printed).
  H4  no path of the owner's PC: a Windows user folder (with / or \\), /home/<name>, /Users/<name>, /mnt/<drive>, and the name of the account that runs the check (read at run time,
      never written in a file; --account adds names): a name of five letters or more anywhere in a word, one of four letters as a whole word; a match that lies wholly inside an e-mail
      address of a person is the e-mail note instead (a maintainer's contact line starts with the name), a match beside it or in a path is a hit. In a RECORD file the same hits are RECORD-PATH. A file that probes for the name is allow-listed BY NAME (ALLOWED_PROBES).
  H5  none of the phrases of KEPT_OUT_PHRASES, in any file but the RECORD files, this tool's source and tests, and LICENSE and
      NOTICE.md (the standard licence texts say "distribute"). In an OWNER-FILE the hits are OWNER-FILE findings. The ONE placeholder line SECURITY.md carries is allowed there
      (PLACEHOLDER) and nowhere else.
  H6  .gitignore still holds the reference/ line and the comment block above it, and no negation line that unignores anything under reference/.
  H7  the files a stranger expects exist and are not empty (REQUIRED_FILES).
  H8  .gitmodules has only https URLs without credentials, and each submodule's path is a gitlink in the index.
  H9  every .sh file is mode 100755 and no other tracked file is executable.
  H10 every relative link of every tracked .md file resolves to a tracked file or folder (a link into out/ or reference/ is therefore a finding), and every #fragment to a heading.
  H11 every `README, <Section>` style reference names a heading that exists in README.md (in CLAUDE.md an unresolved one is OWNER-FILE; in a record it is a NOTE).
  H12 tools/package/check_version.py exits 0: the one version is written the same in every place.

--history reads every commit reachable from any ref: H1 on every name ever added, H2 on every blob, H3 and H4 on every blob (H4 not in the RECORD files' blobs, which are counted), each
pattern timed; a pattern that is not finished within --history-budget prints NOT RUN with the time it took and the number of blobs it had read, never silence.

Standard library only; git is run as a program.
"""
from __future__ import annotations

import argparse
import os
import posixpath
import re
import subprocess
import sys
import time
import urllib.parse
from collections import defaultdict

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)

from running_account import (ACCOUNT_MIN_LETTERS, ACCOUNT_SUBSTRING_LETTERS, GENERIC_NAMES, account_regex, running_accounts, searchable_accounts)  # noqa: E402,F401  (the account that runs the check)

EXIT_OK, EXIT_FINDING, EXIT_UNREADABLE = 0, 1, 2

# ---- classes -------------------------------------------------------------------------------------------------------------------------------
ERROR, RECORD_PATH, OWNER_FILE, NOTE, HISTORY = 'ERROR', 'RECORD-PATH', 'OWNER-FILE', 'NOTE', 'HISTORY'
CLASS_ORDER = (ERROR, HISTORY, OWNER_FILE, RECORD_PATH, NOTE)

# ---- the decisions, as tables ---------------------------------------------------------------------------------------------------------------
# Files that say what was true when they were written. Paths of the owner's PC in them are counted and listed, not errors; a stranger reads them as history.
RECORD_FILES = ('DEVLOG.md', 'docs/parity-backlog.md', 'CLAUDE.md')
RECORD_PREFIXES = ('docs/planning/',)
# Files only the owner changes. A finding in one is reported with its lines and is never edited by this tool or by the step that wrote it.
OWNER_FILES = ('CLAUDE.md', 'docs/permission.md', '.gitignore', '.gitmodules')
# This tool's own source and tests hold the patterns they look for.
SELF_FILES = ('tools/ci/check_public.py', 'tools/ci/test_check_public.py')
# The standard licence texts say "distribute" and "redistribution"; the rights-wording rule does not apply to them.
LICENCE_FILES = ('LICENSE', 'NOTICE.md')

# H1. The list the step names; compared in lower case, with the last dot-separated part of the name.
EXTENSIONS = frozenset('''png bmp jpg jpeg gif webp ico icns mp3 ogg wav esc ent enml par fnt pvr pkm exe dll lib obj so a o apk aab dex jks keystore idsig p12 pfx key pem
zip jar class gz tar tga dds psd ttf otf'''.split())
# H2. The largest tracked file today is 5.4 MB (the planning record); 8 MiB leaves room for it to grow and refuses anything that is not text a person wrote.
MOST_BYTES = 8 * 1024 * 1024
LARGEST_SHOWN = 10

# H3. (id, regex, what it is). The keystore password option takes a value that is not a reference (file:, env:, stdin) and not a placeholder.
SECRET_PATTERNS = (
    ('pem-private-key', re.compile(r'-----BEGIN (?:[A-Z0-9]+ )*PRIVATE KEY(?: BLOCK)?-----'), 'a PEM private key'),
    ('github-token', re.compile(r'\b(?:github_pat_[A-Za-z0-9_]{20,}|gh[pousr]_[A-Za-z0-9]{30,})'), 'a GitHub token'),
    ('aws-access-key', re.compile(r'\b(?:AKIA|ASIA)[0-9A-Z]{16}\b'), 'an AWS access key id'),
    ('slack-token', re.compile(r'\bxox[abposr]-[A-Za-z0-9-]{10,}'), 'a Slack token'),
    ('keystore-password', re.compile(r'(?:--?(?:storepass|keypass|ks-pass|ks-key-pass|key-pass|keystore-pass|store-pass))(?:[ \t]+|=)(?P<v>\S+)'), 'a keystore password written as a literal'),
)


def keystore_literal(token):
    """Whether the token after a keystore password option is a password written out. `pass:x` is; `file:`, `env:`, `stdin`, a shell variable, a placeholder in angle brackets,
    a Windows variable and an ellipsis are where a password comes from, not the password."""
    token = token.strip('"\'')
    if token.startswith('pass:'):
        token = token[5:]
    elif token.startswith(('file:', 'env:', 'stdin')):
        return False
    return bool(re.fullmatch(r'[A-Za-z0-9_!#^&*+=.-]{3,}', token)) and not token.startswith('...')


# A literal that is not a secret, by file, with the reason that is printed (a NOTE) whenever it is the reason a file is let through.
ALLOWED_SECRETS = (
    ('tools/build_android.sh', 'keystore-password', "Android's published debug keystore: its password is public, the same on every machine, and the key signs a debug APK only"),
    ('tools/package/android_package.py', 'keystore-password', "Android's published debug keystore (see build_android.sh): the same public password"),
)

# H4. A name component starts with a letter or digit; these are folders every Windows, Linux or macOS system or CI runner has, or placeholders in a sentence.
WINDOWS_USER = re.compile(r'(?<![A-Za-z0-9_])[A-Za-z]:[\\/]+Users[\\/]+(?P<name>[A-Za-z0-9][A-Za-z0-9._-]+)')
HOME_USER = re.compile(r'(?<![A-Za-z0-9_.-])/(?P<kind>home|Users)/(?P<name>[A-Za-z0-9][A-Za-z0-9._-]+)')
MOUNT_DRIVE = re.compile(r'(?<![A-Za-z0-9_.-])/mnt/[a-z](?![A-Za-z0-9_-])')
# Files that probe for the account's name on purpose, by name, with the reason that is printed.
ALLOWED_PROBES = (
    ('tests/test_mp_applelayout.cpp', 'a test that the bundle layout code holds no account name asks for the name it must not find'),
)

# H5. The phrases the project keeps out of its files, built from the wording planning step 210 found and rewrote. Whitespace between words is any run of white space or a line break plus a comment mark.
_SP = r'[\s*#/>|-]+'
KEPT_OUT_PHRASES = tuple((name, re.compile(text.replace(' ', _SP), re.I)) for name, text in (
    ('data may travel', r'\b(?:data|content|assets?|files?|game|package|archive|bundle)s? (?:may|can|could) travel\b'),
    ('may leave a machine', r"\bmay leave (?:his|her|the owner'?s?|my|our|the) machine\b"),
    ('leave his machine', r"\bleave (?:his|her|the owner'?s?) machine\b"),
    ('whether the data may', r"\bwhether the (?:game'?s |original'?s |owner'?s )?(?:data|content|assets?|game) (?:may|can|is allowed|are allowed)\b"),
    ('allowed to ship', r'\b(?:allowed|permitted|entitled|free) to (?:ship|distribute|redistribute|publish)\b'),
    ('may be shipped', r'\bmay be (?:shipped|distributed|redistributed)\b'),
    ('may ship', r'\bmay (?:ship|distribute|redistribute) (?:the|it|them|this|that|any|its|his)\b'),
    ('right to ship', r'\bri(?:ght|ghts) to (?:ship|distribute|redistribute|reuse|publish)\b'),
    ('a doubt about the right', r'\bdoubt about the right\b'),
    ('redistribution', r'\bredistribut(?:e|es|ed|ing|ion|ions)\b'),
    ('permission does not cover', r'\bpermission (?:covers|does not cover|doesn.t cover|granted|is granted|record)\b'),
    ('no permission to', r'\b(?:no|not have|without|lacks?) (?:the )?permission to\b'),
    ('permission.md', r'\bpermission\.md\b'),
    ('rights holder', r'\bri(?:ght|ghts)(?:\s|-)?holders?\b'),
    ('the grant', r'\b(?:the|this|that|a|written|rights) grant (?:of|and|is|was|covers?|text|record)\b'),
    ('rights grant', r'\bri(?:ght|ghts) grant\b'),
    ('legal basis', r'\blegal basis\b'),
    ('rights caveat', r'\brights (?:caveat|question)\b'),
    ('written permission', r'\bwritten permission\b'),
    ('permission to rebuild', r'\bpermission to (?:rebuild|reuse|remake|redistribute|distribute|ship)\b'),
))
# The ONE placeholder line SECURITY.md carries until the owner adds a contact. The file is the only place it may be.
PLACEHOLDER = 'OWNER TO ADD: the maintainer\'s public contact for security reports'
PLACEHOLDER_FILE = 'SECURITY.md'

# H6: the comment block above .gitignore's reference/ line. CLAUDE.md says the line and its block must not be removed or narrowed, so the rule asks for the block's two anchor phrases and for
# a size near the real one: the block is 30 comment lines today, and a floor a little under that lets a line be reworded and turns a block cut to a stub red.
H6_ANCHORS = ('DELIBERATELY EXCLUDED', 'DELIBERATELY UNANCHORED')
H6_BLOCK_LINES = 30
H6_BLOCK_FLOOR = 25

# H7
REQUIRED_FILES = ('LICENSE', 'NOTICE.md', 'README.md', 'CONTRIBUTING.md', 'SECURITY.md', 'docs/platforms.md', 'docs/building.md')

# H3/H4/H5 read text; these are bytes, not text a person wrote.
TEXT_ENCODINGS = ('utf-8', 'utf-16')

VERSION_SCRIPT = 'tools/package/check_version.py'


class Finding:
    __slots__ = ('cls', 'rule', 'path', 'line', 'text')

    def __init__(self, cls, rule, path, line, text):
        self.cls, self.rule, self.path, self.line, self.text = cls, rule, path, line, text

    def where(self):
        return '%s:%d' % (self.path, self.line) if self.path and self.line else (self.path or '-')

    def __repr__(self):
        return 'Finding(%s %s %s %s)' % (self.cls, self.rule, self.where(), self.text[:60])


class GitError(Exception):
    pass


# ---- git ------------------------------------------------------------------------------------------------------------------------------------

def git(repo, *args, check=True, binary=False):
    """Runs git in `repo` with the user's own configuration out of the way, so that the same tree gives the same answer on every machine."""
    env = dict(os.environ)
    env['GIT_OPTIONAL_LOCKS'] = '0'
    cmd = ['git', '-c', 'core.quotepath=false', '-c', 'core.autocrlf=false', '-C', repo] + list(args)
    try:
        done = subprocess.run(cmd, capture_output=True, env=env)
    except OSError as e:
        raise GitError('git could not be run: %s' % e)
    if check and done.returncode != 0:
        raise GitError('git %s failed (exit %d): %s' % (' '.join(args[:2]), done.returncode, done.stderr.decode('utf-8', 'replace').strip()))
    return done.stdout if binary else done.stdout.decode('utf-8', 'replace')


def list_entries(repo, include_untracked):
    """[(mode, path)] of the index: mode as git writes it (100644, 100755, 120000, 160000). Untracked files, when asked for, have the mode '?': they are read, and H9 has no mode to check of them."""
    raw = git(repo, 'ls-files', '-z', '-s', binary=True)
    entries = []
    for record in raw.split(b'\0'):
        if not record:
            continue
        meta, _, path = record.partition(b'\t')
        entries.append((meta.split()[0].decode(), path.decode('utf-8', 'replace')))
    if include_untracked:
        have = {p for _, p in entries}
        for path in git(repo, 'ls-files', '-z', '--others', '--exclude-standard', binary=True).decode('utf-8', 'replace').split('\0'):
            if path and path not in have:
                entries.append(('?', path))  # not in the index yet: it has no mode to check (H9 skips it), and it is read like any other file
    return sorted(entries, key=lambda e: e[1])


class Tree:
    """The files of a repository as a rule reads them: names, modes, and the bytes of each file from the work tree (cached)."""

    def __init__(self, repo, entries):
        self.repo = repo
        self.entries = entries
        self.modes = dict((p, m) for m, p in entries)
        self.gitlinks = sorted(p for m, p in entries if m == '160000')
        self.files = sorted(p for m, p in entries if m != '160000')
        self.fileset = set(self.files)
        self._bytes = {}
        self._dirs = None

    def has(self, path):
        return path in self.fileset

    def under_gitlink(self, path):
        return any(path == g or path.startswith(g + '/') for g in self.gitlinks)

    def is_dir(self, path):
        if self._dirs is None:
            self._dirs = {''}
            for p in self.files + self.gitlinks:
                parts = p.split('/')
                for i in range(1, len(parts)):
                    self._dirs.add('/'.join(parts[:i]))
                if p in self.gitlinks:
                    self._dirs.add(p)
        return path in self._dirs

    def raw(self, path):
        if path not in self._bytes:
            try:
                with open(os.path.join(self.repo, *path.split('/')), 'rb') as f:
                    self._bytes[path] = f.read()
            except OSError:
                self._bytes[path] = None
        return self._bytes[path]

    def text(self, path):
        """The file as text, or None when it is absent, a binary (a NUL byte that is not UTF-16's) or not decodable."""
        data = self.raw(path)
        if data is None:
            return None
        return decode_text(data)


def decode_text(data):
    if data.startswith((b'\xff\xfe', b'\xfe\xff')):
        try:
            return data.decode('utf-16')
        except UnicodeDecodeError:
            return None
    if b'\0' in data:
        return None
    try:
        return data.decode('utf-8-sig')
    except UnicodeDecodeError:
        return data.decode('latin-1')


def is_record(path):
    return path in RECORD_FILES or path.startswith(RECORD_PREFIXES)


def is_self(path):
    return path in SELF_FILES


def extension_of(path):
    base = posixpath.basename(path)
    return base.rsplit('.', 1)[1].lower() if '.' in base.lstrip('.') else ''


def line_of(text, offset):
    return text.count('\n', 0, offset) + 1


def numbers(lines, shown=80):
    lines = sorted(set(lines))
    return ', '.join(map(str, lines[:shown])) + (' ... (%d more)' % (len(lines) - shown) if len(lines) > shown else '')


# ---- the rules ------------------------------------------------------------------------------------------------------------------------------

class Context:
    def __init__(self, tree, accounts, version_check, readme_headings=None):
        self.tree = tree
        self.accounts = accounts
        self.version_check = version_check


def rule_h1(ctx):
    out = []
    for path in ctx.tree.files:
        ext = extension_of(path)
        if ext in EXTENSIONS:
            out.append(Finding(ERROR, 'H1', path, 0, 'a tracked file with the extension .%s (a picture, sound, binary, package, key or game data file is never committed)' % ext))
    return out


def rule_h2(ctx):
    out, sizes = [], []
    for path in ctx.tree.files:
        data = ctx.tree.raw(path)
        if data is None:
            continue
        sizes.append((len(data), path))
        if len(data) > MOST_BYTES:
            out.append(Finding(ERROR, 'H2', path, 0, '%d bytes: larger than the %d a tracked file may be' % (len(data), MOST_BYTES)))
        if b'\0' in data and not data.startswith((b'\xff\xfe', b'\xfe\xff')):
            out.append(Finding(ERROR, 'H2', path, 0, 'holds a NUL byte at offset %d: a binary file' % data.index(b'\0')))
    for size, path in sorted(sizes, reverse=True)[:LARGEST_SHOWN]:
        out.append(Finding(NOTE, 'H2', path, 0, 'one of the %d largest files: %d bytes' % (LARGEST_SHOWN, size)))
    return out


def rule_h3(ctx):
    out, letters = [], defaultdict(list)
    for path in ctx.tree.files:
        if is_self(path):
            continue
        text = ctx.tree.text(path)
        if not text:
            continue
        for name, regex, what in SECRET_PATTERNS:
            for m in regex.finditer(text):
                if name == 'keystore-password' and not keystore_literal(m.group('v')):
                    continue
                reason = next((why for p, n, why in ALLOWED_SECRETS if p == path and n == name), None)
                line = line_of(text, m.start())
                if reason:
                    letters[(path, name)].append(line)
                else:
                    out.append(Finding(ERROR, 'H3', path, line, 'looks like %s (%s)' % (what, name)))
    for (path, name), lines in sorted(letters.items()):
        reason = next(why for p, n, why in ALLOWED_SECRETS if p == path and n == name)
        out.append(Finding(NOTE, 'H3', path, 0, 'allow-listed %s at line(s) %s: %s' % (name, numbers(lines), reason)))
    return out


def owner_path_hits(text, account):
    """[(line, what)] of the owner-path patterns in a text. The account's name is reported as the account, never spelled."""
    hits = []
    for m in WINDOWS_USER.finditer(text):
        if m.group('name').lower() not in GENERIC_NAMES:
            hits.append((line_of(text, m.start()), 'a Windows user folder'))
    for m in HOME_USER.finditer(text):
        if m.group('name').lower() not in GENERIC_NAMES:
            hits.append((line_of(text, m.start()), 'a /%s/ folder' % m.group('kind')))
    for m in MOUNT_DRIVE.finditer(text):
        hits.append((line_of(text, m.start()), 'a Windows drive mounted into WSL'))
    if account is not None:
        for m in account_matches(text, account):
            hits.append((line_of(text, m.start()), "the running account's name"))
    return hits


EMAIL = re.compile(r'[A-Za-z0-9._%+-]+@(?P<domain>[A-Za-z0-9-]+(?:\.[A-Za-z0-9-]+)*\.[A-Za-z]{2,})')
NOT_AN_ADDRESS = re.compile(r'(?:^|\.)(?:invalid|example|test|localhost)$|^example\.(?:com|org|net)$|(?:^|\.)noreply\.github\.com$|^github\.com$', re.I)


# `AppIcon60x60@2x.png` is a file name, not an address: a "top-level domain" that is a file extension makes the match one.
FILE_EXTENSION_TLDS = frozenset('png jpg jpeg gif ico icns svg md txt py sh json yml yaml cpp hpp h c cc cmake plist xml spv mm kt java gradle html css js ts'.split())


def person_addresses(text):
    """The e-mail addresses of a person in a text (not a placeholder at an example domain, not git@github.com, not a file name such as icon@2x.png), as regular expression matches."""
    return [m for m in EMAIL.finditer(text)
            if not NOT_AN_ADDRESS.search(m.group('domain')) and m.group('domain').rsplit('.', 1)[-1].lower() not in FILE_EXTENSION_TLDS]


def email_lines(text):
    """The lines of a text that hold an e-mail address of a person."""
    return sorted({line_of(text, m.start()) for m in person_addresses(text)})


def account_matches(text, account):
    """The matches of the running account's name (the regular expression of running_account.account_regex) in a text, except a match that lies wholly inside an e-mail address of a
    person. WHY: the name is searched anywhere in a word when it is long (five letters or more), and a maintainer's public contact address commonly starts with it; that address is
    reported once, by the e-mail note of H4, as a deliberate publication, and is not a path or a name of the owner's PC that got in by accident. A match beside an address, or in a path,
    is still a match. Only the checker does this: a built program (release.py) is refused for the name wherever it stands."""
    spans = None
    for m in account.finditer(text):
        if spans is None:
            spans = [(a.start(), a.end()) for a in person_addresses(text)]
        if not any(start <= m.start() and m.end() <= end for start, end in spans):
            yield m


def rule_h4(ctx):
    out = []
    account = account_regex(ctx.accounts)
    for path in ctx.tree.files:
        if is_self(path):
            continue
        text = ctx.tree.text(path)
        if not text:
            continue
        mail = email_lines(text)
        if mail:
            out.append(Finding(NOTE, 'H4', path, 0, 'an e-mail address of a person at %d line(s) (%s): public once the repository is, as it already is in every commit (not printed)'
                               % (len(mail), numbers(mail))))
        hits = owner_path_hits(text, account)
        if not hits:
            continue
        probe = next((why for p, why in ALLOWED_PROBES if p == path), None)
        if probe:
            out.append(Finding(NOTE, 'H4', path, 0, 'allow-listed by name, %d hit(s) at line(s) %s: %s' % (len(hits), numbers([l for l, _ in hits]), probe)))
        elif is_record(path):
            kinds = sorted({k for _, k in hits})
            out.append(Finding(RECORD_PATH, 'H4', path, 0, '%d line(s) with %s: lines %s' % (len({l for l, _ in hits}), ', '.join(kinds), numbers([l for l, _ in hits]))))
        else:
            per_line = defaultdict(set)
            for line, kind in hits:
                per_line[line].add(kind)
            for line, kinds in sorted(per_line.items()):
                out.append(Finding(ERROR, 'H4', path, line, ' and '.join(sorted(kinds)) + " (a path or name of the owner's PC)"))
    return out


def rule_h5(ctx):
    out = []
    for path in ctx.tree.files:
        if is_self(path) or path in LICENCE_FILES:
            continue
        text = ctx.tree.text(path)
        if not text:
            continue
        hits = []
        for name, regex in KEPT_OUT_PHRASES:
            for m in regex.finditer(text):
                hits.append((line_of(text, m.start()), name))
        marks = [line_of(text, m.start()) for m in re.finditer(re.escape(PLACEHOLDER), text)]
        if marks and path == PLACEHOLDER_FILE:
            out.append(Finding(NOTE, 'H5', path, marks[0], "the one allowed placeholder line (%d time(s)): the maintainer's contact is the owner's to add" % len(marks)))
        elif marks:
            out += [Finding(ERROR, 'H5', path, l, 'the placeholder line belongs in %s alone' % PLACEHOLDER_FILE) for l in marks]
        if not hits:
            continue
        lines = [l for l, _ in hits]
        which = ', '.join(sorted({n for _, n in hits}))
        if path in OWNER_FILES:
            out.append(Finding(OWNER_FILE, 'H5', path, 0, 'phrases of the kept-out list at line(s) %s (%s): the owner\'s file, not edited' % (numbers(lines), which)))
        elif is_record(path):
            out.append(Finding(NOTE, 'H5', path, 0, 'a record file: phrases of the kept-out list at %d line(s) (%s)' % (len(set(lines)), which)))
        else:
            for line, name in sorted(set(hits)):
                out.append(Finding(ERROR, 'H5', path, line, 'a phrase the project keeps out of its files (%s)' % name))
    return out


def rule_h6(ctx):
    path = '.gitignore'
    text = ctx.tree.text(path)
    if text is None:
        return [Finding(ERROR, 'H6', path, 0, 'is not tracked or cannot be read: it holds the hard rule\'s ignore line')]
    lines = text.split('\n')
    at = next((i for i, l in enumerate(lines) if l.strip() in ('reference/', '/reference/')), None)
    out = []
    if at is None:
        return [Finding(ERROR, 'H6', path, 0, 'no reference/ line: nothing keeps the original game\'s files out of the repository')]
    block = []
    for l in reversed(lines[:at]):
        if l.startswith('#'):
            block.append(l)
        elif l.strip():
            break
    if len(block) < H6_BLOCK_FLOOR:
        out.append(Finding(ERROR, 'H6', path, at + 1, 'the comment block above the reference/ line is narrowed or gone (%d comment line(s) where the file had %d; at least %d are expected)'
                           % (len(block), H6_BLOCK_LINES, H6_BLOCK_FLOOR)))
    else:
        words = '\n'.join(block)
        out += [Finding(ERROR, 'H6', path, at + 1, 'the comment block above the reference/ line no longer says "%s"' % anchor) for anchor in H6_ANCHORS if anchor not in words]
    for i, l in enumerate(lines):
        s = l.strip()
        if s.startswith('!') and 'reference' in s:
            out.append(Finding(ERROR, 'H6', path, i + 1, 'a negation line that unignores something under reference/: %s' % s))
    return out


def rule_h7(ctx):
    out = []
    for path in REQUIRED_FILES:
        if not ctx.tree.has(path):
            out.append(Finding(ERROR, 'H7', path, 0, 'is not tracked: a stranger expects it'))
        else:
            data = ctx.tree.raw(path)
            if data is None or not data.strip():
                out.append(Finding(ERROR, 'H7', path, 0, 'is empty'))
    return out


def rule_h8(ctx):
    path = '.gitmodules'
    text = ctx.tree.text(path)
    if text is None:
        return [] if not ctx.tree.gitlinks else [Finding(ERROR, 'H8', path, 0, 'a gitlink exists (%s) and .gitmodules does not' % ', '.join(ctx.tree.gitlinks))]
    out, paths = [], []
    for i, line in enumerate(text.split('\n'), 1):
        m = re.match(r'\s*(url|path)\s*=\s*(.*?)\s*$', line)
        if not m:
            continue
        key, value = m.groups()
        if key == 'path':
            paths.append(value)
            continue
        if not value.lower().startswith('https://'):
            out.append(Finding(ERROR, 'H8', path, i, 'a submodule URL that is not https: %s' % value))
            continue
        authority = value[len('https://'):].split('/', 1)[0]
        if '@' in authority or ':' in authority.split('@')[-1] and not authority.rsplit(':', 1)[-1].isdigit():
            out.append(Finding(ERROR, 'H8', path, i, 'a submodule URL with credentials in it: %s' % value.split('@')[-1]))
    for p in paths:
        if p not in ctx.tree.gitlinks:
            out.append(Finding(ERROR, 'H8', path, 0, 'the submodule path %s is not a gitlink in the index' % p))
    return out


def rule_h9(ctx):
    out = []
    for mode, path in ctx.tree.entries:
        if mode == '?':
            continue
        sh = path.endswith('.sh')
        if sh and mode != '100755':
            out.append(Finding(ERROR, 'H9', path, 0, 'a shell script with mode %s (must be 100755: git update-index --chmod=+x)' % mode))
        elif not sh and mode == '100755':
            out.append(Finding(ERROR, 'H9', path, 0, 'an executable file that is not a .sh script'))
        elif mode == '120000':
            out.append(Finding(NOTE, 'H9', path, 0, 'a symbolic link (Windows checkouts make them as files)'))
    return out


# -- markdown: code is not read, links and headings are --

FENCE = re.compile(r'^\s{0,3}(`{3,}|~{3,})')
INLINE_CODE = re.compile(r'(`+)(.+?)\1', re.S)


def blank_code(text):
    """The text with fenced blocks and inline code spans replaced by spaces of the same length (and the same line breaks), so that offsets and lines still match."""
    out, fence = [], None
    for line in text.split('\n'):
        m = FENCE.match(line)
        if fence is None and m:
            fence = m.group(1)[0] * len(m.group(1))
            out.append(' ' * len(line))
        elif fence is not None:
            if m and m.group(1).startswith(fence[0]) and len(m.group(1)) >= len(fence) and not line.strip().strip(fence[0]):
                fence = None
            out.append(' ' * len(line))
        else:
            out.append(line)
    joined = '\n'.join(out)
    return INLINE_CODE.sub(lambda m: re.sub(r'[^\n]', ' ', m.group(0)), joined)


LINK = re.compile(r'(?<!\\)!?\[(?:[^\[\]]|\[[^\]]*\])*\]\(\s*(?P<t><[^>\n]+>|[^\s)]+)(?:\s+"[^"\n]*")?\s*\)')
REFERENCE_DEF = re.compile(r'^\s{0,3}\[[^\]\n]+\]:\s*(?P<t><[^>\n]+>|\S+)', re.M)
HEADING = re.compile(r'^\s{0,3}(#{1,6})[ \t]+(.+?)[ \t]*#*[ \t]*$', re.M)
HTML_ANCHOR = re.compile(r'<a\s[^>]*?(?:name|id)\s*=\s*["\']([^"\']+)["\']', re.I)


def heading_texts(text):
    """[(level, plain text)] of the ATX headings outside code."""
    return [(len(m.group(1)), plain_heading(m.group(2))) for m in HEADING.finditer(blank_fences_only(text))]


def blank_fences_only(text):
    out, fence = [], None
    for line in text.split('\n'):
        m = FENCE.match(line)
        if fence is None and m:
            fence = m.group(1)[0] * len(m.group(1))
            out.append('')
        elif fence is not None:
            if m and m.group(1).startswith(fence[0]) and len(m.group(1)) >= len(fence):
                fence = None
            out.append('')
        else:
            out.append(line)
    return '\n'.join(out)


def plain_heading(raw):
    text = re.sub(r'!?\[([^\]]*)\]\([^)]*\)', r'\1', raw)
    text = re.sub(r'[`*_]+', '', text) if False else text
    return text.strip()


def slug(text):
    """GitHub's anchor for a heading: lower case, the emphasis marks dropped, everything but letters, digits, spaces, hyphens and underscores removed, spaces to hyphens."""
    s = re.sub(r'[`*]', '', text.lower())
    s = re.sub(r'(?<![A-Za-z0-9])_|_(?![A-Za-z0-9])', '', s)
    s = re.sub(r'[^\w\- ]', '', s, flags=re.U)
    return s.strip().replace(' ', '-')


def anchors_of(text):
    seen, found = defaultdict(int), set()
    for _, plain in heading_texts(text):
        base = slug(plain)
        found.add(base if seen[base] == 0 else '%s-%d' % (base, seen[base]))
        seen[base] += 1
    found.update(m.group(1) for m in HTML_ANCHOR.finditer(text))
    return found


def rule_h10(ctx):
    out, anchor_cache = [], {}
    tree = ctx.tree

    def anchors(path):
        if path not in anchor_cache:
            text = tree.text(path)
            anchor_cache[path] = anchors_of(text) if text is not None else set()
        return anchor_cache[path]

    for path in tree.files:
        if not path.lower().endswith('.md') or tree.under_gitlink(path):
            continue
        text = tree.text(path)
        if text is None:
            continue
        code_free = blank_code(text)
        targets = [(m.start('t'), m.group('t')) for m in LINK.finditer(code_free)] + [(m.start('t'), m.group('t')) for m in REFERENCE_DEF.finditer(code_free)]
        for offset, raw in targets:
            target = raw[1:-1] if raw.startswith('<') else raw
            line = line_of(text, offset)
            if re.match(r'^[A-Za-z][A-Za-z0-9+.-]*:', target) or target.startswith('//'):
                continue  # https:, mailto:, ...
            target = urllib.parse.unquote(target)
            base, _, fragment = target.partition('#')
            base = base.partition('?')[0]
            if not base:
                if fragment and fragment.lower() not in {a.lower() for a in anchors(path)}:
                    out.append(Finding(ERROR, 'H10', path, line, 'the link #%s names no heading of this file' % fragment))
                continue
            resolved = posixpath.normpath(base.lstrip('/') if base.startswith('/') else posixpath.join(posixpath.dirname(path), base))
            if resolved.startswith('..'):
                out.append(Finding(ERROR, 'H10', path, line, 'the link %s leaves the repository' % base))
                continue
            if tree.has(resolved):
                if fragment and resolved.lower().endswith('.md') and fragment.lower() not in {a.lower() for a in anchors(resolved)}:
                    out.append(Finding(ERROR, 'H10', path, line, 'the link %s names no heading of %s' % (target, resolved)))
                continue
            if tree.is_dir(resolved) or tree.under_gitlink(resolved):
                continue
            first = resolved.split('/')[0]
            why = ' (a folder git ignores: %s/ is not in a clone)' % first if first in ('out', 'reference', 'build') else ''
            out.append(Finding(ERROR, 'H10', path, line, 'the link %s resolves to %s, which no tracked file or folder is%s' % (base, resolved, why)))
    return out


# "README, <Section>" in three spellings of the title: between asterisks or underscores, in double quotes (straight, or curly: “ ”), and bare (a capital letter, then words). Not a
# path's README (engine/README.md) and not the engine's README named in words ("the engine README", "the engine's README"), which is another file with other headings.
README_NAMED = r'(?<![\w./\\-])(?<!engine )(?<!engine\'s )(?<!engine’s )README(?:\.md)?`?,\s*'
README_REFERENCE_EMPHASIS = re.compile(README_NAMED + r'(?P<m>[*_])(?P<t>[^*_\n]{2,80}?)(?P=m)')
README_REFERENCE_QUOTED = re.compile(README_NAMED + r'["“](?P<t>[^"“”\n]{2,80}?)["”]')
README_REFERENCE_BARE = re.compile(README_NAMED + r'(?P<t>[A-Z][A-Za-z0-9 ]{1,60}?)(?=\s*(?:[);,:]|\.(?:\s|$)|$))', re.M)


def normal_title(text):
    return re.sub(r'\s+', ' ', re.sub(r'[`*_]', '', text)).strip().lower()


def rule_h11(ctx):
    out, tree = [], ctx.tree
    readme = tree.text('README.md')
    if readme is None:
        return []
    titles = {normal_title(t) for _, t in heading_texts(readme)}
    for path in tree.files:
        if tree.under_gitlink(path) or path == 'README.md' or path.startswith('engine/') or is_self(path):
            continue
        text = tree.text(path)
        if not text or 'README' not in text:
            continue
        found = []
        for regex in (README_REFERENCE_EMPHASIS, README_REFERENCE_QUOTED, README_REFERENCE_BARE):
            for m in regex.finditer(text):
                found.append((m.start(), m.group('t')))
        bad = sorted({(line_of(text, at), t) for at, t in found if normal_title(t) not in titles})
        if not bad:
            continue
        if path in OWNER_FILES:
            out += [Finding(OWNER_FILE, 'H11', path, l, 'names the README section "%s", which README.md has no heading for' % t) for l, t in bad]
        elif is_record(path):
            out.append(Finding(NOTE, 'H11', path, 0, 'a record file: %d reference(s) to a README section that has no heading now (lines %s)' % (len(bad), numbers([l for l, _ in bad]))))
        else:
            out += [Finding(ERROR, 'H11', path, l, 'names the README section "%s", which README.md has no heading for' % t) for l, t in bad]
    return out


def run_version_check(repo):
    """(exit code, output) of the repository's own tools/package/check_version.py."""
    script = os.path.join(repo, *VERSION_SCRIPT.split('/'))
    if not os.path.isfile(script):
        return 2, '%s does not exist' % VERSION_SCRIPT
    done = subprocess.run([sys.executable, script, '--repo', repo], capture_output=True, text=True, errors='replace')
    return done.returncode, (done.stdout + done.stderr).strip()


def rule_h12(ctx):
    code, output = ctx.version_check(ctx.tree.repo)
    if code == 0:
        return []
    lines = [l for l in output.split('\n') if l.strip()][:6]
    return [Finding(ERROR, 'H12', VERSION_SCRIPT, 0, 'exit %d: %s' % (code, ' | '.join(lines)))]


RULES = (('H1', rule_h1), ('H2', rule_h2), ('H3', rule_h3), ('H4', rule_h4), ('H5', rule_h5), ('H6', rule_h6), ('H7', rule_h7), ('H8', rule_h8), ('H9', rule_h9),
         ('H10', rule_h10), ('H11', rule_h11), ('H12', rule_h12))


def check(repo, include_untracked=False, rules=None, accounts=None, version_check=run_version_check):
    """The findings of the chosen rules over the repository. `accounts` is a list of names for H4 (default: the account that runs this) and `version_check(repo)` returns
    (exit code, output) for H12; both are arguments so that a test can name an invented account and a stand-in check."""
    repo = os.path.abspath(repo)
    try:
        git(repo, 'rev-parse', '--is-inside-work-tree')
    except GitError as e:
        raise GitError('%s is not a git work tree: %s' % (repo, e))
    tree = Tree(repo, list_entries(repo, include_untracked))
    names = list(accounts) if accounts is not None else running_accounts()
    searched, skipped = searchable_accounts(names)
    ctx = Context(tree, searched, version_check)
    chosen = [r for r, _ in RULES] if rules is None else list(rules)
    unknown = [r for r in chosen if r not in dict(RULES)]
    if unknown:
        raise GitError('no such rule: %s' % ', '.join(unknown))
    findings = []
    for rule, function in RULES:
        if rule in chosen:
            findings += function(ctx)
    if 'H4' in chosen:
        anywhere = len([n for n in searched if len(n) >= ACCOUNT_SUBSTRING_LETTERS])
        findings.append(Finding(NOTE, 'H4', '', 0, 'the account that runs this check: %d name(s) searched (%d anywhere in a word, %d as a whole word; a match inside an e-mail address is the e-mail note instead)%s (never printed)'
                                % (len(searched), anywhere, len(searched) - anywhere, ', %d too short or generic to search' % len(skipped) if skipped else '')))
    return findings


# ---- history --------------------------------------------------------------------------------------------------------------------------------

class HistoryResult:
    def __init__(self):
        self.findings, self.seconds, self.commits, self.blobs, self.authors, self.not_run = [], {}, 0, 0, set(), []


def history_names(repo):
    """{path: first commit} of every name ever ADDED in any commit reachable from any ref."""
    text = git(repo, 'log', '--all', '--no-renames', '--diff-filter=A', '--name-only', '--format=%x01%H')
    names, commit = {}, ''
    for line in text.split('\n'):
        if line.startswith('\x01'):
            commit = line[1:]
        elif line.strip():
            names.setdefault(line, commit)
    return names


def blob_commits(repo):
    """({blob sha: [commits that added or changed it]}, {commit: its age, oldest 0}) over every ref: where in the history a finding's blobs came in."""
    age = {c: i for i, c in enumerate(reversed(git(repo, 'rev-list', '--all').split()))}
    commits, current = defaultdict(list), ''
    for line in git(repo, 'log', '--all', '--no-renames', '--raw', '--no-abbrev', '--format=%x01%H').split('\n'):
        if line.startswith('\x01'):
            current = line[1:]
        elif line.startswith(':'):
            meta = line.split('\t')[0].split()
            if len(meta) >= 5 and meta[4][0] in 'AM':
                commits[meta[3]].append(current)
    return commits, age


def history_blobs(repo):
    """[(sha, path, size)] of every distinct blob reachable from any ref, with one path it was seen under."""
    listing = git(repo, 'rev-list', '--objects', '--all')
    paths = {}
    for line in listing.split('\n'):
        sha, _, path = line.partition(' ')
        if sha and path:
            paths.setdefault(sha, path)
    proc = subprocess.run(['git', '-C', repo, 'cat-file', '--batch-check=%(objecttype) %(objectname) %(objectsize)'], input='\n'.join(paths).encode() + b'\n', capture_output=True)
    blobs = []
    for line in proc.stdout.decode().split('\n'):
        parts = line.split()
        if len(parts) == 3 and parts[0] == 'blob':
            blobs.append((parts[1], paths[parts[1]], int(parts[2])))
    return blobs


def run_history(repo, accounts=None, budget=120.0):
    result = HistoryResult()
    started = time.time()
    searched, skipped = searchable_accounts(list(accounts) if accounts is not None else running_accounts())
    account = account_regex(searched)
    result.commits = len(git(repo, 'rev-list', '--all').split())
    for line in git(repo, 'log', '--all', '--format=%ae').split('\n'):
        if line.strip():
            result.authors.add(line.strip())
    # H1 on every name ever added
    t = time.time()
    for path, commit in sorted(history_names(repo).items()):
        if extension_of(path) in EXTENSIONS:
            result.findings.append(Finding(HISTORY, 'H1', path, 0, 'added with the extension .%s in commit %s' % (extension_of(path), commit[:10])))
    result.seconds['H1 names'] = time.time() - t
    blobs = history_blobs(repo)
    result.blobs = len(blobs)
    # H2 on sizes
    t = time.time()
    for sha, path, size in blobs:
        if size > MOST_BYTES:
            result.findings.append(Finding(HISTORY, 'H2', path, 0, 'a blob of %d bytes (%s), larger than the %d a tracked file may be' % (size, sha[:10], MOST_BYTES)))
    result.seconds['H2 sizes'] = time.time() - t
    # H2 (NUL), H3 and H4 over the content of each blob: one pass of one git process, each pattern timed on its own and abandoned past its budget
    patterns = [('H2 binary', None)] + [('H3 ' + n, r) for n, r, _ in SECRET_PATTERNS] + [('H4 windows-user', WINDOWS_USER), ('H4 home-user', HOME_USER), ('H4 mnt-drive', MOUNT_DRIVE)]
    if account is not None:
        patterns.append(('H4 account', account))
    spent = dict.fromkeys((n for n, _ in patterns), 0.0)
    read = dict.fromkeys(spent, 0)
    dead = set()
    by_pattern = defaultdict(lambda: defaultdict(set))  # name -> path -> {blob shas}
    record_counts = defaultdict(set)
    proc = subprocess.Popen(['git', '-C', repo, 'cat-file', '--batch'], stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    try:
        for sha, path, size in blobs:
            proc.stdin.write(sha.encode() + b'\n')
            proc.stdin.flush()
            header = proc.stdout.readline().split()
            data = proc.stdout.read(int(header[2]))
            proc.stdout.read(1)
            self_file = is_self(path)
            if self_file:
                continue
            binary = b'\0' in data and not data.startswith((b'\xff\xfe', b'\xfe\xff'))
            text = None if binary else decode_text(data)
            for name, regex in patterns:
                if name in dead:
                    continue
                t = time.time()
                hit = False
                if regex is None:
                    hit = binary
                elif text:
                    for m in (account_matches(text, regex) if name == 'H4 account' else regex.finditer(text)):
                        if name.startswith('H4') and 'name' in regex.groupindex and m.group('name').lower() in GENERIC_NAMES:
                            continue
                        if name == 'H3 keystore-password' and not keystore_literal(m.group('v')):
                            continue
                        hit = True
                        break
                spent[name] += time.time() - t
                read[name] += 1
                if hit:
                    if name.startswith('H4') and (is_record(path) or any(path == p for p, _ in ALLOWED_PROBES)):
                        record_counts[name].add(sha)
                    elif name.startswith('H3') and any(path == p and name.endswith(n) for p, n, _ in ALLOWED_SECRETS):
                        record_counts['allowed ' + name].add(sha)
                    else:
                        by_pattern[name][path].add(sha)
                if spent[name] > budget:
                    dead.add(name)
                    result.not_run.append('%s: NOT RUN to the end, stopped after %.1f s and %d of %d blobs (the budget was %.0f s)' % (name, spent[name], read[name], len(blobs), budget))
    finally:
        proc.stdin.close()
        proc.stdout.close()
        proc.wait()
    where = blob_commits(repo) if by_pattern else ({}, {})
    for name, per_path in sorted(by_pattern.items()):
        rule = name.split()[0]
        for path, shas in sorted(per_path.items()):
            commits = sorted({c for sha in shas for c in where[0].get(sha, ())}, key=lambda c: where[1].get(c, 0))
            span = ('; first in commit %s, last in %s (%d commit(s))' % (commits[0][:10], commits[-1][:10], len(commits))) if commits else ''
            result.findings.append(Finding(HISTORY, rule, path, 0, '%s in %d version(s) of the file (e.g. blob %s)%s' % (name[3:], len(shas), sorted(shas)[0][:10], span)))
    for name, shas in sorted(record_counts.items()):
        result.findings.append(Finding(NOTE, name.split()[0] if not name.startswith('allowed') else 'H3', '', 0, '%s: %d blob(s) in record files or allow-listed files, counted and not listed' % (name, len(shas))))
    for name in spent:
        result.seconds[name] = spent[name]
    result.seconds['total'] = time.time() - started
    return result


# ---- the report -----------------------------------------------------------------------------------------------------------------------------

def format_findings(findings):
    rows = sorted(findings, key=lambda f: (CLASS_ORDER.index(f.cls), f.rule.rjust(4), f.path, f.line))
    lines = ['%-11s %-4s %-44s %s' % ('CLASS', 'RULE', 'WHERE', 'FINDING')]
    for f in rows:
        lines.append('%-11s %-4s %-44s %s' % (f.cls, f.rule, f.where(), f.text))
    counts = defaultdict(int)
    for f in findings:
        counts[f.cls] += 1
    lines.append('')
    lines.append('summary: ' + ', '.join('%d %s' % (counts[c], c) for c in CLASS_ORDER if counts[c]) if findings else 'summary: no findings')
    return '\n'.join(lines)


def main(argv=None, out=None):
    out = out or sys.stdout
    ap = argparse.ArgumentParser(description='Reads the tracked files of a git work tree for what a stranger should not find (rules H1 to H12; see the top of this file).')
    ap.add_argument('--repo', default=REPO)
    ap.add_argument('--include-untracked', action='store_true', help='also read files that are not tracked yet and not ignored (for a run before a commit)')
    ap.add_argument('--rules', help='comma-separated rule ids to run (default: all)')
    ap.add_argument('--account', action='append', default=None, help="a name H4 must not find (added to the account that runs the check); repeatable")
    ap.add_argument('--history', action='store_true', help='also read every commit reachable from any ref (slow; read-only)')
    ap.add_argument('--history-budget', type=float, default=120.0, help='seconds one pattern may spend on the history before it is reported NOT RUN (default 120)')
    ap.add_argument('--list-rules', action='store_true')
    args = ap.parse_args(argv)
    if args.list_rules:
        for rule, function in RULES:
            print('%-4s %s' % (rule, (function.__doc__ or '').strip().split('\n')[0]), file=out)
        return EXIT_OK
    chosen = [r.strip() for r in args.rules.split(',')] if args.rules else None
    accounts = (running_accounts() + args.account) if args.account else None
    try:
        findings = check(args.repo, args.include_untracked, chosen, accounts)
    except GitError as e:
        print('check_public: %s' % e, file=sys.stderr)
        return EXIT_UNREADABLE
    print(format_findings(findings), file=out)
    code = EXIT_FINDING if any(f.cls == ERROR for f in findings) else EXIT_OK
    if args.history:
        started = time.time()
        try:
            history = run_history(os.path.abspath(args.repo), accounts, args.history_budget)
        except GitError as e:
            print('check_public --history: %s' % e, file=sys.stderr)
            return EXIT_UNREADABLE
        print('', file=out)
        print('HISTORY: %d commit(s) reachable from any ref, %d distinct blob(s), read in %.1f s' % (history.commits, history.blobs, time.time() - started), file=out)
        print('HISTORY authors (an e-mail address in a commit is public once the repository is): %s' % ', '.join(sorted(history.authors)), file=out)
        for name, seconds in sorted(history.seconds.items()):
            print('HISTORY time  %-22s %.1f s' % (name, seconds), file=out)
        for line in history.not_run:
            print('HISTORY ' + line, file=out)
        print(format_findings(history.findings), file=out)
        if any(f.cls == HISTORY for f in history.findings):
            code = EXIT_FINDING
        if history.not_run:
            code = EXIT_FINDING
    return code


if __name__ == '__main__':
    sys.exit(main())
