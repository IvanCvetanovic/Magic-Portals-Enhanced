#!/usr/bin/env python3
"""The `run:` scripts of one job of a workflow, taken out of the YAML into files (planning step 209, PORT-11), so that a job can be REHEARSED where no runner is, with the text that would run and not a copy typed by hand.

    python tools/ci/job_script.py .github/workflows/ci.yml linux --out DIR [--env NAME=VALUE]...
    then, for each file DIR/NN-<name>.sh in the order of DIR/steps.txt:  bash DIR/NN-<name>.sh

WHAT IT WRITES. One bash script per step that has a `run:` and runs under bash (the default shell of the file), numbered in the job's order, each starting with `set -eo pipefail`, the
variables of the file's, the job's and the step's `env:` that are plain text (`export NAME=value`), and a comment with the step's name and its `if:`. The expression variables
(`${{ ... }}`) are NOT evaluated here: each is listed in the comment as left unset, and `--env NAME=VALUE` sets it. `steps.txt` lists EVERY step in order with what became of it:
a script, a step that is a runner's action (`uses:`, which a rehearsal replaces by its local equivalent or skips and says so), or a step of another shell (cmd), which a Linux
rehearsal cannot run. Nothing is silent. A step that names a variable of the runner (RUNNER_TEMP, GITHUB_WORKSPACE, GITHUB_ENV ...) needs it from the rehearsal's own environment.

WHAT IT IS NOT. It does not run anything and it is not GitHub: the runner's own behaviour (the expressions, the contexts, the actions, the image) is not modelled. It exists so that
the commands a workflow would run are the commands that were run, which is the closest honest evidence a workflow can have before it is dispatched, and the Step says it is that and no more.

Standard library and PyYAML.
"""
from __future__ import annotations

import argparse
import os
import re
import sys

try:
    import yaml
except ImportError:
    yaml = None

EXPRESSION = re.compile(r'\$\{\{.*?\}\}')


def slug(text):
    return re.sub(r'[^A-Za-z0-9]+', '-', text).strip('-').lower()[:48] or 'step'


def quote(value):
    """A value as a single-quoted bash word."""
    return "'" + str(value).replace("'", "'\\''") + "'"


def env_lines(env, overrides):
    """(exports, left unset): `export NAME=value` for each plain variable (or the one --env gave), and the names whose value is an expression with nothing given for them."""
    exports, unset = [], []
    for name, value in (env or {}).items():
        if name in overrides:
            exports.append('export %s=%s' % (name, quote(overrides[name])))
        elif isinstance(value, str) and EXPRESSION.search(value):
            unset.append('%s: %s' % (name, value))
        else:
            exports.append('export %s=%s' % (name, quote(value)))
    for name, value in overrides.items():
        if name not in (env or {}):
            exports.append('export %s=%s' % (name, quote(value)))
    return exports, unset


def extract(text, job_id, overrides=None):
    """[(index, kind, name, script or None, note)] for every step of the job, in order. kind: 'script', 'action' or 'other-shell'."""
    if yaml is None:
        raise RuntimeError('PyYAML is not installed')
    workflow = yaml.safe_load(text)
    jobs = workflow.get('jobs') or {}
    if job_id not in jobs:
        raise KeyError('no job %r (the file has %s)' % (job_id, ', '.join(sorted(jobs))))
    job = jobs[job_id]
    overrides = dict(overrides or {})
    default_shell = (workflow.get('defaults') or {}).get('run', {}).get('shell', 'bash')
    out = []
    for index, step in enumerate(job.get('steps') or [], 1):
        name = step.get('name') or step.get('id') or ('step %d' % index)
        if 'uses' in step:
            out.append((index, 'action', name, None, 'uses %s' % step['uses']))
            continue
        shell = step.get('shell', default_shell)
        if shell != 'bash':
            out.append((index, 'other-shell', name, None, 'shell: %s (a Linux rehearsal cannot run it)' % shell))
            continue
        exports, unset = [], []
        for env in (workflow.get('env'), job.get('env'), step.get('env')):
            e, u = env_lines(env, overrides)
            exports += e
            unset += u
        header = ['#!/usr/bin/env bash', '# step %d of job %s: %s' % (index, job_id, name)]
        if 'if' in step:
            header.append('# if: %s' % step['if'])
        header += ['# left unset (an expression; --env NAME=VALUE sets it): %s' % u for u in unset]
        script = '\n'.join(header + ['set -eo pipefail'] + sorted(set(exports)) + [str(step['run']).rstrip('\n'), ''])
        out.append((index, 'script', name, script, 'if: %s' % step['if'] if 'if' in step else ''))
    return out


def write(steps, out_dir):
    os.makedirs(out_dir, exist_ok=True)
    lines = []
    for index, kind, name, script, note in steps:
        if script is not None:
            file_name = '%02d-%s.sh' % (index, slug(name))
            with open(os.path.join(out_dir, file_name), 'w', encoding='utf-8', newline='\n') as f:
                f.write(script)
            lines.append('%02d\tscript\t%s\t%s\t%s' % (index, file_name, name, note))
        else:
            lines.append('%02d\t%s\t-\t%s\t%s' % (index, kind, name, note))
    with open(os.path.join(out_dir, 'steps.txt'), 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(lines) + '\n')
    return lines


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    p.add_argument('workflow')
    p.add_argument('job')
    p.add_argument('--out', required=True)
    p.add_argument('--env', action='append', default=[], metavar='NAME=VALUE')
    args = p.parse_args(argv)
    if yaml is None:
        print('job_script: NOT RUN - PyYAML is not installed')
        return 3
    overrides = {}
    for item in args.env:
        if '=' not in item:
            print('job_script: --env wants NAME=VALUE, not %r' % item, file=sys.stderr)
            return 2
        name, value = item.split('=', 1)
        overrides[name] = value
    with open(args.workflow, encoding='utf-8') as f:
        text = f.read()
    try:
        steps = extract(text, args.job, overrides)
    except KeyError as e:
        print('job_script: %s' % e.args[0], file=sys.stderr)
        return 2
    for line in write(steps, args.out):
        print(line)
    return 0


if __name__ == '__main__':
    sys.exit(main())
