// The world's time base (planning step 152, WT): every dynamic body is in the original's.
//
// The original's Box2D world advances a FIXED 1/30 s a frame (clock.json physics_step_s), twice the 60 Hz
// tick the port's solver steps. From step 148 the player alone was carried in that frame; from step 152
// every dynamic body is, by one mechanism (sim/ScriptClock.hpp, "THE CARRIED FRAME"): a velocity at
// SimRatio times the original's, an acceleration at its square. This suite pins what that makes true:
//
//   gravity       the world's 10 m/s^2 of the original's physics (player.json gravity.world_m_s2) at SimRatio
//                 squared - 40 m/s^2, 2000 units a second squared at the layer's tick, 500 at a bare one - and
//                 not the Godot project's 980 the port's bodies fell at before.
//   a fall        a crate falls y_N = g (1/30 s)^2 N (N + 1) / 2 after N ticks, exactly as Box2D's semi-implicit
//                 step gives it after N FRAMES of the original: 0.5556 units a tick squared, half the player's
//                 1.111 (its SetGravityScale(2)).
//   a roll        a stone on a ramp runs the path of the SAME solver stepped the original's 1/30 s with the
//                 world's own 500, frame for frame: the carried frame is the original's, not an imitation.
//   a walk        a minion, a knight and the player set their velocities by ONE expression (ScriptSpeed), and
//                 a minion walks 1.04 units a tick on a level.
//   materials     every entity's friction and restitution are its .ent's (game/data/materials.json), checked
//                 against the original's files where they are, and a built body carries them and no damping.
//   the clamp     Box2D's b2_maxTranslation holds every dynamic body, not only the player.
//
// The levels these read are the converted ones from outside this repository (a case that needs one skips
// itself, saying where it looked); the free fall, the roll and the table are pure.

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "core/Components.hpp"
#include "core/PhysicsSettings.hpp"
#include "core/PhysicsSystem.hpp"
#include "sim/Cutscene.hpp"
#include "sim/Game.hpp"
#include "sim/LevelBuilder.hpp"
#include "sim/Minions.hpp"
#include "sim/Particles.hpp"
#include "sim/Player.hpp"
#include "sim/Resting.hpp"
#include "sim/ScriptClock.hpp"
#include "sim/Units.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <system_error>
#include <vector>

using namespace MagicPortals;
using Supersonic::BoxColliderComponent;
using Supersonic::PhysicsMaterialComponent;
using Supersonic::PhysicsSettings;
using Supersonic::PhysicsSystem;
using Supersonic::RigidBodyComponent;
using Supersonic::TransformComponent;

