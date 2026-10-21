#include "sim/Lighting.hpp"

#include "sim/Sprites.hpp"

#include "core/Json.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <sstream>
#include <string_view>
#include <system_error>
#include <unordered_set>
#include <utility>
#include <vector>

namespace MagicPortals::Lighting {

namespace {

constexpr std::string_view kMetaPrefix = "metadata/";
constexpr std::string_view kEthPrefix = "eth_";
constexpr std::string_view kRes = "res://";

enum class Id : std::size_t {
    Ambient,
    Intensity,
    Z,
    Emissive,
    Static,
    ApplyLight,
    Colour,
    Normal,
    Lightmap,
    LightRange,
    LightOffset,
    LightColour,
    Halo,
    HaloOffset,
    HaloSize,
    HaloBrightness,
    Particles,
    Count,
};

// Where the converter writes a key (its tscn.py, _entity_lighting and
// _UNGATED_ENTITY_KEYS). Anywhere: on every entity node whose value is not the
// default. Drawn: only on an entity node that draws a sprite or owns a light.
enum class Place { Root, Anywhere, Drawn };

struct Key {
    std::string_view name;
    Id id;
    Place place;
};

// The whole vocabulary, as the converter wrote it on 14 September 2026 (at
// Enhanced's b572fec), and eth_particles, which it writes since planning step 90.
// A key not listed here is refused, never skipped: a lighting input the
// port silently ignored would be a level drawn wrong with nothing saying why.
constexpr Key kKeys[] = {
    {"eth_ambient", Id::Ambient, Place::Root},
    {"eth_light_intensity", Id::Intensity, Place::Root},
    {"eth_z", Id::Z, Place::Anywhere},
    {"eth_emissive", Id::Emissive, Place::Anywhere},
    {"eth_static", Id::Static, Place::Drawn},
    {"eth_apply_light", Id::ApplyLight, Place::Drawn},
    {"eth_color", Id::Colour, Place::Drawn},
    {"eth_normal", Id::Normal, Place::Drawn},
    {"eth_lightmap", Id::Lightmap, Place::Drawn},
    {"eth_light_range", Id::LightRange, Place::Drawn},
    {"eth_light_offset", Id::LightOffset, Place::Drawn},
    {"eth_light_color", Id::LightColour, Place::Drawn},
    {"eth_halo", Id::Halo, Place::Drawn},
    {"eth_halo_offset", Id::HaloOffset, Place::Drawn},
    {"eth_halo_size", Id::HaloSize, Place::Drawn},
    {"eth_halo_brightness", Id::HaloBrightness, Place::Drawn},
    // Not a lighting input: the node's own <Particles>, as the sidecar file the
    // converter writes for it (planning step 90). Read here because this is where
    // an eth_ key is known or refused - an unknown one draws the whole level
    // unlit - and read into the Look, rather than only let through, so the layer
    // loads exactly the path this reader found to be a file. Refused on a node
    // that draws no sprite, below (step 91).
    {"eth_particles", Id::Particles, Place::Drawn},
};

constexpr std::size_t kIdCount = static_cast<std::size_t>(Id::Count);

const Key* Find(std::string_view name) {
    for (const Key& key : kKeys)
        if (key.name == name) return &key;
    return nullptr;
}

std::string_view NameOf(Id id) {
    for (const Key& key : kKeys)
        if (key.id == id) return key.name;
    return {};
}

std::string At(const Tscn::Node& node) {
    return "line " + std::to_string(node.line) + ": " + node.name + ": ";
}

// Exactly `count` finite numbers with one space between each, which is how the
// converter's _nums writes them. from_chars rather than strtod, which takes its
// decimal point from the C locale.
bool Numbers(std::string_view text, std::size_t count, double* out) {
    std::size_t got = 0;
    std::size_t from = 0;
    while (true) {
        const std::size_t space = text.find(' ', from);
        const std::string_view item =
            text.substr(from, space == std::string_view::npos ? std::string_view::npos : space - from);
        if (item.empty() || got == count) return false;
        double value = 0.0;
        const char* last = item.data() + item.size();
        const auto [ptr, ec] = std::from_chars(item.data(), last, value);
        if (ec != std::errc() || ptr != last || !std::isfinite(value)) return false;
        out[got++] = value;
        if (space == std::string_view::npos) break;
        from = space + 1;
    }
    return got == count;
}

std::string CountWord(std::size_t count) {
    switch (count) {
    case 1: return "one number";
    case 2: return "two numbers";
    case 3: return "three numbers";
    case 4: return "four numbers";
    default: return std::to_string(count) + " numbers";
    }
}

bool Vector(const std::string& text, glm::dvec3& out, std::string& why) {
    double v[3] = {};
    if (!Numbers(text, 3, v)) {
        why = "\"" + text + "\" is not " + CountWord(3);
        return false;
    }
    out = glm::dvec3(v[0], v[1], v[2]);
    return true;
}

bool Vector(const std::string& text, glm::dvec2& out, std::string& why) {
    double v[2] = {};
    if (!Numbers(text, 2, v)) {
        why = "\"" + text + "\" is not " + CountWord(2);
        return false;
    }
    out = glm::dvec2(v[0], v[1]);
    return true;
}

bool Scalar(const std::string& text, double& out, std::string& why) {
    if (!Numbers(text, 1, &out)) {
        why = "\"" + text + "\" is not " + CountWord(1);
        return false;
    }
    return true;
}

bool Flag(const std::string& text, bool& out, std::string& why) {
    if (text != "0" && text != "1") {
        why = "\"" + text + "\" is not \"0\" or \"1\"";
        return false;
    }
    out = text == "1";
    return true;
}

// A res:// path, resolved against resRoot, whether or not it names a file.
bool ResPath(const std::string& text, const std::string& resRoot, std::string& out, std::string& why) {
    if (!text.starts_with(kRes)) {
        why = "\"" + text + "\" is not a res:// path";
        return false;
    }
    out = resRoot + "/" + text.substr(kRes.size());
    return true;
}

// A res:// path, resolved against resRoot, that names a file.
bool Path(const std::string& text, const std::string& resRoot, std::string& out, std::string& why) {
    if (!ResPath(text, resRoot, out, why)) return false;
    std::error_code ec;
    if (!std::filesystem::is_regular_file(out, ec)) {
        why = out + " is not a file";
        return false;
    }
    return true;
}

} // namespace

bool Read(const Tscn::Scene& scene, const std::string& resRoot, const Tiers::Rules& tiers, Scene& out,
          std::string& error) {
    out = Scene{};

    // Which nodes draw a sprite. Sprites::Find is the reader the layer draws with, so
    // the answer is the layer's rather than a second reading of the same scene. (Until
    // CMP-14 it also gave the units a lightmap was checked against; the lightmap is now
    // baked from the sprite's units and is not a file read.)
    std::vector<Sprites::Sprite> sprites;
    if (!Sprites::Find(scene, resRoot, tiers, sprites, error)) return false;
    std::unordered_set<std::string> spriteNodes;
    for (const Sprites::Sprite& sprite : sprites) spriteNodes.insert(sprite.node);

    Scene read;
    const Tscn::Node* root = nullptr;
    bool haveAmbient = false;
    bool haveIntensity = false;
    std::string why;
    const auto fail = [&](const Tscn::Node& node, const std::string& message) {
        error = At(node) + message;
        return false;
    };

    for (const Tscn::Node& node : scene.nodes) {
        const bool isRoot = node.path == ".";
        const bool isEntity = node.parent == ".";
        if (isRoot) root = &node;

        Look look;
        Light light;
        std::array<bool, kIdCount> has{};

        for (const Tscn::Property& property : node.properties) {
            if (!property.key.starts_with(kMetaPrefix)) continue;
            const std::string name = property.key.substr(kMetaPrefix.size());
            if (!name.starts_with(kEthPrefix)) continue;

            const Key* key = Find(name);
            if (key == nullptr) return fail(node, name + " is not a lighting key the port reads");
            if (!isRoot && !isEntity)
                return fail(node, name + " below an entity node, where the converter writes no lighting");
            if (isRoot && key->place != Place::Root) return fail(node, name + " is an entity's key, on the root");
            if (!isRoot && key->place == Place::Root) return fail(node, name + " is the scene's key, on an entity");
            if (property.value.kind != Tscn::Value::Kind::String)
                return fail(node, name + " is not a quoted string");
            has[static_cast<std::size_t>(key->id)] = true;

            const std::string& text = property.value.text;
            bool ok = true;
            switch (key->id) {
            case Id::Ambient: ok = Vector(text, read.ambient, why); haveAmbient = ok; break;
            case Id::Intensity: ok = Scalar(text, read.intensity, why); haveIntensity = ok; break;
            case Id::Z: ok = Scalar(text, look.z, why); break;
            case Id::Emissive: ok = Vector(text, look.emissive, why); break;
            case Id::Static: ok = Flag(text, look.isStatic, why); break;
            case Id::ApplyLight: ok = Flag(text, look.applyLight, why); break;
            case Id::Colour: {
                double v[4] = {};
                ok = Numbers(text, 4, v);
                if (ok) look.colour = glm::dvec4(v[0], v[1], v[2], v[3]);
                else why = "\"" + text + "\" is not " + CountWord(4);
                break;
            }
            case Id::Normal: ok = Path(text, resRoot, look.normal, why); break;
            // A marker, not a file the port reads (CMP-14): resolved, not required to exist.
            case Id::Lightmap: ok = ResPath(text, resRoot, look.lightmap, why); break;
            case Id::LightRange:
                ok = Scalar(text, light.range, why);
                // The engine keeps a light only if its range is positive
                // (ETHEntityProperties.cpp:343), so a converter that wrote one
                // that is not has lost track of which lights exist.
                if (ok && !(light.range > 0.0)) {
                    ok = false;
                    why = "\"" + text + "\" is not a positive range";
                }
                break;
            case Id::LightOffset: ok = Vector(text, light.offset, why); break;
            case Id::LightColour: ok = Vector(text, light.colour, why); break;
            case Id::Halo: ok = Path(text, resRoot, light.halo, why); break;
            case Id::HaloOffset: ok = Vector(text, light.haloOffset, why); break;
            case Id::HaloSize: ok = Vector(text, light.haloSize, why); break;
            case Id::HaloBrightness: ok = Scalar(text, light.haloBrightness, why); break;
            case Id::Particles: ok = Path(text, resRoot, look.particles, why); break;
            case Id::Count: break;
            }
            if (!ok) return fail(node, name + ": " + why);
        }

        if (!isEntity) continue;

        const auto present = [&](Id id) { return has[static_cast<std::size_t>(id)]; };
        const bool drawsSprite = spriteNodes.count(node.name) != 0;
        const bool ownsLight = present(Id::LightRange);

        // A light is identified by its range, which the converter always writes;
        // a halo by its bitmap. The rest of either on its own describes nothing.
        for (const Id id : {Id::LightOffset, Id::LightColour, Id::Halo, Id::HaloOffset, Id::HaloSize, Id::HaloBrightness}) {
            if (present(id) && !ownsLight)
                return fail(node, std::string(NameOf(id)) + " without eth_light_range, which is what makes a light");
        }
        for (const Id id : {Id::HaloOffset, Id::HaloSize, Id::HaloBrightness}) {
            if (present(id) && !present(Id::Halo))
                return fail(node, std::string(NameOf(id)) + " without eth_halo, which is what makes a halo");
        }
        for (const Key& key : kKeys) {
            if (key.place == Place::Drawn && present(key.id) && !drawsSprite && !ownsLight)
                return fail(node, std::string(key.name) + " on a node that neither draws a sprite nor owns a light");
        }
        // The layer draws particle systems for the level's sprites only
        // (buildEmitters walks them), so on a node that owns a light and draws
        // no sprite the key would be read and never drawn - a skip. The converter
        // writes it only where it writes a Sprite2D (planning step 90). A PORT
        // limit, not the engine's: the original draws a spriteless entity's
        // particles (ETHSpriteEntity.cpp:106), and no shipped level has one.
        if (present(Id::Particles) && !drawsSprite)
            return fail(node, "eth_particles on a node that draws no sprite, whose particles the port does not draw");

        if (present(Id::Lightmap)) {
            // Only a static, lit entity with a picture is baked
            // (ETHLightmapGen.cpp:42), and the converter attaches no other.
            if (!look.isStatic) return fail(node, "eth_lightmap on a node that is not static");
            if (!look.applyLight) return fail(node, "eth_lightmap on a node that does not apply light");
            if (!drawsSprite) return fail(node, "eth_lightmap on a node that draws no sprite");

            // And nothing more: until CMP-14 the file was opened and had to be half its
            // sprite's units on both axes (730 of 730 are), because the port drew it. The
            // original does not read it at a screen of 480 px or more; it bakes the
            // lightmap at the sprite's bitmap size in units (ETHLightmapGen.cpp:57-58,
            // with the entity's scale set to 1, :117-118, so a <Scale> stretches the
            // picture and the lightmap over it alike), and so does the port now
            // (LightmapBake). The path is the node's identity and its bake's key.
        }

        if (ownsLight) look.light = std::move(light);
        read.nodes.emplace(node.name, std::move(look));
    }

    // Tscn::Parse refuses a scene with no nodes, so the root is always there.
    if (root == nullptr) {
        error = "the scene has no root node";
        return false;
    }
    if (!haveAmbient) return fail(*root, "the root has no eth_ambient, and every level file declares one");
    if (!haveIntensity) return fail(*root, "the root has no eth_light_intensity, and every level file declares one");

    out = std::move(read);
    return true;
}

bool LoadRules(const std::string& path, Rules& out, std::string& error) {
    namespace Json = Supersonic::Json;
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = path + ": cannot open";
        return false;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    // Named, not a temporary: the parser keeps a reference to what it reads.
    const std::string text = buffer.str();
    Json::Parser parser(text);
    Json::Value root;
    if (!parser.Parse(root)) {
        error = path + ": " + parser.Error();
        return false;
    }
    if (!root.IsObject()) {
        error = path + ": not an object";
        return false;
    }
    const auto light = [&](const char* key, glm::dvec3& into) {
        const Json::Value& value = root[key]["value"];
        bool ok = root.Has(key) && root[key].IsObject() && value.IsArray() && value.AsArray().size() == 3;
        for (std::size_t i = 0; ok && i < 3; ++i) {
            const Json::Value& channel = value.AsArray()[i];
            ok = channel.IsNumber() && std::isfinite(channel.AsNumber()) && channel.AsNumber() >= 0.0 &&
                 channel.AsNumber() <= 1.0;
            if (ok) into[static_cast<glm::length_t>(i)] = channel.AsNumber();
        }
        if (!ok) error = path + ": " + key + ".value is not three numbers from 0 to 1";
        return ok;
    };
    Rules read;
    if (!light("darkest_ambient", read.darkestAmbient) || !light("torch_lit_ambient", read.torchLitAmbient)) {
        return false;
    }
    const Json::Value& greenDown = root["normal_map_green_down"]["value"];
    if (!root.Has("normal_map_green_down") || !root["normal_map_green_down"].IsObject() || !greenDown.IsBool()) {
        error = path + ": normal_map_green_down.value is not true or false";
        return false;
    }
    read.normalMapGreenDown = greenDown.AsBool();
    const Json::Value& scale = root["halo_brightness_scale"]["value"];
    if (!root.Has("halo_brightness_scale") || !root["halo_brightness_scale"].IsObject() || !scale.IsNumber() ||
        !std::isfinite(scale.AsNumber()) || scale.AsNumber() < 0.0 || scale.AsNumber() > 1.0) {
        error = path + ": halo_brightness_scale.value is not a number from 0 to 1";
        return false;
    }
    read.haloBrightnessScale = scale.AsNumber();
    const Json::Value& rgb565 = root["framebuffer_rgb565"]["value"];
    if (!root.Has("framebuffer_rgb565") || !root["framebuffer_rgb565"].IsObject() || !rgb565.IsBool()) {
        error = path + ": framebuffer_rgb565.value is not true or false";
        return false;
    }
    read.framebufferRgb565 = rgb565.AsBool();

    // The script's emissive overrides: two datum names and the character's two values, each a light from 0 to 1
    // (the original's are 0, 0.1 and 0.2), and the world the second one belongs to.
    const Json::Value& script = root["script_emissive"];
    const auto name = [&](const char* key, std::string& into) {
        const bool ok = script.IsObject() && script.Has(key) && script[key].IsString() &&
                        !script[key].AsString("").empty();
        if (ok) {
            into = script[key].AsString("");
        } else {
            error = path + ": script_emissive." + key + " is not a name";
        }
        return ok;
    };
    const auto channels = [&](const Json::Value& value, const char* what, glm::dvec3& into) {
        bool ok = value.IsArray() && value.AsArray().size() == 3;
        for (std::size_t i = 0; ok && i < 3; ++i) {
            const Json::Value& channel = value.AsArray()[i];
            ok = channel.IsNumber() && std::isfinite(channel.AsNumber()) && channel.AsNumber() >= 0.0 &&
                 channel.AsNumber() <= 1.0;
            if (ok) into[static_cast<glm::length_t>(i)] = channel.AsNumber();
        }
        if (!ok) error = path + ": script_emissive." + what + " is not three numbers from 0 to 1";
        return ok;
    };
    if (!name("flag", read.ignoreEmissiveKey) || !name("door_key", read.doorEmissiveKey)) return false;
    const Json::Value& player = script["player"];
    if (!player.IsObject()) {
        error = path + ": script_emissive.player is not an object";
        return false;
    }
    if (!channels(player["value"], "player.value", read.playerEmissive) ||
        !channels(player["lit_value"], "player.lit_value", read.playerLitEmissive)) {
        return false;
    }
    const Json::Value& litWorld = player["lit_world"];
    if (!litWorld.IsNumber() || litWorld.AsNumber() != std::floor(litWorld.AsNumber()) ||
        litWorld.AsNumber() < 0.0 || litWorld.AsNumber() > 3.0) {
        error = path + ": script_emissive.player.lit_world is not a whole number from 0 to 3";
        return false;
    }
    read.playerLitWorld = static_cast<int>(litWorld.AsNumber());

    // The lightmap bake, last: a file without the block is refused by its name (CMP-14).
    std::string bakeError;
    if (!LightmapBake::ParseRules(root["lightmap_bake"], read.lightmapBake, bakeError)) {
        error = path + ": " + bakeError;
        return false;
    }
    out = read;
    return true;
}

LightmapBake::Receiver BakeReceiver(const Sprites::Sprite& sprite, const Look& look) {
    LightmapBake::Receiver receiver;
    receiver.units = Tiers::Units(sprite.texels, sprite.density);
    receiver.scale = sprite.scale;
    receiver.nodePx = sprite.atPx;
    receiver.offsetPx = sprite.offsetPx;
    receiver.turnRad = sprite.rotation;
    receiver.z = look.z;
    receiver.colour = glm::dvec3(look.colour);
    return receiver;
}

bool BakeLights(const Tscn::Scene& scene, const Scene& lighting, std::vector<LightmapBake::SourceLight>& out,
                std::string& error) {
    out.clear();
    for (const Tscn::Node& node : scene.nodes) {
        if (node.parent != ".") continue;
        const auto look = lighting.nodes.find(node.name);
        if (look == lighting.nodes.end() || !look->second.light || !look->second.isStatic) continue;
        const Tscn::Value* at = node.Find("position");
        if (at == nullptr || at->kind != Tscn::Value::Kind::Vector2) {
            error = node.name + " owns a static light and has no position";
            out.clear();
            return false;
        }
        // The owner scale of a level placement is 1 (OwnerScaled, K7), which leaves offset and range as they are.
        const Light from = OwnerScaled(*look->second.light, glm::dvec2(1.0));
        LightmapBake::SourceLight light;
        light.world = glm::dvec3(at->numbers[0] + from.offset.x, at->numbers[1] + from.offset.y,
                                 look->second.z + from.offset.z);
        light.range = from.range;
        light.colour = LightColour(from, lighting.intensity, true, 1.0);
        out.push_back(light);
    }
    return true;
}

ScriptOverride ScriptEmissive(const Rules& rules, const Tscn::Node* properties, int world) {
    ScriptOverride out;
    if (properties == nullptr || properties->Meta(rules.ignoreEmissiveKey) == nullptr) return out;
    out.ignore = true;
    // door_emissive is read inside the flag's branch and nowhere else; absent it is emissiveValue's start, 0.
    double door = 0.0;
    if (const Tscn::Value* value = properties->Meta(rules.doorEmissiveKey); value != nullptr) value->AsNumber(door);
    out.door = glm::dvec3(door);
    out.player = world == rules.playerLitWorld ? rules.playerLitEmissive : rules.playerEmissive;
    return out;
}

std::uint8_t ReceiverMask(bool isStatic, bool applyLight, bool runtimeBake) {
    if (!applyLight) return 0;
    return isStatic && !runtimeBake ? kLiveLights : static_cast<std::uint8_t>(kLiveLights | kStaticLights);
}

bool RuntimeBake(const Torch::State& torch) {
    return torch.lit > 0;
}

std::uint8_t LightLayer(bool ownerStatic) {
    return ownerStatic ? kStaticLights : kLiveLights;
}

double ParticleRatio(int active, int total) {
    if (total <= 0) return 1.0;
    return static_cast<double>(std::clamp(active, 0, total)) / static_cast<double>(total);
}

glm::dvec3 LightColour(const Light& light, double intensity, bool ownerStatic, double ratio) {
    return light.colour * intensity * (ownerStatic ? 1.0 : ratio);
}

glm::dvec3 HaloColour(const Light& light, double ratio, double scale) {
    return light.colour * light.haloBrightness * ratio * scale;
}

double HaloScale(const Rules& rules, const std::optional<double>& own) {
    return own ? *own : rules.haloBrightnessScale;
}

Light OwnerScaled(const Light& light, const glm::dvec2& ownerScale) {
    Light scaled = light;
    // BuildChildLight: the light's position and range take scale.y alone.
    scaled.offset *= ownerScale.y;
    scaled.range *= ownerScale.y;
    // DrawHalo: the size takes both, and the halo stands at the UNSCALED position.
    scaled.haloSize *= ownerScale;
    return scaled;
}

bool FileAmbient(const Tscn::Scene& scene, glm::dvec3& out) {
    for (const Tscn::Node& node : scene.nodes) {
        if (node.path != ".") continue;
        const Tscn::Value* value = node.Meta("eth_ambient");
        glm::dvec3 read(0.0);
        std::string why;
        if (value == nullptr || value->kind != Tscn::Value::Kind::String || !Vector(value->text, read, why)) {
            return false;
        }
        out = read;
        return true;
    }
    return false;
}

glm::dvec3 Ambient(const Rules& rules, const glm::dvec3& fileAmbient, bool darkest, const Torch::State& torch) {
    const bool litNow = std::any_of(torch.lights.begin(), torch.lights.end(),
                                    [](const Torch::Light& light) { return light.lit; });
    if (litNow) return rules.torchLitAmbient;
    if (torch.putOut > 0 || darkest) return rules.darkestAmbient;
    return fileAmbient;
}

glm::dvec3 Ambient(const Rules& rules, const glm::dvec3& fileAmbient, bool darkest, const Torch::State& torch,
                   const RedKey::State& redKey) {
    if (!redKey.present || !redKey.destroyed) return Ambient(rules, fileAmbient, darkest, torch);
    const glm::dvec3 saved = darkest ? rules.darkestAmbient : fileAmbient;
    return saved * redKey.bias;
}

glm::dvec3 AmbientTerm(const glm::dvec3& ambient, const glm::dvec3& emissive) {
    return glm::min(glm::dvec3(1.0), ambient + emissive);
}

} // namespace MagicPortals::Lighting
