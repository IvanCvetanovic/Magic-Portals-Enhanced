// The bodies the engine steps in Box2D's order (planning step 197, CMP-18): game/data/contact_order.json as sim/ContactOrder reads
// and applies it.
//
// WHAT IS PINNED, AND WHAT EACH PIN FAILS UNDER (a pin with no mutation is a sentence, and these are checked by reading the code
// against them, since the C++ cannot be mutated here: the mutation is the edit named, and the check is the line that would go red).
//
//  1. The shipped table names ONE level and TWO bodies, the stone and the plank of 2-29, and every key is a level of the converted
//     directory.                 Fails under: a second level or body added to the table (widened by guess); the plank dropped (the
//     stone alone misses the plank gate on the real exe, 4.03 degrees at x 297 against 3); a typo in a key (a table that quietly
//     flags nothing).
//  2. The loader refuses what would be silent: an unreadable file, text that is not JSON, no `levels` object, a level whose entry is
//     not a list, an empty list, a body that is not a name, a name twice; each error names the level (and the node where there is
//     one), and a refused table leaves the caller's rules alone.   Fails under: a loader that skips a bad entry (the typo leaves a
//     body in the engine's order and nothing says so).
//  3. Apply names what it cannot flag, each in its own words: a node the level does not build, a static body or an area (no rigid
//     body), a kinematic mover (the port moves it). Game::Start refuses the level with the same message.   Fails under: an Apply that
//     skips a node it cannot find, or that gives the three cases one message (a fix to the table is then a guess).
//  4. In 2-29 the flag lands on exactly the stone and the plank: not the barrel (a flagged barrel meets the unflagged player for
//     nothing), not the player, not the door, not the anchor.   Fails under: the barrel added, a flag set on every rigid body.
//  5. In the other 127 converted levels no body is flagged.   Fails under: a table read by a prefix or by the entity's name instead
//     of the level's file name; a flag set by default; a leak of one level's key into the others.
//  6. With an empty table, or a copy of 2-29 under another file name, nothing is flagged: the unkeyed control the exes are compared
//     against is real.   Fails under: the flag set without the table asking.
//  7. The layer-driven 2-29 against the original's rig clip (out/parity/program/CMP18/pre/rig.md, the F18 clip, two takes), the five
//     readings with the bands of +-10 percent round them, frames paired as clip f = port frame + 1: creep 1.053 units a frame (0.95 to
//     1.16), free floor roll 2.025 (1.82 to 2.23), first plank turn f107 (96 to 118), landing f123.0 (111 to 135), the stone at x 153.3
//     on f183.5 (165 to 202). The same layer with the table emptied is outside them (creep 0.28, the stone at the barrel only on
//     474), so it is the table that moves the stone, and the idle player does not move with it.   Fails under: the flag not applied
//     (the keyed rows go to the control's numbers), the engine's field ignored or the deferred move lost (the stone goes nowhere or
//     creeps at 0.28), the flag set on every body (the player's path changes) or the step order wrong (the creep leaves its band).
//  8. The wall reading of the flagged stone (the finding of Part 0, section 5): a stone dropped onto breakable_wall_883 from a gap of
//     12 units or more breaks it flagged as it does unflagged, with margin (Box2D's reading 2.33 m/s and up against the gate 1.5). From
//     a gap of about 6 to 8 units the flagged stone is read one gravity step under Box2D's and the wall holds where Box2D's would break:
//     PRINTED, not pinned, because a per-body Impacts branch is not built and a pin would only fix the departure.   Fails under: the
//     flagged stone read so low that the falls the level can give leave the wall standing.
//
// Reads the converted levels and Enhanced's data from outside this repository, and skips, saying where it looked, when either is
// absent.

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "MagicPortalsLayer.hpp"
#include "core/Components.hpp"
#include "core/Input.hpp"
#include "core/PhysicsSystem.hpp"
#include "core/ViewportInfo.hpp"
#include "sim/ContactOrder.hpp"
#include "sim/Demolish.hpp"
#include "sim/Earthquake.hpp"
#include "sim/Game.hpp"
#include "sim/Hinge.hpp"
#include "sim/ScriptClock.hpp"
#include "sim/Units.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

using namespace MagicPortals;
using Supersonic::RigidBodyComponent;
using Supersonic::TransformComponent;

