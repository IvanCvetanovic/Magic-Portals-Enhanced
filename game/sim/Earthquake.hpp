#pragma once

// The original's earthquake: EarthquakeGenerator (Game.angelscript), the one
// camera shake every startEarthquake call in the script feeds. Decoded in
// game/data/earthquake.json, from the visuals run's decode D11 and its check.
//
// A call OVERWRITES the amplitude with clamp(0.35 x intensity x s, 0, 50) screen
// pixels, s being the screen's pixels per level unit, and does nothing else. On
// each update that is due - 70 ms or more since the last tremble - the generator
// trembles once, alternately:
//   FORTH  the camera down by the amplitude, which is saved;
//   BACK   the camera up by the SAVED value, and only then the amplitude halves.
// The updates between draw at rest, so a tremble is a one-frame spike and the
// shake is a run of them 70 ms or more apart. Below 1 px the amplitude stops.
// The axis is y alone, and nothing in the class draws a random number.
//
// Two consequences that follow from the machine as it stands, and are kept:
//   - a call between a forth and its back is halved at that back before it is
//     ever shown, because the back undoes the OLD saved value;
//   - the phase is never reset, so it carries into the next shake.
//
// Renderer-free and registry-free: a call goes in and a camera move comes out.
// The layer decides where each call lands against the update (When) and moves
// the drawn camera; nothing here reaches Game::Level.

#include <array>
#include <cstddef>
#include <limits>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace MagicPortals::Earthquake {

// Where a call stands in the original's frame against the generator's update:
// a contact callback, a dynamic entity's callback or an earlier controller
// calls before it; the game-state controller and a static entity's callback
// call after it (earthquake.json calls._when).
enum class When { BeforeUpdate, AfterUpdate };

// Every call site the port fires: one earthquake.json row each.
enum class Call : std::size_t {
    Fall,
    FallNoGravity,
    BarrelBlast,
    BeholderDeath,
    DarkDragonDeath,
    GhostDeath,
    WallStruck,
    WallDestroyed,
    StoneDespawned,
    BossRockBroken,
    BeholderHurt,
    DarkDragonMouth,
    DarkDragonShot,
    DoorLanded,
    DoorOpening,
    DoorClosing,
    FireballEnd,
    EnemyShotEnd,
    MinionKilled,
    DoorUnlocked,
    ShockDiamondPicked,
    FireDiamondPicked,
    ShockStrike,
    LightWallGone,
    // The shot's own three (the visuals plan's 10.1): a tap refused for a budget
    // of 0 while the level's no-portal sign is there, a shot an anti-projectile
    // wall kills, and one that ends inside a field.
    NoPortalTap,
    ShotBlocked,
    ShotFailedInField,
    // And a shot an anti-portal agent killed (D11 row 3; the remaster record's
    // step 171): the agent's own static callback.
    AgentKilled,
    // The bodies' own impacts (D11 rows 4, 8 and 25): the player's, a rolling
    // stone's and a minion's begin contacts, each called with its row's factor
    // times the contact's hitIntensity (Impacts.hpp finds the contacts).
    PlayerImpact,
    StoneImpact,
    MinionImpact,
    Count
};

inline constexpr std::size_t kCalls = static_cast<std::size_t>(Call::Count);

// The row's key in earthquake.json's `calls`, which is also what a log names.
const char* KeyOf(Call call);

struct CallRule {
    double intensity = 0.0; // startEarthquake's first argument
    When when = When::BeforeUpdate;
};

