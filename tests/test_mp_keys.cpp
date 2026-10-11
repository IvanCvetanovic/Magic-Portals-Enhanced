// Keys, the keyholes they open, and the locked doors that go with them.
//
// What is pinned here is the DECODE. Keys.hpp cites where each number comes from;
// the ones a test can tell apart from the Godot project's guesses are:
//
//   range   ONE range of 26 px, serving both being picked up and opening a
//           keyhole. The Godot project invents two - pickup_range_px and
//           unlock_range_px - and guesses 40 for each, so a key 28 px from the
//           player is taken in the Godot project and not in the original. That exact
//           distance is tested in both directions.
//   pickup  a distance poll rather than a contact, and anything that is a
//           character OR a minion can take one. A minion carrying a key is
//           therefore tested, because it is the kind of thing a port drops
//           without noticing. Both the poll and the trail run from the
//           carrier's ENTITY point, which for a minion is not its body's
//           centre, and that is tested too.
//   fade    an unlocked keyhole holds 1000 ms and fades over 500, and only then
//           is the door deleted. Nothing re-locks it: `unlocked` is written in
//           one place in the whole binary and read in one other.
//
// And the pairing, which is by COLOUR and not by proximity. level20b is the only
// level in the game holding two pairs of one colour, and it is tested because it
// is the only one where a nearest-door rule could have picked the wrong door: its
// two yellow keyholes sit 57 px from their own doors and 216 px or more from each
// other's.
//
// Reads the converted levels and Enhanced's data from outside this repository,
// and skips, saying where it looked, when either is absent.

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "core/Components.hpp"
#include "sim/Game.hpp"
#include "sim/Keys.hpp"
#include "sim/Lighting.hpp"
#include "sim/RedKey.hpp"
#include "sim/Units.hpp"

#include <algorithm>
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
using Supersonic::RigidBodyComponent;
using Supersonic::TransformComponent;

