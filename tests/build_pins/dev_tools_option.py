#!/usr/bin/env python3
"""PIN C1 (ctest test_mp_build_dev_tools_option): a configure with MAGICPORTALS_DEV_TOOLS set to anything but AUTO, ON or OFF (in any case) FAILS, with the message
the top-level CMakeLists.txt carries; AUTO, ON and OFF do not fail at that line.

WHY. The option exists to take the developer's flags out of a player's program (planning step 202, PORT3-1). It once failed OPEN: CMake read any word that is not a
false constant as ON, so a mistyped release configure silently shipped the tools. The fix is a FATAL_ERROR in CMakeLists.txt, and nothing in the build kept it there.

HOW. The real top-level CMakeLists.txt is copied alone into a scratch folder (no engine/, no game/) and configured with cmake, a C and a C++ compiler, and a try-compile
that builds a static library and not a program (CMAKE_TRY_COMPILE_TARGET_TYPE). The file runs from its first line to the check that engine/ exists, and stops there:
  * a bad value stops at the FATAL_ERROR of the option, at the line the file puts it, with that message (read from the file, the value put in);
  * a good value gets PAST that line, and stops at the next FATAL_ERROR, "engine/ is empty", also read from the file: the positive control. A configure that failed
    for want of a compiler, or on a typo, shows neither message and is reported as SKIPPED or FAILED, never as a pass.
The named mutations weaken the value list of a temporary copy (accept anything, refuse OFF, accept a typo, make the words case-sensitive, make the error a
SEND_ERROR, make the message stop naming the value); each must turn the pin red. The real file is never written.
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pin_common as pc  # noqa: E402

OPTION = 'MAGICPORTALS_DEV_TOOLS'
# What the option refuses and what it accepts: the contract of docs/packaging.md section 6 ("exactly these three words, in any case").
# ('AUTO ' with a trailing space is not here: CMake's own -D handling trims it before the file sees it, measured with cmake 3.28.3, so it is the word AUTO by then.)
BAD_VALUES = ('OF', 'BANANA', '', '0', 'NO', 'FALSE', '1', 'YES', 'TRUE', 'ONN', 'OFF!', 'AUTO,ON')
GOOD_VALUES = ('AUTO', 'ON', 'OFF', 'auto', 'On', 'off')
ENVIRONMENT_FAILURES = ('No CMAKE_C_COMPILER could be found', 'No CMAKE_CXX_COMPILER could be found', 'CMake was unable to find a build program',
                        'is not able to compile a simple test program', 'Could not find compiler', 'The CMAKE_C_COMPILER', 'The CMAKE_CXX_COMPILER')
SEVERITIES = r'(FATAL_ERROR|SEND_ERROR|AUTHOR_WARNING|WARNING|DEPRECATION|NOTICE|STATUS)'


def cmake_message_calls(text):
    """[(line, severity, quoted arguments of the call, unescaped)] for every message(<severity> ...) call of a CMake file."""
    calls = []
    for m in re.finditer(r'message\(\s*' + SEVERITIES + r'\b', text):
        depth, i, quoted, current, escape, args = 1, m.end(), False, [], False, []
        while i < len(text) and depth:
            c = text[i]
            if quoted:
                if escape:
                    current.append({'n': '\n', 't': '\t'}.get(c, c))
                    escape = False
                elif c == '\\':
                    escape = True
                elif c == '"':
                    quoted = False
                    args.append(''.join(current))
                    current = []
                else:
                    current.append(c)
            elif c == '"':
                quoted = True
            elif c == '(':
                depth += 1
            elif c == ')':
                depth -= 1
            i += 1
        calls.append((text.count('\n', 0, m.start()) + 1, m.group(1), args))
    return calls


def option_message(text):
    """(line, template) of the FATAL_ERROR that follows the option's last accepted word, with ${MAGICPORTALS_DEV_TOOLS} left in the template."""
    try:
        anchor = text.rindex('elseif (_magicPortalsDevTools STREQUAL')
    except ValueError:
        raise pc.Fail("the file no longer tests the option's words (no elseif (_magicPortalsDevTools STREQUAL ...))")
    line_anchor = text.count('\n', 0, anchor) + 1
    later = [c for c in cmake_message_calls(text) if c[0] > line_anchor]
    if not later:
        raise pc.Fail("the file has no message() after the option's last accepted word")
    line, severity, args = later[0]
    if severity != 'FATAL_ERROR':
        raise pc.Fail("the message after the option's words is a %s and not a FATAL_ERROR: a bad value would not stop the configure (CMakeLists.txt:%d)" % (severity, line))
    return line, ''.join(args)


