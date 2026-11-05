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
//     min(1, GetFPSRate() / 60 x 0.86): 0.86 a frame at 60 Hz, an exponential brake. The port evaluates it at
//     player.json's air_decay_rate_hz (the owner's ruling of planning step 213: 120 Hz, where it is 1): no brake.
//   - GRAVITY is Box2D's: the world's scale(10) m/s^2 times the body's SetGravityScale(2),
//     with no cap but Box2D's own b2_maxTranslation, 2 m a step, which a level's height
//     never reaches and a hidden, dead body falling on does (Steer applies it).
//
// IN THE PORT'S UNITS. The original's physics advances 1/30 s a frame; the port's solver
// steps its 1/60 s tick (ScriptClock::SimRatio is 2). So the player's velocity is carried at
// SimRatio times the original's, and its gravity is applied as the velocity the original's
// step would add a frame, times that: both cover, each tick, what the original's step covers
// in the frame. Every other dynamic body is in that time base too, by the same mechanism
// (planning step 152: ScriptClock.hpp, "THE CARRIED FRAME" - the world's gravity at the ratio
// squared, a crate's, a stone's and a minion's velocity at the ratio), so a velocity another
// system hands any body is converted at its own seam and read back into the original's units
// where a callback reads it.
//
// Dynamic rather than kinematic because the spike measured F2: a kinematic body
// trips no trigger. Rotation frozen and locked to the plane (S1). Its capsule is the
// original's own shape (planning step 153, CAP): dark_mage.ent's two circles of radius 9.1 at
// +-9 from the entity's position (player.json body), which the engine's one-collider-per-entity
// holds as their hull, a capsule 18.2 wide and 36.2 tall - the Godot project's 20 x 44 guess is gone.
//
// Two places it cannot be the original line for line, both because the port's solver
// is not Box2D:
//   - Gravity is applied every tick, not only off the floor, as Box2D applies it. The
//     solver has no snap, and without gravity the player would hover over a lift going
//     down, since a contact only holds what presses into it. On the floor the contact
//     takes the gravity back out.
//   - It grips nothing but a rolling stone: kFriction is 0, and sim/StoneGrip raises rigid.friction on the
//     ticks a stone is within reach (planning step 216: the decoded pair 0.15, the reaction on the stone).
//     The original's body has friction 0.9
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

namespace Supersonic {
struct RigidBodyComponent;
}

