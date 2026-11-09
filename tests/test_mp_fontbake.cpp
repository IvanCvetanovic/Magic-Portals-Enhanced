// Fonts made at run time for the languages the original's bitmaps cannot spell (planning step 237, PORT-33): sim/FontBake over the
// committed OFL faces of game/data/fonts, routed by languages.json's `fonts` block, and a coverage check of every string the shipped tables draw.
//
//   plan        languages.json's `fonts` block as loaded: the four faces, the two scripts and their ranges, the two slots with the original's
//               own numbers (line height 84 / base 54 / cap 40 / padding 2,8,8,2 for Matura84_shadow; 40 / 33 / 24 / 2,6,6,2 for Verdana40_shadow),
//               the routing of a code point to a face (Latin, Cyrillic and the Turkish letters to the slot's text face, kana and kanji and
//               the full-width forms to Japanese, the Arabic blocks and presentation forms to Arabic); a code point and a range parse only
//               in the one spelling the table uses
//   bake        samples in each script - 'Chast 21' in Cyrillic, 'Bolum 12' in Turkish, a katakana level, a shaped Arabic level, an achievement
//               line in the Verdana slot - give: no missing code point; every requested code point and the base set (every printable ASCII character,
//               space and no-break space) described with a glyph of nonzero size (a space has none, and says width 0); the font's own
//               lineHeight and base; pages that decode as 512 x 512 RGBA with white glyph pixels and black shadow-only pixels in them;
//               a capital H as tall as the slot's cap height and standing ON the baseline; every xadvance the FLOORED advance of the
//               face (checked against stb_truetype itself, and shown to differ from rounding on at least one glyph, so the check could fail)
//   engine      the descriptor loads with the engine's own BitmapFont - the bake's `bake` record is skipped, every character is found, the
//               pages are named beside it - which is what the layer will do
//   determinism two bakes of the same inputs are byte for byte equal, whatever order the code points were given in; any one input changed
//               (a face's bytes, the shadow, the name, a code point) changes the hash
//   cache       Ensure into an empty folder writes every page and the descriptor and leaves no temporary file; a second Ensure bakes and writes
//               NOTHING (files untouched, their times as they were); a missing page, a changed code point or a changed shadow bakes again; a
//               descriptor with another hash is replaced; a page that is empty, cut to 4 bytes or whose first byte is changed (a phone that died
//               between the rename and the data) bakes again and is whole afterwards
//   blur        a shadow blur of 0.01 px at a half-pixel offset (every weight of its kernel underflows) is the unblurred shadow, byte for byte,
//               and the glyphs and the shadow are on the page, not blank
//   refusals   a face the plan does not define, a face not given, bytes that are no TrueType font, a reference glyph the face lacks, a cell
//               too big for a page: each is an error that says which; a code point no face holds is NOT an error but is listed as missing
//   coverage    for each generated language (ru tr uk ja ar) every code point of every string of the shipped tables - the 14 UI strings after
//               Display, so Arabic as the shaped forms, the picker's native names, and, when out/data/achievements.<code>.json exists, every
//               achievement title and description and the secret achievement's two texts - is baked into BOTH slots without a hole. A MUTATION runs the same check with Arabic's
//               face removed from the routing, and with Japanese's, and requires holes: the check can fail
//
// MP_TEST_PORTDATA (TEST ONLY, as test_mp_strings) points the suite at a copy of game/data, so that a changed translation or a face taken
// out of the folder shows up in a real run. MP_FONTBAKE_DUMP=<existing folder> (a developer's switch, not a test) additionally writes the sample
// fonts of the bake case there as matura_<language>.fnt/png and verdana_<language>.fnt/png, so that they can be looked at beside the original's.
//
// No non-ASCII character is written in this file: MSVC is built without /utf-8 here, so every character is a \u escape.

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "core/BitmapFont.hpp"
#include "core/Json.hpp"
#include "sim/Achievements.hpp"
#include "sim/ArabicShaping.hpp"
#include "sim/FontBake.hpp"
#include "sim/Strings.hpp"
#include "sim/Utf8.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

// The C library stb_truetype would include, then stb_truetype itself in a namespace of this file's, as sim/FontBake.cpp does: the independent
// oracle for the advances and the heights.
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4100 4127 4189 4244 4245 4267 4305 4389 4456 4457 4505 4701 4703 4706)
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
namespace TestStbtt {
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "imstb_truetype.h"
} // namespace TestStbtt
#ifdef _MSC_VER
#pragma warning(pop)
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include "stb_image.h"

using namespace MagicPortals;
using Strings::Lang;

namespace {

namespace fs = std::filesystem;

using CodePoints = std::set<std::uint32_t>;

std::string EnvOf(const char* name) {
    std::string text;
#ifdef _WIN32
    char* value = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&value, &length, name) == 0 && value != nullptr) {
        text = value;
        std::free(value);
    }
#else
    if (const char* value = std::getenv(name); value != nullptr) text = value;
#endif
    return text;
}

std::string PortDataDir() {
    const std::string text = EnvOf("MP_TEST_PORTDATA");
    if (text.empty()) return MAGICPORTALS_PORT_DATA_DIR;
    std::printf("test_mp_fontbake: MUTATION - MP_TEST_PORTDATA is set, the port's data is read from %s, not from game/data\n", text.c_str());
    return text;
}

const std::string kData = PortDataDir();

fs::path ScratchRoot() {
    return fs::temp_directory_path() / "supersonic-test-mp-fontbake";
}

fs::path FreshDir(const char* name) {
    const fs::path dir = ScratchRoot() / name;
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    return dir;
}

std::string ReadFile(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::string Hex(std::uint32_t v) {
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "U+%04X", static_cast<unsigned>(v));
    return buffer;
}

std::string ShowSet(const std::vector<std::uint32_t>& points, std::size_t limit = 12) {
    std::string out;
    for (std::size_t i = 0; i < points.size() && i < limit; ++i) out += Hex(points[i]) + " ";
    if (points.size() > limit) out += "... (" + std::to_string(points.size()) + ")";
    return out;
}

// What every test shares: the table as the game loads it, and the faces read from the folder it names.
struct World {
    Strings::LanguageTable languages;
    std::vector<FontBake::FaceBytes> faces;
    std::string fontsDir;
    bool ok = false;
};

World& TheWorld() {
    static World world;
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        std::string error;
        if (!Strings::LoadLanguages(kData + Strings::kLanguagesFile, world.languages, error)) {
            CHECK_MSG(false, "languages.json loads: " + error);
            return world;
        }
        world.fontsDir = kData + "/" + world.languages.Fonts().folder;
        if (!FontBake::LoadFaces(world.languages.Fonts(), world.fontsDir, world.faces, error)) {
            CHECK_MSG(false, "the faces load: " + error);
            return world;
        }
        world.ok = true;
    }
    return world;
}

