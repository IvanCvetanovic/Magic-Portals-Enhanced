#pragma once

// The language picker and its entries (planning step 239, PORT-35): the screen where the player chooses the language of the game's words, the
// globe on the main menu and the globe in the corner of the Options screen that open it.
//
// An ENHANCEMENT, not the original's: the original has one language. So the screen is built from the original's own parts and nothing more - the
// credits' background and back button, its font - and the rest is flat quads and words, as sim/Options.hpp is. Pure, like it: every picture is a
// function of the view's size, the state's clock, the language on the screen and the widget a held touch went down on. The layer keeps the
// choice and the touch, draws what this says through the screen overlay, and decides what a press does.
//
// WHAT IS NOT HERE. Which language a plate stands for, and the name it shows: the layer hands in the languages' own names (Strings, shaped for
// their own language - an Arabic name is shaped as Arabic whatever language is on the screen). The font the names are drawn in: the layer bakes
// one atlas for all eleven scripts (FontBake, languages.json picker_slot) and names it by Pieces' `kNamesFont` role. Choosing, saving and
// switching: the layer's.
//
// The numbers are game/data/picker.json, read STRICTLY by LoadRules here and apart from every other table (a typo costs the picker and never a
// run). Every colour and place is the port's own and says so in the file.

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "sim/Credits.hpp"
#include "sim/Hud.hpp"
#include "sim/MenuState.hpp"
#include "sim/UiLayer.hpp"

namespace MagicPortals::Picker {

using Colour = std::array<int, 4>; // r, g, b, a, each a byte

// The table, within the port's data directory (the layer's portData, game/data in the source tree).
inline constexpr const char* kDataFile = "/picker.json";

// The font ROLE the plates' names are drawn in. It is not a file of the original's: the layer resolves it to the atlas it baked (and a table never
// names it - tools/package/closure.py would resolve a .fnt as an original asset), so it is a constant here.
inline constexpr const char* kNamesFont = "picker.fnt";

// The file the globe is written to, from picker.json's stem.
inline constexpr const char* kGlyphExtension = ".png";

struct Rules {
    // A button of the globe's: a rectangle placed by a fraction of the screen with an offset in units.
    struct Spot {
        glm::dvec2 atScreen{0.0};
        glm::dvec2 origin{0.0};
        glm::dvec2 offsetUnits{0.0};
        glm::dvec2 sizeUnits{0.0};
    };
    struct Entry : Spot {
        std::string glyph;       // the stem of the file the globe is written to (kGlyphExtension is added)
        int glyphPx = 0;         // the picture's side
        double bodyInset = 0.0;  // the fraction of the picture that is transparent on each side
    } entry;                     // the main menu's: it enters as the main menu's buttons do
    Spot corner;                 // the Options screen's: flat on its box

    struct Title {
        std::string font; // a role name of the original's data directory
        double unitsPerFontPx = 0.0;
        double yAtScreen = 0.0;
        int alphaByte = 255;
        double maxWidthUnits = 0.0; // 0: no limit
    } title;

    struct Grid {
        int columns = 0;
        glm::dvec2 plateSizeUnits{0.0};
        glm::dvec2 gapUnits{0.0};
        double topUnits = 0.0;
        double clearLeftUnits = 0.0;
    } grid;

    struct Plate {
        Colour idleFrame{};
        Colour chosenFrame{};
        double wordUnitsPerFontPx = 0.0;
        int idleWordAlphaByte = 255;
        int chosenWordAlphaByte = 255;
        double maxWidthUnits = 0.0;
    } plate;
};

// picker.json, strictly: every object and number below is mandatory, finite and within its range; `error` names the file and the key. `out` is left as it was on failure.
bool LoadRules(const std::string& path, Rules& out, std::string& error);

// ---- the entries -----------------------------------------------------------------------------------------------------

// The main menu's entry `stateMs` into the state: where its entrance has it, drawn and claimed alike, at the layer's entrance alpha, at the press
// tint while `held`. `picture` is the glyph's box derived so that its solid body fills `rect` (Controls::DrawRect with no face inset), as Options' is.
struct Entry {
    Hud::Rect rect;
    Hud::Rect picture;
    int rgbByte = 255;
    int alphaByte = 255;
};
Entry EntryAt(const Rules& rules, const UiLayer::Rules& layer, const MenuState::Rules& state, const glm::dvec2& viewUnits, double stateMs, bool held);

// The Options screen's corner button on a view, and the same at the press tint.
Hud::Rect CornerRect(const Rules& rules, const glm::dvec2& viewUnits);
Entry CornerAt(const Rules& rules, const MenuState::Rules& state, const glm::dvec2& viewUnits, bool held);

// ---- the screen -------------------------------------------------------------------------------------------------------

// What a touch can be on: nothing, the credits' back button, or the plate of language `index` (the enum's order).
struct Target {
    enum class Kind { None, Back, Plate };
    Kind kind = Kind::None;
    int index = 0; // a Plate's

