// The modern controls' fallback pictures: sim/PadArt, two arrows, restart, pause, clear-portals and a gear, drawn by arithmetic into
// a bitmap in memory (planning step 180).
//
// What is pinned is what the rest of the design leans on, not how pretty the shapes are (the pictures are the owner's to judge):
//   * a picture is a pure function of its glyph, its size and its margin, to the last byte;
//   * the two arrows are mirror images, the right one made by drawing the same shape the other way round and not by copying bytes;
//   * the body keeps exactly the margin the data says (controls.json art.generated.*.body_inset, 14 px of 128), with nothing but
//     zero alpha outside it, and the body's own edges lie on pixel boundaries, so the layout can draw it into a box's solid face;
//   * every pixel is straight alpha and none is the magenta some engines key on;
//   * each glyph is where its name says it is, by probing points of its body.
//
// Pure: no window, no file, no original art. It reads one committed file of the port's own, game/data/controls.json, for the margin
// the pictures are drawn with, so it never skips.

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "sim/Controls.hpp"
#include "sim/PadArt.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <map>
#include <string>
#include <utility>
#include <vector>

using namespace MagicPortals;
using PadArt::Bitmap;
using PadArt::Glyph;
using PadArt::Pixel;

namespace {

// The port's own data: MP_TEST_PORTDATA when it is set (TEST ONLY: a mutation test runs this same binary on a copy of game/data), else
// the build's own. _dupenv_s: std::getenv is C4996 on Windows.
std::string PortDataDir() {
    std::string text;
#ifdef _WIN32
    char* value = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&value, &length, "MP_TEST_PORTDATA") == 0 && value != nullptr) {
        text = value;
        std::free(value);
    }
#else
    if (const char* value = std::getenv("MP_TEST_PORTDATA"); value != nullptr) text = value;
#endif
    if (text.empty()) return MAGICPORTALS_PORT_DATA_DIR;
    std::printf("test_mp_padart: MUTATION - MP_TEST_PORTDATA is set, the port's data is read from %s, not from game/data\n", text.c_str());
    return text;
}

const std::string kData = PortDataDir();

// The magenta some engines treat as "no pixel here".
constexpr PadArt::Rgb kMagenta{255, 0, 255};

// The sizes the cases draw at: Magic Rampage's own 128, half and twice it, and the picture's smallest.
const std::array<int, 3> kSizes = {64, 128, 256};
constexpr double kInset = 0.109375; // controls.json art.generated.*.body_inset, 14 px of 128: a test of the data reads it below

// Drawn once and kept: a 256 px picture is a million samples, and most cases only read it.
const Bitmap& Drawn(Glyph glyph, int size, double inset = kInset) {
    static std::map<std::string, Bitmap> cache;
    const std::string key = std::string(PadArt::NameOf(glyph)) + "/" + std::to_string(size) + "/" + std::to_string(inset);
    auto found = cache.find(key);
    if (found == cache.end()) found = cache.emplace(key, PadArt::Draw(glyph, size, inset)).first;
    return found->second;
}

std::string Where(Glyph glyph, int size) {
    return std::string(PadArt::NameOf(glyph)) + " at " + std::to_string(size);
}

struct Box {
    int minX = std::numeric_limits<int>::max();
    int minY = std::numeric_limits<int>::max();
    int maxX = -1;
    int maxY = -1; // inclusive
    int count = 0;

    void Add(int x, int y) {
        minX = std::min(minX, x);
        minY = std::min(minY, y);
        maxX = std::max(maxX, x);
        maxY = std::max(maxY, y);
        ++count;
    }
    bool operator==(const Box& other) const {
        return minX == other.minX && minY == other.minY && maxX == other.maxX && maxY == other.maxY;
    }
};

template <typename F>
Box BoxOf(const Bitmap& bitmap, F&& wanted) {
    Box box;
    for (int y = 0; y < bitmap.size; ++y) {
        for (int x = 0; x < bitmap.size; ++x) {
            if (wanted(bitmap.At(x, y))) box.Add(x, y);
        }
    }
    return box;
}

// The body of a picture of `size` and margin `margin`: the pixels [margin, size - margin) on both axes, as an inclusive box.
Box BodyBox(int size, int margin) {
    Box box;
    box.minX = box.minY = margin;
    box.maxX = box.maxY = size - 1 - margin;
    return box;
}

bool IsGlyphColour(const Pixel& p) {
    return p.a == 255 && p.r == PadArt::kGlyphColour.r && p.g == PadArt::kGlyphColour.g && p.b == PadArt::kGlyphColour.b;
}

