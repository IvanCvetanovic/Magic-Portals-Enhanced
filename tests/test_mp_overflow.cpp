// Does every translation fit the box it is drawn in? (planning step 238, PORT-34)
//
// Nothing wraps in this game, and the boxes are fixed in design units (the view is 256 tall): a translation longer than its English spills out of
// its button, over its neighbour or off the screen. A slot that can be too narrow names a `max_width_units` in its rule table
// (game/data/ui.json, controls.json, camera.json, achievements.json; Hud::FitUnitsPerFontPx scales the text to it), and this suite measures, for
// every language, every string a slot draws, with the REAL font set of that language - the original's bitmaps for en de es fr it pt, the fonts
// FontBake makes from game/data/fonts for ru tr uk ja ar, exactly as the layer asks for them - at the size its rule table draws it, against
// the slot's limit.
//
//   test_mp_overflow               the fourteen interface strings (game/data/lang) in each of the eleven languages, against the slot each is
//                                  drawn in. English must fit with nothing to spare to scale (the limits are the English widths, so no English
//                                  string is ever shrunk), and no translation may need a font smaller than kReadableScale of the rule's: below
//                                  that a line is not readable on a phone, and the translation has to be shortened, not squeezed. Prints each
//                                  language's tightest slot.
//   test_mp_overflow_achievements  the same for the achievements' titles and descriptions (the dashboard's bar), for the unlock card with each
//                                  title in it, for the pause's popup of every level, and for the LOCKED secret achievements' title and sentence
//                                  (the overlay's `secret` block, with each such achievement's chapter and level in it), in each language that has
//                                  out/data/achievements.<code>.json. Those are Asantee's words in other languages, gitignored: without them (or
//                                  without Enhanced's own list) the suite exits 77, SKIPPED, never a pass.
//
// Shown by fitting, not by wrapping: the numbers this prints are the scale each text will be drawn at, relative to its rule's own.
//
// No non-ASCII character is written in this file: MSVC is built without /utf-8 here.

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "core/BitmapFont.hpp"
#include "sim/Achievements.hpp"
#include "sim/Controls.hpp"
#include "sim/Credits.hpp"
#include "sim/Dashboard.hpp"
#include "sim/FontBake.hpp"
#include "sim/Hint.hpp"
#include "sim/Hud.hpp"
#include "sim/Notificator.hpp"
#include "sim/Options.hpp"
#include "sim/Selector.hpp"
#include "sim/Strings.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

using namespace MagicPortals;
using Strings::Lang;

namespace {

// The port's own data: MP_TEST_PORTDATA when it is set (TEST ONLY: a mutation test runs this same binary on a copy of game/data with one translation
// made longer), else the build's own.
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
    std::printf("test_mp_overflow: MUTATION - MP_TEST_PORTDATA is set, the port's data is read from %s, not from game/data\n", text.c_str());
    return text;
}

const std::string kData = PortDataDir();
const std::string kOriginal = MAGICPORTALS_ORIGINAL_DIR;
const std::string kAchievements = MAGICPORTALS_ACHIEVEMENTS_FILE;

// The smallest fraction of its rule's size a text may be drawn at. A translation that needs less is too long for the slot and is shortened
// (the translators were told the widths); this is the limit of what fitting may do before the text is no longer comfortable to read on a phone.
// The shipped translations are all above 0.82 (the tightest is French's titles and German's Classic tile), so 0.75 leaves a little room for an edit and
// fails for a translation twice as long as it should be.
constexpr double kReadableScale = 0.75;

std::string CodeOf(Lang lang) {
    return std::string(Strings::kLangCodes[Strings::IndexOf(lang)]);
}

bool OriginalFontsAreThere() {
    std::error_code ec;
    return std::filesystem::is_regular_file(kOriginal + "/data/Matura84_shadow.fnt", ec) &&
           std::filesystem::is_regular_file(kOriginal + "/data/Verdana40_shadow.fnt", ec);
}

// The two fonts a language draws its text with, as the layer loads them.
struct Fonts {
    Supersonic::BitmapFont matura;
    Supersonic::BitmapFont verdana;
    bool ok = false;
};

