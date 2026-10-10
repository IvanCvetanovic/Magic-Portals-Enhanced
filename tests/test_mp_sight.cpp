// What a knight sees, and what it does about it (planning step 158, G1a).
//
// ETHCallback_minion looks every frame through findEntityInSight: the entities of the three 128-unit buckets of its
// row that its facing runs through, in a 120 degree cone, with nothing between them and it but another knight or the
// player. It then stands (its dwell clock reset), counts the sighting, shows an exclamation mark and plays a sound
// once its own timer lets it, and waits to shoot. It also casts a ray to the waypoint it is walking to: a way that is
// blocked is not walked, it is SEARCHED, with a question mark on a timer of its own. And a knight on a `darkest` level
// is blind until a torch is lit - to the player, not to a portal.
//
// What is pinned is the DECODE (out/parity/program/G1a/decode.md; minions.json `sight` cites every number):
//   the cone     strictly above 0.5 of the facing: 60 degrees a side, the edge out
//   the range    the knight's bucket and the two along its facing, in ITS row of 128 units: no radius
//   the ray      every body but the knights', the player's and what the list names - a wall, a door, a crate, a
//                sensor (an anti_sight_wall.ent) all block - and the ignore list is by name AS SPELLED
//   the marks    an exclamation only on a NEW sighting once exclamationElapsedTime is past 4000; a question every
//                4000 ms of its own clock while the way is blocked and nothing is in sight
//   the timers   the shot's gate, which resets the dwell clock and so moves the knight, from the knight's own velocity
//   the torch    the six levels that are `darkest` and place a knight: 4-25, 4-26, 4-27, 4-29, 4-30, 4-31
//   the legs     every patrol leg of every level is clear in the static level (a blocked leg would freeze a knight)
//
// Reads the converted levels and Enhanced's data from outside this repository for its level cases, and skips those,
// saying where it looked, when either is absent. Its rig cases need neither.

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "core/Components.hpp"
#include "sim/Game.hpp"
#include "sim/LevelBuilder.hpp"
#include "sim/Lighting.hpp"
#include "sim/Minions.hpp"
#include "sim/Roles.hpp"
#include "sim/ScriptClock.hpp"
#include "sim/Tscn.hpp"
#include "sim/Units.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <set>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

using namespace MagicPortals;
using Supersonic::BoxColliderComponent;
using Supersonic::RigidBodyComponent;
using Supersonic::TransformComponent;

