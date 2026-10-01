// Every level a win route wins, won by it.
//
// A route (game/Route.hpp) is a hand-authored tests/routes/<level>.json: walks held
// over spans of the layer's ticks, taps at points of the level on stated ticks, and
// the tick by which the level must be won. This suite finds every file there at run
// time - a track adds a route without touching this code - and plays each through
// the layer as the game does: attached to a bare registry, the app's physics step
// before each of its ticks, its frame after, the pointer published over a 1280x720
// viewport, and the route's input fed through the same DEV schedule main's --route
// and --tap use, which step 100 made a real press: each tap down on its tick and up
// on its release, where its shot leaves (step 137). A route passes when
// goals.completed is set - the player in the open door - by its win_by, with no
// death, no tap that fired nothing on its release or was refused, and no tap on a
// point the view did not show. Each route's line gives its shots' largest aim drift:
// how far the point under a finger was from the point the file names, where the
// camera moved while the finger was down (Route.hpp).
//
// For writing a route, the same binary replays one and says what it did:
//   test_mp test_mp_routes --only <level or file> [--trace <every n ticks>]
// prints the player, the camera, the shots and the portals every n ticks and on
// the tick every tap comes up, and stops at the first problem. The planning record's step 113
// has the route format, and out/parity/program/W.1/ the attempts behind the seeds.
//
// No window and no Vulkan. Skipped with 77 without the converted levels.

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "MagicPortalsLayer.hpp"
#include "Route.hpp"

#include "core/Components.hpp"
#include "core/Input.hpp"
#include "core/PhysicsSystem.hpp"
#include "core/ViewportInfo.hpp"

#include "sim/Chapters.hpp"
#include "sim/Game.hpp"
#include "sim/Units.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

using namespace Supersonic;
using MagicPortals::MagicPortalsLayer;
namespace Route = MagicPortals::Route;

namespace {

const std::string kLevels = MAGICPORTALS_LEVELS_DIR;
const std::string kData = MAGICPORTALS_DATA_DIR;
const std::string kChapters = MAGICPORTALS_CHAPTERS_FILE;
const std::string kRoutes = MAGICPORTALS_ROUTES_DIR;
// Where the pointer rests between taps, as test_mp_layer rests it: a scheduled tap
// owns the pointer on its own ticks wherever this is.
const glm::vec2 kRest(640.0f, 360.0f);

// The floor on how many routes there are, so a lost directory or a bad glob fails
// rather than passing on nothing, and the three the program named first.
constexpr std::size_t kMinRoutes = 10;
const char* const kSeedLevels[] = {"level0", "level8", "level31"};

struct Options {
    std::vector<std::string> only; // level names or route files; empty: every route
    int trace = 0;                  // print every n ticks; 0: only the outcome
};

MagicPortalsLayer::Paths RoutePaths() {
    MagicPortalsLayer::Paths paths;
    paths.prisms = std::filesystem::temp_directory_path() / "supersonic-test-mp-routes";
    return paths;
}

void PublishViewport(entt::registry& registry) {
    ViewportInfo viewport;
    viewport.rect.min = glm::vec2(0.0f, 0.0f);
    viewport.rect.max = glm::vec2(1280.0f, 720.0f);
    viewport.pointerOverGame = true;
    registry.ctx().insert_or_assign(viewport);
}

// Every sound a route's run latched, by event (planning step 142).
using Census = std::map<std::string, int>;

// ETHCallback_light_wall #57-97: its switch once its elapsedTime is above 800
// (CMPIu v6, v800), sounds.json cues.light_switch_after_ms.
constexpr double kLightSwitchAfterMs = 800.0;

// Two latches whose tick a count cannot show (planning step 142's second fix
// round): light_switch on the first tick of a light wall's fade past 800 ms, and
// fire_diamond_drained on the tick a gutter takes a fire diamond
// (ETHCallback_fire_diamond #427-455). Each tick's latches against what the level
// did on that tick; a tick where they differ is kept for the route's check.
struct Moments {
    bool wallPast = false;      // the wall's fade was past 800 ms at the last tick
    std::vector<char> fireGone; // per diamond: a fire one, gone, at the last tick
    bool marked = false;        // the first tick has been read
    int switches = 0;           // due on their tick, and so heard
    int drains = 0;
    std::string off;            // each tick whose latches were not what was due

