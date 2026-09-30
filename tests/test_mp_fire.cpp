// Fire agents, burning crates and barrel bombs.
//
// What is pinned here is the DECODE, and it disagrees with the Godot project in every
// number. Fire.hpp cites where each comes from; the three that a test can tell
// apart are:
//
//   reach   32, the fire agent's GetSize().x, centre to centre. The Godot project reads
//           the entity's 38 x 38 collision box and halves it to 24, so a crate
//           28 px away burns in the original and does not in the Godot project. That
//           exact distance is tested, because it is the only one that separates
//           the two readings.
//   burn    1000 ms before a crate is taken away; the Godot project invents 2000.
//   blast   80 px, against real colliders; the Godot project invents 96.
//
// And one thing that is not a number: fire kills the player, a blast does not.
// The blast's filter is isBreakableOrExplosiveOrBurnable, which excludes
// characters, and no main_char placement carries any of those flags - so although
// the bomb passes killPlayer true, the player is never in the grabbed set. Both
// directions are tested, because that asymmetry is exactly what a port smooths
// away without noticing.
//
// And what a blast breaks (planning step 115): the breakable walls it reaches with
// a clear line, on its own tick, through Demolish - and nothing else of what the
// original's grab sets `destroy` on.
//
// Reads the converted levels and Enhanced's data from outside this repository,
// and skips, saying where it looked, when either is absent.

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "core/Components.hpp"
#include "sim/Demolish.hpp"
#include "sim/Fire.hpp"
#include "sim/Game.hpp"
#include "sim/Units.hpp"

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

using namespace MagicPortals;
using Supersonic::TransformComponent;

namespace {

const std::string kLevels = MAGICPORTALS_LEVELS_DIR;
const std::string kData = MAGICPORTALS_DATA_DIR;
const std::string kPortData = MAGICPORTALS_PORT_DATA_DIR;
constexpr float kStep = 1.0f / 60.0f;

// level9a carries both a fire agent and burnable crates; level12a three barrel
// bombs within a blast of each other.
constexpr const char* kFireLevel = "level9a";
constexpr const char* kBombLevel = "level12a";

struct Run {
    Game::Data data;
    entt::registry registry;
    Game::Level level;
};

bool Begin(const std::string& levelName, Run& run) {
    std::string error;
    const bool ok = Game::LoadData(kLevels + "/" + levelName + ".tscn", kData,
                                   std::filesystem::temp_directory_path() / "supersonic-test-mp-fire", run.data,
                                   error) &&
                    Game::Start(run.data, run.registry, run.level, error);
    CHECK_MSG(ok, levelName + ": " + error);
    return ok;
}

void Tick(Run& run) {
    Game::Tick(run.data, run.registry, run.level, 0.0f, kStep);
}

// Puts a body exactly this far from a point, along +x, and holds nothing: one
// tick of gravity moves a crate about 0.27 px, well inside the margins used here.
void PutAt(entt::registry& registry, entt::entity body, const glm::dvec2& atPx) {
    auto& transform = registry.get<TransformComponent>(body);
    const glm::vec3 world = Units::ToWorld(atPx.x, atPx.y);
    transform.position = glm::vec3(world.x, world.y, transform.position.z);
}

std::filesystem::path Scratch() {
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "supersonic-test-mp-fire";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir;
}

std::string WriteRules(const char* name, const std::string& body) {
    const std::filesystem::path path = Scratch() / name;
    std::ofstream file(path, std::ios::trunc);
    file << body;
    return path.string();
}

void TheDecodedNumbersAreNotTheRemakesGuesses() {
    Fire::Rules rules;
    std::string error;
    CHECK_MSG(Fire::LoadRules(kPortData + "/fire.json", rules, error), kPortData + "/fire.json: " + error);

    CHECK_MSG(::test::nearly(static_cast<float>(rules.reachPx), 32.0f),
              "a fire agent reaches its sprite's 32, not the Godot project's halved collision box of 24");
    CHECK_MSG(::test::nearly(static_cast<float>(rules.burnMs), 1000.0f),
              "BURN_TIME is 1000 ms, not the Godot project's invented 2000");
    CHECK_MSG(::test::nearly(static_cast<float>(rules.blastPx), 80.0f),
              "a blast grabs at 80, not the Godot project's invented 96");

    // Only a crate fades and goes: manageBurnable's one caller is crateCallback.
    // A shock_agent carries the burnable flag and must NOT be in this list, or
    // burning one would delete it and change its level's puzzle.
    bool sawCrate = false;
    bool sawShockAgent = false;
    for (const std::string& name : rules.fadeNames) {
        if (name == "crate_no_emissive") sawCrate = true;
        if (name == "shock_agent") sawShockAgent = true;
    }
    CHECK_MSG(sawCrate, "the crates fade and are taken away");
    CHECK_MSG(!sawShockAgent, "a shock agent carries the flag but is not one of them");
}

void TheReaderRefusesWhatWouldBeSilent() {
    Fire::Rules rules;
    std::string error;

    const std::string zeroReach = WriteRules("zero-reach.json", R"({
      "fire": {"reach_px": 0.0},
      "burn": {"time_ms": 1000.0, "fade_names": ["crate"]},
      "blast": {"radius_px": 80.0},
      "lava": {"entities": ["lava"], "margin_px": 1.0}
    })");
    CHECK_MSG(!Fire::LoadRules(zeroReach, rules, error), "a reach of nothing is refused");

