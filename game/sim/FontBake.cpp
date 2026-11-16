#include "sim/FontBake.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <random>
#include <sstream>
#include <system_error>

// The C library stb_truetype would include, included first so that its own #includes inside the namespace below are no-ops.
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

// stb_truetype, the copy ImGui vendors (engine/third_party/imgui, on SupersonicCore's public include path), compiled here too: imgui_draw.cpp
// compiles its own with STBTT_STATIC, and so does this file, so every function has internal linkage in both and nothing clashes at link time;
// the namespace keeps even the types apart. The same arrangement as Penumbra Enhanced's game/render/FontAtlas.cpp. Its warnings are the
// library's, not ours.
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
namespace MagicPortals::FontBake::Stbtt {
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "imstb_truetype.h"
} // namespace MagicPortals::FontBake::Stbtt
#ifdef _MSC_VER
#pragma warning(pop)
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

// Declarations only: the encoder's implementation is the engine's (renderer/ScreenCapture.cpp), as for the layer's generated pictures.
#include "stb_image_write.h"

namespace MagicPortals::FontBake {

namespace {

using namespace Stbtt;

// Bumped whenever the bake's output for the same inputs changes, so a cache from the older algorithm is not mistaken for current.
constexpr const char* kAlgorithm = "mp-fontbake-1";
// The same for the icons' rasteriser (planning step 249): part of the hash of a slot that carries icons, and of no other, so Matura's atlases keep their names.
constexpr const char* kIconAlgorithm = "mp-fontbake-icons-1";

constexpr std::uint32_t kSpace = 0x20;
constexpr std::uint32_t kNoBreakSpace = 0xA0;
constexpr int kMaxCellSide = 4096; // a sanity bound on a glyph's cell, not a design number: nothing real approaches it
constexpr double kKernelSigmas = 3.0; // the Gaussian is cut where it is 1% of its peak

// The eight bytes every PNG begins with (the PNG specification's signature). Ensure reads them to tell a page that was written whole from the empty or
// torn file a phone leaves when it dies between the rename and the data reaching storage.
constexpr std::size_t kPngSignatureSize = 8;
constexpr std::array<unsigned char, kPngSignatureSize> kPngSignature = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};

bool IsAsciiPunctuationOrDigit(std::uint32_t c) {
    return (c >= '0' && c <= '9') || (c >= 0x21 && c <= 0x2F) || (c >= 0x3A && c <= 0x40) || (c >= 0x5B && c <= 0x60) || (c >= 0x7B && c <= 0x7E);
}

// The Latin letters too: a language's table falls back to ENGLISH for a key it lacks, the credits and the brand are English whatever the language, and a
// draw that does not go through Strings::SlotCodePoints must still find its letters. Fifty-two glyphs a slot buys that.
bool IsAsciiLetter(std::uint32_t c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

// ---- hashing: two FNV-1a lanes, a cache key and not a security measure ----------------------------------------------------------------

struct Hasher {
    std::uint64_t a = 0xcbf29ce484222325ULL;
    std::uint64_t b = 0x84222325cbf29ce4ULL;
    void Byte(unsigned char c) {
        a = (a ^ c) * 0x100000001b3ULL;
        b = (b ^ static_cast<unsigned char>(c + 0x5B)) * 0x100000001b3ULL;
    }
    void Bytes(const void* data, std::size_t size) {
        const auto* p = static_cast<const unsigned char*>(data);
        for (std::size_t i = 0; i < size; ++i) Byte(p[i]);
    }
    void Text(std::string_view text) {
        Number(text.size());
        Bytes(text.data(), text.size());
    }
    void Number(std::uint64_t n) {
        for (int i = 0; i < 8; ++i) Byte(static_cast<unsigned char>(n >> (8 * i)));
    }
    void Real(double v) { // by its text: the same on every compiler for the values a table writes
        char buffer[64];
        std::snprintf(buffer, sizeof(buffer), "%.6f", v);
        Text(buffer);
    }
    std::string Hex() const {
        char buffer[40];
        std::snprintf(buffer, sizeof(buffer), "%016llx%016llx", static_cast<unsigned long long>(a), static_cast<unsigned long long>(b));
        return buffer;
    }
};

const FaceBytes* FindFaceBytes(const std::vector<FaceBytes>& faces, std::string_view id) {
    for (const FaceBytes& face : faces) {
        if (face.id == id) return &face;
    }
    return nullptr;
}

// The ids of every face a slot can draw from, text face first, without repeats.
std::vector<std::string> FacesOfSlot(const SlotSpec& slot) {
    std::vector<std::string> ids{slot.textFace};
    for (const auto& [script, face] : slot.scriptFaces) {
        (void)script;
        if (std::find(ids.begin(), ids.end(), face) == ids.end()) ids.push_back(face);
    }
    return ids;
}

// ---- one face, ready to rasterise ---------------------------------------------------------------------------------------------------

struct LoadedFace {
    std::string id;
    stbtt_fontinfo info;
    float scale = 0.0f;
};

// ---- one glyph ---------------------------------------------------------------------------------------------------------------------

struct Glyph {
    std::uint32_t codePoint = 0;
    int cellW = 0; // 0 for a glyph with nothing to draw (a space)
    int cellH = 0;
    int xoffset = 0;
    int yoffset = 0;
    int xadvance = 0;
    std::vector<unsigned char> rgba; // cellW * cellH * 4
    int page = 0;
    int x = 0;
    int y = 0;
};

// A Gaussian centred `offset` px along an axis, over integer taps, summing to 1.
struct Kernel {
    int first = 0;
    std::vector<float> weights;
};

// No blur: the coverage moved by the offset, shared between the two pixels it lands across.
Kernel UnblurredKernel(double offset) {
    Kernel kernel;
    const int lo = static_cast<int>(std::floor(offset));
    const double frac = offset - lo;
    kernel.first = lo;
    kernel.weights = {static_cast<float>(1.0 - frac), static_cast<float>(frac)};
    return kernel;
}

Kernel MakeKernel(double offset, double sigma) {
    if (sigma <= 0.0) return UnblurredKernel(offset);
    Kernel kernel;
    const int lo = static_cast<int>(std::floor(offset - kKernelSigmas * sigma));
    const int hi = static_cast<int>(std::ceil(offset + kKernelSigmas * sigma));
    kernel.first = lo;
    double sum = 0.0;
    std::vector<double> raw;
    for (int i = lo; i <= hi; ++i) {
        const double d = i - offset;
        raw.push_back(std::exp(-(d * d) / (2.0 * sigma * sigma)));
        sum += raw.back();
    }
    // A blur this narrow, centred between two pixels (sigma 0.01 at a half-pixel offset), underflows every tap to 0: dividing by that sum would make NaN of the
    // whole shadow and, through Compose, of the glyph with it, and the font would be blank without an error. A blur too small to reach a second pixel is no
    // blur, so it is the unblurred kernel, which has one definition of "no blur" and sums to 1.
    if (!(sum > 0.0) || !std::isfinite(sum)) return UnblurredKernel(offset);
    for (const double w : raw) kernel.weights.push_back(static_cast<float>(w / sum));
    return kernel;
}

// out(x, y) = sum_k weights[k] * in(x - (first + k), y) (horizontal) or in(x, y - (first + k)) (vertical): the input moved right/down by the
// kernel's centre and blurred.
void ConvolveX(const std::vector<float>& in, std::vector<float>& out, int w, int h, const Kernel& kernel) {
    out.assign(in.size(), 0.0f);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float sum = 0.0f;
            for (std::size_t k = 0; k < kernel.weights.size(); ++k) {
                const int sx = x - (kernel.first + static_cast<int>(k));
                if (sx >= 0 && sx < w) sum += kernel.weights[k] * in[static_cast<std::size_t>(y) * w + sx];
            }
            out[static_cast<std::size_t>(y) * w + x] = sum;
        }
    }
}

