// WHERE A PICTURE IS DRAWN BETWEEN TICKS (planning step 150, program wave 3a, PIC): the
// original's, which is the tick's own place.
//
// The original draws every entity where its frame left it: SetPositionXY is a b2Body::
// SetTransform inside Update and the draw reads GetPosition at draw time (ETHRenderEntity.cpp:106;
// D10_check section 4a), and there is no interpolation. The engine draws a body between its last
// two ticks (InterpolationSystem::Apply), which at alpha 0 - every --fixed-step frame - is where the
// body stood a tick ago, 16.7 ms late, while everything the layer places from the sim's own point
// (lights, emitters, effects an owner carries, the HUD) stands a tick ahead of it. The layer ends
// every tick by publishing the tick's end as both ends of the interpolation
// (MagicPortalsLayer::drawPicturesAtTick), the camera included, so Apply returns the tick's own
// place at any alpha - and keeps the engine's smoothing only where the display outruns the tick
// (game/data/pictures.json). What is pinned:
//
//   the rules    pictures.json: the line, its band, the window and the stall; what a file that is
//                no band, no window or no number is refused for.
//   the watch    the frames' rate decides: a steady 60 Hz (59.94, 75) never smooths, 90, 120, 144
//                and 240 do within a handful of frames and hold through a stall; the band holds a
//                display near the line, and a rate that falls under its floor lets go; a line of 0
//                never smooths.
//   the walk     the player's picture stands on its body - the sim's own transform, which the engine
//                never interpolates - at alpha 0, a quarter, a half and 1: the offset is constant
//                (the lag is 0 ticks). With the engine's smoothing on, the lag is 1 - alpha ticks:
//                the control that shows the measurement could have read it.
//   the camera   the drawn camera is the follow's centre at every alpha, and in smoothing mode lags.
//   --fixed-step the layer told it is one measures nothing and never smooths.
//   every level  at every tick and every alpha, every interpolated picture's drawn transform is
//                the tick's own, to the bit, and the camera's pose too; the pictures that moved
//                that tick are counted so the check cannot pass over a still level. By default a
//                sample of levels (a walking level, a turret, minions, a knight, a weightless room,
//                the bosses, a lift); with --sweep <file> all 128, each for 422 ticks, and the
//                table of what moved on ticks 418 to 422 written to <file> (the f420 sweep's
//                explanation).
//
// No window and no Vulkan, the layer ticked as the app ticks it. The level parts need the converted
// levels and skip themselves without them; the pure parts always run.

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "MagicPortalsLayer.hpp"

#include "core/Components.hpp"
#include "core/Input.hpp"
#include "core/InterpolationSystem.hpp"
#include "core/PhysicsSystem.hpp"
#include "core/ViewportInfo.hpp"

#include "sim/Chapters.hpp"
#include "sim/Pictures.hpp"
#include "sim/Units.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

using namespace Supersonic;
using MagicPortals::MagicPortalsLayer;
namespace Pictures = MagicPortals::Pictures;
namespace Chapters = MagicPortals::Chapters;
namespace Units = MagicPortals::Units;

namespace {

const std::string kPortData = MAGICPORTALS_PORT_DATA_DIR;
const std::string kLevels = MAGICPORTALS_LEVELS_DIR;
const std::string kChapters = MAGICPORTALS_CHAPTERS_FILE;
const std::string kData = MAGICPORTALS_DATA_DIR;
const glm::vec2 kRest(640.0f, 360.0f);
constexpr double kTickS = 1.0 / 60.0;

// Doubles, which the harness's CHECK_NEAR (floats) would narrow. Both sides are plain reads.
#define CHECK_NEARD(a, b) CHECK_MSG(std::fabs((a) - (b)) < 1e-9, std::string(#a) + " is " + std::to_string(a) + ", expected " + std::to_string(b))

// ---- the rules and the watch --------------------------------------------------------

Pictures::Rules ReadRules() {
    Pictures::Rules rules;
    std::string error;
    CHECK_MSG(Pictures::LoadRules(kPortData + "/pictures.json", rules, error), error);
    return rules;
}

// A pictures.json the loader is to refuse or take, written to a scratch file.
bool Load(const std::string& text, Pictures::Rules& out, std::string& error) {
    const std::filesystem::path path = std::filesystem::temp_directory_path() / "supersonic-test-mp-pictures.json";
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file << text;
    }
    const bool ok = Pictures::LoadRules(path.string(), out, error);
    std::error_code ec;
    std::filesystem::remove(path, ec);
    return ok;
}

std::string RulesText(const std::string& above, const std::string& release, const std::string& window,
                      const std::string& hitch) {
    return "{\"smoothing\": {\"above_hz\": " + above + ", \"release_below_hz\": " + release +
           ", \"window_frames\": " + window + ", \"hitch_ticks\": " + hitch + "}}";
}

