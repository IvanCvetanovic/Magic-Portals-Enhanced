"""Tests of tools/ci/check_workflows.py and tools/ci/job_script.py (planning step 209, PORT-11): the rules every workflow keeps, and the extraction of a job's scripts.

Run:   python -m unittest discover -s tools/ci -p "test_*.py" -v        (needs PyYAML for the tests marked so: pip install pyyaml in a venv; without it they are SKIPPED and say so, never
       passed; the pure functions - the wall-limit reader, the pipe reader, the line joiner - are tested without it)

WHAT THIS PROVES. That the checker passes the four workflow files as they are, and that EACH of its rules turns red when a copy of a real workflow is edited to break it (one or more
named mutations per rule, applied to the text, the original left alone). That the pure readers see what they claim to (a launch of the program behind any of the wrappers the files
use; `curl | sh` and its relatives, and `yes | sdkmanager` not). That a job's scripts are taken out of the YAML as they are written, and that a step the extraction cannot run is
listed and not dropped.
WHAT IT CANNOT PROVE. That GitHub accepts a file or that a step does what it says on a runner. NOT KNOWN until a dispatch; actionlint (run in the Linux subsystem, not here) reads the
syntax and the expressions and shellcheck the scripts, and neither is behaviour.
"""
import importlib.util
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
WORKFLOWS = os.path.join(REPO, '.github', 'workflows')
sys.path.insert(0, HERE)

import check_workflows as cw  # noqa: E402
import job_script as js  # noqa: E402

try:
    import yaml
except ImportError:
    yaml = None


def read(path):
    with open(path, encoding='utf-8') as f:
        return f.read()


def workflow(name):
    return read(os.path.join(WORKFLOWS, name))


def requires_yaml(test):
    def wrapper(self, *a, **kw):
        if yaml is None:
            print('\nSKIPPED %s: PyYAML is not installed - the rule was NOT checked' % self.id(), file=sys.stderr)
            self.skipTest('PyYAML is not installed (pip install pyyaml in a venv): this test did NOT run')
        return test(self, *a, **kw)
    wrapper.__name__ = test.__name__
    wrapper.__doc__ = test.__doc__
    return wrapper


# ---- the pure readers (no PyYAML) -------------------------------------------------------------------------------------------------------------

