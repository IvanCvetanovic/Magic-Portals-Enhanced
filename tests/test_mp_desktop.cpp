// A desktop player's run (planning step 277: players' reports that on Windows the game could not be made full screen and that the phone's on-screen controls
// stayed up). sim/Desktop is the rule and the file; sim/Options the screen with the two switches; the LAYER is what a player meets.
//
// WHAT IS PINNED:
//   - controls.json's `desktop` block as shipped (a window, no walk arrows, the walking hint), and its refusals by name; whose run is a desktop player's;
//   - which keys ask for full screen (F11; Enter with an Alt held; neither alone) and how a start opens (--windowed, --fullscreen, the kept choice, the default);
//   - desktop.json: kept, read back, only the choices made, a broken file kept out and written over, an empty directory memory-only;
//   - the desktop Options screen: its two switches on every view from 5:4 to 21:9, no widget meeting another or leaving the view, claimed only there, lit and
//     filled while on, the tiles' thumbnails with walk arrows only while they are on; a phone's screen has no switch;
//   - the layer: a desktop run draws and claims no walk arrow (a tap where one was is the world's) and the keys walk; a run that is not a desktop's has them;
//     F11 and Alt+Enter ask the window and keep the choice, Enter alone and any key on a run that is not a desktop's do not; the engine's own key codes reach it;
//     the choice of arrows is read by the next run; Options' two switches work by a touch and are drawn as they are; a run without Options (--controls
//     classic) keeps its arrows, a player's saved Classic style hides them; 1-01 says the keys where its arrows are hidden, and only there; the full screen
//     keys leave the intro playing.
//
// NOT TESTED here: the window itself (the engine's NativeWindowControl) and main's start (it sets the engine's option from Desktop::StartFullscreen).

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "MagicPortalsLayer.hpp"
#include "sim/Controls.hpp"
#include "sim/Credits.hpp"
#include "sim/Desktop.hpp"
#include "sim/Hud.hpp"
#include "sim/Options.hpp"
#include "sim/Units.hpp"

#include "core/Components.hpp"
#include "core/Input.hpp"
#include "core/PhysicsSystem.hpp"
#include "core/ScreenOverlay.hpp"
#include "core/ViewportInfo.hpp"
#include "core/WindowControl.hpp"

#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

using namespace Supersonic;
using MagicPortals::MagicPortalsLayer;
namespace Desktop = MagicPortals::Desktop;
namespace Controls = MagicPortals::Controls;
namespace Options = MagicPortals::Options;
namespace Credits = MagicPortals::Credits;
namespace Hud = MagicPortals::Hud;
namespace Units = MagicPortals::Units;

namespace {

const std::string kLevels = MAGICPORTALS_LEVELS_DIR;
const std::string kData = MAGICPORTALS_DATA_DIR;
const std::string kChapters = MAGICPORTALS_CHAPTERS_FILE;
const std::string kPortData = MAGICPORTALS_PORT_DATA_DIR;
const std::string kControls = kPortData + "/controls.json";
const std::string kUi = kPortData + "/ui.json";
const glm::vec2 kRest(640.0f, 360.0f);
constexpr double kPxPerUnit = 720.0 / 256.0;

std::filesystem::path Scratch(const std::string& name) {
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / ("supersonic-test-mp-desktop-" + name);
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);
    return dir;
}

std::string Read(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    std::ostringstream text;
    text << file.rdbuf();
    return text.str();
}

// CHECK_EQ cannot print a string (the harness asks std::to_string of both), so two texts are compared here and shown.
void CheckText(const std::string& got, const std::string& expected) {
    CHECK_MSG(got == expected, "got " + got + ", expected " + expected);
}

// A window that takes the layer's requests when told to, as the engine applies them at the top of the next frame.
class FakeWindow final : public WindowControl {
public:
    bool fullscreen = false;
    int applied = 0;
    bool IsFullscreen() const override { return fullscreen; }
    glm::uvec2 WindowSize() const override { return glm::uvec2(1280u, 720u); }
    std::vector<DisplayMode> DisplayModes() const override { return {}; }
    DisplayMode DesktopMode() const override { return {}; }
    // How many requests were waiting, then applied.
    bool Apply() {
        const Requests requests = TakeRequests();
        if (!requests.setFullscreen) return false;
        fullscreen = requests.fullscreen;
        ++applied;
        return true;
    }
};

// ---- the rule and the file --------------------------------------------------------------------------------------------------

void TheShippedDefaultsAndTheirRefusals() {
    Desktop::Rules rules;
    std::string error;
    CHECK_MSG(Desktop::LoadRules(kControls, rules, error), error);
    CHECK_MSG(!rules.fullscreenDefault, "a desktop run opens in a window until the player says otherwise");
    CHECK_MSG(!rules.walkPadsDefault, "and shows no walk arrows");
    CheckText(rules.walkHint.words, "Walk with the arrow keys");
    CHECK_MSG(rules.walkHint.atScreen.y > 0.88, "below the walk arrows' row");

    const std::filesystem::path dir = Scratch("rules");
    const auto refused = [&](const char* name, const std::string& text, const char* named) {
        const std::filesystem::path file = dir / name;
        std::ofstream(file, std::ios::binary) << text;
        Desktop::Rules out;
        std::string why;
        CHECK_MSG(!Desktop::LoadRules(file.string(), out, why) && why.find(named) != std::string::npos, std::string(name) + " is refused by name: " + why);
    };
    refused("none.json", "{}", "desktop");
    refused("half.json", "{\"desktop\": {\"fullscreen_default\": true}}", "desktop.walk_pads_default");
    refused("word.json", "{\"desktop\": {\"fullscreen_default\": \"no\", \"walk_pads_default\": false}}", "desktop.fullscreen_default");
    refused("nohint.json", "{\"desktop\": {\"fullscreen_default\": false, \"walk_pads_default\": false}}", "desktop.walk_hint");
    refused("nowords.json", "{\"desktop\": {\"fullscreen_default\": false, \"walk_pads_default\": false, \"walk_hint\": {}}}", "words");

    CHECK_MSG(Desktop::PlayerRun(true, false, false), "a person at a desktop build");
    CHECK_MSG(!Desktop::PlayerRun(false, false, false), "a capture or a route");
    CHECK_MSG(!Desktop::PlayerRun(true, true, false), "a phone's build");
    CHECK_MSG(!Desktop::PlayerRun(true, false, true), "the phone profile");
}

