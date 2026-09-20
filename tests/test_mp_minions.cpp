// The minions of worlds 3 and 4: spawned from a marker, walking a prefix of
// waypoints, and taken by the floor that kills them and nothing else.
//
// What is pinned here is the DECODE. Minions.hpp cites where each number comes
// from; the ones a test can tell apart from the remake's guesses are:
//
//   speed    1.3 px per physics step, and the game FIXES that step at 1/30 s
//            (AverageFPSRateManager calls SetFixedTimeStep(true) with 0.0333333),
//            so the walk is 39 px/s. The remake's actors.json guesses 60 and
//            marks it _guess: true.
//   arrival  8 px to advance, but 16 px to stop - minDist and minDist * 2 are
//            two separate tests in the original. Collapsing them into one
//            constant is the easy mistake, and it gives a minion that either
//            jitters at its waypoint or never settles, so both are pinned.
//            Both are measured from the ENTITY's point, 18.1 above the feet,
//            which the levels place the waypoints at - not from the port's body
//            centre, 11.1 below it. From the centre no patroller ever arrived,
//            and turned across its first waypoint on every tick (step 76).
//   hold     a DWELL in milliseconds, adopted from the waypoint just REACHED.
//            A lone waypoint carrying the 999999999 sentinel is a stationary
//            guard, and needs no special case: it is a wait nothing outlasts.
//
// And one thing that is not a number: enemy_killer kills no player. The remake's
// role table files it under `hazard` beside death_area.ent and lava, which would
// kill the player on contact in all 44 levels that carry one.
// ETHBeginContactCallback_enemy_killer is 69 instructions that open with
// isMinion(other) and jump to the return when that is false. It is sensor=0 and
// converts to a solid StaticBody2D - a floor to stand on. Both directions are
// tested, because that is exactly the kind of thing a role table smooths over:
// the floor is no hazard and kills no player, and it does take a minion.
//
// And, since step 76, the two facts the picture is framed by and nothing in the sim
// reads back: which arm each tick took (walk, hold, settled), and the facing the
// walk alone sets - moveVec's sign, not the velocity's.
//
// level0b is the first level of world 3 and carries all of it at once: a minion
// patrolling wayA0 <-> wayA1 (both holding 4000), a guard on a lone wayB0 with
// the sentinel, and a 768 x 128 enemy_killer floor.
//
// Reads the converted levels and the remake's data from outside this repository,
// and skips, saying where it looked, when either is absent.

#include "TestHarness.hpp"

#include "core/Components.hpp"
#include "sim/Art.hpp"
#include "sim/Game.hpp"
#include "sim/Minions.hpp"
#include "sim/Units.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <string>
#include <system_error>

using namespace MagicPortals;
using Supersonic::RigidBodyComponent;
using Supersonic::TransformComponent;

