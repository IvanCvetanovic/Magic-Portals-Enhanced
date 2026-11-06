// Rolling stones, and the walls they break.
//
// Pinned exactly, because the files say it: chapter 1's stones and breakable
// walls, which of the walls the level flags and which break by name, and each
// wall's box. Breaking by name is the owner's testimony, carried in
// demolish.json, so the Godot project's reading - the flag alone - is run too. What
// happens is judged on the tick:
//   - a stone thrown at a wall breaks it and takes its body away, and nothing
//     else;
//   - a stone where the level puts it, and the player walking into a wall,
//     break nothing.
// The contact margin is a guess carried as data, so the edge is found about
// whatever margin the data holds rather than at a number pinned here.
//
// Reads the converted levels and Enhanced's data from outside this repository,
// and skips, saying where it looked, when either is absent.

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "core/Components.hpp"
#include "core/PhysicsSettings.hpp"
#include "sim/Boss.hpp"
#include "sim/Demolish.hpp"
#include "sim/Game.hpp"
#include "sim/Impacts.hpp"
#include "sim/Turrets.hpp"
#include "sim/Units.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

using namespace MagicPortals;
using Supersonic::BoxColliderComponent;
using Supersonic::RigidBodyComponent;
using Supersonic::SphereColliderComponent;
using Supersonic::TransformComponent;

