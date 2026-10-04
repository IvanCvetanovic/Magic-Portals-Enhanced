#pragma once

// Where a picture is drawn between ticks: the original's, which is the tick's own place
// (planning step 150, program wave 3a, PIC; pictures.json has the decode).
//
// The original draws every entity at the position its frame left it: SetPositionXY is a
// b2Body::SetTransform inside Update, and the draw reads GetPosition at draw time
// (ETHRenderEntity.cpp:106), so a body that moved in a frame is drawn moved in that same
// frame, and an effect released from it that frame starts where the picture stands. The
// engine draws a body between its last two ticks (InterpolationSystem), which at alpha 0 -
// every --fixed-step frame - is where the body stood a tick ago. The layer therefore
// publishes each tick's end as both ends of the interpolation (MagicPortalsLayer::
// drawPicturesAtTick), and keeps the engine's interpolation only where the display outruns
// the tick, for smoothness: this is the rule for when that is.

#include <string>

namespace MagicPortals::Pictures {

// pictures.json's `smoothing`. A default-built Rules never smooths (above_hz 0), the
// original's own: the pure suites' reading, which LoadRules replaces.
struct Rules {
    // Keep the engine's interpolation while the display's frames come faster than this,
    // in hertz; 0 never does, so every rate draws at the tick's own place.
    double aboveHz = 0.0;
    // And hand it back when they come slower than this (a band under aboveHz, so a
    // display near the line does not flip every few frames).
    double releaseBelowHz = 0.0;
    // The mean is over about this many frames (an exponential average of the frame's
    // length).
    int windowFrames = 30;
    // A frame longer than this many ticks is a stall (a load, a window drag) and says
    // nothing of the display's rate.
    double hitchTicks = 2.0;
};

// Reads pictures.json. A band that is not one (release above the line), a window under a
// frame, or a negative rate is refused.
bool LoadRules(const std::string& path, Rules& out, std::string& error);

// What the layer knows of the display's rate: the lengths of the frames it is handed.
// Started at the tick's own rate (the rate the port is matched at), so a run begins
// drawing at the tick's place and changes only on what it measures.
class RateWatch {
public:
    // One rendered frame of `frameS` seconds, against a tick of `tickS`.
    void Frame(const Rules& rules, double frameS, double tickS);

    // Whether the engine's interpolation is kept: the mean rate is over the line, and
    // has not since fallen under the band's floor.
    bool Smoothing() const { return m_smoothing; }
    // The mean rate, in hertz; 0 before the first frame is taken.
    double Hz() const { return m_periodS > 0.0 ? 1.0 / m_periodS : 0.0; }

private:
    double m_periodS = 0.0; // the exponential mean of the frames taken; 0 until the first
    bool m_smoothing = false;
};

} // namespace MagicPortals::Pictures
