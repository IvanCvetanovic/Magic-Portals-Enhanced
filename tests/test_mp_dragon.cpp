// Chapter 2's boss: the dragon of level 2-32, and the claw that eats its floor.
//
// WHAT THIS SUITE IS REALLY FOR. The inventory (test_mp_start) counts ROLES, and
// the four names the claw destroys have none - they are scenery in
// entity_roles.json. So every one of them could stand untouched for the whole
// level and test_mp_start would still report level31a as playing, on the strength
// of the boss_spawn nodes alone. Only a suite that watches the BODIES go can tell
// a level whose floor really collapses from one where the claw is drawn and
// inert. That is the blindness light_wall.ent had before test_mp_torch, and the
// reason test_mp_bounce watches a body rather than a position.
//
// THE SECOND THING IT PINS IS THE CAMERA, which is this boss's real novelty.
// level31a is the only level in the game that sets auto_camera, and it sets it to
// the dragon: the camera hard-locks to the dragon's x, and the claw's two
// margins are measured from that left edge. Get the clamp wrong at the end of the
// level and the claw would eat the ground under the exit; get the corner-versus-
// centre reading wrong and every margin would sit half a view out. Both are
// asserted below against the numbers level31a actually ships.
//
// Reads the converted levels from outside this repository, and skips, saying
// where it looked, when they are absent.

#include "TestHarness.hpp"

#include "core/Components.hpp"
#include "sim/Dragon.hpp"
#include "sim/Follower.hpp"
#include "sim/Game.hpp"
#include "sim/Roles.hpp"
#include "sim/Tscn.hpp"
#include "sim/Units.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

using namespace MagicPortals;
using Supersonic::TransformComponent;