    void Tick(const MagicPortals::Game::Level& level, const std::vector<std::string>& latched, int tick) {
        const auto heard = [&latched](const char* event) {
            return static_cast<int>(std::count(latched.begin(), latched.end(), std::string(event)));
        };
        // A wall put back and flagged again fades afresh, with its own switch.
        const bool past = level.torch.wall.going && level.torch.wall.goingMs > kLightSwitchAfterMs;
        const int switchDue = past && !wallPast ? 1 : 0;
        wallPast = past;
        // A level's first tick reads its diamonds against the level as it is made:
        // none gone.
        std::vector<char> gone;
        for (const auto& diamond : level.diamonds.diamonds) {
            gone.push_back(diamond.fire && diamond.gone ? char{1} : char{0});
        }
        if (!marked) fireGone.assign(gone.size(), char{0});
        marked = true;
        int drainDue = 0;
        if (gone.size() == fireGone.size()) {
            for (std::size_t i = 0; i < gone.size(); ++i) drainDue += gone[i] != 0 && fireGone[i] == 0 ? 1 : 0;
        }
        fireGone = std::move(gone);
        const int switched = heard("light_switch");
        const int drained = heard("fire_diamond_drained");
        if (switched != switchDue || drained != drainDue) {
            off += " tick " + std::to_string(tick) + ": light_switch " + std::to_string(switched) + " of " +
                   std::to_string(switchDue) + " due, fire_diamond_drained " + std::to_string(drained) + " of " +
                   std::to_string(drainDue) + ";";
        }
        switches += switchDue;
        drains += drainDue;
    }
};

// One frame as the app runs it under --fixed-step: the physics step, the game's tick
// with nothing pressed but what the route scheduled, the route's own tick after it,
// then the game's frame - which plays, and so clears, what the tick latched: counted
// into `census` first, and read against the tick's moments.
void Frame(MagicPortalsLayer& layer, Route::Driver& driver, entt::registry& registry, Census& census,
           Moments& moments) {
    PhysicsSystem::Update(registry, MagicPortalsLayer::kTick);
    Input::TickInput input;
    input.mousePosition = kRest;
    Input::BeginReplayedTick(input);
    layer.OnFixedUpdate(registry, MagicPortalsLayer::kTick);
    Input::EndReplayedTick();
    driver.AfterTick(layer);
    const std::vector<std::string> latched = layer.LatchedSounds();
    for (const std::string& event : latched) ++census[event];
    if (layer.SimLevel() != nullptr) moments.Tick(*layer.SimLevel(), latched, layer.LayerTicks());
    layer.OnUpdate(registry, MagicPortalsLayer::kTick);
}

// What a won route's run heard, against what its level's own counters say
// happened: each of step 142's latches at its event, once per event (sounds.json
// carries the decode). A relation that holds for every route, or for the level
// whose mechanism it is. And two latches on their very tick (Moments).
void CheckCensus(const std::string& label, const Census& census, const Moments& moments,
                 const MagicPortals::Game::Level& level) {
    const auto heard = [&census](const char* event) {
        const auto found = census.find(event);
        return found == census.end() ? 0 : found->second;
    };
    const auto pin = [&label](bool holds, const std::string& what) { CHECK_MSG(holds, label + ": " + what); };
    pin(moments.off.empty(), "light_switch on its wall's first tick past 800 ms (" + std::to_string(moments.switches) +
                                 " due) and fire_diamond_drained on its diamond's (" +
                                 std::to_string(moments.drains) + " due), on no other:" + moments.off);
    // Silent in the original, so never latched.
    for (const char* silent : {"stone_thrown", "boss_rock_broken", "boss_button_raised", "button_pressed"}) {
        pin(heard(silent) == 0, std::string(silent) + " is silent");
    }
    int timed = 0;
    for (const auto& crystal : level.goals.crystals) timed += crystal.timed ? 1 : 0;
    pin(heard("crystal_alert") == timed, "one crystal_alert per timed crystal: " +
                                             std::to_string(heard("crystal_alert")) + " of " + std::to_string(timed));
    pin(heard("bomb_blast") == level.fire.blasts, "a bomb_blast per blast");
    int spat = level.turrets.converted + level.dragon.fired + level.darkDragon.fired;
    for (const auto& turret : level.turrets.turrets) spat += turret.fired;
    pin(heard("fireball_spat") == spat, "a fireball_spat per fireball made: " + std::to_string(heard("fireball_spat")) +
                                            " of " + std::to_string(spat));
    int spent = 0;
    for (const auto& key : level.keys.keys) spent += key.spent ? 1 : 0;
    pin(heard("keyhole_vanished") == level.keys.opened, "a keyhole_vanished per door opened");
    pin(heard("door_unlocked") <= spent && heard("key_unlocked") <= heard("door_unlocked"),
        "a key's fly-in: door_unlocked then key_unlocked, for spent keys");
    pin(heard("key_picked") >= (level.keys.picked > 0 ? 1 : 0), "a key_picked where a key was taken");
    pin(heard("minion_fell") == level.minions.killed, "a minion_fell per minion the killer floor took");
    pin(heard("claw_crushed") == level.dragon.crushed, "a claw_crushed per platform the claw took");
    if (level.boss.beholder.has_value()) {
        const auto& beholder = *level.boss.beholder;
        pin(heard("boss_appeared") == 1, "the beholder appears once");
        pin(heard("boss_struck") == beholder.hits && heard("boss_hurt") == beholder.hits,
            "boss_struck and boss_hurt together, per hit: " + std::to_string(beholder.hits));
        pin(heard("boss_rock_dropped") == beholder.rocksThrown, "a boss_rock_dropped per rock");
        pin(heard("boss_dead") == (beholder.gone ? 1 : 0), "boss_dead as it goes");
        // DEAD for 3550 ms and a tick: small explosions at 400, 800 ... 3200 ms.
        if (beholder.gone) pin(heard("boss_explosion") == 8, "eight small explosions: " +
                                                                 std::to_string(heard("boss_explosion")));
    }
    if (level.dragon.present) {
        pin(heard("dragon_appeared") == 1, "the dragon appears once");
        pin(heard("dragon_took_off") == (level.dragon.tookOff ? 1 : 0), "dragon_took_off as it takes off");
    }
    if (level.ghost.present && level.ghost.spawned) {
        const auto& ghost = level.ghost;
        pin(heard("ghost_appeared") == 1, "the ghost appears once");
        pin(heard("ghost_item") == ghost.beats + ghost.summons, "a ghost_item per beat and summon");
        pin(heard("ghost_hurt") >= ghost.hits, "a ghost_hurt per burn, and per rage left");
        pin(heard("enemy_shot") == ghost.shotsFired, "an enemy_shot per shot");
        pin(heard("ghost_burst") == (ghost.gone ? 1 : 0), "its burst as it goes");
        pin(heard("ghost_dead") == (ghost.phase == MagicPortals::Ghost::Phase::Death ? 1 : 0), "its death, once");
    }
    if (level.darkDragon.present && level.darkDragon.summoned) {
        const auto& dark = level.darkDragon;
        pin(heard("dark_dragon_appeared") == 1 && heard("dark_dragon_mouth") == 1, "it appears and opens its jaw once");
        pin(heard("dark_dragon_item") == dark.hits, "a dark_dragon_item per wound");
        pin(heard("dark_dragon_dead") == (dark.phase == MagicPortals::DarkDragon::Phase::Dying ? 1 : 0),
            "its death, once");
        pin(heard("dark_dragon_burst") == (dark.gone ? 1 : 0), "its burst as it goes");
        // Each wound's damage sound at its start and at DAMAGE's end, but the last,
        // which goes straight into DYING.
        if (dark.gone) {
            pin(heard("dark_dragon_hurt") == 2 * dark.hits - 1,
                "dark_dragon_hurt twice a wound but the last: " + std::to_string(heard("dark_dragon_hurt")));
        }
    }
}

std::string Describe(const Census& census) {
    std::string text;
    for (const auto& [event, count] : census) text += " " + event + " x" + std::to_string(count);
    return text.empty() ? " none" : text;
}

// The trace a route is written by, which main's --route-trace prints too (Route.hpp).
void Trace(const MagicPortalsLayer& layer, entt::registry& registry, const char* why) {
    Route::Trace(layer, registry, why);
}

struct Outcome {
    int wonTick = 0;
    int shots = 0;
    int held = 0;
    double driftPx = 0.0;
    std::vector<std::string> problems;
    Census census;
    bool censusChecked = false;
    // The achievements the run unlocked, and what its door asked (planning step 143).
    std::vector<MagicPortalsLayer::Unlock> unlocks;
    std::optional<MagicPortals::Achievements::Play> door;
};

// Every unlock of a won route is one its own level's rules or its chapter's
// badges name, at its door with a card - or the red key's, before it - and every
// rule of its level whose terms are all the medal's fires exactly when the door's
// medal passes them: the evaluator against the plain arithmetic, in play.
void CheckAchievements(const std::string& label, const Outcome& outcome,
                       const MagicPortals::Achievements::Rules& rules) {
    namespace Achievements = MagicPortals::Achievements;
    CHECK_MSG(outcome.door.has_value(), label + ": its door asked for achievements");
    if (!outcome.door) return;
    const Achievements::Play& door = *outcome.door;
    const auto unlocked = [&outcome](int id) {
        return std::any_of(outcome.unlocks.begin(), outcome.unlocks.end(),
                           [id](const MagicPortalsLayer::Unlock& unlock) { return unlock.id == id; });
    };
    for (const MagicPortalsLayer::Unlock& unlock : outcome.unlocks) {
        bool named = unlock.id == rules.key.id && unlock.tick <= outcome.wonTick;
        for (const Achievements::LevelRule& rule : rules.level) {
            named = named || (rule.id == unlock.id && rule.world == door.world && rule.level == door.level &&
                              unlock.tick == outcome.wonTick);
        }
        if (static_cast<std::size_t>(door.world) < rules.completion.size()) {
            for (const Achievements::CompletionStep& step : rules.completion[static_cast<std::size_t>(door.world)]) {
                named = named || (step.id == unlock.id && unlock.tick == outcome.wonTick);
            }
        }
        CHECK_MSG(named && unlock.notify, label + ": achievement " + std::to_string(unlock.id) + " on tick " +
                                              std::to_string(unlock.tick) + " is its own, with a card");
    }
    for (const Achievements::LevelRule& rule : rules.level) {
        if (rule.world != door.world || rule.level != door.level) continue;
        bool medalOnly = true;
        bool passes = true;
        for (const Achievements::Term& term : rule.terms) {
            if (term.kind == Achievements::TermKind::MedalAtLeast) {
                passes = passes && door.medal >= static_cast<int>(term.value);
            } else if (term.kind == Achievements::TermKind::MedalAbove) {
                passes = passes && door.medal > static_cast<int>(term.value);
            } else {
                medalOnly = false;
            }
        }
        if (!medalOnly) continue;
        CHECK_MSG(unlocked(rule.id) == passes, label + ": achievement " + std::to_string(rule.id) +
                                                   (passes ? " unlocked" : " not unlocked") + " at medal " +
                                                   std::to_string(door.medal));
    }
}

std::string DescribeUnlocks(const Outcome& outcome) {
    std::string text;
    for (const MagicPortalsLayer::Unlock& unlock : outcome.unlocks) {
        text += " " + std::to_string(unlock.id) + "@" + std::to_string(unlock.tick);
    }
    if (text.empty()) text = " none";
    if (outcome.door) {
        char play[96];
        std::snprintf(play, sizeof play, " (door: medal %d, %d portals, %.1f ms)", outcome.door->medal,
                      outcome.door->portals, outcome.door->gameTimeMs);
        text += play;
    }
    return text;
}

Outcome Play(const Route::Plan& plan, const Options& options, const std::string& label) {
    entt::registry registry;
    PublishViewport(registry);
    MagicPortalsLayer layer(RoutePaths(), plan.level);
    layer.OnAttach(registry);
    Route::Driver driver(plan);
    driver.Begin(layer);
    int lastFailed = -1;
    Census census;
    Moments moments;
    while (!driver.Finished()) {
        Frame(layer, driver, registry, census, moments);
        if (options.trace <= 0) continue;
        const int tick = layer.LayerTicks();
        // The tick a tap comes up on, where its shot has just left.
        const bool tapped = Route::ReleasesOn(plan, tick);
        const MagicPortals::Game::Level* level = layer.SimLevel();
        const int failed = level != nullptr ? level->portals.shotsFailed : lastFailed;
        if (tapped) {
            Trace(layer, registry, "TAP");
            if (!driver.Aims().empty() && driver.Aims().back().tick == tick) {
                std::printf("          the aim's drift %.3f px\n", driver.Aims().back().driftPx);
            }
        } else if (failed != lastFailed && lastFailed >= 0) {
            Trace(layer, registry, "FAIL");
        } else if (tick % options.trace == 0 || driver.Finished()) {
            Trace(layer, registry, driver.WonTick() > 0 ? "WON" : "");
        }
        lastFailed = failed;
    }
    Outcome out;
    out.wonTick = driver.WonTick();
    out.shots = driver.Shots();
    out.held = driver.Held();
    out.driftPx = driver.MaxDriftPx();
    out.problems = driver.Problems();
    out.census = std::move(census);
    out.unlocks = layer.Unlocks();
    out.door = layer.LastDoorPlay();
    if (out.wonTick > 0 && layer.SimLevel() != nullptr) {
        CheckCensus(label + " " + plan.level, out.census, moments, *layer.SimLevel());
        out.censusChecked = true;
    }
    layer.OnDetach(registry);
    return out;
}

std::vector<std::filesystem::path> Discover() {
    std::vector<std::filesystem::path> files;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(kRoutes, ec)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") files.push_back(entry.path());
    }
    std::sort(files.begin(), files.end());
    return files;
}

