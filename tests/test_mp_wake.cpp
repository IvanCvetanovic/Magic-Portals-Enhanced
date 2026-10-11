// What wakes a sleeper when the body it rests on goes (planning step 163, WAKE: backlog R3b-4 and R3c-5).
//
// Box2D wakes both bodies of a contact when it is destroyed (b2Contact::Destroy, b2Contact.cpp:101-109, from DestroyBody)
// and when its touching changes (b2Contact::Update, :210-214); the engine the port runs on wakes a sleeper only by a
// contact that begins with a body that moves, and knows nothing of a body that is no longer there. sim/Resting::Watch is
// the contact rule: a sleeper remembers what it touches, and is woken, before the step that would leave it hanging, when
// one of those bodies is destroyed, stops being solid, or is put somewhere else by the port. This suite pins what that
// makes true, on the tick:
//
//   a wall     a sleeper over a wall that is destroyed - by the port's own Demolish, a crate burnt by Fire, a door a
//              keyhole takes with it - wakes on the tick the contact goes and falls in the step that follows; the engine
//              alone leaves it hanging (the control)
//   a mover    a sleeper over a lift, a platform or a door the port moves wakes the same way
//   no more    a body that is not what the sleeper touches (10 units off, a trigger, a body its masks do not meet) wakes
//              it by nothing: it sleeps on
//   a still    where the level places a sleeper the port cannot find what it rests on by looking (3-12's stone is 1.95
//              units off the crate's corner), so resting.json names it, and the crate going wakes the stone
//   3-10       knight B holds over a breakable wall, asleep; the wall breaks, and the knight falls on that tick
//
// The rigs are bare registries stepped by the engine's own PhysicsSystem; the level cases read the converted levels
// from outside this repository and skip themselves, saying where they looked, without them.

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "core/Components.hpp"
#include "core/PhysicsSystem.hpp"
#include "sim/Demolish.hpp"
#include "sim/Fire.hpp"
#include "sim/Game.hpp"
#include "sim/Impacts.hpp"
#include "sim/Keys.hpp"
#include "sim/Minions.hpp"
#include "sim/Mover.hpp"
#include "sim/Resting.hpp"
#include "sim/ScriptClock.hpp"
#include "sim/Units.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <system_error>
#include <unordered_map>
#include <vector>

using namespace MagicPortals;
using Supersonic::BoxColliderComponent;
using Supersonic::PhysicsSystem;
using Supersonic::RigidBodyComponent;
using Supersonic::TagComponent;
using Supersonic::TransformComponent;

namespace {

const std::string kLevels = MAGICPORTALS_LEVELS_DIR;
const std::string kData = MAGICPORTALS_DATA_DIR;
const std::string kPortData = MAGICPORTALS_PORT_DATA_DIR;
constexpr float kTick = 1.0f / 60.0f;

// ---- the rig -------------------------------------------------------------------------------------------------------

enum class Kind { Static, Loose, Mover, Trigger };

struct Rig {
    entt::registry registry;

    Rig() {
        Resting::Rules rules;
        rules.touchUnits = 0.5;
        rules.movedMetres = 0.0001;
        registry.ctx().insert_or_assign<Resting::Rules>(Resting::Rules(rules));
    }

    // A box at a point in the port's pixels (+y down), `wPx` by `hPx`.
    entt::entity Box(const std::string& name, Kind kind, double xPx, double yPx, double wPx, double hPx) {
        const entt::entity entity = registry.create();
        registry.emplace<TagComponent>(entity, TagComponent{name});
        registry.emplace<TransformComponent>(entity).position = Units::ToWorld(xPx, yPx);
        auto& box = registry.emplace<BoxColliderComponent>(entity);
        box.size = glm::vec3(Units::ToMetres(wPx), Units::ToMetres(hPx), 1.0f);
        box.isTrigger = kind == Kind::Trigger;
        if (kind == Kind::Static || kind == Kind::Trigger) return entity;
        auto& rigid = registry.emplace<RigidBodyComponent>(entity);
        rigid.isKinematic = kind == Kind::Mover;
        return entity;
    }