std::string Describe(const Box& box) {
    return "x " + std::to_string(box.minX) + ".." + std::to_string(box.maxX) + " y " + std::to_string(box.minY) + ".." + std::to_string(box.maxY);
}

// ---- 1: a pure function ---------------------------------------------------------------------------------------------------

void AGlyphIsDeterministic() {
    for (const Glyph glyph : PadArt::kGlyphs) {
        for (const int size : {64, 128}) {
            const Bitmap first = PadArt::Draw(glyph, size, kInset);
            const Bitmap second = PadArt::Draw(glyph, size, kInset);
            CHECK_MSG(!first.Empty() && first.rgba == second.rgba && first.size == second.size, Where(glyph, size));
        }
    }
    // Drawing something else in between changes nothing: no state is kept from one picture to the next.
    const Bitmap before = PadArt::Draw(Glyph::Restart, 96, kInset);
    (void)PadArt::Draw(Glyph::Gear, 128, 0.2);
    (void)PadArt::Draw(Glyph::WalkLeft, 64, kInset);
    const Bitmap after = PadArt::Draw(Glyph::Restart, 96, kInset);
    CHECK(before.rgba == after.rgba);
    // A different margin is a different picture, and a different glyph is.
    CHECK(PadArt::Draw(Glyph::Pause, 128, 0.0).rgba != PadArt::Draw(Glyph::Pause, 128, kInset).rgba);
    CHECK(PadArt::Draw(Glyph::Pause, 128, kInset).rgba != PadArt::Draw(Glyph::Gear, 128, kInset).rgba);
}

// ---- 2: the arrows -------------------------------------------------------------------------------------------------------

void TheTwoArrowsAreMirrorImages() {
    for (const int size : {16, 33, 64, 100, 128, 200, 256}) {
        const Bitmap& left = Drawn(Glyph::WalkLeft, size);
        const Bitmap& right = Drawn(Glyph::WalkRight, size);
        int unequal = 0;
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                if (!(right.At(x, y) == left.At(size - 1 - x, y))) ++unequal;
            }
        }
        // Every pixel, not the shape: the samples of a mirrored pixel are the mirror of the samples, to the last bit.
        CHECK_MSG(unequal == 0, "size " + std::to_string(size) + ": " + std::to_string(unequal) + " pixels differ from the mirror");
        // And neither is its own mirror, so the test cannot pass on a symmetric picture.
        int symmetric = 0;
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                if (left.At(x, y) == left.At(size - 1 - x, y)) ++symmetric;
            }
        }
        CHECK_MSG(symmetric < size * size, "size " + std::to_string(size) + ": the left arrow is its own mirror");
        CHECK(left.rgba != right.rgba);
    }
}

void TheArrowsPointWhereTheirNamesSay() {
    // The tip is a point and the base is a side: of the glyph's pixels, the end the arrow points to holds few and the other many.
    for (const int size : kSizes) {
        for (const Glyph glyph : {Glyph::WalkLeft, Glyph::WalkRight}) {
            const Bitmap& bitmap = Drawn(glyph, size);
            const Box box = BoxOf(bitmap, IsGlyphColour);
            const int tenth = std::max(1, (box.maxX - box.minX + 1) / 10);
            int atLow = 0;
            int atHigh = 0;
            for (int y = box.minY; y <= box.maxY; ++y) {
                for (int x = box.minX; x < box.minX + tenth; ++x) atLow += IsGlyphColour(bitmap.At(x, y)) ? 1 : 0;
                for (int x = box.maxX - tenth + 1; x <= box.maxX; ++x) atHigh += IsGlyphColour(bitmap.At(x, y)) ? 1 : 0;
            }
            const std::string message = Where(glyph, size) + ": the tenth at the left holds " + std::to_string(atLow) + ", at the right " + std::to_string(atHigh);
            if (glyph == Glyph::WalkLeft) {
                CHECK_MSG(atLow < atHigh, message);
            } else {
                CHECK_MSG(atHigh < atLow, message);
            }
        }
    }
}

