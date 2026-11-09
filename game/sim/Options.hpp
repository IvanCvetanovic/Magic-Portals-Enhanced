#pragma once

// The Options screen and the main menu's entry to it (planning step 185, docs/planning/2026-10-15-modern-controls.md 2.3): where the
// player chooses between the original's controls (Classic) and the inset ones (Modern), and the button on the main menu that opens it.
//
// An ENHANCEMENT, not the original's: the original has no options screen. So the screen is built from the original's own parts and
// nothing more - the credits' background and back button, its font - and the rest is flat quads and words. Pure, like sim/Credits.hpp:
// every picture is a function of the view's size, the state's clock, the style chosen and the widget a held touch went down on. The layer
// keeps the choice and the touch, draws what this says through the screen overlay, and decides what a press does.
//
// The numbers are controls.json's `options` block, read STRICTLY by LoadRules here (Controls::LoadRules does not read it, so a typo in it
// costs the modern controls and never a classic run: the layer reads it after and outside the chain of ui.json's loaders, planning step
// 181's D9). Every colour and place is the port's own and says so in the file.

#include <array>
#include <optional>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "sim/Controls.hpp"
#include "sim/Credits.hpp"
#include "sim/Hud.hpp"
#include "sim/MenuState.hpp"
#include "sim/UiLayer.hpp"

namespace MagicPortals::Options {

// The two styles, in the order the screen stacks them, top to bottom.
inline constexpr std::array<Controls::Style, 2> kTileStyles = {Controls::Style::Modern, Controls::Style::Classic};

using Colour = std::array<int, 4>; // r, g, b, a, each a byte

struct Rules {
    // The main menu's entry, in the slot the original's omitted Facebook button had: a placed button, whose `sprite` is the NAME of the
    // glyph file the layer writes (a bare file name, Controls::IsBareFileName) and whose `size_units` is its box.
    UiLayer::Placed entry;
    double entryBodyInset = 0.0; // the fraction of the glyph's picture that is transparent on each side

    struct Words {
        std::string words;
        std::string font; // within the original's data directory
        double unitsPerFontPx = 0.0;
        glm::dvec2 atScreen{0.5}; // the words' centre, as a fraction of the screen
        int alphaByte = 255;
        double maxWidthUnits = 0.0; // 0: no limit (planning step 238; Hud::FitUnitsPerFontPx)
    } title;

    struct Tile {
        glm::dvec2 atScreen{0.5}; // the tile's centre, as a fraction of the screen
        std::string word;
    };
    std::array<Tile, 2> tiles; // by kTileStyles
    glm::dvec2 tileSizeUnits{0.0};
    std::string wordFont;
    double wordUnitsPerFontPx = 0.0;
    double wordMaxWidthUnits = 0.0; // 0: no limit
    glm::dvec2 wordAtUnits{0.0}; // the word's centre from the tile's top-left
    Colour idleFrame{};
    Colour chosenFrame{};
    int idleWordAlphaByte = 255;
    int chosenWordAlphaByte = 255;

    struct Thumbnail {
        glm::dvec2 sizeUnits{0.0};
        glm::dvec2 atUnits{0.0};          // from the tile's top-left
        double referenceHeightUnits = 0.0; // the layout is drawn as it is on a view this tall, and as wide as the thumbnail's shape says
        Colour background{};
        Colour walk{};
        Colour corner{};
    } thumbnail;

