// A level that is left takes its bodies with it (planning step 242, RESTART-1).
//
// WHAT WENT WRONG. A knight's body is built by LevelBuilder::BuildRigidCircle, which adds it to no Built ("the caller owns it"), and the layer's
// unloadLevel destroyed every other body of a level but those. Every retry, skip and menu exit therefore left each knight that was alive as an
// invisible, solid, dynamic sphere in the registry, for the rest of the run. Two things follow from the way the sim reads the registry, and both were
// reported by a player: a button is pressed by ANY dynamic body that overlaps its box (Trigger::DynamicBodiesIn: no contact counts, no owner), so a
// leftover knight pressed it for ever; and a new knight that something solid and invisible held up a little short of its waypoint's arrival circle
// walked in place with its legs cycling, because the walk arm writes its velocity every tick and has no stall handling (a faithful decode of the
// original's script: it is not changed here).
//
// WHAT THIS SUITE HOLDS. Nothing here compares a retried life with a fresh load tick by tick: their first ticks differ by a measured lag per body whose
// cause nobody has found (planning step 242, LEFT FOR THE OWNER), so what is compared is what must not depend on it.
//  - A CENSUS, the cheapest assertion and the one that does not care about timing: the rigid bodies, the colliders, the transforms and the bodies
//    wearing a knight's name, of a fresh attach, of a level after three retries and of a level reached again by way of another one, all at the same
//    age. The same census with a knight-free level as the control (a harness that could not pass would show there first).
//  - Every level of the game, attached, ticked and detached, leaves no rigid body and no collider behind: the grep of every `registry.create` as a
//    measurement, so a class of body some later step builds outside a Built and forgets is heard of on the first level that has one.
//  - Two behaviours the leak produced, pinned with conditions DERIVED at run time from a fresh load of the same level (nothing is committed from the
//    converted levels but the names of a few of their nodes): a button that a knight stood on when the level was retried is not pressed on the new
//    life's first tick, nor before a fresh life's knight reaches it; and after three retries the knights of two levels stand where a fresh load's do
//    and none walks in place (the stall detector lives in THIS test: the sim has none).
//
// MUTATION CHECK (the step's record has the run): with the destroy loop for the knights' bodies out of MagicPortalsLayer::unloadLevel, the census, the
// detach sweep and the pins fail; with it in, they pass. The knight-free control passes either way.
//
// No window and no Vulkan, as test_mp_layer: the app steps physics once before each layer tick, so this suite does the same.

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "MagicPortalsLayer.hpp"

#include "core/Components.hpp"
#include "core/Input.hpp"
#include "core/PhysicsSystem.hpp"
#include "core/ViewportInfo.hpp"

#include "sim/Game.hpp"
#include "sim/Units.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

using namespace Supersonic;
using MagicPortals::MagicPortalsLayer;
namespace Minions = MagicPortals::Minions;
namespace Units = MagicPortals::Units;

