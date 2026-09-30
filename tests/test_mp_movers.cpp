// What moves in a level besides its doors: moving platforms and lifts, and the
// anti-portal agents that patrol.
//
// Pinned exactly, because the files say it: which of chapter 1's levels have
// which, where each stands, its speed, its stride and its end markers. Judged by
// threshold: a platform swings by its stride with the velocity that gets it
// there, a lift reaches its far marker when its speed says, and a patrolling
// agent swings on its axis and refuses no tap itself - the ring it stands in
// does (the remaster record's step 126).
//
// AND WHAT BECOMES OF AN AGENT (step 126, R17): a portal shot still in flight
// that comes within 0.7 x its size kills it and itself, and takes the one ring
// round it. These cases would sit in test_mp_shot beside the shot's other ends;
// they are here because Smart App Control refused both of step 126's links of
// test_mp_shot, and a suite that is launched anyway costs no second refusal. The numbers behind
// the speeds - movers.json's rate scale and lift speed - are the Godot project's
// guesses, carried as data, so the thresholds are computed from them.
//
// Reads the converted levels and Enhanced's data from outside this repository,
// and skips, saying where it looked, when either is absent.

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "core/Components.hpp"
#include "sim/Game.hpp"
#include "sim/Mover.hpp"
#include "sim/Portals.hpp"
#include "sim/Units.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <limits>
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
constexpr double kPi = 3.14159265358979323846;

struct Run {
    Game::Data data;
    entt::registry registry;
    Game::Level level;
};

bool Begin(const std::string& levelName, Run& run) {
    std::string error;
    const bool ok = Game::LoadData(kLevels + "/" + levelName + ".tscn", kData,
                                   std::filesystem::temp_directory_path() / "supersonic-test-mp-movers", run.data,
                                   error) &&
                    Game::Start(run.data, run.registry, run.level, error);
    CHECK_MSG(ok, levelName + ": " + error);
    return ok;
}

void Tick(Run& run) {
    Game::Tick(run.data, run.registry, run.level, 0.0f, kStep);
}

glm::dvec2 BodyPx(const Run& run, entt::entity entity) {
    return Units::ToPixels(run.registry.get<TransformComponent>(entity).position);
}

std::string Px(const glm::dvec2& p) {
    char text[48];
    std::snprintf(text, sizeof text, "(%.2f, %.2f)", p.x, p.y);
    return text;
}

// ---- What the levels say ------------------------------------------------------

void MoversJsonCarriesTheRemakesGuesses() {
    Mover::Rules rules;
    std::string error;
    CHECK_MSG(Mover::LoadRules(std::string(MAGICPORTALS_PORT_DATA_DIR) + "/movers.json", rules, error), error);
    // Guesses, carried and not pinned: only that there is one of each.
    CHECK(rules.oscillationRateScale > 0.0);
    CHECK(rules.liftSpeedPx > 0.0);
}