namespace {

const std::string kLevels = MAGICPORTALS_LEVELS_DIR;
const std::string kData = MAGICPORTALS_DATA_DIR;
const std::string kPortData = MAGICPORTALS_PORT_DATA_DIR;
constexpr float kTick = 1.0f / 60.0f;

constexpr const char* kLevel = "level28a";
constexpr const char* kStone = "rolling_stone_892";
constexpr const char* kPlank = "spinning_platform_no_emissive_ent_890";
constexpr const char* kBarrel = "barrel_bomb_ent_884";
constexpr const char* kAnchor = "anchor_885";
constexpr const char* kDoor = "door_lift_823";
constexpr const char* kWall = "breakable_wall_883";

std::filesystem::path Scratch() {
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "supersonic-test-mp-contact-order";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir;
}

std::string Write(const std::filesystem::path& path, const std::string& body) {
    std::ofstream file(path, std::ios::trunc | std::ios::binary);
    file << body;
    return path.string();
}

struct Run {
    Game::Data data;
    entt::registry registry;
    Game::Level level;
};

// LoadData, then `edit` of the table (an empty edit leaves the shipped one), then Start.
template <typename Edit>
bool Begin(const std::string& levelPath, Run& run, Edit edit, std::string& error) {
    if (!Game::LoadData(levelPath, kData, Scratch() / "prisms", run.data, error)) return false;
    edit(run.data.contactOrder);
    return Game::Start(run.data, run.registry, run.level, error);
}

bool Begin(const std::string& levelName, Run& run, std::string& error) {
    return Begin(kLevels + "/" + levelName + ".tscn", run, [](ContactOrder::Rules&) {}, error);
}

int Flagged(entt::registry& registry) {
    int count = 0;
    for (auto [entity, rigid] : registry.view<RigidBodyComponent>().each()) {
        (void)entity;
        if (rigid.integrateAfterSolve) ++count;
    }
    return count;
}

bool IsFlagged(const Run& run, const char* node) {
    const auto found = run.level.built.entities.find(node);
    if (found == run.level.built.entities.end() || !run.registry.valid(found->second)) return false;
    const auto* rigid = run.registry.try_get<RigidBodyComponent>(found->second);
    return rigid != nullptr && rigid->integrateAfterSolve;
}

bool Contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

// ---- 1. the shipped table ---------------------------------------------------------------------------------------------------

void TheShippedTableNamesOneLevelAndTwoBodies() {
    ContactOrder::Rules rules;
    std::string error;
    CHECK_MSG(ContactOrder::LoadRules(kPortData + "/contact_order.json", rules, error), kPortData + "/contact_order.json: " + error);
    CHECK_MSG(rules.byLevel.size() == 1, "the table names one level: " + std::to_string(rules.byLevel.size()));
    const auto listed = rules.byLevel.find(kLevel);
    CHECK_MSG(listed != rules.byLevel.end(), std::string(kLevel) + " is the level it names");
    if (listed == rules.byLevel.end()) return;
    const std::vector<std::string> expected{kStone, kPlank};
    CHECK_MSG(listed->second == expected, "and its stone and its plank, in that order, nothing else (the barrel is not flagged)");
    CHECK_MSG(ContactOrder::CheckLevels(rules, kLevels, error), "every key of the table is a level of " + kLevels + ": " + error);
}

// ---- 2. the loader's refusals ------------------------------------------------------------------------------------------------

struct Refusal {
    const char* what;
    const char* body;
    std::vector<std::string> inError; // what the error must say
};

void TheLoaderRefusesWhatWouldBeSilent() {
    const Refusal refusals[] = {
        {"text that is not JSON", "{ \"levels\": ", {}},
        {"no levels object", "{ \"_about\": [] }", {"no \"levels\" object"}},
        {"levels that is a list", "{ \"levels\": [] }", {"no \"levels\" object"}},
        {"an entry that is a string", "{ \"levels\": { \"level28a\": \"rolling_stone_892\" } }", {"level28a", "not a list"}},
        {"an empty list", "{ \"levels\": { \"level28a\": [] } }", {"level28a", "lists no node"}},
        {"a body that is a number", "{ \"levels\": { \"level28a\": [1] } }", {"level28a", "non-empty string"}},
        {"a body that is an empty name", "{ \"levels\": { \"level28a\": [\"\"] } }", {"level28a", "non-empty string"}},
        {"a name twice", "{ \"levels\": { \"level28a\": [\"rolling_stone_892\", \"rolling_stone_892\"] } }",
         {"level28a", "rolling_stone_892", "twice"}},
    };
    int n = 0;
    for (const Refusal& refusal : refusals) {
        const std::string path = Write(Scratch() / ("refused-" + std::to_string(++n) + ".json"), refusal.body);
        // A sentinel: a refused table must leave what the caller held alone.
        ContactOrder::Rules rules;
        rules.byLevel["sentinel"] = {"kept"};
        std::string error;
        CHECK_MSG(!ContactOrder::LoadRules(path, rules, error), std::string(refusal.what) + " is refused");
        CHECK_MSG(!error.empty(), std::string(refusal.what) + ": the refusal says something");
        for (const std::string& part : refusal.inError) {
            CHECK_MSG(Contains(error, part), std::string(refusal.what) + ": the error names '" + part + "': " + error);
        }
        CHECK_MSG(rules.byLevel.size() == 1 && rules.byLevel.count("sentinel") == 1, std::string(refusal.what) + ": the caller's rules are left alone");
    }
    {
        ContactOrder::Rules rules;
        std::string error;
        CHECK_MSG(!ContactOrder::LoadRules((Scratch() / "no-such-table.json").string(), rules, error) && Contains(error, "cannot read"),
                  "a table that is not there is refused, saying so: " + error);
    }
    {
        // An empty `levels` is a valid table, the control every exe is first run on, and a key beginning with an underscore at the top is
        // a comment like the others in game/data.
        const std::string path = Write(Scratch() / "empty-levels.json", "{ \"_about\": [\"x\"], \"levels\": {} }");
        ContactOrder::Rules rules;
        rules.byLevel["sentinel"] = {"replaced"};
        std::string error;
        CHECK_MSG(ContactOrder::LoadRules(path, rules, error), "an empty levels object is a valid table: " + error);
        CHECK_MSG(rules.byLevel.empty(), "and it takes no body");
    }
    {
        // A key that is no level is refused by the check against the directory of levels, not by the loader (which sees one level at a time).
        ContactOrder::Rules rules;
        rules.byLevel["level28z"] = {kStone};
        rules.byLevel[kLevel] = {kStone};
        std::string error;
        CHECK_MSG(!ContactOrder::CheckLevels(rules, kLevels, error), "a key that is no level is refused");
        CHECK_MSG(Contains(error, "level28z"), "naming it: " + error);
    }
}

// ---- 3. Apply names what it cannot flag --------------------------------------------------------------------------------------

void ApplyNamesWhatItCannotFlag() {
    struct Case {
        const char* node;
        const char* says;    // the words of its own case
        const char* neither; // and a word of another case that must not be there
    };
    const Case cases[] = {
        {"no_such_node", "does not build", "kinematic"},
        {kAnchor, "static body or an area", "kinematic"},
        {kDoor, "kinematic mover", "static body"},
    };
    for (const Case& one : cases) {
        Run run;
        std::string error;
        // Started with an empty table so that Start is not what refuses; Apply is then called on the level it built.
        if (!Begin(kLevels + "/" + kLevel + ".tscn", run, [](ContactOrder::Rules& rules) { rules.byLevel.clear(); }, error)) {
            CHECK_MSG(false, std::string(kLevel) + ": " + error);
            continue;
        }
        CHECK_MSG(Flagged(run.registry) == 0, "started on an empty table: no body is flagged");
        ContactOrder::Rules rules;
        rules.byLevel[kLevel] = {one.node};
        CHECK_MSG(!ContactOrder::Apply(run.registry, rules, kLevel, run.level.built.entities, error), std::string(one.node) + " cannot be flagged");
        CHECK_MSG(Contains(error, kLevel) && Contains(error, one.node), std::string(one.node) + ": the error names the level and the node: " + error);
        CHECK_MSG(Contains(error, one.says), std::string(one.node) + ": and says '" + one.says + "': " + error);
        CHECK_MSG(!Contains(error, one.neither), std::string(one.node) + ": and not the words of another case: " + error);
        CHECK_MSG(Flagged(run.registry) == 0, std::string(one.node) + ": nothing was flagged");

        // The level itself is refused at its start with that message: the table is what is wrong, and the level does not run on.
        Run refused;
        std::string started;
        const bool ok = Begin(kLevels + "/" + kLevel + ".tscn", refused, [&](ContactOrder::Rules& table) { table = rules; }, started);
        CHECK_MSG(!ok && started == error, std::string(one.node) + ": Game::Start refuses the level with it: " + started);
    }
    {
        // A level the table does not list takes nothing, and Apply says so by saying nothing.
        Run run;
        std::string error;
        if (!Begin(kLevels + "/" + kLevel + ".tscn", run, [](ContactOrder::Rules& rules) { rules.byLevel.clear(); }, error)) {
            CHECK_MSG(false, error);
            return;
        }
        ContactOrder::Rules rules;
        rules.byLevel["level0"] = {kStone};
        CHECK_MSG(ContactOrder::Apply(run.registry, rules, kLevel, run.level.built.entities, error), "a level the table does not list: " + error);
        CHECK_MSG(Flagged(run.registry) == 0, "takes no flag");
    }
}

// ---- 4. to 6. who is flagged -------------------------------------------------------------------------------------------------

void The2_29FlagLandsOnTheStoneAndThePlankAlone() {
    Run run;
    std::string error;
    if (!Begin(kLevel, run, error)) {
        CHECK_MSG(false, std::string(kLevel) + ": " + error);
        return;
    }
    CHECK_MSG(IsFlagged(run, kStone), "the stone is stepped in Box2D's order");
    CHECK_MSG(IsFlagged(run, kPlank), "and so is the plank it tips");
    CHECK_MSG(!IsFlagged(run, kBarrel), "the barrel is not");
    CHECK_MSG(!IsFlagged(run, kDoor) && !IsFlagged(run, kAnchor), "nor the door, nor the anchor");
    const auto* player = run.registry.try_get<RigidBodyComponent>(run.level.player);
    CHECK_MSG(player != nullptr && !player->integrateAfterSolve, "nor the player");
    CHECK_MSG(Flagged(run.registry) == 2, "two bodies in all: " + std::to_string(Flagged(run.registry)));
    const auto* stone = run.registry.try_get<RigidBodyComponent>(run.level.built.entities.at(kStone));
    CHECK_MSG(stone != nullptr && !stone->isKinematic, "and the stone is a loose body");
}

std::vector<std::string> ConvertedLevels() {
    std::vector<std::string> names;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(kLevels, ec)) {
        if (entry.path().extension() == ".tscn") names.push_back(entry.path().stem().string());
    }
    std::sort(names.begin(), names.end());
    return names;
}

