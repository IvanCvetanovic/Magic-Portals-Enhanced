// The portal shot: a tap fires, and the portal opens where the shot arrives.
//
// What the owner said on 11 September is tested as it stands:
//   - the shot flies from the character toward the tap, and the portal opens
//     where it arrives;
//   - a wall on the way stops it, and so does a crate or a stone;
//   - a failed shot costs nothing.
// What the level files say is pinned: level6's projectile blocker and its box.
// The flight is the original's, decoded (shot.json `flight`, planning step 149):
// TheShotFliesAsTheOriginalsProjectileDoes pins its ticks over stated distances,
// its jump, its advance and its clock, and what an agent and a flame see of it.
// The segment test is checked on its own against a turned box, a hull's polygon
// and a circle, where a test by bounds would be wrong.
//
// Reads the converted levels and Enhanced's data from outside this repository,
// and skips, saying where it looked, when either is absent.

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "core/Components.hpp"
#include "sim/Game.hpp"
#include "sim/LevelBuilder.hpp"
#include "sim/ScriptClock.hpp"
#include "sim/Shot.hpp"
#include "sim/Units.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <span>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

using namespace MagicPortals;
using Supersonic::BoxColliderComponent;
using Supersonic::ConvexHullColliderComponent;
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

bool Begin(const std::string& levelName, Run& run) {
    std::string error;
    const bool ok = Game::LoadData(kLevels + "/" + levelName + ".tscn", kData,
                                   std::filesystem::temp_directory_path() / "supersonic-test-mp-shot", run.data,
                                   error) &&
                    Game::Start(run.data, run.registry, run.level, error);
    CHECK_MSG(ok, levelName + ": " + error);

    // THE FIRING RATE OFF, on purpose, for this whole suite.
    //
    // The original refuses a tap for the first 300 ms of a level and for 400 ms
    // after the last one it took (placement.json's cooldowns), and both of its
    // clocks start at zero - so a tap on a freshly built level is refused. That
    // is real, and ThePortalCooldownsHoldATapOff in test_mp_play is what pins
    // it.
    //
    // This suite is about the shot's GEOMETRY: what stops it, where it lands,
    // what it costs. Every case here fires the moment the level is built, and
    // making each one wait out 400 ms would be twelve sleeps testing nothing
    // this file is for. Turned off here rather than worked around silently.
    // BOTH COPIES. Game::Start takes the Level's rules from the Data's, so the
    // one on the level is what an in-level case plays by - and MakeRig below
    // builds its own State straight from run.data.portals, which zeroing only
    // the level's would leave at the real 300/400. Zeroed here rather than in
    // MakeRig so there is no second place to remember.
    run.level.portals.rules.firstPortalMinMs = 0.0;
    run.level.portals.rules.nextPortalMinMs = 0.0;
    run.data.portals.firstPortalMinMs = 0.0;
    run.data.portals.nextPortalMinMs = 0.0;
    return ok;
}

void Tick(Run& run) { Game::Tick(run.data, run.registry, run.level, 0.0f, kStep); }

void PutPlayer(Run& run, const glm::dvec2& atPx) {
    auto& transform = run.registry.get<TransformComponent>(run.level.player);
    const glm::vec3 at = Units::ToWorld(atPx.x, atPx.y);
    transform.position = glm::vec3(at.x, at.y, transform.position.z);
    run.registry.get<RigidBodyComponent>(run.level.player).velocity = glm::vec3(0.0f);
}

glm::dvec2 PlayerPx(const Run& run) {
    return Units::ToPixels(run.registry.get<TransformComponent>(run.level.player).position);
}

std::string Px(const glm::dvec2& at) {
    char text[48];
    std::snprintf(text, sizeof text, "(%.1f, %.1f)", at.x, at.y);
    return text;
}

// How many Ticks the decoded flight takes over `distance`, counted from the Shoot that fires it
// (planning step 149): ETHCallback_projectile jumps 24 on its first callback, which is the first Tick
// after the tap in the layer's order, advances 200 x min(200, ms) / 1000 on each, and arrives on the
// first callback n with 24 + n x advance STRICTLY above the distance. A distance that is an exact
// multiple is not asked of it: the original is in float32.
int FlightTicks(const Shot::Rules& rules, double distance, double scriptMs) {
    const double advance = rules.speedUnits * std::min(rules.frameCapMs, scriptMs) / 1000.0;
    int callbacks = 1;
    while (rules.jumpUnits + callbacks * advance <= distance) ++callbacks;
    return callbacks;
}

// Ticks until the shot in flight lands or fails. Returns how many it took.
int Fly(Run& run) {
    int ticks = 0;
    while (run.level.portals.flight && ticks < 600) {
        Tick(run);
        ++ticks;
    }
    return ticks;
}

// ---- What the levels say ------------------------------------------------------

void Level6sBlocker() {
    // Read off level6.tscn with grep: one anti_projectile_wall.ent, with a
    // trigger_size of 64 x 128 and no offset.
    Run run;
    if (!Begin("level6", run)) return;
    const auto& blockers = run.level.portals.blockers;
    CHECK_EQ(blockers.size(), std::size_t{1});
    if (blockers.size() != 1) return;
    CHECK_MSG(blockers[0].name == "anti_projectile_wall_ent_743", blockers[0].name);
    const glm::dvec2 centre = Units::ToPixels(glm::vec3(blockers[0].box.centre, 0.0f));
    const glm::dvec2 size = glm::dvec2(blockers[0].box.half) * 2.0 * Units::kPixelsPerMetre;
    CHECK_MSG(glm::distance(centre, glm::dvec2(313.0, 185.0)) < 1e-3 && std::fabs(size.x - 64.0) < 1e-3 &&
                  std::fabs(size.y - 128.0) < 1e-3,
              Px(size) + " at " + Px(centre));
}

// ---- Flying -------------------------------------------------------------------

void AShotOpensAPortalWhereItArrives() {
    // level1: straight up from the spawn, into open air.
    Run run;
    if (!Begin("level1", run)) return;
    const glm::dvec2 from = PlayerPx(run);
    const glm::dvec2 aim = from + glm::dvec2(0.0, -48.0);
    CHECK(run.level.portals.Shoot(run.registry, aim));
    CHECK(run.level.portals.flight.has_value());
    CHECK_EQ(run.level.portals.shotsFired, 1);
    // Nothing opens at the tap; the shot has to get there.
    CHECK(run.level.portals.placed.empty());
    CHECK_EQ(run.level.portals.portalsUsed, 0);
    // One at a time.
    CHECK(!run.level.portals.Shoot(run.registry, aim));
    CHECK_EQ(run.level.portals.shotsFired, 1);

    const int ticks = Fly(run);
    // The decoded flight on this suite's bare tick (16.667 ms): 24 + 3.333 n > 48.
    const int expected = FlightTicks(run.data.shot, 48.0, static_cast<double>(kStep) * 1000.0);
    std::printf("  level1: a shot 48 px up landed after %d tick(s); the decoded flight says %d\n", ticks, expected);
    CHECK_MSG(ticks == expected, "landed after " + std::to_string(ticks) + " ticks");
    CHECK_MSG(run.level.portals.placed.size() == 1, "failed on " + run.level.portals.lastFailure);
    if (run.level.portals.placed.size() != 1) return;
    CHECK(glm::distance(run.level.portals.placed[0].atPx, aim) < 1e-9);
    CHECK_EQ(run.level.portals.portalsUsed, 1);
    CHECK_EQ(run.level.portals.shotsFailed, 0);
}

