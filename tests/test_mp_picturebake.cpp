// The words the original draws as pixels, drawn again in the language on the screen (planning steps 247 and 248, LANG-2): sim/PictureBake.
//
// TWO SUITES. test_mp_picturebake is PURE: hand-authored synthetic pictures built in code (a procedural plate with a procedural 'ink' word and shadow standing in for an
// original sprite, a transparent canvas), the committed faces and the committed tables - nothing outside this repository. test_mp_picturebake_original runs the same code
// over the ORIGINAL's eleven sprites (the extracted assets, gitignored) and exits 77 without them.
//
// STEP 248 added (fixtures hand-authored in code: a plate with a straight frame, a medal-like chroma blob, a panel with a corner effect, a strip with transparent gaps):
//   plaque       the frame-column model erases a plaque to 2 grey levels, the ends of the bevel too, and nothing outside the rectangles changes; a chroma blob and a ring of two pixels
//                round it are untouched; EACH guard has a negative fixture that trips that guard ALONE and whose reason names it (golden art under the current recipe, current under
//                the golden, stray ink one row above the rectangle, ink in the margin columns, a tilted plate, a jittering profile, a profile that bends in the hole)
//   panel        V from the left margin alone (a corner effect on the right margin is not read); ink in the margin, above or below the rectangle, a plate that is not a profile
//                times a vignette and margins that disagree on too many rows are each refused by their own guard
//   strip        the cleared rows are drawn again left-aligned in black with no shadow, every row outside the windows is the source's; a phrase wraps after a dash that stands
//                alone, a descender slides the block up, the digit face draws other digits, a Japanese space is a gap only where the recipe says; a cleared band not bounded by
//                transparent rows and an ink window with an opaque pixel outside the cleared rows are refused; Ensure's name holds every phrase and the digit face's bytes
//   paragraph    the fewest lines, centred, inside its box, the pitch the face's, an orphan limit and the lines a recipe saves, and the floor; U+200B and an ASCII space lay out
//                the same glyphs in the three shipped Japanese phrases, and a no-break space keeps the French '!' on its line
//   the loader   the new rules, layouts and pairs are refused one fault at a time (a mini table of one recipe tries every pair the loader does not allow)
// The original-sprite suite adds the golden erase against the current plaque's text-free rows (the OFFLINE ground truth, never a donor), the medal untouched, no edge of the old
// lettering left, the strip's names and logos identical in ten languages, and the three medals composited over every language's current plaque (they hide no ink).
// MUTANTS of the step (37, one at a time, by an environment-variable switchboard in a copy of the sources) are listed in the planning record; each is failed by one of this file's
// two suites (the pure cases, or the original-sprite cases), by test_mp_langlayer or by test_mp_pickerlayer (the layer's three). The review of the step found eleven more alive,
// each closed by a case here: the ring of the plaque and of the panel read in one row only, the ink window of the first band alone, the digit rule at either end ('0' and '9'),
// kSlopeRows read over four rows, the strip's ink alpha multiplied twice, a heading not anchored by its last line, a band's own tunings ignored, the bands' pitch halved, the
// `paint` rectangle ignored, and the ink's right edge left out of the fit of a band.
//
//   table        game/data/picture_recipes.json loads as the seven pictures it should, each with its string key, and the loader REFUSES, one fault at a time, a missing member,
//                a size of zero, an upside-down rectangle, an unknown rule or layout or member, a digest that is not hex, a repeated id, a version it does not read, a step
//                of one, a safe rectangle outside the picture, a twin without its digest (the table's other numbers are untouched, and `out` is left as it was)
//   erase        row_lerp, separable and twin each restore a plate whose word was painted on it, to a stated tolerance, and change nothing outside the rectangle; the guards
//                refuse a picture whose reference columns carry ink, whose twin differs outside its window, or whose word reaches out of the rectangle
//   drawing      the phrase is drawn inside the rectangle and the box of the recipe, the picture outside the rectangle is byte for byte the source, the fit is monotone (a
//                longer phrase never gets a larger scale), lands on the recipe's grid of scales, and stops at the floor (a phrase that needs less is refused, with its
//                scale in the reason); the bytes are deterministic and the PNG is the recipe's size
//   the look     the drop shadow is drawn (darker round the word on a plate, down and to the right; black with alpha on a transparent canvas, capped); the ink of two stacked
//                lines is at least the recipe's gap apart, measured on what was put down, and the pitch grows only where it must; a word is shrunk for its height alone
//   languages    Arabic is drawn in the shaped, visual order (and a stack breaks it in LOGICAL order, so the first word is on the first line); a space in Japanese leaves no
//                gap and U+200B is never drawn; a no-break space is a gap that does not break
//   fail closed  a wrong size, a wrong digest, a code point no face holds, an empty phrase, a phrase that is not UTF-8, a face the slot does not draw with: each keeps the
//                English picture (ok is false, the reason names what, there is no picture), and Ensure leaves no file behind when the folder cannot be made or the target
//                cannot be put in place, finds a whole file without rendering, and renders again over a cut-short one, over one whose signature, width or height is wrong, and never
//                serves a cached file for another version (digest or size) of the sprite; a changed face file is another file name
//   the words    every shipped phrase of every language is drawn from glyphs its faces hold (the fit finds none missing)
//   where named  the seven sprites are named in the tables only where a language switch remakes the layer's cached path of them (the cache audit as a test)
//
// MUTATION TESTS were run on the code (planning step 247), 27 mutants one at a time, each rebuilt and run: the erase zeroed, the collar guard, the floor of a word and of a
// stack, the picture's digest, its size, a missing glyph drawn as the face's notdef, the twin's guards (outside the window, inside it, its digest), Arabic unshaped and
// reversed twice, the Japanese space drawn, U+200B drawn, a temporary file left behind, the PNG check of a cached file cut to its signature, ink allowed past the rectangle,
// and the layer's hook, its refresh loops (the menus', the pause's and end screens', the popups', English coming back, the lazy re-insert), the missing-key rule and the
// remembered failure. Each is failed by this suite or by test_mp_langlayer / test_mp_pickerlayer (the record is the step's, in the planning document). The review of the
// step found seven more alive and they were closed by the cases above: the drop shadow left out, the face bytes left out of a file's name, the pitch of two lines fixed, the
// height of a word ignored by the fit, the source check left out of Ensure, and the width and height and the signature of a cached file left unchecked.
//
// No non-ASCII character is written in this file: MSVC is built without /utf-8 here, so the text under test is read from the JSON or spelt as code points.

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "core/Json.hpp"

#include "sim/ArabicShaping.hpp"
#include "sim/FontBake.hpp"
#include "sim/PictureBake.hpp"
#include "sim/Strings.hpp"
#include "sim/Utf8.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

using namespace MagicPortals;
namespace PB = MagicPortals::PictureBake;
using Strings::Lang;

namespace {

namespace fs = std::filesystem;

const std::string kData = MAGICPORTALS_PORT_DATA_DIR;
const std::string kOriginal = MAGICPORTALS_ORIGINAL_DIR;

// The alpha at which a pixel of a baked picture is VISIBLE on a screen (a shadow's last faint ring is not), and the row of game_over.png at which the lost screen's buttons
// begin: the title is placed at (640, 252) and the buttons at (512, 432) and (768, 432), 1.405 screen px per hd px, and a button's visible top is then at hd row about 201
// (game/data/ui.json, level_end/lost: the measurements are there).
constexpr int kVisibleAlpha = 12;
constexpr int kRestartButtonTopRow = 201;

bool Contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

std::string Code(Lang lang) {
    return std::string(Strings::kLangCodes[Strings::IndexOf(lang)]);
}

std::string Cp(std::initializer_list<std::uint32_t> points) {
    return Utf8::Encode(std::vector<std::uint32_t>(points));
}

// The committed language table and faces, read once.
struct World {
    Strings::LanguageTable languages;
    std::vector<FontBake::FaceBytes> faces;
    PB::Recipes recipes;
    bool ok = false;
};

const World& TheWorld() {
    static const World world = [] {
        World w;
        std::string error;
        const bool languages = Strings::LoadLanguages(kData + Strings::kLanguagesFile, w.languages, error);
        CHECK_MSG(languages, "languages.json reads: " + error);
        if (!languages) return w;
        const bool faces = FontBake::LoadFaces(w.languages.Fonts(), kData + "/" + w.languages.Fonts().folder, w.faces, error);
        CHECK_MSG(faces, "the faces read: " + error);
        const bool recipes = PB::LoadRecipes(kData + PB::kRecipesFile, w.recipes, error);
        CHECK_MSG(recipes, "picture_recipes.json reads: " + error);
        w.ok = languages && faces && recipes;
        return w;
    }();
    return world;
}

PB::Request RequestFor(const PB::Recipe& recipe, Lang lang, const std::string& phrase) {
    PB::Request request;
    request.recipe = &recipe;
    request.plan = &TheWorld().languages.Fonts();
    request.faces = &TheWorld().faces;
    request.lang = lang;
    request.phrase = phrase;
    return request;
}

std::string TableText() {
    std::ifstream in(kData + PB::kRecipesFile, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

fs::path ScratchDir(const std::string& name) {
    const fs::path dir = fs::temp_directory_path() / ("supersonic-test-mp-picturebake-" + name);
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    return dir;
}

void WriteText(const fs::path& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << text;
}

bool Exists(const fs::path& path) {
    std::error_code ec;
    return fs::exists(path, ec);
}

// ---- the table ----------------------------------------------------------------------------------------------------------------------------------

void TheShippedTableLoadsAsTheElevenPictures() {
    const World& world = TheWorld();
    CHECK(world.ok);
    if (!world.ok) return;
    const PB::Recipes& recipes = world.recipes;
    CHECK(recipes.loaded && !recipes.fileHash.empty());
    CHECK_EQ(static_cast<int>(recipes.items.size()), 11);
    std::set<std::string> keys;
    std::set<std::string> sprites;
    for (const PB::Recipe& recipe : recipes.items) {
        for (const std::string& key : recipe.Keys()) keys.insert(key);
        sprites.insert(recipe.sprite);
        CHECK_MSG(recipes.Find(recipe.sprite) == &recipe, recipe.id + " is found by its sprite");
        CHECK_MSG(recipe.width > 0 && recipe.height > 0 && recipe.digest.size() == 32, recipe.id + " has its size and its digest");
        CHECK_MSG(recipe.erase.rect.Valid() && recipe.erase.rect.x1 <= recipe.width && recipe.erase.rect.y1 <= recipe.height, recipe.id + "'s rectangle is inside the picture");
        CHECK_MSG(recipe.text.minScale > 0.0 && recipe.text.minScale <= 1.0, recipe.id + " has a floor");
        CHECK_MSG(world.languages.Fonts().Slot(recipe.text.slot) != nullptr, recipe.id + " names a slot of the plan");
    }
    CHECK_MSG(recipes.Find("level_button.png") == nullptr && recipes.Find("") == nullptr, "a sprite no recipe names is not found (the twin is a source, not a recipe)");
    // The fifteen keys (ten phrases, and the five bands of the strip) are exactly the picture keys of the string table.
    std::set<std::string> want;
    for (const std::string_view key : Strings::kPictureKeys) want.insert(std::string(key));
    CHECK_MSG(keys == want && keys.size() == 15, "the recipes' keys are the string table's picture keys");
    const std::set<std::string> expected = {"main_play_game_button.png", "scores.png", "select_chapter.png", "achievement_points.png", "level_finished.png",
                                            "portals_created_plaque.png", "game_over.png", "current_score_plaque.png", "golden_score_plaque.png", "game_finished.png", "credit_name.png"};
    CHECK_MSG(sprites == expected, "and the sprites are the eleven the original draws words on");
    // Four plates (one of them a twin), two plaques, a panel, a strip and three generated pictures, as measured.
    int plates = 0;
    int twins = 0;
    int generated = 0;
    int plaques = 0;
    int panels = 0;
    int strips = 0;
    for (const PB::Recipe& recipe : recipes.items) {
        plates += recipe.erase.rule == PB::Rule::RowLerp || recipe.erase.rule == PB::Rule::Separable ? 1 : 0;
        twins += recipe.erase.rule == PB::Rule::Twin ? 1 : 0;
        generated += recipe.erase.rule == PB::Rule::Generated ? 1 : 0;
        plaques += recipe.erase.rule == PB::Rule::Plaque ? 1 : 0;
        panels += recipe.erase.rule == PB::Rule::Panel ? 1 : 0;
        strips += recipe.erase.rule == PB::Rule::ClearRows ? 1 : 0;
    }
    CHECK_MSG(plates == 3 && twins == 1 && generated == 3 && plaques == 2 && panels == 1 && strips == 1, "three modelled plates, one twin, three generated pictures, two plaques, a panel and a strip");
    // Each new picture's lettering is a paragraph (the plaques and the card) or bands (the strip), with a floor and the keys its recipe names.
    for (const PB::Recipe& recipe : recipes.items) {
        if (recipe.erase.rule == PB::Rule::Plaque || recipe.erase.rule == PB::Rule::Panel) CHECK_MSG(recipe.text.layout == PB::Layout::Paragraph && !recipe.key.empty(), recipe.id + " is a paragraph of one key");
        if (recipe.erase.rule == PB::Rule::ClearRows) CHECK_MSG(recipe.text.layout == PB::Layout::Bands && recipe.key.empty() && recipe.text.bands.size() == 5 && recipe.erase.clearRows.size() == 5, recipe.id + " is five bands and no key of its own");
        // The strip's heading (the fifth band) is set from its LAST baseline and has tunings of its own that are lighter than the roles' (the two are read by the loader, and a bake that
        // ignored either would change every language's strip: TheStripsAlphaTuningsPitchAnchorAndRightEdgeAreHeld holds them at the layout).
        if (recipe.erase.rule == PB::Rule::ClearRows && recipe.text.bands.size() == 5) {
            const PB::Band& heading = recipe.text.bands.back();
            bool lighter = !heading.faces.empty() && !recipe.text.faces.empty();
            for (const PB::FaceTuning& own : heading.faces) {
                for (const PB::FaceTuning& shared : recipe.text.faces) lighter = lighter && (own.face != shared.face || own.embolden < shared.embolden);
            }
            bool others = true;
            for (std::size_t i = 0; i + 1 < recipe.text.bands.size(); ++i) others = others && !recipe.text.bands[i].anchorLast && recipe.text.bands[i].faces.empty();
            CHECK_MSG(heading.anchorLast && lighter && others, recipe.id + ": only the heading is anchored by its last line and has an embolden of its own");
            // And only the heading is balanced (planning step 249), against the band above it (the modelling role, band 3) and the row of the first name (600), from two lines on: the
            // real-sprite checks look at the heading only where the table says it is balanced, so the table's saying so is held here. The rule's shipped line gap is the original's 0.15.
            bool unbalanced = true;
            for (std::size_t i = 0; i + 1 < recipe.text.bands.size(); ++i) unbalanced = unbalanced && !recipe.text.bands[i].balanced;
            CHECK_MSG(heading.balanced && heading.balanceAbove == 3 && heading.balanceBelow == 600.0 && heading.balanceMinLines == 2 && unbalanced,
                      recipe.id + ": only the heading is balanced, against band 3 and row 600, from two lines on");
            CHECK_MSG(std::fabs(recipe.text.gap - 0.15) < 1e-9, recipe.id + ": the lines of a band keep 0.15 of its size apart (the original's own design band does): " + std::to_string(recipe.text.gap));
        }
    }
}

// Loads `text` as the table; the result's error, or empty when it loaded. `out` is checked to be untouched by a refusal.
std::string LoadText(const std::string& name, const std::string& text) {
    const fs::path dir = ScratchDir("table-" + name);
    const fs::path file = dir / "picture_recipes.json";
    WriteText(file, text);
    PB::Recipes recipes;
    recipes.fileHash = "untouched";
    std::string error;
    const bool ok = PB::LoadRecipes(file.string(), recipes, error);
    CHECK_MSG(ok == error.empty(), name + ": an error exactly when it was refused");
    if (!ok) CHECK_MSG(recipes.fileHash == "untouched" && !recipes.loaded, name + ": a refusal leaves the table as it was");
    return error;
}

void TheLoaderRefusesWhatItShouldNot() {
    const std::string table = TableText();
    CHECK_MSG(LoadText("same", table).empty(), "the committed table, written again, loads");
    struct Fault {
        const char* name;
        std::string from;
        std::string to;
        std::string expect; // what the refusal names
    };
    // Each edit changes ONE member of the first recipe (tap_start) or of the table, and the refusal must name it.
    const std::vector<Fault> faults = {
        {"version", "\"version\": 1", "\"version\": 2", "version"},
        {"no version", "\"version\": 1,", "", "version"},
        {"size zero", "\"size\": [\n        512,\n        128\n      ]", "\"size\": [\n        0,\n        128\n      ]", "size"},
        {"digest not hex", "\"digest\": \"d17efe5ae96f120c776dd9957227c207\"", "\"digest\": \"D17EFE5AE96F120C776DD9957227C207\"", "digest"},
        {"digest short", "\"digest\": \"d17efe5ae96f120c776dd9957227c207\"", "\"digest\": \"d17efe5a\"", "digest"},
        {"unknown rule", "\"rule\": \"row_lerp\"", "\"rule\": \"blur\"", "rule"},
        {"unknown layout", "\"layout\": \"word\"", "\"layout\": \"banner\"", "layout"},
        {"upside-down rect", "\"rect\": [\n          160,\n          46,\n          351,\n          84\n        ]", "\"rect\": [\n          351,\n          46,\n          160,\n          84\n        ]",
         "rect"},
        {"rect outside", "\"rect\": [\n          160,\n          46,\n          351,\n          84\n        ]", "\"rect\": [\n          160,\n          46,\n          351,\n          840\n        ]",
         "rect"},
        {"no guard", "\"guard\": {\n          \"agree_max\": 3.0,\n          \"collar_max\": 5.0\n        }", "\"guard\": {\n          \"agree_max\": 3.0\n        }", "collar_max"},
        {"guard zero", "\"agree_max\": 3.0", "\"agree_max\": 0.0", "guard"},
        {"unknown member", "\"min_scale\": 0.55,", "\"min_scale\": 0.55,\n        \"min_scale_typo\": 0.5,", "min_scale_typo"},
        {"step one", "\"step\": 0.995", "\"step\": 1.0", "step"},
        {"floor zero", "\"min_scale\": 0.55", "\"min_scale\": 0.0", "min_scale"},
        {"anchor outside", "\"anchor\": [\n          255.5,\n          63.5\n        ]", "\"anchor\": [\n          2555.5,\n          63.5\n        ]", "anchor"},
        {"no shadow gain", "\"gain\": 0.95,", "", "gain"},
        {"gain zero", "\"gain\": 0.95", "\"gain\": 0.0", "gain"},
        {"cap above one", "\"cap\": 1.0", "\"cap\": 1.5", "cap"},
        {"stack member in a word", "\"max_height\": 34,", "\"max_height\": 34,\n        \"pitch\": 20.0,", "pitch"},
        {"max_lines four", "\"max_lines\": 2", "\"max_lines\": 4", "max_lines"},
        {"safe outside", "\"safe\": [\n          14,\n          10,\n          498,\n          114\n        ]", "\"safe\": [\n          14,\n          10,\n          498,\n          1140\n        ]", "safe"},
        {"ref_cols one band", "\"ref_cols\": [\n          [\n            160,\n            172\n          ],\n          [\n            338,\n            351\n          ]\n        ]",
         "\"ref_cols\": [\n          [\n            160,\n            172\n          ]\n        ]", "ref_cols"},
        {"twin digest missing", "\"twin_digest\"", "\"twin_digest_\"", "twin_digest"},
        {"twin no inside", "\"min_inside\": 1000", "\"min_inside\": 0", "min_inside"},
        {"repeated sprite", "\"sprite\": \"scores.png\"", "\"sprite\": \"main_play_game_button.png\"", "repeats"},
        {"empty id", "\"id\": \"tap_start\"", "\"id\": \"\"", "id"},
        {"face ratio zero", "\"ratio\": 1.25", "\"ratio\": 0.0", "ratio"},
        {"face reference", "\"reference\": \"U+0078\"", "\"reference\": \"x\"", "reference"},
        {"embolden above one", "\"embolden\": 0.035", "\"embolden\": 1.5", "embolden"},
        {"bad json", "\"version\": 1,", "\"version\": 1,,", "picture_recipes.json"},
    };
    for (const Fault& fault : faults) {
        std::string mutated = table;
        const std::size_t at = mutated.find(fault.from);
        if (at == std::string::npos) {
            CHECK_MSG(false, std::string("the text of fault '") + fault.name + "' is in the committed table (the table's layout changed: edit this case)");
            continue;
        }
        mutated.replace(at, fault.from.size(), fault.to);
        const std::string error = LoadText(fault.name, mutated);
        CHECK_MSG(!error.empty() && Contains(error, fault.expect), std::string("'") + fault.name + "' is refused, naming " + fault.expect + ": " + error);
    }
    std::string error;
    PB::Recipes none;
    CHECK_MSG(!PB::LoadRecipes(kData + "/no-such-table.json", none, error) && Contains(error, "cannot read"), "a missing file is refused: " + error);
}

// ---- synthetic pictures -------------------------------------------------------------------------------------------------------------------------

void Put(PB::Image& image, int x, int y, double r, double g, double b, double a) {
    unsigned char* px = image.At(x, y);
    px[0] = static_cast<unsigned char>(std::clamp(std::lround(r), 0L, 255L));
    px[1] = static_cast<unsigned char>(std::clamp(std::lround(g), 0L, 255L));
    px[2] = static_cast<unsigned char>(std::clamp(std::lround(b), 0L, 255L));
    px[3] = static_cast<unsigned char>(std::clamp(std::lround(a), 0L, 255L));
}

// A Gaussian blur of a w x h field, zero beyond it.
std::vector<double> Blur(const std::vector<double>& in, int w, int h, double sigma) {
    const int reach = static_cast<int>(std::ceil(3.0 * sigma));
    std::vector<double> kernel(static_cast<std::size_t>(2 * reach + 1));
    double sum = 0.0;
    for (int i = -reach; i <= reach; ++i) {
        kernel[static_cast<std::size_t>(i + reach)] = std::exp(-(i * i) / (2.0 * sigma * sigma));
        sum += kernel[static_cast<std::size_t>(i + reach)];
    }
    for (double& k : kernel) k /= sum;
    std::vector<double> across(in.size(), 0.0);
    std::vector<double> out(in.size(), 0.0);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            double s = 0.0;
            for (int i = -reach; i <= reach; ++i) {
                const int sx = x + i;
                if (sx >= 0 && sx < w) s += kernel[static_cast<std::size_t>(i + reach)] * in[static_cast<std::size_t>(y * w + sx)];
            }
            across[static_cast<std::size_t>(y * w + x)] = s;
        }
    }
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            double s = 0.0;
            for (int i = -reach; i <= reach; ++i) {
                const int sy = y + i;
                if (sy >= 0 && sy < h) s += kernel[static_cast<std::size_t>(i + reach)] * across[static_cast<std::size_t>(sy * w + x)];
            }
            out[static_cast<std::size_t>(y * w + x)] = s;
        }
    }
    return out;
}

// A white 'word' (a blob) with a dark halo down and to the right of it, painted over `plate` in straight alpha, as the original's lettering sits on its plates.
PB::Image WithAWord(const PB::Image& plate, const PB::Rect& blob, int halo, double haloSigma) {
    const int w = plate.width;
    const int h = plate.height;
    std::vector<double> ink(static_cast<std::size_t>(w * h), 0.0);
    std::vector<double> shifted(static_cast<std::size_t>(w * h), 0.0);
    for (int y = blob.y0; y < blob.y1; ++y) {
        for (int x = blob.x0; x < blob.x1; ++x) {
            ink[static_cast<std::size_t>(y * w + x)] = 1.0;
            if (x + halo < w && y + halo < h) shifted[static_cast<std::size_t>((y + halo) * w + x + halo)] = 1.0;
        }
    }
    const std::vector<double> shadow = Blur(shifted, w, h, haloSigma);
    PB::Image out = plate;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const double s = std::min(1.0, shadow[static_cast<std::size_t>(y * w + x)] * 0.9);
            const double c = ink[static_cast<std::size_t>(y * w + x)];
            const unsigned char* px = plate.At(x, y);
            const double a = px[3] / 255.0;
            const double oa = s + a * (1.0 - s);
            const double a2 = c + oa * (1.0 - c);
            double rgb[3];
            for (int k = 0; k < 3; ++k) {
                const double under = oa > 1e-9 ? (px[k] / 255.0) * (a * (1.0 - s)) / oa : 0.0;
                rgb[k] = a2 > 1e-9 ? (c + under * oa * (1.0 - c)) / a2 : 0.0;
            }
            Put(out, x, y, rgb[0] * 255.0, rgb[1] * 255.0, rgb[2] * 255.0, a2 * 255.0);
        }
    }
    return out;
}

// A recipe for a word of `px` cap pixels centred in `rect`, with the plate look of the shipped ones. The caller sets the erase.
PB::Recipe WordRecipe(const std::string& id, const PB::Image& picture, const PB::Rect& rect, double px, double minScale) {
    PB::Recipe recipe;
    recipe.id = id;
    recipe.key = std::string(Strings::Key::kPictureTapStart);
    recipe.sprite = id + ".png";
    recipe.width = picture.width;
    recipe.height = picture.height;
    recipe.digest = PB::Digest(picture);
    recipe.erase.rect = rect;
    PB::Text& text = recipe.text;
    text.layout = PB::Layout::Word;
    text.slot = "verdana";
    text.px = px;
    text.anchorX = (rect.x0 + rect.x1) / 2.0;
    text.anchorY = (rect.y0 + rect.y1) / 2.0;
    text.minScale = minScale;
    text.step = 0.98;
    text.inkThreshold = 0.04;
    text.maxWidth = rect.Width() - 12.0;
    text.maxHeight = rect.Height() - 6.0;
    text.shadow.dx = 0.15;
    text.shadow.dy = 0.15;
    text.shadow.sigma = 0.10;
    text.shadow.gain = 0.95;
    text.shadow.cap = 1.0;
    return recipe;
}

// A recipe for a stack of up to two lines on a transparent canvas of the picture's size, with the look of the shipped plaque (no offset, a halo around the ink), for the
// cases that need a generated picture. The caller sets the numbers it is about.
PB::Recipe StackRecipe(const PB::Image& canvas) {
    PB::Recipe recipe = WordRecipe("syn_stack", canvas, PB::Rect{0, 0, canvas.width, canvas.height}, 12.0, 0.6);
    recipe.erase.rule = PB::Rule::Generated;
    PB::Text& text = recipe.text;
    text.layout = PB::Layout::Stack;
    text.slot = "matura";
    text.px = 12.0;
    text.anchorX = 65.0;
    text.anchorY = 62.0;
    text.step = 0.97;
    text.inkThreshold = 0.02;
    text.maxWidth = 112.0;
    text.firstBaseline = 56.0;
    text.pitch = 28.0;
    text.gap = 0.08;
    text.maxLines = 2;
    text.safe = PB::Rect{6, 20, 122, 108};
    text.shadow.dx = 0.0;
    text.shadow.dy = 0.0;
    text.shadow.sigma = 0.12;
    text.shadow.sigmaMin = 1.5;
    text.shadow.gain = 1.5;
    text.shadow.cap = 0.7;
    text.shadow.roundOffsets = true;
    text.faces = {PB::FaceTuning{"kurale", 1.0, 0x78, 0.065}};
    return recipe;
}

// A translucent black plate, flat along x, whose alpha follows a ridge along y (the main menu's play button, in the small); a word on it.
struct RowPlate {
    PB::Image truth;
    PB::Image picture;
    PB::Recipe recipe;
};