const FontBake::SlotSpec& Slot(const FontBake::Plan& plan, std::string_view id) {
    const FontBake::SlotSpec* slot = plan.Slot(id);
    CHECK_MSG(slot != nullptr, "the plan has the slot " + std::string(id));
    static const FontBake::SlotSpec none;
    return slot != nullptr ? *slot : none;
}

// A PNG's pixels, RGBA, or empty.
struct Image {
    int w = 0;
    int h = 0;
    std::vector<unsigned char> rgba;
};

Image Decode(const std::string& png) {
    Image image;
    int channels = 0;
    unsigned char* pixels = stbi_load_from_memory(reinterpret_cast<const unsigned char*>(png.data()), static_cast<int>(png.size()), &image.w, &image.h, &channels, 4);
    if (pixels == nullptr) return image;
    image.rgba.assign(pixels, pixels + static_cast<std::size_t>(image.w) * image.h * 4);
    stbi_image_free(pixels);
    return image;
}

struct Char {
    int x = 0, y = 0, w = 0, h = 0, xoffset = 0, yoffset = 0, xadvance = 0, page = 0;
};

// The `char` lines of a descriptor, read independently of FontBake::Describe.
std::map<std::uint32_t, Char> Chars(const std::string& fnt) {
    std::map<std::uint32_t, Char> out;
    std::istringstream stream(fnt);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.rfind("char ", 0) != 0) continue;
        std::map<std::string, int> kv;
        std::istringstream words(line);
        std::string word;
        while (words >> word) {
            const std::size_t eq = word.find('=');
            if (eq != std::string::npos) kv[word.substr(0, eq)] = std::atoi(word.c_str() + eq + 1);
        }
        Char c;
        c.x = kv["x"];
        c.y = kv["y"];
        c.w = kv["width"];
        c.h = kv["height"];
        c.xoffset = kv["xoffset"];
        c.yoffset = kv["yoffset"];
        c.xadvance = kv["xadvance"];
        c.page = kv["page"];
        out[static_cast<std::uint32_t>(kv["id"])] = c;
    }
    return out;
}

int Common(const std::string& fnt, const char* key) {
    const std::size_t at = fnt.find(std::string(" ") + key + "=");
    return at == std::string::npos ? -1 : std::atoi(fnt.c_str() + at + std::string(key).size() + 2);
}

// ---- the plan ----------------------------------------------------------------------------------------------------------------------------

void ThePlanIsTheOriginalsNumbers() {
    World& world = TheWorld();
    if (!world.ok) return;
    const FontBake::Plan& plan = world.languages.Fonts();
    CHECK(plan.loaded);
    CHECK_MSG(plan.folder == "fonts", "the folder");
    CHECK_EQ(static_cast<int>(plan.faces.size()), 4);
    CHECK_EQ(static_cast<int>(plan.scripts.size()), 2);
    CHECK_EQ(static_cast<int>(plan.slots.size()), 2);
    CHECK_MSG(plan.pickerSlot == "matura", "the picker is drawn in Matura's slot");

    const FontBake::SlotSpec& matura = Slot(plan, Strings::kSlotMatura);
    CHECK_MSG(matura.originalFont == "Matura84_shadow", "Matura's slot replaces Matura84_shadow");
    CHECK_EQ(matura.lineHeight, 84);
    CHECK_EQ(matura.base, 54);
    CHECK_NEAR(static_cast<float>(matura.capHeight), 40.0f); // the macro compares floats; the field is a double
    CHECK_MSG(matura.padUp == 2 && matura.padRight == 8 && matura.padDown == 8 && matura.padLeft == 2, "Matura84's padding is 2,8,8,2");
    CHECK_MSG(matura.spacingX == 2 && matura.spacingY == 2 && matura.pageSize == 512, "spacing 2,2 and 512 px pages");
    CHECK_MSG(matura.textFace == "kurale", "Latin and Cyrillic in Matura's slot are Kurale");
    const FontBake::SlotSpec& verdana = Slot(plan, Strings::kSlotVerdana);
    CHECK_MSG(verdana.originalFont == "Verdana40_shadow", "Verdana's slot replaces Verdana40_shadow");
    CHECK_EQ(verdana.lineHeight, 40);
    CHECK_EQ(verdana.base, 33);
    CHECK_NEAR(static_cast<float>(verdana.capHeight), 24.0f);
    CHECK_MSG(verdana.padUp == 2 && verdana.padRight == 6 && verdana.padDown == 6 && verdana.padLeft == 2, "Verdana40's padding is 2,6,6,2");
    CHECK_MSG(verdana.textFace == "noto_sans_bold", "and in Verdana's, Noto Sans Bold");

    // The routing, a code point at a time.
    const auto face = [&](const FontBake::SlotSpec& slot, std::uint32_t c) { return FontBake::FaceIdOf(plan, slot, c); };
    for (const FontBake::SlotSpec* slot : {&matura, &verdana}) {
        const std::string text = slot->textFace;
        CHECK_MSG(face(*slot, 'H') == text && face(*slot, '7') == text && face(*slot, ' ') == text && face(*slot, '%') == text, "ASCII is the text face's");
        CHECK_MSG(face(*slot, 0x0416) == text && face(*slot, 0x0490) == text, "Cyrillic, Ukrainian Ghe with upturn included, is the text face's");
        CHECK_MSG(face(*slot, 0x011F) == text && face(*slot, 0x0131) == text && face(*slot, 0x015F) == text, "the Turkish letters are the text face's");
        CHECK_MSG(face(*slot, 0x2019) == text && face(*slot, 0x00A0) == text, "typographic punctuation and the no-break space are the text face's");
        CHECK_MSG(face(*slot, 0x65E5) == "noto_sans_jp_bold" && face(*slot, 0x3042) == "noto_sans_jp_bold" && face(*slot, 0x30FC) == "noto_sans_jp_bold", "kanji and kana are Japanese");
        CHECK_MSG(face(*slot, 0x3002) == "noto_sans_jp_bold" && face(*slot, 0xFF01) == "noto_sans_jp_bold", "the CJK stop and the full-width forms are Japanese");
        CHECK_MSG(face(*slot, 0x0627) == "noto_sans_arabic_bold" && face(*slot, 0xFEFB) == "noto_sans_arabic_bold" && face(*slot, 0xFE91) == "noto_sans_arabic_bold",
                  "Arabic letters and presentation forms are Arabic");
    }

    // The spellings a table may use.
    std::uint32_t cp = 0;
    CHECK(FontBake::ParseCodePoint("U+0048", cp) && cp == 0x48);
    CHECK(FontBake::ParseCodePoint("U+10FFFF", cp) && cp == 0x10FFFF);
    CHECK_MSG(!FontBake::ParseCodePoint("u+0048", cp) && !FontBake::ParseCodePoint("U+48", cp) && !FontBake::ParseCodePoint("0048", cp) &&
                  !FontBake::ParseCodePoint("U+0g48", cp) && !FontBake::ParseCodePoint("U+110000", cp) && !FontBake::ParseCodePoint("U+004a", cp),
              "a code point is U+ and 4-6 upper-case hex digits and no more than U+10FFFF");
    FontBake::CodeRange range;
    CHECK(FontBake::ParseRange("3000-30FF", range) && range.first == 0x3000 && range.last == 0x30FF);
    CHECK_MSG(!FontBake::ParseRange("30FF-3000", range) && !FontBake::ParseRange("3000", range) && !FontBake::ParseRange("3000-", range) &&
                  !FontBake::ParseRange("-30FF", range) && !FontBake::ParseRange("3000--30FF", range),
              "a range is FIRST-LAST, in order");

    // The base set: every printable ASCII character (digits, punctuation, letters, the space) and the no-break space.
    const CodePoints base = FontBake::BaseSet();
    for (std::uint32_t c = '0'; c <= '9'; ++c) CHECK_MSG(base.count(c) == 1, "the base set has " + Hex(c));
    CHECK(base.count(' ') && base.count(0xA0) && base.count('%') && base.count('-') && base.count('!') && base.count('{') && base.count('~'));
    CHECK_MSG(base.count('a') == 1 && base.count('Z') == 1 && base.count('A') == 1 && base.count('z') == 1, "the ASCII letters (English is the fallback of every table)");
    CHECK_MSG(base.count(0x416) == 0 && base.count(0xE9) == 0 && base.count(0x3042) == 0, "and no letter of another script");
    CHECK_EQ(static_cast<int>(base.size()), 96); // U+0020-007E (95) and the no-break space
}

