#pragma once

// The interface languages and their strings (planning step 236, PORT-32).
//
// Magic Portals is English; the Enhanced port is shown in eleven languages, in the order of the Magic Rampage Companion app's picker and of
// Penumbra's Languages.hpp: en (the default) de es fr it pt ru tr uk ja ar. This module is the DATA side of that: which languages there are
// (game/data/lang/languages.json), the fourteen short interface strings of each (game/data/lang/<code>.json), and the one place text is
// prepared for the screen (Display). It draws nothing and knows no font; the layer wires it (planning step 238), with the fonts
// (FontBake) and the achievements' translations (Achievements::Localized); the picker is a later step's.
//
// WHAT IS TRANSLATED AND WHAT IS NOT. Only text the port draws with a FONT can be: the fourteen keys below, and the achievements and the secret
// achievement's two sentences (Asantee's words, so their translations stay out of git: they are Achievements::LoadTranslation's gitignored overlay). Text BAKED INTO the original's pictures stays
// English - TAP START, Achievements, Select chapter, level finished, game over, portals created, golden score, current score, points, the
// game-finished card and the credits strip's names - and the credits' proper nouns, the brand and the developer text do too.
//
// ENGLISH IS BIT FOR BIT WHAT IT WAS. The rules fields that hold the English today (ui.json's 'Part ', 'Chapter ', 'new!', ...) stay as the
// defaults of a caller that has no Table, and en.json is tied to them by a test (the drift case of tests/test_mp_strings.cpp).
//
// The files are UTF-8 JSON and the text is UTF-8 everywhere (sim/Utf8.hpp; Hud.hpp says how it is laid out). No non-ASCII character is
// written in C++ source: MSVC is built without /utf-8 here.
//
// Free of the engine's renderer, like the rest of sim/.

#include "sim/FontBake.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace MagicPortals::Strings {

// The languages, in the order of languages.json - which the load enforces, so this enum and that file cannot drift apart.
enum class Lang : std::uint8_t { En, De, Es, Fr, It, Pt, Ru, Tr, Uk, Ja, Ar, Count };
inline constexpr std::size_t kLangCount = static_cast<std::size_t>(Lang::Count);
inline constexpr Lang kDefaultLang = Lang::En;

constexpr std::size_t IndexOf(Lang lang) {
    return static_cast<std::size_t>(lang);
}

// Which fonts draw a language (languages.json `font_set`): the original's own bitmaps, for a language whose strings lie inside their set; or
// fonts sim/FontBake makes at run time from the faces in game/data/fonts, by the routing of languages.json's `fonts` block.
enum class FontSet { Original, Generated };

// The codes of languages.json, in the enum's order: the file must list exactly these (LoadLanguages), so the enum, this list and the file cannot
// drift apart. The table answers every other question about a language (its name, its direction, its font set).
inline constexpr std::array<std::string_view, kLangCount> kLangCodes = {"en", "de", "es", "fr", "it", "pt", "ru", "tr", "uk", "ja", "ar"};

// The set of code points the original's fonts hold, and where that comes from: every one of the seven .fnt files in the original's data/ has
// exactly the ids 32-126 and 160-255 (191 glyphs; the design investigation of planning step 236 parsed all of them), which is Latin-1 without its
// control columns. A language whose font_set is Original may use no other code point in its strings.
inline constexpr std::uint32_t kOriginalFirstAscii = 0x20;
inline constexpr std::uint32_t kOriginalLastAscii = 0x7E;
inline constexpr std::uint32_t kOriginalFirstLatin1 = 0xA0;
inline constexpr std::uint32_t kOriginalLastLatin1 = 0xFF;
constexpr bool InOriginalFonts(std::uint32_t codePoint) {
    return (codePoint >= kOriginalFirstAscii && codePoint <= kOriginalLastAscii) ||
           (codePoint >= kOriginalFirstLatin1 && codePoint <= kOriginalLastLatin1);
}

// ---- the keys ---------------------------------------------------------------------------------------------------------

