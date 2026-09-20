#pragma once

// The original's `Follower` (ETHFramework/Follower.angelscript, bytes
// 25263..26388): A LAGGED POINT, and nothing else. It is not an aiming device, not
// a mover and not a camera - it holds a point that eases toward a destination it
// re-samples at most every `rate` ms, and whatever asks for that point decides what
// it means. The 2-32 dragon lags a POSITION with it (Dragon.hpp); the dark dragon
// lags an ANGLE IN RADIANS carried in x (DarkDragon, the visuals plan's 12.1).
//
// WHAT IT DOES, per tick (`Follower::update`, bytes 26159..26388 ins 3-58; decode
// D7 section 1.3, out/parity/visuals/briefs/decode/D7.md):
//
//     if (getCurrentPos() != destination && switchMs > rate)   // exact !=, strict >
//         reset: a = getCurrentPos(), b = destination, time = stride, elapsed = 0
//         switchMs = 0
//     switchMs += frame ms
//     elapsed  += frame ms
//
// and the point itself (`PositionInterpolator`, `InterpolationTimer`):
//
//     getCurrentPos() = elapsed > time ? b : a + (b - a) x sin(elapsed / time x PI/2)
//
// `smoothEnd(v) = sin(v x PIb)`, PIb = PI/2 (bytes 313899..313971; D9, 00_order
// 8.10). So each period closes sin(rate/stride x PI/2) of the gap to a still
// target: 16.3% per 208 ms for the dragon's 2000/200, an equivalent time constant
// of about 1.22 s.
//
// THE CLOCK IS THE SCRIPT'S, IN WHOLE MILLISECONDS. `GetLastFrameElapsedTime`
// returns a uint that ETHEngine truncates from the frame's real length
// (`ETHEngine.cpp:141-147`, VERIFIED), so a device at 60 Hz feeds the script 16 ms
// a frame, not 16.67. Update takes that whole number, for two reasons: it is what
// the original counted, and a double clock would put the `switchMs > rate` test on
// a knife edge - 12 ticks of 1/60 s is 200.000008 ms, so a 200 ms rate would
// retarget on the 12th tick where the original retargets on the 13th, and which of
// the two the port took would be decided by float representation rather than by the
// decode. D7 section 2's unit-step table is pinned in test_mp_dragon on the 16 ms
// row.
//
// A NEW POINT HOLDS STILL FOR ONE FULL PERIOD, because switchMs starts at 0, and
// `Force` (`forcePosition`, 25839..25929) sets a = b only: it moves the point
// without touching the destination or either clock.
//
// Renderer-free, registry-free and free of Game::Level: a Point is arithmetic.

#include <glm/glm.hpp>

namespace MagicPortals::Follower {

// One `Follower` instance. Its two constants are the constructor's `interpStride`
// and `updateRate`, in whole ms.
struct Point {
    glm::dvec2 dest{0.0}; // m_destPos, which the constructor leaves at (0, 0)
    glm::dvec2 a{0.0};    // the ease's start, m_interp.m_a
    glm::dvec2 b{0.0};    // and its end, m_interp.m_b
    unsigned int strideMs = 0;  // m_interpStride: how long one ease lasts
    unsigned int rateMs = 0;    // m_updateRate: the shortest gap between retargets
    unsigned int timeMs = 0;    // the live ease's length (m_interp's m_time)
    unsigned int elapsedMs = 0; // how far into it (m_elapsedTime)
    unsigned int switchMs = 0;  // since the last retarget (m_switchTime)

    // getPos(): where the point is now.
    glm::dvec2 At() const;

    // setDestinationPos: it changes nothing else, not even the clocks.
    void SetDestination(const glm::dvec2& to) { dest = to; }

    // forcePosition: a = b = at. The destination and both clocks are untouched.
    void Force(const glm::dvec2& at);

    // One tick of `update`, with the frame's length in whole ms.
    void Update(unsigned int frameMs);
};

// The constructor, `Follower(pos, interpStride, updateRate)`: the interpolator is
// built at `at` for both ends, so the point starts there and holds until the first
// retarget.
Point Make(const glm::dvec2& at, unsigned int strideMs, unsigned int rateMs);

// The frame's length as the script sees it: the engine's truncation to whole ms
// (ETHEngine.cpp:141-147). 1/60 s gives 16.
unsigned int FrameMs(double frameSeconds);

} // namespace MagicPortals::Follower
