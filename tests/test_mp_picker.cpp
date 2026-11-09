// The language picker and its entries (planning step 239, PORT-35).
//
// test_mp_picker - the pure half, with no window and nothing outside this repository: game/data/picker.json as it is and as its loader refuses
//   it, the grid, the two globes and the screen's pieces at the nine shapes of window the game is held at (clear of the back button, of each
//   other and of every button they share a screen with), the touch rule, and the atlas of the eleven names baked from the committed faces
//   (every glyph of every name there, none scaled below a readable half).
// test_mp_pickerlayer - the layer's half: a player's run shows the globe, opens the screen, draws every name without a hole in every language, keeps
//   a choice (in language.json, and a fresh layer starts in it), leaves for the screen that opened it, and the runs that must not have a picker
//   (a bare layer, a table that will not read) have none. Needs the converted levels and the original's pictures, and exits 77 without them.
//
// No non-ASCII character is written in this file: MSVC is built without /utf-8 here. The names are read from game/data/lang/languages.json.

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "MagicPortalsLayer.hpp"

#include "core/BitmapFont.hpp"
#include "core/Input.hpp"
#include "core/Json.hpp"
#include "core/PhysicsSystem.hpp"
#include "core/ScreenOverlay.hpp"
#include "core/ViewportInfo.hpp"

#include "sim/Controls.hpp"
#include "sim/Credits.hpp"
#include "sim/FontBake.hpp"
#include "sim/Hud.hpp"
#include "sim/Language.hpp"
#include "sim/MainMenu.hpp"
#include "sim/Options.hpp"
#include "sim/PadArt.hpp"
#include "sim/Pause.hpp"
#include "sim/Picker.hpp"
#include "sim/Strings.hpp"
#include "sim/Utf8.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

using namespace Supersonic;
using MagicPortals::MagicPortalsLayer;
namespace Controls = MagicPortals::Controls;
namespace Credits = MagicPortals::Credits;
namespace FontBake = MagicPortals::FontBake;
namespace Hud = MagicPortals::Hud;
namespace Language = MagicPortals::Language;
namespace MainMenu = MagicPortals::MainMenu;
namespace Options = MagicPortals::Options;
namespace PadArt = MagicPortals::PadArt;
namespace Picker = MagicPortals::Picker;
namespace Strings = MagicPortals::Strings;
namespace Utf8 = MagicPortals::Utf8;
using Strings::Lang;

namespace {

namespace fs = std::filesystem;

// The port's own data: MP_TEST_PORTDATA when it is set (TEST ONLY: a mutation test runs this same binary on a copy of game/data), else the build's own.
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
    std::printf("test_mp_picker: MUTATION - MP_TEST_PORTDATA is set, the port's data is read from %s, not from game/data\n", text.c_str());
    return text;
}

const std::string kData = PortDataDir();
const std::string kPicker = kData + Picker::kDataFile;
const std::string kUi = kData + "/ui.json";
const std::string kControls = kData + "/controls.json";

constexpr double kHeight = 256.0;     // the view's height, in units
constexpr double kSettled = 1.0e9;    // a state clock long past every entrance and bounce
constexpr double kPxPerUnit = 2.8125; // 720 px over the view's 256 units
constexpr double kMatura84LineHeight = 84.0;

struct Aspect {
    const char* name;
    double ratio;
};
// The shapes the screens are held at: 5:4 is the narrowest supported window and 32:9 the widest (the modern-controls design 1.7; test_mp_options).
const std::array<Aspect, 9> kAspects = {{{"5:4", 1.25},
                                         {"4:3", 4.0 / 3.0},
                                         {"3:2", 1.5},
                                         {"16:10", 1.6},
                                         {"16:9", 16.0 / 9.0},
                                         {"20:9", 20.0 / 9.0},
                                         {"21:9", 21.0 / 9.0},
                                         {"32:9", 32.0 / 9.0},
                                         {"2:1", 2.0}}};

glm::dvec2 ViewOf(double ratio) {
    return glm::dvec2(kHeight * ratio, kHeight);
}

bool Near(double a, double b, double eps = 1e-9) {
    return std::fabs(a - b) <= eps;
}

std::string Num(double v) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.4f", v);
    return buffer;
}

std::string Show(const Hud::Rect& rect) {
    const glm::dvec2 max = rect.Max();
    return "(" + Num(rect.min.x) + ", " + Num(rect.min.y) + ")-(" + Num(max.x) + ", " + Num(max.y) + ")";
}

// The gap between two rectangles: the larger of their separations along the axes, negative when their interiors overlap.
double Gap(const Hud::Rect& a, const Hud::Rect& b) {
    const glm::dvec2 amax = a.Max();
    const glm::dvec2 bmax = b.Max();
    return std::max(std::max(b.min.x - amax.x, a.min.x - bmax.x), std::max(b.min.y - amax.y, a.min.y - bmax.y));
}

bool Inside(const Hud::Rect& inner, const Hud::Rect& outer, double eps = 1e-9) {
    const glm::dvec2 imax = inner.Max();
    const glm::dvec2 omax = outer.Max();
    return inner.min.x >= outer.min.x - eps && inner.min.y >= outer.min.y - eps && imax.x <= omax.x + eps && imax.y <= omax.y + eps;
}

bool Contains(const std::string& text, const std::string& what) {
    return text.find(what) != std::string::npos;
}

std::string ReadAll(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    std::ostringstream text;
    text << file.rdbuf();
    return text.str();
}

void WriteFile(const fs::path& path, const std::string& text) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file << text;
}

fs::path Scratch(const std::string& name) {
    const fs::path dir = fs::temp_directory_path() / ("supersonic-test-mp-" + name);
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    return dir;
}

// Everything the sim cases read, loaded once.
struct Loaded {
    Picker::Rules picker;
    Controls::Rules controls;
    Options::Rules options;
    Credits::Rules credits;
    MainMenu::Rules menu;
    Strings::LanguageTable languages;
    bool ok = false;
};

const Loaded& Data() {
    static const Loaded loaded = [] {
        Loaded l;
        std::string error;
        bool ok = Picker::LoadRules(kPicker, l.picker, error);
        CHECK_MSG(ok, "picker.json reads: " + error);
        ok = ok && Controls::LoadRules(kControls, l.controls, error);
        CHECK_MSG(ok, "controls.json reads: " + error);
        ok = ok && Options::LoadRules(kControls, l.controls, l.options, error);
        CHECK_MSG(ok, "and so does its options block: " + error);
        const bool ui = Credits::LoadRules(kUi, l.credits, error) && MainMenu::LoadRules(kUi, l.menu, error);
        CHECK_MSG(ui, "ui.json's credits and main menu read: " + error);
        const bool languages = Strings::LoadLanguages(kData + Strings::kLanguagesFile, l.languages, error);
        CHECK_MSG(languages, "languages.json reads: " + error);
        l.ok = ok && ui && languages;
        return l;
    }();
    return loaded;
}

// ---- the table, as the file says it ----------------------------------------------------------------------------------------------

void ThePickerTableIsWhatTheFileSays() {
    const Loaded& d = Data();
    if (!d.ok) return;
    const Picker::Rules& p = d.picker;
    // The main menu's globe: under the gear, flush right, the gear's size, drawn from a stem into a 128 px picture with the generated glyphs' margin.
    CHECK_MSG(p.entry.atScreen == glm::dvec2(1.0, 0.0) && p.entry.origin == glm::dvec2(1.0, 0.0), "anchored on the top-right corner, by its top-right corner");
    CHECK_MSG(p.entry.offsetUnits == glm::dvec2(0.0, 36.0) && p.entry.sizeUnits == glm::dvec2(32.0, 32.0), "32 x 32 units, 36 under the gear's top");
    CHECK_MSG(p.entry.sizeUnits == d.options.entry.sizeUnits, "the gear's size");
    CHECK_MSG(p.entry.glyph == "language_globe" && p.entry.glyphPx == 128 && p.entry.bodyInset == 0.109375, "the globe's stem, picture and margin");
    CHECK_MSG(p.entry.bodyInset == d.options.entryBodyInset, "the gear's margin");
    // The Options corner.
    CHECK_MSG(p.corner.atScreen == glm::dvec2(1.0, 0.0) && p.corner.origin == glm::dvec2(1.0, 0.0) && p.corner.offsetUnits == glm::dvec2(0.0) &&
                  p.corner.sizeUnits == glm::dvec2(32.0, 32.0),
              "the Options screen's corner is the top-right 32 units");
    // The title is the Options title's font and size.
    CHECK_MSG(p.title.font == "Matura84_shadow.fnt" && p.title.font == d.options.title.font, "the title is in the original's font, as Options' is");
    CHECK_MSG(p.title.unitsPerFontPx == 0.5 && p.title.unitsPerFontPx == d.options.title.unitsPerFontPx, "at the Options title's size");
    CHECK_MSG(p.title.yAtScreen == 0.12 && p.title.alphaByte == 255 && p.title.maxWidthUnits == 200.0, "high, opaque, and fitted to 200 units");
    // The grid: three columns, plates of 72 x 34 with 6 and 8 between, the first row at 62, clear of the back button.
    CHECK_MSG(p.grid.columns == 3 && p.grid.plateSizeUnits == glm::dvec2(72.0, 34.0) && p.grid.gapUnits == glm::dvec2(6.0, 8.0), "three columns of 72 x 34 plates");
    CHECK_MSG(p.grid.topUnits == 62.0 && p.grid.clearLeftUnits == 84.0, "62 down, 84 in from the left");
    // The plates take the Options tiles' colours.
    CHECK_MSG(p.plate.idleFrame == d.options.idleFrame && p.plate.chosenFrame == d.options.chosenFrame, "the tiles' idle and chosen plates");
    CHECK_MSG(p.plate.idleWordAlphaByte == d.options.idleWordAlphaByte && p.plate.chosenWordAlphaByte == d.options.chosenWordAlphaByte, "and their dim and whole words");
    CHECK_MSG(p.plate.wordUnitsPerFontPx == 0.28 && p.plate.maxWidthUnits == 62.0, "a name at 0.28 a font px, fitted to 62 units");
}

