#pragma once

// The contacts the original's own begin-contact callbacks shake the camera on:
// every impact of the player (ETHBeginContactCallback_dark_mage), of a rolling
// stone (ETHBeginContactCallback_rolling_stone) and of a minion
// (ETHBeginContactCallback_minion) - earthquake.json's three impact rows, D11
// rows 4, 8 and 25.
//
// Box2D calls a begin-contact callback on the step a pair of fixtures starts to
// touch, and the callback reads the body's velocity as the step found it, before
// its solver has taken the approach out. The engine's PhysicsSystem reports
// neither a pair's first touch nor that velocity to a layer's tick (and the
// suites step it with no contact list at all), so the port finds both itself,
// as its demolish, trigger and beholder-rock tests already judge touching
// (D11_check C48): the velocity each watched body carries INTO the step, kept
// when the tick has steered it, and after the step every pair whose surfaces
// are within the touching distance, against the pairs that were within it after
// the last step. A pair that is new is a begin, and it keeps the velocities its
// body carried into the steps since it last met or left something, because the
// engine's solver brakes a closing pair before the pair meets (kRecentSteps).
//
// Read only: nothing here writes the registry, draws a random number or reaches
// Game::Level, so nothing the simulation does can change.

#include <cstddef>
#include <utility>
#include <vector>

#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace MagicPortals::Impacts {

// Whose callback a watched body's begins run. A crate's (crateContactCallback)
// is a sound and no shake: planning step 171.
enum class Kind { Player, Stone, Minion, Crate };

struct Watched {
    entt::entity body = entt::null;
    Kind kind = Kind::Player;
    // The original's entity point against the body's centre, in the port's
    // pixels (+y down): a minion's stands above its circle (minions.json
    // body.entity_above_feet_px). The stone's and minion's rows aim at it.
    glm::dvec2 pointOffsetPx{0.0};
    // Where the original's body stood against the port's when it was made, +y
    // down: a minion's is lower (the port centres its circle on the marker, the
    // original puts its entity point there). A body looked at for the first time
    // is looked at there - the pairs it touches, the point it aims from - until
    // its own shape reaches one of those pairs, or it is put somewhere else.
    glm::dvec2 spawnDropPx{0.0};
};

// A collider in the plane, in the port's pixels (+y down): a convex core of one
// point (a sphere), two (a capsule's segment) or more (a box's corners, a hull's
// outline), grown by a radius.
struct Shape {
    std::vector<glm::dvec2> core;
    double radiusPx = 0.0;
};

// An entity's solid collider as a Shape: a box turned about z, a sphere, a
// capsule along its local y, or a hull by its LevelBuilder::PlanePolygon. False
// for a trigger, and for an entity with none of them.
bool ShapeOf(entt::registry& registry, entt::entity entity, Shape& out);

struct Gap {
    double px = 0.0;               // between the two surfaces; at or below 0 where they meet or overlap
    glm::dvec2 normal{0.0, 1.0};   // unit, from a's nearest point toward b's
};

// How far apart two shapes are, and which way b lies from a across the gap: the
// line between their nearest points. Where their cores themselves overlap there
// is no such line, and the direction is the one between the cores' centres.
Gap Between(const Shape& a, const Shape& b);

// How many steps of a body's velocities are kept, back to the last time it met
// or left something: a second at the port's 60 Hz, in which the player's
// gravity reaches its fall's cap (player.json) and past which a stone's only
// grows. The engine's solver takes a closing pair's approach out before the pair
// meets - a speculative contact, apart but inside the step's reach, which it
// brakes to a stop, in stages, from as far as a step's travel above a floor - so
// the speed a pair came together at is a velocity of a step before the one it
// touches on, and the fastest since the body last met or left anything is it.
inline constexpr std::size_t kRecentSteps = 60;

// One pair that began to touch in the step just taken.
struct Begin {
    entt::entity body = entt::null;
    Kind kind = Kind::Player;
    entt::entity other = entt::null;
    glm::dvec2 normal{0.0, 1.0};      // Between's, from the body toward the other
    glm::dvec2 velocityPx{0.0};       // the body's going into the step, px/s, +y down
    glm::dvec2 atPx{0.0};             // the body's entity point going into the step
    glm::dvec2 otherAtPx{0.0};        // the other's entity point: its transform, or a watched body's point
    double gapPx = 0.0;
    // The velocities the body carried into this step and the steps before it,
    // newest first, back to when it last met or left something.
    std::vector<glm::dvec2> recentPx;

    // Of those, the one that came along `along` (a unit vector) fastest: what the
    // pair closed at before the solver took the approach out. The newest where
    // none came along it at all.
    glm::dvec2 Approaching(const glm::dvec2& along) const;
};

class Watch {
public:
    // The step is about to be taken: each watched body's velocity and point, the
    // velocity kept with the ones it carried into the steps since it last met or
    // left something. A body the tick itself has moved since the last step left
    // it - a portal's teleport, the one thing that writes a watched body's place
    // between the step and the steer (Portals.cpp Teleport) - keeps none of them:
    // it comes out at the velocity the exit gives it, and what it carried toward
    // the entry is spent.
    void BeforeStep(entt::registry& registry, const std::vector<Watched>& bodies);

    // The step of `dtS` seconds has been taken: every pair a watched body
    // touches, within `touchPx` of it, and of those the ones it did not touch
    // after the last step. A body looked at for the first time - a level's first
    // tick, a stone just thrown, a body put somewhere farther than the step could
    // carry it - only has its pairs marked; one with a spawn drop is looked at
    // where the original's stands until its own shape reaches one of the pairs
    // that holds (Watched::spawnDropPx).
    std::vector<Begin> AfterStep(entt::registry& registry, double touchPx, double dtS);

    // What a begin of `body` would carry, after AfterStep: for a contact the port
    // judges elsewhere, as Demolish judges a stone against what it breaks. False
    // where BeforeStep kept nothing for `body`.
    bool Carried(entt::entity body, Begin& out) const;

    // Whether the last AfterStep found `body` touching `other`: a pair that met
    // on that step or before it, so any begin of theirs has been read already.
    bool Touching(entt::entity body, entt::entity other) const;

    void Reset();

private:
    using Pair = std::pair<entt::entity, entt::entity>;
    struct Pre {
        entt::entity body = entt::null;
        Kind kind = Kind::Player;
        glm::dvec2 velocityPx{0.0};
        glm::dvec2 centrePx{0.0}; // its transform
        glm::dvec2 atPx{0.0};     // its entity point
        glm::dvec2 pointOffsetPx{0.0};
        std::vector<glm::dvec2> recentPx; // newest first, for the steps to come
        std::vector<glm::dvec2> readPx;   // as this step's begins read them
        glm::dvec2 dropPx{0.0};           // Watched::spawnDropPx while it holds; 0 after
        bool standing = false;            // the dropped shape has come to stand on something...
        double standYPx = 0.0;            // ...with its centre here, where the original's stays
        bool stepped = false;             // a step has left it at afterPx
        glm::dvec2 afterPx{0.0};          // its transform as the last step left it
    };

    std::vector<Pre> m_pre;
    std::vector<Pair> m_touching;     // sorted
    std::vector<entt::entity> m_seen; // sorted: looked at last time
};

} // namespace MagicPortals::Impacts
