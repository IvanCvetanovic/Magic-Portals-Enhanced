// What the run asks of the engine's optional performance features (planning step 190): game/data/engine.json as sim/EngineTuning reads it,
// the kind of run it is read for, and the flags that override it for one run.
//
// WHAT IS PINNED, and what is not. The rule is pinned in full: a table that will not read leaves the engine as it was; the table is read
// strictly (a missing key, a wrong type or a shadow-map size outside the engine's own range is refused, naming the key, and changes
// nothing); a person's run and every other run can differ, field by field; a flag wins over both; a flag's value is parsed exactly. The
// SHIPPED table is read and checked to be inside the engine's ranges, and to keep what parity work depends on for every run that is not a
// person's (hot reload of a file a developer saved, the sprites' on-screen log); its measured values (the shadow maps' size, the sprites-only
// pipelines, what a person's run turns off) are NOT pinned here, for they are a decision the Step records with its numbers and a
// designer retunes in the file: pinning them would turn declining one into a test edit and a relink.
//
// NOT TESTED, and why: main.cpp cannot be linked into a suite, so the wiring from a flag to this module's Overrides, and from Resolved to
// the engine's GameManifest, rests on probes of the built exe (the engine's own log says "Created 4 x 16x16 shadow map layer(s)", and
// "Watching 0 asset path(s)", of what it was given).
//
// Free of the engine and of the renderer, like the rest of sim/: a scratch directory and no window.

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "sim/EngineTuning.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <system_error>

using namespace MagicPortals;

namespace {

std::filesystem::path Scratch(const char* name) {
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "supersonic-test-mp-tuning" / name;
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);
    return dir;
}

std::string Put(const std::filesystem::path& dir, const char* name, const std::string& text) {
    const std::filesystem::path file = dir / name;
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    out << text;
    return file.string();
}

// A table text with one thing changed at a time: the good one is the shape the shipped file has.
std::string TableText(const std::string& size = "16", const std::string& sprites = "true",
                      const std::string& watching = "{\"attended_run\": false, \"other_runs\": true}",
                      const std::string& report = "{\"attended_run\": false, \"other_runs\": true}") {
    return "{\"_about\": [\"x\"], \"shadow_map_resolution\": " + size + ", \"sprites_only_scene_pipelines\": " + sprites +
           ", \"asset_watching\": " + watching + ", \"sprite_report\": " + report + "}";
}

void TheEnginesOwnDefaultsAreWhatNothingAskedFor() {
    // Table{} is the engine as it was: no opt-in, hot reload on, the sprite report on. A table that will not read leaves exactly this.
    const EngineTuning::Resolved plain = EngineTuning::Resolve(EngineTuning::Table{}, true, EngineTuning::Overrides{});
    CHECK_EQ(plain.shadowMapResolution, 0u);
    CHECK_MSG(!plain.spritesOnlyScenePipelines, "no sprites-only pipelines unless asked");
    CHECK_MSG(plain.assetWatching && plain.spriteReport, "hot reload and the sprite report stay on unless asked");
    CHECK_MSG(plain.perfLogSeconds == 0.0f, "no perf log unless asked");
    const EngineTuning::Resolved other = EngineTuning::Resolve(EngineTuning::Table{}, false, EngineTuning::Overrides{});
    CHECK_MSG(other.shadowMapResolution == 0 && !other.spritesOnlyScenePipelines && other.assetWatching && other.spriteReport,
              "and the same for a run that is not a person's");
}

void TheShippedTableReadsAndKeepsWhatParityWorkNeeds() {
    EngineTuning::Table table;
    std::string error;
    const std::string path = std::string(MAGICPORTALS_PORT_DATA_DIR) + "/engine.json";
    CHECK_MSG(EngineTuning::Load(path, table, error), "game/data/engine.json reads: " + error);
    CHECK_MSG(table.shadowMapResolution == 0 ||
                  (table.shadowMapResolution >= EngineTuning::kShadowMapMin && table.shadowMapResolution <= EngineTuning::kShadowMapMax),
              "its shadow-map size is the engine's own sizes or inside the engine's range: " + std::to_string(table.shadowMapResolution));
    // The rule of the Step, not a measurement: a capture, a route, a level walk and any script keep hot reload and the sprites' log.
    CHECK_MSG(table.assetWatching.other, "a run that is not a person's keeps the engine's hot reload of a saved file");
    CHECK_MSG(table.spriteReport.other, "and keeps the sprites' on-screen log");
    // A fixed-step capture is never a person's run, so what the Step turns off for a person cannot change the pictures a gate compares.
    const EngineTuning::Resolved capture = EngineTuning::Resolve(table, false, EngineTuning::Overrides{});
    CHECK_MSG(capture.assetWatching && capture.spriteReport, "so a capture resolves both on");
}

