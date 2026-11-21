// The Supersonic Engine's intro (planning step 272): sim/Splash.hpp, the pure part - the timeline, the clock, the press watch, the layout, the quads and the rule
// that says who sees it. The same intro Penumbra and the Castle of Shadows Enhanced shows (its E35).
//
// WHAT IS PINNED, with no engine, no layer, no window:
//   - THE TIMELINE: 120 ticks (2.0 s at 60 Hz): fade in 24, hold 72, fade out 24; 0 at tick 0, 1 from 24 to 96, 0 from 120; smooth and monotone on each ramp, no step above
//     1.5/24 of the alpha a tick.
//   - THE SKIP: a press before tick 18 is ignored (every tick of the first 18); from tick 18 to 95 it fades the picture out over 24 ticks FROM the alpha it had (never a cut,
//     never up), so the intro ends 24 ticks after the press, at tick 42 at the earliest; a press in the timeline's own fade-out changes nothing; a finished clock stays.
//   - THE PRESS WATCH: a key, a mouse button, a finger (its id modulo 8) or a gamepad button going down is a press; what was held at the first look, or is held still, or goes
//     up, is not; the first look only records.
//   - THE LAYOUT: whole pixels, the logo's proportions, at most 70% of the window's width and half its height, inside the safe area, centred in it, in 14 window shapes with insets;
//     nothing for an empty window or image.
//   - THE QUADS: the flat ground #14171C over the whole window first, then the logo at the alpha (none while it is 0 or without a texture).
//   - WHO SEES IT: a player's run that names no level, and --splash on / off over that.
//   - THE LOGO'S FILE is there, 2258 x 640, and small.
// NOT TESTED HERE: the layer's use of it (test_mp_layer: nothing ticks during the intro, a press ends it, the first game tick is the game's) and the picture on screen (a capture:
// build/game/MagicPortals.exe --splash on --fixed-step --frames 30 --screenshot <absolute path>).

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "sim/Splash.hpp"
#include "sim/Sprites.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

using namespace MagicPortals;

