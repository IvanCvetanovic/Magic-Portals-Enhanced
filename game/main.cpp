// Magic Portals on the Supersonic Engine: the Godot project's levels, in the
// original's order, as the port plays them.
//
// The levels, their order, their art and Enhanced's data are read from
// outside this repository. Where is decided at run time (sim/AssetRoot.hpp,
// planning step 200): --root, a folder next to the program, the user's data
// folder, and last the paths the build was told (SUPERSONIC_MAGICPORTALS_*),
// with a flag for each path over the answer (--levels, --art, --data, ...).
// Drawn with the levels' own art and a few lines of text. See
// docs/planning/2026-09-11-magic-portals-remaster.md.

#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "core/GameRuntime.hpp"
#include "core/JobSystem.hpp"
#include "core/LaunchOptions.hpp"
#include "platform/ExecutablePath.hpp"
#include "core/Log.hpp"
#include "core/SupersonicApp.hpp"
#include "renderer/VulkanContext.hpp"

#include "BuildProfile.hpp"
#if MAGICPORTALS_DEV_TOOLS
#include "LevelVisit.hpp"
#endif
#include "MagicPortalsLayer.hpp"
#include "MagicPortalsMain.hpp"
#if MAGICPORTALS_DEV_TOOLS
#include "Route.hpp"
#endif
#include "RunKind.hpp"
#include "sim/AssetRoot.hpp"
#include "sim/Controls.hpp"
#include "sim/EngineTuning.hpp"
#include "sim/Files.hpp"
#include "sim/Scratch.hpp"
#include "sim/StartChecks.hpp"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif
#if defined(__ANDROID__)
#include "platform/android/AndroidApp.hpp"
#endif

// Where Magic Rampage's modern-control pictures are looked for (game/CMakeLists.txt, SUPERSONIC_MAGICPORTALS_PADART): a
// definition of THIS file alone, so the suites, which never open the folder, are not rebuilt for it. Empty where the
// build did not say, which is "nowhere" and costs a modern run its first picture tier, nothing else.
#ifndef MAGICPORTALS_PADART_DIR
#define MAGICPORTALS_PADART_DIR ""
#endif

namespace {

// Where Magic Rampage's editor pictures (the adjustment screen's eight, tools/art --stage editor) are looked for: the folder `editor` BESIDE the
// controls' (reference/magic-rampage/editor beside reference/magic-rampage/controls), so --padart moves both and the build is told one place.
// Empty where there is no controls folder to be beside: "nowhere". (An anonymous namespace: it is a path rule of this file, and only main asks it.)
std::string EditorArtDirBeside(const std::string& padArtDir) {
    if (padArtDir.empty()) return {};
    std::filesystem::path controls = std::filesystem::path(padArtDir).lexically_normal();
    if (!controls.has_filename()) controls = controls.parent_path(); // "<dir>/" names <dir>
    return (controls.parent_path() / "editor").string();
}

// What --help says of a path's default: the path the build was configured with, or that a release build has none (it finds its data
// at run time, see --root).
std::string DefaultText(const std::string& baked) {
    return baked.empty() ? std::string("none baked in: found at run time") : "default " + baked;
}

} // namespace

// The engine commit this build was configured against (game/CMakeLists.txt), for the log's first line. A definition of THIS file alone,
// for the same reason. "unknown" where git was not there to ask.
#ifndef MAGICPORTALS_ENGINE_COMMIT
#define MAGICPORTALS_ENGINE_COMMIT "unknown"
#endif

namespace {

// The game's name: the manifest's title, the user data directory's and the message box's.
constexpr const char* kTitle = "Magic Portals";

// The last run's log moved to `previous`, replacing the one before (the rule is Scratch::KeepPreviousLog's, which the Android entry uses too, planning
// step 204): a log that cannot be kept costs that copy and says so, never the start.
void KeepPreviousLog(const std::filesystem::path& log, const std::filesystem::path& previous) {
    std::string why;
    if (!MagicPortals::Scratch::KeepPreviousLog(log, previous, why)) {
        std::cerr << "[Magic Portals] the last run's log was not kept in " << previous.string() << ": "
                  << why << std::endl;
    }
}

// What main's catch needs, filled in as the start learns it. Declared outside the try, so the catch can read what the try found.
struct FatalContext {
    std::string logPath;   // where the log is, WRITTEN: empty until the file sink really opened (a box must never name a log that was not written)
    bool attended = false; // a player's run: the only kind that may open a message box (RunKind.hpp)
};

#if defined(_WIN32)
// The text on the screen. MessageBoxW, from UTF-8: the log's path can hold any character a profile name can.
void ShowFatalDialog(const std::string& utf8) {
    const int wide = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    if (wide <= 0) return;
    std::wstring text(static_cast<std::size_t>(wide), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, text.data(), wide);
    MessageBoxW(nullptr, text.c_str(), L"Magic Portals", MB_OK | MB_ICONERROR);
}
#elif defined(__ANDROID__)
// A phone: a dialog of the activity (the engine's Android::ShowMessage), which returns when the player closes it, when the app is asked to finish or after
// five minutes, and keeps the activity answering meanwhile. Without it a phone that cannot start the game shows a black screen and the launcher
// (planning step 203; the sibling port's E39). Only a player's run gets here (RunKind.hpp). False when the activity is not the engine's: the text is then
// in the log alone.
void ShowFatalDialog(const std::string& utf8) { Supersonic::Android::ShowMessage(kTitle, utf8); }
#else
void ShowFatalDialog(const std::string&) {}
#endif

// Whether a library can be loaded by name, and if not why: the question sim/StartChecks.hpp's CheckVulkanLoader is put (planning step 201). Windows:
// the very search the delay-load helper makes at the first Vulkan call (LoadLibraryExW with no flags), and the module is left loaded, so that call
// binds to this copy. A failure carries GetLastError (planning step 202, PORT3-2): 126 is a file not found, 193 a file that is not a valid image
// (the wrong kind of computer, or damaged), and the pure check tells a player which. It is read at once after the failing call and nowhere else, as
// the next call of any kind may overwrite it. Elsewhere nothing is loaded late - the program links its loader outright, or the platform's entry
// point has loaded it - so the answer is yes.
MagicPortals::StartChecks::LoadResult TryLoadLibrary(const std::string& name) {
    MagicPortals::StartChecks::LoadResult result;
#if defined(_WIN32)
    const std::wstring wide(name.begin(), name.end()); // a library's name is ASCII
    if (LoadLibraryExW(wide.c_str(), nullptr, 0) != nullptr) {
        result.loaded = true;
    } else {
        result.error = static_cast<std::uint32_t>(GetLastError());
    }
#else
    (void)name;
    result.loaded = true;
#endif
    return result;
}

// A run that stopped on an error: the reason into the log BEFORE the file sink closes (it used to go to stderr alone, and the
// previous run's lines were all a tester had; the log's own console sink says it on stderr too), then - for a player's run and no other - a
// message box naming the log.
// Never throws: it runs inside a catch.
int ReportFatal(const std::string& what, const FatalContext& context) noexcept {
    try {
        // The log writes an error to stderr as well as to the file sink, so there is no line of our own for stderr.
        SUPERSONIC_LOG_ERROR("Magic Portals") << "fatal: " << what << std::endl;
        Supersonic::Log::CloseFileSink();
        const std::string text = MagicPortals::FatalText(what, context.logPath, context.attended);
        if (!text.empty()) ShowFatalDialog(text);
    } catch (...) {
        // Nothing more can be said; the exit code still is.
    }
    // LAST, so that a hang in it can hide neither the line nor the box (planning step 190). A throw out of the engine's SupersonicApp
    // constructor leaves the job system's workers running: its only Shutdown is in the destructor, which a constructor that threw never
    // runs, and the static pool's joinable threads then end the process at exit with 0xC0000409 instead of the 1 returned below (18 of
    // 18 runs of the constructor-throw probe, Step 189). Shutdown returns at once when the pool is not running, which is every other way
    // here: the throw came before the app existed, or from inside Run, whose app's destructor has already shut the pool down; and what
    // it drains first (Wait) is nothing, for every Dispatch in the constructor's path is followed by its Wait with no throw between
    // them (engine/src/core/TerrainGenerator.cpp:35-83, ParticleSystem.cpp:75-93) and nothing calls Execute. A throw out of it is
    // caught: the exit code still is.
    try {
        Supersonic::JobSystem::Shutdown();
    } catch (...) {
    }
    return EXIT_FAILURE;
}

// What the log's first line says of the machine, after the build and the engine (planning step 203, PORT1-4 (e)). Windows: the code page narrow
// strings mean in this process, 65001 where the application manifest (game/MagicPortals.manifest) took effect, because a path with a character this
// page lacks cannot be narrowed at all; the words are the very ones the line has always had there. Android: the phone, in the engine's one line
// (a log a player sends says what it ran on). Elsewhere paths are UTF-8 bytes and there is nothing to say: the line used to claim a "Windows code page"
// on every platform.
std::string PlatformFacts() {
#if defined(_WIN32)
    const unsigned page = GetACP();
    return ", Windows code page " + std::to_string(page) + (page == CP_UTF8 ? " (UTF-8)" : "");
#elif defined(__ANDROID__)
    return ", " + Supersonic::Android::DeviceSummary();
#else
    return {};
#endif
}

#ifdef NDEBUG
constexpr const char* kBuildType = "Release";
#else
constexpr const char* kBuildType = "Debug";
#endif

} // namespace