void ConvolveY(const std::vector<float>& in, std::vector<float>& out, int w, int h, const Kernel& kernel) {
    out.assign(in.size(), 0.0f);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float sum = 0.0f;
            for (std::size_t k = 0; k < kernel.weights.size(); ++k) {
                const int sy = y - (kernel.first + static_cast<int>(k));
                if (sy >= 0 && sy < h) sum += kernel.weights[k] * in[static_cast<std::size_t>(sy) * w + x];
            }
            out[static_cast<std::size_t>(y) * w + x] = sum;
        }
    }
}

// The coverage thickened: each pixel moves `amount` of the way to the largest of its eight neighbours and itself. A light face standing in
// for a heavy one; 0 leaves the coverage alone.
void Embolden(std::vector<float>& coverage, int w, int h, double amount) {
    if (amount <= 0.0) return;
    const std::vector<float> in = coverage;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float widest = in[static_cast<std::size_t>(y) * w + x];
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    const int sx = x + dx;
                    const int sy = y + dy;
                    if (sx >= 0 && sx < w && sy >= 0 && sy < h) widest = std::max(widest, in[static_cast<std::size_t>(sy) * w + sx]);
                }
            }
            const float here = in[static_cast<std::size_t>(y) * w + x];
            coverage[static_cast<std::size_t>(y) * w + x] = here + static_cast<float>(amount) * (widest - here);
        }
    }
}

unsigned char ToByte(double v) {
    return static_cast<unsigned char>(std::clamp(std::lround(v * 255.0), 0L, 255L));
}

// The glyph drawn in white over its own soft black shadow, as straight-alpha RGBA: colour is the glyph's share of the covered pixel.
void Compose(const std::vector<float>& coverage, const std::vector<float>& shadow, double shadowAlpha, std::vector<unsigned char>& rgba) {
    rgba.assign(coverage.size() * 4, 0);
    for (std::size_t i = 0; i < coverage.size(); ++i) {
        const double g = std::clamp<double>(coverage[i], 0.0, 1.0);
        const double s = std::clamp<double>(shadow[i] * shadowAlpha, 0.0, 1.0);
        const double a = g + s * (1.0 - g);
        const unsigned char alpha = ToByte(a);
        // Rounded alpha, not a, decides whether there is colour: a pixel with no alpha has no colour, so a texel the filter blends in
        // is never tinted by one the encoder dropped.
        const unsigned char white = alpha == 0 ? 0 : ToByte(g / a);
        rgba[i * 4 + 0] = white;
        rgba[i * 4 + 1] = white;
        rgba[i * 4 + 2] = white;
        rgba[i * 4 + 3] = alpha;
    }
}

std::string HexOf(std::uint32_t v); // below

// The cell a bw x bh bitmap needs (the bitmap, its thickening margin on every side, the slot's padding), and whether a page can hold it: the check that comes before the
// bitmap is made, so a glyph of a wrong size is an error and not an allocation.
bool CellFits(const SlotSpec& slot, int bw, int bh, int margin, const std::string& what, std::uint32_t codePoint, int& cellW, int& cellH, std::string& error) {
    cellW = bw + 2 * margin + slot.padLeft + slot.padRight;
    cellH = bh + 2 * margin + slot.padUp + slot.padDown;
    if (cellW > slot.pageSize || cellH > slot.pageSize || cellW > kMaxCellSide || cellH > kMaxCellSide) {
        error = "the glyph U+" + HexOf(codePoint) + " of '" + what + "' is " + std::to_string(cellW) + " x " + std::to_string(cellH) + ", which does not fit a " +
                std::to_string(slot.pageSize) + " px page";
        return false;
    }
    return true;
}

