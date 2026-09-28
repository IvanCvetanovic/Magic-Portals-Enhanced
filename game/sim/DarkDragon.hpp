#pragma once

// Level 4-32's dark dragon: the game's fourth boss, and the last level of it.
//
// THE WHOLE LEVEL IS ONE MECHANISM, and every piece of it is something this port
// already owns:
//
//   - level31c is `darkest` and holds an unlit torch. Shooting it lights it,
//     which takes the light wall away (Torch) and - in the same act - arms this
//     boss. The original watches for the `light_from_projectile.ent` that a lit
//     torch adds; Torch already counts exactly that event, so this reads its
//     count rather than modelling an entity whose only purpose is to be found.
//   - Three seconds later the marker arms, four seconds after that the dragon
//     arrives and `breakable_wall` comes down. That wall is already a
//     Demolish::Breakable, so the removal goes through Demolish as the torch's
//     own wall does - this level is the only one holding both kinds.
//   - It shoots fireballs at the player, on the same sight gate as level31a's
//     dragon, and they are Turrets::Fireball as every other fireball is.
//   - IT IS HURT ONLY BY FIRE, as the ghost is. level31c places no fire agent and
//     no fire diamond: the only fire in the level is the BARREL BOMB, which is
//     teleportable, and the only thing that can light it is the dragon's own
//     fireball. It restocks that bomb after every shot (addEntityIfItCantBeFound,
//     NOT BUILT: without it the port's 4-32 has one bomb for three wounds). The
//     boss supplies its own ammunition and its own detonator, and darkdragon.json
//     has the decode.
//   - Each wound takes your portals away, and as it ends the original fires at the
//     torch ONLY if `light_off.ent` exists - a torch its fire signal put out - and
//     relights it (ins 1675-1694). The port fires at the torch lit or not, and a
//     lit one it puts out, bringing the light wall back: a gameplay divergence
//     filed for the owner (planning step 99).
//   - At hp 0 it drops a YELLOW KEY, and level31c places the only yellow keyhole
//     and locked door and no key of its own. The fight is the lock, as it is for
//     the ghost.
//
// IT IS A SENSOR WITH NO SOLID BODY (dark_dragon.ent: sensor 1, density 0,
// gravityScale 0), so it cannot touch the player and keeps a position rather than
// an entity - the ghost's shape. That is also why Game tests the blast against it
// by hand: Fire::Blast matches what it grabs by body, and this boss has none.
//
// ITS POINT IS THE CALLBACK'S, IN THE CALLBACK'S ORDER (the visuals plan's 12.1,
// planning step 99; ETHCallback_dark_dragon, bytes 389418..397662). Every update
// first calls linearMotion (ins 390-399), which puts the entity at the
// `originalPos` the LAST update wrote plus a vertical swing; then the arm of its
// state reads a WaypointManager whose legs are eased by each waypoint's own
// filter (sim/Ease) and writes this update's `originalPos` - plus, in FIGHTING, the
// neck's lift. So the point a blast is measured against and a shot leaves from
// hovers, eases and lags the path by one update, as the original's does. Its
// angle is the waypoints' in APPEARING, DAMAGE and DYING and, in FIGHTING, an
// angle the original's Follower lags toward the player (sim/Follower); the angle
// places the mouth a shot leaves from. The picture - cell, angle, emissive and
// the mouth's particle system - is the Look the arms write, which the layer draws
// as the dark_dragon.ent the summon adds.
//
// NOT BUILT, and recorded in data/darkdragon.json: every sound, the scattered
// small explosions of DAMAGE and DYING, the death's explode burst, and the
// ghost_utility_spawn.ent dropped beside the key. The earthquakes are the layer's
// (earthquake.json), read from `phase`, the APPEARING `path`'s waypoint and bias,
// `fired` and `gone`.

#include "sim/Ease.hpp"
#include "sim/Follower.hpp"
#include "sim/Motion.hpp"
#include "sim/Roles.hpp"
#include "sim/Tscn.hpp"

#include <cstddef>
#include <string>
#include <vector>

#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace MagicPortals::DarkDragon {

// The callback's own four, and 1 is never used.
enum class Phase : int { Appearing = 0, Fighting = 2, Damage = 3, Dying = 4 };

// Where one of APPEARING's waypoints stands (darkdragon.json appearing.waypoints[].at).
enum class WaypointAt {
    Here,       // GetPositionXY() when the path is built: the spawn, for APPEARING
    Shout,      // the `shout` marker
    ShoutMoved, // shout + shoutMove
    Fighting,   // the `fighting` marker
};

