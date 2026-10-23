// What the modern controls' default layout costs the levels: the census (docs/planning/2026-10-15-modern-controls.md section 1.9, Appendix A;
// planning step 180).
//
// The question is the one the first design failed: of the 128 recorded winning routes, which has a shot that no camera the level allows can
// bring clear of a control, so that a control's hit rectangle would swallow a shot the route needs? The first design's rectangles blocked 25
// of 110 gravity levels where the classic discs block none; the layout that ships, chosen from this census, blocks two for the pair.
//
// HOW IT WORKS (the design's own python, out/parity/program/CTL/arith2/final_layout.py, deadzone_census.py, census2.py, census_aspects.py):
//   * a route's shot taps are points of the level (every tap whose `shot` is not false; the golden's `inputs` count them, on all 128 routes and on
//     the levels that have gravity); the level's bounds are its `max` marker;
//   * the camera's corner is centred when the level is no larger than the view and otherwise anywhere in [0, bounds - view] (Camera::Clamp);
//     the tap is on the view at tap - corner, which the player can park no nearer than EM units to the view's edge (16 here);
//   * a tap is BLOCKED by a rectangle when every camera the clamp allows puts it inside (a closed comparison, as the python's), the
//     rectangle shrunk by `slack` on its sides inside the view (0 here: a player may aim a few units off a recorded point);
//   * the pair is blocked only on gravity levels (a weightless level has no pair); the corner buttons are counted on all 128.
// The rectangles are the module's own: Controls::Boxes and Controls::HitRects on controls.json read from the port's data (MP_TEST_PORTDATA
// when a mutation test sets it), so a data change moves the census. They are checked against a port of the python's arithmetic written out
// below with ITS OWN literals, never read from the data, so a change in the data cannot move both.
//
// THE ORACLE IS DATA: tests/golden/controls_census.json holds the accepted list, per window shape, and the design's tables (the 54 counts of
// growth, slack and edge margin; the first pass's 25 and 37; the size sweep; the corner buttons; the classic discs' none; the spawn
// estimate), written by the design's python at the exact window widths. A level that joins the list fails this suite and so does one that
// leaves it; a correction to the golden is a file and not a relink. THE LIST IS VALID ONLY FOR THE LAYOUT IT NAMES (`valid_for`), in
// particular for hit.grow_px = 0 on every control (the owner's answer to Q0): if controls.json changes any of those numbers, the first
// check that fails says that the census is to be re-run and the list signed again by the owner before the list is relied on again (the default became modern in planning step 198, CTL-13).
//
// Reads tests/routes/*.json and the converted levels (the gitignored out/levels and out/data/chapters.json); without them it exits 77.

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "Route.hpp"

#include "core/Json.hpp"

#include "sim/Camera.hpp"
#include "sim/Chapters.hpp"
#include "sim/Controls.hpp"
#include "sim/Hud.hpp"
#include "sim/Roles.hpp"
#include "sim/Tscn.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <utility>
#include <vector>

using namespace MagicPortals;
namespace Json = Supersonic::Json;

namespace {

const std::string kLevels = MAGICPORTALS_LEVELS_DIR;
const std::string kChapters = MAGICPORTALS_CHAPTERS_FILE;
const std::string kRoutes = MAGICPORTALS_ROUTES_DIR;
const std::string kGoldenFile = std::string(MAGICPORTALS_GOLDEN_DIR) + "/controls_census.json";

// The port's own data: MP_TEST_PORTDATA when it is set (TEST ONLY: a mutation test runs this same binary on a copy of game/data with one
// number changed), else the build's own. _dupenv_s: std::getenv is C4996 on Windows.
std::string PortDataDir() {
    std::string text;
#ifdef _WIN32
    char* value = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&value, &length, "MP_TEST_PORTDATA") == 0 && value != nullptr) {
        text = value;
        std::free(value);
    }
#else
    if (const char* value = std::getenv("MP_TEST_PORTDATA"); value != nullptr) text = value;
#endif
    if (text.empty()) return MAGICPORTALS_PORT_DATA_DIR;
    std::printf("test_mp_census: MUTATION - MP_TEST_PORTDATA is set, the port's data is read from %s, not from game/data\n", text.c_str());
    return text;
}

const std::string kData = PortDataDir();

constexpr double kHeight = 256.0;
constexpr double kTol = 1e-9;

using Labels = std::vector<std::string>; // sorted: a set of levels, as "1-20"

Controls::Rules g_rules;
Hud::Rules g_hud;
Json::Value g_golden;

// ---- what the census reads of the levels and the routes ------------------------------------------------------------------------

struct Level {
    std::string label; // world-level as the player counts them, "1-09"
    bool noGravity = false;
    glm::dvec2 bounds{0.0};
    glm::dvec2 spawn{0.0};
    std::vector<glm::dvec2> taps; // every tap whose shot is not false
};
std::vector<Level> g_levels;

bool Position(const Tscn::Node& node, glm::dvec2& out) {
    const Tscn::Value* value = node.Find("position");
    if (value == nullptr || value->kind != Tscn::Value::Kind::Vector2 || value->numbers.size() != 2) return false;
    out = glm::dvec2(value->numbers[0], value->numbers[1]);
    return true;
}

// The routes, in file-name order, each with its level's bounds (the last `max` marker), spawn (the first node that is the main character, with a
// position) and weightlessness (a `no_gravity` of 1), as the python reads them.
bool LoadFacts(std::string& error) {
    Chapters::Table table;
    if (!Chapters::Load(kChapters, table, error)) return false;
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(kRoutes)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") files.push_back(entry.path());
    }
    std::sort(files.begin(), files.end());
    for (const std::filesystem::path& file : files) {
        Route::Plan plan;
        if (!Route::Load(file.string(), plan, error)) return false;
        const int at = table.Find(plan.level);
        if (at < 0) {
            error = file.string() + ": " + plan.level + " is not in chapters.json";
            return false;
        }
        Level level;
        char label[32];
        std::snprintf(label, sizeof(label), "%d-%02d", table.levels[static_cast<std::size_t>(at)].world + 1, table.levels[static_cast<std::size_t>(at)].index + 1);
        level.label = label;
        Tscn::Scene scene;
        if (!Tscn::Load(kLevels + "/" + plan.level + ".tscn", scene, error)) return false;
        bool hasBounds = false;
        bool hasSpawn = false;
        for (const Tscn::Node& node : scene.nodes) {
            glm::dvec2 position(0.0);
            const bool placed = Position(node, position);
            const std::string entity = Roles::EntityName(node);
            if (placed && entity == "max") {
                level.bounds = position;
                hasBounds = true;
            }
            if (placed && !hasSpawn && (node.name.find("main_char") != std::string::npos || entity.find("main_char") != std::string::npos)) {
                level.spawn = position;
                hasSpawn = true;
            }
            double weightless = 0.0;
            const Tscn::Value* meta = node.Meta("no_gravity");
            if (meta != nullptr && meta->AsNumber(weightless) && weightless == 1.0) level.noGravity = true;
        }
        if (!hasBounds || !hasSpawn) {
            error = plan.level + ": no `max` marker or no main character with a position";
            return false;
        }
        for (const Route::Tap& tap : plan.taps) {
            if (tap.shot) level.taps.push_back(tap.atPx);
        }
        g_levels.push_back(std::move(level));
    }
    return true;
}

// ---- the census's own geometry (a port of the python's, closed comparisons and all) -----------------------------------------

struct Zone {
    double x0 = 0.0;
    double y0 = 0.0;
    double x1 = 0.0;
    double y1 = 0.0;
};

Zone ZoneOf(const Hud::Rect& rect) {
    return Zone{rect.min.x, rect.min.y, rect.Max().x, rect.Max().y};
}

// Only the sides inside the view are shrunk: a side on the view's edge has nothing beyond it to aim at.
Zone Shrink(const Zone& zone, double slack, const glm::dvec2& view) {
    return Zone{zone.x0 + (zone.x0 > 0.001 ? slack : 0.0), zone.y0 + (zone.y0 > 0.001 ? slack : 0.0),
                zone.x1 - (zone.x1 < view.x - 0.001 ? slack : 0.0), zone.y1 - (zone.y1 < view.y - 0.001 ? slack : 0.0)};
}

struct Corner {
    glm::dvec2 lo{0.0};
    glm::dvec2 hi{0.0};
};

// Where the camera's top-left corner can be: centred when the view is no larger... when the level is no larger than the view, else anywhere
// the clamp allows.
Corner CornerRange(const glm::dvec2& bounds, const glm::dvec2& view) {
    Corner corner;
    for (int axis = 0; axis < 2; ++axis) {
        if (view[axis] >= bounds[axis]) {
            corner.lo[axis] = corner.hi[axis] = (bounds[axis] - view[axis]) / 2.0;
        } else {
            corner.lo[axis] = 0.0;
            corner.hi[axis] = bounds[axis] - view[axis];
        }
    }
    return corner;
}

// Where on the view a tap can be put by any camera, `edge` units in from the view's edge at the nearest.
struct Region {
    glm::dvec2 lo{0.0};
    glm::dvec2 hi{0.0};
    bool ok = false;
};

