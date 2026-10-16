// The Options screen and the main menu's entry to it: sim/Options and the `options` block of game/data/controls.json (planning step 185).
//
// An enhancement of this port's, not the original's, so what is pinned is the design (docs/planning/2026-10-15-modern-controls.md, 2.3), in
// literals written out here and never read back from the rules they check: the entry sits in the slot of the original's omitted Facebook
// button (top-right, 32 units square, 1190 to 1280 by 0 to 90 px at 720p); two tiles and the credits' own back button lay out, and meet no
// other widget, at every supported window shape; a tile is pressed by a touch that goes down on it and comes up on it and by nothing else; the
// chosen tile is the lit one; each tile's thumbnail is the pure layout of its style scaled into 64 x 36 units; and the block is read strictly,
// each key refused when it is missing and each value when it is out of its range.
//
// Pure: no window, no registry, no original art. ui.json and controls.json are the port's own and committed (from MP_TEST_PORTDATA when a
// mutation test sets it), so this runs anywhere and never skips. Its scratch files go to the temporary directory. What the screen does in the
// LAYER - the press, the choice, the save - is test_mp_layer's.

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "core/Json.hpp"

#include "sim/Controls.hpp"
#include "sim/Credits.hpp"
#include "sim/Hud.hpp"
#include "sim/MainMenu.hpp"
#include "sim/MenuState.hpp"
#include "sim/Options.hpp"
#include "sim/Pause.hpp"
#include "sim/UiLayer.hpp"

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
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

using namespace MagicPortals;

namespace {

// The port's own data: MP_TEST_PORTDATA when it is set (TEST ONLY: a mutation test runs this same binary on a copy of game/data), else the
// build's own. _dupenv_s: std::getenv is C4996 on Windows.
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
    std::printf("test_mp_options: MUTATION - MP_TEST_PORTDATA is set, the port's data is read from %s, not from game/data\n", text.c_str());
    return text;
}

const std::string kData = PortDataDir();
const std::string kUi = kData + "/ui.json";
const std::string kControls = kData + "/controls.json";

constexpr double kHeight = 256.0;       // the view's height, in units
constexpr double kPxPerUnit = 2.8125;   // 720 px over 256 units: the capture's pixels to a unit
constexpr double kSettled = 1.0e9;      // a state clock long past every entrance and bounce

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

std::string Num(double v) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.4f", v);
    return buffer;
}

std::string Show(const Hud::Rect& rect) {
    const glm::dvec2 max = rect.Max();
    return "(" + Num(rect.min.x) + ", " + Num(rect.min.y) + ")-(" + Num(max.x) + ", " + Num(max.y) + ")";
}

// The gap between two rectangles: the larger of their separations along the axes, negative when their interiors overlap.
double Gap(const Hud::Rect& a, const Hud::Rect& b) {
    const glm::dvec2 amax = a.Max();
    const glm::dvec2 bmax = b.Max();
    const double x = std::max(b.min.x - amax.x, a.min.x - bmax.x);
    const double y = std::max(b.min.y - amax.y, a.min.y - bmax.y);
    return std::max(x, y);
}

bool Inside(const Hud::Rect& inner, const Hud::Rect& outer, double eps = 1e-9) {
    const glm::dvec2 imax = inner.Max();
    const glm::dvec2 omax = outer.Max();
    return inner.min.x >= outer.min.x - eps && inner.min.y >= outer.min.y - eps && imax.x <= omax.x + eps && imax.y <= omax.y + eps;
}

// Everything the cases read, loaded once.
struct Loaded {
    Controls::Rules controls;
    Options::Rules options;
    Credits::Rules credits;
    Hud::Rules hud;
    MainMenu::Rules menu;
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
        const bool ui = Credits::LoadRules(kUi, l.credits, error) && Hud::LoadRules(kUi, l.hud, error) && MainMenu::LoadRules(kUi, l.menu, error);
        CHECK_MSG(ui, "ui.json's credits, hud and main menu read: " + error);
        l.ok = ok && ui;
        return l;
    }();
    return loaded;
}

// ---- the block, as the file says it ----------------------------------------------------------------------------------------------------------

void TheOptionsBlockIsWhatTheFileSays() {
    const Loaded& d = Data();
    if (!d.ok) return;
    const Options::Rules& o = d.options;

    // The entry: the slot of the omitted Facebook button, 32 units square, the gear's file and its margin.
    CHECK_MSG(o.entry.sprite == "options_gear.png", "the gear's file is options_gear.png: " + o.entry.sprite);
    CHECK_MSG(o.entry.atScreen == glm::dvec2(1.0, 0.0) && o.entry.origin == glm::dvec2(1.0, 0.0), "anchored on the top-right corner, by its top-right corner");
    CHECK_MSG(o.entry.sizeUnits == glm::dvec2(32.0, 32.0), "32 x 32 units");
    CHECK_MSG(o.entryBodyInset == 0.109375, "the gear's plate keeps the generated glyphs' margin, 14 px of 128");

    // The title.
    CHECK_MSG(o.title.words == "Controls", "the heading says Controls");
    CHECK_MSG(o.title.font == "Matura84_shadow.fnt", "in the original's font, as the plaque is");
    CHECK_MSG(o.title.unitsPerFontPx == 0.5, "half a unit a font px");
    CHECK_MSG(o.title.atScreen == glm::dvec2(0.5, 0.14), "centred, high");
    CHECK_MSG(o.title.alphaByte == 255, "opaque");

    // The tiles: Modern above Classic, 160 x 48 units.
    CHECK_MSG(Options::kTileStyles[0] == Controls::Style::Modern && Options::kTileStyles[1] == Controls::Style::Classic, "Modern is the upper tile");
    CHECK_MSG(o.tileSizeUnits == glm::dvec2(160.0, 48.0), "160 x 48 units");
    CHECK_MSG(o.tiles[0].atScreen == glm::dvec2(0.5, 0.38) && o.tiles[0].word == "Modern", "Modern at (0.5, 0.38)");
    CHECK_MSG(o.tiles[1].atScreen == glm::dvec2(0.5, 0.62) && o.tiles[1].word == "Classic", "Classic at (0.5, 0.62)");
    CHECK_MSG(o.wordFont == "Matura84_shadow.fnt" && o.wordUnitsPerFontPx == 0.3, "the words in the same font at 0.3 a font px");
    CHECK_MSG(o.wordAtUnits == glm::dvec2(116.0, 24.0), "centred 116 units across and 24 down the tile");
    CHECK_MSG((o.idleFrame == Options::Colour{34, 30, 52, 190}), "the idle plate");
    CHECK_MSG((o.chosenFrame == Options::Colour{96, 78, 156, 240}), "the chosen plate");
    CHECK_MSG(o.idleWordAlphaByte == 170 && o.chosenWordAlphaByte == 255, "the chosen word is whole, the other dimmer");

    // The thumbnail.
    CHECK_MSG(o.thumbnail.sizeUnits == glm::dvec2(64.0, 36.0), "a 64 x 36 picture of the layout");
    CHECK_MSG(o.thumbnail.atUnits == glm::dvec2(8.0, 6.0), "8 in and 6 down from the tile's corner");
    CHECK_MSG(o.thumbnail.referenceHeightUnits == 256.0, "of a view 256 units tall");
    CHECK_MSG((o.thumbnail.background == Options::Colour{16, 14, 26, 255}), "a dark ground");
    CHECK_MSG((o.thumbnail.walk == Options::Colour{205, 212, 232, 255}) && (o.thumbnail.corner == Options::Colour{244, 247, 255, 255}), "the arrows and the corner buttons in two lights");

    // The word fits where it stands: the longest word, Modern, is 236 font px of advance (Matura84_shadow), 70.8 units at 0.3.
    const double thumbRight = o.thumbnail.atUnits.x + o.thumbnail.sizeUnits.x;
    for (const auto& [word, advance] : {std::pair<const char*, double>{"Modern", 236.0}, {"Classic", 217.0}}) {
        const double half = advance * o.wordUnitsPerFontPx * 0.5;
        CHECK_MSG(o.wordAtUnits.x - half > thumbRight + 2.0, std::string(word) + " clears the thumbnail by at least 2 units");
        CHECK_MSG(o.wordAtUnits.x + half < o.tileSizeUnits.x - 2.0, std::string(word) + " clears the tile's edge by at least 2 units");
    }
}

