// Carrancas: the wall gargoyles that spit fireballs, which Enhanced's role
// table calls turrets.
//
// What is pinned here is the DECODE, not the Godot project's reading of it, and the two
// differ in one place that matters. carrancaCallback seeds elapsedTime from the
// node's startStride, gathers each frame, fires when it passes the node's stride
// and resets to zero - so level0a's carranca, whose startStride is 0 and whose
// stride is 1500, first fires a stride in. The Godot project treats startStride as an
// initial delay and fires at once (hazards.gd:346-349). A test that only counted
// fireballs would pass either way, so the FIRST one is timed here.
//
// The fireball's own numbers are its .ent's: 130 px/s and a circle of radius 9.6
// (16 x 16, shape 2, at its <Scale> 1.2). The Godot project's hazards.json invents
// 180 px/s and marks itself _guess; the original states it, so the port does not
// inherit the guess.
//
// And what it meets is its contact callback's (ETHBeginContactCallback_fireball,
// planning step 114): it passes its carranca, a sensor and a character it may
// not kill, and dies on the first solid thing it touches - a wall, a closed door,
// a crate it burns, a barrel it sets off. Until step 114 the port read "sensor"
// as "nothing stops it" and its fireballs flew through walls to the level's edge.
//
// Reads the converted levels and Enhanced's data from outside this repository,
// and skips, saying where it looked, when either is absent.

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "core/Components.hpp"
#include "sim/Game.hpp"
#include "sim/LevelBuilder.hpp"
#include "sim/Portals.hpp"
#include "sim/Shot.hpp"
#include "sim/Turrets.hpp"
#include "sim/Units.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <span>
#include <string>
#include <system_error>
#include <vector>

using namespace MagicPortals;
using Supersonic::BoxColliderComponent;
using Supersonic::ConvexHullColliderComponent;
using Supersonic::SphereColliderComponent;
using Supersonic::TransformComponent;