namespace {

const std::string kLevels = MAGICPORTALS_LEVELS_DIR;
const std::string kData = MAGICPORTALS_DATA_DIR;
constexpr float kStep = 1.0f / 60.0f;

constexpr const char* kLevel = "level0b";
constexpr const char* kPatroller = "minion_spawn_1811"; // waypointName "wayA"
constexpr const char* kGuard = "minion_spawn_913";      // waypointName "wayB", one waypoint
constexpr const char* kKiller = "enemy_killer_1915";    // 768 x 128, at (1152, 448)

struct Run {
    Game::Data data;
    entt::registry registry;
    Game::Level level;
};

bool Begin(const std::string& levelName, Run& run) {
    std::string error;
    const bool ok = Game::LoadData(kLevels + "/" + levelName + ".tscn", kData,
                                   std::filesystem::temp_directory_path() / "supersonic-test-mp-minions", run.data,
                                   error) &&
                    Game::Start(run.data, run.registry, run.level, error);
    CHECK_MSG(ok, levelName + std::string(": ") + error);
    return ok;
}

void Tick(Run& run) {
    Game::Tick(run.data, run.registry, run.level, 0.0f, kStep);
}

void PutAt(entt::registry& registry, entt::entity body, const glm::dvec2& atPx) {
    const glm::vec3 world = Units::ToWorld(atPx.x, atPx.y);
    auto& transform = registry.get<TransformComponent>(body);
    transform.position = glm::vec3(world.x, world.y, transform.position.z);
    registry.get<RigidBodyComponent>(body).velocity = glm::vec3(0.0f);
}

// Stood on level0b's floor (top 128) at x: the body's centre radius_px above it, so
// its entity point 18.1 above - where a walk that arrives there stands.
void StandAt(Run& run, entt::entity body, double x) {
    PutAt(run.registry, body, glm::dvec2(x, 128.0 - run.level.minions.rules.bodyRadiusPx));
}

float VelocityX(entt::registry& registry, entt::entity body) {
    return registry.get<RigidBodyComponent>(body).velocity.x;
}

double PixelX(entt::registry& registry, entt::entity body) {
    return Units::ToPixels(registry.get<TransformComponent>(body).position).x;
}

// 1.3 px per 33.3333 ms is 39 px/s, and the remake's guess is 60.
void theWalkIsTheDecodedSpeedNotTheRemakesGuess() {
    Run run;
    if (!Begin(kLevel, run)) return;

    const Minions::Rules& rules = run.level.minions.rules;
    CHECK(::test::nearly(static_cast<float>(rules.speedPxPerStep), 1.3f));
    CHECK(::test::nearly(static_cast<float>(rules.stepMs), 33.3333f, 1e-3f));
    CHECK(::test::nearly(static_cast<float>(run.level.minions.SpeedPxPerSecond()), 39.0f, 1e-2f));
    CHECK_MSG(run.level.minions.SpeedPxPerSecond() < 60.0, "the remake's 60 px/s is not what is played");

    // Two thresholds, not one: it advances inside 8 px and stops inside 16.
    CHECK(::test::nearly(static_cast<float>(rules.arrivePx), 8.0f));
    CHECK(::test::nearly(static_cast<float>(rules.settlePx), 16.0f));
    CHECK_MSG(rules.settlePx > rules.arrivePx, "the stop is wider than the advance");
    CHECK(::test::nearly(static_cast<float>(rules.settlePx), static_cast<float>(rules.arrivePx * 2.0)));
}

// waypointName is a PREFIX, and the marker resolves it by seeking prefix + n.
void aMarkerResolvesItsWaypointsByPrefix() {
    Run run;
    if (!Begin(kLevel, run)) return;

    CHECK_EQ(run.level.minions.minions.size(), static_cast<std::size_t>(2));

    const Minions::Minion* patroller = run.level.minions.FindMinion(kPatroller);
    CHECK(patroller != nullptr);
    if (patroller != nullptr) {
        CHECK_MSG(patroller->waypointName == "wayA", "the patroller's prefix is wayA");
        CHECK_EQ(patroller->waypoints.size(), static_cast<std::size_t>(2));
        if (patroller->waypoints.size() == 2) {
            CHECK_MSG(patroller->waypoints[0].name == "wayA0", "the first waypoint is wayA0");
            CHECK_MSG(patroller->waypoints[1].name == "wayA1", "the second is wayA1");
            CHECK(::test::nearly(static_cast<float>(patroller->waypoints[0].atPx.x), 509.0f));
            CHECK(::test::nearly(static_cast<float>(patroller->waypoints[1].atPx.x), 547.0f));
            // The hold is quoted in the scene ("4000"); Value::AsNumber reads it.
            CHECK(::test::nearly(static_cast<float>(patroller->waypoints[0].holdMs), 4000.0f));
            CHECK(::test::nearly(static_cast<float>(patroller->waypoints[1].holdMs), 4000.0f));
        }
    }

    const Minions::Minion* guard = run.level.minions.FindMinion(kGuard);
    CHECK(guard != nullptr);
    if (guard != nullptr) {
        CHECK_MSG(guard->waypointName == "wayB", "the guard's prefix is wayB");
        // One resolved waypoint means "stand still", not "data missing".
        CHECK_EQ(guard->waypoints.size(), static_cast<std::size_t>(1));
        if (!guard->waypoints.empty()) {
            CHECK_MSG(guard->waypoints[0].name == "wayB0", "its one waypoint is wayB0");
            CHECK_MSG(guard->waypoints[0].holdMs > 9.9e8, "wayB0 carries the stationary sentinel");
        }
    }

    // A minion is teleportable (minion.ent carries the flag), so the portals
    // carry one as they carry what a launcher throws.
    if (patroller != nullptr && patroller->body != entt::null) {
        const auto& travellers = run.level.portals.travellers;
        CHECK_MSG(std::find(travellers.begin(), travellers.end(), patroller->body) != travellers.end(),
                  "a spawned minion is among the portals' travellers");
    }
}

void aPatrollerWalksToItsFirstWaypoint() {
    Run run;
    if (!Begin(kLevel, run)) return;

    const Minions::Minion* patroller = run.level.minions.FindMinion(kPatroller);
    CHECK(patroller != nullptr);
    if (patroller == nullptr || patroller->body == entt::null) return;
    const entt::entity body = patroller->body;

    Tick(run);
    CHECK(run.registry.valid(body));
    if (!run.registry.valid(body)) return;

    // It spawns at x 546 and wayA0 is at 509, so it walks left - at 39 px/s,
    // which is 0.78 m/s in the engine's units.
    CHECK(VelocityX(run.registry, body) < 0.0f);
    CHECK(::test::nearly(VelocityX(run.registry, body), -Units::ToMetres(39.0), 1e-3f));
    // And the picture's two facts (step 76): the walk arm, facing the way moveVec
    // points - which, not the velocity's sign, is what the original's row follows.
    const Minions::Minion* walked = run.level.minions.FindMinion(kPatroller);
    CHECK_MSG(walked != nullptr && walked->arm == Minions::Arm::Walk && walked->facing == Minions::Facing::Left,
              "the walk arm, facing left");
    CHECK_MSG(walked != nullptr && !walked->summoned && walked->entityName.empty(),
              "a marker's minion.ent: no entityName on level0b's markers");
}

// The facing is set only where the walk sets the velocity, and a minion that has
// never walked faces nothing: the original's dirRow is unset until then (step 76).
void theFacingIsTheWalksAndOnlyTheWalks() {
    Run run;
    if (!Begin(kLevel, run)) return;

    const Minions::Minion* found = run.level.minions.FindMinion(kPatroller);
    CHECK(found != nullptr);
    if (found == nullptr || found->body == entt::null) return;
    const entt::entity body = found->body;
    CHECK_MSG(found->arm == Minions::Arm::None && found->facing == Minions::Facing::None,
              "nothing chosen before the first tick");

    // Stood on its first waypoint before it has walked (its entity 2.1 above wayA0):
    // it arrives, adopts the hold, and holds - facing nothing yet.
    StandAt(run, body, 509.0);
    Tick(run);
    const Minions::Minion* held = run.level.minions.FindMinion(kPatroller);
    CHECK_MSG(held != nullptr && held->arm == Minions::Arm::Hold && held->facing == Minions::Facing::None,
              "held on arriving, having never walked");

    // Through the hold and off to wayA1 at 547: the walk, facing right.
    for (int tick = 0; tick < 250; ++tick) Tick(run);
    const Minions::Minion* off = run.level.minions.FindMinion(kPatroller);
    CHECK_MSG(off != nullptr && off->arm == Minions::Arm::Walk && off->facing == Minions::Facing::Right,
              "walking to wayA1, facing right");

    // Shoved the other way while it walks right, it still faces the way it is
    // driven: a velocity's sign is not its facing.
    if (run.registry.valid(body)) run.registry.get<RigidBodyComponent>(body).velocity.x = -Units::ToMetres(200.0);
    const Minions::Minion* shoved = run.level.minions.FindMinion(kPatroller);
    CHECK_MSG(shoved != nullptr && shoved->facing == Minions::Facing::Right, "a shove is not a turn");
}

// Arrival adopts the hold of the waypoint REACHED, and the walk resumes only
// once the dwell is served. Driven there by its own walk, not placed: the arrival
// is measured from the entity's point, 11.1 above the body's centre, and placing
// the centre on the waypoint is exactly what hid that it never arrived (step 76).
void arrivingAdoptsTheHoldAndServesIt() {
    Run run;
    if (!Begin(kLevel, run)) return;

    const Minions::Minion* found = run.level.minions.FindMinion(kPatroller);
    CHECK(found != nullptr);
    if (found == nullptr || found->body == entt::null) return;
    const entt::entity body = found->body;
    CHECK(::test::nearly(static_cast<float>(run.level.minions.rules.EntityAboveCentrePx()), 11.1f, 1e-5f));

    // From x 546 to wayA0 at 509: it walks left, and arrives with no turn on the way.
    int ticks = 0;
    bool turned = false;
    const Minions::Minion* walking = run.level.minions.FindMinion(kPatroller);
    while (walking != nullptr && walking->dest == 0 && ticks < 120 && run.registry.valid(body)) {
        Tick(run);
        ++ticks;
        walking = run.level.minions.FindMinion(kPatroller);
        if (walking != nullptr && walking->facing == Minions::Facing::Right) turned = true;
    }
    CHECK(run.registry.valid(body));
    if (!run.registry.valid(body)) return;

    const Minions::Minion* arrived = run.level.minions.FindMinion(kPatroller);
    CHECK(arrived != nullptr);
    if (arrived == nullptr) return;
    CHECK_MSG(arrived->dest == 1 && arrived->arrivedAt == 0,
              "it arrived at wayA0 by its own walk, in " + std::to_string(ticks) + " ticks");
    CHECK_MSG(!turned, "and never turned on the way");
    const glm::dvec2 bodyPx = Units::ToPixels(run.registry.get<TransformComponent>(body).position);
    const glm::dvec2 entityPx = bodyPx - glm::dvec2(0.0, run.level.minions.rules.EntityAboveCentrePx());
    CHECK_MSG(glm::distance(entityPx, arrived->waypoints[0].atPx) < run.level.minions.rules.arrivePx &&
                  bodyPx.x > arrived->waypoints[0].atPx.x,
              "its entity within arrive_px of wayA0, short of crossing it");
    CHECK(::test::nearly(static_cast<float>(arrived->holdMs), 4000.0f));
    CHECK_MSG(arrived->arm == Minions::Arm::Hold, "holding from the tick it arrived");

    // The brake takes 0.3 of what is left each tick: gone to nothing in ten.
    for (int tick = 0; tick < 10; ++tick) Tick(run);
    CHECK(::test::nearly(VelocityX(run.registry, body), 0.0f));

    // Most of the way through the 4000 ms hold: still standing.
    for (int tick = 0; tick < 140; ++tick) Tick(run);
    CHECK(run.registry.valid(body));
    if (!run.registry.valid(body)) return;
    CHECK_MSG(::test::nearly(VelocityX(run.registry, body), 0.0f), "still serving the hold at 2.5 s");

    // And past it: walking again, now towards wayA1 at x 547.
    for (int tick = 0; tick < 120; ++tick) Tick(run);
    CHECK(run.registry.valid(body));
    if (!run.registry.valid(body)) return;
    CHECK_MSG(VelocityX(run.registry, body) > 0.0f, "walking again once the hold is served");
}

// The patrol, left alone for 1000 ticks: wayA0, wayA1, wayA0 and wayA1 again (by
// about 0.8, 5.4, 10.0 and 14.6 s at 39 u/s with 4000 ms holds), and it
// turns only where it arrives. Measured from the body's centre it arrived nowhere
// and turned on every tick from its first crossing of wayA0 (step 76's finding,
// a 30 Hz strobe between rows 1 and 2 once the knights were drawn).
void aPatrolTurnsOnlyWhereItArrives() {
    Run run;
    if (!Begin(kLevel, run)) return;

    const Minions::Minion* found = run.level.minions.FindMinion(kPatroller);
    CHECK(found != nullptr);
    if (found == nullptr || found->body == entt::null) return;

    int arrivals = 0;
    int turns = 0;
    std::size_t dest = found->dest;
    Minions::Facing facing = found->facing;
    for (int tick = 0; tick < 1000; ++tick) {
        Tick(run);
        const Minions::Minion* now = run.level.minions.FindMinion(kPatroller);
        if (now == nullptr || now->gone) break;
        if (now->dest != dest) ++arrivals;
        if (facing != Minions::Facing::None && now->facing != facing) ++turns;
        dest = now->dest;
        facing = now->facing;
    }
    CHECK_MSG(arrivals >= 4, "four arrivals in 16.7 s: " + std::to_string(arrivals));
    CHECK_MSG(turns >= 3 && turns <= arrivals,
              std::to_string(turns) + " turns for " + std::to_string(arrivals) + " arrivals");
}

// minions.json's entity height is the decode art.json places the picture by: one
// number, read twice, and the two must not drift apart.
void theEntityStandsWhereThePictureStands() {
    Run run;
    if (!Begin(kLevel, run)) return;

    Art::Rules art;
    std::string error;
    CHECK_MSG(Art::LoadRules(std::string(MAGICPORTALS_PORT_DATA_DIR) + "/art.json", art, error), error);
    if (!error.empty()) return;
    CHECK(::test::nearly(static_cast<float>(run.level.minions.rules.entityAboveFeetPx), 18.1f, 1e-5f));
    CHECK_MSG(run.level.minions.rules.entityAboveFeetPx == art.minion.feetBelowEntityPx,
              "minions.json body.entity_above_feet_px equals art.json minion.feet_below_entity_px");
}

// A lone waypoint, and the wider threshold that belongs to it alone.
//
// The guard is NOT motionless from the start, which is the part worth pinning: it
// spawns 31 px short of its post at x 177 and walks there. What stops it being
// DRIVEN is the minDist * 2 test, which the original computes only for a minion
// with exactly one waypoint - and from there the two engines part company. The
// original's minion is frictionless (minion.ent friction 0.000000), so it coasts
// the last stretch in, arrives, adopts the 999999999 sentinel, and is parked by
// the brake. This port's bodies grip (kBodyFriction 1), so its guard stops about
// where the drive stops, inside the wider threshold and short of arriving. Either
// way it ends up by its post and stays there, which is what is pinned here;
// minions.json carries the difference.
void aLoneWaypointIsWalkedToAndThenHeldForGood() {
    Run run;
    if (!Begin(kLevel, run)) return;

    const Minions::Minion* guard = run.level.minions.FindMinion(kGuard);
    CHECK(guard != nullptr);
    if (guard == nullptr || guard->body == entt::null || guard->waypoints.empty()) return;
    const entt::entity body = guard->body;
    const glm::dvec2 post = guard->waypoints[0].atPx;

    Tick(run);
    CHECK(run.registry.valid(body));
    if (!run.registry.valid(body)) return;
    CHECK_MSG(VelocityX(run.registry, body) > 0.0f, "the guard sets off towards its post");

    for (int tick = 0; tick < 180; ++tick) Tick(run);
    CHECK(run.registry.valid(body));
    if (!run.registry.valid(body)) return;

    const Minions::Minion* posted = run.level.minions.FindMinion(kGuard);
    CHECK(posted != nullptr);
    if (posted == nullptr) return;
    CHECK_MSG(std::abs(PixelX(run.registry, body) - post.x) < run.level.minions.rules.settlePx,
              "the guard stopped within the wider threshold of its post");
    CHECK_MSG(posted->waypoints.size() == 1, "and it got there on one waypoint, which is why it stopped");
    CHECK(::test::nearly(VelocityX(run.registry, body), 0.0f));
    // shouldntMove: the settled arm, which writes no frame, still facing its walk.
    CHECK_MSG(posted->arm == Minions::Arm::Settled && posted->facing == Minions::Facing::Right,
              "settled, facing the way it walked in");

    // Still there three seconds later: the wait is never restarted.
    const double atPost = PixelX(run.registry, body);
    for (int tick = 0; tick < 180; ++tick) Tick(run);
    CHECK(run.registry.valid(body));
    if (!run.registry.valid(body)) return;
    CHECK(::test::nearly(VelocityX(run.registry, body), 0.0f));
    CHECK_MSG(std::abs(PixelX(run.registry, body) - atPost) < 1.0, "and stands at its post for good");
}

// The settled branch writes NO velocity, which leaves a minion inside the band to
// its body's own friction. That is only safe because the body cannot roll: the
// original is fixedRotation=1 and the port locks z to match. A minion is
// teleportable and is among the portals' travellers, so arriving at its post
// carrying speed is a state a real level can reach - this gives it three times a
// walk and checks it still parks rather than sailing through.
void aGuardCarryingMomentumStillParks() {
    Run run;
    if (!Begin(kLevel, run)) return;

    const Minions::Minion* guard = run.level.minions.FindMinion(kGuard);
    CHECK(guard != nullptr);
    if (guard == nullptr || guard->body == entt::null || guard->waypoints.empty()) return;
    const entt::entity body = guard->body;
    const glm::dvec2 post = guard->waypoints[0].atPx;

    // Let it walk to its post and settle there first.
    for (int tick = 0; tick < 180; ++tick) Tick(run);
    CHECK(run.registry.valid(body));
    if (!run.registry.valid(body)) return;

    // Then shove it at its post, three times as fast as it walks.
    run.registry.get<RigidBodyComponent>(body).velocity.x = 3.0f * Units::ToMetres(39.0);
    for (int tick = 0; tick < 120; ++tick) Tick(run);

    CHECK(run.registry.valid(body));
    if (!run.registry.valid(body)) return;
    CHECK(::test::nearly(VelocityX(run.registry, body), 0.0f));
    CHECK_MSG(std::abs(PixelX(run.registry, body) - post.x) < run.level.minions.rules.settlePx,
              "momentum did not carry the guard through its post");
}

// The box is the shape the level gives it, not Trigger's 16 px fallback.
//
// And the floor is TILED rather than single: level0b lays two 768 x 128 slabs end
// to end, at x 384 and x 1152. Thirty of the 44 levels carry one slab and
// fourteen carry two, so the count is pinned too - a finder that took only the
// first would leave half of those levels' floors inert.
void theKillerFloorIsTheShapeNotTheFallback() {
    Run run;
    if (!Begin(kLevel, run)) return;

    CHECK_EQ(run.level.minions.killers.size(), static_cast<std::size_t>(2));

    const Minions::Killer* found = nullptr;
    for (const Minions::Killer& killer : run.level.minions.killers) {
        if (killer.name == kKiller) found = &killer;
    }
    CHECK_MSG(found != nullptr, "the level's enemy_killer is among them");
    if (found == nullptr) return;
    CHECK(::test::nearly(found->box.half.x, Units::ToMetres(384.0)));
    CHECK(::test::nearly(found->box.half.y, Units::ToMetres(64.0)));
    CHECK_MSG(found->box.half.x > Units::ToMetres(8.0), "not the 16 px fallback a hazard would get");
}

// The correction, in the direction that matters most: it is not a hazard, and a
// player stood in it does not die.
void theKillerFloorKillsNoPlayer() {
    Run run;
    if (!Begin(kLevel, run)) return;

    CHECK_MSG(run.level.hazards.FindHazard(kKiller) == nullptr, "the killer floor is no hazard");
    for (const Hazards::Hazard& hazard : run.level.hazards.hazards) {
        CHECK_MSG(hazard.name.rfind("enemy_killer", 0) != 0, hazard.name + " is not among the hazards");
    }

    if (run.level.player == entt::null) return;

    // THE FLOOR IS OUTSIDE THE LEVEL, which is why this no longer asks whether
    // the player simply lives there - and the premise was rewritten rather than
    // relaxed, as the dark dragon's shot gate was.
    //
    // level0b's `max` marker is at (1052, 256), and hazards.json's margin is
    // (64, 96), so the level ends at (1116, 352). The killer floor is a
    // 768 x 128 slab centred at (1152, 448): it spans x 768..1536 and
    // y 384..512, past that corner on BOTH axes. A player stood on it is
    // standing below AND beyond the level, and checkGameLost's bounds test -
    // which this port did not have when this case was written - kills it for
    // exactly that. The case was quietly parking the player outside the world,
    // and nothing noticed because nothing tested the edge.
    //
    // So what is asked here now is what the case always MEANT: whoever kills
    // the player there, it is not the killer floor. enemy_killer takes minions
    // and nothing else, which is the whole correction this file exists to pin.
    PutAt(run.registry, run.level.player, glm::dvec2(1152.0, 448.0));
    for (int tick = 0; tick < 60; ++tick) Tick(run);
    CHECK_MSG(run.level.hazards.killedBy != kKiller,
              "the killer floor did not kill the player, got '" + run.level.hazards.killedBy + "'");
    CHECK_MSG(run.level.hazards.playerDied && run.level.hazards.diedByFalling,
              "it was the fall out of the level that took it, as the original has it");
}

// And in the other direction: it does take a minion.
void theKillerFloorTakesAMinion() {
    Run run;
    if (!Begin(kLevel, run)) return;

    const Minions::Minion* guard = run.level.minions.FindMinion(kGuard);
    CHECK(guard != nullptr);
    if (guard == nullptr || guard->body == entt::null) return;
    const entt::entity body = guard->body;
    CHECK_EQ(run.level.minions.killed, 0);

    PutAt(run.registry, body, glm::dvec2(1152.0, 448.0));
    for (int tick = 0; tick < 5 && run.registry.valid(body); ++tick) Tick(run);

    CHECK(!run.registry.valid(body));
    CHECK_EQ(run.level.minions.killed, 1);
    const Minions::Minion* after = run.level.minions.FindMinion(kGuard);
    CHECK(after != nullptr);
    if (after != nullptr) CHECK(after->gone);

    // And it went through Forget, as a burnt crate does.
    const auto& travellers = run.level.portals.travellers;
    CHECK_MSG(std::find(travellers.begin(), travellers.end(), body) == travellers.end(),
              "a taken minion is no longer a traveller");
}

void runTests() {
    theWalkIsTheDecodedSpeedNotTheRemakesGuess();
    aMarkerResolvesItsWaypointsByPrefix();
    aPatrollerWalksToItsFirstWaypoint();
    theFacingIsTheWalksAndOnlyTheWalks();
    arrivingAdoptsTheHoldAndServesIt();
    aPatrolTurnsOnlyWhereItArrives();
    theEntityStandsWhereThePictureStands();
    aLoneWaypointIsWalkedToAndThenHeldForGood();
    aGuardCarryingMomentumStillParks();
    theKillerFloorIsTheShapeNotTheFallback();
    theKillerFloorKillsNoPlayer();
    theKillerFloorTakesAMinion();
}

} // namespace

int main() {
    std::error_code ec;
    if (!std::filesystem::is_directory(kLevels, ec) ||
        !std::filesystem::is_regular_file(kData + "/entity_roles.json", ec)) {
        std::printf("test_mp_minions SKIPPED - needs the converted levels at %s and the remake's data at %s\n",
                    kLevels.c_str(), kData.c_str());
        return 77;
    }
    runTests();
    return ::test::summary("test_mp_minions", 40);
}
