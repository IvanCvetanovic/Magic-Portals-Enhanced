#pragma once

// Chapter 3's boss: the ghost of level 3-32, and the fight the fire diamond
// exists for.
//
// The game's SECOND boss, and it shares nothing with the first. The beholder
// (Boss.hpp) glides, throws rolling stones, is hurt by a stone rising through it,
// fires rings of spikes and raises a button when it dies. The ghost flies a
// waypoint path, shoots the player, is hurt by FIRE, summons a minion when hurt,
// and drops a KEY when it dies. Nothing in Boss::State - no rocks, no spikes, no
// button - serves any of that, so this is its own module and test_mp_boss's 126
// checks are left alone.
//
// FIVE states, and their numbers are the callback's own first six instructions:
// APPEARING 0, SHOOTING 1, RAGE_MODE 2, DAMAGE 3, DEATH 4, GHOST_RADIUS 64. The
// init writes hp 3.
//
// HOW IT IS HURT: by a barrel's blast, and by nothing else (planning step 117).
// The SHOOTING arm ends with isBurned(this): burned, it goes to DAMAGE, takes
// hp += -1, calls healBurn(this) and sounds. `burned` is set from OUTSIDE, by Game,
// when a blast reaches it (BlastReaches: explode's EntityGrabber and its ray), and
// Tick reads and clears it exactly as healBurn does. A fireball passes through it:
// ghost.ent is a sensor and ETHBeginContactCallback_fireball returns on a sensor
// that is not a shock agent (#46-121), which F15 filmed. The barrel is the one the
// ghost itself puts back at barrel_spawn on every beat (Turn::restock), and the fire
// diamond is what sets it off.
//
// WHEN IT IS THERE, which is the owner's standing ruling R4 (planning step 98).
// The level places ghost_pos, and that is only where the ghost FLIES TO: the
// ghost.ent is added by the black knight's death (ETHCallback_ghost_minion
// #35-104), at the knight's point, one level unit behind it and at scale 0.3, and
// only then does it run: its init block builds the five appear waypoints and
// starts the boss music, and its APPEARING arm flies it from the knight's point to
// ghost_pos, black to white and 0.3 to 1 (ghost.json appear). Before that there is
// no ghost - no shot, no restock, nothing a blast can reach. Game spawns it,
// from Minions::State::deaths, whatever killed the knight.
//
// ITS POINT MOVES, and it is one point. APPEARING writes it from the waypoints and
// SHOOTING and RAGE_MODE from linearMotion's hover (motions.json ghost); DAMAGE and
// DEATH leave it where it is. A shot leaves from it and the key is dropped at it,
// so the simulation keeps it, where every other linearMotion in the port is the
// layer's picture alone.
//
// WHAT IT REPORTS RATHER THAN DOES. Tick returns a Turn: summon a minion here,
// drop a key here, put a barrel and a fire diamond back. Minions owns minion
// bodies, Keys owns keys, Diamonds owns diamonds and Restock what a boss adds, and
// each walks its own list every frame - so Game funnels, as it does for every other
// module that reaches across.
// Its LOOK (colour, alpha, scale, cell) is what the callback writes on the
// entity; nothing in the simulation reads it back, and the layer draws it.
//
// Not built, and recorded in data/ghost.json: the death blast's grab and the static
// body a portal shot cannot cross. (Its sounds are the layer's since planning step
// 142; its scattered explosions, its death's explode picture at 2.0 and the
// restock's ghost_utility_spawn.ent the layer's since step 146, effects.json.)

#include "sim/Minions.hpp"
#include "sim/Motion.hpp"
#include "sim/Restock.hpp"
#include "sim/Roles.hpp"
#include "sim/Tscn.hpp"

#include <cstddef>
#include <string>
#include <vector>

#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace MagicPortals::Ghost {

// BEHOLDER-style state constants, but the ghost's own five.
enum class Phase : int { Appearing = 0, Shooting = 1, Rage = 2, Damage = 3, Death = 4 };

