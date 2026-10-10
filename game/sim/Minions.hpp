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
// 16 px fallback Trigger::FromNode would hand back, which a destroyier still kills
// in (a death area kills on its own polygon since planning step 136, as the
// original's sensor does). A kill floor shrunk to 16 px would miss everything that
// fell past it.
//
// SIGHT, built in planning step 158 (G1a; out/parity/program/G1a/decode.md has the whole state machine). A knight looks
// every frame (findEntityInSight): the entities in the three 128-unit buckets of its row that its facing runs
// through, inside a 120 degree cone, with nothing between them and it but another knight or the player. It does not turn
// or run at what it sees: it stands (its dwell clock reset), shows an exclamation mark and plays a sound the first time,
// and waits to shoot. It also casts a ray to the waypoint it walks to, and a way that is blocked is not walked: it
// searches, looking right and then left, with a question mark every 4000 ms. Blind on a `darkest` level (the ambient is
// DARKEST_AMBIENT_LIGHT), it sees a portal and not the player, and the player's body blocks its way.
//
// SHOOTING, built in planning step 159 (G1b; out/parity/program/G1b/decode.md). When its gate opens a knight calls
// addEnemyShoot: an enemy_shoot.ent is added at its point, flying 120 units a second of the script's clock toward where the
// sighted entity stood, 16 units out to begin with, with no body - it meets nothing on the way. It is judged by the STORED
// target alone (ETHCallback_enemy_shoot looks the target up by id): within 16 units of it, the player has hp set to 0 and a
// portal is killPortal'd, and the shot spends itself; a target that is gone leaves it flying on to its destiny, where it also
// ends. Contact with the player caps a knight's own hold at 1000 ms (ETHBeginContactCallback_minion #12-55), the one thing the
// player's body does to it.
//
// And a minion being breakable: minion.ent carries breakable=1, burnable=1 and teleportable=1; teleportable is wired, since a
// portalled minion landing on the killer floor is how the "kill all minions" achievements are won, and so is burnable against
// a fireball (Burn, planning step 98: the black knight's death by fire is what adds chapter 3's ghost, R4), a fire agent's
// flame (planning step 134, a tick later: Fire::State::minionsAlight), a barrel's blast (planning step 159: explode() destroys
// and burns what its grabber takes) and a rolling stone's crush (Game::Crush, the stone's own begin-contact callback). data/
// minions.json records the rest against what it would take to build them.

#include "sim/LevelBuilder.hpp"
#include "sim/Roles.hpp"
#include "sim/ScriptClock.hpp"
#include "sim/Trigger.hpp"
#include "sim/Tscn.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace MagicPortals::Minions {

// What a knight sees by, and when: minions.json `sight`, every number cited there.
struct Sight {
    double fovCos = 0.0;      // findEntityInSight's 0.5: a target is seen above it
    double bucketUnits = 0.0; // the scene's bucket, 128 level units
    std::string targetPlayer; // 'dark_mage.ent'
    std::string targetPortal; // 'portal.ent'
    // GetClosestContact's ignore lists, by entity name as spelled (ETHEntityNameArrayChooser).
    std::vector<std::string> sightRayIgnores; // the look's ray, agent to target
    std::vector<std::string> wayRayIgnores;   // the walk's ray, knight to waypoint
    std::vector<std::string> wayRayIgnoresWhenSeeing; // added to the walk's while canSeeAnything
    double exclamationStartMs = 0.0;
    double exclamationGateMs = 0.0;
    double questionStartMs = 0.0;
    double questionGateMs = 0.0;
    double searchPeriodMs = 0.0;
    double searchRightAfterMs = 0.0;
    double markAboveUnits = 0.0;  // the mark's y offset above the knight
    double markBehindUnits = 0.0; // and its z offset (the vector3's z, subtracted)
    // addEnemyShoot's gate (ETHCallback_minion #769-842): the timers that move the knight, not the shot itself.
    double waitNearMs = 0.0;
    double waitFarMs = 0.0;
    double nearUnits = 0.0;
    double movingSpeed = 0.0;
    double movingAddMs = 0.0;
    double minIntervalMs = 0.0;
};

// What an enemy_shoot.ent is (minions.json `shot`; the same numbers as ghost.json's, which is the same callback).
struct ShotRules {
    std::string entityName;    // 'enemy_shoot.ent'
    double speedPxS = 0.0;     // 120 units a second of the script's clock
    double hitPx = 0.0;        // fakeRadius = scale(16)
    double nudgePx = 0.0;      // its first call moves it this far along its way
    int firstCell = 0;         // _projectileFrameTimer.set(first, last, stride, true)
    int lastCell = 0;
    double cellStrideMs = 0.0;
};

