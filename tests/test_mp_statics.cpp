// Static portals, on the levels that ship them: level0's two pairs, and level1's
// single portal whose partner is the one the player places.
//
// What the files say is pinned exactly: each static portal's index, destiny,
// colour and place, and the level's portal budget. What happens is judged by
// threshold: a traveller comes out within reach of its exit. That static portals
// stay after use is owner testimony (11 September) and portals.json's
// static_portals_persist; the other setting is run too.
//
// Reads the converted levels and the remake's data from outside this repository,
// and skips, saying where it looked, when either is absent.

#include "TestHarness.hpp"

#include "core/Components.hpp"
#include "sim/Game.hpp"
#include "sim/Portals.hpp"
#include "sim/Roles.hpp"
#include "sim/Units.hpp"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <iterator>
#include <string>
#include <system_error>

using namespace MagicPortals;
using Supersonic::RigidBodyComponent;
using Supersonic::TransformComponent;

namespace {

const std::string kLevels = MAGICPORTALS_LEVELS_DIR;
const std::string kData = MAGICPORTALS_DATA_DIR;
constexpr float kStep = 1.0f / 60.0f;

// Where a traveller comes out: at the exit itself, under the original's rule
// (transit.json), plus what one physics step moves it. Judged, not pinned.
constexpr double kArrivalPx = 20.0;

struct Run {
    Game::Data data;
    entt::registry registry;
    Game::Level level;
};

bool Begin(const std::string& levelName, Run& run) {
    std::string error;
    const bool ok = Game::LoadData(kLevels + "/" + levelName + ".tscn", kData,
                                   std::filesystem::temp_directory_path() / "supersonic-test-mp-statics", run.data,
                                   error) &&
                    Game::Start(run.data, run.registry, run.level, error);
    CHECK_MSG(ok, levelName + ": " + error);
    return ok;
}

void Tick(Run& run, float direction = 0.0f) {
    Game::Tick(run.data, run.registry, run.level, direction, kStep);
}

void PutPlayer(Run& run, const glm::dvec2& atPx) {
    auto& transform = run.registry.get<TransformComponent>(run.level.player);
    const glm::vec3 at = Units::ToWorld(atPx.x, atPx.y);
    transform.position = glm::vec3(at.x, at.y, transform.position.z);
    run.registry.get<RigidBodyComponent>(run.level.player).velocity = glm::vec3(0.0f);
}

glm::dvec2 PlayerPx(const Run& run) {
    return Units::ToPixels(run.registry.get<TransformComponent>(run.level.player).position);
}

const Portals::Static* StaticWithIndex(const Run& run, int index) {
    for (const Portals::Static& portal : run.level.portals.statics) {
        if (portal.index == index) return &portal;
    }
    return nullptr;
}

std::string Px(const glm::dvec2& at) {
    char text[48];
    std::snprintf(text, sizeof text, "(%.1f, %.1f)", at.x, at.y);
    return text;
}

// ---- What the levels say ------------------------------------------------------

void Level0ShipsTwoPairs() {
    Run run;
    if (!Begin("level0", run)) return;
    // Read off level0.tscn by grep: four portal_static nodes, every one active,
    // and a properties node with max_portals "0".
    struct Expected {
        const char* name;
        int index;
        int destiny;
        const char* colour;
        glm::dvec2 atPx;
    };
    const Expected expected[] = {
        {"portal_static_604", 1, 0, "blue", {74.0, 166.0}},
        {"portal_static_603", 0, 1, "red", {150.0, 74.0}},
        {"portal_static_582", 2, 3, "red", {334.0, 200.0}},
        {"portal_static_583", 3, 2, "blue", {498.0, 200.0}},
    };
    const auto& statics = run.level.portals.statics;
    CHECK_EQ(statics.size(), std::size(expected));
    for (std::size_t i = 0; i < statics.size() && i < std::size(expected); ++i) {
        CHECK_MSG(statics[i].name == expected[i].name, statics[i].name);
        CHECK_EQ(statics[i].index, expected[i].index);
        CHECK(statics[i].hasDestiny);
        CHECK_EQ(statics[i].destiny, expected[i].destiny);
        CHECK_MSG(statics[i].colour == expected[i].colour, statics[i].name + " is " + statics[i].colour);
        CHECK(statics[i].atPx == expected[i].atPx);
        CHECK(statics[i].live);
    }
    // No placement at all: level0 is solved with the portals it ships.
    CHECK_EQ(run.level.portals.budget, 0);
    CHECK(!run.level.portals.TryPlace(glm::dvec2(400.0, 150.0)));
    CHECK_EQ(run.level.portals.portalsUsed, 0);
}

// ---- Travelling ----------------------------------------------------------------

void AStaticPortalSendsThePlayerToItsPartnerAndStays() {
    Run run;
    if (!Begin("level0", run)) return;
    const Portals::Static* from = StaticWithIndex(run, 2);
    const Portals::Static* to = StaticWithIndex(run, 3);
    if (from == nullptr || to == nullptr) return;
    const glm::dvec2 toPx = to->atPx;

    PutPlayer(run, from->atPx);
    Tick(run);
    CHECK_EQ(run.level.portals.traversals, 1);
    const double arrived = glm::distance(PlayerPx(run), toPx);
    CHECK_MSG(arrived < kArrivalPx, "the player is at " + Px(PlayerPx(run)) + ", the exit at " + Px(toPx));
    for (const Portals::Static& portal : run.level.portals.statics) CHECK_MSG(portal.live, portal.name);

    // Put down inside its exit, it does not go back while it stays there - an
    // entry is an edge, as body_entered is - nor once the lockout has run out.
    for (int tick = 0; tick < 90; ++tick) Tick(run);
    CHECK_EQ(run.level.portals.traversals, 1);
}

// The three points the original's teleport hands its two effect calls, written
// down by the tick for the frame to draw (Portals::Traversal, the visuals plan's
// step 7.3): where the traveller STOOD, the end it went in by and the end it came
// out of. vanishEffect is called before the move (teleportToOther #61 before
// #69), so the point it records is the one the sparkles are added at, not where
// the player lands.
void ATraversalWritesDownWhatTheEffectsNeed() {
    Run run;
    if (!Begin("level0", run)) return;
    const Portals::Static* from = StaticWithIndex(run, 2);
    const Portals::Static* to = StaticWithIndex(run, 3);
    if (from == nullptr || to == nullptr) return;

    // Nothing has gone through yet, and a tick that carries nothing leaves nothing.
    Tick(run);
    CHECK(run.level.portals.transits.empty());

    // A step off the portal's own point, so that the direction the suck effect is
    // placed along is not zero and the record cannot be confused with the portal.
    const glm::dvec2 stood(from->atPx.x - 6.0, from->atPx.y);
    PutPlayer(run, stood);
    const glm::dvec2 before = PlayerPx(run);
    Tick(run);
    CHECK_EQ(run.level.portals.traversals, 1);
    CHECK_EQ(run.level.portals.transits.size(), std::size_t(1));
    if (run.level.portals.transits.size() != 1) return;
    const Portals::Traversal& went = run.level.portals.transits.front();
    // Within a unit of where it was put: the tick steps physics before the
    // portals, so a third of a unit of falling is already in the point - which is
    // the point the original's vanishEffect is given too, its callbacks running
    // after the step.
    CHECK_MSG(glm::distance(went.travellerPx, before) < 1.0,
              "the traveller stood at " + Px(went.travellerPx) + ", not " + Px(before));
    CHECK_MSG(glm::distance(went.travellerPx, PlayerPx(run)) > kArrivalPx,
              "and that is where it STOOD, not where it landed: " + Px(PlayerPx(run)));
    CHECK_MSG(went.entryPx == from->atPx, "in by " + Px(went.entryPx) + ", " + from->name);
    CHECK_MSG(went.exitPx == to->atPx, "out of " + Px(went.exitPx) + ", " + to->name);

    // And it is this tick's record: the next tick clears it.
    Tick(run);
    CHECK_MSG(run.level.portals.transits.empty(), "the record is one tick's");
}

// ---- a portal the player opened: its hover and its twenty seconds -------------
//
// ETHCallback_portal's own two arms (placement.json `portal`, the visuals plan's
// 7.4). Both are the SIMULATION's and not the picture's: linearMotion ends in
// SetPosition, and PortalManager::doTeleporting reads that same point.
void APlacedPortalHoversOnOnePoint() {
    // level1 (1-02), the one level whose single placement partners a static
    // portal, and the only chapter-1 level the suite can open a portal on by
    // hand: level0 grants none at all.
    Run run;
    if (!Begin("level1", run)) return;
    Portals::State& portals = run.level.portals;
    const Portals::Rules& rules = portals.rules;
    CHECK_MSG(rules.bob.speed == 1.0 && rules.bob.stride == 2.0 && rules.bob.vertical && rules.bob.axisDeg == 0.0 &&
                  rules.bob.startFrom == 0.0 && rules.bob.startTo == 0.0,
              "portal.ent's own speed 1 and stride 2, vertical, unturned, from angle 0");

    // Over the spawn and clear of the player standing there, as
    // Level1sPortalLeadsToThePlacedOne places one.
    const glm::dvec2 at = PlayerPx(run) + glm::dvec2(0.0, -48.0);
    CHECK_MSG(portals.TryPlace(at), "a portal opens 48 units over the player");
    if (portals.placed.empty()) return;
    CHECK_MSG(portals.placed.front().basePx == at && portals.placed.front().atPx == at,
              "it starts where the shot opened it: " + Px(portals.placed.front().atPx));

    // cos(k / 60) x 2 after k ticks of a sixtieth, from its lowest point (the
    // level's +y is down, and cos 0 is +1). The angle moves BEFORE the offset is
    // read, so the first tick is already off the bottom.
    for (int tick = 1; tick <= 240; ++tick) {
        Tick(run);
        if (portals.placed.empty()) break;
        const Portals::Placed& portal = portals.placed.front();
        const double want = at.y + 2.0 * std::cos(static_cast<double>(tick) * (1.0 / 60.0));
        if (tick == 1 || tick == 94 || tick == 188 || tick == 240) {
            CHECK_MSG(std::fabs(portal.atPx.y - want) < 0.01 && portal.atPx.x == at.x,
                      "tick " + std::to_string(tick) + ": " + Px(portal.atPx) + " for y " + std::to_string(want));
            // ONE POINT: the circle a traveller enters by rides with it.
            const glm::vec3 centre = Units::ToWorld(portal.atPx.x, portal.atPx.y);
            CHECK_MSG(portal.trigger.centre == glm::vec2(centre.x, centre.y),
                      "and its entry circle stands on it, not on where it opened");
        }
        if (tick == 240) {
            // Four seconds in it has passed its highest point and is coming back:
            // a whole turn is 2 PI seconds.
            std::printf("  1-02's placed portal: y %.3f at 4 s, from %.1f (base), stride 2\n", portal.atPx.y, at.y);
        }
    }
    CHECK_MSG(!portals.placed.empty(), "and it is still there four seconds in");
    if (!portals.placed.empty()) {
        CHECK_MSG(portals.placed.front().basePx == at, "with the point it opened at kept as its own");
    }
}

void APlacedPortalPerishesAfterTwentySeconds() {
    Run run;
    if (!Begin("level1", run)) return;
    Portals::State& portals = run.level.portals;
    CHECK_MSG(portals.rules.perishMs == 20000.0, "ETHCallback_portal #65's 20,000 ms");
    const glm::dvec2 at = PlayerPx(run) + glm::dvec2(0.0, -48.0);
    CHECK_MSG(portals.TryPlace(at), "a portal opens");
    if (portals.placed.empty()) return;
    const int used = portals.portalsUsed;

    // The FIRST tick writes zero and adds nothing (#22-53), so the clock is one
    // tick behind the ticks.
    Tick(run);
    CHECK_MSG(portals.placed.size() == 1 && portals.placed.front().ageMs == 0.0,
              "the first tick starts its clock at nothing");
    int went = -1;
    for (int tick = 2; tick <= 1300; ++tick) {
        Tick(run);
        if (portals.placed.empty()) {
            went = tick;
            break;
        }
        if (tick == 1195) {
            CHECK_MSG(portals.placed.front().ageMs > 19800.0 && portals.placed.front().ageMs <= 20000.0,
                      "at 1195 ticks it has lived " + std::to_string(portals.placed.front().ageMs) + " ms");
        }
    }
    CHECK_MSG(went > 0, "it perishes rather than standing for ever");
    // 20,000 ms of sixtieths is 1200 of them, plus the one the clock started on,
    // and the compare is strict - so the tick after that.
    CHECK_MSG(went >= 1200 && went <= 1203, "it went on tick " + std::to_string(went) + " (20,000 ms is 1,201)");
    std::printf("  1-02's placed portal perished on tick %d of a sixtieth: %.3f s\n", went,
                static_cast<double>(went) / 60.0);
    // And the budget it took is free again, as insertPortal's own cap is the size
    // of the manager's array: the count of portals USED is not given back, which
    // is the golden score's and only killAll(true) touches.
    CHECK_MSG(portals.portalsUsed == used, "the count of portals used is not given back");
    CHECK_MSG(portals.TryPlace(at), "and the slot it held is free");
}

void AStoneThroughAStaticPairGoesOnce() {
    // The original puts a traveller down at its exit itself, so a stone comes out
    // of a static portal inside it. Entries are edges, and the lockout covers the
    // tick it arrives in, so it does not go back: it drops out of the exit and
    // stays gone. Static portals stay, so this is where a traveller could go back
    // and forth, and a placed pair, spent at once, could not show it.
    Run run;
    if (!Begin("level0", run)) return;
    const Portals::Static* from = StaticWithIndex(run, 0);
    const Portals::Static* to = StaticWithIndex(run, 1);
    if (from == nullptr || to == nullptr) return;
    const glm::dvec2 toPx = to->atPx;
    const entt::entity stone =
        LevelBuilder::BuildRigidCircle(run.registry, "test_stone", from->atPx, 30.0, LevelBuilder::Options{});
    run.level.portals.travellers.push_back(stone);
    Tick(run);
    const glm::dvec2 at = Units::ToPixels(run.registry.get<TransformComponent>(stone).position);
    const bool through = run.level.portals.traversals == 1 && glm::distance(at, toPx) < kArrivalPx;
    CHECK_MSG(through, "the stone is at " + Px(at) + " after " + std::to_string(run.level.portals.traversals) +
                           " traversal(s); the exit is at " + Px(toPx));
    for (int tick = 0; tick < 180; ++tick) Tick(run);
    const glm::dvec2 settled = Units::ToPixels(run.registry.get<TransformComponent>(stone).position);
    CHECK_MSG(run.level.portals.traversals == 1,
              std::to_string(run.level.portals.traversals) + " traversals; the stone is at " + Px(settled));
}

void WithStaticPortalsSpentBothEndsGo() {
    Run run;
    if (!Begin("level0", run)) return;
    run.level.portals.rules.staticPortalsPersist = false;
    const Portals::Static* from = StaticWithIndex(run, 2);
    if (from == nullptr) return;
    PutPlayer(run, from->atPx);
    Tick(run);
    CHECK_EQ(run.level.portals.traversals, 1);
    CHECK(!StaticWithIndex(run, 2)->live);
    CHECK(!StaticWithIndex(run, 3)->live);
    CHECK(StaticWithIndex(run, 0)->live);
    CHECK(StaticWithIndex(run, 1)->live);
}

void Level1sPortalLeadsToThePlacedOne() {
    // level1 ships one static portal, index 0 with destiny 1, and grants one
    // placement. No static portal has index 1.
    {
        Run run;
        if (!Begin("level1", run)) return;
        const auto& statics = run.level.portals.statics;
        CHECK_EQ(statics.size(), std::size_t{1});
        if (statics.size() != 1) return;
        CHECK_EQ(statics[0].index, 0);
        CHECK_EQ(statics[0].destiny, 1);
        CHECK(StaticWithIndex(run, 1) == nullptr);
        CHECK_EQ(run.level.portals.budget, 1);

        // With nothing placed it leads nowhere.
        PutPlayer(run, statics[0].atPx);
        Tick(run);
        CHECK_EQ(run.level.portals.traversals, 0);
        CHECK(glm::distance(PlayerPx(run), statics[0].atPx) < kArrivalPx);
    }
    {
        Run run;
        if (!Begin("level1", run)) return;
        const Portals::Static& portal = run.level.portals.statics.front();
        // Above the spawn, clear of the player standing there.
        const glm::dvec2 spawnPx = PlayerPx(run);
        const glm::dvec2 placedPx = spawnPx + glm::dvec2(0.0, -48.0);
        CHECK(run.level.portals.TryPlace(placedPx));

        PutPlayer(run, portal.atPx);
        Tick(run);
        CHECK_EQ(run.level.portals.traversals, 1);
        CHECK_MSG(glm::distance(PlayerPx(run), placedPx) < kArrivalPx,
                  "the player is at " + Px(PlayerPx(run)) + ", the placed portal at " + Px(placedPx));
        // The placed end is spent, the static end stays.
        CHECK(run.level.portals.placed.empty());
        CHECK(run.level.portals.statics.front().live);
    }
}

// The same partnership the other way about, which is how level 1-2 is actually
// solved: the player walks into the portal they placed and comes out of the
// static one. The port only ever paired a placement with another placement, so
// on a level granting one the traveller passed straight through - found by
// playing it, 12 September. The original takes this path when maxPortals is 1
// (PortalManager::doTeleporting, bytes 139692..140259, calling
// teleportToFirstStaticPortal, bytes 145999..146550).
void Level1sPlacedPortalLeadsToTheStaticOne() {
    Run run;
    if (!Begin("level1", run)) return;
    const glm::dvec2 staticPx = run.level.portals.statics.front().atPx;
    const glm::dvec2 spawnPx = PlayerPx(run);
    const glm::dvec2 placedPx = spawnPx + glm::dvec2(0.0, -48.0);
    CHECK(run.level.portals.TryPlace(placedPx));

    PutPlayer(run, placedPx);
    Tick(run);
    CHECK_EQ(run.level.portals.traversals, 1);
    CHECK_MSG(glm::distance(PlayerPx(run), staticPx) < kArrivalPx,
              "the player is at " + Px(PlayerPx(run)) + ", the static portal at " + Px(staticPx));
    // The placement is spent; the static portal stays, as it does the other way.
    CHECK(run.level.portals.placed.empty());
    CHECK(run.level.portals.statics.front().live);
}

// ---- level0, played ------------------------------------------------------------

void Level0FromTheSpawnHoldingRight() {
    Run run;
    if (!Begin("level0", run)) return;
    std::string trace;
    int tick = 0;
    for (; tick < 1200 && !run.level.goals.completed; ++tick) {
        Tick(run, 1.0f);
        if (tick % 30 == 0) {
            trace += " " + std::to_string(tick) + ":" + Px(PlayerPx(run)) + "/" +
                     std::to_string(run.level.portals.traversals);
        }
    }
    std::printf("  level0 holding right: %s after %.2f s, %d traversal(s)\n",
                run.level.goals.completed ? "completed" : "NOT completed", tick * kStep,
                run.level.portals.traversals);
    CHECK_MSG(run.level.goals.completed, "tick:(x, y)/traversals" + trace);
    CHECK(run.level.portals.traversals >= 1);
    CHECK_EQ(run.level.portals.portalsUsed, 0);
}

void runTests() {
    Level0ShipsTwoPairs();
    AStaticPortalSendsThePlayerToItsPartnerAndStays();
    ATraversalWritesDownWhatTheEffectsNeed();
    APlacedPortalHoversOnOnePoint();
    APlacedPortalPerishesAfterTwentySeconds();
    AStoneThroughAStaticPairGoesOnce();
    WithStaticPortalsSpentBothEndsGo();
    Level1sPortalLeadsToThePlacedOne();
    Level1sPlacedPortalLeadsToTheStaticOne();
    Level0FromTheSpawnHoldingRight();
}

} // namespace

int main() {
    std::error_code ec;
    if (!std::filesystem::is_directory(kLevels, ec) ||
        !std::filesystem::is_regular_file(kData + "/entity_roles.json", ec)) {
        std::printf("test_mp_statics: SKIPPED - needs the converted levels at %s\n"
                    "  and the remake's data at %s.\n"
                    "  Both live outside this repository; configure with\n"
                    "  -DSUPERSONIC_MAGICPORTALS_LEVELS=... and -DSUPERSONIC_MAGICPORTALS_DATA=...\n",
                    kLevels.c_str(), kData.c_str());
        return 77;
    }
    runTests();
    return ::test::summary("test_mp_statics", 40);
}