Region Visible(const Level& level, const glm::dvec2& tap, const glm::dvec2& view, double edge) {
    const Corner corner = CornerRange(level.bounds, view);
    Region region;
    region.lo = glm::dvec2(std::max(tap.x - corner.hi.x, edge), std::max(tap.y - corner.hi.y, edge));
    region.hi = glm::dvec2(std::min(tap.x - corner.lo.x, view.x - edge), std::min(tap.y - corner.lo.y, view.y - edge));
    region.ok = region.lo.x <= region.hi.x && region.lo.y <= region.hi.y;
    return region;
}

bool ForcedIn(const Zone& zone, const Region& region) {
    return region.ok && zone.x0 <= region.lo.x && region.hi.x <= zone.x1 && zone.y0 <= region.lo.y && region.hi.y <= zone.y1;
}

bool ForcedInDisc(const glm::dvec2& centre, double radius, const Region& region) {
    if (!region.ok) return false;
    const double fx = std::max(std::fabs(region.lo.x - centre.x), std::fabs(region.hi.x - centre.x));
    const double fy = std::max(std::fabs(region.lo.y - centre.y), std::fabs(region.hi.y - centre.y));
    return std::hypot(fx, fy) < radius;
}

// The levels with a tap that every camera puts inside a zone, sorted.
template <typename Predicate>
Labels Blocked(const glm::dvec2& view, double em, bool gravityOnly, Predicate&& inside) {
    std::set<std::string> found;
    for (const Level& level : g_levels) {
        if (gravityOnly && level.noGravity) continue;
        for (const glm::dvec2& tap : level.taps) {
            if (inside(Visible(level, tap, view, em))) {
                found.insert(level.label);
                break;
            }
        }
    }
    return Labels(found.begin(), found.end());
}

Labels BlockedBy(const Zone& zone, const glm::dvec2& view, double em, bool gravityOnly) {
    return Blocked(view, em, gravityOnly, [&](const Region& region) { return ForcedIn(zone, region); });
}

// ---- the module's rectangles ---------------------------------------------------------------------------------------------

constexpr std::size_t Ix(Controls::Id id) {
    return static_cast<std::size_t>(id);
}

struct Placed {
    std::array<Hud::Rect, 5> boxes;
    std::array<Hud::Rect, 5> hits;
};

Placed Place(const Controls::Rules& rules, const Controls::Tuning& tuning, const glm::dvec2& view) {
    Placed placed;
    placed.boxes = Controls::Boxes(rules, tuning, view, Controls::Insets{});
    placed.hits = Controls::HitRects(rules, placed.boxes, view, Controls::Insets{});
    return placed;
}

// The two arrows' hit rectangles as ONE zone, which is exact when they tile (the same top, the same bottom, one ending where the other
// begins: the seam is split at its middle). The census assumes they do; a data change that makes them stop is counted and fails the suite.
int g_pairNotTiling = 0;

Zone PairZone(const Placed& placed) {
    const Hud::Rect& left = placed.hits[Ix(Controls::Id::Left)];
    const Hud::Rect& right = placed.hits[Ix(Controls::Id::Right)];
    Zone zone{std::min(left.min.x, right.min.x), std::min(left.min.y, right.min.y), std::max(left.Max().x, right.Max().x),
              std::max(left.Max().y, right.Max().y)};
    const bool tiles = std::fabs(left.Max().x - right.min.x) <= kTol && std::fabs(left.min.y - right.min.y) <= kTol &&
                       std::fabs(left.Max().y - right.Max().y) <= kTol;
    if (!tiles) ++g_pairNotTiling;
    return zone;
}

glm::dvec2 ViewOf(int a, int b) {
    return glm::dvec2(kHeight * static_cast<double>(a) / static_cast<double>(b), kHeight);
}
const glm::dvec2 kView169 = ViewOf(16, 9);

// A look: the data with the pair's growth set to `growPx` on both arrows and every other control's to nothing, a preset and a size.
Controls::Rules WithPairGrowth(const Controls::Rules& rules, double growPx) {
    Controls::Rules edited = rules;
    edited.hit.growPx = {};
    edited.hit.growPx[Ix(Controls::Id::Left)] = growPx;
    edited.hit.growPx[Ix(Controls::Id::Right)] = growPx;
    return edited;
}

Controls::Tuning Look(Controls::Arrows arrows, Controls::Preset preset, std::optional<int> sizeSteps = std::nullopt) {
    Controls::Tuning tuning;
    tuning.arrows = arrows;
    tuning.preset = preset;
    tuning.sizeSteps = sizeSteps;
    return tuning;
}

// The two arrangements of the walk arrows the golden records (planning step 186): `split` ships, `grouped` is planning step 180's, kept as an alternative.
constexpr const auto& kArrangements = Controls::kArrangements; // the module's list (it was a copy here, planning step 191)

// What the walk arrows' hit rectangles are, as the zones the census judges: grouped, ONE (the two tile into a single rectangle, which the census
// assumes and counts when it does not); split, TWO, far apart, each its own. The names are the golden's keys.
struct WalkZone {
    std::string name; // "pair", or "walk_left" / "walk_right"
    Zone zone;
};

std::vector<WalkZone> WalkZones(const Placed& placed, Controls::Arrows arrows) {
    if (arrows == Controls::Arrows::Grouped) return {WalkZone{"pair", PairZone(placed)}};
    return {WalkZone{"walk_left", ZoneOf(placed.hits[Ix(Controls::Id::Left)])}, WalkZone{"walk_right", ZoneOf(placed.hits[Ix(Controls::Id::Right)])}};
}

Labels UnionOf(const std::vector<Labels>& lists) {
    std::set<std::string> found;
    for (const Labels& list : lists) found.insert(list.begin(), list.end());
    return Labels(found.begin(), found.end());
}

// The levels any of the walk zones blocks: the pair's list, or the union of the two arrows'.
Labels BlockedByWalk(const Placed& placed, Controls::Arrows arrows, const glm::dvec2& view, double em, double slack) {
    std::vector<Labels> lists;
    for (const WalkZone& walk : WalkZones(placed, arrows)) lists.push_back(BlockedBy(Shrink(walk.zone, slack, view), view, em, true));
    return UnionOf(lists);
}

// ---- the golden, read strictly enough that a missing key is a failure and not a quiet default -------------------------------

const Json::Value& Key(const Json::Value& from, const char* key, std::string_view where) { // by value: a const std::string& made every call "possibly dangling" to g++ 13
    CHECK_MSG(from.Has(key), "the golden has no \"" + std::string(key) + "\" in " + std::string(where));
    return from[key];
}

const char* ArrangementName(Controls::Arrows arrows) {
    return arrows == Controls::Arrows::Split ? "split" : "grouped";
}

// The golden's block of one arrangement: `arrangements.<name>`, which holds the shapes, the 16:9 zones, the growth table, the size sweep and the spawn
// estimate for that arrangement of the walk arrows.
const Json::Value& Arrangement(Controls::Arrows arrows) {
    return Key(Key(g_golden, "arrangements", "the top level"), ArrangementName(arrows), "arrangements");
}

Labels LabelsOf(const Json::Value& array) {
    Labels labels;
    for (const Json::Value& item : array.AsArray()) labels.push_back(item.AsString());
    std::sort(labels.begin(), labels.end());
    return labels;
}

std::string Join(const Labels& labels) {
    std::string text = "{";
    for (std::size_t i = 0; i < labels.size(); ++i) text += (i == 0 ? "" : ", ") + labels[i];
    return text + "}";
}

// What a list gained and what it lost against the accepted one: the two readings of "a level joined" and "a level left".
std::string Difference(const Labels& computed, const Labels& golden) {
    Labels joined;
    Labels left;
    std::set_difference(computed.begin(), computed.end(), golden.begin(), golden.end(), std::back_inserter(joined));
    std::set_difference(golden.begin(), golden.end(), computed.begin(), computed.end(), std::back_inserter(left));
    return "computed " + Join(computed) + ", accepted " + Join(golden) + " (joined " + Join(joined) + ", left " + Join(left) + ")";
}

void CheckList(const std::string& what, const Labels& computed, const Labels& golden) {
    CHECK_MSG(computed == golden, what + ": " + Difference(computed, golden) +
                                       ". The census is re-run and the owner signs the list again (design 1.9; CTL-13 made modern the default in planning step 198), then tests/golden/controls_census.json follows.");
}

std::array<double, 4> FourOf(const Json::Value& array) {
    std::array<double, 4> numbers{};
    const Json::Array& items = array.AsArray();
    CHECK_EQ(items.size(), std::size_t{4});
    for (std::size_t i = 0; i < 4 && i < items.size(); ++i) numbers[i] = items[i].AsNumber(std::nan(""));
    return numbers;
}

std::string Num(double value) {
    char text[48];
    std::snprintf(text, sizeof(text), "%.10g", value);
    return text;
}