namespace {

const std::string kLevels = MAGICPORTALS_LEVELS_DIR;
const std::string kData = MAGICPORTALS_DATA_DIR;
const std::string kPortData = MAGICPORTALS_PORT_DATA_DIR;
constexpr double kPi = 3.14159265358979323846;

bool LevelsPresent() {
    std::error_code ec;
    return std::filesystem::is_directory(kLevels, ec) &&
           std::filesystem::is_regular_file(kData + "/entity_roles.json", ec);
}

Minions::Rules LoadRules() {
    Minions::Rules rules;
    std::string error;
    CHECK_MSG(Minions::LoadRules(kPortData + "/minions.json", rules, error), error);
    return rules;
}

// A static box, centred, in the port's pixels. `sensor` makes it the trigger a Body of Area2D is.
entt::entity Box(entt::registry& registry, const glm::dvec2& centrePx, const glm::dvec2& sizePx, bool sensor = false) {
    const entt::entity entity = registry.create();
    registry.emplace<TransformComponent>(entity).position = Units::ToWorld(centrePx.x, centrePx.y);
    auto& box = registry.emplace<BoxColliderComponent>(entity);
    box.size = glm::vec3(Units::ToMetres(sizePx.x), Units::ToMetres(sizePx.y), 1.0f);
    box.isTrigger = sensor;
    return entity;
}

// ---- the look: the cone, the range, the facing ------------------------------------------------------------

void TheSightIsTheDecodedNumbers() {
    const Minions::Rules rules = LoadRules();
    const Minions::Sight& sight = rules.sight;
    CHECK_MSG(sight.fovCos == 0.5, "findEntityInSight's 0.5 (PshC4 1056964608, ETHCallback_minion #605)");
    CHECK_MSG(sight.bucketUnits == 128.0, "the bucket is 128 level units, not the 256 the notes said");
    CHECK_MSG(sight.targetPlayer == "dark_mage.ent" && sight.targetPortal == "portal.ent", "the two targets");
    CHECK_MSG(sight.exclamationStartMs == 3500.0 && sight.exclamationGateMs == 4000.0, "the exclamation's clock");
    CHECK_MSG(sight.questionStartMs == 3000.0 && sight.questionGateMs == 4000.0, "the question's clock");
    CHECK_MSG(sight.searchPeriodMs == 4000.0 && sight.searchRightAfterMs == 2000.0, "two seconds right, two left");
    CHECK_MSG(sight.markAboveUnits == 36.0 && sight.markBehindUnits == 32.0, "the mark's offset");
    CHECK_MSG(sight.waitNearMs == 1000.0 && sight.waitFarMs == 650.0 && sight.nearUnits == 42.0 &&
                  sight.movingSpeed == 0.3 && sight.movingAddMs == 3000.0 && sight.minIntervalMs == 2000.0,
              "the shot's gate");
    CHECK_MSG(sight.sightRayIgnores == std::vector<std::string>({"ghost_minion.ent", "minion.ent", "dark_mage.ent"}),
              "findEntityInSight's list");
    CHECK_MSG(sight.wayRayIgnores ==
                  std::vector<std::string>({"ghost_minion.ent", "minion.ent", "char_feet.ent", "crate_small",
                                            "shock_agent", "shock_agent.ent"}),
              "the walk probe's list");
    CHECK_MSG(sight.wayRayIgnoresWhenSeeing == std::vector<std::string>({"dark_mage.ent"}),
              "and what it adds while it can see");
}

// A knight at (300, 64) looking right, the player 40 units out at an angle (y is down, so a positive angle is below).
void TheConeIsAHundredAndTwentyDegrees() {
    const Minions::Rules rules = LoadRules();
    entt::registry registry;
    Minions::Senses senses;
    senses.playerPresent = true;
    const glm::dvec2 knight(300.0, 64.0);
    const glm::dvec2 facing(1.0, 0.0);
    const auto seesAt = [&](const Minions::Rules& by, double degrees) {
        const double radians = degrees * kPi / 180.0;
        senses.playerPx = knight + glm::dvec2(std::cos(radians), std::sin(radians)) * 40.0;
        return Minions::Look(registry, by, knight, facing, senses, {}).has_value();
    };
    for (const double degrees : {0.0, 30.0, -30.0, 59.0, -59.0}) {
        CHECK_MSG(seesAt(rules, degrees), "seen at " + std::to_string(degrees) + " degrees off its facing");
    }
    for (const double degrees : {61.0, -61.0, 90.0, 120.0, 180.0}) {
        CHECK_MSG(!seesAt(rules, degrees), "not seen at " + std::to_string(degrees) + " degrees off its facing");
    }

    // The edge is OUT: the test is `product > fovAngleCos` (#203-204 CMPf, JNP skips). (30, 40) is exactly 0.6 of
    // the way round at any distance - normalize gives (0.6, 0.8) in a double - so a cone of 0.6 loses it, and
    // (30, 39) is inside.
    Minions::Rules edge = rules;
    edge.sight.fovCos = 0.6;
    senses.playerPx = knight + glm::dvec2(30.0, 40.0);
    CHECK_MSG(!Minions::Look(registry, edge, knight, facing, senses, {}), "a target exactly on the cone's edge is not seen");
    senses.playerPx = knight + glm::dvec2(30.0, 39.0);
    CHECK_MSG(Minions::Look(registry, edge, knight, facing, senses, {}).has_value(), "one a hair inside it is");

    // And the cone turns with the facing: the same player behind a knight facing left is in front of it.
    senses.playerPx = knight + glm::dvec2(40.0, 0.0);
    CHECK(Minions::Look(registry, rules, knight, glm::dvec2(1.0, 0.0), senses, {}).has_value());
    CHECK(!Minions::Look(registry, rules, knight, glm::dvec2(-1.0, 0.0), senses, {}));
}

// No radius: the knight's own 128-unit bucket and the two along its facing, in ITS row.
void TheRangeIsThreeBucketsOfItsRow() {
    const Minions::Rules rules = LoadRules();
    entt::registry registry;
    Minions::Senses senses;
    senses.playerPresent = true;
    const glm::dvec2 knight(300.0, 64.0); // bucket (2, 0)
    const auto seen = [&](const glm::dvec2& player, const glm::dvec2& facing) {
        senses.playerPx = player;
        return Minions::Look(registry, rules, knight, facing, senses, {}).has_value();
    };
    CHECK(Minions::BucketOf(knight, rules.sight.bucketUnits) == glm::ivec2(2, 0));
    const glm::dvec2 right(1.0, 0.0);
    const glm::dvec2 left(-1.0, 0.0);
    CHECK_MSG(seen({639.0, 64.0}, right), "the last unit of the third bucket (x 256..639) is seen: 339 away");
    CHECK_MSG(!seen({640.0, 64.0}, right), "the first of the fourth is not: 340 away, in plain view");
    CHECK_MSG(seen({0.0, 64.0}, left), "facing left the three buckets are 2, 1 and 0: x 0 is seen");
    CHECK_MSG(!seen({-1.0, 64.0}, left), "and bucket -1 is not");
    CHECK_MSG(seen({400.0, 127.0}, right), "the row is 128 units tall: y 127 is in it");
    CHECK_MSG(!seen({400.0, 128.0}, right), "y 128 is the next row, though the angle is 33 degrees and nothing stands between");
    CHECK_MSG(!seen({400.0, -1.0}, right), "and so is y -1");
    // A knight on a row's edge sees its own row only: at y 127.5 a player at y 129 is a row away.
    const glm::dvec2 low(300.0, 127.5);
    senses.playerPx = glm::dvec2(330.0, 129.0);
    CHECK_MSG(!Minions::Look(registry, rules, low, right, senses, {}), "a row away, 1.5 units below");
}

// moveVec is (0, 0) until the walk or the search writes it: findEntityInSight prints 'enemy aims nowhere' and finds nothing.
void AFacingThatIsNotAUnitVectorSeesNothing() {
    const Minions::Rules rules = LoadRules();
    entt::registry registry;
    Minions::Senses senses;
    senses.playerPresent = true;
    senses.playerPx = glm::dvec2(330.0, 64.0);
    const glm::dvec2 knight(300.0, 64.0);
    CHECK(!Minions::Look(registry, rules, knight, glm::dvec2(0.0, 0.0), senses, {}));
    CHECK(!Minions::Look(registry, rules, knight, glm::dvec2(1.0, 1.0), senses, {}));
    CHECK(Minions::Look(registry, rules, knight, glm::dvec2(1.0, 0.0), senses, {}).has_value());
}

// ---- the ray -----------------------------------------------------------------------------------------------

void AWallADoorACrateAndASensorBlockTheLook() {
    const Minions::Rules rules = LoadRules();
    entt::registry registry;
    Minions::Senses senses;
    senses.playerPresent = true;
    const glm::dvec2 knight(300.0, 64.0);
    const glm::dvec2 facing(1.0, 0.0);
    senses.playerPx = glm::dvec2(400.0, 64.0);
    CHECK_MSG(Minions::Look(registry, rules, knight, facing, senses, {}).has_value(), "nothing between: seen");

    // Four kinds of body, each alone between them: a wall and a door and a crate are solid, and an anti_sight_wall.ent is a
    // SENSOR - Box2D's ray cast asks no fixture whether it is one, and that is its whole purpose.
    const std::vector<std::pair<const char*, bool>> bodies = {
        {"a wall", false}, {"a door", false}, {"a crate", false}, {"an anti_sight_wall.ent (a sensor)", true}};
    for (const auto& [name, sensor] : bodies) {
        entt::registry local;
        Box(local, glm::dvec2(350.0, 64.0), glm::dvec2(10.0, 100.0), sensor);
        CHECK_MSG(!Minions::Look(local, rules, knight, facing, senses, {}), std::string(name) + " blocks the look");
    }

    // A body BESIDE the ray does not, and one the ray starts inside does not either (Box2D reports neither).
    entt::registry beside;
    Box(beside, glm::dvec2(350.0, 100.0), glm::dvec2(10.0, 20.0)); // y 90..110, the ray is at y 64
    CHECK(Minions::Look(beside, rules, knight, facing, senses, {}).has_value());
    entt::registry around;
    Box(around, knight, glm::dvec2(60.0, 60.0)); // the knight stands inside it
    CHECK_MSG(Minions::Look(around, rules, knight, facing, senses, {}).has_value(), "a body the ray starts inside is not met");

    // And what the list names passes: another knight's body is in the way and is not.
    entt::registry crowded;
    const entt::entity other = Box(crowded, glm::dvec2(350.0, 64.0), glm::dvec2(10.0, 100.0));
    CHECK(!Minions::Look(crowded, rules, knight, facing, senses, {}));
    CHECK_MSG(Minions::Look(crowded, rules, knight, facing, senses, {other}).has_value(), "the knights' own bodies are ignored");
}

// The walk probe's list is by NAME as the level spells it: 'crate_small' passes, 'crate_small.ent' does not, and
// 'shock_agent' and 'shock_agent.ent' both do (ETHEntityNameArrayChooser compares GetEntityName() exactly).
void TheWalkProbeIgnoresByNameAsSpelled() {
    const Minions::Rules rules = LoadRules();
    const std::string text = R"([gd_scene load_steps=3 format=3]

[sub_resource type="RectangleShape2D" id="C"]
size = Vector2(30, 30)

[node name="lvl" type="Node2D"]

[node name="crate_bare" type="Node2D" parent="."]
position = Vector2(100, 64)
metadata/entity_name = "crate_small"

[node name="Body" type="RigidBody2D" parent="crate_bare"]

[node name="Shape" type="CollisionShape2D" parent="crate_bare/Body"]
shape = SubResource("C")

[node name="crate_ext" type="Node2D" parent="."]
position = Vector2(300, 64)
metadata/entity_name = "crate_small.ent"

[node name="Body" type="RigidBody2D" parent="crate_ext"]

[node name="Shape" type="CollisionShape2D" parent="crate_ext/Body"]
shape = SubResource("C")

[node name="agent_bare" type="Node2D" parent="."]
position = Vector2(500, 64)
metadata/entity_name = "shock_agent"

[node name="Body" type="Area2D" parent="agent_bare"]

[node name="Shape" type="CollisionShape2D" parent="agent_bare/Body"]
shape = SubResource("C")

[node name="agent_ext" type="Node2D" parent="."]
position = Vector2(700, 64)
metadata/entity_name = "shock_agent.ent"

[node name="Body" type="Area2D" parent="agent_ext"]

[node name="Shape" type="CollisionShape2D" parent="agent_ext/Body"]
shape = SubResource("C")

[node name="wall" type="Node2D" parent="."]
position = Vector2(900, 64)
metadata/entity_name = "wall04"

[node name="Body" type="StaticBody2D" parent="wall"]

[node name="Shape" type="CollisionShape2D" parent="wall/Body"]
shape = SubResource("C")
)";
    Tscn::Scene scene;
    std::string error;
    CHECK_MSG(Tscn::Parse(text, scene, error), error);
    entt::registry registry;
    LevelBuilder::Options options;
    options.prismDirectory = std::filesystem::temp_directory_path() / "supersonic-test-mp-sight";
    LevelBuilder::Built built;
    CHECK_MSG(LevelBuilder::Build(scene, registry, options, built, error), error);

