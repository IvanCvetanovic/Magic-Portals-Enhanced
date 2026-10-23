// What the start checks before it touches the engine, and what a build without the developer tools says (planning step 201, PORT-3):
// sim/StartChecks.hpp, and the two build facts of game/BuildProfile.hpp.
//
// WHAT IS PINNED. The Vulkan-loader question is asked of an INJECTED loader, so that the answer "no" is reachable without taking vulkan-1.dll off the
// machine the suite runs on (nothing can: it lives in System32): that it is asked exactly once, for exactly that library, that a "yes" costs nothing and
// says nothing, that a "no" -- and a missing loader function -- says what is missing and what to do, and names the Vulkan version the run asks of the
// GPU, clamped as the engine clamps it. The list of the developer's flags a build without the developer tools refuses: every one of the sixteen is
// refused with a line that names it and the option, nothing a player needs is on the list, an exact match only. The two build facts: a suite is built
// as a developer's desktop build is. Each pin says in "mutation:" the change of the code that turns it red; the Step records that they were run.
//
// NOT TESTED, and why: main.cpp cannot be linked into a suite, so that the wiring (the real LoadLibraryExW answer, the throw into the one fatal road,
// the refusal at the flag loop) rests on probes of the built programs. In particular NO RUN SHOWED THE MESSAGE BOX: vulkan-1.dll cannot be made to
// disappear from this machine, so the box is covered by the question and the words pinned here and by reading the code, and by the Step's one
// observed fact, that a program whose loader is present starts. Free of the engine and of the renderer: no window, no file.

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "BuildProfile.hpp"
#include "sim/StartChecks.hpp"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

using namespace MagicPortals;