// ---- the entry ----------------------------------------------------------------------------------------------------------------------------

void TheEntryIsTheFacebookSlot() {
    const Loaded& d = Data();
    if (!d.ok) return;
    const glm::dvec2 view169 = ViewOf(1280.0 / 720.0);
    const Options::Entry entry = Options::EntryAt(d.options, d.menu.layer, d.menu.state, view169, kSettled, false);
    // At 720p the box is (1190, 0) to (1280, 90) px: flush in the top-right corner.
    CHECK_MSG(Near(entry.rect.min.x * kPxPerUnit, 1190.0, 1e-6) && Near(entry.rect.min.y, 0.0), "its left and top edges: (1190, 0) px: " + Show(entry.rect));
    CHECK_MSG(Near(entry.rect.Max().x * kPxPerUnit, 1280.0, 1e-6) && Near(entry.rect.Max().y * kPxPerUnit, 90.0, 1e-6), "its right and bottom: (1280, 90) px");
    CHECK_MSG(entry.rect.size == glm::dvec2(32.0, 32.0), "32 x 32 units");
    CHECK_MSG(entry.alphaByte == 255 && entry.rgbByte == 255, "whole once it is in, untinted");
    // The glyph's plate fills the box: the picture is drawn larger by its transparent margin (14 px of 128 on each side).
    CHECK_MSG(Near(entry.picture.size.x, 32.0 / 0.78125, 1e-9) && Near(entry.picture.size.y, 32.0 / 0.78125, 1e-9), "the picture is 40.96 units: " + Show(entry.picture));
    CHECK_MSG(NearPoint(entry.picture.Centre(), entry.rect.Centre()), "and centred on the box");
    // Held, it is at the press tint of every menu button (204 / 255).
    CHECK_MSG(Options::EntryAt(d.options, d.menu.layer, d.menu.state, view169, kSettled, true).rgbByte == 204, "held it is tinted 204");

    // The entrance is the layer's, as every main-menu button has it: 32 units out along the ray from the view's centre, linear alpha over 700 ms.
    const Options::Entry first = Options::EntryAt(d.options, d.menu.layer, d.menu.state, view169, 0.0, false);
    CHECK_MSG(first.alphaByte == 0, "invisible on the state's first frame");
    CHECK_MSG(Near(glm::length(first.rect.Centre() - entry.rect.Centre()), 32.0, 1e-9), "and 32 units from its place");
    // The ray runs from the view's centre through the button's ANCHOR (UIButton::UIButton: pos is the anchor, not the centre of the sprite), here the top-right corner.
    const glm::dvec2 ray = glm::normalize(UiLayer::Anchor(d.options.entry, view169) - view169 * 0.5);
    CHECK_MSG(NearPoint(glm::normalize(first.rect.Centre() - entry.rect.Centre()), ray, 1e-9), "along the ray from the view's centre through the corner it stands in");
    const Options::Entry half = Options::EntryAt(d.options, d.menu.layer, d.menu.state, view169, 350.0, false);
    CHECK_MSG(half.alphaByte >= 127 && half.alphaByte <= 128, "half in at 350 ms: " + std::to_string(half.alphaByte));
    CHECK_MSG(glm::length(half.rect.Centre() - entry.rect.Centre()) < 32.0 && glm::length(half.rect.Centre() - entry.rect.Centre()) > 0.0, "and part of the way");
    for (const double ms : {700.0, 701.0, 5000.0}) {
        const Options::Entry in = Options::EntryAt(d.options, d.menu.layer, d.menu.state, view169, ms, false);
        CHECK_MSG(in.alphaByte == 255 && NearPoint(in.rect.min, entry.rect.min, 1e-9), "in place and whole from 700 ms: " + std::to_string(ms));
    }

    // The slot is empty at every supported window shape: the entry is flush in the corner and touches no button of the main menu.
    Pause::Switches switches;
    switches.soundOn = true;
    switches.musicOn = true;
    switches.musicAddedMs = 0.0;
    switches.musicDismissedMs = -1.0;
    for (const Aspect& aspect : kAspects) {
        const glm::dvec2 view = ViewOf(aspect.ratio);
        const Options::Entry here = Options::EntryAt(d.options, d.menu.layer, d.menu.state, view, kSettled, false);
        CHECK_MSG(Near(here.rect.Max().x, view.x) && Near(here.rect.min.y, 0.0), std::string(aspect.name) + ": flush in the top-right corner");
        int buttons = 0;
        for (const MainMenu::Piece& piece : MainMenu::Pieces(d.menu, switches, view, kSettled, 0.0, 0u)) {
            if (piece.element != MainMenu::Element::Button) continue;
            ++buttons;
            CHECK_MSG(Gap(piece.hitRect, here.rect) >= -1e-9, std::string(aspect.name) + ": a main-menu button's rectangle " + Show(piece.hitRect) + " does not overlap the entry's " + Show(here.rect));
        }
        CHECK_MSG(buttons == 6, std::string(aspect.name) + ": six buttons of the main menu were asked: " + std::to_string(buttons));
    }
    // The tightest case, 5:4: the title's box ends at x = 288 where the entry starts - touching, not overlapping.
    {
        const glm::dvec2 view = ViewOf(1.25);
        const Options::Entry here = Options::EntryAt(d.options, d.menu.layer, d.menu.state, view, kSettled, false);
        const Hud::Rect title = UiLayer::RectAt(d.menu.title, UiLayer::Anchor(d.menu.title, view));
        CHECK_MSG(Near(title.Max().x, 288.0) && Near(here.rect.min.x, 288.0), "5:4: the title ends at 288 and the entry starts at 288");
    }
}

