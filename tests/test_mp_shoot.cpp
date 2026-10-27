// What a knight does once it has seen: it shoots (planning step 159, G1b).
//
// ETHCallback_minion calls addEnemyShoot when its gate opens (G1a built the gate: test_mp_sight). The shot is an enemy_shoot.ent
// with no body: it starts 16 units out toward where the sighted entity stood, flies 120 units a second of the script's clock,
// and is judged by the entity it was aimed at ALONE, looked up by its id - within 16 units of it (strict) the player has hp set
// to 0 and a portal is killPortal'd, and the shot is destroyed; a target that is gone leaves it flying on to its destiny, where
// it also ends once it has gone farther than that. Contact with the player caps a knight's own hold at 1000 ms, on the edge
// the contact begins. A rolling stone that begins to touch a knight hard enough, and a barrel's blast, kill it.
//
// What is pinned is the DECODE (out/parity/program/G1b/decode.md; minions.json `shot`, `contact` and `crush` cite every number):
//   the shot    nudged 16 units out, flown at 120 a second of the script's clock from the tick after it is made, ended by
//               its range (a strict compare of squared distances) or by its stored target
//   the target  the player or a portal by id, and only that one: the player in the way of a shot at a portal is not hurt
//   the order   the player before a portal inside one bucket, the nearer bucket before the farther (G1a-1)
//   the cap     min(holdTime, 1000) on the begin of a contact with the player's body, not while it lasts, minion.ent alone
//   the deaths  a stone's crush above hitIntensity 1.23, and a barrel's blast through the same one-tick-later burn a flame has
//
// Its rig cases need no data and are the suite test_mp_shoot; its level cases read the converted levels and Enhanced's data from outside this repository and are the
// suite test_mp_shoot_levels, which exits 77 (skipped) when either is absent (the end of this file says why there are two).

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "core/Components.hpp"
#include "sim/Game.hpp"
#include "sim/Ghost.hpp"
#include "sim/Minions.hpp"
#include "sim/ScriptClock.hpp"
#include "sim/Units.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <filesystem>
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
constexpr float kStep = 1.0f / 60.0f;

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

entt::entity Box(entt::registry& registry, const glm::dvec2& centrePx, const glm::dvec2& sizePx) {
    const entt::entity entity = registry.create();
    registry.emplace<TransformComponent>(entity).position = Units::ToWorld(centrePx.x, centrePx.y);
    auto& box = registry.emplace<BoxColliderComponent>(entity);
    box.size = glm::vec3(Units::ToMetres(sizePx.x), Units::ToMetres(sizePx.y), 1.0f);
    return entity;
}

// ---- the numbers ------------------------------------------------------------------------------------------

void TheShotIsTheDecodedNumbersAndTheGhostsToo() {
    const Minions::Rules rules = LoadRules();
    const Minions::ShotRules& shot = rules.shot;
    CHECK_MSG(shot.entityName == "enemy_shoot.ent", "the entity addEnemyShoot adds (#38)");
    CHECK_MSG(shot.speedPxS == 120.0, "AddToPositionXY(dir x scaledUnitsPerSecond(120)) (#150-162)");
    CHECK_MSG(shot.hitPx == 16.0, "fakeRadius = scale(16) (#239-245)");
    CHECK_MSG(shot.nudgePx == 16.0, "AddToPositionXY(scale(direction x 16)) on the first call (#29-55)");
    CHECK_MSG(shot.firstCell == 0 && shot.lastCell == 5 && shot.cellStrideMs == 30.0, "_projectileFrameTimer.set(0, 5, 30, true)");
    CHECK_MSG(rules.contact.holdCapMs == 1000.0, "min(holdTime, 1000) (ETHBeginContactCallback_minion #30-55)");
    CHECK_MSG(std::fabs(rules.contact.touchUnits - 0.5 / 2.8125) < 1e-9, "0.5 screen px at R12's 720 px screen");
    CHECK_MSG(rules.crush.entity == "rolling_stone.ent" && rules.crush.hitAbove == 1.23,
              "a stone's crush is above hitIntensity 1.23 (ETHBeginContactCallback_rolling_stone #139-140)");

    // The ghost calls the same addEnemyShoot and ETHCallback_enemy_shoot: the two files hold the same numbers.
    Ghost::Rules ghost;
    std::string error;
    CHECK_MSG(Ghost::LoadRules(kPortData + "/ghost.json", ghost, error), error);
    CHECK_MSG(ghost.shotSpeedPxS == shot.speedPxS && ghost.shotHitPx == shot.hitPx && ghost.shotNudgePx == shot.nudgePx &&
                  ghost.shotFirstCell == shot.firstCell && ghost.shotLastCell == shot.lastCell &&
                  ghost.shotCellStrideMs == shot.cellStrideMs,
              "ghost.json's shot block is the same entity's, number for number");
}

