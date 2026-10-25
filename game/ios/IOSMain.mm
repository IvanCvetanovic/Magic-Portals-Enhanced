// iOS's way into the game: the engine's main() (engine/src/platform/ios/
// IOSApp.mm) starts UIKit and calls SupersonicMain once the app's scene shows a
// view to draw on, and this does what a desktop's folder layout and command
// line did before handing on to MagicPortalsMain (main.cpp), as Android's entry
// does (android/AndroidMain.cpp):
//
// WRITTEN, NEVER RUN (planning step 206, PORT-8): no iPhone, iPad or Mac was available. This file is the sibling port's IOSMain.mm, whose Apple
// glue was built on GitHub's macOS runners and never run on a device, changed only where this game differs (Step 206 of
// docs/planning/2026-09-11-magic-portals-remaster.md has the table of every changed line). NOT KNOWN: that it compiles with Xcode's SDK, and anything
// about a run.
//
//  - THE FILES. MagicPortals.app carries GameData/ (the game's data, which a
//    data-less download does not have) and engine/assets/shaders
//    (tools/apple/make_app.sh). An app bundle has real paths, unlike an APK's assets, so nothing is unpacked:
//    the game is pointed at the first with --root, which sim/AppleLayout.hpp
//    chooses (and pins, test_mp_applelayout). The
//    bundle is read-only, and the engine writes where it runs (its pipeline
//    cache, assets/scenes), so its working directory is Library/Caches/engine
//    with a copy of the shaders in assets/shaders - ChooseAssetRoot's
//    working-directory rule. Saves and settings go to Library/Application
//    Support (the engine's UserDataDirectory), as on a Mac.
//  - DEVELOPMENT FLAGS. A phone has no command line, so a run's flags come from
//    magicportals_args.txt in the app's Documents folder: whitespace-separated,
//    double quotes around an argument with spaces. Read once and renamed to
//    magicportals_args.used, so one scripted run does not become every later
//    launch: by Unpack::TakeFlagsFile, the code the Android entry uses (one
//    definition of the rule, not a copy). A build with the developer tools off (an iOS build is
//    one) refuses the developer's flags by name, so the file holds a player's
//    flags (--level, --root, the engine's capture flags). A path after one of
//    the flags in AppleLayout::kPathFlags (--root, --levels, --art, --data,
//    --original, --chapters, --achievements, --port-data, --padart, --saves,
//    --screenshot) that is relative means relative to Documents. Only those are
//    made absolute: the engine's other path flags (--screenshot-ui, --record,
//    --replay, --scene) are handed on as typed, and a relative value of one of
//    them lands in the engine's working folder (APPLE-24).
//  - NO GAME DATA. A start that finds none says so in the log (and so on stderr)
//    and in the log file the game keeps, as the Android entry's failed start
//    does, and ends with a failure code: the engine ends the process at once
//    with what this returns, so a dialog would not be seen (the sibling's approach).

#import <Foundation/Foundation.h>

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "MagicPortalsMain.hpp"
#include "core/Log.hpp"
#include "platform/ExecutablePath.hpp"
#include "platform/ios/IOSApp.hpp"
#include "sim/AppleLayout.hpp"
#include "sim/Scratch.hpp"
#include "sim/Unpack.hpp"