void TheKeysAndTheStart() {
    CHECK(Desktop::ToggleAsked(true, false, false));
    CHECK(Desktop::ToggleAsked(false, true, true));
    CHECK_MSG(!Desktop::ToggleAsked(false, true, false), "Enter alone");
    CHECK_MSG(!Desktop::ToggleAsked(false, false, true), "an Alt alone");
    CHECK(!Desktop::ToggleAsked(false, false, false));
    CHECK_EQ(Desktop::kKeyF11, 300);
    CHECK_EQ(Desktop::kKeyKeypadEnter, 335);

    CHECK_MSG(!Desktop::StartFullscreen(std::nullopt, false, false, false), "nothing kept: the default, a window");
    CHECK_MSG(Desktop::StartFullscreen(std::nullopt, true, false, false), "nothing kept: the default, full");
    CHECK_MSG(Desktop::StartFullscreen(true, false, false, false), "kept full");
    CHECK_MSG(!Desktop::StartFullscreen(false, true, false, false), "kept a window");
    CHECK_MSG(!Desktop::StartFullscreen(true, true, false, true), "--windowed wins");
    CHECK_MSG(Desktop::StartFullscreen(false, false, true, false), "--fullscreen wins over the kept window");
}

void TheFile() {
    const std::filesystem::path dir = Scratch("store");
    std::string error;
    Desktop::Store store;
    CHECK_MSG(store.Open(dir.string(), error), error);
    CHECK(!store.Fullscreen() && !store.WalkPads() && store.Persists());
    store.SetFullscreen(true);
    CHECK_MSG(store.Save(error), error);
    CheckText(Read(dir / Desktop::kStoreFile), "{\"version\":1,\"fullscreen\":true}");
    store.SetWalkPads(false);
    CHECK_MSG(store.Save(error), error);
    CheckText(Read(dir / Desktop::kStoreFile), "{\"version\":1,\"fullscreen\":true,\"walk_pads\":false}");
    Desktop::Store back;
    CHECK_MSG(back.Open(dir.string(), error), error);
    CHECK(back.Fullscreen() == std::optional<bool>(true) && back.WalkPads() == std::optional<bool>(false));

    // A value that is not true or false is no choice; a file that is not an object is refused, and written over by the next Save.
    std::ofstream(dir / Desktop::kStoreFile, std::ios::binary | std::ios::trunc) << "{\"fullscreen\": 1, \"walk_pads\": true}";
    CHECK_MSG(back.Open(dir.string(), error), error);
    CHECK(!back.Fullscreen() && back.WalkPads() == std::optional<bool>(true));
    std::ofstream(dir / Desktop::kStoreFile, std::ios::binary | std::ios::trunc) << "[1";
    CHECK(!back.Open(dir.string(), error) && !error.empty());
    CHECK(!back.Fullscreen() && !back.WalkPads());
    back.SetFullscreen(false);
    CHECK_MSG(back.Save(error), error);
    CheckText(Read(dir / Desktop::kStoreFile), "{\"version\":1,\"fullscreen\":false}");

    Desktop::Store memory;
    CHECK_MSG(memory.Open("", error), error);
    memory.SetFullscreen(true);
    CHECK_MSG(memory.Save(error) && !memory.Persists(), "memory only: nothing written");
}

// ---- the screen -----------------------------------------------------------------------------------------------------------

struct Screen {
    Controls::Rules controls;
    Options::Rules phone;
    Options::Rules desktop;
    Credits::Rules credits;
    Hud::Rules hud;
    bool ok = false;
};

Screen LoadScreen() {
    Screen s;
    std::string error;
    s.ok = Controls::LoadRules(kControls, s.controls, error) && Options::LoadRules(kControls, s.controls, s.phone, error) &&
           Credits::LoadRules(kUi, s.credits, error) && Hud::LoadRules(kUi, s.hud, error);
    CHECK_MSG(s.ok, error);
    s.desktop = Options::ForDesktop(s.phone);
    return s;
}

bool Meet(const Hud::Rect& a, const Hud::Rect& b) {
    return a.min.x < b.Max().x && b.min.x < a.Max().x && a.min.y < b.Max().y && b.min.y < a.Max().y;
}

