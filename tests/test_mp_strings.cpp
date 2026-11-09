// The interface languages and their strings (planning step 236, PORT-32): sim/Strings and game/data/lang/.
//
// Eleven languages - en (the default) de es fr it pt ru tr uk ja ar - and fourteen short strings each. What is pinned:
//
//   the table      languages.json lists exactly Strings::Lang's codes in its order, each with its own native name (pinned as code points), its
//                  direction and its font set; and the loader REFUSES a file that does not (a swapped pair, an unknown code, a missing or extra
//                  entry, an unknown member, a bad font_set, an rtl that is not a bool, an empty or ill-formed name, a default language set in
//                  generated fonts)
//   fonts block    (planning step 237) the document's `fonts` - faces, scripts, the two slots, the picker's slot - loads as the data the bake reads and
//                  is refused for each of thirty-odd faults, one at a time: a face that is not a .ttf by name, a range backwards or in lower case or
//                  overlapping another script's, a slot out of order, a padding of three, a page past 512, a shadow past its bounds, a script routed to
//                  no face, a picker in a slot nobody has
//   Display        the identity for every language but Arabic, byte for byte, a malformed byte included; Arabic is the shaper's (test_mp_arabic)
//   English        en.json is read strictly: a missing key, an extra key, a placeholder dropped, added, misnamed or unbalanced, an empty value,
//                  a value that is not a string, an unknown member, a missing file - each refuses the load and names what was wrong
//   parity         EVERY language file has exactly en.json's keys, the same placeholders for each (as a multiset), and the same number of
//                  line breaks; a note (a '_' member) is allowed and nothing else is
//   fallback       a language file that lacks a key, carries other placeholders than English's for it, has a key the port does not know, is
//                  missing, is not JSON or holds an empty or ill-formed value leaves English in the place it should have filled - once, in
//                  Notes() and FellBack(), never in Get
//   Format         Fill's single left-to-right scan: each occurrence replaced once, a value never scanned again, an unknown placeholder left
//                  as it is, lone and unfinished braces untouched
//   drift          en.json is the English the rules fields still hold, read through the loaders that read them (ui.json's caption, pause title,
//                  dashboard header and new label, selector warning with locking's percent, credits tail, controls.json's options words,
//                  camera.json's hint and achievements.json's two cards; the secret achievement's words are not in git at all: the overlay's, tests/test_mp_langsim)
//   charset        a language set in the ORIGINAL fonts uses no code point outside U+0020-007E and U+00A0-00FF (the .fnt files hold exactly
//                  ids 32-126 and 160-255) - none of the typographic quotes, dashes, ellipsis or ligatures that translators reach for - and
//                  no Matura string holds a capital I (Matura draws it like a J); every language is valid UTF-8 without a U+FFFD
//   hygiene        the files are UTF-8 without a BOM or a CR, hold real characters and no \u escape, name no .png or .fnt (tools/package/
//                  closure.py would resolve one as an original asset), and say in their _about that no native speaker reviewed them
//
// A mutation test runs this binary on a copy of game/data with one translation changed (MP_TEST_PORTDATA, as test_mp_hud does).
//
// No non-ASCII character is written in this file: MSVC is built without /utf-8 here, so the text under test is read from the JSON, and the
// few characters named here are code points.

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "core/BitmapFont.hpp"
#include "sim/Achievements.hpp"
#include "sim/Controls.hpp"
#include "sim/Credits.hpp"
#include "sim/Dashboard.hpp"
#include "sim/Hint.hpp"
#include "sim/Hud.hpp"
#include "sim/Locking.hpp"
#include "sim/Options.hpp"
#include "sim/Pause.hpp"
#include "sim/Selector.hpp"
#include "sim/Strings.hpp"
#include "sim/Utf8.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

using namespace MagicPortals;
using Strings::Lang;

namespace {

namespace fs = std::filesystem;

// The port's own data: MP_TEST_PORTDATA when it is set (TEST ONLY: a mutation test runs this same binary on a copy of game/data with one
// number changed), else the build's own. _dupenv_s: std::getenv is C4996 on Windows.
std::string PortDataDir() {
    std::string text;
#ifdef _WIN32
    char* value = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&value, &length, "MP_TEST_PORTDATA") == 0 && value != nullptr) {
        text = value;
        std::free(value);
    }
#else
    if (const char* value = std::getenv("MP_TEST_PORTDATA"); value != nullptr) text = value;
#endif
    if (text.empty()) return MAGICPORTALS_PORT_DATA_DIR;
    // Said, so that a stray variable left in a shell cannot make a normal run fail or pass for no visible reason.
    std::printf("test_mp_strings: MUTATION - MP_TEST_PORTDATA is set, the port's data is read from %s, not from game/data\n", text.c_str());
    return text;
}

const std::string kData = PortDataDir();
const std::string kLangDir = kData + Strings::kLangDirectory;

using Map = std::map<std::string, std::string>;
using CodePoints = std::vector<std::uint32_t>;

// ---- files -------------------------------------------------------------------------------------------------------------------------