// ---- the design's arithmetic, written out with its own literals ------------------------------------------------------------
//
// final_layout.py's `places` and `hits`, line for line. None of these numbers is read from controls.json: they are what the design says, and
// the module is held to them.
namespace Design {

constexpr double kPpu = 2.8125; // px to a unit at 1280 x 720
constexpr double kMargin = 44.0 / kPpu;
constexpr double kGap = 32.0 / kPpu;
constexpr double kBox = 128.0 / kPpu;
constexpr double kCorner = 0.9 * 128.0 / kPpu;
constexpr double kSeam = 32.0 / kPpu + 1e-9;
constexpr double kReach = 48.0 / kPpu;

struct Look {
    double size;
    double bottomPx;
};
// uniform, balanced, mr
constexpr std::array<Look, 3> kLooks = {{{0.7, 44.0}, {0.8, 34.0}, {1.0, 11.0}}};
constexpr std::array<Controls::Preset, 3> kPresetOf = {Controls::Preset::Uniform, Controls::Preset::Balanced, Controls::Preset::Mr};

using Box = std::array<double, 4>; // left, top, right, bottom
using Boxes = std::array<Box, 5>;  // by Controls::Id: walk_left, walk_right, restart, pause, clear

Boxes Places(double width, const Look& look, Controls::Arrows arrows) {
    const double arrow = kBox * look.size;
    const double corner = kCorner * std::max(look.size, 1.0);
    const double bottom = look.bottomPx / kPpu;
    Boxes boxes{};
    boxes[Ix(Controls::Id::Left)] = {kMargin, kHeight - bottom - arrow, kMargin + arrow, kHeight - bottom};
    // Split (planning step 186): walk_right in the bottom-RIGHT corner, inset by the same margins; grouped: beside walk_left, one gap away.
    if (arrows == Controls::Arrows::Split) {
        boxes[Ix(Controls::Id::Right)] = {width - kMargin - arrow, kHeight - bottom - arrow, width - kMargin, kHeight - bottom};
    } else {
        boxes[Ix(Controls::Id::Right)] = {kMargin + arrow + kGap, kHeight - bottom - arrow, kMargin + arrow + kGap + arrow, kHeight - bottom};
    }
    boxes[Ix(Controls::Id::Menu)] = {width - kMargin - corner, kMargin, width - kMargin, kMargin + corner};
    boxes[Ix(Controls::Id::Reset)] = {width - kMargin - corner - kGap - corner, kMargin, width - kMargin - corner - kGap, kMargin + corner};
    boxes[Ix(Controls::Id::Clear)] = {kMargin, kMargin, kMargin + corner, kMargin + corner};
    return boxes;
}

// `growPx` is the pair's growth on all four sides of both arrows, as the python grows 'walk_left' and 'walk_right' by growPx / PPU.
Boxes Hits(double width, const Boxes& boxes, double growPx) {
    Boxes out{};
    for (std::size_t n = 0; n < 5; ++n) {
        const double growth = (n == Ix(Controls::Id::Left) || n == Ix(Controls::Id::Right)) ? growPx / kPpu : 0.0;
        const double bl = boxes[n][0];
        const double bt = boxes[n][1];
        const double br = boxes[n][2];
        const double bb = boxes[n][3];
        double l = bl - growth;
        double t = bt - growth;
        double r = br + growth;
        double b = bb + growth;
        for (std::size_t m = 0; m < 5; ++m) {
            if (m == n) continue;
            const double ol = boxes[m][0];
            const double ot = boxes[m][1];
            const double orr = boxes[m][2];
            const double ob = boxes[m][3];
            if (std::min(bb, ob) > std::max(bt, ot)) {
                if (orr <= bl && bl - orr <= kSeam) l = bl - (bl - orr) / 2.0;
                if (ol >= br && ol - br <= kSeam) r = br + (ol - br) / 2.0;
            }
            if (std::min(br, orr) > std::max(bl, ol)) {
                if (ob <= bt && bt - ob <= kSeam) t = bt - (bt - ob) / 2.0;
                if (ot >= bb && ot - bb <= kSeam) b = bb + (ot - bb) / 2.0;
            }
        }
        if (bl <= kReach) l = 0.0;
        if (bt <= kReach) t = 0.0;
        if (width - br <= kReach) r = width;
        if (kHeight - bb <= kReach) b = kHeight;
        out[n] = {l, t, r, b};
    }
    return out;
}

// deadzone_census.py's first-pass `pair_rect`: the pair as ONE rectangle, `e` of growth all round, snapped to the view's edge when within `snap`.
Zone FirstPassPair(double size, double e, double snap) {
    const double arrow = kBox * size;
    std::array<double, 4> r = {kMargin - e, kHeight - kMargin - arrow - e, kMargin + arrow + kGap + arrow + e, kHeight - kMargin + e};
    if (r[0] < snap) r[0] = 0.0;
    if (kHeight - r[3] < snap) r[3] = kHeight;
    return Zone{r[0], r[1], r[2], r[3]};
}

} // namespace Design

// ---- 1: what the census is fed -----------------------------------------------------------------------------------------

void TheCensusReadsWhatItAssumes() {
    const Json::Value& inputs = Key(g_golden, "inputs", "the top level");
    // What the census is fed is the golden's own `inputs` (written by the design's python from the same routes and levels), and nothing of it is
    // written out here a second time: a re-authored route changes the number of shot taps, and that is a file edit, not a relink. What stays in
    // code is the game's own count of levels (CLAUDE.md pins 128), which a census that quietly read fewer routes could not match.
    constexpr std::size_t kLevelsInTheGame = 128;
    CHECK_EQ(g_levels.size(), kLevelsInTheGame);
    std::size_t taps = 0;
    std::size_t gravityTaps = 0;
    std::size_t gravityLevels = 0;
    Labels weightless;
    for (const Level& level : g_levels) {
        taps += level.taps.size();
        if (level.noGravity) {
            weightless.push_back(level.label);
        } else {
            gravityTaps += level.taps.size();
            ++gravityLevels;
        }
    }
    std::sort(weightless.begin(), weightless.end());
    // What the golden says the python read.
    CHECK_EQ(static_cast<double>(g_levels.size()), Key(inputs, "routes", "inputs").AsNumber());
    CHECK_EQ(static_cast<double>(taps), Key(inputs, "shot_taps", "inputs").AsNumber());
    CHECK_EQ(static_cast<double>(gravityTaps), Key(inputs, "shot_taps_on_gravity_levels", "inputs").AsNumber());
    CHECK_EQ(static_cast<double>(gravityLevels), Key(inputs, "gravity_levels", "inputs").AsNumber());
    CHECK_EQ(Key(inputs, "levels_with_bounds", "inputs").AsNumber(), Key(inputs, "routes", "inputs").AsNumber());
    CHECK_EQ(Key(inputs, "levels_with_a_spawn", "inputs").AsNumber(), Key(inputs, "routes", "inputs").AsNumber());
    CheckList("the weightless levels", weightless, LabelsOf(Key(inputs, "weightless_levels", "inputs")));
    // What holds whatever the routes say, so that an empty list cannot pass for want of input: there are shot taps to judge, on levels with gravity
    // (the pair is judged only there) and on weightless ones (the corner buttons are judged everywhere), and the golden's own levels add up.
    CHECK_MSG(taps > 0 && gravityTaps > 0, "the census has shot taps to judge: " + std::to_string(taps) + ", " + std::to_string(gravityTaps) + " of them on levels with gravity");
    CHECK_MSG(gravityLevels > 0 && !weightless.empty(), std::to_string(gravityLevels) + " levels with gravity and " + std::to_string(weightless.size()) + " weightless");
    CHECK_MSG(Key(inputs, "gravity_levels", "inputs").AsNumber() + static_cast<double>(Key(inputs, "weightless_levels", "inputs").AsArray().size()) ==
                  Key(inputs, "routes", "inputs").AsNumber(),
              "the golden's levels with gravity and its weightless ones are its routes");
    // Every level is one the census can judge: a spawn and every shot tap lie inside its bounds (the level's pixels), so a tap is a point a camera can
    // be put on and a bound is not a marker read off the wrong node.
    for (const Level& level : g_levels) {
        bool inside = level.bounds.x > 0.0 && level.bounds.y > 0.0 && level.spawn.x >= 0.0 && level.spawn.y >= 0.0 && level.spawn.x <= level.bounds.x &&
                      level.spawn.y <= level.bounds.y;
        for (const glm::dvec2& tap : level.taps) {
            inside = inside && tap.x >= 0.0 && tap.y >= 0.0 && tap.x <= level.bounds.x && tap.y <= level.bounds.y;
        }
        CHECK_MSG(inside, level.label + ": its spawn and each shot tap lie inside its bounds");
    }
    // Levels the camera centres on an axis and levels it scrolls on it, on both axes, or the camera model would be tested on one side only (at 16:9
    // it centres most levels vertically and scrolls some; at 32:9 it centres most horizontally and scrolls a few). The counts are the levels' own and
    // not the routes', so they are not pinned here beyond that each side is met.
    const glm::dvec2 wide = ViewOf(32, 9);
    int centredY = 0;
    int scrollsY = 0;
    int centredX = 0;
    int scrollsX = 0;
    for (const Level& level : g_levels) {
        (level.bounds.y <= kView169.y ? centredY : scrollsY) += 1;
        (level.bounds.x <= wide.x ? centredX : scrollsX) += 1;
    }
    CHECK_MSG(centredY > 0 && scrollsY > 0, "at 16:9 some levels are centred vertically and some scroll: " + std::to_string(centredY) + " and " + std::to_string(scrollsY));
    CHECK_MSG(centredX > 0 && scrollsX > 0, "at 32:9 some levels are centred horizontally and some scroll: " + std::to_string(centredX) + " and " + std::to_string(scrollsX));
}

