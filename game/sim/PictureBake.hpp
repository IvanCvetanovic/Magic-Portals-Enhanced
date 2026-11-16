#pragma once

// The words the original draws as PIXELS, drawn again in the language on the screen (planning step 247, LANG-2).
//
// WHY. The string table (sim/Strings) is translated in ten languages, but eleven of the original's pictures carry their words as pixels: TAP START,
// Achievements, Select chapter, points, level finished, portals created, game over and four more (golden score, current score, the game-finished card, the
// credits strip). A player who chose German still read TAP START. This module makes a copy of such a picture whose words are the language's.
//
// WHAT IT DOES, per recipe (game/data/picture_recipes.json, which holds numbers about GEOMETRY and nothing of the pictures):
//   - a PLATE (the picture is a plate with a word on it): the English word is ERASED from the decoded picture by a model made of the picture's own text-free
//     rows and columns (no donor picture and no ink detection: a rectangle is replaced by `row_lerp`, `separable` or `twin`, below), then the translated
//     phrase is DRAWN into the rectangle with the committed faces, with the drop shadow the original's word had;
//   - a GENERATED picture (the picture is only words on a transparent ground): no pixel of the original reaches the output. The sprite is still decoded and
//     checked by its size and digest, so that a machine without the original's picture, or with another version of it, keeps the English one; none of its pixels
//     is used. The phrase is drawn on a transparent canvas of the same size, with the look the original's lettering had (white ink, a soft shadow, the shade
//     along the top and left edges);
//   - the result is a same-size PNG in the scratch folder, under a name that carries the recipe, the language and a hash of every input.
//
//   - (planning step 248) a PLAQUE is a plate with a straight frame and a medal on it: the old lettering is replaced by the plate's own model (a frame column is constant down the
//     plate, an interior pixel is a vertical profile times a horizontal vignette, the profile is interpolated and never extrapolated) and a chroma rule keeps the medal and a ring
//     round it; a PANEL is the same without the frame columns; a STRIP is a picture of words and names on a transparent ground whose word rows are CLEARED and drawn again,
//     in bands, left-aligned, black, with no shadow, the names and the logos untouched by construction (asserted).
//
// WHAT IT DOES NOT DO. English is never baked: the layer asks for the shipped file and gets it, byte for byte (the layer's hook, MagicPortalsLayer::wordedPicture).
// It does not choose a language, shape Arabic (Strings::Display does, once per line, here too), load a face (FontBake::LoadFaces) or draw a menu.
//
// FAIL CLOSED. Every check a recipe states is run on every bake: the picture's size and digest (another version of the sprite is refused), the rows and
// columns the erase model reads must agree with each other (the guard numbers of the recipe), the twin must be bit-exact outside the word's window, the
// phrase's code points must all be in their faces, the fit must reach the recipe's floor, no ink may fall outside the rectangle, and the PNG must reach the
// disk whole. If any fails the result says why in one line and the caller keeps the English picture: nothing throws, aborts or leaves a half-written file.
// A phrase that needs a scale below its floor in a SHIPPED language is a defect of the translation, and tests/test_mp_overflow.cpp fails it.
//
// THE LOOK. One pipeline for both kinds (the prototype's, read from the original's own lettering): every code point goes to the face its script routes to
// (FontBake's routing, by the recipe's slot), is rasterised with stb_truetype at SUPERSAMPLE times the size, placed by float advances with no kerning (the
// original applies none), emboldened by an octagon grey dilation where the recipe says, and box-filtered down to a coverage C. The shadow is
// S = min(cap, gain * Gauss(sigma)(C moved by (dx, dy))), the ink is white (shaded by `inner` where the original's was), and the two are composed over the
// picture's own pixels as straight-alpha black-then-white 'over'. The size of a word is a measure of one reference glyph (a capital H, an x, the kanji for
// day, the alef) times the recipe's px and the fit scale, never above 1: a phrase that does not fit its box is shrunk, stepwise, down to the recipe's floor.
//
// Spaces: an ASCII space is a place a line may break and, except in Japanese, a gap that is drawn; in Japanese it is a place to break and never a gap. U+200B is
// a place to break and never drawn in any language. U+00A0 is a gap that does not break.
//
// Free of the engine's renderer, like the rest of sim/. No non-ASCII character is written in this file or in its source: every such character lives in JSON.

