#pragma once

// What moves a level's movers.
//
// F1 (the engine's 89f48a8) made the contact solve read a kinematic body's
// velocity. A mover that moves its transform and leaves its velocity at zero is
// only a displacement again - it shoves what it meets and carries nothing - and
// it says nothing about it. So nothing in the port moves a kinematic body except
// MoveKinematic, which writes both.

#include "sim/LevelBuilder.hpp"
#include "sim/Motion.hpp"
#include "sim/Roles.hpp"
#include "sim/ScriptClock.hpp"
#include "sim/Tscn.hpp"

#include <string>
#include <vector>

#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace MagicPortals::Mover {

// Puts a kinematic body at `target` for the step about to be taken, with the
// velocity that gets it there in `dt`: (target - where it is) / dt.
void MoveKinematic(entt::registry& registry, entt::entity entity, const glm::vec3& target, float physicsDt);

// Puts a kinematic body at `target` and leaves it no velocity: the original's own SetTransform on a static
// body (ETHPhysicsEntityController::SetPosXY), a displacement the solver pushes what it overlaps out of and
// carries nothing by. The one call a moving platform makes it with is its first, which jumps it a stride from
// where the level placed it (Mover::Movers::Tick).
void PlaceKinematic(entt::registry& registry, entt::entity entity, const glm::vec3& target);

// A lift door (door_lift.ent), as ETHCallback_door_lift runs it (movers.json
// `door`, planning step 133). Until then the port ran the Godot project's
// Mover.gated: 126 px at one speed, turned wherever its channel turned it.
//
// It rises 0.8 of its own height over its node's `stride` milliseconds, eased by
// smoothEnd, and a status says where it is in that: shut, opening, open, closing.
// Its switch starts a rise only from shut and a fall only from fully raised, and
// nothing stops a leg once it has started. So a button pressed for one tick lifts
// the door all the way, and one let go lowers it all the way - but only once it
// has finished rising.
enum class DoorStatus : int { Closing = -1, Shut = 0, Opening = 1, Open = 2 };

// The door's numbers, movers.json `door`, decoded.
struct DoorRules {
    double riseShare = 0.0;      // of its height: openPos is 0.8 x GetSize().y above originalPos
    glm::dvec2 sizeUnits{0.0};   // GetSize(): metal_door.png at scale 1
    double crushShare = 0.0;     // ETHBeginContactCallback_door_lift's doorHalfSize, GetSize() x 0.55
    double easeRad = 0.0;        // PIb: smoothEnd is sin(bias x this), the button's

    double RisePx() const { return riseShare * sizeUnits.y; }
};

struct Door {
    // originalPos and openPos, in the level's pixels (+y down) and in world metres.
    glm::dvec2 closedPx{0.0};
    glm::dvec2 openPx{0.0};
    glm::vec3 closed{0.0f};
    glm::vec3 open{0.0f};
    double strideMs = 0.0;
    double easeRad = 0.0;

    // The PositionInterpolator: m_a, m_b and m_elapsedTime. Built on the level's
    // first frame as (originalPos, originalPos), so it holds the door where it is.
    glm::dvec2 fromPx{0.0};
    glm::dvec2 toPx{0.0};
    double elapsedMs = 0.0;

    // currentPos: where the last tick put it, which is what this tick's status reads.
    glm::dvec2 atPx{0.0};
    DoorStatus status = DoorStatus::Shut;

    // What the layer and the suites watch. `progress` is the share of the rise it
    // stands at, 0 shut and 1 fully raised. `opening` is the leg it last started,
    // so it flips exactly when the status machine starts one (#389, #530).
    // `landings` counts its returns to shut from a fall (#310-315's shake).
    float progress = 0.0f;
    bool opening = false;
    int landings = 0;

    // PositionInterpolator::getCurrentPos: m_b once the clock is past the stride,
    // a + (b - a) x sin(PIb x clamp(t / stride, 0, 1)) before it.
    glm::dvec2 InterpolatedPx() const;