    Minions::State state;
    state.rules = rules;
    state.BindSight(scene, built);
    const auto& passed = state.wayIgnoredPlaced;
    const auto has = [&](const char* node) {
        return std::find(passed.begin(), passed.end(), built.entities.at(node)) != passed.end();
    };
    CHECK_MSG(has("crate_bare") && !has("crate_ext"), "crate_small is passed, crate_small.ent is not");
    CHECK_MSG(has("agent_bare") && has("agent_ext"), "shock_agent and shock_agent.ent both");
    CHECK_MSG(!has("wall"), "and a wall is not");
    CHECK_MSG(state.sightIgnoredPlaced.empty(), "the look's list names no placed body: knights and the player only");

    const Minions::Senses senses;
    // Along y 64, each body alone in the way of a ray across it.
    CHECK_MSG(Minions::RayClear(registry, {60.0, 64.0}, {140.0, 64.0}, passed, senses, false), "through crate_small");
    CHECK_MSG(!Minions::RayClear(registry, {260.0, 64.0}, {340.0, 64.0}, passed, senses, false),
              "not through crate_small.ent");
    CHECK_MSG(Minions::RayClear(registry, {460.0, 64.0}, {540.0, 64.0}, passed, senses, false), "through shock_agent");
    CHECK_MSG(Minions::RayClear(registry, {660.0, 64.0}, {740.0, 64.0}, passed, senses, false),
              "through shock_agent.ent");
    CHECK_MSG(!Minions::RayClear(registry, {860.0, 64.0}, {940.0, 64.0}, passed, senses, false), "not through a wall");
    // A ray of no length is nothing (GetClosestContact: a == b).
    CHECK(Minions::RayClear(registry, {300.0, 64.0}, {300.0, 64.0}, passed, senses, false));
}