void EachControlHasItsOwnGlyph() {
    CHECK(PadArt::GlyphOf(Controls::Id::Left) == Glyph::WalkLeft);
    CHECK(PadArt::GlyphOf(Controls::Id::Right) == Glyph::WalkRight);
    CHECK(PadArt::GlyphOf(Controls::Id::Reset) == Glyph::Restart);
    CHECK(PadArt::GlyphOf(Controls::Id::Menu) == Glyph::Pause);
    CHECK(PadArt::GlyphOf(Controls::Id::Clear) == Glyph::ClearPortals);
    // The names the data, the flag and the saved file use for a control are the glyph's own, so a file named for one is the other's.
    for (const Controls::Id id : Controls::kClaimOrder) {
        CHECK_MSG(std::string(PadArt::NameOf(PadArt::GlyphOf(id))) == Controls::KeyOf(id), Controls::KeyOf(id));
    }
    // Five controls, five different glyphs, and the gear is none of them.
    std::vector<Glyph> seen;
    for (const Controls::Id id : Controls::kClaimOrder) seen.push_back(PadArt::GlyphOf(id));
    std::sort(seen.begin(), seen.end());
    CHECK(std::adjacent_find(seen.begin(), seen.end()) == seen.end());
    CHECK(std::find(seen.begin(), seen.end(), Glyph::Gear) == seen.end());
    CHECK(std::find(seen.begin(), seen.end(), Glyph::Globe) == seen.end());
    CHECK_EQ(PadArt::kGlyphs.size(), std::size_t{7});
}

// ---- 3: the body and its margin ---------------------------------------------------------------------------------------

void ABodyKeepsTheStatedMarginAndNoOtherAlpha() {
    // The margin is the nearest whole pixel of the fraction: 7, 14 and 28 are exact for 0.109375, 11 and 8 are rounded.
    CHECK_EQ(PadArt::BodyMarginPx(64, kInset), 7);
    CHECK_EQ(PadArt::BodyMarginPx(128, kInset), 14);
    CHECK_EQ(PadArt::BodyMarginPx(256, kInset), 28);
    CHECK_EQ(PadArt::BodyMarginPx(100, kInset), 11);
    CHECK_EQ(PadArt::BodyMarginPx(128, 0.0), 0);
    CHECK_EQ(PadArt::BodyMarginPx(64, 0.25), 16);

    for (const int size : kSizes) {
        const int margin = PadArt::BodyMarginPx(size, kInset);
        const Box body = BodyBox(size, margin);
        const Bitmap& left = Drawn(Glyph::WalkLeft, size);
        for (const Glyph glyph : PadArt::kGlyphs) {
            const Bitmap& bitmap = Drawn(glyph, size);
            // Nothing outside the body: every margin pixel is (0, 0, 0, 0), no alpha and no colour to bleed into a filter.
            int strayed = 0;
            for (int y = 0; y < size; ++y) {
                for (int x = 0; x < size; ++x) {
                    const bool inBody = x >= margin && x < size - margin && y >= margin && y < size - margin;
                    if (!inBody && !(bitmap.At(x, y) == Pixel{})) ++strayed;
                }
            }
            CHECK_MSG(strayed == 0, Where(glyph, size) + ": " + std::to_string(strayed) + " pixels outside the body are not transparent");
            // The body reaches its stated margin on every side, by any alpha and by the solid part alone (alpha 250 and over, the
            // threshold Magic Rampage's own margin was measured with): the plate's straight edges lie on pixel boundaries.
            const Box anyAlpha = BoxOf(bitmap, [](const Pixel& p) { return p.a > 0; });
            const Box solid = BoxOf(bitmap, [](const Pixel& p) { return p.a >= 250; });
            CHECK_MSG(anyAlpha == body, Where(glyph, size) + ": alpha > 0 spans " + Describe(anyAlpha) + ", the body is " + Describe(body));
            CHECK_MSG(solid == body, Where(glyph, size) + ": alpha >= 250 spans " + Describe(solid) + ", the body is " + Describe(body));
            // No other alpha: the plate alone makes it, so the seven glyphs share one alpha channel, and a pixel is partly covered only
            // in a corner of the plate, where it is rounded (a corner is 0.3 of the half body, 15 px of the 100 of a 128 picture).
            bool sameAlpha = true;
            bool partialOnlyInCorners = true;
            const int corner = static_cast<int>(std::ceil(0.30 * (size - 2 * margin) / 2.0)) + 1;
            for (int y = 0; y < size; ++y) {
                for (int x = 0; x < size; ++x) {
                    const int a = bitmap.At(x, y).a;
                    if (a != left.At(x, y).a) sameAlpha = false;
                    if (a > 0 && a < 255) {
                        const int nearX = std::min(x - margin, size - 1 - margin - x);
                        const int nearY = std::min(y - margin, size - 1 - margin - y);
                        if (nearX >= corner || nearY >= corner) partialOnlyInCorners = false;
                    }
                }
            }
            CHECK_MSG(sameAlpha, Where(glyph, size) + ": its alpha differs from the arrow's");
            CHECK_MSG(partialOnlyInCorners, Where(glyph, size) + ": a partly covered pixel lies outside the plate's corners");
        }
    }

    // A margin that is not whole is rounded, and the body is what is left: 100 px at 0.109375 is 11 px a side.
    for (const Glyph glyph : PadArt::kGlyphs) {
        const Bitmap& bitmap = Drawn(glyph, 100);
        CHECK_MSG(BoxOf(bitmap, [](const Pixel& p) { return p.a > 0; }) == BodyBox(100, 11), Where(glyph, 100));
    }
    // No margin at all: the plate fills the picture's edge to edge. The largest margin keeps half of it.
    for (const Glyph glyph : PadArt::kGlyphs) {
        CHECK_MSG(BoxOf(Drawn(glyph, 64, 0.0), [](const Pixel& p) { return p.a > 0; }) == BodyBox(64, 0), Where(glyph, 64) + " with no margin");
        CHECK_MSG(BoxOf(Drawn(glyph, 64, 0.25), [](const Pixel& p) { return p.a > 0; }) == BodyBox(64, 16), Where(glyph, 64) + " with the largest margin");
    }
}