void ChapterOnesMovers() {
    // Read off the level files with grep.
    {
        Run run;
        if (Begin("level5", run)) {
            const auto& platforms = run.level.movers.platforms;
            CHECK_EQ(platforms.size(), std::size_t{1});
            if (platforms.size() == 1) {
                CHECK(platforms[0].name == "moving_platform_764");
                CHECK(platforms[0].motion.originPx == glm::dvec2(416.0, 144.0));
                CHECK(platforms[0].motion.speed == 0.7);
                CHECK(platforms[0].motion.stridePx == 64.0);
                CHECK(platforms[0].motion.axis == glm::dvec2(0.0, 1.0));
            }
            CHECK(run.level.movers.lifts.empty());
        }
    }
    {
        Run run;
        if (Begin("level10", run)) {
            const Mover::Platform* platform = run.level.movers.FindPlatform("moving_platform_707");
            CHECK(platform != nullptr && platform->motion.originPx == glm::dvec2(32.0, 184.0) &&
                  platform->motion.speed == 1.0 && platform->motion.stridePx == 70.0);
            const Portals::State& portals = run.level.portals;
            // Agents, and no zone (step 126).
            const Portals::Agent* up = portals.FindAgent("anti_portal_agent_639");
            const Portals::Agent* side = portals.FindAgent("anti_portal_agent_599");
            const Portals::Agent* still = portals.FindAgent("anti_portal_agent_710");
            CHECK(portals.FindZone("anti_portal_agent_639") == nullptr && portals.agents.size() == 3);
            CHECK(up != nullptr && up->centrePx == glm::dvec2(674.0, 60.0) && up->moving &&
                  up->motion.axis == glm::dvec2(0.0, 1.0) && up->motion.speed == 3.0 && up->motion.stridePx == 48.0);
            CHECK(side != nullptr && side->centrePx == glm::dvec2(384.0, 80.0) && side->moving &&
                  side->motion.axis == glm::dvec2(1.0, 0.0) && side->motion.speed == 2.0 &&
                  side->motion.stridePx == 36.0);
            CHECK(still != nullptr && still->centrePx == glm::dvec2(128.0, 100.0) && !still->moving && !still->gone);
            // And each agent stands on a static antiportal of its own.
            struct Field {
                const char* name;
                glm::dvec2 at;
                double scale;
            };
            const Field fields[] = {
                {"antiportal_638", {674.0, 60.0}, 3.5},
                {"antiportal_574", {384.0, 80.0}, 3.5},
                {"antiportal_709", {128.0, 100.0}, 6.0},
            };
            for (const Field& expected : fields) {
                const Portals::NoPortalZone* field = portals.FindZone(expected.name);
                CHECK_MSG(field != nullptr && field->centrePx == expected.at && field->scale == expected.scale &&
                              !field->moving && field->entity == "antiportal" && !field->gone,
                          expected.name);
            }
        }
    }
    struct Shuttles {
        const char* level;
        const char* lift;
        glm::dvec2 a;
        glm::dvec2 b;
    };
    const Shuttles lifts[] = {
        {"level26", "lift_1186", {544.0, 240.0}, {544.0, 144.0}},
        {"level26", "lift_ent_1122", {96.0, 80.0}, {96.0, 240.0}},
        {"level27", "lift_ent_694", {96.0, 240.0}, {96.0, 112.0}},
        {"level28", "lift_ent_796", {96.0, 240.0}, {96.0, 112.0}},
    };
    for (const Shuttles& expected : lifts) {
        Run run;
        if (!Begin(expected.level, run)) continue;
        const Mover::Lift* lift = run.level.movers.FindLift(expected.lift);
        CHECK_MSG(lift != nullptr, std::string(expected.level) + " has " + expected.lift);
        if (lift == nullptr) continue;
        CHECK_MSG(lift->motion.aPx == expected.a && lift->motion.bPx == expected.b && lift->motion.atPx == expected.a,
                  std::string(expected.lift) + " runs " + Px(lift->motion.aPx) + " to " + Px(lift->motion.bPx));
        CHECK(lift->motion.speedPx == run.data.movers.liftSpeedPx);
    }
}

// ---- Moving -------------------------------------------------------------------

void APlatformSwingsByItsStride() {
    Run run;
    if (!Begin("level5", run) || run.level.movers.platforms.empty()) return;
    const Mover::Platform& platform = run.level.movers.platforms.front();
    const double period = 2.0 * kPi / (run.data.movers.oscillationRateScale * platform.motion.speed);
    const int ticks = static_cast<int>(std::ceil(period / kStep)) + 1;
    double lo = std::numeric_limits<double>::max();
    double hi = std::numeric_limits<double>::lowest();
    bool where = true;
    bool upright = true;
    bool velocity = true;
    for (int i = 0; i < ticks; ++i) {
        const glm::vec3 before = run.registry.get<TransformComponent>(platform.entity).position;
        Tick(run);
        const glm::vec3 after = run.registry.get<TransformComponent>(platform.entity).position;
        const glm::dvec2 at = BodyPx(run, platform.entity);
        lo = std::min(lo, at.y);
        hi = std::max(hi, at.y);
        where = where && glm::distance(at, platform.motion.AtPx()) < 0.01;
        upright = upright && std::fabs(at.x - 416.0) < 0.01;
        // It moves with the velocity that gets it there, as a mover must to carry
        // what stands on it (Mover.hpp).
        const glm::vec3 v = run.registry.get<RigidBodyComponent>(platform.entity).velocity;
        velocity = velocity && glm::length(v - (after - before) / kStep) < 1e-3f;
    }
    std::printf("  level5's platform: swung %.2f px over %.2f s, from y %.2f to %.2f\n", hi - lo, period, lo, hi);
    CHECK_MSG(hi - lo > 63.0 && hi - lo <= 64.0 + 1e-6, "a swing of its stride, 64 px");
    CHECK(where);
    CHECK(upright);
    CHECK(velocity);
}