def engine_message(text):
    """(line, message) of the FATAL_ERROR that the existence check of engine/ raises: the next stop after the option."""
    try:
        anchor = text.index('engine/CMakeLists.txt")')
    except ValueError:
        raise pc.Fail('the file no longer checks that engine/ exists')
    line_anchor = text.count('\n', 0, anchor) + 1
    later = [c for c in cmake_message_calls(text) if c[0] >= line_anchor and c[1] == 'FATAL_ERROR']
    if not later:
        raise pc.Fail('the file has no FATAL_ERROR after its check that engine/ exists')
    line, _, args = later[0]
    return line, ''.join(args)


def configure(cmake, source, build, value, compilers):
    cmd = [cmake, '-S', source, '-B', build, '-D%s=%s' % (OPTION, value), '-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY', '-DCMAKE_BUILD_TYPE=Release']
    if compilers[0]:
        cmd.append('-DCMAKE_C_COMPILER=' + compilers[0])
    if compilers[1]:
        cmd.append('-DCMAKE_CXX_COMPILER=' + compilers[1])
    done = subprocess.run(cmd, capture_output=True, text=True, errors='replace', timeout=300)
    return done.returncode, done.stdout + done.stderr


def pin(path, args):
    cmake = pc.which(args.cmake or 'cmake')
    if not cmake:
        raise pc.Skip('no cmake to run (--cmake, or on PATH)')
    compilers = (pc.which(args.c_compiler) if args.c_compiler else (pc.which('gcc') or pc.which('cc') or pc.which('clang')),
                 pc.which(args.cxx_compiler) if args.cxx_compiler else (pc.which('g++') or pc.which('c++') or pc.which('clang++')))
    if not compilers[0] or not compilers[1]:
        raise pc.Skip("no C and C++ compiler to run CMake's project() with (%s, %s)" % compilers)
    if args.cxx_id and args.cxx_id not in ('GNU', 'Clang', 'AppleClang'):
        raise pc.Skip('the compiler of this tree is %s; the pin runs with GCC or Clang' % args.cxx_id)
    text = pc.read_text(path)
    opt_line, template = option_message(text)
    eng_line, eng_template = engine_message(text)
    scratch = tempfile.mkdtemp(prefix='mp-pin-devtools-')
    try:
        source, build = os.path.join(scratch, 'src'), os.path.join(scratch, 'build')
        os.makedirs(source)
        with open(os.path.join(source, 'CMakeLists.txt'), 'w', encoding='utf-8', newline='\n') as f:
            f.write(text)

        # 1. the environment can run it: a good value gets past the option and stops at the engine check (this is also the first positive control)
        code, out = configure(cmake, source, build, 'ON', compilers)
        flat = pc.normalise(out)
        engine_text = pc.normalise(eng_template)
        failing = [line for line in out.split('\n') if any(f in line for f in ENVIRONMENT_FAILURES)]
        if failing:
            raise pc.Skip('CMake cannot run project() here: ' + pc.normalise(failing[0]))
        if code == 0 or engine_text not in flat or ('CMakeLists.txt:%d' % eng_line) not in flat:
            raise pc.Fail('the control configure (%s=ON) did not stop at the engine check as the file says it should (exit %d):\n%s' % (OPTION, code, flat[:600]))

        # 2. the option names its value, so that a mistyped word is found in the message
        if '${%s}' % OPTION not in template:
            raise pc.Fail('the message of the option does not name the value it refused (no ${%s} in it)' % OPTION)

        # 3. every bad value stops AT the option, with the file's message, the value put in; and not at the engine check
        for value in BAD_VALUES:
            code, out = configure(cmake, source, build, value, compilers)
            flat = pc.normalise(out)
            expected = pc.normalise(template.replace('${%s}' % OPTION, value))
            if code == 0:
                raise pc.Fail('%s=%r did not fail the configure' % (OPTION, value))
            if expected not in flat:
                raise pc.Fail("%s=%r failed, but not with the option's message (looked for %r in the output):\n%s" % (OPTION, value, expected[:80], flat[:600]))
            if ('CMakeLists.txt:%d' % opt_line) not in flat:
                raise pc.Fail('%s=%r: the message is not at the line the file puts it (CMakeLists.txt:%d)' % (OPTION, value, opt_line))
            if engine_text in flat:
                raise pc.Fail('%s=%r went on past the option to the engine check: the configure did not stop at the option' % (OPTION, value))

        # 4. every good value gets past the option (positive controls): the next stop is the engine check, and the option's message is not in the output
        for value in GOOD_VALUES:
            code, out = configure(cmake, source, build, value, compilers)
            flat = pc.normalise(out)
            if pc.normalise(template.split('${')[0]) in flat:
                raise pc.Fail("%s=%r was refused by the option's message" % (OPTION, value))
            if engine_text not in flat or ('CMakeLists.txt:%d' % eng_line) not in flat:
                raise pc.Fail('%s=%r did not get past the option to the engine check:\n%s' % (OPTION, value, flat[:600]))
        print('  %d bad values stop at CMakeLists.txt:%d with the file\'s message; %d good values get past it to CMakeLists.txt:%d ("%s...")'
              % (len(BAD_VALUES), opt_line, len(GOOD_VALUES), eng_line, engine_text[:40]))
    finally:
        shutil.rmtree(scratch, ignore_errors=True)


