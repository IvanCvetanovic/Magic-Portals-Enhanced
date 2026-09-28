// Chapter 4's boss: the dark dragon of level 4-32, and the last level of the game.
//
// WHAT THIS SUITE IS REALLY FOR. Every other boss in the game can be reached by
// walking into its level; this one has to be SUMMONED, and it can only be hurt by
// a bomb it lights itself. So the things worth pinning are the chain and the loop:
//
//   - it does NOT appear until the torch is lit, and then only after 3 s + 4 s;
//   - arriving takes `breakable_wall` away, and that wall is a real body;
//   - it is hurt ONLY by a blast - no fireball, no shot, nothing else touches it;
//   - each wound takes the player's portals away and shortens its fire interval;
//   - and at hp 0 it drops the only key the level has, in the level's own colour.
//
// AND, since the visuals plan's 12.1 (planning step 99), the callback's own order:
// the seven eased waypoints it flies in on, the jaw that opens on the fourth, the
// angle the original's Follower lags toward the player, the neck and the hover that
// move the point it shoots from, the eight flash legs of a wound (R20) and the white
// of its death. These moved gameplay - where it stands, where a shot leaves from,
// when it first fires and when a wound ends - so the counts below are re-pinned
// from the decode, not carried over.
//
// The inventory (test_mp_start) counts roles, and `boss_spawn` is one role for
// four different bosses - so it can say level31c plays without a single one of
// those being true. Only this suite can tell.
//
// Reads the converted levels from outside this repository, and skips, saying
// where it looked, when they are absent.

#include "TestHarness.hpp"

#include "core/Components.hpp"
#include "sim/DarkDragon.hpp"
#include "sim/Ease.hpp"
#include "sim/Game.hpp"
#include "sim/Roles.hpp"
#include "sim/Tscn.hpp"
#include "sim/Units.hpp"

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

using namespace MagicPortals;
using Supersonic::TransformComponent;

