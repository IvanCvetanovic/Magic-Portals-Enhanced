#pragma once

// The player: a dynamic capsule, steered the way the original's SideScrollerCharacter
// steers its Box2D body (SideScrollerCharacter::applyForces, bytes 129676..130142), by the
// numbers in Enhanced's player.json - decoded, with their citations (planning step 148).
//
// WHAT THE ORIGINAL DOES, from its listing:
//   - GROUND: the velocity is SET OUTRIGHT every update, with no ramp and no friction of
//     its own: x = the pad's -1, 0 or 1 times speedScale, y left as the body has it. No
//     pad held is a velocity of zero, a dead stop.
//     speedScale = scale(speed) x min(66, elapsed) / GetCurrentPhysicsTimeStepMS, with
//     speed 3 (the constructor's SetV4 3f), elapsed the script's whole milliseconds (16 at
//     60 Hz) and the physics step the FIXED 33.33 ms (clock.json): 3 x 16 / 33.33 metres a
//     second of the original's physics, which that step carries 3 x 16 / 1000 metres a
//     frame - 2.4 units a tick, 144 units a second.
//   - AIR (isTouchingGround not set): the pad is ignored and x is the body's own x times
//     min(1, GetFPSRate() / 60 x 0.86): 0.86 a frame at 60 Hz, an exponential brake.
//   - GRAVITY is Box2D's: the world's scale(10) m/s^2 times the body's SetGravityScale(2),
//     with no cap but Box2D's own b2_maxTranslation, 2 m a step, which a level's height
//     never reaches and a hidden, dead body falling on does (Steer applies it).
//
// IN THE PORT'S UNITS. The original's physics advances 1/30 s a frame; the port's solver
// steps its 1/60 s tick (ScriptClock::SimRatio is 2). So the player's velocity is carried at
// SimRatio times the original's, and its gravity is applied as the velocity the original's
// step would add a frame, times that: both cover, each tick, what the original's step covers
// in the frame. Nothing else in the port is in that time base yet (a crate, a stone and a
// minion fall and slide at the port's real time: planning step 148, RECORDED DEVIATIONS),
// which is why a velocity another system hands the player is converted at that seam.
//
// Dynamic rather than kinematic because the spike measured F2: a kinematic body
// trips no trigger. Rotation frozen and locked to the plane (S1). Its capsule is the
// Godot project's guess (player.json body, marked _guess).
//
// Two places it cannot be the original line for line, both because the port's solver
// is not Box2D:
//   - Gravity is applied every tick, not only off the floor, as Box2D applies it. The
//     solver has no snap, and without gravity the player would hover over a lift going
//     down, since a contact only holds what presses into it. On the floor the contact
//     takes the gravity back out.
//   - It grips nothing: kFriction is 0. The original's body has friction 0.9
//     (dark_mage.ent), but its script sets the velocity every update; measured, a
//     gripping player fights its own steering in the port: at friction 1 the contact takes
//     back most of each tick's walk, and the notch where level30's first two platforms meet
//     holds it still for 26 ticks. test_mp_play walks that seam at both and prints them. What the original's friction
//     also does - an idle player stands still on a slope - is not a friction here, which the
//     engine's integrate-then-solve order cannot make hold one: Steer presses an idle player
//     into the ground along its normal instead (GroundSlope, Tuning::hold*; G10-5, planning
//     step 148's amendment).

#include "sim/ScriptClock.hpp"

#include <string>

#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace MagicPortals::Player {

inline constexpr float kFriction = 0.0f;

struct Tuning {
    double speed = 0.0;         // movement.speed: SideScrollerCharacter.speed, scale() metres a second
    double elapsedCapMs = 0.0;  // movement.elapsed_cap_ms: applyForces' min(66, the script's frame)
    double airDecay = 0.0;      // movement.air_decay: the x velocity kept a frame off the floor...
    double airDecayFps = 0.0;   // movement.air_decay_fps: ...at this GetFPSRate (the 60 of fps / 60)
    double worldGravity = 0.0;  // gravity.world_m_s2: the world's scale(10)
    double gravityScale = 0.0;  // gravity.scale: SetGravityScale(2)
    double maxTranslationM = 0.0;       // gravity.max_translation_m: b2_maxTranslation, Box2D metres a step
    double referencePxPerUnit = 0.0;    // gravity.reference_px_per_unit: the screen pixels a unit is at 720 lines
    double widthPx = 0.0;       // body.width_px
    double heightPx = 0.0;      // body.height_px
    // The ground sensor: char_feet.ent, a polygon the body carries below it, which counts what it
    // touches (feet.*): its size, the chamfer of its octagon's corners, and how near counts as touching.
    double feetWidthPx = 0.0;
    double feetHeightPx = 0.0;
    double feetChamfer = 0.0;
    double feetTouchPx = 0.0;
    // The hold of an idle player on a slope (hold.*): dark_mage.ent's friction 0.9, which a slope up to atan of
    // its mix with the floor's holds, and the two downward probes that read the slope.
    double holdFriction = 0.0;
    double holdProbeOffsetPx = 0.0;
    double holdProbeReachPx = 0.0;
    double holdProbeLineTolPx = 0.0;
};