def mutations_of(text):
    """(name, old, new): the guard weakened in one named way each. The words are those of the real file; a change of the file that makes one stop matching is an error."""
    return [
        ('accept-anything', 'else()\n    message(FATAL_ERROR "MAGICPORTALS_DEV_TOOLS is', 'elseif (FALSE)\n    message(FATAL_ERROR "MAGICPORTALS_DEV_TOOLS is'),
        ('refuse-OFF', 'elseif (_magicPortalsDevTools STREQUAL "OFF")', 'elseif (_magicPortalsDevTools STREQUAL "OFFX")'),
        ('accept-the-typo-OF', 'elseif (_magicPortalsDevTools STREQUAL "ON")', 'elseif (_magicPortalsDevTools STREQUAL "ON" OR _magicPortalsDevTools STREQUAL "OF")'),
        ('case-sensitive', 'string(TOUPPER "${MAGICPORTALS_DEV_TOOLS}" _magicPortalsDevTools)', 'set(_magicPortalsDevTools "${MAGICPORTALS_DEV_TOOLS}")'),
        ('send-error-not-fatal', 'else()\n    message(FATAL_ERROR "MAGICPORTALS_DEV_TOOLS is', 'else()\n    message(SEND_ERROR "MAGICPORTALS_DEV_TOOLS is'),
        ('message-forgets-the-value', 'is \\"${MAGICPORTALS_DEV_TOOLS}\\", which', 'is not one of its three values, which'),
    ]


if __name__ == '__main__':
    sys.exit(pc.main_of(pin, mutations_of, sys.argv[1:], 'test_mp_build_dev_tools_option', 'CMakeLists.txt', __doc__))
