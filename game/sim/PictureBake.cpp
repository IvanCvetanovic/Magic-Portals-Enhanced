#include "sim/PictureBake.hpp"

#include "sim/Utf8.hpp"

#include "core/Json.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <random>
#include <set>
#include <sstream>
#include <system_error>

// The C library stb_truetype would include, included first so that its own #includes inside the namespace below are no-ops.
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

// stb_truetype, as sim/FontBake.cpp compiles it: the copy ImGui vendors (engine/third_party/imgui), STBTT_STATIC so every function has internal linkage here and in
// FontBake.cpp and imgui_draw.cpp and nothing clashes at link time, and a namespace of this file's own so even the types stay apart. Its warnings are the library's.
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
namespace MagicPortals::PictureBake::Stbtt {
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "imstb_truetype.h"
} // namespace MagicPortals::PictureBake::Stbtt
#ifdef _MSC_VER
#pragma warning(pop)
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

// Declarations only: the decoder's and the encoder's implementations are the engine's (renderer/ScreenCapture.cpp and the loader), as for the layer's pictures.
#include "stb_image.h"
#include "stb_image_write.h"

namespace MagicPortals::PictureBake {

namespace {

namespace Json = Supersonic::Json;
using namespace Stbtt;

// Bumped whenever the bake's output for the same inputs changes, so a cache from the older algorithm is not mistaken for current.
constexpr const char* kAlgorithm = "mp-picturebake-1";

constexpr std::uint32_t kSpace = 0x20;
constexpr std::uint32_t kNoBreakSpace = 0xA0;
constexpr std::uint32_t kZeroWidthSpace = 0x200B;
constexpr double kKernelSigmas = 3.0; // the Gaussian is cut where it is 1% of its peak, as FontBake's is
constexpr int kCollar = 3;            // pixels of the picture beyond the rectangle that the erase's guard compares with the model
constexpr unsigned char kHalf = 127;  // a coverage above this is solid, for the pitch and the vertical extent of a line
constexpr int kMaxPitchSteps = 400;
constexpr double kPitchStep = 0.5;    // pixels, as the prototype widened a pair of lines
constexpr std::size_t kPngSignatureSize = 8;
constexpr std::array<unsigned char, kPngSignatureSize> kPngSignature = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
constexpr std::array<unsigned char, 12> kPngEnd = {0x00, 0x00, 0x00, 0x00, 'I', 'E', 'N', 'D', 0xAE, 0x42, 0x60, 0x82};

std::string Fmt(const char* format, ...) {
    char buffer[512];
    va_list args;
    va_start(args, format);
    std::vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    return buffer;
}

std::string HexOf(std::uint32_t v) {
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%04X", static_cast<unsigned>(v));
    return buffer;
}

// Python's round() and numpy's rint(), which the prototype's numbers came from, round a half to the even neighbour; the default rounding mode of nearbyint is that.
int RoundInt(double v) {
    return static_cast<int>(std::nearbyint(v));
}

std::size_t Z(int v) {
    return static_cast<std::size_t>(v);
}

int AlignUp(int v, int step) {
    return ((v + step - 1) / step) * step;
}

// ---- hashing: two FNV-1a lanes, an identity check and a cache key, not a security measure (FontBake's Hasher) -----------------------------------------------

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
    void Number(std::uint64_t n) {
        for (int i = 0; i < 8; ++i) Byte(static_cast<unsigned char>(n >> (8 * i)));
    }
    void Text(std::string_view text) {
        Number(text.size());
        Bytes(text.data(), text.size());
    }
    std::string Hex() const {
        char buffer[40];
        std::snprintf(buffer, sizeof(buffer), "%016llx%016llx", static_cast<unsigned long long>(a), static_cast<unsigned long long>(b));
        return buffer;
    }
};

} // namespace

// ============================================================================================================================
// pictures
// ============================================================================================================================

Image Blank(int width, int height) {
    Image image;
    if (width <= 0 || height <= 0 || width > kMaxSide || height > kMaxSide) return image;
    image.width = width;
    image.height = height;
    image.rgba.assign(Z(width) * Z(height) * 4, 0);
    return image;
}

bool DecodePng(std::string_view bytes, Image& out, std::string& error) {
    int width = 0;
    int height = 0;
    int channels = 0;
    if (bytes.empty() || bytes.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        error = "no picture bytes";
        return false;
    }
    const auto* data = reinterpret_cast<const stbi_uc*>(bytes.data());
    const int size = static_cast<int>(bytes.size());
    if (stbi_info_from_memory(data, size, &width, &height, &channels) == 0) {
        error = std::string("not a picture: ") + (stbi_failure_reason() != nullptr ? stbi_failure_reason() : "unknown");
        return false;
    }
    if (width <= 0 || height <= 0 || width > kMaxSide || height > kMaxSide) {
        error = Fmt("a picture of %d x %d is not one this module reads (the side is at most %d)", width, height, kMaxSide);
        return false;
    }
    stbi_uc* pixels = stbi_load_from_memory(data, size, &width, &height, &channels, 4);
    if (pixels == nullptr) {
        error = std::string("not a picture: ") + (stbi_failure_reason() != nullptr ? stbi_failure_reason() : "unknown");
        return false;
    }
    Image image;
    image.width = width;
    image.height = height;
    image.rgba.assign(pixels, pixels + Z(width) * Z(height) * 4);
    stbi_image_free(pixels);
    out = std::move(image);
    return true;
}

bool ReadImage(const std::string& path, Image& out, std::string& error) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        error = path + ": cannot be read";
        return false;
    }
    const std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::string why;
    if (!DecodePng(bytes, out, why)) {
        error = path + ": " + why;
        return false;
    }
    return true;
}

namespace {

void AppendPng(void* context, void* data, int size) {
    static_cast<std::string*>(context)->append(static_cast<const char*>(data), static_cast<std::size_t>(size));
}

} // namespace

bool EncodePng(const Image& image, std::string& bytes) {
    bytes.clear();
    if (image.Empty()) return false;
    const int ok = stbi_write_png_to_func(AppendPng, &bytes, image.width, image.height, 4, image.rgba.data(), image.width * 4);
    return ok != 0 && !bytes.empty();
}

std::string Digest(const Image& image) {
    Hasher h;
    h.Number(static_cast<std::uint64_t>(image.width));
    h.Number(static_cast<std::uint64_t>(image.height));
    h.Bytes(image.rgba.data(), image.rgba.size());
    return h.Hex();
}

// ============================================================================================================================
// recipes
// ============================================================================================================================

const Recipe* Recipes::Find(std::string_view sprite) const {
    for (const Recipe& recipe : items) {
        if (recipe.sprite == sprite) return &recipe;
    }
    return nullptr;
}

namespace {

// The reading of picture_recipes.json: every member named, of its type, in its bounds; the first fault stops the load, naming the file and the member.
struct Reader {
    std::string path;
    std::string error;

