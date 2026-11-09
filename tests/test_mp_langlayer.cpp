// The layer speaks the language (planning step 238, PORT-34).
//
// PART ONE - the snapshot guard. English is the default and must stay what it was, to the quad: every menu screen, the level caption, the pause
// with its title and its achievements popup, the hint, the two cards and the options screens are driven through the layer's own emit functions,
// and each frame's quad list (the rectangle, the uv, the colour and the turn of every quad, the picture's file NAME as a hash, the clamp) is
// compared with tests/golden/menu_screens.json within a tolerance: a thousandth of the window on the rectangle (about a pixel), a
// hundred-thousandth on the other fractions. The first form of this golden was a hash of the float bits, recorded before the wiring; bits are exact only on the compiler and the
// C library that recorded them (the fades and the turns go through sin and cos), so it was replaced by the numbers, which were dumped from the
// wired layer in the very run that first checked it against that hash (all equal). A case that cannot fail proves nothing: another case moves
// one number of the golden by a hundredth (a difference) and by a millionth (not). The golden is recorded by `test_mp test_mp_langlayer
// --record-golden <dir>` and never re-recorded to make a change pass (the file says so, as the classic goldens do).
// The cases below that need the original's pictures, the converted levels or Enhanced's achievements list skip as a whole without them, and
// say so: a golden is all or nothing.
//
// WHAT THE GOLDEN MAY NOT HOLD (the review of steps 236-239, finding F3). A glyph quad's uv rectangle is the character's cell on the original font's
// page, so the glyph quads of a frame spell the frame's text, and a golden of format 2 spelled Asantee's achievements words (the dashboard's rows, the
// popup, a card's title) to anyone who has the original's .fnt files. The frames that draw those words are flagged where they are captured, from what
// the layer shows and not from their keys; the committed golden (format 3) keeps no glyph quad of them, only their count, the rectangle they fill and ONE
// fingerprint of all of them together; their glyphs in full are in a gitignored file beside the achievements list, compared when it exists. A content
// guard lays every line of Asantee's list out through the original's fonts and looks for it in each frame, so a frame that spells them unflagged fails the
// suite and the recording, and a flagged frame that spells none does too.
//
// The suite's other half (the language cases) follows the guard in this file.
//
// No non-ASCII character is written in this file: MSVC is built without /utf-8 here.

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
#include "sim/Achievements.hpp"
#include "sim/Dashboard.hpp"
#include "sim/FontBake.hpp"
#include "sim/Hud.hpp"
#include "sim/Language.hpp"
#include "sim/Strings.hpp"
#include "sim/MainMenu.hpp"
#include "sim/Options.hpp"
#include "sim/Pause.hpp"
#include "sim/Scores.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

using namespace Supersonic;
using MagicPortals::MagicPortalsLayer;
namespace Hud = MagicPortals::Hud;
namespace Controls = MagicPortals::Controls;
namespace Scores = MagicPortals::Scores;

namespace {

const std::string kLevels = MAGICPORTALS_LEVELS_DIR;
const std::string kData = MAGICPORTALS_DATA_DIR;
const std::string kChapters = MAGICPORTALS_CHAPTERS_FILE;
const std::string kAchievements = MAGICPORTALS_ACHIEVEMENTS_FILE;
const std::string kGoldenDir = MAGICPORTALS_GOLDEN_DIR;
const glm::vec2 kRest(640.0f, 360.0f);
constexpr double kPxPerUnit = 2.8125; // 720 px over the view's 256 units

namespace Achievements = MagicPortals::Achievements;

// A frame as the guard captures it (below, "Asantee's words in a frame"): its quads, and whether the layer showed Asantee's achievements words in it.
struct Captured {
    std::vector<ScreenOverlay::Quad> quads;
    // Decided where the frame is captured, from what the layer shows (the achievements dashboard, the achievements popup, a card for an achievement),
    // and not from the frame's key: a frame of that kind cannot be added without the flag.
    bool asanteeText = false;
    // How many lines of Asantee's achievements list the quads spell (the content guard). It must agree with the flag, or the flag is wrong.
    std::size_t spelled = 0;
};
using Entries = std::map<std::string, Captured>;

// ---- the harness, as test_mp_layer drives a bare layer ------------------------------------------------------------------

void publishViewport(entt::registry& registry, const glm::vec2& window = glm::vec2(1280.0f, 720.0f)) {
    ViewportInfo viewport;
    viewport.rect.min = glm::vec2(0.0f, 0.0f);
    viewport.rect.max = window;
    viewport.pointerOverGame = true;
    registry.ctx().insert_or_assign(viewport);
}

void tickWith(MagicPortalsLayer& layer, entt::registry& registry, const glm::vec2& pointer, std::vector<std::string> down,
              std::vector<std::string> pressed, float wheel = 0.0f) {
    PhysicsSystem::Update(registry, MagicPortalsLayer::kTick);
    Input::TickInput input;
    input.mousePosition = pointer;
    input.down = std::move(down);
    input.pressed = std::move(pressed);
    input.scroll = wheel;
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
    for (int tick = 0; tick < 400 && layer.MenuScreen() != MagicPortalsLayer::Screen::Main; ++tick) {
        tickWith(layer, registry, kRest, {}, {});
    }
    return layer.MenuScreen() == MagicPortalsLayer::Screen::Main;
}

std::vector<ScreenOverlay::Quad> menuFrame(entt::registry& registry, const MagicPortalsLayer& layer) {
    static ScreenOverlay overlay;
    overlay.Clear();
    registry.ctx().insert_or_assign<ScreenOverlay*>(&overlay);
    layer.EmitMenu(registry);
    return overlay.Quads();
}

std::vector<ScreenOverlay::Quad> hudFrame(entt::registry& registry, const MagicPortalsLayer& layer) {
    static ScreenOverlay overlay;
    overlay.Clear();
    registry.ctx().insert_or_assign<ScreenOverlay*>(&overlay);
    layer.EmitHud(registry);
    return overlay.Quads();
}

std::vector<ScreenOverlay::Quad> cardFrame(entt::registry& registry, const MagicPortalsLayer& layer) {
    static ScreenOverlay overlay;
    overlay.Clear();
    registry.ctx().insert_or_assign<ScreenOverlay*>(&overlay);
    layer.EmitNotifications(registry);
    return overlay.Quads();
}

glm::vec2 screenOfView(const MagicPortalsLayer& layer, const glm::dvec2& onView) {
    const glm::dvec2 view = layer.ViewPx();
    return glm::vec2(static_cast<float>(onView.x / view.x * 1280.0), static_cast<float>(onView.y / view.y * 720.0));
}

// A touch down and up at a point of the menu's box (units), and the tick on which what it asked for opens.
void click(MagicPortalsLayer& layer, entt::registry& registry, const glm::dvec2& onView) {
    const glm::vec2 px(static_cast<float>(onView.x * kPxPerUnit), static_cast<float>(onView.y * kPxPerUnit));
    tickWith(layer, registry, px, {MagicPortalsLayer::kTap}, {MagicPortalsLayer::kTap});
    releaseWith(layer, registry, px);
    tickWith(layer, registry, kRest, {}, {});
}

bool originalArtIsThere() {
    std::error_code ec;
    return std::filesystem::is_directory(std::string(MAGICPORTALS_ORIGINAL_DIR) + "/entities", ec);
}

MagicPortalsLayer::Paths testPaths(const std::string& name) {
    MagicPortalsLayer::Paths paths;
    paths.prisms = std::filesystem::temp_directory_path() / ("supersonic-test-mp-" + name);
    return paths;
}

std::filesystem::path scratchDir(const std::string& name) {
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / ("supersonic-test-mp-" + name);
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);
    return dir;
}

// A save with chapter 1 all gold, and chapter 2's last level scored on its own: so the dashboard has unlocked rows, chapter 2 is open, and
// chapter select shows a percent on each of two chapters and the "requires" warning on the one whose boss is scored while it is short.
std::string seededSaves(const std::string& name) {
    const std::filesystem::path dir = scratchDir(name);
    Scores::Store seed;
    std::string why;
    CHECK_MSG(seed.Open(dir.string(), why), why);
    for (int level = 0; level < 32; ++level) seed.Record(0, level, Scores::kGold);
    seed.Record(1, 31, Scores::kBronze);
    CHECK_MSG(seed.Save(why), why);
    return dir.string();
}

// ---- the hash: FNV-1a over what a frame holds ------------------------------------------------------------------------------

constexpr std::uint64_t kFnvBasis = 14695981039346656037ULL;