std::string ReadAll(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void WriteFile(const fs::path& path, const std::string& text) {
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file << text;
}

fs::path ScratchRoot() {
    return fs::temp_directory_path() / "supersonic-test-mp-strings";
}

// A data directory of this case's own with a copy of the real lang/ in it, emptied first: a table that read what the last run left behind
// would pass or fail for reasons nothing here states.
fs::path DataCopy(const char* name) {
    const fs::path dir = ScratchRoot() / name;
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    fs::copy(kLangDir, dir / "lang", fs::copy_options::recursive, ec);
    CHECK_MSG(!ec, std::string("the lang directory copies for ") + name + ": " + ec.message());
    return dir;
}

// A language file's text, from `strings`, with real characters; `about` adds a note.
std::string Escaped(const std::string& text) {
    std::string out;
    for (const char c : text) {
        if (c == '"' || c == '\\') {
            out.push_back('\\');
            out.push_back(c);
        } else if (c == '\n') {
            out += "\\n";
        } else {
            out.push_back(c);
        }
    }
    return out;
}

std::string StringsJson(const Map& strings, bool note = true) {
    std::string out = "{\n";
    if (note) out += "  \"_about\": [\"a test's own copy\"],\n";
    out += "  \"strings\": {\n";
    std::size_t at = 0;
    for (const auto& [key, value] : strings) {
        out += "    \"" + key + "\": \"" + Escaped(value) + "\"" + (++at < strings.size() ? "," : "") + "\n";
    }
    return out + "  }\n}\n";
}

Map ReadStrings(const std::string& code) {
    Map strings;
    std::string error;
    CHECK_MSG(Strings::ReadStringsFile(kLangDir + "/" + code + Strings::kFileExtension, strings, error), "the file reads: " + error);
    return strings;
}

// ---- small helpers -------------------------------------------------------------------------------------------------------------------

CodePoints Cps(const std::string& utf8) {
    return Utf8::Decode(utf8);
}

std::string B(std::initializer_list<int> bytes) {
    std::string out;
    for (const int byte : bytes) out.push_back(static_cast<char>(static_cast<unsigned char>(byte)));
    return out;
}

bool Contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

std::string Show(const CodePoints& points) {
    std::string out;
    for (const std::uint32_t point : points) {
        char buffer[16];
        std::snprintf(buffer, sizeof(buffer), "U+%04X ", static_cast<unsigned>(point));
        out += buffer;
    }
    return out;
}

// `from` replaced by `to` where it occurs exactly once, or the test fails (a mutation that found nothing to change would test nothing).
std::string ReplaceOnce(std::string text, const std::string& from, const std::string& to) {
    const std::size_t at = text.find(from);
    CHECK_MSG(at != std::string::npos && text.find(from, at + 1) == std::string::npos, "the text to change occurs once: " + from);
    if (at == std::string::npos) return text;
    return text.replace(at, from.size(), to);
}

std::vector<Lang> AllLangs() {
    std::vector<Lang> all;
    for (std::size_t i = 0; i < Strings::kLangCount; ++i) all.push_back(static_cast<Lang>(i));
    return all;
}

Strings::LanguageTable RealLanguages() {
    Strings::LanguageTable table;
    std::string error;
    CHECK_MSG(Strings::LoadLanguages(kData + Strings::kLanguagesFile, table, error), "languages.json reads: " + error);
    return table;
}

std::set<std::string> AllKeys() {
    std::set<std::string> keys;
    for (const Strings::KeySpec& spec : Strings::kKeys) keys.insert(std::string(spec.key));
    return keys;
}

// ---- the table -----------------------------------------------------------------------------------------------------------------------------

void TheLanguageTableIsTheEnumsOrder() {
    const Strings::LanguageTable table = RealLanguages();
    CHECK(table.Loaded());
    CHECK_EQ(static_cast<int>(Strings::kLangCount), 11);
    CHECK_EQ(static_cast<int>(Lang::En), 0);
    CHECK_EQ(static_cast<int>(Lang::Ar), 10);
    for (const Lang lang : AllLangs()) {
        const std::size_t i = Strings::IndexOf(lang);
        CHECK_MSG(table.CodeOf(lang) == Strings::kLangCodes[i], "the code of language " + std::to_string(i) + " is " + table.CodeOf(lang));
        CHECK_MSG(table.LangOfCode(table.CodeOf(lang)) == lang, "and it reads back as itself: " + table.CodeOf(lang));
        CHECK_MSG(table.Info(lang).code == table.CodeOf(lang), "Info agrees");
    }
    // The order is the Companion app's picker's and Penumbra's: en de es fr it pt ru tr uk ja ar.
    std::string order;
    for (const Lang lang : AllLangs()) order += (order.empty() ? "" : " ") + table.CodeOf(lang);
    CHECK_MSG(order == "en de es fr it pt ru tr uk ja ar", "the order: " + order);

    CHECK_MSG(!table.LangOfCode("xx"), "an unknown code is nothing");
    CHECK_MSG(!table.LangOfCode(""), "and so is an empty one");
    CHECK_MSG(!table.LangOfCode("EN"), "and a capital one");
    CHECK_MSG(!table.LangOfCode("en-US"), "and a region tag");
    CHECK_MSG(!Strings::LanguageTable{}.LangOfCode("en"), "a table that was never loaded knows no language");

    // The native names, as code points - the picker draws each in its own script.
    const std::vector<std::pair<Lang, CodePoints>> names = {
        {Lang::En, Cps("English")},
        {Lang::De, Cps("Deutsch")},
        {Lang::Es, {'E', 's', 'p', 'a', 0xF1, 'o', 'l'}},
        {Lang::Fr, {'F', 'r', 'a', 'n', 0xE7, 'a', 'i', 's'}},
        {Lang::It, Cps("Italiano")},
        {Lang::Pt, {'P', 'o', 'r', 't', 'u', 'g', 'u', 0xEA, 's'}},
        {Lang::Ru, {0x420, 0x443, 0x441, 0x441, 0x43A, 0x438, 0x439}},
        {Lang::Tr, {'T', 0xFC, 'r', 'k', 0xE7, 'e'}},
        {Lang::Uk, {0x423, 0x43A, 0x440, 0x430, 0x457, 0x43D, 0x441, 0x44C, 0x43A, 0x430}},
        {Lang::Ja, {0x65E5, 0x672C, 0x8A9E}},
        {Lang::Ar, {0x627, 0x644, 0x639, 0x631, 0x628, 0x64A, 0x629}},
    };
    for (const auto& [lang, want] : names) {
        const CodePoints got = Cps(table.NativeName(lang));
        CHECK_MSG(got == want, "the native name of " + table.CodeOf(lang) + " is " + Show(got) + "not " + Show(want));
    }

    // Direction and font set: only Arabic is right to left, and the original's own fonts serve exactly the languages whose strings lie
    // inside their Latin-1 set.
    for (const Lang lang : AllLangs()) {
        CHECK_MSG(table.Rtl(lang) == (lang == Lang::Ar), "only Arabic is right to left: " + table.CodeOf(lang));
        const bool original = lang == Lang::En || lang == Lang::De || lang == Lang::Es || lang == Lang::Fr || lang == Lang::It || lang == Lang::Pt;
        CHECK_MSG(table.FontSetOf(lang) == (original ? Strings::FontSet::Original : Strings::FontSet::Generated),
                  "the font set of " + table.CodeOf(lang));
    }
}

// languages.json as the test writes it: `codes` in this order, each entry from `tweak`.
struct Entry {
    explicit Entry(std::string c) : code(std::move(c)) {}
    std::string code;
    std::string name = "\"Name\"";
    std::string rtl = "false";
    std::string fontSet = "\"original\"";
    std::string extra;
};

// A valid `fonts` block of the test's own: two faces, one script, the two slots. Cases below change one thing in it.
const std::string kTestFonts = R"({
    "_about": ["a test's"],
    "folder": "fonts",
    "faces": [
      {"id": "latin", "file": "a.ttf", "reference": "U+0048", "reference_height": 1.0},
      {"id": "kana", "file": "b.ttf", "reference": "U+65E5", "reference_height": 1.1}
    ],
    "scripts": [
      {"id": "japanese", "ranges": ["3000-30FF", "4E00-9FFF"]}
    ],
    "slots": [
      {"id": "matura", "original_font": "Matura84_shadow", "line_height": 84, "base": 54, "cap_height": 40, "padding": [2, 8, 8, 2], "spacing": [2, 2],
       "page_size": 512, "shadow": {"dx": 2.0, "dy": 2.0, "sigma": 2.0, "alpha": 0.99}, "embolden": 0.0, "text_face": "latin",
       "script_faces": {"japanese": "kana"}},
      {"id": "verdana", "original_font": "Verdana40_shadow", "line_height": 40, "base": 33, "cap_height": 24, "padding": [2, 6, 6, 2], "spacing": [2, 2],
       "page_size": 512, "shadow": {"dx": 1.0, "dy": 1.0, "sigma": 1.25, "alpha": 0.99}, "embolden": 0.0, "text_face": "latin",
       "script_faces": {"japanese": "kana"}}
    ],
    "picker_slot": "matura"
  })";

// languages.json as the test writes it. `fonts` is the block's text ("" for none) and `top` more members of the document.
std::string LanguagesJson(const std::vector<Entry>& entries, const std::string& fonts = kTestFonts, const std::string& top = "") {
    std::string out = "{\n  \"_about\": [\"a test's\"],\n" + top + (fonts.empty() ? "" : "  \"fonts\": " + fonts + ",\n") + "  \"languages\": [\n";
    for (std::size_t i = 0; i < entries.size(); ++i) {
        const Entry& e = entries[i];
        out += "    {\"code\": \"" + e.code + "\", \"name_native\": " + e.name + ", \"rtl\": " + e.rtl + ", \"font_set\": " + e.fontSet + e.extra + "}" +
               (i + 1 < entries.size() ? "," : "") + "\n";
    }
    return out + "  ]\n}\n";
}

std::vector<Entry> RightEntries() {
    std::vector<Entry> entries;
    for (const std::string_view code : Strings::kLangCodes) entries.push_back(Entry{std::string(code)});
    return entries;
}

bool Refused(const std::string& text, std::string& why) {
    const fs::path dir = ScratchRoot() / "languages";
    const fs::path file = dir / "languages.json";
    WriteFile(file, text);
    Strings::LanguageTable table;
    const bool loaded = Strings::LoadLanguages(file.string(), table, why);
    CHECK_MSG(loaded == table.Loaded(), "a table is loaded exactly when the load succeeded");
    return !loaded;
}

void TheLoaderRefusesATableThatIsNotTheEnums() {
    std::string why;
    // The control: the test's own right table loads, with a note and generated sets where the real one has them.
    {
        std::vector<Entry> entries = RightEntries();
        entries[Strings::IndexOf(Lang::Ar)].rtl = "true";
        entries[Strings::IndexOf(Lang::Ja)].fontSet = "\"generated\"";
        const fs::path file = ScratchRoot() / "languages" / "right.json";
        WriteFile(file, LanguagesJson(entries));
        Strings::LanguageTable table;
        CHECK_MSG(Strings::LoadLanguages(file.string(), table, why), "a right table loads: " + why);
        CHECK(table.Rtl(Lang::Ar) && table.FontSetOf(Lang::Ja) == Strings::FontSet::Generated);
        // A failed load leaves the table as it was.
        const std::string before = table.CodeOf(Lang::Ja);
        CHECK(!Strings::LoadLanguages((ScratchRoot() / "languages" / "nothing.json").string(), table, why));
        CHECK_MSG(table.Loaded() && table.CodeOf(Lang::Ja) == before, "and a refusal leaves a loaded table alone");
    }
    {
        std::vector<Entry> entries = RightEntries();
        std::swap(entries[1], entries[2]);
        CHECK_MSG(Refused(LanguagesJson(entries), why) && Contains(why, "languages[1]") && Contains(why, "de"), "two languages swapped: " + why);
    }
    {
        std::vector<Entry> entries = RightEntries();
        entries[3].code = "xx";
        CHECK_MSG(Refused(LanguagesJson(entries), why) && Contains(why, "languages[3]"), "an unknown code: " + why);
    }
    {
        std::vector<Entry> entries = RightEntries();
        entries.pop_back();
        CHECK_MSG(Refused(LanguagesJson(entries), why) && Contains(why, "exactly 11"), "a language missing: " + why);
        entries = RightEntries();
        entries.push_back(Entry{"zz"});
        CHECK_MSG(Refused(LanguagesJson(entries), why) && Contains(why, "exactly 11"), "a language too many: " + why);
    }
    {
        std::vector<Entry> entries = RightEntries();
        entries[4].extra = ", \"faces\": {}";
        CHECK_MSG(Refused(LanguagesJson(entries), why) && Contains(why, "faces"), "an entry member nothing reads (the routing is the document's `fonts`, not an entry's): " + why);
        entries = RightEntries();
        entries[4].extra = ", \"_note\": \"fine\"";
        const fs::path file = ScratchRoot() / "languages" / "note.json";
        WriteFile(file, LanguagesJson(entries));
        Strings::LanguageTable table;
        CHECK_MSG(Strings::LoadLanguages(file.string(), table, why), "but a '_' note is: " + why);
    }
    CHECK_MSG(Refused(LanguagesJson(RightEntries(), kTestFonts, "  \"colors\": 1,\n"), why) && Contains(why, "colors"), "a member of the document nothing reads: " + why);
    {
        std::vector<Entry> entries = RightEntries();
        entries[5].fontSet = "\"bitmap\"";
        CHECK_MSG(Refused(LanguagesJson(entries), why) && Contains(why, "font_set"), "a font_set that is neither: " + why);
        entries = RightEntries();
        entries[5].fontSet = "1";
        CHECK_MSG(Refused(LanguagesJson(entries), why) && Contains(why, "font_set"), "a font_set that is no string: " + why);
        entries = RightEntries();
        entries[6].rtl = "\"no\"";
        CHECK_MSG(Refused(LanguagesJson(entries), why) && Contains(why, "rtl"), "an rtl that is no bool: " + why);
        entries = RightEntries();
        entries[7].name = "\"\"";
        CHECK_MSG(Refused(LanguagesJson(entries), why) && Contains(why, "name_native"), "an empty native name: " + why);
        entries = RightEntries();
        entries[7].name = "3";
        CHECK_MSG(Refused(LanguagesJson(entries), why) && Contains(why, "name_native"), "a native name that is no string: " + why);
        entries = RightEntries();
        entries[7].name = "\"" + B({0xC3, 0x28}) + "\"";
        CHECK_MSG(Refused(LanguagesJson(entries), why) && Contains(why, "UTF-8"), "a native name that is not UTF-8: " + why);
    }
    {
        std::vector<Entry> entries = RightEntries();
        entries[0].fontSet = "\"generated\"";
        CHECK_MSG(Refused(LanguagesJson(entries), why) && Contains(why, "default language"), "English in generated fonts would not be bit for bit: " + why);
        entries = RightEntries();
        entries[0].rtl = "true";
        CHECK_MSG(Refused(LanguagesJson(entries), why) && Contains(why, "default language"), "nor would English right to left: " + why);
    }
    CHECK_MSG(Refused("[]", why) && Contains(why, "not an object"), "an array: " + why);
    CHECK_MSG(Refused("{ nope", why) && !why.empty(), "not JSON: " + why);
    {
        Strings::LanguageTable table;
        CHECK_MSG(!Strings::LoadLanguages((ScratchRoot() / "languages" / "absent.json").string(), table, why) && Contains(why, "cannot open"),
                  "a missing file: " + why);
    }
}

