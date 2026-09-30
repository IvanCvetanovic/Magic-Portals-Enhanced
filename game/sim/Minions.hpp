#pragma once

// The minions of worlds 3 and 4: what patrols, and the floor that kills it.
//
// No minion is ever PLACED. Every one is spawned from an invisible marker -
// minion_spawn - which stamps its patrol onto the spawned entity and then
// deletes itself. The marker carries metadata/waypointName, and that is a
// PREFIX rather than a name: "wayA" means the nodes wayA0, wayA1, ... walked in
// order. ETHCallback_minion's init block counts them by seeking waypointName + n
// until one is missing, then copies each one's position and its holdTime onto
// the minion itself, which is why the walk can wrap simply by asking whether the
// next index has any data at all.
//
// holdTime is a DWELL in milliseconds, not a travel time: a minion that has
// arrived stands until its timer passes the hold, then walks on. A lone wayX0
// with the sentinel 999999999 is therefore a stationary guard, and needs no
// special case anywhere - it is just a dwell nothing will outlast.
//
// What the port takes from the decode rather than from the Godot project's guesses:
//
// - Speed is the literal 1.3, scaled, times elapsedTime / the physics step. The
//   game fixes that step: AverageFPSRateManager sets 1/30 s and calls
//   SetFixedTimeStep(true), so the divisor is 33.3333 ms and the walk is
//   33.3333 ms worth of 1.3 px - 39 px/s. The Godot project's actors.json guesses 60,
//   and says so (_guess: true).
// - Arrival is 8 px. The wider 16 px test - minDist * 2 - is NOT a second gate
//   on every walk: the original computes it only for a minion with exactly one
//   waypoint, and takes it as false for any other. It has to work that way,
//   since a minion that stopped 16 px out could never reach the 8 px it advances
//   at. So a lone guard stops being DRIVEN short of its post; a patrol walks all
//   the way in, arrives, and turns round.
// - Stopping is not one thing but two, and neither is a hard zero. A minion
//   still serving its hold has its velocity multiplied by (0.3, 1) - a brake on
//   x that decays over a few frames, with y left to gravity. One inside the
//   wider threshold is not touched at all: no velocity is set, and the body is
//   left to carry on and to friction. The port does both, which is why
//   `patrol.brake` is a rule rather than a literal zero.
// - The original clamps its frame time to 66 ms - twice the step - so below
//   15 fps it walks SLOWER rather than skipping ahead. Recorded; moot here,
//   where the port's tick is fixed.
//
// enemy_killer lives here rather than in Hazards, and that is a correction.
// Enhanced's role table files it under `hazard`, beside death_area.ent and
// lava, which would kill the PLAYER on contact. Its callback does no such thing:
// ETHBeginContactCallback_enemy_killer (android_game.bin, bytes 436701..437083)
// is 69 instructions that open with isMinion(other) and jump straight to the
// return when that is false. A non-minion touching it gets nothing at all. It is
// sensor=0 and converts to a solid StaticBody2D, so it is a floor the player can
// stand on, and it sits below the bounds of 44 levels. The minion's own callback
// agrees from the other side: it suppresses its landing earthquake and fall
// sound exactly when the thing it hit is named enemy_killer.
//
// Its box is the shape the level gives it (LevelBuilder::ShapeBoundsPx), not the
// 16 px fallback Trigger::FromNode would hand back. A hazard's box is the
// Godot project's, because the port matches the Godot project where the Godot project is what a
// player meets; this floor is not in the Godot project's path at all, and a kill floor
// shrunk to 16 px would miss everything that fell past it.
//
// NOT built here, and deliberately: sight, shooting, and a minion being
// breakable. minion.ent carries breakable=1, burnable=1 and teleportable=1;
// teleportable is wired, since a portalled minion landing on the killer floor is
// how the "kill all minions" achievements are won, and so is burnable against a
// fireball (Burn, planning step 98: the black knight's death by fire is what adds
// chapter 3's ghost, R4) and a fire agent's flame (planning step 134, a tick
// later: Fire::State::minionsAlight). An explosion's burn is not. data/minions.json records
// the rest against what it would take to build them.

#include "sim/LevelBuilder.hpp"
#include "sim/Roles.hpp"
#include "sim/Trigger.hpp"
#include "sim/Tscn.hpp"

#include <cstddef>
#include <string>
#include <vector>

#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace MagicPortals::Minions {