// The game. main below wraps it in the one try that every uncaught exception ends at.
static int RunGame(int argc, char** argv, FatalContext& fatal) {
    // --level, --levels, --art, --data and --saves are the game's own flags. LaunchOptions
    // refuses anything it does not know, which is right for the engine, so the
    // game takes its flags out first and hands on the rest untouched.
    // Its fields hold what this build was configured with until the asset root is resolved below (sim/AssetRoot.hpp): all of them are
    // replaced there, from --root or a folder next to the program or the user's data folder or, last, from these.
    MagicPortals::MagicPortalsLayer::Paths paths;
    // No --level opens the menu, which is the game's own front door. A named
    // level is entered directly, and that is how the suites and every headless
    // render run: neither ever sees the menu.
    std::string start;
    std::vector<char*> engineArgs{argv[0]};
    // The asset root (planning step 200): --root and --no-baked-paths, and a flag for each path that replaces the root's answer for
    // that one path. Resolved after --help, once every flag is read, in the order sim/AssetRoot.hpp states.
    std::string rootFlag;
    bool noBakedPaths = false;
    MagicPortals::AssetRoot::Overrides pathFlags;
    // DEV ONLY: where the medals are kept, instead of the user's own directory.
    // It exists so a parity capture can open a level with a medal recorded - the
    // original shows its current-score plaque only then - from a scores.json
    // written for the purpose, without touching the save of whoever plays.
    std::string savesOverride;
#if MAGICPORTALS_DEV_TOOLS
    // DEV ONLY: walk levels in one process and check the engine's material
    // descriptor sets go back to its pool (LevelVisit.hpp). Empty = a normal run.
    MagicPortals::LevelVisitLayer::Options visit;
    // DEV ONLY: a press of the pause control, or of one of the pause's buttons, on
    // a stated tick. A --fixed-step run takes no input at all, and a parity
    // capture of the pause has to open one - and mute its sound, and resume it -
    // at a known moment. `--press pause@120` presses the pause control on the
    // layer's 120th tick.
    using DevPress = MagicPortals::MagicPortalsLayer::DevPress;
    std::vector<std::pair<int, DevPress>> presses;
    // DEV ONLY: a walk held over a stated span of ticks, as a held arrow would
    // hold it. The finished and lost screens' captures need the player walked
    // into a door or a hazard, which a --fixed-step run cannot do.
    // `--hold right@1-400` holds right from the layer's 1st tick to its 400th.
    struct Hold {
        int from = 0;
        int to = 0;
        float direction = 0.0f;
    };
    std::vector<Hold> holds;
    // DEV ONLY: a touch pressed on a stated tick and released on the next. The
    // popups' captures need a help block tapped, and a popup closed by a touch
    // down on its card, at a known moment. `--tap help@300` taps the level's first
    // help block where the camera has it; `--tap 640,200@300` taps that pixel of a
    // 1280x720 window (the spec's own coordinates), whatever the window's size.
    // `--tap 640,475@300-315` holds it from tick 300 and releases it on 315: the
    // menus' captures need a button held, to see its press tint, then released.
    // In play it goes where a real touch goes (MagicPortalsLayer::readInput): a
    // HUD control, a help block, or the portal shot - a torch lit by a capture. The
    // shot leaves on the RELEASE, as the original's does (placement.json `release`):
    // `--tap 247,141@60` goes down on tick 60, comes up on 61, and fires on 61.
    // Taps go one at a time - the next goes down after the last has come up - except under `--controls modern` with two contacts on
    // (controls.json input.two_contacts, `contacts=` in the flag), where they overlap, each a finger of its own (planning step 191).
    struct Tap {
        int tick = 0;
        int releaseTick = 0;
        std::optional<glm::dvec2> fraction;
    };
    std::vector<Tap> taps;
    // DEV ONLY: a touch that MOVES. The credits' strip and the achievements
    // dashboard scroll with a finger, and a --fixed-step run has none.
    // `--drag 640,500:640,300@300-330` goes down at that pixel of 1280x720 on tick
    // 300, moves at an even pace, and is let go at the second on tick 330.
    struct Drag {
        int tick = 0;
        int releaseTick = 0;
        glm::dvec2 from{0.0};
        glm::dvec2 to{0.0};
    };
    std::vector<Drag> drags;
    // DEV ONLY: 4-32's dark dragon burned on a stated tick, as a barrel's blast in
    // its reach burns it: its wound's flash and its white death are captures no
    // scripted input reaches (MagicPortalsLayer::ScheduleDevBurn).
    std::vector<int> burns;
    // DEV ONLY: the player put at a point of the level on a stated tick
    // (MagicPortalsLayer::ScheduleDevPlace), to film what a door opens: never a route.
    std::vector<std::pair<int, glm::dvec2>> places;
    // DEV ONLY: no 2D light reaches any sprite (MagicPortalsLayer::ForceLightMasksOff),
    // for a capture to be compared with the same capture lit.
    bool lightMasksOff = false;
    // DEV ONLY: a win route (Route.hpp, tests/routes/) replayed in the window, through
    // the same schedule --tap and --hold use, to watch what test_mp_routes asserts.
    // `--route C:/.../tests/routes/level8.json` opens its level unless --level names it.
    std::string routeFile;
    // DEV ONLY: with --route, print Route::Trace every n ticks, on each tap and at the
    // end - test_mp_routes --trace's lines, from the window (planning step 114).
    int routeTrace = 0;
#endif // MAGICPORTALS_DEV_TOOLS
    // `--controls <classic|modern>[,key=value...]`, as given: which on-screen controls the run plays
    // (docs/planning/2026-10-15-modern-controls.md 2.1, 2.2). Taken out here, before the engine's parser,
    // which refuses what it does not know; resolved below, once the rig's own flags are all read.
    std::optional<std::string> controlsFlag;
    // `--padart <absolute dir>` (planning step 184): the folder a modern run looks for Magic Rampage's five pictures in,
    // instead of the one the build was told. An empty folder is the way to see the next tiers (the original's own corner
    // sprites, then the drawn glyphs).
    std::string padArtDir = MAGICPORTALS_PADART_DIR;
    bool padArtGiven = false;
    // DEV ONLY (planning step 190): the engine's optional performance features and the layer's sprite report, over game/data/engine.json for ONE
    // run, so an A/B is one exe with one setting changed. Taken out here, before the engine's parser, which refuses what it does not know.
    MagicPortals::EngineTuning::Overrides tuningFlags;
    // `--profile <desktop|mobile>` (planning step 201): which engine settings the run asks for. Not given, the build's own (BuildProfile.hpp).
    std::optional<bool> profileMobile;
#if MAGICPORTALS_DEV_TOOLS
    std::set<std::string> tuningGiven;
#endif
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
#if MAGICPORTALS_DEV_TOOLS
        if (arg == "--light-masks-off") {
            lightMasksOff = true;
            continue;
        }
        if (arg == "--shadow-maps" || arg == "--sprites-only" || arg == "--asset-watching" || arg == "--sprite-report" ||
            arg == "--perf-log" || arg == "--scene-samples" || arg == "--dynamic-res" || arg == "--perf-overlay") {
            if (i + 1 >= argc) {
                std::cerr << arg << " needs a value\n";
                return EXIT_FAILURE;
            }
            const std::string value = argv[++i];
            // Twice is a mistake, not an override, as it is for --controls: a measurement that meant one setting and ran the other says
            // nothing about either.
            if (!tuningGiven.insert(arg).second) {
                std::cerr << arg << " is given twice; it takes one value\n";
                return EXIT_FAILURE;
            }
            bool ok = false;
            const char* takes = "on or off";
            bool onOff = false;
            std::uint32_t pixels = 0;
            std::uint32_t samples = 0;
            float seconds = 0.0f;
            if (arg == "--shadow-maps") {
                takes = "0 (the engine's own sizes) or a whole number of pixels from 16 to 4096";
                ok = MagicPortals::EngineTuning::ParseShadowMaps(value, pixels);
                if (ok) tuningFlags.shadowMapResolution = pixels;
            } else if (arg == "--perf-log") {
                takes = "a number of seconds above 0 and at most 3600";
                ok = MagicPortals::EngineTuning::ParsePerfLog(value, seconds);
                if (ok) tuningFlags.perfLogSeconds = seconds;
            } else if (arg == "--scene-samples") {
                takes = "1, 2 or 4 (4 is the engine's own)";
                ok = MagicPortals::EngineTuning::ParseSceneSamples(value, samples);
                if (ok) tuningFlags.sceneSamples = samples;
            } else {
                ok = MagicPortals::EngineTuning::ParseSwitch(value, onOff);
                if (ok && arg == "--sprites-only") tuningFlags.spritesOnlyScenePipelines = onOff;
                if (ok && arg == "--asset-watching") tuningFlags.assetWatching = onOff;
                if (ok && arg == "--sprite-report") tuningFlags.spriteReport = onOff;
                if (ok && arg == "--dynamic-res") tuningFlags.dynamicResolution = onOff;
                if (ok && arg == "--perf-overlay") tuningFlags.perfOverlay = onOff;
            }
            if (!ok) {
                std::cerr << arg << " takes " << takes << ", got '" << value << "'\n";
                return EXIT_FAILURE;
            }
            continue;
        }
#endif // MAGICPORTALS_DEV_TOOLS
        if (arg == "--padart") {
            if (i + 1 >= argc) {
                std::cerr << "--padart needs an absolute directory\n";
                return EXIT_FAILURE;
            }
            // Absolute, because the game makes engine/ its working directory before it opens anything: a relative path
            // would be looked for there and found nowhere, and the run would quietly draw the next tier.
            const std::string value = argv[++i];
            if (value.empty() || !std::filesystem::path(value).is_absolute()) {
                std::cerr << "--padart needs an absolute directory, got '" << value << "'\n";
                return EXIT_FAILURE;
            }
            // Twice is a mistake, not an override, as it is for --controls: a capture that meant one folder and ran the other
            // would say nothing about either.
            if (padArtGiven) {
                std::cerr << "--padart is given twice; it takes one absolute directory\n";
                return EXIT_FAILURE;
            }
            padArtGiven = true;
            padArtDir = value;
            continue;
        }
        if (arg == "--controls") {
            if (i + 1 >= argc) {
                std::cerr << "--controls needs a value; " << MagicPortals::Controls::FlagUsage() << "\n";
                return EXIT_FAILURE;
            }
            // Twice is a mistake, not an override: a capture that meant one style and ran the other
            // would say nothing about either.
            if (controlsFlag) {
                std::cerr << "--controls is given twice; " << MagicPortals::Controls::FlagUsage() << "\n";
                return EXIT_FAILURE;
            }
            controlsFlag = std::string(argv[++i]);
            continue;
        }
        if (arg == "--profile") {
            if (i + 1 >= argc) {
                std::cerr << "--profile needs a value: desktop or mobile\n";
                return EXIT_FAILURE;
            }
            const std::string value = argv[++i];
            bool mobile = false;
            if (!MagicPortals::EngineTuning::ParseProfile(value, mobile)) {
                std::cerr << "--profile takes desktop or mobile, got '" << value << "'\n";
                return EXIT_FAILURE;
            }
            // Twice is a mistake, not an override, as it is for --controls.
            if (profileMobile) {
                std::cerr << "--profile is given twice; it takes one value\n";
                return EXIT_FAILURE;
            }
            profileMobile = mobile;
            continue;
        }
#if MAGICPORTALS_DEV_TOOLS
        if (arg == "--route") {
            if (i + 1 >= argc) {
                std::cerr << "--route needs a route file, e.g. tests/routes/level8.json given absolute\n";
                return EXIT_FAILURE;
            }
            routeFile = argv[++i];
            continue;
        }
        if (arg == "--route-trace") {
            const std::string value = i + 1 < argc ? argv[++i] : "";
            try {
                routeTrace = std::stoi(value);
            } catch (const std::exception&) {
                routeTrace = 0;
            }
            if (routeTrace <= 0) {
                std::cerr << "--route-trace needs a whole number of ticks above 0, e.g. --route-trace 10\n";
                return EXIT_FAILURE;
            }
            continue;
        }
        if (arg == "--drag") {
            const std::string value = i + 1 < argc ? argv[++i] : "";
            Drag drag;
            bool ok = false;
            try {
                const std::size_t at = value.find('@');
                const std::size_t colon = value.find(':');
                const std::size_t dash = at == std::string::npos ? std::string::npos : value.find('-', at);
                if (at != std::string::npos && colon != std::string::npos && colon < at && dash != std::string::npos) {
                    const auto point = [](const std::string& text, glm::dvec2& out) {
                        const std::size_t comma = text.find(',');
                        if (comma == std::string::npos) return false;
                        out = glm::dvec2(std::stod(text.substr(0, comma)) / 1280.0,
                                         std::stod(text.substr(comma + 1)) / 720.0);
                        return true;
                    };
                    drag.tick = std::stoi(value.substr(at + 1, dash - at - 1));
                    drag.releaseTick = std::stoi(value.substr(dash + 1));
                    ok = point(value.substr(0, colon), drag.from) &&
                         point(value.substr(colon + 1, at - colon - 1), drag.to);
                }
            } catch (const std::exception&) {
                ok = false;
            }
            if (!ok || drag.tick < 1 || drag.releaseTick <= drag.tick) {
                std::cerr << "--drag wants <x>,<y>:<x>,<y>@<tick>-<release tick> (pixels of 1280x720, tick at least "
                             "1, a release after it), got '"
                          << value << "'\n";
                return EXIT_FAILURE;
            }
            drags.push_back(drag);
            continue;
        }
        if (arg == "--tap") {
            const std::string value = i + 1 < argc ? argv[++i] : "";
            const std::size_t at = value.find('@');
            const std::string what = value.substr(0, at);
            Tap tap;
            bool ok = at != std::string::npos;
            try {
                if (ok) {
                    const std::string when = value.substr(at + 1);
                    const std::size_t dash = when.find('-');
                    tap.tick = std::stoi(when.substr(0, dash));
                    if (dash != std::string::npos) tap.releaseTick = std::stoi(when.substr(dash + 1));
                }
                if (ok && what != "help") {
                    const std::size_t comma = what.find(',');
                    ok = comma != std::string::npos;
                    if (ok) {
                        tap.fraction = glm::dvec2(std::stod(what.substr(0, comma)) / 1280.0,
                                                  std::stod(what.substr(comma + 1)) / 720.0);
                    }
                }
            } catch (const std::exception&) {
                ok = false;
            }
            if (!ok || tap.tick < 1 || (tap.releaseTick != 0 && tap.releaseTick <= tap.tick)) {
                std::cerr << "--tap wants help@<tick> or <x>,<y>@<tick>[-<release tick>] (pixels of 1280x720, tick "
                             "at least 1, a release after it), got '"
                          << value << "'\n";
                return EXIT_FAILURE;
            }
            taps.push_back(tap);
            continue;
        }
        if (arg == "--hold") {
            const std::string value = i + 1 < argc ? argv[++i] : "";
            const std::size_t at = value.find('@');
            const std::size_t dash = at == std::string::npos ? std::string::npos : value.find('-', at);
            const std::string what = value.substr(0, at);
            Hold hold;
            hold.direction = what == "left" ? -1.0f : what == "right" ? 1.0f : 0.0f;
            if (dash != std::string::npos) {
                try {
                    hold.from = std::stoi(value.substr(at + 1, dash - at - 1));
                    hold.to = std::stoi(value.substr(dash + 1));
                } catch (const std::exception&) {
                    hold.from = 0;
                }
            }
            if (hold.direction == 0.0f || hold.from < 1 || hold.to < hold.from) {
                std::cerr << "--hold wants <left|right>@<first tick>-<last tick>, from at least 1, got '" << value
                          << "'\n";
                return EXIT_FAILURE;
            }
            holds.push_back(hold);
            continue;
        }
        if (arg == "--burn-dragon") {
            const std::string value = i + 1 < argc ? argv[++i] : "";
            int tick = 0;
            try {
                std::size_t used = 0;
                tick = std::stoi(value, &used);
                if (used != value.size()) tick = 0;
            } catch (const std::exception&) {
                tick = 0;
            }
            if (tick < 1) {
                std::cerr << "--burn-dragon wants the game's tick to burn it on, at least 1, got '" << value << "'\n";
                return EXIT_FAILURE;
            }
            burns.push_back(tick);
            continue;
        }
        if (arg == "--place") {
            // <x>,<y>@<tick>: the level's own pixels, the layer's tick.
            const std::string value = i + 1 < argc ? argv[++i] : "";
            const std::size_t comma = value.find(',');
            const std::size_t at = value.find('@');
            bool ok = comma != std::string::npos && at != std::string::npos && comma < at;
            double x = 0.0;
            double y = 0.0;
            int tick = 0;
            if (ok) {
                try {
                    std::size_t used = 0;
                    const std::string xs = value.substr(0, comma);
                    const std::string ys = value.substr(comma + 1, at - comma - 1);
                    const std::string ts = value.substr(at + 1);
                    x = std::stod(xs, &used);
                    ok = used == xs.size();
                    y = std::stod(ys, &used);
                    ok = ok && used == ys.size();
                    tick = std::stoi(ts, &used);
                    ok = ok && used == ts.size();
                } catch (const std::exception&) {
                    ok = false;
                }
            }
            if (!ok || tick < 1) {
                std::cerr << "--place wants <x>,<y>@<tick> (the level's pixels, tick at least 1), got '" << value
                          << "'\n";
                return EXIT_FAILURE;
            }
            places.emplace_back(tick, glm::dvec2(x, y));
            continue;
        }
        if (arg == "--press") {
            if (i + 1 >= argc) {
                std::cerr << "--press needs a value, e.g. pause@120\n";
                return EXIT_FAILURE;
            }
            const std::string value = argv[++i];
            const std::size_t at = value.find('@');
            const std::string what = value.substr(0, at);
            const struct {
                const char* name;
                DevPress press;
            } names[] = {{"pause", DevPress::Pause},
                         {"levels", DevPress::Levels},
                         {"resume", DevPress::Resume},
                         {"skip", DevPress::Skip},
                         {"achievements", DevPress::Achievements},
                         {"sound", DevPress::Sound},
                         {"music", DevPress::Music},
                         {"back", DevPress::Back}};
            int tick = 0;
            if (at != std::string::npos) {
                try {
                    tick = std::stoi(value.substr(at + 1));
                } catch (const std::exception&) {
                    tick = 0;
                }
            }
            bool known = false;
            for (const auto& name : names) {
                if (what != name.name) continue;
                known = true;
                if (tick >= 1) presses.emplace_back(tick, name.press);
            }
            if (!known || tick < 1) {
                std::cerr << "--press wants <pause|levels|resume|skip|achievements|sound|music|back>@<tick of at least "
                             "1>,"
                             " got '"
                          << value << "'\n";
                return EXIT_FAILURE;
            }
            continue;
        }
        if (arg == "--visit-levels" || arg == "--visit-passes") {
            if (i + 1 >= argc) {
                std::cerr << arg << " needs a value\n";
                return EXIT_FAILURE;
            }
            const std::string value = argv[++i];
            if (arg == "--visit-levels") {
                visit.levels = value;
            } else {
                try {
                    visit.passes = std::stoi(value);
                } catch (const std::exception&) {
                    visit.passes = 0;
                }
                if (visit.passes < 1) {
                    std::cerr << "--visit-passes wants a whole number of at least 1, got '" << value << "'\n";
                    return EXIT_FAILURE;
                }
            }
            continue;
        }
#endif // MAGICPORTALS_DEV_TOOLS
        if (arg == "--no-baked-paths") {
            // The compiled-in tier switched off for this run: how a release package is proven to hold everything it opens, and the
            // run-time twin of a build configured with MAGICPORTALS_BAKED_PATHS=OFF (both give the same answer from Resolve).
            noBakedPaths = true;
            continue;
        }
        if (arg == "--level" || arg == "--levels" || arg == "--art" || arg == "--data" || arg == "--saves" || arg == "--root" ||
            arg == "--original" || arg == "--chapters" || arg == "--achievements" || arg == "--port-data") {
            if (i + 1 >= argc) {
                std::cerr << arg << " needs a value\n";
                return EXIT_FAILURE;
            }
            const std::string value = argv[++i];
            if (arg == "--level") {
                start = value;
            } else if (arg == "--root") {
                // Twice is a mistake, not an override, as it is for --padart and --controls.
                if (!rootFlag.empty()) {
                    std::cerr << "--root is given twice; it takes one absolute folder\n";
                    return EXIT_FAILURE;
                }
                rootFlag = value;
            } else if (arg == "--levels") {
                pathFlags.levels = value;
            } else if (arg == "--art") {
                pathFlags.art = value;
            } else if (arg == "--saves") {
                savesOverride = value;
            } else if (arg == "--original") {
                pathFlags.original = value;
            } else if (arg == "--chapters") {
                pathFlags.chapters = value;
            } else if (arg == "--achievements") {
                pathFlags.achievements = value;
            } else if (arg == "--port-data") {
                pathFlags.portData = value;
            } else {
                pathFlags.data = value;
            }
            continue;
        }
#if !MAGICPORTALS_DEV_TOOLS
        // A developer's flag in a build made without the developer tools (MAGICPORTALS_DEV_TOOLS=OFF): refused HERE, by name and with the reason,
        // where the engine's parser below would say only that it does not know the option.
        if (MagicPortals::StartChecks::IsDeveloperFlag(arg)) {
            std::cerr << MagicPortals::StartChecks::DeveloperFlagRefusal(arg) << "\n";
            return EXIT_FAILURE;
        }
#endif
        // The engine's own flag, handed on: the layer is told too, because a run whose frames
        // are the tick's own measures nothing of the display and draws every picture at its
        // tick's place (pictures.json, planning step 150).
        if (arg == "--fixed-step") paths.fixedStep = true;
        engineArgs.push_back(argv[i]);
    }