void ThePicturesFileIsRead() {
    const Pictures::Rules rules = ReadRules();
    // The port's own choice, marked _guess in the file: smoothing is kept above 80 Hz and let go under 72.
    CHECK_NEARD(rules.aboveHz, 80.0);
    CHECK_NEARD(rules.releaseBelowHz, 72.0);
    CHECK_EQ(rules.windowFrames, 30);
    CHECK_NEARD(rules.hitchTicks, 2.0);
    // The 60 Hz the port is matched at is under the band whatever the file says.
    CHECK(rules.releaseBelowHz > 60.0);

    Pictures::Rules read;
    std::string error;
    CHECK_MSG(Load(RulesText("80", "72", "30", "2"), read, error), error);
    CHECK_NEARD(read.aboveHz, 80.0);
    // 0 is the original's own, never smoothing: taken.
    CHECK_MSG(Load(RulesText("0", "0", "1", "1.5"), read, error), error);
    CHECK_NEARD(read.aboveHz, 0.0);
    // And what is refused, each with the key it names.
    const std::vector<std::pair<std::string, std::string>> refused = {
        {RulesText("-1", "0", "30", "2"), "above_hz"},
        {RulesText("80", "90", "30", "2"), "release_below_hz"},
        {RulesText("80", "-2", "30", "2"), "release_below_hz"},
        {RulesText("80", "72", "0", "2"), "window_frames"},
        {RulesText("80", "72", "2.5", "2"), "window_frames"},
        {RulesText("80", "72", "30", "1"), "hitch_ticks"},
        {"{\"smoothing\": {\"above_hz\": 80}}", "are numbers"},
        {"{}", "are numbers"},
        {"not json", ""},
    };
    for (const auto& [text, key] : refused) {
        Pictures::Rules untouched;
        untouched.aboveHz = 33.0;
        const bool ok = Load(text, untouched, error);
        CHECK_MSG(!ok, "refused: " + text);
        CHECK_MSG(untouched.aboveHz == 33.0, "a refused file leaves the rules as they were");
        CHECK_MSG(key.empty() || error.find(key) != std::string::npos, key + " is named: " + error);
    }
    {
        Pictures::Rules none;
        CHECK_MSG(!Pictures::LoadRules(kPortData + "/no_such_pictures.json", none, error), "a missing file is refused");
    }
}

// Frames at `hz` fed to a watch until it smooths: how many it took, or -1 over `limit`.
int FramesToSmooth(const Pictures::Rules& rules, Pictures::RateWatch& watch, double hz, int limit) {
    for (int frame = 1; frame <= limit; ++frame) {
        watch.Frame(rules, 1.0 / hz, kTickS);
        if (watch.Smoothing()) return frame;
    }
    return -1;
}

void TheWatchSmoothsOnlyWhereTheDisplayOutrunsTheTick() {
    const Pictures::Rules rules = ReadRules();
    // The matched 60 Hz, a display a little under it and a 75 Hz one: the original's own, the tick's place.
    for (const double hz : {60.0, 59.94, 30.0, 75.0, 79.0}) {
        Pictures::RateWatch watch;
        CHECK_MSG(FramesToSmooth(rules, watch, hz, 3000) == -1, "no smoothing at " + std::to_string(hz) + " Hz");
        CHECK_MSG(std::fabs(watch.Hz() - hz) < 1.0, "the mean reads the rate: " + std::to_string(watch.Hz()));
    }
    // 90, 120, 144 and 240 Hz: smoothed, and within the mean's own window (30 frames, the seed at 60 Hz).
    int previous = 1 << 30;
    for (const double hz : {90.0, 120.0, 144.0, 240.0}) {
        Pictures::RateWatch watch;
        const int took = FramesToSmooth(rules, watch, hz, 3000);
        CHECK_MSG(took > 0 && took < 100, "smoothing at " + std::to_string(hz) + " Hz in " + std::to_string(took));
        CHECK_MSG(took <= previous, "a faster display crosses the line no later");
        previous = took;
        // And holds through the long run, and through a stall (a load, a window drag) of a dozen ticks.
        for (int frame = 0; frame < 500; ++frame) watch.Frame(rules, 1.0 / hz, kTickS);
        CHECK(watch.Smoothing());
        const double hzBefore = watch.Hz();
        watch.Frame(rules, 12.0 * kTickS, kTickS);
        CHECK_MSG(watch.Hz() == hzBefore, "a stall says nothing of the display");
        CHECK(watch.Smoothing());
        // Non-positive frames are nothing either.
        watch.Frame(rules, 0.0, kTickS);
        watch.Frame(rules, -1.0, kTickS);
        CHECK(watch.Hz() == hzBefore);
    }
    // THE BAND: engaged at 144, a display at 76 Hz is over the floor and stays smoothed, one at 70 lets go -
    // and the 76 Hz run that never was engaged never engages, so the line does not flip at the edge.
    {
        Pictures::RateWatch watch;
        FramesToSmooth(rules, watch, 144.0, 500);
        for (int frame = 0; frame < 1000; ++frame) watch.Frame(rules, 1.0 / 76.0, kTickS);
        CHECK_MSG(watch.Smoothing(), "76 Hz is inside the band: kept");
        for (int frame = 0; frame < 1000; ++frame) watch.Frame(rules, 1.0 / 70.0, kTickS);
        CHECK_MSG(!watch.Smoothing(), "70 Hz is under its floor: let go");
        // And back to the matched rate from there: still not smoothing, and 144 again engages again.
        for (int frame = 0; frame < 200; ++frame) watch.Frame(rules, kTickS, kTickS);
        CHECK(!watch.Smoothing());
        CHECK(FramesToSmooth(rules, watch, 144.0, 500) > 0);
        Pictures::RateWatch edge;
        for (int frame = 0; frame < 1000; ++frame) edge.Frame(rules, 1.0 / 76.0, kTickS);
        CHECK_MSG(!edge.Smoothing(), "76 Hz from the start is under the line: not engaged");
    }
    // A line of 0 is the original's at every rate, and lets go of a watch that had engaged.
    {
        Pictures::Rules never = rules;
        never.aboveHz = 0.0;
        never.releaseBelowHz = 0.0;
        Pictures::RateWatch watch;
        CHECK(FramesToSmooth(never, watch, 240.0, 1000) == -1);
        Pictures::RateWatch engaged;
        FramesToSmooth(rules, engaged, 240.0, 500);
        CHECK(engaged.Smoothing());
        engaged.Frame(never, 1.0 / 240.0, kTickS);
        CHECK_MSG(!engaged.Smoothing(), "a switch turned off lets go at once");
    }
    // The pure suites' default is the original's: no line, no smoothing.
    {
        const Pictures::Rules bare;
        Pictures::RateWatch watch;
        CHECK(FramesToSmooth(bare, watch, 240.0, 1000) == -1);
    }
}