    // A loose body put to sleep where it stands, as the engine writes one that has been still long enough.
    entt::entity Sleeper(const std::string& name, double xPx, double yPx, double wPx, double hPx) {
        const entt::entity entity = Box(name, Kind::Loose, xPx, yPx, wPx, hPx);
        auto& rigid = registry.get<RigidBodyComponent>(entity);
        rigid.isSleeping = true;
        rigid.sleepPosition = registry.get<TransformComponent>(entity).position;
        return entity;
    }

    bool Asleep(entt::entity entity) const { return registry.get<RigidBodyComponent>(entity).isSleeping; }
    double YPx(entt::entity entity) const { return Units::ToPixels(registry.get<TransformComponent>(entity).position).y; }
    void Step() { PhysicsSystem::Update(registry, kTick); }
};

// The common pile: a floor, a 60 x 60 wall standing on it with its top at y 220, a 30 x 30 body asleep on the wall.
struct Pile {
    Rig rig;
    entt::entity floor = entt::null;
    entt::entity wall = entt::null;
    entt::entity body = entt::null;

    Pile() {
        floor = rig.Box("floor", Kind::Static, 100.0, 310.0, 600.0, 20.0);   // top at y 300
        wall = rig.Box("wall", Kind::Static, 100.0, 250.0, 60.0, 60.0);       // 220 to 280... under it nothing
        body = rig.Sleeper("body", 100.0, 205.0, 30.0, 30.0);                  // 190 to 220: its bottom on the wall's top
    }
};

// ---- a wall that goes ----------------------------------------------------------------------------------------------

// A sleeper over a wall that is destroyed wakes on the tick the contact goes and falls in the step that follows; the same rig with
// the engine alone leaves it hanging.
void ASleeperOverAWallThatIsDestroyedWakesAndFallsTheSameTick() {
    Pile pile;
    Rig& rig = pile.rig;
    std::vector<Resting::Looked> looked;
    CHECK_MSG(Resting::Watch(rig.registry, &looked).empty(), "nothing is woken the first time a sleeper is seen: it is only looked at");
    CHECK_MSG(looked.size() == 1 && looked[0].sleeperName == "body" && looked[0].supports.size() == 1 &&
                  looked[0].supports[0].first == "wall" && std::fabs(looked[0].supports[0].second) < 1e-3,
              "and the look is reported: the body sleeps on the wall, 0 units off");
    const auto* supports = rig.registry.try_get<Resting::Supports>(pile.body);
    CHECK_MSG(supports != nullptr && supports->scanned, "the sleeper has a record of what it touches");
    if (supports != nullptr) {
        const auto holds = [&](entt::entity entity) {
            return std::any_of(supports->list.begin(), supports->list.end(),
                               [entity](const Resting::Support& support) { return support.entity == entity; });
        };
        CHECK_MSG(holds(pile.wall), "the wall it lies on is one of them");
        CHECK_MSG(!holds(pile.floor), "the floor 80 units under it is not");
    }
    CHECK_MSG(Resting::Watch(rig.registry).empty(), "and a tick later the wall still stands, so it sleeps on");
    CHECK(rig.Asleep(pile.body));
    rig.Step();
    CHECK_MSG(rig.Asleep(pile.body), "the engine leaves a sleeper over a standing wall asleep");

    const double before = rig.YPx(pile.body);
    rig.registry.destroy(pile.wall);
    const std::vector<Resting::Woken> woken = Resting::Watch(rig.registry);
    CHECK_MSG(woken.size() == 1, "the tick the wall is destroyed, one sleeper is woken");
    if (woken.size() == 1) {
        CHECK(woken[0].sleeper == pile.body);
        CHECK_MSG(woken[0].cause == Resting::Cause::Gone, "because its support is gone");
        CHECK_MSG(woken[0].supportName == "wall" && woken[0].sleeperName == "body", "named for the log");
    }
    CHECK_MSG(!rig.Asleep(pile.body), "it is awake");
    CHECK_MSG(rig.registry.try_get<Resting::Supports>(pile.body) == nullptr, "and has no supports until it sleeps again");
    rig.Step();
    CHECK_MSG(rig.YPx(pile.body) > before + 0.05, "and falls in the very next physics step: " + std::to_string(rig.YPx(pile.body) - before));

    // The control: the same pile, the same destruction, the engine alone. It hangs, which is the bug.
    Pile control;
    control.rig.registry.destroy(control.wall);
    for (int tick = 0; tick < 60; ++tick) control.rig.Step();
    CHECK_MSG(control.rig.Asleep(control.body), "without the watch the sleeper still hangs a second after its wall has gone");
    CHECK_MSG(std::fabs(control.rig.YPx(control.body) - 205.0) < 0.001, "in the place it slept");
}

// The same, through the port's own modules that destroy what a body rests on, each on the tick it does it.
void TheModulesThatRemoveASupportWakeWhatRestsOnIt() {
    // Demolish: a breakable wall a stone broke.
    {
        Pile pile;
        Demolish::State demolish;
        Demolish::Breakable breakable;
        breakable.name = "breakable_wall";
        breakable.entity = "breakable_wall";
        breakable.body = pile.wall;
        demolish.breakables.push_back(breakable);
        Resting::Watch(pile.rig.registry);
        CHECK(demolish.Break(pile.rig.registry, "breakable_wall", "a stone") == pile.wall);
        const auto woken = Resting::Watch(pile.rig.registry);
        CHECK_MSG(woken.size() == 1 && !pile.rig.Asleep(pile.body), "a sleeper over a breakable wall wakes the tick the wall breaks");
    }
    // Fire: a crate that has burnt down.
    {
        Pile pile;
        Fire::State fire;
        fire.rules.burnMs = 1000.0;
        Fire::Burnable burnable;
        burnable.name = "crate";
        burnable.body = pile.wall;
        burnable.fades = true;
        burnable.burned = true;
        fire.burnables.push_back(burnable);
        Resting::Watch(pile.rig.registry);
        CHECK_MSG(fire.Tick(pile.rig.registry, entt::null, 0.5).empty() && pile.rig.Asleep(pile.body),
                  "half way through its burn the crate stands and the sleeper sleeps");
        Resting::Watch(pile.rig.registry);
        CHECK_MSG(fire.Tick(pile.rig.registry, entt::null, 0.6).size() == 1, "the crate goes");
        const auto woken = Resting::Watch(pile.rig.registry);
        CHECK_MSG(woken.size() == 1 && !pile.rig.Asleep(pile.body), "a sleeper on a crate that burnt down wakes the tick it goes");
    }
    // Keys: the door a keyhole takes with it when it fades.
    {
        Pile pile;
        Keys::State keys;
        keys.rules.fadeStartMs = 0.0;
        keys.rules.fadeMs = 100.0;
        Keys::Keyhole keyhole;
        keyhole.name = "keyhole";
        keyhole.colour = "yellow";
        keyhole.door = pile.wall;
        keyhole.unlocked = true;
        keys.keyholes.push_back(keyhole);
        Resting::Watch(pile.rig.registry);
        CHECK(keys.Tick(pile.rig.registry, {}, 1.0).size() == 1);
        const auto woken = Resting::Watch(pile.rig.registry);
        CHECK_MSG(woken.size() == 1 && !pile.rig.Asleep(pile.body), "a sleeper on a locked door wakes the tick the door goes");
    }
}

// ---- a mover that moves --------------------------------------------------------------------------------------------

// A lift, a platform or a door the port moves (Mover::MoveKinematic is what each of them calls) leaves the sleeper over it without it.
void ASleeperOverAMoverTheTickItMovesWakes() {
    for (const double dropPx : {2.0, 40.0}) {
        Rig rig;
        rig.Box("floor", Kind::Static, 100.0, 410.0, 600.0, 20.0);
        const entt::entity lift = rig.Box("lift", Kind::Mover, 100.0, 250.0, 60.0, 30.0);   // top at y 235
        const entt::entity body = rig.Sleeper("body", 100.0, 220.0, 30.0, 30.0);            // 205 to 235
        Resting::Watch(rig.registry);
        // Idle: the port writes the place it already has, and nothing happens.
        Mover::MoveKinematic(rig.registry, lift, rig.registry.get<TransformComponent>(lift).position, kTick);
        CHECK_MSG(Resting::Watch(rig.registry).empty() && rig.Asleep(body), "a lift that does not move leaves what is on it asleep");
        const double before = rig.YPx(body);
        Mover::MoveKinematic(rig.registry, lift, Units::ToWorld(100.0, 250.0 + dropPx), kTick);
        const auto woken = Resting::Watch(rig.registry);
        CHECK_MSG(woken.size() == 1 && woken[0].cause == Resting::Cause::Moved,
                  "a lift that moves " + std::to_string(dropPx) + " units wakes the sleeper on it, as a move");
        CHECK(!rig.Asleep(body));
        rig.Step();
        CHECK_MSG(rig.YPx(body) > before + 0.01, "and it falls in the step that follows");
    }
    // A platform's first call places it with no velocity (PlaceKinematic): a move all the same.
    {
        Rig rig;
        const entt::entity platform = rig.Box("platform", Kind::Mover, 100.0, 250.0, 60.0, 30.0);
        const entt::entity body = rig.Sleeper("body", 100.0, 220.0, 30.0, 30.0);
        Resting::Watch(rig.registry);
        Mover::PlaceKinematic(rig.registry, platform, Units::ToWorld(100.0, 230.0));
        CHECK_MSG(Resting::Watch(rig.registry).size() == 1 && !rig.Asleep(body), "a platform jumped a stride wakes what rests on it");
    }
    // A sleeper that is not on the mover is not touched by its moving.
    {
        Rig rig;
        rig.Box("floor", Kind::Static, 100.0, 310.0, 600.0, 20.0);
        const entt::entity lift = rig.Box("lift", Kind::Mover, 400.0, 250.0, 60.0, 30.0);
        const entt::entity body = rig.Sleeper("body", 100.0, 285.0, 30.0, 30.0);
        Resting::Watch(rig.registry);
        Mover::MoveKinematic(rig.registry, lift, Units::ToWorld(400.0, 200.0), kTick);
        CHECK_MSG(Resting::Watch(rig.registry).empty() && rig.Asleep(body), "a lift 300 units off moves and the sleeper sleeps on");
    }
    // The distance a support must be moved is the engine's own: a nudge below it is the same place.
    {
        Rig rig;
        const entt::entity platform = rig.Box("platform", Kind::Mover, 100.0, 250.0, 60.0, 30.0);
        const entt::entity body = rig.Sleeper("body", 100.0, 220.0, 30.0, 30.0);
        Resting::Watch(rig.registry);
        rig.registry.get<TransformComponent>(platform).position.y -= 0.00005f;
        CHECK_MSG(Resting::Watch(rig.registry).empty() && rig.Asleep(body), "a move of 0.00005 m is below moved_m");
        rig.registry.get<TransformComponent>(platform).position.y -= 0.0002f;
        CHECK_MSG(Resting::Watch(rig.registry).size() == 1 && !rig.Asleep(body), "and one of 0.00025 m is not");
    }
}

// A sleeping loose body the port moves (a portal's exit puts it somewhere else) leaves what lay on it, and so does an awake one, by
// WakeAround, which the portal calls before the move; an awake body that the solver itself moves is left to the engine.
void ALooseBodyThePortMovesLeavesWhatLiesOnIt() {
    {
        Rig rig;
        rig.Box("floor", Kind::Static, 100.0, 310.0, 600.0, 20.0);
        const entt::entity crate = rig.Sleeper("crate", 100.0, 280.0, 30.0, 30.0);   // 265 to 295 on the floor
        const entt::entity stone = rig.Sleeper("stone", 100.0, 250.0, 30.0, 30.0);   // 235 to 265 on the crate
        Resting::Watch(rig.registry);
        rig.registry.get<TransformComponent>(crate).position = Units::ToWorld(300.0, 200.0);
        const auto woken = Resting::Watch(rig.registry);
        CHECK_MSG(woken.size() == 1 && woken[0].sleeper == stone && woken[0].cause == Resting::Cause::Moved,
                  "a sleeping crate the port carries away wakes the stone on it");
    }
    {
        Rig rig;
        rig.Box("floor", Kind::Static, 100.0, 310.0, 600.0, 20.0);
        const entt::entity crate = rig.Box("crate", Kind::Loose, 100.0, 280.0, 30.0, 30.0);   // awake
        const entt::entity stone = rig.Sleeper("stone", 100.0, 250.0, 30.0, 30.0);
        Resting::Watch(rig.registry);
        rig.registry.get<TransformComponent>(crate).position.x += 0.01f;
        CHECK_MSG(Resting::Watch(rig.registry).empty() && rig.Asleep(stone),
                  "an awake crate the solver moves is the engine's to wake the stone by, so the watch leaves it");
        CHECK_MSG(Resting::WakeAround(rig.registry, crate) == 1 && !rig.Asleep(stone),
                  "but one the port is about to move wakes it through WakeAround");
        CHECK_MSG(Resting::WakeAround(rig.registry, crate) == 0, "and nothing is left to wake");
    }
}

// ---- no more than what it rests on ---------------------------------------------------------------------------------

void ASleeperSleepsOnWhenSomethingElseIsDestroyed() {
    Rig rig;
    rig.Box("floor", Kind::Static, 100.0, 310.0, 600.0, 20.0);
    const entt::entity wall = rig.Box("wall", Kind::Static, 100.0, 250.0, 60.0, 60.0);
    const entt::entity body = rig.Sleeper("body", 100.0, 205.0, 30.0, 30.0);
    const entt::entity apart = rig.Box("apart", Kind::Static, 100.0, 170.0, 60.0, 20.0);          // 160 to 180: 10 units above its top
    const entt::entity trigger = rig.Box("area", Kind::Trigger, 100.0, 205.0, 60.0, 60.0);        // overlaps it, and is a sensor
    const entt::entity side = rig.Box("side", Kind::Static, 140.0, 205.0, 30.0, 30.0);            // 125 to 155: 10 units off its right
    const entt::entity blind = rig.Box("blind", Kind::Static, 100.0, 230.0, 60.0, 20.0);           // 220 to 240: against its feet...
    rig.registry.get<BoxColliderComponent>(blind).collidesWith = 0;                                // ...and a body its masks do not meet
    Resting::Watch(rig.registry);
    const auto* supports = rig.registry.try_get<Resting::Supports>(body);
    CHECK(supports != nullptr);
    if (supports != nullptr) {
        for (const Resting::Support& support : supports->list) {
            CHECK_MSG(support.entity == wall, "the only thing it rests on is the wall, not " + support.name);
        }
    }
    for (const entt::entity gone : {apart, trigger, side, blind}) {
        rig.registry.destroy(gone);
        CHECK_MSG(Resting::Watch(rig.registry).empty() && rig.Asleep(body), "a body that is not its support goes and the sleeper sleeps on");
    }
    CHECK_MSG(std::fabs(rig.YPx(body) - 205.0) < 1e-4, "where it was");
    rig.registry.destroy(wall);
    CHECK_MSG(Resting::Watch(rig.registry).size() == 1, "and the wall's going is what wakes it");
}

// A support that stops being solid - the body kept, its collider made a sensor - is as gone as a destroyed one is (b2Fixture::SetSensor wakes the
// body, b2Fixture.cpp:220-227, and a sensor makes no manifold, so Box2D's contact with the sleeper is no longer one).
void ASupportThatBecomesASensorWakesWhatRestsOnIt() {
    Rig rig;
    const entt::entity wall = rig.Box("wall", Kind::Static, 100.0, 250.0, 60.0, 60.0);
    const entt::entity body = rig.Sleeper("body", 100.0, 205.0, 30.0, 30.0);
    Resting::Watch(rig.registry);
    rig.registry.get<BoxColliderComponent>(wall).isTrigger = true;
    const auto woken = Resting::Watch(rig.registry);
    CHECK_MSG(woken.size() == 1 && woken[0].cause == Resting::Cause::NotSolid && !rig.Asleep(body), "the sleeper wakes, as its support no longer being solid");
}

// What touches means is the rules' touch_units: two surfaces within it are in contact, and two beyond it are not.
void TouchingIsTheRulesSkin() {
    for (const double gapPx : {0.0, 0.4, 0.6, 3.0}) {
        Rig rig;
        const entt::entity wall = rig.Box("wall", Kind::Static, 100.0, 250.0, 60.0, 60.0);   // top at 220
        const entt::entity body = rig.Sleeper("body", 100.0, 205.0 - gapPx, 30.0, 30.0);
        Resting::Watch(rig.registry);
        rig.registry.destroy(wall);
        const bool woke = !Resting::Watch(rig.registry).empty();
        CHECK_MSG(woke == (gapPx <= 0.5), "a gap of " + std::to_string(gapPx) + " units against a touch of 0.5: " + (woke ? "woken" : "asleep"));
        (void)body;
    }
}

// ---- where the level places a sleeper ------------------------------------------------------------------------------

// 3-12's stone is placed 1.95 units off the crate's corner: a scan does not find the crate, so resting.json names it.
void ANamedSupportIsWhatAPlacedSleeperRestsOn() {
    const auto build = [](Rig& rig, entt::entity& crate, entt::entity& stone) {
        crate = rig.Box("crate", Kind::Loose, 100.0, 250.0, 30.0, 30.0);        // 235 to 265
        stone = rig.Box("stone", Kind::Loose, 100.0, 218.05, 30.0, 30.0);       // its bottom 233.05: 1.95 over the crate's top
    };
    Resting::Rules rules;
    rules.touchUnits = 0.5;
    rules.movedMetres = 0.0001;
    rules.byLevel["levelx"] = {"stone", "crate"};
    // Not named: the gap is more than a touch, and the stone's support is not found.
    {
        Rig rig;
        entt::entity crate = entt::null;
        entt::entity stone = entt::null;
        build(rig, crate, stone);
        std::string error;
        CHECK_MSG(Resting::Apply(rig.registry, rules, "levelx", {{"stone", stone}, {"crate", crate}}, error), error);
        Resting::Watch(rig.registry);
        CHECK_MSG(Resting::WakeAround(rig.registry, crate) == 0 && rig.Asleep(stone),
                  "a stone 1.95 units clear of its crate is not woken by the crate going, unless the table names it");
    }
    // Named: it is.
    {
        Rig rig;
        entt::entity crate = entt::null;
        entt::entity stone = entt::null;
        build(rig, crate, stone);
        Resting::Rules named = rules;
        named.restsOn["levelx"]["stone"] = {"crate"};
        std::string error;
        CHECK_MSG(Resting::Apply(rig.registry, named, "levelx", {{"stone", stone}, {"crate", crate}}, error), error);
        const auto* supports = rig.registry.try_get<Resting::Supports>(stone);
        CHECK_MSG(supports != nullptr && supports->list.size() == 1 && !supports->scanned, "the table's support is in its record before the first look");
        Resting::Watch(rig.registry);
        CHECK_MSG(rig.registry.get<Resting::Supports>(stone).list.size() == 1, "and the look adds nothing that is not touching");
        CHECK_MSG(Resting::Watch(rig.registry).empty() && rig.Asleep(stone), "so long as the crate stands it sleeps");
        rig.registry.destroy(crate);
        const auto woken = Resting::Watch(rig.registry);
        CHECK_MSG(woken.size() == 1 && woken[0].cause == Resting::Cause::Gone && !rig.Asleep(stone),
                  "the stone is woken the tick the crate goes");
        // A name the level did not build is refused: a typo must not leave a pile unwatched.
        Rig other;
        entt::entity c2 = entt::null;
        entt::entity s2 = entt::null;
        build(other, c2, s2);
        Resting::Rules typo = rules;
        typo.restsOn["levelx"]["stone"] = {"crat"};
        CHECK_MSG(!Resting::Apply(other.registry, typo, "levelx", {{"stone", s2}, {"crate", c2}}, error),
                  "a support the level did not build is refused");
    }
}

// ---- 3-10's knight ---------------------------------------------------------------------------------------------------

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
    const std::filesystem::path prisms = std::filesystem::temp_directory_path() / "supersonic-test-mp-wake";
    const bool ok = Game::LoadData(kLevels + "/" + name + ".tscn", kData, prisms, run.data, error) &&
                    Game::Start(run.data, run.registry, run.level, error);
    CHECK_MSG(ok, name + ": " + error);
    return ok;
}

