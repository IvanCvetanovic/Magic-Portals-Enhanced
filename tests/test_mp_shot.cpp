// The portal shot: a tap fires, and the portal opens where the shot arrives.
//
// What the owner said on 11 September is tested as it stands:
//   - the shot flies from the character toward the tap, and the portal opens
//     where it arrives;
//   - a wall on the way stops it (a crate or a stone a portal may open on does not: it opens there);
//   - a failed shot costs nothing.
// What the level files say is pinned: level6's projectile blocker and its box.
// The shot is the original's, decoded. WHERE IT GOES and whether it is doomed are the tap-time ray's
// answer (computePortalFinalPos; planning step 156, R23): the contact for a wall or a body a portal may
// not open on, 10 units past it for one it may, and the shot meets no body on its way - the ray decided
// all of those at the tap. HOW IT FLIES is shot.json `flight` (planning step 149):
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
#include "sim/Zerog.hpp"

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

std::string Named(entt::registry& registry, entt::entity body) {
    if (body == entt::null) return "nothing";
    const auto* tag = registry.try_get<Supersonic::TagComponent>(body);
    return tag != nullptr ? tag->tag : std::string("a body");
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

void AStoneIsWhereAPortalOpens() {
    // level8: from the spawn at the hint arrow behind the stone. The stone, rolling_stone_ent_597, is in
    // the way - and its entity says teleportable 1, so the original's ray (isTeleportable) lets the shot
    // through it: the portal opens ON the stone, 10 units past the ray's contact along the aim
    // (computePortalFinalPos #194-248). Until step 156 the port's shot stopped on it and failed.
    Run run;
    if (!Begin("level8", run)) return;
    const glm::dvec2 tap(30.0, 106.0);
    const glm::dvec2 from = PlayerPx(run);
    const Portals::FinalPos said = run.level.portals.Predict(run.registry, from, tap);
    CHECK_MSG(said.closest != entt::null && Named(run.registry, said.closest) == "rolling_stone_ent_597",
              "the ray meets the stone first: " + Named(run.registry, said.closest));
    CHECK_MSG(!said.willDie && said.preferred, "and a teleportable body is where it may open");
    CHECK(run.level.portals.Shoot(run.registry, tap));
    CHECK_MSG(run.level.portals.flight && run.level.portals.flight->toPx == said.atPx,
              "the shot is aimed at the ray's finalPos, not at the tap");
    Fly(run);
    CHECK_MSG(run.level.portals.placed.size() == 1 && run.level.portals.placed[0].basePx == said.atPx,
              "a portal opens at the ray's end on the stone: " + run.level.portals.lastFailure);
    CHECK_EQ(run.level.portals.shotsFailed, 0);
    CHECK_EQ(run.level.portals.portalsUsed, 1);
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
                if (r.name == e.name && r.centrePx == glm::dvec2(e.x, e.y) && r.plane == Shot::Plane::Horizontal &&
                    r.scale == 1.0) {
                    found = true;
                }
            }
            if (!found) pinned = false;
        }
        CHECK_MSG(pinned, "level11's three reflectors, horizontal, where the file puts them");
    }
    Run twelve;
    if (Begin("level12", twelve)) {
        const std::vector<Portals::Reflector>& reflectors = twelve.level.portals.reflectors;
        CHECK_MSG(reflectors.size() == 1 && reflectors[0].name == "reflect_agent_584" &&
                      reflectors[0].centrePx == glm::dvec2(248.0, 149.0) && reflectors[0].plane == Shot::Plane::Vertical,
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

// ---- What the original works out at the tap (the visuals plan's 10.1) -----------
//
// computePortalFinalPos, run on the port's world at the tap (placement.json final_pos): where the
// shot goes, whether it is already doomed - red_cross rather than portal_mark - and whether
// red_waves (A) goes at the aim. Pinned on 1-07 against footage F3's own taps, and on a bare rig for
// the retry. Until step 156 it decided the pictures alone (the owner's standing ruling R15) and the
// port's flight could disagree with it; the shot now flies by it, so the two agree by construction.

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
        CHECK_MSG(launches.size() == 1 && launches[0].fromPx == from && launches[0].towardPx == said.atPx,
                  std::string(c.what) + ": one launch, on the shot's first tick, from where it left toward the ray's end: " +
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
            // At the SEAM of two bodies either is the body met: from the original's own body (planning step 153: the player
            // 3.9 units lower) the wall case's flight ends on y 128, where single_block_plat_ent_785 meets
            // collision_192x32_ent_822, and the ray names the other of the two. The contact below is what the two must share.
            CHECK_MSG(run.level.portals.lastFailure == Named(run.registry, said.closest) ||
                          glm::distance(ends[0].atPx, said.atPx) < 0.5,
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
    // The shot flies by the retry's answer (step 156): over the box, to the tap, where it opens a portal.
    // Until then the port's flight did not retry and its shot ran into the box while the mark said
    // portal_mark.
    CHECK(rig.state.Shoot(rig.registry, tap));
    CHECK_MSG(rig.state.lastTap.said.retried && rig.state.flight && rig.state.flight->toPx == tap,
              "aimed at the tap, after the retry");
    FlyOut(rig);
    CHECK_MSG(rig.state.placed.size() == 1 && rig.state.placed[0].basePx == tap && rig.state.shotsFailed == 0,
              "and the shot passes over the box and opens at the tap");

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
    CHECK_MSG(rig.state.sinceTapMs == 0.0, "a tap taken restarts the 400 ms (#75-78)");
    tick(10); // 166.7 ms since the tap, the shot still flying
    double before = rig.state.sinceTapMs;
    CHECK(!rig.state.Shoot(rig.registry, glm::dvec2(0.0, 0.0)));
    CHECK_MSG(rig.state.lastTap.reason == Portals::TapReason::Cooldown,
              "within 400 ms of the last tap taken: the original's silent refusal, before its hourglass test");
    CHECK_MSG(rig.state.sinceTapMs == before, "and it moves no clock: the 400 ms runs on");
    tick(20); // 500 ms since the tap taken, the shot still flying
    CHECK(!rig.state.Shoot(rig.registry, glm::dvec2(0.0, 0.0)));
    CHECK_MSG(rig.state.lastTap.reason == Portals::TapReason::InFlight, "past 400 ms with a shot flying: the hourglass");
    CHECK_MSG(rig.state.sinceTapMs == 0.0 && rig.state.flight.has_value(),
              "and the tap RESTARTS the 400 ms, as the original's does before it looks for a projectile (step 156, R23)");
    tick(10); // 166.7 ms after the hourglass tap
    CHECK(!rig.state.Shoot(rig.registry, glm::dvec2(0.0, 0.0)));
    CHECK_MSG(rig.state.lastTap.reason == Portals::TapReason::Cooldown,
              "so a second tap soon after an hourglass tap draws nothing there");
    tick(25); // 583 ms after it
    CHECK(!rig.state.Shoot(rig.registry, glm::dvec2(0.0, 0.0)));
    CHECK_MSG(rig.state.lastTap.reason == Portals::TapReason::InFlight, "and one past 400 ms is an hourglass again");
    rig.state.budget = 0;
    before = rig.state.sinceTapMs;
    CHECK(!rig.state.Shoot(rig.registry, glm::dvec2(0.0, 0.0)));
    CHECK_MSG(rig.state.lastTap.reason == Portals::TapReason::NoBudget, "a budget of 0: blockPortalEffects' tap");
    CHECK_MSG(rig.state.sinceTapMs == before, "which never reads the 400 ms clock and never restarts it");
    rig.state.sinceStartMs = 100.0;
    CHECK(!rig.state.Shoot(rig.registry, glm::dvec2(0.0, 0.0)));
    CHECK_MSG(rig.state.lastTap.reason == Portals::TapReason::TooEarly, "update's first gate comes before the budget");
    CHECK_MSG(rig.state.sinceTapMs == before, "and moves no clock either");
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
    // and its shot flies to the contact and fails there: the jump is not a gap in it. A box 10
    // units from the shooter, inside the first 24, ends the shot on its first callback (it arrives, past
    // its destiny at the box's face).
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

// ---- The shot flies by the tap-time ray (planning step 156, R23) ---------------------------------------------
//
// computePortalFinalPos decides at the tap where the shot goes and whether it is doomed, and the shot, which
// has no body, flies there and does what the ray said where it arrives (ETHCallback_projectile #127-272).
// Every case is on the bare rig: a shooter at (100, 200) in an empty plane, a few boxes, the floored 16 ms.

// The boxes are held in metres, floats: a face at 190 or 300.5 reads within a hundred-thousandth of a unit.
bool Near(const glm::dvec2& a, const glm::dvec2& b, double tolerance = 1e-3) { return glm::distance(a, b) < tolerance; }

entt::entity PutBox(Rig& rig, const char* tag, const glm::dvec2& centre, const glm::dvec2& size, bool looseTeleportable,
                    bool trigger = false) {
    const entt::entity box = rig.registry.create();
    rig.registry.emplace<Supersonic::TagComponent>(box, tag);
    rig.registry.emplace<TransformComponent>(box).position = Units::ToWorld(centre.x, centre.y);
    auto& collider = rig.registry.emplace<BoxColliderComponent>(box);
    collider.size = glm::vec3(Units::ToMetres(size.x), Units::ToMetres(size.y), 1.0f);
    collider.isTrigger = trigger;
    if (looseTeleportable) {
        rig.registry.emplace<RigidBodyComponent>(box);
        rig.state.travellers.push_back(box);
    }
    return box;
}

void TheShotGoesToTheRaysEndAndFailsOrOpensThere() {
    // A wall a portal may not open on: the contact, and the shot fails there. A crate it may: 10 units past the
    // contact, and a portal opens on the crate. Until step 156 the tap was the destination and the shot stopped
    // on the first body it met; here the tap is 300 units out and the ray stops at x 190.
    const glm::dvec2 tap(400.0, 200.0);
    {
        Rig rig;
        if (!MakeRig(rig)) return;
        PutBox(rig, "wall", glm::dvec2(200.0, 200.0), glm::dvec2(20.0, 100.0), false);
        CHECK(rig.state.Shoot(rig.registry, tap));
        CHECK_MSG(rig.state.flight && Near(rig.state.flight->toPx, glm::dvec2(190.0, 200.0)) && rig.state.flight->said.willDie,
                  "a wall: aimed at its face, doomed");
        const int ticks = FlyFloored(rig);
        CHECK_MSG(ticks == FlightTicks(rig.state.shot, 90.0, 16.0), "it flies the 90 units to the face: " + std::to_string(ticks));
        CHECK_MSG(rig.state.failures.size() == 1 && Near(rig.state.failures[0].atPx, glm::dvec2(190.0, 200.0)) &&
                      rig.state.failures[0].kind == Portals::FailKind::Body && rig.state.lastFailure == "wall",
                  "portal_fail at the contact, a body's");
        CHECK(rig.state.placed.empty() && rig.state.shotsFailed == 1 && rig.state.portalsUsed == 0);
    }
    {
        Rig rig;
        if (!MakeRig(rig)) return;
        PutBox(rig, "crate", glm::dvec2(200.0, 200.0), glm::dvec2(20.0, 100.0), true);
        // The tap is ON the crate: with nothing at the tap the head-height retry would run (D3_check B), and with it
        // the first ray's end, which is what this case pins, is the retry's.
        CHECK(rig.state.Shoot(rig.registry, glm::dvec2(205.0, 200.0)));
        CHECK_MSG(rig.state.flight && Near(rig.state.flight->toPx, glm::dvec2(200.0, 200.0)) && !rig.state.flight->said.willDie,
                  "a body a portal may open on: aimed 10 units past its face");
        const int ticks = FlyFloored(rig);
        CHECK_MSG(ticks == FlightTicks(rig.state.shot, 100.0, 16.0), "it flies the 100 units: " + std::to_string(ticks));
        CHECK_MSG(rig.state.placed.size() == 1 && Near(rig.state.placed[0].basePx, glm::dvec2(200.0, 200.0)) &&
                      rig.state.failures.empty(),
                  "and opens a portal there, inside the crate");
    }
    {
        // A SENSOR on no ignore list is a body to the ray (a button; 46 of them, in 37 levels): the shot is doomed at
        // its face (a tall one, which the head-height retry cannot hop), where the port's old flight passed through
        // it and opened a portal beyond. One the ray is told to
        // pass (shock_agent, death_area, anti_sight_wall, gravity_agent) is no body at all.
        Rig rig;
        if (!MakeRig(rig)) return;
        const entt::entity button = PutBox(rig, "button", glm::dvec2(200.0, 200.0), glm::dvec2(20.0, 100.0), false, true);
        CHECK(rig.state.Predict(rig.registry, glm::dvec2(100.0, 200.0), tap).willDie);
        rig.state.rayIgnored.push_back(button);
        const Portals::FinalPos passed = rig.state.Predict(rig.registry, glm::dvec2(100.0, 200.0), tap);
        CHECK_MSG(!passed.willDie && passed.atPx == tap, "an ignored sensor is passed: " + Px(passed.atPx));
        rig.state.rayIgnored.clear();
        CHECK(rig.state.Shoot(rig.registry, tap));
        FlyFloored(rig);
        CHECK_MSG(rig.state.placed.empty() && rig.state.lastFailure == "button" && rig.state.shotsFailed == 1,
                  "the button dooms the shot at the tap: " + rig.state.lastFailure);
    }
}

void TheShotMeetsNoBodyOnItsWay() {
    // projectile.ent has no shape: a wall that comes into the way AFTER the tap (a platform, a loose crate, a door
    // opening) does not stop it, and the ray, which looked once, is not asked again.
    Rig rig;
    if (!MakeRig(rig)) return;
    const glm::dvec2 tap(300.0, 200.0);
    CHECK(rig.state.Shoot(rig.registry, tap));
    for (int tick = 0; tick < 5; ++tick) rig.state.Tick(rig.registry, kFloored);
    CHECK(rig.state.flight.has_value());
    PutBox(rig, "late wall", glm::dvec2(200.0, 200.0), glm::dvec2(20.0, 100.0), false);
    FlyFloored(rig);
    CHECK_MSG(rig.state.placed.size() == 1 && rig.state.placed[0].basePx == tap && rig.state.shotsFailed == 0,
              "it flew through the wall that arrived after the tap and opened at the tap");
}

void AReflectedShotNeverDiesOnAWall() {
    // A horizontal reflector at (200, 100) on the line from (100, 200) to the tap (300, 0) sends the shot out of ITS OWN
    // point toward (300, 200) (planning step 157: the whole 141.4 from the shooter to the reflector, flipped in y), with a
    // wall on the reflected leg's way only. A reflected shot's ray (hasBeenReflected) takes a static body as pushOut: it
    // lands 10 units SHORT of the wall instead of dying on it (computePortalFinalPos #136-156, #194-215). Fired straight at
    // the same wall it is doomed.
    Rig rig;
    if (!MakeRig(rig)) return;
    rig.state.reflectors.push_back({"mirror", glm::dvec2(200.0, 100.0), Shot::Plane::Horizontal, 1.0});
    PutBox(rig, "wall", glm::dvec2(270.0, 230.0), glm::dvec2(40.0, 260.0), false);
    CHECK(rig.state.Shoot(rig.registry, glm::dvec2(300.0, 0.0)));
    FlyFloored(rig);
    // The new leg runs (1, 1) / sqrt 2 from (200, 100) and meets the wall's face x 250 at (250, 150): 10 short of it.
    const glm::dvec2 expected = glm::dvec2(250.0, 150.0) - glm::dvec2(1.0, 1.0) / std::sqrt(2.0) * 10.0;
    CHECK_EQ(rig.state.reflections, 1);
    CHECK_MSG(rig.state.shotsFailed == 0 && rig.state.placed.size() == 1 &&
                  glm::distance(rig.state.placed[0].basePx, expected) < 1e-3,
              "the reflected shot lands 10 short of the wall and opens: " +
                  (rig.state.placed.empty() ? rig.state.lastFailure : Where(rig.state.placed[0].basePx)) + " for " +
                  Where(expected));
    Rig direct;
    if (!MakeRig(direct)) return;
    PutBox(direct, "wall", glm::dvec2(270.0, 230.0), glm::dvec2(40.0, 260.0), false);
    CHECK(direct.state.Shoot(direct.registry, glm::dvec2(400.0, 300.0)));
    CHECK_MSG(direct.state.flight && direct.state.flight->said.willDie, "the same wall dooms an unreflected shot");
}

// ---- The reflector as the original catches and turns (planning step 157, G9) -------------------------------
//
// ETHCallback_reflect_agent (MiscCallbacks 349003..350254; shot.json `reflection` has the listing's lines): a SAMPLE of
// the shot's position STRICTLY within size.x (32) of the agent's point catches it - never the way between two samples,
// never a shot that has arrived - and the shot the agent adds leaves the AGENT'S point, along the mirrored heading, for
// min(the length the old one was fired, the way from where its leg began to the agent). The rig is the flight's own:
// a shooter at (100, 200) in an empty plane and the program's floored 16 ms, so the callbacks sample a shot fired along
// +x from x0 at x0 + 24 + 3.2 n (n = 1 for the first callback, which is the first Tick after the Shoot).

void PutShooter(Rig& rig, const glm::dvec2& atPx) {
    rig.registry.get<TransformComponent>(rig.state.shooter).position = Units::ToWorld(atPx.x, atPx.y);
}

Portals::Reflector Mirror(const char* name, const glm::dvec2& at, Shot::Plane plane) {
    return Portals::Reflector{name, at, plane, 1.0};
}

// What flying a rig's shot to its end showed: the Tick (counted from 1, after the Shoot) of the first turn, every turn, and
// how many Ticks the flight took in all.
struct TurnSeen {
    int tick = -1;
    int turns = 0;
    int ticks = 0;
    Portals::Reflection first;
    std::vector<std::pair<int, Portals::Launch>> launches;
};

TurnSeen FlyAndWatch(Rig& rig, int limit = 1200) {
    TurnSeen seen;
    while (rig.state.flight && seen.ticks < limit) {
        rig.state.Tick(rig.registry, kFloored);
        ++seen.ticks;
        for (const Portals::Launch& launch : rig.state.launches) seen.launches.emplace_back(seen.ticks, launch);
        for (const Portals::Reflection& turn : rig.state.reflected) {
            if (seen.turns++ == 0) {
                seen.tick = seen.ticks;
                seen.first = turn;
            }
        }
    }
    return seen;
}

// The x a shot fired along +x from `x0` is sampled at by its n-th callback.
double SampleX(const Rig& rig, double x0, int callback) {
    return x0 + rig.state.shot.jumpUnits + callback * rig.state.shot.speedUnits * 16.0 / 1000.0;
}

std::string At(const glm::dvec2& p) { return Where(p); }

void TheReflectorsAreTheLevelsOwnCensus() {
    // Every reflect_agent of the 128 levels, as the .esc files carry them (reference/extracted/assets/scenes; the
    // converted levels agree): 23 placements in 19 levels, none with a scale of its own, 9 horizontal, 13 vertical and one
    // with no plane at all - 2-27's reflect_agent_ent_961, whose only custom data is `follow`: GetString('plane') is the
    // empty string there (ETHEntity.cpp:872-877), computeReflectVector's first arm.
    struct Row {
        const char* level; // the file, and the level's W-LL
        const char* world;
        int horizontal;
        int vertical;
        int planeless;
    };
    int horizontal = 0;
    int vertical = 0;
    int planeless = 0;
    for (const Row& row : {Row{"level10a", "2-11", 1, 0, 0}, Row{"level11", "1-12", 3, 0, 0}, Row{"level12", "1-13", 0, 1, 0},
                           Row{"level13b", "3-14", 1, 1, 0}, Row{"level15a", "2-16", 0, 1, 0},
                           Row{"level17a", "2-18", 1, 0, 0}, Row{"level19", "1-20", 0, 1, 0},
                           Row{"level20", "1-21", 0, 2, 0}, Row{"level22", "1-23", 0, 1, 0},
                           Row{"level22c", "4-23", 0, 1, 0}, Row{"level24a", "2-25", 0, 1, 0},
                           Row{"level24b", "3-25", 0, 1, 0}, Row{"level25b", "3-26", 1, 0, 0},
                           Row{"level26a", "2-27", 0, 0, 1}, Row{"level28", "1-29", 0, 1, 0},
                           Row{"level2b", "3-03", 0, 1, 0}, Row{"level4b", "3-05", 0, 1, 0},
                           Row{"level9a", "2-10", 1, 0, 0}, Row{"level9c", "4-10", 1, 0, 0}}) {
        Run run;
        if (!Begin(row.level, run)) return;
        int h = 0;
        int v = 0;
        int n = 0;
        for (const Portals::Reflector& r : run.level.portals.reflectors) {
            CHECK_MSG(r.scale == 1.0, std::string(row.world) + " " + r.name + " carries a scale of its own");
            h += r.plane == Shot::Plane::Horizontal ? 1 : 0;
            v += r.plane == Shot::Plane::Vertical ? 1 : 0;
            n += r.plane == Shot::Plane::Snap ? 1 : 0;
        }
        CHECK_MSG(h == row.horizontal && v == row.vertical && n == row.planeless,
                  std::string(row.world) + " (" + row.level + "): " + std::to_string(h) + " horizontal, " +
                      std::to_string(v) + " vertical, " + std::to_string(n) + " with no plane");
        horizontal += h;
        vertical += v;
        planeless += n;
    }
    CHECK_EQ(horizontal, 9);
    CHECK_EQ(vertical, 13);
    CHECK_EQ(planeless, 1);
}

void TheMirrorLawIsTheListings() {
    // reflect (utilMath 315805..315988): dir - 2 (dir . normal) normal. The normal's sign never matters, its axis does.
    const glm::dvec2 dir(0.6, 0.8);
    CHECK_MSG(glm::distance(Shot::Reflect(dir, {1.0, 0.0}), glm::dvec2(-0.6, 0.8)) < 1e-12, "a vertical mirror flips x");
    CHECK_MSG(glm::distance(Shot::Reflect(dir, {-1.0, 0.0}), glm::dvec2(-0.6, 0.8)) < 1e-12, "whichever way its normal points");
    CHECK_MSG(glm::distance(Shot::Reflect(dir, {0.0, -1.0}), glm::dvec2(0.6, -0.8)) < 1e-12, "a horizontal one flips y");

    // computeReflectVector (348584..349003): vertical is (+-1, 0) by the sign of v.x, any other plane (0, +-1) by v.y's,
    // a zero going to +1.
    CHECK(Shot::ReflectNormal(Shot::Plane::Vertical, {-0.3, 0.9}) == glm::dvec2(-1.0, 0.0));
    CHECK(Shot::ReflectNormal(Shot::Plane::Vertical, {0.3, 0.9}) == glm::dvec2(1.0, 0.0));
    CHECK(Shot::ReflectNormal(Shot::Plane::Vertical, {0.0, -1.0}) == glm::dvec2(1.0, 0.0));
    CHECK(Shot::ReflectNormal(Shot::Plane::Horizontal, {0.9, -0.3}) == glm::dvec2(0.0, -1.0));
    CHECK(Shot::ReflectNormal(Shot::Plane::Horizontal, {0.9, 0.3}) == glm::dvec2(0.0, 1.0));
    CHECK(Shot::ReflectNormal(Shot::Plane::Horizontal, {-1.0, 0.0}) == glm::dvec2(0.0, 1.0));

    // getRightAngleVector (316280..316768): getAngle(v) = atan2(v.x, v.y) from 0 to 2 PI, snapped to the nearest of four
    // axes: a vector toward +y (angle 0) is (0, 1), toward +x (PI/2) is (1, 0), toward -y (PI) is (0, -1) and toward -x
    // (3 PI/2) is (-1, 0), each owning the 90 degrees round it.
    CHECK(Shot::SnapToAxis({0.0, 1.0}) == glm::dvec2(0.0, 1.0));
    CHECK(Shot::SnapToAxis({1.0, 0.0}) == glm::dvec2(1.0, 0.0));
    CHECK(Shot::SnapToAxis({0.0, -1.0}) == glm::dvec2(0.0, -1.0));
    CHECK(Shot::SnapToAxis({-1.0, 0.0}) == glm::dvec2(-1.0, 0.0));
    CHECK_MSG(Shot::SnapToAxis({0.9, 0.5}) == glm::dvec2(1.0, 0.0), "61 degrees round from +y is the +x axis's");
    CHECK_MSG(Shot::SnapToAxis({0.5, 0.9}) == glm::dvec2(0.0, 1.0), "29 degrees is +y's");
    CHECK_MSG(Shot::SnapToAxis({0.5, -0.9}) == glm::dvec2(0.0, -1.0), "151 degrees is -y's");
    CHECK_MSG(Shot::SnapToAxis({-0.9, -0.5}) == glm::dvec2(-1.0, 0.0), "241 degrees is -x's");
    CHECK_MSG(Shot::SnapToAxis({-0.5, 0.9}) == glm::dvec2(0.0, 1.0), "331 degrees is +y's again");
    CHECK_MSG(Shot::SnapToAxis({0.0, 0.0}) == glm::dvec2(0.0, 1.0), "a shot exactly on the point: angle 0");
}

void TheCatchIsAPositionSampleWithinSizeX() {
    // size.x is the sprite's frame x the entity's scale: reflect_agent.png is 32 x 32 at Scale 1.0 (the port's 16 was a
    // guess), and a shot comes off one reflector at most (reflect 0 on the shot it adds).
    Rig numbers;
    if (!MakeRig(numbers)) return;
    CHECK_EQ(numbers.state.shot.reflectSizeUnits, 32.0);
    CHECK_EQ(numbers.state.shot.maxReflections, 1);

    // A vertical reflector 31 units off the line of a shot fired along y 200 is passed by a sample inside 32 (the window
    // is sqrt(32^2 - 31^2) = 7.9 units each side of it, wider than the 3.2 a callback moves), 33 units off never: the
    // catch is a circle of 32 and not the old 16.
    for (const double off : {31.0, 33.0}) {
        Rig rig;
        if (!MakeRig(rig)) return;
        const glm::dvec2 at(260.0, 200.0 - off);
        rig.state.reflectors.push_back(Mirror("mirror", at, Shot::Plane::Vertical));
        CHECK(rig.state.Shoot(rig.registry, glm::dvec2(400.0, 200.0)));
        const TurnSeen seen = FlyAndWatch(rig);
        CHECK_MSG(rig.state.placed.size() == 1, std::to_string(off) + " off: " + rig.state.lastFailure);
        if (rig.state.placed.empty()) continue;
        if (off < 32.0) {
            // The leg is the way from the shooter to the reflector, flipped in x, from the reflector.
            const glm::dvec2 expected(at.x - glm::distance(glm::dvec2(100.0, 200.0), at), at.y);
            CHECK_MSG(seen.turns == 1 && glm::distance(rig.state.placed[0].basePx, expected) < 1e-9,
                      "31 off is caught: it opens at " + At(rig.state.placed[0].basePx) + " for " + At(expected));
            CHECK_MSG(glm::distance(seen.first.atPx, glm::dvec2(SampleX(rig, 100.0, 41), 200.0)) < 1e-9 &&
                          glm::distance(seen.first.atPx, at) < 32.0,
                      "on the 41st callback, 255.2 along, " + At(seen.first.atPx));
        } else {
            CHECK_MSG(seen.turns == 0 && glm::distance(rig.state.placed[0].basePx, glm::dvec2(400.0, 200.0)) < 1e-9,
                      "33 off is not: it opens at the tap, " + At(rig.state.placed[0].basePx));
        }
    }
}

void TheCatchIsStrictlyBelowSizeXAndSizeIsTheEntitysScaled() {
    // CMPf v6, v8 / JNS -> 267 (#83-84) returns when squaredDistance >= size.x x size.x: a sample EXACTLY 32.0 from the
    // agent is not caught, one a hair inside is. The reflector stands straight above the 40th callback's sample, 32.0 up
    // (the sample's x is 252.0 to within 1e-13, so its dx squared is lost in 1024.0): a `<=` would turn the shot there, the
    // listing's `<` lets it by and it opens at the tap. 31.999 up turns it on that callback (the 40th Tick after the Shoot).
    for (const double up : {32.0, 31.999}) {
        Rig rig;
        if (!MakeRig(rig)) return;
        rig.state.reflectors.push_back(
            Mirror("mirror", glm::dvec2(SampleX(rig, 100.0, 40), 200.0 - up), Shot::Plane::Vertical));
        CHECK(rig.state.Shoot(rig.registry, glm::dvec2(400.0, 200.0)));
        const TurnSeen seen = FlyAndWatch(rig);
        CHECK_MSG(rig.state.placed.size() == 1, std::to_string(up) + " up: " + rig.state.lastFailure);
        if (rig.state.placed.empty()) continue;
        if (up == 32.0) {
            CHECK_MSG(seen.turns == 0 && glm::distance(rig.state.placed[0].basePx, glm::dvec2(400.0, 200.0)) < 1e-9,
                      "exactly size.x away is not caught: it opens at the tap, " + At(rig.state.placed[0].basePx));
        } else {
            CHECK_MSG(seen.turns == 1 && seen.tick == 40,
                      "31.999 away is caught on the 40th callback: tick " + std::to_string(seen.tick));
        }
    }
    // size = GetSize() = the sprite's frame x the placement's own scale (ETHSpriteEntity::GetCurrentSize): a reflector
    // placed at scale 2 catches within 64, so one 63 off turns the shot and one 65 off does not.
    for (const double off : {63.0, 65.0}) {
        Rig rig;
        if (!MakeRig(rig)) return;
        rig.state.reflectors.push_back(Portals::Reflector{"big", glm::dvec2(260.0, 200.0 - off), Shot::Plane::Vertical, 2.0});
        CHECK(rig.state.Shoot(rig.registry, glm::dvec2(400.0, 200.0)));
        const TurnSeen seen = FlyAndWatch(rig);
        CHECK_MSG(rig.state.placed.size() == 1, std::to_string(off) + " off at scale 2: " + rig.state.lastFailure);
        CHECK_MSG(seen.turns == (off < 64.0 ? 1 : 0), std::to_string(off) + " off at scale 2: " + std::to_string(seen.turns) +
                                                          " turn(s)");
    }
}

void TheFirstSampleWithinSizeXTurnsTheShotFromTheReflector() {
    // A vertical reflector at (200, 200), a shot fired along y 200 from x 100 toward (400, 200). The sample of the 13th
    // callback, 165.6, is 34.4 away; the 14th's, 168.8, is 31.2: it is caught there, 14 Ticks after the Shoot. The new
    // shot is spawned AT THE REFLECTOR and flies min(300, |(100, 200) - (200, 200)| = 100) back along -x: it opens at
    // (100, 200), after the 24 Ticks a flight of 100 takes (a jump of 24 from the reflector, 3.2 a Tick).
    Rig rig;
    if (!MakeRig(rig)) return;
    rig.state.reflectors.push_back(Mirror("mirror", glm::dvec2(200.0, 200.0), Shot::Plane::Vertical));
    CHECK(rig.state.Shoot(rig.registry, glm::dvec2(400.0, 200.0)));
    CHECK(200.0 - SampleX(rig, 100.0, 13) >= 32.0 && 200.0 - SampleX(rig, 100.0, 14) < 32.0);
    const TurnSeen seen = FlyAndWatch(rig);
    CHECK_EQ(seen.tick, 14);
    CHECK_EQ(seen.turns, 1);
    CHECK_MSG(glm::distance(seen.first.atPx, glm::dvec2(168.8, 200.0)) < 1e-9, "caught at " + At(seen.first.atPx));
    CHECK_MSG(seen.first.agentPx == glm::dvec2(200.0, 200.0), "the new shot is spawned at the reflector");
    CHECK_MSG(glm::distance(seen.first.toPx, glm::dvec2(100.0, 200.0)) < 1e-9, "newDest " + At(seen.first.toPx));
    CHECK_MSG(rig.state.placed.size() == 1 && glm::distance(rig.state.placed[0].basePx, glm::dvec2(100.0, 200.0)) < 1e-9,
              "it opens at newDest");
    CHECK_EQ(seen.ticks, 14 + FlightTicks(rig.state.shot, 100.0, 16.0));
    CHECK_EQ(rig.state.reflections, 1);
}

void TheLegIsTheLessOfTheWayFromItsCasterAndTheLengthItWasFired() {
    // dist = min(sqrt(squaredDistance), distance(casterPos, agent)) (#148-180). The stored squaredDistance is the whole
    // length the shot was fired (never decremented), so a shot caught after a few steps does not "go on for the rest of
    // its way": it goes the whole of it, or back the whole way it came. Both are shown on the head-on case above.
    struct Case {
        double tapX;     // the tap, and so the length fired
        double expectedX; // where the new shot opens, from the reflector at x 200 going -x
        const char* what;
    };
    for (const Case& c : {Case{400.0, 100.0, "fired 300, the reflector 100 from the shooter: 100 back"},
                          Case{190.0, 110.0, "fired 90, the reflector 100 from the shooter: 90 back"}}) {
        Rig rig;
        if (!MakeRig(rig)) return;
        rig.state.reflectors.push_back(Mirror("mirror", glm::dvec2(200.0, 200.0), Shot::Plane::Vertical));
        CHECK(rig.state.Shoot(rig.registry, glm::dvec2(c.tapX, 200.0)));
        const TurnSeen seen = FlyAndWatch(rig);
        CHECK_MSG(seen.turns == 1 && rig.state.placed.size() == 1 &&
                      glm::distance(rig.state.placed[0].basePx, glm::dvec2(c.expectedX, 200.0)) < 1e-9,
                  std::string(c.what) + ": " +
                      (rig.state.placed.empty() ? rig.state.lastFailure : At(rig.state.placed[0].basePx)));
    }
}

void NoCatchOnTheArrivalTick() {
    // The shot deletes itself in its own callback on the frame it passes its destiny, before the static reflector looks:
    // a reflector 31.0 from the arrival sample catches nothing, and one 3.2 units further along the shot's way - the
    // destiny moved from 100 to 103 - catches the sample before it. Fired along y 200 from x 100 to x 200 (arrival on the
    // 24th callback, 200.8, its predecessor 197.6 being 34.2 from the reflector at 231.8) or to x 203 (arrival on the
    // 25th, 204.0; the 24th's 200.8 is 31.0 away: caught, 24 Ticks after the Shoot).
    for (const double length : {100.0, 103.0}) {
        Rig rig;
        if (!MakeRig(rig)) return;
        rig.state.reflectors.push_back(Mirror("mirror", glm::dvec2(231.8, 200.0), Shot::Plane::Vertical));
        CHECK(rig.state.Shoot(rig.registry, glm::dvec2(100.0 + length, 200.0)));
        CHECK(231.8 - SampleX(rig, 100.0, 23) >= 32.0 && 231.8 - SampleX(rig, 100.0, 24) < 32.0);
        const TurnSeen seen = FlyAndWatch(rig);
        CHECK_MSG(rig.state.placed.size() == 1, std::to_string(length) + ": " + rig.state.lastFailure);
        if (rig.state.placed.empty()) continue;
        if (length == 100.0) {
            CHECK_MSG(seen.turns == 0 && glm::distance(rig.state.placed[0].basePx, glm::dvec2(200.0, 200.0)) < 1e-9 &&
                          seen.ticks == FlightTicks(rig.state.shot, 100.0, 16.0),
                      "a reflector within reach of the arrival sample only: no turn, the portal opens at the destiny " +
                          At(rig.state.placed[0].basePx));
        } else {
            CHECK_MSG(seen.turns == 1 && seen.tick == 24 &&
                          glm::distance(rig.state.placed[0].basePx, glm::dvec2(231.8 - 103.0, 200.0)) < 1e-9,
                      "the sample before the arrival: turned on the 24th Tick, the new shot goes the 103 fired: " +
                          At(rig.state.placed[0].basePx));
        }
    }
}

void ASpawnBesideAReflectorIsCaughtOnTheTapsFrame() {
    // A reflector's static callback runs on the frame the tap fires on, and sees the shot at its spawn: the shooter's
    // point. One within 32 of it catches the shot at once - on the first Tick after the Shoot, which runs the tap's frame
    // as its spawn pass - and the new shot, spawned at the reflector, goes min(200 fired, 10 from the shooter to it).
    {
        Rig rig;
        if (!MakeRig(rig)) return;
        rig.state.reflectors.push_back(Mirror("mirror", glm::dvec2(110.0, 200.0), Shot::Plane::Horizontal));
        CHECK(rig.state.Shoot(rig.registry, glm::dvec2(300.0, 200.0)));
        const TurnSeen seen = FlyAndWatch(rig);
        CHECK_MSG(seen.tick == 1 && seen.turns == 1, "caught on the first Tick, at the spawn");
        CHECK_MSG(glm::distance(seen.first.atPx, glm::dvec2(100.0, 200.0)) < 1e-3 &&
                      seen.first.agentPx == glm::dvec2(110.0, 200.0),
                  "the old shot where it was spawned, the new one at the reflector: " + At(seen.first.atPx));
        CHECK_MSG(rig.state.placed.size() == 1 && glm::distance(rig.state.placed[0].basePx, glm::dvec2(120.0, 200.0)) < 1e-9,
                  "it opens 10 on from the reflector: " +
                      (rig.state.placed.empty() ? rig.state.lastFailure : At(rig.state.placed[0].basePx)));
        CHECK_EQ(seen.ticks, 1);
    }
    // 35 away it is not caught at the spawn, and the first callback's sample (127.2) is 7.8 from it: caught there.
    {
        Rig rig;
        if (!MakeRig(rig)) return;
        rig.state.reflectors.push_back(Mirror("mirror", glm::dvec2(135.0, 200.0), Shot::Plane::Horizontal));
        CHECK(rig.state.Shoot(rig.registry, glm::dvec2(300.0, 200.0)));
        const TurnSeen seen = FlyAndWatch(rig);
        CHECK_MSG(seen.turns == 1 && seen.tick == 1 && glm::distance(seen.first.atPx, glm::dvec2(127.2, 200.0)) < 1e-9,
                  "caught by the first callback's sample, not the spawn: " + At(seen.first.atPx));
    }
}

void AReflectedShotIsNeverTurnedAgain() {
    // The shot a reflector adds has reflect 0 (#224-227) and every reflector returns on a shot with none (#39-54): the
    // shot comes off one reflector at most. A horizontal reflector at (200, 100) sends the shot of (100, 200) -> (300, 0)
    // toward (300, 200); a second one at (250, 150) stands ON that leg (70.7 from the first, 70.7 off the first leg's
    // line) and lets it through. The same second reflector alone catches a fresh shot fired along that leg.
    {
        Rig rig;
        if (!MakeRig(rig)) return;
        rig.state.reflectors.push_back(Mirror("first", glm::dvec2(200.0, 100.0), Shot::Plane::Horizontal));
        rig.state.reflectors.push_back(Mirror("second", glm::dvec2(250.0, 150.0), Shot::Plane::Horizontal));
        CHECK(rig.state.Shoot(rig.registry, glm::dvec2(300.0, 0.0)));
        const TurnSeen seen = FlyAndWatch(rig);
        CHECK_MSG(seen.turns == 1 && rig.state.reflections == 1, "turned once, by the first");
        CHECK_MSG(rig.state.placed.size() == 1 && glm::distance(rig.state.placed[0].basePx, glm::dvec2(300.0, 200.0)) < 1e-6,
                  "and through the second to the end of its leg: " +
                      (rig.state.placed.empty() ? rig.state.lastFailure : At(rig.state.placed[0].basePx)));
        CHECK_MSG(seen.first.agentPx == glm::dvec2(200.0, 100.0), "the first's");
    }
    {
        Rig rig;
        if (!MakeRig(rig)) return;
        PutShooter(rig, glm::dvec2(200.0, 100.0));
        rig.state.reflectors.push_back(Mirror("second", glm::dvec2(250.0, 150.0), Shot::Plane::Horizontal));
        CHECK(rig.state.Shoot(rig.registry, glm::dvec2(300.0, 200.0)));
        const TurnSeen seen = FlyAndWatch(rig);
        // Fired 141.4, the reflector 70.7 from the shooter: back up 70.7 from it, to (300, 100).
        CHECK_MSG(seen.turns == 1 && rig.state.placed.size() == 1 &&
                      glm::distance(rig.state.placed[0].basePx, glm::dvec2(300.0, 100.0)) < 1e-6,
                  "a fresh shot along the same leg is turned by it: " +
                      (rig.state.placed.empty() ? rig.state.lastFailure : At(rig.state.placed[0].basePx)));
    }
}

void APlanelessReflectorTurnsTheShotOffTheSideItCameFrom() {
    // 2-27's reflector has no plane: computeReflectVector hands the shot's side of it, normalize(shot - reflector), to
    // getRightAngleVector, which snaps it to an axis, and the shot's x or y flips accordingly. From the left it is a
    // vertical mirror's turn; from above a horizontal one's. The reflector at (200, 200), 100 and 111.8 from the shooter.
    struct Case {
        glm::dvec2 shooter;
        glm::dvec2 tap;
        Shot::Plane plane;
        glm::dvec2 expected;
        const char* what;
    };
    const glm::dvec2 left = glm::dvec2(200.0, -50.0) / std::hypot(200.0, 50.0); // (100, 200) toward (300, 150)
    const glm::dvec2 fromLeft = glm::dvec2(200.0, 200.0) + glm::dvec2(-left.x, left.y) * 100.0;
    for (const Case& c : {Case{{100.0, 200.0}, {300.0, 150.0}, Shot::Plane::Snap, fromLeft, "from the left: x flips"},
                          Case{{100.0, 200.0}, {300.0, 150.0}, Shot::Plane::Vertical, fromLeft, "a vertical plane: the same"},
                          Case{{100.0, 200.0}, {300.0, 150.0}, Shot::Plane::Horizontal,
                               glm::dvec2(200.0, 200.0) + glm::dvec2(left.x, -left.y) * 100.0,
                               "a horizontal plane flips y instead"},
                          Case{{150.0, 100.0}, {250.0, 300.0}, Shot::Plane::Snap, {250.0, 100.0}, "from above: y flips"},
                          Case{{150.0, 100.0}, {250.0, 300.0}, Shot::Plane::Vertical, {150.0, 300.0},
                               "a vertical plane would flip x"}}) {
        Rig rig;
        if (!MakeRig(rig)) return;
        PutShooter(rig, c.shooter);
        rig.state.reflectors.push_back(Mirror("mirror", glm::dvec2(200.0, 200.0), c.plane));
        CHECK(rig.state.Shoot(rig.registry, c.tap));
        const TurnSeen seen = FlyAndWatch(rig);
        CHECK_MSG(seen.turns == 1 && rig.state.placed.size() == 1 &&
                      glm::distance(rig.state.placed[0].basePx, c.expected) < 1e-6,
                  std::string(c.what) + ": " +
                      (rig.state.placed.empty() ? rig.state.lastFailure : At(rig.state.placed[0].basePx)) + " for " +
                      At(c.expected));
    }
}

void TheTurnIsRecordedForItsPictures() {
    // The head-on case again, watched for what the layer draws: the first leg launches from the shooter on the first Tick,
    // the turn is recorded on the 14th with the OLD shot's flash point (where it was caught), the new leg's mark at its
    // clipped end (portal_mark: nothing in its way) and red_waves nowhere; the new leg launches on the Tick after, from
    // the reflector, toward its end.
    Rig rig;
    if (!MakeRig(rig)) return;
    rig.state.reflectors.push_back(Mirror("mirror", glm::dvec2(200.0, 200.0), Shot::Plane::Vertical));
    CHECK(rig.state.Shoot(rig.registry, glm::dvec2(400.0, 200.0)));
    const TurnSeen seen = FlyAndWatch(rig);
    CHECK_EQ(seen.launches.size(), std::size_t{2});
    if (seen.launches.size() != 2) return;
    CHECK_MSG(seen.launches[0].first == 1 && seen.launches[0].second.fromPx == glm::dvec2(100.0, 200.0),
              "the first leg launches on the first callback, from the shooter");
    CHECK_MSG(seen.launches[1].first == seen.tick + 1 && seen.launches[1].second.fromPx == seen.first.agentPx &&
                  glm::distance(seen.launches[1].second.towardPx, seen.first.said.atPx) < 1e-12,
              "the new leg launches on the Tick after the turn, from the reflector, toward its clipped end");
    CHECK_MSG(!seen.first.said.willDie && !seen.first.said.redWaves && seen.first.said.atPx == seen.first.toPx,
              "nothing in the new leg's way: portal_mark at its end, " + At(seen.first.said.atPx));
    // The rig's lists are one Tick's: the Tick after the shot is over has none.
    rig.state.Tick(rig.registry, kFloored);
    CHECK(rig.state.launches.empty() && rig.state.reflected.empty() && rig.state.failures.empty());
}

void OneTwelvesReflectorSendsTheShotToNumbersWorkedOutByHand() {
    // 1-12 (level11), reflect_agent_761 at (180, 140), horizontal, with the player at the level's start point (73, 204) and
    // a tap on the line through it, at 1.5 times the way there: (233.5, 108). The numbers are worked out from the listing
    // by hand, not read back from the sim:
    //  - the tap-time ray meets the underside of wall04_ent_759 (x 148..212, down to y 128) at x = 73 + 76/96 x 160.5
    //    = 200.0625 (the line is y = 204 - 0.5981 (x - 73)), so the first leg is aimed at (200.0625, 128), 148.06 long,
    //    and is doomed there - it never gets so far;
    //  - a sample of it within 32 of the reflector catches it (from 94.4 along: 30.3 away); the reflector is 124.68 from
    //    the player, which is less than 148.06, so dist = 124.68, and the horizontal plane flips the heading's y:
    //    newDest = (180, 140) + (0.8582, +0.5133) x 124.68 = (287, 204), exactly (107, 64) on from the reflector;
    //  - nothing stands on the way from (188, 144.6) to it (the platform's top is y 224, single_block_plat_ent_760 is
    //    x 164..196 from y 190), no field holds (287, 204), and a reflected shot takes no doom from a wall (#150-156):
    //    portal_mark there, and a portal opens at (287, 204).
    Run run;
    if (!Begin("level11", run)) return;
    PutPlayer(run, glm::dvec2(73.0, 204.0));
    CHECK(run.level.portals.Shoot(run.registry, glm::dvec2(233.5, 108.0)));
    if (!run.level.portals.flight) return;
    const glm::dvec2 first = run.level.portals.flight->toPx;
    CHECK_MSG(glm::distance(first, glm::dvec2(200.0 + 1.0 / 16.0, 128.0)) < 1e-3 && run.level.portals.flight->said.willDie,
              "the first leg is aimed at the wall's underside, doomed: " + Px(first));
    int turnTick = -1;
    Portals::Reflection turn;
    for (int tick = 1; tick <= 400 && run.level.portals.flight; ++tick) {
        Tick(run);
        if (turnTick < 0 && !run.level.portals.reflected.empty()) {
            turnTick = tick;
            turn = run.level.portals.reflected.front();
        }
    }
    CHECK_MSG(turnTick > 0, "the shot was turned: " + run.level.portals.lastFailure);
    if (turnTick < 0) return;
    CHECK_MSG(glm::distance(turn.agentPx, glm::dvec2(180.0, 140.0)) < 1e-3, "by reflect_agent_761: " + Px(turn.agentPx));
    CHECK_MSG(glm::distance(turn.atPx, glm::dvec2(180.0, 140.0)) < 32.0, "caught within 32: " + Px(turn.atPx));
    CHECK_MSG(glm::distance(turn.toPx, glm::dvec2(287.0, 204.0)) < 1e-3, "newDest " + Px(turn.toPx));
    CHECK_MSG(glm::distance(turn.said.atPx, glm::dvec2(287.0, 204.0)) < 1e-3 && !turn.said.willDie && !turn.said.redWaves,
              "clipped to itself, not doomed: portal_mark at " + Px(turn.said.atPx));
    CHECK_MSG(run.level.portals.placed.size() == 1 &&
                  glm::distance(run.level.portals.placed[0].basePx, glm::dvec2(287.0, 204.0)) < 1e-3,
              "a portal opens there: " +
                  (run.level.portals.placed.empty() ? run.level.portals.lastFailure
                                                   : Px(run.level.portals.placed[0].basePx)));
}

void TheRecoilAimsAtFinalPos() {
    // managePortalInsertion #158-166: applyImpulse(finalPos). The retry here clears a low static box and meets a
    // crate on its own, higher line, so finalPos is 10 past that crate's face - a few degrees off the tap's
    // direction, which the recoil follows.
    Rig rig;
    if (!MakeRig(rig)) return;
    rig.registry.emplace<RigidBodyComponent>(rig.state.shooter);
    rig.state.noGravity = true;
    rig.state.recoilMps = 2.0;
    PutBox(rig, "low", glm::dvec2(150.0, 205.0), glm::dvec2(20.0, 20.0), false);
    PutBox(rig, "crate", glm::dvec2(250.0, 190.0), glm::dvec2(20.0, 20.0), true);
    const glm::dvec2 from(100.0, 200.0);
    const glm::dvec2 tap(300.0, 200.0);
    CHECK(rig.state.Shoot(rig.registry, tap));
    CHECK_MSG(rig.state.lastTap.said.retried && !rig.state.lastTap.said.willDie && rig.state.flight &&
                  rig.state.flight->toPx != tap && rig.state.flight->toPx.x > 240.0 && rig.state.flight->toPx.x < 260.0,
              "the retry's ray ends on the crate, not at the tap: " +
                  Px(rig.state.flight ? rig.state.flight->toPx : glm::dvec2(0.0)));
    if (!rig.state.flight) return;
    const glm::vec3 pushed = rig.registry.get<RigidBodyComponent>(rig.state.shooter).velocity;
    const glm::vec3 atFinal = Zerog::Impulse(from, rig.state.flight->toPx, 2.0);
    const glm::vec3 atTap = Zerog::Impulse(from, tap, 2.0);
    CHECK_MSG(glm::distance(pushed, atFinal) < 1e-6, "the recoil is away from finalPos");
    CHECK_MSG(glm::distance(pushed, atTap) > 1e-3, "and not away from the tap, which the retry moved it off");
}

void AnArrivalAsksTheFieldOfTheShotsPosition() {
    // G4-8. A doomed shot's portal_fail is portal_fail_in_antiportal (and a shake) when the shot's POSITION after
    // its last advance is in a field (ETHCallback_projectile #201-211) - up to one advance past the destiny, which
    // the port asked of the destiny itself. The wall's face is at x 300.5; the callbacks are at 124 + 3.2 m, so the
    // shot arrives on m = 56 at 303.2, 2.7 past it; the field's edge lies 1.5 beyond the face.
    Rig probe;
    if (!MakeRig(probe)) return;
    const double radius = probe.state.rules.antiportalRadiusPx;
    probe.state.zones.push_back(Portals::NoPortalZone{glm::dvec2(300.5 + radius + 1.5, 200.0), 1.0, "ring", 0.0, std::string()});
    CHECK_MSG(!probe.state.InField(glm::dvec2(300.5, 200.0)) && probe.state.InField(glm::dvec2(303.2, 200.0)),
              "the destiny is outside the field and the overshot position inside it");
    Rig watched;
    if (!MakeRig(watched)) return;
    PutBox(watched, "wall", glm::dvec2(310.5, 200.0), glm::dvec2(20.0, 100.0), false);
    watched.state.zones.push_back(Portals::NoPortalZone{glm::dvec2(300.5 + radius + 1.5, 200.0), 1.0, "ring", 0.0, std::string()});
    CHECK(watched.state.Shoot(watched.registry, glm::dvec2(450.0, 200.0)));
    CHECK_MSG(watched.state.flight && Near(watched.state.flight->toPx, glm::dvec2(300.5, 200.0)), "aimed at the face");
    std::vector<Portals::Failure> ends;
    for (int tick = 0; tick < 600 && watched.state.flight; ++tick) {
        watched.state.Tick(watched.registry, kFloored);
        for (const Portals::Failure& f : watched.state.failures) ends.push_back(f);
    }
    CHECK_MSG(ends.size() == 1 && Near(ends[0].atPx, glm::dvec2(300.5, 200.0)) && ends[0].kind == Portals::FailKind::Body &&
                  ends[0].inField,
              "drawn at the destiny, and in the field by the position the shot had reached");
}

void ANonDoomedArrivalOpensWhereTheRayEnded() {
    // insertPortal has no field test: the ray tested the CONTACT, and the portal opens 10 units past it. A crate whose
    // face (150) is outside a field and whose 10-in point (160) is inside it: the tap is in the crate, so no retry.
    Rig rig;
    if (!MakeRig(rig)) return;
    const double radius = rig.state.rules.antiportalRadiusPx;
    CHECK_MSG(radius > 25.0 && radius < 35.0, "the field's radius, " + std::to_string(radius) + ", puts 150 out and 160 in");
    PutBox(rig, "crate", glm::dvec2(160.0, 200.0), glm::dvec2(20.0, 20.0), true);
    rig.state.zones.push_back(Portals::NoPortalZone{glm::dvec2(185.0, 200.0), 1.0, "ring", 0.0, std::string()});
    const glm::dvec2 tap(165.0, 200.0);
    const Portals::FinalPos said = rig.state.Predict(rig.registry, glm::dvec2(100.0, 200.0), tap);
    CHECK_MSG(!said.willDie && Near(said.atPx, glm::dvec2(160.0, 200.0)), "not doomed: " + Px(said.atPx));
    CHECK(rig.state.Shoot(rig.registry, tap));
    FlyFloored(rig);
    CHECK_MSG(rig.state.placed.size() == 1 && Near(rig.state.placed[0].basePx, glm::dvec2(160.0, 200.0)) &&
                  rig.state.shotsFailed == 0,
              "the portal opens inside the field: " + rig.state.lastFailure);
    // A portal put there by hand is still refused: TryPlace is the port's own test for the suites.
    Rig by_hand;
    if (!MakeRig(by_hand)) return;
    by_hand.state.zones.push_back(Portals::NoPortalZone{glm::dvec2(185.0, 200.0), 1.0, "ring", 0.0, std::string()});
    CHECK(!by_hand.state.TryPlace(glm::dvec2(160.0, 200.0)) && by_hand.state.Insert(glm::dvec2(160.0, 200.0)));
}

// A grid of taps on real levels: whatever the ray says at the tap is what the shot does where it arrives. Every
// tap fires from the same settled player, and the level is left as it is between shots. A shot that a static
// callback took (an agent, a flame, a reflector's turn, a blocker) is counted apart: its end was not the ray's.
void EveryShotEndsAsTheRaySaidItWould() {
    int doomed = 0;
    int opened = 0;
    int taken = 0;
    int retried = 0;
    int fieldFlags = 0;
    for (const char* levelName : {"level6", "level8", "level11", "level12", "level3", "level5"}) {
        Run run;
        if (!Begin(levelName, run)) return;
        for (int tick = 0; tick < 40; ++tick) Tick(run);
        Portals::State& portals = run.level.portals;
        const glm::dvec2 from = PlayerPx(run);
        int tapsOfLevel = 0;
        for (const double dx : {-220.0, -90.0, -30.0, 30.0, 90.0, 220.0}) {
            for (const double dy : {-120.0, -50.0, 0.0, 50.0, 120.0}) {
                portals.KillAll(false);
                portals.flight.reset();
                const glm::dvec2 tap = from + glm::dvec2(dx, dy);
                const glm::dvec2 stood = PlayerPx(run);
                if (!portals.Shoot(run.registry, tap)) continue;
                ++tapsOfLevel;
                const Portals::FinalPos said = portals.lastTap.said;
                const Portals::FinalPos again = portals.Predict(run.registry, portals.flight->fromPx, tap);
                CHECK_MSG(again.atPx == said.atPx && again.willDie == said.willDie,
                          std::string(levelName) + " " + Px(tap) + ": the ray gives one answer");
                CHECK_MSG(portals.flight && portals.flight->toPx == said.atPx && portals.flight->said.willDie == said.willDie,
                          std::string(levelName) + " " + Px(tap) + ": the shot is aimed by the ray's answer");
                std::vector<Portals::Failure> ends;
                bool interfered = false;
                const std::size_t before = portals.placed.size();
                for (int tick = 0; tick < 700 && portals.flight; ++tick) {
                    Tick(run);
                    for (const Portals::Failure& f : portals.failures) ends.push_back(f);
                    if (!portals.agentKills.empty() || !portals.burns.empty() || !portals.reflected.empty()) interfered = true;
                }
                CHECK_MSG(!portals.flight, std::string(levelName) + " " + Px(tap) + ": the shot ended within 700 ticks");
                if (said.retried) ++retried;
                const std::string where = std::string(levelName) + " " + Px(tap) + " from " + Px(stood);
                if (interfered) {
                    ++taken;
                    continue;
                }
                if (!ends.empty() && ends.back().kind == Portals::FailKind::Blocker) {
                    ++taken;
                    continue;
                }
                if (ends.empty()) {
                    // A portal opened: where the ray ended, and the ray had not doomed it. (A torch or a fire diamond
                    // may have spent the shot on these levels: none is in the list.)
                    CHECK_MSG(!said.willDie, where + ": a doomed shot opened a portal");
                    CHECK_MSG(portals.placed.size() >= 1 && portals.placed.back().basePx == said.atPx,
                              where + ": the portal is at the ray's end " + Px(said.atPx) + " (" +
                                  std::to_string(before) + " before)");
                    ++opened;
                } else {
                    CHECK_MSG(said.willDie, where + ": a shot the ray did not doom failed at " + Px(ends.back().atPx));
                    CHECK_MSG(ends.size() == 1 && ends[0].atPx == said.atPx,
                              where + ": it fails at the ray's end " + Px(said.atPx) + ", not " + Px(ends.back().atPx));
                    CHECK_MSG(ends[0].kind == (said.closest != entt::null ? Portals::FailKind::Body : Portals::FailKind::Refused),
                              where + ": the kind says whether the ray met a body");
                    if (ends[0].inField) ++fieldFlags;
                    ++doomed;
                }
            }
        }
        std::printf("  %s: %d taps fired" "\n", levelName, tapsOfLevel);
    }
    std::printf("  a grid of taps: %d opened a portal at the ray's end, %d failed there (%d in a field, %d after the head-height retry), "
                "%d taken by a static callback" "\n",
                opened, doomed, fieldFlags, retried, taken);
    CHECK_MSG(opened > 20 && doomed > 20, "the grid exercises both outcomes");
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
    AStoneIsWhereAPortalOpens();
    TheBlockerStopsIt();
    ANoPortalZoneAtTheTapFailsIt();
    NoPortalsNoShot();
    ExactInThePlane();
    TheReflectorsAreTheLevelsOwn();
    TheFinalPosRulesAreTheDecodedOnes();
    TheTapTimeRayOnF3sTaps();
    TheRetryFromOverTheHead();
    TheGrabberHoldsWhatHoldsThePoint();
    AFireDiamondsMarkIsALeftoverByte();
    ATapSaysWhyItWasRefused();
    TheShotGoesToTheRaysEndAndFailsOrOpensThere();
    TheShotMeetsNoBodyOnItsWay();
    AReflectedShotNeverDiesOnAWall();
    TheReflectorsAreTheLevelsOwnCensus();
    TheMirrorLawIsTheListings();
    TheCatchIsAPositionSampleWithinSizeX();
    TheCatchIsStrictlyBelowSizeXAndSizeIsTheEntitysScaled();
    TheFirstSampleWithinSizeXTurnsTheShotFromTheReflector();
    TheLegIsTheLessOfTheWayFromItsCasterAndTheLengthItWasFired();
    NoCatchOnTheArrivalTick();
    ASpawnBesideAReflectorIsCaughtOnTheTapsFrame();
    AReflectedShotIsNeverTurnedAgain();
    APlanelessReflectorTurnsTheShotOffTheSideItCameFrom();
    TheTurnIsRecordedForItsPictures();
    OneTwelvesReflectorSendsTheShotToNumbersWorkedOutByHand();
    TheRecoilAimsAtFinalPos();
    AnArrivalAsksTheFieldOfTheShotsPosition();
    ANonDoomedArrivalOpensWhereTheRayEnded();
    EveryShotEndsAsTheRaySaidItWould();
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
