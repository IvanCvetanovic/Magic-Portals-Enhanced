#pragma once

// How a test_mp_* suite enters the one executable that holds them all, test_mp
// (test_mp.cpp runs them; tests/CMakeLists.txt says why there is one executable).
//
// Each suite was its own executable, and its main was the suite. It still is,
// under a name: the macros below turn that main into a function with internal
// linkage and register it under the suite's ctest name before main runs, so the
// suite's body, its checks and its summary line are exactly what they were.
//
//   MP_TEST_MAIN(suite, minChecks)         for TestHarness's TEST_MAIN, which
//                                          defines main itself
//   MP_SUITE_MAIN(suite) { ... }           for a suite's own int main()
//   MP_SUITE_MAIN_ARGS(suite, argc, argv)  for int main(int argc, char** argv)
//
// A suite that is compiled in and still defines main stops the link (two mains),
// and one registered under a name ctest does not use fails its ctest entry
// (test_mp refuses a name it does not hold): neither mistake passes quietly.

#include "TestHarness.hpp"

#include <vector>

namespace MpSuite {

// The suite's own main, as its executable called it: argv[0] and then the
// arguments after it. What it returns is the process's exit code - 77 to skip.
using Entry = int (*)(int argc, char** argv);

struct Suite {
    const char* name;
    Entry entry;
};

// Filled before main by every suite's registration. Function-local, so it exists
// before the first registration whatever order the suites' objects initialise in.
inline std::vector<Suite>& Suites() {
    static std::vector<Suite> suites;
    return suites;
}

struct Registration {
    Registration(const char* name, Entry entry) { Suites().push_back({name, entry}); }
};

} // namespace MpSuite

// Unnamed parameters where the suite takes none, so /W4 has nothing to say.
#define MP_TEST_MAIN(suite, minChecks)                                                    \
    static int mpSuiteEntry(int, char**) {                                                \
        runTests();                                                                       \
        return ::test::summary(suite, minChecks);                                         \
    }                                                                                     \
    static const ::MpSuite::Registration mpSuiteRegistration(suite, &mpSuiteEntry);

#define MP_SUITE_MAIN(suite)                                                              \
    static int mpSuiteMain();                                                             \
    static int mpSuiteEntry(int, char**) { return mpSuiteMain(); }                        \
    static const ::MpSuite::Registration mpSuiteRegistration(suite, &mpSuiteEntry);       \
    static int mpSuiteMain()

#define MP_SUITE_MAIN_ARGS(suite, argc, argv)                                             \
    static int mpSuiteMain(int argc, char** argv);                                        \
    static const ::MpSuite::Registration mpSuiteRegistration(suite, &mpSuiteMain);        \
    static int mpSuiteMain(int argc, char** argv)
