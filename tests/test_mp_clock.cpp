// The original's SCRIPT CLOCK, as sim/ScriptClock reads it from game/data/clock.json
// (planning step 147, program wave 3a, CK).
//
// The Android loop measures a frame as a float of milliseconds and ETHEngine::Update
// gives the SCRIPTS only its truncation to an unsigned long (ETHEngine.cpp:147,
// __aeabi_f2uiz in the shipped library), and the C++ side - the physics step, the
// entities' own updates and their particle systems - the float itself (:166). A
// script reads the integer through STimeManager::getLastFrameElapsedTime (bytes
// 7894..7961) or unitsPerSecond (8045..8162): at a steady 60 Hz, 16 ms of a 16.667 ms
// frame. What is pinned:
//
//   the rules    clock.json: 60 Hz, a frame of floor(1000 / 60) = 16 ms; the other
//                rates' floors (30 -> 33, 120 -> 8, 144 -> 6); and what a file that
//                would stop the script clock, or leave it a fraction, is refused for.
//   the floor    the truncation __aeabi_f2uiz does: 16.67 -> 16, 33.33 -> 33, a frame
//                of nothing, a negative one or NaN -> 0.
//   the sum      a tick's seconds are n / 1000 and give back exactly n ms for every
//                whole n up to 1000, so N ticks are 16 N exactly and a threshold at a
//                multiple of 16 trips on the tick the original's integer sum trips on
//                (400 ms on the 25th, 800 on the 50th, 2000 on the 125th) - where the
//                float 0.016f would have made a tick 16.00000076 ms and 25 of them
//                400.00002.
//   the step     a bare float is one length for both deltas (what the pure suites'
//                kStep has always meant); the layer's is the floor for the script and
//                the engine's tick for the physics - which TickStep never touches.
//   per frame    the effects the tick queues and the frame adds: a frame the length of
//                the tick is the floor exactly, any other is scaled by floor / tick.
//   the pace     clock.json's original_frame_hz, the frame rate of the original the world stands
//                for (60 = one tick an original frame, the pace the port had before the key;
//                30 = two): an absent key is matched_hz, a whole divisor of it is taken, and 45,
//                7, 0, 61, 120, a fraction or a non-number is refused naming the key; at 30
//                a tick's simS is half the decoded step, exactly the float 1/60, so SimRatio is
//                exactly 1 (2 at 60), the script's 16 ms and the wall clock stand, and the
//                Step's framePhase runs 0,1,0,1 from the level's first step (the helpers a
//                once-a-frame quantity is written with are test_mp_frameclock's); the
//                original's step OriginalStepCarriedS is the engine's tick times the ticks a
//                frame, bit for bit at one.
//   the readers  a platform, a lift's door and a timed crystal stepped by a script
//                tick (16 ms) and a body carried by the physics step (1/60 s): the
//                door's interpolator, the platform's swing, a crystal's dial (a
//                10,000 ms crystal is whole through its 625th tick and runs out on the
//                626th, a 12,000 on the 751st: the test is strictly above),
//                a Motion's angle per tick, the 600 ms antiportal period (37.5 ticks)
//                and the rocket's shake re-arm (every seventh tick, the sixth being 96
//                ms, where a tick of 16.667 ms re-armed every sixth).
//
// Pure arithmetic: no level, no window.

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "core/Components.hpp"
#include "sim/Cutscene.hpp"
#include "sim/Goals.hpp"
#include "sim/Motion.hpp"
#include "sim/Mover.hpp"
#include "sim/ScriptClock.hpp"
#include "sim/Units.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

using namespace MagicPortals;
using Supersonic::RigidBodyComponent;
using Supersonic::TransformComponent;

namespace {

const std::string kPortData = MAGICPORTALS_PORT_DATA_DIR;
constexpr float kPhysicsStep = 1.0f / 60.0f;

// Doubles, which the harness's CHECK_NEAR (floats) would narrow.
#define CHECK_NEARD(a, b)                                                                                  do {                                                                                                       const double nearA = (a);                                                                              const double nearB = (b);                                                                              CHECK_MSG(std::fabs(nearA - nearB) < 1e-9, std::string(#a) + " is " + std::to_string(nearA) +                                                        ", expected " + std::to_string(nearB));             } while (false)

ScriptClock::Rules ReadRules() {
    ScriptClock::Rules rules;
    std::string error;
    CHECK_MSG(ScriptClock::LoadRules(kPortData + "/clock.json", rules, error), error);
    return rules;
}

// A clock.json the loader is to refuse or take, written to a scratch file.
bool Load(const std::string& text, ScriptClock::Rules& out, std::string& error) {
    const std::filesystem::path path = std::filesystem::temp_directory_path() / "supersonic-test-mp-clock.json";
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file << text;
    }
    const bool ok = ScriptClock::LoadRules(path.string(), out, error);
    std::error_code ec;
    std::filesystem::remove(path, ec);
    return ok;
}

std::string RulesText(const std::string& hz, const std::string& snap = "0.001",
                      const std::string& physics = "0.03333333507180214") {
    return "{\"matched_hz\": " + hz + ", \"snap_ms\": " + snap + ", \"physics_step_s\": " + physics + "}";
}

// The same file with the pace knob written: `frameHz` is the JSON text of original_frame_hz's value (a number, or what is not one).
std::string PaceText(const std::string& frameHz, const std::string& matchedHz = "60") {
    return "{\"matched_hz\": " + matchedHz + ", \"snap_ms\": 0.001, \"physics_step_s\": 0.03333333507180214, \"original_frame_hz\": " +
           frameHz + "}";
}

