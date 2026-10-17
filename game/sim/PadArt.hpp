#pragma once

// The pictures the modern controls fall back on when no better one is on disk: two arrows, restart, pause,
// clear-portals and a gear, drawn by arithmetic into an in-memory bitmap.
//
// WHY THEY ARE DRAWN. The modern layout (docs/planning/2026-10-15-modern-controls.md, section 4.2) draws each control
// with Magic Rampage's own picture when the owner has it in a gitignored folder, then the original's own sprite, and
// last with these. A run with neither - a fresh clone, every test, the CI machine - must still show five controls that
// look like what they are, and the hard rule says no art, generated or not, is ever committed. So the pictures are not
// files: they are a function of a size, and the layer writes the pixels into its own scratch directory when it needs a
// file at all. Nothing here opens a file, so nothing generated can reach a tracked path.
//
// ONE SHAPE FOR ALL SIX. Every glyph sits on the same rounded plate with a lighter rim, so the plate - and with it the
// alpha channel - is identical for the six: only the colour inside it differs. The plate is the picture's BODY, and it
// leaves `bodyInset` of the bitmap transparent on each side (Magic Rampage's own pictures are 14 px of 128: the layout
// measures a control's box and draws the body exactly into the box's solid face, game/sim/Controls.hpp DrawRect). The
// body's edges lie on pixel boundaries, so the margin is exactly zero alpha and the body's bounding box is exactly the
// box the layout says.
//
// PURE AND REPEATABLE. Each pixel is a 4 x 4 grid of samples of a signed-distance shape, and the samples' coordinates are
// worked from whole numbers, so a pixel and its mirror see exactly mirrored points. Only + - * / and sqrt are used - no
// trigonometry - so the same call gives the same bytes on any machine that does IEEE arithmetic. The colours are STRAIGHT
// (not premultiplied) alpha and a transparent pixel is (0, 0, 0, 0). No pixel is the magenta some engines key on.
//
// The glyphs' proportions and colours are the drawing itself, not tunable game numbers, so they are written here beside
// the code that draws them; the one number the layout owns, the body's margin, comes in as an argument from controls.json.

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "sim/Controls.hpp"

namespace MagicPortals::PadArt {

// The six: the five controls' and the options entry's. `kGlyphs` is THE SIX and test_mp_padart holds each of them to the plate
// invariants (one shared alpha channel, six different pictures); the adjustment screen's seven editor glyphs (CTL-11) do not belong in
// it and go in a list of their own, so that no invariant of the six is asked of them.
enum class Glyph { WalkLeft, WalkRight, Restart, Pause, ClearPortals, Gear };
inline constexpr std::size_t kGlyphCount = 6;
inline constexpr std::array<Glyph, kGlyphCount> kGlyphs = {Glyph::WalkLeft, Glyph::WalkRight, Glyph::Restart,
                                                          Glyph::Pause,    Glyph::ClearPortals, Glyph::Gear};

// For a log line and a test's message.
const char* NameOf(Glyph glyph);

// The adjustment screen's pictures (planning step 192, CTL-11): eight, for its seven widgets - the lock has one for each of its states, as
// Magic Rampage has two sprites for it. Drawn on the same plate as the six, but in a list of their own: test_mp_padart holds the six to the
// plate's invariants (one shared alpha channel, six different pictures) and none of that is asked of these, whose open lock sits on a RED
// plate (Magic Rampage's editing frame) and so shares a colour with no other.
enum class EditorGlyph { SizeMinus, SizePlus, OpacityMinus, OpacityPlus, LockLocked, LockUnlocked, Restore, Back };
inline constexpr std::size_t kEditorGlyphCount = 8;
inline constexpr std::array<EditorGlyph, kEditorGlyphCount> kEditorGlyphs = {
    EditorGlyph::SizeMinus,  EditorGlyph::SizePlus,    EditorGlyph::OpacityMinus, EditorGlyph::OpacityPlus,
    EditorGlyph::LockLocked, EditorGlyph::LockUnlocked, EditorGlyph::Restore,      EditorGlyph::Back};
const char* NameOf(EditorGlyph glyph);

struct Rgb {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;

    friend bool operator==(const Rgb& a, const Rgb& b) { return a.r == b.r && a.g == b.g && a.b == b.b; }
};

struct Pixel {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 0;

    friend bool operator==(const Pixel& x, const Pixel& y) { return x.r == y.r && x.g == y.g && x.b == y.b && x.a == y.a; }
};

// The three colours of every glyph: the plate's inside, its rim, and the shape drawn on it. Light on dark, so the
// control reads on a bright level and on a dark one at the HUD's alpha.
inline constexpr Rgb kPlateFill{44, 47, 63};
inline constexpr Rgb kPlateRim{205, 212, 232};
inline constexpr Rgb kGlyphColour{244, 247, 255};
// The open lock's plate: Magic Rampage's red "editing" frame, the colour of the lock while the controls can be moved.
inline constexpr Rgb kEditingFill{150, 38, 22};
inline constexpr Rgb kEditingRim{232, 120, 90};

// The sizes that are drawn: below the smallest a glyph is a few pixels, above the largest a bitmap is megabytes; and the
// largest margin that leaves a body (a quarter of the picture on each side keeps half of it). The margin is the layout's own
// number, Controls::kMaxGeneratedBodyInset, so that controls.json cannot load an inset that this refuses to draw.
inline constexpr int kMinSize = 16;
inline constexpr int kMaxSize = 1024;
inline constexpr double kMaxBodyInset = Controls::kMaxGeneratedBodyInset;

// A square bitmap, `size` pixels a side, row 0 at the top, four bytes a pixel in the order r, g, b, a.
struct Bitmap {
    int size = 0; // 0: nothing was drawn (Draw refused the request)
    std::vector<std::uint8_t> rgba;

    bool Empty() const { return size == 0; }
    Pixel At(int x, int y) const;
};

// The transparent pixels on each side of a picture of `size` whose margin is `bodyInset` of it: the nearest whole pixel
// (14 for 128 and 0.109375, which is exact).
int BodyMarginPx(int size, double bodyInset);

// The glyph at `size` with a transparent margin of `bodyInset` of it on each side. An empty bitmap, never a partial one,
// for a size outside [kMinSize, kMaxSize] or an inset outside [0, kMaxBodyInset] (a NaN included).
Bitmap Draw(Glyph glyph, int size, double bodyInset);

// An editor glyph at `size` with a transparent margin of `bodyInset` of it on each side: the same rules, the same refusals and the same
// plate (its body fills the picture less the margin) as Draw.
Bitmap DrawEditor(EditorGlyph glyph, int size, double bodyInset);

// The glyph that stands for a control. The gear is none's: it belongs to the Options entry, not to the five.
Glyph GlyphOf(Controls::Id id);

} // namespace MagicPortals::PadArt