    const std::string zeroBurn = WriteRules("zero-burn.json", R"({
      "fire": {"reach_px": 32.0},
      "burn": {"time_ms": 0.0, "fade_names": ["crate"]},
      "blast": {"radius_px": 80.0},
      "lava": {"entities": ["lava"], "margin_px": 1.0}
    })");
    CHECK_MSG(!Fire::LoadRules(zeroBurn, rules, error), "and a burn that takes no time");

    const std::string zeroBlast = WriteRules("zero-blast.json", R"({
      "fire": {"reach_px": 32.0},
      "burn": {"time_ms": 1000.0, "fade_names": ["crate"]},
      "blast": {"radius_px": 0.0},
      "lava": {"entities": ["lava"], "margin_px": 1.0}
    })");
    CHECK_MSG(!Fire::LoadRules(zeroBlast, rules, error), "and a blast that reaches nothing");

    // An empty list makes burning a flag and nothing more, in every level at once.
    const std::string noFades = WriteRules("no-fades.json", R"({
      "fire": {"reach_px": 32.0},
      "burn": {"time_ms": 1000.0, "fade_names": []},
      "blast": {"radius_px": 80.0},
      "lava": {"entities": ["lava"], "margin_px": 1.0}
    })");
    CHECK_MSG(!Fire::LoadRules(noFades, rules, error), "and a burn that never removes anything");

    // And a lava bound to no entity, which is a barrel that misses lying there for
    // ever: 4-32's restock would then find it and never put another back.
    const std::string noLava = WriteRules("no-lava.json", R"({
      "fire": {"reach_px": 32.0},
      "burn": {"time_ms": 1000.0, "fade_names": ["crate"]},
      "blast": {"radius_px": 80.0},
      "lava": {"entities": [], "margin_px": 1.0}
    })");
    CHECK_MSG(!Fire::LoadRules(noLava, rules, error), "and a lava that touches nothing");
    // The good file, with each case's one change undone, is read: the refusals
    // above are theirs and not the lava's.
    const std::string good = WriteRules("good.json", R"({
      "fire": {"reach_px": 32.0},
      "burn": {"time_ms": 1000.0, "fade_names": ["crate"]},
      "blast": {"radius_px": 80.0},
      "lava": {"entities": ["lava"], "margin_px": 1.0}
    })");
    CHECK_MSG(Fire::LoadRules(good, rules, error), "and the same file with nothing wrong is read: " + error);
}

void WhatTheLevelsCarry() {
    Run fire;
    if (!Begin(kFireLevel, fire)) return;
    CHECK_MSG(!fire.level.fire.agents.empty(), "level9a places at least one fire agent");
    CHECK_MSG(!fire.level.fire.burnables.empty(), "and burnable crates");

    bool anyFades = false;
    for (const Fire::Burnable& burnable : fire.level.fire.burnables) {
        if (burnable.fades) anyFades = true;
        CHECK_MSG(!burnable.burned, "and nothing is alight before anything reaches it");
    }
    CHECK_MSG(anyFades, "and a crate among them is one that burns away");
    std::printf("  %s: %d fire agent(s), %d burnable(s)\n", kFireLevel,
                static_cast<int>(fire.level.fire.agents.size()),
                static_cast<int>(fire.level.fire.burnables.size()));

    Run bombs;
    if (!Begin(kBombLevel, bombs)) return;
    CHECK_MSG(bombs.level.fire.bombs.size() >= 2, "level12a places several barrel bombs");
    for (const Fire::Bomb& bomb : bombs.level.fire.bombs) {
        CHECK_MSG(!bomb.requested && !bomb.blown, "and none is asked to go off before anything asks it");
    }
    std::printf("  %s: %d bomb(s)\n", kBombLevel, static_cast<int>(bombs.level.fire.bombs.size()));
}