// From player.json. False, with `error`, when a number is missing: player.gd falls
// back to literals, the port does not invent them.
bool LoadTuning(const std::string& path, Tuning& out, std::string& error);

entt::entity Spawn(entt::registry& registry, const glm::dvec2& atPx, const Tuning& tuning);

// isTouchingGround: the feet sensor touches something solid. The original's char_feet.ent is a sensor
// octagon 14 units wide and 10 tall (feet.*) centred on the bottom of the body (SideScrollerCharacter::
// addFeetOffset, ins 56-74: the collision box's pos + (0, size.y / 2)), its numTouches counted by the
// contact callbacks that ignore sensors, characters, fireballs and anti_sight_wall (shouldBeIgnoredByFeet,
// bytes 361584..361843); manageFeet sets the flag at the end of each update. Here the octagon is tested
// against every solid collider but the player's own (Impacts::Between), each tick, so it reads the state
// the last physics step left, as the flag does. Wider than the old one-ray test: a player walking off a
// ledge is on the ground until its feet are 7 units clear, which F13's walk off 1-06's block shows
// (planning step 148). A minion's body counts here, which the original's feet ignore (RECORDED DEVIATIONS).
bool Grounded(entt::registry& registry, entt::entity player, const Tuning& tuning);

// The slope of the ground under the body's centre line, in the port's pixels (+y down): the height
// the surface falls to the right per unit of x, positive where it descends to the right. Three probes
// that start inside the capsule and end below its bottom - on the centre line and `hold.probe_offset_px`
// either side of it - hit the same solid body in one line (to `hold.probe_line_tol_px`); the slope is
// the difference of the outer two over their span. False where one misses, they hit two bodies or they
// are not collinear - an edge, a notch, a chamfer's corner, a round top, nothing under the centre line.
bool GroundSlope(entt::registry& registry, entt::entity player, const Tuning& tuning, double& slope);

// The steepest slope an idle player holds on: the engine mixes friction by the square root, so
// sqrt(hold.friction x the port's floor friction (LevelBuilder::kStaticFriction)), the tangent of the
// angle a body with that friction does not slide down.
double HoldTan(const Tuning& tuning);

// The original's walk speed, in metres a second of ITS physics: speedScale of applyForces,
// speed x min(cap, the script's milliseconds) / the physics step's milliseconds.
double WalkOriginal(const Tuning& tuning, const ScriptClock::Step& step);

// The walk as the port carries it: WalkOriginal at SimRatio. A tick of it covers 2.4 units at the
// decode's 60 Hz, which is what the original's step covers in its frame (3 x 16 ms).
double WalkCarried(const Tuning& tuning, const ScriptClock::Step& step);

// The share of its x velocity a body off the floor keeps a frame: min(1, GetFPSRate() / 60 x 0.86),
// 0.86 at 60 Hz, 0.43 at 30, 1 from 70 frames a second.
double AirShare(const Tuning& tuning, const ScriptClock::Step& step);

// The velocity a tick of the player's gravity adds going down, as the port carries it, in metres a
// second: the original's world gravity times its SetGravityScale, times the physics step it advances a
// frame, at SimRatio - 10 x 2 x 1/30 x 2 = 1.33 m/s a tick, 4000 units a second squared at 60 Hz.
double FallPerTick(const Tuning& tuning, const ScriptClock::Step& step);

// SideScrollerCharacter::applyForces' dirVec (art.json character.push): the
// velocity it sets, which detectPushing aims its ray along, in the original's own units. On the ground
// the walk, `direction` times WalkOriginal, across the body's vertical velocity; off it the body's
// own x velocity times AirShare, across the same. In pixels a second of the original's physics, +y
// down. Asked BEFORE Steer, of the velocity the physics step left, as applyForces reads it before
// it sets its own.
glm::dvec2 PushAim(entt::registry& registry, entt::entity player, const Tuning& tuning, float direction,
                   const ScriptClock::Step& step);

// SideScrollerCharacter::detectPushing: whether a ray from the body's centre,
// `reachPx` along `aimPx` with `offsetPx` added to its far end, meets any body
// but the player's own as GetClosestContact would (Shot::ClosestContact). Never
// when the aim has no x. A picture's question: nothing it answers is kept by the
// level.
bool Pushing(entt::registry& registry, entt::entity player, const glm::dvec2& aimPx, double reachPx,
             const glm::dvec2& offsetPx);

// One tick of steering, before the physics step. `direction` is -1, 0 or 1: the
// two-button pad (docs/original-gameplay.md section 1). Gravity first, as Box2D's step
// integrates it before the contact; then applyForces' velocity; then Box2D's clamp of a
// step's translation (b2_maxTranslation), which scales the whole velocity down. A player on the
// ground with no pad held on a slope it holds on is pressed into it along its normal: x velocity
// -slope x one tick of gravity, so the sink the engine takes before it solves has no part along the slope.
void Steer(entt::registry& registry, entt::entity player, const Tuning& tuning, float direction,
           const ScriptClock::Step& step);

} // namespace MagicPortals::Player
