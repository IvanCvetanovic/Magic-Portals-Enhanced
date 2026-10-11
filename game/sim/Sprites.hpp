#pragma once

// A level's own art: every Sprite2D Enhanced's converter wrote, as the port
// draws it.
//
// The converter gives each entity node that has a picture one Sprite2D child,
// and says only these things about it: the texture, an offset, a scale (from
// the visuals plan's 9.2 on, where the entity states a <Scale>), and - on the
// entity node above it - a z_index and, for a glow, a CanvasItemMaterial whose
// blend_mode is 1. Godot draws a Sprite2D centred, at its image's size times its
// scale with its offset times its scale, and orders a canvas by z_index and then
// by the order of the tree. That is all this reads, and it reads it strictly: a
// sprite that is not a child of an entity node, a texture that is not a res://
// file, or a blend the port does not draw is an error naming the line.
//
// res:// is the directory ABOVE the levels: the converter writes the levels to
// out/levels and the art they name to out/assets, so resolving against the
// levels' parent needs no path of its own. The art, like the levels, is the
// original's and stays outside this repository.
//
// WHICH FILE, AND HOW BIG. The level names the 1x file, and the original never
// draws a named image as it is: its loader takes <dir>/fullhd/<file> or
// <dir>/hd/<file> where one exists, at texels / 2 units (sim/Tiers; the owner's
// rulings R13, the port resolves the tier and the levels stay as the converter
// wrote them, and R12, the choice a 720 px screen makes). The converter copies
// those files beside the 1x copies. So each sprite is drawn from the file
// Tiers::Resolve finds, sized Tiers::Units(texels, density): whole units, the
// same as the 1x file for every tier file that is exactly twice it, and 512 x 256
// for the four skies whose fullhd file is 1024 x 512 over a 455 x 256 one. The
// offset stays the level's, in units: a density never touches it (planning doc
// step 61, decision 2).
//
// AND ITS SCALE, the entity's <Scale>, which Ethanon draws the way Godot does:
// the frame times the scale (ETHSpriteEntity::GetCurrentSize,
// ETHSpriteEntity.cpp:652-683) about the pivot times the scale
// (ETHEntity::ComputeAbsoluteOrigin, ETHEntity.cpp:296). It multiplies the whole
// units, after the truncation, and the offset as the level wrote it (00_order K9):
// never the texels or the density. A lightmap is still checked against the units
// before it (Lighting::Read). Tscn refuses a scale on any node but a Sprite2D,
// and one that is not two numbers above 0.
//
// Renderer-free, like the rest of the simulation side. An image's size comes
// from its header (ImageSize) rather than from decoding it.

#include "sim/Tiers.hpp"
#include "sim/Tscn.hpp"

#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace MagicPortals::Sprites {

struct Sprite {
    std::string node;          // the entity node it belongs to, e.g. "wall_w3_ent_832"
    std::string texture;       // the image drawn: the tier file of its res:// path under resRoot, or that path
    std::string tier;          // the folder the tier file was found in ("fullhd", "hd"); empty for the file named
    float density = 1.0f;      // texels per unit in that file: 1 for the file named
    glm::ivec2 texels{0};      // the drawn file's own size, from its header
    glm::dvec2 scale{1.0};     // the Sprite2D's scale: its entity's <Scale>, 1 where it states none
    glm::dvec2 sizePx{0.0};    // its size in the level's units: Tiers::Units(texels, density) x scale
    glm::dvec2 atPx{0.0};      // the entity node's position
    double rotation = 0.0;     // the entity node's, in Godot's radians: clockwise on the screen
    glm::dvec2 offsetPx{0.0};  // the sprite's offset x scale, in the entity node's frame
    int zIndex = 0;            // the entity node's z_index
    double depth = 0.0;        // its node's eth_z, unrounded (0 where it states none): z_index is its rounding
    int order = 0;             // its place in the level's drawing order, from the back
    bool additive = false;     // blend_mode 1: added to what is behind it rather than mixed
};

// An image drawn by name, as the original's loader draws every image it loads -
// a level's sprites, the entities the script adds, their halos and normal maps,
// particle bitmaps (ETHGraphicResourceManager::AddFile, ETHResourceManager.cpp:133-144;
// ETHParticleManager.cpp:107, :170 for a particle's): the file Tiers::Resolve finds
// for the name, its texels from its header, and its size in whole units. Find draws
// a level's art through it, and the layer the port's own pictures and every particle
// bitmap (00_order 2.3).
struct Image {
    std::string path;     // the file drawn: the tier file, or the name itself
    std::string tier;     // the folder it was found in ("fullhd", "hd"); empty for the name itself
    float density = 1.0f; // texels per unit in that file: 1 for the name itself
    glm::ivec2 texels{0}; // the drawn file's own size, from its header
    glm::ivec2 units{0};  // Tiers::Units(texels, density): whole units
};