// THE BODY A BLAST GRABS, AT THE BARREL'S <Scale> (00_order 9.3, planning step
// 95; the owner's standing ruling R9). barrel_bomb.ent states <Scale 1.3> over a
// 22 x 29 collision box, and the engine builds the body from the box after
// box.Scale (ETHPhysicsSimulator.cpp:72-73, ETHCollisionBox.cpp:105-111): 28.6 x
// 37.7, which the converter writes since step 94. The blast's 80 reaches
// colliders, so this is the size a chain and a push are judged on.
void ABarrelsBodyIsItsScaledBox() {
    Run run;
    if (!Begin(kBombLevel, run)) return;
    CHECK_EQ(run.level.fire.bombs.size(), std::size_t{3});
    int scaled = 0;
    for (const Fire::Bomb& bomb : run.level.fire.bombs) {
        const auto* box = run.registry.try_get<Supersonic::BoxColliderComponent>(bomb.body);
        const bool ok = box != nullptr && std::fabs(box->size.x * Units::kPixelsPerMetre - 28.6) < 1e-4 &&
                        std::fabs(box->size.y * Units::kPixelsPerMetre - 37.7) < 1e-4;
        CHECK_MSG(ok, bomb.name + "'s body is 28.6 x 37.7, the 22 x 29 box at 1.3");
        if (ok) ++scaled;
    }
    std::printf("  %s: %d barrel bodies at 28.6 x 37.7\n", kBombLevel, scaled);
}

// THE test that separates the decode from the Godot project's reading: 28 px is inside
// the original's 32 and outside the Godot project's 24.
void AFireAgentReachesItsSpritesWidth() {
    Run run;
    if (!Begin(kFireLevel, run)) return;
    if (run.level.fire.agents.empty() || run.level.fire.burnables.empty()) {
        CHECK_MSG(false, "level9a carries a fire agent and a crate");
        return;
    }

    const glm::dvec2 agentPx = run.level.fire.agents.front().atPx;
    const std::string name = run.level.fire.burnables.front().name;
    const entt::entity crate = run.level.fire.burnables.front().body;

    PutAt(run.registry, crate, agentPx + glm::dvec2(28.0, 0.0));
    Tick(run);
    const Fire::Burnable* inReach = run.level.fire.FindBurnable(name);
    CHECK_MSG(inReach != nullptr && inReach->burned,
              "a crate 28 px away burns, which the Godot project's halved 24 would not reach");

    // And beyond it, nothing. A fresh level, because the first one is alight.
    Run beyond;
    if (!Begin(kFireLevel, beyond)) return;
    const entt::entity other = beyond.level.fire.burnables.front().body;
    PutAt(beyond.registry, other, beyond.level.fire.agents.front().atPx + glm::dvec2(40.0, 0.0));
    Tick(beyond);
    const Fire::Burnable* outOfReach = beyond.level.fire.FindBurnable(name);
    CHECK_MSG(outOfReach != nullptr && !outOfReach->burned, "and one 40 px away does not");
}

void ABurningCrateGoesAfterItsSecond() {
    Run run;
    if (!Begin(kFireLevel, run)) return;
    if (run.level.fire.agents.empty() || run.level.fire.burnables.empty()) return;

    const std::string name = run.level.fire.burnables.front().name;
    const entt::entity crate = run.level.fire.burnables.front().body;
    PutAt(run.registry, crate, run.level.fire.agents.front().atPx);
    Tick(run);

    const Fire::Burnable* lit = run.level.fire.FindBurnable(name);
    CHECK_MSG(lit != nullptr && lit->burned, "a crate in the flame catches");
    CHECK_MSG(run.registry.valid(crate), "and is still there while it burns");

    // Half of BURN_TIME: still there. That is what makes burning a race.
    for (int tick = 0; tick < 30; ++tick) Tick(run);
    const Fire::Burnable* half = run.level.fire.FindBurnable(name);
    CHECK_MSG(half != nullptr && !half->gone, "half a second in it is still there");

    for (int tick = 0; tick < 31; ++tick) Tick(run);
    const Fire::Burnable* done = run.level.fire.FindBurnable(name);
    CHECK_MSG(done != nullptr && done->gone, "and after its second it is gone");
    CHECK_MSG(!run.registry.valid(crate), "its body taken away, collision and all");
    std::printf("  %s: a crate burned away after %.0f ms\n", kFireLevel, run.data.fire.burnMs);
}