// ---- 4: the pixels ---------------------------------------------------------------------------------------------------

void NoPixelIsExactMagentaAndTransparencyIsBlack() {
    for (const int size : {16, 64, 128, 256}) {
        for (const Glyph glyph : PadArt::kGlyphs) {
            const Bitmap& bitmap = Drawn(glyph, size);
            int magenta = 0;
            int coloured = 0;
            for (int y = 0; y < size; ++y) {
                for (int x = 0; x < size; ++x) {
                    const Pixel p = bitmap.At(x, y);
                    if (p.r == kMagenta.r && p.g == kMagenta.g && p.b == kMagenta.b) ++magenta;
                    if (p.a == 0 && (p.r != 0 || p.g != 0 || p.b != 0)) ++coloured;
                }
            }
            CHECK_MSG(magenta == 0, Where(glyph, size) + ": " + std::to_string(magenta) + " magenta pixels");
            CHECK_MSG(coloured == 0, Where(glyph, size) + ": " + std::to_string(coloured) + " transparent pixels carry a colour");
        }
    }
    // The three colours the glyphs are made of are not magenta either.
    CHECK(!(PadArt::kPlateFill == kMagenta) && !(PadArt::kPlateRim == kMagenta) && !(PadArt::kGlyphColour == kMagenta));
}

void TheAlphaIsTheShareOfASamplesThatAreThereAndTheColourIsTheirs() {
    // Sixteen samples a pixel: alpha is (n x 255 + 8) / 16 for n of them covered, which is 17 values, and a pixel that is not
    // solid still has the colour of what covers it: no channel dips under the darkest of the three colours (a black halo).
    std::array<bool, 256> allowed{};
    for (int covered = 0; covered <= 16; ++covered) allowed[static_cast<std::size_t>((covered * 255 + 8) / 16)] = true;
    CHECK(allowed[0] && allowed[255]);
    const auto darkest = [](int a, int b, int c) { return std::min(a, std::min(b, c)); };
    const int lowR = darkest(PadArt::kPlateFill.r, PadArt::kPlateRim.r, PadArt::kGlyphColour.r);
    const int lowG = darkest(PadArt::kPlateFill.g, PadArt::kPlateRim.g, PadArt::kGlyphColour.g);
    const int lowB = darkest(PadArt::kPlateFill.b, PadArt::kPlateRim.b, PadArt::kGlyphColour.b);
    for (const Glyph glyph : PadArt::kGlyphs) {
        const Bitmap& bitmap = Drawn(glyph, 128);
        int offGrid = 0;
        int haloed = 0;
        int partial = 0;
        for (int y = 0; y < 128; ++y) {
            for (int x = 0; x < 128; ++x) {
                const Pixel p = bitmap.At(x, y);
                if (!allowed[p.a]) ++offGrid;
                if (p.a > 0) {
                    if (p.r < lowR || p.g < lowG || p.b < lowB) ++haloed;
                    if (p.a < 255) ++partial;
                }
            }
        }
        CHECK_MSG(offGrid == 0, Where(glyph, 128) + ": " + std::to_string(offGrid) + " alphas are not a share of sixteen samples");
        CHECK_MSG(haloed == 0, Where(glyph, 128) + ": " + std::to_string(haloed) + " pixels are darker than any of the three colours");
        CHECK_MSG(partial > 0, Where(glyph, 128) + ": no pixel is partly covered, so nothing is anti-aliased");
    }
}