namespace {

const std::string kLevels = MAGICPORTALS_LEVELS_DIR;
const std::string kData = MAGICPORTALS_DATA_DIR;
const std::string kPortData = MAGICPORTALS_PORT_DATA_DIR;
constexpr float kStep = 1.0f / 60.0f;

struct Run {
    Game::Data data;
    entt::registry registry;
    Game::Level level;
};

bool Begin(const std::string& levelName, Run& run) {
    std::string error;
    const bool ok = Game::LoadData(kLevels + "/" + levelName + ".tscn", kData,
                                   std::filesystem::temp_directory_path() / "supersonic-test-mp-turrets", run.data,
                                   error) &&
                    Game::Start(run.data, run.registry, run.level, error);
    CHECK_MSG(ok, levelName + ": " + error);
    return ok;
}

void Tick(Run& run, float direction = 0.0f) { Game::Tick(run.data, run.registry, run.level, direction, kStep); }

std::filesystem::path Scratch() {
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "supersonic-test-mp-turrets";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir;
}

std::string WriteRules(const char* name, const std::string& body) {
    const std::filesystem::path path = Scratch() / name;
    std::ofstream file(path, std::ios::trunc);
    file << body;
    return path.string();
}

// level0a is chapter 2's first level, and one of the ten whose only unported
// role was the carranca.
constexpr const char* kLevel = "level0a";

void TheCarrancasAreFound() {
    Run run;
    if (!Begin(kLevel, run)) return;

    const Turrets::State& turrets = run.level.turrets;
    CHECK_MSG(!turrets.turrets.empty(), "level0a carries at least one carranca");
    if (turrets.turrets.empty()) return;

    std::printf("  %s: %d carranca(s)\n", kLevel, static_cast<int>(turrets.turrets.size()));

    // carranca_right_ent_792: dirX 1.2, dirY 0, so it fires to the RIGHT, and
    // the direction is carried as a unit vector however the data spells it.
    bool sawRightward = false;
    for (const Turrets::Turret& turret : turrets.turrets) {
        const double length =
            std::sqrt(turret.directionPx.x * turret.directionPx.x + turret.directionPx.y * turret.directionPx.y);
        CHECK_MSG(::test::nearly(static_cast<float>(length), 1.0f), "every direction is a unit vector");
        CHECK_MSG(turret.strideMs > 0.0, "and every stride is a real one");
        // Its own body, which its fireballs pass by (isCarranca): the one it spits
        // starts inside it.
        CHECK_MSG(turret.body != entt::null && run.registry.valid(turret.body),
                  turret.name + " carries the body the builder made for it");
        if (turret.directionPx.x > 0.5) sawRightward = true;
    }
    CHECK_MSG(sawRightward, "level0a's carranca_right fires to the right, as its dirX of 1.2 says");
    CHECK_MSG(turrets.Bodies().size() == turrets.turrets.size(), "and Bodies() hands every one of them over");
}

void TheFirstShotComesAStrideIn() {
    Run run;
    if (!Begin(kLevel, run)) return;
    if (run.level.turrets.turrets.empty()) return;

    // The shortest stride in the level decides when the first fireball can
    // appear at all: elapsedTime starts at startStride and has to PASS stride.
    double soonestMs = 0.0;
    for (const Turrets::Turret& turret : run.level.turrets.turrets) {
        const double waitMs = turret.strideMs - turret.elapsedMs;
        if (soonestMs == 0.0 || waitMs < soonestMs) soonestMs = waitMs;
    }
    CHECK_MSG(soonestMs > 0.0, "no carranca fires before the level has begun");

    // NOT at once. This is the whole difference from the Godot project's reading, and
    // a test that only counted fireballs would not see it.
    Tick(run);
    CHECK_MSG(run.level.turrets.fireballs.empty(), "nothing is spat on the first tick");

    const int ticksToFirst = static_cast<int>(soonestMs / (kStep * 1000.0f));
    for (int tick = 1; tick < ticksToFirst; ++tick) Tick(run);
    CHECK_MSG(run.level.turrets.fireballs.empty(),
              "and none until the stride has passed, got " + std::to_string(run.level.turrets.fireballs.size()));

    // Two more: one to pass the stride, and one because the level's carrancas
    // need not share it.
    Tick(run);
    Tick(run);
    CHECK_MSG(!run.level.turrets.fireballs.empty(), "and then one is");
    std::printf("  %s: the first fireball after %d tick(s); its stride is %.0f ms\n", kLevel, ticksToFirst + 1,
                soonestMs);
}

void AFireballFliesAtItsOwnSpeed() {
    Run run;
    if (!Begin(kLevel, run)) return;
    if (run.level.turrets.turrets.empty()) return;

    // Far enough for every carranca in the level to have fired.
    for (int tick = 0; tick < 240 && run.level.turrets.fireballs.empty(); ++tick) Tick(run);
    if (run.level.turrets.fireballs.empty()) {
        CHECK_MSG(false, "no carranca fired within four seconds");
        return;
    }

    const Turrets::Fireball before = run.level.turrets.fireballs.front();
    const glm::dvec2 wasAt = before.atPx;
    Tick(run);

    // The same fireball, if it is still there: found by name, because the tick
    // may have dropped others and fired more.
    for (const Turrets::Fireball& now : run.level.turrets.fireballs) {
        if (now.name != before.name) continue;
        const glm::dvec2 moved = now.atPx - wasAt;
        const double distance = std::sqrt(moved.x * moved.x + moved.y * moved.y);
        // 130 px/s over one sixtieth of a second.
        CHECK_MSG(::test::nearly(static_cast<float>(distance),
                                 static_cast<float>(run.level.turrets.rules.speedPx * kStep), 0.01f),
                  "a fireball travels its .ent's own 130 px/s");
        return;
    }
}

// One tick of travel at 130 px/s: how far the port's stop may sit from the
// geometry's, since a flight is tested where each tick leaves it.
constexpr double kTickTravelPx = 130.0 / 60.0;

void AFireballDiesOnTheFirstWall() {
    Run run;
    if (!Begin(kLevel, run)) return;
    if (run.level.turrets.turrets.empty()) return;

    // A minute of it. carranca_right_ent_792 at (23, 142) fires right, and the
    // first solid body in its lane is block00_no_emissive_ent_840, whose face the
    // fireball's 9.6 circle meets with its centre at x 278.5 (the static lane
    // read from the converted level: out/parity/program/G5/lanes_r9.6.txt). Every
    // fireball of the minute must die THERE - none flies on to the level's edge,
    // as every one did before step 114.
    int spent = 0;
    int wrong = 0;
    double farthestPx = 0.0;
    for (int tick = 0; tick < 3600; ++tick) {
        Tick(run);
        for (const Turrets::Fireball& ball : run.level.turrets.fireballs) {
            farthestPx = std::max(farthestPx, ball.atPx.x);
        }
        for (const glm::dvec2& atPx : run.level.turrets.spentPx) {
            ++spent;
            if (std::fabs(atPx.x - 278.5) > kTickTravelPx || std::fabs(atPx.y - 142.0) > 1e-6) {
                ++wrong;
                std::printf("  %s: a fireball went off at (%.2f, %.2f), not at the block's face\n", kLevel, atPx.x,
                            atPx.y);
            }
        }
    }

    int fired = 0;
    for (const Turrets::Turret& turret : run.level.turrets.turrets) fired += turret.fired;
    CHECK_MSG(fired > 0, "a minute fires some");
    CHECK_MSG(wrong == 0, "every fireball dies on the block's face at x 278.5, one tick's travel either way");
    // Destroyed, every one, and none culled: fired = went off + still flying.
    CHECK_MSG(spent + static_cast<int>(run.level.turrets.fireballs.size()) == fired,
              "and every one fired ends there rather than at the level's edge: " + std::to_string(spent) +
                  " went off and " + std::to_string(run.level.turrets.fireballs.size()) + " fly, of " +
                  std::to_string(fired));
    CHECK_MSG(farthestPx < 278.5 + kTickTravelPx, "and none is ever seen past the block");
    std::printf("  %s after a minute: %d fired, %d went off on the block, %d live; the farthest at x %.2f\n", kLevel,
                fired, spent, static_cast<int>(run.level.turrets.fireballs.size()), farthestPx);
}

// ETHBeginContactCallback_fireball's two passes that are not a carranca: a
// SENSOR is passed by (the converter's Area2D, a trigger here), and so is what
// the fireball merely touches. Measured on the port's own touch, not a level.
void TheTouchIsTheOriginals() {
    entt::registry registry;
    const auto place = [&registry](double xPx, double yPx) {
        const entt::entity entity = registry.create();
        registry.emplace<TransformComponent>(entity).position = Units::ToWorld(xPx, yPx);
        return entity;
    };
    const double r = 9.6;
    const float side = Units::ToMetres(32.0);

    const entt::entity wall = place(100.0, 100.0);
    registry.emplace<BoxColliderComponent>(wall).size = glm::vec3(side, side, 2.0f);
    const entt::entity sensor = place(200.0, 100.0);
    auto& trigger = registry.emplace<BoxColliderComponent>(sensor);
    trigger.size = glm::vec3(side, side, 2.0f);
    trigger.isTrigger = true;
    const entt::entity stone = place(300.0, 100.0);
    registry.emplace<SphereColliderComponent>(stone).radius = Units::ToMetres(8.0);
    const entt::entity hull = place(400.0, 100.0);
    registry.emplace<ConvexHullColliderComponent>(hull);
    auto& outline = registry.emplace<LevelBuilder::PlanePolygon>(hull);
    for (const glm::dvec2 corner : {glm::dvec2(-16.0, -16.0), glm::dvec2(16.0, -16.0), glm::dvec2(16.0, 16.0),
                                    glm::dvec2(-16.0, 16.0)}) {
        outline.points.emplace_back(Units::ToMetres(corner.x), Units::ToMetres(-corner.y));
    }

    const auto only = [](const std::vector<entt::entity>& met, entt::entity body) {
        return met.size() == 1 && met.front() == body;
    };
    const std::span<const entt::entity> none;

    // The box's face is x 116: a circle whose edge only reaches it touches and
    // does not overlap; a hair nearer, it does.
    CHECK_MSG(Shot::SolidBodiesIn(registry, glm::dvec2(116.0 + r + 0.05, 100.0), r, none).empty(),
              "a fireball short of the wall's face has not met it");
    CHECK_MSG(only(Shot::SolidBodiesIn(registry, glm::dvec2(116.0 + r - 0.05, 100.0), r, none), wall),
              "one a hair into it has");
    CHECK_MSG(only(Shot::SolidBodiesIn(registry, glm::dvec2(100.0, 100.0), r, none), wall),
              "and so has one wholly inside it");
    // A sensor, however deep: never.
    CHECK_MSG(Shot::SolidBodiesIn(registry, glm::dvec2(200.0, 100.0), r, none).empty(),
              "a sensor is passed by, however deep the fireball is in it");
    // What the caller names is passed by too: the carrancas' own bodies.
    CHECK_MSG(Shot::SolidBodiesIn(registry, glm::dvec2(100.0, 100.0), r, std::span<const entt::entity>(&wall, 1))
                  .empty(),
              "and so is a body it is told to pass, as a carranca's is");
    // A sphere: the two radii.
    CHECK_MSG(only(Shot::SolidBodiesIn(registry, glm::dvec2(300.0 + 17.5, 100.0), r, none), stone),
              "a stone is met within the two radii, 17.6");
    CHECK_MSG(Shot::SolidBodiesIn(registry, glm::dvec2(300.0 + 17.7, 100.0), r, none).empty(),
              "and not beyond them");
    // A hull, off its corner: within the radius of the corner, met; beyond, not.
    CHECK_MSG(only(Shot::SolidBodiesIn(registry, glm::dvec2(416.0 + 6.0, 116.0 + 6.0), r, none), hull),
              "a hull is met off its corner within the radius (8.49)");
    CHECK_MSG(Shot::SolidBodiesIn(registry, glm::dvec2(416.0 + 7.0, 116.0 + 7.0), r, none).empty(),
              "and not beyond it (9.90)");
}

// addFireball's killMainCharacter clear - a fire diamond's conversion - is passed
// by the character (#1-31): no kill, and the fireball flies on through it.
void ACharacterItMayNotKillIsPassed() {
    Run run;
    if (!Begin(kLevel, run)) return;
    const glm::dvec2 atPx = Units::ToPixels(run.registry.get<TransformComponent>(run.level.player).position);
    Turrets::Fireball ball;
    ball.name = "spared#1";
    ball.atPx = atPx;
    ball.velocityPx = glm::dvec2(run.level.turrets.rules.speedPx, 0.0);
    ball.killsPlayer = false;
    run.level.turrets.fireballs.push_back(ball);
    bool spentOnIt = false;
    for (int tick = 0; tick < 4; ++tick) {
        Tick(run);
        if (!run.level.turrets.spentPx.empty()) spentOnIt = true;
    }
    bool flying = false;
    for (const Turrets::Fireball& now : run.level.turrets.fireballs) {
        if (now.name == ball.name) flying = true;
    }
    CHECK_MSG(!run.level.turrets.playerKilled && !run.level.hazards.playerDied,
              "a fireball that may not kill the character does not");
    CHECK_MSG(flying && !spentOnIt, "and is not spent on it: it flies on through");
}

// 2-12, footage F6 #72: a fireball hits the CLOSED door and goes off there. Before
// step 114 it flew through the door and killed a player standing at the spawn on
// tick 180 (the f420 sweep's log), and later ones went on to the barrel.
void AClosedDoorStopsTheFire() {
    Run run;
    if (!Begin("level11a", run)) return;
    const Fire::Bomb* barrel = run.level.fire.FindBomb("barrel_bomb_ent_1041");
    CHECK_MSG(barrel != nullptr, "level11a's barrel is there");

    // carranca_right_ent_1048 at (61, 200): door_lift_1042's face meets the
    // circle at x 103.5 (lanes_r9.6.txt).
    int spent = 0;
    int firstTick = -1;
    int wrong = 0;
    int tick = 0;
    for (; tick < 900 && !run.level.hazards.playerDied; ++tick) {
        Tick(run);
        for (const glm::dvec2& atPx : run.level.turrets.spentPx) {
            if (firstTick < 0) firstTick = tick;
            ++spent;
            if (std::fabs(atPx.x - 103.5) > kTickTravelPx) ++wrong;
        }
    }
    CHECK_MSG(!run.level.hazards.playerDied,
              "a player standing at the spawn lives through 900 ticks: died on tick " + std::to_string(tick));
    CHECK_MSG(spent >= 3 && wrong == 0,
              "every fireball goes off on the closed door's face at x 103.5: " + std::to_string(spent) + " did, " +
                  std::to_string(wrong) + " elsewhere");
    CHECK_MSG(barrel == nullptr || (!barrel->requested && !barrel->blown),
              "and the barrel behind it is never reached");
    std::printf("  level11a (2-12): %d fireballs went off on the closed door in 900 ticks, the first on tick %d\n",
                spent, firstTick);
}

// 2-05, footage F6 clip A: the right-hand carranca's first fireball goes off on
// the lower crate in front of it, which then burns (#22, #30-32).
void ACrateItTouchesBurns() {
    Run run;
    if (!Begin("level4a", run)) return;
    const Fire::Burnable* crate = run.level.fire.FindBurnable("crate_small_no_emissive_ent_916");
    CHECK_MSG(crate != nullptr, "level4a's crate_small_no_emissive_ent_916 is there");
    if (crate == nullptr) return;
    int burnedTick = -1;
    glm::dvec2 spentPx(0.0);
    for (int tick = 0; tick < 240 && burnedTick < 0; ++tick) {
        Tick(run);
        if (crate->burned) {
            burnedTick = tick;
            CHECK_MSG(!run.level.turrets.spentPx.empty(), "the crate is burned on the tick a fireball goes off on it");
            if (!run.level.turrets.spentPx.empty()) spentPx = run.level.turrets.spentPx.front();
        }
    }
    CHECK_MSG(burnedTick >= 0, "carranca_right_901's first fireball burns the crate it touches");
    CHECK_MSG(std::fabs(spentPx.x - 103.5) <= kTickTravelPx,
              "at its face, x 103.5: went off at x " + std::to_string(spentPx.x));
    std::printf("  level4a (2-05): crate_small_no_emissive_ent_916 burned on tick %d, the fireball at (%.2f, %.2f)\n",
                burnedTick, spentPx.x, spentPx.y);
}

// A barrel it touches is asked to go off (#174-188) and does, on the next tick
// (Fire.hpp's flag and poll). 2-12's barrel, with a fireball put beside it: the
// door keeps the carranca's own away.
void ABarrelItTouchesGoesOff() {
    Run run;
    if (!Begin("level11a", run)) return;
    const Fire::Bomb* barrel = run.level.fire.FindBomb("barrel_bomb_ent_1041");
    CHECK_MSG(barrel != nullptr, "level11a's barrel is there");
    if (barrel == nullptr) return;
    for (int tick = 0; tick < 30; ++tick) Tick(run); // let it settle
    const glm::dvec2 barrelPx = Units::ToPixels(run.registry.get<TransformComponent>(barrel->body).position);
    Turrets::Fireball ball;
    ball.name = "beside#1";
    ball.atPx = barrelPx - glm::dvec2(40.0, 0.0);
    ball.velocityPx = glm::dvec2(run.level.turrets.rules.speedPx, 0.0);
    run.level.turrets.fireballs.push_back(ball);
    int askedTick = -1;
    int blownTick = -1;
    for (int tick = 0; tick < 60 && blownTick < 0; ++tick) {
        Tick(run);
        if (barrel->requested && askedTick < 0) askedTick = tick;
        if (barrel->blown) blownTick = tick;
    }
    CHECK_MSG(askedTick >= 0, "a fireball that touches the barrel asks it to go off");
    CHECK_MSG(blownTick == askedTick + 1, "and it goes off on the next tick");
    CHECK_MSG(run.level.fire.blasts == 1, "once");
    std::printf("  level11a (2-12): a fireball put 40 px left of the barrel asked it on tick %d; it went off on %d\n",
                askedTick, blownTick);
}

// Every carranca in the game, idle: where its first fireball goes off, and that
// none goes off on the tick it was fired (a carranca's mouth inside another
// body). The lanes whose first body is STATIC are pinned to the converted
// geometry (lanes_r9.6.txt); a crate or a stone can have moved, and those are
// printed. Chapter 2 and 4's 21 levels, 28 carrancas.
//
// 2-06 (level5a) is printed and not pinned: its first static body is the left
// wall at x 9.5, but lift_ent_832 comes down into the lane on its own and the
// fireball goes off on the lift at x 192.3 on tick 200. The port's lifts run
// unasked, where the original's idles at its marker `a` until a tap sends it
// (the backlog's G2, not this step's); with that built, 2-06's fire reaches the
// wall again.
void EveryLaneEndsOnItsFirstBody() {
    struct Lane {
        const char* level;
        const char* carranca;
        double stopPx; // the centre's x where its circle first meets a static body
    };
    const Lane pinned[] = {
        {"level0a", "carranca_right_ent_792", 278.5},  {"level1a", "carranca_795", 9.5},
        {"level1a", "carranca_796", 73.5},             {"level1a", "carranca_right_ent_799", 121.0},
        {"level2a", "carranca_ent_677", 198.5},        {"level3a", "carranca_677", 137.5},
        {"level6a", "carranca_891", 232.5},
        {"level7a", "carranca_right_821", 247.5},      {"level8a", "carranca_right_ent_1061", 153.5},
        {"level11a", "carranca_right_ent_1048", 103.5}, {"level12a", "carranca_1060", 152.5},
        {"level13a", "carranca_1143", 360.5},          {"level14a", "carranca_1071", 9.5},
        {"level14a", "carranca_right_1075", 345.5},    {"level17a", "carranca_right_1075", 151.5},
        {"level24a", "carranca_right_1126", 487.5},    {"level27a", "carranca_888", 9.5},
        {"level12c", "carranca_right_ent_2339", 438.5}, {"level13c", "carranca_ent_2287", 201.0},
    };
    const char* levels[] = {"level0a",  "level1a",  "level2a",  "level3a",  "level4a",  "level5a",  "level6a",
                            "level7a",  "level8a",  "level11a", "level12a", "level13a", "level14a", "level17a",
                            "level21a", "level22a", "level24a", "level27a", "level28a", "level12c", "level13c"};
    int carrancas = 0;
    int matched = 0;
    int pinnedSeen = 0;
    int bornDead = 0;
    for (const char* name : levels) {
        Run run;
        if (!Begin(name, run)) continue;
        carrancas += static_cast<int>(run.level.turrets.turrets.size());
        // Each fireball by name while it flies, and where the first of each
        // carranca went off.
        std::map<std::string, glm::dvec2> flying;
        std::map<std::string, std::pair<int, glm::dvec2>> firstEnd; // carranca -> (tick, where)
        std::map<std::string, int> firstFired;
        for (int tick = 0; tick < 720; ++tick) {
            int firedBefore = 0;
            for (const Turrets::Turret& turret : run.level.turrets.turrets) firedBefore += turret.fired;
            Tick(run);
            int firedAfter = 0;
            for (const Turrets::Turret& turret : run.level.turrets.turrets) firedAfter += turret.fired;
            std::set<std::string> now;
            int born = 0;
            for (const Turrets::Fireball& ball : run.level.turrets.fireballs) {
                now.insert(ball.name);
                if (flying.find(ball.name) == flying.end()) ++born;
                flying[ball.name] = ball.atPx;
                const std::string owner = ball.name.substr(0, ball.name.rfind('#'));
                if (firstFired.find(owner) == firstFired.end()) firstFired[owner] = tick;
            }
            // Fired this tick and never seen flying: gone on the tick it was fired.
            bornDead += (firedAfter - firedBefore) - born > 0 ? (firedAfter - firedBefore) - born : 0;
            for (auto it = flying.begin(); it != flying.end();) {
                if (now.count(it->first) != 0) {
                    ++it;
                    continue;
                }
                const std::string owner = it->first.substr(0, it->first.rfind('#'));
                // Where it went off: the spent point nearest where it last flew.
                glm::dvec2 wherePx = it->second;
                double best = 1e9;
                for (const glm::dvec2& atPx : run.level.turrets.spentPx) {
                    const double gap = glm::length(atPx - it->second);
                    if (gap < best) {
                        best = gap;
                        wherePx = atPx;
                    }
                }
                const bool wentOff = best < 4.0 * kTickTravelPx;
                if (it->first == owner + "#1" && firstEnd.find(owner) == firstEnd.end()) {
                    firstEnd[owner] = {wentOff ? tick : -tick, wherePx};
                }
                it = flying.erase(it);
            }
        }
        for (const Turrets::Turret& turret : run.level.turrets.turrets) {
            const auto end = firstEnd.find(turret.name);
            const auto fired = firstFired.find(turret.name);
            if (end == firstEnd.end()) {
                std::printf("  %s %s: its first fireball (fired on tick %d) still flies after 720 ticks\n", name,
                            turret.name.c_str(), fired == firstFired.end() ? -1 : fired->second);
                continue;
            }
            const int endTick = end->second.first;
            const glm::dvec2 at = end->second.second;
            std::printf("  %s %s: fired on tick %d, %s on tick %d at (%.2f, %.2f)", name, turret.name.c_str(),
                        fired == firstFired.end() ? -1 : fired->second, endTick >= 0 ? "went off" : "culled",
                        endTick >= 0 ? endTick : -endTick, at.x, at.y);
            for (const Lane& lane : pinned) {
                if (std::string(lane.level) != name || turret.name != lane.carranca) continue;
                ++pinnedSeen;
                const bool here = endTick >= 0 && std::fabs(at.x - lane.stopPx) <= kTickTravelPx;
                if (here) ++matched;
                std::printf("; the static lane says x %.1f: %s", lane.stopPx, here ? "MATCH" : "MISMATCH");
            }
            std::printf("\n");
        }
    }
    CHECK_MSG(carrancas == 28, "the game's 28 carrancas: " + std::to_string(carrancas));
    CHECK_MSG(bornDead == 0, "no fireball goes off on the tick it was fired: " + std::to_string(bornDead));
    CHECK_MSG(pinnedSeen == 19 && matched == 19,
              "each of the 19 static lanes ends on its first static body: " + std::to_string(matched) + " of " +
                  std::to_string(pinnedSeen));
}

void AFireballBurnsThePlayer() {
    Run run;
    if (!Begin(kLevel, run)) return;
    if (run.level.turrets.turrets.empty()) return;

    // Put the player exactly where the first carranca's mouth is. A fireball
    // spawns at the carranca's own position, so the player is in it at once -
    // which is also what proves the kill runs through Hazards, as the boss's
    // does, rather than through a second death of its own.
    const Turrets::Turret& turret = run.level.turrets.turrets.front();
    auto& transform = run.registry.get<TransformComponent>(run.level.player);
    const glm::vec3 at = Units::ToWorld(turret.atPx.x, turret.atPx.y);
    transform.position = glm::vec3(at.x, at.y, transform.position.z);

    const int ticks = static_cast<int>(turret.strideMs / (kStep * 1000.0f)) + 4;
    for (int tick = 0; tick < ticks && !run.level.hazards.playerDied; ++tick) {
        Tick(run);
        // Held there: the step would otherwise drop it out of the way.
        if (!run.level.hazards.playerDied) {
            auto& held = run.registry.get<TransformComponent>(run.level.player);
            held.position = glm::vec3(at.x, at.y, held.position.z);
        }
    }

    CHECK_MSG(run.level.turrets.playerKilled, "a fireball burns a player standing in it");
    CHECK_MSG(run.level.hazards.playerDied, "and it dies as a hazard kills it");
    CHECK_MSG(run.level.hazards.killedBy == run.level.turrets.killedBy,
              "named by the fireball, got '" + run.level.hazards.killedBy + "'");
    // And the fireball is DESTROYED doing it, with the point recorded: the
    // contact callback sets flames and hp 0 on a character and then falls
    // through to destroy(thisEntity) like any other contact
    // (ETHBeginContactCallback_fireball #123-152). The layer puts
    // small_explosion.ent there.
    CHECK_MSG(run.level.turrets.spentPx.size() == 1,
              "and the fireball that did it is destroyed, once: " +
                  std::to_string(run.level.turrets.spentPx.size()) + " recorded");
    bool alive = false;
    for (const Turrets::Fireball& ball : run.level.turrets.fireballs) {
        if (ball.name == run.level.turrets.killedBy) alive = true;
    }
    CHECK_MSG(!alive, "so it is no longer in the air");
    if (!run.level.turrets.spentPx.empty()) {
        std::printf("  %s: the player was burned by %s, which went off at (%.1f, %.1f)\n", kLevel,
                    run.level.hazards.killedBy.c_str(), run.level.turrets.spentPx.front().x,
                    run.level.turrets.spentPx.front().y);
    }
}

// A fireball that kills the player does not fly on.
//
// ETHBeginContactCallback_fireball takes a character it is allowed to kill down
// the same path as anything else it touches: flames and hp 0 at #123-145, then
// #146-152 destroy(thisEntity), which the fireball's own callback reads next
// frame to delete it and add small_explosion.ent. The port used to leave it in
// the air, where it went on to burn crates and set barrels off SECONDS after the
// level was already lost - which is what the four levels named here do. They are
// walked with no input at all, exactly as a capture walks them, so what is
// measured is what the screen shows.
void AKillingFireballDoesNotFlyOn() {
    // The chapter-2 levels whose idle run killed the player with a fireball
    // inside 900 ticks when this rule landed, walked for 1800 now: since step
    // 114 a fireball dies on the first solid thing it touches, so a crate it
    // burns shields the player until it has burned away, and a closed door or
    // a wall for good.
    const char* levels[] = {"level6a", "level7a", "level11a", "level14a", "level22a"};
    // The three whose lanes end on a door or a wall that nothing opens while the
    // player stands idle: 2-07 (door_lift_936), 2-12 (door_lift_1042) and 2-15
    // (the left wall and door_lift_1136).
    const std::set<std::string> shielded = {"level6a", "level11a", "level14a"};
    int killed = 0;
    for (const char* name : levels) {
        Run run;
        if (!Begin(name, run)) continue;
        int tick = 0;
        int destroys = 0;
        int firstDestroy = -1;
        std::string by;
        for (; tick < 1800; ++tick) {
            Tick(run);
            if (!run.level.turrets.spentPx.empty() && firstDestroy < 0) firstDestroy = tick;
            destroys += static_cast<int>(run.level.turrets.spentPx.size());
            if (run.level.turrets.playerKilled && by.empty()) {
                by = run.level.turrets.killedBy;
                // On the very tick of the kill the destroy is recorded, and the
                // fireball is already out of the air.
                CHECK_MSG(!run.level.turrets.spentPx.empty(),
                          std::string(name) + ": the killing fireball is destroyed on the tick it kills");
                bool alive = false;
                for (const Turrets::Fireball& ball : run.level.turrets.fireballs) {
                    if (ball.name == by) alive = true;
                }
                CHECK_MSG(!alive, std::string(name) + ": and is out of the air, so it burns nothing further");
                ++killed;
                break;
            }
        }
        // What each level does to itself, unattended, printed: the sweep's own
        // explanation. 2-08 and 2-23 burn their crates away one fireball at a
        // time before one reaches the player, as footage F6 shows 2-05 doing.
        std::printf("  %s: %d fireball(s) destroyed, first on tick %d; %s\n", name, destroys, firstDestroy,
                    by.empty() ? "the player survives 1800 ticks" : (by + " killed the player on tick " +
                                                                     std::to_string(tick)).c_str());
        if (shielded.count(name) != 0) {
            CHECK_MSG(by.empty(), std::string(name) + ": a door or a wall keeps the fire off the idle player");
        }
    }
    // Two of the five: 2-08 and 2-23 lose their player once their crates are
    // gone; 2-12 no longer does (its closed door), nor do 2-07 and 2-15.
    CHECK_MSG(killed == 2, "two of the five lose their player to a fireball: " + std::to_string(killed));
}

// THROUGH A PORTAL PAIR, ONCE, AND THE PAIR STAYS (planning step 135, the
// backlog's G6). PortalManager::mayTeleport lets fireball.ent through only while
// its teleportedOnce is clear, doesItDestroyPortals is false for it, and
// teleportToOther puts it at the partner's point and turns its 'dir' back
// (bytes 144944..145873, #152-207). It is in a portal while its centre is closer
// than 8 + 14 = 22 to the portal's: EntityGrabber takes half its UNSCALED 16 x 16
// box (turrets.json portal_grab_px) and adds the portal's own radius.
//
// level0a's lane, y 142 from carranca_right_ent_792 at (23, 142) to the block's
// face at x 278.5: A at (100, 142) in front of it and B at (200, 142) beyond A,
// so the fireball goes in at A's left, comes out of B flying LEFT, and meets A
// again from its right - which it flies through, unmoved, having been through
// once. The pair is still there at the end, and the clear button refunds
// neither end.
constexpr double kGrabPx = 8.0 + 14.0;

const Turrets::Fireball* FindFireball(const Run& run, const std::string& name) {
    for (const Turrets::Fireball& ball : run.level.turrets.fireballs) {
        if (ball.name == name) return &ball;
    }
    return nullptr;
}

void AFireballGoesThroughAPairOnceAndThePairStays() {
    Run run;
    if (!Begin(kLevel, run)) return;
    CHECK_MSG(::test::nearly(static_cast<float>(run.level.turrets.rules.portalGrabPx), 8.0f),
              "a fireball's share of the grab is half its unscaled 16 x 16 box: 8");
    Portals::State& portals = run.level.portals;
    const glm::dvec2 aPx(100.0, 142.0);
    const glm::dvec2 bPx(200.0, 142.0);
    CHECK_MSG(portals.TryPlace(aPx) && portals.TryPlace(bPx), "level0a takes a pair on its lane");
    if (portals.placed.size() != 2) return;
    const int idA = portals.placed[0].id;
    const int idB = portals.placed[1].id;

    // Its first fireball, and nothing else of the level's in the air.
    for (int tick = 0; tick < 240 && run.level.turrets.fireballs.empty(); ++tick) Tick(run);
    CHECK_MSG(run.level.turrets.fireballs.size() == 1, "the carranca fires its first fireball");
    if (run.level.turrets.fireballs.size() != 1) return;
    const std::string name = run.level.turrets.fireballs.front().name;
    const glm::dvec2 speed = run.level.turrets.fireballs.front().velocityPx;
    CHECK_MSG(speed.x > 0.0 && speed.y == 0.0, "flying right along the lane");

    // Until it goes in. Every tick before, its centre was 22 or more from A's
    // point as that tick left both.
    int transitTick = -1;
    double closestBeforePx = 1e9;
    for (int tick = 0; tick < 120 && transitTick < 0; ++tick) {
        Tick(run);
        const Turrets::Fireball* ball = FindFireball(run, name);
        if (ball == nullptr) break;
        if (portals.traversals > 0) {
            transitTick = tick;
            break;
        }
        if (!portals.placed.empty()) {
            closestBeforePx = std::min(closestBeforePx, glm::distance(ball->atPx, portals.placed[0].atPx));
        }
    }
    CHECK_MSG(transitTick >= 0, "it goes into A");
    if (transitTick < 0) return;
    const Turrets::Fireball* through = FindFireball(run, name);
    CHECK_MSG(through != nullptr, "and is still in the air");
    if (through == nullptr) return;
    CHECK_MSG(portals.transits.size() == 1 && portals.traversals == 1, "one traversal, recorded for the frame");
    if (portals.transits.size() != 1) return;
    const Portals::Traversal went = portals.transits.front();
    const double inPx = glm::distance(went.travellerPx, went.entryPx);
    CHECK_MSG(inPx < kGrabPx && closestBeforePx >= kGrabPx,
              "it went in on the first tick its centre was within 22 of A's: " + std::to_string(inPx) +
                  ", and no nearer than " + std::to_string(closestBeforePx) + " before");
    CHECK_MSG(glm::distance(went.entryPx, aPx) < 2.5, "by A (its hover is 2 units)");
    CHECK_MSG(portals.placed.size() == 2 && portals.placed[0].id == idA && portals.placed[1].id == idB,
              "THE PAIR STAYS: both ends are still placed");
    if (portals.placed.size() != 2) return;
    CHECK_MSG(through->atPx == portals.placed[1].atPx && went.exitPx == through->atPx,
              "and it is at B's point, as B stands this tick");
    CHECK_MSG(through->velocityPx == -speed, "flying back the way it came: its 'dir' times -1");
    CHECK_MSG(through->teleportedOnce, "marked teleportedOnce");
    CHECK_MSG(portals.placed[0].teleported && portals.placed[1].teleported, "and both ends hasTeleportedSomething");

    // Then back left through A, unmoved: never a jump, never a second traversal,
    // and it goes off beyond A's far side.
    bool passedA = false;
    bool jumped = false;
    glm::dvec2 lastPx = through->atPx;
    glm::dvec2 offPx(0.0);
    for (int tick = 0; tick < 240; ++tick) {
        Tick(run);
        const Turrets::Fireball* ball = FindFireball(run, name);
        if (ball == nullptr) {
            if (!run.level.turrets.spentPx.empty()) offPx = run.level.turrets.spentPx.front();
            break;
        }
        if (glm::distance(ball->atPx, lastPx) > kTickTravelPx + 1e-4) jumped = true;
        if (portals.placed.size() == 2 && glm::distance(ball->atPx, portals.placed[0].atPx) < kGrabPx) {
            passedA = true;
        }
        lastPx = ball->atPx;
    }
    CHECK_MSG(passedA, "it comes back into A's grab");
    CHECK_MSG(!jumped && portals.traversals == 1, "and flies through it unmoved: mayTeleport refuses it now");
    CHECK_MSG(offPx.x < aPx.x - kGrabPx, "going off beyond it, at x " + std::to_string(offPx.x));
    CHECK_MSG(portals.placed.size() == 2, "the pair is there still");

    // PortalManager::killAll(true): no refund for an end that carried something.
    const int used = portals.portalsUsed;
    CHECK_EQ(portals.KillAll(true), 2);
    CHECK_MSG(portals.portalsUsed == used, "the clear button refunds neither end it went through");
    std::printf("  %s: %s went into A at (%.1f, %.1f), %.2f off its centre, on tick %d; came out of B going "
                "(%.0f, %.0f), passed A again and went off at (%.1f, %.1f)\n",
                kLevel, name.c_str(), went.entryPx.x, went.entryPx.y, inPx, transitTick, -speed.x, -speed.y, offPx.x,
                offPx.y);
}

// A LONE PORTAL HAS NO PARTNER (getOther returns null, teleportToOther does
// nothing), so the fireball flies through it to the block, and a portal nothing
// went through is refunded as before.
void ALonePortalCarriesNoFireball() {
    Run run;
    if (!Begin(kLevel, run)) return;
    Portals::State& portals = run.level.portals;
    CHECK_MSG(portals.TryPlace(glm::dvec2(100.0, 142.0)), "one portal on the lane");
    glm::dvec2 offPx(0.0);
    bool through = false;
    for (int tick = 0; tick < 400 && offPx.x == 0.0; ++tick) {
        Tick(run);
        for (const Turrets::Fireball& ball : run.level.turrets.fireballs) through = through || ball.teleportedOnce;
        if (!run.level.turrets.spentPx.empty()) offPx = run.level.turrets.spentPx.front();
    }
    CHECK_MSG(portals.traversals == 0 && !through, "nothing goes through a lone portal");
    CHECK_MSG(std::fabs(offPx.x - 278.5) <= kTickTravelPx,
              "the fireball goes off on the block's face at x 278.5: " + std::to_string(offPx.x));
    CHECK_EQ(portals.KillAll(true), 1);
    CHECK_MSG(portals.portalsUsed == 0, "and the portal, which carried nothing, is refunded");
}

void TheReaderRefusesWhatWouldBeSilent() {
    Turrets::Rules rules;
    std::string error;

    // The port's own file reads.
    CHECK_MSG(Turrets::LoadRules(kPortData + "/turrets.json", rules, error), kPortData + "/turrets.json: " + error);
    CHECK_MSG(::test::nearly(static_cast<float>(rules.speedPx), 130.0f),
              "fireball.ent's speed is 130, not the Godot project's invented 180");
    CHECK_MSG(::test::nearly(static_cast<float>(rules.radiusPx), 9.6f),
              "and its body is a circle of (16 + 16) / 4 x 1.2 = 9.6, shape 2 at its <Scale>");

    // A fireball that never leaves the mouth, and a carranca that fires every
    // tick, would each look like a level that works.
    const std::string zeroSpeed = WriteRules("zero-speed.json", R"({
      "fireball": {"speed_px_s": 0.0, "radius_px": 9.6, "portal_grab_px": 8.0},
      "stride": {"default_ms": 2000.0},
      "cull": {"margin_px": [64.0, 64.0]}
    })");
    CHECK_MSG(!Turrets::LoadRules(zeroSpeed, rules, error), "a speed of nothing is refused");