void TheRulesAreClockJsons() {
    const ScriptClock::Rules rules = ReadRules();
    CHECK_EQ(rules.matchedHz, 60);
    CHECK_EQ(rules.frameMs, 16u);
    CHECK_NEARD(rules.wallFrameMs, 1000.0 / 60.0);
    CHECK_NEARD(rules.snapMs, 0.001);
    // The original's physics advances a FIXED 1/30 s a frame: AverageFPSRateManager's m_timeStep, the float
    // the listing prints as 0.0333333f - the bits 1023969417, 0.033333335 - not 1/30 exactly, and exactly twice
    // the float 1/60 the engine's tick is.
    CHECK_NEARD(rules.physicsStepS, static_cast<double>(std::bit_cast<float>(std::uint32_t{1023969417u})));
    CHECK(rules.physicsStepS == 2.0 * static_cast<double>(kPhysicsStep));

    // The other rates' floors, as clock.json's note lists them.
    struct Case {
        const char* hz;
        unsigned floorMs;
    };
    for (const Case& c : {Case{"30", 33}, Case{"59", 16}, Case{"60", 16}, Case{"120", 8}, Case{"144", 6},
                          Case{"500", 2}, Case{"1000", 1}, Case{"1", 1000}}) {
        ScriptClock::Rules read;
        std::string error;
        CHECK_MSG(Load(RulesText(c.hz), read, error), std::string(c.hz) + " Hz: " + error);
        CHECK_MSG(read.frameMs == c.floorMs, std::string(c.hz) + " Hz is " + std::to_string(read.frameMs) + " ms, not " +
                                                 std::to_string(c.floorMs));
    }
}

void AFileThatWouldStopOrBlurTheClockIsRefused() {
    ScriptClock::Rules out;
    out.frameMs = 7;
    std::string error;
    // A rate whose frame is under a millisecond floors to 0: the script would never advance.
    CHECK(!Load(RulesText("1001"), out, error));
    CHECK(!Load(RulesText("2000"), out, error));
    CHECK(!Load(RulesText("0"), out, error));
    CHECK(!Load(RulesText("-60"), out, error));
    // Whole hertz: a fraction is not a display's rate here.
    CHECK(!Load(RulesText("59.94"), out, error));
    CHECK(!Load(RulesText("60", "-1"), out, error));
    // A physics step of nothing or less, or one that is not a number, or missing: the player's walk and
    // fall are divided by it.
    CHECK(!Load(RulesText("60", "0.001", "0"), out, error));
    CHECK(!Load(RulesText("60", "0.001", "-0.0333"), out, error));
    CHECK(!Load(RulesText("60", "0.001", "\"0.0333\""), out, error));
    CHECK(!Load("{\"matched_hz\": 60, \"snap_ms\": 0.001}", out, error));
    // A key that is not a number, one missing, a file that is not JSON, one that is not there.
    CHECK(!Load("{\"matched_hz\": \"60\", \"snap_ms\": 0.001}", out, error));
    CHECK(!Load("{\"matched_hz\": 60}", out, error));
    CHECK(!Load("{\"snap_ms\": 0.001}", out, error));
    CHECK(!Load("not json", out, error));
    CHECK(!ScriptClock::LoadRules(kPortData + "/no_such_clock.json", out, error));
    CHECK_MSG(error.find("cannot open") != std::string::npos, error);
    // And what was refused leaves what the caller held.
    CHECK_EQ(out.frameMs, 7u);
}

void TheFloorIsATruncation() {
    // __aeabi_f2uiz: toward zero; 0 for what is not above it.
    CHECK_EQ(ScriptClock::FloorMs(16.6666675), 16u);
    CHECK_EQ(ScriptClock::FloorMs(16.0), 16u);
    CHECK_EQ(ScriptClock::FloorMs(33.3333), 33u);
    CHECK_EQ(ScriptClock::FloorMs(16.999999), 16u);
    CHECK_EQ(ScriptClock::FloorMs(17.0), 17u);
    CHECK_EQ(ScriptClock::FloorMs(1000.0), 1000u);
    CHECK_EQ(ScriptClock::FloorMs(0.999), 0u);
    CHECK_EQ(ScriptClock::FloorMs(0.0), 0u);
    CHECK_EQ(ScriptClock::FloorMs(-3.0), 0u);
    CHECK_EQ(ScriptClock::FloorMs(std::numeric_limits<double>::quiet_NaN()), 0u);
}

void ASumOfTicksIsTheOriginalsIntegerSum() {
    const ScriptClock::Rules rules = ReadRules();
    const ScriptClock::Step step = ScriptClock::TickStep(rules, kPhysicsStep);
    // 16 ms exactly, in seconds and back.
    CHECK(ScriptClock::Ms(step) == 16.0);
    // n / 1000 and back is exact for every whole n up to 1000: the whole range a frame can floor to.
    int inexact = 0;
    for (int n = 1; n <= 1000; ++n) {
        const ScriptClock::Step made(static_cast<double>(n) / 1000.0, kPhysicsStep);
        if (ScriptClock::Ms(made) != static_cast<double>(n)) ++inexact;
    }
    CHECK_MSG(inexact == 0, std::to_string(inexact) + " whole milliseconds that do not come back");
    // The float the port would have handed over, the defect this avoids: 16.00000076 ms.
    CHECK(static_cast<double>(0.016f) * 1000.0 != 16.0);

    // N ticks are 16 N exactly, and a threshold at a multiple of 16 trips where the integer sum does.
    double elapsed = 0.0;
    int first400 = 0, firstAbove400 = 0, first800 = 0, first2000 = 0, first10000 = 0, firstAbove10000 = 0;
    for (int tick = 1; tick <= 700; ++tick) {
        elapsed += ScriptClock::Ms(step);
        if (elapsed != 16.0 * tick) {
            CHECK_MSG(false, "tick " + std::to_string(tick) + " sums to " + std::to_string(elapsed));
            break;
        }
        if (first400 == 0 && elapsed >= 400.0) first400 = tick;
        if (firstAbove400 == 0 && elapsed > 400.0) firstAbove400 = tick;
        if (first800 == 0 && elapsed >= 800.0) first800 = tick;
        if (first2000 == 0 && elapsed >= 2000.0) first2000 = tick;
        if (first10000 == 0 && elapsed >= 10000.0) first10000 = tick;
        if (firstAbove10000 == 0 && elapsed > 10000.0) firstAbove10000 = tick;
    }
    CHECK_EQ(first400, 25);
    CHECK_EQ(firstAbove400, 26);
    CHECK_EQ(first800, 50);
    CHECK_EQ(first2000, 125);
    CHECK_EQ(first10000, 625);
    CHECK_EQ(firstAbove10000, 626);
    CHECK(elapsed == 700.0 * 16.0);

    // What the wall clock does with the same ticks: the engine's own 16.667 each (the float 1/60).
    CHECK(ScriptClock::WallMs(step) == static_cast<double>(kPhysicsStep) * 1000.0);
}