// ---- the shot ----------------------------------------------------------------------------------------------

struct Rig {
    Minions::Rules rules = LoadRules();
    Minions::State state;
    entt::registry registry;
    Minions::Senses senses;
    // 16 whole script milliseconds a tick, a 1/60 s engine tick, the original's 1/30 s physics frame: SimRatio 2.
    ScriptClock::Step step{0.016, 1.0f / 60.0f, 1.0 / 30.0, 60.0};
    int tick = 0;
    std::vector<int> shotTicks;
    std::vector<int> endedTicks;
    std::vector<int> killedPlayerTicks;
    std::vector<std::pair<int, int>> killedPortals; // (tick, id)

    Rig() { state.rules = rules; }

    // A knight whose ENTITY point is at (x, y) serving a 2000 ms hold, facing right.
    void Knight(double x, double y, double holdMs = 2000.0) {
        std::vector<Minions::Waypoint> patrol(2);
        patrol[0].name = "wayA0";
        patrol[0].atPx = glm::dvec2(x + 100.0, y);
        patrol[0].holdMs = holdMs;
        patrol[1].name = "wayA1";
        patrol[1].atPx = glm::dvec2(x - 100.0, y);
        patrol[1].holdMs = holdMs;
        const entt::entity body = state.Summon(registry, "knight", glm::dvec2(x, y + rules.EntityAboveCentrePx()), patrol);
        CHECK(body != entt::null);
        state.minions.back().holdMs = holdMs;
        state.minions.back().moveVec = glm::dvec2(1.0, 0.0);
    }

    void Player(double x, double y) {
        senses.playerPresent = true;
        senses.playerPx = glm::dvec2(x, y);
    }

    // One tick as the layer plays it: the knights' tick before the step, the shots after it.
    void Run(int ticks) {
        for (int i = 0; i < ticks; ++i) {
            ++tick;
            state.Tick(registry, step, senses);
            if (!state.shotsDue.empty()) shotTicks.push_back(tick);
            const bool wasKilled = state.playerKilled;
            const std::size_t endedBefore = static_cast<std::size_t>(state.shotsEnded);
            state.FlyShots(step, senses);
            for (std::size_t e = endedBefore; e < static_cast<std::size_t>(state.shotsEnded); ++e) endedTicks.push_back(tick);
            if (state.playerKilled && !wasKilled) killedPlayerTicks.push_back(tick);
            for (const int id : state.killedPortalIds) killedPortals.emplace_back(tick, id);
        }
    }
    Minions::Minion& K() { return state.minions.front(); }
};