namespace {

bool Near(float a, float b, float tolerance = 1e-6f) { return std::fabs(a - b) <= tolerance; }

void TheTimelineIsTwoSecondsOfFadeHoldFade() {
    CHECK_EQ(Splash::kTotalTicks, 120u);
    CHECK_MSG(Near(Splash::Alpha(0), 0.0f), "tick 0 is invisible");
    CHECK_MSG(Near(Splash::Alpha(Splash::kFadeInTicks), 1.0f), "tick 24 is full");
    CHECK_MSG(Near(Splash::Alpha(Splash::kFadeOutStart), 1.0f), "tick 96 is still full");
    CHECK_MSG(Near(Splash::Alpha(Splash::kTotalTicks), 0.0f), "tick 120 is gone");
    CHECK_MSG(Near(Splash::Alpha(5000), 0.0f), "and stays gone");
    CHECK(!Splash::Done(119));
    CHECK(Splash::Done(120));
    float most = 0.0f;
    for (unsigned tick = 1; tick < Splash::kTotalTicks; ++tick) {
        const float step = Splash::Alpha(tick) - Splash::Alpha(tick - 1);
        most = std::max(most, std::fabs(step));
        if (tick <= Splash::kFadeInTicks) CHECK_MSG(step >= 0.0f, "the fade in never goes down: tick " + std::to_string(tick));
        if (tick > Splash::kFadeOutStart) CHECK_MSG(step <= 0.0f, "the fade out never goes up: tick " + std::to_string(tick));
        if (tick > Splash::kFadeInTicks && tick <= Splash::kFadeOutStart) CHECK_MSG(Near(step, 0.0f), "the hold holds: tick " + std::to_string(tick));
    }
    CHECK_MSG(most <= 1.5f / 24.0f + 1e-6f, "no step of the alpha above 1.5/24: " + std::to_string(most));
}

void AFreeRunningClockRunsOutAfterOneHundredAndTwentyTicks() {
    Splash::Clock clock;
    for (unsigned tick = 0; tick < Splash::kTotalTicks; ++tick) {
        CHECK_MSG(!clock.Done(), "not done before tick " + std::to_string(tick));
        clock.Step(false);
    }
    CHECK(clock.Done());
    CHECK(!clock.Skipped());
    CHECK_EQ(clock.Ticks(), 120u);
    CHECK(Near(clock.Alpha(), 0.0f));
    clock.Step(true);
    CHECK_MSG(clock.Ticks() == 120u && !clock.Skipped(), "a finished clock stays as it is");
}

void APressBeforeTickEighteenIsIgnoredAndFromThenItFadesOut() {
    for (unsigned pressTick = 0; pressTick < Splash::kTotalTicks; ++pressTick) {
        Splash::Clock clock;
        unsigned ranOn = 0;
        for (unsigned tick = 0; tick < 400 && !clock.Done(); ++tick) {
            clock.Step(tick == pressTick);
            ranOn = clock.Ticks();
        }
        const std::string what = "a press on tick " + std::to_string(pressTick);
        if (pressTick < Splash::kSkipFromTick) {
            CHECK_MSG(!clock.Skipped() && ranOn == Splash::kTotalTicks, what + " is ignored (the intro runs its 120)");
        } else if (pressTick < Splash::kFadeOutStart) {
            CHECK_MSG(clock.Skipped() && clock.SkipTick() && *clock.SkipTick() == pressTick, what + " skips on that tick");
            CHECK_MSG(ranOn == pressTick + Splash::kFadeOutTicks, what + " ends 24 ticks after it: " + std::to_string(ranOn));
        } else {
            CHECK_MSG(!clock.Skipped() && ranOn == Splash::kTotalTicks, what + " falls in the timeline's own fade out and changes nothing");
        }
    }
}

void ASkipFadesFromTheAlphaTheScreenHadAndNeverJumpsUp() {
    for (unsigned pressTick : {18u, 20u, 23u, 24u, 50u, 95u}) {
        Splash::Clock clock;
        float previous = 0.0f;
        for (unsigned tick = 0; tick < pressTick; ++tick) {
            clock.Step(false);
            previous = clock.Alpha();
        }
        clock.Step(true); // the press tick
        float now = clock.Alpha();
        const std::string what = "a skip on tick " + std::to_string(pressTick);
        CHECK_MSG(now <= std::max(previous, Splash::Alpha(pressTick)) + 1e-6f, what + " does not jump up: " + std::to_string(previous) + " to " + std::to_string(now));
        for (unsigned tick = 0; tick < Splash::kFadeOutTicks + 2 && !clock.Done(); ++tick) {
            clock.Step(false);
            const float next = clock.Alpha();
            CHECK_MSG(next <= now + 1e-6f, what + " only goes down");
            now = next;
        }
        CHECK_MSG(Near(clock.Alpha(), 0.0f), what + " ends at nothing");
    }
}

Splash::Devices Nothing() { return Splash::Devices{}; }

void ThePressWatchSeesWhatGoesDownAndNothingElse() {
    Splash::PressWatch watch;
    Splash::Devices held = Nothing();
    held.keys[257] = true; // a key still down from before the intro, or from the click that started the game
    CHECK_MSG(!watch.Observe(Splash::HeldOf(held)), "the first look only records");
    CHECK_MSG(!watch.Observe(Splash::HeldOf(held)), "a key still held is no press");
    held.keys[257] = false;
    CHECK_MSG(!watch.Observe(Splash::HeldOf(held)), "a release is no press");
    held.keys[32] = true;
    CHECK_MSG(watch.Observe(Splash::HeldOf(held)), "a key going down is");
    CHECK_MSG(!watch.Observe(Splash::HeldOf(held)), "once");

    // Every kind of input, from rest.
    const auto pressWith = [](const std::function<void(Splash::Devices&)>& set) {
        Splash::PressWatch fresh;
        (void)fresh.Observe(Splash::HeldOf(Splash::Devices{}));
        Splash::Devices devices;
        set(devices);
        return fresh.Observe(Splash::HeldOf(devices));
    };
    CHECK_MSG(pressWith([](Splash::Devices& d) { d.keys[0] = true; }), "key 0");
    CHECK_MSG(pressWith([](Splash::Devices& d) { d.keys[Supersonic::Key::Last] = true; }), "the last key");
    for (std::size_t button = 0; button < 3; ++button) {
        CHECK_MSG(pressWith([button](Splash::Devices& d) { d.mouse[button] = true; }), "mouse button " + std::to_string(button));
    }
    for (int id : {0, 1, 7, 8, 15, 1000}) {
        CHECK_MSG(pressWith([id](Splash::Devices& d) { d.contactIds.push_back(id); }), "a finger with id " + std::to_string(id));
    }
    CHECK_MSG(!pressWith([](Splash::Devices& d) { d.contactIds.push_back(-1); }), "a contact with no id is none");
    for (std::size_t pad = 0; pad < Splash::kPads; ++pad) {
        for (std::size_t button = 0; button < Splash::kPadBits; ++button) {
            CHECK_MSG(pressWith([pad, button](Splash::Devices& d) { d.pads[pad][button] = true; }), "pad " + std::to_string(pad) + " button " + std::to_string(button));
        }
    }
    // A second finger while the first is down is a press, and two fingers of ids that share a slot (8 and 0) are one slot.
    Splash::PressWatch fingers;
    Splash::Devices one;
    one.contactIds = {0};
    (void)fingers.Observe(Splash::HeldOf(one));
    Splash::Devices two = one;
    two.contactIds.push_back(3);
    CHECK_MSG(fingers.Observe(Splash::HeldOf(two)), "a second finger is a press");
}

void TheLogoIsCentredInTheSafeAreaInWholePixels() {
    const glm::vec2 image(2258.0f, 640.0f);
    struct Shape {
        glm::uvec2 window;
        Supersonic::SafeAreaInsets safe;
    };
    Supersonic::SafeAreaInsets none{};
    Supersonic::SafeAreaInsets notch{};
    notch.left = 199.0f;
    Supersonic::SafeAreaInsets bars{};
    bars.top = 90.0f;
    bars.bottom = 70.0f;
    bars.left = 40.0f;
    bars.right = 60.0f;
    Supersonic::SafeAreaInsets absurd{};
    absurd.left = 5000.0f;
    int shapes = 0;
    for (const Shape& shape : {Shape{{2992, 1344}, notch}, Shape{{1280, 720}, none}, Shape{{1024, 768}, none}, Shape{{1080, 2400}, none}, Shape{{720, 1280}, bars},
                               Shape{{2400, 1080}, bars}, Shape{{640, 360}, none}, Shape{{320, 480}, none}, Shape{{3840, 2160}, none}, Shape{{1920, 1080}, notch},
                               Shape{{800, 800}, bars}, Shape{{100, 700}, none}, Shape{{700, 100}, none}, Shape{{1280, 720}, absurd}}) {
        ++shapes;
        const Splash::Layout layout = Splash::ComputeLayout(shape.window, shape.safe, image);
        const std::string what = std::to_string(shape.window.x) + " x " + std::to_string(shape.window.y);
        CHECK_MSG(layout.size.x >= 1.0f && layout.size.y >= 1.0f, what + ": something to draw");
        CHECK_MSG(layout.size.x == std::round(layout.size.x) && layout.size.y == std::round(layout.size.y) && layout.pos.x == std::round(layout.pos.x) && layout.pos.y == std::round(layout.pos.y),
                  what + ": whole pixels");
        CHECK_MSG(std::fabs(layout.size.y - layout.size.x * image.y / image.x) <= 0.5f + 1e-3f,
                  what + ": the logo's proportions (the height is the width's, to the pixel): " + std::to_string(layout.size.x) + " x " + std::to_string(layout.size.y));
        const float windowWidth = static_cast<float>(shape.window.x);
        const float windowHeight = static_cast<float>(shape.window.y);
        CHECK_MSG(layout.size.x <= windowWidth * Splash::kWidthFraction + 1.0f, what + ": at most 70% of the width");
        CHECK_MSG(layout.size.y <= windowHeight * Splash::kHeightFraction + 1.0f, what + ": at most half the height");
        const bool believed = windowWidth - shape.safe.left - shape.safe.right >= 1.0f;
        const float left = believed ? shape.safe.left : 0.0f;
        const float right = believed ? shape.safe.right : 0.0f;
        const float top = believed ? shape.safe.top : 0.0f;
        const float bottom = believed ? shape.safe.bottom : 0.0f;
        CHECK_MSG(layout.pos.x >= left - 0.5f && layout.pos.y >= top - 0.5f && layout.pos.x + layout.size.x <= windowWidth - right + 0.5f &&
                      layout.pos.y + layout.size.y <= windowHeight - bottom + 0.5f,
                  what + ": inside the safe area");
        const float centreX = layout.pos.x + layout.size.x * 0.5f;
        const float centreY = layout.pos.y + layout.size.y * 0.5f;
        CHECK_MSG(std::fabs(centreX - (left + (windowWidth - left - right) * 0.5f)) <= 1.0f && std::fabs(centreY - (top + (windowHeight - top - bottom) * 0.5f)) <= 1.0f,
                  what + ": centred in the safe area: (" + std::to_string(centreX) + ", " + std::to_string(centreY) + ")");
    }
    CHECK_EQ(shapes, 14);
    const Splash::Layout empty = Splash::ComputeLayout(glm::uvec2(0, 720), none, image);
    CHECK_MSG(empty.size.x == 0.0f && empty.size.y == 0.0f, "an empty window has nothing to draw");
    const Splash::Layout noImage = Splash::ComputeLayout(glm::uvec2(1280, 720), none, glm::vec2(0.0f));
    CHECK_MSG(noImage.size.x == 0.0f && noImage.size.y == 0.0f, "and so has an empty image");
}

void TheQuadsAreTheGroundAndThenTheLogoAtItsAlpha() {
    const glm::uvec2 window(1280, 720);
    const Splash::Layout layout = Splash::ComputeLayout(window, Supersonic::SafeAreaInsets{}, glm::vec2(2258.0f, 640.0f));
    const auto quads = Splash::BuildQuads(window, layout, 0.5f, "logo.png");
    CHECK_EQ(quads.size(), std::size_t{2});
    if (quads.size() != 2) return;
    CHECK_MSG(quads[0].texture.empty() && quads[0].min == glm::vec2(0.0f) && quads[0].max == glm::vec2(1.0f), "the ground is flat and covers the window");
    CHECK_MSG(Near(quads[0].color.r, 20.0f / 255.0f) && Near(quads[0].color.g, 23.0f / 255.0f) && Near(quads[0].color.b, 28.0f / 255.0f) && Near(quads[0].color.a, 1.0f),
              "and is #14171C, opaque");
    CHECK_MSG(quads[1].texture == "logo.png" && Near(quads[1].color.a, 0.5f) && Near(quads[1].color.r, 1.0f), "the logo comes next, at the alpha, untinted");
    CHECK_MSG(Near(quads[1].min.x, layout.pos.x / 1280.0f) && Near(quads[1].max.y, (layout.pos.y + layout.size.y) / 720.0f), "at the layout's place, in window fractions");
    CHECK_EQ(Splash::BuildQuads(window, layout, 0.0f, "logo.png").size(), std::size_t{1});
    CHECK_EQ(Splash::BuildQuads(window, layout, 1.0f, "").size(), std::size_t{1});
    CHECK_EQ(Splash::BuildQuads(glm::uvec2(0, 0), layout, 1.0f, "logo.png").size(), std::size_t{0});
    const auto over = Splash::BuildQuads(window, layout, 7.0f, "logo.png");
    CHECK_MSG(over.size() == 2 && Near(over[1].color.a, 1.0f), "an alpha above 1 is 1");
}

void OnlyAPlayersRunThatNamesNoLevelSeesIt() {
    using std::nullopt;
    CHECK_MSG(Splash::Wanted(true, false, nullopt), "a player's run, no level named: the intro");
    CHECK_MSG(!Splash::Wanted(true, true, nullopt), "a player's run with --level: none");
    CHECK_MSG(!Splash::Wanted(false, false, nullopt), "a capture, a route, a scripted run: none");
    CHECK_MSG(!Splash::Wanted(false, true, nullopt), "and neither with a level");
    CHECK_MSG(Splash::Wanted(false, true, true), "--splash on plays it whatever else is given");
    CHECK_MSG(Splash::Wanted(true, true, true), "also on a run that names a level");
    CHECK_MSG(!Splash::Wanted(true, false, false), "--splash off removes it from a normal start");
    CHECK_MSG(!Splash::Wanted(false, false, false), "and from every other");
}

void TheLogoFileIsThereAndSmall() {
    const std::filesystem::path logo = std::filesystem::path(MAGICPORTALS_PORT_DATA_DIR) / Splash::kLogoFile;
    std::error_code ec;
    CHECK_MSG(std::filesystem::is_regular_file(logo, ec), "the logo is in the port's data: " + logo.string());
    if (!std::filesystem::is_regular_file(logo, ec)) return;
    int width = 0;
    int height = 0;
    std::string why;
    CHECK_MSG(Sprites::ImageSize(logo.string(), width, height, why), "its header reads: " + why);
    CHECK_MSG(width == 2258 && height == 640, "it is 2258 x 640: " + std::to_string(width) + " x " + std::to_string(height));
    CHECK_MSG(std::filesystem::file_size(logo, ec) < 120u * 1000u, "and under 120 kB");
}

void runTests() {
    TheTimelineIsTwoSecondsOfFadeHoldFade();
    AFreeRunningClockRunsOutAfterOneHundredAndTwentyTicks();
    APressBeforeTickEighteenIsIgnoredAndFromThenItFadesOut();
    ASkipFadesFromTheAlphaTheScreenHadAndNeverJumpsUp();
    ThePressWatchSeesWhatGoesDownAndNothingElse();
    TheLogoIsCentredInTheSafeAreaInWholePixels();
    TheQuadsAreTheGroundAndThenTheLogoAtItsAlpha();
    OnlyAPlayersRunThatNamesNoLevelSeesIt();
    TheLogoFileIsThereAndSmall();
}

} // namespace

MP_TEST_MAIN("test_mp_splash", 100)
