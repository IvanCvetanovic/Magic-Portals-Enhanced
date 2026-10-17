// The adjustment screen: sim/Adjust, the `adjust` block of game/data/controls.json and the editor pictures sim/PadArt draws (planning step 192, CTL-11).
//
// An enhancement of this port's, not the original's, so what is pinned is the design (docs/planning/2026-10-15-modern-controls.md, section 3, with the two
// decisions made after it: the walk arrows SPLIT, planning step 186, and the Back button at the TOP LEFT under the clear-portals preview, the owner's of
// 2026-10-16), in literals written out here and never read back from the rules they check. The numbers of the clearance tables come from
// out/parity/program/CTL/CTL11/editor_clearance.py (Python arithmetic, not this code): GENERATED, below, by gen_adjust_literals.py.
//
// What is held: the seven widgets and their two rectangles each (a BOX, Magic Rampage's, and the FACE a tap hits) in units at every supported aspect; no
// two faces meet; the Back button is a fixed place that clears clear-portals' box by at least Magic Rampage's own 16 px at every size, aspect and inset;
// no default control is wholly under a widget, and the few that meet one partly are the ones the table records; a tile acts on the release STRICTLY inside it
// and never on an edge; a tile at its limit is inert; the size chain ends exactly on 0.4 and 1.4 and the opacity chain on 0.2 and 1.8; a size step re-locks
// and an opacity step does not; Restore is enabled only while a control is moved; the readouts are the value to one decimal; the unlocked cues are drawn from
// LayOut's own rectangles, and the outlines breathe in ticks; the preview is the level's own layout; and the block is read strictly.
//
// Pure: no window, no registry, no original art. ui.json and controls.json are the port's own and committed (from MP_TEST_PORTDATA when a mutation test sets
// it), so this runs anywhere and never skips. Its scratch files go to the temporary directory. What the screen does in the LAYER - the entry, the press, the
// save, the pictures through the tiers - is test_mp_layer's.

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "core/Json.hpp"

#include "sim/Adjust.hpp"
#include "sim/Controls.hpp"
#include "sim/Hud.hpp"
#include "sim/Options.hpp"
#include "sim/PadArt.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <initializer_list>
#include <optional>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

using namespace MagicPortals;

namespace {

// The port's own data: MP_TEST_PORTDATA when it is set (TEST ONLY: a mutation test runs this same binary on a copy of game/data), else the build's own.
// _dupenv_s: std::getenv is C4996 on Windows.
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
    std::printf("test_mp_adjust: MUTATION - MP_TEST_PORTDATA is set, the port's data is read from %s, not from game/data\n", text.c_str());
    return text;
}

const std::string kData = PortDataDir();
const std::string kUi = kData + "/ui.json";
const std::string kControls = kData + "/controls.json";

constexpr double kHeight = 256.0;     // the view's height, in units
constexpr double kPxPerUnit = 2.8125; // 720 px over 256 units
constexpr double kSettled = 1.0e9;

struct Aspect {
    const char* name;
    double ratio;
};
// The shapes the screen is held at: 5:4 is the narrowest supported window and 32:9 the widest (design 1.7).
const std::array<Aspect, 9> kAspects = {{{"5:4", 1.25},
                                         {"4:3", 4.0 / 3.0},
                                         {"3:2", 1.5},
                                         {"16:10", 1.6},
                                         {"16:9", 16.0 / 9.0},
                                         {"20:9", 20.0 / 9.0},
                                         {"21:9", 21.0 / 9.0},
                                         {"32:9", 32.0 / 9.0},
                                         {"2:1", 2.0}}};

glm::dvec2 ViewOf(double ratio) {
    return glm::dvec2(kHeight * ratio, kHeight);
}

bool Near(double a, double b, double eps = 1e-9) {
    return std::fabs(a - b) <= eps;
}

bool NearPoint(const glm::dvec2& a, const glm::dvec2& b, double eps = 1e-9) {
    return Near(a.x, b.x, eps) && Near(a.y, b.y, eps);
}

bool NearRect(const Hud::Rect& a, const Hud::Rect& b, double eps = 1e-9) {
    return NearPoint(a.min, b.min, eps) && NearPoint(a.size, b.size, eps);
}

std::string Num(double v) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.4f", v);
    return buffer;
}

std::string Show(const Hud::Rect& rect) {
    const glm::dvec2 max = rect.Max();
    return "(" + Num(rect.min.x) + ", " + Num(rect.min.y) + ")-(" + Num(max.x) + ", " + Num(max.y) + ")";
}

Hud::Rect RectOf(const double (&edges)[4]) {
    return Hud::Rect{glm::dvec2(edges[0], edges[1]), glm::dvec2(edges[2] - edges[0], edges[3] - edges[1])};
}

// The distance between two rectangles, as the Python arithmetic has it (editor_clearance.py): negative - minus the SMALLER overlap - where their interiors
// overlap, the distance between the nearest corners when they are apart on both axes, else the larger separation along one axis. `area` is the overlap's.
double Gap(const Hud::Rect& a, const Hud::Rect& c, double* area = nullptr) {
    const glm::dvec2 amax = a.Max();
    const glm::dvec2 cmax = c.Max();
    const double dx = std::max({c.min.x - amax.x, a.min.x - cmax.x, 0.0});
    const double dy = std::max({c.min.y - amax.y, a.min.y - cmax.y, 0.0});
    const double ox = std::min(amax.x, cmax.x) - std::max(a.min.x, c.min.x);
    const double oy = std::min(amax.y, cmax.y) - std::max(a.min.y, c.min.y);
    if (area != nullptr) *area = ox > 0.0 && oy > 0.0 ? ox * oy : 0.0;
    if (ox > 0.0 && oy > 0.0) return -std::min(ox, oy);
    return dx > 0.0 && dy > 0.0 ? std::hypot(dx, dy) : std::max(dx, dy);
}

bool WhollyInside(const Hud::Rect& a, const Hud::Rect& c) {
    const glm::dvec2 amax = a.Max();
    const glm::dvec2 cmax = c.Max();
    return a.min.x >= c.min.x && a.min.y >= c.min.y && amax.x <= cmax.x && amax.y <= cmax.y;
}

// Everything the cases read, loaded once.
struct Loaded {
    Controls::Rules controls;
    Options::Rules options;
    Adjust::Rules adjust;
    Hud::Rules hud;
    bool ok = false;
};

const Loaded& Data() {
    static const Loaded loaded = [] {
        Loaded l;
        std::string error;
        bool ok = Controls::LoadRules(kControls, l.controls, error);
        CHECK_MSG(ok, "controls.json reads: " + error);
        ok = ok && Options::LoadRules(kControls, l.controls, l.options, error);
        CHECK_MSG(ok, "and so does its options block: " + error);
        ok = ok && Adjust::LoadRules(kControls, l.controls, l.options, l.adjust, error);
        CHECK_MSG(ok, "and so does its adjust block: " + error);
        const bool ui = Hud::LoadRules(kUi, l.hud, error);
        CHECK_MSG(ui, "ui.json's hud reads: " + error);
        l.ok = ok && ui;
        return l;
    }();
    return loaded;
}

// The tuning of a size in tenths (7 is the default look's), nothing else changed.
Controls::Tuning SizedTuning(int steps) {
    Controls::Tuning tuning;
    tuning.sizeSteps = steps;
    return tuning.Clamped(Data().controls);
}

const Controls::Insets kNoInsets{};
// The safe areas the Back button's claims are made for (units; left, top, right, bottom): none; a landscape phone's notch and home bar; a status bar over a
// home bar; every side 20 units (56 px at 720p).
const std::array<Controls::Insets, 4> kInsetSets = {{{0, 0, 0, 0}, {21.3, 0, 21.3, 12.1}, {0, 9.0, 0, 12.0}, {20, 20, 20, 20}}};

// GENERATED by out/parity/program/CTL/CTL11/gen_adjust_literals.py from editor_clearance.py (Python arithmetic, not the code under test)
struct MeetRow {
    const char* aspect;
    double ratio;
    int firstBoxStep;      // the first size (in tenths) at which a default control BOX meets a widget BOX; 0: never
    double boxAreaFirst;   // the overlap area there, u^2
    double boxAreaLast;    // and at size 1.4
    int firstFaceStep;     // the first size at which a default control BOX meets a widget FACE; 0: never
    double faceAreaFirst;
    double tilesAt07;      // the least gap, a default control box to the six tiles (not Back), at size 0.7
    double tilesAt14;      // and at 1.4
    double allAt07;        // the least gap to any widget box, Back included, at size 0.7 (negative: they overlap)
};
const MeetRow kMeetRows[] = {
    {"5:4", 1.25, 4, 19.418074, 798.626551, 11, 30.253359, -3.413333, -22.072889, -3.413333},
    {"4:3", 1.3333333333333333, 11, 9.184749, 563.182402, 12, 35.137005, 7.253333, -22.072889, 7.253333},
    {"3:2", 1.5, 14, 92.294106, 92.294106, 0, 0.000000, 28.586667, -4.181333, 22.072889},
    {"16:10", 1.6000000000000001, 0, 0.000000, 0.000000, 0, 0.000000, 41.386667, 8.618667, 22.072889},
    {"16:9", 1.7777777777777777, 0, 0.000000, 0.000000, 0, 0.000000, 64.142222, 31.374222, 22.072889},
    {"20:9", 2.2222222222222223, 0, 0.000000, 0.000000, 0, 0.000000, 121.031111, 88.263111, 22.072889},
    {"21:9", 2.3333333333333335, 0, 0.000000, 0.000000, 0, 0.000000, 135.253333, 102.485333, 22.072889},
    {"32:9", 3.5555555555555554, 0, 0.000000, 0.000000, 0, 0.000000, 291.697778, 258.929778, 22.072889},
    {"2:1", 2, 0, 0.000000, 0.000000, 0, 0.000000, 92.586667, 59.818667, 22.072889},
};

// the widgets at 16:9 (W = 455.111111): box and face, in the order of Adjust::kWidgets
struct WidgetRow {
    const char* name;
    double box[4];
    double face[4];
};
const WidgetRow kWidgetRows169[] = {
    {"size_minus", {173.084444444, 50.915555556, 209.493333333, 87.324444444}, {178.204444444, 56.035555556, 204.373333333, 82.204444444}},
    {"size_plus", {245.617777778, 50.915555556, 282.026666667, 87.324444444}, {250.737777778, 56.035555556, 276.906666667, 82.204444444}},
    {"opacity_minus", {173.084444444, 84.195555556, 209.493333333, 120.604444444}, {178.204444444, 89.315555556, 204.373333333, 115.484444444}},
    {"opacity_plus", {245.617777778, 84.195555556, 282.026666667, 120.604444444}, {250.737777778, 89.315555556, 276.906666667, 115.484444444}},
    {"lock", {176.554666667, 121.031111111, 226.332444444, 170.808888889}, {183.554666667, 128.031111111, 219.332444444, 163.808888889}},
    {"restore", {228.778666667, 121.031111111, 278.556444444, 170.808888889}, {235.778666667, 128.031111111, 271.556444444, 163.808888889}},
    {"back", {15.644444444, 78.677333333, 54.328888889, 117.361777778}, {20.177777778, 83.210666667, 49.795555556, 112.828444444}},
};

// Back, at insets 0, and the clear box at the sizes it is measured at
constexpr double kBackBox[4] = {15.644444444, 78.677333333, 54.328888889, 117.361777778};
constexpr double kBackTopUnits = 78.677333333;
constexpr double kClearanceUnits = 5.688888889; // clearance_px 16
// least clearance, clear_portals box to Back box, over the 9 aspects x 11 sizes x the 4 inset sets
constexpr double kLeastBackToClear = 5.688888889;
// the arrows against the column at size 1.4, at 5:4: the left arrow's right edge 79.360 and the column's left edge 105.529
constexpr double kArrowToColumn54 = 26.168888889;
constexpr double kColumnBottom = 170.808888889;
constexpr double kArrowsTop14 = 176.640000000;
constexpr double kColumnHalfWidth = 54.471111111;

// ---- the block, as the file says it ----------------------------------------------------------------------------------------------------------

void TheAdjustBlockIsWhatTheFileSays() {
    const Loaded& d = Data();
    if (!d.ok) return;
    const Adjust::Rules& a = d.adjust;

    CHECK_MSG((a.backdrop == Adjust::Colour{22, 19, 33, 255}), "an opaque flat backdrop (22, 19, 33)");
    CHECK_MSG(a.rowSize == 0.27 && a.rowOpacity == 0.40 && a.rowLock == 0.57, "Magic Rampage's rows as fractions of the view's height: 0.27, 0.40, 0.57");
    CHECK_MSG(a.tile.boxPx == 102.4 && a.tile.offsetPx == 102.0 && a.tile.faceInset == 0.140625, "the four tiles: 102.4 px, 102 px out, a face 18 / 128 in");
    CHECK_MSG(a.lock.boxPx == 140.0 && a.lock.offsetPx == 73.44 && a.lock.faceInset == 0.140625, "the lock and Restore: 140 px, 73.44 px out, the same frame");
    CHECK_MSG(a.back.boxPx == 108.8 && a.back.faceInset == 0.1171875 && a.back.gapBelowClearPx == 16.0, "Back: 108.8 px, a face 15 / 128 in, 16 px under the clear-portals box");
    CHECK_MSG(a.readout.font == "Matura84_shadow.fnt" && a.readout.unitsPerFontPx == 0.4 && a.readout.alphaByte == 255, "the readouts in the original's font at 0.4 a font px");
    CHECK_MSG(a.inertAlphaByte == 90, "a widget that does nothing is drawn at 90 of 255");
    CHECK_MSG((a.flatTile == Adjust::Colour{205, 212, 232, 255}) && (a.flatUnlocked == Adjust::Colour{200, 60, 50, 255}), "the flat tier's two colours");
    CHECK_MSG(a.unlocked.outlineThicknessUnits == 0.7 && a.unlocked.outlineAlphaByte == 110 && a.unlocked.outlineSwingByte == 70 && a.unlocked.outlinePeriodTicks == 48,
              "the outline: 0.7 unit, alpha 110 +- 70, over 48 ticks");
    CHECK_MSG((a.unlocked.outlineRgb == std::array<int, 3>{255, 255, 255}) && (a.unlocked.hitFrameRgb == std::array<int, 3>{255, 255, 255}), "white outlines and hit frames");
    CHECK_MSG(a.unlocked.hitFrameAlphaByte == 0x22 && a.unlocked.hitFrameThicknessUnits == 0.5, "a hit rectangle is a faint frame: alpha 0x22, half a unit");
    CHECK_MSG(a.unlocked.guideThicknessUnits == 0.7 && (a.unlocked.guide == Adjust::Colour{255, 215, 110, 170}), "the character row's guide: 0.7 unit, a warm line");

    // The pictures: the file and the margin of each, by tier.
    struct Art {
        Adjust::Picture picture;
        const char* mr;
        double mrInset;
        const char* generated;
    };
    const Art arts[] = {{Adjust::Picture::SizeMinus, "size_minus.png", 0.140625, "adjust_size_minus.png"},
                        {Adjust::Picture::SizePlus, "size_plus.png", 0.140625, "adjust_size_plus.png"},
                        {Adjust::Picture::OpacityMinus, "opacity_minus.png", 0.140625, "adjust_opacity_minus.png"},
                        {Adjust::Picture::OpacityPlus, "opacity_plus.png", 0.140625, "adjust_opacity_plus.png"},
                        {Adjust::Picture::LockLocked, "lock_locked.png", 0.140625, "adjust_lock_locked.png"},
                        {Adjust::Picture::LockUnlocked, "lock_unlocked.png", 0.1640625, "adjust_lock_unlocked.png"},
                        {Adjust::Picture::Restore, "restore.png", 0.140625, "adjust_restore.png"},
                        {Adjust::Picture::Back, "back.png", 0.1171875, "adjust_back.png"}};
    std::set<std::string> files;
    for (const Art& art : arts) {
        const std::string name = Adjust::KeyOf(art.picture);
        const auto& mr = a.mr[static_cast<std::size_t>(art.picture)];
        const auto& generated = a.generated[static_cast<std::size_t>(art.picture)];
        CHECK_MSG(mr && generated, name + " has an entry in both tiers");
        if (!mr || !generated) continue;
        CHECK_MSG(mr->file == art.mr && mr->bodyInset == art.mrInset, name + ": Magic Rampage's " + art.mr + " with a margin of " + Num(art.mrInset));
        CHECK_MSG(generated->file == art.generated && generated->bodyInset == 0.109375, name + ": the drawn glyph " + art.generated + " with the controls' margin");
        files.insert(mr->file);
        files.insert(generated->file);
    }
    CHECK_MSG(files.size() == 16u, "sixteen files, no two the same: " + std::to_string(files.size()));
    // The red frame's margin is the tool's measured 21 / 128, the back button's 15 / 128, the stone frame's 18 / 128: the frame decides it.
    CHECK_MSG(a.mr[static_cast<std::size_t>(Adjust::Picture::LockUnlocked)]->bodyInset == 21.0 / 128.0 && a.mr[static_cast<std::size_t>(Adjust::Picture::Back)]->bodyInset == 15.0 / 128.0 &&
                  a.mr[static_cast<std::size_t>(Adjust::Picture::SizeMinus)]->bodyInset == 18.0 / 128.0,
              "the margins are 21, 15 and 18 px of 128");
    // None of the generated files is one the five controls or the gear write into the same folder.
    for (const Adjust::Picture picture : Adjust::kPictures) {
        const std::string& file = a.generated[static_cast<std::size_t>(picture)]->file;
        CHECK_MSG(file != d.options.entry.sprite, std::string(Adjust::KeyOf(picture)) + " is not the gear's file");
        for (const Controls::Id id : Controls::kClaimOrder) {
            CHECK_MSG(file != d.controls.art[static_cast<std::size_t>(Controls::ArtTier::Generated)][static_cast<std::size_t>(id)]->file, std::string(Adjust::KeyOf(picture)) + " is not " + Controls::KeyOf(id) + "'s generated file");
        }
    }
    // The widgets: seven, with names; and eight pictures, the lock's two by its state.
    CHECK(Adjust::kWidgetCount == 7u && Adjust::kPictureCount == 8u && Adjust::kWidgets.size() == 7u && Adjust::kPictures.size() == 8u);
    CHECK_MSG(Adjust::PictureOf(Adjust::Widget::Lock, false) == std::optional<Adjust::Picture>(Adjust::Picture::LockLocked), "a shut lock shows the closed padlock");
    CHECK_MSG(Adjust::PictureOf(Adjust::Widget::Lock, true) == std::optional<Adjust::Picture>(Adjust::Picture::LockUnlocked), "an open one the open padlock");
    CHECK_MSG(!Adjust::PictureOf(Adjust::Widget::None, false), "no widget shows no picture");
    for (const Adjust::Widget widget : Adjust::kWidgets) {
        const std::optional<Adjust::Picture> picture = Adjust::PictureOf(widget, false);
        CHECK_MSG(picture && std::string(Adjust::KeyOf(*picture)).substr(0, std::string(Adjust::NameOf(widget)).size()) == Adjust::NameOf(widget),
                  std::string(Adjust::NameOf(widget)) + "'s picture is named for it");
        CHECK_MSG(Adjust::PictureOfKey(Adjust::KeyOf(*picture)) == picture, "and a key reads back to its picture");
    }
    CHECK_MSG(!Adjust::PictureOfKey("walk_left") && !Adjust::PictureOfKey(""), "a name that is not a picture's is none");
}

// ---- the layout ---------------------------------------------------------------------------------------------------------------------------

void TheWidgetColumnIsMagicRampagesInUnits() {
    const Loaded& d = Data();
    if (!d.ok) return;
    const glm::dvec2 view = ViewOf(16.0 / 9.0);
    const Adjust::Layout layout = Adjust::LayOut(d.adjust, d.controls, view, kNoInsets);
    CHECK_MSG(NearPoint(layout.view, view), "the layout is of this view");
    for (std::size_t i = 0; i < 7; ++i) {
        const WidgetRow& row = kWidgetRows169[i];
        const Adjust::Widget widget = Adjust::kWidgets[i];
        CHECK_MSG(std::string(Adjust::NameOf(widget)) == row.name, std::string("the order of the widgets: ") + row.name);
        const Adjust::Tile& tile = layout.At(widget);
        CHECK_MSG(tile.widget == widget, std::string(row.name) + " is its own tile");
        CHECK_MSG(NearRect(tile.box, RectOf(row.box), 1e-6), std::string(row.name) + "'s box at 16:9 is " + Show(RectOf(row.box)) + ": " + Show(tile.box));
        CHECK_MSG(NearRect(tile.face, RectOf(row.face), 1e-6), std::string(row.name) + "'s face at 16:9 is " + Show(RectOf(row.face)) + ": " + Show(tile.face));
    }
    // In the numbers of Magic Rampage's own screen: a tile 36.409 across, 36.267 either side of the middle, in rows 69.12, 102.40 and 145.92 down.
    CHECK_MSG(Near(layout.At(Adjust::Widget::SizeMinus).box.size.x, 36.4088889, 1e-6) && Near(layout.At(Adjust::Widget::Lock).box.size.x, 49.7777778, 1e-6) &&
                  Near(layout.At(Adjust::Widget::Back).box.size.x, 38.6844444, 1e-6),
              "a tile is 36.409 units, the lock's 49.778 and Back's 38.684");
    CHECK_MSG(Near(layout.At(Adjust::Widget::SizeMinus).box.Centre().y, 69.12, 1e-9) && Near(layout.At(Adjust::Widget::OpacityPlus).box.Centre().y, 102.4, 1e-9) &&
                  Near(layout.At(Adjust::Widget::Restore).box.Centre().y, 145.92, 1e-9),
              "the rows' centres are 69.12, 102.40 and 145.92 units down");
    CHECK_MSG(Near(view.x * 0.5 - layout.At(Adjust::Widget::SizeMinus).box.Centre().x, 36.2666667, 1e-6) && Near(view.x * 0.5 - layout.At(Adjust::Widget::Lock).box.Centre().x, 26.112, 1e-6),
              "the tiles are 36.267 and the lock 26.112 either side of the middle");
    // The column: its bounding box is W/2 +- 54.471 across and 50.916 to 170.809 down.
    CHECK_MSG(Near(layout.column.min.x, view.x * 0.5 - kColumnHalfWidth, 1e-6) && Near(layout.column.Max().x, view.x * 0.5 + kColumnHalfWidth, 1e-6),
              "the column is W/2 +- 54.471 across: " + Show(layout.column));
    CHECK_MSG(Near(layout.column.min.y, 50.915555556, 1e-6) && Near(layout.column.Max().y, kColumnBottom, 1e-6), "and 50.916 to 170.809 down");
}

void TheColumnIsCentredAtEveryAspectAndItsRowsDoNotMove() {
    const Loaded& d = Data();
    if (!d.ok) return;
    const Adjust::Layout reference = Adjust::LayOut(d.adjust, d.controls, ViewOf(16.0 / 9.0), kNoInsets);
    for (const Aspect& aspect : kAspects) {
        const glm::dvec2 view = ViewOf(aspect.ratio);
        const Adjust::Layout layout = Adjust::LayOut(d.adjust, d.controls, view, kNoInsets);
        const std::string at = std::string(" at ") + aspect.name;
        // Mirror images about the view's middle, pair by pair.
        for (const auto& [left, right] : {std::pair{Adjust::Widget::SizeMinus, Adjust::Widget::SizePlus}, {Adjust::Widget::OpacityMinus, Adjust::Widget::OpacityPlus}, {Adjust::Widget::Lock, Adjust::Widget::Restore}}) {
            const Hud::Rect& l = layout.At(left).box;
            const Hud::Rect& r = layout.At(right).box;
            CHECK_MSG(Near(l.Centre().x + r.Centre().x, view.x, 1e-9) && Near(l.Centre().y, r.Centre().y) && NearPoint(l.size, r.size), std::string(Adjust::NameOf(left)) + " and " + Adjust::NameOf(right) + " are mirror images" + at);
        }
        CHECK_MSG(Near(layout.column.min.x + layout.column.Max().x, view.x, 1e-9), std::string("the column is centred") + at);
        // The rows are fractions of the HEIGHT: the same at every aspect (the view's height is always 256).
        for (const Adjust::Widget widget : {Adjust::Widget::SizeMinus, Adjust::Widget::OpacityMinus, Adjust::Widget::Lock}) {
            CHECK_MSG(Near(layout.At(widget).box.min.y, reference.At(widget).box.min.y, 1e-12) && NearPoint(layout.At(widget).box.size, reference.At(widget).box.size, 1e-12),
                      std::string(Adjust::NameOf(widget)) + " does not move or change size with the width" + at);
        }
    }
}

