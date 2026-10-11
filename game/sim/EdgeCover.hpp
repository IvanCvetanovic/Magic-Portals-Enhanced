// THE PIXEL-CENTRE RULE AT THE FRAME'S EDGE (edge_cover.json; planning step 165).
//
// The original draws its frame with GLES2, one sample a pixel, at the pixel's centre
// (the sky's cover, step 145, has the evidence): a quad draws a pixel when it covers
// that pixel's CENTRE. The port's engine draws with 4x MSAA, so a picture whose edge
// lies in the frame's outer half pixel - a left or top edge at 0.3 px, a right edge at
// 1279.69 px - covers the centre of the edge pixel and not all four of its samples:
// the pixel blends with what is behind it (the clear colour, or the sky) where the
// original's is whole.
//
// A STRIP is the remedy that leaves everything else alone: a second quad that covers
// only the gap between the picture's edge and the frame's pixel boundary, over the
// picture's own rows or columns, with the picture's own uv map carried on across it
// (so the pixel's centre, where the strip's fragment is shaded, reads the texel the
// picture reads there). It lies wholly inside the outer pixel, abuts the picture and
// never overlaps it, so a translucent picture is blended once and no pixel that is
// not an edge pixel can move.
//
// THE COMPLEMENTARY HALF (Rules::complement) is the same rule for the picture whose
// INWARD edge lies in the outer pixel - the black bars a tremble moves past the top
// or the bottom of the frame, a picture sliding in from the side - which the first
// half leaves as 4x MSAA blends. Where such a picture covers the pixel's centre, a strip
// draws the rest of the pixel (Snap, StripsOf); where it covers samples of a pixel and
// not its centre, and nothing else of it shows in the frame, the original draws none
// of it, and the layer leaves it out of the frame (Culled). A picture that goes on
// inward from an edge in the pixel's inner half is the one thing neither can take
// away - a strip adds samples and a cull removes a whole quad - so it stays the blend
// it was (Slivers names it, for the log).
//
// Pure arithmetic in the frame's pixels, x from the left and y DOWN from the top. The
// layer reads the quads' places, builds the strips and draws them; this knows no
// registry.

#pragma once

#include <glm/glm.hpp>

#include <array>
#include <cstddef>
#include <string>

