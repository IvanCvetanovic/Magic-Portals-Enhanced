#pragma once

// Level 2-32's dragon: the game's third boss, and the only one that cannot be
// killed.
//
// The beholder (Boss.hpp) is a fight, the ghost (Ghost.hpp) is a fight, and this
// is a CHASE. There is no hp, no damage arm and no death arm in
// ETHCallback_dragon, and the node carries no <Body> - so nothing can hurt the
// dragon and the dragon cannot touch the player. Level 2-32 is won by reaching
// its exit while the dragon strafes it and the claw eats the floor behind.
//
// THREE PARTS, and the third is the one that shapes the level:
//   - THE FLIGHT. It enters off the left edge at (-164, -84), advances 45 px a
//     second, sinks toward a cruising height, and once past the `take_off` marker
//     climbs away and stops taking part.
//   - THE FIREBALLS. Only while it has NOT taken off, only every 4 seconds, and
//     only when nothing solid stands between it and the player. Those are
//     Turrets::Fireball - addFireball is the carranca's own function - so Game
//     pushes them into the level's list and Turrets::Tick flies and judges them,
//     as it does a fire diamond's. The original states that gate as "the closest
//     contact is the character"; the port states it as "no body before the
//     endpoint", because the player is a capsule and Shot::FirstBody does not
//     walk capsules. Same gate, and dragon.json says why it is phrased the other
//     way round.
//   - THE CLAW. It destroys the level's platforms at the CAMERA's left edge.
//
// WHY THIS MODULE OWNS A CAMERA. The claw's every number is measured from
// GetCameraPos(), and level31a is the one level in all 128 that sets
// `auto_camera` - to "dragon.ent". AutoCameraController::update is a hard lock to
// the followed entity's x, clamped to the level, with y pinned to 0. So the
// camera IS this boss's own position, and the module derives it rather than
// having one threaded through Game::Tick. dragon.json carries the decode, and the
// evidence that GetCameraPos() is the view's TOP-LEFT corner rather than its
// centre - which is what puts the claw's margins at the left edge of the screen.
//
// The view's WIDTH is the one quantity from outside, and only the clamp uses it:
// it does not bite until x > 2177, past take_off at 2160. It defaults from
// dragon.json so the sim stands up with no window, and the layer overwrites it.
//
// WHERE IT LOOKS, and it is not at you. The angle comes from a Follower
// (Follower.hpp): a point that LAGS the player by about 1.2 s, which the callback
// re-samples every 200 ms and eases toward over 2000 ms. The head turns toward
// that trailing point, clamped into a 30-degree band, and - the owner's standing
// ruling R19 - THE FIREBALL FLIES ALONG THE SAME LAGGED DIRECTION. Against a
// still player the two agree; against a moving one the shot goes where the player
// was. The sight gate is unaffected: it is still a segment to the LIVE player.
//
// NOT BUILT, and recorded in data/dragon.json: the sounds, the claw's easing
// curves (nothing reads the claw's position), and the knight - which is a
// cutscene that runs only after the level is won.

#include "sim/Follower.hpp"
#include "sim/LevelBuilder.hpp"
#include "sim/Roles.hpp"
#include "sim/Tscn.hpp"

#include <cstddef>
#include <string>
#include <vector>

#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace MagicPortals::Dragon {

// The port's dragon.json.
struct Rules {
    // The flight (ETHCallback_dragon).
    std::string entityName;      // dragon.ent
    std::string takeOffName;     // take_off
    std::string knightSpawnName; // dragon_knight_spawn
    double advancePxS = 0.0;     // 45
    double climbPxS = 0.0;       // -60, once past take_off
    double sinkPxS = 0.0;        // 25, while above its cruising height
    double hoverCeilingPx = 0.0; // 48
    double bobAmplitudePx = 0.0; // 13
    double bobFrequency = 0.0;   // 0.1

    // The Follower that lags the angle's target (Follower.hpp; decode D7).
    glm::dvec2 followerStartPx{0.0};     // the raw (2, 0) of ins 44-45, in level units
    unsigned int followerStrideMs = 0;   // 2000
    unsigned int followerRateMs = 0;     // 200
    glm::dvec2 followerTakeOffPx{0.0};   // scale(128, 64), the destination after take-off

    // The angle it draws itself at (ins 241-261): clamp(deg(getAngle(aim)) + fix -
    // sub, min, max), where aim is the dragon LESS the lagged point.
    double angleFixDeg = 0.0; // 110
    double angleSubDeg = 0.0; // 10
    double angleMinDeg = 0.0; // 340
    double angleMaxDeg = 0.0; // 370

