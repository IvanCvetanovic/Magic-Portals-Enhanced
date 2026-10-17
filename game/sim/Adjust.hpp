#pragma once

// The adjustment screen (planning step 192, CTL-11; docs/planning/2026-10-15-modern-controls.md, section 3): where a player of the
// modern controls steps the size and the opacity of the five controls and sees them as they will stand. An ENHANCEMENT, not the
// original's, built on Magic Rampage's own adjust screen (Penumbra's spec 42, converted by height) and the original's font.
//
// WHAT IT SHOWS. A flat backdrop; the five controls at their real size, alpha and place - drawn from the SAME Controls::LayOut the level
// plays, with clear-portals forced shown (a flag of this screen's own, Scene::previewClearShown, which no level ever reads); and seven
// widgets in a column in the middle: size minus and plus, opacity minus and plus (each row with its readout between), lock and Restore, and
// Back. While the lock is open it draws what a drag will meet: a thin outline breathing round every control's box, every control's hit
// rectangle as a faint frame, and a guide at the character's row.
//
// WHAT IS HERE. Pure functions of data, a view and a tuning - the layer keeps the touch and the lock, draws what Pieces says, and decides
// what a press does with Act's answer. Dragging, the clamp and the overlap refusal are the next step's (CTL-12): nothing here moves a
// control, and the lock only arms what that step builds.
//
// A WIDGET HAS TWO RECTANGLES. Its BOX is Magic Rampage's (the tile's picture is drawn into it, and the clearance the default controls keep
// from it is measured on it). Its FACE is the box shrunk by the picture's own margin: what is seen, and what a tap hits, strictly inside it.
// Two rows' boxes overlap by a few units (36.4 over a 33.3 pitch) where their faces never do.
//
// THE BACK BUTTON stands in a FIXED place, top left, UNDER the clear-portals preview (the owner's decision of 2026-10-16): its left is the
// layout's side margin and its top is the layout's top margin plus the LARGEST corner box (size 1.4) plus a gap, from the safe rectangle's
// corner, so it clears clear_portals' box by that gap at the largest size and by more at every other, and it never moves as the size is
// stepped.
//
// The numbers are controls.json's `adjust` block, read STRICTLY by LoadRules here (Controls::LoadRules does not read it, so a typo in it
// costs the modern controls and never a classic run). Every colour and place is the port's own or Magic Rampage's converted, and says so.

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "sim/Controls.hpp"
#include "sim/Hud.hpp"
#include "sim/Options.hpp"

namespace MagicPortals::Adjust {

using Colour = Options::Colour; // r, g, b, a, each a byte

// The seven widgets, in the order they are drawn and, where a point could be on two, claimed.
enum class Widget { None, SizeMinus, SizePlus, OpacityMinus, OpacityPlus, Lock, Restore, Back };
inline constexpr std::size_t kWidgetCount = 7;
inline constexpr std::array<Widget, kWidgetCount> kWidgets = {Widget::SizeMinus,    Widget::SizePlus, Widget::OpacityMinus, Widget::OpacityPlus,
                                                              Widget::Lock,         Widget::Restore,  Widget::Back};

// For a log line and a test's message.
const char* NameOf(Widget widget);

// The pictures the widgets are drawn with: one per widget, and the lock has one for each of its two states (Magic Rampage has two sprites:
// a closed padlock on the stone frame, an open one on the red frame).
enum class Picture { SizeMinus, SizePlus, OpacityMinus, OpacityPlus, LockLocked, LockUnlocked, Restore, Back };
inline constexpr std::size_t kPictureCount = 8;
inline constexpr std::array<Picture, kPictureCount> kPictures = {Picture::SizeMinus,  Picture::SizePlus,   Picture::OpacityMinus, Picture::OpacityPlus,
                                                                 Picture::LockLocked, Picture::LockUnlocked, Picture::Restore,      Picture::Back};
// The name controls.json's `art` tiers and the art tool's files call a picture by: size_minus, size_plus, opacity_minus, opacity_plus,
// lock_locked, lock_unlocked, restore, back.
const char* KeyOf(Picture picture);
std::optional<Picture> PictureOfKey(const std::string& key);
// The picture a widget shows: the lock's depends on whether it is open. None for no widget.
std::optional<Picture> PictureOf(Widget widget, bool unlocked);

// A size of box and where its centre stands, as the data has them (px of Magic Rampage's reference, converted by height) and the margin
// of the frame its picture is drawn on.
struct TileSize {
    double boxPx = 0.0;
    double offsetPx = 0.0;    // the tile's centre from the view's middle, along x
    double faceInset = 0.0;   // the face is the box shrunk by this fraction of it on each side
};

struct Rules {
    Colour backdrop{};

    // The three rows, as fractions of the view's height: each is centred on the view's middle.
    double rowSize = 0.0;
    double rowOpacity = 0.0;
    double rowLock = 0.0;

    TileSize tile;  // the four tiles of the size and opacity rows
    TileSize lock;  // the lock and Restore

