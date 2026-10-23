#include "sim/StartChecks.hpp"

#include <algorithm>

namespace MagicPortals::StartChecks {

Verdict CheckVulkanLoader(const CanLoad& canLoad, std::uint32_t minimumVulkanMinor) {
    Verdict verdict;
    if (canLoad && canLoad(kVulkanLoaderName)) return verdict;
    // The engine clamps the minimum into 1 to 2 (GameRuntime::ResolveVulkanMinor); the words say what the run will really ask.
    const std::uint32_t minor = std::clamp<std::uint32_t>(minimumVulkanMinor, 1, 2);
    verdict.ok = false;
    verdict.what = std::string("No Vulkan graphics driver was found: Windows could not load ") + kVulkanLoaderName +
                   ", the Vulkan runtime that a graphics card's driver installs. Magic Portals draws with Vulkan and needs a driver that "
                   "provides Vulkan 1." +
                   std::to_string(minor) +
                   " or newer. Install or update the graphics driver from the maker of your graphics card (NVIDIA, AMD or Intel), or run "
                   "Windows Update, restart the computer if the installer asks, and start the game again.";
    return verdict;
}

bool IsDeveloperFlag(const std::string& arg) {
    return std::find(kDeveloperFlags.begin(), kDeveloperFlags.end(), std::string_view(arg)) != kDeveloperFlags.end();
}

std::string DeveloperFlagRefusal(const std::string& arg) {
    return arg + " is a developer's flag, and this build was made without the developer tools (the CMake option "
                 "MAGICPORTALS_DEV_TOOLS=OFF), so it does not have it. --help lists the flags it does have.";
}

} // namespace MagicPortals::StartChecks