    // The fireballs.
    double fireIntervalMs = 0.0; // 4000
    double muzzlePx = 0.0;       // 95
    double muzzleSubDeg = 0.0;   // 110: a = rad(-angle - 110 + 29)
    double muzzleAddDeg = 0.0;   // 29

    // The picture, which its own callback drives (ins 531-625). Drawn only: the
    // layer reads the cell, and the two windows also play and kill the mouth's
    // particle system.
    int columns = 0;              // dragon.ent's SpriteCut x, 2
    int rows = 0;                 // and y, 2
    int startFrame = 0;           // level31a's spriteFrame for it, 2
    double animOpenFromMs = 0.0;  // 3000: the mouth opens, 0 -> frames-1
    double animCloseFromMs = 0.0; // 500: and closes, frames-1 -> 0
    double animSpanMs = 0.0;      // 400, both ways
    int animFrames = 0;           // 4

    // The claw (ETHCallback_dragon_claw).
    std::string clawName;   // dragon_claw.ent
    double scanMs = 0.0;    // 250: the sweep's own clock
    double armPx = 0.0;     // 94: re-arm the swing and remember the height
    double crushPx = 0.0;   // 64: destroy
    double leadPx = 0.0;    // -100: how far left of the camera it hangs
    double liftPx = 0.0;    // 72
    double clawBobAmplitudePx = 0.0; // 13
    double clawBobFrequency = 0.0;   // 0.06
    std::vector<double> holdMs;      // six, summing to 1500
    std::vector<glm::dvec2> pathPx;  // six, the last two identical
    std::vector<double> anglesDeg;   // six; drawn only
    std::vector<std::string> crushes; // the four names isDestroyableByClaw accepts

    // The camera, and the only number from outside the sim.
    double viewWidthPx = 0.0; // 455 at 16:9, _inferred as view.json is
};

bool LoadRules(const std::string& path, Rules& out, std::string& error);

// Whether a boss_spawn node is THIS boss. TWO entity names answer true, which is
// the difference from Boss::Plays and Ghost::Plays: level31a places `dragon.ent`
// and `dragon_knight_spawn`, both under boss_spawn, and a port that claimed only
// the first would leave the level reading as not played while playing perfectly.
// dragon.json says on what ground the knight is claimed.
bool Plays(const Rules& rules, const Tscn::Node& node);

// A platform the claw may take: one of the four names, as the level placed it.
// Scenery, every one - none of the four is in entity_roles.json - so nothing but
// LevelBuilder::Built holds it, which is what makes it safe to destroy.
struct Crushable {
    std::string name;
    entt::entity entity = entt::null;
    glm::dvec2 atPx{0.0};
    bool gone = false;
};

// WaypointManager as this one callback uses it: six points, non-repeating,
// parked on the second-to-last so that the idle pose is motionless.
struct Claw {
    int waypoint = 0;
    double inWaypointMs = 0.0;
    double scanMs = 0.0;
    double heightPx = 0.0; // the y of the last platform that armed it

    // Parked at getNumWaypoints() - 2, which is where the callback starts it.
    void Park(const Rules& rules);
    void Reset();
    void Advance(const Rules& rules, double ms);
    bool IsLastFrame(const Rules& rules) const;

    // Where it is drawn, given the camera's top-left. Presentation: no gameplay
    // reads this, which is why the easing curves are recorded and not applied.
    glm::dvec2 AtPx(const Rules& rules, const glm::dvec2& cameraLeftPx) const;
    double AngleDeg(const Rules& rules) const;
};

// The angle the callback draws itself at, given its aim - the UNIT vector from the
// lagged point TO the dragon (ins 241-261):
//
//     A = clamp(radianToDegree(getAngle(aim)) + 110 - 10, 340, 370)
//
// with `getAngle(v) = atan2f(v.x, v.y)` wrapped into [0, 2 PI) (GameMath.h:711-715).
// For a target to the right at a depression of phi below the horizontal in y-down
// pixels that works out as A = clamp(370 - phi, 340, 370): level or above draws
// +10 degrees, 10 degrees below draws 0, and 30 or more below - or anything behind
// it - draws -20. A positive Ethanon angle turns the sprite counter-clockwise on
// the screen (D7 section 5, VERIFIED down the GLES2 path).
//
// Taken out of Tick so that the map itself can be pinned: the argument order of
// atan2 is the one mistake here that a capture cannot catch, because atan2(y, x)
// would rotate every reading by 90 degrees and still land inside the clamp.
double AngleFor(const Rules& rules, const glm::dvec2& aim);

