#include "sim/Controls.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
#include <vector>

namespace MagicPortals::Controls {

namespace {

// The Hud function's own name for a walk pad's side. Only ever asked of a walk pad.
Hud::Side SideOf(Id walk) {
    return walk == Id::Left ? Hud::Side::Left : Hud::Side::Right;
}

} // namespace

HitShape HitShape::Closed(const Hud::Rect& rect) {
    HitShape shape;
    shape.kind = Kind::Rect;
    shape.rect = rect;
    shape.halfOpen = false;
    return shape;
}

HitShape HitShape::HalfOpen(const Hud::Rect& rect) {
    HitShape shape = Closed(rect);
    shape.halfOpen = true;
    return shape;
}

HitShape HitShape::Disc(const glm::dvec2& centre, double radius) {
    HitShape shape;
    shape.kind = Kind::Disc;
    shape.centre = centre;
    shape.radius = radius;
    return shape;
}

bool HitShape::Contains(const glm::dvec2& point) const {
    if (kind == Kind::Disc) {
        // Strictly inside, written as Hud::OnPad writes it: the squared distance against the
        // squared radius, so a point exactly on the circle is off the pad and the decision is
        // the same one the original's isPointInSphere makes.
        const glm::dvec2 d = point - centre;
        return glm::dot(d, d) < radius * radius;
    }
    if (!halfOpen) return rect.Contains(point);
    const glm::dvec2 max = rect.Max();
    return point.x >= rect.min.x && point.y >= rect.min.y && point.x < max.x && point.y < max.y;
}

Frame ClassicWalkFrame(const Hud::Rules& rules, Id walk, const glm::dvec2& viewUnits, double ageMs) {
    Frame frame;
    if (!IsWalk(walk)) return frame;
    const Hud::Side side = SideOf(walk);
    frame.rect = Hud::PadRect(rules, side, viewUnits, ageMs);
    frame.hit = HitShape::Disc(Hud::PadCorner(rules, side, viewUnits, ageMs), rules.pads.hitRadiusUnits);
    return frame;
}

Frame ClassicCornerFrame(const Hud::Rect& drawn) {
    Frame frame;
    frame.rect = drawn;
    frame.hit = HitShape::Closed(drawn);
    return frame;
}

double ClassicSlideUnits(const Hud::Rules& rules, double ageMs) {
    return Hud::PadSlideUnits(rules, ageMs);
}

glm::dvec2 ClassicRingCentre(const Hud::Rules& rules, Id walk, const glm::dvec2& viewUnits, double ageMs) {
    if (!IsWalk(walk)) return glm::dvec2(0.0);
    return Hud::PadCorner(rules, SideOf(walk), viewUnits, ageMs);
}

bool ClassicOnWalkControl(const Hud::Rules& rules, const glm::dvec2& viewUnits, double ageMs, const glm::dvec2& point) {
    return Hud::OnPad(rules, Hud::Side::Left, viewUnits, ageMs, point) ||
           Hud::OnPad(rules, Hud::Side::Right, viewUnits, ageMs, point);
}

double ClassicWalkOpacity(const Hud::Rules& rules, double ageMs, bool tutorial) {
    return Hud::PadOpacity(rules, ageMs, tutorial);
}

int ClassicWalkAlphaByte(const Hud::Rules& rules, double ageMs, bool tutorial) {
    return Hud::PadAlphaByte(rules, ageMs, tutorial);
}

const Hud::Placement& ClassicPlacement(const Hud::Rules& rules, Id corner) {
    static const Hud::Placement none{};
    switch (corner) {
    case Id::Reset:
        return rules.restart;
    case Id::Menu:
        return rules.pause;
    case Id::Clear:
        return rules.clearPortals;
    case Id::Left:
    case Id::Right:
        break;
    }
    return none;
}

Hud::Rect ClassicPlace(const Hud::Rules& rules, Id corner, const glm::dvec2& viewUnits) {
    return Hud::Place(ClassicPlacement(rules, corner), viewUnits);
}


// ---- the modern controls ----------------------------------------------------------------------

namespace {

namespace Json = Supersonic::Json;

constexpr std::size_t kIds = 5;

std::size_t Index(Id id) {
    return static_cast<std::size_t>(id);
}

std::size_t Index(Preset preset) {
    return static_cast<std::size_t>(preset);
}

// A number that is not a NaN or an infinity, so that no comparison below is asked of one.
bool Finite(double v) {
    return std::isfinite(v);
}

// ---- reading controls.json ---------------------------------------------------------------------

std::string Named(const std::string& where, const char* key) {
    return where.empty() ? std::string(key) : where + "." + key;
}

// A number that must be there and finite, inside [lo, hi].
bool Num(const Json::Value& block, const char* key, double lo, double hi, double& out, std::string& why,
         const std::string& where) {
    const Json::Value& value = block[key];
    if (!value.IsNumber() || !Finite(value.AsNumber()) || value.AsNumber() < lo || value.AsNumber() > hi) {
        why = Named(where, key) + " is missing, not a number, or outside " + std::to_string(lo) + " to " + std::to_string(hi);
        return false;
    }
    out = value.AsNumber();
    return true;
}

bool Whole(const Json::Value& block, const char* key, int lo, int hi, int& out, std::string& why, const std::string& where) {
    double n = 0.0;
    if (!Num(block, key, static_cast<double>(lo), static_cast<double>(hi), n, why, where)) return false;
    if (n != std::floor(n)) {
        why = Named(where, key) + " is not a whole number";
        return false;
    }
    out = static_cast<int>(n);
    return true;
}

bool Text(const Json::Value& block, const char* key, std::string& out, std::string& why, const std::string& where) {
    const Json::Value& value = block[key];
    if (!value.IsString() || value.AsString().empty()) {
        why = Named(where, key) + " is missing or not a non-empty string";
        return false;
    }
    out = value.AsString();
    return true;
}

bool Object(const Json::Value& block, const char* key, std::string& why, const std::string& where) {
    if (!block[key].IsObject()) {
        why = Named(where, key) + " is missing or not an object";
        return false;
    }
    return true;
}

// The whole number of steps `value` is on a grid of `divisor` steps to the unit, or false when it is
// not on it (a thousandth of a step off is not).
bool OnGrid(double value, int divisor, int& steps) {
    const double scaled = value * static_cast<double>(divisor);
    const double whole = std::round(scaled);
    if (std::fabs(scaled - whole) > 1e-9) return false;
    steps = static_cast<int>(whole);
    return true;
}

// A grid: min, max and step, the step a whole part of one so the values are n / divisor by one division.
bool ReadGrid(const Json::Value& block, Grid& out, std::string& why, const std::string& where) {
    Grid grid;
    if (!Num(block, "min", 1e-9, 1e6, grid.min, why, where) || !Num(block, "max", 1e-9, 1e6, grid.max, why, where) ||
        !Num(block, "step", 1e-9, 1e6, grid.step, why, where)) {
        return false;
    }
    const double divisor = std::round(1.0 / grid.step);
    if (divisor < 1.0 || divisor > 1e6 || std::fabs(1.0 / grid.step - divisor) > 1e-9) {
        why = where + ".step is not one over a whole number, so its steps could not be counted";
        return false;
    }
    grid.divisor = static_cast<int>(divisor);
    if (!OnGrid(grid.min, grid.divisor, grid.lowSteps) || !OnGrid(grid.max, grid.divisor, grid.highSteps) ||
        grid.lowSteps >= grid.highSteps) {
        why = where + ".min and .max are not whole steps apart, or max is not above min";
        return false;
    }
    out = grid;
    return true;
}

// `value` as a step of `grid`, inside it.
bool StepOf(const Grid& grid, double value, int& steps) {
    return OnGrid(value, grid.divisor, steps) && steps >= grid.lowSteps && steps <= grid.highSteps;
}

bool ReadArt(const Json::Value& art, Rules& read, std::string& why) {
    if (!art.IsObject()) {
        why = "art is missing or not an object";
        return false;
    }
    const std::array<ArtTier, kArtTierCount> tiers = {ArtTier::MagicRampage, ArtTier::Original, ArtTier::Generated};
    for (const ArtTier tier : tiers) {
        const std::string where = std::string("art.") + NameOf(tier);
        if (!Object(art, NameOf(tier), why, "art")) return false;
        const Json::Value& block = art[NameOf(tier)];
        for (const Id id : kClaimOrder) {
            // The original has a sprite for the three corner buttons and none for a walk pad (design 4.2).
            if (tier == ArtTier::Original && IsWalk(id)) continue;
            if (!Object(block, KeyOf(id), why, where)) return false;
            const Json::Value& entry = block[KeyOf(id)];
            ArtEntry read_entry;
            if (!Text(entry, "file", read_entry.file, why, where + "." + KeyOf(id)) ||
                !Num(entry, "body_inset", 0.0, 0.499, read_entry.bodyInset, why, where + "." + KeyOf(id))) {
                return false;
            }
            read.art[static_cast<std::size_t>(tier)][Index(id)] = read_entry;
        }
    }
    return true;
}

// ---- the places ----------------------------------------------------------------------------------

// A length in the reference's px, in units of this view.
double UnitsOf(const Rules& rules, const glm::dvec2& viewUnits, double px) {
    return px * viewUnits.y / rules.referenceHeightPx;
}

// A move is whole tenths of a unit: one division, so 0.3 is the double 0.3 and not the product 3 x 0.1.
double SnapMoveAxis(double v) {
    if (!Finite(v)) return 0.0;
    const double divisor = std::round(1.0 / Tuning::kMoveSnap);
    const double bound = Tuning::kMoveLimit;
    // + 0.0 turns a negative zero into a zero, so none is ever written to a file.
    return std::round(std::clamp(v, -bound, bound) * divisor) / divisor + 0.0;
}

glm::dvec2 SnapMove(const glm::dvec2& v) {
    return glm::dvec2(SnapMoveAxis(v.x), SnapMoveAxis(v.y));
}

struct Safe {
    glm::dvec2 min;
    glm::dvec2 max;
};

Safe SafeRect(const glm::dvec2& viewUnits, const Insets& insets) {
    return Safe{glm::dvec2(insets.left, insets.top), glm::dvec2(viewUnits.x - insets.right, viewUnits.y - insets.bottom)};
}

glm::dvec2 BoxSize(const Rules& rules, const Tuning& tuning, const glm::dvec2& viewUnits, Id id) {
    const double size = tuning.Size(rules);
    const double box = UnitsOf(rules, viewUnits, rules.boxPx);
    if (IsWalk(id)) return glm::dvec2(box * size);
    return glm::dvec2(box * rules.cornerBoxFactor * std::max(size, rules.cornerMinSize));
}

// Where a control's box is with nothing moved: the margin in from the safe rectangle's edges, the gap
// between neighbours, the pair left then right at the bottom-left, restart then pause at the top-right
// (pause on the corner), clear-portals at the top-left. Written in the order the design's arithmetic
// (CTL/arith2/final_layout.py) writes it.
glm::dvec2 DefaultMin(const Rules& rules, const Tuning& tuning, Id id, const glm::dvec2& viewUnits, const Insets& insets) {
    const Safe safe = SafeRect(viewUnits, insets);
    const double side = UnitsOf(rules, viewUnits, rules.marginSidePx);
    const double top = UnitsOf(rules, viewUnits, rules.marginTopPx);
    const double bottom = UnitsOf(rules, viewUnits, rules.presets[Index(tuning.PresetIn(rules))].marginBottomPx);
    const double gap = UnitsOf(rules, viewUnits, rules.gapPx);
    const double arrow = BoxSize(rules, tuning, viewUnits, Id::Left).x;
    const double corner = BoxSize(rules, tuning, viewUnits, Id::Reset).x;
    switch (id) {
    case Id::Left:
        return glm::dvec2(safe.min.x + side, safe.max.y - bottom - arrow);
    case Id::Right:
        return glm::dvec2(safe.min.x + side + arrow + gap, safe.max.y - bottom - arrow);
    case Id::Menu:
        return glm::dvec2(safe.max.x - side - corner, safe.min.y + top);
    case Id::Reset:
        return glm::dvec2(safe.max.x - side - corner - gap - corner, safe.min.y + top);
    case Id::Clear:
        return glm::dvec2(safe.min.x + side, safe.min.y + top);
    }
    return glm::dvec2(0.0);
}

// The five boxes as the arithmetic puts them: default place, the tuning's move, the shared clamp.
std::array<Hud::Rect, kIds> RawBoxes(const Rules& rules, const Tuning& given, const glm::dvec2& viewUnits, const Insets& insets) {
    const Tuning tuning = given.Clamped(rules);
    std::array<Hud::Rect, kIds> boxes;
    for (const Id id : kClaimOrder) {
        const glm::dvec2 size = BoxSize(rules, tuning, viewUnits, id);
        const glm::dvec2 moved = DefaultMin(rules, tuning, id, viewUnits, insets) + tuning.move[Index(id)];
        boxes[Index(id)] = Hud::Rect{ClampBox(moved, size, viewUnits, insets), size};
    }
    return boxes;
}

Hud::Placement PlacementOf(const std::string& sprite, Id corner, const Hud::Rect& box, const glm::dvec2& viewUnits) {
    Hud::Placement placement;
    placement.sprite = sprite;
    placement.sizeUnits = box.size;
    if (corner == Id::Clear) {
        placement.anchor = Hud::Anchor::TopLeft;
        placement.insetUnits = box.min;
    } else {
        placement.anchor = Hud::Anchor::TopRight;
        placement.insetUnits = glm::dvec2(viewUnits.x - box.Max().x, box.min.y);
    }
    return placement;
}

// A rectangle from its four edges, whose Max() is the right and bottom edge exactly: min + size can be an ulp
// off the edge it was made from, and two hit rectangles that share a seam must meet on the same double or a
// point on it would be both's or neither's.
Hud::Rect FromEdges(double l, double t, double r, double b) {
    const auto size = [](double from, double to) {
        double s = to - from;
        for (int i = 0; i < 4 && from + s < to; ++i) s = std::nextafter(s, std::numeric_limits<double>::infinity());
        for (int i = 0; i < 4 && from + s > to; ++i) s = std::nextafter(s, -std::numeric_limits<double>::infinity());
        return s;
    };
    return Hud::Rect{glm::dvec2(l, t), glm::dvec2(size(l, r), size(t, b))};
}

// ---- the flag ---------------------------------------------------------------------------------------

std::vector<std::string> Split(const std::string& text, char separator) {
    std::vector<std::string> parts;
    std::size_t from = 0;
    while (true) {
        const std::size_t at = text.find(separator, from);
        if (at == std::string::npos) {
            parts.push_back(text.substr(from));
            return parts;
        }
        parts.push_back(text.substr(from, at - from));
        from = at + 1;
    }
}

// The whole text as one finite number, as `1.2` or `-3` or `1e2`: nothing before it, nothing after.
bool ParseNumber(const std::string& text, double& out) {
    if (text.empty()) return false;
    double value = 0.0;
    const char* end = text.data() + text.size();
    const auto [stop, error] = std::from_chars(text.data(), end, value);
    if (error != std::errc{} || stop != end || !Finite(value)) return false;
    out = value;
    return true;
}

std::string NumberText(double v) {
    char buffer[64];
    const auto [end, error] = std::to_chars(buffer, buffer + sizeof(buffer), v + 0.0);
    if (error != std::errc{}) return "0";
    return std::string(buffer, end);
}

} // namespace

const char* KeyOf(Id id) {
    switch (id) {
    case Id::Left:
        return "walk_left";
    case Id::Right:
        return "walk_right";
    case Id::Reset:
        return "restart";
    case Id::Menu:
        return "pause";
    case Id::Clear:
        return "clear_portals";
    }
    return "";
}

std::optional<Id> IdOfKey(const std::string& key) {
    for (const Id id : kClaimOrder) {
        if (key == KeyOf(id)) return id;
    }
    return std::nullopt;
}

const char* NameOf(Style style) {
    return style == Style::Modern ? "modern" : "classic";
}

std::optional<Style> StyleOfName(const std::string& name) {
    for (const Style style : {Style::Classic, Style::Modern}) {
        if (name == NameOf(style)) return style;
    }
    return std::nullopt;
}

const char* NameOf(Preset preset) {
    switch (preset) {
    case Preset::Uniform:
        return "uniform";
    case Preset::Balanced:
        return "balanced";
    case Preset::Mr:
        return "mr";
    }
    return "";
}

std::optional<Preset> PresetOfName(const std::string& name) {
    for (const Preset preset : kPresets) {
        if (name == NameOf(preset)) return preset;
    }
    return std::nullopt;
}

const char* NameOf(ActOn actOn) {
    return actOn == ActOn::Press ? "press" : "release";
}

const char* NameOf(ArtTier tier) {
    switch (tier) {
    case ArtTier::MagicRampage:
        return "mr";
    case ArtTier::Original:
        return "original";
    case ArtTier::Generated:
        return "generated";
    }
    return "";
}

// ---- controls.json --------------------------------------------------------------------------------

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
    if (!root.IsObject()) {
        error = path + ": the document is not an object";
        return false;
    }

