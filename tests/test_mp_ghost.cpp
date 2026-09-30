// Chapter 3's boss: the ghost of level 3-32, and the fight the fire diamond
// exists for.
//
// What is pinned here is the DECODE, and above all two things that make this
// boss different from every other hazard in the port.
//
//   when        the owner's standing ruling R4 (planning step 98): the ghost is
//               NOT on the level from its start. level31b places ghost_pos, which
//               is only where it flies to; the ghost.ent is added by the black
//               knight's death (ETHCallback_ghost_minion #35-104) at the knight's
//               point, and until then there is no ghost, no shot and no restock.
//               Its init block runs on the tick after, and flies it in on five
//               waypoints - black and 0.3 to white and 1 - to hover at ghost_pos.
//   the weapon  a BARREL's blast is the only thing that hurts it (planning step
//               117): its SHOOTING arm ends with isBurned(this), and only explode()
//               burns it - ghost.ent is a sensor, and a fireball passes through it
//               (ETHBeginContactCallback_fireball #46-121; F15 filmed it). The
//               barrel is the one it puts back itself at barrel_spawn on every
//               beat, built from the port's template table (level31b places none),
//               and the fire diamond's fireballs are what set it off.
//
// The rest a careless port gets wrong:
//
//   the gate    it shoots only at a player whose x is past the level's red_line
//               (262 in level31b), so a player on the left of the arena is left
//               alone. The suite uses that as a lever: parked behind the line,
//               the boss never shoots, and everything else can be measured
//               without the player being killed mid-test.
//   the escort  RAGE_MODE waits for the minion it summoned to be killed, and it
//               seeks that one BY NAME.
//   the key     level31b holds a keyhole and a locked door and NO KEY. The only
//               key in the level is the one this boss drops, so the fight is the
//               lock, and the key is yellow because key.ent's own custom data
//               says so - not because level31b's keyhole is.
//
// Reads the converted levels and the remake's data from outside this repository,
// and skips, saying where it looked, when either is absent.

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "core/Components.hpp"
#include "sim/Game.hpp"
#include "sim/LevelBuilder.hpp"
#include "sim/Units.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <filesystem>
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
const double kStepMs = static_cast<double>(kStep) * 1000.0;

constexpr const char* kLevel = "level31b";
constexpr const char* kKnight = "minion_spawn_908";

// Behind the red line at x 262, so the boss never shoots and a test can run for
// twenty seconds without the player being killed in the middle of it.
const glm::dvec2 kBehindTheLine(100.0, 208.0);
// And past it, for the one test that is about the shooting.
const glm::dvec2 kPastTheLine(400.0, 208.0);

struct Run {
    Game::Data data;
    entt::registry registry;
    Game::Level level;
};

bool Begin(const std::string& levelName, Run& run) {
    std::string error;
    const bool ok = Game::LoadData(kLevels + "/" + levelName + ".tscn", kData,
                                   std::filesystem::temp_directory_path() / "supersonic-test-mp-ghost", run.data,
                                   error) &&
                    Game::Start(run.data, run.registry, run.level, error);
    CHECK_MSG(ok, levelName + std::string(": ") + error);
    return ok;
}

void PutAt(entt::registry& registry, entt::entity body, const glm::dvec2& atPx) {
    const glm::vec3 world = Units::ToWorld(atPx.x, atPx.y);
    auto& transform = registry.get<TransformComponent>(body);
    transform.position = glm::vec3(world.x, world.y, transform.position.z);
    if (registry.all_of<RigidBodyComponent>(body)) {
        registry.get<RigidBodyComponent>(body).velocity = glm::vec3(0.0f);
    }
}

// Ticks with the player held where it is put, which is what keeps it out of the
// boss's reach for as long as a test needs.
void Hold(Run& run, const glm::dvec2& atPx, int ticks) {
    for (int tick = 0; tick < ticks; ++tick) {
        if (run.level.player != entt::null) PutAt(run.registry, run.level.player, atPx);
        Game::Tick(run.data, run.registry, run.level, 0.0f, kStep);
    }
}

const Minions::Minion* Knight(const Run& run) {
    return run.level.minions.FindMinion(kKnight);
}

// THE FIRE ROUTE, F15's: a fireball where the knight's body is, the fire
// diamond's own kind (it cannot hurt the player). The fireball block of AfterStep
// burns it (Minions::Burn), and the ghost block adds the ghost. Returns whether the
// knight died on this one tick; `atX`, where across its patrol it was put first.
bool BurnTheKnight(Run& run, const double* atX = nullptr) {
    const Minions::Minion* knight = Knight(run);
    if (knight == nullptr || knight->gone || knight->body == entt::null) return false;
    if (atX != nullptr) {
        const glm::dvec2 standing = Units::ToPixels(run.registry.get<TransformComponent>(knight->body).position);
        PutAt(run.registry, knight->body, glm::dvec2(*atX, standing.y));
    }
    const glm::dvec2 bodyPx = Units::ToPixels(run.registry.get<TransformComponent>(knight->body).position);
    Turrets::Fireball ball;
    ball.name = "test#knight";
    ball.atPx = bodyPx;
    ball.killsPlayer = false;
    run.level.turrets.fireballs.push_back(ball);
    if (run.level.player != entt::null) PutAt(run.registry, run.level.player, kBehindTheLine);
    Game::Tick(run.data, run.registry, run.level, 0.0f, kStep);
    return Knight(run) != nullptr && Knight(run)->gone;
}

// Past the knight's death and into SHOOTING: its init the tick after, and 227
// updates of five waypoints (1200 + 2500 + 50 ms, a stride-0 leg of one callback,
// then the terminal fifth).
void ReachShooting(Run& run) {
    BurnTheKnight(run);
    Hold(run, kBehindTheLine, 240);
}

// One round of the fight: burn it, let the damage run into rage, kill the escort
// it calls, and come back to shooting.
void BurnAndLetItRecover(Run& run) {
    run.level.ghost.burned = true;
    Hold(run, kBehindTheLine, 2); // DAMAGE

    // 4000 ms of damage, then RAGE_MODE and the summon on the next tick.
    Hold(run, kBehindTheLine, 250);

    // Kill whatever it called, which is what rage is waiting for.
    for (const Minions::Minion& minion : run.level.minions.minions) {
        if (minion.gone || minion.body == entt::null) continue;
        if (minion.name.rfind(run.level.ghost.name + "#", 0) != 0) continue;
        run.level.minions.Take(run.registry, {minion.body});
        break;
    }
    Hold(run, kBehindTheLine, 30);
}

double SmoothEnd(double v) {
    return std::sin(v * 1.5707963267948966);
}