void ALiftGoesToItsFarMarkerAndBack() {
    Run run;
    if (!Begin("level27", run) || run.level.movers.lifts.empty()) return;
    const Mover::Lift& lift = run.level.movers.lifts.front();
    const glm::dvec2 a = lift.motion.aPx;
    const glm::dvec2 b = lift.motion.bPx;
    const double legTicks = glm::distance(a, b) / lift.motion.speedPx / kStep;
    int atB = -1;
    int backAtA = -1;
    bool where = true;
    for (int tick = 1; tick <= static_cast<int>(legTicks * 3.0) && backAtA < 0; ++tick) {
        Tick(run);
        where = where && glm::distance(BodyPx(run, lift.entity), lift.motion.atPx) < 0.01;
        if (atB < 0 && lift.motion.atPx == b) atB = tick;
        if (atB > 0 && backAtA < 0 && lift.motion.atPx == a) backAtA = tick;
    }
    std::printf("  level27's lift: at its far marker after %d ticks and back after %d, for %.1f a leg\n", atB, backAtA,
                legTicks);
    CHECK_MSG(atB > 0 && std::fabs(atB - legTicks) <= 1.0, "at b when its speed says");
    CHECK_MSG(backAtA > 0 && std::fabs(backAtA - 2.0 * legTicks) <= 2.0, "and back at a");
    CHECK(where);
}

void APatrollingAgentSwingsAndRefusesNothing() {
    Run run;
    if (!Begin("level10", run)) return;
    const Portals::Agent* agent = run.level.portals.FindAgent("anti_portal_agent_639");
    const Portals::NoPortalZone* field = run.level.portals.FindZone("antiportal_638");
    CHECK(agent != nullptr && agent->moving);
    CHECK(field != nullptr && !field->moving);
    if (agent == nullptr || field == nullptr) return;
    const double radius = run.level.portals.rules.antiportalRadiusPx;
    const glm::dvec2 stood(674.0, 60.0);

    // It swings: a quarter of its period on, it is half its stride from where
    // the level placed it, down the screen.
    const double quarter = kPi / 2.0 / (run.data.movers.oscillationRateScale * agent->motion.speed);
    const int ticks = static_cast<int>(std::lround(quarter / kStep));
    for (int i = 0; i < ticks; ++i) Tick(run);
    const glm::dvec2 now = agent->CentreNowPx();
    CHECK_MSG(now.x == 674.0 && std::fabs(now.y - (60.0 + 24.0)) < 0.5, "at " + Px(now));

    // IT REFUSES NOTHING ITSELF (step 126): with the agent alone and no zone, a
    // portal opens where it is and where it stood. The original's
    // AntiPortalManager gathers antiportals alone, and the agent has no shape.
    Portals::State alone;
    alone.rules = run.level.portals.rules;
    alone.budget = Portals::kPairSize;
    alone.agents.push_back(*agent);
    CHECK(alone.TryPlace(now));
    CHECK(alone.TryPlace(stood));

    // In level10 the ring it stands in refuses both, all the while: antiportal_638
    // holds its whole swing, at the port's radius.
    CHECK(agent->motion.stridePx * 0.5 < radius * field->scale);
    CHECK(!run.level.portals.TryPlace(now));
    CHECK(!run.level.portals.TryPlace(stood));

    // The sideways one moves along x and only x.
    const Portals::Agent* side = run.level.portals.FindAgent("anti_portal_agent_599");
    CHECK(side != nullptr && side->CentreNowPx().y == 80.0 && side->CentreNowPx().x != 384.0);
}

// ---- The anti-portal agent's death (step 126, R17) --------------------------------
//
// ETHCallback_anti_portal_agent (bytes 336011..337087): a portal shot still in
// flight within 0.7 x the agent's size deletes the shot and the agent, and
// deleteAntiPortalInThisBucket then deletes the first ring spelled exactly
// `antiportal` whose field holds the agent. The agent itself refuses nothing.

// The cooldowns off, as test_mp_shot turns them off for the shot's geometry:
// each case fires the moment its level is built.
bool BeginShooting(const std::string& levelName, Run& run) {
    if (!Begin(levelName, run)) return false;
    run.level.portals.rules.firstPortalMinMs = 0.0;
    run.level.portals.rules.nextPortalMinMs = 0.0;
    run.data.portals.firstPortalMinMs = 0.0;
    run.data.portals.nextPortalMinMs = 0.0;
    return true;
}