    // One frame of ETHCallback_door_lift (#210-633) with its switch `active` or not,
    // and the body moved where it puts it (MoveKinematic). The interpolator's clock is
    // the script's; the body's velocity is measured against the physics step.
    void Tick(entt::registry& registry, entt::entity entity, bool active, const ScriptClock::Step& step);
};

// A door as the level places it: shut where the node stands, opening straight up
// by rules.RisePx() over its metadata/stride milliseconds. False, with `error`,
// when the node has no stride.
bool DoorFromNode(const Tscn::Node& node, const DoorRules& rules, Door& out, std::string& error);

// What one callback passes linearMotion besides the node's own `speed` and `stride`: the three
// arguments (movers.json `linear_motion`, decoded).
struct LinearCall {
    bool vertical = true;
    double axisDeg = 0.0;    // linearMotion's `angle`: the axis turned by it
    double startAngle = 0.0; // linearMotion's `startAngle`
};

// The port's own mover numbers, game/data/movers.json: linearMotion's two calls (a moving platform's and
// an anti-portal agent's) and the lift as ETHCallback_lift runs it, all decoded.
struct Rules {
    LinearCall platform;           // ETHCallback_moving_platform: vertical, axis 0, start 0
    LinearCall agent;              // ETHCallback_anti_portal_agent: vertical, axis 0, start 0 ...
    LinearCall agentSideways;      // ... and axis_deg_sideways when its `direction` is directionSideways
    std::string directionSideways; // the string the callback compares `direction` with
    double liftSpeedUnitsPerS = 0.0;   // the callback's literal 60
    glm::dvec2 liftSizeUnits{0.0};     // lift.png's 64 x 32: the rect a touch sends it from
    // blinkColor(this, (0.6, 0.6, 0.6), (1, 1, 1), 300): a triangle between the two
    // colours, each leg a stride long. The picture's, drawn by the layer.
    glm::dvec3 liftBlinkFrom{1.0};
    glm::dvec3 liftBlinkTo{1.0};
    double liftBlinkStrideMs = 0.0;
    // linearMotion's own frame cap and turn (motions.json linear_motion): Game::LoadData copies them in, so
    // that one reading of that file serves every motion and no second copy of two decoded numbers is kept.
    Motion::Rules motion;

    // The call a patrolling agent with this `direction` makes ("" when its node names none).
    const LinearCall& AgentCall(const std::string& direction) const {
        return !directionSideways.empty() && direction == directionSideways ? agentSideways : agent;
    }
};

bool LoadRules(const std::string& path, Rules& out, std::string& error);

// A swing about where the level places something, as the engine's linearMotion runs it for a callback
// that gives it the node's `speed` and `stride` (motions.json linear_motion, planning step 154):
//   first call  originalPos = where the node stands; angle = startAngle
//   every call  angle += speed x min(200, frame ms) / 1000 (Motion::Advance), one turn off once past 2 PI
//   position    originalPos + turned(cos(angle) x stride x sign(speed))
// So it swings the FULL stride either side of the node's point, at `speed` radians a second, and it
// starts a stride along its axis: the first call writes the position already one frame's step in. Until
// that call the entity is where the level put it. The cosine is DetMath's, the contract the swing keeps so
// that it comes out the same on every C runtime (Motion::OffsetPxWithCosine).
struct Oscillation {
    glm::dvec2 originPx{0.0}; // 'originalPos': the node's own point, the middle of the swing
    Motion::Linear linear;    // speed, stride, axis and the angle so far

    // One call of linearMotion over `frameMs` of the script's clock.
    void Advance(const Motion::Rules& rules, double frameMs);
    // Where the last call put it, or where the level stands it before the first.
    glm::dvec2 AtPx() const;
    bool Started() const { return linear.started; }
};

// From a node's position, speed and stride, with the callback's arguments. False, with `error`, when it
// has no speed or no stride: the port does not invent either.
bool OscillationFromNode(const Tscn::Node& node, const LinearCall& call, Oscillation& out, std::string& error);