    Rules read;
    std::string why;
    const auto fail = [&error, &path, &why]() {
        error = path + ": " + why;
        return false;
    };
    constexpr double kBig = 1e6;

    if (!Num(root, "clearance_px", 0.0, kBig, read.clearancePx, why, "")) return fail();
    if (!Object(root, "reference", why, "") ||
        !Num(root["reference"], "height_px", 1.0, kBig, read.referenceHeightPx, why, "reference")) {
        return fail();
    }

    // The two grids first: the presets' sizes are steps of the first.
    if (!Object(root, "size", why, "") || !ReadGrid(root["size"], read.size, why, "size")) return fail();
    const Json::Value& opacity = root["opacity"];
    if (!Object(root, "opacity", why, "") || !ReadGrid(opacity, read.opacity, why, "opacity")) return fail();
    double opacityDefault = 0.0;
    if (!Num(opacity, "default", 1e-9, kBig, opacityDefault, why, "opacity") ||
        !StepOf(read.opacity, opacityDefault, read.opacityDefaultSteps) ||
        !Whole(opacity, "base_alpha_byte", 0, 255, read.baseAlphaByte, why, "opacity") ||
        !Num(opacity, "idle_scale", 1e-9, kBig, read.idleScale, why, "opacity")) {
        if (why.empty()) why = "opacity.default is not a step of the opacity grid";
        return fail();
    }