RowPlate MakeRowPlate() {
    RowPlate plate;
    plate.truth = PB::Blank(160, 48);
    for (int y = 0; y < 48; ++y) {
        const double alpha = 200.0 * std::exp(-std::pow((y - 24.0) / 9.0, 2.0)) + 20.0;
        for (int x = 0; x < 160; ++x) Put(plate.truth, x, y, 0, 0, 0, static_cast<double>(static_cast<int>(alpha)));
    }
    plate.picture = WithAWord(plate.truth, PB::Rect{50, 18, 110, 30}, 3, 2.0);
    plate.recipe = WordRecipe("syn_row", plate.picture, PB::Rect{30, 10, 130, 42}, 12.0, 0.5);
    plate.recipe.erase.rule = PB::Rule::RowLerp;
    plate.recipe.erase.refCols = {PB::Span{10, 28}, PB::Span{132, 150}};
    plate.recipe.erase.guard.agreeMax = 3.0;
    plate.recipe.erase.guard.collarMax = 5.0;
    return plate;
}

// An opaque plate that is a vertical gradient times a horizontal vignette, with per-channel gains; a wide word on it.
RowPlate MakeSeparablePlate() {
    RowPlate plate;
    plate.truth = PB::Blank(160, 48);
    for (int y = 0; y < 48; ++y) {
        const double t = 0.2 + y * (2.9 - 0.2) / 47.0;
        const double fy = 40.0 + 60.0 * std::pow(std::sin(t), 2.0);
        for (int x = 0; x < 160; ++x) {
            double rx = 1.0;
            if (x < 14) rx = 0.55 + (1.0 - 0.55) * x / 13.0;
            if (x >= 160 - 14) rx = 0.55 + (1.0 - 0.55) * (159 - x) / 13.0;
            Put(plate.truth, x, y, fy * 1.0 * rx, fy * 0.85 * rx, fy * 1.25 * rx, 255);
        }
    }
    plate.picture = WithAWord(plate.truth, PB::Rect{24, 16, 136, 30}, 3, 2.0);
    plate.recipe = WordRecipe("syn_sep", plate.picture, PB::Rect{18, 10, 142, 40}, 12.0, 0.5);
    plate.recipe.erase.rule = PB::Rule::Separable;
    plate.recipe.erase.refCols = {PB::Span{6, 16}, PB::Span{144, 154}};
    plate.recipe.erase.refRows = {PB::Span{2, 8}};
    plate.recipe.erase.flatCols = PB::Span{50, 110};
    plate.recipe.erase.guard.collarMax = 4.0;
    plate.recipe.erase.guard.holdoutMean = 3.0;
    plate.recipe.erase.guard.holdoutMax = 8.0;
    return plate;
}

// The picture is another picture moved by (-2, -3) with a white window where the word is.
struct TwinPlate {
    PB::Image twin;
    PB::Image truth;
    PB::Image picture;
    PB::Recipe recipe;
};

TwinPlate MakeTwinPlate() {
    TwinPlate plate;
    plate.twin = PB::Blank(64, 64);
    for (int y = 0; y < 64; ++y) {
        for (int x = 0; x < 64; ++x) {
            const bool inside = x > 6 && x < 58 && y > 6 && y < 58;
            Put(plate.twin, x, y, (x * 3) % 256, (y * 4) % 256, ((x + y) * 2) % 256, inside ? 255 : 0);
        }
    }
    plate.truth = PB::Blank(64, 64);
    for (int y = 0; y < 64; ++y) {
        for (int x = 0; x < 64; ++x) {
            if (x + 2 < 64 && y + 3 < 64) std::copy_n(plate.twin.At(x + 2, y + 3), 4, plate.truth.At(x, y));
        }
    }
    plate.picture = plate.truth;
    for (int y = 40; y < 52; ++y) {
        for (int x = 10; x < 50; ++x) Put(plate.picture, x, y, 255, 255, 255, 255);
    }
    plate.recipe = WordRecipe("syn_twin", plate.picture, PB::Rect{6, 36, 54, 56}, 6.0, 0.5);
    plate.recipe.erase.rule = PB::Rule::Twin;
    plate.recipe.erase.twinSprite = "syn_twin_other.png";
    plate.recipe.erase.twinDigest = PB::Digest(plate.twin);
    plate.recipe.erase.shiftX = -2;
    plate.recipe.erase.shiftY = -3;
    plate.recipe.erase.checkRect = PB::Rect{8, 38, 52, 54};
    plate.recipe.erase.minInside = 100;
    plate.recipe.text.maxWidth = 40.0;
    plate.recipe.text.maxHeight = 14.0;
    return plate;
}

int MostDifference(const PB::Image& a, const PB::Image& b, const PB::Rect& where, double* mean = nullptr) {
    int worst = 0;
    double sum = 0.0;
    int count = 0;
    for (int y = where.y0; y < where.y1; ++y) {
        for (int x = where.x0; x < where.x1; ++x) {
            int d = 0;
            for (int c = 0; c < 4; ++c) d = std::max(d, std::abs(static_cast<int>(a.At(x, y)[c]) - static_cast<int>(b.At(x, y)[c])));
            worst = std::max(worst, d);
            sum += d;
            ++count;
        }
    }
    if (mean != nullptr) *mean = count > 0 ? sum / count : 0.0;
    return worst;
}

int PixelsChangedOutside(const PB::Image& a, const PB::Image& b, const PB::Rect& rect) {
    int changed = 0;
    for (int y = 0; y < a.height; ++y) {
        for (int x = 0; x < a.width; ++x) {
            if (rect.Contains(x, y)) continue;
            if (!std::equal(a.At(x, y), a.At(x, y) + 4, b.At(x, y))) ++changed;
        }
    }
    return changed;
}

void ErasingRestoresAKnownPlate() {
    // row_lerp: the plate is flat along x, so the model is exact up to the rounding of a level.
    {
        const RowPlate plate = MakeRowPlate();
        PB::Image out;
        std::string why;
        CHECK_MSG(PB::EraseWords(plate.recipe, plate.picture, nullptr, out, why), "the row_lerp plate is erased: " + why);
        double mean = 0.0;
        const int worst = MostDifference(out, plate.truth, plate.recipe.erase.rect, &mean);
        CHECK_MSG(worst <= 1, "row_lerp restores the plate to a level: " + std::to_string(worst));
        CHECK_EQ(PixelsChangedOutside(out, plate.picture, plate.recipe.erase.rect), 0);
        // And the word WAS there: the picture differs from the truth by a lot inside the rectangle (the test could not pass on a picture with no word).
        CHECK_MSG(MostDifference(plate.picture, plate.truth, plate.recipe.erase.rect) > 100, "the synthetic word is really on the plate");
    }
    // separable: a vertical profile times a horizontal vignette.
    {
        const RowPlate plate = MakeSeparablePlate();
        PB::Image out;
        std::string why;
        CHECK_MSG(PB::EraseWords(plate.recipe, plate.picture, nullptr, out, why), "the separable plate is erased: " + why);
        double mean = 0.0;
        const int worst = MostDifference(out, plate.truth, plate.recipe.erase.rect, &mean);
        CHECK_MSG(worst <= 3 && mean < 1.0, "separable restores the plate within 3 levels (mean under 1): " + std::to_string(worst) + ", " + std::to_string(mean));
        CHECK_EQ(PixelsChangedOutside(out, plate.picture, plate.recipe.erase.rect), 0);
        CHECK_MSG(MostDifference(plate.picture, plate.truth, plate.recipe.erase.rect) > 100, "the synthetic word is really on the plate");
    }
    // twin: the rectangle is the other picture's, bit for bit.
    {
        const TwinPlate plate = MakeTwinPlate();
        PB::Image out;
        std::string why;
        CHECK_MSG(PB::EraseWords(plate.recipe, plate.picture, &plate.twin, out, why), "the twin plate is erased: " + why);
        CHECK_EQ(MostDifference(out, plate.truth, plate.recipe.erase.rect), 0);
        CHECK_EQ(PixelsChangedOutside(out, plate.picture, plate.recipe.erase.rect), 0);
        CHECK_MSG(MostDifference(plate.picture, plate.truth, plate.recipe.erase.rect) > 100, "the window is really there");
    }
}

void TheGuardsRefuseAWrongPicture() {
    std::string why;
    PB::Image out;
    {
        // Ink in a reference band: the plate's word reaches into the column band the left side is read from.
        RowPlate plate = MakeRowPlate();
        for (int y = 12; y < 40; ++y) {
            for (int x = 14; x < 22; ++x) Put(plate.picture, x, y, 255, 255, 255, 255);
        }
        CHECK_MSG(!PB::EraseWords(plate.recipe, plate.picture, nullptr, out, why) && Contains(why, "reference bands"), "row_lerp refuses a band with ink in it: " + why);
    }
    {
        // The word's shadow reaches out of the rectangle, into the collar.
        RowPlate plate = MakeRowPlate();
        plate.recipe.erase.rect = PB::Rect{60, 10, 100, 42}; // narrower than the word at x 50..113
        CHECK_MSG(!PB::EraseWords(plate.recipe, plate.picture, nullptr, out, why) && Contains(why, "outside the rectangle"), "row_lerp refuses a rectangle the word leaves: " + why);
    }
    {
        // Separable: ink in the right band only, so the two halves of the model disagree.
        RowPlate plate = MakeSeparablePlate();
        for (int y = 12; y < 38; ++y) {
            for (int x = 146; x < 152; ++x) Put(plate.picture, x, y, 255, 255, 255, 255);
        }
        CHECK_MSG(!PB::EraseWords(plate.recipe, plate.picture, nullptr, out, why) && Contains(why, "left band alone"), "separable refuses a band with ink in it: " + why);
    }
    {
        // Separable: the rectangle too small for the word, so the ring beside it is not the plate.
        RowPlate plate = MakeSeparablePlate();
        plate.recipe.erase.rect = PB::Rect{40, 10, 120, 40};
        CHECK_MSG(!PB::EraseWords(plate.recipe, plate.picture, nullptr, out, why) && Contains(why, "outside the rectangle"), "separable refuses a rectangle the word leaves: " + why);
    }
    {
        // Twin: one pixel off outside the window, a twin of the wrong picture, a window with nothing in it, no twin at all.
        const TwinPlate plate = MakeTwinPlate();
        PB::Image bad = plate.picture;
        bad.At(5, 5)[0] ^= 1;
        CHECK_MSG(!PB::EraseWords(plate.recipe, bad, &plate.twin, out, why) && Contains(why, "outside the word's window"), "the twin guard sees ONE pixel: " + why);
        PB::Image otherTwin = plate.twin;
        otherTwin.At(20, 20)[1] ^= 1;
        CHECK_MSG(!PB::EraseWords(plate.recipe, plate.picture, &otherTwin, out, why) && Contains(why, "twin"), "another twin is refused: " + why);
        // The twin's pixel (0, 0) lands at (-2, -3) on the picture: off it, where neither guard looks, so only the twin's digest can tell this twin from the real one.
        PB::Image unseenTwin = plate.twin;
        unseenTwin.At(0, 0)[1] ^= 1;
        CHECK_MSG(!PB::EraseWords(plate.recipe, plate.picture, &unseenTwin, out, why) && Contains(why, "digest"), "a twin that differs only where no guard looks is refused by its digest: " + why);
        CHECK_MSG(!PB::EraseWords(plate.recipe, plate.truth, &plate.twin, out, why) && Contains(why, "only 0 pixels inside"), "a picture with no word is refused: " + why);
        CHECK_MSG(!PB::EraseWords(plate.recipe, plate.picture, nullptr, out, why) && Contains(why, "twin"), "no twin is refused: " + why);
    }
}

// ---- drawing ----------------------------------------------------------------------------------------------------------------------------------

void ThePhraseStaysInItsBoxAndTheRestIsTheSource() {
    const World& world = TheWorld();
    if (!world.ok) return;
    const RowPlate plate = MakeSeparablePlate();
    const std::string phrase = "TAP START";
    const PB::Outcome outcome = PB::Render(RequestFor(plate.recipe, Lang::De, phrase), plate.picture, nullptr);
    CHECK_MSG(outcome.ok, "the phrase is drawn: " + outcome.reason);
    if (!outcome.ok) return;
    const PB::Rect rect = plate.recipe.erase.rect;
    CHECK_MSG(outcome.image.width == plate.picture.width && outcome.image.height == plate.picture.height, "the picture is the recipe's size");
    CHECK_EQ(PixelsChangedOutside(outcome.image, plate.picture, rect), 0);
    CHECK_MSG(outcome.touched.x0 == rect.x0 && outcome.touched.x1 == rect.x1 && outcome.touched.y0 == rect.y0 && outcome.touched.y1 == rect.y1, "it says what it touched");
    // The ink is in the box of the recipe and clear of the rectangle's edges (its shadow reaches a little further, clipped by the rectangle).
    const PB::Rect ink = outcome.ink;
    CHECK_MSG(ink.Valid() && ink.Width() <= plate.recipe.text.maxWidth && ink.Height() <= plate.recipe.text.maxHeight, "the ink fits the recipe's box: " + std::to_string(ink.Width()) + " x " + std::to_string(ink.Height()));
    CHECK_MSG(ink.x0 > rect.x0 && ink.x1 < rect.x1 && ink.y0 > rect.y0 && ink.y1 < rect.y1, "and keeps a margin to the rectangle's edges");
    const double centre = (ink.x0 + ink.x1) / 2.0;
    CHECK_MSG(std::fabs(centre - plate.recipe.text.anchorX) <= 1.0, "the ink is centred on the anchor: " + std::to_string(centre));
    CHECK_EQ(outcome.lines, 1);
    CHECK_MSG(outcome.scale > 0.0 && outcome.scale <= 1.0, "the scale is a fit scale, never above one");
    // White ink: the glyphs' pixels are near white and opaque; the word is on the plate (the interior is brighter than the plate's own at the word's rows).
    int bright = 0;
    for (int y = rect.y0; y < rect.y1; ++y) {
        for (int x = rect.x0; x < rect.x1; ++x) {
            const unsigned char* px = outcome.image.At(x, y);
            if (px[0] > 235 && px[1] > 235 && px[2] > 235 && px[3] == 255) ++bright;
        }
    }
    CHECK_MSG(bright > 40, "white ink is drawn: " + std::to_string(bright) + " white pixels");
    // The same word on the other plates, and on the twin.
    const RowPlate row = MakeRowPlate();
    const PB::Outcome onRow = PB::Render(RequestFor(row.recipe, Lang::En, "GO"), row.picture, nullptr);
    CHECK_MSG(onRow.ok && PixelsChangedOutside(onRow.image, row.picture, row.recipe.erase.rect) == 0, "row_lerp: drawn, nothing outside the rectangle changes: " + onRow.reason);
    const TwinPlate twin = MakeTwinPlate();
    const PB::Outcome onTwin = PB::Render(RequestFor(twin.recipe, Lang::En, "pts"), twin.picture, &twin.twin);
    CHECK_MSG(onTwin.ok && PixelsChangedOutside(onTwin.image, twin.picture, twin.recipe.erase.rect) == 0, "twin: drawn, nothing outside the rectangle changes: " + onTwin.reason);
}

void TheFitIsMonotoneAndStopsAtTheFloor() {
    const World& world = TheWorld();
    if (!world.ok) return;
    RowPlate plate = MakeSeparablePlate();
    plate.recipe.text.step = 0.95;
    // Longer and longer phrases of one letter: the scale never grows, is 1.0 while it fits, is on the recipe's grid, and the ink is always inside the box.
    double previous = 2.0;
    int shrunk = 0;
    int refused = 0;
    std::string phrase;
    for (int length = 1; length <= 40; ++length) {
        phrase += "M";
        const PB::Outcome outcome = PB::FitPhrase(RequestFor(plate.recipe, Lang::En, phrase));
        if (!outcome.ok) {
            ++refused;
            CHECK_MSG(Contains(outcome.reason, "floor"), "a phrase that does not fit says why: " + outcome.reason);
            CHECK_MSG(outcome.image.Empty(), "and has no picture");
            continue;
        }
        CHECK_MSG(refused == 0, "once a phrase is refused, no longer one is drawn (length " + std::to_string(length) + ")");
        CHECK_MSG(outcome.scale <= previous + 1e-12, "the scale does not grow with the phrase: " + std::to_string(outcome.scale) + " after " + std::to_string(previous));
        CHECK_MSG(outcome.scale >= plate.recipe.text.minScale - 1e-9, "never below the floor");
        const double steps = std::log(outcome.scale) / std::log(plate.recipe.text.step);
        CHECK_MSG(std::fabs(steps - std::round(steps)) < 1e-6, "the scale is step^n: " + std::to_string(outcome.scale));
        CHECK_MSG(outcome.ink.Width() <= plate.recipe.text.maxWidth && outcome.ink.Height() <= plate.recipe.text.maxHeight, "the ink is inside the box");
        shrunk += outcome.scale < 1.0 ? 1 : 0;
        previous = outcome.scale;
    }
    CHECK_MSG(shrunk > 3 && refused > 3, "the run went from fitting to shrinking to refused: " + std::to_string(shrunk) + " shrunk, " + std::to_string(refused) + " refused");
    // The same phrase, a floor just above what it needs: refused, and its scale is in the reason; just below: drawn.
    const std::string wide = "MMMMMMMMMMMMMMMM";
    plate.recipe.text.minScale = 0.05;
    const PB::Outcome open = PB::FitPhrase(RequestFor(plate.recipe, Lang::En, wide));
    CHECK_MSG(open.ok && open.scale < 1.0, "a wide phrase fits when shrunk: " + open.reason);
    if (open.ok) {
        plate.recipe.text.minScale = open.scale + 1e-3;
        const PB::Outcome shut = PB::FitPhrase(RequestFor(plate.recipe, Lang::En, wide));
        CHECK_MSG(!shut.ok && Contains(shut.reason, "below the recipe's floor"), "a floor above what the phrase needs refuses it: " + shut.reason);
        plate.recipe.text.minScale = open.scale - 1e-3;
        CHECK_MSG(PB::FitPhrase(RequestFor(plate.recipe, Lang::En, wide)).ok, "and one below it draws it");
    }
}

void AnArabicWordIsDrawnInTheShapedOrder() {
    const World& world = TheWorld();
    if (!world.ok) return;
    const RowPlate plate = MakeSeparablePlate();
    // The TAP START of the Arabic table, spelt as code points: alef noon qaf reh, a space, lam lam beh dal hamza.
    const std::u32string logical = U"\u0627\u0646\u0642\u0631 \u0644\u0644\u0628\u062F\u0621";
    const std::string phrase = ArabicShaping::ToUtf8(logical);
    const PB::Outcome outcome = PB::FitPhrase(RequestFor(plate.recipe, Lang::Ar, phrase));
    CHECK_MSG(outcome.ok, "an Arabic phrase is drawn: " + outcome.reason);
    if (!outcome.ok) return;
    // What is drawn from left to right is exactly what Display makes of the line: the shaped forms in visual order, once.
    const std::vector<std::uint32_t> visual = Utf8::Decode(Strings::Display(Lang::Ar, phrase));
    CHECK_EQ(static_cast<int>(outcome.placed.size()), static_cast<int>(visual.size()));
    bool same = outcome.placed.size() == visual.size();
    for (std::size_t i = 0; same && i < visual.size(); ++i) same = outcome.placed[i].codePoint == visual[i];
    CHECK_MSG(same, "the glyphs are put down in the order Strings::Display leaves, left to right");
    bool increasing = true;
    for (std::size_t i = 1; i < outcome.placed.size(); ++i) increasing = increasing && outcome.placed[i].penX > outcome.placed[i - 1].penX;
    CHECK_MSG(increasing, "and the pen moves right along it");
    // The first LOGICAL letter (alef) is the right-most glyph of the first word, and the logical text is not what is drawn.
    std::vector<std::uint32_t> drawn;
    for (const PB::Placed& placed : outcome.placed) drawn.push_back(placed.codePoint);
    const std::vector<std::uint32_t> logicalPoints(logical.begin(), logical.end());
    CHECK_MSG(drawn != logicalPoints, "the logical text is not drawn as it stands");
    const auto presentation = [](std::uint32_t p) { return (p >= 0xFB50 && p <= 0xFDFF) || (p >= 0xFE70 && p <= 0xFEFF); };
    int forms = 0;
    for (const std::uint32_t point : drawn) forms += presentation(point) ? 1 : 0;
    CHECK_MSG(forms >= 8, "the letters are the presentation forms of the shaper: " + std::to_string(forms));
    // The same word, the other way round, is drawn the other way round: it is the language's display step that decides, not the phrase's letters alone.
    const PB::Outcome english = PB::FitPhrase(RequestFor(plate.recipe, Lang::En, phrase));
    CHECK_MSG(!english.ok || english.placed.empty() || english.placed.front().codePoint == logical.front(), "in a left-to-right language the same letters are drawn as they stand");

    // A stack breaks in LOGICAL order, so that the first word is on the first line (shaping the whole phrase first would put the words on the wrong lines).
    PB::Recipe stack = plate.recipe;
    stack.text.layout = PB::Layout::Stack;
    stack.text.slot = "matura";
    stack.text.maxLines = 2;
    stack.text.firstBaseline = 20.0;
    stack.text.pitch = 14.0;
    stack.text.gap = 0.08;
    stack.text.px = 6.0;
    stack.text.maxWidth = 100.0;
    stack.text.safe = PB::Rect{10, 4, 150, 44};
    const PB::Outcome stacked = PB::FitPhrase(RequestFor(stack, Lang::Ar, phrase));
    CHECK_MSG(stacked.ok && stacked.lines == 2 && stacked.lineTexts.size() == 2, "a stack of two lines: " + stacked.reason);
    if (stacked.ok && stacked.lineTexts.size() == 2) {
        CHECK_MSG(stacked.lineTexts[0] == ArabicShaping::ToUtf8(U"\u0627\u0646\u0642\u0631") && stacked.lineTexts[1] == ArabicShaping::ToUtf8(U"\u0644\u0644\u0628\u062F\u0621"),
                  "the first word of the phrase is the first line");
    }
}

void ASpaceInJapaneseLeavesNoGapAndAZeroWidthSpaceIsNeverDrawn() {
    const World& world = TheWorld();
    if (!world.ok) return;
    RowPlate plate = MakeSeparablePlate();
    plate.recipe.text.maxWidth = 120.0;
    // A real Japanese phrase with its break-only space (the shipped 'level finished').
    Strings::Table ja;
    std::string error;
    CHECK_MSG(ja.Load(kData, Lang::Ja, error), "the Japanese table loads: " + error);
    const std::string spaced = ja.Get(Strings::Key::kPictureLevelFinished);
    std::string joined;
    for (const char c : spaced) {
        if (c != ' ') joined += c;
    }
    CHECK_MSG(spaced != joined && spaced.find(' ') != std::string::npos, "the Japanese phrase has a space in it");
    const PB::Outcome a = PB::FitPhrase(RequestFor(plate.recipe, Lang::Ja, spaced));
    const PB::Outcome b = PB::FitPhrase(RequestFor(plate.recipe, Lang::Ja, joined));
    CHECK_MSG(a.ok && b.ok, "both are drawn: " + a.reason + b.reason);
    if (a.ok && b.ok) {
        CHECK_MSG(a.placed.size() == b.placed.size(), "no glyph is made of the space");
        bool same = a.placed.size() == b.placed.size();
        for (std::size_t i = 0; same && i < a.placed.size(); ++i) same = a.placed[i].codePoint == b.placed[i].codePoint && std::fabs(a.placed[i].penX - b.placed[i].penX) < 1e-9;
        CHECK_MSG(same, "and no gap is left: every pen is where it is without the space");
    }
    // In another language the same space is a gap: the pen after it is further on.
    const PB::Outcome gap = PB::FitPhrase(RequestFor(plate.recipe, Lang::En, "A B"));
    const PB::Outcome none = PB::FitPhrase(RequestFor(plate.recipe, Lang::En, "AB"));
    CHECK_MSG(gap.ok && none.ok && gap.placed.size() == 3 && none.placed.size() == 2 && gap.placed[2].penX > none.placed[1].penX + 1.0, "a space is a gap in English");
    // U+200B is never drawn, in any language, and a no-break space is a gap.
    const PB::Outcome zero = PB::FitPhrase(RequestFor(plate.recipe, Lang::En, "A" + Cp({0x200B}) + "B"));
    CHECK_MSG(zero.ok && zero.placed.size() == 2 && std::fabs(zero.placed[1].penX - none.placed[1].penX) < 1e-9, "U+200B is no glyph and no gap");
    const PB::Outcome nbsp = PB::FitPhrase(RequestFor(plate.recipe, Lang::En, "A" + Cp({0xA0}) + "B"));
    CHECK_MSG(nbsp.ok && nbsp.placed.size() == 3 && std::fabs(nbsp.placed[2].penX - gap.placed[2].penX) < 1e-9, "a no-break space is a gap as wide as the space");

    // In a stack the space and U+200B are where a line may break, and the no-break space is not.
    PB::Recipe stack = plate.recipe;
    stack.text.layout = PB::Layout::Stack;
    stack.text.slot = "matura";
    stack.text.maxLines = 2;
    stack.text.firstBaseline = 20.0;
    stack.text.pitch = 14.0;
    stack.text.gap = 0.08;
    stack.text.px = 6.0;
    stack.text.maxWidth = 150.0;
    stack.text.safe = PB::Rect{10, 2, 150, 46};
    const PB::Outcome broken = PB::FitPhrase(RequestFor(stack, Lang::En, "level finished"));
    CHECK_MSG(broken.ok && broken.lineTexts == std::vector<std::string>({"level", "finished"}), "level finished: two lines at the space");
    const PB::Outcome noBreak = PB::FitPhrase(RequestFor(stack, Lang::En, "ab" + Cp({0xA0}) + "cd ef"));
    CHECK_MSG(noBreak.ok && noBreak.lineTexts.size() == 2 && noBreak.lineTexts[0] == "ab" + Cp({0xA0}) + "cd" && noBreak.lineTexts[1] == "ef", "a no-break space is not a place to break");
    const PB::Outcome zeroBreak = PB::FitPhrase(RequestFor(stack, Lang::En, "abc" + Cp({0x200B}) + "def"));
    CHECK_MSG(zeroBreak.ok && zeroBreak.lineTexts.size() == 2, "U+200B is a place to break");
    const PB::Outcome oneWord = PB::FitPhrase(RequestFor(stack, Lang::En, "single"));
    CHECK_MSG(oneWord.ok && oneWord.lines == 1, "a phrase with no place to break is one line");
}