    bool Fail(const std::string& where, const std::string& what) {
        error = path + ": " + where + " " + what;
        return false;
    }
    // Members that start with '_' are notes; any other member the reader does not know is refused, so a misspelt number is never silently a default.
    bool Only(const Json::Value& object, const std::string& where, std::initializer_list<const char*> known) {
        if (!object.IsObject()) return Fail(where, "is not an object");
        for (const auto& [name, value] : object.AsObject()) {
            (void)value;
            if (!name.empty() && name[0] == '_') continue;
            bool found = false;
            for (const char* key : known) found = found || name == key;
            if (!found) return Fail(where + "." + name, "is not a member this table has");
        }
        return true;
    }
    bool Number(const Json::Value& object, const std::string& where, const char* key, double& out) {
        if (!object.Has(key) || !object[key].IsNumber() || !std::isfinite(object[key].AsNumber())) return Fail(where + "." + key, "is missing or not a number");
        out = object[key].AsNumber();
        return true;
    }
    bool Whole(const Json::Value& value, const std::string& where, int& out) {
        if (!value.IsNumber() || !std::isfinite(value.AsNumber()) || value.AsNumber() != std::floor(value.AsNumber()) || std::fabs(value.AsNumber()) > 1e6) {
            return Fail(where, "is not a whole number");
        }
        out = static_cast<int>(value.AsNumber());
        return true;
    }
    bool WholeMember(const Json::Value& object, const std::string& where, const char* key, int& out) {
        if (!object.Has(key)) return Fail(where + "." + key, "is missing");
        return Whole(object[key], where + "." + key, out);
    }
    bool Text(const Json::Value& object, const std::string& where, const char* key, std::string& out) {
        if (!object.Has(key) || !object[key].IsString() || object[key].AsString().empty()) return Fail(where + "." + key, "is missing or not a non-empty string");
        out = object[key].AsString();
        return true;
    }
    bool Flag(const Json::Value& object, const std::string& where, const char* key, bool& out) {
        if (!object.Has(key) || !object[key].IsBool()) return Fail(where + "." + key, "is missing or not true or false");
        out = object[key].AsBool();
        return true;
    }
    // A flag that may be left out (then `out` is as it was); present, it must be true or false.
    bool FlagOpt(const Json::Value& object, const std::string& where, const char* key, bool& out) {
        if (!object.Has(key)) return true;
        return Flag(object, where, key, out);
    }
    // [a, b] as two numbers.
    bool Pair(const Json::Value& object, const std::string& where, const char* key, double& a, double& b) {
        const std::string name = where + "." + key;
        if (!object.Has(key) || !object[key].IsArray() || object[key].AsArray().size() != 2) return Fail(name, "is not a pair of numbers");
        const Json::Value& first = object[key].AsArray()[0];
        const Json::Value& second = object[key].AsArray()[1];
        if (!first.IsNumber() || !second.IsNumber() || !std::isfinite(first.AsNumber()) || !std::isfinite(second.AsNumber())) return Fail(name, "is not a pair of numbers");
        a = first.AsNumber();
        b = second.AsNumber();
        return true;
    }
    bool WholePair(const Json::Value& object, const std::string& where, const char* key, int& a, int& b) {
        const std::string name = where + "." + key;
        if (!object.Has(key) || !object[key].IsArray() || object[key].AsArray().size() != 2) return Fail(name, "is not a pair of whole numbers");
        return Whole(object[key].AsArray()[0], name + "[0]", a) && Whole(object[key].AsArray()[1], name + "[1]", b);
    }
    // [x0, y0, x1, y1], whole, the right way round.
    bool RectOf(const Json::Value& object, const std::string& where, const char* key, Rect& out) {
        const std::string name = where + "." + key;
        if (!object.Has(key) || !object[key].IsArray() || object[key].AsArray().size() != 4) return Fail(name, "is not four whole numbers [x0, y0, x1, y1]");
        const Json::Array& a = object[key].AsArray();
        Rect r;
        if (!Whole(a[0], name + "[0]", r.x0) || !Whole(a[1], name + "[1]", r.y0) || !Whole(a[2], name + "[2]", r.x1) || !Whole(a[3], name + "[3]", r.y1)) return false;
        if (!r.Valid()) return Fail(name, "is empty or upside down: x0 < x1 and y0 < y1 are needed");
        out = r;
        return true;
    }
    // A list of [a, b) spans.
    bool Spans(const Json::Value& object, const std::string& where, const char* key, std::vector<Span>& out) {
        const std::string name = where + "." + key;
        if (!object.Has(key) || !object[key].IsArray() || object[key].AsArray().empty()) return Fail(name, "is not a list of [a, b] pairs");
        std::vector<Span> read;
        for (std::size_t i = 0; i < object[key].AsArray().size(); ++i) {
            const Json::Value& item = object[key].AsArray()[i];
            const std::string at = name + "[" + std::to_string(i) + "]";
            if (!item.IsArray() || item.AsArray().size() != 2) return Fail(at, "is not a pair of whole numbers");
            Span span;
            if (!Whole(item.AsArray()[0], at + "[0]", span.a) || !Whole(item.AsArray()[1], at + "[1]", span.b)) return false;
            if (span.a < 0 || span.b <= span.a) return Fail(at, "is empty or upside down: 0 <= a < b is needed");
            read.push_back(span);
        }
        out = std::move(read);
        return true;
    }
};

bool IsHex(const std::string& text, std::size_t length) {
    if (text.size() != length) return false;
    for (const char c : text) {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    }
    return true;
}

bool ParseRule(const std::string& name, Rule& out) {
    if (name == "row_lerp") out = Rule::RowLerp;
    else if (name == "separable") out = Rule::Separable;
    else if (name == "twin") out = Rule::Twin;
    else if (name == "generated") out = Rule::Generated;
    else if (name == "plaque") out = Rule::Plaque;
    else if (name == "panel") out = Rule::Panel;
    else if (name == "clear_rows") out = Rule::ClearRows;
    else return false;
    return true;
}

bool ParseLayout(const std::string& name, Layout& out) {
    if (name == "word") out = Layout::Word;
    else if (name == "stack") out = Layout::Stack;
    else if (name == "paragraph") out = Layout::Paragraph;
    else if (name == "bands") out = Layout::Bands;
    else return false;
    return true;
}

bool ParseBreak(const std::string& name, BreakAt& out) {
    if (name == "space") out = BreakAt::Space;
    else if (name == "after_dash") out = BreakAt::AfterDash;
    else return false;
    return true;
}

bool InsidePicture(const Rect& r, int width, int height) {
    return r.x0 >= 0 && r.y0 >= 0 && r.x1 <= width && r.y1 <= height;
}

// [a, b) as a pair of whole numbers with 0 <= a < b <= limit.
bool Range(Reader& in, const Json::Value& block, const std::string& where, const char* key, int limit, Span& out) {
    Span span;
    if (!in.WholePair(block, where, key, span.a, span.b)) return false;
    if (span.a < 0 || span.b <= span.a || span.b > limit) {
        return in.Fail(where + "." + key, "is empty, upside down or outside the picture: 0 <= a < b <= " + std::to_string(limit) + " is needed");
    }
    out = span;
    return true;
}

bool SpanInside(const Span& inner, const Span& outer) {
    return inner.a >= outer.a && inner.b <= outer.b;
}

// A list of [x0, y0, x1, y1] rectangles, whole, the right way round and inside the picture.
bool RectList(Reader& in, const Json::Value& block, const std::string& where, const char* key, int width, int height, std::vector<Rect>& out) {
    const std::string name = where + "." + key;
    if (!block.Has(key) || !block[key].IsArray() || block[key].AsArray().empty()) return in.Fail(name, "is not a list of [x0, y0, x1, y1] rectangles");
    std::vector<Rect> read;
    for (std::size_t i = 0; i < block[key].AsArray().size(); ++i) {
        const Json::Value& item = block[key].AsArray()[i];
        const std::string at = name + "[" + std::to_string(i) + "]";
        if (!item.IsArray() || item.AsArray().size() != 4) return in.Fail(at, "is not four whole numbers [x0, y0, x1, y1]");
        Rect r;
        if (!in.Whole(item.AsArray()[0], at + "[0]", r.x0) || !in.Whole(item.AsArray()[1], at + "[1]", r.y0) || !in.Whole(item.AsArray()[2], at + "[2]", r.x1) ||
            !in.Whole(item.AsArray()[3], at + "[3]", r.y1)) {
            return false;
        }
        if (!r.Valid()) return in.Fail(at, "is empty or upside down: x0 < x1 and y0 < y1 are needed");
        if (!InsidePicture(r, width, height)) return in.Fail(at, "is not inside the picture");
        read.push_back(r);
    }
    out = std::move(read);
    return true;
}

bool ReadPlaque(Reader& in, const Json::Value& block, const std::string& where, Recipe& recipe) {
    Erase& e = recipe.erase;
    if (!in.Only(block, where, {"rule", "interior_cols", "frame_cols", "center_cols", "h_rows", "v_top_rows", "v_margin_rows", "margin_cols", "agree_tol", "rects", "mirror", "flat_cols",
                                "skip_cols", "chroma", "guard"})) {
        return false;
    }
    const int w = recipe.width;
    const int h = recipe.height;
    if (!Range(in, block, where, "interior_cols", w, e.interiorCols) || !in.Spans(block, where, "frame_cols", e.frameCols) || !Range(in, block, where, "center_cols", w, e.centerCols) ||
        !in.Spans(block, where, "h_rows", e.refRows) || !Range(in, block, where, "v_top_rows", h, e.vTopRows) || !Range(in, block, where, "v_margin_rows", h, e.vMarginRows) ||
        !in.Spans(block, where, "margin_cols", e.refCols) || !in.Number(block, where, "agree_tol", e.agreeTol) || !RectList(in, block, where, "rects", w, h, e.rects)) {
        return false;
    }
    if (e.agreeTol <= 0.0) return in.Fail(where + ".agree_tol", "is above zero");
    for (const Span& span : e.frameCols) {
        if (span.b > w) return in.Fail(where + ".frame_cols", "is not inside the picture");
        if (!(span.b <= e.interiorCols.a || span.a >= e.interiorCols.b)) return in.Fail(where + ".frame_cols", "overlaps the interior columns");
    }
    if (!SpanInside(e.centerCols, e.interiorCols)) return in.Fail(where + ".center_cols", "is not inside the interior columns");
    for (const Span& span : e.refRows) {
        if (span.b > h) return in.Fail(where + ".h_rows", "is not inside the picture");
    }
    if (e.vTopRows.b - e.vTopRows.a < 4) return in.Fail(where + ".v_top_rows", "holds at least four rows (the hold-out check needs them)");
    if (e.vMarginRows.a < e.vTopRows.b) return in.Fail(where + ".v_margin_rows", "starts below v_top_rows: the knots of V must ascend");
    if (e.refCols.size() != 2) return in.Fail(where + ".margin_cols", "is the left margin and the right margin: two pairs");
    for (const Span& span : e.refCols) {
        if (!SpanInside(span, e.interiorCols)) return in.Fail(where + ".margin_cols", "is not inside the interior columns");
    }
    if (e.refCols[0].b > e.refCols[1].a) return in.Fail(where + ".margin_cols", "the left margin is not left of the right one");
    // Every column of every rectangle is a frame column or an interior one: the model has a value for it.
    Rect bounds = e.rects[0];
    for (const Rect& r : e.rects) {
        bounds.x0 = std::min(bounds.x0, r.x0);
        bounds.y0 = std::min(bounds.y0, r.y0);
        bounds.x1 = std::max(bounds.x1, r.x1);
        bounds.y1 = std::max(bounds.y1, r.y1);
        for (int x = r.x0; x < r.x1; ++x) {
            bool known = x >= e.interiorCols.a && x < e.interiorCols.b;
            for (const Span& span : e.frameCols) known = known || (x >= span.a && x < span.b);
            if (!known) return in.Fail(where + ".rects", "holds column " + std::to_string(x) + ", which is neither a frame column nor an interior one");
        }
    }
    e.rect = bounds;
    if (block.Has("mirror")) {
        const Json::Value& mirror = block["mirror"];
        const std::string at = where + ".mirror";
        if (!in.Only(mirror, at, {"cols", "axis"}) || !Range(in, mirror, at, "cols", w, e.mirrorCols) || !in.WholeMember(mirror, at, "axis", e.mirrorAxis)) return false;
        if (!SpanInside(e.mirrorCols, e.interiorCols)) return in.Fail(at + ".cols", "is not inside the interior columns");
        for (int x = e.mirrorCols.a; x < e.mirrorCols.b; ++x) {
            const int from = e.mirrorAxis - x;
            if (from < e.interiorCols.a || from >= e.interiorCols.b || (from >= e.mirrorCols.a && from < e.mirrorCols.b)) {
                return in.Fail(at, "column " + std::to_string(x) + " has no clean mirror column inside the interior");
            }
        }
        e.hasMirror = true;
    }
    if (block.Has("flat_cols")) {
        if (!Range(in, block, where, "flat_cols", w, e.flatCols)) return false;
        if (!SpanInside(e.flatCols, e.interiorCols)) return in.Fail(where + ".flat_cols", "is not inside the interior columns");
        e.hasFlat = true;
    }
    if (block.Has("skip_cols")) {
        if (!Range(in, block, where, "skip_cols", w, e.skipCols)) return false;
        e.hasSkip = true;
    }
    if (block.Has("chroma")) {
        const Json::Value& chroma = block["chroma"];
        const std::string at = where + ".chroma";
        if (!in.Only(chroma, at, {"threshold", "radius", "min", "max"}) || !in.WholeMember(chroma, at, "threshold", e.chroma.threshold) ||
            !in.WholeMember(chroma, at, "radius", e.chroma.radius) || !in.WholeMember(chroma, at, "min", e.chroma.keepMin) || !in.WholeMember(chroma, at, "max", e.chroma.keepMax)) {
            return false;
        }
        if (e.chroma.threshold < 1 || e.chroma.threshold > 254) return in.Fail(at + ".threshold", "is from 1 to 254");
        if (e.chroma.radius < 0 || e.chroma.radius > 8) return in.Fail(at + ".radius", "is from 0 to 8");
        if (e.chroma.keepMin < 0 || e.chroma.keepMax < e.chroma.keepMin) return in.Fail(at, "has 0 <= min <= max");
        e.chroma.enabled = true;
    }
    if (!block.Has("guard") || !block["guard"].IsObject()) return in.Fail(where + ".guard", "is missing");
    const Json::Value& guard = block["guard"];
    const std::string gwhere = where + ".guard";
    PlaqueGuard& g = e.plaque;
    if (!in.Only(guard, gwhere, {"sep_mean", "sep_max", "holdout", "margin_window", "margin_ratio", "ring_max", "slope_band"}) || !in.Number(guard, gwhere, "sep_mean", g.sepMean) ||
        !in.Number(guard, gwhere, "sep_max", g.sepMax) || !in.Number(guard, gwhere, "holdout", g.holdout) || !Range(in, guard, gwhere, "margin_window", h, g.marginWindow) ||
        !in.Number(guard, gwhere, "margin_ratio", g.marginRatio) || !in.Number(guard, gwhere, "ring_max", g.ringMax)) {
        return false;
    }
    if (guard.Has("slope_band") && !in.Number(guard, gwhere, "slope_band", g.slopeBand)) return false;
    if (g.sepMean <= 0.0 || g.sepMax <= 0.0 || g.holdout <= 0.0 || g.ringMax <= 0.0 || g.slopeBand < 0.0) return in.Fail(gwhere, "has limits above zero");
    if (g.marginRatio <= 0.0 || g.marginRatio > 1.0) return in.Fail(gwhere + ".margin_ratio", "is above zero and at most one");
    if (!SpanInside(g.marginWindow, e.vMarginRows)) return in.Fail(gwhere + ".margin_window", "is not inside v_margin_rows");
    return true;
}

bool ReadPanel(Reader& in, const Json::Value& block, const std::string& where, Recipe& recipe) {
    Erase& e = recipe.erase;
    if (!in.Only(block, where, {"rule", "rect", "interior_cols", "center_cols", "h_rows", "margin_cols", "agree_tol", "guard"})) return false;
    const int w = recipe.width;
    const int h = recipe.height;
    if (!in.RectOf(block, where, "rect", e.rect) || !Range(in, block, where, "interior_cols", w, e.interiorCols) || !Range(in, block, where, "center_cols", w, e.centerCols) ||
        !in.Spans(block, where, "h_rows", e.refRows) || !in.Spans(block, where, "margin_cols", e.refCols) || !in.Number(block, where, "agree_tol", e.agreeTol)) {
        return false;
    }
    if (!InsidePicture(e.rect, w, h)) return in.Fail(where + ".rect", "is not inside the picture");
    if (e.rect.x0 < e.interiorCols.a || e.rect.x1 > e.interiorCols.b) return in.Fail(where + ".rect", "is not inside the interior columns");
    if (e.rect.y0 < 4 || e.rect.y1 + 3 > h) return in.Fail(where + ".rect", "needs three rows of the picture above and below it for the ring check");
    if (e.agreeTol <= 0.0) return in.Fail(where + ".agree_tol", "is above zero");
    if (!SpanInside(e.centerCols, e.interiorCols)) return in.Fail(where + ".center_cols", "is not inside the interior columns");
    for (const Span& span : e.refRows) {
        if (span.b > h) return in.Fail(where + ".h_rows", "is not inside the picture");
    }
    if (e.refCols.size() != 2) return in.Fail(where + ".margin_cols", "is the left margin and the right margin: two pairs");
    for (const Span& span : e.refCols) {
        if (!SpanInside(span, e.interiorCols)) return in.Fail(where + ".margin_cols", "is not inside the interior columns");
    }
    if (e.refCols[0].b > e.refCols[1].a) return in.Fail(where + ".margin_cols", "the left margin is not left of the right one");
    if (!block.Has("guard") || !block["guard"].IsObject()) return in.Fail(where + ".guard", "is missing");
    const Json::Value& guard = block["guard"];
    const std::string gwhere = where + ".guard";
    PanelGuard& g = e.panel;
    if (!in.Only(guard, gwhere, {"sep_max", "v_margin", "agree_ratio", "ring_max"}) || !in.Number(guard, gwhere, "sep_max", g.sepMax) || !in.Number(guard, gwhere, "v_margin", g.vMargin) ||
        !in.Number(guard, gwhere, "agree_ratio", g.agreeRatio) || !in.Number(guard, gwhere, "ring_max", g.ringMax)) {
        return false;
    }
    if (g.sepMax <= 0.0 || g.vMargin <= 0.0 || g.ringMax <= 0.0) return in.Fail(gwhere, "has limits above zero");
    if (g.agreeRatio <= 0.0 || g.agreeRatio > 1.0) return in.Fail(gwhere + ".agree_ratio", "is above zero and at most one");
    return true;
}

bool ReadClear(Reader& in, const Json::Value& block, const std::string& where, Recipe& recipe) {
    Erase& e = recipe.erase;
    if (!in.Only(block, where, {"rule", "clear"}) || !in.Spans(block, where, "clear", e.clearRows)) return false;
    for (std::size_t i = 0; i < e.clearRows.size(); ++i) {
        const Span& span = e.clearRows[i];
        // A band is bounded by a picture row above and below, which the erase asserts are fully transparent.
        if (span.a < 1 || span.b + 1 > recipe.height) return in.Fail(where + ".clear[" + std::to_string(i) + "]", "has no picture row above and below it to be bounded by");
        if (i > 0 && span.a <= e.clearRows[i - 1].b) return in.Fail(where + ".clear[" + std::to_string(i) + "]", "touches or overlaps the band before it: a transparent row must lie between two bands");
    }
    e.rect = Rect{0, e.clearRows.front().a, recipe.width, e.clearRows.back().b}; // widened by the text's windows when the recipe is complete
    return true;
}

bool ReadErase(Reader& in, const Json::Value& block, const std::string& where, Recipe& recipe) {
    Erase& erase = recipe.erase;
    std::string rule;
    if (!block.IsObject()) return in.Fail(where, "is not an object");
    if (!in.Text(block, where, "rule", rule)) return false;
    if (!ParseRule(rule, erase.rule)) return in.Fail(where + ".rule", "'" + rule + "' is none of row_lerp, separable, twin, generated, plaque, panel and clear_rows");
    if (erase.rule == Rule::Plaque) return ReadPlaque(in, block, where, recipe);
    if (erase.rule == Rule::Panel) return ReadPanel(in, block, where, recipe);
    if (erase.rule == Rule::ClearRows) return ReadClear(in, block, where, recipe);
    if (!in.Only(block, where, {"rule", "rect", "ref_cols", "ref_rows", "flat_cols", "guard", "twin_sprite", "twin_digest", "shift", "check_rect", "min_inside"})) return false;
    const Rect picture{0, 0, recipe.width, recipe.height};
    if (erase.rule == Rule::Generated) {
        erase.rect = picture; // nothing of the picture is read, and all of it is written
        return true;
    }
    if (!in.RectOf(block, where, "rect", erase.rect)) return false;
    if (!InsidePicture(erase.rect, recipe.width, recipe.height)) return in.Fail(where + ".rect", "is not inside the picture");
    if (erase.rule == Rule::Twin) {
        if (!in.Text(block, where, "twin_sprite", erase.twinSprite) || !in.Text(block, where, "twin_digest", erase.twinDigest)) return false;
        if (!IsHex(erase.twinDigest, 32)) return in.Fail(where + ".twin_digest", "is not 32 lower-case hex characters");
        if (!in.WholePair(block, where, "shift", erase.shiftX, erase.shiftY)) return false;
        if (!in.RectOf(block, where, "check_rect", erase.checkRect)) return false;
        if (!InsidePicture(erase.checkRect, recipe.width, recipe.height)) return in.Fail(where + ".check_rect", "is not inside the picture");
        if (erase.checkRect.x0 < erase.rect.x0 || erase.checkRect.y0 < erase.rect.y0 || erase.checkRect.x1 > erase.rect.x1 || erase.checkRect.y1 > erase.rect.y1) {
            return in.Fail(where + ".check_rect", "is not inside the rect it checks the word's window for");
        }
        if (!in.WholeMember(block, where, "min_inside", erase.minInside)) return false;
        if (erase.minInside <= 0) return in.Fail(where + ".min_inside", "is above zero");
        return true;
    }
    // The two rules that model the plate from its own text-free pixels: a left band and a right band of columns, inside the picture and clear of each other.
    if (!in.Spans(block, where, "ref_cols", erase.refCols)) return false;
    if (erase.refCols.size() != 2) return in.Fail(where + ".ref_cols", "is the left band and the right band: two pairs");
    for (const Span& span : erase.refCols) {
        if (span.b > recipe.width) return in.Fail(where + ".ref_cols", "is not inside the picture");
    }
    if (erase.refCols[0].b > erase.refCols[1].a) return in.Fail(where + ".ref_cols", "the left band is not left of the right band");
    if (!block.Has("guard") || !block["guard"].IsObject()) return in.Fail(where + ".guard", "is missing");
    const Json::Value& guard = block["guard"];
    const std::string gwhere = where + ".guard";
    if (erase.rule == Rule::RowLerp) {
        if (!in.Only(guard, gwhere, {"agree_max", "collar_max"}) || !in.Number(guard, gwhere, "agree_max", erase.guard.agreeMax) ||
            !in.Number(guard, gwhere, "collar_max", erase.guard.collarMax)) {
            return false;
        }
        if (erase.guard.agreeMax <= 0.0 || erase.guard.collarMax <= 0.0) return in.Fail(gwhere, "has limits above zero");
        return true;
    }
    if (!in.Spans(block, where, "ref_rows", erase.refRows)) return false;
    for (const Span& span : erase.refRows) {
        if (span.b > recipe.height) return in.Fail(where + ".ref_rows", "is not inside the picture");
    }
    if (!block.Has("flat_cols") || !block["flat_cols"].IsArray() || block["flat_cols"].AsArray().size() != 2) return in.Fail(where + ".flat_cols", "is not a pair of whole numbers");
    if (!in.WholePair(block, where, "flat_cols", erase.flatCols.a, erase.flatCols.b)) return false;
    if (erase.flatCols.a < 0 || erase.flatCols.b <= erase.flatCols.a || erase.flatCols.b > recipe.width) return in.Fail(where + ".flat_cols", "is empty or outside the picture");
    if (!in.Only(guard, gwhere, {"collar_max", "holdout_mean", "holdout_max"}) || !in.Number(guard, gwhere, "collar_max", erase.guard.collarMax) ||
        !in.Number(guard, gwhere, "holdout_mean", erase.guard.holdoutMean) || !in.Number(guard, gwhere, "holdout_max", erase.guard.holdoutMax)) {
        return false;
    }
    if (erase.guard.collarMax <= 0.0 || erase.guard.holdoutMean <= 0.0 || erase.guard.holdoutMax <= 0.0) return in.Fail(gwhere, "has limits above zero");
    return true;
}

bool ReadShadow(Reader& in, const Json::Value& block, const std::string& where, ShadowLook& out) {
    if (!block.Has("shadow") || !block["shadow"].IsObject()) return in.Fail(where + ".shadow", "is missing");
    const Json::Value& shadow = block["shadow"];
    const std::string at = where + ".shadow";
    if (!in.Only(shadow, at, {"dx", "dy", "sigma", "sigma_min", "gain", "cap", "round_offsets"})) return false;
    if (!in.Number(shadow, at, "dx", out.dx) || !in.Number(shadow, at, "dy", out.dy) || !in.Number(shadow, at, "sigma", out.sigma) ||
        !in.Number(shadow, at, "sigma_min", out.sigmaMin) || !in.Number(shadow, at, "gain", out.gain) || !in.Number(shadow, at, "cap", out.cap) ||
        !in.Flag(shadow, at, "round_offsets", out.roundOffsets)) {
        return false;
    }
    if (out.sigma < 0.0 || out.sigmaMin < 0.0) return in.Fail(at, "has a sigma below zero");
    if (out.gain <= 0.0) return in.Fail(at + ".gain", "is above zero");
    if (out.cap <= 0.0 || out.cap > 1.0) return in.Fail(at + ".cap", "is above zero and at most one");
    return true;
}

bool ReadInner(Reader& in, const Json::Value& block, const std::string& where, InnerLook& out) {
    if (!block.Has("inner") || !block["inner"].IsObject()) return in.Fail(where + ".inner", "is missing");
    const Json::Value& inner = block["inner"];
    const std::string at = where + ".inner";
    if (!in.Only(inner, at, {"k", "sigma", "dx", "dy"}) || !in.Number(inner, at, "k", out.k) || !in.Number(inner, at, "sigma", out.sigma) ||
        !in.Number(inner, at, "dx", out.dx) || !in.Number(inner, at, "dy", out.dy)) {
        return false;
    }
    if (out.k < 0.0 || out.k > 1.0) return in.Fail(at + ".k", "is from 0 to 1");
    if (out.sigma < 0.0) return in.Fail(at + ".sigma", "is not below zero");
    return true;
}

// The faces: a member per face id, each with its ratio and its embolden, optionally the glyph it is measured by and (a paragraph's) its line pitch.
bool ReadFaces(Reader& in, const Json::Value& block, const std::string& where, std::vector<FaceTuning>& out) {
    if (!block.Has("faces") || !block["faces"].IsObject()) return in.Fail(where + ".faces", "is missing");
    std::vector<FaceTuning> read;
    for (const auto& [id, value] : block["faces"].AsObject()) {
        if (!id.empty() && id[0] == '_') continue;
        const std::string at = where + ".faces." + id;
        if (!in.Only(value, at, {"ratio", "embolden", "reference", "pitch"})) return false;
        FaceTuning tuning;
        tuning.face = id;
        if (!in.Number(value, at, "ratio", tuning.ratio) || !in.Number(value, at, "embolden", tuning.embolden)) return false;
        if (tuning.ratio <= 0.0) return in.Fail(at + ".ratio", "is above zero");
        if (tuning.embolden < 0.0 || tuning.embolden > 1.0) return in.Fail(at + ".embolden", "is from 0 to 1");
        if (value.Has("reference")) {
            std::uint32_t point = 0;
            if (!value["reference"].IsString() || !FontBake::ParseCodePoint(value["reference"].AsString(), point)) return in.Fail(at + ".reference", "is not a code point like U+0078");
            tuning.reference = point;
        }
        if (value.Has("pitch")) {
            if (!in.Number(value, at, "pitch", tuning.pitch)) return false;
            if (tuning.pitch <= 0.0) return in.Fail(at + ".pitch", "is above zero");
        }
        read.push_back(tuning);
    }
    out = std::move(read);
    return true;
}

// The ink's grey level and whether a Japanese space is a gap: optional members of every layout.
bool ReadInkLook(Reader& in, const Json::Value& block, const std::string& where, Text& text) {
    if (block.Has("ink_level")) {
        if (!in.WholeMember(block, where, "ink_level", text.inkLevel)) return false;
        if (text.inkLevel < 0 || text.inkLevel > 255) return in.Fail(where + ".ink_level", "is from 0 to 255");
    }
    return in.FlagOpt(block, where, "japanese_spaces", text.japaneseSpaces);
}

bool ReadBands(Reader& in, const Json::Value& block, const std::string& where, Recipe& recipe) {
    Text& text = recipe.text;
    if (!in.Only(block, where, {"layout", "slot", "faces", "min_scale", "step", "ink_threshold", "gap", "left", "max_right", "digit_face", "ink_level", "japanese_spaces", "bands"})) return false;
    double left = 0.0;
    double right = 0.0;
    if (!in.Text(block, where, "slot", text.slot) || !in.Number(block, where, "min_scale", text.minScale) || !in.Number(block, where, "step", text.step) ||
        !in.Number(block, where, "ink_threshold", text.inkThreshold) || !in.Number(block, where, "gap", text.gap) || !in.Number(block, where, "left", left) ||
        !in.Number(block, where, "max_right", right) || !ReadFaces(in, block, where, text.faces) || !in.WholeMember(block, where, "ink_level", text.inkLevel) || !ReadInkLook(in, block, where, text)) {
        return false;
    }
    if (text.minScale <= 0.0 || text.minScale > 1.0) return in.Fail(where + ".min_scale", "is above zero and at most one");
    if (text.step <= 0.0 || text.step >= 1.0) return in.Fail(where + ".step", "is above zero and below one");
    if (text.inkThreshold <= 0.0 || text.inkThreshold >= 1.0) return in.Fail(where + ".ink_threshold", "is above zero and below one");
    if (text.gap < 0.0) return in.Fail(where + ".gap", "is not below zero");
    if (left < 0.0 || right <= left || right > recipe.width) return in.Fail(where + ".max_right", "is right of left and inside the picture");
    text.left = left;
    text.maxRight = right;
    if (block.Has("digit_face") && !in.Text(block, where, "digit_face", text.digitFace)) return false;
    if (!block.Has("bands") || !block["bands"].IsArray() || block["bands"].AsArray().empty()) return in.Fail(where + ".bands", "is not a list of bands");
    for (std::size_t i = 0; i < block["bands"].AsArray().size(); ++i) {
        const Json::Value& item = block["bands"].AsArray()[i];
        const std::string at = where + ".bands[" + std::to_string(i) + "]";
        if (!in.Only(item, at, {"key", "px", "first_baseline", "anchor", "pitch", "max_lines", "ink_rows", "break", "faces", "balance"})) return false;
        Band band;
        std::string anchor;
        std::string breakAt;
        if (!in.Text(item, at, "key", band.key) || !in.Number(item, at, "px", band.px) || !in.Number(item, at, "first_baseline", band.firstBaseline) || !in.Text(item, at, "anchor", anchor) ||
            !in.Number(item, at, "pitch", band.pitch) || !in.WholeMember(item, at, "max_lines", band.maxLines) || !Range(in, item, at, "ink_rows", recipe.height, band.inkRows) ||
            !in.Text(item, at, "break", breakAt)) {
            return false;
        }
        if (anchor != "first" && anchor != "last") return in.Fail(at + ".anchor", "'" + anchor + "' is neither first nor last");
        band.anchorLast = anchor == "last";
        if (!ParseBreak(breakAt, band.breakAt)) return in.Fail(at + ".break", "'" + breakAt + "' is neither space nor after_dash");
        if (band.px <= 0.0 || band.pitch <= 0.0) return in.Fail(at + ".px", "and the pitch are above zero");
        if (band.maxLines < 1 || band.maxLines > 4) return in.Fail(at + ".max_lines", "is from 1 to 4");
        if (band.firstBaseline < 0.0 || band.firstBaseline > recipe.height) return in.Fail(at + ".first_baseline", "is not inside the picture");
        if (!text.bands.empty() && band.inkRows.a < text.bands.back().inkRows.b) return in.Fail(at + ".ink_rows", "overlaps the band before it: the bands' windows ascend and do not meet");
        if (item.Has("faces") && !ReadFaces(in, item, at, band.faces)) return false;
        if (item.Has("balance")) {
            const Json::Value& balance = item["balance"];
            const std::string where2 = at + ".balance";
            if (!balance.IsObject()) return in.Fail(where2, "is not an object");
            if (!in.Only(balance, where2, {"above_band", "below_row", "min_lines"})) return false;
            if (!in.WholeMember(balance, where2, "above_band", band.balanceAbove) || !in.Number(balance, where2, "below_row", band.balanceBelow) ||
                !in.WholeMember(balance, where2, "min_lines", band.balanceMinLines)) {
                return false;
            }
            if (band.balanceAbove < 0 || static_cast<std::size_t>(band.balanceAbove) >= text.bands.size()) return in.Fail(where2 + ".above_band", "is not the index of an earlier band");
            if (band.balanceMinLines < 2 || band.balanceMinLines > band.maxLines) return in.Fail(where2 + ".min_lines", "is at least two and at most the band's max_lines (one line has nothing to balance)");
            if (band.balanceBelow < static_cast<double>(band.inkRows.b) || band.balanceBelow > recipe.height) return in.Fail(where2 + ".below_row", "is not a row at or below the band's ink window and inside the picture");
            band.balanced = true;
        }
        text.bands.push_back(std::move(band));
    }
    return true;
}

bool ReadText(Reader& in, const Json::Value& block, const std::string& where, Recipe& recipe) {
    Text& text = recipe.text;
    std::string layout;
    if (!block.IsObject()) return in.Fail(where, "is not an object");
    if (!in.Text(block, where, "layout", layout)) return false;
    if (!ParseLayout(layout, text.layout)) return in.Fail(where + ".layout", "'" + layout + "' is none of word, stack, paragraph and bands");
    if (text.layout == Layout::Bands) return ReadBands(in, block, where, recipe);
    if (!in.Only(block, where, {"layout", "slot", "px", "faces", "anchor", "min_scale", "step", "ink_threshold", "max_width", "max_height", "shadow", "inner", "first_baseline",
                                "pitch", "gap", "max_lines", "safe", "orphan", "save_lines", "paint", "ink_level", "japanese_spaces"})) {
        return false;
    }
    if (!in.Text(block, where, "slot", text.slot) || !in.Number(block, where, "px", text.px) || !in.Pair(block, where, "anchor", text.anchorX, text.anchorY) ||
        !in.Number(block, where, "min_scale", text.minScale) || !in.Number(block, where, "step", text.step) ||
        !in.Number(block, where, "ink_threshold", text.inkThreshold) || !in.Number(block, where, "max_width", text.maxWidth)) {
        return false;
    }
    if (text.px <= 0.0) return in.Fail(where + ".px", "is above zero");
    if (text.minScale <= 0.0 || text.minScale > 1.0) return in.Fail(where + ".min_scale", "is above zero and at most one");
    if (text.step <= 0.0 || text.step >= 1.0) return in.Fail(where + ".step", "is above zero and below one");
    if (text.inkThreshold <= 0.0 || text.inkThreshold >= 1.0) return in.Fail(where + ".ink_threshold", "is above zero and below one");
    if (text.maxWidth <= 0.0) return in.Fail(where + ".max_width", "is above zero");
    if (text.anchorX < 0.0 || text.anchorX > recipe.width || text.anchorY < 0.0 || text.anchorY > recipe.height) return in.Fail(where + ".anchor", "is not inside the picture");

    if (!ReadFaces(in, block, where, text.faces)) return false;
    for (const FaceTuning& tuning : text.faces) {
        // A line pitch of a face belongs to a paragraph: the other layouts refuse it rather than ignore it.
        if (tuning.pitch != 0.0 && text.layout != Layout::Paragraph) return in.Fail(where + ".faces." + tuning.face + ".pitch", "belongs to a paragraph and this layout is not one");
        if (tuning.pitch == 0.0 && text.layout == Layout::Paragraph) return in.Fail(where + ".faces." + tuning.face + ".pitch", "is missing: a paragraph's line pitch is set per face");
    }
    if (!ReadShadow(in, block, where, text.shadow) || !ReadInner(in, block, where, text.inner) || !ReadInkLook(in, block, where, text)) return false;
    if (block.Has("paint")) {
        if (!in.RectOf(block, where, "paint", text.paint)) return false;
        if (!InsidePicture(text.paint, recipe.width, recipe.height)) return in.Fail(where + ".paint", "is not inside the picture");
        const Rect& r = recipe.erase.rect;
        if (text.paint.x0 > r.x0 || text.paint.y0 > r.y0 || text.paint.x1 < r.x1 || text.paint.y1 < r.y1) return in.Fail(where + ".paint", "does not hold the erase's rectangle");
        text.hasPaint = true;
    }

    if (text.layout == Layout::Word) {
        if (!in.Number(block, where, "max_height", text.maxHeight)) return false;
        if (text.maxHeight <= 0.0) return in.Fail(where + ".max_height", "is above zero");
        // A word is not stacked: the members of a stack are refused rather than ignored.
        for (const char* key : {"first_baseline", "pitch", "gap", "max_lines", "safe"}) {
            if (block.Has(key)) return in.Fail(where + "." + key, "belongs to a stack and this layout is a word");
        }
        for (const char* key : {"orphan", "save_lines"}) {
            if (block.Has(key)) return in.Fail(where + "." + key, "belongs to a paragraph and this layout is a word");
        }
        text.maxLines = 1;
        return true;
    }
    if (text.layout == Layout::Stack) {
        if (!in.Number(block, where, "first_baseline", text.firstBaseline) || !in.Number(block, where, "pitch", text.pitch) || !in.Number(block, where, "gap", text.gap) ||
            !in.WholeMember(block, where, "max_lines", text.maxLines) || !in.RectOf(block, where, "safe", text.safe)) {
            return false;
        }
        if (text.pitch <= 0.0 || text.gap < 0.0) return in.Fail(where + ".pitch", "is above zero (and the gap not below)");
        if (text.maxLines < 1 || text.maxLines > 3) return in.Fail(where + ".max_lines", "is from 1 to 3");
        if (!InsidePicture(text.safe, recipe.width, recipe.height)) return in.Fail(where + ".safe", "is not inside the picture");
        if (block.Has("max_height")) return in.Fail(where + ".max_height", "belongs to a word and this layout is a stack");
        for (const char* key : {"orphan", "save_lines"}) {
            if (block.Has(key)) return in.Fail(where + "." + key, "belongs to a paragraph and this layout is a stack");
        }
        return true;
    }
    // A paragraph: the line pitch is per face, the lines are centred on the anchor's x and the block's ink on its y, and the ink must lie inside `safe`.
    if (!in.Number(block, where, "gap", text.gap) || !in.WholeMember(block, where, "max_lines", text.maxLines) || !in.RectOf(block, where, "safe", text.safe) ||
        !in.Number(block, where, "orphan", text.orphan) || !in.Number(block, where, "save_lines", text.saveLines)) {
        return false;
    }
    if (text.gap < 0.0) return in.Fail(where + ".gap", "is not below zero");
    if (text.maxLines < 1 || text.maxLines > 4) return in.Fail(where + ".max_lines", "is from 1 to 4");
    if (!InsidePicture(text.safe, recipe.width, recipe.height)) return in.Fail(where + ".safe", "is not inside the picture");
    if (text.orphan < 0.0 || text.orphan >= 1.0) return in.Fail(where + ".orphan", "is from 0 to below 1");
    if (text.saveLines < 0.0 || text.saveLines >= 0.5) return in.Fail(where + ".save_lines", "is from 0 to below 0.5");
    for (const char* key : {"first_baseline", "pitch", "max_height"}) {
        if (block.Has(key)) return in.Fail(where + "." + key, "does not belong to a paragraph (its pitch is per face)");
    }
    return true;
}

} // namespace