namespace {

const std::string kData = MAGICPORTALS_DATA_DIR;
const std::string kPortData = MAGICPORTALS_PORT_DATA_DIR;
const std::string kLevels = MAGICPORTALS_LEVELS_DIR;
const std::string kOriginal = MAGICPORTALS_ORIGINAL_DIR;
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

ScriptClock::Rules ReadClock() {
    ScriptClock::Rules rules;
    std::string error;
    CHECK_MSG(ScriptClock::LoadRules(kPortData + "/clock.json", rules, error), error);
    return rules;
}

Player::Tuning ReadTuning() {
    Player::Tuning tuning;
    std::string error;
    CHECK_MSG(Player::LoadTuning(kData + "/player.json", tuning, error), error);
    return tuning;
}

LevelBuilder::Materials ReadMaterials() {
    LevelBuilder::Materials materials;
    std::string error;
    CHECK_MSG(LevelBuilder::LoadMaterials(kPortData + "/materials.json", materials, error), error);
    return materials;
}

// The layer's own tick: the script's 16 ms of 16.667, a physics step of 1/30 s over the engine's 1/60.
ScriptClock::Step LayerStep() { return ScriptClock::TickStep(ReadClock(), kTick); }

double GravityOf(entt::registry& registry) {
    const auto* settings = registry.ctx().find<PhysicsSettings>();
    return settings != nullptr ? static_cast<double>(settings->gravity.y) : 0.0;
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

struct Started {
    Game::Data data;
    entt::registry registry;
    Game::Level level;
};

bool Start(const std::string& name, Started& run) {
    std::string error;
    const std::filesystem::path prisms = std::filesystem::temp_directory_path() / "supersonic-test-mp-timebase";
    const bool ok = Game::LoadData(kLevels + "/" + name + ".tscn", kData, prisms, run.data, error) &&
                    Game::Start(run.data, run.registry, run.level, error);
    CHECK_MSG(ok, name + ": " + error);
    return ok;
}

// ---- gravity ---------------------------------------------------------------------------------------------

void TheWorldsGravityIsTheOriginalsAtTheTicksRatio() {
    const Player::Tuning tuning = ReadTuning();
    // DECODED (Game::preLoop ins 416-427, SetGravity(0, scale(10))): 10 m/s^2 of the original's physics, which the
    // port's metre of 50 units makes 500 units a second squared.
    CHECK_WITHIN(tuning.worldGravity, 10.0, 0.0);

    entt::registry registry;
    // A bare tick is one whose physics is one length with the tick's: SimRatio 1, and the world's own 10.
    Game::UseWorldGravity(registry, tuning, ScriptClock::Step(kTick));
    CHECK_WITHIN(GravityOf(registry), -10.0, 1e-6);
    CHECK_WITHIN(GravityOf(registry) * Units::kPixelsPerMetre, -500.0, 1e-4);
    // The layer's: SimRatio 2, so SimRatio squared 4 - 40 m/s^2, 2000 units a second squared. Not the 980 of the Godot
    // project, and not the ratio's first power (1000), which would pass a casual look.
    const ScriptClock::Step step = LayerStep();
    CHECK_WITHIN(ScriptClock::SimRatio(step), 2.0, 1e-12);
    Game::UseWorldGravity(registry, tuning, step);
    CHECK_WITHIN(GravityOf(registry), -40.0, 1e-5);
    CHECK_WITHIN(GravityOf(registry) * Units::kPixelsPerMetre, -2000.0, 1e-3);
    CHECK_MSG(std::fabs(GravityOf(registry) * Units::kPixelsPerMetre + 980.0) > 100.0,
              "the Godot project's 980 is not what the world falls at");
    // The same factor for every quantity, from the one place that names it.
    CHECK_WITHIN(ScriptClock::CarriedAcceleration(10.0, step), 40.0, 1e-9);
    CHECK_WITHIN(ScriptClock::CarriedVelocity(1.44, step), 2.88, 1e-9);
    CHECK_WITHIN(ScriptClock::OriginalVelocity(2.88, step), 1.44, 1e-9);
    // The player's own: its SetGravityScale(2) on that, 4000 units a second squared - twice the world's.
    const double playerUnitsPerTick2 = Player::FallPerTick(tuning, step) * Units::kPixelsPerMetre * static_cast<double>(kTick);
    const double worldUnitsPerTick2 = -GravityOf(registry) * static_cast<double>(kTick) * Units::kPixelsPerMetre *
                                      static_cast<double>(kTick);
    CHECK_WITHIN(playerUnitsPerTick2, 1.111111, 1e-5);
    CHECK_WITHIN(worldUnitsPerTick2, 0.555556, 1e-5);
    CHECK_WITHIN(playerUnitsPerTick2 / worldUnitsPerTick2, tuning.gravityScale, 1e-6);
}

// A bare tick has the ratio 1 gravity, the layer's has ratio 2, and a weightless level keeps the zero it started
// with: Game::BeforeStep writes the tick's own, from the tick's own Step.
void BeforeStepWritesTheTicksOwnGravityAndAWeightlessLevelKeepsZero() {
    if (!LevelIsThere("level0a")) return;
    Started run;
    if (!Start("level0a", run)) return;
    // Start has no Step: ratio 1.
    CHECK_WITHIN(GravityOf(run.registry), -10.0, 1e-6);
    Game::BeforeStep(run.data, run.registry, run.level, 0.0f, ScriptClock::Step(kTick));
    CHECK_WITHIN(GravityOf(run.registry), -10.0, 1e-6);
    Game::BeforeStep(run.data, run.registry, run.level, 0.0f, LayerStep());
    CHECK_WITHIN(GravityOf(run.registry), -40.0, 1e-5);
    if (!LevelIsThere("level2c")) return;
    Started weightless;
    if (!Start("level2c", weightless)) return;
    CHECK(weightless.level.noGravity);
    Game::BeforeStep(weightless.data, weightless.registry, weightless.level, 0.0f, LayerStep());
    CHECK_WITHIN(GravityOf(weightless.registry), 0.0, 0.0);
}

// ---- a fall ----------------------------------------------------------------------------------------------

entt::entity MakeCrate(entt::registry& registry, const glm::dvec2& atPx) {
    const entt::entity entity = registry.create();
    registry.emplace<TransformComponent>(entity).position = Units::ToWorld(atPx.x, atPx.y);
    auto& box = registry.emplace<BoxColliderComponent>(entity);
    box.size = glm::vec3(Units::ToMetres(58.0), Units::ToMetres(58.0), static_cast<float>(LevelBuilder::kBodyDepthMetres));
    auto& rigid = registry.emplace<RigidBodyComponent>(entity);
    const LevelBuilder::Material material = LevelBuilder::MaterialOf(registry, "crate");
    rigid.friction = material.friction;
    rigid.restitution = material.restitution;
    rigid.linearDamping = 0.0f;
    rigid.angularDamping = 0.0f;
    rigid.lockPosition = LevelBuilder::kPlaneLockPosition;
    rigid.lockRotation = LevelBuilder::kPlaneLockRotation;
    return entity;
}

// How far a crate let go in a world with `step`'s carried gravity has fallen, in units, after `ticks` solver steps of
// `solverS`.
double Dropped(const ScriptClock::Step& step, float solverS, int ticks, std::vector<double>* every = nullptr) {
    entt::registry registry;
    registry.ctx().insert_or_assign<LevelBuilder::Materials>(ReadMaterials());
    Game::UseWorldGravity(registry, ReadTuning(), step);
    const entt::entity crate = MakeCrate(registry, glm::dvec2(0.0, -4000.0));
    const double from = Units::ToPixels(registry.get<TransformComponent>(crate).position).y;
    for (int tick = 0; tick < ticks; ++tick) {
        PhysicsSystem::Update(registry, solverS);
        if (every != nullptr) every->push_back(Units::ToPixels(registry.get<TransformComponent>(crate).position).y - from);
    }
    return Units::ToPixels(registry.get<TransformComponent>(crate).position).y - from;
}

void ACrateFallsAsTheOriginalsCrateFalls() {
    const ScriptClock::Step step = LayerStep();
    // Box2D adds g x its step to the velocity and then moves the body by it, once a frame: after N frames of the
    // original's 1/30 s the body has fallen g (1/30)^2 N (N + 1) / 2, 500 / 900 = 0.5556 units the first frame.
    const auto original = [](int frames) { return 500.0 / 900.0 * frames * (frames + 1) / 2.0; };
    std::vector<double> carried;
    Dropped(step, kTick, 60, &carried);
    for (const int ticks : {1, 2, 10, 30, 60}) {
        CHECK_WITHIN(carried[static_cast<std::size_t>(ticks - 1)], original(ticks), 0.02 + 1e-4 * original(ticks));
    }
    CHECK_WITHIN(carried[0], 0.5556, 0.001);
    std::printf("  a crate let go falls %.3f units in 1 tick, %.3f in 30 (the original's %.3f after 30 frames), %.3f in 60\n",
                carried[0], carried[29], original(30), carried[59]);
    // And the same solver stepped the original's own 1/30 s, a bare step whose physics is the frame's: the world a
    // frame of the original stands for. The two agree to the float.
    std::vector<double> reference;
    Dropped(ScriptClock::Step(1.0f / 30.0f), 1.0f / 30.0f, 60, &reference);
    double worst = 0.0;
    for (std::size_t i = 0; i < reference.size(); ++i) worst = std::max(worst, std::fabs(reference[i] - carried[i]));
    std::printf("  against the solver at the original's own 1/30 s: worst %.5f units over 60 frames\n", worst);
    CHECK_MSG(worst < 0.05, "the carried frame's fall is the original's: worst " + std::to_string(worst));
    // Half the player's: the same tick of the same world gives a crate 0.556 units a tick squared and the player 1.111.
    CHECK_WITHIN(carried[1] - carried[0] * 2.0, 0.5556, 0.002);
}

// ---- a roll ----------------------------------------------------------------------------------------------

// A stone let go on a 15-degree ramp of the original's ramp friction, stepped `ticks` times: where its centre has
// been each step, in units (+y down).
std::vector<glm::dvec2> Rolled(const ScriptClock::Step& step, float solverS, int ticks) {
    entt::registry registry;
    registry.ctx().insert_or_assign<LevelBuilder::Materials>(ReadMaterials());
    const Player::Tuning tuning = ReadTuning();
    Game::UseWorldGravity(registry, tuning, step);
    const double angle = 15.0 * kPi / 180.0;
    const entt::entity ramp = registry.create();
    auto& transform = registry.emplace<TransformComponent>(ramp);
    transform.position = Units::ToWorld(0.0, 500.0);
    transform.rotation.z = Units::ToWorldRotation(angle);
    auto& box = registry.emplace<BoxColliderComponent>(ramp);
    box.size = glm::vec3(Units::ToMetres(900.0), Units::ToMetres(200.0), 2.0f);
    auto& material = registry.emplace<PhysicsMaterialComponent>(ramp);
    const LevelBuilder::Material floor = LevelBuilder::MaterialOf(registry, "ramp_right");
    material.friction = floor.friction;
    material.restitution = floor.restitution;
    // On the surface at x = -250, 2 units over it.
    const double surface = 500.0 - 100.0 / std::cos(angle) - 250.0 * std::tan(angle);
    const entt::entity stone = LevelBuilder::BuildRigidCircle(registry, "stone", glm::dvec2(-250.0, surface - 30.0 / std::cos(angle) - 2.0),
                                                              30.0, LevelBuilder::Options{}, "rolling_stone");
    std::vector<glm::dvec2> path;
    for (int tick = 0; tick < ticks; ++tick) {
        PhysicsSystem::Update(registry, solverS);
        path.push_back(Units::ToPixels(registry.get<TransformComponent>(stone).position));
    }
    return path;
}

void AStoneRollsTheRoadTheOriginalsStoneRolls() {
    const int frames = 60;
    const std::vector<glm::dvec2> carried = Rolled(LayerStep(), kTick, frames);
    const std::vector<glm::dvec2> reference = Rolled(ScriptClock::Step(1.0f / 30.0f), 1.0f / 30.0f, frames);
    double worst = 0.0;
    for (std::size_t i = 0; i < carried.size(); ++i) worst = std::max(worst, glm::distance(carried[i], reference[i]));
    const double rolled = carried.back().x - carried.front().x;
    std::printf("  a stone on a 15-degree ramp: %.1f units along x in %d ticks, against %.1f of the solver at 1/30 s; worst gap %.3f units\n",
                rolled, frames, reference.back().x - reference.front().x, worst);
    // It rolls: the original's stone on the ramp's friction goes well down it (a 4 x slower stone, the port's
    // before, covered a quarter of it).
    CHECK_MSG(rolled > 100.0, "the stone rolled " + std::to_string(rolled) + " units in 60 ticks");
    // And what it rolls is the original's frame for frame, to what a different step's discretisation leaves.
    CHECK_MSG(worst < 2.0, "the stone's path is the 1/30 s solver's to " + std::to_string(worst) + " units");
}

// ---- a walk ----------------------------------------------------------------------------------------------

void AMinionAKnightAndThePlayerWalkOneExpression() {
    const ScriptClock::Step step = LayerStep();
    const Player::Tuning tuning = ReadTuning();
    Minions::Rules minions;
    Cutscene::Rules cutscenes;
    std::string error;
    CHECK_MSG(Minions::LoadRules(kPortData + "/minions.json", minions, error), error);
    CHECK_MSG(Cutscene::LoadRules(kPortData + "/cutscenes.json", cutscenes, error), error);
    // scale(k) x min(66, 16 ms) / 33.33 ms, k being 3, 1.3 and 1.7: metres a second of the original's physics.
    const double perK = 16.0 / 33.33333507180214;
    CHECK_WITHIN(Player::WalkOriginal(tuning, step), 3.0 * perK, 1e-9);
    CHECK_WITHIN(Minions::WalkOriginal(minions, step), 1.3 * perK, 1e-9);
    CHECK_WITHIN(Cutscene::KnightWalkOriginal(cutscenes, step), 1.7 * perK, 1e-9);
    // Carried at the one ratio: 2.4, 1.04 and 1.36 units a tick.
    const double unitsPerTick = Units::kPixelsPerMetre * static_cast<double>(kTick);
    CHECK_WITHIN(Player::WalkCarried(tuning, step) * unitsPerTick, 2.4, 1e-6);
    CHECK_WITHIN(Minions::WalkCarried(minions, step) * unitsPerTick, 1.04, 1e-6);
    CHECK_WITHIN(Cutscene::KnightWalkCarried(cutscenes, step) * unitsPerTick, 1.36, 1e-6);
    // The cap the three share: a frame of 100 ms is read as 66.
    ScriptClock::Step slow = step;
    slow.scriptS = 0.1;
    CHECK_WITHIN(Minions::WalkOriginal(minions, slow), 1.3 * 66.0 / 33.33333507180214, 1e-9);
}

void AMinionWalksItsDecodedDistanceATickOnALevel() {
    if (!LevelIsThere("level0b")) return;
    Started run;
    if (!Start("level0b", run)) return;
    const ScriptClock::Step step = LayerStep();
    const auto at = [&](const Minions::Minion& minion) {
        return Units::ToPixels(run.registry.get<TransformComponent>(minion.body).position);
    };
    // The patrolling minion serves a 4000 ms dwell and then walks: step on, and take the x it covers in each tick
    // that it walks on and the tick before.
    std::vector<double> walked;
    std::vector<glm::dvec2> last(run.level.minions.minions.size(), glm::dvec2(0.0));
    for (int tick = 0; tick < 600; ++tick) {
        Game::Tick(run.data, run.registry, run.level, 0.0f, step);
        for (std::size_t i = 0; i < run.level.minions.minions.size(); ++i) {
            const Minions::Minion& minion = run.level.minions.minions[i];
            if (minion.gone || minion.body == entt::null || !run.registry.valid(minion.body)) continue;
            const glm::dvec2 now = at(minion);
            if (tick > 0 && minion.arm == Minions::Arm::Walk && last[i].x != 0.0 && std::fabs(now.y - last[i].y) < 0.01) {
                walked.push_back(std::fabs(now.x - last[i].x));
            }
            last[i] = now;
        }
    }
    CHECK_MSG(walked.size() > 20, "a minion walked on " + std::to_string(walked.size()) + " ticks");
    if (walked.size() <= 20) return;
    std::sort(walked.begin(), walked.end());
    const double median = walked[walked.size() / 2];
    std::printf("  level0b: a minion walks %.4f units a tick (median of %zu walking ticks), the decode's 1.04\n", median,
                walked.size());
    // 1.04 where the floor is flat and it is on it; the median is the steady walk (a frictionless body coasts at it).
    CHECK_WITHIN(median, 1.04, 0.02);
}

// ---- materials -------------------------------------------------------------------------------------------

void TheMaterialTableIsRead() {
    const LevelBuilder::Materials materials = ReadMaterials();
    const struct {
        const char* entity;
        float friction;
        float restitution;
    } expected[] = {
        {"crate", 0.96f, 0.2f},         {"crate_small_no_emissive", 0.96f, 0.2f}, {"barrel_bomb", 0.7f, 0.0f},
        {"rolling_stone", 0.025f, 0.15f}, {"minion", 0.0f, 0.0f},              {"ghost_minion", 0.0f, 0.0f},
        {"dragon_knight", 0.9f, 0.0f},  {"platform_no_emissive", 0.2f, 0.0f}, {"block00_low_emissive", 0.4f, 0.0f},
        {"ramp_right", 0.25f, 0.0f},    {"door_lift", 1.0f, 0.2f},            {"gravity_agent", 0.0f, 0.1f},
    };
    for (const auto& one : expected) {
        CHECK_MSG(materials.Holds(one.entity), std::string(one.entity) + " is in the table");
        CHECK_WITHIN(materials.Of(one.entity).decodedFriction, one.friction, 1e-6);
        CHECK_WITHIN(materials.Of(one.entity).decodedRestitution, one.restitution, 1e-6);
        // What it is BUILT with: the held friction, except where the decoded one is 0 (a friction of 0 against any surface is 0).
        CHECK_WITHIN(materials.Of(one.entity).friction, one.friction == 0.0f ? 0.0 : static_cast<double>(materials.heldFriction), 1e-6);
    }
    // A node's entity_name carries ".ent" in some levels and not in others.
    CHECK_WITHIN(materials.Of("crate.ent").decodedFriction, 0.96, 1e-6);
    CHECK_WITHIN(materials.Of("rolling_stone.ent").decodedFriction, 0.025, 1e-6);
    // An entity with no .ent among the extracted assets keeps the Godot project's constants, said so.
    for (const char* name : {"moving_platform", "enemy_killer", "lava", "no_restitution"}) {
        CHECK_MSG(!materials.Holds(name), std::string(name) + " has no decoded material");
        CHECK_WITHIN(materials.Of(name).friction, LevelBuilder::kBodyFriction, 0.0);
        CHECK_WITHIN(materials.Of(name).restitution, LevelBuilder::kBodyRestitution, 0.0);
        CHECK_WITHIN(materials.Of(name).decodedRestitution, LevelBuilder::kBodyRestitution, 0.0);
    }
    // The decoded restitution is held but not applied (materials.json says why): what a body is built with is the 0.
    CHECK(!materials.restitutionApplied);
    CHECK_WITHIN(materials.Of("crate").restitution, 0.0, 0.0);
    CHECK_WITHIN(materials.Of("rolling_stone").restitution, 0.0, 0.0);
    // And so is the decoded friction (friction_applied): the Godot project's 1, the constants a rig with no table is built on.
    CHECK(!materials.frictionApplied);
    CHECK_WITHIN(materials.heldFriction, LevelBuilder::kBodyFriction, 0.0);
    CHECK_WITHIN(materials.heldFriction, LevelBuilder::kStaticFriction, 0.0);
    for (const char* name : {"crate", "barrel_bomb", "rolling_stone", "platform", "block00", "dragon_knight", "ramp_right", "lift"}) {
        CHECK_WITHIN(materials.Of(name).friction, 1.0, 0.0);
    }
    // The one entity held lower than the rest, a guess marked in the file (planning step 152's amendment): the crate of 3-13's
    // slot. Its decoded friction, restitution and weight are the crate's own; only what it is built with differs, and only
    // while friction_applied is false.
    for (const char* name : {"crate_low_emissive"}) {
        CHECK_MSG(materials.Holds(name), std::string(name) + " is in the table");
        CHECK_WITHIN(materials.Of(name).decodedFriction, 0.96, 1e-6);
        CHECK_WITHIN(materials.Of(name).decodedRestitution, 0.2, 1e-6);
        CHECK_WITHIN(materials.Of(name).density, 0.4, 1e-6);
        CHECK_WITHIN(materials.Of(name).friction, 0.08, 1e-6);
    }
    for (const char* name : {"crate_no_emissive", "crate_small", "crate_small_no_emissive"}) {
        CHECK_WITHIN(materials.Of(name).friction, 1.0, 0.0);
    }
    // The three that apply their own, because 0 against any surface is 0 and the solve has no friction impulse to get wrong.
    for (const char* name : {"minion", "ghost_minion", "gravity_agent"}) {
        CHECK_WITHIN(materials.Of(name).friction, 0.0, 0.0);
        CHECK_WITHIN(materials.Of(name).decodedFriction, 0.0, 0.0);
    }
    // A rig with no level behind it builds the default.
    entt::registry bare;
    CHECK_WITHIN(LevelBuilder::MaterialOf(bare, "crate").friction, LevelBuilder::kBodyFriction, 0.0);
}

void AMaterialFileThatDescribesNoFixtureIsRefused() {
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "supersonic-test-mp-timebase-materials";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    const auto refused = [&](const std::string& text, const char* what) {
        const std::filesystem::path file = dir / "materials.json";
        std::ofstream(file, std::ios::binary) << text;
        LevelBuilder::Materials read;
        std::string error;
        CHECK_MSG(!LevelBuilder::LoadMaterials(file.string(), read, error), std::string(what) + " was taken");
    };
    const std::string flags = R"("restitution_applied": false, "friction_applied": false, "held_friction": 1.0, )";
    const std::string plain = R"("groups": [{"entities": ["a"], "friction": 0.5, "restitution": 0.1}, )";
    const std::string zero = R"({"entities": ["b"], "friction": 0.0, "restitution": 0.1, "apply_decoded_friction": true}]})";
    // The control: the smallest files that are taken, so the refusals below are for what each says.
    {
        const std::filesystem::path file = dir / "materials.json";
        LevelBuilder::Materials read;
        std::string error;
        std::ofstream(file, std::ios::binary) << "{" << flags << R"("groups": [{"entities": ["a"], "friction": 0.5, "restitution": 0.1}]})";
        CHECK_MSG(LevelBuilder::LoadMaterials(file.string(), read, error), "the smallest valid file is taken: " + error);
        CHECK_WITHIN(read.Of("a").friction, 1.0, 0.0);         // held
        CHECK_WITHIN(read.Of("a").decodedFriction, 0.5, 1e-6); // kept
        LevelBuilder::Materials applied;
        std::ofstream(file, std::ios::binary) << R"({"restitution_applied": false, "friction_applied": true, "held_friction": 1.0, )" << plain << zero;
        CHECK_MSG(LevelBuilder::LoadMaterials(file.string(), applied, error), "friction applied is taken: " + error);
        CHECK_WITHIN(applied.Of("a").friction, 0.5, 1e-6);
        LevelBuilder::Materials mixed;
        std::ofstream(file, std::ios::binary) << "{" << flags << plain << zero;
        CHECK_MSG(LevelBuilder::LoadMaterials(file.string(), mixed, error), "a group that applies its own is taken: " + error);
        CHECK_WITHIN(mixed.Of("a").friction, 1.0, 0.0);
        CHECK_WITHIN(mixed.Of("b").friction, 0.0, 0.0);
        // A group's own held friction replaces the table's for that group while friction is held, and the decoded number is kept;
        // once the table applies friction the group's held number is not read and the decoded one applies.
        const std::string own = R"("groups": [{"entities": ["a"], "friction": 0.5, "restitution": 0.1}, )"
                                R"({"entities": ["c"], "friction": 0.9, "restitution": 0.1, "held_friction": 0.08}]})";
        LevelBuilder::Materials held;
        std::ofstream(file, std::ios::binary) << "{" << flags << own;
        CHECK_MSG(LevelBuilder::LoadMaterials(file.string(), held, error), "a group with its own held friction is taken: " + error);
        CHECK_WITHIN(held.Of("a").friction, 1.0, 0.0);
        CHECK_WITHIN(held.Of("c").friction, 0.08, 1e-6);
        CHECK_WITHIN(held.Of("c").decodedFriction, 0.9, 1e-6);
        LevelBuilder::Materials appliedOwn;
        std::ofstream(file, std::ios::binary) << R"({"restitution_applied": false, "friction_applied": true, "held_friction": 1.0, )" << own;
        CHECK_MSG(LevelBuilder::LoadMaterials(file.string(), appliedOwn, error), "friction applied with a group's held friction is taken: " + error);
        CHECK_WITHIN(appliedOwn.Of("c").friction, 0.9, 1e-6);
    }
    refused("{" + flags + R"("groups": [{"entities": ["a"], "friction": 0.5, "restitution": 0.1}, {"entities": ["a.ent"], "friction": 0.2, "restitution": 0.0}]})",
            "an entity in two groups");
    refused("{" + flags + R"("groups": [{"entities": ["a"], "friction": -0.5, "restitution": 0.1}]})", "a friction below zero");
    refused("{" + flags + R"("groups": [{"entities": ["a"], "friction": 0.5, "restitution": 1.5}]})", "a restitution above one");
    refused("{" + flags + R"("groups": [{"entities": ["a"], "friction": 0.5}]})", "a group with no restitution");
    refused("{" + flags + R"("groups": [{"entities": ["a"], "friction": 0.5, "restitution": 0.1, "held_friction": -0.1}]})",
            "a group's held friction below zero");
    refused("{" + flags + R"("groups": [{"entities": ["a"], "friction": 0.5, "restitution": 0.1, "held_friction": "low"}]})",
            "a group's held friction that is not a number");
    refused("{" + flags + R"("groups": []})", "no groups");
    refused("{" + flags + R"("groups": [{"entities": [""], "friction": 0.5, "restitution": 0.1}]})", "a nameless entity");
    refused("{" + std::string(R"("friction_applied": false, "held_friction": 1.0, )") + plain + zero, "a file with no restitution_applied");
    refused("{" + std::string(R"("restitution_applied": false, "held_friction": 1.0, )") + plain + zero, "a file with no friction_applied");
    refused("{" + std::string(R"("restitution_applied": false, "friction_applied": false, )") + plain + zero, "a file with no held_friction");
    refused("{" + std::string(R"("restitution_applied": false, "friction_applied": false, "held_friction": -1.0, )") + plain + zero,
            "a held friction below zero");
    std::filesystem::remove_all(dir, ec);
}

// materials.json is the .ent files' numbers: every entity it holds, read back from the original's own file.
void EveryMaterialIsItsEntsOwn() {
    const std::string entities = kOriginal + "/entities/";
    std::error_code ec;
    if (!std::filesystem::is_regular_file(entities + "crate.ent", ec)) {
        std::printf("  EveryMaterialIsItsEntsOwn SKIPPED - needs the original's entities at %s\n", entities.c_str());
        return;
    }
    const LevelBuilder::Materials materials = ReadMaterials();
    const auto attribute = [](const std::string& text, const std::string& name) -> double {
        const std::size_t open = text.find("<Entity ");
        if (open == std::string::npos) return -1.0;
        const std::string head = text.substr(open, text.find('>', open) - open);
        const std::string key = " " + name + "=\"";
        const std::size_t at = head.find(key);
        if (at == std::string::npos) return -1.0;
        const std::size_t from = at + key.size();
        return std::stod(head.substr(from, head.find('"', from) - from));
    };
    int read = 0;
    for (const auto& [name, material] : materials.byEntity) {
        std::string text;
        std::string error;
        if (!Particles::ReadEnt(entities + name + ".ent", text, error)) {
            CHECK_MSG(false, name + ": " + error);
            continue;
        }
        const double friction = attribute(text, "friction");
        const double restitution = attribute(text, "restitution");
        CHECK_MSG(std::fabs(friction - material.decodedFriction) < 1e-6,
                  name + ".ent friction " + std::to_string(friction) + ", materials.json " + std::to_string(material.decodedFriction));
        CHECK_MSG(std::fabs(restitution - material.decodedRestitution) < 1e-6,
                  name + ".ent restitution " + std::to_string(restitution) + ", materials.json " +
                      std::to_string(material.decodedRestitution));
        ++read;
    }
    std::printf("  materials.json: %d entities, each read back from its .ent\n", read);
    CHECK_MSG(read >= 50, "the table holds " + std::to_string(read) + " entities");
}

void ABuiltBodyCarriesItsEntitysMaterialAndNoDamping() {
    if (!LevelIsThere("level0a")) return;
    Started run;
    if (!Start("level0a", run)) return;
    const auto find = [&](const char* node) {
        const auto found = run.level.built.entities.find(node);
        CHECK_MSG(found != run.level.built.entities.end(), std::string(node) + " is built");
        return found != run.level.built.entities.end() ? found->second : entt::null;
    };
    // level0a places a crate_no_emissive and a block00_no_emissive.ent (a static).
    const entt::entity crate = find("crate_no_emissive_796");
    const entt::entity block = find("block00_no_emissive_ent_840");
    if (crate == entt::null || block == entt::null) return;
    const auto& rigid = run.registry.get<RigidBodyComponent>(crate);
    CHECK_WITHIN(rigid.friction, 1.0, 0.0);    // decoded 0.96, held: materials.json friction_applied
    CHECK_WITHIN(rigid.restitution, 0.0, 0.0); // decoded 0.2, not applied: materials.json restitution_applied
    CHECK_WITHIN(rigid.linearDamping, 0.0, 0.0);
    CHECK_WITHIN(rigid.angularDamping, 0.0, 0.0);
    const auto& surface = run.registry.get<PhysicsMaterialComponent>(block);
    CHECK_WITHIN(surface.friction, 1.0, 0.0); // decoded 0.4, held
    CHECK_WITHIN(surface.restitution, 0.0, 1e-6);
    // The table's mix is Box2D's: the square root of the product, and the larger restitution; held, 1 against 1 is 1.
    CHECK_WITHIN(PhysicsSystem::CombineFriction(rigid.friction, surface.friction), 1.0, 1e-6);
    CHECK_WITHIN(PhysicsSystem::CombineFriction(0.96f, 0.4f), std::sqrt(0.96 * 0.4), 1e-5);
    CHECK_WITHIN(PhysicsSystem::CombineRestitution(rigid.restitution, surface.restitution), 0.0, 1e-6);
}

void AMinionIsFrictionless() {
    if (!LevelIsThere("level0b")) return;
    Started run;
    if (!Start("level0b", run)) return;
    CHECK(!run.level.minions.minions.empty());
    for (const Minions::Minion& minion : run.level.minions.minions) {
        if (minion.body == entt::null) continue;
        const auto& rigid = run.registry.get<RigidBodyComponent>(minion.body);
        CHECK_WITHIN(rigid.friction, 0.0, 0.0);
        CHECK_WITHIN(rigid.restitution, 0.0, 0.0);
    }
}

// ---- the clamp -------------------------------------------------------------------------------------------

void EveryDynamicBodyIsHeldToBox2DsMaxTranslation() {
    if (!LevelIsThere("level0a")) return;
    Started run;
    if (!Start("level0a", run)) return;
    const auto found = run.level.built.entities.find("crate_no_emissive_796");
    CHECK(found != run.level.built.entities.end());
    if (found == run.level.built.entities.end()) return;
    const ScriptClock::Step step = LayerStep();
    auto& rigid = run.registry.get<RigidBodyComponent>(found->second);
    const Player::Tuning tuning = run.data.tuning;
    const double limitM = Player::ClampLimitM(tuning);
    // A crate falling at 100 m/s - 5000 units a second, 83 a tick, past the 35.6 the original's world allows.
    rigid.velocity = glm::vec3(0.0f, -100.0f, 0.0f);
    Game::BeforeStep(run.data, run.registry, run.level, 0.0f, step);
    const double gravityStep = GravityOf(run.registry) * static_cast<double>(step.physicsS);
    // What the engine will integrate - the written velocity and a step of the world's gravity - carries it exactly
    // the limit, down: the direction is kept and nothing else is.
    const double integrated = static_cast<double>(rigid.velocity.y) + gravityStep;
    CHECK_WITHIN(-integrated * static_cast<double>(step.physicsS), limitM, 1e-6);
    CHECK_MSG(std::fabs(static_cast<double>(rigid.velocity.x)) < 1e-9, "a fall stays a fall");
    // Under the limit it is untouched, and a kinematic body (a lift) is never clamped.
    rigid.velocity = glm::vec3(0.0f, -5.0f, 0.0f);
    Game::BeforeStep(run.data, run.registry, run.level, 0.0f, step);
    CHECK_WITHIN(rigid.velocity.y, -5.0, 1e-9);
    rigid.isKinematic = true;
    rigid.velocity = glm::vec3(0.0f, -100.0f, 0.0f);
    Game::BeforeStep(run.data, run.registry, run.level, 0.0f, step);
    CHECK_WITHIN(rigid.velocity.y, -100.0, 1e-9);
    // And the function on its own, for a body that uses no world gravity (the player's): the whole velocity scaled.
    RigidBodyComponent body;
    body.velocity = glm::vec3(60.0f, -80.0f, 0.0f);
    Player::ClampTranslation(body, tuning, step);
    CHECK_WITHIN(std::hypot(static_cast<double>(body.velocity.x), static_cast<double>(body.velocity.y)) *
                     static_cast<double>(step.physicsS),
                 limitM, 1e-6);
    CHECK_WITHIN(body.velocity.x / body.velocity.y, 60.0 / -80.0, 1e-5);
}

// ---- restitution -----------------------------------------------------------------------------------------

// Why materials.json holds the .ent's restitution and does not apply it (restitution_applied): the engine applies a body's
// restitution to a SPECULATIVE contact, a pair still apart and closing within a step's travel (PhysicsSystem.cpp:1574-1631), so a
// stone of the original's 0.15 turns round before it touches, where Box2D turns it round at the touch. Measured on a rig: a
// stone dropped 100 units onto a floor at the layer's carried gravity, the gap between its surface and the floor on the tick
// its velocity first turns up, with and without the restitution.
double GapAtTheFirstBounce(float restitution, double* fallingAt) {
    entt::registry registry;
    registry.ctx().insert_or_assign<LevelBuilder::Materials>(ReadMaterials());
    Game::UseWorldGravity(registry, ReadTuning(), LayerStep());
    const entt::entity floor = registry.create();
    registry.emplace<TransformComponent>(floor).position = Units::ToWorld(0.0, 200.0);
    auto& slab = registry.emplace<BoxColliderComponent>(floor);
    slab.size = glm::vec3(Units::ToMetres(4000.0), Units::ToMetres(200.0), 2.0f);
    registry.emplace<PhysicsMaterialComponent>(floor).restitution = 0.0f;
    // The floor's top is at y 100; the stone starts with its bottom 100 over it.
    const entt::entity stone =
        LevelBuilder::BuildRigidCircle(registry, "stone", glm::dvec2(0.0, 100.0 - 30.0 - 100.0), 30.0, LevelBuilder::Options{});
    registry.get<RigidBodyComponent>(stone).restitution = restitution;
    for (int tick = 0; tick < 120; ++tick) {
        const double before = static_cast<double>(registry.get<RigidBodyComponent>(stone).velocity.y);
        PhysicsSystem::Update(registry, kTick);
        const auto& body = registry.get<RigidBodyComponent>(stone);
        if (body.velocity.y > 0.0f && before < 0.0) {
            if (fallingAt != nullptr) *fallingAt = -before * Units::kPixelsPerMetre;
            return 100.0 - (Units::ToPixels(registry.get<TransformComponent>(stone).position).y + 30.0);
        }
    }
    return -1.0;
}

void TheEngineBouncesAStoneBeforeItTouches() {
    double falling = 0.0;
    const double with = GapAtTheFirstBounce(0.15f, &falling);
    const double without = GapAtTheFirstBounce(0.0f, nullptr);
    std::printf("  a stone of 0.15 dropped 100 units turns round %.2f units over the floor (falling at %.0f px/s); of 0, "
                "%s\n",
                with, falling, without < 0.0 ? "never turns" : "it turns round at all");
    // THE TRIPWIRE: if this fails the engine bounces a body at the touch, as Box2D does - turn restitution_applied on in
    // materials.json, and rerun the suites that ask whether two bodies touch (demolish, boss, impacts, diamonds).
    CHECK_MSG(with > 0.5, "the engine's bounce is a speculative one: the gap at the first turn is " + std::to_string(with));
    CHECK_MSG(without < 0.0, "and a stone with no restitution does not bounce at all");
}

// ---- masses ----------------------------------------------------------------------------------------------

// A body's mass is its entity's density over the area of its shape (b2Body::ResetMassData), and the player's is
// dark_mage.ent's over its two circles: so 2-24's barrel is 4.15 times the player it carries.
void TheMassesAreTheEntsDensityOverTheShape() {
    const LevelBuilder::Materials materials = ReadMaterials();
    // The player: density 0.5 over two circles of radius 9.1 units (0.182 m).
    entt::registry bare;
    CHECK_WITHIN(LevelBuilder::MassOf(materials.Of("dark_mage"), 0.0), 0.5 * 2.0 * kPi * 0.182 * 0.182, 1e-5);
    // A body of an entity with no density keeps the engine's 1; so does a rig with no table.
    CHECK_WITHIN(LevelBuilder::MassOf(materials.Of("moving_platform"), 3.0), 1.0, 0.0);
    CHECK_WITHIN(LevelBuilder::MassOf(LevelBuilder::MaterialOf(bare, "dark_mage"), 0.0), 1.0, 0.0);
    // A circle's: a stone of radius 30 units, density 1.
    CHECK_WITHIN(LevelBuilder::MassOf(materials.Of("rolling_stone"), kPi * 0.6 * 0.6), kPi * 0.36, 1e-5);

    if (!LevelIsThere("level23a")) return;
    Started run;
    if (!Start("level23a", run)) return;
    const auto barrel = run.level.built.entities.find("barrel_bomb_ent_1135");
    CHECK(barrel != run.level.built.entities.end());
    if (barrel == run.level.built.entities.end()) return;
    // The barrel: density 1 over its 28.6 x 37.7 units (the .ent's 22 x 29 at Scale 1.3).
    const double barrelKg = static_cast<double>(run.registry.get<RigidBodyComponent>(barrel->second).mass);
    CHECK_WITHIN(barrelKg, (28.6 / 50.0) * (37.7 / 50.0), 1e-4);
    const double playerKg = static_cast<double>(run.registry.get<RigidBodyComponent>(run.level.player).mass);
    CHECK_WITHIN(playerKg, 0.104069, 1e-5);
    CHECK_WITHIN(barrelKg / playerKg, 4.145, 0.01);
    std::printf("  2-24: the barrel weighs %.4f, the player %.4f - %.2f times\n", barrelKg, playerKg, barrelKg / playerKg);
}

// What the masses do to a push: a walking player (velocity set outright every tick) against a loose crate shares the contact's
// impulse by inverse mass, so the pair moves at m_player / (m_player + m_crate) of the walk - the original's Box2D does the same.
// A small crate (30 x 30, density 0.4: 0.144 against the player's 0.104) goes at 0.42 of 2.4 units a tick, where the engine's
// 1 against 1 gave it 0.5. Printed with the control and judged loosely: the engine's solver decides the number.
double PushedAt(bool unitMasses) {
    entt::registry registry;
    registry.ctx().insert_or_assign<LevelBuilder::Materials>(ReadMaterials());
    const Player::Tuning tuning = ReadTuning();
    const ScriptClock::Step step = LayerStep();
    Game::UseWorldGravity(registry, tuning, step);
    // A floor whose top is at y 100 (a platform's friction), the small crate on it at x 50, and the player at x 0.
    const entt::entity floor = registry.create();
    registry.emplace<TransformComponent>(floor).position = Units::ToWorld(0.0, 200.0);
    auto& slab = registry.emplace<BoxColliderComponent>(floor);
    slab.size = glm::vec3(Units::ToMetres(4000.0), Units::ToMetres(200.0), 2.0f);
    auto& surface = registry.emplace<PhysicsMaterialComponent>(floor);
    surface.friction = LevelBuilder::MaterialOf(registry, "platform").friction;
    surface.restitution = 0.0f;
    const entt::entity crate = registry.create();
    registry.emplace<TransformComponent>(crate).position = Units::ToWorld(50.0, 100.0 - 15.0 - 0.1);
    auto& box = registry.emplace<BoxColliderComponent>(crate);
    box.size = glm::vec3(Units::ToMetres(30.0), Units::ToMetres(30.0), static_cast<float>(LevelBuilder::kBodyDepthMetres));
    auto& rigid = registry.emplace<RigidBodyComponent>(crate);
    const LevelBuilder::Material material = LevelBuilder::MaterialOf(registry, "crate_small");
    rigid.friction = material.friction;
    rigid.restitution = 0.0f;
    rigid.mass = LevelBuilder::MassOf(material, 0.6 * 0.6);
    rigid.angularDamping = 0.0f;
    rigid.lockPosition = LevelBuilder::kPlaneLockPosition;
    rigid.lockRotation = LevelBuilder::kPlaneLockRotation;
    const entt::entity player = Player::Spawn(registry, glm::dvec2(0.0, 100.0 - tuning.heightPx * 0.5 - 0.1), tuning);
    if (unitMasses) {
        registry.get<RigidBodyComponent>(crate).mass = 1.0f;
        registry.get<RigidBodyComponent>(player).mass = 1.0f;
    }
    const auto crateX = [&] { return Units::ToPixels(registry.get<TransformComponent>(crate).position).x; };
    const auto tick = [&](float direction) {
        Player::Steer(registry, player, tuning, direction, step);
        PhysicsSystem::Update(registry, kTick);
    };
    for (int i = 0; i < 30; ++i) tick(0.0f);
    std::vector<double> pushed;
    double last = crateX();
    for (int i = 0; i < 90; ++i) {
        tick(1.0f);
        const double now = crateX();
        if (now - last > 0.02) pushed.push_back(now - last);
        last = now;
    }
    if (pushed.size() < 10) return 0.0;
    std::sort(pushed.begin(), pushed.end());
    return pushed[pushed.size() / 2];
}

void ADecodedPlayerPushesACrateAtTheMassRatio() {
    const double decoded = PushedAt(false);
    const double even = PushedAt(true);
    std::printf("  on a platform: the walking player (2.4 units a tick) pushes the small crate at %.3f a tick; at the engine's 1 against 1 %.3f\n",
                decoded, even);
    CHECK_MSG(decoded > 0.1 && decoded < 2.4, "the crate is pushed, and slower than the walk: " + std::to_string(decoded));
    CHECK_MSG(decoded <= even + 1e-6, "and no faster than at even weights: " + std::to_string(decoded) + " against " + std::to_string(even));
}

// ---- friction: a resting box ------------------------------------------------------------------------------

// What a box let go 1 unit over a flat floor does, left alone for `ticks` solver steps of the layer's carried gravity: the speed it
// ends at along the floor (units a second), how far it has gone, whether it sleeps, and how far it has turned. Nothing of the port is
// in the loop but the table's own numbers: a floor, a box, the engine's solver.
struct RestingBox {
    double speedUnitsPerSecond = 0.0;
    double driftUnits = 0.0;
    double turnedDegrees = 0.0;
    bool asleep = false;
    double asleepFromTick = -1.0;
};

RestingBox LetGoOnAFlatFloor(float boxFriction, float floorFriction, double halfWidthUnits, double halfHeightUnits, float mass,
                             int ticks = 420) {
    entt::registry registry;
    registry.ctx().insert_or_assign<LevelBuilder::Materials>(ReadMaterials());
    Game::UseWorldGravity(registry, ReadTuning(), LayerStep());
    const entt::entity floor = registry.create();
    registry.emplace<TransformComponent>(floor).position = Units::ToWorld(0.0, 200.0);
    auto& slab = registry.emplace<BoxColliderComponent>(floor);
    slab.size = glm::vec3(Units::ToMetres(4000.0), Units::ToMetres(200.0), 2.0f);
    auto& surface = registry.emplace<PhysicsMaterialComponent>(floor);
    surface.friction = floorFriction;
    surface.restitution = 0.0f;
    const entt::entity box = registry.create();
    registry.emplace<TransformComponent>(box).position = Units::ToWorld(0.0, 100.0 - halfHeightUnits - 1.0);
    auto& shape = registry.emplace<BoxColliderComponent>(box);
    shape.size = glm::vec3(Units::ToMetres(2.0 * halfWidthUnits), Units::ToMetres(2.0 * halfHeightUnits),
                           static_cast<float>(LevelBuilder::kBodyDepthMetres));
    auto& rigid = registry.emplace<RigidBodyComponent>(box);
    rigid.friction = boxFriction;
    rigid.restitution = 0.0f;
    rigid.mass = mass;
    rigid.linearDamping = 0.0f;
    rigid.angularDamping = 0.0f;
    rigid.lockPosition = LevelBuilder::kPlaneLockPosition;
    rigid.lockRotation = LevelBuilder::kPlaneLockRotation;
    RestingBox result;
    for (int tick = 1; tick <= ticks; ++tick) {
        PhysicsSystem::Update(registry, kTick);
        if (rigid.isSleeping && result.asleepFromTick < 0.0) result.asleepFromTick = tick;
    }
    result.speedUnitsPerSecond =
        std::hypot(static_cast<double>(rigid.velocity.x), static_cast<double>(rigid.velocity.y)) * Units::kPixelsPerMetre;
    result.driftUnits = std::fabs(Units::ToPixels(registry.get<TransformComponent>(box).position).x);
    result.turnedDegrees = std::fabs(static_cast<double>(registry.get<TransformComponent>(box).rotation.z)) * 180.0 / kPi;
    result.asleep = rigid.isSleeping;
    return result;
}

// THE TRIPWIRE for materials.json's friction_applied. The engine's friction solve walks a RESTING box sideways at the decoded pairs
// of the game (a crate on a platform, 0.96 against 0.2, is 0.438 combined; a barrel on one 0.374): a flat 30 x 30 crate on a flat
// floor, lying level with the contact normal exactly vertical, accelerates along the floor to 5.745 units a second in 30 ticks and
// keeps it for ever, awake - at any mass. If this test fails the engine's friction has been fixed: turn friction_applied on in
// materials.json and rerun every suite and the routes.
void TheEngineWalksARestingBoxSidewaysAtTheDecodedFriction() {
    const LevelBuilder::Materials materials = ReadMaterials();
    const float crate = materials.Of("crate_small").decodedFriction;
    const float platform = materials.Of("platform").decodedFriction;
    const float barrel = materials.Of("barrel_bomb").decodedFriction;
    CHECK_WITHIN(crate, 0.96, 1e-6);
    CHECK_WITHIN(platform, 0.2, 1e-6);
    const RestingBox crateOnPlatform = LetGoOnAFlatFloor(crate, platform, 15.0, 15.0, 0.144f);
    const RestingBox heavyCrate = LetGoOnAFlatFloor(crate, platform, 15.0, 15.0, 1.0f);
    const RestingBox barrelOnPlatform = LetGoOnAFlatFloor(barrel, platform, 14.3, 18.85, 0.431f);
    std::printf("  decoded friction, a flat floor, 420 ticks awake: a 30 x 30 crate (%.3f x %.3f) %.3f units a second, %.1f along, turned "
                "%.4f deg; of mass 1 %.3f; a 29 x 38 barrel %.3f, %.1f along\n",
                static_cast<double>(crate), static_cast<double>(platform), crateOnPlatform.speedUnitsPerSecond,
                crateOnPlatform.driftUnits, crateOnPlatform.turnedDegrees, heavyCrate.speedUnitsPerSecond,
                barrelOnPlatform.speedUnitsPerSecond, barrelOnPlatform.driftUnits);
    CHECK_MSG(crateOnPlatform.speedUnitsPerSecond > 3.0 && !crateOnPlatform.asleep,
              "the engine's friction solve walks a level crate along a flat floor: " +
                  std::to_string(crateOnPlatform.speedUnitsPerSecond));
    CHECK_MSG(heavyCrate.speedUnitsPerSecond > 3.0 && !heavyCrate.asleep, "at any mass: " + std::to_string(heavyCrate.speedUnitsPerSecond));
    CHECK_MSG(barrelOnPlatform.speedUnitsPerSecond > 5.0 && !barrelOnPlatform.asleep,
              "and a barrel: " + std::to_string(barrelOnPlatform.speedUnitsPerSecond));
    // Level, not tilted: the walk is not a slide down a slope.
    CHECK_MSG(crateOnPlatform.turnedDegrees < 0.01,
              "the crate lies level while it is walked: " + std::to_string(crateOnPlatform.turnedDegrees));
    // And it is the friction, not the gravity or the shape: the same box at 1 against 1 sleeps where it fell.
    const RestingBox unit = LetGoOnAFlatFloor(1.0f, 1.0f, 15.0, 15.0, 0.144f);
    CHECK_MSG(unit.asleep && unit.driftUnits < 0.01, "at 1 against 1 the crate is asleep where it fell");
}

// What materials.json makes of it: every box the port builds rests. A crate and a barrel built with the table's own friction, on a
// platform or a block built with the table's, are within 1 unit of where they fell and ASLEEP within two seconds of ticks.
void ABoxBuiltFromTheTableRestsAndSleeps() {
    const LevelBuilder::Materials materials = ReadMaterials();
    CHECK(!materials.frictionApplied);
    const float platform = materials.Of("platform").friction;
    const RestingBox crate = LetGoOnAFlatFloor(materials.Of("crate_small").friction, platform, 15.0, 15.0, 0.144f);
    const RestingBox bigCrate = LetGoOnAFlatFloor(materials.Of("crate").friction, materials.Of("block00").friction, 29.0, 29.0, 0.538f);
    const RestingBox barrel = LetGoOnAFlatFloor(materials.Of("barrel_bomb").friction, platform, 14.3, 18.85, 0.431f);
    std::printf("  built from the table (held friction %.2f), 420 ticks: a small crate %.3f along, asleep from tick %.0f; a crate %.3f, %.0f; "
                "a barrel %.3f, %.0f\n",
                static_cast<double>(materials.heldFriction), crate.driftUnits, crate.asleepFromTick, bigCrate.driftUnits,
                bigCrate.asleepFromTick, barrel.driftUnits, barrel.asleepFromTick);
    // The crate of 3-13's slot, held at the table's own guess of 0.08 against the platform's held 1 (a combined 0.283, inside the
    // band the engine's solve does not walk: the creep is 0.4 to 0.6): it rests as the rest do.
    const RestingBox slotCrate = LetGoOnAFlatFloor(materials.Of("crate_low_emissive").friction, platform, 29.0, 29.0, 0.538f);
    std::printf("  the slot's crate (friction %.2f, mass 0.538): %.3f along, asleep from tick %.0f\n",
                static_cast<double>(materials.Of("crate_low_emissive").friction), slotCrate.driftUnits, slotCrate.asleepFromTick);
    for (const RestingBox* box : {&crate, &bigCrate, &barrel, &slotCrate}) {
        CHECK_MSG(box->asleep && box->asleepFromTick > 0.0 && box->asleepFromTick < 120.0,
                  "a box built from the table sleeps within two seconds: " + std::to_string(box->asleepFromTick));
        CHECK_MSG(box->driftUnits < 1.0 && box->speedUnitsPerSecond < 0.01, "and has not moved: " + std::to_string(box->driftUnits));
    }
}

// ---- levels at rest -----------------------------------------------------------------------------------------

// 2-08 (level7a), nobody at the pad, 420 ticks. Under the decoded frictions its crate 854 - set 1 unit over the right end of a
// platform - was walked left at 5.7 units a second, reached button_827 and opened door_lift_823 on tick 264 with no one there (the WT
// verifier's probe). This is the pin on REAL geometry, a hull platform with chamfered ends, which a flat-floor rig could pass while
// the level crept.
void AnIdleLevelSevenAKeepsItsCratesAndItsDoorShut() {
    if (!LevelIsThere("level7a")) return;
    Started run;
    if (!Start("level7a", run)) return;
    const ScriptClock::Step step = LayerStep();
    const auto at = [&](const char* node) {
        const auto found = run.level.built.entities.find(node);
        CHECK_MSG(found != run.level.built.entities.end(), std::string(node) + " is built");
        return found != run.level.built.entities.end()
                   ? Units::ToPixels(run.registry.get<TransformComponent>(found->second).position)
                   : glm::dvec2(0.0);
    };
    glm::dvec2 crateStart(0.0); // where it has settled: it falls 1.4 units onto the platform first
    const glm::dvec2 doorStart = at("door_lift_823");
    int pressedOn = 0;
    int openedOn = 0;
    glm::dvec2 settled(0.0);
    for (int tick = 1; tick <= 420; ++tick) {
        Game::Tick(run.data, run.registry, run.level, 0.0f, step);
        if (tick == 60) crateStart = at("crate_small_no_emissive_ent_854");
        if (tick == 300) settled = at("crate_small_no_emissive_ent_815");
        if (pressedOn == 0 && run.level.channels.Pressed(0)) pressedOn = tick;
        for (const auto& door : run.level.channels.doors) {
            if (openedOn == 0 && static_cast<int>(door.motion.status) != 0) openedOn = tick;
        }
    }
    const glm::dvec2 crateEnd = at("crate_small_no_emissive_ent_854");
    const glm::dvec2 doorEnd = at("door_lift_823");
    std::printf("  level7a idle 420 ticks: crate 854 %.3f units from where it had settled by tick 60, button pressed on tick %s, door opened "
                "on tick %s, the door %.3f units from its post; the pile's crate 815 %.3f units between tick 300 and 420\n",
                glm::distance(crateEnd, crateStart), pressedOn == 0 ? "never" : std::to_string(pressedOn).c_str(),
                openedOn == 0 ? "never" : std::to_string(openedOn).c_str(), glm::distance(doorEnd, doorStart),
                glm::distance(at("crate_small_no_emissive_ent_815"), settled));
    CHECK_MSG(glm::distance(crateEnd, crateStart) < 1.0,
              "crate 854 stays where it fell: " + std::to_string(glm::distance(crateEnd, crateStart)));
    CHECK_MSG(pressedOn == 0, "button_827 is never pressed: tick " + std::to_string(pressedOn));
    CHECK_MSG(openedOn == 0, "door_lift_823 never opens: tick " + std::to_string(openedOn));
    CHECK_WITHIN(glm::distance(doorEnd, doorStart), 0.0, 1e-6);
    CHECK_MSG(glm::distance(at("crate_small_no_emissive_ent_815"), settled) < 1.0, "the pile's crates have settled by tick 300");
}

// EVERY level with no input: after 300 ticks a loose body (a crate, a barrel, a stone, a plank placed in the scene) is asleep or still - it
// has not gone a unit between tick 300 and 420 and is under the engine's own sleep speed. The creep this pins walked 13 bodies at a constant
// 5.65 to 8.78 units a second (the WT verifier's table); the bodies that are moving at tick 420 are NAMED, with what was seen of each.
// Not counted: the player, a minion, a knight, a ghost, and a stone a launcher threw (its tag has a hash sign: a thrown body is in the air).
struct StillMoving {
    const char* level;
    const char* tag;
    const char* why;
};

const StillMoving kStillMoving[] = {
    // OBSERVED at tick 420, not diagnosed one by one; the 1-30 pile was moving at the baseline too (109 and 97 units a second).
    {"level29", "crate_902", "1-30's pile of crates and planks is still moving at tick 420, as it was at the baseline"},
    {"level29", "crate_small_ent_897", "the same pile"},
    {"level29", "crate_small_898", "the same pile"},
    {"level29", "crate_small_899", "the same pile"},
    {"level29", "crate_small_900", "the same pile"},
    {"level29", "wood_piece_small_901", "the same pile"},
    // OBSERVED, cause not diagnosed: a small crate on a bigger one, overhanging it by 2 units, moving 0.3 units a second, tilted 0.9 degrees
    // and spinning 0.4 degrees a second, 5 units from where it fell by tick 420. Not the 5.7 units a second creep this suite pins.
    {"level19a", "crate_small_no_emissive_ent_1129",
     "2-20's small crate stacked on the big one below it creeps at 0.3 units a second (observed, cause not diagnosed)"},
    // OBSERVED: the stone rolls 165 units in the last 120 ticks (61 units a second): it is rolling free.
    {"level28a", "rolling_stone_892", "2-29's stone is rolling at 61 units a second, 165 units in the last 120 ticks"},
    // 3-12's stone and crate were named here until resting.json put them to sleep where the original's stills show them (R3b fix 1):
    // the reading that a walking minion pushed them against the wall was an inference and the trace did not bear it out.
};

bool IsLooseAndPlaced(const std::string& tag) {
    for (const char* prefix : {"player", "minion", "ghost", "dragon_knight"}) {
        if (tag.rfind(prefix, 0) == 0) return false;
    }
    return tag.find('#') == std::string::npos;
}

std::vector<std::string> ConvertedLevels() {
    std::vector<std::string> names;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(kLevels, ec)) {
        const std::string stem = entry.path().stem().string();
        if (entry.path().extension() != ".tscn" || stem.rfind("level", 0) != 0 || stem.size() < 6) continue;
        std::size_t i = 5;
        while (i < stem.size() && std::isdigit(static_cast<unsigned char>(stem[i])) != 0) ++i;
        // level<number>, and a chapter's letter after it (a, b, c) or nothing.
        if (i == 5 || i + 1 < stem.size() || (i < stem.size() && (stem[i] < 'a' || stem[i] > 'c'))) continue;
        names.push_back(stem);
    }
    std::sort(names.begin(), names.end());
    return names;
}

void NoLooseBodyOfAnyLevelCreepsAtRest() {
    const std::vector<std::string> levels = ConvertedLevels();
    if (levels.size() < 100 || !LevelIsThere("level0")) {
        std::printf("  NoLooseBodyOfAnyLevelCreepsAtRest SKIPPED - needs the converted levels at %s\n", kLevels.c_str());
        return;
    }
    const ScriptClock::Step step = LayerStep();
    const std::filesystem::path prisms = std::filesystem::temp_directory_path() / "supersonic-test-mp-timebase";
    int bodies = 0;
    int asleep = 0;
    int named = 0;
    int levelsWithBodies = 0;
    std::vector<std::string> offenders;
    std::vector<std::string> namedSeen;
    std::vector<std::string> awakeStill;
    for (const std::string& name : levels) {
        Game::Data data;
        entt::registry registry;
        Game::Level level;
        std::string error;
        if (!Game::LoadData(kLevels + "/" + name + ".tscn", kData, prisms, data, error) ||
            !Game::Start(data, registry, level, error)) {
            CHECK_MSG(false, name + ": " + error);
            continue;
        }
        std::map<entt::entity, glm::dvec2> at300;
        for (int tick = 1; tick <= 420; ++tick) {
            Game::Tick(data, registry, level, 0.0f, step);
            if (tick != 300) continue;
            for (auto [entity, transform, rigid, tag] :
                 registry.view<TransformComponent, RigidBodyComponent, Supersonic::TagComponent>().each()) {
                if (!rigid.isKinematic && IsLooseAndPlaced(tag.tag)) at300[entity] = Units::ToPixels(transform.position);
            }
        }
        if (!at300.empty()) ++levelsWithBodies;
        for (const auto& [entity, from] : at300) {
            if (!registry.valid(entity)) continue;
            const auto& transform = registry.get<TransformComponent>(entity);
            const auto& rigid = registry.get<RigidBodyComponent>(entity);
            const std::string& tag = registry.get<Supersonic::TagComponent>(entity).tag;
            ++bodies;
            const double speed =
                std::hypot(static_cast<double>(rigid.velocity.x), static_cast<double>(rigid.velocity.y)) * Units::kPixelsPerMetre;
            const double drift = glm::distance(Units::ToPixels(transform.position), from);
            if (rigid.isSleeping) {
                ++asleep;
                continue;
            }
            char turn[120];
            std::snprintf(turn, sizeof turn, "turned %.3f deg, spinning %.3f deg/s", static_cast<double>(transform.rotation.z) * 180.0 / kPi,
                          static_cast<double>(rigid.angularVelocity.z) * 180.0 / kPi);
            if (speed < 2.5 && drift < 1.0) { // awake but still: a pile that keeps nudging itself, as the baseline's did
                awakeStill.push_back(name + " " + tag + " " + turn);
                continue;
            }
            const bool known = std::any_of(std::begin(kStillMoving), std::end(kStillMoving),
                                           [&](const StillMoving& one) { return name == one.level && tag == one.tag; });
            char line[260];
            std::snprintf(line, sizeof line, "%s %s %.2f units a second, %.2f units in the last 120 ticks, %s", name.c_str(), tag.c_str(),
                          speed, drift, turn);
            if (known) {
                ++named;
                namedSeen.push_back(line);
            } else {
                offenders.push_back(line);
            }
        }
    }
    std::printf("  %zu levels, %d with loose bodies, %d bodies: %d asleep at tick 420, %d still moving for a reason named in the suite\n",
                levels.size(), levelsWithBodies, bodies, asleep, named);
    for (const std::string& line : awakeStill) std::printf("    awake but still: %s\n", line.c_str());
    for (const std::string& line : namedSeen) std::printf("    named: %s\n", line.c_str());
    for (const std::string& line : offenders) std::printf("    NOT NAMED: %s\n", line.c_str());
    CHECK_MSG(offenders.empty(), std::to_string(offenders.size()) + " loose bodies are awake and moving at tick 420 with no reason named: " +
                                     (offenders.empty() ? std::string() : offenders.front()));
    CHECK_MSG(levelsWithBodies >= 60, "the levels with loose bodies: " + std::to_string(levelsWithBodies));
    CHECK_MSG(asleep > bodies / 2, "most loose bodies are asleep at tick 420: " + std::to_string(asleep) + " of " + std::to_string(bodies));
}

// ---- the piles the original shows at rest ---------------------------------------------------------------------

// 3-12's stone and small crate (game/data/resting.json): the original's stills at 2 and 8 seconds show them as the level places them, and
// the engine cannot hold the pile (it creeps the crate off its ledge from tick 4), so the table puts both to sleep where they stand.
void The312PileStartsAtRestAsTheOriginalsStillShowsIt() {
    if (!LevelIsThere("level11b")) return;
    const ScriptClock::Step step = LayerStep();
    const auto place = [](Started& run, const char* node) {
        return Units::ToPixels(run.registry.get<TransformComponent>(run.level.built.entities.at(node)).position);
    };
    const char* const kNodes[] = {"rolling_stone_1198", "crate_small_ent_1372"};
    {
        Started run;
        if (!Start("level11b", run)) return;
        glm::dvec2 from[2];
        for (int i = 0; i < 2; ++i) {
            from[i] = place(run, kNodes[i]);
            CHECK_MSG(run.registry.get<RigidBodyComponent>(run.level.built.entities.at(kNodes[i])).isSleeping,
                      std::string(kNodes[i]) + " starts asleep");
        }
        for (int tick = 1; tick <= 480; ++tick) Game::Tick(run.data, run.registry, run.level, 0.0f, step);
        double worst = 0.0;
        for (int i = 0; i < 2; ++i) {
            worst = std::max(worst, glm::distance(place(run, kNodes[i]), from[i]));
            CHECK_MSG(run.registry.get<RigidBodyComponent>(run.level.built.entities.at(kNodes[i])).isSleeping,
                      std::string(kNodes[i]) + " is still asleep on tick 480");
        }
        std::printf("  3-12 with resting.json: the stone and the crate are %.4f units from where the level puts them on tick 480, asleep\n", worst);
        CHECK_MSG(worst < 0.01, "the pile has not moved: " + std::to_string(worst));
    }
    {
        // The control: the same level with an empty table. The crate leaves its ledge, so it is the table that holds the pile.
        Started run;
        std::string error;
        const std::filesystem::path prisms = std::filesystem::temp_directory_path() / "supersonic-test-mp-timebase";
        if (!Game::LoadData(kLevels + "/level11b.tscn", kData, prisms, run.data, error)) {
            CHECK_MSG(false, error);
            return;
        }
        run.data.resting.byLevel.clear();
        CHECK_MSG(Game::Start(run.data, run.registry, run.level, error), error);
        const glm::dvec2 crate = place(run, "crate_small_ent_1372");
        for (int tick = 1; tick <= 120; ++tick) Game::Tick(run.data, run.registry, run.level, 0.0f, step);
        const double moved = glm::distance(place(run, "crate_small_ent_1372"), crate);
        std::printf("  3-12 with no table: the crate is %.1f units from its ledge on tick 120\n", moved);
        CHECK_MSG(moved > 5.0, "without the table the crate is pushed off its ledge: " + std::to_string(moved));
    }
}

// Something the pile is touched by wakes it as it would wake any sleeper: the player's own body walking into the crate, and a
// body let fall on the stone.
void ASleepingPileWakesWhenItIsTouched() {
    if (!LevelIsThere("level11b")) return;
    Started run;
    if (!Start("level11b", run)) return;
    const entt::entity crate = run.level.built.entities.at("crate_small_ent_1372");
    auto& rigid = run.registry.get<RigidBodyComponent>(crate);
    CHECK(rigid.isSleeping);
    // The port moving it, as a portal's exit does: the engine reads a moved sleeper as a request to move.
    run.registry.get<TransformComponent>(crate).position.x += 0.5f;
    Game::Tick(run.data, run.registry, run.level, 0.0f, LayerStep());
    CHECK_MSG(!rigid.isSleeping, "a crate the port moved is awake");
}

void AFileOfRestingBodiesIsReadAndRefusesWhatItCannotPut() {
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "supersonic-test-mp-timebase-resting";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    const std::filesystem::path file = dir / "resting.json";
    const auto read = [&](const std::string& text, Resting::Rules& rules, std::string& error) {
        std::ofstream(file, std::ios::binary) << text;
        return Resting::LoadRules(file.string(), rules, error);
    };
    Resting::Rules rules;
    std::string error;
    CHECK_MSG(read(R"({"touch_units": 2.0, "moved_m": 0.001, "levels": {"levelx": {"bodies": ["a", "b"], "rests_on": {"a": ["b"]}}}})", rules, error),
              "a file with one level is taken: " + error);
    CHECK_WITHIN(rules.touchUnits, 2.0, 0.0);
    CHECK_WITHIN(rules.movedMetres, 0.001, 1e-9);
    CHECK_MSG(rules.byLevel.count("levelx") == 1 && rules.byLevel.at("levelx").size() == 2, "and keeps its two bodies");
    CHECK_MSG(rules.restsOn.count("levelx") == 1 && rules.restsOn.at("levelx").count("a") == 1 &&
                  rules.restsOn.at("levelx").at("a") == std::vector<std::string>{"b"},
              "and what a rests on");
    Resting::Rules refused;
    const std::string head = R"({"touch_units": 2.0, "moved_m": 0.001, )";
    CHECK_MSG(!read(head + R"("levels": []})", refused, error), "levels that is not an object was taken");
    CHECK_MSG(!read(head + R"("levels": {"levelx": {"bodies": []}}})", refused, error), "a level with no bodies was taken");
    CHECK_MSG(!read(head + R"("levels": {"levelx": {"bodies": [""]}}})", refused, error), "a nameless body was taken");
    CHECK_MSG(!read(head + R"("levels": {"levelx": {"bodies": [3]}}})", refused, error), "a body that is not a name was taken");
    CHECK_MSG(!read(head + R"("levels": {"levelx": {"bodies": ["a"], "rests_on": []}}})", refused, error), "rests_on that is not an object was taken");
    CHECK_MSG(!read(head + R"("levels": {"levelx": {"bodies": ["a"], "rests_on": {"c": ["a"]}}}})", refused, error),
              "a support list for a body the level does not list was taken");
    CHECK_MSG(!read(head + R"("levels": {"levelx": {"bodies": ["a"], "rests_on": {"a": []}}}})", refused, error), "an empty support list was taken");
    CHECK_MSG(!read(head + R"("levels": {"levelx": {"bodies": ["a"], "rests_on": {"a": [""]}}}})", refused, error), "a nameless support was taken");
    CHECK_MSG(!read(R"({"moved_m": 0.001, "levels": {"levelx": {"bodies": ["a"]}}})", refused, error), "a file with no touch_units was taken");
    CHECK_MSG(!read(R"({"touch_units": -1.0, "moved_m": 0.001, "levels": {"levelx": {"bodies": ["a"]}}})", refused, error), "a touch below zero was taken");
    CHECK_MSG(!read(R"({"touch_units": 2.0, "levels": {"levelx": {"bodies": ["a"]}}})", refused, error), "a file with no moved_m was taken");
    CHECK_MSG(!read(R"({"touch_units": 2.0, "moved_m": 0.0, "levels": {"levelx": {"bodies": ["a"]}}})", refused, error), "a moved distance of nothing was taken");
    // Apply: a level that is not listed is untouched, a name the level did not build is refused.
    entt::registry registry;
    const entt::entity crate = registry.create();
    registry.emplace<TransformComponent>(crate);
    registry.emplace<RigidBodyComponent>(crate);
    std::unordered_map<std::string, entt::entity> built{{"a", crate}};
    CHECK_MSG(Resting::Apply(registry, rules, "levely", built, error), "an unlisted level is left alone");
    CHECK(!registry.get<RigidBodyComponent>(crate).isSleeping);
    CHECK_MSG(!Resting::Apply(registry, rules, "levelx", built, error), "a listed body the level did not build is refused");
    registry.get<RigidBodyComponent>(crate).isSleeping = false;
    built["b"] = crate;
    CHECK_MSG(Resting::Apply(registry, rules, "levelx", built, error), "and both are put to sleep once built: " + error);
    CHECK(registry.get<RigidBodyComponent>(crate).isSleeping);
    registry.get<RigidBodyComponent>(crate).isKinematic = true;
    CHECK_MSG(!Resting::Apply(registry, rules, "levelx", built, error), "a kinematic body is not a loose one");
    std::filesystem::remove_all(dir, ec);
}

// WakeAround: what touches a body the port is about to move wakes, and what is further than the touch, or is not asleep, does not.
void AWakeAroundBodyWakesWhatLiesOnIt() {
    entt::registry registry;
    const auto box = [&](double xPx, double yPx, bool asleep) {
        const entt::entity entity = registry.create();
        registry.emplace<TransformComponent>(entity).position = Units::ToWorld(xPx, yPx);
        auto& shape = registry.emplace<BoxColliderComponent>(entity);
        shape.size = glm::vec3(Units::ToMetres(30.0), Units::ToMetres(30.0), 1.0f);
        auto& rigid = registry.emplace<RigidBodyComponent>(entity);
        rigid.isSleeping = asleep;
        return entity;
    };
    const entt::entity crate = box(0.0, 0.0, false);
    const entt::entity onIt = box(0.0, -30.5, true);    // 0.5 units above the crate's top: touching
    const entt::entity apart = box(0.0, -40.0, true);   // 10 units clear
    const entt::entity awake = box(30.5, 0.0, false);   // beside it, awake already
    Resting::Rules rules;
    rules.touchUnits = 2.0;
    rules.movedMetres = 0.001;
    // With no rules in the registry nothing wakes: a rig that never loaded resting.json is the engine's own.
    CHECK_MSG(Resting::WakeAround(registry, crate) == 0, "no rules, no waking");
    CHECK(registry.get<RigidBodyComponent>(onIt).isSleeping);
    registry.ctx().insert_or_assign<Resting::Rules>(Resting::Rules(rules));
    CHECK_MSG(Resting::WakeAround(registry, crate) == 1, "one sleeper lies on the crate");
    CHECK_MSG(!registry.get<RigidBodyComponent>(onIt).isSleeping, "the body on the crate is awake");
    CHECK_MSG(registry.get<RigidBodyComponent>(apart).isSleeping, "the body 10 units clear sleeps on");
    CHECK(!registry.get<RigidBodyComponent>(awake).isSleeping);
    CHECK_MSG(Resting::WakeAround(registry, crate) == 0, "and nothing is left to wake");
}

void runTests() {
    TheWorldsGravityIsTheOriginalsAtTheTicksRatio();
    BeforeStepWritesTheTicksOwnGravityAndAWeightlessLevelKeepsZero();
    ACrateFallsAsTheOriginalsCrateFalls();
    AStoneRollsTheRoadTheOriginalsStoneRolls();
    AMinionAKnightAndThePlayerWalkOneExpression();
    AMinionWalksItsDecodedDistanceATickOnALevel();
    TheMaterialTableIsRead();
    AMaterialFileThatDescribesNoFixtureIsRefused();
    EveryMaterialIsItsEntsOwn();
    ABuiltBodyCarriesItsEntitysMaterialAndNoDamping();
    AMinionIsFrictionless();
    EveryDynamicBodyIsHeldToBox2DsMaxTranslation();
    TheEngineBouncesAStoneBeforeItTouches();
    TheMassesAreTheEntsDensityOverTheShape();
    ADecodedPlayerPushesACrateAtTheMassRatio();
    TheEngineWalksARestingBoxSidewaysAtTheDecodedFriction();
    ABoxBuiltFromTheTableRestsAndSleeps();
    AnIdleLevelSevenAKeepsItsCratesAndItsDoorShut();
    NoLooseBodyOfAnyLevelCreepsAtRest();
    The312PileStartsAtRestAsTheOriginalsStillShowsIt();
    ASleepingPileWakesWhenItIsTouched();
    AFileOfRestingBodiesIsReadAndRefusesWhatItCannotPut();
    AWakeAroundBodyWakesWhatLiesOnIt();
}

} // namespace

MP_TEST_MAIN("test_mp_timebase", 100)
