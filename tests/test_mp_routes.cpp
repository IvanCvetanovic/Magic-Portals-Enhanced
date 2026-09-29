// Every level a win route wins, won by it.
//
// A route (game/Route.hpp) is a hand-authored tests/routes/<level>.json: walks held
// over spans of the layer's ticks, taps at points of the level on stated ticks, and
// the tick by which the level must be won. This suite finds every file there at run
// time - a track adds a route without touching this code - and plays each through
// the layer as the game does: attached to a bare registry, the app's physics step
// before each of its ticks, its frame after, the pointer published over a 1280x720
// viewport, and the route's input fed through the same DEV schedule main's --route
// and --tap use, which step 100 made a real press. A route passes when goals.completed
// is set - the player in the open door - by its win_by, with no death, no tap that
// fired nothing or was refused, and no tap on a point the view did not show.
//
// For writing a route, the same binary replays one and says what it did:
//   test_mp_routes --only <level or file> [--trace <every n ticks>]
// prints the player, the camera, the shots and the portals every n ticks and on
// every tap's tick, and stops at the first problem. The planning record's step 113
// has the route format, and out/parity/program/W.1/ the attempts behind the seeds.
//
// No window and no Vulkan. Skipped with 77 without the converted levels.

#include "TestHarness.hpp"

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

// One frame as the app runs it under --fixed-step: the physics step, the game's tick
// with nothing pressed but what the route scheduled, the route's own tick after it,
// then the game's frame.
void Frame(MagicPortalsLayer& layer, Route::Driver& driver, entt::registry& registry) {
    PhysicsSystem::Update(registry, MagicPortalsLayer::kTick);
    Input::TickInput input;
    input.mousePosition = kRest;
    Input::BeginReplayedTick(input);
    layer.OnFixedUpdate(registry, MagicPortalsLayer::kTick);
    Input::EndReplayedTick();
    driver.AfterTick(layer);
    layer.OnUpdate(registry, MagicPortalsLayer::kTick);
}

void Trace(const MagicPortalsLayer& layer, entt::registry& registry, const char* why) {
    const MagicPortals::Game::Level* level = layer.SimLevel();
    std::printf("    t %5d %-5s", layer.LayerTicks(), why);
    if (level == nullptr) {
        std::printf(" no level\n");
        return;
    }
    if (level->player != entt::null && registry.valid(level->player)) {
        const glm::dvec2 at =
            MagicPortals::Units::ToPixels(registry.get<TransformComponent>(level->player).position);
        std::printf(" player (%.1f, %.1f)", at.x, at.y);
        if (const auto* body = registry.try_get<RigidBodyComponent>(level->player)) {
            std::printf(" v (%.0f, %.0f)", body->velocity.x * MagicPortals::Units::kPixelsPerMetre,
                        -body->velocity.y * MagicPortals::Units::kPixelsPerMetre);
        }
    }
    const glm::dvec2 camera = layer.CameraCentrePx();
    std::printf(" camera (%.1f, %.1f) shots %d/%d", camera.x, camera.y, level->portals.shotsFired,
                level->portals.shotsFailed);
    if (level->portals.flight) std::printf(" FLYING");
    for (const auto& placed : level->portals.placed) std::printf(" [%.1f, %.1f]", placed.atPx.x, placed.atPx.y);
    if (!level->portals.lastFailure.empty()) std::printf(" last fail: %s", level->portals.lastFailure.c_str());
    if (level->goals.completed) std::printf(" DOOR");
    // What else moves: every other traveller by its node's name, what the buttons
    // hold, and 1-32's beholder with its rocks.
    for (const entt::entity body : level->portals.travellers) {
        if (body == level->player || !registry.valid(body)) continue;
        const char* name = "?";
        for (const auto& [node, entity] : level->built.entities) {
            if (entity == body) name = node.c_str();
        }
        const glm::dvec2 at = MagicPortals::Units::ToPixels(registry.get<TransformComponent>(body).position);
        std::printf("\n          %s (%.1f, %.1f)", name, at.x, at.y);
    }
    for (const auto& button : level->channels.buttons) {
        std::printf("\n          button %s channel %d %s", button.name.c_str(), button.channel,
                    level->channels.Pressed(button.channel) ? "PRESSED" : "up");
    }
    if (level->boss.beholder) {
        const auto& boss = *level->boss.beholder;
        std::printf("\n          beholder (%.1f, %.1f) phase %d hits %d", boss.atPx.x, boss.atPx.y,
                    static_cast<int>(boss.phase), boss.hits);
        for (const auto& rock : level->boss.rocks) {
            if (!registry.valid(rock.body)) continue;
            const glm::dvec2 at = MagicPortals::Units::ToPixels(registry.get<TransformComponent>(rock.body).position);
            std::printf(" rock %s (%.1f, %.1f)", rock.name.c_str(), at.x, at.y);
        }
        if (level->boss.buttonRaised) std::printf(" BUTTON RAISED");
    }
    std::printf("\n");
}