void theRulesAndWhatLevelThirtyOneBHolds() {
    Run run;
    if (!Begin(kLevel, run)) return;

    const Ghost::Rules& rules = run.level.ghost.rules;
    CHECK_EQ(rules.maxHp, 3);
    // explode's two boxes, both ghost.json blast's: the grabber's, the .ent's
    // Collision at the global scale alone, and the body its ray meets, that
    // Collision at the scale the ghost was added with.
    CHECK_MSG(rules.grabBoxPx == glm::dvec2(128.0, 139.0) && rules.bodyBoxPx == glm::dvec2(128.0, 139.0) &&
                  rules.bodyOffsetPx == glm::dvec2(5.0, -7.0),
              "ghost.ent's Collision, 128 x 139 at (5, -7)");
    // addEntityIfItCantBeFound's five arguments (ins 594-630), pushed last first.
    CHECK_MSG(rules.restock.search == "barrel_bomb" && rules.restock.add == "barrel_bomb_low_emissive.ent" &&
                  rules.restock.spawn == "barrel_spawn" && rules.restock.effect == "ghost_utility_spawn.ent" &&
                  rules.restock.alias == "barrel_bomb" && rules.restock.templateEntity == "barrel_bomb_low_emissive.ent",
              "the barrel it puts back: barrel_bomb_low_emissive.ent at barrel_spawn, found and named barrel_bomb");
    CHECK(::test::nearly(static_cast<float>(rules.intervalMs), 3000.0f));
    CHECK(::test::nearly(static_cast<float>(rules.toRageMs), 4000.0f));
    CHECK(::test::nearly(static_cast<float>(rules.burstMs), 3550.0f));
    CHECK(::test::nearly(static_cast<float>(rules.shotSpeedPxS), 120.0f));
    CHECK(::test::nearly(static_cast<float>(rules.shotHitPx), 16.0f));
    CHECK_MSG(rules.keyColour == "yellow", "key.ent's own custom data, not level31b's keyhole");
    CHECK_MSG(rules.anchorName == "ghost_pos", "the boss_spawn node, matched by name");

    // R4: what adds it, where and how big, and what the same death deletes.
    CHECK_MSG(rules.spawnedBy == "ghost_minion.ent", "the black knight's death adds it: " + rules.spawnedBy);
    CHECK_MSG(rules.spawnZOffsetUnits == -1.0 && rules.spawnScale == 0.3 && rules.deletesEntityId == 1430,
              "scale(-1) behind it, at scale(0.3), and entity 1430 deleted");
    CHECK_MSG(run.level.ghost.deletesNode == "anti_sight_wall_ent_1430",
              "1430 is level31b's anti_sight_wall: " + run.level.ghost.deletesNode);
    CHECK_MSG(run.level.built.entities.count("anti_sight_wall_ent_1430") == 1, "and the builder made its body");
    // The five waypoints of the init block, and the shot's nudge and cells.
    CHECK_EQ(rules.waypoints.size(), static_cast<std::size_t>(5));
    if (rules.waypoints.size() == 5) {
        const double times[5] = {1200.0, 2500.0, 50.0, 0.0, 0.0};
        const double alphas[5] = {170.0 / 255.0, 204.0 / 255.0, 1.0, 1.0, 1.0};
        int right = 0;
        for (std::size_t i = 0; i < 5; ++i) {
            const Ghost::WaypointRule& one = rules.waypoints[i];
            const bool white = i >= 2;
            if (one.timeMs == times[i] && std::fabs(one.colour.a - alphas[i]) < 1e-12 &&
                one.colour.r == (white ? 1.0 : 0.0) && one.colour.g == one.colour.r && one.colour.b == one.colour.r &&
                one.filter == (i == 1 ? Ghost::Filter::SmoothEnd : Ghost::Filter::Linear) &&
                one.at == (i == 0 ? Ghost::WaypointAt::Spawn
                                  : (i == 1 ? Ghost::WaypointAt::Quarter : Ghost::WaypointAt::Anchor)) &&
                one.scaleIsSpawn == (i == 0) && (i == 0 || one.scale == (i == 1 ? 0.6 : 1.0))) {
                ++right;
            }
        }
        CHECK_MSG(right == 5, "the five addWaypoint calls as the listing makes them: " + std::to_string(right));
    }
    CHECK(rules.midFraction == 0.25 && rules.seedElapsedMs == 3000.0);
    CHECK_MSG(rules.shotNudgePx == 16.0 && rules.shotFirstCell == 0 && rules.shotLastCell == 5 &&
                  rules.shotCellStrideMs == 30.0,
              "a shot starts 16 out, on _projectileFrameTimer.set(0, 5, 30)");
    CHECK_MSG(rules.shootingFrame == 0 && rules.rageFrame == 3 && rules.rageAlpha == 0.5 &&
                  rules.alphaLeavingRage == 1.0 && rules.damageAlpha == 0.5 && rules.deathAlpha == 1.0,
              "cell 0 shooting, 3 at alpha 0.5 raging, 0.5 hurt, 1 dying");
    CHECK_MSG(rules.damageBounce.from == 1.0 && rules.damageBounce.to == 1.1 && rules.damageBounce.strideMs == 500.0 &&
                  rules.deathBounce.from == 1.1 && rules.deathBounce.to == 1.25 && rules.deathBounce.strideMs == 400.0,
              "bounce(1 -> 1.1, 500) hurt, bounce(1.1 -> 1.25, 400) dying");
    // The hover's row, which the simulation keeps (motions.json ghost).
    const Motion::Row& hover = run.data.motion.ghost;
    CHECK_MSG(hover.vertical && hover.axisDeg == 0.0 && std::fabs(hover.speed - 0.7) < 1e-6 && hover.stride == 8.0 &&
                  hover.startFrom == hover.startTo && std::fabs(hover.startFrom - 1.5707963705062866) < 1e-12,
              "linearMotion(this, true, 0, PIb) at ghost.ent's speed 0.7 and stride 8");

    CHECK_MSG(run.level.ghost.present, "level31b places a ghost_pos");
    CHECK_MSG(!run.level.ghost.spawned && !run.level.ghost.Alive(), "and no ghost: the knight is alive (R4)");
    CHECK_MSG(std::fabs(run.level.ghost.anchorPx.x - 488.0) < 1.0 && std::fabs(run.level.ghost.anchorPx.y - 154.0) < 1.0,
              "ghost_pos, where it will fly to");

    // The three markers it reads by name, none of which carries a role the port
    // plays - minion_spawn_point carries no role at all.
    CHECK_MSG(run.level.ghost.hasRedLine, "it found the red line");
    CHECK(::test::nearly(static_cast<float>(run.level.ghost.redLineX), 262.0f));
    CHECK_MSG(run.level.ghost.hasSummonAnchor, "and its summon anchor");
    CHECK_MSG(std::fabs(run.level.ghost.summonPx.x - 436.0) < 1.0 &&
                  std::fabs(run.level.ghost.summonPx.y - 96.0) < 1.0,
              "at minion_spawn_point");
    CHECK_MSG(run.level.ghost.hasDiamondSpawn, "and where a fire diamond goes back");

    // Where its barrel goes back, and what the port builds it from. level31b places
    // no barrel of any name, so the template is the port's table's.
    const Restock::Site& site = run.level.ghost.restockSite;
    CHECK_MSG(site.spawnFound && site.spawnPx == glm::dvec2(190.0, 110.0) && site.spawnZ == 0.0,
              "barrel_spawn_1435 at (190, 110), no z");
    CHECK_MSG(site.templateFromPort && site.templateNode == "barrel_bomb_low_emissive_ent",
              "built from the port's template table: " + site.templateNode);
    int barrels = 0;
    for (const Tscn::Node& node : run.data.scene.nodes) {
        const Tscn::Value* entity = node.Meta("entity_name");
        if (node.parent == "." && entity != nullptr && entity->text.rfind("barrel_bomb", 0) == 0) ++barrels;
    }
    CHECK_MSG(barrels == 0, "level31b places no barrel: " + std::to_string(barrels));
    // The template, as the converter writes barrel_bomb (templates.json checks it
    // against barrel_bomb_low_emissive.ent, byte-identical to barrel_bomb.ent).
    const Tscn::Node* made = run.data.templates.FindNode(site.templateNode);
    CHECK_MSG(made != nullptr, "the table's scene holds the template's node");
    if (made != nullptr) {
        const auto meta = [made](const char* key) {
            const Tscn::Value* value = made->Meta(key);
            return value != nullptr ? value->text : std::string("(none)");
        };
        CHECK_MSG(meta("entity_name") == "barrel_bomb_low_emissive.ent" && meta("explosive") == "1" &&
                      meta("teleportable") == "1" && meta("eth_emissive") == "0.45 0.45 0.45" &&
                      meta("eth_apply_light") == "1" &&
                      meta("eth_normal") == "res://assets/entities/normalmaps/barrel_normal.png",
                  "its custom data and eth_ keys, as barrel_bomb's");
        glm::dvec2 offsetPx(-1.0);
        glm::dvec2 sizePx(0.0);
        CHECK_MSG(LevelBuilder::ShapeBoundsPx(run.data.templates, *made, offsetPx, sizePx) &&
                      offsetPx == glm::dvec2(0.0) && std::fabs(sizePx.x - 28.6) < 1e-9 && std::fabs(sizePx.y - 37.7) < 1e-9,
                  "its box 22 x 29 at Scale 1.3: " + std::to_string(sizePx.x) + " x " + std::to_string(sizePx.y));
        const Tscn::Node* body = run.data.templates.Child(*made, "Body");
        const Tscn::Node* sprite = run.data.templates.Child(*made, "Sprite");
        CHECK_MSG(body != nullptr && body->type == "RigidBody2D", "a rigid body: type 0, static 0");
        const Tscn::Value* texture = sprite != nullptr ? sprite->Find("texture") : nullptr;
        const Tscn::Resource* file = texture != nullptr ? run.data.templates.External(texture->text) : nullptr;
        const Tscn::Value* scale = sprite != nullptr ? sprite->Find("scale") : nullptr;
        CHECK_MSG(file != nullptr && file->path == "res://assets/entities/barrel32_with_sign.png" && scale != nullptr &&
                      scale->numbers.size() == 2 && scale->numbers[0] == 1.3 && scale->numbers[1] == 1.3,
                  "barrel32_with_sign.png at its Scale 1.3");
    }
    // The table's root is the level's own lighting, so the layer reads its look.
    const auto rootMeta = [](const Tscn::Scene& scene, const char* key) {
        const Tscn::Node* root = scene.nodes.empty() ? nullptr : &scene.nodes.front();
        const Tscn::Value* value = root != nullptr ? root->Meta(key) : nullptr;
        return value != nullptr ? value->text : std::string("(none)");
    };
    CHECK_MSG(rootMeta(run.data.templates, "eth_ambient") == rootMeta(run.data.scene, "eth_ambient") &&
                  rootMeta(run.data.templates, "eth_light_intensity") == rootMeta(run.data.scene, "eth_light_intensity") &&
                  rootMeta(run.data.scene, "eth_ambient") != "(none)",
              "the table's root carries level31b's ambient and intensity");

    // The escort's patrol, resolved from the wayA prefix at Find time because
    // AfterStep has no scene to resolve it from.
    CHECK_EQ(run.level.ghost.patrol.size(), static_cast<std::size_t>(2));
    if (run.level.ghost.patrol.size() == 2) {
        CHECK(::test::nearly(static_cast<float>(run.level.ghost.patrol[0].atPx.x), 314.0f));
        CHECK(::test::nearly(static_cast<float>(run.level.ghost.patrol[0].holdMs), 1000.0f));
        CHECK(::test::nearly(static_cast<float>(run.level.ghost.patrol[1].atPx.x), 412.0f));
        CHECK(::test::nearly(static_cast<float>(run.level.ghost.patrol[1].holdMs), 4000.0f));
    }
}