#include "sim/FontBake.hpp"
#include "sim/Strings.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace MagicPortals::PictureBake {

// Where the baked pictures go, inside the layer's scratch folder (FontBake::kScratchFolder's sibling), and the table that says how: both within the port's data
// directory (game/data in the source tree).
inline constexpr const char* kScratchFolder = "pictures";
inline constexpr const char* kRecipesFile = "/picture_recipes.json";
inline constexpr int kRecipesVersion = 1;

// How many times finer than the picture the glyphs are rasterised before they are filtered down. A change of it changes every pixel the bake makes, so it is part
// of the algorithm's name (kAlgorithm in the source), and not a recipe's number.
inline constexpr int kSupersample = 4;

// ---- pictures ---------------------------------------------------------------------------------------------------------------------------

// A decoded picture: straight-alpha RGBA, 8 bits a channel, row after row from the top.
struct Image {
    int width = 0;
    int height = 0;
    std::vector<unsigned char> rgba;

    bool Empty() const { return width <= 0 || height <= 0 || rgba.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4; }
    unsigned char* At(int x, int y) { return rgba.data() + (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)) * 4; }
    const unsigned char* At(int x, int y) const { return rgba.data() + (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)) * 4; }
};

// A transparent picture of the given size (empty for a size that is not positive or is absurd).
Image Blank(int width, int height);

// The PNG's pixels as RGBA, whatever its own format (stb_image's conversion, which is also the engine's loader's). False, with `error`, for bytes that are no
// picture or a picture larger than kMaxSide on a side.
inline constexpr int kMaxSide = 8192;
bool DecodePng(std::string_view bytes, Image& out, std::string& error);
// A file, read whole and decoded.
bool ReadImage(const std::string& path, Image& out, std::string& error);
// stb's PNG encoder, which is a function of the pixels and nothing else: the same picture is the same bytes.
bool EncodePng(const Image& image, std::string& bytes);
// 32 hex characters naming the decoded pixels and the size (two FNV-1a lanes: an identity check against another version of a sprite, not a security measure).
std::string Digest(const Image& image);

// ---- recipes ------------------------------------------------------------------------------------------------------------------------------

// Half-open, in the picture's own pixels, y down.
struct Rect {
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
    int Width() const { return x1 - x0; }
    int Height() const { return y1 - y0; }
    bool Valid() const { return x1 > x0 && y1 > y0; }
    bool Contains(int x, int y) const { return x >= x0 && x < x1 && y >= y0 && y < y1; }
};
// Columns or rows [a, b).
struct Span {
    int a = 0;
    int b = 0;
};

// How the English word leaves the picture. The first three READ THE PICTURE and replace a rectangle of it; Generated uses none of its pixels: the picture is made of the
// words alone (CheckSource still refuses a sprite of another size or digest, but none of its pixels is used). (A later step adds rules as enumerators and
// data: the loader refuses a rule it does not know.)
enum class Rule {
    // The plate under the word is flat along x: a row of the picture is filled with the line between the mean of two text-free column bands, one each side.
    RowLerp,
    // The plate is a vertical profile times a horizontal vignette: the vignette comes from the text-free rows, the profile from the text-free columns.
    Separable,
    // The plate is another shipped picture, moved: the rectangle is copied from it, and the two must be bit-exact everywhere but in the word's window.
    Twin,
    Generated,
    // Planning step 248. A plaque: frame columns are constant down the plate (F(x), a median over the text-free rows), an interior pixel is V(y) * H(x) per channel (H from the
    // text-free rows, V a piecewise-linear line through the text-free rows above and the margin columns beside the old lettering, never extrapolated); the rectangles of the
    // recipe are replaced by the model, except a pixel within a radius of a chromatic one (a medal), which is never touched.
    Plaque,
    // A panel: V(y) is read for EVERY row from the left margin columns, H(x) from the rows that hold no ink; one rectangle is replaced.
    Panel,
    // The words are rows of a picture of words and names on a transparent ground: the rows are cleared (all columns), nothing else is touched. Each cleared band must be bounded
    // by fully transparent rows, and no ink window of the text may hold an opaque pixel outside the cleared rows.
    ClearRows,
};