void NoBodyOfTheOther127LevelsIsFlagged() {
    const std::vector<std::string> levels = ConvertedLevels();
    CHECK_MSG(levels.size() == 128, "the converted directory holds the game's 128 levels: " + std::to_string(levels.size()));
    int visited = 0;
    int bodies = 0;
    int flaggedElsewhere = 0;
    std::string first;
    for (const std::string& name : levels) {
        Run run;
        std::string error;
        if (!Begin(name, run, error)) {
            CHECK_MSG(false, name + ": " + error);
            continue;
        }
        ++visited;
        const int flagged = Flagged(run.registry);
        for (auto [entity, rigid] : run.registry.view<RigidBodyComponent>().each()) {
            (void)entity;
            (void)rigid;
            ++bodies;
        }
        if (name == kLevel) {
            CHECK_MSG(flagged == 2, "2-29 flags two bodies: " + std::to_string(flagged));
            continue;
        }
        flaggedElsewhere += flagged;
        if (flagged != 0 && first.empty()) first = name;
    }
    std::printf("  %d levels started, %d rigid bodies in all, %d flagged outside 2-29\n", visited, bodies, flaggedElsewhere);
    CHECK_MSG(visited == 128, "all 128 levels start: " + std::to_string(visited));
    CHECK_MSG(flaggedElsewhere == 0, "no body of any other level is flagged (first: " + first + ")");
}