#if MAGICPORTALS_DEV_TOOLS
    // A route names its own level, and opens it unless --level says the same.
    std::optional<MagicPortals::Route::Plan> route;
    if (!routeFile.empty()) {
        MagicPortals::Route::Plan plan;
        std::string error;
        if (!MagicPortals::Route::Load(routeFile, plan, error)) {
            std::cerr << "--route: " << error << "\n";
            return EXIT_FAILURE;
        }
        if (start.empty()) start = plan.level;
        if (start != plan.level) {
            std::cerr << "--route plays " << plan.level << ", and --level asks for " << start << "\n";
            return EXIT_FAILURE;
        }
        route = std::move(plan);
    }
#endif // MAGICPORTALS_DEV_TOOLS
    // The two developer inputs the lines below ask about: what the flags said when the build has them, and no in a build that does not.
#if MAGICPORTALS_DEV_TOOLS
    const bool routeGiven = !routeFile.empty();
    const bool visitGiven = !visit.levels.empty();
#else
    const bool routeGiven = false;
    const bool visitGiven = false;
#endif
    const auto options = Supersonic::LaunchOptions::Parse(static_cast<int>(engineArgs.size()), engineArgs.data());
    if (options.helpRequested) {
        std::cout << Supersonic::LaunchOptions::Usage()
                  << "  --level <name>    the level to start at, as chapters.json names it. Without it\n"
                  << "                    the game opens its menu.\n"
                  << "  --root <dir>      the folder that holds the game's data, an absolute path: laid out like this\n"
                  << "                    repository (out/levels, data, game/data, reference/...) or like a released\n"
                  << "                    package (levels, tables, data, port-data, original, assets, pad-art; see\n"
                  << "                    docs/packaging.md). Without it the data is looked for in a 'content' folder\n"
                  << "                    next to the program, then the program's own folder, then 'content' in the\n"
                  << "                    user's data folder for Magic Portals, and last in the paths this build was\n"
                  << "                    configured with. A --root that does not hold the game ends the run.\n"
                  << "  --no-baked-paths  ignore the paths this build was configured with: the data must then come from\n"
                  << "                    --root or one of the folders above. What a release build does always\n"
                  << "  --profile <desktop|mobile>\n"
                  << "                    which engine settings the run asks for (the 'mobile' block of\n"
                  << "                    game/data/engine.json is the phone's, among them the lowest Vulkan\n"
                  << "                    version a GPU may report). Without it the build's own: mobile in a\n"
                  << "                    phone's build, desktop in any other; with it a desktop run can try\n"
                  << "                    the phone's settings, and the other way round\n"
                  << "  --levels <dir>    the converted levels (" << DefaultText(paths.levels) << ")\n"
                  << "  --chapters <file> chapters.json, the level order (" << DefaultText(paths.chapters) << ")\n"
                  << "  --achievements <file>\n"
                  << "                    achievements.json (" << DefaultText(paths.achievements) << ")\n"
                  << "  --original <dir>  the original's extracted assets (" << DefaultText(paths.original) << ")\n"
                  << "  --port-data <dir> the port's own tables, game/data (" << DefaultText(paths.portData) << ")\n"
                  << "  " << MagicPortals::Controls::FlagUsage() << "\n"
                  << "                    which on-screen controls the run plays: the original's (classic)\n"
                  << "                    or Magic Rampage's inset ones (modern, built step by step:\n"
                  << "                    docs/planning/2026-10-15-modern-controls.md). Modern lays out,\n"
                  << "                    draws and claims the five inset controls (pictures: Magic Rampage's\n"
                  << "                    own where --padart's folder holds them, then the original's own\n"
                  << "                    corner sprites, then ones the game draws). Its keys: preset= and\n"
                  << "                    arrows= (arrows=grouped puts both walk arrows at the bottom\n"
                  << "                    left, as the first layout did; the default is one arrow in each\n"
                  << "                    bottom corner) choose the look, and are never kept; size= (0.4 to\n"
                  << "                    1.4 by 0.1), opacity= (0.2 to 1.8 by 0.2) and a move as\n"
                  << "                    walk_left=dx:dy (design units from the control's own corner, in\n"
                  << "                    screen direction, never scaled by size) tune it: they REPLACE the\n"
                  << "                    tuning a player kept, never merge with it, and a value off its\n"
                  << "                    grid is refused.\n"
                  << MagicPortals::StartChecks::ControlsRunKindsHelp(MagicPortals::kDevTools)
                  << "  --padart <dir>    the folder of Magic Rampage's five control pictures a modern run\n"
                  << "                    tries first, an absolute path (" << DefaultText(padArtDir) << ");\n"
                  << "                    the adjustment screen's eight are in the folder `editor` beside it\n"
                  << "  --art <dir>       what their res:// stands for (default the directory above the levels)\n"
                  << "  --data <dir>      Enhanced's data directory (" << DefaultText(paths.data) << ")\n"
                  << "                    Each of --levels, --art, --data, --chapters, --achievements, --original,\n"
                  << "                    --port-data and --padart replaces that one path over the root's answer.\n"
                  << "  --saves <dir>     DEV: keep the medals (scores.json), the sound and music\n"
                  << "                    switches (audio.json), the unlocked achievements\n"
                  << "                    (achievements.json) and the controls chosen in Options\n"
                  << "                    (controls.json) here instead of the user's data\n"
                  << "                    directory - for a capture that needs a medal recorded, that\n"
                  << "                    presses a switch or that unlocks an achievement, without\n"
                  << "                    changing the player's own save. A named --level's start unlocks\n"
                  << "                    silently what the saved medals have earned, as the main menu does.\n"
#if MAGICPORTALS_DEV_TOOLS
                  << "  --press <what>@<tick>\n"
                  << "                    DEV: press the pause control (pause), one of the pause's\n"
                  << "                    buttons (levels, resume, skip, achievements, sound, music) or\n"
                  << "                    the back key on a menu state (back) on the game's tick <tick>,\n"
                  << "                    for a capture; repeatable\n"
                  << "  --tap <help|x,y>@<tick>[-<release>]\n"
                  << "                    DEV: a touch down on the game's tick <tick> and up on the next\n"
                  << "                    (or held there until <release>), on the level's first help block\n"
                  << "                    or at pixel x,y of 1280x720, for a capture that opens or closes a\n"
                  << "                    popup, presses a menu's button or, in play, does what a touch\n"
                  << "                    there does: a HUD control, a help block or a portal shot, which\n"
                  << "                    leaves on the release tick; repeatable. Taps go one at a time,\n"
                  << "                    except under --controls modern with two contacts on, where they\n"
                  << "                    overlap, each a finger (one held on an arrow, another shooting)\n"
                  << "  --drag <x,y>:<x,y>@<tick>-<release>\n"
                  << "                    DEV: a touch down at the first pixel of 1280x720 on <tick>,\n"
                  << "                    moved evenly to the second and let go there on <release>, for a\n"
                  << "                    capture that scrolls the credits or the achievements (in play,\n"
                  << "                    its release fires no shot once it has moved over scale(18));\n"
                  << "                    repeatable\n"
                  << "  --hold <left|right>@<from>-<to>\n"
                  << "                    DEV: hold a walk from the game's tick <from> to <to>, for a\n"
                  << "                    capture that walks into a door or a hazard; repeatable. Give\n"
                  << "                    --saves <absolute dir> too: a run that reaches a door records\n"
                  << "                    its medal on that tick, in the player's own save without it\n"
                  << "  --route <file>    DEV: replay a win route (tests/routes/<level>.json, given\n"
                  << "                    absolute) in the window: its walks and its taps at points of the\n"
                  << "                    level, down and up on the ticks it names (a shot on the up);\n"
                  << "                    says on exit whether it won. Give\n"
                  << "                    --saves <absolute dir> too: a route that wins records its medal\n"
                  << "                    on its door's tick, in the player's own save without it\n"
                  << "  --route-trace <n> DEV: with --route, print the route's trace every n ticks, on\n"
                  << "                    each tap and at the end (test_mp_routes --trace's lines)\n"
                  << "  --burn-dragon <tick>\n"
                  << "                    DEV: burn 4-32's dark dragon on the game's tick <tick>, as a\n"
                  << "                    barrel's blast in its reach does; repeatable\n"
                  << "  --place <x>,<y>@<tick>\n"
                  << "                    DEV: put the player at the level's pixel (x, y) on the game's\n"
                  << "                    tick <tick>, to film what a door opens; never a route; repeatable\n"
                  << "  --visit-levels <lightmapped|all|name,...>\n"
                  << "                    DEV: visit these levels in one process, taking the material set\n"
                  << "                    of every lightmapped sprite, which the level drops on leaving; the\n"
                  << "                    run fails unless the engine's descriptor sets return to its pool\n"
                  << "  --visit-passes <n> DEV: how many times to walk that list (default 1)\n"
                  << "  --light-masks-off DEV: draw the lights and halos but let no light reach a sprite,\n"
                  << "                    for a capture to be compared with the same capture lit\n"
                  << "  --shadow-maps <0|16..4096>\n"
                  << "                    DEV: the shadow maps' size in pixels for this run (0: the engine's\n"
                  << "                    own 2048 and 1024); over game/data/engine.json, for an A/B\n"
                  << "  --sprites-only <on|off>\n"
                  << "                    DEV: build the scene pipelines for unlit draws only (the sprites,\n"
                  << "                    particles and halos); the lit boxes of the B key then draw flat\n"
                  << "  --asset-watching <on|off>\n"
                  << "                    DEV: the engine's stat of every watched file on every frame (hot\n"
                  << "                    reload of a texture a developer saved); off in a player's run\n"
                  << "  --sprite-report <on|off>\n"
                  << "                    DEV: the layer's per-frame walk over its sprites that logs one\n"
                  << "                    crossing the view's edge; on as shipped (engine.json turns it\n"
                  << "                    off for a run), and G dumps all whatever this says\n"
                  << "  --perf-log <seconds>\n"
                  << "                    DEV: the engine's frame statistics (fps, median, 95th percentile,\n"
                  << "                    worst, the split of the frame) in the log every <seconds>\n"
                  << "  --scene-samples <1|2|4>\n"
                  << "                    DEV: the most samples per pixel of the scene target for this run (4,\n"
                  << "                    the default, is the engine's own); what a smaller cap does to the\n"
                  << "                    pictures is the owner's to judge (docs/android.md)\n"
                  << "  --dynamic-res <on|off>\n"
                  << "                    DEV: the scene target sized by how fast the GPU draws it, with the\n"
                  << "                    numbers of the 'mobile' block of game/data/engine.json; the engine\n"
                  << "                    never applies it to a capture or a --fixed-step run\n"
                  << "  --perf-overlay <on|off>\n"
                  << "                    DEV: the engine's readout of the frame rate and where the frame's\n"
                  << "                    time went, over the picture\n"
#endif // MAGICPORTALS_DEV_TOOLS
                  ;
        return EXIT_SUCCESS;
    }
    if (!options.ok) {
        std::cerr << options.error << "\n";
        return EXIT_FAILURE;
    }

    // WHICH KIND OF RUN THIS IS, before anything can fail: only a player's may put a message box up (RunKind.hpp).
    {
        MagicPortals::DevFlags dev;
        dev.saves = !savesOverride.empty();
#if MAGICPORTALS_DEV_TOOLS
        dev.route = !routeFile.empty();
        dev.visit = !visit.levels.empty();
        dev.scripted = !presses.empty() || !holds.empty() || !taps.empty() || !drags.empty() || !burns.empty() || !places.empty();
        dev.lightMasksOff = lightMasksOff;
#endif
        fatal.attended = MagicPortals::AttendedRun(options, dev);
    }

    // WHERE THE GAME'S DATA IS (planning step 200, PORT-2), resolved once and handed down, in the order sim/AssetRoot.hpp states. After
    // --help, which needs no data, and before anything is opened or made: a --root that does not hold the game ends the run here with a
    // message that names it, and nothing has been created (the log, the scratch folder and the engine's window all come after).
    namespace AssetRoot = MagicPortals::AssetRoot;
    AssetRoot::Resolved assetRoot;
    {
        AssetRoot::Inputs inputs;
        inputs.flagRoot = rootFlag;
        inputs.exeDir = Supersonic::ExecutableDirectory().string();
        // Asked for only when --root, the folder next to the program and the bundle have all failed to answer: the engine's
        // UserDataDirectory MAKES the player's data folder when it answers, and a run that has its data elsewhere has no business making
        // it. Nothing here makes <that folder>/content, which is only tested for.
        inputs.userDataContent = [] {
            const std::filesystem::path base = Supersonic::UserDataDirectory(kTitle);
            return base.empty() ? std::string() : (base / "content").string();
        };
        if (!noBakedPaths) {
            // What the build was configured with, still in `paths` from its defaults. Empty in a build configured with
            // MAGICPORTALS_BAKED_PATHS=OFF, which has nothing to offer here.
            inputs.compiledIn.levels = paths.levels;
            inputs.compiledIn.art = paths.art;
            inputs.compiledIn.chapters = paths.chapters;
            inputs.compiledIn.achievements = paths.achievements;
            inputs.compiledIn.data = paths.data;
            inputs.compiledIn.portData = paths.portData;
            inputs.compiledIn.original = paths.original;
            inputs.compiledIn.padArt = MAGICPORTALS_PADART_DIR;
        }
        // The only question put to the file system, and it only looks.
        assetRoot = AssetRoot::Resolve(inputs, AssetRoot::HoldsGameOnDisk);
        if (!assetRoot.ok) {
            std::cerr << "[Magic Portals] " << assetRoot.error << "\n";
            // A player's run gets the box a failed start gets (RunKind.hpp); the text names no log, for none is open yet.
            const std::string text = MagicPortals::FatalText(assetRoot.error, "", fatal.attended);
            if (!text.empty()) ShowFatalDialog(text);
            return EXIT_FAILURE;
        }
        const AssetRoot::Paths resolved = AssetRoot::Apply(assetRoot.paths, pathFlags);
        paths.levels = resolved.levels;
        paths.art = resolved.art;
        paths.chapters = resolved.chapters;
        paths.achievements = resolved.achievements;
        paths.data = resolved.data;
        paths.portData = resolved.portData;
        paths.original = resolved.original;
        if (!padArtGiven) padArtDir = resolved.padArt;
    }

    // WHICH CONTROLS THIS RUN PLAYS (planning step 181), resolved once and handed down: the flag and the kind
    // of run (the rig's three flags make it a developer's, whose controls are the classic ones whatever the
    // data says). After --help, which needs no valid flag, and before anything is opened, so a refused
    // flag costs nothing. The layer plays whatever this resolves (planning step 182): classic where the
    // feature does not exist in the run or the table will not read.
    {
        MagicPortals::Controls::Policy policy = MagicPortals::Controls::ResolvePolicy(
            controlsFlag, paths.fixedStep, routeGiven, visitGiven);
        if (!policy.valid) {
            std::cerr << policy.error << "\n";
            return EXIT_FAILURE;
        }
        paths.controls = std::move(policy);
        // After the move, which replaces the whole struct: the command line has no word for the folder in
        // ResolvePolicy, so it is set here. Classic never reads it.
        paths.controls.artDir = padArtDir;
        paths.controls.editorArtDir = EditorArtDirBeside(padArtDir);
        // A size or an opacity must sit on the grids controls.json names (planning step 187, CTL-10): a capture that asked for size=1.45 would
        // otherwise play the base look and say so only in a log, so the flag is refused here, with the usage, like any other bad item. A table
        // that will not read is not this check's business: the layer says why modern is unavailable, and a classic run never meets it.
        if (paths.controls.available && paths.controls.flag.Tunes()) {
            MagicPortals::Controls::Rules rules;
            MagicPortals::Controls::Tuning tuning;
            std::string why;
            if (MagicPortals::Controls::LoadRules(paths.portData + MagicPortals::Controls::kDataFile, rules, why) &&
                !MagicPortals::Controls::Tuning::FromFlag(paths.controls.flag, rules, tuning, why)) {
                std::cerr << "--controls: " << why << "; " << MagicPortals::Controls::FlagUsage() << "\n";
                return EXIT_FAILURE;
            }
        }
    }

    // WHERE THE LOG AND THE PRISMS GO (planning step 200; the log's own folder was this block's, a step earlier). sim/Scratch.hpp has the
    // rule and the two failures it answers (an Android temporary folder an app cannot write, a shared /tmp whose prism folder belongs to
    // another account): a candidate counts only if a file can be made in it. The system's temporary folder first, by the error_code
    // overload, which says "no" instead of throwing when %TEMP% names nothing (the throwing one ended the process with no word and no
    // log): on a desktop that is where this always was, so a Windows run writes %TEMP%\magicportals.log and %TEMP%\supersonic-magicportals
    // as before. Then the user's data folder's scratch folder, the one candidate that makes a folder, so it is asked for only if the first
    // failed; then the older last resorts. Empty when none will do: the game then runs with no log file and writes no prisms, and says so.
    //
    // A phone's build puts the user's data folder's scratch folder FIRST (Scratch::ArrangedFor, planning step 203): the NDK's libc++ names
    // /data/local/tmp as the temporary folder, which an app cannot write, and the log must be at one path the manifest can name for the system's
    // report of a run that ended unexpectedly. A desktop keeps the order below, so a Windows run still writes %TEMP%\magicportals.log.
    std::vector<MagicPortals::Scratch::Candidate> scratchCandidates = {
        {"the system's temporary folder",
         [] {
             std::error_code ec;
             std::filesystem::path folder = std::filesystem::temp_directory_path(ec);
             if (ec) folder.clear();
             return folder;
         },
         false},
        {"the user's data folder, scratch",
         [] {
             return MagicPortals::Scratch::PhoneLogFolder(Supersonic::UserDataDirectory(kTitle));
         },
         true, MagicPortals::Scratch::Kind::UserDataScratch},
        {"the user's data folder", [] { return Supersonic::UserDataDirectory(kTitle); }, true},
        {"the executable's folder", [] { return Supersonic::ExecutableDirectory(); }, false},
        {"the working directory",
         [] {
             std::error_code ec;
             std::filesystem::path folder = std::filesystem::current_path(ec);
             if (ec) folder.clear();
             return folder;
         },
         false}};
    const MagicPortals::Scratch::Chosen scratch =
        MagicPortals::Scratch::Choose(MagicPortals::Scratch::ArrangedFor(std::move(scratchCandidates), MagicPortals::kMobileBuild), MagicPortals::Scratch::ProbeWritable);
    const std::filesystem::path temp = scratch.ok ? scratch.base : std::filesystem::path();
    if (scratch.ok) paths.prisms = scratch.prisms;

    // The diagnostics to a FILE as well as the console.
    //
    // Nothing else in the engine opens this sink, and Log.hpp says why it
    // matters: a shipped game gets WIN32_EXECUTABLE and has no console at all,
    // so without this its diagnostics go nowhere. That is no use at all when
    // the thing being chased only happens while somebody else is playing.
    if (temp.empty()) {
        std::cerr << "[Magic Portals] no folder to keep a log in; continuing without one (" << scratch.note << ").\n";
    } else {
        // The log and the run before's, from the one definition the Android entry's failed-start record uses too (Scratch::LogPathsIn, planning step 205).
        const MagicPortals::Scratch::LogPaths logs = MagicPortals::Scratch::LogPathsIn(temp);
        const std::filesystem::path logPath = logs.log;
        const std::string logFile = logPath.string();
        // THE LAST RUN'S LOG, KEPT. SetFileSink opens its file truncated
        // (engine/src/core/Log.cpp:90), so a crash's log was gone the moment the
        // game was started again to see whether it happens twice. One run back is
        // kept beside it; a failure here costs that copy, never the start.
        KeepPreviousLog(logPath, logs.previous);
        if (Supersonic::Log::SetFileSink(logFile)) {
            // Only now: the box names the log it was told was written, and a sink that failed to open (a read-only file, a full disk)
            // wrote none (planning step 190).
            // A phone's log is in the app's private storage, which no player can open: its box names none (the report the system shows at the
            // next start prints the end of this very file, supersonic.reportLog in the manifest).
            if (!MagicPortals::kMobileBuild) fatal.logPath = logFile;
            std::cout << "[Magic Portals] logging to " << logFile << std::endl;
            // The FILE's lines carry the seconds since the first (the console's do not): a log a player sends can then say a slow
            // start from a stuck one. A capture compares pictures and never this file.
            Supersonic::Log::SetElapsedTimestamps(true);
            // What the next reader of this file needs first, and nothing that changes from one run of the same build to the next:
            // no clock, no process id.
            SUPERSONIC_LOG_INFO("Magic Portals") << "build: " << kBuildType << (MagicPortals::kMobileBuild ? " (mobile build)" : "") << ", engine "
                                                 << MAGICPORTALS_ENGINE_COMMIT << PlatformFacts() << std::endl;
        } else {
            std::cerr << "[Magic Portals] could not open " << logFile << " for writing; continuing without a log file.\n";
        }
    }
    // The root and the tier that answered, and the scratch folder, next: the first things a report from another person's machine needs. (A start
    // that found NO root never gets here: it ends above, before any log is open, with a message that names every place it looked, on stderr and
    // in the box of a player's run; planning step 203, PORT2-3 (d).)
    SUPERSONIC_LOG_INFO("Magic Portals") << assetRoot.why << std::endl;
    SUPERSONIC_LOG_INFO("Magic Portals") << scratch.note << std::endl;

    // WHAT THE RUN ASKS OF THE ENGINE'S OPTIONAL FEATURES (planning step 190): game/data/engine.json for this kind of run, then the flags over
    // it. A table that will not read leaves the engine as it was (Table{} is that), and the log says so below.
    MagicPortals::EngineTuning::Table tuningTable;
    std::string tuningWhy;
    const bool tuningRead = MagicPortals::EngineTuning::Load(paths.portData + "/engine.json", tuningTable, tuningWhy);
    // The phone profile (game/data/engine.json "mobile"): the build's own unless --profile says the other. A desktop build never applies the block
    // unless asked, so every desktop value is the one it was before the block existed.
    const bool mobile = profileMobile.value_or(MagicPortals::kMobileBuild);