    if (!Object(root, "layout", why, "")) return fail();
    const Json::Value& layout = root["layout"];
    if (!Whole(layout, "version", 0, 1000000, read.layoutVersion, why, "layout")) return fail();
    if (read.layoutVersion != kLayoutVersion) {
        why = "layout.version is " + std::to_string(read.layoutVersion) + ", the code's kLayoutVersion is " +
              std::to_string(kLayoutVersion) + ": whoever changes a default place bumps both";
        return fail();
    }
    if (!Num(layout, "gap_px", 0.0, kBig, read.gapPx, why, "layout") || !Num(layout, "box_px", 1e-9, kBig, read.boxPx, why, "layout") ||
        !Num(layout, "face_inset", 0.0, 0.499, read.faceInset, why, "layout") ||
        !Num(layout, "corner_box_factor", 1e-9, kBig, read.cornerBoxFactor, why, "layout") ||
        !Num(layout, "corner_min_size", 1e-9, kBig, read.cornerMinSize, why, "layout") ||
        !Object(layout, "margin_px", why, "layout") ||
        !Num(layout["margin_px"], "side", 0.0, kBig, read.marginSidePx, why, "layout.margin_px") ||
        !Num(layout["margin_px"], "top", 0.0, kBig, read.marginTopPx, why, "layout.margin_px") ||
        !Num(layout["margin_px"], "bottom", 0.0, kBig, read.marginBottomPx, why, "layout.margin_px") ||
        !Object(layout, "presets", why, "layout")) {
        return fail();
    }
    const Json::Value& presets = layout["presets"];
    for (const Preset preset : kPresets) {
        const std::string where = std::string("layout.presets.") + NameOf(preset);
        if (!Object(presets, NameOf(preset), why, "layout.presets")) return fail();
        const Json::Value& block = presets[NameOf(preset)];
        double size = 0.0;
        Rules::PresetRule rule;
        if (!Num(block, "size", 1e-9, kBig, size, why, where)) return fail();
        if (!StepOf(read.size, size, rule.sizeSteps)) {
            why = where + ".size is not a step of the size grid, or is outside it";
            return fail();
        }
        // The one optional number: a preset that names no bottom margin keeps layout.margin_px.bottom.
        rule.marginBottomPx = read.marginBottomPx;
        if (block.Has("margin_bottom_px") && !Num(block, "margin_bottom_px", 0.0, kBig, rule.marginBottomPx, why, where)) {
            return fail();
        }
        read.presets[Index(preset)] = rule;
    }
    std::string defaultPreset;
    if (!Text(layout, "default_preset", defaultPreset, why, "layout")) return fail();
    if (const auto preset = PresetOfName(defaultPreset)) {
        read.defaultPreset = *preset;
    } else {
        why = "layout.default_preset is not uniform, balanced or mr";
        return fail();
    }