bool LoadRecipes(const std::string& path, Recipes& out, std::string& error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = path + ": cannot read";
        return false;
    }
    const std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    Json::Parser parser(content);
    Json::Value root;
    if (!parser.Parse(root)) {
        error = path + ": " + parser.Error();
        return false;
    }
    Reader in;
    in.path = path;
    if (!in.Only(root, "the table", {"version", "recipes"})) {
        error = in.error;
        return false;
    }
    int version = 0;
    if (!in.WholeMember(root, "the table", "version", version)) {
        error = in.error;
        return false;
    }
    if (version != kRecipesVersion) {
        error = Fmt("%s: version is %d and this build reads %d", path.c_str(), version, kRecipesVersion);
        return false;
    }
    if (!root.Has("recipes") || !root["recipes"].IsArray() || root["recipes"].AsArray().empty()) {
        error = path + ": recipes is missing or not a list";
        return false;
    }
    Recipes read;
    std::set<std::string> ids;
    std::set<std::string> sprites;
    std::set<std::string> keys;
    for (std::size_t i = 0; i < root["recipes"].AsArray().size(); ++i) {
        const Json::Value& item = root["recipes"].AsArray()[i];
        const std::string where = "recipes[" + std::to_string(i) + "]";
        Recipe recipe;
        const bool ok = in.Only(item, where, {"id", "key", "sprite", "size", "digest", "erase", "text"}) && in.Text(item, where, "id", recipe.id) &&
                        in.Text(item, where, "sprite", recipe.sprite) && in.Text(item, where, "digest", recipe.digest) && in.WholePair(item, where, "size", recipe.width, recipe.height);
        if (!ok) {
            error = in.error;
            return false;
        }
        if (recipe.width <= 0 || recipe.height <= 0 || recipe.width > kMaxSide || recipe.height > kMaxSide) {
            error = path + ": " + where + ".size is above zero and within the largest picture this module reads";
            return false;
        }
        if (!IsHex(recipe.digest, 32)) {
            error = path + ": " + where + ".digest is not 32 lower-case hex characters";
            return false;
        }
        if (!item.Has("erase") || !ReadErase(in, item["erase"], where + ".erase", recipe) || !item.Has("text") || !ReadText(in, item["text"], where + ".text", recipe)) {
            error = in.error.empty() ? path + ": " + where + " needs an erase and a text block" : in.error;
            return false;
        }
        // One key for a recipe that draws one phrase; a Bands recipe names a key in each band and has none of its own.
        if (recipe.text.layout == Layout::Bands) {
            if (item.Has("key")) {
                error = path + ": " + where + ".key belongs to a recipe of one phrase: a bands recipe names its keys in its bands";
                return false;
            }
        } else if (!in.Text(item, where, "key", recipe.key)) {
            error = in.error;
            return false;
        }
        bool repeated = !ids.insert(recipe.id).second || !sprites.insert(recipe.sprite).second;
        for (const std::string& key : recipe.Keys()) repeated = !keys.insert(key).second || repeated;
        if (repeated) {
            error = path + ": " + where + " repeats an id, a sprite or a key of another recipe";
            return false;
        }
        // The pairs of an erase rule and a layout that a recipe has been measured for: a plate is a word, a generated picture is a stack, a plaque or a panel is a paragraph, a strip
        // of words and names is its bands. (A later step may mix them; the writing of it needs a recipe to test.)
        const Rule rule = recipe.erase.rule;
        const Layout layout = recipe.text.layout;
        bool paired = false;
        switch (layout) {
        case Layout::Word:
            paired = rule == Rule::RowLerp || rule == Rule::Separable || rule == Rule::Twin || rule == Rule::Generated;
            break;
        case Layout::Stack:
            paired = rule == Rule::Generated;
            break;
        case Layout::Paragraph:
            paired = rule == Rule::Plaque || rule == Rule::Panel;
            break;
        case Layout::Bands:
            paired = rule == Rule::ClearRows;
            break;
        }
        if (!paired || (rule == Rule::ClearRows && layout != Layout::Bands)) {
            error = layout == Layout::Stack ? path + ": " + where + " is a plate with a stack of lines, which no recipe has been measured for"
                                            : path + ": " + where + " pairs this erase rule with this text layout, which no recipe has been measured for";
            return false;
        }
        if (rule == Rule::ClearRows) {
            // Each cleared band is drawn again in the window of the band of the same place, and the window holds it.
            if (recipe.erase.clearRows.size() != recipe.text.bands.size()) {
                error = path + ": " + where + " has " + std::to_string(recipe.erase.clearRows.size()) + " cleared bands and " + std::to_string(recipe.text.bands.size()) + " text bands";
                return false;
            }
            Rect widened = recipe.erase.rect;
            for (std::size_t b = 0; b < recipe.text.bands.size(); ++b) {
                const Span& clear = recipe.erase.clearRows[b];
                const Span& window = recipe.text.bands[b].inkRows;
                if (window.a > clear.a || window.b < clear.b) {
                    error = path + ": " + where + ".text.bands[" + std::to_string(b) + "].ink_rows does not hold the cleared rows " + std::to_string(clear.a) + ".." + std::to_string(clear.b);
                    return false;
                }
                widened.y0 = std::min(widened.y0, window.a);
                widened.y1 = std::max(widened.y1, window.b);
            }
            recipe.erase.rect = widened;
        }
        read.items.push_back(std::move(recipe));
    }
    Hasher h;
    h.Text(content);
    read.fileHash = h.Hex();
    read.loaded = true;
    out = std::move(read);
    return true;
}

// ============================================================================================================================
// the erase
// ============================================================================================================================

namespace {

// A grey level, rounded to the even neighbour as numpy's rint is, and kept in a byte.
unsigned char Level(double v) {
    return static_cast<unsigned char>(std::clamp(std::nearbyint(v), 0.0, 255.0));
}

// What the plate's model says every pixel of `rect` is, from the picture's own text-free pixels. `bands`: 0 both reference bands, 1 the left one alone, 2 the right
// one alone (the hold-out). Four bytes a pixel, row after row of the rect. False, with `why`, when the rectangle or a band is not inside the picture.
bool ModelFill(const Image& src, const Erase& erase, const Rect& rect, int bands, std::vector<unsigned char>& fill, std::string& why) {
    if (rect.x0 < 0 || rect.y0 < 0 || rect.x1 > src.width || rect.y1 > src.height || !rect.Valid()) {
        why = "the rectangle is not inside the picture";
        return false;
    }
    const int w = rect.Width();
    const int h = rect.Height();
    fill.assign(Z(w) * Z(h) * 4, 0);
    const auto px = [&](int x, int y, int c) { return static_cast<double>(src.At(x, y)[c]); };
    std::vector<Span> use;
    if (bands == 0 || bands == 1) use.push_back(erase.refCols[0]);
    if (bands == 0 || bands == 2) use.push_back(erase.refCols[1]);

    if (erase.rule == Rule::RowLerp) {
        const Span left = erase.refCols[0];
        const Span right = erase.refCols[1];
        // The bands of a hold-out are the same band twice, so the line between them is flat.
        const Span a = bands == 2 ? right : left;
        const Span b = bands == 1 ? left : right;
        const double xl = (static_cast<double>(a.a) + a.b - 1) / 2.0;
        const double xr = (static_cast<double>(b.a) + b.b - 1) / 2.0;
        for (int y = rect.y0; y < rect.y1; ++y) {
            double ml[4] = {0, 0, 0, 0};
            double mr[4] = {0, 0, 0, 0};
            for (int c = 0; c < 4; ++c) {
                for (int x = a.a; x < a.b; ++x) ml[c] += px(x, y, c);
                for (int x = b.a; x < b.b; ++x) mr[c] += px(x, y, c);
                ml[c] /= static_cast<double>(a.b - a.a);
                mr[c] /= static_cast<double>(b.b - b.a);
            }
            for (int x = rect.x0; x < rect.x1; ++x) {
                const double t = xr > xl ? std::clamp((x - xl) / (xr - xl), 0.0, 1.0) : 0.0;
                unsigned char* out = &fill[(Z(y - rect.y0) * Z(w) + Z(x - rect.x0)) * 4];
                for (int c = 0; c < 4; ++c) out[c] = Level(ml[c] * (1.0 - t) + mr[c] * t);
            }
        }
        return true;
    }

    // Separable: R(x), the horizontal vignette, from the text-free rows; F(y), the vertical profile, from the text-free columns with the vignette taken out of them.
    int lo = rect.x0;
    int hi = rect.x1;
    for (const Span& span : erase.refCols) {
        lo = std::min(lo, span.a);
        hi = std::max(hi, span.b);
    }
    std::vector<int> rows;
    for (const Span& span : erase.refRows) {
        for (int y = span.a; y < span.b; ++y) rows.push_back(y);
    }
    if (rows.empty() || lo < 0 || hi > src.width) {
        why = "the reference rows or columns are not inside the picture";
        return false;
    }
    double den[4] = {0, 0, 0, 0};
    for (const int y : rows) {
        for (int c = 0; c < 4; ++c) {
            double flat = 0.0;
            for (int x = erase.flatCols.a; x < erase.flatCols.b; ++x) flat += px(x, y, c);
            den[c] += flat / static_cast<double>(erase.flatCols.b - erase.flatCols.a);
        }
    }
    std::vector<double> vignette(Z(hi - lo) * 4, 0.0);
    for (int x = lo; x < hi; ++x) {
        for (int c = 0; c < 4; ++c) {
            double sum = 0.0;
            for (const int y : rows) sum += px(x, y, c);
            vignette[Z(x - lo) * 4 + Z(c)] = sum / std::max(den[c], 1e-6);
        }
    }
    const auto vig = [&](int x, int c) { return vignette[Z(x - lo) * 4 + Z(c)]; };
    for (int y = rect.y0; y < rect.y1; ++y) {
        double profile[4] = {0, 0, 0, 0};
        int count = 0;
        for (const Span& span : use) {
            for (int x = span.a; x < span.b; ++x) {
                for (int c = 0; c < 4; ++c) profile[c] += px(x, y, c) / std::max(vig(x, c), 1e-6);
                ++count;
            }
        }
        for (int c = 0; c < 4; ++c) profile[c] /= static_cast<double>(count);
        for (int x = rect.x0; x < rect.x1; ++x) {
            unsigned char* out = &fill[(Z(y - rect.y0) * Z(w) + Z(x - rect.x0)) * 4];
            for (int c = 0; c < 4; ++c) out[c] = Level(profile[c] * vig(x, c));
        }
    }
    return true;
}

// The picture `twin` moved by (dx, dy): out(x, y) = twin(x - dx, y - dy), transparent where that leaves the twin.
void ShiftedTwin(const Image& twin, int dx, int dy, int width, int height, Image& out) {
    out = Blank(width, height);
    for (int y = 0; y < height; ++y) {
        const int sy = y - dy;
        if (sy < 0 || sy >= twin.height) continue;
        for (int x = 0; x < width; ++x) {
            const int sx = x - dx;
            if (sx < 0 || sx >= twin.width) continue;
            std::memcpy(out.At(x, y), twin.At(sx, sy), 4);
        }
    }
}

bool SamePixel(const unsigned char* a, const unsigned char* b) {
    return a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3];
}

// ---- plaque, panel and cleared rows (planning step 248) --------------------------------------------------------------------------------------------

using Rgb = std::array<double, 3>;

constexpr std::size_t kSlopeRows = 12; // the rows above a hole that its slope is compared with

// numpy's median: the middle value of an odd count, the mean of the two middle ones of an even count.
double Median(std::vector<double>& values) {
    std::sort(values.begin(), values.end());
    const std::size_t n = values.size();
    return n % 2 == 1 ? values[n / 2] : (values[n / 2 - 1] + values[n / 2]) / 2.0;
}

std::vector<int> RowsOf(const std::vector<Span>& spans) {
    std::vector<int> rows;
    for (const Span& span : spans) {
        for (int y = span.a; y < span.b; ++y) rows.push_back(y);
    }
    return rows;
}

// The mean over [cols) of one row of one channel.
Rgb MeanOver(const Image& src, const Span& cols, int y) {
    Rgb mean{0.0, 0.0, 0.0};
    for (int x = cols.a; x < cols.b; ++x) {
        for (int c = 0; c < 3; ++c) mean[Z(c)] += static_cast<double>(src.At(x, y)[c]);
    }
    for (int c = 0; c < 3; ++c) mean[Z(c)] /= static_cast<double>(cols.b - cols.a);
    return mean;
}

double MaxAbs(const Rgb& a, const Rgb& b) {
    double worst = 0.0;
    for (std::size_t c = 0; c < 3; ++c) worst = std::max(worst, std::fabs(a[c] - b[c]));
    return worst;
}

// What a plaque is made of: H(x), the horizontal vignette of the interior, F(x), the frame columns' constant colour, and V(y), the interior's vertical profile as knots.
struct PlateModel {
    int ix0 = 0;
    std::vector<Rgb> h;               // H(x) for the interior columns, from ix0
    std::vector<Rgb> frame;           // F(x) for every column of the picture, meaningful where isFrame
    std::vector<char> isFrame;
    std::vector<double> ky;           // V's knots, ascending
    std::vector<Rgb> kv;
    std::vector<int> acceptedMargin;  // the rows of the margin scan whose two margins agreed