    struct BackButton {
        double boxPx = 0.0;
        double faceInset = 0.0;
        double gapBelowClearPx = 0.0;
    } back;

    struct Readout {
        std::string font;
        double unitsPerFontPx = 0.0;
        int alphaByte = 255;
    } readout;

    int inertAlphaByte = 0;   // a widget at its limit, and Restore with nothing to restore
    Colour flatTile{};        // the last tier of a widget's picture: a flat quad on its face
    Colour flatUnlocked{};    // and the open lock's

    struct Unlocked {
        double outlineThicknessUnits = 0.0;
        int outlineAlphaByte = 0;      // the middle of the breathing
        int outlineSwingByte = 0;      // how far either way
        int outlinePeriodTicks = 0;    // one full breath
        std::array<int, 3> outlineRgb{};
        int hitFrameAlphaByte = 0;
        double hitFrameThicknessUnits = 0.0;
        std::array<int, 3> hitFrameRgb{};
        double guideThicknessUnits = 0.0;
        Colour guide{};
    } unlocked;

    // The picture of each widget through the tiers (design 4.2): Magic Rampage's file in the folder `editor`, and the glyph the game
    // draws and writes. A file name is a bare file name; an entry is there for every picture.
    std::array<std::optional<Controls::ArtEntry>, kPictureCount> mr;
    std::array<std::optional<Controls::ArtEntry>, kPictureCount> generated;
};

// controls.json's `adjust` block. `controls` is the table (reference height, margins, corner box, sizes) the layout is measured by;
// `options` carries the Options gear's file, which no picture of this block may share, and the five generated names of `controls` are
// refused too: the generated tier writes them all into one folder. False, with `error` naming the key.
bool LoadRules(const std::string& path, const Controls::Rules& controls, const Options::Rules& options, Rules& out, std::string& error);

// ---- the layout -----------------------------------------------------------------------------------------------

struct Tile {
    Widget widget = Widget::None;
    Hud::Rect box;
    Hud::Rect face;
    double faceInset = 0.0;
};

struct Layout {
    glm::dvec2 view{0.0};
    std::array<Tile, kWidgetCount> tiles;
    // The bounding box of the six tiles of the column (not Back): the forbidden zone a control dropped in the preview must not enter.
    Hud::Rect column;

