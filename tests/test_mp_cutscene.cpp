// The win cutscenes and the delay before the medal (the program's P2, planning
// step 123): cutscenes.json and sim/Cutscene.
//
// What it pins:
//  - the decode as data: every number cutscenes.json carries, and the rules
//    refused when a span or a speed would stop a cutscene or divide by nothing;
//  - 4-01's launch: the door's (0, -390) and the bridge's unclamped (-32, 0) as
//    smoothBeginning of their spans, the shake re-armed past 100 ms for 6 s, and
//    both frozen at the last pose written below 7600;
//  - 4-19's landing: its clock stopping at 12500, the door's (256, 400), the key's
//    0.9 a frame, the eclipse from its own height below the view's bottom up to it,
//    and the spiral full until about 6.9 s and at 0.1 at the end;
//  - the census over the converted levels: three levels place an end_delay, each
//    the level of one cutscene, and no other level places any of the three;
//  - 2-32's knight on its own level, through Game::Tick: it falls, holds its 800 ms
//    at waypoint0, walks right at 1.7 px a step and goes in at waypoint1 before the
//    level's 6600 ms are up.
//
// The census and the knight read the converted levels from outside this repository
// and are skipped, saying where they looked, when those are absent; the rest reads
// only the port's own committed data and always runs.

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "core/Components.hpp"
#include "sim/Cutscene.hpp"
#include "sim/Ease.hpp"
#include "sim/Game.hpp"
#include "sim/Tscn.hpp"
#include "sim/Units.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

using namespace MagicPortals;

