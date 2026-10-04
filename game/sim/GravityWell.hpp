#pragma once

// Gravity wells: entity `gravity_agent` / `gravity_agent.ent`, role
// `gravity_well`. Ten placements across five levels, and the last mechanic in
// the game that is not a boss.
//
// A well is these things at once, and the port already had the first:
//   - a SOLID CIRCLE. The node is a StaticBody2D with a CircleShape2D, so it is
//     an obstacle to the player and to every loose body. NOT TO A SHOT: the
//     original's projectile.ent has no shape, and the ray that judges a tap
//     passes gravity_agent by name, so Game::Start hands each agent's body to
//     Portals::State::shotPasses and a shot flies through it (00_order R2).
//   - an ATTRACTOR, which is this module: every DYNAMIC body inside its radius
//     is pulled toward the centre. The original's DynamicBodyChooser rejects
//     statics and anything with no physics controller, so platforms and walls
//     stand still inside a well and only loose bodies move.
//   - and A GREEN RING. ETHCallback_gravity_agent adds an antiportal.ent at its
//     own position on the first tick - nothing in the level file says
//     `antiportal`; it exists only in the callback - and that entity SEEN is
//     white_ring.png at radius * 2 - 24 across, added, painted (0.25, 0.5, 0.25)
//     by the callback. It is drawn by the layer from `Rules::ring`
//     (gravitywell.json). IT REFUSES NO PORTAL: it is renamed gravity_area as it
//     is added (ETHScene.cpp:264-266), after AntiPortalManager's constructor has
//     gathered its fields by name (104925..105343 #16-35), so no tap is ever
//     tested against it (00_order R2, decided for the original; the port's zone
//     round each well went in the remaster doc's step 125).
//
// THE FORCE, decoded (ETHCallback_gravity_area, bytes 461443..462571):
//
//     forceDir  = normalize(centre - body)      // toward the centre: it ATTRACTS
//     forceBias = smoothEnd(1 - d^2/r^2)        // sin(x * PI/2), an ease-out
//     force     = forceDir * forceBias * forceLength * (dt_ms / 16.6666)
//     velocity += force                         // ADDED, never set
//
// forceLength is 0.18 where the world has no gravity and 0.5 where it has, and
// that splits these five levels exactly: level16c, level17c and level18c are
// no_gravity levels, level20c and level27c are not. gravitywell.json holds the
// decode, the units, and the one number in it that is inferred rather than
// decoded (PIb).

#include "sim/Roles.hpp"
#include "sim/Tscn.hpp"

#include <string>
#include <vector>

#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace MagicPortals::GravityWell {

// THE RING the agent adds, and the one thing about a well that is seen rather
// than felt: an antiportal.ent, sized from the agent's radius and painted green
// by `area.SetColor(vector3(0.5, 1, 0.5) * 0.5)`. It is renamed `gravity_area` at
// AddEntity, so the callback it gets is the force above and not
// ETHCallback_antiportal: no blink, no turn, one size. gravitywell.json `ring`
// carries the instructions, and the rest of the row is antiportal.ent's own file.
struct Ring {
    std::string sprite;       // its <Sprite>: white_ring.png
    bool additive = false;    // its blendMode 1
    glm::dvec3 colour{0.0};   // what the callback paints it: (0.25, 0.5, 0.25)
    glm::dvec3 emissive{0.0}; // its <EmissiveColor>, rgb: (1, 1, 1)
    bool applyLight = false;  // its applyLight
    bool isStatic = false;    // its static
    double z = 0.0;           // AddEntity's vector3(pos, -5): its depth, and its lighting height
};

// THE LAPS A BODY RUNS ROUND A WELL, and the turn a lap gives the agent's own
// picture (the visuals plan's 11.2; gravitywell.json `roundabout`).
// computeRoundabout (bytes 459694..461443), called by ETHCallback_gravity_area
// #184-197 for every body it pulls, with forceDir = normalize(well - body) and
// inRange, keeps a signed sum of how far that direction has swept, per body and
// per well; a lap is a sum beyond 360 degrees either way, and it starts again at
// 0. What a lap ADDS - roundabout_effect.ent, the sound, the agent's spin - only
// the visible character's does (#332-419), and that is the layer's to add from
// State::lapsNow. The counts it bumps are read by no picture and by nothing the
// port builds (retrieveRondaboutCount serves an achievement).
struct Roundabout {
    double stepDotBelow = 0.0; // 0.98: a step counts only once the direction has turned past acos of this
    double lapDegrees = 0.0;   // 360: a sum beyond this either way is a lap
    // ETHCallback_gravity_agent #174-230, every frame: SetAngle(elastic(smoothEnd(
    // min(effectAngleTime, spinMs) / spinMs)) x spinDegrees x effectAngleSign), the
    // clock zeroed and the sign set by a lap (computeRoundabout #369-419) and
    // started at spinMs (#148-157), so at rest it stands at a whole turn.
    double spinMs = 0.0;       // 3000
    double spinDegrees = 0.0;  // 360
    // Interpolator's elastic (bytes 314142..314380): n itself at 0 and at 1, and
    // otherwise pow(2, decay x n) x sin((n - period / shiftDivisor) x 2 pi / period) + 1.
    double elasticPeriod = 0.0;       // 0.3 (#15, SetV8 raw 0x3fd3333340000000: 0.3f widened)
    double elasticShiftDivisor = 0.0; // 4 (#17-18: s = p / 4)
    double elasticDecay = 0.0;        // -10 (#21-24: pow(2, -10 n))
};