// ---- bake ---------------------------------------------------------------------------------------------------------------------------------

// Strings the game draws, in this file as code points.
const std::u32string kRuPart = U"\u0427\u0430\u0441\u0442\u044C 21";                                   // Chast' 21
const std::u32string kTrChapter = U"B\u00F6l\u00FCm 12 \u011F\u0131\u015F \u0130";                     // Bolum 12 plus the dotted capital I
const std::u32string kJaPart = U"\u30EC\u30D9\u30EB3 \u5B9F\u7E3E\uFF01";                              // level 3, an achievement, a full-width '!'
const std::u32string kArPart = U"\u0627\u0644\u0645\u0631\u062D\u0644\u0629 4";                        // the level 4
const std::u32string kRuAchievement = U"\u0414\u043E\u0441\u0442\u0438\u0436\u0435\u043D\u0438\u0435 \u043F\u043E\u043B\u0443\u0447\u0435\u043D\u043E!";

std::string U8(const std::u32string& text) {
    return ArabicShaping::ToUtf8(text);
}

// The code points of `text` as the game draws it in Arabic (shaped) or as it is.
CodePoints Drawn(Lang lang, const std::u32string& text) {
    CodePoints out;
    for (const std::uint32_t c : Utf8::Decode(Strings::Display(lang, U8(text)))) out.insert(c);
    return out;
}

struct Sample {
    const char* slot;
    const char* name;
    CodePoints points;
};

void DumpIfAsked(const char* name, const FontBake::Baked& baked) {
    const std::string dir = EnvOf("MP_FONTBAKE_DUMP");
    if (dir.empty()) return;
    std::ofstream(fs::path(dir) / (std::string(name) + ".fnt"), std::ios::binary) << baked.fnt;
    for (std::size_t p = 0; p < baked.pages.size(); ++p) std::ofstream(fs::path(dir) / (std::string(name) + "_" + std::to_string(p) + ".png"), std::ios::binary) << baked.pages[p];
}

// The independent oracle: the face, sized as the plan says.
struct Oracle {
    TestStbtt::stbtt_fontinfo info;
    float scale = 0.0f;
    bool ok = false;
};

Oracle OracleFor(const FontBake::Plan& plan, const FontBake::SlotSpec& slot, const std::vector<FontBake::FaceBytes>& faces, const std::string& id) {
    Oracle o;
    const FontBake::FaceSpec* spec = plan.Face(id);
    for (const FontBake::FaceBytes& bytes : faces) {
        if (bytes.id != id || spec == nullptr) continue;
        if (TestStbtt::stbtt_InitFont(&o.info, bytes.data.data(), 0) == 0) return o;
        int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        if (TestStbtt::stbtt_GetCodepointBox(&o.info, static_cast<int>(spec->reference), &x0, &y0, &x1, &y1) == 0 || y1 <= 0) return o;
        o.scale = static_cast<float>(slot.capHeight * spec->referenceHeight / y1);
        o.ok = true;
    }
    return o;
}

