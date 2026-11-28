// A player's frame rate (planning step 285; the owner's request of 2026-11-28 for a choice beside the display's own rate). sim/FrameRate is the rule and the
// file; the LAYER is what a player meets, and what it asks of the engine (core/FrameRateControl) and the window (core/WindowControl).
//
// WHAT IS PINNED:
//   - controls.json's `frame_rate` block as shipped (Auto, the three lists, the tolerance), and its refusals by name;
//   - the choices a platform offers (a desktop's held to its monitor), the cycle a press walks, what each choice asks per platform (Auto nothing; a desktop's
//     number dropped where the display paces it already; Android's refresh requests; iOS a cap only), and whether the row would do anything at all;
//   - framerate.json: kept, read back, a file that cannot be used refused and written over, nothing chosen writes nothing, an empty directory memory-only;
//   - whose run reads it (RunKind's FrameRateRun): a person's, --saves included; never --fixed-step, --frames, a capture, a recording, a replay, a route, a
//     level visit, a scripted input or --hidden;
//   - the layer: a bare layer (every suite's) and a run the engine does not cap read, ask and show nothing; Auto asks nothing, ever; the Options row cycles,
//     keeps the choice and asks for it; a desktop, Android and iOS each ask their own; a phone's monitor is never read; the monitor is asked again when
//     Options opens and when full screen is switched; --controls classic applies the choice with no row; a bad block costs this setting alone;
//   - main.cpp's own wiring, as text: the two lines that give a run the setting and the platform of the build (main cannot be linked into a suite).
//
// NOT TESTED here: the engine's pacer (the engine's test_framepacer), the platform constant as a phone's compiler sees it (its text alone is held), and any
// real display above 60 Hz or any phone.

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "MagicPortalsLayer.hpp"
#include "RunKind.hpp"
#include "sim/Controls.hpp"
#include "sim/FrameRate.hpp"
#include "sim/Options.hpp"

#include "core/FrameRateControl.hpp"
#include "core/Input.hpp"
#include "core/LaunchOptions.hpp"
#include "core/PhysicsSystem.hpp"
#include "core/ScreenOverlay.hpp"
#include "core/ViewportInfo.hpp"
#include "core/WindowControl.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#ifndef MAGICPORTALS_SOURCE_DIR
#error "test_mp_framerate.cpp is given MAGICPORTALS_SOURCE_DIR by tests/CMakeLists.txt"
#endif

using namespace Supersonic;
using MagicPortals::MagicPortalsLayer;
namespace FrameRate = MagicPortals::FrameRate;
namespace Controls = MagicPortals::Controls;
namespace Options = MagicPortals::Options;
namespace Hud = MagicPortals::Hud;
using FrameRate::Choice;
using FrameRate::Platform;