// Where one of the init block's waypoints stands (ghost.json appear.waypoints[].at).
enum class WaypointAt {
    Spawn,   // currentPos: the ghost's own point at its init, the knight's
    Quarter, // midPoint: currentPos + (ghost_pos - currentPos) x mid_fraction
    Anchor,  // destPos: ghost_pos
};

// The filter a leg is eased by: the CURRENT waypoint's.
enum class Filter {
    Linear,    // defaultFilter(v) = v
    SmoothEnd, // smoothEnd(v) = sin(v x PIb)
};

// One addWaypoint call: Waypoint(pos, time, FloatColor(argb), filter, scale).
struct WaypointRule {
    WaypointAt at = WaypointAt::Spawn;
    double timeMs = 0.0;
    glm::dvec4 colour{1.0};    // FloatColor(uint): r, g, b and a, each 0..1
    Filter filter = Filter::Linear;
    bool scaleIsSpawn = false; // GetScale().x at the init
    double scale = 1.0;        // else this
};

// bounce(): from one uniform scale to the other and back, a stride each way,
// eased by smoothEnd, on the entity's one blinkElapsedTime.
struct Bounce {
    double from = 1.0;
    double to = 1.0;
    double strideMs = 0.0;

    double ScaleAt(double elapsedMs) const;
};

// The port's ghost.json.
struct Rules {
    std::string anchorName; // ghost_pos: the boss_spawn node, matched by NAME
    int maxHp = 0;

    // What a blast is measured against (ghost.json blast). explode's EntityGrabber
    // tests its sphere against the .ent's Collision box at the entity's point, by
    // the global scale alone; its ray then has to meet the ghost's own Box2D body
    // first, which was built at the entity's scale when it was added (spawnScale)
    // and never rebuilt. So two boxes, both from ghost.ent's one Collision.
    glm::dvec2 grabBoxPx{0.0};    // Collision Size, as the grabber reads it
    glm::dvec2 bodyBoxPx{0.0};    // Collision Size, before the add's scale
    glm::dvec2 bodyOffsetPx{0.0}; // Collision Position, before the add's scale

    // ins 594-630: the barrel it puts back on every SHOOTING beat (sim/Restock).
    Restock::Rule restock;

    // What adds it (spawn): the entity whose death adds it, the depth it is added
    // at over that entity's own (scale(-1), level units), its scale at the init,
    // and the level entity the same death deletes, by its id.
    std::string spawnedBy;
    double spawnZOffsetUnits = 0.0;
    double spawnScale = 0.0;
    int deletesEntityId = 0;

    double midFraction = 0.0;          // midPoint's quarter
    std::vector<WaypointRule> waypoints; // the five, in the order they are added
    double seedElapsedMs = 0.0;          // SHOOTING starts with its clock already at the interval

    double intervalMs = 0.0; // between shots, and between restocks
    glm::dvec2 muzzlePx{0.0};

    double shotSpeedPxS = 0.0;
    double shotHitPx = 0.0;
    double shotNudgePx = 0.0; // how far out a shot starts: its first callback's AddToPositionXY
    // _projectileFrameTimer.set(first, last, stride, true), every call of every shot
    int shotFirstCell = 0;
    int shotLastCell = 0;
    double shotCellStrideMs = 0.0;

    double toRageMs = 0.0;      // DAMAGE holds this long before RAGE_MODE
    double summonWindowMs = 0.0; // and RAGE_MODE summons inside this

    std::string waypointPrefix;  // wayA
    std::string summonAnchorName; // minion_spawn_point, which carries no role
    std::string redLineName;      // red_line: the x the player must be past
    std::string diamondSpawnName; // fire_diamond_spawn

    double burstMs = 0.0; // DEATH waits this long, then drops the key
    std::string keyEntityName; // key.ent: what DEATH adds, which the layer draws
    std::string keyColour;

    // What each arm writes on the entity (ghost.json look).
    int shootingFrame = 0;
    int rageFrame = 0;
    double rageAlpha = 1.0;
    double alphaLeavingRage = 1.0;
    double damageAlpha = 1.0;
    double deathAlpha = 1.0;
    Bounce damageBounce;
    Bounce deathBounce;
};