// ---- the screen, at every window shape -----------------------------------------------------------------------------------------------------------

void TheTilesAndTheBackButtonLayOutAtEveryAspect() {
    const Loaded& d = Data();
    if (!d.ok) return;
    for (const Aspect& aspect : kAspects) {
        const glm::dvec2 view = ViewOf(aspect.ratio);
        const Hud::Rect whole{glm::dvec2(0.0), view};
        const std::string at = std::string(aspect.name) + ": ";
        for (std::size_t i = 0; i < Options::kTileStyles.size(); ++i) {
            const Controls::Style style = Options::kTileStyles[i];
            const std::string name = at + Controls::NameOf(style) + " tile ";
            const Hud::Rect tile = Options::TileRect(d.options, style, view);
            CHECK_MSG(tile.size == glm::dvec2(160.0, 48.0), name + "is 160 x 48");
            CHECK_MSG(NearPoint(tile.Centre(), d.options.tiles[i].atScreen * view), name + "is centred where its fractions say");
            CHECK_MSG(Inside(tile, whole), name + "is on the screen: " + Show(tile));
            CHECK_MSG(Near(tile.Centre().x, view.x * 0.5), name + "is centred across");
            const Hud::Rect thumbnail{tile.min + d.options.thumbnail.atUnits, d.options.thumbnail.sizeUnits};
            CHECK_MSG(Inside(thumbnail, tile), name + "holds its thumbnail");
            const glm::dvec2 word = tile.min + d.options.wordAtUnits;
            CHECK_MSG(word.x > thumbnail.Max().x && word.x < tile.Max().x && word.y > tile.min.y && word.y < tile.Max().y, name + "holds its word's centre, right of the thumbnail");
        }
        // Modern above Classic, a gap between.
        const Hud::Rect modern = Options::TileRect(d.options, Controls::Style::Modern, view);
        const Hud::Rect classic = Options::TileRect(d.options, Controls::Style::Classic, view);
        CHECK_MSG(modern.Max().y < classic.min.y, at + "Modern is above Classic");
        CHECK_MSG(classic.min.y - modern.Max().y > 10.0, at + "with a gap between them");
        // The title is centred and high.
        CHECK_MSG(NearPoint(d.options.title.atScreen * view, glm::dvec2(view.x * 0.5, 0.14 * kHeight)), at + "the title's centre");
        // The back button is the credits': 64 x 64, 0.05 of the width in, the middle of the left edge.
        const Hud::Rect back = Credits::BackHitRect(d.credits, view, kSettled);
        CHECK_MSG(back.size == glm::dvec2(64.0, 64.0), at + "the back button is 64 x 64");
        CHECK_MSG(Near(back.min.x, 0.05 * view.x - 0.05 * 64.0, 1e-9) && Near(back.min.y, 0.5 * kHeight - 0.5 * 64.0, 1e-9), at + "at the left edge's middle: " + Show(back));
        CHECK_MSG(Inside(back, whole), at + "and on the screen");
    }
    // The tightest case, 5:4: the back button's rectangle ends at 76.8 and a tile starts at 80.
    {
        const glm::dvec2 view = ViewOf(1.25);
        CHECK_MSG(Near(Credits::BackHitRect(d.credits, view, kSettled).Max().x, 76.8, 1e-9), "5:4: the back button ends at 76.8");
        CHECK_MSG(Near(Options::TileRect(d.options, Controls::Style::Modern, view).min.x, 80.0, 1e-9), "5:4: the tiles start at 80");
    }
}

void NoOptionsWidgetOverlapsAnotherAtAnySupportedAspect() {
    const Loaded& d = Data();
    if (!d.ok) return;
    // The font's measure, as Matura84_shadow has it: a line is 84 font px tall and "Controls" 246 px of advance, 278 to its last glyph's edge. At
    // the title's half a unit a font px that is a box 42 units tall and (generously) 140 wide, about its centre.
    constexpr double kTitleHalfWidth = 70.0;
    constexpr double kTitleHalfHeight = 21.0;
    for (const Aspect& aspect : kAspects) {
        const glm::dvec2 view = ViewOf(aspect.ratio);
        const std::string at = std::string(aspect.name) + ": ";
        // The back button at the largest the credits' bounce draws it (every sample of its stride, after the entrance).
        Hud::Rect back = Credits::BackHitRect(d.credits, view, kSettled);
        for (double ms = 700.0; ms <= 1300.0; ms += 20.0) {
            for (const Credits::Piece& piece : Credits::Pieces(d.credits, view, ms, Credits::Scroll{}, false)) {
                if (piece.element != Credits::Element::Back) continue;
                const glm::dvec2 lo = glm::min(back.min, piece.rect.min);
                const glm::dvec2 hi = glm::max(back.Max(), piece.rect.Max());
                back = Hud::Rect{lo, hi - lo};
            }
        }
        const Hud::Rect modern = Options::TileRect(d.options, Controls::Style::Modern, view);
        const Hud::Rect classic = Options::TileRect(d.options, Controls::Style::Classic, view);
        const glm::dvec2 titleCentre = d.options.title.atScreen * view;
        const Hud::Rect title{titleCentre - glm::dvec2(kTitleHalfWidth, kTitleHalfHeight), glm::dvec2(2.0 * kTitleHalfWidth, 2.0 * kTitleHalfHeight)};
        struct Widget {
            const char* name;
            const Hud::Rect* rect;
        };
        const std::array<Widget, 4> widgets = {{{"the back button", &back}, {"the Modern tile", &modern}, {"the Classic tile", &classic}, {"the title", &title}}};
        for (std::size_t i = 0; i < widgets.size(); ++i) {
            for (std::size_t j = i + 1; j < widgets.size(); ++j) {
                const double gap = Gap(*widgets[i].rect, *widgets[j].rect);
                CHECK_MSG(gap > 1.0, at + widgets[i].name + " and " + widgets[j].name + " are clear of each other by more than a unit: " + Num(gap));
            }
        }
        // The entry's slot is clear of all four too, at its entrance's worst (before it settles it is further out, never further in).
        const Options::Entry entry = Options::EntryAt(d.options, d.menu.layer, d.menu.state, view, kSettled, false);
        for (const Widget& widget : widgets) {
            CHECK_MSG(Gap(*widget.rect, entry.rect) > 1.0, at + "the main menu's entry slot is clear of " + widget.name + ", which a press there could otherwise reach");
        }
    }
    // The back button's rectangle against the tile: the closest the two come is 5:4, and it is 3.2 units before the bounce.
    const glm::dvec2 view = ViewOf(1.25);
    CHECK_MSG(Near(Gap(Credits::BackHitRect(d.credits, view, kSettled), Options::TileRect(d.options, Controls::Style::Modern, view)), 3.2, 1e-9), "5:4: 3.2 units between the back button and a tile");
}

