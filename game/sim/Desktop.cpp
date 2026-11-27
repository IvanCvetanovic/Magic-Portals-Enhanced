#include "sim/Desktop.hpp"

#include "core/Json.hpp"
#include "sim/Files.hpp"
#include "sim/UiLayer.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <utility>

namespace MagicPortals::Desktop {

namespace {

namespace Json = Supersonic::Json;

constexpr const char* kBlock = "desktop";
constexpr const char* kFullscreenDefault = "fullscreen_default";
constexpr const char* kWalkPadsDefault = "walk_pads_default";
constexpr const char* kWalkHint = "walk_hint";
constexpr const char* kVersion = "version";
constexpr const char* kFullscreen = "fullscreen";
constexpr const char* kWalkPads = "walk_pads";

} // namespace

bool LoadRules(const std::string& path, Rules& out, std::string& error) {
    Json::Value root;
    if (!UiLayer::Read::File(path, root, error)) return false;
    const Json::Value& block = root[kBlock];
    if (!block.IsObject()) {
        error = path + ": " + kBlock + " is missing or not an object";
        return false;
    }
    Rules read;
    const auto flag = [&](const char* key, bool& into) {
        if (!block[key].IsBool()) {
            error = path + ": " + kBlock + "." + key + " is missing or not true or false";
            return false;
        }
        into = block[key].AsBool();
        return true;
    };
    if (!flag(kFullscreenDefault, read.fullscreenDefault) || !flag(kWalkPadsDefault, read.walkPadsDefault)) return false;
    const Json::Value& hint = block[kWalkHint];
    const std::string where = std::string(kBlock) + "." + kWalkHint;
    if (!hint.IsObject()) {
        error = path + ": " + where + " is missing or not an object";
        return false;
    }
    Rules::WalkHint& words = read.walkHint;
    std::string why;
    if (!UiLayer::Read::Text(hint, "words", words.words, why, where) || !UiLayer::Read::Text(hint, "font", words.font, why, where) ||
        !UiLayer::Read::Positive(hint, "units_per_font_px", words.unitsPerFontPx, why, where) ||
        !UiLayer::Read::Fraction(hint, "at_screen", words.atScreen, why, where) ||
        !UiLayer::Read::Positive(hint, "word_max_width_units", words.maxWidthUnits, why, where) ||
        !UiLayer::Read::Byte(hint, "alpha_byte", words.alphaByte, why, where)) {
        error = path + ": " + why;
        return false;
    }
    out = std::move(read);
    return true;
}

bool Store::Open(const std::string& directory, std::string& error) {
    m_path.clear();
    m_written.clear();
    m_fullscreen.reset();
    m_walkPads.reset();
    error.clear();

    if (directory.empty()) return true;
    m_path = (std::filesystem::path(directory) / kStoreFile).string();

    std::error_code ec;
    if (!std::filesystem::exists(m_path, ec) || ec) return true;

    std::ifstream file(m_path, std::ios::binary);
    if (!file) {
        error = m_path + ": cannot open";
        return false;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string text = buffer.str();
    Json::Parser parser(text);
    Json::Value root;
    if (!parser.Parse(root)) {
        error = m_path + ": " + parser.Error();
        return false;
    }
    if (!root.IsObject()) {
        error = m_path + ": the document is not an object";
        return false;
    }
    if (root[kFullscreen].IsBool()) m_fullscreen = root[kFullscreen].AsBool();
    if (root[kWalkPads].IsBool()) m_walkPads = root[kWalkPads].AsBool();
    return true;
}

std::string Store::Text() const {
    std::string text = std::string("{\"") + kVersion + "\":1";
    const auto member = [&text](const char* key, const std::optional<bool>& value) {
        if (value) text += std::string(",\"") + key + "\":" + (*value ? "true" : "false");
    };
    member(kFullscreen, m_fullscreen);
    member(kWalkPads, m_walkPads);
    return text + "}";
}

bool Store::Save(std::string& error) {
    error.clear();
    if (m_path.empty()) return true;
    const std::string text = Text();
    if (text == m_written) return true;
    if (!Files::WriteAtomic(m_path, text, Files::WriteOptions{}, error)) return false;
    m_written = text;
    return true;
}

} // namespace MagicPortals::Desktop
