#include "sim/SideFill.hpp"

#include "core/Json.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <utility>

namespace MagicPortals::SideFill {

namespace {

namespace Json = Supersonic::Json;

// The two modes' words in the file, in one place.
constexpr const char* kModeEdge = "edge";
constexpr const char* kModeMirror = "mirror";
constexpr int kMostSteps = 256;

bool Number(const Json::Value& block, const char* key, double& out, std::string& why, const std::string& where) {
    if (!block.Has(key) || !block[key].IsNumber() || !std::isfinite(block[key].AsNumber())) {
        why = where + "." + key + " is missing or not a number";
        return false;
    }
    out = block[key].AsNumber();
    return true;
}

// The darkness of a feather slice whose middle is `fraction` of the way from the seam to the window's edge.
float FeatherAlpha(const Picture& picture, double fraction) {
    return static_cast<float>(picture.maxAlpha * std::pow(std::clamp(fraction, 0.0, 1.0), picture.power));
}

} // namespace

bool LoadRules(const std::string& path, Rules& out, std::string& error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = path + ": cannot read";
        return false;
    }
    std::ostringstream text;
    text << file.rdbuf();
    const std::string content = text.str(); // named: the parser keeps a reference to what it reads
    Json::Parser parser(content);
    Json::Value root;
    if (!parser.Parse(root)) {
        error = path + ": " + parser.Error();
        return false;
    }
    if (!root.Has("side_fill") || !root["side_fill"].IsObject()) {
        error = path + ": side_fill is missing or not an object";
        return false;
    }
    const Json::Value& block = root["side_fill"];
    Rules read;
    std::string why;
    const auto fail = [&]() {
        error = path + ": " + why;
        return false;
    };
    if (!block.Has("enabled") || !block["enabled"].IsBool()) {
        why = "side_fill.enabled is missing or not true or false";
        return fail();
    }
    read.enabled = block["enabled"].AsBool();
    double steps = 0.0;
    if (!Number(block, "steps", steps, why, "side_fill") || steps < 1.0 || steps > kMostSteps || steps != std::floor(steps)) {
        if (why.empty()) why = "side_fill.steps is a whole number from 1 to " + std::to_string(kMostSteps);
        return fail();
    }
    read.steps = static_cast<int>(steps);
    if (!Number(block, "overlap_units", read.overlapUnits, why, "side_fill") || read.overlapUnits < 0.0 ||
        !Number(block, "min_gap_units", read.minGapUnits, why, "side_fill") || read.minGapUnits < 0.0) {
        if (why.empty()) why = "side_fill.overlap_units and min_gap_units are not negative";
        return fail();
    }
    if (!block.Has("pictures") || !block["pictures"].IsArray() || block["pictures"].AsArray().empty()) {
        why = "side_fill.pictures is missing or empty";
        return fail();
    }
    for (const Json::Value& item : block["pictures"].AsArray()) {
        Picture picture;
        if (!item.IsObject() || !item.Has("sprite") || !item["sprite"].IsString() || item["sprite"].AsString().empty() ||
            !item.Has("mode") || !item["mode"].IsString()) {
            why = "side_fill.pictures holds an entry with no sprite or no mode";
            return fail();
        }
        picture.sprite = item["sprite"].AsString();
        const std::string mode = item["mode"].AsString();
        if (mode == kModeEdge) {
            picture.mode = Mode::Edge;
        } else if (mode == kModeMirror) {
            picture.mode = Mode::Mirror;
        } else {
            why = "side_fill.pictures: " + picture.sprite + " has the mode \"" + mode + "\", which is neither \"edge\" nor \"mirror\"";
            return fail();
        }
        const std::string where = "side_fill.pictures[" + picture.sprite + "]";
        if (!item.Has("content_units") || !item["content_units"].IsArray() || item["content_units"].AsArray().size() != 2 ||
            !item["content_units"].AsArray()[0].IsNumber() || !item["content_units"].AsArray()[1].IsNumber()) {
            why = where + ".content_units is not two numbers";
            return fail();
        }
        picture.contentFrom = item["content_units"].AsArray()[0].AsNumber();
        picture.contentTo = item["content_units"].AsArray()[1].AsNumber();
        if (!(picture.contentFrom >= 0.0) || !(picture.contentTo > picture.contentFrom)) {
            why = where + ".content_units must run from a non-negative start to a larger end";
            return fail();
        }
        if (!Number(item, "sample_inset_units", picture.sampleInsetUnits, why, where) || picture.sampleInsetUnits < 0.0 ||
            picture.sampleInsetUnits * 2.0 >= picture.contentTo - picture.contentFrom ||
            !Number(item, "max_alpha", picture.maxAlpha, why, where) || picture.maxAlpha < 0.0 || picture.maxAlpha > 1.0 ||
            !Number(item, "power", picture.power, why, where) || !(picture.power > 0.0)) {
            if (why.empty()) why = where + ": sample_inset_units, max_alpha (0..1) or power (> 0) is out of range";
            return fail();
        }
        read.pictures.push_back(std::move(picture));
    }
    out = std::move(read);
    return true;
}

