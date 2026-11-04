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
//            and fade reads this, and so does the player's decoded movement (G10,
//            planning step 148) and will the shot's decoded flight (G4), whose
//            original reads are getLastFrameElapsedTime and scaledUnitsPerSecond too.
//   physics  what the engine integrates (Step::physicsS): the engine's own tick,
//            untouched. A body whose velocity the port sets for the solver to carry
//            (MoveKinematic), and the port's own stand-ins for the solver's
//            gravity and friction, are measured against it.
//   sim      what the ORIGINAL's physics world advances in a frame (Step::simS): a
//            fixed 1/30 s, whatever the frame (AverageFPSRateManager, clock.json),
//            twice the 60 Hz tick. The port's solver steps the tick, so EVERY dynamic
//            body is carried at SimRatio() times the original's velocity (and the world's
//            gravity at its square) to cover the distance the original's step covers in
//            the same frame: the player from planning step 148, every other body from
//            step 152 (CarriedVelocity and CarriedAcceleration, below).
//   wall     the layer's clock, which advances by the engine's tick: what sys:GetTime
//            and sys:GetTimeF read (the earthquake generator, the portal's 400 ms
//            kill, the choosers' blink). Not here.

#include <algorithm>
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
    // What the original's physics world advances in ONE frame, in seconds, whatever the frame's
    // length: AverageFPSRateManager's m_timeStep, the float 0.0333333 (1023969417), handed to
    // SetFixedTimeStepValue every update. The same number clock.json carries.
    double physicsStepS = 0.03333333507180214;
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
//
// `simS` is what the original's physics world advances in the frame this tick stands for, and
// `fps` what GetFPSRate answers (the player's air share no longer reads it: since planning step 213 the one rate the
// air brake is evaluated at is player.json's movement.air_decay_rate_hz). A bare float, and the two-length form, are a tick whose
// physics is one length with the tick's (simS = physicsS: SimRatio 1) at the frame rate the
// tick implies: the pure suites' reading, which TickStep replaces with the decode's.
struct Step {
    double scriptS = 1.0 / 60.0; // exact whole ms / 1000, never 0.016f
    float physicsS = 1.0f / 60.0f;
    double simS = 1.0 / 60.0;
    double fps = 60.0;

    Step() = default;
    // NOLINTNEXTLINE(google-explicit-constructor): see above.
    Step(float tickS)
        : scriptS(static_cast<double>(tickS)), physicsS(tickS), simS(static_cast<double>(tickS)),
          fps(1.0 / static_cast<double>(tickS)) {}
    Step(double script, float physics)
        : scriptS(script), physicsS(physics), simS(static_cast<double>(physics)),
          fps(1.0 / static_cast<double>(physics)) {}
    Step(double script, float physics, double sim, double framesPerSecond)
        : scriptS(script), physicsS(physics), simS(sim), fps(framesPerSecond) {}
};

// The step a tick of the engine's `physicsS` gives the sim under `rules`.
Step TickStep(const Rules& rules, float physicsS);

// The script's milliseconds of a step, exact (a whole number for a layer step).
inline double Ms(const Step& step) { return step.scriptS * 1000.0; }
// How many seconds of the original's physics the engine's tick stands for: 2 at the decode's
// 60 Hz (a fixed 1/30 s a frame over a 1/60 s tick). A body the port carries in the original's
// own Box2D units moves at this times the original's velocity, and is accelerated by its
// acceleration times simS times this, to cover each tick the distance the original's step covers.
inline double SimRatio(const Step& step) {
    return step.physicsS > 0.0f ? step.simS / static_cast<double>(step.physicsS) : 1.0;
}
// THE CARRIED FRAME (planning step 152, WT). One tick of the port is one frame of the original, and
// the original's physics world advances simS in each, where the engine's solver advances physicsS. So
// EVERY dynamic body is carried at SimRatio times the original's velocity and SimRatio squared times its
// acceleration, and these three are the only places the factor is named: the world's gravity
// (Game::UseWorldGravity), every velocity the sim writes (a walk, a pull, a recoil) and every velocity
// it reads back into the original's own units (a hit's intensity, a trail, a push's aim). A quantity the
// original applies once a FRAME - a pull's velocity, a brake's multiplier, a gravity well's lap - is
// the same number a tick, and is not scaled by either.
//
// The velocity the original's physics holds for a body that moves `originalMps` metres of ITS second a
// second, as the port's solver carries it (metres of the engine's second a second).
inline double CarriedVelocity(double originalMps, const Step& step) { return originalMps * SimRatio(step); }
// And the other way: a velocity the port's body carries, in metres of the original's physics second.
inline double OriginalVelocity(double carriedMps, const Step& step) { return carriedMps / SimRatio(step); }
// An acceleration of the original's world (its gravity, 10 m/s^2 of its physics) as the engine's
// PhysicsSettings take it: SimRatio squared, because a distance goes as a half g t^2 of a clock that
// runs SimRatio times as fast.
inline double CarriedAcceleration(double originalMps2, const Step& step) {
    const double ratio = SimRatio(step);
    return originalMps2 * ratio * ratio;
}
// The one expression the original's scripts write a walking body's velocity with, in metres a second of
// its physics: scale(k) x min(cap, the script's whole milliseconds) / GetCurrentPhysicsTimeStepMS
// (SideScrollerCharacter::applyForces ins 15-36, ETHCallback_minion ins 1049-1068, and
// ETHCallback_dragon_knight). `scaleMetres` is the literal k handed to scale(), which is metres here (the
// port's metre is 50 units, as the original's is 50 of the screen's pixels at the 256-line design height).
inline double ScriptSpeed(double scaleMetres, double capMs, const Step& step) {
    const double stepMs = step.simS * 1000.0;
    return stepMs > 0.0 ? scaleMetres * std::min(capMs, Ms(step)) / stepMs : 0.0;
}

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