ScriptClock::Step LayerStep() {
    ScriptClock::Rules rules;
    std::string error;
    CHECK_MSG(ScriptClock::LoadRules(kPortData + "/clock.json", rules, error), error);
    return ScriptClock::TickStep(rules, kTick);
}

// Knight B takes the key on tick 78, walks the breakable wall (x 256 to 384, y 134 to 166) and holds over it for 15 s from about tick 290.
// It is asleep by tick 400; the wall breaks, and the knight falls on the tick that follows.
void ThreeTensKnightFallsOnTheTickItsWallBreaks() {
    if (!LevelIsThere("level9b")) return;
    Started run;
    if (!Start("level9b", run)) return;
    const ScriptClock::Step step = LayerStep();
    for (int tick = 1; tick <= 400; ++tick) Game::Tick(run.data, run.registry, run.level, 0.0f, step);
    entt::entity knight = entt::null;
    for (const Minions::Minion& minion : run.level.minions.minions) {
        if (minion.gone || minion.body == entt::null || !run.registry.valid(minion.body)) continue;
        const glm::dvec2 at = Units::ToPixels(run.registry.get<TransformComponent>(minion.body).position);
        std::printf("  3-10 tick 400: %s at (%.1f, %.1f), %s\n", minion.name.c_str(), at.x, at.y,
                    run.registry.get<RigidBodyComponent>(minion.body).isSleeping ? "asleep" : "awake");
        if (at.x > 256.0 && at.x < 384.0 && at.y < 134.0) knight = minion.body;
    }
    CHECK_MSG(knight != entt::null, "a knight stands over the wall");
    if (knight == entt::null) return;
    auto& rigid = run.registry.get<RigidBodyComponent>(knight);
    CHECK_MSG(rigid.isSleeping, "and sleeps there");
    const double before = Units::ToPixels(run.registry.get<TransformComponent>(knight).position).y;
    for (int tick = 0; tick < 10; ++tick) Game::Tick(run.data, run.registry, run.level, 0.0f, step);
    CHECK_MSG(rigid.isSleeping, "ten ticks on, with the wall standing, it still does");
    const entt::entity gone = run.level.demolish.Break(run.registry, "breakable_wall_1316", "a test");
    CHECK_MSG(gone != entt::null, "the wall breaks");
    Game::Forget(run.level, gone);
    Game::Tick(run.data, run.registry, run.level, 0.0f, step);
    const double first = Units::ToPixels(run.registry.get<TransformComponent>(knight).position).y;
    CHECK_MSG(!rigid.isSleeping, "the knight is awake on the next tick");
    CHECK_MSG(first > before + 0.01, "and has fallen in that tick's own physics step: " + std::to_string(first - before));
    Game::Tick(run.data, run.registry, run.level, 0.0f, step);
    const double after = Units::ToPixels(run.registry.get<TransformComponent>(knight).position).y;
    std::printf("  3-10: the knight over the wall at y %.2f falls to %.2f one tick and to %.2f two ticks after the wall breaks\n", before,
                first, after);
    CHECK_MSG(after > first + 0.1, "and keeps falling: " + std::to_string(after - first));
}