void BakesEveryScriptWithTheOriginalsMetrics() {
    World& world = TheWorld();
    if (!world.ok) return;
    const FontBake::Plan& plan = world.languages.Fonts();
    const std::vector<Sample> samples = {
        {"matura", "matura_ru", Drawn(Lang::Ru, kRuPart)},          {"matura", "matura_tr", Drawn(Lang::Tr, kTrChapter)},
        {"matura", "matura_ja", Drawn(Lang::Ja, kJaPart)},          {"matura", "matura_ar", Drawn(Lang::Ar, kArPart)},
        {"verdana", "verdana_ru", Drawn(Lang::Ru, kRuAchievement)}, {"verdana", "verdana_tr", Drawn(Lang::Tr, kTrChapter)},
        {"verdana", "verdana_ja", Drawn(Lang::Ja, kJaPart)},        {"verdana", "verdana_ar", Drawn(Lang::Ar, kArPart)},
    };
    bool floorDiffersFromRounding = false;
    for (Sample sample : samples) {
        // The capital H is how the faces are sized, and a letter every Latin string has: it is in each sample so that its height and baseline are checked.
        sample.points.insert('H');
        // And the Latin the English strings are made of, which every slot's text face draws whatever the language: the original's own words, so
        // that a developer's look at the dump (MP_FONTBAKE_DUMP) can set the English Matura beside the generated one.
        for (const char c : std::string("Part 21 Chapter Level Controls Achievement unlocked! new!")) sample.points.insert(static_cast<std::uint32_t>(static_cast<unsigned char>(c)));
        const FontBake::SlotSpec& slot = Slot(plan, sample.slot);
        FontBake::Baked baked;
        std::string error;
        CHECK_MSG(FontBake::Bake(plan, slot, world.faces, sample.points, sample.name, baked, error), std::string(sample.name) + " bakes: " + error);
        if (baked.fnt.empty()) continue;
        DumpIfAsked(sample.name, baked);
        CHECK_MSG(baked.missing.empty(), std::string(sample.name) + ": no missing code point: " + ShowSet(baked.missing));

        // The descriptor's own numbers.
        CHECK_MSG(Common(baked.fnt, "lineHeight") == slot.lineHeight && Common(baked.fnt, "base") == slot.base, std::string(sample.name) + ": lineHeight and base are the slot's");
        CHECK_MSG(Common(baked.fnt, "scaleW") == slot.pageSize && Common(baked.fnt, "scaleH") == slot.pageSize, std::string(sample.name) + ": the page size");
        CHECK_MSG(static_cast<int>(baked.pages.size()) == Common(baked.fnt, "pages") && !baked.pages.empty() && baked.pages.size() <= 8, std::string(sample.name) + ": the page count is stated, and small");
        CHECK_MSG(baked.fnt.find("kerning") == std::string::npos, "no kerning: the original applies none");

        // Every requested code point, and the base set, has a glyph of nonzero size, except where a glyph is only an advance.
        const std::map<std::uint32_t, Char> chars = Chars(baked.fnt);
        CodePoints wanted = sample.points;
        const CodePoints base = FontBake::BaseSet();
        wanted.insert(base.begin(), base.end());
        for (const std::uint32_t c : wanted) {
            const auto it = chars.find(c);
            CHECK_MSG(it != chars.end(), std::string(sample.name) + ": " + Hex(c) + " is in the font");
            if (it == chars.end()) continue;
            const bool blank = c == ' ' || c == 0xA0;
            CHECK_MSG(blank ? (it->second.w == 0 && it->second.h == 0) : (it->second.w > 0 && it->second.h > 0), std::string(sample.name) + ": " + Hex(c) + " has the size of what it is");
            CHECK_MSG(it->second.xadvance > 0, std::string(sample.name) + ": " + Hex(c) + " advances the pen");
        }
        CHECK_MSG(chars.size() == wanted.size(), std::string(sample.name) + ": and nothing else is in the font");

        // The pages decode, are square at the page size, hold white glyph and black shadow, and every cell lies on its page.
        bool anyWhite = false;
        bool anyShadowOnly = false;
        for (const std::string& png : baked.pages) {
            const Image image = Decode(png);
            CHECK_MSG(image.w == slot.pageSize && image.h == slot.pageSize, std::string(sample.name) + ": a page decodes at the page size");
            for (std::size_t i = 0; i + 3 < image.rgba.size(); i += 4) {
                anyWhite = anyWhite || (image.rgba[i + 3] == 255 && image.rgba[i] == 255);
                anyShadowOnly = anyShadowOnly || (image.rgba[i + 3] > 40 && image.rgba[i] == 0);
            }
        }
        CHECK_MSG(anyWhite, std::string(sample.name) + ": there are opaque white glyph pixels");
        CHECK_MSG(anyShadowOnly, std::string(sample.name) + ": and shadow-only pixels, black with alpha");
        for (const auto& [c, glyph] : chars) {
            if (glyph.w == 0) continue;
            CHECK_MSG(glyph.page >= 0 && glyph.page < static_cast<int>(baked.pages.size()) && glyph.x >= 0 && glyph.y >= 0 && glyph.x + glyph.w <= slot.pageSize &&
                          glyph.y + glyph.h <= slot.pageSize,
                      std::string(sample.name) + ": " + Hex(c) + "'s cell lies on its page");
        }
        // Cells on a page do not overlap, spacing apart.
        for (const auto& [a, ga] : chars) {
            for (const auto& [b, gb] : chars) {
                if (a >= b || ga.w == 0 || gb.w == 0 || ga.page != gb.page) continue;
                const bool apart = ga.x + ga.w + slot.spacingX <= gb.x || gb.x + gb.w + slot.spacingX <= ga.x || ga.y + ga.h + slot.spacingY <= gb.y ||
                                   gb.y + gb.h + slot.spacingY <= ga.y;
                if (!apart) CHECK_MSG(false, std::string(sample.name) + ": " + Hex(a) + " and " + Hex(b) + " are on the page with the spacing between them");
            }
        }

        // The capital H: as tall as the slot's cap height, on the baseline - for the faces that have one.
        const std::string textFace = slot.textFace;
        if (const auto h = chars.find('H'); h != chars.end()) {
            // Emboldening thickens the coverage by up to a pixel on every side, and the cell keeps a pixel of margin for it: the H is that much taller.
            const int grown = slot.embolden > 0.0 ? 2 : 0;
            const int bitmapH = h->second.h - slot.padUp - slot.padDown - grown;
            CHECK_MSG(std::abs(bitmapH - static_cast<int>(std::lround(slot.capHeight))) <= 1, std::string(sample.name) + ": the capital H is the cap height tall: " + std::to_string(bitmapH));
            CHECK_MSG(std::abs(h->second.yoffset + slot.padUp + grown / 2 + bitmapH - slot.base) <= 1, std::string(sample.name) + ": and stands on the baseline");
        }

        // The advances are the faces' own, floored, checked against stb_truetype itself.
        for (const std::uint32_t c : wanted) {
            const auto it = chars.find(c);
            if (it == chars.end()) continue;
            const Oracle oracle = OracleFor(plan, slot, world.faces, FontBake::FaceIdOf(plan, slot, c));
            CHECK_MSG(oracle.ok, "an oracle for " + Hex(c));
            if (!oracle.ok) continue;
            int advance = 0;
            int bearing = 0;
            TestStbtt::stbtt_GetCodepointHMetrics(&oracle.info, static_cast<int>(c), &advance, &bearing);
            const double exact = static_cast<double>(advance) * oracle.scale;
            CHECK_MSG(it->second.xadvance == static_cast<int>(std::floor(exact)), std::string(sample.name) + ": " + Hex(c) + " advances " + std::to_string(it->second.xadvance) + " where the face says " + std::to_string(exact));
            if (std::floor(exact) != std::floor(exact + 0.5)) floorDiffersFromRounding = true;
        }
    }
    CHECK_MSG(floorDiffersFromRounding, "at least one advance is not a whole number, so flooring and rounding differ and the check above can fail");
}