    if (!Object(root, "hit", why, "")) return fail();
    const Json::Value& hit = root["hit"];
    if (!Num(hit, "seam_px", 0.0, kBig, read.hit.seamPx, why, "hit") ||
        !Num(hit, "edge_reach_px", 0.0, kBig, read.hit.edgeReachPx, why, "hit") || !Object(hit, "grow_px", why, "hit")) {
        return fail();
    }
    for (const Id id : kClaimOrder) {
        if (!Num(hit["grow_px"], KeyOf(id), 0.0, kBig, read.hit.growPx[Index(id)], why, "hit.grow_px")) return fail();
    }

    std::string actOn;
    if (!Object(root, "corner", why, "") || !Text(root["corner"], "act_on", actOn, why, "corner")) return fail();
    if (actOn == NameOf(ActOn::Release)) {
        read.cornerActOn = ActOn::Release;
    } else if (actOn == NameOf(ActOn::Press)) {
        read.cornerActOn = ActOn::Press;
    } else {
        why = "corner.act_on is not release or press";
        return fail();
    }

    if (!Object(root, "pressed", why, "") || !Whole(root["pressed"], "boost_byte", 0, 255, read.pressedBoostByte, why, "pressed") ||
        !Object(root, "ring", why, "") || !Num(root["ring"], "size_over_box", 1e-9, kBig, read.ringSizeOverBox, why, "ring") ||
        !Object(root, "hint", why, "") || !Num(root["hint"], "margin_units", 0.0, kBig, read.hintMarginUnits, why, "hint")) {
        return fail();
    }

