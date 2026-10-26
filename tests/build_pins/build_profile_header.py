#!/usr/bin/env python3
"""PIN C2 (ctest test_mp_build_profile_header): a translation unit that includes game/BuildProfile.hpp WITHOUT the build constants every source of the game is given
FAILS TO COMPILE, with the header's own #error; the same unit given the constants compiles.

WHY. A second file of a mobile program that included the header and was not given MAGICPORTALS_MOBILE, MAGICPORTALS_DEV_TOOLS and MAGICPORTALS_PERF_DIAG used to compile,
quietly disagree with main.cpp, and make `kMobileBuild` mean two things in one program (planning step 202). The header's two #error lines stop that, and nothing in the
build checked that they still do: a deleted #error would leave an undeclared-identifier error in some cases and a silent compile in others.

HOW. A one-line translation unit is written to a scratch folder and compiled with `-fsyntax-only` (no object, no link, nothing is started) against the real header, with:
  * none of the constants, then each of the three missing in turn: must fail with the SECOND #error's text, at its line in the header, and not the first's;
  * all three, in every combination of 0 and 1: must compile, and the constants the header derives (kMobileBuild, kDevTools, kPerfDiag) must be the ones given;
  * the suites' own declaration MAGICPORTALS_BUILD_PROFILE_DESKTOP alone: must compile, and mean "no phone, the developer's tools in, no readout" (the positive control);
  * that declaration together with one of the three: must fail with the FIRST #error's text, at its line.
Both messages are read out of the header (`#error "..."`), not retyped, and compared with the whitespace collapsed. A compile that failed because there is no compiler, or
because of a typo, shows none of them and is SKIPPED or FAILED, never a pass. The named mutations take an #error out, weaken a condition or change the desktop meaning of a
temporary copy of the header; each must turn the pin red. The real header is never written.
"""
import itertools
import os
import re
import shutil
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pin_common as pc  # noqa: E402

FACTS = ('MAGICPORTALS_MOBILE', 'MAGICPORTALS_DEV_TOOLS', 'MAGICPORTALS_PERF_DIAG')
DESKTOP = 'MAGICPORTALS_BUILD_PROFILE_DESKTOP'
CONSTANTS = ('kMobileBuild', 'kDevTools', 'kPerfDiag')


def error_texts(text):
    """[(line, message)] of the lines of the header that carry a message, in file order. A line is `#error "BuildProfile.hpp: ..."`; a line whose directive was taken
    away (commented out, turned into a #warning) still carries its message, which is the point: what the compiler then does is what the pin looks at."""
    found = []
    for n, line in enumerate(text.split('\n'), 1):
        m = re.match(r'^(?:#\s*\w+\s+|//\s*)?"(BuildProfile\.hpp: .*)"\s*$', line)
        if m:
            found.append((n, m.group(1)))
    return found


def unit(expect=None):
    """The translation unit: the include, and when `expect` (three 0/1) is given, a static_assert per constant that the header derives."""
    lines = ['#include "BuildProfile.hpp"']
    if expect is not None:
        for name, value in zip(CONSTANTS, expect):
            lines.append('static_assert(MagicPortals::%s == %s, "%s is not what was asked");' % (name, 'true' if value else 'false', name))
    return '\n'.join(lines) + '\nint main() { return 0; }\n'


def compile_unit(cxx, header_dir, scratch, defines, expect=None):
    source = os.path.join(scratch, 'unit.cpp')
    with open(source, 'w', encoding='utf-8', newline='\n') as f:
        f.write(unit(expect))
    cmd = [cxx, '-std=c++20', '-fsyntax-only', '-I', header_dir, source] + ['-D' + d for d in defines]
    done = subprocess.run(cmd, capture_output=True, text=True, errors='replace', timeout=120)
    return done.returncode, done.stdout + done.stderr


