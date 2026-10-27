"""The Apple workflow (.github/workflows/apple.yml), parsed and pinned (planning step 206, PORT-8).

Run:   python -m unittest discover -s tools/apple -p "test_*.py" -v        (needs PyYAML: pip install pyyaml in a venv, e.g. under out/; without it these tests are
       SKIPPED and say so, never passed; actionlint, when it is on PATH, is run over the file too)

WHAT THIS PROVES. That the file, as written, holds the rules the project set for a workflow that spends a private repository's macOS minutes: it runs on a dispatch
and on the call that release.yml makes (planning step 209) and on nothing else (no push, no pull request, no schedule), asks for no permission but reading the contents, uses no
secret and no token, names no path of the owner's data (no reference folder, no out folder, no GameData), pins every action it uses to a commit sha, bounds every job in time,
makes the three jobs independent of one another, lets NO failed test through by name (the two that used to fail without the data were made to skip, step 209),
ends each job with an upload of its logs that runs whatever happened, passes the flags that keep a runner from rewriting the committed shaders, and puts a wall limit on
every launch of the program. The MUTATIONS run the same checks over an edited copy and require each edit to be refused.
WHAT IT CANNOT PROVE. That GitHub accepts the file or that any step of it does what it says on a macOS runner: NOT KNOWN until it is dispatched (actionlint, where it ran, checks the
syntax and the expressions, not the behaviour).
"""
import os
import re
import shutil
import subprocess
import unittest

try:
    import yaml
except ImportError:  # the tests that need it say so and are skipped
    yaml = None

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
WORKFLOW = os.path.join(REPO, '.github', 'workflows', 'apple.yml')

JOBS = ['macos', 'ios-device', 'ios-sim']
# The names the docs cite (docs/apple.md): a job's and its steps'. A rename here is a rename there.
JOB_NAMES = {
    'macos': 'macOS - build, test, shell, fixture',
    'ios-device': 'iOS device - build, shell ipa',
    'ios-sim': 'iOS simulator - build, shell app',
}
CITED_STEPS = {
    'macos': ['Configure (tests)', 'Build (tests)', 'Test (ctest, data-less)', 'Configure (shell)', 'Build (shell)', 'Assemble the shell', 'Run the fixture (arm64)',
              'Check the picture (arm64)', 'Run with no game data (must fail in words)', 'Rosetta', 'Run the fixture (x86_64 under Rosetta)',
              'Check the picture (x86_64)', 'Source tree untouched', 'Upload the shell', 'Upload logs'],
    'ios-device': ['Configure', 'Build', 'Assemble the shell ipa', 'Upload the shell', 'Upload logs'],
    'ios-sim': ['Configure', 'Build', 'Assemble the shell app', 'Upload the shell', 'Upload logs'],
}
# Planning step 209: an action is pinned to a COMMIT SHA of 40 hex digits (a tag is a name that moves; the runs of 2026-10-25 used the floating v4 tags; the shas below are what those tags named on 2026-10-27).
PINNED = re.compile(r'^[A-Za-z0-9_.-]+/[A-Za-z0-9_./-]+@[0-9a-f]{40}$')
SHA_CHECKOUT = 'actions/checkout@11d5960a326750d5838078e36cf38b85af677262 # v4.4.0'
SHA_CACHE_SAVE = 'actions/cache/save@0057852bfaa89a56745cba8c7296529d2fc39830 # v4.3.0'
SHA_UPLOAD = 'actions/upload-artifact@ea165f8d65b6e75b540449e92b4886f43607fa02 # v4.6.2'
# The triggers: a person's dispatch, and the call that release.yml makes (a called workflow cannot be started by anything but its caller).
TRIGGERS = {'workflow_dispatch', 'workflow_call'}
# The two names are written in pieces, so that this file itself passes the grep for them that the hard rule's pre-commit check makes.
SIBLING = 'pen' + 'umbra'
ORIGINAL_MAKER = 'asan' + 'tee'
# `GITHUB_TOKEN` is the environment spelling of the job's token; `github.token` and github['token'] are the expression spellings of the same thing, which
# `secrets\.` does not catch (the permissions are contents: read, so it could do little, but this workflow needs no token at all). Nor does `secrets\.` catch an
# index (`secrets['K']`) or a dump of a whole context (`toJSON(secrets)`, `toJSON(github)`, a bare `${{ github }}`): the second pattern is the dumps and comes BEFORE
# the bare word, so that a dump is refused by its own rule, and the bare word `secrets` (the plural: "no secret" in a comment is a different word) is the third.
FORBIDDEN_WORDS = (r'secrets\.', r'toJSON\s*\(\s*(secrets|github)\b|\$\{\{\s*github\s*\}\}', r'\bsecrets\b', r'GITHUB_TOKEN', r'github\s*(\.|\[\s*.)\s*token',
                   r'(^|[^A-Za-z0-9_])out/', r'reference', r'GameData', SIBLING, ORIGINAL_MAKER)
WALL_LIMIT = "perl -e 'alarm shift; exec @ARGV'"
# The steps that run or check the Intel program, picked by what they DO: `arch -x86_64` or `arch -arch x86_64` (with or without a line continuation between the
# words) starts it, and `x86_64.png` is the picture it draws. The bare word x86_64 would also pick the universal configure and the lipo check, and `-archs` is not
# `arch `. A rule that picked nothing would pass for want of a step to look at, so the count is pinned too.
INTEL_STEP = re.compile(r'\barch(?:\s|\\)+(?:-x86_64|-arch(?:\s|\\)+x86_64)|x86_64\.png')
ROSETTA_GATE = {'!cancelled()', "steps.rosetta.outcome=='success'"}
PICTURE_CHECK = 'python3 tools/apple/check_png.py "$RUNNER_TEMP/apple-logs/shots/ci_level-macos-%s.png" --min-size 640x360'


def read_text(path):
    """A file's text, with the file closed (an unclosed one is a ResourceWarning, and an error under -W error)."""
    with open(path, encoding='utf-8') as f:
        return f.read()


def trigger_set(workflow):
    """The events the workflow runs on. PyYAML reads the key `on` as the boolean True (YAML 1.1), so both spellings are asked for; a scalar, a list and a mapping
    all name events."""
    on = workflow.get('on', workflow.get(True))
    assert on is not None, 'the workflow has no trigger'
    if isinstance(on, str):
        return {on}
    if isinstance(on, list):
        return set(on)
    return set(on)


def steps_of(workflow, job):
    return workflow['jobs'][job]['steps']