void AStackIsCentredOnItsAnchorAndTheMostLinesThatFitAreUsed() {
    const World& world = TheWorld();
    if (!world.ok) return;
    PB::Image canvas = PB::Blank(128, 128);
    PB::Recipe recipe = StackRecipe(canvas);
    PB::Text& text = recipe.text;
    const PB::Outcome outcome = PB::Render(RequestFor(recipe, Lang::En, "portals created"), canvas, nullptr);
    CHECK_MSG(outcome.ok, "a stack is drawn: " + outcome.reason);
    if (!outcome.ok) return;
    CHECK_MSG(outcome.lines == 2 && outcome.lineTexts == std::vector<std::string>({"portals", "created"}), "two lines, at the space");
    const double cx = (outcome.ink.x0 + outcome.ink.x1) / 2.0;
    const double cy = (outcome.ink.y0 + outcome.ink.y1) / 2.0;
    CHECK_MSG(std::fabs(cx - text.anchorX) <= 3.0 && std::fabs(cy - text.anchorY) <= 4.0, "the block's ink is centred on the anchor: " + std::to_string(cx) + ", " + std::to_string(cy));
    CHECK_MSG(outcome.ink.x0 >= text.safe.x0 && outcome.ink.x1 <= text.safe.x1 && outcome.ink.y0 >= text.safe.y0 && outcome.ink.y1 <= text.safe.y1, "inside the safe rectangle");
    // No pixel of the source: the canvas is transparent, and what is drawn is white ink with a soft dark shadow, so every colour is grey, and the corners stay empty.
    CHECK_EQ(static_cast<int>(outcome.image.At(0, 0)[3]), 0);
    CHECK_EQ(static_cast<int>(outcome.image.At(127, 127)[3]), 0);
    bool grey = true;
    int opaque = 0;
    for (int y = 0; y < 128; ++y) {
        for (int x = 0; x < 128; ++x) {
            const unsigned char* px = outcome.image.At(x, y);
            grey = grey && px[0] == px[1] && px[1] == px[2];
            opaque += px[3] == 255 ? 1 : 0;
            if (px[3] == 0) grey = grey && px[0] == 0;
        }
    }
    CHECK_MSG(grey && opaque > 100, "white ink on transparent: " + std::to_string(opaque) + " opaque pixels");
    // A marker-coloured source is never read: no output pixel equals the marker.
    PB::Image marker = PB::Blank(128, 128);
    for (int y = 0; y < 128; ++y) {
        for (int x = 0; x < 128; ++x) Put(marker, x, y, 17, 33, 49, 255);
    }
    PB::Recipe onMarker = recipe;
    onMarker.digest = PB::Digest(marker);
    const PB::Outcome fromMarker = PB::Render(RequestFor(onMarker, Lang::En, "portals created"), marker, nullptr);
    CHECK_MSG(fromMarker.ok, "a generated picture is drawn over whatever source passes the checks: " + fromMarker.reason);
    bool leaked = false;
    for (int y = 0; fromMarker.ok && y < 128; ++y) {
        for (int x = 0; x < 128; ++x) {
            const unsigned char* px = fromMarker.image.At(x, y);
            leaked = leaked || (px[0] == 17 && px[1] == 33 && px[2] == 49);
        }
    }
    CHECK_MSG(!leaked, "no pixel of the source is in a generated picture");
    CHECK_MSG(fromMarker.ok && fromMarker.image.rgba == outcome.image.rgba, "and it is the same picture whatever the source was");
    // The most lines that fit at or above the floor are used: two where they fit, one where the block is too tall for two (and the one line is within the box).
    PB::Recipe tight = recipe;
    tight.text.minScale = 0.95;
    tight.text.maxWidth = 112.0;
    const PB::Outcome two = PB::FitPhrase(RequestFor(tight, Lang::En, "abc defg"));
    CHECK_MSG(two.ok && two.lines == 2 && two.scale == 1.0, "two lines where they fit: " + two.reason);
    tight.text.safe = PB::Rect{6, 48, 122, 76}; // 28 pixels tall: a line, not two
    const PB::Outcome one = PB::FitPhrase(RequestFor(tight, Lang::En, "abc defg"));
    CHECK_MSG(one.ok && one.lines == 1 && one.lineTexts.size() == 1 && one.lineTexts[0] == "abc defg", "one line where two cannot fit the block: " + one.reason);
    // And none where nothing reaches the floor.
    tight.text.safe = recipe.text.safe;
    tight.text.maxWidth = 30.0;
    const PB::Outcome none = PB::FitPhrase(RequestFor(tight, Lang::En, "portals created"));
    CHECK_MSG(!none.ok && Contains(none.reason, "floor"), "none where not even two reach the floor: " + none.reason);
}

// ---- the look: the shadow, the distance between two lines, the height of a word -------------------------------------------------------------------

// The baseline of the first glyph of `line` (the recipe's pitch is the distance between two of these), or -1 when the outcome has none.
double BaselineOfLine(const PB::Outcome& outcome, int line) {
    for (const PB::Placed& placed : outcome.placed) {
        if (placed.line == line) return placed.baseline;
    }
    return -1.0;
}

// THE DROP SHADOW is a stated part of the look ("a drop shadow like the original's"), and a bake that drew the ink alone would pass every other check in this file. It
// lives where the ink's own coverage is nil, so it is looked for OUTSIDE the ink's box: on an opaque plate it darkens the plate there (black under white, down and to the
// right of the word because the offset is), and on a transparent canvas it is the only thing there, black with alpha.
void TheDropShadowIsDrawn() {
    const World& world = TheWorld();
    if (!world.ok) return;
    {
        const RowPlate plate = MakeSeparablePlate();
        PB::Image erased;
        std::string why;
        CHECK_MSG(PB::EraseWords(plate.recipe, plate.picture, nullptr, erased, why), "the plate is erased: " + why);
        const PB::Outcome outcome = PB::Render(RequestFor(plate.recipe, Lang::En, "TAP START"), plate.picture, nullptr);
        CHECK_MSG(outcome.ok, "the phrase is drawn: " + outcome.reason);
        if (!outcome.ok || erased.Empty()) return;
        const PB::Rect rect = plate.recipe.erase.rect;
        int behind = 0; // shaded pixels right of or below the ink's box, where the offset puts the shadow
        int before = 0; // and left of or above it
        int lighter = 0;
        for (int y = rect.y0; y < rect.y1; ++y) {
            for (int x = rect.x0; x < rect.x1; ++x) {
                if (outcome.ink.Contains(x, y)) continue;
                int darker = 0;
                int brighter = 0;
                for (int c = 0; c < 3; ++c) {
                    const int d = static_cast<int>(erased.At(x, y)[c]) - static_cast<int>(outcome.image.At(x, y)[c]);
                    darker = std::max(darker, d);
                    brighter = std::max(brighter, -d);
                }
                lighter += brighter > 1 ? 1 : 0;
                if (darker < 6) continue;
                if (x >= outcome.ink.x1 || y >= outcome.ink.y1) {
                    ++behind;
                } else {
                    ++before;
                }
            }
        }
        CHECK_MSG(behind >= 40, "the plate is darker round the word than the erased plate is: " + std::to_string(behind) + " pixels by 6 levels or more");
        CHECK_MSG(before * 3 <= behind, "and the shadow falls down and to the right: " + std::to_string(behind) + " behind the word, " + std::to_string(before) + " before it");
        CHECK_MSG(lighter == 0, "nothing outside the ink is lighter than the erased plate: " + std::to_string(lighter));
    }
    {
        PB::Image canvas = PB::Blank(128, 128);
        const PB::Recipe recipe = StackRecipe(canvas);
        const PB::Outcome outcome = PB::Render(RequestFor(recipe, Lang::En, "portals created"), canvas, nullptr);
        CHECK_MSG(outcome.ok, "the stack is drawn: " + outcome.reason);
        if (!outcome.ok) return;
        int halo = 0;
        int strongest = 0;
        int coloured = 0;
        for (int y = 0; y < canvas.height; ++y) {
            for (int x = 0; x < canvas.width; ++x) {
                if (outcome.ink.Contains(x, y)) continue;
                const unsigned char* px = outcome.image.At(x, y);
                if (px[3] < 10) continue;
                ++halo;
                strongest = std::max(strongest, static_cast<int>(px[3]));
                coloured += (px[0] != 0 || px[1] != 0 || px[2] != 0) ? 1 : 0;
            }
        }
        CHECK_MSG(halo >= 150, "a halo of soft pixels round the ink of a generated picture: " + std::to_string(halo));
        CHECK_MSG(coloured == 0, "the shadow is black: " + std::to_string(coloured) + " coloured pixels");
        CHECK_MSG(strongest >= 100 && strongest <= 180, "and its alpha reaches the recipe's cap (0.7) and no more: " + std::to_string(strongest));
    }
}

// THE DISTANCE BETWEEN TWO LINES. A stack's pitch is pushed apart where the first line's descenders reach into the second line's ascenders, and the ink of the two lines
// must not touch (the tall marks of Arabic and Japanese are why the pitch is fitted and not fixed). `gggg llll` set at a pitch tighter than its glyphs would overlap;
// the layout must say how far apart the ink really is, measured on what was put down, and that must be what the recipe's gap asks for.
void TwoStackedLinesStayApart() {
    const World& world = TheWorld();
    if (!world.ok) return;
    PB::Image canvas = PB::Blank(128, 128);
    PB::Recipe recipe = StackRecipe(canvas);
    PB::Text& text = recipe.text;
    text.safe = PB::Rect{6, 4, 122, 124};
    const double wanted = text.gap * text.px; // pixels at scale 1
    // Tighter than the glyphs: the pitch has to grow.
    text.firstBaseline = 53.0;
    text.pitch = 18.0;
    const PB::Outcome tight = PB::FitPhrase(RequestFor(recipe, Lang::En, "gggg llll"));
    CHECK_MSG(tight.ok && tight.lines == 2 && tight.scale == 1.0, "a stack of two lines at full size: " + tight.reason);
    if (tight.ok) {
        const double pitch = BaselineOfLine(tight, 1) - BaselineOfLine(tight, 0);
        CHECK_MSG(pitch > text.pitch + 2.0, "the pitch grew past the recipe's " + std::to_string(text.pitch) + " to keep the lines apart: " + std::to_string(pitch));
        CHECK_MSG(tight.lineGap > 0.0 && tight.lineGap < PB::kNoLineGap && tight.lineGap >= wanted - 0.5, "and the ink of the two lines is at least the gap apart: " + std::to_string(tight.lineGap) + " against " + std::to_string(wanted));
    }
    // Where the recipe's pitch already keeps them apart it is left as it is.
    text.firstBaseline = 40.0;
    text.pitch = 44.0;
    const PB::Outcome loose = PB::FitPhrase(RequestFor(recipe, Lang::En, "gggg llll"));
    CHECK_MSG(loose.ok && loose.lines == 2, "the same two lines at a loose pitch: " + loose.reason);
    if (loose.ok) {
        const double pitch = BaselineOfLine(loose, 1) - BaselineOfLine(loose, 0);
        CHECK_MSG(std::fabs(pitch - text.pitch * loose.scale) <= 0.5, "the recipe's pitch is kept where it is enough: " + std::to_string(pitch));
        CHECK_MSG(loose.lineGap < PB::kNoLineGap && loose.lineGap >= wanted * loose.scale - 0.5, "and the lines are further apart than the gap: " + std::to_string(loose.lineGap));
    }
    // One line has no gap to speak of.
    const PB::Outcome single = PB::FitPhrase(RequestFor(recipe, Lang::En, "gggg"));
    CHECK_MSG(single.ok && single.lines == 1 && single.lineGap == PB::kNoLineGap, "one line reports no gap: " + single.reason);
}

// THE HEIGHT OF A WORD is a limit of its own: a narrow, tall phrase is shrunk for its height alone. The step is coarse so that a fit that ignored the height could not hide in
// the rounding of a scale that is a hair above the answer.
void AWordTallerThanItsBoxIsShrunkForItsHeightAlone() {
    const World& world = TheWorld();
    if (!world.ok) return;
    RowPlate plate = MakeSeparablePlate();
    PB::Text& text = plate.recipe.text;
    text.step = 0.8;
    text.minScale = 0.3;
    text.maxWidth = 112.0;
    text.maxHeight = 9.0; // the capitals of two M are 12 pixels tall at full size, and about 30 wide
    const PB::Outcome outcome = PB::FitPhrase(RequestFor(plate.recipe, Lang::En, "MM"));
    CHECK_MSG(outcome.ok, "a phrase is drawn that is narrow and too tall: " + outcome.reason);
    if (outcome.ok) {
        CHECK_MSG(outcome.scale < 1.0, "it is shrunk: " + std::to_string(outcome.scale));
        CHECK_MSG(outcome.ink.Height() <= text.maxHeight, "until its ink is inside the box's height: " + std::to_string(outcome.ink.Height()));
        CHECK_MSG(outcome.ink.Width() * 3 < text.maxWidth, "and the width was never the limit: " + std::to_string(outcome.ink.Width()));
    }
    // A floor above the scale the height needs refuses it, for the height.
    text.minScale = 0.7;
    const PB::Outcome shut = PB::FitPhrase(RequestFor(plate.recipe, Lang::En, "MM"));
    CHECK_MSG(!shut.ok && Contains(shut.reason, "below the recipe's floor"), "a floor above what the height needs refuses it: " + shut.reason);
}

// ---- failing closed ----------------------------------------------------------------------------------------------------------------------------

void AnythingWrongKeepsTheEnglishPicture() {
    const World& world = TheWorld();
    if (!world.ok) return;
    const RowPlate plate = MakeSeparablePlate();
    const PB::Request request = RequestFor(plate.recipe, Lang::En, "TAP");
    // A wrong size, a wrong digest (one level of one pixel), no picture at all.
    {
        PB::Image smaller = PB::Blank(plate.picture.width - 1, plate.picture.height);
        const PB::Outcome o = PB::Render(request, smaller, nullptr);
        CHECK_MSG(!o.ok && Contains(o.reason, "159 x 48") && Contains(o.reason, "160 x 48") && o.image.Empty(), "a wrong size is refused, naming both: " + o.reason);
    }
    {
        PB::Image bad = plate.picture;
        bad.At(100, 5)[1] ^= 1;
        const PB::Outcome o = PB::Render(request, bad, nullptr);
        CHECK_MSG(!o.ok && Contains(o.reason, "digest") && o.image.Empty(), "a wrong digest is refused: " + o.reason);
    }
    {
        const PB::Outcome o = PB::Render(request, PB::Image(), nullptr);
        CHECK_MSG(!o.ok && Contains(o.reason, "cannot be read"), "no picture is refused: " + o.reason);
    }
    // A code point no face holds (Thai), in a Latin phrase and, shaped, nowhere else: named, with its face.
    {
        const PB::Outcome o = PB::Render(RequestFor(plate.recipe, Lang::En, "TAP " + Cp({0x0E01})), plate.picture, nullptr);
        CHECK_MSG(!o.ok && Contains(o.reason, "no glyph for U+0E01") && Contains(o.reason, "noto_sans_bold") && o.image.Empty(), "a missing glyph is named: " + o.reason);
    }
    // An empty phrase, one that is not UTF-8, a phrase of spaces only.
    CHECK_MSG(!PB::Render(RequestFor(plate.recipe, Lang::En, ""), plate.picture, nullptr).ok, "an empty phrase is refused");
    CHECK_MSG(!PB::Render(RequestFor(plate.recipe, Lang::En, std::string("TAP ") + static_cast<char>(0xFF)), plate.picture, nullptr).ok, "a phrase that is not UTF-8 is refused");
    CHECK_MSG(!PB::Render(RequestFor(plate.recipe, Lang::En, "   "), plate.picture, nullptr).ok, "a phrase of spaces has nothing to draw");
    // A recipe naming a face its slot does not draw with, a slot the plan lacks, a face nobody read.
    {
        PB::Recipe other = plate.recipe;
        other.text.faces = {PB::FaceTuning{"no_such_face", 1.0, 0, 0.0}};
        const PB::Outcome o = PB::Render(RequestFor(other, Lang::En, "TAP"), plate.picture, nullptr);
        CHECK_MSG(!o.ok && Contains(o.reason, "no_such_face"), "a face the slot does not draw with: " + o.reason);
        other = plate.recipe;
        other.text.slot = "no_such_slot";
        const PB::Outcome slot = PB::Render(RequestFor(other, Lang::En, "TAP"), plate.picture, nullptr);
        CHECK_MSG(!slot.ok && Contains(slot.reason, "no_such_slot"), "a slot the plan lacks: " + slot.reason);
        std::vector<FontBake::FaceBytes> fewer = TheWorld().faces;
        fewer.erase(std::remove_if(fewer.begin(), fewer.end(), [](const FontBake::FaceBytes& f) { return f.id == "noto_sans_bold"; }), fewer.end());
        PB::Request unread = RequestFor(plate.recipe, Lang::En, "TAP");
        unread.faces = &fewer;
        const PB::Outcome face = PB::Render(unread, plate.picture, nullptr);
        CHECK_MSG(!face.ok && Contains(face.reason, "noto_sans_bold"), "a face that was not read: " + face.reason);
        PB::Request nothing;
        CHECK_MSG(!PB::Render(nothing, plate.picture, nullptr).ok, "a request with no recipe");
    }
    // The ink must stay inside the rectangle: a recipe whose box is bigger than its rectangle is refused, not cut.
    {
        PB::Recipe big = plate.recipe;
        big.text.maxWidth = 400.0;
        big.text.maxHeight = 48.0;
        big.text.px = 40.0;
        big.text.minScale = 0.9;
        const PB::Outcome o = PB::Render(RequestFor(big, Lang::En, "WIDE WORD"), plate.picture, nullptr);
        CHECK_MSG(!o.ok && Contains(o.reason, "outside the rectangle"), "ink that would be cut is refused: " + o.reason);
    }
}

void TheBytesAreDeterministicAndThePngIsTheRecipesSize() {
    const World& world = TheWorld();
    if (!world.ok) return;
    const RowPlate plate = MakeSeparablePlate();
    for (const Lang lang : {Lang::En, Lang::Ru, Lang::Ar}) {
        const std::string phrase = lang == Lang::En ? "TAP START" : lang == Lang::Ru ? Cp({0x041D, 0x0410, 0x0416, 0x041C, 0x0418, 0x0422, 0x0415}) : ArabicShaping::ToUtf8(U"\u0627\u0646\u0642\u0631");
        const PB::Outcome a = PB::Render(RequestFor(plate.recipe, lang, phrase), plate.picture, nullptr);
        const PB::Outcome b = PB::Render(RequestFor(plate.recipe, lang, phrase), plate.picture, nullptr);
        CHECK_MSG(a.ok && b.ok, Code(lang) + ": drawn: " + a.reason + b.reason);
        if (!a.ok || !b.ok) continue;
        std::string one;
        std::string two;
        CHECK(PB::EncodePng(a.image, one) && PB::EncodePng(b.image, two));
        CHECK_MSG(one == two && !one.empty(), Code(lang) + ": two bakes are the same bytes");
        PB::Image back;
        std::string why;
        CHECK_MSG(PB::DecodePng(one, back, why) && back.width == plate.recipe.width && back.height == plate.recipe.height && back.rgba == a.image.rgba,
                  Code(lang) + ": the PNG is the recipe's size and decodes to the picture: " + why);
    }
    // The encoder refuses nothing it should accept, and the decoder refuses what is not a picture.
    PB::Image back;
    std::string why;
    CHECK_MSG(!PB::DecodePng("not a png", back, why) && !why.empty(), "the decoder refuses bytes that are no picture: " + why);
    CHECK_MSG(!PB::DecodePng("", back, why), "and no bytes");
    std::string bytes;
    CHECK_MSG(!PB::EncodePng(PB::Image(), bytes) && bytes.empty(), "an empty picture is not encoded");
    CHECK_MSG(PB::Blank(0, 5).Empty() && PB::Blank(5, 0).Empty() && PB::Blank(PB::kMaxSide + 1, 5).Empty() && !PB::Blank(3, 3).Empty(), "Blank refuses sizes that are not pictures");
    CHECK_MSG(PB::Digest(plate.picture) == plate.recipe.digest && PB::Digest(plate.truth) != plate.recipe.digest && PB::Digest(plate.picture).size() == 32, "the digest names the pixels");
    PB::Image moved = plate.picture;
    moved.At(0, 0)[3] ^= 1;
    CHECK_MSG(PB::Digest(moved) != PB::Digest(plate.picture), "and one bit of them");
    PB::Image taller = PB::Blank(plate.picture.width / 2, plate.picture.height * 2);
    PB::Image wider = PB::Blank(plate.picture.width * 2, plate.picture.height / 2);
    CHECK_MSG(PB::Digest(taller) != PB::Digest(wider), "and the shape of them, with the same number of pixels");
}

// ---- files ---------------------------------------------------------------------------------------------------------------------------------------

std::vector<std::string> FilesIn(const fs::path& dir) {
    std::vector<std::string> names;
    std::error_code ec;
    for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) names.push_back(it->path().filename().string());
    std::sort(names.begin(), names.end());
    return names;
}

void EnsureWritesWholeOrNothing() {
    const World& world = TheWorld();
    if (!world.ok) return;
    const RowPlate plate = MakeSeparablePlate();
    const fs::path root = ScratchDir("ensure");
    const fs::path sources = root / "sources";
    fs::create_directories(sources);
    std::string bytes;
    CHECK(PB::EncodePng(plate.picture, bytes));
    WriteText(sources / plate.recipe.sprite, bytes);

    PB::Recipes recipes;
    recipes.items = {plate.recipe};
    recipes.fileHash = "synthetic-table";
    recipes.loaded = true;
    const auto wanted = [&](const std::string& phrase, const fs::path& directory) {
        PB::Wanted w;
        w.recipe = &recipes.items[0];
        w.recipes = &recipes;
        w.plan = &TheWorld().languages.Fonts();
        w.fontsDir = kData + "/" + TheWorld().languages.Fonts().folder;
        w.lang = Lang::De;
        w.languageCode = "de";
        w.phrase = phrase;
        w.sourcePath = (sources / plate.recipe.sprite).string();
        w.directory = directory.string();
        return w;
    };

    const fs::path out = root / "pictures";
    const PB::Written first = PB::Ensure(wanted("TAP START", out));
    CHECK_MSG(first.ok && !first.cached && !first.path.empty(), "the first ask bakes: " + first.reason);
    if (!first.ok) return;
    CHECK_MSG(fs::is_regular_file(first.path) && FilesIn(out).size() == 1, "one file, and no temporary beside it");
    const std::string name = fs::path(first.path).filename().string();
    CHECK_MSG(name.rfind("syn_sep_de_", 0) == 0 && name.size() == std::string("syn_sep_de_").size() + 16 + 4 && name.substr(name.size() - 4) == ".png",
              "the name carries the recipe, the language and 16 hex: " + name);
    PB::Image baked;
    std::string why;
    CHECK_MSG(PB::ReadImage(first.path, baked, why) && baked.width == plate.recipe.width && baked.height == plate.recipe.height, "the file is a picture of the recipe's size: " + why);
    // The second ask finds the whole file and renders nothing.
    const auto stamp = fs::last_write_time(first.path);
    const PB::Written again = PB::Ensure(wanted("TAP START", out));
    CHECK_MSG(again.ok && again.cached && again.path == first.path && fs::last_write_time(first.path) == stamp, "the second ask finds it and writes nothing");
    // Another phrase, another language, another table are other files.
    const PB::Written other = PB::Ensure(wanted("TAP", out));
    CHECK_MSG(other.ok && other.path != first.path && FilesIn(out).size() == 2, "another phrase is another file");
    PB::Wanted french = wanted("TAP START", out);
    french.languageCode = "fr";
    CHECK_MSG(PB::Ensure(french).path != first.path, "another language is another file");
    recipes.fileHash = "another-table";
    CHECK_MSG(PB::Ensure(wanted("TAP START", out)).path != first.path, "another table is another file");
    recipes.fileHash = "synthetic-table";
    // Another FACE is another file: the bytes of the faces the slot draws with are in the name, and the scratch folder outlives an update of the app, so a font file that has
    // changed must never find the picture drawn with the old one. (A copy of the fonts folder with one byte appended to the slot's text face: stb_truetype ignores it, so
    // the picture is the same and only the name can tell.)
    {
        const FontBake::Plan& plan = TheWorld().languages.Fonts();
        const FontBake::SlotSpec* slot = plan.Slot(plate.recipe.text.slot);
        const FontBake::FaceSpec* face = slot != nullptr ? plan.Face(slot->textFace) : nullptr;
        CHECK_MSG(face != nullptr, "the slot's text face is in the plan");
        if (face != nullptr) {
            const fs::path same = root / "fonts-same";
            const fs::path changed = root / "fonts-changed";
            std::error_code copied;
            fs::copy(kData + "/" + plan.folder, same, fs::copy_options::recursive, copied);
            CHECK_MSG(!copied, "the fonts folder is copied: " + copied.message());
            fs::copy(kData + "/" + plan.folder, changed, fs::copy_options::recursive, copied);
            CHECK_MSG(!copied, "and copied again: " + copied.message());
            {
                std::ofstream append(changed / face->file, std::ios::binary | std::ios::app);
                append.put('\0');
            }
            PB::Wanted fromCopy = wanted("TAP START", out);
            fromCopy.fontsDir = same.string();
            const PB::Written unchanged = PB::Ensure(fromCopy);
            CHECK_MSG(unchanged.ok && unchanged.cached && unchanged.path == first.path, "a copy of the fonts folder, byte for byte, finds the same file: " + unchanged.reason);
            PB::Wanted fromChanged = wanted("TAP START", out);
            fromChanged.fontsDir = changed.string();
            const PB::Written changedFile = PB::Ensure(fromChanged);
            CHECK_MSG(changedFile.ok && !changedFile.cached && changedFile.path != first.path, "a face that changed by one byte is another file: " + changedFile.reason);
        }
    }
    // A file cut short is found NOT whole and made again; so is an empty one and one with the wrong signature.
    for (const std::size_t keep : {static_cast<std::size_t>(0), static_cast<std::size_t>(7), static_cast<std::size_t>(40), baked.rgba.size() / 100}) {
        std::string whole;
        CHECK(PB::EncodePng(baked, whole));
        // The file is read and CLOSED before it is cut and before Ensure runs: Windows refuses to put a new file over one that is open (the stream's share mode has no delete),
        // which is the test's own handle and not a fault of the bake (the first run of this suite on Windows, planning step 250, said "cannot be put in place").
        std::string held;
        {
            std::ifstream in(first.path, std::ios::binary);
            held.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        }
        WriteText(first.path, held.substr(0, std::min(keep, held.size() - 1)));
        const PB::Written healed = PB::Ensure(wanted("TAP START", out));
        CHECK_MSG(healed.ok && !healed.cached, "a file cut to " + std::to_string(keep) + " bytes is rendered again: " + healed.reason);
        PB::Image check;
        CHECK_MSG(PB::ReadImage(first.path, check, why) && check.rgba == baked.rgba, "and is whole again");
    }
    // A cached file is served for the picture it was made from, and only if it LOOKS whole. (1) The sprite on the disk is another picture now (another version of the
    // art, the same name): the file of the same name is NOT served and the ask fails closed - the name holds no digest of the sprite, so only the check made on every ask
    // can tell. (2) The same for a sprite of another size.
    {
        const fs::path elsewhere = root / "other-sources";
        fs::create_directories(elsewhere);
        std::string wrongBytes;
        CHECK(PB::EncodePng(plate.truth, wrongBytes));
        WriteText(elsewhere / plate.recipe.sprite, wrongBytes);
        PB::Wanted swapped = wanted("TAP START", out);
        swapped.sourcePath = (elsewhere / plate.recipe.sprite).string();
        const PB::Written refused = PB::Ensure(swapped);
        CHECK_MSG(!refused.ok && !refused.cached && Contains(refused.reason, "digest"), "a cached file is not served for another version of the sprite: " + refused.reason);
        std::string smallBytes;
        CHECK(PB::EncodePng(PB::Blank(plate.picture.width - 1, plate.picture.height), smallBytes));
        WriteText(elsewhere / plate.recipe.sprite, smallBytes);
        const PB::Written resized = PB::Ensure(swapped);
        CHECK_MSG(!resized.ok && !resized.cached && Contains(resized.reason, "159 x 48"), "nor for a sprite of another size: " + resized.reason);
        CHECK_MSG(fs::is_regular_file(first.path), "and the cached file is left where it was");
    }
    // A file with the right length and the right last chunk but a wrong header is not whole either: one bit of the signature, of the width and of the height.
    const auto readWhole = [](const std::string& path) {
        std::ifstream in(path, std::ios::binary);
        return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    };
    for (const auto& [offset, what] : std::vector<std::pair<std::size_t, std::string>>{{0, "signature"}, {19, "width"}, {23, "height"}}) {
        std::string held = readWhole(first.path);
        CHECK(held.size() > 40);
        if (held.size() <= 40) continue;
        held[offset] = static_cast<char>(held[offset] ^ 1);
        WriteText(first.path, held);
        const PB::Written healed = PB::Ensure(wanted("TAP START", out));
        CHECK_MSG(healed.ok && !healed.cached, "a file with one bit of its " + what + " wrong is rendered again: " + healed.reason);
        PB::Image check;
        CHECK_MSG(PB::ReadImage(first.path, check, why) && check.rgba == baked.rgba, "and is whole again (" + what + ")");
    }
    // A failed write leaves nothing: a folder that cannot be made (a file is in its way), and a target that cannot be put in place (a folder is there).
    {
        const fs::path blocker = root / "blocker";
        WriteText(blocker, "x");
        const PB::Written w = PB::Ensure(wanted("TAP START", blocker / "inside"));
        CHECK_MSG(!w.ok && !w.reason.empty() && Contains(w.reason, "cannot make"), "a folder that cannot be made fails closed: " + w.reason);
        CHECK_MSG(fs::is_regular_file(blocker) && !Exists(blocker / "inside"), "and nothing is left");
    }
    {
        const fs::path cage = root / "cage";
        fs::create_directories(cage);
        const PB::Written probe = PB::Ensure(wanted("TAP START", cage));
        CHECK(probe.ok);
        std::error_code ec;
        fs::remove(probe.path, ec);
        fs::create_directories(fs::path(probe.path) / "in-the-way");
        const PB::Written w = PB::Ensure(wanted("TAP START", cage));
        CHECK_MSG(!w.ok && !w.reason.empty(), "a target that cannot be replaced fails closed: " + w.reason);
        bool temporary = false;
        for (const std::string& file : FilesIn(cage)) temporary = temporary || (file.size() > 4 && file.substr(file.size() - 4) == ".tmp");
        CHECK_MSG(!temporary, "and no temporary file is left behind");
    }
    // A wrong picture on disk fails closed before anything is written.
    {
        PB::Image wrong = plate.truth; // a plate with no word: same size, another digest
        std::string wrongBytes;
        CHECK(PB::EncodePng(wrong, wrongBytes));
        const fs::path wrongSources = root / "wrong";
        fs::create_directories(wrongSources);
        WriteText(wrongSources / plate.recipe.sprite, wrongBytes);
        PB::Wanted w = wanted("TAP START", root / "wrong-out");
        w.sourcePath = (wrongSources / plate.recipe.sprite).string();
        const PB::Written written = PB::Ensure(w);
        CHECK_MSG(!written.ok && Contains(written.reason, "digest") && !Exists(root / "wrong-out"), "another version of the picture writes no file: " + written.reason);
        PB::Wanted missing = wanted("TAP START", root / "missing-out");
        missing.sourcePath = (root / "no-such.png").string();
        CHECK_MSG(!PB::Ensure(missing).ok && !Exists(root / "missing-out"), "a picture that is not there writes no file");
        PB::Wanted nowhere = wanted("TAP START", std::string());
        CHECK_MSG(!PB::Ensure(nowhere).ok, "no scratch folder is a failure");
        PB::Wanted noFaces = wanted("TAP START", root / "no-faces-out");
        noFaces.fontsDir = (root / "no-fonts").string();
        CHECK_MSG(!PB::Ensure(noFaces).ok && !Exists(root / "no-faces-out"), "faces that cannot be read write no file");
        PB::Wanted nothing;
        CHECK_MSG(!PB::Ensure(nothing).ok, "a request with nothing in it");
    }
}

