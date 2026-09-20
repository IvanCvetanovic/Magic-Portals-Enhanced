#include "sim/Follower.hpp"

#include "core/DetMath.hpp"

#include <algorithm>

namespace MagicPortals::Follower {

namespace {

// smoothEnd(v) = sin(v x PIb), PIb = PI/2 (bytes 313899..313971). Through DetMath,
// as every trigonometric call on this side is: the dragon's angle steers a
// fireball, so the value reaches the simulation and must be the same on every
// machine.
double SmoothEnd(double bias) {
    return static_cast<double>(
        Supersonic::DetMath::sin(static_cast<float>(bias * 1.57079632679489661923)));
}

} // namespace

glm::dvec2 Point::At() const {
    // InterpolationTimer::isOver is `elapsed > time` (bytes 19190..19256), and
    // getCurrentPos returns m_b outright when it is over (20104..20325 ins 3-18).
    // A time of 0 is over on the first read, so the divide below never runs at 0.
    if (timeMs == 0 || elapsedMs > timeMs) return b;
    const double bias = std::clamp(static_cast<double>(elapsedMs) / static_cast<double>(timeMs), 0.0, 1.0);
    return a + (b - a) * SmoothEnd(bias);
}

void Point::Force(const glm::dvec2& at) {
    a = at;
    b = at;
}

void Point::Update(unsigned int frameMs) {
    // The test is on the point as it stands, against the destination, and the
    // comparison is EXACT (vector2::opEquals, addons/scriptmath2d.cpp:195): a point
    // that has arrived stops retargeting, and switchMs then grows without limit
    // until the destination moves.
    const glm::dvec2 now = At();
    if (now != dest && switchMs > rateMs) {
        a = now;
        b = dest;
        timeMs = strideMs;
        elapsedMs = 0;
        switchMs = 0;
    }
    // Both clocks are added to AFTER the test, which is what makes a fresh point
    // hold for one whole period rather than retarget on its second tick.
    switchMs += frameMs;
    elapsedMs += frameMs;
}

Point Make(const glm::dvec2& at, unsigned int strideMs, unsigned int rateMs) {
    Point made;
    made.strideMs = strideMs;
    made.rateMs = rateMs;
    // PositionInterpolator(pos, pos, m_interpStride, true): both ends at `at`, and
    // the timer reset to the stride with no time elapsed.
    made.a = at;
    made.b = at;
    made.timeMs = strideMs;
    made.elapsedMs = 0;
    made.switchMs = 0;
    // m_destPos is default-constructed to (0, 0) and the caller sets it on its
    // first tick, as both dragons' callbacks do.
    made.dest = glm::dvec2(0.0);
    return made;
}

unsigned int FrameMs(double frameSeconds) {
    const double ms = frameSeconds * 1000.0;
    if (!(ms > 0.0)) return 0;
    return static_cast<unsigned int>(ms);
}

} // namespace MagicPortals::Follower