void TheStepKeepsThePhysicsStepAndABareFloatIsOneLengthForBoth() {
    const ScriptClock::Rules rules = ReadRules();
    for (const float physics : {1.0f / 60.0f, 1.0f / 30.0f, 1.0f / 120.0f}) {
        const ScriptClock::Step step = ScriptClock::TickStep(rules, physics);
        CHECK_MSG(step.physicsS == physics, "TickStep leaves the engine's step alone");
        CHECK(step.scriptS * 1000.0 == 16.0);
    }
    // The original's physics second the step carries (planning step 148): the fixed 1/30 s of a frame, a
    // ratio of 2 to the 1/60 s tick, and the 60 GetFPSRate answers at the matched rate.
    const ScriptClock::Step layer = ScriptClock::TickStep(rules, kPhysicsStep);
    CHECK(layer.simS == rules.physicsStepS);
    CHECK_NEARD(ScriptClock::SimRatio(layer), 2.0);
    CHECK(layer.fps == 60.0);
    // A bare float: what a pure suite's `kStep` is, for both - and the original's physics second one with
    // the tick, a ratio of 1, so a suite that steps a body in the original's own units still means it.
    const ScriptClock::Step bare = 1.0f / 60.0f;
    CHECK(bare.physicsS == kPhysicsStep);
    CHECK(bare.scriptS == static_cast<double>(kPhysicsStep));
    CHECK_NEARD(ScriptClock::SimRatio(bare), 1.0);
    CHECK_MSG(std::fabs(bare.fps - 60.0) < 1e-5, "a bare float's frame rate is 1 / the tick");
    const ScriptClock::Step none;
    CHECK(none.physicsS == kPhysicsStep);
    // The engine's own tick is not changed by being asked about.
    const ScriptClock::Step again = ScriptClock::TickStep(rules, kPhysicsStep);
    CHECK(again.physicsS == kPhysicsStep && ScriptClock::WallMs(again) == static_cast<double>(kPhysicsStep) * 1000.0);
}

void ARenderedFrameIsScaledByFloorOverTick() {
    const ScriptClock::Rules rules = ReadRules();
    const double tickMs = static_cast<double>(kPhysicsStep) * 1000.0;
    // A frame the length of the tick, exactly the floor - the engine's float and a double alike.
    CHECK(ScriptClock::OfFrameMs(rules, tickMs, tickMs) == 16.0);
    CHECK(ScriptClock::OfFrameMs(rules, 1000.0 / 60.0, tickMs) == 16.0);
    CHECK(ScriptClock::OfFrameMs(rules, tickMs + 0.0009, tickMs) == 16.0);
    // A frame that held two ticks gives two floors, a display twice as fast half of one.
    CHECK_NEARD(ScriptClock::OfFrameMs(rules, tickMs * 2.0, tickMs), 32.0);
    CHECK_NEARD(ScriptClock::OfFrameMs(rules, tickMs * 0.5, tickMs), 8.0);
    // A frame of nothing, or less, is nothing.
    CHECK(ScriptClock::OfFrameMs(rules, 0.0, tickMs) == 0.0);
    CHECK(ScriptClock::OfFrameMs(rules, -1.0, tickMs) == 0.0);
}

// ---- the readers, each stepped by the script's tick ------------------------------

// A door as the level's builder leaves it, with the interpolator at rest on its closed point.
struct DoorRun {
    entt::registry registry;
    entt::entity entity = entt::null;
    Mover::Door door;
    DoorRun() {
        entity = registry.create();
        registry.emplace<TransformComponent>(entity);
        registry.emplace<RigidBodyComponent>(entity);
        door.closedPx = glm::dvec2(0.0, 0.0);
        door.openPx = glm::dvec2(0.0, -100.0);
        door.strideMs = 5000.0;
        door.easeRad = 1.5707963267948966; // PI / 2: the ease the button's is
        door.atPx = door.closedPx;
    }
};