// What a plate's erase must find true of the picture it reads, or the English picture is kept. Numbers of grey levels (0..255).
struct Guard {
    double agreeMax = 0.0;       // RowLerp: the two reference bands' means differ by at most this on every row of the rectangle
    double collarMax = 0.0;      // Separable and RowLerp: the model grown by 3 px agrees with the picture in that collar to this
    double holdoutMean = 0.0;    // Separable: the fill from the left bands alone against the one from the right alone, on average
    double holdoutMax = 0.0;     // ... and at worst
};

// Plaque: what the model must find true of the picture it reads (grey levels), or the English picture is kept.
struct PlaqueGuard {
    double sepMean = 0.0;      // V(y) H(x) predicts the text-free rows to this mean ...
    double sepMax = 0.0;       // ... and at worst to this
    double holdout = 0.0;      // V made from every other row of the top rows predicts the rest to this
    Span marginWindow;         // the rows whose two margin columns must agree ...
    double marginRatio = 0.0;  // ... for at least this share of them
    double ringMax = 0.0;      // the three rows above the first rectangle, predicted by a model built WITHOUT them, are within this
    double slopeBand = 0.0;    // V's slope across a hole in its knots is within this (per row) of its slope above the hole; 0: not checked
};

// Panel: the same in the panel's terms.
struct PanelGuard {
    double sepMax = 0.0;       // V(y) H(x) predicts the ink-free rows to this at worst
    double vMargin = 0.0;      // the left margin's V against the centre's on those rows
    double agreeRatio = 0.0;   // the share of the rectangle's rows whose two margins agree to agreeTol
    double ringMax = 0.0;      // the three rows above and below the rectangle
};

// A chromatic pixel (a medal) and a radius round it are never replaced; the count of chromatic pixels the picture holds is a check of its own.
struct Chroma {
    bool enabled = false;
    int threshold = 0;         // max(rgb) - min(rgb) above this, with alpha above zero
    int radius = 0;            // Chebyshev, in pixels
    int keepMin = 0;           // the kept pixels (chromatic ones and their rings) number between these
    int keepMax = 0;
};

struct Erase {
    Rule rule = Rule::Generated;
    Rect rect;                    // everything the bake may change: the erase, the new word and its shadow, nothing outside it
    std::vector<Span> refCols;    // text-free over every row of `rect`
    std::vector<Span> refRows;    // text-free across the whole width (Separable)
    Span flatCols;                // inside the flat part of the plate (Separable)
    Guard guard;
    // Twin: the other shipped picture, the shift that lays it on this one (the twin's pixel at (x - shiftX, y - shiftY) is this picture's at (x, y)), the window
    // the English word is in, and how many pixels there must differ in it (a wrong picture differs nowhere, or everywhere).
    std::string twinSprite;
    std::string twinDigest;
    int shiftX = 0;
    int shiftY = 0;
    Rect checkRect;
    int minInside = 0;
    // Plaque and Panel (ref_cols are the left and the right margin columns, ref_rows the rows that hold no ink; flatCols, for a plaque, are optional).
    Span interiorCols;
    std::vector<Span> frameCols;  // Plaque: the straight frame's columns
    Span centerCols;              // where H(x) is 1
    Span vTopRows;                // Plaque: the rows V is read from the centre columns of
    Span vMarginRows;             // Plaque: the rows scanned for margin knots
    double agreeTol = 0.0;        // two margins agree when they differ by at most this
    std::vector<Rect> rects;      // Plaque: the rectangles replaced; `rect` is their bounding box
    bool hasMirror = false;       // Plaque: columns a medal covers take the mirror column's H
    Span mirrorCols;
    int mirrorAxis = 0;
    bool hasFlat = false;         // Plaque: columns where H is 1
    bool hasSkip = false;         // Plaque: columns the checks leave out (the medal's)
    Span skipCols;
    Chroma chroma;
    PlaqueGuard plaque;
    PanelGuard panel;
    // ClearRows
    std::vector<Span> clearRows;  // [a, b) rows, all columns
};

