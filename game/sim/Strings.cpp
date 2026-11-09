#include "sim/Strings.hpp"

#include "core/Json.hpp"
#include "sim/ArabicShaping.hpp"
#include "sim/Utf8.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

namespace MagicPortals::Strings {

namespace {

namespace Json = Supersonic::Json;

// languages.json's members.
namespace LanguageKey {
inline constexpr const char* kLanguages = "languages";
inline constexpr const char* kCode = "code";
inline constexpr const char* kNameNative = "name_native";
inline constexpr const char* kRtl = "rtl";
inline constexpr const char* kFontSet = "font_set";
inline constexpr const char* kFonts = "fonts";
} // namespace LanguageKey

// The `fonts` block's members.
namespace FontsKey {
inline constexpr const char* kFolder = "folder";
inline constexpr const char* kFaces = "faces";
inline constexpr const char* kScripts = "scripts";
inline constexpr const char* kSlots = "slots";
inline constexpr const char* kPickerSlot = "picker_slot";
inline constexpr const char* kId = "id";
inline constexpr const char* kFile = "file";
inline constexpr const char* kReference = "reference";
inline constexpr const char* kReferenceHeight = "reference_height";
inline constexpr const char* kRanges = "ranges";
inline constexpr const char* kOriginalFont = "original_font";
inline constexpr const char* kLineHeight = "line_height";
inline constexpr const char* kBase = "base";
inline constexpr const char* kCapHeight = "cap_height";
inline constexpr const char* kPadding = "padding";
inline constexpr const char* kSpacing = "spacing";
inline constexpr const char* kPageSize = "page_size";
inline constexpr const char* kShadow = "shadow";
inline constexpr const char* kEmbolden = "embolden";
inline constexpr const char* kTextFace = "text_face";
inline constexpr const char* kScriptFaces = "script_faces";
inline constexpr const char* kDx = "dx";
inline constexpr const char* kDy = "dy";
inline constexpr const char* kSigma = "sigma";
inline constexpr const char* kAlpha = "alpha";
} // namespace FontsKey

// The bounds the fonts block is held to: a value outside one is a typo, not a design.
constexpr int kMaxPadding = 64;
constexpr int kMinPageSize = 128;
constexpr int kMaxPageSize = 512; // the original's own page size, and what every phone's texture limit allows
constexpr int kMaxLineHeight = 512;
constexpr double kMaxShadowOffset = 16.0;
constexpr double kMaxShadowSigma = 16.0;
constexpr double kMaxReferenceHeight = 4.0;

// The font_set names.
namespace FontSetName {
inline constexpr const char* kOriginal = "original";
inline constexpr const char* kGenerated = "generated";
} // namespace FontSetName

// A language file's members.
namespace FileKey {
inline constexpr const char* kStrings = "strings";
}

constexpr char kNotePrefix = '_';

bool IsNote(const std::string& key) {
    return !key.empty() && key.front() == kNotePrefix;
}

bool ReadJson(const std::string& path, Json::Value& root, std::string& error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = path + ": cannot open";
        return false;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string text = buffer.str();
    Json::Parser parser(text);
    if (!parser.Parse(root)) {
        error = path + ": " + parser.Error();
        return false;
    }
    if (!root.IsObject()) {
        error = path + ": the document is not an object";
        return false;
    }
    return true;
}

bool IsLetterOrUnderscore(char c) {
    return (c >= 'a' && c <= 'z') || c == '_';
}

// The placeholders every language must carry for `spec`, sorted.
std::vector<std::string> ExpectedPlaceholders(const KeySpec& spec) {
    std::vector<std::string> names;
    for (const std::string_view name : spec.placeholders) {
        if (!name.empty()) names.emplace_back(name);
    }
    std::sort(names.begin(), names.end());
    return names;
}

const KeySpec* FindSpec(std::string_view key) {
    for (const KeySpec& spec : kKeys) {
        if (spec.key == key) return &spec;
    }
    return nullptr;
}

std::string Join(const std::vector<std::string>& names) {
    std::string out;
    for (const std::string& name : names) out += (out.empty() ? "" : ", ") + name;
    return out.empty() ? "none" : out;
}

// Whether `value` carries exactly the placeholders `spec` names; `why` says how not.
bool PlaceholdersFit(const KeySpec& spec, const std::string& value, std::string& why) {
    bool wellFormed = true;
    const std::vector<std::string> found = PlaceholdersOf(value, wellFormed);
    if (!wellFormed) {
        why = "has a brace that is not a {placeholder}";
        return false;
    }
    const std::vector<std::string> expected = ExpectedPlaceholders(spec);
    if (found != expected) {
        why = "has the placeholders (" + Join(found) + ") where English has (" + Join(expected) + ")";
        return false;
    }
    return true;
}

// ---- the `fonts` block -----------------------------------------------------------------------------------------------------------------

bool IsIdentifier(const std::string& text) {
    if (text.empty()) return false;
    for (const char c : text) {
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return false;
    }
    return true;
}

bool IsWholeNumber(const Json::Value& value, int low, int high, int& out) {
    if (!value.IsNumber()) return false;
    const double number = value.AsNumber();
    if (number != std::floor(number) || number < low || number > high) return false;
    out = static_cast<int>(number);
    return true;
}

bool IsNumberIn(const Json::Value& value, double low, double high, double& out) {
    if (!value.IsNumber()) return false;
    const double number = value.AsNumber();
    if (!(number >= low && number <= high)) return false; // NaN fails here
    out = number;
    return true;
}

// Every member of `object` is one of `allowed` or a note.
bool OnlyMembers(const Json::Value& object, std::initializer_list<const char*> allowed, const std::string& where, std::string& error) {
    for (const auto& [key, value] : object.AsObject()) {
        (void)value;
        if (IsNote(key)) continue;
        bool known = false;
        for (const char* name : allowed) known = known || key == name;
        if (!known) {
            error = where + " has the unknown member '" + key + "'";
            return false;
        }
    }
    return true;
}

// A name that stays where it is joined: no separator, no "..", and no ':' - on Windows "C:x.ttf" is relative to a drive's current folder, and joined to
// the fonts folder it REPLACES that folder (and "a.ttf:s" is an NTFS stream, not a file). One rule for the folder and for a face's file.
bool IsPlainName(const std::string& text) {
    return text.find_first_of("/\\:") == std::string::npos && text.find("..") == std::string::npos;
}

bool Overlaps(const FontBake::CodeRange& a, const FontBake::CodeRange& b) {
    return a.first <= b.last && b.first <= a.last;
}

bool ReadFaces(const Json::Value& list, const std::string& where, FontBake::Plan& plan, std::string& error) {
    if (!list.IsArray() || list.AsArray().empty()) {
        error = where + ": 'faces' must be a non-empty array";
        return false;
    }
    for (std::size_t i = 0; i < list.AsArray().size(); ++i) {
        const std::string at = where + ".faces[" + std::to_string(i) + "]";
        const Json::Value& item = list.AsArray()[i];
        if (!item.IsObject()) {
            error = at + " is not an object";
            return false;
        }
        if (!OnlyMembers(item, {FontsKey::kId, FontsKey::kFile, FontsKey::kReference, FontsKey::kReferenceHeight}, at, error)) return false;
        FontBake::FaceSpec face;
        face.id = item[FontsKey::kId].AsString();
        if (!item[FontsKey::kId].IsString() || !IsIdentifier(face.id) || plan.Face(face.id) != nullptr) {
            error = at + ": 'id' must be a new name of lower-case letters, digits and underscores";
            return false;
        }
        face.file = item[FontsKey::kFile].AsString();
        // A .ttf, by name and by name alone: tools/package/closure.py resolves a .fnt or .png name as an original asset and refuses the package.
        if (!item[FontsKey::kFile].IsString() || !IsPlainName(face.file) || face.file.size() < 5 || face.file.compare(face.file.size() - 4, 4, ".ttf") != 0) {
            error = at + ": 'file' must be the name of a .ttf in the fonts folder";
            return false;
        }
        if (!item[FontsKey::kReference].IsString() || !FontBake::ParseCodePoint(item[FontsKey::kReference].AsString(), face.reference)) {
            error = at + ": 'reference' must be a code point written U+XXXX";
            return false;
        }
        if (!IsNumberIn(item[FontsKey::kReferenceHeight], 0.1, kMaxReferenceHeight, face.referenceHeight)) {
            error = at + ": 'reference_height' must be a number from 0.1 to " + std::to_string(static_cast<int>(kMaxReferenceHeight));
            return false;
        }
        plan.faces.push_back(std::move(face));
    }
    return true;
}

bool ReadScripts(const Json::Value& list, const std::string& where, FontBake::Plan& plan, std::string& error) {
    if (!list.IsArray() || list.AsArray().empty()) {
        error = where + ": 'scripts' must be a non-empty array";
        return false;
    }
    for (std::size_t i = 0; i < list.AsArray().size(); ++i) {
        const std::string at = where + ".scripts[" + std::to_string(i) + "]";
        const Json::Value& item = list.AsArray()[i];
        if (!item.IsObject()) {
            error = at + " is not an object";
            return false;
        }
        if (!OnlyMembers(item, {FontsKey::kId, FontsKey::kRanges}, at, error)) return false;
        FontBake::ScriptSpec script;
        script.id = item[FontsKey::kId].AsString();
        const bool fresh = std::none_of(plan.scripts.begin(), plan.scripts.end(), [&](const FontBake::ScriptSpec& s) { return s.id == script.id; });
        if (!item[FontsKey::kId].IsString() || !IsIdentifier(script.id) || !fresh) {
            error = at + ": 'id' must be a new name of lower-case letters, digits and underscores";
            return false;
        }
        const Json::Value& ranges = item[FontsKey::kRanges];
        if (!ranges.IsArray() || ranges.AsArray().empty()) {
            error = at + ": 'ranges' must be a non-empty array";
            return false;
        }
        for (const Json::Value& text : ranges.AsArray()) {
            FontBake::CodeRange range;
            if (!text.IsString() || !FontBake::ParseRange(text.AsString(), range)) {
                error = at + ": a range must be written FIRST-LAST in upper-case hex (3000-30FF)";
                return false;
            }
            // A code point has one script: two ranges that overlap would route it by the order of a list.
            for (const FontBake::ScriptSpec& other : plan.scripts) {
                for (const FontBake::CodeRange& seen : other.ranges) {
                    if (Overlaps(range, seen)) {
                        error = at + ": the range " + text.AsString() + " overlaps one of the script '" + other.id + "'";
                        return false;
                    }
                }
            }
            for (const FontBake::CodeRange& seen : script.ranges) {
                if (Overlaps(range, seen)) {
                    error = at + ": the range " + text.AsString() + " overlaps another of the same script";
                    return false;
                }
            }
            script.ranges.push_back(range);
        }
        plan.scripts.push_back(std::move(script));
    }
    return true;
}

bool ReadSlots(const Json::Value& list, const std::string& where, FontBake::Plan& plan, std::string& error) {
    const std::array<std::string_view, 2> wanted = {kSlotMatura, kSlotVerdana};
    if (!list.IsArray() || list.AsArray().size() != wanted.size()) {
        error = where + ": 'slots' must be an array of exactly the slots " + std::string(wanted[0]) + " and " + std::string(wanted[1]);
        return false;
    }
    for (std::size_t i = 0; i < wanted.size(); ++i) {
        const std::string at = where + ".slots[" + std::to_string(i) + "]";
        const Json::Value& item = list.AsArray()[i];
        if (!item.IsObject()) {
            error = at + " is not an object";
            return false;
        }
        if (!OnlyMembers(item,
                         {FontsKey::kId, FontsKey::kOriginalFont, FontsKey::kLineHeight, FontsKey::kBase, FontsKey::kCapHeight, FontsKey::kPadding,
                          FontsKey::kSpacing, FontsKey::kPageSize, FontsKey::kShadow, FontsKey::kEmbolden, FontsKey::kTextFace, FontsKey::kScriptFaces},
                         at, error)) {
            return false;
        }
        FontBake::SlotSpec slot;
        slot.id = item[FontsKey::kId].AsString();
        if (!item[FontsKey::kId].IsString() || slot.id != wanted[i]) {
            error = at + ": 'id' must be \"" + std::string(wanted[i]) + "\" - the slots are listed in this order";
            return false;
        }
        slot.originalFont = item[FontsKey::kOriginalFont].AsString();
        if (!item[FontsKey::kOriginalFont].IsString() || slot.originalFont.empty() || slot.originalFont.find('.') != std::string::npos) {
            error = at + ": 'original_font' must be the original font's name without an extension";
            return false;
        }
        if (!IsWholeNumber(item[FontsKey::kLineHeight], 1, kMaxLineHeight, slot.lineHeight)) {
            error = at + ": 'line_height' must be a whole number from 1 to " + std::to_string(kMaxLineHeight);
            return false;
        }
        if (!IsWholeNumber(item[FontsKey::kBase], 1, slot.lineHeight, slot.base)) {
            error = at + ": 'base' must be a whole number from 1 to line_height";
            return false;
        }
        if (!IsNumberIn(item[FontsKey::kCapHeight], 1.0, static_cast<double>(slot.lineHeight), slot.capHeight)) {
            error = at + ": 'cap_height' must be a number from 1 to line_height";
            return false;
        }
        const Json::Value& padding = item[FontsKey::kPadding];
        if (!padding.IsArray() || padding.AsArray().size() != 4 || !IsWholeNumber(padding.AsArray()[0], 0, kMaxPadding, slot.padUp) ||
            !IsWholeNumber(padding.AsArray()[1], 0, kMaxPadding, slot.padRight) || !IsWholeNumber(padding.AsArray()[2], 0, kMaxPadding, slot.padDown) ||
            !IsWholeNumber(padding.AsArray()[3], 0, kMaxPadding, slot.padLeft)) {
            error = at + ": 'padding' must be four whole numbers 0-" + std::to_string(kMaxPadding) + " (up, right, down, left, as BMFont writes them)";
            return false;
        }
        const Json::Value& spacing = item[FontsKey::kSpacing];
        if (!spacing.IsArray() || spacing.AsArray().size() != 2 || !IsWholeNumber(spacing.AsArray()[0], 0, kMaxPadding, slot.spacingX) ||
            !IsWholeNumber(spacing.AsArray()[1], 0, kMaxPadding, slot.spacingY)) {
            error = at + ": 'spacing' must be two whole numbers 0-" + std::to_string(kMaxPadding);
            return false;
        }
        if (!IsWholeNumber(item[FontsKey::kPageSize], kMinPageSize, kMaxPageSize, slot.pageSize)) {
            error = at + ": 'page_size' must be a whole number from " + std::to_string(kMinPageSize) + " to " + std::to_string(kMaxPageSize);
            return false;
        }
        const Json::Value& shadow = item[FontsKey::kShadow];
        if (!shadow.IsObject()) {
            error = at + ": 'shadow' must be an object";
            return false;
        }
        if (!OnlyMembers(shadow, {FontsKey::kDx, FontsKey::kDy, FontsKey::kSigma, FontsKey::kAlpha}, at + ".shadow", error)) return false;
        if (!IsNumberIn(shadow[FontsKey::kDx], -kMaxShadowOffset, kMaxShadowOffset, slot.shadow.dx) ||
            !IsNumberIn(shadow[FontsKey::kDy], -kMaxShadowOffset, kMaxShadowOffset, slot.shadow.dy) ||
            !IsNumberIn(shadow[FontsKey::kSigma], 0.0, kMaxShadowSigma, slot.shadow.sigma) || !IsNumberIn(shadow[FontsKey::kAlpha], 0.0, 1.0, slot.shadow.alpha)) {
            error = at + ".shadow: dx and dy must be numbers within 16 px, sigma 0-16, alpha 0-1";
            return false;
        }
        if (!IsNumberIn(item[FontsKey::kEmbolden], 0.0, 1.0, slot.embolden)) {
            error = at + ": 'embolden' must be a number from 0 to 1";
            return false;
        }
        slot.textFace = item[FontsKey::kTextFace].AsString();
        if (!item[FontsKey::kTextFace].IsString() || plan.Face(slot.textFace) == nullptr) {
            error = at + ": 'text_face' must be the id of a face";
            return false;
        }
        const Json::Value& scriptFaces = item[FontsKey::kScriptFaces];
        if (!scriptFaces.IsObject() || scriptFaces.AsObject().size() != plan.scripts.size()) {
            error = at + ": 'script_faces' must name a face for each of the " + std::to_string(plan.scripts.size()) + " scripts, and only them";
            return false;
        }
        for (const FontBake::ScriptSpec& script : plan.scripts) {
            const Json::Value& face = scriptFaces[script.id];
            if (!face.IsString() || plan.Face(face.AsString()) == nullptr) {
                error = at + ": 'script_faces' has no face (a face id) for the script '" + script.id + "'";
                return false;
            }
            slot.scriptFaces.emplace_back(script.id, face.AsString());
        }
        plan.slots.push_back(std::move(slot));
    }
    return true;
}

bool ReadFonts(const std::string& path, const Json::Value& block, FontBake::Plan& plan, std::string& error) {
    const std::string where = path + ": fonts";
    if (!block.IsObject()) {
        error = where + " must be an object (how a generated language is drawn)";
        return false;
    }
    if (!OnlyMembers(block, {FontsKey::kFolder, FontsKey::kFaces, FontsKey::kScripts, FontsKey::kSlots, FontsKey::kPickerSlot}, where, error)) return false;
    FontBake::Plan read;
    read.folder = block[FontsKey::kFolder].AsString();
    if (!block[FontsKey::kFolder].IsString() || read.folder.empty() || !IsPlainName(read.folder)) {
        error = where + ": 'folder' must be a folder name below the data directory";
        return false;
    }
    if (!ReadFaces(block[FontsKey::kFaces], where, read, error) || !ReadScripts(block[FontsKey::kScripts], where, read, error) ||
        !ReadSlots(block[FontsKey::kSlots], where, read, error)) {
        return false;
    }
    read.pickerSlot = block[FontsKey::kPickerSlot].AsString();
    if (!block[FontsKey::kPickerSlot].IsString() || read.Slot(read.pickerSlot) == nullptr) {
        error = where + ": 'picker_slot' must be the id of a slot";
        return false;
    }
    read.loaded = true;
    plan = std::move(read);
    return true;
}

} // namespace

// ---- the language table ---------------------------------------------------------------------------------------------------------

std::optional<Lang> LanguageTable::LangOfCode(std::string_view code) const {
    if (!m_loaded) return std::nullopt;
    for (std::size_t i = 0; i < kLangCount; ++i) {
        if (m_entries[i].code == code) return static_cast<Lang>(i);
    }
    return std::nullopt;
}

bool LoadLanguages(const std::string& path, LanguageTable& out, std::string& error) {
    Json::Value root;
    if (!ReadJson(path, root, error)) return false;
    for (const auto& [key, value] : root.AsObject()) {
        (void)value;
        if (!IsNote(key) && key != LanguageKey::kLanguages && key != LanguageKey::kFonts) {
            error = path + ": unknown member '" + key + "'";
            return false;
        }
    }
    const Json::Value& list = root[LanguageKey::kLanguages];
    if (!list.IsArray() || list.AsArray().size() != kLangCount) {
        error = path + ": 'languages' must be an array of exactly " + std::to_string(kLangCount) + " languages";
        return false;
    }

    std::array<LanguageInfo, kLangCount> read;
    for (std::size_t i = 0; i < kLangCount; ++i) {
        const std::string where = path + ": languages[" + std::to_string(i) + "]";
        const Json::Value& item = list.AsArray()[i];
        if (!item.IsObject()) {
            error = where + " is not an object";
            return false;
        }
        for (const auto& [key, value] : item.AsObject()) {
            (void)value;
            if (!IsNote(key) && key != LanguageKey::kCode && key != LanguageKey::kNameNative && key != LanguageKey::kRtl &&
                key != LanguageKey::kFontSet) {
                error = where + " has the unknown member '" + key + "'";
                return false;
            }
        }
        if (!item[LanguageKey::kCode].IsString() || item[LanguageKey::kCode].AsString() != kLangCodes[i]) {
            error = where + ": 'code' must be \"" + std::string(kLangCodes[i]) + "\" - the file lists the languages in the order of Strings::Lang";
            return false;
        }
        LanguageInfo& info = read[i];
        info.code = item[LanguageKey::kCode].AsString();
        if (!item[LanguageKey::kNameNative].IsString() || item[LanguageKey::kNameNative].AsString().empty() ||
            !Utf8::IsValid(item[LanguageKey::kNameNative].AsString())) {
            error = where + ": 'name_native' is missing, empty, or not valid UTF-8";
            return false;
        }
        info.nameNative = item[LanguageKey::kNameNative].AsString();
        if (!item[LanguageKey::kRtl].IsBool()) {
            error = where + ": 'rtl' is missing or not a bool";
            return false;
        }
        info.rtl = item[LanguageKey::kRtl].AsBool();
        const std::string set = item[LanguageKey::kFontSet].AsString();
        if (set == FontSetName::kOriginal) {
            info.fontSet = FontSet::Original;
        } else if (set == FontSetName::kGenerated) {
            info.fontSet = FontSet::Generated;
        } else {
            error = where + ": 'font_set' is not \"" + FontSetName::kOriginal + "\" or \"" + FontSetName::kGenerated + "\"";
            return false;
        }
    }
    // English is the original's own look, byte for byte: the original's fonts.
    if (read[IndexOf(kDefaultLang)].fontSet != FontSet::Original || read[IndexOf(kDefaultLang)].rtl) {
        error = path + ": the default language must be left to right and set in the original's fonts";
        return false;
    }
    FontBake::Plan fonts;
    if (!ReadFonts(path, root[LanguageKey::kFonts], fonts, error)) return false;
    out.m_entries = std::move(read);
    out.m_fonts = std::move(fonts);
    out.m_loaded = true;
    return true;
}

// ---- placeholders ----------------------------------------------------------------------------------------------------------------

std::vector<std::string> PlaceholdersOf(std::string_view text, bool& wellFormed) {
    wellFormed = true;
    std::vector<std::string> names;
    std::size_t i = 0;
    while (i < text.size()) {
        const char c = text[i];
        if (c == '}') {
            wellFormed = false;
            ++i;
            continue;
        }
        if (c != '{') {
            ++i;
            continue;
        }
        std::size_t j = i + 1;
        while (j < text.size() && IsLetterOrUnderscore(text[j])) ++j;
        if (j > i + 1 && j < text.size() && text[j] == '}') {
            names.emplace_back(text.substr(i + 1, j - i - 1));
            i = j + 1;
        } else {
            wellFormed = false;
            ++i;
        }
    }
    std::sort(names.begin(), names.end());
    return names;
}

std::string Fill(std::string_view text, std::initializer_list<std::pair<std::string_view, std::string>> values) {
    std::string out;
    out.reserve(text.size());
    std::size_t i = 0;
    while (i < text.size()) {
        if (text[i] != '{') {
            out.push_back(text[i]);
            ++i;
            continue;
        }
        std::size_t j = i + 1;
        while (j < text.size() && IsLetterOrUnderscore(text[j])) ++j;
        if (j > i + 1 && j < text.size() && text[j] == '}') {
            const std::string_view name = text.substr(i + 1, j - i - 1);
            const auto found = std::find_if(values.begin(), values.end(), [name](const auto& item) { return item.first == name; });
            if (found != values.end()) {
                out += found->second; // appended, never scanned again
            } else {
                out.append(text.substr(i, j + 1 - i)); // no value: left as written, where a reader will see it
            }
            i = j + 1;
        } else {
            out.push_back('{');
            ++i;
        }
    }
    return out;
}

// ---- the files ------------------------------------------------------------------------------------------------------------------

bool ReadStringsFile(const std::string& path, std::map<std::string, std::string>& strings, std::string& error) {
    Json::Value root;
    if (!ReadJson(path, root, error)) return false;
    for (const auto& [key, value] : root.AsObject()) {
        (void)value;
        if (!IsNote(key) && key != FileKey::kStrings) {
            error = path + ": unknown member '" + key + "'";
            return false;
        }
    }
    const Json::Value& block = root[FileKey::kStrings];
    if (!block.IsObject()) {
        error = path + ": 'strings' is missing or not an object";
        return false;
    }
    std::map<std::string, std::string> read;
    for (const auto& [key, value] : block.AsObject()) {
        if (IsNote(key)) continue;
        if (!value.IsString() || value.AsString().empty()) {
            error = path + ": '" + key + "' is not a non-empty string";
            return false;
        }
        if (!Utf8::IsValid(value.AsString())) {
            error = path + ": '" + key + "' is not valid UTF-8";
            return false;
        }
        read.emplace(key, value.AsString());
    }
    strings = std::move(read);
    return true;
}

bool Table::LoadEnglish(const std::string& dataDir, std::string& error) {
    const std::string path = dataDir + kLangDirectory + "/" + std::string(m_languages.CodeOf(kDefaultLang)) + kFileExtension;
    std::map<std::string, std::string> english;
    if (!ReadStringsFile(path, english, error)) return false;
    for (const KeySpec& spec : kKeys) {
        const auto found = english.find(std::string(spec.key));
        if (found == english.end()) {
            error = path + ": lacks the key '" + std::string(spec.key) + "'";
            return false;
        }
        std::string why;
        if (!PlaceholdersFit(spec, found->second, why)) {
            error = path + ": '" + std::string(spec.key) + "' " + why;
            return false;
        }
    }
    for (const auto& [key, value] : english) {
        (void)value;
        if (FindSpec(key) == nullptr) {
            error = path + ": the key '" + key + "' is not one of the port's (sim/Strings.hpp)";
            return false;
        }
    }
    m_english = std::move(english);
    return true;
}

bool Table::Load(const std::string& dataDir, Lang lang, std::string& error) {
    *this = Table{};
    error.clear();
    if (!LoadLanguages(dataDir + kLanguagesFile, m_languages, error) || !LoadEnglish(dataDir, error)) {
        *this = Table{};
        return false;
    }
    m_lang = lang;
    if (lang != kDefaultLang) {
        const std::string code = m_languages.CodeOf(lang);
        const std::string path = dataDir + kLangDirectory + "/" + code + kFileExtension;
        std::map<std::string, std::string> own;
        std::string why;
        if (!ReadStringsFile(path, own, why)) {
            m_notes.push_back(why + ": English is shown throughout");
            for (const KeySpec& spec : kKeys) m_fellBack.emplace_back(spec.key);
        } else {
            for (const KeySpec& spec : kKeys) {
                const std::string key(spec.key);
                const auto found = own.find(key);
                if (found == own.end()) {
                    m_notes.push_back(path + ": lacks the key '" + key + "': English is shown for it");
                    m_fellBack.push_back(key);
                    continue;
                }
                std::string fault;
                if (!PlaceholdersFit(spec, found->second, fault)) {
                    m_notes.push_back(path + ": '" + key + "' " + fault + ": English is shown for it");
                    m_fellBack.push_back(key);
                    continue;
                }
                m_chosen.emplace(key, found->second);
            }
            for (const auto& [key, value] : own) {
                (void)value;
                if (FindSpec(key) == nullptr) m_notes.push_back(path + ": the key '" + key + "' is not one of the port's: ignored");
            }
        }
    }
    m_loaded = true;
    return true;
}

std::string Table::Get(std::string_view key) const {
    const std::string name(key);
    if (const auto own = m_chosen.find(name); own != m_chosen.end()) return own->second;
    if (const auto english = m_english.find(name); english != m_english.end()) return english->second;
    return name;
}

std::string Table::Format(std::string_view key, std::initializer_list<std::pair<std::string_view, std::string>> values) const {
    return Fill(Get(key), values);
}

std::string Table::Show(std::string_view key, std::initializer_list<std::pair<std::string_view, std::string>> values) const {
    return Display(m_lang, Format(key, values));
}

std::string Table::Shape(std::string_view text) const {
    return Display(m_lang, text);
}

// ---- display ---------------------------------------------------------------------------------------------------------------------

std::string Display(Lang lang, std::string_view text) {
    // Arabic is the one language that is more than code points in a row. Everything else, English first, is the text as it is: not decoded,
    // not copied through a code point, so bit for bit what it was.
    if (lang == Lang::Ar) return ArabicShaping::VisualUtf8(text, true);
    return std::string(text);
}

namespace {

// `text` with every {placeholder} replaced by the digit zero: what the numbers the layer fills in need (FontBake's base set holds every
// digit anyway), and the braces are no glyph of the strings.
std::string WithSampleValues(const std::string& text) {
    std::string out;
    std::size_t i = 0;
    while (i < text.size()) {
        if (text[i] == '{') {
            std::size_t j = i + 1;
            while (j < text.size() && IsLetterOrUnderscore(text[j])) ++j;
            if (j > i + 1 && j < text.size() && text[j] == '}') {
                out += '0';
                i = j + 1;
                continue;
            }
        }
        out += text[i++];
    }
    return out;
}

void AddCodePoints(std::set<std::uint32_t>& into, const std::string& text) {
    for (const std::uint32_t point : Utf8::Decode(text)) {
        if (point >= 0x20) into.insert(point); // a line break is no glyph
    }
}

} // namespace

std::set<std::uint32_t> SlotCodePoints(const Table& table, std::string_view slot, const std::vector<std::string>& extra) {
    std::set<std::uint32_t> points;
    const Lang lang = table.Language();
    const auto addKey = [&](std::string_view key) { AddCodePoints(points, Display(lang, WithSampleValues(table.Get(key)))); };
    if (slot == kSlotMatura) {
        for (const std::string_view key : kMaturaKeys) addKey(key);
    } else if (slot == kSlotVerdana) {
        for (const std::string_view key : kVerdanaKeys) addKey(key);
    } else {
        return points;
    }
    for (const std::string& text : extra) AddCodePoints(points, Display(lang, text));
    return points;
}

std::set<std::uint32_t> PickerCodePoints(const LanguageTable& languages) {
    std::set<std::uint32_t> points;
    if (!languages.Loaded()) return points;
    for (std::size_t i = 0; i < kLangCount; ++i) {
        const Lang lang = static_cast<Lang>(i);
        AddCodePoints(points, Display(lang, languages.NativeName(lang)));
    }
    return points;
}

} // namespace MagicPortals::Strings