// ---- the shipped words ----------------------------------------------------------------------------------------------------------------------

void EveryShippedPhraseIsDrawnFromGlyphsItsFacesHold() {
    const World& world = TheWorld();
    if (!world.ok) return;
    int phrases = 0;
    for (std::size_t i = 1; i < Strings::kLangCount; ++i) {
        const Lang lang = static_cast<Lang>(i);
        Strings::Table table;
        std::string error;
        CHECK_MSG(table.Load(kData, lang, error), error);
        for (const PB::Recipe& recipe : world.recipes.items) {
            ++phrases;
            for (const std::string& key : recipe.Keys()) CHECK_MSG(std::find(table.FellBack().begin(), table.FellBack().end(), key) == table.FellBack().end(), Code(lang) + " has " + key);
            const std::string single = recipe.key.empty() ? std::string() : table.Get(recipe.key);
            PB::Request request = RequestFor(recipe, lang, single);
            for (const PB::Band& band : recipe.text.bands) request.phrases.push_back(table.Get(band.key));
            const PB::Outcome outcome = PB::FitPhrase(request);
            CHECK_MSG(!Contains(outcome.reason, "no glyph") && !Contains(outcome.reason, "no face"), Code(lang) + " " + recipe.id + ": every code point is in its face: " + outcome.reason);
            CHECK_MSG(outcome.ok, Code(lang) + " " + recipe.id + ": the phrase is drawn: " + outcome.reason);
        }
    }
    CHECK_EQ(phrases, 110);
    // The check would notice: an uncovered character in a real phrase is named.
    Strings::Table german;
    std::string error;
    CHECK(german.Load(kData, Lang::De, error));
    const PB::Recipe* tap = world.recipes.Find("main_play_game_button.png");
    CHECK(tap != nullptr);
    if (tap != nullptr) {
        const PB::Outcome bad = PB::FitPhrase(RequestFor(*tap, Lang::De, german.Get(Strings::Key::kPictureTapStart) + Cp({0x0E01})));
        CHECK_MSG(!bad.ok && Contains(bad.reason, "no glyph for U+0E01"), "an edit that adds an uncovered character is caught: " + bad.reason);
    }
}

// ---- where the pictures are named ------------------------------------------------------------------------------------------------------------

// Every string of a JSON value that names one of `names` (with or without the "sprites/" folder), with the member path it sits under.
void CollectNames(const Supersonic::Json::Value& value, const std::string& path, const std::set<std::string>& names, std::vector<std::pair<std::string, std::string>>& found) {
    if (value.IsObject()) {
        for (const auto& [name, child] : value.AsObject()) CollectNames(child, path + "/" + name, names, found);
    } else if (value.IsArray()) {
        for (std::size_t i = 0; i < value.AsArray().size(); ++i) CollectNames(value.AsArray()[i], path + "[" + std::to_string(i) + "]", names, found);
    } else if (value.IsString()) {
        std::string file = value.AsString();
        const std::string folder = "sprites/";
        if (file.compare(0, folder.size(), folder) == 0) file = file.substr(folder.size());
        if (names.count(file) != 0) found.emplace_back(path, value.AsString());
    }
}

// THE CACHE AUDIT, as a test. The layer remakes, after a language switch, the caches of the screens that draw the seven pictures (the menus', the pause's, the end
// screens' and the popups'); a picture another cache holds - the HUD's pads and plaque, the achievement cards - would stay in the old language. None does today, and this
// is how a later edit that names one of the seven pictures somewhere else is caught: it fails here, and the author adds the cache to the switch.
void TheWordedSpritesAreNamedOnlyWhereALanguageSwitchRemakesThem() { // (planning step 247; step 248 added the HUD's plaque, the pause's and the end screen's plaques, the card and the strip)
    const World& world = TheWorld();
    if (!world.ok) return;
    std::set<std::string> names;
    for (const PB::Recipe& recipe : world.recipes.items) names.insert(recipe.sprite);
    // The level HUD's plaque (m_plaqueImage) and the credits' strip (m_menuPictures) joined them at step 248.
    const std::vector<std::string> remade = {"/main_menu/", "/selector/", "/dashboard/", "/pause/", "/level_end/", "/popups/", "/level_start/", "/credits/"};
    int named = 0;
    int tables = 0;
    for (const std::string& directory : {kData, std::string(MAGICPORTALS_DATA_DIR)}) {
        std::error_code ec;
        if (directory.empty() || !fs::is_directory(directory, ec)) continue;
        for (fs::directory_iterator it(directory, ec), end; !ec && it != end; it.increment(ec)) {
            if (it->path().extension() != ".json" || it->path().filename() == "picture_recipes.json") continue;
            std::ifstream in(it->path(), std::ios::binary);
            const std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            Supersonic::Json::Parser parser(content);
            Supersonic::Json::Value root;
            if (!parser.Parse(root)) continue; // a table of another kind of file is not this test's to judge
            ++tables;
            std::vector<std::pair<std::string, std::string>> found;
            CollectNames(root, "", names, found);
            for (const auto& [path, file] : found) {
                ++named;
                bool reached = false;
                for (const std::string& screen : remade) reached = reached || path.compare(0, screen.size(), screen) == 0;
                CHECK_MSG(it->path().filename() == "ui.json" && reached, it->path().filename().string() + " names " + file + " at " + path + ", where a language switch does not remake the picture's path");
            }
        }
    }
    CHECK_MSG(tables > 20, "the tables were read: " + std::to_string(tables));
    CHECK_MSG(named == 14, "the eleven pictures are named in fourteen places of the screens' rules (the Achievements picture twice, the current plaque twice, the golden plaque twice): " + std::to_string(named));
}

// ============================================================================================================================
// planning step 248: plaques, the panel, the strip - synthetic fixtures, hand-authored in code (no original pixel, no extracted file)
// ============================================================================================================================

constexpr int kPlaqueW = 112;
constexpr int kPlaqueH = 160;

// A plate with a straight frame: frame columns [2, 10) and [98, 106) are constant down the plate, the interior [10, 98) is V(y) H(x) per channel, the rows outside [4, 156)
// and the columns outside [2, 106) are transparent.
double PlaqueV(int y) {
    return 100.0 - 0.4 * y;
}

double PlaqueH(int x) {
    const int d = std::min(x - 10, 97 - x);
    return d >= 10 ? 1.0 : 0.75 + 0.25 * d / 10.0;
}

double PlaqueF(int x) {
    const int d = x < 10 ? x - 2 : 105 - x; // 0 outside to 7 inside
    return 140.0 - 12.0 * d + (x >= 98 ? 6.0 : 0.0);
}

void PutPlate(PB::Image& image, int x, int y, double value) {
    Put(image, x, y, value, value * 0.97, value * 0.95, 255); // neutral: the channels differ by less than the chroma threshold (10)
}

struct PlaqueFixture {
    PB::Image truth;    // the plate with no lettering and no medal
    PB::Image picture;  // with two lines of lettering over the frame, and (golden) a medal
    PB::Recipe recipe;
};

// A medal: an orange disc far from the centre columns and a small ribbon inside the first rectangle, both of chroma 60.
void AddMedal(PB::Image& image) {
    for (int y = 36; y < 53; ++y) {
        for (int x = 12; x < 29; ++x) {
            if ((x - 20) * (x - 20) + (y - 44) * (y - 44) <= 64) Put(image, x, y, 220, 160, 100, 255);
        }
    }
    for (int y = 81; y < 84; ++y) {
        for (int x = 50; x < 54; ++x) Put(image, x, y, 220, 160, 100, 255);
    }
}

PlaqueFixture MakePlaque(bool medal) {
    PlaqueFixture f;
    f.truth = PB::Blank(kPlaqueW, kPlaqueH);
    for (int y = 4; y < 156; ++y) {
        for (int x = 2; x < 106; ++x) {
            const bool frame = x < 10 || x >= 98;
            PutPlate(f.truth, x, y, frame ? PlaqueF(x) : PlaqueV(y) * PlaqueH(x));
        }
    }
    PB::Image lettered = WithAWord(f.truth, PB::Rect{8, 86, 100, 97}, 1, 1.0);
    lettered = WithAWord(lettered, PB::Rect{28, 102, 76, 112}, 1, 1.0);
    f.picture = lettered;
    if (medal) AddMedal(f.picture);
    PB::Recipe& r = f.recipe;
    r.id = medal ? "syn_golden" : "syn_current";
    r.key = std::string(Strings::Key::kPictureCurrentScore);
    r.sprite = r.id + ".png";
    r.width = kPlaqueW;
    r.height = kPlaqueH;
    r.digest = PB::Digest(f.picture);
    PB::Erase& e = r.erase;
    e.rule = PB::Rule::Plaque;
    e.interiorCols = PB::Span{10, 98};
    e.frameCols = {PB::Span{2, 10}, PB::Span{98, 106}};
    e.centerCols = PB::Span{34, 74};
    e.refRows = {PB::Span{20, 76}};
    e.vTopRows = PB::Span{20, 76};
    e.vMarginRows = PB::Span{104, 136}; // below the first line's halo (the margin columns are under the first line's ink, as the original's are)
    e.refCols = {PB::Span{11, 14}, PB::Span{94, 97}};
    e.agreeTol = 2.0;
    e.rects = {PB::Rect{2, 80, 106, 125}, PB::Rect{12, 125, 96, 140}};
    e.rect = PB::Rect{2, 80, 106, 140};
    e.chroma.enabled = true;
    e.chroma.threshold = 10;
    e.chroma.radius = 2;
    e.chroma.keepMin = medal ? 1 : 0;
    e.chroma.keepMax = medal ? 100000 : 0;
    if (medal) {
        e.hasSkip = true;
        e.skipCols = PB::Span{10, 33};
    }
    e.plaque.sepMean = 0.8;
    e.plaque.sepMax = 3.0;
    e.plaque.holdout = 2.5;
    e.plaque.marginWindow = PB::Span{110, 132};
    e.plaque.marginRatio = 0.7;
    e.plaque.ringMax = 4.0;
    e.plaque.slopeBand = 0.3;
    PB::Text& t = r.text;
    t.layout = PB::Layout::Paragraph;
    t.slot = "matura";
    t.px = 12.0;
    t.faces = {PB::FaceTuning{"kurale", 1.0, 0, 0.03, 1.4}, PB::FaceTuning{"noto_sans_jp_bold", 1.0, 0, 0.03, 1.3}, PB::FaceTuning{"noto_sans_arabic_bold", 1.0, 0, 0.03, 1.5}};
    t.anchorX = 56.0;
    t.anchorY = 107.0;
    t.minScale = 0.4;
    t.step = 0.97;
    t.inkThreshold = 0.15;
    t.maxWidth = 84.0;
    t.gap = 0.05;
    t.maxLines = 2;
    t.safe = PB::Rect{14, 88, 98, 126};
    t.shadow.dy = 0.08;
    t.shadow.sigmaMin = 1.0;
    t.shadow.gain = 1.8;
    t.shadow.cap = 0.9;
    t.shadow.roundOffsets = true;
    return f;
}

bool IsKept(const PB::Image& image, int x, int y, int radius, int threshold) {
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            const int sx = x + dx;
            const int sy = y + dy;
            if (sx < 0 || sy < 0 || sx >= image.width || sy >= image.height) continue;
            const unsigned char* px = image.At(sx, sy);
            if (px[3] > 0 && std::max({px[0], px[1], px[2]}) - std::min({px[0], px[1], px[2]}) > threshold) return true;
        }
    }
    return false;
}

bool InAny(const std::vector<PB::Rect>& rects, int x, int y) {
    for (const PB::Rect& r : rects) {
        if (r.Contains(x, y)) return true;
    }
    return false;
}

int ChangedOutsideRects(const PB::Image& a, const PB::Image& b, const std::vector<PB::Rect>& rects) {
    int changed = 0;
    for (int y = 0; y < a.height; ++y) {
        for (int x = 0; x < a.width; ++x) {
            if (!InAny(rects, x, y) && !std::equal(a.At(x, y), a.At(x, y) + 4, b.At(x, y))) ++changed;
        }
    }
    return changed;
}

void APlaqueIsErasedByItsOwnModel() {
    for (const bool medal : {false, true}) {
        const PlaqueFixture f = MakePlaque(medal);
        const std::string name = medal ? "golden-like" : "current-like";
        PB::Image out;
        std::string why;
        const bool erasedOk = PB::EraseWords(f.recipe, f.picture, nullptr, out, why);
        CHECK_MSG(erasedOk, name + " plaque is erased: " + why);
        if (out.Empty()) continue;
        CHECK_MSG(MostDifference(f.picture, f.truth, PB::Rect{2, 84, 106, 116}) > 100, name + ": the synthetic lettering is really on the plate");
        // Inside the rectangles the plate is back to a stated tolerance (2 grey levels), frame columns included: the lettering covered the frame's bevel at both ends.
        int worst = 0;
        int kept = 0;
        int keptChanged = 0;
        for (const PB::Rect& r : f.recipe.erase.rects) {
            for (int y = r.y0; y < r.y1; ++y) {
                for (int x = r.x0; x < r.x1; ++x) {
                    if (IsKept(f.picture, x, y, 2, 10)) {
                        ++kept;
                        keptChanged += std::equal(out.At(x, y), out.At(x, y) + 4, f.picture.At(x, y)) ? 0 : 1;
                        continue;
                    }
                    for (int c = 0; c < 3; ++c) worst = std::max(worst, std::abs(static_cast<int>(out.At(x, y)[c]) - static_cast<int>(f.truth.At(x, y)[c])));
                }
            }
        }
        CHECK_MSG(worst <= 2, name + ": the erase is the plate to 2 grey levels, the frame's ends too: " + std::to_string(worst));
        // The medal: the chroma rule keeps every chromatic pixel and a ring of 2 px round it, bit for bit; the pixels three away are replaced.
        if (medal) CHECK_MSG(kept > 20 && keptChanged == 0, "the ribbon and its ring of two pixels are untouched: " + std::to_string(kept) + " kept, " + std::to_string(keptChanged) + " changed");
        else CHECK_EQ(kept, 0);
        CHECK_MSG(ChangedOutsideRects(out, f.picture, f.recipe.erase.rects) == 0, name + ": nothing outside the rectangles changes (the corners of the lower band too)");
        int alphaChanged = 0;
        for (int y = 0; y < out.height; ++y) {
            for (int x = 0; x < out.width; ++x) alphaChanged += out.At(x, y)[3] != f.picture.At(x, y)[3] ? 1 : 0;
        }
        CHECK_EQ(alphaChanged, 0);
        // The ring is two pixels, not three: the pixel three beyond the ribbon (x 50..53, y 81..83) is the model's, and so is the one at the same place on the lettered plate.
        if (medal) CHECK_MSG(MostDifference(out, f.truth, PB::Rect{57, 82, 58, 83}) <= 2, "a pixel three beyond the ribbon is the model's");
    }
}

// Every negative fixture trips ONE guard and the reason NAMES it: a second guard would hide a mutant of the first.
void ThePlaqueGuardsFailClosedEachByItself() {
    PB::Image out;
    std::string why;
    // golden art under the current recipe: refused, and by the separability of the clean rows before the chroma count is even looked at (the medal is not a plate) ...
    {
        const PlaqueFixture golden = MakePlaque(true);
        const PlaqueFixture current = MakePlaque(false);
        {
        const bool erased = PB::EraseWords(current.recipe, golden.picture, nullptr, out, why);
        CHECK_MSG(!erased && Contains(why, "separability") && out.Empty(), "golden art under the current recipe: " + why);
    }
        // ... and, with the medal's columns left out of the separability check, by the chroma rule alone
        PlaqueFixture skipping = MakePlaque(false);
        skipping.recipe.erase.hasSkip = true;
        skipping.recipe.erase.skipCols = PB::Span{10, 33};
        {
        const bool erased = PB::EraseWords(skipping.recipe, golden.picture, nullptr, out, why);
        CHECK_MSG(!erased && Contains(why, "chroma"), "golden art under a current recipe that skips the medal's columns: " + why);
    }
    }
    // current art under the golden recipe: no medal where the recipe needs one
    {
        const PlaqueFixture current = MakePlaque(false);
        PlaqueFixture golden = MakePlaque(true);
        {
        const bool erased = PB::EraseWords(golden.recipe, current.picture, nullptr, out, why);
        CHECK_MSG(!erased && Contains(why, "chroma"), "current art under the golden recipe: " + why);
    }
    }
    // stray ink in EACH of the three rows above the rectangle (77, 78 and 79: the rectangle begins at 80, the clean rows end at 75): a check of one row only would let two of them through
    for (const int row : {77, 78, 79}) {
        PlaqueFixture f = MakePlaque(false);
        for (int x = 30; x < 70; ++x) Put(f.picture, x, row, 235, 235, 235, 255);
        const bool erased = PB::EraseWords(f.recipe, f.picture, nullptr, out, why);
        CHECK_MSG(!erased && Contains(why, "ring"), "stray ink in row " + std::to_string(row) + " above the rectangle: " + why);
    }
    // ink in the margin columns of the rows beside the lettering
    {
        PlaqueFixture f = MakePlaque(false);
        for (int y = 112; y < 132; ++y) {
            for (int x = 11; x < 14; ++x) Put(f.picture, x, y, 240, 240, 240, 255);
        }
        {
        const bool erased = PB::EraseWords(f.recipe, f.picture, nullptr, out, why);
        CHECK_MSG(!erased && Contains(why, "margin rows"), "ink in the margin columns: " + why);
    }
    }
    // a plate that is not a profile times a vignette: a tilt across the clean rows
    {
        PlaqueFixture f = MakePlaque(false);
        for (int y = 20; y < 76; ++y) {
            for (int x = 10; x < 98; ++x) {
                for (int c = 0; c < 3; ++c) f.picture.At(x, y)[c] = static_cast<unsigned char>(std::clamp(f.picture.At(x, y)[c] * (1.0 + 0.0003 * (x - 54) * (y - 48)), 0.0, 255.0));
            }
        }
        {
        const bool erased = PB::EraseWords(f.recipe, f.picture, nullptr, out, why);
        CHECK_MSG(!erased && Contains(why, "separability"), "a tilted plate: " + why);
    }
    }
    // every other clean row jitters: each row is still a profile times a vignette, but V is not smooth
    {
        PlaqueFixture f = MakePlaque(false);
        for (int y = 21; y < 76; y += 2) {
            for (int x = 10; x < 98; ++x) {
                for (int c = 0; c < 3; ++c) f.picture.At(x, y)[c] = static_cast<unsigned char>(std::clamp(f.picture.At(x, y)[c] * 1.12, 0.0, 255.0));
            }
        }
        {
        const bool erased = PB::EraseWords(f.recipe, f.picture, nullptr, out, why);
        CHECK_MSG(!erased && Contains(why, "hold-out"), "a jittering profile: " + why);
    }
    }
    // the profile bends where nothing was read: the rows below the old lettering are darker than the clean rows' line says
    {
        PlaqueFixture f = MakePlaque(false);
        for (int y = 98; y < 156; ++y) {
            for (int x = 2; x < 106; ++x) {
                if (f.picture.At(x, y)[3] == 0) continue;
                if (std::equal(f.picture.At(x, y), f.picture.At(x, y) + 4, f.truth.At(x, y)) == false) continue; // a lettered pixel is not touched
                for (int c = 0; c < 3; ++c) f.picture.At(x, y)[c] = static_cast<unsigned char>(std::clamp(f.picture.At(x, y)[c] - 14.0 * (x >= 10 && x < 98 ? 1.0 : 0.0), 0.0, 255.0));
            }
        }
        {
        const bool erased = PB::EraseWords(f.recipe, f.picture, nullptr, out, why);
        CHECK_MSG(!erased && Contains(why, "slope"), "a profile that bends in the hole: " + why);
    }
    }
    // and the same plaque with its slope check off passes (the guard above is what refused it)
    {
        PlaqueFixture f = MakePlaque(false);
        for (int y = 98; y < 156; ++y) {
            for (int x = 10; x < 98; ++x) {
                if (f.picture.At(x, y)[3] == 0 || !std::equal(f.picture.At(x, y), f.picture.At(x, y) + 4, f.truth.At(x, y))) continue;
                for (int c = 0; c < 3; ++c) f.picture.At(x, y)[c] = static_cast<unsigned char>(std::clamp(f.picture.At(x, y)[c] - 14.0, 0.0, 255.0));
            }
        }
        f.recipe.erase.plaque.slopeBand = 0.0;
        CHECK_MSG(PB::EraseWords(f.recipe, f.picture, nullptr, out, why), "with the slope check off the same plaque is erased: " + why);
    }
    // the digest and the size are the source check's, and a recipe asked on another size says so
    {
        const PlaqueFixture f = MakePlaque(false);
        PB::Image small = PB::Blank(60, 60);
        {
        const bool erased = PB::EraseWords(f.recipe, small, nullptr, out, why);
        CHECK_MSG(!erased && Contains(why, "recipe's size"), "another size: " + why);
    }
    }
}

struct PanelFixture {
    PB::Image truth;
    PB::Image picture;
    PB::Recipe recipe;
};

double PanelV(int y) {
    return 50.0 + 0.5 * y;
}

double PanelH(int x) {
    const int d = std::min(x - 10, 89 - x);
    return 0.8 + 0.2 * std::min(d, 8) / 8.0;
}

PanelFixture MakePanel() {
    PanelFixture f;
    f.truth = PB::Blank(100, 120);
    for (int y = 0; y < 120; ++y) {
        for (int x = 0; x < 100; ++x) {
            const bool interior = x >= 10 && x < 90;
            double v = interior ? PanelV(y) * PanelH(x) : 18.0;
            if (interior && x >= 86 && y >= 40 && y < 60) v *= 1.2; // the corner effect of the right margin, in rows the lettering is beside
            Put(f.truth, x, y, v * 0.8, v * 0.6, v, 255);
        }
    }
    PB::Image lettered = WithAWord(f.truth, PB::Rect{18, 44, 82, 54}, 2, 1.5);
    lettered = WithAWord(lettered, PB::Rect{24, 82, 76, 92}, 2, 1.5);
    f.picture = lettered;
    PB::Recipe& r = f.recipe;
    r.id = "syn_panel";
    r.key = std::string(Strings::Key::kPictureGameFinished);
    r.sprite = "syn_panel.png";
    r.width = 100;
    r.height = 120;
    r.digest = PB::Digest(f.picture);
    PB::Erase& e = r.erase;
    e.rule = PB::Rule::Panel;
    e.rect = PB::Rect{14, 40, 86, 100};
    e.interiorCols = PB::Span{10, 90};
    e.centerCols = PB::Span{30, 70};
    e.refRows = {PB::Span{64, 80}};
    e.refCols = {PB::Span{11, 14}, PB::Span{86, 89}};
    e.agreeTol = 3.0;
    e.panel.sepMax = 2.0;
    e.panel.vMargin = 2.5;
    e.panel.agreeRatio = 0.6;
    e.panel.ringMax = 4.0;
    PB::Text& t = r.text;
    t.layout = PB::Layout::Paragraph;
    t.slot = "verdana";
    t.px = 10.0;
    t.faces = {PB::FaceTuning{"noto_sans_bold", 1.0, 0, 0.025, 1.32}, PB::FaceTuning{"noto_sans_jp_bold", 1.0, 0, 0.025, 1.3}, PB::FaceTuning{"noto_sans_arabic_bold", 1.0, 0, 0.025, 1.5}};
    t.anchorX = 50.0;
    t.anchorY = 70.0;
    t.minScale = 0.5;
    t.step = 0.97;
    t.inkThreshold = 0.15;
    t.maxWidth = 60.0;
    t.gap = 0.04;
    t.maxLines = 4;
    t.orphan = 0.4;
    t.saveLines = 0.0;
    t.safe = PB::Rect{20, 46, 80, 94};
    t.shadow.dy = 0.1;
    t.shadow.sigmaMin = 1.5;
    t.shadow.gain = 1.8;
    t.shadow.cap = 0.9;
    t.shadow.roundOffsets = true;
    return f;
}