// The fourteen strings. A key is a name, never a bare literal at a call site. Per-achievement text is not here, and neither is the secret
// achievement's (its title, and its sentence with the chapter and the level): they are Asantee's words, and the achievements overlay
// (Achievements::LoadTranslation, planning step 238) has its own gitignored files, which hold the achievements by id and a `secret` block.
namespace Key {
inline constexpr std::string_view kHudPart = "hud.part";                         // {n}: the level's number in its chapter
inline constexpr std::string_view kDashboardChapter = "dashboard.chapter";       // {n}: the chapter, 1-4
inline constexpr std::string_view kDashboardNew = "dashboard.new";
inline constexpr std::string_view kSelectorRequires = "selector.requires";       // {percent}: locking's chapter_unlock_percent
inline constexpr std::string_view kCardUnlocked = "card.unlocked";               // {title}: the achievement's title
inline constexpr std::string_view kCardLockedLevel = "card.locked_level";
inline constexpr std::string_view kCreditsEnhanced = "credits.enhanced";
inline constexpr std::string_view kCreditsEngine = "credits.engine";
inline constexpr std::string_view kOptionsTitle = "options.title";
inline constexpr std::string_view kOptionsModern = "options.modern";
inline constexpr std::string_view kOptionsClassic = "options.classic";
inline constexpr std::string_view kOptionsAdjust = "options.adjust";
inline constexpr std::string_view kHintCamera = "hint.camera";
inline constexpr std::string_view kLanguageTitle = "language.title";
} // namespace Key

// The placeholders a string can carry, as the names between braces.
namespace Placeholder {
inline constexpr std::string_view kN = "n";
inline constexpr std::string_view kPercent = "percent";
inline constexpr std::string_view kTitle = "title";
inline constexpr std::string_view kWorld = "world";
inline constexpr std::string_view kLevel = "level";
} // namespace Placeholder

// A key and the placeholders every language's string for it must carry (a multiset, in no order; "" for none).
struct KeySpec {
    std::string_view key;
    std::array<std::string_view, 2> placeholders;
};

inline constexpr std::array<KeySpec, 14> kKeys = {{
    {Key::kHudPart, {Placeholder::kN, ""}},
    {Key::kDashboardChapter, {Placeholder::kN, ""}},
    {Key::kDashboardNew, {"", ""}},
    {Key::kSelectorRequires, {Placeholder::kPercent, ""}},
    {Key::kCardUnlocked, {Placeholder::kTitle, ""}},
    {Key::kCardLockedLevel, {"", ""}},
    {Key::kCreditsEnhanced, {"", ""}},
    {Key::kCreditsEngine, {"", ""}},
    {Key::kOptionsTitle, {"", ""}},
    {Key::kOptionsModern, {"", ""}},
    {Key::kOptionsClassic, {"", ""}},
    {Key::kOptionsAdjust, {"", ""}},
    {Key::kHintCamera, {"", ""}},
    {Key::kLanguageTitle, {"", ""}},
}};

// The strings the Matura slot (Matura84_shadow.fnt) draws, and the ones the Verdana slot (Verdana40_shadow.fnt) does. Matura draws a
// capital I like a J (the glyph at id 73 is a swash), so no Matura string of a language set in the original's fonts may hold a capital 'I'
// (the translators were told, and tests/test_mp_strings.cpp checks). Every key is in exactly one list.
inline constexpr std::array<std::string_view, 11> kMaturaKeys = {
    Key::kHudPart,        Key::kDashboardChapter, Key::kSelectorRequires, Key::kCreditsEnhanced,
    Key::kCreditsEngine,  Key::kOptionsTitle,     Key::kOptionsModern,    Key::kOptionsClassic,
    Key::kOptionsAdjust,  Key::kHintCamera,       Key::kLanguageTitle,
};
inline constexpr std::array<std::string_view, 3> kVerdanaKeys = {
    Key::kDashboardNew, Key::kCardUnlocked, Key::kCardLockedLevel,
};

// ---- the language table (game/data/lang/languages.json) ------------------------------------------------------------------