std::unique_ptr<Fonts> FontsOf(const Strings::Table& table, Lang lang, const std::vector<std::string>& achievementWords, const std::string& folder) {
    auto fonts = std::make_unique<Fonts>();
    std::string why;
    if (table.Languages().FontSetOf(lang) == Strings::FontSet::Original) {
        fonts->ok = fonts->matura.Load(kOriginal + "/data/Matura84_shadow.fnt", why) && fonts->verdana.Load(kOriginal + "/data/Verdana40_shadow.fnt", why);
        CHECK_MSG(fonts->ok, CodeOf(lang) + ": the original's fonts load: " + why);
        return fonts;
    }
    const FontBake::Plan& plan = table.Languages().Fonts();
    std::vector<FontBake::FaceBytes> faces;
    std::string error;
    CHECK_MSG(FontBake::LoadFaces(plan, kData + "/" + plan.folder, faces, error), error);
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / ("supersonic-test-mp-overflow-" + folder);
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    bool all = true;
    for (const FontBake::SlotSpec& slot : plan.slots) {
        const std::set<std::uint32_t> codePoints =
            Strings::SlotCodePoints(table, slot.id, slot.id == Strings::kSlotVerdana ? achievementWords : std::vector<std::string>());
        const FontBake::Written written = FontBake::Ensure(plan, slot, faces, codePoints, dir.string(), slot.originalFont + "_" + CodeOf(lang));
        CHECK_MSG(written.ok, CodeOf(lang) + " " + slot.id + ": " + written.error);
        if (!written.ok) {
            all = false;
            continue;
        }
        Supersonic::BitmapFont& target = slot.id == Strings::kSlotMatura ? fonts->matura : fonts->verdana;
        all = target.Load(written.fntPath, why) && all;
    }
    fonts->ok = all;
    return fonts;
}

// What a text must be scaled by to fit: 1 when it fits, else limit / width (and height / height for lines).
double Required(const Supersonic::BitmapFont& font, const std::string& text, double units, double maxWidth, double maxHeight = 0.0) {
    const double fit = Hud::FitUnitsPerFontPx(font, text, units, maxWidth, maxHeight);
    return fit / units;
}

// One slot of the interface: where its rule is, and the font and key it draws.
struct Slot {
    std::string what;
    std::string key; // the table's key, or "" for a text composed by the case
    bool verdana = false;
    double units = 0.0;
    double maxWidth = 0.0;
    double maxHeight = 0.0;
};

struct Rules {
    Dashboard::Rules dashboard;
    Selector::Rules selector;
    Credits::Rules credits;
    Controls::Rules controls;
    Options::Rules options;
    Hint::Rules hint;
    Notificator::Rules notificator;
    Achievements::Rules achievements;
    bool ok = false;
};

std::unique_ptr<Rules> LoadRules() {
    auto rules = std::make_unique<Rules>();
    std::string error;
    rules->ok = Dashboard::LoadRules(kData + "/ui.json", rules->dashboard, error) && Selector::LoadRules(kData + "/ui.json", rules->selector, error) &&
                Credits::LoadRules(kData + "/ui.json", rules->credits, error) && Controls::LoadRules(kData + Controls::kDataFile, rules->controls, error) &&
                Options::LoadRules(kData + Controls::kDataFile, rules->controls, rules->options, error) &&
                Hint::LoadRules(kData + "/camera.json", rules->hint, error) &&
                Notificator::LoadRules(kData + "/achievements.json", kData + "/gravitywell.json", rules->notificator, error) &&
                Achievements::LoadRules(kData + "/achievements.json", rules->achievements, error);
    CHECK_MSG(rules->ok, "the rule tables read: " + error);
    return rules;
}