// The first shot is due on tick 126 (shootElapsedTime past 2000), made nudged 16 units out toward where the player stood.
void AKnightsShotStartsNudgedTowardWhereThePlayerStood() {
    Rig rig;
    rig.Knight(300.0, 64.0);
    rig.Player(381.0, 64.0);
    rig.Run(126);
    CHECK_MSG(rig.shotTicks.size() == 1 && rig.shotTicks[0] == 126, "the gate opens on tick 126, as test_mp_sight pins");
    CHECK_MSG(rig.state.shotsFired == 1 && rig.state.shots.size() == 1, "and one shot is in the air");
    if (rig.state.shots.empty()) return;
    const Minions::EnemyShot& shot = rig.state.shots.front();
    CHECK_MSG(shot.id == 1 && shot.minion == 0, "the first of the level, the first knight's");
    // The knight's entity point is its float body's centre less 11.1: within a thousandth of a unit of the waypoint's (300, 64).
    CHECK_MSG(glm::length(shot.fromPx - glm::dvec2(300.0, 64.0)) < 1e-3 && shot.destinyPx == glm::dvec2(381.0, 64.0),
              "from the knight's entity point to the player's (initialPos and destiny)");
    CHECK_MSG(glm::length(shot.directionPx - glm::dvec2(1.0, 0.0)) < 1e-3 &&
                  glm::length(shot.velocityPx - glm::dvec2(120.0, 0.0)) < 0.1,
              "its direction is normalize(destiny - casterPos), its speed 120");
    CHECK_MSG(glm::length(shot.atPx - glm::dvec2(316.0, 64.0)) < 1e-3, "nudged 16 units along its way as it is made");
    CHECK_MSG(shot.targetName == "dark_mage.ent" && shot.targetPortalId == -1, "aimed at the player");
    CHECK_MSG(!shot.fresh && rig.state.shotsDue.front().portalId == -1,
              "and it has not flown: the tick it was made on only clears its mark, its first callback is the next frame's");
}

// 120 units a second of the script's clock: 1.92 a tick of 16 ms, from the tick after it is made; it kills the player on the 26th.
void TheShotFliesOnTheScriptClockAndKillsThePlayerItIsAimedAt() {
    Rig rig;
    rig.Knight(300.0, 64.0);
    rig.Player(381.0, 64.0);
    rig.Run(126);
    rig.Run(1);
    CHECK_MSG(std::fabs(rig.state.shots.front().atPx.x - 317.92) < 1e-9, "one 16 ms tick on: 316 + 1.92");
    rig.Run(24); // 25 flights: 316 + 48 = 364, 17 from the player: not within 16
    CHECK_MSG(std::fabs(rig.state.shots.front().atPx.x - 364.0) < 1e-6 && !rig.state.playerKilled,
              "17 units from the player after 25 flights, and he is not hurt");
    rig.Run(1);
    CHECK_MSG(rig.killedPlayerTicks.size() == 1 && rig.killedPlayerTicks[0] == 126 + 26,
              "the 26th flight is 15.08 from him: within 16 (strict): killed on tick 152");
    CHECK_MSG(rig.state.shots.empty() && rig.state.shotsEnded == 1 && rig.state.spentPx.size() == 1,
              "and the shot is spent where it stood");
    if (!rig.state.spentPx.empty()) CHECK(std::fabs(rig.state.spentPx.front().x - 365.92) < 1e-9);
    CHECK_MSG(rig.state.killedBy.find("knight") != std::string::npos && rig.state.killedPortalIds.empty(),
              "named for its knight, and it killed no portal");
    // Judged once: nothing in the air, nothing more to report.
    rig.Run(1);
    CHECK(rig.state.spentPx.empty());
}

// A target that has moved off is simply not within 16: the shot flies on to its destiny and ends the tick it has gone farther.
void AShotAtAPlayerWhoHasGoneFliesOnToItsDestiny() {
    Rig rig;
    rig.Knight(300.0, 64.0);
    rig.Player(381.0, 64.0);
    rig.Run(126);
    rig.Player(381.0, 300.0); // another row, far from the line
    rig.Run(33);              // 33 flights: 379.36 from 300 is 79.36, inside the 81
    CHECK_MSG(rig.state.shots.size() == 1, "still flying 33 flights on");
    rig.Run(1);
    CHECK_MSG(rig.state.shots.empty() && rig.endedTicks.size() == 1 && rig.endedTicks[0] == 126 + 34,
              "the 34th has gone farther than its destiny (81 units): ended on tick 160");
    CHECK_MSG(std::fabs(rig.state.spentPx.front().x - 381.28) < 1e-9, "where it stands, past the destiny: 381.28");
    CHECK_MSG(!rig.state.playerKilled && rig.killedPlayerTicks.empty(), "and nobody is hurt");
}