// What the player's body does to a knight (minions.json `contact`): ETHBeginContactCallback_minion #12-55.
struct ContactRules {
    double holdCapMs = 0.0;  // holdTime = min(holdTime, 1000)
    double touchUnits = 0.0; // a pair has begun to touch within this of the surfaces (earthquake.json impacts.touch_px)
};

// What a rolling stone does to a knight it hits (minions.json `crush`): ETHBeginContactCallback_rolling_stone #131-168.
struct CrushRules {
    std::string entity;     // rolling_stone.ent: the callback's owner
    double hitAbove = 0.0;  // hitIntensity must exceed this
};

// The port's minions.json.
// The body is a circle where the original's is a capsule of two, and it is built
// with z rotation LOCKED: minion.ent is fixedRotation="1", and a circle that may
// turn rolls under friction instead of stopping, which would carry a guard
// through the post it is meant to stand at. Minions::State::Spawn says so where
// it does it.
struct Rules {
    // The walk's own two literals: scale(1.3), which is METRES a second of the original's physics (the
    // port's metre is 50 units), and the 66 ms its frame is capped at. What a tick carries is
    // WalkCarried, off the step - the physics step's 33.33 ms is clock.json's, not this file's.
    double speed = 0.0;
    double elapsedCapMs = 0.0;
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
    Sight sight;
    ShotRules shot;
    ContactRules contact;
    CrushRules crush;

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
    Hold,    // serving its hold: braked, and its standing frame set (#1183-1205: nothing is in sight)
    Settled, // a lone guard inside the wider threshold: left alone
    Watch,   // braked as Hold is, with something in sight: the frame is NOT set (#1184-1190)
    Search,  // its way to the waypoint is blocked and it has seen nothing: it looks right, then left (#1229-1321)
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

    // WHAT IT SEES BY (ETHCallback_minion's custom data; planning step 158).
    //
    // moveVec: the direction it is looking, (+-1, 0), written in the walk arm (even where the lone guard is
    // left alone: #1028 sets it before shouldntMove is asked) and in the search arm, and absent - (0, 0) - until
    // then. findEntityInSight reads it BEFORE the walk arm writes it, so it looks the way it looked a frame ago, and a
    // knight whose moveVec is still (0, 0) sees nothing ('Warning: enemy aims nowhere').
    glm::dvec2 moveVec{0.0, 0.0};
    bool spotted = false;      // targetSpotted
    int spotCount = 0;         // enemySpotCount: every sighting that began with targetSpotted at 0
    double exclamationMs = 0.0; // exclamationElapsedTime, from Sight::exclamationStartMs
    double questionMs = 0.0;    // questionElapsedTime, from Sight::questionStartMs
    double shootMs = 0.0;       // shootElapsedTime
    bool wayWasClear = false;   // the 'isInSight' datum: set whenever the way is clear, cleared by each question mark
    bool wayClear = false;      // this tick's probe: the ray to the waypoint met nothing
    bool seeing = false;        // this tick's findEntityInSight found something (before isFinished is asked)
    // ETHBeginContactCallback_minion's begin: whether the player's body touched this knight at the last judging, so that a
    // pair that is touching now and was not then is a begin (the hold cap runs on that edge alone).
    bool touchingPlayer = false;
};

// What a knight can see this tick, which the caller gathers: ETHCallback_minion asks the engine for
// GetAmbientLight(), the scene's entities and g_stateController.isFinished(); the simulation hands them over.
struct Senses {
    // GetAmbientLight() != DARKEST_AMBIENT_LIGHT (Lighting::Ambient is the port's one function for the ambient).
    bool canSeeAnything = true;
    // GameStateController::isFinished: the level is won or the player is dead.
    bool finished = false;
    // dark_mage.ent, as an entity the buckets hold: its GetPositionXY() is the capsule's centre.
    bool playerPresent = false;
    glm::dvec2 playerPx{0.0, 0.0};
    entt::entity playerBody = entt::null;
    // The two circles of dark_mage.ent's body (centre, radius), which Shot::ClosestContact does not read: the
    // probe of a blind knight is blocked by them (the player's name leaves its ignore list).
    struct Circle {
        glm::dvec2 centrePx{0.0, 0.0};
        double radiusPx = 0.0;
    };
    std::array<Circle, 2> playerBodyCircles{};
    // Every placed portal.ent, oldest first: no body, a point. Each has the id Portals::Placed gave it, which is what a shot
    // aimed at it is judged by (the original stores the target's entity id).
    std::vector<glm::dvec2> portalsPx;
    std::vector<int> portalIds;
};