struct LanguageInfo {
    std::string code;       // "de"
    std::string nameNative; // the language's own name in its own script, UTF-8
    bool rtl = false;
    FontSet fontSet = FontSet::Original;
    // An entry has exactly these four members (and notes), and anything else is refused. HOW a Generated set is drawn is the same for every
    // such language - one routing of script to face per slot - and is the document's `fonts` block (LanguageTable::Fonts), not an entry's.
};

// The slots the `fonts` block must define, in this order: the original's two font slots a generated language replaces. Matura's draws the
// words, Verdana's the achievements and "new!" (kMaturaKeys and kVerdanaKeys say which key is drawn by which). Matura128 draws the counters,
// whose digits every language keeps from the original.
inline constexpr std::string_view kSlotMatura = "matura";
inline constexpr std::string_view kSlotVerdana = "verdana";

class LanguageTable {
public:
    // The code is the language's, or nothing: an unknown code is NOT English.
    std::optional<Lang> LangOfCode(std::string_view code) const;
    const std::string& CodeOf(Lang lang) const { return m_entries[IndexOf(lang)].code; }
    const std::string& NativeName(Lang lang) const { return m_entries[IndexOf(lang)].nameNative; }
    bool Rtl(Lang lang) const { return m_entries[IndexOf(lang)].rtl; }
    FontSet FontSetOf(Lang lang) const { return m_entries[IndexOf(lang)].fontSet; }
    const LanguageInfo& Info(Lang lang) const { return m_entries[IndexOf(lang)]; }
    // How a generated language's fonts are made: the document's `fonts` block (faces, script routing, the two slots), loaded strictly.
    const FontBake::Plan& Fonts() const { return m_fonts; }
    bool Loaded() const { return m_loaded; }

private:
    friend bool LoadLanguages(const std::string& path, LanguageTable& out, std::string& error);
    std::array<LanguageInfo, kLangCount> m_entries;
    FontBake::Plan m_fonts;
    bool m_loaded = false;
};

// languages.json, strictly: it must list exactly kLangCount languages whose order is the enum's - code, name_native (non-empty, valid
// UTF-8), rtl (a bool) and font_set ('original' or 'generated') each - and `en` must be the first and 'original'; and carry the `fonts`
// block that says how a generated language is drawn (faces, scripts, the two slots; FontBake.hpp has the members). Anything else, an unknown
// member included (but not a `_` note), refuses the load: `out` is left as it was and `error` names the file and the member.
bool LoadLanguages(const std::string& path, LanguageTable& out, std::string& error);

// ---- the strings -------------------------------------------------------------------------------------------------------------

// The files' names within the port's data directory (the layer's portData, game/data in the source tree).
inline constexpr const char* kLangDirectory = "/lang";
inline constexpr const char* kLanguagesFile = "/lang/languages.json";
inline constexpr const char* kFileExtension = ".json";

// The placeholders of `text` as a sorted list of the names between braces (a name repeated is repeated). `wellFormed` is false for a
// brace that opens no `{name}` (a lone '{' or '}', an empty or unfinished name); a name is lower-case letters and underscores.
std::vector<std::string> PlaceholdersOf(std::string_view text, bool& wellFormed);

// `text` with each `{name}` that has a value in `values` replaced by it, in one left-to-right scan: a value is never scanned again (a
// title that holds "{n}" stays), each occurrence is replaced, and a `{name}` with no value is left as it is, visibly.
std::string Fill(std::string_view text, std::initializer_list<std::pair<std::string_view, std::string>> values);

// One language file as written: `strings` holds its keys (notes, members that start with '_', are skipped). Checks the shape and that every
// value is a non-empty string of valid UTF-8; the KEY SET and the placeholders are the caller's to check. False with `error` naming `path`.
bool ReadStringsFile(const std::string& path, std::map<std::string, std::string>& strings, std::string& error);

