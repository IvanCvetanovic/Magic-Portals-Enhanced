// The pixel-centre rule at the frame's edge: edge_cover.json and sim/EdgeCover's arithmetic
// (planning step 165).
//
// What is pinned, and from where:
//
//   the rules   edge_cover.json's seven keys, each with its source in the file: the switch,
//               the half pixel (the pixel-centre rule), the first sample of the 4x pattern
//               (Vulkan's standard positions), the turn below which a quad is a picture of
//               the frame's own rows and columns, the complementary half's switch, the
//               log's measuring switch, and the pool of strips made ready first. Required,
//               and refused when out of range.
//   the snap    one axis, at every boundary of the rule: a low edge in (first sample, half a
//               pixel] goes out to the frame's side, a high edge in [first sample, half a
//               pixel) short of the far side does, and every other edge stays - one that
//               leaves every sample covered, one past the pixel's centre, one inside the
//               frame - and a picture that does not cover the edge pixel's centre is left.
//               With the complementary half, the edge that faces INTO the frame goes out to
//               the pixel's boundary the same way, at the same boundaries.
//   the cull    a picture whose part on the frame lies wholly in one outer pixel and covers
//               its centre nowhere is left out of the frame; no other is.
//   the slivers the edge in a pixel's inner half with the picture going on inward: named,
//               for the log, and left as it is.
//   the strips  what a picture needs at each side and at the corners: disjoint from the
//               picture and from each other, and the picture with its strips is the
//               extended rectangle exactly; the long axis cut a pixel past the frame.
//   the uv      the picture's own linear map carried on across a strip: at any point of a
//               strip it is what the picture's map says there.
//
// Pure arithmetic and files the suite writes: it runs anywhere.

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "sim/EdgeCover.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace MagicPortals;

