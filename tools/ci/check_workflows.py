#!/usr/bin/env python3
"""The rules every GitHub workflow of this repository keeps (planning step 209, PORT-11), read from the files. Needs PyYAML; without it this tool says NOT RUN and exits 3, never 0.

    python tools/ci/check_workflows.py [--workflows DIR]          exit 0: every file keeps every rule; 1: a rule is broken (each is printed with its id); 3: not run

WHY. A workflow is code that spends the owner's minutes on machines he does not see, and a file can be edited into something that spends more, reads a secret, runs a program of someone
else's or hides a failure, with every test of the project green. These rules are what the project decided a workflow may be while the repository is private; each is held by a test that
edits a copy of a real workflow and requires THIS tool to refuse the edit (tools/ci/test_check_workflows.py). The rules, by id:
  W01 triggers      a file runs on a dispatch and on nothing else (no push, pull_request, schedule ...). A file in REUSABLE may also be called by another workflow of the repository.
  W02 permissions   `permissions: contents: read` at the top, none widened in a job, no environment, container or service.
  W03 words         no secret, no token expression, no dump of a whole context, no path of the owner's data (no reference folder, no out folder, no GameData), no account name or absolute
                    path of the owner's PC, and not the name of any other project, anywhere in the file (comments included).
  W04 pins          every `uses:` is a commit sha of 40 hex digits (a tag or a branch moves; a sha does not) or a workflow of this repository.
  W05 timeouts      every job that has steps has a timeout-minutes of at most 180.
  W06 logs          every job ends with an upload of its logs that runs whatever happened (`if: always()` or `!cancelled()`).
  W07 wall limits   every launch of the game's program carries a wall limit (`timeout N`, or perl's alarm), and every ctest carries `--timeout`.
  W08 independence  a job waits for another only where NEEDS says so: the table is the design, and a new `needs:` is a decision.
  W09 pipes         no step pipes a download into an interpreter (`curl ... | sh`), runs a process substitution of a download, or evals one.
  W10 runners       every runner is a named image (ubuntu-24.04, never ubuntu-latest): an image that moves under a workflow changes its result.
  W11 forgiveness   `continue-on-error` only on the steps FORGIVEN names.
  W12 expressions   no `${{ }}` inside a `run:`: a value is passed through `env:`, so that the text that runs is the text that is read (and a value cannot inject a command).
  W13 shell         `defaults.run.shell: bash` at the top (so that `a | tee log` fails when `a` fails), and no step that names another shell except where SHELLS says.
  W14 names         the steps of a job have distinct names (the documents cite them).
  W15 retention     an upload keeps its files for at most 30 days.
  W16 head          no `| head` in a bash step unless the line ends in `|| true`: under pipefail, `head` that has had its lines closes the pipe and the writer, if it writes again, dies of SIGPIPE
                    (exit 141) and fails the step. Measured on Ubuntu 24.04: `ldd --version | head -1` failed 33 times in 40 runs. `sed -n 1p` reads all its input.

Standard library and PyYAML. It reads files; it runs nothing and fetches nothing.
"""
from __future__ import annotations

import argparse
import os
import re
import shlex
import sys

try:
    import yaml
except ImportError:
    yaml = None

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
WORKFLOWS = os.path.join(REPO, '.github', 'workflows')
sys.path.insert(0, HERE)

from running_account import account_regex, running_accounts, searchable_accounts  # noqa: E402  (the account that runs the check, read from the system)

EXIT_OK, EXIT_BROKEN, EXIT_NOT_RUN = 0, 1, 3

# ---- the decisions, as tables ----------------------------------------------------------------------------------------------------------
DISPATCH, CALL = 'workflow_dispatch', 'workflow_call'
REUSABLE = {'apple.yml'}  # called by release.yml: the macOS and iOS recipes are written once
# Job -> the jobs it waits for. A job not named here waits for nothing; a file not named here has no job that waits.
NEEDS = {
    'release.yml': {
        'linux': ['version'], 'windows': ['version'], 'android': ['version'], 'apple-macos': ['version'], 'apple-ios': ['version'],
        'collect': ['android', 'apple-ios', 'apple-macos', 'linux', 'version', 'windows'],
    },
}
RUNNERS = ('ubuntu-22.04', 'ubuntu-24.04', 'windows-2022', 'macos-15')
# (file, job, step name): the one step that may fail and be forgiven (Rosetta may be missing from a macOS image; the Intel half then does not run).
FORGIVEN = {('apple.yml', 'macos', 'Rosetta')}
# Steps that name a shell other than bash: the Windows runner's cmd, which loads MSVC's environment for a configure and a build (nothing else can).
SHELLS = {'cmd'}
MOST_TIMEOUT_MINUTES = 180
MOST_RETENTION_DAYS = 30
ALWAYS = ('always()', '!cancelled()')