namespace {

const std::string kLevels = MAGICPORTALS_LEVELS_DIR;
const std::string kData = MAGICPORTALS_DATA_DIR;
const std::string kChapters = MAGICPORTALS_CHAPTERS_FILE;
const glm::vec2 kRest(640.0f, 360.0f);

// The script's tick, in milliseconds (clock.json): a level's age gains this much a tick, so ages are compared in it.
constexpr double kScriptMs = 16.0;

// The audit that found the leak counted these: the levels of the four chapters that place at least one knight.
constexpr std::size_t kKnightLevelsAudited = 35;

// The levels the pins read, by the converted file's stem, and the one node the button pin names. The control is a knight-free level with buttons.
constexpr const char* kButtonLevel = "level14b"; // 3-15
constexpr const char* kButtonNode = "button_1656";
constexpr const char* kStallLevelA = "level15b"; // 3-16
constexpr const char* kStallLevelB = "level20b"; // 3-21
constexpr const char* kControlLevel = "level6c"; // 4-07: no knight

// How long a level lives for the census, and how many times it is retried: knights exist from the level's start, so a short life says it all and
// stays clear of anything timed (a launcher's first stone, a particle pool).
constexpr int kCensusTicks = 20;
constexpr int kRetries = 3;

// What the pins allow for the lag of the first ticks of a retried life (a measured tick or two; not understood): a knight's x in pixels is about a pixel a tick
// while it walks, and a button's first press lands within a few ticks of the fresh one.
constexpr double kLagPx = 2.5;
constexpr int kLagTicks = 3;

// The stall of the report: a knight in the walk arm that has not moved 0.05 px for 30 ticks in a row.
constexpr double kStallPx = 0.05;
constexpr int kStallTicks = 30;

// The control's player, after 400 ticks, against a fresh life's: the same place to within a pixel.
constexpr double kControlPlayerPx = 1.0;

// The levels with a knight, found by the first case from the converted levels themselves (a level whose sim holds a knight at attach).
std::vector<std::string> g_knightLevels;

// A scores.json with a gold medal on every level of the four chapters, in this suite's own saves folder: PressMenu(Level) does nothing for a level
// the save has not opened (LevelChooser::validateItem), and the away-and-back census presses one.
std::filesystem::path SavesDirectory() { return std::filesystem::temp_directory_path() / "supersonic-test-mp-restart-saves"; }

bool SeedSaves() {
    std::error_code ec;
    std::filesystem::create_directories(SavesDirectory(), ec);
    std::string text = "{\"version\":1,\"scores\":[";
    for (int world = 0; world < 4; ++world) {
        for (int level = 0; level < 32; ++level) {
            text += std::string(world == 0 && level == 0 ? "" : ",") + "{\"world\":" + std::to_string(world) + ",\"level\":" + std::to_string(level) +
                    ",\"medal\":3}";
        }
    }
    std::ofstream out(SavesDirectory() / "scores.json", std::ios::binary | std::ios::trunc);
    out << text << "]}";
    return out.good();
}

MagicPortalsLayer::Paths RestartPaths() {
    MagicPortalsLayer::Paths paths;
    paths.prisms = std::filesystem::temp_directory_path() / "supersonic-test-mp-restart";
    paths.saveDir = SavesDirectory().string();
    return paths;
}

void publishViewport(entt::registry& registry) {
    ViewportInfo viewport;
    viewport.rect.min = glm::vec2(0.0f, 0.0f);
    viewport.rect.max = glm::vec2(1280.0f, 720.0f);
    viewport.pointerOverGame = true;
    registry.ctx().insert_or_assign(viewport);
}

// One tick as the app runs it: the physics step, then the layer, with this tick's input fed through Input's replay path.
void tickWith(MagicPortalsLayer& layer, entt::registry& registry, std::vector<std::string> down, std::vector<std::string> pressed) {
    PhysicsSystem::Update(registry, MagicPortalsLayer::kTick);
    Input::TickInput input;
    input.mousePosition = kRest;
    input.down = std::move(down);
    input.pressed = std::move(pressed);
    Input::BeginReplayedTick(input);
    layer.OnFixedUpdate(registry, MagicPortalsLayer::kTick);
    Input::EndReplayedTick();
}

void idle(MagicPortalsLayer& layer, entt::registry& registry, int ticks = 1) {
    for (int tick = 0; tick < ticks; ++tick) tickWith(layer, registry, {}, {});
}

void press(MagicPortalsLayer& layer, entt::registry& registry, const char* action) { tickWith(layer, registry, {action}, {action}); }

// A layer on one level of the converted game, with the registry it lives in. The destructor detaches, as the app does.
struct Rig {
    entt::registry registry;
    MagicPortalsLayer layer;
    bool ok = false;
    bool attached = false;

