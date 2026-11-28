// THE STONE WALL BESIDE A LEVEL NARROWER THAN THE WINDOW (side_walls.json; planning step 284).
//
// The view is 256 units tall and 256 x the window's shape wide, and Camera::Clamp centres a level narrower than that: on a window wider than
// 16:9 the strips beyond a level's two sides showed only the sky, or the black clear (a player's report). On a level whose two sides the
// level itself closes - an invisible wall down the whole height, which stops the player and the tap there - the owner approved a STONE WALL in
// those strips: the game's own double_block_plat stood on end, in the brightness variant the level's own blocks are, at most three columns of
// it, fading into plain black beyond them.
//
// A PICTURE ONLY. No body, no collision, nothing the simulation or its state hash reads. A level with an open side (1-25, several space levels)
// keeps its picture exactly: ClosedSides says which. Enhanced's own addition: the original never drew anything there, and `enabled: false` in
// the file is the switch that draws the frame as it was.
//
// Pure arithmetic over a parsed scene, in the level's units (x from the left, y DOWN from the top, as the level file writes them). The layer
// reads the pieces, makes one quad each when the level's art is built and shows them while the view is wider than the level.

#pragma once

#include "sim/Tscn.hpp"

#include <glm/glm.hpp>

#include <string>
#include <vector>

namespace MagicPortals::SideWalls {

// The port's side_walls.json.
struct Rules {
    // false builds no wall: the frame as it was before planning step 284.
    bool enabled = false;

    // ---- which sides are closed ------------------------------------------------
    // A side is read this far OUTSIDE the level's edge: a wall whose inner face stands no farther out than this closes it.
    double closedReachUnits = 0.0;
    // The entities that count as a wall: static, unmoving, unbreakable collision. Anything not named here leaves a side open.
    std::vector<std::string> solidEntities;

    // ---- the stone -------------------------------------------------------------
    std::string stoneEntity;  // the block's name without suffix and ".ent": double_block_plat
    std::string blackEntity;  // the .ent whose picture is the backing's and the fade's black: black_bar.ent
    // The block and platform family whose placements in a level say which variant its stone is.
    std::vector<std::string> families;
    // The brightness variants' suffixes, in the order a tie falls to when the world's default is not among the tied.
    std::vector<std::string> variants;
    // The variant of each world (chapters.json's world index) for a level that places none of the family; each one of `variants`.
    std::vector<std::string> worldVariants;
    double turnRad = 0.0;      // the stone's turn, in the level file's sense (positive clockwise on the screen): a quarter, stood on end
    double lightHeight = 0.0;  // the depth, in the level's units, the stone's lights are measured at

    // ---- the layout, per side, outward from the level's edge -------------------
    int columns = 0;                    // at most this many columns of stone
    double columnStepUnits = 0.0;       // between two columns' centres
    double rowStepUnits = 0.0;          // between two pieces' centres down a column
    std::vector<double> rowStartUnits;  // a column's first row centre: column i takes [i % size]
    double backingReachUnits = 0.0;     // the black behind the stone runs this far out from the edge
    double marginUnits = 0.0;           // the backing and the fade run this far above y 0 and below the level's height (a tremble's band)
    double fadeFromUnits = 0.0;         // the stone starts to go into black this far out
    double fadeToUnits = 0.0;           // and is black from here
    int fadeSteps = 0;                  // flat black slices between the two, their alpha on a smoothstep
    double fadeCapUnits = 0.0;          // a last slice of whole black past fadeToUnits, over the stones' outer edge

    // ---- the depths ------------------------------------------------------------
    double zFirst = 0.0;  // the first piece's z (the layer's units); each later piece is zStep in front of the one before it
    double zStep = 0.0;
};

bool LoadRules(const std::string& path, Rules& out, std::string& error);

// Which of a level's two sides its own static collision closes over the whole height, 0 to `boundsPx.y`.
struct Closed {
    bool left = false;
    bool right = false;
    bool Both() const { return left && right; }
};

// THE LINE closedReachUnits outside each edge (x = -reach, and boundsPx.x + reach) must be covered from y 0 to the level's height by the
// rectangles of the scene's walls: entity nodes of `solidEntities` whose Body is a StaticBody2D with one unturned RectangleShape2D, at the
// node's own `scale`. A rigid body, a trigger, a polygon, a turned or a breakable or a moving piece, and anything not named, do not count -
// a side they alone close is called open, and keeps its picture.
Closed ClosedSides(const Rules& rules, const Tscn::Scene& scene, const glm::dvec2& boundsPx);

// The brightness variant's suffix for a level: the one most of the family's entities it places carry; among equals, and where it places none,
// the world's own (`world`, chapters.json's index; one outside the list takes the first variant).
std::string Variant(const Rules& rules, const Tscn::Scene& scene, int world);

// The .ent file of the stone in `variant`: double_block_plat_low_emissive.ent.
std::string StoneFile(const Rules& rules, const std::string& variant);

enum class Kind { Backing, Stone, Fade };

struct Piece {
    Kind kind = Kind::Backing;
    int side = -1;            // -1 left, +1 right
    glm::dvec2 centrePx{0.0}; // the level's units
    glm::dvec2 sizePx{0.0};   // the quad's own, before its turn
    double turnRad = 0.0;     // the level file's sense
    double z = 0.0;           // the layer's units; no two pieces share one
    double alpha = 1.0;       // the black's, for a Backing and a Fade; 1 for a Stone
};

// Both sides' pieces, back to front: the backings, then the stones (each side's columns from the inner one out, each from the top down), then
// the fades (from the inner slice out). `stonePx` is the stone's picture as its .ent stands (128 x 32: its length, then its thickness).
std::vector<Piece> Layout(const Rules& rules, const glm::dvec2& boundsPx, const glm::dvec2& stonePx);

// Whether any of the wall is on a view `viewWidthPx` wide: only a view wider than the level shows what is beyond its sides, since the camera
// holds a wider level's view inside it.
bool Shown(double viewWidthPx, double boundsWidthPx);

} // namespace MagicPortals::SideWalls