namespace {

const std::string kLevels = MAGICPORTALS_LEVELS_DIR;
const std::string kData = MAGICPORTALS_DATA_DIR;
const std::string kPortData = MAGICPORTALS_PORT_DATA_DIR;
constexpr float kStep = 1.0f / 60.0f;

// The level the dragon is in, and the numbers level31a.tscn itself carries.
const std::string kLevel = "level31a";
constexpr double kDragonX = -164.0; // off the left edge
constexpr double kDragonY = -84.0;
constexpr double kTakeOffX = 2160.0;
constexpr double kBoundsX = 2632.0;
constexpr double kExitX = 2554.0;

bool Open(const std::string& name, Game::Data& data, entt::registry& registry, Game::Level& level) {
    const std::filesystem::path prisms = std::filesystem::temp_directory_path() / "supersonic-test-mp-dragon";
    std::string error;
    if (!Game::LoadData(kLevels + "/" + name + ".tscn", kData, prisms, data, error) ||
        !Game::Start(data, registry, level, error)) {
        CHECK_MSG(false, name + ": " + error);
        return false;
    }
    return true;
}

void TheRulesRead() {
    Dragon::Rules rules;
    std::string error;
    const bool ok = Dragon::LoadRules(kPortData + "/dragon.json", rules, error);
    CHECK_MSG(ok, error);
    if (!ok) return;

    CHECK(rules.entityName == "dragon.ent");
    CHECK(rules.knightSpawnName == "dragon_knight_spawn");
    CHECK(rules.takeOffName == "take_off");
    CHECK(::test::nearly(static_cast<float>(rules.advancePxS), 45.0f));
    CHECK(::test::nearly(static_cast<float>(rules.climbPxS), -60.0f));
    CHECK(::test::nearly(static_cast<float>(rules.sinkPxS), 25.0f));
    CHECK(::test::nearly(static_cast<float>(rules.fireIntervalMs), 4000.0f));
    CHECK(::test::nearly(static_cast<float>(rules.muzzlePx), 95.0f));

    // SIX waypoints, which is the reading this decode had to be corrected to.
    // Five would leave the claw resting mid-swing: the callback parks it on
    // getNumWaypoints() - 2 and relies on the last two coinciding.
    CHECK_EQ(rules.holdMs.size(), std::size_t{6});
    CHECK_EQ(rules.pathPx.size(), std::size_t{6});
    CHECK(rules.pathPx[4] == rules.pathPx[5]);
    double swingMs = 0.0;
    for (const double hold : rules.holdMs) swingMs += hold;
    CHECK(::test::nearly(static_cast<float>(swingMs), 1500.0f));
    std::printf("  a %zu-point swing of %.0f ms, sweeping every %.0f ms\n", rules.holdMs.size(), swingMs,
                rules.scanMs);

    CHECK(::test::nearly(static_cast<float>(rules.scanMs), 250.0f));
    CHECK(::test::nearly(static_cast<float>(rules.armPx), 94.0f));
    CHECK(::test::nearly(static_cast<float>(rules.crushPx), 64.0f));
    // The crush line inside the arm line, or it would destroy what it never
    // swung at. LoadRules refuses the other way round.
    CHECK(rules.crushPx <= rules.armPx);
    CHECK_EQ(rules.crushes.size(), std::size_t{4});

    // And the one number from outside the sim.
    CHECK(::test::nearly(static_cast<float>(rules.viewWidthPx), 455.0f));

    // THE LAGGED POINT AND THE ANGLE (decode D7). The start offset is the raw
    // (2, 0) of ins 44-45 in the level's units - 2 / 2.8125 - because that one is
    // added with no g_scale.scale, where the take-off offset at ins 189-201 goes
    // through it and is 128 x 64 as written. dragon.json says so; this pins that
    // the two are NOT in the same space.
    CHECK(::test::nearly(static_cast<float>(rules.followerStartPx.x), 2.0f / 2.8125f));
    CHECK(rules.followerStartPx.y == 0.0);
    CHECK(rules.followerTakeOffPx == glm::dvec2(128.0, 64.0));
    CHECK_EQ(rules.followerStrideMs, 2000u);
    CHECK_EQ(rules.followerRateMs, 200u);
    CHECK(::test::nearly(static_cast<float>(rules.angleFixDeg), 110.0f));
    CHECK(::test::nearly(static_cast<float>(rules.angleSubDeg), 10.0f));
    CHECK(::test::nearly(static_cast<float>(rules.angleMinDeg), 340.0f));
    CHECK(::test::nearly(static_cast<float>(rules.angleMaxDeg), 370.0f));
    CHECK(::test::nearly(static_cast<float>(rules.muzzleSubDeg), 110.0f));
    CHECK(::test::nearly(static_cast<float>(rules.muzzleAddDeg), 29.0f));

    // The sheet as dragon.ent cuts it, the cell level31a places it on, and the
    // two windows its callback drives the cell over.
    CHECK_EQ(rules.columns, 2);
    CHECK_EQ(rules.rows, 2);
    CHECK_EQ(rules.startFrame, 2);
    CHECK_EQ(rules.animFrames, 4);
    CHECK(::test::nearly(static_cast<float>(rules.animOpenFromMs), 3000.0f));
    CHECK(::test::nearly(static_cast<float>(rules.animCloseFromMs), 500.0f));
    CHECK(::test::nearly(static_cast<float>(rules.animSpanMs), 400.0f));
}

// THE LAGGED POINT ITSELF (Follower.hpp; decode D7 sections 1 and 2). The table
// below is D7's own unit-step response on the script's 16 ms clock, with the
// destination set before each update - the dragon's order. It is the one place the
// retarget period (13 frames, not 12) and the smoothEnd shaping are pinned as
// numbers rather than as behaviour.
void TheFollowerLagsAsTheScriptsDoes() {
    Follower::Point point = Follower::Make(glm::dvec2(0.0), 2000, 200);
    CHECK(point.At() == glm::dvec2(0.0));

    // Force moves the point and touches neither clock nor destination.
    Follower::Point forced = Follower::Make(glm::dvec2(3.0, 4.0), 2000, 200);
    forced.SetDestination(glm::dvec2(9.0, 9.0));
    forced.Force(glm::dvec2(-1.0, 2.0));
    CHECK(forced.At() == glm::dvec2(-1.0, 2.0));
    CHECK(forced.dest == glm::dvec2(9.0, 9.0));
    CHECK_EQ(forced.switchMs, 0u);

    const struct Step {
        int frames;
        double at;
    } steps[] = {{1, 0.0}, {13, 0.0}, {14, 0.0126}, {27, 0.1732}, {30, 0.2047},
                 {60, 0.4718}, {120, 0.7674}, {180, 0.8975}};
    int frame = 0;
    std::size_t next = 0;
    for (; frame < 180 && next < sizeof(steps) / sizeof(steps[0]); ) {
        ++frame;
        point.SetDestination(glm::dvec2(1.0, 0.0));
        point.Update(16);
        if (frame != steps[next].frames) continue;
        const double at = point.At().x;
        CHECK_MSG(std::fabs(at - steps[next].at) < 0.0001,
                  "after " + std::to_string(frame) + " frames of 16 ms the point is at " +
                      std::to_string(at) + ", D7 says " + std::to_string(steps[next].at));
        ++next;
    }
    CHECK_EQ(next, sizeof(steps) / sizeof(steps[0]));
    std::printf("  the follower closes 16.3%% of its gap every 13 frames: %.4f after 180\n", point.At().x);

    // Arrived exactly, it stops retargeting: getCurrentPos() == m_destPos is an
    // exact comparison, and a point that has converged lets m_switchTime grow.
    Follower::Point arrived = Follower::Make(glm::dvec2(5.0, 5.0), 2000, 200);
    arrived.SetDestination(glm::dvec2(5.0, 5.0));
    for (int i = 0; i < 100; ++i) arrived.Update(16);
    CHECK(arrived.At() == glm::dvec2(5.0, 5.0));
    CHECK(arrived.switchMs > 200u);

    // The frame as the script sees it: the engine truncates to whole ms
    // (ETHEngine.cpp:141-147), so a 60 Hz tick is 16 and not 16.67. Feeding it
    // 16.667 would retarget on the 12th frame, because 12 x 16.667 is 200.00001.
    CHECK_EQ(Follower::FrameMs(1.0 / 60.0), 16u);
    CHECK_EQ(Follower::FrameMs(1.0 / 30.0), 33u);
}

// WHERE IT LOOKS, as a map rather than as a band (decode D7 section 3.3). The
// clamp alone would pass for an atan2 with its arguments the wrong way round -
// every reading would be 90 degrees out and still inside [340, 370] - so the whole
// table is pinned, sign and all.
void TheAngleFollowsTheLaggedPoint() {
    Dragon::Rules rules;
    std::string error;
    if (!Dragon::LoadRules(kPortData + "/dragon.json", rules, error)) return;

    // `aim` is the dragon LESS the lagged point, normalised. A target to the
    // RIGHT of the dragon therefore gives an aim pointing LEFT.
    const auto aimFor = [](double depressionDeg) {
        // The target sits to the right, `depression` below the horizontal in
        // y-down pixels; the aim is its negation.
        const double r = depressionDeg * 3.14159265358979323846 / 180.0;
        return glm::dvec2(-std::cos(r), -std::sin(r));
    };
    const struct Case {
        double depressionDeg;
        double angleDeg;
        const char* what;
    } cases[] = {
        {-20.0, 370.0, "above and to the right"},
        {0.0, 370.0, "level and to the right"},
        {10.0, 360.0, "10 degrees below"},
        {20.0, 350.0, "20 degrees below"},
        {30.0, 340.0, "30 degrees below"},
        {60.0, 340.0, "steeply below"},
    };
    for (const Case& one : cases) {
        const double angle = Dragon::AngleFor(rules, aimFor(one.depressionDeg));
        CHECK_MSG(std::fabs(angle - one.angleDeg) < 0.01,
                  std::string(one.what) + ": " + std::to_string(angle) + ", not " + std::to_string(one.angleDeg));
    }
    // Straight below, and anywhere behind it, is the floor of the band.
    CHECK(std::fabs(Dragon::AngleFor(rules, glm::dvec2(0.0, -1.0)) - 340.0) < 0.01);
    CHECK(std::fabs(Dragon::AngleFor(rules, glm::dvec2(1.0, 0.0)) - 340.0) < 0.01);
    // And a zero vector, which gs2d's Normalize answers for a zero length: the
    // angle falls to the floor rather than going NaN.
    CHECK(std::fabs(Dragon::AngleFor(rules, glm::dvec2(0.0, 0.0)) - 340.0) < 0.01);
    std::printf("  the head turns over [%.0f, %.0f] as the lagged point rises and falls\n", rules.angleMinDeg,
                rules.angleMaxDeg);
}

// THE MOUTH'S TWO WINDOWS (ins 531-625), which drive the cell AND the particle
// system, so they are one rule.
void TheMouthOpensAndClosesOnItsOwnClock() {
    Dragon::Rules rules;
    std::string error;
    if (!Dragon::LoadRules(kPortData + "/dragon.json", rules, error)) return;

    const auto look = [&](double elapsedMs, bool tookOff, bool completed) {
        return Dragon::LookAt(rules, elapsedMs, tookOff, completed);
    };
    // Before the first window nothing is written at all: the cell is level31a's
    // own spriteFrame until 500 ms.
    CHECK(!look(0.0, false, false).setsFrame);
    CHECK(!look(499.0, false, false).setsFrame);
    // It closes over 500..900, 3 -> 0, killing the system every tick of it.
    const struct Cell {
        double at;
        int frame;
    } closing[] = {{501.0, 3}, {650.0, 2}, {750.0, 1}, {850.0, 0}, {899.0, 0}};
    for (const Cell& one : closing) {
        const Dragon::Look l = look(one.at, false, false);
        CHECK_MSG(l.setsFrame && l.kills && !l.plays && l.frame == one.frame,
                  "at " + std::to_string(one.at) + " ms the mouth is cell " + std::to_string(l.frame));
    }
    // Between the windows it is left alone, closed and quiet.
    CHECK(!look(901.0, false, false).setsFrame);
    CHECK(!look(2999.0, false, false).setsFrame);
    // And opens over 3000..3400, 0 -> 3, playing the system every tick of it.
    const Cell opening[] = {{3001.0, 0}, {3150.0, 1}, {3250.0, 2}, {3350.0, 3}, {3399.0, 3}};
    for (const Cell& one : opening) {
        const Dragon::Look l = look(one.at, false, false);
        CHECK_MSG(l.setsFrame && l.plays && !l.kills && l.frame == one.frame,
                  "at " + std::to_string(one.at) + " ms the mouth is cell " + std::to_string(l.frame));
    }
    // Past the open window it HOLDS OPEN, because the clock only returns to zero
    // when it fires: a dragon with nothing in sight keeps its mouth at 3.
    CHECK(!look(3500.0, false, false).setsFrame);
    CHECK(!look(20000.0, false, false).setsFrame);
    // The level finished closes it at once, wherever the clock stands (the second
    // leg of the three-way ||).
    const Dragon::Look finished = look(2000.0, false, true);
    CHECK(finished.setsFrame && finished.kills && finished.frame == 0);
    // And once it has taken off the whole block is skipped: the cell holds.
    CHECK(!look(600.0, true, false).setsFrame);
    CHECK(!look(3200.0, true, false).setsFrame);
    std::printf("  the mouth closes over %.0f..%.0f ms and opens over %.0f..%.0f\n", rules.animCloseFromMs,
                rules.animCloseFromMs + rules.animSpanMs, rules.animOpenFromMs,
                rules.animOpenFromMs + rules.animSpanMs);
}

// Refused, each for a reason a level could actually have.
void TheRulesAreRefusedWhenTheyDoNotMakeADragon() {
    const std::filesystem::path scratch =
        std::filesystem::temp_directory_path() / "supersonic-test-mp-dragon-rules";
    std::error_code ec;
    std::filesystem::create_directories(scratch, ec);

    const struct Case {
        const char* name;
        const char* body;
    } cases[] = {
        {"five waypoints",
         R"({"flight":{"entity":"d","take_off":"t","advance_px_s":45,"climb_px_s":-60,"sink_px_s":25,
             "hover_ceiling_px":48,"bob_amplitude_px":13,"bob_frequency":0.1},
             "fire":{"interval_ms":4000,"muzzle_px":95,"muzzle_sub_deg":110,"muzzle_add_deg":29},
             "knight":{"spawn":"k"},
             "camera":{"view_width_px":455},
             "follower":{"start_offset_px":[0.7111111111111111,0],"after_take_off_px":[128,64],
             "stride_ms":2000,"rate_ms":200},
             "angle":{"fix_deg":110,"sub_deg":10,"min_deg":340,"max_deg":370},
             "picture":{"columns":2,"rows":2,"start_frame":2,"frames":4,"open_from_ms":3000,
             "close_from_ms":500,"span_ms":400},
             "claw":{"entity":"c","scan_ms":250,"arm_px":94,"crush_px":64,"lead_px":-100,"lift_px":72,
             "bob_amplitude_px":13,"bob_frequency":0.06,"hold_ms":[600,200,200,500,0],
             "path_px":[[0,0],[140,0],[120,40],[100,80],[0,0]],"angles_deg":[40,0,0,-20,-15],
             "crushes":["a"]}})"},
        {"a crush line outside the arm line",
         R"({"flight":{"entity":"d","take_off":"t","advance_px_s":45,"climb_px_s":-60,"sink_px_s":25,
             "hover_ceiling_px":48,"bob_amplitude_px":13,"bob_frequency":0.1},
             "fire":{"interval_ms":4000,"muzzle_px":95,"muzzle_sub_deg":110,"muzzle_add_deg":29},
             "knight":{"spawn":"k"},
             "camera":{"view_width_px":455},
             "follower":{"start_offset_px":[0.7111111111111111,0],"after_take_off_px":[128,64],
             "stride_ms":2000,"rate_ms":200},
             "angle":{"fix_deg":110,"sub_deg":10,"min_deg":340,"max_deg":370},
             "picture":{"columns":2,"rows":2,"start_frame":2,"frames":4,"open_from_ms":3000,
             "close_from_ms":500,"span_ms":400},
             "claw":{"entity":"c","scan_ms":250,"arm_px":64,"crush_px":94,"lead_px":-100,"lift_px":72,
             "bob_amplitude_px":13,"bob_frequency":0.06,"hold_ms":[600,200,200,500,0,0],
             "path_px":[[0,0],[140,0],[120,40],[100,80],[0,0],[0,0]],"angles_deg":[40,0,0,-20,-15,-15],
             "crushes":["a"]}})"},
        {"a dragon that never advances",
         R"({"flight":{"entity":"d","take_off":"t","advance_px_s":0,"climb_px_s":-60,"sink_px_s":25,
             "hover_ceiling_px":48,"bob_amplitude_px":13,"bob_frequency":0.1},
             "fire":{"interval_ms":4000,"muzzle_px":95,"muzzle_sub_deg":110,"muzzle_add_deg":29},
             "knight":{"spawn":"k"},
             "camera":{"view_width_px":455},
             "follower":{"start_offset_px":[0.7111111111111111,0],"after_take_off_px":[128,64],
             "stride_ms":2000,"rate_ms":200},
             "angle":{"fix_deg":110,"sub_deg":10,"min_deg":340,"max_deg":370},
             "picture":{"columns":2,"rows":2,"start_frame":2,"frames":4,"open_from_ms":3000,
             "close_from_ms":500,"span_ms":400},
             "claw":{"entity":"c","scan_ms":250,"arm_px":94,"crush_px":64,"lead_px":-100,"lift_px":72,
             "bob_amplitude_px":13,"bob_frequency":0.06,"hold_ms":[600,200,200,500,0,0],
             "path_px":[[0,0],[140,0],[120,40],[100,80],[0,0],[0,0]],"angles_deg":[40,0,0,-20,-15,-15],
             "crushes":["a"]}})"},
        {"a claw with nothing it may take",
         R"({"flight":{"entity":"d","take_off":"t","advance_px_s":45,"climb_px_s":-60,"sink_px_s":25,
             "hover_ceiling_px":48,"bob_amplitude_px":13,"bob_frequency":0.1},
             "fire":{"interval_ms":4000,"muzzle_px":95,"muzzle_sub_deg":110,"muzzle_add_deg":29},
             "knight":{"spawn":"k"},
             "camera":{"view_width_px":455},
             "follower":{"start_offset_px":[0.7111111111111111,0],"after_take_off_px":[128,64],
             "stride_ms":2000,"rate_ms":200},
             "angle":{"fix_deg":110,"sub_deg":10,"min_deg":340,"max_deg":370},
             "picture":{"columns":2,"rows":2,"start_frame":2,"frames":4,"open_from_ms":3000,
             "close_from_ms":500,"span_ms":400},
             "claw":{"entity":"c","scan_ms":250,"arm_px":94,"crush_px":64,"lead_px":-100,"lift_px":72,
             "bob_amplitude_px":13,"bob_frequency":0.06,"hold_ms":[600,200,200,500,0,0],
             "path_px":[[0,0],[140,0],[120,40],[100,80],[0,0],[0,0]],"angles_deg":[40,0,0,-20,-15,-15],
             "crushes":[]}})"},
        // A band the wrong way round would pin every angle to one end of it and
        // the head would never move - which a capture of a 30-degree sweep would
        // not obviously show.
        {"an angle band the wrong way round",
         R"({"flight":{"entity":"d","take_off":"t","advance_px_s":45,"climb_px_s":-60,"sink_px_s":25,
             "hover_ceiling_px":48,"bob_amplitude_px":13,"bob_frequency":0.1},
             "fire":{"interval_ms":4000,"muzzle_px":95,"muzzle_sub_deg":110,"muzzle_add_deg":29},
             "knight":{"spawn":"k"},
             "camera":{"view_width_px":455},
             "follower":{"start_offset_px":[0.7111111111111111,0],"after_take_off_px":[128,64],
             "stride_ms":2000,"rate_ms":200},
             "angle":{"fix_deg":110,"sub_deg":10,"min_deg":370,"max_deg":340},
             "picture":{"columns":2,"rows":2,"start_frame":2,"frames":4,"open_from_ms":3000,
             "close_from_ms":500,"span_ms":400},
             "claw":{"entity":"c","scan_ms":250,"arm_px":94,"crush_px":64,"lead_px":-100,"lift_px":72,
             "bob_amplitude_px":13,"bob_frequency":0.06,"hold_ms":[600,200,200,500,0,0],
             "path_px":[[0,0],[140,0],[120,40],[100,80],[0,0],[0,0]],"angles_deg":[40,0,0,-20,-15,-15],
             "crushes":["a"]}})"},
        // And a cell outside the sheet's own cut: dragon.ent cuts 2 x 2, so there
        // is no frame 4.
        {"a start frame outside the cut",
         R"({"flight":{"entity":"d","take_off":"t","advance_px_s":45,"climb_px_s":-60,"sink_px_s":25,
             "hover_ceiling_px":48,"bob_amplitude_px":13,"bob_frequency":0.1},
             "fire":{"interval_ms":4000,"muzzle_px":95,"muzzle_sub_deg":110,"muzzle_add_deg":29},
             "knight":{"spawn":"k"},
             "camera":{"view_width_px":455},
             "follower":{"start_offset_px":[0.7111111111111111,0],"after_take_off_px":[128,64],
             "stride_ms":2000,"rate_ms":200},
             "angle":{"fix_deg":110,"sub_deg":10,"min_deg":340,"max_deg":370},
             "picture":{"columns":2,"rows":2,"start_frame":4,"frames":4,"open_from_ms":3000,
             "close_from_ms":500,"span_ms":400},
             "claw":{"entity":"c","scan_ms":250,"arm_px":94,"crush_px":64,"lead_px":-100,"lift_px":72,
             "bob_amplitude_px":13,"bob_frequency":0.06,"hold_ms":[600,200,200,500,0,0],
             "path_px":[[0,0],[140,0],[120,40],[100,80],[0,0],[0,0]],"angles_deg":[40,0,0,-20,-15,-15],
             "crushes":["a"]}})"},
    };

    int refused = 0;
    for (const Case& one : cases) {
        const std::filesystem::path file = scratch / "dragon.json";
        {
            std::ofstream out(file, std::ios::binary);
            out << one.body;
        }
        Dragon::Rules rules;
        std::string error;
        const bool ok = Dragon::LoadRules(file.string(), rules, error);
        CHECK_MSG(!ok, std::string("refused: ") + one.name);
        if (!ok) ++refused;
    }
    CHECK_EQ(refused, 6);
}