// The port's minions.json.
// The body is a circle where the original's is a capsule of two, and it is built
// with z rotation LOCKED: minion.ent is fixedRotation="1", and a circle that may
// turn rolls under friction instead of stopping, which would carry a guard
// through the post it is meant to stand at. Minions::State::Spawn says so where
// it does it.
struct Rules {
    double speedPxPerStep = 0.0; // the decoded literal, per physics step
    double stepMs = 0.0;         // the step it is per: SetFixedTimeStepValue(1/30)
    double arrivePx = 0.0;       // minDist: the walk advances inside this
    double settlePx = 0.0;       // minDist * 2: only a lone guard stops inside this
    double brake = 0.0;          // what a minion serving its hold multiplies x by
    double bodyRadiusPx = 0.0;   // one circle for the original's two
    double entityAboveFeetPx = 0.0; // where the original's entity stands over its feet
    std::string killerName;      // the entity name of the floor that kills a minion
    // What a marker adds (minions.json spawn): the entity spawnMinion falls back to
    // when the marker names none, and every entity a marker may name. The one body
    // above serves them all, because their .ent files state the same one.
    std::string defaultEntity;
    std::vector<std::string> entities;
    // And how spawnMinion adds it: AddEntity's angle and scale, which the layer
    // adds the entity's own particle systems at. The body above is the scale-1
    // body, so LoadRules takes no other scale.
    double spawnAngleDeg = 0.0;
    double spawnScale = 1.0;

    // The body's centre less this is the original's entity point, which arrivePx
    // and settlePx are measured from (minions.json body.entity_above_feet_px).
    double EntityAboveCentrePx() const { return entityAboveFeetPx - bodyRadiusPx; }
};

bool LoadRules(const std::string& path, Rules& out, std::string& error);

struct Waypoint {
    std::string name;          // the node's name, waypointName + index
    glm::dvec2 atPx{0.0, 0.0}; // where it stands, in the port's pixels
    double holdMs = 0.0;       // the dwell on ARRIVING here
};

// Which way a minion's last walk went: ETHCallback_minion's moveVec, whose sign
// is all its row is chosen by. None until it has walked.
enum class Facing { None, Left, Right };

// Which of the patrol's arms the last Tick took for a minion - what its picture
// is framed by (Art::UpdateMinion), and nothing the sim reads back.
enum class Arm {
    None,    // not ticked: gone, bodiless, or with no waypoints
    Walk,    // its velocity set toward its destination
    Hold,    // serving its hold: braked
    Settled, // a lone guard inside the wider threshold: left alone
};

struct Minion {
    std::string name;         // the spawner's node name; the minion answers to it
    std::string waypointName; // the prefix, not a name
    // The entity the marker ADDS, and so the one the sim holds: its
    // metadata/entityName, or Rules::defaultEntity where it names none
    // (spawnMinion #1-23). The owner's standing ruling R16 (planning step 97):
    // level31b's marker adds ghost_minion.ent, the chapter-3 boss's black knight,
    // and every other marker a minion.ent. What differs between the two is not
    // the body - minions.json spawn says why one serves both - but what is drawn
    // (art.json minion.black_knight, the layer's aura) and what the black one
    // adds when it dies (ETHCallback_ghost_minion #35-104, R4: not built here).
    std::string entity;
    bool summoned = false;    // called up by the chapter-3 boss (Summon), not spawned by a marker
    glm::dvec2 spawnPx{0.0, 0.0};
    entt::entity body = entt::null;
    std::vector<Waypoint> waypoints;
    std::size_t dest = 0;      // the waypoint being walked to
    std::size_t arrivedAt = 0; // the one last reached, whose hold is being served
    double elapsedMs = 0.0;    // since that arrival
    double holdMs = 0.0;       // adopted on arrival, from the waypoint reached
    bool gone = false;         // killed, by the floor
    Facing facing = Facing::None; // set only where the walk sets the velocity
    Arm arm = Arm::None;          // the last Tick's
};

// An invisible solid floor that destroys a minion and nothing else.
struct Killer {
    std::string name;
    Trigger::Box box;
};

// A minion destroyed on the tick just gone, for the frame to draw what the
// original adds where it died (the visuals plan's 11.3): ETHCallback_minion's
// blood.ent at the minion's own point once `destroy` has marked it (#1478-1504),
// and, where a killer floor did it, ETHBeginContactCallback_enemy_killer's
// skull.ent under it at the view's foot (#24-60).
struct Death {
    std::size_t minion = 0; // its place in State::minions
    // Its ENTITY point, the one GetPosition() reads: the body's centre less
    // Rules::EntityAboveCentrePx, taken before the body is destroyed.
    glm::dvec2 atPx{0.0, 0.0};
    bool byKiller = false; // a killer floor; otherwise a strike (Take) or a burn (Burn)
    bool byFire = false;   // a fireball or a flame burned it (Burn): addFlames, which is not built
};

struct State {
    Rules rules;
    std::vector<Minion> minions;
    std::vector<Killer> killers;

    int killed = 0; // minions taken by a killer floor
    int taken = 0;  // and by something else: a shock diamond's strike
    int burned = 0; // and by a fireball (Burn)

