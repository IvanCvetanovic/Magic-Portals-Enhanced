"""What the two build pins share (planning step 208, PORT-10; ctest names test_mp_build_dev_tools_option and test_mp_build_profile_header).

A PIN here is a test that a bad build setting is REFUSED by the build itself, and for the right reason:

  * it runs the real tool (cmake, the C++ compiler) on a scratch copy of the real file, never on the tracked tree and never on build/;
  * it expects the exact message the file itself carries, READ OUT OF THE SOURCE and not retyped, at the line the file puts it (CMake prints
    "CMakeLists.txt:<line>", the compiler "BuildProfile.hpp:<line>"), with the whitespace collapsed (CMake re-wraps its messages; compilers indent theirs);
  * it has POSITIVE CONTROLS: the same command with a good value succeeds, or gets past that line, so a test that fails because there is no compiler, no
    engine checkout or a typo cannot pass as a refusal;
  * it exits 77 (ctest SKIPPED, never a pass) when the environment cannot run it: Windows (any CMake configure builds a compiler-identification program and a
    Windows link is a Smart App Control exposure; these pins are run in the Linux tree), no cmake, no compiler, an MSVC compiler;
  * it has NAMED MUTATIONS: a copy of the file with the guard weakened in one named way must make the pin go RED. The mutations are made on temporary copies and
    the real file's sha256 is printed before and after, so "restored byte for byte" is a fact and not a promise.

Standard library only. Exit codes: 0 pass, 1 fail, 77 skipped.
"""
import argparse
import hashlib
import os
import shutil
import tempfile

EXIT_PASS, EXIT_FAIL, EXIT_SKIPPED = 0, 1, 77


class Skip(Exception):
    """The environment cannot run this pin: the script exits 77 and says why."""


class Fail(Exception):
    pass


def sha256_of(path):
    with open(path, 'rb') as f:
        return hashlib.sha256(f.read()).hexdigest()


def read_text(path):
    with open(path, encoding='utf-8', newline='') as f:
        return f.read().replace('\r\n', '\n')


def normalise(text):
    """Every run of whitespace is one space: CMake re-wraps and indents its messages, compilers indent theirs."""
    return ' '.join(text.split())


def apply_mutation(text, old, new):
    """`old` must occur exactly once: a mutation that matches nothing, or twice, would test nothing and is an error of the pin and not a green run."""
    if text.count(old) != 1:
        raise Fail('the mutation matches %d place(s) of the file (needs exactly 1): %r' % (text.count(old), old[:70]))
    return text.replace(old, new)


def run_mutations(real_path, mutations, run_pin, label):
    """Runs `run_pin(path of a mutated copy)` once per named mutation; each MUST raise Fail (the pin goes red). Prints a table and returns the number that stayed green.
    The real file is never opened for writing; its sha256 is printed before and after."""
    before = sha256_of(real_path)
    original = read_text(real_path)
    survivors = 0
    scratch = tempfile.mkdtemp(prefix='mp-pin-mutation-')
    print('%s: %d named mutations of %s (sha256 %s)' % (label, len(mutations), os.path.basename(real_path), before[:16]))
    try:
        for name, old, new in mutations:
            folder = os.path.join(scratch, name)
            os.makedirs(folder)
            path = os.path.join(folder, os.path.basename(real_path))
            with open(path, 'w', encoding='utf-8', newline='\n') as f:
                f.write(apply_mutation(original, old, new))
            try:
                run_pin(path)
            except Fail as e:
                print('  %-30s RED   (%s)' % (name, str(e).split('\n')[0][:110]))
            else:
                survivors += 1
                print('  %-30s GREEN -- the pin did not notice this mutation' % name)
    finally:
        shutil.rmtree(scratch, ignore_errors=True)
    after = sha256_of(real_path)
    print('%s: the real file is %s (sha256 %s before and after)' % (label, 'unchanged' if before == after else 'CHANGED', after[:16]))
    if before != after:
        raise Fail('the real file changed during the mutations')
    return survivors


def main_of(pin, mutations_of, argv, label, default_path, doc):
    """The one command line of both pins. `pin(path, args)` runs the pin on a file (raising Fail or Skip); `mutations_of(text)` gives the named mutations."""
    parser = argparse.ArgumentParser(description=doc, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--repo', required=True, help='this repository')
    parser.add_argument('--cmake', default=None, help='the cmake executable (ctest passes ${CMAKE_COMMAND})')
    parser.add_argument('--c-compiler', default=None)
    parser.add_argument('--cxx-compiler', default=None)
    parser.add_argument('--cxx-id', default='', help='CMAKE_CXX_COMPILER_ID of the tree that registers the test')
    parser.add_argument('--file', default=None, help="run the pin on this file instead of the repository's")
    parser.add_argument('--no-mutations', action='store_true', help='run the pin on the real file only')
    args = parser.parse_args(argv)
    try:
        if os.name == 'nt':
            raise Skip('these pins are run in the Linux tree: a CMake configure on Windows builds and links a compiler-identification program, and no Windows link is made for a pin')
        real = args.file or os.path.join(args.repo, default_path)
        pin(real, args)
        print('%s: the pin passes on %s' % (label, real))
        if not args.no_mutations and not args.file:
            survivors = run_mutations(real, mutations_of(read_text(real)), lambda path: pin(path, args), label)
            if survivors:
                raise Fail('%d mutation(s) were not noticed' % survivors)
        print('%s: PASS' % label)
        return EXIT_PASS
    except Skip as e:
        print('%s: SKIPPED (exit 77, never a pass): %s' % (label, e))
        return EXIT_SKIPPED
    except Fail as e:
        print('%s: FAIL: %s' % (label, e))
        return EXIT_FAIL


def which(program):
    if program and os.path.isabs(program):
        return program if os.access(program, os.X_OK) else None
    return shutil.which(program) if program else None