void APanelIsErasedFromItsLeftMarginAlone() {
    const PanelFixture f = MakePanel();
    PB::Image out;
    std::string why;
    CHECK_MSG(PB::EraseWords(f.recipe, f.picture, nullptr, out, why), "the panel is erased: " + why);
    if (out.Empty()) return;
    CHECK_MSG(MostDifference(f.picture, f.truth, f.recipe.erase.rect) > 100, "the synthetic lettering is really on the panel");
    double mean = 0.0;
    const int worst = MostDifference(out, f.truth, f.recipe.erase.rect, &mean);
    // The right margin's corner effect (20 percent for 20 rows) would have lifted the model there by 10 levels or more: only the left margin is read, and the result is the plate's.
    CHECK_MSG(worst <= 2, "the panel is back to 2 grey levels, with a corner effect on the right margin the model does not read: " + std::to_string(worst));
    CHECK_EQ(PixelsChangedOutside(out, f.picture, f.recipe.erase.rect), 0);
    // Each guard by itself.
    {
        PanelFixture bad = MakePanel();
        for (int y = 64; y < 80; ++y) {
            for (int x = 11; x < 14; ++x) Put(bad.picture, x, y, 240, 240, 240, 255);
        }
        {
        const bool erased = PB::EraseWords(bad.recipe, bad.picture, nullptr, out, why);
        CHECK_MSG(!erased && Contains(why, "left margin's profile"), "ink in the left margin: " + why);
    }
    }
    // stray ink in EACH of the three rows above (37, 38, 39: the rectangle begins at 40) and below (100, 101, 102: it ends at 100) the rectangle
    for (const int row : {37, 38, 39}) {
        PanelFixture bad = MakePanel();
        for (int x = 30; x < 70; ++x) Put(bad.picture, x, row, 235, 235, 235, 255);
        const bool erased = PB::EraseWords(bad.recipe, bad.picture, nullptr, out, why);
        CHECK_MSG(!erased && Contains(why, "ring"), "stray ink in row " + std::to_string(row) + " above the rectangle: " + why);
    }
    for (const int row : {100, 101, 102}) {
        PanelFixture bad = MakePanel();
        for (int x = 30; x < 70; ++x) Put(bad.picture, x, row, 235, 235, 235, 255);
        const bool erased = PB::EraseWords(bad.recipe, bad.picture, nullptr, out, why);
        CHECK_MSG(!erased && Contains(why, "ring"), "stray ink in row " + std::to_string(row) + " below the rectangle: " + why);
    }
    {
        // not a profile times a vignette, in the columns between the margins and the centre only, so that the margins and the centre still agree with each other
        PanelFixture bad = MakePanel();
        for (int y = 64; y < 80; ++y) {
            for (int x = 16; x < 28; ++x) {
                for (int c = 0; c < 3; ++c) bad.picture.At(x, y)[c] = static_cast<unsigned char>(std::clamp(bad.picture.At(x, y)[c] * (1.0 + 0.012 * (y - 64)), 0.0, 255.0));
            }
        }
        {
        const bool erased = PB::EraseWords(bad.recipe, bad.picture, nullptr, out, why);
        CHECK_MSG(!erased && Contains(why, "separability"), "a panel that is not a profile times a vignette: " + why);
    }
    }
    {
        PanelFixture bad = MakePanel();
        bad.recipe.erase.panel.agreeRatio = 0.9; // the corner effect makes 20 of 60 rows disagree
        {
        const bool erased = PB::EraseWords(bad.recipe, bad.picture, nullptr, out, why);
        CHECK_MSG(!erased && Contains(why, "margin rows"), "margins that disagree on too many rows: " + why);
    }
    }
}

// ---- the strip -----------------------------------------------------------------------------------------------------------------------------------

struct StripFixture {
    PB::Image picture;
    PB::Recipe recipe;
};

StripFixture MakeStrip() {
    StripFixture f;
    f.picture = PB::Blank(160, 240);
    const auto bar = [&](int x0, int y0, int x1, int y1, double r, double g, double b) {
        for (int y = y0; y < y1; ++y) {
            for (int x = x0; x < x1; ++x) Put(f.picture, x, y, r, g, b, 255);
        }
    };
    bar(12, 20, 120, 30, 0, 0, 0);    // a role
    bar(12, 90, 110, 100, 0, 0, 0);   // another
    bar(10, 52, 100, 64, 40, 40, 160); // a name between them
    bar(10, 130, 100, 150, 40, 40, 160);
    bar(20, 200, 120, 232, 200, 120, 30); // a logo
    PB::Recipe& r = f.recipe;
    r.id = "syn_strip";
    r.sprite = "syn_strip.png";
    r.width = 160;
    r.height = 240;
    r.digest = PB::Digest(f.picture);
    r.erase.rule = PB::Rule::ClearRows;
    r.erase.clearRows = {PB::Span{19, 31}, PB::Span{89, 101}};
    r.erase.rect = PB::Rect{0, 12, 160, 118};
    PB::Text& t = r.text;
    t.layout = PB::Layout::Bands;
    t.slot = "matura";
    t.faces = {PB::FaceTuning{"kurale", 1.0, 0x78, 0.065}, PB::FaceTuning{"noto_sans_jp_bold", 1.25, 0, 0.04}, PB::FaceTuning{"noto_sans_arabic_bold", 1.3, 0, 0.03}};
    t.minScale = 0.7;
    t.step = 0.97;
    t.inkThreshold = 0.02;
    t.gap = 0.05;
    t.left = 12.0;
    t.maxRight = 150.0;
    t.inkLevel = 0;
    t.japaneseSpaces = true;
    PB::Band a;
    a.key = std::string(Strings::Key::kCreditsRoleDesign);
    a.px = 6.0;
    a.firstBaseline = 28.0;
    a.pitch = 13.0;
    a.maxLines = 2;
    a.inkRows = PB::Span{12, 48};
    a.breakAt = PB::BreakAt::AfterDash;
    PB::Band b = a;
    b.key = std::string(Strings::Key::kCreditsRoleModelling);
    b.firstBaseline = 98.0;
    b.maxLines = 1;
    b.inkRows = PB::Span{82, 118};
    t.bands = {a, b};
    return f;
}

PB::Request RequestForBands(const PB::Recipe& recipe, Lang lang, const std::vector<std::string>& phrases) {
    PB::Request request;
    request.recipe = &recipe;
    request.plan = &TheWorld().languages.Fonts();
    request.faces = &TheWorld().faces;
    request.lang = lang;
    request.phrases = phrases;
    return request;
}

void AStripClearsItsBandsAndDrawsThemAgainLeftAligned() {
    const World& world = TheWorld();
    if (!world.ok) return;
    const StripFixture f = MakeStrip();
    const PB::Outcome o = PB::Render(RequestForBands(f.recipe, Lang::En, {"ab - cd", "ef"}), f.picture, nullptr);
    CHECK_MSG(o.ok, "the strip is baked: " + o.reason);
    if (!o.ok) return;
    CHECK_MSG(o.bands.size() == 2 && o.bands[0].key == f.recipe.text.bands[0].key && o.bands[1].lines == 1, "each band says what it came to");
    // Outside the ink windows and the cleared rows every pixel is the source's: the names and the logo are untouched BY CONSTRUCTION.
    int outside = 0;
    int oldBarLeft = 0;
    int notBlack = 0;
    int inked = 0;
    for (int y = 0; y < f.picture.height; ++y) {
        for (int x = 0; x < f.picture.width; ++x) {
            const bool window = (y >= 12 && y < 48) || (y >= 82 && y < 118);
            const unsigned char* px = o.image.At(x, y);
            if (!window) {
                outside += std::equal(px, px + 4, f.picture.At(x, y)) ? 0 : 1;
                continue;
            }
            if (px[3] > 0) {
                ++inked;
                notBlack += (px[0] != 0 || px[1] != 0 || px[2] != 0) ? 1 : 0;
            }
            // the old bars' far ends (x 100..119 of the first, 100..109 of the second) are gone: the whole rows were cleared
            if (((y >= 20 && y < 30 && x >= 100 && x < 120) || (y >= 90 && y < 100 && x >= 100 && x < 110)) && px[3] > 0) ++oldBarLeft;
        }
    }
    CHECK_EQ(outside, 0);
    CHECK_MSG(inked > 80 && notBlack == 0, "the new ink is black (rgb 0, alpha = coverage): " + std::to_string(inked) + " inked, " + std::to_string(notBlack) + " not black");
    CHECK_MSG(oldBarLeft == 0, "the cleared rows hold none of the old words: " + std::to_string(oldBarLeft));
    // Left-aligned on x 12 by the ink's edge, inside its rows and its right edge.
    for (std::size_t i = 0; i < o.bands.size(); ++i) {
        const PB::Rect ink = o.bands[i].ink;
        CHECK_MSG(ink.x0 >= 12 && ink.x0 <= 13, "band " + std::to_string(i) + " begins at the left edge: " + std::to_string(ink.x0));
        CHECK_MSG(ink.x1 <= 150 && ink.y0 >= f.recipe.text.bands[i].inkRows.a && ink.y1 <= f.recipe.text.bands[i].inkRows.b, "band " + std::to_string(i) + " stays in its window");
    }
    // A long phrase wraps AFTER a dash (a dash ends a line and never starts one), at the largest scale that does, the fewest lines.
    PB::Recipe narrow = f.recipe;
    narrow.text.maxRight = 142.0;
    narrow.text.bands[0].maxLines = 2;
    // Cut at any space the better balance would be 'aaaa bbbb - cc' over 'dd - eeee ffff'; cut only after a dash the first line is 'aaaa bbbb -'.
    const PB::Outcome wrapped = PB::FitPhrase(RequestForBands(narrow, Lang::En, {"aaaa bbbb - cc dd - eeee ffff", "x"}));
    CHECK_MSG(wrapped.ok && wrapped.bands[0].lines == 2, "a long phrase wraps in two lines: " + wrapped.reason);
    if (wrapped.ok && wrapped.bands[0].lines == 2) {
        const std::vector<std::string>& lines = wrapped.bands[0].lineTexts;
        CHECK_MSG(lines[0] == "aaaa bbbb -" && lines[1] == "cc dd - eeee ffff", "it breaks after a dash that stands alone: [" + lines[0] + "][" + lines[1] + "]");
    }
    // The band's line limit: one line, a phrase too long for it, a floor to stop at.
    PB::Recipe one = f.recipe;
    one.text.bands[0].maxLines = 1;
    const PB::Outcome refused = PB::FitPhrase(RequestForBands(one, Lang::En, {"aaaaaaaaa bbbbbbbbb ccccccccc ddddddddd eeeeeeeee fffffffff ggggggggg hhhhhhhhh iiiiiiiii jjjjjjjjj", "x"}));
    CHECK_MSG(!refused.ok && Contains(refused.reason, std::string(Strings::Key::kCreditsRoleDesign)) && Contains(refused.reason, "floor"), "a phrase too long for its band's lines fails closed, naming the band: " + refused.reason);
    // A descender slides the block up rather than past the window's last row; a word without one stays on its baseline.
    PB::Recipe low = f.recipe;
    low.text.bands[0].firstBaseline = 46.0;
    low.text.bands[0].inkRows = PB::Span{12, 47};
    const PB::Outcome slid = PB::FitPhrase(RequestForBands(low, Lang::En, {"gggg", "x"}));
    const PB::Outcome stayed = PB::FitPhrase(RequestForBands(low, Lang::En, {"MMMM", "x"}));
    CHECK_MSG(slid.ok && stayed.ok, "both fit: " + slid.reason + stayed.reason);
    if (slid.ok && stayed.ok) {
        CHECK_MSG(slid.bands[0].ink.y1 <= 47 && slid.placed[0].baseline < 46.0 - 0.5, "descenders slide the block up: baseline " + std::to_string(slid.placed[0].baseline));
        CHECK_MSG(std::fabs(stayed.placed[0].baseline - 46.0) < 0.01, "and a word with none stays: " + std::to_string(stayed.placed[0].baseline));
    }
    // Digits from their own face: the same four digits are other ink with the digit face and with none; an unknown face id fails closed.
    PB::Recipe digits = f.recipe;
    digits.text.digitFace = "noto_sans_bold";
    digits.text.faces.push_back(PB::FaceTuning{"noto_sans_bold", 1.0, 0x78, 0.065});
    const PB::Outcome withFace = PB::Render(RequestForBands(digits, Lang::En, {"3333", "x"}), f.picture, nullptr);
    const PB::Outcome withoutFace = PB::Render(RequestForBands(f.recipe, Lang::En, {"3333", "x"}), f.picture, nullptr);
    CHECK_MSG(withFace.ok && withoutFace.ok && withFace.image.rgba != withoutFace.image.rgba, "the digit face draws other digits");
    // ALL ten digits and no other character: '/' and ':' (the characters either side of '0' and '9') are drawn from the slot's own face with the digit face and without it
    for (char c = '/'; c <= ':'; ++c) {
        const std::string text(4, c);
        const PB::Outcome with = PB::Render(RequestForBands(digits, Lang::En, {text, "x"}), f.picture, nullptr);
        const PB::Outcome without = PB::Render(RequestForBands(f.recipe, Lang::En, {text, "x"}), f.picture, nullptr);
        const bool isDigit = c >= '0' && c <= '9';
        CHECK_MSG(with.ok && without.ok && (with.image.rgba != without.image.rgba) == isDigit,
                  std::string("'") + c + "' " + (isDigit ? "is drawn from the digit face" : "is not a digit and is drawn from the slot's face") + ": " + with.reason + without.reason);
    }
    digits.text.digitFace = "no_such_face";
    digits.text.faces.pop_back();
    const PB::Outcome unknown = PB::FitPhrase(RequestForBands(digits, Lang::En, {"3333", "x"}));
    CHECK_MSG(!unknown.ok && Contains(unknown.reason, "no_such_face"), "an unknown digit face fails closed: " + unknown.reason);
    // The spaces of a Japanese phrase: a gap that is drawn here (the strip's dashes have real spaces), never in the plaques.
    PB::Recipe spaced = f.recipe;
    PB::Recipe unspaced = f.recipe;
    unspaced.text.japaneseSpaces = false;
    const std::string jaPhrase = "AB CD";
    const PB::Outcome drawn = PB::FitPhrase(RequestForBands(spaced, Lang::Ja, {jaPhrase, "x"}));
    const PB::Outcome closed = PB::FitPhrase(RequestForBands(unspaced, Lang::Ja, {jaPhrase, "x"}));
    CHECK_MSG(drawn.ok && closed.ok && drawn.bands[0].ink.Width() > closed.bands[0].ink.Width() + 2, "a Japanese space is a gap with japanese_spaces and none without");
    // Determinism, and the PNG.
    const PB::Outcome again = PB::Render(RequestForBands(f.recipe, Lang::En, {"ab - cd", "ef"}), f.picture, nullptr);
    CHECK_MSG(again.ok && again.image.rgba == o.image.rgba, "two bakes are the same pixels");
    // The two asserted guards of the erase: a cleared band must be bounded by transparent rows, and an ink window may hold no opaque pixel outside the cleared rows.
    PB::Image out;
    std::string why;
    {
        StripFixture bad = MakeStrip();
        Put(bad.picture, 40, 18, 0, 0, 0, 255);
        bad.recipe.digest = PB::Digest(bad.picture);
        {
        const bool erased = PB::EraseWords(bad.recipe, bad.picture, nullptr, out, why);
        CHECK_MSG(!erased && Contains(why, "bounded") && out.Empty(), "a cleared band whose row above is not transparent: " + why);
    }
        StripFixture below = MakeStrip();
        Put(below.picture, 40, 101, 0, 0, 0, 255);
        {
        const bool erased = PB::EraseWords(below.recipe, below.picture, nullptr, out, why);
        CHECK_MSG(!erased && Contains(why, "bounded"), "and the row below: " + why);
    }
    }
    // in EACH band's window (the first band's is rows 12..47, the second's 82..117): a check of the first window only would let a stray pixel under the second one through
    for (const int band : {0, 1}) {
        StripFixture bad = MakeStrip();
        Put(bad.picture, 40, band == 0 ? 40 : 110, 0, 0, 0, 255); // inside the window, outside the cleared rows
        const bool erased = PB::EraseWords(bad.recipe, bad.picture, nullptr, out, why);
        CHECK_MSG(!erased && Contains(why, "ink window") && Contains(why, "band " + std::to_string(band)), "an ink window with an opaque pixel outside the cleared rows, band " + std::to_string(band) + ": " + why);
    }
    // And a recipe that asks for fewer or more phrases than it has bands is refused.
    const PB::Outcome few = PB::FitPhrase(RequestForBands(f.recipe, Lang::En, {"only one"}));
    CHECK_MSG(!few.ok && Contains(few.reason, "phrases"), "a phrase too few: " + few.reason);
}

// The strip's ink, the bands' own tunings, the lines' pitch, the anchor of the last line and the right edge, each by a case of its own: the review of step 248 found a mutant of every
// one of them that no test failed.
void TheStripsAlphaTuningsPitchAnchorAndRightEdgeAreHeld() {
    const World& world = TheWorld();
    if (!world.ok) return;
    const StripFixture f = MakeStrip();
    // ALPHA IS THE COVERAGE and is not applied twice: the layer draws the strip at alpha 230 and the baked ink rides it. On a transparent ground with no shadow the ink's alpha IS its
    // coverage, so a pixel the stem of a capital covers whole is alpha 255 (a bake that multiplied the 230 in again would top out at 230).
    {
        PB::Recipe big = f.recipe;
        PB::Band& band = big.text.bands[0];
        band.px = 14.0;
        band.firstBaseline = 42.0;
        band.maxLines = 1;
        const PB::Outcome o = PB::Render(RequestForBands(big, Lang::En, {"HH", "x"}), f.picture, nullptr);
        CHECK_MSG(o.ok, "a large capital is baked: " + o.reason);
        if (o.ok) {
            int maxAlpha = 0;
            int whole = 0;
            for (int y = band.inkRows.a; y < band.inkRows.b; ++y) {
                for (int x = 0; x < o.image.width; ++x) {
                    maxAlpha = std::max(maxAlpha, static_cast<int>(o.image.At(x, y)[3]));
                    whole += o.image.At(x, y)[3] == 255 ? 1 : 0;
                }
            }
            CHECK_MSG(maxAlpha == 255 && whole >= 10, "the ink's alpha is its coverage, 255 where a stem covers the pixel whole, and not multiplied again: the most is " + std::to_string(maxAlpha) + ", " + std::to_string(whole) + " whole pixels");
        }
    }
    // A band's OWN tuning replaces the shared one for that band alone (the heading's embolden is 0.05 against the others' 0.065): a heavier tuning on the first band gives it much more ink
    // and leaves every pixel of the second band's rows as they were.
    {
        PB::Recipe heavy = f.recipe;
        heavy.text.bands[0].faces = {PB::FaceTuning{"kurale", 1.0, 0x78, 0.25}};
        const PB::Outcome plain = PB::Render(RequestForBands(f.recipe, Lang::En, {"ab - cd", "ef"}), f.picture, nullptr);
        const PB::Outcome bold = PB::Render(RequestForBands(heavy, Lang::En, {"ab - cd", "ef"}), f.picture, nullptr);
        CHECK_MSG(plain.ok && bold.ok, "both are baked: " + plain.reason + bold.reason);
        if (plain.ok && bold.ok) {
            const auto ink = [](const PB::Image& image, int y0, int y1) {
                long long sum = 0;
                for (int y = y0; y < y1; ++y) {
                    for (int x = 0; x < image.width; ++x) sum += image.At(x, y)[3];
                }
                return sum;
            };
            CHECK_MSG(ink(bold.image, 12, 48) * 10 > ink(plain.image, 12, 48) * 13, "a band's own embolden gives its band more ink: " + std::to_string(ink(bold.image, 12, 48)) + " against " + std::to_string(ink(plain.image, 12, 48)));
            bool second = true;
            for (int y = 82; y < 118; ++y) second = second && std::equal(plain.image.At(0, y), plain.image.At(0, y) + plain.image.width * 4, bold.image.At(0, y));
            CHECK_MSG(second, "and leaves the other band's rows as they were");
        }
    }
    // The lines' PITCH and the ANCHOR. One band of two lines, no descender, so that nothing pushes the lines apart or slides the block: set from its first baseline the lines are a pitch
    // apart (the first at the baseline, the second one pitch below), and set from its LAST baseline (the heading's) the last line is at the baseline and the first one pitch above.
    {
        PB::Recipe one = f.recipe;
        one.text.bands = {f.recipe.text.bands[0]};
        one.text.maxRight = 142.0;
        PB::Band& band = one.text.bands[0];
        band.maxLines = 2;
        band.inkRows = PB::Span{12, 48};
        const std::vector<std::string> phrase{"aaaa bbbb - cc dd - eeee ffff"};
        band.anchorLast = false;
        band.firstBaseline = 28.0;
        const PB::Outcome fromFirst = PB::FitPhrase(RequestForBands(one, Lang::En, phrase));
        band.anchorLast = true;
        band.firstBaseline = 40.0;
        const PB::Outcome fromLast = PB::FitPhrase(RequestForBands(one, Lang::En, phrase));
        CHECK_MSG(fromFirst.ok && fromFirst.lines == 2 && fromLast.ok && fromLast.lines == 2, "the phrase is two lines both ways: " + fromFirst.reason + fromLast.reason);
        if (fromFirst.ok && fromFirst.lines == 2 && fromLast.ok && fromLast.lines == 2) {
            const double pitchFirst = band.pitch * fromFirst.bands[0].scale;
            const double pitchLast = band.pitch * fromLast.bands[0].scale;
            CHECK_MSG(std::fabs(fromFirst.placed.front().baseline - 28.0) < 0.3 && std::fabs(fromFirst.placed.back().baseline - (28.0 + pitchFirst)) < 0.3,
                      "set from its first baseline the lines are a pitch apart: " + std::to_string(fromFirst.placed.front().baseline) + " and " + std::to_string(fromFirst.placed.back().baseline) + ", pitch " + std::to_string(pitchFirst));
            CHECK_MSG(std::fabs(fromLast.placed.back().baseline - 40.0) < 0.3 && std::fabs(fromLast.placed.front().baseline - (40.0 - pitchLast)) < 0.3,
                      "set from its LAST baseline the last line is at the baseline and the block grows upward: " + std::to_string(fromLast.placed.front().baseline) + " and " + std::to_string(fromLast.placed.back().baseline) +
                          ", pitch " + std::to_string(pitchLast));
        }
    }
    // The RIGHT EDGE is the ink's, not only the advance's: an embolden of three pixels makes the ink wider than the advance, and a right edge one pixel short of the ink (the advance
    // still fits it) refuses the full size.
    {
        PB::Recipe fat = f.recipe;
        fat.text.bands = {f.recipe.text.bands[0]};
        fat.text.bands[0].maxLines = 1;
        fat.text.faces[0].embolden = 0.5;
        const std::vector<std::string> phrase{"abab"};
        const PB::Outcome wide = PB::FitPhrase(RequestForBands(fat, Lang::En, phrase));
        CHECK_MSG(wide.ok && std::fabs(wide.bands[0].scale - 1.0) < 1e-9, "a word fits its band at full size: " + wide.reason);
        if (wide.ok && std::fabs(wide.bands[0].scale - 1.0) < 1e-9) {
            fat.text.maxRight = wide.bands[0].ink.x1 - 1.0;
            const PB::Outcome tight = PB::FitPhrase(RequestForBands(fat, Lang::En, phrase));
            CHECK_MSG(tight.ok && tight.bands[0].scale < 1.0 && tight.bands[0].ink.x1 <= static_cast<int>(std::floor(fat.text.maxRight)),
                      "an ink one pixel past the right edge is shrunk until it is inside: scale " + std::to_string(tight.bands[0].scale) + ", ink to " + std::to_string(tight.bands[0].ink.x1) + " of " + std::to_string(fat.text.maxRight) +
                          ": " + tight.reason);
        }
    }
}

// A strip of a role line, a heading to be wrapped and the first name under it: the heading's ink window runs from under the role to above the name, the heading's cleared rows
// are the middle of it, and a name's bar (not cleared, so never drawn over) begins at row 170.
StripFixture MakeHeadingStrip() {
    StripFixture f = MakeStrip();
    f.picture = PB::Blank(160, 200);
    const auto bar = [&](int x0, int y0, int x1, int y1, double r, double g, double b) {
        for (int y = y0; y < y1; ++y) {
            for (int x = x0; x < x1; ++x) Put(f.picture, x, y, r, g, b, 255);
        }
    };
    bar(12, 20, 120, 30, 0, 0, 0);       // the role line
    bar(12, 100, 150, 140, 0, 0, 0);     // the heading
    bar(10, 170, 100, 182, 40, 40, 160); // the first name
    PB::Recipe& r = f.recipe;
    r.id = "syn_heading";
    r.sprite = "syn_heading.png";
    r.width = 160;
    r.height = 200;
    r.digest = PB::Digest(f.picture);
    r.erase.clearRows = {PB::Span{19, 31}, PB::Span{99, 141}};
    r.erase.rect = PB::Rect{0, 12, 160, 150};
    PB::Band role = r.text.bands[0];
    role.key = std::string(Strings::Key::kCreditsRoleModelling);
    role.maxLines = 1;
    role.px = 6.0;
    role.firstBaseline = 28.0;
    role.inkRows = PB::Span{12, 60};
    PB::Band heading = role;
    heading.key = std::string(Strings::Key::kCreditsThanks);
    heading.px = 9.0;
    heading.anchorLast = true;
    heading.firstBaseline = 136.0;
    heading.pitch = 14.0;
    heading.maxLines = 2;
    heading.inkRows = PB::Span{61, 168};
    heading.breakAt = PB::BreakAt::Space;
    heading.balanced = true;
    heading.balanceAbove = 0;
    heading.balanceBelow = 170.0;
    heading.balanceMinLines = 2;
    r.text.bands = {role, heading};
    return f;
}

// BALANCE (planning step 249): a heading that wraps is placed so that the free rows above it, to the ink of the band above, equal the free rows below it, to the row where the next ink
// begins - and a heading that does not wrap is the picture it was.
void AWrappedHeadingIsBalancedBetweenTheInkAboveAndTheRowBelow() {
    const World& world = TheWorld();
    if (!world.ok) return;
    const StripFixture f = MakeHeadingStrip();
    PB::Recipe plain = f.recipe;
    plain.text.bands[1].balanced = false;
    const std::string role = "aaaa bbbb - cc dd - eeee ffff";
    const std::vector<std::string> twoLines{"ab - cd", "Special thanks here again"};
    const std::vector<std::string> oneLine{"ab - cd", "Thanks"};
    const auto gaps = [](const PB::Outcome& o, double below, double& above, double& under) {
        above = o.bands[1].ink.y0 - o.bands[0].ink.y1;
        under = below - o.bands[1].ink.y1;
    };
    // Two lines: the gaps are equal to within two rows, and both are real; without the rule they are not.
    const PB::Outcome balanced = PB::Render(RequestForBands(f.recipe, Lang::En, twoLines), f.picture, nullptr);
    const PB::Outcome anchored = PB::Render(RequestForBands(plain, Lang::En, twoLines), f.picture, nullptr);
    CHECK_MSG(balanced.ok && anchored.ok && balanced.bands.size() == 2 && balanced.bands[1].lines == 2 && anchored.bands[1].lines == 2, "the heading wraps to two lines both ways: " + balanced.reason + anchored.reason);
    if (!balanced.ok || !anchored.ok || balanced.bands[1].lines != 2) return;
    double above = 0.0;
    double under = 0.0;
    gaps(balanced, 170.0, above, under);
    CHECK_MSG(std::fabs(above - under) <= 1.0 && above >= 10.0, "balanced: the free rows above the heading (" + std::to_string(above) + ") are the free rows below it (" + std::to_string(under) + ") to within one");
    double aboveBefore = 0.0;
    double underBefore = 0.0;
    gaps(anchored, 170.0, aboveBefore, underBefore);
    CHECK_MSG(std::fabs(aboveBefore - underBefore) > 6.0, "anchored by its last baseline the gaps are not equal (" + std::to_string(aboveBefore) + " above, " + std::to_string(underBefore) + " below): the rule moves the heading");
    CHECK_MSG(balanced.bands[1].ink.y0 != anchored.bands[1].ink.y0 && balanced.bands[1].scale == anchored.bands[1].scale, "and it moves it, at the same size");
    // The heading's ink stays inside its window, and the role line above it is the picture it was either way.
    CHECK_MSG(balanced.bands[1].ink.y0 >= f.recipe.text.bands[1].inkRows.a && balanced.bands[1].ink.y1 <= f.recipe.text.bands[1].inkRows.b, "inside its ink rows");
    bool roleSame = true;
    for (int y = 12; y < 60; ++y) roleSame = roleSame && std::equal(balanced.image.At(0, y), balanced.image.At(0, y) + balanced.image.width * 4, anchored.image.At(0, y));
    CHECK_MSG(roleSame, "the band above is the same pixels with and without the rule");

    // One line: byte for byte the unbalanced picture (the one-line headings of English-like languages keep their baseline).
    const PB::Outcome single = PB::Render(RequestForBands(f.recipe, Lang::En, oneLine), f.picture, nullptr);
    const PB::Outcome singlePlain = PB::Render(RequestForBands(plain, Lang::En, oneLine), f.picture, nullptr);
    CHECK_MSG(single.ok && singlePlain.ok && single.bands[1].lines == 1, "a short heading is one line: " + single.reason);
    if (single.ok && singlePlain.ok) {
        CHECK_MSG(single.image.rgba == singlePlain.image.rgba && single.bands[1].ink.y0 == singlePlain.bands[1].ink.y0, "a heading of one line is the same picture with and without the rule");
        CHECK_MSG(std::fabs(single.placed.back().baseline - 136.0) < 0.3, "and its last baseline is the anchor's: " + std::to_string(single.placed.back().baseline));
    }
    // min_lines is the line count that turns it on: with three required, a heading of two keeps its baseline.
    PB::Recipe three = f.recipe;
    three.text.bands[1].maxLines = 3;
    three.text.bands[1].balanceMinLines = 3;
    const PB::Outcome notYet = PB::Render(RequestForBands(three, Lang::En, twoLines), f.picture, nullptr);
    CHECK_MSG(notYet.ok && notYet.bands[1].lines == 2 && notYet.image.rgba == anchored.image.rgba, "a heading of fewer lines than min_lines is anchored as it was");
    // below_row is the row the gap below is measured to: moving it moves the heading by half the move.
    PB::Recipe lower = f.recipe;
    lower.text.bands[1].balanceBelow = 180.0;
    const PB::Outcome moved = PB::Render(RequestForBands(lower, Lang::En, twoLines), f.picture, nullptr);
    if (moved.ok) {
        double a2 = 0.0;
        double u2 = 0.0;
        gaps(moved, 180.0, a2, u2);
        CHECK_MSG(std::fabs(a2 - u2) <= 1.0 && moved.bands[1].ink.y0 >= balanced.bands[1].ink.y0 + 3 && moved.bands[1].ink.y0 <= balanced.bands[1].ink.y0 + 7,
                  "below_row 180 puts the heading half of ten rows lower, balanced to that row: " + std::to_string(moved.bands[1].ink.y0 - balanced.bands[1].ink.y0));
    } else {
        CHECK_MSG(false, "the lower row bakes: " + moved.reason);
    }
    // above_band is the band whose ink bottom the gap above is measured from: a role line of two lines ends lower, and the heading follows its ink, not a number.
    PB::Recipe taller = f.recipe;
    taller.text.bands[0].maxLines = 2;
    taller.text.maxRight = 142.0;
    const std::vector<std::string> longRole{role, "Special thanks here again"};
    const PB::Outcome follows = PB::Render(RequestForBands(taller, Lang::En, longRole), f.picture, nullptr);
    CHECK_MSG(follows.ok && follows.bands[0].lines == 2 && follows.bands[1].lines == 2, "a role of two lines over a heading of two: " + follows.reason);
    if (follows.ok && follows.bands[0].lines == 2 && follows.bands[1].lines == 2) {
        double a3 = 0.0;
        double u3 = 0.0;
        gaps(follows, 170.0, a3, u3);
        CHECK_MSG(follows.bands[0].ink.y1 > balanced.bands[0].ink.y1 + 6 && std::fabs(a3 - u3) <= 1.0 && follows.bands[1].ink.y0 > balanced.bands[1].ink.y0,
                  "the heading is balanced to where the role ended in THIS language (" + std::to_string(follows.bands[0].ink.y1) + "), not to a number: " + std::to_string(a3) + " above, " + std::to_string(u3) + " below");
    }
    // A heading with nowhere to go - its window cannot hold it balanced - shrinks a step (the fit's own rule), and is refused below the floor.
    PB::Recipe tight = f.recipe;
    tight.text.bands[1].inkRows = PB::Span{61, 100};
    const PB::Outcome squeezed = PB::FitPhrase(RequestForBands(tight, Lang::En, twoLines));
    CHECK_MSG(!squeezed.ok || squeezed.bands[1].ink.y1 <= 100, "a window too small for the balanced block is never overrun: " + squeezed.reason);
}