void ABakeIsDeterministicAndItsHashSeesEveryInput() {
    World& world = TheWorld();
    if (!world.ok) return;
    const FontBake::Plan& plan = world.languages.Fonts();
    const FontBake::SlotSpec& slot = Slot(plan, Strings::kSlotMatura);
    const CodePoints points = Drawn(Lang::Ru, kRuPart);
    FontBake::Baked a;
    FontBake::Baked b;
    std::string error;
    CHECK_MSG(FontBake::Bake(plan, slot, world.faces, points, "det", a, error), "bake a: " + error);
    // The same code points, in another order of insertion into another container: a set has no order, but the faces' vector may.
    std::vector<FontBake::FaceBytes> reversed(world.faces.rbegin(), world.faces.rend());
    CHECK_MSG(FontBake::Bake(plan, slot, reversed, points, "det", b, error), "bake b: " + error);
    CHECK_MSG(a.fnt == b.fnt && a.pages == b.pages && a.hash == b.hash, "two bakes of the same inputs, faces handed over in another order, are byte for byte equal");
    CHECK_MSG(a.hash.size() == 32, "a 128-bit hash in hex");

    const std::string base = FontBake::InputHash(plan, slot, world.faces, points, "det");
    CHECK_MSG(base == a.hash, "InputHash is the hash Bake wrote");
    CHECK_MSG(FontBake::InputHash(plan, slot, world.faces, points, "other") != base, "another name is another hash");
    CodePoints more = points;
    more.insert(0x0416);
    CHECK_MSG(FontBake::InputHash(plan, slot, world.faces, more, "det") != base, "another code point");
    CodePoints inBase = points;
    inBase.insert('7'); // already in the base set: not another input
    CHECK_MSG(FontBake::InputHash(plan, slot, world.faces, inBase, "det") == base, "a code point the base set holds anyway changes nothing");
    FontBake::SlotSpec changed = slot;
    changed.shadow.sigma += 0.25;
    CHECK_MSG(FontBake::InputHash(plan, changed, world.faces, points, "det") != base, "the shadow's blur");
    changed = slot;
    changed.capHeight += 1.0;
    CHECK_MSG(FontBake::InputHash(plan, changed, world.faces, points, "det") != base, "the cap height");
    changed = slot;
    changed.embolden = slot.embolden + 0.25;
    CHECK_MSG(FontBake::InputHash(plan, changed, world.faces, points, "det") != base, "the emboldening");
    std::vector<FontBake::FaceBytes> edited = world.faces;
    for (FontBake::FaceBytes& face : edited) {
        if (face.id == slot.textFace) face.data[face.data.size() / 2] ^= 1;
    }
    CHECK_MSG(FontBake::InputHash(plan, slot, edited, points, "det") != base, "a byte of the text face");
    edited = world.faces;
    for (FontBake::FaceBytes& face : edited) {
        if (face.id == "noto_sans_bold") face.data[face.data.size() / 2] ^= 1; // the OTHER slot's text face: Matura's slot never draws it
    }
    CHECK_MSG(FontBake::InputHash(plan, slot, edited, points, "det") == base, "a byte of a face the slot does not use is not an input");
    // The other slot over the same code points is another font.
    CHECK_MSG(FontBake::InputHash(plan, Slot(plan, Strings::kSlotVerdana), world.faces, points, "det") != base, "another slot");
}

void TheEnginesOwnReaderLoadsIt() {
    World& world = TheWorld();
    if (!world.ok) return;
    const FontBake::Plan& plan = world.languages.Fonts();
    for (const std::string_view slotId : {Strings::kSlotMatura, Strings::kSlotVerdana}) {
        const FontBake::SlotSpec& slot = Slot(plan, slotId);
        const fs::path dir = FreshDir("engine");
        CodePoints points = Drawn(Lang::Ja, kJaPart);
        const CodePoints more = Drawn(Lang::Ar, kArPart);
        points.insert(more.begin(), more.end());
        const FontBake::Written written = FontBake::Ensure(plan, slot, world.faces, points, dir.string(), "engine_font");
        CHECK_MSG(written.ok, "Ensure: " + written.error);
        Supersonic::BitmapFont font;
        std::string error;
        CHECK_MSG(font.Load(written.fntPath, error), "the engine's BitmapFont loads the descriptor: " + error);
        CHECK(font.IsLoaded());
        CHECK_EQ(font.LineHeight(), slot.lineHeight);
        CHECK_EQ(font.Base(), slot.base);
        CHECK_EQ(font.PageSize().x, slot.pageSize);
        CHECK_MSG(!font.Pages().empty(), "it names pages");
        for (const std::string& page : font.Pages()) CHECK_MSG(fs::is_regular_file(page), "and the page is beside it: " + page);
        CodePoints wanted = points;
        const CodePoints base = FontBake::BaseSet();
        wanted.insert(base.begin(), base.end());
        for (const std::uint32_t c : wanted) {
            const Supersonic::FontGlyph* glyph = font.Find(c);
            CHECK_MSG(glyph != nullptr, "the engine finds " + Hex(c));
            if (glyph != nullptr && c != ' ' && c != 0xA0) CHECK_MSG(glyph->width > 0 && glyph->height > 0, Hex(c) + " has a size in the engine's reading");
        }
        CHECK_MSG(font.Find(0x1F600) == nullptr, "and a code point nobody asked for is not found");
        // The measure of an ASCII text, byte-wise as the engine does it, is nonzero (the engine is not patched).
        CHECK_MSG(font.Measure("Part 21").x > 0.0f, "the engine measures ASCII with it");
    }
}

// ---- Ensure --------------------------------------------------------------------------------------------------------------------------------

std::vector<fs::path> Files(const fs::path& dir) {
    std::vector<fs::path> files;
    for (const fs::directory_entry& entry : fs::directory_iterator(dir)) files.push_back(entry.path());
    std::sort(files.begin(), files.end());
    return files;
}