// ---- blind -------------------------------------------------------------------------------------------------

void ABlindKnightSeesAPortalAndNotThePlayer() {
    const Minions::Rules rules = LoadRules();
    entt::registry registry;
    Minions::Senses senses;
    senses.playerPresent = true;
    senses.playerPx = glm::dvec2(360.0, 64.0);
    senses.portalsPx.push_back(glm::dvec2(380.0, 64.0));
    const glm::dvec2 knight(300.0, 64.0);
    const glm::dvec2 facing(1.0, 0.0);

    senses.canSeeAnything = true;
    const auto both = Minions::Look(registry, rules, knight, facing, senses, {});
    CHECK(both && both->name == "dark_mage.ent" && both->portalIndex == -1);
    senses.canSeeAnything = false;
    const auto blind = Minions::Look(registry, rules, knight, facing, senses, {});
    CHECK_MSG(blind && blind->name == "portal.ent" && blind->portalIndex == 0,
              "blind, it still sees the portal ('portal.ent' alone is the target list: #578-602)");
    senses.portalsPx.clear();
    CHECK_MSG(!Minions::Look(registry, rules, knight, facing, senses, {}), "and with no portal, nothing");
}

// The walk probe's list gains ';dark_mage.ent' only while the knight can see: a blind knight's way is blocked by the
// player's body - the two circles of dark_mage.ent, which the segment test of the bodies does not hold.
void ABlindKnightsWayIsBlockedByThePlayersBody() {
    entt::registry registry;
    Minions::Senses senses;
    senses.playerPresent = true;
    senses.playerPx = glm::dvec2(350.0, 64.0);
    senses.playerBodyCircles[0] = Minions::Senses::Circle{senses.playerPx - glm::dvec2(0.0, 9.0), 9.1};
    senses.playerBodyCircles[1] = Minions::Senses::Circle{senses.playerPx + glm::dvec2(0.0, 9.0), 9.1};
    const glm::dvec2 from(300.0, 64.0);
    const glm::dvec2 to(400.0, 64.0);
    CHECK_MSG(Minions::RayClear(registry, from, to, {}, senses, false), "a knight that sees passes the player");
    CHECK_MSG(!Minions::RayClear(registry, from, to, {}, senses, true), "one that cannot is stopped by it");
    // The ray past the player's head (y 64 - 9 - 9.1 = 45.9 is the top) is clear.
    CHECK(Minions::RayClear(registry, glm::dvec2(300.0, 45.0), glm::dvec2(400.0, 45.0), {}, senses, true));
    // A ray that starts inside a circle meets nothing of it (Box2D's shape ray cast): from inside the upper one, out
    // along its own row, with the lower one 18 below the line.
    CHECK_MSG(Minions::RayClear(registry, glm::dvec2(350.0, 55.0), glm::dvec2(400.0, 55.0), {}, senses, true),
              "a ray that starts inside a circle meets nothing of it");
}

// ---- the knight's tick: its clocks, its marks, its shot's gate, its search -------------------------------

struct Rig {
    Minions::Rules rules = LoadRules();
    Minions::State state;
    entt::registry registry;
    Minions::Senses senses;
    // 16 whole script milliseconds a tick, a 1/60 s engine tick, the original's 1/30 s physics frame: SimRatio 2.
    ScriptClock::Step step{0.016, 1.0f / 60.0f, 1.0 / 30.0, 60.0};
    int tick = 0;
    struct MarkAt {
        int tick;
        Minions::Mark mark;
    };
    std::vector<MarkAt> marks;
    std::vector<int> shots;

    Rig() { state.rules = rules; }

    // A knight whose ENTITY point is at (x, y), walking between (x + 100, y) and (x - 100, y). `arrived` makes it
    // one that has already reached a waypoint and is serving its hold (2000 ms: holdTime is 0 until the first arrival,
    // so a knight on its first leg walks whatever it sees) and that last walked toward the first of them, to the right.
    void Knight(double x, double y, bool arrived = true, double holdMs = 2000.0) {
        std::vector<Minions::Waypoint> patrol(2);
        patrol[0].name = "wayA0";
        patrol[0].atPx = glm::dvec2(x + 100.0, y);
        patrol[0].holdMs = holdMs;
        patrol[1].name = "wayA1";
        patrol[1].atPx = glm::dvec2(x - 100.0, y);
        patrol[1].holdMs = holdMs;
        const entt::entity body =
            state.Summon(registry, "knight", glm::dvec2(x, y + rules.EntityAboveCentrePx()), patrol);
        CHECK(body != entt::null);
        if (arrived) {
            state.minions.back().holdMs = holdMs;
            state.minions.back().moveVec = glm::dvec2(1.0, 0.0);
        }
    }

    void Player(double x, double y) {
        senses.playerPresent = true;
        senses.playerPx = glm::dvec2(x, y);
    }

    void Run(int ticks) {
        for (int i = 0; i < ticks; ++i) {
            ++tick;
            state.Tick(registry, step, senses);
            for (const Minions::Mark& mark : state.marks) marks.push_back(MarkAt{tick, mark});
            if (!state.shotsDue.empty()) shots.push_back(tick);
        }
    }

    Minions::Minion& K() { return state.minions.front(); }
    RigidBodyComponent& Body() { return registry.get<RigidBodyComponent>(K().body); }
};

