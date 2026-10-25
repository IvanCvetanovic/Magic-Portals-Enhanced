// The Mac's way into the game: main() for a macOS build, doing what a Mac
// needs before MagicPortalsMain (main.cpp) and nothing else.
//
// WRITTEN, NEVER RUN (planning step 206, PORT-8): no Mac was available. This file is the sibling port's MacMain.mm, whose Apple glue ran on GitHub's
// macOS runners, changed only where this game differs (Step 206 of docs/planning/2026-09-11-magic-portals-remaster.md has the table of every changed
// line). NOT KNOWN: that it compiles with Xcode's SDK, that it finds the data in a bundle, that the alert opens.
//
//  - THE FILES, inside MagicPortals.app (tools/apple/make_app.sh). Its
//    Contents/Resources holds GameData/ (the game's data: the levels, the
//    art and the sound, which a data-less download does not have) and
//    engine/assets/shaders (the engine's): the game is pointed at the
//    first with --root, as a packaged desktop folder would be given one, and
//    the engine at a working directory it
//    may write in (its pipeline cache, assets/scenes) - a folder under
//    ~/Library/Caches holding a copy of the shaders, which ChooseAssetRoot's
//    working-directory rule then takes. A bundle's own contents are never
//    written: a signed app that changed itself would no longer open.
//    A player's own --root wins, being the one the game is given (and then
//    nothing is looked up), and the path flags are made absolute first,
//    against the folder they were typed in. WHERE the data may be, which
//    candidate wins and the words of "none" are sim/AppleLayout.hpp's, pinned
//    by test_mp_applelayout: this file only asks the system for strings.
//  - NO GAME DATA. A start that finds none says so in words on stderr and, for
//    a run somebody is at, in an alert (a run a script made, with --frames or
//    --screenshot, never opens one: nobody would close it), and ends with a
//    failure code. It never reaches the game.
//
// Outside a bundle - build/game/MagicPortals, as the suites and captures run it -
// nothing is added; the build's baked paths find everything, as on
// Linux.

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "MagicPortalsMain.hpp"
#include "sim/AppleLayout.hpp"

namespace {

namespace fs = std::filesystem;
namespace AppleLayout = MagicPortals::AppleLayout;

// Contents/Resources when this is MagicPortals.app; empty for a
// build-tree run.
fs::path BundleResources() {
    NSBundle* bundle = NSBundle.mainBundle;
    if (![bundle.bundlePath.pathExtension isEqualToString:@"app"] || bundle.resourcePath == nil) return {};
    return fs::path(bundle.resourcePath.UTF8String);
}

// The user's Application Support folder, as the system names it: only a string, nothing is made (the engine's UserDataDirectory makes the
// player's folder when it answers, and a start that has no data has no business making one).
std::string UserSupportFolder() {
    NSArray<NSString*>* folders = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);
    return folders.count > 0 ? std::string(folders.firstObject.UTF8String) : std::string();
}

// ~/Library/Caches/<bundle id>/engine, with the bundle's shaders copied into
// assets/shaders. Copied every launch: seventeen small files, and a newer
// bundle's shaders must never lose to an older copy. False if it cannot be
// made; the engine then looks where it always does (the build's engine
// checkout, which a bundle run on another Mac does not have).
bool PrepareEngineRoot(const fs::path& resources, fs::path& root) {
    NSArray<NSString*>* caches = NSSearchPathForDirectoriesInDomains(NSCachesDirectory, NSUserDomainMask, YES);
    if (caches.count == 0) return false;
    NSString* identifier = NSBundle.mainBundle.bundleIdentifier;
    root = fs::path(AppleLayout::EngineRoot(AppleLayout::Platform::MacOS, caches.firstObject.UTF8String,
                                            identifier != nil ? identifier.UTF8String : ""));

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

// The alert of a start that cannot go on. A process the Finder started has no running application object yet, and an alert needs one to be shown
// in front: made here, as a regular app, then brought forward. NOT KNOWN on any Mac: whether the alert is seen, or in front.
void ShowAlert(const std::string& words) {
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    [NSApp activateIgnoringOtherApps:YES];
    NSAlert* alert = [[NSAlert alloc] init];
    alert.messageText = [NSString stringWithUTF8String:AppleLayout::kTitle];
    alert.informativeText = [NSString stringWithUTF8String:words.c_str()];
    alert.alertStyle = NSAlertStyleCritical;
    [alert runModal];
}

bool PathExists(const std::string& path) {
    std::error_code ec;
    return fs::exists(path, ec);
}

} // namespace

int main(int argc, char** argv) {
    @autoreleasepool {
        std::vector<std::string> args(argv, argv + argc);
        const fs::path resources = BundleResources();
        if (!resources.empty()) {
            std::error_code ec;
            const fs::path launchedIn = fs::current_path(ec);
            args = AppleLayout::MakeAbsolute(std::move(args), ec ? std::string() : launchedIn.string());

            // Where the game's data is, unless the player or a script said: nothing is looked up for a --root that was given.
            if (!AppleLayout::GivesRoot(args)) {
                const AppleLayout::Choice choice = AppleLayout::Choose(
                    AppleLayout::Candidates(AppleLayout::Platform::MacOS, resources.string(), UserSupportFolder()), PathExists);
                if (!choice.found) {
                    const std::string words = AppleLayout::NoGameDataText(AppleLayout::Platform::MacOS, choice.tried);
                    std::cerr << "[" << AppleLayout::kTitle << "] " << words << std::endl;
                    if (!AppleLayout::Unattended(args)) ShowAlert(words);
                    return EXIT_FAILURE;
                }
                args = AppleLayout::WithRoot(std::move(args), choice.folder);
            }

            fs::path engineRoot;
            if (!PrepareEngineRoot(resources, engineRoot)) {
                std::cerr << "[Magic Portals] could not copy the engine's shaders to " << engineRoot.string() << std::endl;
            } else if (fs::current_path(engineRoot, ec); ec) {
                std::cerr << "[Magic Portals] cannot enter " << engineRoot.string() << ": " << ec.message() << std::endl;
            }
        }

        std::vector<char*> pointers;
        pointers.reserve(args.size() + 1);
        for (std::string& arg : args) pointers.push_back(arg.data());
        pointers.push_back(nullptr);
        return MagicPortalsMain(static_cast<int>(args.size()), pointers.data());
    }
}