def pin(path, args):
    cxx = pc.which(args.cxx_compiler) if args.cxx_compiler else (pc.which('g++') or pc.which('c++') or pc.which('clang++'))
    if not cxx:
        raise pc.Skip('no C++ compiler to run (--cxx-compiler, or g++ or clang++ on PATH)')
    if args.cxx_id and args.cxx_id not in ('GNU', 'Clang', 'AppleClang'):
        raise pc.Skip('the compiler of this tree is %s; the pin runs with GCC or Clang (-fsyntax-only)' % args.cxx_id)
    text = pc.read_text(path)
    errors = error_texts(text)
    if len(errors) != 2:
        raise pc.Fail('the header has %d line(s) that carry a "BuildProfile.hpp: ..." message; the pin expects exactly 2 (the conflict and the missing constants)' % len(errors))
    (line_conflict, conflict), (line_missing, missing) = errors
    if 'MAGICPORTALS_BUILD_PROFILE_DESKTOP' not in conflict or 'MAGICPORTALS_MOBILE, MAGICPORTALS_DEV_TOOLS or MAGICPORTALS_PERF_DIAG' not in conflict:
        raise pc.Fail('the first #error is no longer the conflict one (the declaration together with the facts)')
    if 'neither as all three' not in missing:
        raise pc.Fail('the second #error is no longer the missing-constants one')
    header_dir = os.path.dirname(os.path.abspath(path))
    name = os.path.basename(path)
    scratch = tempfile.mkdtemp(prefix='mp-pin-header-')
    shown = os.path.basename(path)

    def at_line(flat, line):
        return ('%s:%d:' % (shown, line)) in flat

    try:
        if name != 'BuildProfile.hpp':
            copy = os.path.join(scratch, 'inc')
            os.makedirs(copy)
            shutil.copyfile(path, os.path.join(copy, 'BuildProfile.hpp'))
            header_dir, shown = copy, 'BuildProfile.hpp'

        # the positive control first: the suites' declaration alone compiles, and means what the header's comment says
        code, out = compile_unit(cxx, header_dir, scratch, [DESKTOP], expect=(0, 1, 0))
        if code != 0:
            if 'not found' in out or 'No such file' in out and 'BuildProfile.hpp' not in out:
                raise pc.Skip('the compiler cannot run here: ' + pc.normalise(out)[:200])
            raise pc.Fail('the control did not compile (%s alone, expecting no phone, tools in, no readout):\n%s' % (DESKTOP, pc.normalise(out)[:600]))

        # 1. no constants, and each of the three missing: the missing-constants #error, at its line, and not the conflict one
        cases = [[]] + [[d + '=0' for d in FACTS if d != gone] for gone in FACTS]
        for defines in cases:
            code, out = compile_unit(cxx, header_dir, scratch, defines)
            flat = pc.normalise(out)
            label = 'given %s' % (' '.join(defines) or 'none of the constants')
            if code == 0:
                raise pc.Fail('%s: the unit compiled' % label)
            if pc.normalise(missing) not in flat:
                raise pc.Fail("%s: it failed, but not with the header's missing-constants #error:\n%s" % (label, flat[:500]))
            if not at_line(flat, line_missing):
                raise pc.Fail('%s: the #error is not reported at the line the header puts it (%s:%d)' % (label, shown, line_missing))
            if pc.normalise(conflict) in flat:
                raise pc.Fail("%s: it failed with the conflict #error, not the missing-constants one" % label)

        # 2. all three, every combination: it compiles, and the constants are the ones given (the positive controls)
        for combo in itertools.product((0, 1), repeat=3):
            code, out = compile_unit(cxx, header_dir, scratch, ['%s=%d' % (d, v) for d, v in zip(FACTS, combo)], expect=combo)
            if code != 0:
                raise pc.Fail('all three constants given as %s: the unit did not compile:\n%s' % (combo, pc.normalise(out)[:500]))

        # 3. the suites' declaration together with one of the facts: the conflict #error
        for fact in FACTS:
            for value in (0, 1):
                code, out = compile_unit(cxx, header_dir, scratch, [DESKTOP, '%s=%d' % (fact, value)])
                flat = pc.normalise(out)
                label = '%s together with %s=%d' % (DESKTOP, fact, value)
                if code == 0:
                    raise pc.Fail('%s: the unit compiled' % label)
                if pc.normalise(conflict) not in flat:
                    raise pc.Fail("%s: it failed, but not with the header's conflict #error:\n%s" % (label, flat[:500]))
                if not at_line(flat, line_conflict):
                    raise pc.Fail('%s: the #error is not reported at the line the header puts it (%s:%d)' % (label, shown, line_conflict))
        print('  %d cases without the constants stop at %s:%d, 8 with all three compile with the constants asked for, %s alone compiles, and with a fact stops at %s:%d'
              % (len(cases), shown, line_missing, DESKTOP, shown, line_conflict))
    finally:
        shutil.rmtree(scratch, ignore_errors=True)


def mutations_of(text):
    """(name, old, new): the header weakened in one named way each, on a temporary copy."""
    conflict_head = '#error "BuildProfile.hpp: this translation unit says it is'
    missing_head = '#error "BuildProfile.hpp: this translation unit was given'
    return [
        ('delete-the-missing-constants-error', missing_head, '// "BuildProfile.hpp: this translation unit was given'),
        ('delete-the-conflict-error', conflict_head, '// "BuildProfile.hpp: this translation unit says it is'),
        ('two-of-three-is-enough', '#elif !defined(MAGICPORTALS_MOBILE) || !defined(MAGICPORTALS_DEV_TOOLS) || !defined(MAGICPORTALS_PERF_DIAG)',
         '#elif !defined(MAGICPORTALS_MOBILE) && !defined(MAGICPORTALS_DEV_TOOLS) && !defined(MAGICPORTALS_PERF_DIAG)'),
        ('conflict-forgets-the-readout', '#if defined(MAGICPORTALS_MOBILE) || defined(MAGICPORTALS_DEV_TOOLS) || defined(MAGICPORTALS_PERF_DIAG)',
         '#if defined(MAGICPORTALS_MOBILE) || defined(MAGICPORTALS_DEV_TOOLS)'),
        ('the-desktop-declaration-is-gone', '#if defined(MAGICPORTALS_BUILD_PROFILE_DESKTOP)', '#if 0'),
        ('the-desktop-means-no-tools', '#define MAGICPORTALS_DEV_TOOLS 1', '#define MAGICPORTALS_DEV_TOOLS 0'),
    ]


if __name__ == '__main__':
    sys.exit(pc.main_of(pin, mutations_of, sys.argv[1:], 'test_mp_build_profile_header', os.path.join('game', 'BuildProfile.hpp'), __doc__))
