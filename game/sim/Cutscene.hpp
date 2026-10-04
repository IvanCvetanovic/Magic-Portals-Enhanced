#pragma once

// What the original plays between the door and the medal (the program's P2,
// planning step 123; cutscenes.json holds the decode).
//
// A level may set how long that is: an `end_delay` entity's `time` replaces
// gameWonDelay's 1400 ms, and three levels place one - 4-01 5000, 4-19 14500 and
// 2-32 6600. Each of the three fills its delay with a cutscene of its own, run by a
// callback that starts on the frame the level is won:
//   - 4-01's rocket_launch raises the rocket door (0, -390) over 7600 ms, slides
//     the bridge left without a clamp, shakes the camera every 100 ms for 6 s and
//     plays the rocket's track;
//   - 4-19's rocket_landing sinks the rocket door (256, 400) over 12500 ms with the
//     character riding it, shrinks the key it carried, raises the eclipse to the
//     view's bottom-centre, dims the spiral and plays back_home;
//   - 2-32's dragon_knight_spawn adds a knight - once the level is FINISHED, which
//     a death is too - that falls, holds 800 ms at waypoint0, walks to waypoint1 and
//     goes into the door with the door's own effect and sound.
//
// Pure where it can be: the delay, the three paths and the knight's update over a
// registry, as sim/Minions updates a minion. What each cutscene moves is a
// PICTURE - the door, the bridge, the eclipse, the spiral's colour, the key's size
// and alpha, the camera's target - and the layer draws them; the sensors and the
// bodies stay where the level put them, as the door's bob leaves its sensor
// (motions.json door._not_built). The knight is the one body, and it is added only
// after the level is finished, outside Minions, whose list the achievements count.

#include "sim/Art.hpp"
#include "sim/Minions.hpp"
#include "sim/Tscn.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace MagicPortals::Cutscene {

// The port's cutscenes.json.
struct Rules {
    // GameStateController's constructor, #143-175.
    std::string endDelayEntity; // end_delay
    std::string endDelayKey;    // time

    // ETHCallback_rocket_launch.
    struct Launch {
        std::string entity;               // rocket_launch
        std::string door;                 // door.ent
        std::string bridge;               // rocket_bridge
        double launchMs = 0.0;            // 7600: the door and the bridge move while below it
        glm::dvec2 doorTravelPx{0.0};     // (0, -390)
        glm::dvec2 bridgeTravelPx{0.0};   // (-32, 0)
        double bridgeSpanMs = 0.0;        // 3800: the bridge's bias, which is not clamped
        double quakeRearmAboveMs = 0.0;   // 100
        double quakeBeforeMs = 0.0;       // 6000
        std::string track;                // rocket
    } launch;

    // ETHCallback_rocket_landing.
    struct Landing {
        std::string entity;               // rocket_landing
        std::string door;                 // door.ent
        std::string eclipse;              // eclipse.ent
        std::string spiral;               // spiral
        std::string key;                  // key
        double launchMs = 0.0;            // 12500
        glm::dvec2 doorTravelPx{0.0};     // (256, 400)
        glm::dvec2 eclipsePitch{0.0};     // (0.5, 1) of the screen from the camera's corner
        double keyScalePerFrame = 0.0;    // 0.9
        double spiralBrightnessFrom = 0.0; // 2
        double spiralBrightnessTo = 0.0;   // 0.1
        std::string track;                // back_home
    } landing;

    // ETHCallback_dragon_knight_spawn and ETHCallback_dragon_knight.
    struct Knight {
        std::string spawn;           // dragon_knight_spawn
        std::string entity;          // dragon_knight.ent
        std::string waypointPrefix;  // waypoint: waypoint0, waypoint1, ...
        std::string holdKey;         // hold
        double speed = 0.0;          // scale(1.7): METRES a second of the original's physics, as minions.json's
        double elapsedCapMs = 0.0;   // 66: the frame cap of the same expression
        double arrivePx = 0.0;       // scale(16)
        glm::dvec2 finishOffsetPx{0.0}; // levelFinishedEffect's door, from the knight: (16, -16)
        int walkFirstColumn = 0;     // timer.set(0, 3, 120, true)
        int walkLastColumn = 0;
        double walkStrideMs = 0.0;
        int startFrame = 0;          // dragon_knight.ent's startFrame: 4
        int columns = 0;             // its SpriteCut x: 4
        int leftRow = 0;             // 1
        int rightRow = 0;            // 2
        glm::dvec2 pivotPx{0.0};     // its PivotAdjust: (0, 0)
        glm::dvec3 colour{0.0};      // SetColor(V3_ZERO) in its first run
        glm::dvec3 emissive{0.0};    // its .ent's
    } knight;
};

bool LoadRules(const std::string& path, Rules& out, std::string& error);

// ---- the delay ------------------------------------------------------------------

// The level's end_delay time, where it places one: the first node of that entity,
// as SeekEntity finds it. Nothing where it places none or its time is not a number.
std::optional<double> EndDelayMs(const Rules& rules, const Tscn::Scene& scene);

// Whether the level places an entity of this exact name, as SeekEntity asks.
bool Places(const Tscn::Scene& scene, const std::string& entityName);