// holdTime is 0 until the first arrival and moveVec absent until the first walk: a knight that has just been added walks on
// its first tick, WRITING its facing after findEntityInSight has read it, and looks from its second.
void ANewKnightSeesNothingUntilItsFirstWalkHasWrittenItsFacing() {
    Rig rig;
    rig.Knight(300.0, 64.0, false);
    rig.Player(380.0, 64.0);
    CHECK(rig.K().moveVec == glm::dvec2(0.0, 0.0));
    rig.Run(1);
    CHECK_MSG(rig.K().moveVec == glm::dvec2(1.0, 0.0), "the walk sets the facing, toward wayA0 on the right");
    CHECK_MSG(!rig.K().seeing && !rig.K().spotted && rig.K().spotCount == 0, "so nothing is seen on tick 1");
    CHECK(rig.K().arm == Minions::Arm::Walk);
    const double walking = rig.Body().velocity.x;
    CHECK_MSG(walking > 1.0, "62.4 units a second carried at SimRatio 2: 1.248 m/s");
    rig.Run(1);
    CHECK_MSG(rig.K().seeing && rig.K().spotted && rig.K().spotCount == 1, "and the second sees the player");
    CHECK_MSG(rig.K().elapsedMs == 0.0, "a first sighting restarts the dwell clock (#646-684)");
    // That frame still walked, and so does the next: holdTime is 0, and the dwell clock is past it.
    CHECK_MSG(rig.K().arm == Minions::Arm::Walk, "the frame of the sighting is a walk: its dwell was read before the reset");
    rig.Run(1);
    CHECK_MSG(rig.K().arm == Minions::Arm::Walk, "and a knight with no hold yet walks on while it sees (16 ms past 0)");
}

void ASightingStandsAKnightThatHasAHoldToServe() {
    Rig rig;
    rig.Knight(300.0, 64.0);
    rig.Player(380.0, 64.0);
    rig.Body().velocity.x = 1.248f; // it was walking
    rig.Run(1);
    CHECK_MSG(rig.K().seeing && rig.K().spotted && rig.K().spotCount == 1, "it sees on the first tick: its facing is written");
    CHECK_MSG(rig.K().elapsedMs == 0.0, "the dwell clock is zero");
    CHECK_MSG(rig.K().arm == Minions::Arm::Watch, "held, braked, and with the player in sight its standing frame is not set");
    CHECK_MSG(std::fabs(rig.Body().velocity.x - 1.248 * 0.3) < 1e-5, "braked by 0.3 a tick");
    rig.Run(5);
    CHECK(rig.K().arm == Minions::Arm::Watch);
    CHECK_MSG(rig.K().spotCount == 1, "one sighting, however long it lasts");
    rig.Player(380.0, 300.0); // another row
    rig.Run(1);
    CHECK_MSG(!rig.K().spotted && rig.K().arm == Minions::Arm::Hold, "out of sight it stands on its standing frame");
    CHECK_MSG(rig.K().spotCount == 1, "losing it counts nothing");
    // It walks on once its dwell has passed the waypoint's hold: 2000 ms of the clock since the sighting zeroed it.
    rig.Run(124); // tick 131: 16 x 130 = 2080 since tick 1's reset
    CHECK_MSG(rig.K().arm == Minions::Arm::Walk, "the dwell is served again and it walks");
}

void AnExclamationShowsOnlyOnANewSightingOncePastItsTimer() {
    Rig rig;
    rig.Knight(300.0, 64.0);
    rig.Player(380.0, 64.0);
    rig.Run(1);
    CHECK_MSG(rig.marks.empty(), "the first sighting is on tick 1: exclamationElapsedTime is 3516, not past 4000");
    CHECK(rig.K().spotted);
    // Keep it in view until the timer is long past: no mark, because there is no NEW sighting.
    rig.Run(60);
    CHECK_MSG(rig.marks.empty(), "a sighting that goes on shows nothing");
    // Look away on tick 62, back on tick 69.
    rig.Player(380.0, 300.0);
    rig.Run(7);
    CHECK(!rig.K().spotted);
    rig.Player(380.0, 64.0);
    rig.Run(1);
    CHECK_MSG(rig.tick == 69 && rig.marks.size() == 1, "a new sighting past 4000 shows it");
    if (!rig.marks.empty()) {
        const Rig::MarkAt& at = rig.marks.front();
        CHECK(at.tick == 69 && at.mark.kind == Minions::Mark::Kind::Exclamation && at.mark.sound &&
              !at.mark.soundNeedsScreen);
        CHECK_MSG(glm::length(at.mark.entityPx - glm::dvec2(300.0, 64.0)) < 1e-3, "at the knight's entity point");
    }
    CHECK_MSG(rig.K().exclamationMs == 0.0 && rig.K().spotCount == 2, "the timer is zeroed and the sighting counted");
    // Again at once: counted, and no mark - 48 ms on the clock.
    rig.Player(380.0, 300.0);
    rig.Run(2);
    rig.Player(380.0, 64.0);
    rig.Run(1);
    CHECK_MSG(rig.marks.size() == 1 && rig.K().spotCount == 3, "a third sighting counts and shows nothing yet");
    // And after 4000 ms more it shows again: the clock is past 4000 on the 251st tick after its zero (16 x 250 is 4000,
    // not past it), and a sighting from then on shows it.
    rig.Player(380.0, 300.0);
    rig.Run(250);
    rig.Player(380.0, 64.0);
    rig.Run(1);
    CHECK_MSG(rig.marks.size() == 2 && rig.marks[1].tick == 323, "the clock opens again after 4000 ms: tick 323");
}

// A finished game stops the sighting (#616-632: `entityInSight != null && !isFinished()`).
void AFinishedGameSeesNothingNew() {
    Rig rig;
    rig.Knight(300.0, 64.0, false);
    rig.Player(380.0, 64.0);
    rig.senses.finished = true;
    rig.Run(5);
    CHECK_MSG(rig.K().seeing && !rig.K().spotted && rig.K().spotCount == 0,
              "it still looks, but a finished game spots nothing");
    CHECK_MSG(rig.K().arm == Minions::Arm::Walk, "and walks on");
}