namespace {

const std::string kLevels = MAGICPORTALS_LEVELS_DIR;
const std::string kData = MAGICPORTALS_DATA_DIR;
const std::string kPortData = MAGICPORTALS_PORT_DATA_DIR;
constexpr float kStep = 1.0f / 60.0f;

bool Near(double a, double b, double eps = 1e-6) { return std::fabs(a - b) <= eps; }

std::string Show(const glm::dvec2& p) { return "(" + std::to_string(p.x) + ", " + std::to_string(p.y) + ")"; }

bool LevelsThere() {
    std::error_code ec;
    return std::filesystem::is_directory(kLevels, ec) &&
           std::filesystem::is_regular_file(kData + "/entity_roles.json", ec);
}

Cutscene::Rules Load() {
    Cutscene::Rules rules;
    std::string error;
    const bool ok = Cutscene::LoadRules(kPortData + "/cutscenes.json", rules, error);
    CHECK_MSG(ok, error);
    return rules;
}

void TheRulesAreTheDecode() {
    const Cutscene::Rules rules = Load();
    CHECK(rules.endDelayEntity == "end_delay");
    CHECK(rules.endDelayKey == "time");

    const Cutscene::Rules::Launch& launch = rules.launch;
    CHECK(launch.entity == "rocket_launch" && launch.door == "door.ent" && launch.bridge == "rocket_bridge");
    CHECK(launch.launchMs == 7600.0);
    CHECK(launch.doorTravelPx == glm::dvec2(0.0, -390.0));
    CHECK(launch.bridgeTravelPx == glm::dvec2(-32.0, 0.0));
    CHECK(launch.bridgeSpanMs == 3800.0);
    CHECK(launch.quakeRearmAboveMs == 100.0 && launch.quakeBeforeMs == 6000.0);
    CHECK(launch.track == "rocket");

    const Cutscene::Rules::Landing& landing = rules.landing;
    CHECK(landing.entity == "rocket_landing" && landing.door == "door.ent" && landing.eclipse == "eclipse.ent" &&
          landing.spiral == "spiral" && landing.key == "key");
    CHECK(landing.launchMs == 12500.0);
    CHECK(landing.doorTravelPx == glm::dvec2(256.0, 400.0));
    CHECK(landing.eclipsePitch == glm::dvec2(0.5, 1.0));
    CHECK(landing.keyScalePerFrame == 0.9);
    CHECK(landing.spiralBrightnessFrom == 2.0 && landing.spiralBrightnessTo == 0.1);
    CHECK(landing.track == "back_home");

    const Cutscene::Rules::Knight& knight = rules.knight;
    CHECK(knight.spawn == "dragon_knight_spawn" && knight.entity == "dragon_knight.ent");
    CHECK(knight.waypointPrefix == "waypoint" && knight.holdKey == "hold");
    CHECK(knight.speedPxPerStep == 1.7);
    CHECK(knight.arrivePx == 16.0);
    CHECK(knight.finishOffsetPx == glm::dvec2(16.0, -16.0));
    CHECK(knight.walkFirstColumn == 0 && knight.walkLastColumn == 3 && knight.walkStrideMs == 120.0);
    CHECK(knight.startFrame == 4 && knight.columns == 4);
    CHECK(knight.leftRow == 1 && knight.rightRow == 2);
    CHECK(knight.pivotPx == glm::dvec2(0.0, 0.0));
    CHECK(knight.colour == glm::dvec3(0.0) && knight.emissive == glm::dvec3(0.0));
}

// A span of 0 would divide the clock by nothing, and a speed of 0 would hold the
// knight at waypoint0 for ever: both are refused rather than played.
void RulesThatStopACutsceneAreRefused() {
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "supersonic-test-mp-cutscene";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    std::ifstream in(kPortData + "/cutscenes.json", std::ios::binary);
    const std::string good((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    CHECK(!good.empty());
    const auto refused = [&](const std::string& from, const std::string& to) {
        const std::size_t at = good.find(from);
        CHECK_MSG(at != std::string::npos, from);
        if (at == std::string::npos) return;
        std::string bad = good;
        bad.replace(at, from.size(), to);
        const std::filesystem::path file = dir / "cutscenes.json";
        std::ofstream(file, std::ios::binary) << bad;
        Cutscene::Rules rules;
        std::string error;
        CHECK_MSG(!Cutscene::LoadRules(file.string(), rules, error), from + " -> " + to + " was taken");
    };
    refused("\"launch_ms\": 7600.0", "\"launch_ms\": 0.0");
    refused("\"bridge_span_ms\": 3800.0", "\"bridge_span_ms\": 0.0");
    refused("\"speed_px_per_step\": 1.7", "\"speed_px_per_step\": 0.0");
    refused("\"walk_last_column\": 3", "\"walk_last_column\": 2.5");
    refused("\"track\": \"rocket\"", "\"track\": \"\"");
    std::filesystem::remove_all(dir, ec);
}

// 4-01: interpolate(start, destiny, smoothBeginning(t / span)), the bridge's span
// half the door's and unclamped.
void TheLaunchRisesAndTheBridgeOvershoots() {
    const Cutscene::Rules rules = Load();
    const Cutscene::LaunchPose rest = Cutscene::LaunchAt(rules, 0.0);
    CHECK(rest.doorOffsetPx == glm::dvec2(0.0) && rest.bridgeOffsetPx == glm::dvec2(0.0));
    const Cutscene::LaunchPose half = Cutscene::LaunchAt(rules, 3800.0);
    CHECK_MSG(Near(half.doorOffsetPx.y, -390.0 * (1.0 - std::cos(0.25 * 3.14159265358979)), 1e-3),
              Show(half.doorOffsetPx));
    CHECK_MSG(Near(half.bridgeOffsetPx.x, -32.0, 1e-4), Show(half.bridgeOffsetPx));
    const Cutscene::LaunchPose end = Cutscene::LaunchAt(rules, 7600.0);
    CHECK_MSG(Near(end.doorOffsetPx.y, -390.0, 1e-3) && end.doorOffsetPx.x == 0.0, Show(end.doorOffsetPx));
    // No clamp: -64 at 7600, where F17 saw -48 at 5.3 s and the finish covered the rest.
    CHECK_MSG(Near(end.bridgeOffsetPx.x, -64.0, 1e-3) && end.bridgeOffsetPx.y == 0.0, Show(end.bridgeOffsetPx));

    // F17 r1's readings, both relative to its frame #244 (pts 11.402: 0.849 s after
    // the rise's fitted win at 10.553, 0.880 s after the bridge's at 10.522), within
    // twice each fit's rms (4.25 u and 0.29 u).
    const auto rise = [&](double s) {
        return -(Cutscene::LaunchAt(rules, s * 1000.0).doorOffsetPx.y - Cutscene::LaunchAt(rules, 849.0).doorOffsetPx.y);
    };
    const double rises[][2] = {{3.422, 88.5}, {4.094, 126.2}, {4.936, 181.0}, {5.456, 215.5}};
    for (const auto& m : rises) {
        CHECK_MSG(std::fabs(rise(m[0]) - m[1]) < 4.25 * 2.0,
                  "rise at " + std::to_string(m[0]) + " s: " + std::to_string(rise(m[0])) + " against " +
                      std::to_string(m[1]));
    }
    const auto slide = [&](double s) {
        return Cutscene::LaunchAt(rules, s * 1000.0).bridgeOffsetPx.x - Cutscene::LaunchAt(rules, 880.0).bridgeOffsetPx.x;
    };
    const double slides[][2] = {{3.981, -32.4}, {4.611, -40.5}, {5.293, -48.0}};
    for (const auto& m : slides) {
        CHECK_MSG(std::fabs(slide(m[0]) - m[1]) < 0.29 * 2.0,
                  "bridge at " + std::to_string(m[0]) + " s: " + std::to_string(slide(m[0])) + " against " +
                      std::to_string(m[1]));
    }

    // Stepped at 20 ms frames: started once, the shake every sixth frame (120 >
    // 100) while below 6000 ms - 49 of them - and the pose frozen at 7580's.
    Cutscene::Launch launch;
    int quakes = 0;
    int starts = 0;
    int lastQuakeFrame = 0;
    for (int frame = 1; frame <= 500; ++frame) {
        const Cutscene::LaunchStep step = Cutscene::StepLaunch(rules, launch, 20.0);
        if (step.started) ++starts;
        if (step.quake) {
            ++quakes;
            CHECK_MSG(frame % 6 == 0, "a shake on frame " + std::to_string(frame));
            lastQuakeFrame = frame;
        }
        if (frame == 1) CHECK(launch.elapsedMs == 20.0 && launch.pose.doorOffsetPx.y < 0.0);
    }
    CHECK_EQ(starts, 1);
    CHECK_EQ(quakes, 49);
    CHECK_EQ(lastQuakeFrame, 294);
    const Cutscene::LaunchPose frozen = Cutscene::LaunchAt(rules, 7580.0);
    CHECK(launch.pose.doorOffsetPx == frozen.doorOffsetPx && launch.pose.bridgeOffsetPx == frozen.bridgeOffsetPx);
    CHECK(launch.elapsedMs == 10000.0);
}

// 4-19: the clock stops at 12500, and everything is read off the one bias.
void TheLandingSinksTheRocketAndRaisesTheEclipse() {
    const Cutscene::Rules rules = Load();
    Cutscene::Landing landing;
    double dimFromMs = -1.0;
    int starts = 0;
    for (int frame = 1; frame <= 700; ++frame) {
        if (Cutscene::StepLanding(rules, landing, 20.0).started) ++starts;
        if (dimFromMs < 0.0 && Cutscene::SpiralBrightness(rules, landing.bias) < 1.0) dimFromMs = landing.elapsedMs;
        if (frame == 1) {
            CHECK(landing.running && landing.elapsedMs == 20.0);
            CHECK(Near(landing.keyScale, 0.9));
            CHECK_EQ(landing.keyScalings, 1);
        }
        if (frame == 10) CHECK_MSG(Near(landing.keyScale, std::pow(0.9, 10.0), 1e-12), std::to_string(landing.keyScale));
    }
    CHECK_EQ(starts, 1);
    // One Scale(0.9) a frame, the clock's stop notwithstanding: what the layer
    // scales the key's systems by (planning step 141).
    CHECK_EQ(landing.keyScalings, 700);
    CHECK_MSG(landing.elapsedMs == 12500.0, std::to_string(landing.elapsedMs));
    CHECK(Near(landing.bias, 1.0, 1e-6));
    CHECK(Near(Cutscene::LandingDoorOffsetPx(rules, landing.bias).x, 256.0, 1e-3));
    CHECK(Near(Cutscene::LandingDoorOffsetPx(rules, landing.bias).y, 400.0, 1e-3));
    // Full until smoothEnd(bias) passes 1 / 1.9: 6901 ms, the decode's "about 6.9 s"
    // and F17's first drop at 7.1 s, one or two of its frame gaps later.
    CHECK_MSG(dimFromMs > 6880.0 && dimFromMs < 6960.0, std::to_string(dimFromMs));
    CHECK(Near(Cutscene::SpiralBrightness(rules, 0.0), 1.0));
    CHECK(Near(Cutscene::SpiralBrightness(rules, 1.0), 0.1, 1e-6));
    // The eclipse, 64 units tall (32 at scale 2), under a view 256 tall at y 0: from
    // 320 - where level18c places it - up to the view's bottom edge.
    CHECK(Near(Cutscene::LandingEclipseYPx(rules, 0.0, 0.0, 256.0, 64.0), 320.0));
    CHECK(Near(Cutscene::LandingEclipseYPx(rules, 1.0, 0.0, 256.0, 64.0), 256.0, 1e-4));
    CHECK(Near(Cutscene::LandingEclipseYPx(rules, 1.0, 10.0, 256.0, 64.0), 266.0, 1e-4));
    // Bias 0.5: 64 x (1 - smoothEnd(0.5)).
    CHECK(Near(Cutscene::LandingEclipseYPx(rules, 0.5, 0.0, 256.0, 64.0),
               256.0 + 64.0 * (1.0 - Ease::SmoothEnd(0.5)), 1e-9));
}

// Three levels place an end_delay, each the level of one cutscene, and no other
// level places any of the three.
void ThreeLevelsSetADelayAndEachPlaysItsCutscene() {
    if (!LevelsThere()) {
        std::printf("  the census skipped: no converted levels at %s\n", kLevels.c_str());
        return;
    }
    const Cutscene::Rules rules = Load();
    std::map<std::string, double> delays;
    std::map<std::string, std::vector<std::string>> places;
    int levels = 0;
    for (const auto& entry : std::filesystem::directory_iterator(kLevels)) {
        if (entry.path().extension() != ".tscn") continue;
        Tscn::Scene scene;
        std::string error;
        if (!Tscn::Load(entry.path().string(), scene, error)) {
            CHECK_MSG(false, error);
            continue;
        }
        ++levels;
        const std::string name = entry.path().stem().string();
        if (const std::optional<double> ms = Cutscene::EndDelayMs(rules, scene)) delays[name] = *ms;
        for (const std::string* entity : {&rules.launch.entity, &rules.landing.entity, &rules.knight.spawn}) {
            if (Cutscene::Places(scene, *entity)) places[*entity].push_back(name);
        }
    }
    CHECK_EQ(levels, 128);
    CHECK_EQ(delays.size(), std::size_t{3});
    CHECK(delays["level0c"] == 5000.0);
    CHECK(delays["level18c"] == 14500.0);
    CHECK(delays["level31a"] == 6600.0);
    CHECK(places[rules.launch.entity] == std::vector<std::string>{"level0c"});
    CHECK(places[rules.landing.entity] == std::vector<std::string>{"level18c"});
    CHECK(places[rules.knight.spawn] == std::vector<std::string>{"level31a"});
    std::printf("  %d levels: end_delay on %zu (level0c %.0f, level18c %.0f, level31a %.0f)\n", levels, delays.size(),
                delays["level0c"], delays["level18c"], delays["level31a"]);
}

// 2-32's knight, through the port's own tick: added at its spawn, it falls
// undriven, arrives at waypoint0 within 16, holds 800 ms with nothing set, walks
// right on row 2 at 1.7 px a step and deletes itself within 16 of waypoint1 - well
// inside the level's 6600 ms.
void TheKnightWalksInBeforeTheMedal() {
    if (!LevelsThere()) {
        std::printf("  the knight skipped: no converted levels at %s\n", kLevels.c_str());
        return;
    }
    const Cutscene::Rules rules = Load();
    Game::Data data;
    entt::registry registry;
    Game::Level level;
    const std::filesystem::path prisms = std::filesystem::temp_directory_path() / "supersonic-test-mp-cutscene";
    std::string error;
    if (!Game::LoadData(kLevels + "/level31a.tscn", kData, prisms, data, error) ||
        !Game::Start(data, registry, level, error)) {
        CHECK_MSG(false, error);
        return;
    }
    Cutscene::Knight knight;
    CHECK_MSG(Cutscene::FindKnight(rules, data.scene, knight, error), error);
    CHECK(knight.placed);
    CHECK(knight.spawnPx == glm::dvec2(2358.0, -24.0));
    CHECK_EQ(knight.waypoints.size(), std::size_t{2});
    if (knight.waypoints.size() != 2) return;
    CHECK(knight.waypoints[0].atPx == glm::dvec2(2360.0, 175.0) && knight.waypoints[0].holdMs == 800.0);
    CHECK(knight.waypoints[1].atPx == glm::dvec2(2548.0, 175.0) && knight.waypoints[1].holdMs == 0.0);
    CHECK(knight.column == 0 && knight.row == 1); // startFrame 4 on a 4-wide sheet

    CHECK(Cutscene::SpawnKnight(rules, data.minions, registry, knight) != entt::null);
    // Its ENTITY point on the spawn.
    CHECK_MSG(glm::length(Cutscene::KnightEntityPx(data.minions, registry, knight) - knight.spawnPx) < 1e-3,
              Show(Cutscene::KnightEntityPx(data.minions, registry, knight)));
    // Once only.
    const entt::entity body = knight.body;
    CHECK(Cutscene::SpawnKnight(rules, data.minions, registry, knight) == body);

    const double frameMs = static_cast<double>(kStep) * 1000.0;
    int arrivedTick = -1;
    int firstWalkTick = -1;
    int enteredTick = -1;
    int walks = 0;
    bool leftRow = false;
    double walkFromX = 0.0;
    for (int tick = 1; tick <= 600 && enteredTick < 0; ++tick) {
        Game::Tick(data, registry, level, 0.0f, kStep);
        Cutscene::StepKnight(rules, data.minions, registry, knight, frameMs);
        if (arrivedTick < 0 && knight.hasStarted) arrivedTick = tick;
        if (knight.walked) {
            ++walks;
            if (firstWalkTick < 0) {
                firstWalkTick = tick;
                walkFromX = Cutscene::KnightEntityPx(data.minions, registry, knight).x;
            }
            if (knight.row == rules.knight.leftRow) leftRow = true;
        }
        if (knight.entered) {
            enteredTick = tick;
            break;
        }
    }
    // Its mean pace from the first walking update to the one it went in on: the
    // velocity set on each is 1.7 px a 33.3333 ms step, 51 px/s.
    const double speedPxS = enteredTick > firstWalkTick && firstWalkTick > 0
                                ? (knight.enteredAtPx.x - walkFromX) /
                                      (static_cast<double>(enteredTick - firstWalkTick) * static_cast<double>(kStep))
                                : 0.0;
    std::printf("  knight: at waypoint0 on tick %d, walks from %d, in on %d at %s (%d walking ticks, %.2f px/s)\n",
                arrivedTick, firstWalkTick, enteredTick, Show(knight.enteredAtPx).c_str(), walks, speedPxS);
    CHECK(arrivedTick > 0);
    // The hold: 800 ms from the arrival, nothing driven before it passes.
    CHECK_MSG(firstWalkTick - arrivedTick >= 48 && firstWalkTick - arrivedTick <= 50,
              std::to_string(firstWalkTick - arrivedTick) + " ticks between arriving and walking");
    CHECK(!leftRow);
    CHECK_MSG(std::fabs(speedPxS - 51.0) < 1.0, std::to_string(speedPxS));
    CHECK(enteredTick > 0);
    CHECK_MSG(enteredTick * frameMs < 6600.0, std::to_string(enteredTick * frameMs) + " ms after its spawn");
    CHECK_MSG(glm::length(knight.enteredAtPx - knight.waypoints[1].atPx) < rules.knight.arrivePx,
              Show(knight.enteredAtPx));
    CHECK(knight.gone && knight.body == entt::null && !registry.valid(body));
    // And after it has gone nothing moves it again.
    Cutscene::StepKnight(rules, data.minions, registry, knight, frameMs);
    CHECK(!knight.entered && !knight.walked);
}

void runTests() {
    TheRulesAreTheDecode();
    RulesThatStopACutsceneAreRefused();
    TheLaunchRisesAndTheBridgeOvershoots();
    TheLandingSinksTheRocketAndRaisesTheEclipse();
    ThreeLevelsSetADelayAndEachPlaysItsCutscene();
    TheKnightWalksInBeforeTheMedal();
}

} // namespace

MP_TEST_MAIN("test_mp_cutscene", 60)