// A lift as ETHCallback_lift runs it (movers.json `lift`): at rest on its marker `a`
// from the first frame, until a touch goes down inside its picture and SENDS it
// (sendLiftToOtherPoint) - from a to b, from b to a, and from anywhere between back
// to a, whichever way it was going. Sent, it moves toward `dest` at its speed and is
// put on it the frame it has come as far from `origin` as a is from b; there it waits
// for the next touch. Every test of where it is reads this state, never its body,
// whose float round trip would break the callback's exact vector2 equalities.
struct Travel {
    glm::dvec2 aPx{0.0};
    glm::dvec2 bPx{0.0};
    glm::dvec2 atPx{0.0};
    glm::dvec2 destPx{0.0};   // 'dest': where it is going, or where it rests
    glm::dvec2 originPx{0.0}; // 'origin': the end it is measured from
    glm::dvec2 dir{0.0};      // 'dir': normalize(dest - its point when sent)
    double legSquared = 0.0;  // 'squaredDistance': |b - a|^2
    double speedPx = 0.0;
    int sends = 0;            // how many touches have sent it: playElevatorSound each

    bool Moving() const { return atPx != destPx; }
    // sendLiftToOtherPoint.
    void Send();
    // One frame of #263-367: moved by dir x speed x dt while it is not at `dest`, and
    // put on `dest` once it has come the leg's length from `origin`.
    void Advance(double dt);
    // isPointInRect(point, its point, size, (0.5, 0.5)): the rect of `sizeUnits`
    // centred on it, both edges in.
    bool Holds(const glm::dvec2& pointPx, const glm::dvec2& sizeUnits) const;
};

struct Platform {
    std::string name;
    entt::entity entity = entt::null;
    Oscillation motion;
};

struct Lift {
    std::string name;
    entt::entity entity = entt::null;
    Travel motion;
    glm::dvec2 sizeUnits{0.0}; // its picture's, which a touch is tested against
};

// A level's moving platforms and lifts. Its doors are its switches' (Puzzle).
struct Movers {
    std::vector<Platform> platforms;
    std::vector<Lift> lifts;
    Motion::Rules motion; // linearMotion's frame cap and turn, from Rules::motion (Wire)

    // One step of each, before the physics step, every body moved with
    // MoveKinematic so that it carries what stands on it. Where it goes is the script's
    // clock (linearMotion's unitsPerSecond: the angle steps by the tick's whole milliseconds);
    // the velocity that carries it there is the physics step's. A platform's first call, which jumps
    // it a stride from where the level placed it, is a bare teleport (PlaceKinematic).
    //
    // WHEN. The original's callbacks run after its physics step, so what a step meets is the position
    // the frame before wrote; the port moves a body before its step, like every other mover here (a
    // door, a bobbing slab, a lift): a body is a frame ahead of the original's for the step that
    // follows (planning step 154, deviation 4), and the picture drawn after the tick is the original's.
    void Tick(entt::registry& registry, const ScriptClock::Step& step);

    // A touch gone down this frame at `pointPx`, a point of the level: every lift
    // whose picture holds it is sent, as each lift's own callback tests the touch.
    // How many were sent - the caller's to take the touch when it is any.
    int Press(const glm::dvec2& pointPx);

    const Platform* FindPlatform(const std::string& name) const;
    const Lift* FindLift(const std::string& name) const;
};

// A built level's moving platforms and lifts. A lift's ends are the marker
// entities its `a` and `b` name, or its own place where it names none
// (level_runtime.gd:157-163); it is put on `a`, and its speed and size are
// movers.json's for every lift - the callback reads no `speed` of a node's. False,
// with `error`, for a platform with no speed or stride, or a mover not built.
bool Wire(const Tscn::Scene& scene, const Roles::Table& roles, const LevelBuilder::Built& built, const Rules& rules,
          Movers& out, std::string& error);

} // namespace MagicPortals::Mover
