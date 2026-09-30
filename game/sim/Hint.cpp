#include "sim/Hint.hpp"

#include "core/Json.hpp"

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <sstream>

namespace MagicPortals::Hint {

namespace {

namespace Json = Supersonic::Json;

bool Pair(const Json::Value& value, glm::dvec2& out) {
    const Json::Array& items = value.AsArray();
    if (!value.IsArray() || items.size() != 2 || !items[0].IsNumber() || !items[1].IsNumber()) return false;
    out = glm::dvec2(items[0].AsNumber(), items[1].AsNumber());
    return true;
}

} // namespace

bool LoadRules(const std::string& path, Rules& out, std::string& error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = path + ": cannot open";
        return false;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string text = buffer.str();
    Json::Parser parser(text);
    Json::Value root;
    if (!parser.Parse(root)) {
        error = path + ": " + parser.Error();
        return false;
    }
    if (!root.IsObject() || !root.Has("hint") || !root["hint"].IsObject()) {
        error = path + ": no \"hint\" object";
        return false;
    }
    const Json::Value& hint = root["hint"];
    Rules read;
    const struct {
        const char* key;
        std::string* to;
    } strings[] = {{"hand_entity", &read.handEntity},
                   {"arrow_entity", &read.arrowEntity},
                   {"text", &read.text},
                   {"font", &read.font}};
    for (const auto& item : strings) {
        if (!hint.Has(item.key) || !hint[item.key].IsString() || hint[item.key].AsString().empty()) {
            error = path + ": hint." + item.key + " is missing or not a name";
            return false;
        }
        *item.to = hint[item.key].AsString();
    }
    const struct {
        const char* key;
        double* to;
    } numbers[] = {{"units_per_font_px", &read.unitsPerFontPx},
                   {"left_units", &read.leftUnits},
                   {"above_bottom_units", &read.aboveBottomUnits},
                   {"fade_ms", &read.fadeMs},
                   {"life_ms", &read.lifeMs},
                   {"hand_alpha_gain", &read.handAlphaGain},
                   {"arrow_alpha", &read.arrowAlpha},
                   {"arrow_alpha_gain", &read.arrowAlphaGain},
                   {"arrow_bounce_stride_ms", &read.arrowBounce.strideMs}};
    for (const auto& item : numbers) {
        if (!hint.Has(item.key) || !hint[item.key].IsNumber()) {
            error = path + ": hint." + item.key + " is missing or not a number";
            return false;
        }
        *item.to = hint[item.key].AsNumber();
    }
    if (read.unitsPerFontPx <= 0.0 || read.fadeMs <= 0.0 || read.lifeMs <= 0.0 || read.arrowBounce.strideMs <= 0.0) {
        error = path + ": hint's units_per_font_px, fade_ms, life_ms and arrow_bounce_stride_ms must be above 0";
        return false;
    }
    if (!Pair(hint["arrow_bounce_from"], read.arrowBounce.fromScale) ||
        !Pair(hint["arrow_bounce_to"], read.arrowBounce.toScale)) {
        error = path + ": hint.arrow_bounce_from and _to must each be two numbers";
        return false;
    }
    if (!hint.Has("hand_waypoints") || !hint["hand_waypoints"].IsArray() ||
        hint["hand_waypoints"].AsArray().empty()) {
        error = path + ": hint.hand_waypoints is missing or empty";
        return false;
    }
    for (const Json::Value& item : hint["hand_waypoints"].AsArray()) {
        Waypoint waypoint;
        if (!item.IsObject() || !Pair(item["at_units"], waypoint.atUnits) || !item["ms"].IsNumber() ||
            item["ms"].AsNumber() < 0.0 || !item["filter"].IsString() ||
            !Ease::ParseFilter(item["filter"].AsString(), waypoint.filter)) {
            error = path + ": each of hint.hand_waypoints needs at_units (two numbers), ms (not below 0) and a "
                           "filter Ease names";
            return false;
        }
        waypoint.ms = item["ms"].AsNumber();
        read.handWaypoints.push_back(waypoint);
    }
    out = read;
    return true;
}

void Hand::Tick(const Rules& rules, double frameMs) {
    if (gone) return;
    // #245-259: lifeTime += getLastFrameElapsedTime(), on the first call too.
    lifeMs += frameMs;
    // #273-285: past its life it is deleted, and nothing after runs.
    if (lifeMs > rules.lifeMs) {
        gone = true;
        return;
    }
    // #309-310 WaypointManager::update: FrameTimer::set(0, count - 1, this waypoint's
    // time, repeat) (23254..23650) - #3-4 the stride, #6-11 the time added, #13-43 a
    // changed first or last starts again at first with no time, else #45-76 ONE
    // frame once the time reaches the stride, the stride taken off, and past last
    // this REPEATING manager goes back to first with the rest kept (#64-68) - then
    // setCurrentStride(the new waypoint's time).
    const std::vector<Waypoint>& points = rules.handWaypoints;
    if (points.empty()) return;
    const int wantLast = static_cast<int>(points.size()) - 1;
    strideMs = points[static_cast<std::size_t>(frame)].ms;
    timeMs += frameMs;
    if (first != 0 || last != wantLast) {
        first = 0;
        last = wantLast;
        frame = 0;
        timeMs = 0.0;
    } else if (timeMs >= strideMs) {
        ++frame;
        timeMs -= strideMs;
        if (frame > last) frame = first;
    }
    strideMs = points[static_cast<std::size_t>(frame)].ms;
}

glm::dvec2 Hand::OffsetUnits(const Rules& rules) const {
    const std::vector<Waypoint>& points = rules.handWaypoints;
    if (points.empty()) return glm::dvec2(0.0);
    // getCurrentPoint (41513..42094): #3-28 the next waypoint - the first again after
    // the last, as it repeats - and #54-123 THIS waypoint's filter of the raw bias,
    // min(stride, time) / max(stride, 1) (FrameTimer::getBias).
    const Waypoint& current = points[static_cast<std::size_t>(frame)];
    const Waypoint& next = points[static_cast<std::size_t>(frame == last ? first : frame + 1)];
    const double bias = std::min(strideMs, timeMs) / std::max(strideMs, 1.0);
    const double f = Ease::Apply(current.filter, bias);
    return current.atUnits + (next.atUnits - current.atUnits) * f;
}

double Hand::Alpha(const Rules& rules) const {
    // #354-378: every waypoint's colour is FloatColor(-1), whose alpha is 1.
    return std::clamp((1.0 - lifeMs / rules.lifeMs) * rules.handAlphaGain, 0.0, 1.0);
}

void Arrow::Tick(const Rules& rules, double frameMs) {
    if (gone) return;
    lifeMs += frameMs;
    if (lifeMs > rules.lifeMs) gone = true;
}

double Arrow::Alpha(const Rules& rules) const {
    // #69-88: (1 - lifeTime / 20000) x 0.3 x 2, clamped.
    return std::clamp((1.0 - lifeMs / rules.lifeMs) * rules.arrowAlpha * rules.arrowAlphaGain, 0.0, 1.0);
}

glm::dvec2 Arrow::Scale(const Rules& rules) const {
    // bounce's blinkElapsedTime starts at 0 on the first call and adds the same
    // frames lifeTime does.
    return rules.arrowBounce.ScaleAt(lifeMs);
}

double TextAlpha(const Rules& rules, double ageMs) {
    // ETHTextDrawer::Draw (ETHDrawable.cpp:64-75): (GS_BYTE)((1 - clamp(elapsed /
    // time, 0, 1)) x 255); IsAlive is false from elapsed >= time.
    if (ageMs >= rules.fadeMs) return 0.0;
    const double fade = 1.0 - std::clamp(ageMs / rules.fadeMs, 0.0, 1.0);
    return static_cast<double>(static_cast<int>(fade * 255.0)) / 255.0;
}

glm::dvec2 TextTopLeft(const Rules& rules, const glm::dvec2& viewUnits) {
    return glm::dvec2(rules.leftUnits, viewUnits.y - rules.aboveBottomUnits);
}

} // namespace MagicPortals::Hint