// The cell of a glyph whose coverage is `bitmap` (bw x bh, row 0 at the top, values 0..1), the bitmap's top-left corner standing (ix0, iy0) from the pen on the baseline
// (iy0 is negative above it): the slot's padding round it, the thickening, the soft shadow and the composition, which a letter and an icon share. `what` names the face or the
// icon for the error. `glyph.codePoint` is set; false, with `error`, when the cell does not fit a page.
bool MakeCell(const SlotSpec& slot, const std::vector<float>& bitmap, int bw, int bh, int ix0, int iy0, int margin, const Kernel& kernelX, const Kernel& kernelY,
              const std::string& what, Glyph& glyph, std::string& error) {
    if (!CellFits(slot, bw, bh, margin, what, glyph.codePoint, glyph.cellW, glyph.cellH, error)) return false;
    // The coverage on the cell's canvas, the glyph where the padding leaves it.
    std::vector<float> coverage(static_cast<std::size_t>(glyph.cellW) * glyph.cellH, 0.0f);
    for (int y = 0; y < bh; ++y) {
        for (int x = 0; x < bw; ++x) {
            coverage[static_cast<std::size_t>(y + margin + slot.padUp) * glyph.cellW + (x + margin + slot.padLeft)] = bitmap[static_cast<std::size_t>(y) * bw + x];
        }
    }
    Embolden(coverage, glyph.cellW, glyph.cellH, slot.embolden);
    std::vector<float> across;
    std::vector<float> shadow;
    ConvolveX(coverage, across, glyph.cellW, glyph.cellH, kernelX);
    ConvolveY(across, shadow, glyph.cellW, glyph.cellH, kernelY);
    Compose(coverage, shadow, slot.shadow.alpha, glyph.rgba);
    glyph.xoffset = ix0 - margin - slot.padLeft;
    glyph.yoffset = slot.base + iy0 - margin - slot.padUp;
    return true;
}

// ---- icons: vector shapes, rasterised by supersampling --------------------------------------------------------------------------------------------

constexpr int kIconSamples = 8; // sub-samples a side in each pixel: an edge's coverage is known to 1/64
constexpr std::uint32_t kPrivateUseFirst = 0xE000;
constexpr std::uint32_t kPrivateUseLast = 0xF8FF;
constexpr double kMaxIconNumber = 4.0; // a sanity bound on a shape number, in cap heights: a typo, not a design

double SquaredDistanceToSegment(double px, double py, double ax, double ay, double bx, double by) {
    const double dx = bx - ax;
    const double dy = by - ay;
    const double length2 = dx * dx + dy * dy;
    double t = length2 > 0.0 ? ((px - ax) * dx + (py - ay) * dy) / length2 : 0.0;
    t = std::clamp(t, 0.0, 1.0);
    const double cx = ax + t * dx - px;
    const double cy = ay + t * dy - py;
    return cx * cx + cy * cy;
}

// The shapes, in cap heights: x from the ink's left edge, y up from the baseline (the capital's top is 1).
struct CheckShape {
    double width, stroke, kneeX, kneeY, leftY;
};

struct PadlockShape {
    double width, bodyHeight, corner, stroke, inset, hole, holeY, slot, slotBottom;
};

bool InsideCheck(const CheckShape& s, double x, double y) {
    const double r = s.stroke * 0.5;
    const double r2 = r * r;
    return SquaredDistanceToSegment(x, y, r, s.leftY, s.kneeX, s.kneeY) <= r2 || SquaredDistanceToSegment(x, y, s.kneeX, s.kneeY, s.width - r, 1.0 - r) <= r2;
}

bool InsidePadlock(const PadlockShape& s, double x, double y) {
    const double halfWidth = s.width * 0.5;
    const double dx = x - halfWidth;
    // The body: a rounded rectangle (the usual distance to one), with the keyhole cut out of it - a disc and a slot under it.
    const double qx = std::fabs(dx) - (halfWidth - s.corner);
    const double qy = std::fabs(y - s.bodyHeight * 0.5) - (s.bodyHeight * 0.5 - s.corner);
    const double ox = std::max(qx, 0.0);
    const double oy = std::max(qy, 0.0);
    if (std::sqrt(ox * ox + oy * oy) + std::min(std::max(qx, qy), 0.0) - s.corner <= 0.0) {
        const double hy = y - s.holeY;
        if (dx * dx + hy * hy <= s.hole * s.hole) return false;
        if (std::fabs(dx) <= s.slot && y >= s.slotBottom && y <= s.holeY) return false;
        return true;
    }
    // The shackle: an arch whose outer top is the capital's height, its two legs standing on the body.
    const double outer = halfWidth - s.inset;
    const double inner = outer - s.stroke;
    const double centreY = 1.0 - outer;
    if (y >= centreY) {
        const double r2 = dx * dx + (y - centreY) * (y - centreY);
        return r2 <= outer * outer && r2 >= inner * inner;
    }
    return y >= s.bodyHeight && std::fabs(dx) >= inner && std::fabs(dx) <= outer;
}

// The ink's size in pixels for a slot whose capital is `cap` px: the shape's width and the capital's height, each rounded up to a whole pixel.
int IconBitmapWidth(const IconSpec& icon, double cap) {
    return std::max(1, static_cast<int>(std::ceil(icon.Value("width") * cap - 1e-9)));
}

int IconBitmapHeight(double cap) {
    return std::max(1, static_cast<int>(std::ceil(cap - 1e-9)));
}

// The icon's coverage, bw x bh, row 0 at the top: for each pixel the share of kIconSamples^2 sub-samples that lie inside the shape.
void RasteriseIcon(const IconSpec& icon, double cap, int bw, int bh, std::vector<float>& coverage) {
    coverage.assign(static_cast<std::size_t>(bw) * bh, 0.0f);
    CheckShape check{};
    PadlockShape lock{};
    if (icon.kind == IconKind::Check) {
        check = {icon.Value("width"), icon.Value("stroke"), icon.Value("knee_x"), icon.Value("knee_y"), icon.Value("left_y")};
    } else {
        lock = {icon.Value("width"), icon.Value("body_height"), icon.Value("corner"), icon.Value("stroke"), icon.Value("inset"),
                icon.Value("hole"),  icon.Value("hole_y"),      icon.Value("slot"),   icon.Value("slot_bottom")};
    }
    for (int iy = 0; iy < bh; ++iy) {
        for (int ix = 0; ix < bw; ++ix) {
            int inside = 0;
            for (int sy = 0; sy < kIconSamples; ++sy) {
                for (int sx = 0; sx < kIconSamples; ++sx) {
                    const double x = (ix + (sx + 0.5) / kIconSamples) / cap;
                    const double y = (bh - (iy + (sy + 0.5) / kIconSamples)) / cap;
                    inside += (icon.kind == IconKind::Check ? InsideCheck(check, x, y) : InsidePadlock(lock, x, y)) ? 1 : 0;
                }
            }
            coverage[static_cast<std::size_t>(iy) * bw + ix] = static_cast<float>(inside) / static_cast<float>(kIconSamples * kIconSamples);
        }
    }
}

