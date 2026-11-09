// UTF-8 text (planning step 236, PORT-32): sim/Utf8's decoder and encoder.
//
// The interface languages are UTF-8 in JSON and the engine's BitmapFont reads one byte to a glyph, so the port decodes (Hud.hpp says how it
// is laid out). What is pinned here is the decoder alone, with the exact output for each class of malformed input - not "contains U+FFFD",
// which an implementation that replaced a whole run with one would also satisfy:
//
//   ascii        every byte below 0x80 is itself, '\n' and NUL included, and a text of ASCII decodes to its own bytes
//   well formed  two, three and four byte forms, each end of each range (U+0080, U+07FF, U+0800, U+FFFF, U+10000, U+10FFFF), and the
//                Unicode standard's "last before a surrogate" and "first after" (U+D7FF, U+E000)
//   malformed    a stray continuation byte, C0/C1, F5-FF, overlong forms (C0 80, E0 80 80, E0 9F BF, F0 80 80 80), surrogates (ED A0 80),
//                above U+10FFFF (F4 90 80 80), a sequence cut off by the end of the text, and one interrupted by an ASCII byte or by the
//                start of the next sequence - by the Unicode maximal-subpart rule (Utf8.hpp): the byte that interrupts is looked at again
//   encode       the same forms, and a surrogate or a value above U+10FFFF as EF BF BD
//   hud          Hud::LayOutText, LayOutTextFrom and MeasureText over a font this suite writes (planning step 236): for ASCII they are BIT FOR BIT
//                what the byte loops gave (a copy of the old loop is the oracle, 6,000 random strings with newlines and glyphs the font lacks, every
//                field compared with ==, and MeasureText with BitmapFont::Measure); a two-byte, three-byte and four-byte character is ONE glyph
//                looked up by code point and not two or three Latin-1 ones; a code point the font lacks draws nothing, does not advance the
//                pen, and is reported through the optional out-parameter in the order met; a malformed byte is reported as U+FFFD
//   round trips  every code point of every class but the surrogates, one at a time; and a seeded fuzz of 200,000 random byte strings
//                where Decode(Encode(Decode(s))) == Decode(s), valid text comes back byte for byte, and no replacement is made without a
//                U+FFFD in the output
//
// No non-ASCII character is written in this file: MSVC is built without /utf-8 here, so bytes are built from numbers.

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "core/BitmapFont.hpp"
#include "sim/Hud.hpp"
#include "sim/Utf8.hpp"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

using namespace MagicPortals;

