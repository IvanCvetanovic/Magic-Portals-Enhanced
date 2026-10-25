#pragma once

// WHERE THE GAME'S DATA IS IN AN APPLE BUNDLE, AND WHAT THE TWO ENTRIES DO WITH THE COMMAND LINE (planning step 206, PORT-8).
//
// WHY THIS EXISTS. The macOS and iOS entries (game/macos/MacMain.mm, game/ios/IOSMain.mm) are Objective-C++ that no machine here can compile or run,
// so every DECISION they make is kept out of them, in this one header, where a host compiler builds it and test_mp_applelayout pins it: the folders a
// bundle may keep the game's data in and which one wins, whether a run is the kind that may open a dialog, which arguments the game is handed, and
// the words of "there is no game data". The entries only ask the system for the strings (the bundle's folders, the user's) and for whether a path
// exists, and act on the answer. STRINGS ONLY: POSIX separators, no std::filesystem, no platform include, no I/O, so the suite runs the same on
// every host and a call cannot create a folder by asking.
//
// THE TWO LAYOUTS (docs/apple.md; out/parity/program/PLAT/platforms.md, section 6.2, decided there so that the resolver, the layout and the install
// text agree):
//   macOS  <bundle>/Contents/Resources/GameData            inside the app: the player's download holds it
//          <user's Application Support>/Magic Portals/GameData   the fallback a player may drop a data folder into by hand, where a sealed bundle
//                                                          is read-only and a folder BESIDE the app is not found (App Translocation, NOT KNOWN)
//   iOS    <bundle>/GameData                                inside the app: iOS has no "beside"
// A candidate HOLDS THE GAME when the two things the game's own --root check asks about exist under it: the levels folder and the chapters table
// of the packaged shape (sim/AssetRoot.hpp, Layout::Packaged(): the suite ties both names to it, so the entry and the game cannot disagree about
// what a data folder is).
//
// THE ENTRIES HAND THE FOLDER TO THE GAME THROUGH THE EXISTING --root FLAG (WithRoot) and call nothing of AssetRoot or main.cpp that they did not
// already: --root is the game's STRICT tier, so a folder that was chosen and then does not hold the game is an error that names it, and the
// game's own tiers (beside the program, the bundle, the user's data folder) are never reached from an Apple entry.
//
// WHAT IS NOT HERE: anything that asks the system. NOT KNOWN on any Apple device: that any of this finds a folder on a real Mac, iPhone or iPad.

#include <algorithm>
#include <iterator>
#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace MagicPortals::AppleLayout {

enum class Platform { MacOS, IOS };

// The one folder name of both layouts, and the game's own title: the user's data folder is <Application Support>/<kTitle>. The title is main.cpp's
// kTitle (a constant in an anonymous namespace there, which this header cannot reach without editing a file that feeds the Windows program): the
// suite reads main.cpp and holds the two to the same text.
inline constexpr const char* kGameDataFolder = "GameData";
inline constexpr const char* kTitle = "Magic Portals";

// What a folder must hold to be taken for the game: the packaged shape's levels folder and chapters table (AssetRoot::Layout::Packaged()).
inline constexpr const char* kLevelsMarker = "levels";
inline constexpr const char* kChaptersMarker = "tables/chapters.json";

// The developer's flags file of a phone: whitespace-separated flags the app's Documents folder holds for ONE launch (sim/Unpack.hpp, TakeFlagsFile
// reads, parses and consumes it). The same two names as the Android entry's (game/android/AndroidMain.cpp); the suite reads that file and holds
// these to its text.
inline constexpr const char* kFlagsFile = "magicportals_args.txt";
inline constexpr const char* kFlagsUsed = "magicportals_args.used";

// The engine's working folder, below the user's caches: the shaders are copied into <engine root>/assets/shaders at every launch and the program
// runs from there (ChooseAssetRoot takes the working directory when it holds assets/shaders), because a sealed bundle is read-only and the engine
// writes where it runs. <caches>/<bundle id>/engine on a Mac (the id keeps two apps' copies apart), <caches>/engine on an iOS device (its caches
// folder is the app's own already).
inline constexpr const char* kEngineFolder = "engine";
inline constexpr const char* kFallbackBundleId = "MagicPortals";

