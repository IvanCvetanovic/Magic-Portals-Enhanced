"""The name of the account that runs a check (planning step 210, PORT-12), read from the system when the check runs.

WHY. Two checks of this repository look for the name of the account that built or edited something: tools/ci/check_public.py in the tracked files and tools/ci/check_workflows.py in the
workflows (tools/package/release.py does the same in a built program, with a lookup of its own, because the packager runs on its own). The name must not be written into any file
of the project, so it is asked of the system each time: the user name, the name of the home folder (the two can differ on Windows) and the three variables a shell sets. Nothing is
stored, printed or sent. On a CI runner the name is the runner's, which is an ordinary word of the files and is not searched.

THREE KINDS OF NAME. A name of ACCOUNT_SUBSTRING_LETTERS letters or more that is not an ordinary folder's is searched ANYWHERE, as a substring (the literal this lookup replaced was a
substring too, so a path, a log line, a build label glued to a digit and the local part of an address are all found). A name of ACCOUNT_MIN_LETTERS letters (one short of that) is searched as a
WHOLE WORD, because four letters are too few to be found inside longer words without refusing honest text. A name shorter than that, or the name of an ordinary folder (GENERIC_NAMES), is a
word of ordinary text and a bare search for it would flag every file that holds the word: it is skipped here (a path that holds it is still found by the patterns for a user folder), and
the callers say how many names they skipped. tools/package/release.py keeps the same three kinds for a built program, with a lookup of its own, and a test holds the two together.

Standard library only.
"""
import getpass
import os
import re

ACCOUNT_MIN_LETTERS = 4
ACCOUNT_SUBSTRING_LETTERS = 5
GENERIC_NAMES = frozenset('''runner runneradmin public default administrator user username name yourname you me example someone shared guest root ubuntu admin vagrant home
all users account owner'''.split())


def running_accounts():
    """The names of the account that runs this check, from the system now (the user name, the home folder's name, USERNAME, USER, LOGNAME), in that order, repeats included."""
    names = []
    for getter in (getpass.getuser, lambda: os.path.basename(os.path.expanduser('~'))):
        try:
            names.append(getter())
        except Exception:  # getpass raises when no variable and no password database names a user
            pass
    names += [os.environ[v] for v in ('USERNAME', 'USER', 'LOGNAME') if os.environ.get(v)]
    return names


def searchable_accounts(names):
    """(searched, skipped): each name once (case does not matter); a name that is short or ordinary is skipped."""
    searched, skipped, seen = [], [], set()
    for name in names:
        name = (name or '').strip()
        if not name or name.lower() in seen:
            continue
        seen.add(name.lower())
        (skipped if len(name) < ACCOUNT_MIN_LETTERS or name.lower() in GENERIC_NAMES else searched).append(name)
    return searched, skipped


def account_regex(names):
    """A regular expression for the names, in any case; None when there is no name. A name of ACCOUNT_SUBSTRING_LETTERS letters or more is found anywhere, a shorter one only as a whole
    word (no ASCII letter or digit touching it)."""
    if not names:
        return None
    anywhere = [re.escape(n) for n in names if len(n) >= ACCOUNT_SUBSTRING_LETTERS]
    words = [re.escape(n) for n in names if len(n) < ACCOUNT_SUBSTRING_LETTERS]
    parts = anywhere + [r'(?<![A-Za-z0-9])(?:%s)(?![A-Za-z0-9])' % '|'.join(words)] * bool(words)
    return re.compile('|'.join(parts), re.I)
