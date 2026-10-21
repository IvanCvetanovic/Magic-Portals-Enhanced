#pragma once

// THE LIGHTMAP OF A STATIC LIT SPRITE, BAKED AT ONE TEXEL PER BITMAP UNIT (CMP-14, planning step 196).
//
// At a screen 480 px or taller the original does not read the add<id>.png files it ships: its
// shouldUsePrerenderedLights is GetScreenSize().y < 480 (Enhanced's out/parity/asbc/all_functions.txt:61740,
// bytes 313283..313382), the flag EnablePreLoadedLightmapsFromFile defaults to false
// (ETHScriptWrapper.cpp:39), and LoadLightmaps() at the end of LoadScene therefore calls GenerateLightmaps()
// (ETHScriptWrapper.Scene.cpp:198-204, :607-609). That bakes every static applyLight sprite into a render
// target of its BITMAP SIZE IN UNITS (ETHLightmapGen.cpp:57-58): one texel per unit, 2.8125 px on the 1280x720
// rig. The files are half the sprite per axis, so the port, which drew them, drew a lightmap twice as coarse
// as the original's. This computes the original's, on the CPU, with the arithmetic of the light shader
// (hPixelLightDiff.ps, the GLSL form the rig runs) and the geometry around it that the calibration fixed
// (out/parity/program/CMP14/pre/calibration.md, its choices R1-R20); the engine's Light2D twin
// (engine/src/core/Light2D.hpp) is the oracle of the light term in a test, not a dependency.
//
// Pure: no I/O, no clock, no thread, no renderer, no read of anything but the arguments. Double throughout
// but for SOIL's own float resampler, which the original ran in float. The only roundings are the target's.
// A bake is never written anywhere; the layer hands the pixels to the texture registry and nothing else
// (CLAUDE.md, THE HARD RULE).

#include "core/Json.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

namespace MagicPortals::LightmapBake {

// A baked lightmap is registered under a name no file has, so a sprite that names it can never fall back to
// the shipped add<id>.png: an unregistered key fails loudly (the texture registry's "Could not load") and
// draws black. The node's own eth_lightmap path stays the SUFFIX, which keeps the node's identity (730 distinct
// paths, one per node and level) and every "ends with /add696.png" a test and a log reader looks for.
inline constexpr std::string_view kKeyPrefix = "mp-bake:";
std::string KeyOf(const std::string& marker);

// How a channel is rounded to the target's levels. The original's target is GL_UNSIGNED_SHORT_5_6_5
// (GLES2Texture.cpp:150-170) and nothing in the source says how the float-to-fixed store rounds (calibration
// R9: nearest, the engine's own Rgb565 rule, a _guess in lighting.json). Truncate exists so a test can pin the
// other reading.
enum class Rounding { Nearest, Truncate };

// lighting.json `lightmap_bake`.
struct Rules {
    // The OUTPUT pitch, in units per texel. The light is always evaluated at the unit lattice, as the original
    // does at >= 480 px; 1 is the game. The reduction's factor is the only other pitch: the shipped files are
    // that bake reduced 2:1 (calibration R10), which the G0 self-check of the step uses.
    int unitsPerTexel = 1;
    // The target's bits per channel (GL RGB, UNSIGNED_SHORT_5_6_5); 8, 8, 8 for the G0 self-check, the shipped
    // files being 8-bit data.
    std::array<int, 3> bits{5, 6, 5};
    Rounding rounding = Rounding::Nearest;
    // The 2:1 reduction of a unit bake into the shipped pitch: separable, fixed taps over source texels
    // 2k - 3 .. 2k + 4, mirror edge (calibration R10). Read only when unitsPerTexel > 1.
    struct Reduction {
        int factor = 2;
        std::vector<int> taps{-3, -9, 29, 111, 111, 29, -9, -3};
        int denominator = 256;
    } reduction;