// ---- the layer, ticked as the app ticks it --------------------------------------------

MagicPortalsLayer::Paths TestPaths(bool fixedStep = false) {
    MagicPortalsLayer::Paths paths;
    paths.prisms = std::filesystem::temp_directory_path() / "supersonic-test-mp-pictures";
    paths.fixedStep = fixedStep;
    return paths;
}

void publishViewport(entt::registry& registry) {
    ViewportInfo viewport;
    viewport.rect.min = glm::vec2(0.0f, 0.0f);
    viewport.rect.max = glm::vec2(1280.0f, 720.0f);
    viewport.pointerOverGame = true;
    registry.ctx().insert_or_assign(viewport);
}

entt::entity primaryCamera(entt::registry& registry) {
    for (auto [entity, camera] : registry.view<CameraComponent>().each()) {
        if (camera.isPrimary) return entity;
    }
    return entt::null;
}

entt::entity taggedOnce(entt::registry& registry, const char* tag) {
    entt::entity found = entt::null;
    for (auto [entity, t] : registry.view<TagComponent>().each()) {
        if (t.tag == tag) found = entity;
    }
    return found;
}

// One tick as the app runs it: BeginTick, the physics step, the layer with this tick's input, EndTick.
void tickWith(MagicPortalsLayer& layer, entt::registry& registry, std::vector<std::string> down,
              std::vector<std::string> pressed) {
    InterpolationSystem::BeginTick(registry);
    PhysicsSystem::Update(registry, MagicPortalsLayer::kTick);
    Input::TickInput input;
    input.mousePosition = kRest;
    input.down = std::move(down);
    input.pressed = std::move(pressed);
    Input::BeginReplayedTick(input);
    layer.OnFixedUpdate(registry, MagicPortalsLayer::kTick);
    Input::EndReplayedTick();
    InterpolationSystem::EndTick(registry);
}

bool LevelsAreThere(const char* what) {
    std::error_code ec;
    if (std::filesystem::is_directory(kLevels, ec) && std::filesystem::is_regular_file(kChapters, ec) &&
        std::filesystem::is_regular_file(kData + "/entity_roles.json", ec) &&
        std::filesystem::is_regular_file(kData + "/player.json", ec) &&
        std::filesystem::is_regular_file(kData + "/portals.json", ec)) {
        return true;
    }
    std::printf("  %s SKIPPED - needs the converted levels at %s, chapters.json at %s and Enhanced's data at %s\n",
                what, kLevels.c_str(), kChapters.c_str(), kData.c_str());
    return false;
}

// What one walk measured, in the level's metres (50 px each) and in ticks.
struct Walk {
    int ticksMoving = 0;           // ticks the body moved by at least a pixel
    double worstDriftM = 0.0;      // the largest |picture - body - its first offset|, over every frame
    // The mean lag in ticks at each alpha: (body now - where the picture stands) / (body's move this tick).
    std::map<int, double> lagAtAlphaPercent;
    std::map<int, int> samples;
    double worstCameraDriftM = 0.0; // the drawn camera against the follow's centre, over every frame
    double cameraLagAtZero = 0.0;   // the camera's lag in ticks at alpha 0, the mean over ticks it moved
    int cameraTicksMoving = 0;
    bool smoothedAtTheEnd = false;
    double frameHz = 0.0;
    // The player's red eyes - an effect that follows the player's BODY, as an owner's effect does, and is placed
    // each frame from the sim's own point - against the picture the player stands drawn as: the largest change of
    // (eyes - picture), in the level's pixels, over every frame of the steady walk after the first.
    bool eyesFound = false;
    double eyesDriftPx = 0.0;
};

