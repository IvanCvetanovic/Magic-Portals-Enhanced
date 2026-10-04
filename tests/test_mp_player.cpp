// The player's MOVEMENT, as the original's SideScrollerCharacter::applyForces and its Box2D body
// decode (planning step 148, program wave 3a, G10): data/player.json, sim/Player, and the
// original's physics second the script clock carries (clock.json physics_step_s).
//
//   the numbers   speed 3, the elapsed cap 66, the air factor 0.86 at 60, the world's gravity 10
//                 and the body's gravity scale 2 - each a number of the listing (player.json cites
//                 it), and a file that holds none of them, or a zero, is refused.
//   the walk      SET OUTRIGHT: speed x min(66, the script's ms) / the physics step's ms, a tick of
//                 it 2.4 units at the decode's 60 Hz (144 units a second), with no ramp up and a
//                 dead stop with no pad held - through the engine's own solver on a floor.
//   the air       off the floor the pad is ignored and the body keeps min(1, fps / 60 x 0.86) of its
//                 x velocity a tick: 0.86 at 60 Hz, the brake the original's footage shows (F13's
//                 walk off 1-06's block stops in the air).
//   the fall      10 x 2 m/s^2 of the original's physics, which steps 1/30 s a frame: 1.111 units a
//                 frame squared, 4000 units a second squared at a 60 Hz frame - through the solver,
//                 and the speed it lands at as the original's begin-contact callback reads it
//                 (Impacts: hitIntensity 8.9 off a 100-unit drop, whatever the port's tick).
//   the recoil    scale(1.4) of the original's physics, 70 units a second, which is 140 a second
//                 of wall time at 60 Hz and 40-44 on the 17-19 frames a second F10 was filmed at.
//   the hold      an idle player on the ground is pressed into a slope along its normal, not straight
//                 down (planning step 148's amendment, G10-5): he stands still on a 15 or 30-degree
//                 floor for 420 ticks, as the original's does on 1-24's ramp, where the same body
//                 without the hold creeps 0.278 units a tick, and the walk is the same with the
//                 hold as without it.
//
// Pure, but for the standing case on 1-24 at the end, which reads the converted level and skips
// itself without it: a floor, a capsule and the engine's solver, no window.

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "core/Components.hpp"
#include "core/PhysicsSystem.hpp"
#include "sim/Game.hpp"
#include "sim/Impacts.hpp"
#include "sim/LevelBuilder.hpp"
#include "sim/Player.hpp"
#include "sim/ScriptClock.hpp"
#include "sim/Units.hpp"
#include "sim/Zerog.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

using namespace MagicPortals;
using Supersonic::BoxColliderComponent;
using Supersonic::PhysicsMaterialComponent;
using Supersonic::RigidBodyComponent;
using Supersonic::TransformComponent;

namespace {

const std::string kData = MAGICPORTALS_DATA_DIR;
const std::string kPortData = MAGICPORTALS_PORT_DATA_DIR;
const std::string kLevels = MAGICPORTALS_LEVELS_DIR;
constexpr float kTick = 1.0f / 60.0f;
constexpr double kPi = 3.14159265358979323846;

#define CHECK_WITHIN(a, b, tolerance)                                                                       \
    do {                                                                                                    \
        const double withinA = (a);                                                                         \
        const double withinB = (b);                                                                         \
        CHECK_MSG(std::fabs(withinA - withinB) <= (tolerance), std::string(#a) + " is " +                   \
                                                                   std::to_string(withinA) + ", expected " + \
                                                                   std::to_string(withinB));                \
    } while (false)

Player::Tuning ReadTuning() {
    Player::Tuning tuning;
    std::string error;
    CHECK_MSG(Player::LoadTuning(kData + "/player.json", tuning, error), error);
    return tuning;
}

ScriptClock::Rules ReadRules() {
    ScriptClock::Rules rules;
    std::string error;
    CHECK_MSG(ScriptClock::LoadRules(kPortData + "/clock.json", rules, error), error);
    return rules;
}

// A player.json to be refused or taken, written to a scratch file.
bool Load(const std::string& text, Player::Tuning& out, std::string& error) {
    const std::filesystem::path path = std::filesystem::temp_directory_path() / "supersonic-test-mp-player.json";
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file << text;
    }
    const bool ok = Player::LoadTuning(path.string(), out, error);
    std::error_code ec;
    std::filesystem::remove(path, ec);
    return ok;
}

std::string TuningText(const std::string& speed = "3", const std::string& scale = "2",
                       const std::string& world = "10",
                       const std::string& hold = "{\"friction\": 0.9, \"probe_offset_px\": 3, \"probe_reach_px\": 6, "
                                                 "\"probe_line_tol_px\": 0.05}") {
    return "{\"movement\": {\"speed\": " + speed + ", \"elapsed_cap_ms\": 66, \"air_decay\": 0.86, "
           "\"air_decay_fps\": 60}, \"gravity\": {\"world_m_s2\": " + world + ", \"scale\": " + scale +
           ", \"max_translation_m\": 2, \"reference_px_per_unit\": 2.8125}, \"body\": {\"width_px\": 20, \"height_px\": 44}, "
           "\"feet\": {\"width_px\": 14, \"height_px\": 10, \"chamfer\": 0.6, \"touch_px\": 0.5}, "
           "\"hold\": " + hold + "}";
}

// A solid floor whose top is at y = 0 (the port's pixels, +y down), and a world with no gravity of
// its own: the player's is Steer's.
struct Scene {
    entt::registry registry;
    entt::entity player = entt::null;
    Player::Tuning tuning = ReadTuning();
    ScriptClock::Step step = ScriptClock::TickStep(ReadRules(), kTick);

    explicit Scene(bool withFloor, double playerYPx) {
        Game::UseNoGravity(registry);
        if (withFloor) {
            const entt::entity floor = registry.create();
            registry.emplace<TransformComponent>(floor).position = Units::ToWorld(0.0, 500.0);
            auto& box = registry.emplace<BoxColliderComponent>(floor);
            box.size = glm::vec3(Units::ToMetres(20000.0), Units::ToMetres(1000.0), 1.0f);
        }
        player = Player::Spawn(registry, glm::dvec2(0.0, playerYPx), tuning);
    }