// Only the STORED target is judged: aimed at a portal, a shot passes the player standing in its way, and kills the portal.
void AShotAtAPortalKillsThePortalAndNotThePlayerInItsWay() {
    Rig rig;
    rig.Knight(300.0, 64.0);
    rig.senses.canSeeAnything = false; // blind: 'portal.ent' is the only target
    rig.Player(350.0, 64.0);           // standing in the line of fire
    rig.senses.portalsPx = {glm::dvec2(381.0, 64.0)};
    rig.senses.portalIds = {7};
    rig.Run(126);
    CHECK_MSG(rig.state.shots.size() == 1 && rig.state.shots.front().targetName == "portal.ent" &&
                  rig.state.shots.front().targetPortalId == 7,
              "aimed at portal 7, by its id");
    rig.Run(1 + 25);
    CHECK_MSG(rig.killedPortals.size() == 1 && rig.killedPortals[0].first == 152 && rig.killedPortals[0].second == 7,
              "killPortal on portal 7 on the 26th flight");
    CHECK_MSG(!rig.state.playerKilled && rig.killedPlayerTicks.empty(),
              "the player it flew past at 350 is not its target and is not hurt");
    CHECK_MSG(rig.state.shots.empty() && rig.state.shotsEnded == 1, "and the shot is spent with it");
}

// A portal that is gone by the time the shot arrives is not found: the shot flies on to its destiny and ends there.
void AShotWhoseTargetIsGoneFliesOn() {
    Rig rig;
    rig.Knight(300.0, 64.0);
    rig.senses.canSeeAnything = false;
    rig.Player(350.0, 64.0);
    rig.senses.portalsPx = {glm::dvec2(381.0, 64.0)};
    rig.senses.portalIds = {7};
    rig.Run(126);
    rig.senses.portalsPx.clear(); // spent, recycled, killed by something else
    rig.senses.portalIds.clear();
    rig.Run(34);
    CHECK_MSG(rig.killedPortals.empty(), "nothing to kill");
    CHECK_MSG(rig.state.shots.empty() && rig.endedTicks.size() == 1 && rig.endedTicks[0] == 126 + 34 && !rig.state.playerKilled,
              "it ends at its range on the 34th flight, and the player under it is not its target");
    // A different portal at the same spot is a different entity: a new id is not the stored one.
    Rig other;
    other.Knight(300.0, 64.0);
    other.senses.canSeeAnything = false;
    other.Player(350.0, 64.0);
    other.senses.portalsPx = {glm::dvec2(381.0, 64.0)};
    other.senses.portalIds = {7};
    other.Run(126);
    other.senses.portalIds = {8};
    other.Run(34);
    CHECK_MSG(other.killedPortals.empty(), "portal 8 is not portal 7");
}

// A knight that keeps the player in sight shoots again 126 ticks later (2000 ms of shootElapsedTime), one that has lost him does not.
void EveryShotIsCountedAndNamed() {
    Rig rig;
    rig.Knight(300.0, 64.0);
    rig.Player(381.0, 300.0); // another row: it never sees him
    rig.Run(300);
    CHECK_MSG(rig.state.shotsFired == 0 && rig.state.shotsMade == 0 && rig.state.shots.empty(), "no sight, no shot");
    Rig keeps;
    keeps.Knight(300.0, 64.0);
    keeps.Player(381.0, 64.0);
    keeps.senses.finished = false;
    keeps.Run(252);
    CHECK_MSG(keeps.state.shotsFired == 2 && keeps.state.shotsMade == 2 && keeps.shotTicks == std::vector<int>({126, 252}),
              "126 and 252: one shot a 2000 ms interval, counted and numbered from 1");
    CHECK_MSG(keeps.state.shotsEnded == 1, "the first has killed him by then (tick 152): one ended");
}

// ---- the contact's hold cap --------------------------------------------------------------------------------

