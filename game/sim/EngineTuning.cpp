#include "sim/EngineTuning.hpp"

#include "core/Json.hpp"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace MagicPortals::EngineTuning {

namespace Json = Supersonic::Json;

namespace {

// The keys of the "mobile" object, which is the block's whole vocabulary.
constexpr const char* kMobileKey = "mobile";
constexpr const char* kMinimumVulkanMinorKey = "minimum_vulkan_minor";
constexpr const char* kShadowMapKey = "shadow_map_resolution";
constexpr const char* kAssetWatchingKey = "asset_watching";

// The shadow-map rule, in one place for the table's own key and the phone block's: 0, or a whole number from the engine's floor to its ceiling.
bool ValidShadowMapResolution(double asked) {
    return asked == std::floor(asked) && (asked == 0.0 || (asked >= kShadowMapMin && asked <= kShadowMapMax));
}

// The "mobile" object. Required, so that a block lost from the file is noticed where the file is read; every key in it is optional, because a key
// present IS the override and a key absent leaves the desktop's answer.
bool ReadMobile(const Json::Value& root, MobileBlock& out, const std::string& path, std::string& error) {
    const Json::Value& node = root[kMobileKey];
    if (!node.IsObject()) {
        error = path + ": " + kMobileKey + " is an object, the phone profile's answers (" + kMinimumVulkanMinorKey + ", " + kShadowMapKey + ", " +
                kAssetWatchingKey + ")";
        return false;
    }
    for (const std::string& key : node.OrderedKeys()) {
        if (!key.empty() && key[0] == '_') continue; // a note (_about, _decision_pending): not read
        if (key != kMinimumVulkanMinorKey && key != kShadowMapKey && key != kAssetWatchingKey) {
            error = path + ": " + kMobileKey + "." + key + " is not a key of the phone profile (it has " + kMinimumVulkanMinorKey + ", " +
                    kShadowMapKey + " and " + kAssetWatchingKey +
                    "; a key for a decision not yet taken is refused until the code that reads it exists)";
            return false;
        }
    }
    MobileBlock read;
    if (node.Has(kMinimumVulkanMinorKey)) {
        const Json::Value& minor = node[kMinimumVulkanMinorKey];
        const double asked = minor.IsNumber() ? minor.AsNumber() : 0.0;
        if (!minor.IsNumber() || asked != std::floor(asked) || asked < kVulkanMinorMin || asked > kVulkanMinorMax) {
            error = path + ": " + kMobileKey + "." + kMinimumVulkanMinorKey + " is " + std::to_string(kVulkanMinorMin) + " or " +
                    std::to_string(kVulkanMinorMax) + " (the Vulkan 1.x minor a GPU must report)";
            return false;
        }
        read.minimumVulkanMinor = static_cast<std::uint32_t>(asked);
    }
    if (node.Has(kShadowMapKey)) {
        const Json::Value& size = node[kShadowMapKey];
        if (!size.IsNumber() || !ValidShadowMapResolution(size.AsNumber())) {
            error = path + ": " + kMobileKey + "." + kShadowMapKey + " is 0 (the engine's own sizes) or a whole number from " +
                    std::to_string(kShadowMapMin) + " to " + std::to_string(kShadowMapMax);
            return false;
        }
        read.shadowMapResolution = static_cast<std::uint32_t>(size.AsNumber());
    }
    if (node.Has(kAssetWatchingKey)) {
        if (!node[kAssetWatchingKey].IsBool()) {
            error = path + ": " + kMobileKey + "." + kAssetWatchingKey + " is true or false";
            return false;
        }
        read.assetWatching = node[kAssetWatchingKey].AsBool();
    }
    out = read;
    return true;
}

bool ReadPerRun(const Json::Value& root, const char* key, PerRun& out, const std::string& path, std::string& error) {
    const Json::Value& node = root[key];
    if (!node.IsObject() || !node["attended_run"].IsBool() || !node["other_runs"].IsBool()) {
        error = path + ": " + key + " is {\"attended_run\": true|false, \"other_runs\": true|false}";
        return false;
    }
    out.attended = node["attended_run"].AsBool();
    out.other = node["other_runs"].AsBool();
    return true;
}

} // namespace

