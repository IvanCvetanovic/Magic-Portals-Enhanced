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
//
// Pure: a floor, a capsule and the engine's solver, no level and no window.

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "core/Components.hpp"
#include "core/PhysicsSystem.hpp"
#include "sim/Game.hpp"
#include "sim/Impacts.hpp"
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
#include <utility>
#include <vector>

using namespace MagicPortals;
using Supersonic::BoxColliderComponent;
using Supersonic::RigidBodyComponent;
using Supersonic::TransformComponent;

namespace {

const std::string kData = MAGICPORTALS_DATA_DIR;
const std::string kPortData = MAGICPORTALS_PORT_DATA_DIR;
constexpr float kTick = 1.0f / 60.0f;

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
                       const std::string& world = "10") {
    return "{\"movement\": {\"speed\": " + speed + ", \"elapsed_cap_ms\": 66, \"air_decay\": 0.86, "
           "\"air_decay_fps\": 60}, \"gravity\": {\"world_m_s2\": " + world + ", \"scale\": " + scale +
           ", \"max_translation_m\": 2, \"reference_px_per_unit\": 2.8125}, \"body\": {\"width_px\": 20, \"height_px\": 44}, "
           "\"feet\": {\"width_px\": 14, \"height_px\": 10, \"chamfer\": 0.6, \"touch_px\": 0.5}}";
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
}

} // namespace

MP_TEST_MAIN("test_mp_player", 100)