void TheContactCapsAKnightsHoldOnItsBeginOnly() {
    Rig rig;
    rig.Knight(300.0, 64.0);
    // The knight's body is a circle of 7 about 11.1 under its entity point; a box of the player's size touching its side.
    const double bodyY = 64.0 + rig.rules.EntityAboveCentrePx();
    const entt::entity player = Box(rig.registry, glm::dvec2(340.0, bodyY), glm::dvec2(18.0, 36.0));
    rig.senses.playerBody = player;
    rig.Player(340.0, bodyY);
    const auto moveTo = [&](double x) {
        rig.registry.get<TransformComponent>(player).position = Units::ToWorld(x, bodyY);
    };
    rig.K().holdMs = 5000.0;
    rig.Run(1);
    CHECK_MSG(rig.K().holdMs == 5000.0 && !rig.K().touchingPlayer, "apart (the gap is 24): nothing is capped");
    moveTo(316.0); // 300 + 7 + 9: the surfaces meet
    rig.Run(1);
    CHECK_MSG(rig.K().touchingPlayer && rig.K().holdMs == 1000.0, "a contact that begins caps the hold at min(5000, 1000)");
    rig.K().holdMs = 5000.0; // adopted at an arrival while the two still touch
    rig.Run(3);
    CHECK_MSG(rig.K().holdMs == 5000.0, "and a contact that goes on does not cap it again: only the BEGIN does");
    moveTo(340.0);
    rig.Run(1);
    CHECK(!rig.K().touchingPlayer);
    moveTo(316.0);
    rig.Run(1);
    CHECK_MSG(rig.K().holdMs == 1000.0, "a new begin caps again");
    rig.K().holdMs = 400.0;
    moveTo(340.0);
    rig.Run(1);
    moveTo(316.0);
    rig.Run(1);
    CHECK_MSG(rig.K().holdMs == 400.0, "min(): a hold already under the cap is left as it is");

    // minion.ent's callback alone: the black knight has no ETHBeginContactCallback_ of its own.
    Rig black;
    black.Knight(300.0, 64.0);
    black.state.minions.front().entity = "ghost_minion.ent";
    const entt::entity blackPlayer = Box(black.registry, glm::dvec2(316.0, bodyY), glm::dvec2(18.0, 36.0));
    black.senses.playerBody = blackPlayer;
    black.Player(316.0, bodyY);
    black.K().holdMs = 5000.0;
    black.Run(2);
    CHECK_MSG(black.K().holdMs == 5000.0, "ghost_minion.ent is not capped");
}

// ---- which one it sees first -------------------------------------------------------------------------------

// ETHBucketManager: Add pushes BACK (ETHScene.cpp:277); MoveEntity pushes FRONT for an ET_HORIZONTAL entity, and the player and
// portal.ent are both type 0. So inside a bucket the player (added at the start, or moved in) comes before a portal added later,
// older portals before newer; and the walk is the agent's bucket, then the next, then the one after.
void TheTargetIsTheFirstInTheBucketsOrder() {
    const Minions::Rules rules = LoadRules();
    entt::registry registry;
    const glm::dvec2 knight(300.0, 64.0); // bucket (2, 0)
    const glm::dvec2 facing(1.0, 0.0);
    Minions::Senses senses;
    senses.playerPresent = true;

    senses.playerPx = glm::dvec2(330.0, 64.0);
    senses.portalsPx = {glm::dvec2(320.0, 64.0), glm::dvec2(340.0, 64.0)};
    senses.portalIds = {3, 4};
    auto seen = Minions::Look(registry, rules, knight, facing, senses, {});
    CHECK_MSG(seen && seen->name == "dark_mage.ent", "both in the knight's own bucket: the player first");

    senses.canSeeAnything = false;
    seen = Minions::Look(registry, rules, knight, facing, senses, {});
    CHECK_MSG(seen && seen->portalIndex == 0, "blind: the older portal of two in one bucket");
    senses.canSeeAnything = true;

    // A portal in the knight's own bucket beats a player in the next one: the walk is by bucket first.
    senses.playerPx = glm::dvec2(400.0, 64.0);
    senses.portalsPx = {glm::dvec2(320.0, 64.0)};
    senses.portalIds = {3};
    seen = Minions::Look(registry, rules, knight, facing, senses, {});
    CHECK_MSG(seen && seen->name == "portal.ent", "the portal in its own bucket before the player in the next");
    // And a player in the near bucket beats a portal in the far one.
    senses.playerPx = glm::dvec2(330.0, 64.0);
    senses.portalsPx = {glm::dvec2(500.0, 64.0)};
    seen = Minions::Look(registry, rules, knight, facing, senses, {});
    CHECK_MSG(seen && seen->name == "dark_mage.ent", "the player in the knight's bucket before a portal two along");
}

