// What moves in a level besides its doors: moving platforms and lifts, and the
// anti-portal agents that patrol.
//
// Pinned exactly, because the files say it: which of chapter 1's levels have
// which, where each stands, its speed, its stride and its end markers, and the
// game's fourteen lifts. Judged by threshold: a platform swings by its stride with
// the velocity that gets it there, a lift waits on its marker a until a touch sends
// it and reaches the other when 60 units a second says (the record's step 140), and
// a patrolling agent swings on its axis and refuses no tap itself - the ring it
// stands in does (the remaster record's step 126).
//
// AND WHAT BECOMES OF AN AGENT (step 126, R17): a portal shot still in flight
// that comes within 0.7 x its size kills it and itself, and takes the one ring
// round it. These cases would sit in test_mp_shot beside the shot's other ends;
// they are here because Smart App Control refused both of step 126's links of
// test_mp_shot, and a suite that is launched anyway costs no second refusal. The number behind
// the swings - movers.json's rate scale - is the Godot project's guess, carried as
// data, so the thresholds are computed from it; the lift's are decoded and pinned.
//
// AND THE PLATFORM GATE (step 136): nothing goes through a placed pair while a
// moving platform or a lift is at any placed portal, and a static pair is never
// held.
//
// Reads the converted levels and Enhanced's data from outside this repository,
// and skips, saying where it looked, when either is absent.

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "core/Components.hpp"
#include "sim/Game.hpp"
#include "sim/Mover.hpp"
#include "sim/Portals.hpp"
#include "sim/Roles.hpp"
#include "sim/Tscn.hpp"
#include "sim/Units.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <set>
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

void MoversJsonCarriesTheSwingsGuessAndTheDecodedLift() {
    Mover::Rules rules;
    std::string error;
    CHECK_MSG(Mover::LoadRules(std::string(MAGICPORTALS_PORT_DATA_DIR) + "/movers.json", rules, error), error);
    // The swing is a guess, carried and not pinned: only that there is one.
    CHECK(rules.oscillationRateScale > 0.0);
    // The lift is decoded (planning step 140; movers.json lift._source): the literal
    // 60 at ETHCallback_lift #286, lift.png's 64 x 32, and blinkColor's two globals
    // and stride.
    CHECK_MSG(rules.liftSpeedUnitsPerS == 60.0, "a lift runs at the callback's 60 units a second");
    CHECK_MSG(rules.liftSizeUnits == glm::dvec2(64.0, 32.0), "and is touched inside lift.png's 64 x 32");
    CHECK_MSG(rules.liftBlinkFrom == glm::dvec3(0.6) && rules.liftBlinkTo == glm::dvec3(1.0) &&
                  rules.liftBlinkStrideMs == 300.0,
              "and blinks 0.6 grey to white and back, 300 ms a leg");
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
}

