// The Supersonic Engine's intro through the real layer (planning step 272): sim/Splash.hpp is the rule and test_mp_splash pins it over numbers; this suite holds the LAYER to
// it. A bare layer with Paths::splash, driven tick by tick and frame by frame as the app drives it (the engine's own device path: a RawInputState, Input::Update,
// BeginTickInput, then OnFixedUpdate for a tick and OnUpdate for the frame), over the converted levels.
//
// WHAT IS PINNED:
//   - the intro is up from the attach, and for the 120 ticks of its timeline the game does NOT tick: the level's step count stays 0, nothing of the game is on the frame (the
//     overlay holds the flat ground and the logo and nothing else), and the 121st tick is the game's first;
//   - the frame is the ground over the whole window and the logo at the timeline's alpha: 0 at tick 0, 1 from tick 24, the logo's texture is the file in the port's data;
//   - a key pressed before tick 18 changes nothing; a key pressed at tick 30 fades the intro out and the game's first tick is the one after tick 54; a key held from the first
//     look is no press; a click (the mouse button) skips like a key;
//   - a layer built without Paths::splash (every other suite's, every capture's) has no intro and ticks from its first tick as it always did;
//   - a logo that cannot be read (a data folder without images/splash) is no intro, never a blank two seconds.
// Each pin says what turns it red in the Step's mutation list.
//
// NOT TESTED here: the picture on a screen (a capture: --splash on --fixed-step --frames 30 --screenshot <absolute path>) and main's rule for who sees it (test_mp_splash's
// Wanted).

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "MagicPortalsLayer.hpp"
#include "sim/Splash.hpp"

#include "core/Components.hpp"
#include "core/Input.hpp"
#include "core/PhysicsSystem.hpp"
#include "core/ScreenOverlay.hpp"
#include "core/ViewportInfo.hpp"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

using namespace Supersonic;
using MagicPortals::MagicPortalsLayer;
namespace Splash = MagicPortals::Splash;