namespace {

const std::string kLevels = MAGICPORTALS_LEVELS_DIR;
const std::string kData = MAGICPORTALS_DATA_DIR;
constexpr float kStep = 1.0f / 60.0f;

struct Run {
    Game::Data data;
    entt::registry registry;
    Game::Level level;
};

// byName false is the Godot project's reading: only what the level flags breaks.
bool Begin(const std::string& levelName, Run& run, bool byName = true) {
    std::string error;
    bool ok = Game::LoadData(kLevels + "/" + levelName + ".tscn", kData,
                             std::filesystem::temp_directory_path() / "supersonic-test-mp-demolish", run.data, error);
    if (ok && !byName) run.data.demolish.breakableNames.clear();
    ok = ok && Game::Start(run.data, run.registry, run.level, error);
    CHECK_MSG(ok, levelName + ": " + error);

    // THE FIRING RATE OFF, on purpose, and this suite is the one that most needs
    // saying so. The original refuses a tap for the first 300 ms of a level and
    // for 400 ms after the last one it took, and both clocks start at zero
    // (placement.json); ThePortalCooldownsHoldATapOff in test_mp_play pins that.
    //
    // ShootAndLand below returns false when Shoot is REFUSED, and its callers
    // read that as "the shot did not land". With the cooldown live, a play
    // through 1-9 would quietly report a failed shot instead of erroring - a
    // wrong answer that looks like a real one. This suite is about what a stone
    // breaks, so the rate limit is turned off where it can be seen.
    run.level.portals.rules.firstPortalMinMs = 0.0;
    run.level.portals.rules.nextPortalMinMs = 0.0;
    return ok;
}

int g_tick = 0; // this suite's tick, for the contact lines below
std::vector<Demolish::Contact> g_seen; // every begin of a stone against a breakable_wall that Tick has seen

void Tick(Run& run, float direction = 0.0f) {
    Game::Tick(run.data, run.registry, run.level, direction, kStep);
    ++g_tick;
    // What the wall's callback judged on this tick (planning step 175): each begin of a stone against a breakable_wall.
    for (const Demolish::Contact& contact : run.level.demolish.contactsNow) {
        g_seen.push_back(contact);
        std::printf("    tick %d: %s begins on %s at %.3f m/s (%s, the history reads %.3f): %s%s\n", g_tick, contact.stone.c_str(),
                    contact.wall.c_str(), contact.speedMs, contact.freeFlight ? "replayed" : "from the history", contact.historyMs,
                    contact.broke ? "the wall breaks" : "the wall holds", contact.stoneDied ? ", the stone dies" : "");
    }
}

// A body put down at a point in the port's pixels, moving at a velocity in them.
void Put(Run& run, entt::entity body, const glm::dvec2& atPx, const glm::dvec2& velocityPx = glm::dvec2(0.0)) {
    auto& transform = run.registry.get<TransformComponent>(body);
    const glm::vec3 at = Units::ToWorld(atPx.x, atPx.y);
    transform.position = glm::vec3(at.x, at.y, transform.position.z);
    run.registry.get<RigidBodyComponent>(body).velocity =
        glm::vec3(Units::ToMetres(velocityPx.x), Units::ToMetres(-velocityPx.y), 0.0f);
}

glm::dvec2 PxOf(const Run& run, entt::entity body) {
    return Units::ToPixels(run.registry.get<TransformComponent>(body).position);
}

std::string Px(const glm::dvec2& at) {
    char text[48];
    std::snprintf(text, sizeof text, "(%.1f, %.1f)", at.x, at.y);
    return text;
}

bool Built(const Run& run, const char* name) {
    const auto found = run.level.built.entities.find(name);
    return found != run.level.built.entities.end() && run.registry.valid(found->second);
}

// ---- What the levels say ------------------------------------------------------

void ChapterOnesStonesAndWalls() {
    // Read off the level files by the census script: every rolling stone, and
    // every entity flagged breakable or named breakable_wall. A quarter-turned
    // wall lies on its side, 126 x 30.
    struct Wall {
        const char* name;
        glm::dvec2 centre;
        glm::dvec2 size;
        bool flagged;
    };
    struct Expected {
        const char* level;
        std::vector<const char*> stones;
        std::vector<Wall> walls;
    };
    const glm::dvec2 upright(30.0, 126.0);
    const glm::dvec2 onItsSide(126.0, 30.0);
    const Expected expected[] = {
        {"level8", {"rolling_stone_ent_597"}, {{"breakable_wall_625", {320.0, 124.0}, upright, false}}},
        {"level12", {"rolling_stone_619"}, {{"breakable_wall_577", {192.0, 282.0}, onItsSide, false}}},
        {"level18", {"rolling_stone_728"}, {{"breakable_wall_731", {447.0, 282.0}, onItsSide, false}}},
        {"level19",
         {"rolling_stone_728", "rolling_stone_1048"},
         {{"breakable_wall_ent_1061", {512.0, 176.0}, onItsSide, true},
          {"breakable_wall_ent_1056", {68.0, 192.0}, upright, true}}},
        {"level21",
         {"rolling_stone_727", "rolling_stone_753"},
         {{"breakable_wall_719", {300.0, 162.0}, upright, false}, {"breakable_wall_755", {332.0, 162.0}, upright, false}}},
        {"level22", {"rolling_stone_ent_877", "rolling_stone_ent_989"}, {}},
        // Walls with no stone: what breaks them is the launcher's, in step 7.
        {"level23", {}, {{"breakable_wall_922", {448.0, 177.0}, onItsSide, false}}},
        {"level24", {}, {{"breakable_wall_922", {190.0, 144.0}, onItsSide, false}}},
    };
    for (const Expected& level : expected) {
        Run run;
        if (!Begin(level.level, run)) continue;
        const Demolish::State& demolish = run.level.demolish;
        CHECK_MSG(demolish.stones.size() == level.stones.size(),
                  std::string(level.level) + " has " + std::to_string(demolish.stones.size()) + " stones");
        for (const char* name : level.stones) {
            CHECK_MSG(demolish.FindStone(name) != nullptr, std::string(level.level) + " has " + name);
        }
        CHECK_MSG(demolish.breakables.size() == level.walls.size(),
                  std::string(level.level) + " has " + std::to_string(demolish.breakables.size()) + " breakables");
        for (const Wall& wall : level.walls) {
            const Demolish::Breakable* found = demolish.FindBreakable(wall.name);
            CHECK_MSG(found != nullptr, std::string(level.level) + " has " + wall.name);
            if (found == nullptr) continue;
            const glm::dvec2 centre = Units::ToPixels(glm::vec3(found->box.centre, 0.0f));
            const glm::dvec2 size = glm::dvec2(found->box.half) * 2.0 * Units::kPixelsPerMetre;
            CHECK_MSG(glm::distance(centre, wall.centre) < 1e-3 && std::fabs(size.x - wall.size.x) < 1e-3 &&
                          std::fabs(size.y - wall.size.y) < 1e-3,
                      std::string(wall.name) + " is " + Px(size) + " at " + Px(centre));
            CHECK_MSG(found->flagged == wall.flagged, std::string(wall.name) + (found->flagged ? " is" : " is not") +
                                                          " flagged");
            CHECK(!found->broken);
        }
    }
}

void TheRemakesReadingBreaksOnlyWhatIsFlagged() {
    // The same levels with the name rule off. level12a's carranca is flagged, so a
    // stone would break it either way; level3a's wall carries no metadata at all.
    struct Count {
        const char* level;
        std::size_t byName;
        std::size_t flagOnly;
    };
    const Count counts[] = {
        {"level8", 1, 0},  {"level12", 1, 0}, {"level18", 1, 0}, {"level19", 2, 2},
        {"level21", 2, 0}, {"level3a", 1, 0}, {"level12a", 2, 2},
    };
    for (const Count& count : counts) {
        Run withName;
        Run flagOnly;
        if (!Begin(count.level, withName) || !Begin(count.level, flagOnly, false)) continue;
        CHECK_MSG(withName.level.demolish.breakables.size() == count.byName,
                  std::string(count.level) + ": " + std::to_string(withName.level.demolish.breakables.size()) +
                      " breakable by flag or name");
        CHECK_MSG(flagOnly.level.demolish.breakables.size() == count.flagOnly,
                  std::string(count.level) + ": " + std::to_string(flagOnly.level.demolish.breakables.size()) +
                      " breakable by flag alone");
    }
}

// ---- Breaking -----------------------------------------------------------------

// level8's stone, thrown right at its wall from 6 px short of touching it, high
// enough to miss the ramp below: the wall's face is at x = 305 and it stands
// from y = 61 to 187.
void ThrowLevel8sStone(Run& run) {
    const Demolish::Stone& stone = run.level.demolish.stones.front();
    Put(run, stone.body, glm::dvec2(305.0 - 30.0 - 6.0, 95.0), glm::dvec2(300.0, 0.0));
}

void AStoneThrownAtAWallBreaksIt() {
    Run run;
    if (!Begin("level8", run)) return;
    if (run.level.demolish.stones.size() != 1) return;
    const std::string stone = run.level.demolish.stones.front().name;
    ThrowLevel8sStone(run);
    int tick = 0;
    for (; tick < 30 && run.level.demolish.Broken() == 0; ++tick) Tick(run);
    std::printf("  level8's stone, thrown at 300 px/s from 6 px: the wall %s after %d tick(s)\n",
                run.level.demolish.Broken() == 1 ? "broke" : "DID NOT BREAK", tick);
    const Demolish::Breakable* wall = run.level.demolish.FindBreakable("breakable_wall_625");
    CHECK(wall != nullptr);
    if (wall == nullptr) return;
    CHECK(wall->broken);
    CHECK_MSG(wall->brokenBy == stone, "broken by " + wall->brokenBy);
    // The wall's body is gone, collision and all, and nothing else is.
    CHECK(!Built(run, "breakable_wall_625"));
    CHECK(Built(run, "block00_ent_624"));
    CHECK(Built(run, "ramp_left_ent_590"));
    CHECK(Built(run, "rolling_stone_ent_597"));
    // Broken once, and it stays broken.
    for (int more = 0; more < 60; ++more) Tick(run);
    CHECK_EQ(run.level.demolish.Broken(), 1);
    CHECK(!run.level.hazards.playerDied);
}

void WithTheFlagAloneTheStoneStops() {
    Run run;
    if (!Begin("level8", run, false)) return;
    CHECK(run.level.demolish.breakables.empty());
    if (run.level.demolish.stones.size() != 1) return;
    const entt::entity stone = run.level.demolish.stones.front().body;
    ThrowLevel8sStone(run);
    for (int tick = 0; tick < 60; ++tick) Tick(run);
    CHECK(Built(run, "breakable_wall_625"));
    CHECK_MSG(PxOf(run, stone).x < 305.0 - 30.0 + 2.0, "the stone is at " + Px(PxOf(run, stone)));
}

// THE STONE'S BODY AT ITS <Scale> (00_order 9.3, planning step 95; the owner's
// standing ruling R9). 2-22's rolling_stone states <Scale 0.8> inline where
// rolling_stone.ent says 1, over the same 60 x 60 box; the engine makes a circle
// of (size.x + size.y) / 4 (ETHPhysicsSimulator.cpp:59) from the box after
// box.Scale (:72-73, ETHCollisionBox.cpp:105-111): 24, where every other stone is
// 30. What it rolls, rests and breaks with is that circle.
void TwoTwentyTwosStoneIsItsScaledCircle() {
    const auto radiusPx = [](const Run& run, const std::string& name) {
        const auto found = run.level.built.entities.find(name);
        if (found == run.level.built.entities.end()) return -1.0;
        const auto* circle = run.registry.try_get<SphereColliderComponent>(found->second);
        return circle != nullptr ? circle->radius * Units::kPixelsPerMetre : -1.0;
    };
    Run scaled;
    if (Begin("level21a", scaled)) {
        CHECK_MSG(scaled.level.demolish.FindStone("rolling_stone_1135") != nullptr, "2-22's stone is a stone");
        const double r = radiusPx(scaled, "rolling_stone_1135");
        CHECK_MSG(std::fabs(r - 24.0) < 1e-4, "its body a circle of 24, got " + std::to_string(r));
        std::printf("  level21a's stone at 0.8: a circle of %.2f\n", r);
    }
    Run plain;
    if (Begin("level8", plain)) {
        const double r = radiusPx(plain, "rolling_stone_ent_597");
        CHECK_MSG(std::fabs(r - 30.0) < 1e-4, "and level8's, at 1, of 30, got " + std::to_string(r));
    }
}

void AStoneWhereTheLevelPutsItBreaksNothing() {
    // level 1-9 spawns the player 58 px from its stone: standing beside one is safe,
    // and the stone left alone stays where it is.
    Run run;
    if (!Begin("level8", run)) return;
    const glm::dvec2 from = PxOf(run, run.level.demolish.stones.front().body);
    for (int tick = 0; tick < 180; ++tick) Tick(run);
    CHECK_EQ(run.level.demolish.Broken(), 0);
    CHECK(!run.level.hazards.playerDied);
    const glm::dvec2 to = PxOf(run, run.level.demolish.stones.front().body);
    CHECK_MSG(glm::distance(from, to) < 8.0, "the stone went from " + Px(from) + " to " + Px(to));
}

void ThePlayerBreaksNothing() {
    Run run;
    if (!Begin("level8", run)) return;
    const double half = run.data.tuning.widthPx * 0.5;
    Put(run, run.level.player, glm::dvec2(305.0 - half - 2.0, 110.0));
    for (int tick = 0; tick < 90; ++tick) Tick(run, 1.0f);
    CHECK_EQ(run.level.demolish.Broken(), 0);
    CHECK(Built(run, "breakable_wall_625"));
    CHECK_MSG(PxOf(run, run.level.player).x < 305.0, "the player is at " + Px(PxOf(run, run.level.player)));
}

void TheMarginIsTheEdge() {
    // On its own: a stone of the game's 30 px and a wall's box, a quarter pixel
    // either side of the margin.
    Demolish::Rules rules;
    std::string error;
    CHECK_MSG(Demolish::LoadRules(std::string(MAGICPORTALS_PORT_DATA_DIR) + "/demolish.json", rules, error), error);
    CHECK(rules.contactMarginPx > 0.25);
    const double radiusPx = 30.0;
    for (const double gapPx : {rules.contactMarginPx - 0.25, rules.contactMarginPx + 0.25}) {
        entt::registry registry;
        Demolish::State state;
        state.rules = rules;
        const entt::entity stone = registry.create();
        registry.emplace<TransformComponent>(stone).position = Units::ToWorld(0.0, 0.0);
        registry.emplace<SphereColliderComponent>(stone).radius = Units::ToMetres(radiusPx);
        state.stones.push_back(Demolish::Stone{"stone", stone});
        const entt::entity wall = registry.create();
        Demolish::Breakable breakable;
        breakable.name = "wall";
        breakable.body = wall;
        const glm::vec3 centre = Units::ToWorld(radiusPx + gapPx + 15.0, 0.0);
        breakable.box.centre = glm::vec2(centre.x, centre.y);
        breakable.box.half = glm::vec2(Units::ToMetres(15.0), Units::ToMetres(63.0));
        state.breakables.push_back(breakable);

        Impacts::Watch watch;
        state.Tick(registry, watch, {});
        const bool within = gapPx < rules.contactMarginPx;
        CHECK_MSG(state.breakables.front().broken == within,
                  std::to_string(gapPx) + " px apart: " + (state.breakables.front().broken ? "broke" : "stood"));
        CHECK(registry.valid(wall) != within);
        CHECK(registry.valid(stone));
    }
}

// ---- planning step 218: a stone the engine's brake stopped short of the wall --------------

void AStoneTheEnginesBrakeStoppedShortStillStrikesTheWall() {
    // 1-22's second wall (the owner: "the second one just rams into wall but does not destroy it"). The engine moves a body and
    // then solves its contacts, and brakes a closing pair inside the two bodies' travel in the step, so a stone that rolls at a wall
    // ends the step 1 to 6 units from it with its velocity taken out - and never again comes within the margin until it creeps up at
    // 0.1 m/s. Box2D finds the pair touching at the START of the next step with the velocity the stone still carries and the callback
    // reads that. The rig stops the stone where the step would have left it (gap `gapPx` from the wall's face, its velocity taken
    // out after BeforeStep read it) and asks the wall's first look.
    Demolish::Rules rules;
    std::string error;
    CHECK_MSG(Demolish::LoadRules(std::string(MAGICPORTALS_PORT_DATA_DIR) + "/demolish.json", rules, error), error);
    constexpr double kRadiusPx = 30.0;
    constexpr double kStepS = 1.0 / 60.0; // the rig's bare tick: SimRatio 1, so the original's step is this long
    struct Case {
        const char* what;
        double gapPx;
        double vx; // m/s of the original's physics (the rig's velocityScale is 1), +x toward the wall
        double vy;
        bool strikes;  // the wall is looked at (a first look is recorded)
        bool breaks;   // and it breaks
    };
    // 3.0 m/s is 150 px/s: 2.5 px a step. The reach is the margin (1 px, demolish.json) and that.
    const Case cases[] = {
        {"fast, stopped 2 px short", 2.0, 3.0, 0.0, true, true},
        {"fast, stopped at the edge of its step (3.4 px)", 3.4, 3.0, 0.0, true, true},
        {"fast, stopped 3.7 px short: more than its step from the wall", 3.7, 3.0, 0.0, false, false},
        {"slow (1 m/s), within its step: looked at, under the gate", 1.5, 1.0, 0.0, true, false},
        {"fast, moving along the wall and not at it", 2.0, 0.0, 3.0, false, false},
        {"fast, moving away from the wall", 2.0, -3.0, 0.0, false, false},
        {"at rest, 2 px short", 2.0, 0.0, 0.0, false, false},
    };
    for (const Case& c : cases) {
        for (const bool withStep : {true, false}) {
            entt::registry registry;
            Demolish::State state;
            state.rules = rules;
            const entt::entity stone = registry.create();
            registry.emplace<TransformComponent>(stone).position = Units::ToWorld(0.0, 0.0);
            registry.emplace<SphereColliderComponent>(stone).radius = Units::ToMetres(kRadiusPx);
            auto& rigid = registry.emplace<RigidBodyComponent>(stone);
            rigid.velocity = glm::vec3(static_cast<float>(c.vx), static_cast<float>(c.vy), 0.0f);
            state.stones.push_back(Demolish::Stone{"stone", stone, true});
            const entt::entity wall = registry.create();
            Demolish::Breakable breakable;
            breakable.name = "wall";
            breakable.wall = true;
            breakable.body = wall;
            const glm::vec3 centre = Units::ToWorld(kRadiusPx + c.gapPx + 15.0, 0.0);
            breakable.box.centre = glm::vec2(centre.x, centre.y);
            breakable.box.half = glm::vec2(Units::ToMetres(15.0), Units::ToMetres(63.0));
            state.breakables.push_back(breakable);
            // The brake's reach is measured from the wall's SHAPE, which a body of the rig needs a collider to have.
            registry.emplace<TransformComponent>(wall).position = centre;
            registry.emplace<BoxColliderComponent>(wall).size = glm::vec3(Units::ToMetres(30.0), Units::ToMetres(126.0), 1.0f);

            Impacts::Watch watch;
            Impacts::Watched watched;
            watched.body = stone;
            watched.kind = Impacts::Kind::Stone;
            watch.BeforeStep(registry, {watched});
            rigid.velocity = glm::vec3(0.0f); // the step's solve took the approach out
            const std::string how = std::string(c.what) + (withStep ? ", with the step given: " : ", with no step given: ");
            state.Tick(registry, watch, {}, withStep ? kStepS : 0.0);
            const bool broke = state.breakables.front().broken;
            const bool looked = !state.contactsNow.empty();
            if (!withStep) {
                // The touch rule as it was: nothing is looked at from 1.5 px or more away.
                CHECK_MSG(!broke && !looked, how + "the touch rule alone looks at nothing from this far");
                continue;
            }
            CHECK_MSG(looked == c.strikes, how + (looked ? "it was looked at" : "it was not looked at"));
            CHECK_MSG(broke == c.breaks, how + (broke ? "the wall broke" : "the wall stood"));
            if (c.strikes && looked) {
                CHECK_MSG(state.contactsNow.front().braked && state.contactsNow.front().atMargin,
                          how + "and the contact says the engine's brake held it short");
                if (c.breaks) {
                    CHECK_MSG(state.contactsNow.front().speedMs > 2.9 && state.contactsNow.front().speedMs < 3.1,
                              how + "it read its carried 3 m/s, not the creep: " + std::to_string(state.contactsNow.front().speedMs));
                    CHECK_MSG(!registry.valid(stone), how + "and the destroyable stone went with the wall");
                }
            }
        }
    }
}

void AStoneThatHeldTheWallStaysHeld() {
    // A slow stone looked at from short of the wall and a fast push after it: the pair has had its begin (Box2D calls the callback
    // once), so the push, at a gap the margin covers, breaks nothing - the rule the first look always kept.
    Demolish::Rules rules;
    std::string error;
    CHECK_MSG(Demolish::LoadRules(std::string(MAGICPORTALS_PORT_DATA_DIR) + "/demolish.json", rules, error), error);
    constexpr double kRadiusPx = 30.0;
    constexpr double kStepS = 1.0 / 60.0;
    entt::registry registry;
    Demolish::State state;
    state.rules = rules;
    const entt::entity stone = registry.create();
    registry.emplace<TransformComponent>(stone).position = Units::ToWorld(0.0, 0.0);
    registry.emplace<SphereColliderComponent>(stone).radius = Units::ToMetres(kRadiusPx);
    auto& rigid = registry.emplace<RigidBodyComponent>(stone);
    state.stones.push_back(Demolish::Stone{"stone", stone, false});
    const entt::entity wall = registry.create();
    Demolish::Breakable breakable;
    breakable.name = "wall";
    breakable.wall = true;
    breakable.body = wall;
    const glm::vec3 centre = Units::ToWorld(kRadiusPx + 1.5 + 15.0, 0.0);
    breakable.box.centre = glm::vec2(centre.x, centre.y);
    breakable.box.half = glm::vec2(Units::ToMetres(15.0), Units::ToMetres(63.0));
    state.breakables.push_back(breakable);
    registry.emplace<TransformComponent>(wall).position = centre;
    registry.emplace<BoxColliderComponent>(wall).size = glm::vec3(Units::ToMetres(30.0), Units::ToMetres(126.0), 1.0f);
    Impacts::Watch watch;
    Impacts::Watched watched;
    watched.body = stone;
    watched.kind = Impacts::Kind::Stone;
    // A slow touch: looked at, held.
    rigid.velocity = glm::vec3(1.0f, 0.0f, 0.0f);
    watch.BeforeStep(registry, {watched});
    rigid.velocity = glm::vec3(0.0f);
    state.Tick(registry, watch, {}, kStepS);
    CHECK_MSG(!state.breakables.front().broken && state.contactsNow.size() == 1, "the slow stone was looked at and the wall held");
    // Then pushed fast, still where the margin covers it: no second begin.
    rigid.velocity = glm::vec3(3.0f, 0.0f, 0.0f);
    watch.BeforeStep(registry, {watched});
    state.Tick(registry, watch, {}, kStepS);
    CHECK_MSG(!state.breakables.front().broken && state.contactsNow.empty(), "a fast push after the slow touch breaks nothing");
}

// ---- planning step 115: one owner of a breakable's breaking --------------------

// A blast grabs a breakable by its FLAG (isBreakable, listing L67923), and only the
// entities demolish.json blast.entities lists read `destroy` here. 2-04's
// breakable_wall_815 breaks by name alone: never in a blast's grab, as a stone
// breaks it still. 2-12's is flagged: grabbed, and taken by Destroyed.
void ABlastGrabsByTheFlag() {
    Demolish::Rules rules;
    std::string error;
    CHECK_MSG(Demolish::LoadRules(std::string(MAGICPORTALS_PORT_DATA_DIR) + "/demolish.json", rules, error), error);
    CHECK_MSG(rules.blastEntities == (std::vector<std::string>{"breakable_wall", "breakable_wall.ent", "carranca", "carranca.ent",
                                                                 "carranca_right", "carranca_right.ent"}),
              "blast.entities is breakable_wall and, since planning step 223, carranca and carranca_right, in both spellings");

    Run flagless;
    if (!Begin("level3a", flagless)) return;
    const Demolish::Breakable* byName = flagless.level.demolish.FindBreakable("breakable_wall_815");
    CHECK_MSG(byName != nullptr && !byName->flagged && !byName->blastBreaks, "2-04's wall breaks by name alone");
    if (byName == nullptr) return;
    CHECK_MSG(!flagless.level.demolish.Grab(byName->body, "a barrel"), "so a blast's grab passes it by");
    CHECK(flagless.level.demolish.Destroyed(flagless.registry).empty());
    CHECK(!byName->broken && flagless.registry.valid(byName->body));

    Run flagged;
    if (!Begin("level11a", flagged)) return;
    const Demolish::Breakable* wall = flagged.level.demolish.FindBreakable("breakable_wall_1040");
    CHECK_MSG(wall != nullptr && wall->flagged && wall->blastBreaks, "2-12's wall is flagged");
    if (wall == nullptr) return;
    const entt::entity body = wall->body;
    CHECK(!flagged.level.demolish.Grab(entt::null, "a barrel"));
    CHECK(flagged.level.demolish.Grab(body, "barrel_bomb_ent_1041"));
    CHECK_MSG(wall->destroy && !wall->broken, "grabbed: its destroy set, not yet taken");
    const std::vector<entt::entity> gone = flagged.level.demolish.Destroyed(flagged.registry);
    CHECK_MSG(gone.size() == 1 && gone.front() == body, "its own callback takes it");
    CHECK(wall->broken && wall->brokenBy == "barrel_bomb_ent_1041" && !wall->destroy);
    CHECK(!flagged.registry.valid(body) && wall->body == entt::null);
    CHECK_MSG(!flagged.level.demolish.Grab(body, "another"), "and a broken wall is grabbed no more");
    CHECK(flagged.level.demolish.Destroyed(flagged.registry).empty());
}

// PLANNING STEP 223 (the owner's 2-15: "I dont know where the other 2 are"). Two of 2-15's five crystals lie inside the colliders of its two
// carrancas (carranca_1071 over crystal 1140, carranca_right_1075 over 1141), and the designers' barrels free them: explode sets `destroy` on a
// flagged breakable it reaches (L67590 #111-122) and carrancaCallback takes the entity (L85350 #123-157). The port took neither before.
// A carranca the blast grabs is taken by Destroyed like a wall, and a taken carranca spits no more (its entity is deleted before the stride arm).
void ABlastTakesACarrancaAndItSpitsNoMore() {
    Run control;
    if (!Begin("level14a", control)) return;
    Run taken;
    if (!Begin("level14a", taken)) return;
    const Demolish::Breakable* carranca = taken.level.demolish.FindBreakable("carranca_1071");
    const Demolish::Breakable* other = taken.level.demolish.FindBreakable("carranca_right_1075");
    CHECK_MSG(carranca != nullptr && other != nullptr && carranca->flagged && carranca->blastBreaks && other->flagged && other->blastBreaks,
              "2-15's two carrancas are flagged breakables a blast takes");
    if (carranca == nullptr || other == nullptr) return;
    const entt::entity body = carranca->body;
    CHECK(taken.level.demolish.Grab(body, "barrel_bomb_ent_1072"));
    const std::vector<entt::entity> gone = taken.level.demolish.Destroyed(taken.registry);
    CHECK_MSG(gone.size() == 1 && gone.front() == body && carranca->broken && carranca->brokenBy == "barrel_bomb_ent_1072" &&
                  !taken.registry.valid(body),
              "the blast's datum takes the carranca, naming the barrel");
    CHECK_MSG(!other->broken && taken.registry.valid(other->body), "and only the one it reached");

    const auto fired = [](const Run& run, const char* name) {
        for (const Turrets::Turret& turret : run.level.turrets.turrets) {
            if (turret.name == name) return turret.fired;
        }
        return -1;
    };
    for (int tick = 0; tick < 600; ++tick) {
        Game::Tick(control.data, control.registry, control.level, 0.0f, kStep);
        Game::Tick(taken.data, taken.registry, taken.level, 0.0f, kStep);
    }
    CHECK_MSG(fired(control, "carranca_1071") > 0, "left alone, the carranca spits within ten seconds");
    CHECK_MSG(fired(taken, "carranca_1071") == 0, "once taken it spits not one fireball");
    CHECK_MSG(fired(taken, "carranca_right_1075") > 0, "while the other, left standing, still does");
}

// A script's deletion and the way back, as the torch's and the dark dragon's are:
// Break takes the body and says which, once; Restore stands it again.
void BreakAndRestoreByName() {
    Run run;
    if (!Begin("level11a", run)) return;
    const Demolish::Breakable* wall = run.level.demolish.FindBreakable("breakable_wall_1040");
    if (wall == nullptr) return;
    const entt::entity body = wall->body;
    CHECK(run.level.demolish.Break(run.registry, "no_such_wall", "a script") == entt::null);
    CHECK(run.level.demolish.Break(run.registry, "breakable_wall_1040", "a script") == body);
    CHECK(wall->broken && wall->brokenBy == "a script" && wall->body == entt::null);
    CHECK(!run.registry.valid(body));
    CHECK_MSG(run.level.demolish.Break(run.registry, "breakable_wall_1040", "again") == entt::null, "once only");
    const entt::entity again = run.registry.create();
    run.level.demolish.Restore("breakable_wall_1040", again);
    CHECK(!wall->broken && wall->brokenBy.empty() && wall->body == again);
}

// ---- planning step 160 (STK): a stone that begins to touch the player fast enough kills him ----------------

// ETHBeginContactCallback_rolling_stone #107-129: where isCharacter(other) and hitIntensity > 2.5 (CMPIf against 2.5f, JNP skips: STRICTLY
// above), other.SetInt('hp', 0). The number is demolish.json's, with its citation; the beholder's rocks run the same callback and
// boss.json keeps the same 2.5 for Boss::Contacts, which is the one thing here that is checked against the other.
void AStoneKillsThePlayerAboveTwoAndAHalfAndNotAtIt() {
    Demolish::Rules rules;
    std::string error;
    CHECK_MSG(Demolish::LoadRules(std::string(MAGICPORTALS_PORT_DATA_DIR) + "/demolish.json", rules, error), error);
    CHECK_MSG(rules.killsPlayerAbove == 2.5, "#115-116: CMPIf 1075838976 = 2.5f");
    Boss::Rules boss;
    CHECK_MSG(Boss::LoadRules(std::string(MAGICPORTALS_PORT_DATA_DIR) + "/boss.json", boss, error), error);
    CHECK_MSG(boss.crushIntensity == rules.killsPlayerAbove,
              "the beholder's rocks are the same callback: boss.json's copy of the number is this one");

    const double under = std::nextafter(2.5, 0.0);
    const double over = std::nextafter(2.5, 3.0);
    CHECK_MSG(!Demolish::KillsPlayer(rules, 0.0) && !Demolish::KillsPlayer(rules, -3.0) && !Demolish::KillsPlayer(rules, 2.49) &&
                  !Demolish::KillsPlayer(rules, under) && !Demolish::KillsPlayer(rules, 2.5),
              "a stone at rest, moving away, just under 2.5 and exactly 2.5 kill nothing");
    CHECK_MSG(Demolish::KillsPlayer(rules, over) && Demolish::KillsPlayer(rules, 2.51) && Demolish::KillsPlayer(rules, 9.0),
              "the next float above 2.5, 2.51 and a hard hit kill");

    // The state: only a kill names the death, and the first one does.
    Demolish::State state;
    state.rules = rules;
    CHECK_MSG(!state.HitPlayer("a_stone", under) && !state.HitPlayer("a_stone", 2.5) && !state.playerKilled && state.killedBy.empty(),
              "hits at or under 2.5 leave him alive and the death unnamed");
    CHECK_MSG(state.HitPlayer("stone_1", over) && state.playerKilled && state.killedBy == "stone_1", "a hit just over kills, naming the stone");
    CHECK_MSG(!state.HitPlayer("stone_2", 9.0) && state.killedBy == "stone_1", "the first to kill names it");
}

// Game::StoneHitsPlayer on 1-09's real stone: a hit over 2.5 records the kill and the next AfterStep folds it into Hazards as a death
// area's - not a fall, named for the stone; a body that is not the level's stone, and a rock of the beholder's (Boss::Contacts' own
// kill), are not judged here.
void AStonesKillIsFoldedIntoHazardsAndTheBeholdersRocksAreLeftToBoss() {
    Run run;
    if (!Begin("level8", run)) return;
    if (run.level.demolish.stones.empty()) {
        CHECK_MSG(false, "1-09 places a stone");
        return;
    }
    const Demolish::Stone stone = run.level.demolish.stones.front();
    CHECK_MSG(!Game::StoneHitsPlayer(run.level, stone.body, 2.4) && !run.level.demolish.playerKilled, "a hit of 2.4 kills nothing");
    CHECK_MSG(!Game::StoneHitsPlayer(run.level, run.level.player, 9.0) && !run.level.demolish.playerKilled,
              "a body that is not one of the level's stones is not judged");
    // The same body listed as one of the beholder's rocks: Boss::Contacts' to judge.
    Boss::Rock rock;
    rock.name = stone.name;
    rock.body = stone.body;
    run.level.boss.rocks.push_back(rock);
    CHECK_MSG(!Game::StoneHitsPlayer(run.level, stone.body, 9.0) && !run.level.demolish.playerKilled,
              "a beholder's rock is left to Boss::Contacts, however hard it hit");
    run.level.boss.rocks.clear();

    CHECK_MSG(Game::StoneHitsPlayer(run.level, stone.body, 2.6) && run.level.demolish.killedBy == stone.name, "a stone at 2.6 kills");
    CHECK_MSG(!run.level.hazards.playerDied, "the fold is AfterStep's: not yet");
    Tick(run);
    CHECK_MSG(run.level.hazards.playerDied && !run.level.hazards.diedByFalling && run.level.hazards.killedBy == stone.name,
              "folded in as a death area's death, named for the stone: " + run.level.hazards.killedBy);
}


// ---- planning step 175: the breakable wall's contact callback ------------------
//
// ETHBeginContactCallback_breakable_wall (listing L68341, demolish.json `wall`): a body with `demolisher` breaks the wall it
// BEGINS to touch only above 1.5 m/s of the original's own physics (75 level units a second), judged on its whole velocity; a
// stone whose `destroyable` is 1 dies with the wall it breaks. One rig, no level: a stone and walls built by hand, and the stones'
// contact watch run over the steps by hand (Impacts::Watch, as Game keeps it), so each rule is pinned apart from the engine's solver.
// The callback reads one contact at up to two instants and the larger reading judges (Demolish::Contact): when the stone comes within
// contactMarginPx of the wall (the history: the fastest it carried toward it) and when the watch finds the pair begun within Box2D's
// skin (Impacts::Begin::Approaching: the free flight's replay where the stone came through the air).

struct Rig {
    entt::registry registry;
    Demolish::State state;
    // What Game keeps for the stones (Level::stoneWatch): the callback judges the begins it finds.
    Impacts::Watch watch;
    // earthquake.json: the watch's skin and the free-flight replay's judgments, as Game hands them to it.
    Earthquake::Rules quake;
    // Whether the stones are watched as a flight, as Game::WatchedStones watches them (pinned apart in GameWatchesTheStones).
    bool freeFlight = false;
    std::vector<Impacts::Begin> begins; // the last step's

