#pragma once

// Rolling stones, and what they break: the Godot project's Demolisher
// (hazards.gd:97-120), with one rule the owner settled.
//
// - A stone (role demolisher) breaks whatever breakable thing it touches - a
//   breakable_wall only when it BEGINS to touch it above demolish.json `wall`'s
//   speed, and then a stone whose `destroyable` is 1 dies with it
//   (ETHBeginContactCallback_breakable_wall, decoded whole in planning step 175).
//   Breaking takes the entity away entirely, collision included, as
//   Hazards.destroy frees the node. A stone is not a hazard that kills where it
//   stands, and the player may stand beside one (level 1-9 spawns the player 58 px
//   from one) - but one that BEGINS to touch the player at a hitIntensity above
//   demolish.json `stone.kills_player_above` kills him, as the original's stone
//   callback sets his hp to 0 (planning step 160; HitPlayer).
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

#include "sim/Impacts.hpp"
#include "sim/LevelBuilder.hpp"
#include "sim/Roles.hpp"
#include "sim/Trigger.hpp"
#include "sim/Tscn.hpp"

#include <string>
#include <utility>
#include <vector>

#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace MagicPortals::Demolish {

// The port's demolish.json.
struct Rules {
    std::vector<std::string> breakableNames; // entity names breakable without the flag
    double contactMarginPx = 0.0;
    // The entities whose own callback deletes them once a blast has set their
    // `destroy` datum: demolish.json `blast.entities`, either spelling.
    std::vector<std::string> blastEntities;
    // ETHBeginContactCallback_rolling_stone #107-129: the hitIntensity a stone's begin against the
    // player must be ABOVE to set his hp to 0 (demolish.json `stone.kills_player_above`).
    double killsPlayerAbove = 0.0;
    // ETHBeginContactCallback_breakable_wall #16-33: the speed, in metres a second of the ORIGINAL's own physics, a
    // body's whole velocity must be STRICTLY ABOVE for the wall it begins to touch to break (demolish.json
    // `wall.breaks_above_m_s`; 1.5 = 75 level units a second).
    double wallBreaksAboveMs = 0.0;
    // The entities that callback is bound to, in either spelling (demolish.json `wall.entities`).
    std::vector<std::string> wallEntities;
};

bool LoadRules(const std::string& path, Rules& out, std::string& error);

// Whether a stone whose begin against the player reads `hitIntensity` kills him: STRICTLY above the rule's number
// (#115-116: CMPIf, JNP skips).
bool KillsPlayer(const Rules& rules, double hitIntensity);

struct Stone {
    std::string name;
    entt::entity body = entt::null;
    // The stone's `destroyable` datum is exactly 1 (a placed stone's node metadata, or the 1 entity_launcher sets on every
    // stone it throws, #200-209): the wall it breaks destroys it too (ETHBeginContactCallback_breakable_wall #42-60).
    bool destroyable = false;
};

// A stone the wall's callback destroyed, as it stood when the step left it: the layer draws rolling_stone_destroy there.
struct Taken {
    Stone stone;
    glm::dvec2 atPx{0.0};
};

// One begin of a stone against a breakable_wall, as the callback judged it (for the log and the pins). The original's callback runs once,
// inside the step; the port reads that one contact at up to two instants (planning step 175, fix 2): when the stone first comes within
// reach of the wall (`atMargin`: the fastest it carried toward it, over the steps since it last met or left something) and when
// the stones' watch finds the pair begun within Box2D's skin (`atSkin`: the free flight's replay where the stone came through the air,
// Impacts::Begin::Approaching), which is the later one. Both are the engine's solver's braked estimates of one velocity, so the
// larger judges.
struct Contact {
    std::string stone;
    std::string wall;
    double speedMs = 0.0;  // the whole velocity's length it judged by, metres a second of the original's physics: the larger reading
    bool broke = false;    // above the gate: the wall was destroyed
    bool stoneDied = false; // and the stone was destroyable
    bool atMargin = false;  // the stone came within reach of the wall on this tick
    bool atSkin = false;    // and/or the watch found the pair begun on this tick
    bool braked = false;    // the first look was the engine's brake: the stone stood short of the wall but its incoming approach would have
                            // reached it in the step (see Tick's `simS`); not within the margin
    double historyMs = 0.0; // the history's reading at the margin (0 when not atMargin)
    double replayMs = 0.0;  // the watch's begin's reading (0 when not atSkin)
    bool freeFlight = false; // that begin was read as a free flight (replayed)
    int replayedSteps = 0;   // the steps the replay went on past the engine's brake
};