void TheFacesOfTheWidgetsNeverMeetAtAnyAspect() {
    const Loaded& d = Data();
    if (!d.ok) return;
    double least = std::numeric_limits<double>::max();
    for (const Aspect& aspect : kAspects) {
        const Adjust::Layout layout = Adjust::LayOut(d.adjust, d.controls, ViewOf(aspect.ratio), kNoInsets);
        for (std::size_t i = 0; i < 7; ++i) {
            const Hud::Rect& face = layout.At(Adjust::kWidgets[i]).face;
            const Hud::Rect& box = layout.At(Adjust::kWidgets[i]).box;
            CHECK_MSG(WhollyInside(face, box) && face.size.x > 0.0 && face.size.y > 0.0, std::string(Adjust::NameOf(Adjust::kWidgets[i])) + "'s face is a part of its box at " + aspect.name);
            for (std::size_t j = i + 1; j < 7; ++j) {
                const double gap = Gap(face, layout.At(Adjust::kWidgets[j]).face);
                least = std::min(least, gap);
                CHECK_MSG(gap > 7.0, std::string(Adjust::NameOf(Adjust::kWidgets[i])) + " and " + Adjust::NameOf(Adjust::kWidgets[j]) + " faces are " + Num(gap) + " apart at " + aspect.name);
            }
        }
    }
    CHECK_MSG(Near(least, 7.1111111, 1e-5), "the closest two faces anywhere are 7.111 apart (the two rows' tiles): " + Num(least));
}

void TheBoxesOfTwoRowsOverlapByThreePointOneAndTheirFacesDoNot() {
    const Loaded& d = Data();
    if (!d.ok) return;
    const Adjust::Layout layout = Adjust::LayOut(d.adjust, d.controls, ViewOf(1.25), kNoInsets);
    double overlap = 0.0;
    const double gap = Gap(layout.At(Adjust::Widget::SizeMinus).box, layout.At(Adjust::Widget::OpacityMinus).box, &overlap);
    CHECK_MSG(Near(gap, -3.1288889, 1e-6), "the size row's boxes overlap the opacity row's by 3.129 units (36.409 over a pitch of 33.28): " + Num(gap));
    CHECK_MSG(overlap > 100.0, "an overlap of area " + Num(overlap));
    CHECK_MSG(Gap(layout.At(Adjust::Widget::SizeMinus).face, layout.At(Adjust::Widget::OpacityMinus).face) > 7.0, "and their faces, which a tap hits, are 7.1 apart");
    CHECK_MSG(Gap(layout.At(Adjust::Widget::OpacityMinus).box, layout.At(Adjust::Widget::Lock).box) > 0.4 && Gap(layout.At(Adjust::Widget::Lock).box, layout.At(Adjust::Widget::Restore).box) > 2.0,
              "the opacity row clears the lock row's boxes by 0.43 and the lock clears Restore by 2.44");
}

void StrictlyInsideIsOpenOnEveryEdge() {
    const Hud::Rect rect{glm::dvec2(10.0, 20.0), glm::dvec2(30.0, 40.0)};
    CHECK(Adjust::StrictlyInside(rect, glm::dvec2(25.0, 40.0)));
    CHECK(Adjust::StrictlyInside(rect, glm::dvec2(10.0 + 1e-9, 20.0 + 1e-9)) && Adjust::StrictlyInside(rect, glm::dvec2(40.0 - 1e-9, 60.0 - 1e-9)));
    // On an edge, or a corner, is outside: Hud::Rect::Contains is closed, this is not.
    for (const glm::dvec2& edge : {glm::dvec2(10.0, 40.0), glm::dvec2(40.0, 40.0), glm::dvec2(25.0, 20.0), glm::dvec2(25.0, 60.0), glm::dvec2(10.0, 20.0), glm::dvec2(40.0, 60.0), glm::dvec2(10.0, 60.0), glm::dvec2(40.0, 20.0)}) {
        CHECK_MSG(!Adjust::StrictlyInside(rect, edge) && rect.Contains(edge), "(" + Num(edge.x) + ", " + Num(edge.y) + ") is on an edge: outside by this rule, inside by the closed one");
    }
    CHECK(!Adjust::StrictlyInside(rect, glm::dvec2(9.0, 40.0)) && !Adjust::StrictlyInside(rect, glm::dvec2(41.0, 40.0)) && !Adjust::StrictlyInside(rect, glm::dvec2(25.0, 19.0)) &&
          !Adjust::StrictlyInside(rect, glm::dvec2(25.0, 61.0)));
    CHECK(!Adjust::StrictlyInside(Hud::Rect{glm::dvec2(1.0), glm::dvec2(0.0)}, glm::dvec2(1.0)));
}

void WidgetAtNamesTheWidgetWhoseFaceHoldsThePoint() {
    const Loaded& d = Data();
    if (!d.ok) return;
    for (const Aspect& aspect : kAspects) {
        const Adjust::Layout layout = Adjust::LayOut(d.adjust, d.controls, ViewOf(aspect.ratio), kNoInsets);
        for (const Adjust::Widget widget : Adjust::kWidgets) {
            const Hud::Rect& face = layout.At(widget).face;
            const std::string what = std::string(Adjust::NameOf(widget)) + " at " + aspect.name;
            CHECK_MSG(Adjust::WidgetAt(layout, face.Centre()) == widget, what + ": its face's centre is it");
            CHECK_MSG(Adjust::WidgetAt(layout, face.min + glm::dvec2(1e-6)) == widget && Adjust::WidgetAt(layout, face.Max() - glm::dvec2(1e-6)) == widget, what + ": and just inside every corner");
            // Not on the edge of its face, not in its box's transparent margin.
            CHECK_MSG(Adjust::WidgetAt(layout, glm::dvec2(face.min.x, face.Centre().y)) == Adjust::Widget::None && Adjust::WidgetAt(layout, glm::dvec2(face.Max().x, face.Centre().y)) == Adjust::Widget::None &&
                          Adjust::WidgetAt(layout, glm::dvec2(face.Centre().x, face.min.y)) == Adjust::Widget::None && Adjust::WidgetAt(layout, glm::dvec2(face.Centre().x, face.Max().y)) == Adjust::Widget::None,
                      what + ": the edges of the face are not it");
            const Hud::Rect& box = layout.At(widget).box;
            CHECK_MSG(Adjust::WidgetAt(layout, box.min + glm::dvec2(0.5)) == Adjust::Widget::None, what + ": nor is the corner of its box, which the picture leaves transparent");
        }
        CHECK_MSG(Adjust::WidgetAt(layout, glm::dvec2(1.0, 1.0)) == Adjust::Widget::None && Adjust::WidgetAt(layout, ViewOf(aspect.ratio) - glm::dvec2(1.0)) == Adjust::Widget::None,
                  std::string("the corners of the view are no widget at ") + aspect.name);
    }
}

// ---- the Back button, the clearances and the meetings --------------------------------------------------------------------------------------

void TheBackButtonIsAFixedPlaceUnderTheClearPreview() {
    const Loaded& d = Data();
    if (!d.ok) return;
    const glm::dvec2 view = ViewOf(16.0 / 9.0);
    const Hud::Rect back = Adjust::BackBox(d.adjust, d.controls, view, kNoInsets);
    CHECK_MSG(NearRect(back, RectOf(kBackBox), 1e-6), "Back's box is (15.644, 78.677)-(54.329, 117.362): " + Show(back));
    CHECK_MSG(Near(back.min.y, kBackTopUnits, 1e-6) && Near(back.min.x, 44.0 / kPxPerUnit, 1e-9), "its left is the layout's 44 px margin and its top 78.677 units: the margin, the largest corner box (57.344) and 16 px");
    // The top is built from the table: 44 px + 0.9 x 128 px x 1.4 + 16 px, over 2.8125.
    CHECK_MSG(Near(back.min.y, (44.0 + 0.9 * 128.0 * 1.4 + 16.0) / kPxPerUnit, 1e-9), "(44 + 161.28 + 16) / 2.8125");
    // It does not depend on the width, and it clears clear_portals' box by the 16 px (5.689 units) at the largest size and by more at every other.
    double least = std::numeric_limits<double>::max();
    for (const Aspect& aspect : kAspects) {
        const glm::dvec2 v = ViewOf(aspect.ratio);
        CHECK_MSG(NearRect(Adjust::BackBox(d.adjust, d.controls, v, kNoInsets), back, 1e-12), std::string("Back is the same place at ") + aspect.name);
        for (int steps = 4; steps <= 14; ++steps) {
            const Controls::Tuning tuning = SizedTuning(steps);
            const std::array<Hud::Rect, 5> boxes = Controls::Boxes(d.controls, tuning, v, kNoInsets);
            const Hud::Rect& clear = boxes[static_cast<std::size_t>(Controls::Id::Clear)];
            const double gap = Gap(clear, back);
            least = std::min(least, gap);
            CHECK_MSG(gap >= kClearanceUnits - 1e-9, std::string("Back clears clear_portals' box by ") + Num(gap) + " at " + aspect.name + ", size " + Num(steps / 10.0));
            CHECK_MSG(back.min.y > clear.Max().y, std::string("Back is wholly below the clear box at ") + aspect.name + ", size " + Num(steps / 10.0));
        }
    }
    CHECK_MSG(Near(least, kClearanceUnits, 1e-9), "the least clearance is exactly clearance_px 16 = 5.689 units (at size 1.4): " + Num(least));
    // The insets move the corner and the place with it: the clearance is the same under every safe area.
    double leastInset = std::numeric_limits<double>::max();
    for (const Controls::Insets& insets : kInsetSets) {
        const Hud::Rect inset = Adjust::BackBox(d.adjust, d.controls, view, insets);
        CHECK_MSG(Near(inset.min.x - back.min.x, insets.left, 1e-12) && Near(inset.min.y - back.min.y, insets.top, 1e-12) && NearPoint(inset.size, back.size), "Back moves in by the insets and no more");
        for (const Aspect& aspect : kAspects) {
            const glm::dvec2 v = ViewOf(aspect.ratio);
            for (int steps = 4; steps <= 14; ++steps) {
                const std::array<Hud::Rect, 5> boxes = Controls::Boxes(d.controls, SizedTuning(steps), v, insets);
                leastInset = std::min(leastInset, Gap(boxes[static_cast<std::size_t>(Controls::Id::Clear)], Adjust::BackBox(d.adjust, d.controls, v, insets)));
            }
        }
    }
    CHECK_MSG(Near(leastInset, kLeastBackToClear, 1e-9), "and the least clearance to clear_portals over every aspect, size and safe area is 5.689 (the Python's): " + Num(leastInset));
}

void NoOtherDefaultControlMeetsBackAtAnySizeAspectOrSafeArea() {
    const Loaded& d = Data();
    if (!d.ok) return;
    // The split arrows stand in the bottom corners: the left one's top is 176.64 at size 1.4 and Back's bottom 117.36, 59.28 apart, which a top and a bottom
    // inset of less than that between them leave. (Every set below sums to at most 40.)
    CHECK_MSG(Near(kArrowsTop14 - kBackBox[3], 59.2782222, 1e-6), "Back's bottom is 59.278 units above the left arrow's top at size 1.4");
    for (const Controls::Insets& insets : kInsetSets) {
        CHECK_MSG(insets.top + insets.bottom < 59.27, "the safe area is one the claim is made for");
        for (const Aspect& aspect : kAspects) {
            const glm::dvec2 view = ViewOf(aspect.ratio);
            const Hud::Rect back = Adjust::BackBox(d.adjust, d.controls, view, insets);
            for (int steps = 4; steps <= 14; ++steps) {
                const std::array<Hud::Rect, 5> boxes = Controls::Boxes(d.controls, SizedTuning(steps), view, insets);
                for (const Controls::Id id : Controls::kClaimOrder) {
                    if (id == Controls::Id::Clear) continue;
                    const double gap = Gap(boxes[static_cast<std::size_t>(id)], back);
                    CHECK_MSG(gap > 0.0, std::string(Controls::KeyOf(id)) + " does not meet Back at " + aspect.name + ", size " + Num(steps / 10.0) + ": " + Num(gap));
                }
            }
        }
    }
}

void NoDefaultControlIsWhollyUnderAWidgetAtAnyAspectOrSize() {
    const Loaded& d = Data();
    if (!d.ok) return;
    long meetings = 0;
    for (const Aspect& aspect : kAspects) {
        const glm::dvec2 view = ViewOf(aspect.ratio);
        const Adjust::Layout layout = Adjust::LayOut(d.adjust, d.controls, view, kNoInsets);
        for (int steps = 4; steps <= 14; ++steps) {
            const std::array<Hud::Rect, 5> boxes = Controls::Boxes(d.controls, SizedTuning(steps), view, kNoInsets);
            for (const Controls::Id id : Controls::kClaimOrder) {
                for (const Adjust::Widget widget : Adjust::kWidgets) {
                    const std::string what = std::string(Controls::KeyOf(id)) + " under " + Adjust::NameOf(widget) + " at " + aspect.name + ", size " + Num(steps / 10.0);
                    CHECK_MSG(!WhollyInside(boxes[static_cast<std::size_t>(id)], layout.At(widget).box), what);
                    if (Gap(boxes[static_cast<std::size_t>(id)], layout.At(widget).box) < 0.0) ++meetings;
                }
            }
        }
    }
    CHECK_MSG(meetings > 0, "the walk finds the partial meetings the table records (so it is looking): " + std::to_string(meetings));
}

void TheNarrowAspectsMeetAsRecorded() {
    const Loaded& d = Data();
    if (!d.ok) return;
    // "Restart meets the size row's plus tile at 5:4 at every size, from size 1.1 at 4:3, at 1.4 at 3:2, never at 16:10 and wider" - and NOTHING else meets any
    // widget's box, at any size and any aspect: not the arrows (they are in the bottom corners, 26.17 units clear of the column at 5:4 at size 1.4), not
    // pause, not clear (22.07 above Back at size 0.7, 5.69 at 1.4).
    for (std::size_t i = 0; i < kAspects.size(); ++i) {
        const Aspect& aspect = kAspects[i];
        const MeetRow& row = kMeetRows[i];
        CHECK_MSG(std::string(row.aspect) == aspect.name && Near(row.ratio, aspect.ratio, 1e-12), "the rows of the table are the aspects, in order: " + std::string(aspect.name));
        const glm::dvec2 view = ViewOf(aspect.ratio);
        const Adjust::Layout layout = Adjust::LayOut(d.adjust, d.controls, view, kNoInsets);
        int firstBox = 0;
        int firstFace = 0;
        double areaFirst = 0.0;
        double areaLast = 0.0;
        double faceFirst = 0.0;
        for (int steps = 4; steps <= 14; ++steps) {
            const std::array<Hud::Rect, 5> boxes = Controls::Boxes(d.controls, SizedTuning(steps), view, kNoInsets);
            for (const Controls::Id id : Controls::kClaimOrder) {
                for (const Adjust::Widget widget : Adjust::kWidgets) {
                    double area = 0.0;
                    const double gap = Gap(boxes[static_cast<std::size_t>(id)], layout.At(widget).box, &area);
                    if (gap < 0.0) {
                        CHECK_MSG(id == Controls::Id::Reset && widget == Adjust::Widget::SizePlus,
                                  std::string(Controls::KeyOf(id)) + " meets " + Adjust::NameOf(widget) + " at " + aspect.name + ", size " + Num(steps / 10.0) + ": only restart and the size row's plus tile meet");
                        if (firstBox == 0) {
                            firstBox = steps;
                            areaFirst = area;
                        }
                        if (steps == 14) areaLast = area;
                    }
                    double faceArea = 0.0;
                    if (Gap(boxes[static_cast<std::size_t>(id)], layout.At(widget).face, &faceArea) < 0.0 && firstFace == 0) {
                        firstFace = steps;
                        faceFirst = faceArea;
                    }
                }
            }
        }
        CHECK_MSG(firstBox == row.firstBoxStep, std::string(aspect.name) + ": restart first meets a widget box at size " + Num(row.firstBoxStep / 10.0) + " (0: never): " + Num(firstBox / 10.0));
        CHECK_MSG(Near(areaFirst, row.boxAreaFirst, 0.01) && Near(areaLast, row.boxAreaLast, 0.01), std::string(aspect.name) + ": the overlap is " + Num(row.boxAreaFirst) + " u^2 at the first size and " + Num(row.boxAreaLast) + " at 1.4: " + Num(areaFirst) + ", " + Num(areaLast));
        CHECK_MSG(firstFace == row.firstFaceStep && Near(faceFirst, row.faceAreaFirst, 0.01), std::string(aspect.name) + ": restart first meets a widget FACE (a tap) at size " + Num(row.firstFaceStep / 10.0) + " (0: never), " + Num(row.faceAreaFirst) + " u^2: " + Num(firstFace / 10.0) + ", " + Num(faceFirst));
    }
}

void TheClearanceOfTheSplitArrowsAndTheOtherControlsToTheColumn() {
    const Loaded& d = Data();
    if (!d.ok) return;
    // The design's table 3.2, re-derived for the split arrows (CTL/CTL11/editor_clearance.py): the least distance from a default control's box to the six tiles
    // (not Back) at size 0.7 and at 1.4, and to any widget's box at 0.7. Negative is an overlap (minus its smaller side). The grouped pair's figures - 5.83 and
    // 22.86 at size 1.4 - are superseded: with the arrows in the corners they do not arise (see RestoreAndTheColumnClearTheArrows...).
    for (std::size_t i = 0; i < kAspects.size(); ++i) {
        const Aspect& aspect = kAspects[i];
        const MeetRow& row = kMeetRows[i];
        const glm::dvec2 view = ViewOf(aspect.ratio);
        const Adjust::Layout layout = Adjust::LayOut(d.adjust, d.controls, view, kNoInsets);
        for (const int steps : {7, 14}) {
            const std::array<Hud::Rect, 5> boxes = Controls::Boxes(d.controls, SizedTuning(steps), view, kNoInsets);
            double tiles = std::numeric_limits<double>::max();
            double all = std::numeric_limits<double>::max();
            for (const Controls::Id id : Controls::kClaimOrder) {
                for (const Adjust::Widget widget : Adjust::kWidgets) {
                    const double gap = Gap(boxes[static_cast<std::size_t>(id)], layout.At(widget).box);
                    all = std::min(all, gap);
                    if (widget != Adjust::Widget::Back) tiles = std::min(tiles, gap);
                }
            }
            const double expected = steps == 7 ? row.tilesAt07 : row.tilesAt14;
            CHECK_MSG(Near(tiles, expected, 0.01), std::string(aspect.name) + ", size " + Num(steps / 10.0) + ": the least gap to the six tiles is " + Num(expected) + " (the Python's): " + Num(tiles));
            if (steps == 7) CHECK_MSG(Near(all, row.allAt07, 0.01), std::string(aspect.name) + ", size 0.7: the least gap to any widget, Back included, is " + Num(row.allAt07) + ": " + Num(all));
        }
    }
}

void AtSixteenNineTheLeastClearanceToTheTilesIsThirtyOnePointThreeSevenAtSizeOnePointFour() {
    const Loaded& d = Data();
    if (!d.ok) return;
    // Named for the figure it replaces: the grouped pair's 22.86 at 16:9 and size 1.4 (walk_right against the column) is gone with the pair; with the arrows split
    // the nearest default control to the six tiles at 16:9 and size 1.4 is RESTART, 31.37 from the size row's plus tile.
    const glm::dvec2 view = ViewOf(16.0 / 9.0);
    const Adjust::Layout layout = Adjust::LayOut(d.adjust, d.controls, view, kNoInsets);
    const std::array<Hud::Rect, 5> boxes = Controls::Boxes(d.controls, SizedTuning(14), view, kNoInsets);
    double least = std::numeric_limits<double>::max();
    Controls::Id which = Controls::Id::Left;
    for (const Controls::Id id : Controls::kClaimOrder) {
        for (const Adjust::Widget widget : Adjust::kWidgets) {
            if (widget == Adjust::Widget::Back) continue;
            const double gap = Gap(boxes[static_cast<std::size_t>(id)], layout.At(widget).box);
            if (gap < least) {
                least = gap;
                which = id;
            }
        }
    }
    CHECK_MSG(Near(least, 31.374222, 1e-5) && which == Controls::Id::Reset, "the least gap is 31.37 and it is restart's: " + Num(least) + " (" + Controls::KeyOf(which) + ")");
    // The walk arrows: a corner each, 93.7 units from the column at size 1.4 at 16:9.
    const Hud::Rect& left = boxes[static_cast<std::size_t>(Controls::Id::Left)];
    const Hud::Rect& right = boxes[static_cast<std::size_t>(Controls::Id::Right)];
    CHECK_MSG(Near(layout.column.min.x - left.Max().x, 93.724444, 1e-5) && Near(right.min.x - layout.column.Max().x, 93.724444, 1e-5), "and the arrows stand 93.72 from it each side at 16:9");
    CHECK_MSG(Near(left.min.y, kArrowsTop14, 1e-9) && Near(right.min.y, kArrowsTop14, 1e-9), "both tops at size 1.4 are 176.64");
}

void RestoreAndTheColumnClearTheSplitArrowsAtEverySizeAndAspect() {
    const Loaded& d = Data();
    if (!d.ok) return;
    // Named for the figure it replaces: "Restore clears the pair by 5.83 at size 1.4" was the grouped pair's top (176.64) against the column's bottom (170.81); the
    // arrows are in the corners now, so what clears is sideways: the column is 26.17 units from each arrow at 5:4 and size 1.4, and more at every wider aspect and
    // smaller size. The vertical 5.83 is still the arithmetic of the numbers (176.64 - 170.81), and no longer a clearance anything has to keep.
    CHECK_MSG(Near(kArrowsTop14 - kColumnBottom, 5.8311111, 1e-6), "the arrows' top at size 1.4 is 5.831 below the column's bottom");
    double least = std::numeric_limits<double>::max();
    for (const Aspect& aspect : kAspects) {
        const glm::dvec2 view = ViewOf(aspect.ratio);
        const Adjust::Layout layout = Adjust::LayOut(d.adjust, d.controls, view, kNoInsets);
        for (int steps = 4; steps <= 14; ++steps) {
            const std::array<Hud::Rect, 5> boxes = Controls::Boxes(d.controls, SizedTuning(steps), view, kNoInsets);
            for (const Controls::Id id : {Controls::Id::Left, Controls::Id::Right}) {
                const double gap = Gap(boxes[static_cast<std::size_t>(id)], layout.column);
                least = std::min(least, gap);
                CHECK_MSG(gap >= kArrowToColumn54 - 1e-6, std::string(Controls::KeyOf(id)) + " clears the column by " + Num(gap) + " at " + aspect.name + ", size " + Num(steps / 10.0));
            }
        }
    }
    // The least is at 5:4 and size 1.4: 26.169 across and 5.831 down, which is the distance between the corners, 26.81.
    CHECK_MSG(Near(least, std::hypot(kArrowToColumn54, kArrowsTop14 - kColumnBottom), 1e-6), "the least is the corner-to-corner 26.81 (5:4, size 1.4: 26.169 across, 5.831 down): " + Num(least));
    // Restore and the lock themselves, at the narrowest window and the largest arrows: each clears the arrow of its own side by more than the column does.
    const glm::dvec2 narrow = ViewOf(1.25);
    const Adjust::Layout layout = Adjust::LayOut(d.adjust, d.controls, narrow, kNoInsets);
    const std::array<Hud::Rect, 5> boxes = Controls::Boxes(d.controls, SizedTuning(14), narrow, kNoInsets);
    CHECK_MSG(Gap(boxes[static_cast<std::size_t>(Controls::Id::Right)], layout.At(Adjust::Widget::Restore).box) >= kArrowToColumn54 - 1e-6, "Restore clears the right arrow by 26.17 or more at 5:4");
    CHECK_MSG(Gap(boxes[static_cast<std::size_t>(Controls::Id::Left)], layout.At(Adjust::Widget::Lock).box) >= kArrowToColumn54 - 1e-6, "and the lock the left arrow");
}

// ---- the touch -------------------------------------------------------------------------------------------------------------------------------

