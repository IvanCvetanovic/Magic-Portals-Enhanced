#include "sim/LightmapBake.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>

namespace MagicPortals::LightmapBake {

namespace {

namespace Json = Supersonic::Json;

// The twin's own guard under the square root (Light2D.cpp: facing = dot(v, n) * inversesqrt(max(d2, 1e-12))), so a
// light exactly on a texel's centre divides by a tiny length rather than by zero.
constexpr double kMinSquaredDistance = 1e-12;

// The names lighting.json writes a Rounding as; the code compares the enum, never the word.
constexpr std::array<std::pair<std::string_view, Rounding>, 2> kRoundingNames{{
    {"nearest", Rounding::Nearest},
    {"truncate", Rounding::Truncate},
}};

bool Whole(const Json::Value& value, int low, int high, int& out) {
    if (!value.IsNumber()) return false;
    const double number = value.AsNumber();
    if (!std::isfinite(number) || number != std::floor(number) || number < low || number > high) return false;
    out = static_cast<int>(number);
    return true;
}

// One axis of a bilinear tap at the centres of `count` target texels over a map `texels` wide: GL_LINEAR with
// CLAMP_TO_EDGE and no mip (GLES2Texture.cpp:150-170 for the target, SOIL.c:1330-1337 for the maps). Texel k reads
// uv = (k + 1/2) / count, so s = uv * texels - 1/2, the two taps floor(s) and floor(s) + 1, clamped, with weights
// 1 - f and f. For a map of two texels per unit (the hd and fullhd art) at the unit lattice, s = 2k + 1 and f = 0.5:
// the exact mean of the 2 x 2 block.
struct Tap {
    int i0 = 0;
    int i1 = 0;
    double f = 0.0;
};

std::vector<Tap> TapsOf(int texels, int count) {
    std::vector<Tap> taps(static_cast<std::size_t>(count));
    for (int k = 0; k < count; ++k) {
        const double u = (static_cast<double>(k) + 0.5) / static_cast<double>(count);
        const double s = u * static_cast<double>(texels) - 0.5;
        const double base = std::floor(s);
        Tap& tap = taps[static_cast<std::size_t>(k)];
        tap.i0 = std::clamp(static_cast<int>(base), 0, texels - 1);
        tap.i1 = std::clamp(static_cast<int>(base) + 1, 0, texels - 1);
        tap.f = s - base;
    }
    return taps;
}

// The three colour channels of one bilinear tap, 0..1. The operation order is the offline model's
// (a * (1 - fx) * (1 - fy) + b * fx * (1 - fy) + c * (1 - fx) * fy + d * fx * fy), so the two agree to the last bit
// where a value is not within an ulp of a rounding boundary.
glm::dvec3 Fetch(const Image& image, const Tap& x, const Tap& y) {
    const auto texel = [&image](int column, int row, std::size_t channel) {
        const std::size_t at = (static_cast<std::size_t>(row) * static_cast<std::size_t>(image.width) +
                                static_cast<std::size_t>(column)) * 4u + channel;
        return static_cast<double>(image.rgba[at]) / 255.0;
    };
    glm::dvec3 out(0.0);
    for (std::size_t c = 0; c < 3; ++c) {
        const double t00 = texel(x.i0, y.i0, c);
        const double t01 = texel(x.i1, y.i0, c);
        const double t10 = texel(x.i0, y.i1, c);
        const double t11 = texel(x.i1, y.i1, c);
        out[static_cast<glm::length_t>(c)] =
            t00 * (1.0 - x.f) * (1.0 - y.f) + t01 * x.f * (1.0 - y.f) + t10 * (1.0 - x.f) * y.f + t11 * x.f * y.f;
    }
    return out;
}

bool Sized(const Image& image) {
    return image.width > 0 && image.height > 0 &&
           image.rgba.size() == static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height) * 4u;
}

// The index a source texel `at` reads when the edge MIRRORS: -1 reads 0, -2 reads 1, n reads n - 1, n + 1 reads n - 2
// (calibration R10; the first and last output texels' taps stand up to three texels past the edge). The two reflections
// are applied in turn, as the offline model does, and the result clamped, so a source narrower than the taps' reach
// reads inside itself rather than outside.
int Mirror(int at, int count) {
    if (at < 0) at = -at - 1;
    if (at >= count) at = 2 * count - 1 - at;
    return std::clamp(at, 0, count - 1);
}

} // namespace