void TheCameraModelIsTheLayersClamp() {
    // The census's corner range is Camera::Clamp's, whatever the camera wants: the centre it clamps to, less half the view.
    for (const glm::dvec2& view : {kView169, ViewOf(4, 3), ViewOf(32, 9)}) {
        int apart = 0;
        for (const Level& level : g_levels) {
            const Corner mine = CornerRange(level.bounds, view);
            const double big = 1e9;
            const glm::dvec2 lo = Camera::Clamp(glm::dvec2(-big), view, level.bounds) - view * 0.5;
            const glm::dvec2 hi = Camera::Clamp(glm::dvec2(big), view, level.bounds) - view * 0.5;
            if (std::fabs(lo.x - mine.lo.x) > kTol || std::fabs(lo.y - mine.lo.y) > kTol || std::fabs(hi.x - mine.hi.x) > kTol ||
                std::fabs(hi.y - mine.hi.y) > kTol) {
                ++apart;
            }
        }
        CHECK_MSG(apart == 0, std::to_string(apart) + " levels' camera corner ranges differ from Camera::Clamp's at view " + Num(view.x));
    }
}

// ---- 2: the module's rectangles are the design's arithmetic --------------------------------------------------------------

void TheModulesRectanglesAreTheDesignsArithmetic() {
    constexpr std::array<std::pair<int, int>, 9> shapes = {{{5, 4}, {4, 3}, {3, 2}, {16, 10}, {16, 9}, {18, 9}, {20, 9}, {21, 9}, {32, 9}}};
    for (const Controls::Arrows arrows : kArrangements) {
        for (const auto& [a, b] : shapes) {
            const glm::dvec2 view = ViewOf(a, b);
            for (std::size_t look = 0; look < Design::kLooks.size(); ++look) {
                for (const double growPx : {0.0, 16.0}) {
                    const Design::Boxes boxes = Design::Places(view.x, Design::kLooks[look], arrows);
                    const Design::Boxes hits = Design::Hits(view.x, boxes, growPx);
                    // Growth is the module's own copy of the data, edited: the python grows both arrows by growPx.
                    const Placed placed = Place(WithPairGrowth(g_rules, growPx), Look(arrows, Design::kPresetOf[look]), view);
                    std::string wrong;
                    for (const Controls::Id id : Controls::kClaimOrder) {
                        const Hud::Rect& box = placed.boxes[Ix(id)];
                        const Hud::Rect& hit = placed.hits[Ix(id)];
                        const std::array<double, 4> mineBox = {box.min.x, box.min.y, box.Max().x, box.Max().y};
                        const std::array<double, 4> mineHit = {hit.min.x, hit.min.y, hit.Max().x, hit.Max().y};
                        for (std::size_t edge = 0; edge < 4; ++edge) {
                            if (std::fabs(mineBox[edge] - boxes[Ix(id)][edge]) > kTol) {
                                wrong += std::string(" ") + Controls::KeyOf(id) + " box[" + std::to_string(edge) + "] " + Num(mineBox[edge]) + " vs " + Num(boxes[Ix(id)][edge]);
                            }
                            if (std::fabs(mineHit[edge] - hits[Ix(id)][edge]) > kTol) {
                                wrong += std::string(" ") + Controls::KeyOf(id) + " hit[" + std::to_string(edge) + "] " + Num(mineHit[edge]) + " vs " + Num(hits[Ix(id)][edge]);
                            }
                        }
                    }
                    CHECK_MSG(wrong.empty(), std::string(ArrangementName(arrows)) + " " + std::to_string(a) + ":" + std::to_string(b) + " " +
                                                 Controls::NameOf(Design::kPresetOf[look]) + " growth " + Num(growPx) + " px:" + wrong);
                }
            }
        }
    }
}

void TheZonesTheCensusConsumesAreTheDesignsTableAt16x9() {
    for (const Controls::Arrows arrows : kArrangements) {
        const std::string which = std::string(ArrangementName(arrows)) + ": ";
        const Json::Value& zones = Key(Arrangement(arrows), "zones_16x9", ArrangementName(arrows));
        const Placed placed = Place(g_rules, Look(arrows, g_rules.defaultPreset), kView169);
        for (const Controls::Id id : Controls::kClaimOrder) {
            const Json::Value& entry = Key(zones, Controls::KeyOf(id), "zones_16x9");
            const std::array<double, 4> box = FourOf(Key(entry, "box", Controls::KeyOf(id)));
            const std::array<double, 4> hit = FourOf(Key(entry, "hit", Controls::KeyOf(id)));
            const Zone mineBox = ZoneOf(placed.boxes[Ix(id)]);
            const Zone mineHit = ZoneOf(placed.hits[Ix(id)]);
            CHECK_MSG(std::fabs(mineBox.x0 - box[0]) <= kTol && std::fabs(mineBox.y0 - box[1]) <= kTol && std::fabs(mineBox.x1 - box[2]) <= kTol &&
                          std::fabs(mineBox.y1 - box[3]) <= kTol,
                      which + Controls::KeyOf(id) + " box (" + Num(mineBox.x0) + ", " + Num(mineBox.y0) + ")-(" + Num(mineBox.x1) + ", " + Num(mineBox.y1) +
                          ") against the design's (" + Num(box[0]) + ", " + Num(box[1]) + ")-(" + Num(box[2]) + ", " + Num(box[3]) + ")");
            CHECK_MSG(std::fabs(mineHit.x0 - hit[0]) <= kTol && std::fabs(mineHit.y0 - hit[1]) <= kTol && std::fabs(mineHit.x1 - hit[2]) <= kTol &&
                          std::fabs(mineHit.y1 - hit[3]) <= kTol,
                      which + Controls::KeyOf(id) + " hit (" + Num(mineHit.x0) + ", " + Num(mineHit.y0) + ")-(" + Num(mineHit.x1) + ", " + Num(mineHit.y1) +
                          ") against the design's (" + Num(hit[0]) + ", " + Num(hit[1]) + ")-(" + Num(hit[2]) + ", " + Num(hit[3]) + ")");
        }
        // The headline numbers, to the four decimals the design prints: restart's and pause's meeting on x = 392.8178, clear's reaching the corner; and
        // the arrows. Grouped (design 1.2): the pair's hit (0, 208.4978)-(90.7378, 256) split at 53.1911. Split (planning step 186): the left arrow's
        // hit (0, 208.4978)-(47.5022, 256) and the right arrow's (407.6089, 208.4978)-(455.1111, 256).
        const Zone left = ZoneOf(placed.hits[Ix(Controls::Id::Left)]);
        const Zone right = ZoneOf(placed.hits[Ix(Controls::Id::Right)]);
        if (arrows == Controls::Arrows::Grouped) {
            CHECK(std::fabs(left.x0 - 0.0) <= 1e-4 && std::fabs(left.y0 - 208.4978) <= 1e-4 && std::fabs(left.x1 - 53.1911) <= 1e-4 && std::fabs(left.y1 - 256.0) <= 1e-4);
            CHECK(std::fabs(right.x0 - 53.1911) <= 1e-4 && std::fabs(right.x1 - 90.7378) <= 1e-4 && std::fabs(right.y1 - 256.0) <= 1e-4);
        } else {
            CHECK(std::fabs(left.x0 - 0.0) <= 1e-4 && std::fabs(left.y0 - 208.4978) <= 1e-4 && std::fabs(left.x1 - 47.5022) <= 1e-4 && std::fabs(left.y1 - 256.0) <= 1e-4);
            CHECK(std::fabs(right.x0 - 407.6089) <= 1e-4 && std::fabs(right.y0 - 208.4978) <= 1e-4 && std::fabs(right.x1 - kView169.x) <= kTol && std::fabs(right.y1 - 256.0) <= 1e-4);
        }
        const Zone restart = ZoneOf(placed.hits[Ix(Controls::Id::Reset)]);
        const Zone pause = ZoneOf(placed.hits[Ix(Controls::Id::Menu)]);
        CHECK(std::fabs(restart.x1 - 392.8178) <= 1e-4 && std::fabs(pause.x0 - 392.8178) <= 1e-4 && std::fabs(pause.x1 - kView169.x) <= kTol);
    }
}

// ---- 3: the accepted list --------------------------------------------------------------------------------------------------