// R4: while the knight lives there is no ghost - no shot at a player past the red
// line, no restock of a fire diamond that is gone, nothing a fireball can reach.
void nothingBeforeTheKnightDies() {
    Run run;
    if (!Begin(kLevel, run)) return;
    if (run.level.player == entt::null || run.level.diamonds.diamonds.empty()) return;

    // The fire diamond gone, as the gutter mouth takes it: once the ghost shoots it
    // puts one back on every beat, and before it there is no beat.
    for (Diamonds::Diamond& diamond : run.level.diamonds.diamonds) diamond.gone = true;
    Hold(run, kPastTheLine, 1200); // 20 s, past the red line
    CHECK_MSG(!run.level.ghost.spawned && !run.level.ghost.initialised, "no ghost after 20 s with the knight alive");
    CHECK_MSG(run.level.ghost.shots.empty() && run.level.ghost.shotsFired == 0, "and not a shot");
    bool restocked = false;
    for (const Diamonds::Diamond& diamond : run.level.diamonds.diamonds) restocked = restocked || !diamond.gone;
    CHECK_MSG(!restocked, "and no fire diamond put back");
    CHECK_MSG(run.level.restock.added.empty() && run.level.restock.refusals == 0, "and no barrel put back");
    CHECK_MSG(!run.level.hazards.playerDied, "and the player past the line is alive");
    CHECK_MSG(Knight(run) != nullptr && !Knight(run)->gone, "the knight still patrols");
    CHECK_MSG(run.level.built.entities.count("anti_sight_wall_ent_1430") == 1, "and entity 1430 still stands");

    // A fireball through ghost_pos burns nothing: there is nothing there.
    Turrets::Fireball ball;
    ball.name = "test#ghost_pos";
    ball.atPx = run.level.ghost.anchorPx;
    ball.killsPlayer = false;
    run.level.turrets.fireballs.push_back(ball);
    Hold(run, kBehindTheLine, 2);
    CHECK_MSG(!run.level.ghost.burned && run.level.ghost.hits == 0, "a fireball through ghost_pos burns no ghost");
}