void WidgetAtNamesTheWidgetUnderAPoint() {
    const Loaded& d = Data();
    if (!d.ok) return;
    for (const Aspect& aspect : kAspects) {
        const glm::dvec2 view = ViewOf(aspect.ratio);
        const std::string at = std::string(aspect.name) + ": ";
        const auto on = [&](const glm::dvec2& point) { return Options::WidgetAt(d.options, d.credits, view, kSettled, point); };
        const Hud::Rect modern = Options::TileRect(d.options, Controls::Style::Modern, view);
        const Hud::Rect classic = Options::TileRect(d.options, Controls::Style::Classic, view);
        const Hud::Rect back = Credits::BackHitRect(d.credits, view, kSettled);
        CHECK_MSG(on(modern.Centre()) == Options::Widget::ModernTile, at + "the Modern tile's centre");
        CHECK_MSG(on(classic.Centre()) == Options::Widget::ClassicTile, at + "the Classic tile's centre");
        CHECK_MSG(on(back.Centre()) == Options::Widget::Back, at + "the back button's centre");
        // The edges are closed, as Hud::Rect::Contains has them everywhere: a point exactly on one is on the widget, and a hair outside is not.
        CHECK_MSG(on(modern.min) == Options::Widget::ModernTile && on(modern.Max()) == Options::Widget::ModernTile, at + "a tile's corners are the tile's");
        CHECK_MSG(on(modern.min - glm::dvec2(1e-6, 0.0)) == Options::Widget::None && on(modern.Max() + glm::dvec2(0.0, 1e-6)) == Options::Widget::None, at + "a hair outside is nothing");
        CHECK_MSG(on(glm::dvec2(view.x * 0.5, (modern.Max().y + classic.min.y) * 0.5)) == Options::Widget::None, at + "between the tiles is nothing");
        CHECK_MSG(on(glm::dvec2(1.0, 1.0)) == Options::Widget::None && on(view - glm::dvec2(1.0, 1.0)) == Options::Widget::None, at + "the corners of the screen are nothing");
        CHECK_MSG(on(glm::dvec2(-5.0, -5.0)) == Options::Widget::None, at + "off the screen is nothing");
    }
    CHECK(Options::WidgetOf(Controls::Style::Modern) == Options::Widget::ModernTile && Options::WidgetOf(Controls::Style::Classic) == Options::Widget::ClassicTile);
    CHECK(Options::StyleOf(Options::Widget::ModernTile) == std::optional<Controls::Style>(Controls::Style::Modern));
    CHECK(Options::StyleOf(Options::Widget::ClassicTile) == std::optional<Controls::Style>(Controls::Style::Classic));
    CHECK(!Options::StyleOf(Options::Widget::Back) && !Options::StyleOf(Options::Widget::None));
}

// ---- the press ---------------------------------------------------------------------------------------------------------------------------

// Button::update's rule: down on a widget and up on the same widget. Nothing else is a press, however the touch got there.
void ATileIsPressedByADownAndUpInsideOnly() {
    using W = Options::Widget;
    const auto run = [](const std::vector<W>& path, bool pressOnFirstTick = true) {
        // `path` is where the pointer is on each tick the touch is down, the last of them the tick it comes up.
        Options::Touch touch;
        Options::Press last;
        int presses = 0;
        for (std::size_t i = 0; i < path.size(); ++i) {
            const bool lastTick = i + 1 == path.size();
            last = Options::Follow(touch, i == 0 && pressOnFirstTick, lastTick, true, path[i]);
            if (last.happened) ++presses;
            if (!lastTick) CHECK_MSG(!last.happened, "nothing is a press before the touch comes up");
        }
        return std::pair<int, Options::Press>(presses, last);
    };

    // Down and up on a tile: pressed, and it is that tile.
    for (const W tile : {W::ModernTile, W::ClassicTile, W::Back}) {
        const auto [presses, press] = run({tile, tile, tile});
        CHECK_MSG(presses == 1 && press.widget == tile, "down, held and up on the same widget is one press of it");
    }
    // One tick down and up (a click between two ticks) is both of them.
    {
        Options::Touch touch;
        const Options::Press press = Options::Follow(touch, true, true, true, W::ClassicTile);
        CHECK_MSG(press.happened && press.widget == W::ClassicTile, "a click inside one tick is a press");
        CHECK_MSG(touch.down == W::None && touch.held == W::None, "and the touch is forgotten");
    }
    // Down on one tile and up on the other: nothing.
    CHECK_MSG(run({W::ModernTile, W::ModernTile, W::ClassicTile}).first == 0, "down on Modern, slid to Classic and up there: nothing");
    CHECK_MSG(run({W::ClassicTile, W::ModernTile}).first == 0, "down on Classic, up on Modern: nothing");
    // A press that begins elsewhere and slides onto a tile: nothing, the world's or another screen's.
    CHECK_MSG(run({W::None, W::None, W::ModernTile}).first == 0, "down on nothing, up on a tile: nothing");
    CHECK_MSG(run({W::Back, W::ModernTile}).first == 0, "down on the back button, up on a tile: nothing");
    CHECK_MSG(run({W::None, W::ModernTile, W::ModernTile}).first == 0, "down on nothing and held on a tile: nothing");
    // Down on a tile and up outside it: nothing.
    CHECK_MSG(run({W::ClassicTile, W::ClassicTile, W::None}).first == 0, "down on a tile, up in the world: nothing");
    CHECK_MSG(run({W::ModernTile, W::Back}).first == 0, "down on a tile, up on the back button: nothing");
    // Slid off and back on: the last held place is inside, so it is a press (Button::update's own words).
    {
        const auto [presses, press] = run({W::ModernTile, W::None, W::None, W::ModernTile, W::ModernTile});
        CHECK_MSG(presses == 1 && press.widget == W::ModernTile, "down on a tile, off it and back on, up on it: a press");
    }
    // A touch that is already down when the screen opens (no press edge) has no widget it went down on: nothing, wherever it comes up.
    CHECK_MSG(run({W::ModernTile, W::ModernTile}, false).first == 0, "a touch whose press this screen never saw is not a press");
    // Nothing is a press without a release.
    {
        Options::Touch touch;
        for (int tick = 0; tick < 30; ++tick) {
            const Options::Press press = Options::Follow(touch, tick == 0, false, true, W::ModernTile);
            CHECK_MSG(!press.happened, "held, never released: no press");
        }
        CHECK_MSG(touch.down == W::ModernTile, "and the touch is still on the tile it went down on");
    }
    // The press tint: the widget a touch went down on is held while the pointer is inside it, and only then.
    {
        Options::Touch touch;
        Options::Follow(touch, true, false, true, W::ClassicTile);
        CHECK_MSG(touch.held == W::ClassicTile, "pressed, it is held");
        Options::Follow(touch, false, false, true, W::ClassicTile);
        CHECK_MSG(touch.held == W::ClassicTile, "still held");
        Options::Follow(touch, false, false, true, W::ModernTile);
        CHECK_MSG(touch.held == W::None, "slid onto the other tile it is not held, and the other is not either");
        Options::Follow(touch, false, false, true, W::None);
        CHECK_MSG(touch.held == W::None, "off every widget it is not held");
        Options::Follow(touch, false, false, true, W::ClassicTile);
        CHECK_MSG(touch.held == W::ClassicTile, "back on the tile it went down on it is held again");
        Options::Follow(touch, false, true, true, W::ClassicTile);
        CHECK_MSG(touch.held == W::None && touch.down == W::None, "let go, nothing is held");
    }
    {
        Options::Touch touch;
        Options::Follow(touch, true, false, true, W::None);
        CHECK_MSG(touch.held == W::None, "a touch that went down on nothing holds nothing, whatever it passes over");
        Options::Follow(touch, false, false, true, W::ModernTile);
        CHECK_MSG(touch.held == W::None, "not even a tile it slides onto");
        Options::Touch back;
        Options::Follow(back, true, false, true, W::Back);
        CHECK_MSG(back.held == W::Back, "the back button is held at the press tint too");
    }
}