void EveryGlyphIsAsLargeAsAsked() {
    for (const Glyph glyph : PadArt::kGlyphs) {
        for (const int size : {16, 17, 24, 32, 64, 100, 128, 256}) {
            const Bitmap& bitmap = Drawn(glyph, size);
            CHECK_MSG(!bitmap.Empty() && bitmap.size == size, Where(glyph, size));
            CHECK_MSG(bitmap.rgba.size() == static_cast<std::size_t>(size) * static_cast<std::size_t>(size) * 4, Where(glyph, size) + ": the byte count");
        }
    }
    // The largest picture is drawn too (one glyph: it is a million and a half samples a megapixel).
    const Bitmap largest = PadArt::Draw(Glyph::Gear, PadArt::kMaxSize, kInset);
    CHECK(largest.size == PadArt::kMaxSize && largest.rgba.size() == static_cast<std::size_t>(PadArt::kMaxSize) * PadArt::kMaxSize * 4);
    CHECK(BoxOf(largest, [](const Pixel& p) { return p.a > 0; }) == BodyBox(PadArt::kMaxSize, PadArt::BodyMarginPx(PadArt::kMaxSize, kInset)));
}

void WhatIsRefusedIsRefusedWhole() {
    // An empty bitmap, never a partial one, for a size or a margin that cannot be drawn.
    const double notANumber = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();
    CHECK(PadArt::Draw(Glyph::Restart, PadArt::kMinSize - 1, kInset).Empty());
    CHECK(PadArt::Draw(Glyph::Restart, 0, kInset).Empty());
    CHECK(PadArt::Draw(Glyph::Restart, -64, kInset).Empty());
    CHECK(PadArt::Draw(Glyph::Restart, PadArt::kMaxSize + 1, kInset).Empty());
    CHECK(PadArt::Draw(Glyph::Restart, 128, -0.001).Empty());
    CHECK(PadArt::Draw(Glyph::Restart, 128, PadArt::kMaxBodyInset + 0.001).Empty());
    CHECK(PadArt::Draw(Glyph::Restart, 128, notANumber).Empty());
    CHECK(PadArt::Draw(Glyph::Restart, 128, infinity).Empty());
    CHECK(PadArt::Draw(Glyph::Restart, 128, -infinity).Empty());
    CHECK(PadArt::Draw(Glyph::Restart, 128, 0.5).Empty());
    const Bitmap refused = PadArt::Draw(Glyph::Gear, 0, kInset);
    CHECK(refused.size == 0 && refused.rgba.empty());
    // The limits themselves are drawn.
    CHECK(!PadArt::Draw(Glyph::Restart, PadArt::kMinSize, 0.0).Empty());
    CHECK(!PadArt::Draw(Glyph::Restart, 128, PadArt::kMaxBodyInset).Empty());
    // A pixel outside the bitmap is a transparent one, not a read past it.
    const Bitmap& bitmap = Drawn(Glyph::Pause, 64);
    CHECK(bitmap.At(-1, 0) == Pixel{} && bitmap.At(0, -1) == Pixel{} && bitmap.At(64, 0) == Pixel{} && bitmap.At(0, 64) == Pixel{});
    CHECK(PadArt::Bitmap{}.At(0, 0) == Pixel{});
}

// ---- 5: where each glyph is ------------------------------------------------------------------------------------------

// A point of the body, in the body's own coordinates ([-1, 1], +y down), and what a pixel there must be: the glyph's colour, the
// plate's fill, the plate's rim, or nothing (transparent: the margin, or a rounded corner of the plate). Every probe was taken a
// clear 0.03 of the body from the nearest edge of what it names, so the pixel that holds it is wholly that colour at 64 px and up.
enum class Is { Glyph, Fill, Rim, Nothing };
struct Probe {
    Is is;
    double x;
    double y;
};