void AWallStopsIt() {
    // level1: straight down from the spawn, through the floor it stands on.
    Run run;
    if (!Begin("level1", run)) return;
    for (int tick = 0; tick < 60; ++tick) Tick(run);
    const glm::dvec2 from = PlayerPx(run);
    CHECK(run.level.portals.Shoot(run.registry, from + glm::dvec2(0.0, 120.0)));
    Fly(run);
    std::printf("  level1: a shot down through the floor stopped at %s\n", run.level.portals.lastFailure.c_str());
    CHECK(run.level.portals.placed.empty());
    CHECK_EQ(run.level.portals.shotsFailed, 1);
    CHECK_MSG(!run.level.portals.lastFailure.empty() && run.level.portals.lastFailure != "the tap, where no portal may open",
              run.level.portals.lastFailure);
    // It cost nothing: the level's one portal is still there to be had.
    CHECK_EQ(run.level.portals.portalsUsed, 0);
    CHECK(run.level.portals.Shoot(run.registry, PlayerPx(run) + glm::dvec2(0.0, -48.0)));
    Fly(run);
    CHECK_MSG(run.level.portals.placed.size() == 1, "the second failed on " + run.level.portals.lastFailure);
}

void AStoneStopsIt() {
    // level8: from the spawn at the hint arrow behind the stone. The stone,
    // rolling_stone_ent_597, is in the way.
    Run run;
    if (!Begin("level8", run)) return;
    CHECK(run.level.portals.Shoot(run.registry, glm::dvec2(30.0, 106.0)));
    Fly(run);
    CHECK(run.level.portals.placed.empty());
    CHECK_MSG(run.level.portals.lastFailure == "rolling_stone_ent_597", run.level.portals.lastFailure);
    CHECK_EQ(run.level.portals.portalsUsed, 0);
}

void TheBlockerStopsIt() {
    // level6: a shot level across anti_projectile_wall_ent_743 (x 281 to 345)
    // dies in it. With the blocker taken away the same shot lands, so it was the
    // blocker.
    for (const bool withBlocker : {true, false}) {
        Run run;
        if (!Begin("level6", run)) return;
        if (!withBlocker) run.level.portals.blockers.clear();
        PutPlayer(run, glm::dvec2(262.0, 150.0));
        CHECK(run.level.portals.Shoot(run.registry, glm::dvec2(380.0, 150.0)));
        Fly(run);
        const bool landed = run.level.portals.placed.size() == 1;
        CHECK_MSG(landed != withBlocker, std::string(withBlocker ? "with" : "without") + " the blocker: " +
                                             (landed ? "landed" : "stopped at " + run.level.portals.lastFailure));
        if (withBlocker) {
            CHECK_MSG(run.level.portals.lastFailure == "anti_projectile_wall_ent_743", run.level.portals.lastFailure);
        }
    }
}

void ANoPortalZoneAtTheTapFailsIt() {
    // level6's antiportal_687 at (288, 192): a shot down into it, from above and
    // with the blocker out of the way, arrives and opens nothing.
    Run run;
    if (!Begin("level6", run)) return;
    run.level.portals.blockers.clear();
    PutPlayer(run, glm::dvec2(288.0, 150.0));
    CHECK(run.level.portals.Shoot(run.registry, glm::dvec2(288.0, 192.0)));
    Fly(run);
    CHECK(run.level.portals.placed.empty());
    CHECK_EQ(run.level.portals.shotsFailed, 1);
    CHECK_MSG(run.level.portals.lastFailure == "the tap, where no portal may open", run.level.portals.lastFailure);
    CHECK_EQ(run.level.portals.portalsUsed, 0);
}

void NoPortalsNoShot() {
    // level0 grants no placement: a tap fires nothing.
    Run run;
    if (!Begin("level0", run)) return;
    CHECK(!run.level.portals.Shoot(run.registry, PlayerPx(run) + glm::dvec2(0.0, -48.0)));
    CHECK_EQ(run.level.portals.shotsFired, 0);
    CHECK(!run.level.portals.flight);
}

// ---- The segment test, on its own ---------------------------------------------

void ExactInThePlane() {
    // A 100 px square turned by 45 degrees, centred at the origin: a diamond
    // whose corners are 70.7 px out along the axes. Its bounds reach 70.7 px in
    // x and y, so a segment near (60, 60) lies inside them and outside it.
    entt::registry registry;
    const entt::entity square = registry.create();
    auto& transform = registry.emplace<TransformComponent>(square);
    transform.rotation.z = 0.785398163f;
    registry.emplace<BoxColliderComponent>(square).size = glm::vec3(2.0f, 2.0f, 2.0f);

    const auto hit = [&registry](const glm::dvec2& from, const glm::dvec2& to, entt::entity ignore = entt::null) {
        return Shot::FirstBody(registry, from, to, ignore);
    };
    CHECK_MSG(!hit({65.0, -65.0}, {50.0, -50.0}), "inside the diamond's bounds, outside the diamond: no hit");
    const auto into = hit({65.0, -65.0}, {30.0, -30.0});
    CHECK(into.has_value() && into->body == square);
    // |x| + |y| reaches 70.71 at x = 35.36, which is 29.64 px of the 35 along.
    if (into) CHECK_MSG(std::fabs(into->along - (65.0 - 35.3553) / 35.0) < 1e-4, std::to_string(into->along));
    CHECK_MSG(!hit({65.0, -65.0}, {30.0, -30.0}, square), "what the shot leaves from is not met");

    // A trigger stops nothing.
    registry.get<BoxColliderComponent>(square).isTrigger = true;
    CHECK(!hit({65.0, -65.0}, {0.0, 0.0}));

    // A hull, by the polygon LevelBuilder records for it: a triangle, (0, 0),
    // (100, 0) and (0, 100) in pixels, at (200, 0).
    const entt::entity triangle = registry.create();
    registry.emplace<TransformComponent>(triangle).position = Units::ToWorld(200.0, 0.0);
    registry.emplace<ConvexHullColliderComponent>(triangle);
    auto& outline = registry.emplace<LevelBuilder::PlanePolygon>(triangle);
    outline.points = {glm::vec2(0.0f, 0.0f), glm::vec2(2.0f, 0.0f), glm::vec2(0.0f, -2.0f)};
    CHECK_MSG(!hit({280.0, 80.0}, {260.0, 60.0}), "past the triangle's long side: no hit");
    const auto side = hit({280.0, 80.0}, {220.0, 20.0});
    CHECK(side.has_value() && side->body == triangle);
    // The long side is x + y = 300 (in pixels from the triangle's corner at 200):
    // (280, 80) - t(60, 60) meets x - 200 + y = 100 at t = 60/120.
    if (side) CHECK_MSG(std::fabs(side->along - 0.5) < 1e-4, std::to_string(side->along));

    // A circle of 30 px at (0, 200): a segment passing 31 px from its centre
    // misses, one passing through it meets it where it first reaches 30 px.
    const entt::entity ball = registry.create();
    registry.emplace<TransformComponent>(ball).position = Units::ToWorld(0.0, 200.0);
    registry.emplace<SphereColliderComponent>(ball).radius = Units::ToMetres(30.0);
    CHECK(!hit({-100.0, 231.0}, {100.0, 231.0}));
    const auto through = hit({-100.0, 200.0}, {100.0, 200.0});
    CHECK(through.has_value() && through->body == ball);
    if (through) CHECK_MSG(std::fabs(through->along - 70.0 / 200.0) < 1e-4, std::to_string(through->along));
}