void TheCacheWritesNothingTheSecondTime() {
    World& world = TheWorld();
    if (!world.ok) return;
    const FontBake::Plan& plan = world.languages.Fonts();
    const FontBake::SlotSpec& slot = Slot(plan, Strings::kSlotMatura);
    const fs::path dir = FreshDir("cache");
    const CodePoints points = Drawn(Lang::Tr, kTrChapter);

    const FontBake::Written first = FontBake::Ensure(plan, slot, world.faces, points, dir.string(), "cache_font");
    CHECK_MSG(first.ok && !first.cached, "the first Ensure bakes: " + first.error);
    std::vector<fs::path> files = Files(dir);
    CHECK_MSG(first.filesWritten == static_cast<int>(files.size()) && files.size() >= 2, "and writes every page and the descriptor: " + std::to_string(first.filesWritten));
    for (const fs::path& file : files) CHECK_MSG(file.extension() != ".tmp", "no temporary file is left: " + file.string());
    CHECK_MSG(fs::exists(dir / "cache_font.fnt") && fs::exists(dir / "cache_font_0.png"), "the files are <name>.fnt and <name>_<page>.png");
    CHECK_MSG(first.missing.empty(), "nothing is missing");

    std::map<std::string, fs::file_time_type> stamps;
    std::map<std::string, std::string> contents;
    for (const fs::path& file : files) {
        stamps[file.string()] = fs::last_write_time(file);
        contents[file.string()] = ReadFile(file);
    }
    const FontBake::Written second = FontBake::Ensure(plan, slot, world.faces, points, dir.string(), "cache_font");
    CHECK_MSG(second.ok && second.cached && second.filesWritten == 0, "the second Ensure finds the files and writes nothing");
    CHECK_MSG(Files(dir) == files, "no file appeared");
    for (const fs::path& file : files) {
        CHECK_MSG(fs::last_write_time(file) == stamps[file.string()] && ReadFile(file) == contents[file.string()], "and none was touched: " + file.string());
    }

    // A page that is gone: baked again, and the same bytes come back.
    fs::remove(dir / "cache_font_0.png");
    const FontBake::Written third = FontBake::Ensure(plan, slot, world.faces, points, dir.string(), "cache_font");
    CHECK_MSG(third.ok && !third.cached && third.filesWritten >= 1, "a missing page bakes again");
    CHECK_MSG(ReadFile(dir / "cache_font_0.png") == contents[(dir / "cache_font_0.png").string()], "to the same bytes");

    // Another code point, another shadow: another hash, baked again over the same names.
    CodePoints more = points;
    more.insert(0x0416);
    const FontBake::Written fourth = FontBake::Ensure(plan, slot, world.faces, more, dir.string(), "cache_font");
    CHECK_MSG(fourth.ok && !fourth.cached && fourth.filesWritten >= 1, "a new code point bakes again");
    FontBake::Described described;
    CHECK(FontBake::Describe(ReadFile(dir / "cache_font.fnt"), described));
    CHECK_MSG(described.chars.count(0x0416) == 1 && described.hash == FontBake::InputHash(plan, slot, world.faces, more, "cache_font"), "and the descriptor says so");
    FontBake::SlotSpec soft = slot;
    soft.shadow.sigma = 3.0;
    const FontBake::Written fifth = FontBake::Ensure(plan, soft, world.faces, more, dir.string(), "cache_font");
    CHECK_MSG(fifth.ok && !fifth.cached, "a changed shadow bakes again");
    const FontBake::Written sixth = FontBake::Ensure(plan, soft, world.faces, more, dir.string(), "cache_font");
    CHECK_MSG(sixth.ok && sixth.cached && sixth.filesWritten == 0, "and is then found");

    // A descriptor somebody left that is not ours: replaced, never trusted.
    {
        std::ofstream(dir / "cache_font.fnt", std::ios::binary | std::ios::trunc) << "common lineHeight=1 base=1 scaleW=1 scaleH=1 pages=0\nbake hash=nothing missing=-\n";
    }
    const FontBake::Written seventh = FontBake::Ensure(plan, soft, world.faces, more, dir.string(), "cache_font");
    CHECK_MSG(seventh.ok && !seventh.cached && seventh.filesWritten >= 1, "a descriptor with another hash is replaced");
    CHECK_MSG(ReadFile(dir / "cache_font.fnt").find("lineHeight=84") != std::string::npos, "by a real one");

    // A folder that is not there: an error that says so, not a crash.
    const FontBake::Written nowhere = FontBake::Ensure(plan, slot, world.faces, points, (ScratchRoot() / "no" / "such" / "folder").string(), "x");
    CHECK_MSG(!nowhere.ok && !nowhere.error.empty(), "an absent folder is refused: " + nowhere.error);

    // Describe reads a missing list back.
    CHECK(FontBake::Describe("common lineHeight=2 base=1 scaleW=4 scaleH=4\npage id=0 file=\"a_0.png\"\nchar id=65 x=0\nbake hash=ff missing=1F600,41\n", described));
    CHECK_MSG(described.missing == std::vector<std::uint32_t>({0x1F600, 0x41}) && described.hash == "ff" && described.pageFiles == std::vector<std::string>({"a_0.png"}) &&
                  described.chars == CodePoints({65}),
              "Describe: the hash, the missing list, the pages and the chars");
    CHECK_MSG(!FontBake::Describe("char id=1\n", described), "and a text with no common line is no descriptor");
}

// A page a dying phone left empty or cut short (WriteWhole only flushes: the rename can reach storage before the data does), or one that is no PNG, under a
// descriptor whose hash is right: a cache check that only asked "is the page a file" would find it for ever and that language's text would be blank on every launch.
void ATornPageIsBakedAgain() {
    World& world = TheWorld();
    if (!world.ok) return;
    const FontBake::Plan& plan = world.languages.Fonts();
    const FontBake::SlotSpec& slot = Slot(plan, Strings::kSlotMatura);
    const CodePoints points = Drawn(Lang::Tr, kTrChapter);
    const fs::path dir = FreshDir("torn");

    const FontBake::Written first = FontBake::Ensure(plan, slot, world.faces, points, dir.string(), "torn_font");
    CHECK_MSG(first.ok && !first.cached, "the first Ensure bakes: " + first.error);
    FontBake::Described described;
    CHECK(FontBake::Describe(ReadFile(dir / "torn_font.fnt"), described));
    CHECK_MSG(!described.pageFiles.empty(), "the descriptor names its pages");
    std::map<std::string, std::string> whole; // each page as it was baked
    for (const std::string& file : described.pageFiles) whole[file] = ReadFile(dir / file);
    for (const auto& [file, bytes] : whole) CHECK_MSG(bytes.size() > 8 && bytes.compare(1, 3, "PNG") == 0, file + " is a PNG");

    const FontBake::Written intact = FontBake::Ensure(plan, slot, world.faces, points, dir.string(), "torn_font");
    CHECK_MSG(intact.ok && intact.cached && intact.filesWritten == 0, "pages that are whole are found and nothing is written");

    // The first page and, when there is more than one, the last: the check runs on every page the descriptor names.
    std::vector<std::string> victims{described.pageFiles.front()};
    if (described.pageFiles.back() != victims.front()) victims.push_back(described.pageFiles.back());
    for (const std::string& victim : victims) {
        const std::string& good = whole[victim];
        std::string flipped = good;
        flipped[0] = static_cast<char>(flipped[0] ^ 0x01);
        const std::vector<std::pair<std::string, std::string>> damages = {
            {"a zero-length page", std::string()},
            {"a page cut to 4 bytes", good.substr(0, 4)},
            {"a page whose first byte is changed", flipped},
        };
        for (const auto& [what, bytes] : damages) {
            const std::string label = what + " (" + victim + ")";
            std::ofstream(dir / victim, std::ios::binary | std::ios::trunc).write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
            const FontBake::Written again = FontBake::Ensure(plan, slot, world.faces, points, dir.string(), "torn_font");
            CHECK_MSG(again.ok && !again.cached && again.filesWritten > 0, label + " is baked again: " + again.error);
            for (const auto& [file, original] : whole) CHECK_MSG(ReadFile(dir / file) == original, label + ": " + file + " is whole afterwards");
            const FontBake::Written then = FontBake::Ensure(plan, slot, world.faces, points, dir.string(), "torn_font");
            CHECK_MSG(then.ok && then.cached && then.filesWritten == 0, label + ": and the repaired cache is found");
        }
    }
}