# W03. `secrets` as a word (the plural: "no secret" in a comment is another word), the expressions that reach a token or a whole context, and the owner's data. The two names are written in
# pieces, so that this file itself passes the grep for them that the hard rule's pre-commit check makes.
OTHER_PROJECT = 'pen' + 'umbra'
ORIGINAL_MAKER = 'asan' + 'tee'
FORBIDDEN_WORDS = (
    (r'secrets\.', 'a secret'),
    (r'toJSON\s*\(\s*(secrets|github)\b|\$\{\{\s*github\s*\}\}', 'a whole context dumped'),
    (r'\bsecrets\b', 'the secrets context'),
    (r'GITHUB_TOKEN', 'the job token as a variable'),
    (r'github\s*(\.|\[\s*.)\s*token', 'the job token as an expression'),
    (r'(^|[^A-Za-z0-9_])out/', "the owner's out folder"),
    (r'(^|[/\\])reference\b|\breference[/\\]', "the owner's reference folder (the content packager's --reference flag is a flag and is not this)"),
    (r'GameData', "the game's data folder"),
    (r'C:[/\\]Users|/mnt/[a-z]/', "a path of the owner's PC"),
    (OTHER_PROJECT, 'the name of another project'),
    (ORIGINAL_MAKER, "the name of the original's maker"),
)
SHA = re.compile(r'^[A-Za-z0-9_.-]+/[A-Za-z0-9_./-]+@[0-9a-f]{40}$')
LOCAL_WORKFLOW = re.compile(r'^\./\.github/workflows/[A-Za-z0-9_.-]+\.ya?ml$')
PROGRAM = re.compile(r'(^|[/\\])MagicPortals(\.exe)?$')
INTERPRETERS = {'sh', 'bash', 'zsh', 'dash', 'ksh', 'fish', 'python', 'python3', 'perl', 'ruby', 'node', 'pwsh', 'powershell'}
DOWNLOADERS = {'curl', 'wget'}


class Problem:
    def __init__(self, rule, where, text):
        self.rule, self.where, self.text = rule, where, text

    def __str__(self):
        return '%s %s: %s' % (self.rule, self.where, self.text)


# ---- reading a file -------------------------------------------------------------------------------------------------------------------------

def require_yaml():
    if yaml is None:
        raise RuntimeError('PyYAML is not installed')


def triggers_of(workflow):
    """The events the workflow runs on. PyYAML reads the key `on` as the boolean True (YAML 1.1), so both spellings are asked for."""
    on = workflow.get('on', workflow.get(True))
    if on is None:
        return set()
    if isinstance(on, str):
        return {on}
    return set(on)


def logical_lines(script):
    """The lines of a `run:` with continuations joined (`\\` at the end of a line) and blank lines and comments dropped."""
    out, current = [], ''
    for raw in str(script).split('\n'):
        line = raw.rstrip()
        if current:
            line = current + ' ' + line.strip()
            current = ''
        if line.endswith('\\'):
            current = line[:-1].rstrip()
            continue
        if line.strip() and not line.strip().startswith('#'):
            out.append(line.strip())
    if current:
        out.append(current)
    return out


def pipeline_commands(line):
    """The simple commands of a line, each as (the operator before it, its words), split at | || && ; & and the parentheses of a subshell. Quoted text stays in one word. A line the shell
    cannot make sense of (an unbalanced quote) is returned as one command of one word, so that a rule still looks at it."""
    try:
        lexer = shlex.shlex(line, posix=True, punctuation_chars=True)
        lexer.whitespace_split = True
        lexer.commenters = ''
        tokens = list(lexer)
    except ValueError:
        return [('', [line])]
    out, words, operator = [], [], ''
    for token in tokens:
        if token and all(c in '|&;()' for c in token):
            if words:
                out.append((operator, words))
            words, operator = [], token
        else:
            words.append(token)
    if words:
        out.append((operator, words))
    return out