    if (!ReadArt(root["art"], read, why)) return fail();

    if (!Object(root, "census", why, "") || !Num(root["census"], "em_units", 0.0, kBig, read.census.emUnits, why, "census") ||
        !Num(root["census"], "slack_units", 0.0, kBig, read.census.slackUnits, why, "census") ||
        !Num(root["census"], "character_row_units", 0.0, kBig, read.census.characterRowUnits, why, "census")) {
        return fail();
    }

    std::string defaultMode;
    if (!Text(root, "default_mode", defaultMode, why, "")) return fail();
    if (const auto style = StyleOfName(defaultMode)) {
        read.defaultMode = *style;
    } else {
        why = "default_mode is not classic or modern";
        return fail();
    }

    out = std::move(read);
    return true;
}

// ---- the tuning ------------------------------------------------------------------------------------------

bool FlagItems::Any() const {
    if (preset || size || opacity) return true;
    for (const auto& item : move) {
        if (item) return true;
    }
    return false;
}

Preset Tuning::PresetIn(const Rules& rules) const {
    return preset.value_or(rules.defaultPreset);
}

int Tuning::SizeStepsIn(const Rules& rules) const {
    const int own = rules.presets[Index(PresetIn(rules))].sizeSteps;
    return std::clamp(sizeSteps.value_or(own), rules.size.lowSteps, rules.size.highSteps);
}

int Tuning::OpacityStepsIn(const Rules& rules) const {
    return std::clamp(opacitySteps.value_or(rules.opacityDefaultSteps), rules.opacity.lowSteps, rules.opacity.highSteps);
}

double Tuning::Size(const Rules& rules) const {
    return rules.size.ValueOf(SizeStepsIn(rules));
}

double Tuning::Opacity(const Rules& rules) const {
    return rules.opacity.ValueOf(OpacityStepsIn(rules));
}

bool Tuning::IsDefault(const Rules& rules) const {
    if (SizeStepsIn(rules) != rules.presets[Index(PresetIn(rules))].sizeSteps) return false;
    if (OpacityStepsIn(rules) != rules.opacityDefaultSteps) return false;
    for (const glm::dvec2& m : move) {
        const glm::dvec2 snapped = SnapMove(m);
        if (snapped.x != 0.0 || snapped.y != 0.0) return false;
    }
    return true;
}

Tuning Tuning::Clamped(const Rules& rules) const {
    Tuning out = *this;
    // An unset where the value in force is the default's own: a player who never moved the slider, or moved
    // it back, is on the default, and a change of the default then reaches them.
    const int size = SizeStepsIn(rules);
    out.sizeSteps = size == rules.presets[Index(PresetIn(rules))].sizeSteps ? std::nullopt : std::optional<int>(size);
    const int level = OpacityStepsIn(rules);
    out.opacitySteps = level == rules.opacityDefaultSteps ? std::nullopt : std::optional<int>(level);
    for (glm::dvec2& m : out.move) m = SnapMove(m);
    return out;
}

Tuning Tuning::SizeStepped(const Rules& rules, int direction) const {
    Tuning out = *this;
    const int now = SizeStepsIn(rules);
    const int next = direction > 0 ? now + 1 : direction < 0 ? now - 1 : now;
    out.sizeSteps = std::clamp(next, rules.size.lowSteps, rules.size.highSteps);
    return out.Clamped(rules);
}

Tuning Tuning::OpacityStepped(const Rules& rules, int direction) const {
    Tuning out = *this;
    const int now = OpacityStepsIn(rules);
    const int next = direction > 0 ? now + 1 : direction < 0 ? now - 1 : now;
    out.opacitySteps = std::clamp(next, rules.opacity.lowSteps, rules.opacity.highSteps);
    return out.Clamped(rules);
}

Tuning Tuning::ReadFrom(const Json::Value& controls, const Rules& rules) {
    Tuning out;
    if (!controls.IsObject()) return out;

    // A number off its grid is snapped to the nearest step (a half goes away from zero) and held inside
    // the grid; one that is not a number, or not finite, is the default.
    const auto steps = [](const Json::Value& value, const Grid& grid) -> std::optional<int> {
        if (!value.IsNumber() || !Finite(value.AsNumber())) return std::nullopt;
        const double scaled = std::round(value.AsNumber() * static_cast<double>(grid.divisor));
        return static_cast<int>(std::clamp(scaled, static_cast<double>(grid.lowSteps), static_cast<double>(grid.highSteps)));
    };
    out.sizeSteps = steps(controls["size"], rules.size);
    out.opacitySteps = steps(controls["opacity"], rules.opacity);

    // A save made against another arrangement keeps what is not a place and drops the places.
    const Json::Value& layout = controls["layout"];
    const bool sameLayout = !controls.Has("layout") || (layout.IsNumber() && layout.AsNumber() == static_cast<double>(kLayoutVersion));
    const Json::Value& moves = controls["move"];
    if (sameLayout && moves.IsObject()) {
        for (const Id id : kClaimOrder) {
            const Json::Value& pair = moves[KeyOf(id)];
            if (!pair.IsArray() || pair.AsArray().size() != 2 || !pair.AsArray()[0].IsNumber() || !pair.AsArray()[1].IsNumber()) continue;
            const glm::dvec2 raw(pair.AsArray()[0].AsNumber(), pair.AsArray()[1].AsNumber());
            if (Finite(raw.x) && Finite(raw.y)) out.move[Index(id)] = SnapMove(raw);
        }
    }
    return out.Clamped(rules);
}