// A blur narrower than a pixel, centred between two pixels, used to underflow every weight of the shadow's kernel to 0, and the division made NaN of the shadow
// and, through the composing, of the glyph: the whole font came out blank without an error. It is the unblurred shadow now, byte for byte.
void ATinyBlurIsNoBlur() {
    World& world = TheWorld();
    if (!world.ok) return;
    const FontBake::Plan& plan = world.languages.Fonts();
    const FontBake::SlotSpec& slot = Slot(plan, Strings::kSlotMatura);
    const CodePoints points = Drawn(Lang::Ru, kRuPart);
    for (const double dy : {0.5, -0.5}) {
        FontBake::SlotSpec tiny = slot;
        tiny.shadow.sigma = 0.01;
        tiny.shadow.dx = 0.5;
        tiny.shadow.dy = dy;
        FontBake::SlotSpec sharp = tiny;
        sharp.shadow.sigma = 0.0;
        const std::string label = std::string("sigma 0.01 at (0.5, ") + (dy > 0.0 ? "0.5" : "-0.5") + ")";
        FontBake::Baked blurred;
        FontBake::Baked unblurred;
        std::string error;
        CHECK_MSG(FontBake::Bake(plan, tiny, world.faces, points, "tiny", blurred, error), label + " bakes: " + error);
        CHECK_MSG(FontBake::Bake(plan, sharp, world.faces, points, "tiny", unblurred, error), "and sigma 0 bakes: " + error);
        CHECK_MSG(!blurred.pages.empty() && blurred.pages == unblurred.pages, label + " is the unblurred shadow, page for page");

        bool anyWhite = false;
        std::size_t shadowOnly = 0;
        for (const std::string& png : blurred.pages) {
            const Image image = Decode(png);
            CHECK_MSG(image.w == slot.pageSize && image.h == slot.pageSize, label + ": a page decodes");
            for (std::size_t i = 0; i + 3 < image.rgba.size(); i += 4) {
                anyWhite = anyWhite || (image.rgba[i + 3] == 255 && image.rgba[i] == 255);
                if (image.rgba[i + 3] > 40 && image.rgba[i] == 0) ++shadowOnly;
            }
        }
        CHECK_MSG(anyWhite, label + ": the glyphs are drawn");
        CHECK_MSG(shadowOnly > 100, label + ": and so is a shadow under them: " + std::to_string(shadowOnly) + " pixels of black with alpha");
    }
}

// ---- refusals -------------------------------------------------------------------------------------------------------------------------------

void RefusalsAreErrorsThatSayWhy() {
    World& world = TheWorld();
    if (!world.ok) return;
    const FontBake::Plan& plan = world.languages.Fonts();
    const FontBake::SlotSpec& slot = Slot(plan, Strings::kSlotMatura);
    const CodePoints points = Drawn(Lang::Ru, kRuPart);
    FontBake::Baked baked;
    std::string error;

    // A face not given.
    std::vector<FontBake::FaceBytes> fewer;
    for (const FontBake::FaceBytes& face : world.faces) {
        if (face.id != "noto_sans_arabic_bold") fewer.push_back(face);
    }
    CHECK_MSG(!FontBake::Bake(plan, slot, fewer, points, "x", baked, error) && error.find("noto_sans_arabic_bold") != std::string::npos, "a face not given: " + error);
    // Bytes that are no TrueType font.
    std::vector<FontBake::FaceBytes> junk = world.faces;
    for (FontBake::FaceBytes& face : junk) {
        if (face.id == "kurale") face.data.assign(64, 7);
    }
    CHECK_MSG(!FontBake::Bake(plan, slot, junk, points, "x", baked, error) && error.find("TrueType") != std::string::npos, "not a font: " + error);
    // A slot that names a face the plan does not define.
    FontBake::SlotSpec stray = slot;
    stray.textFace = "nobody";
    CHECK_MSG(!FontBake::Bake(plan, stray, world.faces, points, "x", baked, error) && error.find("nobody") != std::string::npos, "an undefined face: " + error);
    // A reference glyph the face lacks: Kurale has no kanji.
    FontBake::Plan odd = plan;
    for (FontBake::FaceSpec& face : odd.faces) {
        if (face.id == "kurale") face.reference = 0x65E5;
    }
    CHECK_MSG(!FontBake::Bake(odd, slot, world.faces, points, "x", baked, error) && error.find("Kurale") != std::string::npos, "a reference glyph the face lacks: " + error);
    // A cell too big for a page: a cap height the size of the page.
    FontBake::SlotSpec huge = slot;
    huge.capHeight = 400.0;
    huge.pageSize = 128;
    CHECK_MSG(!FontBake::Bake(plan, huge, world.faces, points, "x", baked, error) && error.find("page") != std::string::npos, "a cell that does not fit a page: " + error);
    // No line height: an unusable slot.
    FontBake::SlotSpec empty;
    CHECK_MSG(!FontBake::Bake(plan, empty, world.faces, points, "x", baked, error) && !error.empty(), "an empty slot: " + error);
    // A code point no face holds is not an error: it is listed.
    CodePoints odder = points;
    odder.insert(0x1F600);
    CHECK_MSG(FontBake::Bake(plan, slot, world.faces, odder, "x", baked, error), "a code point no face holds still bakes: " + error);
    CHECK_MSG(baked.missing == std::vector<std::uint32_t>({0x1F600}), "and is the one missing: " + ShowSet(baked.missing));
    CHECK_MSG(Chars(baked.fnt).count(0x1F600) == 0, "and not in the font");
    CHECK_MSG(baked.fnt.find("missing=1F600") != std::string::npos, "and in the record");
}

// ---- coverage ---------------------------------------------------------------------------------------------------------------------------------

// Every string, at any depth, of a JSON file that is not a note: the achievements' translations, whatever shape the later step gives them.
void CollectStrings(const Supersonic::Json::Value& value, std::vector<std::string>& out) {
    if (value.IsString()) {
        out.push_back(value.AsString());
    } else if (value.IsArray()) {
        for (const Supersonic::Json::Value& item : value.AsArray()) CollectStrings(item, out);
    } else if (value.IsObject()) {
        for (const auto& [key, item] : value.AsObject()) {
            if (!key.empty() && key.front() == '_') continue;
            CollectStrings(item, out);
        }
    }
}

// The achievements' translated texts of `code` - the secret achievement's title and sentence among them, the sentence with the chapter and the level
// as the digit zero, as Achievements::SecretSamples gives it to the layer - or none when the gitignored file is absent.
bool AchievementTexts(const std::string& code, std::vector<std::string>& out) {
    const fs::path achievements = fs::path(MAGICPORTALS_ACHIEVEMENTS_FILE);
    if (achievements.empty()) return false;
    const fs::path file = achievements.parent_path() / ("achievements." + code + ".json");
    if (!fs::is_regular_file(file)) return false;
    Supersonic::Json::Value root;
    const std::string text = ReadFile(file); // the parser keeps a reference to it
    Supersonic::Json::Parser parser(text);
    if (!parser.Parse(root)) {
        CHECK_MSG(false, file.string() + " parses: " + parser.Error());
        return false;
    }
    CollectStrings(root, out);
    for (std::string& piece : out) {
        piece = Strings::Fill(piece, {{Strings::Placeholder::kWorld, "0"}, {Strings::Placeholder::kLevel, "0"}});
    }
    // The secret block is part of what was collected (the face for the Japanese kanji is subset from these very strings).
    Achievements::Translation translation;
    std::string error;
    const bool read = Achievements::LoadTranslation(file.string(), code, translation, error);
    CHECK_MSG(read, error);
    if (read) {
        Achievements::Content english;
        english.secret.title = "x";
        const std::vector<std::string> samples = Achievements::SecretSamples(Achievements::Localized(english, &translation));
        for (const std::string& sample : samples) {
            CHECK_MSG(std::find(out.begin(), out.end(), sample) != out.end(), file.string() + ": the secret text is part of the coverage input: " + sample);
        }
    }
    return true;
}