    explicit Rig(const std::string& level) : layer(RestartPaths(), level) {
        publishViewport(registry);
        layer.OnAttach(registry);
        attached = true;
        ok = layer.LoadError().empty() && layer.SimLevel() != nullptr;
        CHECK_MSG(ok, level + ": " + layer.LoadError());
    }
    void Detach() {
        if (attached) layer.OnDetach(registry);
        attached = false;
    }
    ~Rig() { Detach(); }
    Rig(const Rig&) = delete;
    Rig& operator=(const Rig&) = delete;
};

bool IsAt(const MagicPortalsLayer& layer, const std::string& name) { return layer.Current() != nullptr && layer.Current()->name == name; }

// Ticks until the level is as old as `ageMs`: a retried life and a fresh one are then compared at the same moment of the level's own clock.
void TickToAge(MagicPortalsLayer& layer, entt::registry& registry, double ageMs) {
    for (int guard = 0; guard < 4000 && layer.LevelAgeMs() + 1e-9 < ageMs; ++guard) idle(layer, registry);
}

// The knights the sim holds that are still in the world: a body the floor, a stone or a shot took is gone from the registry and has a stale handle.
int KnightsAlive(const MagicPortalsLayer& layer, const entt::registry& registry) {
    if (layer.SimLevel() == nullptr) return 0;
    int alive = 0;
    for (const Minions::Minion& minion : layer.SimLevel()->minions.minions) {
        if (!minion.gone && minion.body != entt::null && registry.valid(minion.body)) ++alive;
    }
    return alive;
}

std::set<std::string> KnightNames(const MagicPortalsLayer& layer) {
    std::set<std::string> names;
    if (layer.SimLevel() == nullptr) return names;
    for (const Minions::Minion& minion : layer.SimLevel()->minions.minions) names.insert(minion.name);
    return names;
}

// ---- the census ----------------------------------------------------------------------------------------------------------------------------

struct Census {
    std::size_t rigidBodies = 0;
    std::size_t spheres = 0;
    std::size_t boxes = 0;
    std::size_t capsules = 0;
    std::size_t transforms = 0;
    // The bodies wearing a knight's name (the sim tags a knight's body with its marker's name), by name.
    std::map<std::string, int> knights;
    bool operator==(const Census&) const = default;
};

Census CensusOf(entt::registry& registry, const std::set<std::string>& knightNames) {
    Census census;
    census.rigidBodies = registry.view<RigidBodyComponent>().size();
    census.spheres = registry.view<SphereColliderComponent>().size();
    census.boxes = registry.view<BoxColliderComponent>().size();
    census.capsules = registry.view<CapsuleColliderComponent>().size();
    census.transforms = registry.view<TransformComponent>().size();
    for (auto [entity, tag] : registry.view<TagComponent>().each()) {
        (void)entity;
        if (knightNames.count(tag.tag) != 0) ++census.knights[tag.tag];
    }
    return census;
}

// What differs between two censuses, in words; empty when they are the same.
std::string DeltaOf(const Census& was, const Census& now) {
    std::string out;
    const auto say = [&out](const char* what, std::size_t before, std::size_t after) {
        if (before != after) out += std::string(what) + " " + std::to_string(before) + " -> " + std::to_string(after) + "; ";
    };
    say("rigid bodies", was.rigidBodies, now.rigidBodies);
    say("sphere colliders", was.spheres, now.spheres);
    say("box colliders", was.boxes, now.boxes);
    say("capsule colliders", was.capsules, now.capsules);
    say("transforms", was.transforms, now.transforms);
    std::set<std::string> names;
    for (const auto& [name, count] : was.knights) {
        (void)count;
        names.insert(name);
    }
    for (const auto& [name, count] : now.knights) {
        (void)count;
        names.insert(name);
    }
    for (const std::string& name : names) {
        const auto before = was.knights.find(name);
        const auto after = now.knights.find(name);
        const int a = before == was.knights.end() ? 0 : before->second;
        const int b = after == now.knights.end() ? 0 : after->second;
        if (a != b) out += "bodies named " + name + " " + std::to_string(a) + " -> " + std::to_string(b) + "; ";
    }
    return out;
}

struct Snapshot {
    bool ok = false;
    Census census;
    double ageMs = 0.0;
    int knightsAtExits = 0; // the knights alive each time the level was left on the way, summed: what a leak would add
    std::set<std::string> names;
};

// A level as it is `ticks` ticks after a fresh attach.
Snapshot FreshSnapshot(const std::string& level, int ticks) {
    Snapshot snapshot;
    Rig rig(level);
    if (!rig.ok) return snapshot;
    idle(rig.layer, rig.registry, ticks);
    snapshot.names = KnightNames(rig.layer);
    snapshot.census = CensusOf(rig.registry, snapshot.names);
    snapshot.ageMs = rig.layer.LevelAgeMs();
    snapshot.ok = true;
    return snapshot;
}

// Whether a press that was meant to load the level did: the layer is on it, with a sim, no menu, and a level that has just started.
bool Reloaded(const MagicPortalsLayer& layer, const std::string& level, const std::string& what) {
    const bool reloaded = IsAt(layer, level) && layer.SimLevel() != nullptr && layer.MenuScreen() == MagicPortalsLayer::Screen::None &&
                          layer.LevelAgeMs() < 2.0 * kScriptMs + 1e-9;
    CHECK_MSG(reloaded, level + ": " + what + " did not load the level again (a census that matches because the press was ignored proves nothing)");
    return reloaded;
}

// The level as it is at `fresh`'s age after `retries` retries, each made at that age.
Snapshot AfterRetries(const std::string& level, int retries, int ticks, const Snapshot& fresh) {
    Snapshot snapshot;
    Rig rig(level);
    if (!rig.ok) return snapshot;
    idle(rig.layer, rig.registry, ticks);
    for (int retry = 0; retry < retries; ++retry) {
        snapshot.knightsAtExits += KnightsAlive(rig.layer, rig.registry);
        press(rig.layer, rig.registry, MagicPortalsLayer::kRetry);
        if (!Reloaded(rig.layer, level, "R")) return snapshot;
        TickToAge(rig.layer, rig.registry, fresh.ageMs);
    }
    snapshot.census = CensusOf(rig.registry, fresh.names);
    snapshot.ageMs = rig.layer.LevelAgeMs();
    snapshot.ok = true;
    return snapshot;
}

// The level as it is at `fresh`'s age after N, which loads the level after it (or the chapter's select screen, from a chapter's last), and the grid's
// own button for it back: the same press a tile of the level grid makes.
Snapshot AfterSkipAndBack(const std::string& level, int ticks, const Snapshot& fresh) {
    Snapshot snapshot;
    Rig rig(level);
    if (!rig.ok) return snapshot;
    idle(rig.layer, rig.registry, ticks);
    snapshot.knightsAtExits += KnightsAlive(rig.layer, rig.registry);
    press(rig.layer, rig.registry, MagicPortalsLayer::kSkip);
    const bool away = !(rig.layer.SimLevel() != nullptr && IsAt(rig.layer, level));
    CHECK_MSG(away, level + ": N took the layer away from the level (else the census below proves nothing)");
    if (!away) return snapshot;
    idle(rig.layer, rig.registry, 5);
    snapshot.knightsAtExits += KnightsAlive(rig.layer, rig.registry);

    const int index = rig.layer.ChapterTable().Find(level);
    CHECK_MSG(index >= 0, level + " is in chapters.json");
    if (index < 0) return snapshot;
    MagicPortalsLayer::MenuButton button;
    button.kind = MagicPortalsLayer::MenuButton::Kind::Level;
    button.world = rig.layer.ChapterTable().levels[static_cast<std::size_t>(index)].world;
    button.level = index;
    const bool pressed = rig.layer.PressMenu(rig.registry, button);
    CHECK_MSG(pressed, level + ": the grid's button for the level loads it (the scratch save opens every level)");
    if (!pressed || !Reloaded(rig.layer, level, "the grid's button")) return snapshot;
    TickToAge(rig.layer, rig.registry, fresh.ageMs);
    snapshot.census = CensusOf(rig.registry, fresh.names);
    snapshot.ageMs = rig.layer.LevelAgeMs();
    snapshot.ok = true;
    return snapshot;
}

// ---- the cases -------------------------------------------------------------------------------------------------------------------------------

// EVERY LEVEL, attached, played a little and detached, leaves no body behind. This is also where the knight levels are found: the levels whose sim holds a
// knight the moment it is built.
void EveryLevelLeavesNoBodyBehindWhenItIsDetached() {
    std::vector<std::string> names;
    {
        Rig first("level0");
        if (!first.ok) return;
        for (const auto& level : first.layer.ChapterTable().levels) names.push_back(level.name);
    }
    CHECK_EQ(names.size(), std::size_t{128});
    g_knightLevels.clear();
    for (const std::string& name : names) {
        Rig rig(name);
        if (!rig.ok) continue;
        idle(rig.layer, rig.registry, 30);
        const std::size_t knights = rig.layer.SimLevel() != nullptr ? rig.layer.SimLevel()->minions.minions.size() : 0;
        if (knights > 0) g_knightLevels.push_back(name);
        rig.Detach();
        const Census left = CensusOf(rig.registry, {});
        CHECK_MSG(left.rigidBodies == 0 && left.spheres == 0 && left.boxes == 0 && left.capsules == 0,
                  name + " (" + std::to_string(knights) + " knights placed): detached, the registry still holds " + std::to_string(left.rigidBodies) +
                      " rigid bodies, " + std::to_string(left.spheres) + " sphere, " + std::to_string(left.boxes) + " box and " +
                      std::to_string(left.capsules) + " capsule colliders");
    }
    CHECK_MSG(g_knightLevels.size() == kKnightLevelsAudited, "the converted levels hold " + std::to_string(g_knightLevels.size()) +
                                                              " levels with a knight, and the audit of planning step 242 counted " + std::to_string(kKnightLevelsAudited));
    for (const char* wanted : {kButtonLevel, kStallLevelA, kStallLevelB}) {
        CHECK_MSG(std::find(g_knightLevels.begin(), g_knightLevels.end(), wanted) != g_knightLevels.end(), std::string(wanted) + " has a knight (the pins read it)");
    }
    CHECK_MSG(std::find(g_knightLevels.begin(), g_knightLevels.end(), kControlLevel) == g_knightLevels.end(), std::string(kControlLevel) + " has no knight (it is the control)");
}

// The census of one level: fresh, after three retries, and after away-and-back.
void CheckTheCensusOf(const std::string& level, bool expectKnights) {
    const Snapshot fresh = FreshSnapshot(level, kCensusTicks);
    if (!fresh.ok) return;
    CHECK_MSG(fresh.names.empty() == !expectKnights && fresh.census.knights.size() == fresh.names.size(),
              level + ": the fresh census sees the knights' bodies by name (" + std::to_string(fresh.names.size()) + " knights named, " +
                  std::to_string(fresh.census.knights.size()) + " found)");
    CHECK_MSG(fresh.census.rigidBodies > 0, level + ": the fresh census has bodies to count");

    const Snapshot retried = AfterRetries(level, kRetries, kCensusTicks, fresh);
    if (retried.ok) {
        CHECK_MSG(std::fabs(retried.ageMs - fresh.ageMs) < 1e-6, level + ": the retried life is censused at the fresh one's age");
        CHECK_MSG(retried.census == fresh.census, level + ": after " + std::to_string(kRetries) + " retries the registry differs from a fresh load: " +
                                                      DeltaOf(fresh.census, retried.census) + "(knights alive at the exits, summed: " +
                                                      std::to_string(retried.knightsAtExits) + ")");
    }
    const Snapshot back = AfterSkipAndBack(level, kCensusTicks, fresh);
    if (back.ok) {
        CHECK_MSG(std::fabs(back.ageMs - fresh.ageMs) < 1e-6, level + ": the level reached again is censused at the fresh one's age");
        CHECK_MSG(back.census == fresh.census, level + ": after N and the grid's button back the registry differs from a fresh load: " +
                                                   DeltaOf(fresh.census, back.census) + "(knights alive at the exits, summed: " +
                                                   std::to_string(back.knightsAtExits) + ")");
    }
}

void ALevelThatWasLeftLeavesNothingOfItsKnights() {
    for (const std::string& level : g_knightLevels) CheckTheCensusOf(level, true);
}

// The control: a level with buttons and no knight comes out of the same retries and the same away-and-back with the census of a fresh load, with or without
// the fix. If it did not, the fault would be in this rig and not in the leak.
void AKnightFreeLevelIsTheControl() {
    CheckTheCensusOf(kControlLevel, false);
}

// ---- the behaviours the leak produced ---------------------------------------------------------------------------------------------------------

const MagicPortals::Puzzle::Button* ButtonOf(const MagicPortalsLayer& layer, const char* name) {
    return layer.SimLevel() != nullptr ? layer.SimLevel()->channels.FindButton(name) : nullptr;
}

bool Pressed(const MagicPortalsLayer& layer, const char* name) {
    const MagicPortals::Puzzle::Button* button = ButtonOf(layer, name);
    return button != nullptr && button->pressed;
}

// 3-15: the knight of its first patrol stands on button_1656 for a stretch of every cycle. Retried while it does, the button must be up on the new life's first
// tick and stay up until the new life's own knight reaches it.
void ARetryWhileAKnightStandsOnAButtonLeavesTheButtonUp() {
    Rig rig(kButtonLevel);
    if (!rig.ok) return;
    CHECK_MSG(ButtonOf(rig.layer, kButtonNode) != nullptr, std::string(kButtonNode) + " is a button of " + kButtonLevel);
    if (ButtonOf(rig.layer, kButtonNode) == nullptr) return;

    // A fresh life: the tick the knight first presses it.
    int firstPressed = -1;
    for (int tick = 1; tick <= 900 && firstPressed < 0; ++tick) {
        idle(rig.layer, rig.registry);
        if (Pressed(rig.layer, kButtonNode)) firstPressed = tick;
    }
    CHECK_MSG(firstPressed > 30, "the knight reaches the button after a walk, not at once: first pressed on tick " + std::to_string(firstPressed));
    if (firstPressed <= 30) return;
    // And holds on it: the retry is made with the old knight standing there (the condition the pin is about).
    int held = 0;
    for (int tick = 0; tick < 40; ++tick) {
        idle(rig.layer, rig.registry);
        if (Pressed(rig.layer, kButtonNode)) ++held;
    }
    CHECK_MSG(held == 40, "the knight holds the button for the next 40 ticks (" + std::to_string(held) + " of 40): the retry is made with it standing there");

    press(rig.layer, rig.registry, MagicPortalsLayer::kRetry);
    if (!Reloaded(rig.layer, kButtonLevel, "R")) return;
    int pressedTooSoon = 0;
    int retryFirst = -1;
    for (int life = 1; life <= firstPressed + 20; ++life) {
        if (life > 1) idle(rig.layer, rig.registry);
        if (!Pressed(rig.layer, kButtonNode)) continue;
        if (retryFirst < 0) retryFirst = life;
        if (life < firstPressed - kLagTicks) ++pressedTooSoon;
    }
    // What the run measured, for the record: the pins above allow kLagTicks either way.
    std::printf("  test_mp_restart: %s on %s: first pressed on tick %d of a fresh life and on tick %d of the retried one\n", kButtonNode, kButtonLevel, firstPressed, retryFirst);
    CHECK_MSG(pressedTooSoon == 0, std::string(kButtonNode) + " was pressed on " + std::to_string(pressedTooSoon) + " ticks of the new life before the knight of that life could be there; first pressed on life tick " +
                                       std::to_string(retryFirst) + " (a fresh life: " + std::to_string(firstPressed) + ")");
    CHECK_MSG(retryFirst >= firstPressed - kLagTicks && retryFirst <= firstPressed + kLagTicks, "the new life's knight presses it on life tick " + std::to_string(retryFirst) +
                                                                                                    ", as a fresh life's does on " + std::to_string(firstPressed));
}

struct Sample {
    bool alive = false;
    bool walking = false;
    double x = 0.0;
};
using Trace = std::vector<std::vector<Sample>>;

// Every knight's x and whether it was in the walk arm, as the last tick left them.
std::vector<Sample> RowOf(Rig& rig) {
    std::vector<Sample> row;
    if (rig.layer.SimLevel() == nullptr) return row;
    for (const Minions::Minion& minion : rig.layer.SimLevel()->minions.minions) {
        Sample sample;
        sample.alive = !minion.gone && minion.body != entt::null && rig.registry.valid(minion.body);
        if (sample.alive) {
            sample.walking = minion.arm == Minions::Arm::Walk;
            sample.x = Units::ToPixels(rig.registry.get<TransformComponent>(minion.body).position).x;
        }
        row.push_back(sample);
    }
    return row;
}

// `ticks` ticks of the level the layer is on, each sampled after it. The first tick presses `firstKey` where there is one (a retry), so that the sample of
// index 0 is the new life's first tick; an empty trace if that press did not load the level again.
Trace Life(Rig& rig, int ticks, const char* firstKey, const std::string& level) {
    Trace trace;
    for (int tick = 0; tick < ticks; ++tick) {
        if (tick == 0 && firstKey != nullptr) {
            press(rig.layer, rig.registry, firstKey);
            if (!Reloaded(rig.layer, level, "the last R")) return Trace{};
        } else {
            idle(rig.layer, rig.registry);
        }
        trace.push_back(RowOf(rig));
    }
    return trace;
}

// The longest stretch of ticks a knight spent walking in place.
int LongestWalkInPlace(const Trace& trace, std::size_t knight) {
    int longest = 0;
    int run = 0;
    for (std::size_t tick = 1; tick < trace.size(); ++tick) {
        if (knight >= trace[tick].size() || knight >= trace[tick - 1].size()) return longest;
        const Sample& now = trace[tick][knight];
        const Sample& before = trace[tick - 1][knight];
        if (now.alive && before.alive && now.walking && std::fabs(now.x - before.x) < kStallPx) {
            ++run;
            longest = std::max(longest, run);
        } else {
            run = 0;
        }
    }
    return longest;
}

int MostWalkedInPlace(const Trace& trace) {
    int most = 0;
    const std::size_t knights = trace.empty() ? 0 : trace.back().size();
    for (std::size_t knight = 0; knight < knights; ++knight) most = std::max(most, LongestWalkInPlace(trace, knight));
    return most;
}

// A level after three retries (made near ticks 250, 400 and 550, as the report's measurements were) stands where a fresh load does, and none of its knights
// walks in place. The fresh load is the same level in a rig of its own, idle player and all, so what is compared is whatever the level does.
void AfterThreeRetriesTheKnightsStandWhereAFreshLoadsDo(const char* level) {
    Trace freshTrace;
    {
        Rig fresh(level);
        if (!fresh.ok) return;
        freshTrace = Life(fresh, 400, nullptr, level);
    }

    Rig rig(level);
    if (!rig.ok) return;
    idle(rig.layer, rig.registry, 250);
    // Three lives are left behind, the third of them whole: the fourth is the one recorded.
    int exits = KnightsAlive(rig.layer, rig.registry);
    for (const int span : {150, 150}) {
        press(rig.layer, rig.registry, MagicPortalsLayer::kRetry);
        if (!Reloaded(rig.layer, level, "R")) return;
        idle(rig.layer, rig.registry, span - 1);
        exits += KnightsAlive(rig.layer, rig.registry);
    }
    const Trace life = Life(rig, 400, MagicPortalsLayer::kRetry, level);

    const std::string what = std::string(level) + " after three retries (knights alive at the first three exits, summed: " + std::to_string(exits) + "): ";
    // What the run measured, for the record: the longest walk in place of each life, and the widest gap between the two lives' knights at the two ticks compared.
    double widest = 0.0;
    if (!freshTrace.empty() && !life.empty() && freshTrace.back().size() == life.back().size()) {
        for (const std::size_t at : {std::size_t{249}, std::size_t{399}}) {
            for (std::size_t knight = 0; knight < life[at].size(); ++knight) {
                if (freshTrace[at][knight].alive && life[at][knight].alive) widest = std::max(widest, std::fabs(freshTrace[at][knight].x - life[at][knight].x));
            }
        }
    }
    std::printf("  test_mp_restart: %s after three retries: longest walk in place %d ticks (a fresh life: %d), widest gap to a fresh life's knight %.3f px\n", level,
                MostWalkedInPlace(life), MostWalkedInPlace(freshTrace), widest);
    CHECK_MSG(MostWalkedInPlace(freshTrace) < kStallTicks, std::string(level) + ": a fresh idle life has no knight walking in place for " + std::to_string(kStallTicks) +
                                                               " ticks (the detector's own control): longest " + std::to_string(MostWalkedInPlace(freshTrace)));
    CHECK_MSG(MostWalkedInPlace(life) < kStallTicks, what + "a knight walked in place for " + std::to_string(MostWalkedInPlace(life)) + " ticks in a row");

    CHECK_MSG(!freshTrace.empty() && !life.empty() && freshTrace.back().size() == life.back().size() && !life.back().empty(), what + "the same knights stand in both lives");
    if (freshTrace.empty() || life.empty() || freshTrace.back().size() != life.back().size()) return;
    for (const std::size_t at : {std::size_t{249}, std::size_t{399}}) {
        for (std::size_t knight = 0; knight < life[at].size(); ++knight) {
            const Sample& was = freshTrace[at][knight];
            const Sample& now = life[at][knight];
            CHECK_MSG(was.alive == now.alive && (!was.alive || std::fabs(was.x - now.x) <= kLagPx),
                      what + "knight " + std::to_string(knight) + " on life tick " + std::to_string(at + 1) + " is " +
                          (now.alive ? std::to_string(now.x - was.x) + " px from where a fresh life has it" : std::string("gone where a fresh life has it standing")));
        }
    }
}

// The control of the pins: a level without a knight, retried three times, plays what a fresh load plays, button for button.
void AKnightFreeLevelPlaysTheSameAfterThreeRetries() {
    std::vector<std::vector<bool>> freshFlags;
    glm::dvec2 freshPlayer(0.0);
    {
        Rig fresh(kControlLevel);
        if (!fresh.ok) return;
        CHECK_MSG(!fresh.layer.SimLevel()->channels.buttons.empty() && fresh.layer.SimLevel()->minions.minions.empty(),
                  std::string(kControlLevel) + " has buttons and no knight");
        int pressedTicks = 0;
        for (int tick = 0; tick < 400; ++tick) {
            idle(fresh.layer, fresh.registry);
            std::vector<bool> row;
            bool any = false;
            for (const auto& button : fresh.layer.SimLevel()->channels.buttons) {
                row.push_back(button.pressed);
                any = any || button.pressed;
            }
            pressedTicks += any ? 1 : 0;
            freshFlags.push_back(row);
        }
        freshPlayer = Units::ToPixels(fresh.registry.get<TransformComponent>(fresh.layer.SimLevel()->player).position);
        // For the record: a control in which nothing is ever pressed would compare 400 ticks of "up" with 400 ticks of "up".
        std::printf("  test_mp_restart: %s, fresh life: a button is pressed on %d of 400 ticks\n", kControlLevel, pressedTicks);
    }
    Rig rig(kControlLevel);
    if (!rig.ok) return;
    idle(rig.layer, rig.registry, 250);
    for (const int span : {150, 150}) {
        press(rig.layer, rig.registry, MagicPortalsLayer::kRetry);
        if (!Reloaded(rig.layer, kControlLevel, "R")) return;
        idle(rig.layer, rig.registry, span - 1);
    }
    press(rig.layer, rig.registry, MagicPortalsLayer::kRetry);
    if (!Reloaded(rig.layer, kControlLevel, "the last R")) return;
    int different = 0;
    for (int tick = 0; tick < 400; ++tick) {
        if (tick > 0) idle(rig.layer, rig.registry);
        std::vector<bool> row;
        for (const auto& button : rig.layer.SimLevel()->channels.buttons) row.push_back(button.pressed);
        if (row != freshFlags[static_cast<std::size_t>(tick)]) ++different;
    }
    CHECK_MSG(different == 0, std::string(kControlLevel) + ": " + std::to_string(different) + " of 400 ticks have a button pressed that a fresh life's is not, or the other way");
    // And where the player rests after 400 ticks, the one thing the buttons' flags cannot say if no button is ever pressed in the level.
    const glm::dvec2 retriedPlayer = Units::ToPixels(rig.registry.get<TransformComponent>(rig.layer.SimLevel()->player).position);
    CHECK_MSG(glm::distance(retriedPlayer, freshPlayer) < kControlPlayerPx, std::string(kControlLevel) + ": after 400 ticks the player rests " + std::to_string(glm::distance(retriedPlayer, freshPlayer)) +
                                                                                " px from where a fresh life's rests");
}

void runTests() {
    // First: it finds the knight levels the rest runs over.
    EveryLevelLeavesNoBodyBehindWhenItIsDetached();
    AKnightFreeLevelIsTheControl();
    ALevelThatWasLeftLeavesNothingOfItsKnights();
    ARetryWhileAKnightStandsOnAButtonLeavesTheButtonUp();
    AfterThreeRetriesTheKnightsStandWhereAFreshLoadsDo(kStallLevelA);
    AfterThreeRetriesTheKnightsStandWhereAFreshLoadsDo(kStallLevelB);
    AKnightFreeLevelPlaysTheSameAfterThreeRetries();
}

} // namespace

MP_SUITE_MAIN("test_mp_restart") {
    std::error_code ec;
    if (!std::filesystem::is_directory(kLevels, ec) || !std::filesystem::is_regular_file(kChapters, ec) ||
        !std::filesystem::is_regular_file(kData + "/entity_roles.json", ec) || !std::filesystem::is_regular_file(kData + "/player.json", ec) ||
        !std::filesystem::is_regular_file(kData + "/portals.json", ec)) {
        std::printf("test_mp_restart: SKIPPED - needs the converted levels at %s,\n"
                    "  chapters.json at %s and Enhanced's data at %s.\n"
                    "  All live outside this repository; configure with\n"
                    "  -DSUPERSONIC_MAGICPORTALS_LEVELS=..., _CHAPTERS=... and _DATA=...\n",
                    kLevels.c_str(), kChapters.c_str(), kData.c_str());
        return 77;
    }
    if (!SeedSaves()) {
        std::printf("test_mp_restart: could not write its scratch save into %s\n", SavesDirectory().string().c_str());
        return 1;
    }
    runTests();
    return ::test::summary("test_mp_restart", 300);
}