    // numpy.interp per channel: the line between the knots either side of y, the end values beyond the first and the last knot (NEVER extrapolated).
    Rgb V(double y) const {
        Rgb out{0.0, 0.0, 0.0};
        for (std::size_t c = 0; c < 3; ++c) {
            if (y <= ky.front()) {
                out[c] = kv.front()[c];
            } else if (y >= ky.back()) {
                out[c] = kv.back()[c];
            } else {
                const std::size_t j = static_cast<std::size_t>(std::upper_bound(ky.begin(), ky.end(), y) - ky.begin()) - 1;
                const double slope = (kv[j + 1][c] - kv[j][c]) / (ky[j + 1] - ky[j]);
                out[c] = slope * (y - ky[j]) + kv[j][c];
            }
        }
        return out;
    }
};

// The model, from the picture's own text-free pixels: `vTop` are the rows V is read from the centre columns of (the checks build a second model from fewer of them).
bool BuildPlateModel(const Image& src, const Erase& e, const Span& vTop, PlateModel& m, std::string& why) {
    m = PlateModel{};
    const auto px = [&src](int x, int y, std::size_t c) { return static_cast<double>(src.At(x, y)[c]); };
    const std::vector<int> rows = RowsOf(e.refRows);
    if (rows.empty()) {
        why = "the plaque has no text-free rows to read the vignette from";
        return false;
    }
    m.ix0 = e.interiorCols.a;
    const int cols = e.interiorCols.b - e.interiorCols.a;
    std::vector<Rgb> centre(rows.size());
    for (std::size_t r = 0; r < rows.size(); ++r) centre[r] = MeanOver(src, e.centerCols, rows[r]);
    // H(x): the median over the text-free rows of the pixel over that row's centre mean
    m.h.assign(Z(cols), Rgb{0.0, 0.0, 0.0});
    std::vector<double> samples(rows.size());
    for (int x = e.interiorCols.a; x < e.interiorCols.b; ++x) {
        for (std::size_t c = 0; c < 3; ++c) {
            for (std::size_t r = 0; r < rows.size(); ++r) samples[r] = px(x, rows[r], c) / std::max(centre[r][c], 1e-6);
            m.h[Z(x - m.ix0)][c] = Median(samples);
        }
    }
    // columns a medal covers in every text-free row take their mirror column's value; the flat middle is exactly one by construction
    if (e.hasMirror) {
        for (int x = e.mirrorCols.a; x < e.mirrorCols.b; ++x) m.h[Z(x - m.ix0)] = m.h[Z((e.mirrorAxis - x) - m.ix0)];
    }
    for (int x = e.centerCols.a; x < e.centerCols.b; ++x) m.h[Z(x - m.ix0)] = Rgb{1.0, 1.0, 1.0};
    if (e.hasFlat) {
        for (int x = e.flatCols.a; x < e.flatCols.b; ++x) m.h[Z(x - m.ix0)] = Rgb{1.0, 1.0, 1.0};
    }
    // F(x): a straight frame does not change down the plate
    m.frame.assign(Z(src.width), Rgb{0.0, 0.0, 0.0});
    m.isFrame.assign(Z(src.width), 0);
    for (const Span& span : e.frameCols) {
        for (int x = span.a; x < span.b; ++x) {
            for (std::size_t c = 0; c < 3; ++c) {
                for (std::size_t r = 0; r < rows.size(); ++r) samples[r] = px(x, rows[r], c);
                m.frame[Z(x)][c] = Median(samples);
            }
            m.isFrame[Z(x)] = 1;
        }
    }
    // V's knots: the clean rows above (the mean of the centre columns), then the rows beside the lettering whose two margin columns agree
    for (int y = vTop.a; y < vTop.b; ++y) {
        m.ky.push_back(y);
        m.kv.push_back(MeanOver(src, e.centerCols, y));
    }
    const Span left = e.refCols[0];
    const Span right = e.refCols[1];
    std::vector<double> side;
    for (int y = e.vMarginRows.a; y < e.vMarginRows.b; ++y) {
        Rgb vl{0.0, 0.0, 0.0};
        Rgb vr{0.0, 0.0, 0.0};
        for (std::size_t c = 0; c < 3; ++c) {
            side.clear();
            for (int x = left.a; x < left.b; ++x) side.push_back(px(x, y, c) / m.h[Z(x - m.ix0)][c]);
            vl[c] = Median(side);
            side.clear();
            for (int x = right.a; x < right.b; ++x) side.push_back(px(x, y, c) / m.h[Z(x - m.ix0)][c]);
            vr[c] = Median(side);
        }
        if (MaxAbs(vl, vr) > e.agreeTol) continue;
        m.ky.push_back(y);
        m.kv.push_back(Rgb{(vl[0] + vr[0]) / 2.0, (vl[1] + vr[1]) / 2.0, (vl[2] + vr[2]) / 2.0});
        m.acceptedMargin.push_back(y);
    }
    return true;
}

// The pixels a medal and its ring cover: chromatic ones (the plate is neutral) and every pixel within `radius` of one, as the square a Chebyshev ball is.
std::vector<char> ChromaKeep(const Image& src, const Chroma& chroma, int& count) {
    std::vector<char> chromatic(Z(src.width) * Z(src.height), 0);
    for (int y = 0; y < src.height; ++y) {
        for (int x = 0; x < src.width; ++x) {
            const unsigned char* p = src.At(x, y);
            const int spread = std::max({p[0], p[1], p[2]}) - std::min({p[0], p[1], p[2]});
            chromatic[Z(y) * Z(src.width) + Z(x)] = (spread > chroma.threshold && p[3] > 0) ? 1 : 0;
        }
    }
    std::vector<char> across(chromatic.size(), 0);
    for (int y = 0; y < src.height; ++y) {
        for (int x = 0; x < src.width; ++x) {
            for (int d = -chroma.radius; d <= chroma.radius; ++d) {
                const int sx = x + d;
                if (sx >= 0 && sx < src.width && chromatic[Z(y) * Z(src.width) + Z(sx)] != 0) {
                    across[Z(y) * Z(src.width) + Z(x)] = 1;
                    break;
                }
            }
        }
    }
    std::vector<char> keep(chromatic.size(), 0);
    count = 0;
    for (int y = 0; y < src.height; ++y) {
        for (int x = 0; x < src.width; ++x) {
            for (int d = -chroma.radius; d <= chroma.radius; ++d) {
                const int sy = y + d;
                if (sy >= 0 && sy < src.height && across[Z(sy) * Z(src.width) + Z(x)] != 0) {
                    keep[Z(y) * Z(src.width) + Z(x)] = 1;
                    ++count;
                    break;
                }
            }
        }
    }
    return keep;
}

// The plaque's guards: the picture must be what the model says it is, or the English picture is kept. Each names itself in its reason.
bool PlaqueChecks(const Recipe& recipe, const Image& src, const PlateModel& model, const std::vector<char>& keep, int kept, std::string& reason) {
    const Erase& e = recipe.erase;
    const PlaqueGuard& g = e.plaque;
    const std::vector<int> rows = RowsOf(e.refRows);
    const auto skipped = [&e](int x) { return e.hasSkip && x >= e.skipCols.a && x < e.skipCols.b; };
    // (1) separability: V(y) H(x), V being the row's own centre mean, predicts the text-free rows
    double sum = 0.0;
    double worst = 0.0;
    long long count = 0;
    for (const int y : rows) {
        const Rgb vy = MeanOver(src, e.centerCols, y);
        for (int x = e.interiorCols.a; x < e.interiorCols.b; ++x) {
            if (skipped(x)) continue;
            for (std::size_t c = 0; c < 3; ++c) {
                const double err = std::fabs(vy[c] * model.h[Z(x - model.ix0)][c] - static_cast<double>(src.At(x, y)[c]));
                sum += err;
                worst = std::max(worst, err);
                ++count;
            }
        }
    }
    const double mean = count > 0 ? sum / static_cast<double>(count) : 0.0;
    if (mean > g.sepMean || worst > g.sepMax) {
        reason = Fmt("separability: V(y) H(x) predicts the text-free rows to a mean of %.2f and at worst %.2f grey levels (the limits are %.2f and %.2f): the picture is not this plaque", mean, worst,
                     g.sepMean, g.sepMax);
        return false;
    }
    // (2) hold-out: V made from every other top row predicts the rest
    std::vector<int> top;
    for (int y = e.vTopRows.a; y < e.vTopRows.b; ++y) top.push_back(y);
    double holdout = 0.0;
    {
        std::vector<double> xs;
        std::vector<Rgb> vs;
        for (std::size_t i = 0; i < top.size(); i += 2) {
            xs.push_back(top[i]);
            vs.push_back(MeanOver(src, e.centerCols, top[i]));
        }
        PlateModel sparse;
        sparse.ky = xs;
        sparse.kv = vs;
        for (std::size_t i = 1; i < top.size(); i += 2) holdout = std::max(holdout, MaxAbs(sparse.V(top[i]), MeanOver(src, e.centerCols, top[i])));
    }
    if (holdout > g.holdout) {
        reason = Fmt("hold-out: V made from every other clean row predicts the rest to %.2f grey levels (the limit is %.2f): the plate is not a smooth profile", holdout, g.holdout);
        return false;
    }
    // (3) the margin rows whose two margins agree, over the window beside the old lettering
    int agreeing = 0;
    for (const int y : model.acceptedMargin) agreeing += (y >= g.marginWindow.a && y < g.marginWindow.b) ? 1 : 0;
    const double ratio = static_cast<double>(agreeing) / static_cast<double>(g.marginWindow.b - g.marginWindow.a);
    if (ratio < g.marginRatio) {
        reason = Fmt("margin rows: the two margin columns agree on %.0f%% of the rows beside the lettering (the recipe needs %.0f%%): there is ink in a margin", ratio * 100.0, g.marginRatio * 100.0);
        return false;
    }
    // (4) the ring: the three rows above the first rectangle, predicted by a model built WITHOUT them (and without the rows beside them), are the model's, or the old lettering or its
    // halo reaches out of the rectangle
    int firstRow = e.rects[0].y0;
    for (const Rect& r : e.rects) firstRow = std::min(firstRow, r.y0);
    const Span ringTop{e.vTopRows.a, std::min(e.vTopRows.b, firstRow - 3)};
    if (ringTop.b - ringTop.a < 2) {
        reason = "ring: no clean row is left above the first rectangle to build the model of the ring from";
        return false;
    }
    PlateModel without;
    if (!BuildPlateModel(src, e, ringTop, without, reason)) return false;
    double ring = 0.0;
    const int xa = e.frameCols.front().a;
    const int xb = e.frameCols.back().b;
    for (int y = firstRow - 3; y < firstRow; ++y) {
        const Rgb vy = without.V(y);
        for (int x = xa; x < xb; ++x) {
            if (keep[Z(y) * Z(src.width) + Z(x)] != 0 || skipped(x)) continue;
            Rgb predicted{0.0, 0.0, 0.0};
            if (without.isFrame[Z(x)] != 0) {
                predicted = without.frame[Z(x)];
            } else if (x >= e.interiorCols.a && x < e.interiorCols.b) {
                for (std::size_t c = 0; c < 3; ++c) predicted[c] = vy[c] * without.h[Z(x - without.ix0)][c];
            } else {
                continue;
            }
            Rgb actual{static_cast<double>(src.At(x, y)[0]), static_cast<double>(src.At(x, y)[1]), static_cast<double>(src.At(x, y)[2])};
            ring = std::max(ring, MaxAbs(predicted, actual));
        }
    }
    if (ring > g.ringMax) {
        reason = Fmt("ring: the three rows above the first rectangle differ from the model by %.2f grey levels (the limit is %.2f): ink reaches out of the rectangle", ring, g.ringMax);
        return false;
    }
    // (5) the medal: the kept pixels (chromatic ones and their rings) number what the recipe says, so that another sprite under the recipe is refused
    if (e.chroma.enabled && (kept < e.chroma.keepMin || kept > e.chroma.keepMax)) {
        reason = Fmt("chroma: %d pixels are kept as a medal's (the recipe allows %d to %d): the picture is not this plaque", kept, e.chroma.keepMin, e.chroma.keepMax);
        return false;
    }
    // (6) V's slope across a hole in its knots is its slope above the hole, to a band: a profile that bends where nothing was read is not trusted
    if (g.slopeBand > 0.0 && model.ky.size() > 2) {
        const auto level = [&model](std::size_t i) { return (model.kv[i][0] + model.kv[i][1] + model.kv[i][2]) / 3.0; };
        const std::size_t lastTop = static_cast<std::size_t>(e.vTopRows.b - e.vTopRows.a) - 1;
        // The slope above a hole is read over the last rows of the clean part, not all of it: a plate's top rows are its highlight's ramp and fall less steeply than the rows below.
        const std::size_t span = std::min<std::size_t>(kSlopeRows, lastTop);
        const double above = (level(lastTop) - level(lastTop - span)) / (model.ky[lastTop] - model.ky[lastTop - span]);
        for (std::size_t i = lastTop; i + 1 < model.ky.size(); ++i) {
            const double gap = model.ky[i + 1] - model.ky[i];
            if (gap < 8.0) continue;
            const double across = (level(i + 1) - level(i)) / gap;
            if (std::fabs(across - above) > g.slopeBand) {
                reason = Fmt("slope: V falls %.3f a row across the hole at rows %d to %d, %.3f above it (the band is %.3f)", across, static_cast<int>(model.ky[i]), static_cast<int>(model.ky[i + 1]), above,
                             g.slopeBand);
                return false;
            }
        }
    }
    return true;
}

bool ErasePlaque(const Recipe& recipe, const Image& source, Image& out, std::string& reason) {
    const Erase& e = recipe.erase;
    PlateModel model;
    if (!BuildPlateModel(source, e, e.vTopRows, model, reason)) return false;
    std::vector<char> keep(Z(source.width) * Z(source.height), 0);
    int kept = 0;
    if (e.chroma.enabled) keep = ChromaKeep(source, e.chroma, kept);
    if (!PlaqueChecks(recipe, source, model, keep, kept, reason)) return false;
    out = source;
    for (const Rect& r : e.rects) {
        for (int y = r.y0; y < r.y1; ++y) {
            const Rgb vy = model.V(y);
            for (int x = r.x0; x < r.x1; ++x) {
                if (keep[Z(y) * Z(source.width) + Z(x)] != 0) continue;
                unsigned char* px = out.At(x, y);
                for (std::size_t c = 0; c < 3; ++c) {
                    const double value = model.isFrame[Z(x)] != 0 ? model.frame[Z(x)][c] : vy[c] * model.h[Z(x - model.ix0)][c];
                    px[c] = Level(value);
                }
            }
        }
    }
    return true;
}

// The panel: H(x) from the rows that hold no ink, V(y) for EVERY row from the left margin (the right margin carries a corner effect in the upper rows and is only a check).
struct PanelModel {
    int ix0 = 0;
    std::vector<Rgb> h;
};

bool BuildPanelModel(const Image& src, const Erase& e, PanelModel& m, std::string& why) {
    m = PanelModel{};
    const std::vector<int> rows = RowsOf(e.refRows);
    if (rows.empty()) {
        why = "the panel has no ink-free rows to read the vignette from";
        return false;
    }
    m.ix0 = e.interiorCols.a;
    const int cols = e.interiorCols.b - e.interiorCols.a;
    std::vector<Rgb> centre(rows.size());
    for (std::size_t r = 0; r < rows.size(); ++r) centre[r] = MeanOver(src, e.centerCols, rows[r]);
    m.h.assign(Z(cols), Rgb{0.0, 0.0, 0.0});
    std::vector<double> samples(rows.size());
    for (int x = e.interiorCols.a; x < e.interiorCols.b; ++x) {
        for (std::size_t c = 0; c < 3; ++c) {
            for (std::size_t r = 0; r < rows.size(); ++r) samples[r] = static_cast<double>(src.At(x, rows[r])[c]) / std::max(centre[r][c], 1e-6);
            m.h[Z(x - m.ix0)][c] = Median(samples);
        }
    }
    for (int x = e.centerCols.a; x < e.centerCols.b; ++x) m.h[Z(x - m.ix0)] = Rgb{1.0, 1.0, 1.0};
    return true;
}

// V(y) as the margin `side` (0 left, 1 right) says it is on row y.
Rgb PanelV(const Image& src, const Erase& e, const PanelModel& m, int side, int y) {
    const Span cols = e.refCols[Z(side)];
    Rgb v{0.0, 0.0, 0.0};
    std::vector<double> samples;
    for (std::size_t c = 0; c < 3; ++c) {
        samples.clear();
        for (int x = cols.a; x < cols.b; ++x) samples.push_back(static_cast<double>(src.At(x, y)[c]) / m.h[Z(x - m.ix0)][c]);
        v[c] = Median(samples);
    }
    return v;
}

bool PanelChecks(const Image& src, const Erase& e, const PanelModel& m, std::string& reason) {
    const PanelGuard& g = e.panel;
    const std::vector<int> rows = RowsOf(e.refRows);
    double sep = 0.0;
    double vMargin = 0.0;
    for (const int y : rows) {
        const Rgb vy = MeanOver(src, e.centerCols, y);
        for (int x = e.interiorCols.a; x < e.interiorCols.b; ++x) {
            for (std::size_t c = 0; c < 3; ++c) sep = std::max(sep, std::fabs(vy[c] * m.h[Z(x - m.ix0)][c] - static_cast<double>(src.At(x, y)[c])));
        }
        vMargin = std::max(vMargin, MaxAbs(PanelV(src, e, m, 0, y), vy));
    }
    // The margin first: ink in a margin column is also a failure of the separability (the vignette is read from every interior column), and the reason should name the cause.
    if (vMargin > g.vMargin) {
        reason = Fmt("margin: the left margin's profile differs from the centre's by %.2f grey levels on the ink-free rows (the limit is %.2f): there is ink in the margin", vMargin, g.vMargin);
        return false;
    }
    if (sep > g.sepMax) {
        reason = Fmt("separability: V(y) H(x) predicts the ink-free rows to %.2f grey levels at worst (the limit is %.2f): the picture is not this panel", sep, g.sepMax);
        return false;
    }
    int agreeing = 0;
    for (int y = e.rect.y0; y < e.rect.y1; ++y) agreeing += MaxAbs(PanelV(src, e, m, 0, y), PanelV(src, e, m, 1, y)) <= e.agreeTol ? 1 : 0;
    const double ratio = static_cast<double>(agreeing) / static_cast<double>(e.rect.Height());
    if (ratio < g.agreeRatio) {
        reason = Fmt("margin rows: the two margins agree on %.0f%% of the rectangle's rows (the recipe needs %.0f%%)", ratio * 100.0, g.agreeRatio * 100.0);
        return false;
    }
    double ring = 0.0;
    for (int k = 1; k <= 3; ++k) {
        for (const int y : {e.rect.y0 - k, e.rect.y1 - 1 + k}) ring = std::max(ring, MaxAbs(PanelV(src, e, m, 0, y), MeanOver(src, e.centerCols, y)));
    }
    if (ring > g.ringMax) {
        reason = Fmt("ring: the three rows above and below the rectangle differ from the model by %.2f grey levels (the limit is %.2f): ink reaches out of the rectangle", ring, g.ringMax);
        return false;
    }
    return true;
}

bool ErasePanel(const Recipe& recipe, const Image& source, Image& out, std::string& reason) {
    const Erase& e = recipe.erase;
    PanelModel model;
    if (!BuildPanelModel(source, e, model, reason) || !PanelChecks(source, e, model, reason)) return false;
    out = source;
    for (int y = e.rect.y0; y < e.rect.y1; ++y) {
        const Rgb v = PanelV(source, e, model, 0, y);
        for (int x = e.rect.x0; x < e.rect.x1; ++x) {
            unsigned char* px = out.At(x, y);
            for (std::size_t c = 0; c < 3; ++c) px[c] = Level(v[c] * model.h[Z(x - model.ix0)][c]);
        }
    }
    return true;
}

bool RowIsTransparent(const Image& src, int y, int& at) {
    for (int x = 0; x < src.width; ++x) {
        if (src.At(x, y)[3] != 0) {
            at = x;
            return false;
        }
    }
    return true;
}

