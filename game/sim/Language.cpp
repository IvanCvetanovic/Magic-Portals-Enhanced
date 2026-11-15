#include "sim/Language.hpp"

#include "core/Json.hpp"
#include "sim/Files.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>

namespace MagicPortals::Language {

namespace Json = Supersonic::Json;

// Only `--fixed-step`, `--route` and `--visit-levels` make a run a developer's (Language.hpp says why, and that the capture helpers like `--tap` do not):
// keep this list and main's `--help` text for `--lang` the same.
Policy ResolvePolicy(const std::optional<std::string>& langFlag, bool fixedStep, bool route, bool visitLevels) {
    Policy policy;
    const bool developers = fixedStep || route || visitLevels;
    policy.picker = !developers;
    if (langFlag) {
        policy.forced = *langFlag;
    } else if (developers) {
        policy.forced = std::string(Strings::kLangCodes[Strings::IndexOf(Strings::kDefaultLang)]);
    }
    return policy;
}

namespace {

// ASCII only, by hand: the C library's tolower follows the program's locale, and a locale text is not a place for that to matter.
bool IsAsciiLetter(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
char FoldAscii(char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; }
bool IsAsciiSpace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
// What a locale text puts between its language and the rest: `pt_BR`, `pt-BR`, `pt.UTF-8`, `pt@euro`.
bool EndsTheLanguage(char c) { return c == '_' || c == '-' || c == '.' || c == '@'; }

bool SameLettersIgnoringCase(std::string_view text, std::string_view code) {
    if (text.size() != code.size()) return false;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (FoldAscii(text[i]) != code[i]) return false;
    }
    return true;
}

} // namespace

std::string CodeForLocale(std::string_view locale) {
    while (!locale.empty() && IsAsciiSpace(locale.front())) locale.remove_prefix(1);
    while (!locale.empty() && IsAsciiSpace(locale.back())) locale.remove_suffix(1);
    std::size_t end = 0;
    while (end < locale.size() && IsAsciiLetter(locale[end])) ++end;
    // Letters, and then the end of the text or a separator: `pt1` and `pt:en` are not a locale, and an empty language (`_BR`, `.UTF-8`) is none.
    if (end < locale.size() && !EndsTheLanguage(locale[end])) return {};
    const std::string_view subtag = locale.substr(0, end);
    for (const std::string_view code : Strings::kLangCodes) {
        if (SameLettersIgnoringCase(subtag, code)) return std::string(code);
    }
    return {};
}

void ApplySystemLocale(Policy& policy, bool attended, std::string_view locale) {
    policy.systemDefault.clear();
    if (!attended || policy.forced) return;
    policy.systemDefault = CodeForLocale(locale);
}

bool Store::Open(const std::string& directory, const Strings::LanguageTable& languages, std::string& error) {
    m_languages = languages;
    m_chosen.reset();
    m_path.clear();
    m_fromFile = false;
    m_reason.clear();
    m_written.clear();
    m_saves = 0;
    error.clear();

    if (directory.empty()) return true;
    m_path = (std::filesystem::path(directory) / kStoreFileName).string();

    std::error_code ec;
    if (!std::filesystem::exists(m_path, ec) || ec) return true; // never chosen: English

    // One exit for a file that cannot be used: English, the reason kept, the file untouched.
    const auto unusable = [this, &error](const std::string& why) {
        m_reason = m_path + ": " + why;
        error = m_reason;
        return false;
    };

    std::ifstream file(m_path, std::ios::binary);
    if (!file) return unusable("cannot open");
    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string text = buffer.str();
    Json::Parser parser(text);
    Json::Value root;
    if (!parser.Parse(root)) return unusable(parser.Error());
    if (!root.IsObject()) return unusable("the document is not an object");
    // A version this game does not know was written by another build, whose meaning of the rest is not ours to guess: English, the file untouched. A file
    // with no version at all is a hand-written one and reads (the version is how a LATER layout will be told apart, not a requirement of this one).
    if (root.Has(StoreKey::kVersion)) {
        const Json::Value& version = root[StoreKey::kVersion];
        if (!version.IsNumber() || version.AsNumber() != static_cast<double>(kStoreVersion)) {
            return unusable(std::string("'") + StoreKey::kVersion + "' is not " + std::to_string(kStoreVersion) +
                            ", the only version this game reads" + (version.IsNumber() ? " (the file says " + std::to_string(version.AsNumber()) + ")" : ""));
        }
    }
    const Json::Value& code = root[StoreKey::kLanguage];
    if (!code.IsString()) return unusable("there is no string 'language'");
    const std::optional<Strings::Lang> lang = m_languages.LangOfCode(code.AsString());
    if (!lang) return unusable("'" + code.AsString() + "' is not one of the languages");
    m_chosen = lang;
    m_fromFile = true;
    // What was read is what a Save of it would write: a player who opens and quits does not rewrite the file.
    m_written = Text();
    return true;
}

std::string Store::Text() const {
    if (!m_chosen) return {};
    return std::string("{\"") + StoreKey::kVersion + "\":" + std::to_string(kStoreVersion) + ",\"" + StoreKey::kLanguage + "\":\"" +
           m_languages.CodeOf(*m_chosen) + "\"}";
}

bool Store::Save(std::string& error) {
    error.clear();
    if (m_path.empty() || !m_chosen) return true;
    const std::string text = Text();
    if (text == m_written) return true;

    // The write every store shares (sim/Files, planning step 189): a temporary of this call's own name, flushed and checked after the close,
    // renamed over the file in one step, never removing the live file, the temporary gone on every failure. A failure is not remembered as a
    // write, so the next Save tries again with the bytes it still holds.
    if (!Files::WriteAtomic(m_path, text, Files::WriteOptions{}, error)) return false;
    m_written = text;
    ++m_saves;
    return true;
}

} // namespace MagicPortals::Language