// ---- 4-01: the launch -----------------------------------------------------------

// Where the rocket door and the bridge are, from where the level put them, at
// endElapsedTime `ms`: start + travel x smoothBeginning(ms / span), unclamped.
struct LaunchPose {
    glm::dvec2 doorOffsetPx{0.0};
    glm::dvec2 bridgeOffsetPx{0.0};
};
LaunchPose LaunchAt(const Rules& rules, double ms);

// The callback's clock and what it last wrote.
struct Launch {
    bool running = false;       // its first frame has run
    double elapsedMs = 0.0;     // the door's endElapsedTime
    double quakeCounterMs = 0.0; // its eqCounter
    LaunchPose pose;            // the last pose written: it stops moving at launchMs
};

// What one update did, besides moving the pose.
struct LaunchStep {
    bool started = false; // its first frame: playRocketMusic
    bool quake = false;   // startEarthquake(3, 0)
};

// One update of ETHCallback_rocket_launch on a won frame of `frameMs`.
LaunchStep StepLaunch(const Rules& rules, Launch& launch, double frameMs);

// ---- 4-19: the landing ----------------------------------------------------------

// smoothBeginning(ms / 12500), the landing's one bias.
double LandingBias(const Rules& rules, double ms);
// The door's originalPos from where the level put it: travel x bias.
glm::dvec2 LandingDoorOffsetPx(const Rules& rules, double bias);
// The eclipse's y: the camera corner's y plus the screen's height times the
// pitch, plus its own height times 1 - smoothEnd(bias). Its x is the one it has.
double LandingEclipseYPx(const Rules& rules, double bias, double cornerYPx, double viewHeightPx,
                         double eclipseHeightPx);
// What the spiral's kept colour is multiplied by: 1 until smoothEnd(bias) passes
// 0.526, 0.1 at the end.
double SpiralBrightness(const Rules& rules, double bias);

struct Landing {
    bool running = false;
    double elapsedMs = 0.0; // the door's endElapsedTime, added to only while below 12500
    double bias = 0.0;      // this update's
    double keyScale = 1.0;  // what Scale(0.9) has done to the key, a frame at a time
    int keyScalings = 0;    // how many times it has: the layer scales the key's systems by as many
};

struct LandingStep {
    bool started = false; // its first frame: playBackHomeMusic
};

// One update of ETHCallback_rocket_landing on a won frame of `frameMs`.
LandingStep StepLanding(const Rules& rules, Landing& landing, double frameMs);

// ---- 2-32: the knight -----------------------------------------------------------

struct Waypoint {
    std::string name;
    glm::dvec2 atPx{0.0};
    double holdMs = 0.0; // its GetUInt('hold'): 0 where it carries none
};

struct Knight {
    // What the level places: whether it has a spawn, where, and waypoint0, 1, ...
    // as SeekEntity finds them, up to the first that is missing.
    bool placed = false;
    glm::dvec2 spawnPx{0.0};
    std::vector<Waypoint> waypoints;

    // The spawn's callback, once the level is finished.
    bool spawned = false;
    entt::entity body = entt::null;
    // The knight's own callback.
    bool initialised = false;
    bool gone = false;           // it went into the door and deleted itself
    std::size_t dest = 0;
    std::size_t currentDest = 0;
    bool hasStarted = false;
    double elapsedMs = 0.0;
    double holdMs = 0.0;
    Art::FrameTimer timer;
    int column = 0;              // the frame it shows: its startFrame until a SetFrame
    int row = 0;
    bool walked = false;         // this update set its velocity
    // The update it found no next waypoint: where it stood, for levelFinishedEffect.
    bool entered = false;
    glm::dvec2 enteredAtPx{0.0};
};

// The level's knight, where it places a spawn. False, with `error`, only for a
// spawn whose waypoint0 is missing: the knight would stand at its spawn for ever.
bool FindKnight(const Rules& rules, const Tscn::Scene& scene, Knight& out, std::string& error);

// ETHCallback_dragon_knight_spawn's AddScaledEntity: the knight's body, minion.ent's
// circle with its ENTITY point on the spawn, rotation locked. Once.
entt::entity SpawnKnight(const Rules& rules, const Minions::Rules& body, entt::registry& registry, Knight& knight);

// One run of ETHCallback_dragon_knight, of the script's milliseconds of `step`, before the physics step. The
// first sets it up; the one that finds no next waypoint destroys the body and says
// where (entered, enteredAtPx). Its walk is carried as every body is (ScriptClock::CarriedVelocity).
void StepKnight(const Rules& rules, const Minions::Rules& body, entt::registry& registry, Knight& knight,
                const ScriptClock::Step& step);

// The knight's walking velocity, in metres a second of the original's physics and as the port carries it.
double KnightWalkOriginal(const Rules& rules, const ScriptClock::Step& step);
double KnightWalkCarried(const Rules& rules, const ScriptClock::Step& step);

// Its entity point, the one GetPositionXY reads: the body's centre less
// Minions::Rules::EntityAboveCentrePx. Zero when it has no body.
glm::dvec2 KnightEntityPx(const Minions::Rules& body, entt::registry& registry, const Knight& knight);

} // namespace MagicPortals::Cutscene