// The shot's gate (ETHCallback_minion #769-842), which G1a builds because a shot resets elapsedTime and so decides when a
// knight walks on; what the shot IS is G1b's. State::shotsDue is the one record.
void TheShotsGateIsTheDecodedTimers() {
    {
        // A player 80 units off (past scale(42)), the knight standing: 650 ms of its dwell clock since the
        // sighting AND more than 2000 ms since its last shot - the second is the later at the start: tick 126.
        Rig rig;
        rig.Knight(300.0, 64.0);
        rig.Player(380.0, 64.0);
        rig.Run(300);
        CHECK_MSG(rig.shots.size() >= 2, "it fires on a rhythm");
        if (rig.shots.size() >= 2) {
            CHECK_MSG(rig.shots[0] == 126, "the first: shootElapsedTime is past 2000 on tick 126 (16 x 126 = 2016), got " +
                                                std::to_string(rig.shots[0]));
            CHECK_MSG(rig.shots[1] == 252, "then every 126 ticks (16 x 125 = 2000 is not past it), got " +
                                                std::to_string(rig.shots[1]));
        }
        CHECK_MSG(rig.state.shotsDue.empty() || rig.shots.back() == rig.tick, "the record is the tick's own");
    }
    // The wait itself, with the 2000 ms limit out of the way (shootElapsedTime set high): 650 far, 1000 near, and 3000
    // more while the knight is itself moving. elapsedTime is zero on tick 1 (the sighting) and 16 a tick after.
    const auto firstShot = [](double playerX, bool moving) {
        Rig rig;
        rig.Knight(300.0, 64.0);
        rig.Player(playerX, 64.0);
        rig.K().shootMs = 5000.0;
        for (int i = 0; i < 400 && rig.shots.empty(); ++i) {
            if (moving) rig.Body().velocity.x = 1.0f; // 1.0 / SimRatio 2 = 0.5 > 0.3
            rig.Run(1);
        }
        return rig.shots.empty() ? -1 : rig.shots.front();
    };
    CHECK_MSG(firstShot(380.0, false) == 42, "far (80 units): 650 ms - 16 x 41 = 656 is the first past it: tick 42, got " +
                                                 std::to_string(firstShot(380.0, false)));
    CHECK_MSG(firstShot(330.0, false) == 64, "near (30 units): 1000 ms - 16 x 63 = 1008: tick 64, got " +
                                                 std::to_string(firstShot(330.0, false)));
    CHECK_MSG(firstShot(380.0, true) == 230, "a knight itself moving: 3650 ms - 16 x 229 = 3664: tick 230, got " +
                                                 std::to_string(firstShot(380.0, true)));
    // What a shot is due WITH.
    Rig rig;
    rig.Knight(300.0, 64.0);
    rig.Player(380.0, 64.0);
    rig.K().shootMs = 5000.0;
    rig.senses.portalsPx.push_back(glm::dvec2(390.0, 64.0));
    rig.Run(42);
    CHECK_MSG(rig.shots.size() == 1 && rig.shots[0] == 42, "one shot, on tick 42");
    if (!rig.shots.empty()) {
        CHECK(rig.state.shotsDue.size() == 1);
        const Minions::ShotDue& due = rig.state.shotsDue.front();
        CHECK_MSG(glm::length(due.casterPx - glm::dvec2(300.0, 64.0)) < 1e-3 && due.finalPx == glm::dvec2(380.0, 64.0) &&
                      due.targetName == "dark_mage.ent" && due.portalIndex == -1,
                  "the knight's point, the player's point, the player's name: the player comes before a portal");
    }
    CHECK_MSG(rig.K().elapsedMs == 0.0 && rig.K().shootMs == 0.0, "and both clocks are zero after it");
}

// The search arm and its question mark.
void ABlockedWayIsSearchedNotWaited() {
    Rig rig;
    rig.Knight(300.0, 64.0);
    // A wall across the way to wayA0 (x 400), and none to the other.
    Box(rig.registry, glm::dvec2(350.0, 64.0), glm::dvec2(10.0, 100.0));
    rig.Run(1);
    CHECK_MSG(!rig.K().wayClear && rig.K().arm == Minions::Arm::Search, "no walk: the way is blocked, the knight searches");
    CHECK_MSG(std::fabs(rig.Body().velocity.x) < 1e-6, "it does not move");
    // elapsedTime % 4000 > 2000 looks right, else left: 16 ms on tick 1.
    CHECK_MSG(rig.K().moveVec == glm::dvec2(-1.0, 0.0), "left first");
    rig.Run(124); // tick 125: 2000 ms, which is not past 2000
    CHECK(rig.K().moveVec == glm::dvec2(-1.0, 0.0));
    rig.Run(1); // tick 126: 2016
    CHECK_MSG(rig.K().moveVec == glm::dvec2(1.0, 0.0), "right for the next two seconds");
    rig.Run(124); // tick 250: 4000 % 4000 = 0
    CHECK_MSG(rig.tick == 250 && rig.K().moveVec == glm::dvec2(-1.0, 0.0), "and left again");
    // The question mark: its own clock, from 3000 ms, 16 a search tick, past 4000 on the 63rd (3000 + 16 x 63 = 4008).
    CHECK_MSG(!rig.marks.empty() && rig.marks.front().tick == 63 &&
                  rig.marks.front().mark.kind == Minions::Mark::Kind::Question,
              "the first question on tick 63");
    CHECK_MSG(rig.marks.size() >= 1 && !rig.marks.front().mark.sound,
              "silent: the way was never clear, so the 'isInSight' datum is 0 (#1421-1436)");
    CHECK_MSG(rig.marks.size() >= 1 && rig.marks.front().mark.soundNeedsScreen, "and a sound would need the mark on screen");
    CHECK_MSG(rig.marks.size() == 1, "one in 250 ticks");
    rig.Run(100); // tick 350: the next past 4000 is 251 ticks after tick 63: tick 314
    CHECK_MSG(rig.marks.size() == 2 && rig.marks[1].tick == 314, "the second 4016 ms of searching after the first");
}