// The desktop shape with the frame-rate row (planning step 285): every widget still on the view and none meeting another.
void TheDesktopScreenWithTheRateRowOnEveryShape() {
    Screen s = LoadScreen();
    if (!s.ok) return;
    std::string error;
    CHECK_MSG(Options::LoadRateRow(kControls, s.desktop, error), error);
    const Options::Rules rules = Options::ForRateRow(s.desktop);
    CHECK(rules.desktop.shown && rules.rateRow.shown);
    constexpr double kSettled = 1.0e9;
    for (const double aspect : {5.0 / 4.0, 4.0 / 3.0, 16.0 / 10.0, 16.0 / 9.0, 20.0 / 9.0, 21.0 / 9.0}) {
        const glm::dvec2 view(256.0 * aspect, 256.0);
        const std::string at = " with the row on a view " + std::to_string(view.x) + " wide";
        const std::vector<std::pair<std::string, Hud::Rect>> widgets = {
            {"back", Credits::BackHitRect(s.credits, view, kSettled)},
            {"modern", Options::TileRect(rules, Controls::Style::Modern, view)},
            {"classic", Options::TileRect(rules, Controls::Style::Classic, view)},
            {"adjust", Options::AdjustRect(rules, view)},
            {"full screen", Options::SwitchRect(rules, Options::Widget::Fullscreen, view)},
            {"walk arrows", Options::SwitchRect(rules, Options::Widget::WalkPads, view)},
            {"frame rate", Options::RateRowRect(rules, view)}};
        for (std::size_t i = 0; i < widgets.size(); ++i) {
            const Hud::Rect& r = widgets[i].second;
            if (i > 0) CHECK_MSG(r.min.x >= 0.0 && r.min.y >= 0.0 && r.Max().x <= view.x && r.Max().y <= view.y, widgets[i].first + " on the view" + at);
            for (std::size_t j = i + 1; j < widgets.size(); ++j) CHECK_MSG(!Meet(r, widgets[j].second), widgets[i].first + " and " + widgets[j].first + " do not meet" + at);
        }
        CHECK(Options::WidgetAt(rules, s.credits, view, kSettled, widgets.back().second.Centre(), true) == Options::Widget::RateRow);
    }
}

void TheDesktopScreenOnEveryShape() {
    const Screen s = LoadScreen();
    if (!s.ok) return;
    CHECK(!s.phone.desktop.shown && s.desktop.desktop.shown);
    constexpr double kSettled = 1.0e9;
    // 5:4, 4:3, 16:10, 16:9, 20:9 and 21:9, on the 256-unit view.
    for (const double aspect : {5.0 / 4.0, 4.0 / 3.0, 16.0 / 10.0, 16.0 / 9.0, 20.0 / 9.0, 21.0 / 9.0}) {
        const glm::dvec2 view(256.0 * aspect, 256.0);
        const std::string at = " on a view " + std::to_string(view.x) + " wide";
        const std::vector<std::pair<std::string, Hud::Rect>> widgets = {
            {"back", Credits::BackHitRect(s.credits, view, kSettled)},
            {"modern", Options::TileRect(s.desktop, Controls::Style::Modern, view)},
            {"classic", Options::TileRect(s.desktop, Controls::Style::Classic, view)},
            {"adjust", Options::AdjustRect(s.desktop, view)},
            {"full screen", Options::SwitchRect(s.desktop, Options::Widget::Fullscreen, view)},
            {"walk arrows", Options::SwitchRect(s.desktop, Options::Widget::WalkPads, view)}};
        for (std::size_t i = 0; i < widgets.size(); ++i) {
            const Hud::Rect& r = widgets[i].second;
            if (i > 0) CHECK_MSG(r.min.x >= 0.0 && r.min.y >= 0.0 && r.Max().x <= view.x && r.Max().y <= view.y, widgets[i].first + " on the view" + at);
            for (std::size_t j = i + 1; j < widgets.size(); ++j) CHECK_MSG(!Meet(r, widgets[j].second), widgets[i].first + " and " + widgets[j].first + " do not meet" + at);
        }
        const auto on = [&](const Options::Rules& rules, const glm::dvec2& point) { return Options::WidgetAt(rules, s.credits, view, kSettled, point, true); };
        CHECK_MSG(on(s.desktop, widgets[1].second.Centre()) == Options::Widget::ModernTile, "the Modern tile" + at);
        CHECK_MSG(on(s.desktop, widgets[2].second.Centre()) == Options::Widget::ClassicTile, "the Classic tile" + at);
        CHECK_MSG(on(s.desktop, widgets[3].second.Centre()) == Options::Widget::Adjust, "the adjust button" + at);
        CHECK_MSG(on(s.desktop, widgets[4].second.Centre()) == Options::Widget::Fullscreen, "the full screen switch" + at);
        CHECK_MSG(on(s.desktop, widgets[5].second.Centre()) == Options::Widget::WalkPads, "the walk arrows switch" + at);
        CHECK_MSG(on(s.phone, widgets[4].second.Centre()) != Options::Widget::Fullscreen && on(s.phone, widgets[5].second.Centre()) != Options::Widget::WalkPads,
                  "a phone's screen claims no switch" + at);
    }
}

// The two switches' places are held apart by the reader: closer than a switch is wide, they would lie on each other.
void TheSwitchesMayNotMeet() {
    const Screen s = LoadScreen();
    if (!s.ok) return;
    const std::string text = Read(kControls);
    const std::string key = "\"centre_offset_units\": 79,";
    const std::size_t at = text.find(key);
    CHECK_MSG(at != std::string::npos, "the fixture finds the shipped offset");
    if (at == std::string::npos) return;
    const std::filesystem::path dir = Scratch("meet");
    for (const int offset : {74, 75}) {
        std::string edited = text;
        edited.replace(at, key.size(), "\"centre_offset_units\": " + std::to_string(offset) + ",");
        const std::filesystem::path file = dir / ("controls-" + std::to_string(offset) + ".json");
        std::ofstream(file, std::ios::binary) << edited;
        Options::Rules rules;
        std::string why;
        const bool loaded = Options::LoadRules(file.string(), s.controls, rules, why);
        if (offset == 74) CHECK_MSG(!loaded && why.find("centre_offset_units") != std::string::npos, "74 apart meet: refused, naming it: " + why);
        if (offset == 75) CHECK_MSG(loaded, "75 apart touch and do not meet: " + why);
    }
}

