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
#include <optional>
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
}

} // namespace

MP_TEST_MAIN("test_mp_adjust", 200)