// The `fonts` block (planning step 237): read as strictly as the rest, since a typo in it would draw a language in the wrong face or no glyph.
void TheLoaderRefusesABadFontsBlock() {
    std::string why;
    {
        // The control, and what it loads as.
        const fs::path file = ScratchRoot() / "languages" / "fonts_right.json";
        WriteFile(file, LanguagesJson(RightEntries()));
        Strings::LanguageTable table;
        CHECK_MSG(Strings::LoadLanguages(file.string(), table, why), "the test's own fonts block loads: " + why);
        const FontBake::Plan& plan = table.Fonts();
        CHECK(plan.loaded && plan.folder == "fonts" && plan.faces.size() == 2 && plan.scripts.size() == 1 && plan.slots.size() == 2 && plan.pickerSlot == "matura");
        CHECK_MSG(plan.faces[1].reference == 0x65E5 && plan.faces[1].referenceHeight > 1.09 && plan.faces[1].referenceHeight < 1.11, "a face's reference glyph and ratio");
        CHECK_MSG(plan.scripts[0].ranges.size() == 2 && plan.scripts[0].ranges[1].first == 0x4E00 && plan.scripts[0].ranges[1].last == 0x9FFF, "a script's ranges");
        const FontBake::SlotSpec& verdana = *plan.Slot("verdana");
        CHECK_MSG(verdana.lineHeight == 40 && verdana.base == 33 && verdana.padRight == 6 && verdana.spacingX == 2 && verdana.pageSize == 512 &&
                      verdana.shadow.sigma > 1.24 && verdana.shadow.sigma < 1.26 && verdana.textFace == "latin" && verdana.scriptFaces.size() == 1,
                  "a slot's numbers and routing");
        // A failed load leaves the plan as it was.
        CHECK(!Strings::LoadLanguages((ScratchRoot() / "languages" / "nothing.json").string(), table, why));
        CHECK_MSG(table.Fonts().loaded && table.Fonts().slots.size() == 2, "and a refusal leaves a loaded plan alone");
    }
    // First occurrence of `from` replaced by `to`; the test fails if there is none (a mutation that changed nothing would test nothing).
    const auto first = [](std::string text, const std::string& from, const std::string& to) {
        const std::size_t at = text.find(from);
        CHECK_MSG(at != std::string::npos, "the text to change is in the test's block: " + from);
        if (at != std::string::npos) text.replace(at, from.size(), to);
        return text;
    };
    struct Case {
        const char* what;
        std::string from;
        std::string to;
        std::string needle;
    };
    const std::vector<Case> cases = {
        {"an unknown member of the block", "\"picker_slot\": \"matura\"", "\"picker_slot\": \"matura\", \"hinting\": true", "hinting"},
        {"a folder that climbs out of the data directory", "\"folder\": \"fonts\"", "\"folder\": \"../fonts\"", "folder"},
        {"a folder that is a drive", "\"folder\": \"fonts\"", "\"folder\": \"C:fonts\"", "folder"},
        {"an empty folder", "\"folder\": \"fonts\"", "\"folder\": \"\"", "folder"},
        {"a face id used twice", "\"id\": \"kana\"", "\"id\": \"latin\"", "faces[1]"},
        {"a face id with a capital", "\"id\": \"kana\"", "\"id\": \"Kana\"", "faces[1]"},
        {"a face that is a picture, not a .ttf", "\"file\": \"b.ttf\"", "\"file\": \"b.png\"", ".ttf"},
        {"a face that is a generated font's name", "\"file\": \"b.ttf\"", "\"file\": \"b.fnt\"", ".ttf"},
        {"a face file that climbs out of the folder", "\"file\": \"a.ttf\"", "\"file\": \"../a.ttf\"", ".ttf"},
        // Both end in .ttf, so only the ':' rule refuses them: on Windows the first replaces the fonts folder with a drive's current one, the second is a stream.
        {"a face file that is a drive's", "\"file\": \"a.ttf\"", "\"file\": \"C:a.ttf\"", ".ttf"},
        {"a face file that is a stream of another", "\"file\": \"b.ttf\"", "\"file\": \"a.ttf:b.ttf\"", ".ttf"},
        {"a face reference not written U+XXXX", "\"reference\": \"U+65E5\"", "\"reference\": \"65E5\"", "reference"},
        {"a face ratio of zero", "\"reference_height\": 1.1", "\"reference_height\": 0", "reference_height"},
        {"a face ratio past the bound", "\"reference_height\": 1.1", "\"reference_height\": 9", "reference_height"},
        {"a face member that is not one", "\"reference_height\": 1.1", "\"reference_height\": 1.1, \"bold\": true", "bold"},
        {"a range backwards", "\"3000-30FF\"", "\"30FF-3000\"", "range"},
        {"a range in lower case", "\"4E00-9FFF\"", "\"4e00-9fff\"", "range"},
        {"two ranges of one script that overlap", "\"4E00-9FFF\"", "\"3050-9FFF\"", "overlaps"},
        {"a script with no ranges", "\"ranges\": [\"3000-30FF\", \"4E00-9FFF\"]", "\"ranges\": []", "ranges"},
        {"the slots in the other order", "\"id\": \"matura\", \"original_font\"", "\"id\": \"verdana\", \"original_font\"", "slots[0]"},
        {"an original font named with its extension", "\"original_font\": \"Matura84_shadow\"", "\"original_font\": \"Matura84_shadow.fnt\"", "original_font"},
        {"a line height that is not whole", "\"line_height\": 84", "\"line_height\": 84.5", "line_height"},
        {"a baseline below the line", "\"base\": 54", "\"base\": 90", "base"},
        {"a cap height of nothing", "\"cap_height\": 40", "\"cap_height\": 0", "cap_height"},
        {"padding of three", "\"padding\": [2, 8, 8, 2]", "\"padding\": [2, 8, 8]", "padding"},
        {"padding below zero", "\"padding\": [2, 8, 8, 2]", "\"padding\": [2, 8, 8, -2]", "padding"},
        {"spacing that is a number", "\"spacing\": [2, 2]", "\"spacing\": 2", "spacing"},
        {"a page larger than the original's", "\"page_size\": 512", "\"page_size\": 1024", "page_size"},
        {"a page too small to hold a glyph", "\"page_size\": 512", "\"page_size\": 8", "page_size"},
        {"a shadow with an alpha above one", "\"alpha\": 0.99", "\"alpha\": 2", "shadow"},
        {"a shadow with a negative blur", "\"sigma\": 2.0", "\"sigma\": -1", "shadow"},
        {"a shadow with an offset of a hundred pixels", "\"dx\": 2.0", "\"dx\": 100", "shadow"},
        {"a shadow member that is not one", "\"alpha\": 0.99}", "\"alpha\": 0.99, \"glow\": 1}", "glow"},
        {"emboldening past one", "\"embolden\": 0.0", "\"embolden\": 2.0", "embolden"},
        {"a slot member that is not one", "\"embolden\": 0.0", "\"embolden\": 0.0, \"glow\": 1", "glow"},
        {"a text face nobody defined", "\"text_face\": \"latin\"", "\"text_face\": \"nobody\"", "text_face"},
        {"a script routed nowhere", "\"script_faces\": {\"japanese\": \"kana\"}", "\"script_faces\": {}", "script_faces"},
        {"a script routed to a face nobody defined", "\"script_faces\": {\"japanese\": \"kana\"}", "\"script_faces\": {\"japanese\": \"nobody\"}", "script_faces"},
        {"a route for a script nobody defined", "\"script_faces\": {\"japanese\": \"kana\"}", "\"script_faces\": {\"japanese\": \"kana\", \"arabic\": \"kana\"}", "script_faces"},
        {"a picker in a slot nobody defined", "\"picker_slot\": \"matura\"", "\"picker_slot\": \"nobody\"", "picker_slot"},
    };
    for (const Case& c : cases) {
        const std::string block = first(kTestFonts, c.from, c.to);
        CHECK_MSG(Refused(LanguagesJson(RightEntries(), block), why) && Contains(why, "fonts") && Contains(why, c.needle), std::string(c.what) + ": " + why);
    }
    CHECK_MSG(Refused(LanguagesJson(RightEntries(), ""), why) && Contains(why, "fonts"), "no fonts block at all: " + why);
    CHECK_MSG(Refused(LanguagesJson(RightEntries(), "3"), why) && Contains(why, "object"), "a fonts block that is a number: " + why);
    CHECK_MSG(Refused(LanguagesJson(RightEntries(), "[]"), why) && Contains(why, "object"), "or an array: " + why);
    {
        // Two scripts whose ranges meet: a code point has one script.
        std::string block = first(kTestFonts, "\"scripts\": [\n      {\"id\": \"japanese\", \"ranges\": [\"3000-30FF\", \"4E00-9FFF\"]}\n    ]",
                                  "\"scripts\": [{\"id\": \"japanese\", \"ranges\": [\"3000-30FF\"]}, {\"id\": \"other\", \"ranges\": [\"30A0-30AF\"]}]");
        block = first(block, "\"script_faces\": {\"japanese\": \"kana\"}", "\"script_faces\": {\"japanese\": \"kana\", \"other\": \"kana\"}");
        block = first(block, "\"script_faces\": {\"japanese\": \"kana\"}", "\"script_faces\": {\"japanese\": \"kana\", \"other\": \"kana\"}");
        CHECK_MSG(Refused(LanguagesJson(RightEntries(), block), why) && Contains(why, "overlaps"), "two scripts that share a code point: " + why);
        std::string fine = first(block, "\"30A0-30AF\"", "\"3100-31FF\"");
        const fs::path file = ScratchRoot() / "languages" / "two_scripts.json";
        WriteFile(file, LanguagesJson(RightEntries(), fine));
        Strings::LanguageTable table;
        CHECK_MSG(Strings::LoadLanguages(file.string(), table, why) && table.Fonts().scripts.size() == 2, "and with the ranges apart the same block loads: " + why);
    }
}