// Two entity names answer true, and no other boss's does.
void ItAnswersForItsOwnTwoNodesAndNoOthers() {
    Dragon::Rules rules;
    std::string error;
    if (!Dragon::LoadRules(kPortData + "/dragon.json", rules, error)) return;

    const struct Case {
        const char* level;
        int expected; // boss_spawn nodes this dragon claims
    } cases[] = {
        {"level31a", 2}, // dragon.ent and dragon_knight_spawn
        {"level31", 0},  // the beholder's adder
        {"level31b", 0}, // the ghost
        {"level31c", 0}, // the dark dragon, which is NOT built
    };

    for (const Case& one : cases) {
        Tscn::Scene scene;
        Roles::Table roles;
        if (!Tscn::Load(kLevels + "/" + one.level + ".tscn", scene, error)) {
            CHECK_MSG(false, error);
            continue;
        }
        if (!Roles::Load(kData + "/entity_roles.json", roles, error)) {
            CHECK_MSG(false, error);
            continue;
        }
        int claimed = 0;
        int spawns = 0;
        for (const Tscn::Node& node : scene.nodes) {
            if (node.parent != "." || Roles::RoleOf(roles, node) != Roles::kBossSpawn) continue;
            ++spawns;
            if (Dragon::Plays(rules, node)) ++claimed;
        }
        CHECK_MSG(claimed == one.expected, std::string(one.level) + " has " + std::to_string(claimed) +
                                               " node(s) this dragon claims, of " + std::to_string(spawns) +
                                               " boss spawn(s)");
    }

    // And the role itself is still not blanket-played: the dark dragon has to
    // stay inert, and IsPorted is per-role and cannot say "for three of four".
    CHECK(!Roles::IsPorted(Roles::kBossSpawn));
}

