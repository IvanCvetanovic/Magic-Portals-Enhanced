// The words the original draws as pixels, drawn again in the language on the screen (planning step 247, LANG-2): sim/PictureBake.
//
// TWO SUITES. test_mp_picturebake is PURE: hand-authored synthetic pictures built in code (a procedural plate with a procedural 'ink' word and shadow standing in for an
// original sprite, a transparent canvas), the committed faces and the committed tables - nothing outside this repository. test_mp_picturebake_original runs the same code
// over the ORIGINAL's seven sprites (the extracted assets, gitignored) and exits 77 without them.
//
//   table        game/data/picture_recipes.json loads as the seven pictures it should, each with its string key, and the loader REFUSES, one fault at a time, a missing member,
//                a size of zero, an upside-down rectangle, an unknown rule or layout or member, a digest that is not hex, a repeated id, a version it does not read, a step
//                of one, a safe rectangle outside the picture, a twin without its digest (the table's other numbers are untouched, and `out` is left as it was)
//   erase        row_lerp, separable and twin each restore a plate whose word was painted on it, to a stated tolerance, and change nothing outside the rectangle; the guards
//                refuse a picture whose reference columns carry ink, whose twin differs outside its window, or whose word reaches out of the rectangle
//   drawing      the phrase is drawn inside the rectangle and the box of the recipe, the picture outside the rectangle is byte for byte the source, the fit is monotone (a
//                longer phrase never gets a larger scale), lands on the recipe's grid of scales, and stops at the floor (a phrase that needs less is refused, with its
//                scale in the reason); the bytes are deterministic and the PNG is the recipe's size
//   languages    Arabic is drawn in the shaped, visual order (and a stack breaks it in LOGICAL order, so the first word is on the first line); a space in Japanese leaves no
//                gap and U+200B is never drawn; a no-break space is a gap that does not break
//   fail closed  a wrong size, a wrong digest, a code point no face holds, an empty phrase, a phrase that is not UTF-8, a face the slot does not draw with: each keeps the
//                English picture (ok is false, the reason names what, there is no picture), and Ensure leaves no file behind when the folder cannot be made or the target
//                cannot be put in place, finds a whole file without rendering, and renders again over a cut-short one
//   the words    every shipped phrase of every language is drawn from glyphs its faces hold (the fit finds none missing)
//   where named  the seven sprites are named in the tables only where a language switch remakes the layer's cached path of them (the cache audit as a test)
//
// MUTATION TESTS were run on the code (planning step 247), 27 mutants one at a time, each rebuilt and run: the erase zeroed, the collar guard, the floor of a word and of a
// stack, the picture's digest, its size, a missing glyph drawn as the face's notdef, the twin's guards (outside the window, inside it, its digest), Arabic unshaped and
// reversed twice, the Japanese space drawn, U+200B drawn, a temporary file left behind, the PNG check of a cached file cut to its signature, ink allowed past the rectangle,
// and the layer's hook, its refresh loops (the menus', the pause's and end screens', the popups', English coming back, the lazy re-insert), the missing-key rule and the
// remembered failure. Each is failed by this suite or by test_mp_langlayer / test_mp_pickerlayer (the record is the step's, in the planning document).
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