void ATileActsOnTheReleaseStrictlyInside() {
    using W = Adjust::Widget;
    // Down and up on the same widget (the pointer inside it on the release): a press, on the release, of that widget.
    {
        Adjust::Touch touch;
        Adjust::Press press = Adjust::Follow(touch, true, false, true, W::SizePlus);
        CHECK_MSG(!press.happened && touch.down == W::SizePlus && touch.held == W::SizePlus, "down on a tile: held, nothing yet");
        press = Adjust::Follow(touch, false, false, true, W::SizePlus);
        CHECK_MSG(!press.happened && touch.held == W::SizePlus, "held on: still nothing");
        press = Adjust::Follow(touch, false, true, false, W::SizePlus);
        CHECK_MSG(press.happened && press.widget == W::SizePlus && touch.down == W::None && touch.held == W::None, "up on it: the press, and the touch is forgotten");
    }
    // A click inside one tick is both.
    {
        Adjust::Touch touch;
        const Adjust::Press press = Adjust::Follow(touch, true, true, true, W::Lock);
        CHECK_MSG(press.happened && press.widget == W::Lock, "down and up in one tick is a press");
    }
    // A slide off cancels: down on one, up on another, or on nothing.
    for (const W other : {W::SizeMinus, W::OpacityPlus, W::Restore, W::Back, W::None}) {
        Adjust::Touch touch;
        Adjust::Follow(touch, true, false, true, W::SizePlus);
        const Adjust::Press moved = Adjust::Follow(touch, false, false, true, other);
        CHECK_MSG(touch.held == W::None, std::string("slid onto ") + Adjust::NameOf(other) + ": nothing is held");
        const Adjust::Press press = Adjust::Follow(touch, false, true, false, other);
        CHECK_MSG(!moved.happened && !press.happened, std::string("down on size_plus and up on ") + Adjust::NameOf(other) + " presses nothing");
    }
    // Slid off and back on: held again, and the release inside is a press.
    {
        Adjust::Touch touch;
        Adjust::Follow(touch, true, false, true, W::Back);
        Adjust::Follow(touch, false, false, true, W::None);
        CHECK(touch.held == W::None && touch.down == W::Back);
        Adjust::Follow(touch, false, false, true, W::Back);
        CHECK_MSG(touch.held == W::Back, "slid back on: held again");
        CHECK(Adjust::Follow(touch, false, true, false, W::Back).happened);
    }
    // A press that begins in the world and slides onto a tile does nothing, whatever it does after.
    {
        Adjust::Touch touch;
        Adjust::Follow(touch, true, false, true, W::None);
        Adjust::Follow(touch, false, false, true, W::Lock);
        CHECK_MSG(touch.held == W::None, "a press that began on nothing never holds a tile");
        CHECK(!Adjust::Follow(touch, false, true, false, W::Lock).happened);
    }
    // A release with no press behind it, and a tick with no touch, are nothing.
    {
        Adjust::Touch touch;
        CHECK(!Adjust::Follow(touch, false, true, false, W::SizePlus).happened && !Adjust::Follow(touch, false, false, false, W::SizePlus).happened);
        CHECK(touch.down == W::None && touch.held == W::None);
    }
    // Strictly inside: a release exactly on the edge of the face is outside it (WidgetAt says none), so a press that went down inside and comes up on the edge is
    // not one. (The edge test is StrictlyInsideIsOpenOnEveryEdge; this is its consequence on the real layout.)
    {
        const Loaded& d = Data();
        if (!d.ok) return;
        const Adjust::Layout layout = Adjust::LayOut(d.adjust, d.controls, ViewOf(16.0 / 9.0), kNoInsets);
        const Hud::Rect& face = layout.At(W::OpacityPlus).face;
        Adjust::Touch touch;
        Adjust::Follow(touch, true, false, true, Adjust::WidgetAt(layout, face.Centre()));
        const Adjust::Widget onTheEdge = Adjust::WidgetAt(layout, glm::dvec2(face.Max().x, face.Centre().y));
        CHECK_MSG(onTheEdge == W::None && !Adjust::Follow(touch, false, true, false, onTheEdge).happened, "down inside, up exactly on the face's edge: no press");
    }
}

void ThePressThatOpenedTheScreenIsDeadUntilItLifts() {
    using W = Adjust::Widget;
    // The press that opened the screen is still down, over a tile (the Options button's place is under it at some window shapes): nothing it does counts.
    {
        Adjust::Touch touch;
        touch.dead = true;
        for (int tick = 0; tick < 5; ++tick) {
            const Adjust::Press press = Adjust::Follow(touch, false, false, true, W::SizePlus);
            CHECK_MSG(!press.happened && touch.held == W::None && touch.down == W::None && touch.dead, "a held opening press holds nothing and is still dead on tick " + std::to_string(tick));
        }
        // It lifts: the screen is live from that tick, and the lift itself presses nothing.
        const Adjust::Press lift = Adjust::Follow(touch, false, true, false, W::SizePlus);
        CHECK_MSG(!lift.happened && !touch.dead, "its release presses nothing and ends the death");
        Adjust::Follow(touch, true, false, true, W::SizePlus);
        CHECK_MSG(touch.held == W::SizePlus, "a fresh press after it works");
        CHECK(Adjust::Follow(touch, false, true, false, W::SizePlus).happened);
    }
    // A fresh press ends it too, and is a press (a release and a new press between two ticks).
    {
        Adjust::Touch touch;
        touch.dead = true;
        Adjust::Follow(touch, true, false, true, W::Back);
        CHECK_MSG(!touch.dead && touch.held == W::Back, "a press made while it is dead is a new press");
        CHECK(Adjust::Follow(touch, false, true, false, W::Back).happened);
    }
    // No touch at all: it is not dead any more (nothing is down).
    {
        Adjust::Touch touch;
        touch.dead = true;
        Adjust::Follow(touch, false, false, false, W::None);
        CHECK_MSG(!touch.dead, "with no finger down the screen is live");
    }
}

// ---- what a press does -------------------------------------------------------------------------------------------------------------------

void AtALimitATileIsInert() {
    const Loaded& d = Data();
    if (!d.ok) return;
    using W = Adjust::Widget;
    const Controls::Rules& c = d.controls;
    // The default (0.7 and 1.0): every tile can act, and Restore cannot (no control is moved).
    const Controls::Tuning base;
    for (const W widget : {W::SizeMinus, W::SizePlus, W::OpacityMinus, W::OpacityPlus, W::Lock, W::Back}) {
        CHECK_MSG(Adjust::Enabled(widget, c, base), std::string(Adjust::NameOf(widget)) + " is enabled at the defaults");
    }
    CHECK_MSG(!Adjust::Enabled(W::Restore, c, base) && !Adjust::Enabled(W::None, c, base), "Restore is not, nor is no widget");
    // The size at its limits.
    Controls::Tuning smallest = SizedTuning(4);
    Controls::Tuning largest = SizedTuning(14);
    CHECK_MSG(!Adjust::Enabled(W::SizeMinus, c, smallest) && Adjust::Enabled(W::SizePlus, c, smallest), "at 0.4 only the plus works");
    CHECK_MSG(Adjust::Enabled(W::SizeMinus, c, largest) && !Adjust::Enabled(W::SizePlus, c, largest), "at 1.4 only the minus does");
    for (const W widget : {W::SizeMinus}) {
        const Adjust::Outcome out = Adjust::Act(widget, c, smallest, true);
        CHECK_MSG(!out.acted && !out.tuningChanged && Adjust::SameKept(out.tuning, smallest) && out.unlocked && !out.leave, "a minus at 0.4 does nothing, and does not even re-lock");
    }
    {
        const Adjust::Outcome out = Adjust::Act(W::SizePlus, c, largest, true);
        CHECK_MSG(!out.acted && !out.tuningChanged && out.unlocked, "a plus at 1.4 does nothing and leaves the lock as it was");
    }
    // The opacity at its limits.
    Controls::Tuning faint;
    faint.opacitySteps = 1;
    Controls::Tuning solid;
    solid.opacitySteps = 9;
    CHECK_MSG(!Adjust::Enabled(W::OpacityMinus, c, faint) && Adjust::Enabled(W::OpacityPlus, c, faint), "at 0.2 only the plus works");
    CHECK_MSG(Adjust::Enabled(W::OpacityMinus, c, solid) && !Adjust::Enabled(W::OpacityPlus, c, solid), "at 1.8 only the minus does");
    CHECK_MSG(!Adjust::Act(W::OpacityMinus, c, faint, true).acted && !Adjust::Act(W::OpacityPlus, c, solid, true).acted, "and a press at the limit does nothing");
    // Restore with a move works; Back and the lock always do.
    Controls::Tuning moved;
    moved.move[static_cast<std::size_t>(Controls::Id::Menu)] = glm::dvec2(-10.0, 4.0);
    CHECK_MSG(Adjust::Enabled(W::Restore, c, moved) && Adjust::HasMove(moved) && !Adjust::HasMove(base), "Restore is enabled by a moved control");
    CHECK_MSG(Adjust::Enabled(W::Lock, c, smallest) && Adjust::Enabled(W::Back, c, largest), "the lock and Back work at any limit");
    // The widget a press is on, None, does nothing.
    CHECK_MSG(!Adjust::Act(W::None, c, base, false).acted, "no widget, no act");
}

void TheSizeChainEndsOnTheLimits() {
    const Loaded& d = Data();
    if (!d.ok) return;
    using W = Adjust::Widget;
    const Controls::Rules& c = d.controls;
    Controls::Tuning tuning;
    CHECK_MSG(tuning.Size(c) == 7.0 / 10.0, "it starts at 0.7");
    // Up to 1.4: seven steps, each exactly n / 10 by one division, then inert.
    for (int steps = 8; steps <= 14; ++steps) {
        const Adjust::Outcome out = Adjust::Act(W::SizePlus, c, tuning, false);
        CHECK_MSG(out.acted && out.tuningChanged, "a plus steps up to " + Num(steps / 10.0));
        tuning = out.tuning;
        CHECK_MSG(tuning.SizeStepsIn(c) == steps && tuning.Size(c) == static_cast<double>(steps) / 10.0, "and the size is exactly " + std::to_string(steps) + " / 10");
    }
    CHECK_MSG(tuning.Size(c) == 1.4 && !Adjust::Act(W::SizePlus, c, tuning, false).acted, "it ends exactly on 1.4 and the next plus is inert");
    // Down to 0.4: ten steps.
    for (int steps = 13; steps >= 4; --steps) {
        const Adjust::Outcome out = Adjust::Act(W::SizeMinus, c, tuning, false);
        CHECK_MSG(out.acted && out.tuningChanged, "a minus steps down to " + Num(steps / 10.0));
        tuning = out.tuning;
        CHECK_MSG(tuning.SizeStepsIn(c) == steps && tuning.Size(c) == static_cast<double>(steps) / 10.0, "and the size is exactly " + std::to_string(steps) + " / 10");
    }
    CHECK_MSG(tuning.Size(c) == 0.4 && !Adjust::Act(W::SizeMinus, c, tuning, false).acted, "and ends exactly on 0.4");
    // The opacity: 1.0 up to 1.8 (four steps of 0.2) and down to 0.2 (eight).
    Controls::Tuning opacity;
    CHECK_MSG(opacity.Opacity(c) == 5.0 / 5.0, "the opacity starts at 1.0");
    for (int steps = 6; steps <= 9; ++steps) {
        opacity = Adjust::Act(W::OpacityPlus, c, opacity, false).tuning;
        CHECK_MSG(opacity.OpacityStepsIn(c) == steps && opacity.Opacity(c) == static_cast<double>(steps) / 5.0, "opacity " + std::to_string(steps) + " / 5");
    }
    CHECK_MSG(opacity.Opacity(c) == 1.8 && !Adjust::Act(W::OpacityPlus, c, opacity, false).acted, "ends exactly on 1.8");
    for (int steps = 8; steps >= 1; --steps) {
        opacity = Adjust::Act(W::OpacityMinus, c, opacity, false).tuning;
        CHECK_MSG(opacity.OpacityStepsIn(c) == steps && opacity.Opacity(c) == static_cast<double>(steps) / 5.0, "opacity " + std::to_string(steps) + " / 5");
    }
    CHECK_MSG(opacity.Opacity(c) == 0.2 && !Adjust::Act(W::OpacityMinus, c, opacity, false).acted, "and ends exactly on 0.2");
    // A step back to the default is the default again (nothing kept: a later change of the default reaches the player).
    const Adjust::Outcome up = Adjust::Act(W::SizePlus, c, Controls::Tuning{}, false);
    const Adjust::Outcome back = Adjust::Act(W::SizeMinus, c, up.tuning, false);
    CHECK_MSG(up.tuning.sizeSteps && !back.tuning.sizeSteps && back.tuning.IsDefault(c), "up and down again is the default, with no size kept");
    // Under another look the default is that look's: balanced is 0.8, and a step down from it is 0.7 which is NOT its default.
    Controls::Tuning balanced;
    balanced.preset = Controls::Preset::Balanced;
    const Adjust::Outcome fromBalanced = Adjust::Act(W::SizeMinus, c, balanced, false);
    CHECK_MSG(fromBalanced.tuning.SizeStepsIn(c) == 7 && fromBalanced.tuning.sizeSteps == std::optional<int>(7) && fromBalanced.tuning.preset == std::optional<Controls::Preset>(Controls::Preset::Balanced),
              "under preset=balanced (0.8) a step down is 0.7, kept, and the look is still balanced");
    const Adjust::Outcome backUp = Adjust::Act(W::SizePlus, c, fromBalanced.tuning, false);
    CHECK_MSG(!backUp.tuning.sizeSteps && backUp.tuning.SizeStepsIn(c) == 8 && backUp.tuning.preset == std::optional<Controls::Preset>(Controls::Preset::Balanced),
              "and back up it is the look's own default again, nothing kept");
    Controls::Tuning mr;
    mr.preset = Controls::Preset::Mr;
    const Adjust::Outcome fromMr = Adjust::Act(W::SizeMinus, c, mr, false);
    CHECK_MSG(fromMr.tuning.SizeStepsIn(c) == 9 && !Adjust::Act(W::SizePlus, c, fromMr.tuning, false).tuning.sizeSteps, "preset=mr (1.0): down is 0.9 and up again is the default, nothing kept");
}

void ASizeStepRelocks() {
    const Loaded& d = Data();
    if (!d.ok) return;
    using W = Adjust::Widget;
    const Controls::Rules& c = d.controls;
    for (const bool unlockedBefore : {false, true}) {
        for (const W widget : {W::SizeMinus, W::SizePlus}) {
            const Adjust::Outcome out = Adjust::Act(widget, c, Controls::Tuning{}, unlockedBefore);
            CHECK_MSG(out.acted && out.tuningChanged && !out.unlocked && !out.leave, std::string("a size step re-locks, whatever the lock was: ") + Adjust::NameOf(widget) + (unlockedBefore ? " (open)" : " (shut)"));
        }
    }
    // At a limit the step is inert, and an inert press does not even re-lock (it did nothing).
    CHECK_MSG(Adjust::Act(W::SizeMinus, c, SizedTuning(4), true).unlocked && Adjust::Act(W::SizePlus, c, SizedTuning(14), true).unlocked, "a press at the limit leaves the lock as it was");
}

void AnOpacityStepDoesNotRelock() {
    const Loaded& d = Data();
    if (!d.ok) return;
    using W = Adjust::Widget;
    const Controls::Rules& c = d.controls;
    for (const bool unlockedBefore : {false, true}) {
        for (const W widget : {W::OpacityMinus, W::OpacityPlus}) {
            const Adjust::Outcome out = Adjust::Act(widget, c, Controls::Tuning{}, unlockedBefore);
            CHECK_MSG(out.acted && out.tuningChanged && out.unlocked == unlockedBefore && !out.leave, std::string("an opacity step leaves the lock as it was: ") + Adjust::NameOf(widget) + (unlockedBefore ? " (open)" : " (shut)"));
        }
    }
}

void TheLockTogglesAndBackLeavesAndNothingElse() {
    const Loaded& d = Data();
    if (!d.ok) return;
    using W = Adjust::Widget;
    const Controls::Rules& c = d.controls;
    Controls::Tuning tuning = SizedTuning(11);
    tuning.opacitySteps = 3;
    const Adjust::Outcome open = Adjust::Act(W::Lock, c, tuning, false);
    CHECK_MSG(open.acted && open.unlocked && !open.tuningChanged && Adjust::SameKept(open.tuning, tuning) && !open.leave, "the lock opens, and changes no number");
    const Adjust::Outcome shut = Adjust::Act(W::Lock, c, tuning, open.unlocked);
    CHECK_MSG(shut.acted && !shut.unlocked && !shut.tuningChanged, "and shuts again");
    const Adjust::Outcome leave = Adjust::Act(W::Back, c, tuning, true);
    CHECK_MSG(leave.acted && leave.leave && !leave.tuningChanged && leave.unlocked && Adjust::SameKept(leave.tuning, tuning), "Back leaves and changes nothing, not even the lock (the screen is gone)");
}

void RestoreIsEnabledOnlyWhenAMoveExistsAndKeepsSizeAndOpacity() {
    const Loaded& d = Data();
    if (!d.ok) return;
    using W = Adjust::Widget;
    const Controls::Rules& c = d.controls;
    // With no move: inert, the tuning and the lock untouched.
    Controls::Tuning sized = SizedTuning(12);
    sized.opacitySteps = 2;
    const Adjust::Outcome nothing = Adjust::Act(W::Restore, c, sized, true);
    CHECK_MSG(!nothing.acted && !nothing.tuningChanged && nothing.unlocked && Adjust::SameKept(nothing.tuning, sized), "no move: Restore is inert and re-locks nothing");
    // With moves: every one cleared, the size and the opacity kept, re-locked.
    Controls::Tuning moved = sized;
    moved.move[static_cast<std::size_t>(Controls::Id::Left)] = glm::dvec2(5.0, -2.5);
    moved.move[static_cast<std::size_t>(Controls::Id::Clear)] = glm::dvec2(-12.0, 30.0);
    CHECK_MSG(Adjust::HasMove(moved), "two controls are moved");
    const Adjust::Outcome out = Adjust::Act(W::Restore, c, moved, true);
    CHECK_MSG(out.acted && out.tuningChanged && !out.unlocked && !out.leave, "Restore acts, changes the tuning and re-locks");
    CHECK_MSG(!Adjust::HasMove(out.tuning), "and every move is gone");
    CHECK_MSG(out.tuning.SizeStepsIn(c) == 12 && out.tuning.OpacityStepsIn(c) == 2 && out.tuning.Size(c) == 1.2 && out.tuning.Opacity(c) == 2.0 / 5.0, "while the size (1.2) and the opacity (0.4) are kept");
    CHECK_MSG(!Adjust::Enabled(W::Restore, c, out.tuning), "and Restore is inert again");
    // A single move.
    Controls::Tuning one;
    one.move[static_cast<std::size_t>(Controls::Id::Menu)] = glm::dvec2(0.0, 0.1);
    CHECK_MSG(Adjust::HasMove(one) && Adjust::Act(W::Restore, c, one, false).tuningChanged, "a move of a tenth is a move");
}

void TheReadoutsAreTheValueToOneDecimal() {
    const Loaded& d = Data();
    if (!d.ok) return;
    const Controls::Rules& c = d.controls;
    // Size: 0.4 to 1.4 by 0.1; opacity: 0.2 to 1.8 by 0.2: the VALUE (n / 10 and n / 5), one decimal, digits and the point only.
    const char* sizes[] = {"0.4", "0.5", "0.6", "0.7", "0.8", "0.9", "1.0", "1.1", "1.2", "1.3", "1.4"};
    for (int steps = 4; steps <= 14; ++steps) {
        CHECK_MSG(Adjust::ReadoutText(c, SizedTuning(steps), Adjust::Row::Size) == sizes[steps - 4], std::string("size step ") + std::to_string(steps) + " reads " + sizes[steps - 4] + ": " + Adjust::ReadoutText(c, SizedTuning(steps), Adjust::Row::Size));
    }
    const char* opacities[] = {"0.2", "0.4", "0.6", "0.8", "1.0", "1.2", "1.4", "1.6", "1.8"};
    for (int steps = 1; steps <= 9; ++steps) {
        Controls::Tuning tuning;
        tuning.opacitySteps = steps;
        CHECK_MSG(Adjust::ReadoutText(c, tuning.Clamped(c), Adjust::Row::Opacity) == opacities[steps - 1], std::string("opacity step ") + std::to_string(steps) + " reads " + opacities[steps - 1]);
    }
    CHECK_MSG(Adjust::ReadoutText(c, Controls::Tuning{}, Adjust::Row::Size) == "0.7" && Adjust::ReadoutText(c, Controls::Tuning{}, Adjust::Row::Opacity) == "1.0", "the defaults read 0.7 and 1.0");
    // Under a look: the readout is the size in force.
    Controls::Tuning mr;
    mr.preset = Controls::Preset::Mr;
    CHECK_MSG(Adjust::ReadoutText(c, mr, Adjust::Row::Size) == "1.0", "preset=mr reads 1.0");
    for (int steps = 4; steps <= 14; ++steps) {
        const std::string text = Adjust::ReadoutText(c, SizedTuning(steps), Adjust::Row::Size);
        bool onlyDigits = true;
        for (const char ch : text) onlyDigits = onlyDigits && ((ch >= '0' && ch <= '9') || ch == '.');
        CHECK_MSG(onlyDigits && text.size() == 3u, "digits and the point only, three characters: " + text);
    }
}

// ---- the pictures --------------------------------------------------------------------------------------------------------------------------

void TheOutlinesBreatheInTicksNotWallClock() {
    const Loaded& d = Data();
    if (!d.ok) return;
    const Adjust::Rules::Unlocked& u = d.adjust.unlocked;
    // A triangle over 48 ticks between 110 - 70 = 40 and 110 + 70 = 180: low at the start, high half way, low again at the end of the breath.
    const std::pair<int, int> expected[] = {{0, 40}, {6, 75}, {12, 110}, {18, 145}, {24, 180}, {30, 145}, {36, 110}, {42, 75}, {47, 45}, {48, 40}, {72, 180}, {96, 40}, {120, 180}};
    for (const auto& [ticks, alpha] : expected) {
        const int got = Adjust::OutlineAlphaByte(u, ticks);
        CHECK_MSG(std::abs(got - alpha) <= 1, "tick " + std::to_string(ticks) + " is alpha " + std::to_string(alpha) + ": " + std::to_string(got));
    }
    // Periodic, bounded, and a function of the tick alone.
    int low = 255;
    int high = 0;
    for (int ticks = -100; ticks < 400; ++ticks) {
        const int alpha = Adjust::OutlineAlphaByte(u, ticks);
        low = std::min(low, alpha);
        high = std::max(high, alpha);
        CHECK_MSG(alpha == Adjust::OutlineAlphaByte(u, ticks + 48) && alpha == Adjust::OutlineAlphaByte(u, ticks - 48), "tick " + std::to_string(ticks) + " repeats every 48");
    }
    CHECK_MSG(low == 40 && high == 180, "it breathes between 40 and 180: " + std::to_string(low) + " to " + std::to_string(high));
    // Every step is a small one (a breath, not a flash).
    for (int ticks = 0; ticks < 96; ++ticks) CHECK_MSG(std::abs(Adjust::OutlineAlphaByte(u, ticks + 1) - Adjust::OutlineAlphaByte(u, ticks)) <= 12, "tick " + std::to_string(ticks) + " moves by at most 12 bytes");
}

void FrameStripsAreTheFourSidesOfAFrame() {
    const Hud::Rect rect{glm::dvec2(10.0, 20.0), glm::dvec2(30.0, 40.0)};
    // Inside: the strips lie within the rectangle and cover its edge ring exactly once.
    {
        const std::array<Hud::Rect, 4> strips = Adjust::FrameStrips(rect, 2.0, false);
        double area = 0.0;
        for (const Hud::Rect& strip : strips) {
            CHECK_MSG(WhollyInside(strip, rect), "an inside strip is inside the rectangle");
            area += strip.size.x * strip.size.y;
        }
        CHECK_MSG(Near(area, 30.0 * 40.0 - 26.0 * 36.0, 1e-9), "and the four cover the ring: " + Num(area));
        CHECK(NearRect(strips[0], Hud::Rect{glm::dvec2(10.0, 20.0), glm::dvec2(30.0, 2.0)}) && NearRect(strips[1], Hud::Rect{glm::dvec2(10.0, 58.0), glm::dvec2(30.0, 2.0)}));
        CHECK(NearRect(strips[2], Hud::Rect{glm::dvec2(10.0, 22.0), glm::dvec2(2.0, 36.0)}) && NearRect(strips[3], Hud::Rect{glm::dvec2(38.0, 22.0), glm::dvec2(2.0, 36.0)}));
        for (std::size_t i = 0; i < 4; ++i) {
            for (std::size_t j = i + 1; j < 4; ++j) CHECK_MSG(Gap(strips[i], strips[j]) >= 0.0, "no two strips overlap");
        }
    }
    // Outside: the rectangle's own edge is the frame's inner edge.
    {
        const std::array<Hud::Rect, 4> strips = Adjust::FrameStrips(rect, 1.0, true);
        CHECK(NearRect(strips[0], Hud::Rect{glm::dvec2(9.0, 19.0), glm::dvec2(32.0, 1.0)}) && NearRect(strips[1], Hud::Rect{glm::dvec2(9.0, 60.0), glm::dvec2(32.0, 1.0)}));
        CHECK(NearRect(strips[2], Hud::Rect{glm::dvec2(9.0, 20.0), glm::dvec2(1.0, 40.0)}) && NearRect(strips[3], Hud::Rect{glm::dvec2(40.0, 20.0), glm::dvec2(1.0, 40.0)}));
        for (const Hud::Rect& strip : strips) CHECK_MSG(Gap(strip, rect) >= 0.0, "an outside strip does not enter the rectangle");
    }
    // A frame thicker than the rectangle is a solid, never a negative size.
    for (const Hud::Rect& strip : Adjust::FrameStrips(Hud::Rect{glm::dvec2(0.0), glm::dvec2(2.0, 2.0)}, 5.0, false)) CHECK(strip.size.x >= 0.0 && strip.size.y >= 0.0);
}

// What the pieces of a screen are, by element.
std::vector<Adjust::Piece> Of(const std::vector<Adjust::Piece>& pieces, Adjust::Element element) {
    std::vector<Adjust::Piece> some;
    for (const Adjust::Piece& piece : pieces) {
        if (piece.element == element) some.push_back(piece);
    }
    return some;
}