namespace {

namespace fs = std::filesystem;
namespace AppleLayout = MagicPortals::AppleLayout;
namespace Unpack = MagicPortals::Unpack;

constexpr const char* kLogTag = "Magic Portals";

fs::path UserFolder(NSSearchPathDirectory which) {
    NSArray<NSString*>* folders = NSSearchPathForDirectoriesInDomains(which, NSUserDomainMask, YES);
    return folders.count > 0 ? fs::path(folders.firstObject.UTF8String) : fs::path();
}

// The engine's working directory: somewhere it may write, holding the
// bundle's shaders. Copied every launch - seventeen small files - so an
// updated app never runs an older copy.
bool PrepareEngineRoot(const fs::path& resources, const fs::path& root) {
    const fs::path from = resources / "engine" / "assets" / "shaders";
    const fs::path to = root / "assets" / "shaders";
    std::error_code ec;
    fs::create_directories(to, ec);
    if (ec) return false;
    for (fs::directory_iterator it(from, ec), end; !ec && it != end; it.increment(ec)) {
        if (!it->is_regular_file(ec)) continue;
        fs::copy_file(it->path(), to / it->path().filename(), fs::copy_options::overwrite_existing, ec);
        if (ec) return false;
    }
    return !ec;
}

bool PathExists(const std::string& path) {
    std::error_code ec;
    return fs::exists(path, ec);
}

// What stopped the start, said where it can be: the system's log and stderr (the engine's log does both), and the game's own log file, from the
// folder and the two names the game itself opens its log with (Scratch::PhoneLogFolder and LogPathsIn: the Android entry's own record of a failed
// start, from AndroidMain.cpp), so a tester who is sent the log has the words.
void Tell(const std::string& text) {
    SUPERSONIC_LOG_ERROR(kLogTag) << text;
    try {
        const fs::path folder = MagicPortals::Scratch::PhoneLogFolder(Supersonic::UserDataDirectory(AppleLayout::kTitle));
        if (folder.empty()) return;
        const MagicPortals::Scratch::LogPaths logs = MagicPortals::Scratch::LogPathsIn(folder);
        std::string why;
        if (!Unpack::RecordFailure(logs.log.string(), logs.previous.string(), text, why)) {
            SUPERSONIC_LOG_WARN(kLogTag) << "The log file could not be written: " << why;
        }
    } catch (const std::exception& e) {
        SUPERSONIC_LOG_WARN(kLogTag) << "The log file could not be written: " << e.what();
    }
}

} // namespace

int SupersonicMain(int argc, char** argv) {
    // No @autoreleasepool around the game: the engine drains one of its own
    // each frame (IOSApp.mm), and a pool here, popped when the game returns,
    // would take that one with it before the engine pops it again.
    NSString* resourcePath = NSBundle.mainBundle.resourcePath;
    if (resourcePath == nil) {
        SUPERSONIC_LOG_ERROR(kLogTag) << "The app bundle has no resource folder; stopping.";
        return EXIT_FAILURE;
    }
    const fs::path resources(resourcePath.UTF8String);

    const fs::path engineRoot = AppleLayout::EngineRoot(AppleLayout::Platform::IOS, UserFolder(NSCachesDirectory).string(), std::string());
    if (engineRoot.has_parent_path() && PrepareEngineRoot(resources, engineRoot)) {
        std::error_code ec;
        fs::current_path(engineRoot, ec);
        if (ec) SUPERSONIC_LOG_ERROR(kLogTag) << "Cannot enter " << engineRoot.string() << ": " << ec.message();
    } else {
        SUPERSONIC_LOG_ERROR(kLogTag) << "Could not copy the engine's shaders to " << engineRoot.string() << ".";
    }

    // The development flags, if a file of them was left for this launch.
    const fs::path documents = UserFolder(NSDocumentDirectory);
    std::vector<std::string> args;
    args.emplace_back(argc > 0 && argv != nullptr ? argv[0] : Unpack::kDefaultProgramName);
    if (!documents.empty()) {
        Unpack::FlagsFile flags = Unpack::TakeFlagsFile(documents.string(), AppleLayout::kFlagsFile, AppleLayout::kFlagsUsed);
        if (!flags.info.empty()) SUPERSONIC_LOG_INFO(kLogTag) << flags.info;
        if (!flags.error.empty()) SUPERSONIC_LOG_ERROR(kLogTag) << flags.error;
        if (!flags.warning.empty()) SUPERSONIC_LOG_WARN(kLogTag) << flags.warning;
        for (std::string& flag : flags.args) args.push_back(std::move(flag));
        args = AppleLayout::MakeAbsolute(std::move(args), documents.string());
    }

    // Where the game's data is, unless the flags said: nothing is looked up for a --root that was given.
    if (!AppleLayout::GivesRoot(args)) {
        const AppleLayout::Choice choice =
            AppleLayout::Choose(AppleLayout::Candidates(AppleLayout::Platform::IOS, resources.string(), std::string()), PathExists);
        if (!choice.found) {
            Tell(AppleLayout::NoGameDataText(AppleLayout::Platform::IOS, choice.tried));
            return EXIT_FAILURE;
        }
        args = AppleLayout::WithRoot(std::move(args), choice.folder);
    }

    std::vector<char*> pointers;
    pointers.reserve(args.size() + 1);
    for (std::string& arg : args) pointers.push_back(arg.data());
    pointers.push_back(nullptr);
    return MagicPortalsMain(static_cast<int>(args.size()), pointers.data());
}