namespace MagicPortals::Player {

inline constexpr float kFriction = 0.0f;

struct Tuning {
    double speed = 0.0;         // movement.speed: SideScrollerCharacter.speed, scale() metres a second
    double elapsedCapMs = 0.0;  // movement.elapsed_cap_ms: applyForces' min(66, the script's frame)
    double airDecay = 0.0;      // movement.air_decay: the x velocity kept a frame off the floor...
    double airDecayFps = 0.0;   // movement.air_decay_fps: ...at this GetFPSRate (the 60 of fps / 60)
    double airRateHz = 0.0;     // movement.air_decay_rate_hz: the GetFPSRate the port EVALUATES the formula at (the owner's ruling, planning step 213)
    double worldGravity = 0.0;  // gravity.world_m_s2: the world's scale(10)
    double gravityScale = 0.0;  // gravity.scale: SetGravityScale(2)
    double maxTranslationM = 0.0;       // gravity.max_translation_m: b2_maxTranslation, Box2D metres a step
    double referencePxPerUnit = 0.0;    // gravity.reference_px_per_unit: the screen pixels a unit is at 720 lines
    // The screen pixels a unit is NOW: b2_maxTranslation is 100 screen pixels, so the clamp's limit in units goes with the
    // screen (G10-6). LoadTuning sets it to the reference; the layer overwrites it every tick from the viewport.
    double screenPxPerUnit = 0.0;
    // The body: dark_mage.ent's compound of two circles (body.*). The box is the .ent's <Collision><Size>, the circles'
    // radius and offsets are shares of it, and widthPx / heightPx are the capsule that is their hull, which LoadTuning
    // derives from the four and refuses a file that disagrees with.
    double boxWidthPx = 0.0;         // body.box_width_px: the .ent's Size x, 14
    double boxHeightPx = 0.0;        // body.box_height_px: the .ent's Size y, 36
    double circleRadiusShare = 0.0;  // body.circle_radius_share: radius 1.3 of half the box's width
    double circlePosShare = 0.0;     // body.circle_pos_share: posY +-0.5 of half the box's height
    double widthPx = 0.0;            // body.width_px: 18.2, the circles' full width
    double heightPx = 0.0;           // body.height_px: 36.2, the circles' full height
    // The ground sensor: char_feet.ent, a polygon the body carries below it, which counts what it
    // touches (feet.*): its size, the chamfer of its octagon's corners, and how near counts as touching.
    double feetWidthPx = 0.0;
    double feetHeightPx = 0.0;
    double feetChamfer = 0.0;
    double feetOffsetShare = 0.0;  // feet.offset_box_share: addFeetOffset's collisionBox.size.y x 0.5
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
// octagon 14 units wide and 10 tall (feet.*) standing 18 units below the entity's position (FeetOffsetPx:
// SideScrollerCharacter::addFeetOffset, ins 56-74: the collision box's pos + (0, size.y / 2)), its numTouches counted by the
// contact callbacks that ignore sensors, characters, fireballs and anti_sight_wall (shouldBeIgnoredByFeet,
// bytes 361584..361843); manageFeet sets the flag at the end of each update. Here the octagon is tested
// against every solid collider but the player's own (Impacts::Between), each tick, so it reads the state
// the last physics step left, as the flag does. Wider than the old one-ray test: a player walking off a
// ledge is on the ground until its feet are 7 units clear, which F13's walk off 1-06's block shows
// (planning step 148).
// WHAT COUNTS, as shouldBeIgnoredByFeet decides it: every contact but a sensor's (a trigger here: the port builds no sensor as
// a solid collider), the player's own body (isCharacter: set by GameCharacter's constructor, built for the main character
// alone), `fireball.ent` (no body in the port) and an `anti_sight_wall*` (a sensor). So a minion's or a knight's body IS ground,
// as a crate, a stone, a barrel, a platform and a lift are (planning step 153: step 148 and the backlog read the refused
// "character" as a minion; the listing does not).
bool Grounded(entt::registry& registry, entt::entity player, const Tuning& tuning);

// Where the ground sensor's octagon stands, in units below the entity's position: addFeetOffset's
// collisionBox.size.y x 0.5 (SideScrollerCharacter bytes 128877..129480, ins 56-74), 18 for dark_mage.ent's 14 x 36 box.
// Not the capsule's bottom (18.1): the original's sensor sits 0.1 above the lowest point of its circles.
double FeetOffsetPx(const Tuning& tuning);

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

// The share of its x velocity a body off the floor keeps a frame, the original's own formula at a GetFPSRate of `fps`:
// min(1, fps / 60 x 0.86) - 0.86 at 60, 0.43 at 30, 1 from 70 frames a second.
double AirShareAt(const Tuning& tuning, double fps);

// The share the port uses a tick: the formula at movement.air_decay_rate_hz, NOT at the tick's own rate. The original's brake
// depends on the phone's frame rate and the port plays the same on every phone, so one rate has to be chosen; the owner chose
// 120 Hz (his phone's, planning step 213), where it is 1 and a body off the floor keeps all its x velocity. `step` is not read.
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

// b2_maxTranslation in the port's metres a tick: 2 Box2D metres are 100 SCREEN pixels, so the limit in units is
// 100 x 256 / the screen's height - 35.6 at 720 lines, 23.7 at 1080 (G10-6; planning step 153). Read off
// `tuning.screenPxPerUnit`, which the layer keeps at the viewport's own and everything else leaves at the 720 reference.
double ClampLimitM(const Tuning& tuning);

// Box2D's clamp of a step's translation (b2Island::Solve, b2_maxTranslation), on ANY dynamic body: the
// whole velocity is scaled down so the step carries it no further than ClampLimitM. `addedByTheEngine` is the velocity the engine's own integration will add
// before it moves the body (a body that uses the world's gravity: gravity times the physics step), which
// the clamp judges the translation on and leaves in place. The player's is applied by Steer, after its
// own gravity; Game::BeforeStep applies it to every other body.
void ClampTranslation(Supersonic::RigidBodyComponent& rigid, const Tuning& tuning, const ScriptClock::Step& step,
                      const glm::vec3& addedByTheEngine = glm::vec3(0.0f));

// One tick of steering, before the physics step. `direction` is -1, 0 or 1: the
// two-button pad (docs/original-gameplay.md section 1). Gravity first, as Box2D's step
// integrates it before the contact; then applyForces' velocity; then Box2D's clamp of a
// step's translation (b2_maxTranslation), which scales the whole velocity down. A player on the
// ground with no pad held on a slope it holds on is pressed into it along its normal: x velocity
// -slope x one tick of gravity, so the sink the engine takes before it solves has no part along the slope.
void Steer(entt::registry& registry, entt::entity player, const Tuning& tuning, float direction,
           const ScriptClock::Step& step);

} // namespace MagicPortals::Player
