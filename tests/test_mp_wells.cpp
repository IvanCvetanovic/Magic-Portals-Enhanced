// Gravity wells: entity `gravity_agent` / `gravity_agent.ent`, role
// `gravity_well`. Ten placements across five levels, and the last mechanic in
// the game that is not a boss.
//
// WHAT THIS SUITE IS REALLY FOR. A well is three things, and only two of them
// are anywhere in a level file. The node gives a position, a radius, a
// StaticBody2D and a CircleShape2D - so the solid circle builds itself and the
// attraction can be read off the radius. The THIRD is invisible: the agent's own
// callback adds an antiportal.ent at its position on its first tick, and no
// .tscn in the game mentions it - the green ring. It LOOKS like an antiportal
// and refuses nothing: renamed gravity_area as it is added, it is never one of
// AntiPortalManager's fields (00_order R2). Until the remaster doc's step 170 the
// port played a no-portal zone round every well and let the agent's body stop a
// shot; that both are gone is asserted here, through Portals::State::TryPlace
// and a shot flown by the level's own tick.
//
// Reads the converted levels from outside this repository, and skips, saying
// where it looked, when they are absent.

#include "TestHarness.hpp"

#include "core/Components.hpp"
#include "sim/Game.hpp"
#include "sim/GravityWell.hpp"
#include "sim/Portals.hpp"
#include "sim/Roles.hpp"
#include "sim/Shot.hpp"
#include "sim/Units.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>

using namespace MagicPortals;
using Supersonic::RigidBodyComponent;
using Supersonic::TransformComponent;