bool LoadRules(const std::string& path, Rules& out, std::string& error);

// Whether a boss_spawn node is THIS boss. Unlike Boss::Plays - which reads an
// adder's metadata/entity - ghost_pos carries no `entity` field at all and the
// entity NAME is the whole of it. Mirroring Boss::Plays literally would answer
// false for every level in the game.
bool Plays(const Rules& rules, const Tscn::Node& node);

// One of the ghost's shots: enemy_shoot.ent, which only ever kills the player.
struct Shot {
    int id = 0;               // counted from 1 over the level, never reused: what the layer follows
    glm::dvec2 atPx{0.0};
    glm::dvec2 velocityPx{0.0};
    glm::dvec2 directionPx{0.0}; // its `direction`, a unit vector
    glm::dvec2 fromPx{0.0};      // casterPos: the muzzle, where its range is measured from
    glm::dvec2 destinyPx{0.0};   // finalPos: where the player stood as it was fired
    double flownPx = 0.0;        // from the muzzle, the nudge included
    double rangePx = 0.0;
};

// The patrol the summoned escort walks is Minions::Waypoint and not a type of
// this module's own. It is an ORDINARY minion patrol - the original's rage arm
// calls the same spawnMinion a marker does, with a prefix a marker could have
// carried - so a second identical struct would be two names for one thing, and
// Minions::Summon would need a conversion to say so.

// The WaypointManager the init block builds, as it plays (ghost.json appear).
struct Appear {
    struct Point {
        glm::dvec2 atPx{0.0};
        double timeMs = 0.0;
        glm::dvec4 colour{1.0};
        Filter filter = Filter::Linear;
        double scale = 1.0;
    };
    std::vector<Point> points;
    // Its FrameTimer, as the constructor's own set(0, 0, 0, repeat) leaves it.
    int first = 0;
    int last = 0;
    int frame = 0;
    double timeMs = 0.0;
    double strideMs = 0.0;

    // WaypointManager::update: FrameTimer::set(0, count - 1, this frame's time,
    // false), then setCurrentStride(the new frame's time).
    void Update(double frameMs);
    // getCurrentPoint: this waypoint into the next, by this one's filter.
    Point Current() const;
    bool IsLastFrame() const { return !points.empty() && frame == last; }
};

// What the callback writes on the entity: SetColor, SetAlpha, SetScale, SetFrame.
struct Look {
    glm::dvec3 colour{1.0}; // the .ent's own, white, until its init writes one
    double alpha = 1.0;
    double scale = 1.0;
    int frame = 0;
};

struct State {
    Rules rules;

    bool present = false; // the level places a ghost_pos
    std::string name;     // the node's name
    std::string keyName;  // what the key it drops answers to: its name and "#key"
    glm::dvec2 anchorPx{0.0}; // ghost_pos: where it flies to and hovers about
    // Where it is: the knight's point from the tick it is added, then where each
    // arm puts it. Before then it is ghost_pos and nothing reads it (Alive()).
    glm::dvec2 atPx{0.0};

    // R4. The knight died and the ghost.ent is added (Spawn), and its first
    // callback - the init block - has run.
    bool spawned = false;
    bool initialised = false;
    glm::dvec2 spawnPx{0.0};
    // The node the same death deletes (Rules::deletesEntityId): the level's node
    // whose name carries that id, found by Find. Empty where the level has none.
    std::string deletesNode;

    Appear appear;
    Look look;
    double blinkMs = 0.0;          // bounce's blinkElapsedTime
    Motion::Linear hover;          // linearMotion's custom data
    glm::dvec2 hoverOriginPx{0.0}; // its originalPos, written by its first call