// A table names no picture and no font file of its own (tools/package/closure.py would resolve the name as an original asset): the globe is a stem, and
// the only name that ends in .fnt is the title's role, a file of the original's that exists.
void ThePickerTableNamesNoPicture() {
    const std::string text = ReadAll(kPicker);
    CHECK_MSG(!text.empty(), "picker.json is there");
    std::size_t fnt = 0;
    for (std::size_t at = text.find(".fnt\""); at != std::string::npos; at = text.find(".fnt\"", at + 1)) ++fnt;
    CHECK_MSG(fnt == 1u, "one .fnt name (the title's role) and no other: " + std::to_string(fnt));
    for (const char* extension : {".png\"", ".bmp\"", ".jpg\"", ".ttf\"", ".ogg\"", ".mp3\""}) {
        CHECK_MSG(!Contains(text, extension), std::string("picker.json names no ") + extension);
    }
    CHECK_MSG(Contains(text, "\"_about\"") && Contains(text, "\"_source\""), "it says where its numbers come from");
    CHECK_MSG(text.find('\r') == std::string::npos && text.compare(0, 3, "\xEF\xBB\xBF") != 0, "LF line ends and no byte order mark");
}

void TheLoaderRefusesWhatIsNotATable() {
    const std::string text = ReadAll(kPicker);
    if (text.empty()) return;
    const fs::path dir = Scratch("picker-table");
    const auto load = [&](const std::string& body, std::string& error) {
        WriteFile(dir / "picker.json", body);
        Picker::Rules rules;
        return Picker::LoadRules((dir / "picker.json").string(), rules, error);
    };
    std::string error;
    CHECK_MSG(load(text, error), "the file as it is reads: " + error);
    struct Mutation {
        const char* find;
        const char* replace;
        const char* names; // what the refusal must name
    };
    const std::vector<Mutation> mutations = {
        {"\"glyph\": \"language_globe\"", "\"glyph\": \"language_globe.png\"", "entry.glyph"},
        {"\"glyph\": \"language_globe\"", "\"glyph\": \"../language_globe\"", "entry.glyph"},
        {"\"glyph\": \"language_globe\"", "\"glyph\": \"\"", "entry.glyph"},
        {"\"glyph_px\": 128", "\"glyph_px\": 8", "entry.glyph_px"},
        {"\"glyph_px\": 128", "\"glyph_px\": 128.5", "entry.glyph_px"},
        {"\"body_inset\": 0.109375", "\"body_inset\": 0.9", "entry.body_inset"},
        {"\"offset_units\": [0, 36]", "\"offset_units\": [0]", "entry.offset_units"},
        {"\"corner\": {", "\"corners\": {", "corner"},
        {"\"size_units\": [32, 32]\n  },\n  \"title\"", "\"size_units\": [0, 32]\n  },\n  \"title\"", "corner.size_units"},
        {"\"units_per_font_px\": 0.5", "\"units_per_font_px\": 0", "title.units_per_font_px"},
        {"\"y_at_screen\": 0.12", "\"y_at_screen\": 1.5", "title.y_at_screen"},
        {"\"max_width_units\": 200", "\"max_width_units\": -3", "title.max_width_units"},
        {"\"columns\": 3", "\"columns\": 0", "grid.columns"},
        {"\"columns\": 3", "\"columns\": 12", "grid.columns"},
        {"\"columns\": 3", "\"columns\": 2.5", "grid.columns"},
        {"\"gap_units\": [6, 8]", "\"gap_units\": [-1, 8]", "grid.gap_units"},
        {"\"plate_size_units\": [72, 34]", "\"plate_size_units\": [72, 0]", "grid.plate_size_units"},
        {"\"clear_left_units\": 84", "\"clear_left_units\": 600", "grid.clear_left_units"},
        {"\"idle_frame_rgba\": [34, 30, 52, 190]", "\"idle_frame_rgba\": [34, 30, 52, 300]", "plate.idle_frame_rgba"},
        {"\"idle_frame_rgba\": [34, 30, 52, 190]", "\"idle_frame_rgba\": [34, 30, 52]", "plate.idle_frame_rgba"},
        {"\"chosen_word_alpha_byte\": 255", "\"chosen_word_alpha_byte\": 256", "plate.chosen_word_alpha_byte"},
        {"\"word_units_per_font_px\": 0.28", "\"word_units_per_font_px\": \"big\"", "plate.word_units_per_font_px"},
    };
    for (const Mutation& m : mutations) {
        const std::size_t at = text.find(m.find);
        CHECK_MSG(at != std::string::npos, std::string("the mutation's text is in the file: ") + m.find);
        if (at == std::string::npos) continue;
        std::string mutated = text;
        mutated.replace(at, std::strlen(m.find), m.replace);
        error.clear();
        const bool read = load(mutated, error);
        CHECK_MSG(!read, std::string("refused: ") + m.replace);
        CHECK_MSG(Contains(error, m.names), std::string("and the refusal names ") + m.names + ": " + error);
    }
    CHECK_MSG(!load("not json", error) && !error.empty(), "text that is not JSON is refused");
    CHECK_MSG(!load("[]", error), "and so is a document that is not an object");
    Picker::Rules rules;
    CHECK_MSG(!Picker::LoadRules((dir / "absent.json").string(), rules, error) && Contains(error, "absent.json"), "a file that is not there is refused, by name: " + error);
    // A refused load leaves what the caller had.
    rules.grid.columns = 7;
    (void)load("{}", error);
    Picker::Rules untouched = rules;
    CHECK_MSG(!Picker::LoadRules((dir / "picker.json").string(), untouched, error) && untouched.grid.columns == 7, "a refused load leaves the rules as they were");
    std::error_code ec;
    fs::remove_all(dir, ec);
}

// ---- the grid ------------------------------------------------------------------------------------------------------------------------