enum class Layout {
    Word,  // one line, its ink centred on the anchor; the word is sized by a reference glyph and shrunk until its ink fits a box
    Stack, // up to maxLines lines broken at spaces, the block's ink centred on the anchor, shrunk until the widest line and the block fit
    // Planning step 248. A paragraph: at each scale from 1 down, the arrangement with the FEWEST lines whose widest line fits (then the narrowest widest; no line narrower than
    // `orphan` of the widest), lines centred on the anchor's x by their ink, the block's ink centred on its y, shrunk until the ink is inside `safe`; a coarser scale that
    // needs fewer lines is taken when it is within `saveLines` of the finest feasible one.
    Paragraph,
    // Planning step 248. Bands, each a phrase of its own (its own key, size, first baseline, rows): lines LEFT-aligned on `left` by their ink, the fewest lines that fit at the
    // largest scale, the block slid up where a descender would pass the band's last ink row.
    Bands,
};

// Where a band's phrase may be cut into lines: at any space, or only after a dash that stands alone (" - "), so that a dash ends a line and never starts one.
enum class BreakAt {
    Space,
    AfterDash,
};

// What a size is a multiple of: the HEIGHT OF A REFERENCE GLYPH of the face - its capital H, the kanji for day and the alef as languages.json names them, or the x
// of a Latin face where the recipe says so - times the recipe's px, the fit scale and the face's ratio.
struct FaceTuning {
    std::string face;             // a face id of the plan: "kurale"
    double ratio = 1.0;           // the face is this much bigger than the measure says (the kanji and the alef are drawn larger than a Latin x)
    std::uint32_t reference = 0;  // the glyph the face is measured by; 0 is the plan's (FaceSpec::reference)
    double embolden = 0.0;        // the coverage is grown by this fraction of the (scaled) px, an octagon
    double pitch = 0.0;           // Paragraph: the baselines of lines set in this face are this multiple of the (scaled) px apart, at least
};

struct ShadowLook {
    double dx = 0.0;              // fractions of the measure's scaled px
    double dy = 0.0;
    double sigma = 0.0;
    double sigmaMin = 0.0;        // pixels
    double gain = 0.0;
    double cap = 1.0;             // the shadow's alpha never exceeds this
    bool roundOffsets = false;    // the offset is rounded to whole pixels first
};

struct InnerLook { // ink = 1 - k * Gauss(sigma)(shift(1 - C)): the shade the original's ink has along its left and top edges
    double k = 0.0;
    double sigma = 0.0;
    double dx = 0.0;
    double dy = 0.0;
};

// One band of a Bands text.
struct Band {
    std::string key;              // the string table's key whose phrase it draws
    double px = 0.0;              // the reference glyph's height at scale 1, as Text::px
    double firstBaseline = 0.0;   // the first line's baseline at scale 1 (the LAST line's where anchorLast: the block grows upward)
    bool anchorLast = false;
    double pitch = 0.0;           // the baselines' distance at scale 1
    int maxLines = 1;
    Span inkRows;                 // [a, b): where the band's ink may lie (the rows the original's band has, and the transparent rows between it and its neighbours)
    BreakAt breakAt = BreakAt::Space;
    std::vector<FaceTuning> faces; // tunings that replace Text::faces' of the same face id for this band (the heading's own embolden)
    // BALANCE (planning step 249). A band that wraps to `balanceMinLines` lines or more is MOVED, after its size and its lines are fitted by its baseline as they always were
    // (the same lines at the same size, only higher or lower), so that the free rows between the ink of band `balanceAbove` (an earlier band, as it was drawn in this language)
    // and this band's first line equal the free rows between its last line and row `balanceBelow` (the first row of the ink that follows it): a two-line heading grows upward from
    // its anchor and would read as belonging to the wrong block. A band of fewer lines is laid out as it always was, so a one-line heading is the picture it was; a move that
    // would leave the band's ink rows is not made.
    bool balanced = false;
    int balanceAbove = -1;
    double balanceBelow = 0.0;
    int balanceMinLines = 2;
};

