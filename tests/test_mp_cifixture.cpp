// The CI fixture level (planning step 206, PORT-8): tests/fixtures/ci, the one level a data-less runner can start.
//
// WHY IT EXISTS. A continuous-integration runner has none of the game's data (the original's levels, art and sound are the owner's, gitignored, and never in
// this repository: CLAUDE.md, the hard rule), so a run there could prove that the program builds and links and nothing more. The fixture is a level written for
// this purpose and from nothing else: a room of static boxes, a pressure plate, a gate that the plate opens, a start and an exit, with no picture of any kind,
// no number taken from a level of the original and no name from it, in the converter's own file format and the packaged folder shape --root expects:
//
//   MagicPortals --root <repo>/tests/fixtures/ci --port-data <repo>/game/data --level ci_level --window 1280x720 --fixed-step --frames 300 --screenshot <abs>.png
//
// exits 0 and writes a picture. (The port's own 45 tables are the repository's game/data, named by --port-data and NOT copied into the fixture: the game refuses to start a level
// without them, and a copy would put their notes, which name other projects, into the repository a second time.) It can only say "the shell starts, loads a level, ticks and draws"; it cannot say the game plays (docs/apple.md).
//
// WHAT THIS SUITE PINS, with no window and no converted level (it NEVER skips):
//   - the folder holds hand-written .tscn and .json files and nothing else, of an extension the hard rule's check would not catch (no picture, sound or
//     archive), and no entity of it is named like one of the original's;
//   - the two tables of Enhanced's own inside it (data/player.json and data/portals.json) are byte for byte the repository's: --root makes `data` the fixture's, the game reads three tables
//     there, and the third (the role table) is the fixture's own; a copy that drifted is a failure that names tools/ci/sync_fixture_tables.py;
//   - the level reads through the game's own readers (Tscn, Chapters, Game::LoadData with the fixture's role table), with its counts, starts, and its gate
//     opens in a few ticks of the simulation when something presses its plate, and not before.
//
// THE MUTATION EACH PIN FAILS UNDER is written beside it ("MUTATION:"): each was run on a scratch copy of the fixture (the step's record lists the runs).

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "core/Components.hpp"
#include "sim/Chapters.hpp"
#include "sim/Game.hpp"
#include "sim/Mover.hpp"
#include "sim/Puzzle.hpp"
#include "sim/Roles.hpp"
#include "sim/Tscn.hpp"
#include "sim/Units.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#ifndef MAGICPORTALS_SOURCE_DIR
#error "test_mp_cifixture.cpp is given MAGICPORTALS_SOURCE_DIR by tests/CMakeLists.txt"
#endif

using namespace MagicPortals;
namespace fs = std::filesystem;

