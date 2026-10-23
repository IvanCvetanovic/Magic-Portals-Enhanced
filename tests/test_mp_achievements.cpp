// The achievements as the original unlocks them (planning step 143): the port's
// own game/data/achievements.json read strictly, every rule term by term, the
// menu's pass against the tool's walk of the same bytecode, the saved set and its
// hash, the Notificator's queue, its card and the pause's popup frame.
//
// Pure sim and a scratch directory. The original's list - titles, descriptions,
// icons - is Enhanced's gitignored out/data/achievements.json; the checks that need
// it say so and are skipped without it, and nothing here embeds a word of it.

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "sim/Achievements.hpp"
#include "sim/Ease.hpp"
#include "sim/Notificator.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

using namespace MagicPortals;

namespace {

const std::string kRulesFile = std::string(MAGICPORTALS_PORT_DATA_DIR) + "/achievements.json";
const std::string kWellsFile = std::string(MAGICPORTALS_PORT_DATA_DIR) + "/gravitywell.json";
const std::string kListFile = MAGICPORTALS_ACHIEVEMENTS_FILE;

std::string Ids(const std::vector<int>& ids) {
    std::string text = "[";
    for (std::size_t i = 0; i < ids.size(); ++i) text += (i ? "," : "") + std::to_string(ids[i]);
    return text + "]";
}

std::filesystem::path Scratch() {
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "supersonic-test-mp-achievements";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir;
}

std::string ReadAll(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// A string against the one wanted, said with the line that asked.
void Is(const std::string& got, const std::string& want, int line) {
    CHECK_MSG(got == want, "line " + std::to_string(line) + ": " + got + ", wanted " + want);
}

Achievements::Rules TheRules() {
    Achievements::Rules rules;
    std::string error;
    CHECK_MSG(Achievements::LoadRules(kRulesFile, rules, error), "achievements.json reads: " + error);
    return rules;
}

// What the level answers, set by hand.
struct FakeWorld final : Achievements::World {
    std::map<std::string, int> counts;
    std::set<std::string> present;
    bool doorsShut = true;
    int spotted = 0;
    int laps = 0;

    int Count(const std::vector<std::string>& names) const override {
        int total = 0;
        for (const std::string& name : names) {
            const auto found = counts.find(name);
            if (found != counts.end()) total += found->second;
        }
        return total;
    }
    bool Exists(const std::string& name) const override { return present.count(name) > 0 || Count({name}) > 0; }
    bool LiftDoorsShut(const std::vector<std::string>&) const override { return doorsShut; }
    int SpottedCount() const override { return spotted; }
    int Laps() const override { return laps; }
};

Achievements::Play PlayAt(int world, int level, int medal, int portals, double timeMs = 0.0, bool game = true) {
    Achievements::Play play;
    play.world = world;
    play.level = level;
    play.medal = medal;
    play.portals = portals;
    play.gameTimeMs = timeMs;
    play.game = game;
    return play;
}

void TheRulesReadAndNameEveryIdOnce() {
    const Achievements::Rules rules = TheRules();
    CHECK_EQ(rules.level.size(), std::size_t{67});
    CHECK_EQ(rules.completion.size(), std::size_t{4});
    for (const auto& steps : rules.completion) {
        CHECK_MSG(steps.size() == 3 && steps[0].minPercent == 50 && steps[1].minPercent == 75 &&
                      steps[2].minPercent == 100,
                  "a chapter's badges at 50, 75 and 100, in that order");
    }
    std::vector<int> ids = rules.AllIds();
    std::sort(ids.begin(), ids.end());
    CHECK_MSG(ids.size() == 82 && std::adjacent_find(ids.begin(), ids.end()) == ids.end() && ids.front() == 0 &&
                  ids.back() == 82,
              "82 ids, each once: " + Ids(ids));
    CHECK(rules.key.colour == "red" && rules.key.id == 61 && rules.key.delayMs == 5000.0);
    CHECK(Achievements::HelpBlockId(rules, 3, 0) == 62 && Achievements::HelpBlockId(rules, 3, 1) == 63 &&
          Achievements::HelpBlockId(rules, 0, 1) == -1 && rules.helpBlockDelayMs == 1500.0);
    CHECK(rules.levelDelayMs == 2500.0 && rules.completionDelayMs == 2500.0);
    CHECK(rules.menuPortals == 99999 && rules.menuGameTimeMs == 0.0);
    CHECK(rules.card.durationMs == 3400.0 && rules.card.background == "sprites/nnotificationbg.png" &&
          rules.card.messagePrefix == "Achievement unlocked! " && rules.card.iconDirectory == "sprites/achievements/");
    CHECK(rules.locked.message == "This level is locked!" && rules.locked.durationMs == 1500.0 &&
          rules.locked.delayMs == 0.0);
    CHECK(rules.popup.tiles == "sprites/box_bg.png" && rules.popup.tileColumns == 3 && rules.popup.tileRows == 3 &&
          rules.popup.unlockedMark == "@" && rules.popup.lockedMark == "#" && rules.popup.closeAtX == 0.92 &&
          rules.popup.closeAtY == 0.5 && rules.popup.unitsPerFontPx == 0.4);
    CHECK(rules.save.file == "achievements.json" && rules.signalTorch == "light_off.ent");

    // A file that names a term it does not know, or an id twice, is refused.
    const std::string good = ReadAll(kRulesFile);
    const auto refused = [](const std::string& name, const std::string& text) {
        const std::filesystem::path path = Scratch() / name;
        std::ofstream(path, std::ios::binary | std::ios::trunc) << text;
        Achievements::Rules out;
        std::string why;
        const bool read = Achievements::LoadRules(path.string(), out, why);
        CHECK_MSG(!read && !why.empty(), name + " is refused");
    };
    std::string unknown = good;
    unknown.replace(unknown.find("{\"medal_above\": 0}"), 18, "{\"medal_below\": 0}");
    refused("unknown_term.json", unknown);
    std::string twice = good;
    twice.replace(twice.find("\"id\": 61"), 8, "\"id\": 45");
    refused("id_twice.json", twice);
}

// The ids, against Enhanced's list of them.
void TheRulesMatchTheList() {
    Achievements::Content content;
    std::string error;
    if (!Achievements::Load(kListFile, content, error)) {
        std::printf("  (skipped: the original's list is not at %s)\n", kListFile.c_str());
        return;
    }
    const Achievements::Rules rules = TheRules();
    std::vector<int> ruled = rules.AllIds();
    std::sort(ruled.begin(), ruled.end());
    std::vector<int> listed;
    for (const Achievements::Entry& entry : content.entries) listed.push_back(entry.id);
    std::sort(listed.begin(), listed.end());
    CHECK_MSG(ruled == listed, "every id the list has is unlocked by exactly one rule");
    for (const Achievements::LevelRule& rule : rules.level) {
        const Achievements::Entry* entry = content.Find(rule.id);
        CHECK_MSG(entry != nullptr && entry->world == rule.world && entry->level == rule.level,
                  "achievement " + std::to_string(rule.id) + " is the list's for " + std::to_string(rule.world) + "-" +
                      std::to_string(rule.level));
    }
    for (std::size_t world = 0; world < rules.completion.size(); ++world) {
        for (const Achievements::CompletionStep& step : rules.completion[world]) {
            const Achievements::Entry* entry = content.Find(step.id);
            CHECK_MSG(entry != nullptr && entry->world == static_cast<int>(world) && entry->level == -1,
                      "badge " + std::to_string(step.id) + " is its chapter's own");
        }
    }
    const Achievements::Entry* key = content.Find(rules.key.id);
    CHECK_MSG(key != nullptr && key->world == 3 && key->level == 18, "61 is 4-19's");
    for (const Achievements::Rules::HelpBlock& block : rules.helpBlocks) {
        const Achievements::Entry* entry = content.Find(block.id);
        CHECK(entry != nullptr && entry->world == block.world && entry->level == block.level && entry->isSecret);
    }
    // getAchievementsFromLevel: 59 of the 128 levels are named.
    int named = 0;
    for (int world = 0; world < 4; ++world) {
        for (int level = 0; level < 32; ++level) {
            if (!Achievements::FromLevel(content, world, level).empty()) ++named;
        }
    }
    CHECK_MSG(named == 59, "59 levels have the pause's button in the original, counted " + std::to_string(named));
    std::printf("  the rules name the list's 82 ids; %d levels are named\n", named);
}

// checkPreviouslyUnlockAchievements' inputs - the saved medal, 99999 portals, time
// 0, not in the game - reproduce the tool's own symbolic walk of the same function
// (from_level_medal, from_completion), for every level, medal and percentage.
void TheMenusPassIsTheToolsWalk() {
    Achievements::Content content;
    std::string error;
    if (!Achievements::Load(kListFile, content, error)) {
        std::printf("  (skipped: the original's list is not at %s)\n", kListFile.c_str());
        return;
    }
    const Achievements::Rules rules = TheRules();
    int cells = 0;
    int firing = 0;
    for (int world = 0; world < 4; ++world) {
        for (int level = 0; level < 32; ++level) {
            for (int medal = 0; medal <= 3; ++medal) {
                std::vector<int> ours = Achievements::Dispatches(
                    rules, PlayAt(world, level, medal, rules.menuPortals, rules.menuGameTimeMs, false), nullptr);
                std::sort(ours.begin(), ours.end());
                std::vector<int> tools;
                for (const Achievements::MedalRule& rule : content.fromLevelMedal) {
                    if (rule.world == world && rule.level == level && medal >= rule.minMedal) tools.push_back(rule.id);
                }
                std::sort(tools.begin(), tools.end());
                CHECK_MSG(ours == tools, std::to_string(world) + "-" + std::to_string(level) + " at medal " +
                                             std::to_string(medal) + ": " + Ids(ours) + " against the tool's " +
                                             Ids(tools));
                ++cells;
                firing += static_cast<int>(ours.size());
            }
        }
        for (int percent = 0; percent <= 100; ++percent) {
            std::vector<int> ours = Achievements::CompletionIds(rules, world, percent);
            std::vector<int> tools;
            for (const Achievements::CompletionRule& rule : content.fromCompletion) {
                if (rule.world == world && percent >= rule.minPercent) tools.push_back(rule.id);
            }
            std::vector<int> sorted = ours;
            std::sort(sorted.begin(), sorted.end());
            std::sort(tools.begin(), tools.end());
            CHECK_MSG(sorted == tools, "chapter " + std::to_string(world) + " at " + std::to_string(percent) + "%");
        }
    }
    std::printf("  the menu's pass: %d level-medal cells, %d unlocks, as the tool's walk\n", cells, firing);
}

void EveryTermIsTheOriginalsTest() {
    const Achievements::Rules rules = TheRules();
    const auto at = [&rules](const Achievements::Play& play, const FakeWorld& world) {
        return Ids(Achievements::Dispatches(rules, play, &world));
    };
    FakeWorld world;
    // 1-10: a red agent left (#216, at least 1), in the game.
    world.counts["anti_portal_agent.ent"] = 1;
    Is(at(PlayAt(0, 9, 1, 30), world), "[10]", 242);
    Is(at(PlayAt(0, 9, 3, 30, 0.0, false), world), "[]", 243);
    world.counts["anti_portal_agent.ent"] = 0;
    Is(at(PlayAt(0, 9, 3, 3), world), "[]", 245);
    // 1-26: 8 stones.
    world.counts["rolling_stone.ent"] = 8;
    Is(at(PlayAt(0, 25, 1, 9), world), "[11]", 248);
    world.counts["rolling_stone.ent"] = 7;
    Is(at(PlayAt(0, 25, 1, 9), world), "[]", 250);
    // 1-28: no portal at all.
    Is(at(PlayAt(0, 27, 1, 0), world), "[12]", 252);
    Is(at(PlayAt(0, 27, 3, 1), world), "[]", 253);
    // 1-31: every lift door shut.
    Is(at(PlayAt(0, 30, 1, 5), world), "[13]", 255);
    world.doorsShut = false;
    Is(at(PlayAt(0, 30, 3, 5), world), "[]", 257);
    // 1-13: gold and 8 portals or fewer; 1-20 and 1-21 the portals alone.
    Is(at(PlayAt(0, 12, 3, 8), world), "[78]", 259);
    Is(at(PlayAt(0, 12, 2, 8), world), "[]", 260);
    Is(at(PlayAt(0, 19, 1, 8), world), "[48]", 261);
    Is(at(PlayAt(0, 20, 1, 5), world), "[]", 262);
    // 2-01: the crate unburned, by its own name.
    world.present.insert("crate_no_emissive");
    Is(at(PlayAt(1, 0, 1, 2), world), "[15]", 265);
    world.present.clear();
    Is(at(PlayAt(1, 0, 1, 2), world), "[]", 267);
    // 2-08: every crate of six names burned.
    world.counts["crate_small"] = 1;
    Is(at(PlayAt(1, 7, 1, 2), world), "[]", 270);
    world.counts["crate_small"] = 0;
    Is(at(PlayAt(1, 7, 1, 2), world), "[16]", 272);
    // 2-14: 7 portals, and the small crate burned too.
    world.counts["crate_small_no_emissive.ent"] = 1;
    Is(at(PlayAt(1, 13, 1, 7), world), "[17]", 275);
    world.counts["crate_small_no_emissive.ent"] = 0;
    Is(at(PlayAt(1, 13, 1, 7), world), "[17,80]", 277);
    Is(at(PlayAt(1, 13, 1, 8), world), "[]", 278);
    // 2-28: the crate unburned AND gold (#652, which the description leaves out).
    world.counts["crate_small_no_emissive"] = 1;
    Is(at(PlayAt(1, 27, 3, 4), world), "[20]", 281);
    Is(at(PlayAt(1, 27, 2, 4), world), "[]", 282);
    // 2-32: 22 portals in the game, and the finish whatever.
    Is(at(PlayAt(1, 31, 1, 22), world), "[21,22]", 284);
    Is(at(PlayAt(1, 31, 1, 23), world), "[22]", 285);
    Is(at(PlayAt(1, 31, 1, 5, 0.0, false), world), "[22]", 286);
    // 3-01: every minion dead and silver at least (TP: above 1).
    world.counts["minion.ent"] = 0;
    Is(at(PlayAt(2, 0, 2, 9), world), "[23]", 289);
    Is(at(PlayAt(2, 0, 1, 9), world), "[]", 290);
    // 3-02: both minions standing and none spotted.
    world.counts["minion.ent"] = 2;
    Is(at(PlayAt(2, 1, 1, 9), world), "[24]", 293);
    world.spotted = 1;
    Is(at(PlayAt(2, 1, 1, 9), world), "[]", 295);
    world.spotted = 0;
    world.counts["minion.ent"] = 1;
    Is(at(PlayAt(2, 1, 1, 9), world), "[]", 298);
    // 3-05: 4 portals and unnoticed by two.
    world.counts["minion.ent"] = 2;
    Is(at(PlayAt(2, 4, 1, 4), world), "[27,28]", 301);
    // 3-09: two left and gold, or none left and gold.
    Is(at(PlayAt(2, 8, 3, 9), world), "[31]", 303);
    world.counts["minion.ent"] = 0;
    Is(at(PlayAt(2, 8, 3, 9), world), "[74]", 305);
    // 3-24: unnoticed by one, at any medal (no gold test).
    world.counts["minion.ent"] = 1;
    Is(at(PlayAt(2, 23, 1, 9), world), "[42]", 308);
    // 4-07: 8016 ms and gold.
    Is(at(PlayAt(3, 6, 3, 2, 8016.0), world), "[53]", 310);
    Is(at(PlayAt(3, 6, 3, 2, 8016.5), world), "[]", 311);
    // 4-17: no portal and gold; three laps round the well.
    world.laps = 3;
    Is(at(PlayAt(3, 16, 3, 0), world), "[58,60]", 314);
    world.laps = 2;
    Is(at(PlayAt(3, 16, 2, 0), world), "[]", 316);
    // 4-25: in the dark and gold, or the light on and unnoticed.
    world.present.insert("light_off.ent");
    Is(at(PlayAt(3, 24, 3, 2), world), "[65]", 319);
    world.present.clear();
    Is(at(PlayAt(3, 24, 3, 2), world), "[66]", 321);
    // 4-27: in the dark at any medal (no gold test).
    world.present.insert("light_off.ent");
    Is(at(PlayAt(3, 26, 1, 2), world), "[75]", 324);
    // 4-31: gold, then the dark too.
    Is(at(PlayAt(3, 30, 3, 2), world), "[68,69]", 326);
    Is(at(PlayAt(3, 30, 2, 2), world), "[]", 327);
    // 4-32: the finish, and gold with 12 portals.
    Is(at(PlayAt(3, 31, 3, 12), world), "[73,77]", 329);
    Is(at(PlayAt(3, 31, 3, 13), world), "[73]", 330);
    // Without a level to ask, a rule that needs it is false.
    Is(Ids(Achievements::Dispatches(rules, PlayAt(2, 1, 3, 0), nullptr)), "[]", 332);
}

// THE SEVEN STEALTH ACHIEVEMENTS (planning step 159, G1b): hasBeenUnnoticed(N) is exactly N 'minion.ent' alive AND spottedCount() ==
// 0 (ScoreDashboard 401465..401641), and spottedCount is the sum of the live knights' enemySpotCount - which the layer returns
// from Minions::State::SpotCount() since this step. Each of the seven is true with its knights standing and none having seen, and
// false with one sighting (a knight that saw and then died is out of the sum), with a knight too few or too many, and, where the
// rule asks for them, without its gold medal or with its torch lit.
void TheSevenStealthAchievementsAreTrueAndFalse() {
    const Achievements::Rules rules = TheRules();
    [[maybe_unused]] const auto at = [&rules](const Achievements::Play& play, const FakeWorld& world) {
        return Ids(Achievements::Dispatches(rules, play, &world));
    };
    struct Stealth {
        int id;
        int world;
        int level;
        int knights;
        int minMedal; // the medal the rule asks for (0 where it has no medal test)
        bool dark;    // 66 asks for NO torch to exist (the level lit by none); the others do not look
    };
    const Stealth seven[] = {{24, 2, 1, 2, 0, false},  {26, 2, 3, 2, 0, false},  {28, 2, 4, 2, 0, false},
                             {32, 2, 10, 2, 0, false}, {42, 2, 23, 1, 0, false}, {66, 3, 24, 1, 0, true},
                             {76, 3, 29, 1, 3, false}};
    for (const Stealth& rule : seven) {
        const std::string name = "achievement " + std::to_string(rule.id);
        const int medal = rule.minMedal > 0 ? rule.minMedal : 1;
        const auto fires = [&](const FakeWorld& world, int play) {
            const std::vector<int> got = Achievements::Dispatches(rules, PlayAt(rule.world, rule.level, play, 9), &world);
            return std::find(got.begin(), got.end(), rule.id) != got.end();
        };
        FakeWorld world;
        world.counts["minion.ent"] = rule.knights;
        if (rule.dark) world.present.clear(); // light_off.ent absent: the torch has been lit
        CHECK_MSG(fires(world, medal), name + ": its knights stand and none has seen");
        world.spotted = 1;
        CHECK_MSG(!fires(world, medal), name + ": one sighting spoils it");
        world.spotted = 0;
        world.counts["minion.ent"] = rule.knights - 1;
        CHECK_MSG(!fires(world, medal), name + ": a knight too few");
        world.counts["minion.ent"] = rule.knights + 1;
        CHECK_MSG(!fires(world, medal), name + ": a knight too many");
        world.counts["minion.ent"] = rule.knights;
        if (rule.minMedal > 0) CHECK_MSG(!fires(world, rule.minMedal - 1), name + ": below its medal");
        if (rule.dark) {
            world.present.insert("light_off.ent");
            CHECK_MSG(!fires(world, medal), name + ": with the torch still unlit it is not this one");
        }
    }
}

void TheHashIsTheOriginals() {
    const Achievements::Rules rules = TheRules();
    // Worked by hand from generateAchivementHashCode: sum of id x (t + 2), less 27,
    // times 3, plus 2, as a 32-bit int.
    Is(Achievements::HashOf(rules.save, ""), "", 339);
    Is(Achievements::HashOf(rules.save, "45"), "191", 340);
    Is(Achievements::HashOf(rules.save, "45,6,7"), "329", 341);
    Is(Achievements::HashOf(rules.save, "0"), "-79", 342);
    Is(Achievements::HashOf(rules.save, "3,0,37"), "383", 343);
    Is(Achievements::HashOf(rules.save, "2147483647,2147483647"), "2147483554", 344);
    Is(Achievements::ListOf({45, 6, 7}), "45,6,7", 345);
    Is(Achievements::ListOf({}), "", 346);
}

void TheSavedSetIsKept() {
    const Achievements::Rules rules = TheRules();
    const std::filesystem::path dir = Scratch() / "save";
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);
    const std::filesystem::path file = dir / rules.save.file;

    Achievements::Store store;
    std::string error;
    CHECK_MSG(store.Open(dir.string(), rules.save, error) && store.Ids().empty() && !store.FromFile(),
              "no file: nothing unlocked yet");
    CHECK(store.Add(45) && store.Add(6) && store.Add(7) && !store.Add(6));
    CHECK_MSG(store.Save(error), error);
    const std::string text = ReadAll(file);
    CHECK_MSG(text.find("\"list\": \"45,6,7\"") != std::string::npos && text.find("\"hash\": \"329\"") != std::string::npos,
              "the list and its hash: " + text);

    Achievements::Store again;
    CHECK(again.Open(dir.string(), rules.save, error) && again.FromFile() && !again.HashRefused());
    CHECK_MSG(again.Ids() == std::vector<int>({45, 6, 7}), "read back in the order of unlocking: " + Ids(again.Ids()));
    CHECK(again.Has(6) && !again.Has(8) && again.Sorted() == std::vector<int>({6, 7, 45}));

    // A hash that is not the list's: loadList keeps nothing.
    std::ofstream(file, std::ios::binary | std::ios::trunc)
        << "{\"version\": 1, \"achievements\": {\"list\": \"45,6,7\", \"hash\": \"330\"}}";
    Achievements::Store tampered;
    CHECK(tampered.Open(dir.string(), rules.save, error) && tampered.HashRefused() && tampered.Ids().empty());
    // A file that does not read: said, empty, and written over at the next save.
    std::ofstream(file, std::ios::binary | std::ios::trunc) << "{ not json";
    Achievements::Store broken;
    CHECK(!broken.Open(dir.string(), rules.save, error) && !error.empty() && broken.Ids().empty());
    CHECK(broken.Add(45) && broken.Save(error));
    Achievements::Store fixed;
    CHECK(fixed.Open(dir.string(), rules.save, error) && fixed.Ids() == std::vector<int>({45}));
    // Memory-only: no directory, nothing written.
    Achievements::Store memory;
    CHECK(memory.Open("", rules.save, error) && !memory.Persists() && memory.Add(1) && memory.Save(error));
    std::filesystem::remove_all(dir, ec);
}

Notificator::Rules TheCardRules() {
    Notificator::Rules rules;
    std::string error;
    CHECK_MSG(Notificator::LoadRules(kRulesFile, kWellsFile, rules, error), "the notificator reads: " + error);
    return rules;
}

void TheNotificatorShowsOneCardAtATime() {
    const Notificator::Rules rules = TheCardRules();
    CHECK(rules.normalizedPos == glm::dvec2(0.5, 1.0) && rules.origin == glm::dvec2(0.5, 1.0) &&
          rules.font == "Verdana40_shadow.fnt" && rules.unitsPerFontPx == 0.3 &&
          rules.textOffsetUnits == glm::dvec2(0.0, 4.0) && rules.iconUnits == glm::dvec2(22.0) &&
          rules.backgroundColumns == 3 && rules.musicVolumeShown == 0.15 && rules.musicFirstTrack == "boss");

    // Two cards at once, 2500 ms delay and 3400 ms each, on a 20 ms frame so every
    // boundary is exact: strictly past the delay, one a frame, the second's time
    // starting only once it is first.
    Notificator::Queue queue;
    Notificator::Card a;
    a.message = "A";
    a.sound = "achievement_card";
    a.durationMs = 3400.0;
    a.delayMs = 2500.0;
    Notificator::Card b = a;
    b.message = "B";
    queue.Notify(a);
    queue.Notify(b);
    std::vector<std::string> log;
    for (int frame = 1; frame <= 600; ++frame) {
        for (const Notificator::Event& event : queue.Update(20.0)) {
            log.push_back((event.kind == Notificator::Event::Kind::Shown ? "shown " : "gone ") + event.card.message +
                          "@" + std::to_string(frame));
        }
        if (frame == 200) {
            CHECK_MSG(queue.Front() != nullptr && queue.Front()->message == "A" && queue.ShownCount() == 2,
                      "A is drawn, B waits behind it");
        }
    }
    std::string joined;
    for (const std::string& line : log) joined += line + " ";
    CHECK_MSG(joined == "shown A@126 shown B@127 gone A@297 gone B@467 ",
              "2500 ms then a frame each; 3400 ms each in turn: " + joined);

    // computeEffectScale.
    const auto effect = [&rules](double durationMs, double ageMs) {
        Notificator::Card card;
        card.durationMs = durationMs;
        return Notificator::EffectOf(rules, card, ageMs);
    };
    CHECK_NEAR(static_cast<float>(effect(3400.0, 0.0).scale), 0.0f);
    CHECK_NEAR(static_cast<float>(effect(3400.0, 500.0).scale), static_cast<float>(Notificator::Elastic(rules, 0.5)));
    CHECK_NEAR(static_cast<float>(effect(3400.0, 1000.0).scale), 1.0f);
    CHECK_NEAR(static_cast<float>(effect(3400.0, 2600.0).scale), 1.0f);
    CHECK_NEAR(static_cast<float>(effect(3400.0, 2600.0).alpha), 1.0f);
    CHECK_NEAR(static_cast<float>(effect(3400.0, 3000.0).alpha), 0.5f);
    CHECK_NEAR(static_cast<float>(effect(3400.0, 3000.0).scale), static_cast<float>(Ease::SmoothEnd(0.5)));
    CHECK_NEAR(static_cast<float>(effect(3400.0, 3400.0).alpha), 0.0f);
    // The locked card's 1500: in over 750, and out over the 750 after.
    CHECK_NEAR(static_cast<float>(effect(1500.0, 750.0).scale), 1.0f);
    CHECK_NEAR(static_cast<float>(effect(1500.0, 1125.0).alpha), 0.5f);
    std::printf("  elastic(0.5) %.4f, the fade's scale at half %.4f\n", Notificator::Elastic(rules, 0.5),
                effect(3400.0, 3000.0).scale);
}

void TheCardSitsAtTheBottomCentre() {
    const Notificator::Rules rules = TheCardRules();
    const glm::dvec2 view(1280.0 / 2.8125, 256.0);
    Notificator::Card card;
    card.message = "x";
    card.icon = "sprites/achievements/i.png";
    card.durationMs = 3400.0;
    // At full size: the text 400 x 50 font px is 120 x 15 units, widened by half the
    // 22-unit icon.
    const Notificator::Layout layout = Notificator::LayOut(rules, card, 1500.0, view, glm::dvec2(400.0, 50.0));
    const double width = 120.0 + 11.0;
    const double frame = 64.0 / 3.0;
    const double cx = view.x * 0.5;
    CHECK_NEAR(static_cast<float>(layout.pieces[0].min.x), static_cast<float>(cx - width / 2.0 - frame));
    CHECK_NEAR(static_cast<float>(layout.pieces[0].min.y), 256.0f - 32.0f);
    CHECK_NEAR(static_cast<float>(layout.pieces[0].size.x), static_cast<float>(frame));
    CHECK_NEAR(static_cast<float>(layout.pieces[1].min.x), static_cast<float>(cx - width / 2.0));
    CHECK_NEAR(static_cast<float>(layout.pieces[1].size.x), static_cast<float>(width));
    CHECK_NEAR(static_cast<float>(layout.pieces[2].min.x), static_cast<float>(cx + width / 2.0));
    CHECK_NEAR(static_cast<float>(layout.uvMin[1].x), static_cast<float>(1.0 / 3.0));
    CHECK_NEAR(static_cast<float>(layout.uvMax[2].x), 1.0f);
    CHECK_NEAR(static_cast<float>(layout.textCentre.x), static_cast<float>(cx + 11.0));
    CHECK_NEAR(static_cast<float>(layout.textCentre.y), static_cast<float>(256.0 - 16.0 + 4.0));
    CHECK_NEAR(static_cast<float>(layout.unitsPerFontPx), 0.3f);
    CHECK(layout.hasIcon);
    CHECK_NEAR(static_cast<float>(layout.icon.min.x + 11.0), static_cast<float>(cx - width / 2.0));
    CHECK_NEAR(static_cast<float>(layout.icon.min.y + 11.0), static_cast<float>(256.0 - 16.0 + 4.0));
    // No icon: no pitch.
    card.icon.clear();
    const Notificator::Layout bare = Notificator::LayOut(rules, card, 1500.0, view, glm::dvec2(400.0, 50.0));
    CHECK(!bare.hasIcon);
    CHECK_NEAR(static_cast<float>(bare.textCentre.x), static_cast<float>(cx));
    CHECK_NEAR(static_cast<float>(bare.pieces[1].size.x), 120.0f);
}

void TheBoxFramesTheText() {
    const glm::dvec2 centre(227.5, 128.0);
    const Notificator::Box box =
        Notificator::LayOutBox(glm::dvec2(64.0), 3, 3, 1.0, 0.4, centre, glm::dvec2(0.5), glm::dvec2(200.0, 100.0));
    const glm::dvec2 size(80.0, 40.0);
    const glm::dvec2 pos = centre - size * 0.5;
    const double tile = 64.0 / 3.0;
    CHECK_NEAR(static_cast<float>(box.textTopLeft.x), static_cast<float>(pos.x));
    CHECK_NEAR(static_cast<float>(box.pieces[0].min.x), static_cast<float>(pos.x - tile));
    CHECK_NEAR(static_cast<float>(box.pieces[0].min.y), static_cast<float>(pos.y - tile));
    CHECK_NEAR(static_cast<float>(box.pieces[4].min.x), static_cast<float>(pos.x));
    CHECK_NEAR(static_cast<float>(box.pieces[4].size.x), 80.0f);
    CHECK_NEAR(static_cast<float>(box.pieces[4].size.y), 40.0f);
    CHECK_NEAR(static_cast<float>(box.pieces[8].min.x), static_cast<float>(pos.x + 80.0));
    CHECK_NEAR(static_cast<float>(box.pieces[8].min.y), static_cast<float>(pos.y + 40.0));
    CHECK_NEAR(static_cast<float>(box.pieces[3].min.x), static_cast<float>(pos.x - tile));
    CHECK_NEAR(static_cast<float>(box.pieces[3].size.y), 40.0f);
    CHECK_NEAR(static_cast<float>(box.uvMin[4].x), static_cast<float>(1.0 / 3.0));
    CHECK_NEAR(static_cast<float>(box.uvMax[4].y), static_cast<float>(2.0 / 3.0));
    // The bounce scales the tiles only.
    const Notificator::Box half =
        Notificator::LayOutBox(glm::dvec2(64.0), 3, 3, 0.5, 0.4, centre, glm::dvec2(0.5), glm::dvec2(200.0, 100.0));
    CHECK_NEAR(static_cast<float>(half.pieces[0].size.x), static_cast<float>(tile * 0.5));
    CHECK_NEAR(static_cast<float>(half.pieces[4].size.x), 80.0f);
}

void ThePopupSaysWhatTheLevelHas() {
    const Achievements::Rules rules = TheRules();
    Achievements::Content content;
    content.secret.title = "S";
    content.secret.descriptionPrefix = "P";
    content.secret.descriptionSeparator = "~";
    const auto entry = [](int id, int world, int level, const char* title, bool secret) {
        Achievements::Entry made;
        made.id = id;
        made.world = world;
        made.level = level;
        made.title = title;
        made.description = std::string(title) + " said";
        made.isSecret = secret;
        return made;
    };
    content.entries = {entry(1, 0, 2, "T1", false), entry(5, 0, -1, "Badge", false), entry(2, 0, 2, "T2", true),
                       entry(3, 1, 2, "T3", false)};
    const std::vector<const Achievements::Entry*> level = Achievements::FromLevel(content, 0, 2);
    CHECK(level.size() == 2 && level[0]->id == 1 && level[1]->id == 2);
    CHECK(Achievements::FromLevel(content, 0, -1).empty() && Achievements::FromLevel(content, 2, 2).empty());
    Is(Achievements::PopupText(rules, content, level, {1}), "@ T1:\nT1 said\n\n# S:\nP1~3", 536);
    Is(Achievements::PopupText(rules, content, level, {1, 2}), "@ T1:\nT1 said\n\n@ T2:\nT2 said", 537);
}

void runTests() {
    TheRulesReadAndNameEveryIdOnce();
    TheRulesMatchTheList();
    TheMenusPassIsTheToolsWalk();
    EveryTermIsTheOriginalsTest();
    TheSevenStealthAchievementsAreTrueAndFalse();
    TheHashIsTheOriginals();
    TheSavedSetIsKept();
    TheNotificatorShowsOneCardAtATime();
    TheCardSitsAtTheBottomCentre();
    TheBoxFramesTheText();
    ThePopupSaysWhatTheLevelHas();
}

} // namespace

MP_TEST_MAIN("test_mp_achievements", 150)