    const std::string zeroStride = WriteRules("zero-stride.json", R"({
      "fireball": {"speed_px_s": 130.0, "radius_px": 9.6, "portal_grab_px": 8.0},
      "stride": {"default_ms": 0.0},
      "cull": {"margin_px": [64.0, 64.0]}
    })");
    CHECK_MSG(!Turrets::LoadRules(zeroStride, rules, error), "and so is a stride of nothing");

    // A fireball of no size touches nothing, and would fly through every wall
    // again - the very thing step 114 put right.
    const std::string noRadius = WriteRules("no-radius.json", R"({
      "fireball": {"speed_px_s": 130.0, "hit_px": [16.0, 16.0], "portal_grab_px": 8.0},
      "stride": {"default_ms": 2000.0},
      "cull": {"margin_px": [64.0, 64.0]}
    })");
    CHECK_MSG(!Turrets::LoadRules(noRadius, rules, error), "and a fireball with no radius");

    const std::string noPair = WriteRules("no-pair.json", R"({
      "fireball": {"speed_px_s": 130.0, "radius_px": 9.6, "portal_grab_px": 8.0},
      "stride": {"default_ms": 2000.0},
      "cull": {"margin_px": [64.0]}
    })");
    CHECK_MSG(!Turrets::LoadRules(noPair, rules, error), "and a cull margin that is not two numbers");

    // And one with no share of a portal's grab (step 135), which would slip past
    // every portal it passes between 14 and 22 off its centre.
    const std::string noGrab = WriteRules("no-grab.json", R"({
      "fireball": {"speed_px_s": 130.0, "radius_px": 9.6},
      "stride": {"default_ms": 2000.0},
      "cull": {"margin_px": [64.0, 64.0]}
    })");
    CHECK_MSG(!Turrets::LoadRules(noGrab, rules, error), "and a fireball with no share of a portal's grab");
    const std::string withGrab = WriteRules("with-grab.json", R"({
      "fireball": {"speed_px_s": 130.0, "radius_px": 9.6, "portal_grab_px": 8.0},
      "stride": {"default_ms": 2000.0},
      "cull": {"margin_px": [64.0, 64.0]}
    })");
    CHECK_MSG(Turrets::LoadRules(withGrab, rules, error), "which is all that one lacked: " + error);
}

void runTests() {
    TheReaderRefusesWhatWouldBeSilent();
    TheCarrancasAreFound();
    TheFirstShotComesAStrideIn();
    AFireballFliesAtItsOwnSpeed();
    TheTouchIsTheOriginals();
    AFireballDiesOnTheFirstWall();
    AFireballBurnsThePlayer();
    ACharacterItMayNotKillIsPassed();
    AClosedDoorStopsTheFire();
    ACrateItTouchesBurns();
    ABarrelItTouchesGoesOff();
    AKillingFireballDoesNotFlyOn();
    EveryLaneEndsOnItsFirstBody();
    AFireballGoesThroughAPairOnceAndThePairStays();
    ALonePortalCarriesNoFireball();
}

} // namespace

MP_SUITE_MAIN("test_mp_turrets") {
    std::error_code ec;
    if (!std::filesystem::is_directory(kLevels, ec) ||
        !std::filesystem::is_regular_file(kData + "/entity_roles.json", ec)) {
        std::printf("test_mp_turrets SKIPPED - needs the converted levels at %s and Enhanced's data at %s\n",
                    kLevels.c_str(), kData.c_str());
        return 77;
    }
    runTests();
    return ::test::summary("test_mp_turrets", 50);
}