// Whether `slot` carries `icon`.
bool SlotCarries(const IconSpec& icon, const SlotSpec& slot) {
    return std::find(icon.slots.begin(), icon.slots.end(), slot.id) != icon.slots.end();
}

void AppendPng(void* context, void* data, int size) {
    static_cast<std::string*>(context)->append(static_cast<const char*>(data), static_cast<std::size_t>(size));
}

bool EncodePng(const std::vector<unsigned char>& rgba, int w, int h, std::string& bytes) {
    bytes.clear();
    const int ok = stbi_write_png_to_func(AppendPng, &bytes, w, h, 4, rgba.data(), w * 4);
    return ok != 0 && !bytes.empty();
}

std::string HexOf(std::uint32_t v) {
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%X", static_cast<unsigned>(v));
    return buffer;
}

std::string PageName(std::string_view name, std::size_t page) {
    return std::string(name) + "_" + std::to_string(page) + ".png";
}

// ---- files -------------------------------------------------------------------------------------------------------------------------

bool ReadWhole(const std::filesystem::path& path, std::string& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return true;
}

bool FileHolds(const std::filesystem::path& path, const std::string& bytes) {
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec) || std::filesystem::file_size(path, ec) != bytes.size()) return false;
    std::string held;
    return ReadWhole(path, held) && held == bytes;
}

// Whether a cached page can be trusted without baking again: a regular file that holds at least the PNG signature and begins with it. Merely being a
// file is not enough: WriteWhole only flushes, so a phone that died between the rename and the data reaching storage leaves an empty page (or a cut one)
// under a descriptor with the right hash, and that language's text would be blank on every launch. The signature is what a cheap check can see: a page
// cut somewhere after it still passes.
bool PageLooksWhole(const std::filesystem::path& path) {
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec) || ec) return false;
    const std::uintmax_t size = std::filesystem::file_size(path, ec);
    if (ec || size < kPngSignatureSize) return false;
    std::ifstream in(path, std::ios::binary);
    std::array<unsigned char, kPngSignatureSize> head{};
    in.read(reinterpret_cast<char*>(head.data()), static_cast<std::streamsize>(head.size()));
    return static_cast<std::size_t>(in.gcount()) == head.size() && head == kPngSignature;
}

std::string UniqueSuffix() {
    static std::atomic<unsigned> counter{0};
    std::random_device device;
    return std::to_string(device()) + "-" + std::to_string(counter.fetch_add(1));
}

// Puts `bytes` at `target` without ever showing anyone a half-written file, and writes nothing for a file that already is these bytes
// (MagicPortalsLayer's WriteWhole, the precedent). True when `target` holds the bytes afterwards, by whoever put them there.
bool WriteWhole(const std::filesystem::path& target, const std::string& bytes, bool& wrote) {
    wrote = false;
    if (FileHolds(target, bytes)) return true;
    std::filesystem::path temporary = target;
    temporary += "." + UniqueSuffix() + ".tmp";
    {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        out.flush();
        if (!out) {
            out.close();
            std::error_code ec;
            std::filesystem::remove(temporary, ec);
            return false;
        }
    }
    std::error_code ec;
    std::filesystem::rename(temporary, target, ec);
    if (!ec) {
        wrote = true;
        return true;
    }
    std::filesystem::remove(temporary, ec);
    return FileHolds(target, bytes);
}

} // namespace

// ---- the plan ------------------------------------------------------------------------------------------------------------------------

const FaceSpec* Plan::Face(std::string_view id) const {
    for (const FaceSpec& face : faces) {
        if (face.id == id) return &face;
    }
    return nullptr;
}

const SlotSpec* Plan::Slot(std::string_view id) const {
    for (const SlotSpec& slot : slots) {
        if (slot.id == id) return &slot;
    }
    return nullptr;
}

const IconSpec* Plan::IconAt(std::uint32_t codePoint) const {
    for (const IconSpec& icon : icons) {
        if (icon.codePoint == codePoint) return &icon;
    }
    return nullptr;
}

const IconSpec* Plan::IconStandingFor(std::string_view original) const {
    for (const IconSpec& icon : icons) {
        if (icon.standsFor == original) return &icon;
    }
    return nullptr;
}

std::vector<const IconSpec*> Plan::IconsOf(const SlotSpec& slot) const {
    std::vector<const IconSpec*> carried;
    for (const IconSpec& icon : icons) {
        if (SlotCarries(icon, slot)) carried.push_back(&icon);
    }
    return carried;
}

const std::vector<std::string_view>& IconShapeNames(IconKind kind) {
    static const std::vector<std::string_view> check = {"width", "stroke", "knee_x", "knee_y", "left_y"};
    static const std::vector<std::string_view> padlock = {"width", "body_height", "corner", "stroke", "inset", "hole", "hole_y", "slot", "slot_bottom"};
    return kind == IconKind::Check ? check : padlock;
}

std::string_view IconKindName(IconKind kind) {
    return kind == IconKind::Check ? "check" : "padlock";
}

bool ParseIconKind(std::string_view name, IconKind& out) {
    if (name == "check") {
        out = IconKind::Check;
        return true;
    }
    if (name == "padlock") {
        out = IconKind::Padlock;
        return true;
    }
    return false;
}

double IconSpec::Value(std::string_view name) const {
    const std::vector<std::string_view>& names = IconShapeNames(kind);
    for (std::size_t i = 0; i < names.size() && i < shape.size(); ++i) {
        if (names[i] == name) return shape[i];
    }
    return 0.0;
}