// 1-31 (level30) walked right for `ticks` ticks, drawn as the app draws: after each tick, five frames at
// alpha 0, a quarter, a half, three quarters and 1. `frameS` is what OnUpdate is handed for each frame: the
// tick's own length is a 60 Hz display, a fifth of it a 300 Hz one that the layer measures and then smooths.
Walk WalkLevel30(double frameS, bool fixedStep) {
    Walk walk;
    entt::registry registry;
    publishViewport(registry);
    MagicPortalsLayer layer(TestPaths(fixedStep), "level30");
    layer.OnAttach(registry);
    if (!layer.LoadError().empty() || layer.SimLevel() == nullptr) {
        CHECK_MSG(false, layer.LoadError());
        return walk;
    }
    const entt::entity body = layer.SimLevel()->player;
    entt::entity quad = taggedOnce(registry, "Magic Portals Player Sprite");
    if (quad == entt::null) quad = taggedOnce(registry, "Magic Portals Player"); // no art: its box is the picture
    const entt::entity camera = primaryCamera(registry);
    CHECK(body != entt::null && quad != entt::null && camera != entt::null);
    if (body == entt::null || quad == entt::null || camera == entt::null) return walk;

    glm::vec3 reference(0.0f);
    bool haveReference = false;
    glm::dvec2 eyesReference(0.0);
    bool haveEyesReference = false;
    glm::vec3 bodyBefore = registry.get<TransformComponent>(body).position;
    glm::vec3 cameraBefore = registry.get<CameraComponent>(camera).position;
    constexpr int kAlphas = 5;
    const int ticks = 150;
    for (int tick = 0; tick < ticks; ++tick) {
        tickWith(layer, registry, {MagicPortalsLayer::kRight},
                 tick == 0 ? std::vector<std::string>{MagicPortalsLayer::kRight} : std::vector<std::string>{});
        const glm::vec3 bodyNow = registry.get<TransformComponent>(body).position;
        const float moved = bodyNow.x - bodyBefore.x;
        const glm::vec3 followNow = Units::ToWorld(layer.CameraCentrePx().x, layer.CameraCentrePx().y);
        const float cameraMoved = followNow.x - cameraBefore.x;
        bodyBefore = bodyNow;
        cameraBefore = glm::vec3(followNow.x, followNow.y, cameraBefore.z);
        for (int f = 0; f < kAlphas; ++f) {
            const float alpha = static_cast<float>(f) / static_cast<float>(kAlphas - 1);
            InterpolationSystem::Apply(registry, alpha);
            layer.OnUpdate(registry, static_cast<float>(frameS));
            // OnUpdate may shake the camera or move effects, never a body picture: read after it, as drawn.
            const glm::vec3 picture = registry.get<TransformComponent>(quad).position;
            if (!haveReference && f == kAlphas - 1) {
                reference = picture - bodyNow;
                haveReference = true;
            }
            if (!haveReference) continue;
            // The eyes, as this frame placed them (OnUpdate's updateEffects), against the picture's own point.
            for (const MagicPortalsLayer::EffectReport& report : layer.EffectReports()) {
                if (report.followKind != MagicPortalsLayer::EffectOwner::Kind::Player) continue;
                walk.eyesFound = true;
                const glm::dvec2 offsetPx = report.atPx - Units::ToPixels(picture);
                if (!haveEyesReference && tick >= 10 && f == kAlphas - 1) {
                    eyesReference = offsetPx;
                    haveEyesReference = true;
                }
                if (haveEyesReference) {
                    walk.eyesDriftPx = std::max(walk.eyesDriftPx, glm::length(offsetPx - eyesReference));
                }
                break;
            }
            const glm::vec3 offset = picture - bodyNow;
            const double drift = std::max(std::fabs(offset.x - reference.x), std::fabs(offset.y - reference.y));
            const int percent = static_cast<int>(std::lround(alpha * 100.0f));
            // The lag is read where the walk is steady: the body moved a pixel or more, every frame of it.
            if (std::fabs(moved) >= 0.02f) {
                const double lag = (bodyNow.x - (picture.x - reference.x)) / moved;
                walk.lagAtAlphaPercent[percent] += lag;
                ++walk.samples[percent];
                walk.worstDriftM = std::max(walk.worstDriftM, drift);
            } else if (f == kAlphas - 1) {
                // A body at rest: the picture stands on it at 1 whatever the mode.
                walk.worstDriftM = std::max(walk.worstDriftM, drift);
            }
            const glm::vec3 drawn = registry.get<CameraComponent>(camera).position;
            if (f == 0 && std::fabs(cameraMoved) >= 0.002f) {
                walk.cameraLagAtZero += (followNow.x - drawn.x) / cameraMoved;
                ++walk.cameraTicksMoving;
            }
            if (f == kAlphas - 1) {
                walk.worstCameraDriftM = std::max(
                    walk.worstCameraDriftM, static_cast<double>(std::fabs(drawn.x - followNow.x)));
            }
        }
        if (std::fabs(moved) >= 0.02f) ++walk.ticksMoving;
        if (layer.MenuScreen() != MagicPortalsLayer::Screen::None) break;
    }
    for (auto& [percent, sum] : walk.lagAtAlphaPercent) sum /= static_cast<double>(std::max(1, walk.samples[percent]));
    if (walk.cameraTicksMoving > 0) walk.cameraLagAtZero /= static_cast<double>(walk.cameraTicksMoving);
    walk.smoothedAtTheEnd = layer.PicturesSmoothed();
    walk.frameHz = layer.FrameHz();
    layer.OnDetach(registry);
    return walk;
}