bool Load(const std::string& path, Table& out, std::string& error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = path + ": cannot open";
        return false;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string text = buffer.str();
    Json::Value root;
    Json::Parser parser(text);
    if (!parser.Parse(root)) {
        error = path + ": " + parser.Error();
        return false;
    }
    Table read;
    const Json::Value& size = root["shadow_map_resolution"];
    if (!size.IsNumber()) {
        error = path + ": shadow_map_resolution is a number";
        return false;
    }
    const double asked = size.AsNumber();
    if (!ValidShadowMapResolution(asked)) {
        error = path + ": shadow_map_resolution is 0 (the engine's own sizes) or a whole number from " + std::to_string(kShadowMapMin) +
                " to " + std::to_string(kShadowMapMax);
        return false;
    }
    read.shadowMapResolution = static_cast<std::uint32_t>(asked);
    if (!root["sprites_only_scene_pipelines"].IsBool()) {
        error = path + ": sprites_only_scene_pipelines is true or false";
        return false;
    }
    read.spritesOnlyScenePipelines = root["sprites_only_scene_pipelines"].AsBool();
    if (!ReadPerRun(root, "asset_watching", read.assetWatching, path, error) ||
        !ReadPerRun(root, "sprite_report", read.spriteReport, path, error) || !ReadMobile(root, read.mobile, path, error)) {
        return false;
    }
    out = read;
    return true;
}

Resolved Resolve(const Table& table, bool attended, bool mobile, const Overrides& overrides) {
    // The desktop answer first, line for line what this function was before the phone profile: the profile only ever REPLACES a value, so a
    // mobile=false call cannot see it.
    std::uint32_t shadowMapResolution = table.shadowMapResolution;
    bool assetWatching = attended ? table.assetWatching.attended : table.assetWatching.other;
    Resolved out;
    if (mobile) {
        if (table.mobile.shadowMapResolution) shadowMapResolution = *table.mobile.shadowMapResolution;
        if (table.mobile.assetWatching) assetWatching = *table.mobile.assetWatching;
        out.minimumVulkanMinor = table.mobile.minimumVulkanMinor.value_or(0);
    }
    out.shadowMapResolution = overrides.shadowMapResolution.value_or(shadowMapResolution);
    out.spritesOnlyScenePipelines = overrides.spritesOnlyScenePipelines.value_or(table.spritesOnlyScenePipelines);
    out.assetWatching = overrides.assetWatching.value_or(assetWatching);
    out.spriteReport = overrides.spriteReport.value_or(attended ? table.spriteReport.attended : table.spriteReport.other);
    out.perfLogSeconds = overrides.perfLogSeconds;
    return out;
}

bool ParseSwitch(const std::string& text, bool& out) {
    if (text == "on") {
        out = true;
        return true;
    }
    if (text == "off") {
        out = false;
        return true;
    }
    return false;
}

bool ParseShadowMaps(const std::string& text, std::uint32_t& out) {
    if (text.empty() || text.size() > 6) return false;
    for (const char c : text) {
        if (c < '0' || c > '9') return false;
    }
    const unsigned long value = std::strtoul(text.c_str(), nullptr, 10);
    if (value != 0 && (value < kShadowMapMin || value > kShadowMapMax)) return false;
    out = static_cast<std::uint32_t>(value);
    return true;
}

bool ParseProfile(const std::string& text, bool& mobile) {
    if (text == "desktop") {
        mobile = false;
        return true;
    }
    if (text == "mobile") {
        mobile = true;
        return true;
    }
    return false;
}

bool ParsePerfLog(const std::string& text, float& out) {
    // A flag is exactly a plain decimal number: digits, then at most one '.' and more digits. strtod alone would also take white space, a sign,
    // a hexadecimal float ("0x10") and an exponent ("1e2"), none of which the flag's help text means (planning step 191).
    std::size_t at = 0;
    while (at < text.size() && text[at] >= '0' && text[at] <= '9') ++at;
    if (at == 0) return false;
    if (at < text.size() && text[at] == '.') {
        const std::size_t fraction = ++at;
        while (at < text.size() && text[at] >= '0' && text[at] <= '9') ++at;
        if (at == fraction) return false;
    }
    if (at != text.size()) return false;
    char* end = nullptr;
    const double value = std::strtod(text.c_str(), &end);
    if (end == nullptr || *end != '\0' || !std::isfinite(value) || !(value > 0.0) || value > static_cast<double>(kPerfLogMaxSeconds)) {
        return false;
    }
    out = static_cast<float>(value);
    return true;
}

std::string Describe(const Resolved& resolved) {
    std::ostringstream out;
    out << "shadow maps ";
    if (resolved.shadowMapResolution == 0) {
        out << "as the engine sizes them";
    } else {
        out << resolved.shadowMapResolution << " px";
    }
    out << ", sprites-only pipelines " << (resolved.spritesOnlyScenePipelines ? "on" : "off") << ", asset watching "
        << (resolved.assetWatching ? "on" : "off") << ", sprite report " << (resolved.spriteReport ? "on" : "off") << ", perf log ";
    if (resolved.perfLogSeconds > 0.0f) {
        out << "every " << resolved.perfLogSeconds << " s";
    } else {
        out << "off";
    }
    return out.str();
}

} // namespace MagicPortals::EngineTuning