void TheGoldenIsTakenForTheDataThatShips() {
    const Json::Value& valid = Key(g_golden, "valid_for", "the top level");
    const std::string hint = " - the census golden is valid ONLY for this layout (hit.grow_px 0 on every control, the owner's Q0): re-run the census and have the owner sign the list again";
    const auto same = [&](const char* what, double data, double golden) {
        CHECK_MSG(std::fabs(data - golden) <= 1e-12, std::string(what) + ": controls.json says " + Num(data) + ", the golden was taken at " + Num(golden) + hint);
    };
    const Controls::Preset preset = g_rules.defaultPreset;
    CHECK_MSG(std::string(Controls::NameOf(preset)) == Key(valid, "preset", "valid_for").AsString(), std::string("default preset ") + Controls::NameOf(preset) + hint);
    // The arrangement that ships, and the layout number it is (planning step 186): the golden records both arrangements, and says which one is the data's.
    CHECK_MSG(std::string(Controls::NameOf(g_rules.defaultArrows)) == Key(valid, "default_arrows", "valid_for").AsString(),
              std::string("default arrows ") + Controls::NameOf(g_rules.defaultArrows) + hint);
    same("layout.version", static_cast<double>(g_rules.layoutVersion), Key(valid, "layout_version", "valid_for").AsNumber());
    CHECK_MSG(g_rules.layoutVersion == Controls::kLayoutVersion, "the data's layout number is the code's");
    const Controls::Rules::PresetRule& look = g_rules.presets[static_cast<std::size_t>(preset)];
    same("the default preset's size", g_rules.size.ValueOf(look.sizeSteps), Key(valid, "size", "valid_for").AsNumber());
    const Json::Value& margin = Key(valid, "margin_px", "valid_for");
    same("margin_px.side", g_rules.marginSidePx, Key(margin, "side", "margin_px").AsNumber());
    same("margin_px.top", g_rules.marginTopPx, Key(margin, "top", "margin_px").AsNumber());
    same("the default preset's bottom margin", look.marginBottomPx, Key(margin, "bottom", "margin_px").AsNumber());
    same("gap_px", g_rules.gapPx, Key(valid, "gap_px", "valid_for").AsNumber());
    same("box_px", g_rules.boxPx, Key(valid, "box_px", "valid_for").AsNumber());
    same("corner_box_factor", g_rules.cornerBoxFactor, Key(valid, "corner_box_factor", "valid_for").AsNumber());
    same("corner_min_size", g_rules.cornerMinSize, Key(valid, "corner_min_size", "valid_for").AsNumber());
    const Json::Value& hit = Key(valid, "hit", "valid_for");
    same("hit.seam_px", g_rules.hit.seamPx, Key(hit, "seam_px", "hit").AsNumber());
    same("hit.edge_reach_px", g_rules.hit.edgeReachPx, Key(hit, "edge_reach_px", "hit").AsNumber());
    const Json::Value& growth = Key(hit, "grow_px", "hit");
    for (const Controls::Id id : Controls::kClaimOrder) {
        same((std::string("hit.grow_px.") + Controls::KeyOf(id)).c_str(), g_rules.hit.growPx[Ix(id)], Key(growth, Controls::KeyOf(id), "grow_px").AsNumber());
        // And the golden itself says nothing but nought: a list taken at any growth is not the list the owner is asked to sign.
        CHECK_MSG(Key(growth, Controls::KeyOf(id), "grow_px").AsNumber() == 0.0, std::string("the golden's growth for ") + Controls::KeyOf(id) + " is not 0");
    }
    const Json::Value& census = Key(valid, "census", "valid_for");
    same("census.em_units", g_rules.census.emUnits, Key(census, "em_units", "census").AsNumber());
    same("census.slack_units", g_rules.census.slackUnits, Key(census, "slack_units", "census").AsNumber());
    // Both arrangements are in the golden, each under its own name.
    for (const Controls::Arrows arrows : kArrangements) CHECK_MSG(Arrangement(arrows).IsObject(), std::string("the golden has the ") + ArrangementName(arrows) + " arrangement");
}

void TheCensusAtTheDefaultIsTheAcceptedList() {
    // The levels the walk arrows cost, over the nine shapes, by arrangement: the finding of planning step 186 in one place. GROUPED costs 1-20 and 3-27
    // (the recorded taps at the very bottom-left of a level whose camera cannot scroll left of 0, which any inset pair of 0.7 or more reaches); SPLIT costs
    // none: the left arrow ends at 47.5, under those taps' 72 and 80, and no recorded shot is forced into the bottom-right corner.
    std::map<std::string, Labels> walkAcrossShapes[2];
    for (const Controls::Arrows arrows : kArrangements) {
        const std::string which = std::string(ArrangementName(arrows)) + ": ";
        const Json::Value& block = Arrangement(arrows);
        const Json::Array& shapes = Key(block, "shapes", ArrangementName(arrows)).AsArray();
        CHECK_EQ(shapes.size(), std::size_t{9});
        Labels restartEverywhere;
        Labels clearEverywhere;
        Labels pauseEverywhere;
        // The same unions of the ACCEPTED lists, from the golden: the design's words ("the pair costs 1-20 and 3-27 and no more", "pause nothing") are
        // the golden's, so they are written once, in the file the owner signs, and not again here.
        Labels restartAccepted;
        Labels clearAccepted;
        Labels pauseAccepted;
        std::map<std::string, Labels> walkEverywhere;
        std::map<std::string, Labels> walkAccepted;
        const auto append = [](Labels& into, const Labels& more) { into.insert(into.end(), more.begin(), more.end()); };
        for (const Json::Value& shape : shapes) {
            const std::string name = which + Key(shape, "name", "a shape").AsString();
            const Json::Array& aspect = Key(shape, "aspect", name).AsArray();
            CHECK_EQ(aspect.size(), std::size_t{2});
            if (aspect.size() != 2) continue;
            const glm::dvec2 view = ViewOf(static_cast<int>(aspect[0].AsNumber()), static_cast<int>(aspect[1].AsNumber()));
            const Placed placed = Place(g_rules, Look(arrows, g_rules.defaultPreset), view);
            const double slack = g_rules.census.slackUnits;
            const std::vector<WalkZone> walks = WalkZones(placed, arrows);
            const Zone restart = ZoneOf(placed.hits[Ix(Controls::Id::Reset)]);
            const Zone pause = ZoneOf(placed.hits[Ix(Controls::Id::Menu)]);
            const Zone clear = ZoneOf(placed.hits[Ix(Controls::Id::Clear)]);
            for (const auto& [key, em] : {std::pair<const char*, double>{"blocked", g_rules.census.emUnits}, std::pair<const char*, double>{"blocked_em0", 0.0}}) {
                const Json::Value& accepted = Key(shape, key, name);
                const Labels r = BlockedBy(Shrink(restart, slack, view), view, em, false);
                const Labels m = BlockedBy(Shrink(pause, slack, view), view, em, false);
                const Labels c = BlockedBy(Shrink(clear, slack, view), view, em, false);
                for (const WalkZone& walk : walks) {
                    const Labels w = BlockedBy(Shrink(walk.zone, slack, view), view, em, true);
                    CheckList(name + " " + key + " " + walk.name, w, LabelsOf(Key(accepted, walk.name.c_str(), name)));
                    if (std::string(key) == "blocked") {
                        append(walkEverywhere[walk.name], w);
                        append(walkAccepted[walk.name], LabelsOf(Key(accepted, walk.name.c_str(), name)));
                    }
                }
                CheckList(name + " " + key + " restart", r, LabelsOf(Key(accepted, "restart", name)));
                CheckList(name + " " + key + " pause", m, LabelsOf(Key(accepted, "pause", name)));
                CheckList(name + " " + key + " clear_portals", c, LabelsOf(Key(accepted, "clear_portals", name)));
                if (std::string(key) == "blocked") {
                    append(restartEverywhere, r);
                    append(pauseEverywhere, m);
                    append(clearEverywhere, c);
                    append(restartAccepted, LabelsOf(Key(accepted, "restart", name)));
                    append(pauseAccepted, LabelsOf(Key(accepted, "pause", name)));
                    append(clearAccepted, LabelsOf(Key(accepted, "clear_portals", name)));
                }
            }
            // A shot a player may aim 4 units off the recorded point: the walk arrows' lists do not change.
            if (arrows == Controls::Arrows::Grouped) {
                CheckList(name + " pair at slack 4", BlockedBy(Shrink(walks[0].zone, 4.0, view), view, g_rules.census.emUnits, true), LabelsOf(Key(shape, "blocked_pair_slack4", name)));
            } else {
                const Json::Value& slack4 = Key(shape, "blocked_walk_slack4", name);
                for (const WalkZone& walk : walks) {
                    CheckList(name + " " + walk.name + " at slack 4", BlockedBy(Shrink(walk.zone, 4.0, view), view, g_rules.census.emUnits, true), LabelsOf(Key(slack4, walk.name.c_str(), name)));
                }
            }
        }
        // The shapes together: the levels each control costs on any of the nine, against the union of the accepted lists. (Each shape's list is held
        // to the golden above; this is the union of the same, so a shape that gained a level another lost cannot hide in a sum.)
        const auto unique = [](Labels labels) {
            std::sort(labels.begin(), labels.end());
            labels.erase(std::unique(labels.begin(), labels.end()), labels.end());
            return labels;
        };
        for (const auto& [walkName, found] : walkEverywhere) CheckList(which + walkName + ", over the nine shapes", unique(found), unique(walkAccepted[walkName]));
        CheckList(which + "restart, over the nine shapes", unique(restartEverywhere), unique(restartAccepted));
        CheckList(which + "clear-portals, over the nine shapes", unique(clearEverywhere), unique(clearAccepted));
        CheckList(which + "pause, over the nine shapes", unique(pauseEverywhere), unique(pauseAccepted));
        for (const auto& [walkName, found] : walkEverywhere) walkAcrossShapes[arrows == Controls::Arrows::Split ? 0 : 1][walkName] = unique(found);

        // The headline at 16:9, the shape the owner will see most, at the EM and the slack the golden was taken at WHATEVER controls.json says, from
        // the module's rectangles: the accepted list for that shape is the golden's.
        const Json::Value& valid = Key(g_golden, "valid_for", "the top level");
        const double designEm = Key(Key(valid, "census", "valid_for"), "em_units", "census").AsNumber();
        bool saw169 = false;
        for (const Json::Value& shape : shapes) {
            if (Key(shape, "name", "a shape").AsString() != "16:9") continue;
            saw169 = true;
            const Placed placed = Place(g_rules, Look(arrows, g_rules.defaultPreset), kView169);
            const Json::Value& accepted = Key(shape, "blocked", "16:9");
            Labels firstWalkList;
            for (const WalkZone& walk : WalkZones(placed, arrows)) {
                const Labels listed = LabelsOf(Key(accepted, walk.name.c_str(), "16:9"));
                CheckList(which + "16:9 " + walk.name, BlockedBy(walk.zone, kView169, designEm, true), listed);
                if (firstWalkList.empty()) firstWalkList = listed;
            }
            CheckList(which + "16:9 restart", BlockedBy(ZoneOf(placed.hits[Ix(Controls::Id::Reset)]), kView169, designEm, false), LabelsOf(Key(accepted, "restart", "16:9")));
            CheckList(which + "16:9 pause", BlockedBy(ZoneOf(placed.hits[Ix(Controls::Id::Menu)]), kView169, designEm, false), LabelsOf(Key(accepted, "pause", "16:9")));
            CheckList(which + "16:9 clear-portals", BlockedBy(ZoneOf(placed.hits[Ix(Controls::Id::Clear)]), kView169, designEm, false), LabelsOf(Key(accepted, "clear_portals", "16:9")));

            // The comparison can fail both ways, and its message says which: a level added to an accepted list, and one taken out, are told apart in the
            // words the owner reads ("joined", "left"). A level that is no level (9-99 sorts after every real one) cannot be one of the list's own.
            const Labels pairList = LabelsOf(Key(accepted, "restart", "16:9")); // a list that is non-empty at 16:9 in both arrangements (restart: 2-06)
            Labels more = pairList;
            more.push_back("9-99");
            const std::string added = Difference(more, pairList);
            CHECK_MSG(added.find("joined {9-99}") != std::string::npos && added.find("left {}") != std::string::npos, "an added level is reported as joined: " + added);
            if (!pairList.empty()) {
                Labels fewer = pairList;
                const std::string gone = fewer.back();
                fewer.pop_back();
                const std::string removed = Difference(fewer, pairList);
                CHECK_MSG(removed.find("left {" + gone + "}") != std::string::npos && removed.find("joined {}") != std::string::npos, "a removed level is reported as left: " + removed);
            }
        }
        CHECK_MSG(saw169, which + "the golden has a shape named 16:9");
    }
    // THE FINDING, stated once: the grouped pair costs 1-20 and 3-27; the split arrows cost no level at any of the nine shapes.
    const Labels groupedCost = {"1-20", "3-27"};
    CHECK_MSG(walkAcrossShapes[1]["pair"] == groupedCost, "grouped: the pair costs 1-20 and 3-27 over the nine shapes: " + Join(walkAcrossShapes[1]["pair"]));
    CHECK_MSG(walkAcrossShapes[0]["walk_left"].empty() && walkAcrossShapes[0]["walk_right"].empty(),
              "split: neither arrow costs a level over the nine shapes (1-20 and 3-27 leave the list): " + Join(walkAcrossShapes[0]["walk_left"]) + " " + Join(walkAcrossShapes[0]["walk_right"]));
}