void ABodysPictureStandsOnItsBodyAtEveryAlpha() {
    if (!LevelsAreThere("ABodysPictureStandsOnItsBodyAtEveryAlpha")) return;
    // A 60 Hz display: the frames are the tick's own length, and the layer draws every picture at the tick's
    // place - at alpha 0, a quarter, a half and 1 alike. The lag - how many ticks the picture trails the body
    // it hangs on, the sim's own transform the engine never interpolates - is 0, step 78's measurement taken again.
    const Walk steady = WalkLevel30(kTickS, false);
    std::printf("  level30 at 60 Hz: the player moved on %d of 150 ticks; worst drift %.7f m; lag by alpha 0 / 25 / 50 / "
                "75 / 100 %%: %.4f / %.4f / %.4f / %.4f / %.4f ticks; the camera's lag at alpha 0 %.4f ticks over %d "
                "ticks, worst camera drift %.7f m; the watch reads %.1f Hz, smoothing %s\n",
                steady.ticksMoving, steady.worstDriftM, steady.lagAtAlphaPercent.count(0) ? steady.lagAtAlphaPercent.at(0) : -9.0,
                steady.lagAtAlphaPercent.count(25) ? steady.lagAtAlphaPercent.at(25) : -9.0,
                steady.lagAtAlphaPercent.count(50) ? steady.lagAtAlphaPercent.at(50) : -9.0,
                steady.lagAtAlphaPercent.count(75) ? steady.lagAtAlphaPercent.at(75) : -9.0,
                steady.lagAtAlphaPercent.count(100) ? steady.lagAtAlphaPercent.at(100) : -9.0, steady.cameraLagAtZero,
                steady.cameraTicksMoving, steady.worstCameraDriftM, steady.frameHz, steady.smoothedAtTheEnd ? "on" : "off");
    CHECK_MSG(steady.ticksMoving >= 60, "the player walked: " + std::to_string(steady.ticksMoving) + " ticks");
    CHECK_MSG(steady.worstDriftM < 1e-6, "the picture is the body plus its one offset: " + std::to_string(steady.worstDriftM));
    for (const int percent : {0, 25, 50, 75, 100}) {
        CHECK_MSG(steady.samples.count(percent) && steady.samples.at(percent) >= 60,
                  "frames read at alpha " + std::to_string(percent));
        if (!steady.lagAtAlphaPercent.count(percent)) continue;
        CHECK_MSG(std::fabs(steady.lagAtAlphaPercent.at(percent)) < 1e-4,
                  "lag 0 ticks at alpha " + std::to_string(percent) + ": " +
                      std::to_string(steady.lagAtAlphaPercent.at(percent)));
    }
    CHECK_MSG(steady.cameraTicksMoving >= 20, "the camera followed: " + std::to_string(steady.cameraTicksMoving) + " ticks");
    CHECK_MSG(std::fabs(steady.cameraLagAtZero) < 1e-4, "the camera is drawn where the follow left it: lag " +
                                                           std::to_string(steady.cameraLagAtZero));
    CHECK_MSG(steady.worstCameraDriftM < 1e-6, "and at alpha 1: " + std::to_string(steady.worstCameraDriftM));
    CHECK_MSG(!steady.smoothedAtTheEnd && std::fabs(steady.frameHz - 60.0) < 1.0, "60 Hz frames do not smooth");
    // THE EFFECT AND THE PICTURE AGREE, as the original's do (one entity point, read the same frame by the draw and by
    // the particle system): the eyes the player carries are placed from the sim's point, and the picture stands there.
    CHECK_MSG(steady.eyesFound, "the player carries its red eyes");
    CHECK_MSG(steady.eyesDriftPx < 1e-4, "the eyes keep their place on the picture at every alpha: " +
                                             std::to_string(steady.eyesDriftPx) + " px");

    // THE CONTROL: a 300 Hz display (five frames to a tick) is measured, and the engine's smoothing kept - the
    // picture then trails the body by 1 - alpha ticks, so the walk above could have read a lag had there been one.
    const Walk smooth = WalkLevel30(kTickS / 5.0, false);
    std::printf("  level30 at 300 Hz: lag by alpha 0 / 25 / 50 / 75 / 100 %%: %.4f / %.4f / %.4f / %.4f / %.4f ticks; "
                "the camera's lag at alpha 0 %.4f; the watch reads %.1f Hz, smoothing %s\n",
                smooth.lagAtAlphaPercent.count(0) ? smooth.lagAtAlphaPercent.at(0) : -9.0,
                smooth.lagAtAlphaPercent.count(25) ? smooth.lagAtAlphaPercent.at(25) : -9.0,
                smooth.lagAtAlphaPercent.count(50) ? smooth.lagAtAlphaPercent.at(50) : -9.0,
                smooth.lagAtAlphaPercent.count(75) ? smooth.lagAtAlphaPercent.at(75) : -9.0,
                smooth.lagAtAlphaPercent.count(100) ? smooth.lagAtAlphaPercent.at(100) : -9.0, smooth.cameraLagAtZero,
                smooth.frameHz, smooth.smoothedAtTheEnd ? "on" : "off");
    CHECK_MSG(smooth.smoothedAtTheEnd && std::fabs(smooth.frameHz - 300.0) < 3.0, "300 Hz frames smooth");
    CHECK_MSG(smooth.ticksMoving >= 60, "the player walked on the smoothing display too");
    // Over the whole run the mean carries the first ticks, drawn at the tick's place while the watch converged
    // (about 20 frames: four ticks of 150), so the readings are the smoothing's within that share.
    for (const int percent : {0, 25, 50, 75}) {
        CHECK_MSG(smooth.lagAtAlphaPercent.count(percent) != 0, "frames read at alpha " + std::to_string(percent));
        if (!smooth.lagAtAlphaPercent.count(percent)) continue;
        const double want = 1.0 - static_cast<double>(percent) / 100.0;
        CHECK_MSG(std::fabs(smooth.lagAtAlphaPercent.at(percent) - want) < 0.1,
                  "smoothed lag at alpha " + std::to_string(percent) + " is " + std::to_string(want) + ": " +
                      std::to_string(smooth.lagAtAlphaPercent.at(percent)));
    }
    CHECK_MSG(smooth.cameraLagAtZero > 0.8 && smooth.cameraLagAtZero < 1.05,
              "the smoothed camera trails at alpha 0: " + std::to_string(smooth.cameraLagAtZero));
    // And the price of keeping it, as pictures.json says: the eyes read the sim, so they lead the smoothed picture
    // by up to a tick of the walk (144 u/s: 2.4 px), where the steady display's agree to the bit.
    std::printf("  level30 eyes against the picture: steady %.6f px, smoothed %.4f px\n", steady.eyesDriftPx, smooth.eyesDriftPx);
    CHECK_MSG(smooth.eyesDriftPx > 1.0 && smooth.eyesDriftPx < 3.0,
              "the eyes lead a smoothed picture by under a tick of walking: " + std::to_string(smooth.eyesDriftPx));
    // A --fixed-step run, told so, measures nothing: the same 300 Hz frames never smooth, and every picture
    // stands on its body.
    const Walk fixed = WalkLevel30(kTickS / 5.0, true);
    CHECK_MSG(!fixed.smoothedAtTheEnd && fixed.frameHz == 0.0, "a --fixed-step layer measures nothing");
    CHECK_MSG(fixed.worstDriftM < 1e-6, "and draws every picture at the tick's place: " + std::to_string(fixed.worstDriftM));
    CHECK_MSG(fixed.eyesDriftPx < 1e-4, "its eyes with them: " + std::to_string(fixed.eyesDriftPx));
    for (const auto& [percent, lag] : fixed.lagAtAlphaPercent) {
        CHECK_MSG(std::fabs(lag) < 1e-4, "fixed-step lag 0 at alpha " + std::to_string(percent));
    }
}