bool CheckIcon(const Plan& plan, const IconSpec& icon, std::string& why) {
    const std::string at = "the icon '" + icon.id + "'";
    const std::vector<std::string_view>& names = IconShapeNames(icon.kind);
    if (icon.shape.size() != names.size()) {
        why = at + ": a " + std::string(IconKindName(icon.kind)) + " has " + std::to_string(names.size()) + " shape numbers";
        return false;
    }
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (!std::isfinite(icon.shape[i]) || icon.shape[i] < 0.0 || icon.shape[i] > kMaxIconNumber) {
            why = at + ": the shape number '" + std::string(names[i]) + "' is not from 0 to " + std::to_string(static_cast<int>(kMaxIconNumber)) + " cap heights";
            return false;
        }
    }
    const auto v = [&icon](std::string_view name) { return icon.Value(name); };
    if (icon.kind == IconKind::Check) {
        const double width = v("width");
        const double stroke = v("stroke");
        const double half = stroke * 0.5;
        if (!(stroke > 0.0) || !(width > stroke)) {
            why = at + ": a check mark's stroke is above zero and below its width";
            return false;
        }
        if (v("knee_x") < half || v("knee_x") > width - half || v("knee_y") < half || v("knee_y") > 1.0 - half || v("left_y") < half || v("left_y") > 1.0 - half) {
            why = at + ": a check mark's left end, knee and right end lie inside its box (knee_x, knee_y and left_y are at least half a stroke from the edges)";
            return false;
        }
    } else {
        const double width = v("width");
        const double body = v("body_height");
        const double corner = v("corner");
        const double outer = width * 0.5 - v("inset");
        const double inner = outer - v("stroke");
        if (!(width > 0.0) || !(body > 0.0) || body >= 1.0 || corner > std::min(width, body) * 0.5) {
            why = at + ": a padlock's body is below the capital's height and its corner at most half of its smaller side";
            return false;
        }
        if (!(v("stroke") > 0.0) || !(inner > 0.0) || 1.0 - outer < body) {
            why = at + ": a padlock's arch has a hole and stands on its body (inset and stroke leave an inner radius above zero, and the arch's centre is not below the body's top)";
            return false;
        }
        if (!(v("hole") > 0.0) || v("hole_y") - v("hole") < 0.0 || v("hole_y") + v("hole") > body || v("slot") > v("hole") || v("slot_bottom") > v("hole_y")) {
            why = at + ": a padlock's keyhole (a disc and the slot under it) lies inside its body";
            return false;
        }
    }
    if (!(icon.advance > 0.0) || icon.bearing < 0.0 || icon.advance < icon.bearing + v("width") || icon.advance > kMaxIconNumber) {
        why = at + ": the advance holds the bearing and the ink's width, in cap heights";
        return false;
    }
    if (icon.codePoint < kPrivateUseFirst || icon.codePoint > kPrivateUseLast) {
        why = at + ": the code point is in the private-use area U+E000-F8FF, where no text and no face has a glyph";
        return false;
    }
    for (const ScriptSpec& script : plan.scripts) {
        for (const CodeRange& range : script.ranges) {
            if (icon.codePoint >= range.first && icon.codePoint <= range.last) {
                why = at + ": the code point is in the script '" + script.id + "', which routes it to a face";
                return false;
            }
        }
    }
    if (icon.standsFor.size() != 1 || icon.standsFor[0] <= ' ' || icon.standsFor[0] >= 0x7F || IsAsciiLetter(static_cast<unsigned char>(icon.standsFor[0])) ||
        (icon.standsFor[0] >= '0' && icon.standsFor[0] <= '9')) {
        why = at + ": 'stands_for' is one printable ASCII character that is no letter or digit (the original's glyph code)";
        return false;
    }
    if (icon.slots.empty()) {
        why = at + ": 'slots' names at least one slot";
        return false;
    }
    for (const std::string& slot : icon.slots) {
        if (plan.Slot(slot) == nullptr) {
            why = at + ": 'slots' names '" + slot + "', which is no slot of the plan";
            return false;
        }
    }
    for (const IconSpec& other : plan.icons) {
        if (&other != &icon && (other.id == icon.id || other.codePoint == icon.codePoint || other.standsFor == icon.standsFor)) {
            why = at + ": its id, code point and 'stands_for' are each used by one icon";
            return false;
        }
    }
    return true;
}

bool ParseCodePoint(std::string_view text, std::uint32_t& out) {
    if (text.size() < 6 || text.size() > 8 || text[0] != 'U' || text[1] != '+') return false;
    std::uint32_t value = 0;
    for (std::size_t i = 2; i < text.size(); ++i) {
        const char c = text[i];
        int digit = -1;
        if (c >= '0' && c <= '9') digit = c - '0';
        else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
        else return false; // upper case only: one spelling
        value = value * 16 + static_cast<std::uint32_t>(digit);
    }
    if (value > 0x10FFFF) return false;
    out = value;
    return true;
}

bool ParseRange(std::string_view text, CodeRange& out) {
    // "3000-30FF": two hex numbers of four to six digits, upper case.
    const std::size_t dash = text.find('-');
    if (dash == std::string_view::npos) return false;
    std::uint32_t first = 0;
    std::uint32_t last = 0;
    const std::string a = "U+" + std::string(text.substr(0, dash));
    const std::string b = "U+" + std::string(text.substr(dash + 1));
    if (!ParseCodePoint(a, first) || !ParseCodePoint(b, last) || first > last) return false;
    out.first = first;
    out.last = last;
    return true;
}

const std::string& FaceIdOf(const Plan& plan, const SlotSpec& slot, std::uint32_t codePoint) {
    for (const ScriptSpec& script : plan.scripts) {
        for (const CodeRange& range : script.ranges) {
            if (codePoint < range.first || codePoint > range.last) continue;
            for (const auto& [id, face] : slot.scriptFaces) {
                if (id == script.id) return face;
            }
        }
    }
    return slot.textFace;
}