namespace {

const std::string kLevels = MAGICPORTALS_LEVELS_DIR;
const std::string kData = MAGICPORTALS_DATA_DIR;
const std::string kChapters = MAGICPORTALS_CHAPTERS_FILE;
const glm::vec2 kRest(640.0f, 360.0f);

MagicPortalsLayer::Paths IntroPaths(bool splash) {
    MagicPortalsLayer::Paths paths;
    paths.prisms = std::filesystem::temp_directory_path() / "supersonic-test-mp-splashflow";
    paths.saveDir = (std::filesystem::temp_directory_path() / "supersonic-test-mp-splashflow-saves").string();
    paths.splash = splash;
    return paths;
}

void publishViewport(entt::registry& registry) {
    ViewportInfo viewport;
    viewport.rect.min = glm::vec2(0.0f, 0.0f);
    viewport.rect.max = glm::vec2(1280.0f, 720.0f);
    viewport.pointerOverGame = true;
    registry.ctx().insert_or_assign(viewport);
}

// One tick the way the app runs a live one: the devices as a RawInputState (a key, the mouse's left button), the engine's own contact and edges, then the layer.
void liveTick(MagicPortalsLayer& layer, entt::registry& registry, bool keyDown = false, bool mouseDown = false) {
    RawInputState raw;
    raw.mousePosition = kRest;
    raw.keys[Key::Space] = keyDown;
    raw.mouseButtons[MouseButton::Left] = mouseDown;
    Input::SynthesiseMouseContact(raw);
    Input::Update(raw);
    Input::BeginTickInput();
    PhysicsSystem::Update(registry, MagicPortalsLayer::kTick);
    layer.OnFixedUpdate(registry, MagicPortalsLayer::kTick);
}

// The frame the app draws after a tick, into an overlay of our own.
std::vector<ScreenOverlay::Quad> frame(MagicPortalsLayer& layer, entt::registry& registry) {
    static ScreenOverlay overlay;
    overlay.Clear();
    registry.ctx().insert_or_assign<ScreenOverlay*>(&overlay);
    layer.OnUpdate(registry, MagicPortalsLayer::kTick);
    return overlay.Quads();
}

void restDevices() {
    for (int i = 0; i < 2; ++i) {
        RawInputState raw;
        raw.mousePosition = kRest;
        Input::Update(raw);
        Input::BeginTickInput();
    }
}

bool IsTheIntroFrame(const std::vector<ScreenOverlay::Quad>& quads, float expectAlpha, bool& ok) {
    ok = !quads.empty() && quads.front().min == glm::vec2(0.0f) && quads.front().max == glm::vec2(1.0f) && quads.front().texture.empty();
    if (expectAlpha <= 0.0f) return ok && quads.size() == 1;
    return ok && quads.size() == 2 && !quads[1].texture.empty() && std::fabs(quads[1].color.a - expectAlpha) < 1e-5f;
}

// Runs `ticks` ticks with the key down on tick `pressAt` and after (never when negative), returning the tick (1-based) the intro was found over on, or 0.
int RunUntilOver(MagicPortalsLayer& layer, entt::registry& registry, int pressAt, bool mouse = false, int limit = 300) {
    for (int tick = 1; tick <= limit; ++tick) {
        const bool down = pressAt >= 0 && tick > pressAt; // a press on tick pressAt+1: the first look is tick 1, so "pressAt" ticks of rest come first
        liveTick(layer, registry, !mouse && down, mouse && down);
        (void)frame(layer, registry);
        if (!layer.SplashUp()) return tick;
    }
    return 0;
}

void TheIntroStandsForItsTimelineWithTheGameNotTicking() {
    restDevices();
    entt::registry registry;
    publishViewport(registry);
    MagicPortalsLayer layer(IntroPaths(true), "level0");
    layer.OnAttach(registry);
    CHECK_MSG(layer.LoadError().empty() && layer.SimLevel() != nullptr, layer.LoadError());
    CHECK_MSG(layer.SplashUp(), "the intro is up from the attach");
    for (int tick = 1; tick <= static_cast<int>(Splash::kTotalTicks); ++tick) {
        liveTick(layer, registry);
        const auto quads = frame(layer, registry);
        const float alpha = Splash::Alpha(static_cast<unsigned>(tick));
        bool ok = false;
        CHECK_MSG(IsTheIntroFrame(quads, alpha, ok), "tick " + std::to_string(tick) + ": the ground and the logo at " + std::to_string(alpha) + " and nothing else (" +
                                                           std::to_string(quads.size()) + " quads)");
        CHECK_MSG(layer.LevelStepCount() == 0u, "tick " + std::to_string(tick) + ": the game has not ticked");
        if (tick < static_cast<int>(Splash::kTotalTicks)) CHECK_MSG(layer.SplashUp(), "tick " + std::to_string(tick) + ": still up");
    }
    CHECK_EQ(layer.SplashTicks(), Splash::kTotalTicks);
    // The 121st tick is the game's first.
    liveTick(layer, registry);
    CHECK_MSG(!layer.SplashUp(), "the 121st tick ends it");
    const auto quads = frame(layer, registry);
    bool ok = false;
    CHECK_MSG(!IsTheIntroFrame(quads, 0.0f, ok) || quads.size() != 1, "and the frame after it is the game's, not the ground alone");
    layer.OnDetach(registry);
}

void APressBeforeTickEighteenChangesNothingAndOneAtThirtyEndsItAtFiftyFour() {
    {
        restDevices();
        entt::registry registry;
        publishViewport(registry);
        MagicPortalsLayer layer(IntroPaths(true), "level0");
        layer.OnAttach(registry);
        // Rest for 5 ticks, then the key down from tick 6: a press on tick 6, before tick 18.
        const int over = RunUntilOver(layer, registry, 5);
        CHECK_MSG(over == static_cast<int>(Splash::kTotalTicks) + 1, "a press on tick 6 is ignored: the intro is over on tick " + std::to_string(over));
        layer.OnDetach(registry);
    }
    {
        restDevices();
        entt::registry registry;
        publishViewport(registry);
        MagicPortalsLayer layer(IntroPaths(true), "level0");
        layer.OnAttach(registry);
        // The key goes down on tick 31 (the clock's tick 30): the fade-out is 24 ticks, the intro's last is tick 54 and the game's first the tick after.
        const int over = RunUntilOver(layer, registry, 30);
        CHECK_MSG(over == 55, "a press on tick 31 (the clock's 30) fades out over 24 ticks, ends on tick 54 and is over on tick 55: " + std::to_string(over));
        layer.OnDetach(registry);
    }
    {
        restDevices();
        entt::registry registry;
        publishViewport(registry);
        MagicPortalsLayer layer(IntroPaths(true), "level0");
        layer.OnAttach(registry);
        const int over = RunUntilOver(layer, registry, 30, /*mouse*/ true);
        CHECK_MSG(over == 55, "a mouse click skips like a key: over on tick " + std::to_string(over));
        layer.OnDetach(registry);
    }
}

void AKeyHeldFromTheFirstLookIsNoPress() {
    restDevices();
    entt::registry registry;
    publishViewport(registry);
    MagicPortalsLayer layer(IntroPaths(true), "level0");
    layer.OnAttach(registry);
    int over = 0;
    for (int tick = 1; tick <= 300 && over == 0; ++tick) {
        liveTick(layer, registry, true); // down from the first tick and never let go: the click that started the game, still held
        (void)frame(layer, registry);
        if (!layer.SplashUp()) over = tick;
    }
    CHECK_MSG(over == static_cast<int>(Splash::kTotalTicks) + 1, "a key held from the first look never skips: over on tick " + std::to_string(over));
    layer.OnDetach(registry);
}

void ALayerWithoutTheOptionHasNoIntroAndTicksFromItsFirstTick() {
    restDevices();
    entt::registry registry;
    publishViewport(registry);
    MagicPortalsLayer layer(IntroPaths(false), "level0");
    layer.OnAttach(registry);
    CHECK_MSG(!layer.SplashUp(), "no option, no intro");
    liveTick(layer, registry);
    const auto quads = frame(layer, registry);
    bool ok = false;
    CHECK_MSG(!IsTheIntroFrame(quads, 0.0f, ok), "and the first frame is the game's");
    CHECK_MSG(layer.LevelStepCount() > 0u, "which has ticked: step count " + std::to_string(layer.LevelStepCount()));
    layer.OnDetach(registry);
}

void ADataFolderWithoutTheLogoIsNoIntro() {
    restDevices();
    entt::registry registry;
    publishViewport(registry);
    MagicPortalsLayer::Paths paths = IntroPaths(true);
    const std::filesystem::path empty = std::filesystem::temp_directory_path() / "supersonic-test-mp-splashflow-nologo";
    std::error_code ec;
    std::filesystem::remove_all(empty, ec);
    std::filesystem::copy(std::filesystem::path(MAGICPORTALS_PORT_DATA_DIR), empty, std::filesystem::copy_options::recursive, ec);
    std::filesystem::remove(empty / Splash::kLogoFile, ec);
    paths.portData = empty.string();
    MagicPortalsLayer layer(paths, "level0");
    layer.OnAttach(registry);
    CHECK_MSG(!layer.SplashUp(), "a port data folder without the logo starts without the intro");
    liveTick(layer, registry);
    CHECK_MSG(layer.LevelStepCount() > 0u, "and the game ticks");
    layer.OnDetach(registry);
    std::filesystem::remove_all(empty, ec);
}

void runTests() {
    TheIntroStandsForItsTimelineWithTheGameNotTicking();
    APressBeforeTickEighteenChangesNothingAndOneAtThirtyEndsItAtFiftyFour();
    AKeyHeldFromTheFirstLookIsNoPress();
    ALayerWithoutTheOptionHasNoIntroAndTicksFromItsFirstTick();
    ADataFolderWithoutTheLogoIsNoIntro();
}

} // namespace

MP_SUITE_MAIN("test_mp_splashflow") {
    std::error_code ec;
    if (!std::filesystem::is_directory(kLevels, ec) || !std::filesystem::is_regular_file(kChapters, ec) ||
        !std::filesystem::is_regular_file(kData + "/entity_roles.json", ec)) {
        std::printf("test_mp_splashflow: SKIPPED - needs the converted levels at %s, chapters.json at %s and Enhanced's data at %s\n", kLevels.c_str(), kChapters.c_str(),
                    kData.c_str());
        return 77;
    }
    runTests();
    return ::test::summary("test_mp_splashflow", 20);
}