void TheShippedTableLoadsAsTheSevenPictures() {
    const World& world = TheWorld();
    CHECK(world.ok);
    if (!world.ok) return;
    const PB::Recipes& recipes = world.recipes;
    CHECK(recipes.loaded && !recipes.fileHash.empty());
    CHECK_EQ(static_cast<int>(recipes.items.size()), 7);
    std::set<std::string> keys;
    std::set<std::string> sprites;
    for (const PB::Recipe& recipe : recipes.items) {
        keys.insert(recipe.key);
        sprites.insert(recipe.sprite);
        CHECK_MSG(recipes.Find(recipe.sprite) == &recipe, recipe.id + " is found by its sprite");
        CHECK_MSG(recipe.width > 0 && recipe.height > 0 && recipe.digest.size() == 32, recipe.id + " has its size and its digest");
        CHECK_MSG(recipe.erase.rect.Valid() && recipe.erase.rect.x1 <= recipe.width && recipe.erase.rect.y1 <= recipe.height, recipe.id + "'s rectangle is inside the picture");
        CHECK_MSG(recipe.text.minScale > 0.0 && recipe.text.minScale <= 1.0, recipe.id + " has a floor");
        CHECK_MSG(world.languages.Fonts().Slot(recipe.text.slot) != nullptr, recipe.id + " names a slot of the plan");
    }
    CHECK_MSG(recipes.Find("level_button.png") == nullptr && recipes.Find("") == nullptr, "a sprite no recipe names is not found (the twin is a source, not a recipe)");
    // The seven keys are exactly the picture keys of the string table.
    std::set<std::string> want;
    for (const std::string_view key : Strings::kPictureKeys) want.insert(std::string(key));
    CHECK_MSG(keys == want, "the recipes' keys are the string table's picture keys");
    const std::set<std::string> expected = {"main_play_game_button.png", "scores.png", "select_chapter.png", "achievement_points.png", "level_finished.png",
                                            "portals_created_plaque.png", "game_over.png"};
    CHECK_MSG(sprites == expected, "and the sprites are the seven the original draws words on");
    // Four plates (one of them a twin) and three generated pictures, as measured.
    int plates = 0;
    int twins = 0;
    int generated = 0;
    for (const PB::Recipe& recipe : recipes.items) {
        plates += recipe.erase.rule == PB::Rule::RowLerp || recipe.erase.rule == PB::Rule::Separable ? 1 : 0;
        twins += recipe.erase.rule == PB::Rule::Twin ? 1 : 0;
        generated += recipe.erase.rule == PB::Rule::Generated ? 1 : 0;
    }
    CHECK_MSG(plates == 3 && twins == 1 && generated == 3, "three modelled plates, one twin and three generated pictures");
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
        {"unknown layout", "\"layout\": \"word\"", "\"layout\": \"paragraph\"", "layout"},
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
    PB::Recipe recipe = WordRecipe("syn_stack", canvas, PB::Rect{0, 0, 128, 128}, 12.0, 0.6);
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
    // A file cut short is found NOT whole and made again; so is an empty one and one with the wrong signature.
    for (const std::size_t keep : {static_cast<std::size_t>(0), static_cast<std::size_t>(7), static_cast<std::size_t>(40), baked.rgba.size() / 100}) {
        std::string whole;
        CHECK(PB::EncodePng(baked, whole));
        std::ifstream in(first.path, std::ios::binary);
        const std::string held((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        WriteText(first.path, held.substr(0, std::min(keep, held.size() - 1)));
        const PB::Written healed = PB::Ensure(wanted("TAP START", out));
        CHECK_MSG(healed.ok && !healed.cached, "a file cut to " + std::to_string(keep) + " bytes is rendered again: " + healed.reason);
        PB::Image check;
        CHECK_MSG(PB::ReadImage(first.path, check, why) && check.rgba == baked.rgba, "and is whole again");
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
            CHECK_MSG(std::find(table.FellBack().begin(), table.FellBack().end(), recipe.key) == table.FellBack().end(), Code(lang) + " has " + recipe.key);
            const PB::Outcome outcome = PB::FitPhrase(RequestFor(recipe, lang, table.Get(recipe.key)));
            CHECK_MSG(!Contains(outcome.reason, "no glyph") && !Contains(outcome.reason, "no face"), Code(lang) + " " + recipe.key + ": every code point is in its face: " + outcome.reason);
            CHECK_MSG(outcome.ok, Code(lang) + " " + recipe.key + ": the phrase is drawn: " + outcome.reason);
        }
    }
    CHECK_EQ(phrases, 70);
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
void TheWordedSpritesAreNamedOnlyWhereALanguageSwitchRemakesThem() {
    const World& world = TheWorld();
    if (!world.ok) return;
    std::set<std::string> names;
    for (const PB::Recipe& recipe : world.recipes.items) names.insert(recipe.sprite);
    const std::vector<std::string> remade = {"/main_menu/", "/selector/", "/dashboard/", "/pause/", "/level_end/", "/popups/"};
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
    CHECK_MSG(named == 8, "the seven pictures are named in eight places of the screens' rules (the Achievements picture twice: the main menu's button and the pause's): " + std::to_string(named));
}

void runFreeCases() {
    TheShippedTableLoadsAsTheSevenPictures();
    TheWordedSpritesAreNamedOnlyWhereALanguageSwitchRemakesThem();
    TheLoaderRefusesWhatItShouldNot();
    ErasingRestoresAKnownPlate();
    TheGuardsRefuseAWrongPicture();
    ThePhraseStaysInItsBoxAndTheRestIsTheSource();
    TheFitIsMonotoneAndStopsAtTheFloor();
    AnArabicWordIsDrawnInTheShapedOrder();
    ASpaceInJapaneseLeavesNoGapAndAZeroWidthSpaceIsNeverDrawn();
    AStackIsCentredOnItsAnchorAndTheMostLinesThatFitAreUsed();
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
        double tightest = 9.0;
        std::string tightestLanguage;
        for (std::size_t i = 1; i < Strings::kLangCount; ++i) {
            const Lang lang = static_cast<Lang>(i);
            Strings::Table table;
            std::string error;
            CHECK_MSG(table.Load(kData, lang, error), error);
            const std::string phrase = table.Get(recipe.key);
            const PB::Outcome a = PB::Render(RequestFor(recipe, lang, phrase), source, twinPtr);
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
                // Every pixel outside the rectangle is the source's.
                CHECK_MSG(PixelsChangedOutside(a.image, source, recipe.erase.rect) == 0, name + ": nothing outside the rectangle changed");
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
            // Twice is the same bytes.
            const PB::Outcome b = PB::Render(RequestFor(recipe, lang, phrase), source, twinPtr);
            std::string again;
            CHECK_MSG(b.ok && PB::EncodePng(b.image, again) && again == png, name + ": two bakes are the same bytes");
            if (!dump.empty()) {
                const fs::path dir = fs::path(dump) / Code(lang);
                std::error_code ec;
                fs::create_directories(dir, ec);
                WriteText(dir / (recipe.id + ".png"), png);
            }
        }
        std::printf("  %-16s tightest scale %.3f (%s), floor %.2f\n", recipe.id.c_str(), tightest, tightestLanguage.c_str(), recipe.text.minScale);
        CHECK_MSG(tightest >= recipe.text.minScale, recipe.id + ": every language is at or above the floor");
    }
    if (!dump.empty()) std::printf("  the baked pictures were written under %s (MP_PICTUREBAKE_DUMP): delete them when done, they are made from the original's\n", dump.c_str());
}

void TheBakeIsCheap() {
    const World& world = TheWorld();
    if (!world.ok) return;
    // Milliseconds per picture, the least of three runs (other processes share the machine), printed and bounded generously: this is a pin against a hundredfold, not a benchmark.
    Strings::Table german;
    std::string error;
    CHECK(german.Load(kData, Lang::De, error));
    Strings::Table russian;
    CHECK(russian.Load(kData, Lang::Ru, error));
    for (const PB::Recipe& recipe : world.recipes.items) {
        const PB::Image source = ReadOriginal(recipe.sprite);
        PB::Image twin;
        const PB::Image* twinPtr = nullptr;
        if (recipe.erase.rule == PB::Rule::Twin) {
            twin = ReadOriginal(recipe.erase.twinSprite);
            twinPtr = &twin;
        }
        double best = 1e9;
        for (int run = 0; run < 3; ++run) {
            const auto began = std::chrono::steady_clock::now();
            const PB::Outcome outcome = PB::Render(RequestFor(recipe, Lang::Ru, russian.Get(recipe.key)), source, twinPtr);
            std::string png;
            const bool encoded = outcome.ok && PB::EncodePng(outcome.image, png);
            const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
            CHECK_MSG(encoded, recipe.id + " bakes in Russian");
            best = std::min(best, ms);
        }
        std::printf("  %-16s bake and encode: %.1f ms (least of three, Russian)\n", recipe.id.c_str(), best);
        CHECK_MSG(best < 2000.0, recipe.id + " bakes in well under two seconds: " + std::to_string(best));
    }
}

void runOriginalCases() {
    EveryRecipesGuardsHoldOnTheRealSprite();
    TheErasedAchievementsPlateAgreesWithItsSisterPicture();
    AllTenLanguagesBakeEveryPictureAboveItsFloor();
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
