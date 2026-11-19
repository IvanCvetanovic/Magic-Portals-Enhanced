#pragma once

// Where a suite finds the data, and a scratch copy of the port's data with one clock key set.
//
//   DataDirs::DataDir()      Enhanced's tables (data/): the environment variable MP_DATA_DIR if it is set, else the directory this
//                            build was configured with (MAGICPORTALS_DATA_DIR).
//   DataDirs::PortDataDir()  the port's own tables (game/data/): MP_PORT_DATA_DIR if set, else MAGICPORTALS_PORT_DATA_DIR.
//   DataDirs::PortDataWithClock(originalFrameHz)
//                            a copy of the port's data in a scratch directory whose clock.json says `original_frame_hz` is that,
//                            for a case that has to start the layer, or read the clock, at a pace the shipped file does not carry.
//                            The returned object owns the directory and removes it when it goes, so a run leaves none in the
//                            temporary directory; Ok() is false (and nothing is left behind) when the copy could not be made.
//
// The two variables are for a run that reads another data tree than the build's own (a mutation test, or a pace tried on a copy of
// the data); with neither set the answers are the compiled-in directories, which is what every suite has always read. Every
// function is inline and in this namespace, so a suite that names a thing of its own the same way never meets these.

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>
#include <string>
#include <system_error>
#include <utility>

namespace DataDirs {

// An environment variable's text, or empty. _dupenv_s on Windows: std::getenv is C4996 there, and the build is warning-free.
inline std::string EnvText(const char* name) {
#ifdef _WIN32
    char* value = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&value, &length, name) != 0 || value == nullptr) return {};
    std::string text(value);
    std::free(value);
    return text;
#else
    const char* value = std::getenv(name);
    return value != nullptr ? std::string(value) : std::string();
#endif
}

// The two directories a suite reads its tables from: the build's own, unless the environment names others. MP_DATA_DIR and MP_PORT_DATA_DIR
// are for the replay rig (a run against another data tree) and have to be set for a WHOLE run: a suite that still reads the compiled-in
// directory (test_mp_clock does) would otherwise read other tables than one that uses these, and a stale exported variable does the same.
inline std::string DataDir() {
    const std::string fromEnvironment = EnvText("MP_DATA_DIR");
    return !fromEnvironment.empty() ? fromEnvironment : std::string(MAGICPORTALS_DATA_DIR);
}

inline std::string PortDataDir() {
    const std::string fromEnvironment = EnvText("MP_PORT_DATA_DIR");
    return !fromEnvironment.empty() ? fromEnvironment : std::string(MAGICPORTALS_PORT_DATA_DIR);
}

// A scratch directory this header made, removed when the object goes (or is moved from).
class ScratchData {
public:
    ScratchData() = default;
    explicit ScratchData(std::filesystem::path directory) : m_directory(std::move(directory)) {}
    ScratchData(const ScratchData&) = delete;
    ScratchData& operator=(const ScratchData&) = delete;
    ScratchData(ScratchData&& other) noexcept : m_directory(std::move(other.m_directory)) { other.m_directory.clear(); }
    ScratchData& operator=(ScratchData&& other) noexcept {
        if (this != &other) {
            discard();
            m_directory = std::move(other.m_directory);
            other.m_directory.clear();
        }
        return *this;
    }
    ~ScratchData() { discard(); }

    bool Ok() const { return !m_directory.empty(); }
    // The directory, for a layer's Paths::portData; empty when the copy was not made.
    std::string Path() const { return m_directory.string(); }

private:
    void discard() {
        if (m_directory.empty()) return;
        std::error_code ec;
        std::filesystem::remove_all(m_directory, ec);
        m_directory.clear();
    }

    std::filesystem::path m_directory;
};

// The clock.json text with the number of `original_frame_hz` set to `hz`: rewritten in place when the key is there, added after the opening
// brace when it is not. Empty when the text has no opening brace at all (it is not a JSON object, so there is nothing to set the key in).
inline std::string WithOriginalFrameHz(const std::string& text, int hz) {
    const std::string key = "\"original_frame_hz\"";
    const std::string number = std::to_string(hz);
    const std::size_t at = text.find(key);
    if (at == std::string::npos) {
        const std::size_t brace = text.find('{');
        if (brace == std::string::npos) return {};
        std::string added = text;
        added.insert(brace + 1, "\n  " + key + ": " + number + ",");
        return added;
    }
    const std::size_t colon = text.find(':', at + key.size());
    if (colon == std::string::npos) return {};
    std::size_t first = colon + 1;
    while (first < text.size() && (text[first] == ' ' || text[first] == '\t')) ++first;
    std::size_t last = first;
    while (last < text.size() && text[last] != ',' && text[last] != '}' && text[last] != ' ' && text[last] != '\t' &&
           text[last] != '\r' && text[last] != '\n') {
        ++last;
    }
    if (last == first) return {};
    std::string edited = text;
    edited.replace(first, last - first, number);
    return edited;
}

// A copy of the port's data directory (`from`, by default PortDataDir()) with clock.json's original_frame_hz set to `originalFrameHz`.
// The whole tree is copied, subfolders included (the fonts are a folder of their own). The directory's name carries a random token and a
// counter, so that two suites that run at once (ctest runs one process a suite, two at a time) and two cases of one suite never share
// one. Not OK, with the half-made copy removed, when the source cannot be read or has no clock.json.
inline ScratchData PortDataWithClock(int originalFrameHz, const std::string& from = PortDataDir()) {
    namespace fs = std::filesystem;
    static std::atomic<unsigned> made{0};
    std::error_code ec;
    const fs::path base = fs::temp_directory_path(ec);
    if (ec) return ScratchData();
    std::random_device device;
    const fs::path to = base / ("supersonic-test-mp-clock-" + std::to_string(originalFrameHz) + "hz-" + std::to_string(device()) + "-" +
                                std::to_string(++made));
    fs::remove_all(to, ec);
    fs::create_directories(to, ec);
    if (ec) return ScratchData();
    ScratchData scratch(to);
    fs::copy(fs::path(from), to, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
    if (ec) return ScratchData();
    const fs::path clockFile = to / "clock.json";
    std::string text;
    {
        std::ifstream in(clockFile, std::ios::binary);
        if (!in) return ScratchData();
        text.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }
    const std::string edited = WithOriginalFrameHz(text, originalFrameHz);
    if (edited.empty()) return ScratchData();
    std::ofstream out(clockFile, std::ios::binary | std::ios::trunc);
    out << edited;
    out.close();
    if (!out) return ScratchData();
    return scratch;
}

} // namespace DataDirs
