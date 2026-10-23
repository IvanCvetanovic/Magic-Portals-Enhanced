#include "sim/StartChecks.hpp"

#include "sim/EngineTuning.hpp"

#include <algorithm>

namespace MagicPortals::StartChecks {

Verdict CheckVulkanLoader(const TryLoad& tryLoad, std::uint32_t minimumVulkanMinor) {
    Verdict verdict;
    // An absent function is a question that cannot be answered, and an unanswered question is not a yes: it is a "no" that gave no reason.
    const LoadResult result = tryLoad ? tryLoad(kVulkanLoaderName) : LoadResult{};
    // `loaded` alone decides: a success leaves a stale code behind (see LoadResult), which must never turn a start that works into a stop.
    if (result.loaded) return verdict;
    // The engine clamps the minimum into its range (GameRuntime::ResolveVulkanMinor, EngineTuning::kVulkanMinorMin and Max, which are that range);
    // the words say what the run will really ask.
    const std::uint32_t minor =
        std::clamp<std::uint32_t>(minimumVulkanMinor, EngineTuning::kVulkanMinorMin, EngineTuning::kVulkanMinorMax);
    const std::string name = kVulkanLoaderName;
    const std::string needs = "Magic Portals draws with Vulkan and needs a driver that provides Vulkan 1." + std::to_string(minor) + " or newer. ";
    const std::string restart = "restart the computer if the installer asks, and start the game again.";
    const std::string maker = "from the maker of your graphics card (NVIDIA, AMD or Intel)";
    verdict.ok = false;
    switch (result.error) {
    case kLoadErrorModuleNotFound:
        // 126 is the file, or a file it needs, not being found: the words do not say the loader itself is gone.
        verdict.what = "No Vulkan graphics driver was found: Windows could not load " + name +
                       ", the Vulkan runtime that a graphics card's driver installs, because it or a file it needs was not found (Windows error " +
                       std::to_string(kLoadErrorModuleNotFound) + "). " + needs + "Install or update the graphics driver " + maker +
                       ", or run Windows Update, " + restart;
        break;
    case kLoadErrorBadImage:
        // 193 is a file that IS there and is not a usable image: a reinstall, and a stray copy beside the game, are the cures.
        verdict.what = "The Vulkan graphics driver cannot be used: Windows found " + name +
                       ", the Vulkan runtime that a graphics card's driver installs, and could not run it, because it is not a valid program "
                       "file for this computer (Windows error " +
                       std::to_string(kLoadErrorBadImage) +
                       "). It is damaged, or it was made for another kind of computer, for example a 32-bit file on 64-bit Windows. " + needs +
                       "Reinstall the graphics driver " + maker + ", " + restart + " If a file named " + name +
                       " lies beside the game, take it away.";
        break;
    default:
        // Anything else is shown as it is, so that the log a tester sends says why.
        verdict.what = "Windows could not load " + name + ", the Vulkan runtime that a graphics card's driver installs (" +
                       (result.error == 0 ? std::string("Windows gave no reason") : "Windows error " + std::to_string(result.error)) + "). " +
                       needs + "Install or update the graphics driver " + maker + ", or run Windows Update, " + restart;
        break;
    }
    return verdict;
}

bool IsDeveloperFlag(const std::string& arg) {
    return std::find(kDeveloperFlags.begin(), kDeveloperFlags.end(), std::string_view(arg)) != kDeveloperFlags.end();
}

std::string DeveloperFlagRefusal(const std::string& arg) {
    return arg + " is a developer's flag, and this build was made without the developer tools (the CMake option "
                 "MAGICPORTALS_DEV_TOOLS=OFF), so it does not have it. --help lists the flags it does have.";
}

std::string ControlsRunKindsHelp(bool developerTools) {
    // Each line is indented to the --controls entry's column by the loop below; the text of the developer-tools version is what main.cpp printed
    // before this function, line for line (a --help diff of the default build against Step 201's executable is empty).
    static constexpr std::array<std::string_view, 9> kWithTools = {
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
    // Without the flags --route and --visit-levels, a developer's run is the engine's --fixed-step alone; nothing here names a flag the build refuses.
    static constexpr std::array<std::string_view, 9> kWithoutTools = {
        "A player's run (one without --fixed-step) has an Options button in",
        "the main menu's top-right corner, where the choice is made; it is",
        "kept in controls.json beside the other saves and read at the next",
        "start (shipped default: modern).",
        "With this flag the run STARTS in that style, ignores the saved",
        "choice and writes nothing: Options then chooses for the run only.",
        "`classic` is the original's controls with no Options button. A run",
        "with --fixed-step (the engine's flag, for a capture) is classic and",
        "has no Options button, unless the flag says otherwise",
    };
    const std::array<std::string_view, 9>& lines = developerTools ? kWithTools : kWithoutTools;
    std::string text;
    for (const std::string_view line : lines) {
        text += "                    ";
        text += line;
        text += '\n';
    }
    return text;
}

} // namespace MagicPortals::StartChecks
