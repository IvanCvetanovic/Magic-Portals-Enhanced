#pragma once

// The original's script clock: the whole milliseconds a frame gives the scripts.
//
// The Android loop measures a frame as a float of milliseconds and ETHEngine::Update
// hands it on twice: truncated to an unsigned long for the scripts
// (SetLastFrameElapsedTime, ETHEngine.cpp:147), and as the float itself to the C++
// side (ETHScene::Update, :166) - the physics step, and the entities' own updates
// with their particle systems. A script reads the integer, through
// STimeManager::getLastFrameElapsedTime (bytes 7894..7961), unitsPerSecond (8045..8162)
// or sys:GetLastFrameElapsedTime directly. At a steady 60 Hz that is 16 ms of a 16.667
// ms frame: every scripted motion, timer and blink the original runs advances 4.17 %
// less than the wall clock says (planning step 147; clock.json has the decode).
//
// THREE CLOCKS, and this is the one place the port says which is which:
//   script   what a tick hands the sim as `dt`: ScriptClock::Step::scriptS, exact
//            whole milliseconds over a thousand. Every scripted motion, timer, blink
//            and fade reads this, and so will the player's decoded movement (G10)
//            and the shot's decoded flight (G4), whose original reads are
//            getLastFrameElapsedTime and scaledUnitsPerSecond too.
//   physics  what the engine integrates (Step::physicsS): the engine's own tick,
//            untouched. A body whose velocity the port sets for the solver to carry
//            (MoveKinematic), and the port's own stand-ins for the solver's
//            gravity and friction, are measured against it.
//   wall     the layer's clock, which advances by the engine's tick: what sys:GetTime
//            and sys:GetTimeF read (the earthquake generator, the portal's 400 ms
//            kill, the choosers' blink). Not here.

#include <string>

namespace MagicPortals::ScriptClock {

// The values below are clock.json's own, repeated only so that a default-built Rules (the
// pure suites' `ScriptClock::Rules{}`) is the 60 Hz floor without a file; LoadRules
// replaces every one of them, and the layer never runs on the defaults.
struct Rules {
    int matchedHz = 60;      // the frame rate the original is matched at
    double snapMs = 0.001;   // a frame this close to the tick's length is the tick
    // floor(1000 / matchedHz): the whole milliseconds a frame at that rate gives.
    unsigned frameMs = 16;
    // 1000 / matchedHz: the length of that frame, unfloored.
    double wallFrameMs = 1000.0 / 60.0;
};

// Reads clock.json. A rate that rounds to under one millisecond a frame is refused:
// the script would never advance.
bool LoadRules(const std::string& path, Rules& out, std::string& error);

// The uint STimeManager's reads return for a frame of `frameMs`: the truncation
// __aeabi_f2uiz does (a negative or NaN frame is 0).
unsigned FloorMs(double frameMs);

// One tick's two deltas, which the layer hands the sim. A bare float is a tick whose
// script and physics are one length - what the suites' own `kStep` has always meant,
// so they keep stepping the rules at the length they were written for; only the
// layer, which owns the engine's tick, builds the two apart.
struct Step {
    double scriptS = 1.0 / 60.0; // exact whole ms / 1000, never 0.016f
    float physicsS = 1.0f / 60.0f;

    Step() = default;
    // NOLINTNEXTLINE(google-explicit-constructor): see above.
    Step(float tickS) : scriptS(static_cast<double>(tickS)), physicsS(tickS) {}
    Step(double script, float physics) : scriptS(script), physicsS(physics) {}
};

// The step a tick of the engine's `physicsS` gives the sim under `rules`.
Step TickStep(const Rules& rules, float physicsS);

// The script's milliseconds of a step, exact (a whole number for a layer step).
inline double Ms(const Step& step) { return step.scriptS * 1000.0; }
// What the engine's tick is in wall milliseconds: the layer clock's step, and what the
// wall-clock readers (sys:GetTime, sys:GetTimeF) see pass.
inline double WallMs(const Step& step) { return static_cast<double>(step.physicsS) * 1000.0; }

// The script's milliseconds of one RENDERED frame of `frameMs` of wall time, for the
// per-frame paths (the effects the tick queued and the frame adds). A frame the
// length of the tick is the floor itself, exactly; any other is scaled by
// floor / tick, so a frame that held two ticks gives two floors and a display faster
// than the tick gives smaller shares of one.
double OfFrameMs(const Rules& rules, double frameMs, double tickMs);

} // namespace MagicPortals::ScriptClock