std::uint64_t fnv(std::uint64_t hash, const void* bytes, std::size_t count) {
    const auto* byte = static_cast<const unsigned char*>(bytes);
    for (std::size_t i = 0; i < count; ++i) {
        hash ^= byte[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

std::uint64_t fnvFloat(std::uint64_t hash, float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return fnv(hash, &bits, sizeof(bits));
}

std::string hex(std::uint64_t value) {
    char text[32];
    std::snprintf(text, sizeof(text), "%016llx", static_cast<unsigned long long>(value));
    return text;
}

// Every field of every quad, in order: the rectangle, the uv, the colour, the turn, the clamp and the picture's FILE NAME (its folder is the
// machine's). The same hash test_mp_layer's classic HUD golden uses.
std::uint64_t hashQuads(const std::vector<ScreenOverlay::Quad>& quads) {
    std::uint64_t hash = kFnvBasis;
    for (const ScreenOverlay::Quad& quad : quads) {
        const float numbers[] = {quad.min.x,   quad.min.y,   quad.max.x,   quad.max.y,   quad.uvMin.x,    quad.uvMin.y,
                                 quad.uvMax.x, quad.uvMax.y, quad.color.r, quad.color.g, quad.color.b,     quad.color.a,
                                 quad.basis[0][0], quad.basis[0][1], quad.basis[1][0], quad.basis[1][1]};
        for (const float number : numbers) hash = fnvFloat(hash, number);
        const std::size_t slash = quad.texture.find_last_of("/\\");
        const std::string file = slash == std::string::npos ? quad.texture : quad.texture.substr(slash + 1);
        hash = fnv(hash, file.data(), file.size());
        const unsigned char tail = quad.clampToEdge ? 1 : 0;
        hash = fnv(hash, &tail, 1);
    }
    return hash;
}

// A glyph quad: drawn from a page of one of the original's bitmap fonts (Matura or Verdana). Its uv rectangle IS the character's cell on that page, so a
// run of glyph quads decodes back into text with the font's .fnt file, which anyone who has the original has (the review of steps 236-239, finding F3).
bool isGlyphQuad(const ScreenOverlay::Quad& quad) {
    return quad.texture.find("Matura") != std::string::npos || quad.texture.find("Verdana") != std::string::npos;
}

std::size_t glyphQuads(const std::vector<ScreenOverlay::Quad>& quads) {
    return static_cast<std::size_t>(std::count_if(quads.begin(), quads.end(), isGlyphQuad));
}

// "<hash> quads=N glyphs=M": the hash is the pin; the counts say what a failure was about.
std::string describe(const std::vector<ScreenOverlay::Quad>& quads) {
    char text[96];
    std::snprintf(text, sizeof(text), "%s quads=%zu glyphs=%zu", hex(hashQuads(quads)).c_str(), quads.size(), glyphQuads(quads));
    return text;
}

// ---- Asantee's words in a frame -----------------------------------------------------------------------------------------------

// The 82 achievements' titles and descriptions and the secret achievement's texts are Asantee's words: they live in out/data (gitignored) and no committed
// file may spell them. A frame that draws them has glyph quads, and glyph quads spell what they draw, so such a frame is KNOWN BY CONSTRUCTION (the
// capture functions below decide it from what the layer shows) and its glyph quads stay out of the committed golden; a content guard checks that the
// decision is right in both directions, by laying every line of Asantee's list out through the original's fonts and looking for it in each frame.

// The original's seven bitmap fonts, as the layer loads them (their .fnt, in the original's data folder).
constexpr std::array<const char*, 7> kOriginalFonts = {"Matura128_shadow", "Matura42_hard_shadow", "Matura42_shadow", "Matura64_shadow",
                                                       "Matura84_shadow",  "Verdana20_shadow",      "Verdana40_shadow"};
// Their page pictures (the `page` lines of those .fnt files; theGlyphPagesAreTheOriginalsPages checks the list against the files). A quad that names
// one of them is a glyph, whatever the file it is read from says about the count of glyphs it holds.
constexpr std::array<const char*, 14> kOriginalFontPages = {
    "Matura128_shadow_0.png", "Matura128_shadow_1.png", "Matura128_shadow_2.png", "Matura128_shadow_3.png", "Matura128_shadow_4.png",
    "Matura42_hard_shadow_0.png", "Matura42_shadow_0.png", "Matura64_shadow_0.png", "Matura64_shadow_1.png", "Matura84_shadow_0.png",
    "Matura84_shadow_1.png", "Matura84_shadow_2.png", "Verdana20_shadow.png", "Verdana40_shadow_0.png"};
// Every page of those fonts is 512 x 512, so a glyph's uv is k / 512 exactly (a float holds it with no rounding): the cell is that integer.
constexpr double kPageCells = 512.0;
// A line of Asantee's list shorter than this many glyphs is not looked for: a short title or a number could be spelled by Enhanced's own words.
constexpr std::size_t kMinSpelledGlyphs = 6;

// A picture's FILE NAME as the FNV-1a hash of it (its folder is the machine's, and the name itself is not stored).
std::string textureId(const std::string& texture) {
    const std::size_t slash = texture.find_last_of("/\\");
    const std::string file = slash == std::string::npos ? texture : texture.substr(slash + 1);
    return hex(fnv(kFnvBasis, file.data(), file.size()));
}

const std::set<std::string>& glyphPageIds() {
    static const std::set<std::string> ids = [] {
        std::set<std::string> made;
        for (const char* page : kOriginalFontPages) made.insert(textureId(page));
        return made;
    }();
    return ids;
}

int cellOf(double uv) {
    return static_cast<int>(std::llround(uv * kPageCells));
}

// A glyph as the page holds it: its rectangle in cells, and the page (its picture's hashed name).
struct Cell {
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
    std::string page;
    bool operator==(const Cell& other) const { return x0 == other.x0 && y0 == other.y0 && x1 == other.x1 && y1 == other.y1 && page == other.page; }
};

Cell cellOfQuad(const ScreenOverlay::Quad& quad) {
    Cell cell;
    cell.x0 = cellOf(static_cast<double>(quad.uvMin.x));
    cell.y0 = cellOf(static_cast<double>(quad.uvMin.y));
    cell.x1 = cellOf(static_cast<double>(quad.uvMax.x));
    cell.y1 = cellOf(static_cast<double>(quad.uvMax.y));
    cell.page = textureId(quad.texture);
    return cell;
}

// The original's fonts, read here and not through the layer, so that looking at a frame loads nothing into it.
const std::vector<Supersonic::BitmapFont>& originalFonts() {
    static const std::vector<Supersonic::BitmapFont> fonts = [] {
        std::vector<Supersonic::BitmapFont> loaded;
        for (const char* name : kOriginalFonts) {
            Supersonic::BitmapFont font;
            std::string why;
            if (font.Load(std::string(MAGICPORTALS_ORIGINAL_DIR) + "/data/" + name + ".fnt", why)) loaded.push_back(std::move(font));
        }
        return loaded;
    }();
    return fonts;
}

// Every line of text of Asantee's list: each title and description (a line to a line) and the secret achievement's texts.
const std::vector<std::string>& asanteeLines() {
    static const std::vector<std::string> lines = [] {
        std::vector<std::string> made;
        Achievements::Content content;
        std::string why;
        if (!Achievements::Load(kAchievements, content, why)) return made;
        const auto add = [&made](const std::string& text) {
            std::istringstream in(text);
            std::string line;
            while (std::getline(in, line)) made.push_back(line);
        };
        for (const Achievements::Entry& entry : content.entries) {
            add(entry.title);
            add(entry.description);
        }
        for (const std::string& text : Achievements::SecretSamples(content)) add(text);
        return made;
    }();
    return lines;
}

// What the layer would draw for `line` in `font`, as cells in draw order (a character the font lacks draws nothing, so nothing is here for it either).
std::vector<Cell> cellsOfLine(const Supersonic::BitmapFont& font, const std::string& line) {
    std::vector<Cell> cells;
    const auto& pages = font.Pages();
    for (const Hud::Glyph& glyph : Hud::LayOutTextFrom(font, line, glm::dvec2(0.0), 1.0)) {
        if (glyph.page < 0 || static_cast<std::size_t>(glyph.page) >= pages.size()) continue;
        Cell cell;
        cell.x0 = cellOf(glyph.uvOffset.x);
        cell.y0 = cellOf(glyph.uvOffset.y);
        cell.x1 = cellOf(glyph.uvOffset.x + glyph.uvScale.x);
        cell.y1 = cellOf(glyph.uvOffset.y + glyph.uvScale.y);
        cell.page = textureId(pages[static_cast<std::size_t>(glyph.page)]);
        cells.push_back(std::move(cell));
    }
    return cells;
}

// Asantee's lines, each laid out through each of the original's fonts (a line the font draws in fewer than kMinSpelledGlyphs cells is left out).
const std::vector<std::vector<Cell>>& asanteeNeedles() {
    static const std::vector<std::vector<Cell>> needles = [] {
        std::vector<std::vector<Cell>> made;
        for (const std::string& line : asanteeLines()) {
            for (const Supersonic::BitmapFont& font : originalFonts()) {
                std::vector<Cell> cells = cellsOfLine(font, line);
                if (cells.size() >= kMinSpelledGlyphs) made.push_back(std::move(cells));
            }
        }
        return made;
    }();
    return needles;
}

// How many (line, font) pairs of Asantee's list the quads spell: the glyph cells of the frame, in draw order, hold the line's cells one after the other.
std::size_t spelledLines(const std::vector<ScreenOverlay::Quad>& quads) {
    std::vector<Cell> drawn;
    for (const ScreenOverlay::Quad& quad : quads) {
        if (isGlyphQuad(quad)) drawn.push_back(cellOfQuad(quad));
    }
    std::size_t found = 0;
    for (const std::vector<Cell>& needle : asanteeNeedles()) {
        if (std::search(drawn.begin(), drawn.end(), needle.begin(), needle.end()) != drawn.end()) ++found;
    }
    return found;
}

Captured captured(std::vector<ScreenOverlay::Quad> quads, bool asanteeText) {
    Captured frame;
    frame.spelled = spelledLines(quads);
    frame.quads = std::move(quads);
    frame.asanteeText = asanteeText;
    return frame;
}

// The achievements dashboard draws every row's title and description.
Captured capturedMenu(entt::registry& registry, const MagicPortalsLayer& layer) {
    return captured(menuFrame(registry, layer), layer.MenuScreen() == MagicPortalsLayer::Screen::Achievements);
}

// The pause's achievements popup draws the entries' titles and descriptions.
Captured capturedHud(entt::registry& registry, const MagicPortalsLayer& layer) {
    return captured(hudFrame(registry, layer), layer.AchievementsPopupOpen());
}

// A card for an achievement (its id is not -1) carries the entry's title; the locked-level card has no id and Enhanced's own sentence.
Captured capturedCard(entt::registry& registry, const MagicPortalsLayer& layer) {
    const auto* card = layer.Notifications().Front();
    return captured(cardFrame(registry, layer), card != nullptr && card->id >= 0);
}

// What is wrong with the flags of a set of captured frames: a frame that spells Asantee's words and is not flagged would put them into the golden; a flagged
// frame that spells none means the guard cannot see (or the flag is wrong). Only the frames' keys and counts are named, never a word.
std::vector<std::string> guardProblems(const Entries& frames) {
    std::vector<std::string> problems;
    if (originalFonts().size() != kOriginalFonts.size()) problems.push_back("the original's fonts did not all load: the guard cannot lay Asantee's lines out");
    if (asanteeNeedles().empty()) problems.push_back("achievements.json gave the guard no line to look for");
    for (const auto& [key, frame] : frames) {
        if (!frame.asanteeText && frame.spelled > 0) {
            problems.push_back("\"" + key + "\" spells " + std::to_string(frame.spelled) + " of Asantee's lines and is not flagged as showing them");
        }
        if (frame.asanteeText && frame.spelled == 0) {
            problems.push_back("\"" + key + "\" is flagged as showing Asantee's words but spells none of them: the flag or the guard is wrong");
        }
    }
    return problems;
}

// ---- the English frames -------------------------------------------------------------------------------------------------------

// One of the menu's states in a fresh layer over `saves`, reached from the main menu by `go`, `settle` ticks after it. The keys name what is
// drawn; the layer is detached when the call ends.
template <class Go>
void snapshotMenu(Entries& entries, const std::string& key, const std::string& saves, Go go, int settle, const MagicPortalsLayer::Paths& base) {
    entt::registry registry;
    publishViewport(registry);
    MagicPortalsLayer::Paths paths = base;
    paths.saveDir = saves;
    MagicPortalsLayer layer(paths, "");
    layer.OnAttach(registry);
    if (!layer.LoadError().empty() || !tickToTheMainMenu(layer, registry)) {
        CHECK_MSG(false, key + ": no main menu: " + layer.LoadError());
        return;
    }
    rest(layer, registry, 60);
    go(layer, registry, entries, key);
    rest(layer, registry, settle);
    entries[key] = capturedMenu(registry, layer);
    layer.OnDetach(registry);
}

void snapshotLoadingAndMain(Entries& entries) {
    entt::registry registry;
    publishViewport(registry);
    MagicPortalsLayer layer(testPaths("langlayer-loading"), "");
    layer.OnAttach(registry);
    CHECK_MSG(layer.LoadError().empty(), layer.LoadError());
    rest(layer, registry, 20);
    entries["loading tick 20"] = capturedMenu(registry, layer);
    rest(layer, registry, 40);
    entries["loading tick 60"] = capturedMenu(registry, layer);
    CHECK_MSG(tickToTheMainMenu(layer, registry), "the main menu");
    rest(layer, registry, 30);
    entries["main"] = capturedMenu(registry, layer);
    layer.OnDetach(registry);
}

void snapshotSelectors(Entries& entries, const std::string& saves) {
    const MagicPortalsLayer::Paths paths = testPaths("langlayer-select");
    snapshotMenu(
        entries, "worlds (a save: chapter 1 gold, chapter 2's boss scored)", saves,
        [](MagicPortalsLayer& layer, entt::registry& registry, Entries&, const std::string&) {
            CHECK(layer.PressMainMenu(registry, MagicPortals::MainMenu::Button::Play));
        },
        90, paths);
    for (const int world : {0, 1, 3}) {
        snapshotMenu(
            entries, "levels of chapter " + std::to_string(world + 1) + " (same save)", saves,
            [world](MagicPortalsLayer& layer, entt::registry& registry, Entries&, const std::string&) {
                CHECK(layer.PressMainMenu(registry, MagicPortals::MainMenu::Button::Play));
                rest(layer, registry, 60);
                for (const MagicPortalsLayer::MenuButton& button : layer.MenuButtons()) {
                    if (button.kind == MagicPortalsLayer::MenuButton::Kind::World && button.world == world) {
                        layer.PressMenu(registry, button);
                        break;
                    }
                }
            },
            90, paths);
    }
    snapshotMenu(
        entries, "worlds (a fresh save)", "",
        [](MagicPortalsLayer& layer, entt::registry& registry, Entries&, const std::string&) {
            CHECK(layer.PressMainMenu(registry, MagicPortals::MainMenu::Button::Play));
        },
        90, paths);
}

void snapshotCredits(Entries& entries) {
    const MagicPortalsLayer::Paths paths = testPaths("langlayer-credits");
    // The strip's tail (the enhanced version's closing card) comes up from the bottom edge after about 25 s; three frames while its six lines
    // pass through the view.
    entt::registry registry;
    publishViewport(registry);
    MagicPortalsLayer layer(paths, "");
    layer.OnAttach(registry);
    if (!layer.LoadError().empty() || !tickToTheMainMenu(layer, registry)) {
        CHECK_MSG(false, "credits: no main menu: " + layer.LoadError());
        return;
    }
    rest(layer, registry, 60);
    CHECK(layer.PressMainMenu(registry, MagicPortals::MainMenu::Button::Credits));
    int ticks = 0;
    for (const int at : {60, 1500, 1620, 1740, 1860}) {
        rest(layer, registry, at - ticks);
        ticks = at;
        entries["credits tick " + std::to_string(at)] = capturedMenu(registry, layer);
    }
    layer.OnDetach(registry);
}

void snapshotDashboard(Entries& entries, const std::string& saves) {
    entt::registry registry;
    publishViewport(registry);
    MagicPortalsLayer::Paths paths = testPaths("langlayer-dashboard");
    paths.saveDir = saves;
    MagicPortalsLayer layer(paths, "");
    layer.OnAttach(registry);
    if (!layer.LoadError().empty() || !tickToTheMainMenu(layer, registry)) {
        CHECK_MSG(false, "dashboard: no main menu: " + layer.LoadError());
        return;
    }
    rest(layer, registry, 60);
    CHECK(layer.PressMainMenu(registry, MagicPortals::MainMenu::Button::Achievements));
    rest(layer, registry, 90);
    // The wheel's tick throws the list, and the momentum carries it on: a move of m units travels m / (1 - decay) before it rests. Thirteen
    // frames at equal stops from the top to the bottom, so that every one of the 86 rows is on the view in some frame.
    const MagicPortals::Dashboard::Rules& rules = layer.DashboardRules();
    const double bottom = -MagicPortals::Dashboard::MinScroll(rules, layer.DashboardBoard(), layer.MenuBoxPx());
    constexpr int kFrames = 13;
    const double stop = bottom / (kFrames - 1);
    const float notches = static_cast<float>(-(stop * (1.0 - rules.scroll.momentumDecayPerTick)) / rules.scroll.wheelUnitsPerNotch);
    for (int frame = 0; frame < kFrames; ++frame) {
        entries["dashboard stop " + std::to_string(frame)] = capturedMenu(registry, layer);
        tickWith(layer, registry, kRest, {}, {}, notches);
        rest(layer, registry, 240);
    }
    layer.OnDetach(registry);
}

void snapshotTheLockedCard(Entries& entries) {
    // A fresh save: chapter 1's first level is open and the rest are locked, so a locked level's row is on the first screen.
    entt::registry registry;
    publishViewport(registry);
    MagicPortalsLayer layer(testPaths("langlayer-locked"), "");
    layer.OnAttach(registry);
    if (!layer.LoadError().empty() || !tickToTheMainMenu(layer, registry)) {
        CHECK_MSG(false, "locked card: no main menu: " + layer.LoadError());
        return;
    }
    rest(layer, registry, 60);
    CHECK(layer.PressMainMenu(registry, MagicPortals::MainMenu::Button::Achievements));
    rest(layer, registry, 90);
    const MagicPortals::Dashboard::Board& board = layer.DashboardBoard();
    int locked = -1;
    for (std::size_t i = 0; i < board.lines.size() && locked < 0; ++i) {
        const auto& line = board.lines[i];
        if (!line.header && line.level >= 0 && !line.levelUnlocked) locked = static_cast<int>(i);
    }
    const glm::dvec2 view = layer.MenuBoxPx();
    CHECK_MSG(locked >= 0 && MagicPortals::Dashboard::LineShown(layer.DashboardRules(), view, locked, 0.0),
              "a locked level's row on the first screen: " + std::to_string(locked));
    if (locked < 0) return;
    const Hud::Rect rect = MagicPortals::Dashboard::LineHitRect(layer.DashboardRules(), view, locked, 0.0);
    click(layer, registry, rect.Centre());
    CHECK_MSG(layer.Notifications().Front() != nullptr, "the locked row raises its card");
    rest(layer, registry, 45);
    entries["locked-level card over the dashboard"] = capturedCard(registry, layer);
    layer.OnDetach(registry);
}

void snapshotOptions(Entries& entries) {
    namespace OptionsUi = MagicPortals::Options;
    MagicPortalsLayer::Paths paths = testPaths("langlayer-options");
    paths.controls = Controls::ResolvePolicy(std::string("modern"), false, false, false);
    CHECK_MSG(paths.controls.valid, paths.controls.error);
    paths.controls.withoutOriginalArt = true;
    paths.saveDir = scratchDir("langlayer-options-saves").string();
    entt::registry registry;
    publishViewport(registry);
    MagicPortalsLayer layer(paths, "");
    layer.OnAttach(registry);
    if (!layer.LoadError().empty() || !tickToTheMainMenu(layer, registry) || !layer.OptionsAvailable()) {
        CHECK_MSG(false, "options: no main menu with the entry: " + layer.LoadError());
        return;
    }
    rest(layer, registry, 60);
    entries["main with the options entry"] = capturedMenu(registry, layer);
    const Hud::Rect entry = OptionsUi::EntryAt(layer.OptionsRules(), layer.MainMenuRules().layer, layer.MainMenuRules().state, layer.ViewPx(), 1.0e9, false).rect;
    click(layer, registry, entry.Centre());
    rest(layer, registry, 60);
    CHECK_MSG(layer.MenuScreen() == MagicPortalsLayer::Screen::Options, "the entry opens Options");
    entries["options"] = capturedMenu(registry, layer);
    if (layer.AdjustAvailable()) {
        click(layer, registry, OptionsUi::AdjustRect(layer.OptionsRules(), layer.ViewPx()).Centre());
        rest(layer, registry, 60);
        CHECK_MSG(layer.MenuScreen() == MagicPortalsLayer::Screen::Adjust, "the adjust button opens the adjustment screen");
        entries["adjust"] = capturedMenu(registry, layer);
    }
    layer.OnDetach(registry);
}

void snapshotLevels(Entries& entries) {
    {
        entt::registry registry;
        publishViewport(registry);
        MagicPortalsLayer layer(testPaths("langlayer-caption"), "level0");
        layer.OnAttach(registry);
        CHECK_MSG(layer.SimLevel() != nullptr, layer.LoadError());
        if (layer.SimLevel() == nullptr) return;
        rest(layer, registry, 30);
        entries["caption level0 tick 30"] = capturedHud(registry, layer);
        layer.OnDetach(registry);
    }
    {
        entt::registry registry;
        publishViewport(registry);
        MagicPortalsLayer layer(testPaths("langlayer-hint"), "level20");
        layer.OnAttach(registry);
        CHECK_MSG(layer.SimLevel() != nullptr, layer.LoadError());
        if (layer.SimLevel() == nullptr) return;
        rest(layer, registry, 90);
        entries["caption and hint level20 tick 90"] = capturedHud(registry, layer);
        layer.OnDetach(registry);
    }
    {
        // The pause over 1-31 (level30), and the achievements popup over it.
        entt::registry registry;
        publishViewport(registry);
        MagicPortalsLayer layer(testPaths("langlayer-pause"), "level30");
        layer.OnAttach(registry);
        CHECK_MSG(layer.SimLevel() != nullptr, layer.LoadError());
        if (layer.SimLevel() == nullptr) return;
        rest(layer, registry, 120);
        Hud::Rect control;
        CHECK_MSG(layer.ControlRect(MagicPortalsLayer::Control::Menu, control), "the pause control is shown");
        tickWith(layer, registry, screenOfView(layer, control.Centre()), {MagicPortalsLayer::kTap}, {MagicPortalsLayer::kTap});
        rest(layer, registry, 60);
        CHECK_MSG(layer.Paused(), "the pause is up");
        entries["pause level30"] = capturedHud(registry, layer);
        if (layer.AchievementsContent() != nullptr) {
            CHECK(layer.PressPause(registry, MagicPortals::Pause::Button::Achievements) && layer.AchievementsPopupOpen());
            rest(layer, registry, 60);
            entries["achievements popup over the pause of level30"] = capturedHud(registry, layer);
        }
        layer.OnDetach(registry);
    }
    {
        // 1-01 walked to its door: achievement 45's card, with the title of Enhanced's list.
        MagicPortalsLayer::Paths paths = testPaths("langlayer-unlock");
        paths.saveDir = scratchDir("langlayer-unlock-saves").string();
        entt::registry registry;
        publishViewport(registry);
        MagicPortalsLayer layer(paths, "level0");
        layer.OnAttach(registry);
        CHECK_MSG(layer.SimLevel() != nullptr, layer.LoadError());
        if (layer.SimLevel() == nullptr) return;
        for (int tick = 0; tick < 600 && !layer.Finishing(); ++tick) {
            std::vector<std::string> pressed;
            if (tick == 0) pressed.push_back(MagicPortalsLayer::kRight);
            tickWith(layer, registry, kRest, {MagicPortalsLayer::kRight}, std::move(pressed));
        }
        CHECK_MSG(layer.Finishing(), "level0's door is reached by walking right");
        int shownAt = -1;
        for (int tick = 0; tick < 420 && shownAt < 0; ++tick) {
            rest(layer, registry, 1);
            if (layer.Notifications().Front() != nullptr) shownAt = tick;
        }
        CHECK_MSG(shownAt >= 0, "the unlock card comes up");
        rest(layer, registry, 60);
        entries["unlock card after level0's door"] = capturedCard(registry, layer);
        layer.OnDetach(registry);
    }
}

Entries englishFrames() {
    Entries entries;
    const std::string saves = seededSaves("langlayer-saves");
    snapshotLoadingAndMain(entries);
    snapshotSelectors(entries, saves);
    snapshotCredits(entries);
    snapshotDashboard(entries, saves);
    snapshotTheLockedCard(entries);
    snapshotOptions(entries);
    snapshotLevels(entries);
    return entries;
}

// ---- the golden ---------------------------------------------------------------------------------------------------------------

// A frame's quads as numbers (planning step 238; the first form of this golden hashed the float bits, which is exact only on the compiler and the
// C library that recorded it - the clock-driven fades and the turns go through sin and cos, and a last bit of those differs between libms).
// A quad is its sixteen numbers (the rectangle, the uv, the colour, the 2x2 turn), the picture's FILE NAME as the FNV-1a hash of it (its folder is
// the machine's, and the name itself is not stored), and the clamp.
//
// FORMAT 3 (the review of steps 236-239, finding F3). A glyph quad's uv rectangle is the character's cell on the original font's page, so the glyph
// quads of a frame that draws Asantee's achievements words SPELL them: the format-2 golden held about 3,800 of them, and over a hundred whole titles
// and descriptions could be read back out of it with the original's .fnt files. A frame that draws those words is HIDDEN. The committed file keeps
// its other quads exactly, the number of its glyphs and the rectangle they fill, and no glyph quad. What compares the glyphs themselves is
//  - ONE fingerprint of the glyph sequences of all the hidden frames together, in key order (the cells of the pages, in draw order, with no place):
//    not one per frame, since the hash of a short frame (a card with a single title) could be matched against guessed titles; and
//  - the gitignored menu_screens_text_golden.json beside the achievements list, which holds those glyph quads in full, is written by the same command
//    and compared when it is there.
struct GoldenQuad {
    std::array<double, 16> numbers{};
    std::string texture; // 16 hex digits
    bool clamp = false;
};
struct GoldenFrame {
    std::size_t glyphs = 0; // quads drawn in a Matura or Verdana font
    // Every quad of the frame; in a hidden frame, only those that are not glyphs.
    std::vector<GoldenQuad> quads;
    bool hidden = false;
    // A hidden frame's glyph quads: the rectangle they fill (the four position numbers).
    std::array<double, 4> bounds{};
};
using GoldenFrames = std::map<std::string, GoldenFrame>;
struct Golden {
    GoldenFrames frames;
    std::string fingerprint; // 16 hex digits; see fingerprintOf
};

constexpr int kGoldenFormat = 3;

// How far a number may be from the recorded one. The rectangle is in fractions of the window, 0 to 1 (a thousandth is about a pixel on a 1280 x 720
// window; a float near 1 is 1.2e-7 apart from its neighbour, so thousands of steps of rounding); the uv, the colour and the turn are fractions too,
// held to a hundred times tighter.
constexpr double kPositionTolerance = 1.0e-3;
constexpr double kFractionTolerance = 1.0e-5;
constexpr std::size_t kPositionNumbers = 4;

bool isGlyphId(const std::string& texture) {
    return glyphPageIds().count(texture) != 0;
}

// Every quad of every frame, each frame hidden or not as it was captured.
GoldenFrames fullFramesOf(const Entries& frames) {
    GoldenFrames out;
    for (const auto& [key, shot] : frames) {
        GoldenFrame frame;
        frame.glyphs = glyphQuads(shot.quads);
        frame.hidden = shot.asanteeText;
        for (const ScreenOverlay::Quad& quad : shot.quads) {
            GoldenQuad made;
            const float numbers[] = {quad.min.x,   quad.min.y,   quad.max.x,   quad.max.y,   quad.uvMin.x,    quad.uvMin.y,
                                     quad.uvMax.x, quad.uvMax.y, quad.color.r, quad.color.g, quad.color.b,     quad.color.a,
                                     quad.basis[0][0], quad.basis[0][1], quad.basis[1][0], quad.basis[1][1]};
            for (std::size_t i = 0; i < made.numbers.size(); ++i) made.numbers[i] = numbers[i];
            made.texture = textureId(quad.texture);
            made.clamp = quad.clampToEdge;
            frame.quads.push_back(made);
        }
        out[key] = std::move(frame);
    }
    return out;
}

std::uint64_t fnvLittle(std::uint64_t hash, std::uint64_t value, int bytes) {
    for (int i = 0; i < bytes; ++i) {
        const unsigned char byte = static_cast<unsigned char>((value >> (8 * i)) & 0xFFu);
        hash = fnv(hash, &byte, 1);
    }
    return hash;
}

// ONE fingerprint over the glyph sequences of the hidden frames, in key order: each frame's key, its count of glyphs (64 bits) and its glyph cells in
// draw order (how many quads that are not glyphs were drawn before it, 32 bits; the page's hashed name; then the four cell numbers of the uv
// rectangle, 32 bits each, least significant byte first). The first of those is what keeps the draw order of text and panels pinned (the committed
// file holds the other quads apart from the glyphs, so it cannot say where among them a run of text lies) without a run length in plain text: the
// length of a line of text is the very thing that is withheld. Integers only, and no rectangle: a glyph's uv is k / 512 exactly in a float (the pages
// are 512 x 512, so no division rounds), the text file's seven digits are far inside half a cell, and the cell is rounded to the nearest integer, so
// that every compiler, libm and word size hashes the same bytes. A position, which can pass through sin and cos in an animated piece, is never hashed.
std::string fingerprintOf(const GoldenFrames& full) {
    std::uint64_t hash = kFnvBasis;
    for (const auto& [key, frame] : full) {
        if (!frame.hidden) continue;
        hash = fnv(hash, key.data(), key.size());
        const unsigned char end = 0;
        hash = fnv(hash, &end, 1);
        std::uint64_t count = 0;
        for (const GoldenQuad& quad : frame.quads) {
            if (isGlyphId(quad.texture)) ++count;
        }
        hash = fnvLittle(hash, count, 8);
        std::uint64_t others = 0; // the quads that are not glyphs, drawn so far
        for (const GoldenQuad& quad : frame.quads) {
            if (!isGlyphId(quad.texture)) {
                ++others;
                continue;
            }
            hash = fnvLittle(hash, others, 4);
            hash = fnv(hash, quad.texture.data(), quad.texture.size());
            for (std::size_t i = 4; i < 8; ++i) {
                hash = fnvLittle(hash, static_cast<std::uint32_t>(static_cast<std::int32_t>(cellOf(quad.numbers[i]))), 4);
            }
        }
    }
    return hex(hash);
}

// The committed form of a set of frames: in a hidden frame the glyph quads are gone and what stays of them is their count, the rectangle they fill
// and, for all the hidden frames together, the fingerprint. Glyphs are told by the font pages' picture hashes (kOriginalFontPages), the same way for
// a live frame and for a file; a frame whose count of glyphs is not the count of quads from those pages names a page the list lacks.
bool redact(const GoldenFrames& full, Golden& out, std::string& problem) {
    out = Golden();
    out.fingerprint = fingerprintOf(full);
    for (const auto& [key, frame] : full) {
        GoldenFrame kept = frame;
        kept.quads.clear();
        std::size_t drawn = 0;
        bool any = false;
        for (const GoldenQuad& quad : frame.quads) {
            if (!isGlyphId(quad.texture)) {
                kept.quads.push_back(quad);
                continue;
            }
            ++drawn;
            if (!frame.hidden) {
                kept.quads.push_back(quad);
                continue;
            }
            const double xs[] = {quad.numbers[0], quad.numbers[2]};
            const double ys[] = {quad.numbers[1], quad.numbers[3]};
            const double x0 = std::min(xs[0], xs[1]);
            const double x1 = std::max(xs[0], xs[1]);
            const double y0 = std::min(ys[0], ys[1]);
            const double y1 = std::max(ys[0], ys[1]);
            if (!any) {
                kept.bounds = {x0, y0, x1, y1};
                any = true;
            } else {
                kept.bounds = {std::min(kept.bounds[0], x0), std::min(kept.bounds[1], y0), std::max(kept.bounds[2], x1), std::max(kept.bounds[3], y1)};
            }
        }
        if (drawn != frame.glyphs) {
            problem = "\"" + key + "\" holds " + std::to_string(drawn) + " quads from the original's font pages and counts " + std::to_string(frame.glyphs) +
                      " glyphs: kOriginalFontPages lacks a page";
            return false;
        }
        if (!frame.hidden) kept.bounds = {};
        out.frames[key] = std::move(kept);
    }
    return true;
}

// The hidden frames' glyph quads, in full, as ordinary frames: the content of the local text golden.
GoldenFrames glyphsOfHiddenFrames(const GoldenFrames& full) {
    GoldenFrames out;
    for (const auto& [key, frame] : full) {
        if (!frame.hidden) continue;
        GoldenFrame kept;
        for (const GoldenQuad& quad : frame.quads) {
            if (isGlyphId(quad.texture)) kept.quads.push_back(quad);
        }
        kept.glyphs = kept.quads.size();
        out[key] = std::move(kept);
    }
    return out;
}

// What differs between two goldens: the same frames, hidden or not alike, the same number of quads and glyphs in each, the same pictures and clamps,
// every number within its tolerance, the rectangle a hidden frame's glyphs fill, and the fingerprint. At most `limit` lines, each naming the frame, the
// quad and the number.
std::vector<std::string> differences(const Golden& now, const Golden& golden, std::size_t limit = 8) {
    std::vector<std::string> lines;
    const auto say = [&](const std::string& line) {
        if (lines.size() < limit) lines.push_back(line);
    };
    for (const auto& [key, frame] : now.frames) {
        const auto found = golden.frames.find(key);
        if (found == golden.frames.end()) {
            say("\"" + key + "\" has no entry in the golden");
            continue;
        }
        const GoldenFrame& want = found->second;
        if (frame.hidden != want.hidden) {
            say("\"" + key + "\" is " + (frame.hidden ? "" : "not ") + "a hidden frame, the golden's " + (want.hidden ? "is" : "is not"));
            continue;
        }
        if (frame.quads.size() != want.quads.size() || frame.glyphs != want.glyphs) {
            say("\"" + key + "\" draws " + std::to_string(frame.quads.size()) + " quads (" + std::to_string(frame.glyphs) + " glyphs), the golden has " +
                std::to_string(want.quads.size()) + " (" + std::to_string(want.glyphs) + ")");
            continue;
        }
        for (std::size_t q = 0; q < frame.quads.size(); ++q) {
            const GoldenQuad& a = frame.quads[q];
            const GoldenQuad& b = want.quads[q];
            if (a.texture != b.texture || a.clamp != b.clamp) {
                say("\"" + key + "\" quad " + std::to_string(q) + ": the picture or the clamp is not the golden's");
                continue;
            }
            for (std::size_t i = 0; i < a.numbers.size(); ++i) {
                const double tolerance = i < kPositionNumbers ? kPositionTolerance : kFractionTolerance;
                if (!(std::fabs(a.numbers[i] - b.numbers[i]) <= tolerance)) {
                    char text[160];
                    std::snprintf(text, sizeof(text), "\"%s\" quad %zu number %zu is %.6f, the golden has %.6f", key.c_str(), q, i, a.numbers[i], b.numbers[i]);
                    say(text);
                    break;
                }
            }
        }
        if (frame.hidden) {
            for (std::size_t i = 0; i < frame.bounds.size(); ++i) {
                if (!(std::fabs(frame.bounds[i] - want.bounds[i]) <= kPositionTolerance)) {
                    char text[160];
                    std::snprintf(text, sizeof(text), "\"%s\" the rectangle its glyphs fill: number %zu is %.6f, the golden has %.6f", key.c_str(), i,
                                  frame.bounds[i], want.bounds[i]);
                    say(text);
                    break;
                }
            }
        }
    }
    for (const auto& [key, frame] : golden.frames) {
        (void)frame;
        if (now.frames.find(key) == now.frames.end()) say("the golden's \"" + key + "\" was not produced now");
    }
    if (now.fingerprint != golden.fingerprint) {
        say("the glyph sequences of the hidden frames are not the golden's (the local text golden, if recorded, names the frame)");
    }
    return lines;
}

// Parse a golden from its text. The reader knows format 3 and its keys and nothing else: another format number, a key it was not told of, a hidden frame
// with a glyph quad in it and a frame whose count of glyphs is not the count of its quads from the font pages are all refused. `local` is the gitignored
// text golden (marked "local_only", with glyph quads only and no fingerprint); the committed file may not carry the mark, so that the local file, copied
// into tests/golden by mistake, is refused by the suite and not committed unseen.
bool parseGolden(const std::string& text, bool local, Golden& out, std::string& error) {
    out = Golden();
    Json::Value root;
    Json::Parser parser(text);
    if (!parser.Parse(root)) {
        error = parser.Error();
        return false;
    }
    if (!root.IsObject()) {
        error = "not an object";
        return false;
    }
    static const std::set<std::string> kTopKeys = {"_about", "format", "fingerprint", "frames", "local_only"};
    for (const auto& [key, value] : root.AsObject()) {
        (void)value;
        if (kTopKeys.count(key) == 0) {
            error = "unknown key \"" + key + "\"";
            return false;
        }
    }
    if (!root["format"].IsNumber() || root["format"].AsNumber() != static_cast<double>(kGoldenFormat) || !root["frames"].IsObject()) {
        error = "not format " + std::to_string(kGoldenFormat) + " with a \"frames\" object";
        return false;
    }
    if (local != root.Has("local_only") || (local && !(root["local_only"].IsBool() && root["local_only"].AsBool()))) {
        error = local ? "not the local text golden (\"local_only\" is not true)" : "carries \"local_only\": that is the local text golden, which is never committed";
        return false;
    }
    if (local) {
        if (root.Has("fingerprint")) {
            error = "the local text golden has no fingerprint";
            return false;
        }
    } else {
        const std::string& fingerprint = root["fingerprint"].AsString();
        const bool hexDigits = fingerprint.size() == 16 && fingerprint.find_first_not_of("0123456789abcdef") == std::string::npos;
        if (!root["fingerprint"].IsString() || !hexDigits) {
            error = "\"fingerprint\" is not 16 hex digits";
            return false;
        }
        out.fingerprint = fingerprint;
    }
    static const std::set<std::string> kFrameKeys = {"glyphs", "hidden", "quads"};
    for (const auto& [key, value] : root["frames"].AsObject()) {
        if (!value.IsObject() || !value["glyphs"].IsNumber() || !value["quads"].IsArray()) {
            error = "\"" + key + "\" is not {glyphs, quads}";
            return false;
        }
        for (const auto& [member, ignored] : value.AsObject()) {
            (void)ignored;
            if (kFrameKeys.count(member) == 0) {
                error = "\"" + key + "\" has an unknown member \"" + member + "\"";
                return false;
            }
        }
        GoldenFrame frame;
        frame.glyphs = static_cast<std::size_t>(value["glyphs"].AsNumber());
        if (value.Has("hidden")) {
            const Json::Value& hidden = value["hidden"];
            if (local || !hidden.IsObject() || hidden.AsObject().size() != 1 || !hidden["bounds"].IsArray() || hidden["bounds"].AsArray().size() != 4) {
                error = "\"" + key + "\" is hidden without exactly {\"bounds\": [4 numbers]}, or in the local file";
                return false;
            }
            for (std::size_t i = 0; i < frame.bounds.size(); ++i) {
                const Json::Value& number = hidden["bounds"].AsArray()[i];
                if (!number.IsNumber()) {
                    error = "\"" + key + "\" has a bound that is not a number";
                    return false;
                }
                frame.bounds[i] = number.AsNumber();
            }
            frame.hidden = true;
        }
        std::size_t drawn = 0;
        for (const Json::Value& item : value["quads"].AsArray()) {
            if (!item.IsArray() || item.AsArray().size() != 18) {
                error = "a quad of \"" + key + "\" is not 16 numbers, a picture and a clamp";
                return false;
            }
            const Json::Array& fields = item.AsArray();
            GoldenQuad quad;
            for (std::size_t i = 0; i < quad.numbers.size(); ++i) {
                if (!fields[i].IsNumber()) {
                    error = "a number of \"" + key + "\" is not a number";
                    return false;
                }
                quad.numbers[i] = fields[i].AsNumber();
            }
            if (!fields[16].IsString() || !fields[17].IsNumber()) {
                error = "a quad of \"" + key + "\" lacks its picture or its clamp";
                return false;
            }
            quad.texture = fields[16].AsString();
            quad.clamp = fields[17].AsNumber() != 0.0;
            if (isGlyphId(quad.texture)) ++drawn;
            if (frame.hidden && isGlyphId(quad.texture)) {
                error = "\"" + key + "\" is a hidden frame and holds a glyph quad";
                return false;
            }
            if (local && !isGlyphId(quad.texture)) {
                error = "\"" + key + "\" holds a quad that is not a glyph, in the local file";
                return false;
            }
            frame.quads.push_back(quad);
        }
        if (!frame.hidden && drawn != frame.glyphs) {
            error = "\"" + key + "\" counts " + std::to_string(frame.glyphs) + " glyphs and holds " + std::to_string(drawn) + " quads from the font pages";
            return false;
        }
        if (frame.hidden && frame.glyphs == 0) {
            error = "\"" + key + "\" is hidden and has no glyph";
            return false;
        }
        out.frames[key] = std::move(frame);
    }
    return true;
}

bool readGolden(const std::string& path, bool local, Golden& out, std::string& error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = path + " cannot be opened";
        return false;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    if (!parseGolden(buffer.str(), local, out, error)) {
        error = path + ": " + error;
        return false;
    }
    return true;
}

bool dataIsThere(const char* test) {
    std::error_code ec;
    if (!std::filesystem::is_directory(kLevels, ec) || !std::filesystem::is_regular_file(kChapters, ec) ||
        !std::filesystem::is_regular_file(kAchievements, ec) || !originalArtIsThere()) {
        std::printf("  %s SKIPPED - needs the converted levels, chapters.json, achievements.json and the original's extracted assets\n", test);
        return false;
    }
    return true;
}

// The gitignored text golden, beside the achievements list (out/data is gitignored in full): the hidden frames' glyph quads, which spell Asantee's words.
std::filesystem::path localTextGoldenPath() {
    return std::filesystem::path(kAchievements).parent_path() / "menu_screens_text_golden.json";
}

// The one rail under that file: .gitignore covers out/ and nothing else, so the recorder writes the file only where a folder of the path is called out.
// An achievements list that a build cache points at some other folder of the repository would put Asantee's words one `git add -A` from a commit.
bool underAnOutFolder(const std::filesystem::path& path) {
    for (const std::filesystem::path& part : path.parent_path()) {
        if (part == "out") return true;
    }
    return false;
}

void theLocalTextGoldenStaysUnderOut() {
    CHECK_MSG(underAnOutFolder("C:/work/Magic-Portals-Enhanced/out/data/menu_screens_text_golden.json"), "the default place, a Windows path");
    CHECK_MSG(underAnOutFolder("/home/someone/mpn2/data/out/data/menu_screens_text_golden.json"), "a Linux path with out in the middle");
    CHECK_MSG(underAnOutFolder("out/data/menu_screens_text_golden.json"), "a relative path");
    CHECK_MSG(!underAnOutFolder("/home/someone/repo/data/menu_screens_text_golden.json"), "a folder of the repository that is not out/");
    CHECK_MSG(!underAnOutFolder("/home/someone/repo/outside/data/menu_screens_text_golden.json"), "a folder that only begins with out");
    CHECK_MSG(!underAnOutFolder("/home/someone/repo/tests/golden/out"), "a file called out is not a folder called out");
    CHECK_MSG(!underAnOutFolder("menu_screens_text_golden.json"), "no folder at all");
    // The place this build writes to is the default, and passes.
    CHECK_MSG(std::string(kAchievements).empty() || underAnOutFolder(localTextGoldenPath()),
              "this build's local text golden is under an out folder: " + localTextGoldenPath().generic_string());
}

void theEnglishFramesAreWhatTheyWere() {
    if (!dataIsThere("theEnglishFramesAreWhatTheyWere")) return;
    const Entries frames = englishFrames();
    CHECK_MSG(frames.size() >= 30u, "the guard covers the screens: " + std::to_string(frames.size()) + " frames");
    // Every frame draws something: an empty menu frame would equal an empty golden.
    for (const auto& [key, frame] : frames) CHECK_MSG(!frame.quads.empty(), "\"" + key + "\" draws quads");
    // The flags are right in both directions (by content): no frame that spells Asantee's words goes unflagged, no flagged frame is blind.
    const std::vector<std::string> problems = guardProblems(frames);
    CHECK_MSG(problems.empty(), "the frames that draw Asantee's words are not the flagged ones: " + (problems.empty() ? std::string() : problems.front()) +
                                    " (" + std::to_string(problems.size()) + " problem(s))");
    const GoldenFrames full = fullFramesOf(frames);
    Golden now;
    std::string problem;
    CHECK_MSG(redact(full, now, problem), problem);
    Golden golden;
    std::string error;
    const bool read = readGolden(kGoldenDir + "/menu_screens.json", false, golden, error);
    CHECK_MSG(read, error + " - record it with: test_mp test_mp_langlayer --record-golden <tests/golden>");
    if (!read) return;
    const std::vector<std::string> lines = differences(now, golden);
    CHECK_MSG(lines.empty(), "menu_screens.json: English is not what it was: " + (lines.empty() ? std::string() : lines.front()) + " (" + std::to_string(lines.size()) + " shown)");
    for (std::size_t i = 1; i < lines.size(); ++i) std::printf("  %s\n", lines[i].c_str());
    // The hidden frames' glyphs, in full, against the gitignored text golden where it was recorded (a clone with no out/ has none, and passes without).
    const std::filesystem::path local = localTextGoldenPath();
    std::error_code ec;
    if (!std::filesystem::is_regular_file(local, ec)) {
        std::printf("  the local text golden %s is not recorded: the hidden frames' glyphs are compared by their fingerprint only\n", local.generic_string().c_str());
        return;
    }
    Golden text;
    const bool readText = readGolden(local.string(), true, text, error);
    CHECK_MSG(readText, error + " - delete it and record it again with --record-golden");
    if (!readText) return;
    Golden nowText;
    nowText.frames = glyphsOfHiddenFrames(full);
    const std::vector<std::string> textLines = differences(nowText, text);
    CHECK_MSG(textLines.empty(), "menu_screens_text_golden.json: the hidden frames' glyphs are not what they were: " + (textLines.empty() ? std::string() : textLines.front()) +
                                     " (" + std::to_string(textLines.size()) + " shown)");
    for (std::size_t i = 1; i < textLines.size(); ++i) std::printf("  %s\n", textLines[i].c_str());
}

// The comparison can fail and it can pass: the golden against itself with one position moved by a hundredth of the window is a difference, by a
// millionth is not; one number of a fraction moved by a thousandth is a difference; a picture, a clamp, a quad dropped or a frame missing are; and
// for a hidden frame the rectangle its glyphs fill, its count of glyphs, its hiding and the fingerprint are too.
// It needs no original asset - the committed golden alone.
void theNumericGoldenComparesWithATolerance() {
    Golden golden;
    std::string error;
    CHECK_MSG(readGolden(kGoldenDir + "/menu_screens.json", false, golden, error), error);
    if (golden.frames.empty()) return;
    CHECK_MSG(golden.frames.size() >= 30u, "the golden holds the screens: " + std::to_string(golden.frames.size()));
    CHECK_MSG(differences(golden, golden).empty(), "the golden equals itself");
    // The frame with the most quads of those that keep all of theirs.
    GoldenFrames::iterator biggest = golden.frames.end();
    for (auto it = golden.frames.begin(); it != golden.frames.end(); ++it) {
        if (!it->second.hidden && (biggest == golden.frames.end() || it->second.quads.size() > biggest->second.quads.size())) biggest = it;
    }
    CHECK_MSG(biggest != golden.frames.end() && biggest->second.quads.size() >= 100u, "a frame with a good many quads");
    if (biggest == golden.frames.end()) return;
    const std::size_t last = biggest->second.quads.size() - 1;
    const auto moved = [&](std::size_t quad, std::size_t number, double by) {
        Golden copy = golden;
        copy.frames[biggest->first].quads[quad].numbers[number] += by;
        return differences(copy, golden);
    };
    for (const std::size_t quad : {static_cast<std::size_t>(0), last / 2, last}) {
        CHECK_MSG(!moved(quad, 0, 0.01).empty() && !moved(quad, 3, -0.01).empty(), "a position moved by 0.01 (a dozen pixels) is a difference");
        CHECK_MSG(moved(quad, 0, 1.0e-6).empty() && moved(quad, 3, -1.0e-6).empty(), "a position moved by 1e-6 is not");
        CHECK_MSG(!moved(quad, 4, 1.0e-3).empty() && !moved(quad, 11, -1.0e-3).empty(), "a uv or a colour moved by 0.001 is a difference");
        CHECK_MSG(moved(quad, 8, 1.0e-7).empty(), "and by 1e-7 is not");
    }
    {
        Golden copy = golden;
        copy.frames[biggest->first].quads[last].texture = "0000000000000000";
        CHECK_MSG(!differences(copy, golden).empty(), "another picture is a difference");
        copy = golden;
        copy.frames[biggest->first].quads[last].clamp = !copy.frames[biggest->first].quads[last].clamp;
        CHECK_MSG(!differences(copy, golden).empty(), "another clamp is a difference");
        copy = golden;
        copy.frames[biggest->first].quads.pop_back();
        CHECK_MSG(!differences(copy, golden).empty() && !differences(golden, copy).empty(), "a quad fewer, or one more, is a difference");
        copy = golden;
        copy.frames.erase(biggest->first);
        CHECK_MSG(!differences(copy, golden).empty() && !differences(golden, copy).empty(), "a frame missing, or an extra one, is a difference");
        copy = golden;
        copy.frames[biggest->first].glyphs += 1;
        CHECK_MSG(!differences(copy, golden).empty(), "a different count of glyphs is a difference");
    }
    // A hidden frame: the one with the most quads that are not glyphs.
    GoldenFrames::iterator hidden = golden.frames.end();
    for (auto it = golden.frames.begin(); it != golden.frames.end(); ++it) {
        if (it->second.hidden && (hidden == golden.frames.end() || it->second.quads.size() > hidden->second.quads.size())) hidden = it;
    }
    CHECK_MSG(hidden != golden.frames.end() && !hidden->second.quads.empty(), "a hidden frame that keeps some quads");
    if (hidden == golden.frames.end() || hidden->second.quads.empty()) return;
    {
        Golden copy = golden;
        GoldenFrame& frame = copy.frames[hidden->first];
        frame.quads[0].numbers[0] += 0.01;
        CHECK_MSG(!differences(copy, golden).empty(), "a hidden frame's other quads are compared like any: one moved by 0.01 is a difference");
        copy = golden;
        copy.frames[hidden->first].bounds[0] += 0.01;
        CHECK_MSG(!differences(copy, golden).empty(), "the rectangle a hidden frame's glyphs fill, moved by 0.01, is a difference");
        copy = golden;
        copy.frames[hidden->first].bounds[3] -= 0.01;
        CHECK_MSG(!differences(copy, golden).empty(), "and so is its other edge");
        copy = golden;
        copy.frames[hidden->first].bounds[2] += 1.0e-6;
        CHECK_MSG(differences(copy, golden).empty(), "the rectangle moved by 1e-6 is not");
        copy = golden;
        copy.frames[hidden->first].glyphs += 1;
        CHECK_MSG(!differences(copy, golden).empty(), "a hidden frame with another count of glyphs is a difference");
        copy = golden;
        copy.frames[hidden->first].hidden = false;
        CHECK_MSG(!differences(copy, golden).empty() && !differences(golden, copy).empty(), "a frame hidden in one and shown in the other is a difference");
        copy = golden;
        copy.fingerprint = copy.fingerprint == "0000000000000000" ? "1111111111111111" : "0000000000000000";
        CHECK_MSG(!differences(copy, golden).empty(), "another fingerprint is a difference");
    }
}

// The committed golden spells nothing of Asantee's achievements: the frames that draw them are hidden, none holds a glyph quad, and every other
// frame's count of glyphs is its count of quads from the font pages. The suite needs no original asset for this - the page hashes are in the suite.
void theCommittedGoldenHidesAsanteesWords() {
    Golden golden;
    std::string error;
    CHECK_MSG(readGolden(kGoldenDir + "/menu_screens.json", false, golden, error), error);
    if (golden.frames.empty()) return;
    std::size_t hiddenFrames = 0;
    std::size_t hiddenGlyphs = 0;
    for (const auto& [key, frame] : golden.frames) {
        std::size_t drawn = 0;
        for (const GoldenQuad& quad : frame.quads) {
            if (isGlyphId(quad.texture)) ++drawn;
        }
        if (frame.hidden) {
            ++hiddenFrames;
            hiddenGlyphs += frame.glyphs;
            CHECK_MSG(drawn == 0, "\"" + key + "\" is hidden and holds " + std::to_string(drawn) + " glyph quads");
        } else {
            CHECK_MSG(drawn == frame.glyphs, "\"" + key + "\" counts " + std::to_string(frame.glyphs) + " glyphs and holds " + std::to_string(drawn));
        }
    }
    // The frames that draw the achievements: the dashboard at thirteen stops, the popup over the pause and the unlock card.
    CHECK_MSG(hiddenFrames >= 15u, "the achievements frames are hidden: " + std::to_string(hiddenFrames));
    CHECK_MSG(hiddenGlyphs >= 3800u, "and they are the ones with the glyphs: " + std::to_string(hiddenGlyphs));
}

// A glyph quad of the synthetic frames: a page, the cell on it (in 1/512) and a place.
GoldenQuad syntheticGlyph(const std::string& page, int x, int y, int w, int h, double at) {
    GoldenQuad quad;
    quad.numbers = {at, 0.40, at + 0.02, 0.45, x / kPageCells, y / kPageCells, (x + w) / kPageCells, (y + h) / kPageCells, 1.0, 1.0, 1.0, 1.0, 1.0, 0.0, 0.0, 1.0};
    quad.texture = page;
    quad.clamp = true;
    return quad;
}

GoldenQuad syntheticSprite(double at) {
    GoldenQuad quad;
    quad.numbers = {at, 0.10, at + 0.1, 0.20, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 0.0, 0.0, 1.0};
    quad.texture = "00000000000000aa";
    quad.clamp = true;
    return quad;
}

// The fingerprint tells the glyphs and their order and nothing of where they are: a cell changed, two glyphs swapped, one dropped, another page, another
// key are all another fingerprint; a frame moved by the positions' tolerance, or the text file's rounding of the uv, is not. The reader of the committed
// file would refuse the glyph quads a hidden frame must not have.
void theGlyphFingerprintTellsTheSequence() {
    const std::string page = *glyphPageIds().begin();
    const std::string other = *glyphPageIds().rbegin();
    CHECK_MSG(page != other, "two font pages to tell apart");
    GoldenFrames full;
    {
        GoldenFrame& frame = full["b hidden"];
        frame.hidden = true;
        frame.quads = {syntheticSprite(0.1), syntheticGlyph(page, 10, 20, 12, 30, 0.30), syntheticGlyph(page, 30, 20, 14, 30, 0.33),
                       syntheticGlyph(page, 50, 60, 12, 30, 0.36), syntheticGlyph(other, 70, 20, 12, 28, 0.39)};
        frame.glyphs = 4;
    }
    {
        GoldenFrame& frame = full["a hidden"];
        frame.hidden = true;
        frame.quads = {syntheticGlyph(page, 90, 20, 12, 30, 0.50), syntheticSprite(0.2), syntheticGlyph(page, 110, 20, 12, 30, 0.53)};
        frame.glyphs = 2;
    }
    {
        GoldenFrame& frame = full["c shown"];
        frame.quads = {syntheticGlyph(page, 130, 20, 12, 30, 0.60), syntheticSprite(0.3)};
        frame.glyphs = 1;
    }
    Golden base;
    std::string problem;
    CHECK_MSG(redact(full, base, problem), problem);
    CHECK_MSG(base.frames["b hidden"].quads.size() == 1 && base.frames["a hidden"].quads.size() == 1 && base.frames["c shown"].quads.size() == 2,
              "a hidden frame keeps its other quads and loses its glyphs; a shown frame keeps all");
    CHECK_MSG(base.frames["b hidden"].glyphs == 4 && base.frames["a hidden"].glyphs == 2, "and the count of the glyphs");
    const std::array<double, 4> bounds = base.frames["b hidden"].bounds;
    CHECK_MSG(bounds[0] == 0.30 && bounds[2] == 0.39 + 0.02 && bounds[1] == 0.40 && bounds[3] == 0.45, "and the rectangle they fill");
    for (const auto& [key, frame] : base.frames) {
        for (const GoldenQuad& quad : frame.quads) {
            if (frame.hidden) CHECK_MSG(!isGlyphId(quad.texture), "\"" + key + "\": no glyph quad in a hidden frame");
        }
    }
    const auto fingerprintAfter = [&](const std::function<void(GoldenFrames&)>& change) {
        GoldenFrames copy = full;
        change(copy);
        return fingerprintOf(copy);
    };
    const std::string want = fingerprintOf(full);
    CHECK_MSG(want == base.fingerprint && want.size() == 16, "the fingerprint is the one the redaction holds, 16 hex digits");
    // Unchanged by what is not the sequence.
    CHECK_MSG(fingerprintAfter([](GoldenFrames& f) {
                  for (GoldenQuad& quad : f["b hidden"].quads) {
                      quad.numbers[0] += 0.0004;
                      quad.numbers[2] += 0.0004;
                      quad.numbers[1] -= 0.0004;
                  }
              }) == want,
              "glyphs moved by 0.0004 of the window: the same fingerprint");
    CHECK_MSG(fingerprintAfter([](GoldenFrames& f) {
                  for (GoldenQuad& quad : f["b hidden"].quads) quad.numbers[8] = 0.5;
              }) == want,
              "a colour is not in it");
    CHECK_MSG(fingerprintAfter([](GoldenFrames& f) { f["c shown"].quads[0] = syntheticGlyph(f["c shown"].quads[0].texture, 1, 2, 3, 4, 0.9); }) == want,
              "a frame that is not hidden is not in it");
    CHECK_MSG(fingerprintAfter([](GoldenFrames& f) { f["b hidden"].quads[0].numbers[0] += 0.3; }) == want, "nor is a sprite");
    CHECK_MSG(fingerprintAfter([](GoldenFrames& f) {
                  for (GoldenQuad& quad : f["b hidden"].quads) {
                      for (std::size_t i = 4; i < 8; ++i) quad.numbers[i] += 1.0e-5; // the text file's seven digits lose less than this
                  }
              }) == want,
              "a uv off by 1e-5 (the text file's rounding) rounds to the same cell");
    // Changed by the sequence.
    CHECK_MSG(fingerprintAfter([](GoldenFrames& f) {
                  for (std::size_t i = 4; i < 8; i += 2) f["b hidden"].quads[2].numbers[i] += 1.0 / kPageCells;
              }) != want,
              "a glyph one cell to the right: another fingerprint");
    CHECK_MSG(fingerprintAfter([](GoldenFrames& f) { f["b hidden"].quads[3].numbers[7] += 1.0 / kPageCells; }) != want, "a glyph one cell taller: another fingerprint");
    CHECK_MSG(fingerprintAfter([](GoldenFrames& f) { std::swap(f["b hidden"].quads[1], f["b hidden"].quads[2]); }) != want, "two glyphs swapped: another fingerprint");
    CHECK_MSG(fingerprintAfter([](GoldenFrames& f) { f["b hidden"].quads.erase(f["b hidden"].quads.begin() + 2); }) != want, "a glyph fewer: another fingerprint");
    CHECK_MSG(fingerprintAfter([](GoldenFrames& f) { f["b hidden"].quads[1].texture = f["b hidden"].quads[4].texture == f["b hidden"].quads[1].texture ? "x" : f["b hidden"].quads[4].texture; }) != want,
              "a glyph from another page: another fingerprint");
    CHECK_MSG(fingerprintAfter([](GoldenFrames& f) {
                  f["z hidden"] = f["a hidden"];
                  f.erase("a hidden");
              }) != want,
              "the same glyphs under another key: another fingerprint");
    CHECK_MSG(fingerprintAfter([](GoldenFrames& f) { f["c shown"].hidden = true; }) != want, "a frame that begins to be hidden: another fingerprint");
    // The draw order of text and panels: a sprite drawn after a glyph that was drawn before it (text under a panel) is another fingerprint, though the
    // glyphs, their order and every sprite are the same.
    CHECK_MSG(fingerprintAfter([](GoldenFrames& f) { std::swap(f["b hidden"].quads[0], f["b hidden"].quads[1]); }) != want,
              "a sprite drawn after the first glyph instead of before it: another fingerprint");
    CHECK_MSG(fingerprintAfter([](GoldenFrames& f) { std::swap(f["a hidden"].quads[1], f["a hidden"].quads[2]); }) != want,
              "a sprite drawn after the last glyph instead of between two: another fingerprint");
    CHECK_MSG(fingerprintAfter([](GoldenFrames& f) { f["b hidden"].quads.erase(f["b hidden"].quads.begin()); }) != want,
              "a sprite fewer before the glyphs: another fingerprint");
    // The text file writes the uv with seven digits: every cell of a 512 page survives it.
    bool rounded = true;
    for (int cell = 0; cell <= 512; ++cell) {
        char text[40];
        std::snprintf(text, sizeof(text), "%.7g", static_cast<double>(static_cast<float>(cell / kPageCells)));
        rounded = rounded && cellOf(std::strtod(text, nullptr)) == cell;
    }
    CHECK_MSG(rounded, "every cell of a page round-trips through the text file's seven digits");
    // The comparison of two redactions: a hidden frame moved within the tolerance is equal, beyond it is not.
    GoldenFrames shifted = full;
    for (GoldenQuad& quad : shifted["b hidden"].quads) {
        quad.numbers[0] += 0.0004;
        quad.numbers[2] += 0.0004;
    }
    Golden shiftedGolden;
    CHECK_MSG(redact(shifted, shiftedGolden, problem) && differences(shiftedGolden, base).empty(), "glyphs shifted by 0.0004: equal to the golden");
    for (GoldenQuad& quad : shifted["b hidden"].quads) {
        quad.numbers[0] += 0.01;
        quad.numbers[2] += 0.01;
    }
    CHECK_MSG(redact(shifted, shiftedGolden, problem) && !differences(shiftedGolden, base).empty(), "glyphs shifted by 0.0104: not");
    // A glyph from a page the list does not know is a problem the redaction names, not a glyph left in a hidden frame.
    GoldenFrames unknown = full;
    unknown["b hidden"].glyphs = 5;
    Golden unused;
    CHECK_MSG(!redact(unknown, unused, problem) && problem.find("kOriginalFontPages") != std::string::npos, "a count of glyphs the pages do not explain is refused");
}

// The reader knows format 3 and nothing else.
void theReaderRefusesWhatItDoesNotKnow() {
    const std::string glyph = *glyphPageIds().begin();
    const std::string quadHead = "[0.1,0.2,0.3,0.4,0.1,0.2,0.3,0.4,1,1,1,1,1,0,0,1,\"";
    const std::string glyphQuad = quadHead + glyph + "\",1]";
    const std::string sprite = quadHead + "00000000000000aa\",1]";
    const std::string fingerprint = "\"fingerprint\": \"0123456789abcdef\"";
    const std::string visible = "\"v\": {\"glyphs\": 1, \"quads\": [" + glyphQuad + "," + sprite + "]}";
    const std::string hidden = "\"h\": {\"glyphs\": 2, \"hidden\": {\"bounds\": [0.1,0.2,0.3,0.4]}, \"quads\": [" + sprite + "]}";
    const auto doc = [&](const std::string& format, const std::string& extraTop, const std::string& frames, const std::string& print = std::string()) {
        return "{\"format\": " + format + ", " + (print.empty() ? fingerprint : print) + extraTop + ", \"frames\": {" + frames + "}}";
    };
    const auto reads = [](const std::string& text, bool local = false) {
        Golden golden;
        std::string error;
        return parseGolden(text, local, golden, error);
    };
    CHECK_MSG(reads(doc("3", "", visible + "," + hidden)), "the format it knows, with a shown and a hidden frame, is read");
    CHECK_MSG(!reads(doc("2", "", visible + "," + hidden)), "format 2 (every glyph quad stored) is refused");
    CHECK_MSG(!reads(doc("4", "", visible + "," + hidden)), "a format from the future is refused");
    CHECK_MSG(!reads(doc("3.5", "", visible + "," + hidden)), "and one in between");
    CHECK_MSG(!reads("{\"fingerprint\": \"0123456789abcdef\", \"frames\": {}}"), "no format is refused");
    CHECK_MSG(!reads(doc("3", ", \"extra\": 1", visible + "," + hidden)), "a top-level key it was not told of is refused");
    CHECK_MSG(!reads(doc("3", "", "\"v\": {\"glyphs\": 1, \"text\": 1, \"quads\": [" + glyphQuad + "]}")), "a member of a frame it was not told of is refused");
    CHECK_MSG(!reads(doc("3", "", "\"h\": {\"glyphs\": 2, \"hidden\": {\"bounds\": [0.1,0.2,0.3,0.4], \"cells\": [1]}, \"quads\": [" + sprite + "]}")),
              "so is a member of the hidden mark");
    CHECK_MSG(!reads(doc("3", "", "\"h\": {\"glyphs\": 2, \"hidden\": {\"bounds\": [0.1,0.2,0.3]}, \"quads\": [" + sprite + "]}")), "so is a hidden mark with three bounds");
    CHECK_MSG(!reads(doc("3", "", "\"h\": {\"glyphs\": 2, \"hidden\": {\"bounds\": [0.1,0.2,0.3,0.4]}, \"quads\": [" + glyphQuad + "," + sprite + "]}")),
              "a hidden frame with a glyph quad in it is refused");
    CHECK_MSG(!reads(doc("3", "", "\"v\": {\"glyphs\": 2, \"quads\": [" + glyphQuad + "," + sprite + "]}")), "a frame that counts more glyphs than it holds is refused");
    CHECK_MSG(!reads(doc("3", "", "\"v\": {\"glyphs\": 0, \"quads\": [" + glyphQuad + "," + sprite + "]}")), "and one that counts fewer");
    CHECK_MSG(!reads(doc("3", "", "\"h\": {\"glyphs\": 0, \"hidden\": {\"bounds\": [0.1,0.2,0.3,0.4]}, \"quads\": [" + sprite + "]}")), "a hidden frame with no glyph is refused");
    CHECK_MSG(!reads(doc("3", "", visible, "\"fingerprint\": \"0123\"")), "a fingerprint that is not 16 digits is refused");
    CHECK_MSG(!reads(doc("3", "", visible, "\"fingerprint\": \"0123456789ABCDEF\"")), "or not lower-case hex");
    CHECK_MSG(!reads("{\"format\": 3, \"frames\": {" + visible + "}}"), "or none");
    CHECK_MSG(!reads(doc("3", ", \"local_only\": true", visible)), "the committed file may not carry the local mark");
    const std::string localFrame = "\"v\": {\"glyphs\": 1, \"quads\": [" + glyphQuad + "]}";
    const std::string localDoc = "{\"format\": 3, \"local_only\": true, \"frames\": {" + localFrame + "}}";
    CHECK_MSG(reads(localDoc, true), "the local text golden is read as one: glyph quads, the mark, no fingerprint");
    CHECK_MSG(!reads(localDoc, false), "and refused as the committed one (no fingerprint)");
    CHECK_MSG(!reads("{\"format\": 3, \"frames\": {" + localFrame + "}}", true), "a local file without its mark is refused");
    CHECK_MSG(!reads("{\"format\": 3, \"local_only\": true, \"frames\": {\"v\": {\"glyphs\": 1, \"quads\": [" + sprite + "]}}}", true),
              "a local file with a quad that is not a glyph is refused");
    CHECK_MSG(!reads("{\"format\": 3, \"local_only\": true, " + fingerprint + ", \"frames\": {" + localFrame + "}}", true), "a local file with a fingerprint is refused");
    CHECK_MSG(!reads("not json"), "and text that is not JSON");
}

// The page list in this suite is the original's: the pages the seven .fnt files name, all 512 x 512 (the cell of a glyph is its uv times 512).
void theGlyphPagesAreTheOriginalsPages() {
    if (!originalArtIsThere()) {
        std::printf("  theGlyphPagesAreTheOriginalsPages SKIPPED - needs the original's extracted assets\n");
        return;
    }
    CHECK_MSG(originalFonts().size() == kOriginalFonts.size(), "all of the original's fonts are read: " + std::to_string(originalFonts().size()));
    std::set<std::string> pages;
    for (const Supersonic::BitmapFont& font : originalFonts()) {
        CHECK_MSG(font.PageSize() == glm::ivec2(512, 512), "a font page is 512 x 512");
        for (const std::string& page : font.Pages()) pages.insert(std::filesystem::path(page).filename().string());
    }
    const std::set<std::string> listed(kOriginalFontPages.begin(), kOriginalFontPages.end());
    CHECK_MSG(pages == listed, "the pages of the fonts are kOriginalFontPages: " + std::to_string(pages.size()) + " against " + std::to_string(listed.size()));
}

// A quad per glyph of `line` in `font`, as the layer draws them.
std::vector<ScreenOverlay::Quad> quadsOfLine(const Supersonic::BitmapFont& font, const std::string& line) {
    std::vector<ScreenOverlay::Quad> quads;
    const auto& pages = font.Pages();
    for (const Hud::Glyph& glyph : Hud::LayOutTextFrom(font, line, glm::dvec2(0.0), 1.0)) {
        if (glyph.page < 0 || static_cast<std::size_t>(glyph.page) >= pages.size()) continue;
        ScreenOverlay::Quad quad;
        quad.uvMin = glm::vec2(glyph.uvOffset);
        quad.uvMax = glm::vec2(glyph.uvOffset + glyph.uvScale);
        quad.texture = pages[static_cast<std::size_t>(glyph.page)];
        quads.push_back(quad);
    }
    return quads;
}

// The guard can see what it guards: a line of Asantee's list laid out through a font is found in a frame, among other glyphs and with the quads of
// pictures between; one glyph moved by a cell, or two swapped, is not. It needs the achievements list and the original's fonts.
void theGuardSeesAsanteesWordsWhereverTheyAreDrawn() {
    if (!dataIsThere("theGuardSeesAsanteesWordsWhereverTheyAreDrawn")) return;
    CHECK_MSG(originalFonts().size() == kOriginalFonts.size(), "the original's fonts are read");
    if (originalFonts().size() != kOriginalFonts.size()) return;
    const Supersonic::BitmapFont& verdana = originalFonts().back();
    std::string line;
    for (const std::string& candidate : asanteeLines()) {
        if (cellsOfLine(verdana, candidate).size() >= kMinSpelledGlyphs + 4) {
            line = candidate;
            break;
        }
    }
    CHECK_MSG(!line.empty(), "a line of the list that Verdana40 draws in ten cells or more");
    if (line.empty()) return;
    const std::vector<ScreenOverlay::Quad> words = quadsOfLine(verdana, line);
    CHECK_MSG(spelledLines(words) >= 1u, "the line, laid out, is seen");
    std::vector<ScreenOverlay::Quad> frame = quadsOfLine(originalFonts()[4], "Magic Portals Enhanced");
    ScreenOverlay::Quad picture;
    picture.texture = "icon.png";
    frame.push_back(picture);
    frame.insert(frame.end(), words.begin(), words.end());
    frame.push_back(picture);
    const std::vector<ScreenOverlay::Quad> after = quadsOfLine(verdana, "Credits");
    frame.insert(frame.end(), after.begin(), after.end());
    CHECK_MSG(spelledLines(frame) >= 1u, "and seen among other glyphs, pictures between");
    CHECK_MSG(spelledLines(quadsOfLine(originalFonts()[4], "Magic Portals Enhanced")) == 0u, "Enhanced's own words are not Asantee's");
    {
        std::vector<ScreenOverlay::Quad> moved = words;
        for (ScreenOverlay::Quad& quad : moved) {
            quad.uvMin.x += static_cast<float>(1.0 / kPageCells);
            quad.uvMax.x += static_cast<float>(1.0 / kPageCells);
        }
        CHECK_MSG(spelledLines(moved) == 0u, "every glyph a cell to the right: not seen");
    }
    {
        std::vector<ScreenOverlay::Quad> swapped = words;
        std::size_t at = 0;
        while (at + 1 < swapped.size() && cellOfQuad(swapped[at]) == cellOfQuad(swapped[at + 1])) ++at;
        std::swap(swapped[at], swapped[at + 1]);
        CHECK_MSG(spelledLines(swapped) < spelledLines(words), "two glyphs swapped: the line is gone");
    }
    {
        std::vector<ScreenOverlay::Quad> fewer = words;
        fewer.erase(fewer.begin() + static_cast<std::ptrdiff_t>(fewer.size() / 2));
        CHECK_MSG(spelledLines(fewer) < spelledLines(words), "a glyph fewer: the line is gone");
    }
    // The guard's verdict on a set of frames, both ways.
    Entries frames;
    frames["words, flagged"] = captured(words, true);
    CHECK_MSG(guardProblems(frames).empty(), "a frame that spells them and is flagged is fine");
    frames["words, not flagged"] = captured(words, false);
    CHECK_MSG(guardProblems(frames).size() == 1u, "the same frame not flagged is a problem");
    frames["words, not flagged"] = captured(quadsOfLine(verdana, "Credits"), true);
    CHECK_MSG(guardProblems(frames).size() == 1u, "and a flagged frame that spells none is one too");
}

bool writeGolden(const std::string& path, const std::string& about, const Golden& golden, bool local) {
    std::error_code ec;
    if (std::filesystem::exists(path, ec)) {
        std::printf("test_mp_langlayer: %s exists; a golden is never overwritten (delete it by hand to record it again)\n", path.c_str());
        return false;
    }
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        std::printf("test_mp_langlayer: cannot write %s\n", path.c_str());
        return false;
    }
    out << "{\n  \"_about\": \"" << about << "\",\n  \"format\": " << kGoldenFormat << ",\n";
    if (local) {
        out << "  \"local_only\": true,\n";
    } else {
        out << "  \"fingerprint\": \"" << golden.fingerprint << "\",\n";
    }
    out << "  \"frames\": {\n";
    std::size_t left = golden.frames.size();
    for (const auto& [key, frame] : golden.frames) {
        out << "    \"" << key << "\": {\"glyphs\": " << frame.glyphs;
        if (frame.hidden) {
            out << ", \"hidden\": {\"bounds\": [";
            for (std::size_t i = 0; i < frame.bounds.size(); ++i) {
                char text[40];
                std::snprintf(text, sizeof(text), "%.4f", frame.bounds[i]);
                out << text << (i + 1 < frame.bounds.size() ? "," : "");
            }
            out << "]}";
        }
        out << ", \"quads\": [\n";
        for (std::size_t q = 0; q < frame.quads.size(); ++q) {
            const GoldenQuad& quad = frame.quads[q];
            out << "      [";
            for (std::size_t i = 0; i < quad.numbers.size(); ++i) {
                const double number = quad.numbers[i] == 0.0 ? 0.0 : quad.numbers[i]; // never "-0"
                char text[40];
                // The rectangle to ten-thousandths of the window (its tolerance is a thousandth), the fractions to seven significant digits (theirs is
                // 1e-5): short for the 0s and 1s most of them are.
                std::snprintf(text, sizeof(text), i < kPositionNumbers ? "%.4f" : "%.7g", number);
                out << text << ",";
            }
            out << "\"" << quad.texture << "\"," << (quad.clamp ? 1 : 0) << "]" << (q + 1 < frame.quads.size() ? "," : "") << "\n";
        }
        out << "    ]}" << (--left > 0 ? "," : "") << "\n";
    }
    out << "  }\n}\n";
    std::printf("test_mp_langlayer: wrote %zu frames to %s\n", golden.frames.size(), path.c_str());
    return true;
}

int recordGolden(const std::string& dir) {
    if (!dataIsThere("--record-golden")) return 77;
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    const Entries frames = englishFrames();
    const std::vector<std::string> problems = guardProblems(frames);
    for (const std::string& problem : problems) std::printf("test_mp_langlayer: %s\n", problem.c_str());
    if (!problems.empty()) return 1;
    const GoldenFrames full = fullFramesOf(frames);
    Golden golden;
    std::string problem;
    if (full.empty() || !redact(full, golden, problem)) {
        std::printf("test_mp_langlayer: nothing to record: %s\n", problem.c_str());
        return 1;
    }
    // Both files or neither: the committed one in `dir`, and the local text golden beside the achievements list, which is not in the repository.
    const std::string committed = dir + "/menu_screens.json";
    const std::filesystem::path local = localTextGoldenPath();
    if (!underAnOutFolder(local)) {
        std::printf("test_mp_langlayer: %s is not under a folder called out, which .gitignore covers; the file spells Asantee's words and is not written there\n",
                    local.generic_string().c_str());
        return 1;
    }
    if (std::filesystem::exists(committed, ec) || std::filesystem::exists(local, ec)) {
        std::printf("test_mp_langlayer: %s or %s exists; a golden is never overwritten (delete both by hand to record them again)\n", committed.c_str(),
                    local.generic_string().c_str());
        return 1;
    }
    const std::string about =
        "The English frames of every menu screen and the level texts, as numbers: every quad EmitMenu, EmitHud and EmitNotifications produce "
        "(planning step 238, PORT-34) - the rectangle, the uv, the colour and the turn, the picture's file name as a hash and the clamp. "
        "A suite compares what the layer draws now with these within a tolerance (the rectangle, in fractions of the window, 0.001; the other "
        "fractions 0.00001), so "
        "that a last-bit difference of the C library's sin and cos does not fail it and a moved glyph does. The numbers were dumped from "
        "the wired layer in the very run that first checked the layer's frames against the bit-hash golden recorded BEFORE the "
        "languages were wired in (every frame equal, bit for bit), so the default language is proven to draw what it always drew. "
        "A golden is never re-recorded to make a change pass: if English changed on purpose, the step says so and records why. "
        "FORMAT 3 (review of steps 236 to 239, finding F3): a glyph quad's uv rectangle is the character's cell on the original font's page, so the glyph "
        "quads of a frame that draws Asantee's achievements words (the dashboard, the achievements popup, a card for an achievement - the frames marked "
        "hidden) would spell them out of this file with the original's .fnt files. Those frames therefore carry no glyph quad here: only their other "
        "quads, their number of glyphs, the rectangle the glyphs fill, and ONE fingerprint (the top-level one) of the glyph sequences of all the hidden "
        "frames together - cells of the font pages in draw order, with how many other quads were drawn before each, no place. Their glyphs in full are in menu_screens_text_golden.json beside "
        "out/data/achievements.json (gitignored, never committed), written by this same command and compared when it is there. "
        "What is still stored and true: no pixel; no word of Asantee's achievements; but the glyph quads of every other frame, which spell only "
        "Enhanced's own words (game/data/lang), numbers and percents, and so are still the original fonts' atlas geometry for those characters.";
    const bool wrote = writeGolden(committed, about, golden, false) &&
        writeGolden(local.string(),
                    "LOCAL ONLY, GITIGNORED (out/data is not in the repository): the glyph quads of the frames that draw Asantee's achievements words, in full "
                    "(the dashboard at thirteen stops, the achievements popup, the unlock card). They SPELL those words with the original's .fnt files. "
                    "Never commit, copy or paste this file: tests/golden/menu_screens.json holds only their count, rectangle and one fingerprint. "
                    "Written by test_mp test_mp_langlayer --record-golden, compared by the same suite when this file exists.",
                    Golden{glyphsOfHiddenFrames(full), std::string()}, true);
    return wrote ? ::test::summary("test_mp_langlayer --record-golden", 0) : 1;
}

// ============================================================================================================================
// PART TWO - the layer speaks each language
// ============================================================================================================================

namespace Strings = MagicPortals::Strings;
namespace FontBake = MagicPortals::FontBake;
namespace Language = MagicPortals::Language;
using Strings::Lang;

const std::string kOriginal = MAGICPORTALS_ORIGINAL_DIR;

MagicPortalsLayer::Paths langPaths(const std::string& name, const std::optional<std::string>& forced = std::nullopt) {
    MagicPortalsLayer::Paths paths = testPaths("langlayer-" + name);
    // A clean scratch folder: the fonts a case expects are baked by it, not left by an earlier run.
    std::error_code ec;
    std::filesystem::remove_all(paths.prisms, ec);
    paths.language.forced = forced;
    return paths;
}

// A bare layer on a registry, attached.
struct Run {
    entt::registry registry;
    MagicPortalsLayer layer;
    Run(const MagicPortalsLayer::Paths& paths, const std::string& level) : layer(paths, level) {
        publishViewport(registry);
        layer.OnAttach(registry);
    }
    ~Run() { layer.OnDetach(registry); }
    Run(const Run&) = delete;
    Run& operator=(const Run&) = delete;
};

std::string codeOf(Lang lang) {
    return std::string(Strings::kLangCodes[Strings::IndexOf(lang)]);
}

// Whether the picture a glyph quad names is one of the generated font's pages for `code` (Matura84_shadow_ru_0.png): the original's are
// Matura84_shadow_0.png and the like.
bool isGeneratedPage(const std::string& texture, const std::string& code) {
    return texture.find("_" + code + "_") != std::string::npos;
}

std::vector<const ScreenOverlay::Quad*> glyphQuadsOf(const std::vector<ScreenOverlay::Quad>& quads) {
    std::vector<const ScreenOverlay::Quad*> found;
    for (const ScreenOverlay::Quad& quad : quads) {
        if (quad.texture.find("Matura") != std::string::npos || quad.texture.find("Verdana") != std::string::npos) found.push_back(&quad);
    }
    return found;
}

// The letters of a frame come from the language's own font and from no other: every generated language's quads name its pages, and a language
// set in the original's fonts names none of a generated one's.
void checkFontsOfFrame(const std::vector<ScreenOverlay::Quad>& frame, const MagicPortalsLayer& layer, const std::string& what) {
    const std::string code = codeOf(layer.CurrentLanguage());
    const auto glyphs = glyphQuadsOf(frame);
    CHECK_MSG(!glyphs.empty(), what + ": the frame draws letters");
    for (const ScreenOverlay::Quad* quad : glyphs) {
        const bool generated = isGeneratedPage(quad->texture, code);
        if (layer.GeneratedFonts()) {
            // Matura128 (the counters' digits) stays the original's in every language.
            CHECK_MSG(generated || quad->texture.find("Matura128") != std::string::npos,
                      what + ": a letter of the generated language is drawn from " + quad->texture);
        } else {
            CHECK_MSG(!generated, what + ": a letter of the original's language is drawn from a generated page: " + quad->texture);
        }
    }
}

// The path of the first reasonable place where a level's walk-to-the-door works, as the snapshot drives it.
void walkToTheDoor(MagicPortalsLayer& layer, entt::registry& registry) {
    for (int tick = 0; tick < 600 && !layer.Finishing(); ++tick) {
        std::vector<std::string> pressed;
        if (tick == 0) pressed.push_back(MagicPortalsLayer::kRight);
        tickWith(layer, registry, kRest, {MagicPortalsLayer::kRight}, std::move(pressed));
    }
}

std::string translationFile(const std::string& code) {
    return (std::filesystem::path(kAchievements).parent_path() / ("achievements." + code + ".json")).string();
}

// ---- the flag starts every language ----------------------------------------------------------------------------------------------

void everyLanguageStartsFromTheFlag() {
    if (!originalArtIsThere()) {
        std::printf("  everyLanguageStartsFromTheFlag SKIPPED - needs the original's extracted assets\n");
        return;
    }
    for (std::size_t i = 0; i < Strings::kLangCount; ++i) {
        const Lang lang = static_cast<Lang>(i);
        const std::string code = codeOf(lang);
        const MagicPortalsLayer::Paths paths = langPaths("start-" + code, code);
        // The cost of the start: the scratch folder is empty, so a generated language bakes both its fonts here; a second start on the same folder finds them.
        const auto began = std::chrono::steady_clock::now();
        Run run(paths, "");
        const double coldMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
        const auto again = std::chrono::steady_clock::now();
        { Run cached(paths, ""); }
        const double cachedMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - again).count();
        std::printf("  %s: attach %.0f ms from an empty scratch folder, %.0f ms with the fonts already there\n", code.c_str(), coldMs, cachedMs);
        const MagicPortalsLayer& layer = run.layer;
        CHECK_MSG(layer.LanguageReady() && layer.CurrentLanguage() == lang && layer.RequestedLanguage() == lang, code + ": the flag's language is the one on the screen");
        const bool generated = layer.StringTable().Languages().FontSetOf(lang) == Strings::FontSet::Generated;
        CHECK_MSG(layer.GeneratedFonts() == generated, code + ": fonts are baked exactly for the languages whose font_set says so");
        for (const std::string& note : layer.LanguageNotes()) {
            CHECK_MSG(note.find("font") == std::string::npos, code + ": no note about fonts: " + note);
        }
        const std::string matura = layer.UiFontFile("Matura84_shadow.fnt");
        const std::string verdana = layer.UiFontFile("Verdana40_shadow.fnt");
        std::error_code ec;
        if (generated) {
            const std::string folder = (paths.prisms / FontBake::kScratchFolder).generic_string();
            CHECK_MSG(std::filesystem::path(matura).generic_string() == folder + "/Matura84_shadow_" + code + ".fnt" && std::filesystem::is_regular_file(matura, ec),
                      code + ": Matura's slot is baked into the scratch folder: " + matura);
            CHECK_MSG(std::filesystem::path(verdana).generic_string() == folder + "/Verdana40_shadow_" + code + ".fnt" && std::filesystem::is_regular_file(verdana, ec),
                      code + ": and Verdana's: " + verdana);
            const BitmapFont* font = run.layer.UiFontOf("Matura84_shadow.fnt");
            CHECK_MSG(font != nullptr && font->IsLoaded(), code + ": the engine reads the baked font");
        } else {
            CHECK_MSG(matura == kOriginal + "/data/Matura84_shadow.fnt" && verdana == kOriginal + "/data/Verdana40_shadow.fnt",
                      code + ": a language set in the original's bitmaps reads the original's files: " + matura);
        }
        CHECK_MSG(layer.UiFontFile("Matura128_shadow.fnt") == kOriginal + "/data/Matura128_shadow.fnt", code + ": the counters' digits are the original's in every language");
        CHECK_MSG(!layer.LanguageStore().Persists() && !layer.LanguageStore().FromFile(), code + ": a forced run neither reads nor can write the saved choice");
    }
}

// ---- the level's caption ----------------------------------------------------------------------------------------------------------

void theCaptionAndThePauseSpeakEachLanguage() {
    if (!dataIsThere("theCaptionAndThePauseSpeakEachLanguage")) return;
    for (const char* code : {"de", "ru", "ja", "ar"}) {
        Run run(langPaths(std::string("caption-") + code, std::string(code)), "level0");
        MagicPortalsLayer& layer = run.layer;
        CHECK_MSG(layer.SimLevel() != nullptr, std::string(code) + ": " + layer.LoadError());
        if (layer.SimLevel() == nullptr) continue;
        rest(layer, run.registry, 30);
        const std::string expected = layer.StringTable().Show(Strings::Key::kHudPart, {{Strings::Placeholder::kN, "1"}});
        CHECK_MSG(layer.CaptionWords() == expected && expected != "Part 1", std::string(code) + ": the caption is the table's hud.part for level 1: " + layer.CaptionWords());
        const std::vector<ScreenOverlay::Quad> frame = hudFrame(run.registry, layer);
        const BitmapFont* font = layer.UiFontOf("Matura84_shadow.fnt");
        CHECK_MSG(font != nullptr, std::string(code) + ": the caption's font loads");
        if (font == nullptr) continue;
        std::vector<std::uint32_t> holes;
        const std::size_t laid = Hud::LayOutText(*font, expected, glm::dvec2(0.0), 1.0, &holes).size();
        CHECK_MSG(holes.empty(), std::string(code) + ": the caption has no hole in its font");
        const std::size_t drawn = static_cast<std::size_t>(std::count_if(frame.begin(), frame.end(), [](const ScreenOverlay::Quad& quad) {
            return quad.texture.find("Matura84_shadow") != std::string::npos;
        }));
        CHECK_MSG(drawn == laid && laid > 0, std::string(code) + ": the frame draws exactly the caption's letters in Matura84: " + std::to_string(drawn) + " of " + std::to_string(laid));
        checkFontsOfFrame(frame, layer, std::string(code) + " caption");
    }
    // English is the rules' own, still.
    {
        Run run(langPaths("caption-en"), "level0");
        rest(run.layer, run.registry, 30);
        CHECK_MSG(run.layer.CaptionWords() == "Part 1" && run.layer.CurrentLanguage() == Lang::En && !run.layer.LanguageStore().Persists(), "English: Part 1");
    }
    // The pause: its title is the same words, drawn from the language's fonts.
    {
        Run run(langPaths("pause-ru", "ru"), "level30");
        MagicPortalsLayer& layer = run.layer;
        CHECK_MSG(layer.SimLevel() != nullptr, layer.LoadError());
        if (layer.SimLevel() == nullptr) return;
        rest(layer, run.registry, 120);
        Hud::Rect control;
        CHECK(layer.ControlRect(MagicPortalsLayer::Control::Menu, control));
        tickWith(layer, run.registry, screenOfView(layer, control.Centre()), {MagicPortalsLayer::kTap}, {MagicPortalsLayer::kTap});
        rest(layer, run.registry, 60);
        CHECK_MSG(layer.Paused(), "the pause is up");
        const std::string title = MagicPortals::Pause::TitleText(layer.PauseRules(), layer.PausedLevel(), &layer.StringTable());
        CHECK_MSG(title == layer.StringTable().Show(Strings::Key::kHudPart, {{Strings::Placeholder::kN, std::to_string(layer.PausedLevel().index + 1)}}) && title != "Part 31",
                  "the pause's title: " + title);
        checkFontsOfFrame(hudFrame(run.registry, layer), layer, "the pause in Russian");
        // And its achievements popup, over it: shown in Russian (the dashboard's file), from the generated Verdana.
        if (layer.AchievementsContent() != nullptr && std::filesystem::is_regular_file(translationFile("ru"))) {
            CHECK(layer.PressPause(run.registry, MagicPortals::Pause::Button::Achievements) && layer.AchievementsPopupOpen());
            rest(layer, run.registry, 60);
            Achievements::Translation translation;
            std::string error;
            CHECK_MSG(Achievements::LoadTranslation(translationFile("ru"), "ru", translation, error), error);
            const Achievements::Entry* entry = layer.AchievementsContent()->Find(13);
            CHECK_MSG(entry != nullptr && entry->title == translation.words.at(13).title, "the popup's entry is the Russian one");
            CHECK_MSG(layer.AchievementsPopupText().find(translation.words.at(13).title) != std::string::npos &&
                          layer.AchievementsPopupText().find(translation.words.at(13).description) != std::string::npos,
                      "and the popup text holds its Russian title and description");
            checkFontsOfFrame(hudFrame(run.registry, layer), layer, "the achievements popup in Russian");
        }
    }
    // The popup's text is shaped as it is composed: in Arabic it is the shaped form of the logical text (and not the logical text), in Russian they are one.
    for (const char* code : {"ru", "ar"}) {
        if (!std::filesystem::is_regular_file(translationFile(code))) continue;
        Run run(langPaths(std::string("popup-") + code, std::string(code)), "level30");
        MagicPortalsLayer& layer = run.layer;
        CHECK_MSG(layer.SimLevel() != nullptr && layer.AchievementsContent() != nullptr, layer.LoadError());
        if (layer.SimLevel() == nullptr || layer.AchievementsContent() == nullptr) continue;
        rest(layer, run.registry, 120);
        Hud::Rect control;
        CHECK(layer.ControlRect(MagicPortalsLayer::Control::Menu, control));
        tickWith(layer, run.registry, screenOfView(layer, control.Centre()), {MagicPortalsLayer::kTap}, {MagicPortalsLayer::kTap});
        rest(layer, run.registry, 60);
        CHECK(layer.PressPause(run.registry, MagicPortals::Pause::Button::Achievements) && layer.AchievementsPopupOpen());
        const MagicPortals::Chapters::Level* level = layer.Current();
        CHECK(level != nullptr);
        if (level == nullptr) continue;
        const std::string logical = Achievements::PopupText(layer.AchievementRules(), *layer.AchievementsContent(),
                                                            Achievements::FromLevel(*layer.AchievementsContent(), level->world, level->index),
                                                            layer.AchievementStore().Sorted());
        CHECK_MSG(layer.AchievementsPopupText() == layer.StringTable().Shape(logical), std::string(code) + ": the popup is the logical text, shaped once");
        CHECK_MSG((layer.AchievementsPopupText() != logical) == (std::string(code) == "ar"), std::string(code) + ": and it differs from the logical text exactly where a language is shaped");
    }
}

// ---- the dashboard ------------------------------------------------------------------------------------------------------------------

void theDashboardShowsTheTranslatedRows() {
    if (!dataIsThere("theDashboardShowsTheTranslatedRows")) return;
    for (const char* code : {"de", "ru", "ja", "ar"}) {
        const std::string name(code);
        Run run(langPaths("dashboard-" + name, name), "");
        MagicPortalsLayer& layer = run.layer;
        if (!layer.LoadError().empty() || !tickToTheMainMenu(layer, run.registry)) {
            CHECK_MSG(false, name + ": no main menu");
            continue;
        }
        rest(layer, run.registry, 60);
        CHECK(layer.PressMainMenu(run.registry, MagicPortals::MainMenu::Button::Achievements));
        rest(layer, run.registry, 90);
        const MagicPortals::Dashboard::Board& board = layer.DashboardBoard();
        CHECK_MSG(board.lines.size() > 4u, name + ": the board has rows");
        const bool haveFile = std::filesystem::is_regular_file(translationFile(name));
        Achievements::Translation translation;
        std::string error;
        if (haveFile) CHECK_MSG(Achievements::LoadTranslation(translationFile(name), name, translation, error), error);
        int checked = 0;
        for (const MagicPortals::Dashboard::Line& line : board.lines) {
            if (line.header || line.entry < 0) continue;
            const Achievements::Entry& entry = layer.AchievementsContent()->entries[static_cast<std::size_t>(line.entry)];
            if (entry.isSecret) continue;
            const std::string title = haveFile ? translation.words.at(entry.id).title : entry.title;
            const std::string description = haveFile ? translation.words.at(entry.id).description : entry.description;
            // A locked level's row shows the achievement's own words unless it is secret: shown, shaped for the language.
            CHECK_MSG(line.title == layer.StringTable().Shape(title) && line.description == layer.StringTable().Shape(description),
                      name + ": row of achievement " + std::to_string(entry.id) + " is in the language");
            if (++checked >= 12) break;
        }
        CHECK_MSG(checked >= 5, name + ": enough rows checked: " + std::to_string(checked));
        // A secret achievement that is still locked shows the overlay's `secret` words with its own chapter and level in the sentence.
        int secretRows = 0;
        for (const MagicPortals::Dashboard::Line& line : board.lines) {
            if (line.header || line.entry < 0 || !haveFile) continue;
            const Achievements::Entry& entry = layer.AchievementsContent()->entries[static_cast<std::size_t>(line.entry)];
            if (!entry.isSecret || line.unlocked) continue;
            const std::string sentence = Strings::Fill(translation.secret.description, {{Strings::Placeholder::kWorld, std::to_string(entry.world + 1)},
                                                                                        {Strings::Placeholder::kLevel, std::to_string(entry.level + 1)}});
            CHECK_MSG(line.title == layer.StringTable().Shape(translation.secret.title) && line.description == layer.StringTable().Shape(sentence),
                      name + ": the locked secret row of achievement " + std::to_string(entry.id) + " is the overlay's words");
            ++secretRows;
        }
        if (haveFile) CHECK_MSG(secretRows > 0, name + ": a locked secret row was checked");
        // The pieces: the first header is the table's dashboard.chapter, carried to the font by the layer.
        for (const MagicPortals::Dashboard::Piece& piece : MagicPortals::Dashboard::Pieces(layer.DashboardRules(), board, layer.DashboardState(), layer.MenuBoxPx(),
                                                                                           5000.0, 0.0, &layer.StringTable())) {
            if (piece.kind == MagicPortals::Dashboard::Kind::Text && piece.centred && piece.file == layer.DashboardRules().header.font && piece.words != "0") {
                CHECK_MSG(piece.words == layer.StringTable().Show(Strings::Key::kDashboardChapter, {{Strings::Placeholder::kN, "1"}}), name + ": the header: " + piece.words);
                break;
            }
        }
        const std::vector<ScreenOverlay::Quad> frame = menuFrame(run.registry, layer);
        checkFontsOfFrame(frame, layer, name + " dashboard");
        // The header the layer DRAWS is the table's: its letters are in the band the first chapter's header stands in, as many as the table's words have.
        {
            const glm::dvec2 view = layer.MenuBoxPx();
            const MagicPortals::Dashboard::Layout layout = MagicPortals::Dashboard::LayOut(layer.DashboardRules(), view);
            const glm::dvec2 centre(layout.columnAdvance + layer.DashboardRules().header.atUnits.x, MagicPortals::Dashboard::LineY(layer.DashboardRules(), view, 0, 0.0) + layer.DashboardRules().header.atUnits.y);
            const Hud::Rect band{centre - glm::dvec2(90.0, 14.0), glm::dvec2(180.0, 28.0)};
            const std::string header = layer.StringTable().Show(Strings::Key::kDashboardChapter, {{Strings::Placeholder::kN, "1"}});
            const BitmapFont* font = layer.UiFontOf(layer.DashboardRules().header.font);
            CHECK(font != nullptr);
            if (font != nullptr) {
                std::size_t inBand = 0;
                for (const ScreenOverlay::Quad* quad : glyphQuadsOf(frame)) {
                    if (band.Contains((glm::dvec2(quad->min) + glm::dvec2(quad->max)) * 0.5 * view)) ++inBand;
                }
                CHECK_MSG(inBand == Hud::LayOutText(*font, header, glm::dvec2(0.0), 1.0).size() && inBand > 0u,
                          name + ": the first chapter's header is the table's '" + header + "': " + std::to_string(inBand) + " letters");
                CHECK_MSG(name == "de" || inBand != 8u, name + ": and not English's 'Chapter 1'");
            }
        }
    }
}

// ---- the cards ----------------------------------------------------------------------------------------------------------------------

void theCardsSpeakTheLanguage() {
    if (!dataIsThere("theCardsSpeakTheLanguage")) return;
    for (const char* code : {"de", "ru", "ar"}) {
        const std::string name(code);
        MagicPortalsLayer::Paths paths = langPaths("unlock-" + name, name);
        paths.saveDir = scratchDir("langlayer-saves-unlock-" + name).string();
        Run run(paths, "level0");
        MagicPortalsLayer& layer = run.layer;
        CHECK_MSG(layer.SimLevel() != nullptr, layer.LoadError());
        if (layer.SimLevel() == nullptr) continue;
        walkToTheDoor(layer, run.registry);
        CHECK_MSG(layer.Finishing(), name + ": level0's door is reached");
        const MagicPortals::Notificator::Card* card = nullptr;
        for (int tick = 0; tick < 420 && card == nullptr; ++tick) {
            rest(layer, run.registry, 1);
            card = layer.Notifications().Front();
        }
        CHECK_MSG(card != nullptr, name + ": the unlock card comes up");
        if (card == nullptr) continue;
        const Achievements::Entry* entry = layer.AchievementsContent()->Find(card->id);
        CHECK_MSG(entry != nullptr, name + ": the card is for a listed achievement");
        if (entry == nullptr) continue;
        const std::string expected = layer.StringTable().Show(Strings::Key::kCardUnlocked, {{Strings::Placeholder::kTitle, entry->title}});
        CHECK_MSG(card->message == expected && card->message.find("Achievement unlocked") == std::string::npos,
                  name + ": the card's words are the table's card.unlocked with the translated title");
        if (std::filesystem::is_regular_file(translationFile(name))) {
            Achievements::Translation translation;
            std::string error;
            CHECK_MSG(Achievements::LoadTranslation(translationFile(name), name, translation, error) && entry->title == translation.words.at(entry->id).title,
                      name + ": and that title is the translation file's: " + entry->title);
        }
        rest(layer, run.registry, 60);
        checkFontsOfFrame(cardFrame(run.registry, layer), layer, name + " unlock card");
    }
    // The locked-level card of the dashboard, in Japanese.
    {
        Run run(langPaths("locked-ja", "ja"), "");
        MagicPortalsLayer& layer = run.layer;
        if (!layer.LoadError().empty() || !tickToTheMainMenu(layer, run.registry)) {
            CHECK_MSG(false, "ja: no main menu");
            return;
        }
        rest(layer, run.registry, 60);
        CHECK(layer.PressMainMenu(run.registry, MagicPortals::MainMenu::Button::Achievements));
        rest(layer, run.registry, 90);
        const MagicPortals::Dashboard::Board& board = layer.DashboardBoard();
        int locked = -1;
        for (std::size_t i = 0; i < board.lines.size() && locked < 0; ++i) {
            if (!board.lines[i].header && board.lines[i].level >= 0 && !board.lines[i].levelUnlocked) locked = static_cast<int>(i);
        }
        CHECK_MSG(locked >= 0, "a locked level's row");
        if (locked < 0) return;
        click(layer, run.registry, MagicPortals::Dashboard::LineHitRect(layer.DashboardRules(), layer.MenuBoxPx(), locked, 0.0).Centre());
        const MagicPortals::Notificator::Card* card = layer.Notifications().Front();
        CHECK_MSG(card != nullptr && card->message == layer.StringTable().Show(Strings::Key::kCardLockedLevel) && card->message != "This level is locked!",
                  "the locked card is the table's card.locked_level");
        rest(layer, run.registry, 45);
        checkFontsOfFrame(cardFrame(run.registry, layer), layer, "the locked card in Japanese");
    }
}

// ---- English comes back ------------------------------------------------------------------------------------------------------------

// A screen in English, the same screen in Russian, and in English again with no tick between: the frames of the first and the third are the
// same quads to the last float, and the second is not.
template <class Go>
void roundTrip(const std::string& what, const std::string& saves, Go go, bool differs) {
    MagicPortalsLayer::Paths paths = langPaths("round-" + what);
    paths.saveDir = saves;
    Run run(paths, "");
    MagicPortalsLayer& layer = run.layer;
    if (!layer.LoadError().empty() || !tickToTheMainMenu(layer, run.registry)) {
        CHECK_MSG(false, what + ": no main menu");
        return;
    }
    rest(layer, run.registry, 60);
    go(layer, run.registry);
    const std::vector<ScreenOverlay::Quad> before = menuFrame(run.registry, layer);
    CHECK_MSG(layer.SetLanguage(Lang::Ru) && layer.CurrentLanguage() == Lang::Ru, what + ": Russian");
    const std::vector<ScreenOverlay::Quad> russian = menuFrame(run.registry, layer);
    if (differs) CHECK_MSG(describe(russian) != describe(before), what + ": Russian draws something else: " + describe(russian));
    CHECK_MSG(layer.SetLanguage(Lang::En) && layer.CurrentLanguage() == Lang::En, what + ": English again");
    const std::vector<ScreenOverlay::Quad> after = menuFrame(run.registry, layer);
    CHECK_MSG(after.size() == before.size() && describe(after) == describe(before),
              what + ": the same quads as before, to the last float: " + describe(after) + " against " + describe(before));
    // Quad by quad, so that a hash collision cannot hide a difference.
    bool same = after.size() == before.size();
    for (std::size_t i = 0; same && i < after.size(); ++i) {
        same = after[i].min == before[i].min && after[i].max == before[i].max && after[i].uvMin == before[i].uvMin && after[i].uvMax == before[i].uvMax &&
               after[i].color == before[i].color && after[i].texture == before[i].texture;
    }
    CHECK_MSG(same, what + ": and every quad is equal field by field");
}

void settingTheLanguageTwiceReturnsToTheSameEnglish() {
    if (!dataIsThere("settingTheLanguageTwiceReturnsToTheSameEnglish")) return;
    const std::string saves = seededSaves("langlayer-round-saves");
    roundTrip("dashboard", "", [](MagicPortalsLayer& layer, entt::registry& registry) {
        CHECK(layer.PressMainMenu(registry, MagicPortals::MainMenu::Button::Achievements));
        rest(layer, registry, 90);
    }, true);
    roundTrip("worlds", saves, [](MagicPortalsLayer& layer, entt::registry& registry) {
        CHECK(layer.PressMainMenu(registry, MagicPortals::MainMenu::Button::Play));
        rest(layer, registry, 90);
    }, false);
    roundTrip("credits", "", [](MagicPortalsLayer& layer, entt::registry& registry) {
        CHECK(layer.PressMainMenu(registry, MagicPortals::MainMenu::Button::Credits));
        rest(layer, registry, 1560);
    }, true);
    // The level's caption and the open popup are rebuilt too, on the same layer: Russian and back.
    {
        Run run(langPaths("round-level"), "level0");
        MagicPortalsLayer& layer = run.layer;
        CHECK(layer.SimLevel() != nullptr);
        if (layer.SimLevel() == nullptr) return;
        rest(layer, run.registry, 30);
        const std::string english = layer.CaptionWords();
        const std::string before = describe(hudFrame(run.registry, layer));
        CHECK_MSG(layer.SetLanguage(Lang::Ru) && layer.CaptionWords() != english && layer.CaptionWords() == layer.StringTable().Show(Strings::Key::kHudPart, {{Strings::Placeholder::kN, "1"}}),
                  "the caption is composed again in Russian: " + layer.CaptionWords());
        const std::string russian = describe(hudFrame(run.registry, layer));
        CHECK_MSG(russian != before, "and drawn from the generated font");
        CHECK_MSG(layer.SetLanguage(Lang::En) && layer.CaptionWords() == english && describe(hudFrame(run.registry, layer)) == before, "English again: the same caption and the same quads");
    }
}

// ---- no hole in any language -----------------------------------------------------------------------------------------------------------

std::vector<std::string> holesOf(MagicPortalsLayer& layer, const std::string& text, const std::string& role) {
    std::vector<std::string> found;
    const BitmapFont* font = layer.UiFontOf(role);
    if (font == nullptr) return {"no font " + role};
    std::vector<std::uint32_t> holes;
    Hud::LayOutText(*font, text, glm::dvec2(0.0), 1.0, &holes);
    for (const std::uint32_t hole : holes) {
        char at[16];
        std::snprintf(at, sizeof(at), "U+%04X", static_cast<unsigned>(hole));
        found.push_back(at);
    }
    return found;
}

void noStringOfAnyLanguageIsDrawnWithAHole() {
    if (!originalArtIsThere()) {
        std::printf("  noStringOfAnyLanguageIsDrawnWithAHole SKIPPED - needs the original's extracted assets\n");
        return;
    }
    Run run(langPaths("coverage"), "");
    MagicPortalsLayer& layer = run.layer;
    const Achievements::Content* achievements = layer.AchievementsContent();
    std::size_t strings = 0;
    for (std::size_t i = 0; i < Strings::kLangCount; ++i) {
        const Lang lang = static_cast<Lang>(i);
        const std::string code = codeOf(lang);
        CHECK_MSG(layer.SetLanguage(lang) && layer.CurrentLanguage() == lang, code + ": the language is set");
        const Strings::Table& table = layer.StringTable();
        const std::string title = achievements != nullptr && !layer.AchievementsContent()->entries.empty() ? layer.AchievementsContent()->entries[0].title : "X";
        const auto show = [&](std::string_view key) {
            return table.Show(key, {{Strings::Placeholder::kN, "21"}, {Strings::Placeholder::kPercent, "60"}, {Strings::Placeholder::kTitle, title},
                                    {Strings::Placeholder::kWorld, "4"}, {Strings::Placeholder::kLevel, "32"}});
        };
        std::vector<std::string> holes;
        const auto check = [&](const std::string& text, const std::string& role, const std::string& what) {
            ++strings;
            for (const std::string& hole : holesOf(layer, text, role)) holes.push_back(what + " " + hole);
        };
        for (const std::string_view key : Strings::kMaturaKeys) check(show(key), "Matura84_shadow.fnt", std::string(key));
        for (const std::string_view key : Strings::kVerdanaKeys) check(show(key), "Verdana40_shadow.fnt", std::string(key));
        // The caption in the ASCII-only digits of the counters: the original's Matura128 holds them in every language.
        check("0123456789/%", "Matura128_shadow.fnt", "counters");
        if (layer.AchievementsContent() != nullptr) {
            const Achievements::Content& shown = *layer.AchievementsContent();
            for (const Achievements::Entry& entry : shown.entries) {
                check(table.Shape(Achievements::TitleOf(shown, entry, true)), "Verdana40_shadow.fnt", "title " + std::to_string(entry.id));
                check(table.Shape(Achievements::DescriptionOf(shown, entry, true)), "Verdana40_shadow.fnt", "description " + std::to_string(entry.id));
                if (entry.isSecret) {
                    check(table.Shape(Achievements::TitleOf(shown, entry, false)), "Verdana40_shadow.fnt", "secret title");
                    check(table.Shape(Achievements::DescriptionOf(shown, entry, false)), "Verdana40_shadow.fnt", "secret description " + std::to_string(entry.id));
                }
            }
        }
        std::string first;
        for (std::size_t h = 0; h < holes.size() && h < 3; ++h) first += (first.empty() ? "" : "; ") + holes[h];
        CHECK_MSG(holes.empty(), code + ": " + std::to_string(holes.size()) + " code point(s) with no glyph: " + first);
    }
    CHECK_MSG(strings > 11u * 17u, "a good many strings were laid out: " + std::to_string(strings));
    // The check can fail: Russian words laid out in the original's English font have holes, and so do Japanese ones.
    CHECK(layer.SetLanguage(Lang::Ru));
    const std::string russian = layer.StringTable().Show(Strings::Key::kDashboardNew);
    CHECK(layer.SetLanguage(Lang::En));
    CHECK_MSG(!holesOf(layer, russian, "Verdana40_shadow.fnt").empty(), "mutation: Russian text in the original's Verdana has holes, and the check sees them");
    CHECK(layer.SetLanguage(Lang::Ja));
    const std::string kana = layer.StringTable().Show(Strings::Key::kDashboardNew);
    CHECK(layer.SetLanguage(Lang::En));
    CHECK_MSG(!holesOf(layer, kana, "Matura84_shadow.fnt").empty(), "mutation: Japanese text in the original's Matura has holes, and the check sees them");
}

// ---- a bake that fails ------------------------------------------------------------------------------------------------------------

void aFailingBakeFallsBackToEnglishLoudly() {
    if (!dataIsThere("aFailingBakeFallsBackToEnglishLoudly")) return;
    // English's own main menu, as the reference: the same ticks on a layer that speaks it.
    std::string englishMain;
    {
        Run run(langPaths("bake-ref"), "");
        CHECK(tickToTheMainMenu(run.layer, run.registry));
        rest(run.layer, run.registry, 30);
        englishMain = describe(menuFrame(run.registry, run.layer));
    }
    const auto expectEnglish = [&](Run& run, const std::string& what) {
        MagicPortalsLayer& layer = run.layer;
        CHECK_MSG(layer.CurrentLanguage() == Lang::En && layer.RequestedLanguage() == Lang::Ru, what + ": English on the screen, Russian asked for");
        CHECK_MSG(!layer.GeneratedFonts(), what + ": the original's fonts");
        bool said = false;
        for (const std::string& note : layer.LanguageNotes()) said = said || (note.find("English is shown instead") != std::string::npos && note.find("fonts for ru") != std::string::npos);
        CHECK_MSG(said, what + ": and the notes say why, once");
        CHECK_MSG(layer.UiFontFile("Matura84_shadow.fnt") == kOriginal + "/data/Matura84_shadow.fnt", what + ": Matura is the original's file");
        CHECK(tickToTheMainMenu(layer, run.registry));
        rest(layer, run.registry, 30);
        CHECK_MSG(describe(menuFrame(run.registry, layer)) == englishMain, what + ": and the menu is drawn exactly as English draws it");
        CHECK_MSG(layer.CaptionWords().empty() && layer.StringTable().Language() == Lang::En, what + ": with English's strings");
    };
    {
        // No scratch folder to write the fonts in.
        MagicPortalsLayer::Paths paths = langPaths("bake-noprisms", "ru");
        paths.prisms.clear();
        Run run(paths, "");
        expectEnglish(run, "no scratch folder");
    }
    {
        // A scratch folder that cannot be made: its parent is a file.
        MagicPortalsLayer::Paths paths = langPaths("bake-blocked", "ru");
        std::error_code ec;
        std::filesystem::create_directories(paths.prisms, ec);
        { std::ofstream(paths.prisms / "blocker") << "x"; }
        paths.prisms = paths.prisms / "blocker" / "inside";
        Run run(paths, "");
        expectEnglish(run, "a scratch folder under a file");
    }
    {
        // A face that is not there: a copy of the port's data without Kurale.
        MagicPortalsLayer::Paths paths = langPaths("bake-noface", "ru");
        const std::filesystem::path copy = scratchDir("langlayer-portdata");
        std::error_code ec;
        std::filesystem::copy(paths.portData, copy, std::filesystem::copy_options::recursive, ec);
        CHECK_MSG(!ec, "the data is copied: " + ec.message());
        std::filesystem::remove(copy / "fonts" / "Kurale-Regular.ttf", ec);
        paths.portData = copy.string();
        Run run(paths, "");
        expectEnglish(run, "a face missing");
    }
    // The choice is still the player's: a layer that cannot bake keeps what was chosen.
    {
        MagicPortalsLayer::Paths paths = langPaths("bake-choice");
        paths.prisms.clear();
        paths.saveDir = scratchDir("langlayer-bake-choice-saves").string();
        {
            Run run(paths, "");
            CHECK_MSG(!run.layer.ChooseLanguage(Lang::Ru) && run.layer.CurrentLanguage() == Lang::En && run.layer.LanguageStore().Chosen() == Lang::Ru,
                      "ChooseLanguage reports that Russian is not on the screen, and keeps it as the choice");
        }
        Run again(paths, "");
        CHECK_MSG(again.layer.RequestedLanguage() == Lang::Ru && again.layer.CurrentLanguage() == Lang::En, "the next start asks for Russian again");
        MagicPortalsLayer::Paths better = paths;
        better.prisms = testPaths("langlayer-bake-choice-ok").prisms;
        Run capable(better, "");
        CHECK_MSG(capable.layer.CurrentLanguage() == Lang::Ru && capable.layer.GeneratedFonts(), "and a start that can make the fonts speaks it");
    }
}

// ---- the hint, mirrored ------------------------------------------------------------------------------------------------------------

void theHintIsMirroredInArabic() {
    if (!dataIsThere("theHintIsMirroredInArabic")) return;
    struct Extent {
        double left = 1.0e9;
        double right = -1.0e9;
        std::size_t glyphs = 0;
        double view = 0.0;
    };
    const auto extentOf = [](const std::optional<std::string>& code, const std::string& name) {
        Run run(langPaths("hint-" + name, code), "level20");
        MagicPortalsLayer& layer = run.layer;
        Extent extent;
        CHECK_MSG(layer.SimLevel() != nullptr, layer.LoadError());
        if (layer.SimLevel() == nullptr) return extent;
        // Past the caption's three seconds (188 ticks), so that the hint is the only text on the view.
        rest(layer, run.registry, 200);
        const glm::dvec2 view = layer.ViewPx();
        extent.view = view.x;
        for (const ScreenOverlay::Quad* quad : glyphQuadsOf(hudFrame(run.registry, layer))) {
            extent.left = std::min(extent.left, static_cast<double>(quad->min.x) * view.x);
            extent.right = std::max(extent.right, static_cast<double>(quad->max.x) * view.x);
            ++extent.glyphs;
        }
        return extent;
    };
    const Extent english = extentOf(std::nullopt, "en");
    const Extent arabic = extentOf(std::string("ar"), "ar");
    const Extent russian = extentOf(std::string("ru"), "ru");
    CHECK_MSG(english.glyphs > 10u && arabic.glyphs > 5u && russian.glyphs > 5u, "each draws the hint: " + std::to_string(english.glyphs) + ", " + std::to_string(arabic.glyphs) + ", " + std::to_string(russian.glyphs));
    CHECK_MSG(english.left >= 46.0 && english.left <= 56.0, "English starts about 50 units in from the left: " + std::to_string(english.left));
    CHECK_MSG(russian.left >= 46.0 && russian.left <= 60.0, "Russian too (left to right): " + std::to_string(russian.left));
    CHECK_MSG(arabic.right <= arabic.view - 50.0 + 6.0 && arabic.right >= arabic.view - 50.0 - 12.0,
              "Arabic ends about 50 units in from the RIGHT edge (" + std::to_string(arabic.view) + "): " + std::to_string(arabic.right));
    CHECK_MSG(arabic.left > 50.0 || arabic.right > arabic.view * 0.6, "and so stands on the right of the view: " + std::to_string(arabic.left) + " to " + std::to_string(arabic.right));
    CHECK_MSG(english.right <= english.view && russian.right <= russian.view && arabic.right <= arabic.view && arabic.left >= 0.0, "none runs off the view");
}

// ---- the policy and the saved choice -----------------------------------------------------------------------------------------------

std::string fileText(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

void theFlagAndTheSavedChoiceAndEnglish() {
    if (!originalArtIsThere()) {
        std::printf("  theFlagAndTheSavedChoiceAndEnglish SKIPPED - needs the original's extracted assets\n");
        return;
    }
    const std::string saved = "{\"version\":1,\"language\":\"de\"}";
    // A flagless run speaks the saved choice.
    {
        const std::filesystem::path dir = scratchDir("langlayer-saves-policy-1");
        { std::ofstream(dir / Language::kStoreFileName) << saved; }
        MagicPortalsLayer::Paths paths = langPaths("policy-1");
        paths.saveDir = dir.string();
        Run run(paths, "");
        CHECK_MSG(run.layer.CurrentLanguage() == Lang::De && run.layer.LanguageStore().FromFile() && run.layer.LanguageStore().Persists(), "a flagless run speaks the saved choice");
    }
    // A developer's or parity run (--fixed-step, --route, --visit-levels) with no --lang draws English whatever the player saved, reads nothing and writes
    // nothing: the policy main resolves from the command line (Language::ResolvePolicy), handed to a layer whose save folder holds a German choice.
    {
        const struct {
            const char* name;
            bool fixedStep;
            bool route;
            bool visit;
        } rigs[] = {{"fixed-step", true, false, false}, {"route", false, true, false}, {"visit-levels", false, false, true}};
        for (const auto& rig : rigs) {
            const std::filesystem::path dir = scratchDir(std::string("langlayer-saves-policy-rig-") + rig.name);
            { std::ofstream(dir / Language::kStoreFileName) << saved; }
            MagicPortalsLayer::Paths paths = langPaths(std::string("policy-rig-") + rig.name);
            paths.saveDir = dir.string();
            paths.language = Language::ResolvePolicy(std::nullopt, rig.fixedStep, rig.route, rig.visit);
            Run run(paths, "");
            MagicPortalsLayer& layer = run.layer;
            CHECK_MSG(layer.CurrentLanguage() == Lang::En && !layer.LanguageStore().FromFile() && !layer.LanguageStore().Persists(),
                      std::string(rig.name) + ": a saved German choice is neither read nor drawn");
            CHECK_MSG(layer.StringTable().Get(Strings::Key::kDashboardNew) == "new!" && !layer.LanguageAvailable(), std::string(rig.name) + ": the words are English and there is no picker");
            layer.ChooseLanguage(Lang::Fr); // the run may change its language in memory; it must not reach the file
            CHECK_MSG(fileText(dir / Language::kStoreFileName) == saved && layer.LanguageStore().SaveCount() == 0, std::string(rig.name) + ": the saved choice is as it was");
        }
        // The same save folder under a player's policy is German: the file was there to be read.
        const std::filesystem::path dir = scratchDir("langlayer-saves-policy-rig-player");
        { std::ofstream(dir / Language::kStoreFileName) << saved; }
        MagicPortalsLayer::Paths paths = langPaths("policy-rig-player");
        paths.saveDir = dir.string();
        paths.language = Language::ResolvePolicy(std::nullopt, false, false, false);
        Run run(paths, "");
        CHECK_MSG(run.layer.CurrentLanguage() == Lang::De && run.layer.LanguageStore().FromFile(), "and under a player's policy that very folder is read: German");
        // --lang with a rig flag: the flag.
        MagicPortalsLayer::Paths flagged = langPaths("policy-rig-flag");
        flagged.saveDir = dir.string();
        flagged.language = Language::ResolvePolicy(std::string("fr"), true, false, false);
        Run frenchRun(flagged, "");
        CHECK_MSG(frenchRun.layer.CurrentLanguage() == Lang::Fr, "--lang wins in a fixed-step run");
    }
    // A forced run speaks the flag, reads nothing, and writes nothing, whatever it chooses.
    {
        const std::filesystem::path dir = scratchDir("langlayer-saves-policy-2");
        { std::ofstream(dir / Language::kStoreFileName) << saved; }
        MagicPortalsLayer::Paths paths = langPaths("policy-2", std::string("ru"));
        paths.saveDir = dir.string();
        Run run(paths, "");
        MagicPortalsLayer& layer = run.layer;
        CHECK_MSG(layer.CurrentLanguage() == Lang::Ru && !layer.LanguageStore().FromFile() && !layer.LanguageStore().Persists(), "a forced run speaks the flag and does not read language.json");
        CHECK_MSG(layer.ChooseLanguage(Lang::Fr) && layer.CurrentLanguage() == Lang::Fr, "and can be changed for the run");
        CHECK_MSG(fileText(dir / Language::kStoreFileName) == saved && layer.LanguageStore().SaveCount() == 0, "but never writes: the saved choice is as it was");
    }
    // A flagless run writes the choice, and the next run speaks it.
    {
        const std::filesystem::path dir = scratchDir("langlayer-saves-policy-3");
        MagicPortalsLayer::Paths paths = langPaths("policy-3");
        paths.saveDir = dir.string();
        {
            Run run(paths, "");
            CHECK_MSG(run.layer.CurrentLanguage() == Lang::En && !run.layer.LanguageStore().Chosen(), "nothing chosen is English");
            CHECK_MSG(!std::filesystem::exists(dir / Language::kStoreFileName), "and starting writes nothing");
            CHECK(run.layer.ChooseLanguage(Lang::Es));
            CHECK_MSG(fileText(dir / Language::kStoreFileName) == "{\"version\":1,\"language\":\"es\"}" && run.layer.LanguageStore().SaveCount() == 1, "the choice is written, once");
        }
        Run again(paths, "");
        CHECK_MSG(again.layer.CurrentLanguage() == Lang::Es, "and the next run speaks it");
    }
    // Memory only: no save directory.
    {
        Run run(langPaths("policy-4"), "");
        CHECK(run.layer.CurrentLanguage() == Lang::En && !run.layer.LanguageStore().Persists());
        CHECK_MSG(run.layer.ChooseLanguage(Lang::De) && run.layer.CurrentLanguage() == Lang::De && run.layer.LanguageStore().SaveCount() == 0, "chosen in memory, nothing written");
    }
    // A flag no table has, and a file that will not read: English, said.
    {
        Run run(langPaths("policy-5", std::string("xx")), "");
        CHECK_MSG(run.layer.CurrentLanguage() == Lang::En, "a code the table does not have is English, not a guess");
    }
    {
        const std::filesystem::path dir = scratchDir("langlayer-saves-policy-6");
        { std::ofstream(dir / Language::kStoreFileName) << "not json"; }
        MagicPortalsLayer::Paths paths = langPaths("policy-6");
        paths.saveDir = dir.string();
        Run run(paths, "");
        CHECK_MSG(run.layer.CurrentLanguage() == Lang::En && !run.layer.LanguageStore().Reason().empty() && fileText(dir / Language::kStoreFileName) == "not json",
                  "a corrupt file is English with the reason kept, and the file is left as it is");
    }
}

// ---- the achievements' translation, whole and in part -------------------------------------------------------------------------------

std::string jsonString(const std::string& text) {
    std::string out = "\"";
    for (const char c : text) {
        if (c == '"' || c == '\\') out += '\\';
        if (c == '\n') {
            out += "\\n";
            continue;
        }
        out += c;
    }
    return out + "\"";
}

void aShortTranslationShowsEnglishForWhatItLacks() {
    if (!dataIsThere("aShortTranslationShowsEnglishForWhatItLacks")) return;
    if (!std::filesystem::is_regular_file(translationFile("de"))) {
        std::printf("  aShortTranslationShowsEnglishForWhatItLacks SKIPPED - needs out/data/achievements.de.json\n");
        return;
    }
    const std::filesystem::path dir = scratchDir("langlayer-overlay");
    std::error_code ec;
    std::filesystem::copy_file(kAchievements, dir / "achievements.json", ec);
    Achievements::Translation german;
    std::string error;
    CHECK_MSG(Achievements::LoadTranslation(translationFile("de"), "de", german, error), error);
    std::string text = "{\"format\": 1, \"language\": \"de\", \"secret\": {\"title\": " + jsonString(german.secret.title) + ", \"description\": " +
                       jsonString(german.secret.description) + "}, \"achievements\": {";
    bool first = true;
    for (const auto& [id, words] : german.words) {
        if (id == 45) continue;
        text += std::string(first ? "" : ", ") + "\"" + std::to_string(id) + "\": {\"title\": " + jsonString(words.title) + ", \"description\": " + jsonString(words.description) + "}";
        first = false;
    }
    text += "}}";
    { std::ofstream(dir / "achievements.de.json", std::ios::binary) << text; }
    MagicPortalsLayer::Paths paths = langPaths("overlay-de", std::string("de"));
    paths.achievements = (dir / "achievements.json").string();
    {
        Run run(paths, "");
        const MagicPortalsLayer& layer = run.layer;
        const Achievements::Entry* shown = layer.AchievementsContent()->Find(45);
        const Achievements::Entry* english = layer.AchievementsEnglish()->Find(45);
        const Achievements::Entry* other = layer.AchievementsContent()->Find(0);
        CHECK_MSG(shown != nullptr && english != nullptr && shown->title == english->title && shown->description == english->description, "an id the file lacks shows its English");
        CHECK_MSG(other != nullptr && other->title == german.words.at(0).title, "the others are German");
        bool said = false;
        for (const std::string& note : layer.LanguageNotes()) said = said || (note.find("no de text") != std::string::npos && note.find("45") != std::string::npos);
        CHECK_MSG(said, "and the notes say which one, once");
    }
    // No file at all for the language: every row is English, the interface German, and it is said.
    {
        MagicPortalsLayer::Paths none = langPaths("overlay-fr", std::string("fr"));
        none.achievements = (dir / "achievements.json").string();
        Run run(none, "");
        const MagicPortalsLayer& layer = run.layer;
        bool allEnglish = layer.CurrentLanguage() == Lang::Fr;
        for (const Achievements::Entry& entry : layer.AchievementsEnglish()->entries) {
            const Achievements::Entry* shown = layer.AchievementsContent()->Find(entry.id);
            allEnglish = allEnglish && shown != nullptr && shown->title == entry.title && shown->description == entry.description;
        }
        CHECK_MSG(allEnglish, "without a file the achievements are English while the interface is French");
        bool said = false;
        for (const std::string& note : layer.LanguageNotes()) said = said || (note.find("shown in English") != std::string::npos && note.find("achievements.fr.json") != std::string::npos);
        CHECK_MSG(said, "and the note names the file it looked for");
        CHECK_MSG(layer.AchievementsContent()->secret.title == layer.AchievementsEnglish()->secret.title && layer.AchievementsContent()->secret.descriptionTemplate.empty(),
                  "the secret's words are the English ones too, as the original composes them");
    }
}

void aBareLayerIsEnglishAndWritesNothing() {
    if (!dataIsThere("aBareLayerIsEnglishAndWritesNothing")) return;
    Run run(testPaths("langlayer-bare"), "");
    const MagicPortalsLayer& layer = run.layer;
    CHECK_MSG(layer.CurrentLanguage() == Lang::En && layer.RequestedLanguage() == Lang::En && layer.LanguageReady(), "English");
    CHECK_MSG(!layer.LanguageStore().Persists() && layer.LanguageStore().SaveCount() == 0, "with nowhere to write");
    CHECK_MSG(!layer.GeneratedFonts() && layer.LanguageNotes().empty(), "the original's fonts and nothing to say");
    CHECK_MSG(layer.AchievementsContent() != nullptr && layer.AchievementsContent()->entries.size() == layer.AchievementsEnglish()->entries.size() &&
                  layer.AchievementsContent()->entries[0].title == layer.AchievementsEnglish()->entries[0].title,
              "and the shown list is the English one");
    CHECK_MSG(std::string(Strings::kLangCodes[0]) == "en", "en is the default");
}

// ---- the fit reaches the screen ---------------------------------------------------------------------------------------------------------

// The horizontal extent, in design units, of the glyph quads of a frame that lie inside `box` (units of the menu's box).
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

// The options screen, in a language whose words are longer than their slots: the word is drawn no wider than its limit (+ the glyphs' soft
// shadow, which is padding in the cell, not advance). Without the fit the German tile word is 12% wider than the 80 units it has.
void theOptionsWordsAreDrawnInsideTheirSlots() {
    if (!dataIsThere("theOptionsWordsAreDrawnInsideTheirSlots")) return;
    namespace OptionsUi = MagicPortals::Options;
    // What a drawn word's ink extends past its advances: the cells' padding (the soft shadow) and a script face's swashes. The German word measures 4.6 over
    // its limit at the fitted size; unfitted it is 14.4 over, so 8 tells the two apart.
    constexpr double kShadowUnits = 8.0;
    for (const char* code : {"de", "ru"}) {
        MagicPortalsLayer::Paths paths = langPaths(std::string("options-") + code, std::string(code));
        paths.controls = Controls::ResolvePolicy(std::string("modern"), false, false, false);
        paths.controls.withoutOriginalArt = true;
        Run run(paths, "");
        MagicPortalsLayer& layer = run.layer;
        if (!layer.LoadError().empty() || !tickToTheMainMenu(layer, run.registry) || !layer.OptionsAvailable()) {
            CHECK_MSG(false, std::string(code) + ": no options entry: " + layer.LoadError());
            continue;
        }
        rest(layer, run.registry, 60);
        const Hud::Rect entry = OptionsUi::EntryAt(layer.OptionsRules(), layer.MainMenuRules().layer, layer.MainMenuRules().state, layer.ViewPx(), 1.0e9, false).rect;
        click(layer, run.registry, entry.Centre());
        rest(layer, run.registry, 60);
        CHECK_MSG(layer.MenuScreen() == MagicPortalsLayer::Screen::Options, std::string(code) + ": Options opens");
        const std::vector<ScreenOverlay::Quad> frame = menuFrame(run.registry, layer);
        const glm::dvec2 view = layer.MenuBoxPx();
        const OptionsUi::Rules& rules = layer.OptionsRules();
        const BitmapFont* font = layer.UiFontOf(rules.wordFont);
        CHECK_MSG(font != nullptr, std::string(code) + ": the word font loads");
        if (font == nullptr) continue;
        bool fittedOne = false;
        for (const Controls::Style style : OptionsUi::kTileStyles) {
            const Hud::Rect tile = OptionsUi::TileRect(rules, style, view);
            // The word's own area: right of the thumbnail.
            const Hud::Rect area{tile.min + glm::dvec2(rules.thumbnail.atUnits.x + rules.thumbnail.sizeUnits.x, 0.0),
                                 glm::dvec2(tile.size.x - rules.thumbnail.atUnits.x - rules.thumbnail.sizeUnits.x, tile.size.y)};
            const Span span = spanInside(frame, view, area);
            const std::string words = layer.StringTable().Show(style == Controls::Style::Modern ? Strings::Key::kOptionsModern : Strings::Key::kOptionsClassic);
            const double natural = static_cast<double>(Hud::MeasureText(*font, words).x) * rules.wordUnitsPerFontPx;
            CHECK_MSG(span.glyphs > 3u, std::string(code) + ": the tile's word is drawn");
            CHECK_MSG(span.glyphs == Hud::LayOutText(*font, words, glm::dvec2(0.0), 1.0).size(),
                      std::string(code) + ": and it is the table's word '" + words + "' that is drawn: " + std::to_string(span.glyphs) + " letters");
            CHECK_MSG(span.Width() <= rules.wordMaxWidthUnits + kShadowUnits,
                      std::string(code) + ": the " + Controls::NameOf(style) + " word is no wider than its limit " + std::to_string(rules.wordMaxWidthUnits) + ": " + std::to_string(span.Width()));
            if (natural > rules.wordMaxWidthUnits) {
                fittedOne = true;
                CHECK_MSG(span.Width() > rules.wordMaxWidthUnits * 0.8, std::string(code) + ": and fills it, being scaled and not cut: " + std::to_string(span.Width()));
            }
        }
        if (std::string(code) == "de") CHECK_MSG(fittedOne, "German's Classic is wider than its slot, so the fit has something to do here");
    }
}

// A dashboard row whose translated title is wider than the slot: drawn exactly as wide as the slot.
void theDashboardsTitlesAreFittedOnTheScreen() {
    if (!dataIsThere("theDashboardsTitlesAreFittedOnTheScreen")) return;
    if (!std::filesystem::is_regular_file(translationFile("fr"))) {
        std::printf("  theDashboardsTitlesAreFittedOnTheScreen SKIPPED - needs out/data/achievements.fr.json\n");
        return;
    }
    Run run(langPaths("fit-fr", std::string("fr")), "");
    MagicPortalsLayer& layer = run.layer;
    if (!layer.LoadError().empty() || !tickToTheMainMenu(layer, run.registry)) {
        CHECK_MSG(false, "fr: no main menu");
        return;
    }
    rest(layer, run.registry, 60);
    CHECK(layer.PressMainMenu(run.registry, MagicPortals::MainMenu::Button::Achievements));
    rest(layer, run.registry, 90);
    namespace Dashboard = MagicPortals::Dashboard;
    const Dashboard::Rules& rules = layer.DashboardRules();
    const BitmapFont* font = layer.UiFontOf(rules.title.font);
    CHECK_MSG(font != nullptr, "the title font loads");
    if (font == nullptr) return;
    const glm::dvec2 view = layer.MenuBoxPx();
    // The row whose title needs the smallest scale.
    int worst = -1;
    double worstScale = 1.0;
    const Dashboard::Board& board = layer.DashboardBoard();
    for (std::size_t i = 0; i < board.lines.size(); ++i) {
        // A row with no 'new!' label, whose column the title would otherwise share with the band below.
        if (board.lines[i].header || board.lines[i].isNew) continue;
        const double scale = Hud::FitUnitsPerFontPx(*font, board.lines[i].title, rules.title.unitsPerFontPx, rules.title.maxWidthUnits) / rules.title.unitsPerFontPx;
        if (scale < worstScale) {
            worstScale = scale;
            worst = static_cast<int>(i);
        }
    }
    CHECK_MSG(worst >= 0 && worstScale < 0.95, "a French title is wider than the slot: scale " + std::to_string(worstScale));
    if (worst < 0) return;
    // Wheel the list until that row is on the view.
    const double decay = rules.scroll.momentumDecayPerTick;
    for (int step = 0; step < 40 && !Dashboard::LineShown(rules, view, worst, layer.DashboardState().scroll); ++step) {
        const float notches = static_cast<float>(-(view.y * (1.0 - decay)) / rules.scroll.wheelUnitsPerNotch);
        tickWith(layer, run.registry, kRest, {}, {}, notches);
        rest(layer, run.registry, 240);
    }
    CHECK_MSG(Dashboard::LineShown(rules, view, worst, layer.DashboardState().scroll), "the row is on the view");
    const Dashboard::Layout layout = Dashboard::LayOut(rules, view);
    const double top = Dashboard::LineY(rules, view, worst, layer.DashboardState().scroll);
    const double barLeft = layout.columnAdvance + layout.iconSize.x;
    // The title's band: from its top (8 units into the row) one line of the font, as wide as the bar and the unfitted title could be.
    const Hud::Rect band{glm::dvec2(barLeft, top + rules.title.atUnits.y - 1.0), glm::dvec2(layout.barSize.x + 60.0, 18.0)};
    const Span span = spanInside(menuFrame(run.registry, layer), view, band);
    CHECK_MSG(span.glyphs > 5u, "the title is drawn: " + std::to_string(span.glyphs));
    const double natural = static_cast<double>(Hud::MeasureText(*font, board.lines[static_cast<std::size_t>(worst)].title).x) * rules.title.unitsPerFontPx;
    CHECK_MSG(span.Width() <= rules.title.maxWidthUnits + 4.0 && span.Width() < natural - 2.0,
              "drawn " + std::to_string(span.Width()) + " wide where it is " + std::to_string(natural) + " unfitted and the slot is " + std::to_string(rules.title.maxWidthUnits));
}

// ---- the fit reaches the hint, the unlock card and the popup ------------------------------------------------------------------------------

// No shipped translation is wider than the hint's, the card's or the popup's slot (test_mp_overflow measures them all), so the fit at those three draw
// sites has nothing to do in a real run today. Their limits are DATA, so a copy of the port's data with tight ones makes ENGLISH too wide for them, and
// the layer must then draw the text no wider (or taller) than the limit: the proof that each site hands the fitted size to the layout.
struct Patch {
    const char* file;
    const char* from;
    const char* to;
};

std::string tightData(const std::string& name, const std::vector<Patch>& patches) {
    const std::filesystem::path dir = scratchDir("langlayer-data-" + name);
    std::error_code ec;
    std::filesystem::copy(testPaths("tight").portData, dir, std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing, ec);
    CHECK_MSG(!ec, "the port's data is copied: " + ec.message());
    for (const Patch& patch : patches) {
        const std::filesystem::path file = dir / patch.file;
        std::string text = fileText(file);
        const std::size_t at = text.find(patch.from);
        CHECK_MSG(at != std::string::npos, std::string(patch.file) + " holds " + patch.from);
        if (at == std::string::npos) continue;
        text.replace(at, std::string(patch.from).size(), patch.to);
        std::ofstream out(file, std::ios::binary | std::ios::trunc);
        out << text;
    }
    return dir.string();
}

struct Extent2 {
    double left = 1.0e9;
    double right = -1.0e9;
    double top = 1.0e9;
    double bottom = -1.0e9;
    std::size_t glyphs = 0;
    double Width() const { return glyphs > 0 ? right - left : 0.0; }
    double Height() const { return glyphs > 0 ? bottom - top : 0.0; }
};

Extent2 extentOfGlyphs(const std::vector<ScreenOverlay::Quad>& frame, const glm::dvec2& view, const char* fontName) {
    Extent2 extent;
    for (const ScreenOverlay::Quad* quad : glyphQuadsOf(frame)) {
        if (quad->texture.find(fontName) == std::string::npos) continue;
        extent.left = std::min(extent.left, static_cast<double>(quad->min.x) * view.x);
        extent.right = std::max(extent.right, static_cast<double>(quad->max.x) * view.x);
        extent.top = std::min(extent.top, static_cast<double>(quad->min.y) * view.y);
        extent.bottom = std::max(extent.bottom, static_cast<double>(quad->max.y) * view.y);
        ++extent.glyphs;
    }
    return extent;
}

void aTightLimitScalesTheHintTheCardAndThePopup() {
    if (!dataIsThere("aTightLimitScalesTheHintTheCardAndThePopup")) return;
    constexpr double kInk = 8.0; // what a drawn word's ink extends past its advances (the soft shadow's cell padding)
    const std::string tight = tightData("tight", {{"camera.json", "\"max_width_units\": 256", "\"max_width_units\": 120"},
                                                  {"achievements.json", "\"max_width_units\": 269.5", "\"max_width_units\": 120"},
                                                  {"achievements.json", "\"max_width_units\": 276", "\"max_width_units\": 140"},
                                                  {"achievements.json", "\"max_height_units\": 150", "\"max_height_units\": 40"}});
    const auto pathsFor = [&](const std::string& name, bool tightened) {
        MagicPortalsLayer::Paths paths = langPaths("tight-" + name);
        if (tightened) paths.portData = tight;
        return paths;
    };
    // 1-21's hint: 255.6 wide in English, 120 allowed.
    for (const bool tightened : {false, true}) {
        Run run(pathsFor("hint", tightened), "level20");
        MagicPortalsLayer& layer = run.layer;
        CHECK_MSG(layer.SimLevel() != nullptr, layer.LoadError());
        if (layer.SimLevel() == nullptr) continue;
        rest(layer, run.registry, 200); // past the caption: the hint is the only Matura text
        const Extent2 extent = extentOfGlyphs(hudFrame(run.registry, layer), layer.ViewPx(), "Matura");
        CHECK_MSG(extent.glyphs > 10u, "the hint is drawn");
        if (tightened) {
            CHECK_MSG(extent.Width() <= 120.0 + kInk && extent.Width() > 90.0, "tight: the hint is scaled to its 120: " + std::to_string(extent.Width()));
        } else {
            CHECK_MSG(extent.Width() > 240.0, "shipped: the hint is its natural 256: " + std::to_string(extent.Width()));
        }
    }
    // The unlock card: its text 120 wide at most.
    for (const bool tightened : {false, true}) {
        MagicPortalsLayer::Paths paths = pathsFor("card", tightened);
        paths.saveDir = scratchDir(std::string("langlayer-saves-tight-card-") + (tightened ? "t" : "s")).string();
        Run run(paths, "level0");
        MagicPortalsLayer& layer = run.layer;
        CHECK_MSG(layer.SimLevel() != nullptr, layer.LoadError());
        if (layer.SimLevel() == nullptr) continue;
        walkToTheDoor(layer, run.registry);
        for (int tick = 0; tick < 420 && layer.Notifications().Front() == nullptr; ++tick) rest(layer, run.registry, 1);
        CHECK_MSG(layer.Notifications().Front() != nullptr, "the card comes up");
        if (layer.Notifications().Front() == nullptr) continue;
        rest(layer, run.registry, 60);
        const Extent2 extent = extentOfGlyphs(cardFrame(run.registry, layer), layer.ViewPx(), "Verdana");
        CHECK_MSG(extent.glyphs > 10u, "the card's words are drawn");
        if (tightened) {
            CHECK_MSG(extent.Width() <= 120.0 + kInk, "tight: the card's text is scaled to its 120: " + std::to_string(extent.Width()));
        } else {
            CHECK_MSG(extent.Width() > 120.0 + kInk, "shipped: the card's text is wider than the tight limit: " + std::to_string(extent.Width()));
        }
    }
    // The pause's popup of 1-31: its text 140 wide and 40 tall at most.
    {
        for (const bool tightened : {false, true}) {
            Run run(pathsFor("popup", tightened), "level30");
            MagicPortalsLayer& layer = run.layer;
            CHECK_MSG(layer.SimLevel() != nullptr, layer.LoadError());
            if (layer.SimLevel() == nullptr) continue;
            rest(layer, run.registry, 120);
            Hud::Rect control;
            CHECK(layer.ControlRect(MagicPortalsLayer::Control::Menu, control));
            tickWith(layer, run.registry, screenOfView(layer, control.Centre()), {MagicPortalsLayer::kTap}, {MagicPortalsLayer::kTap});
            rest(layer, run.registry, 60);
            CHECK(layer.PressPause(run.registry, MagicPortals::Pause::Button::Achievements) && layer.AchievementsPopupOpen());
            rest(layer, run.registry, 90);
            const Extent2 extent = extentOfGlyphs(hudFrame(run.registry, layer), layer.ViewPx(), "Verdana");
            CHECK_MSG(extent.glyphs > 20u, "the popup's text is drawn");
            if (tightened) {
                CHECK_MSG(extent.Width() <= 140.0 + kInk && extent.Height() <= 40.0 + kInk, "tight: the popup's text is scaled into 140 x 40: " + std::to_string(extent.Width()) + " x " + std::to_string(extent.Height()));
            } else {
                CHECK_MSG(extent.Width() > 140.0 + kInk || extent.Height() > 40.0 + kInk, "shipped: it is larger than the tight limits: " + std::to_string(extent.Width()) + " x " + std::to_string(extent.Height()));
            }
        }
    }
}

void runTests() {
    theNumericGoldenComparesWithATolerance();
    theCommittedGoldenHidesAsanteesWords();
    theGlyphFingerprintTellsTheSequence();
    theLocalTextGoldenStaysUnderOut();
    theReaderRefusesWhatItDoesNotKnow();
    theGlyphPagesAreTheOriginalsPages();
    theGuardSeesAsanteesWordsWhereverTheyAreDrawn();
    theEnglishFramesAreWhatTheyWere();
    aBareLayerIsEnglishAndWritesNothing();
    everyLanguageStartsFromTheFlag();
    theCaptionAndThePauseSpeakEachLanguage();
    theDashboardShowsTheTranslatedRows();
    theCardsSpeakTheLanguage();
    settingTheLanguageTwiceReturnsToTheSameEnglish();
    noStringOfAnyLanguageIsDrawnWithAHole();
    aFailingBakeFallsBackToEnglishLoudly();
    theHintIsMirroredInArabic();
    theFlagAndTheSavedChoiceAndEnglish();
    aShortTranslationShowsEnglishForWhatItLacks();
    theOptionsWordsAreDrawnInsideTheirSlots();
    theDashboardsTitlesAreFittedOnTheScreen();
    aTightLimitScalesTheHintTheCardAndThePopup();
}

} // namespace

MP_SUITE_MAIN_ARGS("test_mp_langlayer", argc, argv) {
    std::string recordDir;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--record-golden" && i + 1 < argc) {
            recordDir = argv[++i];
        } else {
            std::printf("test_mp test_mp_langlayer [--record-golden <dir for menu_screens.json>]\n");
            return 2;
        }
    }
    std::error_code ec;
    if (!std::filesystem::is_directory(kLevels, ec) || !std::filesystem::is_regular_file(kChapters, ec) ||
        !std::filesystem::is_regular_file(kData + "/entity_roles.json", ec)) {
        std::printf("test_mp_langlayer: SKIPPED - needs the converted levels at %s, chapters.json at %s and Enhanced's data at %s.\n",
                    kLevels.c_str(), kChapters.c_str(), kData.c_str());
        return 77;
    }
    if (!recordDir.empty()) return recordGolden(recordDir);
    runTests();
    return ::test::summary("test_mp_langlayer", 200);
}