namespace {

using CodePoints = std::vector<std::uint32_t>;

// A string of exactly these bytes.
std::string B(std::initializer_list<int> bytes) {
    std::string out;
    for (const int byte : bytes) out.push_back(static_cast<char>(static_cast<unsigned char>(byte)));
    return out;
}

constexpr std::uint32_t kBad = Utf8::kReplacement;

std::string Show(const CodePoints& points) {
    std::string out = "[";
    for (const std::uint32_t point : points) {
        char buffer[16];
        std::snprintf(buffer, sizeof(buffer), " %04X", static_cast<unsigned>(point));
        out += buffer;
    }
    return out + " ]";
}

std::string ShowBytes(const std::string& bytes) {
    std::string out;
    for (const char c : bytes) {
        char buffer[8];
        std::snprintf(buffer, sizeof(buffer), "%02X ", static_cast<unsigned>(static_cast<unsigned char>(c)));
        out += buffer;
    }
    return out;
}

// `bytes` decodes to exactly `want`, with `replaced` replacements.
void Decodes(const std::string& bytes, const CodePoints& want, std::size_t replaced) {
    std::size_t made = 99;
    const CodePoints got = Utf8::Decode(bytes, made);
    CHECK_MSG(got == want, "decoding " + ShowBytes(bytes) + "gave " + Show(got) + ", wanted " + Show(want));
    CHECK_MSG(made == replaced, "decoding " + ShowBytes(bytes) + "replaced " + std::to_string(made) + ", wanted " + std::to_string(replaced));
    CHECK_MSG(Utf8::Decode(bytes) == want, "the overload without a count agrees on " + ShowBytes(bytes));
    CHECK_MSG(Utf8::IsValid(bytes) == (replaced == 0), "IsValid on " + ShowBytes(bytes));
}

void AsciiIsItself() {
    std::string all;
    CodePoints want;
    for (int c = 0; c < 0x80; ++c) {
        all.push_back(static_cast<char>(c));
        want.push_back(static_cast<std::uint32_t>(c));
    }
    Decodes(all, want, 0);
    Decodes("", {}, 0);
    Decodes("Part 17", {'P', 'a', 'r', 't', ' ', '1', '7'}, 0);
    // A newline is a newline: the layout breaks lines on it.
    Decodes("a\nb\n\n", {'a', '\n', 'b', '\n', '\n'}, 0);
    Decodes(std::string("a\0b", 3), {'a', 0, 'b'}, 0);
}

void WellFormedSequences() {
    Decodes(B({0xC2, 0x80}), {0x80}, 0);
    Decodes(B({0xC3, 0xA9}), {0xE9}, 0);
    Decodes(B({0xDF, 0xBF}), {0x7FF}, 0);
    Decodes(B({0xE0, 0xA0, 0x80}), {0x800}, 0);
    Decodes(B({0xE2, 0x82, 0xAC}), {0x20AC}, 0);
    Decodes(B({0xED, 0x9F, 0xBF}), {0xD7FF}, 0);
    Decodes(B({0xEE, 0x80, 0x80}), {0xE000}, 0);
    Decodes(B({0xEF, 0xBF, 0xBD}), {0xFFFD}, 0); // a literal U+FFFD is valid text
    Decodes(B({0xEF, 0xBF, 0xBF}), {0xFFFF}, 0);
    Decodes(B({0xF0, 0x90, 0x80, 0x80}), {0x10000}, 0);
    Decodes(B({0xF0, 0x9F, 0x98, 0x80}), {0x1F600}, 0);
    Decodes(B({0xF4, 0x8F, 0xBF, 0xBF}), {0x10FFFF}, 0);
    // The languages' own letters: a (Cyrillic) Zhe, a (Japanese) hiragana A, an (Arabic) alef, and a (Turkish) dotless i and g-breve.
    Decodes(B({0xD0, 0x96, 0xE3, 0x81, 0x82, 0xD8, 0xA7, 0xC4, 0xB1, 0xC4, 0x9F}), {0x416, 0x3042, 0x627, 0x131, 0x11F}, 0);
    // Mixed with ASCII on both sides, and a newline inside.
    Decodes("a" + B({0xC3, 0xA9}) + "\n" + B({0xE2, 0x82, 0xAC}) + "z", {'a', 0xE9, '\n', 0x20AC, 'z'}, 0);
}

void StrayAndForbiddenBytes() {
    Decodes(B({0x80}), {kBad}, 1);
    Decodes(B({0xBF}), {kBad}, 1);
    Decodes(B({0x80, 0x80, 0x80}), {kBad, kBad, kBad}, 3);
    Decodes(B({0xC0}), {kBad}, 1);
    Decodes(B({0xC1}), {kBad}, 1);
    Decodes(B({0xF5}), {kBad}, 1);
    Decodes(B({0xFE}), {kBad}, 1);
    Decodes(B({0xFF}), {kBad}, 1);
    Decodes("a" + B({0xFF}) + "b", {'a', kBad, 'b'}, 1);
    // A Latin-1 byte on its own, which is what a text in the old byte-per-glyph encoding looks like: the degree sign, and an e with an acute.
    Decodes(B({0xB0}), {kBad}, 1);
    Decodes("1080" + B({0xBA}) + " spin", {'1', '0', '8', '0', kBad, ' ', 's', 'p', 'i', 'n'}, 1);
    Decodes("caf" + B({0xE9}), {'c', 'a', 'f', kBad}, 1);
}

void OverlongSurrogateAndTooLarge() {
    // Overlong: C0 and C1 start nothing, so each byte is its own replacement.
    Decodes(B({0xC0, 0x80}), {kBad, kBad}, 2);
    Decodes(B({0xC1, 0xBF}), {kBad, kBad}, 2);
    // E0 may only be followed by A0-BF: the lead stands alone and every following continuation byte is stray.
    Decodes(B({0xE0, 0x80, 0x80}), {kBad, kBad, kBad}, 3);
    Decodes(B({0xE0, 0x9F, 0xBF}), {kBad, kBad, kBad}, 3);
    Decodes(B({0xF0, 0x80, 0x80, 0x80}), {kBad, kBad, kBad, kBad}, 4);
    Decodes(B({0xF0, 0x8F, 0xBF, 0xBF}), {kBad, kBad, kBad, kBad}, 4);
    // A surrogate: ED may only be followed by 80-9F.
    Decodes(B({0xED, 0xA0, 0x80}), {kBad, kBad, kBad}, 3);
    Decodes(B({0xED, 0xBF, 0xBF}), {kBad, kBad, kBad}, 3);
    // And above U+10FFFF: F4 may only be followed by 80-8F, and F5-FF start nothing.
    Decodes(B({0xF4, 0x90, 0x80, 0x80}), {kBad, kBad, kBad, kBad}, 4);
    Decodes(B({0xF5, 0x80, 0x80, 0x80}), {kBad, kBad, kBad, kBad}, 4);
    Decodes(B({0xF7, 0xBF, 0xBF, 0xBF}), {kBad, kBad, kBad, kBad}, 4);
    // A CESU-8 pair of surrogates, which a careless decoder joins into U+10000: six replacements.
    Decodes(B({0xED, 0xA0, 0x80, 0xED, 0xB0, 0x80}), {kBad, kBad, kBad, kBad, kBad, kBad}, 6);
}

void TruncatedAndInterruptedSequences() {
    // Cut off by the end of the text: ONE replacement for the bytes it had.
    Decodes(B({0xC3}), {kBad}, 1);
    Decodes(B({0xE2}), {kBad}, 1);
    Decodes(B({0xE2, 0x82}), {kBad}, 1);
    Decodes(B({0xF0}), {kBad}, 1);
    Decodes(B({0xF0, 0x9F}), {kBad}, 1);
    Decodes(B({0xF0, 0x9F, 0x98}), {kBad}, 1);
    Decodes("a" + B({0xC3}), {'a', kBad}, 1);
    Decodes("ab" + B({0xE2, 0x82}), {'a', 'b', kBad}, 1);
    // Interrupted by an ASCII byte: one replacement, and the ASCII byte is itself (it is looked at again).
    Decodes(B({0xE2, 0x82}) + "A", {kBad, 'A'}, 1);
    Decodes(B({0xC3}) + "\n", {kBad, '\n'}, 1);
    Decodes(B({0xF0, 0x9F}) + "A", {kBad, 'A'}, 1);
    // Interrupted by the start of another sequence: the next one decodes whole.
    Decodes(B({0xE2, 0xE2, 0x82, 0xAC}), {kBad, 0x20AC}, 1);
    Decodes(B({0xC3, 0xC3, 0xA9}), {kBad, 0xE9}, 1);
    Decodes(B({0xF0, 0x9F, 0xE2, 0x82, 0xAC}), {kBad, 0x20AC}, 1);
    // A lead followed by a stray lead that is not a lead: both are replaced, separately.
    Decodes(B({0xE2, 0xFF}), {kBad, kBad}, 2);
    // A valid sequence's second byte out of range, the first of a three-byte one.
    Decodes(B({0xE2, 0x41, 0x82}), {kBad, 'A', kBad}, 2);
}

void EncodeIsTheInverse() {
    CHECK_MSG(Utf8::Encode(0x41) == "A", "A");
    CHECK_MSG(Utf8::Encode('\n') == "\n", "newline");
    CHECK_MSG(Utf8::Encode(0) == std::string(1, '\0'), "NUL is one byte");
    CHECK_MSG(Utf8::Encode(0x7F) == B({0x7F}), "U+007F");
    CHECK_MSG(Utf8::Encode(0x80) == B({0xC2, 0x80}), "U+0080");
    CHECK_MSG(Utf8::Encode(0xE9) == B({0xC3, 0xA9}), "U+00E9");
    CHECK_MSG(Utf8::Encode(0x7FF) == B({0xDF, 0xBF}), "U+07FF");
    CHECK_MSG(Utf8::Encode(0x800) == B({0xE0, 0xA0, 0x80}), "U+0800");
    CHECK_MSG(Utf8::Encode(0x20AC) == B({0xE2, 0x82, 0xAC}), "U+20AC");
    CHECK_MSG(Utf8::Encode(0xD7FF) == B({0xED, 0x9F, 0xBF}), "U+D7FF");
    CHECK_MSG(Utf8::Encode(0xE000) == B({0xEE, 0x80, 0x80}), "U+E000");
    CHECK_MSG(Utf8::Encode(0xFFFF) == B({0xEF, 0xBF, 0xBF}), "U+FFFF");
    CHECK_MSG(Utf8::Encode(0x10000) == B({0xF0, 0x90, 0x80, 0x80}), "U+10000");
    CHECK_MSG(Utf8::Encode(0x1F600) == B({0xF0, 0x9F, 0x98, 0x80}), "U+1F600");
    CHECK_MSG(Utf8::Encode(0x10FFFF) == B({0xF4, 0x8F, 0xBF, 0xBF}), "U+10FFFF");

    // What cannot be encoded is the replacement character, never an invalid sequence.
    const std::string replacement = B({0xEF, 0xBF, 0xBD});
    CHECK_MSG(Utf8::Encode(0xD800) == replacement, "the first surrogate");
    CHECK_MSG(Utf8::Encode(0xDBFF) == replacement, "a high surrogate");
    CHECK_MSG(Utf8::Encode(0xDC00) == replacement, "a low surrogate");
    CHECK_MSG(Utf8::Encode(0xDFFF) == replacement, "the last surrogate");
    CHECK_MSG(Utf8::Encode(0x110000) == replacement, "one past the last code point");
    CHECK_MSG(Utf8::Encode(0xFFFFFFFFu) == replacement, "the largest value");
    CHECK_MSG(Utf8::IsValid(Utf8::Encode(0xD800)), "and what it wrote is valid");

    CHECK_MSG(Utf8::Encode(CodePoints{}) == "", "no code points, no bytes");
    CHECK_MSG(Utf8::Encode(CodePoints{'a', 0xE9, '\n', 0x20AC, 0x1F600}) == "a" + B({0xC3, 0xA9}) + "\n" + B({0xE2, 0x82, 0xAC}) + B({0xF0, 0x9F, 0x98, 0x80}),
              "a run of code points");
}

void EveryCodePointRoundTrips() {
    int tried = 0;
    bool allWhole = true;
    std::string firstBad;
    for (std::uint32_t cp = 0; cp <= Utf8::kLastCodePoint; ++cp) {
        if (cp >= 0xD800 && cp <= 0xDFFF) continue;
        const std::string bytes = Utf8::Encode(cp);
        std::size_t replaced = 0;
        const CodePoints back = Utf8::Decode(bytes, replaced);
        const std::size_t expectedLength = cp < 0x80 ? 1 : cp < 0x800 ? 2 : cp < 0x10000 ? 3 : 4;
        if (back.size() != 1 || back[0] != cp || replaced != 0 || bytes.size() != expectedLength) {
            if (allWhole) firstBad = Show({cp}) + " -> " + ShowBytes(bytes);
            allWhole = false;
        }
        ++tried;
    }
    CHECK_MSG(allWhole, "every code point encodes to the shortest form and decodes to itself; first failure " + firstBad);
    CHECK_EQ(tried, 0x110000 - 0x800);
}

// A small seeded generator (a 64-bit LCG), so the fuzz is the same on every platform.
struct Rng {
    std::uint64_t state;
    std::uint32_t next() {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        return static_cast<std::uint32_t>(state >> 33);
    }
};

void TheFuzzKeepsItsInvariants() {
    // Bytes that matter to the decoder, over-represented: the edges of every range in Table 3-7, ASCII, and anything.
    const int edges[] = {0x00, 0x0A, 0x41, 0x7F, 0x80, 0x8F, 0x90, 0x9F, 0xA0, 0xBF, 0xC0, 0xC1, 0xC2, 0xDF, 0xE0,
                         0xE1, 0xEC, 0xED, 0xEE, 0xEF, 0xF0, 0xF1, 0xF3, 0xF4, 0xF5, 0xFF};
    constexpr int kEdges = static_cast<int>(sizeof(edges) / sizeof(edges[0]));
    Rng rng{20261108};
    int failures = 0;
    int validTexts = 0;
    int brokenTexts = 0;
    std::string firstFailure;
    const auto note = [&](const std::string& why, const std::string& bytes) {
        if (failures++ == 0) firstFailure = why + ": " + ShowBytes(bytes);
    };
    for (int i = 0; i < 200000; ++i) {
        const int length = static_cast<int>(rng.next() % 13);
        std::string bytes;
        for (int k = 0; k < length; ++k) {
            const std::uint32_t pick = rng.next();
            const int byte = pick % 4 == 0 ? static_cast<int>((pick >> 8) & 0xFF) : edges[(pick >> 8) % kEdges];
            bytes.push_back(static_cast<char>(static_cast<unsigned char>(byte)));
        }
        std::size_t replaced = 0;
        const CodePoints decoded = Utf8::Decode(bytes, replaced);
        if (decoded.size() > bytes.size()) note("more code points than bytes", bytes);
        // Re-encoding what was decoded and decoding it again changes nothing, and is valid.
        const std::string again = Utf8::Encode(decoded);
        std::size_t replacedAgain = 0;
        if (Utf8::Decode(again, replacedAgain) != decoded || replacedAgain != 0) note("Decode(Encode(Decode(s))) != Decode(s)", bytes);
        // Every replacement made is a U+FFFD in the output (a literal U+FFFD in a valid sequence may add more, never fewer).
        std::size_t replacementsInOutput = 0;
        for (const std::uint32_t point : decoded) replacementsInOutput += point == kBad ? 1 : 0;
        if (replaced > replacementsInOutput) note("more replacements counted than U+FFFD produced", bytes);
        if (replaced == 0) {
            ++validTexts;
            if (Utf8::Encode(decoded) != bytes) note("valid text does not come back byte for byte", bytes);
        } else {
            ++brokenTexts;
            if (Utf8::Encode(decoded) == bytes) note("a text with a replacement came back unchanged", bytes);
        }
        if (Utf8::IsValid(bytes) != (replaced == 0)) note("IsValid disagrees with the count", bytes);
    }
    CHECK_MSG(failures == 0, "the fuzz's invariants hold; first failure: " + firstFailure);
    // The generator reaches both sides, so a decoder that rejected everything or nothing would not pass.
    CHECK_MSG(validTexts > 20000, "the fuzz made valid texts: " + std::to_string(validTexts));
    CHECK_MSG(brokenTexts > 20000, "the fuzz made broken texts: " + std::to_string(brokenTexts));
}

void NothingIsReadPastTheEnd() {
    // The text is a view of a buffer that continues with a continuation byte right after the end: a decoder that looked one byte too far
    // would take it as the missing tail of the cut-off sequence.
    const std::string buffer = B({0xE2, 0x82, 0xAC, 0xE2, 0x82, 0xAC});
    const std::string_view cut(buffer.data(), 2);
    CHECK_MSG(Utf8::Decode(cut) == (CodePoints{kBad}), "two bytes of a three-byte sequence, with its third byte in memory just after");
    const std::string_view four(buffer.data(), 4);
    CHECK_MSG(Utf8::Decode(four) == (CodePoints{0x20AC, kBad}), "a whole one and a cut one, with the rest in memory just after");
    CHECK_MSG(Utf8::Decode(std::string_view{}).empty(), "an empty view");
    CHECK_MSG(Utf8::Decode(std::string_view(buffer.data(), 0)).empty(), "a view of nothing");
}

// ---- Hud: layout and measure by code point --------------------------------------------------------------------------------------------

// A BMFont descriptor this suite writes (the engine's own suites do the same: the game's fonts are Asantee's and are not in the repository).
// ASCII 32-126 except '#', '@' and '~' (so a missing glyph is in reach of plain text), with numbers that differ from glyph to glyph, on two pages;
// and four glyphs beyond Latin-1's byte range of a lookup: U+00E9 (which a byte loop would find at id 233 - but only for the single byte E9),
// U+0416, U+3042 and U+0627.
bool InFont(std::uint32_t id) {
    if (id == '#' || id == '@' || id == '~') return false;
    return (id >= 32 && id <= 126) || id == 0xE9 || id == 0x416 || id == 0x3042 || id == 0x627;
}

std::string FontText() {
    std::string text =
        "info face=\"Test\" size=16\n"
        "common lineHeight=20 base=15 scaleW=256 scaleH=128 pages=2\n"
        "page id=0 file=\"p0.png\"\n"
        "page id=1 file=\"p1.png\"\n";
    for (std::uint32_t id = 0; id < 0x4000; ++id) {
        if (!InFont(id)) continue;
        const std::uint32_t k = id % 0x100;
        char line[256];
        std::snprintf(line, sizeof(line), "char id=%u x=%u y=%u width=%u height=%u xoffset=%d yoffset=%d xadvance=%u page=%u\n", static_cast<unsigned>(id),
                      (k % 16) * 16, (k / 16) * 8, 5 + k % 7, 8 + k % 5, static_cast<int>(k % 3) - 1, static_cast<int>(k % 4), 7 + k % 6, k % 2);
        text += line;
    }
    return text;
}

const Supersonic::BitmapFont& TestFont() {
    static Supersonic::BitmapFont font;
    static bool loaded = false;
    if (!loaded) {
        const std::filesystem::path dir = std::filesystem::temp_directory_path() / "supersonic-test-mp-utf8";
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        const std::filesystem::path file = dir / "test.fnt";
        {
            std::ofstream out(file, std::ios::binary | std::ios::trunc);
            out << FontText();
        }
        std::string error;
        CHECK_MSG(font.Load(file.string(), error), "the test font loads: " + error);
        loaded = true;
    }
    return font;
}

// The loop Hud::LayOutTextFrom had before step 236: one BYTE a glyph. The oracle for ASCII.
std::vector<Hud::Glyph> ByteLoopLayOutFrom(const Supersonic::BitmapFont& font, const std::string& text, const glm::dvec2& topLeft, double scale) {
    std::vector<Hud::Glyph> glyphs;
    const glm::dvec2 page(font.PageSize());
    double pen = 0.0;
    double lineTop = 0.0;
    for (const char c : text) {
        if (c == '\n') {
            pen = 0.0;
            lineTop += static_cast<double>(font.LineHeight());
            continue;
        }
        const Supersonic::FontGlyph* found = font.Find(static_cast<std::uint32_t>(static_cast<unsigned char>(c)));
        if (found == nullptr) continue;
        if (found->width > 0 && found->height > 0) {
            Hud::Glyph glyph;
            glyph.rect.min = topLeft + glm::dvec2(pen + found->xoffset, lineTop + found->yoffset) * scale;
            glyph.rect.size = glm::dvec2(found->width, found->height) * scale;
            glyph.page = found->page;
            glyph.uvScale = glm::dvec2(found->width, found->height) / page;
            glyph.uvOffset = glm::dvec2(found->x, found->y) / page;
            glyphs.push_back(glyph);
        }
        pen += static_cast<double>(found->xadvance);
    }
    return glyphs;
}

bool SameGlyphs(const std::vector<Hud::Glyph>& a, const std::vector<Hud::Glyph>& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (!(a[i].rect.min == b[i].rect.min && a[i].rect.size == b[i].rect.size && a[i].page == b[i].page && a[i].uvScale == b[i].uvScale &&
              a[i].uvOffset == b[i].uvOffset)) {
            return false;
        }
    }
    return true;
}