namespace MagicPortals::EdgeCover {

// The port's edge_cover.json.
struct Rules {
    // false draws the pictures alone, as the port did before step 165.
    bool enabled = false;
    // The outer half pixel: an edge nearer than this to the frame's side leaves the edge
    // pixel's centre covered. 0.5, the pixel-centre rule itself.
    double bandPx = 0.5;
    // The first sample of the 4x pattern from a pixel's side (Vulkan's standard
    // positions are at 0.125, 0.375, 0.625 and 0.875 of a pixel along each axis): an edge
    // nearer than this leaves no sample of the edge pixel uncovered.
    double firstSamplePx = 0.125;
    // A quad turned by more than this about any axis is not an axis-aligned picture:
    // its edges are not the frame's rows and columns, and it keeps the MSAA blend.
    double turnRad = 1e-6;
    // The complementary half (above): the strips of a picture's inward edge in the outer
    // pixel, and the cull of a picture that covers no pixel centre of the frame. false is
    // the first half alone, as the first build of step 165 drew.
    bool complement = false;
    // The layer's log says each (picture, side) once, the frame it first has a strip, a cull or a kept
    // sliver, and 64 a level at most. true says every one of them EVERY frame, without the cap: a
    // capture of a pan or a tremble then carries the rule's own answer for each frame, to set the
    // frame's edge pixels against. A measuring switch, false in the file as shipped.
    bool logEveryFrame = false;
    // How many strips the layer makes ready when it attaches, before it makes anything else. A
    // strip is an entity, and the renderer breaks a tie between draws of one depth and key by
    // the order the registry stores them in (RenderSystem's `gathered`): an entity made and
    // destroyed in the middle of a run moves another one in the stores (swap and pop), so strips
    // made on demand late in a level would reorder the particles of one system and move a pixel
    // of their glow by a step. Strips made first stand in front of every other entity in every
    // store and never move one.
    std::size_t poolSize = 0;
};

bool LoadRules(const std::string& path, Rules& out, std::string& error);

struct Interval {
    double lo = 0.0;
    double hi = 0.0;
    bool Empty() const { return !(hi > lo); }
};

// A rectangle of the frame, in pixels.
struct Rect {
    Interval x;
    Interval y;
};

// One axis of a picture, `picture.lo` to `picture.hi` in a frame `framePx` pixels
// across, with each end moved to the pixel boundary the pixel-centre rule draws it to
// where that is the FRAME's own edge: the low edge to 0 when it lies in (firstSample,
// band] and the picture covers the edge pixel's centre (its high edge past it), the
// high edge to `framePx` when it lies in [firstSample, band) short of it and the
// picture covers that pixel's centre. With `complement`, the ends that face INTO the
// frame go the same way, to the boundary of the outer pixel they lie in: the high edge
// of a picture that begins outside the frame, in (band, 1 - firstSample] from the near
// side, and the low edge of one that runs on past the far side, mirrored. Every other
// edge comes back as it was: one that leaves every sample covered needs nothing, one
// past the centre is not the original's pixel, and one inside the frame, beyond the
// outer pixel, stays the MSAA blend it always was.
Interval Snap(const Rules& rules, const Interval& picture, double framePx);

// Whether the pixel-centre rule leaves this picture out of the frame altogether (only with
// `complement`): the part of it on the frame lies wholly in one of the outermost pixels
// of an axis - the first or the last row or column - and covers that pixel's centre
// nowhere, so it covers no pixel centre of the frame, and the original draws none of it
// where the port's MSAA would draw the samples it does cover. A picture with anything of it
// in the frame past the outer pixel is never culled: an edge inside the frame stays a blend.
bool Culled(const Rules& rules, const Rect& picture, double frameWidthPx, double frameHeightPx);

// Which side of the frame a strip is on.
enum class Side { Left, Right, Top, Bottom };

struct Strip {
    Side side = Side::Left;
    Rect rect;
};

// The strips a picture needs, at most one a side. A left or right strip runs over the
// picture's own rows and a top or bottom strip over the picture's EXTENDED columns, so
// the corner a picture at two edges leaves is covered once. The long axis of each is
// cut to a pixel beyond the frame, where nothing is drawn. A picture that is not at
// the frame's edge has none, and neither has one a rule leaves off.
struct Strips {
    static constexpr std::size_t kMax = 4;
    std::array<Strip, kMax> strip;
    std::size_t count = 0;
};
Strips StripsOf(const Rules& rules, const Rect& picture, double frameWidthPx, double frameHeightPx);

// The sides of the frame where a picture's edge lies in the inner half of the outer pixel - past
// its centre, within its samples - and the picture goes on inward past that pixel, so that it
// covers samples of a pixel whose centre it does not cover and shows more of itself beside it:
// the one blend a strip cannot take away (it adds samples) and a cull cannot (it takes the whole
// quad). Indexed by Side; the picture is left as it is, and the layer only says so in the log.
struct Slivers {
    std::array<bool, 4> side{};
    bool Any() const { return side[0] || side[1] || side[2] || side[3]; }
};
Slivers SliversOf(const Rules& rules, const Rect& picture, double frameWidthPx, double frameHeightPx);

// The uv map of a strip, where the picture's own is `baseScale` and `baseOffset` over
// its rectangle (u across it from its left, v down it from its top - the quad
// primitive's): the same linear map carried on across the strip, so every texel sits
// where the picture would draw it. `scale` and `offset` are what a material wants.
struct Uv {
    glm::dvec2 scale{1.0};
    glm::dvec2 offset{0.0};
};
Uv UvOf(const Rect& picture, const Rect& strip, const glm::dvec2& baseScale, const glm::dvec2& baseOffset);

} // namespace MagicPortals::EdgeCover
