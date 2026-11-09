#pragma once

// The credits screen the main menu's info button opens - CreditsScreen, with
// CreditsScreenLayer - as ui.json's `credits` block and the arithmetic of its
// update: the back button's place, entrance and bounce, the papyrus, and the one
// image of names that scrolls up it for ever.
//
// Pure, like sim/MainMenu.hpp: every picture is a function of the view's size,
// the state's clock, the scroll and whether the back button is held. The layer
// keeps the scroll between ticks and steps it once a tick (Enhanced's ui3 spec
// 8.1, P1), and draws these pieces through the screen overlay.

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "sim/Hud.hpp"
#include "sim/MenuState.hpp"
#include "sim/UiLayer.hpp"

namespace MagicPortals::Credits {

struct Rules {
    UiLayer::Rules layer;
    MenuState::Rules state;

    struct Background {
        std::string sprite; // named within the original's assets
        glm::dvec2 sizeUnits{0.0};
        glm::dvec2 centreOfScreen{0.5};
    } background;

    UiLayer::Placed back;
    MenuState::Bounce backBounce;

    struct Papyrus {
        std::string sprite;
        glm::dvec2 xOfWidth{0.0}; // the two factors of the width its left edge is at
        glm::dvec2 sizeUnits{0.0};
    } papyrus;

    struct Strip {
        std::string sprite;
        glm::dvec2 sizeUnits{0.0};
        int alphaByte = 255;
        double screenPxPerSecond = 0.0; // UnitsPerSecond's argument: screen pixels
        double atScreenPx = 0.0;        // the screen height those pixels are taken at (P2)
        double flingDecayPerTick = 0.0;
    } strip;

    // ENHANCED (planning step 212): a closing card under the original's picture, on the same strip, in the original's fonts: who made this version and its
    // engine. Data (ui.json `credits.tail`), centred in the column; the original's strip ends one unit under the Ethanon logo and has none.
    struct Tail {
        double heightUnits = 0.0;  // how far the strip grows below its picture
        double centreXUnits = 0.0; // the column's middle, from its left edge
        int inkByte = 0;           // the tint of the (white) glyphs: the picture's ink is black
        struct Line {
            std::string words;
            std::string font; // a file of the original's data/ folder
            double unitsPerFontPx = 0.0;
            double centreYUnits = 0.0; // from the strip's top
            // The string table's key for this line's words (planning step 238), where the words are a sentence that is translated; empty
            // for a name or an address, which every language keeps. The `words` stay as the English the line is laid out with.
            std::string textKey;
            double maxWidthUnits = 0.0; // 0: no limit (Hud::FitUnitsPerFontPx)
        };
        std::vector<Line> lines;
    } tail;
};

// ui.json's `ui_layer`, `menu_state` and `credits`, strictly.
bool LoadRules(const std::string& path, Rules& out, std::string& error);

// P2: the strip's speed in units a second at any view - the original's pixels a
// second as they are on a screen `strip.atScreenPx` tall, 256 units.
double UnitsPerSecond(const Rules& rules, const glm::dvec2& viewUnits);

// The strip's top edge, and the last move a touch gave it.
struct Scroll {
    double y = 0.0;
    double moveSpeed = 0.0;
};

// The constructor's: the strip's top on the view's bottom edge, still.
Scroll Start(const glm::dvec2& viewUnits);

// One tick of CreditsScreenLayer::update, `tickMs` long. While `touching`, the
// strip goes with the touch's move this tick, which becomes the fling; otherwise
// the fling decays by a tick's factor and the strip rises by a tick's speed less
// it. Then the wrap: above its own height off the top, it comes back in at the
// bottom edge; below the bottom edge, back to above the top.
void Step(const Rules& rules, Scroll& scroll, const glm::dvec2& viewUnits, double tickMs, bool touching,
          double moveUnitsY);

// The papyrus's and the strip's left edge.
double ColumnX(const Rules& rules, const glm::dvec2& viewUnits);

// The whole strip: the original's picture and the closing card under it. The wrap is taken at this, not at the picture's height.
double StripHeight(const Rules& rules);

// One line of the closing card as drawn this tick: its centre in view units, and the ink and alpha it is tinted with. The card is NOT a Piece: it
// is words and not a picture, so Pieces (and the Options screen, which borrows the credits' background and back button from it) stay as the
// original has them.
struct Words {
    std::string text;
    std::string font;
    glm::dvec2 centre{0.0};
    double unitsPerFontPx = 0.0;
    double maxWidthUnits = 0.0;
    int rgbByte = 0;
    int alphaByte = 255;
};
// With a string table (planning step 238: every language but English) the lines that name a `text_key` are the table's words, shaped.
std::vector<Words> TailWords(const Rules& rules, const glm::dvec2& viewUnits, const Scroll& scroll, const Strings::Table* strings = nullptr);

enum class Element { Background, Back, Papyrus, Strip };

struct Piece {
    Element element = Element::Background;
    std::string file; // the sprite, as ui.json names it
    Hud::Rect rect;   // as drawn: the bounce included
    int rgbByte = 255;
    int alphaByte = 255;
};

// Every picture `stateMs` after the state's first frame, in the original's order:
// the scene's background, the layer's back button (UILayer::draw), then
// CreditsScreenLayer::draw's papyrus and strip. `backHeld` draws the button at the
// press tint.
std::vector<Piece> Pieces(const Rules& rules, const glm::dvec2& viewUnits, double stateMs, const Scroll& scroll,
                          bool backHeld);

// Button::isPointInButton's rectangle for the back button this tick: where its
// entrance has it, with no bounce.
Hud::Rect BackHitRect(const Rules& rules, const glm::dvec2& viewUnits, double stateMs);

} // namespace MagicPortals::Credits