// A bare State with the level's rules and a shooter at (100, 200); one agent at
// (300, 200) inside a bare `antiportal` of scale 3 and an `antiportal.ent` of
// scale 1 on the same point.
struct Rig {
    entt::registry registry;
    Portals::State state;
};

bool MakeRig(Rig& rig) {
    Run run;
    if (!BeginShooting("level1", run)) return false;
    rig.state.rules = run.data.portals;
    rig.state.shot = run.data.shot;
    rig.state.budget = 2;
    rig.state.shooter = rig.registry.create();
    rig.registry.emplace<TransformComponent>(rig.state.shooter).position = Units::ToWorld(100.0, 200.0);
    Portals::NoPortalZone bare;
    bare.centrePx = glm::dvec2(300.0, 200.0);
    bare.scale = 3.0;
    bare.name = "ring_bare";
    bare.entity = "antiportal";
    Portals::NoPortalZone spelled = bare;
    spelled.scale = 1.0;
    spelled.name = "ring_ent";
    spelled.entity = "antiportal.ent";
    rig.state.zones = {bare, spelled};
    Portals::Agent agent;
    agent.name = "agent";
    agent.centrePx = glm::dvec2(300.0, 200.0);
    rig.state.agents = {agent};
    return true;
}

void FlyOut(Rig& rig) {
    for (int tick = 0; tick < 600 && rig.state.flight; ++tick) rig.state.Tick(rig.registry, kStep);
}

void TheAgentsNumbersAreTheDecodedOnes() {
    Run run;
    if (!Begin("level12b", run)) return;
    const Portals::Rules::AgentRules& agent = run.data.portals.agent;
    CHECK(agent.sizeUnits == 32.0);          // anti_portal_agent.png, 32 x 32
    CHECK(agent.killShare == 0.7);           // #11-22
    CHECK(agent.ringEntity == "antiportal"); // #13-33, the bare spelling
    CHECK(agent.bucketUnits == 256.0);       // _ETH_DEFAULT_BUCKET_SIZE
    CHECK(agent.seekNeighbours == (std::vector<glm::dvec2>{{-1.0, 0.0}, {1.0, 0.0}, {0.0, -1.0}, {0.0, 1.0}}));
    // Its own role, ported, and no zone (entity_roles.json).
    CHECK(run.data.roles.roleOf.at("anti_portal_agent") == Roles::kAntiPortalAgent);
    CHECK(run.data.roles.roleOf.at("anti_portal_agent.ent") == Roles::kAntiPortalAgent);
    CHECK(Roles::IsPorted(Roles::kAntiPortalAgent));
    // 3-13: the one agent, at (82, 84), and the ring round it, scale 13.
    const Portals::State& portals = run.level.portals;
    CHECK_EQ(portals.agents.size(), std::size_t{1});
    const Portals::Agent* found = portals.FindAgent("anti_portal_agent_ent_1536");
    CHECK(found != nullptr && found->centrePx == glm::dvec2(82.0, 84.0) && found->scale == 1.0 && !found->moving);
    CHECK(portals.FindZone("anti_portal_agent_ent_1536") == nullptr);
    const Portals::NoPortalZone* ring = portals.FindZone("antiportal_1520");
    CHECK(ring != nullptr && ring->entity == "antiportal" && ring->scale == 13.0 && !ring->gone);
    if (found != nullptr) CHECK_NEAR(static_cast<float>(portals.AgentReachPx(*found)), 22.4f);
}