// "1-09", as the backlog and the captures name a level.
std::string Label(const MagicPortals::Chapters::Table& chapters, const std::string& level) {
    const int at = chapters.Find(level);
    if (at < 0) return "?";
    const auto& entry = chapters.levels[static_cast<std::size_t>(at)];
    char text[16];
    std::snprintf(text, sizeof text, "%d-%02d", entry.world + 1, entry.index + 1);
    return text;
}

void RunRoutes(const Options& options) {
    MagicPortals::Chapters::Table chapters;
    std::string error;
    CHECK_MSG(MagicPortals::Chapters::Load(kChapters, chapters, error), error);
    MagicPortals::Achievements::Rules achievementRules;
    const bool rulesRead = MagicPortals::Achievements::LoadRules(
        std::string(MAGICPORTALS_PORT_DATA_DIR) + "/achievements.json", achievementRules, error);
    CHECK_MSG(rulesRead, error);

    std::vector<std::filesystem::path> files;
    if (options.only.empty()) {
        files = Discover();
        std::printf("  %zu route(s) in %s\n", files.size(), kRoutes.c_str());
        CHECK_MSG(files.size() >= kMinRoutes, "at least " + std::to_string(kMinRoutes) + " routes, found " +
                                                  std::to_string(files.size()));
        for (const char* seed : kSeedLevels) {
            const bool there = std::any_of(files.begin(), files.end(), [&](const std::filesystem::path& file) {
                return file.stem().string() == seed;
            });
            CHECK_MSG(there, std::string("the seed route ") + seed + ".json is there");
        }
    } else {
        for (const std::string& what : options.only) {
            const std::filesystem::path given(what);
            files.push_back(given.has_extension() ? given : std::filesystem::path(kRoutes) / (what + ".json"));
        }
    }

    for (const std::filesystem::path& file : files) {
        Route::Plan plan;
        if (!Route::Load(file.string(), plan, error)) {
            CHECK_MSG(false, error);
            continue;
        }
        CHECK_MSG(file.stem().string() == plan.level,
                  file.filename().string() + " is named for its level, " + plan.level);
        const std::string label = Label(chapters, plan.level);
        const Outcome outcome = Play(plan, options, label);
        // With the entries the platform gate held on the way, where it held any (step 136).
        const std::string held = outcome.held > 0 ? ", held " + std::to_string(outcome.held) : std::string();
        if (outcome.wonTick > 0) {
            std::printf("  %s %s: won on tick %d (win_by %d), %d shot(s), the aim's drift at most %.3f px%s\n",
                        label.c_str(), plan.level.c_str(), outcome.wonTick, plan.winBy, outcome.shots,
                        outcome.driftPx, held.c_str());
        } else {
            std::printf("  %s %s: NOT WON%s\n", label.c_str(), plan.level.c_str(), held.c_str());
        }
        // What it heard (planning step 142), on a line of its own.
        std::printf("  %s %s sounds:%s\n", label.c_str(), plan.level.c_str(), Describe(outcome.census).c_str());
        // And what it unlocked (planning step 143).
        std::printf("  %s %s achievements:%s\n", label.c_str(), plan.level.c_str(), DescribeUnlocks(outcome).c_str());
        if (outcome.wonTick > 0 && rulesRead) CheckAchievements(label + " " + plan.level, outcome, achievementRules);
        std::string problems;
        for (const std::string& problem : outcome.problems) problems += (problems.empty() ? "" : "; ") + problem;
        CHECK_MSG(outcome.problems.empty(), label + " " + plan.level + ": " + problems);
        CHECK_MSG(outcome.wonTick > 0 && outcome.wonTick <= plan.winBy,
                  label + " " + plan.level + " is won by tick " + std::to_string(plan.winBy));
    }
}

} // namespace

MP_SUITE_MAIN_ARGS("test_mp_routes", argc, argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--only" && i + 1 < argc) {
            options.only.push_back(argv[++i]);
        } else if (arg == "--trace" && i + 1 < argc) {
            options.trace = std::max(1, std::atoi(argv[++i]));
        } else {
            std::printf("test_mp test_mp_routes [--only <level or route file>]... [--trace <every n ticks>]\n");
            return 2;
        }
    }
    std::error_code ec;
    if (!std::filesystem::is_directory(kLevels, ec) || !std::filesystem::is_regular_file(kChapters, ec) ||
        !std::filesystem::is_regular_file(kData + "/entity_roles.json", ec)) {
        std::printf("test_mp_routes: SKIPPED - needs the converted levels at %s,\n"
                    "  chapters.json at %s and Enhanced's data at %s.\n",
                    kLevels.c_str(), kChapters.c_str(), kData.c_str());
        return 77;
    }
    RunRoutes(options);
    return ::test::summary("test_mp_routes", options.only.empty() ? static_cast<int>(kMinRoutes) * 3 : 1);
}
