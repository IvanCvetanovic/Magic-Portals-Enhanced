#include "sim/EdgeCover.hpp"

#include "core/Json.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <utility>

namespace MagicPortals::EdgeCover {

namespace Json = Supersonic::Json;

bool LoadRules(const std::string& path, Rules& out, std::string& error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = path + ": cannot read";
        return false;
    }
    std::ostringstream text;
    text << file.rdbuf();

    // Named, not a temporary: the parser keeps a reference to what it reads.
    const std::string content = text.str();
    Json::Parser parser(content);
    Json::Value root;
    if (!parser.Parse(root)) {
        error = path + ": " + parser.Error();
        return false;
    }
    if (!root.Has("edge_cover")) {
        error = path + ": edge_cover is missing";
        return false;
    }
    const Json::Value& block = root["edge_cover"];
    const auto number = [&](const char* key, double& into) -> bool {
        if (!block.Has(key) || !block[key].IsNumber()) {
            error = path + ": edge_cover." + key + " is missing or not a number";
            return false;
        }
        into = block[key].AsNumber();
        return true;
    };

    Rules read;
    if (!block.Has("enabled") || !block["enabled"].IsBool()) {
        error = path + ": edge_cover.enabled is missing or not true or false";
        return false;
    }
    read.enabled = block["enabled"].AsBool();
    if (!block.Has("complement") || !block["complement"].IsBool()) {
        error = path + ": edge_cover.complement is missing or not true or false";
        return false;
    }
    read.complement = block["complement"].AsBool();
    if (!block.Has("log_every_frame") || !block["log_every_frame"].IsBool()) {
        error = path + ": edge_cover.log_every_frame is missing or not true or false";
        return false;
    }
    read.logEveryFrame = block["log_every_frame"].AsBool();
    double pool = 0.0;
    if (!number("band_px", read.bandPx) || !number("first_sample_px", read.firstSamplePx) ||
        !number("turn_rad", read.turnRad) || !number("pool_size", pool)) {
        return false;
    }
    // Each of these would draw a strip that is not the pixel-centre rule: a band past the
    // centre covers pixels whose centres the picture does not, and a first sample past
    // the band is a gap no strip is ever asked for.
    if (!(read.bandPx > 0.0) || read.bandPx > 0.5) {
        error = path + ": edge_cover.band_px is above 0 and at most 0.5, the centre of the edge pixel";
        return false;
    }
    if (read.firstSamplePx < 0.0 || !(read.firstSamplePx < read.bandPx)) {
        error = path + ": edge_cover.first_sample_px is at least 0 and below band_px";
        return false;
    }
    if (read.turnRad < 0.0) {
        error = path + ": edge_cover.turn_rad is not below 0";
        return false;
    }
    if (pool < 0.0 || pool > 256.0 || pool != std::floor(pool)) {
        error = path + ": edge_cover.pool_size is a whole number from 0 to 256";
        return false;
    }
    read.poolSize = static_cast<std::size_t>(pool);
    out = std::move(read);
    return true;
}

Interval Snap(const Rules& rules, const Interval& picture, double framePx) {
    Interval out = picture;
    if (!(picture.hi > picture.lo) || !(framePx > 0.0)) return out;
    // One of the two outermost pixels, [from, from + 1], whose centre lies at `centre`. A picture
    // draws it when it covers that centre: its low edge at or before it, its high edge past it. Where
    // it does, each end that lies inside the pixel goes out to the pixel's boundary when it leaves a
    // sample uncovered: the low edge when it is past the pixel's first sample (a sample exactly on a
    // low edge is drawn), the high edge when it is short of its last (one exactly on a high edge is
    // not). `lowFacesSide` says which end looks toward the frame's own side - the low edge of the near
    // pixel, the high edge of the far one - which the first half of the rule has always drawn out;
    // the other end faces into the frame and is the complementary half's.
    const auto fill = [&](double from, double centre, bool lowFacesSide) {
        if (!(picture.lo <= centre && picture.hi > centre)) return;
        const double to = from + 1.0;
        if (picture.lo > from + rules.firstSamplePx && (lowFacesSide || rules.complement)) out.lo = from;
        if (picture.hi < to && to - picture.hi >= rules.firstSamplePx && (!lowFacesSide || rules.complement)) out.hi = to;
    };
    fill(0.0, rules.bandPx, true);
    fill(framePx - 1.0, framePx - rules.bandPx, false);
    return out;
}

namespace {

// How far past the frame a strip's long axis runs: a whole pixel, past which nothing is drawn.
constexpr double kCutMarginPx = 1.0;

// A strip's long axis, cut to a pixel beyond the frame: past it nothing is drawn, and the
// strip's uv is linear, so the cut moves no texel.
Interval Cut(const Interval& along, double framePx) {
    return Interval{std::max(along.lo, -kCutMarginPx), std::min(along.hi, framePx + kCutMarginPx)};
}

void Add(Strips& out, Side side, const Interval& x, const Interval& y) {
    if (out.count >= Strips::kMax || x.Empty() || y.Empty()) return;
    out.strip[out.count] = Strip{side, Rect{x, y}};
    ++out.count;
}

} // namespace