const std::vector<Probe>& ProbesFor(Glyph glyph) {
    // The left arrow's, and the right arrow's are these with x negated (the arrows are mirror images).
    static const std::vector<Probe> left = {
        {Is::Glyph, 0.0, 0.0},      {Is::Glyph, -0.30, 0.0},   {Is::Glyph, 0.30, 0.0},    {Is::Glyph, 0.0, 0.30},   {Is::Glyph, 0.0, -0.30},
        {Is::Fill, -0.58, 0.0},     {Is::Fill, 0.0, 0.55},     {Is::Fill, 0.58, 0.0},     {Is::Fill, 0.58, 0.58},   {Is::Fill, 0.0, -0.55},
        {Is::Rim, 0.9625, 0.0},     {Is::Rim, 0.0, 0.9625},    {Is::Nothing, 1.12, 0.0},  {Is::Nothing, 0.0, -1.12}, {Is::Nothing, -0.97, -0.97},
        {Is::Nothing, 0.97, 0.97},
    };
    static const std::vector<Probe> right = [] {
        std::vector<Probe> mirrored = left;
        for (Probe& probe : mirrored) probe.x = -probe.x;
        return mirrored;
    }();
    static const std::vector<Probe> restart = {
        {Is::Glyph, -0.46, 0.05},   {Is::Glyph, 0.0, 0.51},    {Is::Glyph, 0.325, 0.375}, {Is::Glyph, 0.0, -0.41},  {Is::Glyph, 0.12, -0.38},
        {Is::Fill, 0.0, 0.05},      {Is::Fill, 0.40, -0.20},   {Is::Fill, 0.50, -0.35},   {Is::Fill, -0.70, 0.05},  {Is::Fill, 0.70, 0.70},
        {Is::Rim, 0.9625, 0.0},     {Is::Nothing, -1.12, 0.0},
    };
    static const std::vector<Probe> pause = {
        {Is::Glyph, -0.22, 0.0},    {Is::Glyph, 0.22, 0.0},    {Is::Glyph, 0.22, 0.34},   {Is::Glyph, -0.22, -0.34},
        {Is::Fill, 0.0, 0.0},       {Is::Fill, 0.0, 0.30},     {Is::Fill, -0.50, 0.0},    {Is::Fill, 0.22, 0.52},   {Is::Fill, -0.22, -0.52},
        {Is::Rim, 0.9625, 0.0},     {Is::Nothing, 0.0, 1.12},
    };
    static const std::vector<Probe> clearPortals = {
        {Is::Glyph, 0.46, 0.0},     {Is::Glyph, 0.0, 0.46},    {Is::Glyph, -0.46, 0.0},   {Is::Glyph, 0.0, 0.0},    {Is::Glyph, 0.10, 0.10},  {Is::Glyph, -0.10, 0.10},
        {Is::Fill, 0.28, 0.0},      {Is::Fill, 0.0, -0.28},    {Is::Fill, 0.62, 0.0},     {Is::Fill, 0.62, 0.62},
        {Is::Rim, 0.9625, 0.0},     {Is::Nothing, 1.12, 0.0},
    };
    static const std::vector<Probe> gear = {
        {Is::Glyph, 0.25, 0.0},     {Is::Glyph, 0.0, 0.25},    {Is::Glyph, 0.47, 0.0},    {Is::Glyph, 0.0, -0.47},  {Is::Glyph, 0.33, 0.33},  {Is::Glyph, -0.33, 0.33},
        {Is::Fill, 0.0, 0.0},       {Is::Fill, 0.10, 0.0},     {Is::Fill, 0.45, 0.19},    {Is::Fill, 0.19, 0.45},   {Is::Fill, 0.62, 0.0},
        {Is::Rim, 0.9625, 0.0},     {Is::Nothing, 1.12, 0.0},
    };
    // The globe (planning step 239): the ring, the equator, a parallel and the meridian are the glyph; between them the plate shows. (0.149, 0.35) is on the
    // meridian alone: the ellipse x^2/0.23^2 + y^2/0.46^2 = 1 at y = 0.35 is x = 0.1492, and the ring (from 0.40 out), the equator and the parallels
    // (0.21 to 0.29) are all clear of it.
    static const std::vector<Probe> globe = {
        {Is::Glyph, 0.46, 0.0},     {Is::Glyph, 0.0, 0.46},    {Is::Glyph, -0.46, 0.0},   {Is::Glyph, 0.0, 0.0},    {Is::Glyph, 0.30, 0.0},   {Is::Glyph, 0.0, 0.25},
        {Is::Glyph, 0.10, -0.25},   {Is::Glyph, 0.149, 0.35},  {Is::Glyph, -0.149, -0.35},
        {Is::Fill, 0.0, 0.12},      {Is::Fill, 0.35, 0.12},    {Is::Fill, -0.15, -0.12},  {Is::Fill, 0.0, 0.37},    {Is::Fill, 0.62, 0.0},    {Is::Fill, 0.62, 0.62},
        {Is::Rim, 0.9625, 0.0},     {Is::Nothing, 1.12, 0.0},
    };
    switch (glyph) {
    case Glyph::WalkLeft:
        return left;
    case Glyph::WalkRight:
        return right;
    case Glyph::Restart:
        return restart;
    case Glyph::Pause:
        return pause;
    case Glyph::ClearPortals:
        return clearPortals;
    case Glyph::Gear:
        return gear;
    case Glyph::Globe:
        return globe;
    }
    return left;
}