void AsciiLaysOutAndMeasuresBitForBit() {
    const Supersonic::BitmapFont& font = TestFont();
    CHECK(font.IsLoaded());
    Rng rng{77};
    int differences = 0;
    int withNewline = 0;
    int withMissing = 0;
    int glyphsMade = 0;
    std::string first;
    for (int i = 0; i < 6000; ++i) {
        const int length = static_cast<int>(rng.next() % 40);
        std::string text;
        for (int k = 0; k < length; ++k) {
            const std::uint32_t pick = rng.next() % 40;
            text.push_back(pick == 0 ? '\n' : pick == 1 ? '#' : pick == 2 ? '@' : pick == 3 ? '~' : static_cast<char>(32 + rng.next() % 95));
        }
        withNewline += text.find('\n') != std::string::npos ? 1 : 0;
        withMissing += (text.find('#') != std::string::npos || text.find('@') != std::string::npos || text.find('~') != std::string::npos) ? 1 : 0;
        const glm::dvec2 topLeft(static_cast<double>(rng.next() % 300) * 0.37, static_cast<double>(rng.next() % 200) * 0.41);
        const double scale = 0.125 + static_cast<double>(rng.next() % 16) * 0.0625;

        const std::vector<Hud::Glyph> now = Hud::LayOutTextFrom(font, text, topLeft, scale);
        const std::vector<Hud::Glyph> then = ByteLoopLayOutFrom(font, text, topLeft, scale);
        glyphsMade += static_cast<int>(now.size());
        // The centred layout, from the engine's own Measure, as the old LayOutText had it.
        const glm::dvec2 centre(topLeft.x + 100.0, topLeft.y + 50.0);
        const glm::dvec2 box = glm::dvec2(font.Measure(text)) * scale;
        const std::vector<Hud::Glyph> centredThen = ByteLoopLayOutFrom(font, text, centre - box * 0.5, scale);
        const std::vector<Hud::Glyph> centredNow = Hud::LayOutText(font, text, centre, scale);
        const glm::vec2 measuredNow = Hud::MeasureText(font, text);
        const glm::vec2 measuredThen = font.Measure(text);
        if (!SameGlyphs(now, then) || !SameGlyphs(centredNow, centredThen) || measuredNow != measuredThen) {
            if (differences++ == 0) first = text;
        }
    }
    CHECK_MSG(differences == 0, "ASCII lays out and measures exactly as the byte loops did; first difference: \"" + first + "\"");
    CHECK_MSG(withNewline > 1500 && withMissing > 1500 && glyphsMade > 60000,
              "and the strings reached newlines, missing glyphs and many glyphs: " + std::to_string(withNewline) + ", " + std::to_string(withMissing) + ", " +
                  std::to_string(glyphsMade));

    // The edges, by name.
    for (const char* text : {"", "\n", "\n\n", "a\n", "\nb", "a\n\nb\n", "~", "#@~", " ", "Part 17", "Chapter 3\nnew!"}) {
        CHECK_MSG(Hud::MeasureText(font, text) == font.Measure(text), std::string("MeasureText of \"") + text + "\"");
        CHECK_MSG(SameGlyphs(Hud::LayOutTextFrom(font, text, glm::dvec2(3.0, 4.0), 0.5), ByteLoopLayOutFrom(font, text, glm::dvec2(3.0, 4.0), 0.5)),
                  std::string("LayOutTextFrom of \"") + text + "\"");
    }
    CHECK_MSG(Hud::MeasureText(font, "") == glm::vec2(0.0f, 20.0f), "an empty text is no wide and one line tall, as the engine has it");
    CHECK_MSG(Hud::MeasureText(font, "a\n\nb").y == 60.0f, "three lines");

    // A font that is not loaded.
    const Supersonic::BitmapFont none;
    CHECK_MSG(Hud::MeasureText(none, "abc") == glm::vec2(0.0f), "an unloaded font measures nothing");
    CHECK_MSG(Hud::LayOutText(none, "abc", glm::dvec2(1.0), 1.0).empty() && Hud::LayOutTextFrom(none, "abc", glm::dvec2(1.0), 1.0).empty(), "and lays out nothing");
}