// The knight's death by fire - F15's route - adds the ghost where the knight was,
// deletes entity 1430 in the same moment, and runs its init the tick after.
void aFireballOnTheKnightAddsTheGhost() {
    Run run;
    if (!Begin(kLevel, run)) return;
    Hold(run, kBehindTheLine, 30);
    const Minions::Minion* knight = Knight(run);
    CHECK_MSG(knight != nullptr && knight->entity == "ghost_minion.ent", "level31b's marker adds the black knight");
    if (knight == nullptr) return;
    const entt::entity wall = run.level.built.entities.count("anti_sight_wall_ent_1430") == 1
                                  ? run.level.built.entities.at("anti_sight_wall_ent_1430")
                                  : entt::null;

    const bool died = BurnTheKnight(run);
    CHECK_MSG(died, "a fireball on the knight burns it, that tick");
    if (!died) return;
    CHECK_EQ(run.level.minions.burned, 1);
    CHECK_MSG(!run.level.minions.deaths.empty() && run.level.minions.deaths.back().byFire &&
                  !run.level.minions.deaths.back().byKiller,
              "a Death by fire, for its blood");
    const glm::dvec2 deathPx = run.level.minions.deaths.empty() ? glm::dvec2(0.0) : run.level.minions.deaths.back().atPx;
    CHECK_MSG(run.level.turrets.fireballs.empty(), "and the fireball is spent on it");

    // THE TICK IT DIES: the ghost.ent added at its point, at the .ent's own look and
    // scale(0.3), its init not yet run; entity 1430 gone with it.
    const Ghost::State& ghost = run.level.ghost;
    CHECK_MSG(ghost.spawned && !ghost.initialised && ghost.Alive(), "added, and its first callback still to come");
    CHECK_MSG(ghost.atPx == deathPx && ghost.spawnPx == deathPx,
              "at the knight's entity point (" + std::to_string(deathPx.x) + ", " + std::to_string(deathPx.y) + ")");
    CHECK_MSG(ghost.look.colour == glm::dvec3(1.0) && ghost.look.alpha == 1.0 && ghost.look.scale == 0.3 &&
                  ghost.look.frame == 0,
              "white, alpha 1, scale 0.3, cell 0: the .ent's own look at the spawn's scale");
    CHECK_MSG(run.level.built.entities.count("anti_sight_wall_ent_1430") == 0 &&
                  (wall == entt::null || !run.registry.valid(wall)),
              "DeleteEntity(SeekEntity(1430)): the sight wall's body is gone");
    CHECK_MSG(run.level.built.entities.count("anti_sight_wall_ent_1438") == 1, "and the other sight wall is not");

    // THE TICK AFTER: the init block and the first APPEARING update - waypoint 0
    // exactly, black at alpha 0xAA / 255, scale 0.3, where it was added.
    Hold(run, kBehindTheLine, 1);
    CHECK_MSG(ghost.initialised && ghost.phase == Ghost::Phase::Appearing, "its init has run, and it is appearing");
    CHECK_MSG(ghost.appear.points.size() == 5, "five waypoints");
    CHECK_MSG(ghost.look.colour == glm::dvec3(0.0) && std::fabs(ghost.look.alpha - 170.0 / 255.0) < 1e-12 &&
                  std::fabs(ghost.look.scale - 0.3) < 1e-12 && ghost.atPx == deathPx,
              "waypoint 0: black, alpha 0.667, scale 0.3, at the knight's point");
    CHECK_EQ(ghost.hp, 3);
    CHECK_MSG(ghost.shots.empty(), "and it has not shot");
}

// The other ways a knight dies add it too: the knight's own callback deletes it
// however it was destroyed, and ghost_minion's then finds it dead.
void aKillerFloorAddsItToo() {
    Run run;
    if (!Begin(kLevel, run)) return;
    const Minions::Minion* knight = Knight(run);
    if (knight == nullptr || run.level.minions.killers.empty()) return;
    const glm::vec2 centre = run.level.minions.killers.front().box.centre;
    const glm::dvec2 floorPx = Units::ToPixels(glm::vec3(centre.x, centre.y, 0.0f));
    int ticks = 0;
    for (; ticks < 5 && !Knight(run)->gone; ++ticks) {
        PutAt(run.registry, Knight(run)->body, floorPx);
        Hold(run, kBehindTheLine, 1);
    }
    CHECK_MSG(Knight(run)->gone && run.level.minions.killed == 1, "the killer floor took the knight");
    CHECK_MSG(run.level.ghost.spawned && run.level.minions.deaths.size() == 1 &&
                  run.level.ghost.spawnPx == run.level.minions.deaths.front().atPx,
              "and the ghost was added where it went, far below the view");
    CHECK_MSG(std::fabs(run.level.ghost.spawnPx.y - floorPx.y) < 40.0, "at y " + std::to_string(run.level.ghost.spawnPx.y));
}

// THE APPEARANCE: the WaypointManager as it plays, tick by tick - and against F15,
// which measured the original's scale and path with the knight killed at (640, 208).
void itFliesInOnFiveWaypoints() {
    Run run;
    if (!Begin(kLevel, run)) return;
    Hold(run, kBehindTheLine, 10);
    // F15's knight died at about (640, 208) (its section 3, P0 of the fit).
    const double killX = 640.0;
    if (!BurnTheKnight(run, &killX)) {
        CHECK_MSG(false, "the knight burned at x 640");
        return;
    }
    const Ghost::State& ghost = run.level.ghost;
    const glm::dvec2 spawn = ghost.spawnPx;
    const glm::dvec2 anchor = ghost.anchorPx;
    const glm::dvec2 quarter = spawn + (anchor - spawn) * 0.25;
    CHECK_MSG(std::fabs(spawn.x - killX) < 1.0 && std::fabs(spawn.y - 208.0) < 3.0,
              "it is added where the knight died, on its patrol: (" + std::to_string(spawn.x) + ", " +
                  std::to_string(spawn.y) + ")");

    // The closed form of the five waypoints, from the first update's zero: 1200 ms
    // linear to the quarter, alpha 0.667 -> 0.8, scale 0.3 -> 0.6, black; 2500 ms
    // smoothEnd to ghost_pos, black -> white, alpha 0.8 -> 1, scale 0.6 -> 1.
    struct Want {
        glm::dvec2 at;
        double scale, alpha, grey;
    };
    const auto model = [&](double ms) {
        if (ms < 1200.0) {
            const double f = ms / 1200.0;
            return Want{spawn + (quarter - spawn) * f, 0.3 + 0.3 * f, 170.0 / 255.0 + (34.0 / 255.0) * f, 0.0};
        }
        if (ms < 3700.0) {
            const double f = SmoothEnd((ms - 1200.0) / 2500.0);
            return Want{quarter + (anchor - quarter) * f, 0.6 + 0.4 * f, 204.0 / 255.0 + (51.0 / 255.0) * f, f};
        }
        return Want{anchor, 1.0, 1.0, 1.0};
    };
    double worstAt = 0.0, worstLook = 0.0;
    int shootingOn = -1;
    int frameOneOn = -1;
    std::vector<double> scaleAt(400, -1.0);
    std::vector<glm::dvec2> pointAt(400, glm::dvec2(0.0));
    for (int n = 1; n <= 260; ++n) {
        Hold(run, kBehindTheLine, 1);
        scaleAt[static_cast<std::size_t>(n)] = ghost.look.scale;
        pointAt[static_cast<std::size_t>(n)] = ghost.atPx;
        if (frameOneOn < 0 && ghost.appear.frame == 1) frameOneOn = n;
        if (ghost.phase != Ghost::Phase::Appearing) {
            if (shootingOn < 0) shootingOn = n;
            continue;
        }
        const Want want = model(static_cast<double>(n - 1) * kStepMs);
        worstAt = std::max(worstAt, glm::distance(ghost.atPx, want.at));
        worstLook = std::max({worstLook, std::fabs(ghost.look.scale - want.scale), std::fabs(ghost.look.alpha - want.alpha),
                              std::fabs(ghost.look.colour.r - want.grey)});
    }
    CHECK_MSG(worstAt < 1e-3 && worstLook < 1e-4, "every APPEARING update is the five waypoints' closed form: " +
                                                      std::to_string(worstAt) + " u, " + std::to_string(worstLook));
    CHECK_MSG(frameOneOn == 73, "the second waypoint from update 73, 1200 ms in: " + std::to_string(frameOneOn));
    CHECK_MSG(shootingOn == 227,
              "SHOOTING on update 227: 3750 ms, a callback on the stride-0 leg, then the fifth: " +
                  std::to_string(shootingOn));
    CHECK_MSG(ghost.phase == Ghost::Phase::Shooting && ghost.look.colour == glm::dvec3(1.0) && ghost.look.alpha == 1.0 &&
                  ghost.look.scale == 1.0 && ghost.look.frame == 0,
              "and it arrives white, alpha 1, scale 1, on cell 0");

    // F15 (out/parity/footage/F15.md section 3, r1): the original's scale at each
    // measured moment after the knight's death (t_d 10.15), within the 0.03 its own
    // fit held, and the entity's path within 5 u of the points measured - the port's
    // tick n is n x 1/60 s after the death, and its first update the tick after.
    const struct {
        double s;
        double scale;
        glm::dvec2 at;
    } f15[] = {{11.879 - 10.15, 0.70, glm::dvec2(566.6, 180.9)}, {12.267 - 10.15, 0.80, glm::dvec2(540.9, 172.4)},
               {12.666 - 10.15, 0.875, glm::dvec2(523.1, 166.5)}, {12.886 - 10.15, 0.925, glm::dvec2(-1.0)},
               {13.092 - 10.15, 0.95, glm::dvec2(503.3, 159.6)},  {13.260 - 10.15, 0.975, glm::dvec2(-1.0)},
               {13.585 - 10.15, 1.0, glm::dvec2(490.8, 155.4)}};
    double worstScale = 0.0, worstPath = 0.0;
    for (const auto& sample : f15) {
        const int n = static_cast<int>(std::lround(sample.s * 1000.0 / kStepMs));
        const double scale = scaleAt[static_cast<std::size_t>(n)];
        worstScale = std::max(worstScale, std::fabs(scale - sample.scale));
        if (sample.at.x > 0.0) worstPath = std::max(worstPath, glm::distance(pointAt[static_cast<std::size_t>(n)], sample.at));
        std::printf("  F15 +%.3f s: scale %.3f (original %.3f), at (%.1f, %.1f)\n", sample.s, scale, sample.scale,
                    pointAt[static_cast<std::size_t>(n)].x, pointAt[static_cast<std::size_t>(n)].y);
    }
    CHECK_MSG(worstScale <= 0.03, "the scale F15 measured, within 0.03: worst " + std::to_string(worstScale));
    CHECK_MSG(worstPath <= 5.0, "the path F15 measured, within 5 u: worst " + std::to_string(worstPath));
}