Strips StripsOf(const Rules& rules, const Rect& picture, double frameWidthPx, double frameHeightPx) {
    Strips out;
    if (picture.x.Empty() || picture.y.Empty()) return out;
    const Interval extendedX = Snap(rules, picture.x, frameWidthPx);
    const Interval extendedY = Snap(rules, picture.y, frameHeightPx);
    const Interval rows = Cut(picture.y, frameHeightPx);
    const Interval columns = Cut(extendedX, frameWidthPx);
    if (extendedX.lo < picture.x.lo) Add(out, Side::Left, Interval{extendedX.lo, picture.x.lo}, rows);
    if (extendedX.hi > picture.x.hi) Add(out, Side::Right, Interval{picture.x.hi, extendedX.hi}, rows);
    if (extendedY.lo < picture.y.lo) Add(out, Side::Top, columns, Interval{extendedY.lo, picture.y.lo});
    if (extendedY.hi > picture.y.hi) Add(out, Side::Bottom, columns, Interval{picture.y.hi, extendedY.hi});
    return out;
}

namespace {

// The part of one axis of a picture that the frame shows, `along` cut to [0, framePx].
Interval OnFrame(const Interval& along, double framePx) {
    return Interval{std::max(along.lo, 0.0), std::min(along.hi, framePx)};
}

// Whether `shown`, the part of an axis the frame shows, lies wholly in the first pixel or wholly
// in the last of the axis and covers that pixel's centre nowhere.
bool InOneEdgePixelWithoutItsCentre(const Rules& rules, const Interval& shown, double framePx) {
    if (shown.hi <= 1.0) return !(shown.lo <= rules.bandPx && shown.hi > rules.bandPx);
    if (shown.lo >= framePx - 1.0) {
        const double centre = framePx - rules.bandPx;
        return !(shown.lo <= centre && shown.hi > centre);
    }
    return false;
}

} // namespace

bool Culled(const Rules& rules, const Rect& picture, double frameWidthPx, double frameHeightPx) {
    if (!rules.complement || !(frameWidthPx > 0.0) || !(frameHeightPx > 0.0)) return false;
    const Interval shownX = OnFrame(picture.x, frameWidthPx);
    const Interval shownY = OnFrame(picture.y, frameHeightPx);
    if (shownX.Empty() || shownY.Empty()) return false;
    return InOneEdgePixelWithoutItsCentre(rules, shownX, frameWidthPx) ||
           InOneEdgePixelWithoutItsCentre(rules, shownY, frameHeightPx);
}

Slivers SliversOf(const Rules& rules, const Rect& picture, double frameWidthPx, double frameHeightPx) {
    Slivers out;
    if (!(frameWidthPx > 0.0) || !(frameHeightPx > 0.0) || picture.x.Empty() || picture.y.Empty()) return out;
    if (OnFrame(picture.x, frameWidthPx).Empty() || OnFrame(picture.y, frameHeightPx).Empty()) return out;
    // One axis: the picture's low edge in the inner half of the first pixel (past its centre, no
    // later than its last sample) with the picture going on past that pixel, or its high edge in
    // the inner half of the last one with the picture going on before it.
    const auto axis = [&](const Interval& along, double framePx, Side nearSide, Side farSide) {
        if (along.lo > rules.bandPx && along.lo <= 1.0 - rules.firstSamplePx && along.hi > 1.0) {
            out.side[static_cast<std::size_t>(nearSide)] = true;
        }
        const double from = framePx - 1.0;
        if (along.hi <= framePx - rules.bandPx && along.hi > from + rules.firstSamplePx && along.lo < from) {
            out.side[static_cast<std::size_t>(farSide)] = true;
        }
    };
    axis(picture.x, frameWidthPx, Side::Left, Side::Right);
    axis(picture.y, frameHeightPx, Side::Top, Side::Bottom);
    return out;
}

Uv UvOf(const Rect& picture, const Rect& strip, const glm::dvec2& baseScale, const glm::dvec2& baseOffset) {
    const double width = picture.x.hi - picture.x.lo;
    const double height = picture.y.hi - picture.y.lo;
    if (!(width > 0.0) || !(height > 0.0)) return Uv{baseScale, baseOffset};
    Uv out;
    out.scale = glm::dvec2(baseScale.x * (strip.x.hi - strip.x.lo) / width,
                           baseScale.y * (strip.y.hi - strip.y.lo) / height);
    out.offset = glm::dvec2(baseOffset.x + baseScale.x * (strip.x.lo - picture.x.lo) / width,
                            baseOffset.y + baseScale.y * (strip.y.lo - picture.y.lo) / height);
    return out;
}

} // namespace MagicPortals::EdgeCover