// THE LINE GAP (planning step 249): the least clear distance between two lines of a band is the recipe's `gap` of the band's px, scaled; the lines are pushed apart where the ink
// needs it and nowhere else, and a gap that is already kept changes nothing.
void ALineGapIsHeldBetweenTheLinesOfABand() {
    const World& world = TheWorld();
    if (!world.ok) return;
    const StripFixture f = MakeStrip();
    // Descenders over ascenders: the tight pair.
    const std::vector<std::string> phrase{"gggg gggg - hhhh hhhh", "x"};
    double least = 1e9;
    double largest = 0.0;
    for (const double gap : {0.0, 0.05, 0.15, 0.30, 0.45}) {
        PB::Recipe recipe = f.recipe;
        recipe.text.gap = gap;
        recipe.text.maxRight = 90.0;
        const PB::Outcome o = PB::FitPhrase(RequestForBands(recipe, Lang::En, phrase));
        CHECK_MSG(o.ok && o.bands[0].lines == 2, "two lines at gap " + std::to_string(gap) + ": " + o.reason);
        if (!o.ok || o.bands[0].lines != 2) continue;
        const double wanted = gap * recipe.text.bands[0].px * o.bands[0].scale;
        CHECK_MSG(o.bands[0].lineGap < PB::kNoLineGap && o.bands[0].lineGap >= wanted - 0.5, "gap " + std::to_string(gap) + ": the lines' ink is at least " + std::to_string(wanted) + " px apart: " + std::to_string(o.bands[0].lineGap));
        CHECK_MSG(o.bands[1].lines == 1 && o.bands[1].lineGap == PB::kNoLineGap, "a band of one line has no gap to speak of");
        if (gap == 0.0) least = o.bands[0].lineGap;
        largest = o.bands[0].lineGap;
    }
    CHECK_MSG(largest > least + 1.0, "a bigger gap really pushes the lines apart: " + std::to_string(least) + " px at 0, " + std::to_string(largest) + " at 0.45");
    // A gap the pitch already keeps changes nothing: the same pixels at gap 0 and at a gap that is below what the pitch leaves.
    PB::Recipe loose = f.recipe;
    loose.text.bands[0].pitch = 30.0;
    loose.text.maxRight = 90.0;
    PB::Recipe loose0 = loose;
    loose0.text.gap = 0.0;
    PB::Recipe loose15 = loose;
    loose15.text.gap = 0.15;
    const PB::Outcome a = PB::Render(RequestForBands(loose0, Lang::En, phrase), f.picture, nullptr);
    const PB::Outcome b = PB::Render(RequestForBands(loose15, Lang::En, phrase), f.picture, nullptr);
    CHECK_MSG(a.ok && b.ok && a.image.rgba == b.image.rgba, "lines already far apart are not moved by the gap: " + a.reason + b.reason);
}

// The halo of a word is written beyond the erased rectangle only where the recipe's `paint` rectangle says so (the golden plaque's, to row 216): with it the halo darkens plate pixels
// below the rectangle and none beyond the paint rectangle, without it none outside the erased one is touched.
void ThePaintRectangleLetsTheHaloReachPastTheErasedRectangle() {
    const World& world = TheWorld();
    if (!world.ok) return;
    PlaqueFixture f = MakePlaque(false);
    PB::Text& t = f.recipe.text;
    t.anchorY = 132.0; // ink to about row 138, inside the rectangle (to 140); its halo reaches row 142 and below
    t.safe = PB::Rect{14, 100, 98, 140};
    t.maxLines = 1;
    const std::string phrase = "ON ME";
    const PB::Outcome without = PB::Render(RequestFor(f.recipe, Lang::En, phrase), f.picture, nullptr);
    CHECK_MSG(without.ok, "a word at the bottom of the plaque is baked: " + without.reason);
    t.hasPaint = true;
    t.paint = PB::Rect{2, 80, 106, 150};
    const PB::Outcome with = PB::Render(RequestFor(f.recipe, Lang::En, phrase), f.picture, nullptr);
    CHECK_MSG(with.ok, "and with a paint rectangle: " + with.reason);
    if (!without.ok || !with.ok) return;
    const PB::Rect erased = f.recipe.erase.rect;
    CHECK_MSG(ChangedOutsideRects(without.image, f.picture, {erased}) == 0, "without a paint rectangle nothing outside the erased one changes");
    CHECK_MSG(without.touched.y1 == erased.y1 && with.touched.y1 == 150 && with.touched.y0 == erased.y0, "and the outcome says what it may touch: " + std::to_string(without.touched.y1) + ", " + std::to_string(with.touched.y1));
    int reached = 0;
    for (int y = erased.y1; y < 150; ++y) {
        for (int x = 0; x < f.picture.width; ++x) reached += std::equal(with.image.At(x, y), with.image.At(x, y) + 4, f.picture.At(x, y)) ? 0 : 1;
    }
    CHECK_MSG(reached >= 20, "with it the halo darkens plate pixels below the erased rectangle: " + std::to_string(reached));
    CHECK_MSG(ChangedOutsideRects(with.image, f.picture, {PB::Rect{2, 80, 106, 150}}) == 0, "and nothing beyond the paint rectangle");
}

// V's slope across a hole in its knots is compared with its slope over the LAST TWELVE clean rows: a plate whose profile flattens over its last five clean rows (a lift of half a level
// more each row) passes at twelve, and a check that read only the last four would refuse it.
void TheSlopeIsReadOverTheLastTwelveCleanRows() {
    const World& world = TheWorld();
    if (!world.ok) return;
    PlaqueFixture f = MakePlaque(false);
    for (int y = 71; y < 76; ++y) {
        const double lift = 0.5 * (y - 70);
        for (int x = 10; x < 98; ++x) {
            for (int c = 0; c < 3; ++c) f.picture.At(x, y)[c] = static_cast<unsigned char>(std::clamp(std::lround(f.picture.At(x, y)[c] * (PlaqueV(y) + lift) / PlaqueV(y)), 0L, 255L));
        }
    }
    f.recipe.erase.plaque.slopeBand = 0.45;
    f.recipe.erase.plaque.ringMax = 6.0; // the last clean rows are lifted by 2.5 levels, and the ring is read from rows that are not
    PB::Image out;
    std::string why;
    CHECK_MSG(PB::EraseWords(f.recipe, f.picture, nullptr, out, why), "a profile that flattens over its last five clean rows is erased (the slope is read over twelve): " + why);
    // and the band is a real limit: a band of 0.2 refuses the same plate, naming the slope
    f.recipe.erase.plaque.slopeBand = 0.2;
    CHECK_MSG(!PB::EraseWords(f.recipe, f.picture, nullptr, out, why) && Contains(why, "slope"), "a band of 0.2 refuses it: " + why);
}

// ---- paragraphs -----------------------------------------------------------------------------------------------------------------------------------

void AParagraphIsCentredInsideItsBoxWithTheFewestLines() {
    const World& world = TheWorld();
    if (!world.ok) return;
    const PlaqueFixture f = MakePlaque(false);
    const PB::Recipe& recipe = f.recipe;
    const PB::Outcome o = PB::Render(RequestFor(recipe, Lang::En, "current score"), f.picture, nullptr);
    CHECK_MSG(o.ok, "a paragraph is drawn on the plaque: " + o.reason);
    if (!o.ok) return;
    const PB::Rect safe = recipe.text.safe;
    CHECK_MSG(o.ink.x0 >= safe.x0 && o.ink.x1 <= safe.x1 && o.ink.y0 >= safe.y0 && o.ink.y1 <= safe.y1, "the ink is inside the box");
    CHECK_MSG(std::fabs((o.ink.x0 + o.ink.x1) / 2.0 - recipe.text.anchorX) <= 2.0 && std::fabs((o.ink.y0 + o.ink.y1) / 2.0 - recipe.text.anchorY) <= 3.0,
              "the block is centred on its anchor, across and down: " + std::to_string((o.ink.x0 + o.ink.x1) / 2.0) + ", " + std::to_string((o.ink.y0 + o.ink.y1) / 2.0));
    CHECK_MSG(ChangedOutsideRects(o.image, f.picture, {o.touched}) == 0, "nothing outside what it says it touched changes");
    CHECK_MSG(o.lines == 2 && o.lineTexts.size() == 2 && o.scale <= 1.0 && o.scale >= recipe.text.minScale, "two lines at a scale between the floor and one");
    if (o.lineGap < PB::kNoLineGap) CHECK_MSG(o.lineGap >= recipe.text.gap * recipe.text.px * o.scale - 0.5, "the two lines' ink is the gap apart: " + std::to_string(o.lineGap));
    // The line pitch is the FACE's (1.4 of the scaled px for Kurale here): the baselines are at least that far apart, and in a face tuned to 1.0 they are nearer.
    {
        const auto baselines = [](const PB::Outcome& outcome) {
            double first = -1.0;
            double second = -1.0;
            for (const PB::Placed& placed : outcome.placed) {
                if (placed.line == 0 && first < 0.0) first = placed.baseline;
                if (placed.line == 1 && second < 0.0) second = placed.baseline;
            }
            return second - first;
        };
        CHECK_MSG(baselines(o) >= 1.4 * recipe.text.px * o.scale - 0.3, "the baselines are a pitch apart: " + std::to_string(baselines(o)));
        PB::Recipe loose = recipe;
        loose.text.faces[0].pitch = 1.0;
        const PB::Outcome nearer = PB::FitPhrase(RequestFor(loose, Lang::En, "current score"));
        CHECK_MSG(nearer.ok && baselines(nearer) < baselines(o) - 1.0, "a face tuned to a smaller pitch sets its lines nearer: " + std::to_string(baselines(nearer)) + " against " + std::to_string(baselines(o)));
    }
    // The fewest lines: a short phrase is ONE line (a stack would make two), and the lines never get fewer as the phrase gets longer.
    const PB::Outcome shortPhrase = PB::FitPhrase(RequestFor(recipe, Lang::En, "go on"));
    CHECK_MSG(shortPhrase.ok && shortPhrase.lines == 1, "a short phrase is one line: " + std::to_string(shortPhrase.lines));
    int previous = 1;
    std::string phrase;
    int lastLines = 0;
    for (int words = 1; words <= 8; ++words) {
        phrase += (words > 1 ? " " : "") + std::string("ab");
        const PB::Outcome grown = PB::FitPhrase(RequestFor(recipe, Lang::En, phrase));
        if (!grown.ok) break;
        CHECK_MSG(grown.lines >= previous, "the lines do not get fewer as the phrase grows: " + std::to_string(grown.lines) + " after " + std::to_string(previous));
        previous = grown.lines;
        lastLines = grown.lines;
    }
    CHECK_MSG(lastLines == 2, "and a long one is two: " + std::to_string(lastLines));
    // Down to the floor, no further: a floor above what the phrase needs refuses it, naming the scale.
    PB::Recipe tight = recipe;
    tight.text.maxWidth = 30.0;
    tight.text.minScale = 0.95;
    const PB::Outcome refused = PB::FitPhrase(RequestFor(tight, Lang::En, "current score"));
    CHECK_MSG(!refused.ok && Contains(refused.reason, "below the recipe's floor"), "a paragraph below its floor fails closed: " + refused.reason);
}

void AnOrphanLineAndTheLinesSavedAreWeighedByTheRecipe() {
    const World& world = TheWorld();
    if (!world.ok) return;
    const PlaqueFixture f = MakePlaque(false);
    PB::Recipe recipe = f.recipe;
    recipe.text.maxWidth = 400.0;
    recipe.text.safe = PB::Rect{0, 0, 112, 160};
    const PB::Outcome one = PB::FitPhrase(RequestFor(recipe, Lang::En, "WWWW WW"));
    CHECK_MSG(one.ok && one.lines == 1, "on a wide box 'WWWW WW' is one line: " + one.reason);
    if (!one.ok) return;
    // A box too narrow for one line and wide enough for 'WWWW': two lines, the second half the first; an orphan limit of 0.4 allows it and one of 0.99 does not.
    recipe.text.maxWidth = one.ink.Width() * 0.78;
    recipe.text.safe = PB::Rect{0, 0, 112, 160};
    recipe.text.orphan = 0.0;
    recipe.text.minScale = 0.9; // a floor above what the one-line layout needs (0.78): two lines at full size, or nothing
    const PB::Outcome two = PB::FitPhrase(RequestFor(recipe, Lang::En, "WWWW WW"));
    CHECK_MSG(two.ok && two.lines == 2 && std::fabs(two.scale - 1.0) < 1e-9, "a narrower box makes two lines at full size: " + two.reason + " lines " + std::to_string(two.lines));
    recipe.text.orphan = 0.4;
    CHECK_MSG(PB::FitPhrase(RequestFor(recipe, Lang::En, "WWWW WW")).ok, "an orphan limit of 0.4 allows a second line half the first");
    recipe.text.orphan = 0.99;
    const PB::Outcome orphaned = PB::FitPhrase(RequestFor(recipe, Lang::En, "WWWW WW"));
    CHECK_MSG(!orphaned.ok, "and one of 0.99 does not: a line that is a fraction of the other is refused");
    // saveLines: fewer lines at a slightly smaller scale win when the recipe says a line saved is worth that much. Three words; two fit at about 0.85, three at 1.
    PB::Recipe saving = f.recipe;
    saving.text.safe = PB::Rect{0, 0, 112, 160};
    saving.text.maxLines = 3;
    saving.text.maxWidth = 1000.0;
    const PB::Outcome two2 = PB::FitPhrase(RequestFor(saving, Lang::En, "MMMM MMMM"));
    CHECK_MSG(two2.ok && two2.lines == 1, "two words fit one line on a wide box");
    if (!two2.ok) return;
    saving.text.maxWidth = two2.ink.Width() * 0.9;
    saving.text.saveLines = 0.0;
    const PB::Outcome exact = PB::FitPhrase(RequestFor(saving, Lang::En, "MMMM MMMM"));
    CHECK_MSG(exact.ok && exact.lines == 2 && exact.scale > 0.9, "without saveLines two words in a narrow box are two lines at the largest scale: " + std::to_string(exact.scale));
    saving.text.maxWidth = two2.ink.Width() * 0.95;
    saving.text.saveLines = 0.0;
    const PB::Outcome crisp = PB::FitPhrase(RequestFor(saving, Lang::En, "MMMM MMMM MMMM"));
    saving.text.saveLines = 0.2;
    const PB::Outcome saved = PB::FitPhrase(RequestFor(saving, Lang::En, "MMMM MMMM MMMM"));
    CHECK_MSG(crisp.ok && saved.ok && saved.lines < crisp.lines && saved.scale < crisp.scale, "saveLines takes fewer lines at a smaller scale: " + std::to_string(crisp.lines) + " lines at " + std::to_string(crisp.scale) + " against " +
                                                                                              std::to_string(saved.lines) + " at " + std::to_string(saved.scale));
}

// A break the data marks with U+200B and one it marks with an ASCII space (which in Japanese only breaks) are the same layout, glyph for glyph - for every shipped phrase
// of the three paragraph pictures; and a no-break space of the French card never leaves its '!' alone on a line.
void TheBreakMarksOfTheNewPhrasesAreHandled() {
    const World& world = TheWorld();
    if (!world.ok) return;
    Strings::Table japanese;
    Strings::Table french;
    std::string error;
    CHECK(japanese.Load(kData, Lang::Ja, error) && french.Load(kData, Lang::Fr, error));
    int compared = 0;
    for (const std::string_view key : {Strings::Key::kPictureCurrentScore, Strings::Key::kPictureGoldenScore, Strings::Key::kPictureGameFinished}) {
        const PB::Recipe* recipe = nullptr;
        for (const PB::Recipe& candidate : world.recipes.items) recipe = candidate.key == key ? &candidate : recipe;
        CHECK_MSG(recipe != nullptr, std::string(key) + " has a recipe");
        if (recipe == nullptr) continue;
        const std::string phrase = japanese.Get(std::string(key));
        CHECK_MSG(phrase.find(' ') != std::string::npos, std::string(key) + " in Japanese holds a place to break");
        std::string zero;
        for (const char c : phrase) zero += c == ' ' ? Cp({0x200B}) : std::string(1, c);
        const PB::Outcome spaced = PB::FitPhrase(RequestFor(*recipe, Lang::Ja, phrase));
        const PB::Outcome marked = PB::FitPhrase(RequestFor(*recipe, Lang::Ja, zero));
        CHECK_MSG(spaced.ok && marked.ok, std::string(key) + ": both are drawn: " + spaced.reason + marked.reason);
        if (!spaced.ok || !marked.ok) continue;
        bool same = spaced.placed.size() == marked.placed.size() && spaced.scale == marked.scale && spaced.lines == marked.lines;
        for (std::size_t i = 0; same && i < spaced.placed.size(); ++i) {
            same = spaced.placed[i].codePoint == marked.placed[i].codePoint && spaced.placed[i].penX == marked.placed[i].penX && spaced.placed[i].baseline == marked.placed[i].baseline;
            if (spaced.placed[i].codePoint == 0x200B || spaced.placed[i].codePoint == 0x20) same = false; // neither is ever drawn
        }
        CHECK_MSG(same, std::string(key) + ": an ASCII space and U+200B lay out the same glyphs, and neither is drawn");
        ++compared;
    }
    CHECK_EQ(compared, 3);
    const PB::Recipe* card = world.recipes.Find("game_finished.png");
    CHECK(card != nullptr);
    if (card == nullptr) return;
    const std::string phrase = french.Get(Strings::Key::kPictureGameFinished);
    CHECK_MSG(phrase.find(Cp({0x00A0, '!'})) != std::string::npos, "the French card holds a no-break space before its '!'");
    const PB::Outcome o = PB::FitPhrase(RequestFor(*card, Lang::Fr, phrase));
    CHECK_MSG(o.ok, "the French card is drawn: " + o.reason);
    for (const std::string& line : o.lineTexts) CHECK_MSG(line.empty() || line.front() != '!', "no line begins with the '!': [" + line + "]");
    // Arabic breaks in LOGICAL order, so the first word is on the first line.
    Strings::Table arabic;
    CHECK(arabic.Load(kData, Lang::Ar, error));
    const std::string arabicPhrase = arabic.Get(Strings::Key::kPictureGameFinished);
    const PB::Outcome ar = PB::FitPhrase(RequestFor(*card, Lang::Ar, arabicPhrase));
    CHECK_MSG(ar.ok && ar.lineTexts.size() >= 2 && arabicPhrase.rfind(ar.lineTexts[0], 0) == 0, "the Arabic card's first line is its first words");
}

// The shipped phrases of the credits strip and the card are drawn above the floors by the recipes' own numbers.
void EveryShippedPhraseOfTheNewPicturesIsAboveItsFloor() {
    const World& world = TheWorld();
    if (!world.ok) return;
    int fits = 0;
    for (std::size_t i = 1; i < Strings::kLangCount; ++i) {
        const Lang lang = static_cast<Lang>(i);
        Strings::Table table;
        std::string error;
        CHECK_MSG(table.Load(kData, lang, error), error);
        for (const PB::Recipe& recipe : world.recipes.items) {
            if (recipe.text.layout != PB::Layout::Paragraph && recipe.text.layout != PB::Layout::Bands) continue;
            const std::string single = recipe.key.empty() ? std::string() : table.Get(recipe.key);
            PB::Request request = RequestFor(recipe, lang, single);
            for (const PB::Band& band : recipe.text.bands) request.phrases.push_back(table.Get(band.key));
            const PB::Outcome o = PB::FitPhrase(request);
            CHECK_MSG(o.ok && o.scale >= recipe.text.minScale - 1e-9, recipe.id + " in " + Code(lang) + " is at or above its floor: " + o.reason);
            ++fits;
        }
    }
    CHECK_EQ(fits, 40);
}

// ---- the loader, for the new rules and layouts ----------------------------------------------------------------------------------------------------

// A table of ONE recipe from the pieces of its erase and text blocks: the pairs of rule and layout the loader refuses are tried here, with everything else valid.
std::string MiniTable(const std::string& erase, const std::string& text, const std::string& keyLine = "\"key\": \"picture.points\",") {
    return "{\"version\": 1, \"recipes\": [{\"id\": \"mini\", " + keyLine + " \"sprite\": \"mini.png\", \"size\": [128, 128], \"digest\": \"0123456789abcdef0123456789abcdef\", \"erase\": " + erase +
           ", \"text\": " + text + "}]}";
}