namespace {

const std::string kLevels = MAGICPORTALS_LEVELS_DIR;
const std::string kData = MAGICPORTALS_DATA_DIR;
const std::string kPortData = MAGICPORTALS_PORT_DATA_DIR;
constexpr float kStep = 1.0f / 60.0f;
constexpr double kPi = 3.14159265358979323846;

const std::string kLevel = "level31c";
// What level31c.tscn itself carries.
constexpr double kSpawnX = -144.0;
constexpr double kSpawnY = 210.0;
constexpr double kShoutX = -4.0;
constexpr double kShoutY = 140.0;
constexpr double kFightX = 16.0;
constexpr double kFightY = 56.0;
constexpr double kKeyX = 78.0;
constexpr double kKeyY = 208.0;
constexpr double kKeyZ = 6.0;
// Where the player can stand in the dragon's sight once the fight is on: the floor
// right of block 2256, where F16's player shot the torch from (F16.md section 1).
const glm::dvec2 kFloorPx(241.0, 200.0);

// The schedule the decode gives on the port's tick, counted from the summon's tick
// (0): the init on the next (1), each waypoint on the update FrameTimer::set moves
// it on, FIGHTING on the last. out/parity/visuals/12.1/work/model_121.py.
constexpr int kLegStarts[] = {1, 151, 152, 211, 271, 313, 445};
constexpr int kFightingTick = 445;

bool Open(const std::string& name, Game::Data& data, entt::registry& registry, Game::Level& level) {
    const std::filesystem::path prisms = std::filesystem::temp_directory_path() / "supersonic-test-mp-darkdragon";
    std::string error;
    if (!Game::LoadData(kLevels + "/" + name + ".tscn", kData, prisms, data, error) ||
        !Game::Start(data, registry, level, error)) {
        CHECK_MSG(false, name + ": " + error);
        return false;
    }
    return true;
}

// Ticks until `done`, or for at most `ticks`. Returns how many it took.
template <typename Fn>
int RunUntil(Game::Data& data, entt::registry& registry, Game::Level& level, int ticks, Fn done) {
    for (int tick = 0; tick < ticks; ++tick) {
        if (done()) return tick;
        Game::Tick(data, registry, level, 0.0f, kStep);
    }
    return ticks;
}

std::string Pt(const glm::dvec2& p) { return "(" + std::to_string(p.x) + ", " + std::to_string(p.y) + ")"; }

void PutPlayerAt(entt::registry& registry, entt::entity player, const glm::dvec2& atPx) {
    auto& transform = registry.get<TransformComponent>(player);
    const glm::vec3 world = Units::ToWorld(atPx.x, atPx.y);
    transform.position = glm::vec3(world.x, world.y, transform.position.z);
    if (registry.all_of<Supersonic::RigidBodyComponent>(player)) {
        registry.get<Supersonic::RigidBodyComponent>(player).velocity = glm::vec3(0.0f);
    }
}

glm::dvec2 PlayerPx(const entt::registry& registry, entt::entity player) {
    return Units::ToPixels(registry.get<TransformComponent>(player).position);
}

void TheRulesRead() {
    DarkDragon::Rules rules;
    std::string error;
    const bool ok = DarkDragon::LoadRules(kPortData + "/darkdragon.json", rules, error);
    CHECK_MSG(ok, error);
    if (!ok) return;

    CHECK(rules.spawnName == "dark_dragon_spawn");
    CHECK(rules.entityName == "dark_dragon.ent");
    CHECK(rules.wallName == "breakable_wall");
    CHECK(rules.keyColour == "yellow");
    CHECK(rules.keyEntity == "key.ent");
    CHECK(::test::nearly(static_cast<float>(rules.armMs), 3000.0f));
    CHECK(::test::nearly(static_cast<float>(rules.summonMs), 4000.0f));
    CHECK_EQ(rules.maxHp, 3);
    CHECK(::test::nearly(static_cast<float>(rules.radiusPx), 110.0f));
    CHECK(::test::nearly(static_cast<float>(rules.fireIntervalMs), 4200.0f));
    CHECK(::test::nearly(static_cast<float>(rules.fireIntervalStepMs), 600.0f));
    CHECK(::test::nearly(static_cast<float>(rules.muzzlePx), 95.0f));
    CHECK(::test::nearly(static_cast<float>(rules.fireballParticleScale), 2.0f));

    // Seven waypoints carrying it in from off the left edge, 7400 ms in all, at the
    // decoded angles (ins 178-341) - and the jaw opening on the fourth.
    CHECK_EQ(rules.appear.size(), std::size_t{7});
    double appearMs = 0.0;
    for (const DarkDragon::AppearWaypoint& one : rules.appear) appearMs += one.ms;
    CHECK(::test::nearly(static_cast<float>(appearMs), 7400.0f));
    const double angles[] = {14.0, 0.0, 0.0, 10.0, 28.0, 27.0, -12.0};
    const Ease::Filter filters[] = {Ease::Filter::SmoothBothSides, Ease::Filter::SmoothBothSides,
                                    Ease::Filter::SmoothBeginning, Ease::Filter::SmoothEnd,
                                    Ease::Filter::SmoothBeginning, Ease::Filter::SmoothEnd,
                                    Ease::Filter::SmoothBothSides};
    bool asDecoded = rules.appear.size() == 7;
    for (std::size_t i = 0; i < rules.appear.size() && i < 7; ++i) {
        asDecoded = asDecoded && rules.appear[i].angleDeg == angles[i] && rules.appear[i].filter == filters[i];
    }
    CHECK_MSG(asDecoded, "the seven waypoints' angles and filters, as the init block pushes them");
    CHECK(rules.appear.front().at == DarkDragon::WaypointAt::Here);
    CHECK(rules.appear.back().at == DarkDragon::WaypointAt::Fighting);
    CHECK_EQ(rules.mouthLeg, 3);

    // THE OWNER'S STANDING RULING R20: EIGHT flash legs of 300 ms after the first
    // 1000, 3400 ms in all, where the port had six and 2800.
    CHECK_EQ(rules.damageFlashes, 8);
    CHECK(::test::nearly(static_cast<float>(rules.damageFirstMs + rules.damageFlashes * rules.damageFlashMs),
                         3400.0f));
    CHECK(rules.damageRed == glm::dvec3(1.0, 0.0, 0.0));
    CHECK(rules.dyingEmissive == glm::dvec3(1.0));
    CHECK(::test::nearly(static_cast<float>(rules.damageAngleAddDeg), 27.0f));
    CHECK(::test::nearly(static_cast<float>(rules.dyingAngleAddDeg), 30.0f));
    // The Follower (ins 343-356) and computeDarkDragonAngle's PIb x 1.2.
    CHECK_EQ(rules.followerStrideMs, 800u);
    CHECK_EQ(rules.followerRateMs, 150u);
    CHECK(::test::nearly(static_cast<float>(rules.aimOffsetPib), 1.2f));
    // Its depth: 0, from its second update on (darkdragon.json look).
    CHECK(rules.zUnits == 0.0);
    std::printf("  summoned %.0f + %.0f ms after the torch, arrives over %.0f ms, hp %d, a wound %.0f ms\n",
                rules.armMs, rules.summonMs, appearMs, rules.maxHp,
                rules.damageFirstMs + rules.damageFlashes * rules.damageFlashMs);
}

// Refused, each for a reason a data file could actually have: the real file with
// one field swapped per case.
void TheRulesAreRefusedWhenTheyDoNotMakeABoss() {
    const std::filesystem::path scratch =
        std::filesystem::temp_directory_path() / "supersonic-test-mp-darkdragon-rules";
    std::error_code ec;
    std::filesystem::create_directories(scratch, ec);

    std::ifstream in(kPortData + "/darkdragon.json", std::ios::binary);
    std::ostringstream read;
    read << in.rdbuf();
    const std::string real = read.str();
    const auto swapped = [&real](const std::string& from, const std::string& to) {
        std::string text = real;
        const std::size_t at = text.find(from);
        if (at == std::string::npos) return std::string();
        text.replace(at, from.size(), to);
        return text;
    };

    const struct Case {
        const char* name;
        std::string text;
    } cases[] = {
        {"a boss with no hit points", swapped("\"max_hp\": 3", "\"max_hp\": 0")},
        {"a boss no blast can reach", swapped("\"radius_px\": 110.0", "\"radius_px\": 0.0")},
        // 3 hp and a 2100 step would leave the last wound a zero interval, which
        // is a boss that fires every tick.
        {"a step that outruns its own interval",
         swapped("\"fire_interval_step_ms\": 600.0", "\"fire_interval_step_ms\": 2100.0")},
        {"six waypoints instead of seven",
         swapped(",\n      { \"at\": \"fighting\", \"ms\": 0.0, \"filter\": \"smooth_both_sides\", \"angle_deg\": -12.0 }",
                 "")},
        {"a wound with no flash", swapped("\"flashes\": 8", "\"flashes\": 0")},
        {"a filter the original has not got",
         swapped("\"filter\": \"smooth_end\", \"angle_deg\": 10.0", "\"filter\": \"smooth_middle\", \"angle_deg\": 10.0")},
        {"a waypoint at a place it has not got",
         swapped("\"at\": \"shout_moved\", \"ms\": 700.0", "\"at\": \"shout_below\", \"ms\": 700.0")},
    };

    int refused = 0;
    for (const Case& one : cases) {
        CHECK_MSG(!one.text.empty(), std::string("the real file carries what this case swaps: ") + one.name);
        if (one.text.empty()) continue;
        const std::filesystem::path file = scratch / "darkdragon.json";
        {
            std::ofstream out(file, std::ios::binary);
            out << one.text;
        }
        DarkDragon::Rules rules;
        std::string error;
        const bool ok = DarkDragon::LoadRules(file.string(), rules, error);
        CHECK_MSG(!ok, std::string("refused: ") + one.name);
        if (!ok) ++refused;
    }
    CHECK_EQ(refused, 7);
}

// sim/Ease, the listing's three filters (bytes 313899..314142).
void TheEasingFiltersAreTheScripts() {
    // smoothBeginning at 0, 0.5 and 1: 1 - sin((1 - v) x PIb).
    CHECK(::test::nearly(static_cast<float>(Ease::SmoothBeginning(0.0)), 0.0f, 1e-6f));
    CHECK(::test::nearly(static_cast<float>(Ease::SmoothBeginning(0.5)), 0.29289322f, 1e-6f));
    CHECK(::test::nearly(static_cast<float>(Ease::SmoothBeginning(1.0)), 1.0f, 1e-6f));
    // smoothEnd: sin(v x PIb).
    CHECK(::test::nearly(static_cast<float>(Ease::SmoothEnd(0.0)), 0.0f, 1e-6f));
    CHECK(::test::nearly(static_cast<float>(Ease::SmoothEnd(0.5)), 0.70710678f, 1e-6f));
    CHECK(::test::nearly(static_cast<float>(Ease::SmoothEnd(1.0)), 1.0f, 1e-6f));
    // smoothBothSides: smoothBeginning(smoothEnd(v)).
    CHECK(::test::nearly(static_cast<float>(Ease::SmoothBothSides(0.5)),
                         static_cast<float>(1.0 - std::sin((1.0 - std::sin(0.25 * kPi)) * kPi / 2.0)), 1e-6f));
    CHECK(::test::nearly(static_cast<float>(Ease::SmoothBothSides(1.0)), 1.0f, 1e-6f));
    CHECK(Ease::Apply(Ease::Filter::Linear, 0.3) == 0.3);
    Ease::Filter parsed = Ease::Filter::Linear;
    CHECK(Ease::ParseFilter("smooth_beginning", parsed) && parsed == Ease::Filter::SmoothBeginning);
    CHECK(!Ease::ParseFilter("smoothBeginning", parsed));
    // THE JAW'S THRESHOLDS on a 1000 ms waypoint: clamp(uint(smoothBeginning(b) x 14))
    // reaches 1, 2 and 3 at these fractions of it.
    const double opens[] = {0.242, 0.345, 0.425};
    for (int cell = 1; cell <= 3; ++cell) {
        const double at = opens[cell - 1];
        CHECK_MSG(static_cast<int>(Ease::SmoothBeginning(at + 0.001) * 14.0) >= cell &&
                      static_cast<int>(Ease::SmoothBeginning(at - 0.001) * 14.0) < cell,
                  "the jaw reaches cell " + std::to_string(cell) + " at " + std::to_string(at) + " of its waypoint");
    }
}

// DarkDragon::Path is the WaypointManager as it plays: one waypoint an update at
// most, the first update restarting it, a 0 ms waypoint lasting one update, the
// bias raw and the filter this waypoint's own.
void ThePathMovesOnOneWaypointAnUpdate() {
    DarkDragon::Path path;
    path.points.push_back({glm::dvec2(0.0), 100.0, glm::dvec3(0.0), Ease::Filter::SmoothEnd, 0.0});
    path.points.push_back({glm::dvec2(10.0, 0.0), 0.0, glm::dvec3(1.0), Ease::Filter::Linear, 10.0});
    path.points.push_back({glm::dvec2(10.0, 20.0), 50.0, glm::dvec3(0.5), Ease::Filter::Linear, 20.0});
    path.points.push_back({glm::dvec2(0.0, 20.0), 0.0, glm::dvec3(0.0), Ease::Filter::Linear, 30.0});

    path.Update(40.0); // the first call restarts it at waypoint 0 with no time
    CHECK_EQ(path.frame, 0);
    CHECK(path.timeMs == 0.0 && path.Bias() == 0.0);
    path.Update(40.0);
    CHECK_EQ(path.frame, 0);
    // 40 of 100 ms, eased by waypoint 0's smoothEnd - the position, the angle and the colour alike.
    const DarkDragon::Path::Point half = path.Current();
    const double eased = Ease::SmoothEnd(0.4);
    CHECK(::test::nearly(static_cast<float>(half.atPx.x), static_cast<float>(10.0 * eased), 1e-5f));
    CHECK(::test::nearly(static_cast<float>(half.angleDeg), static_cast<float>(10.0 * eased), 1e-5f));
    CHECK(::test::nearly(static_cast<float>(half.colour.r), static_cast<float>(eased), 1e-5f));
    path.Update(40.0);
    path.Update(40.0); // 120 >= 100: ONE waypoint on, 20 ms carried
    CHECK_EQ(path.frame, 1);
    CHECK(::test::nearly(static_cast<float>(path.timeMs), 20.0f));
    path.Update(40.0); // a 0 ms waypoint lasts one update
    CHECK_EQ(path.frame, 2);
    path.Update(40.0);
    path.Update(40.0); // 100 >= 50: to the last, which holds with no time
    CHECK_EQ(path.frame, 3);
    CHECK(path.IsLastFrame());
    path.Update(40.0);
    CHECK_EQ(path.frame, 3);
    CHECK(path.Current().atPx == glm::dvec2(0.0, 20.0) && path.Current().angleDeg == 30.0);
}

void ItAnswersForItsOwnNodeAndNoOthers() {
    DarkDragon::Rules rules;
    std::string error;
    if (!DarkDragon::LoadRules(kPortData + "/darkdragon.json", rules, error)) return;

    const struct Case {
        const char* level;
        int expected;
    } cases[] = {{"level31c", 1}, {"level31", 0}, {"level31a", 0}, {"level31b", 0}};

    for (const Case& one : cases) {
        Tscn::Scene scene;
        Roles::Table roles;
        if (!Tscn::Load(kLevels + "/" + one.level + ".tscn", scene, error) ||
            !Roles::Load(kData + "/entity_roles.json", roles, error)) {
            CHECK_MSG(false, error);
            continue;
        }
        int claimed = 0;
        for (const Tscn::Node& node : scene.nodes) {
            if (node.parent != "." || Roles::RoleOf(roles, node) != Roles::kBossSpawn) continue;
            if (DarkDragon::Plays(rules, node)) ++claimed;
        }
        CHECK_MSG(claimed == one.expected,
                  std::string(one.level) + " has " + std::to_string(claimed) + " node(s) this boss claims");
    }
    CHECK(!Roles::IsPorted(Roles::kBossSpawn));
}

void TheLevelPlacesItWhereTheFileSaysIt() {
    Game::Data data;
    entt::registry registry;
    Game::Level level;
    if (!Open(kLevel, data, registry, level)) return;

    CHECK(level.darkDragon.present);
    if (!level.darkDragon.present) return;
    CHECK(::test::nearly(static_cast<float>(level.darkDragon.spawnPx.x), static_cast<float>(kSpawnX)));
    CHECK(::test::nearly(static_cast<float>(level.darkDragon.spawnPx.y), static_cast<float>(kSpawnY)));
    CHECK(::test::nearly(static_cast<float>(level.darkDragon.shoutPx.x), static_cast<float>(kShoutX)));
    CHECK(::test::nearly(static_cast<float>(level.darkDragon.shoutPx.y), static_cast<float>(kShoutY)));
    CHECK(::test::nearly(static_cast<float>(level.darkDragon.fightingPx.x), static_cast<float>(kFightX)));
    CHECK(::test::nearly(static_cast<float>(level.darkDragon.fightingPx.y), static_cast<float>(kFightY)));
    CHECK(level.darkDragon.hasKeyPos);
    CHECK(::test::nearly(static_cast<float>(level.darkDragon.keyPosPx.x), static_cast<float>(kKeyX)));
    CHECK(::test::nearly(static_cast<float>(level.darkDragon.keyPosPx.y), static_cast<float>(kKeyY)));
    // The key it drops is added at key_pos's own vector3 (ins 1851-1878): its z 6.
    CHECK(::test::nearly(static_cast<float>(level.darkDragon.keyZUnits), static_cast<float>(kKeyZ)));
    CHECK(level.darkDragon.keyName == level.darkDragon.name + "#key");

    // The wall it breaks, resolved to the NODE Demolish holds it by, and that
    // node is a real breakable with a body.
    CHECK(!level.darkDragon.wallNode.empty());
    const Demolish::Breakable* wall = nullptr;
    for (const Demolish::Breakable& one : level.demolish.breakables) {
        if (one.name == level.darkDragon.wallNode) wall = &one;
    }
    CHECK_MSG(wall != nullptr, "breakable_wall is a Demolish::Breakable");
    if (wall != nullptr) {
        CHECK(!wall->broken);
        CHECK(registry.valid(wall->body));
    }

    // The level it is in: a torch to summon it, a bomb to hurt it, and a yellow
    // lock its key is the only opener of.
    CHECK(!level.torch.lights.empty());
    CHECK(!level.fire.bombs.empty());
    CHECK(level.keys.keys.empty()); // the level places NO key: the fight is the lock
    CHECK(!level.keys.keyholes.empty());
    std::printf("  %s: %zu torch(es), %zu bomb(s), %zu keyhole(s) and no key of its own\n", kLevel.c_str(),
                level.torch.lights.size(), level.fire.bombs.size(), level.keys.keyholes.size());

    // And no other level in the game has one.
    for (const char* other : {"level31", "level31a", "level31b", "level30c"}) {
        Game::Data second;
        entt::registry secondRegistry;
        Game::Level secondLevel;
        if (!Open(other, second, secondRegistry, secondLevel)) continue;
        CHECK_MSG(!secondLevel.darkDragon.present, std::string(other) + " places no dark dragon");
    }
}

// THE CHAIN: it does not come until the player makes light.
void ItDoesNotComeUntilTheTorchIsLit() {
    Game::Data data;
    entt::registry registry;
    Game::Level level;
    if (!Open(kLevel, data, registry, level)) return;
    if (!level.darkDragon.present) return;

    // Thirty seconds with the torch unlit, and nothing arrives. This is the check
    // that would catch a boss that simply spawns on a timer.
    for (int tick = 0; tick < 1800; ++tick) Game::Tick(data, registry, level, 0.0f, kStep);
    CHECK(!level.darkDragon.armed);
    CHECK(!level.darkDragon.alive);
    CHECK(!level.darkDragon.summoned);

    // Light it - which is what a portal shot passing the torch does - and the
    // chain runs: 3000 ms to arm, then 4000 ms more.
    level.torch.lit = 1;
    const int armed = RunUntil(data, registry, level, 600, [&] { return level.darkDragon.armed; });
    CHECK(level.darkDragon.armed);
    CHECK(!level.darkDragon.alive);
    const int came = RunUntil(data, registry, level, 600, [&] { return level.darkDragon.alive; });
    CHECK(level.darkDragon.alive);
    CHECK_EQ(level.darkDragon.hp, level.darkDragon.rules.maxHp);
    std::printf("  armed after %.2f s of light, arrived %.2f s later\n", armed * kStep, came * kStep);
    CHECK(armed * kStep > 2.9f && armed * kStep < 3.2f);
    CHECK(came * kStep > 3.9f && came * kStep < 4.2f);

    // And arriving took the wall away - the body, not just a flag.
    bool broken = false;
    for (const Demolish::Breakable& one : level.demolish.breakables) {
        if (one.name != level.darkDragon.wallNode) continue;
        broken = one.broken;
        CHECK_MSG(one.body == entt::null, "the wall's body went with it");
    }
    CHECK_MSG(broken, "arriving broke breakable_wall");

    // It flies in from off the left edge and settles where `fighting` is: the
    // point its FIGHTING updates stand it at, from the tick FIGHTING begins, is
    // `fighting` itself; where it stands is that less the one update the hover
    // rides behind, plus the hover.
    RunUntil(data, registry, level, 900, [&] { return level.darkDragon.phase == DarkDragon::Phase::Fighting; });
    CHECK(level.darkDragon.phase == DarkDragon::Phase::Fighting);
    CHECK(level.darkDragon.originPx == glm::dvec2(kFightX, kFightY));
    CHECK_MSG(std::abs(level.darkDragon.atPx.x - kFightX) < 0.01 &&
                  std::abs(level.darkDragon.atPx.y - kFightY) <= level.darkDragon.rules.hoverStridePx + 0.01,
              "within the hover of `fighting`: " + Pt(level.darkDragon.atPx));
}

// Brings it in and leaves it fighting.
bool Summon(Game::Data& data, entt::registry& registry, Game::Level& level) {
    if (!level.darkDragon.present) return false;
    level.torch.lit = 1;
    RunUntil(data, registry, level, 1800,
             [&] { return level.darkDragon.phase == DarkDragon::Phase::Fighting && level.darkDragon.alive; });
    return level.darkDragon.alive && level.darkDragon.phase == DarkDragon::Phase::Fighting;
}

// Brings it to the tick the summon adds it, and says so.
bool Arrive(Game::Data& data, entt::registry& registry, Game::Level& level) {
    if (!level.darkDragon.present) return false;
    level.torch.lit = 1;
    RunUntil(data, registry, level, 1800, [&] { return level.darkDragon.alive; });
    return level.darkDragon.alive;
}

// THE FLY-IN (ins 178-682): seven eased waypoints, the jaw opening on the fourth,
// the hover riding one update behind the path, and FIGHTING on the last.
void ItComesInOnItsSevenWaypoints() {
    Game::Data data;
    entt::registry registry;
    Game::Level level;
    if (!Open(kLevel, data, registry, level)) return;
    if (!Arrive(data, registry, level)) return;
    const DarkDragon::State& dragon = level.darkDragon;
    const DarkDragon::Rules& rules = dragon.rules;

    // The summon's own tick: dark_dragon.ent as AddScaledEntity makes it, before its
    // first update - its startFrame, angle 0, its own emissive, its system playing.
    CHECK(!dragon.initialised && dragon.atPx == dragon.spawnPx);
    CHECK_EQ(dragon.look.frame, rules.startFrame);
    CHECK(dragon.look.angleDeg == 0.0 && dragon.look.emissive == rules.emissive && !dragon.look.systemKilled);

    std::vector<int> legStarts;
    std::vector<int> cells;
    int restarts = 0;
    int restartsOffTheWidest = 0;
    double worstHover = 0.0;
    double worstBehind = 0.0;
    int fighting = 0;
    glm::dvec2 origin = dragon.originPx;
    int lastLeg = -1;
    for (int tick = 1; tick <= kFightingTick + 1 && fighting == 0; ++tick) {
        Game::Tick(data, registry, level, 0.0f, kStep);
        if (tick == 1) {
            // THE INIT: SetFrame(0), KillParticleSystem(0), hp 3, the Follower made.
            CHECK(dragon.initialised && dragon.look.systemKilled);
            CHECK_EQ(dragon.hp, rules.maxHp);
            CHECK(dragon.follower.strideMs == rules.followerStrideMs && dragon.follower.b == glm::dvec2(0.0));
        }
        // linearMotion: where it stands is the LAST update's originalPos plus the
        // swing, 7 cos(angle), vertical.
        const glm::dvec2 swing = dragon.atPx - origin;
        worstHover = std::max(worstHover, std::abs(swing.x) + std::abs(swing.y - rules.hoverStridePx *
                                                                                      std::cos(dragon.hover.angle)));
        // And this update's originalPos is the path's eased point.
        worstBehind = std::max(worstBehind, glm::distance(dragon.originPx, dragon.path.Current().atPx));
        origin = dragon.originPx;
        if (dragon.path.frame != lastLeg) {
            legStarts.push_back(tick);
            lastLeg = dragon.path.frame;
        }
        cells.push_back(dragon.look.frame);
        if (dragon.look.systemRestarted) {
            ++restarts;
            if (dragon.look.frame != rules.lastFrame) ++restartsOffTheWidest;
        }
        // THE ANGLE is the waypoints': exactly 0 on the 0 ms one at `shout`, whose
        // next is 0 too, and -12 on the last.
        if (tick == kLegStarts[1]) CHECK(std::abs(dragon.look.angleDeg) < 1e-12);
        if (dragon.phase == DarkDragon::Phase::Fighting) fighting = tick;
    }
    CHECK_EQ(legStarts.size(), std::size_t{7});
    bool asScheduled = legStarts.size() == 7;
    for (std::size_t i = 0; i < legStarts.size() && i < 7; ++i) asScheduled = asScheduled && legStarts[i] == kLegStarts[i];
    std::string starts;
    for (const int one : legStarts) starts += std::to_string(one) + " ";
    CHECK_MSG(asScheduled, "the waypoints begin on 1 151 152 211 271 313 445: " + starts);
    CHECK_EQ(fighting, kFightingTick);
    CHECK_MSG(worstHover < 1e-4, "it stands at the last originalPos plus the swing: " + std::to_string(worstHover));
    CHECK_MSG(worstBehind < 1e-9, "and writes the path's point as the next: " + std::to_string(worstBehind));
    // THE JAW: 0 through the first three waypoints, 0 -> 3 on the fourth - 1 on
    // its 16th update (bias 0.25), 2 on its 22nd, 3 on its 27th - and 3 held on.
    bool shutBefore = true;
    for (int tick = 1; tick < 226 && tick <= static_cast<int>(cells.size()); ++tick) {
        shutBefore = shutBefore && cells[static_cast<std::size_t>(tick - 1)] == 0;
    }
    CHECK_MSG(shutBefore, "cell 0 until the jaw's waypoint is a quarter through");
    CHECK(cells.size() >= 238 && cells[225] == 1 && cells[230] == 1 && cells[231] == 2 && cells[235] == 2 &&
          cells[236] == 3);
    bool heldOpen = true;
    for (std::size_t i = 236; i < cells.size(); ++i) heldOpen = heldOpen && cells[i] == 3;
    CHECK_MSG(heldOpen, "and held on cell 3 through the last two waypoints and into FIGHTING");
    // PlayParticleSystem(0) every update of the jaw's waypoint on the widest cell,
    // and nowhere else in the fly-in: ticks 237 to 270.
    CHECK_EQ(restarts, 34);
    CHECK_EQ(restartsOffTheWidest, 0);
    CHECK_MSG(!dragon.look.systemKilled, "the mouth's fire runs on from the jaw's waypoint, killed by nothing");
    // THE FIRE CLOCK is NOT zeroed entering FIGHTING: the jaw's waypoint held it at
    // 0, and it ran on through the last two, 175 updates.
    CHECK(::test::nearly(static_cast<float>(dragon.elapsedMs), static_cast<float>(175.0 * kStep * 1000.0), 0.01f));
    // And the Follower was forced to -12 + 360.
    CHECK(::test::nearly(static_cast<float>(dragon.look.angleDeg), -12.0f));
    CHECK(::test::nearly(static_cast<float>(dragon.follower.a.x * 180.0 / kPi), 348.0f, 1e-3f));
    std::printf("  waypoints begin on %s- the jaw 1/2/3 on 226/232/237, FIGHTING on %d, its clock at %.1f ms\n",
                starts.c_str(), fighting, dragon.elapsedMs);
}

// THE FIGHT'S ANGLE (ins 688-818): the Follower's lagged angle toward the player,
// the neck it lifts, the mouth a shot leaves from - and the first shot 1.3 s in.
void ItTurnsTowardTheLaggedAngle() {
    Game::Data data;
    entt::registry registry;
    Game::Level level;
    if (!Open(kLevel, data, registry, level)) return;
    if (!Summon(data, registry, level)) return;
    const DarkDragon::State& dragon = level.darkDragon;
    const DarkDragon::Rules& rules = dragon.rules;
    const double entryClock = dragon.elapsedMs;

    std::vector<double> angles;
    std::vector<double> dests;
    double worstNeck = 0.0;
    double worstDest = 0.0;
    int shotAfter = 0;
    glm::dvec2 shotFrom(0.0);
    glm::dvec2 shotAim(0.0);
    double shotAngle = 0.0;
    glm::dvec2 shotAtPx(0.0);
    glm::dvec2 shotPlayer(0.0);
    for (int tick = 1; tick <= 200; ++tick) {
        // Stood where it can be seen, as F16's player stood.
        if (tick == 1) PutPlayerAt(registry, level.player, kFloorPx);
        const int fired = dragon.fired;
        Game::Tick(data, registry, level, 0.0f, kStep);
        angles.push_back(dragon.look.angleDeg);
        dests.push_back(dragon.follower.dest.x * 180.0 / kPi);
        const glm::dvec2 playerPx = PlayerPx(registry, level.player);
        worstDest = std::max(worstDest, std::abs(dragon.follower.dest.x -
                                                 DarkDragon::AimRadians(rules, dragon.atPx, playerPx)));
        worstNeck = std::max(worstNeck, std::abs(dragon.originPx.y - dragon.fightingPx.y -
                                                 DarkDragon::NeckLiftPx(rules, dragon.look.angleDeg, dragon.neckMs)));
        if (dragon.fired > fired && shotAfter == 0) {
            shotAfter = tick;
            shotAngle = dragon.look.angleDeg;
            shotAtPx = dragon.atPx;
            shotPlayer = playerPx;
            for (const Turrets::Fireball& ball : level.turrets.fireballs) {
                if (ball.name.rfind(dragon.name, 0) != 0) continue;
                shotFrom = ball.atPx;
                const double speed = std::sqrt(glm::dot(ball.velocityPx, ball.velocityPx));
                shotAim = speed > 0.0 ? ball.velocityPx / speed : glm::dvec2(0.0);
            }
        }
    }
    // HELD, then eased: nine updates at 348 - the Follower holds a forced point for
    // one whole period of 150 ms on the script's 16 ms clock - then the tenth
    // retargets and closes sin(16 / 800 x PI/2) = 3.14 % of the way.
    bool held = true;
    for (int i = 0; i < 9; ++i) held = held && std::abs(angles[static_cast<std::size_t>(i)] - 348.0) < 1e-9;
    CHECK_MSG(held, "nine FIGHTING updates at the forced 348");
    const double closed = (angles[9] - 348.0) / (dests[8] - 348.0);
    CHECK_MSG(std::abs(closed - std::sin(16.0 / 800.0 * kPi / 2.0)) < 1e-4,
              "the tenth closes 3.14 % toward the destination the ninth set: " + std::to_string(closed));
    // Each update's destination is computeDarkDragonAngle of where it stands and
    // where the player is: the dragon LESS the player, + PIb x 1.2, unclamped.
    CHECK_MSG(worstDest < 1e-9, "the destination is the aim of this update's positions: " + std::to_string(worstDest));
    // STEADY: toward 378 - phi. After three seconds the lag is within a degree and a half of
    // what the target is now, the hover swinging the target under it.
    const double settled = angles.back();
    const double target = dests.back();
    CHECK_MSG(std::abs(settled - target) < 1.5, "settled within a degree and a half of its target: " + std::to_string(settled) +
                                                     " against " + std::to_string(target));
    CHECK_MSG(target > 340.0 && target < 378.0,
              "a player below it to the right is 378 - phi, phi above 0: " + std::to_string(target));
    // THE NECK, every update, on the point the next stands at.
    CHECK_MSG(worstNeck < 1e-9, "originalPos is `fighting` plus the neck's lift: " + std::to_string(worstNeck));
    // THE FIRST SHOT comes 1.3 s into the fight, not 4.2 s: the clock came in at
    // 2917 ms. 4200 - 2917 = 1283 ms, the 77th update.
    CHECK(::test::nearly(static_cast<float>(entryClock), static_cast<float>(175.0 * kStep * 1000.0), 0.01f));
    CHECK_EQ(shotAfter, 77);
    // FROM THE MOUTH, AT THE LIVE PLAYER: the muzzle turned with the head, and the
    // shot aimed from there.
    const glm::dvec2 mouth = DarkDragon::MuzzlePx(rules, shotAtPx, shotAngle);
    CHECK_MSG(glm::distance(shotFrom, mouth) < 1e-9, "it leaves from the mouth: " + Pt(shotFrom) + " vs " + Pt(mouth));
    CHECK_MSG(glm::distance(shotAim, glm::normalize(shotPlayer - mouth)) < 1e-6,
              "toward the player from there: " + Pt(shotAim));
    std::printf("  348 for 9 updates, %.2f %% on the 10th, %.2f toward %.2f after %zu; first shot on the %dth, "
                "from %s\n",
                closed * 100.0, settled, target, angles.size(), shotAfter, Pt(shotFrom).c_str());
}

// THE LOOP: only a blast hurts it, and each wound costs the player their portals.
void OnlyABlastHurtsIt() {
    Game::Data data;
    entt::registry registry;
    Game::Level level;
    if (!Open(kLevel, data, registry, level)) return;
    if (!Summon(data, registry, level)) return;

    const int hp = level.darkDragon.hp;

    // Ten seconds of its own fireballs flying about, and it is unhurt: a fireball
    // detonates a bomb but cannot touch the boss.
    for (int tick = 0; tick < 600; ++tick) Game::Tick(data, registry, level, 0.0f, kStep);
    CHECK_EQ(level.darkDragon.hp, hp);
    CHECK_EQ(level.darkDragon.hits, 0);

    // Now the bomb, moved beside it as portalling it there would - and asked to go
    // off, which is what its own fireball does.
    Fire::Bomb* bomb = nullptr;
    for (Fire::Bomb& one : level.fire.bombs) {
        if (!one.blown && registry.valid(one.body)) bomb = &one;
    }
    CHECK(bomb != nullptr);
    if (bomb == nullptr) return;

    // Two portals on the board, so the wound has something to take away.
    level.portals.TryPlace(glm::dvec2(300.0, 200.0));
    level.portals.TryPlace(glm::dvec2(360.0, 200.0));
    const std::size_t placed = level.portals.placed.size();
    CHECK(placed > 0);

    registry.get<TransformComponent>(bomb->body).position =
        Units::ToWorld(level.darkDragon.atPx.x + 20.0, level.darkDragon.atPx.y);
    bomb->requested = true;
    Game::Tick(data, registry, level, 0.0f, kStep);

    CHECK_EQ(level.darkDragon.hits, 1);
    CHECK_EQ(level.darkDragon.hp, hp - 1);
    CHECK(level.darkDragon.phase == DarkDragon::Phase::Damage);
    // Wounding it takes every portal away.
    CHECK(level.portals.placed.empty());
    // But not the budget: killing portals is not a refund.
    CHECK(level.portals.portalsUsed > 0);
    std::printf("  a blast beside it: hp %d -> %d, and %zu portal(s) taken away\n", hp, level.darkDragon.hp, placed);

    // And it comes back to the fight, faster than it was - 3400 ms and the path's
    // own first update after the wound (R20), with a shot at the torch on that tick.
    const double before = level.darkDragon.rules.fireIntervalMs;
    const int firedBefore = level.darkDragon.fired;
    const int back = RunUntil(data, registry, level, 600,
                              [&] { return level.darkDragon.phase == DarkDragon::Phase::Fighting; });
    CHECK(level.darkDragon.phase == DarkDragon::Phase::Fighting);
    CHECK_EQ(back, 205);
    CHECK_MSG(level.darkDragon.fired == firedBefore + 1, "it shot at the torch as the wound ended");
    CHECK(level.darkDragon.FireIntervalMs() < before);
    CHECK(::test::nearly(static_cast<float>(level.darkDragon.FireIntervalMs()),
                         static_cast<float>(before - level.darkDragon.rules.fireIntervalStepMs)));
    std::printf("  the wound lasted %d updates, %.0f ms\n", back, back * kStep * 1000.0);
}

// THE WOUND'S PICTURE (ins 1125-1613): red and bright, eased, the jaw shut and
// opened again, the head thrown back 27 degrees - and the torch shot from the
// mouth at the angle it ends on.
void TheWoundFlashesRedAndBright() {
    Game::Data data;
    entt::registry registry;
    Game::Level level;
    if (!Open(kLevel, data, registry, level)) return;
    if (!Summon(data, registry, level)) return;
    const DarkDragon::State& dragon = level.darkDragon;
    const DarkDragon::Rules& rules = dragon.rules;
    PutPlayerAt(registry, level.player, kFloorPx);
    for (int tick = 0; tick < 100; ++tick) Game::Tick(data, registry, level, 0.0f, kStep);

    level.darkDragon.burned = true;
    Game::Tick(data, registry, level, 0.0f, kStep);
    CHECK(dragon.phase == DarkDragon::Phase::Damage);
    const double hitDeg = dragon.path.points.empty() ? 0.0 : dragon.path.points.front().angleDeg;
    const glm::dvec2 hitPx = dragon.path.points.empty() ? glm::dvec2(0.0) : dragon.path.points.front().atPx;
    CHECK_MSG(dragon.path.points.size() == 10, "ten waypoints: the wound, eight flashes, and back");
    CHECK(hitPx == dragon.atPx && hitDeg == dragon.look.angleDeg);

    std::vector<glm::dvec3> emissive;
    std::vector<int> cells;
    std::vector<double> angles;
    double worstColour = 0.0;
    glm::dvec2 torchFrom(0.0);
    glm::dvec2 torchAim(0.0);
    glm::dvec2 standing(0.0);
    double endAngle = 0.0;
    for (int tick = 1; tick <= 205; ++tick) {
        const int fired = dragon.fired;
        Game::Tick(data, registry, level, 0.0f, kStep);
        if (dragon.phase == DarkDragon::Phase::Damage) {
            worstColour = std::max(worstColour, glm::distance(dragon.look.emissive, dragon.path.Current().colour));
        }
        emissive.push_back(dragon.look.emissive);
        cells.push_back(dragon.look.frame);
        angles.push_back(dragon.look.angleDeg);
        if (dragon.fired > fired) {
            standing = dragon.atPx;
            endAngle = dragon.look.angleDeg;
            for (const Turrets::Fireball& ball : level.turrets.fireballs) {
                if (ball.name != dragon.name + "#" + std::to_string(dragon.fired)) continue;
                torchFrom = ball.atPx;
                const double speed = std::sqrt(glm::dot(ball.velocityPx, ball.velocityPx));
                torchAim = speed > 0.0 ? ball.velocityPx / speed : glm::dvec2(0.0);
            }
        }
    }
    CHECK(dragon.phase == DarkDragon::Phase::Fighting);
    CHECK_MSG(worstColour < 1e-12, "the emissive IS the path's colour, every update: " + std::to_string(worstColour));
    // The legs begin on updates 1, 61, 79, ... 187 and 205: the .ent's own, then red,
    // bright, red ... bright, and its own again as FIGHTING takes over.
    const glm::dvec3 bright = rules.emissive * rules.damageBrightTimes;
    CHECK(glm::distance(emissive[0], rules.emissive) < 1e-6);
    bool flashes = true;
    for (int leg = 1; leg <= 8; ++leg) {
        const glm::dvec3 want = leg % 2 == 1 ? rules.damageRed : bright;
        flashes = flashes && glm::distance(emissive[static_cast<std::size_t>(61 + (leg - 1) * 18 - 1)], want) < 1e-6;
    }
    CHECK_MSG(flashes, "red on the first, third, fifth and seventh flash; three times its own on the others");
    CHECK(glm::distance(emissive[204], rules.emissive) < 1e-6);
    // At its reddest the red term is the whole of it and green none: R >= 2 G by far.
    CHECK(emissive[60].r >= 2.0 * emissive[60].g);
    // THE JAW: shut over the first leg (0 by its 41st update), opened again over the last flash.
    CHECK(cells[0] == 3 && cells[39] == 1 && cells[40] == 0 && cells[185] == 0);
    CHECK(cells[193] == 1 && cells[195] == 2 && cells[198] == 3 && cells[204] == 3);
    // THE HEAD thrown back: the angle it was hit at, eased to +27 over the first leg.
    CHECK(::test::nearly(static_cast<float>(angles[0]), static_cast<float>(hitDeg), 1e-4f));
    CHECK(::test::nearly(static_cast<float>(angles[60]), static_cast<float>(hitDeg + 27.0), 1e-4f));
    CHECK(::test::nearly(static_cast<float>(endAngle), static_cast<float>(hitDeg + 27.0), 1e-4f));
    // THE TORCH SHOT: from its mouth at that angle, along its own point's line to the light.
    const glm::dvec2 torchPx = level.torch.lights.front().atPx;
    CHECK_MSG(glm::distance(torchFrom, DarkDragon::MuzzlePx(rules, standing, endAngle)) < 1e-9,
              "from the mouth: " + Pt(torchFrom));
    CHECK_MSG(glm::distance(torchAim, glm::normalize(torchPx - standing)) < 1e-6,
              "along the line from its point to the torch: " + Pt(torchAim));
    // And, from the floor F16's player stood on, it passes within the torch's reach
    // (hasProjectileAround's scale(24)). The port fires it at the torch lit or not;
    // the original only at an unlit light_off.ent, which it relights (ins
    // 1675-1694) - the owner's row in planning step 99. This pins the arithmetic.
    const glm::dvec2 across = torchPx - torchFrom;
    const double miss = std::abs(across.x * torchAim.y - across.y * torchAim.x);
    CHECK_MSG(miss < level.torch.rules.reachPx, "the torch shot passes " + std::to_string(miss) + " from the torch");
    std::printf("  hit at %.2f deg, thrown to %.2f; the torch shot from %s passes %.1f from the torch\n", hitDeg,
                endAngle, Pt(torchFrom).c_str(), miss);
}

// THE WOUND IS READ AFTER THE MOUTH AND THE SHOT (ins 829-1067), and all three only
// while the level is not finished (ins 821-827).
void TheWoundIsReadAfterItsShot() {
    // A blast on the update it fires: the shot leaves first, then DAMAGE.
    {
        Game::Data data;
        entt::registry registry;
        Game::Level level;
        if (!Open(kLevel, data, registry, level)) return;
        if (!Summon(data, registry, level)) return;
        DarkDragon::State& dragon = level.darkDragon;
        const int fired = dragon.fired;
        registry.get<TransformComponent>(level.player).position = Units::ToWorld(dragon.atPx.x, dragon.atPx.y + 40.0);
        dragon.elapsedMs = dragon.FireIntervalMs() + 1.0;
        dragon.burned = true;
        Game::Tick(data, registry, level, 0.0f, kStep);
        CHECK_MSG(dragon.fired == fired + 1, "the shot of the update it was burned on left: " +
                                                 std::to_string(dragon.fired - fired));
        CHECK(dragon.phase == DarkDragon::Phase::Damage && dragon.hits == 1 && !dragon.burned);
        CHECK_MSG(dragon.look.systemRestarted && !dragon.look.systemKilled,
                  "its mouth restarted above three quarters of the interval, before the wound");
    }
    // A blast below half the interval: the mouth is killed before DAMAGE, and DAMAGE
    // writes neither Kill nor Play, so it stays killed through the wound.
    {
        Game::Data data;
        entt::registry registry;
        Game::Level level;
        if (!Open(kLevel, data, registry, level)) return;
        if (!Summon(data, registry, level)) return;
        DarkDragon::State& dragon = level.darkDragon;
        dragon.elapsedMs = 0.0;
        dragon.look.systemKilled = false;
        dragon.burned = true;
        Game::Tick(data, registry, level, 0.0f, kStep);
        CHECK(dragon.phase == DarkDragon::Phase::Damage);
        CHECK_MSG(dragon.look.systemKilled && !dragon.look.systemRestarted, "killed on the update it was wounded");
        for (int tick = 0; tick < 100; ++tick) Game::Tick(data, registry, level, 0.0f, kStep);
        CHECK_MSG(dragon.phase == DarkDragon::Phase::Damage && dragon.look.systemKilled, "and killed through DAMAGE");
    }
    // A finished level: no mouth, no shot, no wound read.
    {
        Game::Data data;
        entt::registry registry;
        Game::Level level;
        if (!Open(kLevel, data, registry, level)) return;
        if (!Summon(data, registry, level)) return;
        DarkDragon::State& dragon = level.darkDragon;
        const int fired = dragon.fired;
        level.goals.completed = true;
        dragon.elapsedMs = dragon.FireIntervalMs() + 1.0;
        dragon.burned = true;
        Game::Tick(data, registry, level, 0.0f, kStep);
        CHECK_MSG(dragon.phase == DarkDragon::Phase::Fighting && dragon.hits == 0 && dragon.burned &&
                      dragon.fired == fired,
                  "isFinished guards the mouth, the shot and the wound alike");
    }
    std::printf("  the wound is read after the mouth and the shot, and not once the level is finished\n");
}

// It shoots at the player, and the shot is a fireball that kills.
void ItSpitsAtThePlayer() {
    Game::Data data;
    entt::registry registry;
    Game::Level level;
    if (!Open(kLevel, data, registry, level)) return;
    if (!Summon(data, registry, level)) return;

    // IT CANNOT SEE ACROSS ITS OWN ARENA, and that is the gate rather than a
    // fault. It fights at (16, 56) and the player spawns at (710, 48), 694 px away
    // through the pillars and platforms level31c stands between them - so with its
    // clock well past the interval it still does not shoot. The first cut of this
    // suite ticked six seconds here, asserted a shot, and failed for exactly this
    // reason: the assertion assumed line of sight the level does not give.
    {
        const std::size_t before = level.turrets.fireballs.size();
        const int fired = level.darkDragon.fired;
        level.darkDragon.elapsedMs = level.darkDragon.FireIntervalMs() + 1.0;
        Game::Tick(data, registry, level, 0.0f, kStep);
        CHECK_EQ(level.turrets.fireballs.size(), before);
        CHECK_EQ(level.darkDragon.fired, fired);
        std::printf("  refused the shot across %.0f px of its own arena\n",
                    std::abs(710.0 - level.darkDragon.atPx.x));
    }

    // And it shoots the moment the player comes to it. The clock is set rather
    // than waited out, because a player left standing for six seconds falls, and
    // what is being pinned here is the gate and not gravity.
    {
        const std::size_t before = level.turrets.fireballs.size();
        const int fired = level.darkDragon.fired;
        registry.get<TransformComponent>(level.player).position =
            Units::ToWorld(level.darkDragon.atPx.x, level.darkDragon.atPx.y + 40.0);
        level.darkDragon.elapsedMs = level.darkDragon.FireIntervalMs() + 1.0;
        Game::Tick(data, registry, level, 0.0f, kStep);

        CHECK(level.darkDragon.fired > fired);
        CHECK(level.turrets.fireballs.size() > before);
        bool mine = false;
        for (const Turrets::Fireball& ball : level.turrets.fireballs) {
            if (ball.name.rfind(level.darkDragon.name, 0) != 0) continue;
            mine = true;
            // A carranca's kills and a fire diamond's does not; this one kills.
            CHECK(ball.killsPlayer);
            // And it is the ONE fireball in the game drawn at twice the size:
            // addFireball's fireballScale, 2 at both of this callback's calls
            // (PshC4 2f at #972 and #1714) and 1 at every other caller's. It
            // scales fireball.ent's first particle system alone.
            CHECK_MSG(::test::nearly(static_cast<float>(ball.particleScale), 2.0f),
                      "the dark dragon's fire is ScaleParticleSystem(0, 2): " +
                          std::to_string(ball.particleScale));
        }
        CHECK(mine);
        // And its clock restarted, so the next shot is a whole interval away.
        CHECK(level.darkDragon.elapsedMs < level.darkDragon.FireIntervalMs());
        std::printf("  and took it from 40 px away, at a %.0f ms interval\n", level.darkDragon.FireIntervalMs());
    }
}

// THE END OF THE GAME: it falls, and leaves the only key the level has.
void ItDropsTheLevelsOnlyKey() {
    Game::Data data;
    entt::registry registry;
    Game::Level level;
    if (!Open(kLevel, data, registry, level)) return;
    if (!Summon(data, registry, level)) return;

    // One from death, so the next blast ends it.
    level.darkDragon.hp = 1;
    Fire::Bomb* bomb = nullptr;
    for (Fire::Bomb& one : level.fire.bombs) {
        if (!one.blown && registry.valid(one.body)) bomb = &one;
    }
    if (bomb == nullptr) {
        CHECK_MSG(false, "level31c places a bomb");
        return;
    }
    registry.get<TransformComponent>(bomb->body).position =
        Units::ToWorld(level.darkDragon.atPx.x, level.darkDragon.atPx.y + 20.0);
    bomb->requested = true;
    Game::Tick(data, registry, level, 0.0f, kStep);

    CHECK_EQ(level.darkDragon.hp, 0);
    CHECK(level.darkDragon.phase == DarkDragon::Phase::Dying);
    CHECK(level.keys.keys.empty()); // not yet: it falls first
    const double hitDeg = level.darkDragon.look.angleDeg;

    // WHITE from its first dying update (FloatColor(-1)), the head turning +30 over
    // the first leg, the jaw shut over it.
    Game::Tick(data, registry, level, 0.0f, kStep);
    CHECK(level.darkDragon.look.emissive == glm::dvec3(1.0));
    CHECK_EQ(level.darkDragon.look.frame, 3);
    for (int tick = 0; tick < 180; ++tick) Game::Tick(data, registry, level, 0.0f, kStep);
    CHECK_EQ(level.darkDragon.look.frame, 0);
    CHECK(::test::nearly(static_cast<float>(level.darkDragon.look.angleDeg), static_cast<float>(hitDeg + 30.0), 1e-3f));
    CHECK(level.darkDragon.look.emissive == glm::dvec3(1.0));

    // 3000 ms hovering and 6000 ms falling, and the path's first update.
    const int fell = 181 + RunUntil(data, registry, level, 900, [&] { return level.darkDragon.gone; });
    CHECK(level.darkDragon.gone);
    CHECK(!level.darkDragon.Alive());
    std::printf("  it fell over %.2f s and dropped its key\n", fell * kStep);
    CHECK_EQ(fell, 541);

    CHECK_EQ(level.keys.keys.size(), std::size_t{1});
    if (!level.keys.keys.empty()) {
        const Keys::Key& dropped = level.keys.keys.front();
        // The colour the level's keyhole and its locked door both carry.
        CHECK(dropped.colour == "yellow");
        CHECK(dropped.name == level.darkDragon.keyName);
        CHECK(::test::nearly(static_cast<float>(dropped.atPx.x), static_cast<float>(kKeyX)));
        CHECK(::test::nearly(static_cast<float>(dropped.atPx.y), static_cast<float>(kKeyY)));
        bool matches = false;
        for (const Keys::Keyhole& hole : level.keys.keyholes) {
            if (hole.colour == dropped.colour) matches = true;
        }
        CHECK_MSG(matches, "the key it drops opens the keyhole the level places");
    }

    // AND THE PLATFORM, added at the same waypoint. Until the port had a way to
    // place a body with no node behind it this was a recorded gap, and with a
    // level edge in place it was the difference between the key being reachable
    // and the player falling out of the world going for it.
    const entt::entity platform = level.darkDragon.platformBody;
    CHECK_MSG(platform != entt::null && registry.valid(platform), "the death adds its platform");
    if (platform != entt::null && registry.valid(platform)) {
        const glm::dvec2 atPx = Units::ToPixels(registry.get<TransformComponent>(platform).position);
        // platform_pos, read off level31c.
        CHECK(::test::nearly(static_cast<float>(atPx.x), 160.0f));
        CHECK(::test::nearly(static_cast<float>(atPx.y), 240.0f));
        // The template sibling's own box, not a number this test invented.
        const auto& box = registry.get<Supersonic::BoxColliderComponent>(platform);
        const float widthPx = box.size.x * static_cast<float>(Units::kPixelsPerMetre);
        const float heightPx = box.size.y * static_cast<float>(Units::kPixelsPerMetre);
        CHECK(::test::nearly(widthPx, 64.0f));
        CHECK(::test::nearly(heightPx, 32.0f));
        std::printf("  and a %.0f x %.0f platform at (%.0f, %.0f), from the sibling the level places\n", widthPx,
                    heightPx, atPx.x, atPx.y);
    }

    // And it stays dead.
    for (int tick = 0; tick < 300; ++tick) Game::Tick(data, registry, level, 0.0f, kStep);
    CHECK_EQ(level.keys.keys.size(), std::size_t{1});
    CHECK(level.darkDragon.gone);
}

void runTests() {
    TheRulesRead();
    TheRulesAreRefusedWhenTheyDoNotMakeABoss();
    TheEasingFiltersAreTheScripts();
    ThePathMovesOnOneWaypointAnUpdate();
    ItAnswersForItsOwnNodeAndNoOthers();
    TheLevelPlacesItWhereTheFileSaysIt();
    ItDoesNotComeUntilTheTorchIsLit();
    ItComesInOnItsSevenWaypoints();
    ItTurnsTowardTheLaggedAngle();
    OnlyABlastHurtsIt();
    TheWoundFlashesRedAndBright();
    TheWoundIsReadAfterItsShot();
    ItSpitsAtThePlayer();
    ItDropsTheLevelsOnlyKey();
}

} // namespace

int main() {
    std::error_code ec;
    if (!std::filesystem::is_directory(kLevels, ec) ||
        !std::filesystem::is_regular_file(kData + "/entity_roles.json", ec)) {
        std::printf("test_mp_darkdragon: SKIPPED - needs the converted levels at %s\n"
                    "  and the remake's data at %s.\n"
                    "  Both live outside this repository; configure with\n"
                    "  -DSUPERSONIC_MAGICPORTALS_LEVELS=... and -DSUPERSONIC_MAGICPORTALS_DATA=...\n",
                    kLevels.c_str(), kData.c_str());
        return 77;
    }
    runTests();
    return ::test::summary("test_mp_darkdragon", 60);
}