// ---- English, strictly -------------------------------------------------------------------------------------------------------------------

// The table over a data directory whose en.json is `english`; false with `why` when the load is refused.
bool EnglishLoads(const char* name, const std::string& english, std::string& why) {
    const fs::path dir = DataCopy(name);
    WriteFile(dir / "lang" / "en.json", english);
    Strings::Table table;
    const bool ok = table.Load(dir.string(), Lang::En, why);
    CHECK_MSG(ok == table.Loaded(), std::string("loaded exactly when the load succeeded: ") + name);
    if (!ok) CHECK_MSG(table.Get(Strings::Key::kHudPart) == Strings::Key::kHudPart, "a refused table shows key names, not stale text");
    return ok;
}

void EnglishIsReadStrictly() {
    const Map english = ReadStrings("en");
    std::string why;
    CHECK_MSG(EnglishLoads("en_same", StringsJson(english), why), "the real English, re-written, loads: " + why);
    CHECK_MSG(EnglishLoads("en_nonote", StringsJson(english, false), why), "and without its note: " + why);
    {
        Map m = english;
        m["_extra"] = "a note inside the strings";
        CHECK_MSG(EnglishLoads("en_note_inside", StringsJson(m), why), "a '_' member among the strings is a note: " + why);
    }
    for (const Strings::KeySpec& spec : Strings::kKeys) {
        Map m = english;
        m.erase(std::string(spec.key));
        CHECK_MSG(!EnglishLoads("en_missing", StringsJson(m), why) && Contains(why, "lacks the key '" + std::string(spec.key) + "'"),
                  "a missing key is refused by name: " + why);
    }
    {
        Map m = english;
        m["hud.partt"] = "Part {n}";
        CHECK_MSG(!EnglishLoads("en_extra", StringsJson(m), why) && Contains(why, "hud.partt"), "an extra key is refused by name: " + why);
    }
    // Placeholder mismatch.
    const std::string part(Strings::Key::kHudPart);
    {
        Map m = english;
        m[part] = "Part";
        CHECK_MSG(!EnglishLoads("en_ph_dropped", StringsJson(m), why) && Contains(why, part) && Contains(why, "placeholders"), "a placeholder dropped: " + why);
        m[part] = "Part {n} of {percent}";
        CHECK_MSG(!EnglishLoads("en_ph_added", StringsJson(m), why) && Contains(why, part), "a placeholder added: " + why);
        m[part] = "Part {n} {n}";
        CHECK_MSG(!EnglishLoads("en_ph_twice", StringsJson(m), why) && Contains(why, part), "a placeholder twice: " + why);
        m[part] = "Part {number}";
        CHECK_MSG(!EnglishLoads("en_ph_name", StringsJson(m), why) && Contains(why, part), "a placeholder misnamed: " + why);
        m[part] = "Part {n";
        CHECK_MSG(!EnglishLoads("en_ph_open", StringsJson(m), why) && Contains(why, "not a {placeholder}"), "an unfinished brace: " + why);
        m[part] = "Part n}";
        CHECK_MSG(!EnglishLoads("en_ph_close", StringsJson(m), why) && Contains(why, "not a {placeholder}"), "a lone closing brace: " + why);
        m[part] = "Part {}";
        CHECK_MSG(!EnglishLoads("en_ph_empty", StringsJson(m), why), "an empty placeholder: " + why);
        m = english;
        const std::string requires_(Strings::Key::kSelectorRequires);
        m[requires_] = "{percent}% {title}";
        CHECK_MSG(!EnglishLoads("en_ph_swapped", StringsJson(m), why) && Contains(why, requires_), "another key's placeholder in place of its own: " + why);
    }
    {
        Map m = english;
        m[part] = "";
        CHECK_MSG(!EnglishLoads("en_empty", StringsJson(m), why) && Contains(why, "non-empty"), "an empty value: " + why);
    }
    CHECK_MSG(!EnglishLoads("en_number", ReplaceOnce(StringsJson(english), "\"Part {n}\"", "7"), why) && Contains(why, "non-empty string"), "a number: " + why);
    CHECK_MSG(!EnglishLoads("en_nested", ReplaceOnce(StringsJson(english), "\"Part {n}\"", "{\"a\": \"b\"}"), why), "an object: " + why);
    CHECK_MSG(!EnglishLoads("en_member", "{\"strings\": {}, \"extra\": 1}", why) && Contains(why, "extra"), "an unknown member of the document: " + why);
    CHECK_MSG(!EnglishLoads("en_nostrings", "{\"_about\": []}", why) && Contains(why, "'strings'"), "no strings: " + why);
    CHECK_MSG(!EnglishLoads("en_strings_array", "{\"strings\": []}", why) && Contains(why, "'strings'"), "strings as an array: " + why);
    CHECK_MSG(!EnglishLoads("en_array", "[]", why) && Contains(why, "not an object"), "the document an array: " + why);
    CHECK_MSG(!EnglishLoads("en_broken", "{ nope", why), "not JSON: " + why);
    {
        Map m = english;
        m[part] = "Part " + B({0xE9});
        CHECK_MSG(!EnglishLoads("en_latin1", StringsJson(m), why) && Contains(why, "UTF-8"), "a Latin-1 byte, which is not UTF-8: " + why);
    }
    {
        const fs::path dir = DataCopy("en_gone");
        std::error_code ec;
        fs::remove(dir / "lang" / "en.json", ec);
        Strings::Table table;
        CHECK_MSG(!table.Load(dir.string(), Lang::De, why) && Contains(why, "cannot open"), "no en.json, whatever language is asked for: " + why);
        fs::remove(dir / "lang" / "languages.json", ec);
        CHECK_MSG(!table.Load(dir.string(), Lang::En, why) && Contains(why, "languages.json"), "no languages.json: " + why);
    }
    {
        // A refused load after a good one leaves nothing of the good one.
        Strings::Table table;
        CHECK_MSG(table.Load(kData, Lang::De, why), "the real German loads: " + why);
        const fs::path dir = DataCopy("en_after");
        WriteFile(dir / "lang" / "en.json", "{ nope");
        CHECK(!table.Load(dir.string(), Lang::De, why));
        CHECK_MSG(!table.Loaded() && table.Get(Strings::Key::kHudPart) == Strings::Key::kHudPart && table.FellBack().empty(),
                  "the table is empty again");
    }
}