void TheLoaderRefusesWhatTheNewRulesDoNotAllow() {
    const std::string table = TableText();
    struct Fault {
        const char* name;
        std::string from;
        std::string to;
        std::string expect;
    };
    const std::vector<Fault> faults = {
        {"margin ratio", "\"margin_ratio\": 0.7,", "\"margin_ratio\": 1.5,", "margin_ratio"},
        {"slope band", "\"slope_band\": 0.3,", "\"slope_band\": -1.0,", "guard"},
        {"agree ratio", "\"agree_ratio\": 0.7,", "\"agree_ratio\": 0.0,", "agree_ratio"},
        {"break", "\"break\": \"after_dash\"", "\"break\": \"anywhere\"", "break"},
        {"anchor", "\"anchor\": \"first\"", "\"anchor\": \"middle\"", "anchor"},
        {"max right", "\"max_right\": 470.0", "\"max_right\": 4700.0", "max_right"},
        {"ink level", "\"ink_level\": 0", "\"ink_level\": 300", "ink_level"},
        {"orphan", "\"orphan\": 0.0,", "\"orphan\": 1.5,", "orphan"},
        {"save lines", "\"save_lines\": 0.0\n", "\"save_lines\": 0.9\n", "save_lines"},
        {"clear rows as a plaque", "\"rule\": \"clear_rows\"", "\"rule\": \"plaque\"", "clear"},
        {"bands key at the top", "\"id\": \"credits_strip\",", "\"id\": \"credits_strip\",\n      \"key\": \"picture.points\",", "belongs to a recipe of one phrase"},
        {"a face without its pitch in a paragraph", "\"embolden\": 0.03,\n            \"pitch\": 1.4\n", "\"embolden\": 0.03\n", "pitch"},
        {"a paragraph made a word", "\"layout\": \"paragraph\"", "\"layout\": \"word\"", "pitch"},
        {"a chroma radius", "\"radius\": 2,", "\"radius\": 20,", "radius"},
        {"unknown member of a plaque", "\"agree_tol\": 2.0,", "\"agree_tol\": 2.0,\n        \"agree_tol_typo\": 1.0,", "agree_tol_typo"},
        {"a cleared band touching the next", "\n            188,\n", "\n            101,\n", "touches"},
    };
    for (const Fault& fault : faults) {
        std::string mutated = table;
        const std::size_t at = mutated.find(fault.from);
        if (at == std::string::npos) {
            CHECK_MSG(false, std::string("the text of fault '") + fault.name + "' is in the committed table (its layout changed: edit this case)");
            continue;
        }
        mutated.replace(at, fault.from.size(), fault.to);
        const std::string error = LoadText(std::string("n-") + fault.name, mutated);
        CHECK_MSG(!error.empty() && Contains(error, fault.expect), std::string("'") + fault.name + "' is refused, naming " + fault.expect + ": " + error);
    }
    // The pairs of a rule and a layout no recipe has been measured for are refused, whatever else is in order: a stack on a plate, a paragraph on a modelled plate, a word on a
    // plaque, bands on a plate and a plate under bands.
    const std::string rowLerp = "{\"rule\": \"row_lerp\", \"rect\": [20, 20, 100, 60], \"ref_cols\": [[4, 16], [104, 116]], \"guard\": {\"agree_max\": 3.0, \"collar_max\": 5.0}}";
    const std::string generated = "{\"rule\": \"generated\"}";
    const std::string shadow = "\"shadow\": {\"dx\": 0.0, \"dy\": 0.0, \"sigma\": 0.1, \"sigma_min\": 0.0, \"gain\": 1.0, \"cap\": 1.0, \"round_offsets\": false}, \"inner\": {\"k\": 0.0, \"sigma\": 0.0, \"dx\": 0.0, \"dy\": 0.0}";
    const std::string common = "\"slot\": \"matura\", \"px\": 12.0, \"anchor\": [64.0, 64.0], \"min_scale\": 0.5, \"step\": 0.97, \"ink_threshold\": 0.04, \"max_width\": 100, ";
    const std::string word = "{\"layout\": \"word\", " + common + "\"faces\": {}, \"max_height\": 30, " + shadow + "}";
    const std::string stack = "{\"layout\": \"stack\", " + common + "\"faces\": {}, " + shadow + ", \"first_baseline\": 60.0, \"pitch\": 20.0, \"gap\": 0.05, \"max_lines\": 2, \"safe\": [10, 10, 118, 118]}";
    const std::string paragraph = "{\"layout\": \"paragraph\", " + common + "\"faces\": {\"kurale\": {\"ratio\": 1.0, \"embolden\": 0.03, \"pitch\": 1.4}}, " + shadow +
                                  ", \"gap\": 0.05, \"max_lines\": 2, \"safe\": [10, 10, 118, 118], \"orphan\": 0.0, \"save_lines\": 0.0}";
    CHECK_MSG(LoadText("mini-word-on-row-lerp", MiniTable(rowLerp, word)).empty(), "the mini table's word on a row_lerp plate loads (the control)");
    CHECK_MSG(LoadText("mini-stack-on-generated", MiniTable(generated, stack)).empty(), "the mini table's stack on a generated picture loads (the control)");
    const std::string stackOnPlate = LoadText("mini-stack-on-plate", MiniTable(rowLerp, stack));
    CHECK_MSG(Contains(stackOnPlate, "stack of lines"), "a stack on a plate is refused: " + stackOnPlate);
    const std::string paragraphOnPlate = LoadText("mini-paragraph-on-plate", MiniTable(rowLerp, paragraph));
    CHECK_MSG(Contains(paragraphOnPlate, "no recipe has been measured for"), "a paragraph on a row_lerp plate is refused: " + paragraphOnPlate);
    const std::string paragraphOnGenerated = LoadText("mini-paragraph-on-generated", MiniTable(generated, paragraph));
    CHECK_MSG(Contains(paragraphOnGenerated, "no recipe has been measured for"), "a paragraph on a generated picture is refused: " + paragraphOnGenerated);
    const std::string clearWithWord = LoadText("mini-word-on-clear", MiniTable("{\"rule\": \"clear_rows\", \"clear\": [[20, 40]]}", word));
    CHECK_MSG(!clearWithWord.empty(), "cleared rows under a word are refused: " + clearWithWord);
    const std::string bandsText = "{\"layout\": \"bands\", \"slot\": \"matura\", \"faces\": {\"kurale\": {\"ratio\": 1.0, \"embolden\": 0.03, \"reference\": \"U+0078\"}}, \"min_scale\": 0.7, \"step\": 0.97, "
                                  "\"ink_threshold\": 0.02, \"gap\": 0.05, \"left\": 10.0, \"max_right\": 100.0, \"ink_level\": 0, \"bands\": [{\"key\": \"credits.thanks\", \"px\": 8.0, "
                                  "\"first_baseline\": 36.0, \"anchor\": \"first\", \"pitch\": 16.0, \"max_lines\": 1, \"ink_rows\": [18, 44], \"break\": \"space\"}]}";
    const std::string clear = "{\"rule\": \"clear_rows\", \"clear\": [[20, 40]]}";
    CHECK_MSG(LoadText("mini-bands-on-clear", MiniTable(clear, bandsText, "")).empty(), "bands over cleared rows load (the control)");
    const std::string bandsOnPlate = LoadText("mini-bands-on-plate", MiniTable(rowLerp, bandsText, ""));
    CHECK_MSG(Contains(bandsOnPlate, "no recipe has been measured for"), "bands on a plate are refused: " + bandsOnPlate);
    std::string windowShort = bandsText;
    windowShort.replace(windowShort.find("[18, 44]"), 8, "[22, 44]");
    const std::string notHeld = LoadText("mini-window", MiniTable(clear, windowShort, ""));
    CHECK_MSG(Contains(notHeld, "does not hold the cleared rows"), "a band's window that does not hold its cleared rows is refused: " + notHeld);
    std::string twoClears = clear;
    twoClears.replace(twoClears.find("[[20, 40]]"), 10, "[[20, 40], [60, 70]]");
    const std::string count = LoadText("mini-count", MiniTable(twoClears, bandsText, ""));
    CHECK_MSG(Contains(count, "cleared bands"), "a different number of cleared bands and bands is refused: " + count);
    // A band's balance (planning step 249): its three members, an earlier band to measure from, two lines at least and no more than the band wraps to, a row at or under its window.
    {
        const std::string firstBand = "{\"key\": \"credits.role.design\", \"px\": 8.0, \"first_baseline\": 36.0, \"anchor\": \"first\", \"pitch\": 16.0, \"max_lines\": 1, \"ink_rows\": [18, 44], \"break\": \"space\"}";
        const std::string secondBand = "{\"key\": \"credits.thanks\", \"px\": 8.0, \"first_baseline\": 76.0, \"anchor\": \"last\", \"pitch\": 16.0, \"max_lines\": 2, \"ink_rows\": [58, 100], \"break\": \"space\", \"balance\": ";
        const std::string head = bandsText.substr(0, bandsText.find("\"bands\": [") + 10);
        const auto twoBands = [&](const std::string& balance) { return head + firstBand + ", " + secondBand + balance + "}]}"; };
        const std::string rows = "{\"rule\": \"clear_rows\", \"clear\": [[20, 40], [60, 80]]}";
        const std::string good = "{\"above_band\": 0, \"below_row\": 110, \"min_lines\": 2}";
        CHECK_MSG(LoadText("mini-balance-ok", MiniTable(rows, twoBands(good), "")).empty(), "a band with a balance loads (the control)");
        const std::vector<std::pair<std::string, std::string>> bad = {
            {"{\"above_band\": 1, \"below_row\": 110, \"min_lines\": 2}", "above_band"},   // itself
            {"{\"above_band\": 5, \"below_row\": 110, \"min_lines\": 2}", "above_band"},   // none
            {"{\"above_band\": -1, \"below_row\": 110, \"min_lines\": 2}", "above_band"},
            {"{\"above_band\": 0, \"below_row\": 110, \"min_lines\": 1}", "min_lines"},   // nothing to balance
            {"{\"above_band\": 0, \"below_row\": 110, \"min_lines\": 3}", "min_lines"},   // more than the band wraps to
            {"{\"above_band\": 0, \"below_row\": 99, \"min_lines\": 2}", "below_row"},    // inside its window
            {"{\"above_band\": 0, \"below_row\": 500, \"min_lines\": 2}", "below_row"},   // off the picture
            {"{\"above_band\": 0, \"min_lines\": 2}", "below_row"},
            {"{\"above_band\": 0, \"below_row\": 110, \"min_lines\": 2, \"extra\": 1}", "extra"},
            {"[0, 110, 2]", "balance"},
        };
        for (const auto& [balance, needle] : bad) {
            const std::string error = LoadText("mini-balance-bad", MiniTable(rows, twoBands(balance), ""));
            CHECK_MSG(!error.empty() && Contains(error, needle), "a balance " + balance + " is refused, naming " + needle + ": " + error);
        }
    }
}

// A picture of several phrases: every phrase is in the file's name, and so are the bytes of the face the digits are drawn from, though no script routes to it.
void EnsureNamesEveryPhraseAndTheDigitFace() {
    const World& world = TheWorld();
    if (!world.ok) return;
    StripFixture strip = MakeStrip();
    strip.recipe.text.digitFace = "noto_sans_bold";
    strip.recipe.text.faces.push_back(PB::FaceTuning{"noto_sans_bold", 1.0, 0x78, 0.065});
    const fs::path root = ScratchDir("ensure-bands");
    const fs::path sources = root / "sources";
    fs::create_directories(sources);
    std::string bytes;
    CHECK(PB::EncodePng(strip.picture, bytes));
    WriteText(sources / strip.recipe.sprite, bytes);
    PB::Recipes recipes;
    recipes.items = {strip.recipe};
    recipes.fileHash = "synthetic-strip";
    recipes.loaded = true;
    const fs::path out = root / "pictures";
    const auto wanted = [&](const std::vector<std::string>& phrases, const std::string& fonts) {
        PB::Wanted w;
        w.recipe = &recipes.items[0];
        w.recipes = &recipes;
        w.plan = &TheWorld().languages.Fonts();
        w.fontsDir = fonts;
        w.lang = Lang::De;
        w.languageCode = "de";
        w.phrases = phrases;
        w.sourcePath = (sources / strip.recipe.sprite).string();
        w.directory = out.string();
        return w;
    };
    const std::string fonts = kData + "/" + TheWorld().languages.Fonts().folder;
    const PB::Written first = PB::Ensure(wanted({"alpha - 33", "delta"}, fonts));
    CHECK_MSG(first.ok && !first.cached, "a picture of two phrases is baked: " + first.reason);
    if (!first.ok) return;
    PB::Image baked;
    std::string why;
    CHECK_MSG(PB::ReadImage(first.path, baked, why) && baked.width == 160 && baked.height == 240, "the file is the strip's size");
    CHECK_MSG(PB::Ensure(wanted({"alpha - 33", "delta"}, fonts)).cached, "asked again, the file is found");
    CHECK_MSG(PB::Ensure(wanted({"alpha - 34", "delta"}, fonts)).path != first.path, "another first phrase is another file");
    CHECK_MSG(PB::Ensure(wanted({"alpha - 33", "deltas"}, fonts)).path != first.path, "another second phrase is another file");
    CHECK_MSG(PB::Ensure(wanted({"alpha - 33"}, fonts)).reason.find("phrases") != std::string::npos, "one phrase for two bands fails closed");
    // The digit face's bytes: a copy of the fonts folder with one byte appended to NotoSans-Bold, which no script of the matura slot routes to, is another file here, and is the same
    // file for a recipe that draws no digit from it.
    const FontBake::Plan& plan = TheWorld().languages.Fonts();
    const FontBake::FaceSpec* digits = plan.Face("noto_sans_bold");
    CHECK_MSG(digits != nullptr, "the plan has the digit face");
    if (digits == nullptr) return;
    const fs::path changed = root / "fonts-changed";
    std::error_code copied;
    fs::copy(kData + "/" + plan.folder, changed, fs::copy_options::recursive, copied);
    CHECK_MSG(!copied, "the fonts folder is copied: " + copied.message());
    {
        std::ofstream append(changed / digits->file, std::ios::binary | std::ios::app);
        append.put('\0');
    }
    const PB::Written other = PB::Ensure(wanted({"alpha - 33", "delta"}, changed.string()));
    CHECK_MSG(other.ok && other.path != first.path, "a digit face that changed by one byte is another file: " + other.reason);
    recipes.items[0].text.digitFace.clear();
    recipes.items[0].text.faces.pop_back();
    const PB::Written plain = PB::Ensure(wanted({"alpha - 33", "delta"}, fonts));
    const PB::Written plainChanged = PB::Ensure(wanted({"alpha - 33", "delta"}, changed.string()));
    CHECK_MSG(plain.ok && plainChanged.ok && plain.path == plainChanged.path && plain.path != first.path, "a recipe with no digit face does not depend on that face's bytes");
}

void runFreeCases() {
    TheShippedTableLoadsAsTheElevenPictures();
    TheLoaderRefusesWhatTheNewRulesDoNotAllow();
    APlaqueIsErasedByItsOwnModel();
    ThePlaqueGuardsFailClosedEachByItself();
    APanelIsErasedFromItsLeftMarginAlone();
    AStripClearsItsBandsAndDrawsThemAgainLeftAligned();
    TheStripsAlphaTuningsPitchAnchorAndRightEdgeAreHeld();
    AWrappedHeadingIsBalancedBetweenTheInkAboveAndTheRowBelow();
    ALineGapIsHeldBetweenTheLinesOfABand();
    ThePaintRectangleLetsTheHaloReachPastTheErasedRectangle();
    TheSlopeIsReadOverTheLastTwelveCleanRows();
    AParagraphIsCentredInsideItsBoxWithTheFewestLines();
    AnOrphanLineAndTheLinesSavedAreWeighedByTheRecipe();
    TheBreakMarksOfTheNewPhrasesAreHandled();
    EveryShippedPhraseOfTheNewPicturesIsAboveItsFloor();
    EnsureNamesEveryPhraseAndTheDigitFace();
    TheWordedSpritesAreNamedOnlyWhereALanguageSwitchRemakesThem();
    TheLoaderRefusesWhatItShouldNot();
    ErasingRestoresAKnownPlate();
    TheGuardsRefuseAWrongPicture();
    ThePhraseStaysInItsBoxAndTheRestIsTheSource();
    TheFitIsMonotoneAndStopsAtTheFloor();
    AnArabicWordIsDrawnInTheShapedOrder();
    ASpaceInJapaneseLeavesNoGapAndAZeroWidthSpaceIsNeverDrawn();
    AStackIsCentredOnItsAnchorAndTheMostLinesThatFitAreUsed();
    TheDropShadowIsDrawn();
    TwoStackedLinesStayApart();
    AWordTallerThanItsBoxIsShrunkForItsHeightAlone();
    AnythingWrongKeepsTheEnglishPicture();
    TheBytesAreDeterministicAndThePngIsTheRecipesSize();
    EnsureWritesWholeOrNothing();
    EveryShippedPhraseIsDrawnFromGlyphsItsFacesHold();
}

// ============================================================================================================================
// the second suite: the ORIGINAL's sprites
// ============================================================================================================================

PB::Image ReadOriginal(const std::string& sprite) {
    PB::Image image;
    std::string error;
    CHECK_MSG(PB::ReadImage(kOriginal + "/sprites/hd/" + sprite, image, error), error);
    return image;
}

// MP_PICTUREBAKE_DUMP=<dir> writes every baked picture of this run, as <dir>/<language>/<id>.png, so that a reviewer can look at them. Never by default, and never
// into the repository: the pictures are made from the original's.
std::string DumpDirectory() {
    std::string text;
#ifdef _WIN32
    char* value = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&value, &length, "MP_PICTUREBAKE_DUMP") == 0 && value != nullptr) {
        text = value;
        std::free(value);
    }
#else
    if (const char* value = std::getenv("MP_PICTUREBAKE_DUMP"); value != nullptr) text = value;
#endif
    return text;
}

void EveryRecipesGuardsHoldOnTheRealSprite() {
    const World& world = TheWorld();
    if (!world.ok) return;
    for (const PB::Recipe& recipe : world.recipes.items) {
        const PB::Image source = ReadOriginal(recipe.sprite);
        std::string why;
        const bool checked = PB::CheckSource(recipe, source, why);
        // The digest of a sprite that is not the one measured says what it is, so a fresh table can be made from it: print it when it is wrong.
        CHECK_MSG(checked, recipe.id + ": the sprite is the one the recipe was measured on: " + why);
        if (!checked) continue;
        PB::Image twin;
        const PB::Image* twinPtr = nullptr;
        if (recipe.erase.rule == PB::Rule::Twin) {
            twin = ReadOriginal(recipe.erase.twinSprite);
            twinPtr = &twin;
        }
        PB::Image erased;
        CHECK_MSG(PB::EraseWords(recipe, source, twinPtr, erased, why), recipe.id + ": the erase's guards hold on the real picture: " + why);
        if (erased.Empty()) continue;
        if (recipe.erase.rule == PB::Rule::Generated) continue;
        CHECK_EQ(PixelsChangedOutside(erased, source, recipe.erase.rect), 0);
        // The English word is gone from the rectangle: every pixel of it was replaced by the model (or by the twin's), and a pixel of the rectangle is never the picture's
        // own unless the model agrees.
        const int replaced = [&] {
            int count = 0;
            for (int y = recipe.erase.rect.y0; y < recipe.erase.rect.y1; ++y) {
                for (int x = recipe.erase.rect.x0; x < recipe.erase.rect.x1; ++x) count += std::equal(erased.At(x, y), erased.At(x, y) + 4, source.At(x, y)) ? 0 : 1;
            }
            return count;
        }();
        CHECK_MSG(replaced > 200, recipe.id + ": the rectangle held a word that was replaced: " + std::to_string(replaced) + " pixels differ");
        if (recipe.erase.rule == PB::Rule::RowLerp || recipe.erase.rule == PB::Rule::Separable) {
            // NO INK OF THE ENGLISH WORD SURVIVES (the prototype's residual metric): the model is a straight line along a row (row_lerp) or a constant (separable, in its flat
            // zone), and glyph edges are neither, so the largest second difference along x of the erased window is tiny where the source's is large. A word left in, or half
            // an erase, shows here whatever else holds.
            const bool line = recipe.erase.rule == PB::Rule::RowLerp;
            const int xa = (line ? recipe.erase.rect.x0 : recipe.erase.flatCols.a) + 1;
            const int xb = (line ? recipe.erase.rect.x1 : recipe.erase.flatCols.b) - 1;
            int worstErased = 0;
            int worstSource = 0;
            for (int y = recipe.erase.rect.y0; y < recipe.erase.rect.y1; ++y) {
                for (int x = std::max(xa, recipe.erase.rect.x0 + 1); x < std::min(xb, recipe.erase.rect.x1 - 1); ++x) {
                    for (int c = 0; c < 3; ++c) {
                        const auto second = [&](const PB::Image& image) {
                            return std::abs(static_cast<int>(image.At(x - 1, y)[c]) - 2 * static_cast<int>(image.At(x, y)[c]) + static_cast<int>(image.At(x + 1, y)[c]));
                        };
                        worstErased = std::max(worstErased, second(erased));
                        worstSource = std::max(worstSource, second(source));
                    }
                }
            }
            std::printf("  %-16s residual of the English word: the erased window's sharpest edge is %d grey levels, the source's was %d\n", recipe.id.c_str(), worstErased, worstSource);
            CHECK_MSG(worstErased <= 3 && worstSource >= 30, recipe.id + ": no edge of the English word is left in the erased window: " + std::to_string(worstErased) + " (the source's: " + std::to_string(worstSource) + ")");
        }
        if (recipe.erase.rule == PB::Rule::Twin) {
            // The twin IS the truth: the erased rectangle equals the twin moved, exactly.
            int wrong = 0;
            for (int y = recipe.erase.rect.y0; y < recipe.erase.rect.y1; ++y) {
                for (int x = recipe.erase.rect.x0; x < recipe.erase.rect.x1; ++x) {
                    const int sx = x - recipe.erase.shiftX;
                    const int sy = y - recipe.erase.shiftY;
                    const bool inside = sx >= 0 && sy >= 0 && sx < twin.width && sy < twin.height;
                    const unsigned char* want = inside ? twin.At(sx, sy) : nullptr;
                    static const unsigned char kClear[4] = {0, 0, 0, 0};
                    wrong += std::equal(erased.At(x, y), erased.At(x, y) + 4, want != nullptr ? want : kClear) ? 0 : 1;
                }
            }
            CHECK_MSG(wrong == 0, recipe.id + ": the erased window is the twin's, pixel for pixel: " + std::to_string(wrong) + " differ");
        }
    }
}

// The one sprite with a ground truth: the unshipped sister picture of the achievements' plate differs from it only inside the word's window, so its text-free columns are
// what the erase should have produced. The columns are chosen as the prototype chose them: the ink columns of the sister's own word, widened by six on each side, are left out.
void TheErasedAchievementsPlateAgreesWithItsSisterPicture() {
    const World& world = TheWorld();
    if (!world.ok) return;
    const PB::Recipe* recipe = world.recipes.Find("scores.png");
    CHECK(recipe != nullptr);
    if (recipe == nullptr) return;
    std::error_code ec;
    if (!fs::is_regular_file(kOriginal + "/sprites/hd/credits_.png", ec)) {
        std::printf("  the achievements plate's ground truth SKIPPED - the unshipped sister picture credits_.png is not among the extracted assets\n");
        return;
    }
    const PB::Image source = ReadOriginal(recipe->sprite);
    const PB::Image sister = ReadOriginal("credits_.png");
    PB::Image erased;
    std::string why;
    CHECK_MSG(PB::EraseWords(*recipe, source, nullptr, erased, why), why);
    CHECK_MSG(sister.width == source.width && sister.height == source.height, "the sister is the same size");
    if (erased.Empty() || sister.width != source.width) return;
    // The sister differs from the achievements' picture only inside its own word's window.
    PB::Rect differs{source.width, source.height, 0, 0};
    for (int y = 0; y < source.height; ++y) {
        for (int x = 0; x < source.width; ++x) {
            if (std::equal(source.At(x, y), source.At(x, y) + 4, sister.At(x, y))) continue;
            differs.x0 = std::min(differs.x0, x);
            differs.y0 = std::min(differs.y0, y);
            differs.x1 = std::max(differs.x1, x + 1);
            differs.y1 = std::max(differs.y1, y + 1);
        }
    }
    CHECK_MSG(differs.x0 >= 24 && differs.x1 <= 235 && differs.y0 >= 18 && differs.y1 <= 49, "the sister differs from the plate only inside x 24..234, y 18..48");
    // The sister's ink columns, from the brightness of its own word.
    int inkFirst = source.width;
    int inkLast = -1;
    for (int x = 24; x < 235; ++x) {
        for (int y = 18; y < 49; ++y) {
            const unsigned char* px = sister.At(x, y);
            if (std::max({px[0], px[1], px[2]}) > 150) {
                inkFirst = std::min(inkFirst, x);
                inkLast = std::max(inkLast, x);
            }
        }
    }
    CHECK_MSG(inkLast > inkFirst, "the sister has a word of its own");
    double sum = 0.0;
    int worst = 0;
    int count = 0;
    for (int y = recipe->erase.rect.y0; y < recipe->erase.rect.y1; ++y) {
        for (int x = recipe->erase.rect.x0; x < recipe->erase.rect.x1; ++x) {
            if (x >= inkFirst - 6 && x <= inkLast + 6) continue;
            int d = 0;
            for (int c = 0; c < 4; ++c) d = std::max(d, std::abs(static_cast<int>(erased.At(x, y)[c]) - static_cast<int>(sister.At(x, y)[c])));
            sum += d;
            worst = std::max(worst, d);
            ++count;
        }
    }
    const double mean = count > 0 ? sum / count : 0.0;
    std::printf("  the erased achievements plate against its sister picture, on %d text-free pixels: mean %.2f, worst %d grey levels\n", count, mean, worst);
    CHECK_MSG(count > 2500, "enough text-free pixels to say anything: " + std::to_string(count));
    CHECK_MSG(mean <= 2.0 && worst <= 4, "the erase is the sister's plate to a mean of 2 and at worst 4 levels: " + std::to_string(mean) + ", " + std::to_string(worst));
}

// The request for a recipe in a language, whichever kind of recipe: one phrase, or one phrase a band. `holder` keeps the phrase alive.
PB::Request RequestFromTable(const PB::Recipe& recipe, Lang lang, const Strings::Table& table, std::string& holder) {
    holder = recipe.key.empty() ? std::string() : table.Get(recipe.key);
    PB::Request request = RequestFor(recipe, lang, holder);
    for (const PB::Band& band : recipe.text.bands) request.phrases.push_back(table.Get(band.key));
    return request;
}

// What a real strip's bands must be in every language (planning step 249). The LEADING: two lines of a band keep the recipe's gap of the band's px apart, and in any case at least
// kLeastLineGap free hd rows (the original's own English design band keeps 3 free rows between its lines, 2.0 by distance, at the same x-height). The HEADING: a heading of two lines
// has as many free rows above it, under the ink of the role line above, as below it, above the row of the first name, to within kBalanceTolerance (the rows an ink box at the recipe's
// threshold and a rounded baseline can differ by), and both are ten rows at least; a heading of one line keeps its last baseline, the recipe's 565.
constexpr double kLeastLineGap = 1.5;
constexpr double kBalanceTolerance = 1.0;
constexpr double kLeastHeadingGap = 10.0;

void CheckTheStripsBands(const PB::Recipe& recipe, const PB::Outcome& a, const std::string& name) {
    CHECK_MSG(a.bands.size() == recipe.text.bands.size(), name + ": a result a band");
    if (a.bands.size() != recipe.text.bands.size()) return;
    for (std::size_t i = 0; i < a.bands.size(); ++i) {
        const PB::Band& band = recipe.text.bands[i];
        const PB::BandResult& result = a.bands[i];
        if (result.lines >= 2) {
            const double wanted = recipe.text.gap * band.px * result.scale;
            CHECK_MSG(result.lineGap < PB::kNoLineGap && result.lineGap >= wanted - 0.5 && result.lineGap >= kLeastLineGap,
                      name + ": band " + std::to_string(i) + " keeps its lines apart: " + std::to_string(result.lineGap) + " hd rows, the gap " + std::to_string(wanted) + ", at least " + std::to_string(kLeastLineGap));
        } else {
            CHECK_MSG(result.lineGap == PB::kNoLineGap, name + ": band " + std::to_string(i) + " of one line has no gap");
        }
        if (!band.balanced) continue;
        const PB::BandResult& above = a.bands[static_cast<std::size_t>(band.balanceAbove)];
        const double freeAbove = result.ink.y0 - above.ink.y1;
        const double freeBelow = band.balanceBelow - result.ink.y1;
        if (result.lines >= band.balanceMinLines) {
            CHECK_MSG(std::fabs(freeAbove - freeBelow) <= kBalanceTolerance && std::min(freeAbove, freeBelow) >= kLeastHeadingGap,
                      name + ": the wrapped heading is between its neighbours: " + std::to_string(freeAbove) + " free rows above, " + std::to_string(freeBelow) + " below");
            std::printf("  %s: the wrapped heading has %.0f free rows above it and %.0f below\n", name.c_str(), freeAbove, freeBelow);
        } else {
            // The heading's last glyph stands on the recipe's baseline.
            double last = -1.0;
            for (const PB::Placed& placed : a.placed) last = placed.baseline;
            CHECK_MSG(std::fabs(last - band.firstBaseline) < 0.3, name + ": a heading of one line keeps its baseline " + std::to_string(band.firstBaseline) + ": " + std::to_string(last));
        }
    }
}