// ---- 4: the design's tables ---------------------------------------------------------------------------------------------

void TheCensusAgreesWithTheDesignsTables() {
    for (const Controls::Arrows arrows : kArrangements) {
        const Json::Value& block = Arrangement(arrows);
        const Json::Value& table = Key(Key(block, "growth_table", ArrangementName(arrows)), "counts", "growth_table");
        const Json::Value& lists = Key(block, "growth_16_lists", ArrangementName(arrows));
        for (std::size_t look = 0; look < Design::kLooks.size(); ++look) {
            const std::string preset = Controls::NameOf(Design::kPresetOf[look]);
            const std::string which = std::string(ArrangementName(arrows)) + " " + preset;
            const Json::Value& byGrowth = Key(table, preset.c_str(), "growth_table");
            for (const double growPx : {0.0, 16.0}) {
                const Json::Value& counts = Key(byGrowth, growPx == 0.0 ? "0" : "16", preset);
                const Placed placed = Place(WithPairGrowth(g_rules, growPx), Look(arrows, Design::kPresetOf[look]), kView169);
                const Json::Array& byEm = counts.AsArray();
                CHECK_EQ(byEm.size(), std::size_t{3});
                const std::array<double, 3> ems = {0.0, 8.0, 16.0};
                const std::array<double, 3> slacks = {0.0, 4.0, 8.0};
                for (std::size_t e = 0; e < 3 && e < byEm.size(); ++e) {
                    const Json::Array& bySlack = byEm[e].AsArray();
                    CHECK_EQ(bySlack.size(), std::size_t{3});
                    for (std::size_t s = 0; s < 3 && s < bySlack.size(); ++s) {
                        const std::size_t mine = BlockedByWalk(placed, arrows, kView169, ems[e], slacks[s]).size();
                        CHECK_MSG(static_cast<double>(mine) == bySlack[s].AsNumber(),
                                  which + " growth " + Num(growPx) + " px, EM " + Num(ems[e]) + ", slack " + Num(slacks[s]) + ": " + std::to_string(mine) + " levels, the design's " +
                                      Num(bySlack[s].AsNumber()));
                    }
                }
                if (growPx == 16.0) {
                    CheckList(which + " with 16 px of growth", BlockedByWalk(placed, arrows, kView169, 16.0, 0.0), LabelsOf(Key(lists, preset.c_str(), "growth_16_lists")));
                }
            }
        }
    }
}

void AGrowthOfSixteenPxCostsWhatQ0Says() {
    // The number the owner decides on (Q0/Q6): what 16 px of growth costs at each look against none, at EM 16 and slack 0. The design's text states
    // it for the GROUPED pair (8 levels at the uniform look, 14 at balanced, 16 at mr, against the 2 of no growth); the SPLIT arrows' numbers are the
    // golden's too (0 at the uniform look even with 16 px of growth, 4 at balanced, 10 at mr, against none of no growth): those numbers are written once,
    // in the golden's `growth_table`, which is read here. The shipped data has none, so none of this is in the game.
    for (const Controls::Arrows arrows : kArrangements) {
        const Json::Value& table = Key(Key(Arrangement(arrows), "growth_table", ArrangementName(arrows)), "counts", "growth_table");
        std::array<std::size_t, 3> grownCosts{};
        for (std::size_t look = 0; look < 3; ++look) {
            const std::string preset = Controls::NameOf(Design::kPresetOf[look]);
            const std::string which = std::string(ArrangementName(arrows)) + " " + preset;
            const Json::Value& byGrowth = Key(table, preset.c_str(), "growth_table");
            // [EM 0, 8, 16][slack 0, 4, 8]: EM 16 is the third, slack 0 the first.
            const auto cell = [&](const char* growth) { return Key(byGrowth, growth, preset).AsArray().at(2).AsArray().at(0).AsNumber(); };
            const Placed grown = Place(WithPairGrowth(g_rules, 16.0), Look(arrows, Design::kPresetOf[look]), kView169);
            const Placed plain = Place(WithPairGrowth(g_rules, 0.0), Look(arrows, Design::kPresetOf[look]), kView169);
            const std::size_t withGrowth = BlockedByWalk(grown, arrows, kView169, 16.0, 0.0).size();
            const std::size_t without = BlockedByWalk(plain, arrows, kView169, 16.0, 0.0).size();
            CHECK_MSG(static_cast<double>(withGrowth) == cell("16"), which + " with 16 px of growth blocks " + std::to_string(withGrowth) + " levels, the design's " + Num(cell("16")));
            CHECK_MSG(static_cast<double>(without) == cell("0"), which + " with no growth blocks " + std::to_string(without) + " levels, the design's " + Num(cell("0")));
            // What the owner decides on: growth costs levels, at every look. For the grouped pair strictly; for the split arrows at the uniform look it
            // costs none at all (the golden says 0 against 0), and at the others strictly.
            if (arrows == Controls::Arrows::Grouped || look != 0) {
                CHECK_MSG(withGrowth > without, which + ": 16 px of growth costs " + std::to_string(withGrowth) + " levels against " + std::to_string(without) + " without");
            } else {
                CHECK_MSG(withGrowth >= without, which + ": 16 px of growth costs " + std::to_string(withGrowth) + " levels against " + std::to_string(without) + " without");
            }
            grownCosts[look] = withGrowth;
        }
        // And the larger the look, the more it costs once any growth is asked for (the reason the look that ships is the uniform one).
        CHECK_MSG(grownCosts[0] <= grownCosts[1] && grownCosts[1] <= grownCosts[2], std::string(ArrangementName(arrows)) + ": with 16 px of growth the looks cost " +
                                                                                      std::to_string(grownCosts[0]) + ", " + std::to_string(grownCosts[1]) + " and " + std::to_string(grownCosts[2]) + " levels");
    }
}