void ADoorsInterpolatorRunsOnTheScriptsMillisecondsAndItsBodyOnThePhysicsStep() {
    const ScriptClock::Rules rules = ReadRules();
    const ScriptClock::Step step = ScriptClock::TickStep(rules, kPhysicsStep);
    DoorRun run;
    // 25 ticks of a door held open: 400 ms of the interpolator's elapsedTime, exactly.
    for (int tick = 1; tick <= 25; ++tick) {
        run.door.Tick(run.registry, run.entity, true, step);
        CHECK_MSG(run.door.elapsedMs == 16.0 * tick, "tick " + std::to_string(tick) + ": " +
                                                         std::to_string(run.door.elapsedMs) + " ms");
    }
    CHECK(run.door.status == Mover::DoorStatus::Opening);

    // The body is carried by the PHYSICS step: the solver moves it by velocity x 1/60, so
    // the velocity is the tick's displacement over that, whatever the script's frame is.
    const glm::vec3 before = run.registry.get<TransformComponent>(run.entity).position;
    run.door.Tick(run.registry, run.entity, true, step);
    const glm::vec3 after = run.registry.get<TransformComponent>(run.entity).position;
    const glm::vec3 velocity = run.registry.get<RigidBodyComponent>(run.entity).velocity;
    CHECK_MSG(std::fabs(velocity.y * kPhysicsStep - (after.y - before.y)) < 1e-6f,
              "velocity x the physics step is the displacement: " + std::to_string(velocity.y * kPhysicsStep) + " against " +
                  std::to_string(after.y - before.y));
    // And not over the script's 16 ms: the velocity x 0.016 is 0.96 of the displacement, so
    // the solver, which moves a body by its own 1/60, lands it where the door's interpolator put it.
    const float moved = after.y - before.y;
    CHECK_MSG(moved != 0.0f, "the door moved on its 26th tick");
    if (moved != 0.0f) {
        CHECK_MSG(std::fabs(velocity.y * 0.016f / moved - 0.96f) < 1e-3f,
                  "the velocity is not measured against the script's frame: " +
                      std::to_string(velocity.y * 0.016f / moved));
    }
}

void APlatformAndALiftStepOnTheScriptAndAreCarriedByThePhysicsStep() {
    const ScriptClock::Rules rules = ReadRules();
    const ScriptClock::Step step = ScriptClock::TickStep(rules, kPhysicsStep);
    entt::registry registry;
    Mover::Movers movers;
    const entt::entity body = registry.create();
    registry.emplace<TransformComponent>(body);
    registry.emplace<RigidBodyComponent>(body);
    // linearMotion's own frame cap and turn, as the layer's rules carry them (motions.json).
    std::string motionError;
    CHECK_MSG(Motion::LoadRules(kPortData + "/motions.json", movers.motion, motionError), motionError);
    Mover::Platform platform;
    platform.name = "platform";
    platform.entity = body;
    platform.motion.originPx = glm::dvec2(0.0, 0.0);
    platform.motion.linear.vertical = true;
    platform.motion.linear.stride = 64.0;
    platform.motion.linear.speed = 1.0;
    movers.platforms.push_back(platform);

    // A lift sent to its far marker: 60 units a second is 0.96 units a script tick.
    const entt::entity cage = registry.create();
    registry.emplace<TransformComponent>(cage);
    registry.emplace<RigidBodyComponent>(cage);
    Mover::Lift lift;
    lift.name = "lift";
    lift.entity = cage;
    lift.motion.aPx = glm::dvec2(0.0, 0.0);
    lift.motion.bPx = glm::dvec2(0.0, 600.0);
    lift.motion.atPx = lift.motion.aPx;
    lift.motion.destPx = lift.motion.aPx;
    lift.motion.legSquared = 600.0 * 600.0;
    lift.motion.speedPx = 60.0;
    lift.motion.Send();
    movers.lifts.push_back(lift);

    for (int tick = 1; tick <= 620; ++tick) movers.Tick(registry, step);
    // 620 ticks of 16 ms: 9.92 s of the script's clock on the swing - its angle is that much of a radian a
    // second at speed 1, less the one turn linearMotion takes off once it is past 2 PI (on the 393rd tick) -
    // and the lift 595.2 of its 600 units out at 0.96 a tick (60 u/s x 16 ms).
    CHECK_NEARD(movers.platforms.front().motion.linear.angle, 620.0 * 0.016 - movers.motion.wrapRad);
    CHECK_NEARD(movers.lifts.front().motion.atPx.y, 620.0 * 0.96);
    CHECK(movers.lifts.front().motion.Moving());
    for (int tick = 621; tick <= 626; ++tick) movers.Tick(registry, step);
    // The 626th tick is the first past the leg's 600: it puts the lift on its marker.
    CHECK(!movers.lifts.front().motion.Moving() && movers.lifts.front().motion.atPx.y == 600.0);
    // The body is carried by the PHYSICS step: its velocity is the tick's displacement over 1/60.
    const float before = registry.get<TransformComponent>(body).position.y;
    movers.Tick(registry, step);
    const float after = registry.get<TransformComponent>(body).position.y;
    const float velocity = registry.get<RigidBodyComponent>(body).velocity.y;
    CHECK_MSG(std::fabs(velocity * kPhysicsStep - (after - before)) < 1e-6f, "the platform is carried by the physics step: " +
                                                                              std::to_string(velocity * kPhysicsStep) + " against " +
                                                                              std::to_string(after - before));
}

