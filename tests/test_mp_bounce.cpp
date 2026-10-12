// The bouncers of chapter 4's weightless rooms: entity `bounce`, role `bouncer`.
//
// Four slabs across three levels, each bobbing one pixel up and one pixel down
// every 5.24 seconds. The motion is decoration; what is NOT decoration is that
// the slab is solid and moves, so it shoves what it meets - and carries nothing.
//
// WHAT THIS SUITE IS REALLY FOR. The travel is ONE PIXEL, so no test of position
// could ever tell a bouncer that moves from one that does not - a static body
// would sit at the same coordinates to within a pixel and pass every check below
// about where it is. So the assertions that matter here are that the body is
// KINEMATIC (a build-time property of Roles::Moves and nothing the tick can
// repair: the engine's solver does not move a static body at all) and that it is
// PLACED, with no velocity, as the original's SetTransform places it (planning
// step 173, CMP-15): a velocity would carry a rider the original's slab leaves
// where it is. test_mp_torch watches a body for the same reason.
//
// And the PHASE: cos(0) is 1, so a bouncer starts at full displacement rather
// than at the node the level places it on - the node is the MIDDLE of the bob.
// That is the trap fields.json records for the shock rings, and it is the one a
// sine would get wrong while looking right in motion.
//
// Reads the converted levels from outside this repository, and skips, saying
// where it looked, when they are absent.

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "core/Components.hpp"
#include "sim/Bounce.hpp"
#include "sim/Game.hpp"
#include "sim/Impacts.hpp"
#include "sim/Roles.hpp"
#include "sim/Tscn.hpp"
#include "sim/Units.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
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
const std::string kPortData = MAGICPORTALS_PORT_DATA_DIR;
constexpr float kStep = 1.0f / 60.0f;
constexpr double kPi = 3.14159265358979323846;

bool Open(const std::string& name, Game::Data& data, entt::registry& registry, Game::Level& level) {
    const std::filesystem::path prisms = std::filesystem::temp_directory_path() / "supersonic-test-mp-bounce";
    std::string error;
    if (!Game::LoadData(kLevels + "/" + name + ".tscn", kData, prisms, data, error) ||
        !Game::Start(data, registry, level, error)) {
        CHECK_MSG(false, name + ": " + error);
        return false;
    }
    return true;
}

void TheRulesRead() {
    Bounce::Rules rules;
    std::string error;
    const bool ok = Bounce::LoadRules(kPortData + "/bounce.json", rules, error);
    CHECK_MSG(ok, error);
    if (!ok) return;
    CHECK(::test::nearly(static_cast<float>(rules.speedRadPerSec), 1.2f));
    CHECK(::test::nearly(static_cast<float>(rules.stridePx), 1.0f));
    CHECK(::test::nearly(static_cast<float>(rules.frameClampMs), 200.0f));
    const double periodS = 2.0 * kPi / rules.speedRadPerSec;
    std::printf("  %.1f rad/s, %.1f px either way, a full bob every %.2f s\n", rules.speedRadPerSec, rules.stridePx,
                periodS);
    CHECK(periodS > 5.0 && periodS < 5.5);
}

// The census, counted as NODES.
void FourSlabsAcrossThreeLevels() {
    const struct Level {
        const char* name;
        std::size_t count;
    } levels[] = {{"level1c", 1}, {"level6c", 2}, {"level16c", 1}};

    std::size_t total = 0;
    for (const Level& one : levels) {
        Game::Data data;
        entt::registry registry;
        Game::Level level;
        if (!Open(one.name, data, registry, level)) continue;
        CHECK_MSG(level.bounce.bobs.size() == one.count,
                  std::string(one.name) + " places " + std::to_string(one.count) + " bouncer(s)");
        total += level.bounce.bobs.size();
        // Every one of them is in a level that sets no_gravity, which is where
        // its numbers come from: setNoGravityLinearMotionProperties.
        CHECK_MSG(level.noGravity, std::string(one.name) + " is a zero-gravity level");
    }
    CHECK_EQ(total, std::size_t{4});
    std::printf("  %zu bouncer(s) across 3 levels\n", total);

    // And a level with none has none - without this, a Wire that found every
    // node in the game would pass every count above.
    {
        Game::Data data;
        entt::registry registry;
        Game::Level level;
        if (Open("level0", data, registry, level)) CHECK(level.bounce.bobs.empty());
    }
}