// ---- Reflectors ------------------------------------------------------------------

std::string Where(const glm::dvec2& p) { return "(" + std::to_string(p.x) + ", " + std::to_string(p.y) + ")"; }

void TheReflectorsAreTheLevelsOwn() {
    // Pinned to the files: level 1-12's three, all horizontal, and 1-13's one,
    // vertical.
    Run eleven;
    if (Begin("level11", eleven)) {
        const std::vector<Portals::Reflector>& reflectors = eleven.level.portals.reflectors;
        CHECK_EQ(reflectors.size(), std::size_t{3});
        struct Expected {
            const char* name;
            double x;
            double y;
        };
        bool pinned = reflectors.size() == 3;
        for (const Expected& e : {Expected{"reflect_agent_675", 638.0, 26.0}, Expected{"reflect_agent_761", 180.0, 140.0},
                                  Expected{"reflect_agent_623", 422.0, 115.0}}) {
            bool found = false;
            for (const Portals::Reflector& r : reflectors) {
                if (r.name == e.name && r.centrePx == glm::dvec2(e.x, e.y) && !r.vertical) found = true;
            }
            if (!found) pinned = false;
        }
        CHECK_MSG(pinned, "level11's three reflectors, horizontal, where the file puts them");
    }
    Run twelve;
    if (Begin("level12", twelve)) {
        const std::vector<Portals::Reflector>& reflectors = twelve.level.portals.reflectors;
        CHECK_MSG(reflectors.size() == 1 && reflectors[0].name == "reflect_agent_584" &&
                      reflectors[0].centrePx == glm::dvec2(248.0, 149.0) && reflectors[0].vertical,
                  "level12's one, vertical");
    }
}

// A shot in an empty plane from (100, 200), with the rules a level loads and the
// reflectors a test puts down.
struct Rig {
    entt::registry registry;
    Portals::State state;
};

bool MakeRig(Rig& rig) {
    Run run;
    if (!Begin("level1", run)) return false;
    rig.state.rules = run.data.portals;
    rig.state.shot = run.data.shot;
    rig.state.budget = 2;
    rig.state.shooter = rig.registry.create();
    rig.registry.emplace<TransformComponent>(rig.state.shooter).position = Units::ToWorld(100.0, 200.0);
    return true;
}

void FlyOut(Rig& rig) {
    for (int tick = 0; tick < 600 && rig.state.flight; ++tick) rig.state.Tick(rig.registry, kStep);
}

void AReflectorTurnsAShotMirrored() {
    // A horizontal reflector at (200, 100), on the line from (100, 200) to a tap
    // at (300, 0). The shot comes within the catch radius at E, turns its y back,
    // and goes on for the rest of its way: the portal opens at the tap mirrored
    // across the line through E, at the end of a way as long as the one fired.
    Rig rig;
    if (!MakeRig(rig)) return;
    rig.state.shot.maxReflections = 1;
    rig.state.reflectors.push_back({"mirror", glm::dvec2(200.0, 100.0), false});
    CHECK(rig.state.Shoot(rig.registry, glm::dvec2(300.0, 0.0)));
    FlyOut(rig);
    const double k = rig.state.shot.reflectRadiusPx / std::sqrt(2.0);
    const glm::dvec2 entry(200.0 - k, 100.0 + k);
    const glm::dvec2 expected(300.0, 2.0 * entry.y);
    CHECK_EQ(rig.state.reflections, 1);
    CHECK_MSG(rig.state.placed.size() == 1, "it opened a portal: " + rig.state.lastFailure);
    if (rig.state.placed.empty()) return;
    const glm::dvec2 at = rig.state.placed[0].atPx;
    CHECK_MSG(glm::distance(at, expected) < 1e-6, Where(at) + " for " + Where(expected));
    const double way = glm::distance(glm::dvec2(100.0, 200.0), entry) + glm::distance(entry, at);
    const double fired = glm::distance(glm::dvec2(100.0, 200.0), glm::dvec2(300.0, 0.0));
    CHECK_MSG(std::fabs(way - fired) < 1e-6, "a way of " + std::to_string(way) + " for " + std::to_string(fired));

    // Without the reflector, the same tap opens at the tap.
    Rig bare;
    if (!MakeRig(bare)) return;
    CHECK(bare.state.Shoot(bare.registry, glm::dvec2(300.0, 0.0)));
    FlyOut(bare);
    CHECK_MSG(bare.state.placed.size() == 1 && glm::distance(bare.state.placed[0].atPx, glm::dvec2(300.0, 0.0)) < 1e-6,
              "no reflector, no turn");
}

void AShotComesOffOneReflectorAtMost() {
    // Two vertical reflectors on one line, at x 200 and 60, and a shot from 100
    // toward 400. The first turns it back toward the second. Allowed one
    // reflection - shot.json's reading of hasBeenReflected - it passes the second
    // and opens at x -32. Allowed two, the second turns it again, and it opens at
    // x 184, where its way runs out.
    for (const int allowed : {1, 2}) {
        Rig rig;
        if (!MakeRig(rig)) return;
        rig.state.shot.reflectRadiusPx = 16.0;
        rig.state.shot.maxReflections = allowed;
        rig.state.reflectors.push_back({"first", glm::dvec2(200.0, 200.0), true});
        rig.state.reflectors.push_back({"second", glm::dvec2(60.0, 200.0), true});
        CHECK(rig.state.Shoot(rig.registry, glm::dvec2(400.0, 200.0)));
        FlyOut(rig);
        CHECK_EQ(rig.state.reflections, allowed);
        const double expectedX = allowed == 1 ? -32.0 : 184.0;
        const bool landed = rig.state.placed.size() == 1 &&
                            glm::distance(rig.state.placed[0].atPx, glm::dvec2(expectedX, 200.0)) < 1e-6;
        CHECK_MSG(landed, std::to_string(allowed) + " allowed: " +
                              (rig.state.placed.empty() ? rig.state.lastFailure : Where(rig.state.placed[0].atPx)));
    }
    // shot.json allows one, as its note says; the test above holds either way.
    Rig rig;
    if (MakeRig(rig)) CHECK(rig.state.shot.maxReflections >= 0 && rig.state.shot.reflectRadiusPx > 0.0);
}

void AReflectorWithNoPlaneTakesTheDefault() {
    // level26a's reflect_agent_ent_961 is the one placement in the game that
    // gives no plane. The level starts, and the reflector takes shot.json's
    // default, whichever way it is set.
    for (const bool vertical : {true, false}) {
        Run run;
        std::string error;
        bool ok = Game::LoadData(kLevels + "/level26a.tscn", kData,
                                 std::filesystem::temp_directory_path() / "supersonic-test-mp-shot", run.data, error);
        if (ok) {
            run.data.shot.defaultPlaneVertical = vertical;
            ok = Game::Start(run.data, run.registry, run.level, error);
        }
        CHECK_MSG(ok, "level26a: " + error);
        if (!ok) continue;
        const Portals::Reflector* found = nullptr;
        for (const Portals::Reflector& r : run.level.portals.reflectors) {
            if (r.name == "reflect_agent_ent_961") found = &r;
        }
        CHECK_MSG(found != nullptr && found->vertical == vertical,
                  std::string("the default, ") + (vertical ? "vertical" : "horizontal"));
    }
}

