#pragma once

// The loose bodies whose step the engine takes in Box2D's order (planning step 197, CMP-18): the contact solve first, then
// the move.
//
// The original steps Box2D once a frame at 1/30 s, and Box2D integrates positions AFTER the velocity solve (b2Island::Solve:
// velocities, then the contact constraints, then x += v h). The engine does the other: it moves a body by the velocity it
// carried into the step and only then takes the approach out of its contacts (PhysicsSystem::Update), so every contact acts
// a step late. Over a sphere rolling down a shallow slope that costs it most of its speed - 2-29's stone creeps at 0.28 units a
// frame where the original's creeps at 1.053 (the rig clip, out/parity/footage/F18_stone_2-29.mp4) - and the engine's creep
// depends on the step where Box2D's does not (docs/planning/2026-09-11-magic-portals-remaster.md, step 197).
//
// The engine carries that order as a per-body opt-in, RigidBodyComponent::integrateAfterSolve, default off. WHICH bodies take
// it is data here, not code (game/data/contact_order.json), by the level's file name and the nodes' names, because the order
// was measured on one level and is not known to be right for any other body: a stone that rests at the foot of a slope is still
// under-driven by it, and a widening is a data edit with its own measurement behind it.

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include <entt/entt.hpp>

namespace MagicPortals::ContactOrder {

struct Rules {
    // By the level's file name without its extension ("level28a"): the nodes whose bodies are stepped in Box2D's order, in
    // the order the table lists them.
    std::unordered_map<std::string, std::vector<std::string>> byLevel;
};

// Strict: a table that is not an object of non-empty lists of non-empty, distinct names is an error that names the level, so
// a typo cannot quietly leave a body in the engine's order. An empty "levels" object is a valid table (no body takes it, which
// is the control the exes are compared against).
bool LoadRules(const std::string& path, Rules& out, std::string& error);

// Sets the flag on each body the level's entry names, from the nodes the builder made (node name to entity). False, with
// `error` naming the level and the node, when a listed node is not one the builder made, was built as a static body or an area
// (it has no rigid body to step), or is a kinematic mover (the port moves it; the solver has no order to give it): each is said
// in its own words, because a name the level does not have must be fixed in the table and never skipped.
bool Apply(entt::registry& registry, const Rules& rules, const std::string& level,
           const std::unordered_map<std::string, entt::entity>& built, std::string& error);

// The table names only levels that exist. LoadRules and Apply see one level at a time (and a scratch scene in another
// directory, or a copy of a listed level under another file name, is the unkeyed control and must load), so this is the
// check against the whole directory of converted levels: false, with `error`, at the first key (in name order) that has no
// `<key>.tscn` in `levelsDirectory`.
bool CheckLevels(const Rules& rules, const std::filesystem::path& levelsDirectory, std::string& error);

} // namespace MagicPortals::ContactOrder