void TheFirstPassPlacementWouldHaveFailed() {
    // The NEGATIVE CONTROL of the whole census: the review's placement (size 1.0 or 1.4, margin 44 px, 32 px of growth and a 16 px snap on the
    // pair as one rectangle) blocks 25 and 37 levels where the accepted layouts block 2 (grouped) and none (split) - so the census can say no, and it said
    // it of the first design.
    const Json::Value& pass = Key(g_golden, "first_pass", "the top level");
    const Json::Array& ems = Key(pass, "ems", "first_pass").AsArray();
    const double e32 = 32.0 / Design::kPpu;
    const double snap = 16.0 / Design::kPpu;
    for (const auto& [tag, size, steps] : {std::tuple<const char*, double, int>{"size_1_0", 1.0, 10}, std::tuple<const char*, double, int>{"size_1_4", 1.4, 14}}) {
        const Json::Value& row = Key(pass, tag, "first_pass");
        const Zone design = Design::FirstPassPair(size, e32, snap);
        const Zone boxOnly = Design::FirstPassPair(size, 0.0, 0.0);
        const std::array<double, 4> zone = FourOf(Key(row, "zone", tag));
        CHECK(std::fabs(design.x0 - zone[0]) <= kTol && std::fabs(design.y0 - zone[1]) <= kTol && std::fabs(design.x1 - zone[2]) <= kTol && std::fabs(design.y1 - zone[3]) <= kTol);
        const Json::Array& designCounts = Key(row, "design", tag).AsArray();
        const Json::Array& boxCounts = Key(row, "box_only", tag).AsArray();
        CHECK_EQ(designCounts.size(), ems.size());
        for (std::size_t i = 0; i < ems.size() && i < designCounts.size() && i < boxCounts.size(); ++i) {
            const std::size_t withGrowth = BlockedBy(design, kView169, ems[i].AsNumber(), true).size();
            const std::size_t boxAlone = BlockedBy(boxOnly, kView169, ems[i].AsNumber(), true).size();
            CHECK_MSG(static_cast<double>(withGrowth) == designCounts[i].AsNumber(), std::string(tag) + " first pass at EM " + Num(ems[i].AsNumber()) + ": " + std::to_string(withGrowth) + ", the design's " + Num(designCounts[i].AsNumber()));
            CHECK_MSG(static_cast<double>(boxAlone) == boxCounts[i].AsNumber(), std::string(tag) + " box alone at EM " + Num(ems[i].AsNumber()) + ": " + std::to_string(boxAlone) + ", the design's " + Num(boxCounts[i].AsNumber()));
        }
        // The headline, at the player's EM of 16 units: the design's text says 25 of 110 at size 1.0 and 37 at size 1.4, which the golden holds (its
        // `design` row at EM 16), against what the accepted layouts block.
        std::size_t at16 = ems.size();
        for (std::size_t i = 0; i < ems.size(); ++i) {
            if (ems[i].AsNumber() == 16.0) at16 = i;
        }
        const std::size_t headline = BlockedBy(design, kView169, 16.0, true).size();
        CHECK_MSG(at16 < designCounts.size(), std::string(tag) + ": the golden counts the first pass at EM 16");
        if (at16 < designCounts.size()) CHECK_EQ(static_cast<double>(headline), designCounts[at16].AsNumber());
        const std::size_t acceptedGrouped = BlockedBy(PairZone(Place(g_rules, Look(Controls::Arrows::Grouped, g_rules.defaultPreset), kView169)), kView169, 16.0, true).size();
        const std::size_t acceptedSplit = BlockedByWalk(Place(g_rules, Look(Controls::Arrows::Split, g_rules.defaultPreset), kView169), Controls::Arrows::Split, kView169, 16.0, 0.0).size();
        CHECK_MSG(headline > acceptedGrouped, std::string(tag) + ": the first pass blocks " + std::to_string(headline) + " levels and the grouped layout " + std::to_string(acceptedGrouped));
        CHECK_MSG(headline > acceptedSplit, std::string(tag) + ": the first pass blocks " + std::to_string(headline) + " levels and the split layout " + std::to_string(acceptedSplit));

        // The module reproduces it, with no snap of its own: the uniform look at the size, 32 px of growth on both arrows, and its edge reach (48
        // px) doing what the snap did. The pair it lays out (GROUPED: the first design's was a pair) is the same rectangle to a billionth.
        const Placed placed = Place(WithPairGrowth(g_rules, 32.0), Look(Controls::Arrows::Grouped, Controls::Preset::Uniform, steps), kView169);
        const Zone mine = PairZone(placed);
        CHECK_MSG(std::fabs(mine.x0 - design.x0) <= kTol && std::fabs(mine.y0 - design.y0) <= kTol && std::fabs(mine.x1 - design.x1) <= kTol && std::fabs(mine.y1 - design.y1) <= kTol,
                  std::string(tag) + " the module's pair (" + Num(mine.x0) + ", " + Num(mine.y0) + ")-(" + Num(mine.x1) + ", " + Num(mine.y1) + ") against the review's");
        CHECK_EQ(BlockedBy(mine, kView169, 16.0, true).size(), headline);
    }
}

void TheClassicDiscsBlockNothing() {
    // The positive control of the machinery: the original's pads (a disc of radius 64 about each bottom corner, taken from the classic module and
    // the original's own numbers) block no level at any EM, so the census can come out empty where the layout is gentle.
    const Json::Value& discs = Key(g_golden, "classic_discs", "the top level");
    const Json::Array& ems = Key(discs, "ems", "classic_discs").AsArray();
    const Json::Array& counts = Key(discs, "blocked", "classic_discs").AsArray();
    const Controls::Frame left = Controls::ClassicWalkFrame(g_hud, Controls::Id::Left, kView169, 5000.0);
    const Controls::Frame right = Controls::ClassicWalkFrame(g_hud, Controls::Id::Right, kView169, 5000.0);
    CHECK(left.hit.kind == Controls::HitShape::Kind::Disc && right.hit.kind == Controls::HitShape::Kind::Disc);
    CHECK(left.hit.radius == 64.0 && right.hit.radius == 64.0);
    CHECK(std::fabs(left.hit.centre.x) <= kTol && std::fabs(left.hit.centre.y - kHeight) <= kTol && std::fabs(right.hit.centre.x - kView169.x) <= kTol &&
          std::fabs(right.hit.centre.y - kHeight) <= kTol);
    CHECK_EQ(ems.size(), counts.size());
    for (std::size_t i = 0; i < ems.size() && i < counts.size(); ++i) {
        const Labels blocked = Blocked(kView169, ems[i].AsNumber(), true, [&](const Region& region) {
            return ForcedInDisc(left.hit.centre, left.hit.radius, region) || ForcedInDisc(right.hit.centre, right.hit.radius, region);
        });
        CHECK_MSG(blocked.empty() && counts[i].AsNumber() == 0.0, "the classic discs at EM " + Num(ems[i].AsNumber()) + " block " + Join(blocked));
    }
}

void ThePairIsBelowTheCharacterRowOrTheCensusFails() {
    // The arrows' top against the character's row (about 206: the standard waist line). GROUPED: at or below it costs the 2 levels whose recorded shot sits
    // in the corner whatever the size, above it the count climbs (7 at 0.8, 17 at 0.9, 20 at 1.0): so a size that sits above the row cannot get through this
    // suite without the golden being changed on purpose - the rule is a biconditional there, and it is kept. SPLIT (planning step 186): the arrows stand
    // in two corners, where they reach fewer recorded shots, so the biconditional is NOT true of them: size 0.8 puts the top at 203.95, above the row, and
    // still blocks no level, and 0.9 is the first size that blocks any (3). What holds, and is held here, is the implication that matters to the default:
    // at or below the row, no more than the 2 levels of the grouped layout (in fact none); and the golden's rows say the rest.
    const double row = g_rules.census.characterRowUnits;
    CHECK_EQ(row, 206.0);
    for (const Controls::Arrows arrows : kArrangements) {
        const Json::Array& rows = Key(Key(Arrangement(arrows), "size_sweep", ArrangementName(arrows)), "rows", "size_sweep").AsArray();
        CHECK_EQ(rows.size(), std::size_t{11});
        std::size_t previous = 0;
        for (const Json::Value& entry : rows) {
            const int steps = static_cast<int>(Key(entry, "size_steps", "a sweep row").AsNumber());
            const Placed placed = Place(WithPairGrowth(g_rules, 0.0), Look(arrows, Controls::Preset::Uniform, steps), kView169);
            const double top = placed.boxes[Ix(Controls::Id::Left)].min.y;
            const Labels blocked = BlockedByWalk(placed, arrows, kView169, 16.0, 0.0);
            const std::string where = std::string(ArrangementName(arrows)) + " size " + Num(Key(entry, "size", "a sweep row").AsNumber());
            CHECK_MSG(std::fabs(top - Key(entry, "pair_top", "a sweep row").AsNumber()) <= kTol, where + ": the arrows' top " + Num(top) + ", the design's " + Num(Key(entry, "pair_top", "a sweep row").AsNumber()));
            CheckList(where, blocked, LabelsOf(Key(entry, "blocked", "a sweep row")));
            if (arrows == Controls::Arrows::Grouped) {
                // The rule itself: at or below the row, no more than the 2 levels of the accepted layout; above it, more.
                CHECK_MSG((top >= row) == (blocked.size() <= 2), where + ": the pair's top is " + Num(top) + " against the row " + Num(row) + " and it blocks " + std::to_string(blocked.size()));
            } else {
                CHECK_MSG(top < row || blocked.size() <= 2, where + ": the arrows' top is " + Num(top) + " at or below the row " + Num(row) + " and they block " + std::to_string(blocked.size()));
                // The cost does not fall as the arrows grow: a bigger arrow reaches every shot a smaller one does (the hit rectangles nest).
                CHECK_MSG(blocked.size() >= previous, where + ": blocks " + std::to_string(blocked.size()) + " levels, fewer than the size before (" + std::to_string(previous) + ")");
            }
            previous = blocked.size();
        }
    }
    // The split golden says what the Step says: nothing is blocked through size 0.8, and 3 levels are from 0.9.
    {
        const Json::Array& rows = Key(Key(Arrangement(Controls::Arrows::Split), "size_sweep", "split"), "rows", "size_sweep").AsArray();
        for (const Json::Value& entry : rows) {
            const int steps = static_cast<int>(Key(entry, "size_steps", "a sweep row").AsNumber());
            const std::size_t listed = LabelsOf(Key(entry, "blocked", "a sweep row")).size();
            CHECK_MSG((steps <= 8) == (listed == 0), "split size " + Num(Key(entry, "size", "a sweep row").AsNumber()) + ": " + std::to_string(listed) + " levels (none through 0.8, some from 0.9)");
        }
    }
    // And the three looks that ship all sit at or below the row, in both arrangements.
    for (const Controls::Arrows arrows : kArrangements) {
        for (std::size_t look = 0; look < 3; ++look) {
            const Placed placed = Place(g_rules, Look(arrows, Design::kPresetOf[look]), kView169);
            CHECK_MSG(placed.boxes[Ix(Controls::Id::Left)].min.y >= row,
                      std::string(ArrangementName(arrows)) + " " + Controls::NameOf(Design::kPresetOf[look]) + " puts the arrows' top above the character's row");
        }
    }
}