void ATableThatWillNotReadIsRefusedWhollyAndSaysWhichKey() {
    const std::filesystem::path dir = Scratch("refused");
    const struct {
        const char* what;
        std::string text;
        const char* names;
    } bad[] = {
        {"not JSON at all", "{ this is not json", ".json"},
        {"an array", "[1, 2, 3]", "shadow_map_resolution"},
        {"no resolution", "{\"sprites_only_scene_pipelines\": true}", "shadow_map_resolution"},
        {"a text resolution", TableText("\"16\""), "shadow_map_resolution"},
        {"a resolution under the engine's floor", TableText("15"), "shadow_map_resolution"},
        {"a resolution over the engine's ceiling", TableText("4097"), "shadow_map_resolution"},
        {"a fractional resolution", TableText("16.5"), "shadow_map_resolution"},
        {"a negative resolution", TableText("-16"), "shadow_map_resolution"},
        {"sprites-only as a number", TableText("16", "1"), "sprites_only_scene_pipelines"},
        {"watching as a bare bool", TableText("16", "true", "false"), "asset_watching"},
        {"watching without the other runs", TableText("16", "true", "{\"attended_run\": false}"), "asset_watching"},
        {"the report with a text field",
         TableText("16", "true", "{\"attended_run\": false, \"other_runs\": true}", "{\"attended_run\": \"off\", \"other_runs\": true}"),
         "sprite_report"},
        {"no report", "{\"shadow_map_resolution\": 16, \"sprites_only_scene_pipelines\": true, \"asset_watching\": "
                      "{\"attended_run\": false, \"other_runs\": true}}",
         "sprite_report"},
    };
    int n = 0;
    for (const auto& row : bad) {
        EngineTuning::Table table;
        table.shadowMapResolution = 1234; // a marker: a refused read must not touch the table it was handed
        std::string error;
        const std::string file = Put(dir, ("bad" + std::to_string(n++) + ".json").c_str(), row.text);
        CHECK_MSG(!EngineTuning::Load(file, table, error), std::string(row.what) + ": refused");
        CHECK_MSG(error.find(row.names) != std::string::npos, std::string(row.what) + ": the error names " + row.names + ": " + error);
        CHECK_MSG(table.shadowMapResolution == 1234, std::string(row.what) + ": and the table is untouched");
    }
    EngineTuning::Table table;
    std::string error;
    CHECK_MSG(!EngineTuning::Load((dir / "no_such_file.json").string(), table, error) && error.find("cannot open") != std::string::npos,
              "a file that is not there is refused: " + error);
    // And the good shape, so the refusals above are not the reader refusing everything.
    CHECK_MSG(EngineTuning::Load(Put(dir, "good.json", TableText()), table, error), "the good shape reads: " + error);
    CHECK_MSG(EngineTuning::Load(Put(dir, "zero.json", TableText("0")), table, error) && table.shadowMapResolution == 0,
              "0 is the engine's own sizes: " + error);
    CHECK_MSG(EngineTuning::Load(Put(dir, "top.json", TableText("4096")), table, error) && table.shadowMapResolution == 4096,
              "and the ceiling itself is allowed: " + error);
}

void ThePersonsRunAndEveryOtherRunCanDiffer() {
    const std::filesystem::path dir = Scratch("kinds");
    EngineTuning::Table table;
    std::string error;
    CHECK_MSG(EngineTuning::Load(Put(dir, "t.json", TableText("16", "true")), table, error), error);
    CHECK_EQ(table.shadowMapResolution, 16u);
    CHECK(table.spritesOnlyScenePipelines);
    const EngineTuning::Resolved person = EngineTuning::Resolve(table, true, EngineTuning::Overrides{});
    const EngineTuning::Resolved other = EngineTuning::Resolve(table, false, EngineTuning::Overrides{});
    CHECK_MSG(!person.assetWatching && !person.spriteReport, "a person's run: no stat of every watched file, no sprite walk");
    CHECK_MSG(other.assetWatching && other.spriteReport, "any other run: hot reload and the log, as parity work has them");
    CHECK_MSG(person.shadowMapResolution == 16 && other.shadowMapResolution == 16, "the shadow maps are the same for both");
    CHECK_MSG(person.spritesOnlyScenePipelines && other.spritesOnlyScenePipelines, "and so are the pipelines");

    // The other way round, to show the fields are read from the right sides and not swapped.
    CHECK_MSG(EngineTuning::Load(Put(dir, "u.json", TableText("0", "false", "{\"attended_run\": true, \"other_runs\": false}",
                                                               "{\"attended_run\": true, \"other_runs\": false}")),
                                 table, error),
              error);
    const EngineTuning::Resolved person2 = EngineTuning::Resolve(table, true, EngineTuning::Overrides{});
    const EngineTuning::Resolved other2 = EngineTuning::Resolve(table, false, EngineTuning::Overrides{});
    CHECK_MSG(person2.assetWatching && person2.spriteReport && !other2.assetWatching && !other2.spriteReport,
              "attended_run is a person's run and other_runs the rest");
    CHECK_MSG(person2.shadowMapResolution == 0 && !person2.spritesOnlyScenePipelines, "0 and false are the engine as it was");
}