std::string Tuning::ToJson(const Rules& rules) const {
    const Tuning clamped = Clamped(rules);
    std::string text;
    const auto add = [&text](const std::string& member) {
        text += text.empty() ? "" : ",";
        text += member;
    };
    if (clamped.sizeSteps) add("\"size\":" + NumberText(rules.size.ValueOf(*clamped.sizeSteps)));
    if (clamped.opacitySteps) add("\"opacity\":" + NumberText(rules.opacity.ValueOf(*clamped.opacitySteps)));
    std::string moves;
    for (const Id id : kClaimOrder) {
        const glm::dvec2& m = clamped.move[Index(id)];
        if (m.x == 0.0 && m.y == 0.0) continue;
        moves += moves.empty() ? "" : ",";
        moves += std::string("\"") + KeyOf(id) + "\":[" + NumberText(m.x) + "," + NumberText(m.y) + "]";
    }
    if (!moves.empty()) add("\"move\":{" + moves + "}");
    return "{" + text + "}";
}

const char* FlagUsage() {
    static const std::string usage = [] {
        std::string text = "--controls <classic|modern>[,key=value...]  keys: preset=";
        for (const Preset preset : kPresets) text += std::string(preset == kPresets.front() ? "" : "|") + NameOf(preset);
        text += ", size=<n>, opacity=<n>, and a move as <key>=<dx>:<dy> for ";
        for (const Id id : kClaimOrder) text += std::string(id == kClaimOrder.front() ? "" : ", ") + KeyOf(id);
        return text;
    }();
    return usage.c_str();
}

bool Tuning::ParseFlag(const std::string& items, FlagItems& out, std::string& error) {
    FlagItems read;
    if (items.empty()) {
        out = read;
        return true;
    }
    const auto refuse = [&error](const std::string& why) {
        error = why + "; " + FlagUsage();
        return false;
    };
    for (const std::string& item : Split(items, ',')) {
        const std::size_t equals = item.find('=');
        if (equals == std::string::npos || equals == 0 || equals + 1 == item.size()) {
            return refuse("'" + item + "' is not key=value");
        }
        const std::string key = item.substr(0, equals);
        const std::string value = item.substr(equals + 1);
        if (key == "preset") {
            if (read.preset) return refuse("preset is given twice");
            const auto preset = PresetOfName(value);
            if (!preset) return refuse("'" + value + "' is not a preset");
            read.preset = preset;
        } else if (key == "size" || key == "opacity") {
            std::optional<double>& slot = key == "size" ? read.size : read.opacity;
            if (slot) return refuse(key + " is given twice");
            double number = 0.0;
            if (!ParseNumber(value, number)) return refuse("'" + value + "' is not a number for " + key);
            slot = number;
        } else if (const auto id = IdOfKey(key)) {
            if (read.move[Index(*id)]) return refuse(key + " is given twice");
            const std::vector<std::string> parts = Split(value, ':');
            double dx = 0.0;
            double dy = 0.0;
            if (parts.size() != 2 || !ParseNumber(parts[0], dx) || !ParseNumber(parts[1], dy)) {
                return refuse("'" + value + "' is not dx:dy for " + key);
            }
            if (std::fabs(dx) > kMoveLimit || std::fabs(dy) > kMoveLimit) {
                return refuse("a move of " + key + " is more than " + NumberText(kMoveLimit) + " units");
            }
            read.move[Index(*id)] = glm::dvec2(dx, dy);
        } else {
            return refuse("'" + key + "' is not a key");
        }
    }
    out = read;
    return true;
}

bool Tuning::FromFlag(const FlagItems& items, const Rules& rules, Tuning& out, std::string& error) {
    Tuning tuning;
    tuning.preset = items.preset;
    if (items.size) {
        int steps = 0;
        if (!StepOf(rules.size, *items.size, steps)) {
            error = "size " + NumberText(*items.size) + " is not a step of " + NumberText(rules.size.step) + " from " +
                    NumberText(rules.size.min) + " to " + NumberText(rules.size.max);
            return false;
        }
        tuning.sizeSteps = steps;
    }
    if (items.opacity) {
        int steps = 0;
        if (!StepOf(rules.opacity, *items.opacity, steps)) {
            error = "opacity " + NumberText(*items.opacity) + " is not a step of " + NumberText(rules.opacity.step) + " from " +
                    NumberText(rules.opacity.min) + " to " + NumberText(rules.opacity.max);
            return false;
        }
        tuning.opacitySteps = steps;
    }
    for (const Id id : kClaimOrder) {
        if (items.move[Index(id)]) tuning.move[Index(id)] = *items.move[Index(id)];
    }
    out = tuning.Clamped(rules);
    return true;
}

// ---- the layout ----------------------------------------------------------------------------------------

glm::dvec2 ClampBox(const glm::dvec2& min, const glm::dvec2& size, const glm::dvec2& viewUnits, const Insets& insets) {
    const Safe safe = SafeRect(viewUnits, insets);
    // A box larger than the safe rectangle sits against its top-left corner rather than breaking the clamp.
    const double highX = std::max(safe.min.x, safe.max.x - size.x);
    const double highY = std::max(safe.min.y, safe.max.y - size.y);
    return glm::dvec2(std::clamp(min.x, safe.min.x, highX), std::clamp(min.y, safe.min.y, highY));
}