// The strings of one language over English, read from the port's data directory.
class Table {
public:
    // Reads <dataDir>/lang/languages.json, then <dataDir>/lang/en.json STRICTLY - its keys are exactly kKeys, each value has exactly its
    // key's placeholders, none is empty - and then, unless `lang` is English, <dataDir>/lang/<code>.json. False, with `error`, for any
    // fault in the table or in en.json (the game cannot show text without them), leaving the Table empty.
    //
    // The chosen language's own file is read with a fallback and never refuses: a file that is missing or unreadable, a key it lacks, a
    // value that is empty or carries other placeholders than English's, or a member that is no key, each leaves English in that place and
    // is written into Notes() ONCE (this is the "logged once per run": the layer logs the notes at load, and Get never logs).
    bool Load(const std::string& dataDir, Lang lang, std::string& error);

    Lang Language() const { return m_lang; }
    const LanguageTable& Languages() const { return m_languages; }
    bool Loaded() const { return m_loaded; }

    // The chosen language's string for `key`, else English's, else the key's own name (a key nobody defined shows as itself, visibly).
    std::string Get(std::string_view key) const;

    // Get(key) with its {placeholders} filled (Fill).
    std::string Format(std::string_view key, std::initializer_list<std::pair<std::string_view, std::string>> values) const;

    // What reaches a font (planning step 238): Format(key, values) and then Display, once - the one call every composed string of the layer
    // makes, so that the shaper of a right-to-left language runs after the placeholders are filled and never before or twice. For English it is
    // Format, byte for byte.
    std::string Show(std::string_view key, std::initializer_list<std::pair<std::string_view, std::string>> values = {}) const;
    // Display for the table's own language, for a text that was composed elsewhere (an achievement's title and description, a popup's lines).
    std::string Shape(std::string_view text) const;
    // Whether the table's language is written right to left (languages.json `rtl`).
    bool Rtl() const { return m_languages.Rtl(m_lang); }

    // The keys the chosen language does not define itself, and so show in English. Empty for English.
    const std::vector<std::string>& FellBack() const { return m_fellBack; }
    // What Load found wrong with the chosen language's file, one line each, for the caller to log once.
    const std::vector<std::string>& Notes() const { return m_notes; }

private:
    bool LoadEnglish(const std::string& dataDir, std::string& error);

    LanguageTable m_languages;
    Lang m_lang = kDefaultLang;
    bool m_loaded = false;
    std::map<std::string, std::string> m_english;
    std::map<std::string, std::string> m_chosen; // only the keys the chosen language defines well; empty for English
    std::vector<std::string> m_fellBack;
    std::vector<std::string> m_notes;
};

// ---- what a slot draws ------------------------------------------------------------------------------------------------------------

// The code points the Matura slot or the Verdana slot (kSlotMatura, kSlotVerdana) draws for the table's language, AFTER Display (so the
// shaped forms of Arabic, not its letters): every string of the slot's keys with each {placeholder} replaced by the digit zero, and every
// string of `extra` - the texts that come from elsewhere and are drawn in the slot, the achievements' titles and descriptions and the secret
// achievement's title and sentence (Achievements::SecretSamples) for the Verdana slot. FontBake adds digits and ASCII punctuation itself. An unknown slot id is an empty set.
std::set<std::uint32_t> SlotCodePoints(const Table& table, std::string_view slot, const std::vector<std::string>& extra = {});

// The code points of every language's native name after Display: what the picker's atlas holds. It is drawn in one font, so Cyrillic,
// kana and shaped Arabic all have to be in it.
std::set<std::uint32_t> PickerCodePoints(const LanguageTable& languages);

// ---- display ----------------------------------------------------------------------------------------------------------------

// The ONE place text is prepared for the screen: whatever draws or measures a string of a UI language takes it through here first, so that a
// language that needs more than code points in a row - Arabic - is handled in one function and not at fifteen draw sites.
//
// For Arabic it is the shaper (sim/ArabicShaping, planning step 237): the logical-order text becomes presentation forms (U+FE70-FEFF, the
// lam-alef ligature) in visual order, per line, with digit and Latin runs kept left to right and brackets mirrored, so that Hud's layout
// and measure stay left to right and unchanged. For every other language it is the identity, byte for byte, a malformed byte included.
// Apply it ONCE, after the placeholders are filled: its output is visual order, which another pass would reverse. Nothing may shape elsewhere.
std::string Display(Lang lang, std::string_view text);

} // namespace MagicPortals::Strings