// One APPEARING addWaypoint call: Waypoint(pos, time, FloatColor(-1), filter, angle).
// Its colour is white and never read: APPEARING calls no SetEmissiveColor.
struct AppearWaypoint {
    WaypointAt at = WaypointAt::Here;
    double ms = 0.0;
    Ease::Filter filter = Ease::Filter::Linear;
    double angleDeg = 0.0;
};

// The port's darkdragon.json.
struct Rules {
    std::string spawnName;    // dark_dragon_spawn
    std::string entityName;   // dark_dragon.ent, which no level file places
    std::string wallName;     // breakable_wall
    std::string shoutName;    // shout
    std::string fightingName; // fighting
    std::string keyPosName;      // key_pos
    std::string platformPosName; // platform_pos
    std::string keyColour;       // yellow
    std::string keyEntity;       // key.ent: what the death adds at key_pos, which the layer draws
    std::string platformEntity;  // single_block_plat_no_emissive.ent
    std::string torchEntity;     // light_off.ent: what a wound's shot seeks, an unlit torch

    double armMs = 0.0;    // 3000: the light stands this long before the marker arms
    double summonMs = 0.0; // 4000: and this long again before the dragon arrives

    double radiusPx = 0.0; // 110, half of dark_dragon.ent's 220 x 220 Collision
    int maxHp = 0;         // 3

    double fireIntervalMs = 0.0;     // 4200
    double fireIntervalStepMs = 0.0; // 600 shorter for each wound taken
    double muzzlePx = 0.0;           // 95
    // The muzzle's angle, rad(-GetAngle() - 110 + 29) (ins 913-922, 1622-1631):
    // the mouth dark_dragon.ent's particle system stands at, turned with the head.
    double muzzleSubDeg = 0.0; // 110
    double muzzleAddDeg = 0.0; // 29
    // addFireball's fireballScale, 2 here and 1 for every other caller: it scales
    // fireball.ent's FIRST particle system alone and nothing else of the entity
    // (darkdragon.json's source note). It rides on the fireball it makes
    // (Turrets::Fireball::particleScale), because the fireball outlives the call.
    double fireballParticleScale = 1.0;
    // The mouth's particle system in FIGHTING: killed every update while the fire
    // clock is below interval / killBelowDivisor, restarted every update while it
    // is above interval / 4 x playAboveQuarters (ins 829-847, uint arithmetic).
    unsigned int killBelowDivisor = 0; // 2
    unsigned int playAboveQuarters = 0; // 3

    // The Follower that lags the FIGHTING angle (ins 343-356): stride and rate in
    // whole ms, and computeDarkDragonAngle's PIb x aimOffsetPib (398213..398405).
    unsigned int followerStrideMs = 0; // 800
    unsigned int followerRateMs = 0;   // 150
    double aimOffsetPib = 0.0;         // 1.2
    double seedAddDeg = 0.0;           // 360: APPEARING -> FIGHTING only (ins 665-679)
    // The neck (ins 734-818): lift = (smoothBothSides(clamp((top - A) / span)) x 2 - 1)
    // x heightPx x min(neckMs / rampMs, 1), on the point the next update stands at.
    double neckHeightPx = 0.0; // scale(18)
    double neckTopDeg = 0.0;   // 365
    double neckSpanDeg = 0.0;  // 25
    double neckRampMs = 0.0;   // 2000

    // linearMotion(this, vertical, 0, PIb) with dark_dragon.ent's speed and stride.
    double hoverSpeed = 0.0;      // 1.6 radians a second
    double hoverStridePx = 0.0;   // 7
    double hoverStartAngle = 0.0; // PIb

    std::vector<AppearWaypoint> appear; // seven, summing to 7400 ms
    glm::dvec2 shoutMovePx{0.0};        // (8, -32)
    int mouthLeg = 0;                   // 3: the waypoint the jaw opens on
    double appearMouthSteps = 0.0;      // 14: clamp(uint(smoothBeginning(bias) x 14), 0, 3)

    // The picture's own facts (dark_dragon.ent): the cell it is added on, its last
    // cell, its <EmissiveColor>, and the depth the callback holds it at.
    int startFrame = 0;          // 2
    int lastFrame = 0;           // 3
    glm::dvec3 emissive{0.0};    // (0.15, 0.15, 0.15)
    double zUnits = 0.0;         // 0 from its second update on (darkdragon.json look)