void TheQuestionIsHeardOnlyAfterTheWayHadBeenClear() {
    Rig rig;
    rig.Knight(300.0, 64.0, false);
    rig.Run(1); // clear: the datum is set, and the knight walks
    CHECK(rig.K().wayClear && rig.K().wayWasClear && rig.K().arm == Minions::Arm::Walk);
    const entt::entity wall = Box(rig.registry, glm::dvec2(350.0, 64.0), glm::dvec2(10.0, 100.0));
    rig.Run(63); // ticks 2..64: the search clock counts from tick 2, 3000 + 16 x 63 = 4008 on tick 64
    CHECK_MSG(rig.marks.size() == 1 && rig.marks[0].tick == 64, "the question on tick 64");
    CHECK_MSG(rig.marks[0].mark.sound, "heard: the way had been clear");
    CHECK_MSG(!rig.K().wayWasClear, "and the datum is cleared by it");
    rig.Run(251);
    CHECK_MSG(rig.marks.size() == 2 && !rig.marks[1].mark.sound, "the next one, still blocked, is silent");
    // Open the way, and the datum comes back with the walk.
    rig.registry.destroy(wall);
    rig.Run(1);
    CHECK(rig.K().wayClear && rig.K().wayWasClear);
}

void ABlindKnightIsStoppedByThePlayerAndASeeingOneIsNot() {
    for (const bool canSee : {true, false}) {
        Rig rig;
        rig.Knight(300.0, 64.0, false);
        rig.Player(350.0, 64.0);
        rig.senses.canSeeAnything = canSee;
        rig.senses.playerBodyCircles[0] = Minions::Senses::Circle{glm::dvec2(350.0, 55.0), 9.1};
        rig.senses.playerBodyCircles[1] = Minions::Senses::Circle{glm::dvec2(350.0, 73.0), 9.1};
        rig.Run(3);
        if (canSee) {
            // It sees the player ahead of it, and its way is not blocked by it.
            CHECK_MSG(rig.K().wayClear && rig.K().seeing, "a knight that can see passes the player's body, and sees it");
        } else {
            CHECK_MSG(!rig.K().wayClear && !rig.K().seeing && rig.K().arm == Minions::Arm::Search,
                      "a blind one does not see it and is stopped by it: it searches");
        }
    }
}

void TheSpotCountCountsKnightsAndNotTheBlackOne() {
    Rig rig;
    rig.Knight(300.0, 64.0);
    rig.Knight(900.0, 64.0);
    rig.state.minions[1].entity = "ghost_minion.ent";
    rig.Player(380.0, 64.0);
    rig.Run(2);
    CHECK_MSG(rig.state.minions[0].spotCount == 1 && rig.state.minions[1].spotCount == 0, "only the first looks at it");
    rig.Player(980.0, 64.0);
    rig.Run(2);
    CHECK_MSG(rig.state.minions[1].spotCount == 1, "the black knight sees as the others do");
    CHECK_MSG(rig.state.SpotCount() == 1, "but it is not in spottedCount: GetEntityArray('minion.ent') alone");
    rig.state.minions[0].gone = true;
    CHECK_MSG(rig.state.SpotCount() == 0, "and a knight that is gone is gone from it");
}

// ---- the levels ---------------------------------------------------------------------------------------------

struct Loaded {
    Game::Data data;
    entt::registry registry;
    Game::Level level;
};

bool Begin(const std::string& levelName, Loaded& run) {
    std::string error;
    const bool ok = Game::LoadData(kLevels + "/" + levelName + ".tscn", kData,
                                   std::filesystem::temp_directory_path() / "supersonic-test-mp-sight", run.data, error) &&
                    Game::Start(run.data, run.registry, run.level, error);
    CHECK_MSG(ok, levelName + ": " + error);
    return ok;
}

// The levels, by their converted file: the six `darkest` levels with a knight are 4-25 to 4-27 and 4-29 to 4-31.
const std::vector<std::pair<const char*, const char*>> kDarkKnightLevels = {
    {"level24c", "4-25"}, {"level25c", "4-26"}, {"level26c", "4-27"},
    {"level28c", "4-29"}, {"level29c", "4-30"}, {"level30c", "4-31"}};

std::vector<std::string> LevelsWithKnights() {
    std::vector<std::string> names;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(kLevels, ec)) {
        if (entry.path().extension() != ".tscn") continue;
        const std::string stem = entry.path().stem().string();
        if (stem.rfind("level", 0) != 0) continue;
        Tscn::Scene scene;
        std::string error;
        if (!Tscn::Load(entry.path().string(), scene, error)) continue;
        for (const Tscn::Node& node : scene.nodes) {
            if (node.parent != ".") continue;
            const std::string entity = Roles::EntityName(node);
            if (entity.rfind("minion_spawn", 0) == 0 && node.Meta("waypointName") != nullptr) {
                names.push_back(stem);
                break;
            }
        }
    }
    std::sort(names.begin(), names.end());
    return names;
}

