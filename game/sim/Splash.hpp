#pragma once

// THE SUPERSONIC ENGINE'S INTRO (planning step 272), the same as the one Penumbra and the Castle of Shadows Enhanced shows: this edition runs on the
// Supersonic Engine, and a normal start shows its logo for two seconds before anything of the game. The original showed nothing.
//
// Everything here is pure - ticks in, numbers and quads out - so a suite checks it without a window; the layer (MagicPortalsLayer.cpp) owns the clock,
// the devices and the overlay.
//
// THE TIMELINE is counted in engine ticks (60 Hz, never the wall clock): the picture fades in over 24 ticks, holds for 72 and fades out over 24, 120 in
// all (2.0 s). The ramps are smoothstep, so each is smooth and monotone and meets the hold with no corner.
//
// SKIPPING. A press (a key, a mouse button, a touch, a gamepad button going down - never one already held) from tick 18 on ends the intro by fading
// out, never by a cut. The fade-out starts from the alpha the picture has at that moment and takes 24 ticks, so a press in the first fade-in (ticks 18
// to 23, where the alpha is still below 1) does not make the picture jump up before it fades. A press before tick 18 is ignored: the first 18 ticks
// (0.3 s) belong to the click that started the game, a key still being released, the window taking the focus. A press while the fade-out has begun on
// its own changes nothing, the end is on its way.
//
// THE PICTURE is a flat ground and the logo (images/splash/supersonic-logo.png in the port's data, 2258 x 640: the lockup centred on a canvas of the
// same ground colour, so the picture's edge cannot be seen). It is centred in the display's safe area, 70% of the window's width (at most half its
// height, and inside the safe area), at whole pixels so its edges stay sharp; the lockup is centred in the picture, so it is centred on the screen too.
// Both are ScreenOverlay quads, which are drawn over the finished image in display values: the ground's bytes are the ground the picture was made on.
//
// WHO PLAYS IT. Only a normal start: any flag a player would not type turns it off (Wanted), so every headless capture, every test of the layer and
// every developer flag - present or added later - keeps what it had. --splash on forces it (a capture of the intro itself) and --splash off removes it.

#include <array>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

#include "core/Input.hpp"
#include "core/ScreenOverlay.hpp"
#include "platform/SafeArea.hpp"

namespace MagicPortals::Splash {

inline constexpr unsigned kFadeInTicks = 24;
inline constexpr unsigned kHoldTicks = 72;
inline constexpr unsigned kFadeOutTicks = 24;
inline constexpr unsigned kTotalTicks = kFadeInTicks + kHoldTicks + kFadeOutTicks;
static_assert(kTotalTicks == 120, "the intro is two seconds at 60 Hz");
// Where the unskipped timeline begins to fade out.
inline constexpr unsigned kFadeOutStart = kFadeInTicks + kHoldTicks;
// The first tick a press may end the intro on.
inline constexpr unsigned kSkipFromTick = 18;

// The logo's width as a fraction of the window's, and the most of its height it may take.
inline constexpr float kWidthFraction = 0.70f;
inline constexpr float kHeightFraction = 0.50f;

// The ground: #14171C, the fill of the logo's own panel (supersonic-logo.svg's rect) and the colour of the picture's canvas, as display bytes.
inline constexpr std::array<std::uint8_t, 3> kGround = {0x14, 0x17, 0x1C};

// Under the port's data folder.
inline constexpr const char* kLogoFile = "images/splash/supersonic-logo.png";

// ---- The timeline -------------------------------------------------------------------------------------------

// The picture's opacity at `tick` of the free-running timeline: 0 at tick 0, 1 from tick 24 to tick 96, 0 from tick 120 on. Smooth and monotone on
// each ramp.
float Alpha(unsigned tick);
// Whether the free-running timeline is over after `tick` ticks.
bool Done(unsigned tick);

// The intro's clock. Step once per engine tick the intro runs; Alpha is what to draw after that tick.
class Clock {
public:
    // One tick. `pressed`: something went down since the previous tick (PressWatch). It ends the intro only from kSkipFromTick, and not once the
    // fade-out has begun by itself. A finished clock stays as it is.
    void Step(bool pressed);

