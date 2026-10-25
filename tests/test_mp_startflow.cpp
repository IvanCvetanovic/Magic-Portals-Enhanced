// The level start on a phone, through the real layer (planning step 205, PORT-7): sim/LevelStart.hpp is the rule and test_mp_levelstart pins it over scripts; this
// suite holds the LAYER to it. A bare layer with Paths::coverLevelStarts (a phone's run), driven tick by tick and frame by frame as the app drives it
// (OnFixedUpdate for a tick, OnUpdate for the frame that follows), over the converted levels.
//
// WHAT IS PINNED:
//   - a press on a level tile loads NOTHING in the tick that reads it, nor in the ticks of the frame it came in; the next frame the layer draws is ONE plain black
//     quad over the whole viewport (no menu, no text); the load runs in the first tick AFTER that frame; the level's first frame is the level's own;
//   - a press and a hold made while the cover is up, and one first seen on the tick after the level's first frame (what the system queued during the stall), fire
//     no shot and walk nobody; the same press a tick later is the player's;
//   - a run that does not cover its level starts (every other suite's bare Paths) loads inside the tick that asked, as it always did, and a press from the
//     visit tool's call (PressMenu) is synchronous in both;
//   - the play of a level started this way is the play of the level started the old way: the same level, at the same state, N ticks after the load - for a tile, and for
//     Retry and Next on the medal screen (the old path's twin reads the same replayed mouse the end screen has always read; fix round R of step 205);
//   - the press's noise is made ONCE, in the tick that asked, and not again by the worker that loads (read from the layer's latch before any frame drops it: no suite has a
//     sound device, and a frame with none clears what was latched);
//   - a press on a LOCKED tile asks for no start and draws no cover (a black frame for a tile that will not start would be a defect the player sees; the machine stays Idle).
// Each pin says in "mutation:" the change that turns it red; the Step records that they were run.
//
// NOT TESTED here, and why: the real exe's pictures and the real queue of a phone (the Step's scripted runs of the real exe press a tile, Retry, Next, the restart control, the
// dashboard's Start and the pause's Skip from the menu and the end screen: out/parity/program/PORT/PORT7). The state compared here is the sim's (the level, the shots, the
// camera, the player), never a picture: what an animation keyed on the layer's clock draws is the scripted runs' to say.

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "MagicPortalsLayer.hpp"

#include "core/Components.hpp"
#include "core/Input.hpp"
#include "core/PhysicsSystem.hpp"
#include "core/ScreenOverlay.hpp"
#include "core/ViewportInfo.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

using namespace Supersonic;
using MagicPortals::MagicPortalsLayer;
namespace LevelStart = MagicPortals::LevelStart;

