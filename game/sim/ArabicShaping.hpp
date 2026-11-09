#pragma once

// Arabic for a bitmap font that knows neither shaping nor direction (planning step 237, PORT-33). Adapted from Penumbra Enhanced's
// game/render/ArabicShaping (ENHANCEMENT E24; Ivan's own code, MIT-0): the engine's BitmapFont, and the port's Hud layout over it, put one
// code point's glyph after another from left to right, and a hundred-odd UI strings need only this.
//
// SHAPING. Each Arabic letter takes its isolated, final, initial or medial form from its neighbours' joining types (Unicode's
// ArabicShaping.txt: U non-joining, R right-joining, D dual-joining, C join-causing - the tatweel - and T transparent - the harakat, skipped
// when looking for a neighbour), as the Arabic Presentation Forms-B code points, which the bundled Noto Sans Arabic maps to the joined
// glyphs. Lam followed by alef (plain, madda, hamza above or below) becomes the one lam-alef ligature, isolated or final. The translations
// carry no harakat: nothing positions a mark over its letter.
//
// DIRECTION, one line at a time (a simplified Unicode Bidirectional Algorithm, UAX #9, at a single paragraph level): the letters' own
// direction, digits after Arabic kept as a number, a separator between two digits ("12:05") part of the number, neutrals between two runs
// of one direction taking it and others the paragraph's, a bracket pair taking the direction of what it encloses in its context, then the
// runs reordered - Latin words, proper nouns and numbers read left to right inside a right-to-left line - and ( ) [ ] { } < > << >> mirrored
// where they read right to left. What comes out is what the layout puts from left to right.
//
// ONE DEVIATION FROM UAX #9, made for this game and pinned by tests/test_mp_arabic.cpp: a hyphen between two numbers that follow Arabic letters
// joins them (W4), so the level "1-2" is drawn "1-2" and not "2-1" - what a player looking for that level expects. A hyphen anywhere else is the standard's.
//
// A line with no letter in it - a number, a time, "[<]" - is drawn as it is. Right alignment is the caller's job; centred text, which most of
// this game uses, needs nothing. Not done, and said so rather than hidden: marks (harakat) are not positioned; the algorithm is a single
// paragraph level with no explicit embeddings; lam-alef is the only ligature.
//
// APPLY IT ONCE. The output is visual order in presentation forms: shaping or reordering it again would reverse it. Strings::Display is the
// one place it is called.

#include <string>
#include <string_view>

namespace MagicPortals::ArabicShaping {

// A code point of the Arabic blocks or presentation forms (strong right to left).
bool IsArabic(char32_t codePoint);
// Joined forms and lam-alef ligatures, in logical order.
std::u32string Shape(const std::u32string& logical);
// One line, logical order in, the order to draw it from left to right out: shaped, resolved, reordered and mirrored. No line breaks in it.
std::u32string VisualLine(const std::u32string& logical, bool rightToLeft);
// Every line of a text, each alone; the breaks are kept where they are.
std::u32string Visual(const std::u32string& logical, bool rightToLeft);

// The same over UTF-8 (decoded by sim/Utf8). A text that is left to right and holds no Arabic letter comes back as its own bytes.
std::string VisualUtf8(std::string_view utf8, bool rightToLeft);

// The conversions the above use, for tests and callers that hold the other form.
std::u32string FromUtf8(std::string_view utf8);
std::string ToUtf8(const std::u32string& text);

} // namespace MagicPortals::ArabicShaping