std::string KeyOf(const std::string& marker) {
    return std::string(kKeyPrefix) + marker;
}

bool ParseRules(const Json::Value& block, Rules& out, std::string& error) {
    if (!block.IsObject()) {
        error = "lightmap_bake is not an object";
        return false;
    }
    Rules read;

    // The reduction first: units_per_texel is checked against its factor.
    const Json::Value& reduction = block["reduction"];
    if (!block.Has("reduction") || !reduction.IsObject()) {
        error = "lightmap_bake.reduction is not an object";
        return false;
    }
    if (!Whole(reduction["factor"], 2, 2, read.reduction.factor)) {
        error = "lightmap_bake.reduction.factor is not 2, the one reduction the calibration fixed";
        return false;
    }
    if (!Whole(reduction["denominator"], 1, 65536, read.reduction.denominator)) {
        error = "lightmap_bake.reduction.denominator is not a whole number from 1 to 65536";
        return false;
    }
    const Json::Value& taps = reduction["taps"];
    // 4 taps per factor: two source texels per output texel and one more block each side.
    if (!taps.IsArray() || taps.AsArray().size() != static_cast<std::size_t>(4 * read.reduction.factor)) {
        error = "lightmap_bake.reduction.taps is not " + std::to_string(4 * read.reduction.factor) + " numbers";
        return false;
    }
    read.reduction.taps.clear();
    long long sum = 0;
    for (const Json::Value& tap : taps.AsArray()) {
        int whole = 0;
        if (!Whole(tap, -65536, 65536, whole)) {
            error = "lightmap_bake.reduction.taps holds a number that is not a whole number";
            return false;
        }
        read.reduction.taps.push_back(whole);
        sum += whole;
    }
    if (sum != read.reduction.denominator) {
        error = "lightmap_bake.reduction.taps sum to " + std::to_string(sum) + ", not the denominator " +
                std::to_string(read.reduction.denominator) + ": a constant would not stay constant";
        return false;
    }

    const Json::Value& pitch = block["units_per_texel"];
    if (!block.Has("units_per_texel") || !pitch.IsObject() ||
        !Whole(pitch["value"], 1, read.reduction.factor, read.unitsPerTexel)) {
        error = "lightmap_bake.units_per_texel.value is not 1 or the reduction's factor " +
                std::to_string(read.reduction.factor);
        return false;
    }

    const Json::Value& target = block["target"];
    if (!block.Has("target") || !target.IsObject()) {
        error = "lightmap_bake.target is not an object";
        return false;
    }
    constexpr std::array<const char*, 3> kBitKeys{"red_bits", "green_bits", "blue_bits"};
    for (std::size_t c = 0; c < kBitKeys.size(); ++c) {
        if (!Whole(target[kBitKeys[c]], 1, 8, read.bits[c])) {
            error = std::string("lightmap_bake.target.") + kBitKeys[c] + " is not a whole number from 1 to 8";
            return false;
        }
    }
    bool named = false;
    if (target["rounding"].IsString()) {
        for (const auto& [name, rounding] : kRoundingNames) {
            if (target["rounding"].AsString() == name) {
                read.rounding = rounding;
                named = true;
            }
        }
    }
    if (!named) {
        error = "lightmap_bake.target.rounding is not \"nearest\" or \"truncate\"";
        return false;
    }

    out = read;
    return true;
}