void AllTenLanguagesBakeEveryPictureAboveItsFloor() {
    const World& world = TheWorld();
    if (!world.ok) return;
    const std::string dump = DumpDirectory();
    for (const PB::Recipe& recipe : world.recipes.items) {
        const PB::Image source = ReadOriginal(recipe.sprite);
        PB::Image twin;
        const PB::Image* twinPtr = nullptr;
        if (recipe.erase.rule == PB::Rule::Twin) {
            twin = ReadOriginal(recipe.erase.twinSprite);
            twinPtr = &twin;
        }
        // The plate as the erase leaves it, to tell the shadow (what the bake adds round the ink) from the plate.
        PB::Image erased;
        if (recipe.erase.rule != PB::Rule::Generated) {
            std::string why;
            CHECK_MSG(PB::EraseWords(recipe, source, twinPtr, erased, why), recipe.id + ": " + why);
        }
        double tightest = 9.0;
        std::string tightestLanguage;
        double closest = PB::kNoLineGap; // the least distance between two lines' ink, over the languages
        double lowestRow = 0.0;          // the lowest row any visible pixel reaches, over the languages
        for (std::size_t i = 1; i < Strings::kLangCount; ++i) {
            const Lang lang = static_cast<Lang>(i);
            Strings::Table table;
            std::string error;
            CHECK_MSG(table.Load(kData, lang, error), error);
            std::string holder;
            const PB::Request request = RequestFromTable(recipe, lang, table, holder);
            const PB::Outcome a = PB::Render(request, source, twinPtr);
            const std::string name = recipe.id + " in " + Code(lang);
            CHECK_MSG(a.ok, name + " is baked: " + a.reason);
            if (!a.ok) continue;
            CHECK_MSG(a.scale >= recipe.text.minScale - 1e-9, name + " is at or above the floor: " + std::to_string(a.scale));
            if (a.scale < tightest) {
                tightest = a.scale;
                tightestLanguage = Code(lang);
            }
            CHECK_MSG(a.image.width == source.width && a.image.height == source.height, name + " has the source's size");
            std::string png;
            CHECK_MSG(PB::EncodePng(a.image, png), name + " encodes");
            if (recipe.erase.rule != PB::Rule::Generated) {
                // Every pixel outside what the bake says it touched (the erase's rectangle, and where the shadow may reach beyond it) is the source's.
                CHECK_MSG(PixelsChangedOutside(a.image, source, a.touched) == 0, name + ": nothing outside the rectangle changed");
            } else {
                // No ink of the original is in a generated picture: the outermost two pixels hold no more than the faintest shadow, and of the soft pixels fewer than two in a
                // hundred equal the source's (white ink equals white ink wherever it is, so the opaque ones are left out).
                int ring = 0;
                int soft = 0;
                int same = 0;
                for (int y = 0; y < a.image.height; ++y) {
                    for (int x = 0; x < a.image.width; ++x) {
                        const unsigned char* px = a.image.At(x, y);
                        if (x < 2 || y < 2 || x >= a.image.width - 2 || y >= a.image.height - 2) ring = std::max(ring, static_cast<int>(px[3]));
                        if (px[3] > 0 && px[3] < 255) {
                            ++soft;
                            same += std::equal(px, px + 4, source.At(x, y)) ? 1 : 0;
                        }
                    }
                }
                CHECK_MSG(ring <= 4, name + ": the shadow is not cut by the picture's edge: " + std::to_string(ring));
                CHECK_MSG(soft > 0 && same * 50 < soft, name + ": no pixel of the original is reused: " + std::to_string(same) + " of " + std::to_string(soft));
            }
            // THE SHADOW is there: pixels outside the ink's box that the bake made (a generated picture) or changed (a plate), by more than the ink's own antialiasing. (The strip's
            // ink has none.)
            int shaded = 0;
            int lowest = -1;
            for (int y = 0; y < a.image.height; ++y) {
                for (int x = 0; x < a.image.width; ++x) {
                    const unsigned char* px = a.image.At(x, y);
                    if (recipe.erase.rule == PB::Rule::Generated && px[3] >= kVisibleAlpha) lowest = y; // a plate is opaque to its last row: only a generated picture has a bottom
                    if (a.ink.Contains(x, y)) continue;
                    if (recipe.erase.rule == PB::Rule::Generated) {
                        shaded += px[3] >= 10 ? 1 : 0;
                    } else if (!erased.Empty()) {
                        int d = 0;
                        for (int c = 0; c < 4; ++c) d = std::max(d, std::abs(static_cast<int>(px[c]) - static_cast<int>(erased.At(x, y)[c])));
                        shaded += d >= (recipe.text.layout == PB::Layout::Paragraph ? 3 : 8) ? 1 : 0; // a halo darkens a dark plate by a few levels
                    }
                }
            }
            if (recipe.text.layout != PB::Layout::Bands) CHECK_MSG(shaded >= 40, name + ": a drop shadow is drawn round the ink: " + std::to_string(shaded) + " pixels");
            lowestRow = std::max(lowestRow, static_cast<double>(lowest));
            // TWO LINES NEVER TOUCH: the ink of the second is at least the recipe's gap (less the half pixel a whole step of the pitch and a rounded column can take) below the first's.
            if (a.lines == 2 && recipe.text.layout != PB::Layout::Bands) {
                const double wanted = recipe.text.gap * recipe.text.px * a.scale;
                CHECK_MSG(a.lineGap > 0.0 && a.lineGap < PB::kNoLineGap && a.lineGap >= wanted - 0.5, name + ": the two lines' ink is at least the gap apart: " + std::to_string(a.lineGap) + " px, the gap " + std::to_string(wanted));
                closest = std::min(closest, a.lineGap - wanted);
            }
            if (recipe.text.layout == PB::Layout::Bands) CheckTheStripsBands(recipe, a, name);
            // Twice is the same bytes.
            const PB::Outcome b = PB::Render(request, source, twinPtr);
            std::string again;
            CHECK_MSG(b.ok && PB::EncodePng(b.image, again) && again == png, name + ": two bakes are the same bytes");
            if (!dump.empty()) {
                const fs::path dir = fs::path(dump) / Code(lang);
                std::error_code ec;
                fs::create_directories(dir, ec);
                WriteText(dir / (recipe.id + ".png"), png);
            }
        }
        std::printf("  %-16s tightest scale %.3f (%s), floor %.2f", recipe.id.c_str(), tightest, tightestLanguage.c_str(), recipe.text.minScale);
        if (recipe.erase.rule == PB::Rule::Generated) std::printf("; lowest visible row %.0f", lowestRow);
        if (closest < PB::kNoLineGap) std::printf("; two lines at least %+.2f px beyond the gap", closest);
        std::printf("\n");
        if (recipe.id == "game_over") {
            // THE LOST SCREEN: the restart and list buttons begin at hd row kRestartButtonTopRow of this picture, and no language's title - ink or shadow - may reach them.
            CHECK_MSG(lowestRow < kRestartButtonTopRow, recipe.id + ": the title (its shadow too) ends above the lost screen's buttons: row " + std::to_string(static_cast<int>(lowestRow)));
        }
        CHECK_MSG(tightest >= recipe.text.minScale, recipe.id + ": every language is at or above the floor");
    }
    if (!dump.empty()) std::printf("  the baked pictures were written under %s (MP_PICTUREBAKE_DUMP): delete them when done, they are made from the original's\n", dump.c_str());
}

void TheBakeIsCheap() {
    const World& world = TheWorld();
    if (!world.ok) return;
    // Milliseconds per picture, the least of three runs (other processes share the machine), printed and bounded generously: this is a pin against a hundredfold, not a benchmark.
    Strings::Table russian;
    std::string error;
    CHECK(russian.Load(kData, Lang::Ru, error));
    for (const PB::Recipe& recipe : world.recipes.items) {
        const PB::Image source = ReadOriginal(recipe.sprite);
        PB::Image twin;
        const PB::Image* twinPtr = nullptr;
        if (recipe.erase.rule == PB::Rule::Twin) {
            twin = ReadOriginal(recipe.erase.twinSprite);
            twinPtr = &twin;
        }
        std::string holder;
        const PB::Request request = RequestFromTable(recipe, Lang::Ru, russian, holder);
        double best = 1e9;
        for (int run = 0; run < 3; ++run) {
            const auto began = std::chrono::steady_clock::now();
            const PB::Outcome outcome = PB::Render(request, source, twinPtr);
            std::string png;
            const bool encoded = outcome.ok && PB::EncodePng(outcome.image, png);
            const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
            CHECK_MSG(encoded, recipe.id + " bakes in Russian: " + outcome.reason);
            best = std::min(best, ms);
        }
        std::printf("  %-16s bake and encode: %.1f ms (least of three, Russian)\n", recipe.id.c_str(), best);
        CHECK_MSG(best < 2000.0, recipe.id + " bakes in well under two seconds: " + std::to_string(best));
        // WHAT A CACHE HIT COSTS (planning step 249): Ensure decodes the sprite and checks its size and digest, reads every face, and hashes the bytes of the faces the slot uses
        // before it looks for the file - the safety of step 247, which this step leaves as it is. The least of three finds of the file the first Ensure wrote.
        PB::Wanted wanted;
        wanted.recipe = &recipe;
        wanted.recipes = &world.recipes;
        wanted.plan = &world.languages.Fonts();
        wanted.fontsDir = kData + "/" + wanted.plan->folder;
        wanted.lang = Lang::Ru;
        wanted.languageCode = "ru";
        if (!recipe.key.empty()) wanted.phrase = russian.Get(recipe.key);
        for (const PB::Band& band : recipe.text.bands) wanted.phrases.push_back(russian.Get(band.key));
        wanted.sourcePath = kOriginal + "/sprites/hd/" + recipe.sprite;
        wanted.twinPath = kOriginal + "/sprites/hd/" + recipe.erase.twinSprite;
        wanted.directory = ScratchDir("cache-hit-cost").string();
        const PB::Written made = PB::Ensure(wanted);
        CHECK_MSG(made.ok && !made.cached, recipe.id + ": the first Ensure bakes: " + made.reason);
        double found = 1e9;
        for (int run = 0; run < 3; ++run) {
            const auto began = std::chrono::steady_clock::now();
            const PB::Written again = PB::Ensure(wanted);
            const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
            CHECK_MSG(again.ok && again.cached && again.path == made.path, recipe.id + ": the second Ensure finds the file: " + again.reason);
            found = std::min(found, ms);
        }
        std::printf("  %-16s a cache hit (the file is found again): %.1f ms (least of three, Russian)\n", recipe.id.c_str(), found);
    }
}

// ---- the four pictures of step 248, on the real sprites -----------------------------------------------------------------------------------------------

int OpaquePixels(const PB::Image& image, const PB::Rect& where) {
    int count = 0;
    for (int y = where.y0; y < where.y1; ++y) {
        for (int x = where.x0; x < where.x1; ++x) count += image.At(x, y)[3] != 0 ? 1 : 0;
    }
    return count;
}

bool SameRows(const PB::Image& a, const PB::Image& b, int y0, int y1) {
    for (int y = y0; y < y1; ++y) {
        if (!std::equal(a.At(0, y), a.At(0, y) + a.width * 4, b.At(0, y))) return false;
    }
    return true;
}

void ThePlaquesAndTheCardAreErasedClean() {
    const World& world = TheWorld();
    if (!world.ok) return;
    // The golden plaque is the current plaque 55 rows lower, with a medal: the current plaque's text-free rows are the golden erase's OFFLINE ground truth (never a donor).
    const PB::Recipe* golden = world.recipes.Find("golden_score_plaque.png");
    const PB::Recipe* current = world.recipes.Find("current_score_plaque.png");
    const PB::Recipe* card = world.recipes.Find("game_finished.png");
    CHECK(golden != nullptr && current != nullptr && card != nullptr);
    if (golden == nullptr || current == nullptr || card == nullptr) return;
    const PB::Image goldenSource = ReadOriginal(golden->sprite);
    const PB::Image currentSource = ReadOriginal(current->sprite);
    PB::Image goldenErased;
    PB::Image currentErased;
    std::string why;
    CHECK_MSG(PB::EraseWords(*golden, goldenSource, nullptr, goldenErased, why), "the golden plaque is erased: " + why);
    CHECK_MSG(PB::EraseWords(*current, currentSource, nullptr, currentErased, why), "the current plaque is erased: " + why);
    if (goldenErased.Empty() || currentErased.Empty()) return;
    int interiorWorst = 0;
    double interiorSum = 0.0;
    int interiorCount = 0;
    int frameWorst = 0;
    for (int y = 100; y < 156; ++y) {
        for (int x = 2; x < 126; ++x) {
            if (IsKept(goldenSource, x, y, 2, 10)) continue;
            int d = 0;
            for (int c = 0; c < 3; ++c) d = std::max(d, std::abs(static_cast<int>(goldenErased.At(x, y)[c]) - static_cast<int>(currentSource.At(x, y - 55)[c])));
            if (x >= 13 && x < 115) {
                interiorWorst = std::max(interiorWorst, d);
                interiorSum += d;
                ++interiorCount;
            } else {
                frameWorst = std::max(frameWorst, d);
            }
        }
    }
    std::printf("  the golden erase against the current plaque's text-free rows: interior mean %.2f, worst %d; frame columns worst %d\n", interiorSum / std::max(1, interiorCount), interiorWorst, frameWorst);
    CHECK_MSG(interiorWorst <= 2 && interiorSum / std::max(1, interiorCount) < 0.7, "the golden erase is the plate's own text-free rows to 2 grey levels inside (the prototype's 0.45 mean)");
    CHECK_MSG(frameWorst == 0, "and exactly on the frame columns: " + std::to_string(frameWorst));
    // The medal: no chromatic pixel of the golden plaque changed.
    int chromatic = 0;
    int changed = 0;
    for (int y = 0; y < goldenSource.height; ++y) {
        for (int x = 0; x < goldenSource.width; ++x) {
            const unsigned char* px = goldenSource.At(x, y);
            if (px[3] == 0 || std::max({px[0], px[1], px[2]}) - std::min({px[0], px[1], px[2]}) <= 10) continue;
            ++chromatic;
            changed += std::equal(px, px + 4, goldenErased.At(x, y)) ? 0 : 1;
        }
    }
    CHECK_MSG(chromatic > 1400 && changed == 0, "the medal is untouched by the erase: " + std::to_string(chromatic) + " chromatic pixels, " + std::to_string(changed) + " changed");
    // No edge of the old lettering is left: along a row of the erased interior (below the clean rows) the largest second difference is a few levels; the source's was far more. Down a
    // frame column of the rectangle the colour does not change at all.
    for (const PB::Recipe* recipe : {current, golden}) {
        const PB::Image& source = recipe == current ? currentSource : goldenSource;
        const PB::Image& erased = recipe == current ? currentErased : goldenErased;
        int worstErased = 0;
        int worstSource = 0;
        const int y0 = recipe == current ? 100 : 112; // the golden's rows under the ribbon
        const int y1 = recipe == current ? 140 : 190;
        for (int y = y0; y < y1; ++y) {
            for (int x = 40; x < 105; ++x) {
                if (IsKept(source, x, y, 2, 10) || IsKept(source, x - 1, y, 2, 10) || IsKept(source, x + 1, y, 2, 10)) continue;
                for (int c = 0; c < 3; ++c) {
                    worstErased = std::max(worstErased, std::abs(erased.At(x - 1, y)[c] - 2 * erased.At(x, y)[c] + erased.At(x + 1, y)[c]));
                    worstSource = std::max(worstSource, std::abs(source.At(x - 1, y)[c] - 2 * source.At(x, y)[c] + source.At(x + 1, y)[c]));
                }
            }
        }
        std::printf("  %-14s residual of the old lettering: the erased rows' sharpest edge is %d grey levels, the source's was %d\n", recipe->id.c_str(), worstErased, worstSource);
        CHECK_MSG(worstErased <= 3 && worstSource >= 30, recipe->id + ": no edge of the old lettering is left: " + std::to_string(worstErased) + " (the source's " + std::to_string(worstSource) + ")");
        int frameSteps = 0;
        for (int x = 2; x < 13; ++x) {
            for (int y = y0; y + 1 < y1; ++y) {
                for (int c = 0; c < 3; ++c) frameSteps += erased.At(x, y)[c] != erased.At(x, y + 1)[c] ? 1 : 0;
            }
        }
        CHECK_MSG(frameSteps == 0, recipe->id + ": a frame column is constant down the erased rows: " + std::to_string(frameSteps) + " steps");
    }
    // Each plaque refuses the other's picture (the real sprites, so the real guards).
    PB::Image out;
    CHECK_MSG(!PB::EraseWords(*current, goldenSource, nullptr, out, why), "the golden plaque under the current recipe is refused: " + why);
    CHECK_MSG(!PB::EraseWords(*golden, currentSource, nullptr, out, why), "the current plaque under the golden recipe is refused: " + why);
    std::printf("  (both refusals: %s)\n", why.c_str());
    // The card: no edge of the two paragraphs is left in its rectangle, and the rows outside it are the picture's.
    const PB::Image cardSource = ReadOriginal(card->sprite);
    PB::Image cardErased;
    CHECK_MSG(PB::EraseWords(*card, cardSource, nullptr, cardErased, why), "the card is erased: " + why);
    if (!cardErased.Empty()) {
        int worstErased = 0;
        int worstSource = 0;
        for (int y = 170; y < 345; ++y) {
            for (int x = 130; x < 300; ++x) {
                for (int c = 0; c < 3; ++c) {
                    worstErased = std::max(worstErased, std::abs(cardErased.At(x - 1, y)[c] - 2 * cardErased.At(x, y)[c] + cardErased.At(x + 1, y)[c]));
                    worstSource = std::max(worstSource, std::abs(cardSource.At(x - 1, y)[c] - 2 * cardSource.At(x, y)[c] + cardSource.At(x + 1, y)[c]));
                }
            }
        }
        std::printf("  game_finished  residual of the old paragraphs: %d grey levels, the source's was %d\n", worstErased, worstSource);
        CHECK_MSG(worstErased <= 3 && worstSource >= 30, "no edge of the card's old paragraphs is left: " + std::to_string(worstErased) + " (the source's " + std::to_string(worstSource) + ")");
        CHECK_EQ(PixelsChangedOutside(cardErased, cardSource, card->erase.rect), 0);
    }
}

void TheStripKeepsItsNamesAndLogosAndTheMedalStaysOnTheBakedPlaque() {
    const World& world = TheWorld();
    if (!world.ok) return;
    const PB::Recipe* strip = world.recipes.Find("credit_name.png");
    CHECK(strip != nullptr);
    if (strip == nullptr) return;
    const PB::Image source = ReadOriginal(strip->sprite);
    PB::Image cleared;
    std::string why;
    CHECK_MSG(PB::EraseWords(*strip, source, nullptr, cleared, why), "the strip's bands are cleared: " + why);
    if (cleared.Empty()) return;
    int total = 0;
    int inBands = 0;
    for (int y = 0; y < source.height; ++y) {
        for (int x = 0; x < source.width; ++x) {
            if (source.At(x, y)[3] == 0) continue;
            ++total;
            for (const PB::Span& span : strip->erase.clearRows) inBands += (y >= span.a && y < span.b) ? 1 : 0;
        }
    }
    CHECK_MSG(total == 88150 && inBands == 26364, "the cleared rows hold 26,364 of the strip's 88,150 non-transparent pixels (the prototype's count): " + std::to_string(inBands) + " of " + std::to_string(total));
    // The company line (rows 328..358) and the Ethanon and Asantee logos (rows 798..892 and 953..1022) are bit-identical in every language's strip; so is every pixel outside the windows.
    CHECK_MSG(OpaquePixels(source, PB::Rect{0, 328, source.width, 359}) > 500 && OpaquePixels(source, PB::Rect{0, 798, source.width, 893}) > 2000 && OpaquePixels(source, PB::Rect{0, 953, source.width, 1023}) > 2000,
              "the rows kept (the company line and the two logos) hold opaque pixels in the original");
    for (std::size_t i = 1; i < Strings::kLangCount; ++i) {
        const Lang lang = static_cast<Lang>(i);
        Strings::Table table;
        std::string error;
        CHECK_MSG(table.Load(kData, lang, error), error);
        std::string holder;
        const PB::Outcome o = PB::Render(RequestFromTable(*strip, lang, table, holder), source, nullptr);
        CHECK_MSG(o.ok, Code(lang) + ": the strip is baked: " + o.reason);
        if (!o.ok) continue;
        const std::string name = "the strip in " + Code(lang);
        CHECK_MSG(SameRows(o.image, source, 328, 359) && SameRows(o.image, source, 798, 893) && SameRows(o.image, source, 953, 1023), name + ": the company line and both logos are the original's, pixel for pixel");
        // The four names and 'Daniel and Carol Santee' (with its 'and') sit between the bands: every row outside every window is the source's.
        int rowsDiffer = 0;
        for (int y = 0; y < source.height; ++y) {
            bool window = false;
            for (const PB::Band& band : strip->text.bands) window = window || (y >= band.inkRows.a && y < band.inkRows.b);
            if (!window && !SameRows(o.image, source, y, y + 1)) ++rowsDiffer;
        }
        CHECK_MSG(rowsDiffer == 0, name + ": every row outside the bands' windows is the original's: " + std::to_string(rowsDiffer) + " differ");
        // The new ink is black with alpha = coverage, and each band got ink.
        int notBlack = 0;
        for (const PB::BandResult& band : o.bands) {
            CHECK_MSG(band.ink.Valid() && band.ink.x0 >= 63 && band.ink.x0 <= 66 && band.ink.x1 <= 470, name + ": " + band.key + " starts at the original's left edge and ends inside the right one: " + std::to_string(band.ink.x0) + ".." + std::to_string(band.ink.x1));
        }
        int maxAlpha = 0;
        for (const PB::Span& span : strip->erase.clearRows) {
            for (int y = span.a; y < span.b; ++y) {
                for (int x = 0; x < o.image.width; ++x) {
                    const unsigned char* px = o.image.At(x, y);
                    notBlack += (px[3] != 0 && (px[0] != 0 || px[1] != 0 || px[2] != 0)) ? 1 : 0;
                    maxAlpha = std::max(maxAlpha, static_cast<int>(px[3]));
                }
            }
        }
        CHECK_MSG(notBlack == 0, name + ": the new ink is black: " + std::to_string(notBlack) + " pixels are not");
        // The alpha is the coverage and is NOT multiplied by the strip's own 230 (the layer draws the picture at 230): where a stem covers a pixel whole, its alpha is 255.
        CHECK_MSG(maxAlpha == 255, name + ": the ink's alpha is its coverage, 255 at most and at least: " + std::to_string(maxAlpha));
        // The heading ('Special thanks') of ONE line stands on the original's baseline, row 565 (de, tr, ja, ar: planning step 249 moves no heading of one line). A heading of two lines
        // grew upward from that baseline until step 249 and is now placed between its neighbours (CheckTheStripsBands, which holds it to equal gaps above and below).
        CHECK_MSG(!o.bands.empty() && !o.placed.empty() && (o.bands.back().lines >= 2 || std::fabs(o.placed.back().baseline - 565.0) < 0.5),
                  name + ": a heading of one line stands on baseline 565: " + std::to_string(o.placed.empty() ? 0.0 : o.placed.back().baseline));
        std::printf("  the strip in %s: scale and lines of the five bands:", Code(lang).c_str());
        for (const PB::BandResult& band : o.bands) std::printf(" %.3f/%d", band.scale, band.lines);
        std::printf("; free rows between the lines:");
        for (const PB::BandResult& band : o.bands) {
            if (band.lineGap < PB::kNoLineGap) std::printf(" %.2f", band.lineGap);
            else std::printf(" -");
        }
        std::printf("; the heading's ink rows %d..%d\n", o.bands.back().ink.y0, o.bands.back().ink.y1);
    }
}

// The medal is drawn OVER the current plaque, 8 hd rows below its top (the medal's centre is 28 units above the plaque's, and the sprites are 64 units a side at density 2): its
// opaque pixels (alpha 128 and over: the medal's body and its ribbon, which end at medal row 105, plaque row 113) must hide none of the translated ink, in every language and for all
// three medals. The medal's soft shadow (alpha 8 to 127, to medal row 118) does fall on the first rows of the ink and dims it a little, as it dims the original's lettering: counted
// and printed, not refused.
void TheMedalIsDrawnOverTheTranslatedPlaqueAndHidesNoInk() {
    const World& world = TheWorld();
    if (!world.ok) return;
    const PB::Recipe* current = world.recipes.Find("current_score_plaque.png");
    CHECK(current != nullptr);
    if (current == nullptr) return;
    const PB::Image source = ReadOriginal(current->sprite);
    PB::Image erased;
    std::string why;
    CHECK_MSG(PB::EraseWords(*current, source, nullptr, erased, why), why);
    int checkedInk = 0;
    int softest = 0;
    for (const char* medalName : {"medal_bronze_l.png", "medal_silver_l.png", "medal_gold_l.png"}) {
        const PB::Image medal = ReadOriginal(medalName);
        CHECK_MSG(medal.width == 128 && medal.height == 128, std::string(medalName) + " is 64 x 64 units at density 2");
        for (std::size_t i = 1; i < Strings::kLangCount; ++i) {
            const Lang lang = static_cast<Lang>(i);
            Strings::Table table;
            std::string error;
            CHECK_MSG(table.Load(kData, lang, error), error);
            std::string holder;
            const PB::Outcome o = PB::Render(RequestFromTable(*current, lang, table, holder), source, nullptr);
            CHECK_MSG(o.ok, Code(lang) + ": the current plaque is baked: " + o.reason);
            if (!o.ok) continue;
            int hidden = 0;
            int dimmed = 0;
            int ink = 0;
            for (int y = 0; y < medal.height; ++y) {
                for (int x = 0; x < medal.width; ++x) {
                    const int py = y + 8;
                    if (py >= o.image.height || medal.At(x, y)[3] < 8) continue;
                    const bool opaque = medal.At(x, y)[3] >= 128;
                    // A pixel of white ink: much brighter than the erased plate under it.
                    const unsigned char* baked = o.image.At(x, py);
                    const unsigned char* plate = erased.At(x, py);
                    if (std::max({baked[0], baked[1], baked[2]}) - std::max({plate[0], plate[1], plate[2]}) >= 40) (opaque ? hidden : dimmed) += 1;
                }
            }
            for (int y = o.ink.y0; y < o.ink.y1; ++y) {
                for (int x = o.ink.x0; x < o.ink.x1; ++x) ink += o.image.At(x, y)[0] > 230 ? 1 : 0;
            }
            checkedInk += ink;
            softest = std::max(softest, dimmed);
            CHECK_MSG(hidden == 0 && ink > 100, std::string(medalName) + " over the plaque in " + Code(lang) + " hides none of the translated ink: " + std::to_string(hidden) + " ink pixels under it of " + std::to_string(ink));
        }
    }
    std::printf("  the medals' soft shadows fall on at most %d ink pixels of the translated plaque (they dim them, hide none)\n", softest);
    CHECK_MSG(checkedInk > 3000, "the test looked at ink");
}

// The golden plaque's medal is part of its sprite and the translated ink and halo are drawn over the plaque: the medal's own (chromatic) pixels must come out bit for bit the sprite's
// in every language (the halo reaches about five rows above the ink, and the ribbon's tips end four rows above the ink box).
void TheGoldenMedalSurvivesTheTranslatedWords() {
    const World& world = TheWorld();
    if (!world.ok) return;
    const PB::Recipe* golden = world.recipes.Find("golden_score_plaque.png");
    CHECK(golden != nullptr);
    if (golden == nullptr) return;
    const PB::Image source = ReadOriginal(golden->sprite);
    int overall = 0;
    for (std::size_t i = 1; i < Strings::kLangCount; ++i) {
        const Lang lang = static_cast<Lang>(i);
        Strings::Table table;
        std::string error;
        CHECK_MSG(table.Load(kData, lang, error), error);
        std::string holder;
        const PB::Outcome o = PB::Render(RequestFromTable(*golden, lang, table, holder), source, nullptr);
        CHECK_MSG(o.ok, Code(lang) + ": the golden plaque is baked: " + o.reason);
        if (!o.ok) continue;
        int worst = 0;
        int changed = 0;
        for (int y = 0; y < source.height; ++y) {
            for (int x = 0; x < source.width; ++x) {
                const unsigned char* px = source.At(x, y);
                if (px[3] == 0 || std::max({px[0], px[1], px[2]}) - std::min({px[0], px[1], px[2]}) <= 10) continue;
                int d = 0;
                for (int c = 0; c < 3; ++c) d = std::max(d, std::abs(static_cast<int>(o.image.At(x, y)[c]) - static_cast<int>(px[c])));
                worst = std::max(worst, d);
                changed += d > 0 ? 1 : 0;
            }
        }
        overall = std::max(overall, worst);
        CHECK_MSG(worst == 0, Code(lang) + ": the golden medal's own pixels are the sprite's, bit for bit, under the halo of the words (the ink box starts four rows under the ribbon's tips): " + std::to_string(worst) + " (" + std::to_string(changed) + " pixels differ)");
    }
    std::printf("  the golden medal's pixels are the sprite's in all ten languages (the largest change is %d grey levels)\n", overall);
}

void runOriginalCases() {
    EveryRecipesGuardsHoldOnTheRealSprite();
    TheErasedAchievementsPlateAgreesWithItsSisterPicture();
    AllTenLanguagesBakeEveryPictureAboveItsFloor();
    ThePlaquesAndTheCardAreErasedClean();
    TheStripKeepsItsNamesAndLogosAndTheMedalStaysOnTheBakedPlaque();
    TheMedalIsDrawnOverTheTranslatedPlaqueAndHidesNoInk();
    TheGoldenMedalSurvivesTheTranslatedWords();
    TheBakeIsCheap();
}

} // namespace

static constexpr int kSkippedExit = 77; // the suites' own code for "skipped": SKIPS_WITH_77, ctest's SKIP_RETURN_CODE (tests/test_mp.cpp)

static int freeEntry(int, char**) {
    runFreeCases();
    return ::test::summary("test_mp_picturebake", 400);
}

static int originalEntry(int, char**) {
    std::error_code ec;
    if (kOriginal.empty() || !fs::is_regular_file(kOriginal + "/sprites/hd/main_play_game_button.png", ec)) {
        std::printf("test_mp_picturebake_original: SKIPPED - needs the original's extracted assets at %s (no check was run)\n", kOriginal.c_str());
        return kSkippedExit;
    }
    runOriginalCases();
    return ::test::summary("test_mp_picturebake_original", 300);
}

static const ::MpSuite::Registration freeRegistration("test_mp_picturebake", &freeEntry);
static const ::MpSuite::Registration originalRegistration("test_mp_picturebake_original", &originalEntry);