struct Text {
    Layout layout = Layout::Word;
    std::string slot;             // a slot of the plan whose routing of script to face is used ("verdana", "matura")
    double px = 0.0;              // the height of the reference glyph at scale 1 (and ratio 1), in the picture's pixels
    std::vector<FaceTuning> faces;
    double anchorX = 0.0;
    double anchorY = 0.0;
    double minScale = 0.0;        // the floor: below it the recipe fails closed
    double step = 0.99;           // the scales tried are step^n, n = 0, 1, ...
    double inkThreshold = 0.04;   // the coverage that counts as ink when the ink's box is measured
    ShadowLook shadow;
    InnerLook inner;
    double maxWidth = 0.0;        // Word: the ink's width; Stack: the widest line's advance
    double maxHeight = 0.0;       // Word: the ink's height
    // Stack
    double firstBaseline = 0.0;   // the first line's baseline at scale 1 (the block keeps its middle when it shrinks)
    double pitch = 0.0;           // the baselines' distance at scale 1
    double gap = 0.0;             // the lines' ink is kept this many scaled px apart (a fraction of the measure)
    int maxLines = 1;
    Rect safe;                    // where the block's ink may lie
    // Paragraph
    double orphan = 0.0;          // with two lines or more, the narrowest is at least this fraction of the widest
    double saveLines = 0.0;       // scale: a layout with fewer lines within this of the finest feasible scale is taken
    // Where the new word's shadow may be written, where that is more than the erase's rectangle (the old lettering's halo was darker than the plate the model makes under it, and the
    // new halo is drawn over plate pixels that were never the old lettering's); empty: the erase's rectangle.
    bool hasPaint = false;
    Rect paint;
    int inkLevel = 255;           // the ink's grey level: 255 white, 0 black
    bool japaneseSpaces = false;  // an ASCII space in Japanese is a gap that is drawn, as in every other language (the credits' " - " has real spaces)
    // Bands
    double left = 0.0;            // the lines' ink starts here (picture pixels)
    double maxRight = 0.0;        // and ends no further right than this
    std::string digitFace;        // ASCII digits are drawn from this face (a face id of the plan), where it is not empty
    std::vector<Band> bands;
};

struct Recipe {
    std::string id;               // "tap_start": the stem of the baked file's name
    std::string key;              // the string table's key whose phrase is drawn (empty for a Bands recipe, whose bands each name theirs)
    std::string sprite;           // the picture's name as the layer asks for it: "main_play_game_button.png"
    int width = 0;
    int height = 0;
    std::string digest;           // of the sprite's pixels as decoded, so another version of the sprite is refused
    Erase erase;
    Text text;

    // Every string-table key this picture draws, in the order its phrases are asked for: the key, or each band's.
    std::vector<std::string> Keys() const {
        std::vector<std::string> keys;
        if (!key.empty()) keys.push_back(key);
        for (const Band& band : text.bands) keys.push_back(band.key);
        return keys;
    }
};

struct Recipes {
    std::vector<Recipe> items;
    std::string fileHash;         // of the table's bytes: part of every baked file's name, so a changed table never finds an old picture
    bool loaded = false;

    // The recipe for a sprite named as the layer names it, or null.
    const Recipe* Find(std::string_view sprite) const;
};

// picture_recipes.json, STRICTLY, like keys.json: every key present and of its type, sizes above zero, rectangles the right way round and inside the picture, a
// rule and a layout it knows, a unique id, sprite and key per recipe, a digest of 32 hex characters. A note is a member that starts with '_'. `out` is left as it was
// and `error` names the file and the member when anything is wrong.
bool LoadRecipes(const std::string& path, Recipes& out, std::string& error);

// ---- the bake ----------------------------------------------------------------------------------------------------------------------------

struct Request {
    const Recipe* recipe = nullptr;
    const FontBake::Plan* plan = nullptr;                    // languages.json's `fonts`: the faces and the routing
    const std::vector<FontBake::FaceBytes>* faces = nullptr; // FontBake::LoadFaces
    Strings::Lang lang = Strings::kDefaultLang;
    std::string_view phrase;                                 // as the table holds it: UTF-8, logical order
    std::vector<std::string> phrases;                        // a Bands recipe: one phrase per band, in the recipe's order
};

// One glyph as it was put down, in visual order (the order the shaper left, left to right along its line).
struct Placed {
    std::uint32_t codePoint = 0;
    int line = 0;
    double penX = 0.0;            // the pen's x, in the picture's pixels
    double baseline = 0.0;
};