    const Tile& At(Widget widget) const;
};

// The widgets on a view, `insets` the safe area's (Back stands from its corner). The column is centred on the view's middle at every
// aspect; Back is where BackBox says.
Layout LayOut(const Rules& rules, const Controls::Rules& controls, const glm::dvec2& viewUnits, const Controls::Insets& insets);

// The Back button's box: a fixed place, a function of the table and the insets alone - never of the size being stepped.
Hud::Rect BackBox(const Rules& rules, const Controls::Rules& controls, const glm::dvec2& viewUnits, const Controls::Insets& insets);

// STRICTLY inside: a point on an edge is outside (a tap on a tile's edge is not a tap on the tile). Hud::Rect::Contains is closed.
bool StrictlyInside(const Hud::Rect& rect, const glm::dvec2& point);

// The widget whose FACE holds the point, strictly. The faces never overlap (a test holds it at every aspect), so the order of kWidgets is
// not a rule.
Widget WidgetAt(const Layout& layout, const glm::dvec2& point);

// ---- the touch -------------------------------------------------------------------------------------------------

// THE ONE TOUCH THE SCREEN FOLLOWS, as Options::Touch follows its own: the touch that goes down on a widget works it only while it is
// inside it, and it acts on the release strictly inside the widget it went down on. A press that begins elsewhere and slides onto a
// widget does nothing, and nor does one let go outside it.
//
// `dead` is the press that OPENED the screen: set when the screen opens, it spends everything until the touch has been lifted, so a press
// still down from the Options button (a release that has not come yet) can neither press a tile nor leave. A fresh press ends it.
struct Touch {
    Widget down = Widget::None; // the widget the touch went down on
    Widget held = Widget::None; // `down`, while the touch is still inside it: drawn at the press tint
    bool dead = false;
};

struct Press {
    bool happened = false;
    Widget widget = Widget::None;
};

// One tick of the touch. `pressed` is the tick it goes down, `released` the tick it comes up, `touching` that it is down this tick, and `on`
// the widget the pointer is on this tick (WidgetAt: strictly inside a face) - none off the game. On the release, `happened` is whether it
// was a press, and `widget` the widget it went down on; the touch is then forgotten (but not `dead`, which only a lift or a press ends).
Press Follow(Touch& touch, bool pressed, bool released, bool touching, Widget on);

// ---- what a press does -----------------------------------------------------------------------------------

// Whether a press on the widget would do anything: false for a tile at its limit (the size at 0.4 or 1.4, the opacity at 0.2 or 1.8) and
// for Restore while no control is moved. The lock and Back always can. Inert, a widget is drawn at `inertAlphaByte` and acts on nothing.
bool Enabled(Widget widget, const Controls::Rules& rules, const Controls::Tuning& tuning);

// Whether any control is moved from its default place: what Restore is enabled by.
bool HasMove(const Controls::Tuning& tuning);

// The tunings are the same: a size, an opacity and the five moves (the look and the arrangement are a capture's and not compared).
bool SameKept(const Controls::Tuning& a, const Controls::Tuning& b);

struct Outcome {
    bool acted = false;          // the widget did something: a step, a restore, the lock, a leave
    bool tuningChanged = false;  // `tuning` differs from the one given
    Controls::Tuning tuning;
    bool unlocked = false;       // the lock after the press
    bool leave = false;          // Back
};

// What a press on `widget` does, given the tuning in force and the lock:
//   * a size step is one grid step (0.1) and RE-LOCKS; at a limit it is inert;
//   * an opacity step is one grid step (0.2) and does NOT re-lock; at a limit it is inert;
//   * the lock toggles (it is never saved: the screen always opens locked);
//   * Restore clears every move, KEEPS the size and the opacity, and re-locks; inert while no control is moved;
//   * Back leaves.
// A size step does not touch a move: sending the controls that now meet home is the next step's (CTL-12), with the drag.
Outcome Act(Widget widget, const Controls::Rules& rules, const Controls::Tuning& tuning, bool unlocked);

// ---- the readouts ----------------------------------------------------------------------------------------------

enum class Row { Size, Opacity };

// The number between a row's two tiles: the VALUE in force, n / 10 for the size and n / 5 for the opacity (one division, as the tuning has
// them), one decimal: "0.7", "1.0", "1.4". Digits and the point only - the tiles are the minus and the plus.
std::string ReadoutText(const Controls::Rules& rules, const Controls::Tuning& tuning, Row row);

// ---- the pictures ----------------------------------------------------------------------------------------------

enum class Element {
    Backdrop,  // a flat quad over the whole view
    Preview,   // one of the five controls, at its box (the layer draws its picture into the face with the tier it resolved)
    HitFrame,  // a flat quad: one side of a control's hit rectangle drawn as a faint frame (unlocked only)
    Outline,   // a flat quad: one side of the breathing outline round a control's box (unlocked only)
    Guide,     // a flat quad across the view at the character's row (unlocked only)
    Tile,      // one of the seven widgets, at its box (the layer draws its picture into the face, or a flat quad on it)
    Readout    // words
};

struct Piece {
    Element element = Element::Backdrop;
    Hud::Rect rect;                          // the box of a Preview and a Tile; the quad of the rest; Readout: its centre is `rect.min`
    Colour colour{255, 255, 255, 255};      // a flat quad's colour; a Tile's flat-tier colour (tinted while held); a Readout's (alpha in [3])
    double alpha = 1.0;                      // a Preview's and a Tile's picture alpha, 0 to 1
    int tintByte = 255;                      // a Tile's picture is multiplied by this (the press tint)
    std::optional<Controls::Id> control;     // Preview, HitFrame, Outline
    Widget widget = Widget::None;            // Tile
    Picture picture = Picture::SizeMinus;    // Tile
    double faceInset = 0.0;                  // the face the picture's body fills: the box shrunk by this
    std::string words;                       // Readout
    std::string font;
    double unitsPerFontPx = 0.0;
};

// What the screen is showing: the lock, the widget held, the state's own ticks (the outlines breathe on them, never on the wall clock),
// the press tint, and the PREVIEW FLAG - clear-portals forced shown, which in a level it is only while a portal is alive. The flag is this
// struct's and nothing else reads it: no level is loaded under this screen and none ever sees it.
struct Scene {
    bool unlocked = false;
    Widget held = Widget::None;
    int ticks = 0;
    int pressTintByte = 255;
    bool previewClearShown = true;
};

// One side of a frame: the four thin rectangles round `rect` (top, bottom, left, right), `outside` them or inside its edge.
std::array<Hud::Rect, 4> FrameStrips(const Hud::Rect& rect, double thickness, bool outside);

// The outline's alpha byte `ticks` into the state: a triangle between middle - swing and middle + swing over the period.
int OutlineAlphaByte(const Rules::Unlocked& unlocked, int ticks);

// The alpha a preview control is drawn at, as the level draws it settled: a walk arrow at AlphaByte of the HUD's own byte, a corner
// button at the HUD's opacity times the idle scale and the opacity in force, capped at 1.
double PreviewAlpha(Controls::Id id, const Controls::Rules& controls, const Hud::Rules& hud, const Controls::Tuning& tuning);

// Every picture of the screen, in the order they are drawn: the backdrop; the five controls from Controls::LayOut at the settled age (the
// same rectangles the level plays); where unlocked, their hit frames, their outlines and the guide; then the seven widgets and the two
// readouts (so a widget is over whatever is under it).
std::vector<Piece> Pieces(const Rules& rules, const Controls::Rules& controls, const Hud::Rules& hud, const Controls::Tuning& tuning,
                          const glm::dvec2& viewUnits, const Controls::Insets& insets, const Scene& scene);

} // namespace MagicPortals::Adjust