void AFlagWinsOverTheTableForOneRun() {
    EngineTuning::Table table; // a person's run, everything off in the table: the flags turn each on
    table.assetWatching.attended = false;
    table.spriteReport.attended = false;
    EngineTuning::Overrides flags;
    flags.assetWatching = true;
    flags.spriteReport = true;
    flags.spritesOnlyScenePipelines = true;
    flags.shadowMapResolution = 64;
    flags.perfLogSeconds = 2.5f;
    const EngineTuning::Resolved on = EngineTuning::Resolve(table, true, flags);
    CHECK_MSG(on.assetWatching && on.spriteReport && on.spritesOnlyScenePipelines && on.shadowMapResolution == 64,
              "each flag turned its setting on over the table");
    CHECK_MSG(on.perfLogSeconds == 2.5f, "the perf log seconds pass through");

    table.assetWatching = {true, true};
    table.spriteReport = {true, true};
    table.spritesOnlyScenePipelines = true;
    table.shadowMapResolution = 16;
    EngineTuning::Overrides off;
    off.assetWatching = false;
    off.spriteReport = false;
    off.spritesOnlyScenePipelines = false;
    off.shadowMapResolution = 0; // 0 is a value too: the engine's own sizes, asked for
    const EngineTuning::Resolved down = EngineTuning::Resolve(table, false, off);
    CHECK_MSG(!down.assetWatching && !down.spriteReport && !down.spritesOnlyScenePipelines && down.shadowMapResolution == 0,
              "and each turned its setting off, 0 included, over the table, for a run that is not a person's too");
    // An absent flag leaves the table's answer alone.
    EngineTuning::Overrides some;
    some.spriteReport = false;
    const EngineTuning::Resolved partial = EngineTuning::Resolve(table, true, some);
    CHECK_MSG(partial.assetWatching && !partial.spriteReport && partial.shadowMapResolution == 16 && partial.spritesOnlyScenePipelines,
              "a flag changes its own setting and no other");
}

void TheFlagsValuesAreParsedExactly() {
    bool b = false;
    CHECK(EngineTuning::ParseSwitch("on", b) && b);
    CHECK(EngineTuning::ParseSwitch("off", b) && !b);
    for (const char* text : {"", "ON", "Off", "1", "0", "true", "yes", "on ", " on", "onn"}) {
        b = true;
        CHECK_MSG(!EngineTuning::ParseSwitch(text, b) && b, std::string("a switch is exactly on or off, not '") + text + "'");
    }
    std::uint32_t n = 7;
    CHECK(EngineTuning::ParseShadowMaps("0", n) && n == 0);
    CHECK(EngineTuning::ParseShadowMaps("16", n) && n == 16);
    CHECK(EngineTuning::ParseShadowMaps("2048", n) && n == 2048);
    CHECK(EngineTuning::ParseShadowMaps("4096", n) && n == 4096);
    for (const char* text : {"", "15", "1", "4097", "99999999", "-16", "+16", "16.5", "0x10", "abc", "16px", " 16", "1e3"}) {
        n = 7;
        CHECK_MSG(!EngineTuning::ParseShadowMaps(text, n) && n == 7, std::string("a size is 0 or a whole number from 16 to 4096, not '") + text + "'");
    }
    float s = 0.0f;
    CHECK(EngineTuning::ParsePerfLog("5", s) && s == 5.0f);
    CHECK(EngineTuning::ParsePerfLog("0.5", s) && s == 0.5f);
    CHECK(EngineTuning::ParsePerfLog("3600", s) && s == 3600.0f);
    for (const char* text : {"", "0", "-1", "3601", "nan", "inf", "5s", "abc", " 5", "1e9"}) {
        s = -1.0f;
        CHECK_MSG(!EngineTuning::ParsePerfLog(text, s) && s == -1.0f, std::string("seconds are above 0 and at most 3600, not '") + text + "'");
    }
}

void TheLogLineSaysWhatTheRunAskedFor() {
    EngineTuning::Resolved r;
    r.shadowMapResolution = 16;
    r.spritesOnlyScenePipelines = true;
    r.assetWatching = false;
    r.spriteReport = false;
    const std::string line = EngineTuning::Describe(r);
    CHECK_MSG(line.find("shadow maps 16 px") != std::string::npos && line.find("sprites-only pipelines on") != std::string::npos &&
                  line.find("asset watching off") != std::string::npos && line.find("sprite report off") != std::string::npos &&
                  line.find("perf log off") != std::string::npos,
              "each setting is named with its value: " + line);
    r.shadowMapResolution = 0;
    r.perfLogSeconds = 5.0f;
    const std::string plain = EngineTuning::Describe(r);
    CHECK_MSG(plain.find("as the engine sizes them") != std::string::npos && plain.find("every 5 s") != std::string::npos,
              "0 is said as the engine's own sizes, and a perf log with its period: " + plain);
}

void runTests() {
    TheEnginesOwnDefaultsAreWhatNothingAskedFor();
    TheShippedTableReadsAndKeepsWhatParityWorkNeeds();
    ATableThatWillNotReadIsRefusedWhollyAndSaysWhichKey();
    ThePersonsRunAndEveryOtherRunCanDiffer();
    AFlagWinsOverTheTableForOneRun();
    TheFlagsValuesAreParsedExactly();
    TheLogLineSaysWhatTheRunAskedFor();
}

} // namespace

MP_TEST_MAIN("test_mp_tuning", 100)