// ---- What the original works out at the tap (the visuals plan's 10.1) -----------
//
// computePortalFinalPos, run on the port's world for the PICTURES a tap leaves
// (placement.json final_pos; the owner's standing ruling R15): whether the shot
// is already doomed - red_cross rather than portal_mark - and whether red_waves
// (A) goes at the aim. Pinned on 1-07 against footage F3's own taps, where the
// port's flight agrees, and on a bare rig for the retry, where it cannot.

std::string Named(entt::registry& registry, entt::entity body) {
    if (body == entt::null) return "nothing";
    const auto* tag = registry.try_get<Supersonic::TagComponent>(body);
    return tag != nullptr ? tag->tag : std::string("a body");
}

void TheFinalPosRulesAreTheDecodedOnes() {
    Run run;
    if (!Begin("level6", run)) return;
    const Portals::Rules::FinalPosRules& rules = run.data.portals.finalPos;
    CHECK_EQ(rules.rayStartUnits, 8.0);
    CHECK_EQ(rules.pushUnits, 10.0);
    CHECK_MSG(rules.retryFromUnits == glm::dvec2(0.0, -30.0), Px(rules.retryFromUnits));
    CHECK_EQ(rules.retryBackUnits, 6.0);
    const std::vector<std::string> ignores{"char_feet.ent",   "dark_mage.ent", "death_area.ent",
                                           "anti_sight_wall.ent", "shock_agent", "shock_agent.ent",
                                           "fireball.ent",    "gravity_agent", "gravity_agent.ent"};
    CHECK_MSG(rules.rayIgnores == ignores, "GetClosestContact's ignore list, one name each, as the listing spells it");
    CHECK_MSG(run.data.portals.noPortalSignEntity == "no_portal_sign.ent", run.data.portals.noPortalSignEntity);
    // 1-07 places none of the ignored and no sign; 1-15 places the sign.
    CHECK(run.level.portals.rayIgnored.empty());
    CHECK(!run.level.portals.noPortalSign);
    Run fifteen;
    if (Begin("level14", fifteen)) {
        CHECK_MSG(fifteen.level.portals.noPortalSign, "level14 (1-15) places no_portal_sign.ent");
        CHECK_EQ(fifteen.level.portals.budget, 0);
    }
}

void TheTapTimeRayOnF3sTaps() {
    // level6 (1-07), the player where the level puts it, and footage F3's taps
    // in level units (its screen px / 2.8125, the camera at 0): the wall, the
    // long shot the blocker kills, the port's own in-field tap and the outside
    // one. Each is then flown by the port, and the end it records is compared.
    struct Case {
        const char* what;
        glm::dvec2 tapPx;
        bool willDie;
        bool redWaves;
        bool meets;                   // the tap-time ray meets a body (the one the flight meets)
        Portals::FailKind ends;       // how the port's own flight ends
        bool opens;                   // or it opens a portal
        bool endsInField;
    };
    const Case cases[] = {
        {"the wall, F3 (450, 315)", glm::dvec2(160.0, 112.0), true, false, true,
         Portals::FailKind::Body, false, false},
        {"the long shot, F3 (1100, 450)", glm::dvec2(391.111, 160.0), false, false, false,
         Portals::FailKind::Blocker, false, true},
        {"in the port's field, 28 u from antiportal_687", glm::dvec2(259.911, 194.844), true, true, false,
         Portals::FailKind::Refused, false, true},
        {"outside it, F3 (537, 540)", glm::dvec2(190.933, 192.0), false, false, false, Portals::FailKind::Body,
         true, false},
    };
    for (const Case& c : cases) {
        Run run;
        if (!Begin("level6", run)) return;
        for (int tick = 0; tick < 30; ++tick) Tick(run); // the player settles on the floor
        const glm::dvec2 from = PlayerPx(run);
        const Portals::FinalPos said = run.level.portals.Predict(run.registry, from, c.tapPx);
        CHECK_MSG(said.willDie == c.willDie && said.redWaves == c.redWaves && (said.closest != entt::null) == c.meets,
                  std::string(c.what) + ": willDie " + (said.willDie ? "yes" : "no") + ", waves " +
                      (said.redWaves ? "yes" : "no") + ", met " + Named(run.registry, said.closest));
        CHECK_MSG(!said.retried || c.willDie, std::string(c.what) + ": a clear shot never retries");
        CHECK(run.level.portals.Shoot(run.registry, c.tapPx));
        CHECK(run.level.portals.lastTap.reason == Portals::TapReason::Fired);
        // The port's own shot, unchanged by any of this: what it met, where.
        std::vector<Portals::Failure> ends;
        std::vector<Portals::Launch> launches;
        for (int tick = 0; tick < 600 && run.level.portals.flight; ++tick) {
            Tick(run);
            for (const Portals::Failure& f : run.level.portals.failures) ends.push_back(f);
            for (const Portals::Launch& l : run.level.portals.launches) launches.push_back(l);
        }
        CHECK_MSG(launches.size() == 1 && launches[0].fromPx == from && launches[0].towardPx == c.tapPx,
                  std::string(c.what) + ": one launch, on the shot's first tick, from where it left: " +
                      std::to_string(launches.size()));
        if (c.opens) {
            CHECK_MSG(ends.empty() && run.level.portals.placed.size() == 1,
                      std::string(c.what) + " opens a portal: " + run.level.portals.lastFailure);
            continue;
        }
        CHECK_MSG(ends.size() == 1, std::string(c.what) + ": one end recorded, " + std::to_string(ends.size()));
        if (ends.size() != 1) continue;
        CHECK_MSG(ends[0].kind == c.ends && ends[0].inField == c.endsInField,
                  std::string(c.what) + ": ended at " + Px(ends[0].atPx) + " by " + run.level.portals.lastFailure +
                      (ends[0].inField ? ", in a field" : ""));
        std::printf("  1-07 %s: %s at the tap, %s; the port's shot ends at %s (%s)\n", c.what,
                    said.willDie ? "red_cross" : "portal_mark", said.redWaves ? "red_waves (A)" : "no waves",
                    Px(ends[0].atPx).c_str(), run.level.portals.lastFailure.c_str());
        if (c.ends == Portals::FailKind::Body) {
            // The body the port's flight met is the one the tap-time ray met, and
            // the tap-time ray's contact is on the same line a little further on:
            // it starts 8 units out and the flight at the centre.
            CHECK_MSG(run.level.portals.lastFailure == Named(run.registry, said.closest),
                      "the ray met " + Named(run.registry, said.closest) + ", the flight " +
                          run.level.portals.lastFailure);
            CHECK_MSG(glm::distance(ends[0].atPx, said.atPx) < 1.0,
                      "the flight's contact " + Px(ends[0].atPx) + " is the ray's " + Px(said.atPx));
        }
        if (c.ends == Portals::FailKind::Refused) {
            CHECK_MSG(ends[0].atPx == c.tapPx, "refused at the tap itself: " + Px(ends[0].atPx));
        }
    }
}