// The slots of the fourteen keys the tables draw with a limit, with the numbers of the real rule tables.
std::vector<Slot> SlotsOf(const Rules& rules) {
    std::vector<Slot> slots;
    const auto line = [&](const std::string& key) -> const Credits::Rules::Tail::Line* {
        for (const Credits::Rules::Tail::Line& tail : rules.credits.tail.lines) {
            if (tail.textKey == key) return &tail;
        }
        return nullptr;
    };
    slots.push_back({"dashboard header", std::string(Strings::Key::kDashboardChapter), false, rules.dashboard.header.unitsPerFontPx, rules.dashboard.header.maxWidthUnits, 0.0});
    slots.push_back({"dashboard new!", std::string(Strings::Key::kDashboardNew), true, rules.dashboard.newLabel.text.unitsPerFontPx, rules.dashboard.newLabel.text.maxWidthUnits, 0.0});
    slots.push_back({"chapter select's warning", std::string(Strings::Key::kSelectorRequires), false, rules.selector.chapters.warning.unitsPerFontPx, rules.selector.chapters.warning.maxWidthUnits, 0.0});
    slots.push_back({"locked-level card", std::string(Strings::Key::kCardLockedLevel), true, rules.notificator.unitsPerFontPx, rules.notificator.maxWidthUnits, 0.0});
    for (const char* key : {Strings::Key::kCreditsEnhanced.data(), Strings::Key::kCreditsEngine.data()}) {
        if (const Credits::Rules::Tail::Line* tail = line(key)) {
            slots.push_back({std::string("credits line ") + key, key, false, tail->unitsPerFontPx, tail->maxWidthUnits, 0.0});
        }
    }
    slots.push_back({"options title", std::string(Strings::Key::kOptionsTitle), false, rules.options.title.unitsPerFontPx, rules.options.title.maxWidthUnits, 0.0});
    slots.push_back({"options Modern tile", std::string(Strings::Key::kOptionsModern), false, rules.options.wordUnitsPerFontPx, rules.options.wordMaxWidthUnits, 0.0});
    slots.push_back({"options Classic tile", std::string(Strings::Key::kOptionsClassic), false, rules.options.wordUnitsPerFontPx, rules.options.wordMaxWidthUnits, 0.0});
    slots.push_back({"options adjust button", std::string(Strings::Key::kOptionsAdjust), false, rules.options.adjust.unitsPerFontPx, rules.options.adjust.maxWidthUnits, 0.0});
    slots.push_back({"1-21's hint", std::string(Strings::Key::kHintCamera), false, rules.hint.unitsPerFontPx, rules.hint.maxWidthUnits, 0.0});
    return slots;
}

void TheLimitsAreInTheTables() {
    const std::unique_ptr<Rules> rules = LoadRules();
    if (!rules->ok) return;
    const std::vector<Slot> slots = SlotsOf(*rules);
    CHECK_MSG(slots.size() == 11u, "eleven slots with a limit: " + std::to_string(slots.size()));
    for (const Slot& slot : slots) {
        CHECK_MSG(slot.units > 0.0 && slot.maxWidth > 0.0, slot.what + " has a size and a limit in its rule table");
    }
    CHECK_MSG(rules->notificator.maxWidthUnits > 0.0 && rules->achievements.popup.maxWidthUnits > 0.0 && rules->achievements.popup.maxHeightUnits > 0.0,
              "and so do the card and the popup");
}

void EveryTranslationFitsItsSlot() {
    if (!OriginalFontsAreThere()) {
        std::printf("  EveryTranslationFitsItsSlot SKIPPED - needs the original's extracted fonts at %s/data\n", kOriginal.c_str());
        return;
    }
    const std::unique_ptr<Rules> rules = LoadRules();
    if (!rules->ok) return;
    const std::vector<Slot> slots = SlotsOf(*rules);
    for (std::size_t i = 0; i < Strings::kLangCount; ++i) {
        const Lang lang = static_cast<Lang>(i);
        const std::string code = CodeOf(lang);
        Strings::Table table;
        std::string error;
        CHECK_MSG(table.Load(kData, lang, error), code + ": " + error);
        const std::unique_ptr<Fonts> fonts = FontsOf(table, lang, {}, "ui-" + code);
        if (!fonts->ok) continue;
        double tightest = 1.0;
        std::string tightestWhat = "-";
        for (const Slot& slot : slots) {
            const std::string shown = table.Show(slot.key, {{Strings::Placeholder::kN, "4"}, {Strings::Placeholder::kPercent, "60"},
                                                            {Strings::Placeholder::kTitle, ""}, {Strings::Placeholder::kWorld, "4"},
                                                            {Strings::Placeholder::kLevel, "32"}});
            const Supersonic::BitmapFont& font = slot.verdana ? fonts->verdana : fonts->matura;
            const double scale = Required(font, shown, slot.units, slot.maxWidth, slot.maxHeight);
            if (scale < tightest) {
                tightest = scale;
                tightestWhat = slot.what;
            }
            if (lang == Lang::En) {
                // The limits ARE the English widths: not one English string is shrunk, to the bit.
                CHECK_MSG(Hud::FitUnitsPerFontPx(font, shown, slot.units, slot.maxWidth, slot.maxHeight) == slot.units, "English is not shrunk in " + slot.what);
            }
            CHECK_MSG(scale >= kReadableScale, code + ": " + slot.what + " needs " + std::to_string(scale) + " of its size to fit, below " + std::to_string(kReadableScale) +
                                                   " (\"" + (shown.size() > 60 ? shown.substr(0, 60) : shown) + "\")");
        }
        std::printf("  %s: the tightest slot is %s, drawn at %.2f of its rule's size\n", code.c_str(), tightestWhat.c_str(), tightest);
    }
}