void TheGridIsInsideTheViewAndClearOfTheBackButtonAtEveryAspect() {
    const Loaded& d = Data();
    if (!d.ok) return;
    const std::size_t count = Strings::kLangCount;
    CHECK_EQ(count, std::size_t{11});
    for (const Aspect& aspect : kAspects) {
        const glm::dvec2 view = ViewOf(aspect.ratio);
        const Hud::Rect whole{glm::dvec2(0.0), view};
        const Hud::Rect back = Credits::BackHitRect(d.credits, view, kSettled);
        std::vector<Hud::Rect> plates;
        for (std::size_t i = 0; i < count; ++i) plates.push_back(Picker::PlateRect(d.picker, view, i, count));
        for (std::size_t i = 0; i < count; ++i) {
            const std::string where = std::string(aspect.name) + " plate " + std::to_string(i) + " " + Show(plates[i]);
            CHECK_MSG(Inside(plates[i], whole), where + " is inside the view");
            CHECK_MSG(Gap(plates[i], back) >= 0.0, where + " is clear of the back button " + Show(back));
            CHECK_MSG(plates[i].size == d.picker.grid.plateSizeUnits, where + " is a plate");
            for (std::size_t j = i + 1; j < count; ++j) CHECK_MSG(Gap(plates[i], plates[j]) >= 0.0, where + " overlaps plate " + std::to_string(j));
        }
        // Three rows of three and a last of two, the last centred under the others.
        const Hud::Rect grid = Picker::GridRect(d.picker, view, count);
        CHECK_MSG(Inside(grid, whole), std::string(aspect.name) + ": the grid is inside the view " + Show(grid));
        CHECK_MSG(Near(grid.size.y, 4 * 34.0 + 3 * 8.0) && Near(grid.size.x, 3 * 72.0 + 2 * 6.0), std::string(aspect.name) + ": 228 x 160");
        CHECK_MSG(Near(plates[0].min.y, 62.0) && Near(plates[3].min.y, 62.0 + 42.0) && Near(plates[9].min.y, 62.0 + 3 * 42.0), std::string(aspect.name) + ": the rows");
        CHECK_MSG(Near(plates[0].min.x, grid.min.x) && Near(plates[2].Max().x, grid.Max().x), std::string(aspect.name) + ": a full row spans the grid");
        CHECK_MSG(Near((plates[9].min.x + plates[10].Max().x) * 0.5, grid.min.x + grid.size.x * 0.5), std::string(aspect.name) + ": the last row of two is centred");
        // Centred on the screen where it clears the button, and moved right only as far as it takes.
        const double centred = view.x * 0.5 - grid.size.x * 0.5;
        if (centred >= d.picker.grid.clearLeftUnits) {
            CHECK_MSG(Near(grid.min.x, centred), std::string(aspect.name) + ": centred on the screen");
        } else {
            CHECK_MSG(Near(grid.min.x, d.picker.grid.clearLeftUnits), std::string(aspect.name) + ": moved right to clear the back button, no further");
        }
        // The title is centred on the grid and above it: its 84 px line at 0.5 a px is 42 units tall.
        const glm::dvec2 title = Picker::TitleAt(d.picker, view, count);
        CHECK_MSG(Near(title.x, grid.min.x + grid.size.x * 0.5), std::string(aspect.name) + ": the title is centred on the grid");
        CHECK_MSG(title.y + 0.5 * kMatura84LineHeight * d.picker.title.unitsPerFontPx < grid.min.y, std::string(aspect.name) + ": and its line ends above the grid");
        CHECK_MSG(title.x - d.picker.title.maxWidthUnits * 0.5 >= 0.0 && title.x + d.picker.title.maxWidthUnits * 0.5 <= view.x, std::string(aspect.name) + ": and fits the view");
    }
    // A single plate or an empty row is still a layout.
    const glm::dvec2 view = ViewOf(16.0 / 9.0);
    CHECK_MSG(Inside(Picker::PlateRect(d.picker, view, 0, 1), Hud::Rect{glm::dvec2(0.0), view}), "one plate is laid out");
}

void TheGlobesAreClearOfEveryButtonTheyShareAScreenWithAtEveryAspect() {
    const Loaded& d = Data();
    if (!d.ok) return;
    const MagicPortals::Pause::Switches switches;
    for (const Aspect& aspect : kAspects) {
        const glm::dvec2 view = ViewOf(aspect.ratio);
        const Hud::Rect whole{glm::dvec2(0.0), view};
        const std::string a = aspect.name;
        // The main menu's globe, settled: inside the view, flush with the right edge, clear of the gear, and on no button of the main menu.
        const Picker::Entry globe = Picker::EntryAt(d.picker, d.menu.layer, d.menu.state, view, kSettled, false);
        CHECK_MSG(Inside(globe.rect, whole), a + ": the globe " + Show(globe.rect) + " is inside the view");
        CHECK_MSG(Near(globe.rect.Max().x, view.x) && Near(globe.rect.min.y, 36.0), a + ": flush right, 36 down");
        const Hud::Rect gear = Options::EntryAt(d.options, d.menu.layer, d.menu.state, view, kSettled, false).rect;
        CHECK_MSG(Gap(globe.rect, gear) >= 0.0, a + ": clear of the gear " + Show(gear));
        CHECK_MSG(Near(gear.Max().y, globe.rect.min.y - 4.0), a + ": 4 units under the gear");
        const glm::dvec2 centre = globe.rect.Centre();
        for (const glm::dvec2& point : {centre, globe.rect.min + glm::dvec2(0.01), globe.rect.Max() - glm::dvec2(0.01), glm::dvec2(globe.rect.min.x + 0.01, globe.rect.Max().y - 0.01),
                                        glm::dvec2(globe.rect.Max().x - 0.01, globe.rect.min.y + 0.01)}) {
            CHECK_MSG(MainMenu::ButtonsAt(d.menu, switches, view, kSettled, point) == 0u, a + ": no main menu button under the globe at " + Num(point.x) + ", " + Num(point.y));
        }
        // The picture is the box with the margin taken out, as the gear's is.
        CHECK_MSG(globe.picture.size.x > globe.rect.size.x && Near(globe.picture.Centre().x, globe.rect.Centre().x), a + ": its picture is its box with the margin");
        // Before its entrance has begun it is transparent, as every main menu button is; held, it is at the press tint.
        CHECK_MSG(Picker::EntryAt(d.picker, d.menu.layer, d.menu.state, view, 0.0, false).alphaByte == 0, a + ": invisible at the first frame");
        CHECK_MSG(Picker::EntryAt(d.picker, d.menu.layer, d.menu.state, view, kSettled, true).rgbByte == d.menu.state.pressTintByte &&
                      Picker::EntryAt(d.picker, d.menu.layer, d.menu.state, view, kSettled, false).rgbByte == 255,
                  a + ": held it is at the press tint");

        // The Options corner: inside, flush top-right, on none of the screen's widgets.
        const Hud::Rect corner = Picker::CornerRect(d.picker, view);
        CHECK_MSG(Inside(corner, whole) && Near(corner.Max().x, view.x) && Near(corner.min.y, 0.0), a + ": the corner is the top-right 32 units: " + Show(corner));
        for (const Controls::Style style : Options::kTileStyles) CHECK_MSG(Gap(corner, Options::TileRect(d.options, style, view)) > 0.0, a + ": clear of the tiles");
        CHECK_MSG(Gap(corner, Options::AdjustRect(d.options, view)) > 0.0, a + ": clear of the Adjust button");
        CHECK_MSG(Gap(corner, Credits::BackHitRect(d.credits, view, kSettled)) > 0.0, a + ": clear of the back button");
        for (const glm::dvec2& point : {corner.Centre(), corner.min + glm::dvec2(0.01), corner.Max() - glm::dvec2(0.01)}) {
            CHECK_MSG(Options::WidgetAt(d.options, d.credits, view, kSettled, point, true) == Options::Widget::None, a + ": no Options widget under the corner");
        }
        // The title of Options is centred (its centre is at 0.5 of the width), and the corner is at least 60 units right of that centre: a title no wider than 120
        // units, centred, cannot reach it. Arithmetic, not a measured ink.
        CHECK_MSG(corner.min.x > view.x * d.options.title.atScreen.x + 60.0, a + ": and 60 units or more right of the title's centre");
        const Picker::Entry held = Picker::CornerAt(d.picker, d.menu.state, view, true);
        CHECK_MSG(held.rect.min == corner.min && held.rgbByte == d.menu.state.pressTintByte && held.alphaByte == 255, a + ": the corner held is at the press tint, whole");
    }
}

// ---- the touch -------------------------------------------------------------------------------------------------------------------------