// ---- the lit tile ----------------------------------------------------------------------------------------------------------------------------

// The pieces of a tile, by element.
const Options::Piece* PieceOf(const std::vector<Options::Piece>& pieces, Options::Element element, Controls::Style style) {
    for (const Options::Piece& piece : pieces) {
        if (piece.element == element && piece.tile == std::optional<Controls::Style>(style)) return &piece;
    }
    return nullptr;
}

void TheChosenTileIsLit() {
    const Loaded& d = Data();
    if (!d.ok) return;
    const glm::dvec2 view = ViewOf(1280.0 / 720.0);
    const Controls::Tuning tuning;
    const auto pieces = [&](Controls::Style chosen, Options::Widget held) {
        return Options::Pieces(d.options, d.credits, d.controls, d.hud, tuning, view, kSettled, chosen, held);
    };
    for (const Controls::Style chosen : Options::kTileStyles) {
        const Controls::Style other = chosen == Controls::Style::Modern ? Controls::Style::Classic : Controls::Style::Modern;
        const std::vector<Options::Piece> all = pieces(chosen, Options::Widget::None);
        const std::string name = std::string(Controls::NameOf(chosen)) + " chosen: ";
        const Options::Piece* lit = PieceOf(all, Options::Element::TileFrame, chosen);
        const Options::Piece* dim = PieceOf(all, Options::Element::TileFrame, other);
        CHECK_MSG(lit != nullptr && dim != nullptr, name + "both tiles have a frame");
        if (lit == nullptr || dim == nullptr) continue;
        CHECK_MSG(lit->colour == d.options.chosenFrame, name + "the chosen tile's frame is the lit plate");
        CHECK_MSG(dim->colour == d.options.idleFrame, name + "the other tile's frame is the idle plate");
        const Options::Piece* litWord = PieceOf(all, Options::Element::Word, chosen);
        const Options::Piece* dimWord = PieceOf(all, Options::Element::Word, other);
        CHECK_MSG(litWord != nullptr && dimWord != nullptr, name + "both tiles have a word");
        if (litWord == nullptr || dimWord == nullptr) continue;
        CHECK_MSG(litWord->colour[3] == 255 && dimWord->colour[3] == 170, name + "the chosen word is whole and the other dimmer");
        CHECK_MSG(litWord->words == (chosen == Controls::Style::Modern ? "Modern" : "Classic") && dimWord->words == (other == Controls::Style::Modern ? "Modern" : "Classic"), name + "each tile says its own word");
        CHECK_MSG(litWord->font == "Matura84_shadow.fnt" && litWord->unitsPerFontPx == 0.3, name + "in the original's font");
        // The word is written about the tile's own point, and the frame is the tile's rectangle.
        CHECK_MSG(lit->rect.size == glm::dvec2(160.0, 48.0) && NearPoint(litWord->rect.min, lit->rect.min + glm::dvec2(116.0, 24.0)), name + "the word's centre is 116 across and 24 down the frame");
    }
    // A tile held at the press tint is multiplied by it (204 / 255), the alpha untouched; the other tile and the rest are not.
    {
        const std::vector<Options::Piece> held = pieces(Controls::Style::Modern, Options::Widget::ModernTile);
        const Options::Piece* frame = PieceOf(held, Options::Element::TileFrame, Controls::Style::Modern);
        CHECK(frame != nullptr);
        if (frame != nullptr) {
            CHECK_MSG(frame->colour == (Options::Colour{96 * 204 / 255, 78 * 204 / 255, 156 * 204 / 255, 240}), "the held tile's frame is tinted 204 / 255 in r, g and b");
        }
        const Options::Piece* idle = PieceOf(held, Options::Element::TileFrame, Controls::Style::Classic);
        CHECK(idle != nullptr && idle->colour == d.options.idleFrame);
        const Options::Piece* word = PieceOf(held, Options::Element::Word, Controls::Style::Modern);
        CHECK_MSG(word != nullptr && word->colour == (Options::Colour{255 * 204 / 255, 255 * 204 / 255, 255 * 204 / 255, 255}), "and so is its word");
    }
    // The back button held is the credits' own tinted sprite; no tile is.
    {
        const std::vector<Options::Piece> held = pieces(Controls::Style::Classic, Options::Widget::Back);
        const Options::Piece* back = nullptr;
        for (const Options::Piece& piece : held) {
            if (piece.element == Options::Element::Back) back = &piece;
        }
        CHECK(back != nullptr);
        if (back != nullptr) CHECK_MSG(back->colour[0] == 204 && back->colour[1] == 204 && back->colour[2] == 204, "the held back button is at the press tint");
        const Options::Piece* frame = PieceOf(held, Options::Element::TileFrame, Controls::Style::Modern);
        CHECK(frame != nullptr && frame->colour == d.options.idleFrame);
        const Options::Piece* classic = PieceOf(held, Options::Element::TileFrame, Controls::Style::Classic);
        CHECK(classic != nullptr && classic->colour == d.options.chosenFrame);
        const std::vector<Options::Piece> free = pieces(Controls::Style::Classic, Options::Widget::None);
        for (const Options::Piece& piece : free) {
            if (piece.element == Options::Element::Back) CHECK_MSG(piece.colour[0] == 255, "released it is untinted");
        }
    }
    // The order of drawing, and what there is: the credits' background and back button, the title, then per tile a frame, a base, five
    // controls and a word.
    {
        const std::vector<Options::Piece> all = pieces(Controls::Style::Modern, Options::Widget::None);
        CHECK_MSG(all.size() == 2u + 1u + 2u * 8u, "nineteen pieces: " + std::to_string(all.size()));
        if (all.size() == 19u) {
            CHECK(all[0].element == Options::Element::Background && all[0].file == d.credits.background.sprite);
            CHECK(all[1].element == Options::Element::Back && all[1].file == d.credits.back.sprite);
            CHECK(all[2].element == Options::Element::Title && all[2].words == "Controls");
            for (std::size_t tile = 0; tile < 2; ++tile) {
                const std::size_t at = 3 + tile * 8;
                CHECK(all[at].element == Options::Element::TileFrame && all[at + 1].element == Options::Element::ThumbnailBase);
                for (std::size_t i = 0; i < 5; ++i) CHECK(all[at + 2 + i].element == Options::Element::ThumbnailControl && all[at + 2 + i].control == std::optional<Controls::Id>(Controls::kClaimOrder[i]));
                CHECK(all[at + 7].element == Options::Element::Word);
            }
            CHECK_MSG(all[3].tile == std::optional<Controls::Style>(Controls::Style::Modern) && all[11].tile == std::optional<Controls::Style>(Controls::Style::Classic), "Modern's pieces first");
        }
    }
}