namespace {

const std::string kLevels = MAGICPORTALS_LEVELS_DIR;
const std::string kData = MAGICPORTALS_DATA_DIR;
const std::string kChapters = MAGICPORTALS_CHAPTERS_FILE;
const glm::vec2 kRest(640.0f, 360.0f);

// The first three levels of chapter 1 cleared, so that its fourth (level3) is open: a tile a shot can be fired in, with no tutorial popup to close. The save is a
// scratch folder's own scores.json (a medal of 3 each), never anybody's save.
MagicPortalsLayer::Paths PhonePaths(bool cover) {
    MagicPortalsLayer::Paths paths;
    paths.prisms = std::filesystem::temp_directory_path() / "supersonic-test-mp-startflow";
    const std::filesystem::path saves = std::filesystem::temp_directory_path() / "supersonic-test-mp-startflow-saves";
    std::error_code ec;
    std::filesystem::create_directories(saves, ec);
    std::ofstream(saves / "scores.json", std::ios::binary | std::ios::trunc)
        << "{\"version\": 1, \"scores\": [{\"world\": 0, \"level\": 0, \"medal\": 3}, {\"world\": 0, \"level\": 1, \"medal\": 3}, {\"world\": 0, \"level\": 2, \"medal\": 3}]}";
    paths.saveDir = saves.string();
    paths.coverLevelStarts = cover;
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
void tickWith(MagicPortalsLayer& layer, entt::registry& registry, const glm::vec2& pointer, bool down, bool pressed, bool released) {
    PhysicsSystem::Update(registry, MagicPortalsLayer::kTick);
    Input::TickInput input;
    input.mousePosition = pointer;
    if (down) input.down = {MagicPortalsLayer::kTap};
    if (pressed) input.pressed = {MagicPortalsLayer::kTap};
    if (released) input.released = {MagicPortalsLayer::kTap};
    Input::BeginReplayedTick(input);
    layer.OnFixedUpdate(registry, MagicPortalsLayer::kTick);
    Input::EndReplayedTick();
}

void idleTick(MagicPortalsLayer& layer, entt::registry& registry) { tickWith(layer, registry, kRest, false, false, false); }
void pressTick(MagicPortalsLayer& layer, entt::registry& registry, const glm::vec2& at) { tickWith(layer, registry, at, true, true, false); }
void holdTick(MagicPortalsLayer& layer, entt::registry& registry, const glm::vec2& at) { tickWith(layer, registry, at, true, false, false); }
void liftTick(MagicPortalsLayer& layer, entt::registry& registry, const glm::vec2& at) { tickWith(layer, registry, at, false, false, true); }

// The frame the app draws after a tick: OnUpdate, into an overlay of our own.
std::vector<ScreenOverlay::Quad> frame(MagicPortalsLayer& layer, entt::registry& registry) {
    static ScreenOverlay overlay;
    overlay.Clear();
    registry.ctx().insert_or_assign<ScreenOverlay*>(&overlay);
    layer.OnUpdate(registry, MagicPortalsLayer::kTick);
    return overlay.Quads();
}

// Whether the frame is the plain black cover: exactly one quad, the whole viewport, black and opaque, no picture.
bool IsTheCover(const std::vector<ScreenOverlay::Quad>& quads) {
    if (quads.size() != 1) return false;
    const ScreenOverlay::Quad& quad = quads.front();
    return quad.min == glm::vec2(0.0f) && quad.max == glm::vec2(1.0f) && quad.color == glm::vec4(0.0f, 0.0f, 0.0f, 1.0f) && quad.texture.empty();
}

// The noise a tile, Retry, Next and the restart control make (the sound rules' event name). A press makes it ONCE, in the tick that asks for the start.
const char* const kButtonNoise = "level_button";

// How many times that noise is latched RIGHT NOW. Only meaningful before a frame: OnUpdate plays what is latched and, with no sound device (every suite), drops it - so a
// count read after a frame() is always 0 and proves nothing. Read it after the tick, before the next frame.
int LatchedButtonNoises(const MagicPortalsLayer& layer) {
    int count = 0;
    for (const std::string& sound : layer.LatchedSounds()) count += sound == kButtonNoise ? 1 : 0;
    return count;
}

glm::vec2 ScreenOfView(const MagicPortalsLayer& layer, const glm::dvec2& onView) {
    const glm::dvec2 view = layer.MenuBoxPx();
    return glm::vec2(static_cast<float>(onView.x / view.x * 1280.0), static_cast<float>(onView.y / view.y * 720.0));
}

const MagicPortalsLayer::MenuButton* MenuButtonOf(const MagicPortalsLayer& layer, MagicPortalsLayer::MenuButton::Kind kind, int which = 0) {
    int seen = 0;
    for (const MagicPortalsLayer::MenuButton& button : layer.MenuButtons()) {
        if (button.kind != kind) continue;
        if (seen++ == which) return &button;
    }
    return nullptr;
}

// Attached with no level named, walked from the loading screen to the grid of chapter 1 by the layer's own synchronous presses (as the layer suite does), so that
// the one press under test is the tile's.
bool WalkToTheGrid(MagicPortalsLayer& layer, entt::registry& registry) {
    using Kind = MagicPortalsLayer::MenuButton::Kind;
    layer.OnAttach(registry);
    if (!layer.LoadError().empty()) return false;
    for (int tick = 0; tick < 400 && layer.MenuScreen() != MagicPortalsLayer::Screen::Main; ++tick) idleTick(layer, registry);
    if (layer.MenuScreen() != MagicPortalsLayer::Screen::Main) return false;
    const MagicPortalsLayer::MenuButton* play = MenuButtonOf(layer, Kind::Play);
    if (play == nullptr) return false;
    layer.PressMenu(registry, *play);
    const MagicPortalsLayer::MenuButton* world = MenuButtonOf(layer, Kind::World, 0);
    if (world == nullptr) return false;
    layer.PressMenu(registry, *world);
    for (int tick = 0; tick < 60; ++tick) idleTick(layer, registry); // the tiles come to rest
    return layer.MenuScreen() == MagicPortalsLayer::Screen::Levels && MenuButtonOf(layer, Kind::Level, 0) != nullptr;
}

// A touch on the first level's tile: down on one tick, up on the next; the press is acted on by the tick after that (the menu's pending press).
glm::vec2 TheFirstTile(const MagicPortalsLayer& layer, int which = 0) {
    const MagicPortalsLayer::MenuButton* first = MenuButtonOf(layer, MagicPortalsLayer::MenuButton::Kind::Level, which);
    return ScreenOfView(layer, first->centrePx);
}

void ATilePressOnAPhoneCoversThenLoadsAfterTheCoverIsDrawn() {
    // mutation: M1 of the machine (the tick-count rule) or the layer asking the machine nothing before loading (the load in the press's own tick); Draw not called (no cover);
    // EmitMenu drawn under the cover; the load run in OnUpdate; the worker presses WITH the noise (it is made twice); the request never latches it (it is not made).
    using Screen = MagicPortalsLayer::Screen;
    entt::registry registry;
    publishViewport(registry);
    MagicPortalsLayer layer(PhonePaths(true), "");
    CHECK_MSG(WalkToTheGrid(layer, registry), "the grid of chapter 1 is up: " + layer.LoadError());
    if (layer.MenuScreen() != Screen::Levels) return;
    const glm::vec2 tile = TheFirstTile(layer);
    CHECK_MSG(frame(layer, registry).size() > 1, "before the press the frame is the grid: many quads");
    pressTick(layer, registry, tile);
    liftTick(layer, registry, tile);
    CHECK_MSG(layer.LevelStartMachine().phase() == LevelStart::Phase::Idle, "the lift only releases: the menu acts on it a tick later");
    idleTick(layer, registry); // the pending press is acted on here
    CHECK_MSG(layer.LevelStartMachine().phase() == LevelStart::Phase::CoverWanted, "the tile press ASKS for a start");
    CHECK_MSG(layer.SimLevel() == nullptr && layer.MenuScreen() == Screen::Levels, "and loads nothing in the tick that read it: the grid is still the screen");
    // The press's noise is made when it is asked for, once (read before any frame drops the latch).
    CHECK_MSG(LatchedButtonNoises(layer) == 1, "the press made its noise once, in the tick that asked: " + std::to_string(LatchedButtonNoises(layer)));
    // The other four ticks the engine may run before it draws: nothing loads in any of them.
    for (int tick = 0; tick < 4; ++tick) idleTick(layer, registry);
    CHECK_MSG(layer.SimLevel() == nullptr && layer.LevelStartMachine().phase() == LevelStart::Phase::CoverWanted,
              "and not in the four ticks that follow in the same frame (kMaxPhysicsStepsPerFrame is 5)");
    // The frame the layer draws next is the cover.
    const std::vector<ScreenOverlay::Quad> cover = frame(layer, registry);
    CHECK_MSG(IsTheCover(cover), "the frame after the press is ONE plain black quad over the whole viewport: " + std::to_string(cover.size()) + " quad(s)");
    CHECK_MSG(layer.LevelStartMachine().phase() == LevelStart::Phase::CoverIssued && layer.LevelStartMachine().coverFrames() == 1, "the cover was issued to the draw");
    CHECK_MSG(layer.SimLevel() == nullptr, "and the level is still not loaded when the cover is drawn");
    // A second frame with no tick between is the cover again.
    CHECK_MSG(IsTheCover(frame(layer, registry)), "a frame with no tick draws the cover again");
    // The next tick loads.
    idleTick(layer, registry);
    CHECK_MSG(layer.SimLevel() != nullptr && layer.MenuScreen() == Screen::None, "the first tick after the cover was drawn loads the level: " + layer.LoadError());
    CHECK_MSG(layer.LevelStartMachine().phase() == LevelStart::Phase::AwaitFirstFrame, "and waits for the level's first frame");
    // The worker that loads does not press with a noise of its own: the cover frames between the ask and the load dropped the ask's, so a count here is the worker's.
    CHECK_MSG(LatchedButtonNoises(layer) == 0, "the load did not make the press's noise again: " + std::to_string(LatchedButtonNoises(layer)));
    // The level's first frame is the level's own (many quads, no cover).
    const std::vector<ScreenOverlay::Quad> first = frame(layer, registry);
    CHECK_MSG(!IsTheCover(first) && !first.empty(), "the frame after the load is the level's: " + std::to_string(first.size()) + " quads");
    CHECK_MSG(layer.LevelStartMachine().phase() == LevelStart::Phase::Settling, "and the machine is settling");
}

void TheLevelAPhonePressStartsIsTheLevelTheOldPathStarts() {
    // mutation: the worker runs another call than the old path's (a different level, a different order), or Begin's request carries the wrong index.
    entt::registry oldRegistry;
    publishViewport(oldRegistry);
    MagicPortalsLayer old(PhonePaths(false), "");
    entt::registry newRegistry;
    publishViewport(newRegistry);
    MagicPortalsLayer phone(PhonePaths(true), "");
    if (!WalkToTheGrid(old, oldRegistry) || !WalkToTheGrid(phone, newRegistry)) {
        CHECK_MSG(false, "both layers reach the grid");
        return;
    }
    // The old path: the same touch, the same ticks; it loads in the tick that acts on the press.
    const glm::vec2 tile = TheFirstTile(old);
    pressTick(old, oldRegistry, tile);
    liftTick(old, oldRegistry, tile);
    idleTick(old, oldRegistry);
    CHECK_MSG(old.SimLevel() != nullptr && old.LevelStartMachine().phase() == LevelStart::Phase::Idle, "a run that does not cover loads in the tick that acted on the press");
    // The phone: pressed the same, covered, loaded.
    pressTick(phone, newRegistry, tile);
    liftTick(phone, newRegistry, tile);
    idleTick(phone, newRegistry);
    frame(phone, newRegistry);
    idleTick(phone, newRegistry);
    CHECK_MSG(phone.SimLevel() != nullptr, "and the phone's load has run");
    if (old.SimLevel() == nullptr || phone.SimLevel() == nullptr) return;
    CHECK_MSG(old.Current() != nullptr && phone.Current() != nullptr && old.Current()->name == phone.Current()->name,
              "the same level: " + (old.Current() != nullptr ? old.Current()->name : std::string("?")) + " and " + (phone.Current() != nullptr ? phone.Current()->name : std::string("?")));
    // The same state at tick 0 and after the same number of level ticks.
    for (int tick = 0; tick < 30; ++tick) {
        idleTick(old, oldRegistry);
        frame(old, oldRegistry);
        idleTick(phone, newRegistry);
        frame(phone, newRegistry);
    }
    CHECK_MSG(old.SimLevel()->portals.shotsFired == phone.SimLevel()->portals.shotsFired, "no shot in either");
    CHECK_MSG(old.CameraCentrePx() == phone.CameraCentrePx(), "the camera stands in the same place after 30 ticks");
    const auto playerAt = [](const MagicPortalsLayer& layer, entt::registry& registry) {
        return registry.get<TransformComponent>(layer.SimLevel()->player).position;
    };
    CHECK_MSG(playerAt(old, oldRegistry) == playerAt(phone, newRegistry), "and the player stands at the same place");
}

// A touch that is pressed on one tick, held for `hold` ticks and let go: a finger that stays on the glass long enough for the first-tap gate (400 ms, 26 ticks) to
// have passed, so that on the old path its release is a portal shot.
void FingerFor(MagicPortalsLayer& layer, entt::registry& registry, const glm::vec2& at, int hold) {
    pressTick(layer, registry, at);
    for (int tick = 0; tick < hold; ++tick) holdTick(layer, registry, at);
    liftTick(layer, registry, at);
}

// The same walk to a started level on the old path and on a phone, and the hazard on the old path: a finger that came down right after the load and stayed is a shot.
void APressMadeWhileTheCoverIsUpOrQueuedDuringTheLoadFiresNothing() {
    // mutation: Filter not called in pointerThisTick (the press reaches the level); the settling tick not ignored; Begin not marking the held finger; the cover tick not
    // reading the pointer (a press edge lost unread turns up later as a hold with no press).
    using Screen = MagicPortalsLayer::Screen;
    const glm::vec2 inTheWorld(900.0f, 250.0f);
    struct Outcome {
        bool ran = false;
        int shots = -1;
        int ignoredAtTheEnd = -1;
        std::string name;
    };
    // `when`: 0 = a finger pressed UNDER THE COVER (before the load); 1 = a finger first seen on the SETTLING tick (queued during the stall); 2 = a finger first seen
    // on the first tick the player can press on.
    const auto play = [&](bool cover, int when) {
        Outcome out;
        entt::registry registry;
        publishViewport(registry);
        MagicPortalsLayer layer(PhonePaths(cover), "");
        if (!WalkToTheGrid(layer, registry) || layer.MenuScreen() != Screen::Levels) return out;
        const glm::vec2 tile = TheFirstTile(layer, 3);
        pressTick(layer, registry, tile);
        liftTick(layer, registry, tile);
        idleTick(layer, registry); // acts on the press: asks (a phone) or loads (the old path)
        if (cover) {
            if (when == 0) {
                pressTick(layer, registry, inTheWorld); // under the cover
                frame(layer, registry);
                for (int tick = 0; tick < 40; ++tick) holdTick(layer, registry, inTheWorld); // loads in the first of these
                frame(layer, registry);
                liftTick(layer, registry, inTheWorld);
            } else {
                frame(layer, registry); // the cover
                idleTick(layer, registry); // loads
                frame(layer, registry);    // the level's first frame
                if (when == 1) {
                    FingerFor(layer, registry, inTheWorld, 40); // first seen on the settling tick
                } else {
                    idleTick(layer, registry); // the settling tick passes with nothing
                    FingerFor(layer, registry, inTheWorld, 40);
                }
            }
        } else {
            // The old path: loaded in that tick; the same finger as soon as the same ticks allow.
            frame(layer, registry);
            if (when == 0) {
                pressTick(layer, registry, inTheWorld);
                for (int tick = 0; tick < 40; ++tick) holdTick(layer, registry, inTheWorld);
                liftTick(layer, registry, inTheWorld);
            } else if (when == 1) {
                FingerFor(layer, registry, inTheWorld, 40);
            } else {
                idleTick(layer, registry);
                FingerFor(layer, registry, inTheWorld, 40);
            }
        }
        // Let the shot (if one was fired on the release) show.
        for (int tick = 0; tick < 6; ++tick) idleTick(layer, registry);
        if (layer.SimLevel() == nullptr) return out;
        out.ran = true;
        out.name = layer.Current() != nullptr ? layer.Current()->name : std::string();
        out.shots = layer.SimLevel()->portals.shotsFired;
        out.ignoredAtTheEnd = static_cast<int>(layer.LevelStartMachine().IgnoredContacts().size());
        return out;
    };
    // The hazard, on the old path: a finger that comes down right after the load and stays is a shot (this is what the cover and the rule remove).
    const Outcome oldQueued = play(false, 1);
    CHECK_MSG(oldQueued.ran, "the old path started the level");
    // On a phone, a finger pressed under the cover, one first seen on the settling tick, and each of them held for 40 ticks and let go: no shot.
    for (const int when : {0, 1}) {
        const Outcome phone = play(true, when);
        CHECK_MSG(phone.ran, "the phone started the level");
        CHECK_MSG(phone.name == "level3", "the worker started the right level: " + phone.name);
        CHECK_MSG(phone.shots == 0, std::string("a finger ") + (when == 0 ? "pressed under the cover" : "first seen on the settling tick") + " fires no shot: " + std::to_string(phone.shots));
        CHECK_MSG(phone.ignoredAtTheEnd == 0, "and its identity was forgotten when it lifted");
    }
    // The player's own press, one tick later than the settling tick, is a shot on a phone as on the old path: the rule takes nothing from a finger that comes after.
    const Outcome oldPlayer = play(false, 2);
    const Outcome phonePlayer = play(true, 2);
    CHECK_MSG(oldPlayer.ran && phonePlayer.ran && phonePlayer.shots == oldPlayer.shots, "a press after the settling tick is the player's, as on the old path: " + std::to_string(phonePlayer.shots) +
                                                                                           " shot(s) against " + std::to_string(oldPlayer.shots));
    // What the old path does with the finger queued during the stall, for the Step's record (the hazard is a shot only when the level lets a shot out that early).
    std::printf("  test_mp_startflow: a finger down right after the load on the old path fired %d shot(s); a player's press one tick later fired %d on a phone\n", oldQueued.shots, phonePlayer.shots);
}

void ARunThatDoesNotCoverLoadsInTheTickThatAskedAndPressMenuIsAlwaysSynchronous() {
    // mutation: the cover mode on for a bare Paths; PressMenu deferred.
    using Screen = MagicPortalsLayer::Screen;
    for (const bool cover : {false, true}) {
        entt::registry registry;
        publishViewport(registry);
        MagicPortalsLayer layer(PhonePaths(cover), "");
        CHECK_MSG(WalkToTheGrid(layer, registry), "the grid is up");
        if (layer.MenuScreen() != Screen::Levels) continue;
        // The visit tool's call: PressMenu with a Level button loads at once, covered run or not.
        const MagicPortalsLayer::MenuButton* first = MenuButtonOf(layer, MagicPortalsLayer::MenuButton::Kind::Level, 0);
        const MagicPortalsLayer::MenuButton button = *first;
        layer.PressMenu(registry, button);
        CHECK_MSG(layer.SimLevel() != nullptr && layer.MenuScreen() == Screen::None, std::string("PressMenu is synchronous, ") + (cover ? "covered run" : "plain run"));
        CHECK_MSG(layer.LevelStartMachine().phase() == LevelStart::Phase::Idle, "and asks the machine for nothing");
    }
}

// The ticks that hold the right key, as a walk into level0's door (the fixture test_mp_statics proves reaches its exit by holding right).
void WalkIntoTheDoor(MagicPortalsLayer& layer, entt::registry& registry) {
    for (int tick = 0; tick < 600 && layer.MenuScreen() != MagicPortalsLayer::Screen::Finished; ++tick) {
        PhysicsSystem::Update(registry, MagicPortalsLayer::kTick);
        Input::TickInput input;
        input.mousePosition = kRest;
        input.down = {MagicPortalsLayer::kRight};
        if (tick == 0) input.pressed = {MagicPortalsLayer::kRight};
        Input::BeginReplayedTick(input);
        layer.OnFixedUpdate(registry, MagicPortalsLayer::kTick);
        Input::EndReplayedTick();
        frame(layer, registry);
    }
    for (int tick = 0; tick < 150; ++tick) {
        idleTick(layer, registry);
        frame(layer, registry);
    }
}

glm::vec2 ScreenOfLevelView(const MagicPortalsLayer& layer, const glm::dvec2& onView) {
    const glm::dvec2 view = layer.ViewPx();
    return glm::vec2(static_cast<float>(onView.x / view.x * 1280.0), static_cast<float>(onView.y / view.y * 720.0));
}

// Where the medal screen DRAWS a button: the middle of its picture's overlay quad, which is where a player's finger finds it; (-1, -1) where it is not drawn.
glm::vec2 WhereTheMedalScreenDraws(MagicPortalsLayer& layer, entt::registry& registry, MagicPortalsLayer::MenuButton::Kind kind) {
    const std::vector<ScreenOverlay::Quad> quads = frame(layer, registry);
    const std::string picture = kind == MagicPortalsLayer::MenuButton::Kind::Retry ? "button_restart.png" : "button_right.png";
    glm::vec2 at(-1.0f, -1.0f);
    for (const ScreenOverlay::Quad& quad : quads) {
        if (quad.texture.size() >= picture.size() && quad.texture.compare(quad.texture.size() - picture.size(), std::string::npos, picture) == 0) {
            at = (quad.min + quad.max) * 0.5f * glm::vec2(1280.0f, 720.0f);
        }
    }
    return at;
}

void RetryAndNextOnTheMedalScreenAreCoveredAndLoadAfterTheCover() {
    // mutation: the end screen's press goes to PressMenu (not pressFromInput); the end screen reads the raw mouse in a phone's run; the worker loads the wrong level.
    using Screen = MagicPortalsLayer::Screen;
    using Kind = MagicPortalsLayer::MenuButton::Kind;
    for (const Kind kind : {Kind::Retry, Kind::Next}) {
        entt::registry registry;
        publishViewport(registry);
        MagicPortalsLayer layer(PhonePaths(true), "level0");
        layer.OnAttach(registry);
        if (!layer.LoadError().empty()) {
            CHECK_MSG(false, layer.LoadError());
            return;
        }
        WalkIntoTheDoor(layer, registry);
        CHECK_MSG(layer.MenuScreen() == Screen::Finished, "level0 is finished and the medal screen is up");
        if (layer.MenuScreen() != Screen::Finished) return;
        // The button where the screen DRAWS it (the overlay quad of its picture), which is where a player's finger finds it.
        const glm::vec2 at = WhereTheMedalScreenDraws(layer, registry, kind);
        const std::string what = kind == Kind::Retry ? "Retry" : "Next";
        CHECK_MSG(at.x > 0.0f && at.x < 1280.0f && at.y > 0.0f && at.y < 720.0f, what + "'s button is drawn on the screen");
        if (at.x < 0.0f) return;
        pressTick(layer, registry, at);
        CHECK_MSG(layer.LevelStartMachine().phase() == LevelStart::Phase::CoverWanted, what + " ASKS for a start on a phone");
        CHECK_MSG(LatchedButtonNoises(layer) == 1, what + "'s press made its noise once, in the tick that asked: " + std::to_string(LatchedButtonNoises(layer)));
        CHECK_MSG(layer.MenuScreen() == Screen::Finished && layer.Current() != nullptr && layer.Current()->name == "level0", what + " loads nothing in the tick that read the press");
        for (int tick = 0; tick < 4; ++tick) holdTick(layer, registry, at); // the other ticks of the frame, with the finger down
        CHECK_MSG(layer.Current()->name == "level0" && layer.MenuScreen() == Screen::Finished, "and nothing in the ticks that follow in the frame");
        CHECK_MSG(IsTheCover(frame(layer, registry)), "the frame that follows is the cover");
        holdTick(layer, registry, at); // the tick that loads, the finger still down
        CHECK_MSG(layer.MenuScreen() == Screen::None && layer.SimLevel() != nullptr, what + ": the level came up in the first tick after the cover");
        CHECK_MSG(LatchedButtonNoises(layer) == 0, what + ": the load did not make the press's noise again: " + std::to_string(LatchedButtonNoises(layer)));
        CHECK_MSG(layer.Current() != nullptr && layer.Current()->name == (kind == Kind::Retry ? "level0" : "level1"), what + " started the right level");
        CHECK_MSG(!IsTheCover(frame(layer, registry)), "and its first frame is the level's");
        // The finger that pressed the button is still down on the new level: it walks nobody and fires nothing until it lifts.
        for (int tick = 0; tick < 40; ++tick) {
            holdTick(layer, registry, at);
            frame(layer, registry);
        }
        CHECK_MSG(layer.SimLevel()->portals.shotsFired == 0, "and fires no shot");
        liftTick(layer, registry, at);
        CHECK_MSG(layer.SimLevel()->portals.shotsFired == 0 && layer.LevelStartMachine().IgnoredContacts().empty(), what + ": its release was swallowed and forgotten");
    }
}

// The sim's state some ticks after a start, without a picture: which level, how many shots, where the camera and the player stand.
struct StartedState {
    bool up = false;
    std::string name;
    int shots = -1;
    glm::dvec2 camera{0.0};
    glm::vec3 player{0.0f};
};

StartedState StateOf(const MagicPortalsLayer& layer, entt::registry& registry) {
    StartedState state;
    if (layer.SimLevel() == nullptr || layer.MenuScreen() != MagicPortalsLayer::Screen::None || layer.Current() == nullptr) return state;
    state.up = true;
    state.name = layer.Current()->name;
    state.shots = layer.SimLevel()->portals.shotsFired;
    state.camera = layer.CameraCentrePx();
    state.player = registry.get<TransformComponent>(layer.SimLevel()->player).position;
    return state;
}

void RetryAndNextOnAPhoneStartTheLevelTheOldPathStarts() {
    // mutation: the worker runs another call than the old path's for Retry or Next (another level, another order), or the request carries the wrong kind.
    using Screen = MagicPortalsLayer::Screen;
    using Kind = MagicPortalsLayer::MenuButton::Kind;
    for (const Kind kind : {Kind::Retry, Kind::Next}) {
        const std::string what = kind == Kind::Retry ? "Retry" : "Next";
        // The same medal screen and the same press; the old path (no cover) loads in the tick that read it, a phone asks, draws the cover and loads in the first tick after.
        // The finger is up from the next tick on in both, so there is nothing for the rule to ignore, and the state is compared N ticks after each load.
        const auto play = [&](bool cover) {
            entt::registry registry;
            publishViewport(registry);
            MagicPortalsLayer layer(PhonePaths(cover), "level0");
            layer.OnAttach(registry);
            if (!layer.LoadError().empty()) return StartedState{};
            WalkIntoTheDoor(layer, registry);
            if (layer.MenuScreen() != Screen::Finished) return StartedState{};
            const glm::vec2 at = WhereTheMedalScreenDraws(layer, registry, kind);
            if (at.x < 0.0f) return StartedState{};
            pressTick(layer, registry, at); // the old path loads here; a phone asks
            if (cover) {
                frame(layer, registry);    // the cover
                idleTick(layer, registry); // the load
            }
            frame(layer, registry); // the level's first frame
            for (int tick = 0; tick < 30; ++tick) {
                idleTick(layer, registry);
                frame(layer, registry);
            }
            return StateOf(layer, registry);
        };
        const StartedState old = play(false);
        const StartedState phone = play(true);
        CHECK_MSG(old.up && phone.up, what + ": both paths started a level");
        if (!old.up || !phone.up) continue;
        const std::string expected = kind == Kind::Retry ? "level0" : "level1";
        CHECK_MSG(old.name == expected && phone.name == expected, what + ": the same level on both paths: " + old.name + " and " + phone.name);
        CHECK_MSG(old.shots == phone.shots, what + ": the same shots on both");
        CHECK_MSG(old.camera == phone.camera, what + ": the camera stands in the same place 30 ticks after the load");
        CHECK_MSG(old.player == phone.player, what + ": and the player stands at the same place");
        // And the sim's state is a direct load's: the pictures that differ from one (the Step's deviation 2) are no difference of the level, its shots, the camera or the player.
        entt::registry directRegistry;
        publishViewport(directRegistry);
        MagicPortalsLayer direct(PhonePaths(false), expected);
        direct.OnAttach(directRegistry);
        frame(direct, directRegistry);
        for (int tick = 0; tick < 30; ++tick) {
            idleTick(direct, directRegistry);
            frame(direct, directRegistry);
        }
        const StartedState straight = StateOf(direct, directRegistry);
        CHECK_MSG(straight.up && straight.name == old.name && straight.shots == old.shots && straight.camera == old.camera && straight.player == old.player,
                  what + ": the state 30 ticks after the load is the state of a direct load of the same level");
    }
}

void TheRestartControlOnAPhoneIsCoveredTooAndStartsTheLevelAgain() {
    // mutation: the Reset case loads at once on a phone; the tick after the request steps the old level.
    using Screen = MagicPortalsLayer::Screen;
    entt::registry registry;
    publishViewport(registry);
    MagicPortalsLayer layer(PhonePaths(true), "level3");
    layer.OnAttach(registry);
    if (!layer.LoadError().empty()) {
        CHECK_MSG(false, layer.LoadError());
        return;
    }
    for (int tick = 0; tick < 40; ++tick) {
        idleTick(layer, registry);
        frame(layer, registry);
    }
    // A shot, so that a fresh level can be told from the old one.
    const glm::vec2 inTheWorld(900.0f, 250.0f);
    FingerFor(layer, registry, inTheWorld, 2);
    for (int tick = 0; tick < 30; ++tick) idleTick(layer, registry);
    CHECK_MSG(layer.SimLevel() != nullptr && layer.SimLevel()->portals.shotsFired == 1, "a shot was fired in level3");
    MagicPortals::Hud::Rect rect;
    if (!layer.ControlRect(MagicPortalsLayer::Control::Reset, rect)) {
        CHECK_MSG(false, "the restart control is there");
        return;
    }
    const glm::vec2 at = ScreenOfLevelView(layer, rect.Centre());
    pressTick(layer, registry, at);
    CHECK_MSG(layer.LevelStartMachine().phase() == LevelStart::Phase::CoverWanted, "the restart control ASKS for a start on a phone");
    CHECK_MSG(LatchedButtonNoises(layer) == 1, "the control's press made its noise once, in the tick that asked: " + std::to_string(LatchedButtonNoises(layer)));
    CHECK_MSG(layer.SimLevel()->portals.shotsFired == 1, "and the old level is still the level in the tick that read the press");
    holdTick(layer, registry, at);
    CHECK_MSG(layer.SimLevel()->portals.shotsFired == 1 && layer.LevelStartMachine().phase() == LevelStart::Phase::CoverWanted,
              "and in the ticks of that frame");
    CHECK_MSG(IsTheCover(frame(layer, registry)), "the next frame is the cover");
    holdTick(layer, registry, at);
    CHECK_MSG(layer.SimLevel() != nullptr && layer.MenuScreen() == Screen::None && layer.SimLevel()->portals.shotsFired == 0, "the first tick after it starts level3 again, with no shot fired");
    CHECK_MSG(LatchedButtonNoises(layer) == 0, "and the load did not make the control's noise again: " + std::to_string(LatchedButtonNoises(layer)));
    CHECK_MSG(layer.Current() != nullptr && layer.Current()->name == "level3", "it is level3");
    frame(layer, registry);
    for (int tick = 0; tick < 3; ++tick) {
        holdTick(layer, registry, at);
        frame(layer, registry);
    }
    liftTick(layer, registry, at);
    CHECK_MSG(layer.SimLevel()->portals.shotsFired == 0, "the finger that pressed the control fires nothing when it lifts on the new level");
}

void APressOnALockedTileAsksForNoStartAndDrawsNoCover() {
    // mutation: O7 of the verification (fix round 2 of step 205) - pressFromInput's Level case without its Locking::LevelUnlocked test, so that a tile that will not start
    // still asks for a start and the player is shown a black frame and then the same grid.
    using Screen = MagicPortalsLayer::Screen;
    // PhonePaths clears levels 0 to 2, so level3 is open and level4 (the fifth tile) is not: a level is open when the one before it has a medal.
    constexpr int kLockedTile = 4;
    for (const bool cover : {false, true}) {
        const std::string path = cover ? "a phone: " : "the old path: ";
        entt::registry registry;
        publishViewport(registry);
        MagicPortalsLayer layer(PhonePaths(cover), "");
        CHECK_MSG(WalkToTheGrid(layer, registry), path + "the grid of chapter 1 is up: " + layer.LoadError());
        if (layer.MenuScreen() != Screen::Levels) continue;
        const glm::vec2 tile = TheFirstTile(layer, kLockedTile);
        frame(layer, registry);
        pressTick(layer, registry, tile);
        liftTick(layer, registry, tile);
        idleTick(layer, registry); // the menu acts on the press here: a tile that is open asks (a phone) or loads (the old path)
        // The press did reach the menu's button handler: PressMenu makes the tile's noise "whether or not the tile is open", so a press the grid never took would not have.
        // Without this the checks below could pass because no press happened at all.
        CHECK_MSG(LatchedButtonNoises(layer) == 1, path + "the press on the locked tile reached the menu (its noise was made once): " + std::to_string(LatchedButtonNoises(layer)));
        CHECK_MSG(layer.LevelStartMachine().Idle(), path + "a locked tile asks for no start");
        CHECK_MSG(layer.SimLevel() == nullptr && layer.MenuScreen() == Screen::Levels, path + "and starts nothing: the grid is still the screen");
        // No cover is ever drawn: the frames that follow, and the ticks the engine runs between them, are the grid.
        for (int round = 0; round < 6; ++round) {
            const std::vector<ScreenOverlay::Quad> drawn = frame(layer, registry);
            CHECK_MSG(!IsTheCover(drawn) && drawn.size() > 1, path + "the frame after the press is the grid, not a black frame: " + std::to_string(drawn.size()) + " quad(s)");
            idleTick(layer, registry);
        }
        CHECK_MSG(layer.LevelStartMachine().Idle() && layer.LevelStartMachine().coverFrames() == 0, path + "the machine never left Idle and no cover was issued to a draw");
        CHECK_MSG(layer.SimLevel() == nullptr && layer.MenuScreen() == Screen::Levels, path + "and no level came up later either");
    }
}

void runTests() {
    ATilePressOnAPhoneCoversThenLoadsAfterTheCoverIsDrawn();
    TheLevelAPhonePressStartsIsTheLevelTheOldPathStarts();
    APressMadeWhileTheCoverIsUpOrQueuedDuringTheLoadFiresNothing();
    ARunThatDoesNotCoverLoadsInTheTickThatAskedAndPressMenuIsAlwaysSynchronous();
    RetryAndNextOnTheMedalScreenAreCoveredAndLoadAfterTheCover();
    RetryAndNextOnAPhoneStartTheLevelTheOldPathStarts();
    TheRestartControlOnAPhoneIsCoveredTooAndStartsTheLevelAgain();
    APressOnALockedTileAsksForNoStartAndDrawsNoCover();
}

} // namespace

MP_SUITE_MAIN("test_mp_startflow") {
    std::error_code ec;
    // The menus, the tiles and the medal screen are drawn with the original's extracted art, which a machine can lack while it has the levels (test_mp_layer says the same).
    if (!std::filesystem::is_directory(kLevels, ec) || !std::filesystem::is_regular_file(kChapters, ec) ||
        !std::filesystem::is_regular_file(kData + "/entity_roles.json", ec) || !std::filesystem::is_directory(std::string(MAGICPORTALS_ORIGINAL_DIR) + "/entities", ec)) {
        std::printf("test_mp_startflow: SKIPPED - needs the converted levels at %s, chapters.json at %s, Enhanced's data at %s and the original's extracted assets at %s.\n",
                    kLevels.c_str(), kChapters.c_str(), kData.c_str(), MAGICPORTALS_ORIGINAL_DIR);
        return 77;
    }
    runTests();
    return ::test::summary("test_mp_startflow", 20);
}