    RigidBodyComponent& Rigid() { return registry.get<RigidBodyComponent>(player); }
    glm::dvec2 AtPx() { return Units::ToPixels(registry.get<TransformComponent>(player).position); }

    // One tick as the game takes it: steered, then the solver's step.
    void Tick(float direction) {
        Player::Steer(registry, player, tuning, direction, step);
        Supersonic::PhysicsSystem::Update(registry, kTick);
    }
};

void TheNumbersAreTheListingsAndAFileWithoutThemIsRefused() {
    const Player::Tuning tuning = ReadTuning();
    CHECK_EQ(tuning.speed, 3.0);              // SideScrollerCharacter ctor ins 13-15: SetV4 3f
    CHECK_EQ(tuning.elapsedCapMs, 66.0);      // applyForces ins 15: PshC4 66
    CHECK_EQ(tuning.airDecay, 0.86);          // applyForces ins 77: MULIf 0.86f (the float is 0.8600000143)
    CHECK_EQ(tuning.airDecayFps, 60.0);       // applyForces ins 75: 60f
    CHECK_EQ(tuning.worldGravity, 10.0);      // Game::preLoop ins 417: scale(10)
    CHECK_EQ(tuning.gravityScale, 2.0);       // SideScrollerCharacter ctor ins 58: SetGravityScale(2)
    CHECK_EQ(tuning.maxTranslationM, 2.0);       // b2_maxTranslation (b2Settings.h:101)
    CHECK_EQ(tuning.referencePxPerUnit, 2.8125); // 720 / 256, the line count of every capture
    // The ground sensor, char_feet.ent: 14 x 10, corners cut at 0.6, touching within Box2D's skin.
    CHECK_EQ(tuning.feetWidthPx, 14.0);
    CHECK_EQ(tuning.feetHeightPx, 10.0);
    CHECK_EQ(tuning.feetChamfer, 0.6);
    CHECK_EQ(tuning.feetTouchPx, 0.5);
    // The hold: dark_mage.ent's friction (UTF-16: friction="0.900000", fixedRotation="1"), and the port's own two probes.
    CHECK_EQ(tuning.holdFriction, 0.9);
    CHECK_EQ(tuning.holdProbeOffsetPx, 3.0);
    CHECK_EQ(tuning.holdProbeReachPx, 6.0);
    CHECK_EQ(tuning.holdProbeLineTolPx, 0.05);
    // The Godot project's guesses are not carried any more.
    CHECK_MSG(tuning.widthPx == 20.0 && tuning.heightPx == 44.0, "the capsule is still the guess, and still read");

    Player::Tuning out;
    std::string error;
    CHECK_MSG(Load(TuningText(), out, error), error);
    // A walk of nothing, a gravity scale of nothing or a world without gravity describes another game.
    CHECK(!Load(TuningText("0"), out, error));
    CHECK(!Load(TuningText("3", "0"), out, error));
    CHECK(!Load(TuningText("3", "2", "0"), out, error));
    CHECK(!Load(TuningText("-3"), out, error));
    // A body that holds nothing is the regression this section closed (the idle creep), and a hold with no probes
    // reads no slope: each refused, naming its key.
    const std::pair<std::string, std::string> refusedHolds[] = {
        {"{\"friction\": 0, \"probe_offset_px\": 3, \"probe_reach_px\": 6, \"probe_line_tol_px\": 0.05}", "hold.friction"},
        {"{\"friction\": \"x\", \"probe_offset_px\": 3, \"probe_reach_px\": 6, \"probe_line_tol_px\": 0.05}", "hold.friction"},
        {"{\"friction\": 0.9, \"probe_reach_px\": 6, \"probe_line_tol_px\": 0.05}", "hold.probe_offset_px"},
        {"{\"friction\": 0.9, \"probe_offset_px\": 3, \"probe_reach_px\": 0, \"probe_line_tol_px\": 0.05}", "hold.probe_reach_px"},
        {"{\"friction\": 0.9, \"probe_offset_px\": 3, \"probe_reach_px\": 6}", "hold.probe_line_tol_px"},
        {"{}", "hold.friction"},
    };
    for (const auto& [text, key] : refusedHolds) {
        CHECK(!Load(TuningText("3", "2", "10", text), out, error));
        CHECK_MSG(error.find(key) != std::string::npos, error);
    }
    // And a number missing: the port does not invent one.
    CHECK(!Load("{\"movement\": {\"speed\": 3}, \"gravity\": {\"world_m_s2\": 10, \"scale\": 2}, "
                "\"body\": {\"width_px\": 20, \"height_px\": 44}}",
                out, error));
    CHECK_MSG(error.find("elapsed_cap_ms") != std::string::npos, error);
    CHECK(!Player::LoadTuning(kData + "/no_such_player.json", out, error));
}

void TheWalkIsTwoPointFourUnitsATick() {
    const Player::Tuning tuning = ReadTuning();
    const ScriptClock::Rules rules = ReadRules();
    const ScriptClock::Step step = ScriptClock::TickStep(rules, kTick);

    // speedScale = scale(3) x min(66, 16) / 33.3333: 1.44 metres a second of the original's physics.
    CHECK_WITHIN(Player::WalkOriginal(tuning, step), 3.0 * 16.0 / (rules.physicsStepS * 1000.0), 1e-12);
    CHECK_WITHIN(Player::WalkOriginal(tuning, step), 1.44, 1e-6);
    // Carried at the physics ratio 2: 2.88 m/s of the port's 1/60 s step, which is the 2.4 units
    // a frame (3 x 16 ms) the original's 1/30 s step covers at 1.44.
    CHECK_WITHIN(ScriptClock::SimRatio(step), 2.0, 1e-12);
    CHECK_WITHIN(Player::WalkCarried(tuning, step), 2.88, 1e-6);
    const double unitsATick = Player::WalkCarried(tuning, step) * static_cast<double>(kTick) * Units::kPixelsPerMetre;
    CHECK_WITHIN(unitsATick, 2.4, 1e-6);
    CHECK_WITHIN(unitsATick * 60.0, 144.0, 1e-4);
    // 150 units a second of the script's metre, times 16 / 16.667: the floor's 4.17 % less.
    CHECK_WITHIN(unitsATick * 60.0, 150.0 * 16.0 / (1000.0 / 60.0), 1e-4);

    // The cap, min(66, ms): a frame of 200 ms is the 66, and the walk of one scales with it.
    const ScriptClock::Step slow(0.2, kTick, rules.physicsStepS, 5.0);
    CHECK_WITHIN(Player::WalkOriginal(tuning, slow), 3.0 * 66.0 / (rules.physicsStepS * 1000.0), 1e-9);
    // And below it, the walk is the script's frame: 33 ms of a 30 Hz frame, twice 16.5 of a half.
    const ScriptClock::Step thirty(0.033, kTick, rules.physicsStepS, 30.0);
    CHECK_WITHIN(Player::WalkOriginal(tuning, thirty), 3.0 * 33.0 / (rules.physicsStepS * 1000.0), 1e-9);

    // A pure suite's bare float is the original's physics second one with its tick: 3 m/s, the
    // 150 units a second the script's metre gives with no floor.
    const ScriptClock::Step bare = kTick;
    CHECK_WITHIN(Player::WalkCarried(tuning, bare) * Units::kPixelsPerMetre, 150.0, 1e-3);
}

void TheWalkIsSetOutrightAndStopsDead() {
    Scene scene(true, -23.0);
    // Settle onto the floor: the fall into it is taken by the contact.
    for (int tick = 0; tick < 40; ++tick) scene.Tick(0.0f);
    const double restY = scene.AtPx().y;
    CHECK_WITHIN(restY, -22.0, 1.5);
    CHECK_MSG(Player::Grounded(scene.registry, scene.player, scene.tuning), "standing on the floor");

    const double fromX = scene.AtPx().x;
    // NO RAMP: the first tick's velocity is the walk, whole.
    Player::Steer(scene.registry, scene.player, scene.tuning, 1.0f, scene.step);
    CHECK_WITHIN(scene.Rigid().velocity.x, 2.88, 1e-5);
    Supersonic::PhysicsSystem::Update(scene.registry, kTick);
    for (int tick = 1; tick < 60; ++tick) scene.Tick(1.0f);
    // A second at 60 Hz: 60 ticks of 2.4 units.
    CHECK_WITHIN(scene.AtPx().x - fromX, 144.0, 0.5);
    CHECK_WITHIN(scene.Rigid().velocity.x, 2.88, 1e-4);

    // The other way reverses at once, with no ramp through zero.
    Player::Steer(scene.registry, scene.player, scene.tuning, -1.0f, scene.step);
    CHECK_WITHIN(scene.Rigid().velocity.x, -2.88, 1e-5);
    Supersonic::PhysicsSystem::Update(scene.registry, kTick);

    // No pad held is a velocity of nothing: the dead stop, not a friction's slide.
    Player::Steer(scene.registry, scene.player, scene.tuning, 0.0f, scene.step);
    CHECK_EQ(scene.Rigid().velocity.x, 0.0f);
    Supersonic::PhysicsSystem::Update(scene.registry, kTick);
    const double stoppedX = scene.AtPx().x;
    for (int tick = 0; tick < 30; ++tick) scene.Tick(0.0f);
    CHECK_WITHIN(scene.AtPx().x, stoppedX, 0.01);
}

void InTheAirThePadIsIgnoredAndTheXVelocityDecays() {
    const Player::Tuning tuning = ReadTuning();
    const ScriptClock::Rules rules = ReadRules();
    const ScriptClock::Step step = ScriptClock::TickStep(rules, kTick);
    // min(1, GetFPSRate() / 60 x 0.86).
    CHECK_WITHIN(Player::AirShare(tuning, step), 0.86, 1e-12);
    CHECK_WITHIN(Player::AirShare(tuning, ScriptClock::Step(0.033, kTick, rules.physicsStepS, 30.0)), 0.43, 1e-12);
    CHECK_WITHIN(Player::AirShare(tuning, ScriptClock::Step(0.017, kTick, rules.physicsStepS, 17.0)), 0.86 * 17.0 / 60.0,
                 1e-12);
    // From 70 frames a second it is 1: the min.
    CHECK_WITHIN(Player::AirShare(tuning, ScriptClock::Step(0.008, kTick, rules.physicsStepS, 120.0)), 1.0, 1e-12);
    CHECK_WITHIN(Player::AirShare(tuning, ScriptClock::Step(0.014, kTick, rules.physicsStepS, 70.0)), 1.0, 1e-12);

    // The same, through Steer on a body with no floor under it, and the pad held either way or not at all.
    for (const float pad : {-1.0f, 0.0f, 1.0f}) {
        Scene scene(false, -300.0);
        scene.Rigid().velocity.x = 2.88f;
        CHECK_MSG(!Player::Grounded(scene.registry, scene.player, scene.tuning), "nothing under the feet");
        double expected = 2.88;
        for (int tick = 1; tick <= 8; ++tick) {
            Player::Steer(scene.registry, scene.player, scene.tuning, pad, scene.step);
            expected *= 0.86;
            CHECK_WITHIN(scene.Rigid().velocity.x, expected, 1e-5);
            Supersonic::PhysicsSystem::Update(scene.registry, kTick);
        }
        // Eight ticks leave 0.86^8 = 29.8 % of it: the brake F13's 1-06 walk off the block shows.
        CHECK_WITHIN(expected / 2.88, std::pow(0.86, 8.0), 1e-12);
    }
}

void TheFallIsFourThousandUnitsASecondSquared() {
    const Player::Tuning tuning = ReadTuning();
    const ScriptClock::Rules rules = ReadRules();
    const ScriptClock::Step step = ScriptClock::TickStep(rules, kTick);
    // World gravity 10 x gravity scale 2 = 20 m/s^2, a velocity of 20 / 30 m/s a frame of the original's
    // physics, and the port carries it at the ratio 2: 1.3333 m/s a tick, 1.111 units a tick squared.
    CHECK_WITHIN(Player::FallPerTick(tuning, step), 20.0 * rules.physicsStepS * 2.0, 1e-12);
    CHECK_WITHIN(Player::FallPerTick(tuning, step), 4.0 / 3.0, 1e-6);
    const double perTickUnits = Player::FallPerTick(tuning, step) * Units::kPixelsPerMetre * static_cast<double>(kTick);
    CHECK_WITHIN(perTickUnits, 20.0 * 50.0 * rules.physicsStepS * rules.physicsStepS, 1e-9);
    CHECK_WITHIN(perTickUnits, 1.0 / 0.9, 1e-6);
    CHECK_WITHIN(Player::FallPerTick(tuning, step) * Units::kPixelsPerMetre / static_cast<double>(kTick), 4000.0, 1e-3);
    // A pure suite's bare float is 1000 units a second squared: the original's own second.
    CHECK_WITHIN(Player::FallPerTick(tuning, ScriptClock::Step(kTick)) * Units::kPixelsPerMetre /
                     static_cast<double>(kTick),
                 1000.0, 1e-3);

    // Through the solver: free of any floor, n ticks leave n a squared over 2 plus n a over 2 - Box2D's
    // order, the velocity first and then the move. Thirty ticks, under the clamp a fall meets on the 33rd
    // (below): half a second of it covers 516 units, where the original's physics covers a full second's
    // worth of its own - a wall second is four times a real-time fall.
    Scene scene(false, -2500.0);
    const double fromY = scene.AtPx().y;
    for (int tick = 1; tick <= 30; ++tick) {
        scene.Tick(0.0f);
        const double fallen = scene.AtPx().y - fromY;
        const double wanted = perTickUnits * tick * (tick + 1) / 2.0;
        CHECK_MSG(std::fabs(fallen - wanted) < 0.05 + 1e-4 * wanted,
                  "tick " + std::to_string(tick) + ": fallen " + std::to_string(fallen) + ", wanted " +
                      std::to_string(wanted));
        if (std::fabs(fallen - wanted) >= 0.05 + 1e-4 * wanted) break;
    }
    CHECK_WITHIN(scene.Rigid().velocity.y, -30.0 * Player::FallPerTick(tuning, step), 1e-3);
    CHECK_WITHIN(scene.AtPx().y - fromY, perTickUnits * 30.0 * 31.0 / 2.0, 1.0);
}

void ABodyThatFallsOnIsClampedToBox2DsMaxTranslation() {
    const Player::Tuning tuning = ReadTuning();
    const ScriptClock::Step step = ScriptClock::TickStep(ReadRules(), kTick);
    // 2 m of Box2D's a step is 100 screen pixels, 35.6 units at the 720-line reference: 0.711 m of the
    // port's a tick, 42.7 m/s. A fall of 256 units never reaches it (23.9 units a tick)...
    const double limitM = tuning.maxTranslationM / tuning.referencePxPerUnit;
    CHECK_WITHIN(limitM, 0.7111111, 1e-6);
    CHECK_WITHIN(limitM * Units::kPixelsPerMetre, 35.5556, 1e-3);
    Scene scene(false, -2500.0);
    double fastest = 0.0;
    int reachedOn = -1;
    for (int tick = 1; tick <= 200; ++tick) {
        scene.Tick(0.0f);
        const double perTickUnits = -static_cast<double>(scene.Rigid().velocity.y) * Units::kPixelsPerMetre *
                                    static_cast<double>(kTick);
        fastest = std::max(fastest, perTickUnits);
        if (reachedOn < 0 && perTickUnits >= limitM * Units::kPixelsPerMetre - 1e-3) reachedOn = tick;
        if (tick == 20) CHECK_MSG(perTickUnits < 0.7 * limitM * Units::kPixelsPerMetre, "twenty ticks of falling are under the clamp");
    }
    // ... and a body that goes on falling does, on the tick its translation passes it (the 33rd: 1.111 x 32 = 35.6),
    // and goes no faster after.
    CHECK_MSG(reachedOn >= 31 && reachedOn <= 34, "reached the clamp on tick " + std::to_string(reachedOn));
    CHECK_WITHIN(fastest, limitM * Units::kPixelsPerMetre, 1e-3);
    CHECK_WITHIN(-scene.Rigid().velocity.y, limitM / static_cast<double>(kTick), 1e-3);
    // The clamp scales the whole velocity, direction kept (b2Island.cpp:286).
    scene.Rigid().velocity = glm::vec3(60.0f, -60.0f, 0.0f);
    Player::Steer(scene.registry, scene.player, scene.tuning, 0.0f, scene.step);
    const glm::vec3 v = scene.Rigid().velocity;
    CHECK_WITHIN(std::hypot(static_cast<double>(v.x), static_cast<double>(v.y)) * static_cast<double>(kTick), limitM, 1e-5);
    CHECK_WITHIN(v.y / v.x, -(60.0 + Player::FallPerTick(tuning, step)) / (60.0 * Player::AirShare(tuning, step)), 1e-4);
}

void TheLandingSpeedIsReadAsTheOriginalsCallbackReadsIt() {
    // Dropped 100 units onto the floor. The original's body falls v^2 = 2 g d, g = 1000 units a second
    // squared of its physics, so 447 units a second and a hitIntensity (over 50) of 8.94 - whatever frame
    // rate it runs at, since its step is the fixed 1/30 s. The port carries twice that, and a begin
    // reports the player's velocity by 1 / SimRatio.
    Scene scene(true, -122.0);
    Impacts::Watch watch;
    Impacts::Watched watched{scene.player, Impacts::Kind::Player, {}};
    watched.velocityScale = 1.0 / ScriptClock::SimRatio(scene.step);
    double hit = 0.0;
    double carried = 0.0;
    for (int tick = 0; tick < 90 && hit == 0.0; ++tick) {
        // The layer's order: steered, then what the watch carries into the step, then the step.
        Player::Steer(scene.registry, scene.player, scene.tuning, 0.0f, scene.step);
        watch.BeforeStep(scene.registry, {watched});
        carried = std::max(carried, -static_cast<double>(scene.Rigid().velocity.y) * Units::kPixelsPerMetre);
        Supersonic::PhysicsSystem::Update(scene.registry, kTick);
        for (const Impacts::Begin& begin : watch.AfterStep(scene.registry, 0.5, static_cast<double>(kTick))) {
            if (begin.kind != Impacts::Kind::Player) continue;
            const glm::dvec2 approach = begin.Approaching(begin.normal);
            hit = std::fabs(glm::dot(approach, begin.normal)) / 50.0;
        }
    }
    std::printf("  a 100-unit drop: the port carried %.1f units a second, the callback reads hitIntensity %.2f\n",
                carried, hit);
    CHECK_MSG(hit > 0.0, "the landing began a contact");
    // The continuous fall is v = sqrt(2 x 1000 x 100) = 447 units a second, hitIntensity 8.94. The frames
    // are discrete: the original's own body, falling 1.111 units a frame squared, is 101 units down on its
    // 13th frame at 433 units a second (8.67) when its contact begins, and the port's solver brakes the
    // closing pair a step before it meets - from 13.3 units a tick off the floor - so the speed the watch
    // reads is the 12th tick's, 400 (8.0). Within 11 % of the continuous figure, in the original's own units:
    // the old Godot gravity's 100-unit drop read 9.8.
    CHECK_MSG(std::fabs(hit - std::sqrt(2.0 * 1000.0 * 100.0) / 50.0) < 1.0,
              "about 8.9, read 7.5 to 9.5: " + std::to_string(hit));
    // And the port's own speed was twice what the callback reads: the ratio is the one seam.
    CHECK_MSG(carried / 50.0 > 1.8 * hit && carried / 50.0 < 2.2 * hit, "the port carries it at the ratio");
}

void TheFeetAreAnOctagonFourteenWideAndAWalkerLeavesTheGroundSevenClear() {
    // A floor whose right edge is x = 0 and whose top is y = 0, the player's capsule resting on it.
    entt::registry registry;
    Game::UseNoGravity(registry);
    const entt::entity floor = registry.create();
    registry.emplace<TransformComponent>(floor).position = Units::ToWorld(-10000.0, 500.0);
    auto& box = registry.emplace<BoxColliderComponent>(floor);
    box.size = glm::vec3(Units::ToMetres(20000.0), Units::ToMetres(1000.0), 1.0f);
    const Player::Tuning tuning = ReadTuning();
    const entt::entity player = Player::Spawn(registry, glm::dvec2(0.0, -22.0), tuning);
    const auto groundedAt = [&](double xPx, double yPx) {
        registry.get<TransformComponent>(player).position = Units::ToWorld(xPx, yPx);
        return Player::Grounded(registry, player, tuning);
    };
    // Standing on it, anywhere over it: the sensor is 14 wide, so the centre may be 7 past the edge
    // (the octagon is full width at the floor's plane) and the player is still on the ground...
    CHECK(groundedAt(-30.0, -22.0));
    CHECK(groundedAt(0.0, -22.0));
    CHECK_MSG(groundedAt(6.9, -22.0), "feet 7 wide: the centre 6.9 past the edge is still on it");
    // ... and off it once the whole sensor is clear (7 and the skin).
    CHECK_MSG(!groundedAt(7.6, -22.0), "7.6 past the edge, the feet are clear");
    // It reaches 5 below the bottom of the body and 5 above: hovering 4 units over the floor is on it,
    // 6 is not; sunk 4 into it is.
    CHECK(groundedAt(-30.0, -26.0));
    CHECK_MSG(!groundedAt(-30.0, -28.5), "6.5 over the floor is not");
    CHECK(groundedAt(-30.0, -18.0));
    // A body beside the sensor's side: the floor's edge is a face, and falling along it, within 7 of it,
    // the player is still touching it, where a ray from the centre would have left it a step ago.
    CHECK_MSG(groundedAt(5.0, 40.0), "falling past the floor's side face within 7 of it is still on it");
    CHECK_MSG(!groundedAt(9.0, 40.0), "and clear of it, not");
    // A trigger is not solid ground: the sensor ignores a sensor.
    const entt::entity trigger = registry.create();
    registry.emplace<TransformComponent>(trigger).position = Units::ToWorld(100.0, 0.0);
    auto& area = registry.emplace<BoxColliderComponent>(trigger);
    area.size = glm::vec3(Units::ToMetres(200.0), Units::ToMetres(40.0), 1.0f);
    area.isTrigger = true;
    CHECK_MSG(!groundedAt(100.0, -22.0), "standing in a trigger over nothing is not on the ground");
    // And the player's own capsule never counts.
    CHECK_MSG(!groundedAt(500.0, -500.0), "in the air, nothing");
}

void TheRecoilIsSeventyUnitsAPhysicsSecond() {
    const ScriptClock::Rules rules = ReadRules();
    const ScriptClock::Step step = ScriptClock::TickStep(rules, kTick);
    Zerog::Rules zero;
    std::string error;
    CHECK_MSG(Zerog::LoadRules(kPortData + "/zerog.json", zero, error), error);
    // scale(1.4): 70 units a second of the original's physics.
    CHECK_WITHIN(zero.recoilMetresPerSecond * Units::kPixelsPerMetre, 70.0, 1e-9);
    // The shooter is carried at the ratio: 2.33 units a tick, 140 a second of wall time at 60 Hz.
    const glm::vec3 shove = Zerog::Impulse(glm::dvec2(0.0), glm::dvec2(100.0, 0.0),
                                           zero.recoilMetresPerSecond * ScriptClock::SimRatio(step));
    CHECK_WITHIN(-shove.x * Units::kPixelsPerMetre * static_cast<double>(kTick), 7.0 / 3.0, 1e-5);
    CHECK_WITHIN(-shove.x * Units::kPixelsPerMetre, 140.0, 1e-4);
    // The footage: F10 filmed 17-19 frames a second, where the same shove is 70 x fps / 30.
    const double at17 = 70.0 * 17.0 / 30.0;
    const double at19 = 70.0 * 19.0 / 30.0;
    CHECK_MSG(at17 > 39.0 && at17 < 40.5 && at19 > 44.0 && at19 < 45.0,
              "40-44 units a second, as F10 measured: " + std::to_string(at17) + " to " + std::to_string(at19));
}

// A tilted floor: a box 600 x 200 units turned by `angleRad`, a player dropped on it near its middle.
// `holdFriction` is the tuning's hold, which a case sets to a thousandth of nothing to see what the player did
// without it (the loader refuses a zero; a body set past it is the same as no hold: no slope passes the gate).
struct Slope {
    entt::registry registry;
    entt::entity player = entt::null;
    Player::Tuning tuning = ReadTuning();
    ScriptClock::Step step = ScriptClock::TickStep(ReadRules(), kTick);