void TheSwitchesShowWhatIsOn() {
    const Screen s = LoadScreen();
    if (!s.ok) return;
    const glm::dvec2 view(256.0 * 16.0 / 9.0, 256.0);
    const auto pieces = [&](const Options::Rules& rules, bool full, bool walk) {
        return Options::Pieces(rules, s.credits, s.controls, s.hud, Controls::Tuning{}, view, 1.0e9, Controls::Style::Modern, Options::Widget::None, false, nullptr,
                               Options::Switches{full, walk});
    };
    for (const bool full : {false, true}) {
        for (const bool walk : {false, true}) {
            const std::string what = std::string("full ") + (full ? "on" : "off") + ", arrows " + (walk ? "on" : "off");
            int frames = 0;
            int marks[2] = {0, 0};
            int walks = 0;
            std::vector<std::string> words;
            for (const Options::Piece& piece : pieces(s.desktop, full, walk)) {
                const bool isFull = piece.widget == Options::Widget::Fullscreen;
                if (piece.element == Options::Element::SwitchFrame) {
                    ++frames;
                    const bool on = isFull ? full : walk;
                    CHECK_MSG(piece.colour == (on ? s.desktop.chosenFrame : s.desktop.idleFrame), what + ": a switch is lit while on");
                }
                if (piece.element == Options::Element::SwitchMark) ++marks[isFull ? 0 : 1];
                if (piece.element == Options::Element::ThumbnailControl && Controls::IsWalk(*piece.control)) ++walks;
                if (piece.element == Options::Element::Word || piece.element == Options::Element::Title) words.push_back(piece.words);
            }
            CHECK_EQ(frames, 2);
            CHECK_MSG(marks[0] == (full ? 5 : 4) && marks[1] == (walk ? 5 : 4), what + ": a mark is a square's four edges, filled while on");
            CHECK_MSG(walks == (walk ? 4 : 0), what + ": the thumbnails' walk arrows only while they are on: " + std::to_string(walks));
            const std::vector<std::string> expected = {"Options", "Modern", "Classic", "Fullscreen", "Walk arrows"};
            CHECK_MSG(words == expected, what + ": the title and the words");
        }
    }
    // A phone's screen: no switch, the old title, the walk arrows on both thumbnails whatever the switches say.
    int walks = 0;
    bool anySwitch = false;
    std::string title;
    for (const Options::Piece& piece : pieces(s.phone, true, false)) {
        anySwitch = anySwitch || piece.element == Options::Element::SwitchFrame || piece.element == Options::Element::SwitchMark;
        if (piece.element == Options::Element::ThumbnailControl && Controls::IsWalk(*piece.control)) ++walks;
        if (piece.element == Options::Element::Title) title = piece.words;
    }
    CHECK(!anySwitch);
    CHECK_EQ(walks, 4);
    CheckText(title, "Controls");
}

// ---- the layer -----------------------------------------------------------------------------------------------------------

MagicPortalsLayer::Paths PlayerPaths(const std::string& saves, bool desktop) {
    MagicPortalsLayer::Paths paths;
    paths.prisms = std::filesystem::temp_directory_path() / "supersonic-test-mp-desktop-prisms";
    paths.saveDir = saves;
    paths.controls = Controls::ResolvePolicy(std::nullopt, false, false, false); // a player's run
    paths.desktop = desktop;
    return paths;
}

void PublishViewport(entt::registry& registry) {
    ViewportInfo viewport;
    viewport.rect.min = glm::vec2(0.0f, 0.0f);
    viewport.rect.max = glm::vec2(1280.0f, 720.0f);
    viewport.pointerOverGame = true;
    registry.ctx().insert_or_assign(viewport);
}

void Tick(MagicPortalsLayer& layer, entt::registry& registry, const glm::vec2& pointer = kRest, std::vector<std::string> down = {},
          std::vector<std::string> pressed = {}, std::vector<std::string> released = {}) {
    PhysicsSystem::Update(registry, MagicPortalsLayer::kTick);
    Input::TickInput input;
    input.mousePosition = pointer;
    input.down = std::move(down);
    input.pressed = std::move(pressed);
    input.released = std::move(released);
    Input::BeginReplayedTick(input);
    layer.OnFixedUpdate(registry, MagicPortalsLayer::kTick);
    Input::EndReplayedTick();
}

glm::vec2 PixelOfView(const glm::dvec2& onView) {
    return glm::vec2(static_cast<float>(onView.x * kPxPerUnit), static_cast<float>(onView.y * kPxPerUnit));
}

void Click(MagicPortalsLayer& layer, entt::registry& registry, const glm::dvec2& onView) {
    const glm::vec2 px = PixelOfView(onView);
    Tick(layer, registry, px, {MagicPortalsLayer::kTap}, {MagicPortalsLayer::kTap});
    Tick(layer, registry, px, {}, {}, {MagicPortalsLayer::kTap});
    Tick(layer, registry);
}

double PlayerX(const MagicPortalsLayer& layer, const entt::registry& registry) {
    return Units::ToPixels(registry.get<TransformComponent>(layer.SimLevel()->player).position).x;
}

// Where the left walk arrow's hit rectangle is, from a run that shows it.
std::optional<glm::dvec2> LeftArrowAt() {
    entt::registry registry;
    PublishViewport(registry);
    MagicPortalsLayer layer(PlayerPaths(Scratch("arrow-saves").string(), false), "level0");
    layer.OnAttach(registry);
    CHECK_MSG(layer.SimLevel() != nullptr, layer.LoadError());
    if (layer.SimLevel() == nullptr) return std::nullopt;
    for (int tick = 0; tick < 120; ++tick) Tick(layer, registry);
    Hud::Rect rect;
    const bool shown = layer.ControlRect(MagicPortalsLayer::Control::Left, rect);
    CHECK_MSG(shown && layer.WalkPadsShown(), "a run that is not a desktop player's shows the walk arrows");
    CHECK_MSG(shown && layer.ControlAt(rect.Centre()) == MagicPortalsLayer::Control::Left, "and claims a touch on one");
    layer.OnDetach(registry);
    return shown ? std::optional<glm::dvec2>(rect.Centre()) : std::nullopt;
}