    // Every minion destroyed since the last Tick, in the order it went. Cleared
    // by Tick and written by Cull and Take, as Turrets::State::spentPx is cleared
    // and written: a record for the layer, which nothing in the simulation reads.
    std::vector<Death> deaths;

    // Every minion its marker asks for, built where the marker stands. Returns
    // the bodies it made, which the caller hands to the systems that carry a
    // body it did not build - a minion is teleportable, so Game puts it among
    // the portals' travellers, as it does what a launcher throws.
    std::vector<entt::entity> Spawn(entt::registry& registry);

    // One tick, BEFORE the physics step: each minion serves its dwell and then
    // walks, as the player is steered before the step rather than after it.
    void Tick(entt::registry& registry, float dt);

    // And after it: a minion left standing in a killer floor is taken. Returns
    // the bodies it took away, which the caller must pass through Forget. Split
    // from Tick for the reason Launchers::Throw and ::Cull are - what moves a
    // body and what judges where the step left it are two different moments.
    std::vector<entt::entity> Cull(entt::registry& registry);

    // A minion called up mid-run rather than placed: chapter 3's boss summons one
    // when it is hurt (Ghost.hpp). Built exactly as Spawn builds one, with the
    // same locked rotation and for the same reason, and pushed onto this list so
    // that Tick walks it, a killer floor can take it, and Game can count what
    // still stands. Returns the body, for the caller to hand to the portals as
    // Game hands what Spawn makes - a minion is teleportable. entt::null when
    // there is nothing to build it from.
    //
    // The patrol is passed IN, already resolved: the boss resolves its prefix
    // when the level is found, because Game::AfterStep has no scene to resolve a
    // waypoint name from. What it adds is a minion.ent: the rage arm hands
    // spawnMinion that literal (ETHCallback_ghost ins 1137), which is also
    // spawnMinion's own fallback, Rules::defaultEntity.
    entt::entity Summon(entt::registry& registry, const std::string& name, const glm::dvec2& atPx,
                        const std::vector<Waypoint>& patrol);

    // A minion taken by something that is not a killer floor: a shock diamond's
    // strike, which the original does with the same destroy() the floor calls.
    // Removal lives here rather than in the module that struck it, because Tick
    // walks this list every frame - a minion destroyed behind this one's back
    // would sit in it with an invalid body. Returns what it destroyed, for
    // Forget, as Cull does; a body that is not a minion of this level is ignored.
    std::vector<entt::entity> Take(entt::registry& registry, const std::vector<entt::entity>& bodies);

    // A minion a fireball reached (planning step 98). minion.ent and ghost_minion.ent
    // both carry burnable=1, and ETHBeginContactCallback_fireball calls burn() on
    // any non-sensor it touches that is (#159-173); the minion's own callback then
    // finds itself burned - isBurned, destroy, addFlames (ETHCallback_minion
    // #1456-1471) - and destroyed, and adds blood.ent and deletes itself
    // (#1478-1504). So a fireball kills a minion outright, the frame it touches it,
    // and records a Death as Take does. Returns what it destroyed, for Forget.
    // A fire agent's flame burns one too (#193-220 of its callback), and Game calls
    // this for it on the tick after, when the minion's callback would read it.
    std::vector<entt::entity> Burn(entt::registry& registry, const std::vector<entt::entity>& bodies);

    const Minion* FindMinion(const std::string& name) const;

    // What the decoded literal works out to, per second: 39, not the 60 the
    // Godot project guesses.
    double SpeedPxPerSecond() const;
};

// A level's minion markers, the waypoints each one's prefix resolves, and the
// killer floors.
//
// False, with `error`, for a marker whose waypointName is present but resolves
// nothing - that would be a converter regression, and across all 128 levels no
// prefix does: of 63 markers in 35 levels, 62 carry a prefix, 56 of those
// resolve two waypoints and 6 resolve one, which are the stationary guards.
//
// The 63rd is NOT a marker at all, and an earlier wording here had that wrong.
// It is level31b's minion_spawn_point, which carries no role in entity_roles.json
// and so is never collected below - it is chapter 3's BOSS's summon anchor, read
// by name by Ghost (Ghost.hpp). level31b's real marker, minion_spawn_908, does
// carry a waypointName ("wayB") like every other. data/minions.json said so
// correctly all along; this comment and the one in Find did not.
//
// The empty-waypointName branch in Find is therefore defensive rather than
// exercised: no marker in any of the 128 levels lacks one.
//
// And false for a marker whose entity is not one Rules::entities names: a body
// the port does not know is not built as a minion.ent's in silence. No level
// names one - level31b's ghost_minion.ent is the only entityName in the 128.
bool Find(const Tscn::Scene& scene, const Roles::Table& roles, const Rules& rules, State& out, std::string& error);

} // namespace MagicPortals::Minions