// ---- every picture of every level ---------------------------------------------------

struct Mover {
    std::string tag;
    double px = 0.0;
};

// What one tick changed: the pictures that stood somewhere else, turned differently or were another size at its end than at
// its start - how far the farthest edge of each moved, in the level's units - the largest of them, and the camera.
struct TickStat {
    int moved = 0;
    double cameraPx = 0.0;
    Mover largest;
    // Of those that moved, the ones whose picture is on the view at the tick's end (the camera's own rectangle, the
    // quad's box, shown): what a capture of that tick can have changed in.
    int movedOnScreen = 0;
    Mover largestOnScreen;
};

// What a picture stood as at a tick's start, and how far its farthest edge has gone from that at the tick's end: its centre's
// move, its turn about the centre (the angle times the half-size), or its growth (half the change of size), the largest of the
// three, in the level's units. The space sky's spiral turns 1.7 degrees a second about a centre that never moves: it is an edge
// move, and a position alone would not see it.
struct Stood {
    glm::vec3 position{0.0f};
    glm::vec3 rotation{0.0f};
    glm::vec3 scale{1.0f};
};

double EdgeMoveUnits(const Stood& was, const TransformComponent& now) {
    const double centre = glm::length(glm::vec2(now.position.x - was.position.x, now.position.y - was.position.y));
    const double half = 0.5 * std::max(std::fabs(now.scale.x), std::fabs(now.scale.y));
    constexpr double kTau = 6.283185307179586;
    double turn = std::fmod(std::fabs(static_cast<double>(now.rotation.z) - static_cast<double>(was.rotation.z)), kTau);
    if (turn > kTau * 0.5) turn = kTau - turn;
    const double grow =
        0.5 * std::max(std::fabs(now.scale.x - was.scale.x), std::fabs(now.scale.y - was.scale.y));
    return std::max({centre, turn * half, grow}) * Units::kPixelsPerMetre;
}

struct LevelRun {
    int ticks = 0;
    std::map<int, TickStat> window; // the ticks asked for (keepFrom..keepTo), by tick number from 1
    bool smoothed = false;     // the layer ended the run keeping the engine's smoothing
    int picturesSeen = 0;      // interpolated pictures, over the ticks (the population)
    int mismatches = 0;        // a picture or the camera off its tick's place at some alpha
    int movedTicks = 0;        // picture-moves counted (a picture that moved on a tick, summed)
    std::string firstMismatch;
    // At the last tick:
    int movedLast = 0;
    double cameraLastPx = 0.0;
    Mover largestLast;
    std::map<std::string, int> movedByTag;
};