namespace {

bool Has(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

void TheLoaderThatIsThereCostsNothingAndSaysNothing() {
    std::vector<std::string> asked;
    const StartChecks::Verdict verdict = StartChecks::CheckVulkanLoader(
        [&](const std::string& library) {
            asked.push_back(library);
            return true;
        },
        2);
    // mutation: return not-ok whatever the loader says; return the words with ok set.
    CHECK_MSG(verdict.ok, "a loader that loads is a start that goes on");
    CHECK_MSG(verdict.what.empty(), "and has nothing to say");
    // mutation: ask for another name (the import library's, "vulkan-1", "vulkan.dll"), or ask twice, or not at all.
    CHECK_EQ(asked.size(), std::size_t{1});
    CHECK_MSG(asked.size() == 1 && asked[0] == "vulkan-1.dll", "it is asked once, for the Vulkan loader, by the name Windows loads it under");
    CHECK_MSG(std::string(StartChecks::kVulkanLoaderName) == "vulkan-1.dll", "which is the name the Windows release delay-loads (game/windows/MagicPortalsWindows.cmake)");
}

void TheLoaderThatIsMissingSaysWhatIsMissingAndWhatToDo() {
    const StartChecks::Verdict verdict = StartChecks::CheckVulkanLoader([](const std::string&) { return false; }, 2);
    // mutation: return ok when the loader fails (the Step's mutation of the check: "a failing loader returns success").
    CHECK_MSG(!verdict.ok, "a loader that will not load is a start that stops");
    CHECK_MSG(!verdict.what.empty(), "and says why");
    // What is missing: the driver's Vulkan runtime, by the name of the file Windows could not load.
    CHECK_MSG(Has(verdict.what, "vulkan-1.dll"), "it names the file Windows could not load: " + verdict.what);
    CHECK_MSG(Has(verdict.what, "Vulkan") && Has(verdict.what, "graphics"), "and what it is, in words a player can use");
    // What to do: update or install the graphics driver, from whom, and start again.
    CHECK_MSG(Has(verdict.what, "Install or update the graphics driver"), "it says what to do");
    CHECK_MSG(Has(verdict.what, "NVIDIA") && Has(verdict.what, "AMD") && Has(verdict.what, "Intel"), "and from whom");
    CHECK_MSG(Has(verdict.what, "start the game again"), "and what to do after");
    // The version the run asks of a GPU, so that a person with a 1.1 driver does not read "1.2" as their trouble.
    // mutation: hard-code 1.2 in the words; drop the clamp.
    CHECK_MSG(Has(verdict.what, "Vulkan 1.2 or newer"), "a run that asks for 1.2 says 1.2");
    const StartChecks::Verdict phone = StartChecks::CheckVulkanLoader([](const std::string&) { return false; }, 1);
    CHECK_MSG(Has(phone.what, "Vulkan 1.1 or newer") && !Has(phone.what, "Vulkan 1.2"), "a run that asks for 1.1 says 1.1 and not 1.2");
    // Clamped into the engine's 1 to 2 (GameRuntime::ResolveVulkanMinor), the numbers the engine would really use.
    const StartChecks::Verdict zero = StartChecks::CheckVulkanLoader([](const std::string&) { return false; }, 0);
    const StartChecks::Verdict three = StartChecks::CheckVulkanLoader([](const std::string&) { return false; }, 3);
    CHECK_MSG(Has(zero.what, "Vulkan 1.1 or newer"), "a minimum of 0 is the engine's floor, 1.1");
    CHECK_MSG(Has(three.what, "Vulkan 1.2 or newer"), "and a minimum of 3 its ceiling, 1.2");
    // No path, no account name and no machine in the words: they go into a log a player sends and a box a player reads.
    CHECK_MSG(!Has(verdict.what, "\\") && !Has(verdict.what, "/") && !Has(verdict.what, ":\\"), "the words carry no path");
}

void TheLoaderIsAskedByNameAndOnlyThatNameDecides() {
    // The only library that matters is the one named: a machine that has every other DLL and not this one is still a machine without a loader.
    // mutation: ask for a different library; treat any "yes" as one.
    const StartChecks::Verdict others = StartChecks::CheckVulkanLoader(
        [](const std::string& library) { return library != "vulkan-1.dll"; }, 2);
    CHECK_MSG(!others.ok, "every library but the Vulkan loader loading is not a Vulkan loader");
    const StartChecks::Verdict only = StartChecks::CheckVulkanLoader(
        [](const std::string& library) { return library == "vulkan-1.dll"; }, 2);
    CHECK_MSG(only.ok, "and the Vulkan loader alone loading is enough");
    // No loader function at all is a question that cannot be answered, and an unanswered question is not a yes.
    // mutation: treat an empty function as ok.
    const StartChecks::Verdict none = StartChecks::CheckVulkanLoader(StartChecks::CanLoad{}, 2);
    CHECK_MSG(!none.ok && !none.what.empty(), "an absent loader function is a stop, with its words");
}

void TheDevelopersFlagsAreTheSixteenAndOnlyThose() {
    // mutation: drop one from the list, add a player's flag to it, make the match a prefix or case-blind.
    const std::vector<std::string> wanted = {"--route",         "--route-trace", "--tap",           "--hold",         "--drag",        "--press",
                                             "--place",         "--burn-dragon", "--visit-levels",  "--visit-passes", "--light-masks-off",
                                             "--shadow-maps",   "--sprites-only", "--asset-watching", "--sprite-report", "--perf-log"};
    CHECK_EQ(StartChecks::kDeveloperFlags.size(), wanted.size());
    std::set<std::string> seen;
    for (const std::string_view flag : StartChecks::kDeveloperFlags) seen.insert(std::string(flag));
    CHECK_MSG(seen.size() == StartChecks::kDeveloperFlags.size(), "no flag is listed twice");
    for (const std::string& flag : wanted) {
        CHECK_MSG(seen.count(flag) == 1, flag + " is on the list");
        CHECK_MSG(StartChecks::IsDeveloperFlag(flag), flag + " is a developer's flag");
        const std::string line = StartChecks::DeveloperFlagRefusal(flag);
        CHECK_MSG(Has(line, flag), flag + ": the refusal names the flag: " + line);
        CHECK_MSG(Has(line, "MAGICPORTALS_DEV_TOOLS=OFF"), flag + ": and the option that took it out");
        CHECK_MSG(Has(line, "--help"), flag + ": and where the flags that are there are listed");
    }
}

void NothingAPlayerNeedsIsOnTheList() {
    // The flags a player's build keeps (game/main.cpp): the game's own, the release's, and the engine's.
    // mutation: put any of them on the list.
    const char* const kept[] = {"--level",      "--root",     "--no-baked-paths", "--levels",   "--art",      "--data",   "--original", "--chapters",
                                "--achievements", "--port-data", "--padart",      "--saves",    "--controls", "--profile", "--help",     "-h",
                                "--window",     "--fixed-step", "--frames",       "--screenshot", "--screenshot-ui", "--screenshot-every",
                                "--hidden",     "--fullscreen", "--windowed",     "--scene",    "--record",   "--replay", "--import-assets"};
    for (const char* flag : kept) {
        CHECK_MSG(!StartChecks::IsDeveloperFlag(flag), std::string(flag) + " is a player's flag, kept in a build without the developer tools");
    }
    // An exact match only: a near miss is the engine's parser's to refuse as unknown, not ours to call a developer's flag.
    for (const char* near : {"", "route", "--routes", "--rout", "--Route", "--ROUTE", " --route", "--route ", "--route=1", "-route", "--tap2", "--perf", "--perf-logs"}) {
        CHECK_MSG(!StartChecks::IsDeveloperFlag(near), std::string("'") + near + "' is not the flag --route or any other, spelled exactly");
    }
}

void AnySuiteIsBuiltAsTheDevelopersDesktopBuildIs() {
    // game/BuildProfile.hpp says what a translation unit that was given neither fact is: the build the suites are. main.cpp is given both, by CMake.
    // mutation: default MAGICPORTALS_MOBILE to 1 or MAGICPORTALS_DEV_TOOLS to 0 in the header.
    CHECK_MSG(!kMobileBuild, "a suite is not a phone's build");
    CHECK_MSG(kDevTools, "and has the developer tools: test_mp_routes needs them");
}

void runTests() {
    TheLoaderThatIsThereCostsNothingAndSaysNothing();
    TheLoaderThatIsMissingSaysWhatIsMissingAndWhatToDo();
    TheLoaderIsAskedByNameAndOnlyThatNameDecides();
    TheDevelopersFlagsAreTheSixteenAndOnlyThose();
    NothingAPlayerNeedsIsOnTheList();
    AnySuiteIsBuiltAsTheDevelopersDesktopBuildIs();
}

} // namespace

MP_TEST_MAIN("test_mp_platform", 100)
