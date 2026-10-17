#include "sim/Controls.hpp"

#include "sim/Files.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <filesystem>
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

// Whether a box stands in the RIGHT half of the view: the one test of "which side edge is nearer", which the slide (an arrow comes in
// from that edge) and the hint (only an arrow in the left half is one the text can be started clear of) both ask.
bool InRightHalf(const Hud::Rect& box, const glm::dvec2& viewUnits) {
    return box.Centre().x > viewUnits.x * 0.5;
}

// A number that is not a NaN or an infinity, so that no comparison below is asked of one.
bool Finite(double v) {
    return std::isfinite(v);
}

// ASCII lower case, for the file names Windows compares without regard to it.
std::string LowerAscii(std::string text) {
    for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

// The largest margin, as a fraction of a picture or of a box on each side, that leaves anything of it: a half leaves nothing.
constexpr double kMaxInset = 0.499;

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

bool Flag(const Json::Value& block, const char* key, bool& out, std::string& why, const std::string& where) {
    const Json::Value& value = block[key];
    if (!value.IsBool()) {
        why = Named(where, key) + " is missing or not true or false";
        return false;
    }
    out = value.AsBool();
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
// not on it (a thousandth of a step off is not) or is not a number of steps an int holds.
bool OnGrid(double value, int divisor, int& steps) {
    const double scaled = value * static_cast<double>(divisor);
    const double whole = std::round(scaled);
    // The range comes BEFORE the conversion, written as a test that a number passes: a double beyond an int is not a number of
    // steps, and converting it is undefined behaviour - which `--controls modern,size=1e30` reaches from the command line. A
    // NaN fails it, and so does the infinity that 1e308 times a divisor overflows to (the comparison below would be a NaN
    // and would let it through).
    constexpr double kLowest = static_cast<double>(std::numeric_limits<int>::lowest());
    constexpr double kHighest = static_cast<double>(std::numeric_limits<int>::max());
    if (!(whole >= kLowest && whole <= kHighest)) return false;
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
        // (Also where a limit is more steps than an int counts: a step of a millionth under a limit of a million.)
        why = where + ".min and .max are not whole steps of .step that an int can count, or max is not above min";
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
            // A picture the port draws itself has the margin sim/PadArt can draw, and no more: a value that loads must draw.
            const double mostInset = tier == ArtTier::Generated ? kMaxGeneratedBodyInset : kMaxInset;
            if (!Text(entry, "file", read_entry.file, why, where + "." + KeyOf(id)) ||
                !Num(entry, "body_inset", 0.0, mostInset, read_entry.bodyInset, why, where + "." + KeyOf(id))) {
                return false;
            }
            // The layer JOINS this name to the folder of its tier, and the generated tier writes there: a name that climbs out of
            // the folder (a separator, "..", a drive) is refused here, at load, so no path built from the data can leave it.
            if (!IsBareFileName(read_entry.file)) {
                why = where + "." + KeyOf(id) + ".file is not a bare file name (no folder, no \"..\", no drive): \"" + read_entry.file + "\"";
                return false;
            }
            read.art[static_cast<std::size_t>(tier)][Index(id)] = read_entry;
        }
    }
    // The generated tier WRITES its five files into one folder: two controls that named the same file would overwrite each other's picture
    // there, each run drawing the later glyph on both, with nothing said. (Windows folds case, so "A.png" and "a.png" are one file.)
    for (const Id later : kClaimOrder) {
        for (const Id earlier : kClaimOrder) {
            if (earlier == later) break;
            const ArtEntry& a = *read.art[static_cast<std::size_t>(ArtTier::Generated)][Index(earlier)];
            const ArtEntry& b = *read.art[static_cast<std::size_t>(ArtTier::Generated)][Index(later)];
            if (LowerAscii(a.file) == LowerAscii(b.file)) {
                why = std::string("art.generated.") + KeyOf(later) + ".file names the same file as art.generated." + KeyOf(earlier) +
                      ".file (\"" + b.file + "\"): each control's generated picture is a file of its own";
                return false;
            }
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
// between neighbours, walk_left at the bottom-left and walk_right at the bottom-right (Split, the owner's
// decision of 2026-10-16) or right beside it (Grouped, the arrangement of planning step 180), restart then
// pause at the top-right (pause on the corner), clear-portals at the top-left. Written in the order the
// design's arithmetic (CTL/arith2/final_layout.py, CTL6b/work/arrows_model.py) writes it.
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
        // Split: inset from the bottom-RIGHT corner by the same margins as the left arrow is from its corner.
        if (tuning.ArrowsIn(rules) == Arrows::Split) return glm::dvec2(safe.max.x - side - arrow, safe.max.y - bottom - arrow);
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

Insets InsetsFromWindowPx(double left, double top, double right, double bottom, double windowHeightPx, double viewHeightUnits) {
    Insets insets;
    if (!(windowHeightPx > 0.0) || !(viewHeightUnits > 0.0) || !Finite(windowHeightPx) || !Finite(viewHeightUnits)) return insets;
    const double unitsPerPx = viewHeightUnits / windowHeightPx;
    const auto side = [unitsPerPx](double px) { return Finite(px) && px > 0.0 ? px * unitsPerPx : 0.0; };
    insets.left = side(left);
    insets.top = side(top);
    insets.right = side(right);
    insets.bottom = side(bottom);
    return insets;
}

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

const char* NameOf(Arrows arrows) {
    return arrows == Arrows::Grouped ? "grouped" : "split";
}

std::optional<Arrows> ArrowsOfName(const std::string& name) {
    for (const Arrows arrows : kArrangements) {
        if (name == NameOf(arrows)) return arrows;
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
        !Num(layout, "face_inset", 0.0, kMaxInset, read.faceInset, why, "layout") ||
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
        // Every look names its own bottom margin, and a key that is missing is refused naming it. (It used to fall back to
        // layout.margin_px.bottom, so a misspelt one loaded cleanly and drew the pair at the wrong height.)
        if (!Num(block, "margin_bottom_px", 0.0, kBig, rule.marginBottomPx, why, where)) return fail();
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
    std::string defaultArrows;
    if (!Text(layout, "default_arrows", defaultArrows, why, "layout")) return fail();
    if (const auto arrows = ArrowsOfName(defaultArrows)) {
        read.defaultArrows = *arrows;
    } else {
        why = "layout.default_arrows is not split or grouped";
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

    if (!Object(root, "input", why, "") || !Flag(root["input"], "two_contacts", read.twoContacts, why, "input")) return fail();

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

// ---- the pictures a modern run draws -----------------------------------------------------------------------

namespace {

// The names Windows keeps for devices: a file called any of them, with or without an extension ("CON", "con.png"), is the device and not
// a file, in whatever folder it is asked for. Compared without regard to case, on the part before the first dot.
bool IsDeviceName(const std::string& name) {
    std::string stem = name.substr(0, name.find('.'));
    while (!stem.empty() && stem.back() == ' ') stem.pop_back();
    for (char& c : stem) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    for (const char* device : {"CON", "PRN", "AUX", "NUL", "CONIN$", "CONOUT$"}) {
        if (stem == device) return true;
    }
    return stem.size() == 4 && (stem.compare(0, 3, "COM") == 0 || stem.compare(0, 3, "LPT") == 0) && stem[3] >= '1' && stem[3] <= '9';
}

} // namespace

bool IsBareFileName(const std::string& name) {
    if (name.empty() || name == "." || name == "..") return false;
    // Windows reads "name." and "name " as "name", so a name that ends in either is another file than it says.
    if (name.back() == '.' || name.back() == ' ') return false;
    for (const char c : name) {
        const unsigned char u = static_cast<unsigned char>(c);
        // A control character (DEL among them) and every character Windows refuses in a name: a separator of either kind, a colon (a
        // drive, or an NTFS stream) and the five that are wildcards or redirections.
        if (u < 0x20 || u == 0x7F || c == '/' || c == '\\' || c == ':' || c == '<' || c == '>' || c == '"' || c == '|' || c == '?' ||
            c == '*') {
            return false;
        }
    }
    if (IsDeviceName(name)) return false;
    return name.find("..") == std::string::npos;
}

bool ReadArtManifest(const std::string& path, ArtManifest& out, std::string& why) {
    out.sizes.clear();
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) return true;
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        why = path + ": cannot open";
        return false;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string text = buffer.str();
    Json::Parser parser(text);
    Json::Value root;
    if (!parser.Parse(root)) {
        why = path + ": " + parser.Error();
        return false;
    }
    if (!root.IsObject()) {
        why = path + ": the document is not an object";
        return false;
    }
    ArtManifest read;
    // `outputs` names the pictures; a manifest without it names no size, which is the default's case.
    for (const auto& [name, entry] : root["outputs"].AsObject()) {
        // A picture the tool recorded without a size has none to be held to.
        if (!entry.IsObject() || !entry.Has("size")) continue;
        const Json::Array& size = entry["size"].AsArray();
        const auto whole = [](const Json::Value& v) {
            return v.IsNumber() && Finite(v.AsNumber()) && v.AsNumber() == std::floor(v.AsNumber()) && v.AsNumber() >= 1.0 &&
                   v.AsNumber() <= static_cast<double>(kMaxManifestPx);
        };
        if (size.size() != 2 || !whole(size[0]) || !whole(size[1])) {
            why = path + ": outputs." + name + ".size is not two whole numbers from 1 to " + std::to_string(kMaxManifestPx);
            return false;
        }
        read.sizes[name] = glm::ivec2(static_cast<int>(size[0].AsNumber()), static_cast<int>(size[1].AsNumber()));
    }
    out = std::move(read);
    return true;
}

glm::ivec2 ExpectedArtSize(const ArtManifest& manifest, const std::string& file, int defaultPx) {
    const auto named = manifest.sizes.find(file);
    return named != manifest.sizes.end() ? named->second : glm::ivec2(defaultPx);
}

const char* NameOf(const ArtSource& source) {
    return source ? NameOf(*source) : "flat";
}

std::string ArtLine(const ArtSources& sources) {
    std::string line = "modern controls art:";
    for (const Id id : kClaimOrder) {
        line += std::string(" ") + KeyOf(id) + "=" + NameOf(sources[Index(id)]);
    }
    return line;
}

bool ArtMixed(const ArtSources& sources) {
    for (const ArtSource& source : sources) {
        if (source != sources[0]) return true;
    }
    return false;
}

bool ArtHalfDressed(const ArtSources& sources) {
    if (!ArtMixed(sources)) return false;
    for (const ArtSource& source : sources) {
        if (source == ArtSource(ArtTier::MagicRampage)) return true;
    }
    return false;
}

// ---- the tuning ------------------------------------------------------------------------------------------

bool FlagItems::Any() const {
    return preset || arrows || twoContacts || Tunes();
}

bool FlagItems::Tunes() const {
    if (size || opacity) return true;
    for (const auto& item : move) {
        if (item) return true;
    }
    return false;
}

Preset Tuning::PresetIn(const Rules& rules) const {
    return preset.value_or(rules.defaultPreset);
}

Arrows Tuning::ArrowsIn(const Rules& rules) const {
    return arrows.value_or(rules.defaultArrows);
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
    out.sizeSteps = steps(controls[TuningKey::kSize], rules.size);
    out.opacitySteps = steps(controls[TuningKey::kOpacity], rules.opacity);

    // A save made against another arrangement keeps what is not a place and drops the places.
    const Json::Value& layout = controls[TuningKey::kLayout];
    const bool sameLayout = !controls.Has(TuningKey::kLayout) || (layout.IsNumber() && layout.AsNumber() == static_cast<double>(kLayoutVersion));
    const Json::Value& moves = controls[TuningKey::kMove];
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
    return "{" + ToJsonMembers(rules) + "}";
}

std::string Tuning::ToJsonMembers(const Rules& rules) const {
    const Tuning clamped = Clamped(rules);
    std::string text;
    const auto add = [&text](const std::string& member) {
        text += text.empty() ? "" : ",";
        text += member;
    };
    const auto member = [](const char* key) { return std::string("\"") + key + "\":"; };
    if (clamped.sizeSteps) add(member(TuningKey::kSize) + NumberText(rules.size.ValueOf(*clamped.sizeSteps)));
    if (clamped.opacitySteps) add(member(TuningKey::kOpacity) + NumberText(rules.opacity.ValueOf(*clamped.opacitySteps)));
    std::string moves;
    for (const Id id : kClaimOrder) {
        const glm::dvec2& m = clamped.move[Index(id)];
        if (m.x == 0.0 && m.y == 0.0) continue;
        moves += moves.empty() ? "" : ",";
        moves += std::string("\"") + KeyOf(id) + "\":[" + NumberText(m.x) + "," + NumberText(m.y) + "]";
    }
    if (!moves.empty()) add(member(TuningKey::kMove) + "{" + moves + "}");
    return text;
}

const char* FlagUsage() {
    static const std::string usage = [] {
        std::string text = std::string("--controls <classic|modern>[,key=value...]  keys: ") + TuningKey::kPreset + "=";
        for (const Preset preset : kPresets) text += std::string(preset == kPresets.front() ? "" : "|") + NameOf(preset);
        text += std::string(", ") + TuningKey::kArrows + "=" + NameOf(Arrows::Split) + "|" + NameOf(Arrows::Grouped);
        text += std::string(", ") + TuningKey::kContacts + "=" + TuningKey::kContactsOne + "|" + TuningKey::kContactsTwo;
        text += std::string(", ") + TuningKey::kSize + "=<n>, " + TuningKey::kOpacity + "=<n>, and a move as <key>=<dx>:<dy> for ";
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
        if (key == TuningKey::kPreset) {
            if (read.preset) return refuse("preset is given twice");
            const auto preset = PresetOfName(value);
            if (!preset) return refuse("'" + value + "' is not a preset");
            read.preset = preset;
        } else if (key == TuningKey::kArrows) {
            if (read.arrows) return refuse("arrows is given twice");
            const auto arrows = ArrowsOfName(value);
            if (!arrows) return refuse("'" + value + "' is not an arrangement");
            read.arrows = arrows;
        } else if (key == TuningKey::kContacts) {
            if (read.twoContacts) return refuse("contacts is given twice");
            if (value != TuningKey::kContactsOne && value != TuningKey::kContactsTwo) return refuse("'" + value + "' is not one or two");
            read.twoContacts = value == TuningKey::kContactsTwo;
        } else if (key == TuningKey::kSize || key == TuningKey::kOpacity) {
            std::optional<double>& slot = key == TuningKey::kSize ? read.size : read.opacity;
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
    tuning.arrows = items.arrows;
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

// ---- the saved choice -----------------------------------------------------------------------------------

bool Store::Open(const std::string& directory, const Rules& rules, std::string& error) {
    m_rules = rules;
    m_mode.reset();
    m_tuning = Tuning{};
    m_path.clear();
    m_fromFile = false;
    m_written.clear();
    error.clear();

    if (directory.empty()) return true;
    m_path = (std::filesystem::path(directory) / kStoreFileName).string();

    std::error_code ec;
    if (!std::filesystem::exists(m_path, ec) || ec) return true; // never chosen: the shipped default

    std::ifstream file(m_path, std::ios::binary);
    if (!file) {
        error = m_path + ": cannot open";
        return false;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string text = buffer.str();
    Json::Parser parser(text);
    Json::Value root;
    if (!parser.Parse(root)) {
        error = m_path + ": " + parser.Error();
        return false;
    }
    if (!root.IsObject()) {
        error = m_path + ": the document is not an object";
        return false;
    }
    // Field by field, as the original's loadValue gives its default for a value the file lacks.
    const Json::Value& controls = root[TuningKey::kControls];
    if (controls.IsObject()) {
        if (controls[TuningKey::kMode].IsString()) m_mode = StyleOfName(controls[TuningKey::kMode].AsString());
        m_tuning = Tuning::ReadFrom(controls, m_rules);
    }
    m_fromFile = true;
    return true;
}

void Store::SetTuning(const Tuning& tuning) {
    // Only what a player keeps: the look and the arrangement are a capture's and are never written, so they are not held either.
    Tuning kept = tuning;
    kept.preset.reset();
    kept.arrows.reset();
    m_tuning = kept.Clamped(m_rules);
}

std::string Store::Text() const {
    std::string text = std::string("{\"") + TuningKey::kVersion + "\":1,\"" + TuningKey::kControls + "\":{";
    if (m_mode) text += std::string("\"") + TuningKey::kMode + "\":\"" + NameOf(*m_mode) + "\",";
    text += std::string("\"") + TuningKey::kLayout + "\":" + std::to_string(kLayoutVersion);
    const std::string members = m_tuning.ToJsonMembers(m_rules);
    if (!members.empty()) text += "," + members;
    return text + "}}";
}

bool Store::Save(std::string& error) {
    error.clear();
    if (m_path.empty()) return true;
    const std::string text = Text();
    if (text == m_written) return true;

    // The write every store shares (sim/Files, planning step 189), which is this Save's own shape from planning step 187: a temporary
    // of this call's own name, flushed and checked after the close, renamed over the file in one step, never removing the live file,
    // the temporary gone on every failure. No backup is kept: this file is a choice and not progress, and a directory of saves that
    // holds controls.json and nothing beside it is what the Options cases assert. A failure is not remembered as a write, so the
    // next Save tries again with the bytes it still holds.
    if (!Files::WriteAtomic(m_path, text, Files::WriteOptions{}, error)) return false;
    m_written = text;
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
        const bool fromRight = InRightHalf(settled.rect, viewUnits);
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
    double clear = row.classicLeft;
    for (const Id id : kClaimOrder) {
        if (!IsWalk(id) || !shown[Index(id)]) continue;
        // Only an arrow in the LEFT half of the view (the slide's own nearer-edge test) is one the text can be started clear of: an arrow in
        // the right half (split: the bottom-right corner) is crossed by the text, as the classic text crosses its disc, when the free span
        // between the two is narrower than the text - starting further right would only carry the text off the view.
        if (InRightHalf(layout.At(id).rect, viewUnits)) continue;
        const Hud::Rect& hit = layout.At(id).hit.rect;
        const glm::dvec2 max = hit.Max();
        if (hit.min.y < row.top + row.height && max.y > row.top) {
            meets = true;
            clear = std::max(clear, max.x);
        }
    }
    if (!meets) return row.classicLeft;
    return std::min(clear, viewUnits.x - row.extent - rules.hintMarginUnits);
}

// ---- two contacts (planning step 191) ---------------------------------------------------------------------------

namespace {

bool IsLive(const std::vector<LiveContact>& live, int id) {
    if (id == kNoContact) return false;
    for (const LiveContact& contact : live) {
        if (contact.id == id) return true;
    }
    return false;
}

} // namespace

ContactRoles AssignContacts(const std::vector<LiveContact>& live, const ContactRoles& before) {
    ContactRoles roles;
    if (live.empty()) return roles;
    // One finger is the world's, and the controls' when it is on a control: the combined reading the layer always had. (A finger left
    // from last tick that held the controls role is NOT kept here: with nobody else down, where it is now is what it is.)
    if (live.size() == 1) {
        roles.world = live.front().id;
        if (live.front().onControl) roles.controls = live.front().id;
        return roles;
    }
    // The controls finger: the one that held the role and is still down, else the first that is on a control.
    if (IsLive(live, before.controls)) {
        roles.controls = before.controls;
    } else {
        for (const LiveContact& contact : live) {
            if (contact.onControl) {
                roles.controls = contact.id;
                break;
            }
        }
    }
    // The world finger: neither the controls' nor on a control. The one that held the role and still qualifies keeps it.
    const auto candidate = [&live, &roles](int id) {
        if (id == roles.controls) return false;
        for (const LiveContact& contact : live) {
            if (contact.id == id) return !contact.onControl;
        }
        return false;
    };
    if (candidate(before.world)) {
        roles.world = before.world;
    } else {
        for (const LiveContact& contact : live) {
            if (candidate(contact.id)) {
                roles.world = contact.id;
                break;
            }
        }
    }
    return roles;
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