namespace {

const std::string kLevels = MAGICPORTALS_LEVELS_DIR;
const std::string kData = MAGICPORTALS_DATA_DIR;
const std::string kPortData = MAGICPORTALS_PORT_DATA_DIR;
constexpr float kStep = 1.0f / 60.0f;

// A zero-gravity level with one well, and a level with the world's gravity and
// one - the two sides of the forceLength branch.
constexpr const char* kWeightless = "level16c";
constexpr const char* kHeavy = "level20c";

double Length(const glm::vec3& v) {
    return std::sqrt(static_cast<double>(v.x * v.x + v.y * v.y));
}

bool Open(const std::string& name, Game::Data& data, entt::registry& registry, Game::Level& level) {
    const std::filesystem::path prisms = std::filesystem::temp_directory_path() / "supersonic-test-mp-wells";
    std::string error;
    if (!Game::LoadData(kLevels + "/" + name + ".tscn", kData, prisms, data, error) ||
        !Game::Start(data, registry, level, error)) {
        CHECK_MSG(false, name + ": " + error);
        return false;
    }
    return true;
}

void TheRulesRead() {
    GravityWell::Rules rules;
    std::string error;
    const bool ok = GravityWell::LoadRules(kPortData + "/gravitywell.json", rules, error);
    CHECK_MSG(ok, error);
    if (!ok) return;
    CHECK(::test::nearly(static_cast<float>(rules.forceZeroGravity), 0.18f));
    CHECK(::test::nearly(static_cast<float>(rules.forceWithGravity), 0.5f));
    CHECK(::test::nearly(static_cast<float>(rules.referenceFrameMs), 16.6666f));
    CHECK(::test::nearly(static_cast<float>(rules.zoneShrinkPx), 24.0f));
    std::printf("  pulls at %.2f m/s in zero gravity and %.2f with, per %.4f ms frame\n", rules.forceZeroGravity,
                rules.forceWithGravity, rules.referenceFrameMs);

    // And the ring the agent adds: antiportal.ent's own file, and
    // the green its callback paints it at instr 117-131.
    const GravityWell::Ring& ring = rules.ring;
    CHECK_MSG(ring.sprite == "white_ring.png" && ring.additive, "white_ring.png, added (blendMode 1)");
    CHECK_MSG(ring.colour == glm::dvec3(0.25, 0.5, 0.25), "SetColor(vector3(0.5, 1, 0.5) * 0.5): green");
    CHECK_MSG(ring.emissive == glm::dvec3(1.0), "antiportal.ent's EmissiveColor (1, 1, 1)");
    CHECK_MSG(!ring.applyLight && ring.isStatic, "applyLight 0, static 1");
    CHECK_MSG(ring.z == -5.0, "AddEntity's vector3(pos, -5)");
}

// THE RING IS THE AREA SEEN: scaleToSize gives the added antiportal a SIZE of
// radius * 2 - 24, and that is what is drawn. It is not a zone: no well hands the
// portals one (00_order R2, step 170), at any of the ten placements.
void TheRingIsTheAreaSeen() {
    GravityWell::Rules rules;
    std::string error;
    if (!GravityWell::LoadRules(kPortData + "/gravitywell.json", rules, error)) return;

    const struct Level {
        const char* name;
        double sizes[4];
        std::size_t count;
    } levels[] = {{"level16c", {188.0, 0.0, 0.0, 0.0}, 1},
                  {"level17c", {188.0, 256.0, 188.0, 188.0}, 4},
                  {"level18c", {256.0, 104.0, 104.0, 0.0}, 3},
                  {"level20c", {496.0, 0.0, 0.0, 0.0}, 1},
                  {"level27c", {232.0, 0.0, 0.0, 0.0}, 1}};

    std::size_t rings = 0;
    for (const Level& one : levels) {
        Game::Data data;
        entt::registry registry;
        Game::Level level;
        if (!Open(one.name, data, registry, level)) continue;
        if (level.wells.wells.size() != one.count) continue;
        for (std::size_t i = 0; i < level.wells.wells.size(); ++i) {
            const GravityWell::Well& well = level.wells.wells[i];
            const double size = well.RingSizePx(rules);
            CHECK_MSG(::test::nearly(static_cast<float>(size), static_cast<float>(one.sizes[i])),
                      well.name + " is drawn " + std::to_string(one.sizes[i]) + " units across, not " +
                          std::to_string(size));
            // And it refuses nothing, at every placement.
            CHECK_MSG(level.portals.FindZone(well.name) == nullptr, well.name + " gives the portals no zone");
            ++rings;
        }
        std::printf("  %s: %zu ring(s), the first %.0f units across\n", one.name, level.wells.wells.size(),
                    level.wells.wells.front().RingSizePx(rules));
    }
    CHECK_EQ(rings, std::size_t{10});
}

// A ring is antiportal.ent's own row, and none of it may be guessed: a file that
// is missing any of it is refused rather than drawn at a default on five levels.
void ARingThatIsNotTheEntitysIsRefused() {
    std::ifstream file(kPortData + "/gravitywell.json", std::ios::binary);
    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string real = buffer.str();
    CHECK(real.find("\"ring\": {") != std::string::npos);

    const std::filesystem::path scratch = std::filesystem::temp_directory_path() / "supersonic-test-mp-wells";
    std::error_code ec;
    std::filesystem::create_directories(scratch, ec);
    const auto refused = [&real, &scratch](const std::string& from, const std::string& to, const std::string& name) {
        std::string text = real;
        const std::size_t at = text.find(from);
        CHECK_MSG(at != std::string::npos, from);
        if (at == std::string::npos) return;
        text.replace(at, from.size(), to);
        const std::filesystem::path path = scratch / ("gravitywell-ring-" + name + ".json");
        std::ofstream out(path, std::ios::binary);
        out << text;
        out.close();
        GravityWell::Rules rules;
        std::string error;
        const bool ok = GravityWell::LoadRules(path.string(), rules, error);
        CHECK_MSG(!ok, "refused: " + name);
        CHECK_MSG(error.find("ring") != std::string::npos, name + ": " + error);
    };
    refused("\"ring\": {", "\"ring_gone\": {", "absent");
    refused("\"sprite\": \"white_ring.png\"", "\"sprite\": \"\"", "no picture");
    refused("\"additive\": true", "\"additive\": 1", "a blend that is not a bool");
    refused("\"colour\": [0.25, 0.5, 0.25]", "\"colour\": [0.25, 0.5]", "two channels");
    refused("\"colour\": [0.25, 0.5, 0.25]", "\"colour\": [0.25, -0.5, 0.25]", "below zero");
    refused("\"emissive\": [1.0, 1.0, 1.0]", "\"emissive\": \"1 1 1\"", "an emissive that is not three numbers");
    refused("\"apply_light\": false", "\"apply_light\": \"no\"", "a lighting flag that is not a bool");
    refused("\"z\": -5.0", "\"z\": \"-5\"", "a depth that is not a number");
}

// The census, counted by reading every node's radius.
void TenWellsAcrossFiveLevels() {
    const struct Level {
        const char* name;
        std::size_t count;
    } levels[] = {{"level16c", 1}, {"level17c", 4}, {"level18c", 3}, {"level20c", 1}, {"level27c", 1}};

    std::size_t total = 0;
    for (const Level& one : levels) {
        Game::Data data;
        entt::registry registry;
        Game::Level level;
        if (!Open(one.name, data, registry, level)) continue;
        CHECK_MSG(level.wells.wells.size() == one.count,
                  std::string(one.name) + " places " + std::to_string(one.count) + " gravity well(s)");
        total += level.wells.wells.size();
        for (const GravityWell::Well& well : level.wells.wells) {
            CHECK_MSG(well.radiusPx > 0.0, well.name + " has a radius");
        }
    }
    CHECK_EQ(total, std::size_t{10});
    std::printf("  %zu gravity well(s) across 5 levels\n", total);

    // A level with none has none.
    {
        Game::Data data;
        entt::registry registry;
        Game::Level level;
        if (Open("level0", data, registry, level)) CHECK(level.wells.wells.empty());
    }
}

// radius * 2 - 24 across, not radius * 2. Twenty-four units at every placement.
void TheRingIsTwentyFourUnitsNarrowerThanTheWell() {
    GravityWell::Rules rules;
    std::string error;
    if (!GravityWell::LoadRules(kPortData + "/gravitywell.json", rules, error)) return;

    GravityWell::Well well;
    well.radiusPx = 106.0;
    CHECK(::test::nearly(static_cast<float>(well.RingSizePx(rules)), 188.0f));
    well.radiusPx = 260.0;
    CHECK(::test::nearly(static_cast<float>(well.RingSizePx(rules)), 496.0f));
    well.radiusPx = 64.0;
    CHECK(::test::nearly(static_cast<float>(well.RingSizePx(rules)), 104.0f));
}

// The arithmetic on its own, away from any level.
void ItPullsInwardAndFadesToNothingAtTheRim() {
    GravityWell::Rules rules;
    std::string error;
    if (!GravityWell::LoadRules(kPortData + "/gravitywell.json", rules, error)) return;

    GravityWell::Well well;
    well.name = "well";
    well.atPx = glm::dvec2(100.0, 100.0);
    well.radiusPx = 100.0;

    // A body to the RIGHT of the centre is pulled LEFT.
    const glm::vec3 fromRight = GravityWell::Pull(well, rules, glm::dvec2(150.0, 100.0), true, kStep);
    CHECK(fromRight.x < 0.0f);
    CHECK(::test::nearly(fromRight.y, 0.0f));

    // A body BELOW it - a larger y in the remake's pixels, which count down - is
    // pulled UP, which is +y in the engine's metres. The sign trap again.
    const glm::vec3 fromBelow = GravityWell::Pull(well, rules, glm::dvec2(100.0, 150.0), true, kStep);
    CHECK(fromBelow.y > 0.0f);
    CHECK(::test::nearly(fromBelow.x, 0.0f));

    // Nearer the centre pulls HARDER: smoothEnd(1 - d^2/r^2) is an ease-out.
    const double near = Length(GravityWell::Pull(well, rules, glm::dvec2(110.0, 100.0), true, kStep));
    const double mid = Length(GravityWell::Pull(well, rules, glm::dvec2(150.0, 100.0), true, kStep));
    const double rim = Length(GravityWell::Pull(well, rules, glm::dvec2(195.0, 100.0), true, kStep));
    std::printf("  a 100 px well pulls %.5f at 10 px, %.5f at 50, %.5f at 95\n", near, mid, rim);
    CHECK(near > mid);
    CHECK(mid > rim);
    CHECK(rim > 0.0);

    // OUTSIDE is nothing at all, and the rim itself is outside - the original's
    // test is a strict less-than.
    CHECK_EQ(Length(GravityWell::Pull(well, rules, glm::dvec2(200.0, 100.0), true, kStep)), 0.0);
    CHECK_EQ(Length(GravityWell::Pull(well, rules, glm::dvec2(400.0, 100.0), true, kStep)), 0.0);
    // And dead centre is nothing rather than a NaN.
    const glm::vec3 centre = GravityWell::Pull(well, rules, well.atPx, true, kStep);
    CHECK(::test::nearly(centre.x, 0.0f));
    CHECK(::test::nearly(centre.y, 0.0f));
}

// 0.18 against 0.5, and the branch is the LEVEL's flag.
void ItPullsHarderWhereTheWorldHasGravity() {
    GravityWell::Rules rules;
    std::string error;
    if (!GravityWell::LoadRules(kPortData + "/gravitywell.json", rules, error)) return;

    GravityWell::Well well;
    well.atPx = glm::dvec2(0.0, 0.0);
    well.radiusPx = 100.0;
    const glm::dvec2 at(50.0, 0.0);

    const double weightless = Length(GravityWell::Pull(well, rules, at, true, kStep));
    const double heavy = Length(GravityWell::Pull(well, rules, at, false, kStep));
    CHECK(heavy > weightless);
    // Exactly the ratio of the two strengths, since everything else is equal.
    CHECK(::test::nearly(static_cast<float>(heavy / weightless),
                         static_cast<float>(rules.forceWithGravity / rules.forceZeroGravity)));

    // And the levels carry the flag the branch reads.
    {
        Game::Data data;
        entt::registry registry;
        Game::Level level;
        if (Open(kWeightless, data, registry, level)) CHECK(level.wells.noGravity);
    }
    {
        Game::Data data;
        entt::registry registry;
        Game::Level level;
        if (Open(kHeavy, data, registry, level)) CHECK(!level.wells.noGravity);
    }
}

// A WELL REFUSES NO PORTAL (00_order R2, decided for the original; the remaster
// doc's step 170). The area its agent adds is antiportal.ent, but it is renamed
// gravity_area as it is added (ETHScene.cpp:264-266), after AntiPortalManager's
// constructor has gathered its fields by name (104925..105343 #16-35; 00_order
// section 8.4), so the original never tests a tap against it. Until step 170 the
// port refused a tap within radius - 12 of every well, and this suite asserted
// both taps below refused.
void AWellRefusesNoPortal() {
    Game::Data data;
    entt::registry registry;
    Game::Level level;
    if (!Open(kWeightless, data, registry, level)) return;
    if (level.wells.wells.empty()) {
        CHECK_MSG(false, "level16c places a gravity well");
        return;
    }

    const GravityWell::Well& well = level.wells.wells.front();
    const double ringHalfPx = well.RingSizePx(level.wells.rules) * 0.5;

    // No zone was handed to the portals for it, and level16c places none of its
    // own, so the level plays no zone at all.
    CHECK_MSG(level.portals.FindZone(well.name) == nullptr, well.name + " gives the portals no no-portal zone");
    CHECK(level.portals.zones.empty());
    CHECK(!level.portals.InField(well.atPx));

    // Dead centre opens a portal, and so does a tap just inside the ring's rim:
    // the two taps the port refused before step 170.
    CHECK(level.portals.TryPlace(well.atPx));
    CHECK(level.portals.TryPlace(well.atPx + glm::dvec2(ringHalfPx - 2.0, 0.0)));
    CHECK_EQ(static_cast<int>(level.portals.placed.size()), 2);
    std::printf("  %s: %s refuses no portal, at its centre (%.0f, %.0f) or %.0f px out\n", kWeightless,
                well.name.c_str(), well.atPx.x, well.atPx.y, ringHalfPx - 2.0);
}

// AND A SHOT FLIES THROUGH ITS AGENT (R2). The original's projectile.ent has no
// shape and meets nothing on its way (00_order section 8.4, D3 with its check),
// and the tap-time ray passes gravity_agent by name (computePortalFinalPos #30-51,
// placement.json final_pos.ray_ignores). So the agent - solid to the player and to
// every loose body - is handed to Portals::State::shotPasses, and a shot aimed
// through it opens its portal beyond. Until step 170 the port's shot died on it.
void AShotFliesThroughAWell() {
    Game::Data data;
    entt::registry registry;
    Game::Level level;
    if (!Open(kWeightless, data, registry, level)) return;
    if (level.wells.wells.empty()) return;

    const GravityWell::Well& well = level.wells.wells.front();
    const auto found = level.built.entities.find(well.name);
    CHECK_MSG(found != level.built.entities.end(), well.name + " was built");
    if (found == level.built.entities.end()) return;
    const entt::entity agent = found->second;
    CHECK_MSG(std::find(level.portals.shotPasses.begin(), level.portals.shotPasses.end(), agent) !=
                  level.portals.shotPasses.end(),
              well.name + " is a body a shot passes");
    CHECK_EQ(static_cast<int>(level.portals.shotPasses.size()), 1);

    // Past the 400 ms a level holds its first tap off for. Nothing moves the
    // player meanwhile: level16c has no gravity, and the spawn is outside the
    // well's reach.
    for (int tick = 0; tick < 30; ++tick) Game::Tick(data, registry, level, 0.0f, kStep);
    const glm::dvec2 fromPx = Units::ToPixels(registry.get<TransformComponent>(level.player).position);
    // Through the agent's centre, and as far again beyond it as half the way to it.
    const glm::dvec2 tapPx = fromPx + (well.atPx - fromPx) * 1.5;

    // The agent IS on the way: the first solid body the shot's own test meets,
    // passing the player alone - which is what stopped the port's shot before.
    const auto blocked = Shot::FirstBody(registry, fromPx, tapPx, level.player);
    CHECK_MSG(blocked.has_value() && blocked->body == agent, "the agent is the first solid body on the shot's way");
    std::vector<entt::entity> passes = level.portals.shotPasses;
    passes.push_back(level.player);
    CHECK_MSG(!Shot::FirstBodyPassing(registry, fromPx, tapPx, passes).has_value(), "and nothing else is");

    // The tap's picture already said so: the ray passes the agent, so portal_mark.
    const Portals::FinalPos predicted = level.portals.Predict(registry, fromPx, tapPx);
    CHECK(!predicted.willDie && !predicted.redWaves);

    // And the shot, fired as a tap fires it and flown by the level's own tick.
    CHECK(level.portals.Shoot(registry, tapPx));
    for (int tick = 0; tick < 60 && level.portals.flight; ++tick) Game::Tick(data, registry, level, 0.0f, kStep);
    CHECK(!level.portals.flight.has_value());
    CHECK_EQ(level.portals.shotsFailed, 0);
    CHECK_EQ(static_cast<int>(level.portals.placed.size()), 1);
    if (!level.portals.placed.empty()) {
        CHECK(glm::distance(level.portals.placed.front().basePx, tapPx) < 1e-9);
    }
    std::printf("  %s: a shot from (%.1f, %.1f) through %s opens its portal at (%.1f, %.1f)\n", kWeightless, fromPx.x,
                fromPx.y, well.name.c_str(), tapPx.x, tapPx.y);
}

// A dynamic body is pulled; a kinematic one is not.
void ItTakesTheLooseBodiesOnly() {
    Game::Data data;
    entt::registry registry;
    Game::Level level;
    if (!Open(kWeightless, data, registry, level)) return;
    if (level.wells.wells.empty()) return;

    // The player is dynamic, so a well reaches it. Put it inside one and let it
    // go: in a zero-gravity level nothing else can move it.
    const GravityWell::Well& well = level.wells.wells.front();
    auto& transform = registry.get<TransformComponent>(level.player);
    const glm::dvec2 startPx = well.atPx + glm::dvec2(well.radiusPx * 0.5, 0.0);
    const glm::vec3 start = Units::ToWorld(startPx.x, startPx.y);
    transform.position = glm::vec3(start.x, start.y, transform.position.z);
    registry.get<RigidBodyComponent>(level.player).velocity = glm::vec3(0.0f);

    const double before = glm::distance(startPx, well.atPx);
    for (int tick = 0; tick < 30; ++tick) Game::Tick(data, registry, level, 0.0f, kStep);
    const glm::dvec2 nowPx = Units::ToPixels(registry.get<TransformComponent>(level.player).position);
    const double after = glm::distance(nowPx, well.atPx);
    std::printf("  %s: the player falls from %.1f px to %.1f px of the well in half a second\n", kWeightless, before,
                after);
    CHECK(after < before);
}

// THE LAPS (the visuals plan's 11.2; gravitywell.json roundabout): the numbers
// the listing states, and a file that leaves them out refused.
void TheRoundaboutRulesRead() {
    GravityWell::Rules rules;
    std::string error;
    CHECK_MSG(GravityWell::LoadRules(kPortData + "/gravitywell.json", rules, error), error);
    const GravityWell::Roundabout& lap = rules.roundabout;
    CHECK_MSG(lap.stepDotBelow == 0.98 && lap.lapDegrees == 360.0, "computeRoundabout #185 0.98f and #285 360f");
    CHECK_MSG(lap.spinMs == 3000.0 && lap.spinDegrees == 360.0, "ETHCallback_gravity_agent #191-215: 3000 and 360");
    CHECK_MSG(lap.elasticPeriod == 0.3 && lap.elasticShiftDivisor == 4.0 && lap.elasticDecay == -10.0,
              "elastic's p 0.3, s = p / 4, pow(2, -10 n)");

    std::ifstream file(kPortData + "/gravitywell.json", std::ios::binary);
    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string real = buffer.str();
    const std::filesystem::path scratch = std::filesystem::temp_directory_path() / "supersonic-test-mp-wells";
    std::filesystem::create_directories(scratch);
    const auto refused = [&real, &scratch](const std::string& from, const std::string& to, const std::string& name) {
        std::string text = real;
        const std::size_t at = text.find(from);
        CHECK_MSG(at != std::string::npos, from);
        if (at == std::string::npos) return;
        text.replace(at, from.size(), to);
        const std::filesystem::path path = scratch / ("gravitywell-roundabout-" + name + ".json");
        std::ofstream(path, std::ios::binary) << text;
        GravityWell::Rules rules;
        std::string error;
        CHECK_MSG(!GravityWell::LoadRules(path.string(), rules, error) && error.find("roundabout") != std::string::npos,
                  name + " refused: " + error);
    };
    refused("\"roundabout\": {", "\"roundabout_gone\": {", "missing");
    refused("\"step_dot_below\": 0.98", "\"step_dot_below\": 1.0", "every-step");
    refused("\"lap_degrees\": 360.0", "\"lap_degrees\": 0.0", "no-lap");
    refused("\"elastic_decay\": -10.0", "\"elastic_decay\": 10.0", "no-settle");
    refused("\"spin_ms\": 3000.0,", "", "no-spin-time");
}

// elastic (Interpolator.angelscript bytes 314142..314380) and the agent's turn.
void TheAgentTurnsOnceAndSettles() {
    GravityWell::Rules rules;
    std::string error;
    if (!GravityWell::LoadRules(kPortData + "/gravitywell.json", rules, error)) return;
    const GravityWell::Roundabout& lap = rules.roundabout;
    CHECK_MSG(GravityWell::Elastic(lap, 0.0f) == 0.0f && GravityWell::Elastic(lap, 1.0f) == 1.0f,
              "n itself at 0 and at 1 (#1-13)");
    // 2^-5 x sin((0.5 - 0.075) x 2 pi / 0.3) + 1 = 1/32 x sin(8.9012) + 1, and
    // 8.9012 - 2 pi is 5 pi / 6: 1 + 1/64.
    const float half = GravityWell::Elastic(lap, 0.5f);
    CHECK_MSG(std::fabs(half - 1.015625f) < 1e-5f, "elastic(0.5) = 1.015625: " + std::to_string(half));
    // At rest - the clock at 3000 and past it - a whole turn, EXACTLY, which draws as
    // no turn; at a lap's moment, none.
    CHECK_EQ(GravityWell::SpinDegrees(lap, 3000.0, 1.0), 360.0);
    CHECK_EQ(GravityWell::SpinDegrees(lap, 90000.0, 1.0), 360.0);
    CHECK_EQ(GravityWell::SpinDegrees(lap, 3000.0, -1.0), -360.0);
    CHECK_EQ(GravityWell::SpinDegrees(lap, 0.0, 1.0), 0.0);
    // Between, it overshoots the turn and comes back to it, and the sign is the lap's.
    double most = 0.0;
    double mostAt = 0.0;
    for (double ms = 0.0; ms <= 3000.0; ms += 1000.0 / 60.0) {
        const double turned = GravityWell::SpinDegrees(lap, ms, 1.0);
        CHECK_MSG(std::fabs(GravityWell::SpinDegrees(lap, ms, -1.0) + turned) < 1e-9, "the other way, the same turn");
        if (turned > most) {
            most = turned;
            mostAt = ms;
        }
    }
    std::printf("  a lapped agent turns %.3f degrees at most, at %.0f ms, and settles on 360 at 3000\n", most, mostAt);
    // elastic's first crest is near n = 2 s = 0.15, where 2^-1.5 is over 1: about
    // 1.37 turns, a quarter of a second in (494 degrees at 250 ms on this sampling).
    CHECK_MSG(most > 480.0 && most < 500.0 && mostAt > 200.0 && mostAt < 320.0,
              "it overshoots the whole turn by a third, early: " + std::to_string(most));
}

// computeRoundabout's sweep on its own: nothing on the first call, a step only
// past 11.48 degrees, nothing out of range and nothing forgotten there, and a lap
// strictly past 360 - counter-clockwise on the screen (+y down) positive.
void ASweepCountsTheWayRoundAndLaps() {
    GravityWell::Rules rules;
    std::string error;
    if (!GravityWell::LoadRules(kPortData + "/gravitywell.json", rules, error)) return;
    const GravityWell::Roundabout& lap = rules.roundabout;
    // A body on a circle round a well at the origin, at `deg` counter-clockwise on
    // the screen from the well's right: (cos, -sin), y down.
    const auto toWell = [](double deg) {
        const double rad = deg * 3.14159265358979323846 / 180.0;
        const float x = -static_cast<float>(std::cos(rad));
        const float y = static_cast<float>(std::sin(rad));
        const float length = std::sqrt(x * x + y * y);
        return glm::vec2(x / length, y / length);
    };
    const auto bodyY = [](double deg) { return -static_cast<float>(std::sin(deg * 3.14159265358979323846 / 180.0)); };

    GravityWell::Sweep sweep;
    double degrees = 0.0;
    CHECK_MSG(!GravityWell::StepSweep(lap, sweep, true, toWell(0.0), true, 0.0f, bodyY(0.0), degrees) &&
                  sweep.roundSum == 0.0f,
              "the first call starts the sweep and counts nothing");
    // 5 and 10 degrees are under acos(0.98): not counted, and the last direction kept.
    GravityWell::StepSweep(lap, sweep, false, toWell(5.0), true, 0.0f, bodyY(5.0), degrees);
    GravityWell::StepSweep(lap, sweep, false, toWell(10.0), true, 0.0f, bodyY(10.0), degrees);
    CHECK_MSG(sweep.roundSum == 0.0f, "a drift under 11.48 degrees is not a step");
    GravityWell::StepSweep(lap, sweep, false, toWell(15.0), true, 0.0f, bodyY(15.0), degrees);
    CHECK_MSG(std::fabs(sweep.roundSum - 0.2617994f) < 1e-4f,
              "past it, the whole 15 degrees since the last step, counter-clockwise positive: " +
                  std::to_string(sweep.roundSum));
    // Out of range: nothing counted, nothing forgotten.
    GravityWell::StepSweep(lap, sweep, false, toWell(60.0), false, 0.0f, bodyY(60.0), degrees);
    CHECK_MSG(std::fabs(sweep.roundSum - 0.2617994f) < 1e-4f, "out of range the sum is kept and not added to");

    // Round and round, counter-clockwise in 15 degree steps: a lap past 360.
    GravityWell::Sweep ccw;
    GravityWell::StepSweep(lap, ccw, true, toWell(0.0), true, 0.0f, bodyY(0.0), degrees);
    int laps = 0;
    double lapped = 0.0;
    int stepsToLap = 0;
    for (int step = 1; step <= 60 && laps == 0; ++step) {
        const double deg = step * 15.0;
        if (GravityWell::StepSweep(lap, ccw, false, toWell(deg), true, 0.0f, bodyY(deg), degrees)) {
            ++laps;
            lapped = degrees;
            stepsToLap = step;
        }
    }
    std::printf("  counter-clockwise in 15 degree steps: a lap of %.4f degrees on step %d\n", lapped, stepsToLap);
    CHECK_MSG(laps == 1 && lapped > 360.0 && lapped < 376.0 && ccw.ccw == 1 && ccw.cw == 0 && ccw.roundSum == 0.0f,
              "a counter-clockwise lap, positive, counted ccw, and the sum starts again");
    // And clockwise, negative.
    GravityWell::Sweep cw;
    GravityWell::StepSweep(lap, cw, true, toWell(0.0), true, 0.0f, bodyY(0.0), degrees);
    double back = 0.0;
    for (int step = 1; step <= 60 && back == 0.0; ++step) {
        const double deg = -step * 15.0;
        if (GravityWell::StepSweep(lap, cw, false, toWell(deg), true, 0.0f, bodyY(deg), degrees)) back = degrees;
    }
    CHECK_MSG(back < -360.0 && back > -376.0 && cw.cw == 1, "a clockwise lap, negative: " + std::to_string(back));
}

// And through Game::Tick: the player walked round level16c's well
// counter-clockwise on the screen, a lap recorded for it on the tick it passes
// 360, and none for a body that stays still.
void ThePlayerLapsAWell() {
    Game::Data data;
    entt::registry registry;
    Game::Level level;
    if (!Open(kWeightless, data, registry, level)) return;
    if (level.wells.wells.empty() || level.player == entt::null) return;
    const GravityWell::Well& well = level.wells.wells.front();
    const double radius = well.radiusPx * 0.5;
    int lapTick = -1;
    double degrees = 0.0;
    for (int tick = 0; tick < 200 && lapTick < 0; ++tick) {
        // 4 degrees a tick: 90 ticks a turn.
        const double rad = tick * 4.0 * 3.14159265358979323846 / 180.0;
        const glm::dvec2 atPx = well.atPx + glm::dvec2(std::cos(rad), -std::sin(rad)) * radius;
        const glm::vec3 at = Units::ToWorld(atPx.x, atPx.y);
        auto& transform = registry.get<TransformComponent>(level.player);
        transform.position = glm::vec3(at.x, at.y, transform.position.z);
        registry.get<RigidBodyComponent>(level.player).velocity = glm::vec3(0.0f);
        Game::Tick(data, registry, level, 0.0f, kStep);
        for (const GravityWell::Lap& lap : level.wells.lapsNow) {
            if (lap.body == level.player && lap.well == 0) {
                lapTick = tick;
                degrees = lap.degrees;
            }
        }
    }
    std::printf("  %s: the player laps %s on tick %d, %.3f degrees\n", kWeightless, well.name.c_str(), lapTick,
                degrees);
    CHECK_MSG(lapTick > 85 && lapTick < 100 && degrees > 360.0, "one lap, a whole turn in, counter-clockwise");
    Game::Tick(data, registry, level, 0.0f, kStep);
    CHECK_MSG(level.wells.lapsNow.empty(), "the record is the tick's own");
}

void TheRoleIsPlayed() {
    CHECK(Roles::IsPorted(Roles::kGravityWell));
    // It does not MOVE, though: the agent is a solid circle that stands still.
    CHECK(!Roles::Moves(Roles::kGravityWell));
}

void runTests() {
    TheRulesRead();
    TenWellsAcrossFiveLevels();
    TheRingIsTwentyFourUnitsNarrowerThanTheWell();
    TheRingIsTheAreaSeen();
    ARingThatIsNotTheEntitysIsRefused();
    ItPullsInwardAndFadesToNothingAtTheRim();
    ItPullsHarderWhereTheWorldHasGravity();
    AWellRefusesNoPortal();
    AShotFliesThroughAWell();
    ItTakesTheLooseBodiesOnly();
    TheRoundaboutRulesRead();
    TheAgentTurnsOnceAndSettles();
    ASweepCountsTheWayRoundAndLaps();
    ThePlayerLapsAWell();
    TheRoleIsPlayed();
}

} // namespace

int main() {
    std::error_code ec;
    if (!std::filesystem::is_directory(kLevels, ec) ||
        !std::filesystem::is_regular_file(kData + "/entity_roles.json", ec)) {
        std::printf("test_mp_wells: SKIPPED - needs the converted levels at %s\n"
                    "  and the remake's data at %s.\n"
                    "  Both live outside this repository; configure with\n"
                    "  -DSUPERSONIC_MAGICPORTALS_LEVELS=... and -DSUPERSONIC_MAGICPORTALS_DATA=...\n",
                    kLevels.c_str(), kData.c_str());
        return 77;
    }
    runTests();
    return ::test::summary("test_mp_wells", 90);
}