// THE HOVER (motions.json ghost): vertical only, 8 units about ghost_pos, 2 PI /
// 0.7 s a swing, from the centre and moving UP at full speed (D8_check C2, F15
// section 4), in SHOOTING and RAGE_MODE only.
void itHoversAboutGhostPos() {
    Run run;
    if (!Begin(kLevel, run)) return;
    ReachShooting(run);
    const Ghost::State& ghost = run.level.ghost;
    CHECK_MSG(ghost.phase == Ghost::Phase::Shooting, "shooting");
    CHECK_MSG(ghost.hover.started && ghost.hoverOriginPx == ghost.anchorPx,
              "its linearMotion's originalPos is ghost_pos, where APPEARING left it");

    // Its first calls, from a fresh arrival: the first step already taken, moving up.
    Run fresh;
    if (!Begin(kLevel, fresh)) return;
    BurnTheKnight(fresh);
    int n = 0;
    while (n < 400 && fresh.level.ghost.phase == Ghost::Phase::Appearing) {
        Hold(fresh, kBehindTheLine, 1);
        ++n;
    }
    const double y0 = fresh.level.ghost.atPx.y;
    Hold(fresh, kBehindTheLine, 1);
    const double y1 = fresh.level.ghost.atPx.y;
    Hold(fresh, kBehindTheLine, 1);
    const double y2 = fresh.level.ghost.atPx.y;
    const double speed = (y2 - y1) / (kStepMs / 1000.0);
    CHECK_MSG(y0 == fresh.level.ghost.anchorPx.y, "it arrives at ghost_pos: " + std::to_string(y0));
    CHECK_MSG(y1 < y0 && std::fabs(speed + 5.6) < 0.01,
              "and moves UP from the centre at 5.6 u/s: " + std::to_string(speed) + " u/s");

    double top = 1e9, bottom = -1e9, xDrift = 0.0;
    int crossings = 0;
    int firstCrossing = -1, lastCrossing = -1;
    double last = ghost.atPx.y - ghost.anchorPx.y;
    for (int tick = 1; tick <= 1200; ++tick) {
        Hold(run, kBehindTheLine, 1);
        const double dy = ghost.atPx.y - ghost.anchorPx.y;
        top = std::min(top, dy);
        bottom = std::max(bottom, dy);
        xDrift = std::max(xDrift, std::fabs(ghost.atPx.x - ghost.anchorPx.x));
        // Upward through the centre: dy goes from + to -.
        if (last > 0.0 && dy <= 0.0) {
            if (firstCrossing < 0) firstCrossing = tick;
            lastCrossing = tick;
            ++crossings;
        }
        last = dy;
    }
    CHECK_MSG(std::fabs(top + 8.0) < 0.01 && std::fabs(bottom - 8.0) < 0.01,
              "8 units up and down: " + std::to_string(top) + " to " + std::to_string(bottom));
    CHECK_MSG(xDrift == 0.0, "and nowhere across: " + std::to_string(xDrift));
    const double periodMs = crossings >= 2 ? (lastCrossing - firstCrossing) * kStepMs / (crossings - 1) : 0.0;
    CHECK_MSG(std::fabs(periodMs - 2.0 * 3.14159265358979 / 0.7 * 1000.0) < 20.0,
              "a swing every 8.976 s: " + std::to_string(periodMs) + " ms");
    std::printf("  level31b: the ghost hovers %.3f to %.3f u about ghost_pos, %.1f ms a swing\n", top, bottom, periodMs);

    // Frozen in DAMAGE: where the swing left it, and on through RAGE_MODE's calls.
    run.level.ghost.burned = true;
    Hold(run, kBehindTheLine, 2);
    CHECK(ghost.phase == Ghost::Phase::Damage);
    const glm::dvec2 held = ghost.atPx;
    Hold(run, kBehindTheLine, 120);
    CHECK_MSG(ghost.atPx == held, "DAMAGE does not call linearMotion: it holds");
}

// The red line, which is the whole of whether it shoots at all.
void theRedLineGatesItsShooting() {
    Run run;
    if (!Begin(kLevel, run)) return;
    if (run.level.player == entt::null) return;

    ReachShooting(run);
    // Two whole intervals with the player behind the line.
    Hold(run, kBehindTheLine, 400);
    CHECK_MSG(run.level.ghost.shotsFired == 0, "a player left of the red line is not shot at");

    // And past it, where the first shot comes on the first beat after: from its own
    // point plus the muzzle, 16 units along its way already (ETHCallback_enemy_shoot's
    // first call), its range measured from the muzzle.
    const int before = run.level.ghost.shotsMade;
    int tick = 0;
    for (; tick < 200 && run.level.ghost.shots.empty(); ++tick) Hold(run, kPastTheLine, 1);
    CHECK_MSG(!run.level.ghost.shots.empty() && run.level.ghost.shotsMade == before + 1, "past it, it shoots");
    if (run.level.ghost.shots.empty()) return;
    const Ghost::Shot& shot = run.level.ghost.shots.front();
    const glm::dvec2 muzzle = run.level.ghost.atPx + run.level.ghost.rules.muzzlePx;
    CHECK_MSG(glm::distance(shot.fromPx, muzzle) < 1e-9, "from its point plus (-20, -32)");
    CHECK_MSG(glm::distance(shot.atPx, shot.fromPx + shot.directionPx * 16.0) < 1e-9 && shot.flownPx == 16.0,
              "and 16 units along its way");
    CHECK_MSG(std::fabs(shot.rangePx - glm::distance(shot.destinyPx, shot.fromPx)) < 1e-9 &&
                  std::fabs(glm::length(shot.directionPx) - 1.0) < 1e-12,
              "to where the player stood, measured from the muzzle");
    std::printf("  level31b: shot %d fired, from (%.2f, %.2f) at (%.2f, %.2f)\n", shot.id, shot.fromPx.x, shot.fromPx.y,
                shot.destinyPx.x, shot.destinyPx.y);

    // It flies out its range and is spent there, leaving where for the layer.
    bool spent = false;
    for (int more = 0; more < 200 && !spent; ++more) {
        Hold(run, kBehindTheLine, 1);
        spent = !run.level.ghost.spentPx.empty();
    }
    CHECK_MSG(spent, "a shot that reaches its range ends, and says where");
}