    double damageFirstMs = 0.0;   // 1000
    double damageFlashMs = 0.0;   // 300
    int damageFlashes = 0;        // 8 (R20)
    glm::dvec2 damageMovePx{0.0}; // (32, -32)
    double damageAngleAddDeg = 0.0; // 27
    Ease::Filter damageFilter = Ease::Filter::SmoothBothSides;
    glm::dvec3 damageRed{0.0};    // FloatColor(0xFFFF0000)
    double damageBrightTimes = 0.0; // 3: the bright legs are the .ent's emissive times this
    double damageMouthSteps = 0.0;  // 7

    double dyingHoverMs = 0.0;   // 3000
    double dyingDriftMs = 0.0;   // 6000
    glm::dvec2 fallMovePx{0.0};  // (32, -64)
    double fallPx = 0.0;         // 256
    double dyingAngleAddDeg = 0.0; // 30
    Ease::Filter dyingFilter = Ease::Filter::SmoothBothSides;
    glm::dvec3 dyingEmissive{0.0}; // FloatColor(-1): white
    double dyingMouthSteps = 0.0;  // 14
};

bool LoadRules(const std::string& path, Rules& out, std::string& error);

// Whether a boss_spawn node is THIS boss. By entity name, as Ghost::Plays is:
// `dark_dragon_spawn` is a marker and carries no `entity` field for Boss::Plays
// to read.
bool Plays(const Rules& rules, const Tscn::Node& node);

// The WaypointManager an arm builds, as it plays (WaypointManager.angelscript,
// bytes 40760..42634): a FrameTimer that moves on AT MOST ONE waypoint a call, its
// first call restarting it at waypoint 0 with no time, and a point interpolated
// from this waypoint into the next by THIS waypoint's filter - the position, the
// angle and the colour alike.
struct Path {
    struct Point {
        glm::dvec2 atPx{0.0};
        double ms = 0.0;
        glm::dvec3 colour{1.0}; // FloatColor, rgb: what SetEmissiveColor takes
        Ease::Filter filter = Ease::Filter::Linear;
        double angleDeg = 0.0;
    };
    std::vector<Point> points;
    // Its FrameTimer, as the constructor's own set(0, 0, 0, repeat) leaves it.
    int first = 0;
    int last = 0;
    int frame = 0;
    double timeMs = 0.0;
    double strideMs = 0.0;

    // WaypointManager::update (42421..42634): FrameTimer::set(0, count - 1, this
    // waypoint's time, false) - set (23254..23650) - then setCurrentStride.
    void Update(double frameMs);
    // FrameTimer::getBias (23716..23810): min(stride, time) / max(stride, 1), raw.
    double Bias() const;
    // getCurrentPoint (41513..42094): this waypoint into the next, by this one's filter.
    Point Current() const;
    bool IsLastFrame() const { return !points.empty() && frame == last; }
};

// What the callback writes on the entity - SetFrame, SetAngle, SetEmissiveColor,
// and Kill/PlayParticleSystem(0) - which the layer draws and nothing in the
// simulation reads back.
struct Look {
    int frame = 0;
    double angleDeg = 0.0;       // Ethanon's, counter-clockwise on the screen
    glm::dvec3 emissive{0.0};
    bool systemKilled = false;   // KillParticleSystem(0): no new particles
    // PlayParticleSystem(0) this update: Kill(false) then Play, which resets every
    // particle to the entity unreleased (ETHSpriteEntity.cpp:795-811,
    // ETHParticleManager.cpp:274-286). The engine draws after the callback, so an
    // update it is called on draws none of the fire, and the first update without
    // it releases particle 0 at the mouth (the layer's updateEffects).
    bool systemRestarted = false;
};

struct State {
    Rules rules;

    bool present = false; // the level places a dark_dragon_spawn
    std::string name;
    // What the key it drops answers to (Keys::Key::name), and the depth its
    // AddScaledEntity gives it: key_pos's own vector3 (ins 1851-1878).
    std::string keyName;
    double keyZUnits = 0.0;
    // The NODE the wall stands as, resolved from the entity name the rules give:
    // Demolish holds its breakables by node name, and `breakable_wall` is what
    // the level calls the entity.
    std::string wallNode;
    glm::dvec2 spawnPx{0.0};
    glm::dvec2 shoutPx{0.0};
    glm::dvec2 fightingPx{0.0};
    bool hasKeyPos = false;
    glm::dvec2 keyPosPx{0.0};
    bool hasPlatformPos = false;
    glm::dvec2 platformPosPx{0.0};
    // The node the level ALREADY places for the same entity, used as the
    // template for the one the death adds: its shape is the box and its sprite
    // is the picture. level31c places three single_block_plat_no_emissive nodes,
    // and there is no .png in entities/ for that entity - only the .ent - so a
    // picture has to be borrowed from a sibling rather than loaded by name.
    std::string platformNode;
    // The platform once it has been built, so the layer can draw it and the
    // level's unload can take it away.
    entt::entity platformBody{entt::null};