// ---- the deaths --------------------------------------------------------------------------------------------

struct Loaded {
    Game::Data data;
    entt::registry registry;
    Game::Level level;
    ScriptClock::Step step;
};

bool Begin(const std::string& levelName, Loaded& run) {
    std::string error;
    const bool ok = Game::LoadData(kLevels + "/" + levelName + ".tscn", kData,
                                   std::filesystem::temp_directory_path() / "supersonic-test-mp-shoot", run.data, error) &&
                    Game::Start(run.data, run.registry, run.level, error);
    CHECK_MSG(ok, levelName + ": " + error);
    ScriptClock::Rules clock;
    CHECK_MSG(ScriptClock::LoadRules(kPortData + "/clock.json", clock, error), error);
    run.step = ScriptClock::TickStep(clock, kStep);
    return ok;
}

void Tick(Loaded& run, int ticks) {
    for (int i = 0; i < ticks; ++i) Game::Tick(run.data, run.registry, run.level, 0.0f, run.step);
}

// A rolling stone's crush: Game::Crush takes the knights it is handed as a strike does - destroyed, counted, its blood recorded.
void AStonesCrushTakesTheKnight() {
    Loaded run;
    if (!Begin("level1b", run)) return; // 3-02: two knights
    Minions::State& state = run.level.minions;
    CHECK_MSG(state.minions.size() == 2, "3-02 has two knights");
    const entt::entity first = state.minions.front().body;
    const int before = state.taken;
    Tick(run, 2);
    Game::Crush(run.registry, run.level, {first});
    CHECK_MSG(state.minions.front().gone && state.taken == before + 1, "the first is destroyed and counted");
    CHECK_MSG(!run.registry.valid(first), "its body is gone from the world");
    CHECK_MSG(state.SpotCount() == 0 && !state.minions.back().gone, "the other stands");
    CHECK_MSG(Minions::Crushes(state.rules, 1.24) && !Minions::Crushes(state.rules, 1.23) && !Minions::Crushes(state.rules, 0.0),
              "above 1.23, never at it (CMPIf, JNP skips)");
}

// A barrel's blast reaches a knight in its radius with a clear ray (explode: destroy and burn what the grabber takes), and the
// knight's callback reads both on its next frame: the flame's one-tick-later burn.
void ABlastKillsAKnightInItsReach() {
    if (!LevelsPresent()) return;
    Loaded run;
    if (!Begin("level18b", run)) return; // 3-19: five barrel bombs and a knight
    Fire::State& fire = run.level.fire;
    Minions::State& state = run.level.minions;
    if (fire.bombs.empty() || state.minions.empty()) {
        CHECK_MSG(false, "3-19 has bombs and a knight");
        return;
    }
    Fire::Bomb& bomb = fire.bombs.front();
    const glm::dvec2 bombPx = Units::ToPixels(run.registry.get<TransformComponent>(bomb.body).position);
    // The knight put 40 units to the bomb's side, at rest.
    Minions::Minion& knight = state.minions.front();
    {
        auto& transform = run.registry.get<TransformComponent>(knight.body);
        const glm::vec3 at = Units::ToWorld(bombPx.x + 40.0, bombPx.y);
        transform.position.x = at.x;
        transform.position.y = at.y;
    }
    run.registry.get<RigidBodyComponent>(knight.body).velocity = glm::vec3(0.0f);
    bomb.requested = true;
    int gone = -1;
    for (int tick = 1; tick <= 10 && gone < 0; ++tick) {
        Tick(run, 1);
        if (knight.gone) gone = tick;
    }
    CHECK_MSG(fire.blasts >= 1, "the bomb went off");
    CHECK_MSG(gone > 0 && state.burned == 1, "and the knight was burned by it: gone on tick " + std::to_string(gone));
}