void ADesktopRunHasNoWalkArrowsAndTheKeysWalk() {
    const std::optional<glm::dvec2> arrow = LeftArrowAt();
    if (!arrow) return;
    entt::registry registry;
    PublishViewport(registry);
    MagicPortalsLayer layer(PlayerPaths(Scratch("hidden-saves").string(), true), "level0");
    layer.OnAttach(registry);
    CHECK_MSG(layer.SimLevel() != nullptr, layer.LoadError());
    if (layer.SimLevel() == nullptr) return;
    CHECK(!layer.WalkPadsShown());
    for (int tick = 0; tick < 120; ++tick) Tick(layer, registry);
    Hud::Rect rect;
    CHECK_MSG(!layer.ControlRect(MagicPortalsLayer::Control::Left, rect) && !layer.ControlRect(MagicPortalsLayer::Control::Right, rect),
              "no walk arrow on a desktop's level");
    CHECK_MSG(layer.ControlRect(MagicPortalsLayer::Control::Menu, rect) && layer.ControlRect(MagicPortalsLayer::Control::Reset, rect),
              "the pause and restart buttons stay: a mouse works them");
    CHECK_MSG(!layer.ControlAt(*arrow), "and nothing claims a touch where the arrow was");
    // A held press where the left arrow was walks nobody.
    const double before = PlayerX(layer, registry);
    const glm::vec2 px = PixelOfView(*arrow);
    Tick(layer, registry, px, {MagicPortalsLayer::kTap}, {MagicPortalsLayer::kTap});
    for (int tick = 0; tick < 30; ++tick) Tick(layer, registry, px, {MagicPortalsLayer::kTap});
    Tick(layer, registry, px, {}, {}, {MagicPortalsLayer::kTap});
    CHECK_MSG(std::fabs(PlayerX(layer, registry) - before) < 1.0, "a press there is the world's: he stood, " + std::to_string(PlayerX(layer, registry) - before));
    // The keys walk: A, then the right arrow.
    const double start = PlayerX(layer, registry);
    for (int tick = 0; tick < 30; ++tick) Tick(layer, registry, kRest, {MagicPortalsLayer::kLeftAlt});
    const double walked = PlayerX(layer, registry);
    CHECK_MSG(walked < start - 5.0, "A walks him left: " + std::to_string(walked - start));
    for (int tick = 0; tick < 30; ++tick) Tick(layer, registry, kRest, {MagicPortalsLayer::kRight});
    CHECK_MSG(PlayerX(layer, registry) > walked + 5.0, "the right arrow walks him right");
    layer.OnDetach(registry);
}

// Where the arrow was, a drag is the world's: it pans the camera, which it never does while the finger is on a walk arrow (Camera::Track's onPad).
void ADragWhereAnArrowWasPansTheCamera() {
    const std::optional<glm::dvec2> arrow = LeftArrowAt();
    if (!arrow) return;
    const auto cameraAfter = [&](bool drag) -> std::optional<glm::dvec2> {
        entt::registry registry;
        PublishViewport(registry);
        MagicPortalsLayer layer(PlayerPaths(Scratch("pan-saves").string(), true), "level6b");
        layer.OnAttach(registry);
        CHECK_MSG(layer.SimLevel() != nullptr, layer.LoadError());
        if (layer.SimLevel() == nullptr) return std::nullopt;
        for (int tick = 0; tick < 120; ++tick) Tick(layer, registry);
        // Twelve ticks of 1 unit each, inside the 32-unit square the arrow stood in.
        glm::dvec2 at = *arrow - glm::dvec2(6.0, -6.0);
        for (int tick = 0; tick < 12; ++tick) {
            if (drag) {
                Tick(layer, registry, PixelOfView(at), {MagicPortalsLayer::kTap}, tick == 0 ? std::vector<std::string>{MagicPortalsLayer::kTap} : std::vector<std::string>{});
                at += glm::dvec2(1.0, -1.0);
            } else {
                Tick(layer, registry);
            }
        }
        const glm::dvec2 centre = layer.CameraCentrePx();
        layer.OnDetach(registry);
        return centre;
    };
    const std::optional<glm::dvec2> still = cameraAfter(false);
    const std::optional<glm::dvec2> dragged = cameraAfter(true);
    if (!still || !dragged) return;
    CHECK_MSG(glm::distance(*still, *dragged) > 2.0, "the drag panned the camera by " + std::to_string(glm::distance(*still, *dragged)) + " units");
}

