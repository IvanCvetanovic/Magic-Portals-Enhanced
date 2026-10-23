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
//    developer nothing about WHY; this list lets the refusal say that the flag exists and is not in this build.
//
// Free of the engine, of the renderer and of the operating system, like the rest of sim/.

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace MagicPortals::StartChecks {

// The library a Windows release build loads late. A name, not a path: the answer to "can it be loaded" is the system's, by its own search.
inline constexpr const char* kVulkanLoaderName = "vulkan-1.dll";

struct Verdict {
    bool ok = true;
    // Empty when ok. Otherwise what is missing and what to do, for a player: main puts it in the log and on stderr and, for a person's run
    // only (RunKind.hpp), in the one message box a failed start shows.
    std::string what;
};

// Whether the named library can be loaded. Injected, so that a suite can say "no" without removing a DLL from the machine it runs on.
using CanLoad = std::function<bool(const std::string& library)>;

// Asks `canLoad` for the Vulkan loader. `minimumVulkanMinor` is the 1.x the run asks of a GPU (1 or 2, as GameManifest::minimumVulkanMinor
// clamps it): the words name it, because a person with a 1.1 driver reads "Vulkan 1.2" as their trouble.
Verdict CheckVulkanLoader(const CanLoad& canLoad, std::uint32_t minimumVulkanMinor);

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

} // namespace MagicPortals::StartChecks