# ---- the wall limit ---------------------------------------------------------------------------------------------------------------------

def skip_wrapper(words):
    """After a wrapper command (timeout, xvfb-run, perl's alarm, sudo ...) the words that remain: the wrapper's own options and arguments are skipped by its arity. Returns
    (wrappers seen, the words from the command it wraps)."""
    wrappers = []
    i = 0
    while i < len(words):
        word = words[i]
        if re.fullmatch(r'[A-Za-z_][A-Za-z0-9_]*=.*', word):
            i += 1
            continue
        base = os.path.basename(word)
        if base == 'timeout':
            wrappers.append('timeout')
            i += 1
            while i < len(words) and words[i].startswith('-'):
                i += 2 if words[i] in ('-k', '--kill-after', '-s', '--signal') else 1
            i += 1  # the duration
        elif base == 'xvfb-run':
            wrappers.append('xvfb-run')
            i += 1
            while i < len(words) and words[i].startswith('-'):
                i += 2 if words[i] in ('-s', '--server-args', '-e', '--error-file', '-n', '--server-num', '-w', '--wait', '-f', '--auth-file') else 1
        elif base == 'perl' and i + 3 < len(words) and words[i + 1] == '-e' and 'alarm' in words[i + 2]:
            wrappers.append('timeout')  # perl -e 'alarm shift; exec @ARGV' SECONDS command...: the wall limit apple.yml uses where macOS has no timeout
            i += 4
        elif base in ('sudo', 'nohup', 'setsid', 'time', 'env', 'arch', 'stdbuf', 'nice'):
            wrappers.append(base)
            i += 1
            while i < len(words) and words[i].startswith('-'):
                i += 1
        else:
            break
    return wrappers, words[i:]


def launches(script):
    """[(line, wrappers)] for every launch of the game's program in a bash `run:`, and [(line, wrappers)] for every ctest. A launch is a command whose word, after its wrappers, is a path
    that ends in MagicPortals (or MagicPortals.exe)."""
    program, tests = [], []
    for line in logical_lines(script):
        for _, words in pipeline_commands(line):
            wrappers, rest = skip_wrapper(words)
            if not rest:
                continue
            if PROGRAM.search(rest[0]):
                program.append((line, wrappers))
            elif os.path.basename(rest[0]) == 'ctest':
                tests.append((line, wrappers, rest))
    return program, tests


def pipes_into_an_interpreter(script):
    """The lines that run a download through a pipe into an interpreter, through a process substitution, or through eval or sh -c of a command substitution."""
    found = []
    for line in logical_lines(script):
        commands = pipeline_commands(line)
        names = []
        for operator, words in commands:
            _, rest = skip_wrapper(words)
            names.append((operator, os.path.basename(rest[0]) if rest else ''))
        for index, (operator, name) in enumerate(names):
            if operator == '|' and name in INTERPRETERS and any(earlier in DOWNLOADERS for _, earlier in names[:index]):
                found.append(line)
                break
        else:
            if re.search(r'<\(\s*(curl|wget)\b', line) or re.search(r'\b(sh|bash|zsh|dash|eval|source)\b[^|;&]*\$\(\s*(curl|wget)\b', line):
                found.append(line)
    return found


HEAD_PIPE = re.compile(r'(?<!\|)\|(?!\|)\s*head\b')  # a pipe, not the `||` of a fallback
FORGIVEN_LINE = re.compile(r'\|\|\s*true\s*$')


def pipes_into_head(script):
    """The lines that pipe into `head` (W16) and do not end in `|| true`."""
    return [line for line in logical_lines(script) if HEAD_PIPE.search(line) and not FORGIVEN_LINE.search(line)]


# ---- the rules --------------------------------------------------------------------------------------------------------------------------

def steps_of(job):
    return job.get('steps') or []


