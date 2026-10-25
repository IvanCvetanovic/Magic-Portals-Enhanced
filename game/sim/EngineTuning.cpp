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
constexpr const char* kSpritesOnlyKey = "sprites_only_scene_pipelines";
constexpr const char* kSpriteReportKey = "sprite_report";
constexpr const char* kSceneSamplesKey = "scene_samples";
constexpr const char* kDynamicResolutionKey = "dynamic_resolution";
// The keys of the dynamic-resolution object, which is a vocabulary of its own: all seven are required when the object is present.
constexpr const char* kDrEnabledKey = "enabled";
constexpr const char* kDrStartMaxPixelsKey = "start_max_pixels";
constexpr const char* kDrMinScaleKey = "min_scale";
constexpr const char* kDrTargetFpsKey = "target_fps";
constexpr const char* kDrRelaxedRiseKey = "relaxed_rise";
constexpr const char* kDrMinTargetHeightKey = "min_target_height";
constexpr const char* kDrEmergencyBelowFpsKey = "emergency_below_fps";

bool IsWholeNumber(const Json::Value& value) { return value.IsNumber() && value.AsNumber() == std::floor(value.AsNumber()); }

bool SceneSamplesAllowed(double asked) {
    for (const std::uint32_t allowed : kSceneSamplesAllowed) {
        if (asked == static_cast<double>(allowed)) return true;
    }
    return false;
}