void ATimedCrystalRunsOutOnTheTickItsMillisecondsSumTo() {
    const ScriptClock::Rules rules = ReadRules();
    const ScriptClock::Step step = ScriptClock::TickStep(rules, kPhysicsStep);
    entt::registry registry;
    Goals::State goals;
    // The level's `time` is milliseconds, summed as the original's uint is, and tested STRICTLY
    // above (ETHCallback_timer ins 102-103, CMPu / JNP): the sum is exactly 10000 on the 625th
    // tick and the crystal is still there, so 10 s goes on the 626th, 12 s on the 751st, 2 s on the
    // 126th - where a 16.667 ms tick made them the 600th, 720th and 120th (a float's 1/60 sums a
    // hair over, so test_mp_timed's bare-float suites still see those).
    for (const double life : {2000.0, 10000.0, 12000.0}) {
        Goals::Crystal crystal;
        crystal.name = "crystal_" + std::to_string(static_cast<int>(life));
        crystal.timed = true;
        crystal.lifeMs = life;
        crystal.lifeS = life / 1000.0;
        goals.crystals.push_back(crystal);
    }
    int expiredAt[3] = {0, 0, 0};
    for (int tick = 1; tick <= 800; ++tick) {
        goals.Tick(registry, entt::null, step.scriptS);
        for (std::size_t i = 0; i < goals.crystals.size(); ++i) {
            if (goals.crystals[i].expired && expiredAt[i] == 0) expiredAt[i] = tick;
        }
    }
    CHECK_EQ(expiredAt[0], 126);
    CHECK_EQ(expiredAt[1], 626);
    CHECK_EQ(expiredAt[2], 751);
    // The dial's clock is that sum itself, held at the tick the crystal went: the first above.
    CHECK(goals.crystals[0].elapsedMs == 2016.0);
    CHECK(goals.crystals[1].elapsedMs == 10016.0);
    CHECK(goals.crystals[2].elapsedMs == 12016.0);
    // A crystal with no time to begin with never goes.
    Goals::State untimed;
    Goals::Crystal plain;
    plain.timed = true;
    untimed.crystals.push_back(plain);
    for (int tick = 0; tick < 10; ++tick) untimed.Tick(registry, entt::null, step.scriptS);
    CHECK(!untimed.crystals.front().expired);
}

void AMotionStepsItsAngleBySpeedTimesTheScriptsMilliseconds() {
    const ScriptClock::Rules rules = ReadRules();
    const ScriptClock::Step step = ScriptClock::TickStep(rules, kPhysicsStep);
    Motion::Rules motion;
    std::string error;
    CHECK_MSG(Motion::LoadRules(kPortData + "/motions.json", motion, error), error);
    // A crystal's bob: speed 2 rad/s, so 0.032 rad a script tick, the first call stepping before
    // the offset is read. After 100 ticks the angle is the start's + 3.2 (inside a turn).
    const double start = 0.5;
    Motion::Linear linear = Motion::Start(motion.crystal, start);
    for (int tick = 1; tick <= 100; ++tick) Motion::Advance(motion, linear, ScriptClock::Ms(step));
    CHECK_MSG(std::fabs(linear.angle - (start + motion.crystal.speed * 100.0 * 0.016)) < 1e-9,
              "the angle after 100 ticks: " + std::to_string(linear.angle));
    // The same ticks on the engine's 16.667 ms would have gone 4.17 % further.
    Motion::Linear wall = Motion::Start(motion.crystal, start);
    for (int tick = 1; tick <= 100; ++tick) Motion::Advance(motion, wall, 1000.0 / 60.0);
    CHECK_MSG(wall.angle > linear.angle + 0.1, "and the wall clock's tick goes further");
    // The bob's period is pi seconds: 196.35 ticks of 16 ms, 188.5 of 16.667.
    CHECK_NEARD(3.14159265358979323846 / (motion.crystal.speed * 0.016) , 98.17477042468103);
}

void TheAntiportalsPeriodIsThirtySevenAndAHalfTicks() {
    // The ring's blink runs 0.2 -> 0.4 -> 0.2 on 300 ms legs: 600 ms of the script's clock, which
    // is 37.5 ticks of 16 ms, and 625 ms of wall clock at 60 Hz (the old port's 36 ticks).
    const double periodMs = 600.0;
    CHECK(periodMs / 16.0 == 37.5);
    CHECK_NEARD(periodMs / 16.0 * (1000.0 / 60.0), 625.0);
    // Whole periods in whole ticks: 75 ticks are two periods, 1200 ms, to the millisecond.
    CHECK(75.0 * 16.0 == 2.0 * periodMs);
}

void TheRocketsShakeReArmsEverySeventhTick() {
    const ScriptClock::Rules rules = ReadRules();
    Cutscene::Rules cutscene;
    std::string error;
    CHECK_MSG(Cutscene::LoadRules(kPortData + "/cutscenes.json", cutscene, error), error);
    CHECK_NEARD(cutscene.launch.quakeRearmAboveMs, 100.0);
    // ETHCallback_rocket_launch adds the frame to its counter and re-arms the quake once it is past
    // 100 ms: whole milliseconds give 7 ticks (112 ms; 6 are 96), where the tick's 16.667 gave
    // 6 (100.000005). Step 123's deviation 2, which this clock settles.
    const auto spacings = [&cutscene](double frameMs) {
        Cutscene::Launch launch;
        std::vector<int> shakes;
        for (int tick = 1; tick <= 200; ++tick) {
            if (Cutscene::StepLaunch(cutscene, launch, frameMs).quake) shakes.push_back(tick);
        }
        return shakes;
    };
    const std::vector<int> script = spacings(ScriptClock::Ms(ScriptClock::TickStep(rules, kPhysicsStep)));
    CHECK(script.size() >= 20);
    int offBeat = 0;
    for (std::size_t i = 0; i < script.size(); ++i) {
        if (script[i] != static_cast<int>(7 * (i + 1))) ++offBeat;
    }
    CHECK_MSG(offBeat == 0, "the shake re-arms on ticks 7, 14, 21, ...: " + std::to_string(offBeat) + " off");
    // The wall tick, as it was: every sixth.
    const std::vector<int> wall = spacings(static_cast<double>(kPhysicsStep) * 1000.0);
    CHECK(wall.size() >= 20 && wall[0] == 6 && wall[1] == 12);
    // Within the launch's 6000 ms: 6000 / 16 = 375 ticks, so 53 shakes in a full launch.
    Cutscene::Launch launch;
    int count = 0;
    for (int tick = 1; tick <= 600; ++tick) {
        if (Cutscene::StepLaunch(cutscene, launch, 16.0).quake) ++count;
    }
    CHECK_EQ(count, 53);
}