def check_file(name, text, accounts=None):
    """Every rule over one workflow file's text. Returns a list of Problem. `accounts`: the names W03 must not find (default: the account that runs the check, read from the system)."""
    require_yaml()
    problems = []

    def bad(rule, where, message):
        problems.append(Problem(rule, '%s %s' % (name, where) if where else name, message))

    try:
        workflow = yaml.safe_load(text)
    except yaml.YAMLError as e:
        return [Problem('W00', name, 'not YAML: %s' % e)]
    if not isinstance(workflow, dict) or not isinstance(workflow.get('jobs'), dict) or not workflow['jobs']:
        return [Problem('W00', name, 'no jobs')]
    jobs = workflow['jobs']

    # W01
    triggers = triggers_of(workflow)
    allowed = {DISPATCH, CALL} if name in REUSABLE else {DISPATCH}
    if DISPATCH not in triggers:
        bad('W01', '', 'the file has no workflow_dispatch trigger (found %s)' % sorted(triggers))
    for extra in sorted(triggers - allowed):
        bad('W01', '', 'the trigger %s is not allowed: a workflow runs on a dispatch only%s' % (extra, ' (and, in this file, when another workflow calls it)' if name in REUSABLE else ''))
    # W02
    if workflow.get('permissions') != {'contents': 'read'}:
        bad('W02', '', 'permissions must be exactly contents: read, found %r' % (workflow.get('permissions'),))
    for job_id, job in jobs.items():
        for key in ('permissions', 'environment', 'container', 'services'):
            if key in job:
                bad('W02', 'job %s' % job_id, 'has %s: a job may not widen what the file allows' % key)
    # W03
    for pattern, what in FORBIDDEN_WORDS:
        m = re.search(pattern, text, re.I | re.M)
        if m:
            bad('W03', '', 'the file holds %s (%r)' % (what, m.group(0)))
    searched, _ = searchable_accounts(running_accounts() if accounts is None else accounts)
    if account_regex(searched) and account_regex(searched).search(text):
        bad('W03', '', "the file holds the name of the account that runs the check (never printed): an account name or a path of the owner's PC")
    # W04
    for job_id, job in jobs.items():
        refs = [('job', job['uses'])] if 'uses' in job else []
        refs += [('step %r' % (s.get('name') or s.get('id') or '?'), s['uses']) for s in steps_of(job) if 'uses' in s]
        for where, ref in refs:
            ref = str(ref)
            if not (SHA.match(ref) or LOCAL_WORKFLOW.match(ref)):
                bad('W04', 'job %s, %s' % (job_id, where), 'uses %r, which is not a commit sha of 40 hex digits (or a workflow of this repository)' % ref)
            elif LOCAL_WORKFLOW.match(ref) and not os.path.isfile(os.path.join(REPO, ref[2:])):
                bad('W04', 'job %s, %s' % (job_id, where), 'calls %s, which is not in the repository' % ref)
    # W05
    for job_id, job in jobs.items():
        if 'uses' in job:
            continue
        minutes = job.get('timeout-minutes')
        if not isinstance(minutes, int) or isinstance(minutes, bool) or not 0 < minutes <= MOST_TIMEOUT_MINUTES:
            bad('W05', 'job %s' % job_id, 'has no timeout-minutes of at most %d (found %r)' % (MOST_TIMEOUT_MINUTES, minutes))
    # W06
    for job_id, job in jobs.items():
        steps = steps_of(job)
        if 'uses' in job:
            continue
        if not steps:
            bad('W06', 'job %s' % job_id, 'has no steps')
            continue
        last = steps[-1]
        condition = str(last.get('if', '')).replace('${{', '').replace('}}', '').replace(' ', '')
        if not str(last.get('uses', '')).startswith('actions/upload-artifact@') or condition not in ALWAYS:
            bad('W06', 'job %s' % job_id, 'does not end with an upload of its logs that runs whatever happened (last step %r, if: %r)' % (last.get('name'), last.get('if')))
    # W07, W09, W12, W13, W14, W15, W11
    top_shell = (workflow.get('defaults') or {}).get('run', {}).get('shell')
    if top_shell != 'bash':
        bad('W13', '', 'defaults.run.shell must be bash (found %r): without pipefail a pipe to tee hides the failure of the command before it' % top_shell)
    for job_id, job in jobs.items():
        names = []
        for step in steps_of(job):
            label = 'job %s, step %r' % (job_id, step.get('name') or step.get('id') or '?')
            names.append(step.get('name'))
            shell = step.get('shell')
            if shell is not None and shell != 'bash' and shell not in SHELLS:
                bad('W13', label, 'names the shell %r' % shell)
            if 'continue-on-error' in step and (name, job_id, step.get('name')) not in FORGIVEN:
                bad('W11', label, 'may fail and be forgiven (continue-on-error), and is not in the table of steps that may')
            if 'run' in step:
                script = str(step['run'])
                if '${{' in script:
                    bad('W12', label, 'has ${{ }} inside its run: pass the value through env: (text %r)' % script[script.index('${{'):script.index('${{') + 40])
                if shell in (None, 'bash'):
                    program, tests = launches(script)
                    for line, wrappers in program:
                        if 'timeout' not in wrappers:
                            bad('W07', label, 'launches the program with no wall limit: %s' % line[:140])
                    for line, wrappers, rest in tests:
                        if not any(w == '--timeout' or w.startswith('--timeout=') for w in rest):
                            bad('W07', label, 'runs ctest with no --timeout: %s' % line[:140])
                    for line in pipes_into_an_interpreter(script):
                        bad('W09', label, 'runs a download through an interpreter: %s' % line[:140])
                    for line in pipes_into_head(script):
                        bad('W16', label, 'pipes into head, which can kill the writer with SIGPIPE under pipefail (exit 141); use `sed -n 1p` (or end the line in `|| true`): %s' % line[:140])
            if str(step.get('uses', '')).startswith('actions/upload-artifact@'):
                days = (step.get('with') or {}).get('retention-days')
                if not isinstance(days, int) or isinstance(days, bool) or not 0 < days <= MOST_RETENTION_DAYS:
                    bad('W15', label, 'keeps its files for %r days: at most %d, and it must say' % (days, MOST_RETENTION_DAYS))
        named = [n for n in names if n]
        for n in sorted(set(named)):
            if named.count(n) > 1:
                bad('W14', 'job %s' % job_id, 'two steps are named %r' % n)
        if 'continue-on-error' in job:
            bad('W11', 'job %s' % job_id, 'may fail and be forgiven (continue-on-error on the job)')
    # W08
    table = NEEDS.get(name, {})
    for job_id, job in jobs.items():
        needs = job.get('needs')
        needs = [needs] if isinstance(needs, str) else list(needs or [])
        if sorted(needs) != sorted(table.get(job_id, [])):
            bad('W08', 'job %s' % job_id, 'waits for %s and the table says %s: a job waits only where the table (NEEDS) says so' % (sorted(needs), sorted(table.get(job_id, []))))
    for job_id in table:
        if job_id not in jobs:
            bad('W08', 'job %s' % job_id, 'is in the table of waits and not in the file')
    # W10
    for job_id, job in jobs.items():
        if 'uses' in job:
            continue
        runner = job.get('runs-on')
        if runner not in RUNNERS:
            bad('W10', 'job %s' % job_id, 'runs on %r, which is not one of the pinned images %s' % (runner, ', '.join(RUNNERS)))
    return problems