std::set<std::uint32_t> BaseSet() {
    std::set<std::uint32_t> base{kSpace, kNoBreakSpace};
    for (std::uint32_t c = 0x21; c <= 0x7E; ++c) {
        if (IsAsciiPunctuationOrDigit(c) || IsAsciiLetter(c)) base.insert(c);
    }
    return base;
}

std::set<std::uint32_t> WantedPoints(const Plan& plan, const SlotSpec& slot, const std::set<std::uint32_t>& codePoints) {
    std::set<std::uint32_t> all = codePoints;
    const std::set<std::uint32_t> base = BaseSet();
    all.insert(base.begin(), base.end());
    for (const IconSpec* icon : plan.IconsOf(slot)) all.insert(icon->codePoint);
    return all;
}

bool LoadFaces(const Plan& plan, const std::string& dir, std::vector<FaceBytes>& out, std::string& error) {
    std::vector<FaceBytes> read;
    for (const FaceSpec& face : plan.faces) {
        const std::filesystem::path path = std::filesystem::path(dir) / face.file;
        std::string bytes;
        if (!ReadWhole(path, bytes) || bytes.empty()) {
            error = path.generic_string() + ": cannot be read";
            return false;
        }
        FaceBytes item;
        item.id = face.id;
        item.data.assign(bytes.begin(), bytes.end());
        read.push_back(std::move(item));
    }
    out = std::move(read);
    return true;
}

// ---- the hash ---------------------------------------------------------------------------------------------------------------------------

std::string InputHash(const Plan& plan, const SlotSpec& slot, const std::vector<FaceBytes>& faces, const std::set<std::uint32_t>& codePoints,
                      std::string_view name) {
    Hasher h;
    h.Text(kAlgorithm);
    h.Text(name);
    h.Text(slot.id);
    h.Text(slot.originalFont);
    for (const int n : {slot.lineHeight, slot.base, slot.padUp, slot.padRight, slot.padDown, slot.padLeft, slot.spacingX, slot.spacingY, slot.pageSize}) {
        h.Number(static_cast<std::uint64_t>(static_cast<std::int64_t>(n)));
    }
    for (const double v : {slot.capHeight, slot.shadow.dx, slot.shadow.dy, slot.shadow.sigma, slot.shadow.alpha, slot.embolden}) h.Real(v);
    for (const std::string& id : FacesOfSlot(slot)) {
        h.Text(id);
        if (const FaceSpec* spec = plan.Face(id)) {
            h.Text(spec->file);
            h.Number(spec->reference);
            h.Real(spec->referenceHeight);
        }
        if (const FaceBytes* bytes = FindFaceBytes(faces, id)) {
            h.Number(bytes->data.size());
            h.Bytes(bytes->data.data(), bytes->data.size());
        }
    }
    // The routing: which script's ranges go to which face, as the slot sees it.
    for (const ScriptSpec& script : plan.scripts) {
        for (const auto& [id, face] : slot.scriptFaces) {
            if (id != script.id) continue;
            h.Text(id);
            h.Text(face);
            for (const CodeRange& range : script.ranges) {
                h.Number(range.first);
                h.Number(range.last);
            }
        }
    }
    // The icons the slot carries and the way they are drawn: only in a slot that has any, so another slot's hash is what it was before the icons.
    const std::vector<const IconSpec*> icons = plan.IconsOf(slot);
    if (!icons.empty()) {
        h.Text(kIconAlgorithm);
        h.Number(icons.size());
        for (const IconSpec* icon : icons) {
            h.Text(icon->id);
            h.Text(IconKindName(icon->kind));
            h.Number(icon->codePoint);
            h.Real(icon->advance);
            h.Real(icon->bearing);
            h.Number(icon->shape.size());
            for (const double value : icon->shape) h.Real(value);
        }
    }
    const std::set<std::uint32_t> all = WantedPoints(plan, slot, codePoints);
    h.Number(all.size());
    for (const std::uint32_t c : all) h.Number(c);
    return h.Hex();
}

// ---- the bake -----------------------------------------------------------------------------------------------------------------------------