#if MAGICPORTALS_PERF_DIAG
    // A PERFORMANCE-DIAGNOSTIC build asks a PLAYER'S run (a tester's) for the engine's readout and a statistics line every few seconds, unless a flag said otherwise, and for
    // nothing else: a capture or a route of such a build must not carry the readout in its picture. Compiled out of every other build: code that is not there, not a branch.
    if (fatal.attended) MagicPortals::EngineTuning::ApplyPerfDiag(tuningFlags);
#endif
    const MagicPortals::EngineTuning::Resolved tuning =
        MagicPortals::EngineTuning::Resolve(tuningTable, fatal.attended, mobile, tuningFlags);
    paths.spriteReport = tuning.spriteReport;
    // A phone's run covers its level starts (planning step 205, sim/LevelStart.hpp); every other run starts a level inside the tick that asked, as it always did.
    paths.coverLevelStarts = mobile;
    // A phone's run cannot do without its options file (planning step 203, PORT3-3 (d)): the lowest Vulkan version a phone's GPU may report is in it,
    // and the engine's own defaults ask for 1.2, which many phones' stock drivers do not offer. A start with the defaults would end with a GPU
    // complaint that blames the phone; this one says what is wrong. A desktop run with the file unreadable goes on with the engine's defaults, as
    // it always has (the warning below says so).
    if (mobile && !tuningRead) {
        throw std::runtime_error("the engine options in " + paths.portData + "/engine.json could not be read (" + tuningWhy +
                                 "), and a phone's run needs them: its lowest Vulkan version is in them. The game's files are damaged.");
    }

    // Declared, not discovered, for the reason Wolf Brigade's main gives: a
    // game binary is a game whether or not a manifest sits beside it.
    Supersonic::GameManifest manifest;
    manifest.isGame = true;
    manifest.title = kTitle;
    // The engine's opt-ins, each off by default there (engine/src/core/GameRuntime.hpp). Every one is set from the table above and nowhere else.
    manifest.shadowMapResolution = tuning.shadowMapResolution;
    manifest.spritesOnlyScenePipelines = tuning.spritesOnlyScenePipelines;
    manifest.assetWatching = tuning.assetWatching;
    manifest.perfLogSeconds = tuning.perfLogSeconds;
    manifest.perfOverlay = tuning.perfOverlay;
    // The scene target's sample cap and dynamic resolution (planning step 205): asked for only where the table's phone block or a flag asks, so that every run that asks
    // for neither leaves the manifest as it was. The phone block ships with neither on: the owner's decision.
    if (tuning.sceneSamples != 0) manifest.sceneSamples = tuning.sceneSamples;
    if (tuning.dynamicResolution.enabled) {
        manifest.dynamicResolution.enabled = true;
        manifest.dynamicResolution.startMaxPixels = tuning.dynamicResolution.startMaxPixels;
        manifest.dynamicResolution.minScale = tuning.dynamicResolution.minScale;
        manifest.dynamicResolution.targetFps = tuning.dynamicResolution.targetFps;
        manifest.dynamicResolution.relaxedRise = tuning.dynamicResolution.relaxedRise;
        manifest.dynamicResolution.minTargetHeight = tuning.dynamicResolution.minTargetHeight;
        manifest.dynamicResolution.emergencyBelowFps = tuning.dynamicResolution.emergencyBelowFps;
    }
    // Asked only where the profile asks (a phone's): left alone, the engine wants Vulkan 1.2 as it always has, and a phone whose stock driver
    // reports 1.1 would have no GPU the engine accepts and end its start before the first frame (sibling port, E39).
    if (tuning.minimumVulkanMinor != 0) manifest.minimumVulkanMinor = tuning.minimumVulkanMinor;
    // The window is shown the moment it exists, and nothing reads its messages until the first frame: a start that takes five seconds
    // or more (a cold disk, an antivirus scanning a freshly copied exe, a slow GPU driver compiling its pipelines) makes Windows call
    // it "Not responding" and offer to close it, which looks like a crash. This reads the window's events between the startup stages,
    // so the longest it goes unread is the longest single stage. Desktop only, and it changes no picture.
    manifest.pumpEventsDuringStartup = true;
    // No startup scene: the layer builds the level. Left at the manifest's
    // default, a run from the repository root opens the editor's demo scene
    // beside it, which the first headless render showed above level30.
    manifest.startupScene.clear();

    // WHERE THIS GAME MAY WRITE, resolved once, here, and handed down.
    //
    // The layer does not ask for itself, which is WolfBrigadeLayer's contract
    // and exists for its reason: "where may I write" is a question about the
    // machine rather than about this game, and a layer that resolved its own
    // path could not be built by a test without one. Eighteen suites build this
    // layer bare, and every one of them would otherwise be writing the same
    // scores.json into the ctest working directory and reading each other's.
    //
    // Empty when the platform will not say: the medals are then not kept, which
    // the game says out loud rather than writing somewhere unpredictable.
    const std::filesystem::path saveDir =
        savesOverride.empty() ? Supersonic::UserDataDirectory(manifest.title) : std::filesystem::path(savesOverride);
    if (saveDir.empty()) {
        std::cerr << "[Magic Portals] no writable user directory; "
                     "medals earned this session will not be kept.\n";
    }
    paths.saveDir = saveDir.string();
    SUPERSONIC_LOG_INFO("Magic Portals") << "folders: levels " << paths.levels << ", data " << paths.data << ", port data "
                                         << paths.portData << ", saves " << (paths.saveDir.empty() ? "(none)" : paths.saveDir)
                                         << std::endl;
    SUPERSONIC_LOG_INFO("Magic Portals") << "folders, the rest: art " << paths.art << ", chapters " << paths.chapters << ", original "
                                         << paths.original << ", achievements " << paths.achievements << ", pad art "
                                         << (padArtDir.empty() ? "(none)" : padArtDir) << std::endl;
    if (!tuningRead) {
        SUPERSONIC_LOG_WARN("Magic Portals") << "engine options not read, the engine's own defaults are used: " << tuningWhy << std::endl;
    }
    // The Vulkan minimum is read back from the MANIFEST, what the engine will be handed, and not from the table: a profile that was resolved and
    // never assigned would show here as 1.2.
    SUPERSONIC_LOG_INFO("Magic Portals") << "engine options (" << (fatal.attended ? "a player's run" : "not a player's run")
                                         << (mobile ? ", mobile profile" : "") << "): " << MagicPortals::EngineTuning::Describe(tuning)
                                         << ", minimum Vulkan 1." << Supersonic::GameRuntime::ResolveVulkanMinor(manifest) << ", scene samples "
                                         << Supersonic::GameRuntime::ResolveSceneSamples(manifest) << std::endl;