bool PixelIs(const Pixel& p, Is is) {
    const auto colour = [&](const PadArt::Rgb& c) { return p.a == 255 && p.r == c.r && p.g == c.g && p.b == c.b; };
    switch (is) {
    case Is::Glyph:
        return colour(PadArt::kGlyphColour);
    case Is::Fill:
        return colour(PadArt::kPlateFill);
    case Is::Rim:
        return colour(PadArt::kPlateRim);
    case Is::Nothing:
        return p.a == 0;
    }
    return false;
}

const char* NameOf(Is is) {
    switch (is) {
    case Is::Glyph:
        return "the glyph's colour";
    case Is::Fill:
        return "the plate's fill";
    case Is::Rim:
        return "the plate's rim";
    case Is::Nothing:
        return "transparent";
    }
    return "?";
}

void EveryGlyphIsWhereItsNameSays() {
    for (const Glyph glyph : PadArt::kGlyphs) {
        const std::vector<Probe>& probes = ProbesFor(glyph);
        for (const int size : kSizes) {
            const Bitmap& bitmap = Drawn(glyph, size);
            const int margin = PadArt::BodyMarginPx(size, kInset);
            const double body = static_cast<double>(size - 2 * margin);
            for (const Probe& probe : probes) {
                // The rim is 0.075 of the half body wide: under two pixels at 64, so a pixel there is a mix. It is probed from 128 up.
                if (probe.is == Is::Rim && size < 128) continue;
                const int x = static_cast<int>(std::floor(static_cast<double>(margin) + (probe.x + 1.0) / 2.0 * body));
                const int y = static_cast<int>(std::floor(static_cast<double>(margin) + (probe.y + 1.0) / 2.0 * body));
                const Pixel p = bitmap.At(x, y);
                CHECK_MSG(PixelIs(p, probe.is), Where(glyph, size) + ": the pixel at body (" + std::to_string(probe.x) + ", " + std::to_string(probe.y) +
                                                    ") should be " + NameOf(probe.is) + ", it is (" + std::to_string(p.r) + ", " + std::to_string(p.g) + ", " +
                                                    std::to_string(p.b) + ", " + std::to_string(p.a) + ")");
            }
        }
    }
}

void EveryGlyphFillsItsPlateAndSitsInItsMiddle() {
    constexpr int kSize = 128;
    const int margin = PadArt::BodyMarginPx(kSize, kInset);
    const double body = kSize - 2 * margin;
    for (const Glyph glyph : PadArt::kGlyphs) {
        const Bitmap& bitmap = Drawn(glyph, kSize);
        const Box box = BoxOf(bitmap, IsGlyphColour);
        const std::string where = Where(glyph, kSize);
        // A fraction of the plate that is neither a speck nor the whole of it.
        const double share = box.count / (body * body);
        CHECK_MSG(share > 0.06 && share < 0.25, where + ": the glyph is " + std::to_string(share) + " of the body");
        // Centred on the picture's middle to a few percent (an arrow is shifted a little so its mass, not its box, is centred), and clear
        // of the rim: the glyph's box stays inside the middle 0.65 of the body.
        const double centreX = (box.minX + box.maxX + 1) / 2.0;
        const double centreY = (box.minY + box.maxY + 1) / 2.0;
        CHECK_MSG(std::fabs(centreX - kSize / 2.0) <= 0.04 * kSize && std::fabs(centreY - kSize / 2.0) <= 0.04 * kSize,
                  where + ": its box is centred on (" + std::to_string(centreX) + ", " + std::to_string(centreY) + ")");
        const double clearance = 0.5 * body * (1.0 - 0.65);
        CHECK_MSG(box.minX >= margin + clearance - 1 && box.maxX <= kSize - 1 - margin - clearance + 1 && box.minY >= margin + clearance - 1 &&
                      box.maxY <= kSize - 1 - margin - clearance + 1,
                  where + ": it reaches " + Describe(box));
    }
    // Seven different pictures.
    for (std::size_t a = 0; a < PadArt::kGlyphCount; ++a) {
        for (std::size_t b = a + 1; b < PadArt::kGlyphCount; ++b) {
            CHECK_MSG(Drawn(PadArt::kGlyphs[a], kSize).rgba != Drawn(PadArt::kGlyphs[b], kSize).rgba,
                      std::string(PadArt::NameOf(PadArt::kGlyphs[a])) + " and " + PadArt::NameOf(PadArt::kGlyphs[b]) + " are the same picture");
        }
    }
}