// Cleared rows: each band is bounded by a fully transparent row above and below (so a clear can never cut a name or a logo), and the ink windows of the text hold no opaque pixel
// outside the cleared rows (so a word drawn in a window can never overwrite one).
bool EraseClearRows(const Recipe& recipe, const Image& source, Image& out, std::string& reason) {
    const Erase& e = recipe.erase;
    int at = 0;
    for (const Span& span : e.clearRows) {
        for (const int y : {span.a - 1, span.b}) {
            if (!RowIsTransparent(source, y, at)) {
                reason = Fmt("the cleared rows %d to %d are not bounded by transparent rows: row %d holds an opaque pixel at x %d", span.a, span.b - 1, y, at);
                return false;
            }
        }
    }
    const auto cleared = [&e](int y) {
        for (const Span& span : e.clearRows) {
            if (y >= span.a && y < span.b) return true;
        }
        return false;
    };
    for (std::size_t b = 0; b < recipe.text.bands.size(); ++b) {
        const Span& window = recipe.text.bands[b].inkRows;
        for (int y = window.a; y < window.b; ++y) {
            if (cleared(y)) continue;
            if (!RowIsTransparent(source, y, at)) {
                reason = Fmt("the ink window of band %d holds an opaque pixel at (%d, %d) outside the cleared rows: drawing there would overwrite it", static_cast<int>(b), at, y);
                return false;
            }
        }
    }
    out = source;
    for (const Span& span : e.clearRows) {
        for (int y = span.a; y < span.b; ++y) std::memset(out.At(0, y), 0, Z(source.width) * 4);
    }
    return true;
}

} // namespace

bool CheckSource(const Recipe& recipe, const Image& source, std::string& reason) {
    if (source.Empty()) {
        reason = "the picture is not there or cannot be read";
        return false;
    }
    if (source.width != recipe.width || source.height != recipe.height) {
        reason = Fmt("the picture is %d x %d and the recipe was measured on %d x %d", source.width, source.height, recipe.width, recipe.height);
        return false;
    }
    const std::string digest = Digest(source);
    if (digest != recipe.digest) {
        reason = "the picture is not the one the recipe was measured on (its digest is " + digest + ", the recipe's " + recipe.digest + ")";
        return false;
    }
    return true;
}

bool EraseWords(const Recipe& recipe, const Image& source, const Image* twin, Image& out, std::string& reason) {
    const Erase& erase = recipe.erase;
    if (erase.rule == Rule::Generated) {
        out = Blank(recipe.width, recipe.height);
        return !out.Empty();
    }
    if (source.Empty() || source.width != recipe.width || source.height != recipe.height) {
        reason = "the picture is not the recipe's size";
        return false;
    }
    if (erase.rule == Rule::Plaque) return ErasePlaque(recipe, source, out, reason);
    if (erase.rule == Rule::Panel) return ErasePanel(recipe, source, out, reason);
    if (erase.rule == Rule::ClearRows) return EraseClearRows(recipe, source, out, reason);
    if (erase.rule == Rule::Twin) {
        if (twin == nullptr || twin->Empty()) {
            reason = "the twin picture " + erase.twinSprite + " is not there or cannot be read";
            return false;
        }
        const std::string digest = Digest(*twin);
        if (digest != erase.twinDigest) {
            reason = "the twin picture " + erase.twinSprite + " is not the one the recipe was measured on (its digest is " + digest + ")";
            return false;
        }
        Image shifted;
        ShiftedTwin(*twin, erase.shiftX, erase.shiftY, recipe.width, recipe.height, shifted);
        // The guard: bit-exact everywhere but in the word's window, and different inside it (a wrong picture is the same nowhere or different everywhere).
        long long outside = 0;
        long long inside = 0;
        for (int y = 0; y < recipe.height; ++y) {
            for (int x = 0; x < recipe.width; ++x) {
                if (SamePixel(shifted.At(x, y), source.At(x, y))) continue;
                if (erase.checkRect.Contains(x, y)) ++inside;
                else ++outside;
            }
        }
        if (outside != 0) {
            reason = Fmt("the twin %s differs from the picture in %lld pixels outside the word's window", erase.twinSprite.c_str(), outside);
            return false;
        }
        if (inside < erase.minInside) {
            reason = Fmt("the twin %s differs from the picture in only %lld pixels inside the word's window (the recipe wants %d)", erase.twinSprite.c_str(), inside, erase.minInside);
            return false;
        }
        out = source;
        for (int y = erase.rect.y0; y < erase.rect.y1; ++y) {
            std::memcpy(out.At(erase.rect.x0, y), shifted.At(erase.rect.x0, y), Z(erase.rect.Width()) * 4);
        }
        return true;
    }

    std::vector<unsigned char> fill;
    if (!ModelFill(source, erase, erase.rect, 0, fill, reason)) return false;

    // The guards: the picture's own text-free pixels must agree with the model they are the evidence for.
    if (erase.rule == Rule::RowLerp) {
        const Span a = erase.refCols[0];
        const Span b = erase.refCols[1];
        double worst = 0.0;
        for (int y = erase.rect.y0; y < erase.rect.y1; ++y) {
            for (int c = 0; c < 4; ++c) {
                double ml = 0.0;
                double mr = 0.0;
                for (int x = a.a; x < a.b; ++x) ml += source.At(x, y)[c];
                for (int x = b.a; x < b.b; ++x) mr += source.At(x, y)[c];
                worst = std::max(worst, std::fabs(ml / (a.b - a.a) - mr / (b.b - b.a)));
            }
        }
        if (worst > erase.guard.agreeMax) {
            reason = Fmt("the two reference bands differ by %.2f grey levels on a row (the limit is %.2f): they are not both free of the word", worst, erase.guard.agreeMax);
            return false;
        }
    } else {
        std::vector<unsigned char> fromLeft;
        std::vector<unsigned char> fromRight;
        if (!ModelFill(source, erase, erase.rect, 1, fromLeft, reason) || !ModelFill(source, erase, erase.rect, 2, fromRight, reason)) return false;
        double sum = 0.0;
        int worst = 0;
        const std::size_t pixels = Z(erase.rect.Width()) * Z(erase.rect.Height());
        for (std::size_t i = 0; i < pixels; ++i) {
            int d = 0;
            for (std::size_t c = 0; c < 4; ++c) d = std::max(d, std::abs(static_cast<int>(fromLeft[i * 4 + c]) - static_cast<int>(fromRight[i * 4 + c])));
            sum += d;
            worst = std::max(worst, d);
        }
        const double mean = sum / static_cast<double>(pixels);
        if (mean > erase.guard.holdoutMean || worst > erase.guard.holdoutMax) {
            reason = Fmt("the fill from the left band alone and from the right alone differ by %.2f on average and %d at most (the limits are %.2f and %.2f)", mean, worst,
                         erase.guard.holdoutMean, erase.guard.holdoutMax);
            return false;
        }
    }
    // The collar: the model, made over a rectangle grown by kCollar on the sides and the top (and the bottom of a row-by-row fill), must agree with the picture in the
    // ring it adds. If the word's shadow reached the ring the rectangle is too small.
    Rect grown{erase.rect.x0 - kCollar, erase.rect.y0 - kCollar, erase.rect.x1 + kCollar, erase.rect.y1 + (erase.rule == Rule::RowLerp ? kCollar : 0)};
    std::vector<unsigned char> big;
    if (!ModelFill(source, erase, grown, 0, big, reason)) return false;
    int ringWorst = 0;
    for (int y = grown.y0; y < grown.y1; ++y) {
        for (int x = grown.x0; x < grown.x1; ++x) {
            if (erase.rect.Contains(x, y)) continue;
            const unsigned char* model = &big[(Z(y - grown.y0) * Z(grown.Width()) + Z(x - grown.x0)) * 4];
            for (int c = 0; c < 4; ++c) ringWorst = std::max(ringWorst, std::abs(static_cast<int>(source.At(x, y)[c]) - static_cast<int>(model[c])));
        }
    }
    if (ringWorst > erase.guard.collarMax) {
        reason = Fmt("the picture differs from the model by %d grey levels just outside the rectangle (the limit is %.2f): the English word reaches out of it", ringWorst,
                     erase.guard.collarMax);
        return false;
    }
    out = source;
    for (int y = erase.rect.y0; y < erase.rect.y1; ++y) {
        std::memcpy(out.At(erase.rect.x0, y), &fill[Z(y - erase.rect.y0) * Z(erase.rect.Width()) * 4], Z(erase.rect.Width()) * 4);
    }
    return true;
}

// ============================================================================================================================
// the words
// ============================================================================================================================

namespace {

// A face of the slot's routing, sized for one recipe.
struct Face {
    std::string id;
    stbtt_fontinfo info{};
    double ratio = 1.0;
    double embolden = 0.0;
    double referenceUnits = 0.0; // the top of the reference glyph's box, in font units
    double referenceMul = 1.0;   // FaceSpec::referenceHeight, when the plan's own reference glyph is the measure
};

// What a bake works with: the recipe's text block, the plan's routing, the faces loaded, the language.
struct Set {
    const Text* text = nullptr;
    const FontBake::Plan* plan = nullptr;
    const FontBake::SlotSpec* slot = nullptr;
    std::vector<Face> faces;
    Strings::Lang lang = Strings::kDefaultLang;
    int width = 0;  // the picture
    int height = 0;

    const Face* FaceOf(std::uint32_t codePoint) const {
        const std::string* id = &FontBake::FaceIdOf(*plan, *slot, codePoint);
        // The recipe may draw the ASCII digits from another face (Kurale's 3 reads like an 8 once emboldened at the strip's size).
        if (!text->digitFace.empty() && codePoint >= '0' && codePoint <= '9') id = &text->digitFace;
        for (const Face& face : faces) {
            if (face.id == *id) return &face;
        }
        return nullptr;
    }
    // Pixels (at the picture's size) one font unit of `face` is, at fit scale `scale`.
    double UnitScale(const Face& face, double scale) const { return text->px * scale * face.ratio * face.referenceMul / face.referenceUnits; }
    // The embolden of `face`, in supersampled pixels, as a whole number of steps.
    int EmboldenSteps(const Face& face, double scale) const { return RoundInt(face.embolden * text->px * scale * kSupersample); }
};

bool MakeSet(const Request& request, const Text& text, Set& set, std::string& why) {
    if (request.recipe == nullptr || request.plan == nullptr || request.faces == nullptr) {
        why = "the request has no recipe, plan or faces";
        return false;
    }
    set.text = &text;
    set.plan = request.plan;
    set.lang = request.lang;
    set.width = request.recipe->width;
    set.height = request.recipe->height;
    set.slot = request.plan->Slot(set.text->slot);
    if (set.slot == nullptr) {
        why = "the plan has no slot '" + set.text->slot + "'";
        return false;
    }
    std::vector<std::string> ids{set.slot->textFace};
    for (const auto& [script, face] : set.slot->scriptFaces) {
        (void)script;
        if (std::find(ids.begin(), ids.end(), face) == ids.end()) ids.push_back(face);
    }
    if (!set.text->digitFace.empty() && std::find(ids.begin(), ids.end(), set.text->digitFace) == ids.end()) ids.push_back(set.text->digitFace);
    for (const FaceTuning& tuning : set.text->faces) {
        if (std::find(ids.begin(), ids.end(), tuning.face) == ids.end()) {
            why = "the recipe tunes the face '" + tuning.face + "', which the slot '" + set.slot->id + "' does not draw with";
            return false;
        }
    }
    for (const std::string& id : ids) {
        const FontBake::FaceSpec* spec = request.plan->Face(id);
        const FontBake::FaceBytes* bytes = nullptr;
        for (const FontBake::FaceBytes& candidate : *request.faces) {
            if (candidate.id == id) bytes = &candidate;
        }
        if (spec == nullptr || bytes == nullptr || bytes->data.empty()) {
            why = "the face '" + id + "' is not in the plan or was not read";
            return false;
        }
        Face face;
        face.id = id;
        const int offset = stbtt_GetFontOffsetForIndex(bytes->data.data(), 0);
        if (offset < 0 || stbtt_InitFont(&face.info, bytes->data.data(), offset) == 0) {
            why = spec->file + ": not a TrueType font";
            return false;
        }
        std::uint32_t reference = spec->reference;
        face.referenceMul = spec->referenceHeight;
        for (const FaceTuning& tuning : set.text->faces) {
            if (tuning.face != id) continue;
            face.ratio = tuning.ratio;
            face.embolden = tuning.embolden;
            if (tuning.reference != 0) {
                reference = tuning.reference;
                face.referenceMul = 1.0;
            }
        }
        int x0 = 0;
        int y0 = 0;
        int x1 = 0;
        int y1 = 0;
        if (stbtt_FindGlyphIndex(&face.info, static_cast<int>(reference)) == 0 || stbtt_GetCodepointBox(&face.info, static_cast<int>(reference), &x0, &y0, &x1, &y1) == 0 ||
            y1 <= 0) {
            why = spec->file + ": has no glyph to measure its size by (U+" + HexOf(reference) + ")";
            return false;
        }
        face.referenceUnits = static_cast<double>(y1);
        set.faces.push_back(std::move(face));
    }
    return true;
}

// ---- the words as lines -------------------------------------------------------------------------------------------------------------------

// A phrase as the table holds it, cut into the pieces its break opportunities leave: pieces[i] is followed by separators[i] (a space, or U+200B), and the last has none.
struct Phrase {
    std::vector<std::string> pieces;
    std::vector<std::uint32_t> separators;
};

// With BreakAt::AfterDash only the space after a dash that stands alone (" - ") is a place to break, so that a dash ends a line and never starts one; U+200B always is.
Phrase Pieces(std::string_view phrase, BreakAt breakAt = BreakAt::Space) {
    Phrase out;
    std::vector<std::uint32_t> points = Utf8::Decode(phrase);
    std::vector<std::uint32_t> current;
    for (std::size_t i = 0; i < points.size(); ++i) {
        const std::uint32_t point = points[i];
        const bool afterDash = i >= 2 && points[i - 1] == '-' && points[i - 2] == kSpace;
        if (point == kZeroWidthSpace || (point == kSpace && (breakAt == BreakAt::Space || afterDash))) {
            out.pieces.push_back(Utf8::Encode(current));
            out.separators.push_back(point);
            current.clear();
        } else {
            current.push_back(point);
        }
    }
    out.pieces.push_back(Utf8::Encode(current));
    return out;
}

// The pieces [from, to) as one line of text, in logical order: the separators between them are kept (a drawn space, or the invisible break), and the line is not
// shaped yet.
std::string JoinPieces(const Phrase& phrase, std::size_t from, std::size_t to) {
    std::string line;
    for (std::size_t i = from; i < to; ++i) {
        line += phrase.pieces[i];
        if (i + 1 < to) line += Utf8::Encode(phrase.separators[i]);
    }
    return line;
}

// One line, ready to draw: break-only characters taken out (U+200B anywhere, and the ASCII space in Japanese), then the language's display step (Arabic shaped and put
// in visual order), as the code points that will be drawn from left to right. The breaks were made in logical order BEFORE this, so the words of an Arabic phrase are on
// the lines they belong on.
std::vector<std::uint32_t> ShowLine(Strings::Lang lang, const std::string& logical, bool japaneseSpaces = false) {
    std::vector<std::uint32_t> kept;
    for (const std::uint32_t point : Utf8::Decode(logical)) {
        if (point == kZeroWidthSpace) continue;
        if (point == kSpace && lang == Strings::Lang::Ja && !japaneseSpaces) continue;
        kept.push_back(point);
    }
    return Utf8::Decode(Strings::Display(lang, Utf8::Encode(kept)));
}

bool IsGap(std::uint32_t point) {
    return point == kSpace || point == kNoBreakSpace;
}

// One glyph set down on a line, before the raster exists.
struct Run {
    std::uint32_t codePoint = 0;
    const Face* face = nullptr;
    int glyph = 0;
    bool draws = false;
    double pen = 0.0;     // where its pen is, along the line, in supersampled pixels
    double advance = 0.0;
    double unitScale = 0.0; // supersampled pixels a font unit is
    int ix0 = 0;
    int iy0 = 0;
    int ix1 = 0;
    int iy1 = 0;
};

// The glyphs of a line at `scale`: faces, indices and advances. False, with `why`, when a code point has no glyph in its face.
bool SetRuns(const Set& set, const std::vector<std::uint32_t>& points, double scale, std::vector<Run>& runs, double& advance, std::string& why) {
    runs.clear();
    double pen = 0.0;
    for (const std::uint32_t point : points) {
        Run run;
        run.codePoint = point;
        run.face = set.FaceOf(point);
        if (run.face == nullptr) {
            why = "no face is routed for U+" + HexOf(point);
            return false;
        }
        run.unitScale = set.UnitScale(*run.face, scale) * kSupersample;
        if (IsGap(point)) {
            // A gap is the face's own space (a face with no no-break space uses its space's advance).
            int glyph = point == kNoBreakSpace ? stbtt_FindGlyphIndex(&run.face->info, static_cast<int>(kNoBreakSpace)) : 0;
            if (glyph == 0) glyph = stbtt_FindGlyphIndex(&run.face->info, static_cast<int>(kSpace));
            if (glyph == 0) {
                why = "the face " + run.face->id + " has no space";
                return false;
            }
            run.glyph = glyph;
        } else {
            run.glyph = stbtt_FindGlyphIndex(&run.face->info, static_cast<int>(point));
            if (run.glyph == 0) {
                why = "no glyph for U+" + HexOf(point) + " in the face " + run.face->id;
                return false;
            }
            run.draws = true;
        }
        int advanceUnits = 0;
        int bearing = 0;
        stbtt_GetGlyphHMetrics(&run.face->info, run.glyph, &advanceUnits, &bearing);
        run.advance = static_cast<double>(advanceUnits) * run.unitScale;
        run.pen = pen;
        pen += run.advance;
        if (run.draws) {
            const float sc = static_cast<float>(run.unitScale);
            stbtt_GetGlyphBitmapBox(&run.face->info, run.glyph, sc, sc, &run.ix0, &run.iy0, &run.ix1, &run.iy1);
        }
        runs.push_back(run);
    }
    advance = pen;
    return true;
}

// ---- rasters --------------------------------------------------------------------------------------------------------------------------------

// A line drawn at kSupersample times the picture's pixels. The pen starts at column penX and the baseline is row baseY, both whole pixels of the picture, so the
// raster can be put down on the picture's own grid.
struct LineRaster {
    int w = 0;
    int h = 0;
    int penX = 0;
    int baseY = 0;
    std::vector<unsigned char> a;
    std::vector<Run> runs;
};

// n steps of grey dilation, each the maximum over the pixel and its 4 neighbours (even steps) or 8 (odd ones): an octagon of reach n. The dilation of a union is the
// union of the dilations, so glyphs can be thickened one at a time.
void Dilate(std::vector<unsigned char>& cell, int w, int h, int steps) {
    std::vector<unsigned char> next(cell.size());
    for (int step = 0; step < steps; ++step) {
        const bool diagonals = step % 2 == 1;
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                unsigned char m = cell[Z(y) * Z(w) + Z(x)];
                if (x > 0) m = std::max(m, cell[Z(y) * Z(w) + Z(x - 1)]);
                if (x + 1 < w) m = std::max(m, cell[Z(y) * Z(w) + Z(x + 1)]);
                if (y > 0) m = std::max(m, cell[Z(y - 1) * Z(w) + Z(x)]);
                if (y + 1 < h) m = std::max(m, cell[Z(y + 1) * Z(w) + Z(x)]);
                if (diagonals) {
                    if (x > 0 && y > 0) m = std::max(m, cell[Z(y - 1) * Z(w) + Z(x - 1)]);
                    if (x + 1 < w && y > 0) m = std::max(m, cell[Z(y - 1) * Z(w) + Z(x + 1)]);
                    if (x > 0 && y + 1 < h) m = std::max(m, cell[Z(y + 1) * Z(w) + Z(x - 1)]);
                    if (x + 1 < w && y + 1 < h) m = std::max(m, cell[Z(y + 1) * Z(w) + Z(x + 1)]);
                }
                next[Z(y) * Z(w) + Z(x)] = m;
            }
        }
        cell.swap(next);
    }
}