// ---- paths ------------------------------------------------------------------------------------------------------------------------

// `dir` and `name` with exactly one '/' between them; an empty side gives the other. No normalisation: the entries hand in what the system said.
inline std::string Join(const std::string& dir, const std::string& name) {
    if (dir.empty()) return name;
    if (name.empty()) return dir;
    const bool dirEnds = dir.back() == '/';
    const bool nameBegins = name.front() == '/';
    if (dirEnds && nameBegins) return dir + name.substr(1);
    if (dirEnds || nameBegins) return dir + name;
    return dir + "/" + name;
}

inline std::string EngineRoot(Platform platform, const std::string& cachesFolder, const std::string& bundleId) {
    if (cachesFolder.empty()) return {};
    if (platform == Platform::IOS) return Join(cachesFolder, kEngineFolder);
    return Join(Join(cachesFolder, bundleId.empty() ? std::string(kFallbackBundleId) : bundleId), kEngineFolder);
}

// ---- where the data may be ----------------------------------------------------------------------------------------------------------

struct Candidate {
    std::string label;  // for the words of a failure: "inside the app", "the user's Application Support folder"
    std::string folder; // the GameData folder itself
};

// The folders to look in, in the order they are looked in. `bundleFolder` is Contents/Resources on a Mac and the bundle's root on iOS, as the
// system names them; `userSupport` is the user's Application Support folder (macOS only; empty where the system names none, and then that
// candidate is left out, as is an empty bundle folder).
inline std::vector<Candidate> Candidates(Platform platform, const std::string& bundleFolder, const std::string& userSupport) {
    std::vector<Candidate> found;
    if (!bundleFolder.empty()) found.push_back({"inside the app", Join(bundleFolder, kGameDataFolder)});
    if (platform == Platform::MacOS && !userSupport.empty()) {
        found.push_back({"the Application Support folder", Join(Join(userSupport, kTitle), kGameDataFolder)});
    }
    return found;
}

// The only question asked of a machine: whether a path exists. A std::function so the suite hands in a set of names and a count of questions.
using Exists = std::function<bool(const std::string& path)>;

inline bool HoldsGame(const std::string& folder, const Exists& exists) {
    return exists && !folder.empty() && exists(Join(folder, kLevelsMarker)) && exists(Join(folder, kChaptersMarker));
}

struct Choice {
    bool found = false;
    std::string folder; // the winning candidate's folder
    std::string label;
    std::vector<std::string> tried; // "<label>: <folder>", every candidate asked about, in order (all of them when none held the game)
};

// The first candidate that holds the game. Never touches anything but `exists`.
inline Choice Choose(const std::vector<Candidate>& candidates, const Exists& exists) {
    Choice choice;
    for (const Candidate& candidate : candidates) {
        choice.tried.push_back(candidate.label + ": " + candidate.folder);
        if (HoldsGame(candidate.folder, exists)) {
            choice.found = true;
            choice.folder = candidate.folder;
            choice.label = candidate.label;
            return choice;
        }
    }
    return choice;
}

// ---- the arguments ---------------------------------------------------------------------------------------------------------------------

// The flags that take a path, the game's own and the one engine flag a capture uses. The entries change the working directory (to the engine's
// folder, above), so a relative path typed on a command line would otherwise mean something the shell did not mean; MakeAbsolute resolves each
// against the folder the program was STARTED in. Only the spaced form ("--flag value") is read, as the game's own parser reads it.
inline constexpr const char* kPathFlags[] = {"--root", "--levels", "--art", "--data", "--original", "--chapters", "--achievements", "--port-data", "--padart",
                                             "--saves", "--screenshot"};

inline bool IsPathFlag(const std::string& arg) {
    return std::any_of(std::begin(kPathFlags), std::end(kPathFlags), [&](const char* flag) { return arg == flag; });
}