// ---- 6: the data ---------------------------------------------------------------------------------------------------

void TheGeneratedTierOfControlsJsonIsWhatThePadArtDraws() {
    Controls::Rules rules;
    std::string error;
    CHECK_MSG(Controls::LoadRules(kData + "/controls.json", rules, error), "controls.json reads: " + error);
    const std::size_t generated = static_cast<std::size_t>(Controls::ArtTier::Generated);
    for (const Controls::Id id : Controls::kClaimOrder) {
        const auto& entry = rules.art[generated][static_cast<std::size_t>(id)];
        const std::string key = Controls::KeyOf(id);
        CHECK_MSG(entry.has_value(), key + " has a generated picture in controls.json");
        if (!entry) continue;
        // The file the layer will write, and the margin the layout will draw its body with.
        CHECK_MSG(entry->file == "modern_" + key + ".png", key + ": the file is " + entry->file);
        // The margin of the generated tier is the solid face's own: a tier change does not move the face (design 1.3).
        CHECK_MSG(entry->bodyInset == rules.faceInset, key + ": body_inset " + std::to_string(entry->bodyInset) + ", face_inset " + std::to_string(rules.faceInset));
        // And what PadArt draws with it keeps that margin to the pixel: 14 of 128.
        const Bitmap bitmap = PadArt::Draw(PadArt::GlyphOf(id), 128, entry->bodyInset);
        CHECK_MSG(!bitmap.Empty(), key);
        CHECK_MSG(BoxOf(bitmap, [](const Pixel& p) { return p.a >= 250; }) == BodyBox(128, 14), key + ": the body's box at 128 px");
    }
    // The number itself, written out: 14 of 128. (The data may say it another way, but not mean another thing.)
    const auto& left = rules.art[generated][static_cast<std::size_t>(Controls::Id::Left)];
    CHECK(left.has_value() && left->bodyInset == 0.109375);
    // What the loader lets a generated entry carry is what this draws: one number (the layout's, 0.25), so that a value in controls.json that
    // loads is a value that draws, and the largest of them draws a body (the picture is never asked for nothing and given nothing).
    CHECK(PadArt::kMaxBodyInset == Controls::kMaxGeneratedBodyInset && Controls::kMaxGeneratedBodyInset == 0.25);
    for (const Controls::Id id : Controls::kClaimOrder) {
        const Bitmap largest = PadArt::Draw(PadArt::GlyphOf(id), 128, Controls::kMaxGeneratedBodyInset);
        CHECK_MSG(!largest.Empty() && BoxOf(largest, [](const Pixel& p) { return p.a >= 250; }) == BodyBox(128, 32), std::string(Controls::KeyOf(id)) + ": the largest margin the loader allows draws, 32 px of 128 on each side");
        CHECK_MSG(PadArt::Draw(PadArt::GlyphOf(id), 128, Controls::kMaxGeneratedBodyInset + 0.001).Empty(), std::string(Controls::KeyOf(id)) + ": a margin the loader refuses is not drawn");
    }
}

void runTests() {
    AGlyphIsDeterministic();
    TheTwoArrowsAreMirrorImages();
    TheArrowsPointWhereTheirNamesSay();
    EachControlHasItsOwnGlyph();
    ABodyKeepsTheStatedMarginAndNoOtherAlpha();
    NoPixelIsExactMagentaAndTransparencyIsBlack();
    TheAlphaIsTheShareOfASamplesThatAreThereAndTheColourIsTheirs();
    EveryGlyphIsAsLargeAsAsked();
    WhatIsRefusedIsRefusedWhole();
    EveryGlyphIsWhereItsNameSays();
    EveryGlyphFillsItsPlateAndSitsInItsMiddle();
    TheGeneratedTierOfControlsJsonIsWhatThePadArtDraws();
}

} // namespace

MP_TEST_MAIN("test_mp_padart", 60)