std::array<Hud::Rect, 5> Boxes(const Rules& rules, const Tuning& tuning, const glm::dvec2& viewUnits, const Insets& insets) {
    std::array<Hud::Rect, kIds> boxes = RawBoxes(rules, tuning, viewUnits, insets);
    // A corner button at rest is where the Hud::Placement puts it, so the end of its entrance (LevelEnd::HudEntered,
    // which places the same Placement) and its place here are one rectangle, bit for bit.
    for (const Id id : kClaimOrder) {
        if (IsWalk(id)) continue;
        boxes[Index(id)] = Hud::Place(PlacementOf(std::string(), id, boxes[Index(id)], viewUnits), viewUnits);
    }
    return boxes;
}

Hud::Placement PlacementFor(const Rules& rules, const Hud::Rules& hud, const Tuning& tuning, Id corner,
                            const glm::dvec2& viewUnits, const Insets& insets) {
    if (IsWalk(corner)) return Hud::Placement{};
    return PlacementOf(ClassicPlacement(hud, corner).sprite, corner, RawBoxes(rules, tuning, viewUnits, insets)[Index(corner)], viewUnits);
}

std::array<Hud::Rect, 5> HitRects(const Rules& rules, const std::array<Hud::Rect, 5>& boxes, const glm::dvec2& viewUnits,
                                  const Insets& insets) {
    // The arrows' gap IS the seam by construction (32 px each), so a gap that is exactly it must count: a
    // billionth of a unit of slack stands for the rounding of two sums, as the design's arithmetic gives it.
    constexpr double kSlack = 1e-9;
    const double seam = UnitsOf(rules, viewUnits, rules.hit.seamPx) + kSlack;
    const double reach = UnitsOf(rules, viewUnits, rules.hit.edgeReachPx) + kSlack;
    const Safe safe = SafeRect(viewUnits, insets);
    std::array<Hud::Rect, kIds> hits;

    for (const Id id : kClaimOrder) {
        const Hud::Rect& box = boxes[Index(id)];
        const double grow = UnitsOf(rules, viewUnits, rules.hit.growPx[Index(id)]);
        const glm::dvec2 max = box.Max();
        double l = box.min.x - grow;
        double t = box.min.y - grow;
        double r = max.x + grow;
        double b = max.y + grow;

        // A side that faces another box across a gap of at most the seam goes to the middle of it. The nearest
        // neighbour on a side decides, so the answer does not depend on which was looked at first; the middle is
        // (a + b) / 2 whichever of the two boxes asks, so both reach the same double.
        double nearestLeft = seam;
        double nearestRight = seam;
        double nearestTop = seam;
        double nearestBottom = seam;
        for (const Id other : kClaimOrder) {
            if (other == id) continue;
            const Hud::Rect& o = boxes[Index(other)];
            const glm::dvec2 omax = o.Max();
            if (std::min(max.y, omax.y) > std::max(box.min.y, o.min.y)) { // they share a band of rows
                if (omax.x <= box.min.x && box.min.x - omax.x <= nearestLeft) {
                    nearestLeft = box.min.x - omax.x;
                    l = (omax.x + box.min.x) * 0.5;
                }
                if (o.min.x >= max.x && o.min.x - max.x <= nearestRight) {
                    nearestRight = o.min.x - max.x;
                    r = (max.x + o.min.x) * 0.5;
                }
            }
            if (std::min(max.x, omax.x) > std::max(box.min.x, o.min.x)) { // and of columns
                if (omax.y <= box.min.y && box.min.y - omax.y <= nearestTop) {
                    nearestTop = box.min.y - omax.y;
                    t = (omax.y + box.min.y) * 0.5;
                }
                if (o.min.y >= max.y && o.min.y - max.y <= nearestBottom) {
                    nearestBottom = o.min.y - max.y;
                    b = (max.y + o.min.y) * 0.5;
                }
            }
        }

        // A side within the reach of the safe rectangle's edge goes to the VIEW's edge: the strip beside a box
        // is the control's, and a missed tap in a corner is not a portal.
        if (box.min.x - safe.min.x <= reach) l = 0.0;
        if (box.min.y - safe.min.y <= reach) t = 0.0;
        if (safe.max.x - max.x <= reach) r = viewUnits.x;
        if (safe.max.y - max.y <= reach) b = viewUnits.y;

        // And never off the view, whatever the growth.
        l = std::max(l, 0.0);
        t = std::max(t, 0.0);
        r = std::min(r, viewUnits.x);
        b = std::min(b, viewUnits.y);
        hits[Index(id)] = FromEdges(l, t, std::max(r, l), std::max(b, t));
    }
    return hits;
}

Frame FollowDrawn(const Frame& settled, const Hud::Rect& drawn) {
    const glm::dvec2 delta = drawn.min - settled.rect.min;
    Frame frame = settled;
    frame.rect = drawn;
    if (frame.hit.kind == HitShape::Kind::Disc) {
        frame.hit.centre += delta;
    } else {
        frame.hit.rect.min += delta;
    }
    return frame;
}

Layout LayOut(Style style, const Rules& rules, const Hud::Rules& hud, const Tuning& tuning, const glm::dvec2& viewUnits,
              const Insets& insets, double ageMs) {
    Layout layout;
    layout.style = style;
    if (style == Style::Classic) {
        for (const Id id : kClaimOrder) {
            layout.frames[Index(id)] = IsWalk(id) ? ClassicWalkFrame(hud, id, viewUnits, ageMs)
                                                  : ClassicCornerFrame(ClassicPlace(hud, id, viewUnits));
        }
        return layout;
    }

    const std::array<Hud::Rect, kIds> boxes = Boxes(rules, tuning, viewUnits, insets);
    const std::array<Hud::Rect, kIds> hits = HitRects(rules, boxes, viewUnits, insets);
    const double slide = ClassicSlideUnits(hud, ageMs);
    for (const Id id : kClaimOrder) {
        Frame settled;
        settled.rect = boxes[Index(id)];
        settled.hit = HitShape::HalfOpen(hits[Index(id)]);
        if (!IsWalk(id)) {
            layout.frames[Index(id)] = settled;
            continue;
        }
        // Each arrow comes in from the nearer of the view's side edges, the classic pads' own distance and
        // time; the hit rectangle goes with the picture. Which edge is judged where the arrow rests.
        const bool fromRight = settled.rect.Centre().x > viewUnits.x * 0.5;
        Hud::Rect drawn = settled.rect;
        drawn.min.x += fromRight ? slide : -slide;
        layout.frames[Index(id)] = FollowDrawn(settled, drawn);
    }
    return layout;
}