void ThePreviewIsTheLevelsOwnLayoutWithClearForced() {
    const Loaded& d = Data();
    if (!d.ok) return;
    const glm::dvec2 view = ViewOf(16.0 / 9.0);
    for (const int steps : {4, 7, 10, 14}) {
        Controls::Tuning tuning = SizedTuning(steps);
        tuning.opacitySteps = steps == 14 ? 9 : steps == 4 ? 1 : 5;
        tuning = tuning.Clamped(d.controls);
        const std::vector<Adjust::Piece> pieces = Adjust::Pieces(d.adjust, d.controls, d.hud, tuning, view, kNoInsets, Adjust::Scene{});
        const std::vector<Adjust::Piece> preview = Of(pieces, Adjust::Element::Preview);
        const Controls::Layout layout = Controls::LayOut(Controls::Style::Modern, d.controls, d.hud, tuning, view, kNoInsets, kSettled);
        CHECK_MSG(preview.size() == 5u, "all five controls are drawn, size " + Num(steps / 10.0) + ": " + std::to_string(preview.size()));
        for (std::size_t i = 0; i < preview.size() && i < 5; ++i) {
            const Controls::Id id = Controls::kClaimOrder[i];
            CHECK_MSG(preview[i].control == std::optional<Controls::Id>(id), std::string("in the claim order: ") + Controls::KeyOf(id));
            CHECK_MSG(NearRect(preview[i].rect, layout.At(id).rect, 1e-12), std::string(Controls::KeyOf(id)) + " is where Controls::LayOut puts it at size " + Num(steps / 10.0) + ": " + Show(preview[i].rect));
            CHECK_MSG(preview[i].faceInset == d.controls.faceInset, std::string(Controls::KeyOf(id)) + " is drawn into the controls' face");
        }
    }
    // The alpha as the level draws it settled: the pads' byte through the idle scale and the opacity (a whole byte), a corner button's fraction.
    const auto alphas = [&](int opacitySteps) {
        Controls::Tuning tuning;
        tuning.opacitySteps = opacitySteps;
        tuning = tuning.Clamped(d.controls);
        std::vector<double> out;
        for (const Adjust::Piece& piece : Of(Adjust::Pieces(d.adjust, d.controls, d.hud, tuning, view, kNoInsets, Adjust::Scene{}), Adjust::Element::Preview)) out.push_back(piece.alpha);
        return out;
    };
    const std::vector<double> base = alphas(5);
    CHECK_MSG(base.size() == 5u, "five at the base");
    for (const double a : base) CHECK_MSG(Near(a, 120.0 / 255.0, 1e-12), "every control is the HUD's own 120 of 255 at the base opacity: " + Num(a));
    const std::vector<double> faint = alphas(1);
    const std::vector<double> solid = alphas(9);
    CHECK_MSG(faint.size() == 5u && solid.size() == 5u, "and five at the limits");
    if (faint.size() == 5u && solid.size() == 5u) {
        CHECK_MSG(Near(faint[0], 24.0 / 255.0, 1e-12) && Near(faint[1], 24.0 / 255.0, 1e-12), "the arrows at opacity 0.2: 24 of 255");
        CHECK_MSG(Near(solid[0], 216.0 / 255.0, 1e-12) && Near(solid[1], 216.0 / 255.0, 1e-12), "and at 1.8: 216 of 255");
        for (std::size_t i = 2; i < 5; ++i) {
            CHECK_MSG(Near(faint[i], 120.0 / 255.0 * 0.2, 1e-12) && Near(solid[i], std::min(1.0, 120.0 / 255.0 * 1.8), 1e-12), std::string("the corner buttons follow the opacity: ") + Controls::KeyOf(Controls::kClaimOrder[i]));
        }
    }
    // The function is the same one the pieces call.
    for (const Controls::Id id : Controls::kClaimOrder) CHECK(Near(Adjust::PreviewAlpha(id, d.controls, d.hud, Controls::Tuning{}), 120.0 / 255.0, 1e-12));
    // Moved controls are drawn where they are moved (a flag or a file put them there): the preview shows the layout in force.
    Controls::Tuning moved;
    moved.move[static_cast<std::size_t>(Controls::Id::Menu)] = glm::dvec2(-20.0, 30.0);
    const std::vector<Adjust::Piece> pieces = Of(Adjust::Pieces(d.adjust, d.controls, d.hud, moved, view, kNoInsets, Adjust::Scene{}), Adjust::Element::Preview);
    const Controls::Layout movedLayout = Controls::LayOut(Controls::Style::Modern, d.controls, d.hud, moved, view, kNoInsets, kSettled);
    CHECK_MSG(pieces.size() == 5u && NearRect(pieces[3].rect, movedLayout.At(Controls::Id::Menu).rect, 1e-12) && !NearRect(pieces[3].rect, Controls::LayOut(Controls::Style::Modern, d.controls, d.hud, Controls::Tuning{}, view, kNoInsets, kSettled).At(Controls::Id::Menu).rect, 1e-3),
              "a moved control is drawn where it was moved");
}

void ThePreviewFlagIsTheScenesAloneAndForcesClearShown() {
    const Loaded& d = Data();
    if (!d.ok) return;
    const glm::dvec2 view = ViewOf(16.0 / 9.0);
    Adjust::Scene scene;
    CHECK_MSG(scene.previewClearShown && !scene.unlocked && scene.held == Adjust::Widget::None && scene.ticks == 0, "a scene starts locked, nothing held, clear shown");
    for (const bool shown : {true, false}) {
        scene.previewClearShown = shown;
        for (const bool unlocked : {false, true}) {
            scene.unlocked = unlocked;
            const std::vector<Adjust::Piece> pieces = Adjust::Pieces(d.adjust, d.controls, d.hud, Controls::Tuning{}, view, kNoInsets, scene);
            const std::vector<Adjust::Piece> preview = Of(pieces, Adjust::Element::Preview);
            bool clear = false;
            for (const Adjust::Piece& piece : preview) clear = clear || piece.control == std::optional<Controls::Id>(Controls::Id::Clear);
            CHECK_MSG(clear == shown && preview.size() == (shown ? 5u : 4u), std::string("clear-portals is in the preview exactly when the flag says (") + (shown ? "shown" : "not shown") + ")");
            // And its cues go with it.
            bool cue = false;
            for (const Adjust::Piece& piece : pieces) cue = cue || ((piece.element == Adjust::Element::Outline || piece.element == Adjust::Element::HitFrame) && piece.control == std::optional<Controls::Id>(Controls::Id::Clear));
            CHECK_MSG(cue == (shown && unlocked), "and so are its outline and hit frame, while unlocked");
        }
    }
}

void WhileUnlockedTheEditorDrawsEveryHitRectAndTheCharacterRowGuide() {
    const Loaded& d = Data();
    if (!d.ok) return;
    for (const Aspect& aspect : {kAspects[0], kAspects[4], kAspects[7]}) {
        const glm::dvec2 view = ViewOf(aspect.ratio);
        for (const int steps : {4, 7, 14}) {
            const Controls::Tuning tuning = SizedTuning(steps);
            const std::string at = std::string(" at ") + aspect.name + ", size " + Num(steps / 10.0);
            Adjust::Scene scene;
            const std::vector<Adjust::Piece> locked = Adjust::Pieces(d.adjust, d.controls, d.hud, tuning, view, kNoInsets, scene);
            scene.unlocked = true;
            scene.ticks = 12;
            const std::vector<Adjust::Piece> open = Adjust::Pieces(d.adjust, d.controls, d.hud, tuning, view, kNoInsets, scene);
            // Locked: backdrop, five controls, seven widgets, two readouts - and not one cue.
            CHECK_MSG(locked.size() == 1u + 5u + 7u + 2u && Of(locked, Adjust::Element::HitFrame).empty() && Of(locked, Adjust::Element::Outline).empty() && Of(locked, Adjust::Element::Guide).empty(),
                      std::string("locked, the screen draws no cue") + at + ": " + std::to_string(locked.size()));
            // Unlocked: four strips of a hit frame and four of an outline for each of five, and one guide.
            const std::vector<Adjust::Piece> frames = Of(open, Adjust::Element::HitFrame);
            const std::vector<Adjust::Piece> outlines = Of(open, Adjust::Element::Outline);
            const std::vector<Adjust::Piece> guide = Of(open, Adjust::Element::Guide);
            CHECK_MSG(frames.size() == 20u && outlines.size() == 20u && guide.size() == 1u && open.size() == locked.size() + 41u, std::string("unlocked: 20 hit-frame strips, 20 outline strips and a guide") + at);
            const Controls::Layout layout = Controls::LayOut(Controls::Style::Modern, d.controls, d.hud, tuning, view, kNoInsets, kSettled);
            for (std::size_t i = 0; i < 5; ++i) {
                const Controls::Id id = Controls::kClaimOrder[i];
                const Controls::Frame& frame = layout.At(id);
                // The hit frame's strips are the four sides of LayOut's own hit rectangle, inside its edge.
                const std::array<Hud::Rect, 4> hit = Adjust::FrameStrips(frame.hit.rect, d.adjust.unlocked.hitFrameThicknessUnits, false);
                const std::array<Hud::Rect, 4> outline = Adjust::FrameStrips(frame.rect, d.adjust.unlocked.outlineThicknessUnits, true);
                for (std::size_t side = 0; side < 4; ++side) {
                    CHECK_MSG(frames[i * 4 + side].control == std::optional<Controls::Id>(id) && NearRect(frames[i * 4 + side].rect, hit[side], 1e-12), std::string(Controls::KeyOf(id)) + "'s hit frame is LayOut's own hit rectangle" + at);
                    CHECK_MSG(outlines[i * 4 + side].control == std::optional<Controls::Id>(id) && NearRect(outlines[i * 4 + side].rect, outline[side], 1e-12), std::string(Controls::KeyOf(id)) + "'s outline is round its box" + at);
                    CHECK_MSG(frames[i * 4 + side].colour == (Adjust::Colour{255, 255, 255, 0x22}), "a hit frame is a faint white, alpha 0x22");
                    CHECK_MSG(outlines[i * 4 + side].colour == (Adjust::Colour{255, 255, 255, Adjust::OutlineAlphaByte(d.adjust.unlocked, 12)}) && outlines[i * 4 + side].colour[3] == 110,
                              "an outline is white at the breath's alpha for tick 12 (110)");
                }
            }
            // The guide: across the whole width, a thin line centred on the character's row, 206 - the census's own number.
            const Adjust::Piece& line = guide[0];
            CHECK_MSG(Near(line.rect.min.x, 0.0) && Near(line.rect.size.x, view.x) && Near(line.rect.Centre().y, 206.0, 1e-9) && Near(line.rect.size.y, 0.7, 1e-12), std::string("the guide is a 0.7-unit line across the view at y 206") + at);
            CHECK_MSG(line.colour == (Adjust::Colour{255, 215, 110, 170}), "in the data's colour");
            CHECK_MSG(Near(d.controls.census.characterRowUnits, 206.0), "and 206 is the census's own character_row_units");
        }
    }
    // The outlines breathe with the state's ticks: tick 0 and tick 24 differ, tick 48 is tick 0.
    Adjust::Scene scene;
    scene.unlocked = true;
    const glm::dvec2 view = ViewOf(16.0 / 9.0);
    const auto outlineAlpha = [&](int ticks) {
        scene.ticks = ticks;
        return Of(Adjust::Pieces(d.adjust, d.controls, d.hud, Controls::Tuning{}, view, kNoInsets, scene), Adjust::Element::Outline).front().colour[3];
    };
    CHECK_MSG(outlineAlpha(0) == 40 && outlineAlpha(24) == 180 && outlineAlpha(48) == outlineAlpha(0), "the pieces breathe on the ticks they are given");
    // Drawn over the preview and under the widgets: the order of the list.
    scene.ticks = 0;
    const std::vector<Adjust::Piece> pieces = Adjust::Pieces(d.adjust, d.controls, d.hud, Controls::Tuning{}, view, kNoInsets, scene);
    std::size_t lastPreview = 0;
    std::size_t firstCue = pieces.size();
    std::size_t firstTile = pieces.size();
    for (std::size_t i = 0; i < pieces.size(); ++i) {
        if (pieces[i].element == Adjust::Element::Preview) lastPreview = i;
        if ((pieces[i].element == Adjust::Element::HitFrame || pieces[i].element == Adjust::Element::Outline || pieces[i].element == Adjust::Element::Guide) && firstCue == pieces.size()) firstCue = i;
        if (pieces[i].element == Adjust::Element::Tile && firstTile == pieces.size()) firstTile = i;
    }
    CHECK_MSG(pieces.front().element == Adjust::Element::Backdrop && lastPreview < firstCue && firstCue < firstTile, "the backdrop, then the controls, then the cues, then the widgets");
    CHECK_MSG(pieces.back().element == Adjust::Element::Readout, "and the readouts last");
}

void TheTilesAreDrawnInTheOrderTheyAreClaimedAtTheInertAlphaAndTheHeldTint() {
    const Loaded& d = Data();
    if (!d.ok) return;
    const glm::dvec2 view = ViewOf(16.0 / 9.0);
    // At the defaults: Restore is inert (no move), everything else works.
    {
        const std::vector<Adjust::Piece> tiles = Of(Adjust::Pieces(d.adjust, d.controls, d.hud, Controls::Tuning{}, view, kNoInsets, Adjust::Scene{}), Adjust::Element::Tile);
        CHECK_MSG(tiles.size() == 7u, "seven widgets");
        for (std::size_t i = 0; i < tiles.size() && i < 7; ++i) {
            const Adjust::Widget widget = Adjust::kWidgets[i];
            CHECK_MSG(tiles[i].widget == widget, std::string("in kWidgets' order: ") + Adjust::NameOf(widget));
            const double expected = widget == Adjust::Widget::Restore ? 90.0 / 255.0 : 1.0;
            CHECK_MSG(Near(tiles[i].alpha, expected, 1e-12), std::string(Adjust::NameOf(widget)) + " is drawn at " + Num(expected) + " at the defaults: " + Num(tiles[i].alpha));
            CHECK_MSG(tiles[i].tintByte == 255 && tiles[i].picture == *Adjust::PictureOf(widget, false) && Near(tiles[i].faceInset, widget == Adjust::Widget::Back ? 0.1171875 : 0.140625, 1e-12), std::string(Adjust::NameOf(widget)) + ": untinted, its picture, its face");
        }
    }
    // At the limits: the size minus at 0.4 and the opacity plus at 1.8 are inert.
    {
        Controls::Tuning tuning = SizedTuning(4);
        tuning.opacitySteps = 9;
        const std::vector<Adjust::Piece> tiles = Of(Adjust::Pieces(d.adjust, d.controls, d.hud, tuning.Clamped(d.controls), view, kNoInsets, Adjust::Scene{}), Adjust::Element::Tile);
        CHECK_MSG(tiles.size() == 7u && Near(tiles[0].alpha, 90.0 / 255.0) && Near(tiles[1].alpha, 1.0) && Near(tiles[2].alpha, 1.0) && Near(tiles[3].alpha, 90.0 / 255.0),
                  "size_minus at 0.4 and opacity_plus at 1.8 are drawn at 90 of 255");
    }
    // A moved control makes Restore live.
    {
        Controls::Tuning tuning;
        tuning.move[static_cast<std::size_t>(Controls::Id::Reset)] = glm::dvec2(3.0, 3.0);
        const std::vector<Adjust::Piece> tiles = Of(Adjust::Pieces(d.adjust, d.controls, d.hud, tuning, view, kNoInsets, Adjust::Scene{}), Adjust::Element::Tile);
        CHECK_MSG(tiles.size() == 7u && Near(tiles[5].alpha, 1.0), "Restore is drawn whole once a control is moved");
    }
    // The lock's picture follows its state, and the flat tier's colour is the red one while it is open.
    {
        Adjust::Scene scene;
        scene.unlocked = true;
        const std::vector<Adjust::Piece> tiles = Of(Adjust::Pieces(d.adjust, d.controls, d.hud, Controls::Tuning{}, view, kNoInsets, scene), Adjust::Element::Tile);
        CHECK_MSG(tiles.size() == 7u && tiles[4].picture == Adjust::Picture::LockUnlocked && tiles[4].colour == d.adjust.flatUnlocked && tiles[0].colour == d.adjust.flatTile, "open, the lock shows the open padlock on the red plate, the rest the stone plate");
        scene.unlocked = false;
        const std::vector<Adjust::Piece> shut = Of(Adjust::Pieces(d.adjust, d.controls, d.hud, Controls::Tuning{}, view, kNoInsets, scene), Adjust::Element::Tile);
        CHECK_MSG(shut.size() == 7u && shut[4].picture == Adjust::Picture::LockLocked && shut[4].colour == d.adjust.flatTile, "shut, the closed padlock on the stone one");
    }
    // The held widget is tinted by the press tint, and only it.
    {
        Adjust::Scene scene;
        scene.held = Adjust::Widget::OpacityPlus;
        scene.pressTintByte = 204;
        const std::vector<Adjust::Piece> tiles = Of(Adjust::Pieces(d.adjust, d.controls, d.hud, Controls::Tuning{}, view, kNoInsets, scene), Adjust::Element::Tile);
        CHECK_MSG(tiles.size() == 7u, "seven again");
        for (std::size_t i = 0; i < tiles.size() && i < 7; ++i) {
            const bool held = Adjust::kWidgets[i] == Adjust::Widget::OpacityPlus;
            CHECK_MSG(tiles[i].tintByte == (held ? 204 : 255), std::string(Adjust::NameOf(Adjust::kWidgets[i])) + (held ? " is tinted 204" : " is not tinted"));
            CHECK_MSG(tiles[i].colour[0] == (held ? 205 * 204 / 255 : 205), "and so is its flat colour");
        }
    }
    // The two readouts: centred on the middle, on their row, in the data's font.
    {
        const std::vector<Adjust::Piece> words = Of(Adjust::Pieces(d.adjust, d.controls, d.hud, SizedTuning(9), view, kNoInsets, Adjust::Scene{}), Adjust::Element::Readout);
        CHECK_MSG(words.size() == 2u, "two readouts");
        if (words.size() == 2u) {
            CHECK_MSG(words[0].words == "0.9" && words[1].words == "1.0", "the size and the opacity in force: " + words[0].words + ", " + words[1].words);
            CHECK_MSG(NearPoint(words[0].rect.min, glm::dvec2(view.x * 0.5, 69.12), 1e-9) && NearPoint(words[1].rect.min, glm::dvec2(view.x * 0.5, 102.4), 1e-9), "centred on the middle of each row");
            CHECK_MSG(words[0].font == "Matura84_shadow.fnt" && words[0].unitsPerFontPx == 0.4 && words[0].colour[3] == 255, "in the original's font at 0.4");
        }
    }
}

// ---- the editor glyphs ----------------------------------------------------------------------------------------------------------------------

void TheEditorGlyphsAreEightPicturesOnOnePlate() {
    const double inset = 0.109375;
    const int size = 128;
    std::vector<PadArt::Bitmap> bitmaps;
    std::set<std::vector<std::uint8_t>> pictures;
    for (const PadArt::EditorGlyph glyph : PadArt::kEditorGlyphs) {
        const PadArt::Bitmap bitmap = PadArt::DrawEditor(glyph, size, inset);
        CHECK_MSG(!bitmap.Empty() && bitmap.size == size && bitmap.rgba.size() == static_cast<std::size_t>(size * size * 4), std::string(PadArt::NameOf(glyph)) + " is a 128 px square RGBA");
        pictures.insert(bitmap.rgba);
        bitmaps.push_back(bitmap);
    }
    CHECK_MSG(pictures.size() == 8u && PadArt::kEditorGlyphCount == 8u, "eight different pictures");
    if (bitmaps.size() != 8u) return;
    // One plate: the alpha channel is the same eight times, and the same as the six controls' (the margin is exactly transparent, the body exactly solid).
    const PadArt::Bitmap reference = PadArt::Draw(PadArt::Glyph::WalkLeft, size, inset);
    const int margin = PadArt::BodyMarginPx(size, inset);
    CHECK_MSG(margin == 14, "a 14 px margin of 128");
    for (std::size_t g = 0; g < 8; ++g) {
        bool sameAlpha = true;
        bool clear = true;
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                sameAlpha = sameAlpha && bitmaps[g].At(x, y).a == reference.At(x, y).a;
                const bool inMargin = x < margin || y < margin || x >= size - margin || y >= size - margin;
                if (inMargin && bitmaps[g].At(x, y).a != 0) clear = false;
                // A transparent pixel carries no colour to bleed into a filter.
                if (bitmaps[g].At(x, y).a == 0) clear = clear && bitmaps[g].At(x, y).r == 0 && bitmaps[g].At(x, y).g == 0 && bitmaps[g].At(x, y).b == 0;
            }
        }
        CHECK_MSG(sameAlpha, std::string(PadArt::NameOf(PadArt::kEditorGlyphs[g])) + " has the plate's own alpha channel, the six controls'");
        CHECK_MSG(clear, std::string(PadArt::NameOf(PadArt::kEditorGlyphs[g])) + "'s margin is exactly transparent and no transparent pixel has a colour");
    }
    // The plates: stone for seven, the editing red for the open lock alone. Sampled in the plate away from the glyph: a pixel just inside the rim, mid-left.
    const auto plateAt = [&](std::size_t g) { return bitmaps[g].At(margin + 14, size / 2); };
    for (std::size_t g = 0; g < 8; ++g) {
        const PadArt::Pixel pixel = plateAt(g);
        const bool open = PadArt::kEditorGlyphs[g] == PadArt::EditorGlyph::LockUnlocked;
        CHECK_MSG(pixel.a == 255 && (pixel.r == PadArt::kPlateFill.r) == !open, std::string(PadArt::NameOf(PadArt::kEditorGlyphs[g])) + (open ? " is on the red plate" : " is on the stone plate"));
    }
    CHECK_MSG(plateAt(5).r == PadArt::kEditingFill.r && plateAt(5).g == PadArt::kEditingFill.g && plateAt(5).b == PadArt::kEditingFill.b, "the open lock's plate is the editing red, exactly");
    // The glyph is drawn: light pixels exist inside every plate, and the middle of the open lock's body is the glyph's colour.
    for (std::size_t g = 0; g < 8; ++g) {
        int light = 0;
        for (int y = margin; y < size - margin; ++y) {
            for (int x = margin; x < size - margin; ++x) {
                const PadArt::Pixel p = bitmaps[g].At(x, y);
                if (p.a == 255 && p.r == PadArt::kGlyphColour.r && p.g == PadArt::kGlyphColour.g && p.b == PadArt::kGlyphColour.b) ++light;
            }
        }
        CHECK_MSG(light > 300, std::string(PadArt::NameOf(PadArt::kEditorGlyphs[g])) + " has a glyph on its plate: " + std::to_string(light) + " light pixels");
    }
    // The size pictures differ the way they say: the plus square covers more than the minus one, the full square more than the empty one.
    const auto lightCount = [&](std::size_t g) {
        int light = 0;
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) light += bitmaps[g].At(x, y).r == PadArt::kGlyphColour.r && bitmaps[g].At(x, y).a == 255 ? 1 : 0;
        }
        return light;
    };
    CHECK_MSG(lightCount(1) > 2 * lightCount(0), "the enlarge square is more than twice the shrink one: " + std::to_string(lightCount(1)) + " against " + std::to_string(lightCount(0)));
    CHECK_MSG(lightCount(3) * 2 > lightCount(2) * 3, "the full square is half as much again as the empty one's ring: " + std::to_string(lightCount(3)) + " against " + std::to_string(lightCount(2)));
    // The same call, the same bytes; and a request outside the limits is refused, never half done.
    CHECK_MSG(PadArt::DrawEditor(PadArt::EditorGlyph::Restore, size, inset).rgba == bitmaps[6].rgba, "the same call gives the same bytes");
    CHECK_MSG(PadArt::DrawEditor(PadArt::EditorGlyph::Back, 15, inset).Empty() && PadArt::DrawEditor(PadArt::EditorGlyph::Back, 1025, inset).Empty() && PadArt::DrawEditor(PadArt::EditorGlyph::Back, 128, 0.26).Empty() &&
                  PadArt::DrawEditor(PadArt::EditorGlyph::Back, 128, -0.01).Empty() && PadArt::DrawEditor(PadArt::EditorGlyph::Back, 128, std::numeric_limits<double>::quiet_NaN()).Empty(),
              "a size under 16 or over 1024 and a margin under 0, over a quarter or not a number are refused");
    CHECK_MSG(!PadArt::DrawEditor(PadArt::EditorGlyph::Back, 16, 0.0).Empty() && !PadArt::DrawEditor(PadArt::EditorGlyph::Back, 128, 0.25).Empty(), "and the smallest size and the largest margin are drawn");
    // The editor glyphs are not among the six, which test_mp_padart holds to the plate's invariants.
    CHECK_MSG(PadArt::kGlyphCount == 6u && PadArt::kGlyphs.size() == 6u, "the six are still six");
    // The names, one each.
    std::set<std::string> names;
    for (const PadArt::EditorGlyph glyph : PadArt::kEditorGlyphs) names.insert(PadArt::NameOf(glyph));
    CHECK_MSG(names.size() == 8u && names.count("lock_unlocked") == 1u && names.count("back") == 1u, "eight names");
    // The names are the pictures' keys, so the art tool's files, the data and the glyphs agree.
    for (std::size_t i = 0; i < 8; ++i) CHECK_MSG(std::string(PadArt::NameOf(PadArt::kEditorGlyphs[i])) == Adjust::KeyOf(Adjust::kPictures[i]), "glyph " + std::to_string(i) + " is the picture of the same name");
}