    friend bool operator==(const Target& a, const Target& b) { return a.kind == b.kind && (a.kind != Kind::Plate || a.index == b.index); }
    friend bool operator!=(const Target& a, const Target& b) { return !(a == b); }
};

// THE ONE TOUCH THE SCREEN FOLLOWS, as sim/Options Follow does for its widgets: the touch that goes down on a target works it only while it is
// inside it (the press tint), and acts on the release inside the target it went down on. A press that begins elsewhere and slides onto a plate
// does nothing, and so does one that goes down on a plate and is let go outside it.
struct Touch {
    Target down;
    Target held; // `down`, while the touch is still inside it: drawn at the press tint
};

struct Press {
    bool happened = false;
    Target target;
};

// One tick of the touch; `on` is the target the pointer is on this tick (none off the game). On the release `happened` says whether it was a
// press, and `target` the target it went down on; the touch is then forgotten.
Press Follow(Touch& touch, bool pressed, bool released, bool touching, const Target& on);

// The rectangle of plate `index` of `count` on a view. The plates are laid out row by row, `grid.columns` to a row, and a row that is not full is
// centred under the others. The grid is centred on the screen and moved right only as far as it takes to clear the back button.
Hud::Rect PlateRect(const Rules& rules, const glm::dvec2& viewUnits, std::size_t index, std::size_t count);

// The grid's rectangle (all its plates) and the centre of its first row's middle - where the title is centred.
Hud::Rect GridRect(const Rules& rules, const glm::dvec2& viewUnits, std::size_t count);
glm::dvec2 TitleAt(const Rules& rules, const glm::dvec2& viewUnits, std::size_t count);

// The target a point is on: the back button where its entrance has it (Credits::BackHitRect), else the plate whose rectangle holds it (closed on
// every edge, as Hud::Rect::Contains), else none.
Target TargetAt(const Rules& rules, const Credits::Rules& credits, const glm::dvec2& viewUnits, double stateMs, const glm::dvec2& point, std::size_t count);

enum class Element {
    Background, // the credits' background (a sprite)
    Back,       // the credits' back button (a sprite)
    Title,      // words
    PlateFrame, // a flat quad
    PlateWord   // words
};

struct Piece {
    Element element = Element::Background;
    std::string file; // a sprite, as ui.json names it, for Background and Back
    Hud::Rect rect;   // a sprite's and a quad's rectangle; for words, the rectangle whose centre they are written on (zero size)
    Colour colour{255, 255, 255, 255}; // a flat quad's colour and a word's (its alpha); a sprite's grey in r and its alpha in a
    std::string words;
    std::string font;
    double unitsPerFontPx = 0.0;
    double maxWidthUnits = 0.0; // words: the slot's limit, 0 for none; the layer fits the words to it
    int plate = -1;             // a PlateFrame's and a PlateWord's
};

// Every picture of the screen `stateMs` after its first frame, in drawing order: the credits' background, its back button (at its bounce and press
// tint), the title, then each plate - its frame and its name. The plate of `chosen` is lit; the target `held` (a touch went down on it and is
// still inside it) is drawn at the press tint. `title` and `names` are the words ready for the font (shaped), `names` in the enum's order.
std::vector<Piece> Pieces(const Rules& rules, const Credits::Rules& credits, const glm::dvec2& viewUnits, double stateMs, std::size_t chosen,
                          const Target& held, const std::string& title, const std::vector<std::string>& names);

} // namespace MagicPortals::Picker