bool Bake(const Plan& plan, const SlotSpec& slot, const std::vector<FaceBytes>& faces, const std::set<std::uint32_t>& codePoints, std::string_view name,
          Baked& out, std::string& error) {
    if (slot.lineHeight <= 0 || slot.capHeight <= 0.0 || slot.pageSize <= 0) {
        error = "slot '" + slot.id + "' has no line height, cap height or page size";
        return false;
    }
    // Each face of the slot, sized so that its reference glyph is as tall as the slot says.
    std::vector<LoadedFace> loaded;
    loaded.reserve(FacesOfSlot(slot).size());
    for (const std::string& id : FacesOfSlot(slot)) {
        const FaceSpec* spec = plan.Face(id);
        const FaceBytes* bytes = FindFaceBytes(faces, id);
        if (spec == nullptr) {
            error = "slot '" + slot.id + "' names the face '" + id + "', which the plan does not define";
            return false;
        }
        if (bytes == nullptr || bytes->data.empty()) {
            error = "the face '" + id + "' (" + spec->file + ") was not given";
            return false;
        }
        LoadedFace face;
        face.id = id;
        // GetFontOffsetForIndex says -1 for bytes that are no font at all: InitFont is not offered those. (stb_truetype trusts a font's tables, as
        // it says; the faces here are the committed ones, whose hashes the README records.)
        const int offset = stbtt_GetFontOffsetForIndex(bytes->data.data(), 0);
        if (offset < 0 || stbtt_InitFont(&face.info, bytes->data.data(), offset) == 0) {
            error = spec->file + ": not a TrueType font";
            return false;
        }
        int x0 = 0;
        int y0 = 0;
        int x1 = 0;
        int y1 = 0;
        // A code point the face lacks has the .notdef glyph's box, a plausible one: the face must really hold the reference glyph.
        if (stbtt_FindGlyphIndex(&face.info, static_cast<int>(spec->reference)) == 0 ||
            stbtt_GetCodepointBox(&face.info, static_cast<int>(spec->reference), &x0, &y0, &x1, &y1) == 0 || y1 <= 0) {
            error = spec->file + ": has no glyph to measure its size by (U+" + HexOf(spec->reference) + ")";
            return false;
        }
        face.scale = static_cast<float>(slot.capHeight * spec->referenceHeight / static_cast<double>(y1));
        loaded.push_back(std::move(face));
    }
    const auto faceNamed = [&loaded](const std::string& id) -> LoadedFace* {
        for (LoadedFace& face : loaded) {
            if (face.id == id) return &face;
        }
        return nullptr;
    };

    const std::set<std::uint32_t> wanted = WantedPoints(plan, slot, codePoints);

    const int margin = slot.embolden > 0.0 ? 1 : 0;
    const Kernel kernelX = MakeKernel(slot.shadow.dx, slot.shadow.sigma);
    const Kernel kernelY = MakeKernel(slot.shadow.dy, slot.shadow.sigma);

    std::vector<Glyph> glyphs;
    std::vector<std::uint32_t> missing;
    for (const std::uint32_t codePoint : wanted) {
        // An icon the slot carries is drawn from its shape, and never looked up in a face (it is no letter, and would be reported missing).
        if (const IconSpec* icon = plan.IconAt(codePoint); icon != nullptr && SlotCarries(*icon, slot)) {
            const double cap = slot.capHeight;
            const int bw = IconBitmapWidth(*icon, cap);
            const int bh = IconBitmapHeight(cap);
            std::vector<float> bitmap;
            RasteriseIcon(*icon, cap, bw, bh, bitmap);
            Glyph made;
            made.codePoint = codePoint;
            made.xadvance = static_cast<int>(std::floor(icon->advance * cap));
            // The ink stands on the baseline: its top is bh above it, its left edge the bearing right of the pen.
            if (!MakeCell(slot, bitmap, bw, bh, static_cast<int>(std::lround(icon->bearing * cap)), -bh, margin, kernelX, kernelY, icon->id, made, error)) return false;
            glyphs.push_back(std::move(made));
            continue;
        }
        LoadedFace* face = faceNamed(FaceIdOf(plan, slot, codePoint));
        const int glyphIndex = face == nullptr ? 0 : stbtt_FindGlyphIndex(&face->info, static_cast<int>(codePoint));
        if (glyphIndex == 0) {
            missing.push_back(codePoint);
            continue;
        }
        Glyph glyph;
        glyph.codePoint = codePoint;
        int advance = 0;
        int bearing = 0;
        stbtt_GetGlyphHMetrics(&face->info, glyphIndex, &advance, &bearing);
        glyph.xadvance = static_cast<int>(std::floor(static_cast<double>(advance) * face->scale));

        int ix0 = 0;
        int iy0 = 0;
        int ix1 = 0;
        int iy1 = 0;
        stbtt_GetGlyphBitmapBox(&face->info, glyphIndex, face->scale, face->scale, &ix0, &iy0, &ix1, &iy1);
        const int bw = ix1 - ix0;
        const int bh = iy1 - iy0;
        if (bw > 0 && bh > 0) {
            int cellW = 0;
            int cellH = 0;
            if (!CellFits(slot, bw, bh, margin, face->id, codePoint, cellW, cellH, error)) return false;
            std::vector<unsigned char> raster(static_cast<std::size_t>(bw) * bh, 0);
            stbtt_MakeGlyphBitmap(&face->info, raster.data(), bw, bh, bw, face->scale, face->scale, glyphIndex);
            std::vector<float> bitmap(raster.size());
            for (std::size_t i = 0; i < raster.size(); ++i) bitmap[i] = static_cast<float>(raster[i]) / 255.0f;
            if (!MakeCell(slot, bitmap, bw, bh, ix0, iy0, margin, kernelX, kernelY, face->id, glyph, error)) return false;
        }
        glyphs.push_back(std::move(glyph));
    }

    // Shelves, tallest first, ties by code point: a fixed order, so the same inputs make the same atlas.
    std::vector<std::size_t> order;
    for (std::size_t i = 0; i < glyphs.size(); ++i) {
        if (glyphs[i].cellW > 0) order.push_back(i);
    }
    std::sort(order.begin(), order.end(), [&glyphs](std::size_t a, std::size_t b) {
        if (glyphs[a].cellH != glyphs[b].cellH) return glyphs[a].cellH > glyphs[b].cellH;
        return glyphs[a].codePoint < glyphs[b].codePoint;
    });
    int page = 0;
    int penX = 0;
    int penY = 0;
    int shelfH = 0;
    for (const std::size_t i : order) {
        Glyph& glyph = glyphs[i];
        if (penX + glyph.cellW > slot.pageSize) { // a new shelf
            penX = 0;
            penY += shelfH + slot.spacingY;
            shelfH = 0;
        }
        if (penY + glyph.cellH > slot.pageSize) { // a new page
            ++page;
            penX = 0;
            penY = 0;
            shelfH = 0;
        }
        glyph.page = page;
        glyph.x = penX;
        glyph.y = penY;
        penX += glyph.cellW + slot.spacingX;
        shelfH = std::max(shelfH, glyph.cellH);
    }
    const std::size_t pageCount = order.empty() ? 1 : static_cast<std::size_t>(page) + 1;

    const std::size_t pageBytes = static_cast<std::size_t>(slot.pageSize) * slot.pageSize * 4;
    std::vector<std::vector<unsigned char>> sheets(pageCount, std::vector<unsigned char>(pageBytes, 0));
    for (const Glyph& glyph : glyphs) {
        if (glyph.cellW == 0) continue;
        std::vector<unsigned char>& sheet = sheets[static_cast<std::size_t>(glyph.page)];
        for (int y = 0; y < glyph.cellH; ++y) {
            std::copy_n(glyph.rgba.begin() + static_cast<std::ptrdiff_t>(y) * glyph.cellW * 4, static_cast<std::ptrdiff_t>(glyph.cellW) * 4,
                        sheet.begin() + (static_cast<std::ptrdiff_t>(glyph.y + y) * slot.pageSize + glyph.x) * 4);
        }
    }

    Baked baked;
    baked.pages.resize(pageCount);
    for (std::size_t p = 0; p < pageCount; ++p) {
        if (!EncodePng(sheets[p], slot.pageSize, slot.pageSize, baked.pages[p])) {
            error = "page " + std::to_string(p) + " could not be encoded as a PNG";
            return false;
        }
    }
    baked.hash = InputHash(plan, slot, faces, codePoints, name);
    baked.missing = missing;
    baked.glyphs = glyphs.size();

    std::ostringstream fnt;
    fnt << "info face=\"" << name << "\" size=" << slot.lineHeight << " bold=1 italic=0 charset=\"\" unicode=1 stretchH=100 smooth=1 aa=1 padding="
        << slot.padUp << ',' << slot.padRight << ',' << slot.padDown << ',' << slot.padLeft << " spacing=" << slot.spacingX << ',' << slot.spacingY
        << " outline=0\n";
    fnt << "common lineHeight=" << slot.lineHeight << " base=" << slot.base << " scaleW=" << slot.pageSize << " scaleH=" << slot.pageSize
        << " pages=" << pageCount << " packed=0 alphaChnl=0 redChnl=4 greenChnl=4 blueChnl=4\n";
    for (std::size_t p = 0; p < pageCount; ++p) fnt << "page id=" << p << " file=\"" << PageName(name, p) << "\"\n";
    fnt << "chars count=" << glyphs.size() << "\n";
    for (const Glyph& glyph : glyphs) { // `wanted` is a set: ascending
        fnt << "char id=" << glyph.codePoint << " x=" << glyph.x << " y=" << glyph.y << " width=" << glyph.cellW << " height=" << glyph.cellH
            << " xoffset=" << glyph.xoffset << " yoffset=" << glyph.yoffset << " xadvance=" << glyph.xadvance << " page=" << glyph.page << " chnl=15\n";
    }
    // The cache record. The engine's reader takes it for an unknown record and skips it.
    std::string list;
    for (const std::uint32_t c : missing) list += (list.empty() ? "" : ",") + HexOf(c);
    fnt << "bake hash=" << baked.hash << " missing=" << (list.empty() ? "-" : list) << "\n";
    baked.fnt = fnt.str();
    out = std::move(baked);
    return true;
}

