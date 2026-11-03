// The sides of a menu picture on a wide window: side_fill.json and sim/SideFill's arithmetic (planning step 212).
//
// What is pinned, and from where:
//
//   the rules    side_fill.json's keys, strictly: the switch, the feather's slices, the overlap, the least gap, and each picture's mode, measured content
//                and darkness. A missing key, an unknown mode, a negative number and an empty list are refused by name.
//   no gap       at 16:9 and at 4:3 (and every window the picture covers) the fill is EMPTY, so a menu frame there is the frame it was.
//   a gap        at 20:9 and 21:9 each side gets one textured quad and `steps` flat black ones; the textured quad runs from the window's edge to the
//                picture's content (plus the overlap under it), carries ONE column of the picture's edge (edge mode) or its reflection (mirror mode), and the
//                feather is disjoint slices whose darkness is 0 at the seam and rises to max_alpha at the window's edge.
//   the sides    the left and right are mirror images; a picture panned off to one side (the chapter select) fills only the other.
//   the switch   `enabled: false` draws nothing at any width.
//
// Pure arithmetic and files the suite writes: it runs anywhere and never skips.

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "sim/SideFill.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace MagicPortals;

namespace {

const std::string kPortData = MAGICPORTALS_PORT_DATA_DIR;
const std::string kMain = "entities/main_menu_bg.png";
const std::string kPaper = "entities/world_select_bg.png";

constexpr double kBand = 256.0; // the menu box is 256 units tall

bool Near(double a, double b, double eps = 1e-9) {
    return std::fabs(a - b) <= eps;
}

SideFill::Rules TheRules() {
    SideFill::Rules rules;
    std::string error;
    CHECK_MSG(SideFill::LoadRules(kPortData + "/side_fill.json", rules, error), error);
    return rules;
}

glm::dvec2 View(double aspect) {
    return glm::dvec2(kBand * aspect, kBand);
}

// A 512 x 256 picture centred in the view, as the main menu draws its own.
Hud::Rect Centred(const glm::dvec2& view) {
    return Hud::Rect{glm::dvec2((view.x - 512.0) * 0.5, 0.0), glm::dvec2(512.0, 256.0)};
}

// A temporary file the suite writes, removed at the end of the scope.
struct TempJson {
    std::string path;
    explicit TempJson(const std::string& text) {
        path = (std::filesystem::temp_directory_path() / ("test_mp_sidefill_" + std::to_string(counter()++) + ".json")).string();
        std::ofstream(path, std::ios::binary) << text;
    }
    ~TempJson() {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
    }
    static int& counter() {
        static int n = 0;
        return n;
    }
};

std::string Doc(const std::string& enabled = "true", const std::string& steps = "8", const std::string& overlap = "1.0",
                const std::string& minGap = "0.25", const std::string& pictures = "") {
    const std::string list = pictures.empty()
                                 ? R"([{"sprite": "a.png", "mode": "edge", "content_units": [28, 484], "sample_inset_units": 0.5, "max_alpha": 0.8, "power": 1.0}])"
                                 : pictures;
    return R"({"side_fill": {"enabled": )" + enabled + R"(, "steps": )" + steps + R"(, "overlap_units": )" + overlap +
           R"(, "min_gap_units": )" + minGap + R"(, "pictures": )" + list + "}}";
}

bool Loads(const std::string& text, std::string& error) {
    const TempJson file(text);
    SideFill::Rules rules;
    return SideFill::LoadRules(file.path, rules, error);
}

void TheRulesAreTheFilesAndAreStrict() {
    const SideFill::Rules rules = TheRules();
    CHECK(rules.enabled);
    CHECK_EQ(rules.steps, 64);
    CHECK_EQ(rules.overlapUnits, 1.0);
    CHECK_EQ(rules.minGapUnits, 0.25);
    CHECK_EQ(rules.pictures.size(), static_cast<std::size_t>(2));
    if (rules.pictures.size() == 2) {
        // MEASURED: the main menu's file has 28 black units at each side; the paper picture has none.
        CHECK(rules.pictures[0].sprite == kMain && rules.pictures[0].mode == SideFill::Mode::Edge);
        CHECK(rules.pictures[0].contentFrom == 28.0 && rules.pictures[0].contentTo == 484.0);
        CHECK(rules.pictures[1].sprite == kPaper && rules.pictures[1].mode == SideFill::Mode::Mirror);
        CHECK(rules.pictures[1].contentFrom == 0.0 && rules.pictures[1].contentTo == 512.0);
    }

    std::string error;
    CHECK_MSG(Loads(Doc(), error), error);
    CHECK_MSG(!Loads("not json", error), "a file that is not JSON is refused");
    CHECK_MSG(!Loads(R"({"other": {}})", error) && error.find("side_fill") != std::string::npos, "no side_fill block: " + error);
    CHECK_MSG(!Loads(R"({"side_fill": {"steps": 8}})", error) && error.find("enabled") != std::string::npos, "no switch: " + error);
    CHECK_MSG(!Loads(Doc("true", "0"), error) && error.find("steps") != std::string::npos, "steps of 0: " + error);
    CHECK_MSG(!Loads(Doc("true", "2.5"), error), "steps that is not whole");
    CHECK_MSG(!Loads(Doc("true", "300"), error), "too many steps");
    CHECK_MSG(!Loads(Doc("true", "8", "-1"), error), "a negative overlap");
    CHECK_MSG(!Loads(Doc("true", "8", "1.0", "-0.1"), error), "a negative least gap");
    CHECK_MSG(!Loads(Doc("true", "8", "1.0", "0.25", "[]"), error), "no pictures");
    const std::string badMode = R"([{"sprite": "a.png", "mode": "stretch", "content_units": [0, 512], "sample_inset_units": 0.5, "max_alpha": 0.5, "power": 1}])";
    CHECK_MSG(!Loads(Doc("true", "8", "1.0", "0.25", badMode), error) && error.find("stretch") != std::string::npos, "an unknown mode: " + error);
    const std::string backwards = R"([{"sprite": "a.png", "mode": "edge", "content_units": [484, 28], "sample_inset_units": 0.5, "max_alpha": 0.5, "power": 1}])";
    CHECK_MSG(!Loads(Doc("true", "8", "1.0", "0.25", backwards), error), "content that ends before it starts");
    const std::string tooDark = R"([{"sprite": "a.png", "mode": "edge", "content_units": [0, 512], "sample_inset_units": 0.5, "max_alpha": 1.5, "power": 1}])";
    CHECK_MSG(!Loads(Doc("true", "8", "1.0", "0.25", tooDark), error), "a darkness above 1");
    const std::string noPower = R"([{"sprite": "a.png", "mode": "edge", "content_units": [0, 512], "sample_inset_units": 0.5, "max_alpha": 0.5, "power": 0}])";
    CHECK_MSG(!Loads(Doc("true", "8", "1.0", "0.25", noPower), error), "a power of 0");
}

void NothingIsAddedWhereThePictureCoversTheWindow() {
    const SideFill::Rules rules = TheRules();
    // 16:9 (455.11 units): the 456-unit content of the main menu's picture overhangs the window by 0.44 units. 4:3: the window is narrower than the picture.
    for (const double aspect : {16.0 / 9.0, 4.0 / 3.0, 1.0, 5.0 / 4.0}) {
        const glm::dvec2 view = View(aspect);
        CHECK_MSG(SideFill::Quads(rules, kMain, Centred(view), view).empty(),
                  "the main menu's picture at aspect " + std::to_string(aspect) + " adds no quad");
    }
    // The paper picture is 512 wide with no padding: it covers every window up to 2:1.
    for (const double aspect : {16.0 / 9.0, 4.0 / 3.0, 2.0}) {
        const glm::dvec2 view = View(aspect);
        CHECK_MSG(SideFill::Quads(rules, kPaper, Centred(view), view).empty(), "the paper picture at aspect " + std::to_string(aspect) + " adds no quad");
    }
    const glm::dvec2 wide = View(20.0 / 9.0);
    CHECK_MSG(SideFill::Quads(rules, "entities/some_other.png", Centred(wide), wide).empty(), "a picture it does not know adds none");
    SideFill::Rules off = rules;
    off.enabled = false;
    CHECK_MSG(SideFill::Quads(off, kMain, Centred(wide), wide).empty(), "enabled: false draws the original's frame");
}

void ADeepGapIsFilledOnBothSides() {
    const SideFill::Rules rules = TheRules();
    const glm::dvec2 view = View(2400.0 / 1080.0); // a 20:9 phone: 568.89 x 256
    const Hud::Rect picture = Centred(view);
    const std::vector<SideFill::Quad> quads = SideFill::Quads(rules, kMain, picture, view);
    // One textured quad and `steps` flat ones at each side.
    CHECK_EQ(quads.size(), static_cast<std::size_t>(2 * (1 + rules.steps)));
    if (quads.size() != static_cast<std::size_t>(2 * (1 + rules.steps))) return;

    const double seamLeft = picture.min.x + 28.0;
    const double seamRight = picture.min.x + 484.0;
    const double gap = seamLeft; // from x = 0
    CHECK_MSG(Near(gap, (568.8889 - 456.0) * 0.5, 1e-3), "the gap at 20:9 is 56.44 units a side: " + std::to_string(gap));

    // The left textured quad: from the window's edge to the seam plus the overlap, over the picture's rows, one column of u.
    const SideFill::Quad& left = quads[0];
    CHECK(left.textured);
    CHECK_MSG(Near(left.rect.min.x, 0.0) && Near(left.rect.Max().x, seamLeft + rules.overlapUnits), "the left quad runs from the window's edge under the picture");
    CHECK_MSG(Near(left.rect.min.y, 0.0) && Near(left.rect.Max().y, 256.0), "and over the picture's rows");
    const double uLeft = (28.0 + 0.5) / 512.0;
    CHECK_MSG(Near(left.uvMin.x, uLeft) && Near(left.uvMax.x, uLeft), "ONE column of the picture's edge, inside the content and never in the padding");
    CHECK_MSG(Near(left.uvMin.y, 0.0) && Near(left.uvMax.y, 1.0), "all its rows");

    // The feather: slices that touch and do not overlap, darkness 0 at the seam rising to max_alpha at the window's edge.
    std::vector<SideFill::Quad> feather(quads.begin() + 1, quads.begin() + 1 + rules.steps);
    bool apart = true;
    bool rising = true;
    for (std::size_t i = 0; i < feather.size(); ++i) {
        CHECK_MSG(!feather[i].textured && feather[i].colour.r == 0.0f && feather[i].colour.g == 0.0f && feather[i].colour.b == 0.0f, "flat black");
        // Slice i lies i slices from the seam, outward = leftward.
        const double near = seamLeft - gap * static_cast<double>(i) / rules.steps;
        const double far = seamLeft - gap * static_cast<double>(i + 1) / rules.steps;
        apart = apart && Near(feather[i].rect.Max().x, near, 1e-9) && Near(feather[i].rect.min.x, far, 1e-9);
        if (i > 0) rising = rising && feather[i].colour.a > feather[i - 1].colour.a;
    }
    CHECK_MSG(apart, "the slices tile the gap exactly: no overlap, no hole");
    CHECK_MSG(rising, "the darkness rises away from the seam");
    CHECK_MSG(Near(feather.front().colour.a, 0.8 * 0.5 / rules.steps, 1e-6) || feather.front().colour.a < 0.02f, "almost clear at the seam");
    const double maxAlpha = rules.pictures[0].maxAlpha;
    CHECK_MSG(Near(feather.back().colour.a, maxAlpha * (rules.steps - 0.5) / rules.steps, 1e-6), "and almost max_alpha at the window's edge");

    // The right side is the left's mirror image about the view's middle.
    const SideFill::Quad& right = quads[1 + rules.steps];
    CHECK(right.textured);
    CHECK_MSG(Near(right.rect.min.x, seamRight - rules.overlapUnits) && Near(right.rect.Max().x, view.x), "the right quad is the left's mirror image");
    const double uRight = (484.0 - 0.5) / 512.0;
    CHECK_MSG(Near(right.uvMin.x, uRight) && Near(right.uvMax.x, uRight), "the right edge column");
    for (std::size_t i = 0; i < feather.size(); ++i) {
        const SideFill::Quad& mirrored = quads[2 + rules.steps + i];
        CHECK_MSG(Near(mirrored.colour.a, feather[i].colour.a, 1e-9) && Near(mirrored.rect.size.x, feather[i].rect.size.x, 1e-9),
                  "each right slice is as dark and as wide as its left twin");
    }
    // The gap beside it never reaches under the picture's content: the fill meets the picture at its seam.
    CHECK_MSG(left.rect.Max().x <= seamLeft + rules.overlapUnits + 1e-9, "no further under the picture than the overlap");
}

void TheMirrorModeReflectsThePaperAboutTheSeam() {
    const SideFill::Rules rules = TheRules();
    const glm::dvec2 view = View(2400.0 / 1080.0);
    const Hud::Rect picture = Centred(view); // 512 wide: 28.44 units of window at each side
    const std::vector<SideFill::Quad> quads = SideFill::Quads(rules, kPaper, picture, view);
    CHECK_EQ(quads.size(), static_cast<std::size_t>(2 * (1 + rules.steps)));
    if (quads.size() != static_cast<std::size_t>(2 * (1 + rules.steps))) return;
    const double seamLeft = picture.min.x;
    const SideFill::Quad& left = quads[0];
    CHECK(left.textured);
    // Left: u = distance from the seam / 512, so it falls to 0 at the seam and is the reflection: a reversed uv pair where the quad's far end is its left.
    const double expectFar = (0.0 + (seamLeft - 0.0)) / 512.0;
    const double expectNear = (0.0 + (seamLeft - left.rect.Max().x)) / 512.0; // = -overlap / 512: under the picture
    CHECK_MSG(Near(left.uvMin.x, expectFar, 1e-9) && Near(left.uvMax.x, expectNear, 1e-9),
              "the left quad's u runs from the reflected far end down to the seam: " + std::to_string(left.uvMin.x) + ", " + std::to_string(left.uvMax.x));
    CHECK_MSG(left.uvMin.x > left.uvMax.x, "a reversed pair is a mirror");
    const SideFill::Quad& right = quads[1 + rules.steps];
    CHECK_MSG(right.uvMin.x > right.uvMax.x, "and at the right too");
    CHECK_MSG(Near(right.uvMin.x, (512.0 + rules.overlapUnits) / 512.0, 1e-9), "its seam end is the content's end plus the overlap");
    // The paper's feather is lighter than the dark picture's.
    CHECK_MSG(quads[rules.steps].colour.a <= static_cast<float>(rules.pictures[1].maxAlpha) + 1e-6f, "never darker than max_alpha");
}

void APicturePannedToOneSideIsFilledOnTheOtherOnly() {
    const SideFill::Rules rules = TheRules();
    const glm::dvec2 view = View(2400.0 / 1080.0);
    // The chapter select's paper starts at the window's left edge (its pan is 0 when the picture is narrower than the view + 512): the gap is at the right.
    const Hud::Rect picture{glm::dvec2(0.0, 0.0), glm::dvec2(512.0, 256.0)};
    const std::vector<SideFill::Quad> quads = SideFill::Quads(rules, kPaper, picture, view);
    CHECK_EQ(quads.size(), static_cast<std::size_t>(1 + rules.steps));
    if (quads.empty()) return;
    CHECK_MSG(quads[0].rect.Max().x >= view.x - 1e-9 && quads[0].rect.min.x >= 512.0 - rules.overlapUnits - 1e-9, "the fill is at the right, from the picture's edge to the window's");
    // A picture panned far enough to cover the window has no fill.
    const Hud::Rect panned{glm::dvec2(-60.0, 0.0), glm::dvec2(512.0, 256.0)};
    CHECK_MSG(SideFill::Quads(rules, kPaper, panned, view).size() == static_cast<std::size_t>(1 + rules.steps), "pan 60: still 0..452 against 568.9: a gap");
}

void APictureTallerOrShorterThanTheViewKeepsItsRows() {
    const SideFill::Rules rules = TheRules();
    const glm::dvec2 view = View(2400.0 / 1080.0);
    // The credits and the selector can draw a picture that is not exactly the view's height: the fill takes the picture's rows, clipped to the view.
    const Hud::Rect shifted{glm::dvec2((view.x - 512.0) * 0.5, -16.0), glm::dvec2(512.0, 256.0)};
    const std::vector<SideFill::Quad> quads = SideFill::Quads(rules, kMain, shifted, view);
    CHECK(!quads.empty());
    if (quads.empty()) return;
    CHECK_MSG(Near(quads[0].rect.min.y, 0.0) && Near(quads[0].rect.Max().y, 240.0), "the rows are the picture's, clipped to the view");
    CHECK_MSG(Near(quads[0].uvMin.y, 16.0 / 256.0) && Near(quads[0].uvMax.y, 1.0), "and its v follows: the first 16 units of the picture are off the top");
}

} // namespace

MP_SUITE_MAIN("test_mp_sidefill") {
    TheRulesAreTheFilesAndAreStrict();
    NothingIsAddedWhereThePictureCoversTheWindow();
    ADeepGapIsFilledOnBothSides();
    TheMirrorModeReflectsThePaperAboutTheSeam();
    APicturePannedToOneSideIsFilledOnTheOtherOnly();
    APictureTallerOrShorterThanTheViewKeepsItsRows();
    return ::test::summary("test_mp_sidefill", 190);
}