#if MAGICPORTALS_PERF_DIAG
    SUPERSONIC_LOG_INFO("Magic Portals") << "PERFORMANCE-DIAGNOSTIC BUILD: a player's run of it has the engine's frame readout over the picture and its statistics in this log every "
                                         << MagicPortals::EngineTuning::kPerfDiagLogSeconds << " s (this run is " << (fatal.attended ? "a player's" : "not a player's: it has them only if a flag asked") << "); nothing else differs from the shipped game" << std::endl;
#endif

    // THE VULKAN LOADER, before the engine calls Vulkan once (planning step 201). A Windows release build links vulkan-1.dll late
    // (MAGICPORTALS_WIN_GUI), so a PC without a graphics driver gets here instead of a loader dialog that names a DLL and nothing more. Thrown, so
    // that it takes the road every other failed start takes: the log, stderr, ONE message box for a person's run and none for a script's, exit 1.
    // Every Windows build asks it; where the loader is linked at process start the answer is yes, because the program is running.
    {
        const MagicPortals::StartChecks::Verdict loader =
            MagicPortals::StartChecks::CheckVulkanLoader(TryLoadLibrary, Supersonic::GameRuntime::ResolveVulkanMinor(manifest));
        if (!loader.ok) throw std::runtime_error(loader.what);
    }

