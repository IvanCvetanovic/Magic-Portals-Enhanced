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
    // and the body moved where it puts it (MoveKinematic).
    void Tick(entt::registry& registry, entt::entity entity, bool active, float dt);
};

// A door as the level places it: shut where the node stands, opening straight up
// by rules.RisePx() over its metadata/stride milliseconds. False, with `error`,
// when the node has no stride.
bool DoorFromNode(const Tscn::Node& node, const DoorRules& rules, Door& out, std::string& error);

// The port's own mover numbers, game/data/movers.json: two of the
// Godot project's guesses, which it keeps in behaviours.gd rather than in its data.
struct Rules {
    double oscillationRateScale = 0.0; // RATE_SCALE_K, _guess
    double liftSpeedPx = 0.0;          // speed_px_s's default, _guess
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

// A lift, behaviours.gd's SHUTTLE profile (:129-138, :165-169): toward marker `b`
// at its speed, then back to `a`, turning the moment it arrives. It starts where
// the level places it, which in every placement is marker `a`.
struct Shuttle {
    glm::dvec2 aPx{0.0};
    glm::dvec2 bPx{0.0};
    double speedPx = 0.0;
    bool forward = true; // heading for b
    glm::dvec2 atPx{0.0};

    void Advance(double dt);
};

struct Platform {
    std::string name;
    entt::entity entity = entt::null;
    Oscillation motion;
};

struct Lift {
    std::string name;
    entt::entity entity = entt::null;
    Shuttle motion;
};

// A level's moving platforms and lifts. Its doors are its switches' (Puzzle).
struct Movers {
    std::vector<Platform> platforms;
    std::vector<Lift> lifts;

    // One step of each, before the physics step, every body moved with
    // MoveKinematic so that it carries what stands on it.
    void Tick(entt::registry& registry, float dt);

    const Platform* FindPlatform(const std::string& name) const;
    const Lift* FindLift(const std::string& name) const;
};

// A built level's moving platforms and lifts. A lift's ends are the marker
// entities its `a` and `b` name, or its own place where it names none
// (level_runtime.gd:157-163), and its speed is its own or movers.json's. False,
// with `error`, for a platform with no speed or stride, or a mover not built.
bool Wire(const Tscn::Scene& scene, const Roles::Table& roles, const LevelBuilder::Built& built, const Rules& rules,
          Movers& out, std::string& error);

} // namespace MagicPortals::Mover