// ---- parity ------------------------------------------------------------------------------------------------------------------------------

std::vector<std::string> Placeholders(const std::string& text) {
    bool wellFormed = true;
    std::vector<std::string> names = Strings::PlaceholdersOf(text, wellFormed);
    CHECK_MSG(wellFormed, "a well-formed string: " + text);
    return names;
}

std::size_t Breaks(const std::string& text) {
    return static_cast<std::size_t>(std::count(text.begin(), text.end(), '\n'));
}

void EveryLanguageHasExactlyEnglishsKeysAndPlaceholders() {
    const Strings::LanguageTable table = RealLanguages();
    const Map english = ReadStrings("en");
    const std::set<std::string> keys = AllKeys();
    CHECK_EQ(static_cast<int>(keys.size()), 14);
    {
        std::set<std::string> found;
        for (const auto& [key, value] : english) {
            (void)value;
            found.insert(key);
        }
        CHECK_MSG(found == keys, "en.json has exactly the fourteen keys of Strings::kKeys");
    }
    int translated = 0;
    for (const Lang lang : AllLangs()) {
        const std::string code = table.CodeOf(lang);
        const Map own = ReadStrings(code);
        std::set<std::string> found;
        for (const auto& [key, value] : own) {
            (void)value;
            found.insert(key);
        }
        for (const std::string& key : keys) CHECK_MSG(found.count(key) == 1, code + " lacks the key " + key);
        for (const std::string& key : found) CHECK_MSG(keys.count(key) == 1, code + " has the key " + key + ", which the port does not have");
        for (const std::string& key : keys) {
            if (own.count(key) == 0) continue;
            const std::string& text = own.at(key);
            CHECK_MSG(!text.empty(), code + "." + key + " is not empty");
            CHECK_MSG(Placeholders(text) == Placeholders(english.at(key)), code + "." + key + " carries English's placeholders: " + text);
            CHECK_MSG(Breaks(text) == Breaks(english.at(key)), code + "." + key + " has English's line breaks");
            if (lang != Lang::En && text != english.at(key)) ++translated;
        }
    }
    // Ten languages by fourteen keys, of which a few are the same word in several languages (Modern, a name): a file that was copied from
    // English would be a handful.
    CHECK_MSG(translated >= 122, "the languages are translated, not copied: " + std::to_string(translated) + " of 140 differ from English");

    // Each Placeholder::k name is what the key table says, and the table's placeholders are the ones English really carries.
    for (const Strings::KeySpec& spec : Strings::kKeys) {
        std::vector<std::string> expected;
        for (const std::string_view name : spec.placeholders) {
            if (!name.empty()) expected.emplace_back(name);
        }
        std::sort(expected.begin(), expected.end());
        CHECK_MSG(Placeholders(english.at(std::string(spec.key))) == expected, std::string(spec.key) + ": the key table names English's placeholders");
    }
    // Every key is in exactly one font list.
    std::set<std::string> listed;
    for (const std::string_view key : Strings::kMaturaKeys) CHECK_MSG(listed.insert(std::string(key)).second, "in the Matura list once: " + std::string(key));
    for (const std::string_view key : Strings::kVerdanaKeys) CHECK_MSG(listed.insert(std::string(key)).second, "in one list only: " + std::string(key));
    CHECK_MSG(listed == keys, "the Matura and Verdana lists together are every key");
}

void EveryLanguageLoads() {
    for (const Lang lang : AllLangs()) {
        Strings::Table table;
        std::string error;
        CHECK_MSG(table.Load(kData, lang, error), "the table loads: " + error);
        const std::string code = table.Languages().CodeOf(lang);
        CHECK(table.Loaded() && table.Language() == lang);
        CHECK_MSG(table.FellBack().empty(), code + " falls back on nothing");
        CHECK_MSG(table.Notes().empty(), code + " has nothing to report: " + (table.Notes().empty() ? "" : table.Notes().front()));
        const Map own = ReadStrings(code);
        for (const auto& [key, value] : own) CHECK_MSG(table.Get(key) == value, code + ".Get(" + key + ") is the file's string");
        CHECK_MSG(table.Get("no.such.key") == "no.such.key", "a key nobody defined shows as itself");
    }
}

// ---- fallback --------------------------------------------------------------------------------------------------------------------------

void AMissingOrBadStringShowsEnglishOnce() {
    const Map english = ReadStrings("en");
    const Map german = ReadStrings("de");
    const std::string camera(Strings::Key::kHintCamera);
    const std::string part(Strings::Key::kHudPart);
    std::string why;

    {
        const fs::path dir = DataCopy("fb_missing");
        Map m = german;
        m.erase(camera);
        WriteFile(dir / "lang" / "de.json", StringsJson(m));
        Strings::Table table;
        CHECK_MSG(table.Load(dir.string(), Lang::De, why), "a language file that lacks a key still loads: " + why);
        CHECK_MSG(table.Get(camera) == english.at(camera), "the missing key is English's");
        CHECK_MSG(table.Get(part) == german.at(part), "the others are German's");
        CHECK_MSG(table.FellBack() == std::vector<std::string>{camera}, "FellBack names exactly it");
        CHECK_MSG(table.Notes().size() == 1 && Contains(table.Notes()[0], camera) && Contains(table.Notes()[0], "English"), "and Notes says so once");
        for (int i = 0; i < 5; ++i) table.Get(camera);
        CHECK_MSG(table.Notes().size() == 1, "reading it again adds nothing: Get never logs");
    }
    {
        const fs::path dir = DataCopy("fb_placeholder");
        Map m = german;
        m[part] = "Level";
        m[std::string(Strings::Key::kCardUnlocked)] = "Neuer Erfolg! {n}";
        WriteFile(dir / "lang" / "de.json", StringsJson(m));
        Strings::Table table;
        CHECK_MSG(table.Load(dir.string(), Lang::De, why), "placeholders that are not English's still load: " + why);
        CHECK_MSG(table.Get(part) == english.at(part), "a placeholder dropped shows English's string");
        CHECK_MSG(table.Get(std::string(Strings::Key::kCardUnlocked)) == english.at(std::string(Strings::Key::kCardUnlocked)), "and so does a misnamed one");
        CHECK_MSG(table.FellBack().size() == 2 && table.Notes().size() == 2, "two keys, two notes");
        CHECK_MSG(table.Format(Strings::Key::kHudPart, {{Strings::Placeholder::kN, "5"}}) == "Part 5", "and Format fills English's");
    }
    {
        const fs::path dir = DataCopy("fb_extra");
        Map m = german;
        m["bogus.key"] = "Quatsch";
        WriteFile(dir / "lang" / "de.json", StringsJson(m));
        Strings::Table table;
        CHECK_MSG(table.Load(dir.string(), Lang::De, why), "a file with a key the port lacks loads: " + why);
        CHECK_MSG(table.FellBack().empty() && table.Notes().size() == 1 && Contains(table.Notes()[0], "bogus.key"), "the key is noted and ignored");
        CHECK_MSG(table.Get("bogus.key") == "bogus.key", "it is not a string of the table");
    }
    // Files that cannot be used at all: English throughout, one note.
    const std::vector<std::pair<std::string, std::string>> broken = {
        {"fb_json", "{ nope"},
        {"fb_array", "[]"},
        {"fb_nostrings", "{\"_about\": []}"},
        {"fb_empty_value", StringsJson([&] { Map m = german; m[part] = ""; return m; }())},
        {"fb_latin1", StringsJson([&] { Map m = german; m[part] = "Ebene " + B({0xE9}); return m; }())},
        {"fb_number", ReplaceOnce(StringsJson(german), "\"" + german.at(part) + "\"", "5")},
    };
    for (const auto& [name, text] : broken) {
        const fs::path dir = DataCopy(name.c_str());
        WriteFile(dir / "lang" / "de.json", text);
        Strings::Table table;
        CHECK_MSG(table.Load(dir.string(), Lang::De, why), name + ": an unusable language file still loads: " + why);
        CHECK_MSG(table.FellBack().size() == Strings::kKeys.size() && table.Notes().size() == 1, name + ": every key falls back, with one note");
        bool allEnglish = true;
        for (const auto& [key, value] : english) allEnglish = allEnglish && table.Get(key) == value;
        CHECK_MSG(allEnglish, name + ": and every string is English's");
    }
    {
        const fs::path dir = DataCopy("fb_gone");
        std::error_code ec;
        fs::remove(dir / "lang" / "de.json", ec);
        Strings::Table table;
        CHECK_MSG(table.Load(dir.string(), Lang::De, why), "no de.json still loads: " + why);
        CHECK_MSG(table.FellBack().size() == Strings::kKeys.size() && table.Notes().size() == 1 && Contains(table.Notes()[0], "cannot open"), "all English, one note");
    }
    {
        // English asked for needs no second file and says nothing.
        const fs::path dir = DataCopy("fb_english");
        std::error_code ec;
        for (const auto& item : fs::directory_iterator(dir / "lang")) {
            if (item.path().filename() != "en.json" && item.path().filename() != "languages.json") fs::remove(item.path(), ec);
        }
        Strings::Table table;
        CHECK_MSG(table.Load(dir.string(), Lang::En, why) && table.Notes().empty() && table.FellBack().empty(), "English alone: " + why);
        // And a table is reusable: German over it, then English again.
        CHECK_MSG(table.Load(kData, Lang::De, why) && table.Get(part) == german.at(part), "German loads into the same table");
        CHECK_MSG(table.Load(kData, Lang::En, why) && table.Get(part) == english.at(part) && table.Language() == Lang::En, "and English again, with nothing of German left");
    }
}

