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
#include <cmath>
#include <map>
#include <string>

namespace MagicPortals::ScriptClock {

// The values below are clock.json's own, repeated only so that a default-built Rules (the
// pure suites' `ScriptClock::Rules{}`) is the 60 Hz floor without a file; LoadRules
// replaces every one of them, and the layer never runs on the defaults.
struct Rules {
    int matchedHz = 60;      // the frame rate the original is matched at: the rate of the port's TICK
    double snapMs = 0.001;   // a frame this close to the tick's length is the tick
    // floor(1000 / matchedHz): the whole milliseconds a frame at that rate gives.
    unsigned frameMs = 16;
    // 1000 / matchedHz: the length of that frame, unfloored.
    double wallFrameMs = 1000.0 / 60.0;
    // What the original's physics world advances in ONE frame, in seconds, whatever the frame's
    // length: AverageFPSRateManager's m_timeStep, the float 0.0333333 (1023969417), handed to
    // SetFixedTimeStepValue every update. The same number clock.json carries.
    double physicsStepS = 0.03333333507180214;
    // THE PACE KNOB (clock.json original_frame_hz): the frame rate of the original that the world
    // stands for, in whole hertz. matchedHz stays the rate of the TICK (the script's floor and every
    // engine-facing contract read it); this is how many of the original's frames a second the
    // world is played at. At matchedHz it is the pace the port always had (one tick an original
    // frame); below it, an original frame is frameTicks ticks long.
    int originalFrameHz = 60;
    // How many ticks one original frame is: matchedHz / originalFrameHz, a whole number by
    // construction (SetOriginalFrameHz is the only thing that writes the pair). 1 is today's pace.
    int frameTicks = 1;
    // The file's own original_frame_hz, which every level without a line of its own in originalFrameHzByLevel plays at: what
    // ForLevel puts back. SetOriginalFrameHz does not write it (it is the pace the Rules was MADE at; only LoadRules sets it).
    int baseOriginalFrameHz = 60;
    // clock.json original_frame_hz_by_level (planning step 262): a level's name to the frame rate of the original its world is played at,
    // read by ForLevel. A level named here plays at its own pace and the rest at the file's; empty, as in the suites' bare Rules, is
    // one pace for every level.
    std::map<std::string, int> originalFrameHzByLevel;
    // Puts the pace of `level` (its file stem, as Game::LoadData reads it) into originalFrameHz and frameTicks: its own line of the table
    // or, with none, the file's own. False, changing nothing, for a pace that does not divide matchedHz (LoadRules refuses those, so a
    // loaded table never gives one).
    bool ForLevel(const std::string& level);
    // Sets originalFrameHz and frameTicks TOGETHER, so a Rules built in code cannot hold one
    // without the other. Valid is a whole number of hertz from 1 to matchedHz that divides it
    // (45, 0, 61 and 120 are not valid at 60: a frame must be a whole number of ticks, and
    // rounding 45 to 1 would be a pace nobody asked for). Returns false and changes nothing
    // otherwise. Call it after matchedHz is final: it reads it.
    bool SetOriginalFrameHz(int hz);
};

// Reads clock.json. A rate that rounds to under one millisecond a frame is refused:
// the script would never advance. original_frame_hz is optional: absent, it is matched_hz
// itself (frameTicks 1); present, it must be a whole number of hertz from 1 to matched_hz
// that divides it, or the file is refused, naming the key.
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
// `fps` what GetFPSRate answers (the player's air share reads it only when player.json has no movement.air_decay_rate_hz: since
// planning step 213 the one rate the air brake is evaluated at is that key, or the level's own in air_decay_rate_hz_by_level, and the shipped
// file has the key). A bare float, and the two-length form, are a tick whose
// physics is one length with the tick's (simS = physicsS: SimRatio 1) at the frame rate the
// tick implies: the pure suites' reading, which TickStep replaces with the decode's.
//
// `frameTicks` is how many ticks one frame of the original is (Rules::frameTicks) and `framePhase` which
// of them this tick is: 0 for the first tick of a frame, up to frameTicks - 1. A bare float and the
// two- and four-length forms are one tick a frame, phase 0 - exactly what the pure suites have always
// meant - so nothing that builds a Step by hand changes. Only the air brake (Player::AirShare, through frameTicks) and the translation clamp read them so far; the helpers
// below say how a once-a-frame quantity reads them.
struct Step {
    double scriptS = 1.0 / 60.0; // exact whole ms / 1000, never 0.016f
    float physicsS = 1.0f / 60.0f;
    double simS = 1.0 / 60.0;
    double fps = 60.0;
    int frameTicks = 1;
    int framePhase = 0;