// What a tick's knights added, for the layer and for G1b: cleared by every Tick, as `deaths` is.
struct Mark {
    enum class Kind { Exclamation, Question };
    std::size_t minion = 0;
    Kind kind = Kind::Exclamation;
    // The knight's ENTITY point (GetPosition()); the mark is added Sight::markAboveUnits above it and
    // Sight::markBehindUnits less in z.
    glm::dvec2 entityPx{0.0, 0.0};
    // playEnemySpottedSound with the exclamation, always; playMinionQuestionMarkSound with the question only when
    // the mark's point is on screen (the layer's test) AND the way had been clear since the last one.
    bool sound = false;
    bool soundNeedsScreen = false;
};

// The shot a knight is due to fire (addEnemyShoot's arguments, #869-884): the hook for G1b.
struct ShotDue {
    std::size_t minion = 0;
    glm::dvec2 casterPx{0.0, 0.0}; // the knight's entity point
    glm::dvec2 finalPx{0.0, 0.0};  // the target's
    std::string targetName;        // 'dark_mage.ent' or 'portal.ent'
    int portalIndex = -1;          // the placed portal's place in Senses::portalsPx; -1 for the player
    int portalId = -1;             // and its Portals::Placed id, which the shot keeps; -1 for the player
};

// One enemy_shoot.ent in the air (addEnemyShoot, ETHCallback_enemy_shoot): a point that flies straight toward `destinyPx` and
// is judged against its STORED target alone. It has no body and meets nothing on the way.
struct EnemyShot {
    int id = 0;                    // counted from 1 over the level, never reused: what the layer follows
    std::size_t minion = 0;        // the knight that fired it (its place in State::minions)
    glm::dvec2 atPx{0.0, 0.0};
    glm::dvec2 velocityPx{0.0, 0.0}; // units a second of the script's clock: direction x ShotRules::speedPxS
    glm::dvec2 directionPx{0.0, 0.0}; // its `direction`: normalize(destiny - casterPos), zero if they are the same point
    glm::dvec2 fromPx{0.0, 0.0};   // casterPos, and initialPos: where its range is measured from
    glm::dvec2 destinyPx{0.0, 0.0}; // finalPos: where the sighted entity stood as it was fired
    std::string targetName;        // 'dark_mage.ent' or 'portal.ent'
    int targetPortalId = -1;       // the placed portal it was aimed at; -1 for the player
    // Made this tick and not yet flown: its first callback is the next frame's, which adds the nudge's step (the port nudges it
    // as it is made, and flies it from the next tick).
    bool fresh = true;
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
    bool byFire = false;   // a fireball or a flame burned it (Burn): addFlames, the layer's fire32 (step 146)
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

    // What this tick's knights added: the marks they show and the shots they are due to fire. Cleared by Tick.
    std::vector<Mark> marks;
    std::vector<ShotDue> shotsDue;

    // The shots in the air, and what they have done. `shotsFired` and `shotsEnded` count every shot of the level (the layer's
    // sounds and shakes are each one per count); `spentPx` is where each shot that ended THIS tick stood as it was destroyed,
    // by reaching its destiny or its target, and `killedPortalIds` the placed portals it killPortal'd, both refilled by every
    // FlyShots. The player's death is reported, not acted on: Game folds it into Hazards as the boss's and the ghost's are.
    std::vector<EnemyShot> shots;
    int shotsMade = 0;
    int shotsFired = 0;
    int shotsEnded = 0;
    std::vector<glm::dvec2> spentPx;
    std::vector<int> killedPortalIds;
    bool playerKilled = false;
    std::string killedBy;

    // The placed bodies the knights' two rays pass through by NAME, as the original's ignore lists name them
    // (Sight::sightRayIgnores, wayRayIgnores): bound once by BindSight, from the nodes the level places. The
    // knights' own bodies are the other half of those lists, and Tick adds them as they stand.
    std::vector<entt::entity> sightIgnoredPlaced;
    std::vector<entt::entity> wayIgnoredPlaced;

    // Finds the placed bodies the two ignore lists name, by the entity name each node is spelled with ('crate_small' is
    // passed and 'crate_small.ent' is not). After Find, with the level's built bodies.
    void BindSight(const Tscn::Scene& scene, const LevelBuilder::Built& built);

    // Every sighting the `minion.ent` knights have made: spottedCount (ScoreDashboard 401234..401465), which
    // hasBeenUnnoticed reads for the seven stealth achievements. The black knight is not counted (it is a
    // ghost_minion.ent, and the original's GetEntityArray asks for 'minion.ent' alone). G1b wires it to the layer's
    // SpottedCount(); nothing in the simulation reads it.
    int SpotCount() const;