// The head-height retry, on a bare rig from (100, 200): a static box low in the
// way dooms the first ray; the retry from 30 over the character clears it.
void TheRetryFromOverTheHead() {
    Rig rig;
    if (!MakeRig(rig)) return;
    const entt::entity low = rig.registry.create();
    rig.registry.emplace<Supersonic::TagComponent>(low, "low");
    rig.registry.emplace<TransformComponent>(low).position = Units::ToWorld(150.0, 205.0);
    rig.registry.emplace<BoxColliderComponent>(low).size =
        glm::vec3(Units::ToMetres(20.0), Units::ToMetres(20.0), 1.0f);
    const glm::dvec2 tap(300.0, 200.0);
    const Portals::FinalPos first = rig.state.FinalPosFrom(rig.registry, tap, glm::dvec2(100.0, 200.0), false);
    CHECK_MSG(first.willDie && first.closest == low && std::fabs(first.atPx.x - 140.0) < 1e-6,
              "the first ray: doomed on the box's face at x 140, " + Px(first.atPx));
    const Portals::FinalPos said = rig.state.Predict(rig.registry, glm::dvec2(100.0, 200.0), tap);
    CHECK_MSG(said.retried && !said.willDie && said.atPx == tap,
              "the retry over the head clears it: portal_mark, " + Px(said.atPx));
    // The port's flight does not retry: its shot runs into the box. The mark
    // and the end disagree here, and that is R23's, not this picture's.
    CHECK(rig.state.Shoot(rig.registry, tap));
    FlyOut(rig);
    CHECK_MSG(rig.state.placed.empty() && rig.state.lastFailure == "low", "the port's shot stops on it");

    // AND THE RETRY CAN DOOM A SHOT THE FIRST RAY LET THROUGH (D3_check B): a
    // crate the first ray ends on - teleportable, so it may open there - with
    // nothing at the tap, and a static beam over the character's line.
    Rig second;
    if (!MakeRig(second)) return;
    const entt::entity crate = second.registry.create();
    second.registry.emplace<TransformComponent>(crate).position = Units::ToWorld(150.0, 200.0);
    second.registry.emplace<BoxColliderComponent>(crate).size =
        glm::vec3(Units::ToMetres(20.0), Units::ToMetres(20.0), 1.0f);
    second.registry.emplace<RigidBodyComponent>(crate);
    second.state.travellers.push_back(crate);
    const entt::entity beam = second.registry.create();
    second.registry.emplace<TransformComponent>(beam).position = Units::ToWorld(200.0, 185.0);
    second.registry.emplace<BoxColliderComponent>(beam).size =
        glm::vec3(Units::ToMetres(30.0), Units::ToMetres(20.0), 1.0f);
    const Portals::FinalPos onCrate = second.state.FinalPosFrom(second.registry, tap, glm::dvec2(100.0, 200.0), false);
    CHECK_MSG(!onCrate.willDie && onCrate.preferred && std::fabs(onCrate.atPx.x - 150.0) < 1e-6,
              "the first ray opens 10 into the crate: " + Px(onCrate.atPx));
    const Portals::FinalPos doomed = second.state.Predict(second.registry, glm::dvec2(100.0, 200.0), tap);
    CHECK_MSG(doomed.retried && doomed.willDie && doomed.atPx == onCrate.atPx,
              "the retry meets the beam: red_cross, and the shot still aimed at the crate, " + Px(doomed.atPx));
    // With the beam out of the way the retry clears the crate and aims at the tap.
    second.registry.destroy(beam);
    const Portals::FinalPos over = second.state.Predict(second.registry, glm::dvec2(100.0, 200.0), tap);
    CHECK_MSG(over.retried && !over.willDie && over.atPx == tap, "no beam: over the crate to the tap, " + Px(over.atPx));
    // And a tap ON the crate is not avoided: something stands at the tap.
    const Portals::FinalPos onIt = second.state.Predict(second.registry, glm::dvec2(100.0, 200.0), glm::dvec2(150.0, 200.0));
    CHECK_MSG(!onIt.retried && !onIt.willDie, "a tap on the crate keeps the first ray");
    // A reflected shot is never doomed by a static body: it stops 10 short.
    const entt::entity wall = second.registry.create();
    second.registry.emplace<TransformComponent>(wall).position = Units::ToWorld(250.0, 300.0);
    second.registry.emplace<BoxColliderComponent>(wall).size =
        glm::vec3(Units::ToMetres(20.0), Units::ToMetres(100.0), 1.0f);
    const Portals::FinalPos off = second.state.FinalPosFrom(second.registry, glm::dvec2(400.0, 300.0),
                                                            glm::dvec2(100.0, 300.0), true);
    CHECK_MSG(!off.willDie && std::fabs(off.atPx.x - 230.0) < 1e-6,
              "a reflected shot at a wall lands 10 short of it: " + Px(off.atPx));
}

void TheGrabberHoldsWhatHoldsThePoint() {
    entt::registry registry;
    const entt::entity box = registry.create();
    registry.emplace<TransformComponent>(box).position = Units::ToWorld(100.0, 100.0);
    registry.emplace<BoxColliderComponent>(box).size = glm::vec3(Units::ToMetres(20.0), Units::ToMetres(20.0), 1.0f);
    const entt::entity sensor = registry.create();
    registry.emplace<TransformComponent>(sensor).position = Units::ToWorld(200.0, 100.0);
    auto& ball = registry.emplace<SphereColliderComponent>(sensor);
    ball.radius = Units::ToMetres(10.0);
    ball.isTrigger = true;
    CHECK(Shot::BodiesAt(registry, glm::dvec2(105.0, 95.0)) == std::vector<entt::entity>{box});
    CHECK(Shot::BodiesAt(registry, glm::dvec2(110.0, 100.0)) == std::vector<entt::entity>{box}); // on the edge
    CHECK(Shot::BodiesAt(registry, glm::dvec2(111.0, 100.0)).empty());
    CHECK_MSG(Shot::BodiesAt(registry, glm::dvec2(205.0, 100.0)) == std::vector<entt::entity>{sensor},
              "a sensor is grabbed too: grabAllObstacles takes anything");
    CHECK(Shot::BodiesAt(registry, glm::dvec2(211.0, 100.0)).empty());
    // And the ray passes what it is told to, and never what it starts inside.
    const entt::entity both[] = {sensor};
    CHECK_MSG(Shot::ClosestContact(registry, glm::dvec2(150.0, 100.0), glm::dvec2(300.0, 100.0),
                                   std::span<const entt::entity>(both))
                      .has_value() == false,
              "the ignore list passes the sensor");
    CHECK(Shot::ClosestContact(registry, glm::dvec2(150.0, 100.0), glm::dvec2(300.0, 100.0), entt::entity{entt::null})->body ==
          sensor);
    CHECK_MSG(!Shot::ClosestContact(registry, glm::dvec2(100.0, 100.0), glm::dvec2(50.0, 100.0), entt::entity{entt::null}),
              "a ray starting inside the box does not meet it (Box2D)");
}

void AFireDiamondsMarkIsALeftoverByte() {
    // The method's fire-diamond branch writes no hasFailed; what is read is the
    // low byte of absTouchPos.x's float bits. A whole pixel is 0 in it - 450.0f
    // is 0x43E10000 - and a fraction generally is not: 450.3f is 0x43E12666.
    CHECK_MSG(!Portals::State::FireDiamondWillDie(450.0), "450.0 px: portal_mark");
    CHECK_MSG(!Portals::State::FireDiamondWillDie(731.25), "731.25 px: 0x4436D000, portal_mark");
    CHECK_MSG(Portals::State::FireDiamondWillDie(450.3), "450.3 px: 0x66 in the low byte, red_cross");
}

