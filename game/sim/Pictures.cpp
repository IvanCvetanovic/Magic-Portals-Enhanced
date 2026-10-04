#include "sim/Pictures.hpp"

#include "core/Json.hpp"

#include <cmath>
#include <fstream>
#include <sstream>

namespace MagicPortals::Pictures {

namespace Json = Supersonic::Json;

bool LoadRules(const std::string& path, Rules& out, std::string& error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = path + ": cannot open";
        return false;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string text = buffer.str();
    Json::Value root;
    Json::Parser parser(text);
    if (!parser.Parse(root)) {
        error = path + ": " + parser.Error();
        return false;
    }
    const Json::Value& smoothing = root["smoothing"];
    if (!smoothing["above_hz"].IsNumber() || !smoothing["release_below_hz"].IsNumber() ||
        !smoothing["window_frames"].IsNumber() || !smoothing["hitch_ticks"].IsNumber()) {
        error = path + ": smoothing.above_hz, release_below_hz, window_frames and hitch_ticks are numbers";
        return false;
    }
    Rules read;
    read.aboveHz = smoothing["above_hz"].AsNumber();
    read.releaseBelowHz = smoothing["release_below_hz"].AsNumber();
    const double window = smoothing["window_frames"].AsNumber();
    read.hitchTicks = smoothing["hitch_ticks"].AsNumber();
    if (!(read.aboveHz >= 0.0)) {
        error = path + ": smoothing.above_hz is not negative (0 never smooths)";
        return false;
    }
    // The band's floor sits at or under its line, or the switch could never let go.
    if (!(read.releaseBelowHz >= 0.0) || read.releaseBelowHz > read.aboveHz) {
        error = path + ": smoothing.release_below_hz is from 0 to above_hz";
        return false;
    }
    if (!(window >= 1.0) || window != std::floor(window)) {
        error = path + ": smoothing.window_frames is a whole number of frames, at least 1";
        return false;
    }
    read.windowFrames = static_cast<int>(window);
    if (!(read.hitchTicks > 1.0)) {
        error = path + ": smoothing.hitch_ticks is above 1 (a tick's own frame is not a stall)";
        return false;
    }
    out = read;
    return true;
}

void RateWatch::Frame(const Rules& rules, double frameS, double tickS) {
    // Never smoothing is the original's: nothing is measured, and a switch turned off
    // while it was on lets go at once.
    if (!(rules.aboveHz > 0.0)) {
        m_smoothing = false;
        return;
    }
    if (!(frameS > 0.0) || !(tickS > 0.0)) return;
    // A stall says nothing of the display: the mean keeps what it had.
    if (frameS > tickS * rules.hitchTicks) return;
    // Seeded at the tick's own length, the rate the port is matched at, so the first
    // frames of a run draw at the tick's place and the mean walks to what it sees.
    if (!(m_periodS > 0.0)) m_periodS = tickS;
    const double share = 1.0 / static_cast<double>(rules.windowFrames);
    m_periodS += (frameS - m_periodS) * share;
    const double hz = 1.0 / m_periodS;
    if (!m_smoothing && hz > rules.aboveHz) {
        m_smoothing = true;
    } else if (m_smoothing && hz < rules.releaseBelowHz) {
        m_smoothing = false;
    }
}

} // namespace MagicPortals::Pictures