void TheLevelPlacesItWhereTheFileSaysIt() {
    Game::Data data;
    entt::registry registry;
    Game::Level level;
    if (!Open(kLevel, data, registry, level)) return;

    CHECK(level.dragon.present);
    if (!level.dragon.present) return;
    CHECK(::test::nearly(static_cast<float>(level.dragon.virtualPx.x), static_cast<float>(kDragonX)));
    CHECK(::test::nearly(static_cast<float>(level.dragon.virtualPx.y), static_cast<float>(kDragonY)));
    CHECK(level.dragon.hasTakeOff);
    CHECK(::test::nearly(static_cast<float>(level.dragon.takeOffPx.x), static_cast<float>(kTakeOffX)));
    CHECK(level.dragon.hasClaw);
    CHECK(::test::nearly(static_cast<float>(level.dragon.boundsX), static_cast<float>(kBoundsX)));
    CHECK(!level.dragon.tookOff);

    // The claw idles motionless, parked where setCurrentWaypoint(n - 2) puts it.
    // Asserted as the LISTING states it rather than as "already armed": the two
    // coincide from the first tick onward, and the first sweep is 250 ms away, so
    // pinning the decoded index is the claim that can actually be checked.
    CHECK_EQ(level.dragon.claw.waypoint, static_cast<int>(level.dragon.rules.holdMs.size()) - 2);
    CHECK(!level.dragon.crushables.empty());
    Game::Tick(data, registry, level, 0.0f, kStep);
    CHECK(level.dragon.claw.IsLastFrame(level.dragon.rules));

    // Twenty-two platforms it may take, counted from the level rather than
    // assumed - and every one of them a body that was actually built.
    CHECK_EQ(level.dragon.crushables.size(), std::size_t{22});
    for (const Dragon::Crushable& one : level.dragon.crushables) {
        CHECK_MSG(registry.valid(one.entity), one.name + " was built");
    }
    std::printf("  %s: the dragon at (%.0f, %.0f), take_off at %.0f, %zu platform(s) the claw may take\n",
                kLevel.c_str(), level.dragon.virtualPx.x, level.dragon.virtualPx.y, level.dragon.takeOffPx.x,
                level.dragon.crushables.size());

    // And no other level in the game has one.
    for (const char* other : {"level30a", "level31", "level31b", "level31c"}) {
        Game::Data second;
        entt::registry secondRegistry;
        Game::Level secondLevel;
        if (!Open(other, second, secondRegistry, secondLevel)) continue;
        CHECK_MSG(!secondLevel.dragon.present, std::string(other) + " places no dragon");
    }
}