// What ETHCallback_dragon does to the PICTURE on one tick (ins 531-625), as a rule
// of its own so that the cell and the mouth's particles cannot disagree: both the
// frame the layer draws and the play/kill of the system come from this one call.
//
// The whole block sits inside `if (!tookOff)` (ins 531-534), so a dragon that has
// climbed away keeps whatever cell it last had and its system stays as it was.
//   3000 < e < 3400          PlayParticleSystem(0), frame = clamp(int((e-3000)/400 x 4), 0, 3)
//   (500 < e < 900) || isFinished || tookOff
//                            KillParticleSystem(0), frame = clamp(3 - int((e-500)/400 x 4), 0, 3)
// The three-way `||` is read from ins 584-598; its `tookOff` leg can never be true
// inside the block above it. `e` only ever returns to 0 when the dragon fires.
struct Look {
    bool setsFrame = false; // SetFrame ran this tick
    int frame = 0;          // with this cell
    bool plays = false;     // PlayParticleSystem(0) ran this tick
    bool kills = false;     // KillParticleSystem(0) ran this tick
};
Look LookAt(const Rules& rules, double elapsedMs, bool tookOff, bool completed);

struct State {
    Rules rules;

    bool present = false; // the level places a dragon.ent
    std::string name;
    glm::dvec2 atPx{0.0};      // where it is DRAWN, which is what the camera follows
    glm::dvec2 virtualPx{0.0}; // and the position the flight integrates
    bool hasTakeOff = false;
    glm::dvec2 takeOffPx{0.0};
    bool tookOff = false;
    double elapsedMs = 0.0;

    // The lagged point the head turns toward, and the fireball flies at (R19).
    Follower::Point follower;
    // Ethanon's angle, counter-clockwise on the screen: what SetAngle was last
    // given, in [angleMinDeg, angleMaxDeg]. It is computed from the position this
    // tick STARTED at, because the original writes the move after the angle.
    double angleDeg = 0.0;
    // normalize(dragonPos - followerPos) as that same line built it: the head's
    // aim, and the fireball's flies along its negation.
    glm::dvec2 aim{0.0};

    // THE PICTURE, drawn only, as Claw::AtPx is: the cell its callback last set
    // and whether its system is killed. `restarted` is true on the ticks
    // PlayParticleSystem(0) ran, which resets every particle to the mouth.
    int frame = 0;
    bool systemKilled = false;
    bool systemRestarted = false;

    bool hasClaw = false;
    Claw claw;
    std::vector<Crushable> crushables;

    double boundsX = 0.0;     // the level's `max`, for the camera's clamp
    double viewWidthPx = 0.0; // seeded from the rules; the layer may overwrite it

    int fired = 0;
    int crushed = 0;

    // The camera's TOP-LEFT corner, in the remake's pixels: a hard lock to the
    // dragon's x, clamped into the level, with y at 0. AutoCameraController.
    glm::dvec2 CameraLeftPx() const;

    // What a tick asks Game to do on its behalf, as Ghost::Turn does: Game owns
    // the registry's bodies and the level's fireball list.
    struct Turn {
        std::vector<entt::entity> crushed;    // platforms the claw took
        std::vector<std::string> crushedNames; // and their node names, for Built
        bool fired = false;
        glm::dvec2 firePx{0.0}; // where the fireball starts
        glm::dvec2 aimPx{0.0};  // a UNIT vector; Game gives it the fireball's speed
    };

    // One tick, before the physics step: it sets positions, it spits as
    // Turrets::Fire does, and it takes geometry away before the solver reads it.
    // `completed` is GameStateController::isFinished, which gates the firing.
    Turn Tick(entt::registry& registry, entt::entity player, bool completed, float dt);
};

// The level's dragon, its claw, the take_off marker, the level's far corner and
// every platform the claw may take. A level with no dragon.ent gets an empty
// State, which is every level but one.
//
// A whitelist node that is not in `built` is skipped rather than refused: the
// landing check builds no statics (Game::Start's withStatics), and there is then
// nothing for the claw to take.
bool Find(const Tscn::Scene& scene, const Roles::Table& roles, const LevelBuilder::Built& built, const Rules& rules,
          State& out, std::string& error);

} // namespace MagicPortals::Dragon