// The barrel restocked, and where the port's pieces of it live: Fire's bomb of
// that name, and its body.
const Restock::Added* Barrel(const Run& run, std::size_t index) {
    return index < run.level.restock.added.size() ? &run.level.restock.added[index] : nullptr;
}

// Ask a restocked barrel to go off where it is put, as a fireball on it would
// (ETHBeginContactCallback_fireball #174-188): it blasts on its own next turn.
void SetOff(Run& run, const Restock::Added& barrel, const glm::dvec2& atPx) {
    if (!run.registry.valid(barrel.body)) return;
    PutAt(run.registry, barrel.body, atPx);
    for (Fire::Bomb& bomb : run.level.fire.bombs) {
        if (bomb.body == barrel.body) bomb.requested = true;
    }
}

// A BARREL, and nothing else (planning step 117). A fireball passes through it -
// the original's returns on a sensor - and a blast burns it when the grabber's
// sphere meets its box and the ray meets its body first.
void aFireballPassesThroughItAndOnlyABlastBurnsIt() {
    Run run;
    if (!Begin(kLevel, run)) return;
    ReachShooting(run);

    CHECK_EQ(run.level.ghost.hp, 3);
    CHECK_EQ(run.level.ghost.hits, 0);
    Hold(run, kBehindTheLine, 300);
    CHECK_MSG(run.level.ghost.hp == 3 && run.level.ghost.phase == Ghost::Phase::Shooting,
              "untouched, and still shooting, with nothing set off");

    // A fireball through where it hovers: not spent, and nothing burned.
    Turrets::Fireball ball;
    ball.name = "test#ghost";
    ball.atPx = run.level.ghost.atPx;
    ball.velocityPx = glm::dvec2(run.level.turrets.rules.speedPx, 0.0);
    ball.killsPlayer = false;
    run.level.turrets.fireballs.push_back(ball);
    Hold(run, kBehindTheLine, 2);
    const auto flying = [&run]() {
        for (const Turrets::Fireball& one : run.level.turrets.fireballs) {
            if (one.name == "test#ghost") return true;
        }
        return false;
    };
    CHECK_MSG(run.level.ghost.hp == 3 && run.level.ghost.hits == 0 && !run.level.ghost.burned &&
                  run.level.ghost.phase == Ghost::Phase::Shooting,
              "a fireball through the ghost burns nothing: it is a sensor");
    CHECK_MSG(flying(), "and the fireball flies on through it");
    std::erase_if(run.level.turrets.fireballs, [](const Turrets::Fireball& one) { return one.name == "test#ghost"; });

    // explode's reach, measured from the ghost's own point: the grabber's 80 against
    // its 128 x 139 box, then the ray to its body, 38.4 x 41.7 about (1.5, -2.1).
    const Ghost::State& ghost = run.level.ghost;
    const double r = run.level.fire.rules.blastPx;
    const glm::dvec2 at = ghost.atPx;
    CHECK_MSG(r == 80.0, "the grabber's radius, fire.json blast");
    CHECK_MSG(ghost.BlastReaches(run.registry, at + glm::dvec2(64.0 + 79.0, 0.0), r),
              "79 past the box's side: reached, with a clear line");
    CHECK_MSG(!ghost.BlastReaches(run.registry, at + glm::dvec2(64.0 + 81.0, 0.0), r), "81 past it: not reached");
    CHECK_MSG(!ghost.BlastReaches(run.registry, at + glm::dvec2(64.0 + 60.0, -(69.5 + 60.0)), r),
              "60 past two sides, 84.9 from the corner: a sphere against a box, not a box");
    CHECK_MSG(!ghost.BlastReaches(run.registry, at + glm::dvec2(0.0, 10.0), r),
              "inside its body: Box2D meets nothing from inside, so the ray's contact is not the ghost");
    CHECK_MSG(ghost.BlastReaches(run.registry, at + glm::dvec2(0.0, -30.0), r),
              "just outside its body, above: reached");
    {
        LevelBuilder::Options options;
        options.prismDirectory = run.data.prisms;
        const entt::entity wall = LevelBuilder::BuildStaticBox(run.registry, "test#wall", at + glm::dvec2(70.0, 0.0),
                                                              glm::dvec2(10.0, 100.0), options);
        CHECK_MSG(!ghost.BlastReaches(run.registry, at + glm::dvec2(110.0, 0.0), r),
                  "behind a solid wall: the ray's closest contact is the wall");
        CHECK_MSG(ghost.BlastReaches(run.registry, at + glm::dvec2(-60.0, -50.0), r),
                  "above the double block's end, on the open side: reached");
        run.registry.destroy(wall);
    }

    // Now the barrel it put back on its first beat, which fell into the pocket.
    const Restock::Added* first = Barrel(run, 0);
    CHECK_MSG(first != nullptr && run.registry.valid(first->body), "the first beat put a barrel back");
    if (first == nullptr) return;
    // Set off where it lies, far from the ghost: nothing.
    SetOff(run, *first, glm::dvec2(190.0, 205.0));
    Hold(run, kBehindTheLine, 3);
    CHECK_MSG(!run.registry.valid(first->body), "the barrel went off");
    CHECK_MSG(run.level.ghost.hp == 3 && run.level.ghost.phase == Ghost::Phase::Shooting,
              "in the pocket, 300 u from it, its blast reaches no ghost");
    // The next beat puts another back; set off on the floor past the ghost, outside
    // F15's 128 x 139 box and within the grabber's reach (the route's spot).
    for (int tick = 0; tick < 200 && run.level.restock.added.size() < 2; ++tick) Hold(run, kBehindTheLine, 1);
    const Restock::Added* second = Barrel(run, 1);
    CHECK_MSG(second != nullptr, "the next beat put another back");
    if (second == nullptr) return;
    Hold(run, kBehindTheLine, 40);
    const glm::dvec2 spot(590.0, 205.0);
    SetOff(run, *second, spot);
    // Asked, it goes off on the next tick, and the ghost reads the burn on that
    // same tick (its Tick runs after the blast test in AfterStep).
    Game::Tick(run.data, run.registry, run.level, 0.0f, kStep);
    CHECK_MSG(run.level.fire.blasts >= 2 && !run.registry.valid(second->body), "the barrel went off");
    CHECK_MSG(run.level.ghost.hp == 2 && run.level.ghost.hits == 1 && run.level.ghost.phase == Ghost::Phase::Damage,
              "a barrel's blast at (590, 205) burns it: hp " + std::to_string(run.level.ghost.hp));
    CHECK_MSG(!run.level.ghost.burned, "healBurn: the flag is cleared rather than left set");
}