Hud::Rect DrawRect(const Hud::Rect& box, double faceInset, double bodyInset) {
    const glm::dvec2 faceMin = box.min + box.size * faceInset;
    const glm::dvec2 faceSize = box.size * (1.0 - 2.0 * faceInset);
    // The picture's body is the middle (1 - 2 mu) of it; it is drawn as much larger than the face as
    // that takes, each side by mu / (1 - 2 mu) of the face.
    const double grow = bodyInset / (1.0 - 2.0 * bodyInset);
    Hud::Rect rect;
    rect.min = faceMin - faceSize * grow;
    rect.size = faceSize * (1.0 + 2.0 * grow);
    return rect;
}

glm::dvec2 MoveFor(const Rules& rules, const Tuning& tuning, Id id, const glm::dvec2& wantedBoxMin, const glm::dvec2& viewUnits,
                   const Insets& insets) {
    const Tuning valid = tuning.Clamped(rules);
    const glm::dvec2 size = BoxSize(rules, valid, viewUnits, id);
    const glm::dvec2 wanted = ClampBox(wantedBoxMin, size, viewUnits, insets);
    return SnapMove(wanted - DefaultMin(rules, valid, id, viewUnits, insets));
}

// ---- what a layout draws ---------------------------------------------------------------------------------

int AlphaByte(const Rules& rules, const Tuning& tuning, int classicByte) {
    const double scaled = static_cast<double>(classicByte) * rules.idleScale * tuning.Opacity(rules);
    if (!(scaled > 0.0)) return 0;
    if (scaled >= 255.0) return 255;
    return static_cast<int>(std::lround(scaled));
}

RingPlace RingFor(const Rules& rules, const Hud::Rules& hud, const Hud::Ring& ring, const Frame& pad) {
    RingPlace place;
    if (!ring.shown) return place;
    place.shown = true;
    place.centre = pad.rect.Centre();
    place.alpha = ring.alpha;
    // The classic ring is `ringSizeUnits` across at its fullest and grows to it; the modern one is
    // `size_over_box` boxes across at its fullest, by the same fraction of it.
    const double fraction = hud.pads.ringSizeUnits > 0.0 ? ring.sizeUnits / hud.pads.ringSizeUnits : 0.0;
    place.sizeUnits = rules.ringSizeOverBox * pad.rect.size.x * fraction;
    return place;
}

double HintLeft(Style style, const Rules& rules, const Layout& layout, const HintRow& row, const glm::dvec2& viewUnits,
                const std::array<bool, 5>& shown) {
    if (style == Style::Classic) return row.classicLeft;
    bool meets = false;
    double pairRight = row.classicLeft;
    for (const Id id : kClaimOrder) {
        if (!IsWalk(id) || !shown[Index(id)]) continue;
        const Hud::Rect& hit = layout.At(id).hit.rect;
        const glm::dvec2 max = hit.Max();
        if (hit.min.y < row.top + row.height && max.y > row.top) {
            meets = true;
            pairRight = std::max(pairRight, max.x);
        }
    }
    if (!meets) return row.classicLeft;
    return std::min(pairRight, viewUnits.x - row.extent - rules.hintMarginUnits);
}

// ---- which style a run plays --------------------------------------------------------------------------------

Policy ResolvePolicy(const std::optional<std::string>& controlsFlag, bool fixedStep, bool route, bool visitLevels) {
    // A flag that cannot be read leaves nothing of the feature standing, so the caller that ignores `valid` still plays classic.
    const auto refused = [](const std::string& why) {
        Policy policy;
        policy.valid = false;
        policy.error = why;
        return policy;
    };
    Policy policy;
    if (controlsFlag) {
        const std::size_t comma = controlsFlag->find(',');
        const std::string mode = controlsFlag->substr(0, comma);
        const std::optional<Style> style = StyleOfName(mode);
        if (!style) return refused("--controls '" + mode + "' is not classic or modern; " + FlagUsage());
        policy.forced = style;
        if (*style == Style::Classic) {
            // Classic is the feature not existing in the run: there is nothing for a key to tune.
            if (comma != std::string::npos) return refused(std::string("--controls classic takes no keys; ") + FlagUsage());
            return policy;
        }
        policy.available = true;
        if (comma != std::string::npos) {
            // A trailing comma is an empty item, and refused with the rest.
            const std::string items = controlsFlag->substr(comma + 1);
            if (items.empty()) return refused(std::string("--controls modern, ends in a comma; ") + FlagUsage());
            std::string error;
            if (!Tuning::ParseFlag(items, policy.flag, error)) return refused(error);
        }
        return policy;
    }
    // A developer's run (the rig's three flags) plays the classic controls and has no feature to speak of; any
    // other run is a player's, which may save.
    if (fixedStep || route || visitLevels) return policy;
    policy.available = true;
    policy.saveAllowed = true;
    return policy;
}

Style ResolveStyle(const Policy& policy, const std::optional<Style>& saved, Style shippedDefault) {
    if (!policy.available) return Style::Classic;
    if (policy.forced) return *policy.forced;
    if (saved) return *saved;
    return shippedDefault;
}

} // namespace MagicPortals::Controls