class TheReaders(unittest.TestCase):
    def test_continuations_are_joined_and_blank_lines_and_comments_dropped(self):
        script = 'a \\\n  b \\\n  c\n\n# a comment\nd'
        self.assertEqual(cw.logical_lines(script), ['a b c', 'd'])

    def test_a_launch_behind_each_wrapper_the_files_use_is_seen_with_the_wrappers(self):
        cases = {
            'timeout 300 xvfb-run -a -s "-screen 0 1280x720x24" "$B/game/MagicPortals" --level x': ['timeout', 'xvfb-run'],
            'xvfb-run -a timeout 60 "$P/MagicPortals" --frames 1': ['xvfb-run', 'timeout'],
            "perl -e 'alarm shift; exec @ARGV' 300 \"$P/MagicPortals.app/Contents/MacOS/MagicPortals\" --root x": ['timeout'],
            "perl -e 'alarm shift; exec @ARGV' 300 arch -x86_64 \"$P/MagicPortals.app/Contents/MacOS/MagicPortals\" --root x": ['timeout', 'arch'],
            '"$B/game/MagicPortals" --help': [],
            'FOO=bar sudo "$B/game/MagicPortals"': ['sudo'],
            'nohup timeout 5 ./MagicPortals.exe': ['nohup', 'timeout'],
        }
        for line, wrappers in cases.items():
            program, tests = cw.launches(line)
            self.assertEqual(len(program), 1, line)
            self.assertEqual(program[0][1], wrappers, line)

    def test_a_command_that_only_names_the_program_is_not_a_launch(self):
        for line in ('python3 tools/package/release.py check-program "$B/game/MagicPortals" --platform linux', 'cp "$B/game/MagicPortals" "$P/linux/MagicPortals"',
                     'readelf -d "$B/game/MagicPortals" | grep NEEDED', 'ls -l "$P"/player/MagicPortals-*/', 'lipo -archs "$B/game/MagicPortals"',
                     'tar xzf "$ARCHIVE" -C "$P"', 'echo "MagicPortals"', 'cd "$GITHUB_WORKSPACE/MagicPortals"'):
            self.assertEqual(cw.launches(line), ([], []), line)

    def test_the_program_after_a_pipe_or_and_is_a_launch_of_its_own(self):
        program, _ = cw.launches('echo x && "$B/MagicPortals" --help || true')
        self.assertEqual(len(program), 1)
        program, _ = cw.launches('echo x | timeout 5 "$B/MagicPortals"')
        self.assertEqual(program[0][1], ['timeout'])

    def test_ctest_is_seen_with_its_words(self):
        _, tests = cw.launches('ctest --test-dir build --output-on-failure --timeout 900 -j 2 2>&1 | tee log')
        self.assertEqual(len(tests), 1)
        self.assertIn('--timeout', tests[0][2])
        _, tests = cw.launches('ctest -N | grep -c x')
        self.assertEqual(len(tests), 1)

    def test_a_download_piped_into_an_interpreter_is_found(self):
        for line in ('curl -fsSL https://example.org/install.sh | sh', 'curl -fsSL https://example.org/x | sudo bash', 'wget -qO- https://example.org/x | bash -s -- -y',
                     'curl -s https://example.org/x.py | python3 -', 'curl -s https://example.org/x | tee x.sh | sh', 'bash <(curl -s https://example.org/x)',
                     'sh -c "$(curl -fsSL https://example.org/x)"', 'eval "$(wget -qO- https://example.org/x)"', 'curl -fsSL https://example.org/x | perl'):
            self.assertEqual(cw.pipes_into_an_interpreter(line), [line], line)

    def test_what_is_not_a_download_into_an_interpreter_is_not_found(self):
        for line in ('yes | sdkmanager --licenses', 'yes | "$ANDROID_HOME/cmdline-tools/latest/bin/sdkmanager" --licenses > log 2>&1 || true',
                     'echo no | avdmanager create avd -n x', 'curl -fsSL -o "$T/x.tar.gz" https://example.org/x', 'echo "$SHA  $T/x.tar.gz" | sha256sum -c -',
                     'cat file | grep x', 'python3 -c "print(1)" | tee log', 'ls | sh -c "cat"', 'echo hi | bash'):
            self.assertEqual(cw.pipes_into_an_interpreter(line), [], line)

    def test_a_pipe_into_head_is_found_unless_the_line_is_forgiven(self):
        for line in ('ldd --version | head -1', 'cmake --version|head -1', 'java -version 2>&1 | head -1', '"$ANDROID_HOME/emulator/emulator" -version | head -3',
                     'sort -rn | head -40 || false', 'ls | head', 'a | head -n 2 | tee log'):
            self.assertEqual(cw.pipes_into_head(line), [line], line)
        for line in ('sort -rn | head -40 || true', 'ldd --version | sed -n 1p', 'head -30 "$RUNNER_TEMP"/player/MagicPortals-*/PLAYING.txt', 'echo "ahead of time"', 'echo $(( 1 || head ))',
                     '# ldd --version | head -1'):
            self.assertEqual(cw.pipes_into_head(line), [], line)

    def test_an_unbalanced_line_is_looked_at_and_does_not_end_the_run(self):
        self.assertEqual(cw.launches("echo 'unbalanced"), ([], []))
        self.assertEqual(cw.pipes_into_an_interpreter("curl 'unbalanced | sh"), [])

    def test_the_wrappers_arity(self):
        wrappers, rest = cw.skip_wrapper(['timeout', '-k', '5', '300', 'prog', 'x'])
        self.assertEqual((wrappers, rest), (['timeout'], ['prog', 'x']))
        wrappers, rest = cw.skip_wrapper(['xvfb-run', '-a', '-s', '-screen 0 1x1x24', 'prog'])
        self.assertEqual((wrappers, rest), (['xvfb-run'], ['prog']))
        wrappers, rest = cw.skip_wrapper(['prog', '--timeout', '5'])
        self.assertEqual((wrappers, rest), ([], ['prog', '--timeout', '5']))

    def test_the_triggers_of_every_spelling_of_on(self):
        self.assertEqual(cw.triggers_of({True: 'workflow_dispatch'}), {'workflow_dispatch'})
        self.assertEqual(cw.triggers_of({'on': ['push', 'workflow_dispatch']}), {'push', 'workflow_dispatch'})
        self.assertEqual(cw.triggers_of({'on': {'push': None, 'workflow_dispatch': None}}), {'push', 'workflow_dispatch'})
        self.assertEqual(cw.triggers_of({}), set())

    def test_without_pyyaml_the_tool_says_not_run_and_exits_3(self):
        original = cw.yaml
        cw.yaml = None
        try:
            import contextlib
            import io
            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                code = cw.main([])
        finally:
            cw.yaml = original
        self.assertEqual(code, cw.EXIT_NOT_RUN)
        self.assertIn('NOT RUN', out.getvalue())
        self.assertIn('never a pass', out.getvalue())


# ---- the real files, and a mutation for each rule ---------------------------------------------------------------------------------------

class TheRealFiles(unittest.TestCase):
    @requires_yaml
    def test_every_workflow_of_the_repository_keeps_every_rule(self):
        results = cw.check_folder(WORKFLOWS)
        self.assertEqual(sorted(results), ['android-emulator.yml', 'apple-release.yml', 'apple.yml', 'ci.yml', 'release.yml'])
        for name, problems in results.items():
            self.assertEqual([str(p) for p in problems], [], name)

    @requires_yaml
    def test_the_command_line_exits_0_on_the_real_files_and_1_on_a_broken_folder(self):
        import contextlib
        import io
        with contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(cw.main([]), 0)
        tmp = tempfile.mkdtemp(prefix='mp-wf-test-')
        self.addCleanup(shutil.rmtree, tmp, True)
        with open(os.path.join(tmp, 'x.yml'), 'w', encoding='utf-8') as f:
            f.write('on: push\njobs:\n  a:\n    runs-on: ubuntu-latest\n    steps: []\n')
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            self.assertEqual(cw.main(['--workflows', tmp]), 1)
        self.assertIn('W01', out.getvalue())

    @requires_yaml
    def test_a_file_that_is_not_yaml_or_has_no_jobs_is_a_problem_not_a_traceback(self):
        self.assertEqual(cw.check_file('x.yml', ': : :\n  - [')[0].rule, 'W00')
        self.assertEqual(cw.check_file('x.yml', 'on: workflow_dispatch\n')[0].rule, 'W00')