// The camera is the whole of the claw's frame of reference, so it is pinned on
// its own before anything that depends on it.
void TheCameraIsTheDragonHeldInsideTheLevel() {
    Game::Data data;
    entt::registry registry;
    Game::Level level;
    if (!Open(kLevel, data, registry, level)) return;
    if (!level.dragon.present) return;

    // It starts OFF the left edge, so the camera is clamped to 0 and stays there
    // until the dragon reaches the level. camMin is (0,0).
    CHECK(level.dragon.atPx.x < 0.0);
    CHECK(::test::nearly(static_cast<float>(level.dragon.CameraLeftPx().x), 0.0f));
    // And y is pinned to 0, which is the whole of the vertical: the level is one
    // view tall.
    CHECK(::test::nearly(static_cast<float>(level.dragon.CameraLeftPx().y), 0.0f));

    // Ten seconds in, the dragon has advanced 45 px a second and the camera is
    // with it.
    for (int tick = 0; tick < 600; ++tick) Game::Tick(data, registry, level, 0.0f, kStep);
    const double flown = level.dragon.virtualPx.x - kDragonX;
    CHECK(flown > 440.0 && flown < 460.0);
    CHECK(::test::nearly(static_cast<float>(level.dragon.CameraLeftPx().x),
                         static_cast<float>(level.dragon.atPx.x)));
    std::printf("  after 10 s: the dragon at x %.1f, the camera's left edge at %.1f\n", level.dragon.atPx.x,
                level.dragon.CameraLeftPx().x);

    // And the clamp at the far end: the camera never shows past the level.
    level.dragon.atPx.x = kBoundsX + 1000.0;
    const double most = kBoundsX - level.dragon.viewWidthPx;
    CHECK(::test::nearly(static_cast<float>(level.dragon.CameraLeftPx().x), static_cast<float>(most)));
    // Which is what keeps the exit out of the claw's reach - the assertion the
    // whole level rests on.
    CHECK(most + level.dragon.rules.crushPx < kExitX);
}

