#pragma once

// Fonts made at run time for the languages the original's bitmaps cannot spell (planning step 237, PORT-33).
//
// WHY. The original draws its words in Matura MT Script Capitals (Matura84_shadow) and its achievements in Verdana (Verdana40_shadow),
// as bitmap fonts that hold Latin-1 and nothing else. Russian, Ukrainian, Turkish, Japanese and Arabic need glyphs they do not have, and
// those two fonts are Asantee's and Microsoft's: nothing is extended, copied or derived from them. So a language outside Latin-1 draws
// EVERY font slot from an atlas this module bakes from the open-licence faces in game/data/fonts (Kurale for Matura's slot, Noto Sans Bold
// for Verdana's, Noto Sans JP and Noto Sans Arabic for their scripts), in the original's own BMFont shape - white glyph, a baked black soft
// shadow, the same line height and baseline, the same cell padding - so the engine's BitmapFont, the port's Hud layout and the layer's
// scale arithmetic read it as they read the original.
//
// WHAT IT IS NOT. It does not choose a language (Strings does), shape Arabic (ArabicShaping, run by Strings::Display before any layout),
// load fonts into the renderer (the layer does, MagicPortalsLayer::fontFile and bakeLanguageFonts) or name a generated file anywhere in a table: tools/package/closure.py would
// resolve a .fnt or .png name as an original asset. The tables name a .ttf and a face id only.
//
// WHAT IS DATA. Everything below that is a number or a choice - which face draws which script, each slot's line height, baseline, cap
// height, padding, shadow - is game/data/lang/languages.json's `fonts` block (Strings loads it strictly into a Plan); the measured
// numbers carry the measurement in their `_source` notes. This file holds mechanism only.
//
// THE BAKE. Per code point: the face its script routes to (a code point no script names goes to the slot's text face), its coverage
// rasterised with stb_truetype (the engine vendors it under imgui; nothing new is a dependency) at the scale that makes the face's
// REFERENCE glyph (a capital H, or the Arabic alef, or a kanji) as tall as the slot's cap height times the face's ratio; placed on the
// slot's baseline; the soft shadow convolved from the same coverage (a Gaussian, offset and blurred as the slot says); composited as
// white over black into straight-alpha RGBA; packed in shelves on pages no larger than the slot's page size. xadvance is the face's
// advance FLOORED to a whole pixel, so the joined letters of Arabic overlap rather than gap (BMFont's xadvance is an integer, and the
// engine draws at the glyph's integer positions). No kerning: the original applies none.
//
// DETERMINISTIC. The same inputs give the same bytes: glyphs are packed in a fixed order and the PNG encoder is stb's, whose output is a
// function of the pixels. A cached bake is found by an input hash written into the .fnt (a `bake` record the engine's reader skips), and
// Ensure writes nothing when the files on disk already hold that hash - the precedent is the layer's generated pictures
// (MagicPortalsLayer's WriteWhole), whose files are replaced atomically and are only ever written when they differ.
//
// Free of the engine's renderer, like the rest of sim/.

#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace MagicPortals::FontBake {

// Where the baked fonts are written, inside the layer's scratch folder (a sibling of the committed faces' folder, which is the plan's `folder`),
// and the suffix of the picker's atlas (`<original font>_picker`): the layer, which writes them, and the suites, which look for them, say them
// once, here.
inline constexpr const char* kScratchFolder = "fonts";
inline constexpr const char* kPickerSuffix = "_picker";

// ---- the plan: languages.json `fonts`, as data ----------------------------------------------------------------------------------

struct CodeRange {
    std::uint32_t first = 0;
    std::uint32_t last = 0; // inclusive
};

// A .ttf in the fonts folder, and the glyph that says how big it is: the face is scaled so that `reference`'s top (above the baseline)
// is `referenceHeight` times the slot's cap height. A Latin face's reference is its capital H (ratio 1: the cap heights match); the
// Arabic face's is the alef; the Japanese face's a kanji, whose ratio is a typographic choice, not a measurement.
struct FaceSpec {
    std::string id;   // "kurale"
    std::string file; // "Kurale-Regular.ttf", in the plan's folder
    std::uint32_t reference = 'H';
    double referenceHeight = 1.0;
};

// A script is a set of code point ranges that routes to a slot's own face for it (the slot's scriptFaces); a code point in none is the
// slot's text face's.
struct ScriptSpec {
    std::string id; // "japanese"
    std::vector<CodeRange> ranges;
};

// The baked soft shadow, in the slot's pixels: the glyph's coverage offset right and down by (dx, dy) and blurred by a Gaussian of
// `sigma`, at `alpha` of full black, under the glyph.
struct Shadow {
    double dx = 0.0;
    double dy = 0.0;
    double sigma = 0.0;
    double alpha = 0.0;
};