def gate_terms(condition):
    """The terms of an `if:` expression joined by `&&`, with the spaces taken out and the `${{ }}` taken off, as a set; None when the expression holds an `||` (an
    alternative makes any term of it optional, so no term of it is a gate). Searching the text for the wanted term instead would pass
    `steps.rosetta.outcome == 'success' || !cancelled()`, which runs the step when Rosetta did NOT install."""
    text = str(condition).strip()
    if text.startswith('${{') and text.endswith('}}'):
        text = text[3:-2]
    if '||' in text:
        return None
    return {term.replace(' ', '') for term in text.split('&&')}


def run_lines(step):
    """The non-blank lines of a step's `run:`, each stripped."""
    return [line.strip() for line in str(step.get('run', '')).splitlines() if line.strip()]


def check_workflow(text):
    """Every rule, over the file's text. Raises AssertionError naming the one broken."""
    workflow = yaml.safe_load(text)
    # The trigger.
    assert trigger_set(workflow) == TRIGGERS, 'the triggers are workflow_dispatch and workflow_call (release.yml calls this file) and nothing else, found %s' % sorted(trigger_set(workflow))
    on = workflow.get('on', workflow.get(True))
    job_input = on['workflow_dispatch']['inputs']['job']
    assert job_input['type'] == 'choice' and job_input['options'] == ['macos', 'ios-device', 'ios-sim', 'all'] and job_input['default'] == 'all', 'the job input'
    assert set(on['workflow_dispatch']['inputs']) == {'job'}, 'the dispatch takes one input'
    call_input = on['workflow_call']['inputs']['job']
    assert call_input['type'] == 'string' and call_input['default'] == 'all' and set(on['workflow_call']['inputs']) == {'job'}, 'the call takes the same one input, as a string'
    # What it may do.
    assert workflow.get('permissions') == {'contents': 'read'}, 'permissions: contents: read, and nothing more'
    for name, job in workflow['jobs'].items():
        assert 'permissions' not in job, name + ' widens the permissions'
        assert 'environment' not in job and 'container' not in job and 'services' not in job, name + ' reaches for an environment, a container or a service'
    for pattern in FORBIDDEN_WORDS:
        assert not re.search(pattern, text, re.I | re.M), 'the file holds %r' % pattern
    # Cost control.
    assert isinstance(workflow.get('concurrency'), dict) and workflow['concurrency'].get('group') and workflow['concurrency'].get('cancel-in-progress') is False, 'concurrency'
    assert sorted(workflow['jobs']) == sorted(JOBS), 'the jobs: %s' % sorted(workflow['jobs'])
    for name, job in workflow['jobs'].items():
        assert isinstance(job.get('timeout-minutes'), int) and 0 < job['timeout-minutes'] <= 120, name + ' has a timeout'
        assert 'needs' not in job, name + ' waits for another job: one failure would hide the next job\'s result'
        assert 'strategy' not in job, name + ' has a strategy (fail-fast belongs to a matrix, and there is none)'
        assert job['runs-on'] == 'macos-15', name + ' runs on macos-15'
        assert 'inputs.job' in str(job.get('if')) and ("'all'" in str(job['if'])) and ("'%s'" % name in str(job['if'])), name + ' is chosen by the job input'
        assert job['name'] == JOB_NAMES[name], 'the name of job %s' % name
    # Every action pinned; every upload bounded.
    uses = []
    for name in JOBS:
        for step in steps_of(workflow, name):
            if 'uses' in step:
                uses.append((name, step['uses']))
                assert PINNED.match(step['uses']), 'unpinned action %r in %s' % (step['uses'], name)
                if step['uses'].startswith('actions/upload-artifact'):
                    assert isinstance(step['with'].get('retention-days'), int) and step['with']['retention-days'] <= 14, 'an upload keeps its files for at most two weeks'
    assert any(u.startswith('actions/checkout@') for _, u in uses) and any(u.startswith('actions/upload-artifact@') for _, u in uses), 'checkout and upload are used'
    for name in JOBS:
        checkout = [s for s in steps_of(workflow, name) if str(s.get('uses', '')).startswith('actions/checkout@')]
        assert len(checkout) == 1 and checkout[0]['with'] == {'submodules': 'recursive'}, name + ' checks out with the engine submodule'
    # The steps the docs name, in each job; and the last step of each is the logs, always.
    for name in JOBS:
        names = [s.get('name', '') for s in steps_of(workflow, name)]
        for cited in CITED_STEPS[name]:
            assert cited in names, 'step %r of %s is missing' % (cited, name)
        assert len(set(names)) == len(names), 'two steps of %s share a name: %s' % (name, sorted(n for n in set(names) if names.count(n) > 1))
        last = steps_of(workflow, name)[-1]
        assert last.get('name') == 'Upload logs' and str(last.get('if')).strip() == 'always()' and last['uses'].startswith('actions/upload-artifact@'), name + ' ends with an upload of its logs that always runs'
        assert 'apple-logs' in last['with']['path'], name + ' uploads the folder the logs are written to'
    # The logs it uploads are the ones it writes.
    for log in ('configure-macos-tests.log', 'build-macos-tests.log', 'ctest-macos.log', 'configure-macos-shell.log', 'build-macos-shell.log', 'assemble-macos.log',
                'configure-ios-device.log', 'build-ios-device.log', 'configure-ios-sim.log', 'build-ios-sim.log'):
        assert 'apple-logs/' + log in text, log + ' is written'
    # How it builds.
    configures = re.findall(r'cmake -S \. -B (\S+)((?:[^\n]*\\\n)*[^\n]*)', text)
    assert len(configures) == 4, 'four configures (macOS tests, macOS shell, iOS device, iOS simulator), found %d' % len(configures)
    for tree, rest in configures:
        assert '-DGLSL_COMPILER=GLSL_COMPILER-NOTFOUND' in rest, tree + ': the committed shaders are never rewritten'
        assert '-DSUPERSONIC_ENABLE_VALIDATION=OFF' in rest, tree + ': no validation layers'
        assert '-DSUPERSONIC_MOLTENVK_LIBRARY=' in rest and 'libMoltenVK.a' in rest, tree + ': MoltenVK is linked in'
        assert '-DCMAKE_BUILD_TYPE=Release' in rest, tree
        if 'ios' in tree:
            assert '-DCMAKE_SYSTEM_NAME=iOS' in rest and '-DCMAKE_OSX_DEPLOYMENT_TARGET=16.3' in rest and '-DCMAKE_OSX_ARCHITECTURES=arm64' in rest, tree
            assert ('-DCMAKE_OSX_SYSROOT=iphoneos' in rest) == ('device' in tree) and ('-DCMAKE_OSX_SYSROOT=iphonesimulator' in rest) == ('sim' in tree), tree
        else:
            assert '-DCMAKE_OSX_DEPLOYMENT_TARGET=13.3' in rest, tree
        if 'shell' in tree:
            assert '-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"' in rest and '-DMAGICPORTALS_BAKED_PATHS=OFF' in rest and '-DMAGICPORTALS_DEV_TOOLS=OFF' in rest, tree + ': the shell is a release-style universal build'
    # MoltenVK: one version, in the cache key and the download.
    assert workflow['env']['MOLTENVK_VERSION'] == 'v1.4.2', 'MoltenVK v1.4.2'
    for name in JOBS:
        caches = [s for s in steps_of(workflow, name) if s.get('uses', '').startswith('actions/cache@')]
        assert len(caches) == 1 and 'env.MOLTENVK_VERSION' in caches[0]['with']['key'], name + ' caches MoltenVK by its version'
    assert 'releases/download/$MOLTENVK_VERSION/MoltenVK-all.tar' in text, 'the download is the release of that version'
    assert re.search(r'^  MOLTENVK_SHA256: [0-9a-f]{64}$', text, re.M), 'the MoltenVK tarball has a pinned sha256 (64 hex digits)'
    assert text.count('curl -fsSL') == text.count('echo "$MOLTENVK_SHA256  $RUNNER_TEMP/mvk.tar" | shasum -a 256 -c -') == 3, 'every download of the tarball is checked against it'
    # The suites, the skip count and the fixture.
    ctest = [s for s in steps_of(workflow, 'macos') if s.get('name') == 'Test (ctest, data-less)'][0]['run']
    assert 'ctest --test-dir build' in ctest and "grep -c '(Skipped)'" in ctest and 'never counted as passed' in ctest, 'the skip count is printed and never counted as a pass'
    assert 'known=' not in ctest and 'unexpected' not in ctest, 'no failure is let through by name any more (planning step 209)'
    assert '::error::failed tests' in ctest and 'if [ -n "$failed" ]' in ctest, 'any failed test fails the step'
    assert '--timeout 900' in ctest, 'ctest has a wall limit'
    assert text.count('--root "$GITHUB_WORKSPACE/tests/fixtures/ci" --port-data "$GITHUB_WORKSPACE/game/data" --level ci_level') == 2, 'the fixture is run twice: arm64 and x86_64, with the tables of the port from the checkout'
    assert text.count('--frames 300') == 2 and text.count('--window 1280x720') == 2 and text.count('--fixed-step') >= 3, 'the fixture runs 300 fixed ticks at 1280x720'
    # A wall limit on every launch of the program.
    lines = text.splitlines()
    launches = [i for i, line in enumerate(lines) if 'MagicPortals.app/Contents/MacOS/MagicPortals' in line and 'player/MagicPortals/' in line]
    assert len(launches) == 3, 'three launches of the program (arm64, no game data, x86_64), found %d' % len(launches)
    for i in launches:
        assert WALL_LIMIT in lines[i - 1], 'a launch with no wall limit just before it: line %d' % (i + 1)
    # The Intel half runs only where Rosetta installed. Not every runner image can install it, and that step alone may fail (continue-on-error): a runner without it
    # would otherwise fail the job on a bad CPU type. The steps are picked by what they run (`arch -x86_64`, the x86_64 picture), not by the bare word x86_64,
    # which the universal configure and the lipo check also hold; exactly two are expected, so that the rule cannot pass for want of a step to look at.
    macos = steps_of(workflow, 'macos')
    rosetta = [s for s in macos if s.get('id') == 'rosetta']
    assert len(rosetta) == 1 and rosetta[0].get('continue-on-error') is True and 'install-rosetta' in str(rosetta[0].get('run')), \
        'one step with the id rosetta installs Rosetta, and it alone may fail (continue-on-error)'
    intel = [s for s in macos if INTEL_STEP.search(str(s.get('run', '')))]
    assert len(intel) == 2, 'two steps run or check the x86_64 program, found %d' % len(intel)
    for step in intel:
        assert gate_terms(step.get('if')) == ROSETTA_GATE, \
            'the x86_64 step %r is not gated on Rosetta having installed (its condition must be exactly !cancelled() && the Rosetta step\'s outcome, no ||)' % step.get('name')
    # Nothing else may fail and be forgiven: a step, or a whole job, that carries continue-on-error turns a red result green. Only the Rosetta step does.
    for name, job in workflow['jobs'].items():
        assert 'continue-on-error' not in job, name + ' may fail and be forgiven (continue-on-error on the job)'
        for step in job['steps']:
            if 'continue-on-error' in step:
                assert step.get('id') == 'rosetta', 'step %r of %s may fail and be forgiven (continue-on-error): only the Rosetta step may' % (step.get('name'), name)
    # The fixture runs gate the job: the program's exit code is the step's last word (`test "$rc" = 0`, nothing after it, no `|| true`), and each picture is checked by
    # the checker itself, as the step's only command. The run with no game data must exit 1 and then say so in words. These are the lines whose removal would let a
    # crash, or a flat picture, pass.
    by_name = {s.get('name'): s for s in macos}
    for run_name, check_name, arch in (('Run the fixture (arm64)', 'Check the picture (arm64)', 'arm64'),
                                       ('Run the fixture (x86_64 under Rosetta)', 'Check the picture (x86_64)', 'x86_64')):
        assert run_lines(by_name[run_name])[-1] == 'test "$rc" = 0', 'step %r ends with the test of the exit code, test "$rc" = 0' % run_name
        assert run_lines(by_name[check_name]) == [PICTURE_CHECK % arch], 'step %r only runs the picture check, check_png.py on the %s picture' % (check_name, arch)
    nodata = run_lines(by_name['Run with no game data (must fail in words)'])
    assert 'test "$rc" = 1' in nodata and nodata[-1].startswith('grep -q "did not find the game\'s data"'), \
        'the run with no game data must exit 1 and say so in words, and the words are the last check'
    # One job's failure never stops the next block of the macOS job: after the suites every step runs unless the run was cancelled.
    after = False
    for step in macos:
        if step.get('name') == 'Test (ctest, data-less)':
            after = True
            continue
        if after and step.get('name') not in ('Collect', 'ccache (save)', 'Upload the shell', 'Upload logs'):
            assert '!cancelled()' in str(step.get('if')), 'step %r would be skipped after a failure' % step.get('name')
    return workflow


