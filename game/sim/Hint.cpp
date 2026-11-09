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
    if (hint.Has("max_width_units") && (!hint["max_width_units"].IsNumber() || !(hint["max_width_units"].AsNumber() > 0.0))) {
        error = path + ": hint.max_width_units is not a number above zero";
        return false;
    }
    if (hint.Has("max_width_units")) read.maxWidthUnits = hint["max_width_units"].AsNumber();
    if (hint.Has("mirror_when_rtl")) {
        if (!hint["mirror_when_rtl"].IsBool()) {
            error = path + ": hint.mirror_when_rtl is not true or false";
            return false;
        }
        read.mirrorWhenRtl = hint["mirror_when_rtl"].AsBool();
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
    // 1-21's four are all FloatColor(-1): no alpha is written, and each is 1.
    if (!hint.Has("hand_waypoints") ||
        !ParseWaypoints(hint["hand_waypoints"], false, path + ": hint.hand_waypoints", read.handWaypoints, error)) {
        if (error.empty()) error = path + ": hint.hand_waypoints is missing";
        return false;
    }
    out = read;
    return true;
}

bool ParseWaypoints(const Json::Value& list, bool withAlpha, const std::string& where, std::vector<Waypoint>& out,
                    std::string& error) {
    out.clear();
    if (!list.IsArray() || list.AsArray().empty()) {
        error = where + " is not a list of waypoints";
        return false;
    }
    for (const Json::Value& item : list.AsArray()) {
        Waypoint waypoint;
        if (!item.IsObject() || !Pair(item["at_units"], waypoint.atUnits) || !item["ms"].IsNumber() ||
            item["ms"].AsNumber() < 0.0 || !item["filter"].IsString() ||
            !Ease::ParseFilter(item["filter"].AsString(), waypoint.filter)) {
            error = where + ": each waypoint needs at_units (two numbers), ms (not below 0) and a filter Ease names";
            return false;
        }
        waypoint.ms = item["ms"].AsNumber();
        if (withAlpha) {
            if (!item["alpha"].IsNumber() || item["alpha"].AsNumber() < 0.0 || item["alpha"].AsNumber() > 1.0) {
                error = where + ": each waypoint needs its colour's alpha, 0 to 1";
                return false;
            }
            waypoint.alpha = item["alpha"].AsNumber();
        }
        out.push_back(waypoint);
    }
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
    // #309-310 WaypointManager::update.
    Round::Tick(rules.handWaypoints, frameMs);
}

void Round::Tick(const std::vector<Waypoint>& points, double frameMs) {
    // FrameTimer::set(0, count - 1, this waypoint's time, repeat) (23254..23650) -
    // #3-4 the stride, #6-11 the time added, #13-43 a changed first or last starts
    // again at first with no time, else #45-76 ONE frame once the time reaches the
    // stride, the stride taken off, and past last this REPEATING manager goes back
    // to first with the rest kept (#64-68) - then setCurrentStride(the new
    // waypoint's time).
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

glm::dvec2 Hand::OffsetUnits(const Rules& rules) const { return Round::OffsetUnits(rules.handWaypoints); }

glm::dvec2 Round::OffsetUnits(const std::vector<Waypoint>& points) const {
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

double Round::Alpha(const std::vector<Waypoint>& points) const {
    if (points.empty()) return 1.0;
    // #82-101: the colours interpolated by the same filtered bias.
    const Waypoint& current = points[static_cast<std::size_t>(frame)];
    const Waypoint& next = points[static_cast<std::size_t>(frame == last ? first : frame + 1)];
    const double bias = std::min(strideMs, timeMs) / std::max(strideMs, 1.0);
    const double f = Ease::Apply(current.filter, bias);
    return current.alpha + (next.alpha - current.alpha) * f;
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

glm::dvec2 TextTopLeft(const Rules& rules, const glm::dvec2& viewUnits, bool rtl, double textWidthUnits) {
    if (rtl && rules.mirrorWhenRtl) return glm::dvec2(viewUnits.x - rules.leftUnits - textWidthUnits, viewUnits.y - rules.aboveBottomUnits);
    return glm::dvec2(rules.leftUnits, viewUnits.y - rules.aboveBottomUnits);
}

} // namespace MagicPortals::Hint
