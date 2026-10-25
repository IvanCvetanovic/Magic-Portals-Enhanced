// What the start checks before it touches the engine, and what a build without the developer tools says (planning step 201, PORT-3; the error
// codes, the help text and the build facts' header made safe, planning step 202, PORT-4): sim/StartChecks.hpp, and the two build facts of
// game/BuildProfile.hpp.
//
// WHAT IS PINNED. The Vulkan-loader question is asked of an INJECTED loader, so that the answer "no" is reachable without taking vulkan-1.dll off the
// machine the suite runs on (nothing can: it lives in System32): that it is asked exactly once, for exactly that library, that a "yes" costs nothing and
// says nothing WHATEVER STALE CODE comes with it, that a "no" -- and a missing loader function -- says what is missing and what to do, and names the
// Vulkan version the run asks of the GPU, clamped as the engine clamps it (by the engine tuning's own constants). A "no" is told three ways by the
// operating system's code: 126 (not found), 193 (found and not a valid image) and any other (shown as it is), and the three are told differently.
// The list of the developer's flags a build without the developer tools refuses: every one of the nineteen is refused with a line that names it and the
// option, nothing a player needs is on the list, an exact match only; and the --controls help entry of such a build names none of them while the
// default build's entry is, byte for byte, what it was. The build facts: a suite is built as a developer's desktop build is, by its own declaration.
// Each pin says in "mutation:" the change of the code that turns it red; the Step records that they were run.
//
// NOT TESTED, and why: main.cpp cannot be linked into a suite, so that the wiring (the real LoadLibraryExW answer, the throw into the one fatal road,
// the refusal at the flag loop) rests on probes of the built programs. In particular NO RUN SHOWED THE MESSAGE BOX: vulkan-1.dll cannot be made to
// disappear from this machine, so the box is covered by the question and the words pinned here and by reading the code, and by the Step's one
// observed fact, that a program whose loader is present starts. Free of the engine and of the renderer: no window, no file.

#include "TestHarness.hpp"
#include "MpSuite.hpp"

// A suite is not part of the program, so it is given none of the build's facts (game/CMakeLists.txt gives them to the MagicPortals target alone) and
// says outright what it is: the developer's desktop build every suite is. Without this line BuildProfile.hpp stops the compile (step 202).
#define MAGICPORTALS_BUILD_PROFILE_DESKTOP
#include "BuildProfile.hpp"
#include "sim/EngineTuning.hpp"
#include "sim/StartChecks.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

using namespace MagicPortals;