bool DrawLine(const Set& set, const std::vector<std::uint32_t>& points, double scale, LineRaster& out, std::string& why) {
    std::vector<Run> runs;
    double advance = 0.0;
    if (!SetRuns(set, points, scale, runs, advance, why)) return false;
    int minX = std::numeric_limits<int>::max();
    int maxX = std::numeric_limits<int>::min();
    int minY = std::numeric_limits<int>::max();
    int maxY = std::numeric_limits<int>::min();
    for (const Run& run : runs) {
        if (!run.draws || run.ix1 <= run.ix0 || run.iy1 <= run.iy0) continue;
        const int n = set.EmboldenSteps(*run.face, scale);
        const int origin = RoundInt(run.pen);
        minX = std::min(minX, origin + run.ix0 - n);
        maxX = std::max(maxX, origin + run.ix1 + n);
        minY = std::min(minY, run.iy0 - n);
        maxY = std::max(maxY, run.iy1 + n);
    }
    if (minX > maxX) {
        why = "the phrase has nothing to draw";
        return false;
    }
    // Whole picture pixels of room on every side, the pen and the baseline on the picture's grid.
    const int penX = AlignUp(std::max(0, -minX) + 2, kSupersample);
    const int baseY = AlignUp(std::max(0, -minY) + 2, kSupersample);
    const int w = AlignUp(penX + std::max(maxX, static_cast<int>(std::ceil(advance))) + 2, kSupersample);
    const int h = AlignUp(baseY + maxY + 2, kSupersample);
    if (w > kMaxSide * kSupersample || h > kMaxSide * kSupersample) {
        why = "the phrase is far larger than any picture";
        return false;
    }
    LineRaster raster;
    raster.w = w;
    raster.h = h;
    raster.penX = penX;
    raster.baseY = baseY;
    raster.a.assign(Z(w) * Z(h), 0);
    for (const Run& run : runs) {
        if (!run.draws || run.ix1 <= run.ix0 || run.iy1 <= run.iy0) continue;
        const int bw = run.ix1 - run.ix0;
        const int bh = run.iy1 - run.iy0;
        const int n = set.EmboldenSteps(*run.face, scale);
        const int cw = bw + 2 * n;
        const int ch = bh + 2 * n;
        std::vector<unsigned char> bitmap(Z(bw) * Z(bh), 0);
        const float sc = static_cast<float>(run.unitScale);
        stbtt_MakeGlyphBitmap(&run.face->info, bitmap.data(), bw, bh, bw, sc, sc, run.glyph);
        std::vector<unsigned char> cell(Z(cw) * Z(ch), 0);
        for (int y = 0; y < bh; ++y) std::memcpy(&cell[Z(y + n) * Z(cw) + Z(n)], &bitmap[Z(y) * Z(bw)], Z(bw));
        if (n > 0) Dilate(cell, cw, ch, n);
        const int left = penX + RoundInt(run.pen) + run.ix0 - n;
        const int top = baseY + run.iy0 - n;
        for (int y = 0; y < ch; ++y) {
            const int ty = top + y;
            if (ty < 0 || ty >= h) continue;
            for (int x = 0; x < cw; ++x) {
                const int tx = left + x;
                if (tx < 0 || tx >= w) continue;
                unsigned char& target = raster.a[Z(ty) * Z(w) + Z(tx)];
                target = std::max(target, cell[Z(y) * Z(cw) + Z(x)]);
            }
        }
    }
    raster.runs = std::move(runs);
    out = std::move(raster);
    return true;
}

// The picture's coverage: floats from 0 to 1, a block of kSupersample squared bytes averaged to one value. The cells are those of a window of the picture whose first cell is the
// picture pixel (x0, y0) (the paragraph and the bands lay out on a canvas the size of their window, not of the picture); the whole-picture layouts have it at (0, 0).
struct Coverage {
    int x0 = 0;
    int y0 = 0;
    int w = 0;
    int h = 0;
    std::vector<float> v;
    // The coverage at a PICTURE pixel: zero outside the window.
    float At(int x, int y) const {
        const int lx = x - x0;
        const int ly = y - y0;
        if (lx < 0 || ly < 0 || lx >= w || ly >= h) return 0.0f;
        return v[Z(ly) * Z(w) + Z(lx)];
    }
};

Coverage Downsample(const std::vector<unsigned char>& canvas, int w, int h, int originX = 0, int originY = 0) {
    Coverage out;
    out.x0 = originX;
    out.y0 = originY;
    out.w = w / kSupersample;
    out.h = h / kSupersample;
    out.v.assign(Z(out.w) * Z(out.h), 0.0f);
    constexpr double kNorm = 255.0 * kSupersample * kSupersample;
    for (int y = 0; y < out.h; ++y) {
        for (int x = 0; x < out.w; ++x) {
            unsigned sum = 0;
            for (int j = 0; j < kSupersample; ++j) {
                const unsigned char* row = &canvas[Z(y * kSupersample + j) * Z(w) + Z(x * kSupersample)];
                for (int i = 0; i < kSupersample; ++i) sum += row[i];
            }
            out.v[Z(y) * Z(out.w) + Z(x)] = static_cast<float>(static_cast<double>(sum) / kNorm);
        }
    }
    return out;
}

// Max-composes `raster` onto `canvas` (w x h supersampled pixels) with its top-left corner at (ox, oy).
void PutLine(std::vector<unsigned char>& canvas, int w, int h, const LineRaster& raster, int ox, int oy) {
    for (int y = 0; y < raster.h; ++y) {
        const int ty = oy + y;
        if (ty < 0 || ty >= h) continue;
        for (int x = 0; x < raster.w; ++x) {
            const int tx = ox + x;
            if (tx < 0 || tx >= w) continue;
            unsigned char& target = canvas[Z(ty) * Z(w) + Z(tx)];
            target = std::max(target, raster.a[Z(y) * Z(raster.w) + Z(x)]);
        }
    }
}

// The box where the coverage is above `threshold`: [x0, x1) by [y0, y1). Empty (x1 <= x0) when nothing is.
Rect InkBox(const Coverage& c, double threshold) {
    Rect box{c.x0 + c.w, c.y0 + c.h, c.x0, c.y0};
    for (int y = c.y0; y < c.y0 + c.h; ++y) {
        for (int x = c.x0; x < c.x0 + c.w; ++x) {
            if (static_cast<double>(c.At(x, y)) <= threshold) continue;
            box.x0 = std::min(box.x0, x);
            box.y0 = std::min(box.y0, y);
            box.x1 = std::max(box.x1, x + 1);
            box.y1 = std::max(box.y1, y + 1);
        }
    }
    return box;
}

// Columns of `raster` that hold any ink: [first, last + 1).
bool InkColumns(const LineRaster& raster, int& first, int& end) {
    first = raster.w;
    end = 0;
    for (int y = 0; y < raster.h; ++y) {
        for (int x = 0; x < raster.w; ++x) {
            if (raster.a[Z(y) * Z(raster.w) + Z(x)] == 0) continue;
            first = std::min(first, x);
            end = std::max(end, x + 1);
        }
    }
    return end > first;
}

// Rows of `raster` that hold solid ink: [first, last + 1).
bool SolidRows(const LineRaster& raster, int& first, int& end) {
    first = raster.h;
    end = 0;
    for (int y = 0; y < raster.h; ++y) {
        for (int x = 0; x < raster.w; ++x) {
            if (raster.a[Z(y) * Z(raster.w) + Z(x)] <= kHalf) continue;
            first = std::min(first, y);
            end = std::max(end, y + 1);
            break;
        }
    }
    return end > first;
}

// ---- the pitch of two stacked lines -----------------------------------------------------------------------------------------------------------

// For each column, the lowest solid row of the upper line and the highest solid row of the lower one (columns where there is none are skipped by a sentinel).
constexpr int kNoRow = std::numeric_limits<int>::min() / 4;

void ColumnEdges(const LineRaster& raster, std::vector<int>& top, std::vector<int>& bottom) {
    top.assign(Z(raster.w), -kNoRow);
    bottom.assign(Z(raster.w), kNoRow);
    for (int y = 0; y < raster.h; ++y) {
        for (int x = 0; x < raster.w; ++x) {
            if (raster.a[Z(y) * Z(raster.w) + Z(x)] <= kHalf) continue;
            top[Z(x)] = std::min(top[Z(x)], y);
            bottom[Z(x)] = std::max(bottom[Z(x)], y);
        }
    }
}

// The smallest baseline distance, at least `start` pixels and in steps of kPitchStep, at which the lower line's ink, grown by `gap` pixels, touches none of the upper
// line's, the two lines centred on one another by their ink as they will be drawn: a descender pushes the next line down only where an ascender really is under it.
// The growth is the same octagon the embolden is (its reach in a column at a horizontal distance d is found by growing a single pixel), applied to the lower line's
// highest solid row of each column.
// `leftAligned`: the two lines are laid with their ink's left edges on one column (the bands'), not centred on one another (a stack's and a paragraph's).
double FitPitch(const LineRaster& upper, const LineRaster& lower, double start, double gap, bool leftAligned = false) {
    std::vector<int> upperTop;
    std::vector<int> upperBottom;
    std::vector<int> lowerTop;
    std::vector<int> lowerBottom;
    ColumnEdges(upper, upperTop, upperBottom);
    ColumnEdges(lower, lowerTop, lowerBottom);
    const int n = RoundInt(gap * kSupersample);
    // The octagon: grow a point n steps, and read off how far up it reaches at each horizontal offset.
    const int side = 2 * n + 1;
    std::vector<unsigned char> element(Z(side) * Z(side), 0);
    element[Z(n) * Z(side) + Z(n)] = 255;
    Dilate(element, side, side, n);
    std::vector<int> reach(Z(side), -1);
    for (int dx = 0; dx < side; ++dx) {
        for (int y = 0; y < side; ++y) {
            if (element[Z(y) * Z(side) + Z(dx)] != 0) reach[Z(dx)] = std::max(reach[Z(dx)], n - y);
        }
    }
    // The lower line's grown top, per column of its own raster.
    std::vector<int> grown(Z(lower.w), -kNoRow);
    for (int x = 0; x < lower.w; ++x) {
        for (int d = -n; d <= n; ++d) {
            const int sx = x + d;
            if (sx < 0 || sx >= lower.w || lowerTop[Z(sx)] == -kNoRow || reach[Z(d + n)] < 0) continue;
            grown[Z(x)] = std::min(grown[Z(x)], lowerTop[Z(sx)] - reach[Z(d + n)]);
        }
    }
    int upperFirst = 0;
    int upperEnd = 0;
    int lowerFirst = 0;
    int lowerEnd = 0;
    if (!InkColumns(upper, upperFirst, upperEnd) || !InkColumns(lower, lowerFirst, lowerEnd)) return start;
    const int shift = leftAligned ? upperFirst - lowerFirst : RoundInt((upperFirst + upperEnd) / 2.0 - (lowerFirst + lowerEnd) / 2.0); // added to the lower line's columns
    // The lower line's top must stay below the upper line's bottom in every column where both are.
    int need = std::numeric_limits<int>::min(); // the least (lower row - upper row) offset that keeps them apart, in supersampled pixels
    for (int x = 0; x < upper.w; ++x) {
        const int lx = x - shift;
        if (lx < 0 || lx >= lower.w || upperBottom[Z(x)] == kNoRow || grown[Z(lx)] == -kNoRow) continue;
        need = std::max(need, upperBottom[Z(x)] - grown[Z(lx)] + 1);
    }
    if (need == std::numeric_limits<int>::min()) return start;
    // off = (upper baseline row + dy) - lower baseline row is where the lower raster's row 0 sits in the upper's frame; it must be at least `need`.
    double p = start;
    for (int i = 0; i < kMaxPitchSteps; ++i) {
        const int dy = RoundInt(p * kSupersample);
        if (upper.baseY + dy - lower.baseY >= need) return p;
        p += kPitchStep;
    }
    return p;
}

// ---- the block --------------------------------------------------------------------------------------------------------------------------------

struct Placement {
    Coverage cov;                 // the picture's size
    Rect ink;                     // where the coverage is above the recipe's threshold
    std::vector<Placed> placed;
    int lines = 0;
    std::vector<std::string> lineTexts;
    double lineGap = kNoLineGap;  // Outcome::lineGap
};

// How far apart two lines' solid ink really is, measured on the layout that was drawn and not on FitPitch's own sums (it shares ColumnEdges and the solid threshold with
// FitPitch, not its growth of the lower line or its steps): per canvas column both reach, the free rows
// between the upper line's lowest solid pixel and the lower line's highest, in picture pixels; the least over the columns. (ox, oy) is where each raster's corner went.
double MeasureLineGap(const LineRaster& upper, int upperX, int upperY, const LineRaster& lower, int lowerX, int lowerY) {
    std::vector<int> upperTop;
    std::vector<int> upperBottom;
    std::vector<int> lowerTop;
    std::vector<int> lowerBottom;
    ColumnEdges(upper, upperTop, upperBottom);
    ColumnEdges(lower, lowerTop, lowerBottom);
    int least = std::numeric_limits<int>::max();
    for (int x = 0; x < upper.w; ++x) {
        const int lx = upperX + x - lowerX;
        if (lx < 0 || lx >= lower.w || upperBottom[Z(x)] == kNoRow || lowerTop[Z(lx)] == -kNoRow) continue;
        least = std::min(least, (lowerY + lowerTop[Z(lx)]) - (upperY + upperBottom[Z(x)]) - 1);
    }
    return least == std::numeric_limits<int>::max() ? kNoLineGap : static_cast<double>(least) / kSupersample;
}

void NotePlaced(const LineRaster& raster, int line, int ox, int oy, Placement& out) {
    for (const Run& run : raster.runs) {
        Placed placed;
        placed.codePoint = run.codePoint;
        placed.line = line;
        placed.penX = static_cast<double>(ox + raster.penX) / kSupersample + run.pen / kSupersample;
        placed.baseline = static_cast<double>(oy + raster.baseY) / kSupersample;
        out.placed.push_back(placed);
    }
}

// A word: one line, its ink centred (by the box where the coverage is above the threshold, on the picture's grid) on the anchor, the cap band on the anchor's y.
bool LayOutWord(const Set& set, const std::vector<std::uint32_t>& points, double scale, Placement& out, std::string& why) {
    const Text& text = *set.text;
    LineRaster raster;
    if (!DrawLine(set, points, scale, raster, why)) return false;
    const Coverage own = Downsample(raster.a, raster.w, raster.h);
    const Rect box = InkBox(own, text.inkThreshold);
    if (!box.Valid()) {
        why = "the phrase has no ink at the recipe's threshold";
        return false;
    }
    const int ox = RoundInt(text.anchorX - (static_cast<double>(box.x0) + box.x1) / 2.0);
    const int oy = RoundInt(text.anchorY + scale * text.px / 2.0 - static_cast<double>(raster.baseY / kSupersample));
    std::vector<unsigned char> canvas(Z(set.width * kSupersample) * Z(set.height * kSupersample), 0);
    PutLine(canvas, set.width * kSupersample, set.height * kSupersample, raster, ox * kSupersample, oy * kSupersample);
    out.cov = Downsample(canvas, set.width * kSupersample, set.height * kSupersample);
    out.ink = InkBox(out.cov, text.inkThreshold);
    out.lines = 1;
    NotePlaced(raster, 0, ox * kSupersample, oy * kSupersample, out);
    return true;
}

// A stack of lines: baselines about the block's middle, the lines' own ink kept apart, the block's ink centred on the anchor, each line centred by its own ink.
bool LayOutStack(const Set& set, const std::vector<std::vector<std::uint32_t>>& lines, double scale, Placement& out, std::string& why) {
    const Text& text = *set.text;
    const int n = static_cast<int>(lines.size());
    std::vector<LineRaster> rasters(Z(n));
    for (int i = 0; i < n; ++i) {
        if (!DrawLine(set, lines[Z(i)], scale, rasters[Z(i)], why)) return false;
    }
    const double mid = text.firstBaseline + text.pitch / 2.0; // the block keeps its middle baseline when it shrinks
    double pitch = text.pitch * scale;
    if (n == 2) pitch = FitPitch(rasters[0], rasters[1], pitch, text.gap * text.px * scale);
    std::vector<double> baselines(Z(n));
    for (int i = 0; i < n; ++i) baselines[Z(i)] = mid + (static_cast<double>(i) - (n - 1) / 2.0) * pitch;
    // The block's ink, from the first line's highest solid row to the last line's lowest, is centred on the anchor whatever its ascenders and descenders are.
    double top = std::numeric_limits<double>::max();
    double bottom = std::numeric_limits<double>::lowest();
    for (int i = 0; i < n; ++i) {
        int first = 0;
        int end = 0;
        if (!SolidRows(rasters[Z(i)], first, end)) {
            why = "a line of the phrase has no solid ink";
            return false;
        }
        top = std::min(top, baselines[Z(i)] - static_cast<double>(rasters[Z(i)].baseY - first) / kSupersample);
        bottom = std::max(bottom, baselines[Z(i)] + static_cast<double>(end - rasters[Z(i)].baseY) / kSupersample);
    }
    const double shift = text.anchorY - (top + bottom) / 2.0;
    std::vector<unsigned char> canvas(Z(set.width * kSupersample) * Z(set.height * kSupersample), 0);
    std::vector<int> corners(Z(n) * 2, 0); // where each raster's top-left corner went: x, y
    for (int i = 0; i < n; ++i) {
        int first = 0;
        int end = 0;
        if (!InkColumns(rasters[Z(i)], first, end)) {
            why = "a line of the phrase has no ink";
            return false;
        }
        const double inkCentre = (static_cast<double>(first) + end) / 2.0;
        const int ox = RoundInt(text.anchorX * kSupersample - inkCentre);
        const int oy = RoundInt((baselines[Z(i)] + shift) * kSupersample - rasters[Z(i)].baseY);
        corners[Z(i) * 2] = ox;
        corners[Z(i) * 2 + 1] = oy;
        PutLine(canvas, set.width * kSupersample, set.height * kSupersample, rasters[Z(i)], ox, oy);
        NotePlaced(rasters[Z(i)], i, ox, oy, out);
    }
    if (n == 2) out.lineGap = MeasureLineGap(rasters[0], corners[0], corners[1], rasters[1], corners[2], corners[3]);
    out.cov = Downsample(canvas, set.width * kSupersample, set.height * kSupersample);
    out.ink = InkBox(out.cov, text.inkThreshold);
    out.lines = n;
    return true;
}

double AdvanceOf(const Set& set, const std::vector<std::uint32_t>& points, std::string& why) {
    std::vector<Run> runs;
    double advance = 0.0;
    if (!SetRuns(set, points, 1.0, runs, advance, why)) return -1.0;
    return advance / kSupersample;
}

// What the fit decided: the scale, and the layout at it.
struct Fitted {
    double scale = 0.0;
    Placement placement;
};

// The block fits when the ink is inside the recipe's box: a word's ink no wider and no higher than the box, a stack's ink inside the safe rectangle.
bool FitsBox(const Text& text, const Rect& ink) {
    if (!ink.Valid()) return false;
    if (text.layout == Layout::Word) return ink.Width() <= text.maxWidth && ink.Height() <= text.maxHeight;
    return ink.x0 >= text.safe.x0 && ink.x1 <= text.safe.x1 && ink.y0 >= text.safe.y0 && ink.y1 <= text.safe.y1;
}

std::string ScaleFailure(const Recipe& recipe, double scale) {
    return Fmt("%s would need scale %.3f, below the recipe's floor %.3f", recipe.id.c_str(), scale, recipe.text.minScale);
}

bool FitWord(const Set& set, const Recipe& recipe, const std::string& phrase, Fitted& out, std::string& why) {
    const Text& text = *set.text;
    const std::vector<std::uint32_t> points = ShowLine(set.lang, phrase, text.japaneseSpaces);
    if (points.empty()) {
        why = "the phrase is empty";
        return false;
    }
    // The scale a box of the recipe's size would take, from one layout at 1.0: ink grows linearly with the scale, so the search starts one step above the answer and
    // the layout at each step is the arbiter.
    Placement first;
    if (!LayOutWord(set, points, 1.0, first, why)) return false;
    double wanted = 1.0;
    if (first.ink.Width() > 0 && first.ink.Height() > 0) wanted = std::min({1.0, text.maxWidth / first.ink.Width(), text.maxHeight / first.ink.Height()});
    int n = 0;
    if (wanted < 1.0) n = std::max(0, static_cast<int>(std::floor(std::log(wanted) / std::log(text.step))) - 1);
    double worst = 1.0;
    for (; n < 100000; ++n) {
        const double scale = std::pow(text.step, n);
        worst = scale;
        if (scale < text.minScale - 1e-9) {
            why = ScaleFailure(recipe, scale);
            return false;
        }
        Placement placement;
        if (n == 0) placement = first;
        else if (!LayOutWord(set, points, scale, placement, why)) return false;
        if (!FitsBox(text, placement.ink)) continue;
        placement.lineTexts = {phrase};
        out.scale = scale;
        out.placement = std::move(placement);
        return true;
    }
    why = ScaleFailure(recipe, worst);
    return false;
}