struct Breakable {
    std::string name;
    std::string entity;   // its entity, as Roles::EntityName reads it
    entt::entity body = entt::null;
    Trigger::Box box;     // the shape's bounds, in world metres, not grown
    bool flagged = false; // by metadata/breakable, rather than by name
    // Its entity's contact callback is ETHBeginContactCallback_breakable_wall (rules.wallEntities): a stone breaks it above
    // a speed, at a begin, and a destroyable one dies with it. Any other breakable keeps the touch rule.
    bool wall = false;
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
    // A stone's begin against the player at a hitIntensity above rules.killsPlayerAbove (HitPlayer): the stone's
    // name. Game::AfterStep folds it into Hazards, as every other death is.
    bool playerKilled = false;
    std::string killedBy;

    // The stone `stone`'s begin against the player, read at `hitIntensity`: kills him where KillsPlayer says so
    // (the first to do so names the death). Returns whether this call killed him.
    bool HitPlayer(const std::string& stone, double hitIntensity);

    // This tick's begins of a stone against a breakable_wall, and the stones the callback destroyed (Tick clears both).
    // A destroyed stone is kept in `takenNow` until the next Tick, so FindStone still answers for the tick's latches.
    std::vector<Contact> contactsNow;
    std::vector<Taken> takenNow;
    // The (wall body, stone body) pairs that were within reach after the last Tick: a first look is a pair that is not.
    std::vector<std::pair<entt::entity, entt::entity>> touching;

    // One tick, after the physics step: each stone breaks what it touches. A breakable_wall is judged at the BEGIN of a stone's
    // touch, by the speed the stone carried into the contact - the whole length of the velocity, over SimRatio (which the watch's
    // Watched::velocityScale has already divided out) - above rules.wallBreaksAboveMs, strictly; it takes a destroyable stone with
    // it. The begin is read at two instants and the larger reading judges (see Contact): when the stone first comes within
    // rules.contactMarginPx of the wall's box (State::touching), by the fastest it had toward the wall since it last met or left
    // something (`watch`, Impacts::Watch::Carried: the engine's solver brakes a closing pair before it meets, so the speed going
    // into the last step is low); and when the stones' watch finds the pair begun within Box2D's skin (`begins`: the watch's
    // AfterStep of this step, taken before anything after it moves or takes a body, so the wall still stands), by what Box2D's
    // own step reads of a stone that came through the air (Impacts::Begin::Approaching: the free flight's replay) - the layer's impact
    // watch reads the same contact so, on the same step. A second look only ever breaks a wall the first held. Any other
    // breakable keeps the touch rule. The wall's body goes and its handle is kept, which the layer's impact watch matches the
    // stone's contact by. Returns the bodies of the stones destroyed, for the caller's Forget.
    //
    // `simS` is what the original's physics world advances in the step this tick stands for (ScriptClock::Step::simS), and with it the
    // first look also takes a stone the engine's brake stopped SHORT of the wall (planning step 218). The engine moves a body and then
    // solves its contacts, and a pair apart but inside the two bodies' travel in the step is braked, so a stone that rolls at a wall
    // ends the step 1 to 6 units from it with its velocity taken out: it never comes within contactMarginPx, and the creep that
    // finally does reads 0.1 m/s, so a wall that Box2D's step would have broken at the stone's full speed held (1-22's second wall).
    // Box2D takes the position first too, and finds the pair touching at the START of the next step with the velocity the stone
    // still carries: so a stone whose incoming approach along the line to the wall, over this step, reaches the wall's reach
    // (contactMarginPx) from where the step left it is read now, a step before Box2D would, by what it carried. 0 (the default) is
    // the touch rule as it was, which the suites' rigs that step nothing keep.
    std::vector<entt::entity> Tick(entt::registry& registry, const Impacts::Watch& watch, const std::vector<Impacts::Begin>& begins,
                                   double simS = 0.0);

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