void AnAgentKillsAShotStillInFlightAndItsRing() {
    Rig rig;
    if (!MakeRig(rig)) return;
    // Before: the bare ring (96 px) and the spelled one (32 px) both refuse.
    CHECK(rig.state.InField(glm::dvec2(300.0, 240.0)));
    CHECK(rig.state.InField(glm::dvec2(300.0, 210.0)));
    CHECK(rig.state.Shoot(rig.registry, glm::dvec2(500.0, 200.0)));
    std::vector<Portals::AgentKill> kills;
    int killedOn = -1;
    for (int tick = 0; tick < 600 && rig.state.flight; ++tick) {
        rig.state.Tick(rig.registry, kStep);
        if (!rig.state.agentKills.empty()) {
            kills = rig.state.agentKills;
            killedOn = tick;
        }
    }
    CHECK_EQ(kills.size(), std::size_t{1});
    CHECK_EQ(rig.state.agentsKilled, 1);
    if (kills.size() != 1) return;
    const Portals::AgentKill& kill = kills[0];
    // Where it first came within 0.7 x 32 = 22.4 px, short of the agent.
    CHECK_MSG(kill.agent == "agent" && kill.atPx == glm::dvec2(300.0, 200.0) &&
                  glm::distance(kill.shotPx, glm::dvec2(300.0 - 22.4, 200.0)) < 1e-9,
              "killed at " + Px(kill.shotPx));
    // One ring, the bare spelling; the antiportal.ent on the same point stays.
    CHECK_MSG(kill.ring == "ring_bare", "deleted " + kill.ring);
    CHECK(rig.state.zones[0].gone && !rig.state.zones[1].gone);
    CHECK(rig.state.agents[0].gone);
    // The shot is deleted, not failed: no failure, no portal, nothing spent.
    CHECK(!rig.state.flight && rig.state.placed.empty() && rig.state.failures.empty());
    CHECK_EQ(rig.state.shotsFailed, 0);
    CHECK_EQ(rig.state.portalsUsed, 0);
    CHECK(rig.state.lastFailure.empty());
    // The record is the tick's own.
    rig.state.Tick(rig.registry, kStep);
    CHECK(rig.state.agentKills.empty());
    // Where the bare ring was, a portal may open; the spelled one still refuses.
    CHECK(!rig.state.InField(glm::dvec2(300.0, 240.0)));
    CHECK(rig.state.InField(glm::dvec2(300.0, 210.0)));
    CHECK(rig.state.TryPlace(glm::dvec2(300.0, 240.0)));
    std::printf("  an agent killed a shot on its tick %d at %s and took %s\n", killedOn, Px(kill.shotPx).c_str(),
                kill.ring.c_str());
}

void AnAgentRefusesNothingAndLetsAWideShotBy() {
    // The agent alone, with no ring: a portal opens on it (TryPlace, as an
    // arriving shot opens one).
    {
        Rig rig;
        if (!MakeRig(rig)) return;
        rig.state.zones.clear();
        CHECK(!rig.state.InField(glm::dvec2(300.0, 200.0)));
        CHECK(rig.state.TryPlace(glm::dvec2(300.0, 200.0)));
    }
    // A shot passing 24.8 px off flies on and opens its portal.
    {
        Rig rig;
        if (!MakeRig(rig)) return;
        rig.state.zones.clear();
        CHECK(rig.state.Shoot(rig.registry, glm::dvec2(500.0, 250.0)));
        FlyOut(rig);
        CHECK_EQ(rig.state.agentsKilled, 0);
        CHECK(!rig.state.agents[0].gone && rig.state.placed.size() == 1);
    }
    // One whose way ends 30 px short of it never comes within reach.
    {
        Rig rig;
        if (!MakeRig(rig)) return;
        rig.state.zones.clear();
        CHECK(rig.state.Shoot(rig.registry, glm::dvec2(270.0, 200.0)));
        FlyOut(rig);
        CHECK_EQ(rig.state.agentsKilled, 0);
        CHECK(rig.state.placed.size() == 1);
    }
    // Nor one whose way ends a tenth of a pixel short of its reach: the shot has
    // arrived, and is no longer in flight (#79-108), before it would come near.
    {
        Rig rig;
        if (!MakeRig(rig)) return;
        rig.state.zones.clear();
        CHECK(rig.state.Shoot(rig.registry, glm::dvec2(300.0 - 22.5, 200.0)));
        FlyOut(rig);
        CHECK_EQ(rig.state.agentsKilled, 0);
        CHECK(rig.state.placed.size() == 1);
    }
    // A dead agent kills nothing more.
    {
        Rig rig;
        if (!MakeRig(rig)) return;
        rig.state.agents[0].gone = true;
        rig.state.zones.clear();
        CHECK(rig.state.Shoot(rig.registry, glm::dvec2(500.0, 200.0)));
        FlyOut(rig);
        CHECK_EQ(rig.state.agentsKilled, 0);
        CHECK(rig.state.placed.size() == 1);
    }
}