// ---- the thumbnails ---------------------------------------------------------------------------------------------------------------------

void TheThumbnailsAreTheTwoLayoutsScaled() {
    const Loaded& d = Data();
    if (!d.ok) return;
    const glm::dvec2 view = ViewOf(1280.0 / 720.0);
    const Controls::Tuning tuning;
    const double scale = 36.0 / 256.0; // the thumbnail's height over the view's
    for (const Controls::Style style : Options::kTileStyles) {
        const std::string name = std::string(Controls::NameOf(style)) + " thumbnail: ";
        const Hud::Rect tile = Options::TileRect(d.options, style, view);
        const Hud::Rect base{tile.min + glm::dvec2(8.0, 6.0), glm::dvec2(64.0, 36.0)};
        const std::array<Hud::Rect, 5> rects = Options::ThumbnailRects(d.options, d.controls, d.hud, tuning, style, tile);
        // The reference view is the thumbnail's own shape at the real view's height: 455.11 x 256, 16:9 whatever the window.
        const glm::dvec2 reference(256.0 * 64.0 / 36.0, 256.0);
        const Controls::Layout layout = Controls::LayOut(style, d.controls, d.hud, tuning, reference, Controls::Insets{}, kSettled);
        for (const Controls::Id id : Controls::kClaimOrder) {
            const std::size_t i = static_cast<std::size_t>(id);
            const Hud::Rect want{base.min + layout.At(id).rect.min * scale, layout.At(id).rect.size * scale};
            CHECK_MSG(NearPoint(rects[i].min, want.min, 1e-9) && NearPoint(rects[i].size, want.size, 1e-9), name + Controls::KeyOf(id) + " is the layout's rectangle scaled by 36 / 256: " + Show(rects[i]));
            CHECK_MSG(Inside(rects[i], base), name + Controls::KeyOf(id) + " is inside the thumbnail: " + Show(rects[i]));
        }
    }
    const Hud::Rect modernTile = Options::TileRect(d.options, Controls::Style::Modern, view);
    const Hud::Rect classicTile = Options::TileRect(d.options, Controls::Style::Classic, view);
    const std::array<Hud::Rect, 5> modern = Options::ThumbnailRects(d.options, d.controls, d.hud, tuning, Controls::Style::Modern, modernTile);
    const std::array<Hud::Rect, 5> classic = Options::ThumbnailRects(d.options, d.controls, d.hud, tuning, Controls::Style::Classic, classicTile);

    // The MODERN thumbnail from the design's table 1.2 (boxes at 16:9, size 0.7, uniform), written out: x0, y0, x1, y1 in the 455.11 x 256
    // view, times 36 / 256, from the thumbnail's corner (8, 6) in the tile.
    struct Row {
        Controls::Id id;
        double box[4];
    };
    // The default arrangement is the SPLIT one (planning step 186): walk_right stands in the bottom-right corner, so the thumbnail shows one arrow in each
    // bottom corner, as the classic thumbnail does but inset. The grouped table (both arrows at the bottom left, planning step 180) is kept: the thumbnail of
    // a run asked for `arrows=grouped` shows it.
    const Row table[5] = {{Controls::Id::Left, {15.6444, 208.4978, 47.5022, 240.3556}},
                          {Controls::Id::Right, {407.6089, 208.4978, 439.4667, 240.3556}},
                          {Controls::Id::Reset, {346.1689, 15.6444, 387.1289, 56.6044}},
                          {Controls::Id::Menu, {398.5067, 15.6444, 439.4667, 56.6044}},
                          {Controls::Id::Clear, {15.6444, 15.6444, 56.6044, 56.6044}}};
    for (const Row& row : table) {
        const Hud::Rect& got = modern[static_cast<std::size_t>(row.id)];
        const glm::dvec2 min = got.min - modernTile.min;
        const glm::dvec2 max = got.Max() - modernTile.min;
        CHECK_MSG(Near(min.x, 8.0 + row.box[0] * scale, 2e-4) && Near(min.y, 6.0 + row.box[1] * scale, 2e-4) && Near(max.x, 8.0 + row.box[2] * scale, 2e-4) &&
                      Near(max.y, 6.0 + row.box[3] * scale, 2e-4),
                  std::string("modern ") + Controls::KeyOf(row.id) + " is the split table's box scaled: " + Show(got));
    }
    {
        Controls::Tuning grouped;
        grouped.arrows = Controls::Arrows::Grouped;
        const std::array<Hud::Rect, 5> groupedRects = Options::ThumbnailRects(d.options, d.controls, d.hud, grouped, Controls::Style::Modern, modernTile);
        const Row groupedTable[2] = {{Controls::Id::Left, {15.6444, 208.4978, 47.5022, 240.3556}}, {Controls::Id::Right, {58.8800, 208.4978, 90.7378, 240.3556}}};
        for (const Row& row : groupedTable) {
            const Hud::Rect& got = groupedRects[static_cast<std::size_t>(row.id)];
            const glm::dvec2 min = got.min - modernTile.min;
            const glm::dvec2 max = got.Max() - modernTile.min;
            CHECK_MSG(Near(min.x, 8.0 + row.box[0] * scale, 2e-4) && Near(min.y, 6.0 + row.box[1] * scale, 2e-4) && Near(max.x, 8.0 + row.box[2] * scale, 2e-4) &&
                          Near(max.y, 6.0 + row.box[3] * scale, 2e-4),
                      std::string("grouped ") + Controls::KeyOf(row.id) + " is planning step 180's box scaled: " + Show(got));
        }
        // The two arrangements differ in the right arrow alone, and the split thumbnail has the arrows in opposite corners of the picture.
        CHECK_MSG(!NearPoint(groupedRects[1].min, modern[1].min) && NearPoint(groupedRects[0].min, modern[0].min) && NearPoint(groupedRects[3].min, modern[3].min),
                  "the arrangements differ in the right arrow's place alone");
        const double mid = modernTile.min.x + 8.0 + 32.0;
        CHECK_MSG(modern[0].Max().x < mid && modern[1].min.x > mid, "split: the left arrow is in the thumbnail's left half and the right arrow in its right half");
    }
    // The CLASSIC thumbnail from the classic functions themselves: the 64-unit pads flush in the bottom corners, the 32-unit buttons where
    // ui.json puts them.
    for (const Controls::Id id : Controls::kClaimOrder) {
        const glm::dvec2 reference(256.0 * 64.0 / 36.0, 256.0);
        const Hud::Rect drawn = Controls::IsWalk(id) ? Controls::ClassicWalkFrame(d.hud, id, reference, kSettled).rect : Controls::ClassicPlace(d.hud, id, reference);
        const Hud::Rect& got = classic[static_cast<std::size_t>(id)];
        CHECK_MSG(NearPoint(got.min - classicTile.min, glm::dvec2(8.0, 6.0) + drawn.min * scale, 1e-9) && NearPoint(got.size, drawn.size * scale, 1e-9),
                  std::string("classic ") + Controls::KeyOf(id) + " is the classic function's rectangle scaled: " + Show(got));
    }
    CHECK_MSG(Near(classic[0].size.x, 64.0 * scale, 1e-9) && Near(classic[1].size.x, 64.0 * scale, 1e-9), "the classic pads are 64 units: 9 units in the thumbnail");
    CHECK_MSG(Near(classic[0].min.y + classic[0].size.y, classicTile.min.y + 6.0 + 36.0, 1e-9), "and stand on the thumbnail's floor");
    // They are two different pictures: the classic pads are twice the modern arrows' side and flush in the corners.
    CHECK_MSG(classic[0].size.x > modern[0].size.x * 1.9, "the classic pad is about twice the modern arrow");
    CHECK_MSG(Near(classic[0].min.x, classicTile.min.x + 8.0, 1e-9) && modern[0].min.x > modernTile.min.x + 8.0 + 1.0, "the pad is flush in the corner and the arrow is inset");
    // A tuning's look reaches the modern thumbnail (the preset is the one thing the layer applies): the larger look gives a larger arrow.
    {
        Controls::Tuning mr;
        mr.preset = Controls::Preset::Mr;
        const std::array<Hud::Rect, 5> big = Options::ThumbnailRects(d.options, d.controls, d.hud, mr, Controls::Style::Modern, modernTile);
        CHECK_MSG(big[0].size.x > modern[0].size.x * 1.3, "preset mr's arrow (size 1.0) is larger than the default's (0.7) in the thumbnail");
    }
    // The tuning leaves the classic thumbnail alone: it is the original's, whatever the look.
    {
        Controls::Tuning mr;
        mr.preset = Controls::Preset::Mr;
        const std::array<Hud::Rect, 5> same = Options::ThumbnailRects(d.options, d.controls, d.hud, mr, Controls::Style::Classic, classicTile);
        for (std::size_t i = 0; i < 5; ++i) CHECK(NearPoint(same[i].min, classic[i].min) && NearPoint(same[i].size, classic[i].size));
    }
}