void FireKillsThePlayerAndABlastDoesNot() {
    // Fire does.
    Run burned;
    if (!Begin(kFireLevel, burned)) return;
    if (burned.level.fire.agents.empty()) return;
    const glm::dvec2 agentPx = burned.level.fire.agents.front().atPx;
    for (int tick = 0; tick < 3 && !burned.level.hazards.playerDied; ++tick) {
        PutAt(burned.registry, burned.level.player, agentPx);
        Tick(burned);
    }
    CHECK_MSG(burned.level.fire.playerKilled, "a player standing in the flame burns");
    CHECK_MSG(burned.level.hazards.playerDied, "and dies as a hazard kills it");
    CHECK_MSG(burned.level.hazards.killedBy == burned.level.fire.killedBy,
              "named by the fire agent, got '" + burned.level.hazards.killedBy + "'");

    // A blast does not, and this is the original's own answer.
    Run blown;
    if (!Begin(kBombLevel, blown)) return;
    if (blown.level.fire.bombs.empty()) return;
    const entt::entity bomb = blown.level.fire.bombs.front().body;
    const glm::dvec2 bombPx = Units::ToPixels(blown.registry.get<TransformComponent>(bomb).position);
    PutAt(blown.registry, blown.level.player, bombPx);
    blown.level.fire.bombs.front().requested = true;
    Tick(blown);
    Tick(blown);

    CHECK_MSG(blown.level.fire.blasts > 0, "the bomb the player stands on goes off");
    CHECK_MSG(!blown.level.fire.playerKilled,
              "and the player is untouched: the grabber takes breakable, explosive and burnable, and a character is "
              "none of those");
    CHECK_MSG(!blown.level.hazards.playerDied, "so nothing kills it");
    std::printf("  %s: %d blast(s) under the player, which survived\n", kBombLevel, blown.level.fire.blasts);
}

void AChainRipplesATickAtATime() {
    Run run;
    if (!Begin(kBombLevel, run)) return;
    if (run.level.fire.bombs.size() < 2) return;

    run.level.fire.bombs.front().requested = true;
    Tick(run);
    CHECK_EQ(run.level.fire.blasts, 1);

    int asked = 0;
    for (const Fire::Bomb& bomb : run.level.fire.bombs) {
        if (bomb.requested && !bomb.blown) ++asked;
    }
    CHECK_MSG(asked > 0, "the first blast asks its neighbours rather than setting them off itself");

    Tick(run);
    CHECK_MSG(run.level.fire.blasts > 1, "and they go off on the tick after, not the same one");
    std::printf("  %s: a chain of %d went off, one tick apart\n", kBombLevel, run.level.fire.blasts);
}

// ---- planning step 115: what a blast breaks ------------------------------------------

const Demolish::Breakable* BreakableNamed(const Run& run, const std::string& name) {
    return run.level.demolish.FindBreakable(name);
}

bool Grabbed(const Run& run, entt::entity body) {
    for (const Fire::Grab& grab : run.level.fire.grabbedNow) {
        if (grab.body == body) return true;
    }
    return false;
}