    Step() = default;
    // NOLINTNEXTLINE(google-explicit-constructor): see above.
    Step(float tickS)
        : scriptS(static_cast<double>(tickS)), physicsS(tickS), simS(static_cast<double>(tickS)),
          fps(1.0 / static_cast<double>(tickS)) {}
    Step(double script, float physics)
        : scriptS(script), physicsS(physics), simS(static_cast<double>(physics)),
          fps(1.0 / static_cast<double>(physics)) {}
    // The two trailing parameters are defaulted, so every existing brace or positional call of four stays valid.
    Step(double script, float physics, double sim, double framesPerSecond, int ticksPerFrame = 1, int phaseInFrame = 0)
        : scriptS(script), physicsS(physics), simS(sim), fps(framesPerSecond), frameTicks(ticksPerFrame),
          framePhase(phaseInFrame) {}
};

// The step a tick of the engine's `physicsS` gives the sim under `rules`. `levelStep` is how many
// steps of the level the layer has made so far (MagicPortalsLayer::LevelStepCount: 0 for the level's
// first), which fixes the frame phase: levelStep % frameTicks. With frameTicks 1 (the pace before
// original_frame_hz existed) the phase is 0 and `simS`, `fps` and every other field are what they were.
// Under frameTicks 2 the tick advances half the original's physics step: simS = physicsStepS / frameTicks, an
// exact halving (it only moves the exponent), which for the float 1/30 clock.json carries is the float 1/60 the
// engine ticks at - a SimRatio of exactly 1.
Step TickStep(const Rules& rules, float physicsS, unsigned levelStep = 0);

// The script's milliseconds of a step, exact (a whole number for a layer step).
inline double Ms(const Step& step) { return step.scriptS * 1000.0; }
// How many seconds of the original's physics the engine's tick stands for: 2 at the decode's
// 60 Hz (a fixed 1/30 s a frame over a 1/60 s tick). A body the port carries in the original's
// own Box2D units moves at this times the original's velocity, and is accelerated by its
// acceleration times simS times this, to cover each tick the distance the original's step covers.
inline double SimRatio(const Step& step) {
    return step.physicsS > 0.0f ? step.simS / static_cast<double>(step.physicsS) : 1.0;
}
// THE CARRIED FRAME (planning step 152, WT). While frameTicks is 1 - the pace the port had until planning step 270, which
// clock.json's original_frame_hz of 30 has since changed for every level - one tick of the port is one frame of the original, and
// the original's physics world advances simS in each, where the engine's solver advances physicsS. So
// EVERY dynamic body is carried at SimRatio times the original's velocity and SimRatio squared times its
// acceleration, and these three are the only places the factor is named: the world's gravity
// (Game::UseWorldGravity), every velocity the sim writes (a walk, a pull, a recoil) and every velocity
// it reads back into the original's own units (a hit's intensity, a trail, a push's aim). A quantity the
// original applies once a FRAME - a pull's velocity, a brake's multiplier, a gravity well's lap - is,
// while frameTicks is 1, the same number a tick, and is not scaled by either. (At the shipped two ticks a frame a pull
// or a brake is a real-time quantity read through the script's own milliseconds, or one of the helpers below.) It stays so for every
// caller of this file today: the helpers after WallMs (FrameTravel, FrameRoot, FrameDelayTicks,
// OriginalStepCarriedS) are the named rules for a frame of more than one tick, each the identity at
// one; so far only the air brake (FrameTravel, FrameRoot) and the translation clamp (OriginalStepCarriedS) call them.
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

// ---- what the original does ONCE A FRAME, when a frame is more than one tick ---------------------
//
// Each helper takes the number of ticks a frame is (a Step's frameTicks) and is the identity at one
// tick a frame: FrameTravel and FrameRoot return their own input (an early return, no pow, no
// rounding), FrameDelayTicks the frames themselves rounded to whole ticks, OriginalStepCarriedS the
// engine's own tick - so the pace the port has always had reads bit for bit what it read. A tick
// count below one is read as one.
//
// FrameTravel (class V, distance-preserving): the share `share` of a free velocity the original
// multiplies in ONCE a frame, as a share applied EACH TICK so that the distance travelled after the
// key is released is the original's. Released at v units a frame and braked by s each frame, a body
// travels v s / (1 - s) in all (a geometric series); at v / n units a tick, braked by r each tick, it
// travels (v / n) r / (1 - r). The two are equal when r / (1 - r) = n s / (1 - s), that is
// r = n s / (1 + (n - 1) s). 0.43 at two ticks a frame is 0.601399: a release at 2.4 units a tick then
// glides 3.621 units, against the 3.734 the original's 4.95 units a frame gives (the 3 % is the walk's
// speed, 144 against 148.5 units a second, not this rule).
inline double FrameTravel(double share, int ticksPerFrame) {
    if (ticksPerFrame <= 1) return share;
    const double n = static_cast<double>(ticksPerFrame);
    return n * share / (1.0 + (n - 1.0) * share);
}
// FrameRoot (class R, decay-preserving): the n-th root of a share the original applies once a
// frame, so that n ticks of it multiply to the original's share at each frame boundary. For what has
// no travel at stake (a cosmetic fade).
inline double FrameRoot(double share, int ticksPerFrame) {
    if (ticksPerFrame <= 1) return share;
    return std::pow(share, 1.0 / static_cast<double>(ticksPerFrame));
}
// FrameDelayTicks (class D, a delay): the whole ticks `frames` original frames are, for a state the
// original reads that many frames old (the ground flag's lag). std::lround, half away from zero, so
// 2.25 frames at two ticks a frame (4.5 ticks) is 5 and 2.5 frames (5 ticks) is 5; Python's round()
// would say 4 for the first, and the suite pins the C++ one. Nothing for a frame count that is not
// above zero (0 frames is no delay, and a negative or NaN count is no delay either).
inline int FrameDelayTicks(double frames, const Step& step) {
    if (!(frames > 0.0)) return 0;
    const int ticks = step.frameTicks > 1 ? step.frameTicks : 1;
    return static_cast<int>(std::lround(frames * static_cast<double>(ticks)));
}
// OriginalStepCarriedS (class O, original step): the seconds of the engine's clock ONE STEP OF THE
// ORIGINAL's physics takes, in the unit the port's carried velocities are measured in: frameTicks
// times the engine's tick. What a rule that judges per ORIGINAL step reads (Box2D's translation
// clamp, the impact reader's replay of the solver). An integer times a double, so exact at every
// frameTicks; at one it is `static_cast<double>(step.physicsS)` bit for bit, whatever `simS` a
// hand-built Step carries (a Step(1.0 / 60.0, 1.0f / 60.0f, 1.0 / 30.0, 60.0) has a SimRatio just
// under 2 and still returns the float tick). For a Step TickStep built it also equals physicsStepS
// divided by SimRatio, to a relative 1e-12 (the division form is not bit-exact for a hand-built
// ratio, which is why this one is the product).
inline double OriginalStepCarriedS(const Step& step) {
    const int ticks = step.frameTicks > 1 ? step.frameTicks : 1;
    return static_cast<double>(ticks) * static_cast<double>(step.physicsS);
}

// The script's milliseconds of one RENDERED frame of `frameMs` of wall time, for the
// per-frame paths (the effects the tick queued and the frame adds). A frame the
// length of the tick is the floor itself, exactly; any other is scaled by
// floor / tick, so a frame that held two ticks gives two floors and a display faster
// than the tick gives smaller shares of one.
double OfFrameMs(const Rules& rules, double frameMs, double tickMs);

} // namespace MagicPortals::ScriptClock