void TheKeysSwitchFullScreenAndTheChoiceIsKept() {
    const std::filesystem::path saves = Scratch("keys-saves");
    entt::registry registry;
    PublishViewport(registry);
    FakeWindow window;
    registry.ctx().insert_or_assign<WindowControl*>(&window);
    MagicPortalsLayer layer(PlayerPaths(saves.string(), true), "level0");
    layer.OnAttach(registry);
    CHECK_MSG(layer.SimLevel() != nullptr, layer.LoadError());
    CHECK_MSG(!std::filesystem::exists(saves / Desktop::kStoreFile), "nothing is written before a choice");
    const auto toggled = [&](std::vector<std::string> down, std::vector<std::string> pressed) {
        Tick(layer, registry, kRest, std::move(down), std::move(pressed));
        const bool asked = window.Apply();
        Tick(layer, registry);
        return asked;
    };
    CHECK_MSG(toggled({MagicPortalsLayer::kFullscreen}, {MagicPortalsLayer::kFullscreen}) && window.fullscreen, "F11 goes full screen");
    CheckText(Read(saves / Desktop::kStoreFile), "{\"version\":1,\"fullscreen\":true}");
    CHECK_MSG(toggled({MagicPortalsLayer::kFullscreen}, {MagicPortalsLayer::kFullscreen}) && !window.fullscreen, "F11 comes back");
    CHECK_MSG(toggled({MagicPortalsLayer::kAltKey, MagicPortalsLayer::kFullscreenEnter}, {MagicPortalsLayer::kFullscreenEnter}) && window.fullscreen,
              "Alt+Enter goes full screen");
    CHECK_MSG(!toggled({MagicPortalsLayer::kFullscreenEnter}, {MagicPortalsLayer::kFullscreenEnter}) && window.fullscreen, "Enter alone does nothing");
    CHECK_MSG(!toggled({MagicPortalsLayer::kAltKey}, {MagicPortalsLayer::kAltKey}), "nor an Alt alone");
    // A held F11 is one press.
    Tick(layer, registry, kRest, {MagicPortalsLayer::kFullscreen}, {MagicPortalsLayer::kFullscreen});
    window.Apply();
    for (int tick = 0; tick < 10; ++tick) Tick(layer, registry, kRest, {MagicPortalsLayer::kFullscreen});
    CHECK_MSG(!window.Apply() && !window.fullscreen, "held, it switched once");
    CheckText(Read(saves / Desktop::kStoreFile), "{\"version\":1,\"fullscreen\":false}");
    layer.OnDetach(registry);

    // The engine's own key codes: a raw F11 and a raw right Alt with the keypad's Enter, through the engine's bindings.
    for (const int variant : {0, 1, 2}) {
        entt::registry live;
        PublishViewport(live);
        FakeWindow liveWindow;
        live.ctx().insert_or_assign<WindowControl*>(&liveWindow);
        MagicPortalsLayer fresh(PlayerPaths(Scratch("live-saves").string(), true), "level0");
        fresh.OnAttach(live);
        for (int frame = 0; frame < 2; ++frame) {
            RawInputState raw;
            raw.mousePosition = kRest;
            Input::Update(raw);
            Input::BeginTickInput();
        }
        RawInputState raw;
        raw.mousePosition = kRest;
        if (variant == 0) {
            raw.keys[Desktop::kKeyF11] = true;
        } else if (variant == 1) {
            raw.keys[Key::RightAlt] = true;
            raw.keys[Desktop::kKeyKeypadEnter] = true;
        } else {
            raw.keys[Key::LeftAlt] = true;
            raw.keys[Key::Enter] = true;
        }
        Input::Update(raw);
        Input::BeginTickInput();
        PhysicsSystem::Update(live, MagicPortalsLayer::kTick);
        fresh.OnFixedUpdate(live, MagicPortalsLayer::kTick);
        CHECK_MSG(liveWindow.Apply() && liveWindow.fullscreen,
                  variant == 0   ? "a raw F11 goes full screen"
                  : variant == 1 ? "a raw right Alt with the keypad's Enter does"
                                 : "a raw left Alt with Enter does");
        fresh.OnDetach(live);
        for (int frame = 0; frame < 2; ++frame) {
            RawInputState rest;
            rest.mousePosition = kRest;
            Input::Update(rest);
            Input::BeginTickInput();
        }
    }

    // A run that is not a desktop player's hears none of it.
    entt::registry phone;
    PublishViewport(phone);
    FakeWindow phoneWindow;
    phone.ctx().insert_or_assign<WindowControl*>(&phoneWindow);
    const std::filesystem::path phoneSaves = Scratch("phone-saves");
    MagicPortalsLayer other(PlayerPaths(phoneSaves.string(), false), "level0");
    other.OnAttach(phone);
    Tick(other, phone, kRest, {MagicPortalsLayer::kFullscreen}, {MagicPortalsLayer::kFullscreen});
    Tick(other, phone, kRest, {MagicPortalsLayer::kAltKey, MagicPortalsLayer::kFullscreenEnter}, {MagicPortalsLayer::kFullscreenEnter});
    CHECK_MSG(!phoneWindow.Apply(), "no request on a run that is not a desktop player's");
    CHECK(!std::filesystem::exists(phoneSaves / Desktop::kStoreFile));
    other.OnDetach(phone);
}

void TheArrowsChoiceIsReadByTheNextRun() {
    const std::filesystem::path saves = Scratch("kept-saves");
    std::ofstream(saves / Desktop::kStoreFile, std::ios::binary) << "{\"version\":1,\"walk_pads\":true}";
    entt::registry registry;
    PublishViewport(registry);
    MagicPortalsLayer layer(PlayerPaths(saves.string(), true), "level0");
    layer.OnAttach(registry);
    CHECK_MSG(layer.SimLevel() != nullptr, layer.LoadError());
    CHECK(layer.WalkPadsShown());
    for (int tick = 0; tick < 120; ++tick) Tick(layer, registry);
    Hud::Rect rect;
    CHECK_MSG(layer.ControlRect(MagicPortalsLayer::Control::Left, rect) && layer.ControlRect(MagicPortalsLayer::Control::Right, rect),
              "a player who turned them on has them");
    layer.OnDetach(registry);
}