// ---- Format ----------------------------------------------------------------------------------------------------------------------------

void FillIsOneLeftToRightScan() {
    CHECK_MSG(Strings::Fill("Part {n}", {{"n", "5"}}) == "Part 5", "one placeholder");
    CHECK_MSG(Strings::Fill("Part", {{"n", "5"}}) == "Part", "none in the text");
    CHECK_MSG(Strings::Fill("", {{"n", "5"}}) == "", "an empty text");
    CHECK_MSG(Strings::Fill("{n}", {{"n", "5"}}) == "5", "the whole text");
    CHECK_MSG(Strings::Fill("{n}{n}", {{"n", "5"}}) == "55", "side by side");
    CHECK_MSG(Strings::Fill("{n} of {n}", {{"n", "5"}}) == "5 of 5", "an occurrence each, not only the first");
    CHECK_MSG(Strings::Fill("level {world}-{level}", {{"world", "4"}, {"level", "17"}}) == "level 4-17", "two names");
    CHECK_MSG(Strings::Fill("level {level}-{world}", {{"world", "4"}, {"level", "17"}}) == "level 17-4", "in the text's order, not the values'");
    CHECK_MSG(Strings::Fill("a\nlevel {world}", {{"world", "4"}}) == "a\nlevel 4", "a line break is kept");
    // An unknown placeholder is left where a reader will see it.
    CHECK_MSG(Strings::Fill("{n} {title}", {{"n", "5"}}) == "5 {title}", "no value for {title}");
    CHECK_MSG(Strings::Fill("{n}", {}) == "{n}", "no values at all");
    // A value is never scanned again.
    CHECK_MSG(Strings::Fill("{title} / {n}", {{"title", "{n}"}, {"n", "5"}}) == "{n} / 5", "a value that holds a placeholder keeps it");
    CHECK_MSG(Strings::Fill("{title}", {{"title", "{title}"}}) == "{title}", "and one that holds itself does not loop");
    CHECK_MSG(Strings::Fill("{n}", {{"n", "{"}}) == "{", "a value that is a brace");
    // A value for a name the text does not have changes nothing.
    CHECK_MSG(Strings::Fill("Part {n}", {{"n", "5"}, {"percent", "60"}}) == "Part 5", "an unused value");
    // Not placeholders: left as written.
    CHECK_MSG(Strings::Fill("{", {{"n", "5"}}) == "{", "a lone opening brace");
    CHECK_MSG(Strings::Fill("}", {{"n", "5"}}) == "}", "a lone closing brace");
    CHECK_MSG(Strings::Fill("{n", {{"n", "5"}}) == "{n", "unfinished");
    CHECK_MSG(Strings::Fill("n}", {{"n", "5"}}) == "n}", "unstarted");
    CHECK_MSG(Strings::Fill("{}", {{"n", "5"}}) == "{}", "empty");
    CHECK_MSG(Strings::Fill("{N}", {{"n", "5"}}) == "{N}", "a name is lower case");
    CHECK_MSG(Strings::Fill("{n n}", {{"n", "5"}}) == "{n n}", "and has no space");
    CHECK_MSG(Strings::Fill("{{n}}", {{"n", "5"}}) == "{5}", "braces around a placeholder stay");
    CHECK_MSG(Strings::Fill("{n}%", {{"n", "5"}}) == "5%", "a percent sign after");
    CHECK_MSG(Strings::Fill("100%{n}", {{"n", "5"}}) == "100%5", "and before");
    // The first value for a name wins.
    CHECK_MSG(Strings::Fill("{n}", {{"n", "a"}, {"n", "b"}}) == "a", "a name given twice");
    // UTF-8 in the text and the value goes through untouched.
    {
        const std::string text = B({0xD0, 0x96}) + " {n} " + B({0xE3, 0x81, 0x82});
        CHECK_MSG(Strings::Fill(text, {{"n", B({0xD8, 0xA7})}}) == B({0xD0, 0x96}) + " " + B({0xD8, 0xA7}) + " " + B({0xE3, 0x81, 0x82}), "multi-byte text around a placeholder");
    }

    // PlaceholdersOf: sorted, repeated names repeated, and the well-formedness flag.
    bool ok = true;
    CHECK_MSG(Strings::PlaceholdersOf("level {world}-{level}", ok) == (std::vector<std::string>{"level", "world"}) && ok, "sorted names");
    CHECK_MSG(Strings::PlaceholdersOf("{n} {n}", ok) == (std::vector<std::string>{"n", "n"}) && ok, "a repeated name repeated");
    CHECK_MSG(Strings::PlaceholdersOf("no braces", ok).empty() && ok, "none");
    Strings::PlaceholdersOf("{n", ok);
    CHECK_MSG(!ok, "an unfinished brace is not well formed");
    Strings::PlaceholdersOf("n}", ok);
    CHECK_MSG(!ok, "nor a lone closing one");
    Strings::PlaceholdersOf("{}", ok);
    CHECK_MSG(!ok, "nor an empty one");
    Strings::PlaceholdersOf("{N}", ok);
    CHECK_MSG(!ok, "nor an upper-case name");
}

void FormatFillsTheTablesString() {
    Strings::Table english;
    std::string why;
    CHECK_MSG(english.Load(kData, Lang::En, why), why);
    CHECK_MSG(english.Format(Strings::Key::kHudPart, {{Strings::Placeholder::kN, "5"}}) == "Part 5", "Part 5");
    CHECK_MSG(english.Format(Strings::Key::kDashboardChapter, {{Strings::Placeholder::kN, "3"}}) == "Chapter 3", "Chapter 3");
    CHECK_MSG(english.Format(Strings::Key::kSelectorRequires, {{Strings::Placeholder::kPercent, "60"}}) == "requires 60%", "requires 60%");
    CHECK_MSG(english.Format(Strings::Key::kCardUnlocked, {{Strings::Placeholder::kTitle, "Invented title, number II"}}) ==
                  "Achievement unlocked! Invented title, number II",
              "the card");
    CHECK_MSG(english.Format(Strings::Key::kHudPart, {}) == "Part {n}", "no value: the placeholder stays, visibly");
    CHECK_MSG(english.Format(Strings::Key::kDashboardNew, {{Strings::Placeholder::kN, "5"}}) == "new!", "a string without placeholders is as it is");
    CHECK_MSG(english.Format("no.such.key", {{Strings::Placeholder::kN, "5"}}) == "no.such.key", "an unknown key is its own name");

    // A title that holds a placeholder is not expanded into the card.
    CHECK_MSG(english.Format(Strings::Key::kCardUnlocked, {{Strings::Placeholder::kTitle, "{n}"}}) == "Achievement unlocked! {n}", "a title of {n}");

    // The same keys in German fill the same way.
    Strings::Table german;
    CHECK_MSG(german.Load(kData, Lang::De, why), why);
    const std::string part = german.Get(Strings::Key::kHudPart);
    const std::string filled = german.Format(Strings::Key::kHudPart, {{Strings::Placeholder::kN, "5"}});
    CHECK_MSG(filled == Strings::Fill(part, {{"n", "5"}}) && Contains(filled, "5") && !Contains(filled, "{"), "German fills {n}: " + filled);
}

// ---- display ----------------------------------------------------------------------------------------------------------------------------

void DisplayIsTheIdentityExceptForArabic() {
    // The one choke point (planning step 237): nothing may change a string on its way to the screen except Arabic's shaping, which
    // tests/test_mp_arabic.cpp pins case by case. Every other language is the identity BYTE FOR BYTE - a malformed byte too.
    const std::string arabicWord = B({0xD8, 0xA7, 0xD9, 0x84, 0xD8, 0xB9, 0xD8, 0xB1, 0xD8, 0xA8, 0xD9, 0x8A, 0xD8, 0xA9});
    const std::vector<std::string> samples = {
        "", "Part 5", "line one\nline two", B({0xD0, 0x96, 0xD0, 0xB5}), B({0xE3, 0x81, 0x82, 0xE3, 0x81, 0x84}), arabicWord + " 12", B({0xC3, 0xA9}),
        B({0xFF, 0x41}), // not even valid UTF-8 is touched here
    };
    for (const Lang lang : AllLangs()) {
        if (lang == Lang::Ar) continue;
        for (const std::string& sample : samples) CHECK_MSG(Strings::Display(lang, sample) == sample, "Display is the identity for " + std::to_string(Strings::IndexOf(lang)));
    }
    // Arabic: a text with no Arabic letter in it reads as it was (a Latin, Cyrillic or kana line in a right-to-left paragraph is ordered as
    // what it is), and a text with Arabic in it is shaped: no longer the letters it was written in.
    for (const std::size_t at : {0u, 1u, 2u, 3u, 4u, 6u}) CHECK_MSG(Strings::Display(Lang::Ar, samples[at]) == samples[at], "Display(Ar) leaves a text with no Arabic as it is: " + std::to_string(at));
    const std::string shaped = Strings::Display(Lang::Ar, arabicWord + " 12");
    CHECK_MSG(shaped != arabicWord + " 12" && Utf8::IsValid(shaped), "Display(Ar) shapes Arabic");
    CHECK_MSG(Strings::Display(Lang::Ar, samples[7]) == B({'A', 0xEF, 0xBF, 0xBD}), "and a malformed byte becomes U+FFFD there (ordered as the neutral it is, after the Latin letter in a right-to-left line), where the layout would have reported it anyway");
}