void ApplyPixelMask(Image& image) {
    if (!Sized(image)) return;
    for (std::size_t at = 0; at + 3 < image.rgba.size(); at += 4) {
        std::uint8_t* const texel = image.rgba.data() + at;
        // mask 0xFFFF00FF: r 255, g 0, b 255, and its alpha byte 0xFF, so the key is the colour alone.
        const bool key = texel[0] == 0xFF && texel[1] == 0x00 && texel[2] == 0xFF;
        if (key || texel[3] == 0) texel[0] = texel[1] = texel[2] = texel[3] = 0;
    }
}

void StretchToPowerOfTwo(Image& image) {
    if (!Sized(image)) return;
    int wide = 1;
    int tall = 1;
    while (wide < image.width) wide *= 2;
    while (tall < image.height) tall *= 2;
    if (wide == image.width && tall == image.height) return;
    // up_scale_image returns 0 and leaves its output untouched for a target under two texels a side; no shipped
    // image is that small and SOIL's caller does not look at the result either.
    if (wide < 2 || tall < 2 || image.width < 2 || image.height < 2) return;

    const int width = image.width;
    const int height = image.height;
    const int channels = 4;
    std::vector<std::uint8_t> resampled(static_cast<std::size_t>(wide) * static_cast<std::size_t>(tall) * 4u);
    // image_helper.c:13-82, in its own float and its own order of operations.
    const float dx = (static_cast<float>(width) - 1.0f) / (static_cast<float>(wide) - 1.0f);
    const float dy = (static_cast<float>(height) - 1.0f) / (static_cast<float>(tall) - 1.0f);
    const std::uint8_t* const orig = image.rgba.data();
    for (int y = 0; y < tall; ++y) {
        float sampley = static_cast<float>(y) * dy;
        int inty = static_cast<int>(sampley);
        if (inty > height - 2) inty = height - 2;
        sampley -= static_cast<float>(inty);
        for (int x = 0; x < wide; ++x) {
            float samplex = static_cast<float>(x) * dx;
            int intx = static_cast<int>(samplex);
            if (intx > width - 2) intx = width - 2;
            samplex -= static_cast<float>(intx);
            int base = (inty * width + intx) * channels;
            for (int c = 0; c < channels; ++c) {
                float value = 0.5f;
                value += static_cast<float>(orig[base]) * (1.0f - samplex) * (1.0f - sampley);
                value += static_cast<float>(orig[base + channels]) * samplex * (1.0f - sampley);
                value += static_cast<float>(orig[base + width * channels]) * (1.0f - samplex) * sampley;
                value += static_cast<float>(orig[base + width * channels + channels]) * samplex * sampley;
                ++base;
                resampled[static_cast<std::size_t>(y) * static_cast<std::size_t>(wide) * 4u +
                          static_cast<std::size_t>(x) * 4u + static_cast<std::size_t>(c)] =
                    static_cast<std::uint8_t>(value);
            }
        }
    }
    image.rgba = std::move(resampled);
    image.width = wide;
    image.height = tall;
}