void TheOptionsSwitchesWork() {
    const std::filesystem::path saves = Scratch("options-saves");
    entt::registry registry;
    PublishViewport(registry);
    FakeWindow window;
    registry.ctx().insert_or_assign<WindowControl*>(&window);
    MagicPortalsLayer layer(PlayerPaths(saves.string(), true), "");
    layer.OnAttach(registry);
    CHECK_MSG(layer.LoadError().empty(), layer.LoadError());
    for (int tick = 0; tick < 400 && layer.MenuScreen() != MagicPortalsLayer::Screen::Main; ++tick) Tick(layer, registry);
    CHECK(layer.MenuScreen() == MagicPortalsLayer::Screen::Main && layer.OptionsAvailable());
    if (layer.MenuScreen() != MagicPortalsLayer::Screen::Main || !layer.OptionsAvailable()) return;
    for (int tick = 0; tick < 60; ++tick) Tick(layer, registry);
    CHECK_MSG(layer.OptionsRules().desktop.shown, "the desktop screen");
    const Hud::Rect entry = Options::EntryAt(layer.OptionsRules(), layer.MainMenuRules().layer, layer.MainMenuRules().state, layer.ViewPx(), 1.0e9, false).rect;
    Click(layer, registry, entry.Centre());
    for (int tick = 0; tick < 60; ++tick) Tick(layer, registry);
    CHECK(layer.MenuScreen() == MagicPortalsLayer::Screen::Options);
    if (layer.MenuScreen() != MagicPortalsLayer::Screen::Options) return;
    CHECK_MSG(!layer.AdjustAvailable(), "with no arrows on the picture there is nothing to adjust");

    // The plate the layer draws for a switch: the quad on its rectangle, and whether it is the lit colour.
    const auto lit = [&](Options::Widget widget) -> std::optional<bool> {
        static ScreenOverlay overlay;
        overlay.Clear();
        registry.ctx().insert_or_assign<ScreenOverlay*>(&overlay);
        layer.EmitMenu(registry);
        const glm::dvec2 view = layer.ViewPx();
        const Hud::Rect rect = Options::SwitchRect(layer.OptionsRules(), widget, view);
        const auto colour = [](const Options::Colour& c) {
            return glm::vec4(static_cast<float>(c[0]), static_cast<float>(c[1]), static_cast<float>(c[2]), static_cast<float>(c[3])) / 255.0f;
        };
        for (const ScreenOverlay::Quad& quad : overlay.Quads()) {
            if (glm::length(quad.min - glm::vec2(rect.min / view)) > 1e-4f || glm::length(quad.max - glm::vec2(rect.Max() / view)) > 1e-4f) continue;
            if (glm::length(quad.color - colour(layer.OptionsRules().chosenFrame)) < 1e-4f) return true;
            if (glm::length(quad.color - colour(layer.OptionsRules().idleFrame)) < 1e-4f) return false;
        }
        return std::nullopt;
    };
    CHECK_MSG(lit(Options::Widget::Fullscreen) == std::optional<bool>(false) && lit(Options::Widget::WalkPads) == std::optional<bool>(false),
              "both drawn off");

    Click(layer, registry, Options::SwitchRect(layer.OptionsRules(), Options::Widget::Fullscreen, layer.ViewPx()).Centre());
    CHECK_MSG(window.Apply() && window.fullscreen, "the full screen switch asks the window");
    CHECK_MSG(lit(Options::Widget::Fullscreen) == std::optional<bool>(true), "and is drawn on");
    Click(layer, registry, Options::SwitchRect(layer.OptionsRules(), Options::Widget::WalkPads, layer.ViewPx()).Centre());
    CHECK_MSG(layer.WalkPadsShown() && layer.AdjustAvailable(), "the walk arrows switch turns them on, and the adjust button with them");
    CHECK_MSG(lit(Options::Widget::WalkPads) == std::optional<bool>(true), "and is drawn on");
    CheckText(Read(saves / Desktop::kStoreFile), "{\"version\":1,\"fullscreen\":true,\"walk_pads\":true}");
    Click(layer, registry, Options::SwitchRect(layer.OptionsRules(), Options::Widget::WalkPads, layer.ViewPx()).Centre());
    Click(layer, registry, Options::SwitchRect(layer.OptionsRules(), Options::Widget::Fullscreen, layer.ViewPx()).Centre());
    CHECK_MSG(window.Apply() && !window.fullscreen && !layer.WalkPadsShown(), "and both switch back");
    CheckText(Read(saves / Desktop::kStoreFile), "{\"version\":1,\"fullscreen\":false,\"walk_pads\":false}");
    layer.OnDetach(registry);
}

void runPureTests() {
    TheShippedDefaultsAndTheirRefusals();
    TheKeysAndTheStart();
    TheFile();
    TheDesktopScreenOnEveryShape();
    TheDesktopScreenWithTheRateRowOnEveryShape();
    TheSwitchesMayNotMeet();
    TheSwitchesShowWhatIsOn();
}

// --controls classic on a desktop: no Options screen to switch them back, so the arrows stay. A player's saved Classic style has the screen, and hides them.
void ARunWithoutOptionsKeepsItsArrows() {
    for (const bool flag : {true, false}) {
        const std::filesystem::path saves = Scratch(flag ? "classic-flag-saves" : "classic-saved-saves");
        if (!flag) std::ofstream(saves / "controls.json", std::ios::binary) << "{\"version\":1,\"controls\":{\"mode\":\"classic\"}}";
        MagicPortalsLayer::Paths paths = PlayerPaths(saves.string(), true);
        if (flag) paths.controls = Controls::ResolvePolicy(std::string("classic"), false, false, false);
        entt::registry registry;
        PublishViewport(registry);
        MagicPortalsLayer layer(paths, "level0");
        layer.OnAttach(registry);
        CHECK_MSG(layer.SimLevel() != nullptr, layer.LoadError());
        if (layer.SimLevel() == nullptr) return;
        for (int tick = 0; tick < 120; ++tick) Tick(layer, registry);
        Hud::Rect rect;
        const bool left = layer.ControlRect(MagicPortalsLayer::Control::Left, rect);
        const bool restart = layer.ControlRect(MagicPortalsLayer::Control::Reset, rect);
        CHECK_MSG(restart, "the classic restart is there");
        if (flag) {
            CHECK_MSG(!layer.OptionsAvailable() && layer.WalkPadsShown() && left, "--controls classic keeps the original's arrows");
        } else {
            CHECK_MSG(layer.OptionsAvailable() && !layer.WalkPadsShown() && !left, "a saved Classic style hides them, as Modern does");
        }
        layer.OnDetach(registry);
    }
}