def check_folder(folder):
    """{file name: [Problem]} for every .yml of the folder."""
    results = {}
    for name in sorted(os.listdir(folder)):
        if name.endswith(('.yml', '.yaml')):
            with open(os.path.join(folder, name), encoding='utf-8') as f:
                results[name] = check_file(name, f.read())
    return results


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    p.add_argument('--workflows', default=WORKFLOWS)
    args = p.parse_args(argv)
    if yaml is None:
        print('check_workflows: NOT RUN - PyYAML is not installed (pip install pyyaml in a virtual environment), so no workflow was read. This is never a pass.')
        return EXIT_NOT_RUN
    if not os.path.isdir(args.workflows):
        print('check_workflows: no folder %s' % args.workflows, file=sys.stderr)
        return EXIT_BROKEN
    results = check_folder(args.workflows)
    if not results:
        print('check_workflows: %s holds no workflow' % args.workflows, file=sys.stderr)
        return EXIT_BROKEN
    broken = 0
    for name, problems in results.items():
        print('%s: %s' % (name, 'ok' if not problems else '%d problem(s)' % len(problems)))
        for problem in problems:
            print('  ' + str(problem))
            broken += 1
    print('check_workflows: %d file(s), %s' % (len(results), 'every rule kept' if not broken else '%d problem(s)' % broken))
    return EXIT_BROKEN if broken else EXIT_OK


if __name__ == '__main__':
    sys.exit(main())