void TheRingIsSoughtInTheAgentsBucketThenItsNeighbours() {
    // deleteAntiPortalInThisBucket's order: the agent's own 256 px bucket, then
    // left, right, up and down; the first ring whose field holds the agent.
    Rig rig;
    if (!MakeRig(rig)) return;
    rig.state.zones.clear();
    Portals::NoPortalZone ring;
    ring.entity = "antiportal";
    ring.scale = 10.0; // 320 px at the port's radius
    // The agent at (250, 250), in bucket (0, 0). Rings holding it: one in bucket
    // (1, 0) to its right, one in bucket (0, 1) below, one in its own.
    ring.name = "right";
    ring.centrePx = glm::dvec2(300.0, 250.0);
    rig.state.zones.push_back(ring);
    ring.name = "below";
    ring.centrePx = glm::dvec2(250.0, 300.0);
    rig.state.zones.push_back(ring);
    ring.name = "own";
    ring.centrePx = glm::dvec2(200.0, 250.0);
    rig.state.zones.push_back(ring);
    CHECK(rig.state.DeleteRingAt(glm::dvec2(250.0, 250.0)) == "own");
    CHECK(rig.state.DeleteRingAt(glm::dvec2(250.0, 250.0)) == "right");
    CHECK(rig.state.DeleteRingAt(glm::dvec2(250.0, 250.0)) == "below");
    CHECK(rig.state.DeleteRingAt(glm::dvec2(250.0, 250.0)).empty());
    // Two buckets off is not sought, and nor is a ring that does not hold it.
    rig.state.zones.clear();
    ring.scale = 40.0;
    ring.name = "two off";
    ring.centrePx = glm::dvec2(600.0, 250.0); // bucket (2, 0)
    rig.state.zones.push_back(ring);
    ring.scale = 0.5; // 16 px, the agent 20 px off
    ring.name = "too small";
    ring.centrePx = glm::dvec2(230.0, 250.0);
    rig.state.zones.push_back(ring);
    CHECK(rig.state.DeleteRingAt(glm::dvec2(250.0, 250.0)).empty());
    CHECK(!rig.state.zones[0].gone && !rig.state.zones[1].gone);
}

// FOOTAGE F17 (out/parity/footage/F17.md 1.1-1.2), 4-01: the player against the
// locked pillar at (153, 146), a tap at (450.1, 85.0). The shot passes 16 u from
// anti_portal_agent_ent_2107 (418, 108) and the agent bursts; antiportal_2108's
// field is gone, and F17's second portal, 14 u over the block, may open.
void F17sShotKillsTheAgentOn401() {
    Run run;
    if (!BeginShooting("level0c", run)) return;
    Portals::State& portals = run.level.portals;
    const glm::dvec2 second(372.0, 98.0);
    CHECK_MSG(portals.InField(second), "F17's second portal is inside antiportal_2108 before the kill");
    // The port-only way round the agent the audit found under the old 16 x 4 = 64:
    // a portal at (490, 60), 68 px from the ring's centre, inside the original's
    // 32 x 4 = 128 (R1, step 127) while the agent lives.
    const glm::dvec2 aroundIt(490.0, 60.0);
    CHECK_MSG(portals.InField(aroundIt), "(490, 60) is inside antiportal_2108 at 32 x scale");
    auto& transform = run.registry.get<TransformComponent>(run.level.player);
    const glm::vec3 at = Units::ToWorld(153.0, 146.0);
    transform.position = glm::vec3(at.x, at.y, transform.position.z);
    run.registry.get<RigidBodyComponent>(run.level.player).velocity = glm::vec3(0.0f);
    CHECK(portals.Shoot(run.registry, glm::dvec2(450.1, 85.0)));
    std::vector<Portals::AgentKill> kills;
    for (int tick = 0; tick < 600 && portals.flight; ++tick) {
        Tick(run);
        for (const Portals::AgentKill& kill : portals.agentKills) kills.push_back(kill);
    }
    CHECK_MSG(kills.size() == 1, "killed once, not failed at " + portals.lastFailure);
    if (kills.size() != 1) return;
    CHECK(kills[0].agent == "anti_portal_agent_ent_2107" && kills[0].ring == "antiportal_2108");
    CHECK(kills[0].atPx == glm::dvec2(418.0, 108.0));
    CHECK_MSG(glm::distance(kills[0].shotPx, kills[0].atPx) < 22.4 + 1e-9, Px(kills[0].shotPx));
    CHECK_EQ(portals.shotsFailed, 0);
    CHECK(portals.placed.empty());
    CHECK_MSG(!portals.InField(second), "and after it, it is not");
    CHECK_MSG(!portals.InField(aroundIt), "nor (490, 60)");
    CHECK(portals.TryPlace(second));
    std::printf("  4-01, F17's shot: %s killed at %s, %s deleted\n", kills[0].agent.c_str(),
                Px(kills[0].shotPx).c_str(), kills[0].ring.c_str());
}