void TheTouchActsOnTheReleaseInsideWhereItWentDown() {
    using Kind = Picker::Target::Kind;
    const Picker::Target none;
    const Picker::Target back{Kind::Back, 0};
    const Picker::Target plate3{Kind::Plate, 3};
    const Picker::Target plate4{Kind::Plate, 4};
    const Picker::Target plateThreeAgain{Kind::Plate, 3};
    const Picker::Target backOther{Kind::Back, 7};
    CHECK_MSG(plate3 == plateThreeAgain && plate3 != plate4 && back != none && back == backOther, "targets compare by kind, and by index for a plate alone");

    {   // A press and a release on the same plate is a press of it, and the touch is forgotten.
        Picker::Touch touch;
        Picker::Press press = Picker::Follow(touch, true, false, true, plate3);
        CHECK_MSG(!press.happened && touch.held == plate3 && touch.down == plate3, "pressed: held at the press tint");
        press = Picker::Follow(touch, false, false, true, plate3);
        CHECK_MSG(!press.happened && touch.held == plate3, "held on");
        press = Picker::Follow(touch, false, true, false, plate3);
        CHECK_MSG(press.happened && press.target == plate3, "released inside: pressed");
        CHECK_MSG(touch.down == none && touch.held == none, "and forgotten");
    }
    {   // Let go outside it: nothing.
        Picker::Touch touch;
        (void)Picker::Follow(touch, true, false, true, plate3);
        Picker::Press press = Picker::Follow(touch, false, false, true, plate4);
        CHECK_MSG(!press.happened && touch.held == none && touch.down == plate3, "slid onto another plate: not held (and not that plate's either)");
        press = Picker::Follow(touch, false, true, false, plate4);
        CHECK_MSG(!press.happened, "released on another plate: nothing");
    }
    {   // A press that begins elsewhere and slides onto a plate does nothing.
        Picker::Touch touch;
        (void)Picker::Follow(touch, true, false, true, none);
        (void)Picker::Follow(touch, false, false, true, plate3);
        const Picker::Press press = Picker::Follow(touch, false, true, false, plate3);
        CHECK_MSG(!press.happened, "a touch that began on nothing presses nothing");
    }
    {   // Press and release in one tick are both of them.
        Picker::Touch touch;
        const Picker::Press press = Picker::Follow(touch, true, true, false, back);
        CHECK_MSG(press.happened && press.target == back, "a tap in one tick presses the back button");
    }
    {   // Slide off and back on: held again, and the release inside acts.
        Picker::Touch touch;
        (void)Picker::Follow(touch, true, false, true, plate3);
        (void)Picker::Follow(touch, false, false, true, none);
        CHECK_MSG(touch.held == none, "off it: not held");
        (void)Picker::Follow(touch, false, false, true, plate3);
        CHECK_MSG(touch.held == plate3, "back on it: held");
        CHECK_MSG(Picker::Follow(touch, false, true, false, plate3).happened, "and the release acts");
    }
}

void TheTargetUnderAPointIsTheBackButtonAPlateOrNothing() {
    const Loaded& d = Data();
    if (!d.ok) return;
    using Kind = Picker::Target::Kind;
    const std::size_t count = Strings::kLangCount;
    for (const Aspect& aspect : kAspects) {
        const glm::dvec2 view = ViewOf(aspect.ratio);
        for (std::size_t i = 0; i < count; ++i) {
            const Hud::Rect rect = Picker::PlateRect(d.picker, view, i, count);
            const Picker::Target on = Picker::TargetAt(d.picker, d.credits, view, kSettled, rect.Centre(), count);
            CHECK_MSG(on.kind == Kind::Plate && on.index == static_cast<int>(i), std::string(aspect.name) + ": the centre of plate " + std::to_string(i) + " is that plate");
            // Closed on every edge, as Hud::Rect::Contains.
            CHECK_MSG(Picker::TargetAt(d.picker, d.credits, view, kSettled, rect.min, count) == on, std::string(aspect.name) + ": its corner is it too");
        }
        CHECK_MSG(Picker::TargetAt(d.picker, d.credits, view, kSettled, Credits::BackHitRect(d.credits, view, kSettled).Centre(), count).kind == Kind::Back,
                  std::string(aspect.name) + ": the back button");
        // Between two plates, and the middle of the screen's top-left corner, is nothing.
        const Hud::Rect first = Picker::PlateRect(d.picker, view, 0, count);
        const Hud::Rect second = Picker::PlateRect(d.picker, view, 1, count);
        const glm::dvec2 between((first.Max().x + second.min.x) * 0.5, first.Centre().y);
        CHECK_MSG(Picker::TargetAt(d.picker, d.credits, view, kSettled, between, count).kind == Kind::None, std::string(aspect.name) + ": the gap between two plates is nothing");
        CHECK_MSG(Picker::TargetAt(d.picker, d.credits, view, kSettled, glm::dvec2(1.0, 1.0), count).kind == Kind::None, std::string(aspect.name) + ": nor is the corner");
    }
}

// ---- the pieces ---------------------------------------------------------------------------------------------------------------------------

void ThePiecesAreInDrawingOrderAndTheChosenPlateIsLit() {
    const Loaded& d = Data();
    if (!d.ok) return;
    using Element = Picker::Element;
    using Kind = Picker::Target::Kind;
    std::vector<std::string> names;
    for (std::size_t i = 0; i < Strings::kLangCount; ++i) names.push_back("name" + std::to_string(i));
    const glm::dvec2 view = ViewOf(16.0 / 9.0);
    const std::vector<Picker::Piece> pieces = Picker::Pieces(d.picker, d.credits, view, kSettled, 3, Picker::Target{}, "Title", names);
    CHECK_EQ(pieces.size(), std::size_t{2 + 1 + 2 * 11});
    if (pieces.size() != 25u) return;
    CHECK_MSG(pieces[0].element == Element::Background && pieces[0].file == d.credits.background.sprite, "the credits' background first");
    CHECK_MSG(pieces[1].element == Element::Back && pieces[1].file == d.credits.back.sprite, "then its back button");
    CHECK_MSG(pieces[2].element == Element::Title && pieces[2].words == "Title" && pieces[2].font == d.picker.title.font &&
                  pieces[2].unitsPerFontPx == d.picker.title.unitsPerFontPx && pieces[2].maxWidthUnits == d.picker.title.maxWidthUnits,
              "then the title, as it was handed in");
    CHECK_MSG(pieces[2].rect.min == Picker::TitleAt(d.picker, view, 11) && pieces[2].rect.size == glm::dvec2(0.0), "centred where the grid says");
    for (std::size_t i = 0; i < 11; ++i) {
        const Picker::Piece& frame = pieces[3 + 2 * i];
        const Picker::Piece& word = pieces[4 + 2 * i];
        CHECK_MSG(frame.element == Element::PlateFrame && frame.plate == static_cast<int>(i) && frame.rect.min == Picker::PlateRect(d.picker, view, i, 11).min,
                  "plate " + std::to_string(i) + "'s frame");
        CHECK_MSG(word.element == Element::PlateWord && word.plate == static_cast<int>(i) && word.words == names[i] && word.font == Picker::kNamesFont &&
                      word.unitsPerFontPx == d.picker.plate.wordUnitsPerFontPx && word.maxWidthUnits == d.picker.plate.maxWidthUnits &&
                      word.rect.Centre() == Picker::PlateRect(d.picker, view, i, 11).Centre(),
                  "and its name, in the names' font, on the plate's centre");
        const bool lit = i == 3;
        CHECK_MSG(frame.colour == (lit ? d.picker.plate.chosenFrame : d.picker.plate.idleFrame), "plate " + std::to_string(i) + (lit ? " is lit" : " is idle"));
        CHECK_MSG(word.colour[3] == (lit ? d.picker.plate.chosenWordAlphaByte : d.picker.plate.idleWordAlphaByte), "and so is its name");
    }
    // A held plate is multiplied by the press tint; the others are not.
    {
        const Picker::Target held{Kind::Plate, 5};
        const std::vector<Picker::Piece> tinted = Picker::Pieces(d.picker, d.credits, view, kSettled, 3, held, "Title", names);
        const int tint = d.credits.state.pressTintByte;
        CHECK_MSG(tinted[3 + 2 * 5].colour[0] == d.picker.plate.idleFrame[0] * tint / 255 && tinted[3 + 2 * 5].colour[3] == d.picker.plate.idleFrame[3], "the held plate's frame at the press tint, its alpha whole");
        CHECK_MSG(tinted[4 + 2 * 5].colour[0] == 255 * tint / 255, "and its name");
        CHECK_MSG(tinted[3 + 2 * 4].colour == d.picker.plate.idleFrame && tinted[3 + 2 * 3].colour == d.picker.plate.chosenFrame, "the others as they were");
        const std::vector<Picker::Piece> back = Picker::Pieces(d.picker, d.credits, view, kSettled, 3, Picker::Target{Kind::Back, 0}, "Title", names);
        CHECK_MSG(back[1].colour[0] == tint && back[3].colour == d.picker.plate.idleFrame, "the back button held is at the press tint and no plate is");
    }
}

// ---- the atlas -------------------------------------------------------------------------------------------------------------------------