// What Outcome::lineGap says when there are not two lines (or no column holds ink of both).
inline constexpr double kNoLineGap = 1e9;

// What one band of a Bands recipe came to.
struct BandResult {
    std::string key;
    double scale = 0.0;
    int lines = 0;
    std::vector<std::string> lineTexts;
    Rect ink;                     // the ink's box, in the picture's pixels
    double lineGap = kNoLineGap;  // Outcome::lineGap for this band's lines (kNoLineGap for a band of one line)
};

struct Outcome {
    bool ok = false;
    std::string reason;           // one line naming what refused, when !ok
    Image image;                  // the finished picture, the recipe's size
    double scale = 0.0;           // the fit scale reached (1.0: nothing shrunk)
    int lines = 0;
    std::vector<std::string> lineTexts; // each line as it was set, logical order, spaces that only break removed
    Rect ink;                     // the ink's box where the coverage reaches the recipe's threshold, in the picture's pixels
    // Two lines: the least free distance, in the picture's pixels, between the upper line's lowest solid ink and the lower line's highest in a column both reach,
    // measured on what was put down (negative: they touch or overlap); kNoLineGap otherwise. The pitch's promise (the lines stay the recipe's `gap` apart), as a
    // number that a test can hold it to.
    double lineGap = kNoLineGap;
    Rect touched;                 // the rectangle every changed pixel is inside
    std::vector<Placed> placed;
    std::vector<BandResult> bands; // a Bands recipe: each band's result (scale is the least of them)
};

// Whether the picture is the one the recipe was measured on: its size and its digest. `reason` says how not.
bool CheckSource(const Recipe& recipe, const Image& source, std::string& reason);

// Checks the source (and, for a twin, the twin), erases, fits and draws. `source` is the decoded sprite; `twin` the decoded twin sprite for a Twin recipe (else
// null). Nothing is written anywhere.
Outcome Render(const Request& request, const Image& source, const Image* twin);

// The fit alone: the same layout Render draws, with no picture and none of the picture's checks. ok, the scale reached, the lines, the ink's box and the glyphs
// as placed (no image); or why the phrase cannot be drawn (below the floor, a code point no face holds). What tests/test_mp_overflow.cpp asks of every phrase.
Outcome FitPhrase(const Request& request);

// The erase alone: `out` is `source` with the recipe's rectangle replaced by the model. False, with `reason`, when a guard of the recipe refuses the picture.
// For a Generated recipe `out` is a transparent picture of the recipe's size.
bool EraseWords(const Recipe& recipe, const Image& source, const Image* twin, Image& out, std::string& reason);

// ---- files -------------------------------------------------------------------------------------------------------------------------------

struct Wanted {
    const Recipe* recipe = nullptr;
    const Recipes* recipes = nullptr; // whose fileHash names the file
    const FontBake::Plan* plan = nullptr;
    std::string fontsDir;             // where the plan's face files are
    Strings::Lang lang = Strings::kDefaultLang;
    std::string languageCode;         // "de": in the file's name
    std::string phrase;
    std::vector<std::string> phrases; // a Bands recipe: one phrase per band, in the recipe's order
    std::string sourcePath;           // the recipe's sprite, as the layer found it
    std::string twinPath;             // the twin sprite, for a Twin recipe
    std::string directory;            // the scratch folder: made if it is not there
};

struct Written {
    bool ok = false;
    bool cached = false;              // the file was already there, whole: nothing was rendered
    std::string path;                 // generic separators
    std::string reason;
    double scale = 0.0;
    int lines = 0;
    double milliseconds = 0.0;
};

// The name a bake is written under: <id>_<code>_<16 hex>.png, the hash over the algorithm, the table's bytes, the phrase, the language and the bytes of the faces.
std::string BakedName(const Recipe& recipe, std::string_view languageCode, std::string_view hash);

// The picture, in `directory`: found when a file of this name is there and looks whole (the PNG signature, the recipe's size in its header, its last chunk),
// otherwise made and written whole to a name of its own and renamed over the target. The source is decoded and checked every time, found or not. Fails closed (ok
// false, `reason`, nothing left behind) on anything: a missing face, a check, a fit below the floor, a folder that cannot be made or written.
Written Ensure(const Wanted& wanted);

} // namespace MagicPortals::PictureBake