// explode #111-122 sets `destroy` on the breakable wall its grab reaches, and the
// wall's own callback takes it away on the same frame (demolish.json `blast`):
// 2-12's barrel_bomb_ent_1041 stands 34 units from breakable_wall_1040, as the
// level places them and as footage F6 shows them go (#159-#160).
void ABlastBreaksTheWallItReachesOnItsTick() {
    Run run;
    if (!Begin("level11a", run)) return;
    const Demolish::Breakable* wall = BreakableNamed(run, "breakable_wall_1040");
    CHECK_MSG(wall != nullptr, "2-12's wall is a Demolish::Breakable");
    CHECK_MSG(run.level.fire.bombs.size() == 1, "2-12 places one barrel");
    if (wall == nullptr || run.level.fire.bombs.size() != 1) return;
    CHECK_MSG(wall->flagged && wall->blastBreaks, "flagged, and breakable_wall is in demolish.json blast.entities");
    const entt::entity body = wall->body;
    CHECK(run.registry.valid(body));

    run.level.fire.bombs.front().requested = true;
    Tick(run);
    CHECK_EQ(run.level.fire.blasts, 1);
    CHECK_MSG(Grabbed(run, body), "the blast reaches the wall with a clear line");
    CHECK_MSG(wall->broken, "and the wall is gone on the blast's own tick");
    CHECK_MSG(wall->brokenBy == run.level.fire.bombs.front().name,
              "broken by the barrel, got '" + wall->brokenBy + "'");
    CHECK_MSG(!run.registry.valid(body) && wall->body == entt::null, "its body taken away, as Demolish takes one");
    CHECK_EQ(run.level.demolish.Broken(), 1);
    std::printf("  level11a: barrel_bomb_ent_1041's blast broke breakable_wall_1040 on its own tick\n");
}

// The grab keeps only what it sees (getFirstContactExcept, #63-109): on 2-16 a
// barrel put down over the left ledge, whose sphere reaches breakable_wall_1142's
// box 54 units off, blasts along a line that meets the ledge's top first.
void AWallBehindRockStands() {
    Run run;
    if (!Begin("level15a", run)) return;
    const Demolish::Breakable* wall = BreakableNamed(run, "breakable_wall_1142");
    const Fire::Bomb* barrel = run.level.fire.FindBomb("barrel_bomb_ent_1187");
    CHECK_MSG(wall != nullptr && barrel != nullptr, "2-16's wall and its barrel on the block");
    if (wall == nullptr || barrel == nullptr) return;
    PutAt(run.registry, barrel->body, glm::dvec2(80.0, 140.0));
    for (Fire::Bomb& bomb : run.level.fire.bombs) {
        if (bomb.name == "barrel_bomb_ent_1187") bomb.requested = true;
    }
    Tick(run);
    CHECK_MSG(run.level.fire.blasts >= 1, "the barrel went off over the ledge");
    CHECK_MSG(!Grabbed(run, wall->body), "the wall is behind the ledge from there");
    CHECK_MSG(!wall->broken, "so it stands");
    std::printf("  level15a: a blast at (80, 140) left breakable_wall_1142 standing behind the ledge\n");
}

// Out of reach, it stands: 2-13's barrels go off where the level places them, 321
// units and more from its wall.
void AWallOutOfReachStands() {
    Run run;
    if (!Begin(kBombLevel, run)) return;
    const Demolish::Breakable* wall = BreakableNamed(run, "breakable_wall_1040");
    CHECK(wall != nullptr);
    if (wall == nullptr || run.level.fire.bombs.empty()) return;
    run.level.fire.bombs.front().requested = true;
    for (int tick = 0; tick < 5; ++tick) Tick(run);
    CHECK_MSG(run.level.fire.blasts >= 1, "the barrels went off");
    CHECK_MSG(!wall->broken, "and the wall stands");
    std::printf("  %s: %d blast(s) far from breakable_wall_1040, which stands\n", kBombLevel, run.level.fire.blasts);
}

// The port breaks the walls alone. 2-18's barrel_bomb_ent_1151 stands 54 units
// above carranca_right_1075, a flagged breakable the blast grabs with a clear
// line; the original's carrancaCallback (#128-183) would then delete it. The port
// leaves it to Turrets, which is not built (demolish.json blast._not_built), so
// it stands and goes on firing - pinned so that building it moves this line.
void ABlastLeavesACarrancaStanding() {
    Run run;
    if (!Begin("level17a", run)) return;
    const Demolish::Breakable* carranca = BreakableNamed(run, "carranca_right_1075");
    CHECK_MSG(carranca != nullptr && carranca->flagged && !carranca->blastBreaks,
              "a carranca is a flagged breakable, and not one the port's blast breaks");
    if (carranca == nullptr || run.level.fire.bombs.empty()) return;
    run.level.fire.bombs.front().requested = true;
    Tick(run);
    CHECK_EQ(run.level.fire.blasts, 1);
    CHECK_MSG(Grabbed(run, carranca->body), "the grab reaches it, as the original's does");
    CHECK_MSG(!carranca->broken && run.registry.valid(carranca->body), "and it stands");
}