// THE GAME'S FOURTEEN LIFTS (planning step 140): every lift in the 128 converted
// levels, both spellings, with the markers it names, read off the level files. Each
// is wired on its marker a, at rest there, at movers.json's 60 and lift.png's size -
// 2-02's lift_789 carries a `speed` of 120 that ETHCallback_lift never reads - and
// each node already stands on its marker a, so the first frame's SetPositionXY(a)
// moves none of them.
void TheFourteenLiftsWaitOnTheirMarkers() {
    struct Placed {
        const char* level;
        const char* lift;
        glm::dvec2 a;
        glm::dvec2 b;
    };
    const Placed placed[] = {
        {"level26", "lift_1186", {544.0, 240.0}, {544.0, 144.0}},  // 1-27
        {"level26", "lift_ent_1122", {96.0, 80.0}, {96.0, 240.0}}, // 1-27
        {"level27", "lift_ent_694", {96.0, 240.0}, {96.0, 112.0}}, // 1-28
        {"level28", "lift_ent_796", {96.0, 240.0}, {96.0, 112.0}}, // 1-29
        {"level1a", "lift_789", {96.0, 368.0}, {96.0, 94.0}},      // 2-02
        {"level5a", "lift_ent_832", {160.0, 112.0}, {160.0, 240.0}}, // 2-06
        {"level6a", "lift_ent_883", {352.0, 256.0}, {352.0, 80.0}},  // 2-07
        {"level8a", "lift_ent_1016", {32.0, 435.0}, {32.0, 102.0}},  // 2-09
        {"level17a", "lift_ent_1142", {480.0, 283.0}, {480.0, 108.0}}, // 2-18
        {"level24a", "lift_ent_1137", {608.0, 240.0}, {608.0, 161.0}}, // 2-25
        {"level7b", "lift_ent_1532", {96.0, 240.0}, {96.0, 144.0}},    // 3-08
        {"level9b", "lift_ent_1318", {96.0, 320.0}, {96.0, 160.0}},    // 3-10
        {"level22b", "lift_ent_1935", {96.0, 240.0}, {96.0, 162.0}},   // 3-23
        {"level30b", "lift_ent_2438", {480.0, 272.0}, {480.0, 400.0}}, // 3-31
    };
    // The census, off the files: a node whose entity is lift or lift.ent.
    int levels = 0;
    int lifts = 0;
    int unlisted = 0;
    std::set<std::string> liftLevels;
    std::error_code ec;
    for (const auto& file : std::filesystem::directory_iterator(kLevels, ec)) {
        if (file.path().extension() != ".tscn") continue;
        Tscn::Scene scene;
        std::string error;
        if (!Tscn::Load(file.path().string(), scene, error)) continue;
        ++levels;
        const std::string level = file.path().stem().string();
        for (const Tscn::Node& node : scene.nodes) {
            const std::string entity = Roles::EntityName(node);
            if (node.parent != "." || (entity != "lift" && entity != "lift.ent")) continue;
            ++lifts;
            liftLevels.insert(level);
            const bool listed = std::any_of(std::begin(placed), std::end(placed), [&](const Placed& p) {
                return level == p.level && node.name == p.lift;
            });
            if (!listed) ++unlisted;
        }
    }
    CHECK_MSG(levels == 128, "the 128 converted levels read: " + std::to_string(levels));
    CHECK_MSG(lifts == 14 && liftLevels.size() == 13u && unlisted == 0,
              "14 lifts on 13 levels, each one listed here: " + std::to_string(lifts) + " on " +
                  std::to_string(liftLevels.size()) + ", " + std::to_string(unlisted) + " unlisted");

    for (const Placed& expected : placed) {
        Run run;
        if (!Begin(expected.level, run)) continue;
        const Mover::Lift* lift = run.level.movers.FindLift(expected.lift);
        CHECK_MSG(lift != nullptr, std::string(expected.level) + " has " + expected.lift);
        if (lift == nullptr) continue;
        const Mover::Travel& motion = lift->motion;
        CHECK_MSG(motion.aPx == expected.a && motion.bPx == expected.b,
                  std::string(expected.lift) + " runs " + Px(motion.aPx) + " to " + Px(motion.bPx));
        CHECK_MSG(motion.atPx == expected.a && motion.destPx == expected.a && motion.originPx == expected.a &&
                      !motion.Moving() && motion.sends == 0,
                  std::string(expected.lift) + " waits on its marker a");
        CHECK_MSG(glm::distance(BodyPx(run, lift->entity), expected.a) < 1e-3,
                  std::string(expected.lift) + "'s node stands on its a");
        CHECK_MSG(motion.legSquared == glm::dot(expected.b - expected.a, expected.b - expected.a),
                  std::string(expected.lift) + "'s leg");
        CHECK_MSG(motion.speedPx == 60.0 && lift->sizeUnits == glm::dvec2(64.0, 32.0),
                  std::string(expected.lift) + " at 60 a second, touched inside 64 x 32");
    }
    // 2-02's node says 120; nothing reads it.
    Run run;
    if (Begin("level1a", run)) {
        const Tscn::Node* node = run.data.scene.FindNode("lift_789");
        double speed = 0.0;
        CHECK_MSG(node != nullptr && node->Meta("speed") != nullptr && node->Meta("speed")->AsNumber(speed) &&
                      speed == 120.0,
                  "2-02's lift_789 carries a speed of 120");
        const Mover::Lift* lift = run.level.movers.FindLift("lift_789");
        CHECK_MSG(lift != nullptr && lift->motion.speedPx == 60.0, "and runs at 60, as every lift does");
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

// A LIFT WAITS UNTIL A TOUCH SENDS IT (ETHCallback_lift; planning step 140): at rest
// on its marker a, its body still, for as long as nothing touches it; sent, at 60
// units a second to b, put on b exactly and at rest there; sent again, back to a.
void ALiftWaitsOnItsMarkerUntilSentAndStopsAtTheOther() {
    Run run;
    if (!Begin("level27", run) || run.level.movers.lifts.empty()) return;
    Mover::Lift& lift = run.level.movers.lifts.front();
    const glm::dvec2 a = lift.motion.aPx;
    const glm::dvec2 b = lift.motion.bPx;
    // 600 ticks untouched: on a, the body there and still.
    bool resting = true;
    for (int tick = 0; tick < 600; ++tick) {
        Tick(run);
        const glm::vec3 v = run.registry.get<RigidBodyComponent>(lift.entity).velocity;
        resting = resting && lift.motion.atPx == a && !lift.motion.Moving() &&
                  glm::distance(BodyPx(run, lift.entity), a) < 1e-3 && glm::length(v) == 0.0f;
    }
    CHECK_MSG(resting, "untouched, the lift stays on its marker a for 600 ticks, its body still");

    // Sent: toward b, from a.
    lift.motion.Send();
    CHECK_MSG(lift.motion.destPx == b && lift.motion.originPx == a && lift.motion.sends == 1 &&
                  glm::length(lift.motion.dir - glm::normalize(b - a)) < 1e-12,
              "sent from a, it goes to b, measured from a");
    const double legTicks = glm::distance(a, b) / 60.0 / kStep;
    int atB = -1;
    bool where = true;
    bool speed = true;
    for (int tick = 1; tick <= static_cast<int>(legTicks * 2.0) && atB < 0; ++tick) {
        const glm::dvec2 before = lift.motion.atPx;
        Tick(run);
        where = where && glm::distance(BodyPx(run, lift.entity), lift.motion.atPx) < 0.01;
        if (lift.motion.atPx == b) {
            atB = tick;
        } else {
            speed = speed && std::fabs(glm::distance(before, lift.motion.atPx) - 60.0 * kStep) < 1e-6;
        }
    }
    std::printf("  level27's lift: sent, on its marker b after %d ticks, for %.2f a leg at 60 units a second\n", atB,
                legTicks);
    CHECK_MSG(atB > 0 && std::fabs(atB - legTicks) <= 1.0, "on b when 60 units a second says");
    CHECK_MSG(speed, "each tick 60 x 1/60 units along its way");
    CHECK(where);
    // And it stays there: no turn, no second leg.
    bool stays = true;
    for (int tick = 0; tick < 300; ++tick) {
        Tick(run);
        stays = stays && lift.motion.atPx == b && !lift.motion.Moving() &&
                glm::distance(BodyPx(run, lift.entity), b) < 1e-3;
    }
    CHECK_MSG(stays, "at b it stays for 300 ticks");

    // Sent again: back to a, measured from b.
    lift.motion.Send();
    CHECK_MSG(lift.motion.destPx == a && lift.motion.originPx == b && lift.motion.sends == 2,
              "sent from b, it goes back to a, measured from b");
    int atA = -1;
    for (int tick = 1; tick <= static_cast<int>(legTicks * 2.0) && atA < 0; ++tick) {
        Tick(run);
        if (lift.motion.atPx == a) atA = tick;
    }
    CHECK_MSG(atA > 0 && std::fabs(atA - legTicks) <= 1.0, "on a a leg later: " + std::to_string(atA));
}

// sendLiftToOtherPoint's third branch: a touch on a lift that is on neither marker
// sends it back to a, measured from b, whichever way it was going - so a lift on
// its way up is turned round, and one on its way down goes on down.
void ATouchOnAMovingLiftSendsItBackToItsMarkerA() {
    Run run;
    if (!Begin("level27", run) || run.level.movers.lifts.empty()) return;
    Mover::Lift& lift = run.level.movers.lifts.front();
    const glm::dvec2 a = lift.motion.aPx;
    const glm::dvec2 b = lift.motion.bPx;
    lift.motion.Send();
    for (int tick = 0; tick < 40; ++tick) Tick(run);
    const glm::dvec2 midway = lift.motion.atPx;
    CHECK_MSG(lift.motion.Moving() && midway != a && midway != b, "40 ticks on it is between its markers");
    lift.motion.Send();
    CHECK_MSG(lift.motion.destPx == a && lift.motion.originPx == b &&
                  glm::length(lift.motion.dir - glm::normalize(a - midway)) < 1e-12,
              "a touch there turns it back to a, measured from b");
    int back = -1;
    for (int tick = 1; tick <= 200 && back < 0; ++tick) {
        Tick(run);
        if (lift.motion.atPx == a) back = tick;
    }
    const double ticks = glm::distance(midway, a) / 60.0 / kStep;
    CHECK_MSG(back > 0 && std::fabs(back - ticks) <= 1.0,
              "and puts it on a when it has come as far from b as a is: " + std::to_string(back));

    // On its way down from b, a touch changes nothing: it was going to a already.
    lift.motion.Send();
    for (int tick = 0; tick < 200 && lift.motion.Moving(); ++tick) Tick(run);
    CHECK(lift.motion.atPx == b);
    lift.motion.Send();
    for (int tick = 0; tick < 30; ++tick) Tick(run);
    const glm::dvec2 dir = lift.motion.dir;
    lift.motion.Send();
    CHECK_MSG(lift.motion.destPx == a && lift.motion.originPx == b && glm::length(lift.motion.dir - dir) < 1e-12,
              "on its way down to a, a touch sends it on to a");
    CHECK(lift.motion.sends == 5);
}

// The touch: isPointInRect on the lift's point and its picture's size, both edges
// in; every lift whose picture holds it is sent, and a touch anywhere else sends
// none.
void ATouchInsideALiftsPictureSendsIt() {
    Mover::Movers movers;
    Mover::Lift lift;
    lift.name = "lift_test";
    lift.sizeUnits = glm::dvec2(64.0, 32.0);
    lift.motion.aPx = glm::dvec2(100.0, 200.0);
    lift.motion.bPx = glm::dvec2(100.0, 100.0);
    lift.motion.atPx = lift.motion.aPx;
    lift.motion.destPx = lift.motion.aPx;
    lift.motion.originPx = lift.motion.aPx;
    lift.motion.legSquared = 100.0 * 100.0;
    lift.motion.speedPx = 60.0;
    movers.lifts.push_back(lift);
    const glm::dvec2 at = lift.motion.aPx;
    CHECK_MSG(movers.Press(at + glm::dvec2(-32.0, -16.0)) == 1, "the rect's corner is in");
    CHECK_MSG(movers.Press(at + glm::dvec2(32.0, 16.0)) == 1, "and its far corner");
    CHECK_MSG(movers.Press(at + glm::dvec2(32.001, 0.0)) == 0 && movers.Press(at + glm::dvec2(0.0, -16.001)) == 0,
              "a hair past either edge is out");
    CHECK_MSG(movers.lifts.front().motion.sends == 2, "two touches sent it, the misses none");
    // A second lift over the same point is sent by the same touch.
    Mover::Lift other = lift;
    other.name = "lift_other";
    movers.lifts.push_back(other);
    CHECK_MSG(movers.Press(at) == 2, "a touch over two lifts sends both");
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
    // The first POSITION the shot is sampled at within 0.7 x 32 = 22.4 px, short of the agent: the
    // static callback looks once a frame (planning step 149), so within one advance (3.33 px on a
    // bare tick) of the 22.4 and strictly inside it.
    const double killedAt = glm::distance(kill.shotPx, glm::dvec2(300.0, 200.0));
    CHECK_MSG(kill.agent == "agent" && kill.atPx == glm::dvec2(300.0, 200.0) && killedAt < 22.4 &&
                  killedAt >= 22.4 - 200.0 / 60.0 && kill.shotPx.x < 300.0,
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

// ---- The platform gate (planning step 136) ----------------------------------
//
// PortalManager::teleportToOther #36-42 teleports nothing while
// hasMovingPlatformInAnyPortal: an entity named moving_platform, lift or lift.ent
// within 0.4 x the portal's 14 of any placed portal's point, by EntityGrabber's box
// with its corner planes (transit.json `platform_gate`). Only the placed pair asks.

void TheGatesNumbersAreTheDecodedOnes() {
    Run run;
    if (!Begin("level5", run)) return;
    const Portals::Rules::GateRules& gate = run.data.portals.gate;
    CHECK(gate.entities == (std::vector<std::string>{"moving_platform", "lift", "lift.ent"}));
    CHECK(gate.radiusShare == 0.4);
    CHECK_NEAR(static_cast<float>(gate.radiusShare * run.data.portals.entryRadiusPx), 5.6f);

    // Each level's movers, by the names isMovingPlatform compares, at their own
    // collision box's half.
    struct Expected {
        const char* level;
        const char* name;
        glm::dvec2 half;
    };
    const Expected bodies[] = {
        {"level5", "moving_platform_764", {32.0, 16.0}},  {"level5a", "lift_ent_832", {31.0, 16.0}},
        {"level1a", "lift_789", {31.0, 16.0}},            {"level11c", "moving_platform_2251", {32.0, 32.0}},
        {"level11c", "moving_platform_2235", {32.0, 16.0}},
    };
    const std::size_t counts[] = {1, 1, 1, 2, 2};
    std::size_t index = 0;
    for (const Expected& expected : bodies) {
        const std::size_t count = counts[index++];
        Run level;
        if (!Begin(expected.level, level)) continue;
        const auto& found = level.level.portals.gateBodies;
        const auto body = std::find_if(found.begin(), found.end(),
                                       [&](const Portals::GateBody& one) { return one.name == expected.name; });
        CHECK_MSG(found.size() == count && body != found.end() && body->halfPx == expected.half &&
                      level.registry.valid(body->entity),
                  std::string(expected.level) + " gates on " + expected.name);
    }
    // Not moving_platform_single.ent, which isMovingPlatform does not name: level0's
    // moving_platform_single_ent_579 is no gate body.
    Run none;
    if (Begin("level0", none)) CHECK(none.level.portals.gateBodies.empty());
}

void TheGateReachesOverTheBoxWithItsCornersCut() {
    // A 64 x 32 box at the origin, a radius of 5.6: short of it by less than the radius
    // past a side, and past a corner only while a + b < 5.6 x sqrt 2 = 7.92.
    const glm::dvec2 centre(0.0);
    const glm::dvec2 half(32.0, 16.0);
    const double r = 5.6;
    struct Case {
        glm::dvec2 at;
        bool grabbed;
    };
    const Case cases[] = {
        {{0.0, 0.0}, true},           {{37.5, 0.0}, true},    {{37.7, 0.0}, false},  {{-37.5, 0.0}, true},
        {{0.0, 21.5}, true},          {{0.0, -21.7}, false},  {{35.9, 19.9}, true},  {{36.0, 20.0}, false},
        {{-35.9, -19.9}, true},       {{37.5, 18.5}, false},  {{37.5, 16.0}, true},  {{32.0, 21.5}, true},
    };
    for (const Case& one : cases) {
        CHECK_MSG(Portals::State::GrabsBox(centre, half, one.at, r) == one.grabbed,
                  "at " + Px(one.at) + (one.grabbed ? " grabbed" : " not grabbed"));
    }
}

void TheGateIsReadStrictly() {
    const std::filesystem::path directory = std::filesystem::temp_directory_path() / "supersonic-test-mp-movers";
    std::filesystem::create_directories(directory);
    const auto write = [&](const char* name, const std::string& text) {
        const std::filesystem::path path = directory / name;
        std::ofstream(path, std::ios::binary) << text;
        return path.string();
    };
    Portals::Rules rules;
    std::string error;
    CHECK_MSG(Portals::LoadGate(write("gate_good.json",
                                      R"({"platform_gate": {"entities": ["lift"], "radius_share": 0.5}})"),
                                rules, error),
              error);
    CHECK(rules.gate.entities == std::vector<std::string>{"lift"} && rules.gate.radiusShare == 0.5);
    const char* const bad[] = {
        R"({"transit": {}})",
        R"({"platform_gate": {"entities": [], "radius_share": 0.4}})",
        R"({"platform_gate": {"entities": ["lift"], "radius_share": 0}})",
        R"({"platform_gate": {"entities": ["lift", 3], "radius_share": 0.4}})",
        R"({"platform_gate": {"entities": ["lift"]}})",
    };
    for (const char* text : bad) {
        error.clear();
        CHECK_MSG(!Portals::LoadGate(write("gate_bad.json", text), rules, error) && !error.empty(),
                  std::string("refused: ") + text);
    }
}

// A bare State with level5's rules, a traveller of radius 4 and one gate body, 64 x 32,
// put wherever a case wants it.
struct GateRig {
    entt::registry registry;
    Portals::State state;
    entt::entity traveller = entt::null;
    entt::entity platform = entt::null;

    void Put(entt::entity entity, const glm::dvec2& atPx) {
        registry.get<TransformComponent>(entity).position = Units::ToWorld(atPx.x, atPx.y);
    }
    glm::dvec2 At(entt::entity entity) const {
        return Units::ToPixels(registry.get<TransformComponent>(entity).position);
    }
};

bool MakeGateRig(GateRig& rig) {
    Run run;
    if (!Begin("level5", run)) return false;
    rig.state.rules = run.data.portals;
    rig.state.shot = run.data.shot;
    rig.state.budget = 2;
    rig.traveller = rig.registry.create();
    rig.registry.emplace<TransformComponent>(rig.traveller);
    rig.registry.emplace<Supersonic::SphereColliderComponent>(rig.traveller).radius = Units::ToMetres(4.0);
    rig.registry.emplace<RigidBodyComponent>(rig.traveller);
    rig.state.travellers = {rig.traveller};
    rig.platform = rig.registry.create();
    rig.registry.emplace<TransformComponent>(rig.platform);
    rig.state.gateBodies = {Portals::GateBody{"platform", rig.platform, {32.0, 16.0}}};
    rig.Put(rig.traveller, {100.0, 100.0});
    rig.Put(rig.platform, {600.0, 600.0});
    return true;
}

void APlatformAtAPortalHoldsThePlacedPair() {
    GateRig rig;
    if (!MakeGateRig(rig)) return;
    Portals::State& portals = rig.state;
    CHECK(portals.TryPlace({100.0, 100.0}) && portals.TryPlace({200.0, 100.0}));
    // The platform's top 3 u under B's point: b = -13, grabbed whatever the hover.
    rig.Put(rig.platform, {200.0, 113.0});
    CHECK(portals.PlatformInAnyPortal(rig.registry));
    for (int tick = 0; tick < 5; ++tick) portals.Tick(rig.registry, kStep);
    CHECK_MSG(portals.traversals == 0 && portals.transitsHeld == 5 && portals.placed.size() == 2,
              "held on each of five ticks, and both ends still there: " + std::to_string(portals.transitsHeld));
    CHECK_MSG(glm::distance(rig.At(rig.traveller), glm::dvec2(100.0, 100.0)) < 1e-3, "the traveller unmoved");
    // Off the portal, 5.7 past its side at rest (hover 2 u either way is not enough).
    rig.Put(rig.platform, {200.0, 100.0 + 16.0 + 5.6 + 2.2});
    CHECK(!portals.PlatformInAnyPortal(rig.registry));
    portals.Tick(rig.registry, kStep);
    CHECK_MSG(portals.traversals == 1 && portals.transitsHeld == 5,
              "the traveller still in A goes through on the first tick the platform has gone");
    CHECK_MSG(glm::distance(rig.At(rig.traveller), glm::dvec2(200.0, 100.0)) < 2.5,
              "out of B: " + Px(rig.At(rig.traveller)));

    // ANY placed portal: the platform at A holds a traveller going in at B too.
    GateRig other;
    if (!MakeGateRig(other)) return;
    CHECK(other.state.TryPlace({300.0, 100.0}) && other.state.TryPlace({100.0, 100.0}));
    other.Put(other.platform, {300.0, 85.0});
    other.state.Tick(other.registry, kStep);
    CHECK_MSG(other.state.traversals == 0 && other.state.transitsHeld == 1, "a platform at the other end holds it");
}

void AStaticPairIsNeverHeld() {
    GateRig rig;
    if (!MakeGateRig(rig)) return;
    Portals::State& portals = rig.state;
    const auto box = [](const glm::dvec2& atPx) {
        Trigger::Box out;
        const glm::vec3 centre = Units::ToWorld(atPx.x, atPx.y);
        out.centre = glm::vec2(centre.x, centre.y);
        out.half = glm::vec2(Units::ToMetres(8.0));
        return out;
    };
    Portals::Static from;
    from.name = "static_0";
    from.index = 0;
    from.destiny = 1;
    from.hasDestiny = true;
    from.atPx = {100.0, 100.0};
    from.trigger = box(from.atPx);
    Portals::Static to = from;
    to.name = "static_1";
    to.index = 1;
    to.destiny = 0;
    to.atPx = {200.0, 100.0};
    to.trigger = box(to.atPx);
    portals.statics = {from, to};
    // A placed portal with the platform over it: the gate is up for a placed pair.
    CHECK(portals.TryPlace({400.0, 100.0}));
    rig.Put(rig.platform, {400.0, 100.0});
    CHECK(portals.PlatformInAnyPortal(rig.registry));
    portals.Tick(rig.registry, kStep);
    CHECK_MSG(portals.traversals == 1 && portals.transitsHeld == 0,
              "ETHCallback_portal_static never asks: through the static pair");
    CHECK(glm::distance(rig.At(rig.traveller), glm::dvec2(200.0, 100.0)) < 1e-3);
}

void AFireballIsHeldAsATravellerIs() {
    GateRig rig;
    if (!MakeGateRig(rig)) return;
    Portals::State& portals = rig.state;
    rig.Put(rig.traveller, {600.0, 100.0});
    CHECK(portals.TryPlace({100.0, 100.0}) && portals.TryPlace({200.0, 100.0}));
    rig.Put(rig.platform, {100.0, 80.0});
    const glm::dvec2 ballPx(110.0, 100.0); // 10 u from A, inside its 8 + 14
    CHECK_MSG(!portals.CarryFireball(rig.registry, ballPx, 8.0), "held while the platform is at A");
    CHECK(portals.transitsHeld == 1 && portals.traversals == 0);
    rig.Put(rig.platform, {600.0, 600.0});
    const auto exit = portals.CarryFireball(rig.registry, ballPx, 8.0);
    CHECK_MSG(exit && *exit == portals.placed[1].atPx, "and carried to B once it has gone");
    CHECK(portals.transitsHeld == 1 && portals.traversals == 1);
}

void Level5sPlatformHoldsThePlayerUntilItSwingsAway() {
    // level5's moving_platform_764 swings y 144 + 32 sin(2 x 0.7 t) (the port's swing,
    // movers.json; R7 is not built). A portal at (416, 100), which its box comes within
    // 5.6 of while its centre is above y 121.6, and one on the player at the spawn.
    Run run;
    if (!Begin("level5", run) || run.level.movers.platforms.empty()) return;
    Portals::State& portals = run.level.portals;
    const Mover::Platform& platform = run.level.movers.platforms.front();
    const glm::dvec2 aPx(416.0, 100.0);
    const double radius = portals.rules.gate.radiusShare * portals.rules.entryRadiusPx;
    // Until the gate would be up at A: the platform near the top of its swing.
    int tick = 0;
    for (; tick < 400; ++tick) {
        Tick(run);
        if (tick > 30 && Portals::State::GrabsBox(BodyPx(run, platform.entity), {32.0, 16.0}, aPx, radius - 2.5)) break;
    }
    const glm::dvec2 standPx = BodyPx(run, run.level.player);
    CHECK_MSG(portals.TryPlace(aPx) && portals.TryPlace(standPx), "a pair: A over the swing, B on the player");
    if (portals.placed.size() != 2) return;
    const int idA = portals.placed[0].id;
    CHECK(portals.PlatformInAnyPortal(run.registry));
    int held = 0;
    int transitTick = -1;
    bool gateUpWhileHeld = true;
    glm::dvec2 lastAPx = portals.placed[0].atPx; // A where the last tick left it, hover and all
    for (int more = 0; more < 200 && transitTick < 0; ++more) {
        const int before = portals.transitsHeld;
        Tick(run);
        if (portals.traversals > 0) {
            transitTick = tick + 1 + more;
            break;
        }
        if (portals.transitsHeld > before) {
            ++held;
            gateUpWhileHeld = gateUpWhileHeld && portals.PlatformInAnyPortal(run.registry);
        }
        for (const Portals::Placed& portal : portals.placed) {
            if (portal.id == idA) lastAPx = portal.atPx;
        }
    }
    const glm::dvec2 platformPx = BodyPx(run, platform.entity);
    std::printf("  level5: A (416, 100), B on the player at %s; held %d tick(s), through on tick %d with the "
                "platform's centre at y %.2f\n",
                Px(standPx).c_str(), held, transitTick, platformPx.y);
    CHECK_MSG(held > 10 && gateUpWhileHeld, "held while the platform's box is within 5.6 of A");
    CHECK_MSG(transitTick > 0, "and through once it has swung away");
    // A's hover moves it hundredths of a unit a tick and the platform under 0.75.
    CHECK_MSG(!Portals::State::GrabsBox(platformPx, {32.0, 16.0}, lastAPx, radius - 0.05) &&
                  Portals::State::GrabsBox(platformPx, {32.0, 16.0}, lastAPx, radius + 1.0),
              "on the first tick its box is no longer within 5.6 of A");
    CHECK_MSG(glm::distance(BodyPx(run, run.level.player), aPx) < 5.0, "the player out of A");
}

void runTests() {
    MoversJsonCarriesTheSwingsGuessAndTheDecodedLift();
    ChapterOnesMovers();
    TheFourteenLiftsWaitOnTheirMarkers();
    APlatformSwingsByItsStride();
    ALiftWaitsOnItsMarkerUntilSentAndStopsAtTheOther();
    ATouchOnAMovingLiftSendsItBackToItsMarkerA();
    ATouchInsideALiftsPictureSendsIt();
    APatrollingAgentSwingsAndRefusesNothing();
    TheAgentsNumbersAreTheDecodedOnes();
    AnAgentKillsAShotStillInFlightAndItsRing();
    AnAgentRefusesNothingAndLetsAWideShotBy();
    TheRingIsSoughtInTheAgentsBucketThenItsNeighbours();
    F17sShotKillsTheAgentOn401();
    F3sTapsBracketTheFieldOn107();
    TheGatesNumbersAreTheDecodedOnes();
    TheGateReachesOverTheBoxWithItsCornersCut();
    TheGateIsReadStrictly();
    APlatformAtAPortalHoldsThePlacedPair();
    AStaticPairIsNeverHeld();
    AFireballIsHeldAsATravellerIs();
    Level5sPlatformHoldsThePlayerUntilItSwingsAway();
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