// The cap on a real level: knight B of 3-16 holds 5000 ms at wayB0; the player put against it releases it within a second.
void ThePlayersTouchReleasesAKnightFromItsLongHold() {
    if (!LevelsPresent()) return;
    Loaded run;
    if (!Begin("level15b", run)) return;
    Minions::State& state = run.level.minions;
    const Minions::Minion* knight = state.FindMinion("minion_spawn_913");
    CHECK(knight != nullptr);
    if (knight == nullptr) return;
    Tick(run, 200); // it has walked to wayB0 (302) and is serving its 5000 ms
    CHECK_MSG(knight->arm == Minions::Arm::Hold && knight->holdMs == 5000.0 && !knight->touchingPlayer,
              "holding the 5000 ms of wayB0, the player 230 units away");
    const glm::dvec2 knightPx = Units::ToPixels(run.registry.get<TransformComponent>(knight->body).position);
    {
        auto& transform = run.registry.get<TransformComponent>(run.level.player);
        const glm::vec3 at = Units::ToWorld(knightPx.x - 16.0, knightPx.y); // 7 + 9.1 apart is touching
        transform.position.x = at.x;
        transform.position.y = at.y;
        run.registry.get<RigidBodyComponent>(run.level.player).velocity = glm::vec3(0.0f);
    }
    Tick(run, 1);
    CHECK_MSG(knight->touchingPlayer && knight->holdMs == 1000.0,
              "the begin of the contact capped its hold: " + std::to_string(knight->holdMs));
    CHECK_MSG(knight->arm == Minions::Arm::Walk, "and its dwell (over 2 s) is past the cap: it walks on");
}

// The whole of it on a real level: 3-16, the player left standing. The knight sees him, shoots, and the shot kills him.
void ALeftStandingPlayerIsShotOnTheRealLevel() {
    if (!LevelsPresent()) return;
    Loaded run;
    if (!Begin("level15b", run)) return;
    Minions::State& state = run.level.minions;
    int firstShot = -1;
    int died = -1;
    for (int tick = 1; tick <= 900 && died < 0; ++tick) {
        Tick(run, 1);
        if (firstShot < 0 && state.shotsFired > 0) firstShot = tick;
        if (run.level.hazards.playerDied) died = tick;
    }
    std::printf("  3-16, the player left standing: first shot on tick %d, the player dead on tick %d (%s)\n", firstShot, died,
                run.level.hazards.killedBy.c_str());
    CHECK_MSG(firstShot > 0 && died > firstShot, "a shot is fired, and it kills him");
    CHECK_MSG(died - firstShot < 150, "inside the ~1.8 s of its flight");
    CHECK_MSG(run.level.hazards.killedBy == state.killedBy && !state.killedBy.empty(),
              "through the hazards' own funnel, named for the knight that shot");
}

