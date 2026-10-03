#include "sim/ScriptClock.hpp"

#include "core/Json.hpp"

#include <cmath>
#include <fstream>
#include <sstream>

namespace MagicPortals::ScriptClock {

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
    if (!root["matched_hz"].IsNumber() || !root["snap_ms"].IsNumber()) {
        error = path + ": matched_hz and snap_ms are numbers";
        return false;
    }
    Rules read;
    const double hz = root["matched_hz"].AsNumber();
    // Whole hertz, and a rate whose frame is at least a millisecond: the floor of
    // anything shorter is 0, a script clock that never advances.
    if (!(hz >= 1.0) || hz != std::floor(hz) || !(1000.0 / hz >= 1.0)) {
        error = path + ": matched_hz is a whole number of hertz from 1 to 1000";
        return false;
    }
    read.matchedHz = static_cast<int>(hz);
    read.snapMs = root["snap_ms"].AsNumber();
    if (!(read.snapMs >= 0.0)) {
        error = path + ": snap_ms is not negative";
        return false;
    }
    read.wallFrameMs = 1000.0 / hz;
    read.frameMs = FloorMs(read.wallFrameMs);
    if (!root["physics_step_s"].IsNumber() || !(root["physics_step_s"].AsNumber() > 0.0)) {
        error = path + ": physics_step_s is a number above 0";
        return false;
    }
    read.physicsStepS = root["physics_step_s"].AsNumber();
    out = read;
    return true;
}

unsigned FloorMs(double frameMs) {
    // __aeabi_f2uiz: the truncation toward zero, and 0 for what is not above it.
    if (!(frameMs > 0.0)) return 0;
    return static_cast<unsigned>(frameMs);
}

Step TickStep(const Rules& rules, float physicsS) {
    // n / 1000.0 and back by * 1000.0 is exact for every whole n up to 1000 (the
    // suite checks all of them), so a sum of ticks is the original's integer sum.
    return Step(static_cast<double>(rules.frameMs) / 1000.0, physicsS, rules.physicsStepS,
                static_cast<double>(rules.matchedHz));
}

double OfFrameMs(const Rules& rules, double frameMs, double tickMs) {
    if (!(frameMs > 0.0) || !(tickMs > 0.0)) return 0.0;
    if (std::fabs(frameMs - tickMs) <= rules.snapMs) return static_cast<double>(rules.frameMs);
    return frameMs * static_cast<double>(rules.frameMs) / tickMs;
}

} // namespace MagicPortals::ScriptClock