    // THE SUMMON. `armed` is the marker's own `destroy` flag, which deletes
    // nothing: it only says the light has been seen.
    bool armed = false;
    double armingMs = 0.0;
    bool summoned = false;
    double summoningMs = 0.0;

    // THE DRAGON, once it is there.
    bool alive = false;
    bool gone = false;
    // Its first update has run the init block (ins 15-375), which the tick after
    // the summon is: the summon's tick draws it as dark_dragon.ent comes.
    bool initialised = false;
    glm::dvec2 atPx{0.0};   // the entity's point: the blast is measured to it, the shots leave from it
    glm::dvec2 originPx{0.0}; // `originalPos`: where the next update's linearMotion stands it
    Phase phase = Phase::Appearing;
    int hp = 0;
    double elapsedMs = 0.0; // its firing clock, `elapsedTime`
    double neckMs = 0.0;    // `neckAdjustElapsedTime`
    Path path;              // the WaypointManager the current arm plays
    Follower::Point follower; // the FIGHTING angle, in radians, carried in x
    Motion::Linear hover;     // linearMotion's custom data
    Look look;

    // Set from OUTSIDE, by the same Game block that burns a crate: a blast within
    // reach sets this, and Tick reads and clears it exactly as healBurn does. The
    // dragon is not a scene entity and so is never a Fire::Burnable.
    bool burned = false;

    int fired = 0;
    int hits = 0;

    // How long it holds the interval now: 4200, then 3600, then 3000.
    double FireIntervalMs() const;

    // What a tick asks Game to do on its behalf, as Ghost::Turn does.
    struct Turn {
        bool brokeWall = false;   // Demolish should take `breakable_wall` away
        bool killedPortals = false; // PortalManager::killAll
        bool firedAtTorch = false;  // a fireball aimed at the level's torch
        bool fired = false;         // and one aimed at the player
        glm::dvec2 firePx{0.0};
        glm::dvec2 aimPx{0.0}; // a UNIT vector; Game gives it the fireball's speed
        bool droppedKey = false;
        glm::dvec2 keyPx{0.0};
        // AND THE PLATFORM, which this port could not build until it had a way
        // to place a body with no node behind it. There was deliberately no flag
        // here while that was true, because a flag Game could not act on is a
        // hole shaped like a check; LevelBuilder::BuildStaticBox is that way, so
        // the flag exists now and Game acts on it.
        bool droppedPlatform = false;
        glm::dvec2 platformPx{0.0};
    };

    // One tick, after the physics step - it judges where the step left the
    // player, as the ghost does. `torchLit` is Torch::State::lit > 0, which is the
    // light the original's marker looks for; `torchPx` is where that torch stands,
    // for the shot a wound sends at it; `completed` is isFinished; `motion` is
    // motions.json's linearMotion arithmetic (its frame cap and its wrap).
    Turn Tick(entt::registry& registry, entt::entity player, bool torchLit, const glm::dvec2& torchPx,
              bool hasTorch, bool completed, const Motion::Rules& motion, float dt);

    bool Alive() const { return present && alive && !gone; }
};

// computeDarkDragonAngle (398213..398405): getAngle(normalize(dragon - character))
// + PIb x offset, in radians and unclamped. getAngle is atan2(x, y), X FIRST,
// wrapped into [0, 2 PI) (GameMath.h:711-715).
double AimRadians(const Rules& rules, const glm::dvec2& dragonPx, const glm::dvec2& characterPx);

// Where a shot leaves from at Ethanon angle `angleDeg`: the entity's point less
// scale(95) x (sin a, -cos a), a = rad(-angle - 110 + 29) - the mouth, turned with
// the head (ins 913-957).
glm::dvec2 MuzzlePx(const Rules& rules, const glm::dvec2& atPx, double angleDeg);

// The neck's lift for a smoothed angle and a neck clock (ins 760-798), +y down.
double NeckLiftPx(const Rules& rules, double angleDeg, double neckMs);

// The level's dark_dragon_spawn and the four markers it reads by name. A level
// with no spawn gets an empty State, which is every level but one.
bool Find(const Tscn::Scene& scene, const Roles::Table& roles, const Rules& rules, State& out, std::string& error);

} // namespace MagicPortals::DarkDragon