// The eleven names baked from the committed faces: every glyph there (a hole in a name is a plate that cannot be read), and none so wide that its plate
// scales it below a readable half of the rule's size.
void TheAtlasOfTheNamesHoldsEveryNameAndEveryNameFitsItsPlate() {
    const Loaded& d = Data();
    if (!d.ok) return;
    const FontBake::Plan& plan = d.languages.Fonts();
    const FontBake::SlotSpec* slot = plan.Slot(plan.pickerSlot);
    CHECK_MSG(slot != nullptr, "languages.json names a picker slot: " + plan.pickerSlot);
    if (slot == nullptr) return;
    std::vector<FontBake::FaceBytes> faces;
    std::string error;
    CHECK_MSG(FontBake::LoadFaces(plan, kData + "/" + plan.folder, faces, error), "the committed faces read: " + error);
    const fs::path dir = Scratch("picker-atlas");
    const std::set<std::uint32_t> wanted = Strings::PickerCodePoints(d.languages);
    CHECK_MSG(wanted.size() > 30u, "the names hold " + std::to_string(wanted.size()) + " different code points");
    const FontBake::Written written = FontBake::Ensure(plan, *slot, faces, wanted, dir.string(), slot->originalFont + FontBake::kPickerSuffix);
    CHECK_MSG(written.ok, "the atlas bakes: " + written.error);
    if (!written.ok) return;
    CHECK_MSG(written.missing.empty(), "no face lacks a glyph of a name: " + std::to_string(written.missing.size()) + " missing");
    BitmapFont font;
    CHECK_MSG(font.Load(written.fntPath, error), "the engine reads it: " + error);
    // And a second Ensure finds it there.
    CHECK_MSG(FontBake::Ensure(plan, *slot, faces, wanted, dir.string(), slot->originalFont + FontBake::kPickerSuffix).cached, "baked once: the second call finds it");
    double narrowest = 1.0e9;
    for (std::size_t i = 0; i < Strings::kLangCount; ++i) {
        const auto lang = static_cast<Lang>(i);
        const std::string name = Strings::Display(lang, d.languages.NativeName(lang));
        std::vector<std::uint32_t> missing;
        const std::vector<Hud::Glyph> glyphs = Hud::LayOutText(font, name, glm::dvec2(0.0), 1.0, &missing);
        CHECK_MSG(missing.empty(), d.languages.CodeOf(lang) + ": the atlas has every glyph of the name");
        CHECK_MSG(glyphs.size() == Utf8::Decode(name).size(), d.languages.CodeOf(lang) + ": and draws one for each code point: " + std::to_string(glyphs.size()));
        const double fitted = Hud::FitUnitsPerFontPx(font, name, d.picker.plate.wordUnitsPerFontPx, d.picker.plate.maxWidthUnits);
        const double scale = fitted / d.picker.plate.wordUnitsPerFontPx;
        narrowest = std::min(narrowest, scale);
        CHECK_MSG(scale >= 0.5, d.languages.CodeOf(lang) + ": the name needs " + Num(scale) + " of its size to fit its plate (a half is the least that reads)");
        const double width = static_cast<double>(Hud::MeasureText(font, name).x) * fitted;
        CHECK_MSG(width <= d.picker.plate.maxWidthUnits + 1e-6 && width <= d.picker.grid.plateSizeUnits.x, d.languages.CodeOf(lang) + ": and is no wider than its plate");
    }
    std::printf("  the tightest name is drawn at %.2f of the rule's size\n", narrowest);
    std::error_code ec;
    fs::remove_all(dir, ec);
}

void TheGlobeIsADrawnGlyphAndTheStemIsBare() {
    // The game draws the globe: the glyph of the entry's size and margin is a bitmap, and the picture is the same bytes every time.
    const Loaded& d = Data();
    if (!d.ok) return;
    const PadArt::Bitmap a = PadArt::Draw(PadArt::Glyph::Globe, d.picker.entry.glyphPx, d.picker.entry.bodyInset);
    const PadArt::Bitmap b = PadArt::Draw(PadArt::Glyph::Globe, d.picker.entry.glyphPx, d.picker.entry.bodyInset);
    CHECK_MSG(!a.Empty() && a.size == d.picker.entry.glyphPx && a.rgba == b.rgba, "the globe is drawn, and the same each time");
    CHECK_MSG(std::string(PadArt::NameOf(PadArt::Glyph::Globe)) == "globe", "its name is globe");
    CHECK_MSG(a.rgba != PadArt::Draw(PadArt::Glyph::Gear, d.picker.entry.glyphPx, d.picker.entry.bodyInset).rgba, "and it is not the gear");
    CHECK_MSG(std::string(Picker::kGlyphExtension) == ".png", "the file the layer writes ends in .png");
    CHECK_MSG(!Contains(d.picker.entry.glyph, ".") && !Contains(d.picker.entry.glyph, "/"), "from a stem with no dot and no slash");
}

void runTests() {
    ThePickerTableIsWhatTheFileSays();
    ThePickerTableNamesNoPicture();
    TheLoaderRefusesWhatIsNotATable();
    TheGridIsInsideTheViewAndClearOfTheBackButtonAtEveryAspect();
    TheGlobesAreClearOfEveryButtonTheyShareAScreenWithAtEveryAspect();
    TheTouchActsOnTheReleaseInsideWhereItWentDown();
    TheTargetUnderAPointIsTheBackButtonAPlateOrNothing();
    ThePiecesAreInDrawingOrderAndTheChosenPlateIsLit();
    TheAtlasOfTheNamesHoldsEveryNameAndEveryNameFitsItsPlate();
    TheGlobeIsADrawnGlyphAndTheStemIsBare();
}

} // namespace

MP_TEST_MAIN("test_mp_picker", 300)

// ============================================================================================================================
// test_mp_pickerlayer - the layer
// ============================================================================================================================