void ATapSaysWhyItWasRefused() {
    // A rig with the ORIGINAL's clocks back on (Begin turns them off for the
    // geometry cases), and a shot that flies far enough to outlast 400 ms.
    Rig rig;
    if (!MakeRig(rig)) return;
    rig.state.rules.firstPortalMinMs = 300.0;
    rig.state.rules.nextPortalMinMs = 400.0;
    const glm::dvec2 far(3000.0, 200.0);
    const auto tick = [&rig](int n) {
        for (int i = 0; i < n; ++i) rig.state.Tick(rig.registry, kStep);
    };
    CHECK(!rig.state.Shoot(rig.registry, far));
    CHECK_MSG(rig.state.lastTap.reason == Portals::TapReason::TooEarly && rig.state.lastTap.atPx == far,
              "a tap in the level's first 300 ms: too early, silent");
    tick(25); // 416.7 ms: past both gates
    CHECK(rig.state.Shoot(rig.registry, far));
    CHECK(rig.state.lastTap.reason == Portals::TapReason::Fired);
    tick(10); // 166.7 ms since the tap, the shot still flying
    CHECK(!rig.state.Shoot(rig.registry, glm::dvec2(0.0, 0.0)));
    CHECK_MSG(rig.state.lastTap.reason == Portals::TapReason::Cooldown,
              "within 400 ms of the last tap taken: the original's silent refusal, before its hourglass test");
    tick(20); // 500 ms since the tap taken, the shot still flying
    const double before = rig.state.sinceTapMs;
    CHECK(!rig.state.Shoot(rig.registry, glm::dvec2(0.0, 0.0)));
    CHECK_MSG(rig.state.lastTap.reason == Portals::TapReason::InFlight, "past 400 ms with a shot flying: the hourglass");
    CHECK_MSG(rig.state.sinceTapMs == before && rig.state.flight.has_value(),
              "and the 400 ms is NOT restarted, as the original's is (R23, not played)");
    rig.state.budget = 0;
    CHECK(!rig.state.Shoot(rig.registry, glm::dvec2(0.0, 0.0)));
    CHECK_MSG(rig.state.lastTap.reason == Portals::TapReason::NoBudget, "a budget of 0: blockPortalEffects' tap");
    rig.state.sinceStartMs = 100.0;
    CHECK(!rig.state.Shoot(rig.registry, glm::dvec2(0.0, 0.0)));
    CHECK_MSG(rig.state.lastTap.reason == Portals::TapReason::TooEarly, "update's first gate comes before the budget");
}

void AReflectionIsRecordedForItsPictures() {
    // AReflectorTurnsAShotMirrored's shot, watched tick by tick: one launch from
    // the shooter on its first tick, one turn where it met the reflector - worked
    // out as a reflected shot to its new destination, which nothing blocks - and
    // one more launch, the new leg's, on the tick after.
    Rig rig;
    if (!MakeRig(rig)) return;
    rig.state.shot.maxReflections = 1;
    rig.state.reflectors.push_back({"mirror", glm::dvec2(200.0, 100.0), false});
    CHECK(rig.state.Shoot(rig.registry, glm::dvec2(300.0, 0.0)));
    std::vector<std::pair<int, Portals::Launch>> launches;
    std::vector<std::pair<int, Portals::Reflection>> turns;
    for (int tick = 0; tick < 600 && rig.state.flight; ++tick) {
        rig.state.Tick(rig.registry, kStep);
        for (const Portals::Launch& l : rig.state.launches) launches.emplace_back(tick, l);
        for (const Portals::Reflection& r : rig.state.reflected) turns.emplace_back(tick, r);
    }
    CHECK_EQ(launches.size(), std::size_t{2});
    CHECK_EQ(turns.size(), std::size_t{1});
    if (launches.size() != 2 || turns.size() != 1) return;
    const double k = rig.state.shot.reflectRadiusPx / std::sqrt(2.0);
    const glm::dvec2 entry(200.0 - k, 100.0 + k);
    CHECK_MSG(launches[0].first == 0 && launches[0].second.fromPx == glm::dvec2(100.0, 200.0),
              "the first leg launches on the shot's first callback, from the shooter");
    CHECK_MSG(glm::distance(turns[0].second.atPx, entry) < 1e-6, "turned where it met the reflector: " +
                                                                      Px(turns[0].second.atPx));
    CHECK_MSG(launches[1].first == turns[0].first + 1 && launches[1].second.fromPx == turns[0].second.atPx &&
                  launches[1].second.towardPx == turns[0].second.toPx,
              "the new leg launches on the tick after the turn, from where it turned");
    const Portals::FinalPos& said = turns[0].second.said;
    CHECK_MSG(!said.willDie && !said.redWaves && said.atPx == turns[0].second.toPx,
              "nothing in the new leg's way: portal_mark at its end, " + Px(said.atPx));
    // The rig's lists are this tick's alone: the tick after the shot is over has none.
    rig.state.Tick(rig.registry, kStep);
    CHECK(rig.state.launches.empty() && rig.state.reflected.empty() && rig.state.failures.empty());
}

void EnteringACircle() {
    const auto across = Shot::EntersCircle({0.0, 0.0}, {100.0, 0.0}, {50.0, 0.0}, 10.0);
    CHECK_MSG(across && std::fabs(*across - 0.4) < 1e-12, "met where it first comes within 10 px");
    CHECK_MSG(!Shot::EntersCircle({0.0, 0.0}, {100.0, 0.0}, {50.0, 11.0}, 10.0), "passing 11 px off, never");
    CHECK_MSG(!Shot::EntersCircle({0.0, 0.0}, {30.0, 0.0}, {50.0, 0.0}, 10.0), "stopping short, never");
    const auto inside = Shot::EntersCircle({50.0, 5.0}, {100.0, 0.0}, {50.0, 0.0}, 10.0);
    CHECK_MSG(inside && *inside == 0.0, "starting within it, at once");
}

// ---- The decoded flight (planning step 149) ---------------------------------------------
//
// ETHCallback_projectile, from the shot's rig: a shooter at (100, 200) in an empty plane, a tap
// along +x, and the 60 Hz step the program matches - the script's FLOORED 16 ms, so one advance is
// 200 x 16 / 1000 = 3.2 units and the first callback's 24 + 3.2 = 27.2.
const ScriptClock::Step kFloored(0.016, 1.0f / 60.0f);

int FlyFloored(Rig& rig, int limit = 1200) {
    int ticks = 0;
    while (rig.state.flight && ticks < limit) {
        rig.state.Tick(rig.registry, kFloored);
        ++ticks;
    }
    return ticks;
}

void TheDecodedFlightNumbersAreShotJsons() {
    Run run;
    if (!Begin("level1", run)) return;
    const Shot::Rules& rules = run.data.shot;
    CHECK_EQ(rules.speedUnits, 200.0);     // scaledUnitsPerSecond(200f), #113-116
    CHECK_EQ(rules.jumpUnits, 24.0);       // g_scale.scale(24), #29-55 (PshC4 1103101952)
    CHECK_EQ(rules.frameCapMs, 200.0);     // unitsPerSecond's min(200, ...), STimeManager ins 5-9
}

void TheShotFliesAsTheOriginalsProjectileDoes() {
    // Ticks of flight over stated distances: 20 and 27.0 arrive on the first callback (the jump and the
    // first advance carry the shot to 27.2 > the distance), 27.3 needs a second, and so on. The 600
    // units a second the port gave it until step 149 would land 300 in 30 ticks; the decoded 192 a
    // second of wall time takes 87.
    struct Case {
        double distance;
        int ticks; // Ticks from the Shoot that fires it
    };
    for (const Case& c : {Case{20.0, 1}, Case{27.0, 1}, Case{27.3, 2}, Case{100.0, 24}, Case{300.0, 87},
                          Case{500.0, 149}}) {
        Rig rig;
        if (!MakeRig(rig)) return;
        const glm::dvec2 tap(100.0 + c.distance, 200.0);
        CHECK(rig.state.Shoot(rig.registry, tap));
        const int ticks = FlyFloored(rig);
        CHECK_MSG(ticks == c.ticks, std::to_string(c.distance) + " units: " + std::to_string(ticks) +
                                        " ticks, the decode says " + std::to_string(c.ticks));
        CHECK_MSG(ticks == FlightTicks(rig.state.shot, c.distance, 16.0), "and the helper agrees");
        // It opens AT the destiny - the shot overshoots it and is deleted, it is never snapped to it.
        CHECK_MSG(rig.state.placed.size() == 1 && rig.state.placed[0].basePx == tap,
                  std::to_string(c.distance) + " units: the portal opens at the tap " + Where(tap));
    }
}