// A path that starts at the root of the file system. POSIX only (this header runs on Apple): "C:/x" is relative here, as it is to the system.
inline bool IsAbsolutePosix(const std::string& path) { return !path.empty() && path.front() == '/'; }

// `args` (argv[0] first) with the value after every path flag made absolute against `launchDirectory`: left alone when it already is, when it is
// empty, and when `launchDirectory` is empty (the system named none).
inline std::vector<std::string> MakeAbsolute(std::vector<std::string> args, const std::string& launchDirectory) {
    if (launchDirectory.empty()) return args;
    for (std::size_t i = 1; i + 1 < args.size(); ++i) {
        if (!IsPathFlag(args[i])) continue;
        std::string& value = args[i + 1];
        if (!value.empty() && !IsAbsolutePosix(value)) value = Join(launchDirectory, value);
        ++i; // the value is not a flag, whatever it spells
    }
    return args;
}

// Whether the arguments already say where the data is (--root, anywhere after the program's name). Then the entry looks nothing up and shows
// nothing: the game's strict --root check says what is wrong, in words, and a CI run that points at its own folder never meets a dialog.
inline bool GivesRoot(const std::vector<std::string>& args) {
    for (std::size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "--root") return true;
        if (IsPathFlag(args[i])) ++i; // skip its value
    }
    return false;
}

// `args` with "--root <root>" put right after the program's name, unless the caller gave one (then they are returned as they were).
inline std::vector<std::string> WithRoot(std::vector<std::string> args, const std::string& root) {
    if (root.empty() || GivesRoot(args)) return args;
    if (args.empty()) args.push_back("MagicPortals");
    args.insert(args.begin() + 1, {"--root", root});
    return args;
}

// Whether somebody is at this run, from its command line alone: the same narrow rule as RunKind.hpp's AttendedRun, which needs the engine's parsed
// options and cannot be called before the entry has chosen a folder. A run is UNATTENDED when any flag that only a script gives is present: a capture
// (--frames, --screenshot, --screenshot-ui, --screenshot-every), a fixed step, a hidden window, a recording or a replay, asset import, and the
// game's developer flags (--saves, --route, --route-trace, --visit-levels, --press, --hold, --tap, --drag, --burn-dragon, --place,
// --light-masks-off). Only an attended run may open a dialog: a dialog on a runner that nobody watches blocks until the job times out.
inline constexpr const char* kUnattendedFlags[] = {"--frames", "--fixed-step", "--hidden", "--screenshot", "--screenshot-ui", "--screenshot-every",
                                                   "--record", "--replay", "--import-assets", "--saves", "--route", "--route-trace", "--visit-levels",
                                                   "--press", "--hold", "--tap", "--drag", "--burn-dragon", "--place", "--light-masks-off"};

inline bool Unattended(const std::vector<std::string>& args) {
    for (std::size_t i = 1; i < args.size(); ++i) {
        if (std::any_of(std::begin(kUnattendedFlags), std::end(kUnattendedFlags), [&](const char* flag) { return args[i] == flag; })) return true;
    }
    return false;
}

// ---- the words ------------------------------------------------------------------------------------------------------------------------------

// What a start that found no game data says, in words, naming where it looked. The same text goes to stderr, to the log and (macOS, an attended
// run) to a dialog.
inline std::string NoGameDataText(Platform platform, const std::vector<std::string>& tried) {
    std::string text = "Magic Portals cannot start: it did not find the game's data.\n\n";
    text += "This program holds the game's engine and rules, but not its levels, pictures or sound.";
    if (platform == Platform::IOS) {
        text += " They are meant to be part of the app, and this copy was made without them.";
    } else {
        text += " They are meant to be part of the app, or in a folder called GameData that you put in one of the places below.";
    }
    text += "\n\nIt looked in:";
    if (tried.empty()) text += "\n  (nowhere: the system named no folder to look in)";
    for (const std::string& place : tried) text += "\n  " + place;
    return text;
}

} // namespace MagicPortals::AppleLayout