    // The button that opens the adjustment screen (planning step 192, CTL-11): under the two tiles, drawn and claimed only while the style in
    // force is Modern. A placed rectangle (a fraction of the screen for its centre, units for its size), a word in the original's font on
    // a plate.
    struct AdjustButton {
        glm::dvec2 atScreen{0.5};
        glm::dvec2 sizeUnits{0.0};
        std::string word;
        std::string font;
        double unitsPerFontPx = 0.0;
        glm::dvec2 wordAtUnits{0.0}; // the word's centre from the button's top-left
        Colour frame{};
        int wordAlphaByte = 255;
        double maxWidthUnits = 0.0; // 0: no limit
    } adjust;
};

// controls.json's `options` block. `controls` is the table the file's `art.generated` names were read into: the gear's file must not be
// one of them (the generated tier writes them all into one folder). False, with `error` naming the key.
bool LoadRules(const std::string& path, const Controls::Rules& controls, Rules& out, std::string& error);

// ---- the main menu's entry ---------------------------------------------------------------------------------

// The entry `stateMs` into the main menu's state: where its entrance has it, drawn and claimed alike (the original's Button::isPointInButton
// is the rectangle it is at, no bounce), at the layer's entrance alpha, at the press tint while `held`. The picture is drawn into
// `picture`, derived from the box so that the glyph's solid body fills it (Controls::DrawRect with a face inset of nothing).
struct Entry {
    Hud::Rect rect;
    Hud::Rect picture;
    int rgbByte = 255;
    int alphaByte = 255;
};
Entry EntryAt(const Rules& rules, const UiLayer::Rules& layer, const MenuState::Rules& state, const glm::dvec2& viewUnits, double stateMs,
              bool held);

// ---- the screen ------------------------------------------------------------------------------------------------

// What a touch can be on. The back button is the credits': 64 x 64 at the left edge's middle, a place no main-menu button occupies, so
// the press that returns cannot also press one. `Adjust` is the button under the tiles that opens the adjustment screen: a widget only
// while the style in force is Modern (WidgetAt and Pieces are told).
enum class Widget { None, Back, ModernTile, ClassicTile, Adjust };

Widget WidgetOf(Controls::Style style);
// The style a tile chooses; none for a widget that is not a tile.
std::optional<Controls::Style> StyleOf(Widget widget);

// THE ONE TOUCH THE SCREEN FOLLOWS, as Button::update follows a touch on a button: the touch that goes down on a widget works it only while
// it is inside it (the press tint), and acts on the release inside the widget it went down on. A press that begins elsewhere and slides onto
// a widget does nothing, and so does one that goes down on a widget and is let go outside it.
struct Touch {
    Widget down = Widget::None; // the widget the touch went down on
    Widget held = Widget::None; // `down`, while the touch is still inside it: drawn at the press tint
};

struct Press {
    bool happened = false;
    Widget widget = Widget::None;
};

// One tick of the touch. `pressed` is the tick the touch goes down, `released` the tick it comes up, `touching` that it is down this tick (a
// press and a release in one tick are both of them), and `on` the widget the pointer is on this tick - none off the game. On the release,
// `happened` is whether it was a press, and `widget` the widget it went down on; the touch is then forgotten.
Press Follow(Touch& touch, bool pressed, bool released, bool touching, Widget on);

// A tile's rectangle on a view.
Hud::Rect TileRect(const Rules& rules, Controls::Style style, const glm::dvec2& viewUnits);

// The adjustment button's rectangle on a view.
Hud::Rect AdjustRect(const Rules& rules, const glm::dvec2& viewUnits);

// The widget a point is on: the back button where its entrance has it (Credits::BackHitRect), else the tile whose rectangle holds it
// (closed on every edge, as Hud::Rect::Contains), else - only where `adjustShown` - the adjustment button, else none. The kinds cannot
// overlap at any supported aspect (a test holds it), so the order is not a rule. A button that is not shown is not claimed.
Widget WidgetAt(const Rules& rules, const Credits::Rules& credits, const glm::dvec2& viewUnits, double stateMs, const glm::dvec2& point,
                bool adjustShown = false);

// The five controls of a style as a tile's thumbnail shows them: the pure layout of that style on a reference view (the thumbnail's own
// shape, `referenceHeightUnits` tall), scaled into the thumbnail's rectangle. `tuning` is the modern look in force.
std::array<Hud::Rect, 5> ThumbnailRects(const Rules& rules, const Controls::Rules& controls, const Hud::Rules& hud,
                                        const Controls::Tuning& tuning, Controls::Style style, const Hud::Rect& tile);

enum class Element {
    Background,    // the credits' background (a sprite)
    Back,          // the credits' back button (a sprite)
    Title,         // words
    TileFrame,     // a flat quad
    ThumbnailBase, // a flat quad
    ThumbnailControl, // a flat quad, one of the five
    Word,          // words
    AdjustFrame    // a flat quad: the adjustment button's plate
};

struct Piece {
    Element element = Element::Background;
    std::string file; // a sprite, as ui.json names it, for Background and Back
    Hud::Rect rect;   // a sprite's and a quad's rectangle; for words, the rectangle whose centre they are written on
    Colour colour{255, 255, 255, 255}; // a flat quad's colour and a word's (its alpha); a sprite's grey in r and its alpha in a
    std::string words;
    std::string font;
    double unitsPerFontPx = 0.0;
    double maxWidthUnits = 0.0;          // words: the slot's limit, 0 for none; the layer fits the words to it
    std::optional<Controls::Style> tile; // the tile a piece belongs to
    std::optional<Controls::Id> control;  // ThumbnailControl: which of the five
};

// Every picture of the screen `stateMs` after its first frame, in drawing order: the credits' background, its back button (at its bounce
// and press tint), the title, then each tile - frame, thumbnail base, the five controls, its word. The tile of `chosen` is lit; the widget
// `held` (a touch went down on it and is still inside it) is drawn at the press tint. Where `adjustShown`, the adjustment button (its plate
// and its word) comes last. With a string table (planning step 238: every language but English) the title, the two tiles' words and the
// adjustment button's word are the table's, shaped.
std::vector<Piece> Pieces(const Rules& rules, const Credits::Rules& credits, const Controls::Rules& controls, const Hud::Rules& hud,
                          const Controls::Tuning& tuning, const glm::dvec2& viewUnits, double stateMs, Controls::Style chosen, Widget held,
                          bool adjustShown = false, const Strings::Table* strings = nullptr);

} // namespace MagicPortals::Options