// ---- the pace knob: original_frame_hz, the frame's ticks and the original's step ----------------------------------

void ThePaceKnobIsReadWholeAndOnlyAsADivisorOfTheMatchedRate() {
    // The shipped file is at the matched pace: one tick an original frame, the pace the port had before the key existed.
    const ScriptClock::Rules shipped = ReadRules();
    CHECK_EQ(shipped.originalFrameHz, 60);
    CHECK_EQ(shipped.frameTicks, 1);
    // And a Rules built in code is the same floor without a file.
    const ScriptClock::Rules bare{};
    CHECK_EQ(bare.originalFrameHz, 60);
    CHECK_EQ(bare.frameTicks, 1);

    struct Pace {
        const char* hz;
        int frameHz;
        int ticks;
    };
    for (const Pace& p : {Pace{"60", 60, 1}, Pace{"30", 30, 2}, Pace{"30.0", 30, 2}, Pace{"20", 20, 3}, Pace{"15", 15, 4},
                          Pace{"10", 10, 6}, Pace{"1", 1, 60}}) {
        ScriptClock::Rules read;
        std::string error;
        CHECK_MSG(Load(PaceText(p.hz), read, error), std::string(p.hz) + " Hz: " + error);
        CHECK_MSG(read.originalFrameHz == p.frameHz && read.frameTicks == p.ticks,
                  std::string(p.hz) + " Hz is " + std::to_string(read.frameTicks) + " ticks a frame, not " + std::to_string(p.ticks));
        // The tick's rate, the script's floor and the original's physics step are the matched file's, whatever the pace.
        CHECK_EQ(read.matchedHz, 60);
        CHECK_EQ(read.frameMs, 16u);
        CHECK(read.physicsStepS == shipped.physicsStepS);
    }

    // An absent key is the matched rate itself: every matched_hz the loader takes reads as it always did, one tick a frame.
    for (const char* hz : {"1", "30", "59", "60", "120", "144", "500", "1000"}) {
        ScriptClock::Rules read;
        std::string error;
        CHECK_MSG(Load(RulesText(hz), read, error), std::string(hz) + " Hz: " + error);
        CHECK_MSG(read.originalFrameHz == read.matchedHz && read.frameTicks == 1,
                  std::string(hz) + " Hz without the key is its own pace, one tick a frame");
    }
    // The ticks follow the matched rate of the file: 15 a second is two ticks at 30, 500 two at 1000.
    ScriptClock::Rules at30;
    std::string at30Error;
    CHECK_MSG(Load(PaceText("15", "30"), at30, at30Error), at30Error);
    CHECK(at30.matchedHz == 30 && at30.originalFrameHz == 15 && at30.frameTicks == 2 && at30.frameMs == 33u);
    ScriptClock::Rules at1000;
    CHECK_MSG(Load(PaceText("500", "1000"), at1000, at30Error), at30Error);
    CHECK(at1000.matchedHz == 1000 && at1000.frameTicks == 2 && at1000.frameMs == 1u);

    // Refused, naming the key: not a divisor (45, 7), nothing or less (0, -30), above the matched rate (61, 120, 1e9), a fraction
    // (30.5, 0.5), and what is not a number (a string, null, true, an array, an object). And what was refused leaves what the caller held.
    for (const char* value : {"45", "7", "0", "-30", "61", "120", "1e9", "30.5", "0.5", "\"30\"", "null", "true", "[]", "{}"}) {
        ScriptClock::Rules out;
        out.frameMs = 7;
        out.frameTicks = 5;
        std::string error;
        CHECK_MSG(!Load(PaceText(value), out, error), std::string("original_frame_hz ") + value + " is refused");
        CHECK_MSG(error.find("original_frame_hz") != std::string::npos, "the refusal names the key: " + error);
        CHECK_MSG(out.frameMs == 7u && out.frameTicks == 5, "and leaves the caller's Rules alone");
    }
    // Above or not dividing the file's own matched rate, whatever the 60 of the shipped one says.
    ScriptClock::Rules refusedOut;
    std::string refusedError;
    CHECK(!Load(PaceText("60", "30"), refusedOut, refusedError));
    CHECK_MSG(refusedError.find("original_frame_hz") != std::string::npos, refusedError);
    CHECK(!Load(PaceText("300", "1000"), refusedOut, refusedError));
    CHECK(!Load(PaceText("40", "30"), refusedOut, refusedError));
}

// ---- a level's own pace: original_frame_hz_by_level (planning step 262) -----------------------------------------------