PlacedLight PlaceLight(const Receiver& receiver, const SourceLight& light) {
    PlacedLight placed;
    placed.colour = light.colour;
    // ComputeAbsoluteOrigin(size) for ET_HORIZONTAL (ETHEntity.cpp:270-299): where the node stands in the SCALED
    // bitmap, EO_CENTER, the size over two less the Sprite2D offset (the converter writes that as the negated pivot).
    const glm::dvec2 sizeScaled = glm::dvec2(receiver.units) * receiver.scale;
    const glm::dvec2 origin = sizeScaled * 0.5 - receiver.offsetPx;
    // BuildChildLight's position, moved with the entity to the target's corner (ETHScene.cpp:394-401):
    // newPos - oldPos + owner.pos, the height relative to the receiver's own z (the entity sits at z 0 in the bake).
    const glm::dvec2 rel = glm::dvec2(light.world) - receiver.nodePx;
    // The light, then the entity, are brought to scale 1: ETHLightmapGen.cpp:117-120 divides the light's position and
    // range by scale.y (ALL of the position, x included: the original's quirk), and the entity is placed at newPos
    // divided by its scale per axis (:82).
    const double scaleY = receiver.scale.y;
    const glm::dvec2 entityPos = origin / receiver.scale;
    placed.range = light.range / scaleY;
    glm::dvec2 at = (rel + origin) / scaleY;
    const double height = (light.world.z - receiver.z) / scaleY;
    if (receiver.turnRad != 0.0) {
        // BeginLightPass turns the LIGHT into the sprite's own frame by RotateZ(-angle) about the entity
        // (ETHPixelLightDiffuseSpecular.cpp:118-128); the sprite itself is drawn unturned (ETHRenderEntity.cpp:151).
        // With r Godot's rotation, the negation of the esc angle: d' = (d.x cos r + d.y sin r, -d.x sin r + d.y cos r).
        // d is built from rel itself rather than from (at - entityPos), so an unscaled sprite pays no add-then-subtract
        // rounding.
        const glm::dvec2 d = rel / scaleY + (origin / scaleY - entityPos);
        const double cosine = std::cos(receiver.turnRad);
        const double sine = std::sin(receiver.turnRad);
        at = entityPos + glm::dvec2(d.x * cosine + d.y * sine, -d.x * sine + d.y * cosine);
    }
    placed.at = glm::dvec3(at, height);
    return placed;
}

glm::dvec3 Pass(const PlacedLight& light, const glm::dvec3& point, const glm::dvec3& normal, const glm::dvec3& tint) {
    const glm::dvec3 v = light.at - point;
    const double d2 = v.x * v.x + v.y * v.y + v.z * v.z;
    const double r2 = light.range * light.range;
    // Exactly zero at or beyond the range, as the twin's `d2 >= r2` and the shader's min(d2, r2) both give.
    if (!(d2 < r2)) return glm::dvec3(0.0);
    const double attenuation = 1.0 - d2 / r2;
    const double facing = (v.x * normal.x + v.y * normal.y + v.z * normal.z) / std::sqrt(std::max(d2, kMinSquaredDistance));
    const double product = attenuation * facing;
    glm::dvec3 out(0.0);
    for (glm::length_t c = 0; c < 3; ++c) out[c] = std::clamp(tint[c] * light.colour[c] * product, 0.0, 1.0);
    return out;
}

int LevelOf(double value, int maxLevel, Rounding rounding) {
    const double scaled = std::clamp(value, 0.0, 1.0) * static_cast<double>(maxLevel);
    const double level = std::floor(rounding == Rounding::Nearest ? scaled + 0.5 : scaled);
    return std::clamp(static_cast<int>(level), 0, maxLevel);
}

int ByteOf(int level, int maxLevel) {
    // floor(level * 255 / max + 1/2) in integers. Neither 31 nor 63 can leave a tie (a tie needs 2 * level * 255 odd
    // multiples of max), so the half is never decided here.
    return (level * 255 * 2 + maxLevel) / (2 * maxLevel);
}