// THE ONE THAT MATTERS: the bodies go.
void TheClawTakesTheFloorAndTheRegistryAgrees() {
    Game::Data data;
    entt::registry registry;
    Game::Level level;
    if (!Open(kLevel, data, registry, level)) return;
    if (!level.dragon.present) return;

    // Nothing has gone before the dragon reaches the level.
    CHECK_EQ(level.dragon.crushed, 0);

    const std::size_t placed = level.dragon.crushables.size();
    // Thirty seconds: the dragon is at x ~1186 by then, so the destroy line has
    // swept most of the level.
    for (int tick = 0; tick < 1800; ++tick) Game::Tick(data, registry, level, 0.0f, kStep);

    CHECK(level.dragon.crushed > 0);
    std::printf("  after 30 s: %d of %zu platform(s) taken, the camera's left edge at %.1f\n", level.dragon.crushed,
                placed, level.dragon.CameraLeftPx().x);

    const double line = level.dragon.CameraLeftPx().x + level.dragon.rules.crushPx;
    // The sweep runs on a 250 ms clock of its own, not every frame, so the line
    // can have moved a sweep's worth of travel past a platform that is still
    // standing and perfectly correct. That slack is the claw's own cadence -
    // 45 px a second for a quarter second - and not a tolerance for being wrong.
    const double sweptPx = level.dragon.rules.advancePxS * level.dragon.rules.scanMs / 1000.0;
    int gone = 0;
    for (const Dragon::Crushable& one : level.dragon.crushables) {
        if (!one.gone) {
            // What stands, stands because it is beyond the destroy line. This is
            // the check that would catch a claw eating the level from the wrong
            // end.
            CHECK_MSG(one.atPx.x >= line - sweptPx - 1.0,
                      one.name + " stands, and is no more than one sweep behind the destroy line");
            continue;
        }
        ++gone;
        // What went, went because it was behind the line - and its BODY went with
        // it. A claw that only set a flag would pass every count above.
        CHECK_MSG(one.atPx.x < line + 1.0, one.name + " went, and was behind the destroy line");
        CHECK_MSG(one.entity == entt::null, one.name + " was dropped from the claw's list");
    }
    CHECK_EQ(gone, level.dragon.crushed);

    // And the level's own map of built bodies no longer names them, so nothing
    // else in the port can reach a destroyed entity through it.
    for (const Dragon::Crushable& one : level.dragon.crushables) {
        if (!one.gone) continue;
        CHECK_MSG(level.built.entities.find(one.name) == level.built.entities.end(),
                  one.name + " is gone from the built level");
    }
}

// The exit's ground survives the whole level, which is what makes 2-32 winnable.
void TheExitIsNeverEaten() {
    Game::Data data;
    entt::registry registry;
    Game::Level level;
    if (!Open(kLevel, data, registry, level)) return;
    if (!level.dragon.present) return;

    // A full minute: longer than the dragon's whole run. It reaches take_off at
    // (2160 + 164) / 45 = 51.6 s.
    for (int tick = 0; tick < 3600; ++tick) Game::Tick(data, registry, level, 0.0f, kStep);

    CHECK(level.dragon.tookOff);
    std::printf("  after 60 s: took off %s, %d platform(s) taken in all\n", level.dragon.tookOff ? "yes" : "no",
                level.dragon.crushed);

    for (const Dragon::Crushable& one : level.dragon.crushables) {
        if (!one.gone) continue;
        CHECK_MSG(one.atPx.x < kExitX, one.name + " went, and stood short of the exit");
    }
}