// ---- the census ------------------------------------------------------------------------------------------------------

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

// What can go or move in a level, by its tag: a breakable wall, a crate or barrel that burns or blows, a door, a lift or platform, a body that is
// killed or carried (a knight, a stone, a key).
bool CanGo(const std::string& tag) {
    // A plain "platform" is a static floor; the moving ones are "moving_platform", the lifts "lift", the bobbing slabs "bounce".
    for (const char* word : {"breakable", "crate", "barrel", "bomb", "door", "lift", "moving_platform", "bounce", "minion", "stone", "ghost", "knight"}) {
        if (tag.find(word) != std::string::npos) return true;
    }
    return false;
}

// Every level, idle for 420 ticks as test_mp_timebase's creep census runs them: what the watch wakes (none is expected without a play, and the levels
// that do show it are named) and, at tick 420, which sleepers rest on something that can go or move - the levels the watch can change by a play.
void TheLevelsRestingCensus() {
    const std::vector<std::string> levels = ConvertedLevels();
    if (levels.size() < 100 || !LevelIsThere("level0")) {
        std::printf("  TheLevelsRestingCensus SKIPPED - needs the converted levels at %s\n", kLevels.c_str());
        return;
    }
    const ScriptClock::Step step = LayerStep();
    int sleepers = 0;
    int onSomethingThatCanGo = 0;
    int wakes = 0;
    for (const std::string& name : levels) {
        Started run;
        if (!Start(name, run)) continue;
        for (int tick = 1; tick <= 420; ++tick) {
            Game::Tick(run.data, run.registry, run.level, 0.0f, step);
            for (const Resting::Woken& woken : run.level.restingWoken) {
                ++wakes;
                std::printf("  %s tick %d: %s woken, its support %s %s\n", name.c_str(), tick, woken.sleeperName.c_str(),
                            woken.supportName.c_str(), Resting::CauseName(woken.cause));
            }
        }
        for (auto [entity, rigid] : run.registry.view<RigidBodyComponent>().each()) {
            if (!rigid.isSleeping || rigid.isKinematic) continue;
            ++sleepers;
            const auto* supports = run.registry.try_get<Resting::Supports>(entity);
            const auto* tag = run.registry.try_get<TagComponent>(entity);
            if (supports == nullptr || tag == nullptr) continue;
            std::string goes;
            for (const Resting::Support& support : supports->list) {
                if (CanGo(support.name)) goes += (goes.empty() ? "" : ", ") + support.name;
            }
            if (goes.empty()) continue;
            ++onSomethingThatCanGo;
            std::printf("  %s: %s asleep on %s\n", name.c_str(), tag->tag.c_str(), goes.c_str());
        }
    }
    std::printf("  %zu levels idle for 420 ticks: %d sleepers at the end, %d of them on something that can go or move, %d woken by the watch\n",
                levels.size(), sleepers, onSomethingThatCanGo, wakes);
    CHECK_MSG(sleepers > 0, "some bodies are asleep at tick 420");
}

void runTests() {
    ASleeperOverAWallThatIsDestroyedWakesAndFallsTheSameTick();
    TheModulesThatRemoveASupportWakeWhatRestsOnIt();
    ASleeperOverAMoverTheTickItMovesWakes();
    ALooseBodyThePortMovesLeavesWhatLiesOnIt();
    ASleeperSleepsOnWhenSomethingElseIsDestroyed();
    ASupportThatBecomesASensorWakesWhatRestsOnIt();
    TouchingIsTheRulesSkin();
    ANamedSupportIsWhatAPlacedSleeperRestsOn();
    ThreeTensKnightFallsOnTheTickItsWallBreaks();
    TheLevelsRestingCensus();
}

} // namespace

MP_TEST_MAIN("test_mp_wake", 60)