#if MAGICPORTALS_DEV_TOOLS
    // Outside the app, because the layer stack is torn down with it.
    MagicPortals::LevelVisitLayer::Result visitResult;
#endif
    // What the layer and the app say about the run, read before the app goes: the layer's attach has long since run, and the app
    // owns the count of captures it wrote.
    bool startFailed = false;
    std::string missingCaptures;
    {
        Supersonic::SupersonicApp app(options, &manifest);

        // SAID OUT LOUD, because the two cases are indistinguishable otherwise.
        //
        // A run with the validation layers absent produces exactly the output a
        // clean run does: none. This game was played through a Release build,
        // where SUPERSONIC_ENABLE_VALIDATION defaults to 0, and its silent log
        // was nearly read as evidence that the frames were clean - when in fact
        // nothing had been watching. VulkanContext.hpp makes the same point
        // about CI: "a green run that never loaded the layer proves nothing and
        // looks identical to one that did".
        SUPERSONIC_LOG_INFO("Magic Portals")
            << "Vulkan validation layers: "
            << (Supersonic::VulkanContext::ValidationLayersActive() ? "ACTIVE"
                                                                    : "NOT LOADED - nothing is checking this run")
            << std::endl;

        auto game = std::make_unique<MagicPortals::MagicPortalsLayer>(paths, start);
#if MAGICPORTALS_DEV_TOOLS
        for (const auto& [tick, press] : presses) game->ScheduleDevPress(tick, press);
        for (const Hold& hold : holds) game->ScheduleDevHold(hold.from, hold.to, hold.direction);
        for (const int burn : burns) game->ScheduleDevBurn(burn);
        for (const auto& [tick, atPx] : places) game->ScheduleDevPlace(tick, atPx);
        for (const Tap& tap : taps) game->ScheduleDevTap(tap.tick, tap.fraction, tap.releaseTick);
        for (const Drag& drag : drags) game->ScheduleDevDrag(drag.tick, drag.from, drag.to, drag.releaseTick);
#endif
        MagicPortals::MagicPortalsLayer& gameLayer = *game;
#if MAGICPORTALS_DEV_TOOLS
        gameLayer.ForceLightMasksOff(lightMasksOff);
#endif
        app.PushLayer(std::move(game));
#if MAGICPORTALS_DEV_TOOLS
        // After the game's, so each of its ticks follows the game's own (Route.hpp).
        if (route) {
            app.PushLayer(std::make_unique<MagicPortals::Route::ReplayLayer>(gameLayer, std::move(*route), routeTrace));
        }
        if (!visit.levels.empty()) {
            app.PushLayer(std::make_unique<MagicPortals::LevelVisitLayer>(gameLayer, paths, visit, visitResult));
        }
#endif
        app.Run();
        startFailed = gameLayer.StartFailed();
        // Like the engine's own main (engine/src/main.cpp): every way a capture fails only logs, and a script that finds last week's
        // PNG at the path it named would take it for this run's.
        missingCaptures = options.MissingCaptures(app.CapturesWritten());
    }

    // Every reason the run failed is said, and any one of them fails it.
    bool failed = false;
    if (startFailed) {
        // The window stayed open on a black frame with the reason as a label (the layer's updateHud); a launcher could not tell that
        // from a run that played.
        std::cerr << "[Magic Portals] the game could not start; the reason is above and in the log." << std::endl;
        failed = true;
    }