void TheTorchGivesTheKnightsTheirSightBackOnTheDarkLevels() {
    const std::vector<std::string> levels = LevelsWithKnights();
    CHECK_MSG(levels.size() == 35, "35 levels carry a knight marker: " + std::to_string(levels.size()));
    std::set<std::string> dark;
    for (const auto& [file, name] : kDarkKnightLevels) dark.insert(file);
    int blind = 0;
    for (const std::string& file : levels) {
        Loaded run;
        if (!Begin(file, run)) continue;
        const bool canSee = Game::SensesOf(run.data, run.registry, run.level).canSeeAnything;
        const bool darkLevel = dark.count(file) == 1;
        CHECK_MSG(run.level.darkest == darkLevel, file + ": `darkest` is exactly the six: " + std::to_string(darkLevel));
        CHECK_MSG(canSee == !darkLevel, file + (canSee ? " sees" : " is blind") + " at the start");
        if (!canSee) ++blind;
        // No level's own ambient is the darkest value, or a lit level would be blind without saying so.
        CHECK_MSG(run.level.fileAmbient != run.data.lighting.darkestAmbient, file + "'s file ambient is not the darkest");
    }
    CHECK_EQ(blind, 6);

    // A torch lit gives the sight back; put out, it is taken again (ETHCallback_light_off, ETHCallback_fire_signal).
    for (const auto& [file, name] : kDarkKnightLevels) {
        Loaded run;
        if (!Begin(file, run)) continue;
        CHECK_MSG(!run.level.torch.lights.empty(), std::string(name) + " has a torch");
        if (run.level.torch.lights.empty()) continue;
        CHECK_MSG(!Game::SensesOf(run.data, run.registry, run.level).canSeeAnything, std::string(name) + ": blind in the dark");
        run.level.torch.lights.front().lit = true;
        run.level.torch.lit = 1;
        CHECK_MSG(Game::SensesOf(run.data, run.registry, run.level).canSeeAnything, std::string(name) + ": a lit torch gives it back");
        run.level.torch.lights.front().lit = false;
        run.level.torch.putOut = 1;
        CHECK_MSG(!Game::SensesOf(run.data, run.registry, run.level).canSeeAnything,
                  std::string(name) + ": a torch put out takes it away again");
    }
}

// The game is finished when the level is won or the player is dead.
void TheGameIsFinishedWhenTheLevelIsWonOrThePlayerIsDead() {
    Loaded run;
    if (!Begin("level0b", run)) return;
    CHECK(!Game::SensesOf(run.data, run.registry, run.level).finished);
    run.level.hazards.playerDied = true;
    CHECK(Game::SensesOf(run.data, run.registry, run.level).finished);
    run.level.hazards.playerDied = false;
    run.level.goals.completed = true;
    CHECK(Game::SensesOf(run.data, run.registry, run.level).finished);
}

// The probe is the original's own, so a patrol that the original's knights walk has to be clear in the static level. A
// leg that is not would freeze a knight on the spot (the question mark and the search) where the port had it walk -
// and that is the one risk G1a's reachability gate carries (minions.json's old note). Every consecutive pair of
// waypoints, both ways, from each waypoint's point.
void EveryPatrolLegIsClearInTheStaticLevel() {
    const std::vector<std::string> levels = LevelsWithKnights();
    int legs = 0;
    int blocked = 0;
    for (const std::string& file : levels) {
        Loaded run;
        if (!Begin(file, run)) continue;
        Minions::State& state = run.level.minions;
        std::vector<entt::entity> ignore = state.wayIgnoredPlaced;
        for (const Minions::Minion& minion : state.minions) {
            if (minion.body != entt::null) ignore.push_back(minion.body);
        }
        const Minions::Senses senses;
        for (const Minions::Minion& minion : state.minions) {
            const std::size_t n = minion.waypoints.size();
            const glm::dvec2 start = minion.spawnPx - glm::dvec2(0.0, state.rules.EntityAboveCentrePx());
            // From where it is first placed to its first waypoint, then each waypoint to the next and the last to the first.
            std::vector<std::pair<glm::dvec2, glm::dvec2>> pairs;
            if (n >= 1) pairs.emplace_back(start, minion.waypoints[0].atPx);
            if (n >= 2) {
                for (std::size_t i = 0; i < n; ++i) pairs.emplace_back(minion.waypoints[i].atPx, minion.waypoints[(i + 1) % n].atPx);
            }
            for (const auto& [from, to] : pairs) {
                ++legs;
                if (!Minions::RayClear(run.registry, from, to, ignore, senses, false)) {
                    ++blocked;
                    char text[160];
                    std::snprintf(text, sizeof text, "%s %s: (%.1f, %.1f) to (%.1f, %.1f) is blocked", file.c_str(),
                                  minion.name.c_str(), from.x, from.y, to.x, to.y);
                    CHECK_MSG(false, text);
                }
            }
        }
    }
    std::printf("  %d patrol legs in %zu levels, %d blocked\n", legs, levels.size(), blocked);
    CHECK(legs > 100);
}

void runTests() {
    TheSightIsTheDecodedNumbers();
    TheConeIsAHundredAndTwentyDegrees();
    TheRangeIsThreeBucketsOfItsRow();
    AFacingThatIsNotAUnitVectorSeesNothing();
    AWallADoorACrateAndASensorBlockTheLook();
    TheWalkProbeIgnoresByNameAsSpelled();
    ABlindKnightSeesAPortalAndNotThePlayer();
    ABlindKnightsWayIsBlockedByThePlayersBody();
    ANewKnightSeesNothingUntilItsFirstWalkHasWrittenItsFacing();
    ASightingStandsAKnightThatHasAHoldToServe();
    AnExclamationShowsOnlyOnANewSightingOncePastItsTimer();
    AFinishedGameSeesNothingNew();
    TheShotsGateIsTheDecodedTimers();
    ABlockedWayIsSearchedNotWaited();
    TheQuestionIsHeardOnlyAfterTheWayHadBeenClear();
    ABlindKnightIsStoppedByThePlayerAndASeeingOneIsNot();
    TheSpotCountCountsKnightsAndNotTheBlackOne();
    if (!LevelsPresent()) {
        std::printf("  the level cases SKIPPED - need the converted levels at %s and Enhanced's data at %s\n",
                    kLevels.c_str(), kData.c_str());
        return;
    }
    TheTorchGivesTheKnightsTheirSightBackOnTheDarkLevels();
    TheGameIsFinishedWhenTheLevelIsWonOrThePlayerIsDead();
    EveryPatrolLegIsClearInTheStaticLevel();
}

} // namespace

MP_TEST_MAIN("test_mp_sight", 120)