// elastic(n), in the script's floats.
float Elastic(const Roundabout& rules, float n);

// The agent's own angle `ms` after a lap in `sign` (+1 counter-clockwise on the
// screen, -1 clockwise), in Ethanon's degrees: 0 at the lap, past a whole turn to
// about 1.37 of one a quarter of a second in, then settling on the turn. At spinMs
// and after, exactly spinDegrees x sign, which draws as no turn at all.
double SpinDegrees(const Roundabout& rules, double ms, double sign);

struct Rules {
    double forceZeroGravity = 0.0; // 0.18, where the world's gravity is V2_ZERO
    double forceWithGravity = 0.0; // 0.5, where it is not
    double referenceFrameMs = 0.0; // 16.6666: the frame the force is quoted per
    double zoneShrinkPx = 0.0;     // 24: the added antiportal's SIZE is radius*2 - this
    Ring ring;                     // and what that antiportal looks like
    Roundabout roundabout;         // and the laps round it
};

// False, with `error`, for a file that does not read, and for one whose numbers
// would make a well that does nothing or a ring bigger than its own well.
bool LoadRules(const std::string& path, Rules& out, std::string& error);

struct Well {
    std::string name;
    glm::dvec2 atPx{0.0};
    double radiusPx = 0.0;

    // THE SIZE the agent's scaleToSize gives its antiportal, across: radius*2 -
    // 24, which is what is DRAWN. 188 units at level16c's radius of 106. Only
    // drawn: the area refuses no portal (00_order R2), so nothing plays half of it.
    double RingSizePx(const Rules& rules) const { return radiusPx * 2.0 - rules.zoneShrinkPx; }
};

// The velocity a single well adds to a body this tick, in the engine's plane.
// Zero for a body outside the well, and for one exactly at the centre - which
// is the case normalize cannot answer.
//
// `noGravity` picks the strength, and it is the LEVEL's flag rather than a
// question put to the registry: the original branches on GetGravity() ==
// V2_ZERO, and Game::Level::noGravity is what set that in the first place.
glm::vec3 Pull(const Well& well, const Rules& rules, const glm::dvec2& bodyPx, bool noGravity, double dt);

// One body's sweep round one well: computeRoundabout's custom data on the body,
// named by the well's id (roundSum_<id>, lastVector_<id>, cw/ccwRoundCount_<id>).
struct Sweep {
    entt::entity body{entt::null};
    int well = -1;
    float roundSum = 0.0f;      // radians, signed: above 0 counter-clockwise on the screen
    glm::vec2 lastVector{0.0f}; // the direction the last counted step ended on
    int cw = 0;                 // laps each way
    int ccw = 0;
};

// A lap, on the tick it was run: the sum it went past lapDegrees with, in
// degrees - its sign is the lap's way round.
struct Lap {
    entt::entity body{entt::null};
    int well = -1;
    double degrees = 0.0;
};

// One step of a sweep, as computeRoundabout takes it: `toWell` is
// normalize(well - body) in floats, `inRange` the pull's own test, and the two
// y's the well's and the body's (+y down). A body met for the first time starts
// its sweep here and counts nothing. True, with `lapDegrees`, on the step a lap
// is run.
bool StepSweep(const Roundabout& rules, Sweep& sweep, bool first, const glm::vec2& toWell, bool inRange, float wellY,
               float bodyY, double& lapDegrees);

struct State {
    Rules rules;
    std::vector<Well> wells;
    bool noGravity = false; // the level's, set by Game::Start

    // Every body's sweep round every well, in the order each was first met, and
    // the laps this tick ran (Tick empties it first). Write-only as far as the
    // simulation goes: nothing here reads a lap, and nothing it pulls changes by
    // one. The layer reads lapsNow.
    std::vector<Sweep> sweeps;
    std::vector<Lap> lapsNow;

    // One step, before the physics step, over every DYNAMIC body: the ones with
    // a RigidBodyComponent that are not kinematic. A kinematic body is moved by
    // code and would fight whatever moves it; a static one has no velocity at
    // all. That is the port's reading of DynamicBodyChooser. Each body's sweep
    // round each well is stepped from the same point the pull is measured from.
    //
    // Every body is carried at `velocityRatio` times the original's velocity (ScriptClock::SimRatio,
    // planning step 152), and the pull is a velocity the original adds to its own once a frame, so each
    // is given it that many times over: the player's, a crate's, a stone's, a minion's alike.
    void Tick(entt::registry& registry, double dt, double velocityRatio = 1.0);

    const Well* Find(const std::string& name) const;
};

// A level's gravity wells, by role. The radius is the node's own
// metadata/radius, which runs from 64 to 260 px across the game - so unlike a
// bouncer, this IS level-authored data.
//
// False, with `error`, for a well with no position or no radius: the original
// reads GetFloat('radius') and a well without one would pull nothing anywhere.
bool Find(const Tscn::Scene& scene, const Roles::Table& roles, const Rules& rules, State& out, std::string& error);

} // namespace MagicPortals::GravityWell
