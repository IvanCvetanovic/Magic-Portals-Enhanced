// The modules that speak a language (planning step 238, PORT-34), without a layer or a window: what each takes from the string table, and the
// fit to a slot's width.
//
//   fit         Hud::FitUnitsPerFontPx over a font baked at run time (sim/FontBake, so no original asset is needed): no limit, or a text that fits, is
//               returned bit for bit; a wider one is scaled so that it is exactly the limit wide; a height limit does the same for lines; a font that
//               is not loaded leaves the size alone
//   table       Strings::Table::Show is Format and then Display, once (English and Russian: Format, byte for byte; Arabic: shaped), Shape is Display,
//               Rtl is languages.json's flag
//   hud         CaptionText and Pause::TitleText take the table's hud.part, filled, and without a table are rules.prefix + the number
//   dashboard   Pieces' header and "new!" are the table's (and the rules' own without one), and every text piece carries its slot's limit
//   selector    BuildChapters' warning is the table's requires with locking.json's percent (and empty without a table, so the rules' words draw)
//   credits     TailWords: the two sentences that name a text_key are the table's, the names and the address are never; every line has its limit
//   options     Pieces' title, two tile words and adjust word are the table's; the limits are the controls.json ones
//   hint        TextTopLeft: left-anchored; in a right-to-left language on a rule that mirrors, anchored at the right edge the same distance in
//   rules       the optional max_width_units / mirror_when_rtl members: absent is no limit, a number above zero is read, anything else refuses the
//               load and names the member (a mutated copy of the tables in a scratch folder)
//   achievements  LoadTranslation reads format 1 and refuses a missing file, a wrong format, another language, a key that is no id, an empty
//               string, and a `secret` block that is missing, empty or whose sentence does not hold {world} and {level} once each; Localized puts a
//               language's words over the English by id (an id it lacks keeps its English and is listed), the secret achievement's words are the
//               overlay's `secret` block with {world}-{level} filled (they are in no committed table), and without a language the list is the English one
//
// Every text a case writes is synthetic: no Asantee word is in this file. No non-ASCII character is written either (MSVC is built without /utf-8
// here): the text a case needs in another script is a \u escape.

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "core/BitmapFont.hpp"
#include "sim/Achievements.hpp"
#include "sim/Chapters.hpp"
#include "sim/Controls.hpp"
#include "sim/Credits.hpp"
#include "sim/Dashboard.hpp"
#include "sim/FontBake.hpp"
#include "sim/Hint.hpp"
#include "sim/Hud.hpp"
#include "sim/Locking.hpp"
#include "sim/Options.hpp"
#include "sim/Pause.hpp"
#include "sim/Scores.hpp"
#include "sim/Selector.hpp"
#include "sim/Strings.hpp"
#include "sim/Utf8.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

using namespace MagicPortals;
using Strings::Lang;

namespace {

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
    std::printf("test_mp_langsim: MUTATION - MP_TEST_PORTDATA is set, the port's data is read from %s, not from game/data\n", text.c_str());
    return text;
}

const std::string kData = PortDataDir();

bool Near(double a, double b, double eps = 1e-9) {
    return std::fabs(a - b) <= eps;
}

std::filesystem::path Scratch(const std::string& name) {
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / ("supersonic-test-mp-langsim-" + name);
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);
    return dir;
}

void WriteText(const std::filesystem::path& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary);
    out << text;
}