// Runs `name` for `ticks` ticks with no input - what a --fixed-step capture of the level feeds it - and holds
// every interpolated picture to its tick's own place at alpha 0, 0.37 and 1 after each. `lastTick` is the tick
// whose displacements are kept (the f420 sweep's).
LevelRun RunLevel(const std::string& name, int ticks, bool fiveFramesATick = false, int keepFrom = 0, int keepTo = -1) {
    LevelRun run;
    entt::registry registry;
    publishViewport(registry);
    MagicPortalsLayer layer(TestPaths(), name);
    layer.OnAttach(registry);
    if (!layer.LoadError().empty() || layer.SimLevel() == nullptr) {
        CHECK_MSG(false, name + ": " + layer.LoadError());
        return run;
    }
    const entt::entity camera = primaryCamera(registry);
    for (int tick = 1; tick <= ticks; ++tick) {
        // What each picture stood at as the tick began: the last tick's end, which BeginTick restores.
        InterpolationSystem::BeginTick(registry);
        std::map<entt::entity, Stood> before;
        for (auto [entity, interpolated, transform] : registry.view<InterpolatedTransformComponent, TransformComponent>().each()) {
            if (interpolated.captured) before[entity] = Stood{transform.position, transform.rotation, transform.scale};
        }
        glm::vec3 cameraBefore(0.0f);
        if (camera != entt::null) cameraBefore = registry.get<CameraComponent>(camera).position;
        // The tick: BeginTick above is the tickWith's own, so only the rest of it is run here.
        PhysicsSystem::Update(registry, MagicPortalsLayer::kTick);
        Input::TickInput input;
        input.mousePosition = kRest;
        Input::BeginReplayedTick(input);
        layer.OnFixedUpdate(registry, MagicPortalsLayer::kTick);
        Input::EndReplayedTick();
        InterpolationSystem::EndTick(registry);

        int moved = 0;
        int movedOnScreen = 0;
        Mover largest;
        Mover largestOnScreen;
        glm::vec2 viewMin(0.0f), viewMax(0.0f);
        if (camera != entt::null) {
            const auto& cam = registry.get<CameraComponent>(camera);
            const float halfHeight = cam.orthoHeight * 0.5f;
            const float halfWidth = halfHeight * cam.aspect;
            viewMin = glm::vec2(cam.position.x - halfWidth, cam.position.y - halfHeight);
            viewMax = glm::vec2(cam.position.x + halfWidth, cam.position.y + halfHeight);
        }
        for (auto [entity, interpolated, transform] : registry.view<InterpolatedTransformComponent, TransformComponent>().each()) {
            ++run.picturesSeen;
            const auto was = before.find(entity);
            if (was == before.end()) continue;
            const double px = EdgeMoveUnits(was->second, transform);
            if (px > 1e-6) {
                ++moved;
                ++run.movedTicks;
                const std::string tag = registry.all_of<TagComponent>(entity) ? registry.get<TagComponent>(entity).tag : "?";
                if (px > largest.px) largest = {tag, px};
                if (tick == ticks) ++run.movedByTag[tag];
                // On the view: the quad's box (one unit on a side, centred, scaled) overlaps the camera's rectangle, and it
                // is shown. A turned picture is taken by its unturned box, a little short of the truth at a corner.
                const auto* renderable = registry.try_get<RenderableComponent>(entity);
                const glm::vec2 half(transform.scale.x * 0.5f, transform.scale.y * 0.5f);
                const bool shown = renderable != nullptr && renderable->isVisible;
                if (shown && transform.position.x + half.x >= viewMin.x && transform.position.x - half.x <= viewMax.x &&
                    transform.position.y + half.y >= viewMin.y && transform.position.y - half.y <= viewMax.y) {
                    ++movedOnScreen;
                    if (px > largestOnScreen.px) largestOnScreen = {tag, px};
                }
            }
        }
        double cameraPx = 0.0;
        if (camera != entt::null) {
            const glm::vec3 now = registry.get<CameraComponent>(camera).position;
            cameraPx = glm::length(glm::vec2(now.x - cameraBefore.x, now.y - cameraBefore.y)) * Units::kPixelsPerMetre;
        }
        if (tick == ticks) {
            run.movedLast = moved;
            run.largestLast = largest;
            run.cameraLastPx = cameraPx;
        }
        if (tick >= keepFrom && tick <= keepTo) {
            run.window[tick] = TickStat{moved, cameraPx, largest, movedOnScreen, largestOnScreen};
        }
        // The frames: each picture's drawn transform is the tick's own, bit for bit.
        std::vector<std::pair<entt::entity, InterpolatedTransformComponent>> ends;
        for (auto [entity, interpolated] : registry.view<InterpolatedTransformComponent>().each()) {
            ends.emplace_back(entity, interpolated);
        }
        for (const float alpha : {0.0f, 0.37f, 1.0f}) {
            InterpolationSystem::Apply(registry, alpha);
            for (const auto& [entity, end] : ends) {
                if (!end.captured) continue;
                const auto& drawn = registry.get<TransformComponent>(entity);
                if (drawn.position != end.currentPosition || drawn.rotation != end.currentRotation ||
                    drawn.scale != end.currentScale) {
                    ++run.mismatches;
                    if (run.firstMismatch.empty()) {
                        run.firstMismatch = name + " tick " + std::to_string(tick) + " alpha " + std::to_string(alpha) + ": " +
                                            (registry.all_of<TagComponent>(entity) ? registry.get<TagComponent>(entity).tag : "?");
                    }
                }
            }
            if (camera != entt::null) {
                const auto& interpolated = registry.get<InterpolatedCameraComponent>(camera);
                const auto& drawn = registry.get<CameraComponent>(camera);
                if (interpolated.captured && (drawn.position != interpolated.current.position ||
                                              drawn.yaw != interpolated.current.yaw || drawn.pitch != interpolated.current.pitch ||
                                              drawn.orthoHeight != interpolated.current.orthoHeight)) {
                    ++run.mismatches;
                    if (run.firstMismatch.empty()) {
                        run.firstMismatch = name + " tick " + std::to_string(tick) + " alpha " + std::to_string(alpha) + ": the camera";
                    }
                }
            }
        }
        ++run.ticks;
        // The finished and lost screens are up over a level that goes on running (the capture's frames go on too), so
        // those two are not the end of the run; a menu is.
        const MagicPortalsLayer::Screen screen = layer.MenuScreen();
        if (screen != MagicPortalsLayer::Screen::None && screen != MagicPortalsLayer::Screen::Finished &&
            screen != MagicPortalsLayer::Screen::Dead) {
            break;
        }
        // A 300 Hz display, for the control: the frames the layer measures, and then keeps the engine's smoothing for.
        if (fiveFramesATick) {
            for (int frame = 0; frame < 5; ++frame) layer.OnUpdate(registry, MagicPortalsLayer::kTick / 5.0f);
        }
    }
    run.smoothed = layer.PicturesSmoothed();
    layer.OnDetach(registry);
    return run;
}