bool Bake(const Rules& rules, const Receiver& receiver, const Image& diffuseSource, const Image* normalSource,
          const std::vector<SourceLight>& lights, Baked& out, std::string& error) {
    out = Baked{};
    const int pitch = rules.unitsPerTexel;
    if (receiver.units.x <= 0 || receiver.units.y <= 0) {
        error = "the sprite is " + std::to_string(receiver.units.x) + "x" + std::to_string(receiver.units.y) + " units";
        return false;
    }
    if (!(receiver.scale.x > 0.0) || !(receiver.scale.y > 0.0)) {
        error = "the sprite's scale is not above 0";
        return false;
    }
    if (pitch != 1 && pitch != rules.reduction.factor) {
        error = "units_per_texel " + std::to_string(pitch) + " is neither 1 nor the reduction's factor " +
                std::to_string(rules.reduction.factor);
        return false;
    }
    if (receiver.units.x % pitch != 0 || receiver.units.y % pitch != 0) {
        error = "a sprite of " + std::to_string(receiver.units.x) + "x" + std::to_string(receiver.units.y) +
                " units has no whole number of texels at " + std::to_string(pitch) + " units a texel";
        return false;
    }
    if (!Sized(diffuseSource) || (normalSource != nullptr && !Sized(*normalSource))) {
        error = "an image whose length is not its width * height * 4";
        return false;
    }

    Image diffuse = diffuseSource;
    ApplyPixelMask(diffuse);
    StretchToPowerOfTwo(diffuse);
    Image normalMap;
    if (normalSource != nullptr) {
        normalMap = *normalSource;
        ApplyPixelMask(normalMap);
        StretchToPowerOfTwo(normalMap);
    }

    const int width = receiver.units.x;
    const int height = receiver.units.y;
    const std::vector<Tap> diffuseColumns = TapsOf(diffuse.width, width);
    const std::vector<Tap> diffuseRows = TapsOf(diffuse.height, height);
    std::vector<Tap> normalColumns;
    std::vector<Tap> normalRows;
    if (normalSource != nullptr) {
        normalColumns = TapsOf(normalMap.width, width);
        normalRows = TapsOf(normalMap.height, height);
    }
    std::vector<PlacedLight> placed;
    placed.reserve(lights.size());
    for (const SourceLight& light : lights) placed.push_back(PlaceLight(receiver, light));

    const std::array<int, 3> maxLevel{rules.MaxLevel(0), rules.MaxLevel(1), rules.MaxLevel(2)};
    // The unit lattice: levels[(row * width + column) * 3 + channel].
    std::vector<int> levels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3u, 0);
    for (int row = 0; row < height; ++row) {
        for (int column = 0; column < width; ++column) {
            // Texel (column, row)'s centre, at z 0: the sprite's plane (ETHScene.cpp:376-382).
            const glm::dvec3 point(static_cast<double>(column) + 0.5, static_cast<double>(row) + 0.5, 0.0);
            const glm::dvec3 texel = Fetch(diffuse, diffuseColumns[static_cast<std::size_t>(column)],
                                           diffuseRows[static_cast<std::size_t>(row)]);
            // N = 2n - 1, in the original's frame (y down), not renormalised; a flat map is (0, 0, 1).
            const glm::dvec3 encoded = normalSource != nullptr
                                           ? Fetch(normalMap, normalColumns[static_cast<std::size_t>(column)],
                                                   normalRows[static_cast<std::size_t>(row)])
                                           : glm::dvec3(0.5, 0.5, 1.0);
            const glm::dvec3 normal = encoded * 2.0 - 1.0;
            const glm::dvec3 tint = texel * receiver.colour;
            int sum[3] = {0, 0, 0};
            for (const PlacedLight& light : placed) {
                const glm::dvec3 pass = Pass(light, point, normal, tint);
                // ONE store per light into the black temporary target, then the lightmap target's add of two values
                // already on the same grid: an integer sum, saturating at the channel's top (ETHLightmapGen.cpp:91-167).
                for (std::size_t c = 0; c < 3; ++c) {
                    sum[c] += LevelOf(pass[static_cast<glm::length_t>(c)], maxLevel[c], rules.rounding);
                }
            }
            int* const at = &levels[(static_cast<std::size_t>(row) * static_cast<std::size_t>(width) +
                                     static_cast<std::size_t>(column)) * 3u];
            for (std::size_t c = 0; c < 3; ++c) at[c] = std::min(sum[c], maxLevel[c]);
        }
    }

    // The 8-bit expansion the engine's overlay is uploaded with.
    std::vector<std::uint8_t> bytes(levels.size());
    for (std::size_t i = 0; i < levels.size(); ++i) {
        bytes[i] = static_cast<std::uint8_t>(ByteOf(levels[i], maxLevel[i % 3]));
    }

    int outWidth = width;
    int outHeight = height;
    if (pitch > 1) {
        bytes = Reduce(rules.reduction, bytes, width, height);
        outWidth = width / pitch;
        outHeight = height / pitch;
        if (bytes.empty()) {
            error = "the unit bake of " + std::to_string(width) + "x" + std::to_string(height) + " units cannot be reduced";
            return false;
        }
    }

    out.width = outWidth;
    out.height = outHeight;
    out.rgba.assign(static_cast<std::size_t>(outWidth) * static_cast<std::size_t>(outHeight) * 4u, 255);
    for (std::size_t texel = 0; texel < static_cast<std::size_t>(outWidth) * static_cast<std::size_t>(outHeight); ++texel) {
        for (std::size_t c = 0; c < 3; ++c) out.rgba[texel * 4u + c] = bytes[texel * 3u + c];
    }
    return true;
}