class TheMutations(unittest.TestCase):
    """One edit of a copy of a real file per case; the baseline is asserted clean first, so that a rule that never fired could not pass."""

    def mutated(self, name, old, new, rule, count=1, expect=None):
        text = workflow(name)
        self.assertEqual(text.count(old), count, 'the mutation anchor occurs %d time(s) in %s: %r' % (text.count(old), name, old[:70]))
        self.assertEqual(cw.check_file(name, text), [], 'the file is clean before the edit')
        problems = cw.check_file(name, text.replace(old, new))
        hit = [p for p in problems if p.rule == rule]
        self.assertTrue(hit, 'MUTATION not refused by %s; the problems were %s' % (rule, [str(p) for p in problems]))
        if expect:
            self.assertTrue([p for p in hit if expect in str(p)], '%s was refused, but not with %r: %s' % (rule, expect, [str(p) for p in hit]))

    DISPATCH = 'on:\n  workflow_dispatch:\n'

    # W01
    @requires_yaml
    def test_w01_a_push_trigger(self):
        self.mutated('ci.yml', self.DISPATCH, 'on:\n  push:\n    branches: [main]\n  workflow_dispatch:\n', 'W01', expect='push')

    @requires_yaml
    def test_w01_a_pull_request_trigger_in_the_release_workflow(self):
        self.mutated('release.yml', self.DISPATCH, 'on:\n  pull_request:\n  workflow_dispatch:\n', 'W01', expect='pull_request')

    @requires_yaml
    def test_w01_a_schedule(self):
        self.mutated('android-emulator.yml', self.DISPATCH, 'on:\n  schedule:\n    - cron: "0 3 * * *"\n  workflow_dispatch:\n', 'W01', expect='schedule')

    @requires_yaml
    def test_w01_a_call_trigger_in_a_file_that_nothing_calls(self):
        self.mutated('ci.yml', self.DISPATCH, 'on:\n  workflow_call:\n  workflow_dispatch:\n', 'W01', expect='workflow_call')

    @requires_yaml
    def test_w01_the_dispatch_dropped(self):
        text = workflow('apple.yml')
        a = text.index('  workflow_dispatch:\n')
        b = text.index('  workflow_call:\n')
        problems = cw.check_file('apple.yml', text[:a] + text[b:])
        self.assertTrue([p for p in problems if p.rule == 'W01' and 'no workflow_dispatch' in p.text], [str(p) for p in problems])

    @requires_yaml
    def test_w01_apple_may_be_called_and_that_is_all(self):
        self.assertEqual(cw.check_file('apple.yml', workflow('apple.yml')), [])
        self.mutated('apple.yml', self.DISPATCH, 'on:\n  push:\n  workflow_dispatch:\n', 'W01', expect='push')

    # W02
    @requires_yaml
    def test_w02_write_permission(self):
        self.mutated('ci.yml', 'permissions:\n  contents: read\n', 'permissions:\n  contents: write\n', 'W02')

    @requires_yaml
    def test_w02_a_job_widens_the_permissions(self):
        self.mutated('release.yml', '    name: The version is the project\'s\n', '    name: The version is the project\'s\n    permissions:\n      contents: write\n', 'W02', expect='job version')

    @requires_yaml
    def test_w02_an_environment_or_a_container(self):
        self.mutated('ci.yml', '    name: Tools - Python tests, workflow checks\n', '    name: Tools - Python tests, workflow checks\n    environment: production\n', 'W02', expect='environment')
        self.mutated('ci.yml', '    name: Tools - Python tests, workflow checks\n', '    name: Tools - Python tests, workflow checks\n    container: ubuntu:22.04\n', 'W02', expect='container')

    # W03
    @requires_yaml
    def test_w03_a_secret_by_name_by_index_and_whole(self):
        for expression in ('${{ secrets.SIGNING_KEY }}', "${{ secrets['K'] }}", '${{ toJSON(secrets) }}', '${{ toJSON(github) }}', '${{ github }}'):
            self.mutated('ci.yml', '  CCACHE_MAXSIZE: 600M\n', '  CCACHE_MAXSIZE: 600M\n  KEY: %s\n' % expression, 'W03')

    @requires_yaml
    def test_w03_the_job_token_in_each_spelling(self):
        for line in ('KEY: ${{ github.token }}', "KEY: ${{ github['token'] }}", 'GITHUB_TOKEN: x'):
            self.mutated('ci.yml', '  CCACHE_MAXSIZE: 600M\n', '  CCACHE_MAXSIZE: 600M\n  %s\n' % line, 'W03')

    @requires_yaml
    def test_w03_a_path_of_the_owners_data(self):
        anchor = '          python3 tools/package/release.py check-version\n'
        self.mutated('ci.yml', '        run: python tools/package/check_version.py\n', '        run: python tools/package/check_version.py "$GITHUB_WORKSPACE/reference/extracted"\n', 'W03', expect='reference')
        self.mutated('ci.yml', '        run: python tools/package/check_version.py\n', '        run: python tools/package/check_version.py "$GITHUB_WORKSPACE/out/levels"\n', 'W03', expect='out folder')
        self.mutated('ci.yml', '        run: python tools/package/check_version.py\n', '        run: python tools/package/check_version.py "$X/GameData/levels"\n', 'W03', expect='data folder')
        self.assertTrue(anchor)

    @requires_yaml
    def test_w03_the_content_packagers_reference_flag_is_a_flag_and_not_the_folder(self):
        self.assertIn('--reference', workflow('android-emulator.yml'))
        self.assertEqual(cw.check_file('android-emulator.yml', workflow('android-emulator.yml')), [])

    @requires_yaml
    def test_w03_a_path_of_the_owners_pc(self):
        for text in ('C:/Users/someone/x', 'C:\\Users\\someone\\x', '/mnt/' + 'c/x'):
            self.mutated('ci.yml', '        run: python tools/package/check_version.py\n', '        run: python tools/package/check_version.py %s\n' % text, 'W03')

    @requires_yaml
    def test_w03_the_name_of_the_account_that_runs_the_check_is_asked_of_the_system_and_never_printed(self):
        # an INVENTED name stands for the account (the check reads the real one from the system when it runs, so no file of the project spells it)
        anchor = '        run: python tools/package/check_version.py\n'
        with mock.patch.object(cw, 'running_accounts', return_value=['zorbgar']):
            self.mutated('ci.yml', anchor, '        run: python tools/package/check_version.py x/zorbgar/y\n', 'W03', expect='account that runs the check')
            self.mutated('ci.yml', anchor, '        run: python tools/package/check_version.py # ZORBGAR\n', 'W03', expect='account that runs the check')
            problems = cw.check_file('ci.yml', workflow('ci.yml').replace(anchor, anchor + '        # zorbgar\n'))
            self.assertNotIn('zorbgar', ' '.join(str(p) for p in problems).lower())
            # a name of five letters or more is a substring search (fix round 2): glued to letters or digits it is still the account
            for glued in ('zorbgarage', 'xxzorbgar', 'zorbgar2'):
                self.mutated('ci.yml', anchor, anchor + '        # %s\n' % glued, 'W03', expect='account that runs the check')
        # a name of four letters is a whole word: inside a longer word it is ordinary text, alone it is the account
        with mock.patch.object(cw, 'running_accounts', return_value=['zorb']):
            self.assertEqual(cw.check_file('ci.yml', workflow('ci.yml').replace(anchor, anchor + '        # zorbing xzorbx zorb2\n')), [])
            self.mutated('ci.yml', anchor, anchor + '        # a zorb here\n', 'W03', expect='account that runs the check')
        # a short or ordinary name is a word of ordinary text and is not searched
        with mock.patch.object(cw, 'running_accounts', return_value=['bo', 'runner']):
            self.assertEqual(cw.check_file('ci.yml', workflow('ci.yml').replace(anchor, anchor + '        # bo runner\n')), [])

    @requires_yaml
    def test_w03_the_name_of_another_project_or_the_originals_maker(self):
        for name in (cw.OTHER_PROJECT, cw.ORIGINAL_MAKER):
            self.mutated('ci.yml', '        run: python tools/package/check_version.py\n', '        run: python tools/package/check_version.py # %s\n' % name, 'W03')

    # W04
    @requires_yaml
    def test_w04_a_tag_instead_of_a_sha(self):
        self.mutated('release.yml', 'actions/download-artifact@d3f86a106a0bac45b974a628896c90dbdf5c8093 # v4.3.0', 'actions/download-artifact@v4', 'W04', expect='v4')

    @requires_yaml
    def test_w04_a_short_sha_a_branch_and_no_version(self):
        sha = 'actions/upload-artifact@ea165f8d65b6e75b540449e92b4886f43607fa02'
        text = workflow('android-emulator.yml')
        self.assertGreaterEqual(text.count(sha), 1)
        for bad in ('actions/upload-artifact@ea165f8d', 'actions/upload-artifact@main', 'actions/upload-artifact', 'docker://alpine:3'):
            problems = cw.check_file('android-emulator.yml', text.replace(sha, bad))
            self.assertTrue([p for p in problems if p.rule == 'W04'], bad)

    @requires_yaml
    def test_w04_a_call_of_a_workflow_that_is_not_there(self):
        self.mutated('release.yml', 'uses: ./.github/workflows/apple.yml\n    with:\n      job: macos\n', 'uses: ./.github/workflows/nope.yml\n    with:\n      job: macos\n', 'W04', expect='not in the repository')

    @requires_yaml
    def test_w04_a_job_that_calls_a_workflow_of_another_repository_by_tag(self):
        self.mutated('release.yml', 'uses: ./.github/workflows/apple.yml\n    with:\n      job: macos\n', 'uses: someone/else/.github/workflows/x.yml@v1\n    with:\n      job: macos\n', 'W04')

    # W05
    @requires_yaml
    def test_w05_no_timeout(self):
        self.mutated('ci.yml', "    if: ${{ inputs.job == 'all' || inputs.job == 'tools' }}\n    runs-on: ubuntu-24.04\n    timeout-minutes: 45\n",
                     "    if: ${{ inputs.job == 'all' || inputs.job == 'tools' }}\n    runs-on: ubuntu-24.04\n", 'W05', expect='tools')

    @requires_yaml
    def test_w05_a_timeout_that_is_a_day(self):
        self.mutated('ci.yml', "    runs-on: ubuntu-24.04\n    timeout-minutes: 45\n", "    runs-on: ubuntu-24.04\n    timeout-minutes: 1440\n", 'W05')

    @requires_yaml
    def test_w05_a_called_workflow_needs_none_in_the_caller(self):
        self.assertEqual(cw.check_file('release.yml', workflow('release.yml')), [])

    # W06
    @requires_yaml
    def test_w06_the_logs_upload_loses_its_always(self):
        self.mutated('ci.yml', "      - name: Upload logs\n        if: always()\n        uses: actions/upload-artifact@ea165f8d65b6e75b540449e92b4886f43607fa02 # v4.6.2\n        with:\n          name: tools-logs\n",
                     "      - name: Upload logs\n        uses: actions/upload-artifact@ea165f8d65b6e75b540449e92b4886f43607fa02 # v4.6.2\n        with:\n          name: tools-logs\n", 'W06', expect='tools')

    @requires_yaml
    def test_w06_the_logs_upload_runs_only_on_success(self):
        self.mutated('ci.yml', "        if: always()\n        uses: actions/upload-artifact@ea165f8d65b6e75b540449e92b4886f43607fa02 # v4.6.2\n        with:\n          name: tools-logs\n",
                     "        if: success()\n        uses: actions/upload-artifact@ea165f8d65b6e75b540449e92b4886f43607fa02 # v4.6.2\n        with:\n          name: tools-logs\n", 'W06')

    @requires_yaml
    def test_w06_a_step_after_the_upload(self):
        text = workflow('release.yml')
        extra = '      - name: One more\n        run: echo more\n'
        problems = cw.check_file('release.yml', text.rstrip('\n') + '\n' + extra)
        self.assertTrue([p for p in problems if p.rule == 'W06'], [str(p) for p in problems])

    # W07
    @requires_yaml
    def test_w07_a_launch_with_no_wall_limit(self):
        self.mutated('ci.yml', '          timeout 300 xvfb-run -a -s "-screen 0 1280x720x24" "$RUNNER_TEMP/build/game/MagicPortals"', '          xvfb-run -a -s "-screen 0 1280x720x24" "$RUNNER_TEMP/build/game/MagicPortals"',
                     'W07', expect='no wall limit')

    @requires_yaml
    def test_w07_the_run_with_no_data_has_one_too(self):
        self.mutated('ci.yml', '          timeout 60 xvfb-run -a "$player/MagicPortals"', '          xvfb-run -a "$player/MagicPortals"', 'W07')

    @requires_yaml
    def test_w07_the_apple_launches_keep_their_alarm(self):
        text = workflow('apple.yml')
        problems = cw.check_file('apple.yml', text.replace("          perl -e 'alarm shift; exec @ARGV' 60 \\\n", '          \\\n'))
        self.assertTrue([p for p in problems if p.rule == 'W07'], [str(p) for p in problems])

    @requires_yaml
    def test_w07_ctest_without_a_timeout(self):
        self.mutated('ci.yml', 'ctest --test-dir "$build" --output-on-failure --timeout 900 2>&1', 'ctest --test-dir "$build" --output-on-failure 2>&1', 'W07', expect='ctest')

    # W08
    @requires_yaml
    def test_w08_a_job_waits_for_another_where_the_table_says_it_does_not(self):
        self.mutated('ci.yml', "    if: ${{ inputs.job == 'all' || inputs.job == 'android' }}\n", "    if: ${{ inputs.job == 'all' || inputs.job == 'android' }}\n    needs: [linux]\n", 'W08', expect='android')

    @requires_yaml
    def test_w08_the_collect_job_stops_waiting_for_one_of_them(self):
        self.mutated('release.yml', '    needs: [version, linux, windows, android, apple-macos, apple-ios]\n', '    needs: [version, linux, windows, android, apple-macos]\n', 'W08', expect='collect')

    @requires_yaml
    def test_w08_a_build_job_stops_waiting_for_the_version_check(self):
        self.mutated('release.yml', '    name: Linux program\n    needs: [version]\n', '    name: Linux program\n', 'W08', expect='linux')

    # W09
    @requires_yaml
    def test_w09_a_download_piped_into_a_shell(self):
        self.mutated('ci.yml', '        run: python tools/package/check_version.py\n', '        run: curl -fsSL https://example.org/install.sh | sh\n', 'W09')

    @requires_yaml
    def test_w09_a_process_substitution_of_a_download(self):
        self.mutated('ci.yml', '        run: python tools/package/check_version.py\n', '        run: bash <(curl -fsSL https://example.org/install.sh)\n', 'W09')

    @requires_yaml
    def test_w09_the_licence_line_of_the_sdk_manager_is_not_a_download(self):
        self.assertIn('yes | "$ANDROID_HOME/cmdline-tools/latest/bin/sdkmanager" --licenses', workflow('ci.yml'))
        self.assertEqual([p for p in cw.check_file('ci.yml', workflow('ci.yml')) if p.rule == 'W09'], [])

    # W10
    @requires_yaml
    def test_w10_an_image_that_moves(self):
        self.mutated('ci.yml', "    if: ${{ inputs.job == 'all' || inputs.job == 'tools' }}\n    runs-on: ubuntu-24.04\n", "    if: ${{ inputs.job == 'all' || inputs.job == 'tools' }}\n    runs-on: ubuntu-latest\n", 'W10', expect='ubuntu-latest')

    # W11
    @requires_yaml
    def test_w11_a_step_that_may_fail_and_be_forgiven(self):
        self.mutated('ci.yml', '      - name: The workflow checker\n', '      - name: The workflow checker\n        continue-on-error: true\n', 'W11')

    @requires_yaml
    def test_w11_a_job_that_may_fail_and_be_forgiven(self):
        self.mutated('ci.yml', "    if: ${{ inputs.job == 'all' || inputs.job == 'tools' }}\n", "    if: ${{ inputs.job == 'all' || inputs.job == 'tools' }}\n    continue-on-error: true\n", 'W11')

    @requires_yaml
    def test_w11_only_the_rosetta_step_is_forgiven_and_only_in_apple(self):
        self.assertEqual(cw.check_file('apple.yml', workflow('apple.yml')), [])
        self.mutated('apple.yml', '      - name: Run the fixture (arm64)\n        if: ${{ !cancelled() }}\n', '      - name: Run the fixture (arm64)\n        if: ${{ !cancelled() }}\n        continue-on-error: true\n', 'W11')

    # W12
    @requires_yaml
    def test_w12_an_expression_inside_a_run(self):
        self.mutated('ci.yml', '        run: python tools/package/check_version.py\n', '        run: echo ${{ github.sha }}\n', 'W12')

    # W13
    @requires_yaml
    def test_w13_the_default_shell_is_not_bash(self):
        self.mutated('ci.yml', '    shell: bash\n', '    shell: sh\n', 'W13')

    @requires_yaml
    def test_w13_a_step_in_another_shell(self):
        self.mutated('ci.yml', '      - name: Make the icon\n        run: python tools/art/make_app_icon.py --out "$RUNNER_TEMP/icons"\n',
                     '      - name: Make the icon\n        shell: pwsh\n        run: python tools/art/make_app_icon.py --out "$RUNNER_TEMP/icons"\n', 'W13', expect='pwsh')

    @requires_yaml
    def test_w13_cmd_is_allowed_where_msvc_needs_it(self):
        self.assertIn('shell: cmd', workflow('ci.yml'))
        self.assertEqual([p for p in cw.check_file('ci.yml', workflow('ci.yml')) if p.rule == 'W13'], [])

    # W14
    @requires_yaml
    def test_w14_two_steps_of_a_job_with_one_name(self):
        self.mutated('ci.yml', '      - name: The version in every place it is written\n', '      - name: The workflow checker\n', 'W14')

    # W15
    @requires_yaml
    def test_w15_an_upload_that_keeps_its_files_for_ninety_days(self):
        self.mutated('ci.yml', "          name: tools-logs\n          retention-days: 7\n", "          name: tools-logs\n          retention-days: 90\n", 'W15')

    # W16
    @requires_yaml
    def test_w16_the_version_of_ldd_piped_into_head(self):
        # measured on Ubuntu 24.04 (the verifier of step 209): this very line failed 33 times in 40 runs under pipefail, exit 141
        self.mutated('ci.yml', '          ldd --version | sed -n 1p\n', '          ldd --version | head -1\n', 'W16', expect='SIGPIPE')

    @requires_yaml
    def test_w16_the_same_line_of_the_release_workflow(self):
        self.mutated('release.yml', '            ldd --version | sed -n 1p\n', '            ldd --version | head -1\n', 'W16')

    @requires_yaml
    def test_w16_a_forgiven_head_is_the_one_line_apple_keeps(self):
        text = workflow('apple.yml')
        self.assertIn('| sort -rn | head -40 || true', text)
        self.assertEqual([p for p in cw.check_file('apple.yml', text) if p.rule == 'W16'], [])
        self.assertEqual([p for p in cw.check_file('apple.yml', text.replace('| head -40 || true', '| head -40')) if p.rule == 'W16'][0].rule, 'W16')

    @requires_yaml
    def test_w15_an_upload_that_says_nothing(self):
        self.mutated('ci.yml', "          name: tools-logs\n          retention-days: 7\n", "          name: tools-logs\n", 'W15')