void ALevelCanPlayAtItsOwnPaceAndOnlyAtOneTheKeyTakes() {
    // The shipped file: 3-22 (level21b, step 262) and 4-01 (level0c, step 266) at the 30 of the owner's videos, every other level at the file's own 60.
    const ScriptClock::Rules shipped = ReadRules();
    CHECK_EQ(shipped.baseOriginalFrameHz, 60);
    CHECK_MSG(shipped.originalFrameHzByLevel.size() == 2 && shipped.originalFrameHzByLevel.count("level21b") == 1 &&
                  shipped.originalFrameHzByLevel.at("level21b") == 30 && shipped.originalFrameHzByLevel.count("level0c") == 1 &&
                  shipped.originalFrameHzByLevel.at("level0c") == 30,
              "3-22 (level21b) and 4-01 (level0c) are the two levels the shipped table names, at 30");
    ScriptClock::Rules rules = shipped;
    CHECK(rules.ForLevel("level21b"));
    CHECK_MSG(rules.originalFrameHz == 30 && rules.frameTicks == 2, "level21b plays an original frame in two ticks");
    CHECK(ScriptClock::SimRatio(ScriptClock::TickStep(rules, kPhysicsStep)) == 1.0);
    // Back to the file's own for every level the table does not name, and for what is no level at all.
    for (const char* other : {"level0b", "level21a", "level21bb", "LEVEL21B", "level0cc", ""}) {
        CHECK(rules.ForLevel("level21b"));
        CHECK(rules.ForLevel(other));
        CHECK_MSG(rules.originalFrameHz == 60 && rules.frameTicks == 1, std::string("'") + other + "' plays at the file's own 60");
    }
    // A Rules built in code has no table: ForLevel leaves every level at its pace.
    ScriptClock::Rules bare{};
    CHECK(bare.ForLevel("level21b"));
    CHECK(bare.originalFrameHz == 60 && bare.frameTicks == 1);

    // The table beside a file's own pace of 30: one level back at 60, the rest at 30, and ForLevel puts the FILE's pace back (not the
    // last level's).
    const auto text = [](const std::string& base, const std::string& table) {
        return "{\"matched_hz\": 60, \"snap_ms\": 0.001, \"physics_step_s\": 0.03333333507180214, \"original_frame_hz\": " + base +
               ", \"original_frame_hz_by_level\": " + table + "}";
    };
    ScriptClock::Rules mixed;
    std::string error;
    CHECK_MSG(Load(text("30", "{\"level0b\": 60, \"level1b\": 20}"), mixed, error), error);
    CHECK(mixed.baseOriginalFrameHz == 30 && mixed.originalFrameHz == 30 && mixed.originalFrameHzByLevel.size() == 2);
    CHECK(mixed.ForLevel("level0b") && mixed.originalFrameHz == 60 && mixed.frameTicks == 1);
    CHECK(mixed.ForLevel("level1b") && mixed.originalFrameHz == 20 && mixed.frameTicks == 3);
    CHECK(mixed.ForLevel("level2b") && mixed.originalFrameHz == 30 && mixed.frameTicks == 2);
    // An empty table is no table, and the key is optional.
    ScriptClock::Rules none;
    CHECK_MSG(Load(text("60", "{}"), none, error), error);
    CHECK(none.originalFrameHzByLevel.empty());
    ScriptClock::Rules noKey;
    CHECK_MSG(Load(PaceText("30"), noKey, error), error);
    CHECK(noKey.originalFrameHzByLevel.empty() && noKey.baseOriginalFrameHz == 30);

    // Refused, naming the key: a pace that is not a divisor of 60 (45, 7), nothing (0), above it (61, 120), a fraction, not a number; a level
    // of nothing or a comment where a level belongs; a table that is not an object. And what was refused leaves what the caller held.
    for (const char* table : {"{\"level21b\": 45}", "{\"level21b\": 7}", "{\"level21b\": 0}", "{\"level21b\": 61}", "{\"level21b\": 120}",
                              "{\"level21b\": 30.5}", "{\"level21b\": \"30\"}", "{\"level21b\": null}", "{\"\": 30}", "{\"_note\": 30}", "[]", "30", "null"}) {
        ScriptClock::Rules out;
        out.frameMs = 7;
        out.frameTicks = 5;
        CHECK_MSG(!Load(text("60", table), out, error), std::string("original_frame_hz_by_level ") + table + " is refused");
        CHECK_MSG(error.find("original_frame_hz_by_level") != std::string::npos, "the refusal names the key: " + error);
        CHECK_MSG(out.frameMs == 7u && out.frameTicks == 5 && out.originalFrameHzByLevel.empty(), "and leaves the caller's Rules alone");
    }
}

void ThirtyHertzHalvesTheStepSoTheSimRatioIsExactlyOneAndSixtyIsTwo() {
    ScriptClock::Rules rules30;
    std::string error;
    CHECK_MSG(Load(PaceText("30"), rules30, error), error);
    const ScriptClock::Step step = ScriptClock::TickStep(rules30, kPhysicsStep);
    // simS is half the decoded step: the float 1/30 halved is the float 1/60 the engine ticks at, EXACTLY, so the ratio is
    // exactly 1 and a body is carried at the original's own velocity.
    CHECK(step.simS == static_cast<double>(kPhysicsStep));
    CHECK(step.simS == rules30.physicsStepS / 2.0);
    CHECK(ScriptClock::SimRatio(step) == 1.0);
    // What the pace leaves alone: the script's floor, the engine's tick and the wall clock.
    CHECK(step.scriptS * 1000.0 == 16.0);
    CHECK(ScriptClock::Ms(step) == 16.0);
    CHECK(step.physicsS == kPhysicsStep);
    CHECK(ScriptClock::WallMs(step) == static_cast<double>(kPhysicsStep) * 1000.0);
    // What GetFPSRate answers is the pace's rate, and the frame is two ticks, starting on its first.
    CHECK(step.fps == 30.0);
    CHECK_EQ(step.frameTicks, 2);
    CHECK_EQ(step.framePhase, 0);
    for (unsigned levelStep = 0; levelStep < 6; ++levelStep) {
        CHECK_EQ(ScriptClock::TickStep(rules30, kPhysicsStep, levelStep).framePhase, static_cast<int>(levelStep % 2u));
    }

    // At 60 the shipped file's step is what it was: the full decoded step, a ratio of exactly 2, one tick a frame, phase 0 always.
    const ScriptClock::Rules rules60 = ReadRules();
    const ScriptClock::Step matched = ScriptClock::TickStep(rules60, kPhysicsStep);
    CHECK(matched.simS == rules60.physicsStepS);
    CHECK(ScriptClock::SimRatio(matched) == 2.0);
    CHECK(matched.fps == 60.0);
    CHECK_EQ(matched.frameTicks, 1);
    CHECK_EQ(matched.framePhase, 0);
    for (unsigned levelStep = 0; levelStep < 6; ++levelStep) {
        CHECK_EQ(ScriptClock::TickStep(rules60, kPhysicsStep, levelStep).framePhase, 0);
    }
    // And a Step built by hand is one tick a frame and phase 0, as it always meant.
    const ScriptClock::Step bare(kPhysicsStep);
    CHECK_EQ(bare.frameTicks, 1);
    CHECK_EQ(bare.framePhase, 0);

    // THE NEUTRAL FLOOR: with no key every matched rate's TickStep is the three numbers it was before the pace existed - the
    // script's whole milliseconds, the decoded physics step and the matched rate as the frame rate - at any level step.
    for (const char* hz : {"1", "30", "60", "120", "144", "1000"}) {
        ScriptClock::Rules read;
        std::string readError;
        CHECK_MSG(Load(RulesText(hz), read, readError), readError);
        const ScriptClock::Step made = ScriptClock::TickStep(read, kPhysicsStep, 7u);
        CHECK_MSG(made.scriptS == static_cast<double>(read.frameMs) / 1000.0, std::string(hz) + " Hz: the script's step");
        CHECK_MSG(made.simS == read.physicsStepS, std::string(hz) + " Hz: the decoded physics step");
        CHECK_MSG(made.fps == static_cast<double>(read.matchedHz), std::string(hz) + " Hz: the frame rate");
        CHECK_MSG(made.frameTicks == 1 && made.framePhase == 0, std::string(hz) + " Hz: one tick a frame, phase 0");
    }
}