// ---- what the block refuses -------------------------------------------------------------------------------------------------------------

using Path = std::vector<std::string>;

std::filesystem::path ScratchDir() {
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "supersonic-test-mp-options";
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

// Loads a document through both readers: the table's (which ignores the options block) and the block's own.
bool LoadBoth(const std::string& name, const std::string& text, Controls::Rules& controls, Options::Rules& options, std::string& error) {
    const std::filesystem::path path = ScratchDir() / name;
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file << text;
    }
    return Controls::LoadRules(path.string(), controls, error) && Options::LoadRules(path.string(), controls, options, error);
}

void WhatTheOptionsBlockRefuses() {
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
    CHECK_MSG(root["options"].IsObject(), "the file has an options block");
    if (!root["options"].IsObject()) return;

    Controls::Rules controls;
    Options::Rules options;
    std::string error;
    CHECK_MSG(LoadBoth("whole.json", Write(root), controls, options, error), "the file rewritten loads through both readers (so a refusal is the edit's): " + error);

    // Each key of the block removed, one at a time, in a scratch copy: refused, and the message names it. The block itself, too.
    std::vector<Path> paths = {Path{"options"}};
    Path prefix = {"options"};
    Collect(root["options"], prefix, paths);
    CHECK_MSG(paths.size() > 30, "the block has " + std::to_string(paths.size()) + " keys");
    long refused = 0;
    for (const Path& path : paths) {
        Controls::Rules c;
        Options::Rules o;
        std::string why;
        const bool loaded = LoadBoth("removed.json", Write(Edit(root, path, 0, std::nullopt)), c, o, why);
        const bool named = !loaded && why.find(path.back()) != std::string::npos;
        if (named) ++refused;
        CHECK_MSG(named, "removing " + Dotted(path) + " is refused, naming it: " + why);
    }
    CHECK_MSG(refused == static_cast<long>(paths.size()), std::to_string(refused) + " keys refused of " + std::to_string(paths.size()));

    // The table does not read the block: the same file without it is Controls::LoadRules' own business, which accepts it (so the block is the
    // Options reader's alone, and a typo in it cannot be the table's refusal).
    {
        Controls::Rules c;
        std::string why;
        const std::filesystem::path file = ScratchDir() / "no_options.json";
        {
            std::ofstream out(file, std::ios::binary | std::ios::trunc);
            out << Write(Edit(root, Path{"options"}, 0, std::nullopt));
        }
        CHECK_MSG(Controls::LoadRules(file.string(), c, why), "the table loads without the block: " + why);
        Options::Rules o;
        CHECK_MSG(!Options::LoadRules(file.string(), c, o, why) && why.find("options") != std::string::npos, "and the block's reader refuses its absence, naming it: " + why);
    }
    // A typo in a note, and a key the reader does not know, are not refused.
    {
        Json::Value edited = Edit(root, Path{"options", "_sourse"}, 0, Json::Value(std::string("typo")));
        edited = Edit(edited, Path{"options", "tile", "extra"}, 0, Json::Value(5.0));
        edited = Edit(edited, Path{"options", "entry", "_source"}, 0, Json::Value(std::string("not even an array")));
        Controls::Rules c;
        Options::Rules o;
        std::string why;
        CHECK_MSG(LoadBoth("typo.json", Write(edited), c, o, why), "a typo in a note and an unknown key are not refused: " + why);
    }

    // A value out of its type or its range is refused, and the values that are in range load.
    struct Row {
        Path path;
        const char* json;
        bool loads;
    };
    const std::vector<Row> rows = {
        {{"options", "entry", "file"}, "\"options_gear.png\"", true},
        {{"options", "entry", "file"}, "\"renamed_gear.png\"", true},
        {{"options", "entry", "file"}, "\"\"", false},
        {{"options", "entry", "file"}, "5", false},
        {{"options", "entry", "file"}, "\"../gear.png\"", false},
        {{"options", "entry", "file"}, "\"..\\\\gear.png\"", false},
        {{"options", "entry", "file"}, "\"sub/gear.png\"", false},
        {{"options", "entry", "file"}, "\"C:gear.png\"", false},
        {{"options", "entry", "file"}, "\"gear.png \"", false},
        {{"options", "entry", "file"}, "\"gear*.png\"", false},
        {{"options", "entry", "file"}, "\"CON\"", false},
        {{"options", "entry", "file"}, "\"nul.png\"", false},
        // One of the five generated files is another file than the gear (they are written into one folder).
        {{"options", "entry", "file"}, "\"modern_walk_left.png\"", false},
        {{"options", "entry", "file"}, "\"MODERN_CLEAR_PORTALS.PNG\"", false},
        {{"options", "entry", "body_inset"}, "0.25", true},
        {{"options", "entry", "body_inset"}, "0", true},
        {{"options", "entry", "body_inset"}, "0.2501", false},
        {{"options", "entry", "body_inset"}, "-0.01", false},
        {{"options", "entry", "size_units"}, "[32, 0]", false},
        {{"options", "entry", "size_units"}, "[-1, 32]", false},
        {{"options", "entry", "size_units"}, "[32]", false},
        {{"options", "entry", "size_units"}, "[48, 48]", true},
        {{"options", "entry", "at_screen"}, "[1.5, 0]", false},
        {{"options", "entry", "at_screen"}, "[1, -0.5]", false},
        {{"options", "entry", "origin"}, "[2, 0]", false},
        {{"options", "title", "words"}, "\"\"", false},
        {{"options", "title", "words"}, "\"Options\"", true},
        {{"options", "title", "font"}, "\"\"", false},
        {{"options", "title", "units_per_font_px"}, "0", false},
        {{"options", "title", "units_per_font_px"}, "-0.5", false},
        {{"options", "title", "alpha_byte"}, "256", false},
        {{"options", "title", "alpha_byte"}, "-1", false},
        {{"options", "title", "alpha_byte"}, "128", true},
        {{"options", "title", "at_screen"}, "[0.5, 1.1]", false},
        {{"options", "tile", "size_units"}, "[0, 48]", false},
        {{"options", "tile", "size_units"}, "[160, 48]", true},
        {{"options", "tile", "units_per_font_px"}, "0", false},
        {{"options", "tile", "word_at_units"}, "[116]", false},
        {{"options", "tile", "word_at_units"}, "[\"a\", 24]", false},
        {{"options", "tile", "idle_frame_rgba"}, "[1, 2, 3]", false},
        {{"options", "tile", "idle_frame_rgba"}, "[1, 2, 3, 4, 5]", false},
        {{"options", "tile", "idle_frame_rgba"}, "[1, 2, 3, 256]", false},
        {{"options", "tile", "idle_frame_rgba"}, "[1, 2, 3, 4.5]", false},
        {{"options", "tile", "idle_frame_rgba"}, "[1, 2, \"3\", 4]", false},
        {{"options", "tile", "idle_frame_rgba"}, "[-1, 2, 3, 4]", false},
        {{"options", "tile", "idle_frame_rgba"}, "[0, 0, 0, 0]", true},
        {{"options", "tile", "idle_frame_rgba"}, "[255, 255, 255, 255]", true},
        {{"options", "tile", "chosen_frame_rgba"}, "7", false},
        {{"options", "tile", "idle_word_alpha_byte"}, "300", false},
        {{"options", "tile", "chosen_word_alpha_byte"}, "-5", false},
        {{"options", "tile", "modern", "word"}, "\"\"", false},
        {{"options", "tile", "modern", "word"}, "\"Inset\"", true},
        {{"options", "tile", "classic", "at_screen"}, "[0.5, 2]", false},
        {{"options", "tile", "thumbnail", "size_units"}, "[64, 0]", false},
        {{"options", "tile", "thumbnail", "at_units"}, "[8]", false},
        {{"options", "tile", "thumbnail", "reference_height_units"}, "0", false},
        {{"options", "tile", "thumbnail", "reference_height_units"}, "-256", false},
        {{"options", "tile", "thumbnail", "background_rgba"}, "[1, 2, 3, 400]", false},
        {{"options", "tile", "thumbnail", "walk_rgba"}, "[]", false},
        {{"options", "tile", "thumbnail", "corner_rgba"}, "null", false},
        {{"options", "tile"}, "5", false},
        {{"options", "tile", "thumbnail"}, "\"x\"", false},
        {{"options", "entry"}, "[]", false},
        {{"options"}, "5", false},
    };
    for (const Row& row : rows) {
        Controls::Rules c;
        Options::Rules o;
        std::string why;
        const bool loaded = LoadBoth("edited.json", Write(Edit(root, row.path, 0, Parse(row.json))), c, o, why);
        CHECK_MSG(loaded == row.loads, Dotted(row.path) + " = " + row.json + (row.loads ? " loads" : " is refused") + (loaded ? "" : ": " + why));
        if (!row.loads) CHECK_MSG(!loaded && why.find(row.path.back()) != std::string::npos, Dotted(row.path) + ": the message names it: " + why);
    }
    // An edit that loads is read as set: the file is read, not copied.
    {
        Controls::Rules c;
        Options::Rules o;
        std::string why;
        CHECK(LoadBoth("set.json", Write(Edit(Edit(root, Path{"options", "title", "words"}, 0, Parse("\"Options\"")), Path{"options", "tile", "size_units"}, 0, Parse("[170, 50]"))), c, o, why));
        CHECK_MSG(o.title.words == "Options" && o.tileSizeUnits == glm::dvec2(170.0, 50.0), "the words and the size are what the file says");
    }
    // Not a document: nothing there, not an object, not JSON.
    {
        Controls::Rules c;
        Options::Rules o;
        std::string why;
        CHECK(!Options::LoadRules((ScratchDir() / "missing-controls.json").string(), c, o, why) && why.find("cannot open") != std::string::npos);
        std::ofstream(ScratchDir() / "array.json") << "[]";
        CHECK(!Options::LoadRules((ScratchDir() / "array.json").string(), c, o, why) && !why.empty());
        std::ofstream(ScratchDir() / "broken.json") << "{\"options\": ";
        CHECK(!Options::LoadRules((ScratchDir() / "broken.json").string(), c, o, why) && !why.empty());
    }
    // A refused read leaves the output as it was: nothing half-read.
    {
        Controls::Rules c;
        Options::Rules o;
        std::string why;
        CHECK(LoadBoth("keep.json", Write(root), c, o, why));
        const std::string kept = o.title.words;
        CHECK(!LoadBoth("bad.json", Write(Edit(root, Path{"options", "tile", "thumbnail", "walk_rgba"}, 0, std::nullopt)), c, o, why));
        CHECK_MSG(o.title.words == kept && o.tileSizeUnits == glm::dvec2(160.0, 48.0), "a refused read changes nothing of what was loaded");
    }
}

void runTests() {
    TheOptionsBlockIsWhatTheFileSays();
    TheEntryIsTheFacebookSlot();
    TheTilesAndTheBackButtonLayOutAtEveryAspect();
    NoOptionsWidgetOverlapsAnotherAtAnySupportedAspect();
    WidgetAtNamesTheWidgetUnderAPoint();
    ATileIsPressedByADownAndUpInsideOnly();
    TheChosenTileIsLit();
    TheThumbnailsAreTheTwoLayoutsScaled();
    WhatTheOptionsBlockRefuses();
}

} // namespace

MP_TEST_MAIN("test_mp_options", 150)
