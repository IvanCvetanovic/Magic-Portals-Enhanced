#pragma once

// UTF-8 text, decoded and encoded by the port itself (planning step 236, PORT-32).
//
// WHY THIS EXISTS. The UI languages (sim/Strings.hpp) are UTF-8 in JSON, and the engine's BitmapFont reads text one BYTE to a glyph
// (BitmapFont::Measure and BuildText; Hud.hpp says so loudly). The engine is not patched: the port decodes here, and Hud::LayOutText,
// Hud::MeasureText and the display step of Strings look glyphs up by code point. The text of the original game is English, whose
// bytes are the same ids, so an ASCII string decodes to itself and every existing layout is bit for bit what it was.
//
// WHAT A BAD BYTE BECOMES. U+FFFD, by the Unicode standard's "maximal subpart" rule (the Unicode Standard 15, section 3.9, "U+FFFD
// Substitution of Maximal Subparts"; the same answer a WHATWG decoder gives): one replacement for each byte that cannot start a
// sequence (0x80-0xC1 and 0xF5-0xFF), and one for each truncated or interrupted sequence, which takes the valid bytes it had and not
// the byte that interrupted it, so that byte is looked at again as the start of what follows. Concretely: C0 80 is two (C0 and 80 are
// each a byte that starts nothing), ED A0 80 is three (a surrogate: ED may only be followed by 80-9F, so ED stands alone, and A0 and
// 80 are stray), F4 90 80 80 is four (above U+10FFFF), E2 82 at the end of the text is one, and E2 82 followed by 'A' is a
// replacement and then 'A'. An overlong form is two or more, by the same rule. Nothing here throws, and nothing reads past the end.
//
// Free of the engine, like the rest of sim/. No non-ASCII character is written in this file or its source: MSVC is built without
// /utf-8 here, so every non-ASCII text lives in JSON data and in tests as escapes.

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace MagicPortals::Utf8 {

inline constexpr std::uint32_t kReplacement = 0xFFFD;
inline constexpr std::uint32_t kLastCodePoint = 0x10FFFF;

// The code points of `text`, with a bad byte (see above) as kReplacement. '\n' and every ASCII byte are themselves.
std::vector<std::uint32_t> Decode(std::string_view text);

// The same, and `replaced` is how many replacements the decoder MADE (a literal U+FFFD in the text is not counted): zero for text that is
// valid UTF-8.
std::vector<std::uint32_t> Decode(std::string_view text, std::size_t& replaced);

// True when `text` is valid UTF-8 (the decoder made no replacement). A literal U+FFFD in valid text is valid.
bool IsValid(std::string_view text);

// The UTF-8 bytes of `codePoint`; a surrogate (U+D800-DFFF) or a value above U+10FFFF is kReplacement's bytes (EF BF BD).
std::string Encode(std::uint32_t codePoint);

// And of a run of code points.
std::string Encode(const std::vector<std::uint32_t>& codePoints);

} // namespace MagicPortals::Utf8