#if MAGICPORTALS_DEV_TOOLS
    if (!visit.levels.empty() && (!visitResult.finished || !visitResult.failures.empty())) {
        std::cerr << "[Magic Portals] level visit "
                  << (visitResult.finished ? "failed" : "did not finish") << ": "
                  << visitResult.failures.size() << " failure(s)";
        if (!visitResult.failures.empty()) std::cerr << ", the first: " << visitResult.failures.front();
        std::cerr << std::endl;
        failed = true;
    }
#endif
    if (!missingCaptures.empty()) {
        std::cerr << "[Magic Portals] " << missingCaptures << std::endl;
        failed = true;
    }
    const unsigned validationErrors = Supersonic::VulkanContext::ValidationErrorCount();
    if (validationErrors > 0) {
        std::cerr << "[Magic Portals] " << validationErrors << " Vulkan validation error(s); failing the run."
                  << std::endl;
        failed = true;
    }

    // The last line main writes itself, after the app is gone: its absence from a log is how a run that ended by a crash is told from
    // one that ended on purpose (the engine's own "Workers joined" is no proof, for it is logged while an exception unwinds too).
    SUPERSONIC_LOG_INFO("Magic Portals") << "finished: " << (failed ? "failed" : "clean") << std::endl;
    Supersonic::Log::CloseFileSink();
    return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}

// The whole of the game under ONE try. A throw from anywhere in it - an argument, a path that cannot be narrowed, the engine's start,
// the layer - ends here with the reason in the log, and an exit code that says it failed; not as a process the system kills with no
// word (the exit 0xC0000409 of a profile folder whose name the ANSI code page lacks, planning step 189).
//
// A function of its own since planning step 203 (MagicPortalsMain.hpp): a desktop's main() below calls it, and so does Android's SupersonicMain
// (game/android/AndroidMain.cpp), which has no main() of its own to keep (the engine owns the process's real entry there).
int MagicPortalsMain(int argc, char** argv) {
    FatalContext fatal;
    try {
        return RunGame(argc, argv, fatal);
    } catch (const std::exception& e) {
        return ReportFatal(e.what(), fatal);
    } catch (...) {
        return ReportFatal("an exception that is not a std::exception was thrown", fatal);
    }
}

#if !defined(__ANDROID__)
int main(int argc, char** argv) { return MagicPortalsMain(argc, argv); }
#endif