void AnEmptyTableOrACopyUnderAnotherNameFlagsNothing() {
    {
        Run run;
        std::string error;
        if (!Begin(kLevels + "/" + kLevel + ".tscn", run, [](ContactOrder::Rules& rules) { rules.byLevel.clear(); }, error)) {
            CHECK_MSG(false, error);
            return;
        }
        CHECK_MSG(Flagged(run.registry) == 0, "an empty table flags nothing in 2-29");
    }
    {
        // The table is keyed by the file's name: a copy of 2-29 under another one is the control, the engine's own order.
        std::error_code ec;
        const std::filesystem::path copy = Scratch() / "copy-of-2-29.tscn";
        std::filesystem::copy_file(kLevels + "/" + kLevel + ".tscn", copy, std::filesystem::copy_options::overwrite_existing, ec);
        CHECK_MSG(!ec, "a copy of 2-29 is made: " + ec.message());
        Run run;
        std::string error;
        if (!Begin(copy.string(), run, [](ContactOrder::Rules&) {}, error)) {
            CHECK_MSG(false, "the copy starts: " + error);
            return;
        }
        CHECK_MSG(run.data.levelName == "copy-of-2-29", "the copy is keyed by its own name: " + run.data.levelName);
        CHECK_MSG(Flagged(run.registry) == 0, "and takes no flag, though the shipped table is read");
    }
}