// ---- reading a descriptor back ------------------------------------------------------------------------------------------------------------

namespace {

// The value of `key=` on a line, quotes removed; "" when it is not there.
std::string ValueOf(const std::string& line, std::string_view key) {
    const std::string needle = " " + std::string(key) + "=";
    const std::size_t at = line.find(needle);
    if (at == std::string::npos) return {};
    std::size_t i = at + needle.size();
    if (i < line.size() && line[i] == '"') {
        const std::size_t end = line.find('"', i + 1);
        return end == std::string::npos ? std::string() : line.substr(i + 1, end - i - 1);
    }
    const std::size_t end = line.find(' ', i);
    return line.substr(i, end == std::string::npos ? std::string::npos : end - i);
}

} // namespace

bool Describe(std::string_view fnt, Described& out) {
    Described described;
    bool common = false;
    std::istringstream stream{std::string(fnt)};
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.rfind("common ", 0) == 0) {
            common = true;
        } else if (line.rfind("char ", 0) == 0) {
            const std::string id = ValueOf(line, "id");
            if (!id.empty()) described.chars.insert(static_cast<std::uint32_t>(std::strtoul(id.c_str(), nullptr, 10)));
        } else if (line.rfind("page ", 0) == 0) {
            const std::string file = ValueOf(line, "file");
            if (!file.empty()) described.pageFiles.push_back(file);
        } else if (line.rfind("bake ", 0) == 0) {
            described.hash = ValueOf(line, "hash");
            const std::string list = ValueOf(line, "missing");
            if (!list.empty() && list != "-") {
                std::size_t start = 0;
                while (start <= list.size()) {
                    const std::size_t comma = list.find(',', start);
                    const std::string item = list.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
                    if (!item.empty()) described.missing.push_back(static_cast<std::uint32_t>(std::strtoul(item.c_str(), nullptr, 16)));
                    if (comma == std::string::npos) break;
                    start = comma + 1;
                }
            }
        }
    }
    if (!common) return false;
    out = std::move(described);
    return true;
}

// ---- Ensure -------------------------------------------------------------------------------------------------------------------------------

Written Ensure(const Plan& plan, const SlotSpec& slot, const std::vector<FaceBytes>& faces, const std::set<std::uint32_t>& codePoints,
               const std::string& dir, std::string_view name) {
    Written result;
    const std::filesystem::path folder(dir);
    const std::filesystem::path fntPath = folder / (std::string(name) + ".fnt");
    result.fntPath = fntPath.generic_string();
    const std::string hash = InputHash(plan, slot, faces, codePoints, name);

    // Found: the descriptor is there with this hash and every page it names looks whole (PageLooksWhole).
    std::string held;
    Described described;
    if (ReadWhole(fntPath, held) && Describe(held, described) && described.hash == hash && !described.pageFiles.empty()) {
        bool pagesThere = true;
        for (const std::string& file : described.pageFiles) {
            if (!PageLooksWhole(folder / file)) pagesThere = false;
        }
        if (pagesThere) {
            result.ok = true;
            result.cached = true;
            result.missing = described.missing;
            return result;
        }
    }

    Baked baked;
    if (!Bake(plan, slot, faces, codePoints, name, baked, result.error)) return result;
    for (std::size_t p = 0; p < baked.pages.size(); ++p) {
        bool wrote = false;
        if (!WriteWhole(folder / PageName(name, p), baked.pages[p], wrote)) {
            result.error = (folder / PageName(name, p)).generic_string() + ": cannot be written";
            return result;
        }
        if (wrote) ++result.filesWritten;
    }
    bool wrote = false;
    if (!WriteWhole(fntPath, baked.fnt, wrote)) {
        result.error = result.fntPath + ": cannot be written";
        return result;
    }
    if (wrote) ++result.filesWritten;
    result.ok = true;
    result.missing = baked.missing;
    return result;
}

} // namespace MagicPortals::FontBake