namespace {

const std::string kPortData = MAGICPORTALS_PORT_DATA_DIR;

EdgeCover::Rules TheRules() {
    EdgeCover::Rules rules;
    std::string error;
    CHECK_MSG(EdgeCover::LoadRules(kPortData + "/edge_cover.json", rules, error), error);
    return rules;
}

bool Same(double a, double b) { return std::fabs(a - b) <= 1e-9; }

double Area(const EdgeCover::Rect& r) {
    return std::max(0.0, r.x.hi - r.x.lo) * std::max(0.0, r.y.hi - r.y.lo);
}

double Overlap(const EdgeCover::Rect& a, const EdgeCover::Rect& b) {
    EdgeCover::Rect both;
    both.x = {std::max(a.x.lo, b.x.lo), std::min(a.x.hi, b.x.hi)};
    both.y = {std::max(a.y.lo, b.y.lo), std::min(a.y.hi, b.y.hi)};
    return Area(both);
}

// A file in the temp folder, to read as rules.
std::string Written(const std::string& name, const std::string& body) {
    const std::filesystem::path path = std::filesystem::temp_directory_path() / ("supersonic-test-mp-edgecover-" + name);
    std::ofstream(path, std::ios::binary) << body;
    return path.string();
}

std::string RulesText(const std::string& enabled, const std::string& band, const std::string& first,
                      const std::string& turn, const std::string& pool = "24", const std::string& complement = "true",
                      const std::string& logEvery = "false") {
    std::vector<std::string> parts;
    if (!enabled.empty()) parts.push_back("\"enabled\": " + enabled);
    if (!complement.empty()) parts.push_back("\"complement\": " + complement);
    if (!logEvery.empty()) parts.push_back("\"log_every_frame\": " + logEvery);
    if (!band.empty()) parts.push_back("\"band_px\": " + band);
    if (!first.empty()) parts.push_back("\"first_sample_px\": " + first);
    if (!turn.empty()) parts.push_back("\"turn_rad\": " + turn);
    if (!pool.empty()) parts.push_back("\"pool_size\": " + pool);
    std::string body;
    for (std::size_t i = 0; i < parts.size(); ++i) body += (i == 0 ? "" : ", ") + parts[i];
    return "{\"edge_cover\": {" + body + "}}";
}

void TheRulesAreEdgeCoverJsons() {
    const EdgeCover::Rules rules = TheRules();
    CHECK_MSG(rules.enabled, "the switch is on: the original's frame");
    CHECK(rules.bandPx == 0.5);
    CHECK(rules.firstSamplePx == 0.125);
    CHECK(rules.turnRad == 1e-6);
    CHECK_MSG(rules.complement, "the complementary half is on: the original's frame at an inward edge too");
    CHECK_MSG(!rules.logEveryFrame, "the measuring switch is off as shipped");
    CHECK_MSG(rules.poolSize == 24, "24 strips are made ready, first of all");

    // Every key is required and read as what it is.
    EdgeCover::Rules read;
    std::string error;
    CHECK_MSG(EdgeCover::LoadRules(Written("ok.json", RulesText("false", "0.5", "0.125", "1e-6")), read, error), error);
    CHECK_MSG(!read.enabled, "false reads as false: the pictures alone, as before step 165");
    CHECK_MSG(EdgeCover::LoadRules(Written("complement_off.json", RulesText("true", "0.5", "0.125", "1e-6", "24", "false")), read, error), error);
    CHECK_MSG(!read.complement && read.enabled, "the complementary half reads as its own switch");
    CHECK_MSG(EdgeCover::LoadRules(Written("log_on.json", RulesText("true", "0.5", "0.125", "1e-6", "24", "true", "true")), read, error), error);
    CHECK_MSG(read.logEveryFrame, "the measuring switch reads as true");
    CHECK_MSG(EdgeCover::LoadRules(Written("pool_zero.json", RulesText("true", "0.5", "0.125", "1e-6", "0")), read, error), error);
    CHECK_MSG(read.poolSize == 0, "a pool of 0 is allowed: every strip is made on demand");
    const struct {
        const char* name;
        std::string body;
        const char* mentions;
    } refused[] = {
        {"no_switch.json", RulesText("", "0.5", "0.125", "1e-6"), "enabled"},
        {"switch_number.json", RulesText("1", "0.5", "0.125", "1e-6"), "enabled"},
        {"no_band.json", RulesText("true", "", "0.125", "1e-6"), "band_px"},
        {"no_first.json", RulesText("true", "0.5", "", "1e-6"), "first_sample_px"},
        {"no_turn.json", RulesText("true", "0.5", "0.125", ""), "turn_rad"},
        {"no_complement.json", RulesText("true", "0.5", "0.125", "1e-6", "24", ""), "complement"},
        {"no_log_every_frame.json", RulesText("true", "0.5", "0.125", "1e-6", "24", "true", ""), "log_every_frame"},
        {"log_every_frame_number.json", RulesText("true", "0.5", "0.125", "1e-6", "24", "true", "0"), "log_every_frame"},
        {"complement_number.json", RulesText("true", "0.5", "0.125", "1e-6", "24", "1"), "complement"},
        {"band_zero.json", RulesText("true", "0", "0", "1e-6"), "band_px"},
        {"band_past_centre.json", RulesText("true", "0.6", "0.125", "1e-6"), "band_px"},
        {"first_at_band.json", RulesText("true", "0.5", "0.5", "1e-6"), "first_sample_px"},
        {"first_negative.json", RulesText("true", "0.5", "-0.1", "1e-6"), "first_sample_px"},
        {"turn_negative.json", RulesText("true", "0.5", "0.125", "-1"), "turn_rad"},
        {"no_pool.json", RulesText("true", "0.5", "0.125", "1e-6", ""), "pool_size"},
        {"pool_fraction.json", RulesText("true", "0.5", "0.125", "1e-6", "2.5"), "pool_size"},
        {"pool_negative.json", RulesText("true", "0.5", "0.125", "1e-6", "-1"), "pool_size"},
        {"pool_huge.json", RulesText("true", "0.5", "0.125", "1e-6", "257"), "pool_size"},
        {"no_block.json", "{}", "edge_cover"},
        {"not_json.json", "{\"edge_cover\": ", ""},
    };
    for (const auto& one : refused) {
        EdgeCover::Rules out;
        out.enabled = true;
        std::string why;
        const bool ok = EdgeCover::LoadRules(Written(one.name, one.body), out, why);
        CHECK_MSG(!ok, std::string(one.name) + " is refused");
        CHECK_MSG(why.find(one.mentions) != std::string::npos,
                  std::string(one.name) + " says which key: " + why);
        CHECK_MSG(out.enabled, std::string(one.name) + " leaves the rules as they were");
    }
    CHECK_MSG(!EdgeCover::LoadRules(kPortData + "/no_such_file.json", read, error), "a missing file is refused");
    CHECK_MSG(error.find("cannot read") != std::string::npos, error);
}

void TheSnapIsThePixelCentreRuleAtEveryBoundary() {
    const EdgeCover::Rules rules = TheRules();
    const double frame = 1280.0;
    const auto snap = [&](double lo, double hi) { return EdgeCover::Snap(rules, {lo, hi}, frame); };

    // A low edge: out to the frame's side when it lies past the first sample and at or before
    // the pixel's centre. A centre exactly on the edge is drawn (the rule's >=).
    const double lowGoes[] = {0.1251, 0.2, 0.3, 0.4999, 0.5};
    for (const double lo : lowGoes) {
        const EdgeCover::Interval got = snap(lo, 600.0);
        CHECK_MSG(got.lo == 0.0 && got.hi == 600.0, "a low edge at " + std::to_string(lo) + " goes out to 0");
    }
    // One that leaves every sample covered, one past the centre, one on or off the frame:
    const double lowStays[] = {-3.0, 0.0, 0.05, 0.125, 0.5001, 0.6, 0.99, 1.0, 3.0, 100.25};
    for (const double lo : lowStays) {
        const EdgeCover::Interval got = snap(lo, 600.0);
        CHECK_MSG(got.lo == lo && got.hi == 600.0, "a low edge at " + std::to_string(lo) + " stays");
    }
    // A high edge, mirrored: out to the frame's side from the first sample's gap to the centre's,
    // the centre exactly on the edge not drawn (the rule's <).
    const double highGoes[] = {1279.875, 1279.8, 1279.69, 1279.51, 1279.5001};
    for (const double hi : highGoes) {
        const EdgeCover::Interval got = snap(600.0, hi);
        CHECK_MSG(got.lo == 600.0 && got.hi == frame, "a high edge at " + std::to_string(hi) + " goes out to 1280");
    }
    const double highStays[] = {1283.0, 1280.0, 1279.95, 1279.876, 1279.5, 1279.4, 1279.0, 1000.0};
    for (const double hi : highStays) {
        const EdgeCover::Interval got = snap(600.0, hi);
        CHECK_MSG(got.lo == 600.0 && got.hi == hi, "a high edge at " + std::to_string(hi) + " stays");
    }
    // The measured case: 2-05's bar4_contrast.png ends at 1279.6875 px.
    const EdgeCover::Interval bar = snap(1279.6875 - 180.0, 1279.6875);
    CHECK_MSG(bar.hi == frame, "2-05's wall, ending at 1279.6875 px, is drawn out to the frame");

    // Both ends at once, and a frame of another size.
    const EdgeCover::Interval both = snap(0.3, 1279.7);
    CHECK(both.lo == 0.0 && both.hi == frame);
    const EdgeCover::Interval wide = EdgeCover::Snap(rules, {0.3, 1919.7}, 1920.0);
    CHECK_MSG(wide.lo == 0.0 && wide.hi == 1920.0, "a 1920 px frame's edges are its own");

    // A picture that does not cover the edge pixel's centre is not the original's pixel: left.
    const EdgeCover::Interval sliverLow = snap(0.3, 0.4);
    CHECK_MSG(sliverLow.lo == 0.3 && sliverLow.hi == 0.4, "a picture 0.1 px wide at the side covers no centre");
    const EdgeCover::Interval sliverHigh = snap(1279.6, 1279.7);
    CHECK_MSG(sliverHigh.lo == 1279.6 && sliverHigh.hi == 1279.7, "nor one at the far side");
    const EdgeCover::Interval narrowLow = snap(0.3, 0.5);
    CHECK_MSG(narrowLow.lo == 0.3 && narrowLow.hi == 0.5, "nor one that ends on the centre (a high edge's centre is not drawn)");
    // Past the frame entirely.
    const EdgeCover::Interval gone = snap(-500.0, -10.0);
    CHECK(gone.lo == -500.0 && gone.hi == -10.0);
    const EdgeCover::Interval beyond = snap(1400.0, 1500.0);
    CHECK(beyond.lo == 1400.0 && beyond.hi == 1500.0);

    // What cannot be a picture or a frame comes back as given.
    const EdgeCover::Interval none = snap(10.0, 10.0);
    CHECK(none.lo == 10.0 && none.hi == 10.0);
    const EdgeCover::Interval flipped = snap(10.0, 5.0);
    CHECK(flipped.lo == 10.0 && flipped.hi == 5.0);
    const EdgeCover::Interval noFrame = EdgeCover::Snap(rules, {0.3, 1279.7}, 0.0);
    CHECK(noFrame.lo == 0.3 && noFrame.hi == 1279.7);

    // The switch and the numbers are the rules': a band of a quarter pixel moves the line.
    EdgeCover::Rules narrow = rules;
    narrow.bandPx = 0.25;
    const EdgeCover::Interval quarter = EdgeCover::Snap(narrow, {0.3, 600.0}, frame);
    CHECK_MSG(quarter.lo == 0.3, "a 0.25 band leaves an edge at 0.3");
    const EdgeCover::Interval inside = EdgeCover::Snap(narrow, {0.2, 600.0}, frame);
    CHECK_MSG(inside.lo == 0.0, "and takes one at 0.2");
}

void StripsFillExactlyWhatTheRuleDraws() {
    EdgeCover::Rules rules = TheRules();
    const double w = 1280.0;
    const double h = 720.0;
    using EdgeCover::Rect;

    // Not at the frame's edge: none. Inside, on it, past it, and at a boundary.
    const Rect inside{{100.0, 600.0}, {100.0, 300.0}};
    CHECK_EQ(EdgeCover::StripsOf(rules, inside, w, h).count, static_cast<std::size_t>(0));
    const Rect onEdges{{0.0, 1280.0}, {0.0, 720.0}};
    CHECK_EQ(EdgeCover::StripsOf(rules, onEdges, w, h).count, static_cast<std::size_t>(0));
    const Rect past{{-60.0, 1340.0}, {-4.0, 800.0}};
    CHECK_EQ(EdgeCover::StripsOf(rules, past, w, h).count, static_cast<std::size_t>(0));
    const Rect pastCentre{{0.6, 600.0}, {0.7, 300.0}};
    CHECK_EQ(EdgeCover::StripsOf(rules, pastCentre, w, h).count, static_cast<std::size_t>(0));

    // 2-05's wall: one strip on the right, over the wall's rows, 0.3125 px wide.
    const Rect wall{{1099.6875, 1279.6875}, {421.0, 586.0}};
    const EdgeCover::Strips right = EdgeCover::StripsOf(rules, wall, w, h);
    CHECK_EQ(right.count, static_cast<std::size_t>(1));
    if (right.count == 1) {
        CHECK(right.strip[0].side == EdgeCover::Side::Right);
        CHECK(Same(right.strip[0].rect.x.lo, 1279.6875) && Same(right.strip[0].rect.x.hi, 1280.0));
        CHECK(Same(right.strip[0].rect.y.lo, 421.0) && Same(right.strip[0].rect.y.hi, 586.0));
    }

    // Each side alone.
    const Rect left{{0.3, 600.0}, {100.0, 200.0}};
    const EdgeCover::Strips l = EdgeCover::StripsOf(rules, left, w, h);
    CHECK_EQ(l.count, static_cast<std::size_t>(1));
    if (l.count == 1) {
        CHECK(l.strip[0].side == EdgeCover::Side::Left);
        CHECK(Same(l.strip[0].rect.x.lo, 0.0) && Same(l.strip[0].rect.x.hi, 0.3));
        CHECK(Same(l.strip[0].rect.y.lo, 100.0) && Same(l.strip[0].rect.y.hi, 200.0));
    }
    const Rect top{{400.0, 800.0}, {0.4, 300.0}};
    const EdgeCover::Strips t = EdgeCover::StripsOf(rules, top, w, h);
    CHECK_EQ(t.count, static_cast<std::size_t>(1));
    if (t.count == 1) {
        CHECK(t.strip[0].side == EdgeCover::Side::Top);
        CHECK(Same(t.strip[0].rect.x.lo, 400.0) && Same(t.strip[0].rect.x.hi, 800.0));
        CHECK(Same(t.strip[0].rect.y.lo, 0.0) && Same(t.strip[0].rect.y.hi, 0.4));
    }
    const Rect bottom{{400.0, 800.0}, {500.0, 719.6}};
    const EdgeCover::Strips b = EdgeCover::StripsOf(rules, bottom, w, h);
    CHECK_EQ(b.count, static_cast<std::size_t>(1));
    if (b.count == 1) {
        CHECK(b.strip[0].side == EdgeCover::Side::Bottom);
        CHECK(Same(b.strip[0].rect.y.lo, 719.6) && Same(b.strip[0].rect.y.hi, 720.0));
    }

    // The corners and the whole frame: the picture and its strips are the extended rectangle,
    // each strip apart from the picture and from every other.
    const Rect cases[] = {
        {{0.3, 600.0}, {0.4, 300.0}},          // top left
        {{700.0, 1279.7}, {0.2, 300.0}},       // top right
        {{0.3, 600.0}, {400.0, 719.8}},        // bottom left
        {{700.0, 1279.6}, {400.0, 719.55}},    // bottom right
        {{0.3, 1279.7}, {0.2, 719.8}},         // all four
        {{0.3, 1279.7}, {100.0, 300.0}},       // both sides
        {{100.0, 800.0}, {0.2, 719.8}},        // top and bottom
    };
    for (const Rect& picture : cases) {
        const EdgeCover::Strips s = EdgeCover::StripsOf(rules, picture, w, h);
        const EdgeCover::Interval ex = EdgeCover::Snap(rules, picture.x, w);
        const EdgeCover::Interval ey = EdgeCover::Snap(rules, picture.y, h);
        const double extended = Area(Rect{ex, ey});
        double total = Area(picture);
        bool apart = true;
        for (std::size_t i = 0; i < s.count; ++i) {
            total += Area(s.strip[i].rect);
            apart = apart && Overlap(s.strip[i].rect, picture) == 0.0;
            for (std::size_t j = i + 1; j < s.count; ++j) apart = apart && Overlap(s.strip[i].rect, s.strip[j].rect) == 0.0;
        }
        CHECK_MSG(apart, "strips never overlap the picture or each other, picture x " + std::to_string(picture.x.lo) +
                             ".." + std::to_string(picture.x.hi));
        CHECK_MSG(std::fabs(total - extended) < 1e-6, "the picture and its strips are the extended rectangle: " +
                                                          std::to_string(total) + " against " + std::to_string(extended));
    }
    // All four at once is the frame, exactly.
    const EdgeCover::Strips all = EdgeCover::StripsOf(rules, cases[4], w, h);
    CHECK_EQ(all.count, static_cast<std::size_t>(4));
    double frame = Area(cases[4]);
    for (std::size_t i = 0; i < all.count; ++i) frame += Area(all.strip[i].rect);
    CHECK_MSG(std::fabs(frame - w * h) < 1e-6, "a picture at all four edges and its strips are the whole frame");

    // The long axis is cut a pixel past the frame, where nothing is drawn.
    const Rect tall{{0.3, 600.0}, {-5000.0, 5000.0}};
    const EdgeCover::Strips cut = EdgeCover::StripsOf(rules, tall, w, h);
    CHECK_EQ(cut.count, static_cast<std::size_t>(1));
    if (cut.count == 1) CHECK(Same(cut.strip[0].rect.y.lo, -1.0) && Same(cut.strip[0].rect.y.hi, 721.0));
    // And a picture whose rows the frame does not show needs none.
    const Rect below{{0.3, 600.0}, {900.0, 1000.0}};
    CHECK_EQ(EdgeCover::StripsOf(rules, below, w, h).count, static_cast<std::size_t>(0));
    const Rect above{{0.3, 600.0}, {-300.0, -10.0}};
    CHECK_EQ(EdgeCover::StripsOf(rules, above, w, h).count, static_cast<std::size_t>(0));
    const Rect empty{{5.0, 5.0}, {0.0, 100.0}};
    CHECK_EQ(EdgeCover::StripsOf(rules, empty, w, h).count, static_cast<std::size_t>(0));

    // The switch lives in the layer: off, the arithmetic still says what the rule would do.
    rules.enabled = false;
    CHECK_EQ(EdgeCover::StripsOf(rules, left, w, h).count, static_cast<std::size_t>(1));
}

void AStripsUvIsThePicturesOwnMapCarriedOn() {
    const EdgeCover::Rules rules = TheRules();
    const double w = 1280.0;
    const double h = 720.0;
    // A picture whose own uv is a cell of a sheet: scale (0.5, 0.25), offset (0.25, 0.5).
    const glm::dvec2 scale(0.5, 0.25);
    const glm::dvec2 offset(0.25, 0.5);
    const EdgeCover::Rect pictures[] = {
        {{0.3, 600.0}, {100.0, 200.0}},
        {{700.0, 1279.69}, {0.4, 719.7}},
        {{0.2, 1279.8}, {0.3, 719.6}},
        // The complementary half's strips: a bar's inward edge, and a picture coming in from a side.
        {{-100.0, 1400.0}, {-500.0, 0.7}},
        {{-100.0, 1400.0}, {719.3, 1200.0}},
        {{-50.0, 0.7}, {100.0, 200.0}},
        {{1279.3, 1500.0}, {100.0, 200.0}},
    };
    for (const EdgeCover::Rect& picture : pictures) {
        const EdgeCover::Strips strips = EdgeCover::StripsOf(rules, picture, w, h);
        CHECK(strips.count > 0);
        for (std::size_t i = 0; i < strips.count; ++i) {
            const EdgeCover::Rect& strip = strips.strip[i].rect;
            const EdgeCover::Uv uv = EdgeCover::UvOf(picture, strip, scale, offset);
            // A strip's own u and v run 0 to 1 across it, left to right and top to bottom. At
            // each of nine points the map must give what the picture's map gives there.
            for (const double fu : {0.0, 0.5, 1.0}) {
                for (const double fv : {0.0, 0.5, 1.0}) {
                    const double x = strip.x.lo + fu * (strip.x.hi - strip.x.lo);
                    const double y = strip.y.lo + fv * (strip.y.hi - strip.y.lo);
                    const double u = uv.offset.x + uv.scale.x * fu;
                    const double v = uv.offset.y + uv.scale.y * fv;
                    const double wantU = offset.x + scale.x * (x - picture.x.lo) / (picture.x.hi - picture.x.lo);
                    const double wantV = offset.y + scale.y * (y - picture.y.lo) / (picture.y.hi - picture.y.lo);
                    CHECK_MSG(Same(u, wantU) && Same(v, wantV),
                              "strip " + std::to_string(i) + " at (" + std::to_string(fu) + ", " + std::to_string(fv) +
                                  "): uv (" + std::to_string(u) + ", " + std::to_string(v) + ") against (" +
                                  std::to_string(wantU) + ", " + std::to_string(wantV) + ")");
                }
            }
        }
    }
    // The picture over itself is its own map; a strip left of it runs back before its offset.
    const EdgeCover::Rect picture{{0.3, 600.0}, {100.0, 200.0}};
    const EdgeCover::Uv self = EdgeCover::UvOf(picture, picture, scale, offset);
    CHECK(Same(self.scale.x, scale.x) && Same(self.scale.y, scale.y) && Same(self.offset.x, offset.x) &&
          Same(self.offset.y, offset.y));
    const EdgeCover::Rect left{{0.0, 0.3}, {100.0, 200.0}};
    const EdgeCover::Uv before = EdgeCover::UvOf(picture, left, scale, offset);
    CHECK_MSG(before.offset.x < offset.x && Same(before.offset.x + before.scale.x, offset.x),
              "a strip left of the picture ends where the picture's u begins");
    CHECK(Same(before.offset.y, offset.y) && Same(before.scale.y, scale.y));
    // A picture with no size has no map to carry: the base comes back.
    const EdgeCover::Rect nothing{{5.0, 5.0}, {5.0, 9.0}};
    const EdgeCover::Uv same = EdgeCover::UvOf(nothing, left, scale, offset);
    CHECK(same.scale == scale && same.offset == offset);
}

// THE COMPLEMENTARY HALF at every boundary of its rule (Rules::complement).
void TheInwardEdgeGoesOutToThePixelsBoundary() {
    const EdgeCover::Rules rules = TheRules();
    const double frame = 1280.0;
    const auto snap = [&](double lo, double hi) { return EdgeCover::Snap(rules, {lo, hi}, frame); };

    // A picture that begins outside the frame: its high edge, in the first pixel's inner half, goes
    // out to 1 when the centre is covered (the edge past 0.5) and a sample is left uncovered (the
    // edge at or before 0.875: a sample exactly on a high edge is not drawn).
    const double nearGoes[] = {0.5001, 0.6, 0.7, 0.85, 0.875};
    for (const double hi : nearGoes) {
        const EdgeCover::Interval got = snap(-500.0, hi);
        CHECK_MSG(got.lo == -500.0 && got.hi == 1.0, "a high edge at " + std::to_string(hi) + " goes out to 1");
    }
    const double nearStays[] = {0.0, 0.1, 0.125, 0.4, 0.5, 0.876, 0.95, 1.0, 1.4, 100.0};
    for (const double hi : nearStays) {
        const EdgeCover::Interval got = snap(-500.0, hi);
        CHECK_MSG(got.lo == -500.0 && got.hi == hi, "a high edge at " + std::to_string(hi) + " stays");
    }
    // The far side, mirrored: the low edge of a picture that runs on past the frame, in the last
    // pixel's inner half, goes out to 1279 when the centre is covered (the edge at or before 1279.5:
    // a centre exactly on a low edge is drawn) and a sample is uncovered (the edge past 1279.125: a
    // sample exactly on a low edge is drawn).
    const double farGoes[] = {1279.1251, 1279.2, 1279.3, 1279.45, 1279.5};
    for (const double lo : farGoes) {
        const EdgeCover::Interval got = snap(lo, 1500.0);
        CHECK_MSG(got.lo == 1279.0 && got.hi == 1500.0, "a low edge at " + std::to_string(lo) + " goes out to 1279");
    }
    const double farStays[] = {1279.0, 1279.1, 1279.125, 1279.5001, 1279.9, 1280.0, 1300.0, 1000.0};
    for (const double lo : farStays) {
        const EdgeCover::Interval got = snap(lo, 1500.0);
        CHECK_MSG(got.lo == lo && got.hi == 1500.0, "a low edge at " + std::to_string(lo) + " stays");
    }
    // A picture inside the outer pixel that covers its centre is drawn out at both ends; one that
    // does not is not the pixel's.
    const EdgeCover::Interval inside = snap(0.2, 0.7);
    CHECK_MSG(inside.lo == 0.0 && inside.hi == 1.0, "a picture 0.2 to 0.7 px fills the first pixel");
    const EdgeCover::Interval insideFar = snap(1279.3, 1279.8);
    CHECK_MSG(insideFar.lo == 1279.0 && insideFar.hi == 1280.0, "and one 1279.3 to 1279.8 the last");
    const EdgeCover::Interval neither = snap(0.6, 0.9);
    CHECK_MSG(neither.lo == 0.6 && neither.hi == 0.9, "a picture 0.6 to 0.9 px covers no centre and is left (it is culled)");
    // The first half is what it was without the complement, and the complement adds only the inward ends.
    EdgeCover::Rules without = rules;
    without.complement = false;
    CHECK(EdgeCover::Snap(without, {-500.0, 0.7}, frame).hi == 0.7);
    CHECK(EdgeCover::Snap(without, {1279.3, 1500.0}, frame).lo == 1279.3);
    CHECK(EdgeCover::Snap(without, {0.3, 600.0}, frame).lo == 0.0);
    CHECK(EdgeCover::Snap(without, {600.0, 1279.7}, frame).hi == frame);
    CHECK(EdgeCover::Snap(without, {0.2, 0.7}, frame).lo == 0.0 && EdgeCover::Snap(without, {0.2, 0.7}, frame).hi == 0.7);
    // A frame of another size.
    const EdgeCover::Interval wide = EdgeCover::Snap(rules, {-5.0, 0.7}, 1920.0);
    CHECK(wide.hi == 1.0);
    const EdgeCover::Interval farWide = EdgeCover::Snap(rules, {1919.3, 3000.0}, 1920.0);
    CHECK_MSG(farWide.lo == 1919.0, "a 1920 px frame's last pixel is its own");
    // A band of a quarter pixel moves the centre: an edge at 0.3 covers it, one at 0.2 does not.
    EdgeCover::Rules narrow = rules;
    narrow.bandPx = 0.25;
    CHECK(EdgeCover::Snap(narrow, {-500.0, 0.3}, frame).hi == 1.0);
    CHECK(EdgeCover::Snap(narrow, {-500.0, 0.2}, frame).hi == 0.2);
}

void APictureThatCoversNoPixelCentreOfTheFrameIsLeftOut() {
    const EdgeCover::Rules rules = TheRules();
    const double w = 1280.0;
    const double h = 720.0;
    using EdgeCover::Rect;
    const auto culled = [&](double xlo, double xhi, double ylo, double yhi) {
        return EdgeCover::Culled(rules, Rect{{xlo, xhi}, {ylo, yhi}}, w, h);
    };
    // A bar across the whole frame whose inward edge is in the first row: left out while its edge is
    // short of the row's centre, drawn (and given a strip, not a cull) past it.
    CHECK_MSG(culled(-100.0, 1400.0, -500.0, 0.4), "a top bar ending 0.4 px down");
    CHECK_MSG(culled(-100.0, 1400.0, -500.0, 0.13), "or 0.13");
    CHECK_MSG(culled(-100.0, 1400.0, -500.0, 0.5), "or exactly on the centre (a high edge's centre is not drawn)");
    CHECK_MSG(!culled(-100.0, 1400.0, -500.0, 0.5001), "past the centre it is drawn");
    CHECK_MSG(!culled(-100.0, 1400.0, -500.0, 0.9), "and at 0.9");
    CHECK_MSG(!culled(-100.0, 1400.0, -500.0, 1.4), "and past the first row");
    CHECK_MSG(!culled(-100.0, 1400.0, -500.0, 0.0), "a bar that shows nothing of itself is not culled: there is nothing to leave");
    CHECK_MSG(!culled(-100.0, 1400.0, -500.0, -3.0), "nor one wholly outside");
    // The bottom, mirrored: the last row's centre is 719.5, and a low edge at it is drawn.
    CHECK_MSG(culled(-100.0, 1400.0, 719.6, 1200.0), "a bottom bar from 719.6");
    CHECK_MSG(culled(-100.0, 1400.0, 719.99, 1200.0), "or from 719.99");
    CHECK_MSG(!culled(-100.0, 1400.0, 719.5, 1200.0), "from the centre exactly it is drawn");
    CHECK_MSG(!culled(-100.0, 1400.0, 719.4, 1200.0), "and from 719.4");
    CHECK_MSG(!culled(-100.0, 1400.0, 720.0, 1200.0), "from the frame's own edge it shows nothing");
    // The sides.
    CHECK_MSG(culled(-50.0, 0.3, 100.0, 200.0), "a picture ending 0.3 px into the first column");
    CHECK_MSG(!culled(-50.0, 0.7, 100.0, 200.0), "0.7 px, and it is drawn");
    CHECK_MSG(culled(1279.8, 1500.0, 100.0, 200.0), "one starting 1279.8");
    CHECK_MSG(!culled(1279.3, 1500.0, 100.0, 200.0), "1279.3, and it is drawn");
    // A corner, and a picture inside one pixel.
    CHECK_MSG(culled(-5.0, 0.3, -5.0, 0.3), "a picture in the corner pixel that covers no centre");
    CHECK_MSG(culled(0.2, 0.4, 100.0, 200.0), "a picture 0.2 to 0.4 px wide in the first column");
    CHECK_MSG(!culled(0.2, 0.7, 100.0, 200.0), "0.2 to 0.7 covers the centre and is drawn out with strips");
    CHECK_MSG(culled(1279.55, 1279.9, 100.0, 200.0), "and one in the last column past its centre");
    // What has anything of it past the outer pixel is never culled: an edge inside the frame is the
    // MSAA blend it always was, and a sliver beside more of a picture is not the cull's to take.
    CHECK_MSG(!culled(0.7, 500.0, 100.0, 200.0), "a picture that begins 0.7 px in and goes on");
    CHECK_MSG(!culled(100.1, 100.4, 100.0, 200.0), "a hairline inside the frame");
    CHECK_MSG(!culled(100.0, 600.0, 100.0, 200.0), "a picture inside the frame");
    CHECK_MSG(!culled(-10.0, 1400.0, -10.0, 800.0), "a picture across the whole frame");
    CHECK_MSG(!culled(5.0, 5.0, 0.0, 100.0), "an empty one");
    // Another frame's size.
    CHECK_MSG(EdgeCover::Culled(rules, Rect{{-100.0, 3000.0}, {1079.6, 2000.0}}, 1920.0, 1080.0), "a 1080 px frame's last row");
    // The switch.
    EdgeCover::Rules without = rules;
    without.complement = false;
    CHECK_MSG(!EdgeCover::Culled(without, Rect{{-100.0, 1400.0}, {-500.0, 0.4}}, w, h), "without the complement, nothing is culled");
    CHECK_MSG(!EdgeCover::Culled(rules, Rect{{-100.0, 1400.0}, {-500.0, 0.4}}, 0.0, h), "and no frame has nothing to leave out of");
}

void ASliverBesideMoreOfAPictureIsNamed() {
    const EdgeCover::Rules rules = TheRules();
    const double w = 1280.0;
    const double h = 720.0;
    using EdgeCover::Rect;
    using EdgeCover::Side;
    const auto slivers = [&](double xlo, double xhi, double ylo, double yhi) {
        return EdgeCover::SliversOf(rules, Rect{{xlo, xhi}, {ylo, yhi}}, w, h);
    };
    // The left edge in the first column's inner half, the picture going on: 0.5 to 0.875 (a sample
    // exactly on a low edge is drawn).
    const double leftYes[] = {0.5001, 0.6, 0.75, 0.875};
    for (const double lo : leftYes) {
        CHECK_MSG(slivers(lo, 500.0, 100.0, 200.0).side[static_cast<int>(Side::Left)], "a left edge at " + std::to_string(lo));
    }
    const double leftNo[] = {0.0, 0.3, 0.5, 0.876, 1.0, 1.5};
    for (const double lo : leftNo) {
        CHECK_MSG(!slivers(lo, 500.0, 100.0, 200.0).Any(), "no sliver at a left edge of " + std::to_string(lo));
    }
    // The right side: the high edge in the last column's inner half, past its first sample at 1279.125 and
    // at or before the centre at 1279.5.
    const double rightYes[] = {1279.1251, 1279.25, 1279.4, 1279.5};
    for (const double hi : rightYes) {
        CHECK_MSG(slivers(600.0, hi, 100.0, 200.0).side[static_cast<int>(Side::Right)], "a right edge at " + std::to_string(hi));
    }
    const double rightNo[] = {1279.125, 1279.0, 1279.5001, 1279.9, 1280.0};
    for (const double hi : rightNo) {
        CHECK_MSG(!slivers(600.0, hi, 100.0, 200.0).Any(), "no sliver at a right edge of " + std::to_string(hi));
    }
    // Top and bottom, and two at once.
    CHECK(slivers(100.0, 600.0, 0.7, 300.0).side[static_cast<int>(Side::Top)]);
    CHECK(slivers(100.0, 600.0, 400.0, 719.3).side[static_cast<int>(Side::Bottom)]);
    const EdgeCover::Slivers both = slivers(0.7, 1279.3, 100.0, 200.0);
    CHECK(both.side[static_cast<int>(Side::Left)] && both.side[static_cast<int>(Side::Right)] && !both.side[2] && !both.side[3]);
    // A picture that ends inside the pixel is the cull's or the strips', not a sliver; one off the frame is nothing.
    CHECK_MSG(!slivers(0.6, 0.9, 100.0, 200.0).Any(), "a picture wholly in the pixel is not one that goes on");
    CHECK_MSG(!slivers(0.7, 500.0, 900.0, 1000.0).Any(), "and one whose rows the frame does not show");
    CHECK_MSG(!slivers(-500.0, -10.0, 100.0, 200.0).Any(), "or whose columns it does not");
    CHECK_MSG(!EdgeCover::SliversOf(rules, Rect{{0.7, 500.0}, {100.0, 200.0}}, 0.0, h).Any(), "no frame, no sliver");
}

void AnInwardEdgeGetsTheStripsOfItsPixel() {
    const EdgeCover::Rules rules = TheRules();
    const double w = 1280.0;
    const double h = 720.0;
    using EdgeCover::Rect;
    // A top bar: one strip below it, 0.7 to 1, over its columns cut a pixel beyond the frame.
    const EdgeCover::Strips top = EdgeCover::StripsOf(rules, Rect{{-100.0, 1400.0}, {-500.0, 0.7}}, w, h);
    CHECK_EQ(top.count, static_cast<std::size_t>(1));
    if (top.count == 1) {
        CHECK(top.strip[0].side == EdgeCover::Side::Bottom);
        CHECK(Same(top.strip[0].rect.y.lo, 0.7) && Same(top.strip[0].rect.y.hi, 1.0));
        CHECK(Same(top.strip[0].rect.x.lo, -1.0) && Same(top.strip[0].rect.x.hi, 1281.0));
    }
    // A bottom bar: one above it, 719 to 719.3.
    const EdgeCover::Strips bottom = EdgeCover::StripsOf(rules, Rect{{-100.0, 1400.0}, {719.3, 1200.0}}, w, h);
    CHECK_EQ(bottom.count, static_cast<std::size_t>(1));
    if (bottom.count == 1) {
        CHECK(bottom.strip[0].side == EdgeCover::Side::Top);
        CHECK(Same(bottom.strip[0].rect.y.lo, 719.0) && Same(bottom.strip[0].rect.y.hi, 719.3));
    }
    // A picture that comes in from the left: its strip is beside it, at the frame's side, over its rows.
    const EdgeCover::Strips left = EdgeCover::StripsOf(rules, Rect{{-50.0, 0.7}, {100.0, 200.0}}, w, h);
    CHECK_EQ(left.count, static_cast<std::size_t>(1));
    if (left.count == 1) {
        CHECK(left.strip[0].side == EdgeCover::Side::Right);
        CHECK(Same(left.strip[0].rect.x.lo, 0.7) && Same(left.strip[0].rect.x.hi, 1.0));
        CHECK(Same(left.strip[0].rect.y.lo, 100.0) && Same(left.strip[0].rect.y.hi, 200.0));
    }
    const EdgeCover::Strips right = EdgeCover::StripsOf(rules, Rect{{1279.3, 1500.0}, {100.0, 200.0}}, w, h);
    CHECK_EQ(right.count, static_cast<std::size_t>(1));
    if (right.count == 1) {
        CHECK(right.strip[0].side == EdgeCover::Side::Left);
        CHECK(Same(right.strip[0].rect.x.lo, 1279.0) && Same(right.strip[0].rect.x.hi, 1279.3));
    }
    // A picture inside one pixel: a strip at each end, none overlapping.
    const EdgeCover::Strips narrow = EdgeCover::StripsOf(rules, Rect{{0.2, 0.7}, {100.0, 200.0}}, w, h);
    CHECK_EQ(narrow.count, static_cast<std::size_t>(2));
    // A bar that is wholly past the frame, or ends at the pixel's first sample or its far boundary, needs none.
    CHECK_EQ(EdgeCover::StripsOf(rules, Rect{{-100.0, 1400.0}, {-500.0, 0.4}}, w, h).count, static_cast<std::size_t>(0));
    CHECK_EQ(EdgeCover::StripsOf(rules, Rect{{-100.0, 1400.0}, {-500.0, 0.9}}, w, h).count, static_cast<std::size_t>(0));
    CHECK_EQ(EdgeCover::StripsOf(rules, Rect{{-100.0, 1400.0}, {-500.0, 1.4}}, w, h).count, static_cast<std::size_t>(0));
    CHECK_EQ(EdgeCover::StripsOf(rules, Rect{{-100.0, 1400.0}, {-500.0, -2.0}}, w, h).count, static_cast<std::size_t>(0));
    // And without the complement the inward edge is left, as the first build left it.
    EdgeCover::Rules without = rules;
    without.complement = false;
    CHECK_EQ(EdgeCover::StripsOf(without, Rect{{-100.0, 1400.0}, {-500.0, 0.7}}, w, h).count, static_cast<std::size_t>(0));
    CHECK_EQ(EdgeCover::StripsOf(without, Rect{{-50.0, 0.7}, {100.0, 200.0}}, w, h).count, static_cast<std::size_t>(0));
    // The picture, its strips and the rest of the pixel are the extended rectangle: an inward edge on each of four sides.
    const Rect pictures[] = {
        {{-100.0, 1400.0}, {-500.0, 0.7}},
        {{-100.0, 1400.0}, {719.3, 1200.0}},
        {{-50.0, 0.7}, {100.0, 200.0}},
        {{1279.3, 1500.0}, {100.0, 200.0}},
        {{0.2, 0.7}, {0.2, 0.7}},
        {{-100.0, 0.8}, {-100.0, 0.9}},
    };
    for (const Rect& picture : pictures) {
        const EdgeCover::Strips s = EdgeCover::StripsOf(rules, picture, w, h);
        const EdgeCover::Interval ex = EdgeCover::Snap(rules, picture.x, w);
        const EdgeCover::Interval ey = EdgeCover::Snap(rules, picture.y, h);
        // Both the picture and the extended rectangle are cut to a pixel beyond the frame, where nothing is drawn.
        const auto cut = [](const EdgeCover::Interval& a, double frame) {
            return EdgeCover::Interval{std::max(a.lo, -1.0), std::min(a.hi, frame + 1.0)};
        };
        const Rect cutPicture{cut(picture.x, w), cut(picture.y, h)};
        const Rect extended{cut(ex, w), cut(ey, h)};
        double total = Area(cutPicture);
        bool apart = true;
        for (std::size_t i = 0; i < s.count; ++i) {
            total += Area(s.strip[i].rect);
            apart = apart && Overlap(s.strip[i].rect, cutPicture) == 0.0;
            for (std::size_t j = i + 1; j < s.count; ++j) apart = apart && Overlap(s.strip[i].rect, s.strip[j].rect) == 0.0;
        }
        CHECK_MSG(apart, "no strip overlaps the picture or another, picture y " + std::to_string(picture.y.lo) + ".." +
                             std::to_string(picture.y.hi));
        CHECK_MSG(std::fabs(total - Area(extended)) < 1e-6,
                  "the picture and its strips are the extended rectangle: " + std::to_string(total) + " against " +
                      std::to_string(Area(extended)));
    }
}

} // namespace

MP_SUITE_MAIN("test_mp_edgecover") {
    TheRulesAreEdgeCoverJsons();
    TheSnapIsThePixelCentreRuleAtEveryBoundary();
    StripsFillExactlyWhatTheRuleDraws();
    AStripsUvIsThePicturesOwnMapCarriedOn();
    TheInwardEdgeGoesOutToThePixelsBoundary();
    APictureThatCoversNoPixelCentreOfTheFrameIsLeftOut();
    ASliverBesideMoreOfAPictureIsNamed();
    AnInwardEdgeGetsTheStripsOfItsPixel();
    return ::test::summary("test_mp_edgecover", 150);
}
