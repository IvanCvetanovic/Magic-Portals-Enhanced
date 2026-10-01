// Hazards: what kills the player, and where.
//
// Pinned exactly, because the files say it: chapter 1's four hazards. A DEATH AREA
// KILLS ON ITS WHOLE SHAPE (planning step 136, hazards.json `death_area`, decoded):
// level5's is its 60 x 16 octagon and level25a's its 52 x 16. The destroyiers carry no
// trigger_size, so each still kills in LevelRuntime's 16 px fallback box at its node
// (Hazards.hpp). What happens is judged on the tick: the player dies entering the
// octagon anywhere - beside the old box too - and lives past its cut corner, inside
// its bounds. Only the player can die - the characters are the player alone
// (isCharacter), and Hazards::State::Tick is given it and nothing else.
//
// Reads the converted levels and Enhanced's data from outside this repository,
// and skips, saying where it looked, when either is absent.

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "core/Components.hpp"
#include "sim/Game.hpp"
#include "sim/Hazards.hpp"
#include "sim/Units.hpp"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

using namespace MagicPortals;
using Supersonic::RigidBodyComponent;
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

bool Begin(const std::string& levelName, Run& run) {
    std::string error;
    const bool ok = Game::LoadData(kLevels + "/" + levelName + ".tscn", kData,
                                   std::filesystem::temp_directory_path() / "supersonic-test-mp-hazards", run.data,
                                   error) &&
                    Game::Start(run.data, run.registry, run.level, error);
    CHECK_MSG(ok, levelName + ": " + error);
    return ok;
}

void PutPlayer(Run& run, const glm::dvec2& atPx) {
    auto& transform = run.registry.get<TransformComponent>(run.level.player);
    const glm::vec3 at = Units::ToWorld(atPx.x, atPx.y);
    transform.position = glm::vec3(at.x, at.y, transform.position.z);
    run.registry.get<RigidBodyComponent>(run.level.player).velocity = glm::vec3(0.0f);
}

void ChapterOnesFourHazards() {
    // Read off the level files with grep: a death_area in level5, destroyiers in
    // levels 23 and 24, none with a trigger_size.
    struct Expected {
        const char* level;
        const char* name;
        glm::dvec2 at;
    };
    const Expected expected[] = {
        {"level23", "destroyier_ent_1051", {448.0, 344.0}},
        {"level23", "destroyier_ent_1074", {64.0, 390.0}},
        {"level24", "destroyier_ent_1195", {448.0, 504.0}},
    };
    for (const Expected& hazard : expected) {
        Run run;
        if (!Begin(hazard.level, run)) continue;
        const Hazards::Hazard* found = run.level.hazards.FindHazard(hazard.name);
        CHECK_MSG(found != nullptr, std::string(hazard.level) + " has " + hazard.name);
        if (found == nullptr) continue;
        const glm::dvec2 centre = Units::ToPixels(glm::vec3(found->box.centre, 0.0f));
        const glm::dvec2 size = glm::dvec2(found->box.half) * 2.0 * Units::kPixelsPerMetre;
        CHECK_MSG(glm::distance(centre, hazard.at) < 1e-3 && std::fabs(size.x - 16.0) < 1e-3 &&
                      std::fabs(size.y - 16.0) < 1e-3,
                  std::string(hazard.name) + ": the 16 px fallback at its node");
    }
    // The two death areas, on their own octagons: the level's per-instance collision
    // size (60 x 16 and 52 x 16) about the node, the eight points death_area.ent gives.
    struct Area {
        const char* level;
        const char* name;
        glm::dvec2 at;
        glm::dvec2 size;
    };
    const Area areas[] = {
        {"level5", "death_area_ent_800", {513.0, 104.0}, {60.0, 16.0}},
        {"level25a", "death_area_ent_978", {491.0, 182.0}, {52.0, 16.0}},
    };
    const glm::dvec2 unit[] = {{-0.6, 1.0}, {-1.0, 0.6}, {-1.0, -0.6}, {-0.6, -1.0},
                               {0.6, -1.0},  {1.0, -0.6}, {1.0, 0.6},   {0.6, 1.0}};
    for (const Area& area : areas) {
        Run run;
        if (!Begin(area.level, run)) continue;
        CHECK_EQ(run.level.hazards.hazards.size(), std::size_t{1});
        const Hazards::Hazard* found = run.level.hazards.FindHazard(area.name);
        CHECK_MSG(found != nullptr && found->outlinePx.size() == 8, std::string(area.level) + " has " + area.name);
        if (found == nullptr || found->outlinePx.size() != 8) continue;
        bool points = true;
        for (std::size_t i = 0; i < 8; ++i) {
            points = points && glm::distance(found->outlinePx[i], area.at + unit[i] * area.size * 0.5) < 1e-6;
        }
        CHECK_MSG(points, std::string(area.name) + ": its octagon, point by point");
        const glm::dvec2 centre = Units::ToPixels(glm::vec3(found->box.centre, 0.0f));
        const glm::dvec2 size = glm::dvec2(found->box.half) * 2.0 * Units::kPixelsPerMetre;
        CHECK_MSG(glm::distance(centre, area.at) < 1e-3 && glm::distance(size, area.size) < 1e-3,
                  std::string(area.name) + ": its box is the octagon's bounds");
    }
    // And what hazards.json names: the callback's two spellings.
    Run level5;
    if (Begin("level5", level5)) {
        CHECK(level5.data.hazards.wholeShapeNames == (std::vector<std::string>{"death_area", "death_area.ent"}));
    }
}