// ---- 7. the layer-driven 2-29 ------------------------------------------------------------------------------------------------

void PublishViewport(entt::registry& registry) {
    Supersonic::ViewportInfo viewport;
    viewport.rect.min = glm::vec2(0.0f, 0.0f);
    viewport.rect.max = glm::vec2(1280.0f, 720.0f);
    viewport.pointerOverGame = true;
    registry.ctx().insert_or_assign(viewport);
}

struct Series {
    std::vector<glm::dvec2> stone;
    std::vector<double> turned; // the plank's angle off its rest, rad
    std::vector<glm::dvec2> barrel;
    std::vector<glm::dvec2> player;
};

// 2-29 played by the layer for `frames` frames with nobody pressing anything, as the app runs one under --fixed-step: the physics step,
// the layer's fixed tick, the layer's frame. `portData` empty: the shipped game/data; else a directory to read the port's data from.
bool Drive(const std::string& portData, int frames, Series& series) {
    entt::registry registry;
    PublishViewport(registry);
    MagicPortalsLayer::Paths paths;
    paths.prisms = Scratch() / "prisms";
    if (!portData.empty()) paths.portData = portData;
    MagicPortalsLayer layer(paths, kLevel);
    layer.OnAttach(registry);
    const Game::Level* level = layer.SimLevel();
    if (level == nullptr || level->hinge.hinges.empty()) {
        CHECK_MSG(false, "the layer started 2-29");
        return false;
    }
    const Hinge::Hinged& hinged = level->hinge.hinges.front();
    const entt::entity stone = level->built.entities.at(kStone);
    const entt::entity barrel = level->built.entities.at(kBarrel);
    const entt::entity player = level->player;
    const auto pixels = [&](entt::entity entity) { return Units::ToPixels(registry.get<TransformComponent>(entity).position); };
    const auto record = [&]() {
        series.stone.push_back(pixels(stone));
        series.turned.push_back(Hinge::AngleOf(registry, hinged) - hinged.restRad);
        series.barrel.push_back(pixels(barrel));
        series.player.push_back(pixels(player));
    };
    record();
    for (int frame = 1; frame <= frames; ++frame) {
        Supersonic::PhysicsSystem::Update(registry, MagicPortalsLayer::kTick);
        Supersonic::Input::TickInput input;
        input.mousePosition = glm::vec2(640.0f, 360.0f);
        Supersonic::Input::BeginReplayedTick(input);
        layer.OnFixedUpdate(registry, MagicPortalsLayer::kTick);
        Supersonic::Input::EndReplayedTick();
        layer.OnUpdate(registry, MagicPortalsLayer::kTick);
        record();
    }
    layer.OnDetach(registry);
    return true;
}

struct Readings {
    double creep = -1.0;     // units a frame between x 400 and x 340, 60 units over the frames between
    double firstTurn = -1.0; // the first frame the plank is more than 0.05 rad off its rest
    double landing = -1.0;   // y first over 190
    double contact = -1.0;   // x first at or under 153.3
    double floorRoll = -1.0; // units a frame from 10 frames after the landing to 5 before the contact
};

double Crossing(const std::vector<glm::dvec2>& path, double x0) {
    for (std::size_t i = 1; i < path.size(); ++i) {
        if (path[i].x <= x0 && path[i - 1].x > x0) {
            return static_cast<double>(i - 1) + (path[i - 1].x - x0) / (path[i - 1].x - path[i].x);
        }
    }
    return -1.0;
}

Readings Read(const Series& s) {
    Readings r;
    const double a = Crossing(s.stone, 400.0);
    const double b = Crossing(s.stone, 340.0);
    if (a >= 0.0 && b >= 0.0) r.creep = 60.0 / (b - a);
    r.contact = Crossing(s.stone, 153.3);
    for (std::size_t i = 1; i < s.stone.size(); ++i) {
        if (s.stone[i].y > 190.0 && s.stone[i - 1].y <= 190.0) {
            r.landing = static_cast<double>(i - 1) + (190.0 - s.stone[i - 1].y) / (s.stone[i].y - s.stone[i - 1].y);
            break;
        }
    }
    // The plank's rest is the median of its turn over frames 20 to 60, while the stone is still on the block.
    std::vector<double> window(s.turned.begin() + 20, s.turned.begin() + 60);
    std::sort(window.begin(), window.end());
    const double rest = window[window.size() / 2];
    for (std::size_t i = 20; i < s.turned.size(); ++i) {
        if (std::fabs(s.turned[i] - rest) > 0.05) {
            r.firstTurn = static_cast<double>(i);
            break;
        }
    }
    if (r.landing >= 0.0 && r.contact > r.landing + 20.0) {
        const auto from = static_cast<std::size_t>(std::floor(r.landing)) + 10;
        const auto to = static_cast<std::size_t>(std::floor(r.contact)) - 5;
        if (to > from && to < s.stone.size()) r.floorRoll = (s.stone[from].x - s.stone[to].x) / static_cast<double>(to - from);
    }
    return r;
}