// The editor's glyphs have the same tiers as the five controls' (planning step 192; the same size check as step 184's): Magic Rampage's own file, a glyph the
// game draws itself, then a flat quad. What the layer does with them - the files, the sizes, the warnings - is test_mp_layer's TheEditorGlyphsFallBackTierByTier;
// what is pinned HERE is that every tier has what it needs: the data names a file for the first two of each of the eight pictures, the drawn glyph is there at
// the box's size with the data's margin, and no two pictures of the data are one file.
void TheEditorGlyphsFallBackTierByTier() {
    const Loaded& d = Data();
    if (!d.ok) return;
    const int boxPx = static_cast<int>(std::lround(d.controls.boxPx));
    CHECK_MSG(boxPx == 128, "the box the pictures are made at is 128 px");
    for (std::size_t i = 0; i < Adjust::kPictureCount; ++i) {
        const Adjust::Picture picture = Adjust::kPictures[i];
        const std::string key = Adjust::KeyOf(picture);
        // Tier one: a file of Magic Rampage's, in the folder `editor`, named in the data with its margin.
        const auto& mr = d.adjust.mr[i];
        CHECK_MSG(mr && mr->file == key + ".png" && mr->bodyInset > 0.0 && mr->bodyInset <= 0.25, key + ": tier one is " + key + ".png with a margin");
        // Tier two: the glyph the game draws, in a file of its own with the controls' margin, at the box's size.
        const auto& generated = d.adjust.generated[i];
        CHECK_MSG(generated && generated->file == "adjust_" + key + ".png" && generated->bodyInset == 0.109375 && generated->bodyInset <= PadArt::kMaxBodyInset, key + ": tier two is adjust_" + key + ".png with the controls' margin");
        if (!generated) continue;
        const PadArt::Bitmap bitmap = PadArt::DrawEditor(PadArt::kEditorGlyphs[i], boxPx, generated->bodyInset);
        CHECK_MSG(!bitmap.Empty() && bitmap.size == boxPx && bitmap.At(PadArt::BodyMarginPx(boxPx, generated->bodyInset) - 1, boxPx / 2).a == 0 && bitmap.At(PadArt::BodyMarginPx(boxPx, generated->bodyInset), boxPx / 2).a > 0,
                  key + ": the glyph is drawn at 128 px with its body exactly where the margin says");
        // Tier three: a flat quad on the face, in the data's colour (the open lock's is the red one).
        CHECK_MSG(d.adjust.flatTile[3] == 255 && d.adjust.flatUnlocked[3] == 255, key + ": and a flat quad is the last resort");
        // No two tiers share a file name with another picture's.
        for (std::size_t j = i + 1; j < Adjust::kPictureCount; ++j) {
            CHECK_MSG(d.adjust.mr[i]->file != d.adjust.mr[j]->file && d.adjust.generated[i]->file != d.adjust.generated[j]->file, key + " and " + Adjust::KeyOf(Adjust::kPictures[j]) + " are not one file");
        }
    }
    // The tiers' order is the controls': Magic Rampage's first, the drawn glyph last (the original has no sprite for any of the eight).
    CHECK_MSG(static_cast<int>(Controls::ArtTier::MagicRampage) < static_cast<int>(Controls::ArtTier::Generated), "Magic Rampage's tier is tried before the drawn one");
}

// ---- what the block refuses -------------------------------------------------------------------------------------------------------------

using Path = std::vector<std::string>;

std::filesystem::path ScratchDir() {
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "supersonic-test-mp-adjust";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir;
}

std::string Write(const Supersonic::Json::Value& value) {
    namespace Json = Supersonic::Json;
    switch (value.GetType()) {
    case Json::Type::Null:
        return "null";
    case Json::Type::Bool:
        return value.AsBool() ? "true" : "false";
    case Json::Type::Number: {
        const double n = value.AsNumber();
        if (!std::isfinite(n)) return n < 0.0 ? "-1e999" : "1e999";
        char buffer[64];
        const auto [end, error] = std::to_chars(buffer, buffer + sizeof(buffer), n);
        return error == std::errc{} ? std::string(buffer, end) : std::string("0");
    }
    case Json::Type::String:
        return "\"" + Json::Escape(value.AsString()) + "\"";
    case Json::Type::Array: {
        std::string text = "[";
        for (const Json::Value& item : value.AsArray()) text += (text.size() > 1 ? "," : "") + Write(item);
        return text + "]";
    }
    case Json::Type::Object: {
        std::string text = "{";
        for (const std::string& key : value.OrderedKeys()) text += (text.size() > 1 ? "," : "") + std::string("\"") + Json::Escape(key) + "\":" + Write(value[key]);
        return text + "}";
    }
    }
    return "null";
}

Supersonic::Json::Value Edit(const Supersonic::Json::Value& node, const Path& path, std::size_t depth, const std::optional<Supersonic::Json::Value>& replacement) {
    Supersonic::Json::Object object = node.AsObject();
    if (depth + 1 == path.size()) {
        if (replacement) {
            object.insert_or_assign(path[depth], *replacement);
        } else {
            object.erase(path[depth]);
        }
    } else {
        object.insert_or_assign(path[depth], Edit(node[path[depth]], path, depth + 1, replacement));
    }
    return Supersonic::Json::Value(std::move(object));
}

std::string Dotted(const Path& path) {
    std::string text;
    for (const std::string& part : path) text += (text.empty() ? "" : ".") + part;
    return text;
}

// Every key under `node` that does not begin with an underscore, as a path from `prefix`.
void Collect(const Supersonic::Json::Value& node, Path& prefix, std::vector<Path>& out) {
    for (const std::string& key : node.OrderedKeys()) {
        if (!key.empty() && key[0] == '_') continue;
        prefix.push_back(key);
        out.push_back(prefix);
        if (node[key].IsObject()) Collect(node[key], prefix, out);
        prefix.pop_back();
    }
}

Supersonic::Json::Value Parse(const std::string& text) {
    Supersonic::Json::Value value;
    std::string error;
    CHECK_MSG(Supersonic::Json::Parse(text, value, error), "parses: " + text + ": " + error);
    return value;
}

Supersonic::Json::Value ReadControls() {
    std::ifstream file(kControls, std::ios::binary);
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return Parse(buffer.str());
}

// Loads a document through the three readers: the table's (which ignores both blocks), the options block's and the adjust block's own.
bool LoadAll(const std::string& name, const std::string& text, Controls::Rules& controls, Options::Rules& options, Adjust::Rules& adjust, std::string& error) {
    const std::filesystem::path path = ScratchDir() / name;
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file << text;
    }
    return Controls::LoadRules(path.string(), controls, error) && Options::LoadRules(path.string(), controls, options, error) && Adjust::LoadRules(path.string(), controls, options, adjust, error);
}

void WhatTheAdjustBlockRefuses() {
    namespace Json = Supersonic::Json;
    struct Clean {
        static void Run() {
            std::error_code ec;
            std::filesystem::remove_all(ScratchDir(), ec);
        }
        Clean() { Run(); }
        ~Clean() { Run(); }
    } clean;
    const Json::Value root = ReadControls();
    CHECK_MSG(root["adjust"].IsObject(), "the file has an adjust block");
    if (!root["adjust"].IsObject()) return;

    Controls::Rules controls;
    Options::Rules options;
    Adjust::Rules adjust;
    std::string error;
    CHECK_MSG(LoadAll("whole.json", Write(root), controls, options, adjust, error), "the file rewritten loads through all three readers (so a refusal is the edit's): " + error);

    // Each key of the block removed, one at a time, in a scratch copy: refused, and the message names it. The block itself, too.
    std::vector<Path> paths = {Path{"adjust"}};
    Path prefix = {"adjust"};
    Collect(root["adjust"], prefix, paths);
    CHECK_MSG(paths.size() > 80, "the block has " + std::to_string(paths.size()) + " keys");
    long refused = 0;
    for (const Path& path : paths) {
        Controls::Rules c;
        Options::Rules o;
        Adjust::Rules a;
        std::string why;
        const bool loaded = LoadAll("removed.json", Write(Edit(root, path, 0, std::nullopt)), c, o, a, why);
        const bool named = !loaded && why.find(path.back()) != std::string::npos;
        if (named) ++refused;
        CHECK_MSG(named, "removing " + Dotted(path) + " is refused, naming it: " + why);
    }
    CHECK_MSG(refused == static_cast<long>(paths.size()), std::to_string(refused) + " keys refused of " + std::to_string(paths.size()));

    // The other readers do not read the block: the same file without it loads through the table's and the options block's, and the adjust reader refuses its absence.
    {
        Controls::Rules c;
        Options::Rules o;
        std::string why;
        const std::filesystem::path file = ScratchDir() / "no_adjust.json";
        {
            std::ofstream out(file, std::ios::binary | std::ios::trunc);
            out << Write(Edit(root, Path{"adjust"}, 0, std::nullopt));
        }
        CHECK_MSG(Controls::LoadRules(file.string(), c, why) && Options::LoadRules(file.string(), c, o, why), "the table and the options block load without it: " + why);
        Adjust::Rules a;
        CHECK_MSG(!Adjust::LoadRules(file.string(), c, o, a, why) && why.find("adjust") != std::string::npos, "and the adjust reader refuses its absence, naming it: " + why);
    }
    // A typo in a note, and a key the reader does not know, are not refused.
    {
        Json::Value edited = Edit(root, Path{"adjust", "_sourse"}, 0, Json::Value(std::string("typo")));
        edited = Edit(edited, Path{"adjust", "tile", "extra"}, 0, Json::Value(5.0));
        edited = Edit(edited, Path{"adjust", "tile", "_source"}, 0, Json::Value(std::string("not even an array")));
        Controls::Rules c;
        Options::Rules o;
        Adjust::Rules a;
        std::string why;
        CHECK_MSG(LoadAll("typo.json", Write(edited), c, o, a, why), "a typo in a note and an unknown key are not refused: " + why);
    }

    // A value out of its type or its range is refused, and the values that are in range load.
    struct Row {
        Path path;
        const char* json;
        bool loads;
    };
    const std::vector<Row> rows = {
        {{"adjust", "backdrop_rgba"}, "[1, 2, 3]", false},
        {{"adjust", "backdrop_rgba"}, "[1, 2, 3, 256]", false},
        {{"adjust", "backdrop_rgba"}, "[1, 2, 3, 4.5]", false},
        {{"adjust", "backdrop_rgba"}, "[-1, 2, 3, 4]", false},
        {{"adjust", "backdrop_rgba"}, "[\"1\", 2, 3, 4]", false},
        {{"adjust", "backdrop_rgba"}, "[0, 0, 0, 0]", true},
        {{"adjust", "backdrop_rgba"}, "7", false},
        {{"adjust", "rows", "size"}, "1.1", false},
        {{"adjust", "rows", "size"}, "-0.1", false},
        {{"adjust", "rows", "size"}, "0.3", true},
        {{"adjust", "rows", "opacity"}, "\"a\"", false},
        {{"adjust", "rows"}, "5", false},
        {{"adjust", "tile", "box_px"}, "0", false},
        {{"adjust", "tile", "box_px"}, "-102.4", false},
        {{"adjust", "tile", "box_px"}, "120", true},
        {{"adjust", "tile", "offset_px"}, "0", false},
        {{"adjust", "tile", "face_inset"}, "0.26", false},
        {{"adjust", "tile", "face_inset"}, "-0.01", false},
        {{"adjust", "tile", "face_inset"}, "0", true},
        {{"adjust", "tile", "face_inset"}, "0.25", true},
        {{"adjust", "lock", "box_px"}, "0", false},
        {{"adjust", "lock", "face_inset"}, "0.3", false},
        {{"adjust", "back", "box_px"}, "-1", false},
        {{"adjust", "back", "face_inset"}, "0.5", false},
        {{"adjust", "back", "gap_below_clear_px"}, "-1", false},
        {{"adjust", "back", "gap_below_clear_px"}, "0", true},
        {{"adjust", "back", "gap_below_clear_px"}, "20000", false},
        {{"adjust", "readout", "font"}, "\"\"", false},
        {{"adjust", "readout", "font"}, "5", false},
        {{"adjust", "readout", "units_per_font_px"}, "0", false},
        {{"adjust", "readout", "units_per_font_px"}, "-0.4", false},
        {{"adjust", "readout", "alpha_byte"}, "256", false},
        {{"adjust", "readout", "alpha_byte"}, "-1", false},
        {{"adjust", "readout", "alpha_byte"}, "128", true},
        {{"adjust", "inert", "alpha_byte"}, "300", false},
        {{"adjust", "inert", "alpha_byte"}, "0", true},
        {{"adjust", "flat", "tile_rgba"}, "[1, 2, 3]", false},
        {{"adjust", "flat", "unlocked_rgba"}, "null", false},
        {{"adjust", "unlocked", "outline_thickness_units"}, "0", false},
        {{"adjust", "unlocked", "outline_alpha_byte"}, "256", false},
        {{"adjust", "unlocked", "outline_swing_byte"}, "150", false},          // 110 + 150 is over a byte
        {{"adjust", "unlocked", "outline_swing_byte"}, "-1", false},
        {{"adjust", "unlocked", "outline_swing_byte"}, "0", true},
        {{"adjust", "unlocked", "outline_swing_byte"}, "110", true},
        {{"adjust", "unlocked", "outline_period_ticks"}, "0", false},
        {{"adjust", "unlocked", "outline_period_ticks"}, "47.5", false},
        {{"adjust", "unlocked", "outline_period_ticks"}, "60", true},
        {{"adjust", "unlocked", "outline_rgb"}, "[1, 2, 3, 4]", false},
        {{"adjust", "unlocked", "outline_rgb"}, "[1, 2, 256]", false},
        {{"adjust", "unlocked", "hit_frame_alpha_byte"}, "300", false},
        {{"adjust", "unlocked", "hit_frame_thickness_units"}, "0", false},
        {{"adjust", "unlocked", "hit_frame_rgb"}, "[1, 2]", false},
        {{"adjust", "unlocked", "guide_thickness_units"}, "-1", false},
        {{"adjust", "unlocked", "guide_rgba"}, "[1, 2, 3]", false},
        {{"adjust", "unlocked", "held_outline_thickness_units"}, "0", false},
        {{"adjust", "unlocked", "held_outline_thickness_units"}, "-1", false},
        {{"adjust", "unlocked", "held_outline_thickness_units"}, "1.5", true},
        {{"adjust", "unlocked", "held_outline_alpha_byte"}, "256", false},
        {{"adjust", "unlocked", "held_outline_alpha_byte"}, "-1", false},
        {{"adjust", "unlocked", "held_outline_alpha_byte"}, "200", true},
        {{"adjust", "drag"}, "5", false},
        {{"adjust", "drag", "step_units"}, "0.19", false},
        {{"adjust", "drag", "step_units"}, "0.2", true},
        {{"adjust", "drag", "step_units"}, "8", true},
        {{"adjust", "drag", "step_units"}, "8.01", false},
        {{"adjust", "drag", "step_units"}, "-0.5", false},
        {{"adjust", "drag", "step_units"}, "\"a\"", false},
        // The art: a file is a bare file name, and no two of the sixteen are the same file.
        {{"adjust", "art", "mr", "back", "file"}, "\"back.png\"", true},
        {{"adjust", "art", "mr", "back", "file"}, "\"renamed.png\"", true},
        {{"adjust", "art", "mr", "back", "file"}, "\"\"", false},
        {{"adjust", "art", "mr", "back", "file"}, "5", false},
        {{"adjust", "art", "mr", "back", "file"}, "\"../back.png\"", false},
        {{"adjust", "art", "mr", "back", "file"}, "\"sub/back.png\"", false},
        {{"adjust", "art", "mr", "back", "file"}, "\"C:back.png\"", false},
        {{"adjust", "art", "mr", "back", "file"}, "\"back.png \"", false},
        {{"adjust", "art", "mr", "back", "file"}, "\"CON\"", false},
        {{"adjust", "art", "mr", "back", "file"}, "\"restore.png\"", false},
        {{"adjust", "art", "mr", "back", "file"}, "\"RESTORE.PNG\"", false},
        {{"adjust", "art", "mr", "back", "body_inset"}, "0.25", true},
        {{"adjust", "art", "mr", "back", "body_inset"}, "0.2501", false},
        {{"adjust", "art", "mr", "back", "body_inset"}, "-0.1", false},
        {{"adjust", "art", "mr", "back"}, "[]", false},
        {{"adjust", "art", "mr"}, "5", false},
        {{"adjust", "art", "generated", "restore", "file"}, "\"adjust_restore.png\"", true},
        {{"adjust", "art", "generated", "restore", "file"}, "\"adjust_back.png\"", false},
        {{"adjust", "art", "generated", "restore", "file"}, "\"ADJUST_BACK.PNG\"", false},
        // One of the five generated controls' files, and the gear: the generated tier writes them all into one folder.
        {{"adjust", "art", "generated", "restore", "file"}, "\"modern_walk_left.png\"", false},
        {{"adjust", "art", "generated", "restore", "file"}, "\"MODERN_PAUSE.PNG\"", false},
        {{"adjust", "art", "generated", "restore", "file"}, "\"options_gear.png\"", false},
        {{"adjust", "art", "generated", "restore", "file"}, "\"..\\\\x.png\"", false},
        {{"adjust", "art", "generated", "restore", "body_inset"}, "0.3", false},
        {{"adjust", "art", "generated", "restore", "body_inset"}, "0.2", true},
        {{"adjust", "art"}, "\"x\"", false},
        {{"adjust"}, "5", false},
    };
    for (const Row& row : rows) {
        Controls::Rules c;
        Options::Rules o;
        Adjust::Rules a;
        std::string why;
        const bool loaded = LoadAll("edited.json", Write(Edit(root, row.path, 0, Parse(row.json))), c, o, a, why);
        CHECK_MSG(loaded == row.loads, Dotted(row.path) + " = " + row.json + (row.loads ? " loads" : " is refused") + (loaded ? "" : ": " + why));
        if (!row.loads) CHECK_MSG(!loaded && why.find(row.path.back()) != std::string::npos, Dotted(row.path) + ": the message names it: " + why);
    }
    // An edit that loads is read as set: the file is read, not copied.
    {
        Controls::Rules c;
        Options::Rules o;
        Adjust::Rules a;
        std::string why;
        CHECK(LoadAll("set.json", Write(Edit(Edit(root, Path{"adjust", "back", "gap_below_clear_px"}, 0, Parse("24")), Path{"adjust", "readout", "units_per_font_px"}, 0, Parse("0.35"))), c, o, a, why));
        CHECK_MSG(a.back.gapBelowClearPx == 24.0 && a.readout.unitsPerFontPx == 0.35, "the gap and the scale are what the file says");
        // And the place of Back follows the gap: 8 px more is 2.84 units lower, by the arithmetic of the layout.
        const glm::dvec2 view = ViewOf(16.0 / 9.0);
        const Hud::Rect moved = Adjust::BackBox(a, c, view, kNoInsets);
        CHECK_MSG(Near(moved.min.y - Adjust::BackBox(Data().adjust, Data().controls, view, kNoInsets).min.y, 8.0 / kPxPerUnit, 1e-9), "a gap of 24 px puts Back 8 px lower");
    }
    // Not a document: nothing there, not an object, not JSON.
    {
        Controls::Rules c;
        Options::Rules o;
        Adjust::Rules a;
        std::string why;
        CHECK(!Adjust::LoadRules((ScratchDir() / "missing-controls.json").string(), c, o, a, why) && why.find("cannot open") != std::string::npos);
        std::ofstream(ScratchDir() / "array.json") << "[]";
        CHECK(!Adjust::LoadRules((ScratchDir() / "array.json").string(), c, o, a, why) && !why.empty());
        std::ofstream(ScratchDir() / "broken.json") << "{\"adjust\": ";
        CHECK(!Adjust::LoadRules((ScratchDir() / "broken.json").string(), c, o, a, why) && !why.empty());
    }
    // A refused read leaves the output as it was: nothing half-read.
    {
        Controls::Rules c;
        Options::Rules o;
        Adjust::Rules a;
        std::string why;
        CHECK(LoadAll("keep.json", Write(root), c, o, a, why));
        const int kept = a.inertAlphaByte;
        CHECK(!LoadAll("bad.json", Write(Edit(root, Path{"adjust", "art", "generated", "back", "body_inset"}, 0, std::nullopt)), c, o, a, why));
        CHECK_MSG(a.inertAlphaByte == kept && a.back.boxPx == 108.8 && a.mr[0] && a.generated[7], "a refused read changes nothing of what was loaded");
    }
}

// ---- THE DRAG (planning step 193, CTL-12) --------------------------------------------------------------------------------------------------
//
// The drag is Adjust::Drag, a pure class fed one pointer and the screen. Pinned here with no layer: a pad that drives it the way the layer does, tick by
// tick, the tuning in force changing only when a tick says `commit`. THE LITERALS of the scenarios come from out/parity/program/CTL/CTL12/work/model_drag.py,
// an independent Python model of the specification (the shared clamp, the clearance, the walk in steps, the bisection) that reads controls.json for its
// constants and nothing of this code: the scenarios are the ones of 16:9 at the default size, where the default boxes are
//   walk_left (15.6444, 208.4978)-(47.5022, 240.3556)   restart (346.1689, 15.6444)-(387.1289, 56.6044)   clear_portals (15.6444, 15.6444)-(56.6044, 56.6044)
// and the widgets are kWidgetRows169's. A move is kept to a tenth of a unit, so a control stops within a tenth of what it may not meet.

constexpr std::size_t Ix(Controls::Id id) {
    return static_cast<std::size_t>(id);
}

// A screen of the editor: a view, its safe area's insets, the widgets on it.
struct Screen {
    glm::dvec2 view;
    Controls::Insets insets;
    Adjust::Layout widgets;
    Adjust::Field field;
    explicit Screen(double ratio, const Controls::Insets& ins = Controls::Insets{})
        : view(ViewOf(ratio)), insets(ins), widgets(Adjust::LayOut(Data().adjust, Data().controls, ViewOf(ratio), ins)), field{ViewOf(ratio), ins} {}
    std::array<Hud::Rect, 5> Boxes(const Controls::Tuning& tuning) const { return Controls::Boxes(Data().controls, tuning, view, insets); }
    double Clearance() const { return Adjust::ClearanceUnits(Data().controls, view); }
};

// Drives Adjust::Drag as the layer does, tick by tick: the tuning in force changes ONLY on a tick that says `commit`.
struct Pad {
    const Screen& screen;
    Adjust::Drag drag;
    Controls::Tuning tuning;
    bool unlocked = true;
    int commits = 0;
    int ticks = 0;
    Adjust::Step last;

    explicit Pad(const Screen& s) : screen(s) {}

    Adjust::Step Tick(bool pressed, bool released, bool touching, const glm::dvec2& at, bool dead = false, bool over = true) {
        const Adjust::Pointer pointer{over, pressed, released, touching, at};
        const Adjust::Context context{Data().adjust, Data().controls, screen.widgets, screen.field, tuning, unlocked};
        last = drag.Tick(pointer, dead, context);
        ++ticks;
        if (last.commit) {
            tuning = last.tuning;
            ++commits;
        }
        return last;
    }
    Adjust::Step Press(const glm::dvec2& at) { return Tick(true, false, true, at); }
    Adjust::Step Move(const glm::dvec2& at) { return Tick(false, false, true, at); }
    Adjust::Step Release(const glm::dvec2& at) { return Tick(false, true, false, at); }
    // A tick with no finger at all (the pointer is off the game: the layer's touch is empty).
    Adjust::Step Rest() { return Tick(false, false, false, glm::dvec2(0.0), false, false); }

    // The pointer goes from `from` to `to` at `speed` units a tick, the last tick landing on `to`.
    void Along(const glm::dvec2& from, const glm::dvec2& to, double speed) {
        const double length = glm::length(to - from);
        const int n = std::max(1, static_cast<int>(std::ceil(length / speed)));
        for (int i = 1; i <= n; ++i) Move(from + (to - from) * (static_cast<double>(i) / static_cast<double>(n)));
    }
    // A whole drag, as --drag makes one: down at `from`, to `to` at an even pace, let go there.
    Adjust::Step Straight(const glm::dvec2& from, const glm::dvec2& to, double speed) {
        Press(from);
        Along(from, to, speed);
        return Release(to);
    }
    // The box the screen shows for `id` now: where the walk has the held control, else the tuning in force.
    Hud::Rect Shown(Controls::Id id) const { return screen.Boxes(drag.Grabbed() ? drag.Live() : tuning)[Ix(id)]; }
};

glm::dvec2 MoveOf(const Controls::Tuning& tuning, Controls::Id id) {
    return tuning.move[Ix(id)];
}