// The "dynamic_resolution" object of the phone block: all seven keys, each of its type and inside the engine's own range (DynamicResolution::Mend clamps what
// is outside it, which a table should say instead of having mended). A note (an underscore key) is skipped; any other key is refused by name.
bool ReadDynamicResolution(const Json::Value& node, DynamicResolutionBlock& out, const std::string& path, std::string& error) {
    const std::string where = path + ": " + kMobileKey + "." + kDynamicResolutionKey;
    if (!node.IsObject()) {
        error = where + " is an object: {" + kDrEnabledKey + ", " + kDrStartMaxPixelsKey + ", " + kDrMinScaleKey + ", " + kDrTargetFpsKey + ", " +
                kDrRelaxedRiseKey + ", " + kDrMinTargetHeightKey + ", " + kDrEmergencyBelowFpsKey + "}";
        return false;
    }
    for (const std::string& key : node.OrderedKeys()) {
        if (!key.empty() && key[0] == '_') continue;
        if (key != kDrEnabledKey && key != kDrStartMaxPixelsKey && key != kDrMinScaleKey && key != kDrTargetFpsKey && key != kDrRelaxedRiseKey &&
            key != kDrMinTargetHeightKey && key != kDrEmergencyBelowFpsKey) {
            error = where + "." + key + " is not a key of the dynamic-resolution block (it has " + kDrEnabledKey + ", " + kDrStartMaxPixelsKey + ", " +
                    kDrMinScaleKey + ", " + kDrTargetFpsKey + ", " + kDrRelaxedRiseKey + ", " + kDrMinTargetHeightKey + " and " + kDrEmergencyBelowFpsKey + ")";
            return false;
        }
    }
    const auto need = [&](const char* key) -> const Json::Value* {
        if (!node.Has(key)) {
            error = where + "." + key + " is missing (all seven keys are required, so that a half-written block fails where it is read)";
            return nullptr;
        }
        return &node[key];
    };
    DynamicResolutionBlock read;
    const Json::Value* value = nullptr;
    if ((value = need(kDrEnabledKey)) == nullptr) return false;
    if (!value->IsBool()) {
        error = where + "." + kDrEnabledKey + " is true or false";
        return false;
    }
    read.enabled = value->AsBool();
    if ((value = need(kDrStartMaxPixelsKey)) == nullptr) return false;
    if (!IsWholeNumber(*value) || value->AsNumber() < 0.0 || value->AsNumber() > static_cast<double>(kDynamicMostPixels)) {
        error = where + "." + kDrStartMaxPixelsKey + " is 0 (no budget) or a whole number of pixels up to " + std::to_string(kDynamicMostPixels);
        return false;
    }
    read.startMaxPixels = static_cast<std::uint32_t>(value->AsNumber());
    if ((value = need(kDrMinScaleKey)) == nullptr) return false;
    if (!value->IsNumber() || !(value->AsNumber() >= kDynamicMinScaleMin) || !(value->AsNumber() <= kDynamicMinScaleMax)) {
        error = where + "." + kDrMinScaleKey + " is a number from 0.05 to 1 (the engine's own range for the floor of the scale)";
        return false;
    }
    read.minScale = static_cast<float>(value->AsNumber());
    if ((value = need(kDrTargetFpsKey)) == nullptr) return false;
    if (!value->IsNumber() || !(value->AsNumber() >= kDynamicTargetFpsMin) || !(value->AsNumber() <= kDynamicTargetFpsMax)) {
        error = where + "." + kDrTargetFpsKey + " is a number of frames a second from 1 to 240";
        return false;
    }
    read.targetFps = static_cast<float>(value->AsNumber());
    if ((value = need(kDrRelaxedRiseKey)) == nullptr) return false;
    if (!value->IsBool()) {
        error = where + "." + kDrRelaxedRiseKey + " is true or false";
        return false;
    }
    read.relaxedRise = value->AsBool();
    if ((value = need(kDrMinTargetHeightKey)) == nullptr) return false;
    if (!IsWholeNumber(*value) || value->AsNumber() < 0.0 || value->AsNumber() > static_cast<double>(kDynamicMostTargetHeight)) {
        error = where + "." + kDrMinTargetHeightKey + " is 0 (no pixel floor) or a whole number of lines up to " + std::to_string(kDynamicMostTargetHeight);
        return false;
    }
    read.minTargetHeight = static_cast<std::uint32_t>(value->AsNumber());
    if ((value = need(kDrEmergencyBelowFpsKey)) == nullptr) return false;
    // 0 is "no emergency floor" (DynamicResolution::Mend); above the target it would trigger on every frame, which is a typo and not a setting.
    if (!value->IsNumber() || !(value->AsNumber() >= 0.0) || !(value->AsNumber() < read.targetFps)) {
        error = where + "." + kDrEmergencyBelowFpsKey + " is 0 (none) or a number of frames a second below target_fps";
        return false;
    }
    read.emergencyBelowFps = static_cast<float>(value->AsNumber());
    out = read;
    return true;
}

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
                kAssetWatchingKey + ", " + kSpritesOnlyKey + ", " + kSpriteReportKey + ", " + kSceneSamplesKey + ", " + kDynamicResolutionKey + ")";
        return false;
    }
    for (const std::string& key : node.OrderedKeys()) {
        if (!key.empty() && key[0] == '_') continue; // a note (_about, _decision_pending): not read
        if (key != kMinimumVulkanMinorKey && key != kShadowMapKey && key != kAssetWatchingKey && key != kSpritesOnlyKey && key != kSpriteReportKey &&
            key != kSceneSamplesKey && key != kDynamicResolutionKey) {
            error = path + ": " + kMobileKey + "." + key + " is not a key of the phone profile (it has " + kMinimumVulkanMinorKey + ", " +
                    kShadowMapKey + ", " + kAssetWatchingKey + ", " + kSpritesOnlyKey + ", " + kSpriteReportKey + ", " + kSceneSamplesKey + " and " +
                    kDynamicResolutionKey + "; a key for a decision not yet taken is refused until the code that reads it exists)";
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
    if (node.Has(kSpritesOnlyKey)) {
        if (!node[kSpritesOnlyKey].IsBool()) {
            error = path + ": " + kMobileKey + "." + kSpritesOnlyKey + " is true or false";
            return false;
        }
        read.spritesOnlyScenePipelines = node[kSpritesOnlyKey].AsBool();
    }
    if (node.Has(kSpriteReportKey)) {
        if (!node[kSpriteReportKey].IsBool()) {
            error = path + ": " + kMobileKey + "." + kSpriteReportKey + " is true or false (for both kinds of run; the top-level key is the one with two)";
            return false;
        }
        read.spriteReport = node[kSpriteReportKey].AsBool();
    }
    if (node.Has(kSceneSamplesKey)) {
        const Json::Value& samples = node[kSceneSamplesKey];
        if (!samples.IsNumber() || !SceneSamplesAllowed(samples.AsNumber())) {
            error = path + ": " + kMobileKey + "." + kSceneSamplesKey + " is 1, 2 or 4 (the scene target's sample cap; leave the key out for the engine's own)";
            return false;
        }
        read.sceneSamples = static_cast<std::uint32_t>(samples.AsNumber());
    }
    if (node.Has(kDynamicResolutionKey)) {
        DynamicResolutionBlock dynamic;
        if (!ReadDynamicResolution(node[kDynamicResolutionKey], dynamic, path, error)) return false;
        read.dynamicResolution = dynamic;
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
    bool spritesOnly = table.spritesOnlyScenePipelines;
    bool spriteReport = attended ? table.spriteReport.attended : table.spriteReport.other;
    std::uint32_t sceneSamples = 0;
    DynamicResolutionBlock dynamicResolution; // disabled: the engine as it was
    Resolved out;
    if (mobile) {
        if (table.mobile.shadowMapResolution) shadowMapResolution = *table.mobile.shadowMapResolution;
        if (table.mobile.assetWatching) assetWatching = *table.mobile.assetWatching;
        if (table.mobile.spritesOnlyScenePipelines) spritesOnly = *table.mobile.spritesOnlyScenePipelines;
        if (table.mobile.spriteReport) spriteReport = *table.mobile.spriteReport;
        if (table.mobile.sceneSamples) sceneSamples = *table.mobile.sceneSamples;
        if (table.mobile.dynamicResolution) dynamicResolution = *table.mobile.dynamicResolution;
        out.minimumVulkanMinor = table.mobile.minimumVulkanMinor.value_or(0);
    }
    // --dynamic-res over any profile: the mobile block's numbers are the only numbers there are (the sibling's, E41 and E47), so a desktop run asked for
    // dynamic resolution gets them too; the flag alone decides whether it is on.
    if (overrides.dynamicResolution) {
        if (!mobile && table.mobile.dynamicResolution) dynamicResolution = *table.mobile.dynamicResolution;
        dynamicResolution.enabled = *overrides.dynamicResolution;
    }
    out.shadowMapResolution = overrides.shadowMapResolution.value_or(shadowMapResolution);
    out.spritesOnlyScenePipelines = overrides.spritesOnlyScenePipelines.value_or(spritesOnly);
    out.assetWatching = overrides.assetWatching.value_or(assetWatching);
    out.spriteReport = overrides.spriteReport.value_or(spriteReport);
    out.perfLogSeconds = overrides.perfLogSeconds;
    out.sceneSamples = overrides.sceneSamples.value_or(sceneSamples);
    out.dynamicResolution = dynamicResolution;
    out.perfOverlay = overrides.perfOverlay.value_or(false);
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

bool ParseSceneSamples(const std::string& text, std::uint32_t& out) {
    // Exactly the digit, as the flag's help text says: no sign, no space, no "04", no "4.0".
    if (text.size() != 1) return false;
    for (const std::uint32_t allowed : kSceneSamplesAllowed) {
        if (text[0] == static_cast<char>('0' + allowed)) {
            out = allowed;
            return true;
        }
    }
    return false;
}

void ApplyPerfDiag(Overrides& overrides) {
    if (!overrides.perfOverlay) overrides.perfOverlay = true;
    if (!(overrides.perfLogSeconds > 0.0f)) overrides.perfLogSeconds = kPerfDiagLogSeconds;
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
    if (resolved.perfOverlay) out << ", perf overlay on";
    if (resolved.dynamicResolution.enabled) {
        const DynamicResolutionBlock& dr = resolved.dynamicResolution;
        out << ", dynamic resolution on (start budget " << dr.startMaxPixels << " px, floor " << dr.minScale << ", target " << dr.targetFps << " fps, relaxed rise "
            << (dr.relaxedRise ? "on" : "off") << ", floor " << dr.minTargetHeight << " lines, emergency below " << dr.emergencyBelowFps << " fps)";
    } else {
        out << ", dynamic resolution off";
    }
    return out.str();
}

} // namespace MagicPortals::EngineTuning