// ---- An antiportal refuses at 32 x scale (step 127, R1) ---------------------------
//
// FOOTAGE F3 (out/parity/footage/F3.md sections 2 and 6.1), 1-7 (level6): the
// player at the spawn, every ray clear, antiportal_687 at (288, 192), scale 3.
// The original refused taps 35.6, 78.7, 93.9 and 94.9 u from its centre and
// accepted 97.1 and 98.1: an edge in (31.64, 32.36] x scale. The port plays
// 32 x 3 = 96; its old 16 x 3 = 48 accepted the last three refusals.
void F3sTapsBracketTheFieldOn107() {
    Run run;
    if (!Begin("level6", run)) return;
    Portals::State& portals = run.level.portals;
    const Portals::NoPortalZone* ring = portals.FindZone("antiportal_687");
    CHECK(ring != nullptr);
    if (ring == nullptr) return;
    CHECK(ring->centrePx == glm::dvec2(288.0, 192.0) && ring->scale == 3.0 && ring->entity == "antiportal");
    CHECK_NEAR(static_cast<float>(portals.rules.antiportalRadiusPx * ring->scale), 96.0f);
    const glm::dvec2 refused[] = {{252.44, 192.0}, {213.33, 216.89}, {194.13, 192.0}, {193.07, 192.0}};
    for (const glm::dvec2& tap : refused) {
        CHECK_MSG(portals.InField(tap) && portals.Holds(*ring, tap),
                  "F3 refused " + Px(tap) + ", " + std::to_string(glm::distance(tap, ring->centrePx)) + " u off");
        CHECK(!portals.TryPlace(tap));
    }
    const glm::dvec2 accepted[] = {{190.93, 192.0}, {189.87, 192.0}};
    for (const glm::dvec2& tap : accepted) {
        CHECK_MSG(!portals.InField(tap),
                  "F3 accepted " + Px(tap) + ", " + std::to_string(glm::distance(tap, ring->centrePx)) + " u off");
        CHECK(portals.TryPlace(tap));
    }
    CHECK_EQ(portals.portalsUsed, 2);
    // The port's one no_portal zone, no_portal_705 at (232, 194): not the
    // manager's, but tested at the same radius x its scale of 1 - and all of it
    // inside antiportal_687's field, so it refuses nothing the ring does not.
    const Portals::NoPortalZone* sign = portals.FindZone("no_portal_705");
    CHECK(sign != nullptr);
    if (sign != nullptr) {
        CHECK(sign->scale == 1.0);
        CHECK(glm::distance(sign->centrePx, ring->centrePx) + portals.rules.antiportalRadiusPx * sign->scale <
              portals.rules.antiportalRadiusPx * ring->scale);
    }
}

void runTests() {
    MoversJsonCarriesTheRemakesGuesses();
    ChapterOnesMovers();
    APlatformSwingsByItsStride();
    ALiftGoesToItsFarMarkerAndBack();
    APatrollingAgentSwingsAndRefusesNothing();
    TheAgentsNumbersAreTheDecodedOnes();
    AnAgentKillsAShotStillInFlightAndItsRing();
    AnAgentRefusesNothingAndLetsAWideShotBy();
    TheRingIsSoughtInTheAgentsBucketThenItsNeighbours();
    F17sShotKillsTheAgentOn401();
    F3sTapsBracketTheFieldOn107();
}

} // namespace

MP_SUITE_MAIN("test_mp_movers") {
    std::error_code ec;
    if (!std::filesystem::is_directory(kLevels, ec) ||
        !std::filesystem::is_regular_file(kData + "/entity_roles.json", ec)) {
        std::printf("test_mp_movers: SKIPPED - needs the converted levels at %s\n"
                    "  and Enhanced's data at %s.\n"
                    "  Both live outside this repository; configure with\n"
                    "  -DSUPERSONIC_MAGICPORTALS_LEVELS=... and -DSUPERSONIC_MAGICPORTALS_DATA=...\n",
                    kLevels.c_str(), kData.c_str());
        return 77;
    }
    runTests();
    return ::test::summary("test_mp_movers", 30);
}