// ---- drift ------------------------------------------------------------------------------------------------------------------------------

void EnglishIsTheEnglishTheRulesFieldsStillHold() {
    Strings::Table en;
    std::string why;
    CHECK_MSG(en.Load(kData, Lang::En, why), why);
    const std::string ui = kData + "/ui.json";
    const auto n = [](int value) { return std::string(std::to_string(value)); };

    // hud.part <-> the level caption (ui.json level_start.caption.prefix + the number) and the pause title (pause.title.prefix + the number).
    Hud::Rules hud;
    CHECK_MSG(Hud::LoadRules(ui, hud, why), "ui.json's hud reads: " + why);
    for (int index = 0; index < 32; ++index) {
        CHECK_MSG(Hud::CaptionText(hud, index) == en.Format(Strings::Key::kHudPart, {{Strings::Placeholder::kN, n(index + 1)}}),
                  "the caption of level " + n(index + 1) + " is hud.part");
    }
    CHECK_MSG(Hud::CaptionText(hud, 4) == "Part 5", "and \"Part 5\" is what it has always been");
    Pause::Rules pause;
    CHECK_MSG(Pause::LoadRules(ui, pause, why), "ui.json's pause reads: " + why);
    for (int index = 0; index < 32; ++index) {
        Pause::Level level;
        level.index = index;
        CHECK_MSG(Pause::TitleText(pause, level) == en.Format(Strings::Key::kHudPart, {{Strings::Placeholder::kN, n(index + 1)}}),
                  "the pause title of level " + n(index + 1) + " is hud.part");
    }

    // dashboard.chapter, dashboard.new.
    Dashboard::Rules dashboard;
    CHECK_MSG(Dashboard::LoadRules(ui, dashboard, why), "ui.json's dashboard reads: " + why);
    for (int chapter = 1; chapter <= 4; ++chapter) {
        CHECK_MSG(dashboard.headerPrefix + n(chapter) == en.Format(Strings::Key::kDashboardChapter, {{Strings::Placeholder::kN, n(chapter)}}),
                  "the header of chapter " + n(chapter));
    }
    CHECK_MSG(dashboard.newLabel.words == en.Get(Strings::Key::kDashboardNew), "the new label");

    // selector.requires, with locking's percent as {percent}.
    Selector::Rules selector;
    CHECK_MSG(Selector::LoadRules(ui, selector, why), "ui.json's selector reads: " + why);
    Locking::Rules locking;
    CHECK_MSG(Locking::LoadRules(ui, locking, why), "ui.json's locking reads: " + why);
    CHECK_MSG(selector.chapters.warningWords == en.Format(Strings::Key::kSelectorRequires, {{Strings::Placeholder::kPercent, n(locking.chapterUnlockPercent)}}),
              "the chapter selector's warning: " + selector.chapters.warningWords);
    CHECK_MSG(locking.chapterUnlockPercent == 60, "from locking's 60");

    // credits.enhanced, credits.engine: two of the tail's six lines.
    Credits::Rules credits;
    CHECK_MSG(Credits::LoadRules(ui, credits, why), "ui.json's credits read: " + why);
    int enhanced = 0;
    int engine = 0;
    for (const Credits::Rules::Tail::Line& line : credits.tail.lines) {
        enhanced += line.words == en.Get(Strings::Key::kCreditsEnhanced) ? 1 : 0;
        engine += line.words == en.Get(Strings::Key::kCreditsEngine) ? 1 : 0;
    }
    CHECK_MSG(enhanced == 1 && engine == 1, "the credits tail holds credits.enhanced and credits.engine once each");

    // options.*: controls.json's options block.
    Controls::Rules controls;
    Options::Rules options;
    CHECK_MSG(Controls::LoadRules(kData + "/controls.json", controls, why) && Options::LoadRules(kData + "/controls.json", controls, options, why),
              "controls.json's options read: " + why);
    CHECK_MSG(options.title.words == en.Get(Strings::Key::kOptionsTitle), "the options title: " + options.title.words);
    const std::set<std::string> tiles = {options.tiles[0].word, options.tiles[1].word};
    CHECK_MSG(tiles == (std::set<std::string>{en.Get(Strings::Key::kOptionsModern), en.Get(Strings::Key::kOptionsClassic)}), "the two tiles' words");
    CHECK_MSG(options.adjust.word == en.Get(Strings::Key::kOptionsAdjust), "the adjust button: " + options.adjust.word);

    // hint.camera: camera.json's hint.
    Hint::Rules hint;
    CHECK_MSG(Hint::LoadRules(kData + "/camera.json", hint, why), "camera.json's hint reads: " + why);
    CHECK_MSG(hint.text == en.Get(Strings::Key::kHintCamera), "the camera hint: " + hint.text);

    // card.unlocked, card.locked_level: achievements.json's two cards.
    Achievements::Rules achievements;
    CHECK_MSG(Achievements::LoadRules(kData + "/achievements.json", achievements, why), "achievements.json reads: " + why);
    for (const std::string title : {"Invented level 1-1", "Invented title, number II", "x"}) {
        CHECK_MSG(achievements.card.messagePrefix + title == en.Format(Strings::Key::kCardUnlocked, {{Strings::Placeholder::kTitle, title}}),
                  "the unlock card of \"" + title + "\"");
    }
    CHECK_MSG(achievements.locked.message == en.Get(Strings::Key::kCardLockedLevel), "the locked-level card: " + achievements.locked.message);

    // The secret achievement's words are Asantee's and are in no committed table: no key of Strings::kKeys is about it, and no language file has one.
    for (const Strings::KeySpec& spec : Strings::kKeys) CHECK_MSG(std::string(spec.key).find("secret") == std::string::npos, "no key is the secret achievement's: " + std::string(spec.key));
}

// ---- the character set ------------------------------------------------------------------------------------------------------------------

// The typographic characters translators reach for and the original's fonts do not hold: curly quotes, low quotes, guillemets, the
// ellipsis, en and em dashes, the oe ligatures, Y with a diaeresis, the euro sign.
constexpr std::uint32_t kAbsentFromTheOriginalFonts[] = {0x2018, 0x2019, 0x201A, 0x201B, 0x201C, 0x201D, 0x201E, 0x2026, 0x2013, 0x2014, 0x2012, 0x2015,
                                                          0x0152, 0x0153, 0x0178, 0x20AC, 0x2039, 0x203A, 0x2022, 0x2122};

bool IsMatura(const std::string& key) {
    for (const std::string_view matura : Strings::kMaturaKeys) {
        if (matura == key) return true;
    }
    return false;
}

void OriginalFontLanguagesStayInsideTheOriginalFontsSet() {
    const Strings::LanguageTable table = RealLanguages();
    // The constant is the files': 32-126 and 160-255.
    CHECK(Strings::InOriginalFonts(0x20) && Strings::InOriginalFonts(0x7E) && Strings::InOriginalFonts(0xA0) && Strings::InOriginalFonts(0xFF));
    CHECK(!Strings::InOriginalFonts(0x1F) && !Strings::InOriginalFonts(0x7F) && !Strings::InOriginalFonts(0x9F) && !Strings::InOriginalFonts(0x100));
    for (const std::uint32_t point : kAbsentFromTheOriginalFonts) CHECK_MSG(!Strings::InOriginalFonts(point), "the set holds no " + Show({point}));

    int checkedStrings = 0;
    int latin1Letters = 0;
    for (const Lang lang : AllLangs()) {
        if (table.FontSetOf(lang) != Strings::FontSet::Original) continue;
        const std::string code = table.CodeOf(lang);
        const Map own = ReadStrings(code);
        for (const auto& [key, text] : own) {
            ++checkedStrings;
            // A newline is the layout's, not a glyph.
            for (const std::uint32_t point : Cps(text)) {
                if (point == '\n') continue;
                CHECK_MSG(Strings::InOriginalFonts(point), code + "." + key + " has " + Show({point}) + "which the original's fonts do not hold: " + text);
                latin1Letters += point >= 0xA0 ? 1 : 0;
                for (const std::uint32_t typographic : kAbsentFromTheOriginalFonts) CHECK_MSG(point != typographic, code + "." + key + " has a typographic character");
                // Matura draws a capital I like a J.
                if (IsMatura(key)) CHECK_MSG(point != 'I', code + "." + key + " (a Matura string) has a capital I: " + text);
            }
        }
        // The picker draws the native name, and a language set in the original's fonts names itself in the same set. The picker's names are not drawn in
        // the original's fonts: one generated atlas holds all eleven (MagicPortalsLayer::ensurePicker, in the slot languages.json calls picker_slot), so
        // Italiano's capital I, which the original's Matura would draw like a J, is no concern of the picker's. This pins only the name's character set.
        for (const std::uint32_t point : Cps(table.NativeName(lang))) {
            CHECK_MSG(Strings::InOriginalFonts(point), code + "'s native name is in the original fonts' set");
        }
    }
    CHECK_EQ(checkedStrings, static_cast<int>(6 * Strings::kKeys.size()));
    CHECK_MSG(latin1Letters > 20, "the accented letters of de es fr it pt were seen: " + std::to_string(latin1Letters));

    // The one-line proof that the test would notice a capital I: Matura-font English holds none, and a word with one is found.
    CHECK_MSG(IsMatura(std::string(Strings::Key::kHudPart)) && !IsMatura(std::string(Strings::Key::kDashboardNew)), "the Matura list is the Matura slot's");
}