namespace {

const std::string kLevels = MAGICPORTALS_LEVELS_DIR;
const std::string kChapters = MAGICPORTALS_CHAPTERS_FILE;
const glm::vec2 kRest(640.0f, 360.0f);

void publishViewport(entt::registry& registry, const glm::vec2& window = glm::vec2(1280.0f, 720.0f)) {
    ViewportInfo viewport;
    viewport.rect.min = glm::vec2(0.0f, 0.0f);
    viewport.rect.max = window;
    viewport.pointerOverGame = true;
    registry.ctx().insert_or_assign(viewport);
}

void tickWith(MagicPortalsLayer& layer, entt::registry& registry, const glm::vec2& pointer, std::vector<std::string> down, std::vector<std::string> pressed) {
    PhysicsSystem::Update(registry, MagicPortalsLayer::kTick);
    Input::TickInput input;
    input.mousePosition = pointer;
    input.down = std::move(down);
    input.pressed = std::move(pressed);
    Input::BeginReplayedTick(input);
    layer.OnFixedUpdate(registry, MagicPortalsLayer::kTick);
    Input::EndReplayedTick();
}

void releaseWith(MagicPortalsLayer& layer, entt::registry& registry, const glm::vec2& pointer) {
    PhysicsSystem::Update(registry, MagicPortalsLayer::kTick);
    Input::TickInput input;
    input.mousePosition = pointer;
    input.released = {MagicPortalsLayer::kTap};
    Input::BeginReplayedTick(input);
    layer.OnFixedUpdate(registry, MagicPortalsLayer::kTick);
    Input::EndReplayedTick();
}

void rest(MagicPortalsLayer& layer, entt::registry& registry, int ticks) {
    for (int tick = 0; tick < ticks; ++tick) tickWith(layer, registry, kRest, {}, {});
}

bool tickToTheMainMenu(MagicPortalsLayer& layer, entt::registry& registry) {
    for (int tick = 0; tick < 400 && layer.MenuScreen() != MagicPortalsLayer::Screen::Main; ++tick) tickWith(layer, registry, kRest, {}, {});
    return layer.MenuScreen() == MagicPortalsLayer::Screen::Main;
}

std::vector<ScreenOverlay::Quad> menuFrame(entt::registry& registry, const MagicPortalsLayer& layer) {
    static ScreenOverlay overlay;
    overlay.Clear();
    registry.ctx().insert_or_assign<ScreenOverlay*>(&overlay);
    layer.EmitMenu(registry);
    return overlay.Quads();
}

// A touch down and up at a point of the menu's box (units), and the tick on which what it asked for opens; then a rest, so the screen's entrance is over.
void click(MagicPortalsLayer& layer, entt::registry& registry, const glm::dvec2& onView) {
    const glm::vec2 px(static_cast<float>(onView.x * kPxPerUnit), static_cast<float>(onView.y * kPxPerUnit));
    tickWith(layer, registry, px, {MagicPortalsLayer::kTap}, {MagicPortalsLayer::kTap});
    releaseWith(layer, registry, px);
    tickWith(layer, registry, kRest, {}, {});
    rest(layer, registry, 70);
}

// The same, as the pointer path takes a PRESS that is let go elsewhere: nothing may happen.
void pressAndSlideOff(MagicPortalsLayer& layer, entt::registry& registry, const glm::dvec2& onView, const glm::dvec2& offView) {
    const glm::vec2 px(static_cast<float>(onView.x * kPxPerUnit), static_cast<float>(onView.y * kPxPerUnit));
    const glm::vec2 off(static_cast<float>(offView.x * kPxPerUnit), static_cast<float>(offView.y * kPxPerUnit));
    tickWith(layer, registry, px, {MagicPortalsLayer::kTap}, {MagicPortalsLayer::kTap});
    releaseWith(layer, registry, off);
    rest(layer, registry, 70);
}

bool originalArtIsThere() {
    std::error_code ec;
    return fs::is_directory(std::string(MAGICPORTALS_ORIGINAL_DIR) + "/entities", ec);
}

bool dataIsThere(const char* test) {
    std::error_code ec;
    if (!fs::is_directory(kLevels, ec) || !fs::is_regular_file(kChapters, ec) || !originalArtIsThere()) {
        std::printf("  %s SKIPPED - needs the converted levels, chapters.json and the original's extracted assets\n", test);
        return false;
    }
    return true;
}

// A player's run: the picker offered, a save folder of its own, a clean scratch folder. `modern` adds the feature the gear needs.
MagicPortalsLayer::Paths playerPaths(const std::string& name, bool modern, bool offered = true) {
    MagicPortalsLayer::Paths paths;
    paths.prisms = fs::temp_directory_path() / ("supersonic-test-mp-picker-" + name);
    std::error_code ec;
    fs::remove_all(paths.prisms, ec);
    paths.language.picker = offered;
    paths.saveDir = Scratch("picker-" + name + "-saves").string();
    if (modern) {
        paths.controls = Controls::ResolvePolicy(std::string("modern"), false, false, false);
        paths.controls.withoutOriginalArt = true;
    }
    return paths;
}

struct Run {
    entt::registry registry;
    MagicPortalsLayer layer;
    explicit Run(const MagicPortalsLayer::Paths& paths) : layer(paths, "") {
        publishViewport(registry);
        layer.OnAttach(registry);
    }
    ~Run() { layer.OnDetach(registry); }
    Run(const Run&) = delete;
    Run& operator=(const Run&) = delete;
};

std::vector<const ScreenOverlay::Quad*> glyphQuadsOf(const std::vector<ScreenOverlay::Quad>& quads) {
    std::vector<const ScreenOverlay::Quad*> found;
    for (const ScreenOverlay::Quad& quad : quads) {
        if (quad.texture.find("Matura") != std::string::npos || quad.texture.find("Verdana") != std::string::npos) found.push_back(&quad);
    }
    return found;
}

// The glyph quads of a frame whose centre lies inside `box` (units of the menu's box): how many, and how far they reach.
struct Span {
    double left = 1.0e9;
    double right = -1.0e9;
    std::size_t glyphs = 0;
    double Width() const { return glyphs > 0 ? right - left : 0.0; }
};

Span spanInside(const std::vector<ScreenOverlay::Quad>& frame, const glm::dvec2& view, const Hud::Rect& box) {
    Span span;
    for (const ScreenOverlay::Quad* quad : glyphQuadsOf(frame)) {
        const glm::dvec2 centre = (glm::dvec2(quad->min) + glm::dvec2(quad->max)) * 0.5 * view;
        if (!box.Contains(centre)) continue;
        span.left = std::min(span.left, static_cast<double>(quad->min.x) * view.x);
        span.right = std::max(span.right, static_cast<double>(quad->max.x) * view.x);
        ++span.glyphs;
    }
    return span;
}

bool sameColour(const glm::vec4& quad, const Picker::Colour& colour) {
    return std::fabs(quad.r * 255.0f - static_cast<float>(colour[0])) < 0.6f && std::fabs(quad.g * 255.0f - static_cast<float>(colour[1])) < 0.6f &&
           std::fabs(quad.b * 255.0f - static_cast<float>(colour[2])) < 0.6f && std::fabs(quad.a * 255.0f - static_cast<float>(colour[3])) < 0.6f;
}

// Every plate of the picker on the screen: its frame is there (lit if it is the language on the screen), its name is drawn with every glyph and from the
// picker's atlas, and the title has the language's own letters. `what` says which case.
void checkThePickerIsDrawn(MagicPortalsLayer& layer, entt::registry& registry, const std::string& what) {
    const Picker::Rules& rules = layer.PickerRules();
    const std::vector<ScreenOverlay::Quad> frame = menuFrame(registry, layer);
    const glm::dvec2 view = layer.MenuBoxPx();
    const std::size_t count = layer.PickerNames().size();
    CHECK_MSG(count == Strings::kLangCount, what + ": eleven names");
    int lit = 0;
    int idle = 0;
    for (const ScreenOverlay::Quad& quad : frame) {
        if (!quad.texture.empty()) continue;
        if (sameColour(quad.color, rules.plate.chosenFrame)) ++lit;
        if (sameColour(quad.color, rules.plate.idleFrame)) ++idle;
    }
    CHECK_MSG(lit == 1 && idle == static_cast<int>(count) - 1, what + ": one plate is lit and ten are idle: " + std::to_string(lit) + " lit, " + std::to_string(idle) + " idle");
    std::size_t drawn = 0;
    for (std::size_t i = 0; i < count; ++i) {
        const Hud::Rect rect = Picker::PlateRect(rules, view, i, count);
        const Span span = spanInside(frame, view, rect);
        const std::size_t expected = Utf8::Decode(layer.PickerNames()[i]).size();
        CHECK_MSG(span.glyphs == expected, what + ": plate " + std::to_string(i) + " draws all " + std::to_string(expected) + " letters of its name, not " + std::to_string(span.glyphs));
        // The shadow is padding in the glyph's cell, not advance: a few units over the limit is ink that is not there.
        CHECK_MSG(span.Width() <= rules.plate.maxWidthUnits + 8.0, what + ": plate " + std::to_string(i) + "'s name is " + Num(span.Width()) + " wide, over its " + Num(rules.plate.maxWidthUnits));
        drawn += span.glyphs;
    }
    // The names are from the picker's atlas and from no other font: every glyph quad within a plate names a page of it.
    std::size_t fromAtlas = 0;
    for (const ScreenOverlay::Quad* quad : glyphQuadsOf(frame)) {
        if (quad->texture.find(std::string(FontBake::kPickerSuffix) + "_") != std::string::npos) ++fromAtlas;
    }
    CHECK_MSG(fromAtlas == drawn, what + ": every letter of every plate is from the picker's atlas: " + std::to_string(fromAtlas) + " of " + std::to_string(drawn));
    // The title: the language's own word, in the font the language is drawn in, centred over the grid.
    const BitmapFont* font = layer.UiFontOf(rules.title.font);
    CHECK_MSG(font != nullptr, what + ": the title's font loads");
    if (font == nullptr) return;
    const std::string title = layer.StringTable().Show(Strings::Key::kLanguageTitle);
    const glm::dvec2 at = Picker::TitleAt(rules, view, count);
    const Hud::Rect band{glm::dvec2(at.x - rules.title.maxWidthUnits, at.y - 30.0), glm::dvec2(rules.title.maxWidthUnits * 2.0, 60.0)};
    const Span heading = spanInside(frame, view, band);
    CHECK_MSG(heading.glyphs == Hud::LayOutText(*font, title, glm::dvec2(0.0), 1.0).size() && heading.glyphs > 0u, what + ": the title '" + title + "' is drawn: " + std::to_string(heading.glyphs) + " letters");
    CHECK_MSG(heading.Width() <= rules.title.maxWidthUnits + 8.0, what + ": and inside its limit");
}

void ABareLayerOffersNoPicker() {
    if (!dataIsThere("ABareLayerOffersNoPicker")) return;
    Run run(playerPaths("bare", false, false));
    MagicPortalsLayer& layer = run.layer;
    if (!layer.LoadError().empty() || !tickToTheMainMenu(layer, run.registry)) {
        CHECK_MSG(false, "no main menu: " + layer.LoadError());
        return;
    }
    rest(layer, run.registry, 60);
    CHECK_MSG(!layer.LanguageAvailable() && layer.LanguageEntryImage().empty() && layer.PickerRules().entry.glyph.empty(), "a run that does not offer the picker reads nothing of it");
    CHECK_MSG(!Language::Policy{}.picker, "and that is the policy of every bare Paths");
    const std::vector<ScreenOverlay::Quad> bare = menuFrame(run.registry, layer);
    // Where a globe would be, a tap does nothing.
    Picker::Rules rules;
    std::string error;
    CHECK_MSG(Picker::LoadRules(kPicker, rules, error), error);
    const Hud::Rect where = Picker::EntryAt(rules, layer.MainMenuRules().layer, layer.MainMenuRules().state, layer.ViewPx(), 1.0e9, false).rect;
    click(layer, run.registry, where.Centre());
    CHECK_MSG(layer.MenuScreen() == MagicPortalsLayer::Screen::Main, "a tap where the globe would be stays on the main menu");
    // And the same menu, offered the picker, has exactly one quad more: the globe.
    Run offered(playerPaths("bare-offered", false));
    if (!tickToTheMainMenu(offered.layer, offered.registry)) return;
    rest(offered.layer, offered.registry, 60);
    CHECK_MSG(menuFrame(offered.registry, offered.layer).size() == bare.size() + 1u, "offered the picker, the main menu has exactly one quad more: the globe");
}

void TheGlobeOpensThePickerWhichIsDrawnWithoutAHoleInEveryLanguage() {
    if (!dataIsThere("TheGlobeOpensThePickerWhichIsDrawnWithoutAHoleInEveryLanguage")) return;
    // A classic run: the controls feature does not exist in it, so there is no gear and no Options. The picker does not depend on either.
    Run run(playerPaths("main", false));
    MagicPortalsLayer& layer = run.layer;
    if (!layer.LoadError().empty() || !tickToTheMainMenu(layer, run.registry)) {
        CHECK_MSG(false, "no main menu: " + layer.LoadError());
        return;
    }
    rest(layer, run.registry, 60);
    CHECK_MSG(!layer.OptionsAvailable(), "the run has no Options");
    CHECK_MSG(layer.LanguageAvailable(), "and the picker is offered all the same");
    CHECK_MSG(!layer.LanguageEntryImage().empty() && fs::is_regular_file(layer.LanguageEntryImage()) && layer.LanguageEntryImage().find("language_globe.png") != std::string::npos,
              "the globe is a file the game wrote: " + layer.LanguageEntryImage());
    CHECK_MSG(layer.PickerFontFile().empty(), "the names' atlas is not baked at the start");
    const MagicPortalsLayer& constLayer = layer;
    const Picker::Entry globe = Picker::EntryAt(layer.PickerRules(), layer.MainMenuRules().layer, layer.MainMenuRules().state, layer.ViewPx(), 1.0e9, false);
    {
        const std::vector<ScreenOverlay::Quad> frame = menuFrame(run.registry, constLayer);
        int found = 0;
        for (const ScreenOverlay::Quad& quad : frame) {
            if (quad.texture != layer.LanguageEntryImage()) continue;
            ++found;
            const glm::dvec2 view = layer.MenuBoxPx();
            CHECK_MSG(Near(quad.min.x * view.x, globe.picture.min.x, 1e-3) && Near(quad.max.y * view.y, globe.picture.Max().y, 1e-3), "the globe is drawn on its box, by its picture");
        }
        CHECK_MSG(found == 1, "the main menu draws the globe once");
    }
    // A press that is let go elsewhere opens nothing; a tap opens the picker.
    pressAndSlideOff(layer, run.registry, globe.rect.Centre(), glm::dvec2(100.0, 100.0));
    CHECK_MSG(layer.MenuScreen() == MagicPortalsLayer::Screen::Main, "a press on the globe let go elsewhere opens nothing");
    click(layer, run.registry, globe.rect.Centre());
    CHECK_MSG(layer.MenuScreen() == MagicPortalsLayer::Screen::Language, "a tap on the globe opens the picker");
    CHECK_MSG(layer.LanguageReturnsTo() == MagicPortalsLayer::Screen::Main, "which returns to the main menu");
    CHECK_MSG(!layer.PickerFontFile().empty() && fs::is_regular_file(layer.PickerFontFile()), "the atlas was baked on entry: " + layer.PickerFontFile());
    CHECK_MSG(layer.CurrentLanguage() == Lang::En, "in English");
    checkThePickerIsDrawn(layer, run.registry, "en");
    // The picker is drawn in every language, and the plates are the same eleven: the atlas is no language's.
    for (const Lang lang : {Lang::Ar, Lang::Ja, Lang::Ru, Lang::De, Lang::En}) {
        const std::string code(Strings::kLangCodes[Strings::IndexOf(lang)]);
        CHECK_MSG(layer.ChooseLanguage(lang) && layer.CurrentLanguage() == lang, code + ": chosen");
        checkThePickerIsDrawn(layer, run.registry, code);
    }
}

void ATapOnAPlateChoosesAndKeepsTheLanguage() {
    if (!dataIsThere("ATapOnAPlateChoosesAndKeepsTheLanguage")) return;
    const MagicPortalsLayer::Paths paths = playerPaths("choose", false);
    const fs::path saved = fs::path(paths.saveDir) / Language::kStoreFileName;
    {
        Run run(paths);
        MagicPortalsLayer& layer = run.layer;
        if (!layer.LoadError().empty() || !tickToTheMainMenu(layer, run.registry)) {
            CHECK_MSG(false, "no main menu: " + layer.LoadError());
            return;
        }
        rest(layer, run.registry, 60);
        CHECK_MSG(!fs::exists(saved), "nothing is saved before the player chooses");
        click(layer, run.registry, Picker::EntryAt(layer.PickerRules(), layer.MainMenuRules().layer, layer.MainMenuRules().state, layer.ViewPx(), 1.0e9, false).rect.Centre());
        CHECK_MSG(layer.MenuScreen() == MagicPortalsLayer::Screen::Language, "the picker is open");
        const glm::dvec2 view = layer.MenuBoxPx();
        const auto plateOf = [&](Lang lang) { return Picker::PlateRect(layer.PickerRules(), view, Strings::IndexOf(lang), Strings::kLangCount).Centre(); };
        // Pressing the lit one is a choice too (and kept); a press let go off a plate is none.
        pressAndSlideOff(layer, run.registry, plateOf(Lang::De), glm::dvec2(10.0, 10.0));
        CHECK_MSG(layer.CurrentLanguage() == Lang::En && !fs::exists(saved), "a press on a plate let go elsewhere chooses nothing");
        for (const Lang lang : {Lang::De, Lang::Ja, Lang::Ar, Lang::Ru}) {
            const std::string code(Strings::kLangCodes[Strings::IndexOf(lang)]);
            click(layer, run.registry, plateOf(lang));
            CHECK_MSG(layer.CurrentLanguage() == lang, code + ": a tap on the plate speaks the language at once");
            CHECK_MSG(layer.MenuScreen() == MagicPortalsLayer::Screen::Language, code + ": and the screen stays");
            CHECK_MSG(layer.LanguageStore().Chosen() == lang, code + ": the store holds it");
            CHECK_MSG(fs::is_regular_file(saved) && Contains(ReadAll(saved), "\"" + code + "\""), code + ": and language.json says so: " + ReadAll(saved));
            // The fonts of the language, and the plates in them: a generated language's fonts do not touch the picker's.
            CHECK_MSG(layer.GeneratedFonts() == (layer.StringTable().Languages().FontSetOf(lang) == Strings::FontSet::Generated), code + ": and its own fonts");
            checkThePickerIsDrawn(layer, run.registry, code);
        }
        // Back to the main menu by the back button.
        click(layer, run.registry, Credits::BackHitRect(layer.CreditsRules(), view, 1.0e9).Centre());
        CHECK_MSG(layer.MenuScreen() == MagicPortalsLayer::Screen::Main, "the back button returns to the main menu");
        CHECK_MSG(layer.CurrentLanguage() == Lang::Ru, "in the language chosen");
    }
    // A fresh layer on the same save folder starts in it, with the choice still there.
    {
        Run run(paths);
        CHECK_MSG(run.layer.CurrentLanguage() == Lang::Ru && run.layer.LanguageStore().FromFile(), "a fresh layer starts in the language the player chose");
        CHECK_MSG(run.layer.GeneratedFonts(), "with its fonts baked");
        // And the player can go back to English.
        if (tickToTheMainMenu(run.layer, run.registry)) {
            rest(run.layer, run.registry, 60);
            click(run.layer, run.registry, Picker::EntryAt(run.layer.PickerRules(), run.layer.MainMenuRules().layer, run.layer.MainMenuRules().state, run.layer.ViewPx(), 1.0e9, false).rect.Centre());
            click(run.layer, run.registry, Picker::PlateRect(run.layer.PickerRules(), run.layer.MenuBoxPx(), 0, Strings::kLangCount).Centre());
            CHECK_MSG(run.layer.CurrentLanguage() == Lang::En && Contains(ReadAll(saved), "\"en\""), "the first plate is English, and it is kept");
        }
    }
    {
        Run run(paths);
        CHECK_MSG(run.layer.CurrentLanguage() == Lang::En, "English chosen back is English at the next start");
    }
}

void TheBackKeyAndTheCornerGlobeReturnToTheScreenThatOpenedThePicker() {
    if (!dataIsThere("TheBackKeyAndTheCornerGlobeReturnToTheScreenThatOpenedThePicker")) return;
    Run run(playerPaths("back", true));
    MagicPortalsLayer& layer = run.layer;
    if (!layer.LoadError().empty() || !tickToTheMainMenu(layer, run.registry) || !layer.OptionsAvailable()) {
        CHECK_MSG(false, "no main menu with Options: " + layer.LoadError());
        return;
    }
    rest(layer, run.registry, 60);
    const glm::dvec2 view = layer.MenuBoxPx();
    const MainMenu::Rules& menu = layer.MainMenuRules();
    // The gear and the globe are both there, one under the other, and each opens its own.
    const Hud::Rect gear = Options::EntryAt(layer.OptionsRules(), menu.layer, menu.state, view, 1.0e9, false).rect;
    const Hud::Rect globe = Picker::EntryAt(layer.PickerRules(), menu.layer, menu.state, view, 1.0e9, false).rect;
    CHECK_MSG(Gap(gear, globe) >= 0.0, "the gear and the globe are apart");
    click(layer, run.registry, gear.Centre());
    CHECK_MSG(layer.MenuScreen() == MagicPortalsLayer::Screen::Options, "the gear still opens Options");
    // Options draws the corner globe, and only once.
    {
        const std::vector<ScreenOverlay::Quad> frame = menuFrame(run.registry, layer);
        int found = 0;
        for (const ScreenOverlay::Quad& quad : frame) found += quad.texture == layer.LanguageEntryImage() ? 1 : 0;
        CHECK_MSG(found == 1, "Options draws the language globe once");
    }
    // The pinned things do not move: a tap on a tile still chooses a style, not a language.
    const Options::Rules& options = layer.OptionsRules();
    const Controls::Style before = layer.ControlsStyle();
    click(layer, run.registry, Options::TileRect(options, Controls::Style::Classic, view).Centre());
    CHECK_MSG(layer.MenuScreen() == MagicPortalsLayer::Screen::Options && layer.ControlsStyle() == Controls::Style::Classic, "the Classic tile still chooses Classic");
    click(layer, run.registry, Options::TileRect(options, before, view).Centre());
    // The corner opens the picker, which returns to Options by its back button and by the back key.
    click(layer, run.registry, Picker::CornerRect(layer.PickerRules(), view).Centre());
    CHECK_MSG(layer.MenuScreen() == MagicPortalsLayer::Screen::Language && layer.LanguageReturnsTo() == MagicPortalsLayer::Screen::Options, "the corner globe opens the picker from Options");
    click(layer, run.registry, Credits::BackHitRect(layer.CreditsRules(), view, 1.0e9).Centre());
    CHECK_MSG(layer.MenuScreen() == MagicPortalsLayer::Screen::Options, "the back button returns to Options");
    click(layer, run.registry, Picker::CornerRect(layer.PickerRules(), view).Centre());
    CHECK_MSG(layer.MenuScreen() == MagicPortalsLayer::Screen::Language, "opened again");
    tickWith(layer, run.registry, kRest, {}, {MagicPortalsLayer::kBack});
    rest(layer, run.registry, 70);
    CHECK_MSG(layer.MenuScreen() == MagicPortalsLayer::Screen::Options, "and so does the back key");
    // The main menu's globe returns to the main menu.
    tickWith(layer, run.registry, kRest, {}, {MagicPortalsLayer::kBack});
    rest(layer, run.registry, 70);
    CHECK_MSG(layer.MenuScreen() == MagicPortalsLayer::Screen::Main, "back from Options is the main menu");
    click(layer, run.registry, globe.Centre());
    CHECK_MSG(layer.MenuScreen() == MagicPortalsLayer::Screen::Language && layer.LanguageReturnsTo() == MagicPortalsLayer::Screen::Main, "the main menu's globe opens the picker");
    tickWith(layer, run.registry, kRest, {}, {MagicPortalsLayer::kBack});
    rest(layer, run.registry, 70);
    CHECK_MSG(layer.MenuScreen() == MagicPortalsLayer::Screen::Main, "the back key returns to the main menu");
}

void AForcedRunKeepsItsChoiceInMemoryOnly() {
    if (!dataIsThere("AForcedRunKeepsItsChoiceInMemoryOnly")) return;
    MagicPortalsLayer::Paths paths = playerPaths("forced", false);
    paths.language.forced = "fr";
    Run run(paths);
    MagicPortalsLayer& layer = run.layer;
    if (!layer.LoadError().empty() || !tickToTheMainMenu(layer, run.registry)) {
        CHECK_MSG(false, "no main menu: " + layer.LoadError());
        return;
    }
    rest(layer, run.registry, 60);
    CHECK_MSG(layer.CurrentLanguage() == Lang::Fr && layer.LanguageAvailable(), "a forced run speaks the flag's language and offers the picker");
    click(layer, run.registry, Picker::EntryAt(layer.PickerRules(), layer.MainMenuRules().layer, layer.MainMenuRules().state, layer.ViewPx(), 1.0e9, false).rect.Centre());
    CHECK_MSG(layer.MenuScreen() == MagicPortalsLayer::Screen::Language, "the picker opens");
    click(layer, run.registry, Picker::PlateRect(layer.PickerRules(), layer.MenuBoxPx(), Strings::IndexOf(Lang::De), Strings::kLangCount).Centre());
    CHECK_MSG(layer.CurrentLanguage() == Lang::De, "a plate changes the language for the run");
    CHECK_MSG(!fs::exists(fs::path(paths.saveDir) / Language::kStoreFileName), "and nothing is written: a forced run neither reads nor writes language.json");
}

void APickerTableThatWillNotReadCostsOnlyThePicker() {
    if (!dataIsThere("APickerTableThatWillNotReadCostsOnlyThePicker")) return;
    for (const bool absent : {false, true}) {
        const fs::path data = Scratch(absent ? "picker-data-absent" : "picker-data-broken");
        std::error_code ec;
        fs::copy(kData, data, fs::copy_options::recursive, ec);
        CHECK_MSG(!ec, "a copy of the port's data: " + ec.message());
        if (absent) {
            fs::remove(data / "picker.json", ec);
        } else {
            std::string table = ReadAll(data / "picker.json");
            const std::size_t at = table.find("\"columns\": 3");
            CHECK_MSG(at != std::string::npos, "the table has its columns");
            if (at != std::string::npos) table.replace(at, 12, "\"columns\": 0");
            WriteFile(data / "picker.json", table);
        }
        MagicPortalsLayer::Paths paths = playerPaths(absent ? "absent" : "broken", false);
        paths.portData = data.string();
        Run run(paths);
        MagicPortalsLayer& layer = run.layer;
        const std::string what = absent ? "no picker.json" : "a picker.json that will not read";
        if (!layer.LoadError().empty() || !tickToTheMainMenu(layer, run.registry)) {
            CHECK_MSG(false, what + ": the run does not start: " + layer.LoadError());
            continue;
        }
        rest(layer, run.registry, 60);
        CHECK_MSG(!layer.LanguageAvailable() && layer.LanguageEntryImage().empty(), what + ": no picker and no globe");
        CHECK_MSG(layer.CurrentLanguage() == Lang::En, what + ": and the run is as it was");
        click(layer, run.registry, glm::dvec2(layer.ViewPx().x - 16.0, 52.0));
        CHECK_MSG(layer.MenuScreen() == MagicPortalsLayer::Screen::Main, what + ": a tap where the globe would be does nothing");
        fs::remove_all(data, ec);
    }
}

} // namespace

static int mpPickerLayerMain(int, char**) {
    std::error_code ec;
    if (!fs::is_directory(kLevels, ec) || !fs::is_regular_file(kChapters, ec) || !fs::is_regular_file(std::string(MAGICPORTALS_DATA_DIR) + "/entity_roles.json", ec)) {
        std::printf("test_mp_pickerlayer: SKIPPED - needs the converted levels at %s, chapters.json at %s and Enhanced's data at %s.\n", kLevels.c_str(), kChapters.c_str(),
                    MAGICPORTALS_DATA_DIR);
        return 77;
    }
    ABareLayerOffersNoPicker();
    TheGlobeOpensThePickerWhichIsDrawnWithoutAHoleInEveryLanguage();
    ATapOnAPlateChoosesAndKeepsTheLanguage();
    TheBackKeyAndTheCornerGlobeReturnToTheScreenThatOpenedThePicker();
    AForcedRunKeepsItsChoiceInMemoryOnly();
    APickerTableThatWillNotReadCostsOnlyThePicker();
    return ::test::summary("test_mp_pickerlayer", 100);
}

static const ::MpSuite::Registration mpPickerLayerRegistration("test_mp_pickerlayer", &mpPickerLayerMain);