// THE LAVA (ETHBeginContactCallback_lava, listing L68416, #1-30; planning step
// 116): whatever begins a contact with it is burned when burnable and then asked
// to go off. So a barrel that falls into level31c's lava goes off there, a tick
// after it touches, as a fireball's barrel does - and nothing it does kills.
void TheLavaSetsOffABarrelThatFallsIn() {
    Fire::Rules rules;
    std::string error;
    CHECK_MSG(Fire::LoadRules(kPortData + "/fire.json", rules, error), error);
    const bool bothSpellings = rules.lavaEntities.size() == 2 && rules.lavaEntities[0] == "lava" &&
                               rules.lavaEntities[1] == "lava.ent";
    CHECK_MSG(bothSpellings, "the callback is bound to lava by name, both spellings");

    Run run;
    if (!Begin("level31c", run)) return;
    CHECK_EQ(run.level.fire.lavas.size(), std::size_t{1});
    CHECK_EQ(run.level.fire.bombs.size(), std::size_t{1});
    if (run.level.fire.lavas.size() != 1 || run.level.fire.bombs.size() != 1) return;
    const Fire::Lava& lava = run.level.fire.lavas.front();
    CHECK(lava.name == "lava_1140");
    // lava_1140: a 768 x 128 box at (384, 462), its top at y 398.
    const glm::dvec2 centrePx = Units::ToPixels(glm::vec3(lava.box.centre, 0.0f));
    CHECK(::test::nearly(static_cast<float>(centrePx.x), 384.0f, 1e-3f));
    CHECK(::test::nearly(static_cast<float>(centrePx.y), 462.0f, 1e-3f));
    CHECK(::test::nearly(lava.box.half.x * static_cast<float>(Units::kPixelsPerMetre), 384.0f, 1e-3f));
    CHECK(::test::nearly(lava.box.half.y * static_cast<float>(Units::kPixelsPerMetre), 64.0f, 1e-3f));

    // The barrel over the gap between the light wall and block 2256, let fall.
    Fire::Bomb& bomb = run.level.fire.bombs.front();
    PutAt(run.registry, bomb.body, glm::dvec2(160.0, 250.0));
    int asked = -1;
    for (int tick = 1; tick <= 240 && asked < 0; ++tick) {
        Tick(run);
        if (bomb.requested) asked = tick;
    }
    CHECK_MSG(asked > 0, "the barrel is asked to go off when it lands in the lava");
    if (asked <= 0) return;
    const glm::dvec2 landedPx = Units::ToPixels(run.registry.get<TransformComponent>(bomb.body).position);
    CHECK_MSG(!bomb.blown && run.level.fire.blasts == 0, "and goes off on its own next turn, not on the touch");
    // Resting on the lava's top: its centre half its 37.7 height above y 398.
    CHECK_MSG(std::fabs(landedPx.y - (398.0 - 37.7 * 0.5)) < 2.0, "on the lava's top: " + std::to_string(landedPx.y));
    Tick(run);
    CHECK(bomb.blown);
    CHECK_EQ(run.level.fire.blasts, 1);
    CHECK_EQ(run.level.fire.blastsPx.size(), std::size_t{1});
    CHECK_MSG(!run.level.hazards.playerDied, "and nothing died of it");
    std::printf("  a barrel dropped from y 250 touched the lava on tick %d at y %.2f and went off on the next\n",
                asked, landedPx.y);

    // And a barrel nowhere near it is never asked: every other level has no lava.
    Run bombs;
    if (!Begin(kBombLevel, bombs)) return;
    CHECK(bombs.level.fire.lavas.empty());
}