struct Outcome {
    int wonTick = 0;
    int shots = 0;
    std::vector<std::string> problems;
};

Outcome Play(const Route::Plan& plan, const Options& options) {
    entt::registry registry;
    PublishViewport(registry);
    MagicPortalsLayer layer(RoutePaths(), plan.level);
    layer.OnAttach(registry);
    Route::Driver driver(plan);
    driver.Begin(layer);
    int lastFailed = -1;
    while (!driver.Finished()) {
        Frame(layer, driver, registry);
        if (options.trace <= 0) continue;
        const int tick = layer.LayerTicks();
        const bool tapped = std::any_of(plan.taps.begin(), plan.taps.end(),
                                        [&](const Route::Tap& tap) { return tap.tick == tick; });
        const MagicPortals::Game::Level* level = layer.SimLevel();
        const int failed = level != nullptr ? level->portals.shotsFailed : lastFailed;
        if (tapped) {
            Trace(layer, registry, "TAP");
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
    out.problems = driver.Problems();
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
        const Outcome outcome = Play(plan, options);
        if (outcome.wonTick > 0) {
            std::printf("  %s %s: won on tick %d (win_by %d), %d shot(s)\n", label.c_str(), plan.level.c_str(),
                        outcome.wonTick, plan.winBy, outcome.shots);
        } else {
            std::printf("  %s %s: NOT WON\n", label.c_str(), plan.level.c_str());
        }
        std::string problems;
        for (const std::string& problem : outcome.problems) problems += (problems.empty() ? "" : "; ") + problem;
        CHECK_MSG(outcome.problems.empty(), label + " " + plan.level + ": " + problems);
        CHECK_MSG(outcome.wonTick > 0 && outcome.wonTick <= plan.winBy,
                  label + " " + plan.level + " is won by tick " + std::to_string(plan.winBy));
    }
}

} // namespace

int main(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--only" && i + 1 < argc) {
            options.only.push_back(argv[++i]);
        } else if (arg == "--trace" && i + 1 < argc) {
            options.trace = std::max(1, std::atoi(argv[++i]));
        } else {
            std::printf("test_mp_routes [--only <level or route file>]... [--trace <every n ticks>]\n");
            return 2;
        }
    }
    std::error_code ec;
    if (!std::filesystem::is_directory(kLevels, ec) || !std::filesystem::is_regular_file(kChapters, ec) ||
        !std::filesystem::is_regular_file(kData + "/entity_roles.json", ec)) {
        std::printf("test_mp_routes: SKIPPED - needs the converted levels at %s,\n"
                    "  chapters.json at %s and the remake's data at %s.\n",
                    kLevels.c_str(), kChapters.c_str(), kData.c_str());
        return 77;
    }
    RunRoutes(options);
    return ::test::summary("test_mp_routes", options.only.empty() ? static_cast<int>(kMinRoutes) * 3 : 1);
}