void EveryPictureStandsWhereItsTickLeftIt() {
    if (!LevelsAreThere("EveryPictureStandsWhereItsTickLeftIt")) return;
    // A walking level, 1-31; a carranca and its fireballs, 2-01; a lift, 1-27; a reflector, 2-27; the knights' levels
    // 3-10 and 3-15; a weightless room's hover, 4-02; the bosses, 2-32 (the dragon), 3-32 (the ghost) and 4-32 (the
    // dark dragon).
    const std::vector<std::string> sample = {"level30",  "level0a",  "level26",  "level26a", "level9b",
                                             "level14b", "level1c",  "level31a", "level31b", "level31c"};
    int moving = 0;
    for (const std::string& name : sample) {
        const LevelRun run = RunLevel(name, 480);
        std::printf("  %s: %d ticks, %d picture-ticks, %d moves, %d at tick 480%s%s, camera %.2f px; off the tick's place: %d%s%s\n",
                    name.c_str(), run.ticks, run.picturesSeen, run.movedTicks, run.movedLast,
                    run.largestLast.px > 0.0 ? ", the largest " : "", run.largestLast.tag.c_str(), run.cameraLastPx,
                    run.mismatches, run.firstMismatch.empty() ? "" : " first ", run.firstMismatch.c_str());
        CHECK_MSG(run.ticks >= 100, name + " ran " + std::to_string(run.ticks) + " ticks");
        CHECK_MSG(run.picturesSeen > 100, name + " has pictures to hold: " + std::to_string(run.picturesSeen));
        CHECK_MSG(run.mismatches == 0, name + ": a picture is off its tick's place: " + run.firstMismatch);
        if (run.movedTicks > 0) ++moving;
    }
    // The check passes over nothing only if no level of the sample ever moved a picture.
    CHECK_MSG(moving >= 5, "pictures moved on " + std::to_string(moving) + " of the sample's levels");
    // THE CONTROL: the same check on a 300 Hz display, where the layer keeps the engine's smoothing, finds pictures
    // off their tick's place - so a clean sample above could have found them.
    const LevelRun smooth = RunLevel("level30", 200, true);
    std::printf("  level30 at 300 Hz: smoothing %s, %d picture-frames off the tick's place%s%s\n", smooth.smoothed ? "on" : "off",
                smooth.mismatches, smooth.firstMismatch.empty() ? "" : ", first ", smooth.firstMismatch.c_str());
    CHECK_MSG(smooth.smoothed, "300 Hz is smoothed");
    CHECK_MSG(smooth.mismatches > 100, "and smoothing draws moving pictures off the tick's place: " +
                                           std::to_string(smooth.mismatches));
}

// `--sweep <file>`: all 128 levels for 422 ticks, and what moved on each of ticks 418 to 422 - the f420 sweep's own
// tick is among them - written as a table. Every level holds the same check as the sample's.
void TheFullSweep(const std::string& file) {
    if (!LevelsAreThere("TheFullSweep")) return;
    Chapters::Table table;
    std::string error;
    CHECK_MSG(Chapters::Load(kChapters, table, error), error);
    std::ofstream out(file, std::ios::trunc);
    out << "# PIC (planning step 150): what changed on ticks 418..422 of each level, with no input - the f420 capture's own\n"
           "# tick is among them. A picture changed when its centre moved, it turned or it grew; px is how far its farthest edge went\n"
           "# (level units, 2.8125 screen px each). label level tick camera_px pictures_changed largest_px largest_tag changed_on_view "
           "largest_on_view_px largest_on_view_tag off_the_ticks_place\n";
    int mismatched = 0;
    for (const Chapters::Level& level : table.levels) {
        const LevelRun run = RunLevel(level.name, 422, false, 418, 422);
        // "1-01", as the sweep's files are named (Chapters::Label has no zero).
        char label[16];
        std::snprintf(label, sizeof(label), "%d-%02d", level.world + 1, level.index + 1);
        for (const auto& [tick, stat] : run.window) {
            std::string tag = stat.largest.tag.empty() ? "-" : stat.largest.tag;
            std::replace(tag.begin(), tag.end(), ' ', '_');
            std::string viewTag = stat.largestOnScreen.tag.empty() ? "-" : stat.largestOnScreen.tag;
            std::replace(viewTag.begin(), viewTag.end(), ' ', '_');
            out << label << ' ' << level.name << ' ' << tick << ' ' << stat.cameraPx << ' ' << stat.moved << ' '
                << stat.largest.px << ' ' << tag << ' ' << stat.movedOnScreen << ' ' << stat.largestOnScreen.px << ' ' << viewTag
                << ' ' << run.mismatches << '\n';
        }
        out.flush();
        CHECK_MSG(run.mismatches == 0, level.name + ": " + run.firstMismatch);
        CHECK_MSG(run.ticks > 0, level.name + " ran");
        if (run.mismatches != 0) ++mismatched;
    }
    std::printf("  the full sweep: %zu levels, %d with a picture off its tick's place; the table is %s\n", table.levels.size(),
                mismatched, file.c_str());
}

void runTests(const std::string& sweepFile) {
    ThePicturesFileIsRead();
    TheWatchSmoothsOnlyWhereTheDisplayOutrunsTheTick();
    ABodysPictureStandsOnItsBodyAtEveryAlpha();
    EveryPictureStandsWhereItsTickLeftIt();
    if (!sweepFile.empty()) TheFullSweep(sweepFile);
}

} // namespace

MP_SUITE_MAIN_ARGS("test_mp_pictures", argc, argv) {
    std::string sweepFile;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--sweep" && i + 1 < argc) {
            sweepFile = argv[++i];
        } else {
            std::printf("test_mp test_mp_pictures [--sweep <file for the 128-level table>]\n");
            return 2;
        }
    }
    runTests(sweepFile);
    return ::test::summary("test_mp_pictures", 60);
}