// THE RUN A CAPTURE OF 2-32 ACTUALLY SEES, measured rather than assumed: the tick
// the camera's hard lock engages, the ticks the mouth opens and closes, the first
// shot and the first swing of the claw. The visuals plan's step 5.2 reads its
// gates on these frames, and they are printed here so the record quotes a
// measurement and not an estimate. Nothing is touched: this is the level playing
// on its own with the player standing where it spawns.
void TheRunItPlaysOnItsOwn() {
    Game::Data data;
    entt::registry registry;
    Game::Level level;
    if (!Open(kLevel, data, registry, level)) return;
    if (!level.dragon.present) return;

    int lockedAt = -1;    // the first tick the dragon's x is inside the level
    int shutAt = -1;      // the first tick the close window has it back at cell 0
    int openingAt = -1;   // the first tick of the open window: the system is restarted
    int wideAt = -1;      // and the first tick the mouth is fully open
    int firedAt = -1;     // the first shot
    int closedAt = -1;    // and the first tick after it that it is closed again
    int armedAt = -1;     // the first tick the claw restarts its swing
    double lowest = 1e9;
    double highest = -1e9;
    int previousWaypoint = level.dragon.claw.waypoint;
    for (int tick = 1; tick <= 600; ++tick) {
        Game::Tick(data, registry, level, 0.0f, kStep);
        if (lockedAt < 0 && level.dragon.atPx.x >= 0.0) lockedAt = tick;
        if (shutAt < 0 && level.dragon.frame == 0) shutAt = tick;
        if (openingAt < 0 && level.dragon.systemRestarted) openingAt = tick;
        if (openingAt > 0 && wideAt < 0 && level.dragon.frame == level.dragon.rules.animFrames - 1) wideAt = tick;
        if (firedAt < 0 && level.dragon.fired > 0) firedAt = tick;
        if (firedAt > 0 && closedAt < 0 && level.dragon.frame == 0) closedAt = tick;
        if (armedAt < 0 && previousWaypoint != 0 && level.dragon.claw.waypoint == 0) armedAt = tick;
        previousWaypoint = level.dragon.claw.waypoint;
        lowest = std::min(lowest, level.dragon.angleDeg);
        highest = std::max(highest, level.dragon.angleDeg);
    }
    std::printf("  over 600 ticks: camera locks at f%d, mouth shuts at f%d, opens from f%d (wide at f%d),"
                " fires at f%d, shuts again at f%d, claw swings at f%d\n",
                lockedAt, shutAt, openingAt, wideAt, firedAt, closedAt, armedAt);
    // And the frames a capture of this level is read on, each with what the port
    // drew there: the required side of the visuals plan's 5.2 gate table.
    {
        Game::Data again;
        entt::registry registry2;
        Game::Level level2;
        if (Open(kLevel, again, registry2, level2) && level2.dragon.present) {
            const int wanted[] = {150, 180, 186, 192, 198, 204, 240, 270, 276, 282, 288, 300,
                                  360, 390, 402, 414, 420, 426, 432, 438, 444};
            std::size_t next = 0;
            for (int tick = 1; tick <= 480 && next < sizeof(wanted) / sizeof(wanted[0]); ++tick) {
                Game::Tick(again, registry2, level2, 0.0f, kStep);
                if (tick != wanted[next]) continue;
                ++next;
                const glm::dvec2 claw =
                    level2.dragon.claw.AtPx(level2.dragon.rules, level2.dragon.CameraLeftPx());
                std::printf("    f%-3d cell %d  angle %6.2f  at (%7.2f, %7.2f)  camera %7.2f  claw wp %d"
                            " at (%7.2f, %6.2f) angle %5.1f\n",
                            tick, level2.dragon.frame, level2.dragon.angleDeg, level2.dragon.atPx.x,
                            level2.dragon.atPx.y, level2.dragon.CameraLeftPx().x, level2.dragon.claw.waypoint,
                            claw.x, claw.y, level2.dragon.claw.AngleDeg(level2.dragon.rules));
            }
        }
    }
    std::printf("  and the angle stays in [%.1f, %.1f] of the band [%.0f, %.0f]\n", lowest, highest,
                level.dragon.rules.angleMinDeg, level.dragon.rules.angleMaxDeg);

    // Each of the four happens, and in this order: the mouth opens before it
    // fires, and the claw cannot swing until the camera has crossed the level's
    // first platform.
    CHECK(lockedAt > 0 && shutAt > 0 && openingAt > 0 && wideAt > 0 && firedAt > 0 && closedAt > 0 && armedAt > 0);
    CHECK(shutAt < openingAt && openingAt < wideAt && wideAt < firedAt && firedAt < closedAt);
    CHECK(lockedAt < armedAt);
    // The angle never leaves the clamp, which is the cheapest guard against a
    // getAngle with its arguments swapped.
    CHECK(lowest >= level.dragon.rules.angleMinDeg - 1e-9);
    CHECK(highest <= level.dragon.rules.angleMaxDeg + 1e-9);

    // AND THE THIRD LEG OF THE WINDOW'S `||` IS LIVE, not decoration: a finished
    // level closes the mouth and kills the system wherever the clock stands (ins
    // 588-591). It is reachable on this level - the exit is at x 2554 and the
    // dragon does not take off until x 2160, which it reaches at 51.6 s, so a
    // player who finishes before then is finishing in front of a dragon that is
    // still flying - and the layer goes on ticking the level while the door's
    // effect runs. Driven through Tick itself rather than through LookAt, because
    // what this pins is that Tick passes the flag on.
    {
        const int before = level.dragon.frame;
        level.dragon.Tick(registry, level.player, true, kStep);
        CHECK_MSG(level.dragon.frame == 0 && level.dragon.systemKilled,
                  "a finished level shuts the mouth (cell " + std::to_string(before) + " -> " +
                      std::to_string(level.dragon.frame) + ")");
    }
}