// A slab the solver does not move is not built (it is placed with no velocity, below, and the engine
// shoves what it moves into only because the body is kinematic).
void EverySlabIsKinematic() {
    Game::Data data;
    entt::registry registry;
    Game::Level level;
    if (!Open("level6c", data, registry, level)) return;

    for (const Bounce::Bob& bob : level.bounce.bobs) {
        CHECK_MSG(registry.valid(bob.entity), bob.name + " was built");
        if (!registry.valid(bob.entity)) continue;
        const auto* rigid = registry.try_get<RigidBodyComponent>(bob.entity);
        CHECK_MSG(rigid != nullptr, bob.name + " has a body to be moved by");
        if (rigid == nullptr) continue;
        CHECK_MSG(rigid->isKinematic, bob.name + " is KINEMATIC, so the solver moves it and it shoves what it meets");
    }
}

// cos(0) = 1: it begins a whole stride from where the level places it.
void ItBeginsAtFullDisplacementNotAtItsNode() {
    Game::Data data;
    entt::registry registry;
    Game::Level level;
    if (!Open("level1c", data, registry, level)) return;
    if (level.bounce.bobs.empty()) {
        CHECK_MSG(false, "level1c places a bouncer");
        return;
    }

    const Bounce::Bob& bob = level.bounce.bobs.front();
    // The node's own place, which the level file gives as (140, 202).
    CHECK(::test::nearly(static_cast<float>(bob.originPx.x), 140.0f));
    CHECK(::test::nearly(static_cast<float>(bob.originPx.y), 202.0f));

    const glm::dvec2 at = bob.AtPx(level.bounce.rules);
    CHECK(::test::nearly(static_cast<float>(at.x), static_cast<float>(bob.originPx.x)));
    CHECK(::test::nearly(static_cast<float>(at.y - bob.originPx.y), static_cast<float>(level.bounce.rules.stridePx)));
}

// Half a period away it is a whole stride the other side, and the travel from
// end to end is twice the stride - one pixel each way.
void ItBobsAndComesBack() {
    Game::Data data;
    entt::registry registry;
    Game::Level level;
    if (!Open("level1c", data, registry, level)) return;
    if (level.bounce.bobs.empty()) return;

    const std::string name = level.bounce.bobs.front().name;
    const glm::dvec2 originPx = level.bounce.bobs.front().originPx;
    const double stridePx = level.bounce.rules.stridePx;

    double lowest = originPx.y;
    double highest = originPx.y;
    // Half a bob: PI / 1.2 = 2.618 s, which is 157 ticks at 60 a second.
    for (int tick = 0; tick < 157; ++tick) {
        Game::Tick(data, registry, level, 0.0f, kStep);
        const Bounce::Bob* bob = level.bounce.Find(name);
        CHECK_MSG(bob != nullptr, name + " is still there");
        if (bob == nullptr) return;
        const double y = bob->AtPx(level.bounce.rules).y;
        lowest = std::min(lowest, y);
        highest = std::max(highest, y);
    }

    const Bounce::Bob* bob = level.bounce.Find(name);
    if (bob == nullptr) return;
    const double endY = bob->AtPx(level.bounce.rules).y;
    std::printf("  level1c: from %+.4f to %+.4f px about its node, ending %+.4f\n", lowest - originPx.y,
                highest - originPx.y, endY - originPx.y);

    // A whole stride the other side of the node.
    CHECK(endY < originPx.y - stridePx * 0.99);
    // And it never went further than a stride either way.
    CHECK(highest - originPx.y <= stridePx + 0.001);
    CHECK(originPx.y - lowest <= stridePx + 0.001);
}

// The body in the registry goes where the bob says, not merely the arithmetic.
void TheBodyItselfMoves() {
    Game::Data data;
    entt::registry registry;
    Game::Level level;
    if (!Open("level1c", data, registry, level)) return;
    if (level.bounce.bobs.empty()) return;

    const std::string name = level.bounce.bobs.front().name;
    for (int tick = 0; tick < 30; ++tick) Game::Tick(data, registry, level, 0.0f, kStep);

    const Bounce::Bob* bob = level.bounce.Find(name);
    CHECK(bob != nullptr);
    if (bob == nullptr || !registry.valid(bob->entity)) return;
    const glm::dvec2 saidPx = bob->AtPx(level.bounce.rules);
    const glm::dvec2 isPx = Units::ToPixels(registry.get<TransformComponent>(bob->entity).position);
    CHECK(::test::nearly(static_cast<float>(isPx.x), static_cast<float>(saidPx.x)));
    CHECK(::test::nearly(static_cast<float>(isPx.y), static_cast<float>(saidPx.y)));
}

