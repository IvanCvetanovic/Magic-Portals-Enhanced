// test_mp: every test_mp_* suite of the port, in one executable.
//
//   test_mp                    every suite, one after the other, in this process
//   test_mp <suite> [args...]  one suite, as its own executable ran it: the same
//                              output, the same exit code (77 when it skips), and
//                              the arguments after the name are the suite's own
//                              (test_mp test_mp_routes --only level8 --trace 10).
//                              The name may drop its test_mp_ prefix.
//   test_mp --list             the suites' names
//
// ctest runs one suite per entry, `test_mp <suite>`, each in its own process, so a
// suite's ctest result is what its own executable's was. Why one executable, and
// not one per suite: tests/CMakeLists.txt. How a suite registers: MpSuite.hpp.

#include "MpSuite.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <exception>
#include <string>
#include <vector>

namespace {

// The suites' own code for "skipped": their SKIPS_WITH_77, ctest's SKIP_RETURN_CODE.
constexpr int kSkipped = 77;
constexpr int kBadArguments = 2;
constexpr const char* kPrefix = "test_mp_";

int Run(const MpSuite::Suite& suite, int argc, char** argv) {
    // The harness's two counters are one pair for the whole process, and each
    // suite's summary line reports them: a suite starts from zero, as it did in
    // an executable of its own.
    ::test::g_checks = 0;
    ::test::g_failures = 0;
    int result = 1;
    try {
        result = suite.entry(argc, argv);
    } catch (const std::exception& e) {
        std::printf("  FAIL %s threw: %s\n", suite.name, e.what());
    } catch (...) {
        std::printf("  FAIL %s threw\n", suite.name);
    }
    std::fflush(stdout);
    return result;
}

} // namespace

int main(int argc, char** argv) {
    std::vector<MpSuite::Suite> suites = MpSuite::Suites();
    std::sort(suites.begin(), suites.end(),
              [](const MpSuite::Suite& a, const MpSuite::Suite& b) { return std::strcmp(a.name, b.name) < 0; });
    for (std::size_t i = 1; i < suites.size(); ++i) {
        if (std::strcmp(suites[i - 1].name, suites[i].name) == 0) {
            std::printf("test_mp: two suites are registered as %s\n", suites[i].name);
            return 1;
        }
    }

    if (argc >= 2 && std::strcmp(argv[1], "--list") == 0) {
        for (const MpSuite::Suite& suite : suites) std::printf("%s\n", suite.name);
        return 0;
    }

    if (argc >= 2) {
        std::string name = argv[1];
        if (name.rfind(kPrefix, 0) != 0) name = kPrefix + name;
        const auto found = std::find_if(suites.begin(), suites.end(), [&](const MpSuite::Suite& suite) {
            return name == suite.name;
        });
        if (found == suites.end()) {
            std::printf("test_mp: no suite %s (test_mp --list names the %zu it holds)\n", name.c_str(),
                        suites.size());
            return kBadArguments;
        }
        // The suite's argv is its name and then its own arguments.
        return Run(*found, argc - 1, argv + 1);
    }

    int passed = 0;
    int skipped = 0;
    std::vector<std::string> failed;
    for (const MpSuite::Suite& suite : suites) {
        std::printf("--- %s\n", suite.name);
        std::fflush(stdout);
        std::string name = suite.name;
        char* suiteArgv[] = {name.data(), nullptr};
        const int result = Run(suite, 1, suiteArgv);
        if (result == 0) {
            ++passed;
        } else if (result == kSkipped) {
            ++skipped;
        } else {
            failed.push_back(suite.name + std::string(" (exit ") + std::to_string(result) + ")");
        }
    }
    std::printf("test_mp: %zu suites, %d passed, %d skipped, %zu failed\n", suites.size(), passed, skipped,
                failed.size());
    for (const std::string& suite : failed) std::printf("  FAILED %s\n", suite.c_str());
    return failed.empty() ? 0 : 1;
}