// The committed tuning of the same straight drag at each of several paces.
std::vector<Controls::Tuning> AtPaces(const Screen& screen, const glm::dvec2& from, const glm::dvec2& to, std::initializer_list<double> paces) {
    std::vector<Controls::Tuning> out;
    for (const double pace : paces) {
        Pad pad(screen);
        pad.Straight(from, to, pace);
        out.push_back(pad.tuning);
    }
    return out;
}

bool AllEqual(const std::vector<Controls::Tuning>& tunings) {
    for (const Controls::Tuning& tuning : tunings) {
        if (!Adjust::SameKept(tuning, tunings.front())) return false;
    }
    return true;
}

bool InsideSafe(const Hud::Rect& box, const Screen& screen, double eps = 1e-9) {
    const glm::dvec2 max = box.Max();
    return box.min.x >= screen.insets.left - eps && box.min.y >= screen.insets.top - eps && max.x <= screen.view.x - screen.insets.right + eps &&
           max.y <= screen.view.y - screen.insets.bottom + eps;
}

void TheDragDataIsWhatTheFileSays() {
    const Loaded& d = Data();
    if (!d.ok) return;
    const Adjust::Rules& a = d.adjust;
    CHECK_MSG(a.unlocked.heldOutlineThicknessUnits == 1.0 && a.unlocked.heldOutlineAlphaByte == 255, "the outline of a held control is 1 unit and solid (design 3.3.2)");
    CHECK_MSG(a.drag.stepUnits == 0.5, "a drag walks in steps of half a unit");
    CHECK_MSG(a.drag.stepUnits > 2.0 * Controls::Tuning::kMoveSnap && a.drag.stepUnits <= 8.0, "more than twice the tenth a move is kept to, and no more than a quarter of the smallest box");
    CHECK_MSG(d.controls.clearancePx == 16.0, "the clearance between two controls is Magic Rampage's 16 px");
    CHECK_MSG(Near(Adjust::ClearanceUnits(d.controls, ViewOf(16.0 / 9.0)), kClearanceUnits, 1e-8), "which is 5.689 units on the view's 256");
    CHECK_MSG(Near(Adjust::ClearanceUnits(d.controls, glm::dvec2(500.0, 512.0)), 2.0 * kClearanceUnits, 1e-8), "and scales with the view's height");
}

void TheOverlapRuleIsMagicRampagesClearance() {
    const Hud::Rect a{glm::dvec2(0.0, 0.0), glm::dvec2(10.0, 10.0)};
    const double c = 4.0;
    // Apart by more than the clearance, exactly the clearance, and less: only less meets. Symmetric.
    for (const double gap : {20.0, 4.0 + 1e-6, 4.0}) {
        const Hud::Rect b{glm::dvec2(10.0 + gap, 0.0), glm::dvec2(10.0, 10.0)};
        CHECK_MSG(!Adjust::Meets(a, b, c) && !Adjust::Meets(b, a, c), "a gap of " + Num(gap) + " against a clearance of 4 is clear, both ways");
    }
    for (const double gap : {4.0 - 1e-6, 3.9, 0.0, -1.0}) {
        const Hud::Rect b{glm::dvec2(10.0 + gap, 0.0), glm::dvec2(10.0, 10.0)};
        CHECK_MSG(Adjust::Meets(a, b, c) && Adjust::Meets(b, a, c), "a gap of " + Num(gap) + " against a clearance of 4 meets, both ways");
    }
    // Diagonal neighbours: the clearance is on each axis (the box grown on every side), so a corner within it on both axes meets.
    const Hud::Rect corner{glm::dvec2(13.0, 13.0), glm::dvec2(10.0, 10.0)};
    CHECK_MSG(Adjust::Meets(a, corner, c) && !Adjust::Meets(a, corner, 2.0), "diagonally 3 apart on each axis meets at 4 and not at 2");
    // Zero clearance is the plain overlap: touching boxes do not meet.
    const Hud::Rect touching{glm::dvec2(10.0, 0.0), glm::dvec2(10.0, 10.0)};
    CHECK_MSG(!Adjust::Meets(a, touching, 0.0) && Adjust::OverlapArea(a, touching) == 0.0, "touching boxes share no area and do not meet at no clearance");
    CHECK_MSG(Near(Adjust::OverlapArea(a, Hud::Rect{glm::dvec2(5.0, 5.0), glm::dvec2(10.0, 10.0)}), 25.0) && Near(Adjust::OverlapArea(a, a), 100.0), "the shared area is the product of the two overlaps");
    // AnyMeet: five boxes, one pair.
    std::array<Hud::Rect, 5> five;
    for (std::size_t i = 0; i < five.size(); ++i) five[i] = Hud::Rect{glm::dvec2(30.0 * static_cast<double>(i), 0.0), glm::dvec2(10.0, 10.0)};
    CHECK_MSG(!Adjust::AnyMeet(five, c), "five boxes 20 apart do not meet at 4");
    five[3].min.x = five[2].min.x + 10.0 + 3.0;
    CHECK_MSG(Adjust::AnyMeet(five, c) && !Adjust::AnyMeet(five, 2.0), "two of them 3 apart do, at 4 and not at 2");
}

// A press on a control's drawn box grabs it, and it follows the pointer by the offset it was grabbed with; the tuning in force does not change until the release.
void ADragMovesTheBoxWithThePointersOffset() {
    const Loaded& d = Data();
    if (!d.ok) return;
    const Screen s(16.0 / 9.0);
    // Grabbed 10 right and 20 down of its corner, and again at its far corner: the same pointer path gives the same place (the offset is the pointer's).
    for (const glm::dvec2 offset : {glm::dvec2(10.0, 20.0), glm::dvec2(31.0, 31.0), glm::dvec2(0.5, 0.5)}) {
        Pad pad(s);
        const Hud::Rect start = pad.Shown(Controls::Id::Left);
        const glm::dvec2 press = start.min + offset;
        const Adjust::Step down = pad.Press(press);
        CHECK_MSG(down.use == Adjust::Use::Control && down.grabbed == std::optional<Controls::Id>(Controls::Id::Left) && !down.moved && !down.commit,
                  "pressed on walk_left's box (" + Num(offset.x) + ", " + Num(offset.y) + ") in: it is grabbed, nothing moved, nothing kept");
        const Adjust::Step half = pad.Move(press + glm::dvec2(30.0, -40.0));
        CHECK_MSG(half.moved && NearPoint(pad.Shown(Controls::Id::Left).min, start.min + glm::dvec2(30.0, -40.0), 1e-9), "30 right and 40 up of the press, the box stands 30 right and 40 up of where it began, whatever the offset");
        const Adjust::Step farStep = pad.Move(press + glm::dvec2(60.0, -90.0));
        CHECK_MSG(farStep.moved && NearPoint(pad.Shown(Controls::Id::Left).min, start.min + glm::dvec2(60.0, -90.0), 1e-9), "and at (60, -90) it stands at (75.6444, 118.4978)");
        CHECK_MSG(NearPoint(pad.Shown(Controls::Id::Left).min, glm::dvec2(75.644444444, 118.497777778), 1e-6), "the literal of the Python model");
        CHECK_MSG(!pad.last.commit && pad.commits == 0 && Adjust::SameKept(pad.tuning, Controls::Tuning{}), "the tuning in force has not changed while the finger is down");
        CHECK_MSG(NearPoint(MoveOf(farStep.tuning, Controls::Id::Left), glm::dvec2(60.0, -90.0)) && farStep.grabbed, "the screen is shown the tuning with the move (60, -90)");
        const Adjust::Step up = pad.Release(press + glm::dvec2(60.0, -90.0));
        CHECK_MSG(up.commit && up.use == Adjust::Use::Control && !up.grabbed && pad.commits == 1, "the release keeps it, once, and lets go");
        CHECK_MSG(NearPoint(MoveOf(pad.tuning, Controls::Id::Left), glm::dvec2(60.0, -90.0)) && NearRect(pad.Shown(Controls::Id::Left), Hud::Rect{glm::dvec2(75.644444444, 118.497777778), start.size}, 1e-6),
                  "the kept move is (60, -90): the box is (75.6444, 118.4978)-(107.5022, 150.3556)");
        for (const Controls::Id other : Controls::kClaimOrder) {
            if (other != Controls::Id::Left) CHECK_MSG(NearPoint(MoveOf(pad.tuning, other), glm::dvec2(0.0)), std::string(Controls::KeyOf(other)) + " did not move");
        }
        CHECK_MSG(pad.Rest().use == Adjust::Use::None, "and the next tick, with no finger, is nothing");
    }
}

void ADragIsClampedWhollyInsideTheView() {
    const Loaded& d = Data();
    if (!d.ok) return;
    int walks = 0;
    for (const Aspect& aspect : kAspects) {
        for (const Controls::Insets& insets : kInsetSets) {
            const Screen s(aspect.ratio, insets);
            for (const Controls::Id id : Controls::kClaimOrder) {
                // Each control grabbed in the middle of its box and dragged far past every edge and corner of the view, in eight directions.
                for (int dir = 0; dir < 8; ++dir) {
                    const double angle = static_cast<double>(dir) * 0.7853981633974483;
                    Pad pad(s);
                    const glm::dvec2 grab = pad.Shown(id).Centre();
                    pad.Press(grab);
                    const glm::dvec2 faraway = grab + glm::dvec2(std::cos(angle), std::sin(angle)) * 2000.0;
                    for (int tick = 1; tick <= 8; ++tick) {
                        pad.Move(grab + (faraway - grab) * (static_cast<double>(tick) / 8.0));
                        CHECK_MSG(InsideSafe(pad.Shown(id), s), std::string(aspect.name) + " " + Controls::KeyOf(id) + " heading " + std::to_string(dir) + ": the box is inside the safe rectangle on tick " + std::to_string(tick) + ": " + Show(pad.Shown(id)));
                    }
                    pad.Release(faraway);
                    CHECK_MSG(InsideSafe(pad.Shown(id), s), std::string(aspect.name) + " " + Controls::KeyOf(id) + " heading " + std::to_string(dir) + ": and kept inside it");
                    ++walks;
                }
            }
        }
    }
    CHECK_MSG(walks == 9 * 4 * 5 * 8, "1,440 walks to the edges: " + std::to_string(walks));
    // The literal: walk_right dragged past the bottom-right corner stands at the corner, a tenth's rounding inside it (Python model).
    {
        const Screen s(16.0 / 9.0);
        Pad pad(s);
        pad.Straight(pad.Shown(Controls::Id::Right).Centre(), glm::dvec2(1000.0, 1000.0), 40.0);
        const Hud::Rect box = pad.Shown(Controls::Id::Right);
        CHECK_MSG(NearRect(box, Hud::Rect{glm::dvec2(423.208888889, 224.097777778), box.size}, 1e-6) && box.Max().x <= s.view.x && box.Max().y <= s.view.y && s.view.x - box.Max().x < 0.05 + 1e-9,
                  "walk_right past the bottom-right corner: " + Show(box));
        CHECK_MSG(NearPoint(MoveOf(pad.tuning, Controls::Id::Right), glm::dvec2(15.6, 15.6)), "a move of (15.6, 15.6) from its place");
    }
    {
        const Screen s(16.0 / 9.0, Controls::Insets{21.3, 0.0, 21.3, 12.1});
        Pad pad(s);
        pad.Straight(pad.Shown(Controls::Id::Right).Centre(), glm::dvec2(1000.0, 1000.0), 40.0);
        const Hud::Rect box = pad.Shown(Controls::Id::Right);
        CHECK_MSG(NearRect(box, Hud::Rect{glm::dvec2(401.908888889, 211.997777778), box.size}, 1e-6), "and with a notch's insets it stops at the safe rectangle's corner, not the view's: " + Show(box));
    }
}

void ACandidateMeetingAnotherBoxIsRefusedAndTheControlStays() {
    const Loaded& d = Data();
    if (!d.ok) return;
    const Screen s(16.0 / 9.0);
    Pad pad(s);
    const Hud::Rect restart = pad.Shown(Controls::Id::Reset);
    const Hud::Rect pause = pad.Shown(Controls::Id::Menu);
    const double gap0 = pause.min.x - restart.Max().x;
    CHECK_MSG(Near(gap0, 11.377777778, 1e-8) && s.Clearance() < gap0, "restart stands 11.378 from pause, further than the clearance 5.689");
    const glm::dvec2 grab = restart.Centre();
    pad.Press(grab);
    pad.Move(grab + glm::dvec2(3.0, 0.0));
    CHECK_MSG(NearPoint(pad.Shown(Controls::Id::Reset).min, restart.min + glm::dvec2(3.0, 0.0), 1e-9), "3 units toward pause is free");
    pad.Move(grab + glm::dvec2(60.0, 0.0));
    const Hud::Rect stopped = pad.Shown(Controls::Id::Reset);
    const double gap = pause.min.x - stopped.Max().x;
    CHECK_MSG(gap >= s.Clearance() && gap < s.Clearance() + 0.1 + 1e-9, "60 units on, the pointer is inside pause and restart stands the clearance from it, to a tenth: gap " + Num(gap));
    CHECK_MSG(NearPoint(stopped.min - restart.min, glm::dvec2(5.6, 0.0), 1e-9), "at 5.6 from where it began (the Python model)");
    pad.Move(grab + glm::dvec2(200.0, 0.0));
    CHECK_MSG(!pad.last.moved && NearRect(pad.Shown(Controls::Id::Reset), stopped), "further on, the control stays");
    pad.Move(grab + glm::dvec2(20.0, 0.0));
    CHECK_MSG(NearRect(pad.Shown(Controls::Id::Reset), stopped), "and with the pointer back at 20 - still inside pause's clearance - it stays where it is: it does not jump to the pointer");
    pad.Release(grab + glm::dvec2(200.0, 0.0));
    CHECK_MSG(pad.last.commit && pad.commits == 1 && NearPoint(MoveOf(pad.tuning, Controls::Id::Reset), glm::dvec2(5.6, 0.0)) && NearPoint(MoveOf(pad.tuning, Controls::Id::Menu), glm::dvec2(0.0)),
              "the keeping is where it stayed; pause did not move");
    CHECK_MSG(!Adjust::AnyMeet(s.Boxes(pad.tuning), s.Clearance()), "and no two of the five meet");
    // Every control against every other: dragged straight at each neighbour from where it stands, none ever ends within the clearance of one.
    int pairs = 0;
    for (const Controls::Id a : Controls::kClaimOrder) {
        for (const Controls::Id b : Controls::kClaimOrder) {
            if (a == b) continue;
            Pad each(s);
            const glm::dvec2 from = each.Shown(a).Centre();
            each.Straight(from, each.Shown(b).Centre(), 7.0);
            CHECK_MSG(!Adjust::AnyMeet(s.Boxes(each.tuning), s.Clearance()), std::string(Controls::KeyOf(a)) + " dragged at " + Controls::KeyOf(b) + " ends clear of it");
            ++pairs;
        }
    }
    CHECK_MSG(pairs == 20, "twenty ordered pairs");
}

void ADragSlidesAlongAnObstacleInsteadOfStickingToIt() {
    const Loaded& d = Data();
    if (!d.ok) return;
    const Screen s(16.0 / 9.0);
    // restart toward pause and down: x is refused at the clearance, y is free, so it goes down along pause's side instead of stopping where it met it.
    {
        Pad pad(s);
        const Hud::Rect restart = pad.Shown(Controls::Id::Reset);
        const glm::dvec2 grab = restart.Centre();
        pad.Press(grab);
        pad.Move(grab + glm::dvec2(30.0, 25.0));
        const Hud::Rect now = pad.Shown(Controls::Id::Reset);
        CHECK_MSG(NearPoint(now.min - restart.min, glm::dvec2(5.6, 25.0), 1e-9), "right 30 and down 25: x stops at 5.6, y goes on to 25: " + Show(now));
        pad.Move(grab + glm::dvec2(30.0, 60.0));
        CHECK_MSG(NearPoint(pad.Shown(Controls::Id::Reset).min - restart.min, glm::dvec2(5.6, 60.0), 1e-9), "and on down to 60, still along pause's side: it slid, it did not stick");
        // Back up with the pointer still past pause's side: restart is below pause now, so nothing holds it from the pointer's x - it goes there at once,
        // x first - and pause's underside, the clearance above it, stops it going further up.
        const Hud::Rect pause = pad.Shown(Controls::Id::Menu);
        pad.Move(grab + glm::dvec2(30.0, 40.0));
        const Hud::Rect below = pad.Shown(Controls::Id::Reset);
        const double underside = below.min.y - pause.Max().y;
        CHECK_MSG(NearPoint(below.min - restart.min, glm::dvec2(30.0, 46.7), 1e-9) && underside >= s.Clearance() && underside < s.Clearance() + 0.1 + 1e-9,
                  "and up again with the pointer past pause: under it, the clearance below its underside (to a tenth), at the pointer's x: " + Show(below));
        // The pointer comes back along pause's side: the control goes up it, to the pointer.
        pad.Move(grab + glm::dvec2(3.0, 0.0));
        CHECK_MSG(NearPoint(pad.Shown(Controls::Id::Reset).min - restart.min, glm::dvec2(3.0, 0.0), 1e-9), "and with the pointer back clear of pause's side it goes up it, to the pointer: " + Show(pad.Shown(Controls::Id::Reset)));
        pad.Release(grab + glm::dvec2(3.0, 0.0));
        CHECK_MSG(NearPoint(MoveOf(pad.tuning, Controls::Id::Reset), glm::dvec2(3.0, 0.0)), "kept at (3, 0)");
    }
    // The same drag at several paces: 1 unit a tick, 40, and the ones between and beyond - the end states agree (review 2 F9). A fast move that would
    // jump a neighbour ends on the same valid place as a slow one.
    struct Row {
        const char* what;
        Controls::Id id;
        glm::dvec2 delta;     // the pointer's move from the middle of the box
        glm::dvec2 expect;    // the kept move (Python model)
    };
    const Row rows[] = {
        {"restart across pause (ends inside it)", Controls::Id::Reset, glm::dvec2(80.0, 0.0), glm::dvec2(5.6, 0.0)},
        {"restart right and down along pause", Controls::Id::Reset, glm::dvec2(30.0, 25.0), glm::dvec2(5.6, 25.0)},
        {"walk_left up into Back", Controls::Id::Left, glm::dvec2(0.0, -150.0), glm::dvec2(0.0, -91.1)},
        {"clear_portals right into the size row", Controls::Id::Clear, glm::dvec2(300.0, 0.0), glm::dvec2(116.4, 0.0)},
        {"clear_portals down onto Back", Controls::Id::Clear, glm::dvec2(0.0, 200.0), glm::dvec2(0.0, 22.0)},
        {"clear_portals right and down along Back", Controls::Id::Clear, glm::dvec2(20.0, 200.0), glm::dvec2(20.0, 22.0)},
    };
    for (const Row& row : rows) {
        const glm::dvec2 from = Pad(s).Shown(row.id).Centre();
        const std::vector<Controls::Tuning> ends = AtPaces(s, from, from + row.delta, {0.5, 1.0, 3.0, 7.3, 40.0, 200.0, 5000.0});
        CHECK_MSG(AllEqual(ends), std::string(row.what) + ": seven paces from 0.5 to 5,000 units a tick end on the same place");
        CHECK_MSG(NearPoint(MoveOf(ends.front(), row.id), row.expect, 1e-9) && NearPoint(MoveOf(ends[4], row.id), row.expect, 1e-9),
                  std::string(row.what) + ": at the Python model's (" + Num(row.expect.x) + ", " + Num(row.expect.y) + "): " + Num(MoveOf(ends.front(), row.id).x) + ", " + Num(MoveOf(ends.front(), row.id).y));
        CHECK_MSG(!Adjust::AnyMeet(s.Boxes(ends[4]), s.Clearance()), std::string(row.what) + ": and clear of every control");
    }
}

// At size 1.0 an arrow is 128 px, the clearance 16 px: 144 px is 51.2 units, a multiple of the tenth a move is kept to, so two arrows CAN stand exactly the
// clearance apart. That is clear - in both directions, to the last bit - and the walk and the five boxes' test agree on it (ten thousand fuzz gestures found it).
void AGapOfExactlyTheClearanceIsClearInBothDirectionsAndTheWalkStopsThere() {
    const Loaded& d = Data();
    if (!d.ok) return;
    const Controls::Rules& c = d.controls;
    const Screen s(16.0 / 9.0);
    Controls::Tuning tuning = SizedTuning(10);
    const std::array<Hud::Rect, 5> home = s.Boxes(tuning);
    const double alignX = -(home[Ix(Controls::Id::Right)].min.x - home[Ix(Controls::Id::Left)].min.x);
    // walk_right moved onto walk_left's column and 51.2 above it: its bottom is exactly the clearance above walk_left's top.
    tuning.move[Ix(Controls::Id::Right)] = glm::dvec2(std::round(alignX * 10.0) / 10.0, -51.2);
    tuning = tuning.Clamped(c);
    const std::array<Hud::Rect, 5> boxes = s.Boxes(tuning);
    const Hud::Rect& left = boxes[Ix(Controls::Id::Left)];
    const Hud::Rect& right = boxes[Ix(Controls::Id::Right)];
    const double gap = left.min.y - right.Max().y;
    CHECK_MSG(Near(gap, s.Clearance(), 1e-9) && std::fabs(left.min.x - right.min.x) < 0.06, "the arrows stand " + Num(gap) + " apart (the clearance is " + Num(s.Clearance()) + ") in one column");
    CHECK_MSG(!Adjust::Meets(left, right, s.Clearance()) && !Adjust::Meets(right, left, s.Clearance()) && !Adjust::AnyMeet(boxes, s.Clearance()), "a gap of exactly the clearance is clear, both ways");
    // A tenth closer meets, a tenth further does not.
    Controls::Tuning closer = tuning;
    closer.move[Ix(Controls::Id::Right)].y = -51.1;
    Controls::Tuning further = tuning;
    further.move[Ix(Controls::Id::Right)].y = -51.3;
    const std::array<Hud::Rect, 5> a = s.Boxes(closer);
    const std::array<Hud::Rect, 5> b = s.Boxes(further);
    CHECK_MSG(Adjust::Meets(a[Ix(Controls::Id::Left)], a[Ix(Controls::Id::Right)], s.Clearance()) && Adjust::Meets(a[Ix(Controls::Id::Right)], a[Ix(Controls::Id::Left)], s.Clearance()), "a tenth closer, they meet");
    CHECK_MSG(!Adjust::AnyMeet(b, s.Clearance()), "a tenth further, they do not");
    // The walk takes that place (the box stands exactly there) and not one tenth nearer: walk_right dragged straight down toward walk_left from a tenth above.
    Pad pad(s);
    pad.tuning = further;
    const glm::dvec2 grab = pad.Shown(Controls::Id::Right).Centre();
    pad.Straight(grab, grab + glm::dvec2(0.0, 40.0), 3.0);
    CHECK_MSG(NearPoint(MoveOf(pad.tuning, Controls::Id::Right), glm::dvec2(tuning.move[Ix(Controls::Id::Right)].x, -51.2)) && !Adjust::AnyMeet(s.Boxes(pad.tuning), s.Clearance()),
              "dragged down at walk_left it stops at the 51.2: " + Num(MoveOf(pad.tuning, Controls::Id::Right).y));
}

void ACandidateMeetingAWidgetIsRefused() {
    const Loaded& d = Data();
    if (!d.ok) return;
    const Screen s(16.0 / 9.0);
    // restart left into the size row's plus tile, clear_portals right into its minus tile: each stops at the tile's box, to a tenth.
    {
        Pad pad(s);
        pad.Straight(pad.Shown(Controls::Id::Reset).Centre(), pad.Shown(Controls::Id::Reset).Centre() - glm::dvec2(140.0, 0.0), 40.0);
        const Hud::Rect box = pad.Shown(Controls::Id::Reset);
        const Hud::Rect tile = s.widgets.At(Adjust::Widget::SizePlus).box;
        CHECK_MSG(NearPoint(MoveOf(pad.tuning, Controls::Id::Reset), glm::dvec2(-64.1, 0.0)), "restart left 140: it stops at (-64.1, 0), the plus tile's right edge");
        CHECK_MSG(Adjust::OverlapArea(box, tile) == 0.0 && box.min.x - tile.Max().x >= 0.0 && box.min.x - tile.Max().x < 0.1 + 1e-9, "flush against the tile's box, to a tenth: " + Num(box.min.x - tile.Max().x));
    }
    {
        Pad pad(s);
        pad.Straight(pad.Shown(Controls::Id::Clear).Centre(), pad.Shown(Controls::Id::Clear).Centre() + glm::dvec2(300.0, 0.0), 40.0);
        const Hud::Rect box = pad.Shown(Controls::Id::Clear);
        const Hud::Rect tile = s.widgets.At(Adjust::Widget::SizeMinus).box;
        CHECK_MSG(NearPoint(MoveOf(pad.tuning, Controls::Id::Clear), glm::dvec2(116.4, 0.0)) && Adjust::OverlapArea(box, tile) == 0.0 && tile.min.x - box.Max().x < 0.1 + 1e-9, "clear_portals right: stops at (116.4, 0) against the minus tile");
    }
    // A drop on every widget's middle, from the free side: never inside it. Nothing of the seven is a place a control may be dropped.
    int drops = 0;
    for (const Adjust::Widget widget : Adjust::kWidgets) {
        for (const Controls::Id id : Controls::kClaimOrder) {
            Pad pad(s);
            const glm::dvec2 from = pad.Shown(id).Centre();
            const Hud::Rect target = s.widgets.At(widget).box;
            pad.Straight(from, target.Centre(), 9.0);
            for (const Adjust::Widget other : Adjust::kWidgets) {
                CHECK_MSG(Adjust::OverlapArea(pad.Shown(id), s.widgets.At(other).box) == 0.0, std::string(Controls::KeyOf(id)) + " dragged to the " + Adjust::NameOf(widget) + " does not rest on the " + Adjust::NameOf(other));
            }
            ++drops;
        }
    }
    CHECK_MSG(drops == 35, "seven widgets, five controls: " + std::to_string(drops));
}

