#pragma once

// What moves a level's movers.
//
// F1 (the engine's 89f48a8) made the contact solve read a kinematic body's
// velocity. A mover that moves its transform and leaves its velocity at zero is
// only a displacement again - it shoves what it meets and carries nothing - and
// it says nothing about it. So nothing in the port moves a kinematic body except
// MoveKinematic, which writes both.

#include "sim/LevelBuilder.hpp"
#include "sim/Roles.hpp"
#include "sim/Tscn.hpp"

#include <string>
#include <vector>

#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace MagicPortals::Mover {

// Puts a kinematic body at `target` for the step about to be taken, with the
// velocity that gets it there in `dt`: (target - where it is) / dt.
void MoveKinematic(entt::registry& registry, entt::entity entity, const glm::vec3& target, float dt);

// A switched door, behaviours.gd's Mover.gated (:140-148, :170-176): it opens by
// rising its own height, 126 px, over `stride` milliseconds, and closes the same
// way. Reversed mid-travel it resumes from where it is rather than snapping.
inline constexpr double kDoorRisePx = 126.0;

struct Door {
    glm::vec3 closed{0.0f}; // world metres, where the level places it
    glm::vec3 open{0.0f};
    float durationS = 1.0f;
    float progress = 0.0f; // 0 closed, 1 open
    bool opening = false;

    // One step of travel, and the body moved there (MoveKinematic).
    void Tick(entt::registry& registry, entt::entity entity, float dt);
};

// A door as the level places it: closed where the node stands, opening upward by
// kDoorRisePx over its metadata/stride milliseconds - a bare number with no
// `speed`, so a duration, by behaviours.gd's stride rule. False, with `error`,
// when the node has no stride.
bool DoorFromNode(const Tscn::Node& node, Door& out, std::string& error);

// The port's own mover numbers, game/data/movers.json: the Godot project's guessed
// swing, and the lift as ETHCallback_lift runs it (decoded).
struct Rules {
    double oscillationRateScale = 0.0; // RATE_SCALE_K, _guess
    double liftSpeedUnitsPerS = 0.0;   // the callback's literal 60
    glm::dvec2 liftSizeUnits{0.0};     // lift.png's 64 x 32: the rect a touch sends it from
    // blinkColor(this, (0.6, 0.6, 0.6), (1, 1, 1), 300): a triangle between the two
    // colours, each leg a stride long. The picture's, drawn by the layer.
    glm::dvec3 liftBlinkFrom{1.0};
    glm::dvec3 liftBlinkTo{1.0};
    double liftBlinkStrideMs = 0.0;
};

bool LoadRules(const std::string& path, Rules& out, std::string& error);

// A swing about where the level places something, behaviours.gd's OSCILLATE
// profile (:158-164), along an axis, by half its stride either way:
//   offset = sin(rateScale * speed * t) * stride / 2.
// `speed` is a frequency, not a velocity, by the Godot project's reading of the data.
// It starts at the level's place, t = 0. Moving platforms swing vertically, and
// patrolling no-portal zones on the axis their `direction` names.
struct Oscillation {
    glm::dvec2 originPx{0.0};
    glm::dvec2 axis{0.0, 1.0}; // in the Godot project's pixels, +y down: its Vector2.DOWN
    double stridePx = 0.0;
    double speed = 0.0;
    double rateScale = 0.0;
    double t = 0.0;

    glm::dvec2 AtPx() const;
};

// From a node's position, speed and stride. False, with `error`, when it has no
// speed or no stride: the Godot project falls back to literals, the port does not
// invent them.
bool OscillationFromNode(const Tscn::Node& node, const glm::dvec2& axis, double rateScale, Oscillation& out,
                         std::string& error);

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

    // One step of each, before the physics step, every body moved with
    // MoveKinematic so that it carries what stands on it.
    void Tick(entt::registry& registry, float dt);

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
