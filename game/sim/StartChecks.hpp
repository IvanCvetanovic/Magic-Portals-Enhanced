#pragma once

// WHAT THE START CHECKS BEFORE IT TOUCHES THE ENGINE, and what it says when a build has no developer tools (planning step 201, PORT-3).
// Two rules that main.cpp cannot hand to a suite, so they are pure functions over what they are told:
//
// 1. THE VULKAN LOADER. A Windows release build (MAGICPORTALS_WIN_GUI=ON) links vulkan-1.dll LATE: the loader finds it at the first Vulkan
//    call instead of at process start. Linked at process start, a PC with no graphics driver at all never reaches main(): Windows refuses to
//    start the program with a dialog that names a DLL and nothing else, and a player who double-clicked an icon sees that and no game. Late,
//    the program starts, asks the question below before it calls Vulkan once, and says in plain words what is missing and what to do. The
//    question itself ("can this library be loaded?") is Windows' and is asked in main.cpp; the answer, and the words, are here.
//
// 2. THE DEVELOPER'S FLAGS. A build made with MAGICPORTALS_DEV_TOOLS=OFF (a phone's, by default) does not carry --route, --tap, --visit-levels and
//    the rest. A flag it does not know the engine's parser would refuse with a message about an unknown option, which is true and tells a
//    developer nothing about WHY; this list lets the refusal say that the flag exists and is not in this build. And the one help entry that
//    speaks of the developer's runs (--controls) is a function of the build's kind, so that a build without the flags does not name them.
//
// Free of the engine, of the renderer and of the operating system, like the rest of sim/.

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace MagicPortals::StartChecks {

// The library a Windows release build loads late. A name, not a path: the answer to "can it be loaded" is the system's, by its own search.
// THE ONE DEFINITION of the name: game/windows/MagicPortalsWindows.cmake reads this very line at configure time and passes the name it finds to
// the linker as /DELAYLOAD (and stops the configure when it cannot find it), so what the code asks for and what the link delays cannot differ.
// Keep the declaration below on one line, in exactly the form `inline constexpr const char* kVulkanLoaderName = "<name>";`, which is the shape the
// build looks for (a configure that finds none, or two, stops).
inline constexpr const char* kVulkanLoaderName = "vulkan-1.dll";

// The two Windows error codes that tell a player something different (GetLastError after a failed LoadLibraryExW), spelled once. sim/ takes no
// windows.h, so the numbers are written here with the names Windows gives them: ERROR_MOD_NOT_FOUND and ERROR_BAD_EXE_FORMAT.
inline constexpr std::uint32_t kLoadErrorModuleNotFound = 126; // the file, or a file it depends on, was not found
inline constexpr std::uint32_t kLoadErrorBadImage = 193;       // the file was found and is not a valid image for this process (damaged, or 32-bit)

struct Verdict {
    bool ok = true;
    // Empty when ok. Otherwise what is missing and what to do, for a player: main puts it in the log and on stderr and, for a person's run
    // only (RunKind.hpp), in the one message box a failed start shows.
    std::string what;
};

// What loading the named library came to. `error` is the operating system's code for WHY it failed (Windows: GetLastError, read at once after the
// failing call) and means nothing when `loaded` is true: a success leaves whatever code was there before, so only `loaded` decides, never `error`.
struct LoadResult {
    bool loaded = false;
    std::uint32_t error = 0; // 0: no reason was given
};

// Tries to load the named library. Injected, so that a suite can say "no", with any code, without removing a DLL from the machine it runs on.
using TryLoad = std::function<LoadResult(const std::string& library)>;

// Asks `tryLoad` for the Vulkan loader. `minimumVulkanMinor` is the 1.x the run asks of a GPU (the engine's 1 to 2, as GameManifest::minimumVulkanMinor
// clamps it): the words name it, because a person with a 1.1 driver reads "Vulkan 1.2" as their trouble. A failure is told three ways: the loader
// or a file it needs was not found (126), the file is there and is not a usable image (193: damaged, or the wrong kind of computer), and any other
// code, shown as it is. An absent function is a question that cannot be answered, told as a failure that gave no reason.
Verdict CheckVulkanLoader(const TryLoad& tryLoad, std::uint32_t minimumVulkanMinor);

// The flags MAGICPORTALS_DEV_TOOLS=OFF takes out of the program, spelled as the command line spells them. The flags a player needs are not
// here: --level, --root, --no-baked-paths, the per-path flags, --saves, --controls, --profile and the engine's own.
inline constexpr std::array<std::string_view, 16> kDeveloperFlags = {
    "--route",        "--route-trace", "--tap",           "--hold",        "--drag",         "--press",
    "--place",        "--burn-dragon", "--visit-levels",  "--visit-passes", "--light-masks-off",
    "--shadow-maps",  "--sprites-only", "--asset-watching", "--sprite-report", "--perf-log",
};

// Whether `arg` is one of kDeveloperFlags (an exact match: "--routes" is not "--route").
bool IsDeveloperFlag(const std::string& arg);

// The line a build without the developer tools prints, and exits 1 on, for one of them.
std::string DeveloperFlagRefusal(const std::string& arg);

// The tail of --help's --controls entry: which runs have the Options button and which are classic. It says that "a developer's run" is classic, and
// what a developer's run IS depends on the build: with the developer tools it is a run with --fixed-step, --route or --visit-levels; without them
// only --fixed-step (the engine's flag) is left, and the text must not name two flags the build refuses (Step 201's finding, PORT3-2). With
// `developerTools` true the text is, byte for byte, what --help said before this function existed. Ends with a newline; every line is indented to
// the entry's column.
std::string ControlsRunKindsHelp(bool developerTools);

} // namespace MagicPortals::StartChecks