void AMultiByteCharacterIsOneGlyph() {
    const Supersonic::BitmapFont& font = TestFont();
    const Supersonic::FontGlyph* eacute = font.Find(0xE9);
    const Supersonic::FontGlyph* zhe = font.Find(0x416);
    const Supersonic::FontGlyph* hira = font.Find(0x3042);
    const Supersonic::FontGlyph* alef = font.Find(0x627);
    const Supersonic::FontGlyph* a = font.Find('a');
    CHECK_MSG(eacute && zhe && hira && alef && a, "the test font holds the glyphs");
    if (!(eacute && zhe && hira && alef && a)) return;

    const std::string text = B({0xC3, 0xA9}); // U+00E9
    const glm::dvec2 at(10.0, 20.0);
    const std::vector<Hud::Glyph> one = Hud::LayOutTextFrom(font, text, at, 2.0);
    CHECK_MSG(one.size() == 1, "the two bytes of U+00E9 are one glyph: " + std::to_string(one.size()));
    if (one.size() == 1) {
        CHECK_MSG(one[0].rect.min == at + glm::dvec2(eacute->xoffset, eacute->yoffset) * 2.0, "at the glyph's own offset");
        CHECK_MSG(one[0].rect.size == glm::dvec2(eacute->width, eacute->height) * 2.0, "its own size");
        CHECK_MSG(one[0].page == eacute->page, "its own page");
        CHECK_MSG(one[0].uvOffset == glm::dvec2(eacute->x, eacute->y) / glm::dvec2(256.0, 128.0), "its own place on the page");
    }
    CHECK_MSG(Hud::MeasureText(font, text) == glm::vec2(static_cast<float>(eacute->xadvance), 20.0f), "and measures as its one advance");
    // The old byte loop found nothing for the same bytes (C3 and A9 are not ids of the test font): the difference is the point.
    CHECK_MSG(ByteLoopLayOutFrom(font, text, at, 2.0).empty(), "where the byte loop drew nothing");

    // Two-, three- and four-byte characters side by side with ASCII: the pen advances by each one's own advance.
    const std::string mixed = "a" + B({0xC3, 0xA9}) + B({0xD0, 0x96}) + B({0xE3, 0x81, 0x82}) + B({0xD8, 0xA7}) + "a";
    const std::vector<Hud::Glyph> glyphs = Hud::LayOutTextFrom(font, mixed, glm::dvec2(0.0), 1.0);
    CHECK_MSG(glyphs.size() == 6, "six code points, six glyphs: " + std::to_string(glyphs.size()));
    const float advances = static_cast<float>(a->xadvance + eacute->xadvance + zhe->xadvance + hira->xadvance + alef->xadvance + a->xadvance);
    CHECK_MSG(Hud::MeasureText(font, mixed).x == advances, "and the sum of their advances is the width");
    if (glyphs.size() == 6) {
        const double lastPen = static_cast<double>(a->xadvance + eacute->xadvance + zhe->xadvance + hira->xadvance + alef->xadvance);
        CHECK_MSG(glyphs[5].rect.min.x == lastPen + a->xoffset, "the last 'a' sits after all five advances");
    }

    // A newline between multi-byte characters starts a line, and the width is the widest line.
    const std::vector<Hud::Glyph> lines = Hud::LayOutTextFrom(font, text + "\n" + text, glm::dvec2(0.0), 1.0);
    CHECK_MSG(lines.size() == 2 && lines[1].rect.min.y == 20.0 + eacute->yoffset && lines[1].rect.min.x == lines[0].rect.min.x, "a second line");
    CHECK_MSG(Hud::MeasureText(font, text + "\n" + text) == glm::vec2(static_cast<float>(eacute->xadvance), 40.0f), "and the widest line is one character");

    // Centred: the box is the decoded width.
    const std::vector<Hud::Glyph> centred = Hud::LayOutText(font, text, glm::dvec2(100.0, 50.0), 2.0);
    CHECK_MSG(centred.size() == 1, "centred, one glyph");
    if (centred.size() == 1) {
        const double left = 100.0 - static_cast<double>(eacute->xadvance) * 2.0 * 0.5;
        const double top = 50.0 - 20.0 * 2.0 * 0.5;
        CHECK_MSG(centred[0].rect.min == glm::dvec2(left + eacute->xoffset * 2.0, top + eacute->yoffset * 2.0), "centred on its one advance and the line height");
    }
}