// It fires at the player, and only when it can see one.
void ItSpitsAtWhatItCanSee() {
    Game::Data data;
    entt::registry registry;
    Game::Level level;
    if (!Open(kLevel, data, registry, level)) return;
    if (!level.dragon.present) return;

    const glm::dvec2 playerPx = Units::ToPixels(registry.get<TransformComponent>(level.player).position);

    // Put it just above the player with its clock already past the interval, and
    // one tick should produce one fireball. BOTH positions are planted: virtualPx
    // is what the flight integrates, and atPx is the position the last tick DREW,
    // which is what the aim reads (the callback writes its move after its angle).
    //
    // THE LAGGED POINT IS PLANTED TOO. This first case converges it on the player,
    // which is where a still player leaves it, so the shot flies at the player and
    // the muzzle can be checked against a known angle. switchMs is zeroed so the
    // update below cannot retarget and move the point mid-test.
    const glm::dvec2 perch = playerPx + glm::dvec2(0.0, -56.0);
    const std::size_t before = level.turrets.fireballs.size();
    level.dragon.virtualPx = perch;
    level.dragon.atPx = perch;
    level.dragon.follower.Force(playerPx);
    level.dragon.follower.switchMs = 0;
    level.dragon.elapsedMs = level.dragon.rules.fireIntervalMs + 1.0;
    Game::Tick(data, registry, level, 0.0f, kStep);

    CHECK(level.turrets.fireballs.size() > before);
    CHECK(level.dragon.fired > 0);
    if (level.turrets.fireballs.size() > before) {
        const Turrets::Fireball& made = level.turrets.fireballs.back();
        // A carranca's kills and a fire diamond's does not; this one kills.
        CHECK(made.killsPlayer);
        CHECK(made.name.rfind(level.dragon.name, 0) == 0);
        // Aimed at the lagged point, which here IS the player: straight down, and
        // exactly so, because the point was forced onto it.
        const double speed = std::sqrt(made.velocityPx.x * made.velocityPx.x +
                                       made.velocityPx.y * made.velocityPx.y);
        CHECK(::test::nearly(static_cast<float>(speed), static_cast<float>(level.turrets.rules.speedPx)));
        CHECK(made.velocityPx.y > 0.0); // the player is below it
        CHECK(std::fabs(made.velocityPx.x) < 1e-9);
        // THE MUZZLE, as the decode gives it (D7 section 3.4): a = rad(-A - 81),
        // and the shot starts at the dragon LESS 95 x (sin a, -cos a). With the
        // player straight below, the aim points straight up, getAngle reads 180
        // degrees and the clamp pins A at its floor of 340 - where the decode's own
        // table says the muzzle offset is (83.1, 46.1), in front of the head and
        // below it, 95 px away. Taken back one step of flight, because
        // Turrets::Fire flies what it was handed inside this same tick.
        const glm::dvec2 muzzle =
            made.atPx - made.velocityPx * static_cast<double>(kStep) - level.dragon.atPx;
        const double reach = std::sqrt(muzzle.x * muzzle.x + muzzle.y * muzzle.y);
        CHECK(::test::nearly(static_cast<float>(level.dragon.angleDeg), 340.0f));
        CHECK(::test::nearly(static_cast<float>(reach), static_cast<float>(level.dragon.rules.muzzlePx)));
        CHECK_MSG(std::fabs(muzzle.x - 83.089) < 0.02 && std::fabs(muzzle.y - 46.057) < 0.02,
                  "the muzzle sits at (" + std::to_string(muzzle.x) + ", " + std::to_string(muzzle.y) +
                      "), the decode says (83.09, 46.06) - printed (83.1, 46.1) rounded");
        std::printf("  a fireball from %.0f px above the player, at %.0f px/s, out of a muzzle at (%.1f, %.1f)\n",
                    56.0, speed, muzzle.x, muzzle.y);
    }
    // And its clock restarted, so the next one is an interval away.
    CHECK(level.dragon.elapsedMs < level.dragon.rules.fireIntervalMs);

    // R19, AND IT IS THE WHOLE OF THE RULING: the shot flies along the LAGGED
    // direction, not at the player. The point is put 200 px to the player's left -
    // where a player who has just run right would have left it - and the shot has
    // to go left and down, away from a player standing straight below. The port
    // aimed along the sight segment until the visuals plan's step 5.2, and that
    // aim would send this one straight down.
    {
        const std::size_t before2 = level.turrets.fireballs.size();
        const glm::dvec2 lagged = playerPx + glm::dvec2(-200.0, 0.0);
        level.dragon.virtualPx = perch;
        level.dragon.atPx = perch;
        level.dragon.follower.Force(lagged);
        level.dragon.follower.switchMs = 0;
        level.dragon.elapsedMs = level.dragon.rules.fireIntervalMs + 1.0;
        Game::Tick(data, registry, level, 0.0f, kStep);
        CHECK(level.turrets.fireballs.size() > before2);
        if (level.turrets.fireballs.size() > before2) {
            const Turrets::Fireball& made = level.turrets.fireballs.back();
            const glm::dvec2 toLag = lagged - perch;
            const double length = std::sqrt(toLag.x * toLag.x + toLag.y * toLag.y);
            const glm::dvec2 want = toLag / length * level.turrets.rules.speedPx;
            CHECK_MSG(made.velocityPx.x < 0.0,
                      "the shot goes toward the lagged point, left of the player: x " +
                          std::to_string(made.velocityPx.x));
            CHECK(std::fabs(made.velocityPx.x - want.x) < 0.01);
            CHECK(std::fabs(made.velocityPx.y - want.y) < 0.01);
            std::printf("  and at a point 200 px behind the player it flies (%.1f, %.1f), not straight down\n",
                        made.velocityPx.x, made.velocityPx.y);
        }
    }

    // AND THE GATE REFUSES. Put it under the floor the player is standing on,
    // with its clock past the interval, and the shot must not happen - otherwise
    // the check above is a rubber stamp that would pass for a dragon that fires
    // through the world.
    {
        const std::size_t before2 = level.turrets.fireballs.size();
        const int fired2 = level.dragon.fired;
        level.dragon.virtualPx = playerPx + glm::dvec2(0.0, 200.0);
        level.dragon.elapsedMs = level.dragon.rules.fireIntervalMs + 1.0;
        Game::Tick(data, registry, level, 0.0f, kStep);
        CHECK_EQ(level.turrets.fireballs.size(), before2);
        CHECK_EQ(level.dragon.fired, fired2);
        std::printf("  and refused the shot with the floor in the way\n");
    }

    // Once it has taken off it stops, which is what ends its part of the level.
    level.dragon.virtualPx = playerPx + glm::dvec2(0.0, -56.0);
    level.dragon.tookOff = true;
    level.dragon.elapsedMs = level.dragon.rules.fireIntervalMs + 1.0;
    const std::size_t after = level.turrets.fireballs.size();
    Game::Tick(data, registry, level, 0.0f, kStep);
    CHECK_EQ(level.turrets.fireballs.size(), after);
}

void runTests() {
    TheRulesRead();
    TheRulesAreRefusedWhenTheyDoNotMakeADragon();
    TheFollowerLagsAsTheScriptsDoes();
    TheAngleFollowsTheLaggedPoint();
    TheMouthOpensAndClosesOnItsOwnClock();
    ItAnswersForItsOwnTwoNodesAndNoOthers();
    TheLevelPlacesItWhereTheFileSaysIt();
    TheCameraIsTheDragonHeldInsideTheLevel();
    TheClawTakesTheFloorAndTheRegistryAgrees();
    TheExitIsNeverEaten();
    TheRunItPlaysOnItsOwn();
    ItSpitsAtWhatItCanSee();
}

} // namespace

int main() {
    std::error_code ec;
    if (!std::filesystem::is_directory(kLevels, ec) ||
        !std::filesystem::is_regular_file(kData + "/entity_roles.json", ec)) {
        std::printf("test_mp_dragon: SKIPPED - needs the converted levels at %s\n"
                    "  and the remake's data at %s.\n"
                    "  Both live outside this repository; configure with\n"
                    "  -DSUPERSONIC_MAGICPORTALS_LEVELS=... and -DSUPERSONIC_MAGICPORTALS_DATA=...\n",
                    kLevels.c_str(), kData.c_str());
        return 77;
    }
    runTests();
    return ::test::summary("test_mp_dragon", 60);
}