// The port's earthquake.json, strictly: every row and every number required.
struct Rules {
    double forceScale = 0.0;        // 0.35
    double capPx = 0.0;             // 50: a bare literal, in screen pixels
    double stopPx = 0.0;            // 1: the same
    double trembleIntervalMs = 0.0; // 70
    double backScale = 0.0;         // 0.5, on the back alone
    bool forthFirst = true;         // eqForth's first value
    double screenPxPerUnit = 0.0;   // s: 720 / 256 by R12
    std::array<CallRule, kCalls> calls{};
    // The entity whose callbacks wall_struck and wall_destroyed are, and
    // wall_destroyed's per-instance multiplier: that node's datum of this name,
    // where it is above 0.
    std::string wallEntity;
    std::string wallScaleKey;
    // dark_dragon_mouth: the APPEARING waypoint leg it is called on, every frame,
    // past this bias.
    int mouthLeg = 0;
    double mouthBiasAbove = 0.0;
    // The impact rows. A contact's hitIntensity is its approach speed, in level
    // px a second, over hitPxPerUnit; a row calls where that is above its
    // `hit_above`, with its `intensity` as the factor. touchPx is how near two
    // surfaces are touching, a bare literal in screen px as capPx is.
    double hitPxPerUnit = 0.0;
    double touchPx = 0.0;
    std::array<double, kCalls> hitAbove{};
    std::array<std::vector<std::string>, kCalls> spares{}; // entity names a row's callback passes over, exactly
    std::array<std::string, kCalls> entity{};              // the entity whose callback a row is

    const CallRule& Of(Call call) const { return calls[static_cast<std::size_t>(call)]; }
};

bool LoadRules(const std::string& path, Rules& out, std::string& error);

// EarthquakeGenerator's four fields, and the clock stamp.
struct Generator {
    double amplitudePx = 0.0; // `earthquake`, screen px
    double backPx = 0.0;      // `backValue`: what the last forth moved
    bool forth = true;        // `eqForth`: which way the next tremble goes
    // `lastTremble`. The original's starts at 0 on a clock already seconds old
    // when the first level plays, so its first call is due at once: this one
    // starts that way on a clock that may be young.
    double lastTrembleMs = -std::numeric_limits<double>::infinity();

    // As the constructor leaves it.
    void Reset(const Rules& rules);

    // EarthquakeGenerator::startEarthquake: the amplitude, overwritten.
    void Start(const Rules& rules, double intensity);

    // EarthquakeGenerator::update at `nowMs` on the clock the caller keeps. What
    // comes back is how far it moves the camera this frame, in screen px along
    // the level's y (down positive): the amplitude on a forth, minus the saved
    // value on a back, and 0 on a frame with no tremble.
    double Update(const Rules& rules, double nowMs);

    bool Running() const { return amplitudePx != 0.0; }
};

// Whether a node's entity name is `entity` in either spelling a level uses: the
// bare name or the name of its .ent (00_order 8.0 rule 4).
bool IsEntity(const std::string& nodeEntityName, const std::string& entity);

// Screen px to the level's units, by the screen the port takes (R12).
double ToUnits(const Rules& rules, double px);

// A contact's hitIntensity: `velocityPx` (level px a second) along `direction`, a
// unit vector, over hit_px_per_unit - dp2(GetLinearVelocity, normal) /
// getScale() as the three callbacks compute it (D11_check C47).
double HitIntensity(const Rules& rules, const glm::dvec2& velocityPx, const glm::dvec2& direction);

// Whether an impact row calls on a contact of `hit` with an entity named
// `otherEntity`: above its hit_above, and the other not one it passes over.
bool ImpactCalls(const Rules& rules, Call call, double hit, const std::string& otherEntity);

// Whether a dark dragon whose APPEARING path stands on waypoint `leg` at raw bias
// `bias` - DarkDragon::Path's frame and Bias() as this update's Path::Update left
// them, the getFrame of #548-551 and the bias #573 compares - is on
// dark_dragon_mouth's leg past its bias: the updates the callback re-arms the shake
// on. Since the merge of planning step 112 the path is the sim's own
// WaypointManager (planning step 99), which moves on at most one waypoint an
// update, so the leg is read from it and not summed from its holds.
bool InMouthLeg(const Rules& rules, int leg, double bias);

} // namespace MagicPortals::Earthquake