    // Every minion its marker asks for, built where the marker stands. Returns
    // the bodies it made, which the caller hands to the systems that carry a
    // body it did not build - a minion is teleportable, so Game puts it among
    // the portals' travellers, as it does what a launcher throws.
    std::vector<entt::entity> Spawn(entt::registry& registry);

    // One tick, BEFORE the physics step: each minion looks, then serves its dwell and then
    // walks, as the player is steered before the step rather than after it. `senses` is what the engine
    // would tell its callback: the ambient light, whether the game is finished, and where the player and the
    // placed portals stand. The first form sees nothing (no player, no portal) and finds its way as a knight in
    // a lit room does.
    void Tick(entt::registry& registry, const ScriptClock::Step& step);
    void Tick(entt::registry& registry, const ScriptClock::Step& step, const Senses& senses);

    // The shots fly on, AFTER the physics step, where it left the player and the portals (ETHCallback_enemy_shoot, listing
    // 435266..436701): each moves speedPxS x the tick's script seconds along its direction, ends once it has gone FARTHER from
    // where it started than its destiny is (squaredDistance, a strict compare), and is judged against the entity it was
    // aimed at, found by its stored id: within hitPx of it (strict), the player is killed (playerKilled) and a portal's id is
    // put in killedPortalIds - and the shot ends there. A shot made this tick does not fly yet. Clears spentPx and
    // killedPortalIds first.
    void FlyShots(const ScriptClock::Step& step, const Senses& senses);

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

    // A minion taken by something that is not a killer floor: a rolling stone's crush (Game::Crush), a shock diamond's
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

};

// ETHBeginContactCallback_rolling_stone #131-168: a stone that begins to touch a knight at `hitIntensity` (dp2(its velocity,
// the normal between the two entity points) / getScale()) crushes it only ABOVE minions.json `crush.hit_above` (CMPIf, JNP skips).
bool Crushes(const Rules& rules, double hitIntensity);

// A bucket: GetBucket(p) = floor(p / bucketUnits) per axis (ETHBucketManager.cpp:47-50).
glm::ivec2 BucketOf(const glm::dvec2& point, double bucketUnits);

// What findEntityInSight returned.
struct Seen {
    glm::dvec2 atPx{0.0, 0.0};
    std::string name;      // 'dark_mage.ent' or 'portal.ent'
    int portalIndex = -1;  // its place in Senses::portalsPx; -1 for the player
};

// findEntityInSight (bytes 433047..434575) for a knight whose entity point is `agentPx` and whose last moveVec is
// `moveVec`: the first candidate, in the order the original walks its buckets (the agent's, the next, the one after;
// the player before the portals inside one), whose name is a target (the player's only while canSeeAnything), whose
// direction from the knight is above rules.sight.fovCos of the facing, and whose ray from the knight meets no body
// but `ignore` (the knights' own bodies and the named placed ones). Nothing when moveVec is not a unit vector.
std::optional<Seen> Look(entt::registry& registry, const Rules& rules, const glm::dvec2& agentPx,
                         const glm::dvec2& moveVec, const Senses& senses, const std::vector<entt::entity>& ignore);

// Whether GetClosestContact(from, to, ..., list) returns nothing: the ray meets no body but `ignore`, and - when
// `blindToPlayer` - not the player's two circles either (the walk probe of a knight that cannot see: its ignore list
// then lacks 'dark_mage.ent').
bool RayClear(entt::registry& registry, const glm::dvec2& fromPx, const glm::dvec2& toPx,
              const std::vector<entt::entity>& ignore, const Senses& senses, bool playerBlocks);

// The velocity ETHCallback_minion sets a walking minion: scale(1.3) x min(66, the script's milliseconds) /
// GetCurrentPhysicsTimeStepMS (ins 1049-1068) across its own vertical velocity (ins 1091-1110), in metres a
// second of the original's physics - 1.3 x 16 / 33.33 = 0.624, which its 1/30 s step carries 1.04 units a
// frame. And as the port's body carries it: at SimRatio, so a tick of the engine's covers what the
// original's step covers in the frame - 62.4 units a second at the decoded 60 Hz, where the unit this
// file read it in (1.3 px a 33.3 ms step, 39 a second) was a fiftieth of the metre scale() gives.
double WalkOriginal(const Rules& rules, const ScriptClock::Step& step);
double WalkCarried(const Rules& rules, const ScriptClock::Step& step);

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