void AMissingCodePointIsReportedAndDrawsNothing() {
    const Supersonic::BitmapFont& font = TestFont();
    const glm::dvec2 at(0.0);
    std::vector<std::uint32_t> missing;

    // Reported in the order met, a repeated one repeated; the pen does not advance for it, and the rest is laid out as if it were not there.
    const std::vector<Hud::Glyph> withHoles = Hud::LayOutTextFrom(font, "~a#b~", at, 1.0, &missing);
    const std::vector<Hud::Glyph> without = Hud::LayOutTextFrom(font, "ab", at, 1.0);
    CHECK_MSG(missing == (CodePoints{'~', '#', '~'}), "missing, in order and repeated: " + Show(missing));
    CHECK_MSG(SameGlyphs(withHoles, without), "and the others are laid out as if the missing were not there");
    CHECK_MSG(Hud::MeasureText(font, "~a#b~") == Hud::MeasureText(font, "ab"), "as is the width");

    // Nothing missing: nothing reported, and the vector is appended to and not cleared.
    missing = {0x41};
    Hud::LayOutTextFrom(font, "abc", at, 1.0, &missing);
    CHECK_MSG(missing == (CodePoints{0x41}), "nothing added for text the font has");
    Hud::LayOutTextFrom(font, "~", at, 1.0, &missing);
    CHECK_MSG(missing == (CodePoints{0x41, '~'}), "and appended to when something is");

    // The centred layout reports through the same parameter.
    missing.clear();
    Hud::LayOutText(font, "a" + B({0xE2, 0x82, 0xAC}) + "b", glm::dvec2(50.0), 1.0, &missing);
    CHECK_MSG(missing == (CodePoints{0x20AC}), "U+20AC is not in the test font: " + Show(missing));

    // The newline is a newline and never a missing glyph.
    missing.clear();
    Hud::LayOutTextFrom(font, "a\nb\n", at, 1.0, &missing);
    CHECK_MSG(missing.empty(), "a line break is not a glyph");

    // A malformed byte reaches the lookup as U+FFFD, which no font holds: reported, drawn as nothing. This is the change for the old encoding:
    // a Latin-1 byte on its own (E9) drew the font's glyph 233 before, and is malformed UTF-8 now.
    missing.clear();
    const std::vector<Hud::Glyph> bad = Hud::LayOutTextFrom(font, "a" + B({0xE9}) + "b", at, 1.0, &missing);
    CHECK_MSG(missing == (CodePoints{Utf8::kReplacement}), "a stray Latin-1 byte is reported as U+FFFD: " + Show(missing));
    CHECK_MSG(SameGlyphs(bad, without), "and draws nothing");
    CHECK_MSG(ByteLoopLayOutFrom(font, B({0xE9}), at, 1.0).size() == 1, "(the byte loop drew the glyph at id 233 for it)");
    missing.clear();
    Hud::LayOutTextFrom(font, "a" + B({0xE2, 0x82}), at, 1.0, &missing);
    CHECK_MSG(missing == (CodePoints{Utf8::kReplacement}), "a cut-off sequence is one U+FFFD: " + Show(missing));
}

void runTests() {
    AsciiIsItself();
    WellFormedSequences();
    StrayAndForbiddenBytes();
    OverlongSurrogateAndTooLarge();
    TruncatedAndInterruptedSequences();
    EncodeIsTheInverse();
    EveryCodePointRoundTrips();
    TheFuzzKeepsItsInvariants();
    NothingIsReadPastTheEnd();
    AsciiLaysOutAndMeasuresBitForBit();
    AMultiByteCharacterIsOneGlyph();
    AMissingCodePointIsReportedAndDrawsNothing();
}

} // namespace

MP_TEST_MAIN("test_mp_utf8", 300)