namespace {

bool Has(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

// The loader's answers, spelled once: loaded (with whatever code the system left lying there), or not loaded and why.
StartChecks::LoadResult Loaded(std::uint32_t staleCode = 0) { return StartChecks::LoadResult{true, staleCode}; }
StartChecks::LoadResult NotLoaded(std::uint32_t code) { return StartChecks::LoadResult{false, code}; }

// The check, asked of a loader that fails with `code`.
StartChecks::Verdict Fails(std::uint32_t code, std::uint32_t minor = 2) {
    return StartChecks::CheckVulkanLoader([code](const std::string&) { return NotLoaded(code); }, minor);
}

// `text` with every occurrence of `part` replaced by `with`: to compare two messages that differ by one number alone.
std::string Replaced(std::string text, const std::string& part, const std::string& with) {
    for (std::size_t at = text.find(part); at != std::string::npos; at = text.find(part, at + with.size())) text.replace(at, part.size(), with);
    return text;
}

void TheLoaderThatIsThereCostsNothingAndSaysNothing() {
    std::vector<std::string> asked;
    const StartChecks::Verdict verdict = StartChecks::CheckVulkanLoader(
        [&](const std::string& library) {
            asked.push_back(library);
            return Loaded();
        },
        2);
    // mutation: return not-ok whatever the loader says; return the words with ok set.
    CHECK_MSG(verdict.ok, "a loader that loads is a start that goes on");
    CHECK_MSG(verdict.what.empty(), "and has nothing to say");
    // mutation: ask for another name (the import library's, "vulkan-1", "vulkan.dll"), or ask twice, or not at all.
    CHECK_EQ(asked.size(), std::size_t{1});
    CHECK_MSG(asked.size() == 1 && asked[0] == "vulkan-1.dll", "it is asked once, for the Vulkan loader, by the name Windows loads it under");
    // The claim is only the value. That the Windows release delays THIS name is the build's doing, not this check's: game/windows/MagicPortalsWindows.cmake
    // reads the constant's line out of sim/StartChecks.hpp at configure time and hands what it finds to the linker as /DELAYLOAD (step 202).
    CHECK_MSG(std::string(StartChecks::kVulkanLoaderName) == "vulkan-1.dll", "the constant is the name Windows loads the Vulkan loader under");
}

void ALoadThatSucceedsIsASuccessWhateverCodeIsLeftOver() {
    // A successful load leaves GetLastError at whatever it was before (an earlier failure, a probe of a file that was not there): only `loaded`
    // decides. A start that works must never be stopped by a number that means nothing.
    // mutation: decide on `error` instead of `loaded` (ok only when the code is 0); ok only when loaded and the code is not 126 or 193.
    for (const std::uint32_t stale : {0u, 2u, 5u, StartChecks::kLoadErrorModuleNotFound, StartChecks::kLoadErrorBadImage, 1114u, 0xFFFFFFFFu}) {
        const StartChecks::Verdict verdict = StartChecks::CheckVulkanLoader([stale](const std::string&) { return Loaded(stale); }, 2);
        CHECK_MSG(verdict.ok && verdict.what.empty(), "a loader that loads is a start that goes on, with the stale code " + std::to_string(stale));
    }
}

void TheLoaderThatIsMissingSaysWhatIsMissingAndWhatToDo() {
    const StartChecks::Verdict verdict = Fails(StartChecks::kLoadErrorModuleNotFound);
    // mutation: return ok when the loader fails (the Step's mutation of the check: "a failing loader returns success").
    CHECK_MSG(!verdict.ok, "a loader that will not load is a start that stops");
    CHECK_MSG(!verdict.what.empty(), "and says why");
    // What is missing: the driver's Vulkan runtime, by the name of the file Windows could not load, and the code Windows gave.
    CHECK_MSG(Has(verdict.what, "vulkan-1.dll"), "it names the file Windows could not load: " + verdict.what);
    CHECK_MSG(Has(verdict.what, "Vulkan") && Has(verdict.what, "graphics"), "and what it is, in words a player can use");
    // mutation: say the driver is missing in the words of 193 or of any other code; drop the code; show another number.
    CHECK_MSG(Has(verdict.what, "No Vulkan graphics driver was found"), "126 is the driver not found");
    CHECK_MSG(Has(verdict.what, "(Windows error 126)"), "and the code is in the words, for the log a tester sends");
    // 126 is also a file the loader needs being gone, so the words do not claim that the loader file itself is absent.
    CHECK_MSG(Has(verdict.what, "or a file it needs was not found"), "it, or a file it needs: 126 does not say which");
    // What to do: update or install the graphics driver, from whom, and start again.
    CHECK_MSG(Has(verdict.what, "Install or update the graphics driver"), "it says what to do");
    CHECK_MSG(Has(verdict.what, "NVIDIA") && Has(verdict.what, "AMD") && Has(verdict.what, "Intel"), "and from whom");
    CHECK_MSG(Has(verdict.what, "start the game again"), "and what to do after");
    // The version the run asks of a GPU, so that a person with a 1.1 driver does not read "1.2" as their trouble.
    // mutation: hard-code 1.2 in the words; drop the clamp.
    CHECK_MSG(Has(verdict.what, "Vulkan 1.2 or newer"), "a run that asks for 1.2 says 1.2");
    const StartChecks::Verdict phone = Fails(StartChecks::kLoadErrorModuleNotFound, 1);
    CHECK_MSG(Has(phone.what, "Vulkan 1.1 or newer") && !Has(phone.what, "Vulkan 1.2"), "a run that asks for 1.1 says 1.1 and not 1.2");
    // Clamped into the engine's range (GameRuntime::ResolveVulkanMinor), the numbers the engine would really use.
    const StartChecks::Verdict zero = Fails(StartChecks::kLoadErrorModuleNotFound, 0);
    const StartChecks::Verdict three = Fails(StartChecks::kLoadErrorModuleNotFound, 3);
    CHECK_MSG(Has(zero.what, "Vulkan 1.1 or newer"), "a minimum of 0 is the engine's floor, 1.1");
    CHECK_MSG(Has(three.what, "Vulkan 1.2 or newer"), "and a minimum of 3 its ceiling, 1.2");
    // No path, no account name and no machine in the words: they go into a log a player sends and a box a player reads.
    CHECK_MSG(!Has(verdict.what, "\\") && !Has(verdict.what, "/") && !Has(verdict.what, ":\\"), "the words carry no path");
}

void TheCodeDecidesWhatThePlayerIsTold() {
    const StartChecks::Verdict notFound = Fails(StartChecks::kLoadErrorModuleNotFound);
    const StartChecks::Verdict badImage = Fails(StartChecks::kLoadErrorBadImage);
    const StartChecks::Verdict initFailed = Fails(1114); // ERROR_DLL_INIT_FAILED: the driver's own start failed
    const StartChecks::Verdict denied = Fails(5);        // ERROR_ACCESS_DENIED
    // The two named codes are what the operating system says they are (sim/ takes no windows.h, so the numbers are spelled once there).
    // mutation: change either constant (127 is ERROR_PROC_NOT_FOUND, 194 nothing).
    CHECK_EQ(StartChecks::kLoadErrorModuleNotFound, std::uint32_t{126});
    CHECK_EQ(StartChecks::kLoadErrorBadImage, std::uint32_t{193});
    // 193: the file IS there and cannot be used. What it is, why, what to do, and the stray copy a developer's folder may hold.
    // mutation: give 193 the words of 126 or of any other code; drop its code; drop the reinstall advice or the stray-copy sentence.
    CHECK_MSG(!badImage.ok && !badImage.what.empty(), "193 stops the start and says why");
    CHECK_MSG(Has(badImage.what, "(Windows error 193)"), "with its code: " + badImage.what);
    CHECK_MSG(Has(badImage.what, "Windows found vulkan-1.dll") && Has(badImage.what, "not a valid program file for this computer"),
              "the file was found and is not a usable image");
    CHECK_MSG(Has(badImage.what, "damaged") && Has(badImage.what, "32-bit file on 64-bit Windows"), "damaged, or for another kind of computer");
    CHECK_MSG(Has(badImage.what, "Reinstall the graphics driver") && Has(badImage.what, "NVIDIA") && Has(badImage.what, "start the game again"),
              "the cure is a reinstall of the driver, from its maker");
    CHECK_MSG(Has(badImage.what, "beside the game, take it away"), "and a stray copy of the file beside the game is to go");
    CHECK_MSG(!Has(badImage.what, "No Vulkan graphics driver was found") && !Has(badImage.what, "was not found"), "it does not say the driver was not found");
    // 126 and 193 are told differently, and neither in the words of the other.
    // mutation: treat every code alike (the 126 words for all, or the default words for all).
    CHECK_MSG(notFound.what != badImage.what, "126 and 193 are told differently");
    CHECK_MSG(!Has(notFound.what, "193") && !Has(notFound.what, "not a valid program file") && !Has(notFound.what, "damaged"),
              "126 does not speak of an image");
    // Any other code is shown as it is, so that the log says why; and the words say neither of the two named things.
    // mutation: show a fixed number; drop the number; let the default words speak of a file not found or an invalid image.
    CHECK_MSG(Has(initFailed.what, "(Windows error 1114)"), "1114 is shown as 1114: " + initFailed.what);
    CHECK_MSG(Has(denied.what, "(Windows error 5)"), "5 is shown as 5");
    CHECK_MSG(initFailed.what != denied.what, "two other codes are told with their own numbers");
    CHECK_MSG(Replaced(initFailed.what, "(Windows error 1114)", "(CODE)") == Replaced(denied.what, "(Windows error 5)", "(CODE)"),
              "and they are told in the same words but for the number");
    CHECK_MSG(initFailed.what != notFound.what && initFailed.what != badImage.what, "and not in the words of 126 or 193");
    CHECK_MSG(!Has(initFailed.what, "was not found") && !Has(initFailed.what, "not a valid program file") && !Has(initFailed.what, "No Vulkan graphics driver"),
              "they claim neither of the two named causes");
    CHECK_MSG(Has(initFailed.what, "Windows could not load vulkan-1.dll") && Has(initFailed.what, "Install or update the graphics driver"),
              "what failed, and what to try");
    // A "no" that gave no reason (code 0) says so, and a number is not made up for it.
    // mutation: show "(Windows error 0)"; use the 126 words for 0.
    const StartChecks::Verdict noReason = Fails(0);
    CHECK_MSG(!noReason.ok && Has(noReason.what, "(Windows gave no reason)") && !Has(noReason.what, "Windows error"), "code 0 is 'no reason', not an error 0");
    CHECK_MSG(noReason.what != notFound.what, "and is not told as 126");
    // Every one names the file, the version the run asks of a GPU, the makers, and carries no path.
    // mutation: drop the version from one of the branches.
    for (const StartChecks::Verdict* verdict : {&notFound, &badImage, &initFailed, &denied, &noReason}) {
        CHECK_MSG(Has(verdict->what, "vulkan-1.dll") && Has(verdict->what, "Vulkan 1.2 or newer"), "it names the loader and the version: " + verdict->what);
        CHECK_MSG(Has(verdict->what, "NVIDIA") && Has(verdict->what, "AMD") && Has(verdict->what, "Intel"), "and the makers");
        CHECK_MSG(!Has(verdict->what, "\\") && !Has(verdict->what, "/"), "and no path");
    }
}

void TheVersionIsClampedByTheEnginesOwnRange() {
    // The run asks of a GPU what the engine would use: GameRuntime::ResolveVulkanMinor's range, which EngineTuning keeps as kVulkanMinorMin and Max.
    // The words use those constants, not two numbers of their own.
    // mutation: clamp with other literals (1, 3) or (2, 2); clamp to the constants swapped; do not clamp.
    CHECK_EQ(EngineTuning::kVulkanMinorMin, std::uint32_t{1});
    CHECK_EQ(EngineTuning::kVulkanMinorMax, std::uint32_t{2});
    for (const std::uint32_t code : {StartChecks::kLoadErrorModuleNotFound, StartChecks::kLoadErrorBadImage, std::uint32_t{1114}}) {
        for (std::uint32_t asked = 0; asked <= EngineTuning::kVulkanMinorMax + 2; ++asked) {
            const std::uint32_t used = std::clamp(asked, EngineTuning::kVulkanMinorMin, EngineTuning::kVulkanMinorMax);
            const std::uint32_t other = used == EngineTuning::kVulkanMinorMin ? EngineTuning::kVulkanMinorMax : EngineTuning::kVulkanMinorMin;
            const StartChecks::Verdict verdict = Fails(code, asked);
            CHECK_MSG(Has(verdict.what, "Vulkan 1." + std::to_string(used) + " or newer"),
                      "code " + std::to_string(code) + ", asked " + std::to_string(asked) + ": says 1." + std::to_string(used));
            CHECK_MSG(!Has(verdict.what, "Vulkan 1." + std::to_string(other) + " or newer"), "and not the other");
        }
    }
}

void TheLoaderIsAskedByNameAndOnlyThatNameDecides() {
    // The only library that matters is the one named: a machine that has every other DLL and not this one is still a machine without a loader.
    // mutation: ask for a different library; treat any "yes" as one.
    const StartChecks::Verdict others = StartChecks::CheckVulkanLoader(
        [](const std::string& library) { return library != "vulkan-1.dll" ? Loaded() : NotLoaded(StartChecks::kLoadErrorModuleNotFound); }, 2);
    CHECK_MSG(!others.ok, "every library but the Vulkan loader loading is not a Vulkan loader");
    const StartChecks::Verdict only = StartChecks::CheckVulkanLoader(
        [](const std::string& library) { return library == "vulkan-1.dll" ? Loaded() : NotLoaded(StartChecks::kLoadErrorModuleNotFound); }, 2);
    CHECK_MSG(only.ok, "and the Vulkan loader alone loading is enough");
    // No loader function at all is a question that cannot be answered, and an unanswered question is not a yes: it is a "no" that gave no reason.
    // mutation: treat an empty function as ok.
    const StartChecks::Verdict none = StartChecks::CheckVulkanLoader(StartChecks::TryLoad{}, 2);
    CHECK_MSG(!none.ok && !none.what.empty(), "an absent loader function is a stop, with its words");
    CHECK_MSG(Has(none.what, "Windows gave no reason"), "and it gave no reason");
}

void TheDevelopersFlagsAreTheNineteenAndOnlyThose() {
    // mutation: drop one from the list, add a player's flag to it, make the match a prefix or case-blind.
    const std::vector<std::string> wanted = {"--route",         "--route-trace", "--tap",           "--hold",         "--drag",        "--press",
                                             "--place",         "--burn-dragon", "--visit-levels",  "--visit-passes", "--light-masks-off",
                                             "--shadow-maps",   "--sprites-only", "--asset-watching", "--sprite-report", "--perf-log",
                                             "--scene-samples", "--dynamic-res",  "--perf-overlay"};
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

void TheControlsHelpOfABuildWithoutTheToolsNamesNoFlagItRefuses() {
    // The tail of --help's --controls entry (game/main.cpp prints it). main.cpp cannot be linked into a suite, so the text is a function of the build's
    // kind, and this is where its two versions are held to what they must say. The grep of the built programs' whole --help for the nineteen flags is
    // the Step's own proof of the rest of the text.
    const std::string withTools = StartChecks::ControlsRunKindsHelp(true);
    const std::string withoutTools = StartChecks::ControlsRunKindsHelp(false);
    // The default build's text is, byte for byte, what main.cpp printed before this function: nine lines at the entry's column of 20.
    // mutation: change a word of the developer-tools text; reflow it; drop the final newline; return the other version.
    const std::array<const char*, 9> before = {
        "A player's run (none of the developer's flags",
        "below) has an Options button in the main menu's top-right corner,",
        "where the choice is made; it is kept in controls.json beside the",
        "other saves and read at the next start (shipped default: modern).",
        "With this flag the run STARTS in that style, ignores the saved",
        "choice and writes nothing: Options then chooses for the run only.",
        "`classic` is the original's controls with no Options button. A",
        "developer's run (--fixed-step, --route, --visit-levels) is classic",
        "and has no Options button, unless the flag says otherwise",
    };
    std::string golden;
    for (const char* line : before) golden += std::string(20, ' ') + line + '\n';
    CHECK_MSG(withTools == golden, "with the developer tools the text is what --help said before: " + withTools);
    CHECK_MSG(Has(withTools, "--route") && Has(withTools, "--visit-levels"), "and it names the two flags that build has");
    // Without the tools it names not one of the nineteen flags the build refuses (substring: no flag may appear even inside another word).
    // mutation: return the with-tools text for false; name any of the nineteen in the without-tools text.
    for (const std::string_view flag : StartChecks::kDeveloperFlags) {
        CHECK_MSG(!Has(withoutTools, std::string(flag)), "the text of a build without the tools does not name " + std::string(flag));
    }
    CHECK_MSG(withoutTools != withTools, "the two builds do not say the same");
    // And it is not left with the words that pointed at a list that is not there, and says what a developer's run still is.
    // mutation: keep "none of the developer's flags below".
    CHECK_MSG(!Has(withoutTools, "below") && !Has(withoutTools, "developer's flags"), "it points at no list of developer's flags");
    CHECK_MSG(Has(withoutTools, "--fixed-step"), "it names the one flag that makes a run classic there, the engine's --fixed-step");
    CHECK_MSG(Has(withoutTools, "Options button") && Has(withoutTools, "controls.json") && Has(withoutTools, "STARTS in that style"),
              "and still says what a player's run has");
    // Shape: nine lines, each at the column and ended by a newline, so that the entry that follows starts on its own line.
    for (const std::string& text : {withTools, withoutTools}) {
        CHECK_MSG(!text.empty() && text.back() == '\n', "ends with a newline");
        std::size_t lines = 0;
        std::size_t at = 0;
        while (at < text.size()) {
            const std::size_t end = text.find('\n', at);
            const std::string line = text.substr(at, end - at);
            CHECK_MSG(line.compare(0, 20, std::string(20, ' ')) == 0 && line.size() > 20 && line[20] != ' ', "a line is at the entry's column: '" + line + "'");
            CHECK_MSG(line.size() <= 90, "and fits a terminal: '" + line + "'");
            ++lines;
            // A text with no newline at its end has a last line that ends the walk: the loop must end on a broken text, not run away on it.
            if (end == std::string::npos) break;
            at = end + 1;
        }
        CHECK_EQ(lines, std::size_t{9});
    }
}

void AnySuiteIsBuiltAsTheDevelopersDesktopBuildIs() {
    // game/BuildProfile.hpp gives a translation unit that DECLARES itself the developer's desktop one (this file does, above its include) what that build is:
    // no phone, the developer's tools in. A unit given neither the facts nor the declaration does not compile at all (the header's #error), so there
    // is no third answer to pin; the Step shows that, with the compilers, on a second translation unit of a MOBILE configure.
    // mutation: make the declared desktop a phone (MAGICPORTALS_MOBILE 1) or take the tools out (MAGICPORTALS_DEV_TOOLS 0) in the header.
    CHECK_MSG(!kMobileBuild, "a suite is not a phone's build");
    CHECK_MSG(kDevTools, "and has the developer tools: test_mp_routes needs them");
}

void runTests() {
    TheLoaderThatIsThereCostsNothingAndSaysNothing();
    ALoadThatSucceedsIsASuccessWhateverCodeIsLeftOver();
    TheLoaderThatIsMissingSaysWhatIsMissingAndWhatToDo();
    TheCodeDecidesWhatThePlayerIsTold();
    TheVersionIsClampedByTheEnginesOwnRange();
    TheLoaderIsAskedByNameAndOnlyThatNameDecides();
    TheDevelopersFlagsAreTheNineteenAndOnlyThose();
    NothingAPlayerNeedsIsOnTheList();
    TheControlsHelpOfABuildWithoutTheToolsNamesNoFlagItRefuses();
    AnySuiteIsBuiltAsTheDevelopersDesktopBuildIs();
}

} // namespace

MP_TEST_MAIN("test_mp_platform", 100)
