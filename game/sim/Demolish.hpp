#pragma once

// Rolling stones, and what they break: the Godot project's Demolisher
// (hazards.gd:97-120), with one rule the owner settled.
//
// - A stone (role demolisher) breaks whatever breakable thing it touches.
//   Breaking takes the entity away entirely, collision included, as
//   Hazards.destroy frees the node. A stone is not a hazard: it kills nothing,
//   and the player may stand beside one (level 1-9 spawns the player 58 px from
//   one).
// - What is breakable: an entity whose metadata/breakable is set, which is all
//   the Godot project reads, or one whose name demolish.json lists. Eight
//   breakable_wall placements carry no flag, and that is in the original's own
//   scene data, so in the Godot project a stone cannot break them. The owner played the
//   original and confirmed, on 11 September, that stones broke those walls.
//   demolish.json says so beside the list.
// - Touching is judged as the port judges its triggers (Trigger.hpp), in its
//   own tick after the step. The test is the stone's circle against the
//   breakable's box, grown by demolish.json's contact margin, because a solver
//   leaves a stone resting against a wall touching it rather than overlapping
//   it. The box is the shape's bounds, so a breakable_wall's octagon is met up to
//   its corners' 1.5 x 6.3 px chamfer early.
// - Quarter turns only. A breakable turned by any other angle is refused,
//   because its box would not be its shape. Every breakable in the game is
//   turned by 0 or by a quarter turn.
// - What the Godot project's launcher would throw is step 7. Levels 23 and 24 have a
//   breakable wall and no stone until then.
//
// THE ONE OWNER OF A BREAKABLE'S BREAKING (planning step 115). Whatever takes a
// breakable away - a stone's touch (Tick), a barrel's blast (Grab, then
// Destroyed), the torch's light wall and the dark dragon's summon (Break), 4-19's
// red key state (SeekAndDestroy, then Destroyed; step 118) - does it through this
// state, so the layer's watches (wall_destroy, its earthquake, the
// demolition sound) read one record. The blast is the original's two halves:
// explode (MiscCallbacks, listing L67590) #111-122 sets `destroy` on every entity
// its grab reaches with a clear line, and ETHCallback_breakable_wall (L68771)
// #1-86 reads it and deletes the wall. The grab takes a breakable by its FLAG
// (isBreakable, L67923: GetUInt('breakable') != 0), never by name, and only the
// entities demolish.json `blast.entities` lists read the datum here: a carranca,
// a light wall, a shock agent and a minion read it too in the original, and are
// left to their own modules (demolish.json `blast._not_built`).

#include "sim/LevelBuilder.hpp"
#include "sim/Roles.hpp"
#include "sim/Trigger.hpp"
#include "sim/Tscn.hpp"

#include <string>
#include <vector>

#include <entt/entt.hpp>

namespace MagicPortals::Demolish {

// The port's demolish.json.
struct Rules {
    std::vector<std::string> breakableNames; // entity names breakable without the flag
    double contactMarginPx = 0.0;
    // The entities whose own callback deletes them once a blast has set their
    // `destroy` datum: demolish.json `blast.entities`, either spelling.
    std::vector<std::string> blastEntities;
};

bool LoadRules(const std::string& path, Rules& out, std::string& error);

struct Stone {
    std::string name;
    entt::entity body = entt::null;
};

struct Breakable {
    std::string name;
    std::string entity;   // its entity, as Roles::EntityName reads it
    entt::entity body = entt::null;
    Trigger::Box box;     // the shape's bounds, in world metres, not grown
    bool flagged = false; // by metadata/breakable, rather than by name
    bool blastBreaks = false; // flagged, and its entity in rules.blastEntities
    // The original's `destroy` datum, set by a blast's grab and read by the
    // wall's own callback (Destroyed), with what set it.
    bool destroy = false;
    std::string destroyBy;
    bool broken = false;
    std::string brokenBy; // the stone, barrel, torch or dragon that broke it
};

struct State {
    Rules rules;
    std::vector<Stone> stones;
    std::vector<Breakable> breakables;

    // One tick, after the physics step: each stone breaks what it touches. The
    // wall's body goes and its handle is kept, which the layer's impact watch
    // matches the stone's contact by.
    void Tick(entt::registry& registry);

    // A script's deletion, now: the torch's light wall and the dark dragon's wall.
    // The body goes and the handle is cleared. Returns the body taken, for the
    // caller's Forget, or entt::null when there was none standing.
    entt::entity Break(entt::registry& registry, const std::string& name, const std::string& by);
    // And the way back (the torch's signal, shot): the wall stands again with the
    // body the builder made for it.
    void Restore(const std::string& name, entt::entity body);

    // A blast's grab (explode #111-122): `destroy` set on each breakable standing
    // whose body is `body` and which the blast breaks. Returns whether one was.
    bool Grab(entt::entity body, const std::string& by);
    // A script's destroy(SeekEntity(entity)) - 4-19's red key state, #194-220:
    // `destroy` set on the first breakable standing whose entity is EXACTLY
    // `entity` (SeekEntity compares GetEntityName() whole, ETHBucketManager.cpp:
    // 394-406, so `breakable_wall.ent` is not `breakable_wall`), flag or none - its
    // own callback reads the datum, not the flag. Destroyed then takes it. Returns
    // its node's name, or empty when none stands.
    std::string SeekAndDestroy(const std::string& entity, const std::string& by);
    // ETHCallback_breakable_wall #1-86: every breakable whose `destroy` is set is
    // taken away. Run after the blasts of the same tick, as the original runs its
    // static entities' callbacks after the dynamic ones (ETHScene.cpp:464-491).
    // Returns the bodies taken, for the caller's Forget.
    std::vector<entt::entity> Destroyed(entt::registry& registry);

    const Stone* FindStone(const std::string& name) const;
    const Breakable* FindBreakable(const std::string& name) const;
    int Broken() const;
};

// A level's stones and breakables, each with the body LevelBuilder built for it.
// A breakable with no body built is passed over, which is every static one when
// the level is started without its statics. False, with `error`, for a stone
// with no body, or a breakable turned by anything but a quarter turn.
bool Find(const Tscn::Scene& scene, const Roles::Table& roles, const LevelBuilder::Built& built, const Rules& rules,
          State& out, std::string& error);

} // namespace MagicPortals::Demolish