// ins 594-630: on every SHOOTING beat, whichever side of the red line the player is,
// a barrel at barrel_spawn when none by the name barrel_bomb stands - and never on
// any other arm's update.
void itPutsItsBarrelBackOnEveryBeat() {
    Run run;
    if (!Begin(kLevel, run)) return;
    BurnTheKnight(run);
    Hold(run, kBehindTheLine, 200);
    CHECK_MSG(run.level.ghost.phase == Ghost::Phase::Appearing && run.level.restock.added.empty(),
              "none while it flies in");
    // The fire diamond gone, as the gutter takes it, so the beat puts one back too.
    for (Diamonds::Diamond& diamond : run.level.diamonds.diamonds) {
        if (diamond.fire) diamond.gone = true;
    }
    int ticks = 0;
    while (ticks < 100 && run.level.restock.added.empty()) {
        Hold(run, kBehindTheLine, 1);
        ++ticks;
    }
    CHECK_MSG(run.level.ghost.phase == Ghost::Phase::Shooting && run.level.restock.added.size() == 1,
              "the first beat of SHOOTING puts one back");
    const Restock::Added* first = Barrel(run, 0);
    if (first == nullptr) return;
    CHECK_MSG(first->name == "barrel_bomb_low_emissive_ent#1" && first->alias == "barrel_bomb" &&
                  first->templateFromPort && first->atPx == glm::dvec2(190.0, 110.0),
              "barrel_bomb_low_emissive.ent, answering to barrel_bomb, at barrel_spawn: " + first->name);
    CHECK_MSG(run.level.ghost.shotsFired == 0, "with the player behind the red line: the line gates the shot alone");
    CHECK_MSG(run.level.restock.refusals == 0, "and nothing refused");
    bool diamondBack = false;
    for (const Diamonds::Diamond& diamond : run.level.diamonds.diamonds) {
        if (diamond.fire && !diamond.gone && diamond.atPx == run.level.ghost.diamondSpawnPx) diamondBack = true;
    }
    CHECK_MSG(diamondBack, "and the fire diamond is back at its spawn on the same beat");
    CHECK_MSG(run.registry.all_of<Supersonic::BoxColliderComponent>(first->body), "a box");
    if (run.registry.all_of<Supersonic::BoxColliderComponent>(first->body)) {
        const glm::vec3 size = run.registry.get<Supersonic::BoxColliderComponent>(first->body).size;
        CHECK_MSG(std::fabs(size.x - Units::ToMetres(28.6)) < 1e-6f && std::fabs(size.y - Units::ToMetres(37.7)) < 1e-6f,
                  "of 28.6 x 37.7");
    }
    bool bomb = false;
    for (const Fire::Bomb& one : run.level.fire.bombs) bomb = bomb || (one.body == first->body && one.name == first->name);
    const bool traveller = std::find(run.level.portals.travellers.begin(), run.level.portals.travellers.end(),
                                     first->body) != run.level.portals.travellers.end();
    CHECK_MSG(bomb && traveller, "Fire's bomb and a portals traveller, as a placed barrel is");

    // It falls into the pocket and rests on the floor there.
    Hold(run, kBehindTheLine, 60);
    const glm::dvec2 rest = Units::ToPixels(run.registry.get<TransformComponent>(first->body).position);
    CHECK_MSG(std::fabs(rest.x - 190.0) < 1.0 && std::fabs(rest.y - (224.0 - 37.7 / 2.0)) < 1.0,
              "at rest on the pocket's floor: (" + std::to_string(rest.x) + ", " + std::to_string(rest.y) + ")");
    std::printf("  level31b: the ghost's barrel back at (190, 110) on SHOOTING's first beat, %d ticks after the knight's "
                "death, at rest at (%.2f, %.2f)\n",
                201 + ticks, rest.x, rest.y);

    // While it stands, the beats put none back.
    Hold(run, kBehindTheLine, 400);
    CHECK_MSG(run.level.restock.added.size() == 1, "none while it stands, two beats on");
    // Gone, the next beat does.
    SetOff(run, *first, rest);
    Hold(run, kBehindTheLine, 3);
    CHECK_MSG(!run.registry.valid(first->body), "gone");
    for (int tick = 0; tick < 200 && run.level.restock.added.size() < 2; ++tick) Hold(run, kBehindTheLine, 1);
    CHECK_MSG(run.level.restock.added.size() == 2 && run.level.restock.added.back().name == "barrel_bomb_low_emissive_ent#2",
              "the next beat puts #2 back");

    // Hurt, and raging: no beat, so no barrel, whatever is gone.
    run.level.ghost.burned = true;
    Hold(run, kBehindTheLine, 2);
    if (const Restock::Added* second = Barrel(run, 1)) SetOff(run, *second, glm::dvec2(190.0, 205.0));
    Hold(run, kBehindTheLine, 400);
    CHECK_MSG(run.level.ghost.phase == Ghost::Phase::Rage && run.level.restock.added.size() == 2,
              "through DAMAGE and into RAGE_MODE, none put back");
}

// RAGE_MODE (#1013-1083): on EVERY update, while the player stands with hp above 0,
// a fire diamond back at its spawn if none is left.
void rageRestocksTheFireDiamondEveryUpdate() {
    Run run;
    if (!Begin(kLevel, run)) return;
    ReachShooting(run);
    run.level.ghost.burned = true;
    Hold(run, kBehindTheLine, 2);
    Hold(run, kBehindTheLine, 250);
    CHECK_MSG(run.level.ghost.phase == Ghost::Phase::Rage, "raging");
    const auto fireDiamondGone = [&run]() {
        bool gone = true;
        for (const Diamonds::Diamond& diamond : run.level.diamonds.diamonds) {
            if (diamond.fire && !diamond.gone) gone = false;
        }
        return gone;
    };
    for (Diamonds::Diamond& diamond : run.level.diamonds.diamonds) {
        if (diamond.fire) diamond.gone = true;
    }
    Hold(run, kBehindTheLine, 1);
    CHECK_MSG(!fireDiamondGone(), "gone in rage, it is back on the next update");
    // Not for a dead player: the arm asks for one with hp above 0.
    for (Diamonds::Diamond& diamond : run.level.diamonds.diamonds) {
        if (diamond.fire) diamond.gone = true;
    }
    run.level.hazards.playerDied = true;
    Hold(run, kBehindTheLine, 30);
    CHECK_MSG(run.level.ghost.phase == Ghost::Phase::Rage && fireDiamondGone(), "and not for a dead player");
}

