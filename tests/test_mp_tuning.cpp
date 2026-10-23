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
// THE PHONE PROFILE (planning step 201, PORT-3) is pinned at both ends. FOR A DESKTOP RUN it changes nothing: Resolve with mobile false is compared, over a grid
// of tables, kinds of run and flags, with a COPY of the rule as it was before the profile existed (OldResolve below), and a literal table of rows
// says the same without the copy. FOR A PHONE RUN the "mobile" block replaces what it holds and nothing it does not, the flags still win, and the block is
// read as strictly as the rest of the file: a key it does not know is refused by name. The SHIPPED block is read, and the one fact of it that is a rule and
// not a tuning decision is pinned: it asks for Vulkan 1.1, because a phone whose stock driver reports 1.1 has no GPU the engine accepts otherwise. Each
// pin below says, in "mutation:", the change to the code that turns it red; the mutations were run (the Step records them).
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

// The phone block of the good shape: all three keys, and a note, which is not read.
const char* const kGoodMobile = "{\"_about\": [\"x\"], \"minimum_vulkan_minor\": 1, \"shadow_map_resolution\": 16, \"asset_watching\": false}";

// A table text with one thing changed at a time: the good one is the shape the shipped file has.
std::string TableText(const std::string& size = "16", const std::string& sprites = "true",
                      const std::string& watching = "{\"attended_run\": false, \"other_runs\": true}",
                      const std::string& report = "{\"attended_run\": false, \"other_runs\": true}",
                      const std::string& mobile = kGoodMobile) {
    return "{\"_about\": [\"x\"], \"shadow_map_resolution\": " + size + ", \"sprites_only_scene_pipelines\": " + sprites +
           ", \"asset_watching\": " + watching + ", \"sprite_report\": " + report + ", \"mobile\": " + mobile + "}";
}

// The rule as it was before the phone profile existed (planning step 190, game/sim/EngineTuning.cpp at 45d43ec), COPIED here and not shared: an oracle
// that is the code it checks cannot fail. Its output has no minimum Vulkan, which the old rule did not have.
EngineTuning::Resolved OldResolve(const EngineTuning::Table& table, bool attended, const EngineTuning::Overrides& overrides) {
    EngineTuning::Resolved out;
    out.shadowMapResolution = overrides.shadowMapResolution.value_or(table.shadowMapResolution);
    out.spritesOnlyScenePipelines = overrides.spritesOnlyScenePipelines.value_or(table.spritesOnlyScenePipelines);
    out.assetWatching = overrides.assetWatching.value_or(attended ? table.assetWatching.attended : table.assetWatching.other);
    out.spriteReport = overrides.spriteReport.value_or(attended ? table.spriteReport.attended : table.spriteReport.other);
    out.perfLogSeconds = overrides.perfLogSeconds;
    return out;
}

bool SameAnswer(const EngineTuning::Resolved& a, const EngineTuning::Resolved& b) {
    return a.shadowMapResolution == b.shadowMapResolution && a.spritesOnlyScenePipelines == b.spritesOnlyScenePipelines &&
           a.assetWatching == b.assetWatching && a.spriteReport == b.spriteReport && a.perfLogSeconds == b.perfLogSeconds &&
           a.minimumVulkanMinor == b.minimumVulkanMinor;
}