// The same constants against the original's own font files, where the extracted assets are present.
void TheConstantIsTheOriginalFontsRealSet() {
    const std::string original = MAGICPORTALS_ORIGINAL_DIR;
    const std::vector<std::pair<std::string, bool>> fonts = {{"Matura84_shadow.fnt", true}, {"Verdana40_shadow.fnt", false}};
    const Strings::LanguageTable table = RealLanguages();
    for (const auto& [name, isMatura] : fonts) {
        const std::string path = original + "/data/" + name;
        Supersonic::BitmapFont font;
        std::string why;
        if (original.empty() || !fs::exists(path) || !font.Load(path, why)) {
            std::printf("  test_mp_strings: the original's %s was NOT read - needs the extracted assets at %s\n", name.c_str(), original.c_str());
            continue;
        }
        // The set is exactly Strings::InOriginalFonts: nothing below 0x100 is held that it denies and nothing it allows is missing.
        int held = 0;
        for (std::uint32_t point = 0; point < 0x300; ++point) {
            const bool has = font.Find(point) != nullptr;
            held += has ? 1 : 0;
            CHECK_MSG(has == Strings::InOriginalFonts(point), name + ": " + Show({point}) + (has ? "is held" : "is not held") + ", the constant says the opposite");
        }
        CHECK_EQ(held, 191);
        for (const Lang lang : AllLangs()) {
            if (table.FontSetOf(lang) != Strings::FontSet::Original) continue;
            const Map own = ReadStrings(table.CodeOf(lang));
            for (const auto& [key, text] : own) {
                if (IsMatura(key) != isMatura) continue;
                for (const std::uint32_t point : Cps(text)) {
                    if (point != '\n') CHECK_MSG(font.Find(point) != nullptr, table.CodeOf(lang) + "." + key + " has " + Show({point}) + "which " + name + " lacks");
                }
            }
        }
    }
}

void EveryLanguageIsValidUtf8WithNoReplacementCharacter() {
    const Strings::LanguageTable table = RealLanguages();
    int points = 0;
    int beyondLatin1 = 0;
    for (const Lang lang : AllLangs()) {
        const std::string code = table.CodeOf(lang);
        std::vector<std::pair<std::string, std::string>> texts;
        for (const auto& [key, text] : ReadStrings(code)) texts.emplace_back(code + "." + key, text);
        texts.emplace_back(code + ".name_native", table.NativeName(lang));
        for (const auto& [where, text] : texts) {
            std::size_t replaced = 0;
            const CodePoints decoded = Utf8::Decode(text, replaced);
            CHECK_MSG(replaced == 0, where + " is valid UTF-8");
            for (const std::uint32_t point : decoded) {
                ++points;
                beyondLatin1 += point > 0xFF ? 1 : 0;
                CHECK_MSG(point != Utf8::kReplacement, where + " holds no U+FFFD");
                CHECK_MSG(point == '\n' || point >= 0x20, where + " holds no control character other than a line break");
                CHECK_MSG(point < 0xD800 || point > 0xDFFF, where + " holds no surrogate");
            }
            CHECK_MSG(text.find("\r") == std::string::npos, where + " has no carriage return");
            CHECK_MSG(text.find("  ") == std::string::npos && (text.empty() || (text.front() != ' ' && text.back() != ' ')), where + " has no doubled or edge spaces: \"" + text + "\"");
        }
    }
    // The other scripts really are in the files (a test that read nothing would pass).
    CHECK_MSG(beyondLatin1 > 150, "ru tr uk ja ar hold characters beyond Latin-1: " + std::to_string(beyondLatin1) + " of " + std::to_string(points));
    // Specific marks of each script, by code point.
    const auto has = [&](const char* code, auto inRange) {
        for (const auto& [key, text] : ReadStrings(code)) {
            for (const std::uint32_t point : Cps(text)) {
                if (inRange(point)) return true;
            }
        }
        return false;
    };
    CHECK_MSG(has("ru", [](std::uint32_t p) { return p >= 0x410 && p <= 0x44F; }), "ru holds Cyrillic");
    CHECK_MSG(has("uk", [](std::uint32_t p) { return p >= 0x410 && p <= 0x44F; }), "uk holds Cyrillic");
    CHECK_MSG(has("ja", [](std::uint32_t p) { return (p >= 0x3040 && p <= 0x30FF) || (p >= 0x4E00 && p <= 0x9FFF); }), "ja holds kana or kanji");
    CHECK_MSG(has("ar", [](std::uint32_t p) { return p >= 0x600 && p <= 0x6FF; }), "ar holds Arabic");
    CHECK_MSG(has("tr", [](std::uint32_t p) { return p == 0x11F || p == 0x131 || p == 0x15F || p == 0x130 || p == 0x11E || p == 0x15E; }),
              "tr holds a Turkish letter the original's fonts lack");
    // Arabic carries logical-order text with no presentation forms and no marks: the shaper's job and the font's, not the file's.
    for (const auto& [key, text] : ReadStrings("ar")) {
        for (const std::uint32_t point : Cps(text)) {
            CHECK_MSG(!(point >= 0xFB50 && point <= 0xFEFF), "ar." + key + " is in logical order, not pre-shaped: " + Show({point}));
            CHECK_MSG(!(point >= 0x64B && point <= 0x65F), "ar." + key + " has no harakat: " + Show({point}));
        }
    }
}

// ---- the files themselves --------------------------------------------------------------------------------------------------------------

void TheFilesAreCleanAndSayWhereTheyCameFrom() {
    int files = 0;
    for (const auto& item : fs::directory_iterator(kLangDir)) {
        if (item.path().extension() != Strings::kFileExtension) continue;
        ++files;
        const std::string name = item.path().filename().string();
        const std::string text = ReadAll(item.path());
        CHECK_MSG(text.size() > 2 && text.compare(0, 3, B({0xEF, 0xBB, 0xBF})) != 0, name + ": UTF-8 without a BOM");
        CHECK_MSG(text.find('\r') == std::string::npos, name + ": LF line ends");
        CHECK_MSG(Utf8::IsValid(text), name + ": valid UTF-8 throughout");
        CHECK_MSG(text.find("\\u") == std::string::npos, name + ": real characters, no \\u escape");
        CHECK_MSG(text.find(".png\"") == std::string::npos && text.find(".fnt\"") == std::string::npos && text.find(".bmp\"") == std::string::npos,
                  name + ": names no picture or font (tools/package/closure.py would resolve it as an original asset)");
        if (name == "languages.json") continue;
        CHECK_MSG(Contains(text, "\"_about\""), name + ": has its _about");
        if (name == "en.json") continue;
        CHECK_MSG(Contains(text, "machine-translated by a language model from English") && Contains(text, "NOT by a native speaker") &&
                      Contains(text, "terms aligned with the Magic Rampage Companion app") && Contains(text, "reviewed by a second model pass"),
                  name + ": says it is a machine translation, reviewed by a second model and by no native speaker");
    }
    CHECK_EQ(files, 12);
}

void runTests() {
    TheLanguageTableIsTheEnumsOrder();
    TheLoaderRefusesATableThatIsNotTheEnums();
    TheLoaderRefusesABadFontsBlock();
    EnglishIsReadStrictly();
    EveryLanguageHasExactlyEnglishsKeysAndPlaceholders();
    EveryLanguageLoads();
    AMissingOrBadStringShowsEnglishOnce();
    FillIsOneLeftToRightScan();
    FormatFillsTheTablesString();
    DisplayIsTheIdentityExceptForArabic();
    EnglishIsTheEnglishTheRulesFieldsStillHold();
    OriginalFontLanguagesStayInsideTheOriginalFontsSet();
    TheConstantIsTheOriginalFontsRealSet();
    EveryLanguageIsValidUtf8WithNoReplacementCharacter();
    TheFilesAreCleanAndSayWhereTheyCameFrom();
}

} // namespace

MP_TEST_MAIN("test_mp_strings", 2500)