void TheCornerButtonsCostWhatTheDesignSays() {
    const Json::Value& corners = Key(g_golden, "corners_16x9", "the top level");
    const Placed placed = Place(g_rules, Controls::Tuning{}, kView169);
    for (const Controls::Id id : {Controls::Id::Reset, Controls::Id::Menu, Controls::Id::Clear}) {
        const Json::Value& entry = Key(corners, Controls::KeyOf(id), "corners_16x9");
        const Zone zone = ZoneOf(placed.hits[Ix(id)]);
        for (const auto& [key, em, slack] : {std::tuple<const char*, double, double>{"em0_slack0", 0.0, 0.0}, std::tuple<const char*, double, double>{"em0_slack4", 0.0, 4.0},
                                              std::tuple<const char*, double, double>{"em16_slack0", 16.0, 0.0}, std::tuple<const char*, double, double>{"em16_slack4", 16.0, 4.0}}) {
            CheckList(std::string(Controls::KeyOf(id)) + " " + key, BlockedBy(Shrink(zone, slack, kView169), kView169, em, false), LabelsOf(Key(entry, key, Controls::KeyOf(id))));
        }
    }
    // The plan states in prose that restart costs one level and pause none; the levels are the golden's (read above), and what is held here is the
    // shape of it: pause costs no more than either of the other two.
    const std::size_t restartCost = BlockedBy(ZoneOf(placed.hits[Ix(Controls::Id::Reset)]), kView169, 16.0, false).size();
    const std::size_t pauseCost = BlockedBy(ZoneOf(placed.hits[Ix(Controls::Id::Menu)]), kView169, 16.0, false).size();
    const std::size_t clearCost = BlockedBy(ZoneOf(placed.hits[Ix(Controls::Id::Clear)]), kView169, 16.0, false).size();
    CHECK_MSG(pauseCost <= restartCost && pauseCost <= clearCost, "at 16:9, EM 16 pause costs " + std::to_string(pauseCost) + " levels, restart " + std::to_string(restartCost) + ", clear-portals " + std::to_string(clearCost));
}

void TheCharacterAtLevelStartBehindThePairIsTheDesignsEstimate() {
    // The character at level start behind the arrows' BOX (the spawn marker seen through the camera that opens on it). The design's text names the
    // levels (grouped: the uniform look 2-02 alone, the other two five), which the golden holds; for the split arrows the golden says 2-02 at every look
    // (the right arrow is in a corner no spawn marker is in). The reason the look that ships is the uniform one is that it costs least here, and that
    // is the relation held below.
    for (const Controls::Arrows arrows : kArrangements) {
        const Json::Value& behind = Key(Arrangement(arrows), "character_behind_the_pair", ArrangementName(arrows));
        std::array<std::size_t, 3> counts{};
        for (std::size_t look = 0; look < 3; ++look) {
            const Placed placed = Place(g_rules, Look(arrows, Design::kPresetOf[look]), kView169);
            const Hud::Rect& left = placed.boxes[Ix(Controls::Id::Left)];
            const Hud::Rect& right = placed.boxes[Ix(Controls::Id::Right)];
            std::set<std::string> found;
            for (const Level& level : g_levels) {
                if (level.noGravity) continue;
                const Corner corner = CornerRange(level.bounds, kView169);
                glm::dvec2 camera(0.0);
                for (int axis = 0; axis < 2; ++axis) {
                    camera[axis] = kView169[axis] >= level.bounds[axis] ? corner.lo[axis]
                                                                         : std::min(std::max(level.spawn[axis] - kView169[axis] / 2.0, 0.0), level.bounds[axis] - kView169[axis]);
                }
                const glm::dvec2 at = level.spawn - camera;
                const bool inLeft = left.min.x <= at.x && at.x <= left.Max().x && left.min.y <= at.y && at.y <= left.Max().y;
                const bool inRight = right.min.x <= at.x && at.x <= right.Max().x && right.min.y <= at.y && at.y <= right.Max().y;
                // Grouped: the pair as one box, the gap between the arrows included (the design's estimate); split: either box.
                const bool inPair = left.min.x <= at.x && at.x <= right.Max().x && left.min.y <= at.y && at.y <= left.Max().y;
                if (arrows == Controls::Arrows::Grouped ? inPair : (inLeft || inRight)) found.insert(level.label);
            }
            const Labels mine(found.begin(), found.end());
            const std::string preset = Controls::NameOf(Design::kPresetOf[look]);
            CheckList(std::string(ArrangementName(arrows)) + ": the character behind the arrows, " + preset, mine, LabelsOf(Key(behind, preset.c_str(), "character_behind_the_pair")));
            counts[look] = mine.size();
        }
        CHECK_MSG(counts[0] <= counts[1] && counts[0] <= counts[2], std::string(ArrangementName(arrows)) + ": the uniform look puts the character behind the arrows in " + std::to_string(counts[0]) +
                                                                      " levels, the others in " + std::to_string(counts[1]) + " and " + std::to_string(counts[2]));
    }
}

void ThePairAlwaysTilesWhereTheCensusAssumesIt() {
    CHECK_MSG(g_pairNotTiling == 0, std::to_string(g_pairNotTiling) + " times the two arrows' hit rectangles did not tile into one zone: the census would overstate what the pair claims");
}

void runTests() {
    TheCensusReadsWhatItAssumes();
    TheCameraModelIsTheLayersClamp();
    TheModulesRectanglesAreTheDesignsArithmetic();
    TheZonesTheCensusConsumesAreTheDesignsTableAt16x9();
    TheGoldenIsTakenForTheDataThatShips();
    TheCensusAtTheDefaultIsTheAcceptedList();
    TheCensusAgreesWithTheDesignsTables();
    AGrowthOfSixteenPxCostsWhatQ0Says();
    TheFirstPassPlacementWouldHaveFailed();
    TheClassicDiscsBlockNothing();
    ThePairIsBelowTheCharacterRowOrTheCensusFails();
    TheCornerButtonsCostWhatTheDesignSays();
    TheCharacterAtLevelStartBehindThePairIsTheDesignsEstimate();
    ThePairAlwaysTilesWhereTheCensusAssumesIt();
}

} // namespace

MP_SUITE_MAIN("test_mp_census") {
    std::error_code ec;
    if (!std::filesystem::is_directory(kLevels, ec) || !std::filesystem::is_regular_file(kChapters, ec) || !std::filesystem::is_directory(kRoutes, ec)) {
        std::printf("test_mp_census: SKIPPED - needs the converted levels at %s,\n"
                    "  chapters.json at %s and the routes at %s.\n"
                    "  The first two live outside this repository; configure with\n"
                    "  -DSUPERSONIC_MAGICPORTALS_LEVELS=... and -DSUPERSONIC_MAGICPORTALS_CHAPTERS=...\n",
                    kLevels.c_str(), kChapters.c_str(), kRoutes.c_str());
        return 77;
    }
    // The census's own inputs are committed, so a failure to read one is a failure and not a skip.
    std::string error;
    if (!Controls::LoadRules(kData + "/controls.json", g_rules, error)) {
        std::printf("test_mp_census: controls.json: %s\n", error.c_str());
        return 1;
    }
    if (!Hud::LoadRules(kData + "/ui.json", g_hud, error)) {
        std::printf("test_mp_census: ui.json: %s\n", error.c_str());
        return 1;
    }
    {
        std::ifstream file(kGoldenFile, std::ios::binary);
        std::ostringstream buffer;
        buffer << file.rdbuf();
        const std::string text = buffer.str(); // the parser keeps a reference to it
        Json::Parser parser(text);
        if (!file || !parser.Parse(g_golden) || !g_golden.IsObject()) {
            std::printf("test_mp_census: %s: %s\n", kGoldenFile.c_str(), file ? parser.Error().c_str() : "cannot open");
            return 1;
        }
    }
    if (!LoadFacts(error)) {
        std::printf("test_mp_census: %s\n", error.c_str());
        return 1;
    }
    runTests();
    return ::test::summary("test_mp_census", 40);
}