void AControlDroppedOnTheBackButtonIsRefused() {
    const Loaded& d = Data();
    if (!d.ok) return;
    const Screen s(16.0 / 9.0);
    const Hud::Rect back = s.widgets.At(Adjust::Widget::Back).box;
    CHECK_MSG(NearRect(back, RectOf(kBackBox), 1e-6), "Back stands at the top left, under clear_portals' place: " + Show(back));
    // From below: walk_left dragged up until the pointer is inside Back.
    {
        Pad pad(s);
        const glm::dvec2 from = pad.Shown(Controls::Id::Left).Centre();
        const glm::dvec2 into = back.Centre();
        pad.Straight(from, glm::dvec2(from.x, into.y), 25.0);
        const Hud::Rect box = pad.Shown(Controls::Id::Left);
        CHECK_MSG(NearPoint(MoveOf(pad.tuning, Controls::Id::Left), glm::dvec2(0.0, -91.1)), "walk_left up into Back stops at (0, -91.1)");
        CHECK_MSG(Adjust::OverlapArea(box, back) == 0.0 && box.min.y >= back.Max().y && box.min.y - back.Max().y < 0.1 + 1e-9, "its top is Back's bottom, to a tenth: " + Num(box.min.y - back.Max().y));
        CHECK_MSG(Adjust::StrictlyInside(s.widgets.At(Adjust::Widget::Back).face, glm::dvec2(from.x, into.y)), "while the pointer stands inside Back's face");
    }
    // From above: clear_portals dragged down onto Back.
    {
        Pad pad(s);
        const glm::dvec2 from = pad.Shown(Controls::Id::Clear).Centre();
        pad.Straight(from, back.Centre(), 5.0);
        const Hud::Rect box = pad.Shown(Controls::Id::Clear);
        CHECK_MSG(Adjust::OverlapArea(box, back) == 0.0 && !Adjust::Meets(box, back, 0.0), "clear_portals dropped on Back does not rest on it: " + Show(box));
        CHECK_MSG(back.min.y - box.Max().y >= 0.0 && back.min.y - box.Max().y < 0.1 + 1e-9, "it stays just above: " + Num(back.min.y - box.Max().y));
    }
    // At every aspect and with every set of insets, a control dragged onto Back never rests on it.
    int drops = 0;
    for (const Aspect& aspect : kAspects) {
        for (const Controls::Insets& insets : kInsetSets) {
            const Screen t(aspect.ratio, insets);
            const Hud::Rect b = t.widgets.At(Adjust::Widget::Back).box;
            for (const Controls::Id id : Controls::kClaimOrder) {
                Pad pad(t);
                pad.Straight(pad.Shown(id).Centre(), b.Centre(), 11.0);
                CHECK_MSG(Adjust::OverlapArea(pad.Shown(id), b) == 0.0, std::string(aspect.name) + " " + Controls::KeyOf(id) + " dropped on Back");
                ++drops;
            }
        }
    }
    CHECK_MSG(drops == 9 * 4 * 5, "180 drops on Back: " + std::to_string(drops));
}

void AGrabIsTheDrawnBoxNotTheHitRect() {
    const Loaded& d = Data();
    if (!d.ok) return;
    const Screen s(16.0 / 9.0);
    const Controls::Layout layout = Controls::LayOut(Controls::Style::Modern, d.controls, d.hud, Controls::Tuning{}, s.view, s.insets, kSettled);
    // walk_left's hit rectangle reaches the view's left and bottom edges; its box does not.
    const Hud::Rect box = layout.At(Controls::Id::Left).rect;
    CHECK_MSG(NearRect(box, Hud::Rect{glm::dvec2(15.644444444, 208.497777778), glm::dvec2(31.857777778)}, 1e-6), "walk_left's box (15.6444, 208.4978)");
    const glm::dvec2 inHitOnly(5.0, 250.0);
    CHECK_MSG(layout.At(Controls::Id::Left).hit.Contains(inHitOnly) && !Adjust::StrictlyInside(box, inHitOnly), "(5, 250) is in the hit rectangle and outside the box");
    {
        Pad pad(s);
        const Adjust::Step step = pad.Press(inHitOnly);
        CHECK_MSG(step.use == Adjust::Use::None && !step.grabbed, "a press in the hit rectangle and outside the box grabs nothing");
        pad.Move(inHitOnly + glm::dvec2(40.0, -40.0));
        CHECK_MSG(!pad.last.moved && pad.Release(inHitOnly).commit == false, "and dragging on moves nothing and keeps nothing");
    }
    // The edges: strictly inside, as a tile is.
    const glm::dvec2 max = box.Max();
    const glm::dvec2 mid = box.Centre();
    const struct {
        const char* where;
        glm::dvec2 at;
        bool grabs;
    } probes[] = {
        {"the left edge", glm::dvec2(box.min.x, mid.y), false},
        {"0.01 inside the left edge", glm::dvec2(box.min.x + 0.01, mid.y), true},
        {"the right edge", glm::dvec2(max.x, mid.y), false},
        {"0.01 inside the right edge", glm::dvec2(max.x - 0.01, mid.y), true},
        {"the top edge", glm::dvec2(mid.x, box.min.y), false},
        {"0.01 inside the top edge", glm::dvec2(mid.x, box.min.y + 0.01), true},
        {"the bottom edge", glm::dvec2(mid.x, max.y), false},
        {"0.01 inside the bottom edge", glm::dvec2(mid.x, max.y - 0.01), true},
        {"0.01 outside the corner", glm::dvec2(max.x + 0.01, max.y + 0.01), false},
    };
    for (const auto& probe : probes) {
        Pad pad(s);
        const Adjust::Step step = pad.Press(probe.at);
        CHECK_MSG((step.use == Adjust::Use::Control && step.grabbed == std::optional<Controls::Id>(Controls::Id::Left)) == probe.grabs, std::string("a press on ") + probe.where + (probe.grabs ? " grabs" : " does not grab"));
    }
    // The corner buttons' hit rectangles reach the view's edge too: a press at the very corner is the world's (it grabs nothing); inside the box it grabs.
    {
        Pad pad(s);
        CHECK_MSG(pad.Press(glm::dvec2(s.view.x - 1.0, 1.0)).use == Adjust::Use::None, "the corner of the view, in pause's hit rectangle and outside its box, grabs nothing");
        Pad inside(s);
        const Adjust::Step step = inside.Press(inside.Shown(Controls::Id::Menu).Centre());
        CHECK_MSG(step.use == Adjust::Use::Control && step.grabbed == std::optional<Controls::Id>(Controls::Id::Menu), "and a press in pause's box grabs pause");
    }
    // Two boxes on one point (a hand-made file can do it): the one the preview draws on top, the later in the claim order.
    {
        Controls::Tuning together;
        together.move[Ix(Controls::Id::Right)] = glm::dvec2(-392.0, 0.0);
        const std::array<Hud::Rect, 5> boxes = s.Boxes(together);
        CHECK_MSG(Adjust::OverlapArea(boxes[Ix(Controls::Id::Right)], boxes[Ix(Controls::Id::Left)]) > 1000.0, "walk_right is moved onto walk_left's place (a tenth off)");
        Pad pad(s);
        pad.tuning = together;
        const Adjust::Step step = pad.Press(boxes[Ix(Controls::Id::Left)].Centre());
        CHECK_MSG(step.grabbed == std::optional<Controls::Id>(Controls::Id::Right), "a press on both takes the one drawn over the other");
    }
}

void AWidgetWinsWhereItCoversAControl() {
    const Loaded& d = Data();
    if (!d.ok) return;
    // At 5:4 and size 1.4 restart's box meets the size row's plus tile by 798.6 units^2 (the recorded meeting), and its face by some.
    const Screen s(1.25);
    Pad pad(s);
    pad.tuning = SizedTuning(14);
    const Hud::Rect restart = pad.Shown(Controls::Id::Reset);
    const Hud::Rect face = s.widgets.At(Adjust::Widget::SizePlus).face;
    const glm::dvec2 low(std::max(restart.min.x, face.min.x), std::max(restart.min.y, face.min.y));
    const glm::dvec2 high(std::min(restart.Max().x, face.Max().x), std::min(restart.Max().y, face.Max().y));
    CHECK_MSG(low.x < high.x && low.y < high.y, "restart's box and the plus tile's face share area at 5:4 and size 1.4");
    const glm::dvec2 covered = (low + high) * 0.5;
    CHECK_MSG(Adjust::StrictlyInside(restart, covered) && Adjust::WidgetAt(s.widgets, covered) == Adjust::Widget::SizePlus, "a point in both: restart's box and the plus tile's face");
    const Adjust::Step press = pad.Press(covered);
    CHECK_MSG(press.use == Adjust::Use::Widget && !press.grabbed, "pressed there it is the widget's: nothing is grabbed");
    pad.Move(covered + glm::dvec2(-30.0, 0.0));
    CHECK_MSG(!pad.last.moved && pad.Release(covered).commit == false && pad.commits == 0, "and dragging on from it moves nothing");
    // The control's uncovered part is all a drag can take.
    const glm::dvec2 uncovered(restart.Max().x - 3.0, restart.min.y + 3.0);
    CHECK_MSG(Adjust::StrictlyInside(restart, uncovered) && Adjust::WidgetAt(s.widgets, uncovered) == Adjust::Widget::None, "a point of restart's box under no face");
    Pad other(s);
    other.tuning = SizedTuning(14);
    const Adjust::Step grab = other.Press(uncovered);
    CHECK_MSG(grab.use == Adjust::Use::Control && grab.grabbed == std::optional<Controls::Id>(Controls::Id::Reset), "pressed there it is grabbed");
    // Locked: nothing is grabbed anywhere.
    Pad locked(s);
    locked.tuning = SizedTuning(14);
    locked.unlocked = false;
    CHECK_MSG(locked.Press(uncovered).use == Adjust::Use::None && locked.Press(covered).use == Adjust::Use::Widget, "locked, a control is not grabbed and a widget is still a widget");
}

void ADeadEntryPressDoesNotGrab() {
    const Loaded& d = Data();
    if (!d.ok) return;
    const Screen s(16.0 / 9.0);
    // StaysDead is Follow's own question: the death stands while the press that opened the screen is down and no fresh press has come.
    Adjust::Touch dead;
    dead.dead = true;
    CHECK_MSG(Adjust::StaysDead(dead, false, true) && !Adjust::StaysDead(dead, true, true) && !Adjust::StaysDead(dead, false, false) && !Adjust::StaysDead(Adjust::Touch{}, false, true),
              "it stands for a held press, and ends on a fresh press, on a lift, and is never there for a live touch");
    // The lock is OPEN (forced, as a layer cannot have it on the first tick) and the press that opened the screen is still down over a control: it grabs nothing, wherever it goes.
    Pad pad(s);
    const glm::dvec2 over = pad.Shown(Controls::Id::Reset).Centre();
    for (int tick = 0; tick < 3; ++tick) {
        const Adjust::Step step = pad.Tick(false, false, true, over + glm::dvec2(static_cast<double>(tick), 0.0), true);
        CHECK_MSG(step.use == Adjust::Use::Dead && !step.grabbed && !step.moved && !step.commit, "a dead press over restart's box grabs nothing (tick " + std::to_string(tick) + ")");
    }
    pad.Tick(false, false, true, pad.Shown(Controls::Id::Left).Centre(), true);
    CHECK_MSG(pad.last.use == Adjust::Use::Dead && !pad.drag.Grabbed(), "and slid onto walk_left it still grabs nothing");
    // It lifts (a tick with no finger); a fresh press on the same box grabs.
    CHECK_MSG(pad.Tick(false, true, false, over, false).use == Adjust::Use::Dead && !pad.drag.Grabbed(), "the lift is the end of a dead touch, and grabs nothing");
    CHECK_MSG(pad.Rest().use == Adjust::Use::None, "and the tick after it is none");
    CHECK_MSG(pad.Press(over).use == Adjust::Use::Control && pad.drag.Grabbed() == std::optional<Controls::Id>(Controls::Id::Reset), "and a fresh press grabs");
}

void TheScreenAlwaysOpensLockedAndTheLockIsNeverSaved() {
    const Loaded& d = Data();
    if (!d.ok) return;
    const Screen s(16.0 / 9.0);
    // Locked, a press on a control's box is nothing and a drag moves nothing: the lock is what arms it.
    Pad locked(s);
    locked.unlocked = false;
    const glm::dvec2 from = locked.Shown(Controls::Id::Left).Centre();
    CHECK_MSG(locked.Press(from).use == Adjust::Use::None, "locked: a press on walk_left's box is nothing");
    locked.Along(from, from + glm::dvec2(60.0, -60.0), 5.0);
    CHECK_MSG(!locked.last.moved && !locked.drag.Grabbed() && locked.Release(from).commit == false, "and nothing moves or is kept");
    // The lock tile toggles, and changes no number: the lock is not in the tuning, so it cannot be in the file.
    const Controls::Rules& c = d.controls;
    Pad dragged(s);
    dragged.Straight(dragged.Shown(Controls::Id::Left).Centre(), dragged.Shown(Controls::Id::Left).Centre() + glm::dvec2(40.0, -30.0), 10.0);
    const Adjust::Outcome open = Adjust::Act(Adjust::Widget::Lock, c, dragged.tuning, false, &s.field);
    CHECK_MSG(open.acted && open.unlocked && !open.tuningChanged, "the lock opens and no number changes");
    const std::string text = dragged.tuning.ToJson(c);
    CHECK_MSG(text.find("lock") == std::string::npos && text.find("unlock") == std::string::npos && text.find("move") != std::string::npos, "the tuning a drag kept says its move and nothing of the lock: " + text);
    // A size step re-locks; an opacity step does not (with a field, as the layer calls Act).
    CHECK_MSG(!Adjust::Act(Adjust::Widget::SizePlus, c, dragged.tuning, true, &s.field).unlocked && Adjust::Act(Adjust::Widget::OpacityMinus, c, dragged.tuning, true, &s.field).unlocked, "a size step re-locks with a drag in the file; an opacity step does not");
}

void ADragCommitsOnceAtTheRelease() {
    const Loaded& d = Data();
    if (!d.ok) return;
    const Screen s(16.0 / 9.0);
    Pad pad(s);
    const glm::dvec2 from = pad.Shown(Controls::Id::Right).Centre();
    pad.Press(from);
    int moves = 0;
    glm::dvec2 at = from;
    for (int tick = 0; tick < 200; ++tick) {
        at = from - glm::dvec2(0.5 * tick, 0.5 * tick);
        const Adjust::Step step = pad.Move(at);
        moves += step.moved ? 1 : 0;
        CHECK_MSG(!step.commit, "tick " + std::to_string(tick) + " of the drag keeps nothing");
    }
    CHECK_MSG(moves > 150 && pad.commits == 0 && Adjust::SameKept(pad.tuning, Controls::Tuning{}), "200 ticks, the control moved on " + std::to_string(moves) + " of them and the tuning in force is as it was");
    const Adjust::Step up = pad.Release(at);
    CHECK_MSG(up.commit && pad.commits == 1 && !Adjust::SameKept(pad.tuning, Controls::Tuning{}), "the release keeps it: once");
    pad.Rest();
    pad.Rest();
    CHECK_MSG(pad.commits == 1, "and the ticks after it keep nothing more");
}

void NothingIsSavedIfTheSameAsLastSaved() {
    const Loaded& d = Data();
    if (!d.ok) return;
    const Screen s(16.0 / 9.0);
    // A drag that comes back to where it began is not a change: no keeping.
    {
        Pad pad(s);
        const glm::dvec2 from = pad.Shown(Controls::Id::Reset).Centre();
        pad.Press(from);
        pad.Along(from, from + glm::dvec2(-40.0, 30.0), 7.0);
        CHECK_MSG(pad.last.moved, "it went somewhere");
        pad.Along(from + glm::dvec2(-40.0, 30.0), from, 7.0);
        const Adjust::Step up = pad.Release(from);
        CHECK_MSG(!up.commit && pad.commits == 0 && Adjust::SameKept(up.tuning, Controls::Tuning{}), "and came home: the release keeps nothing");
    }
    // From a moved place to the same moved place: nothing either.
    {
        Pad pad(s);
        pad.tuning.move[Ix(Controls::Id::Menu)] = glm::dvec2(-5.0, 10.0);
        const glm::dvec2 from = pad.Shown(Controls::Id::Menu).Centre();
        pad.Press(from);
        pad.Along(from, from + glm::dvec2(5.0, 5.0), 1.0);
        pad.Along(from + glm::dvec2(5.0, 5.0), from, 1.0);
        CHECK_MSG(!pad.Release(from).commit && pad.commits == 0, "a moved control dragged and put back is not a change");
    }
    // A press and a release with no move at all.
    {
        Pad pad(s);
        const glm::dvec2 from = pad.Shown(Controls::Id::Clear).Centre();
        pad.Press(from);
        CHECK_MSG(!pad.Release(from).commit && pad.commits == 0, "a click on a control keeps nothing");
    }
}

void ALostFingerEndsAGrabWhereItStands() {
    const Loaded& d = Data();
    if (!d.ok) return;
    const Screen s(16.0 / 9.0);
    // The pointer leaves the window with the button down: the layer's touch is empty (no release is seen). The grab ends where it stands, and is kept.
    {
        Pad pad(s);
        const glm::dvec2 from = pad.Shown(Controls::Id::Left).Centre();
        pad.Press(from);
        pad.Along(from, from + glm::dvec2(50.0, -20.0), 5.0);
        const Adjust::Step lost = pad.Rest();
        CHECK_MSG(lost.commit && !lost.grabbed && pad.commits == 1 && NearPoint(MoveOf(pad.tuning, Controls::Id::Left), glm::dvec2(50.0, -20.0)), "a finger that is gone ends the grab where it stood: kept at (50, -20)");
        CHECK_MSG(pad.Rest().use == Adjust::Use::None && pad.commits == 1, "once");
    }
    // A press while a grab is held (its release was never seen): the grab ends, and that press takes nothing.
    {
        Pad pad(s);
        const glm::dvec2 from = pad.Shown(Controls::Id::Left).Centre();
        pad.Press(from);
        pad.Move(from + glm::dvec2(20.0, 0.0));
        const Adjust::Step again = pad.Press(pad.Shown(Controls::Id::Right).Centre());
        CHECK_MSG(again.commit && !again.grabbed && again.use == Adjust::Use::None && pad.commits == 1, "a second press ends the first grab and grabs nothing itself");
    }
    // Cancel: nothing is kept, nothing is held.
    {
        Pad pad(s);
        const glm::dvec2 from = pad.Shown(Controls::Id::Left).Centre();
        pad.Press(from);
        pad.Along(from, from + glm::dvec2(30.0, 0.0), 5.0);
        CHECK_MSG(pad.drag.Grabbed() && pad.last.moved, "a control is held and has moved");
        pad.drag.Cancel();
        CHECK_MSG(!pad.drag.Grabbed() && pad.drag.CurrentUse() == Adjust::Use::None, "cancelled, none is held");
        const Adjust::Step up = pad.Release(from + glm::dvec2(30.0, 0.0));
        CHECK_MSG(!up.commit && pad.commits == 0 && Adjust::SameKept(pad.tuning, Controls::Tuning{}), "and its release keeps nothing");
    }
    // The lock closing under a grab drops it (it cannot with one pointer; the rule is the drag's own).
    {
        Pad pad(s);
        const glm::dvec2 from = pad.Shown(Controls::Id::Left).Centre();
        pad.Press(from);
        pad.unlocked = false;
        const Adjust::Step step = pad.Move(from + glm::dvec2(20.0, 0.0));
        CHECK_MSG(!step.moved && !step.grabbed && !step.commit && !pad.drag.Grabbed(), "the lock closed: the grab is dropped and nothing is kept");
    }
}

void ADragStepsOutOfAnObstacleItStartsInAndNeverGoesDeeper() {
    const Loaded& d = Data();
    if (!d.ok) return;
    // At 5:4 and size 1.4 restart's default box already meets the size row's plus tile (the recorded meeting, 798.6 u^2). It can be dragged out of it, and
    // never further in.
    const Screen s(1.25);
    Pad pad(s);
    pad.tuning = SizedTuning(14);
    const Hud::Rect restart = pad.Shown(Controls::Id::Reset);
    const Hud::Rect tile = s.widgets.At(Adjust::Widget::SizePlus).box;
    const double start = Adjust::OverlapArea(restart, tile);
    CHECK_MSG(Near(start, 798.626551, 1e-4), "restart's box meets the plus tile's by 798.6 units^2 where it stands: " + Num(start));
    const glm::dvec2 grab(restart.Max().x - 3.0, restart.min.y + 3.0);
    // Up: the overlap shrinks at every step, so it goes (as far as the top of the view, which is a clamp).
    pad.Straight(grab, grab + glm::dvec2(0.0, -30.0), 5.0);
    const double up = Adjust::OverlapArea(pad.Shown(Controls::Id::Reset), tile);
    CHECK_MSG(NearPoint(MoveOf(pad.tuning, Controls::Id::Reset), glm::dvec2(0.0, -15.6)), "dragged up it goes to the top of the view: (0, -15.6)");
    CHECK_MSG(up > 0.0 && up < start && Near(up, 234.197751, 1e-4), "and meets the tile less: " + Num(up));
    // Down again: deeper, refused. Left: deeper, refused. Both leave the tuning as it was.
    for (const glm::dvec2 deeper : {glm::dvec2(0.0, 30.0), glm::dvec2(-20.0, 0.0), glm::dvec2(-20.0, 20.0)}) {
        const Controls::Tuning before = pad.tuning;
        const glm::dvec2 at = pad.Shown(Controls::Id::Reset).Max() - glm::dvec2(3.0, 3.0);
        pad.Straight(at, at + deeper, 5.0);
        CHECK_MSG(Adjust::SameKept(pad.tuning, before), "dragged (" + Num(deeper.x) + ", " + Num(deeper.y) + ") - deeper into the tile - it does not move");
        CHECK_MSG(Adjust::OverlapArea(pad.Shown(Controls::Id::Reset), tile) <= up + 1e-9, "the overlap is no bigger than it was");
    }
    // A control that starts inside a WIDGET does not make the other controls any easier: restart still keeps its clearance from pause.
    CHECK_MSG(!Adjust::Meets(pad.Shown(Controls::Id::Reset), pad.Shown(Controls::Id::Menu), s.Clearance()), "and clear of pause");
}