// Every way of cutting pieces [0, count) into `lines` runs at their gaps: the cut positions, ascending, first the earliest.
void CutsFrom(std::size_t from, std::size_t count, int cutsLeft, std::vector<std::size_t>& current, std::vector<std::vector<std::size_t>>& out) {
    if (cutsLeft == 0) {
        out.push_back(current);
        return;
    }
    for (std::size_t i = from; i + static_cast<std::size_t>(cutsLeft) <= count; ++i) {
        current.push_back(i);
        CutsFrom(i + 1, count, cutsLeft - 1, current, out);
        current.pop_back();
    }
}

void Cuts(std::size_t count, int lines, std::vector<std::vector<std::size_t>>& out) {
    std::vector<std::size_t> current;
    CutsFrom(1, count, lines - 1, current, out);
}

bool FitStack(const Set& set, const Recipe& recipe, const std::string& phrase, Fitted& out, std::string& why) {
    const Text& text = *set.text;
    const Phrase pieces = Pieces(phrase);
    const int available = static_cast<int>(pieces.pieces.size());
    std::string lastWhy;
    // The most lines the recipe allows that fit at or above the floor; fewer only when more cannot.
    for (int k = std::min(text.maxLines, available); k >= 1; --k) {
        std::vector<std::vector<std::size_t>> cuts;
        Cuts(pieces.pieces.size(), k, cuts);
        // The cut with the narrowest widest line at scale 1, the earliest of equals (advances grow with the scale, so one cut serves every scale).
        double best = std::numeric_limits<double>::max();
        std::vector<std::string> bestLines;
        std::vector<std::vector<std::uint32_t>> bestPoints;
        for (const std::vector<std::size_t>& cut : cuts) {
            std::vector<std::string> lines;
            std::size_t from = 0;
            for (std::size_t c = 0; c <= cut.size(); ++c) {
                const std::size_t to = c < cut.size() ? cut[c] : pieces.pieces.size();
                lines.push_back(JoinPieces(pieces, from, to));
                from = to;
            }
            std::vector<std::vector<std::uint32_t>> shown;
            double widest = 0.0;
            bool ok = true;
            for (const std::string& line : lines) {
                shown.push_back(ShowLine(set.lang, line, text.japaneseSpaces));
                const double advance = AdvanceOf(set, shown.back(), why);
                if (advance < 0.0 || shown.back().empty()) {
                    ok = false;
                    break;
                }
                widest = std::max(widest, advance);
            }
            if (!ok) {
                if (why.empty()) why = "a line of the phrase is empty";
                return false;
            }
            if (widest < best) {
                best = widest;
                bestLines = lines;
                bestPoints = shown;
            }
        }
        if (bestLines.empty() || best <= 0.0) continue;
        // Scales from the first whose widest advance fits the recipe's width: the advance is the cheap test, the layout the one that counts.
        int n = 0;
        if (best > text.maxWidth) n = std::max(0, static_cast<int>(std::ceil(std::log(text.maxWidth / best) / std::log(text.step) - 1e-9)));
        for (; n < 100000; ++n) {
            const double scale = std::pow(text.step, n);
            if (scale < text.minScale - 1e-9) {
                lastWhy = ScaleFailure(recipe, scale) + Fmt(" in %d line(s)", k);
                break;
            }
            if (best * scale > text.maxWidth + 1e-9) continue;
            Placement placement;
            if (!LayOutStack(set, bestPoints, scale, placement, why)) return false;
            if (!FitsBox(text, placement.ink)) continue;
            placement.lineTexts = bestLines;
            out.scale = scale;
            out.placement = std::move(placement);
            return true;
        }
    }
    why = lastWhy.empty() ? "the phrase cannot be set" : lastWhy;
    return false;
}

// ---- paragraphs and bands (planning step 248) --------------------------------------------------------------------------------------------------

// How far a layout's canvas reaches beyond the box the ink may lie in: the ink of a phrase that does not fit is seen in this margin (and refused) instead of being cut off.
constexpr int kWindowMargin = 24;

Rect GrownWindow(const Rect& r, int width, int height) {
    return Rect{std::max(0, r.x0 - kWindowMargin), std::max(0, r.y0 - kWindowMargin), std::min(width, r.x1 + kWindowMargin), std::min(height, r.y1 + kWindowMargin)};
}

enum class Align { Centre, Left };

// What a block of lines is laid out by.
struct BlockSpec {
    Align align = Align::Centre;
    double anchorX = 0.0;          // Centre: where each line's ink is centred; Left: where each line's ink begins
    bool centreBlock = true;       // the block's solid ink is centred on anchorY (a paragraph); else anchorY is a baseline (a band's)
    double anchorY = 0.0;
    bool anchorLast = false;       // the baseline is the LAST line's: the block grows upward
    bool slide = false;            // the block is slid up until its lowest solid ink is above slideBound (a descender must not pass the band's last ink row)
    double slideBound = 0.0;
    double startPitch = 0.0;       // the baselines' distance at this scale, before two lines are pushed apart
    double gap = 0.0;              // the ink of two lines is kept this far apart (pixels at this scale)
    Rect window;                   // the picture's pixels the canvas covers
};

// Several lines at one scale on a canvas of the block's window: baselines a pitch apart (more where the ink of two neighbours would touch), each line placed by its ink,
// the block placed by `spec`. The coverage is in the window's cells, whose first is the window's first pixel.
bool LayOutBlock(const Set& set, const std::vector<std::vector<std::uint32_t>>& lines, double scale, const BlockSpec& spec, Placement& out, std::string& why) {
    const Text& text = *set.text;
    const int n = static_cast<int>(lines.size());
    if (n < 1 || !spec.window.Valid()) {
        why = "nothing to lay out";
        return false;
    }
    std::vector<LineRaster> rasters(Z(n));
    for (int i = 0; i < n; ++i) {
        if (!DrawLine(set, lines[Z(i)], scale, rasters[Z(i)], why)) return false;
    }
    std::vector<double> rel(Z(n), 0.0);
    for (int i = 1; i < n; ++i) {
        rel[Z(i)] = rel[Z(i - 1)] + FitPitch(rasters[Z(i - 1)], rasters[Z(i)], spec.startPitch, spec.gap, spec.align == Align::Left);
    }
    std::vector<double> top(Z(n));
    std::vector<double> bottom(Z(n));
    for (int i = 0; i < n; ++i) {
        int first = 0;
        int end = 0;
        if (!SolidRows(rasters[Z(i)], first, end)) {
            why = "a line of the phrase has no solid ink";
            return false;
        }
        top[Z(i)] = rel[Z(i)] - static_cast<double>(rasters[Z(i)].baseY - first) / kSupersample;
        bottom[Z(i)] = rel[Z(i)] + static_cast<double>(end - rasters[Z(i)].baseY) / kSupersample;
    }
    double base0 = 0.0; // the first line's baseline, in picture pixels
    if (spec.centreBlock) {
        base0 = spec.anchorY - (*std::min_element(top.begin(), top.end()) + *std::max_element(bottom.begin(), bottom.end())) / 2.0;
    } else {
        base0 = spec.anchorLast ? spec.anchorY - rel[Z(n - 1)] : spec.anchorY;
        if (spec.slide) {
            const double over = base0 + *std::max_element(bottom.begin(), bottom.end()) - spec.slideBound;
            if (over > 0.0) base0 -= over;
        }
    }
    const int cw = spec.window.Width() * kSupersample;
    const int ch = spec.window.Height() * kSupersample;
    std::vector<unsigned char> canvas(Z(cw) * Z(ch), 0);
    std::vector<int> corners(Z(n) * 2, 0); // where each raster's top-left corner went, in the picture's supersampled pixels: x, y
    for (int i = 0; i < n; ++i) {
        int first = 0;
        int end = 0;
        if (!InkColumns(rasters[Z(i)], first, end)) {
            why = "a line of the phrase has no ink";
            return false;
        }
        const int ox = spec.align == Align::Centre ? RoundInt(spec.anchorX * kSupersample - (static_cast<double>(first) + end) / 2.0) : RoundInt(spec.anchorX * kSupersample - first);
        const int oy = RoundInt((base0 + rel[Z(i)]) * kSupersample - rasters[Z(i)].baseY);
        corners[Z(i) * 2] = ox;
        corners[Z(i) * 2 + 1] = oy;
        PutLine(canvas, cw, ch, rasters[Z(i)], ox - spec.window.x0 * kSupersample, oy - spec.window.y0 * kSupersample);
        NotePlaced(rasters[Z(i)], i, ox, oy, out);
    }
    for (int i = 0; i + 1 < n; ++i) {
        const double gap = MeasureLineGap(rasters[Z(i)], corners[Z(i) * 2], corners[Z(i) * 2 + 1], rasters[Z(i + 1)], corners[Z(i + 1) * 2], corners[Z(i + 1) * 2 + 1]);
        if (gap < out.lineGap) out.lineGap = gap;
    }
    out.cov = Downsample(canvas, cw, ch, spec.window.x0, spec.window.y0);
    out.ink = InkBox(out.cov, text.inkThreshold);
    out.lines = n;
    return true;
}

// A phrase cut into k lines, every way: each line as it is set (logical text), as it is drawn (shown), and its advance at scale 1.
struct Arrangement {
    std::vector<std::string> texts;
    std::vector<std::vector<std::uint32_t>> shown;
    double widest = 0.0;
    double narrowest = 0.0;
};

// byLines[k - 1] holds every arrangement of the phrase in k lines, for k from 1 to the most the phrase and the recipe allow.
bool Arrange(const Set& set, const std::string& phrase, BreakAt breakAt, int maxLines, std::vector<std::vector<Arrangement>>& byLines, std::string& why) {
    const Phrase pieces = Pieces(phrase, breakAt);
    const int available = static_cast<int>(pieces.pieces.size());
    byLines.assign(Z(std::min(maxLines, available)), {});
    for (int k = 1; k <= static_cast<int>(byLines.size()); ++k) {
        std::vector<std::vector<std::size_t>> cuts;
        Cuts(pieces.pieces.size(), k, cuts);
        for (const std::vector<std::size_t>& cut : cuts) {
            Arrangement arrangement;
            std::size_t from = 0;
            arrangement.narrowest = std::numeric_limits<double>::max();
            for (std::size_t c = 0; c <= cut.size(); ++c) {
                const std::size_t to = c < cut.size() ? cut[c] : pieces.pieces.size();
                arrangement.texts.push_back(JoinPieces(pieces, from, to));
                from = to;
                arrangement.shown.push_back(ShowLine(set.lang, arrangement.texts.back(), set.text->japaneseSpaces));
                if (arrangement.shown.back().empty()) {
                    why = "a line of the phrase is empty";
                    return false;
                }
                const double advance = AdvanceOf(set, arrangement.shown.back(), why);
                if (advance < 0.0) return false;
                arrangement.widest = std::max(arrangement.widest, advance);
                arrangement.narrowest = std::min(arrangement.narrowest, advance);
            }
            byLines[Z(k - 1)].push_back(std::move(arrangement));
        }
    }
    return true;
}

// The arrangement with the FEWEST lines whose widest line fits `maxWidth` at `scale` (and, with two lines or more, whose narrowest is at least `orphan` of its widest), the one with
// the narrowest widest line of them, the earliest of equals. Null when none fits.
const Arrangement* PickArrangement(const std::vector<std::vector<Arrangement>>& byLines, double scale, double maxWidth, double orphan) {
    for (std::size_t k = 0; k < byLines.size(); ++k) {
        const Arrangement* best = nullptr;
        for (const Arrangement& candidate : byLines[k]) {
            if (candidate.widest * scale > maxWidth + 1e-9) continue;
            if (k >= 1 && candidate.narrowest < orphan * candidate.widest) continue;
            if (best == nullptr || candidate.widest < best->widest) best = &candidate;
        }
        if (best != nullptr) return best;
    }
    return nullptr;
}

// The pitch ratio of the face the phrase is first drawn in: a paragraph's lines are as far apart as that face's tuning says.
double PitchRatioOf(const Set& set, const std::vector<std::uint32_t>& points) {
    for (const std::uint32_t point : points) {
        if (IsGap(point)) continue;
        const Face* face = set.FaceOf(point);
        if (face == nullptr) continue;
        for (const FaceTuning& tuning : set.text->faces) {
            if (tuning.face == face->id) return tuning.pitch;
        }
        return 0.0;
    }
    return 0.0;
}

// A paragraph: at each scale from 1 down, the fewest lines that fit, laid out and tried against the recipe's box; the first scale that fits is the largest, and a layout of fewer lines
// within `saveLines` of it below is taken instead (a line saved is worth a few pixels of size).
bool FitParagraph(const Set& set, const Recipe& recipe, const std::string& phrase, Fitted& out, std::string& why) {
    const Text& text = *set.text;
    std::vector<std::vector<Arrangement>> byLines;
    if (!Arrange(set, phrase, BreakAt::Space, text.maxLines, byLines, why)) return false;
    if (byLines.empty() || byLines[0].empty()) {
        why = "the phrase cannot be set";
        return false;
    }
    const double pitchRatio = PitchRatioOf(set, byLines[0][0].shown[0]);
    if (pitchRatio <= 0.0) {
        why = "the recipe has no line pitch for the face the phrase is drawn in";
        return false;
    }
    const Rect window = GrownWindow(text.safe, set.width, set.height);
    bool found = false;
    double finest = 0.0;
    for (int n = 0; n < 100000; ++n) {
        const double scale = std::pow(text.step, n);
        if (found && scale < finest - text.saveLines - 1e-9) break;
        if (scale < text.minScale - 1e-9) {
            if (!found) why = ScaleFailure(recipe, scale);
            break;
        }
        const Arrangement* arrangement = PickArrangement(byLines, scale, text.maxWidth, text.orphan);
        if (arrangement == nullptr) continue;
        BlockSpec spec;
        spec.align = Align::Centre;
        spec.anchorX = text.anchorX;
        spec.anchorY = text.anchorY;
        spec.centreBlock = true;
        spec.startPitch = pitchRatio * text.px * scale;
        spec.gap = text.gap * text.px * scale;
        spec.window = window;
        Placement placement;
        if (!LayOutBlock(set, arrangement->shown, scale, spec, placement, why)) return false;
        if (!FitsBox(text, placement.ink)) continue;
        placement.lineTexts = arrangement->texts;
        if (!found || placement.lines < out.placement.lines) {
            if (!found) finest = scale;
            found = true;
            out.scale = scale;
            out.placement = std::move(placement);
        }
        if (text.saveLines <= 0.0 || out.placement.lines == 1) break;
    }
    if (!found) {
        if (why.empty()) why = ScaleFailure(recipe, text.minScale);
        return false;
    }
    why.clear();
    return true;
}

// The Text a band is drawn with: the recipe's, with the band's size and the band's own tunings over the shared ones.
Text BandText(const Text& base, const Band& band) {
    Text text = base;
    text.px = band.px;
    text.bands.clear();
    for (const FaceTuning& over : band.faces) {
        bool replaced = false;
        for (FaceTuning& own : text.faces) {
            if (own.face == over.face) {
                own = over;
                replaced = true;
            }
        }
        if (!replaced) text.faces.push_back(over);
    }
    return text;
}

// One band: at each scale from 1 down, for 1 line up to the band's most, the cut with the narrowest widest line, left-aligned; the first that is within the right edge and the
// band's ink rows is taken (wrapping wins over shrinking).
// `aboveBottom` is the first free row under the ink of the band this one balances against (Band::balanceAbove), or negative when the band balances against none.
bool FitBand(const Set& set, const Recipe& recipe, const Band& band, const std::string& phrase, double aboveBottom, Fitted& out, std::string& why) {
    const Text& text = *set.text;
    std::vector<std::vector<Arrangement>> byLines;
    if (!Arrange(set, phrase, band.breakAt, band.maxLines, byLines, why)) return false;
    if (byLines.empty() || byLines[0].empty()) {
        why = "the phrase cannot be set";
        return false;
    }
    const Rect rows{0, band.inkRows.a, set.width, band.inkRows.b};
    const Rect window = GrownWindow(rows, set.width, set.height);
    const double room = text.maxRight - text.left;
    for (int n = 0; n < 100000; ++n) {
        const double scale = std::pow(text.step, n);
        if (scale < text.minScale - 1e-9) {
            why = Fmt("%s would need scale %.3f, below the recipe's floor %.3f", band.key.c_str(), scale, text.minScale);
            return false;
        }
        for (std::size_t k = 0; k < byLines.size(); ++k) {
            const Arrangement* best = nullptr;
            for (const Arrangement& candidate : byLines[k]) {
                if (best == nullptr || candidate.widest < best->widest) best = &candidate;
            }
            if (best == nullptr || best->widest * scale > room + 1e-9) continue;
            BlockSpec spec;
            spec.align = Align::Left;
            spec.anchorX = text.left;
            spec.anchorY = band.firstBaseline;
            spec.anchorLast = band.anchorLast;
            spec.centreBlock = false;
            spec.slide = true;
            spec.slideBound = band.inkRows.b;
            spec.startPitch = band.pitch * scale;
            spec.gap = text.gap * band.px * scale;
            spec.window = window;
            Placement placement;
            if (!LayOutBlock(set, best->shown, scale, spec, placement, why)) return false;
            const auto inside = [&](const Placement& p) {
                return p.ink.Valid() && p.ink.x1 <= static_cast<int>(std::floor(text.maxRight)) && p.ink.y0 >= band.inkRows.a && p.ink.y1 <= band.inkRows.b;
            };
            if (!inside(placement)) continue;
            // WHAT FITS IS DECIDED BY THE BASELINE'S LAYOUT, as it always was (the scale, the lines, which cut): a band that wraps to enough lines is then MOVED, the same lines at
            // the same size, to between the ink above it and the row where the ink below it begins, its block's ink centred there. Moved after the fit, not fitted there, so that
            // the room the move frees does not turn a heading of one line into one of two, or make a heading larger than it was (step 249 changes where it stands, not what it is).
            // A move that would leave the band's ink rows keeps the baseline's placement.
            if (band.balanced && aboveBottom >= 0.0 && static_cast<int>(best->texts.size()) >= band.balanceMinLines) {
                BlockSpec moved = spec;
                moved.anchorY = (aboveBottom + band.balanceBelow) / 2.0;
                moved.centreBlock = true;
                moved.slide = false;
                Placement balanced;
                if (!LayOutBlock(set, best->shown, scale, moved, balanced, why)) return false;
                if (inside(balanced)) placement = std::move(balanced);
            }
            placement.lineTexts = best->texts;
            out.scale = scale;
            out.placement = std::move(placement);
            return true;
        }
    }
    (void)recipe;
    why = "the phrase cannot be set";
    return false;
}

// Every band of a recipe fitted to its own phrase (request.phrases, in the bands' order). Any band below the floor, or any phrase no face can draw, fails the whole picture.
struct BandFit {
    Text text;
    Fitted fit;
};

bool FitBands(const Request& request, std::vector<BandFit>& out, std::string& why) {
    const Recipe& recipe = *request.recipe;
    if (request.phrases.size() != recipe.text.bands.size()) {
        why = Fmt("the request holds %d phrases for the recipe's %d bands", static_cast<int>(request.phrases.size()), static_cast<int>(recipe.text.bands.size()));
        return false;
    }
    out.clear();
    for (std::size_t i = 0; i < recipe.text.bands.size(); ++i) {
        const Band& band = recipe.text.bands[i];
        const std::string& phrase = request.phrases[i];
        if (phrase.empty() || !Utf8::IsValid(phrase)) {
            why = band.key + ": the phrase is empty or not valid UTF-8";
            return false;
        }
        BandFit made;
        made.text = BandText(recipe.text, band);
        Set set;
        if (!MakeSet(request, made.text, set, why)) return false;
        const double aboveBottom = band.balanced ? static_cast<double>(out[Z(band.balanceAbove)].fit.placement.ink.y1) : -1.0;
        if (!FitBand(set, recipe, band, phrase, aboveBottom, made.fit, why)) {
            why = band.key + ": " + why;
            return false;
        }
        out.push_back(std::move(made));
    }
    return true;
}