class TheTies(unittest.TestCase):
    def test_the_apt_packages_of_the_linux_jobs_hold_the_ones_the_build_script_names(self):
        header = read(os.path.join(REPO, 'tools', 'build_linux.sh'))
        m = re.search(r'sudo apt-get install -y ((?:[^\n]*\\\n#\s*)*[^\n]*)', header)
        self.assertTrue(m, 'the header names the packages')
        wanted = set(re.sub(r'[\\#]', ' ', m.group(1)).split())
        self.assertIn('libvulkan-dev', wanted)
        self.assertIn('ninja-build', wanted)
        ci = workflow('ci.yml')
        for job in ('linux', 'linux-release', 'tools'):
            body = ci[ci.index('\n  %s:\n' % job):]
            install = re.search(r'sudo apt-get install -y ([^\n]+)', body).group(1).split()
            self.assertEqual(sorted(wanted - set(install)), [], 'the job %s installs every package the build script names' % job)
            if job != 'tools':   # the tools job runs a configure and no program: no display, no Vulkan driver
                for runtime in ('xvfb', 'mesa-vulkan-drivers'):
                    self.assertIn(runtime, install, job)

    def test_the_glibc_floor_is_one_number_in_both_workflows_and_the_documents_that_state_it(self):
        numbers = {n: re.search(r'LINUX_GLIBC_FLOOR: "([0-9.]+)"', workflow(n)).group(1) for n in ('ci.yml', 'release.yml')}
        self.assertEqual(len(set(numbers.values())), 1, numbers)
        floor = set(numbers.values()).pop()
        # the documents that state it: the recipe in ci.md and packaging.md names the floor the workflows pass, and no document passes another one (platforms.md only points at them)
        for doc in ('ci.md', 'packaging.md'):
            self.assertIn('--max-glibc ' + floor, read(os.path.join(REPO, 'docs', doc)), doc)
        for doc in ('ci.md', 'packaging.md', 'platforms.md', 'building.md'):
            others = set(re.findall(r'--max-glibc (\d+\.\d+)', read(os.path.join(REPO, 'docs', doc)))) - {floor}
            # building.md and ci.md also show a floor of the build machine's own (2.38) for the rehearsal and for the refusal that proves the check
            self.assertLessEqual(others, {'2.38'}, '%s passes a floor other than the workflows\' %s: %s' % (doc, floor, sorted(others)))

    def test_no_workflow_builds_the_default_target_of_the_windows_tree(self):
        # the default target also builds MagicPortalsSpike (game/CMakeLists.txt), the stale spike CLAUDE.md says is never built; the Linux jobs build build_linux.sh's two targets
        for name in ('ci.yml', 'release.yml'):
            for line in workflow(name).split('\n'):
                if 'cmake --build' in line:
                    self.assertIn('--target', line, '%s builds the default target: %s' % (name, line.strip()[:100]))
        self.assertIn('--target MagicPortals test_mp >', workflow('ci.yml'))

    def test_the_android_tool_versions_are_one_set_in_all_three_workflows(self):
        for key in ('ANDROID_NDK_VERSION', 'ANDROID_BUILD_TOOLS_VERSION', 'ANDROID_PLATFORM'):
            values = {n: re.search(r'  %s: (\S+)' % key, workflow(n)).group(1) for n in ('ci.yml', 'release.yml', 'android-emulator.yml')}
            self.assertEqual(len(set(values.values())), 1, values)
        self.assertEqual(re.search(r'ANDROID_NDK_VERSION: (\S+)', workflow('ci.yml')).group(1), '28.2.13676358', 'the NDK of docs/android.md section 1')
        self.assertIn('28.2.13676358', read(os.path.join(REPO, 'docs', 'android.md')))

    def test_the_actionlint_release_is_pinned_by_a_real_sha256(self):
        ci = workflow('ci.yml')
        sha = re.search(r'ACTIONLINT_SHA256: (\S+)', ci).group(1)
        self.assertRegex(sha, r'^[0-9a-f]{64}$')
        self.assertNotEqual(sha, '0' * 64)
        self.assertRegex(re.search(r'ACTIONLINT_VERSION: (\S+)', ci).group(1), r'^\d+\.\d+\.\d+$')
        self.assertIn('sha256sum -c -', ci)
        self.assertLess(ci.index('sha256sum -c -'), ci.index('tar xzf "$RUNNER_TEMP/actionlint.tar.gz"'), 'checked before it is unpacked')

    def test_the_avd_of_the_emulator_workflow_is_made_from_the_maker_scripts_own_table(self):
        text = workflow('android-emulator.yml')
        self.assertIn('e.apply_config(os.path.expanduser("~/.android/avd/smoke.avd/config.ini"), e.AVD_CONFIG)', text)
        spec = importlib.util.spec_from_file_location('android_emulator_for_the_tie', os.path.join(REPO, 'tools', 'android_emulator.py'))
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        self.assertEqual(module.AVD_CONFIG['hw.lcd.width'], '720')
        self.assertTrue(callable(module.apply_config))
        self.assertEqual(module.MIN_FREE_MEMORY_BYTES, 6 * 1024 ** 3, 'the 6 GiB floor of the maker\'s script is untouched')

    def test_the_release_workflow_waits_as_the_checkers_table_says(self):
        if yaml is None:
            print('\nSKIPPED %s: PyYAML is not installed' % self.id(), file=sys.stderr)
            self.skipTest('PyYAML is not installed')
        release = yaml.safe_load(workflow('release.yml'))
        got = {j: sorted(v.get('needs') or []) for j, v in release['jobs'].items()}
        want = {j: sorted(cw.NEEDS['release.yml'].get(j, [])) for j in release['jobs']}
        self.assertEqual(got, want)

    def test_the_fixture_run_flags_are_the_ones_apple_uses(self):
        for name in ('ci.yml',):
            text = workflow(name)
            self.assertIn('--root "$GITHUB_WORKSPACE/tests/fixtures/ci" --port-data "$GITHUB_WORKSPACE/game/data" --level ci_level', text)
            self.assertIn('--window 1280x720 --fixed-step --frames 300', text)
            self.assertIn('--min-size 640x360', text)

    def test_the_windows_fixture_run_is_said_not_to_have_run(self):
        text = workflow('ci.yml')
        self.assertIn('NOT RUN', text[text.index('Run the fixture (NOT RUN on Windows)'):])
        self.assertNotIn('MagicPortals.exe" --root', text, 'no launch of the Windows program with a fixture')

    def test_no_workflow_says_that_a_step_works_on_github(self):
        for name in sorted(os.listdir(WORKFLOWS)):
            text = workflow(name).lower()
            self.assertIn('never', text[:3000], name)
            for phrase in ('verified on github', 'has been run on github'):
                self.assertNotIn(phrase, text, name)
            self.assertIsNone(re.search(r'(?<!says that a step )works on github', text), name)  # "nothing here says that a step works on GitHub" is the denial, not the claim

    def test_the_emulator_workflow_says_in_its_first_lines_that_it_was_never_dispatched(self):
        self.assertIn('WRITTEN FOR A RUNNER, NEVER DISPATCHED', workflow('android-emulator.yml')[:200])