std::vector<Quad> Quads(const Rules& rules, const std::string& sprite, const Hud::Rect& picture, const glm::dvec2& viewUnits) {
    std::vector<Quad> quads;
    if (!rules.enabled) return quads;
    const auto known = std::find_if(rules.pictures.begin(), rules.pictures.end(),
                                    [&](const Picture& candidate) { return candidate.sprite == sprite; });
    if (known == rules.pictures.end() || picture.size.x <= 0.0 || picture.size.y <= 0.0) return quads;
    const Picture& pic = *known;

    // Where the picture's visible content ends at each side, and how far from there the window goes on.
    const double seamLeft = picture.min.x + pic.contentFrom;
    const double seamRight = picture.min.x + pic.contentTo;
    const double gapLeft = seamLeft;
    const double gapRight = viewUnits.x - seamRight;
    if (gapLeft <= rules.minGapUnits && gapRight <= rules.minGapUnits) return quads;
    // The fill takes the picture's own rows, and the uv rows that go with them.
    const double top = std::max(0.0, picture.min.y);
    const double bottom = std::min(viewUnits.y, picture.Max().y);
    if (!(bottom > top)) return quads;
    const double v0 = (top - picture.min.y) / picture.size.y;
    const double v1 = (bottom - picture.min.y) / picture.size.y;
    const double width = picture.size.x;
    const int steps = std::max(rules.steps, 1);

    // One side. `outward` is -1 at the left and +1 at the right; the seam is where the content ends, the gap runs from it to the window's edge.
    const auto side = [&](double seam, double gap, double outward) {
        if (gap <= rules.minGapUnits) return;
        Quad filled;
        filled.textured = true;
        const double far = seam + outward * gap;
        const double under = seam - outward * rules.overlapUnits; // runs under the picture's first texel
        const double lo = std::min(far, under);
        const double hi = std::max(far, under);
        filled.rect = Hud::Rect{glm::dvec2(lo, top), glm::dvec2(hi - lo, bottom - top)};
        if (pic.mode == Mode::Edge) {
            // One column of the outermost content, never the padding: the sampler clamps, the quad carries that column across.
            const double u = outward < 0.0 ? (pic.contentFrom + pic.sampleInsetUnits) / width : (pic.contentTo - pic.sampleInsetUnits) / width;
            filled.uvMin = glm::dvec2(u, v0);
            filled.uvMax = glm::dvec2(u, v1);
        } else {
            // The content reflected about the seam: the distance from the seam, turned into a place inside the content. Linear in x, so the two ends
            // say it all: at the left, u rises away from the seam (content start + distance); at the right it falls (content end - distance); the
            // pair is therefore reversed (a mirror) and the part under the picture runs on past the content's end by the overlap only.
            const auto u = [&](double x) {
                const double distance = (x - seam) * outward; // grows outward, negative under the picture
                return (outward < 0.0 ? pic.contentFrom + distance : pic.contentTo - distance) / width;
            };
            filled.uvMin = glm::dvec2(u(lo), v0);
            filled.uvMax = glm::dvec2(u(hi), v1);
        }
        quads.push_back(filled);
        // The feather: flat black, nothing at the seam and `maxAlpha` at the window's edge, in `steps` slices that do not overlap.
        for (int i = 0; i < steps; ++i) {
            const double from = gap * i / steps;
            const double to = gap * (i + 1) / steps;
            Quad slice;
            const double a = seam + outward * from;
            const double b = seam + outward * to;
            slice.rect = Hud::Rect{glm::dvec2(std::min(a, b), top), glm::dvec2(std::fabs(b - a), bottom - top)};
            slice.colour = glm::vec4(0.0f, 0.0f, 0.0f, FeatherAlpha(pic, (i + 0.5) / steps));
            slice.textured = false;
            quads.push_back(slice);
        }
    };
    side(seamLeft, gapLeft, -1.0);
    side(seamRight, gapRight, 1.0);
    return quads;
}

} // namespace MagicPortals::SideFill