bool Fit(const Request& request, Set& set, Fitted& out, std::string& why) {
    if (!MakeSet(request, request.recipe->text, set, why)) return false;
    const Recipe& recipe = *request.recipe;
    const std::string phrase(request.phrase);
    if (phrase.empty() || !Utf8::IsValid(phrase)) {
        why = "the phrase is empty or not valid UTF-8";
        return false;
    }
    switch (recipe.text.layout) {
    case Layout::Word:
        return FitWord(set, recipe, phrase, out, why);
    case Layout::Stack:
        return FitStack(set, recipe, phrase, out, why);
    case Layout::Paragraph:
        return FitParagraph(set, recipe, phrase, out, why);
    case Layout::Bands:
        break;
    }
    why = "a bands recipe has a phrase per band, not one phrase";
    return false;
}

// ---- the look: shadow, shade, composition ------------------------------------------------------------------------------------------------------

struct Kernel {
    int first = 0;
    std::vector<double> weights;
};

// A Gaussian centred `offset` pixels along an axis, over integer taps, summing to one (FontBake's, in doubles).
Kernel MakeKernel(double offset, double sigma) {
    Kernel kernel;
    if (sigma <= 0.0) {
        const int lo = static_cast<int>(std::floor(offset));
        const double frac = offset - lo;
        kernel.first = lo;
        kernel.weights = {1.0 - frac, frac};
        return kernel;
    }
    const int lo = static_cast<int>(std::floor(offset - kKernelSigmas * sigma));
    const int hi = static_cast<int>(std::ceil(offset + kKernelSigmas * sigma));
    kernel.first = lo;
    double sum = 0.0;
    for (int i = lo; i <= hi; ++i) {
        const double d = i - offset;
        kernel.weights.push_back(std::exp(-(d * d) / (2.0 * sigma * sigma)));
        sum += kernel.weights.back();
    }
    if (!(sum > 0.0) || !std::isfinite(sum)) {
        kernel.weights.clear();
        const int base = static_cast<int>(std::floor(offset));
        const double frac = offset - base;
        kernel.first = base;
        kernel.weights = {1.0 - frac, frac};
        return kernel;
    }
    for (double& w : kernel.weights) w /= sum;
    return kernel;
}

// A block of doubles over a window of the picture.
struct Field {
    int w = 0;
    int h = 0;
    std::vector<double> v;
    double& At(int x, int y) { return v[Z(y) * Z(w) + Z(x)]; }
    double At(int x, int y) const { return v[Z(y) * Z(w) + Z(x)]; }
};

// The field moved right and down by the kernels' centres and blurred, zero beyond the window: out(x, y) = sum_k wx[k] sum_j wy[j] in(x - (fx + k), y - (fy + j)).
Field Convolve(const Field& in, const Kernel& along, const Kernel& down) {
    Field across;
    across.w = in.w;
    across.h = in.h;
    across.v.assign(in.v.size(), 0.0);
    for (int y = 0; y < in.h; ++y) {
        for (int x = 0; x < in.w; ++x) {
            double sum = 0.0;
            for (std::size_t k = 0; k < along.weights.size(); ++k) {
                const int sx = x - (along.first + static_cast<int>(k));
                if (sx >= 0 && sx < in.w) sum += along.weights[k] * in.At(sx, y);
            }
            across.At(x, y) = sum;
        }
    }
    Field out;
    out.w = in.w;
    out.h = in.h;
    out.v.assign(in.v.size(), 0.0);
    for (int y = 0; y < in.h; ++y) {
        for (int x = 0; x < in.w; ++x) {
            double sum = 0.0;
            for (std::size_t k = 0; k < down.weights.size(); ++k) {
                const int sy = y - (down.first + static_cast<int>(k));
                if (sy >= 0 && sy < in.h) sum += down.weights[k] * across.At(x, sy);
            }
            out.At(x, y) = sum;
        }
    }
    return out;
}

// The finished picture: `image` (the erased plate, or a transparent canvas) with the words over it. Only pixels inside `clip` are written, and of those only the ones
// the ink or the shadow touches.
bool Compose(const Text& text, double scale, const Placement& placement, const Rect& clip, Image& image, std::string& why) {
    const Coverage& cov = placement.cov;
    // The window the shadow and the shade can reach: the coverage's own extent, grown by the kernels' reach.
    Rect extent = InkBox(cov, 0.0);
    if (!extent.Valid()) {
        why = "nothing was drawn";
        return false;
    }
    const double measure = text.px * scale;
    const double sigma = std::max(text.shadow.sigmaMin, text.shadow.sigma * measure);
    double dx = text.shadow.dx * measure;
    double dy = text.shadow.dy * measure;
    if (text.shadow.roundOffsets) {
        dx = RoundInt(dx);
        dy = RoundInt(dy);
    }
    const double isigma = std::max(1.0, text.inner.sigma * measure);
    const double idx = static_cast<double>(RoundInt(text.inner.dx * measure));
    const double idy = static_cast<double>(RoundInt(text.inner.dy * measure));
    const double reach = std::ceil(kKernelSigmas * std::max(sigma, text.inner.k > 0.0 ? isigma : 0.0)) + std::ceil(std::max({std::fabs(dx), std::fabs(dy), std::fabs(idx), std::fabs(idy)})) + 2.0;
    const int margin = static_cast<int>(reach);
    const Rect window{std::max(0, extent.x0 - margin), std::max(0, extent.y0 - margin), std::min(image.width, extent.x1 + margin), std::min(image.height, extent.y1 + margin)};
    Field coverage;
    coverage.w = window.Width();
    coverage.h = window.Height();
    coverage.v.assign(Z(coverage.w) * Z(coverage.h), 0.0);
    for (int y = 0; y < coverage.h; ++y) {
        for (int x = 0; x < coverage.w; ++x) coverage.At(x, y) = static_cast<double>(cov.At(window.x0 + x, window.y0 + y));
    }
    // No shadow at all (a recipe of bands, whose ink is plain): a field of zeros, not a blur of nothing.
    Field shadow;
    if (text.shadow.gain > 0.0) {
        shadow = Convolve(coverage, MakeKernel(dx, sigma), MakeKernel(dy, sigma));
        for (double& s : shadow.v) s = std::min(text.shadow.cap, text.shadow.gain * s);
    } else {
        shadow.w = coverage.w;
        shadow.h = coverage.h;
        shadow.v.assign(coverage.v.size(), 0.0);
    }
    Field shade;
    const bool shaded = text.inner.k > 0.0;
    if (shaded) shade = Convolve(coverage, MakeKernel(idx, isigma), MakeKernel(idy, isigma));
    const Rect write{std::max(window.x0, clip.x0), std::max(window.y0, clip.y0), std::min(window.x1, clip.x1), std::min(window.y1, clip.y1)};
    for (int y = write.y0; y < write.y1; ++y) {
        for (int x = write.x0; x < write.x1; ++x) {
            const int fx = x - window.x0;
            const int fy = y - window.y0;
            const double c = coverage.At(fx, fy);
            const double s = shadow.At(fx, fy);
            if (c <= 0.0 && s <= 0.0) continue; // untouched: the plate's own pixel stays as it is
            // The ink is white, shaded along the edges the original's was: ink = 1 - k * (1 - shade), the shade being the blurred, moved coverage.
            const double ink = (shaded ? 1.0 - text.inner.k * (1.0 - shade.At(fx, fy)) : 1.0) * (static_cast<double>(text.inkLevel) / 255.0);
            unsigned char* px = image.At(x, y);
            const double a = px[3] / 255.0;
            // black shadow over the plate, then the white ink over both, in straight alpha
            const double oa = s + a * (1.0 - s);
            double rgb[3];
            for (int k = 0; k < 3; ++k) {
                const double plate = px[k] / 255.0;
                rgb[k] = oa > 1e-6 ? plate * (a * (1.0 - s)) / oa : 0.0;
            }
            const double a2 = c + oa * (1.0 - c);
            for (int k = 0; k < 3; ++k) {
                const double mixed = a2 > 1e-6 ? (ink * c + rgb[k] * (oa * (1.0 - c))) / a2 : 0.0;
                px[k] = Level(mixed * 255.0);
            }
            px[3] = Level(a2 * 255.0);
            if (px[3] == 0) px[0] = px[1] = px[2] = 0; // no alpha, no colour: a texel the filter blends in is never tinted by one the encoder dropped
        }
    }
    // The ink must be inside the clip: a word that leaves its rectangle would be cut, and the rectangle is all the erase made room in.
    for (int y = cov.y0; y < cov.y0 + cov.h; ++y) {
        for (int x = cov.x0; x < cov.x0 + cov.w; ++x) {
            if (static_cast<double>(cov.At(x, y)) > text.inkThreshold && !clip.Contains(x, y)) {
                why = Fmt("the ink reaches (%d, %d), outside the rectangle the erase made room in", x, y);
                return false;
            }
        }
    }
    return true;
}

} // namespace

namespace {

// The rectangle every changed pixel is inside: the erase's, and where the word's shadow may be written beyond it.
Rect Writable(const Recipe& recipe) {
    Rect r = recipe.erase.rect;
    if (recipe.text.hasPaint) {
        r.x0 = std::min(r.x0, recipe.text.paint.x0);
        r.y0 = std::min(r.y0, recipe.text.paint.y0);
        r.x1 = std::max(r.x1, recipe.text.paint.x1);
        r.y1 = std::max(r.y1, recipe.text.paint.y1);
    }
    return r;
}

// What a Bands recipe's fits say, in the outcome: each band's result, the least scale, the lines and the glyphs of all of them.
void NoteBands(const Recipe& recipe, std::vector<BandFit>& fits, Outcome& outcome) {
    outcome.scale = 1.0;
    outcome.lines = 0;
    Rect ink{recipe.width, recipe.height, 0, 0};
    for (std::size_t i = 0; i < fits.size(); ++i) {
        Placement& placement = fits[i].fit.placement;
        BandResult result;
        result.key = recipe.text.bands[i].key;
        result.scale = fits[i].fit.scale;
        result.lines = placement.lines;
        result.lineTexts = placement.lineTexts;
        result.ink = placement.ink;
        result.lineGap = placement.lineGap;
        outcome.bands.push_back(std::move(result));
        outcome.scale = std::min(outcome.scale, fits[i].fit.scale);
        outcome.lines += placement.lines;
        outcome.lineTexts.insert(outcome.lineTexts.end(), placement.lineTexts.begin(), placement.lineTexts.end());
        outcome.lineGap = std::min(outcome.lineGap, placement.lineGap);
        ink.x0 = std::min(ink.x0, placement.ink.x0);
        ink.y0 = std::min(ink.y0, placement.ink.y0);
        ink.x1 = std::max(ink.x1, placement.ink.x1);
        ink.y1 = std::max(ink.y1, placement.ink.y1);
        for (Placed& placed : placement.placed) outcome.placed.push_back(placed);
    }
    outcome.ink = ink;
    outcome.touched = recipe.erase.rect;
}

} // namespace

Outcome FitPhrase(const Request& request) {
    Outcome outcome;
    if (request.recipe != nullptr && request.recipe->text.layout == Layout::Bands) {
        std::vector<BandFit> fits;
        if (!FitBands(request, fits, outcome.reason)) return outcome;
        NoteBands(*request.recipe, fits, outcome);
        outcome.ok = true;
        return outcome;
    }
    Set set;
    Fitted fitted;
    if (!Fit(request, set, fitted, outcome.reason)) return outcome;
    outcome.ok = true;
    outcome.scale = fitted.scale;
    outcome.lines = fitted.placement.lines;
    outcome.lineTexts = fitted.placement.lineTexts;
    outcome.ink = fitted.placement.ink;
    outcome.lineGap = fitted.placement.lineGap;
    outcome.placed = std::move(fitted.placement.placed);
    outcome.touched = Writable(*request.recipe);
    return outcome;
}

Outcome Render(const Request& request, const Image& source, const Image* twin) {
    Outcome outcome;
    if (request.recipe == nullptr) {
        outcome.reason = "the request has no recipe";
        return outcome;
    }
    const Recipe& recipe = *request.recipe;
    if (!CheckSource(recipe, source, outcome.reason)) return outcome;
    Image base;
    if (!EraseWords(recipe, source, twin, base, outcome.reason)) return outcome;
    if (recipe.text.layout == Layout::Bands) {
        // Each band is drawn into its own window of rows: the ink must lie in it, and a band's words can reach no name or logo of another band.
        std::vector<BandFit> fits;
        if (!FitBands(request, fits, outcome.reason)) return outcome;
        for (std::size_t i = 0; i < fits.size(); ++i) {
            const Band& band = recipe.text.bands[i];
            const Rect rows{0, band.inkRows.a, recipe.width, band.inkRows.b};
            if (!Compose(fits[i].text, fits[i].fit.scale, fits[i].fit.placement, rows, base, outcome.reason)) {
                outcome.reason = band.key + ": " + outcome.reason;
                return outcome;
            }
        }
        NoteBands(recipe, fits, outcome);
        outcome.image = std::move(base);
        outcome.ok = true;
        return outcome;
    }
    Set set;
    Fitted fitted;
    if (!Fit(request, set, fitted, outcome.reason)) return outcome;
    if (!Compose(recipe.text, fitted.scale, fitted.placement, Writable(recipe), base, outcome.reason)) return outcome;
    outcome.ok = true;
    outcome.image = std::move(base);
    outcome.scale = fitted.scale;
    outcome.lines = fitted.placement.lines;
    outcome.lineTexts = fitted.placement.lineTexts;
    outcome.ink = fitted.placement.ink;
    outcome.lineGap = fitted.placement.lineGap;
    outcome.placed = std::move(fitted.placement.placed);
    outcome.touched = Writable(recipe);
    return outcome;
}

// ============================================================================================================================
// files
// ============================================================================================================================

namespace {

std::string UniqueSuffix() {
    static std::atomic<unsigned> counter{0};
    std::random_device device;
    return std::to_string(device()) + "-" + std::to_string(counter.fetch_add(1));
}

// Whether a file can be trusted without rendering again: a regular file that begins with the PNG signature, whose header says the recipe's size, and that ends with the
// PNG's last chunk. A file cut short anywhere, or empty (a phone that died between the rename and the data reaching storage), fails one of the three.
bool LooksWhole(const std::filesystem::path& path, int width, int height) {
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec) || ec) return false;
    const std::uintmax_t size = std::filesystem::file_size(path, ec);
    if (ec || size < kPngSignatureSize + 25 + kPngEnd.size()) return false;
    std::ifstream in(path, std::ios::binary);
    std::array<unsigned char, 24> head{};
    in.read(reinterpret_cast<char*>(head.data()), static_cast<std::streamsize>(head.size()));
    if (static_cast<std::size_t>(in.gcount()) != head.size()) return false;
    if (!std::equal(kPngSignature.begin(), kPngSignature.end(), head.begin())) return false;
    const auto be32 = [&head](std::size_t at) {
        return (static_cast<std::uint32_t>(head[at]) << 24) | (static_cast<std::uint32_t>(head[at + 1]) << 16) | (static_cast<std::uint32_t>(head[at + 2]) << 8) |
               static_cast<std::uint32_t>(head[at + 3]);
    };
    if (be32(16) != static_cast<std::uint32_t>(width) || be32(20) != static_cast<std::uint32_t>(height)) return false;
    std::array<unsigned char, 12> tail{};
    in.seekg(-static_cast<std::streamoff>(tail.size()), std::ios::end);
    in.read(reinterpret_cast<char*>(tail.data()), static_cast<std::streamsize>(tail.size()));
    return static_cast<std::size_t>(in.gcount()) == tail.size() && tail == kPngEnd;
}

// Puts `bytes` at `target` without ever showing anyone a half-written file (FontBake's WriteWhole): a name of its own first, then a rename; whatever fails leaves
// neither the target nor the temporary behind.
bool WriteWhole(const std::filesystem::path& target, const std::string& bytes, std::string& why) {
    std::filesystem::path temporary = target;
    temporary += "." + UniqueSuffix() + ".tmp";
    {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        if (!out) {
            why = temporary.generic_string() + ": cannot be created";
            return false;
        }
        out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        out.flush();
        if (!out) {
            out.close();
            std::error_code ec;
            std::filesystem::remove(temporary, ec);
            why = temporary.generic_string() + ": cannot be written";
            return false;
        }
    }
    std::error_code ec;
    std::filesystem::rename(temporary, target, ec);
    if (ec) {
        std::filesystem::remove(temporary, ec);
        why = target.generic_string() + ": cannot be put in place";
        return false;
    }
    return true;
}

} // namespace

std::string BakedName(const Recipe& recipe, std::string_view languageCode, std::string_view hash) {
    return recipe.id + "_" + std::string(languageCode) + "_" + std::string(hash.substr(0, 16)) + ".png";
}

Written Ensure(const Wanted& wanted) {
    const auto began = std::chrono::steady_clock::now();
    Written result;
    const auto elapsed = [&began]() { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count(); };
    const auto fail = [&](const std::string& why) {
        result.ok = false;
        result.reason = why;
        result.milliseconds = elapsed();
        return result;
    };
    if (wanted.recipe == nullptr || wanted.recipes == nullptr || wanted.plan == nullptr) return fail("the request has no recipe, table or plan");
    if (wanted.directory.empty()) return fail("there is no scratch folder to write the picture in");
    const Recipe& recipe = *wanted.recipe;
    std::vector<FontBake::FaceBytes> faces;
    std::string why;
    if (!FontBake::LoadFaces(*wanted.plan, wanted.fontsDir, faces, why)) return fail(why);
    Image source;
    if (!ReadImage(wanted.sourcePath, source, why)) return fail(why);
    if (!CheckSource(recipe, source, why)) return fail(why);
    Image twin;
    if (recipe.erase.rule == Rule::Twin && !ReadImage(wanted.twinPath, twin, why)) return fail(why);

    // The name: every input that changes the picture is in the hash.
    Hasher h;
    h.Text(kAlgorithm);
    h.Text(wanted.recipes->fileHash);
    h.Text(recipe.id);
    h.Text(wanted.languageCode);
    h.Number(static_cast<std::uint64_t>(wanted.lang));
    h.Text(wanted.phrase);
    h.Number(wanted.phrases.size());
    for (const std::string& phrase : wanted.phrases) h.Text(phrase);
    if (const FontBake::SlotSpec* slot = wanted.plan->Slot(recipe.text.slot)) {
        std::vector<std::string> ids{slot->textFace};
        for (const auto& [script, face] : slot->scriptFaces) {
            (void)script;
            if (std::find(ids.begin(), ids.end(), face) == ids.end()) ids.push_back(face);
        }
        // The face the digits are drawn from is an input too, though no script routes to it.
        if (!recipe.text.digitFace.empty() && std::find(ids.begin(), ids.end(), recipe.text.digitFace) == ids.end()) ids.push_back(recipe.text.digitFace);
        for (const std::string& id : ids) {
            h.Text(id);
            for (const FontBake::FaceBytes& bytes : faces) {
                if (bytes.id != id) continue;
                h.Number(bytes.data.size());
                h.Bytes(bytes.data.data(), bytes.data.size());
            }
        }
    }
    const std::filesystem::path directory(wanted.directory);
    const std::filesystem::path target = directory / BakedName(recipe, wanted.languageCode, h.Hex());
    result.path = target.generic_string();
    if (LooksWhole(target, recipe.width, recipe.height)) {
        result.ok = true;
        result.cached = true;
        result.milliseconds = elapsed();
        return result;
    }

    Request request;
    request.recipe = &recipe;
    request.plan = wanted.plan;
    request.faces = &faces;
    request.lang = wanted.lang;
    request.phrase = wanted.phrase;
    request.phrases = wanted.phrases;
    Outcome outcome = Render(request, source, recipe.erase.rule == Rule::Twin ? &twin : nullptr);
    if (!outcome.ok) return fail(outcome.reason);
    std::string bytes;
    if (!EncodePng(outcome.image, bytes)) return fail("the picture could not be encoded as a PNG");
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    if (ec) return fail("cannot make " + directory.generic_string() + ": " + ec.message());
    if (!WriteWhole(target, bytes, why)) return fail(why);
    result.ok = true;
    result.scale = outcome.scale;
    result.lines = outcome.lines;
    result.milliseconds = elapsed();
    return result;
}

} // namespace MagicPortals::PictureBake