std::vector<std::uint8_t> Reduce(const Rules::Reduction& reduction, const std::vector<std::uint8_t>& rgb, int width,
                                 int height) {
    const int factor = reduction.factor;
    const int span = static_cast<int>(reduction.taps.size());
    if (factor < 1 || span != 4 * factor || reduction.denominator < 1 || width < factor || height < factor ||
        width % factor != 0 || height % factor != 0 ||
        rgb.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3u) {
        return {};
    }
    // Output texel k stands at unit 2k + 1, between source texels 2k and 2k + 1, with taps over 2k - 3 .. 2k + 4. Every
    // product is a whole number over the denominator, a power of two, so the sums are exact and the final rounding is
    // decided without noise.
    const double denominator = static_cast<double>(reduction.denominator);
    const int reach = span / 2 - 1;  // 3 for 8 taps: the first tap is 3 source texels before 2k
    const int outWidth = width / factor;
    const int outHeight = height / factor;
    const auto tap = [&reduction](int t) { return static_cast<double>(reduction.taps[static_cast<std::size_t>(t)]); };

    // Rows first: every column, outHeight rows.
    std::vector<double> rows(static_cast<std::size_t>(width) * static_cast<std::size_t>(outHeight) * 3u, 0.0);
    for (int k = 0; k < outHeight; ++k) {
        for (int x = 0; x < width; ++x) {
            for (std::size_t c = 0; c < 3; ++c) {
                double sum = 0.0;
                for (int t = 0; t < span; ++t) {
                    const int source = Mirror(factor * k - reach + t, height);
                    sum += tap(t) * static_cast<double>(rgb[(static_cast<std::size_t>(source) * static_cast<std::size_t>(width) +
                                                            static_cast<std::size_t>(x)) * 3u + c]);
                }
                rows[(static_cast<std::size_t>(k) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)) * 3u + c] =
                    sum / denominator;
            }
        }
    }
    // Then columns, and to the nearest 8-bit value: the taps overshoot at a step, so it is clamped.
    std::vector<std::uint8_t> out(static_cast<std::size_t>(outWidth) * static_cast<std::size_t>(outHeight) * 3u, 0);
    for (int y = 0; y < outHeight; ++y) {
        for (int k = 0; k < outWidth; ++k) {
            for (std::size_t c = 0; c < 3; ++c) {
                double sum = 0.0;
                for (int t = 0; t < span; ++t) {
                    const int source = Mirror(factor * k - reach + t, width);
                    sum += tap(t) * rows[(static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                                          static_cast<std::size_t>(source)) * 3u + c];
                }
                const double value = std::clamp(std::floor(sum / denominator + 0.5), 0.0, 255.0);
                out[(static_cast<std::size_t>(y) * static_cast<std::size_t>(outWidth) + static_cast<std::size_t>(k)) * 3u + c] =
                    static_cast<std::uint8_t>(value);
            }
        }
    }
    return out;
}

} // namespace MagicPortals::LightmapBake