std::string ReadText(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

Strings::Table Load(Lang lang) {
    Strings::Table table;
    std::string error;
    CHECK_MSG(table.Load(kData, lang, error), "the table loads: " + error);
    return table;
}

// A font baked from the committed faces for `slotId`, over the printable ASCII plus `extra`; the engine's own reader loads it.
struct BakedFont {
    Supersonic::BitmapFont font;
    bool ok = false;
};

BakedFont Bake(const std::string& folder, const std::string& slotId, const std::string& name, const std::set<std::uint32_t>& extra) {
    BakedFont baked;
    const Strings::Table table = Load(Lang::Ru);
    const FontBake::Plan& plan = table.Languages().Fonts();
    const FontBake::SlotSpec* slot = plan.Slot(slotId);
    CHECK_MSG(slot != nullptr, "the plan has the slot " + slotId);
    if (slot == nullptr) return baked;
    std::vector<FontBake::FaceBytes> faces;
    std::string error;
    CHECK_MSG(FontBake::LoadFaces(plan, kData + "/" + plan.folder, faces, error), error);
    const std::filesystem::path dir = Scratch(folder);
    const FontBake::Written written = FontBake::Ensure(plan, *slot, faces, extra, dir.string(), name);
    CHECK_MSG(written.ok, written.error);
    if (!written.ok) return baked;
    std::string why;
    baked.ok = baked.font.Load(written.fntPath, why);
    CHECK_MSG(baked.ok, "the engine loads the baked font: " + why);
    return baked;
}

// ---- fit ----------------------------------------------------------------------------------------------------------------------

void TheFitScalesTextToItsSlotAndNothingElse() {
    BakedFont latin = Bake("fit", "matura", "Fit_matura", {});
    if (!latin.ok) return;
    const Supersonic::BitmapFont& font = latin.font;
    const std::string text = "move the camera around";
    const double natural = static_cast<double>(Hud::MeasureText(font, text).x);
    CHECK_MSG(natural > 100.0, "the text has a width: " + std::to_string(natural));
    const double units = 0.4;
    const double width = natural * units;

    // No limit, or room to spare, leaves the size exactly as it was (the ==, not a tolerance: English must not move by a bit).
    CHECK(Hud::FitUnitsPerFontPx(font, text, units, 0.0) == units);
    CHECK(Hud::FitUnitsPerFontPx(font, text, units, 0.0, 0.0) == units);
    CHECK(Hud::FitUnitsPerFontPx(font, text, units, width) == units);
    CHECK(Hud::FitUnitsPerFontPx(font, text, units, width + 0.001) == units);
    CHECK(Hud::FitUnitsPerFontPx(font, text, units, width * 3.0) == units);
    // A text wider than the limit is scaled so that it is exactly that wide, and never grown.
    const double half = Hud::FitUnitsPerFontPx(font, text, units, width * 0.5);
    CHECK_MSG(Near(half, units * 0.5), "half the room: half the size: " + std::to_string(half));
    CHECK_MSG(Near(natural * half, width * 0.5), "and so exactly the limit wide");
    CHECK(Hud::FitUnitsPerFontPx(font, text, units, width * 0.9) < units);
    // A text of several lines: the height limit does for them what the width limit does for a line.
    const std::string lines = "one\ntwo\nthree";
    const glm::vec2 box = Hud::MeasureText(font, lines);
    CHECK_MSG(box.y > 2.5f * static_cast<float>(font.LineHeight()), "three lines are three lines tall");
    const double tall = static_cast<double>(box.y) * units;
    CHECK(Hud::FitUnitsPerFontPx(font, lines, units, 0.0, tall) == units);
    CHECK(Near(Hud::FitUnitsPerFontPx(font, lines, units, 0.0, tall * 0.5), units * 0.5));
    // Both limits: the smaller scale wins.
    const double both = Hud::FitUnitsPerFontPx(font, lines, units, static_cast<double>(box.x) * units * 0.25, tall * 0.5);
    CHECK_MSG(Near(both, units * 0.25), "the tighter of the two limits wins: " + std::to_string(both));
    // Code points, not bytes: a Cyrillic text is measured by its letters.
    const std::string russian = "\xD0\x90\xD0\x91\xD0\x92"; // three letters, six bytes
    const BakedFont cyrillic = Bake("fit-cyrillic", "matura", "Fit_cyrillic", {0x410, 0x411, 0x412});
    if (cyrillic.ok) {
        CHECK_MSG(Hud::MeasureText(cyrillic.font, russian).x > 0.0f, "three Cyrillic letters have a width");
        const double w = static_cast<double>(Hud::MeasureText(cyrillic.font, russian).x) * units;
        CHECK(Near(Hud::FitUnitsPerFontPx(cyrillic.font, russian, units, w * 0.5), units * 0.5));
    }
    // A font that did not load: nothing to measure, the size is left alone.
    const Supersonic::BitmapFont none;
    CHECK(Hud::FitUnitsPerFontPx(none, text, units, 1.0) == units);
}

// ---- the table ----------------------------------------------------------------------------------------------------------------

void ShowIsFormatAndThenDisplayOnce() {
    const Strings::Table english = Load(Lang::En);
    const Strings::Table russian = Load(Lang::Ru);
    const Strings::Table arabic = Load(Lang::Ar);
    for (const Strings::Table* table : {&english, &russian}) {
        CHECK_MSG(table->Show(Strings::Key::kHudPart, {{Strings::Placeholder::kN, "7"}}) ==
                      table->Format(Strings::Key::kHudPart, {{Strings::Placeholder::kN, "7"}}),
                  "for a language that is not shaped, Show is Format byte for byte");
        CHECK(table->Shape("abc 123") == "abc 123");
    }
    const std::string logical = arabic.Format(Strings::Key::kHudPart, {{Strings::Placeholder::kN, "7"}});
    const std::string shown = arabic.Show(Strings::Key::kHudPart, {{Strings::Placeholder::kN, "7"}});
    CHECK_MSG(shown != logical, "Arabic is shaped by Show");
    CHECK_MSG(shown == Strings::Display(Lang::Ar, logical), "and by the one Display, once");
    CHECK_MSG(arabic.Shape(logical) == shown, "Shape is that Display for text composed elsewhere");
    CHECK_MSG(arabic.Show(Strings::Key::kDashboardNew) == Strings::Display(Lang::Ar, arabic.Get(Strings::Key::kDashboardNew)),
              "Show with no values is the key's text, shaped");
    CHECK(arabic.Rtl() && !russian.Rtl() && !english.Rtl());
    // The placeholders are filled BEFORE the shaper: the digit is in the shaped text, left to right.
    CHECK_MSG(shown.find('7') != std::string::npos, "the number is in the shaped line: " + shown);
}

// ---- Hud and Pause ------------------------------------------------------------------------------------------------------------

void TheCaptionAndThePauseTitleAreTheTablesWords() {
    Hud::Rules hud;
    std::string error;
    CHECK_MSG(Hud::LoadRules(kData + "/ui.json", hud, error), error);
    const Strings::Table german = Load(Lang::De);
    CHECK((Hud::CaptionText(hud, 0)) == (std::string("Part 1")));
    CHECK((Hud::CaptionText(hud, 20)) == (std::string("Part 21")));
    CHECK((Hud::CaptionText(hud, 20, nullptr)) == (std::string("Part 21")));
    const std::string expected = german.Format(Strings::Key::kHudPart, {{Strings::Placeholder::kN, "21"}});
    CHECK_MSG(Hud::CaptionText(hud, 20, &german) == expected && expected.find("21") != std::string::npos && expected != "Part 21",
              "the caption is the table's hud.part with the level's number: " + expected);
    const Strings::Table japanese = Load(Lang::Ja);
    const std::string number = Hud::CaptionText(hud, 4, &japanese);
    CHECK_MSG(number.find('5') != std::string::npos && Utf8::Decode(number).size() > 2u && number.find("Part") == std::string::npos,
              "a language that puts the number inside the sentence: " + number);

    Pause::Rules pause;
    CHECK_MSG(Pause::LoadRules(kData + "/ui.json", pause, error), error);
    Pause::Level level;
    level.index = 20;
    CHECK((Pause::TitleText(pause, level)) == (std::string("Part 21")));
    CHECK_MSG(Pause::TitleText(pause, level, &german) == expected, "the pause's title is the same words");
}

// ---- Dashboard ----------------------------------------------------------------------------------------------------------------

void TheDashboardsWordsAreTheTablesAndEverySlotHasItsLimit() {
    Dashboard::Rules rules;
    std::string error;
    CHECK_MSG(Dashboard::LoadRules(kData + "/ui.json", rules, error), error);
    CHECK_MSG(rules.title.maxWidthUnits == 171.0 && rules.description.maxWidthUnits == 207.0 && rules.header.maxWidthUnits == 200.0 &&
                  rules.newLabel.text.maxWidthUnits == 52.0,
              "the limits ui.json names: title 171, description 207, header 200, new! 52");
    CHECK_MSG(rules.points.maxWidthUnits == 0.0 && rules.total.maxWidthUnits == 0.0, "numbers have none");

    // A board of one chapter header and one new, unlocked row.
    Dashboard::Board board;
    Dashboard::Line header;
    header.header = true;
    header.world = 1;
    board.lines.push_back(header);
    Dashboard::Line row;
    row.world = 1;
    row.level = 3;
    row.entry = 0;
    row.unlocked = true;
    row.levelUnlocked = true;
    row.isNew = true;
    row.title = "T";
    row.description = "D";
    row.icon = "i.png";
    row.points = 5;
    board.lines.push_back(row);
    board.achievements = 1;
    board.worlds = 4;
    const glm::dvec2 view(320.0, 256.0);
    const auto wordsOf = [&](const Strings::Table* table, const std::string& font, bool centred) {
        std::vector<std::string> found;
        for (const Dashboard::Piece& piece :
             Dashboard::Pieces(rules, board, Dashboard::State{}, view, 5000.0, 1000.0, table)) {
            if (piece.kind == Dashboard::Kind::Text && piece.file == font && piece.centred == centred) found.push_back(piece.words);
        }
        return found;
    };
    const std::vector<std::string> englishHeader = wordsOf(nullptr, rules.header.font, true);
    CHECK_MSG(!englishHeader.empty() && englishHeader.front() == "Chapter 2", "without a table: the rules' prefix and the chapter");
    const Strings::Table russian = Load(Lang::Ru);
    const std::vector<std::string> russianHeader = wordsOf(&russian, rules.header.font, true);
    CHECK_MSG(!russianHeader.empty() &&
                  russianHeader.front() == russian.Show(Strings::Key::kDashboardChapter, {{Strings::Placeholder::kN, "2"}}),
              "with one: the table's dashboard.chapter with the chapter");
    bool sawNew = false;
    bool sawEnglishNew = false;
    for (const Dashboard::Piece& piece : Dashboard::Pieces(rules, board, Dashboard::State{}, view, 5000.0, 1000.0, &russian)) {
        if (piece.kind != Dashboard::Kind::Text) continue;
        if (piece.words == russian.Show(Strings::Key::kDashboardNew)) {
            sawNew = true;
            CHECK_MSG(piece.maxWidthUnits == 52.0, "and the label carries its slot's limit");
        }
    }
    for (const Dashboard::Piece& piece : Dashboard::Pieces(rules, board, Dashboard::State{}, view, 5000.0, 1000.0, nullptr)) {
        if (piece.kind == Dashboard::Kind::Text && piece.words == "new!") sawEnglishNew = true;
        if (piece.kind == Dashboard::Kind::Text && piece.words == "T") CHECK_MSG(piece.maxWidthUnits == 171.0, "the title carries its limit");
        if (piece.kind == Dashboard::Kind::Text && piece.words == "D") CHECK_MSG(piece.maxWidthUnits == 207.0, "the description carries its limit");
    }
    CHECK_MSG(sawNew, "the new! mark is the table's");
    CHECK_MSG(sawEnglishNew, "and 'new!' without one");
}

// ---- Selector -----------------------------------------------------------------------------------------------------------------

void TheWarningIsTheTablesWithLockingsPercent() {
    Selector::Rules rules;
    std::string error;
    CHECK_MSG(Selector::LoadRules(kData + "/ui.json", rules, error), error);
    CHECK_MSG(rules.chapters.warning.maxWidthUnits == 91.0, "the warning's limit is ui.json's 91");
    Locking::Rules locking;
    CHECK_MSG(Locking::LoadRules(kData + "/ui.json", locking, error), error);
    Chapters::Table table;
    for (int world = 0; world < 4; ++world) {
        for (int level = 0; level < 4; ++level) {
            Chapters::Level entry;
            entry.world = world;
            entry.index = level;
            entry.name = "w" + std::to_string(world) + "l" + std::to_string(level);
            table.levels.push_back(entry);
        }
    }
    Scores::Store scores;
    CHECK(scores.Open("", error));
    const Strings::Table german = Load(Lang::De);
    CHECK_MSG(Selector::BuildChapters(rules, locking, scores, table).warningWords.empty(), "without a table the board holds none: the rules' words draw");
    CHECK(Selector::WarningWords(locking, nullptr).empty());
    const std::string words = Selector::WarningWords(locking, &german);
    CHECK_MSG(words == german.Show(Strings::Key::kSelectorRequires, {{Strings::Placeholder::kPercent, std::to_string(locking.chapterUnlockPercent)}}) &&
                  words.find(std::to_string(locking.chapterUnlockPercent)) != std::string::npos,
              "with one it is the table's requires, and locking.json's percent in it: " + words);
    CHECK((Selector::BuildChapters(rules, locking, scores, table, &german).warningWords) == (words));
    // The board's words are what the warning piece draws; the rules' own where it holds none.
    Scores::Store warned;
    CHECK(warned.Open("", error));
    warned.Record(1, 3, Scores::kBronze);
    for (const Strings::Table* strings : {static_cast<const Strings::Table*>(nullptr), &german}) {
        const Selector::Board board = Selector::BuildChapters(rules, locking, warned, table, strings);
        CHECK_MSG(board.chapters.size() > 1 && board.chapters[1].warned, "chapter 2's boss is scored while it is short");
        bool drawn = false;
        for (const Selector::Piece& piece : Selector::Pieces(rules, board, Selector::Open(board, 0), glm::dvec2(320.0, 256.0), 5000.0, 100.0,
                                                             Selector::Control::None, -1)) {
            if (piece.kind == Selector::Kind::Text && piece.maxWidthUnits == 91.0) {
                drawn = true;
                CHECK_MSG(piece.words == (strings == nullptr ? rules.chapters.warningWords : words), "the warning piece draws " + piece.words);
            }
        }
        CHECK_MSG(drawn, "the warning is among the pieces (it blinks, but is always there)");
    }
}

// ---- Credits ------------------------------------------------------------------------------------------------------------------

void TheCreditsTranslateTheirTwoSentencesAndNothingElse() {
    Credits::Rules rules;
    std::string error;
    CHECK_MSG(Credits::LoadRules(kData + "/ui.json", rules, error), error);
    const glm::dvec2 view(320.0, 256.0);
    const Credits::Scroll scroll = Credits::Start(view);
    const std::vector<Credits::Words> english = Credits::TailWords(rules, view, scroll);
    const Strings::Table russian = Load(Lang::Ru);
    const std::vector<Credits::Words> shown = Credits::TailWords(rules, view, scroll, &russian);
    CHECK_EQ(english.size(), rules.tail.lines.size());
    CHECK_EQ(shown.size(), english.size());
    int translated = 0;
    for (std::size_t i = 0; i < english.size() && i < shown.size(); ++i) {
        const Credits::Rules::Tail::Line& line = rules.tail.lines[i];
        if (line.textKey.empty()) {
            CHECK_MSG(shown[i].text == english[i].text && english[i].text == line.words, "'" + line.words + "' is a name or an address: every language keeps it");
            CHECK_MSG(shown[i].maxWidthUnits == 0.0, "and it has no limit");
            continue;
        }
        ++translated;
        CHECK_MSG(shown[i].text == russian.Show(line.textKey) && shown[i].text != line.words, "'" + line.words + "' is the table's " + line.textKey);
        CHECK_MSG(shown[i].maxWidthUnits == 250.0 && english[i].maxWidthUnits == 250.0, "and carries the 250 limit, in English too");
        CHECK_MSG(english[i].text == line.words, "English is the line's own words");
    }
    CHECK_EQ(translated, 2);
    CHECK_MSG(rules.tail.lines[1].textKey == Strings::Key::kCreditsEnhanced && rules.tail.lines[3].textKey == Strings::Key::kCreditsEngine,
              "the sentences are credits.enhanced and credits.engine");
}

// ---- Options ------------------------------------------------------------------------------------------------------------------

void TheOptionsWordsAreTheTables() {
    Controls::Rules controls;
    std::string error;
    CHECK_MSG(Controls::LoadRules(kData + Controls::kDataFile, controls, error), error);
    Options::Rules rules;
    CHECK_MSG(Options::LoadRules(kData + Controls::kDataFile, controls, rules, error), error);
    CHECK_MSG(rules.title.maxWidthUnits == 160.0 && rules.wordMaxWidthUnits == 80.0 && rules.adjust.maxWidthUnits == 148.0,
              "the limits controls.json names: title 160, tile words 80, adjust 148");
    Credits::Rules credits;
    CHECK_MSG(Credits::LoadRules(kData + "/ui.json", credits, error), error);
    Hud::Rules hud;
    CHECK_MSG(Hud::LoadRules(kData + "/ui.json", hud, error), error);
    const Strings::Table german = Load(Lang::De);
    const glm::dvec2 view(320.0, 256.0);
    const auto words = [&](const Strings::Table* table) {
        std::vector<std::pair<std::string, double>> found;
        for (const Options::Piece& piece : Options::Pieces(rules, credits, controls, hud, Controls::Tuning{}, view, 5000.0, Controls::Style::Modern,
                                                           Options::Widget::None, true, table)) {
            if (piece.element == Options::Element::Title || piece.element == Options::Element::Word) found.emplace_back(piece.words, piece.maxWidthUnits);
        }
        return found;
    };
    const auto english = words(nullptr);
    const auto shown = words(&german);
    CHECK_EQ(english.size(), 4u);
    CHECK_EQ(shown.size(), 4u);
    if (english.size() != 4u || shown.size() != 4u) return;
    CHECK_MSG(english[0].first == "Controls" && english[1].first == "Modern" && english[2].first == "Classic" && english[3].first == "Adjust controls",
              "English: the four words the rules hold");
    CHECK_MSG(shown[0].first == german.Show(Strings::Key::kOptionsTitle) && shown[1].first == german.Show(Strings::Key::kOptionsModern) &&
                  shown[2].first == german.Show(Strings::Key::kOptionsClassic) && shown[3].first == german.Show(Strings::Key::kOptionsAdjust),
              "German: the table's title, Modern, Classic and adjust words");
    CHECK_MSG(shown[0].second == 160.0 && shown[1].second == 80.0 && shown[2].second == 80.0 && shown[3].second == 148.0, "each with its limit");
}

// ---- Hint ---------------------------------------------------------------------------------------------------------------------

void TheHintIsMirroredForARightToLeftLanguage() {
    Hint::Rules rules;
    std::string error;
    CHECK_MSG(Hint::LoadRules(kData + "/camera.json", rules, error), error);
    CHECK_MSG(rules.maxWidthUnits == 256.0 && rules.mirrorWhenRtl, "camera.json names the limit 256 and mirror_when_rtl");
    const glm::dvec2 view(320.0, 256.0);
    const glm::dvec2 left = Hint::TextTopLeft(rules, view);
    CHECK_MSG(left.x == rules.leftUnits && left.y == view.y - rules.aboveBottomUnits, "left to right: 50 units in from the left, as it always was");
    CHECK(Hint::TextTopLeft(rules, view, false, 200.0) == left);
    const glm::dvec2 mirrored = Hint::TextTopLeft(rules, view, true, 200.0);
    CHECK_MSG(Near(mirrored.x + 200.0, view.x - rules.leftUnits) && mirrored.y == left.y,
              "right to left: the text's right edge is 50 units in from the right: " + std::to_string(mirrored.x));
    Hint::Rules plain = rules;
    plain.mirrorWhenRtl = false;
    CHECK_MSG(Hint::TextTopLeft(plain, view, true, 200.0) == left, "a rule that does not mirror is left-anchored whatever the language");
}

// ---- the optional members of the tables ------------------------------------------------------------------------------------

// A copy of ui.json with one text replaced, in a scratch folder: the loader must refuse a limit that is not a number above zero.
void ALimitThatIsNotANumberAboveZeroRefusesTheLoad() {
    const std::string original = ReadText(kData + "/ui.json");
    CHECK_MSG(!original.empty(), "ui.json reads");
    const std::string from = "\"max_width_units\": 207";
    const std::size_t at = original.find(from);
    CHECK_MSG(at != std::string::npos, "the dashboard description names its limit");
    if (at == std::string::npos) return;
    struct Case {
        const char* replacement;
        bool loads;
        const char* what;
    };
    const Case cases[] = {{"\"max_width_units\": 207", true, "the shipped number"},
                          {"\"max_width_units\": 0", false, "zero"},
                          {"\"max_width_units\": -5", false, "a negative number"},
                          {"\"max_width_units\": \"wide\"", false, "a string"},
                          {"\"max_width_units\": null", false, "null"},
                          {"\"max_width_units_\": 207", true, "the member absent: no limit, and the load is as it was"}};
    for (const Case& test : cases) {
        std::string text = original;
        text.replace(at, from.size(), test.replacement);
        const std::filesystem::path dir = Scratch("limit");
        WriteText(dir / "ui.json", text);
        Dashboard::Rules rules;
        std::string error;
        const bool loaded = Dashboard::LoadRules((dir / "ui.json").string(), rules, error);
        CHECK_MSG(loaded == test.loads, std::string(test.what) + ": " + (loaded ? "loaded" : error));
        if (!test.loads) CHECK_MSG(error.find("max_width_units") != std::string::npos, std::string(test.what) + " is refused and the member is named: " + error);
        if (test.loads && std::string(test.what).find("absent") != std::string::npos) {
            CHECK_MSG(rules.description.maxWidthUnits == 0.0, "absent is no limit");
        }
    }
    // The hint's two members.
    const std::string camera = ReadText(kData + "/camera.json");
    for (const std::string& bad : {std::string("\"mirror_when_rtl\": 1"), std::string("\"max_width_units\": 0")}) {
        const std::string key = bad.substr(0, bad.find(':'));
        const std::size_t position = camera.find(key);
        CHECK_MSG(position != std::string::npos, key + " is in camera.json");
        if (position == std::string::npos) continue;
        const std::size_t end = camera.find_first_of(",\r\n", position);
        std::string text = camera;
        text.replace(position, end - position, bad);
        const std::filesystem::path dir = Scratch("hint-limit");
        WriteText(dir / "camera.json", text);
        Hint::Rules rules;
        std::string error;
        CHECK_MSG(!Hint::LoadRules((dir / "camera.json").string(), rules, error) && error.find(key.substr(1, key.size() - 2)) != std::string::npos,
                  bad + " refuses the hint's load and names the member: " + error);
    }
}

// ---- Achievements -------------------------------------------------------------------------------------------------------------

Achievements::Content SyntheticContent() {
    Achievements::Content content;
    for (int id = 0; id < 3; ++id) {
        Achievements::Entry entry;
        entry.id = id;
        entry.world = id;
        entry.level = id;
        entry.points = 5;
        entry.title = "Title " + std::to_string(id);
        entry.description = "Describe " + std::to_string(id);
        entry.icon = "i.png";
        entry.isSecret = id == 2;
        content.entries.push_back(entry);
    }
    content.secret.title = "Secret";
    content.secret.descriptionPrefix = "From level ";
    content.secret.descriptionSeparator = "-";
    content.secret.icon = "s.png";
    return content;
}

void TheTranslationOverlaysTheEnglishListById() {
    const std::filesystem::path dir = Scratch("translation");
    // The overlay carries the secret achievement's two texts as well (Asantee's words, so in the gitignored overlay and in no committed table); every
    // refusal below is written WITH a good secret block, so that it is the fault named that refuses the file and not the lack of this one.
    const std::string secretBlock = R"J("secret": {"title": "Geheim", "description": "Aus\nLevel {world}-{level}"})J";
    const std::string good = std::string(R"J({"format": 1, "language": "de", )J") + secretBlock +
                             R"J(, "achievements": {"0": {"title": "Titel 0", "description": "Beschreibung 0"}, "2": {"title": "Titel 2", "description": "Beschreibung 2"}}})J";
    WriteText(dir / "ok.json", good);
    Achievements::Translation translation;
    std::string error;
    CHECK_MSG(Achievements::LoadTranslation((dir / "ok.json").string(), "de", translation, error), error);
    CHECK_EQ(translation.words.size(), 2u);
    CHECK(translation.words.at(0).title == "Titel 0" && translation.words.at(2).description == "Beschreibung 2");
    CHECK_MSG(translation.secret.title == "Geheim" && translation.secret.description == "Aus\nLevel {world}-{level}", "the secret block is read, its line break a line break");

    // The refusals, each naming the file.
    struct Bad {
        const char* what;
        std::string text;
        const char* code;
    };
    const std::string achievements = R"J("achievements": {"0": {"title": "a", "description": "b"}})J";
    const std::string open = R"J({"format": 1, )J";
    const auto withSecret = [&](const std::string& secret) { return open + secret + ", " + achievements + "}"; };
    const auto withEntries = [&](const std::string& entries) { return open + secretBlock + R"J(, "achievements": )J" + entries + "}"; };
    const Bad bad[] = {
        {"not JSON", "{", "de"},
        {"another format", R"J({"format": 2, )J" + secretBlock + R"J(, "achievements": {}})J", "de"},
        {"no achievements object", open + secretBlock + "}", "de"},
        {"another language", R"J({"format": 1, "language": "fr", )J" + secretBlock + R"J(, "achievements": {}})J", "de"},
        {"a key that is no id", withEntries(R"J({"zero": {"title": "a", "description": "b"}})J"), "de"},
        {"a negative id", withEntries(R"J({"-1": {"title": "a", "description": "b"}})J"), "de"},
        {"an empty title", withEntries(R"J({"0": {"title": "", "description": "b"}})J"), "de"},
        {"a missing description", withEntries(R"J({"0": {"title": "a"}})J"), "de"},
        {"a title that is no string", withEntries(R"J({"0": {"title": 5, "description": "b"}})J"), "de"},
        {"no secret block", open + achievements + "}", "de"},
        {"a secret block that is no object", withSecret(R"J("secret": "Geheim")J"), "de"},
        {"a secret without a title", withSecret(R"J("secret": {"description": "Level {world}-{level}"})J"), "de"},
        {"an empty secret title", withSecret(R"J("secret": {"title": "", "description": "Level {world}-{level}"})J"), "de"},
        {"a secret title with a brace", withSecret(R"J("secret": {"title": "Ge{world}heim", "description": "Level {world}-{level}"})J"), "de"},
        {"a secret without a description", withSecret(R"J("secret": {"title": "Geheim"})J"), "de"},
        {"a secret description that is no string", withSecret(R"J("secret": {"title": "Geheim", "description": 5})J"), "de"},
        {"a secret sentence without the chapter", withSecret(R"J("secret": {"title": "Geheim", "description": "Level {level}"})J"), "de"},
        {"a secret sentence without the level", withSecret(R"J("secret": {"title": "Geheim", "description": "Level {world}"})J"), "de"},
        {"a secret sentence with the chapter twice", withSecret(R"J("secret": {"title": "Geheim", "description": "Level {world}-{world}-{level}"})J"), "de"},
        {"a secret sentence with another placeholder", withSecret(R"J("secret": {"title": "Geheim", "description": "Level {world}-{level} {n}"})J"), "de"},
        {"a secret sentence with an unfinished brace", withSecret(R"J("secret": {"title": "Geheim", "description": "Level {world}-{level} {"})J"), "de"},
        {"a secret sentence with no placeholder at all", withSecret(R"J("secret": {"title": "Geheim", "description": "Level 1-1"})J"), "de"},
    };
    for (const Bad& test : bad) {
        WriteText(dir / "bad.json", test.text);
        Achievements::Translation untouched;
        untouched.code = "kept";
        std::string why;
        CHECK_MSG(!Achievements::LoadTranslation((dir / "bad.json").string(), test.code, untouched, why) && untouched.code == "kept" &&
                      why.find("bad.json") != std::string::npos,
                  std::string(test.what) + " is refused, `out` is left as it was, and the file is named: " + why);
    }
    // The two placeholders in the other order are the language's business.
    WriteText(dir / "order.json", withSecret(R"J("secret": {"title": "Geheim", "description": "{level} Level {world}"})J"));
    Achievements::Translation reordered;
    CHECK_MSG(Achievements::LoadTranslation((dir / "order.json").string(), "de", reordered, error), "{level} before {world} is read: " + error);
    CHECK_MSG(!Achievements::LoadTranslation((dir / "absent.json").string(), "de", translation, error) && error.find("absent.json") != std::string::npos,
              "a file that is not there is refused, by name: " + error);

    // Localized: the language over the English by id, the rest English and listed.
    const Achievements::Content english = SyntheticContent();
    std::vector<int> missing;
    const Achievements::Content shown = Achievements::Localized(english, &translation, &missing);
    CHECK_EQ(shown.entries.size(), 3u);
    CHECK(shown.entries[0].title == "Titel 0" && shown.entries[0].description == "Beschreibung 0");
    CHECK_MSG(shown.entries[1].title == "Title 1" && shown.entries[1].description == "Describe 1", "an id the file lacks keeps its English");
    CHECK_MSG(missing == std::vector<int>{1}, "and is listed");
    CHECK_MSG(shown.entries[0].icon == "i.png" && shown.entries[0].points == 5 && shown.entries[2].isSecret, "nothing but the words changes");
    // The secret's words are the overlay's: the title, and the sentence with the chapter and the level filled; the icon stays the original's.
    CHECK((shown.secret.title) == (std::string("Geheim")));
    CHECK((shown.secret.descriptionTemplate) == (std::string("Aus\nLevel {world}-{level}")));
    CHECK_MSG(shown.secret.icon == "s.png", "the secret's picture is not a word");
    const std::string secret = Achievements::DescriptionOf(shown, shown.entries[2], false);
    CHECK_MSG(secret == "Aus\nLevel 3-3", "the locked secret's sentence has its chapter and its level filled: " + secret);
    CHECK((Achievements::TitleOf(shown, shown.entries[2], false)) == (std::string("Geheim")));
    CHECK_MSG(Achievements::DescriptionOf(shown, shown.entries[2], true) == "Beschreibung 2", "unlocked, the entry's own words");
    // SecretSamples: the two texts as they will be drawn, the chapter and the level as a digit (what the font coverage needs).
    CHECK_MSG((Achievements::SecretSamples(shown) == std::vector<std::string>{"Geheim", "Aus\nLevel 0-0"}), "the samples of a translated secret");
    CHECK_MSG((Achievements::SecretSamples(english) == std::vector<std::string>{"Secret", "From level 0-0"}), "the samples of the English one");
    // Without a language: the English list, entry for entry, and the original's sentence for the secret.
    const Achievements::Content same = Achievements::Localized(english, nullptr);
    CHECK(same.entries.size() == 3u && same.entries[1].title == "Title 1" && same.secret.descriptionTemplate.empty() && same.secret.title == "Secret");
    CHECK((Achievements::DescriptionOf(same, same.entries[2], false)) == (std::string("From level 3-3")));
    CHECK((Achievements::DescriptionOf(english, english.entries[2], false)) == (std::string("From level 3-3")));
}

void runTests() {
    TheFitScalesTextToItsSlotAndNothingElse();
    ShowIsFormatAndThenDisplayOnce();
    TheCaptionAndThePauseTitleAreTheTablesWords();
    TheDashboardsWordsAreTheTablesAndEverySlotHasItsLimit();
    TheWarningIsTheTablesWithLockingsPercent();
    TheCreditsTranslateTheirTwoSentencesAndNothingElse();
    TheOptionsWordsAreTheTables();
    TheHintIsMirroredForARightToLeftLanguage();
    ALimitThatIsNotANumberAboveZeroRefusesTheLoad();
    TheTranslationOverlaysTheEnglishListById();
}

} // namespace

MP_TEST_MAIN("test_mp_langsim", 150)