    // The ticks that have run.
    unsigned Ticks() const { return m_ticks; }
    // A press ended it, rather than the timeline.
    bool Skipped() const { return m_skipTick.has_value(); }
    // The tick (of Ticks()'s count, the one that was running) the press ended it on.
    std::optional<unsigned> SkipTick() const { return m_skipTick; }
    bool Done() const;
    // After a skip: the alpha it had at the press, faded to 0 over kFadeOutTicks. Else Splash::Alpha(Ticks()).
    float Alpha() const;

private:
    unsigned m_ticks = 0;
    std::optional<unsigned> m_skipTick;
};

// ---- What counts as a press ---------------------------------------------------------------------------------

// Every input an intro can be ended with, one bit each: any key (by the engine's code), the three mouse buttons, eight touch slots (a contact's id
// modulo 8), and for the first four gamepads every button of the standard layout. Sticks and triggers are not here: a pad at rest drifts.
inline constexpr std::size_t kPads = 4;
inline constexpr std::size_t kPadBits = static_cast<std::size_t>(Supersonic::Pad::ButtonCount);
inline constexpr std::size_t kKeyBase = 0;
inline constexpr std::size_t kMouseBase = kKeyBase + static_cast<std::size_t>(Supersonic::Key::Last) + 1;
inline constexpr std::size_t kTouchBase = kMouseBase + 3;
inline constexpr std::size_t kPadBase = kTouchBase + 8;
inline constexpr std::size_t kHeldBits = kPadBase + kPads * kPadBits;
using Held = std::bitset<kHeldBits>;

// What the devices hold, as plain numbers a suite can write: the layer fills it from the engine's input.
struct Devices {
    std::array<bool, Supersonic::Key::Last + 1> keys{};
    std::array<bool, 3> mouse{};
    std::vector<int> contactIds;                          // the ids of the fingers that are down
    std::array<std::array<bool, kPadBits>, kPads> pads{}; // the buttons of the first four gamepads
};

Held HeldOf(const Devices& devices);

// Press edges: the inputs held now that were not held at the previous Observe. The first Observe only records what is held, so a key still down from
// before the intro, or from the click that started the game, is no press.
class PressWatch {
public:
    bool Observe(const Held& held);
    // What was held at the last look.
    const Held& HeldNow() const { return m_held; }

private:
    Held m_held;
    bool m_have = false;
};

// ---- The picture --------------------------------------------------------------------------------------------

// Where the logo goes, in window pixels: whole pixels, its own proportions.
struct Layout {
    glm::vec2 pos{0.0f};
    glm::vec2 size{0.0f};
};

// `window` is the image the game is drawn into, `safe` the display's insets in its pixels (zero on a desktop), `image` the logo's size in pixels. Zero
// size where there is nothing to draw (an empty window or image).
Layout ComputeLayout(const glm::uvec2& window, const Supersonic::SafeAreaInsets& safe, const glm::vec2& image);

// The overlay quads of one frame, in drawing order: the flat ground over the whole window, then the logo at `alpha` (nothing while that is 0, or without
// a texture).
std::vector<Supersonic::ScreenOverlay::Quad> BuildQuads(const glm::uvec2& window, const Layout& layout, float alpha, const std::string& logoTexture);

// ---- Who plays it ----------------------------------------------------------------------------------------------

// Whether this start plays the intro. `attended` is the port's own definition of a player's run (RunKind.hpp, AttendedRun: a window the game opened for
// someone to play, no capture, no route, no scripted input, no developer flag of the engine's), `directStart` that a level was named on the command line
// (--level: a developer's way to begin in the middle). `forced` is the last "--splash on" or "--splash off" given: on plays it whatever else is given (a
// capture of the intro itself: --splash on --fixed-step --frames 30 --screenshot <path>), off removes it. Without one the intro plays on a player's run
// that names no level, so every headless capture, every suite and every developer flag - present or added later - keeps what it had.
bool Wanted(bool attended, bool directStart, std::optional<bool> forced);

} // namespace MagicPortals::Splash
