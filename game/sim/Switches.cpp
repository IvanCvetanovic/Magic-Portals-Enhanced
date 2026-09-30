#include "sim/Switches.hpp"

#include "core/Json.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>

namespace MagicPortals::Switches {

namespace {

namespace Json = Supersonic::Json;

// sounds.json's block and its two values.
constexpr const char* kRulesBlock = "switches";
constexpr const char* kVolumeDefault = "global_volume_default";
constexpr const char* kMusicDefault = "music_enabled_default";

// audio.json: the original's entity and value names (saveVolume #4-8,
// saveMusicState #7-11).
constexpr const char* kEntity = "audio";
constexpr const char* kGlobalVolume = "globalVolume";
constexpr const char* kMusicEnabled = "musicEnabled";
constexpr int kVersion = 1;

// manageSoundSwitch #11 SetGlobalVolume(1f), #23 SetGlobalVolume(0f): what the
// switch sets, not a tuning - GlobalSoundSwitch reads 0 as off and anything else
// as on.
constexpr double kVolumeOn = 1.0;
constexpr double kVolumeOff = 0.0;

bool ReadText(const std::string& path, std::string& text) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    std::ostringstream buffer;
    buffer << file.rdbuf();
    text = buffer.str();
    return true;
}

} // namespace

bool LoadDefaults(const std::string& path, Defaults& out, std::string& error) {
    std::string text;
    if (!ReadText(path, text)) {
        error = path + ": cannot open";
        return false;
    }
    Json::Parser parser(text);
    Json::Value root;
    if (!parser.Parse(root)) {
        error = path + ": " + parser.Error();
        return false;
    }
    const Json::Value& block = root[kRulesBlock];
    if (!block[kVolumeDefault].IsNumber() || block[kVolumeDefault].AsNumber(-1.0) < 0.0 ||
        !block[kMusicDefault].IsBool()) {
        error = path + ": switches has a " + kVolumeDefault + " of at least 0 and a boolean " + kMusicDefault;
        return false;
    }
    out.globalVolume = block[kVolumeDefault].AsNumber();
    out.musicEnabled = block[kMusicDefault].AsBool();
    return true;
}

bool Store::Open(const std::string& directory, const Defaults& defaults, std::string& error) {
    m_globalVolume = defaults.globalVolume;
    m_musicEnabled = defaults.musicEnabled;
    m_path.clear();
    m_fromFile = false;
    error.clear();

    if (directory.empty()) return true;
    m_path = (std::filesystem::path(directory) / kFileName).string();

    std::error_code ec;
    if (!std::filesystem::exists(m_path, ec) || ec) return true; // never toggled: the defaults

    std::string text;
    if (!ReadText(m_path, text)) {
        error = m_path + ": cannot open";
        return false;
    }
    Json::Parser parser(text);
    Json::Value root;
    if (!parser.Parse(root)) {
        error = m_path + ": " + parser.Error();
        return false;
    }
    // Value by value, as loadValue gives its default for a value the file lacks.
    const Json::Value& audio = root[kEntity];
    const Json::Value& volume = audio[kGlobalVolume];
    if (volume.IsNumber() && volume.AsNumber() >= 0.0) m_globalVolume = volume.AsNumber();
    if (audio[kMusicEnabled].IsBool()) m_musicEnabled = audio[kMusicEnabled].AsBool();
    m_fromFile = true;
    return true;
}

void Store::SetSound(bool on) { m_globalVolume = on ? kVolumeOn : kVolumeOff; }

void Store::SetMusic(bool on) { m_musicEnabled = on; }

bool Store::Save(std::string& error) const {
    error.clear();
    if (m_path.empty()) return true;

    const std::filesystem::path path(m_path);
    std::error_code ec;
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path(), ec);

    const std::filesystem::path temp = path.string() + ".tmp";
    {
        std::ofstream file(temp, std::ios::binary | std::ios::trunc);
        if (!file) {
            error = temp.string() + ": cannot open for writing";
            return false;
        }
        file << "{\n";
        file << "  \"version\": " << kVersion << ",\n";
        file << "  \"" << kEntity << "\": {\n";
        file << "    \"" << kGlobalVolume << "\": " << m_globalVolume << ",\n";
        file << "    \"" << kMusicEnabled << "\": " << (m_musicEnabled ? "true" : "false") << "\n";
        file << "  }\n";
        file << "}\n";
        if (!file.good()) {
            error = temp.string() + ": write failed";
            return false;
        }
    }

    ec.clear();
    std::filesystem::rename(temp, path, ec);
    if (ec) {
        // Some filesystems refuse a rename onto an existing file.
        ec.clear();
        std::filesystem::remove(path, ec);
        ec.clear();
        std::filesystem::rename(temp, path, ec);
    }
    if (ec) {
        error = m_path + ": " + ec.message();
        return false;
    }
    return true;
}

} // namespace MagicPortals::Switches