void Print(const char* which, const Readings& r) {
    std::printf("  2-29 %s: creep %.4f u/frame, free floor roll %.4f, first plank turn frame %.1f (clip f %.1f), landing %.2f (f %.2f), stone at the barrel %.2f (f %.2f)\n",
                which, r.creep, r.floorRoll, r.firstTurn, r.firstTurn + 1.0, r.landing, r.landing + 1.0, r.contact, r.contact + 1.0);
}

bool Within(double value, double low, double high) {
    return value >= low && value <= high;
}

// A copy of the port's data with the table emptied: the layer reads its port data from a directory, so the control is the same layer
// and the same binary on a table that names nothing (no relink, no switch in the code).
std::string DataWithAnEmptyTable() {
    const std::filesystem::path copy = Scratch() / "port-data-empty-table";
    std::error_code ec;
    std::filesystem::remove_all(copy, ec);
    std::filesystem::copy(kPortData, copy, std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing, ec);
    CHECK_MSG(!ec, "the port's data is copied: " + ec.message());
    Write(copy / "contact_order.json", "{ \"levels\": {} }");
    return copy.string();
}

void The2_29StoneRollsAsTheOriginalsDoesAndOnlyBecauseOfTheTable() {
    constexpr int kFrames = 560;
    Series keyed;
    Series control;
    if (!Drive("", kFrames, keyed) || !Drive(DataWithAnEmptyTable(), kFrames, control)) return;
    const Readings k = Read(keyed);
    const Readings c = Read(control);
    Print("keyed (the shipped table)", k);
    Print("control (an empty table)", c);

    // The rig clip's readings with their +-10 percent bands: rig.md section 0 and the decision's section 4.
    CHECK_MSG(Within(k.creep, 0.95, 1.16), "the creep is in the clip's band round 1.053: " + std::to_string(k.creep));
    CHECK_MSG(Within(k.floorRoll, 1.82, 2.23), "the free floor roll is in the band round 2.025: " + std::to_string(k.floorRoll));
    CHECK_MSG(Within(k.firstTurn + 1.0, 96.0, 118.0), "the plank first turns inside f96 to f118 (the clip's f107): f" + std::to_string(k.firstTurn + 1.0));
    CHECK_MSG(Within(k.landing + 1.0, 111.0, 135.0), "the stone lands inside f111 to f135 (the clip's f123.0): f" + std::to_string(k.landing + 1.0));
    CHECK_MSG(Within(k.contact + 1.0, 165.0, 202.0), "and meets the barrel inside f165 to f202 (the clip's f183.5): f" + std::to_string(k.contact + 1.0));

    // The control is outside every one of them: the table is what moves the stone.
    CHECK_MSG(c.creep > 0.0 && c.creep < 0.5, "with an empty table the stone creeps at about 0.28, not at the clip's rate: " + std::to_string(c.creep));
    CHECK_MSG(c.contact < 0.0 || c.contact + 1.0 > 300.0, "and meets the barrel late or not at all within " + std::to_string(kFrames) + " frames");
    CHECK_MSG(c.landing < 0.0 || c.landing + 1.0 > 300.0, "and lands late: " + std::to_string(c.landing + 1.0));

    // The player stands where he is put in both: the flag is on the stone and the plank, and nothing of theirs reaches him.
    double worst = 0.0;
    for (std::size_t i = 0; i < keyed.player.size() && i < control.player.size(); ++i) {
        worst = std::max(worst, glm::distance(keyed.player[i], control.player[i]));
    }
    std::printf("  2-29: the idle player is at most %.4f units from the control's over %d frames\n", worst, kFrames);
    CHECK_MSG(worst < 0.05, "the idle player moves as in the control: " + std::to_string(worst));
    // The barrel is the same body in both until the stone reaches it.
    const std::size_t early = std::min<std::size_t>(150, keyed.barrel.size());
    double barrelWorst = 0.0;
    for (std::size_t i = 0; i < early; ++i) barrelWorst = std::max(barrelWorst, glm::distance(keyed.barrel[i], control.barrel[i]));
    CHECK_MSG(barrelWorst < 0.05, "the barrel is where the control's is until the stone is on it: " + std::to_string(barrelWorst));
}