namespace {

const std::string kLevels = MAGICPORTALS_LEVELS_DIR;
const std::string kData = MAGICPORTALS_DATA_DIR;
const std::string kChapters = MAGICPORTALS_CHAPTERS_FILE;
const std::string kPortData = MAGICPORTALS_PORT_DATA_DIR;
const std::string kControls = kPortData + "/controls.json";
const glm::vec2 kRest(640.0f, 360.0f);
constexpr double kPxPerUnit = 720.0 / 256.0;
constexpr std::uint32_t kHighest = WindowControl::kHighestRefreshRate;
constexpr std::uint32_t kWithdraw = WindowControl::kDesktopRefreshRate;

std::filesystem::path Scratch(const std::string& name) {
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / ("supersonic-test-mp-framerate-" + name);
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

void Write(const std::filesystem::path& path, const std::string& text) {
    std::ofstream(path, std::ios::binary | std::ios::trunc) << text;
}

void CheckText(const std::string& got, const std::string& expected) {
    CHECK_MSG(got == expected, "got " + got + ", expected " + expected);
}

std::string Names(const std::vector<Choice>& choices) {
    std::string text;
    for (const Choice& choice : choices) text += (text.empty() ? "" : " ") + FrameRate::NameOf(choice);
    return text;
}

FrameRate::Rules Shipped() {
    FrameRate::Rules rules;
    std::string error;
    CHECK_MSG(FrameRate::LoadRules(kControls, rules, error), error);
    return rules;
}

// ---- the data ---------------------------------------------------------------------------------------------------------------

void TheShippedBlockAndItsRefusals() {
    const FrameRate::Rules rules = Shipped();
    CHECK_MSG(rules.fallback == Choice::Auto(), "a player who has chosen nothing plays Auto");
    CheckText(Names(rules.desktop), "30 60 90 120 144 165 240");
    CheckText(Names(rules.android), "30 60 max");
    CheckText(Names(rules.ios), "30 60");
    CHECK(rules.refreshToleranceHz == 1.0);
    CHECK_EQ(rules.androidLowRequestHz, 60);
    CHECK(rules.smoothWhenLimited);

    const std::filesystem::path dir = Scratch("rules");
    const std::string good = "{\"frame_rate\": {\"default\": 60, \"desktop_choices\": [60], \"android_choices\": [\"max\"], \"ios_choices\": [], "
                             "\"refresh_tolerance_hz\": 0, \"android_low_request_hz\": 90, \"smooth_when_limited\": false}}";
    Write(dir / "good.json", good);
    FrameRate::Rules edited;
    std::string why;
    CHECK_MSG(FrameRate::LoadRules((dir / "good.json").string(), edited, why), why);
    CHECK_MSG(edited.fallback == Choice::Of(60) && edited.ios.empty() && Names(edited.android) == "max" && !edited.smoothWhenLimited,
              "a number as the default, an empty list, max alone");

    const auto refused = [&](const std::string& from, const std::string& to, const char* named) {
        std::string text = good;
        const std::size_t at = text.find(from);
        CHECK_MSG(at != std::string::npos, "the edit applies: " + from);
        if (at == std::string::npos) return;
        text.replace(at, from.size(), to);
        Write(dir / "bad.json", text);
        FrameRate::Rules out = rules;
        std::string error;
        CHECK_MSG(!FrameRate::LoadRules((dir / "bad.json").string(), out, error) && error.find(named) != std::string::npos, to + " is refused by name: " + error);
        CHECK_MSG(out.desktop == rules.desktop && out.fallback == rules.fallback, "and what was held is left");
    };
    refused("\"frame_rate\":", "\"frame_rates\":", "frame_rate");
    refused("\"default\": 60", "\"default\": 5", "default");
    refused("\"default\": 60", "\"default\": \"fast\"", "default");
    refused("\"default\": 60", "\"default\": 60.5", "default");
    refused("\"desktop_choices\": [60]", "\"desktop_choices\": [60, 30]", "desktop_choices");
    refused("\"desktop_choices\": [60]", "\"desktop_choices\": [60, 60]", "desktop_choices");
    refused("\"desktop_choices\": [60]", "\"desktop_choices\": [\"max\"]", "desktop_choices");
    refused("\"desktop_choices\": [60]", "\"desktop_choices\": [\"auto\"]", "desktop_choices");
    refused("\"desktop_choices\": [60]", "\"desktop_choices\": [2000]", "desktop_choices");
    refused("\"desktop_choices\": [60]", "\"desktop_choices\": 60", "desktop_choices");
    refused("\"android_choices\": [\"max\"]", "\"android_choices\": [\"max\", 60]", "android_choices");
    refused("\"ios_choices\": []", "\"ios_choices\": [\"max\"]", "ios_choices");
    refused("\"ios_choices\": []", "\"unused\": []", "ios_choices");
    refused("\"refresh_tolerance_hz\": 0", "\"refresh_tolerance_hz\": -1", "refresh_tolerance_hz");
    refused("\"refresh_tolerance_hz\": 0", "\"refresh_tolerance_hz\": 11", "refresh_tolerance_hz");
    refused("\"android_low_request_hz\": 90", "\"android_low_request_hz\": 0", "android_low_request_hz");
    refused("\"smooth_when_limited\": false", "\"smooth_when_limited\": 0", "smooth_when_limited");
}

// ---- what a choice asks --------------------------------------------------------------------------------------------------------

void TheChoicesOfferedAndTheCycle() {
    const FrameRate::Rules rules = Shipped();
    CheckText(Names(FrameRate::Offered(rules, Platform::Desktop, 60)), "30 60");
    CheckText(Names(FrameRate::Offered(rules, Platform::Desktop, 59)), "30 60"); // 59.94 as some desktops report it
    CheckText(Names(FrameRate::Offered(rules, Platform::Desktop, 75)), "30 60");
    CheckText(Names(FrameRate::Offered(rules, Platform::Desktop, 144)), "30 60 90 120 144");
    CheckText(Names(FrameRate::Offered(rules, Platform::Desktop, 360)), "30 60 90 120 144 165 240");
    CheckText(Names(FrameRate::Offered(rules, Platform::Desktop, 0)), "30 60 90 120 144 165 240"); // unknown: all
    CheckText(Names(FrameRate::Offered(rules, Platform::Desktop, 24)), "");
    CheckText(Names(FrameRate::Offered(rules, Platform::Android, 60)), "30 60 max");
    CheckText(Names(FrameRate::Offered(rules, Platform::Ios, 0)), "30 60");

    const auto cycle = [&](Platform platform, int refresh) {
        std::string walked;
        Choice choice = Choice::Auto();
        for (int press = 0; press < 12; ++press) {
            choice = FrameRate::Next(choice, FrameRate::Offered(rules, platform, refresh));
            walked += (walked.empty() ? "" : " ") + FrameRate::NameOf(choice);
            if (choice == Choice::Auto()) break;
        }
        return walked;
    };
    CheckText(cycle(Platform::Desktop, 60), "30 60 auto");
    CheckText(cycle(Platform::Desktop, 144), "30 60 90 120 144 auto");
    CheckText(cycle(Platform::Android, 0), "30 60 max auto");
    CheckText(cycle(Platform::Ios, 0), "30 60 auto");
    // A choice kept from another monitor goes on upwards, else back to Auto.
    const std::vector<Choice> sixty = FrameRate::Offered(rules, Platform::Desktop, 60);
    CHECK(FrameRate::Next(Choice::Of(144), sixty) == Choice::Auto());
    CHECK(FrameRate::Next(Choice::Of(45), sixty) == Choice::Of(60));
    CHECK(FrameRate::Next(Choice::Max(), sixty) == Choice::Auto());
    CHECK(FrameRate::Next(Choice::Of(90), FrameRate::Offered(rules, Platform::Android, 0)) == Choice::Max());
    CHECK(FrameRate::Next(Choice::Auto(), {}) == Choice::Auto());
}

void WhatEachPlatformAsks() {
    const FrameRate::Rules rules = Shipped();
    const auto asks = [&](Platform platform, const Choice& choice, int refresh, bool paced) {
        return FrameRate::RequestFor(rules, platform, choice, refresh, paced);
    };
    const FrameRate::Request nothing;
    for (const Platform platform : {Platform::Desktop, Platform::Android, Platform::Ios}) {
        for (const int refresh : {0, 60, 144}) {
            for (const bool paced : {false, true}) {
                CHECK_MSG(asks(platform, Choice::Auto(), refresh, paced) == nothing, "Auto asks nothing anywhere");
            }
        }
    }
    // A desktop: a cap, dropped where the display paces the loop at that rate or under it already.
    CHECK_EQ(asks(Platform::Desktop, Choice::Of(30), 60, true).cap, 30);
    CHECK_EQ(asks(Platform::Desktop, Choice::Of(60), 60, true).cap, 0);
    CHECK_EQ(asks(Platform::Desktop, Choice::Of(60), 59, true).cap, 0);
    CHECK_EQ(asks(Platform::Desktop, Choice::Of(60), 61, true).cap, 0);
    CHECK_EQ(asks(Platform::Desktop, Choice::Of(60), 62, true).cap, 60);
    CHECK_EQ(asks(Platform::Desktop, Choice::Of(144), 60, true).cap, 0);
    CHECK_EQ(asks(Platform::Desktop, Choice::Of(60), 144, true).cap, 60);
    CHECK_MSG(asks(Platform::Desktop, Choice::Of(60), 60, false).cap == 60, "Mailbox: nothing paces it, so the cap is asked");
    CHECK_MSG(asks(Platform::Desktop, Choice::Of(60), 0, true).cap == 60, "an unknown refresh: the cap is asked");
    CHECK_EQ(asks(Platform::Desktop, Choice::Max(), 60, false).cap, 0);
    for (const Choice& choice : {Choice::Of(30), Choice::Of(60), Choice::Max()}) {
        CHECK_MSG(!asks(Platform::Desktop, choice, 60, true).displayHz, "a desktop's display is left to itself");
    }
    // Android: a cap and the display asked for 60 (or the number, if higher); max: no cap, the highest.
    CHECK((asks(Platform::Android, Choice::Of(30), 0, true) == FrameRate::Request{30, 60u}));
    CHECK((asks(Platform::Android, Choice::Of(60), 0, true) == FrameRate::Request{60, 60u}));
    CHECK((asks(Platform::Android, Choice::Of(90), 0, true) == FrameRate::Request{90, 90u}));
    CHECK((asks(Platform::Android, Choice::Max(), 0, true) == FrameRate::Request{0, kHighest}));
    // iOS: a cap only, whatever the window says of the screen.
    CHECK((asks(Platform::Ios, Choice::Of(30), 60, true) == FrameRate::Request{30, std::nullopt}));
    CHECK((asks(Platform::Ios, Choice::Of(60), 60, true) == FrameRate::Request{60, std::nullopt}));
    CHECK(asks(Platform::Ios, Choice::Max(), 0, true) == nothing);

    // The row is shown only where it would change something.
    CHECK(FrameRate::DoesSomething(rules, Platform::Desktop, Choice::Auto(), 60, true));
    CHECK_MSG(!FrameRate::DoesSomething(rules, Platform::Desktop, Choice::Auto(), 30, true), "30 Hz FIFO: its one choice is paced already");
    CHECK_MSG(!FrameRate::DoesSomething(rules, Platform::Desktop, Choice::Of(60), 30, true), "and a kept 60 asks nothing there either");
    CHECK_MSG(FrameRate::DoesSomething(rules, Platform::Desktop, Choice::Of(60), 30, false), "Mailbox: a kept 60 is a cap");
    CHECK(FrameRate::DoesSomething(rules, Platform::Android, Choice::Auto(), 0, true));
    CHECK(FrameRate::DoesSomething(rules, Platform::Ios, Choice::Auto(), 0, true));
    FrameRate::Rules none;
    CHECK_MSG(!FrameRate::DoesSomething(none, Platform::Android, Choice::Auto(), 0, true), "nothing offered: no row");
}

// ---- the file --------------------------------------------------------------------------------------------------------------

void TheFile() {
    const std::filesystem::path dir = Scratch("store");
    const std::filesystem::path file = dir / FrameRate::kStoreFile;
    std::string error;
    FrameRate::Store store;
    CHECK_MSG(store.Open(dir.string(), error), error);
    CHECK(!store.Chosen() && store.Persists());
    CHECK_MSG(store.Save(error) && !std::filesystem::exists(file), "nothing chosen writes nothing");
    store.SetChoice(Choice::Of(30));
    CHECK_MSG(store.Save(error), error);
    CheckText(Read(file), "{\"version\":1,\"frame_rate\":30}");
    store.SetChoice(Choice::Max());
    CHECK_MSG(store.Save(error), error);
    CheckText(Read(file), "{\"version\":1,\"frame_rate\":\"max\"}");
    store.SetChoice(Choice::Auto());
    CHECK_MSG(store.Save(error), error);
    CheckText(Read(file), "{\"version\":1,\"frame_rate\":\"auto\"}");
    FrameRate::Store back;
    CHECK_MSG(back.Open(dir.string(), error), error);
    CHECK_MSG(back.Chosen() == std::optional<Choice>(Choice::Auto()), "Auto chosen is a choice, kept");
    // A file without a version reads; a choice of a monitor this run does not have is kept as it is.
    Write(file, "{\"frame_rate\": 240}");
    CHECK_MSG(back.Open(dir.string(), error), error);
    CHECK(back.Chosen() == std::optional<Choice>(Choice::Of(240)));
    for (const char* bad : {"[1", "[]", "{}", "{\"frame_rate\": 5}", "{\"frame_rate\": \"fast\"}", "{\"frame_rate\": true}", "{\"frame_rate\": 30.5}",
                            "{\"version\":2,\"frame_rate\":30}", "{\"version\":\"1\",\"frame_rate\":30}"}) {
        Write(file, bad);
        CHECK_MSG(!back.Open(dir.string(), error) && !error.empty() && !back.Chosen(), std::string(bad) + " is refused: " + error);
        CHECK_MSG(back.Save(error) && Read(file) == bad, std::string(bad) + " is left until the player chooses");
        back.SetChoice(Choice::Of(60));
        CHECK_MSG(back.Save(error), error);
        CheckText(Read(file), "{\"version\":1,\"frame_rate\":60}");
    }
    FrameRate::Store memory;
    CHECK_MSG(memory.Open("", error), error);
    memory.SetChoice(Choice::Of(30));
    CHECK_MSG(memory.Save(error) && !memory.Persists(), "memory only: nothing written");
}

// ---- whose run ------------------------------------------------------------------------------------------------------------------

void WhoseRunReadsIt() {
    const LaunchOptions player;
    CHECK(MagicPortals::FrameRateRun(player, MagicPortals::DevFlags{}));
    MagicPortals::DevFlags saves;
    saves.saves = true;
    CHECK_MSG(MagicPortals::FrameRateRun(player, saves), "--saves alone is a run somebody plays");
    CHECK_MSG(!MagicPortals::AttendedRun(player, saves), "though not one that may open a message box");
    const auto engine = [](void (*set)(LaunchOptions&)) {
        LaunchOptions options;
        set(options);
        return options;
    };
    const std::pair<const char*, LaunchOptions> never[] = {
        {"--fixed-step", engine([](LaunchOptions& o) { o.fixedDelta = 1.0f / 60.0f; })},
        {"--frames", engine([](LaunchOptions& o) { o.maxFrames = 120; })},
        {"--screenshot", engine([](LaunchOptions& o) { o.screenshotPath = "shot.png"; })},
        {"--screenshot-ui", engine([](LaunchOptions& o) { o.screenshotUiPath = "ui.png"; })},
        {"--screenshot-every", engine([](LaunchOptions& o) { o.screenshotEvery = 10; })},
        {"--record", engine([](LaunchOptions& o) { o.recordPath = "run.rec"; })},
        {"--replay", engine([](LaunchOptions& o) { o.replayPath = "run.rec"; })},
        {"--hidden", engine([](LaunchOptions& o) { o.hidden = true; })},
    };
    for (const auto& [name, options] : never) {
        CHECK_MSG(!MagicPortals::FrameRateRun(options, MagicPortals::DevFlags{}), std::string(name) + " never reads the setting");
        CHECK_MSG(!MagicPortals::FrameRateRun(options, saves), std::string(name) + " with --saves neither");
    }
    for (int flag = 0; flag < 4; ++flag) {
        MagicPortals::DevFlags dev = saves;
        if (flag == 0) dev.route = true;
        if (flag == 1) dev.visit = true;
        if (flag == 2) dev.scripted = true;
        if (flag == 3) dev.lightMasksOff = true;
        CHECK_MSG(!MagicPortals::FrameRateRun(player, dev), "developer flag " + std::to_string(flag) + " never reads it");
    }
    CHECK_MSG(!MagicPortalsLayer::Paths{}.frameRate.available, "a bare layer's paths have no setting");
}

// Nothing but main switches the setting on, and every suite above builds its Policy by hand: without these lines no player has the setting and every other
// check stands. The platform constant's two phone branches are compiled by no host here.
// MUTATION: either of main's two lines removed or commented out; a platform given another's constant; TargetConditionals.h left out (TARGET_OS_IPHONE would
// then be undefined, and an iPhone a desktop).
void MainSwitchesTheSettingOn() {
    std::string source = Read(std::filesystem::path(MAGICPORTALS_SOURCE_DIR) / "game" / "main.cpp");
    source.erase(std::remove(source.begin(), source.end(), '\r'), source.end());
    CHECK_MSG(!source.empty(), "game/main.cpp is read");
    const auto once = [&](const std::string& text) {
        const std::size_t at = source.find(text);
        CHECK_MSG(at != std::string::npos && at == source.rfind(text), "main.cpp holds once:\n" + text);
    };
    once("\n        paths.frameRate.available = MagicPortals::FrameRateRun(options, dev);\n");
    once("\n        paths.frameRate.platform = kFrameRatePlatform;\n");
    once("\n#if defined(__APPLE__)\n#include <TargetConditionals.h>");
    once("\n#if defined(__ANDROID__)\n"
         "constexpr MagicPortals::FrameRate::Platform kFrameRatePlatform = MagicPortals::FrameRate::Platform::Android;\n"
         "#elif defined(__APPLE__) && TARGET_OS_IPHONE\n"
         "constexpr MagicPortals::FrameRate::Platform kFrameRatePlatform = MagicPortals::FrameRate::Platform::Ios;\n"
         "#else\n"
         "constexpr MagicPortals::FrameRate::Platform kFrameRatePlatform = MagicPortals::FrameRate::Platform::Desktop;\n"
         "#endif\n");
}

// ---- the layer -------------------------------------------------------------------------------------------------------------------

// The window: the monitor's rate it answers with, how often it was asked, and the refresh requests the layer made.
class FakeWindow final : public WindowControl {
public:
    std::uint32_t refresh = 60;
    mutable int modeAsked = 0;
    bool fullscreen = false;
    bool IsFullscreen() const override { return fullscreen; }
    glm::uvec2 WindowSize() const override { return glm::uvec2(1280u, 720u); }
    std::vector<DisplayMode> DisplayModes() const override { return {}; }
    DisplayMode DesktopMode() const override {
        ++modeAsked;
        return DisplayMode{1920u, 1080u, refresh};
    }
    // The refresh rate asked for since the last call, if one was; the requests are applied (full screen taken).
    std::optional<std::uint32_t> TakeRefresh() {
        const Requests requests = TakeRequests();
        if (requests.setFullscreen) fullscreen = requests.fullscreen;
        return requests.setRefreshRate ? std::optional<std::uint32_t>(requests.refreshRate) : std::nullopt;
    }
};

// A run as the app makes one: the viewport, the engine's frame-rate control (available, FIFO unless a case says) and the window.
struct Rig {
    entt::registry registry;
    FrameRateControl control;
    FakeWindow window;
    ScreenOverlay overlay;
    explicit Rig(const std::string& present = "Fifo", bool available = true) {
        ViewportInfo viewport;
        viewport.rect.min = glm::vec2(0.0f, 0.0f);
        viewport.rect.max = glm::vec2(1280.0f, 720.0f);
        viewport.pointerOverGame = true;
        registry.ctx().insert_or_assign(viewport);
        control.Publish(available, 0, present);
        registry.ctx().insert_or_assign<FrameRateControl*>(&control);
        registry.ctx().insert_or_assign<WindowControl*>(&window);
        registry.ctx().insert_or_assign<ScreenOverlay*>(&overlay);
    }
    // The cap asked since the last call, if one was.
    std::optional<int> TakeCap() {
        int fps = -1;
        return control.TakeRequest(&fps) ? std::optional<int>(fps) : std::nullopt;
    }
};

MagicPortalsLayer::Paths RatePaths(const std::string& saves, Platform platform) {
    MagicPortalsLayer::Paths paths;
    paths.prisms = std::filesystem::temp_directory_path() / "supersonic-test-mp-framerate-prisms";
    paths.saveDir = saves;
    paths.controls = Controls::ResolvePolicy(std::nullopt, false, false, false); // a player's run
    paths.frameRate = FrameRate::Policy{true, platform};
    return paths;
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

void Click(MagicPortalsLayer& layer, entt::registry& registry, const glm::dvec2& onView) {
    const glm::vec2 px(static_cast<float>(onView.x * kPxPerUnit), static_cast<float>(onView.y * kPxPerUnit));
    Tick(layer, registry, px, {MagicPortalsLayer::kTap}, {MagicPortalsLayer::kTap});
    Tick(layer, registry, px, {}, {}, {MagicPortalsLayer::kTap});
    Tick(layer, registry);
}

bool OpenOptions(MagicPortalsLayer& layer, entt::registry& registry) {
    for (int tick = 0; tick < 400 && layer.MenuScreen() != MagicPortalsLayer::Screen::Main; ++tick) Tick(layer, registry);
    CHECK_MSG(layer.MenuScreen() == MagicPortalsLayer::Screen::Main && layer.OptionsAvailable(), "the main menu with Options: " + layer.LoadError());
    if (layer.MenuScreen() != MagicPortalsLayer::Screen::Main || !layer.OptionsAvailable()) return false;
    for (int tick = 0; tick < 60; ++tick) Tick(layer, registry);
    const Hud::Rect entry = Options::EntryAt(layer.OptionsRules(), layer.MainMenuRules().layer, layer.MainMenuRules().state, layer.ViewPx(), 1.0e9, false).rect;
    Click(layer, registry, entry.Centre());
    for (int tick = 0; tick < 60; ++tick) Tick(layer, registry);
    CHECK_MSG(layer.MenuScreen() == MagicPortalsLayer::Screen::Options, "Options opens");
    return layer.MenuScreen() == MagicPortalsLayer::Screen::Options;
}

void PressBack(MagicPortalsLayer& layer, entt::registry& registry) {
    Tick(layer, registry, kRest, {MagicPortalsLayer::kBack}, {MagicPortalsLayer::kBack});
    for (int tick = 0; tick < 60; ++tick) Tick(layer, registry);
}

void PressRow(MagicPortalsLayer& layer, entt::registry& registry) {
    CHECK(layer.OptionsRules().rateRow.shown);
    Click(layer, registry, Options::RateRowRect(layer.OptionsRules(), layer.ViewPx()).Centre());
}

// Whether the row's plate is among the menu's quads.
bool RowDrawn(Rig& rig, const MagicPortalsLayer& layer) {
    rig.overlay.Clear();
    layer.EmitMenu(rig.registry);
    const glm::dvec2 view = layer.ViewPx();
    const Hud::Rect rect = Options::RateRowRect(layer.OptionsRules(), view);
    for (const ScreenOverlay::Quad& quad : rig.overlay.Quads()) {
        if (glm::length(quad.min - glm::vec2(rect.min / view)) < 1e-4f && glm::length(quad.max - glm::vec2(rect.Max() / view)) < 1e-4f) return true;
    }
    return false;
}

void NothingWhereTheRunHasNoSetting() {
    // A bare layer's run, and one the engine does not cap (a capture): a kept 30 is not read, nothing asked, no row, the screen as it was.
    for (const bool unavailableEngine : {false, true}) {
        const std::filesystem::path saves = Scratch("none-saves");
        Write(saves / FrameRate::kStoreFile, "{\"version\":1,\"frame_rate\":30}");
        Rig rig("Fifo", !unavailableEngine);
        MagicPortalsLayer::Paths paths = RatePaths(saves.string(), Platform::Android);
        if (!unavailableEngine) paths.frameRate = FrameRate::Policy{};
        MagicPortalsLayer layer(paths, "");
        layer.OnAttach(rig.registry);
        const std::string what = unavailableEngine ? "an engine that applies no cap" : "no setting in the run";
        CHECK_MSG(!layer.FrameRateReady() && layer.FrameRateStore().Path().empty(), what + ": the file is not opened");
        CHECK_MSG(layer.FrameRateChoice() == Choice::Auto() && layer.FrameCap() == 0, what + ": Auto");
        if (!OpenOptions(layer, rig.registry)) return;
        CHECK_MSG(!layer.OptionsRules().rateRow.shown && !RowDrawn(rig, layer), what + ": no row");
        CHECK_MSG(layer.OptionsRules().title.atScreen.y == 0.14, what + ": the screen stands as it did");
        CHECK_MSG(!rig.TakeCap() && !rig.window.TakeRefresh() && rig.window.modeAsked == 0, what + ": nothing asked of the engine or the window");
        CheckText(Read(saves / FrameRate::kStoreFile), "{\"version\":1,\"frame_rate\":30}");
        layer.OnDetach(rig.registry);
    }
}

void ADesktopAutoAsksNothingAndTheRowCycles() {
    const std::filesystem::path saves = Scratch("desktop-saves");
    Rig rig("Fifo");
    MagicPortalsLayer::Paths paths = RatePaths(saves.string(), Platform::Desktop);
    paths.desktop = true;
    MagicPortalsLayer layer(paths, "");
    layer.OnAttach(rig.registry);
    CHECK_MSG(layer.FrameRateReady() && layer.FrameRateChoice() == Choice::Auto(), "the shipped default");
    CHECK_MSG(!rig.TakeCap() && !rig.window.TakeRefresh(), "Auto at the start asks nothing");
    CHECK(!layer.PicturesSmoothed());
    if (!OpenOptions(layer, rig.registry)) return;
    CHECK_MSG(!rig.TakeCap() && !rig.window.TakeRefresh(), "nor when Options opens");
    CHECK_MSG(layer.OptionsRules().rateRow.shown && layer.OptionsRules().desktop.shown && RowDrawn(rig, layer), "the desktop screen with the row");
    CheckText(Options::RateRowWords(layer.OptionsRules(), layer.FrameRateChoice(), nullptr), "Frame rate: Auto");
    CHECK_MSG(!std::filesystem::exists(saves / FrameRate::kStoreFile), "nothing is written before a pick");

    PressRow(layer, rig.registry);
    CHECK(layer.FrameRateChoice() == Choice::Of(30));
    CHECK_MSG(rig.TakeCap() == std::optional<int>(30) && layer.FrameCap() == 30, "30 caps the drawing");
    CheckText(Read(saves / FrameRate::kStoreFile), "{\"version\":1,\"frame_rate\":30}");
    CHECK_MSG(layer.PicturesSmoothed(), "a cap under the refresh decides the smoothing");
    CheckText(Options::RateRowWords(layer.OptionsRules(), layer.FrameRateChoice(), nullptr), "Frame rate: 30");

    PressRow(layer, rig.registry);
    CHECK(layer.FrameRateChoice() == Choice::Of(60));
    CHECK_MSG(rig.TakeCap() == std::optional<int>(0) && layer.FrameCap() == 0, "60 on a 60 Hz FIFO screen is the display's own: the cap withdrawn");
    CHECK_MSG(!layer.PicturesSmoothed(), "and the watch decides again");
    CheckText(Read(saves / FrameRate::kStoreFile), "{\"version\":1,\"frame_rate\":60}");

    PressRow(layer, rig.registry);
    CHECK(layer.FrameRateChoice() == Choice::Auto());
    CHECK_MSG(!rig.TakeCap(), "Auto after 60 asks nothing more");
    CheckText(Read(saves / FrameRate::kStoreFile), "{\"version\":1,\"frame_rate\":\"auto\"}");
    CHECK_MSG(!rig.window.TakeRefresh(), "a desktop's display is never asked");
    CHECK(layer.MenuScreen() == MagicPortalsLayer::Screen::Options);
    layer.OnDetach(rig.registry);
}

void AKeptChoiceIsAskedForAtTheStart() {
    const std::filesystem::path saves = Scratch("kept-saves");
    Write(saves / FrameRate::kStoreFile, "{\"version\":1,\"frame_rate\":30}");
    {
        Rig rig("Fifo");
        MagicPortalsLayer layer(RatePaths(saves.string(), Platform::Desktop), "level0");
        layer.OnAttach(rig.registry);
        CHECK_MSG(layer.SimLevel() != nullptr, layer.LoadError());
        CHECK_MSG(rig.TakeCap() == std::optional<int>(30) && layer.PicturesSmoothed(), "30 asked and smoothed at the start");
        for (int tick = 0; tick < 30; ++tick) Tick(layer, rig.registry);
        // A frame does not hand the decision back to the watch.
        for (int frame = 0; frame < 60; ++frame) layer.OnUpdate(rig.registry, 1.0f / 30.0f);
        CHECK(layer.PicturesSmoothed());
        CHECK_MSG(!rig.TakeCap(), "and nothing asked again");
        layer.OnDetach(rig.registry);
    }
    {
        // A kept 60 on a 60 Hz FIFO screen is exactly Auto.
        Write(saves / FrameRate::kStoreFile, "{\"version\":1,\"frame_rate\":60}");
        Rig rig("Fifo");
        MagicPortalsLayer layer(RatePaths(saves.string(), Platform::Desktop), "level0");
        layer.OnAttach(rig.registry);
        CHECK_MSG(!rig.TakeCap() && !rig.window.TakeRefresh() && !layer.PicturesSmoothed(), "nothing asked");
        layer.OnDetach(rig.registry);
    }
    {
        // On Mailbox nothing else holds 60, so it is asked, and the cap decides the smoothing: left to the watch, 60 frames a second are under
        // its line and each picture would stand at its tick's own place.
        Rig rig("Mailbox");
        MagicPortalsLayer layer(RatePaths(saves.string(), Platform::Desktop), "level0");
        layer.OnAttach(rig.registry);
        CHECK_MSG(rig.TakeCap() == std::optional<int>(60), "Mailbox: 60 is a cap");
        CHECK_MSG(layer.PicturesSmoothed(), "and smoothed on a 60 Hz screen");
        for (int tick = 0; tick < 30; ++tick) Tick(layer, rig.registry);
        for (int frame = 0; frame < 60; ++frame) layer.OnUpdate(rig.registry, 1.0f / 60.0f);
        CHECK_MSG(layer.PicturesSmoothed(), "frames at 60 do not hand the decision to the watch");
        layer.OnDetach(rig.registry);
    }
}

void TheMonitorIsAskedAgain() {
    const std::filesystem::path saves = Scratch("monitor-saves");
    Write(saves / FrameRate::kStoreFile, "{\"version\":1,\"frame_rate\":60}");
    Rig rig("Fifo");
    rig.window.refresh = 144;
    MagicPortalsLayer::Paths paths = RatePaths(saves.string(), Platform::Desktop);
    paths.desktop = true;
    MagicPortalsLayer layer(paths, "");
    layer.OnAttach(rig.registry);
    CHECK_MSG(rig.TakeCap() == std::optional<int>(60) && layer.PicturesSmoothed(), "60 on a 144 Hz screen is a cap, and smoothed");
    // The window moved to a 60 Hz screen, then F11.
    rig.window.refresh = 60;
    Tick(layer, rig.registry, kRest, {MagicPortalsLayer::kFullscreen}, {MagicPortalsLayer::kFullscreen});
    CHECK_MSG(rig.TakeCap() == std::optional<int>(0) && !layer.PicturesSmoothed(), "a full screen switch asks the monitor again");
    rig.window.refresh = 120;
    if (!OpenOptions(layer, rig.registry)) return;
    CHECK_MSG(rig.TakeCap() == std::optional<int>(60), "and so does Options opening");
    PressBack(layer, rig.registry);
    // A screen on which the row would do nothing: not shown.
    rig.window.refresh = 30;
    if (!OpenOptions(layer, rig.registry)) return;
    CHECK_MSG(!layer.OptionsRules().rateRow.shown && !RowDrawn(rig, layer), "a 30 Hz FIFO screen: no row");
    CHECK_MSG(layer.OptionsRules().title.atScreen == layer.OptionsRules().desktop.titleAtScreen, "and the desktop screen stands as it did");
    PressBack(layer, rig.registry);
    rig.window.refresh = 60;
    if (!OpenOptions(layer, rig.registry)) return;
    CHECK_MSG(layer.OptionsRules().rateRow.shown, "back on a 60 Hz screen the row is back");
    CheckText(Read(saves / FrameRate::kStoreFile), "{\"version\":1,\"frame_rate\":60}"); // none of this wrote a choice
    layer.OnDetach(rig.registry);
}

void AndroidAsksTheDisplayToo() {
    const std::filesystem::path saves = Scratch("android-saves");
    {
        // Auto: nothing at all, the display untouched, its mode never read.
        Rig rig("Mailbox");
        MagicPortalsLayer layer(RatePaths(saves.string(), Platform::Android), "");
        layer.OnAttach(rig.registry);
        CHECK_MSG(!rig.TakeCap() && !rig.window.TakeRefresh(), "Auto at the start never touches the display");
        if (!OpenOptions(layer, rig.registry)) return;
        CHECK_MSG(layer.OptionsRules().rateRow.shown && !layer.OptionsRules().desktop.shown, "the phone screen with the row");
        PressRow(layer, rig.registry); // 30
        CHECK(rig.TakeCap() == std::optional<int>(30) && rig.window.TakeRefresh() == std::optional<std::uint32_t>(60u));
        PressRow(layer, rig.registry); // 60
        CHECK_MSG(rig.TakeCap() == std::optional<int>(60) && !rig.window.TakeRefresh(), "60 Hz was asked already");
        CHECK(layer.PicturesSmoothed());
        PressRow(layer, rig.registry); // max
        CHECK(rig.TakeCap() == std::optional<int>(0) && rig.window.TakeRefresh() == std::optional<std::uint32_t>(kHighest));
        CHECK_MSG(!layer.PicturesSmoothed(), "no cap: the watch's");
        CheckText(Read(saves / FrameRate::kStoreFile), "{\"version\":1,\"frame_rate\":\"max\"}");
        PressRow(layer, rig.registry); // auto
        CHECK_MSG(!rig.TakeCap() && rig.window.TakeRefresh() == std::optional<std::uint32_t>(kWithdraw), "Auto withdraws the request this run made");
        CHECK_MSG(rig.window.modeAsked == 0, "a phone's display mode is never read");
        PressBack(layer, rig.registry);
        if (!OpenOptions(layer, rig.registry)) return;
        CHECK_MSG(!rig.TakeCap() && !rig.window.TakeRefresh(), "and asks nothing again");
        layer.OnDetach(rig.registry);
    }
    {
        Write(saves / FrameRate::kStoreFile, "{\"version\":1,\"frame_rate\":\"max\"}");
        Rig rig("Fifo");
        MagicPortalsLayer layer(RatePaths(saves.string(), Platform::Android), "level0");
        layer.OnAttach(rig.registry);
        CHECK_MSG(!rig.TakeCap() && rig.window.TakeRefresh() == std::optional<std::uint32_t>(kHighest), "a kept max asks again at the start");
        layer.OnDetach(rig.registry);
    }
}

void IosCapsOnly() {
    const std::filesystem::path saves = Scratch("ios-saves");
    Write(saves / FrameRate::kStoreFile, "{\"version\":1,\"frame_rate\":60}");
    Rig rig("Fifo");
    rig.window.refresh = 60; // what iOS's window control answers whatever the screen
    MagicPortalsLayer layer(RatePaths(saves.string(), Platform::Ios), "level0");
    layer.OnAttach(rig.registry);
    CHECK_MSG(rig.TakeCap() == std::optional<int>(60), "60 is a cap on an iPhone, FIFO or not");
    CHECK_MSG(!rig.window.TakeRefresh() && rig.window.modeAsked == 0, "the display neither asked nor read");
    layer.OnDetach(rig.registry);
}

void ClassicAppliesWithoutARow() {
    const std::filesystem::path saves = Scratch("classic-saves");
    Write(saves / FrameRate::kStoreFile, "{\"version\":1,\"frame_rate\":30}");
    Rig rig("Fifo");
    MagicPortalsLayer::Paths paths = RatePaths(saves.string(), Platform::Desktop);
    paths.controls = Controls::ResolvePolicy(std::string("classic"), false, false, false);
    MagicPortalsLayer layer(paths, "level0");
    layer.OnAttach(rig.registry);
    CHECK_MSG(!layer.OptionsAvailable() && !layer.OptionsRules().rateRow.shown, "no Options screen, no row");
    CHECK_MSG(rig.TakeCap() == std::optional<int>(30), "the kept choice applies all the same");
    layer.OnDetach(rig.registry);
}

// A copy of the port's tables with one edit of controls.json.
std::string EditedData(const std::string& name, const std::string& from, const std::string& to) {
    const std::filesystem::path dir = Scratch("data-" + name);
    std::error_code ec;
    std::filesystem::copy(kPortData, dir, std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing, ec);
    CHECK_MSG(!ec, "the tables copied: " + ec.message());
    std::string text = Read(dir / "controls.json");
    const std::size_t at = text.find(from);
    CHECK_MSG(at != std::string::npos && text.find(from, at + 1) == std::string::npos, "the edit applies once: " + from);
    if (at != std::string::npos) text.replace(at, from.size(), to);
    Write(dir / "controls.json", text);
    return dir.string();
}

void TheDataDecides() {
    {
        // Another default: a player who has chosen nothing gets it, and it is not written.
        const std::filesystem::path saves = Scratch("default-saves");
        Rig rig("Fifo");
        MagicPortalsLayer::Paths paths = RatePaths(saves.string(), Platform::Android);
        paths.portData = EditedData("default", "\"default\": \"auto\"", "\"default\": 60");
        MagicPortalsLayer layer(paths, "level0");
        layer.OnAttach(rig.registry);
        CHECK_MSG(layer.FrameRateChoice() == Choice::Of(60), "the data's default");
        CHECK(rig.TakeCap() == std::optional<int>(60) && rig.window.TakeRefresh() == std::optional<std::uint32_t>(60u));
        CHECK_MSG(!std::filesystem::exists(saves / FrameRate::kStoreFile), "a default is not the player's choice");
        layer.OnDetach(rig.registry);
    }
    {
        // A bad frame_rate block: Auto, no row, the rest of the run as it was.
        const std::filesystem::path saves = Scratch("badrules-saves");
        Write(saves / FrameRate::kStoreFile, "{\"version\":1,\"frame_rate\":30}");
        Rig rig("Fifo");
        MagicPortalsLayer::Paths paths = RatePaths(saves.string(), Platform::Android);
        paths.portData = EditedData("badrules", "\"smooth_when_limited\": true", "\"smooth_when_limited\": 1");
        MagicPortalsLayer layer(paths, "");
        layer.OnAttach(rig.registry);
        CHECK_MSG(!layer.FrameRateReady() && layer.FrameRateChoice() == Choice::Auto(), "Auto");
        if (!OpenOptions(layer, rig.registry)) return;
        CHECK_MSG(!layer.OptionsRules().rateRow.shown, "no row");
        CHECK(!rig.TakeCap() && !rig.window.TakeRefresh());
        layer.OnDetach(rig.registry);
    }
    {
        // A bad row layout: no row; the kept choice still applies, and Options is as it was.
        const std::filesystem::path saves = Scratch("badrow-saves");
        Write(saves / FrameRate::kStoreFile, "{\"version\":1,\"frame_rate\":30}");
        Rig rig("Fifo");
        MagicPortalsLayer::Paths paths = RatePaths(saves.string(), Platform::Android);
        paths.portData = EditedData("badrow", "\"auto_word\": \"Frame rate: Auto\"", "\"auto_word\": 1");
        MagicPortalsLayer layer(paths, "");
        layer.OnAttach(rig.registry);
        CHECK_MSG(layer.FrameRateReady(), "the setting stands");
        CHECK(rig.TakeCap() == std::optional<int>(30));
        if (!OpenOptions(layer, rig.registry)) return;
        CHECK_MSG(!layer.OptionsRules().rateRow.shown && layer.OptionsAvailable(), "Options without the row");
        CHECK(layer.OptionsRules().title.atScreen.y == 0.14);
        layer.OnDetach(rig.registry);
    }
    {
        // A kept file that cannot be read: the default, and written over at the next pick.
        const std::filesystem::path saves = Scratch("badfile-saves");
        Write(saves / FrameRate::kStoreFile, "{nonsense");
        Rig rig("Fifo");
        MagicPortalsLayer layer(RatePaths(saves.string(), Platform::Ios), "");
        layer.OnAttach(rig.registry);
        CHECK(layer.FrameRateReady() && layer.FrameRateChoice() == Choice::Auto() && !rig.TakeCap());
        if (!OpenOptions(layer, rig.registry)) return;
        PressRow(layer, rig.registry);
        CheckText(Read(saves / FrameRate::kStoreFile), "{\"version\":1,\"frame_rate\":30}");
        layer.OnDetach(rig.registry);
    }
}

void runPureTests() {
    TheShippedBlockAndItsRefusals();
    TheChoicesOfferedAndTheCycle();
    WhatEachPlatformAsks();
    TheFile();
    WhoseRunReadsIt();
    MainSwitchesTheSettingOn();
}

void runLayerTests() {
    NothingWhereTheRunHasNoSetting();
    ADesktopAutoAsksNothingAndTheRowCycles();
    AKeptChoiceIsAskedForAtTheStart();
    TheMonitorIsAskedAgain();
    AndroidAsksTheDisplayToo();
    IosCapsOnly();
    ClassicAppliesWithoutARow();
    TheDataDecides();
}

int pureEntry(int, char**) {
    runPureTests();
    return ::test::summary("test_mp_framerate", 150);
}

// The layer's cases, a suite of their own that exits 77 without the owner's data (CMakeLists: IN_SOURCE).
int levelsEntry(int, char**) {
    std::error_code ec;
    if (!std::filesystem::is_directory(kLevels, ec) || !std::filesystem::is_regular_file(kChapters, ec) ||
        !std::filesystem::is_regular_file(kData + "/entity_roles.json", ec)) {
        std::printf("test_mp_framerate_levels: SKIPPED - needs the converted levels at %s, chapters.json at %s and Enhanced's data at %s\n", kLevels.c_str(),
                    kChapters.c_str(), kData.c_str());
        return 77;
    }
    runLayerTests();
    return ::test::summary("test_mp_framerate_levels", 60);
}

const ::MpSuite::Registration mpPureRegistration("test_mp_framerate", &pureEntry);
const ::MpSuite::Registration mpLevelsRegistration("test_mp_framerate_levels", &levelsEntry);

} // namespace