    Rig() {
        std::string error;
        CHECK_MSG(Earthquake::LoadRules(std::string(MAGICPORTALS_PORT_DATA_DIR) + "/earthquake.json", quake, error), error);
    }
};

Demolish::Rules LoadedRules() {
    Demolish::Rules rules;
    std::string error;
    CHECK_MSG(Demolish::LoadRules(std::string(MAGICPORTALS_PORT_DATA_DIR) + "/demolish.json", rules, error), error);
    return rules;
}

// The skin the watch finds a pair begun by, in level units: Box2D's polygon radius (earthquake.json impacts.touch_px).
double SkinOf(const Rig& rig) { return Earthquake::ToUnits(rig.quake, rig.quake.touchPx); }

// A stone of the game's 30 px at (atXPx, 0).
entt::entity AddStone(Rig& rig, const char* name, double atXPx, bool destroyable) {
    const entt::entity stone = rig.registry.create();
    rig.registry.emplace<TransformComponent>(stone).position = Units::ToWorld(atXPx, 0.0);
    rig.registry.emplace<SphereColliderComponent>(stone).radius = Units::ToMetres(30.0);
    rig.registry.emplace<RigidBodyComponent>(stone);
    rig.state.stones.push_back(Demolish::Stone{name, stone, destroyable});
    return stone;
}

// A breakable of `sizePx` centred at `centrePx`. isWall is whether its entity's callback is the wall's.
entt::entity AddBox(Rig& rig, const char* name, const glm::dvec2& centrePx, const glm::dvec2& sizePx, bool isWall) {
    const entt::entity body = rig.registry.create();
    const glm::vec3 centre = Units::ToWorld(centrePx.x, centrePx.y);
    rig.registry.emplace<TransformComponent>(body).position = centre;
    auto& box = rig.registry.emplace<Supersonic::BoxColliderComponent>(body);
    box.size = glm::vec3(Units::ToMetres(sizePx.x), Units::ToMetres(sizePx.y), 1.0f);
    Demolish::Breakable breakable;
    breakable.name = name;
    breakable.body = body;
    breakable.wall = isWall;
    breakable.box.centre = glm::vec2(centre.x, centre.y);
    breakable.box.half = glm::vec2(Units::ToMetres(sizePx.x * 0.5), Units::ToMetres(sizePx.y * 0.5));
    rig.state.breakables.push_back(breakable);
    return body;
}

// A wall of 30 x 126 px whose left face stands at x = leftFacePx.
entt::entity AddWall(Rig& rig, const char* name, double leftFacePx, bool isWall) {
    return AddBox(rig, name, glm::dvec2(leftFacePx + 15.0, 0.0), glm::dvec2(30.0, 126.0), isWall);
}

glm::dvec2 PlaceOf(Rig& rig, entt::entity body) { return Units::ToPixels(rig.registry.get<TransformComponent>(body).position); }

void Place(Rig& rig, entt::entity body, const glm::dvec2& atPx) {
    rig.registry.get<TransformComponent>(body).position = Units::ToWorld(atPx.x, atPx.y);
}

// The stone going into a step at `velocityPx` (px/s of the original's physics, +y down), as Game::BeforeStep hands it to the
// watch. A step carried at SimRatio 1 here.
void GoingIn(Rig& rig, entt::entity stone, const glm::dvec2& velocityPx) {
    rig.registry.get<RigidBodyComponent>(stone).velocity =
        glm::vec3(Units::ToMetres(velocityPx.x), Units::ToMetres(-velocityPx.y), 0.0f);
    std::vector<Impacts::Watched> watched;
    for (const Demolish::Stone& held : rig.state.stones) {
        Impacts::Watched one;
        one.body = held.body;
        one.kind = Impacts::Kind::Stone;
        one.freeFlight = rig.freeFlight;
        watched.push_back(one);
    }
    rig.watch.BeforeStep(rig.registry, watched);
}

// The step is over, the stones where it left them: the watch finds the pairs that began, and the callback judges them, as
// Game::AfterStep does. Returns the stones the callback destroyed.
std::vector<entt::entity> Stepped(Rig& rig) {
    const Impacts::FreeFlightRules flight{Earthquake::ToUnits(rig.quake, rig.quake.circleTouchPx), rig.quake.brakeReachSlackUnits,
                                          rig.quake.brakeFrictionCone};
    rig.begins = rig.watch.AfterStep(rig.registry, SkinOf(rig), 1.0 / 60.0, flight);
    return rig.state.Tick(rig.registry, rig.watch, rig.begins);
}

// The stone comes to where it stands now from `fromPx` away, going into the step at `velocityPx`: two steps, because the watch
// finds the begin of a stone it has looked at before (a level's first step, or a stone just put, only marks its pairs).
std::vector<entt::entity> Meets(Rig& rig, entt::entity stone, const glm::dvec2& velocityPx,
                                const glm::dvec2& fromPx = glm::dvec2(-20.0, 0.0)) {
    const glm::dvec2 at = PlaceOf(rig, stone);
    Place(rig, stone, at + fromPx);
    GoingIn(rig, stone, glm::dvec2(0.0));
    Stepped(rig);
    GoingIn(rig, stone, velocityPx);
    Place(rig, stone, at);
    return Stepped(rig);
}

void TheWallsGateIsTheCallbacks() {
    const Demolish::Rules rules = LoadedRules();
    // #16-33: scale(1.5) against the whole velocity's length, both in the scaled metres of the original's Box2D.
    CHECK_MSG(rules.wallBreaksAboveMs == 1.5, "the gate is 1.5 m/s, got " + std::to_string(rules.wallBreaksAboveMs));
    const double gatePx = rules.wallBreaksAboveMs * Units::kPixelsPerMetre;
    CHECK_MSG(gatePx == 75.0, "which is 75 level units a second (D11_check C40), got " + std::to_string(gatePx));
    CHECK(std::find(rules.wallEntities.begin(), rules.wallEntities.end(), "breakable_wall") != rules.wallEntities.end());
    CHECK(std::find(rules.wallEntities.begin(), rules.wallEntities.end(), "breakable_wall.ent") != rules.wallEntities.end());

    struct Case {
        const char* what;
        glm::dvec2 velocityPx;
        bool breaks;
    };
    const Case cases[] = {
        {"at rest", {0.0, 0.0}, false},
        {"creeping at 4.4 px/s (1-22's stone against its wall 2: hit 0.088)", {4.4, 0.0}, false},
        // The engine's velocity is a float, as the original's Box2D's is: a hundredth of a px/s either side is what it holds.
        {"just under the gate", {gatePx - 0.01, 0.0}, false},
        {"exactly the gate (JNP skips on equal)", {gatePx, 0.0}, false},
        {"just over the gate", {gatePx + 0.01, 0.0}, true},
        {"300 px/s", {300.0, 0.0}, true},
        // The WHOLE velocity's length, not a projection along the wall's line: a stone going down past the face, and one at 45
        // degrees whose components are each under the gate.
        {"100 px/s along the wall's face (the length, not a projection)", {0.0, 100.0}, true},
        {"60 px/s each way (84.9 whole)", {60.0, 60.0}, true},
    };
    for (const Case& c : cases) {
        Rig rig;
        rig.state.rules = rules;
        const entt::entity stone = AddStone(rig, "stone", 0.0, false);
        // The stone's edge at x = 30 and the wall's face 0.5 px past it: inside the contact margin, outside Box2D's skin.
        const entt::entity wall = AddWall(rig, "wall", 30.5, true);
        const std::vector<entt::entity> taken = Meets(rig, stone, c.velocityPx);
        CHECK_MSG(rig.state.contactsNow.size() == 1, std::string(c.what) + ": one begin");
        if (rig.state.contactsNow.size() != 1) continue;
        const Demolish::Contact& contact = rig.state.contactsNow.front();
        CHECK_MSG(contact.atMargin && !contact.atSkin, std::string(c.what) + ": read at the margin alone");
        CHECK_MSG(contact.broke == c.breaks, std::string(c.what) + ": the wall " + (contact.broke ? "broke" : "held"));
        CHECK_MSG(rig.registry.valid(wall) != c.breaks, std::string(c.what) + ": the wall's body " +
                                                           (rig.registry.valid(wall) ? "stands" : "is gone"));
        CHECK_MSG(rig.state.breakables.front().broken == c.breaks, std::string(c.what));
        CHECK_MSG(std::fabs(contact.speedMs - glm::length(c.velocityPx) / Units::kPixelsPerMetre) < 1e-5,
                  std::string(c.what) + ": read " + std::to_string(contact.speedMs) + " m/s");
        CHECK_MSG(taken.empty() && rig.registry.valid(stone) && rig.state.stones.size() == 1,
                  std::string(c.what) + ": a stone that is not destroyable is never taken");
    }
    // The contact margin is the same edge for a wall, a quarter pixel either side of it.
    for (const double gapPx : {rules.contactMarginPx - 0.25, rules.contactMarginPx + 0.25}) {
        Rig rig;
        rig.state.rules = rules;
        const entt::entity stone = AddStone(rig, "stone", 0.0, false);
        const entt::entity wall = AddWall(rig, "wall", 30.0 + gapPx, true);
        Meets(rig, stone, glm::dvec2(300.0, 0.0));
        const bool within = gapPx < rules.contactMarginPx;
        CHECK_MSG(rig.registry.valid(wall) != within, std::to_string(gapPx) + " px apart: the wall " + (within ? "breaks" : "stands"));
    }
    // And within Box2D's skin the watch finds the pair begun on that same step: one contact, read at both instants.
    {
        Rig rig;
        rig.state.rules = rules;
        const entt::entity stone = AddStone(rig, "stone", 0.0, false);
        AddWall(rig, "wall", 30.1, true);
        Meets(rig, stone, glm::dvec2(300.0, 0.0));
        CHECK_EQ(rig.state.contactsNow.size(), std::size_t{1});
        if (rig.state.contactsNow.size() == 1) {
            const Demolish::Contact& contact = rig.state.contactsNow.front();
            CHECK_MSG(contact.atMargin && contact.atSkin && contact.broke, "read at the margin and at the skin on one step");
            CHECK_MSG(std::fabs(contact.speedMs - 6.0) < 1e-5 && std::fabs(contact.historyMs - 6.0) < 1e-5 && std::fabs(contact.replayMs - 6.0) < 1e-5,
                      "6 m/s by both: " + std::to_string(contact.historyMs) + ", " + std::to_string(contact.replayMs));
        }
    }
}

void ADestroyableStoneDiesAtTheFirstWallItBreaks() {
    const Demolish::Rules rules = LoadedRules();
    // Two walls, one beside the stone and one under it, both begun on the same step: both break, and the stone goes once.
    {
        Rig rig;
        rig.state.rules = rules;
        const entt::entity stone = AddStone(rig, "thrown#1", 0.0, true);
        const entt::entity beside = AddWall(rig, "right wall", 30.1, true);
        const entt::entity under = AddBox(rig, "lower wall", glm::dvec2(0.0, 30.1 + 15.0), glm::dvec2(126.0, 30.0), true);
        const std::vector<entt::entity> taken = Meets(rig, stone, glm::dvec2(150.0, 150.0), glm::dvec2(-20.0, -20.0));
        CHECK_MSG(!rig.registry.valid(beside) && !rig.registry.valid(under), "both walls the stone began to touch on one step break");
        CHECK_EQ(rig.state.contactsNow.size(), std::size_t{2});
        CHECK_MSG(taken.size() == 1 && taken.front() == stone && !rig.registry.valid(stone), "the stone is destroyed, once");
        CHECK_MSG(rig.state.stones.empty(), "and the level no longer holds it as a stone");
        CHECK_EQ(rig.state.takenNow.size(), std::size_t{1});
        if (rig.state.takenNow.size() == 1) {
            const Demolish::Taken& gone = rig.state.takenNow.front();
            CHECK_MSG(gone.stone.name == "thrown#1" && glm::distance(gone.atPx, glm::dvec2(0.0)) < 1e-3,
                      "kept with its name and where the step left it, for the puff: " + gone.stone.name);
        }
        for (const Demolish::Contact& contact : rig.state.contactsNow) CHECK(contact.broke && contact.stoneDied);
        // The tick's latches still ask who broke the wall: the name answers until the next Tick.
        CHECK_MSG(rig.state.FindStone("thrown#1") != nullptr && rig.state.breakables.front().brokenBy == "thrown#1",
                  "FindStone answers for the stone destroyed this tick");
        rig.state.Tick(rig.registry, rig.watch, {});
        CHECK_MSG(rig.state.FindStone("thrown#1") == nullptr && rig.state.takenNow.empty(), "and not for the next");
    }
    // A stone that is not destroyable lives; one that is, but touches slowly, lives, and the wall stands.
    {
        Rig rig;
        rig.state.rules = rules;
        const entt::entity stone = AddStone(rig, "placed", 0.0, false);
        const entt::entity wall = AddWall(rig, "wall", 30.1, true);
        const std::vector<entt::entity> taken = Meets(rig, stone, glm::dvec2(200.0, 0.0));
        CHECK_MSG(!rig.registry.valid(wall) && rig.registry.valid(stone) && taken.empty() && rig.state.takenNow.empty(),
                  "a stone with no `destroyable` breaks the wall and rolls on");
        CHECK_MSG(!rig.state.contactsNow.empty() && !rig.state.contactsNow.front().stoneDied, "the contact says so");
    }
    {
        Rig rig;
        rig.state.rules = rules;
        const entt::entity stone = AddStone(rig, "creeper", 0.0, true);
        const entt::entity wall = AddWall(rig, "wall", 30.1, true);
        const std::vector<entt::entity> taken = Meets(rig, stone, glm::dvec2(70.0, 0.0));
        CHECK_MSG(rig.registry.valid(wall) && rig.registry.valid(stone) && taken.empty() && rig.state.takenNow.empty(),
                  "a destroyable stone under the gate neither breaks the wall nor dies (#65 skips the whole arm)");
    }
}

void AWallIsJudgedAtTheBeginOfTheTouch() {
    const Demolish::Rules rules = LoadedRules();
    Rig rig;
    rig.state.rules = rules;
    const entt::entity stone = AddStone(rig, "stone", 0.0, false);
    const entt::entity wall = AddWall(rig, "wall", 30.1, true);
    // It begins to touch at 10 px/s: held. It is then pushed to 300 px/s while still touching: no second begin, so the wall stands.
    Meets(rig, stone, glm::dvec2(10.0, 0.0));
    CHECK_MSG(rig.registry.valid(wall) && rig.state.contactsNow.size() == 1 && !rig.state.contactsNow.front().broke, "held at the begin");
    for (int tick = 0; tick < 5; ++tick) {
        GoingIn(rig, stone, glm::dvec2(300.0, 0.0));
        Stepped(rig);
        CHECK_MSG(rig.registry.valid(wall) && rig.state.contactsNow.empty(),
                  "a stone that keeps touching and picks up speed is not a new begin: the wall stands");
    }
    // It leaves, and comes back fast: a begin, and the wall breaks.
    GoingIn(rig, stone, glm::dvec2(300.0, 0.0));
    Place(rig, stone, glm::dvec2(-20.0, 0.0));
    Stepped(rig);
    CHECK_MSG(rig.state.contactsNow.empty() && rig.state.touching.empty() && !rig.watch.Touching(stone, wall), "apart: no pair");
    GoingIn(rig, stone, glm::dvec2(300.0, 0.0));
    Place(rig, stone, glm::dvec2(0.0, 0.0));
    Stepped(rig);
    CHECK_MSG(!rig.registry.valid(wall) && rig.state.contactsNow.size() == 1 && rig.state.contactsNow.front().broke,
              "back at 300 px/s it is a begin, and breaks it");
}

void ABreakableThatIsNotAWallKeepsTheTouchRule() {
    const Demolish::Rules rules = LoadedRules();
    // A carranca is a flagged breakable the stone breaks in the port only (a separate row of the backlog): the gate and the
    // death are the WALL's callback's, so this one still goes on touch, at rest, with no stone taken - and with no begin of the
    // watch's at all, which only the wall's callback reads.
    Rig rig;
    rig.state.rules = rules;
    const entt::entity stone = AddStone(rig, "stone", 0.0, true);
    const entt::entity thing = AddWall(rig, "carranca", 30.5, false);
    const std::vector<entt::entity> taken = rig.state.Tick(rig.registry, rig.watch, {});
    CHECK_MSG(!rig.registry.valid(thing) && rig.state.breakables.front().broken, "broken by touch, at rest");
    CHECK_MSG(taken.empty() && rig.registry.valid(stone) && rig.state.contactsNow.empty(), "no contact of the wall's callback, no stone taken");
}

// The speed is read as the impact watch reads a stone, over the steps and not from the last one alone: the engine's solver brakes
// a closing pair before it meets, so the velocity going into the step that meets it is already slow. A stone that came at 300
// px/s and was braked to 20 by the time it touches reads as 300 - and one that never was fast reads as slow.
void TheSpeedIsTheFastestTheStoneCameTowardTheWall() {
    const Demolish::Rules rules = LoadedRules();
    for (const bool wasFast : {true, false}) {
        Rig rig;
        rig.state.rules = rules;
        const entt::entity stone = AddStone(rig, "stone", -20.0, false);
        const entt::entity wall = AddWall(rig, "wall", 30.1, true);
        // Two steps' worth of going in (the stone apart from the wall): fast, then braked; or slow throughout.
        GoingIn(rig, stone, glm::dvec2(wasFast ? 300.0 : 30.0, 0.0));
        Stepped(rig);
        GoingIn(rig, stone, glm::dvec2(20.0, 0.0));
        // The step has put it against the wall.
        Place(rig, stone, glm::dvec2(0.0, 0.0));
        Stepped(rig);
        CHECK_EQ(rig.state.contactsNow.size(), std::size_t{1});
        if (rig.state.contactsNow.size() != 1) continue;
        const Demolish::Contact& contact = rig.state.contactsNow.front();
        CHECK_MSG(contact.broke == wasFast && rig.registry.valid(wall) == !wasFast,
                  std::string(wasFast ? "300 then 20" : "30 then 20") + ": read " + std::to_string(contact.speedMs) + " m/s, the wall " +
                      (contact.broke ? "broke" : "held"));
        CHECK_MSG(std::fabs(contact.speedMs - (wasFast ? 6.0 : 0.6)) < 1e-5, "the faster of the two: " + std::to_string(contact.speedMs));
        CHECK_MSG(!contact.freeFlight && std::fabs(contact.historyMs - contact.speedMs) < 1e-9,
                  "read from the history, which is what a body not in a free flight is read by");
    }
}

// PLANNING STEP 175, FIX 2: a stone that came to the wall through the air is read, at the watch's begin, as Box2D's own step reads it
// (Impacts.hpp, planning step 162), which is how the layer's impact watch reads the very same contact. A stone let go at rest `gapPx`
// over a wall's top (y = 100), each step taken by hand as the engine would (gravity, then the position, then the brake), with the
// layer's world gravity of 2000 px/s^2 on a 1/60 s tick (33.3 px/s a step). The engine's solver takes half of the approach out at the
// end of step 3, so the stone carries 50 into step 4: the free fall is within the skin on step 4 and reads 4 g dt = 133.3 px/s =
// 2.667 m/s as a free flight, where the history's fastest is the 66.7 it carried into step 3 = 1.333 m/s - under the gate. 3-13's
// stone read 1.333 and 1.953 so, and the wall held where the layer's own read of the same contact says it breaks.
struct Reading {
    bool atMargin;
    bool atSkin;
    double historyMs;
    double replayMs;
    bool broke;
};

void AStoneThatCameThroughTheAirIsReadAsTheOriginalsStepReadsIt() {
    const Demolish::Rules rules = LoadedRules();
    constexpr double kStepS = 1.0 / 60.0;
    constexpr double kGravityPxS2 = 2000.0;
    constexpr double kGravityStepPx = kGravityPxS2 * kStepS;
    constexpr double kMetre = 50.0; // px per metre of the original's physics (ETHPhysicsSimulator.cpp:27)
    const double g = kGravityStepPx / kMetre; // one step of gravity, m/s
    struct Case {
        const char* what;
        double gapPx;
        int brakeAfter; // the step after which the solver halves the approach; 0 none
        bool freeFlight;
        std::vector<Reading> contacts; // what the callback judges, in order
    };
    const Case cases[] = {
        // The stone is within reach of the wall (1 px) on step 3, 0.67 px from it: the history reads 2 g and the wall holds. On step 4
        // the watch finds the pair begun (the braked stone's own position is 0.72 px in): the replay reads 4 g, and the wall breaks.
        {"4 px, braked after step 3, a free flight: held at the margin, broken at the watch's begin", 4.0, 3, true,
         {{true, false, 2.0 * g, 0.0, false}, {false, true, 0.0, 4.0 * g, true}}},
        // The same fall without the flag: the watch's begin reads the history, 2 g again, and there is nothing new to say of it.
        {"the same fall read by the history alone: held, and held again", 4.0, 3, false, {{true, false, 2.0 * g, 0.0, false}}},
        // Within reach and begun on the same step (step 4): both readings of one contact, the larger judging.
        {"4.8 px, braked after step 3, a free flight: both on step 4", 4.8, 3, true, {{true, true, 2.0 * g, 4.0 * g, true}}},
        {"the same fall read by the history alone: both on step 4, 2 g by both", 4.8, 3, false, {{true, true, 2.0 * g, 2.0 * g, false}}},
        // Never braked: the history holds what it carried into step 4, 3 g, a step below the free flight's own.
        {"4.8 px, never braked, a free flight: 3 g by the history, 4 g by the replay", 4.8, 0, true,
         {{true, true, 3.0 * g, 4.0 * g, true}}},
    };
    for (const Case& c : cases) {
        Rig rig;
        rig.state.rules = rules;
        rig.freeFlight = c.freeFlight;
        Supersonic::PhysicsSettings settings;
        settings.gravity = glm::vec3(0.0f, -static_cast<float>(Units::ToMetres(kGravityPxS2)), 0.0f);
        rig.registry.ctx().emplace<Supersonic::PhysicsSettings>(settings);
        const entt::entity stone = AddStone(rig, "stone", 0.0, false);
        const entt::entity wall = AddBox(rig, "wall", glm::dvec2(0.0, 115.0), glm::dvec2(126.0, 30.0), true);
        glm::dvec2 at(0.0, 70.0 - c.gapPx);
        glm::dvec2 carried(0.0);
        std::vector<Demolish::Contact> seen;
        for (int step = 1; step <= 8; ++step) {
            const glm::dvec2 velocity = carried + glm::dvec2(0.0, kGravityStepPx);
            const glm::dvec2 to = at + velocity * kStepS;
            Place(rig, stone, at);
            GoingIn(rig, stone, carried);
            Place(rig, stone, to);
            Stepped(rig);
            for (const Demolish::Contact& contact : rig.state.contactsNow) seen.push_back(contact);
            at = to;
            carried = velocity;
            if (step == c.brakeAfter) carried.y = velocity.y * 0.5;
        }
        CHECK_MSG(seen.size() == c.contacts.size(), std::string(c.what) + ": " + std::to_string(seen.size()) + " contact(s) judged");
        for (std::size_t i = 0; i < seen.size() && i < c.contacts.size(); ++i) {
            const Demolish::Contact& got = seen[i];
            const Reading& want = c.contacts[i];
            const std::string name = std::string(c.what) + ", contact " + std::to_string(i + 1);
            CHECK_MSG(got.atMargin == want.atMargin && got.atSkin == want.atSkin, name + ": read at the margin / at the skin");
            CHECK_MSG(std::fabs(got.historyMs - want.historyMs) < 1e-4, name + ": the history reads " + std::to_string(got.historyMs) +
                                                                               " of " + std::to_string(want.historyMs));
            CHECK_MSG(std::fabs(got.replayMs - want.replayMs) < 1e-4,
                      name + ": the watch's begin reads " + std::to_string(got.replayMs) + " of " + std::to_string(want.replayMs));
            CHECK_MSG(std::fabs(got.speedMs - std::max(want.historyMs, want.replayMs)) < 1e-4 && got.broke == want.broke,
                      name + ": judged by " + std::to_string(got.speedMs) + ", the wall " + (got.broke ? "broke" : "held"));
            CHECK_MSG(got.freeFlight == (want.atSkin && c.freeFlight), name + ": replayed or not");
        }
        const bool broke = !c.contacts.empty() && c.contacts.back().broke;
        CHECK_MSG(rig.registry.valid(wall) != broke, std::string(c.what) + ": the wall " + (broke ? "is gone" : "stands"));
    }
}

// Game keeps that watch for the stones (Level::stoneWatch) through every tick, whether or not a layer runs, and reads it in the
// original's own units (every body is carried at SimRatio times its velocity, ScriptClock.hpp; a bare tick is SimRatio 1). It
// watches each stone as a free flight, as the layer's own watch does: the wall's callback and the layer's impact row read one contact.
void GameWatchesTheStones() {
    Run run;
    if (!Begin("level8", run)) return;
    if (run.level.demolish.stones.size() != 1) return;
    const entt::entity stone = run.level.demolish.stones.front().body;
    const std::vector<Impacts::Watched> watched = Game::WatchedStones(run.level, ScriptClock::Step(kStep));
    CHECK_EQ(watched.size(), std::size_t{1});
    if (watched.size() == 1) {
        CHECK_MSG(watched.front().body == stone && watched.front().kind == Impacts::Kind::Stone, "the level's stone, as a stone");
        CHECK_MSG(watched.front().freeFlight, "read as a free flight where it came through the air");
        CHECK_MSG(std::fabs(watched.front().velocityScale - 1.0) < 1e-12, "at the original's own velocity on a bare tick");
    }
    // At the layer's 60 Hz over the original's 1/30 s world step (SimRatio 2: the body carried at twice the original's
    // velocity) it reports half.
    const ScriptClock::Step doubled(1.0 / 60.0, 1.0f / 60.0f, 1.0 / 30.0, 60.0);
    const std::vector<Impacts::Watched> ticked = Game::WatchedStones(run.level, doubled);
    CHECK_MSG(ticked.size() == 1 && std::fabs(ScriptClock::SimRatio(doubled) - 2.0) < 1e-6 &&
                  std::fabs(ticked.front().velocityScale - 0.5) < 1e-6,
              "and 1 / SimRatio on the layer's tick");
    Put(run, stone, glm::dvec2(100.0, 95.0), glm::dvec2(300.0, 0.0));
    Tick(run);
    Impacts::Begin read;
    CHECK_MSG(run.level.stoneWatch.Carried(stone, read), "the stone is in the level's watch");
    CHECK_MSG(std::fabs(read.velocityPx.x - 300.0) < 1e-3 && !read.recentPx.empty() && std::fabs(read.recentPx.front().x - 300.0) < 1e-3,
              "300 px/s going into the step: " + Px(read.velocityPx));
}

// ETHBeginContactCallback_breakable_wall #52-53: CMPIu against the immediate 1 - `destroyable` is EXACTLY 1, which is not "set". 3-13's
// stone with each value its node could carry.
void DestroyableIsExactlyOne() {
    Run run;
    if (!Begin("level12b", run)) return;
    struct Case {
        const char* text;
        bool destroyable;
    };
    const Case cases[] = {{"1", true}, {"2", false}, {"0", false}, {"", false}, {"1.5", false}};
    for (const Case& c : cases) {
        Tscn::Scene scene = run.data.scene;
        bool set = false;
        for (Tscn::Node& node : scene.nodes) {
            if (node.name != "rolling_stone_1540") continue;
            for (Tscn::Property& property : node.properties) {
                if (property.key != "metadata/destroyable") continue;
                property.value.kind = Tscn::Value::Kind::String;
                property.value.text = c.text;
                set = true;
            }
        }
        CHECK_MSG(set, "3-13's stone carries the datum");
        Demolish::State found;
        std::string error;
        CHECK_MSG(Demolish::Find(scene, run.data.roles, run.level.built, run.data.demolish, found, error), error);
        const Demolish::Stone* stone = found.FindStone("rolling_stone_1540");
        CHECK_MSG(stone != nullptr && stone->destroyable == c.destroyable,
                  std::string("destroyable \"") + c.text + "\": " + (stone != nullptr && stone->destroyable ? "dies" : "lives"));
    }
    // And the datum missing altogether.
    Tscn::Scene scene = run.data.scene;
    for (Tscn::Node& node : scene.nodes) {
        if (node.name != "rolling_stone_1540") continue;
        node.properties.erase(std::remove_if(node.properties.begin(), node.properties.end(),
                                             [](const Tscn::Property& p) { return p.key == "metadata/destroyable"; }),
                              node.properties.end());
    }
    Demolish::State found;
    std::string error;
    CHECK_MSG(Demolish::Find(scene, run.data.roles, run.level.built, run.data.demolish, found, error), error);
    const Demolish::Stone* stone = found.FindStone("rolling_stone_1540");
    CHECK_MSG(stone != nullptr && !stone->destroyable, "no datum: it lives");
}

// ---- the levels: which stones are destroyable ---------------------------------

void TheStonesTheOriginalMakesDestroyableAre() {
    // The census (out/parity/program/WALL/census.md): 12 placed stones on 10 levels carry the uint `destroyable` 1; the other
    // 15 placed stones, and what a launcher throws until entity_launcher sets it, do not.
    struct Level {
        const char* level;
        std::vector<const char*> destroyable;
        std::vector<const char*> plain;
    };
    const Level levels[] = {
        {"level8", {}, {"rolling_stone_ent_597"}},
        {"level12", {"rolling_stone_619"}, {}},
        {"level18", {"rolling_stone_728"}, {}},
        {"level19", {"rolling_stone_728", "rolling_stone_1048"}, {}},
        {"level21", {"rolling_stone_727", "rolling_stone_753"}, {}},
        {"level22", {}, {"rolling_stone_ent_877", "rolling_stone_ent_989"}},
        {"level3a", {}, {"rolling_stone_808"}},
        {"level28a", {}, {"rolling_stone_892"}},
        {"level9b", {"rolling_stone_1198"}, {}},
        {"level11b", {"rolling_stone_1198"}, {}},
        {"level12b", {"rolling_stone_1540"}, {}},
        {"level13b", {"rolling_stone_1231"}, {}},
        {"level17b", {"rolling_stone_2090"}, {}},
        {"level20c", {"rolling_stone_1759"}, {}},
    };
    int destroyable = 0;
    for (const Level& level : levels) {
        Run run;
        if (!Begin(level.level, run)) continue;
        for (const char* name : level.destroyable) {
            const Demolish::Stone* stone = run.level.demolish.FindStone(name);
            CHECK_MSG(stone != nullptr && stone->destroyable, std::string(level.level) + ": " + name + " is destroyable");
            ++destroyable;
        }
        for (const char* name : level.plain) {
            const Demolish::Stone* stone = run.level.demolish.FindStone(name);
            CHECK_MSG(stone != nullptr && !stone->destroyable, std::string(level.level) + ": " + name + " is not");
        }
        // And the walls of the level are walls: every breakable_wall of these levels takes the callback.
        for (const Demolish::Breakable& breakable : run.level.demolish.breakables) {
            const bool aWall = breakable.entity == "breakable_wall" || breakable.entity == "breakable_wall.ent";
            CHECK_MSG(breakable.wall == aWall, std::string(level.level) + ": " + breakable.name + " (" + breakable.entity + ")");
        }
    }
    CHECK_MSG(destroyable == 12, "12 destroyable placed stones are listed, and found: " + std::to_string(destroyable));
}

// ---- the callback on a real level -----------------------------------------------

void AStoneThatTouchesSlowlyBreaksNothingEvenWhenItFallsAlongTheWallAfter() {
    // level8's stone, put 2 px from its wall's face and sent at 40 px/s: it begins to touch the wall under the gate (hit 0.8),
    // then the ramp lets it drop with its side against the face, and its speed grows far past 75 px/s while it touches. The
    // original calls the callback once, at the begin: the wall stands.
    Run run;
    if (!Begin("level8", run)) return;
    if (run.level.demolish.stones.size() != 1) return;
    const entt::entity stone = run.level.demolish.stones.front().body;
    Put(run, stone, glm::dvec2(305.0 - 30.0 - 2.0, 95.0), glm::dvec2(40.0, 0.0));
    std::vector<Demolish::Contact> seen;
    for (int tick = 0; tick < 120; ++tick) {
        Tick(run);
        for (const Demolish::Contact& contact : run.level.demolish.contactsNow) seen.push_back(contact);
    }
    CHECK_MSG(!seen.empty(), "the stone begins to touch the wall");
    for (const Demolish::Contact& contact : seen) {
        std::printf("  level8's slow stone: begins on %s at %.3f m/s: %s\n", contact.wall.c_str(), contact.speedMs,
                    contact.broke ? "breaks" : "holds");
        CHECK_MSG(!contact.broke && contact.speedMs < run.data.demolish.wallBreaksAboveMs, "held at the begin");
    }
    CHECK_EQ(run.level.demolish.Broken(), 0);
    CHECK(Built(run, "breakable_wall_625"));
    CHECK_MSG(run.registry.valid(stone) && !run.level.demolish.stones.empty(), "and the stone is not taken");
}

// ---- level 1-9, played -------------------------------------------------------

// Fires a shot and waits for it to land or fail. True when a portal opened.
bool ShootAndLand(Run& run, const glm::dvec2& atPx) {
    const std::size_t before = run.level.portals.placed.size();
    if (!run.level.portals.Shoot(run.registry, atPx)) return false;
    for (int tick = 0; tick < 600 && run.level.portals.flight; ++tick) Tick(run);
    return run.level.portals.placed.size() > before;
}

// Holds right until the player is at `x` or past it, then lets go and lets it
// settle.
void WalkRightTo(Run& run, double x) {
    for (int tick = 0; tick < 180 && PxOf(run, run.level.player).x < x; ++tick) Tick(run, 1.0f);
    for (int tick = 0; tick < 30; ++tick) Tick(run);
}

void Level8WithTwoShots() {
    // The hand-drawn arrow at (30, 106) points behind the stone, but a shot from
    // the spawn at it meets the stone (test_mp_shot), and so does one from the
    // spawn to anywhere up there low enough for the player to come out of. So
    // the player walks right onto the slope first. From there a shot to (30, 30)
    // passes about 40 px from the stone's centre, over its 30 px. Then one more
    // portal just ahead: walking through it comes out behind the stone,
    // into the gap between it and the level's edge, and walking right pushes it off
    // the ledge.
    //
    // PLANNING STEP 175 (GAMEPLAY): this play used to break the wall and finish the level (4.87 s). The stone it pushes off the
    // ledge comes down the ramp and reaches the wall at 0.70 m/s of the original's physics, under ETHBeginContactCallback_
    // breakable_wall's 1.5 (#16-33), and the original calls that callback once, at the begin: the wall stands and the level is
    // not finished by this play. The wave's route phase finds 1-09 a way that breaks it at speed.
    //
    // PLANNING STEP 213 (GAMEPLAY): the owner ruled the air brake off (player.json movement.air_decay_rate_hz 120): a body off the floor keeps all its x
    // velocity. Walking RIGHT into the near portal now carries the player out of the far one flying right at 144 units a second, so in this rig he lands on the stone's
    // top-left and walks over it, and the stone is never pushed (it only creeps). The play is re-aimed: the near portal is placed to the player's LEFT and he walks
    // LEFT into it, so he comes out of the far portal flying left, meets the level's edge, drops into the gap behind the stone and, holding right from tick 30,
    // pushes it along the ledge and down the ramp: it reaches the wall at 3.78 m/s of the original's physics, over the gate of 1.5, the wall breaks at the begin and
    // the level is finished (5.00 s) - what this play did before step 175. The slow touch that leaves a wall standing is held by the tests above
    // (AStoneThatTouchesSlowlyBreaksNothingEvenWhenItFallsAlongTheWallAfter, TheWallsGateIsTheCallbacks).
    Run run;
    if (!Begin("level8", run)) return;
    g_seen.clear();
    for (int tick = 0; tick < 30; ++tick) Tick(run);
    const entt::entity stone = run.level.demolish.stones.front().body;
    WalkRightTo(run, 195.0);
    // The results are taken before the messages are built: a check's message is
    // built before its condition is.
    const bool far = ShootAndLand(run, glm::dvec2(30.0, 30.0));
    CHECK_MSG(far, "the far shot, from " + Px(PxOf(run, run.level.player)) + ", stopped at " +
                       run.level.portals.lastFailure);
    const bool near = ShootAndLand(run, PxOf(run, run.level.player) + glm::dvec2(-40.0, 0.0));
    CHECK_MSG(near, "the near shot stopped at " + run.level.portals.lastFailure);
    CHECK_EQ(run.level.portals.shotsFailed, 0);
    std::string trace;
    int tick = 0;
    for (; tick < 900 && !run.level.goals.completed && !run.level.hazards.playerDied; ++tick) {
        // Left into the near portal (the first 30 ticks), then right, behind the stone.
        Tick(run, tick < 30 ? -1.0f : 1.0f);
        if (tick % 30 == 0) {
            trace += " " + std::to_string(tick) + ":" + Px(PxOf(run, run.level.player)) + Px(PxOf(run, stone)) +
                     std::to_string(run.level.demolish.Broken());
        }
    }
    std::printf("  level8 with two shots, left through the portal then right: %s after %.2f s, the wall %s\n",
                run.level.goals.completed ? "completed" : "NOT completed", tick * kStep,
                run.level.demolish.Broken() == 1 ? "broken" : "standing");
    CHECK_MSG(!g_seen.empty(), "the pushed stone begins to touch the wall: tick:player/stone/broken" + trace);
    // The begin the original's callback reads, once: the stone comes at the wall over the gate, and the wall breaks at it.
    for (const Demolish::Contact& contact : g_seen) {
        CHECK_MSG(contact.broke && contact.speedMs >= run.data.demolish.wallBreaksAboveMs,
                  "read " + std::to_string(contact.speedMs) + " m/s at the begin: over the gate, and the wall breaks");
    }
    CHECK_MSG(run.level.demolish.Broken() == 1, "the wall is broken: tick:player/stone/broken" + trace);
    CHECK_MSG(run.level.goals.completed, "and the level is finished by this play");
}

void runTests() {
    ChapterOnesStonesAndWalls();
    TheRemakesReadingBreaksOnlyWhatIsFlagged();
    AStoneThrownAtAWallBreaksIt();
    WithTheFlagAloneTheStoneStops();
    TwoTwentyTwosStoneIsItsScaledCircle();
    AStoneWhereTheLevelPutsItBreaksNothing();
    ThePlayerBreaksNothing();
    TheMarginIsTheEdge();
    AStoneTheEnginesBrakeStoppedShortStillStrikesTheWall();
    AStoneThatHeldTheWallStaysHeld();
    ABlastGrabsByTheFlag();
    ABlastTakesACarrancaAndItSpitsNoMore();
    BreakAndRestoreByName();
    AStoneKillsThePlayerAboveTwoAndAHalfAndNotAtIt();
    AStonesKillIsFoldedIntoHazardsAndTheBeholdersRocksAreLeftToBoss();
    TheWallsGateIsTheCallbacks();
    ADestroyableStoneDiesAtTheFirstWallItBreaks();
    AWallIsJudgedAtTheBeginOfTheTouch();
    ABreakableThatIsNotAWallKeepsTheTouchRule();
    TheSpeedIsTheFastestTheStoneCameTowardTheWall();
    AStoneThatCameThroughTheAirIsReadAsTheOriginalsStepReadsIt();
    GameWatchesTheStones();
    TheStonesTheOriginalMakesDestroyableAre();
    DestroyableIsExactlyOne();
    AStoneThatTouchesSlowlyBreaksNothingEvenWhenItFallsAlongTheWallAfter();
    Level8WithTwoShots();
}

} // namespace

MP_SUITE_MAIN("test_mp_demolish") {
    std::error_code ec;
    if (!std::filesystem::is_directory(kLevels, ec) ||
        !std::filesystem::is_regular_file(kData + "/entity_roles.json", ec)) {
        std::printf("test_mp_demolish: SKIPPED - needs the converted levels at %s\n"
                    "  and Enhanced's data at %s.\n"
                    "  Both live outside this repository; configure with\n"
                    "  -DSUPERSONIC_MAGICPORTALS_LEVELS=... and -DSUPERSONIC_MAGICPORTALS_DATA=...\n",
                    kLevels.c_str(), kData.c_str());
        return 77;
    }
    runTests();
    return ::test::summary("test_mp_demolish", 250);
}
