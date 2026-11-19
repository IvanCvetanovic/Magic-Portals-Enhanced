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
    // The pace knob. Absent is the matched rate itself, one tick an original frame: a file written before the key
    // existed (and every matched_hz the suites load with no key) reads exactly as it always did.
    int originalHz = read.matchedHz;
    const char* const paceError = ": original_frame_hz is a whole number of hertz from 1 to matched_hz that divides it";
    if (root.Has("original_frame_hz")) {
        const Json::Value& key = root["original_frame_hz"];
        const double wanted = key.AsNumber();
        // Judged as a double first: a fraction, a string, a number past matched_hz or one that would not fit an int
        // must not be converted before it is refused.
        if (!key.IsNumber() || !(wanted >= 1.0) || !(wanted <= hz) || wanted != std::floor(wanted)) {
            error = path + paceError;
            return false;
        }
        originalHz = static_cast<int>(wanted);
    }
    if (!read.SetOriginalFrameHz(originalHz)) {
        error = path + paceError;
        return false;
    }
    out = read;
    return true;
}

bool Rules::SetOriginalFrameHz(int hz) {
    // Ordered so that the modulo below never divides by zero: hz is at least 1 and no more than matchedHz first.
    if (hz < 1 || hz > matchedHz || matchedHz % hz != 0) return false;
    originalFrameHz = hz;
    frameTicks = matchedHz / hz;
    return true;
}

unsigned FloorMs(double frameMs) {
    // __aeabi_f2uiz: the truncation toward zero, and 0 for what is not above it.
    if (!(frameMs > 0.0)) return 0;
    return static_cast<unsigned>(frameMs);
}

Step TickStep(const Rules& rules, float physicsS, unsigned levelStep) {
    // A Rules is made whole by SetOriginalFrameHz, which keeps frameTicks at 1 or more; one poked by hand to less
    // is read as 1 rather than divided by.
    const int ticksPerFrame = rules.frameTicks > 1 ? rules.frameTicks : 1;
    const unsigned phase = levelStep % static_cast<unsigned>(ticksPerFrame);
    // n / 1000.0 and back by * 1000.0 is exact for every whole n up to 1000 (the
    // suite checks all of them), so a sum of ticks is the original's integer sum.
    //
    // One tick advances 1 / frameTicks of the original's physics step: dividing by 1.0 returns the double
    // itself, so a one-tick frame hands the sim the number it always had, and halving is exact. `fps` is
    // what GetFPSRate answers at the pace the world stands for, which is the matched rate unless
    // original_frame_hz says otherwise.
    return Step(static_cast<double>(rules.frameMs) / 1000.0, physicsS, rules.physicsStepS / static_cast<double>(ticksPerFrame),
                static_cast<double>(rules.originalFrameHz), ticksPerFrame, static_cast<int>(phase));
}

double OfFrameMs(const Rules& rules, double frameMs, double tickMs) {
    if (!(frameMs > 0.0) || !(tickMs > 0.0)) return 0.0;
    if (std::fabs(frameMs - tickMs) <= rules.snapMs) return static_cast<double>(rules.frameMs);
    return frameMs * static_cast<double>(rules.frameMs) / tickMs;
}

} // namespace MagicPortals::ScriptClock
