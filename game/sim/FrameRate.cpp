#include "sim/FrameRate.hpp"

#include "core/Json.hpp"
#include "core/WindowControl.hpp"
#include "sim/Files.hpp"
#include "sim/UiLayer.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <utility>

namespace MagicPortals::FrameRate {

namespace {

namespace Json = Supersonic::Json;

constexpr const char* kBlock = "frame_rate";
constexpr const char* kDefault = "default";
constexpr const char* kDesktopChoices = "desktop_choices";
constexpr const char* kAndroidChoices = "android_choices";
constexpr const char* kIosChoices = "ios_choices";
constexpr const char* kTolerance = "refresh_tolerance_hz";
constexpr const char* kLowRequest = "android_low_request_hz";
constexpr const char* kSmooth = "smooth_when_limited";
constexpr const char* kAutoWord = "auto";
constexpr const char* kMaxWord = "max";
constexpr const char* kVersion = "version";
constexpr const char* kStored = "frame_rate";
constexpr int kStoreVersion = 1;
// A display that is not this far from its own rate counts as it; a larger allowance would call 60 on a 75 Hz screen the same.
constexpr double kHighestToleranceHz = 10.0;

// A whole number of frames a second in range.
bool IsFps(const Json::Value& value, int& out) {
    if (!value.IsNumber()) return false;
    const double number = value.AsNumber();
    if (!std::isfinite(number) || number != std::floor(number) || number < kLowestFps || number > kHighestFps) return false;
    out = static_cast<int>(number);
    return true;
}

// "auto", "max" or a number.
bool ReadChoice(const Json::Value& value, Choice& out) {
    if (value.IsString()) {
        if (value.AsString() == kAutoWord) {
            out = Choice::Auto();
            return true;
        }
        if (value.AsString() == kMaxWord) {
            out = Choice::Max();
            return true;
        }
        return false;
    }
    int fps = 0;
    if (!IsFps(value, fps)) return false;
    out = Choice::Of(fps);
    return true;
}

// The value as the file and the data write it.
std::string Written(const Choice& choice) {
    return choice.kind == Choice::Kind::Fps ? std::to_string(choice.fps) : "\"" + NameOf(choice) + "\"";
}

} // namespace

std::string NameOf(const Choice& choice) {
    switch (choice.kind) {
    case Choice::Kind::Auto:
        break;
    case Choice::Kind::Max:
        return kMaxWord;
    case Choice::Kind::Fps:
        return std::to_string(choice.fps);
    }
    return kAutoWord;
}

bool LoadRules(const std::string& path, Rules& out, std::string& error) {
    Json::Value root;
    if (!UiLayer::Read::File(path, root, error)) return false;
    const Json::Value& block = root[kBlock];
    if (!block.IsObject()) {
        error = path + ": " + kBlock + " is missing or not an object";
        return false;
    }
    const auto fail = [&error, &path](const char* key, const std::string& why) {
        error = path + ": " + kBlock + "." + key + " " + why;
        return false;
    };
    const std::string range = std::to_string(kLowestFps) + " to " + std::to_string(kHighestFps);

    Rules read;
    if (!ReadChoice(block[kDefault], read.fallback)) return fail(kDefault, "is missing, or not \"auto\", \"max\" or a whole number from " + range);

    const auto list = [&](const char* key, bool maxAllowed, std::vector<Choice>& into) {
        const Json::Value& value = block[key];
        if (!value.IsArray()) return fail(key, "is missing or not a list");
        int last = 0;
        bool sawMax = false;
        for (const Json::Value& item : value.AsArray()) {
            Choice choice;
            if (!ReadChoice(item, choice) || choice.kind == Choice::Kind::Auto || (choice.kind == Choice::Kind::Max && !maxAllowed)) {
                return fail(key, std::string("holds something that is not a whole number from ") + range + (maxAllowed ? " or \"max\"" : ""));
            }
            if (sawMax || (choice.kind == Choice::Kind::Fps && choice.fps <= last)) return fail(key, "does not go upwards (\"max\" last)");
            sawMax = choice.kind == Choice::Kind::Max;
            if (!sawMax) last = choice.fps;
            into.push_back(choice);
        }
        return true;
    };
    if (!list(kDesktopChoices, false, read.desktop) || !list(kAndroidChoices, true, read.android) || !list(kIosChoices, false, read.ios)) return false;

    const Json::Value& tolerance = block[kTolerance];
    if (!tolerance.IsNumber() || !std::isfinite(tolerance.AsNumber()) || tolerance.AsNumber() < 0.0 || tolerance.AsNumber() > kHighestToleranceHz) {
        return fail(kTolerance, "is missing or not a number from 0 to 10");
    }
    read.refreshToleranceHz = tolerance.AsNumber();
    if (!IsFps(block[kLowRequest], read.androidLowRequestHz)) return fail(kLowRequest, "is missing or not a whole number from " + range);
    if (!block[kSmooth].IsBool()) return fail(kSmooth, "is missing or not true or false");
    read.smoothWhenLimited = block[kSmooth].AsBool();
    out = std::move(read);
    return true;
}

std::vector<Choice> Offered(const Rules& rules, Platform platform, int refreshHz) {
    switch (platform) {
    case Platform::Android:
        return rules.android;
    case Platform::Ios:
        return rules.ios;
    case Platform::Desktop:
        break;
    }
    std::vector<Choice> offered;
    for (const Choice& choice : rules.desktop) {
        if (refreshHz > 0 && choice.kind == Choice::Kind::Fps && choice.fps > refreshHz + rules.refreshToleranceHz) continue;
        offered.push_back(choice);
    }
    return offered;
}

Choice Next(const Choice& current, const std::vector<Choice>& offered) {
    if (current.kind == Choice::Kind::Max) return Choice::Auto();
    for (const Choice& choice : offered) {
        // "max" stands above every number.
        if (current.kind == Choice::Kind::Auto || choice.kind == Choice::Kind::Max || choice.fps > current.fps) return choice;
    }
    return Choice::Auto();
}

Request RequestFor(const Rules& rules, Platform platform, const Choice& choice, int refreshHz, bool presentPaced) {
    Request request;
    switch (choice.kind) {
    case Choice::Kind::Auto:
        return request;
    case Choice::Kind::Max:
        // No cap anywhere; only Android has a display to ask for its highest rate.
        if (platform == Platform::Android) request.displayHz = Supersonic::WindowControl::kHighestRefreshRate;
        return request;
    case Choice::Kind::Fps:
        break;
    }
    request.cap = choice.fps;
    if (platform == Platform::Android) {
        request.displayHz = static_cast<std::uint32_t>(std::max(choice.fps, rules.androidLowRequestHz));
    } else if (platform == Platform::Desktop && presentPaced && refreshHz > 0 && choice.fps >= refreshHz - rules.refreshToleranceHz) {
        // The display paces the loop at this rate or under it already.
        request.cap = 0;
    }
    return request;
}

bool DoesSomething(const Rules& rules, Platform platform, const Choice& current, int refreshHz, bool presentPaced) {
    const Request nothing;
    if (RequestFor(rules, platform, current, refreshHz, presentPaced) != nothing) return true;
    for (const Choice& choice : Offered(rules, platform, refreshHz)) {
        if (RequestFor(rules, platform, choice, refreshHz, presentPaced) != nothing) return true;
    }
    return false;
}

bool Store::Open(const std::string& directory, std::string& error) {
    m_path.clear();
    m_written.clear();
    m_chosen.reset();
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
    if (root.Has(kVersion) && (!root[kVersion].IsNumber() || root[kVersion].AsNumber() != kStoreVersion)) {
        error = m_path + ": it is not of version " + std::to_string(kStoreVersion);
        return false;
    }
    Choice choice;
    if (!ReadChoice(root[kStored], choice)) {
        error = m_path + ": " + kStored + " is missing, or not \"auto\", \"max\" or a whole number from " + std::to_string(kLowestFps) + " to " +
                std::to_string(kHighestFps);
        return false;
    }
    m_chosen = choice;
    return true;
}

std::string Store::Text() const {
    if (!m_chosen) return {};
    return std::string("{\"") + kVersion + "\":" + std::to_string(kStoreVersion) + ",\"" + kStored + "\":" + Written(*m_chosen) + "}";
}

bool Store::Save(std::string& error) {
    error.clear();
    if (m_path.empty() || !m_chosen) return true;
    const std::string text = Text();
    if (text == m_written) return true;
    if (!Files::WriteAtomic(m_path, text, Files::WriteOptions{}, error)) return false;
    m_written = text;
    return true;
}

} // namespace MagicPortals::FrameRate