// One of the original's font slots, as the generated font must reproduce it.
struct SlotSpec {
    std::string id;           // "matura"
    std::string originalFont; // "Matura84_shadow": the name the layer asks for, which a baked font replaces
    int lineHeight = 0;       // the original's, so units_per_font_px and every placement stay as they are
    int base = 0;             // the baseline's distance from the top of a line
    double capHeight = 0.0;   // px a capital H stands above the baseline in the original
    int padUp = 0;            // BMFont padding: the transparent border each glyph's cell carries, which holds the shadow
    int padRight = 0;
    int padDown = 0;
    int padLeft = 0;
    int spacingX = 0; // texels between two cells on a page
    int spacingY = 0;
    int pageSize = 512; // a page is pageSize x pageSize
    Shadow shadow;
    double embolden = 0.0; // 0..1: how much of a pixel the coverage is thickened by (a light face standing in for a heavy one)
    std::string textFace;  // the face id of everything no script routes elsewhere
    std::vector<std::pair<std::string, std::string>> scriptFaces; // script id -> face id
};

struct Plan {
    std::string folder; // below the port's data directory: "fonts"
    std::vector<FaceSpec> faces;
    std::vector<ScriptSpec> scripts;
    std::vector<SlotSpec> slots;
    std::string pickerSlot; // the slot the language picker's names are baked in
    bool loaded = false;

    const FaceSpec* Face(std::string_view id) const;
    const SlotSpec* Slot(std::string_view id) const;
};

// "U+0048" -> 0x48 (four to six hex digits), and "3000-30FF" -> a range. False for anything else.
bool ParseCodePoint(std::string_view text, std::uint32_t& out);
bool ParseRange(std::string_view text, CodeRange& out);

// The face id a code point is drawn from in `slot`: the first script whose ranges hold it and which the slot routes, else the text face.
const std::string& FaceIdOf(const Plan& plan, const SlotSpec& slot, std::uint32_t codePoint);

// Digits, ASCII punctuation, the ASCII letters, the space and the no-break space: what every slot holds whatever the strings say, since text is composed
// with numbers and with the separators around them, a table falls back to English for a key it lacks, and the credits and the brand are English.
std::set<std::uint32_t> BaseSet();

// ---- the bake -----------------------------------------------------------------------------------------------------------------------

struct FaceBytes {
    std::string id;
    std::vector<unsigned char> data;
};

// Every face of the plan read from <dir>/<file>; false with `error` naming the file that could not be read.
bool LoadFaces(const Plan& plan, const std::string& dir, std::vector<FaceBytes>& out, std::string& error);

struct Baked {
    std::string fnt;                // the BMFont text descriptor
    std::vector<std::string> pages; // PNG bytes, one per page
    std::vector<std::uint32_t> missing; // code points no face had a glyph for, ascending: not in the font
    std::size_t glyphs = 0;         // `char` lines, the space's included
    std::string hash;               // of the inputs; also in the .fnt
};

// The hash that names a bake's inputs: the plan's slot, the bytes of every face the slot uses, the code points (BaseSet added) and the
// name. Cheap; Ensure computes it to find out whether it must bake.
std::string InputHash(const Plan& plan, const SlotSpec& slot, const std::vector<FaceBytes>& faces, const std::set<std::uint32_t>& codePoints,
                      std::string_view name);

// A font for `slot` over `codePoints` (BaseSet is added): `name` is the font's face name and the stem of its page files (<name>_<page>.png).
// False, with `error`, when a face of the slot's routing is not in `faces`, a face cannot be read as a TrueType font, a reference glyph is
// absent, or a glyph's cell does not fit a page. A code point that no face holds is NOT an error: it is in Baked::missing, and the font
// simply has no glyph for it (the layout draws nothing there and says so).
bool Bake(const Plan& plan, const SlotSpec& slot, const std::vector<FaceBytes>& faces, const std::set<std::uint32_t>& codePoints, std::string_view name,
          Baked& out, std::string& error);

struct Written {
    bool ok = false;
    bool cached = false;     // the files already held this bake's hash: nothing was baked and nothing written
    int filesWritten = 0;
    std::string fntPath;     // <dir>/<name>.fnt, with '/' separators
    std::vector<std::uint32_t> missing;
    std::string error;
};

// The font in `dir` (which must exist), as <name>.fnt and <name>_<page>.png. When <name>.fnt is there with this input hash and every page it
// names is a file that begins with the PNG signature (an empty or cut-short page is not found), nothing is baked and nothing is written. Otherwise
// it is baked and each file is written whole to a name of its own and renamed over its target, so a window reading the folder never sees half a
// file, pages first and the descriptor last (a descriptor is only ever on disk with its pages).
Written Ensure(const Plan& plan, const SlotSpec& slot, const std::vector<FaceBytes>& faces, const std::set<std::uint32_t>& codePoints,
               const std::string& dir, std::string_view name);

// The code points the descriptor `fnt` (text) names with a `char` line, and the `bake` record's hash and missing list; false when it has no
// `common` line. For the cache check and for tests.
struct Described {
    std::set<std::uint32_t> chars;
    std::string hash;
    std::vector<std::uint32_t> missing;
    std::vector<std::string> pageFiles;
};
bool Describe(std::string_view fnt, Described& out);

} // namespace MagicPortals::FontBake