namespace {

const std::string kLevels = MAGICPORTALS_LEVELS_DIR;
const std::string kData = MAGICPORTALS_DATA_DIR;
constexpr float kStep = 1.0f / 60.0f;

// level7b is the first level in the game with a key: one yellow trio, the key at
// (174, 34), its keyhole at (588, 108) and its door at (624, 66).
constexpr const char* kLevel = "level7b";
constexpr const char* kKey = "key_ent_1335";
constexpr const char* kKeyhole = "keyhole_1338";
constexpr const char* kDoor = "door_locked_ent_1343";

struct Run {
    Game::Data data;
    entt::registry registry;
    Game::Level level;
};

bool Begin(const std::string& levelName, Run& run) {
    std::string error;
    const bool ok = Game::LoadData(kLevels + "/" + levelName + ".tscn", kData,
                                   std::filesystem::temp_directory_path() / "supersonic-test-mp-keys", run.data,
                                   error) &&
                    Game::Start(run.data, run.registry, run.level, error);
    CHECK_MSG(ok, levelName + std::string(": ") + error);
    return ok;
}

void Tick(Run& run) {
    Game::Tick(run.data, run.registry, run.level, 0.0f, kStep);
}

void PutAt(entt::registry& registry, entt::entity body, const glm::dvec2& atPx) {
    const glm::vec3 world = Units::ToWorld(atPx.x, atPx.y);
    auto& transform = registry.get<TransformComponent>(body);
    transform.position = glm::vec3(world.x, world.y, transform.position.z);
    if (registry.all_of<RigidBodyComponent>(body)) {
        registry.get<RigidBodyComponent>(body).velocity = glm::vec3(0.0f);
    }
}

void theOneRangeServesBothTests() {
    Run run;
    if (!Begin(kLevel, run)) return;

    const Keys::Rules& rules = run.level.keys.rules;
    CHECK(::test::nearly(static_cast<float>(rules.rangePx), 26.0f));
    CHECK_MSG(rules.rangePx < 40.0, "the Godot project's 40 px is not what is played");
    CHECK(::test::nearly(static_cast<float>(rules.leashPx), 40.0f));
    CHECK(::test::nearly(static_cast<float>(rules.fadeStartMs), 1000.0f));
    CHECK(::test::nearly(static_cast<float>(rules.fadeMs), 500.0f));

    CHECK_EQ(run.level.keys.keys.size(), static_cast<std::size_t>(1));
    CHECK_EQ(run.level.keys.keyholes.size(), static_cast<std::size_t>(1));

    const Keys::Key* key = run.level.keys.FindKey(kKey);
    CHECK(key != nullptr);
    if (key != nullptr) {
        CHECK_MSG(key->colour == "yellow", "the key is yellow");
        CHECK(key->owner == entt::null);
    }
    const Keys::Keyhole* keyhole = run.level.keys.FindKeyhole(kKeyhole);
    CHECK(keyhole != nullptr);
    if (keyhole != nullptr) {
        CHECK_MSG(keyhole->colour == "yellow", "and so is its keyhole");
        CHECK_MSG(keyhole->doorName == kDoor, "which took the door of its own colour");
        CHECK(!keyhole->unlocked);
    }
}

// The range, swept rather than asserted at one spot.
//
// Keys::Tick runs AFTER the physics step, and level7b's key lies between two
// collision polygons, so a player put down beside it is resolved somewhere else
// before the keys are judged - which makes any fixed offset a fiction. So each
// offset is tried, the distance the module really measured is read back, and the
// decision is pinned against THAT. What pins the number itself is the pair of
// brackets at the end: a key taken from beyond 20 px is not a tiny range, and one
// left lying while the player stood inside 40 is not the Godot project's guess.
void aKeyIsTakenAtTwentySixPixels() {
    const struct Case {
        double atPx;
        bool taken;
    } cases[] = {{16.0, true}, {20.0, true}, {24.0, true}, {28.0, false}, {32.0, false}, {40.0, false}};

    // `taken` above is what the decoded 26 says of each offset, and is kept as
    // the record of it; what is asserted is the measurement.
    bool tookBeyondTwenty = false;
    bool leftInsideForty = false;

    for (const Case& one : cases) {
        Run run;
        if (!Begin(kLevel, run)) continue;
        const Keys::Key* key = run.level.keys.FindKey(kKey);
        if (key == nullptr || run.level.player == entt::null) continue;
        const glm::dvec2 keyAt = key->atPx;

        PutAt(run.registry, run.level.player, glm::dvec2(keyAt.x + one.atPx, keyAt.y));
        Tick(run);

        const Keys::Key* after = run.level.keys.FindKey(kKey);
        CHECK(after != nullptr);
        if (after == nullptr) continue;

        // Where the player ACTUALLY is when the keys are judged. Keys::Tick runs
        // after the physics step, so a spot inside the level's geometry would be
        // resolved out from under this and the distance pinned here would not be
        // the distance the module measured.
        const glm::dvec2 playerPx =
            Units::ToPixels(run.registry.get<TransformComponent>(run.level.player).position);
        const double measured = std::sqrt((playerPx.x - keyAt.x) * (playerPx.x - keyAt.x) +
                                          (playerPx.y - keyAt.y) * (playerPx.y - keyAt.y));
        const bool taken = after->owner != entt::null;
        CHECK_MSG(taken == (measured < run.level.keys.rules.rangePx),
                  std::string("at ") + std::to_string(static_cast<int>(measured)) + " px the key is " +
                      (taken ? "taken" : "left"));

        // level7b spawns a minion 34 px from the key, and a minion can carry one.
        // If anything took it at a distance the player could not, say so.
        CHECK_MSG(!taken || after->owner == run.level.player,
                  "whatever took the key, it was the player and not a minion");

        if (taken && measured > 20.0) tookBeyondTwenty = true;
        if (!taken && measured < 40.0) leftInsideForty = true;
    }

    CHECK_MSG(tookBeyondTwenty, "a key is taken from beyond 20 px");
    CHECK_MSG(leftInsideForty, "and left lying from inside 40, which the Godot project's guess would have taken");
}

// Anything that is a character OR a minion takes one, and level7b has both: a
// minion_spawn at (208, 34), which is 34 px from the key lying at (174, 34). This
// one follows the player's.
void aCarriedKeyTrailsItsOwner() {
    Run run;
    if (!Begin(kLevel, run)) return;
    const Keys::Key* key = run.level.keys.FindKey(kKey);
    if (key == nullptr || run.level.player == entt::null) return;

    // By VALUE: FindKey hands back a pointer into the state, so reading it again
    // after the ticks below would be reading where the key is NOW, and comparing
    // that against itself.
    const glm::dvec2 lyingAt = key->atPx;

    PutAt(run.registry, run.level.player, lyingAt);
    Tick(run);
    const Keys::Key* taken = run.level.keys.FindKey(kKey);
    CHECK(taken != nullptr);
    if (taken == nullptr) return;
    CHECK(taken->owner == run.level.player);
    CHECK_EQ(run.level.keys.picked, 1);

    // Carried well away, the key follows rather than staying put - and it stays
    // within the leash rather than being pinned exactly on its owner.
    PutAt(run.registry, run.level.player, glm::dvec2(lyingAt.x + 300.0, lyingAt.y));
    for (int tick = 0; tick < 30; ++tick) Tick(run);

    const Keys::Key* carried = run.level.keys.FindKey(kKey);
    CHECK(carried != nullptr);
    if (carried == nullptr) return;
    const glm::dvec2 playerPx = Units::ToPixels(run.registry.get<TransformComponent>(run.level.player).position);
    const double gap = std::sqrt((carried->atPx.x - playerPx.x) * (carried->atPx.x - playerPx.x) +
                                 (carried->atPx.y - playerPx.y) * (carried->atPx.y - playerPx.y));
    CHECK_MSG(gap <= run.level.keys.rules.leashPx + 1.0, "a carried key stays inside its leash");
    CHECK_MSG(std::fabs(carried->atPx.x - lyingAt.x) > 100.0, "and it has left where it was lying");
}

// The pickup filter is isCharacter OR isMinion, so a minion carries a key just as
// the player does. Pinned because it is the one thing that separates a key from a
// diamond, whose candidate loop tests isCharacter and stops there: the decode
// skips a non-character at instruction 123 and never reaches the range test.
//
// level7b holds three minion_spawns and the nearest of them lies 34 px from the
// key, outside the 26 px range, so none of them takes it where it lies - which is
// what the sweep above asserts from the other side. This puts the first minion ON
// the key and reads back who owns it. Carriers are judged player-first, so an
// owner that is the minion is the filter being exercised rather than an accident
// of ordering.
void aMinionCanCarryAKey() {
    Run run;
    if (!Begin(kLevel, run)) return;

    const Keys::Key* key = run.level.keys.FindKey(kKey);
    CHECK(key != nullptr);
    if (key == nullptr) return;
    CHECK_MSG(!run.level.minions.minions.empty(), "level7b spawns a minion");
    if (run.level.minions.minions.empty()) return;

    const entt::entity minion = run.level.minions.minions[0].body;
    CHECK(minion != entt::null);
    if (minion == entt::null) return;
    const glm::dvec2 lyingAt = key->atPx;

    PutAt(run.registry, minion, lyingAt);
    Tick(run);

    const Keys::Key* after = run.level.keys.FindKey(kKey);
    CHECK(after != nullptr);
    if (after == nullptr) return;
    CHECK_MSG(after->owner == minion, "a minion took the key");
    CHECK_MSG(after->owner != run.level.player, "and it was not the player, which is a level away");
    CHECK_EQ(run.level.keys.picked, 1);

    // And it leads the key at its entity point, the body's centre less 11.1, as
    // Game offers it: not at the centre, where the key would ride at its feet.
    // Read once the minion has stood still in y for half a second on the ledge
    // under the key, so the trail's lag is in x alone.
    int still = 0;
    double lastY = Units::ToPixels(run.registry.get<TransformComponent>(minion).position).y;
    for (int tick = 0; tick < 240 && still < 30 && run.registry.valid(minion); ++tick) {
        Tick(run);
        if (!run.registry.valid(minion)) break;
        const double y = Units::ToPixels(run.registry.get<TransformComponent>(minion).position).y;
        still = std::fabs(y - lastY) < 0.05 ? still + 1 : 0;
        lastY = y;
    }
    CHECK_MSG(still >= 30, "the minion came to rest in y");
    const Keys::Key* led = run.level.keys.FindKey(kKey);
    CHECK(led != nullptr);
    if (led == nullptr || !run.registry.valid(minion)) return;
    CHECK_MSG(led->owner == minion, "the minion still has it");
    const glm::dvec2 bodyPx = Units::ToPixels(run.registry.get<TransformComponent>(minion).position);
    const double pointY = bodyPx.y - run.level.minions.rules.EntityAboveCentrePx();
    CHECK_MSG(std::fabs(led->atPx.y - pointY) < 1.0,
              "the key rides at the minion's entity point: y " + std::to_string(led->atPx.y) + " against " +
                  std::to_string(pointY) + ", the body's centre " + std::to_string(bodyPx.y));
    CHECK_MSG(std::fabs(led->atPx.x - bodyPx.x) <= run.level.keys.rules.leashPx, "and within the leash in x");
}

// The carry path itself, on a bare registry with no physics to move anything:
// the pickup and the trail both run from the carrier's POINT. ETHCallback_key
// measures the 26 from the carrier's GetPositionXY (ins 744-753) and follows the
// same GetPositionXY (932-957 the leash, 959-976 followUp), the entity's point;
// a minion's stands EntityAboveCentrePx over its body's centre. So a key 30 above
// that centre is 18.9 from the point and taken, one 25 below is 36.1 from it and
// left, and a taken key comes to rest ON the point and is slammed onto it once
// its owner is past the leash.
void theCarryRunsFromTheCarriersPoint() {
    Run run;
    if (!Begin(kLevel, run)) return;
    const Keys::Rules& rules = run.level.keys.rules;
    const double above = run.level.minions.rules.EntityAboveCentrePx();
    CHECK_MSG(std::fabs(above - 11.1) < 1e-9, "a minion's entity point is 11.1 over its body's centre (18.1 - 7)");

    entt::registry registry;
    const entt::entity body = registry.create();
    const glm::dvec2 centre(100.0, 100.0);
    registry.emplace<TransformComponent>(body).position = Units::ToWorld(centre.x, centre.y);
    const Carry::Carrier atPoint{body, glm::dvec2(0.0, -above)};
    const Carry::Carrier atCentre{body, glm::dvec2(0.0, 0.0)};
    const auto within = [](double a, double b) { return std::fabs(a - b) < 1e-3; };

    Carry::Carried over;
    over.atPx = centre + glm::dvec2(0.0, -30.0);
    CHECK_MSG(!Carry::Acquire(over, registry, {atCentre}, rules.rangePx), "30 from a body's centre is outside 26");
    CHECK_MSG(Carry::Acquire(over, registry, {atPoint}, rules.rangePx), "and 18.9 from its point is inside it");
    CHECK(over.owner == body);
    CHECK(within(over.ownerPointFromCentrePx.y, -above));

    Carry::Carried under;
    under.atPx = centre + glm::dvec2(0.0, 25.0);
    CHECK_MSG(!Carry::Acquire(under, registry, {atPoint}, rules.rangePx),
              "25 from the centre is 36.1 from the point: left lying");
    CHECK(under.owner == entt::null);

    for (int tick = 0; tick < 120; ++tick) Carry::Trail(over, registry, rules, 1000.0 / 60.0);
    CHECK_MSG(within(over.atPx.x, centre.x) && within(over.atPx.y, centre.y - above),
              "a carried key comes to rest on its owner's point: " + std::to_string(over.atPx.x) + ", " +
                  std::to_string(over.atPx.y));

    const glm::dvec2 moved(400.0, 100.0);
    registry.get<TransformComponent>(body).position = Units::ToWorld(moved.x, moved.y);
    Carry::Trail(over, registry, rules, 1000.0 / 60.0);
    CHECK_MSG(within(over.atPx.x, moved.x) && within(over.atPx.y, moved.y - above),
              "and past the leash it is slammed onto that point, not onto the centre");

    registry.destroy(body);
    CHECK_MSG(Carry::Drop(over, registry), "an owner that has ceased to be lets go");
    CHECK(over.owner == entt::null);
    CHECK(within(over.ownerPointFromCentrePx.y, 0.0));
}

// The whole of what unlocking does to a level: after the hold and the fade, the
// door body is gone.
void unlockingHoldsThenFadesThenTakesTheDoor() {
    Run run;
    if (!Begin(kLevel, run)) return;
    const Keys::Key* key = run.level.keys.FindKey(kKey);
    const Keys::Keyhole* keyhole = run.level.keys.FindKeyhole(kKeyhole);
    if (key == nullptr || keyhole == nullptr || run.level.player == entt::null) return;

    const entt::entity door = keyhole->door;
    CHECK_MSG(door != entt::null && run.registry.valid(door), "the door is built and standing");
    const glm::dvec2 keyholeAt = keyhole->atPx;
    CHECK_MSG(key->opened == -1, "a key has opened nothing yet");
    const int keyholeIndex = static_cast<int>(keyhole - run.level.keys.keyholes.data());

    // Take the key, then carry it to the keyhole.
    PutAt(run.registry, run.level.player, key->atPx);
    Tick(run);
    PutAt(run.registry, run.level.player, keyholeAt);
    for (int tick = 0; tick < 30; ++tick) Tick(run);

    const Keys::Keyhole* reached = run.level.keys.FindKeyhole(kKeyhole);
    CHECK(reached != nullptr);
    if (reached == nullptr) return;
    CHECK_MSG(reached->unlocked, "the key opened its keyhole");
    CHECK_EQ(run.level.keys.unlocked, 1);
    const Keys::Key* spent = run.level.keys.FindKey(kKey);
    if (spent != nullptr) CHECK_MSG(spent->spent, "and the key is spent");
    // And it says which keyhole it opened, which the layer takes its picture away with.
    if (spent != nullptr) CHECK_MSG(spent->opened == keyholeIndex, "the spent key names the keyhole it opened");

    // Still there through the hold: 1000 ms, and the fade has not begun.
    CHECK_MSG(run.registry.valid(door), "the door stands through the hold");

    // Through the hold and the fade - 1500 ms in all - it goes.
    for (int tick = 0; tick < 120; ++tick) Tick(run);
    const Keys::Keyhole* faded = run.level.keys.FindKeyhole(kKeyhole);
    CHECK(faded != nullptr);
    if (faded == nullptr) return;
    CHECK_MSG(faded->gone, "the keyhole faded away");
    CHECK(::test::nearly(static_cast<float>(faded->alpha), 0.0f));
    CHECK_MSG(!run.registry.valid(door), "and took its door with it");
    CHECK_EQ(run.level.keys.opened, 1);
}

// The one level holding two pairs of a colour: each keyhole takes its own door.
void twoPairsOfOneColourStillPairCorrectly() {
    Run run;
    if (!Begin("level20b", run)) return;

    CHECK_EQ(run.level.keys.keyholes.size(), static_cast<std::size_t>(3));
    CHECK_EQ(run.level.keys.keys.size(), static_cast<std::size_t>(3));

    // keyhole (602,114) belongs to door (640,72), and (368,144) to (400,192).
    const struct Pair {
        glm::dvec2 keyhole;
        glm::dvec2 door;
    } expected[] = {{{602.0, 114.0}, {640.0, 72.0}}, {{368.0, 144.0}, {400.0, 192.0}}, {{78.0, 202.0}, {128.0, 192.0}}};

    for (const Pair& want : expected) {
        const Keys::Keyhole* found = nullptr;
        for (const Keys::Keyhole& keyhole : run.level.keys.keyholes) {
            if (std::fabs(keyhole.atPx.x - want.keyhole.x) < 1.0 && std::fabs(keyhole.atPx.y - want.keyhole.y) < 1.0) {
                found = &keyhole;
            }
        }
        CHECK(found != nullptr);
        if (found == nullptr || found->door == entt::null) continue;
        const glm::dvec2 doorAt = Units::ToPixels(run.registry.get<TransformComponent>(found->door).position);
        CHECK_MSG(std::fabs(doorAt.x - want.door.x) < 1.0 && std::fabs(doorAt.y - want.door.y) < 1.0,
                  found->name + " took the door at its own end of the level");
    }
}

// Every level that carries the roles starts, and every keyhole found a door.
void everyLevelWithAKeyholePairsIt() {
    const char* levels[] = {"level7b", "level9b", "level15b", "level20b", "level29b", "level0c", "level27c"};
    for (const char* name : levels) {
        Run run;
        if (!Begin(name, run)) continue;
        for (const Keys::Keyhole& keyhole : run.level.keys.keyholes) {
            CHECK_MSG(!keyhole.doorName.empty(), std::string(name) + ": " + keyhole.name + " has a door");
            CHECK_MSG(keyhole.door == entt::null || run.registry.valid(keyhole.door),
                      std::string(name) + ": " + keyhole.name + "'s door is built");
        }
    }
}

// ---- 4-19's red key state (planning step 118; keys.json red_key_state) -------------------
//
// level18c places the one red_key_state in the game on key_2701's own point, and
// breakable_wall_2756 between the key's room and the rocket. ETHCallback_red_key_state
// (listing L97171) watches the first entity named exactly `key` in its bucket; the
// tick that key has an owner it plays its sound and destroy()s itself, and from the
// next tick it fades its colour and the ambient by smoothBeginning(smoothEnd(1 -
// t/6000)) and at 6000 ms destroy()s SeekEntity('breakable_wall').

constexpr const char* kRedLevel = "level18c";
constexpr const char* kRedState = "red_key_state_ent_2754";
constexpr const char* kRedKey = "key_2701";
constexpr const char* kRedWall = "breakable_wall_2756";

void theRedKeyStateStandsOnItsKey() {
    Run run;
    if (!Begin(kRedLevel, run)) return;
    const RedKey::State& red = run.level.redKey;
    CHECK_MSG(red.present && red.name == kRedState, "level18c's red_key_state, got '" + red.name + "'");
    CHECK(red.atPx == glm::dvec2(440.0, 128.0));
    CHECK_EQ(red.keyNodes.size(), static_cast<std::size_t>(1));
    if (!red.keyNodes.empty()) CHECK(red.keyNodes.front() == kRedKey);
    // The decode's numbers, read from keys.json.
    CHECK(red.rules.keyEntity == "key" && red.rules.wallEntity == "breakable_wall");
    CHECK(::test::nearly(static_cast<float>(red.rules.fadeMs), 6000.0f));
    CHECK(red.rules.colour == glm::dvec3(1.0, 0.1, 0.1));
    CHECK(red.rules.scale == glm::dvec2(0.8, 1.5));
    // Game::start's own LoadScene bucket, vector2(128, 128) x getScale() over positions
    // x getScale(): 128 of the level's units (step 118's fix round).
    CHECK(red.rules.bucketUnits == 128.0);
    // Its bucket, the key's and the wall's are one: (3, 1), x 384-512, y 128-256 - the
    // state and the key on that row's top edge.
    const Keys::Key* key = run.level.keys.FindKey(kRedKey);
    CHECK(key != nullptr);
    if (key != nullptr) {
        CHECK(key->colour == "red");
        CHECK(red.BucketOf(key->atPx) == red.BucketOf(red.atPx));
    }
    CHECK(red.BucketOf(red.atPx) == glm::ivec2(3, 1));
    bool wallSeen = false;
    for (const Tscn::Node& node : run.data.scene.nodes) {
        if (node.name != kRedWall) continue;
        const Tscn::Value* position = node.Find("position");
        if (position == nullptr || position->kind != Tscn::Value::Kind::Vector2) break;
        wallSeen = true;
        CHECK(red.BucketOf(glm::dvec2(position->numbers[0], position->numbers[1])) == glm::ivec2(3, 1));
    }
    CHECK_MSG(wallSeen, "level18c's breakable_wall_2756 and its position");
    // The curve at its ends and its middle: 1, 1 - sin(PIb (1 - sin(PIb / 2))), 0.
    CHECK(std::fabs(red.BiasAt(0.0) - 1.0) < 1e-6);
    CHECK_MSG(std::fabs(red.BiasAt(3000.0) - 0.556) < 1e-3, "at 3000 ms " + std::to_string(red.BiasAt(3000.0)));
    CHECK(std::fabs(red.BiasAt(6000.0)) < 1e-6);
    // A level that places none has none.
    Run other;
    if (Begin(kLevel, other)) CHECK(!other.level.redKey.present);
}

void theRedKeyTakenFadesTheLightThenBreaksTheWall() {
    Run run;
    if (!Begin(kRedLevel, run)) return;
    const Demolish::Breakable* wall = run.level.demolish.FindBreakable(kRedWall);
    CHECK_MSG(wall != nullptr, "level18c's breakable_wall_2756");
    if (wall == nullptr || run.level.player == entt::null) return;
    const entt::entity wallBody = wall->body;
    const RedKey::State& red = run.level.redKey;

    // Left lying, nothing happens: the player floats at its spawn, weightless.
    for (int tick = 0; tick < 120; ++tick) Tick(run);
    CHECK_MSG(!red.destroyed && red.bias == 1.0 && !wall->broken, "an untaken key fades nothing");

    // Taken: on that tick the sound's moment, and the clock starts on the next.
    PutAt(run.registry, run.level.player, glm::dvec2(440.0, 128.0));
    int taken = -1;
    for (int tick = 1; tick <= 5 && taken < 0; ++tick) {
        Tick(run);
        if (red.destroyed) taken = tick;
    }
    const Keys::Key* key = run.level.keys.FindKey(kRedKey);
    CHECK_MSG(taken == 1, "the key's taking seen on its own tick, got " + std::to_string(taken));
    CHECK(key != nullptr && key->owner == run.level.player);
    CHECK(red.takenBy == kRedKey);
    CHECK_MSG(red.elapsedMs == 0.0 && red.bias == 1.0, "nothing faded on the tick it was taken");
    const glm::dvec3 file(0.8, 0.8, 0.9); // level18c's eth_ambient
    const Lighting::Rules lighting = run.data.lighting;
    const Torch::State noTorch;
    CHECK(Lighting::Ambient(lighting, file, false, noTorch, red) == file);

    // Half way: 180 ticks of 1/60 s after the taking.
    int since = 0; // ticks since the tick it was taken on
    for (; since < 180; ++since) Tick(run);
    CHECK_MSG(std::fabs(red.elapsedMs - 3000.0) < 0.01, "3000 ms on, got " + std::to_string(red.elapsedMs));
    CHECK_MSG(std::fabs(red.bias - 0.556) < 1e-3, "bias 0.556, got " + std::to_string(red.bias));
    const glm::dvec3 half = Lighting::Ambient(lighting, file, false, noTorch, red);
    CHECK_MSG(glm::length(half - file * red.bias) < 1e-12, "the ambient is the start's times the bias");
    CHECK(!wall->broken && run.registry.valid(wallBody));

    // 6000 ms: 360 ticks after the taking, the state goes and the wall with it.
    int broken = -1;
    while (since < 400 && broken < 0) {
        Tick(run);
        ++since;
        if (wall->broken) broken = since;
    }
    CHECK_MSG(broken == 360, "the wall broken 360 ticks after the taking, got " + std::to_string(broken));
    CHECK(red.gone && red.wallName == kRedWall);
    CHECK_MSG(wall->brokenBy == kRedState, "broken by the red key state, got '" + wall->brokenBy + "'");
    CHECK_MSG(!run.registry.valid(wallBody), "its body gone");
    CHECK_MSG(red.bias < 1e-6, "and the light with it: bias " + std::to_string(red.bias));
    CHECK(glm::length(Lighting::Ambient(lighting, file, false, noTorch, red)) < 1e-6);
    CHECK_MSG(!run.level.hazards.playerDied, "the player in the key's room lives through it");
}

// SeekEntity reads the WHOLE name: breakable_wall.ent is not breakable_wall, and a
// broken wall is not found twice.
void theRedKeysWallIsSoughtByItsExactName() {
    Run run;
    if (!Begin(kRedLevel, run)) return;
    CHECK(run.level.demolish.SeekAndDestroy("breakable_wall.ent", "a test").empty());
    CHECK(run.level.demolish.SeekAndDestroy("breakable_wall", "a test") == kRedWall);
    CHECK_EQ(run.level.demolish.Destroyed(run.registry).size(), static_cast<std::size_t>(1));
    CHECK(run.level.demolish.SeekAndDestroy("breakable_wall", "a test").empty());
}

// ---- the red key's own light (planning step 168; keys.json red_light) ----------------------
//
// ETHCallback_key #119-186: for a key whose `color` datum is 'red' and that is not hidden, every call adds
// AddLight(GetPosition() + (0, 0, 6 x scale), (1, 0, 0), 48 x scale, false). The numbers are read from the file;
// the rule is the colour test, and the census says which keys of the 128 levels it reaches.

void theRedKeysOwnLightIsTheDecodedRule() {
    Run run;
    if (!Begin(kRedLevel, run)) return;
    const Keys::Rules::OwnLight& light = run.level.keys.rules.ownLight;
    CHECK_MSG(light.colourName == "red", "the datum it tests for: " + light.colourName);
    CHECK_MSG(light.colour == glm::dvec3(1.0, 0.0, 0.0), "AddLight's colour is red (#162-166, pushed z, y, x)");
    CHECK_MSG(light.rangeUnits == 48.0, "the range, 48 x scale (#160)");
    // 6, and over the key's z: #168-173 push (6 x scale, 0, 0) for a vector3 built z first, so it is a height
    // (planning step 69 read it as an x offset).
    CHECK_MSG(light.heightUnits == 6.0, "the height over the key's own z, 6 x scale (#168)");
    const Keys::Key* red = run.level.keys.FindKey(kRedKey);
    CHECK(red != nullptr);
    if (red != nullptr) CHECK_MSG(Keys::HasOwnLight(run.level.keys.rules, *red), "level18c's red key has it");
    Run yellow;
    if (Begin(kLevel, yellow)) {
        const Keys::Key* key = yellow.level.keys.FindKey(kKey);
        CHECK(key != nullptr);
        if (key != nullptr) CHECK_MSG(!Keys::HasOwnLight(yellow.level.keys.rules, *key), "a yellow key does not");
    }
}

// WHICH KEYS: of the 128 levels' 39 keys exactly two are red - level18c's key_2701 (4-19) and level19c's key_1335
// (4-20) - and none is hidden (the datum #64-88 of scaleEntities read), which is why the port carries no hidden
// flag; the boss's drops are yellow (ghost.json, darkdragon.json). A converted level that placed a third red key,
// or hid one, would have to be looked at.
void onlyTwoKeysAreRedAndNoneIsHidden() {
    int keys = 0;
    std::vector<std::string> red;
    int hidden = 0;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(kLevels, ec)) {
        if (entry.path().extension() != ".tscn") continue;
        std::ifstream file(entry.path(), std::ios::binary);
        std::ostringstream text;
        text << file.rdbuf();
        Tscn::Scene scene;
        std::string error;
        if (!Tscn::Parse(text.str(), scene, error)) {
            CHECK_MSG(false, entry.path().filename().string() + ": " + error);
            continue;
        }
        for (const Tscn::Node& node : scene.nodes) {
            if (node.parent != ".") continue;
            const Tscn::Value* name = node.Meta("entity_name");
            if (name == nullptr || (name->text != "key" && name->text != "key.ent")) continue;
            ++keys;
            if (node.Meta("hidden") != nullptr) ++hidden;
            const Tscn::Value* colour = node.Meta("color");
            if (colour != nullptr && colour->text == "red") red.push_back(entry.path().stem().string() + "/" + node.name);
        }
    }
    std::sort(red.begin(), red.end());
    CHECK_MSG(keys == 39, std::to_string(keys) + " keys placed in the levels (keys.json counts 39)");
    CHECK_MSG(red.size() == 2 && red[0] == std::string("level18c/") + kRedKey && red[1] == "level19c/key_1335",
              "the red keys: " + std::to_string(red.size()));
    CHECK_MSG(hidden == 0, std::to_string(hidden) + " hidden key(s)");
}

void runTests() {
    theRedKeysOwnLightIsTheDecodedRule();
    onlyTwoKeysAreRedAndNoneIsHidden();
    theOneRangeServesBothTests();
    aKeyIsTakenAtTwentySixPixels();
    aCarriedKeyTrailsItsOwner();
    aMinionCanCarryAKey();
    theCarryRunsFromTheCarriersPoint();
    unlockingHoldsThenFadesThenTakesTheDoor();
    twoPairsOfOneColourStillPairCorrectly();
    everyLevelWithAKeyholePairsIt();
    theRedKeyStateStandsOnItsKey();
    theRedKeyTakenFadesTheLightThenBreaksTheWall();
    theRedKeysWallIsSoughtByItsExactName();
}

} // namespace

MP_SUITE_MAIN("test_mp_keys") {
    std::error_code ec;
    if (!std::filesystem::is_directory(kLevels, ec) ||
        !std::filesystem::is_regular_file(kData + "/entity_roles.json", ec)) {
        std::printf("test_mp_keys SKIPPED - needs the converted levels at %s and Enhanced's data at %s\n",
                    kLevels.c_str(), kData.c_str());
        return 77;
    }
    runTests();
    return ::test::summary("test_mp_keys", 40);
}