// A size step sends home the controls whose new boxes meet, and only those (design 3.3.4).
void ASizeStepSendsHomeOnlyTheControlsThatNowMeet() {
    const Loaded& d = Data();
    if (!d.ok) return;
    const Controls::Rules& c = d.controls;
    const Screen s(16.0 / 9.0);
    // The arrows 12 units apart at 0.7 (clear of each other by the clearance 5.689) and 2.9 apart at 0.8; restart moved 20 left, meeting nothing.
    Controls::Tuning tuning;
    const std::array<Hud::Rect, 5> home = s.Boxes(tuning);
    const double span = home[Ix(Controls::Id::Right)].min.x - home[Ix(Controls::Id::Left)].Max().x; // 359.3 at the default size
    tuning.move[Ix(Controls::Id::Left)] = glm::dvec2(100.0, 0.0);
    tuning.move[Ix(Controls::Id::Right)] = glm::dvec2(-(span - 12.0 - 100.0), 0.0);
    tuning.move[Ix(Controls::Id::Reset)] = glm::dvec2(-20.0, 0.0);
    tuning = tuning.Clamped(c);
    {
        const std::array<Hud::Rect, 5> now = s.Boxes(tuning);
        const double gap = now[Ix(Controls::Id::Right)].min.x - now[Ix(Controls::Id::Left)].Max().x;
        CHECK_MSG(Near(gap, 12.0, 0.06) && !Adjust::AnyMeet(now, s.Clearance()), "the arrows stand 12 apart (to the tenth a move is kept to) at 0.7 and no two meet: " + Num(gap));
        Controls::Tuning bigger = tuning;
        bigger.sizeSteps = 8;
        const std::array<Hud::Rect, 5> after = s.Boxes(bigger);
        CHECK_MSG(Adjust::Meets(after[Ix(Controls::Id::Left)], after[Ix(Controls::Id::Right)], s.Clearance()) && Near(after[Ix(Controls::Id::Right)].min.x - after[Ix(Controls::Id::Left)].Max().x, 2.8977777778, 0.06),
                  "at 0.8 they are 2.9 apart: they meet");
    }
    const Adjust::Outcome out = Adjust::Act(Adjust::Widget::SizePlus, c, tuning, true, &s.field);
    CHECK_MSG(out.acted && out.tuningChanged && !out.unlocked && out.tuning.SizeStepsIn(c) == 8, "the step is taken and the screen re-locks");
    CHECK_MSG(NearPoint(MoveOf(out.tuning, Controls::Id::Left), glm::dvec2(0.0)) && NearPoint(MoveOf(out.tuning, Controls::Id::Right), glm::dvec2(0.0)), "both arrows of the pair that meets go home");
    CHECK_MSG(NearPoint(MoveOf(out.tuning, Controls::Id::Reset), glm::dvec2(-20.0, 0.0)), "restart, which meets nothing, keeps its move");
    CHECK_MSG(!Adjust::AnyMeet(s.Boxes(out.tuning), s.Clearance()), "and no two of the five meet");
    // With no field the answer is Step 192's: the moves are left as they are.
    const Adjust::Outcome plain = Adjust::Act(Adjust::Widget::SizePlus, c, tuning, true);
    CHECK_MSG(NearPoint(MoveOf(plain.tuning, Controls::Id::Left), glm::dvec2(100.0, 0.0)), "asked without a field it sends nothing home");
    // A step that makes no boxes meet keeps every move.
    Controls::Tuning calm;
    calm.move[Ix(Controls::Id::Left)] = glm::dvec2(30.0, -20.0);
    calm.move[Ix(Controls::Id::Menu)] = glm::dvec2(-4.0, 5.0); // pause 4 left: 7.4 from restart, clear of its 5.69
    const Adjust::Outcome kept = Adjust::Act(Adjust::Widget::SizePlus, c, calm, false, &s.field);
    CHECK_MSG(NearPoint(MoveOf(kept.tuning, Controls::Id::Left), glm::dvec2(30.0, -20.0)) && NearPoint(MoveOf(kept.tuning, Controls::Id::Menu), glm::dvec2(-4.0, 5.0)), "a step that meets nothing sends nothing home");
    // SendHome is the function, and it is a function of the stepped tuning alone.
    Controls::Tuning stepped = tuning;
    stepped.sizeSteps = 8;
    CHECK_MSG(Adjust::SameKept(Adjust::SendHome(c, stepped, s.field), out.tuning), "SendHome on the stepped tuning is what Act gave");
}

void IfAnyPairStillMeetsEveryoneGoesHome() {
    const Loaded& d = Data();
    if (!d.ok) return;
    const Controls::Rules& c = d.controls;
    const Screen s(16.0 / 9.0);
    // As above, and clear_portals moved down so that it stands 6 units above the left arrow's HOME at 0.7 (clear of it) and 1.45 above its home at 0.8: the
    // arrows meet and go home, and the left arrow at home now meets clear_portals, which met nothing and was not sent: so everyone goes home.
    Controls::Tuning tuning;
    const std::array<Hud::Rect, 5> home = s.Boxes(tuning);
    const double span = home[Ix(Controls::Id::Right)].min.x - home[Ix(Controls::Id::Left)].Max().x;
    tuning.move[Ix(Controls::Id::Left)] = glm::dvec2(100.0, 0.0);
    tuning.move[Ix(Controls::Id::Right)] = glm::dvec2(-(span - 12.0 - 100.0), 0.0);
    tuning.move[Ix(Controls::Id::Reset)] = glm::dvec2(-20.0, 0.0);
    const double clearY = home[Ix(Controls::Id::Left)].min.y - home[Ix(Controls::Id::Clear)].size.y - 6.0;
    tuning.move[Ix(Controls::Id::Clear)] = glm::dvec2(0.0, clearY - home[Ix(Controls::Id::Clear)].min.y);
    tuning = tuning.Clamped(c);
    {
        CHECK_MSG(!Adjust::AnyMeet(s.Boxes(tuning), s.Clearance()), "at 0.7 no two meet");
        Controls::Tuning bigger = tuning;
        bigger.sizeSteps = 8;
        const std::array<Hud::Rect, 5> after = s.Boxes(bigger);
        int meeting = 0;
        for (const Controls::Id a : Controls::kClaimOrder) {
            for (const Controls::Id b : Controls::kClaimOrder) {
                if (Ix(a) < Ix(b) && Adjust::Meets(after[Ix(a)], after[Ix(b)], s.Clearance())) ++meeting;
            }
        }
        CHECK_MSG(meeting == 1 && Adjust::Meets(after[Ix(Controls::Id::Left)], after[Ix(Controls::Id::Right)], s.Clearance()), "at 0.8 only the arrows meet");
        Controls::Tuning homed = bigger;
        homed.move[Ix(Controls::Id::Left)] = glm::dvec2(0.0);
        homed.move[Ix(Controls::Id::Right)] = glm::dvec2(0.0);
        CHECK_MSG(Adjust::AnyMeet(s.Boxes(homed), s.Clearance()), "and with those two home, the left arrow meets clear_portals");
    }
    const Adjust::Outcome out = Adjust::Act(Adjust::Widget::SizePlus, c, tuning, true, &s.field);
    CHECK_MSG(!Adjust::HasMove(out.tuning), "so everyone goes home - restart and clear_portals too: " + out.tuning.ToJson(c));
    CHECK_MSG(out.tuning.SizeStepsIn(c) == 8 && !out.unlocked, "the size is the step's and the screen is locked");
    CHECK_MSG(!Adjust::AnyMeet(s.Boxes(out.tuning), s.Clearance()), "and no two of the five meet");
}

void RestoreAfterRealDragsClearsThemAllAndKeepsSizeAndOpacity() {
    const Loaded& d = Data();
    if (!d.ok) return;
    const Controls::Rules& c = d.controls;
    const Screen s(16.0 / 9.0);
    Pad pad(s);
    pad.tuning = SizedTuning(9);
    pad.tuning.opacitySteps = 3;
    CHECK_MSG(!Adjust::Enabled(Adjust::Widget::Restore, c, pad.tuning), "Restore is inert while nothing is moved");
    pad.Straight(pad.Shown(Controls::Id::Left).Centre(), pad.Shown(Controls::Id::Left).Centre() + glm::dvec2(70.0, -50.0), 9.0);
    pad.Straight(pad.Shown(Controls::Id::Menu).Centre(), pad.Shown(Controls::Id::Menu).Centre() + glm::dvec2(-30.0, 40.0), 9.0);
    CHECK_MSG(pad.commits == 2 && Adjust::HasMove(pad.tuning) && Adjust::Enabled(Adjust::Widget::Restore, c, pad.tuning), "two controls dragged: Restore is lit");
    const Adjust::Outcome out = Adjust::Act(Adjust::Widget::Restore, c, pad.tuning, true, &s.field);
    CHECK_MSG(out.acted && out.tuningChanged && !out.unlocked && !Adjust::HasMove(out.tuning), "Restore clears every move and re-locks");
    CHECK_MSG(out.tuning.SizeStepsIn(c) == 9 && out.tuning.OpacityStepsIn(c) == 3 && out.tuning.opacitySteps == std::optional<int>(3), "and keeps the size (0.9) and the opacity (0.6)");
    CHECK_MSG(!Adjust::Enabled(Adjust::Widget::Restore, c, out.tuning), "and is inert again");
}

void TheHeldOutlineIsThickerAndSolidAndTheRestBreatheInTicks() {
    const Loaded& d = Data();
    if (!d.ok) return;
    const Adjust::Rules::Unlocked& u = d.adjust.unlocked;
    const glm::dvec2 view = ViewOf(16.0 / 9.0);
    const Hud::Rect restart = Controls::Boxes(d.controls, Controls::Tuning{}, view, kNoInsets)[Ix(Controls::Id::Reset)];
    for (const int ticks : {0, 7, 24, 31, 48}) {
        Adjust::Scene scene;
        scene.unlocked = true;
        scene.ticks = ticks;
        scene.grabbed = Controls::Id::Reset;
        const std::vector<Adjust::Piece> pieces = Adjust::Pieces(d.adjust, d.controls, d.hud, Controls::Tuning{}, view, kNoInsets, scene);
        const std::vector<Adjust::Piece> outlines = Of(pieces, Adjust::Element::Outline);
        CHECK_MSG(outlines.size() == 20u, "twenty outline strips, as ever: tick " + std::to_string(ticks));
        int held = 0;
        for (const Adjust::Piece& piece : outlines) {
            if (piece.control == std::optional<Controls::Id>(Controls::Id::Reset)) {
                ++held;
                CHECK_MSG(piece.colour[3] == 255 && u.heldOutlineAlphaByte == 255, "the held control's outline is solid at tick " + std::to_string(ticks));
                CHECK_MSG(Near(std::min(piece.rect.size.x, piece.rect.size.y), 1.0, 1e-9), "and 1 unit thick");
            } else {
                CHECK_MSG(piece.colour[3] == Adjust::OutlineAlphaByte(u, ticks), "the others breathe on the state's ticks: tick " + std::to_string(ticks));
                CHECK_MSG(Near(std::min(piece.rect.size.x, piece.rect.size.y), 0.7, 1e-9), "and are 0.7 thick");
            }
        }
        CHECK_MSG(held == 4, "four strips round the held control");
        // The strips are the frame outside its box, 1 unit out.
        const std::array<Hud::Rect, 4> want = Adjust::FrameStrips(restart, 1.0, true);
        int matched = 0;
        for (const Adjust::Piece& piece : outlines) {
            for (const Hud::Rect& strip : want) matched += piece.control == std::optional<Controls::Id>(Controls::Id::Reset) && NearRect(piece.rect, strip, 1e-9) ? 1 : 0;
        }
        CHECK_MSG(matched == 4, "round restart's box, outside it");
    }
    // Nothing held: all twenty breathe.
    Adjust::Scene scene;
    scene.unlocked = true;
    scene.ticks = 12;
    for (const Adjust::Piece& piece : Of(Adjust::Pieces(d.adjust, d.controls, d.hud, Controls::Tuning{}, view, kNoInsets, scene), Adjust::Element::Outline)) {
        CHECK_MSG(piece.colour[3] == Adjust::OutlineAlphaByte(u, 12) && Near(std::min(piece.rect.size.x, piece.rect.size.y), 0.7, 1e-9), "with none held every outline is the breathing one");
    }
}

// G3: ten thousand seeded gestures of every kind the editor has - drags at several paces, size and opacity steps, the lock, Restore - at random shapes and
// with random safe areas, never leave a box outside the safe rectangle, two boxes meeting, or a tuning that does not read back the same. (The window is
// not resized inside a run: a saved move is pulled in by the layout and left as stored, so boxes can meet after a resize - a limit, recorded.) Two hundred
// runs of fifty gestures, each run on a view of its own; the same gestures every time (seeded).
void TenThousandRandomGesturesNeverBreakTheLayout() {
    const Loaded& d = Data();
    if (!d.ok) return;
    const Controls::Rules& c = d.controls;
    std::mt19937_64 rng(0x6D70193ULL);
    const auto unit = [&rng]() { return std::uniform_real_distribution<double>(0.0, 1.0)(rng); };
    const auto pick = [&rng](int n) { return std::uniform_int_distribution<int>(0, n - 1)(rng); };
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "supersonic-test-mp-adjust-fuzz";
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);

    long gestures = 0;
    long drags = 0;
    long grabs = 0;
    long commits = 0;
    long homings = 0;
    long restores = 0;
    long relocks = 0;
    long files = 0;
    long dragTicks = 0;
    long badBoxes = 0;
    long meetings = 0;
    long badRoundTrips = 0;
    long badFiles = 0;
    long otherMoved = 0;
    long notKept = 0;

    // The invariants of a laid-out tuning: every box wholly inside the safe rectangle, no two boxes within the clearance of each other.
    const auto invariants = [&](const Screen& s, const Controls::Tuning& tuning, const std::string& where) {
        const std::array<Hud::Rect, 5> boxes = s.Boxes(tuning);
        for (const Controls::Id id : Controls::kClaimOrder) {
            if (!InsideSafe(boxes[Ix(id)], s)) {
                ++badBoxes;
                CHECK_MSG(false, where + ": " + Controls::KeyOf(id) + " is outside the safe rectangle: " + Show(boxes[Ix(id)]));
            }
        }
        if (Adjust::AnyMeet(boxes, s.Clearance())) {
            ++meetings;
            CHECK_MSG(false, where + ": two boxes meet");
        }
    };

    for (int run = 0; run < 200; ++run) {
        const Aspect& aspect = kAspects[static_cast<std::size_t>(pick(9))];
        Controls::Insets insets;
        if (unit() < 0.5) insets = Controls::Insets{unit() * 24.0, unit() * 24.0, unit() * 24.0, unit() * 24.0};
        const Screen s(aspect.ratio, insets);
        Pad pad(s);
        if (unit() < 0.5) pad.tuning = SizedTuning(4 + pick(11));
        pad.unlocked = false;
        const std::string runName = "run " + std::to_string(run) + " " + aspect.name;
        for (int g = 0; g < 50; ++g) {
            ++gestures;
            const std::string where = runName + " gesture " + std::to_string(g);
            const Controls::Tuning before = pad.tuning;
            const int kind = pick(100);
            if (kind < 70) {
                // A drag. Open the lock first, as a player does (a tile is a gesture of its own).
                if (!pad.unlocked) pad.unlocked = Adjust::Act(Adjust::Widget::Lock, c, pad.tuning, false, &s.field).unlocked;
                ++drags;
                const Controls::Id id = Controls::kClaimOrder[static_cast<std::size_t>(pick(5))];
                const Hud::Rect box = pad.Shown(id);
                // Mostly a press inside the control's box; sometimes anywhere on the view (a widget, empty space, another control).
                const glm::dvec2 press = unit() < 0.8 ? box.min + glm::dvec2(unit(), unit()) * box.size : glm::dvec2(unit() * s.view.x, unit() * s.view.y);
                const double paces[] = {1.0, 7.0, 40.0, 300.0};
                const double pace = paces[pick(4)];
                const Adjust::Step down = pad.Press(press);
                const bool grabbed = down.grabbed.has_value();
                grabs += grabbed ? 1 : 0;
                glm::dvec2 at = press;
                const int legs = 1 + pick(2);
                for (int leg = 0; leg < legs; ++leg) {
                    // Mostly a wander near the press, sometimes across the view (and past its edges).
                    const glm::dvec2 to = unit() < 0.35 ? glm::dvec2(-40.0 + unit() * (s.view.x + 80.0), -40.0 + unit() * (s.view.y + 80.0))
                                                         : at + glm::dvec2(unit() - 0.5, unit() - 0.5) * 120.0;
                    const double length = glm::length(to - at);
                    const int n = std::max(1, static_cast<int>(std::ceil(length / pace)));
                    for (int i = 1; i <= n; ++i) {
                        const Adjust::Step step = pad.Move(at + (to - at) * (static_cast<double>(i) / static_cast<double>(n)));
                        ++dragTicks;
                        if (grabbed && (i % 4 == 0 || i == n)) {
                            const std::array<Hud::Rect, 5> boxes = s.Boxes(step.tuning);
                            for (const Controls::Id each : Controls::kClaimOrder) {
                                if (!InsideSafe(boxes[Ix(each)], s)) {
                                    ++badBoxes;
                                    CHECK_MSG(false, where + ": mid-drag " + Controls::KeyOf(each) + " is outside the safe rectangle");
                                }
                            }
                            if (Adjust::AnyMeet(boxes, s.Clearance())) {
                                ++meetings;
                                CHECK_MSG(false, where + ": mid-drag two boxes meet");
                            }
                        }
                    }
                    at = to;
                }
                const Adjust::Step up = pad.Release(at);
                commits += up.commit ? 1 : 0;
                if (grabbed) {
                    // Only the grabbed control may have changed, and the tuning in force changed iff the release said to keep it.
                    for (const Controls::Id each : Controls::kClaimOrder) {
                        if (each != *down.grabbed && !NearPoint(MoveOf(pad.tuning, each), MoveOf(before, each), 0.0)) ++otherMoved;
                    }
                    if (up.commit == Adjust::SameKept(pad.tuning, before)) ++notKept;
                } else if (!Adjust::SameKept(pad.tuning, before)) {
                    ++notKept;
                }
            } else if (kind < 80) {
                const Adjust::Widget widget = unit() < 0.5 ? Adjust::Widget::SizeMinus : Adjust::Widget::SizePlus;
                const Adjust::Outcome out = Adjust::Act(widget, c, pad.tuning, pad.unlocked, &s.field);
                if (out.acted) {
                    const Controls::Tuning plain = before.SizeStepped(c, widget == Adjust::Widget::SizePlus ? 1 : -1);
                    homings += Adjust::SameKept(out.tuning, plain) ? 0 : 1;
                    pad.tuning = out.tuning;
                    pad.unlocked = out.unlocked;
                    relocks += out.unlocked ? 0 : 1;
                    CHECK_MSG(!pad.unlocked, where + ": a size step re-locks");
                }
            } else if (kind < 88) {
                const Adjust::Outcome out = Adjust::Act(unit() < 0.5 ? Adjust::Widget::OpacityMinus : Adjust::Widget::OpacityPlus, c, pad.tuning, pad.unlocked, &s.field);
                if (out.acted) {
                    CHECK_MSG(out.unlocked == pad.unlocked && !Adjust::SameKept(out.tuning, pad.tuning), where + ": an opacity step changes the opacity and keeps the lock");
                    for (const Controls::Id each : Controls::kClaimOrder) {
                        if (!NearPoint(MoveOf(out.tuning, each), MoveOf(pad.tuning, each), 0.0)) ++otherMoved;
                    }
                    pad.tuning = out.tuning;
                }
            } else if (kind < 94) {
                pad.unlocked = Adjust::Act(Adjust::Widget::Lock, c, pad.tuning, pad.unlocked, &s.field).unlocked;
            } else {
                const Adjust::Outcome out = Adjust::Act(Adjust::Widget::Restore, c, pad.tuning, pad.unlocked, &s.field);
                if (out.acted) {
                    ++restores;
                    pad.tuning = out.tuning;
                    pad.unlocked = out.unlocked;
                    CHECK_MSG(!pad.unlocked && !Adjust::HasMove(pad.tuning), where + ": Restore clears the moves and re-locks");
                }
            }
            invariants(s, pad.tuning, where);
            // The tuning is valid as it stands, and reads back the same from its text.
            if (!Adjust::SameKept(pad.tuning.Clamped(c), pad.tuning)) {
                ++badRoundTrips;
                CHECK_MSG(false, where + ": the tuning is not valid as it stands");
            }
            {
                Supersonic::Json::Value parsed;
                std::string why;
                const std::string text = pad.tuning.ToJson(c);
                if (!Supersonic::Json::Parse(text, parsed, why) || !Adjust::SameKept(Controls::Tuning::ReadFrom(parsed, c), pad.tuning)) {
                    ++badRoundTrips;
                    CHECK_MSG(false, where + ": the tuning does not read back the same from " + text);
                }
            }
            // And through a file, the last gesture of every run: Store::Save, then Store::Open of the same directory.
            if (g == 49) {
                ++files;
                Controls::Store store;
                std::string error;
                const bool opened = store.Open(dir.string(), c, error);
                store.SetMode(Controls::Style::Modern);
                store.SetTuning(pad.tuning);
                const bool saved = store.Save(error);
                Controls::Store again;
                const bool reopened = again.Open(dir.string(), c, error);
                if (!(opened && saved && reopened) || !Adjust::SameKept(again.SavedTuning(), pad.tuning)) {
                    ++badFiles;
                    CHECK_MSG(false, where + ": the file does not read back to the same tuning: " + again.Text());
                }
            }
        }
    }
    std::filesystem::remove_all(dir, ec);
    CHECK_MSG(gestures == 10000, "ten thousand gestures: " + std::to_string(gestures));
    CHECK_MSG(badBoxes == 0 && meetings == 0 && badRoundTrips == 0 && badFiles == 0 && otherMoved == 0 && notKept == 0,
              "no box outside the safe rectangle, no two boxes meeting, no tuning or file that fails to read back, no control moved by the drag of another, no release that kept what it should not: " +
                  std::to_string(badBoxes) + ", " + std::to_string(meetings) + ", " + std::to_string(badRoundTrips) + ", " + std::to_string(badFiles) + ", " + std::to_string(otherMoved) + ", " + std::to_string(notKept));
    // The fuzz exercised the editor: it is not a loop over nothing (the figures are far under what the seed gives).
    CHECK_MSG(drags > 6000 && grabs > 2500 && commits > 1000 && restores > 50 && relocks > 300 && files == 200 && dragTicks > 20000,
              "the gestures were drags " + std::to_string(drags) + ", grabs " + std::to_string(grabs) + ", kept " + std::to_string(commits) + ", Restores " + std::to_string(restores) + ", re-locks " +
                  std::to_string(relocks) + ", files " + std::to_string(files) + ", drag ticks " + std::to_string(dragTicks) + "; sizes that sent controls home " + std::to_string(homings));
}

void runTests() {
    TheAdjustBlockIsWhatTheFileSays();
    TheWidgetColumnIsMagicRampagesInUnits();
    TheColumnIsCentredAtEveryAspectAndItsRowsDoNotMove();
    TheFacesOfTheWidgetsNeverMeetAtAnyAspect();
    TheBoxesOfTwoRowsOverlapByThreePointOneAndTheirFacesDoNot();
    StrictlyInsideIsOpenOnEveryEdge();
    WidgetAtNamesTheWidgetWhoseFaceHoldsThePoint();
    TheBackButtonIsAFixedPlaceUnderTheClearPreview();
    NoOtherDefaultControlMeetsBackAtAnySizeAspectOrSafeArea();
    NoDefaultControlIsWhollyUnderAWidgetAtAnyAspectOrSize();
    TheNarrowAspectsMeetAsRecorded();
    TheClearanceOfTheSplitArrowsAndTheOtherControlsToTheColumn();
    AtSixteenNineTheLeastClearanceToTheTilesIsThirtyOnePointThreeSevenAtSizeOnePointFour();
    RestoreAndTheColumnClearTheSplitArrowsAtEverySizeAndAspect();
    ATileActsOnTheReleaseStrictlyInside();
    ThePressThatOpenedTheScreenIsDeadUntilItLifts();
    AtALimitATileIsInert();
    TheSizeChainEndsOnTheLimits();
    ASizeStepRelocks();
    AnOpacityStepDoesNotRelock();
    TheLockTogglesAndBackLeavesAndNothingElse();
    RestoreIsEnabledOnlyWhenAMoveExistsAndKeepsSizeAndOpacity();
    TheReadoutsAreTheValueToOneDecimal();
    TheOutlinesBreatheInTicksNotWallClock();
    FrameStripsAreTheFourSidesOfAFrame();
    ThePreviewIsTheLevelsOwnLayoutWithClearForced();
    ThePreviewFlagIsTheScenesAloneAndForcesClearShown();
    WhileUnlockedTheEditorDrawsEveryHitRectAndTheCharacterRowGuide();
    TheTilesAreDrawnInTheOrderTheyAreClaimedAtTheInertAlphaAndTheHeldTint();
    TheEditorGlyphsAreEightPicturesOnOnePlate();
    TheEditorGlyphsFallBackTierByTier();
    WhatTheAdjustBlockRefuses();
    // The drag (planning step 193, CTL-12).
    TheDragDataIsWhatTheFileSays();
    TheOverlapRuleIsMagicRampagesClearance();
    ADragMovesTheBoxWithThePointersOffset();
    ADragIsClampedWhollyInsideTheView();
    ACandidateMeetingAnotherBoxIsRefusedAndTheControlStays();
    ADragSlidesAlongAnObstacleInsteadOfStickingToIt();
    AGapOfExactlyTheClearanceIsClearInBothDirectionsAndTheWalkStopsThere();
    ACandidateMeetingAWidgetIsRefused();
    AControlDroppedOnTheBackButtonIsRefused();
    AGrabIsTheDrawnBoxNotTheHitRect();
    AWidgetWinsWhereItCoversAControl();
    ADeadEntryPressDoesNotGrab();
    TheScreenAlwaysOpensLockedAndTheLockIsNeverSaved();
    ADragCommitsOnceAtTheRelease();
    NothingIsSavedIfTheSameAsLastSaved();
    ALostFingerEndsAGrabWhereItStands();
    ADragStepsOutOfAnObstacleItStartsInAndNeverGoesDeeper();
    ASizeStepSendsHomeOnlyTheControlsThatNowMeet();
    IfAnyPairStillMeetsEveryoneGoesHome();
    RestoreAfterRealDragsClearsThemAllAndKeepsSizeAndOpacity();
    TheHeldOutlineIsThickerAndSolidAndTheRestBreatheInTicks();
    TenThousandRandomGesturesNeverBreakTheLayout();
}

} // namespace

MP_TEST_MAIN("test_mp_adjust", 200)