// THE RULE (planning step 173, CMP-15). ETHCallback_bounce moves its slab with linearMotion, whose SetPosition is
// b2Body::SetTransform on a static body (ETHPhysicsEntityController.cpp:98-103): a placement, with no velocity. The port
// had written the velocity that gets the slab there (Mover::MoveKinematic), which the engine's contact solve reads, so
// whatever touched a slab was carried by it at the slab's own speed. The slab's velocity is zero after every tick, on
// every one of the four, over a bob and a half.
void EverySlabIsPlacedWithNoVelocity() {
    const char* const names[] = {"level1c", "level6c", "level16c"};
    for (const char* name : names) {
        Game::Data data;
        entt::registry registry;
        Game::Level level;
        if (!Open(name, data, registry, level)) continue;

        std::size_t checked = 0;
        std::size_t moving = 0;
        int firstMoving = 0;
        for (int tick = 1; tick <= 450; ++tick) {
            Game::Tick(data, registry, level, 0.0f, kStep);
            for (const Bounce::Bob& bob : level.bounce.bobs) {
                const auto* rigid = registry.try_get<RigidBodyComponent>(bob.entity);
                if (rigid == nullptr) continue;
                ++checked;
                if (rigid->velocity == glm::vec3(0.0f)) continue;
                ++moving;
                if (firstMoving == 0) firstMoving = tick;
            }
        }
        CHECK_MSG(checked > 0, std::string(name) + ": a slab to read");
        CHECK_MSG(moving == 0, std::string(name) + ": " + std::to_string(moving) + " of " + std::to_string(checked) +
                                   " slab-ticks carry a velocity (the first on tick " + std::to_string(firstMoving) +
                                   "): a slab is placed, not moved");
    }
}

// One sample of the rider trace: where the player stood, and how fast, on a tick.
struct Sample {
    int tick = 0;
    double yPx = 0.0;
    double vyPxS = 0.0;
};

// How near a body must be to a slab to be at it. The engine rests a body a little off what it stands on (the 2 px
// resting offset recorded as CMP16-5), so this is a touch and a bit.
constexpr double kAtSlabPx = 3.0;

// What an idle level's dynamic bodies do against its slabs: the player's samples at the named ticks, and how close any
// dynamic body (the player, a crate) came to any slab - the census's measure of a rider.
struct RiderTrace {
    std::vector<Sample> samples;
    double nearestPx = std::numeric_limits<double>::max(); // the smallest gap between any dynamic body and any slab
    int atSlabTicks = 0;                                   // ticks on which some body was within kAtSlabPx of one
    int firstAtSlab = 0;
};

RiderTrace TraceRiders(const char* name, int ticks, const std::vector<int>& sampleAt) {
    RiderTrace trace;
    Game::Data data;
    entt::registry registry;
    Game::Level level;
    if (!Open(name, data, registry, level)) return trace;

    for (int tick = 1; tick <= ticks; ++tick) {
        Game::Tick(data, registry, level, 0.0f, kStep);

        bool atSlab = false;
        for (const Bounce::Bob& bob : level.bounce.bobs) {
            Impacts::Shape slab;
            if (!registry.valid(bob.entity) || !Impacts::ShapeOf(registry, bob.entity, slab)) continue;
            for (const auto [body, rigid] : registry.view<RigidBodyComponent>().each()) {
                if (rigid.isKinematic || body == bob.entity) continue;
                Impacts::Shape shape;
                if (!Impacts::ShapeOf(registry, body, shape)) continue;
                const double gap = Impacts::Between(shape, slab).px;
                trace.nearestPx = std::min(trace.nearestPx, gap);
                if (gap <= kAtSlabPx) atSlab = true;
            }
        }
        if (atSlab) {
            ++trace.atSlabTicks;
            if (trace.firstAtSlab == 0) trace.firstAtSlab = tick;
        }

        if (std::find(sampleAt.begin(), sampleAt.end(), tick) != sampleAt.end() && registry.valid(level.player)) {
            Sample sample;
            sample.tick = tick;
            sample.yPx = Units::ToPixels(registry.get<TransformComponent>(level.player).position).y;
            sample.vyPxS = -static_cast<double>(registry.get<RigidBodyComponent>(level.player).velocity.y) *
                           Units::kPixelsPerMetre;
            trace.samples.push_back(sample);
        }
    }
    return trace;
}

