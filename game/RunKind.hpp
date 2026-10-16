#pragma once

// What kind of run this is, from its command line: the one question main asks before it may open a window of its own.
//
// WHY IT IS ITS OWN FILE (planning step 189). When the game stops on an error it writes the reason to its log, and for a
// PLAYER it also puts the reason on the screen with the log's path in it: a game with no console, double-clicked, that dies
// without a word looks exactly like one that never started. But a message box waits for a click, and a capture, a route, a
// level walk or any script that launches the exe has nobody to give it, so for those the run must end on its own with a
// non-zero exit code and the reason in the log and on stderr. Which runs are which is a rule, and a rule that can block an
// unattended run is a rule a test has to be able to reach: main.cpp cannot be linked into a suite, so the rule is here, over
// the engine's parsed options and the port's own developer flags.

#include "core/LaunchOptions.hpp"

namespace MagicPortals {

// The developer's flags of THIS port, which main takes out of the command line before the engine's parser sees it (the engine's
// own are in LaunchOptions).
struct DevFlags {
    bool saves = false;         // --saves: the medals somewhere else than the player's own
    bool route = false;         // --route (and --route-trace)
    bool visit = false;         // --visit-levels
    bool scripted = false;      // --press, --hold, --tap, --drag, --burn-dragon, --place: input a player does not give
    bool lightMasksOff = false; // --light-masks-off
};

// Whether a person is at this run: a window the game opened for someone to play. False for every run a script makes, and the
// rule is deliberately the narrow one: anything that is not plainly a player's run is unattended, so the only way to block an
// unattended run is for a developer flag to be missing from this list.
//
// It is also the port's ONE definition of "a player-style run" for everything else that must differ for a person (planning step 190):
// the engine's per-frame watch of every file for a developer's hot reload, and the layer's per-frame sprite log, are off for an attended
// run and stay on for every other (sim/EngineTuning, game/data/engine.json). The measurement flags of that step (--shadow-maps,
// --sprites-only, --asset-watching, --sprite-report, --perf-log) are NOT in DevFlags: they change what a run costs, not who is at it.
inline bool AttendedRun(const Supersonic::LaunchOptions& engine, const DevFlags& dev) {
    const bool engineUnattended = engine.maxFrames > 0 || engine.fixedDelta > 0.0f || engine.hidden ||
                                  !engine.screenshotPath.empty() || !engine.screenshotUiPath.empty() ||
                                  engine.screenshotEvery > 0 || !engine.recordPath.empty() ||
                                  !engine.replayPath.empty() || engine.importAssets;
    const bool portUnattended = dev.saves || dev.route || dev.visit || dev.scripted || dev.lightMasksOff;
    return !engineUnattended && !portUnattended;
}

} // namespace MagicPortals