void TheKeysAreSaidOnTheTutorialLevel() {
    struct Case {
        const char* level;
        bool desktop;
        bool arrows;
        bool said;
    };
    for (const Case& c : {Case{"level0", true, false, true}, Case{"level0", true, true, false}, Case{"level0", false, false, false},
                          Case{"level1", true, false, false}}) {
        const std::filesystem::path saves = Scratch("hint-saves");
        if (c.arrows) std::ofstream(saves / Desktop::kStoreFile, std::ios::binary) << "{\"walk_pads\":true}";
        entt::registry registry;
        PublishViewport(registry);
        MagicPortalsLayer layer(PlayerPaths(saves.string(), c.desktop), c.level);
        layer.OnAttach(registry);
        CHECK_MSG(layer.SimLevel() != nullptr, layer.LoadError());
        if (layer.SimLevel() == nullptr) return;
        for (int tick = 0; tick < 120; ++tick) Tick(layer, registry);
        const std::string what = std::string(c.level) + (c.desktop ? " on a desktop" : " on a phone") + (c.arrows ? " with arrows" : "");
        CHECK_MSG(layer.WalkHintWords() == (c.said ? std::string("Walk with the arrow keys") : std::string()),
                  what + ": says \"" + layer.WalkHintWords() + "\"");
        // And drawn: glyphs of the original's font along the bottom, only where it is said.
        static ScreenOverlay overlay;
        overlay.Clear();
        registry.ctx().insert_or_assign<ScreenOverlay*>(&overlay);
        layer.EmitHud(registry);
        int low = 0;
        for (const ScreenOverlay::Quad& quad : overlay.Quads()) {
            if (quad.min.y > 0.86f && quad.max.y < 1.0f && quad.texture.find("Matura") != std::string::npos) ++low;
        }
        if (c.said) CHECK_MSG(low >= 18, what + ": the words' letters along the bottom: " + std::to_string(low));
        // On 1-01 alone: 1-02 opens a popup of its own, whose words are not this rule's.
        if (!c.said && std::string(c.level) == "level0") CHECK_MSG(low == 0, what + ": no letters along the bottom: " + std::to_string(low));
        layer.OnDetach(registry);
    }
}

// F11 or Alt+Enter during the intro changes the window and leaves the intro playing; any other key still skips it.
void TheFullScreenKeysDoNotSkipTheIntro() {
    for (const int variant : {0, 1, 2, 3}) {
        entt::registry registry;
        PublishViewport(registry);
        FakeWindow window;
        registry.ctx().insert_or_assign<WindowControl*>(&window);
        MagicPortalsLayer::Paths paths = PlayerPaths(Scratch("intro-saves").string(), true);
        paths.splash = true;
        MagicPortalsLayer layer(paths, "level0");
        layer.OnAttach(registry);
        CHECK(layer.SplashUp());
        const auto live = [&](int tick) {
            RawInputState raw;
            raw.mousePosition = kRest;
            if (tick >= 31) {
                if (variant == 0) raw.keys[Desktop::kKeyF11] = true;
                if (variant == 1) {
                    raw.keys[Key::LeftAlt] = true;
                    raw.keys[Key::Enter] = tick >= 32;
                }
                // Alt+Enter with the Alt let go first and Enter held on: still no skip.
                if (variant == 3) {
                    raw.keys[Key::LeftAlt] = tick < 40;
                    raw.keys[Key::Enter] = tick >= 32;
                }
                if (variant == 2) raw.keys[Key::Space] = true;
            }
            Input::Update(raw);
            Input::BeginTickInput();
            PhysicsSystem::Update(registry, MagicPortalsLayer::kTick);
            layer.OnFixedUpdate(registry, MagicPortalsLayer::kTick);
        };
        int over = 0;
        for (int tick = 1; tick <= 200 && over == 0; ++tick) {
            live(tick);
            if (!layer.SplashUp()) over = tick;
        }
        for (int frame = 0; frame < 2; ++frame) {
            RawInputState rest;
            rest.mousePosition = kRest;
            Input::Update(rest);
            Input::BeginTickInput();
        }
        if (variant == 2) {
            CHECK_MSG(over > 0 && over < 80, "Space skips the intro: over on tick " + std::to_string(over));
        } else {
            CHECK_MSG(over > 100, std::string(variant == 0 ? "F11" : variant == 1 ? "Alt+Enter" : "Alt+Enter with Alt let go first") +
                                      " leaves it playing: over on tick " + std::to_string(over));
            CHECK_MSG(window.Apply() && window.fullscreen, "and goes full screen");
        }
        layer.OnDetach(registry);
    }
}

void runLayerTests() {
    ARunWithoutOptionsKeepsItsArrows();
    TheKeysAreSaidOnTheTutorialLevel();
    TheFullScreenKeysDoNotSkipTheIntro();
    ADesktopRunHasNoWalkArrowsAndTheKeysWalk();
    ADragWhereAnArrowWasPansTheCamera();
    TheKeysSwitchFullScreenAndTheChoiceIsKept();
    TheArrowsChoiceIsReadByTheNextRun();
    TheOptionsSwitchesWork();
}

int pureEntry(int, char**) {
    runPureTests();
    return ::test::summary("test_mp_desktop", 100);
}

// The layer's cases, a suite of their own that exits 77 without the owner's data (CMakeLists: IN_SOURCE).
int levelsEntry(int, char**) {
    std::error_code ec;
    if (!std::filesystem::is_directory(kLevels, ec) || !std::filesystem::is_regular_file(kChapters, ec) ||
        !std::filesystem::is_regular_file(kData + "/entity_roles.json", ec)) {
        std::printf("test_mp_desktop_levels: SKIPPED - needs the converted levels at %s, chapters.json at %s and Enhanced's data at %s\n", kLevels.c_str(),
                    kChapters.c_str(), kData.c_str());
        return 77;
    }
    runLayerTests();
    return ::test::summary("test_mp_desktop_levels", 30);
}

const ::MpSuite::Registration mpPureRegistration("test_mp_desktop", &pureEntry);
const ::MpSuite::Registration mpLevelsRegistration("test_mp_desktop_levels", &levelsEntry);

} // namespace