    bool hasRedLine = false;
    double redLineX = 0.0;
    bool hasSummonAnchor = false;
    glm::dvec2 summonPx{0.0};
    bool hasDiamondSpawn = false;
    glm::dvec2 diamondSpawnPx{0.0};
    std::vector<Minions::Waypoint> patrol; // what the summoned escort walks
    // Where its barrel is put back (barrel_spawn) and what the port builds it from:
    // level31b places no barrel, so the template is the port's table's. Resolved by
    // Game::Start, which holds that table.
    Restock::Site restockSite;

    Phase phase = Phase::Appearing;
    int hp = 0;
    double elapsedMs = 0.0;
    bool burned = false; // set by Game when a blast reaches it; cleared here
    bool summonedThisRage = false;
    bool gone = false;

    int shotsFired = 0;
    int shotsMade = 0; // every shot of the level: the ids
    int hits = 0;      // times it was burned
    int summons = 0;   // minions it called
    // SHOOTING's beats, each of which ends in playGhostItemAppearSound whatever
    // its restocks added (ins 670-672). Counted for the layer's sound (planning
    // step 142); nothing in the simulation reads it.
    int beats = 0;
    std::vector<Shot> shots;
    // Where each shot that ended on this tick stood as it was destroyed - in its
    // range or on the player - cleared and refilled by every Tick, for the layer
    // (the explosion it leaves): nothing in the simulation reads it.
    std::vector<glm::dvec2> spentPx;

    // Reported, not acted on: Game::AfterStep folds this into Hazards, as the
    // boss's, the carrancas', the fire's and the rings' are.
    bool playerKilled = false;
    std::string killedBy;

    // What a tick asks Game to do on its behalf.
    struct Turn {
        bool summon = false; // a minion, at summonPx, on the patrol above
        bool droppedKey = false;
        glm::dvec2 keyPx{0.0};
        // A fire diamond back at its spawn when none is left: on SHOOTING's beat
        // (ins 632-667), and on every RAGE_MODE update while the player lives
        // (ins 1034-1083).
        bool restockDiamond = false;
        // A barrel bomb back at barrel_spawn when none is found (ins 594-630): on
        // SHOOTING's beat, whether or not a shot leaves - the red line gates the
        // shot alone (F15 finding 3).
        bool restock = false;
        bool initialised = false; // its init block ran this tick: the boss music, the appear effect
    };

    // ETHCallback_ghost_minion #43-78: the ghost.ent added at the dying knight's
    // point (its GetPosition before its own callback ran). It is drawn at the
    // .ent's own look at the spawn's scale until its first callback, on the NEXT
    // Tick (ghost.json spawn._first_callback).
    void Spawn(const glm::dvec2& knightPx);

    // One tick, after the physics step. `playerAlive` is whether the player stands
    // with hp above 0 - RAGE_MODE's diamond restock asks it - `minionsStanding` how
    // many minions the level still has - RAGE_MODE waits on its escort being killed
    // - and `motion` the motions.json the hover's linearMotion runs by.
    Turn Tick(entt::registry& registry, entt::entity player, bool playerAlive, std::size_t minionsStanding,
              const Motion::Rules& motion, float dt);

    // explode's reach (MiscCallbacks, listing L67590, #33-122) for the one entity it
    // can burn that the port holds as no body: EntityGrabber's sphere of the blast's
    // radius against grabBoxPx at its point, then getFirstContactExcept's ray from
    // the blast to its point, whose closest contact must be its own body. From
    // inside that body Box2D reports no contact at all (b2PolygonShape::RayCast), so
    // a blast there does not reach it; and the ray is the port's, as Fire::Blast's
    // is: solid bodies only.
    bool BlastReaches(entt::registry& registry, const glm::dvec2& blastPx, double blastRadiusPx) const;

    bool Alive() const { return present && spawned && !gone; }
};

// The level's ghost, the markers it reads by name, and the patrol its escort
// walks. A level with no ghost_pos gets an empty State, which is every level but
// one. False, with `error`, for a ghost_pos with no position.
bool Find(const Tscn::Scene& scene, const Roles::Table& roles, const Rules& rules, State& out, std::string& error);

} // namespace MagicPortals::Ghost