// A knight that sees a PORTAL shoots it and kills it, and the port's portal list loses it by id, with its budget used.
void AKnightKillsThePortalItSees() {
    if (!LevelsPresent()) return;
    Loaded run;
    if (!Begin("level15b", run)) return;
    Portals::State& portals = run.level.portals;
    // Two portals in the knight's row, left of its second waypoint: the nearer bucket's one is what it faces first.
    CHECK(portals.TryPlace(glm::dvec2(150.0, 208.0)));
    const int id = portals.placed.back().id;
    const int usedBefore = portals.portalsUsed;
    int killedAt = -1;
    for (int tick = 1; tick <= 900 && killedAt < 0; ++tick) {
        Tick(run, 1);
        const bool still = std::any_of(portals.placed.begin(), portals.placed.end(),
                                       [id](const Portals::Placed& placed) { return placed.id == id; });
        if (!still) killedAt = tick;
    }
    std::printf("  3-16, a portal at (150, 208): killed on tick %d\n", killedAt);
    // The portal is in bucket 1 and the player in bucket 0: a knight facing left walks its own bucket, then the next, then the one after, so
    // it is the portal that is in sight first and the portal its first shot is aimed at (the knight fires on 412 with the player left standing).
    CHECK_MSG(killedAt > 412 && killedAt < 500 && !run.level.hazards.playerDied,
              "the portal is killed by the first shot, and the player it flew past is not: tick " + std::to_string(killedAt));
    CHECK_MSG(portals.portalsUsed == usedBefore, "killPortal refunds nothing and spends nothing: the count is as it was");
    CHECK_MSG(portals.placed.empty(), "and it has left the pair");
}

// This file holds TWO suites (planning step 209, PORT-11; the rows PORT5-7 and APPLE-22). It was one suite, test_mp_shoot, whose level cases skipped themselves without the converted
// levels and still returned its summary: with the owner's data absent it ran 72 of its 100 checks, fell under its floor of 90 and FAILED, where a suite that cannot run says so.
// The nine rig cases need no data and stay in test_mp_shoot (72 checks); the five level cases are test_mp_shoot_levels (28 checks), which returns 77 before its first check when
// the levels are absent, and ctest reports it SKIPPED, never passed (SKIPS_WITH_77 in tests/CMakeLists.txt). With the data present the two run 72 + 28 = 100 checks, as the
// one suite did. Each floor is its own: no floor was lowered to let a skip pass, and a suite that runs fewer checks than its floor still fails.
void runRigCases() {
    TheShotIsTheDecodedNumbersAndTheGhostsToo();
    AKnightsShotStartsNudgedTowardWhereThePlayerStood();
    TheShotFliesOnTheScriptClockAndKillsThePlayerItIsAimedAt();
    AShotAtAPlayerWhoHasGoneFliesOnToItsDestiny();
    AShotAtAPortalKillsThePortalAndNotThePlayerInItsWay();
    AShotWhoseTargetIsGoneFliesOn();
    EveryShotIsCountedAndNamed();
    TheContactCapsAKnightsHoldOnItsBeginOnly();
    TheTargetIsTheFirstInTheBucketsOrder();
}

void runLevelCases() {
    AStonesCrushTakesTheKnight();
    ABlastKillsAKnightInItsReach();
    ThePlayersTouchReleasesAKnightFromItsLongHold();
    ALeftStandingPlayerIsShotOnTheRealLevel();
    AKnightKillsThePortalItSees();
}

// The floors: about 90 % of each suite's own count (72 and 28), as the harness asks ("well below the real count, so a handful of checks added or removed does not trip it").
constexpr int kRigChecksAtLeast = 65;
constexpr int kLevelChecksAtLeast = 25;
constexpr int kSkipped = 77; // the suites' own code for "skipped": SKIPS_WITH_77, ctest's SKIP_RETURN_CODE (tests/test_mp.cpp)

int rigEntry(int, char**) {
    runRigCases();
    return ::test::summary("test_mp_shoot", kRigChecksAtLeast);
}

int levelEntry(int, char**) {
    if (!LevelsPresent()) {
        std::printf("test_mp_shoot_levels: SKIPPED - needs the converted levels at %s and Enhanced's data at %s (no check was run)\n", kLevels.c_str(), kData.c_str());
        return kSkipped;
    }
    runLevelCases();
    return ::test::summary("test_mp_shoot_levels", kLevelChecksAtLeast);
}

const ::MpSuite::Registration rigRegistration("test_mp_shoot", &rigEntry);
const ::MpSuite::Registration levelRegistration("test_mp_shoot_levels", &levelEntry);

} // namespace