// A SHOT THROUGH THE FLAME BECOMES A FIREBALL (planning step 134): burnProjectile,
// from ETHCallback_fire_agent #253-265. A shot in flight 10 px over level9a's agent
// is deleted where it first comes within 32 of it - no portal, no failure - and a
// fireball goes on from there along its heading, at fireball.ent's 130 u/s, and
// cannot hurt the player. Then it does what any fireball does: burns the first
// crate it runs into.
void AShotThroughTheFlameBecomesAFireball() {
    Run run;
    if (!Begin(kFireLevel, run)) return;
    if (run.level.fire.agents.empty()) return;
    const Fire::Agent agent = run.level.fire.agents.front();
    CHECK_EQ(run.level.portals.flames.size(), run.level.fire.agents.size());

    Portals::Flight shot;
    shot.fromPx = agent.atPx + glm::dvec2(-81.0, -10.0);
    shot.toPx = agent.atPx + glm::dvec2(79.0, -10.0);
    shot.atPx = shot.fromPx;
    run.level.portals.flight = shot;
    int burnedOn = 0;
    for (int tick = 1; tick <= 20 && burnedOn == 0; ++tick) {
        Tick(run);
        if (!run.level.portals.burns.empty()) burnedOn = tick;
    }
    CHECK_MSG(burnedOn > 0, "a shot 10 px over the agent is burned");
    if (burnedOn == 0) return;

    const Portals::Burn burn = run.level.portals.burns.front();
    const double entryX = agent.atPx.x - std::sqrt(32.0 * 32.0 - 10.0 * 10.0);
    CHECK_MSG(burn.flame == agent.name, "by " + agent.name + ", got '" + burn.flame + "'");
    CHECK_MSG(std::fabs(burn.atPx.x - entryX) < 1e-6 && std::fabs(burn.atPx.y - shot.fromPx.y) < 1e-6,
              "where it first came within the 32, x " + std::to_string(entryX));
    CHECK_MSG(std::fabs(burn.heading.x - 1.0) < 1e-9 && std::fabs(burn.heading.y) < 1e-9,
              "along its own heading");
    CHECK_MSG(!run.level.portals.flight.has_value(), "the shot is gone");
    CHECK_MSG(run.level.portals.placed.empty(), "and opened no portal");
    CHECK_EQ(run.level.portals.shotsFailed, 0);
    CHECK_EQ(run.level.portals.shotsBurned, 1);
    CHECK_EQ(run.level.turrets.fireballs.size(), std::size_t{1});
    if (run.level.turrets.fireballs.empty()) return;
    const Turrets::Fireball ball = run.level.turrets.fireballs.front();
    CHECK_MSG(ball.name == agent.name + "#1", "the fireball is named for its flame, got '" + ball.name + "'");
    CHECK_MSG(std::fabs(ball.atPx.x - entryX) < 1e-6, "and starts where the shot was burned");
    CHECK_MSG(std::fabs(ball.velocityPx.x - 130.0) < 1e-9 && std::fabs(ball.velocityPx.y) < 1e-9,
              "flying on at fireball.ent's 130 u/s along the shot's heading");
    CHECK_MSG(!ball.killsPlayer, "with killMainCharacter clear, as burnProjectile passes it");

    // The crate in its lane: crate_no_emissive_937, 133 u on. It catches.
    const Fire::Burnable* crate = run.level.fire.FindBurnable("crate_no_emissive_937");
    CHECK_MSG(crate != nullptr, "level9a's lower crate is a burnable");
    int litOn = 0;
    for (int tick = 1; tick <= 90 && litOn == 0; ++tick) {
        Tick(run);
        crate = run.level.fire.FindBurnable("crate_no_emissive_937");
        if (crate != nullptr && crate->burned) litOn = tick;
    }
    CHECK_MSG(litOn > 0, "and the fireball burns the crate it runs into");
    CHECK_MSG(!run.level.hazards.playerDied, "and nothing killed the player");
    std::printf("  %s: a shot 10 px over %s burned on tick %d at x %.3f; its fireball lit %s %d ticks later\n",
                kFireLevel, agent.name.c_str(), burnedOn, burn.atPx.x, "crate_no_emissive_937", litOn);
}

// And a shot that never comes within the 32 flies on, as does one whose way ends
// short of the flame: only a shot still in flight is burned.
void AShotThatMissesTheFlameFliesOn() {
    Run past;
    if (!Begin(kFireLevel, past)) return;
    if (past.level.fire.agents.empty()) return;
    const glm::dvec2 agentPx = past.level.fire.agents.front().atPx;
    Portals::Flight over;
    over.fromPx = agentPx + glm::dvec2(-81.0, -33.0);
    over.toPx = agentPx + glm::dvec2(79.0, -33.0);
    over.atPx = over.fromPx;
    past.level.portals.flight = over;
    for (int tick = 0; tick < 20 && past.level.portals.flight.has_value(); ++tick) Tick(past);
    CHECK_EQ(past.level.portals.shotsBurned, 0);
    CHECK_MSG(past.level.turrets.fireballs.empty(), "a shot 33 px over the agent is not burned");
    CHECK_EQ(past.level.portals.placed.size(), std::size_t{1});

    Run shortOf;
    if (!Begin(kFireLevel, shortOf)) return;
    Portals::Flight stops;
    stops.fromPx = agentPx + glm::dvec2(-81.0, -10.0);
    stops.toPx = agentPx + glm::dvec2(-31.0, -10.0); // 32.6 from the agent, where it opens
    stops.atPx = stops.fromPx;
    shortOf.level.portals.flight = stops;
    for (int tick = 0; tick < 20 && shortOf.level.portals.flight.has_value(); ++tick) Tick(shortOf);
    CHECK_EQ(shortOf.level.portals.shotsBurned, 0);
    CHECK_EQ(shortOf.level.portals.placed.size(), std::size_t{1});
}