// ---- 8. the wall reading of the flagged stone --------------------------------------------------------------------------------

struct WallRun {
    Game::Data data;
    entt::registry registry;
    Game::Level level;
    ScriptClock::Step step;
    entt::entity stone = entt::null;
    double simRatio = 1.0;
};

bool BeginWall(WallRun& run, bool flagged) {
    std::string error;
    bool ok = Game::LoadData(kLevels + "/" + std::string(kLevel) + ".tscn", kData, Scratch() / "prisms", run.data, error) &&
              Game::Start(run.data, run.registry, run.level, error);
    CHECK_MSG(ok, std::string(kLevel) + ": " + error);
    if (!ok) return false;
    ScriptClock::Rules clock;
    CHECK_MSG(ScriptClock::LoadRules(kPortData + "/clock.json", clock, error), error);
    run.step = ScriptClock::TickStep(clock, kTick);
    run.simRatio = ScriptClock::SimRatio(run.step);
    // The order is the CASE's, whatever the table says: every body unflagged, then the stone as asked.
    for (const auto& entry : run.level.built.entities) {
        if (auto* rigid = run.registry.try_get<RigidBodyComponent>(entry.second)) rigid->integrateAfterSolve = false;
    }
    run.stone = run.level.built.entities.at(kStone);
    run.registry.get<RigidBodyComponent>(run.stone).integrateAfterSolve = flagged;
    return true;
}

void Put(WallRun& run, const glm::dvec2& atPx, const glm::dvec2& originalVelocityPxS) {
    auto& transform = run.registry.get<TransformComponent>(run.stone);
    const glm::vec3 at = Units::ToWorld(atPx.x, atPx.y);
    transform.position = glm::vec3(at.x, at.y, transform.position.z);
    auto& rigid = run.registry.get<RigidBodyComponent>(run.stone);
    // Every body is carried at SimRatio times the original's velocity.
    rigid.velocity = glm::vec3(Units::ToMetres(originalVelocityPxS.x * run.simRatio), Units::ToMetres(-originalVelocityPxS.y * run.simRatio), 0.0f);
    rigid.angularVelocity = glm::vec3(0.0f);
    rigid.isSleeping = false;
    rigid.sleepTimer = 0.0f;
}

// The wall moved into open ground: standing on the floor at x 250, its top at y 98, with the ceiling over a stone dropped from as high as
// a gap of 38. The wall's own box follows (Demolish judges the margin by it).
bool MoveTheWall(WallRun& run) {
    const glm::dvec2 wallCentre(250.0, 161.0);
    Demolish::Breakable* wall = nullptr;
    for (Demolish::Breakable& breakable : run.level.demolish.breakables) {
        if (breakable.name == kWall) wall = &breakable;
    }
    CHECK_MSG(wall != nullptr, "2-29 has its wall");
    if (wall == nullptr) return false;
    run.registry.get<TransformComponent>(wall->body).position = Units::ToWorld(wallCentre.x, wallCentre.y);
    wall->box.centre = glm::vec2(Units::ToWorld(wallCentre.x, wallCentre.y));
    // A few ticks first, so the watch has seen the stone and reads the Put as the port moving it (a first look only marks pairs).
    for (int tick = 0; tick < 3; ++tick) Game::Tick(run.data, run.registry, run.level, 0.0f, run.step);
    return true;
}

struct Begun {
    bool judged = false;
    bool broke = false;
    double speedMs = 0.0;
};

Begun FirstBegin(WallRun& run, int ticks) {
    Begun begun;
    for (int tick = 1; tick <= ticks && !begun.judged; ++tick) {
        Game::Tick(run.data, run.registry, run.level, 0.0f, run.step);
        for (const Demolish::Contact& contact : run.level.demolish.contactsNow) {
            begun.judged = true;
            begun.broke = contact.broke;
            begun.speedMs = contact.speedMs;
        }
    }
    return begun;
}

Begun Drop(double gapPx, bool flagged) {
    WallRun run;
    if (!BeginWall(run, flagged) || !MoveTheWall(run)) return {};
    const double top = 161.0 - 63.0;
    Put(run, glm::dvec2(250.0, top - 30.0 - gapPx), glm::dvec2(0.0));
    return FirstBegin(run, 90);
}