def requires_yaml(test):
    return unittest.skipIf(yaml is None, 'PyYAML is not installed (pip install pyyaml in a venv): this test did NOT run')(test)


class AppleWorkflow(unittest.TestCase):
    text = read_text(WORKFLOW) if os.path.isfile(WORKFLOW) else ''

    @requires_yaml
    def test_the_workflow_holds_every_rule(self):
        check_workflow(self.text)

    @requires_yaml
    def test_the_only_triggers_are_a_dispatch_and_the_call_release_yml_makes(self):
        workflow = yaml.safe_load(self.text)
        self.assertEqual(trigger_set(workflow), TRIGGERS)
        release = read_text(os.path.join(REPO, '.github', 'workflows', 'release.yml'))
        self.assertIn('uses: ./.github/workflows/apple.yml', release, 'the call is what the second trigger is for')
        # PyYAML reads the key `on` as the boolean True; the triggers sit under exactly one of the two spellings, never both.
        self.assertEqual(sum(key in workflow for key in (True, 'on')), 1, 'the triggers are under exactly one of the keys True and "on"')

    @requires_yaml
    def test_the_dispatch_choice_selects_each_job_and_all(self):
        workflow = yaml.safe_load(self.text)
        for name in JOBS:
            condition = workflow['jobs'][name]['if']
            self.assertIn("inputs.job == 'all'", condition)
            self.assertIn("inputs.job == '%s'" % name, condition)

    def test_trigger_set_reads_the_three_spellings_of_on(self):
        self.assertEqual(trigger_set({True: 'workflow_dispatch'}), {'workflow_dispatch'})
        self.assertEqual(trigger_set({'on': ['push', 'workflow_dispatch']}), {'push', 'workflow_dispatch'})
        self.assertEqual(trigger_set({'on': {'push': {'branches': ['main']}, 'workflow_dispatch': None}}), {'push', 'workflow_dispatch'})

    def test_every_artifact_the_workflow_names_is_one_the_assembly_script_writes(self):
        # The script writes the three shells and the workflow names them again to upload (and, for the macOS one, to unpack): a rename on one side only
        # would upload nothing, or fail only on the paid run.
        names = sorted(set(re.findall(r'MagicPortals-[A-Za-z0-9_.-]+\.(?:zip|ipa)', self.text)))
        self.assertEqual(names, ['MagicPortals-iOS-shell.ipa', 'MagicPortals-iOS-simulator-shell.zip', 'MagicPortals-macOS-shell.zip'])
        script = read_text(os.path.join(HERE, 'make_app.sh'))
        for name in names:
            self.assertIn(name, script, name)

    @requires_yaml
    def test_the_doc_cites_the_names_this_file_has(self):
        doc = os.path.join(REPO, 'docs', 'apple.md')
        if not os.path.isfile(doc):
            self.skipTest('docs/apple.md is missing: nothing to compare with')
        text = read_text(doc)
        for name in JOB_NAMES.values():
            self.assertIn(name, text, 'docs/apple.md cites the job %r' % name)
        for step in ('Test (ctest, data-less)', 'Run the fixture (arm64)', 'Run with no game data (must fail in words)', 'Run the fixture (x86_64 under Rosetta)'):
            self.assertIn(step, text, 'docs/apple.md cites the step %r' % step)

    # ---- the ctest step's own script, run against recorded ctest outputs --------------------------------------------------------------------------

    def run_ctest_step(self, ctest_output, ctest_exit, tests_cmake=None):
        """The `run:` of the data-less ctest step, run under bash with a stand-in `ctest` that prints `ctest_output` and exits `ctest_exit`. (exit status, standard output)"""
        import tempfile
        bash = shutil.which('bash')
        if bash is None or 'system32' in bash.lower():
            for candidate in (r'C:\Program Files\Git\bin\bash.exe', r'C:\Program Files (x86)\Git\bin\bash.exe'):
                if os.path.isfile(candidate):
                    bash = candidate
                    break
        if bash is None:
            self.skipTest('no bash: this test did NOT run')
        workflow = yaml.safe_load(self.text)
        script = [st for st in steps_of(workflow, 'macos') if st.get('name') == 'Test (ctest, data-less)'][0]['run']
        with tempfile.TemporaryDirectory() as folder:
            folder = folder.replace('\\', '/')
            os.makedirs(folder + '/bin')
            os.makedirs(folder + '/work/tests')
            os.makedirs(folder + '/temp/apple-logs')
            with open(folder + '/bin/ctest', 'w', newline='\n') as f:
                f.write("#!/usr/bin/env bash\ncat <<'EOF'\n%s\nEOF\nexit %d\n" % (ctest_output, ctest_exit))
            os.chmod(folder + '/bin/ctest', 0o755)  # on Linux a stand-in that is not executable is skipped, and the real ctest answers instead
            with open(folder + '/work/tests/CMakeLists.txt', 'w', newline='\n') as f:
                f.write(tests_cmake or 'add_mp_test(test_mp_a)\nadd_mp_test(test_mp_levels SKIPS_WITH_77)\n')
            with open(folder + '/script.sh', 'w', newline='\n') as f:
                f.write(script)
            posix = folder if not (os.name == 'nt' and folder[1:2] == ':') else '/' + folder[0].lower() + folder[2:]
            env = dict(os.environ, RUNNER_TEMP=posix + '/temp')
            command = 'export PATH="%s/bin:$PATH"; cd %s/work && bash -eo pipefail %s/script.sh' % (posix, posix, posix)
            proc = subprocess.run([bash, '-c', command], capture_output=True, text=True, env=env, timeout=120)
            return proc.returncode, proc.stdout + proc.stderr

    # What ctest prints when the data is absent and nothing fails: the suites that need it are listed as not run (Skipped), and the exit code is 0.
    ONLY_SKIPS = (
        'The following tests did not run:\n'
        '\t  3 - test_mp_levels (Skipped)\n'
        '\n'
        '100% tests passed, 0 tests failed out of 5')

    # The two suites that used to fail without the data (APPLE-22) are failures like any other now: the step lets no failure through by name.
    SHOOT_FAILS = (
        'The following tests did not run:\n'
        '\t  3 - test_mp_levels (Skipped)\n'
        '\n'
        'The following tests FAILED:\n'
        '\t 38 - test_mp_shoot (Failed)\n'
        'Errors while running CTest')

    @requires_yaml
    def test_the_ctest_step_passes_when_the_suites_that_need_data_are_skipped_and_says_so(self):
        code, text = self.run_ctest_step(self.ONLY_SKIPS, 0)
        self.assertEqual(code, 0, text)
        self.assertIn('skipped: 1 (never counted as passed)', text)
        self.assertIn('failed: none', text)

    @requires_yaml
    def test_the_ctest_step_fails_on_any_failed_test_the_two_that_used_to_fail_included(self):
        for output in (self.SHOOT_FAILS, self.SHOOT_FAILS.replace('test_mp_shoot', 'test_mp_sprites'), self.SHOOT_FAILS.replace('test_mp_shoot', 'test_mp_tscn')):
            code, text = self.run_ctest_step(output, 8)
            self.assertEqual(code, 1, text)
            self.assertIn('failed tests: ', text)
        code, text = self.run_ctest_step(self.SHOOT_FAILS.replace('Errors while', '\t 52 - test_mp_sprites (Failed)\nErrors while'), 8)
        self.assertEqual(code, 1, text)
        self.assertIn('failed tests: test_mp_shoot test_mp_sprites', text)

    @requires_yaml
    def test_the_ctest_step_fails_when_ctest_dies_without_naming_a_failure(self):
        code, text = self.run_ctest_step('ctest: segmentation fault', 139)
        self.assertEqual(code, 1, text)
        self.assertIn('ctest exited 139 with no failed test named', text)

    @requires_yaml
    def test_the_ctest_step_passes_when_everything_passes(self):
        code, text = self.run_ctest_step('100% tests passed, 0 tests failed out of 5', 0)
        self.assertEqual(code, 0, text)
        self.assertIn('failed: none', text)

    @requires_yaml
    def test_the_ctest_step_warns_when_the_skip_count_is_not_the_marked_count(self):
        marked_twice = 'add_mp_test(test_mp_a SKIPS_WITH_77)\nadd_mp_test(test_mp_b SKIPS_WITH_77)\n'
        code, text = self.run_ctest_step(self.ONLY_SKIPS, 0, tests_cmake=marked_twice)
        self.assertEqual(code, 0, text)
        self.assertIn('::warning::1 suites skipped, 2 are marked as needing data', text)

    def test_the_count_of_marked_suites_reads_the_form_the_split_suites_are_written_in(self):
        # the step counts the lines `add_mp_test(<name> ... SKIPS_WITH_77)`; planning step 209 added three in the form `add_mp_test(<name> IN_SOURCE <file> SKIPS_WITH_77)`
        pattern = re.compile(r'^add_mp_test\(.* SKIPS_WITH_77\)')
        self.assertTrue(pattern.match('add_mp_test(test_mp_levels SKIPS_WITH_77)'))
        self.assertTrue(pattern.match('add_mp_test(test_mp_shoot_levels IN_SOURCE test_mp_shoot SKIPS_WITH_77)'))
        self.assertFalse(pattern.match('add_mp_test(test_mp_shoot)'))
        text = read_text(os.path.join(REPO, 'tests', 'CMakeLists.txt')).replace('\r\n', '\n')
        for name in ('test_mp_shoot_levels', 'test_mp_sprites_original', 'test_mp_sprites_levels'):
            self.assertTrue(re.search(r'^add_mp_test\(%s IN_SOURCE test_mp_\w+ SKIPS_WITH_77\)$' % name, text, re.M), name)
        self.assertIn('add_mp_test(test_mp_shoot)\n', text)
        self.assertIn('add_mp_test(test_mp_sprites)\n', text)

    # ---- MUTATIONS: each edit of the file is refused, by the rule it breaks ----------------------------------------------------------------------

    def refused(self, old, new, why, expect):
        """The file as it is passes every rule; with this one edit the rule named by `expect` refuses it."""
        self.assertEqual(self.text.count(old), 1, 'the mutation applies once: %r (%d)' % (old, self.text.count(old)))
        check_workflow(self.text)
        with self.assertRaises(AssertionError, msg='MUTATION "%s" was not refused' % why) as caught:
            check_workflow(self.text.replace(old, new))
        self.assertIn(expect, str(caught.exception), 'MUTATION "%s" was refused by another rule' % why)

    def refused_everywhere(self, old, new, why, expect):
        self.assertGreaterEqual(self.text.count(old), 1, old)
        check_workflow(self.text)
        with self.assertRaises(AssertionError, msg='MUTATION "%s" was not refused' % why) as caught:
            check_workflow(self.text.replace(old, new))
        self.assertIn(expect, str(caught.exception), 'MUTATION "%s" was refused by another rule' % why)

    TRIGGER = 'on:\n  workflow_dispatch:\n'

    @requires_yaml
    def test_mutation_the_moltenvk_hash_not_a_hash(self):
        self.refused('  MOLTENVK_SHA256: 562a15a29bc358446a56a4091c5f7e08f604184187c1d34f712148b61ef17276\n', '  MOLTENVK_SHA256: 562a15a2\n', 'a short hash', 'pinned sha256')

    @requires_yaml
    def test_mutation_one_download_not_checked(self):
        check = '          echo "$MOLTENVK_SHA256  $RUNNER_TEMP/mvk.tar" | shasum -a 256 -c -\n'
        self.assertEqual(self.text.count(check), 3)
        check_workflow(self.text)
        with self.assertRaises(AssertionError, msg='MUTATION "one download unchecked" was not refused') as caught:
            check_workflow(self.text.replace(check, '', 1))
        self.assertIn('every download of the tarball is checked', str(caught.exception))

    @requires_yaml
    def test_mutation_a_push_trigger(self):
        self.refused(self.TRIGGER, 'on:\n  push:\n    branches: [main]\n  workflow_dispatch:\n', 'push trigger', 'the triggers are workflow_dispatch and workflow_call')

    @requires_yaml
    def test_mutation_a_pull_request_trigger(self):
        self.refused(self.TRIGGER, 'on:\n  pull_request:\n  workflow_dispatch:\n', 'pull_request trigger', 'the triggers are workflow_dispatch and workflow_call')

    @requires_yaml
    def test_mutation_a_schedule(self):
        self.refused(self.TRIGGER, 'on:\n  schedule:\n    - cron: "0 3 * * *"\n  workflow_dispatch:\n', 'schedule trigger', 'the triggers are workflow_dispatch and workflow_call')

    @requires_yaml
    def test_mutation_the_call_trigger_dropped(self):
        # release.yml calls this file: without the trigger its call would fail at the first dispatch of the release
        self.refused('  workflow_call:\n    inputs:\n      job:\n        description: Which job to run (macos, ios-device, ios-sim or all)\n        type: string\n        default: all\n', '',
                     'no workflow_call trigger', 'the triggers are workflow_dispatch and workflow_call')

    @requires_yaml
    def test_mutation_the_call_input_is_no_longer_the_one_string(self):
        self.refused('        description: Which job to run (macos, ios-device, ios-sim or all)\n        type: string\n', '        description: Which job to run (macos, ios-device, ios-sim or all)\n        type: boolean\n',
                     'a call input that is a boolean', 'the call takes the same one input')

    @requires_yaml
    def test_mutation_a_secret(self):
        self.refused('  CCACHE_MAXSIZE: 600M\n', '  CCACHE_MAXSIZE: 600M\n  KEY: ${{ secrets.SIGNING_KEY }}\n', 'a secret', "the file holds 'secrets")

    @requires_yaml
    def test_mutation_the_job_token_as_an_expression(self):
        # `GITHUB_TOKEN` was refused already; `github.token` is the same token spelled as an expression, and the `secrets.` rule does not see it.
        self.refused('  CCACHE_MAXSIZE: 600M\n', '  CCACHE_MAXSIZE: 600M\n  KEY: ${{ github.token }}\n', 'github.token', "the file holds 'github")

    @requires_yaml
    def test_mutation_the_job_token_as_an_index(self):
        self.refused('  CCACHE_MAXSIZE: 600M\n', "  CCACHE_MAXSIZE: 600M\n  KEY: ${{ github['token'] }}\n", "github['token']", "the file holds 'github")

    @requires_yaml
    def test_mutation_the_job_token_as_an_environment_variable(self):
        self.refused('  CCACHE_MAXSIZE: 600M\n', '  CCACHE_MAXSIZE: 600M\n  GITHUB_TOKEN: x\n', 'GITHUB_TOKEN', "the file holds 'GITHUB_TOKEN'")

    @requires_yaml
    def test_mutation_an_unpinned_action(self):
        old = SHA_CHECKOUT.split(' ')[0]
        self.assertEqual(self.text.count(old), 3)
        self.assertEqual(self.text.replace(old, 'actions/checkout@main', 1).count('actions/checkout@main'), 1)
        check_workflow(self.text)
        with self.assertRaises(AssertionError) as caught:
            check_workflow(self.text.replace(old, 'actions/checkout@main', 1))
        self.assertIn('unpinned action', str(caught.exception))

    @requires_yaml
    def test_mutation_an_action_pinned_to_a_tag_and_not_a_sha(self):
        # planning step 209: the file was pinned to the tag v4 (it ran twice so); a tag moves, so it is refused now
        old = SHA_CHECKOUT.split(' ')[0]
        self.refused_everywhere(old, 'actions/checkout@v4', 'a tag where a sha is', 'unpinned action')

    @requires_yaml
    def test_mutation_a_sha_that_is_not_forty_digits(self):
        self.refused_everywhere(SHA_UPLOAD.split(' ')[0], 'actions/upload-artifact@ea165f8d65b6e75b540449e92b4886f43607fa0', 'a sha cut short', 'unpinned action')

    @requires_yaml
    def test_mutation_an_action_with_no_version(self):
        self.refused_everywhere(SHA_CACHE_SAVE.split(' ')[0], 'actions/cache/save', 'no version at all', 'unpinned action')

    @requires_yaml
    def test_mutation_a_branch_instead_of_a_version(self):
        self.refused_everywhere(SHA_UPLOAD.split(' ')[0], 'actions/upload-artifact@master', 'a branch name', 'unpinned action')

    @requires_yaml
    def test_the_pins_are_the_shas_the_other_workflows_use(self):
        # one sha per action in the whole repository: a second file that pinned another commit of the same action would be two decisions
        folder = os.path.join(REPO, '.github', 'workflows')
        pins = {}
        for name in sorted(os.listdir(folder)):
            for ref in re.findall(r'uses:\s*(actions/[A-Za-z0-9_./-]+@[0-9a-f]{40})', read_text(os.path.join(folder, name))):
                action, sha = ref.split('@')
                pins.setdefault(action.split('/')[1], set()).add(sha)
        for action, shas in sorted(pins.items()):
            self.assertEqual(len(shas), 1, 'the action %s is pinned to %d different commits across the workflows' % (action, len(shas)))

    @requires_yaml
    def test_mutation_write_permission(self):
        self.refused('permissions:\n  contents: read\n', 'permissions:\n  contents: write\n', 'contents: write', 'permissions: contents: read')

    @requires_yaml
    def test_mutation_no_timeout(self):
        self.refused('    runs-on: macos-15\n    timeout-minutes: 90\n', '    runs-on: macos-15\n', 'no timeout on the macOS job', 'has a timeout')

    @requires_yaml
    def test_mutation_a_job_waits_for_another(self):
        self.refused("    if: ${{ inputs.job == 'all' || inputs.job == 'ios-sim' }}\n", "    if: ${{ inputs.job == 'all' || inputs.job == 'ios-sim' }}\n    needs: [macos]\n",
                     'needs', 'waits for another job')

    @requires_yaml
    def test_mutation_a_job_with_a_matrix(self):
        self.refused("    if: ${{ inputs.job == 'all' || inputs.job == 'ios-sim' }}\n", "    if: ${{ inputs.job == 'all' || inputs.job == 'ios-sim' }}\n    strategy:\n      fail-fast: true\n",
                     'a strategy', 'has a strategy')

    @requires_yaml
    def test_mutation_the_logs_upload_stops_running_after_a_failure(self):
        self.refused('      - name: Upload logs\n        if: always()\n        uses: %s\n        with:\n          name: ios-sim-logs\n' % SHA_UPLOAD,
                     '      - name: Upload logs\n        uses: %s\n        with:\n          name: ios-sim-logs\n' % SHA_UPLOAD, 'no always() on the last upload',
                     'ends with an upload of its logs that always runs')

    @requires_yaml
    def test_mutation_the_shader_flag_dropped(self):
        self.refused('            -DCMAKE_OSX_DEPLOYMENT_TARGET=16.3 -DGLSL_COMPILER=GLSL_COMPILER-NOTFOUND \\\n            -DSUPERSONIC_MOLTENVK_LIBRARY="$HOME/moltenvk-ios-sim',
                     '            -DCMAKE_OSX_DEPLOYMENT_TARGET=16.3 \\\n            -DSUPERSONIC_MOLTENVK_LIBRARY="$HOME/moltenvk-ios-sim', 'no GLSL_COMPILER flag in the simulator configure',
                     'the committed shaders are never rewritten')

    @requires_yaml
    def test_mutation_validation_left_on(self):
        self.refused_everywhere('-DSUPERSONIC_ENABLE_VALIDATION=OFF', '-DSUPERSONIC_ENABLE_VALIDATION=ON', 'validation on', 'no validation layers')

    @requires_yaml
    def test_mutation_the_shell_built_with_baked_paths(self):
        self.refused('-DMAGICPORTALS_BAKED_PATHS=OFF -DMAGICPORTALS_DEV_TOOLS=OFF', '-DMAGICPORTALS_DEV_TOOLS=OFF', 'a shell with the build machine\'s paths in it',
                     'a release-style universal build')

    @requires_yaml
    def test_mutation_a_path_of_the_owners_data(self):
        self.refused('--root "$GITHUB_WORKSPACE/tests/fixtures/ci" --port-data "$GITHUB_WORKSPACE/game/data" --level ci_level \\\n            --window 1280x720 --fixed-step --frames 300 --screenshot "$RUNNER_TEMP/apple-logs/shots/ci_level-macos-arm64.png"',
                     '--root "$GITHUB_WORKSPACE/reference/extracted" --port-data "$GITHUB_WORKSPACE/game/data" --level ci_level \\\n            --window 1280x720 --fixed-step --frames 300 --screenshot "$RUNNER_TEMP/apple-logs/shots/ci_level-macos-arm64.png"',
                     'a reference folder', "the file holds 'reference'")

    @requires_yaml
    def test_mutation_the_x86_64_run_no_longer_waits_for_rosetta(self):
        self.refused("      - name: Run the fixture (x86_64 under Rosetta)\n        if: ${{ !cancelled() && steps.rosetta.outcome == 'success' }}\n",
                     "      - name: Run the fixture (x86_64 under Rosetta)\n        if: ${{ !cancelled() }}\n", 'the x86_64 run not gated on Rosetta',
                     'is not gated on Rosetta having installed')

    @requires_yaml
    def test_mutation_the_x86_64_picture_check_no_longer_waits_for_rosetta(self):
        self.refused("      - name: Check the picture (x86_64)\n        if: ${{ !cancelled() && steps.rosetta.outcome == 'success' }}\n",
                     "      - name: Check the picture (x86_64)\n        if: ${{ !cancelled() }}\n", 'the x86_64 picture check not gated on Rosetta',
                     'is not gated on Rosetta having installed')

    @requires_yaml
    def test_mutation_the_rosetta_step_loses_its_id(self):
        # Without the id the gate reads an outcome that does not exist: always false, and the Intel half would never run.
        self.refused('      - name: Rosetta\n        id: rosetta\n', '      - name: Rosetta\n', 'the Rosetta step without an id', 'one step with the id rosetta')

    @requires_yaml
    def test_mutation_the_rosetta_step_may_no_longer_fail(self):
        self.refused('        continue-on-error: true\n        run: sudo softwareupdate --install-rosetta --agree-to-license\n',
                     '        run: sudo softwareupdate --install-rosetta --agree-to-license\n', 'a runner that cannot install Rosetta fails the job',
                     'one step with the id rosetta')

    @requires_yaml
    def test_mutation_a_third_step_runs_the_x86_64_program_unguarded(self):
        marker = "      # Nothing the build wrote may be a change of a tracked file (the engine's shaders above all).\n"
        self.refused(marker, "      - name: A stray Intel run\n        run: arch -x86_64 true\n\n" + marker, 'an unguarded x86_64 step',
                     'two steps run or check the x86_64 program')

    @requires_yaml
    def test_mutation_a_stray_intel_step_spelled_arch_dash_arch(self):
        # `arch -arch x86_64` starts the Intel program as `arch -x86_64` does; a rule that picked its steps by the one spelling would not see this one.
        marker = "      # Nothing the build wrote may be a change of a tracked file (the engine's shaders above all).\n"
        self.refused(marker, "      - name: A stray Intel run\n        run: arch -arch x86_64 true\n\n" + marker, 'an unguarded step spelled arch -arch x86_64',
                     'two steps run or check the x86_64 program')

    def test_the_intel_step_selector_reads_the_spellings_and_not_the_lipo_check(self):
        for text in ('arch -x86_64 prog', 'arch -arch x86_64 prog', '/usr/bin/arch   -x86_64 prog', 'arch \\\n  -x86_64 prog', 'arch -arch \\\n  x86_64 prog',
                     'python3 check_png.py "shots/ci_level-macos-x86_64.png"'):
            self.assertTrue(INTEL_STEP.search(text), text)
        for text in ('lipo -archs "$APP"', '-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"', 'echo "architectures: arm64 x86_64"', 'arch -arm64 prog'):
            self.assertFalse(INTEL_STEP.search(text), text)

    @requires_yaml
    def test_mutation_the_rosetta_gate_made_vacuous_by_an_alternative(self):
        # The text still holds both terms, so a search for the term passes it; as an `||` it runs the Intel step when Rosetta did NOT install.
        self.refused("      - name: Run the fixture (x86_64 under Rosetta)\n        if: ${{ !cancelled() && steps.rosetta.outcome == 'success' }}\n",
                     "      - name: Run the fixture (x86_64 under Rosetta)\n        if: ${{ steps.rosetta.outcome == 'success' || !cancelled() }}\n", 'the Rosetta gate as an ||',
                     'is not gated on Rosetta having installed')

    @requires_yaml
    def test_mutation_the_rosetta_gate_loses_its_cancel_term(self):
        self.refused("      - name: Check the picture (x86_64)\n        if: ${{ !cancelled() && steps.rosetta.outcome == 'success' }}\n",
                     "      - name: Check the picture (x86_64)\n        if: ${{ steps.rosetta.outcome == 'success' }}\n", 'the Rosetta gate without !cancelled()',
                     'is not gated on Rosetta having installed')

    def test_gate_terms_reads_an_expression_the_way_the_rules_need(self):
        self.assertEqual(gate_terms("${{ !cancelled() && steps.rosetta.outcome == 'success' }}"), ROSETTA_GATE)
        self.assertEqual(gate_terms("!cancelled() && steps.rosetta.outcome=='success'"), ROSETTA_GATE, 'a bare expression, no ${{ }}, is legal too')
        self.assertIsNone(gate_terms("${{ steps.rosetta.outcome == 'success' || !cancelled() }}"))
        self.assertIsNone(gate_terms("${{ (steps.rosetta.outcome == 'success' || x) && !cancelled() }}"))
        self.assertNotEqual(gate_terms("${{ !cancelled() }}"), ROSETTA_GATE)

    @requires_yaml
    def test_mutation_a_secret_by_index(self):
        self.refused('  CCACHE_MAXSIZE: 600M\n', "  CCACHE_MAXSIZE: 600M\n  KEY: ${{ secrets['K'] }}\n", "secrets['K']", r"the file holds '\\bsecrets\\b'")

    @requires_yaml
    def test_mutation_the_whole_secrets_context_as_json(self):
        self.refused('  CCACHE_MAXSIZE: 600M\n', '  CCACHE_MAXSIZE: 600M\n  ALL: ${{ toJSON(secrets) }}\n', 'toJSON(secrets)', "the file holds 'toJSON")

    @requires_yaml
    def test_mutation_the_whole_github_context_as_json(self):
        self.refused('  CCACHE_MAXSIZE: 600M\n', '  CCACHE_MAXSIZE: 600M\n  ALL: ${{ toJSON(github) }}\n', 'toJSON(github)', "the file holds 'toJSON")

    @requires_yaml
    def test_mutation_the_whole_github_context_bare(self):
        self.refused('  CCACHE_MAXSIZE: 600M\n', '  CCACHE_MAXSIZE: 600M\n  ALL: ${{ github }}\n', 'a bare github context', "the file holds 'toJSON")

    @requires_yaml
    def test_mutation_the_arm64_fixture_step_may_fail_and_be_forgiven(self):
        self.refused("      - name: Run the fixture (arm64)\n        if: ${{ !cancelled() }}\n", "      - name: Run the fixture (arm64)\n        if: ${{ !cancelled() }}\n        continue-on-error: true\n",
                     'continue-on-error on the arm64 fixture run', 'only the Rosetta step may')

    @requires_yaml
    def test_mutation_a_whole_job_may_fail_and_be_forgiven(self):
        self.refused('    runs-on: macos-15\n    timeout-minutes: 90\n', '    runs-on: macos-15\n    timeout-minutes: 90\n    continue-on-error: true\n', 'continue-on-error on the macOS job',
                     'continue-on-error on the job')

    @requires_yaml
    def test_mutation_the_arm64_exit_code_is_no_longer_tested(self):
        self.refused('          echo "exit code: $rc"\n          test "$rc" = 0\n\n      - name: Check the picture (arm64)\n',
                     '          echo "exit code: $rc"\n\n      - name: Check the picture (arm64)\n', 'no test of the arm64 exit code', 'ends with the test of the exit code')

    @requires_yaml
    def test_mutation_the_arm64_exit_code_is_forgiven(self):
        self.refused('          echo "exit code: $rc"\n          test "$rc" = 0\n\n      - name: Check the picture (arm64)\n',
                     '          echo "exit code: $rc"\n          test "$rc" = 0 || true\n\n      - name: Check the picture (arm64)\n', 'the arm64 exit code forgiven',
                     'ends with the test of the exit code')

    @requires_yaml
    def test_mutation_the_x86_64_exit_code_is_no_longer_tested(self):
        self.refused('          echo "exit code: $rc"\n          test "$rc" = 0\n\n      - name: Check the picture (x86_64)\n',
                     '          echo "exit code: $rc"\n\n      - name: Check the picture (x86_64)\n', 'no test of the x86_64 exit code', 'ends with the test of the exit code')

    @requires_yaml
    def test_mutation_the_arm64_picture_check_is_replaced_by_true(self):
        self.refused('        run: python3 tools/apple/check_png.py "$RUNNER_TEMP/apple-logs/shots/ci_level-macos-arm64.png" --min-size 640x360\n', '        run: "true"\n',
                     'the arm64 picture check replaced by true', 'only runs the picture check')

    @requires_yaml
    def test_mutation_the_arm64_picture_check_is_forgiven(self):
        self.refused('        run: python3 tools/apple/check_png.py "$RUNNER_TEMP/apple-logs/shots/ci_level-macos-arm64.png" --min-size 640x360\n',
                     '        run: python3 tools/apple/check_png.py "$RUNNER_TEMP/apple-logs/shots/ci_level-macos-arm64.png" --min-size 640x360 || true\n',
                     'the arm64 picture check forgiven', 'only runs the picture check')

    @requires_yaml
    def test_mutation_the_arm64_picture_check_loses_its_size(self):
        self.refused('ci_level-macos-arm64.png" --min-size 640x360\n', 'ci_level-macos-arm64.png"\n', 'the arm64 picture check without its size', 'only runs the picture check')

    @requires_yaml
    def test_mutation_the_x86_64_picture_check_is_forgiven(self):
        self.refused('        run: python3 tools/apple/check_png.py "$RUNNER_TEMP/apple-logs/shots/ci_level-macos-x86_64.png" --min-size 640x360\n',
                     '        run: python3 tools/apple/check_png.py "$RUNNER_TEMP/apple-logs/shots/ci_level-macos-x86_64.png" --min-size 640x360 || true\n',
                     'the x86_64 picture check forgiven', 'only runs the picture check')

    @requires_yaml
    def test_mutation_the_run_with_no_data_no_longer_tests_its_exit_code(self):
        self.refused('          test "$rc" = 1\n', '', 'no test of the exit code of the run with no data', 'must exit 1 and say so in words')

    @requires_yaml
    def test_mutation_the_run_with_no_data_no_longer_checks_the_words(self):
        self.refused('          grep -q "did not find the game\'s data" "$RUNNER_TEMP/apple-logs/run-macos-nodata.log"\n', '', 'no check of the words', 'must exit 1 and say so in words')

    @requires_yaml
    def test_mutation_a_launch_without_its_wall_limit(self):
        self.refused("          perl -e 'alarm shift; exec @ARGV' 60 \\\n", '          \\\n', 'no wall limit', 'a launch with no wall limit')

    @requires_yaml
    def test_mutation_the_job_input_changed(self):
        self.refused('          - ios-sim\n          - all\n        default: all', '          - ios-sim\n          - all\n        default: macos', 'default other than all', 'the job input')

    @requires_yaml
    def test_mutation_the_skip_count_dropped(self):
        self.refused("skipped=\"$(grep -c '(Skipped)'", "skipped=\"$(grep -c '(Passed)'", 'the skip count counting passes', 'the skip count is printed')

    @requires_yaml
    def test_mutation_the_ctest_step_forgets_to_fail_on_a_failed_test(self):
        line = '          if [ -n "$failed" ]; then echo "::error::failed tests: $failed"; exit 1; fi\n'
        self.refused(line, '', 'no check for a failed test', 'any failed test fails the step')

    @requires_yaml
    def test_mutation_the_two_failures_are_let_through_by_name_again(self):
        # the list that planning step 209 took out: its return would let test_mp_shoot and test_mp_sprites fail without a red job
        self.refused('          echo "failed: ${failed:-none}"\n', '          known="test_mp_shoot test_mp_sprites"\n          echo "failed: ${failed:-none}"\n', 'a known list back', 'no failure is let through by name')

    @requires_yaml
    def test_mutation_ctest_without_a_wall_limit(self):
        self.refused('ctest --test-dir build --output-on-failure --timeout 900 2>&1', 'ctest --test-dir build --output-on-failure 2>&1', 'no ctest timeout', 'ctest has a wall limit')

    @requires_yaml
    def test_mutation_a_step_after_the_suites_that_a_failure_would_skip(self):
        self.refused('      - name: Build (shell)\n        if: ${{ !cancelled() }}\n', '      - name: Build (shell)\n', 'a step with no !cancelled()', 'would be skipped after a failure')

    # ---- actionlint, where it is on PATH ---------------------------------------------------------------------------------------------------------

    def test_actionlint_finds_nothing(self):
        tool = shutil.which('actionlint')
        if tool is None:
            self.skipTest('actionlint is not on PATH (pip install actionlint-py): this test did NOT run')
        proc = subprocess.run([tool, '-no-color', WORKFLOW], capture_output=True, text=True, timeout=120)
        self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)


if __name__ == '__main__':
    unittest.main()