// False, with `error`, for a drawn file that cannot be sized (ImageSize's reason)
// or that is less than one unit on an axis at its density. `out` then still says
// which file was tried, with texels and units 0.
bool Drawn(const Tiers::Rules& tiers, const std::string& named, Image& out, std::string& error);

// Every sprite of a level, in drawing order: by z_index, then as the file lists
// them. `resRoot` is the directory res:// stands for, and `tiers` the search each
// named image goes through (data/tiers.json; Tiers::Rules{} searches nothing and
// draws every file as named). False, with `error`, for anything the reader does
// not know, and for an image it cannot size - the tier file's, when it is drawn.
bool Find(const Tscn::Scene& scene, const std::string& resRoot, const Tiers::Rules& tiers, std::vector<Sprite>& out,
          std::string& error);

// Where a sprite's centre is, in the level's pixels: the node's position plus
// the offset, turned with the node.
glm::dvec2 CentrePx(const Sprite& sprite);

// ---- WHICH OF TWO PICTURES AT ONE DEPTH IS DRAWN OVER THE OTHER ------------------
//
// Find's order is Godot's: z_index, then the file. Ethanon's is not. Its renderer
// keys one multimap by a draw hash that, for a type-0 entity, is the entity's z
// normalised over the scene's height range (ETHEntityRenderingManager.cpp:119-131),
// so two pictures at one z TIE, and a tie is drawn in the order they were inserted
// (std::multimap::insert puts an equal key after its peers) - which is the order
// ETHScene::MapEntitiesToBeRendered walks the entities in: the visible buckets one
// after the other, a row at a time and each row from its lowest column to its
// highest (ETHBucketManager::GetIntersectingBuckets, :52-90: y is the outer loop),
// and inside one bucket its list (ETHScene.cpp:541-576). The bucket of an entity is
// floor(position / the scene's bucket size) (ETHBucketManager.cpp:47-50), and the
// scene's is 128 x getScale drawn pixels, 128 of the level's units at any screen
// (art.json draw_order). So the picture whose entity stands in the LATER bucket - the
// lower row, or the same row and the higher column - is drawn over the other.
//
// WHAT IS NOT BUILT: inside ONE bucket the order is the file's, as Godot's is. The
// step-118 note says SGlobalScale::scaleEntities moves each entity the scale carries
// to another bucket, and ETHBucketManager::MoveEntity (:245-252) puts a type-0 entity
// at the FRONT of its new bucket, so the moved ones would sit in REVERSE file order
// ahead of the unmoved. None of the 9 pairs the comparison read stands in one bucket;
// the one same-bucket pair that can be read (4-16's block00_ent_2393 and
// double_block_plat_ent_2439, both moved, both in bucket (0, 3)) is drawn in the file's
// order by the original: on
// the 2,448 frame pixels where the two pictures differ, 0.81 grey levels from the port's
// frame against 68.9 from the other order (art.json draw_order._not_built).

// The rule (art.json draw_order): the scene's bucket, in the level's units, and how far under a bucket line a position may read
// and still be ON it. The levels put entities on the lines exactly - x = 128, y = 256 - and the original's positions are float
// values that stay exact; the port's bodies hold metres, and an entity that never moved reads back 127.99999 from them, which
// floor would put in the bucket before. The tolerance is far above that noise (about 1e-4 of a unit at the level's edge) and far
// below anything a body does.
struct DrawOrder {
    double bucketUnits = 0.0;
    double lineToleranceUnits = 0.0;
};

// A bucket: row = floor(y / bucketUnits), column = floor(x / bucketUnits), each read the tolerance's way. Ordered the way the
// original walks them: by row, then by column.
struct Bucket {
    int row = 0;
    int column = 0;
    auto operator<=>(const Bucket&) const = default;
};
Bucket BucketOf(const glm::dvec2& at, const DrawOrder& rule);

// One picture's standing for Rank: its z_index and unrounded depth, where its
// entity stands, and the slot it holds now.
struct Standing {
    int zIndex = 0;
    double depth = 0.0;
    glm::dvec2 at{0.0};
    int slot = 0;
};

// Re-deals the slots of `standing`, which is in Find's order (z_index ascending, the
// file's order inside one z_index): within each run of one z_index the slots it holds
// go back out by (depth, bucket row, bucket column, place in the run), so that the
// slots stay the same SET - nothing after the run moves, and a gap Find left (a
// picture the layer took out) stays a gap. Where the buckets are all one, every slot
// goes back to where it was. Pure in `standing`: the same standings give the same
// slots whatever they held before.
void Rank(std::vector<Standing>& standing, const DrawOrder& rule);

// An image's size, from its header: a PNG or a BMP, the two kinds of file the
// converter copies, or a JPEG, which five of the original's entities name for a
// particle system (particles/explosion.JPG). A JPEG is sized only when its
// segments and frame header are laid out as the renderer's decoder reads them;
// the tables and the components' sampling are left to the renderer. False, with
// `error` naming the file, for anything else.
bool ImageSize(const std::string& path, int& width, int& height, std::string& error);

} // namespace MagicPortals::Sprites