    Slope(double angleRad, double holdFriction) {
        tuning.holdFriction = holdFriction;
        Game::UseNoGravity(registry);
        const entt::entity floor = registry.create();
        auto& transform = registry.emplace<TransformComponent>(floor);
        transform.position = Units::ToWorld(0.0, 500.0);
        transform.rotation.z = Units::ToWorldRotation(angleRad);
        auto& box = registry.emplace<BoxColliderComponent>(floor);
        box.size = glm::vec3(Units::ToMetres(600.0), Units::ToMetres(200.0), 1.0f);
        // A level's static floor: Godot's defaults, friction 1 and no bounce, through the material
        // LevelBuilder gives it. Left without one the engine's own would bounce the sink off.
        auto& material = registry.emplace<PhysicsMaterialComponent>(floor);
        material.friction = LevelBuilder::kStaticFriction;
        material.restitution = LevelBuilder::kStaticRestitution;
        // 12 units over the surface where the box's centre line meets it: 100 / cos above the box's centre.
        player = Player::Spawn(registry, glm::dvec2(0.0, 500.0 - 100.0 / std::cos(angleRad) - 22.0 - 12.0), tuning);
    }

    glm::dvec2 AtPx() { return Units::ToPixels(registry.get<TransformComponent>(player).position); }
    RigidBodyComponent& Rigid() { return registry.get<RigidBodyComponent>(player); }
    void Tick(float direction) {
        Player::Steer(registry, player, tuning, direction, step);
        Supersonic::PhysicsSystem::Update(registry, kTick);
    }
};

constexpr double kNoHold = 1e-9;

void TheGroundSlopeIsReadFromTwoProbesUnderTheCentreLine() {
    // The slope the box was turned to, on either side of the flat.
    const double angle = 15.0 * kPi / 180.0;
    for (const double sign : {1.0, -1.0}) {
        Slope slope(sign * angle, ReadTuning().holdFriction);
        for (int tick = 0; tick < 60; ++tick) slope.Tick(0.0f);
        double read = 0.0;
        CHECK_MSG(Player::GroundSlope(slope.registry, slope.player, slope.tuning, read), "the probes meet the box");
        // Which way is not the case's to say (the sign of a rotation is the port's), how steep is: tan 15 degrees.
        CHECK_WITHIN(std::fabs(read), std::tan(angle), 1e-3);
    }

    // The flat floor reads exactly 0, whatever the player's x.
    Scene flat(true, -23.0);
    for (int tick = 0; tick < 40; ++tick) flat.Tick(0.0f);
    double read = 1.0;
    CHECK_MSG(Player::GroundSlope(flat.registry, flat.player, flat.tuning, read), "the probes meet the floor");
    CHECK_EQ(read, 0.0);

    // Nothing under the centre line within the probes' reach: no slope, and the player is steered as he was.
    Scene air(false, -300.0);
    CHECK_MSG(!Player::GroundSlope(air.registry, air.player, air.tuning, read), "nothing under the feet");

    // A round top is no plane: three probes over a sphere's crown are not in one line, so nothing is read and the
    // player is steered as he was (a stone or a minion under him; a chamfer's corner within the probes' span the same).
    {
        entt::registry roundTop;
        Game::UseNoGravity(roundTop);
        const entt::entity ball = roundTop.create();
        roundTop.emplace<TransformComponent>(ball).position = Units::ToWorld(0.0, 60.0);
        roundTop.emplace<Supersonic::SphereColliderComponent>(ball).radius = Units::ToMetres(40.0);
        const Player::Tuning ballTuning = ReadTuning();
        // The crown is at y 20; the player's bottom is at 22: the probes (6 either way of it) meet the sphere.
        const entt::entity rider = Player::Spawn(roundTop, glm::dvec2(0.0, 0.0), ballTuning);
        double crown = 1.0;
        CHECK_MSG(!Player::GroundSlope(roundTop, rider, ballTuning, crown), "a round top is not a plane");
    }

    // The floor's right edge at x = 0 and the player's centre over it: one probe meets it and the other does not.
    entt::registry registry;
    Game::UseNoGravity(registry);
    const entt::entity floor = registry.create();
    registry.emplace<TransformComponent>(floor).position = Units::ToWorld(-10000.0, 500.0);
    auto& box = registry.emplace<BoxColliderComponent>(floor);
    box.size = glm::vec3(Units::ToMetres(20000.0), Units::ToMetres(1000.0), 1.0f);
    const Player::Tuning tuning = ReadTuning();
    const entt::entity player = Player::Spawn(registry, glm::dvec2(1.0, -22.0), tuning);
    CHECK_MSG(!Player::GroundSlope(registry, player, tuning, read), "a probe past the edge misses: not a slope");
}

void AnIdlePlayerIsPressedAlongTheGroundsNormalAndAWalkingOneIsNot() {
    const double angle = 15.0 * kPi / 180.0;
    Slope slope(angle, ReadTuning().holdFriction);
    for (int tick = 0; tick < 60; ++tick) slope.Tick(0.0f);
    CHECK_MSG(Player::Grounded(slope.registry, slope.player, slope.tuning), "standing on the slope");
    double read = 0.0;
    CHECK(Player::GroundSlope(slope.registry, slope.player, slope.tuning, read));
    const double fall = Player::FallPerTick(slope.tuning, slope.step);

    // Idle: x velocity -slope x g, and the tick of gravity added straight down as ever - together a sink along the
    // normal (the slope is the ratio of the two, the sign that of a push INTO the surface).
    const float vyBefore = slope.Rigid().velocity.y;
    Player::Steer(slope.registry, slope.player, slope.tuning, 0.0f, slope.step);
    CHECK_WITHIN(slope.Rigid().velocity.x, -read * fall, 1e-5);
    CHECK_WITHIN(slope.Rigid().velocity.y - vyBefore, -fall, 1e-5);
    CHECK_WITHIN(slope.Rigid().velocity.x / (slope.Rigid().velocity.y - vyBefore), read, 1e-5);
    // A pad held: the walk, whole, as it was - the hold is for an idle player and nothing else.
    for (const float pad : {1.0f, -1.0f}) {
        Player::Steer(slope.registry, slope.player, slope.tuning, pad, slope.step);
        CHECK_WITHIN(slope.Rigid().velocity.x, pad * Player::WalkCarried(slope.tuning, slope.step), 1e-5);
    }

    // On the flat it is the old steering exactly: x velocity 0.
    Scene flat(true, -23.0);
    for (int tick = 0; tick < 40; ++tick) flat.Tick(0.0f);
    Player::Steer(flat.registry, flat.player, flat.tuning, 0.0f, flat.step);
    CHECK_EQ(flat.Rigid().velocity.x, 0.0f);

    // Past what friction holds - tan 50 degrees 1.19 against the 0.949 of the 0.9 body's mix with a floor of 1 -
    // the player is steered as before and goes down it, as the original's does on a ramp that steep.
    Slope steep(50.0 * kPi / 180.0, ReadTuning().holdFriction);
    for (int tick = 0; tick < 60; ++tick) steep.Tick(0.0f);
    if (Player::Grounded(steep.registry, steep.player, steep.tuning)) {
        Player::Steer(steep.registry, steep.player, steep.tuning, 0.0f, steep.step);
        CHECK_EQ(steep.Rigid().velocity.x, 0.0f);
    }
    CHECK_WITHIN(Player::HoldTan(ReadTuning()), std::sqrt(0.9), 1e-6);
    // The weightless levels never call Steer, and the airborne player is not pressed anywhere: no hold in the air.
    Scene air(false, -300.0);
    air.Rigid().velocity.x = 2.88f;
    Player::Steer(air.registry, air.player, air.tuning, 0.0f, air.step);
    CHECK_WITHIN(air.Rigid().velocity.x, 2.88 * 0.86, 1e-5);
}

void AnIdlePlayerStandsStillOnASlope() {
    // 1-24's ramp is 15 degrees. A frictionless body sinks into it a tick of gravity straight down and is pushed out
    // along its normal: 1.111 x sin 15 x cos 15 = 0.278 units a tick in x, 117 units in 420 ticks along the slope,
    // which is what the port's idle player did before the hold. 30 degrees (tan 0.577) is held as well.
    for (const double degrees : {15.0, 30.0}) {
        for (const double sign : {1.0, -1.0}) {
            Slope held(sign * degrees * kPi / 180.0, ReadTuning().holdFriction);
            Slope loose(sign * degrees * kPi / 180.0, kNoHold);
            for (int tick = 0; tick < 90; ++tick) {
                held.Tick(0.0f);
                loose.Tick(0.0f);
            }
            CHECK_MSG(Player::Grounded(held.registry, held.player, held.tuning), "the held player is on the slope");
            const glm::dvec2 from = held.AtPx();
            const glm::dvec2 looseFrom = loose.AtPx();
            double worst = 0.0;
            for (int tick = 0; tick < 420; ++tick) {
                held.Tick(0.0f);
                loose.Tick(0.0f);
                worst = std::max(worst, glm::length(held.AtPx() - from));
            }
            const double crept = glm::length(loose.AtPx() - looseFrom);
            std::printf("  a %.0f-degree floor, idle 420 ticks: held %.3f units from where it stood, without the hold %.1f\n",
                        degrees, worst, crept);
            // The pin the G10 verifier asked for: within 1 unit for 420 ticks.
            CHECK_MSG(worst < 1.0, "the idle player moved " + std::to_string(worst) + " units");
            // And the control: without the hold it goes, so the case cannot pass on a floor that holds by itself.
            CHECK_MSG(crept > 50.0, "the control crept " + std::to_string(crept) + " units");
        }
    }
}

void TheWalkIsTheSameWithTheHoldAsWithout() {
    // A pad held is not steered by the hold at all, so the walk - which F13 shows losing nothing - is the walk of the
    // body with no hold, from a stand on the slope as well as on the flat.
    const double angle = 15.0 * kPi / 180.0;
    Slope held(angle, ReadTuning().holdFriction);
    Slope loose(angle, kNoHold);
    for (int tick = 0; tick < 90; ++tick) {
        held.Tick(0.0f);
        loose.Tick(0.0f);
    }
    for (const float pad : {1.0f, -1.0f}) {
        const glm::dvec2 heldFrom = held.AtPx();
        const glm::dvec2 looseFrom = loose.AtPx();
        for (int tick = 0; tick < 10; ++tick) {
            held.Tick(pad);
            loose.Tick(pad);
        }
        const glm::dvec2 heldWalked = held.AtPx() - heldFrom;
        const glm::dvec2 looseWalked = loose.AtPx() - looseFrom;
        // The two stand at other places on the slope (the loose one has crept 130 units), so a few hundredths of
        // a unit of the solver's state differ; a grip would take up to 28 % of each step, 6.7 units of the ten.
        CHECK_WITHIN(heldWalked.x, looseWalked.x, 0.1);
        CHECK_WITHIN(heldWalked.y, looseWalked.y, 0.1);
        // And it is a walk: ten ticks of 2.4 units along the slope, less its cosine.
        CHECK_MSG(std::fabs(heldWalked.x) > 20.0, "ten ticks of the walk moved " + std::to_string(heldWalked.x));
    }
}

bool LevelIsThere(const std::string& name) {
    std::error_code ec;
    if (std::filesystem::is_regular_file(kLevels + "/" + name + ".tscn", ec) &&
        std::filesystem::is_regular_file(kData + "/entity_roles.json", ec)) {
        return true;
    }
    std::printf("  %s SKIPPED - needs the converted level at %s/%s.tscn and Enhanced's data at %s\n", name.c_str(),
                kLevels.c_str(), name.c_str(), kData.c_str());
    return false;
}

// A level started and stood in for `ticks` with no pad held, after `settle` ticks to land: the most the
// player moved from where it stood when the count began.
double StandingDrift(const std::string& name, int settle, int ticks, double holdFriction, double& endY,
                     bool unitMasses = false) {
    Game::Data data;
    entt::registry registry;
    Game::Level level;
    std::string error;
    const std::filesystem::path prisms = std::filesystem::temp_directory_path() / "supersonic-test-mp-player";
    if (!Game::LoadData(kLevels + "/" + name + ".tscn", kData, prisms, data, error) ||
        !Game::Start(data, registry, level, error)) {
        CHECK_MSG(false, name + ": " + error);
        return 0.0;
    }
    data.tuning.holdFriction = holdFriction;
    // The control of the barrel case: every body as heavy as every other, the engine's default of 1, which is how
    // the port weighed them until planning step 152.
    if (unitMasses) {
        for (const entt::entity body : registry.view<RigidBodyComponent>()) registry.get<RigidBodyComponent>(body).mass = 1.0f;
    }
    const ScriptClock::Step step = ScriptClock::TickStep(ReadRules(), kTick);
    for (int tick = 0; tick < settle; ++tick) Game::Tick(data, registry, level, 0.0f, step);
    const auto at = [&] { return Units::ToPixels(registry.get<TransformComponent>(level.player).position); };
    const glm::dvec2 from = at();
    double worst = 0.0;
    for (int tick = 0; tick < ticks; ++tick) {
        Game::Tick(data, registry, level, 0.0f, step);
        worst = std::max(worst, glm::length(at() - from));
    }
    endY = at().y - from.y;
    return worst;
}

void AnIdlePlayerStandsStillOnTheOriginalsRamp() {
    // 1-24 (level23): the original's stills at 2.0, 4.5 and 8.0 s show the idle player at the same pixel on
    // ramp_right; the port's slid 0.278 units a tick down it and took the timed crystal on tick 328. The G10
    // verifier's pin: within 1 unit for 420 ticks.
    const std::string name = "level23";
    if (!LevelIsThere(name)) return;
    double endY = 0.0;
    double looseEndY = 0.0;
    const double held = StandingDrift(name, 60, 420, ReadTuning().holdFriction, endY);
    const double loose = StandingDrift(name, 60, 420, kNoHold, looseEndY);
    std::printf("  %s idle 420 ticks: held %.3f units from where it stood (y %+.3f), without the hold %.1f\n", name.c_str(),
                held, endY, loose);
    CHECK_MSG(held < 1.0, name + ": the idle player moved " + std::to_string(held) + " units");
    // The regression itself, so the pin cannot pass on a level that never moved him.
    CHECK_MSG(loose > 50.0, name + ": without the hold it moved " + std::to_string(loose) + " units");

    // 2-24 (level23a): the player starts on barrel_bomb_ent_1135's edge, and the port's player left it by tick 140 with the
    // barrel itself moving under him (the hold has no slope to read on a flat top: G10-5b). Not the world's time base, which
    // moved it from 36.0 to 36.3 units: the MASS. The engine weighed every body 1, so a player standing on a barrel's edge
    // tipped it out from under him; the original's barrel (density 1 over 28.6 x 37.7 units) is 4.15 times the player
    // (density 0.5 over two circles of radius 9.1), and the port's bodies now weigh what the .ent files say
    // (materials.json). The G10 verifier's pin, as for the ramp: within 1 unit for 420 ticks.
    if (LevelIsThere("level23a")) {
        const double barrelHeld = StandingDrift("level23a", 60, 420, ReadTuning().holdFriction, endY);
        const double barrelEvenWeights = StandingDrift("level23a", 60, 420, ReadTuning().holdFriction, looseEndY, true);
        std::printf("  level23a idle 420 ticks: %.3f units from where it stood (y %+.3f), with every body weighing 1 (the engine's, "
                    "and the port's until step 152) %.1f (y %+.3f)\n",
                    barrelHeld, endY, barrelEvenWeights, looseEndY);
        CHECK_MSG(barrelHeld < 1.0, "level23a: the idle player moved " + std::to_string(barrelHeld) + " units");
        // The regression itself, so the pin cannot pass on a level that never moved him.
        CHECK_MSG(barrelEvenWeights > 10.0, "level23a: with even weights he moved " + std::to_string(barrelEvenWeights) + " units");
    }
}

void runTests() {
    TheNumbersAreTheListingsAndAFileWithoutThemIsRefused();
    TheWalkIsTwoPointFourUnitsATick();
    TheWalkIsSetOutrightAndStopsDead();
    InTheAirThePadIsIgnoredAndTheXVelocityDecays();
    TheFeetAreAnOctagonFourteenWideAndAWalkerLeavesTheGroundSevenClear();
    TheFallIsFourThousandUnitsASecondSquared();
    ABodyThatFallsOnIsClampedToBox2DsMaxTranslation();
    TheLandingSpeedIsReadAsTheOriginalsCallbackReadsIt();
    TheRecoilIsSeventyUnitsAPhysicsSecond();
    TheGroundSlopeIsReadFromTwoProbesUnderTheCentreLine();
    AnIdlePlayerIsPressedAlongTheGroundsNormalAndAWalkingOneIsNot();
    AnIdlePlayerStandsStillOnASlope();
    TheWalkIsTheSameWithTheHoldAsWithout();
    AnIdlePlayerStandsStillOnTheOriginalsRamp();
}

} // namespace

MP_TEST_MAIN("test_mp_player", 100)