# ---- job_script --------------------------------------------------------------------------------------------------------------------------

class TheExtraction(unittest.TestCase):
    @requires_yaml
    def test_every_step_of_the_linux_job_is_accounted_for(self):
        text = workflow('ci.yml')
        steps = js.extract(text, 'linux')
        job = yaml.safe_load(text)['jobs']['linux']
        self.assertEqual(len(steps), len(job['steps']))
        kinds = [k for _, k, _, _, _ in steps]
        self.assertEqual(kinds.count('action'), sum(1 for s in job['steps'] if 'uses' in s))
        self.assertEqual(kinds.count('script'), sum(1 for s in job['steps'] if 'run' in s))
        self.assertNotIn('other-shell', kinds)
        self.assertEqual([i for i, *_ in steps], list(range(1, len(steps) + 1)))

    @requires_yaml
    def test_the_windows_job_lists_its_cmd_steps_and_does_not_drop_them(self):
        steps = js.extract(workflow('ci.yml'), 'windows')
        others = [n for _, k, n, _, _ in steps if k == 'other-shell']
        self.assertEqual(others, ['Build (program and suites)', 'Build (release program)'])

    @requires_yaml
    def test_each_script_is_valid_bash_and_starts_the_way_the_runner_runs_it(self):
        bash = shutil.which('bash')
        if bash is None or 'system32' in bash.lower():
            print('\nSKIPPED %s: no bash' % self.id(), file=sys.stderr)
            self.skipTest('no bash: the scripts were NOT syntax-checked')
        tmp = tempfile.mkdtemp(prefix='mp-jobscript-')
        self.addCleanup(shutil.rmtree, tmp, True)
        for workflow_name, job in (('ci.yml', 'linux'), ('ci.yml', 'linux-release'), ('ci.yml', 'tools'), ('release.yml', 'collect'), ('android-emulator.yml', 'smoke')):
            folder = os.path.join(tmp, workflow_name, job)
            lines = js.write(js.extract(workflow(workflow_name), job), folder)
            for line in lines:
                parts = line.split('\t')
                if parts[1] == 'script':
                    path = os.path.join(folder, parts[2])
                    done = subprocess.run([bash, '-n', path], capture_output=True, text=True)
                    self.assertEqual(done.returncode, 0, path + done.stderr)
                    text = read(path)
                    self.assertIn('set -eo pipefail', text)
                    self.assertTrue(text.startswith('#!/usr/bin/env bash\n'))

    @requires_yaml
    def test_plain_env_is_exported_and_an_expression_is_left_unset_and_says_so(self):
        text = ("name: x\non: workflow_dispatch\nenv:\n  A: one\n  B: ${{ inputs.b }}\njobs:\n  j:\n    runs-on: ubuntu-24.04\n    env:\n      C: \"it's\"\n"
                "    steps:\n      - name: First step\n        env:\n          D: ${{ github.sha }}\n        run: |\n          echo \"$A $C\"\n")
        steps = js.extract(text, 'j')
        script = steps[0][3]
        self.assertIn("export A='one'", script)
        self.assertIn("export C='it'\\''s'", script)
        self.assertNotIn('export B', script)
        self.assertIn('# left unset (an expression; --env NAME=VALUE sets it): B: ${{ inputs.b }}', script)
        self.assertIn('# left unset (an expression; --env NAME=VALUE sets it): D: ${{ github.sha }}', script)
        given = js.extract(text, 'j', {'B': 'b value', 'E': 'extra'})[0][3]
        self.assertIn("export B='b value'", given)
        self.assertIn("export E='extra'", given)

    @requires_yaml
    def test_an_unknown_job_is_a_key_error_naming_the_jobs(self):
        with self.assertRaises(KeyError) as caught:
            js.extract(workflow('ci.yml'), 'nope')
        self.assertIn('linux', caught.exception.args[0])

    @requires_yaml
    def test_the_written_files_are_the_scripts_and_steps_txt_lists_everything(self):
        tmp = tempfile.mkdtemp(prefix='mp-jobscript-')
        self.addCleanup(shutil.rmtree, tmp, True)
        steps = js.extract(workflow('ci.yml'), 'linux-release')
        lines = js.write(steps, tmp)
        listed = read(os.path.join(tmp, 'steps.txt')).splitlines()
        self.assertEqual(listed, lines)
        self.assertEqual(len(lines), len(steps))
        files = sorted(n for n in os.listdir(tmp) if n.endswith('.sh'))
        self.assertEqual(len(files), sum(1 for s in steps if s[1] == 'script'))
        self.assertTrue(all(re.match(r'^\d\d-[a-z0-9-]+\.sh$', n) for n in files), files)
        again = tempfile.mkdtemp(prefix='mp-jobscript-')
        self.addCleanup(shutil.rmtree, again, True)
        js.write(js.extract(workflow('ci.yml'), 'linux-release'), again)
        for n in files:
            self.assertEqual(read(os.path.join(tmp, n)), read(os.path.join(again, n)), 'the same text twice')

    @requires_yaml
    def test_the_command_line_writes_the_folder_and_reports_a_missing_job(self):
        import contextlib
        import io
        tmp = tempfile.mkdtemp(prefix='mp-jobscript-')
        self.addCleanup(shutil.rmtree, tmp, True)
        with contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(js.main([os.path.join(WORKFLOWS, 'ci.yml'), 'tools', '--out', tmp, '--env', 'FOO=bar']), 0)
        self.assertTrue(os.path.isfile(os.path.join(tmp, 'steps.txt')))
        with contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(js.main([os.path.join(WORKFLOWS, 'ci.yml'), 'nope', '--out', tmp]), 2)
            self.assertEqual(js.main([os.path.join(WORKFLOWS, 'ci.yml'), 'tools', '--out', tmp, '--env', 'FOO']), 2)


if __name__ == '__main__':
    unittest.main()