void runTests() {
    TheLimitsAreInTheTables();
    EveryTranslationFitsItsSlot();
}

} // namespace

MP_TEST_MAIN("test_mp_overflow", 40)

// ---- the achievements -------------------------------------------------------------------------------------------------------------------

namespace {

bool TranslationsAreThere() {
    std::error_code ec;
    return std::filesystem::is_regular_file(kAchievements, ec);
}

} // namespace

static int mpOverflowAchievementsMain(int, char**) {
    std::error_code ec;
    int found = 0;
    for (std::size_t i = 1; i < Strings::kLangCount; ++i) {
        found += std::filesystem::is_regular_file((std::filesystem::path(kAchievements).parent_path() / ("achievements." + CodeOf(static_cast<Lang>(i)) + ".json")), ec) ? 1 : 0;
    }
    if (!OriginalFontsAreThere() || !TranslationsAreThere() || found == 0) {
        std::printf("test_mp_overflow_achievements: SKIPPED - needs Enhanced's achievements list at %s, at least one achievements.<code>.json beside it and the original's fonts at %s/data\n",
                    kAchievements.c_str(), kOriginal.c_str());
        return 77;
    }
    const std::unique_ptr<Rules> rules = LoadRules();
    Achievements::Content english;
    std::string error;
    CHECK_MSG(Achievements::Load(kAchievements, english, error), error);
    if (!rules->ok || english.entries.empty()) return 1;
    const std::vector<int> nobody;
    for (std::size_t i = 0; i < Strings::kLangCount; ++i) {
        const Lang lang = static_cast<Lang>(i);
        const std::string code = CodeOf(lang);
        Strings::Table table;
        CHECK_MSG(table.Load(kData, lang, error), code + ": " + error);
        Achievements::Translation translation;
        const std::string file = (std::filesystem::path(kAchievements).parent_path() / ("achievements." + code + ".json")).string();
        const bool there = lang != Lang::En && std::filesystem::is_regular_file(file, ec);
        const bool have = there && Achievements::LoadTranslation(file, code, translation, error);
        if (lang != Lang::En && !there) {
            std::printf("  %s: no translation file, not measured\n", code.c_str());
            continue;
        }
        // A file that is there and will not read (its secret block missing, say) is a failure, never a silent skip.
        CHECK_MSG(lang == Lang::En || have, code + ": the translation file reads: " + error);
        if (lang != Lang::En && !have) continue;
        const Achievements::Content shown = Achievements::Localized(english, lang == Lang::En ? nullptr : &translation);
        std::vector<std::string> words;
        for (const Achievements::Entry& entry : shown.entries) {
            words.push_back(table.Shape(entry.title));
            words.push_back(table.Shape(entry.description));
        }
        for (const std::string& text : Achievements::SecretSamples(shown)) words.push_back(table.Shape(text));
        const std::unique_ptr<Fonts> fonts = FontsOf(table, lang, words, "ach-" + code);
        if (!fonts->ok) continue;
        const Supersonic::BitmapFont& font = fonts->verdana;
        double worstTitle = 1.0;
        double worstDescription = 1.0;
        double worstCard = 1.0;
        double worstSecret = 1.0;
        int secretsMeasured = 0;
        for (const Achievements::Entry& entry : shown.entries) {
            const std::string title = table.Shape(entry.title);
            const std::string description = table.Shape(entry.description);
            const double t = Required(font, title, rules->dashboard.title.unitsPerFontPx, rules->dashboard.title.maxWidthUnits);
            const double d = Required(font, description, rules->dashboard.description.unitsPerFontPx, rules->dashboard.description.maxWidthUnits);
            const std::string card = table.Show(Strings::Key::kCardUnlocked, {{Strings::Placeholder::kTitle, entry.title}});
            const double c = Required(font, card, rules->notificator.unitsPerFontPx, rules->notificator.maxWidthUnits);
            if (t < worstTitle) worstTitle = t;
            if (d < worstDescription) worstDescription = d;
            if (c < worstCard) worstCard = c;
            if (lang == Lang::En) {
                CHECK_MSG(t == 1.0 && d == 1.0 && c == 1.0, "English achievement " + std::to_string(entry.id) + " is not shrunk (title, description, card)");
            }
            CHECK_MSG(t >= kReadableScale, code + ": the title of " + std::to_string(entry.id) + " needs " + std::to_string(t) + " of its size");
            CHECK_MSG(d >= kReadableScale, code + ": the description of " + std::to_string(entry.id) + " needs " + std::to_string(d) + " of its size");
            CHECK_MSG(c >= kReadableScale, code + ": the unlock card of " + std::to_string(entry.id) + " needs " + std::to_string(c) + " of its size");
            if (!entry.isSecret) continue;
            // While locked a secret achievement shows the overlay's words, with its own chapter and level in the sentence.
            const double lockedTitle = Required(font, table.Shape(Achievements::TitleOf(shown, entry, false)), rules->dashboard.title.unitsPerFontPx,
                                                rules->dashboard.title.maxWidthUnits);
            const double lockedSentence = Required(font, table.Shape(Achievements::DescriptionOf(shown, entry, false)), rules->dashboard.description.unitsPerFontPx,
                                                   rules->dashboard.description.maxWidthUnits);
            ++secretsMeasured;
            if (lockedTitle < worstSecret) worstSecret = lockedTitle;
            if (lockedSentence < worstSecret) worstSecret = lockedSentence;
            if (lang == Lang::En) {
                CHECK_MSG(lockedTitle == 1.0 && lockedSentence == 1.0, "English locked secret achievement " + std::to_string(entry.id) + " is not shrunk");
            }
            CHECK_MSG(lockedTitle >= kReadableScale, code + ": the locked secret title of " + std::to_string(entry.id) + " needs " + std::to_string(lockedTitle) + " of its size");
            CHECK_MSG(lockedSentence >= kReadableScale, code + ": the locked secret sentence of " + std::to_string(entry.id) + " needs " + std::to_string(lockedSentence) + " of its size");
        }
        CHECK_MSG(secretsMeasured > 0, code + ": Enhanced's list has secret achievements, and their locked words were measured");
        // The pause's popup of each level that has achievements.
        double worstPopup = 1.0;
        for (int world = 0; world < 4; ++world) {
            for (int level = 0; level < 32; ++level) {
                const std::vector<const Achievements::Entry*> entries = Achievements::FromLevel(shown, world, level);
                if (entries.empty()) continue;
                const std::string text = table.Shape(Achievements::PopupText(rules->achievements, shown, entries, nobody));
                const double p = Required(font, text, rules->achievements.popup.unitsPerFontPx, rules->achievements.popup.maxWidthUnits, rules->achievements.popup.maxHeightUnits);
                if (p < worstPopup) worstPopup = p;
                CHECK_MSG(p >= kReadableScale, code + ": the popup of " + std::to_string(world + 1) + "-" + std::to_string(level + 1) + " needs " + std::to_string(p) + " of its size");
            }
        }
        std::printf("  %s: worst title %.2f, description %.2f, unlock card %.2f, locked secret %.2f (%d), popup %.2f of the rule's size\n", code.c_str(), worstTitle,
                    worstDescription, worstCard, worstSecret, secretsMeasured, worstPopup);
    }
    return ::test::summary("test_mp_overflow_achievements", 200);
}

static const ::MpSuite::Registration mpOverflowAchievementsRegistration("test_mp_overflow_achievements", &mpOverflowAchievementsMain);
