#include "sim/SideWalls.hpp"

#include "core/Json.hpp"
#include "sim/LevelBuilder.hpp"
#include "sim/Roles.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <utility>

namespace MagicPortals::SideWalls {

namespace {

namespace Json = Supersonic::Json;

constexpr double kPi = 3.14159265358979323846;
constexpr const char* kBlock = "side_walls";
constexpr const char* kExtension = ".ent";
constexpr int kMostColumns = 8;
constexpr int kMostFadeSteps = 256;
// A turn, in radians, a wall's rectangle may carry and still be read as upright; and the gap, in units, two of them may leave and still
// meet: the level file's own rounding, nothing a player could pass.
constexpr double kUpright = 1e-6;
constexpr double kMeets = 1e-6;
// The node keys that say a piece breaks or moves: such a piece closes nothing, whatever its name.
constexpr const char* kBreaksOrMoves[] = {"breakable", "stride", "speed", "moveStride"};

// One reading of the file: the first key that is wrong is the one named.
struct Reader {
    const Json::Value& block;
    std::string why;

    bool Fail(const std::string& message) {
        if (why.empty()) why = message;
        return false;
    }
    static std::string Where(const char* key) { return std::string(kBlock) + "." + key; }

    // A finite number from `lo` up (`lo` itself too, unless `above`).
    bool Number(const char* key, double& out, double lo, bool above = false) {
        if (!block.Has(key) || !block[key].IsNumber() || !std::isfinite(block[key].AsNumber())) {
            return Fail(Where(key) + " is missing or not a number");
        }
        out = block[key].AsNumber();
        if (above ? !(out > lo) : !(out >= lo)) {
            return Fail(Where(key) + (above ? " is not above " : " is below ") + std::to_string(lo));
        }
        return true;
    }
    bool Count(const char* key, int& out, int most) {
        double value = 0.0;
        if (!Number(key, value, 1.0)) return false;
        if (value > most || value != std::floor(value)) {
            return Fail(Where(key) + " is a whole number from 1 to " + std::to_string(most));
        }
        out = static_cast<int>(value);
        return true;
    }
    bool Name(const char* key, std::string& out) {
        if (!block.Has(key) || !block[key].IsString() || block[key].AsString().empty()) {
            return Fail(Where(key) + " is missing or not a name");
        }
        out = block[key].AsString();
        return true;
    }
    // A list of at least one string; an empty string is one only where `emptyIsOne` (a variant's suffix).
    bool Names(const char* key, std::vector<std::string>& out, bool emptyIsOne) {
        if (!block.Has(key) || !block[key].IsArray() || block[key].AsArray().empty()) {
            return Fail(Where(key) + " is missing or empty");
        }
        for (const Json::Value& item : block[key].AsArray()) {
            if (!item.IsString() || (!emptyIsOne && item.AsString().empty())) {
                return Fail(Where(key) + " holds something that is not a name");
            }
            out.push_back(item.AsString());
        }
        return true;
    }
};

std::string WithoutExtension(const std::string& name) {
    const std::string extension = kExtension;
    if (name.size() > extension.size() && name.compare(name.size() - extension.size(), extension.size(), extension) == 0) {
        return name.substr(0, name.size() - extension.size());
    }
    return name;
}

bool Upright(const Tscn::Node& node) {
    const Tscn::Value* turn = node.Find("rotation");
    double radians = 0.0;
    return turn == nullptr || (turn->AsNumber(radians) && std::fabs(radians) <= kUpright);
}

glm::dvec2 VectorOf(const Tscn::Value* value) {
    if (value == nullptr || value->kind != Tscn::Value::Kind::Vector2) return glm::dvec2(0.0);
    return glm::dvec2(value->numbers[0], value->numbers[1]);
}

// The rectangle a wall's node closes with, in the level's units; false for anything that is not a wall.
bool WallRect(const Rules& rules, const Tscn::Scene& scene, const Tscn::Node& node, glm::dvec2& lo, glm::dvec2& hi) {
    if (node.parent != "." || node.type != "Node2D") return false;
    const std::string name = Roles::EntityName(node);
    if (std::find(rules.solidEntities.begin(), rules.solidEntities.end(), name) == rules.solidEntities.end()) return false;
    for (const char* key : kBreaksOrMoves) {
        if (node.Meta(key) != nullptr) return false;
    }
    const Tscn::Node* body = scene.Child(node, "Body");
    if (body == nullptr || body->type != "StaticBody2D") return false;
    const Tscn::Node* shape = scene.Child(*body, "Shape");
    if (shape == nullptr || shape->type != "CollisionShape2D") return false;
    if (!Upright(node) || !Upright(*body) || !Upright(*shape)) return false;
    const Tscn::Value* ref = shape->Find("shape");
    const Tscn::Resource* resource = ref != nullptr ? scene.Embedded(ref->text) : nullptr;
    if (resource == nullptr || resource->type != "RectangleShape2D") return false;
    double scale = 1.0;
    std::string why;
    if (!LevelBuilder::ReadCustomScale(node, scale, why)) return false;
    const glm::dvec2 size = VectorOf(resource->Find("size")) * scale;
    if (!(size.x > 0.0) || !(size.y > 0.0)) return false;
    const glm::dvec2 centre = VectorOf(node.Find("position")) +
                              (VectorOf(body->Find("position")) + VectorOf(shape->Find("position"))) * scale;
    lo = centre - size * 0.5;
    hi = centre + size * 0.5;
    return true;
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
    if (!root.Has(kBlock) || !root[kBlock].IsObject()) {
        error = path + ": " + kBlock + " is missing or not an object";
        return false;
    }
    Reader in{root[kBlock], {}};
    Rules read;
    const auto fail = [&]() {
        error = path + ": " + in.why;
        return false;
    };
    if (!in.block.Has("enabled") || !in.block["enabled"].IsBool()) {
        in.Fail(Reader::Where("enabled") + " is missing or not true or false");
        return fail();
    }
    read.enabled = in.block["enabled"].AsBool();
    double turnDeg = 0.0;
    if (!in.Number("closed_reach_units", read.closedReachUnits, 0.0, true) ||
        !in.Names("solid_entities", read.solidEntities, false) || !in.Name("stone_entity", read.stoneEntity) ||
        !in.Name("black_entity", read.blackEntity) || !in.Names("block_families", read.families, false) ||
        !in.Names("variants", read.variants, true) || !in.Names("world_default_variants", read.worldVariants, true) ||
        !in.Number("turn_deg", turnDeg, -360.0) || !in.Number("light_height_units", read.lightHeight, -1000.0) ||
        !in.Count("columns", read.columns, kMostColumns) || !in.Number("column_step_units", read.columnStepUnits, 0.0, true) ||
        !in.Number("row_step_units", read.rowStepUnits, 0.0, true) ||
        !in.Number("backing_reach_units", read.backingReachUnits, 0.0, true) || !in.Number("margin_units", read.marginUnits, 0.0) ||
        !in.Number("fade_from_units", read.fadeFromUnits, 0.0) ||
        !in.Number("fade_to_units", read.fadeToUnits, read.fadeFromUnits, true) ||
        !in.Count("fade_steps", read.fadeSteps, kMostFadeSteps) || !in.Number("fade_cap_units", read.fadeCapUnits, 0.0, true) ||
        !in.Number("z_first", read.zFirst, 0.0, true) || !in.Number("z_step", read.zStep, 0.0, true)) {
        return fail();
    }
    read.turnRad = turnDeg * kPi / 180.0;
    for (const std::string& variant : read.worldVariants) {
        if (std::find(read.variants.begin(), read.variants.end(), variant) == read.variants.end()) {
            in.Fail(Reader::Where("world_default_variants") + " names \"" + variant + "\", which is not one of the variants");
            return fail();
        }
    }
    if (!in.block.Has("row_start_units") || !in.block["row_start_units"].IsArray() ||
        in.block["row_start_units"].AsArray().empty()) {
        in.Fail(Reader::Where("row_start_units") + " is missing or empty");
        return fail();
    }
    for (const Json::Value& item : in.block["row_start_units"].AsArray()) {
        if (!item.IsNumber() || !std::isfinite(item.AsNumber())) {
            in.Fail(Reader::Where("row_start_units") + " holds something that is not a number");
            return fail();
        }
        read.rowStartUnits.push_back(item.AsNumber());
    }
    if (read.backingReachUnits < read.fadeToUnits + read.fadeCapUnits) {
        in.Fail(Reader::Where("backing_reach_units") + " stops short of the fade's end");
        return fail();
    }
    out = std::move(read);
    return true;
}

Closed ClosedSides(const Rules& rules, const Tscn::Scene& scene, const glm::dvec2& boundsPx) {
    Closed closed;
    if (!(boundsPx.x > 0.0) || !(boundsPx.y > 0.0)) return closed;
    const auto covers = [&](double x) {
        std::vector<std::pair<double, double>> spans;
        for (const Tscn::Node& node : scene.nodes) {
            glm::dvec2 lo(0.0), hi(0.0);
            if (WallRect(rules, scene, node, lo, hi) && lo.x <= x && x <= hi.x) spans.emplace_back(lo.y, hi.y);
        }
        std::sort(spans.begin(), spans.end());
        double reached = 0.0;
        for (const auto& [top, bottom] : spans) {
            if (top > reached + kMeets) break;
            reached = std::max(reached, bottom);
        }
        return reached >= boundsPx.y - kMeets;
    };
    closed.left = covers(-rules.closedReachUnits);
    closed.right = covers(boundsPx.x + rules.closedReachUnits);
    return closed;
}

std::string Variant(const Rules& rules, const Tscn::Scene& scene, int world) {
    std::vector<int> placed(rules.variants.size(), 0);
    for (const Tscn::Node& node : scene.nodes) {
        if (node.parent != ".") continue;
        const std::string name = WithoutExtension(Roles::EntityName(node));
        for (std::size_t i = 0; i < rules.variants.size(); ++i) {
            for (const std::string& family : rules.families) {
                if (name == family + rules.variants[i]) ++placed[i];
            }
        }
    }
    const bool known = world >= 0 && static_cast<std::size_t>(world) < rules.worldVariants.size();
    const std::string fallback = known ? rules.worldVariants[static_cast<std::size_t>(world)]
                                       : (rules.variants.empty() ? std::string() : rules.variants.front());
    const int most = placed.empty() ? 0 : *std::max_element(placed.begin(), placed.end());
    if (most == 0) return fallback;
    for (std::size_t i = 0; i < placed.size(); ++i) {
        if (placed[i] == most && rules.variants[i] == fallback) return fallback;
    }
    for (std::size_t i = 0; i < placed.size(); ++i) {
        if (placed[i] == most) return rules.variants[i];
    }
    return fallback;
}

std::string StoneFile(const Rules& rules, const std::string& variant) {
    return rules.stoneEntity + variant + kExtension;
}

std::vector<Piece> Layout(const Rules& rules, const glm::dvec2& boundsPx, const glm::dvec2& stonePx) {
    std::vector<Piece> pieces;
    if (!rules.enabled || !(boundsPx.x > 0.0) || !(boundsPx.y > 0.0) || !(stonePx.x > 0.0) || !(stonePx.y > 0.0) ||
        !(rules.rowStepUnits > 0.0) || rules.rowStartUnits.empty()) {
        return pieces;
    }
    const double top = -rules.marginUnits;
    const double bottom = boundsPx.y + rules.marginUnits;
    constexpr int kSides[] = {-1, 1};
    const auto edgeOf = [&](int side) { return side < 0 ? 0.0 : boundsPx.x; };
    // A flat black quad over the whole height, from `from` to `to` units outward from a side's edge.
    const auto black = [&](Kind kind, int side, double from, double to, double alpha) {
        Piece piece;
        piece.kind = kind;
        piece.side = side;
        piece.centrePx = glm::dvec2(edgeOf(side) + side * (from + to) * 0.5, (top + bottom) * 0.5);
        piece.sizePx = glm::dvec2(to - from, bottom - top);
        piece.alpha = alpha;
        pieces.push_back(piece);
    };
    for (const int side : kSides) black(Kind::Backing, side, 0.0, rules.backingReachUnits, 1.0);
    // Stood on end, the picture's thickness lies across the screen and its length down it.
    const double length = stonePx.x;
    const double thickness = stonePx.y;
    for (const int side : kSides) {
        for (int column = 0; column < rules.columns; ++column) {
            const double x = edgeOf(side) + side * (thickness * 0.5 + rules.columnStepUnits * column);
            double y = rules.rowStartUnits[static_cast<std::size_t>(column) % rules.rowStartUnits.size()];
            while (y - length * 0.5 > 0.0) y -= rules.rowStepUnits;
            for (; y - length * 0.5 < boundsPx.y; y += rules.rowStepUnits) {
                Piece piece;
                piece.kind = Kind::Stone;
                piece.side = side;
                piece.centrePx = glm::dvec2(x, y);
                piece.sizePx = stonePx;
                piece.turnRad = rules.turnRad;
                pieces.push_back(piece);
            }
        }
    }
    // The fade: slices side by side, none over another, each of one alpha - the smoothstep at its middle - and a last one of whole black.
    const double step = (rules.fadeToUnits - rules.fadeFromUnits) / std::max(rules.fadeSteps, 1);
    for (const int side : kSides) {
        for (int i = 0; i < rules.fadeSteps; ++i) {
            const double u = (i + 0.5) / rules.fadeSteps;
            black(Kind::Fade, side, rules.fadeFromUnits + step * i, rules.fadeFromUnits + step * (i + 1), u * u * (3.0 - 2.0 * u));
        }
        black(Kind::Fade, side, rules.fadeToUnits, rules.fadeToUnits + rules.fadeCapUnits, 1.0);
    }
    for (std::size_t i = 0; i < pieces.size(); ++i) pieces[i].z = rules.zFirst + rules.zStep * static_cast<double>(i);
    return pieces;
}

bool Shown(double viewWidthPx, double boundsWidthPx) {
    return viewWidthPx > boundsWidthPx;
}

} // namespace MagicPortals::SideWalls