void TheEnginesOwnDefaultsAreWhatNothingAskedFor() {
    // Table{} is the engine as it was: no opt-in, hot reload on, the sprite report on. A table that will not read leaves exactly this.
    const EngineTuning::Resolved plain = EngineTuning::Resolve(EngineTuning::Table{}, true, false, EngineTuning::Overrides{});
    CHECK_EQ(plain.shadowMapResolution, 0u);
    CHECK_MSG(!plain.spritesOnlyScenePipelines, "no sprites-only pipelines unless asked");
    CHECK_MSG(plain.assetWatching && plain.spriteReport, "hot reload and the sprite report stay on unless asked");
    CHECK_MSG(plain.perfLogSeconds == 0.0f, "no perf log unless asked");
    const EngineTuning::Resolved other = EngineTuning::Resolve(EngineTuning::Table{}, false, false, EngineTuning::Overrides{});
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
    const EngineTuning::Resolved capture = EngineTuning::Resolve(table, false, false, EngineTuning::Overrides{});
    CHECK_MSG(capture.assetWatching && capture.spriteReport, "so a capture resolves both on");
    // THE SHIPPED FILE, resolved for the desktop, is what the rule as it was gives for it: every row of the file (the shadow maps, the sprites-only pipelines,
    // the watching, the sprite report), for a person's run and for any other, with no value written here that a decision could change.
    // mutation: lay the phone block over a desktop answer (R1 of the Step's list); swap which kind of run a PerRun field is read for.
    for (const bool attended : {true, false}) {
        CHECK_MSG(SameAnswer(EngineTuning::Resolve(table, attended, false, EngineTuning::Overrides{}), OldResolve(table, attended, EngineTuning::Overrides{})),
                  std::string("the shipped file resolves, for the desktop, as the rule as it was resolved it: ") + (attended ? "a person's run" : "any other run"));
    }

    // THE PHONE BLOCK OF THE SHIPPED FILE. The one fact pinned is a rule and not a tuning decision: a phone whose stock driver reports Vulkan 1.1 has
    // no GPU the engine accepts unless the run asks for 1.1 (engine/src/core/GameRuntime.hpp, minimumVulkanMinor), and the start then ends before its
    // first frame. mutation: delete minimum_vulkan_minor from the shipped file, or set it to 2.
    CHECK_MSG(table.mobile.minimumVulkanMinor.has_value() && *table.mobile.minimumVulkanMinor == 1,
              "the shipped phone profile asks for Vulkan 1.1, so that a phone whose driver reports 1.1 is not refused");
    // THE STEP'S OWN RULE (planning step 201), lifted by the phone-profile step on purpose and not by accident: until then the profile turns nothing on that
    // could change a picture. The sample cap and dynamic resolution have no key at all (a key that is not in MobileBlock is refused when the file is read,
    // so that this file reading is the proof); of the settings that exist, the profile does not touch the sprites-only pipelines or the sprite report.
    // mutation: add "sprites_only_scene_pipelines": true to the shipped block (refused by name: the file stops reading), or make Resolve lay a mobile
    // sprites-only value over the table.
    for (const bool attended : {true, false}) {
        const EngineTuning::Resolved desktop = EngineTuning::Resolve(table, attended, false, EngineTuning::Overrides{});
        const EngineTuning::Resolved phone = EngineTuning::Resolve(table, attended, true, EngineTuning::Overrides{});
        CHECK_MSG(desktop.spritesOnlyScenePipelines == phone.spritesOnlyScenePipelines && desktop.spriteReport == phone.spriteReport &&
                      desktop.perfLogSeconds == phone.perfLogSeconds,
                  "the shipped phone profile leaves the sprites-only pipelines, the sprite report and the perf log as the desktop has them");
        CHECK_MSG(desktop.minimumVulkanMinor == 0, "and the desktop run does not ask for a Vulkan minimum: the engine's own 1.2");
        CHECK_MSG(phone.minimumVulkanMinor == 1, "while the phone run asks for 1.1");
    }
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
        // The phone block (planning step 201). Required as a whole, strict in each key, and a key it does not know is refused by name.
        // mutation: make ReadMobile optional, skip its key loop, or take any number for the minimum.
        {"no phone block",
         "{\"shadow_map_resolution\": 16, \"sprites_only_scene_pipelines\": true, \"asset_watching\": {\"attended_run\": false, \"other_runs\": true}, "
         "\"sprite_report\": {\"attended_run\": false, \"other_runs\": true}}",
         "mobile"},
        {"a phone block that is a list", TableText("16", "true", "{\"attended_run\": false, \"other_runs\": true}",
                                                   "{\"attended_run\": false, \"other_runs\": true}", "[1]"),
         "mobile"},
        {"the sample cap as a phone key",
         TableText("16", "true", "{\"attended_run\": false, \"other_runs\": true}", "{\"attended_run\": false, \"other_runs\": true}",
                   "{\"minimum_vulkan_minor\": 1, \"scene_samples\": 1}"),
         "mobile.scene_samples"},
        {"dynamic resolution as a phone key",
         TableText("16", "true", "{\"attended_run\": false, \"other_runs\": true}", "{\"attended_run\": false, \"other_runs\": true}",
                   "{\"dynamic_resolution\": true}"),
         "mobile.dynamic_resolution"},
        {"a misspelt phone key",
         TableText("16", "true", "{\"attended_run\": false, \"other_runs\": true}", "{\"attended_run\": false, \"other_runs\": true}",
                   "{\"minimum_vulkan_minnor\": 1}"),
         "mobile.minimum_vulkan_minnor"},
    };
    // The values of the block's own keys, each wrong in one way. (The table's other keys are the good ones.)
    const struct {
        const char* what;
        const char* block;
        const char* names;
    } badMobile[] = {
        {"a minimum of 0", "{\"minimum_vulkan_minor\": 0}", "mobile.minimum_vulkan_minor"},
        {"a minimum of 3", "{\"minimum_vulkan_minor\": 3}", "mobile.minimum_vulkan_minor"},
        {"a fractional minimum", "{\"minimum_vulkan_minor\": 1.5}", "mobile.minimum_vulkan_minor"},
        {"a text minimum", "{\"minimum_vulkan_minor\": \"1\"}", "mobile.minimum_vulkan_minor"},
        {"a null minimum", "{\"minimum_vulkan_minor\": null}", "mobile.minimum_vulkan_minor"},
        {"a phone resolution under the floor", "{\"shadow_map_resolution\": 15}", "mobile.shadow_map_resolution"},
        {"a phone resolution over the ceiling", "{\"shadow_map_resolution\": 4097}", "mobile.shadow_map_resolution"},
        {"a text phone resolution", "{\"shadow_map_resolution\": \"16\"}", "mobile.shadow_map_resolution"},
        {"phone asset watching as text", "{\"asset_watching\": \"false\"}", "mobile.asset_watching"},
        {"phone asset watching as a per-run object", "{\"asset_watching\": {\"attended_run\": false, \"other_runs\": true}}", "mobile.asset_watching"},
    };
    int n = 0;
    const auto refused = [&](const char* what, const std::string& text, const char* names) {
        EngineTuning::Table table;
        table.shadowMapResolution = 1234; // a marker: a refused read must not touch the table it was handed
        table.mobile.minimumVulkanMinor = 2;
        std::string error;
        const std::string file = Put(dir, ("bad" + std::to_string(n++) + ".json").c_str(), text);
        CHECK_MSG(!EngineTuning::Load(file, table, error), std::string(what) + ": refused");
        CHECK_MSG(error.find(names) != std::string::npos, std::string(what) + ": the error names " + names + ": " + error);
        CHECK_MSG(table.shadowMapResolution == 1234 && table.mobile.minimumVulkanMinor == 2u, std::string(what) + ": and the table is untouched");
    };
    for (const auto& row : bad) refused(row.what, row.text, row.names);
    for (const auto& row : badMobile) {
        refused(row.what, TableText("16", "true", "{\"attended_run\": false, \"other_runs\": true}", "{\"attended_run\": false, \"other_runs\": true}", row.block),
                row.names);
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

void ThePhoneBlockIsReadKeyByKey() {
    const std::filesystem::path dir = Scratch("mobile");
    const std::string watching = "{\"attended_run\": false, \"other_runs\": true}";
    const auto read = [&](const char* name, const std::string& block, EngineTuning::Table& table, std::string& error) {
        return EngineTuning::Load(Put(dir, name, TableText("16", "true", watching, watching, block)), table, error);
    };
    EngineTuning::Table table;
    std::string error;
    // The good block, all three keys and a note that is not read.
    CHECK_MSG(read("all.json", kGoodMobile, table, error), "the good block reads: " + error);
    CHECK_MSG(table.mobile.minimumVulkanMinor && *table.mobile.minimumVulkanMinor == 1 && table.mobile.shadowMapResolution &&
                  *table.mobile.shadowMapResolution == 16 && table.mobile.assetWatching && !*table.mobile.assetWatching,
              "and holds what it says");
    // An empty block is a block: every key is optional, and a key left out is a key not asked.
    EngineTuning::Table empty;
    empty.mobile.minimumVulkanMinor = 2; // a marker: a successful read REPLACES the block, it does not merge into what was there
    empty.mobile.assetWatching = true;
    CHECK_MSG(read("empty.json", "{}", empty, error), "an empty block reads: " + error);
    CHECK_MSG(!empty.mobile.minimumVulkanMinor && !empty.mobile.shadowMapResolution && !empty.mobile.assetWatching,
              "and asks for nothing (a read replaces the block)");
    // Each key alone, and the edges of each range.
    EngineTuning::Table one;
    CHECK_MSG(read("m1.json", "{\"minimum_vulkan_minor\": 1}", one, error) && one.mobile.minimumVulkanMinor == 1u && !one.mobile.shadowMapResolution &&
                  !one.mobile.assetWatching,
              "the minimum alone: " + error);
    CHECK_MSG(read("m2.json", "{\"minimum_vulkan_minor\": 2}", one, error) && one.mobile.minimumVulkanMinor == 2u, "and 2 is allowed: " + error);
    CHECK_MSG(read("s0.json", "{\"shadow_map_resolution\": 0}", one, error) && one.mobile.shadowMapResolution && *one.mobile.shadowMapResolution == 0u,
              "a phone resolution of 0, the engine's own sizes, is a value like the table's: " + error);
    CHECK_MSG(read("s4096.json", "{\"shadow_map_resolution\": 4096}", one, error) && one.mobile.shadowMapResolution == 4096u,
              "and the ceiling: " + error);
    CHECK_MSG(read("w1.json", "{\"asset_watching\": true}", one, error) && one.mobile.assetWatching && *one.mobile.assetWatching, "watching true: " + error);
    // Notes are skipped, whatever they hold, but only by their underscore: a note can say anything a key cannot.
    CHECK_MSG(read("note.json", "{\"_decision_pending\": {\"scene_samples\": \"the owner's\"}, \"_x\": 7, \"minimum_vulkan_minor\": 1}", one, error) &&
                  one.mobile.minimumVulkanMinor == 1u,
              "a note beside the keys, even one that names a key that does not exist, is not read: " + error);
}

void ThePersonsRunAndEveryOtherRunCanDiffer() {
    const std::filesystem::path dir = Scratch("kinds");
    EngineTuning::Table table;
    std::string error;
    CHECK_MSG(EngineTuning::Load(Put(dir, "t.json", TableText("16", "true")), table, error), error);
    CHECK_EQ(table.shadowMapResolution, 16u);
    CHECK(table.spritesOnlyScenePipelines);
    const EngineTuning::Resolved person = EngineTuning::Resolve(table, true, false, EngineTuning::Overrides{});
    const EngineTuning::Resolved other = EngineTuning::Resolve(table, false, false, EngineTuning::Overrides{});
    CHECK_MSG(!person.assetWatching && !person.spriteReport, "a person's run: no stat of every watched file, no sprite walk");
    CHECK_MSG(other.assetWatching && other.spriteReport, "any other run: hot reload and the log, as parity work has them");
    CHECK_MSG(person.shadowMapResolution == 16 && other.shadowMapResolution == 16, "the shadow maps are the same for both");
    CHECK_MSG(person.spritesOnlyScenePipelines && other.spritesOnlyScenePipelines, "and so are the pipelines");

    // The other way round, to show the fields are read from the right sides and not swapped.
    CHECK_MSG(EngineTuning::Load(Put(dir, "u.json", TableText("0", "false", "{\"attended_run\": true, \"other_runs\": false}",
                                                               "{\"attended_run\": true, \"other_runs\": false}")),
                                 table, error),
              error);
    const EngineTuning::Resolved person2 = EngineTuning::Resolve(table, true, false, EngineTuning::Overrides{});
    const EngineTuning::Resolved other2 = EngineTuning::Resolve(table, false, false, EngineTuning::Overrides{});
    CHECK_MSG(person2.assetWatching && person2.spriteReport && !other2.assetWatching && !other2.spriteReport,
              "attended_run is a person's run and other_runs the rest");
    CHECK_MSG(person2.shadowMapResolution == 0 && !person2.spritesOnlyScenePipelines, "0 and false are the engine as it was");
}

void TheDesktopAnswerIsTheOldAnswerForEveryTableAndEveryRun() {
    // THE PIN OF THE STEP: with mobile false, Resolve is the rule as it was, for every table, both kinds of run and every combination of flags.
    // mutation: make Resolve lay the mobile block over the answer whatever `mobile` says; make it return minimumVulkanMinor from the block when
    // mobile is false; drop the attended/other split of the watching or the report.
    const std::uint32_t sizes[] = {0, 16, 2048, 4096};
    long compared = 0;
    long mismatches = 0;
    const EngineTuning::MobileBlock blocks[] = {
        {},                                                     // asks for nothing
        {std::optional<std::uint32_t>(1), std::optional<std::uint32_t>(16), std::optional<bool>(false)}, // the shipped shape
        {std::optional<std::uint32_t>(2), std::optional<std::uint32_t>(4096), std::optional<bool>(true)}, // every value different from the table's
    };
    const std::optional<std::uint32_t> shadowFlags[] = {std::nullopt, 0u, 64u};
    const std::optional<bool> switches[] = {std::nullopt, true, false};
    const float perfs[] = {0.0f, 2.5f};
    for (const std::uint32_t size : sizes) {
        for (const bool sprites : {false, true}) {
            for (int w = 0; w < 4; ++w) {
                for (int r = 0; r < 4; ++r) {
                    for (const EngineTuning::MobileBlock& block : blocks) {
                        EngineTuning::Table table;
                        table.shadowMapResolution = size;
                        table.spritesOnlyScenePipelines = sprites;
                        table.assetWatching = {(w & 1) != 0, (w & 2) != 0};
                        table.spriteReport = {(r & 1) != 0, (r & 2) != 0};
                        table.mobile = block;
                        for (const bool attended : {true, false}) {
                            for (const auto& shadow : shadowFlags) {
                                for (const auto& sp : switches) {
                                    for (const auto& aw : switches) {
                                        for (const auto& sr : switches) {
                                            for (const float perf : perfs) {
                                                EngineTuning::Overrides flags;
                                                flags.shadowMapResolution = shadow;
                                                flags.spritesOnlyScenePipelines = sp;
                                                flags.assetWatching = aw;
                                                flags.spriteReport = sr;
                                                flags.perfLogSeconds = perf;
                                                ++compared;
                                                if (!SameAnswer(EngineTuning::Resolve(table, attended, false, flags), OldResolve(table, attended, flags))) ++mismatches;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    CHECK_MSG(compared == 4L * 2 * 4 * 4 * 3 * 2 * 3 * 3 * 3 * 3 * 2, "the grid is the size it was built to be: " + std::to_string(compared));
    CHECK_MSG(mismatches == 0, "Resolve with mobile false equals the rule as it was in every one of them: " + std::to_string(mismatches) + " differ");

    // The same, as a LITERAL table and not against a copy: one fixed table, read as the file reads, each kind of run, no flags and then the flags. These
    // numbers are not the shipped file's (they are the rows of an invented one), so no decision is pinned. mutation: any change to the desktop answer.
    EngineTuning::Table golden;
    golden.shadowMapResolution = 16;
    golden.spritesOnlyScenePipelines = true;
    golden.assetWatching = {false, true};
    golden.spriteReport = {true, false};
    golden.mobile.minimumVulkanMinor = 1;
    golden.mobile.shadowMapResolution = 4096;
    golden.mobile.assetWatching = true;
    const struct {
        bool attended;
        std::uint32_t shadow;
        bool sprites, watching, report;
    } rows[] = {{true, 16, true, false, true}, {false, 16, true, true, false}};
    for (const auto& row : rows) {
        const EngineTuning::Resolved got = EngineTuning::Resolve(golden, row.attended, false, EngineTuning::Overrides{});
        CHECK_MSG(got.shadowMapResolution == row.shadow && got.spritesOnlyScenePipelines == row.sprites && got.assetWatching == row.watching &&
                      got.spriteReport == row.report && got.perfLogSeconds == 0.0f && got.minimumVulkanMinor == 0,
                  std::string("the desktop row for ") + (row.attended ? "a person's run" : "any other run") + " is the table's own");
    }
}

void ThePhoneProfileLaysItsBlockOverTheDesktopAnswer() {
    EngineTuning::Table table;
    table.shadowMapResolution = 64;
    table.spritesOnlyScenePipelines = false;
    table.assetWatching = {true, true};
    table.spriteReport = {true, true};
    table.mobile.minimumVulkanMinor = 1;
    table.mobile.shadowMapResolution = 16;
    table.mobile.assetWatching = false;
    // mutation: ignore `mobile` (the phone gets the desktop's answer); apply the block's watching only to an attended run; forget to copy the minimum.
    for (const bool attended : {true, false}) {
        const EngineTuning::Resolved phone = EngineTuning::Resolve(table, attended, true, EngineTuning::Overrides{});
        CHECK_MSG(phone.minimumVulkanMinor == 1, "the phone run asks for the block's Vulkan minimum");
        CHECK_MSG(phone.shadowMapResolution == 16, "its shadow maps are the block's, not the table's 64");
        CHECK_MSG(!phone.assetWatching, std::string("and it watches no file, ") + (attended ? "attended" : "unattended") + ": the block's key is for both kinds of run");
        CHECK_MSG(!phone.spritesOnlyScenePipelines && phone.spriteReport, "while a setting the block does not hold stays the table's");
    }
    // A key left out of the block leaves the desktop answer, key by key.
    table.mobile.minimumVulkanMinor.reset();
    table.mobile.assetWatching.reset();
    const EngineTuning::Resolved partial = EngineTuning::Resolve(table, false, true, EngineTuning::Overrides{});
    CHECK_MSG(partial.minimumVulkanMinor == 0, "no minimum in the block is no minimum asked (0: the engine's own)");
    CHECK_MSG(partial.assetWatching, "no watching key leaves the table's own for that kind of run");
    CHECK_MSG(partial.shadowMapResolution == 16, "and the key that is there is still applied");
    table.mobile.shadowMapResolution.reset();
    const EngineTuning::Resolved none = EngineTuning::Resolve(table, true, true, EngineTuning::Overrides{});
    CHECK_MSG(SameAnswer(none, EngineTuning::Resolve(table, true, false, EngineTuning::Overrides{})),
              "an empty block makes the phone's answer the desktop's");
    // The flags are for ONE run and win over the profile as they win over the table. mutation: apply the block after the flags.
    table.mobile.minimumVulkanMinor = 1;
    table.mobile.shadowMapResolution = 16;
    table.mobile.assetWatching = false;
    EngineTuning::Overrides flags;
    flags.shadowMapResolution = 0;
    flags.assetWatching = true;
    const EngineTuning::Resolved flagged = EngineTuning::Resolve(table, true, true, flags);
    CHECK_MSG(flagged.shadowMapResolution == 0 && flagged.assetWatching && flagged.minimumVulkanMinor == 1,
              "a flag wins over the phone block, and what no flag names is still the block's");
}

void TheProfileFlagTakesExactlyTwoWords() {
    bool mobile = false;
    // mutation: accept any text, or compare case-insensitively.
    CHECK(EngineTuning::ParseProfile("mobile", mobile) && mobile);
    CHECK(EngineTuning::ParseProfile("desktop", mobile) && !mobile);
    for (const char* text : {"", "Mobile", "Desktop", "DESKTOP", "phone", "tablet", "mobile ", " mobile", "mobile\n", "desktop ", "mobil", "mobiles", "desktop,mobile", "1", "on"}) {
        mobile = true;
        CHECK_MSG(!EngineTuning::ParseProfile(text, mobile) && mobile, std::string("a profile is exactly desktop or mobile, not '") + text + "'");
        mobile = false;
        CHECK_MSG(!EngineTuning::ParseProfile(text, mobile) && !mobile, std::string("and a refused word leaves the answer alone: '") + text + "'");
    }
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
    const EngineTuning::Resolved on = EngineTuning::Resolve(table, true, false, flags);
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
    const EngineTuning::Resolved down = EngineTuning::Resolve(table, false, false, off);
    CHECK_MSG(!down.assetWatching && !down.spriteReport && !down.spritesOnlyScenePipelines && down.shadowMapResolution == 0,
              "and each turned its setting off, 0 included, over the table, for a run that is not a person's too");
    // An absent flag leaves the table's answer alone.
    EngineTuning::Overrides some;
    some.spriteReport = false;
    const EngineTuning::Resolved partial = EngineTuning::Resolve(table, true, false, some);
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
    CHECK(EngineTuning::ParsePerfLog("12.25", s) && s == 12.25f);
    CHECK(EngineTuning::ParsePerfLog("0.001", s) && s == 0.001f);
    // A plain decimal number and nothing strtod would also take: a hexadecimal float, an exponent, a bare or doubled point, a sign, white space, a digit that is
    // not an ASCII one (planning step 191; `--perf-log 0x10` used to be accepted, with a comment that said "exactly a number").
    for (const char* text : {"", "0", "-1", "3601", "nan", "inf", "5s", "abc", " 5", "1e9", "0x10", "0X10", "0x1p3", "1e2", "1E2", "2e0", ".5", "5.", "1.2.3", "+5", "-5", "5 ",
                             "5\t", "1,5", "1_0", "0.0", "00", "3600.5"}) {
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
    ThePhoneBlockIsReadKeyByKey();
    ThePersonsRunAndEveryOtherRunCanDiffer();
    TheDesktopAnswerIsTheOldAnswerForEveryTableAndEveryRun();
    ThePhoneProfileLaysItsBlockOverTheDesktopAnswer();
    TheProfileFlagTakesExactlyTwoWords();
    AFlagWinsOverTheTableForOneRun();
    TheFlagsValuesAreParsedExactly();
    TheLogLineSaysWhatTheRunAskedFor();
}

} // namespace

MP_TEST_MAIN("test_mp_tuning", 100)