// The holes of `plan` for `lang`: every code point of what the shipped tables draw that some slot's font lacks. Empty when covered.
std::vector<std::string> HolesOf(const FontBake::Plan& plan, const Strings::Table& table, const std::vector<FontBake::FaceBytes>& faces,
                                 const std::vector<std::string>& achievements, std::size_t& drawn) {
    std::vector<std::string> holes;
    const std::string code(Strings::kLangCodes[Strings::IndexOf(table.Language())]);
    // Both slots hold every UI string's code points: a string can be drawn in either, and a key moved between them must not open a hole.
    CodePoints ui = Strings::SlotCodePoints(table, Strings::kSlotMatura);
    const CodePoints verdana = Strings::SlotCodePoints(table, Strings::kSlotVerdana, achievements);
    ui.insert(verdana.begin(), verdana.end());
    drawn = ui.size();
    for (const std::string_view slotId : {Strings::kSlotMatura, Strings::kSlotVerdana}) {
        const FontBake::SlotSpec* slot = plan.Slot(slotId);
        FontBake::Baked baked;
        std::string error;
        if (slot == nullptr || !FontBake::Bake(plan, *slot, faces, ui, std::string(slotId) + "_" + code, baked, error)) {
            holes.push_back(code + " " + std::string(slotId) + ": " + error);
            continue;
        }
        const std::map<std::uint32_t, Char> chars = Chars(baked.fnt);
        for (const std::uint32_t c : ui) {
            if (chars.count(c) == 0) holes.push_back(code + " " + std::string(slotId) + " lacks " + Hex(c));
        }
    }
    return holes;
}

void EveryStringTheTablesDrawIsInBothSlots() {
    World& world = TheWorld();
    if (!world.ok) return;
    const FontBake::Plan& plan = world.languages.Fonts();
    std::size_t absent = 0;
    std::size_t covered = 0;
    for (std::size_t i = 0; i < Strings::kLangCount; ++i) {
        const Lang lang = static_cast<Lang>(i);
        if (world.languages.FontSetOf(lang) != Strings::FontSet::Generated) continue;
        Strings::Table table;
        std::string error;
        CHECK_MSG(table.Load(kData, lang, error), "the table loads: " + error);
        std::vector<std::string> achievements;
        const std::string code = world.languages.CodeOf(lang);
        if (!AchievementTexts(code, achievements)) {
            ++absent;
            std::printf("test_mp_fontbake: no achievements.%s.json beside the achievements table: its texts are not part of this run's coverage\n", code.c_str());
        }
        std::size_t drawn = 0;
        const std::vector<std::string> holes = HolesOf(plan, table, world.faces, achievements, drawn);
        CHECK_MSG(holes.empty(), "a hole in the fonts of " + code + ": " + (holes.empty() ? "" : holes.front()) + " (" + std::to_string(holes.size()) + ")");
        CHECK_MSG(drawn >= 30, code + ": the fourteen strings have more than a handful of code points: " + std::to_string(drawn));
        if (!achievements.empty()) CHECK_MSG(drawn >= 60, code + ": with the achievements, more still: " + std::to_string(drawn));
        ++covered;
    }
    CHECK_EQ(static_cast<int>(covered), 5);
    if (absent == covered) std::printf("test_mp_fontbake: SKIPPED the achievements' part of the coverage: none of the five files is there\n");

    // The picker draws every language's own name at once, in one atlas.
    const CodePoints picker = Strings::PickerCodePoints(world.languages);
    CHECK_MSG(picker.count(0x65E5) == 1 && picker.count(0x0420) == 1 && picker.count(0xFE91) == 1 && picker.count('E') == 1, "the picker's code points are the names': kanji, Cyrillic, shaped Arabic, Latin");
    const FontBake::SlotSpec& pickerSlot = Slot(plan, plan.pickerSlot);
    FontBake::Baked baked;
    std::string error;
    CHECK_MSG(FontBake::Bake(plan, pickerSlot, world.faces, picker, "picker", baked, error), "the picker bakes: " + error);
    CHECK_MSG(baked.missing.empty(), "and has every glyph of all eleven names: " + ShowSet(baked.missing));
    DumpIfAsked("picker", baked);
    // Arabic as ordinary letters would be missing a form in the presentation-form range: the shaped text is what the picker holds.
    CHECK_MSG(picker.count(0x0627) == 0, "the picker holds Arabic as the shaped forms, not the letters");
}

// The coverage check must be able to fail: the same check with a face dropped from the routing finds the holes.
void TheCoverageCheckCanFail() {
    World& world = TheWorld();
    if (!world.ok) return;
    const FontBake::Plan& plan = world.languages.Fonts();
    for (const char* script : {"arabic", "japanese"}) {
        FontBake::Plan mutated = plan;
        for (FontBake::SlotSpec& slot : mutated.slots) {
            for (auto& [id, face] : slot.scriptFaces) {
                if (id == script) face = slot.textFace; // the script now goes to a face that has none of it
            }
        }
        const Lang lang = std::string(script) == "arabic" ? Lang::Ar : Lang::Ja;
        Strings::Table table;
        std::string error;
        CHECK_MSG(table.Load(kData, lang, error), "the table loads: " + error);
        std::size_t drawn = 0;
        const std::vector<std::string> holes = HolesOf(mutated, table, world.faces, {}, drawn);
        CHECK_MSG(!holes.empty(), std::string("with ") + script + " dropped from the routing, the coverage finds holes");
        std::size_t drawnReal = 0;
        CHECK_MSG(HolesOf(plan, table, world.faces, {}, drawnReal).empty(), std::string("and the real routing has none for ") + script);
    }
    // And a language that needs no script of its own is not hurt by dropping them: Russian over the mutated plan is still whole.
    FontBake::Plan mutated = plan;
    for (FontBake::SlotSpec& slot : mutated.slots) {
        for (auto& [id, face] : slot.scriptFaces) {
            (void)id;
            face = slot.textFace;
        }
    }
    Strings::Table russian;
    std::string error;
    CHECK(russian.Load(kData, Lang::Ru, error));
    std::size_t drawn = 0;
    CHECK_MSG(HolesOf(mutated, russian, world.faces, {}, drawn).empty(), "Russian needs only the text faces: dropping the scripts' faces leaves it whole");
}

void runTests() {
    ThePlanIsTheOriginalsNumbers();
    BakesEveryScriptWithTheOriginalsMetrics();
    ABakeIsDeterministicAndItsHashSeesEveryInput();
    TheEnginesOwnReaderLoadsIt();
    TheCacheWritesNothingTheSecondTime();
    ATornPageIsBakedAgain();
    ATinyBlurIsNoBlur();
    RefusalsAreErrorsThatSayWhy();
    EveryStringTheTablesDrawIsInBothSlots();
    TheCoverageCheckCanFail();
}

} // namespace

MP_TEST_MAIN("test_mp_fontbake", 800)
