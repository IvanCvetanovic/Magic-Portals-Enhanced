// THE SIDES OF A MENU PICTURE ON A WIDE WINDOW (side_fill.json; planning step 212).
//
// The original's menu pictures are painted for the screens of 2013: the main menu's is 456 units wide of a 512-unit file (28 units of black
// padding on each side) and 256 tall, so a window wider than 16:9 shows the picture in the middle and the engine's clear colour, black, at both
// sides. A phone is 19.5:9 to 21:9; the owner asked for those columns to be filled with something more polished than black.
//
// What this draws is the picture's own edge, carried on outward and darkened toward the window's side: for a picture of painted scenery, ONE
// COLUMN of its outermost texels stretched across the gap (the "edge" mode: no second dragon, no seam), and a feather of flat black quads
// whose alpha rises from 0 at the seam to `max_alpha` at the window's edge, so the extension is a shadow and not a smear. A picture of paper
// ornaments takes the "mirror" mode (its content reflected about the seam, with a lighter feather). Both are quads of the screen overlay, drawn
// right after the picture and before the buttons, so nothing is under a button that was not before; nothing is hit-tested (they are not
// pieces).
//
// Quads() is empty when the picture covers the window's width: at 16:9 and at 4:3 a menu frame is exactly the frame it was, quad for quad.
//
// Pure arithmetic in view units (x from the left, y down); the layer reads the quads and draws them. Enhanced's own addition: the original never
// had it, and `enabled: false` in the file is the switch that draws the original's frame.

#pragma once

#include "sim/Hud.hpp"

#include <glm/glm.hpp>

#include <string>
#include <vector>

namespace MagicPortals::SideFill {

enum class Mode { Edge, Mirror };

// One picture the fill knows, by the sprite name ui.json gives it.
struct Picture {
    std::string sprite;
    Mode mode = Mode::Edge;
    // The picture's visible content in its own units from its left edge: the main menu's file has 28 units of black each side, measured.
    double contentFrom = 0.0;
    double contentTo = 0.0;
    // How far inside the content the edge column is sampled (Edge): half a unit, one texel of the 2x file, and never the padding.
    double sampleInsetUnits = 0.5;
    // The darkness at the window's own edge, and the curve to it (1 is linear).
    double maxAlpha = 0.0;
    double power = 1.0;
};

struct Rules {
    bool enabled = false;
    int steps = 1;            // flat quads a side's feather is cut into
    double overlapUnits = 0;  // the textured quad runs this far under the picture, over its antialiased first texel
    double minGapUnits = 0.0; // a gap narrower than this draws nothing
    std::vector<Picture> pictures;
};

// side_fill.json, strictly.
bool LoadRules(const std::string& path, Rules& out, std::string& error);

struct Quad {
    Hud::Rect rect;           // view units
    glm::dvec2 uvMin{0.0};    // textured quads only
    glm::dvec2 uvMax{1.0};
    glm::vec4 colour{1.0f};
    bool textured = false;    // the picture's own sprite, else a flat colour
};

// The quads, in drawing order, that fill the gap at each side of `picture` (its rect in view units, as the layer draws it) in a view
// `viewUnits` wide. Empty when the fill is off, when `sprite` is not one it knows, or when neither gap is wider than minGapUnits.
std::vector<Quad> Quads(const Rules& rules, const std::string& sprite, const Hud::Rect& picture, const glm::dvec2& viewUnits);

} // namespace MagicPortals::SideFill