void TheShotMovesOnItsOwnTicks() {
    Rig rig;
    if (!MakeRig(rig)) return;
    const glm::dvec2 from(100.0, 200.0);
    const auto at = [&rig] { return rig.state.flight ? rig.state.flight->atPx : glm::dvec2(0.0); };
    CHECK(rig.state.Shoot(rig.registry, glm::dvec2(400.0, 200.0)));
    CHECK(rig.state.flight && rig.state.flight->fresh && !rig.state.flight->launched);
    // The first Tick after the tap is the original's next frame: initialPos stored, the 24-unit jump,
    // projectile_shoot flashed from where it was fired (the picture adds the 24), then the advance of
    // 3.2: 27.2 out.
    rig.state.Tick(rig.registry, kFloored);
    CHECK_MSG(rig.state.flight && std::fabs(rig.state.flight->atPx.x - 127.2) < 1e-9 && rig.state.flight->atPx.y == 200.0,
              "the first callback carries it to 24 + 3.2: " + Where(at()));
    CHECK_MSG(rig.state.launches.size() == 1 && rig.state.launches[0].fromPx == from, "and flashes once");
    // Then 3.2 a tick, 192 units a second of wall time.
    double x = 127.2;
    for (int tick = 0; tick < 10; ++tick) {
        rig.state.Tick(rig.registry, kFloored);
        x += 3.2;
        CHECK_MSG(rig.state.flight && std::fabs(rig.state.flight->atPx.x - x) < 1e-9 && rig.state.launches.empty(),
                  "tick " + std::to_string(tick + 2) + ": " + Where(at()));
    }
}

void TheShotReadsTheScriptClockAndItsCap() {
    // The advance is speed x min(200, the script's ms) / 1000 (STimeManager::unitsPerSecond), so it
    // follows the floored clock: a bare 16.667 ms tick would carry it 3.333, and takes 83 ticks over
    // 300 where the floored 16 ms takes 87. And a frame over the cap (a stall) is only 200 ms's worth.
    {
        Rig rig;
        if (!MakeRig(rig)) return;
        CHECK(rig.state.Shoot(rig.registry, glm::dvec2(400.0, 200.0)));
        int ticks = 0;
        for (; rig.state.flight && ticks < 1200; ++ticks) rig.state.Tick(rig.registry, kStep);
        CHECK_MSG(ticks == 83 && ticks == FlightTicks(rig.state.shot, 300.0, static_cast<double>(kStep) * 1000.0),
                  "the unfloored tick: " + std::to_string(ticks));
    }
    {
        Rig rig;
        if (!MakeRig(rig)) return;
        CHECK(rig.state.Shoot(rig.registry, glm::dvec2(1000.0, 200.0)));
        const ScriptClock::Step stall(0.25, 1.0f / 60.0f);
        rig.state.Tick(rig.registry, stall); // 24 + 200 x 200 / 1000 = 64, not 24 + 50
        CHECK_MSG(rig.state.flight && std::fabs(rig.state.flight->atPx.x - (100.0 + 64.0)) < 1e-9,
                  "a 250 ms frame advances 40, the cap's: " +
                      Where(rig.state.flight ? rig.state.flight->atPx : glm::dvec2(0.0)));
    }
}

void AnAgentSeesTheShotWhereItIsSampled() {
    // ETHCallback_anti_portal_agent is a static callback: it sees the shot once a frame, where the
    // shot's own callback and the tap's `loop` have left it - at its spawn on the tap's frame, then
    // 27.2 out, then every 3.2 - and asks that it be short of the end of its way and within
    // 0.7 x 32 = 22.4 (both strict). The agent's reach here is 22.4.
    const auto agentAt = [](Rig& rig, const glm::dvec2& at) {
        Portals::Agent agent;
        agent.name = "agent";
        agent.centrePx = at;
        rig.state.agents = {agent};
    };
    // (1) Within reach of the PLAYER: dead in the first pass of the first Tick, the static callbacks of the
    // frame the tap fired it on, before the shot has had a callback.
    {
        Rig rig;
        if (!MakeRig(rig)) return;
        agentAt(rig, glm::dvec2(115.0, 215.0)); // 21.2 from (100, 200)
        CHECK(rig.state.Shoot(rig.registry, glm::dvec2(400.0, 200.0)));
        rig.state.Tick(rig.registry, kFloored);
        CHECK_MSG(rig.state.agentKills.size() == 1 && rig.state.agentsKilled == 1 && !rig.state.flight &&
                      rig.state.agentKills[0].shotPx == glm::dvec2(100.0, 200.0),
                  "an agent within 22.4 of the player is killed at the spawn, before any callback");
    }
    // (2) A self-tap whose destiny is the player's own point (nothing to fly): it is not short of the end
    // of its way, so the static pass spares the agent; the shot then arrives on its first callback.
    {
        Rig rig;
        if (!MakeRig(rig)) return;
        agentAt(rig, glm::dvec2(115.0, 215.0));
        CHECK(rig.state.Shoot(rig.registry, glm::dvec2(100.0, 200.0)));
        FlyFloored(rig);
        CHECK_MSG(rig.state.agentsKilled == 0 && rig.state.placed.size() == 1, "a shot with no way to fly kills nothing");
    }
    // (3) An agent only the first hop's LINE passes: 20 off the axis, midway through the jump, 24.2 from
    // the spawn and from the point 27.2 out. The sampled positions never come within 22.4 of it, and the
    // shot flies on to its tap; the old per-tick stretch killed it.
    {
        Rig rig;
        if (!MakeRig(rig)) return;
        agentAt(rig, glm::dvec2(113.6, 220.0));
        CHECK(rig.state.Shoot(rig.registry, glm::dvec2(400.0, 200.0)));
        FlyFloored(rig);
        CHECK_MSG(rig.state.agentsKilled == 0 && rig.state.placed.size() == 1 && !rig.state.agents[0].gone,
                  "an agent the jump passes between its samples is not killed");
    }
    // (4) An agent in the line of flight: killed on the first callback whose position is within 22.4. Its
    // callbacks are at x = 127.2 + 3.2 (m - 1); 200 - 22.4 = 177.6, so m = 17 (178.4), Tick 17.
    {
        Rig rig;
        if (!MakeRig(rig)) return;
        agentAt(rig, glm::dvec2(200.0, 200.0));
        CHECK(rig.state.Shoot(rig.registry, glm::dvec2(400.0, 200.0)));
        const int ticks = FlyFloored(rig);
        CHECK_MSG(ticks == 17 && rig.state.agentKills.size() == 1 &&
                      std::fabs(rig.state.agentKills[0].shotPx.x - 178.4) < 1e-9,
                  "killed on tick " + std::to_string(ticks) + " at " +
                      (rig.state.agentKills.empty() ? std::string("nowhere") : Where(rig.state.agentKills[0].shotPx)));
    }
    // (5) A shot that ARRIVES is deleted before the static callbacks look: an agent 22.4 or less from the
    // tap, with the last in-flight sample (197.6) 24.4 from it, is not met. The tap is at x 200 and the
    // agent at 222 (22 from the tap, 24.4 from the last sample): the portal opens at the tap.
    {
        Rig rig;
        if (!MakeRig(rig)) return;
        agentAt(rig, glm::dvec2(222.0, 200.0));
        CHECK(rig.state.Shoot(rig.registry, glm::dvec2(200.0, 200.0)));
        const int ticks = FlyFloored(rig);
        CHECK_MSG(ticks == 24 && rig.state.agentsKilled == 0 && rig.state.placed.size() == 1 &&
                      rig.state.placed[0].basePx == glm::dvec2(200.0, 200.0),
                  "arrived on tick " + std::to_string(ticks) + ", the agent spared");
    }
}