// Hurt, it calls an escort and waits for the player to kill it - and looks the part.
void hurtItCallsAnEscortAndWaitsForIt() {
    Run run;
    if (!Begin(kLevel, run)) return;
    ReachShooting(run);

    const std::size_t before = run.level.minions.minions.size();
    run.level.ghost.burned = true;
    Hold(run, kBehindTheLine, 2);
    // DAMAGE: alpha 0.5 and bounce(1 -> 1.1, 500) on its one blinkElapsedTime.
    const Ghost::State& ghost = run.level.ghost;
    CHECK_MSG(ghost.phase == Ghost::Phase::Damage && ghost.look.alpha == 0.5 && ghost.blinkMs == kStepMs &&
                  std::fabs(ghost.look.scale - ghost.rules.damageBounce.ScaleAt(kStepMs)) < 1e-12 &&
                  ghost.look.scale > 1.0,
              "hurt: alpha 0.5, bouncing from 1 toward 1.1: " + std::to_string(ghost.look.scale));
    Hold(run, kBehindTheLine, 250); // through the 4000 ms of damage, into rage

    CHECK_MSG(run.level.ghost.summons == 1, "rage calls one escort");
    CHECK_MSG(run.level.minions.minions.size() == before + 1, "and Minions owns the body it built");
    // Marked as called, not spawned, and a plain minion.ent (ETHCallback_ghost ins
    // 1137): the picture stands it at its anchor's depth and draws it lit, where
    // the level's own marker named ghost_minion.ent, the black knight (step 76).
    if (run.level.minions.minions.size() == before + 1) {
        const Minions::Minion& called = run.level.minions.minions.back();
        CHECK_MSG(called.summoned && called.entity == "minion.ent", "the escort is summoned, and a minion.ent");
    }
    CHECK_MSG(run.level.ghost.phase == Ghost::Phase::Rage, "and it waits there");
    // RAGE_MODE: cell 3 at alpha 0.5, at the scale DAMAGE's bounce last left.
    CHECK_MSG(ghost.look.frame == 3 && ghost.look.alpha == 0.5, "raging: cell 3, alpha 0.5");

    // It keeps waiting while that escort stands - this is the part that would
    // deadlock if the level's own patroller were counted as one of its own.
    Hold(run, kBehindTheLine, 120);
    CHECK_MSG(run.level.ghost.phase == Ghost::Phase::Rage, "still waiting two seconds later");
    CHECK_MSG(run.level.ghost.summons == 1, "and it does not call a second");

    entt::entity escort = entt::null;
    for (const Minions::Minion& minion : run.level.minions.minions) {
        if (minion.gone || minion.body == entt::null) continue;
        if (minion.name.rfind(run.level.ghost.name + "#", 0) != 0) continue;
        escort = minion.body;
        break;
    }
    CHECK_MSG(escort != entt::null, "the escort is findable by the boss's own name");
    if (escort == entt::null) return;

    // A fireball on the escort burns it (minion.ent is burnable too).
    const glm::dvec2 escortPx = Units::ToPixels(run.registry.get<TransformComponent>(escort).position);
    Turrets::Fireball ball;
    ball.name = "test#escort";
    ball.atPx = escortPx;
    ball.killsPlayer = false;
    run.level.turrets.fireballs.push_back(ball);
    Hold(run, kBehindTheLine, 30);
    CHECK_MSG(!run.registry.valid(escort) && run.level.minions.burned == 2, "the escort burned");
    CHECK_MSG(run.level.ghost.phase == Ghost::Phase::Shooting && ghost.look.alpha == 1.0 && ghost.look.frame == 0,
              "killed, and the boss comes back out at alpha 1, on cell 0");
}

// Three burns, and the key that is the level's only one.
void threeBurnsAndItDropsAYellowKey() {
    Run run;
    if (!Begin(kLevel, run)) return;
    ReachShooting(run);

    const std::size_t keysBefore = run.level.keys.keys.size();
    CHECK_MSG(keysBefore == 0, "level31b ships no key at all: the fight is the lock");
    CHECK_MSG(!run.level.keys.keyholes.empty(), "but it does ship the keyhole");

    BurnAndLetItRecover(run);
    CHECK_EQ(run.level.ghost.hp, 2);
    BurnAndLetItRecover(run);
    CHECK_EQ(run.level.ghost.hp, 1);

    // The third takes it to nothing, and DAMAGE goes straight to DEATH.
    run.level.ghost.burned = true;
    Hold(run, kBehindTheLine, 4);
    CHECK_EQ(run.level.ghost.hp, 0);
    CHECK_MSG(run.level.ghost.phase == Ghost::Phase::Death, "hp gone, it dies rather than raging again");
    CHECK_MSG(run.level.keys.keys.size() == keysBefore, "and it has not dropped the key yet");
    // DEATH: alpha 1 and bounce(1.1 -> 1.25, 400), held where it is.
    const glm::dvec2 diedAt = run.level.ghost.atPx;
    CHECK_MSG(run.level.ghost.look.alpha == 1.0 && run.level.ghost.look.scale >= 1.1 && run.level.ghost.look.scale <= 1.25,
              "dying: alpha 1, bouncing 1.1 to 1.25: " + std::to_string(run.level.ghost.look.scale));

    // 3550 ms of bursting, and then the key.
    Hold(run, kBehindTheLine, 230);
    CHECK_MSG(run.level.ghost.gone, "the ghost is gone");
    CHECK_MSG(run.level.ghost.atPx == diedAt, "and it never moved while it died");
    CHECK_MSG(run.level.keys.keys.size() == keysBefore + 1, "and a key is where it was");
    if (run.level.keys.keys.size() == keysBefore + 1) {
        const Keys::Key& dropped = run.level.keys.keys.back();
        CHECK_MSG(dropped.colour == "yellow", "yellow, from key.ent's own data; got '" + dropped.colour + "'");
        CHECK_MSG(dropped.name == run.level.ghost.keyName && dropped.name == "ghost_pos_1365#key",
                  "it answers to the ghost's name: " + dropped.name);
        CHECK_MSG(glm::distance(dropped.atPx, diedAt) < 1e-9, "dropped where it died, its hover's point");
    }
    CHECK_EQ(run.level.ghost.hits, 3);
    std::printf("  level31b: the ghost took %d burn(s) and dropped a %s key\n", run.level.ghost.hits,
                run.level.keys.keys.empty() ? "no" : run.level.keys.keys.back().colour.c_str());
}

// One boss, one level. The others carry a boss_spawn the port does not play, and
// Ghost::Plays answering per NODE is what keeps that honest.
void noOtherLevelHasOne() {
    for (const char* name : {"level31", "level31a", "level30b", "level29b"}) {
        Run run;
        if (!Begin(name, run)) continue;
        CHECK_MSG(!run.level.ghost.present, std::string(name) + " has no ghost");
        CHECK_MSG(run.level.ghost.patrol.empty(), std::string(name) + " resolves no escort patrol");
        CHECK_MSG(run.level.ghost.deletesNode.empty(), std::string(name) + " has nothing for a knight to delete");
    }
}

void runTests() {
    theRulesAndWhatLevelThirtyOneBHolds();
    nothingBeforeTheKnightDies();
    aFireballOnTheKnightAddsTheGhost();
    aKillerFloorAddsItToo();
    itFliesInOnFiveWaypoints();
    itHoversAboutGhostPos();
    theRedLineGatesItsShooting();
    aFireballPassesThroughItAndOnlyABlastBurnsIt();
    itPutsItsBarrelBackOnEveryBeat();
    rageRestocksTheFireDiamondEveryUpdate();
    hurtItCallsAnEscortAndWaitsForIt();
    threeBurnsAndItDropsAYellowKey();
    noOtherLevelHasOne();
}

} // namespace

MP_SUITE_MAIN("test_mp_ghost") {
    std::error_code ec;
    if (!std::filesystem::is_directory(kLevels, ec) ||
        !std::filesystem::is_regular_file(kData + "/entity_roles.json", ec)) {
        std::printf("test_mp_ghost SKIPPED - needs the converted levels at %s and the remake's data at %s\n",
                    kLevels.c_str(), kData.c_str());
        return 77;
    }
    runTests();
    return ::test::summary("test_mp_ghost", 130);
}