    // The largest level a channel holds: 31, 63, 31.
    int MaxLevel(std::size_t channel) const { return (1 << bits[channel]) - 1; }
};

// False, with `error` naming the key, for a block that is not exactly what lighting.json writes (every key
// required, nothing else read): see Lighting::LoadRules.
bool ParseRules(const Supersonic::Json::Value& block, Rules& out, std::string& error);

// A decoded source, rows top to bottom, RGBA8 (width * height * 4 bytes). A file with no alpha is decoded with
// alpha 255, as the original's loader forces RGBA whenever it masks (GLES2Texture.cpp:283-300).
struct Image {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgba;
};

// GLES2Texture.cpp:73-92: a texel that is exactly (255, 0, 255) or has alpha 0 becomes (0, 0, 0, 0), on an RGB
// file too (its alpha is 255 and the key still applies). Both the sprite and its normal map are loaded masked
// (ETHResourceManager.cpp:136; ETHSpriteEntity.cpp:92-100).
void ApplyPixelMask(Image& image);

// SOIL_FLAG_POWER_OF_TWO (GLES2Texture.cpp:303): an axis that is not a power of two is stretched up to the next
// one by up_scale_image (SOIL's image_helper.c:13-82): corner-aligned bilinear, +0.5, truncated to 8 bits. A
// float32 routine in the original and kept so here (the operations in SOIL's order), the one place this module
// is not double: the original's bytes are what the bake samples. A power-of-two image is not touched. The
// sprite's uv still spans [0, 1] over the stretched image.
void StretchToPowerOfTwo(Image& image);

// One receiver sprite, in the original's terms (ETHLightmapGen.cpp, ETHEntity::ComputeAbsoluteOrigin).
struct Receiver {
    glm::ivec2 units{0};        // Tiers::Units(texels, density): the bitmap in units, BEFORE its scale
    glm::dvec2 scale{1.0};      // the entity's <Scale> (Sprites::Sprite::scale); lights are divided by scale.y
    glm::dvec2 nodePx{0.0};     // the entity node's position, in the level's units
    glm::dvec2 offsetPx{0.0};   // the Sprite2D offset x scale, in the node's frame: the NEGATED pivot
    double turnRad = 0.0;       // Sprites::Sprite::rotation, Godot's radians: clockwise on the screen, the esc angle's negation
    double z = 0.0;             // the node's eth_z, unrounded: the plane every light's height is measured from
    glm::dvec3 colour{1.0};     // the instance colour's rgb (eth_color; white in every shipped lightmapped sprite)
};

// A static light as ETHScene::GenerateLightmaps builds it (BuildChildLight, ETHEntityRenderingManager.cpp:174-184),
// in the LEVEL's units.
struct SourceLight {
    glm::dvec3 world{0.0};      // owner position + the light's offset; z = the owner's eth_z + the offset's z
    double range = 0.0;         // units
    glm::dvec3 colour{1.0};     // the light's colour x the scene's lightIntensity (Lighting::LightColour, static owner)
};

// A light in the sprite's own frame: x right, y DOWN the sprite, z towards the viewer, origin at the top-left
// of its bitmap, in units, as the original's pass runs it (ETHPixelLightDiffuseSpecular.cpp:118-146).
struct PlacedLight {
    glm::dvec3 at{0.0};
    double range = 0.0;
    glm::dvec3 colour{1.0};
};
PlacedLight PlaceLight(const Receiver& receiver, const SourceLight& light);

// What one light adds to one texel before the store, clamped to [0, 1]: the GLSL pass (hPixelLightDiff.ps,
// shaders.h:1261-1304)
//   clamp(tint * colour * (1 - d2/r2) * dot(L - P, N) / |L - P|),  0 at d2 >= r2,
// N the normal map's texel decoded as 2n - 1 and NOT renormalised, no factor of the diffuse alpha, no alpha test.
// The same number the engine's Light2D::Contribution gives (a test holds one to the other).
glm::dvec3 Pass(const PlacedLight& light, const glm::dvec3& point, const glm::dvec3& normal, const glm::dvec3& tint);

// The level a value in [0, 1] stores as, in a channel of `maxLevel`: floor(v * max + 1/2) (Nearest) or
// floor(v * max) (Truncate), clamped to the channel.
int LevelOf(double value, int maxLevel, Rounding rounding);

// round(level * 255 / maxLevel): the engine's own 5/6/5 -> 8 bit rule (engine/ARCHITECTURE.md), at most 0.5/255
// from the exact fraction. The identity for an 8-bit channel.
int ByteOf(int level, int maxLevel);

struct Baked {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgba;   // width * height * 4, rows top to bottom, alpha 255
};

// The 2:1 reduction of calibration R10 on its own: `rgb` is width * height * 3 bytes (both sizes even), reduced
// separably, rows first, by the fixed taps over source texels 2k - 3 .. 2k + 4 with a MIRROR edge, in double, the
// intermediate neither rounded nor clamped, then to the nearest 8-bit value and clamped to [0, 255] (a tie rounds up).
// Returns (width / 2) * (height / 2) * 3 bytes, or nothing for a size it cannot reduce. The shipped add<id>.png files
// are the unit bake reduced this way; the game never does (it bakes at one unit a texel) and the G0 self-check of
// planning step 196 does.
std::vector<std::uint8_t> Reduce(const Rules::Reduction& reduction, const std::vector<std::uint8_t>& rgb, int width,
                                 int height);

// The lightmap of one sprite: for every texel of the unit lattice, centre at (k + 1/2) units from the sprite's
// top-left, ONE bilinear tap of each map at the texel's uv (clamped, no mip), each light's pass stored once
// (clamped to [0, 1], rounded to the target) into a black temporary, the lightmap the saturating integer sum of
// those levels (ETHLightmapGen.cpp:86-168). With Rules::unitsPerTexel above 1 the unit bake is reduced to that
// pitch (calibration R10). `normal` null is a flat map (0, 0, 1). The sources are copied, masked and stretched
// here, in that order. Deterministic: the iteration order is rows, columns, lights in the order given.
// False, with `error`, for a sprite or an image it cannot bake (a size the pitch does not divide, an image
// whose length is not its size): `out` is then empty.
bool Bake(const Rules& rules, const Receiver& receiver, const Image& diffuse, const Image* normal,
          const std::vector<SourceLight>& lights, Baked& out, std::string& error);

} // namespace MagicPortals::LightmapBake