void AFlameSeesTheShotWhereItIsSampled() {
    // ETHCallback_fire_agent: squaredDistance(the shot's position, the agent's) < size.x ^ 2 (32), once
    // a static frame, with no in-flight test of its own - and none for a shot that arrived.
    const auto flameAt = [](Rig& rig, const glm::dvec2& at) {
        rig.state.flames = {Portals::Flame{"flame", at, 32.0}};
    };
    // (1) Within the 32 of the player: burned where it was fired, in the first pass of the first Tick,
    // heading along its leg.
    {
        Rig rig;
        if (!MakeRig(rig)) return;
        flameAt(rig, glm::dvec2(110.0, 200.0));
        CHECK(rig.state.Shoot(rig.registry, glm::dvec2(400.0, 200.0)));
        rig.state.Tick(rig.registry, kFloored);
        CHECK_MSG(rig.state.burns.size() == 1 && rig.state.burns[0].atPx == glm::dvec2(100.0, 200.0) &&
                      rig.state.burns[0].heading == glm::dvec2(1.0, 0.0) && !rig.state.flight,
                  "burned where it was fired, before any callback");
    }
    // (2) A flame only the first hop's line passes within 32 of (30 off the axis, 32.9 from the spawn and
    // from 27.2 out): not burned.
    {
        Rig rig;
        if (!MakeRig(rig)) return;
        flameAt(rig, glm::dvec2(113.6, 230.0));
        CHECK(rig.state.Shoot(rig.registry, glm::dvec2(400.0, 200.0)));
        FlyFloored(rig);
        CHECK_MSG(rig.state.shotsBurned == 0 && rig.state.placed.size() == 1, "the jump's line is not sampled");
    }
    // (3) In the line of flight: burned at the first sampled position within 32 of it - x 168.8 (the
    // callback m = 14, Tick 14) for a flame at x 200 - not where the line first crosses the circle (168).
    {
        Rig rig;
        if (!MakeRig(rig)) return;
        flameAt(rig, glm::dvec2(200.0, 200.0));
        CHECK(rig.state.Shoot(rig.registry, glm::dvec2(400.0, 200.0)));
        int burnedOn = 0;
        for (int tick = 1; tick <= 100 && rig.state.flight; ++tick) {
            rig.state.Tick(rig.registry, kFloored);
            if (!rig.state.burns.empty()) burnedOn = tick;
        }
        CHECK_MSG(burnedOn == 14 && rig.state.burns.size() == 1 && std::fabs(rig.state.burns[0].atPx.x - 168.8) < 1e-9,
                  "burned on tick " + std::to_string(burnedOn));
    }
    // (4) A shot that ARRIVES is gone before the static callbacks look: the tap at x 200 and a flame at
    // 231.5 (31.5 from the tap), the last in-flight sample (197.6) 33.9 from it: opens its portal.
    {
        Rig rig;
        if (!MakeRig(rig)) return;
        flameAt(rig, glm::dvec2(231.5, 200.0));
        CHECK(rig.state.Shoot(rig.registry, glm::dvec2(200.0, 200.0)));
        FlyFloored(rig);
        CHECK_MSG(rig.state.shotsBurned == 0 && rig.state.placed.size() == 1, "the arrived shot is not burned");
    }
}

void AWallInTheJumpStillStopsTheShot() {
    // The original decides a wall at the tap, by a ray from 8 units out that sees every wall on the way,
    // and the port stops its shot on the first body along the way: the jump is not a gap in it. A box 10
    // units from the shooter, inside the first 24, stops the shot on its first callback.
    Rig rig;
    if (!MakeRig(rig)) return;
    const entt::entity wall = rig.registry.create();
    rig.registry.emplace<Supersonic::TagComponent>(wall, "wall");
    rig.registry.emplace<TransformComponent>(wall).position = Units::ToWorld(115.0, 200.0);
    rig.registry.emplace<BoxColliderComponent>(wall).size = glm::vec3(Units::ToMetres(10.0), Units::ToMetres(60.0), 1.0f);
    CHECK(rig.state.Shoot(rig.registry, glm::dvec2(400.0, 200.0)));
    const int ticks = FlyFloored(rig);
    CHECK_MSG(ticks == 1 && rig.state.lastFailure == "wall" && rig.state.placed.empty() && rig.state.failures.size() == 1 &&
                  std::fabs(rig.state.failures[0].atPx.x - 110.0) < 1e-3,
              "stopped on tick " + std::to_string(ticks) + " by '" + rig.state.lastFailure + "'");
}

void runTests() {
    Level6sBlocker();
    TheDecodedFlightNumbersAreShotJsons();
    TheShotFliesAsTheOriginalsProjectileDoes();
    TheShotMovesOnItsOwnTicks();
    TheShotReadsTheScriptClockAndItsCap();
    AnAgentSeesTheShotWhereItIsSampled();
    AFlameSeesTheShotWhereItIsSampled();
    AWallInTheJumpStillStopsTheShot();
    AShotOpensAPortalWhereItArrives();
    AWallStopsIt();
    AStoneStopsIt();
    TheBlockerStopsIt();
    ANoPortalZoneAtTheTapFailsIt();
    NoPortalsNoShot();
    ExactInThePlane();
    TheReflectorsAreTheLevelsOwn();
    AReflectorTurnsAShotMirrored();
    AShotComesOffOneReflectorAtMost();
    EnteringACircle();
    AReflectorWithNoPlaneTakesTheDefault();
    TheFinalPosRulesAreTheDecodedOnes();
    TheTapTimeRayOnF3sTaps();
    TheRetryFromOverTheHead();
    TheGrabberHoldsWhatHoldsThePoint();
    AFireDiamondsMarkIsALeftoverByte();
    ATapSaysWhyItWasRefused();
    AReflectionIsRecordedForItsPictures();
}

} // namespace

MP_SUITE_MAIN("test_mp_shot") {
    std::error_code ec;
    if (!std::filesystem::is_directory(kLevels, ec) ||
        !std::filesystem::is_regular_file(kData + "/entity_roles.json", ec)) {
        std::printf("test_mp_shot: SKIPPED - needs the converted levels at %s\n"
                    "  and Enhanced's data at %s.\n"
                    "  Both live outside this repository; configure with\n"
                    "  -DSUPERSONIC_MAGICPORTALS_LEVELS=... and -DSUPERSONIC_MAGICPORTALS_DATA=...\n",
                    kLevels.c_str(), kData.c_str());
        return 77;
    }
    runTests();
    return ::test::summary("test_mp_shot", 30);
}