// A FLAME BURNS A MINION (planning step 134): the second of the callback's four
// tests, burn() on a burnable - and a minion is no character. Its own callback
// finds itself burned on the frame after, a dynamic entity's running before the
// static agent's, so it is still there on the tick it is set alight and gone on the
// next. Measured from its entity point. level27c's knight, 4-28's key carrier.
void AFlameBurnsAMinionOnTheNextTick() {
    constexpr const char* kMinionLevel = "level27c";
    Run run;
    if (!Begin(kMinionLevel, run)) return;
    if (run.level.fire.agents.empty() || run.level.minions.minions.empty()) {
        CHECK_MSG(false, "level27c places a fire agent and a knight");
        return;
    }
    const glm::dvec2 agentPx = run.level.fire.agents.front().atPx;
    const entt::entity knight = run.level.minions.minions.front().body;
    const double above = run.level.minions.rules.EntityAboveCentrePx();

    // Its entity point 40 above the agent: out of reach, and nothing happens.
    PutAt(run.registry, knight, agentPx + glm::dvec2(0.0, -40.0 + above));
    Tick(run);
    CHECK_MSG(run.level.fire.minionsAlight.empty(), "a knight whose point is 40 from the flame is not alight");

    // 20 above it: alight, and still standing this tick.
    PutAt(run.registry, knight, agentPx + glm::dvec2(0.0, -20.0 + above));
    Tick(run);
    const int burnedBefore = run.level.minions.burned;
    CHECK_EQ(run.level.fire.minionsAlight.size(), std::size_t{1});
    CHECK_MSG(run.registry.valid(knight) && !run.level.minions.minions.front().gone,
              "a knight set alight is still there on the tick it catches");
    Tick(run);
    CHECK_MSG(run.level.minions.minions.front().gone && !run.registry.valid(knight),
              "and destroyed by its own callback on the next");
    CHECK_EQ(run.level.minions.burned, burnedBefore + 1);
    bool byFire = false;
    for (const Minions::Death& death : run.level.minions.deaths) byFire = byFire || death.byFire;
    CHECK_MSG(byFire, "a death by fire, whose blood the frame adds");
    CHECK_MSG(run.level.fire.minionsAlight.empty(), "and nothing is left alight");
    std::printf("  %s: a knight 20 from %s caught on one tick and was gone on the next\n", kMinionLevel,
                run.level.fire.agents.front().name.c_str());
}

void runTests() {
    TheDecodedNumbersAreNotTheRemakesGuesses();
    TheReaderRefusesWhatWouldBeSilent();
    WhatTheLevelsCarry();
    ABarrelsBodyIsItsScaledBox();
    AFireAgentReachesItsSpritesWidth();
    ABurningCrateGoesAfterItsSecond();
    FireKillsThePlayerAndABlastDoesNot();
    AChainRipplesATickAtATime();
    ABlastBreaksTheWallItReachesOnItsTick();
    AWallBehindRockStands();
    AWallOutOfReachStands();
    ABlastLeavesACarrancaStanding();
    TheLavaSetsOffABarrelThatFallsIn();
    AShotThroughTheFlameBecomesAFireball();
    AShotThatMissesTheFlameFliesOn();
    AFlameBurnsAMinionOnTheNextTick();
}

} // namespace

MP_SUITE_MAIN("test_mp_fire") {
    std::error_code ec;
    if (!std::filesystem::is_directory(kLevels, ec) ||
        !std::filesystem::is_regular_file(kData + "/entity_roles.json", ec)) {
        std::printf("test_mp_fire SKIPPED - needs the converted levels at %s and Enhanced's data at %s\n",
                    kLevels.c_str(), kData.c_str());
        return 77;
    }
    runTests();
    return ::test::summary("test_mp_fire", 25);
}