// THE RIDER OF 4-02 (CMP-15). The astronaut stands on level1c's slab in a room with no gravity. In the original he stays
// where he is - his eye holds y 380 +-0.5 in all six of the library's stills while the slab's top moves between 475 and
// 481 - and the port had him drift up at the slab's peak speed, 17 px by tick 420, because the slab's velocity carried
// him. Placed, he is moved by the slab's travel only: pushed up by what it rises into him, never launched. From tick 75
// to tick 450 his height changes by at most the slab's own travel from end to end (twice its stride), and his vertical
// speed stays below a tenth of the slab's peak speed, which a carried rider keeps for ever.
void TheRiderOfLevel1cIsNotCarried() {
    const std::vector<int> at = {75, 150, 300, 420, 450};
    const RiderTrace trace = TraceRiders("level1c", 450, at);
    CHECK_MSG(trace.samples.size() == at.size(), "level1c: the player's five samples");
    if (trace.samples.size() != at.size()) return;

    Bounce::Rules rules;
    std::string error;
    if (!Bounce::LoadRules(kPortData + "/bounce.json", rules, error)) {
        CHECK_MSG(false, error);
        return;
    }
    for (const Sample& sample : trace.samples) {
        std::printf("  level1c tick %3d: the player at y %8.3f px, vy %+8.4f px/s\n", sample.tick, sample.yPx,
                    sample.vyPxS);
    }
    const double travelPx = 2.0 * rules.stridePx;
    const double movedPx = std::abs(trace.samples.back().yPx - trace.samples.front().yPx);
    std::printf("  level1c: the player moves %.3f px from tick 75 to tick 450 (the slab's travel is %.3f px)\n", movedPx,
                travelPx);
    CHECK_MSG(movedPx <= travelPx + 0.25, "level1c: the rider is moved by no more than the slab's own travel");
    const double peakPxS = rules.stridePx * rules.speedRadPerSec;
    for (const Sample& sample : trace.samples) {
        CHECK_MSG(std::abs(sample.vyPxS) <= 0.1 * peakPxS,
                  "level1c tick " + std::to_string(sample.tick) + ": the rider carries no velocity of the slab's");
    }
    // He does stand at it, which is the premise of the rule.
    CHECK_MSG(trace.nearestPx <= kAtSlabPx, "level1c: the player stands at the slab");
}

// THE CENSUS OF RIDERS (planning step 173): which of the three levels has a body at a slab, idle. Printed, so that the
// planning record's census says what the tree measured; 4-02's is pinned above and the other two by the routes' traces.
void WhoStandsAtTheSlabs() {
    const char* const names[] = {"level1c", "level6c", "level16c"};
    for (const char* name : names) {
        const RiderTrace trace = TraceRiders(name, 450, {});
        std::printf("  %s idle: nearest body to a slab %.3f px; within %.1f px on %d of 450 ticks (the first on tick %d)\n",
                    name, trace.nearestPx, kAtSlabPx, trace.atSlabTicks, trace.firstAtSlab);
    }
}

// The role is played now, so no level reports it inert.
void TheRoleIsPlayed() {
    CHECK(Roles::IsPorted(Roles::kBouncer));
    CHECK(Roles::Moves(Roles::kBouncer));
    // And a role that is NOT played is still not, so admitting this one did not
    // admit everything.
    //
    // This named `gravity_well` until that was built a step later, which is a
    // guard doing its job: it failed the moment the thing it was pinning stopped
    // being true. `boss_spawn` takes its place because the port plays two of the
    // four bosses and leaves level31a's dragon and level31c's dark dragon inert -
    // Roles::IsPorted is per-role and cannot say "for two of them".
    CHECK(!Roles::IsPorted(Roles::kBossSpawn));
}

void runTests() {
    TheRulesRead();
    FourSlabsAcrossThreeLevels();
    EverySlabIsKinematic();
    ItBeginsAtFullDisplacementNotAtItsNode();
    ItBobsAndComesBack();
    TheBodyItselfMoves();
    EverySlabIsPlacedWithNoVelocity();
    TheRiderOfLevel1cIsNotCarried();
    WhoStandsAtTheSlabs();
    TheRoleIsPlayed();
}

} // namespace

MP_SUITE_MAIN("test_mp_bounce") {
    std::error_code ec;
    if (!std::filesystem::is_directory(kLevels, ec) ||
        !std::filesystem::is_regular_file(kData + "/entity_roles.json", ec)) {
        std::printf("test_mp_bounce: SKIPPED - needs the converted levels at %s\n"
                    "  and Enhanced's data at %s.\n"
                    "  Both live outside this repository; configure with\n"
                    "  -DSUPERSONIC_MAGICPORTALS_LEVELS=... and -DSUPERSONIC_MAGICPORTALS_DATA=...\n",
                    kLevels.c_str(), kData.c_str());
        return 77;
    }
    runTests();
    return ::test::summary("test_mp_bounce", 40);
}