void TheOriginalStepIsTheEnginesTickTimesTheTicksAFrame() {
    const double tick = static_cast<double>(kPhysicsStep);
    // A step built by hand is one tick a frame, and the original's step is then the engine's tick bit for bit - whatever simS it carries.
    const ScriptClock::Step none;
    const ScriptClock::Step bare(kPhysicsStep);
    const ScriptClock::Step lengths(0.016, kPhysicsStep);
    const ScriptClock::Step doubled(1.0 / 60.0, 1.0f / 60.0f, 1.0 / 30.0, 60.0);
    CHECK(ScriptClock::OriginalStepCarriedS(none) == tick);
    CHECK(ScriptClock::OriginalStepCarriedS(bare) == tick);
    CHECK(ScriptClock::OriginalStepCarriedS(lengths) == tick);
    CHECK(ScriptClock::OriginalStepCarriedS(doubled) == tick);
    // That hand-built ratio is just under 2 (the float 1/60 against the double 1/30), which is why the step is a product and not
    // the division physicsStepS / SimRatio: the division would not be bit-exact here.
    CHECK(ScriptClock::SimRatio(doubled) < 2.0 && ScriptClock::SimRatio(doubled) > 1.99999);
    // Two ticks a frame: exactly twice the tick.
    const ScriptClock::Step two(0.016, kPhysicsStep, 1.0 / 60.0, 30.0, 2, 1);
    CHECK(ScriptClock::OriginalStepCarriedS(two) == 2.0 * tick);
    // A Step whose count was poked below one is read as one tick a frame.
    ScriptClock::Step poked;
    poked.frameTicks = 0;
    CHECK(ScriptClock::OriginalStepCarriedS(poked) == tick);

    // For a Step TickStep built, the product is also the division the old reading would have made, to a relative 1e-12.
    for (const char* frameHz : {"60", "30", "20", "10"}) {
        ScriptClock::Rules rules;
        std::string error;
        CHECK_MSG(Load(PaceText(frameHz), rules, error), error);
        const ScriptClock::Step step = ScriptClock::TickStep(rules, kPhysicsStep);
        const double carried = ScriptClock::OriginalStepCarriedS(step);
        const double byDivision = rules.physicsStepS / ScriptClock::SimRatio(step);
        CHECK_MSG(carried == static_cast<double>(step.frameTicks) * tick, std::string(frameHz) + " Hz: ticks a frame times the tick");
        CHECK_MSG(std::fabs(carried - byDivision) <= 1e-12 * carried,
                  std::string(frameHz) + " Hz: " + std::to_string(carried) + " against the division's " + std::to_string(byDivision));
    }
}

void runTests() {
    TheRulesAreClockJsons();
    AFileThatWouldStopOrBlurTheClockIsRefused();
    TheFloorIsATruncation();
    ASumOfTicksIsTheOriginalsIntegerSum();
    TheStepKeepsThePhysicsStepAndABareFloatIsOneLengthForBoth();
    ARenderedFrameIsScaledByFloorOverTick();
    ADoorsInterpolatorRunsOnTheScriptsMillisecondsAndItsBodyOnThePhysicsStep();
    APlatformAndALiftStepOnTheScriptAndAreCarriedByThePhysicsStep();
    ATimedCrystalRunsOutOnTheTickItsMillisecondsSumTo();
    AMotionStepsItsAngleBySpeedTimesTheScriptsMilliseconds();
    TheAntiportalsPeriodIsThirtySevenAndAHalfTicks();
    TheRocketsShakeReArmsEverySeventhTick();
    ThePaceKnobIsReadWholeAndOnlyAsADivisorOfTheMatchedRate();
    ALevelCanPlayAtItsOwnPaceAndOnlyAtOneTheKeyTakes();
    ThirtyHertzHalvesTheStepSoTheSimRatioIsExactlyOneAndSixtyIsTwo();
    TheOriginalStepIsTheEnginesTickTimesTheTicksAFrame();
}

} // namespace

MP_TEST_MAIN("test_mp_clock", 125)