namespace {

const fs::path kRepo = MAGICPORTALS_SOURCE_DIR;
const fs::path kFixture = kRepo / "tests" / "fixtures" / "ci";
constexpr float kStep = 1.0f / 60.0f;
constexpr const char* kSyncCommand = "python tools/ci/sync_fixture_tables.py";

std::string Slurp(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

// The same table whatever the working tree's line endings (core.autocrlf): CRLF folded to LF before comparing.
std::string LfOnly(std::string text) {
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\r' && i + 1 < text.size() && text[i + 1] == '\n') continue;
        out += text[i];
    }
    return out;
}

std::vector<std::string> Names(const fs::path& folder, const char* extension) {
    std::vector<std::string> names;
    std::error_code ec;
    for (fs::directory_iterator it(folder, ec), end; !ec && it != end; it.increment(ec)) {
        if (it->is_regular_file(ec) && it->path().extension() == extension) names.push_back(it->path().filename().string());
    }
    std::sort(names.begin(), names.end());
    return names;
}

std::string Joined(const std::vector<std::string>& v) {
    std::string s;
    for (const std::string& item : v) s += (s.empty() ? "" : ", ") + item;
    return s.empty() ? "(none)" : s;
}

// ---- the folder -----------------------------------------------------------------------------------------------------------------------------

// MUTATION: a .png, .mp3, .ent, .esc, .ogg or .zip file dropped into the fixture (each of the extension list); a .py or a .md file dropped in (the folder holds
// two kinds of file and no third); the tables' count changed by one file.
void TheFolderHoldsHandWrittenTextAndNothingElse() {
    std::set<std::string> kinds;
    std::size_t files = 0;
    std::vector<std::string> odd;
    std::error_code ec;
    for (fs::recursive_directory_iterator it(kFixture, ec), end; !ec && it != end; it.increment(ec)) {
        if (!it->is_regular_file(ec)) continue;
        ++files;
        const std::string extension = it->path().extension().string();
        kinds.insert(extension);
        if (extension != ".tscn" && extension != ".json") odd.push_back(fs::relative(it->path(), kFixture).generic_string());
    }
    CHECK_MSG(files > 0, "the fixture folder exists: " + kFixture.string());
    CHECK_MSG(odd.empty(), "only .tscn and .json belong in the fixture, found " + Joined(odd));
    CHECK_MSG(kinds == std::set<std::string>({".json", ".tscn"}), "both kinds are there");
    // The packaged shape's folders, and the only .tscn.
    CHECK_MSG(Names(kFixture / "levels", ".tscn") == std::vector<std::string>{"ci_level.tscn"}, "one level: " + Joined(Names(kFixture / "levels", ".tscn")));
    CHECK_MSG(Names(kFixture / "tables", ".json") == std::vector<std::string>{"chapters.json"}, "one table beside it: " + Joined(Names(kFixture / "tables", ".json")));
    CHECK_MSG(Names(kFixture / "data", ".json") == std::vector<std::string>({"entity_roles.json", "player.json", "portals.json"}),
              "three tables of Enhanced's: " + Joined(Names(kFixture / "data", ".json")));
    // Counted: the level, the chapters table and Enhanced's three; and no folder of the port's own tables.
    CHECK_MSG(files == 5, std::to_string(files) + " files, expected 5");
    CHECK_MSG(Names(kFixture / "port-data", ".json").empty(), "the port's own tables are named by --port-data, not copied into the fixture");
}

// MUTATION: data/player.json or data/portals.json edited in either place; one of them deleted from the fixture; a surplus table put beside them. The failure names the command that fixes it.
void EnhancedsTwoTablesAreCopiedByteForByte() {
    std::vector<std::string> drifted;
    for (const char* name : {"player.json", "portals.json"}) {
        if (LfOnly(Slurp(kRepo / "data" / name)) != LfOnly(Slurp(kFixture / "data" / name))) drifted.push_back(std::string("data/") + name);
    }
    CHECK_MSG(drifted.empty(), "the fixture's copy differs from the repository's: " + Joined(drifted) + "; run " + kSyncCommand);
}

// MUTATION: an ext_resource (a picture) added to the level; a node given an entity name that data/entity_roles.json lists; an entity name ending in ".ent";
// a name copied from level30.
void NothingInTheLevelIsTheOriginalsNameOrArt() {
    Tscn::Scene scene;
    std::string error;
    CHECK_MSG(Tscn::Load((kFixture / "levels" / "ci_level.tscn").string(), scene, error), error);
    CHECK_MSG(scene.external.empty(), "the level places no picture: " + std::to_string(scene.external.size()) + " external resource(s)");

    Roles::Table repoRoles;
    CHECK_MSG(Roles::Load((kRepo / "data" / "entity_roles.json").string(), repoRoles, error), error);
    Roles::Table fixtureRoles;
    CHECK_MSG(Roles::Load((kFixture / "data" / "entity_roles.json").string(), fixtureRoles, error), error);
    std::set<std::string> used;
    for (const Tscn::Node& node : scene.nodes) {
        const Tscn::Value* name = node.Meta("entity_name");
        if (name != nullptr) used.insert(name->text);
    }
    CHECK_MSG(used.size() == 6, "the level names six entities: " + std::to_string(used.size()));
    for (const std::string& name : used) {
        CHECK_MSG(repoRoles.roleOf.count(name) == 0, "'" + name + "' is a name of data/entity_roles.json (the original's)");
        CHECK_MSG(name.size() < 4 || name.compare(name.size() - 4, 4, ".ent") != 0, "'" + name + "' ends in .ent, a file of the original's format");
        CHECK_MSG(fixtureRoles.roleOf.count(name) == 1, "'" + name + "' has a role in the fixture's own table");
    }
    // And the node and level names are the fixture's: the file's own text holds no number's worth of the original's level names.
    const std::string text = Slurp(kFixture / "levels" / "ci_level.tscn");
    CHECK_MSG(text.find("level30") == std::string::npos && text.find("res://") == std::string::npos, "no original level name, no resource path");
}

// ---- the level ---------------------------------------------------------------------------------------------------------------------------------

// MUTATION: chapters.json given a second level or another name; the level's file renamed.
void TheChaptersTableNamesTheLevel() {
    Chapters::Table table;
    std::string error;
    CHECK_MSG(Chapters::Load((kFixture / "tables" / "chapters.json").string(), table, error), error);
    CHECK_MSG(table.levels.size() == 1 && table.levels[0].name == "ci_level", "one level, ci_level");
    CHECK_MSG(table.Find("ci_level") == 0 && table.Find("level0") == -1, "found by name, and no other");
}

struct Run {
    Game::Data data;
    entt::registry registry;
    Game::Level level;
};

bool Open(Run& run, std::string& error) {
    const fs::path prisms = fs::temp_directory_path() / "supersonic-test-mp-cifixture";
    return Game::LoadData((kFixture / "levels" / "ci_level.tscn").string(), (kFixture / "data").string(), prisms, run.data, error,
                          (kRepo / "game" / "data").string()) &&
           Game::Start(run.data, run.registry, run.level, error);
}

// MUTATION: a static box added or removed in the level (the counts change); a role missing from the fixture's role table (the exit, the plate or the gate: the
// start then fails or the counts do); the gate or the plate given another switch index (the link between them is the pin below).
void TheLevelReadsAndStartsThroughTheGamesOwnReaders() {
    Run run;
    std::string error;
    const bool ok = Open(run, error);
    CHECK_MSG(ok, error);
    if (!ok) return;
    // Counted off the file: four static boxes of the room (floor, two walls, one ledge); the gate is a mover, built with a body of its own.
    CHECK_MSG(run.data.scene.nodes.size() > 20, "the scene was read: " + std::to_string(run.data.scene.nodes.size()) + " nodes");
    CHECK_MSG(run.level.built.statics == 4, "four static bodies: floor, two walls and a ledge: " + std::to_string(run.level.built.statics));
    CHECK_MSG(run.level.built.movers == 1, "one mover, the gate: " + std::to_string(run.level.built.movers));
    CHECK_MSG(run.level.built.areas == 1, "one area, the plate: " + std::to_string(run.level.built.areas));
    CHECK_MSG(run.level.channels.buttons.size() == 1 && run.level.channels.doors.size() == 1,
              "one plate and one gate: " + std::to_string(run.level.channels.buttons.size()) + " and " + std::to_string(run.level.channels.doors.size()));
    CHECK_MSG(run.level.goals.crystals.empty(), "no crystal: the exit is open from the start");
    CHECK_MSG(run.level.goals.ExitOpen(), "and the exit is open");
    CHECK_MSG(run.level.player != entt::null && run.registry.valid(run.level.player), "the player stands at the start");
    CHECK_MSG(!run.level.noGravity && !run.level.darkest, "an ordinary level");
    if (run.level.channels.doors.size() == 1 && run.level.channels.buttons.size() == 1) {
        CHECK_MSG(run.level.channels.doors[0].channel == run.level.channels.buttons[0].channel, "the gate listens to the plate's channel");
    }
}

// MUTATION: the plate's idx or the gate's switchIdx changed (the gate then never answers); the gate's stride made 0; the plate's body not a dynamic-body
// trigger; Channels::Tick not reading the plate. The control half (nothing presses it, nothing moves) fails under a gate that starts open or a plate that is
// always pressed.
void TheGateOpensInAFewTicksWhenTheStartPressesThePlateAndNotBefore() {
    Run run;
    std::string error;
    const bool ok = Open(run, error);
    CHECK_MSG(ok, error);
    if (!ok || run.level.channels.doors.size() != 1) return;
    const Puzzle::SwitchedDoor& gate = run.level.channels.doors[0];
    const ScriptClock::Step step(kStep);
    const double shutY = gate.motion.atPx.y;

    // Nothing stands on the plate: sixty ticks, a second, and the gate has not moved.
    for (int tick = 0; tick < 60; ++tick) Game::Tick(run.data, run.registry, run.level, 0.0f, step);
    CHECK_MSG(gate.motion.status == Mover::DoorStatus::Shut && gate.motion.atPx.y == shutY, "an unpressed gate stays shut");
    CHECK_MSG(!run.level.channels.buttons[0].pressed, "and the plate is up");

    // The player put on the plate (nothing steps the physics here, so a body stays where it is put): the plate is pressed on the first tick and the gate
    // rises from the next.
    const glm::dvec2 platePx(240.0, 218.0);
    run.registry.get<Supersonic::TransformComponent>(run.level.player).position = Units::ToWorld(platePx.x, platePx.y);
    int pressedOn = -1;
    int movingOn = -1;
    for (int tick = 1; tick <= 12; ++tick) {
        Game::Tick(run.data, run.registry, run.level, 0.0f, step);
        if (pressedOn < 0 && run.level.channels.buttons[0].pressed) pressedOn = tick;
        if (movingOn < 0 && gate.motion.status == Mover::DoorStatus::Opening) movingOn = tick;
    }
    CHECK_MSG(pressedOn >= 1 && pressedOn <= 2, "the plate is pressed within two ticks of the player's arrival: " + std::to_string(pressedOn));
    CHECK_MSG(movingOn >= 1 && movingOn <= 3, "the gate starts to rise within three ticks of it: " + std::to_string(movingOn));
    CHECK_MSG(gate.motion.atPx.y < shutY - 0.5, "and has risen: y " + std::to_string(gate.motion.atPx.y) + " against " + std::to_string(shutY) + " shut");
    std::printf("  the gate: pressed on tick %d, rising on tick %d, y %.2f from %.2f\n", pressedOn, movingOn, gate.motion.atPx.y, shutY);
}

void runTests() {
    TheFolderHoldsHandWrittenTextAndNothingElse();
    EnhancedsTwoTablesAreCopiedByteForByte();
    NothingInTheLevelIsTheOriginalsNameOrArt();
    TheChaptersTableNamesTheLevel();
    TheLevelReadsAndStartsThroughTheGamesOwnReaders();
    TheGateOpensInAFewTicksWhenTheStartPressesThePlateAndNotBefore();
}

} // namespace

MP_TEST_MAIN("test_mp_cifixture", 30)