// Box2D's own reading of a free fall from rest onto a surface `gapPx` below (the original's step: v += g h, x += v h, 1/30 s, g = 500 units
// a second squared): k g h on the first step k whose position leaves the stone within the skin.
double Box2dReadingMs(double gapPx, double skinPx) {
    const double gh = 500.0 / 30.0; // units a second gained a step
    double y = 0.0;
    for (int k = 1; k < 400; ++k) {
        y += gh * k / 30.0;
        if (gapPx - y <= skinPx) return gh * k / 50.0; // metres a second of the original's physics: 50 units a metre
    }
    return -1.0;
}

void TheFlaggedStoneBreaksTheWallFromAFallTheLevelCanGive() {
    double gate = 0.0;
    double skin = 0.0;
    {
        WallRun probe;
        if (!BeginWall(probe, false)) return;
        gate = probe.data.demolish.wallBreaksAboveMs;
        skin = Earthquake::ToUnits(probe.data.earthquake, probe.data.earthquake.touchPx);
        std::printf("  the wall: gate %.3f m/s of the original's physics, SimRatio %.3f, skin %.4f units\n", gate, probe.simRatio, skin);
    }
    for (const double gap : {3.0, 5.0, 6.0, 7.0, 8.0, 10.0, 12.0, 15.0, 20.0, 25.0, 30.0, 38.0}) {
        const double box2d = Box2dReadingMs(gap, skin);
        const Begun plain = Drop(gap, false);
        const Begun flagged = Drop(gap, true);
        std::printf("  drop from a gap of %4.1f u | Box2D %.3f m/s %-6s | unflagged %.3f %-6s | flagged %.3f %s\n", gap, box2d,
                    box2d > gate ? "BREAKS" : "holds", plain.speedMs, plain.judged ? (plain.broke ? "BREAKS" : "holds") : "none",
                    flagged.speedMs, flagged.judged ? (flagged.broke ? "BREAKS" : "holds") : "none");
        // From 12 units up Box2D reads 2.33 m/s and over against 1.5: the flagged stone is read at most one gravity step (0.33 m/s) under it,
        // which is margin to spare. Near the gate (about 6 to 8 units) it is read a step under and the wall holds where Box2D's would not:
        // printed above, the departure Part 0 recorded.
        if (gap >= 12.0) {
            CHECK_MSG(flagged.judged && flagged.broke, "a flagged stone dropped from a gap of " + std::to_string(gap) + " units breaks the wall");
        }
    }
    // The portal's exit: the stone put overlapping the wall's face at a speed, as a portal's exit gives it, in open ground (the door of the
    // level is out of it). PRINTED: the reading of a body the port moved is the velocity it carries into its first step.
    for (const double speed : {20.0, 40.0, 60.0, 70.0, 80.0, 100.0, 144.0, 200.0}) {
        Begun both[2];
        for (int flagged = 0; flagged < 2; ++flagged) {
            WallRun run;
            if (!BeginWall(run, flagged != 0) || !MoveTheWall(run)) continue;
            // Overlapping the wall's left face (x 235) by 2 units, on the floor line, going into it.
            Put(run, glm::dvec2(235.0 - 30.0 + 2.0, 194.0), glm::dvec2(speed, 0.0));
            both[flagged] = FirstBegin(run, 40);
        }
        std::printf("  exit at %5.1f u/s | unflagged %.3f m/s %-6s | flagged %.3f m/s %s\n", speed, both[0].speedMs,
                    both[0].judged ? (both[0].broke ? "BREAKS" : "holds") : "none", both[1].speedMs,
                    both[1].judged ? (both[1].broke ? "BREAKS" : "holds") : "none");
    }
}

void runTests() {
    TheShippedTableNamesOneLevelAndTwoBodies();
    TheLoaderRefusesWhatWouldBeSilent();
    ApplyNamesWhatItCannotFlag();
    The2_29FlagLandsOnTheStoneAndThePlankAlone();
    NoBodyOfTheOther127LevelsIsFlagged();
    AnEmptyTableOrACopyUnderAnotherNameFlagsNothing();
    The2_29StoneRollsAsTheOriginalsDoesAndOnlyBecauseOfTheTable();
    TheFlaggedStoneBreaksTheWallFromAFallTheLevelCanGive();
}

} // namespace

MP_SUITE_MAIN("test_mp_contact_order") {
    std::error_code ec;
    if (!std::filesystem::is_regular_file(kLevels + "/" + kLevel + ".tscn", ec) ||
        !std::filesystem::is_regular_file(kData + "/entity_roles.json", ec)) {
        std::printf("test_mp_contact_order SKIPPED - needs the converted levels at %s and Enhanced's data at %s\n", kLevels.c_str(),
                    kData.c_str());
        return 77;
    }
    runTests();
    return ::test::summary("test_mp_contact_order", 90);
}