// The player put at a point and judged there, with no step between: the octagon's
// geometry against the player's collider alone.
bool JudgedAt(const glm::dvec2& atPx) {
    Run run;
    if (!Begin("level5", run)) return false;
    PutPlayer(run, atPx);
    run.level.hazards.Tick(run.registry, run.level.player);
    return run.level.hazards.playerDied;
}

void TheWholeOctagonKills() {
    // The player's collider: a capsule 20 wide (player.json, a guess), so its sides
    // are 10 from its centre. The octagon spans x 483..543, its ends x 483 and 543
    // over y 99.2..108.8, its top y 96 over x 495..531.
    // Beside the old 16 px box (x 505..521), inside the octagon: it dies now.
    CHECK_MSG(JudgedAt({513.0 - 8.0 - 2.0 - 10.0, 104.0}), "beside the old box, inside the octagon, the player dies");
    CHECK_MSG(JudgedAt({474.0, 104.0}), "its side 1 u into the octagon's end: dies");
    CHECK_MSG(!JudgedAt({472.0, 104.0}), "its side 1 u short of the octagon's end: lives");
    CHECK_MSG(JudgedAt({552.0, 104.0}), "the same at the other end: dies");
    CHECK_MSG(!JudgedAt({554.0, 104.0}), "and lives 1 u short of it");
    // Inside the octagon's bounds, past its cut corner: the capsule's foot at (484,
    // 97.5), 1.05 off the chamfer from (483, 99.2) to (495, 96).
    CHECK_MSG(!JudgedAt({484.0, 97.5 - 22.0}), "in the bounds, over the cut corner: lives");
    CHECK_MSG(JudgedAt({513.0, 97.5 - 22.0}), "the same foot over its middle: dies");
    // In the middle, through the whole tick: it dies on that tick, and says where.
    {
        Run run;
        if (!Begin("level5", run)) return;
        PutPlayer(run, glm::dvec2(513.0, 104.0));
        Game::Tick(run.data, run.registry, run.level, 0.0f, kStep);
        CHECK(run.level.hazards.playerDied);
        CHECK(run.level.hazards.killedBy == "death_area_ent_800");
    }
}

void TheLevelsEdgeIsADeathToo() {
    // checkGameLost's SECOND death, which nothing here used to test: the player
    // against the level's own extent. level1's `max` marker is at (512, 256) and
    // hazards.json's margin is (64, 96), so the player lives anywhere inside
    // (-64, -96)..(576, 352) and dies the moment it is outside.
    {
        Run run;
        if (Begin("level1", run)) {
            CHECK_MSG(run.level.hazards.haveBounds, "level1 carries its level_bounds marker");
            CHECK_MSG(std::fabs(run.level.hazards.boundsPx.x - 512.0) < 1e-3 &&
                          std::fabs(run.level.hazards.boundsPx.y - 256.0) < 1e-3,
                      "and it is the level's extent");
            CHECK_MSG(std::fabs(run.level.hazards.marginPx.x - 64.0) < 1e-3 &&
                          std::fabs(run.level.hazards.marginPx.y - 96.0) < 1e-3,
                      "with hazards.json's (64, 96) margin, which is the original's size * 2");
        }
    }

    struct Case {
        const char* what;
        glm::dvec2 at;
        bool dies;
    };
    // +y is DOWN in the port's pixels (Units::ToWorld negates it), so falling
    // into a pit is y PAST the marker - which is the case the owner found.
    const Case cases[] = {
        {"down the pit, past the margin", {256.0, 400.0}, true},
        {"down the pit, still inside the margin", {256.0, 300.0}, false},
        {"off the right", {640.0, 128.0}, true},
        {"off the left", {-80.0, 128.0}, true},
        {"up past the ceiling", {256.0, -120.0}, true},
        {"in the middle of the level", {256.0, 128.0}, false},
    };
    for (const Case& one : cases) {
        Run run;
        if (!Begin("level1", run)) continue;
        PutPlayer(run, one.at);
        Game::Tick(run.data, run.registry, run.level, 0.0f, kStep);
        CHECK_MSG(run.level.hazards.playerDied == one.dies,
                  std::string(one.what) + ": " + (run.level.hazards.playerDied ? "died" : "lived"));
        // And it says which death it was, because the two do not sound alike:
        // a fall plays playDieByFallSound and an hp death plays nothing.
        if (one.dies && run.level.hazards.playerDied) {
            CHECK_MSG(run.level.hazards.diedByFalling, std::string(one.what) + " is a fall");
            CHECK_MSG(run.level.hazards.killedBy == "the fall",
                      std::string(one.what) + " is named a fall, got '" + run.level.hazards.killedBy + "'");
        }
    }
}

void runTests() {
    ChapterOnesFourHazards();
    TheWholeOctagonKills();
    TheLevelsEdgeIsADeathToo();
}

} // namespace

MP_SUITE_MAIN("test_mp_hazards") {
    std::error_code ec;
    if (!std::filesystem::is_directory(kLevels, ec) ||
        !std::filesystem::is_regular_file(kData + "/entity_roles.json", ec)) {
        std::printf("test_mp_hazards: SKIPPED - needs the converted levels at %s\n"
                    "  and Enhanced's data at %s.\n"
                    "  Both live outside this repository; configure with\n"
                    "  -DSUPERSONIC_MAGICPORTALS_LEVELS=... and -DSUPERSONIC_MAGICPORTALS_DATA=...\n",
                    kLevels.c_str(), kData.c_str());
        return 77;
    }
    runTests();
    return ::test::summary("test_mp_hazards", 10);
}
