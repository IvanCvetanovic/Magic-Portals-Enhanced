// The five on-screen controls' geometry: sim/Controls, the one place that says where each is
// drawn and where a touch counts as on it (planning steps 179 and 180).
//
// The classic functions there FORWARD to sim/Hud's pad arithmetic, so a suite that only compared
// them with sim/Hud would pass whatever Hud said. Two kinds of check keep it honest. The
// equivalences walk a grid of points and ask the module and Hud the same question, because the
// layer now asks the module and the 128 sweeps, the claim map and the HUD digest were taken of
// Hud's answers; and the literals are written out in the units ui.json states them in (a hit
// circle of 64 units, a slide of 128 over 700 ms, the pulse's 163 at 1-01's door), so a change to
// either side, or to the data under both, fails here and not only in a level.
//
// From the second half of the file (planning step 180) the cases are of the MODERN controls: game/data/controls.json, the
// layout, the hit rule, the tuning and the policy of which style a run plays. They are the numbers of
// docs/planning/2026-10-15-modern-controls.md as literals, so a number of the data that moves, in the file or in a copy of it
// read through MP_TEST_PORTDATA, fails here; none is read back from the Rules it checks.
//
// Pure: no window, no registry, no level, no original art. ui.json, camera.json and controls.json are the port's own and
// committed, so this runs anywhere and never skips.

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "sim/Controls.hpp"
#include "sim/Hint.hpp"
#include "sim/Hud.hpp"
#include "sim/LevelEnd.hpp"
#include "sim/Switches.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <system_error>
#include <tuple>
#include <utility>
#include <vector>

using namespace MagicPortals;

namespace {

// The port's own data: MP_TEST_PORTDATA when it is set (TEST ONLY: a mutation test runs this same
// binary on a copy of game/data with one number changed, as test_mp_hud and test_mp_layer do), else the
// build's own. _dupenv_s: std::getenv is C4996 on Windows.
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
    std::printf("test_mp_controls: MUTATION - MP_TEST_PORTDATA is set, the port's data is read from %s, not from game/data\n",
                text.c_str());
    return text;
}

const std::string kData = PortDataDir();
const std::string kUi = kData + "/ui.json";

// The original's captures: 1280x720, and 2.8125 screen px to a design unit. The view is 256 units tall.
constexpr double kHeight = 256.0;
const glm::dvec2 kView169(1280.0 / 2.8125, kHeight);
const glm::dvec2 kView43(kHeight * 4.0 / 3.0, kHeight);
const glm::dvec2 kView21(kHeight * 2.0, kHeight);
const std::array<glm::dvec2, 3> kViews = {kView169, kView43, kView21};

// The level ages the pads' slide is judged at: before it starts, through it, its end and long after.
const std::array<double, 8> kAges = {0.0, 100.0, 240.0, 350.0, 480.0, 640.0, 700.0, 5000.0};

const std::array<Controls::Id, 2> kWalks = {Controls::Id::Left, Controls::Id::Right};
const std::array<Controls::Id, 3> kCorners = {Controls::Id::Reset, Controls::Id::Menu, Controls::Id::Clear};

bool Near(double a, double b, double eps) {
    return std::fabs(a - b) <= eps;
}

std::string Num(double v) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.6f", v);
    return buffer;
}

std::string At(const glm::dvec2& p) {
    return "(" + Num(p.x) + ", " + Num(p.y) + ")";
}

Hud::Rules Load() {
    Hud::Rules rules;
    std::string error;
    CHECK_MSG(Hud::LoadRules(kUi, rules, error), "ui.json reads: " + error);
    return rules;
}

Hud::Side SideOf(Controls::Id walk) {
    return walk == Controls::Id::Left ? Hud::Side::Left : Hud::Side::Right;
}

bool SameRect(const Hud::Rect& a, const Hud::Rect& b) {
    return a.min.x == b.min.x && a.min.y == b.min.y && a.size.x == b.size.x && a.size.y == b.size.y;
}

// Every point of the view and a margin round it on a quarter-unit lattice, which holds the pads' circles' edges
// and every rectangle's edges (all of them multiples of a quarter) and runs a little past the view on each side.
template <typename F>
void Grid(const glm::dvec2& view, F&& visit) {
    constexpr double kStep = 0.25;
    constexpr double kMargin = 8.0;
    for (double y = -kMargin; y <= view.y + kMargin; y += kStep) {
        for (double x = -kMargin; x <= view.x + kMargin; x += kStep) visit(glm::dvec2(x, y));
    }
}

const char* NameOf(Controls::Id id) {
    switch (id) {
    case Controls::Id::Left:
        return "Left";
    case Controls::Id::Right:
        return "Right";
    case Controls::Id::Reset:
        return "Reset";
    case Controls::Id::Menu:
        return "Menu";
    case Controls::Id::Clear:
        return "Clear";
    }
    return "?";
}

const char* ShapeOf(const glm::dvec2& view) {
    if (view == kView169) return "16:9";
    if (view == kView43) return "4:3";
    return "2:1";
}

// ---- the equivalences: the module and Hud answer alike -------------------------------------------------

void ClassicHitEqualsTheHudFunctionsEverywhere() {
    const Hud::Rules rules = Load();
    long pointsInside = 0;
    for (const glm::dvec2& view : kViews) {
        for (const double age : kAges) {
            for (const Controls::Id walk : kWalks) {
                const Controls::Frame frame = Controls::ClassicWalkFrame(rules, walk, view, age);
                long differ = 0;
                Grid(view, [&](const glm::dvec2& p) {
                    const bool theirs = Hud::OnPad(rules, SideOf(walk), view, age, p);
                    if (frame.hit.Contains(p) != theirs) ++differ;
                    if (theirs) ++pointsInside;
                });
                CHECK_MSG(differ == 0, std::string(NameOf(walk)) + " pad at " + ShapeOf(view) + ", age " + Num(age) +
                                           " ms: the shape and Hud::OnPad differ at " + std::to_string(differ) + " points");
            }
        }
        // A corner button's claim is its drawn rectangle, closed: Hud::Rect::Contains, in its place and where a
        // button coming in or going out has moved it.
        for (const Controls::Id corner : kCorners) {
            for (const glm::dvec2 shift : {glm::dvec2(0.0), glm::dvec2(7.5, -3.25)}) {
                Hud::Rect drawn = Controls::ClassicPlace(rules, corner, view);
                drawn.min += shift;
                const Controls::Frame frame = Controls::ClassicCornerFrame(drawn);
                long differ = 0;
                Grid(view, [&](const glm::dvec2& p) {
                    if (frame.hit.Contains(p) != drawn.Contains(p)) ++differ;
                });
                CHECK_MSG(differ == 0, std::string(NameOf(corner)) + " at " + ShapeOf(view) + " shifted " + At(shift) +
                                           ": the shape and Rect::Contains differ at " + std::to_string(differ) + " points");
            }
        }
    }
    // Both sides saying "nobody is on a pad" everywhere would agree too: the grid must reach points that are on one.
    CHECK_MSG(pointsInside > 100000, "the grid met " + std::to_string(pointsInside) + " points on a pad");
}

void ClassicFramesAreTheHudsRectsExactly() {
    const Hud::Rules rules = Load();
    for (const glm::dvec2& view : kViews) {
        for (const double age : kAges) {
            for (const Controls::Id walk : kWalks) {
                const Controls::Frame frame = Controls::ClassicWalkFrame(rules, walk, view, age);
                const Hud::Rect rect = Hud::PadRect(rules, SideOf(walk), view, age);
                CHECK_MSG(SameRect(frame.rect, rect), std::string(NameOf(walk)) + " at " + ShapeOf(view) + ", age " + Num(age) +
                                                          ": the drawn rectangle is " + At(frame.rect.min) + ", Hud's is " +
                                                          At(rect.min));
                // The disc is about the pad's screen corner, as it slides, of the rules' hit radius: exactly.
                const glm::dvec2 corner = Hud::PadCorner(rules, SideOf(walk), view, age);
                CHECK_MSG(frame.hit.kind == Controls::HitShape::Kind::Disc && frame.hit.centre.x == corner.x &&
                              frame.hit.centre.y == corner.y && frame.hit.radius == rules.pads.hitRadiusUnits,
                          std::string(NameOf(walk)) + " at " + ShapeOf(view) + ", age " + Num(age) + ": the disc is about " +
                              At(frame.hit.centre) + ", the corner is " + At(corner));
            }
        }
        for (const Controls::Id corner : kCorners) {
            const Hud::Placement& placement = Controls::ClassicPlacement(rules, corner);
            const Hud::Rect placed = Controls::ClassicPlace(rules, corner, view);
            CHECK_MSG(SameRect(placed, Hud::Place(placement, view)), std::string(NameOf(corner)) + " at " + ShapeOf(view) +
                                                                         ": placed " + At(placed.min) + ", Hud's " +
                                                                         At(Hud::Place(placement, view).min));
            const Controls::Frame frame = Controls::ClassicCornerFrame(placed);
            CHECK_MSG(SameRect(frame.rect, placed) && SameRect(frame.hit.rect, placed) &&
                          frame.hit.kind == Controls::HitShape::Kind::Rect && !frame.hit.halfOpen,
                      std::string(NameOf(corner)) + ": the frame is the drawn rectangle, closed");
        }
    }
    // The three placements are ui.json's own members, not copies.
    CHECK(&Controls::ClassicPlacement(rules, Controls::Id::Reset) == &rules.restart);
    CHECK(&Controls::ClassicPlacement(rules, Controls::Id::Menu) == &rules.pause);
    CHECK(&Controls::ClassicPlacement(rules, Controls::Id::Clear) == &rules.clearPortals);
    for (const double age : kAges) {
        CHECK_MSG(Controls::ClassicSlideUnits(rules, age) == Hud::PadSlideUnits(rules, age),
                  "the slide at " + Num(age) + " ms is Hud's");
    }
}

void ClassicWalkClaimEqualsTheTwoOnPadCalls() {
    const Hud::Rules rules = Load();
    for (const glm::dvec2& view : kViews) {
        for (const double age : kAges) {
            const Controls::Frame left = Controls::ClassicWalkFrame(rules, Controls::Id::Left, view, age);
            const Controls::Frame right = Controls::ClassicWalkFrame(rules, Controls::Id::Right, view, age);
            long differ = 0;
            long notTheTwoDiscs = 0;
            long onEither = 0;
            Grid(view, [&](const glm::dvec2& p) {
                const bool theirs = Hud::OnPad(rules, Hud::Side::Left, view, age, p) ||
                                    Hud::OnPad(rules, Hud::Side::Right, view, age, p);
                const bool claim = Controls::ClassicOnWalkControl(rules, view, age, p);
                if (claim != theirs) ++differ;
                // The same question asked of the two laid-out frames, which is what a claim reads: the pads together.
                if (claim != (left.hit.Contains(p) || right.hit.Contains(p))) ++notTheTwoDiscs;
                if (theirs) ++onEither;
            });
            CHECK_MSG(differ == 0, std::string(ShapeOf(view)) + ", age " + Num(age) + " ms: the walk claim and the two OnPad calls differ at " +
                                       std::to_string(differ) + " points (" + std::to_string(onEither) + " on a pad)");
            CHECK_MSG(notTheTwoDiscs == 0, std::string(ShapeOf(view)) + ", age " + Num(age) +
                                               " ms: the walk claim is not the two frames' discs together, at " + std::to_string(notTheTwoDiscs) + " points");
        }
    }
    // Asked of the rules and the view and the age alone: nothing about the pads being drawn enters it. A settled pad claims
    // its corner and the strip along the floor; the middle of the view is the world's.
    CHECK(Controls::ClassicOnWalkControl(rules, kView169, 5000.0, glm::dvec2(1.0, kHeight - 1.0)));
    CHECK(Controls::ClassicOnWalkControl(rules, kView169, 5000.0, glm::dvec2(kView169.x - 1.0, kHeight - 1.0)));
    CHECK(!Controls::ClassicOnWalkControl(rules, kView169, 5000.0, kView169 * 0.5));
}

void TheEndByteIsThePulsesByte() {
    const Hud::Rules rules = Load();
    // The pads decay from the byte the pulse left them at, so the byte and the fraction are one number: the opacity is the
    // byte over 255 at every age, in the tutorial and out of it, through the pulse and long after it.
    for (const bool tutorial : {false, true}) {
        long differ = 0;
        long flat = 0;
        int lowest = 255;
        int highest = 0;
        for (double age = -16.0; age <= 20000.0; age += 4.0) {
            const int byte = Controls::ClassicWalkAlphaByte(rules, age, tutorial);
            if (Controls::ClassicWalkOpacity(rules, age, tutorial) != static_cast<double>(byte) / 255.0) ++differ;
            if (byte != Hud::PadAlphaByte(rules, age, tutorial)) ++differ;
            if (Controls::ClassicWalkOpacity(rules, age, tutorial) != Hud::PadOpacity(rules, age, tutorial)) ++differ;
            if (byte == rules.alphaByte) ++flat;
            lowest = std::min(lowest, byte);
            highest = std::max(highest, byte);
        }
        CHECK_MSG(differ == 0, std::string(tutorial ? "tutorial" : "ordinary") + " pads: the opacity is not the byte over 255 at " +
                                   std::to_string(differ) + " ages");
        CHECK_MSG(lowest == 120 && flat > 0, "the pulse never goes under the flat 120 and ends on it: lowest " + std::to_string(lowest));
        // The triangle's top is the base and the whole variation: the bias is 1 on the first tick of every odd stride.
        CHECK_MSG(highest == (tutorial ? 120 + 90 : 120 + 40), "the pulse peaks at the variation: " + std::to_string(highest));
    }
    // And the two ends the step-178 pins worked out by hand from ui.json's literals (a 300 ms stride, a flat 120, the odd
    // strides run down, truncated, a variation of 90 in the tutorial and 40 elsewhere), equal to the game's own log.
    // 1-01's door on tick 217: 3456 ms is 11 strides and 156 ms, run down: bias 1 - 0.52, uint(0.48 x 90) = 43.
    CHECK_EQ(Controls::ClassicWalkAlphaByte(rules, 3456.0, true), 163);
    // 2-10's death on tick 80: 1264 ms is 4 strides and 64 ms, run up: bias 0.2133, uint(0.2133 x 40) = 8.
    CHECK_EQ(Controls::ClassicWalkAlphaByte(rules, 1264.0, false), 128);
    // Past the pulse it is flat, and the fraction is the 120 of 255 the HUD is drawn at.
    CHECK_EQ(Controls::ClassicWalkAlphaByte(rules, 20000.0, true), 120);
    CHECK_MSG(Near(Controls::ClassicWalkOpacity(rules, 20000.0, false), 120.0 / 255.0, 1e-15), "flat at 120 of 255");
}

// ---- the shapes, from literals -------------------------------------------------------------------------

void AWalkPadsClaimIsACircleOfSixtyFourUnitsAboutItsCorner() {
    const Hud::Rules rules = Load();
    const glm::dvec2 view = kView169;
    for (const Controls::Id walk : kWalks) {
        const Controls::Frame frame = Controls::ClassicWalkFrame(rules, walk, view, 5000.0);
        const bool left = walk == Controls::Id::Left;
        const glm::dvec2 corner(left ? 0.0 : view.x, kHeight);
        // The corner is the bottom corner of the view, the pad settled, and the circle is 64 units: literals, not the rules'.
        CHECK_MSG(frame.hit.centre == corner, std::string(NameOf(walk)) + ": the circle is about " + At(frame.hit.centre));
        CHECK_MSG(frame.hit.radius == 64.0, std::string(NameOf(walk)) + ": the radius is " + Num(frame.hit.radius));
        // 63.99 from the corner along the floor, up the wall and on the diagonal is on the pad; 64.01 is not.
        const double along = left ? 1.0 : -1.0; // away from the screen's edge, along the floor
        const double diagonal = 64.0 / std::sqrt(2.0);
        CHECK(frame.hit.Contains(corner + glm::dvec2(along * 63.99, 0.0)));
        CHECK(!frame.hit.Contains(corner + glm::dvec2(along * 64.01, 0.0)));
        CHECK(frame.hit.Contains(corner + glm::dvec2(0.0, -63.99)));
        CHECK(!frame.hit.Contains(corner + glm::dvec2(0.0, -64.01)));
        CHECK(frame.hit.Contains(corner + glm::dvec2(along * (diagonal - 0.01), -(diagonal - 0.01))));
        CHECK(!frame.hit.Contains(corner + glm::dvec2(along * (diagonal + 0.01), -(diagonal + 0.01))));
        // Exactly 64 from the corner is off the pad: isPointInSphere is strict, and the squared distance is exactly 4096.
        CHECK_MSG(!frame.hit.Contains(corner + glm::dvec2(along * 64.0, 0.0)), std::string(NameOf(walk)) + ": exactly 64 along the floor");
        CHECK_MSG(!frame.hit.Contains(corner + glm::dvec2(0.0, -64.0)), std::string(NameOf(walk)) + ": exactly 64 up the wall");
        // The drawn square is 64 units and sits on the floor, and its far corner is outside the circle that claims the pad.
        CHECK_MSG(frame.rect.size == glm::dvec2(64.0) && frame.rect.min.y == kHeight - 64.0,
                  std::string(NameOf(walk)) + ": drawn 64 units square on the floor");
        const glm::dvec2 farCorner = left ? glm::dvec2(frame.rect.Max().x, frame.rect.min.y) : frame.rect.min;
        CHECK_MSG(!frame.hit.Contains(farCorner), std::string(NameOf(walk)) + ": the square's far corner is not on the pad");
    }
    // The left pad is flush in the left corner and the right pad in the right: 0 to 64 and W - 64 to W.
    CHECK(Controls::ClassicWalkFrame(rules, Controls::Id::Left, view, 5000.0).rect.min.x == 0.0);
    CHECK(Near(Controls::ClassicWalkFrame(rules, Controls::Id::Right, view, 5000.0).rect.min.x, view.x - 64.0, 1e-12));
    // And the other pad's corner is neither's.
    CHECK(!Controls::ClassicWalkFrame(rules, Controls::Id::Left, view, 5000.0).hit.Contains(glm::dvec2(view.x - 1.0, kHeight - 1.0)));
    CHECK(!Controls::ClassicWalkFrame(rules, Controls::Id::Right, view, 5000.0).hit.Contains(glm::dvec2(1.0, kHeight - 1.0)));
}

void ThePadsSlideAndTheRingFollowsTheCorner() {
    const Hud::Rules rules = Load();
    // ScreenPad's interpolators: 128 units outside the corner at zero, in by 700 ms, smoothEnd between.
    CHECK_EQ(Controls::ClassicSlideUnits(rules, 0.0), 128.0);
    CHECK_EQ(Controls::ClassicSlideUnits(rules, 700.0), 0.0);
    CHECK_EQ(Controls::ClassicSlideUnits(rules, 9999.0), 0.0);
    // smoothEnd is a half at a third of the way: 700 / 3 ms in, a pad is its own width (64) out.
    CHECK_MSG(Near(Controls::ClassicSlideUnits(rules, 700.0 / 3.0), 64.0, 1e-9), "a third of the way: " + Num(Controls::ClassicSlideUnits(rules, 700.0 / 3.0)));
    // and at half the way, 128 x (1 - sin(pi / 4)).
    CHECK_MSG(Near(Controls::ClassicSlideUnits(rules, 350.0), 128.0 * (1.0 - std::sqrt(0.5)), 1e-9), "half way: " + Num(Controls::ClassicSlideUnits(rules, 350.0)));

    const glm::dvec2 view = kView169;
    // The ring is centred on the pad's screen CORNER, which slides with the pad, not on the middle of the drawn square.
    const glm::dvec2 settledLeft = Controls::ClassicRingCentre(rules, Controls::Id::Left, view, 5000.0);
    const glm::dvec2 settledRight = Controls::ClassicRingCentre(rules, Controls::Id::Right, view, 5000.0);
    CHECK_MSG(settledLeft == glm::dvec2(0.0, kHeight), "settled, the left ring is about " + At(settledLeft));
    CHECK_MSG(settledRight == glm::dvec2(view.x, kHeight), "settled, the right ring is about " + At(settledRight));
    CHECK_MSG(Controls::ClassicRingCentre(rules, Controls::Id::Left, view, 0.0) == glm::dvec2(-128.0, kHeight), "at the first tick the left ring is 128 units out");
    CHECK_MSG(Controls::ClassicRingCentre(rules, Controls::Id::Right, view, 0.0) == glm::dvec2(view.x + 128.0, kHeight), "and the right one");
    for (const double age : kAges) {
        for (const Controls::Id walk : kWalks) {
            const glm::dvec2 centre = Controls::ClassicRingCentre(rules, walk, view, age);
            const glm::dvec2 theirs = Hud::PadCorner(rules, SideOf(walk), view, age);
            CHECK_MSG(centre.x == theirs.x && centre.y == theirs.y, std::string(NameOf(walk)) + " ring at " + Num(age) + " ms: " + At(centre) + " against Hud's " + At(theirs));
            // It is also the centre of the claim's disc: the ring, the circle and the pad's corner are one point.
            const Controls::Frame frame = Controls::ClassicWalkFrame(rules, walk, view, age);
            CHECK_MSG(frame.hit.centre.x == centre.x && frame.hit.centre.y == centre.y, std::string(NameOf(walk)) + " at " + Num(age) + " ms: the disc and the ring are about different points");
        }
    }
}

void ACornerButtonsClaimIsItsDrawnRectangleAndAHalfOpenOneLosesItsFarEdges() {
    // Closed: the edges are in, a thousandth outside is not (Hud::Rect::Contains).
    const Hud::Rect drawn{glm::dvec2(100.0, 50.0), glm::dvec2(32.0, 32.0)};
    const Controls::HitShape closed = Controls::HitShape::Closed(drawn);
    CHECK(closed.Contains(glm::dvec2(100.0, 50.0)));
    CHECK(closed.Contains(glm::dvec2(132.0, 82.0)));
    CHECK(closed.Contains(glm::dvec2(132.0, 50.0)));
    CHECK(closed.Contains(glm::dvec2(100.0, 82.0)));
    CHECK(closed.Contains(glm::dvec2(116.0, 66.0)));
    CHECK(!closed.Contains(glm::dvec2(99.999, 66.0)));
    CHECK(!closed.Contains(glm::dvec2(132.001, 66.0)));
    CHECK(!closed.Contains(glm::dvec2(116.0, 49.999)));
    CHECK(!closed.Contains(glm::dvec2(116.0, 82.001)));
    // Half-open: [min, max). The near edges are in, the far edges are out, so two rectangles that share an edge never both claim it.
    const Controls::HitShape half = Controls::HitShape::HalfOpen(drawn);
    CHECK(half.Contains(glm::dvec2(100.0, 50.0)));
    CHECK(half.Contains(glm::dvec2(131.999, 81.999)));
    CHECK(!half.Contains(glm::dvec2(132.0, 66.0)));
    CHECK(!half.Contains(glm::dvec2(116.0, 82.0)));
    CHECK(!half.Contains(glm::dvec2(132.0, 82.0)));
    CHECK(!half.Contains(glm::dvec2(99.999, 66.0)));
    const Controls::HitShape neighbour = Controls::HitShape::HalfOpen(Hud::Rect{glm::dvec2(132.0, 50.0), glm::dvec2(32.0, 32.0)});
    for (const double y : {50.0, 66.0, 81.0}) {
        CHECK_MSG(half.Contains(glm::dvec2(132.0, y)) != neighbour.Contains(glm::dvec2(132.0, y)), "a point on the shared edge is claimed by exactly one");
    }
    CHECK(neighbour.Contains(glm::dvec2(132.0, 66.0)));
    // Two closed rectangles claim a shared edge twice: what the half-open flag is for, and what the classic corner buttons,
    // which never meet, do not need.
    CHECK(closed.Contains(glm::dvec2(132.0, 66.0)));
    // A disc is strict, whatever its centre: exactly the radius is outside.
    const Controls::HitShape disc = Controls::HitShape::Disc(glm::dvec2(10.0, 20.0), 5.0);
    CHECK(disc.Contains(glm::dvec2(10.0, 20.0)));
    CHECK(disc.Contains(glm::dvec2(14.99, 20.0)));
    CHECK(!disc.Contains(glm::dvec2(15.0, 20.0)));
    CHECK(!disc.Contains(glm::dvec2(10.0, 25.0)));
    CHECK(!disc.Contains(glm::dvec2(15.01, 20.0)));
}

void AControlNotLaidOutYetClaimsNothing() {
    // A default shape is an empty disc, so a button built and not yet laid out claims no point - not the one point (0, 0) that
    // a closed rectangle of no size would.
    const Controls::HitShape none;
    CHECK(none.kind == Controls::HitShape::Kind::Disc && none.radius == 0.0);
    long claimed = 0;
    Grid(kView169, [&](const glm::dvec2& p) {
        if (none.Contains(p)) ++claimed;
    });
    CHECK_MSG(claimed == 0, "a default shape claimed " + std::to_string(claimed) + " points");
    CHECK(!none.Contains(glm::dvec2(0.0)));
    const Controls::Frame frame;
    CHECK(frame.rect.size == glm::dvec2(0.0) && !frame.hit.Contains(glm::dvec2(0.0)));
    // A corner button asked for a pad's frame, or a pad asked for a placement, is empty rather than somebody else's.
    const Hud::Rules rules = Load();
    for (const Controls::Id corner : kCorners) {
        const Controls::Frame empty = Controls::ClassicWalkFrame(rules, corner, kView169, 5000.0);
        CHECK_MSG(empty.rect.size == glm::dvec2(0.0) && !empty.hit.Contains(glm::dvec2(0.0)) && !empty.hit.Contains(kView169 * 0.5),
                  std::string(NameOf(corner)) + " is not a walk pad and has no pad frame");
        CHECK_MSG(Controls::ClassicRingCentre(rules, corner, kView169, 5000.0) == glm::dvec2(0.0), std::string(NameOf(corner)) + " has no ring");
    }
    for (const Controls::Id walk : kWalks) {
        const Hud::Placement& placement = Controls::ClassicPlacement(rules, walk);
        CHECK_MSG(placement.sprite.empty() && placement.sizeUnits == glm::dvec2(0.0), std::string(NameOf(walk)) + " has no placement");
    }
}

void TheCornerButtonsSitWhereUiJsonPutsThem() {
    const Hud::Rules rules = Load();
    const glm::dvec2 view = kView169;
    // The three 32-unit squares of the original's, from its captures: restart (1100, 0) and pause (1190, 0) in the top right,
    // 90 px each at 1280 x 720, and clear-portals in the top left. Literals, in units.
    const Hud::Rect restart = Controls::ClassicPlace(rules, Controls::Id::Reset, view);
    const Hud::Rect pause = Controls::ClassicPlace(rules, Controls::Id::Menu, view);
    const Hud::Rect clear = Controls::ClassicPlace(rules, Controls::Id::Clear, view);
    CHECK_MSG(Near(restart.min.x, view.x - 64.0, 1e-12) && restart.min.y == 0.0 && restart.size == glm::dvec2(32.0), "restart: " + At(restart.min));
    CHECK_MSG(Near(pause.min.x, view.x - 32.0, 1e-12) && pause.min.y == 0.0 && pause.size == glm::dvec2(32.0), "pause: " + At(pause.min));
    CHECK_MSG(clear.min == glm::dvec2(0.0) && clear.size == glm::dvec2(32.0), "clear-portals: " + At(clear.min));
    // They hug their corners whatever the window's shape.
    CHECK(Near(Controls::ClassicPlace(rules, Controls::Id::Menu, kView43).Max().x, kView43.x, 1e-12));
    CHECK(Near(Controls::ClassicPlace(rules, Controls::Id::Reset, kView21).min.x, kView21.x - 64.0, 1e-12));
    // Restart and pause share an edge, so the closed claim of both holds the point on it, and restart - first in the claim
    // order - takes it: the order is what settles it, not the shapes.
    const glm::dvec2 shared(restart.Max().x, 16.0);
    const Controls::HitShape restartHit = Controls::ClassicCornerFrame(restart).hit;
    const Controls::HitShape pauseHit = Controls::ClassicCornerFrame(pause).hit;
    CHECK_MSG(restartHit.Contains(shared) && pauseHit.Contains(shared), "both closed rectangles hold the shared edge");
    CHECK_MSG(Controls::kClaimOrder[2] == Controls::Id::Reset && Controls::kClaimOrder[3] == Controls::Id::Menu,
              "restart is claimed before pause");
}

void TheIdsAreInTheClaimOrderAndOnlyThePadsWalk() {
    CHECK_EQ(static_cast<int>(Controls::kClaimOrder.size()), 5);
    CHECK(Controls::kClaimOrder[0] == Controls::Id::Left);
    CHECK(Controls::kClaimOrder[1] == Controls::Id::Right);
    CHECK(Controls::kClaimOrder[2] == Controls::Id::Reset);
    CHECK(Controls::kClaimOrder[3] == Controls::Id::Menu);
    CHECK(Controls::kClaimOrder[4] == Controls::Id::Clear);
    // The order is the enumeration's own, so a suite that indexes by it and the layer that builds in it agree.
    for (std::size_t i = 0; i < Controls::kClaimOrder.size(); ++i) {
        CHECK_MSG(static_cast<std::size_t>(Controls::kClaimOrder[i]) == i, std::string(NameOf(Controls::kClaimOrder[i])) + " is not at its place");
    }
    CHECK(Controls::IsWalk(Controls::Id::Left));
    CHECK(Controls::IsWalk(Controls::Id::Right));
    CHECK(!Controls::IsWalk(Controls::Id::Reset));
    CHECK(!Controls::IsWalk(Controls::Id::Menu));
    CHECK(!Controls::IsWalk(Controls::Id::Clear));
}

// ======================================================================================================
// The modern controls (planning step 180): controls.json, the layout, the hit rule, the tuning and the
// policy. Every expectation below is a literal from docs/planning/2026-10-15-modern-controls.md (1.2's
// table, Appendix A) or worked from literals in the units that table states them in, never read back
// from the Rules it checks: a number in controls.json that moves must fail a case, whether it is the
// repository's file edited or a copy read through MP_TEST_PORTDATA.
// ======================================================================================================

const std::string kControls = kData + "/controls.json";

constexpr double kRefHeightPx = 720.0;       // Magic Rampage's reference, in which the data is written
constexpr double kPxPerUnit = 2.8125;        // 720 / 256: the capture's px to a design unit
const Controls::Insets kNoInsets;
// The shipped default: the walk arrows SPLIT, walk_left in the bottom-left corner and walk_right in the bottom-right one (planning step 186, the
// owner's decision of 2026-10-16). Every pin written for the grouped pair of planning step 180 is aimed at kGrouped, the arrangement that
// `--controls modern,arrows=grouped` asks for, and kept.
const Controls::Tuning kNoTuning;
const Controls::Tuning kGrouped = [] {
    Controls::Tuning tuning;
    tuning.arrows = Controls::Arrows::Grouped;
    return tuning;
}();
// A tuning for one arrangement, a size and a look, nothing else.
Controls::Tuning TunedFor(Controls::Arrows arrows, std::optional<int> sizeSteps = std::nullopt, std::optional<Controls::Preset> preset = std::nullopt) {
    Controls::Tuning tuning;
    tuning.arrows = arrows;
    tuning.sizeSteps = sizeSteps;
    tuning.preset = preset;
    return tuning;
}
const std::array<Controls::Arrows, 2> kArrangements = {Controls::Arrows::Split, Controls::Arrows::Grouped};
const char* ArrangementName(Controls::Arrows arrows) {
    return arrows == Controls::Arrows::Split ? "split" : "grouped";
}

struct Aspect {
    const char* name;
    double ratio;
};
// The shapes the layout is held at: 5:4 is the narrowest supported window and 32:9 the widest (design 1.7).
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

Controls::Rules LoadModern() {
    Controls::Rules rules;
    std::string error;
    CHECK_MSG(Controls::LoadRules(kControls, rules, error), "controls.json reads: " + error);
    return rules;
}

bool SameBits(const Hud::Rect& a, const Hud::Rect& b) {
    return std::memcmp(&a.min.x, &b.min.x, sizeof(double)) == 0 && std::memcmp(&a.min.y, &b.min.y, sizeof(double)) == 0 &&
           std::memcmp(&a.size.x, &b.size.x, sizeof(double)) == 0 && std::memcmp(&a.size.y, &b.size.y, sizeof(double)) == 0;
}

bool SameBits(const Controls::HitShape& a, const Controls::HitShape& b) {
    return a.kind == b.kind && a.halfOpen == b.halfOpen && SameBits(a.rect, b.rect) &&
           std::memcmp(&a.centre, &b.centre, sizeof(a.centre)) == 0 && std::memcmp(&a.radius, &b.radius, sizeof(double)) == 0;
}

// A rectangle from its edges, as the design's table states it.
struct Edges {
    double l;
    double t;
    double r;
    double b;
};

bool RectIs(const Hud::Rect& rect, const Edges& want, double eps) {
    const glm::dvec2 max = rect.Max();
    return Near(rect.min.x, want.l, eps) && Near(rect.min.y, want.t, eps) && Near(max.x, want.r, eps) && Near(max.y, want.b, eps);
}

std::string EdgesOf(const Hud::Rect& rect) {
    const glm::dvec2 max = rect.Max();
    return "(" + Num(rect.min.x) + ", " + Num(rect.min.y) + ")-(" + Num(max.x) + ", " + Num(max.y) + ")";
}

double AreaOf(const Hud::Rect& rect) {
    return rect.size.x * rect.size.y;
}

double Overlap(const Hud::Rect& a, const Hud::Rect& b) {
    const glm::dvec2 amax = a.Max();
    const glm::dvec2 bmax = b.Max();
    const double w = std::min(amax.x, bmax.x) - std::max(a.min.x, b.min.x);
    const double h = std::min(amax.y, bmax.y) - std::max(a.min.y, b.min.y);
    return (w > 0.0 && h > 0.0) ? w * h : 0.0;
}

// ---- 1.2's table, 16:9, size 0.7, 44 px on every edge. By Id: Left, Right, Reset (restart), Menu (pause), Clear. ----
//
// The GROUPED arrangement's (planning step 180: both arrows at the bottom left) and the SPLIT one's (planning step 186: walk_right in the bottom-right
// corner). The corner buttons are the same in both. The split numbers are worked by out/parity/program/CTL/CTL6b/work/split_numbers.py from the
// design's formulas and the owner's decision, not from the module.

struct Table {
    std::array<Edges, 5> box;
    std::array<Edges, 5> hit;
    std::array<Edges, 5> boxPx;
    std::array<Edges, 5> hitPx;
    std::array<glm::dvec2, 5> centrePx;
    std::array<double, 5> hitArea;
    double total; // the five hit areas together
};

const Table kGroupedTable = {
    {{{15.6444, 208.4978, 47.5022, 240.3556},
      {58.8800, 208.4978, 90.7378, 240.3556},
      {346.1689, 15.6444, 387.1289, 56.6044},
      {398.5067, 15.6444, 439.4667, 56.6044},
      {15.6444, 15.6444, 56.6044, 56.6044}}},
    {{{0.0, 208.4978, 53.1911, 256.0},
      {53.1911, 208.4978, 90.7378, 256.0},
      {346.1689, 0.0, 392.8178, 56.6044},
      {392.8178, 0.0, 455.1111, 56.6044},
      {0.0, 0.0, 56.6044, 56.6044}}},
    {{{44.0, 586.4, 133.6, 676.0},
      {165.6, 586.4, 255.2, 676.0},
      {973.6, 44.0, 1088.8, 159.2},
      {1120.8, 44.0, 1236.0, 159.2},
      {44.0, 44.0, 159.2, 159.2}}},
    {{{0.0, 586.4, 149.6, 720.0},
      {149.6, 586.4, 255.2, 720.0},
      {973.6, 0.0, 1104.8, 159.2},
      {1104.8, 0.0, 1280.0, 159.2},
      {0.0, 0.0, 159.2, 159.2}}},
    {{{88.8, 631.2}, {210.4, 631.2}, {1031.2, 101.6}, {1178.4, 101.6}, {101.6, 101.6}}},
    {2526.7, 1783.6, 2640.5, 3526.1, 3204.1},
    13680.9};

const Table kSplitTable = {
    {{{15.6444, 208.4978, 47.5022, 240.3556},
      {407.6089, 208.4978, 439.4667, 240.3556},
      {346.1689, 15.6444, 387.1289, 56.6044},
      {398.5067, 15.6444, 439.4667, 56.6044},
      {15.6444, 15.6444, 56.6044, 56.6044}}},
    {{{0.0, 208.4978, 47.5022, 256.0},
      {407.6089, 208.4978, 455.1111, 256.0},
      {346.1689, 0.0, 392.8178, 56.6044},
      {392.8178, 0.0, 455.1111, 56.6044},
      {0.0, 0.0, 56.6044, 56.6044}}},
    {{{44.0, 586.4, 133.6, 676.0},
      {1146.4, 586.4, 1236.0, 676.0},
      {973.6, 44.0, 1088.8, 159.2},
      {1120.8, 44.0, 1236.0, 159.2},
      {44.0, 44.0, 159.2, 159.2}}},
    {{{0.0, 586.4, 133.6, 720.0},
      {1146.4, 586.4, 1280.0, 720.0},
      {973.6, 0.0, 1104.8, 159.2},
      {1104.8, 0.0, 1280.0, 159.2},
      {0.0, 0.0, 159.2, 159.2}}},
    {{{88.8, 631.2}, {1191.2, 631.2}, {1031.2, 101.6}, {1178.4, 101.6}, {101.6, 101.6}}},
    {2256.5, 2256.5, 2640.5, 3526.1, 3204.1},
    13883.6};

const Table& TableFor(Controls::Arrows arrows) {
    return arrows == Controls::Arrows::Split ? kSplitTable : kGroupedTable;
}

constexpr double kTable = 6e-5; // the table is written to four decimals

// The classic HUD's dead zone, from literals: two quarter discs of 64 and three squares of 32 (Appendix A: 9,505.98).
const double kClassicPair = 2.0 * (3.14159265358979323846 / 4.0) * 64.0 * 64.0;
const double kClassicTotal = kClassicPair + 3.0 * 32.0 * 32.0;

// ---- the data ---------------------------------------------------------------------------------------------

void TheModernRulesAreTheFilesNumbers() {
    const Hud::Rules hud = Load();
    const Controls::Rules rules = LoadModern();

    // 1 was the pair grouped at the bottom left (planning step 180); 2 is the arrows split, one in each bottom corner (planning step 186).
    CHECK_EQ(Controls::kLayoutVersion, 2);
    CHECK_EQ(rules.layoutVersion, 2);
    CHECK_MSG(rules.defaultArrows == Controls::Arrows::Split, "the arrows ship split, one in each bottom corner (the owner, 2026-10-16)");
    CHECK(rules.referenceHeightPx == 720.0);
    CHECK(rules.clearancePx == 16.0);
    CHECK(rules.marginSidePx == 44.0 && rules.marginTopPx == 44.0 && rules.marginBottomPx == 44.0);
    CHECK(rules.gapPx == 32.0 && rules.boxPx == 128.0);
    CHECK(rules.cornerBoxFactor == 0.9 && rules.cornerMinSize == 1.0);
    CHECK(rules.faceInset == 0.109375);

    // The three looks: a size on the grid and a bottom margin each.
    CHECK_EQ(rules.presets[static_cast<std::size_t>(Controls::Preset::Uniform)].sizeSteps, 7);
    CHECK_EQ(rules.presets[static_cast<std::size_t>(Controls::Preset::Balanced)].sizeSteps, 8);
    CHECK_EQ(rules.presets[static_cast<std::size_t>(Controls::Preset::Mr)].sizeSteps, 10);
    CHECK(rules.presets[static_cast<std::size_t>(Controls::Preset::Uniform)].marginBottomPx == 44.0);
    CHECK(rules.presets[static_cast<std::size_t>(Controls::Preset::Balanced)].marginBottomPx == 34.0);
    CHECK(rules.presets[static_cast<std::size_t>(Controls::Preset::Mr)].marginBottomPx == 11.0);
    CHECK(rules.defaultPreset == Controls::Preset::Uniform);

    // No growth toward the world, for any control (the owner's Q0: the growth is a later, measured data change).
    for (const Controls::Id id : Controls::kClaimOrder) {
        CHECK_MSG(rules.hit.growPx[static_cast<std::size_t>(id)] == 0.0, std::string(Controls::KeyOf(id)) + " grows by 0 px");
    }
    CHECK(rules.hit.seamPx == 32.0 && rules.hit.edgeReachPx == 48.0);
    CHECK(rules.cornerActOn == Controls::ActOn::Release);

    // The grids: eleven sizes, nine opacities, counted in whole steps.
    CHECK(rules.size.min == 0.4 && rules.size.max == 1.4 && rules.size.step == 0.1);
    CHECK_EQ(rules.size.divisor, 10);
    CHECK_EQ(rules.size.lowSteps, 4);
    CHECK_EQ(rules.size.highSteps, 14);
    CHECK(rules.opacity.min == 0.2 && rules.opacity.max == 1.8 && rules.opacity.step == 0.2);
    CHECK_EQ(rules.opacity.divisor, 5);
    CHECK_EQ(rules.opacity.lowSteps, 1);
    CHECK_EQ(rules.opacity.highSteps, 9);
    CHECK_EQ(rules.opacityDefaultSteps, 5);
    CHECK_EQ(rules.baseAlphaByte, 120);
    CHECK(rules.idleScale == 1.0);
    CHECK_EQ(rules.pressedBoostByte, 38);
    CHECK(rules.ringSizeOverBox == 3.125);
    CHECK(rules.hintMarginUnits == 8.0);
    CHECK(rules.census.emUnits == 16.0 && rules.census.slackUnits == 0.0 && rules.census.characterRowUnits == 206.0);
    CHECK(rules.defaultMode == Controls::Style::Classic);
    // The margin a picture the port draws itself may carry: a quarter on each side (sim/PadArt's own bound is this number).
    CHECK(Controls::kMaxGeneratedBodyInset == 0.25);

    // The art: Magic Rampage's five and the generated five by Id, the original's three corner sprites and no walk pad.
    const std::array<const char*, 5> mrFiles = {"walk_left.png", "walk_right.png", "restart.png", "pause.png", "clear_portals.png"};
    const std::array<const char*, 5> generatedFiles = {"modern_walk_left.png", "modern_walk_right.png", "modern_restart.png",
                                                       "modern_pause.png", "modern_clear_portals.png"};
    for (const Controls::Id id : Controls::kClaimOrder) {
        const std::size_t i = static_cast<std::size_t>(id);
        const auto& mr = rules.art[static_cast<std::size_t>(Controls::ArtTier::MagicRampage)][i];
        const auto& generated = rules.art[static_cast<std::size_t>(Controls::ArtTier::Generated)][i];
        const auto& original = rules.art[static_cast<std::size_t>(Controls::ArtTier::Original)][i];
        CHECK_MSG(mr && mr->file == mrFiles[i] && mr->bodyInset == 0.109375, std::string(Controls::KeyOf(id)) + ": Magic Rampage's picture");
        CHECK_MSG(generated && generated->file == generatedFiles[i] && generated->bodyInset == 0.109375,
                  std::string(Controls::KeyOf(id)) + ": the generated picture");
        CHECK_MSG(static_cast<bool>(original) == !Controls::IsWalk(id), std::string(Controls::KeyOf(id)) + ": the original has a sprite for a corner button only");
    }
    const auto& originals = rules.art[static_cast<std::size_t>(Controls::ArtTier::Original)];
    CHECK(originals[2] && originals[2]->bodyInset == 0.0 && originals[3] && originals[3]->bodyInset == 0.0 &&
          originals[4] && originals[4]->bodyInset == 0.0);

    // What this file states of numbers ui.json also states. A divergence is a toggle that changes the picture.
    CHECK_MSG(rules.baseAlphaByte == hud.alphaByte, "the modern base byte is the classic one: " + std::to_string(hud.alphaByte));
    CHECK_MSG(Near(hud.pads.ringSizeUnits / hud.pads.sizeUnits, rules.ringSizeOverBox, 1e-12),
              "the modern ring is the classic ring over its pad: " + Num(hud.pads.ringSizeUnits / hud.pads.sizeUnits));
    CHECK_MSG(originals[2]->file == hud.restart.sprite && originals[3]->file == hud.pause.sprite && originals[4]->file == hud.clearPortals.sprite,
              "the original's three corner sprites are ui.json's own");
    CHECK_MSG(hud.pads.slideFromUnits == 128.0 && hud.pads.slideMs == 700.0, "the slide the modern pair borrows is 128 units over 700 ms");

    // 1-21's hint, which the modern layout moves, starts at 50 in the classic style (camera.json).
    Hint::Rules hint;
    std::string error;
    CHECK_MSG(Hint::LoadRules(kData + "/camera.json", hint, error), "camera.json reads: " + error);
    CHECK_MSG(hint.leftUnits == 50.0 && hint.aboveBottomUnits == 32.0, "the classic hint starts 50 from the left, 32 above the bottom");
}

void TheNamesAreOnePlaceAndRoundTrip() {
    const std::array<const char*, 5> keys = {"walk_left", "walk_right", "restart", "pause", "clear_portals"};
    for (const Controls::Id id : Controls::kClaimOrder) {
        const std::size_t i = static_cast<std::size_t>(id);
        CHECK_MSG(std::string(Controls::KeyOf(id)) == keys[i], std::string("the key of control ") + std::to_string(i));
        const auto back = Controls::IdOfKey(keys[i]);
        CHECK_MSG(back && *back == id, std::string(keys[i]) + " names its control");
    }
    CHECK(!Controls::IdOfKey("").has_value());
    CHECK(!Controls::IdOfKey("Walk_Left").has_value());
    CHECK(!Controls::IdOfKey("walk_left ").has_value());
    CHECK(!Controls::IdOfKey("clear").has_value());

    CHECK(std::string(Controls::NameOf(Controls::Style::Classic)) == "classic" && std::string(Controls::NameOf(Controls::Style::Modern)) == "modern");
    CHECK(Controls::StyleOfName("modern") == Controls::Style::Modern && Controls::StyleOfName("classic") == Controls::Style::Classic);
    CHECK(!Controls::StyleOfName("Modern").has_value() && !Controls::StyleOfName("").has_value() && !Controls::StyleOfName("both").has_value());

    const std::array<const char*, 3> presets = {"uniform", "balanced", "mr"};
    for (std::size_t i = 0; i < Controls::kPresets.size(); ++i) {
        CHECK(std::string(Controls::NameOf(Controls::kPresets[i])) == presets[i]);
        CHECK(Controls::PresetOfName(presets[i]) == Controls::kPresets[i]);
    }
    CHECK(!Controls::PresetOfName("").has_value() && !Controls::PresetOfName("Uniform").has_value() && !Controls::PresetOfName("big").has_value());
    CHECK(std::string(Controls::NameOf(Controls::Arrows::Split)) == "split" && std::string(Controls::NameOf(Controls::Arrows::Grouped)) == "grouped");
    CHECK(Controls::ArrowsOfName("split") == Controls::Arrows::Split && Controls::ArrowsOfName("grouped") == Controls::Arrows::Grouped);
    CHECK(!Controls::ArrowsOfName("").has_value() && !Controls::ArrowsOfName("Split").has_value() && !Controls::ArrowsOfName("together").has_value());
    CHECK(std::string(Controls::NameOf(Controls::ActOn::Release)) == "release" && std::string(Controls::NameOf(Controls::ActOn::Press)) == "press");
    CHECK(std::string(Controls::NameOf(Controls::ArtTier::MagicRampage)) == "mr" && std::string(Controls::NameOf(Controls::ArtTier::Original)) == "original" &&
          std::string(Controls::NameOf(Controls::ArtTier::Generated)) == "generated");
}

// ---- the default places ---------------------------------------------------------------------------------

// One arrangement's places against its table: the boxes, the hit rectangles, the px of a 1280 x 720 capture, the centres and the areas.
void CheckDefaultPlaces(Controls::Arrows arrows) {
    const Hud::Rules hud = Load();
    const Controls::Rules rules = LoadModern();
    const Controls::Tuning tuning = TunedFor(arrows);
    const Table& table = TableFor(arrows);
    const std::string which = std::string(ArrangementName(arrows)) + ": ";

    const Controls::Layout layout = Controls::LayOut(Controls::Style::Modern, rules, hud, tuning, kView169, kNoInsets, 5000.0);
    CHECK(layout.style == Controls::Style::Modern);
    const std::array<Hud::Rect, 5> boxes = Controls::Boxes(rules, tuning, kView169, kNoInsets);
    const std::array<Hud::Rect, 5> hits = Controls::HitRects(rules, boxes, kView169, kNoInsets);
    double total = 0.0;
    for (const Controls::Id id : Controls::kClaimOrder) {
        const std::size_t i = static_cast<std::size_t>(id);
        const std::string name = which + Controls::KeyOf(id);
        const Controls::Frame& frame = layout.At(id);
        // The box and the hit rectangle, in units, against the design's table; and the same through every way of asking.
        CHECK_MSG(RectIs(frame.rect, table.box[i], kTable), name + " box " + EdgesOf(frame.rect));
        CHECK_MSG(SameBits(frame.rect, boxes[i]), name + ": the layout's box is Boxes'");
        CHECK_MSG(frame.hit.kind == Controls::HitShape::Kind::Rect && frame.hit.halfOpen, name + ": the hit is a half-open rectangle");
        CHECK_MSG(RectIs(frame.hit.rect, table.hit[i], kTable), name + " hit " + EdgesOf(frame.hit.rect));
        CHECK_MSG(SameBits(frame.hit.rect, hits[i]), name + ": the layout's hit is HitRects'");
        // And in the px of a 1280 x 720 capture, where the design's table also states them.
        const Hud::Rect px{frame.rect.min * kPxPerUnit, frame.rect.size * kPxPerUnit};
        CHECK_MSG(RectIs(px, table.boxPx[i], 0.06), name + " box in px " + EdgesOf(px));
        const Hud::Rect hitPx{frame.hit.rect.min * kPxPerUnit, frame.hit.rect.size * kPxPerUnit};
        CHECK_MSG(RectIs(hitPx, table.hitPx[i], 0.06), name + " hit in px " + EdgesOf(hitPx));
        const glm::dvec2 centre = frame.rect.Centre() * kPxPerUnit;
        CHECK_MSG(Near(centre.x, table.centrePx[i].x, 0.06) && Near(centre.y, table.centrePx[i].y, 0.06), name + " centre in px (" + Num(centre.x) + ", " + Num(centre.y) + ")");
        CHECK_MSG(Near(AreaOf(frame.hit.rect), table.hitArea[i], 0.06), name + " hit area " + Num(AreaOf(frame.hit.rect)));
        total += AreaOf(frame.hit.rect);
    }
    CHECK_MSG(Near(total, table.total, 0.1), which + "the five hit areas total " + Num(total));

    // The same places worked from the data's own numbers, to a billionth: the margin 44 px, the gap 32, the box 128, the
    // size 0.7 and the corner box 0.9 x 128 (a corner button never under 1.0), all in px of the 720 reference.
    const double u = kHeight / kRefHeightPx;
    const double margin = 44.0 * u;
    const double gap = 32.0 * u;
    const double arrow = 128.0 * u * 0.7;
    const double corner = 0.9 * 128.0 * u * 1.0;
    const double W = kView169.x;
    CHECK(Near(boxes[0].min.x, margin, 1e-9) && Near(boxes[0].min.y, kHeight - margin - arrow, 1e-9) && Near(boxes[0].size.x, arrow, 1e-9));
    CHECK(Near(boxes[1].min.y, kHeight - margin - arrow, 1e-9) && Near(boxes[1].size.x, arrow, 1e-9));
    CHECK(Near(boxes[3].min.x, W - margin - corner, 1e-9) && Near(boxes[3].min.y, margin, 1e-9) && Near(boxes[3].size.x, corner, 1e-9));
    CHECK(Near(boxes[2].min.x, W - margin - corner - gap - corner, 1e-9) && Near(boxes[2].min.y, margin, 1e-9));
    CHECK(Near(boxes[4].min.x, margin, 1e-9) && Near(boxes[4].min.y, margin, 1e-9) && Near(boxes[4].size.y, corner, 1e-9));
    if (arrows == Controls::Arrows::Grouped) {
        // The arrows sit side by side and the corner buttons in a row: nothing is placed but by those four numbers.
        CHECK(Near(boxes[1].min.x, margin + arrow + gap, 1e-9));
        CHECK(Near(boxes[1].min.x - boxes[0].Max().x, gap, 1e-9) && Near(boxes[3].min.x - boxes[2].Max().x, gap, 1e-9));
    } else {
        // Split: walk_right is inset from the bottom-RIGHT corner by the margin on both edges, as walk_left is from the bottom-left, and the
        // two arrows are a view apart (no gap between them, no seam).
        CHECK_MSG(Near(boxes[1].Max().x, W - margin, 1e-9) && Near(kHeight - boxes[1].Max().y, margin, 1e-9), "walk_right is 44 px (15.64 units) from the view's right and bottom edges");
        CHECK_MSG(Near(boxes[0].min.x, margin, 1e-9) && Near(kHeight - boxes[0].Max().y, margin, 1e-9), "and walk_left the same from the left and the bottom");
        CHECK_MSG(Near(boxes[1].min.x - boxes[0].Max().x, W - 2.0 * margin - 2.0 * arrow, 1e-9) && boxes[1].min.x - boxes[0].Max().x > 300.0, "the arrows stand the width of the view apart");
        CHECK(Near(boxes[3].min.x - boxes[2].Max().x, gap, 1e-9));
        // The mirror: the right arrow is the left arrow reflected in the view's middle line.
        CHECK_MSG(Near(boxes[0].min.x + boxes[1].Max().x, W, 1e-9) && Near(boxes[0].Max().x + boxes[1].min.x, W, 1e-9) && boxes[0].min.y == boxes[1].min.y,
                  "walk_right is walk_left mirrored about the view's middle");
    }
}

void TheDefaultPlacesArePinnedBesideTheLayoutNumber() {
    // Whoever changes a default place bumps this number and the file's together; a saved move made against another
    // arrangement is then dropped, not misapplied (design 2.4). This case is what makes them do it. 2 since planning step 186: the right arrow
    // stands in the bottom-right corner (it was 1 with both at the bottom left).
    CHECK_EQ(Controls::kLayoutVersion, 2);
    // The shipped default is the split arrangement, and the bare Tuning asks for it...
    const Controls::Rules rules = LoadModern();
    CHECK_MSG(kNoTuning.ArrowsIn(rules) == Controls::Arrows::Split && !kNoTuning.arrows, "a tuning that says nothing is the data's default, split");
    CHECK_MSG(kGrouped.ArrowsIn(rules) == Controls::Arrows::Grouped, "and `arrows=grouped` asks for the other");
    {
        const std::array<Hud::Rect, 5> bare = Controls::Boxes(rules, kNoTuning, kView169, kNoInsets);
        const std::array<Hud::Rect, 5> split = Controls::Boxes(rules, TunedFor(Controls::Arrows::Split), kView169, kNoInsets);
        for (std::size_t i = 0; i < 5; ++i) CHECK_MSG(SameBits(bare[i], split[i]), "the default tuning lays out split, bit for bit: control " + std::to_string(i));
    }
    // ...and each arrangement against its own table.
    for (const Controls::Arrows arrows : kArrangements) CheckDefaultPlaces(arrows);
}

void ThePresetsKeepThePairTopAtOrBelowTheCharacterRow() {
    const Controls::Rules rules = LoadModern();
    // The standard waist line of the character, in view units (review 2 F1): the pair's top must not be above it, or a
    // recorded winning shot sits under the pair. Worked from the three looks' own sizes and margins, in px of 720.
    constexpr double kCharacterRow = 206.0;
    const double u = kHeight / kRefHeightPx;
    struct Look {
        Controls::Preset preset;
        double size;
        double bottomPx;
        double top;
    };
    const std::array<Look, 3> looks = {{{Controls::Preset::Uniform, 0.7, 44.0, 208.50},
                                        {Controls::Preset::Balanced, 0.8, 34.0, 207.50},
                                        {Controls::Preset::Mr, 1.0, 11.0, 206.58}}};
    for (const Look& look : looks) {
        Controls::Tuning tuning;
        tuning.preset = look.preset;
        const std::string name = Controls::NameOf(look.preset);
        CHECK_MSG(Near(tuning.Size(rules), look.size, 1e-12), name + " is size " + Num(tuning.Size(rules)));
        const std::array<Hud::Rect, 5> boxes = Controls::Boxes(rules, tuning, kView169, kNoInsets);
        const double top = boxes[0].min.y;
        CHECK_MSG(Near(top, look.top, 0.005), name + ": the pair's top is " + Num(top) + ", the design's " + Num(look.top));
        CHECK_MSG(Near(top, kHeight - look.bottomPx * u - 128.0 * u * look.size, 1e-9), name + ": the pair's top worked from its margin and size");
        CHECK_MSG(top >= kCharacterRow, name + ": the pair's top " + Num(top) + " is above the character's row " + Num(kCharacterRow));
        CHECK_MSG(Near(boxes[1].min.y, top, 1e-12), name + ": the two arrows share a top");
        // The corner buttons do not follow the arrows' size under 1.0 (design D1: never under 40.96, the classic's 32 body).
        CHECK_MSG(Near(boxes[3].size.x, 40.96, 1e-9) && Near(boxes[2].size.x, 40.96, 1e-9) && Near(boxes[4].size.x, 40.96, 1e-9),
                  name + ": the corner boxes are 40.96");
        // And the look changes nothing but the arrows' size and bottom margin: the corners are where they were.
        const std::array<Hud::Rect, 5> uniform = Controls::Boxes(rules, kNoTuning, kView169, kNoInsets);
        for (const std::size_t corner : {2u, 3u, 4u}) {
            CHECK_MSG(SameBits(boxes[corner], uniform[corner]), name + ": corner " + std::to_string(corner) + " is the uniform look's");
        }
    }
    // The look in force is the player's when set and the data's default otherwise.
    CHECK(kNoTuning.PresetIn(rules) == Controls::Preset::Uniform);
    Controls::Tuning mr;
    mr.preset = Controls::Preset::Mr;
    CHECK(mr.PresetIn(rules) == Controls::Preset::Mr);
}

void TheSizeStepsEndExactlyOnTheLimits() {
    const Controls::Rules rules = LoadModern();
    // Up from the default 0.7: seven steps to 1.4, each the value n / 10 by one division, which is the double the literal is.
    const std::array<double, 8> up = {0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4};
    Controls::Tuning tuning;
    CHECK(tuning.Size(rules) == up[0]);
    for (std::size_t i = 1; i < up.size(); ++i) {
        tuning = tuning.SizeStepped(rules, +1);
        CHECK_MSG(tuning.Size(rules) == up[i], "step up " + std::to_string(i) + " is " + Num(tuning.Size(rules)));
        CHECK_MSG(tuning.SizeStepsIn(rules) == static_cast<int>(7 + i), "step up " + std::to_string(i) + " is step " + std::to_string(7 + i));
    }
    // A step at the limit gives the same tuning, however many are asked.
    for (int more = 0; more < 3; ++more) {
        const Controls::Tuning again = tuning.SizeStepped(rules, +1);
        CHECK_MSG(again.Size(rules) == 1.4 && again.SizeStepsIn(rules) == 14 && again.sizeSteps == tuning.sizeSteps, "a step up at 1.4 is 1.4");
        tuning = again;
    }
    // And all the way down, through the default, to 0.4: eleven values.
    int visited = 0;
    for (int steps = 14; steps > 4; --steps) {
        tuning = tuning.SizeStepped(rules, -1);
        CHECK_MSG(tuning.SizeStepsIn(rules) == steps - 1 && tuning.Size(rules) == static_cast<double>(steps - 1) / 10.0, "step down to " + std::to_string(steps - 1));
        ++visited;
    }
    CHECK_EQ(visited, 10);
    CHECK_MSG(tuning.Size(rules) == 0.4, "ten steps down from 1.4 is exactly 0.4");
    for (int more = 0; more < 3; ++more) {
        const Controls::Tuning again = tuning.SizeStepped(rules, -1);
        CHECK_MSG(again.Size(rules) == 0.4 && again.SizeStepsIn(rules) == 4, "a step down at 0.4 is 0.4");
        tuning = again;
    }
    // Up three and down three is the default again, and then nothing is stored: a change of the default reaches the player.
    Controls::Tuning there = kNoTuning.SizeStepped(rules, +1).SizeStepped(rules, +1).SizeStepped(rules, +1);
    CHECK(there.sizeSteps && *there.sizeSteps == 10 && !there.IsDefault(rules));
    there = there.SizeStepped(rules, -1).SizeStepped(rules, -1).SizeStepped(rules, -1);
    CHECK_MSG(!there.sizeSteps && there.IsDefault(rules), "back on the default, the size is unset");
    CHECK(kNoTuning.SizeStepped(rules, 0).IsDefault(rules));

    // The steps are the look's own: the same ten steps of 0.1 from balanced's 0.8 and mr's 1.0, ending on the same limits.
    for (const Controls::Preset preset : Controls::kPresets) {
        Controls::Tuning t;
        t.preset = preset;
        for (int i = 0; i < 20; ++i) t = t.SizeStepped(rules, +1);
        CHECK_MSG(t.Size(rules) == 1.4, std::string(Controls::NameOf(preset)) + " ends on 1.4");
        for (int i = 0; i < 20; ++i) t = t.SizeStepped(rules, -1);
        CHECK_MSG(t.Size(rules) == 0.4, std::string(Controls::NameOf(preset)) + " ends on 0.4");
    }

    // The opacity: nine values from 0.2 to 1.8 in steps of 0.2, the same way.
    Controls::Tuning level;
    CHECK(level.Opacity(rules) == 1.0 && level.OpacityStepsIn(rules) == 5);
    const std::array<double, 4> brighter = {1.2, 1.4, 1.6, 1.8};
    for (const double want : brighter) {
        level = level.OpacityStepped(rules, +1);
        CHECK_MSG(level.Opacity(rules) == want, "opacity up to " + Num(want) + " is " + Num(level.Opacity(rules)));
    }
    CHECK(level.OpacityStepped(rules, +1).Opacity(rules) == 1.8 && level.OpacityStepped(rules, +1).opacitySteps == level.opacitySteps);
    for (int i = 0; i < 8; ++i) level = level.OpacityStepped(rules, -1);
    CHECK_MSG(level.Opacity(rules) == 0.2 && level.OpacityStepsIn(rules) == 1, "eight steps down from 1.8 is exactly 0.2");
    CHECK(level.OpacityStepped(rules, -1).Opacity(rules) == 0.2);
    CHECK_MSG(level.OpacityStepped(rules, +1).OpacityStepped(rules, +1).OpacityStepped(rules, +1).OpacityStepped(rules, +1).IsDefault(rules),
              "four steps up from 0.2 is the default 1.0");

    // A long random walk against the integer model: the steps are what is counted, so no drift can build up.
    std::mt19937_64 rng(20261015);
    int model = 7;
    Controls::Tuning walked;
    long mismatches = 0;
    for (int i = 0; i < 4000; ++i) {
        const int direction = (rng() % 2) == 0 ? -1 : +1;
        model = std::clamp(model + direction, 4, 14);
        walked = walked.SizeStepped(rules, direction);
        if (walked.SizeStepsIn(rules) != model || walked.Size(rules) != static_cast<double>(model) / 10.0) ++mismatches;
    }
    CHECK_MSG(mismatches == 0, "4,000 steps drifted from the integer model " + std::to_string(mismatches) + " times");
}

void MarginsAndGapsDoNotGrowWithSize() {
    const Controls::Rules rules = LoadModern();
    const double margin = 44.0 * kHeight / kRefHeightPx;
    const double gap = 32.0 * kHeight / kRefHeightPx;
    for (const Controls::Arrows arrows : kArrangements) for (int steps = 4; steps <= 14; ++steps) {
        const Controls::Tuning tuning = TunedFor(arrows, steps);
        const std::array<Hud::Rect, 5> b = Controls::Boxes(rules, tuning, kView169, kNoInsets);
        const std::string at = std::string(ArrangementName(arrows)) + " size " + Num(static_cast<double>(steps) / 10.0) + ": ";
        // Every control is the same distance from its two corner edges at every size, and the neighbours the same gap apart.
        CHECK_MSG(Near(b[0].min.x, margin, 1e-9) && Near(kHeight - b[0].Max().y, margin, 1e-9), at + "walk_left keeps its margins from the bottom-left");
        CHECK_MSG(Near(b[0].min.y, b[1].min.y, 1e-12), at + "the arrows share a top");
        if (arrows == Controls::Arrows::Grouped) {
            CHECK_MSG(Near(b[1].min.x - b[0].Max().x, gap, 1e-9), at + "the grouped arrows keep their gap");
        } else {
            // Split: walk_right keeps the same margins from the bottom-RIGHT corner, whatever the size.
            CHECK_MSG(Near(kView169.x - b[1].Max().x, margin, 1e-9) && Near(kHeight - b[1].Max().y, margin, 1e-9), at + "walk_right keeps its margins from the bottom-right");
        }
        CHECK_MSG(Near(kView169.x - b[3].Max().x, margin, 1e-9) && Near(b[3].min.y, margin, 1e-9), at + "pause keeps its margins from the top-right");
        CHECK_MSG(Near(b[3].min.x - b[2].Max().x, gap, 1e-9) && Near(b[2].min.y, margin, 1e-9), at + "restart keeps its gap to pause");
        CHECK_MSG(Near(b[4].min.x, margin, 1e-9) && Near(b[4].min.y, margin, 1e-9), at + "clear-portals keeps its margins from the top-left");
        // The arrow is the box (128 px) times the size.
        CHECK_MSG(Near(b[0].size.x, 128.0 * kHeight / kRefHeightPx * static_cast<double>(steps) / 10.0, 1e-9), at + "the arrow's box");
    }
}

void ACornerButtonNeverShrinksBelowItsMinimumSize() {
    const Controls::Rules rules = LoadModern();
    // 0.9 x 128 px = 40.96 units whose solid face (the box less 0.109375 a side) is exactly the classic corner button's 32.
    const std::array<std::pair<int, double>, 5> corners = {{{4, 40.96}, {7, 40.96}, {10, 40.96}, {12, 49.152}, {14, 57.344}}};
    for (const auto& [steps, want] : corners) {
        Controls::Tuning tuning;
        tuning.sizeSteps = steps;
        const std::array<Hud::Rect, 5> b = Controls::Boxes(rules, tuning, kView169, kNoInsets);
        for (const std::size_t corner : {2u, 3u, 4u}) {
            CHECK_MSG(Near(b[corner].size.x, want, 1e-9) && Near(b[corner].size.y, want, 1e-9),
                      "size " + Num(static_cast<double>(steps) / 10.0) + ": corner " + std::to_string(corner) + " is " + Num(b[corner].size.x));
        }
    }
    Controls::Tuning smallest;
    smallest.sizeSteps = 4;
    const Hud::Rect box = Controls::Boxes(rules, smallest, kView169, kNoInsets)[3];
    const Hud::Rect face = Controls::DrawRect(box, 0.109375, 0.109375);
    CHECK_MSG(Near(box.size.x * (1.0 - 2.0 * 0.109375), 32.0, 1e-9), "a corner box's solid face is the classic button's 32 units");
    CHECK_MSG(Near(face.size.x, 40.96, 1e-9), "a Magic Rampage picture fills the box");
}

// ---- design 1.2's size table, and Appendix A's rows for the narrowest windows --------------------------------------------

// The plan prints a number to the precision it has, and each is held to it: a pair's area to a tenth (0.06), the table's boxes and edges to four
// decimals (kTable, 6e-5), Appendix A's corner x to three decimals (6e-4) and its clear-portals edge to two (6e-3). Beside each printed number is
// the same number worked from the plan's own formula (the margin 44 px, the gap 32, the box 128 and the 720 reference, by height), to four
// decimals, which rounds to the printed one and is what the tolerance 6e-5 is about.
void ThePlansSizeTableAndItsNarrowWindowRowsArePinned() {
    const Controls::Rules rules = LoadModern();
    struct Column {
        int steps;           // the size's step on the grid: 4 is 0.4
        double arrow;        // an arrow's box
        double corner;       // a corner button's box, 0.9 x 128 px x max(size, 1)
        double pairRight;    // the right arrow's right edge
        double pairTop;      // both arrows' top
        double pairHitArea;  // the two arrows' hit rectangles together, u^2
        double ofClassic;    // that against the classic discs' 6,434.0, to two decimals
    };
    const std::array<Column, 5> columns = {{{4, 18.2044, 40.96, 63.4311, 222.1511, 2147.1, 0.33},
                                            {7, 31.8578, 40.96, 90.7378, 208.4978, 4310.2, 0.67},
                                            {10, 45.5111, 40.96, 118.0444, 194.8444, 7219.1, 1.12},
                                            {12, 54.6133, 49.152, 136.2489, 185.7422, 9572.5, 1.49},
                                            {14, 63.7156, 57.344, 154.4533, 176.6400, 12257.4, 1.91}}};
    // The plan's table is the GROUPED pair's (planning step 180); the split arrows' own columns follow it.
    for (const Column& c : columns) {
        const Controls::Tuning tuning = TunedFor(Controls::Arrows::Grouped, c.steps);
        const std::array<Hud::Rect, 5> boxes = Controls::Boxes(rules, tuning, kView169, kNoInsets);
        const std::array<Hud::Rect, 5> hits = Controls::HitRects(rules, boxes, kView169, kNoInsets);
        const std::string at = "size " + Num(static_cast<double>(c.steps) / 10.0) + ": ";
        CHECK_MSG(Near(boxes[0].size.x, c.arrow, kTable) && Near(boxes[1].size.x, c.arrow, kTable) && Near(boxes[0].size.y, c.arrow, kTable), at + "the arrow's box is " + Num(boxes[0].size.x));
        for (const std::size_t corner : {2u, 3u, 4u}) {
            CHECK_MSG(Near(boxes[corner].size.x, c.corner, kTable) && Near(boxes[corner].size.y, c.corner, kTable), at + "corner " + std::to_string(corner) + " is " + Num(boxes[corner].size.x));
        }
        CHECK_MSG(Near(boxes[1].Max().x, c.pairRight, kTable), at + "the pair's right edge is " + Num(boxes[1].Max().x));
        CHECK_MSG(Near(boxes[0].min.y, c.pairTop, kTable) && Near(boxes[1].min.y, c.pairTop, kTable), at + "the pair's top is " + Num(boxes[0].min.y));
        // The pair's hit area: the two rectangles together, which is what the plan prints, not a sum of two rounded areas.
        const double area = AreaOf(hits[0]) + AreaOf(hits[1]);
        CHECK_MSG(Near(area, c.pairHitArea, 0.06), at + "the pair's hit area is " + Num(area) + ", the plan's " + Num(c.pairHitArea));
        CHECK_MSG(Near(area / kClassicPair, c.ofClassic, 0.0051), at + "the pair's hit area is " + Num(area / kClassicPair) + " of the classic discs', the plan's " + Num(c.ofClassic));
    }

    // Appendix A: at the narrowest supported windows, with the arrows at their largest (size 1.4, so the corner boxes are 57.344), where the pair
    // and the corner buttons are closest: pause and restart's left edges, clear-portals' right edge, the pair's right edge and top.
    struct Narrow {
        const char* name;
        double ratio;
        double pauseX;        // worked from the formula, four decimals
        double pauseXPlan;    // as the plan prints it
        double restartX;
        double restartXPlan;
        double clearRight;
        double clearRightPlan;
    };
    const std::array<Narrow, 2> narrow = {{{"5:4 (W 320)", 1.25, 247.0116, 247.012, 178.2898, 178.290, 72.9884, 72.99},
                                           {"4:3 (W 341.333)", 4.0 / 3.0, 268.3449, 268.345, 199.6231, 199.623, 72.9884, 72.99}}};
    const Controls::Tuning largest = TunedFor(Controls::Arrows::Grouped, 14);
    for (const Narrow& n : narrow) {
        const glm::dvec2 view = ViewOf(n.ratio);
        const std::array<Hud::Rect, 5> boxes = Controls::Boxes(rules, largest, view, kNoInsets);
        const std::string at = std::string(n.name) + " size 1.4: ";
        CHECK_MSG(Near(boxes[3].min.x, n.pauseX, kTable) && Near(boxes[3].min.x, n.pauseXPlan, 6e-4), at + "pause.x is " + Num(boxes[3].min.x) + ", the plan's " + Num(n.pauseXPlan));
        CHECK_MSG(Near(boxes[2].min.x, n.restartX, kTable) && Near(boxes[2].min.x, n.restartXPlan, 6e-4), at + "restart.x is " + Num(boxes[2].min.x) + ", the plan's " + Num(n.restartXPlan));
        CHECK_MSG(Near(boxes[4].Max().x, n.clearRight, kTable) && Near(boxes[4].Max().x, n.clearRightPlan, 6e-3), at + "clear-portals' right edge is " + Num(boxes[4].Max().x) + ", the plan's " + Num(n.clearRightPlan));
        // The pair is the bottom-left group and never moves with the window: its right edge and top are the 16:9 table's last column.
        CHECK_MSG(Near(boxes[1].Max().x, 154.4533, kTable) && Near(boxes[0].min.y, 176.64, kTable), at + "the pair's right edge " + Num(boxes[1].Max().x) + " and top " + Num(boxes[0].min.y));
        // And the corner row stays right of the clear-portals box and left of the window: pause ends exactly 44 px (15.6444) from the right edge.
        CHECK_MSG(Near(view.x - boxes[3].Max().x, 44.0 / kPxPerUnit, 1e-9) && boxes[2].Max().x < boxes[3].min.x && boxes[4].Max().x < boxes[2].min.x,
                  at + "the corner row is in order and pause keeps its margin from the right edge");
    }

    // The SPLIT arrows' own columns (planning step 186), the same five sizes, worked by CTL6b/work/split_numbers.py from the design's formula: each
    // arrow's hit is a square from the view's edge to the far side of its box, so its area is (15.6444 + the box)^2, and the pair of them is 0.70 of the
    // classic discs at the default (against the grouped pair's 0.67) and passes the classic total at size 1.0.
    struct SplitColumn {
        int steps;
        double leftHitRight;  // the left arrow's hit right edge (= its box's)
        double rightBoxLeft;  // the right arrow's box left edge (= its hit's)
        double top;
        double oneArea;       // one arrow's hit
        double twoArea;       // both
        double ofClassic;     // both against the classic discs' 6,434.0, to two decimals
    };
    const std::array<SplitColumn, 5> splitColumns = {{{4, 33.8489, 421.2622, 222.1511, 1145.7, 2291.5, 0.36},
                                                      {7, 47.5022, 407.6089, 208.4978, 2256.5, 4512.9, 0.70},
                                                      {10, 61.1556, 393.9556, 194.8444, 3740.0, 7480.0, 1.16},
                                                      {12, 70.2578, 384.8533, 185.7422, 4936.2, 9872.3, 1.53},
                                                      {14, 79.3600, 375.7511, 176.6400, 6298.0, 12596.0, 1.96}}};
    for (const SplitColumn& c : splitColumns) {
        const Controls::Tuning tuning = TunedFor(Controls::Arrows::Split, c.steps);
        const std::array<Hud::Rect, 5> boxes = Controls::Boxes(rules, tuning, kView169, kNoInsets);
        const std::array<Hud::Rect, 5> hits = Controls::HitRects(rules, boxes, kView169, kNoInsets);
        const std::string at = "split size " + Num(static_cast<double>(c.steps) / 10.0) + ": ";
        CHECK_MSG(Near(boxes[0].Max().x, c.leftHitRight, kTable) && Near(hits[0].Max().x, c.leftHitRight, kTable) && hits[0].min.x == 0.0, at + "the left arrow's hit runs from the view's edge to " + Num(hits[0].Max().x));
        CHECK_MSG(Near(boxes[1].min.x, c.rightBoxLeft, kTable) && Near(hits[1].min.x, c.rightBoxLeft, kTable) && hits[1].Max().x == kView169.x, at + "the right arrow's hit runs from " + Num(hits[1].min.x) + " to the view's edge");
        CHECK_MSG(Near(boxes[0].min.y, c.top, kTable) && Near(boxes[1].min.y, c.top, kTable) && hits[0].Max().y == kHeight && hits[1].Max().y == kHeight, at + "both reach the view's bottom from " + Num(boxes[0].min.y));
        CHECK_MSG(Near(AreaOf(hits[0]), c.oneArea, 0.06) && Near(AreaOf(hits[1]), c.oneArea, 0.06), at + "each arrow's hit area is " + Num(AreaOf(hits[0])) + ", the model's " + Num(c.oneArea));
        const double area = AreaOf(hits[0]) + AreaOf(hits[1]);
        CHECK_MSG(Near(area, c.twoArea, 0.1) && Near(area / kClassicPair, c.ofClassic, 0.0051), at + "the two together are " + Num(area) + ", " + Num(area / kClassicPair) + " of the classic discs'");
    }
    // At the narrowest windows and the largest arrows the two arrows are still far apart: 5:4 and 4:3 at size 1.4 (the right arrow's box begins at
    // W - 15.6444 - 63.7156, the left one's hit ends at 79.36).
    for (const Narrow& n : narrow) {
        const glm::dvec2 view = ViewOf(n.ratio);
        const Controls::Tuning tuning = TunedFor(Controls::Arrows::Split, 14);
        const std::array<Hud::Rect, 5> boxes = Controls::Boxes(rules, tuning, view, kNoInsets);
        const std::array<Hud::Rect, 5> hits = Controls::HitRects(rules, boxes, view, kNoInsets);
        const std::string at = std::string("split ") + n.name + " size 1.4: ";
        CHECK_MSG(Near(hits[1].min.x, view.x - 15.6444 - 63.7156, kTable) && Near(hits[0].Max().x, 79.36, kTable), at + "the arrows' hits end " + Num(hits[0].Max().x) + " and begin " + Num(hits[1].min.x));
        CHECK_MSG(hits[1].min.x - hits[0].Max().x > 150.0, at + "and are " + Num(hits[1].min.x - hits[0].Max().x) + " apart");
    }
}

// ---- the hit rule ---------------------------------------------------------------------------------------------

void NoTwoDefaultBoxesOverlapAtAnyAspectOrSize() {
    const Hud::Rules hud = Load();
    const Controls::Rules rules = LoadModern();
    long boxPairs = 0;
    long hitPairs = 0;
    for (const Controls::Arrows arrows : kArrangements) for (const Aspect& aspect : kAspects) {
        const glm::dvec2 view = ViewOf(aspect.ratio);
        for (int steps = 4; steps <= 14; ++steps) {
            const Controls::Tuning tuning = TunedFor(arrows, steps);
            const Controls::Layout layout = Controls::LayOut(Controls::Style::Modern, rules, hud, tuning, view, kNoInsets, 5000.0);
            const std::string at = std::string(ArrangementName(arrows)) + " " + aspect.name + " size " + Num(static_cast<double>(steps) / 10.0) + ": ";
            for (std::size_t i = 0; i < 5; ++i) {
                const Hud::Rect& box = layout.frames[i].rect;
                const glm::dvec2 max = box.Max();
                CHECK_MSG(box.min.x >= 15.6 && box.min.y >= 15.6 && max.x <= view.x - 15.6 && max.y <= view.y - 15.6,
                          at + "control " + std::to_string(i) + " is inside the margin: " + EdgesOf(box));
                for (std::size_t j = i + 1; j < 5; ++j) {
                    ++boxPairs;
                    CHECK_MSG(Overlap(layout.frames[i].rect, layout.frames[j].rect) <= 1e-9,
                              at + "boxes " + std::to_string(i) + " and " + std::to_string(j) + " overlap");
                    ++hitPairs;
                    CHECK_MSG(Overlap(layout.frames[i].hit.rect, layout.frames[j].hit.rect) <= 1e-9,
                              at + "hit rectangles " + std::to_string(i) + " and " + std::to_string(j) + " overlap");
                }
            }
        }
    }
    CHECK_EQ(static_cast<int>(boxPairs), 2 * 9 * 11 * 10); // two arrangements
    CHECK_EQ(static_cast<int>(hitPairs), 2 * 9 * 11 * 10);
}

void CheckSeams(Controls::Arrows arrows) {
    const Hud::Rules hud = Load();
    const Controls::Rules rules = LoadModern();
    const Controls::Tuning tuning = TunedFor(arrows);
    const bool grouped = arrows == Controls::Arrows::Grouped;
    const std::string which = std::string(ArrangementName(arrows)) + ": ";
    const Controls::Layout layout = Controls::LayOut(Controls::Style::Modern, rules, hud, tuning, kView169, kNoInsets, 5000.0);
    const Controls::Frame& left = layout.At(Controls::Id::Left);
    const Controls::Frame& right = layout.At(Controls::Id::Right);
    const Controls::Frame& restart = layout.At(Controls::Id::Reset);
    const Controls::Frame& pause = layout.At(Controls::Id::Menu);

    if (grouped) {
        // The arrows' gap is 11.3778 and is split at its middle, 53.1911: the left arrow's hit ends there, the right one's starts there.
        const double mid = (left.rect.Max().x + right.rect.min.x) * 0.5;
        CHECK_MSG(Near(mid, 53.1911, kTable), which + "the arrows' seam is at " + Num(mid));
        CHECK_MSG(left.hit.rect.Max().x == mid && right.hit.rect.min.x == mid, which + "both hit rectangles meet on the very same double");
        const glm::dvec2 onSeam(mid, 224.0);
        CHECK_MSG(!left.hit.Contains(onSeam) && right.hit.Contains(onSeam), which + "a point on the shared edge belongs to the later control, the right arrow");
        CHECK(left.hit.Contains(glm::dvec2(mid - 1e-6, 224.0)) && !right.hit.Contains(glm::dvec2(mid - 1e-6, 224.0)));
        CHECK(!left.hit.Contains(glm::dvec2(mid + 1e-6, 224.0)) && right.hit.Contains(glm::dvec2(mid + 1e-6, 224.0)));
    } else {
        // Split: the arrows stand the width of the view apart, so there is NO seam between them (planning step 186): each keeps the side of its
        // box that faces the middle of the view, and what lies between them is the world, where a tap fires a portal.
        CHECK_MSG(left.hit.rect.Max().x == left.rect.Max().x && right.hit.rect.min.x == right.rect.min.x, which + "each arrow's hit ends where its box does, toward the middle of the view");
        for (const double x : {kView169.x * 0.5, left.rect.Max().x + 0.01, right.rect.min.x - 0.01, left.rect.Max().x + 100.0}) {
            CHECK_MSG(!left.hit.Contains(glm::dvec2(x, 230.0)) && !right.hit.Contains(glm::dvec2(x, 230.0)), which + "between the arrows (x " + Num(x) + ") is the world");
        }
        // And the arrows' own far sides are the view's edges: a point on the left edge is the left arrow's, one 0.01 inside the right edge the right's.
        CHECK(left.hit.Contains(glm::dvec2(0.0, 230.0)) && right.hit.Contains(glm::dvec2(kView169.x - 0.01, 230.0)));
    }
    // The same for restart and pause, 11.3778 apart, split at 392.8178.
    const double mid2 = (restart.rect.Max().x + pause.rect.min.x) * 0.5;
    CHECK_MSG(Near(mid2, 392.8178, kTable), which + "restart and pause's seam is at " + Num(mid2));
    CHECK_MSG(restart.hit.rect.Max().x == mid2 && pause.hit.rect.min.x == mid2, which + "restart's hit ends where pause's begins, on the same double");
    CHECK(!restart.hit.Contains(glm::dvec2(mid2, 30.0)) && pause.hit.Contains(glm::dvec2(mid2, 30.0)));
    CHECK(restart.hit.Contains(glm::dvec2(mid2 - 1e-6, 30.0)) && !pause.hit.Contains(glm::dvec2(mid2 - 1e-6, 30.0)));

    // On every quarter unit of the view at 16:9 and 4:3, a point is claimed by at most one control; grouped: every point between the arrows'
    // boxes, in the seam, is claimed by one of the pair (no dead ground in a seam); split: every point between the arrows is the world's.
    for (const glm::dvec2 view : {kView169, kView43}) {
        const Controls::Layout laid = Controls::LayOut(Controls::Style::Modern, rules, hud, tuning, view, kNoInsets, 5000.0);
        long twice = 0;
        long claimed = 0;
        long seamPoints = 0;
        long seamDead = 0;
        long betweenClaimed = 0;
        long betweenPoints = 0;
        Grid(view, [&](const glm::dvec2& p) {
            int claims = 0;
            for (const Controls::Frame& frame : laid.frames) claims += frame.hit.Contains(p) ? 1 : 0;
            if (claims > 1) ++twice;
            if (claims == 1) ++claimed;
            const Hud::Rect& a = laid.At(Controls::Id::Left).rect;
            const Hud::Rect& b = laid.At(Controls::Id::Right).rect;
            if (p.x > a.Max().x && p.x < b.min.x && p.y >= a.min.y && p.y < a.Max().y) {
                ++seamPoints;
                if (!laid.At(Controls::Id::Left).hit.Contains(p) && !laid.At(Controls::Id::Right).hit.Contains(p)) ++seamDead;
                ++betweenPoints;
                if (laid.At(Controls::Id::Left).hit.Contains(p) || laid.At(Controls::Id::Right).hit.Contains(p)) ++betweenClaimed;
            }
        });
        const std::string at = which + ShapeOf(view) + ": ";
        CHECK_MSG(twice == 0, at + std::to_string(twice) + " points are claimed twice");
        CHECK_MSG(claimed > 20000, at + "the lattice met " + std::to_string(claimed) + " claimed points");
        if (grouped) {
            CHECK_MSG(seamPoints > 100 && seamDead == 0, at + std::to_string(seamDead) + " of " + std::to_string(seamPoints) + " seam points are dead");
        } else {
            CHECK_MSG(betweenPoints > 10000 && betweenClaimed == 0, at + std::to_string(betweenClaimed) + " of " + std::to_string(betweenPoints) + " points between the arrows are claimed");
        }
    }

    // The seam is on one double at every aspect and size, because the rectangles are made so that min + size is the edge.
    long apart = 0;
    for (const Aspect& aspect : kAspects) {
        const glm::dvec2 view = ViewOf(aspect.ratio);
        for (int steps = 4; steps <= 14; ++steps) {
            const Controls::Layout l = Controls::LayOut(Controls::Style::Modern, rules, hud, TunedFor(arrows, steps), view, kNoInsets, 5000.0);
            if (grouped && l.At(Controls::Id::Left).hit.rect.Max().x != l.At(Controls::Id::Right).hit.rect.min.x) ++apart;
            if (l.At(Controls::Id::Reset).hit.rect.Max().x != l.At(Controls::Id::Menu).hit.rect.min.x) ++apart;
        }
    }
    CHECK_MSG(apart == 0, which + std::to_string(apart) + " seams are not on one double");

    // With more than one neighbour on a side (a moved control), the nearest decides: a synthetic row of boxes.
    std::array<Hud::Rect, 5> boxes = Controls::Boxes(rules, tuning, kView169, kNoInsets);
    boxes[0] = Hud::Rect{glm::dvec2(100.0, 100.0), glm::dvec2(20.0, 20.0)};
    boxes[1] = Hud::Rect{glm::dvec2(130.0, 100.0), glm::dvec2(20.0, 20.0)}; // 10 away on the right
    boxes[4] = Hud::Rect{glm::dvec2(70.0, 100.0), glm::dvec2(20.0, 20.0)};  // 10 away on the left
    boxes[2] = Hud::Rect{glm::dvec2(300.0, 0.0), glm::dvec2(20.0, 20.0)};
    boxes[3] = Hud::Rect{glm::dvec2(330.0, 0.0), glm::dvec2(20.0, 20.0)};
    const std::array<Hud::Rect, 5> hits = Controls::HitRects(rules, boxes, kView169, kNoInsets);
    CHECK_MSG(Near(hits[0].min.x, 95.0, 1e-9) && Near(hits[0].Max().x, 125.0, 1e-9), which + "a box between two neighbours 10 away on each side is cut at both middles");
    CHECK_MSG(Near(hits[1].min.x, 125.0, 1e-9) && Near(hits[4].Max().x, 95.0, 1e-9), which + "and each neighbour is cut at the same middle");
    // A gap wider than the seam is not split: 40 px = 14.2 units is more than 32 px.
    boxes[1] = Hud::Rect{glm::dvec2(160.0, 100.0), glm::dvec2(20.0, 20.0)};
    const std::array<Hud::Rect, 5> apartHits = Controls::HitRects(rules, boxes, kView169, kNoInsets);
    CHECK_MSG(Near(apartHits[0].Max().x, 120.0, 1e-9) && Near(apartHits[1].min.x, 160.0, 1e-9), which + "a gap of 40 units is wider than the seam and is left alone");
}

void TheSeamIsSplitAtItsMiddleAndNothingOverlaps() {
    for (const Controls::Arrows arrows : kArrangements) CheckSeams(arrows);
}

void TheEdgeReachGoesToTheViewEdgeOnlyWithinTheReach() {
    const Hud::Rules hud = Load();
    const Controls::Rules rules = LoadModern();
    // The GROUPED arrows first (planning step 180's cases), then the split ones.
    const Controls::Layout layout = Controls::LayOut(Controls::Style::Modern, rules, hud, kGrouped, kView169, kNoInsets, 5000.0);
    const auto box = [&](Controls::Id id) { return layout.At(id).rect; };
    const auto hit = [&](Controls::Id id) { return layout.At(id).hit.rect; };

    // The 15.64-unit margin is within the 17.07-unit reach: the sides toward a view edge go to it.
    CHECK(hit(Controls::Id::Left).min.x == 0.0 && hit(Controls::Id::Left).Max().y == kHeight);
    CHECK(hit(Controls::Id::Right).Max().y == kHeight);
    CHECK(hit(Controls::Id::Menu).Max().x == kView169.x && hit(Controls::Id::Menu).min.y == 0.0);
    CHECK(hit(Controls::Id::Reset).min.y == 0.0);
    CHECK(hit(Controls::Id::Clear).min.x == 0.0 && hit(Controls::Id::Clear).min.y == 0.0);
    // The sides toward the world are the box's own: the arrow's top, the right arrow's right side, a corner button's bottom.
    CHECK(hit(Controls::Id::Left).min.y == box(Controls::Id::Left).min.y);
    CHECK(hit(Controls::Id::Right).min.y == box(Controls::Id::Right).min.y && hit(Controls::Id::Right).Max().x == box(Controls::Id::Right).Max().x);
    CHECK(hit(Controls::Id::Menu).Max().y == box(Controls::Id::Menu).Max().y);
    CHECK(hit(Controls::Id::Reset).Max().y == box(Controls::Id::Reset).Max().y);
    CHECK(hit(Controls::Id::Clear).Max().x == box(Controls::Id::Clear).Max().x && hit(Controls::Id::Clear).Max().y == box(Controls::Id::Clear).Max().y);
    // A tap in the strip between the box and the screen's corner is the control's, not a portal.
    CHECK(layout.At(Controls::Id::Left).hit.Contains(glm::dvec2(3.0, 252.0)));
    CHECK(layout.At(Controls::Id::Menu).hit.Contains(glm::dvec2(kView169.x - 3.0, 3.0)));
    CHECK(layout.At(Controls::Id::Clear).hit.Contains(glm::dvec2(3.0, 3.0)));
    // And just above the left arrow's box, toward the world, a tap fires.
    CHECK(!layout.At(Controls::Id::Left).hit.Contains(glm::dvec2(10.0, box(Controls::Id::Left).min.y - 0.01)));

    // SPLIT (planning step 186): walk_right is in the bottom-right corner and reaches the view's right edge and bottom, as walk_left reaches the
    // left edge and the bottom; each keeps its box's own top and the side of its box that faces the middle of the view.
    {
        const Controls::Layout split = Controls::LayOut(Controls::Style::Modern, rules, hud, kNoTuning, kView169, kNoInsets, 5000.0);
        const Hud::Rect& l = split.At(Controls::Id::Left).hit.rect;
        const Hud::Rect& r = split.At(Controls::Id::Right).hit.rect;
        const Hud::Rect& lb = split.At(Controls::Id::Left).rect;
        const Hud::Rect& rb = split.At(Controls::Id::Right).rect;
        CHECK_MSG(l.min.x == 0.0 && l.Max().y == kHeight && l.min.y == lb.min.y && l.Max().x == lb.Max().x, "split: the left arrow reaches the left edge and the bottom, and keeps its top and its inner side");
        CHECK_MSG(r.Max().x == kView169.x && r.Max().y == kHeight && r.min.y == rb.min.y && r.min.x == rb.min.x, "split: the right arrow reaches the right edge and the bottom, and keeps its top and its inner side");
        // A tap in the strip between the right arrow's box and the view's bottom-right corner is the arrow's, not a portal.
        CHECK(split.At(Controls::Id::Right).hit.Contains(glm::dvec2(kView169.x - 3.0, 252.0)));
        CHECK(split.At(Controls::Id::Right).hit.Contains(glm::dvec2(kView169.x - 0.01, kHeight - 0.01)));
        // And just above the right arrow's box, and just left of it, toward the world, a tap fires.
        CHECK(!split.At(Controls::Id::Right).hit.Contains(glm::dvec2(kView169.x - 20.0, rb.min.y - 0.01)));
        CHECK(!split.At(Controls::Id::Right).hit.Contains(glm::dvec2(rb.min.x - 0.01, 232.0)));
    }

    // The reach is the data's: at 43.99 px a margin of 44 px is out of it, at exactly 44 px (the equality is in) and
    // above it is in, and at 0 nothing reaches. The px are the reference's, and the boxes sit 44 px from the edges.
    const double u = kHeight / kRefHeightPx;
    struct Probe {
        double reachPx;
        bool reaches;
    };
    for (const Probe& probe : {Probe{43.99, false}, Probe{44.0, true}, Probe{44.01, true}, Probe{48.0, true}, Probe{0.0, false}}) {
        Controls::Rules edited = rules;
        edited.hit.edgeReachPx = probe.reachPx;
        const std::array<Hud::Rect, 5> boxes = Controls::Boxes(edited, kNoTuning, kView169, kNoInsets);
        {
            // Split: the right arrow's right side and bottom reach the view's edge exactly when the reach does.
            const std::array<Hud::Rect, 5> splitHits = Controls::HitRects(edited, boxes, kView169, kNoInsets);
            CHECK_MSG((splitHits[1].Max().x == kView169.x) == probe.reaches && (splitHits[1].Max().y == kHeight) == probe.reaches,
                      "split, reach " + Num(probe.reachPx) + " px: the right arrow's right side and bottom");
            CHECK_MSG(splitHits[1].Max().x == boxes[1].Max().x || probe.reaches, "split, reach " + Num(probe.reachPx) + " px: with no reach the right arrow's right side is its box's");
        }
        const std::array<Hud::Rect, 5> hits = Controls::HitRects(edited, boxes, kView169, kNoInsets);
        const std::string at = "reach " + Num(probe.reachPx) + " px: ";
        CHECK_MSG((hits[0].min.x == 0.0) == probe.reaches && (hits[0].min.x == boxes[0].min.x) == !probe.reaches, at + "the left arrow's left side");
        CHECK_MSG((hits[0].Max().y == kHeight) == probe.reaches, at + "the left arrow's bottom");
        CHECK_MSG((hits[3].Max().x == kView169.x) == probe.reaches && (hits[3].min.y == 0.0) == probe.reaches, at + "pause's right side and top");
        CHECK_MSG((hits[4].min.x == 0.0) == probe.reaches, at + "clear-portals' left side");
        CHECK_MSG(Near(boxes[0].min.x, 44.0 * u, 1e-9), at + "and the box itself does not move");
    }

    // The reach is measured from the safe rectangle's edge and goes to the VIEW's edge.
    const Controls::Insets notch{10.0, 20.0, 30.0, 40.0};
    const std::array<Hud::Rect, 5> boxes = Controls::Boxes(rules, kNoTuning, kView169, notch);
    const std::array<Hud::Rect, 5> hits = Controls::HitRects(rules, boxes, kView169, notch);
    CHECK_MSG(Near(boxes[0].min.x, 10.0 + 44.0 * u, 1e-9) && hits[0].min.x == 0.0, "the left arrow's box is in the notch's margin and its hit goes to the view's left edge");
    CHECK_MSG(hits[0].Max().y == kHeight && Near(boxes[0].Max().y, kHeight - 40.0 - 44.0 * u, 1e-9), "and its bottom to the view's bottom");
    CHECK_MSG(hits[3].Max().x == kView169.x && Near(boxes[3].Max().x, kView169.x - 30.0 - 44.0 * u, 1e-9), "pause's right goes to the view's right edge");
    CHECK_MSG(hits[4].min.y == 0.0 && Near(boxes[4].min.y, 20.0 + 44.0 * u, 1e-9), "clear-portals' top goes to the view's top");
    // Split: the right arrow's box is in the notch's margin from the right (30) and the bottom (40), and its hit goes to the view's right edge and bottom.
    CHECK_MSG(Near(boxes[1].Max().x, kView169.x - 30.0 - 44.0 * u, 1e-9) && Near(boxes[1].Max().y, kHeight - 40.0 - 44.0 * u, 1e-9), "the right arrow's box is in the notch's margin");
    CHECK_MSG(hits[1].Max().x == kView169.x && hits[1].Max().y == kHeight, "and its hit goes to the view's right edge and bottom");
}

void CheckNoGrowth(Controls::Arrows arrows) {
    const Controls::Rules rules = LoadModern();
    const Controls::Tuning tuning = TunedFor(arrows);
    const std::string which = std::string(ArrangementName(arrows)) + ": ";
    const std::array<Hud::Rect, 5> boxes = Controls::Boxes(rules, tuning, kView169, kNoInsets);
    const std::array<Hud::Rect, 5> hits = Controls::HitRects(rules, boxes, kView169, kNoInsets);
    // Every side of every hit rectangle is the box's own, the middle of a seam or the view's edge: nothing is a margin added.
    for (const Controls::Id id : Controls::kClaimOrder) {
        const std::size_t i = static_cast<std::size_t>(id);
        const Hud::Rect& box = boxes[i];
        const Hud::Rect& hit = hits[i];
        const glm::dvec2 bmax = box.Max();
        const glm::dvec2 hmax = hit.Max();
        const auto owns = [&](double edge, double boxEdge, double viewEdge, bool seamSide, double seamEdge) {
            return edge == boxEdge || edge == viewEdge || (seamSide && Near(edge, seamEdge, 1e-9));
        };
        const std::string name = which + Controls::KeyOf(id);
        // The seams: restart|pause at the middle of their gap and, GROUPED, the pair's; the other sides have none (split has no seam between the arrows).
        const bool grouped = arrows == Controls::Arrows::Grouped;
        const bool pairLeft = grouped && id == Controls::Id::Left;
        const bool pairRight = grouped && id == Controls::Id::Right;
        const bool restart = id == Controls::Id::Reset;
        const bool pause = id == Controls::Id::Menu;
        CHECK_MSG(owns(hit.min.x, box.min.x, 0.0, pairRight || pause, (boxes[pairRight ? 0 : 2].Max().x + box.min.x) * 0.5), name + ": the hit's left side");
        CHECK_MSG(owns(hmax.x, bmax.x, kView169.x, pairLeft || restart, (bmax.x + boxes[pairLeft ? 1 : 3].min.x) * 0.5), name + ": the hit's right side");
        CHECK_MSG(owns(hit.min.y, box.min.y, 0.0, false, 0.0), name + ": the hit's top side");
        CHECK_MSG(owns(hmax.y, bmax.y, kHeight, false, 0.0), name + ": the hit's bottom side");
    }

    // Growth is the data's, per control and per side, and only toward the world: 16 px is 5.69 units of it.
    const double grow = 16.0 * kHeight / kRefHeightPx;
    Controls::Rules edited = rules;
    edited.hit.growPx[static_cast<std::size_t>(Controls::Id::Right)] = 16.0;
    const std::array<Hud::Rect, 5> grown = Controls::HitRects(edited, boxes, kView169, kNoInsets);
    CHECK_MSG(Near(grown[1].min.y, boxes[1].min.y - grow, 1e-9), which + "16 px of growth lifts the right arrow's top by 5.69 units");
    if (arrows == Controls::Arrows::Grouped) {
        CHECK_MSG(Near(grown[1].Max().x, boxes[1].Max().x + grow, 1e-9), which + "and pushes its right side out by 5.69 units");
        // The seam and the edge still win over it: the seam stays at the middle of the gap, the bottom at the view's.
        CHECK_MSG(grown[1].min.x == hits[1].min.x && grown[1].Max().y == kHeight, which + "the right arrow's seam and bottom are what they were");
    } else {
        // Split: the right arrow's side toward the world is its LEFT one, which is pushed out by 5.69 units; its right side is the view's edge,
        // which the growth cannot pass, and its bottom stays on the view's bottom.
        CHECK_MSG(Near(grown[1].min.x, boxes[1].min.x - grow, 1e-9), which + "and pushes its left side out by 5.69 units");
        CHECK_MSG(grown[1].Max().x == kView169.x && grown[1].Max().y == kHeight, which + "its right side and bottom stay on the view's edges");
    }
    CHECK_MSG(SameBits(grown[0], hits[0]) && SameBits(grown[2], hits[2]) && SameBits(grown[3], hits[3]) && SameBits(grown[4], hits[4]),
              which + "and only the one control's");
    edited.hit.growPx[static_cast<std::size_t>(Controls::Id::Left)] = 16.0;
    const std::array<Hud::Rect, 5> bothGrown = Controls::HitRects(edited, boxes, kView169, kNoInsets);
    CHECK_MSG(Near(bothGrown[0].min.y, boxes[0].min.y - grow, 1e-9), which + "16 px on walk_left lifts its top: this is the change a mutation of the data makes");
    CHECK_MSG(!SameBits(bothGrown[0], hits[0]), which + "and it is seen");
}

void ThereIsNoGrowthTowardTheWorldByDefault() {
    for (const Controls::Arrows arrows : kArrangements) CheckNoGrowth(arrows);
}

void AHitRectNeverLeavesTheView() {
    const Hud::Rules hud = Load();
    const Controls::Rules rules = LoadModern();
    long checked = 0;
    for (const Controls::Arrows arrows : kArrangements) for (const Aspect& aspect : kAspects) {
        const glm::dvec2 view = ViewOf(aspect.ratio);
        for (int steps = 4; steps <= 14; ++steps) {
            const Controls::Tuning tuning = TunedFor(arrows, steps);
            const Controls::Layout layout = Controls::LayOut(Controls::Style::Modern, rules, hud, tuning, view, kNoInsets, 5000.0);
            for (std::size_t i = 0; i < 5; ++i) {
                const Hud::Rect& hit = layout.frames[i].hit.rect;
                const glm::dvec2 max = hit.Max();
                ++checked;
                CHECK_MSG(hit.min.x >= 0.0 && hit.min.y >= 0.0 && max.x <= view.x && max.y <= view.y && hit.size.x > 0.0 && hit.size.y > 0.0,
                          std::string(ArrangementName(arrows)) + " " + aspect.name + " size " + Num(static_cast<double>(steps) / 10.0) + " control " + std::to_string(i) + ": " + EdgesOf(hit));
                // And a hit holds its box: it is a growth of it.
                const Hud::Rect& box = layout.frames[i].rect;
                const glm::dvec2 bmax = box.Max();
                CHECK_MSG(hit.min.x <= box.min.x + 1e-9 && hit.min.y <= box.min.y + 1e-9 && max.x >= bmax.x - 1e-9 && max.y >= bmax.y - 1e-9,
                          std::string(aspect.name) + " control " + std::to_string(i) + ": the hit does not hold its box");
            }
        }
    }
    CHECK_EQ(static_cast<int>(checked), 2 * 9 * 11 * 5); // two arrangements
    // Whatever the growth: 1,000 px of it is held to the view.
    Controls::Rules edited = rules;
    for (double& grow : edited.hit.growPx) grow = 1000.0;
    for (const Controls::Arrows arrows : kArrangements) {
        const std::array<Hud::Rect, 5> boxes = Controls::Boxes(edited, TunedFor(arrows), kView169, kNoInsets);
        for (const Hud::Rect& hit : Controls::HitRects(edited, boxes, kView169, kNoInsets)) {
            const glm::dvec2 max = hit.Max();
            CHECK_MSG(hit.min.x >= 0.0 && hit.min.y >= 0.0 && max.x <= kView169.x && max.y <= kHeight,
                      std::string(ArrangementName(arrows)) + ": 1,000 px of growth is held to the view: " + EdgesOf(hit));
        }
    }
}

void TheModernHudIsNoMoreThanOneAndAHalfTimesTheClassicsAtDefaultSize() {
    const Hud::Rules hud = Load();
    const Controls::Rules rules = LoadModern();
    CHECK_MSG(Near(kClassicTotal, 9505.98, 0.01), "the classic dead zone is " + Num(kClassicTotal));
    // Per arrangement, per aspect, in total (clear shown, so all five) and for the arrows alone. GROUPED is planning step 180's (1.44 times the
    // classic, the pair 0.67); SPLIT is planning step 186's: each arrow's hit is a square of 47.50 units, so the two are 4,512.9 (0.70 of the classic
    // discs) and the whole HUD 13,883.6, 1.46 times the classic and under the budget of 1.5 at every aspect (the areas do not depend on the width).
    struct Budget {
        Controls::Arrows arrows;
        double total;     // u^2
        double times;     // of the classic total, to two decimals
        double pair;      // the two arrows' hit areas together, u^2
        double pairTimes; // of the classic discs', to two decimals
        std::array<double, 3> percent; // of the view at 16:9, 4:3 and 5:4
        double balancedTotal;
        double balancedPairShare;
        double mrTotal;
        double mrPairShare;
    };
    const std::array<Budget, 2> budgets = {{{Controls::Arrows::Split, 13883.6, 1.46, 4512.9, 0.70, {11.92, 15.89, 16.95}, 14419.6, 0.785, 15415.6, 0.94},
                                            {Controls::Arrows::Grouped, 13680.9, 1.44, 4310.2, 0.67, {11.7, 15.7, 16.7}, 14212.7, 0.75, 15204.7, 0.91}}};
    const std::array<std::pair<const char*, double>, 3> shapes = {{{"16:9", 16.0 / 9.0}, {"4:3", 4.0 / 3.0}, {"5:4", 1.25}}};
    for (const Budget& budget : budgets) {
        const Controls::Tuning tuning = TunedFor(budget.arrows);
        std::size_t at = 0;
        for (const auto& [name, ratio] : shapes) {
            const glm::dvec2 view = ViewOf(ratio);
            const Controls::Layout layout = Controls::LayOut(Controls::Style::Modern, rules, hud, tuning, view, kNoInsets, 5000.0);
            double total = 0.0;
            for (const Controls::Frame& frame : layout.frames) total += AreaOf(frame.hit.rect);
            const double pair = AreaOf(layout.At(Controls::Id::Left).hit.rect) + AreaOf(layout.At(Controls::Id::Right).hit.rect);
            const double ratioTotal = total / kClassicTotal;
            const std::string where = std::string(ArrangementName(budget.arrows)) + " " + name + ": ";
            CHECK_MSG(ratioTotal <= 1.5, where + "the modern HUD is " + Num(ratioTotal) + " times the classic's, over the budget of 1.5");
            CHECK_MSG(pair / kClassicPair <= 1.0, where + "the arrows alone are " + Num(pair / kClassicPair) + " times the classic discs, over 1.0");
            CHECK_MSG(Near(total, budget.total, 0.1) && Near(ratioTotal, budget.times, 0.005), where + "the total is " + Num(total) + ", " + Num(ratioTotal) + " times");
            CHECK_MSG(Near(pair, budget.pair, 0.1) && Near(pair / kClassicPair, budget.pairTimes, 0.005), where + "the arrows are " + Num(pair));
            CHECK_MSG(Near(100.0 * total / (view.x * view.y), budget.percent[at], 0.05), where + Num(100.0 * total / (view.x * view.y)) + " percent of the view");
            ++at;
        }
        // The two other looks, pinned as the numbers the owner decides on (Appendix A); only the default is held to the budget.
        struct Look {
            Controls::Preset preset;
            double total;
            double pairShare;
        };
        for (const Look& look : {Look{Controls::Preset::Balanced, budget.balancedTotal, budget.balancedPairShare}, Look{Controls::Preset::Mr, budget.mrTotal, budget.mrPairShare}}) {
            const Controls::Tuning tuned = TunedFor(budget.arrows, std::nullopt, look.preset);
            const Controls::Layout layout = Controls::LayOut(Controls::Style::Modern, rules, hud, tuned, kView169, kNoInsets, 5000.0);
            double total = 0.0;
            for (const Controls::Frame& frame : layout.frames) total += AreaOf(frame.hit.rect);
            const double pair = AreaOf(layout.At(Controls::Id::Left).hit.rect) + AreaOf(layout.At(Controls::Id::Right).hit.rect);
            CHECK_MSG(Near(total, look.total, 0.1) && Near(pair / kClassicPair, look.pairShare, 0.005),
                      std::string(ArrangementName(budget.arrows)) + " " + Controls::NameOf(look.preset) + ": total " + Num(total) + ", the arrows " + Num(pair / kClassicPair));
        }
    }
    // The classic style's own areas, from the same function: 3,217 a disc quarter and 1,024 a button (a literal check of the baseline).
    const Controls::Layout classic = Controls::LayOut(Controls::Style::Classic, rules, hud, kNoTuning, kView169, kNoInsets, 5000.0);
    CHECK(classic.style == Controls::Style::Classic);
    CHECK_MSG(AreaOf(classic.At(Controls::Id::Reset).hit.rect) == 1024.0 && AreaOf(classic.At(Controls::Id::Menu).hit.rect) == 1024.0 &&
                  AreaOf(classic.At(Controls::Id::Clear).hit.rect) == 1024.0,
              "a classic corner button's hit is its 32 x 32");
}

// ---- the slide and the entrances -----------------------------------------------------------------------------

void TheSlideMovesEachArrowFromItsNearerEdgeAndTheHitFollows() {
    const Hud::Rules hud = Load();
    const Controls::Rules rules = LoadModern();
    for (const Controls::Arrows arrows : kArrangements) {
        const Controls::Tuning tuning = TunedFor(arrows);
        const std::string which = std::string(ArrangementName(arrows)) + ": ";
        const Controls::Layout rest = Controls::LayOut(Controls::Style::Modern, rules, hud, tuning, kView169, kNoInsets, 5000.0);
        // The classic pads' own slide: 128 units out at the start, in by 700 ms, smoothEnd between (ui.json).
        const std::array<std::pair<double, double>, 5> ages = {{{0.0, 128.0}, {350.0, 128.0 * (1.0 - std::sqrt(0.5))}, {699.0, -1.0}, {700.0, 0.0}, {701.0, 0.0}}};
        for (const auto& [age, slide] : ages) {
            const Controls::Layout laid = Controls::LayOut(Controls::Style::Modern, rules, hud, tuning, kView169, kNoInsets, age);
            const double want = slide < 0.0 ? Hud::PadSlideUnits(hud, age) : slide;
            for (const Controls::Id id : kWalks) {
                const std::string at = which + Controls::KeyOf(id) + " at " + Num(age) + " ms: ";
                const Controls::Frame& frame = laid.At(id);
                const Controls::Frame& settled = rest.At(id);
                // Each arrow comes in from the NEARER of the view's side edges, judged where it rests: grouped, both rest in the left half and come
                // in from the left; split, walk_right rests in the right half and comes in from the right. The box and the hit move the same way.
                const double toward = settled.rect.Centre().x > kView169.x * 0.5 ? -1.0 : 1.0; // settled - frame: + from the left, - from the right
                CHECK_MSG(Near(settled.rect.min.x - frame.rect.min.x, toward * want, 1e-9) && frame.rect.min.y == settled.rect.min.y, at + "the box is " + Num(want) + " out");
                CHECK_MSG(Near(settled.hit.rect.min.x - frame.hit.rect.min.x, toward * want, 1e-9) && frame.hit.rect.min.y == settled.hit.rect.min.y, at + "and the hit goes with it");
                CHECK_MSG(frame.rect.size == settled.rect.size && frame.hit.rect.size == settled.hit.rect.size, at + "neither changes size");
                if (want == 0.0) CHECK_MSG(SameBits(frame.rect, settled.rect) && SameBits(frame.hit, settled.hit), at + "once in, the frame is the settled one, bit for bit");
            }
            // The corner buttons do not slide: their entrance is a Placement (the next case).
            for (const Controls::Id id : kCorners) {
                CHECK_MSG(SameBits(laid.At(id).rect, rest.At(id).rect) && SameBits(laid.At(id).hit, rest.At(id).hit), which + Controls::KeyOf(id) + " at " + Num(age) + " ms is not slid");
            }
        }
        CHECK_MSG(Controls::ClassicSlideUnits(hud, 0.0) == 128.0 && Controls::ClassicSlideUnits(hud, 700.0) == 0.0, "the slide is the classic's: 128 units over 700 ms");
        // At the very start every arrow is wholly off the screen at the side it comes from (as the classic pads are), so nothing in the view is claimed.
        const Controls::Layout start = Controls::LayOut(Controls::Style::Modern, rules, hud, tuning, kView169, kNoInsets, 0.0);
        CHECK(start.At(Controls::Id::Left).rect.Max().x < 0.0);
        if (arrows == Controls::Arrows::Grouped) {
            CHECK(start.At(Controls::Id::Right).rect.Max().x < 0.0);
        } else {
            CHECK_MSG(start.At(Controls::Id::Right).rect.min.x > kView169.x, which + "walk_right starts wholly off the view's right edge");
        }
        for (const double x : {10.0, kView169.x - 10.0, 80.0, kView169.x - 80.0}) {
            CHECK(!start.At(Controls::Id::Left).hit.Contains(glm::dvec2(x, 230.0)) && !start.At(Controls::Id::Right).hit.Contains(glm::dvec2(x, 230.0)));
        }
        CHECK(rest.At(Controls::Id::Left).hit.Contains(glm::dvec2(10.0, 230.0)));
        if (arrows == Controls::Arrows::Split) CHECK(rest.At(Controls::Id::Right).hit.Contains(glm::dvec2(kView169.x - 10.0, 230.0)));

        // An arrow moved into the other half comes in from the other edge: the nearer edge is judged where it rests. (Grouped: walk_left moved right;
        // split: walk_right moved left, 300 units, which fits.)
        const Controls::Id mover = arrows == Controls::Arrows::Grouped ? Controls::Id::Left : Controls::Id::Right;
        const Controls::Id other = arrows == Controls::Arrows::Grouped ? Controls::Id::Right : Controls::Id::Left;
        Controls::Tuning moved = tuning;
        moved.move[static_cast<std::size_t>(mover)] = glm::dvec2(arrows == Controls::Arrows::Grouped ? 300.0 : -300.0, 0.0);
        const Controls::Layout settled = Controls::LayOut(Controls::Style::Modern, rules, hud, moved, kView169, kNoInsets, 5000.0);
        const Controls::Layout early = Controls::LayOut(Controls::Style::Modern, rules, hud, moved, kView169, kNoInsets, 0.0);
        const bool movedToTheRight = arrows == Controls::Arrows::Grouped;
        CHECK_MSG((settled.At(mover).rect.Centre().x > kView169.x * 0.5) == movedToTheRight, which + "the moved arrow rests in the " + (movedToTheRight ? "right" : "left") + " half");
        CHECK_MSG(Near(early.At(mover).rect.min.x - settled.At(mover).rect.min.x, movedToTheRight ? 128.0 : -128.0, 1e-9), which + "and comes in from the " + (movedToTheRight ? "right" : "left"));
        CHECK_MSG(Near(early.At(mover).hit.rect.min.x - settled.At(mover).hit.rect.min.x, movedToTheRight ? 128.0 : -128.0, 1e-9), which + "its hit with it");
        // The other arrow comes from its own side: grouped, from the left (-128); split, walk_left from the left (-128) as before.
        CHECK_MSG(Near(early.At(other).rect.min.x - settled.At(other).rect.min.x, -128.0, 1e-9), which + "while the other arrow still comes from the left");
    }
}

void TheCornerPlacementsGiveTheOriginalsEntranceAtTheInsetCorners() {
    const Hud::Rules hud = Load();
    const Controls::Rules rules = LoadModern();
    LevelEnd::Rules end;
    std::string error;
    CHECK_MSG(LevelEnd::LoadRules(kUi, end, error), "ui.json's level_end reads: " + error);
    const std::array<Hud::Rect, 5> boxes = Controls::Boxes(rules, kNoTuning, kView169, kNoInsets);

    struct Corner {
        Controls::Id id;
        Hud::Anchor anchor;
        glm::dvec2 inset;
    };
    // The insets from the anchor's two edges (the design's 1.2): 15.6444 from the top, and from the right 15.6444 for pause and
    // 15.6444 + 40.96 + 11.3778 = 67.9822 for restart; clear-portals 15.6444 from both of the top-left's.
    const std::array<Corner, 3> corners = {{{Controls::Id::Reset, Hud::Anchor::TopRight, {67.9822, 15.6444}},
                                            {Controls::Id::Menu, Hud::Anchor::TopRight, {15.6444, 15.6444}},
                                            {Controls::Id::Clear, Hud::Anchor::TopLeft, {15.6444, 15.6444}}}};
    for (const Corner& corner : corners) {
        const Hud::Placement placement = Controls::PlacementFor(rules, hud, kNoTuning, corner.id, kView169, kNoInsets);
        const std::string name = Controls::KeyOf(corner.id);
        CHECK_MSG(placement.anchor == corner.anchor, name + ": its anchor");
        CHECK_MSG(Near(placement.insetUnits.x, corner.inset.x, kTable) && Near(placement.insetUnits.y, corner.inset.y, kTable), name + ": its inset (" + Num(placement.insetUnits.x) + ", " + Num(placement.insetUnits.y) + ")");
        CHECK_MSG(Near(placement.sizeUnits.x, 40.96, 1e-9) && Near(placement.sizeUnits.y, 40.96, 1e-9), name + ": its size 40.96");
        CHECK_MSG(placement.sprite == Controls::ClassicPlacement(hud, corner.id).sprite && !placement.sprite.empty(), name + ": its sprite is the classic placement's");
        // A button at rest and the end of its entrance are one rectangle, bit for bit.
        const Hud::Rect home = Hud::Place(placement, kView169);
        CHECK_MSG(SameBits(home, boxes[static_cast<std::size_t>(corner.id)]), name + ": the box is the placement's own rectangle");

        // The entrance, LevelEnd::HudEntered on it unchanged: along the ray from the view's centre through the anchor, 32 units out,
        // in over 700 ms with the alpha rising linearly to the classic byte.
        const glm::dvec2 anchor = LevelEnd::HudAnchor(placement, kView169);
        const glm::dvec2 ray = anchor - kView169 * 0.5;
        const glm::dvec2 out = ray / glm::length(ray) * 32.0;
        const LevelEnd::Dismissed first = LevelEnd::HudEntered(end, placement, hud.alphaByte, kView169, 0.0);
        CHECK_MSG(Near(first.rect.min.x - home.min.x, out.x, 1e-9) && Near(first.rect.min.y - home.min.y, out.y, 1e-9), name + ": at 0 ms it is 32 units out along the ray");
        CHECK_MSG(first.shown && first.alpha == 0.0, name + ": and invisible");
        const LevelEnd::Dismissed half = LevelEnd::HudEntered(end, placement, hud.alphaByte, kView169, 350.0);
        CHECK_MSG(Near(half.alpha, 0.5 * 120.0 / 255.0, 1.0 / 255.0), name + ": half in, at half its alpha " + Num(half.alpha));
        const LevelEnd::Dismissed done = LevelEnd::HudEntered(end, placement, hud.alphaByte, kView169, 701.0);
        CHECK_MSG(SameBits(done.rect, home) && Near(done.alpha, 120.0 / 255.0, 1e-12), name + ": in, it is home at the classic alpha");
        // The dismissal at a death: the same ray, and gone.
        const LevelEnd::Dismissed leaving = LevelEnd::HudDismissed(end, placement, hud.alphaByte, kView169, 0.0);
        CHECK_MSG(leaving.shown && SameBits(leaving.rect, home), name + ": the dismissal starts at home");
        CHECK_MSG(!LevelEnd::HudDismissed(end, placement, hud.alphaByte, kView169, 100000.0).shown, name + ": and ends");
    }
    CHECK_MSG(Controls::PlacementFor(rules, hud, kNoTuning, Controls::Id::Left, kView169, kNoInsets).sprite.empty() &&
                  Controls::PlacementFor(rules, hud, kNoTuning, Controls::Id::Right, kView169, kNoInsets).sizeUnits == glm::dvec2(0.0),
              "a pad has no placement");

    // A moved or inset button is still a placement: the box through the placement is the box asked for.
    Controls::Tuning moved;
    moved.move[static_cast<std::size_t>(Controls::Id::Menu)] = glm::dvec2(-20.0, 30.0);
    const Controls::Insets notch{0.0, 8.0, 12.0, 0.0};
    const Hud::Rect shifted = Controls::Boxes(rules, moved, kView169, notch)[3];
    const Hud::Placement placement = Controls::PlacementFor(rules, hud, moved, Controls::Id::Menu, kView169, notch);
    CHECK_MSG(SameBits(Hud::Place(placement, kView169), shifted), "a moved, inset pause button is the placement's rectangle");
    CHECK_MSG(Near(kView169.x - shifted.Max().x, 12.0 + 44.0 * kHeight / kRefHeightPx + 20.0, 1e-9) && Near(shifted.min.y, 8.0 + 44.0 * kHeight / kRefHeightPx + 30.0, 1e-9),
              "from the right 12 + 15.64 + 20 and from the top 8 + 15.64 + 30");
}

// ---- the tuning's arithmetic ----------------------------------------------------------------------------------

void DefaultTuningIsTheBaseBitForBit() {
    const Hud::Rules hud = Load();
    const Controls::Rules rules = LoadModern();
    Controls::Tuning explicitDefault; // the look's own size and opacity written out, and no move
    explicitDefault.sizeSteps = 7;
    explicitDefault.opacitySteps = 5;
    CHECK(kNoTuning.IsDefault(rules) && explicitDefault.IsDefault(rules));
    CHECK_MSG(!explicitDefault.Clamped(rules).sizeSteps && !explicitDefault.Clamped(rules).opacitySteps, "a size and an opacity equal to the default are stored as unset");
    CHECK(explicitDefault.Size(rules) == kNoTuning.Size(rules) && explicitDefault.Opacity(rules) == kNoTuning.Opacity(rules));
    // The arrangement is a capture's, not a player's: it changes where a default control stands but it is not a change a player made, so a tuning that
    // only asks for one is still the default (IsDefault) and writes nothing (ToJson).
    CHECK_MSG(kGrouped.IsDefault(rules) && kGrouped.ToJson(rules) == "{}" && kGrouped.ToJsonMembers(rules).empty(), "`arrows=grouped` is not saved");
    CHECK_MSG(kGrouped.Clamped(rules).arrows == Controls::Arrows::Grouped, "and clamping the tuning keeps it");

    for (const Controls::Arrows arrows : kArrangements) {
        Controls::Tuning base = TunedFor(arrows);
        Controls::Tuning written = explicitDefault;
        written.arrows = arrows;
        long differ = 0;
        for (const glm::dvec2& view : {kView169, kView43, kView21, ViewOf(1.25), ViewOf(32.0 / 9.0)}) {
            for (const double age : kAges) {
                const Controls::Layout a = Controls::LayOut(Controls::Style::Modern, rules, hud, base, view, kNoInsets, age);
                const Controls::Layout b = Controls::LayOut(Controls::Style::Modern, rules, hud, written, view, kNoInsets, age);
                const Controls::Layout c = Controls::LayOut(Controls::Style::Modern, rules, hud, written.Clamped(rules), view, kNoInsets, age);
                for (std::size_t i = 0; i < 5; ++i) {
                    if (!SameBits(a.frames[i].rect, b.frames[i].rect) || !SameBits(a.frames[i].hit, b.frames[i].hit)) ++differ;
                    if (!SameBits(a.frames[i].rect, c.frames[i].rect) || !SameBits(a.frames[i].hit, c.frames[i].hit)) ++differ;
                }
            }
        }
        CHECK_MSG(differ == 0, std::string(ArrangementName(arrows)) + ": a default tuning written out differs from none in " + std::to_string(differ) + " frames");
    }

    // The layout against the design's own formula, in its own order of operations, compared by the doubles' bits: the
    // tuning adds nothing (a size n / 10, a move of 0 added, a clamp that does not move a box inside the view).
    for (const Controls::Arrows arrows : kArrangements) for (const glm::dvec2& view : {kView169, kView43, kView21}) {
        const auto px = [&view](double n) { return n * view.y / 720.0; };
        const double side = px(44.0);
        const double top = px(44.0);
        const double bottom = px(44.0);
        const double gap = px(32.0);
        const double box = px(128.0);
        const double arrow = box * 0.7;
        const double corner = box * 0.9 * std::max(0.7, 1.0);
        const std::array<Hud::Rect, 5> boxes = Controls::Boxes(rules, TunedFor(arrows), view, kNoInsets);
        const std::string where = std::string(ArrangementName(arrows)) + " " + ShapeOf(view) + ": ";
        const Hud::Rect left{glm::dvec2(0.0 + side, (view.y - 0.0) - bottom - arrow), glm::dvec2(arrow)};
        const Hud::Rect right = arrows == Controls::Arrows::Grouped
                                    ? Hud::Rect{glm::dvec2(0.0 + side + arrow + gap, (view.y - 0.0) - bottom - arrow), glm::dvec2(arrow)}
                                    : Hud::Rect{glm::dvec2((view.x - 0.0) - side - arrow, (view.y - 0.0) - bottom - arrow), glm::dvec2(arrow)};
        CHECK_MSG(SameBits(boxes[0], left), where + "the left arrow's box, bit for bit: " + EdgesOf(boxes[0]));
        CHECK_MSG(SameBits(boxes[1], right), where + "the right arrow's box, bit for bit: " + EdgesOf(boxes[1]));
        // A corner button at rest is its Placement's rectangle (the entrance's end), measured from the view's corner.
        const auto place = [&view, corner](Hud::Anchor anchor, const glm::dvec2& min) {
            Hud::Placement p;
            p.anchor = anchor;
            p.sizeUnits = glm::dvec2(corner);
            p.insetUnits = anchor == Hud::Anchor::TopLeft ? min : glm::dvec2(view.x - (min.x + corner), min.y);
            return Hud::Place(p, view);
        };
        const double pauseX = (view.x - 0.0) - side - corner;
        const double restartX = (view.x - 0.0) - side - corner - gap - corner;
        CHECK_MSG(SameBits(boxes[3], place(Hud::Anchor::TopRight, glm::dvec2(pauseX, 0.0 + top))), where + "pause, bit for bit");
        CHECK_MSG(SameBits(boxes[2], place(Hud::Anchor::TopRight, glm::dvec2(restartX, 0.0 + top))), where + "restart, bit for bit");
        CHECK_MSG(SameBits(boxes[4], place(Hud::Anchor::TopLeft, glm::dvec2(0.0 + side, 0.0 + top))), where + "clear-portals, bit for bit");
    }
    // And the hit rectangles from the same formula to a billionth, whatever the aspect: the seam, the reach, nothing else.
    for (const Controls::Arrows arrows : kArrangements) for (const Aspect& aspect : kAspects) {
        const glm::dvec2 view = ViewOf(aspect.ratio);
        const std::array<Hud::Rect, 5> boxes = Controls::Boxes(rules, TunedFor(arrows), view, kNoInsets);
        const std::array<Hud::Rect, 5> hits = Controls::HitRects(rules, boxes, view, kNoInsets);
        const double seamArrows = (boxes[0].Max().x + boxes[1].min.x) * 0.5;
        const double seamButtons = (boxes[2].Max().x + boxes[3].min.x) * 0.5;
        const std::string where = std::string(ArrangementName(arrows)) + " " + aspect.name + ": ";
        if (arrows == Controls::Arrows::Grouped) {
            CHECK_MSG(RectIs(hits[0], {0.0, boxes[0].min.y, seamArrows, view.y}, 1e-9), where + "the left arrow's hit " + EdgesOf(hits[0]));
            CHECK_MSG(RectIs(hits[1], {seamArrows, boxes[1].min.y, boxes[1].Max().x, view.y}, 1e-9), where + "the right arrow's hit " + EdgesOf(hits[1]));
        } else {
            // Split: no seam; each arrow is its box plus the strips to its own two view edges.
            CHECK_MSG(RectIs(hits[0], {0.0, boxes[0].min.y, boxes[0].Max().x, view.y}, 1e-9), where + "the left arrow's hit " + EdgesOf(hits[0]));
            CHECK_MSG(RectIs(hits[1], {boxes[1].min.x, boxes[1].min.y, view.x, view.y}, 1e-9), where + "the right arrow's hit " + EdgesOf(hits[1]));
        }
        CHECK_MSG(RectIs(hits[2], {boxes[2].min.x, 0.0, seamButtons, boxes[2].Max().y}, 1e-9), where + "restart's hit " + EdgesOf(hits[2]));
        CHECK_MSG(RectIs(hits[3], {seamButtons, 0.0, view.x, boxes[3].Max().y}, 1e-9), where + "pause's hit " + EdgesOf(hits[3]));
        CHECK_MSG(RectIs(hits[4], {0.0, 0.0, boxes[4].Max().x, boxes[4].Max().y}, 1e-9), where + "clear-portals' hit " + EdgesOf(hits[4]));
    }
}

void AMoveIsInUnitsFromTheAnchorAndNotScaledBySize() {
    const Controls::Rules rules = LoadModern();
    for (const int steps : {4, 7, 10, 14}) {
        Controls::Tuning sized;
        sized.sizeSteps = steps;
        const std::array<Hud::Rect, 5> home = Controls::Boxes(rules, sized, kView169, kNoInsets);
        Controls::Tuning moved = sized;
        moved.move[static_cast<std::size_t>(Controls::Id::Left)] = glm::dvec2(10.5, -7.3);
        moved.move[static_cast<std::size_t>(Controls::Id::Menu)] = glm::dvec2(-20.0, 30.0);
        moved.move[static_cast<std::size_t>(Controls::Id::Clear)] = glm::dvec2(0.1, 0.0);
        const std::array<Hud::Rect, 5> there = Controls::Boxes(rules, moved, kView169, kNoInsets);
        const std::string at = "size " + Num(static_cast<double>(steps) / 10.0) + ": ";
        // The move is the same number of units at every size: it is from the control's place, not a fraction of its box.
        CHECK_MSG(Near(there[0].min.x - home[0].min.x, 10.5, 1e-9) && Near(there[0].min.y - home[0].min.y, -7.3, 1e-9), at + "the left arrow is 10.5, -7.3 from its place");
        CHECK_MSG(Near(there[3].min.x - home[3].min.x, -20.0, 1e-9) && Near(there[3].min.y - home[3].min.y, 30.0, 1e-9), at + "pause is -20, 30 from its place");
        CHECK_MSG(Near(there[4].min.x - home[4].min.x, 0.1, 1e-9) && there[4].min.y == home[4].min.y, at + "clear-portals is 0.1 from its place");
        CHECK_MSG(SameBits(there[1], home[1]) && SameBits(there[2], home[2]), at + "the others are where they were");
        CHECK_MSG(there[0].size == home[0].size && there[3].size == home[3].size, at + "a move does not change a box");
    }
    // Tuned and clamped, a move is whole tenths: 10.54 is 10.5 and -3.06 is -3.1, and NaN and infinity are no move.
    Controls::Tuning loose;
    loose.move[0] = glm::dvec2(10.54, -3.06);
    loose.move[1] = glm::dvec2(std::nan(""), std::numeric_limits<double>::infinity());
    loose.move[2] = glm::dvec2(5000.0, -5000.0);
    loose.move[3] = glm::dvec2(-0.04, 0.04);
    const Controls::Tuning clean = loose.Clamped(rules);
    CHECK(clean.move[0].x == 10.5 && clean.move[0].y == -3.1);
    CHECK(clean.move[1].x == 0.0 && clean.move[1].y == 0.0);
    CHECK_MSG(clean.move[2].x == 1024.0 && clean.move[2].y == -1024.0, "a move is held to 1,024 units");
    CHECK(clean.move[3].x == 0.0 && clean.move[3].y == 0.0 && !std::signbit(clean.move[3].x));
    CHECK_MSG(!loose.IsDefault(rules) && kNoTuning.IsDefault(rules), "a tuning with a move is not the default");
    Controls::Tuning almost;
    almost.move[0] = glm::dvec2(0.04, -0.04);
    CHECK_MSG(almost.IsDefault(rules), "a move that snaps to nothing is no move");
}

// A deterministic stream: the standard distributions are not portable, the engine's raw output is.
struct Rng {
    std::mt19937_64 engine;
    explicit Rng(std::uint64_t seed) : engine(seed) {}
    std::uint64_t Below(std::uint64_t n) { return engine() % n; }
    int Range(int lo, int hi) { return lo + static_cast<int>(Below(static_cast<std::uint64_t>(hi - lo + 1))); }
};

void MoveForIsTheInverseOfPlace() {
    const Controls::Rules rules = LoadModern();
    Rng rng(1015);
    const std::array<double, 4> ratios = {4.0 / 3.0, 16.0 / 9.0, 2.0, 21.0 / 9.0};
    long exact = 0;
    long clamped = 0;
    long fixedPoint = 0;
    for (int i = 0; i < 10000; ++i) {
        const Controls::Id id = Controls::kClaimOrder[rng.Below(5)];
        const std::size_t at = static_cast<std::size_t>(id);
        const glm::dvec2 view = ViewOf(ratios[rng.Below(4)]);
        Controls::Insets insets;
        if (rng.Below(4) == 0) insets = Controls::Insets{static_cast<double>(rng.Below(25)), static_cast<double>(rng.Below(25)), static_cast<double>(rng.Below(25)), static_cast<double>(rng.Below(25))};
        Controls::Tuning tuning;
        tuning.sizeSteps = rng.Range(4, 14);
        // A move on the tenth lattice, up to 300 units either way: some fit the safe rectangle and some are pulled in.
        const glm::dvec2 move(static_cast<double>(rng.Range(-3000, 3000)) / 10.0, static_cast<double>(rng.Range(-3000, 3000)) / 10.0);
        tuning.move[at] = move;

        const Hud::Rect box = Controls::Boxes(rules, tuning, view, insets)[at];
        Controls::Tuning home = tuning;
        home.move[at] = glm::dvec2(0.0);
        const glm::dvec2 wanted = Controls::Boxes(rules, home, view, insets)[at].min + move;
        const bool fits = Near(box.min.x, wanted.x, 1e-9) && Near(box.min.y, wanted.y, 1e-9);

        // The move asked back for the box where it stands is the move that put it there, to the digit, where it fitted;
        // and where it was pulled in, it is the move of the box that stands, which stands again where it stands.
        const glm::dvec2 got = Controls::MoveFor(rules, tuning, id, box.min, view, insets);
        if (fits) {
            ++exact;
            if (got.x != move.x || got.y != move.y) CHECK_MSG(false, "a move of " + At(move) + " came back as " + At(got));
        } else {
            ++clamped;
        }
        // Placed by its own move the box stands where it stood, to the snap of a move: a box that was pulled in against the safe
        // rectangle's edge has a move that is a whole tenth, which lands it up to half a tenth either side of the edge, inside
        // (a box is never put outside) - and asked again, the same move comes back, so the second round trip is exact.
        Controls::Tuning again = tuning;
        again.move[at] = got;
        const Hud::Rect box2 = Controls::Boxes(rules, again, view, insets)[at];
        const glm::dvec2 again2 = Controls::MoveFor(rules, again, id, box2.min, view, insets);
        const double half = Controls::Tuning::kMoveSnap / 2.0 + 1e-9;
        if (Near(box2.min.x, box.min.x, half) && Near(box2.min.y, box.min.y, half) && again2.x == got.x && again2.y == got.y) {
            ++fixedPoint;
        } else {
            CHECK_MSG(false, "the box at " + At(box.min) + " moved to " + At(box2.min) + " when placed by its own move " + At(got) + ", which asked again is " + At(again2));
        }
    }
    CHECK_MSG(exact > 1500 && clamped > 1500, "the round trips met " + std::to_string(exact) + " placements that fitted and " + std::to_string(clamped) + " that were pulled in");
    CHECK_MSG(exact + clamped == 10000 && fixedPoint == 10000, "10,000 round trips, each stable: " + std::to_string(fixedPoint));

    // A wish off the lattice, or off the view, comes back as a placement: tenths, and a box that is inside.
    long offLattice = 0;
    for (int i = 0; i < 3000; ++i) {
        const Controls::Id id = Controls::kClaimOrder[rng.Below(5)];
        const std::size_t at = static_cast<std::size_t>(id);
        const glm::dvec2 view = ViewOf(ratios[rng.Below(4)]);
        const Controls::Insets insets{};
        Controls::Tuning tuning;
        tuning.sizeSteps = rng.Range(4, 14);
        const glm::dvec2 wish(static_cast<double>(rng.Range(-20000, 80000)) / 100.0, static_cast<double>(rng.Range(-20000, 60000)) / 100.0);
        const glm::dvec2 got = Controls::MoveFor(rules, tuning, id, wish, view, insets);
        const bool tenths = Near(got.x * 10.0, std::round(got.x * 10.0), 1e-9) && Near(got.y * 10.0, std::round(got.y * 10.0), 1e-9);
        Controls::Tuning placed = tuning;
        placed.move[at] = got;
        const Hud::Rect box = Controls::Boxes(rules, placed, view, insets)[at];
        const glm::dvec2 max = box.Max();
        const bool inside = box.min.x >= -1e-9 && box.min.y >= -1e-9 && max.x <= view.x + 1e-9 && max.y <= view.y + 1e-9;
        // It is where the clamped wish is, to the snap of a tenth either way.
        const glm::dvec2 clampedWish = Controls::ClampBox(wish, box.size, view, insets);
        const bool near = Near(box.min.x, clampedWish.x, 0.05 + 1e-9) && Near(box.min.y, clampedWish.y, 0.05 + 1e-9);
        if (!tenths || !inside || !near) ++offLattice;
    }
    CHECK_MSG(offLattice == 0, std::to_string(offLattice) + " of 3,000 off-lattice wishes were not tenths, inside, and near the clamped wish");
}

void ClampKeepsEveryBoxWhollyInsideTheSafeRectangle() {
    const Hud::Rules hud = Load();
    const Controls::Rules rules = LoadModern();
    Rng rng(20261015);
    long outside = 0;
    long pulled = 0;
    for (int i = 0; i < 3000; ++i) {
        const glm::dvec2 view = ViewOf(kAspects[rng.Below(kAspects.size())].ratio);
        const Controls::Insets insets{static_cast<double>(rng.Below(30)), static_cast<double>(rng.Below(30)), static_cast<double>(rng.Below(30)), static_cast<double>(rng.Below(30))};
        Controls::Tuning tuning;
        tuning.sizeSteps = rng.Range(4, 14);
        for (glm::dvec2& m : tuning.move) m = glm::dvec2(static_cast<double>(rng.Range(-10240, 10240)) / 10.0, static_cast<double>(rng.Range(-10240, 10240)) / 10.0);
        const Controls::Layout layout = Controls::LayOut(Controls::Style::Modern, rules, hud, tuning, view, insets, 5000.0);
        for (const Controls::Frame& frame : layout.frames) {
            const glm::dvec2 max = frame.rect.Max();
            if (frame.rect.min.x < insets.left - 1e-9 || frame.rect.min.y < insets.top - 1e-9 || max.x > view.x - insets.right + 1e-9 ||
                max.y > view.y - insets.bottom + 1e-9) {
                ++outside;
            }
        }
        const std::array<Hud::Rect, 5> unmoved = Controls::Boxes(rules, Controls::Tuning{}, view, insets);
        for (std::size_t c = 0; c < 5; ++c) {
            if (!Near(layout.frames[c].rect.min.x, unmoved[c].min.x + tuning.Clamped(rules).move[c].x, 1e-9)) ++pulled;
        }
    }
    CHECK_MSG(outside == 0, std::to_string(outside) + " boxes were laid out outside the safe rectangle");
    CHECK_MSG(pulled > 1000, "the sweep pulled " + std::to_string(pulled) + " boxes in");

    // A saved move that no longer fits is pulled in when it is laid out and left as stored.
    Controls::Tuning far;
    far.move[0] = glm::dvec2(1000.0, 0.0);
    CHECK(far.Clamped(rules).move[0].x == 1000.0);
    CHECK(Controls::Boxes(rules, far, kView169, kNoInsets)[0].Max().x <= kView169.x + 1e-9);
    CHECK(Controls::Boxes(rules, far, kView43, kNoInsets)[0].Max().x <= kView43.x + 1e-9);

    // The shared clamp itself: a box inside is not moved by a bit, one outside is pulled in the least, and one that is
    // larger than the safe rectangle sits against its top-left.
    const glm::dvec2 inside(100.123456789, 50.987654321);
    const glm::dvec2 same = Controls::ClampBox(inside, glm::dvec2(30.0), kView169, kNoInsets);
    CHECK(same.x == inside.x && same.y == inside.y);
    const glm::dvec2 pulledIn = Controls::ClampBox(glm::dvec2(-5.0, 1000.0), glm::dvec2(30.0), kView169, Controls::Insets{10.0, 20.0, 30.0, 40.0});
    CHECK_MSG(pulledIn.x == 10.0 && pulledIn.y == kHeight - 40.0 - 30.0, "pulled to the safe rectangle's left and bottom: " + At(pulledIn));
    const glm::dvec2 huge = Controls::ClampBox(glm::dvec2(300.0, 300.0), glm::dvec2(9999.0), kView169, Controls::Insets{10.0, 20.0, 30.0, 40.0});
    CHECK_MSG(huge.x == 10.0 && huge.y == 20.0, "a box larger than the safe rectangle sits at its top-left: " + At(huge));
}

void SafeInsetsMoveTheDefaultsInward() {
    const Hud::Rules hud = Load();
    const Controls::Rules rules = LoadModern();
    const double m = 44.0 * kHeight / kRefHeightPx;
    const Controls::Insets notch{10.0, 20.0, 30.0, 40.0};
    const std::array<Hud::Rect, 5> flat = Controls::Boxes(rules, kNoTuning, kView169, kNoInsets);
    const std::array<Hud::Rect, 5> inset = Controls::Boxes(rules, kNoTuning, kView169, notch);
    // The left arrow moves in from the left and the bottom, restart and pause from the right and the top, clear-portals from the left and the top.
    // SPLIT (the default): the right arrow moves in from the RIGHT and the bottom; GROUPED, it moves in from the left with the left arrow.
    CHECK_MSG(Near(inset[0].min.x - flat[0].min.x, 10.0, 1e-9) && Near(inset[0].min.y - flat[0].min.y, -40.0, 1e-9), "the left arrow is 10 in from the left and 40 up from the bottom");
    CHECK_MSG(Near(inset[1].min.x - flat[1].min.x, -30.0, 1e-9) && Near(inset[1].min.y - flat[1].min.y, -40.0, 1e-9), "and the split right arrow is 30 in from the right and 40 up from the bottom");
    {
        const std::array<Hud::Rect, 5> groupedFlat = Controls::Boxes(rules, kGrouped, kView169, kNoInsets);
        const std::array<Hud::Rect, 5> groupedInset = Controls::Boxes(rules, kGrouped, kView169, notch);
        CHECK_MSG(Near(groupedInset[1].min.x - groupedFlat[1].min.x, 10.0, 1e-9) && Near(groupedInset[1].min.y - groupedFlat[1].min.y, -40.0, 1e-9), "the grouped right arrow is 10 in from the left and 40 up from the bottom");
        CHECK(groupedInset[0].size == groupedFlat[0].size && Near(groupedInset[1].min.x - groupedInset[0].Max().x, groupedFlat[1].min.x - groupedFlat[0].Max().x, 1e-9));
    }
    CHECK_MSG(Near(inset[3].min.x - flat[3].min.x, -30.0, 1e-9) && Near(inset[3].min.y - flat[3].min.y, 20.0, 1e-9), "pause is 30 in from the right and 20 down from the top");
    CHECK_MSG(Near(inset[2].min.x - flat[2].min.x, -30.0, 1e-9) && Near(inset[2].min.y - flat[2].min.y, 20.0, 1e-9), "and restart");
    CHECK_MSG(Near(inset[4].min.x - flat[4].min.x, 10.0, 1e-9) && Near(inset[4].min.y - flat[4].min.y, 20.0, 1e-9), "clear-portals is 10 in from the left and 20 down");
    // The margin is from the safe rectangle: 15.64 inside it, on every control.
    CHECK_MSG(Near(inset[0].min.x, 10.0 + m, 1e-9) && Near(kHeight - 40.0 - inset[0].Max().y, m, 1e-9), "the pair's margin is measured from the safe rectangle");
    CHECK_MSG(Near(kView169.x - 30.0 - inset[3].Max().x, m, 1e-9) && Near(inset[3].min.y - 20.0, m, 1e-9), "and pause's");
    CHECK_MSG(Near(inset[4].min.x - 10.0, m, 1e-9) && Near(inset[4].min.y - 20.0, m, 1e-9), "and clear-portals'");
    // The sizes are the same, and the split right arrow's margin is measured from the safe rectangle's right and bottom edges too.
    CHECK(inset[0].size == flat[0].size && inset[1].size == flat[1].size);
    CHECK_MSG(Near(kView169.x - 30.0 - inset[1].Max().x, m, 1e-9) && Near(kHeight - 40.0 - inset[1].Max().y, m, 1e-9), "the split right arrow's margin is measured from the safe rectangle");
    // Zero insets are no insets, to the bit; the classic layout ignores them altogether.
    const std::array<Hud::Rect, 5> zero = Controls::Boxes(rules, kNoTuning, kView169, Controls::Insets{0.0, 0.0, 0.0, 0.0});
    for (std::size_t i = 0; i < 5; ++i) CHECK_MSG(SameBits(zero[i], flat[i]), "zero insets leave control " + std::to_string(i) + " where it was");
    const Controls::Layout classicFlat = Controls::LayOut(Controls::Style::Classic, rules, hud, kNoTuning, kView169, kNoInsets, 5000.0);
    const Controls::Layout classicNotch = Controls::LayOut(Controls::Style::Classic, rules, hud, kNoTuning, kView169, notch, 5000.0);
    for (std::size_t i = 0; i < 5; ++i) CHECK_MSG(SameBits(classicFlat.frames[i].rect, classicNotch.frames[i].rect) && SameBits(classicFlat.frames[i].hit, classicNotch.frames[i].hit), "the classic layout ignores the insets");
    // A tap in the notch's own strip is still the arrow's: the hit goes to the view's edge.
    const Controls::Layout laid = Controls::LayOut(Controls::Style::Modern, rules, hud, kNoTuning, kView169, notch, 5000.0);
    CHECK(laid.At(Controls::Id::Left).hit.Contains(glm::dvec2(2.0, kHeight - 2.0)));
    CHECK(laid.At(Controls::Id::Menu).hit.Contains(glm::dvec2(kView169.x - 2.0, 2.0)));
    CHECK_MSG(laid.At(Controls::Id::Right).hit.Contains(glm::dvec2(kView169.x - 2.0, kHeight - 2.0)), "and the split right arrow's, to the view's bottom-right corner");
}

void TheDrawRectPutsEveryTiersBodyOnTheSameFace() {
    const Controls::Rules rules = LoadModern();
    const std::array<Hud::Rect, 5> boxes = Controls::Boxes(rules, kNoTuning, kView169, kNoInsets);
    const double phi = rules.faceInset;
    for (std::size_t tier = 0; tier < Controls::kArtTierCount; ++tier) {
        for (const Controls::Id id : Controls::kClaimOrder) {
            const std::size_t i = static_cast<std::size_t>(id);
            const auto& entry = rules.art[tier][i];
            if (!entry) continue;
            const Hud::Rect draw = Controls::DrawRect(boxes[i], phi, entry->bodyInset);
            // Whatever the picture's own margin, the solid body inside it lands on the box's solid face.
            const glm::dvec2 bodyMin = draw.min + draw.size * entry->bodyInset;
            const glm::dvec2 bodySize = draw.size * (1.0 - 2.0 * entry->bodyInset);
            const glm::dvec2 faceMin = boxes[i].min + boxes[i].size * phi;
            const glm::dvec2 faceSize = boxes[i].size * (1.0 - 2.0 * phi);
            const std::string at = std::string(Controls::NameOf(static_cast<Controls::ArtTier>(tier))) + " " + Controls::KeyOf(id) + ": ";
            CHECK_MSG(Near(bodyMin.x, faceMin.x, 1e-9) && Near(bodyMin.y, faceMin.y, 1e-9) && Near(bodySize.x, faceSize.x, 1e-9) && Near(bodySize.y, faceSize.y, 1e-9),
                      at + "the body is " + At(bodyMin) + " " + At(bodySize) + ", the face " + At(faceMin) + " " + At(faceSize));
            if (tier == static_cast<std::size_t>(Controls::ArtTier::Original)) {
                CHECK_MSG(Near(draw.size.x, boxes[i].size.x * (1.0 - 2.0 * phi), 1e-9), at + "an opaque sprite is drawn into the face, 22 percent smaller than its box");
            } else {
                CHECK_MSG(RectIs(draw, {boxes[i].min.x, boxes[i].min.y, boxes[i].Max().x, boxes[i].Max().y}, 1e-9), at + "a picture with the same margin as the face's fills the box");
            }
        }
    }
    // The numbers: the arrow's box 31.8578 has a face of 24.89 (70 px at 720p) and a corner box's 32.0 (90 px).
    CHECK_MSG(Near(boxes[0].size.x * (1.0 - 2.0 * phi), 70.0 / kPxPerUnit, 1e-9), "the arrow's solid face is 70 px, 24.89 units");
    CHECK_MSG(Near(boxes[3].size.x * (1.0 - 2.0 * phi), 32.0, 1e-9), "a corner button's solid face is 32 units");
    // Any margin at all.
    for (const double mu : {0.0, 0.05, 0.1, 0.25, 0.4}) {
        const Hud::Rect draw = Controls::DrawRect(boxes[0], phi, mu);
        const glm::dvec2 bodyMin = draw.min + draw.size * mu;
        const glm::dvec2 bodySize = draw.size * (1.0 - 2.0 * mu);
        CHECK_MSG(Near(bodyMin.x, boxes[0].min.x + boxes[0].size.x * phi, 1e-9) && Near(bodySize.y, boxes[0].size.y * (1.0 - 2.0 * phi), 1e-9), "margin " + Num(mu));
        CHECK(Near(draw.Centre().x, boxes[0].Centre().x, 1e-9) && Near(draw.Centre().y, boxes[0].Centre().y, 1e-9));
    }
}

void TheOpacityIsAMultiplierOnTheByteAndTheDefaultIsUnchanged() {
    const Hud::Rules hud = Load();
    const Controls::Rules rules = LoadModern();
    // At the default the byte is the classic one at every age of the pulse, in the tutorial and out of it.
    for (const bool tutorial : {false, true}) {
        long differ = 0;
        for (double age = -16.0; age <= 20000.0; age += 4.0) {
            const int classic = Hud::PadAlphaByte(hud, age, tutorial);
            if (Controls::AlphaByte(rules, kNoTuning, classic) != classic) ++differ;
        }
        CHECK_MSG(differ == 0, std::string(tutorial ? "tutorial" : "ordinary") + " pads: the default byte differs from the classic at " + std::to_string(differ) + " ages");
    }
    // Opacity is a multiplier of 0.2 to 1.8: of the flat 120, every step is a whole byte, 24 a step.
    for (int n = 1; n <= 9; ++n) {
        Controls::Tuning tuning;
        tuning.opacitySteps = n;
        CHECK_MSG(tuning.Opacity(rules) == static_cast<double>(n) / 5.0, "opacity step " + std::to_string(n) + " is " + Num(tuning.Opacity(rules)));
        CHECK_MSG(Controls::AlphaByte(rules, tuning, 120) == 24 * n, "120 at opacity " + Num(tuning.Opacity(rules)) + " is " + std::to_string(Controls::AlphaByte(rules, tuning, 120)));
    }
    // Capped at a byte, never negative, and rounded to the nearest: 163 (1-01's door) x 0.6 = 97.8, x 1.8 = 293.4.
    Controls::Tuning dim;
    dim.opacitySteps = 3;
    Controls::Tuning bright;
    bright.opacitySteps = 9;
    CHECK_EQ(Controls::AlphaByte(rules, dim, 163), 98);
    CHECK_EQ(Controls::AlphaByte(rules, bright, 163), 255);
    CHECK_EQ(Controls::AlphaByte(rules, bright, 128), 230);
    CHECK_EQ(Controls::AlphaByte(rules, bright, 210), 255);
    CHECK_EQ(Controls::AlphaByte(rules, bright, 0), 0);
    CHECK_EQ(Controls::AlphaByte(rules, bright, -5), 0);
    CHECK_EQ(Controls::AlphaByte(rules, kNoTuning, 255), 255);
    int last = -1;
    long backwards = 0;
    for (int n = 1; n <= 9; ++n) {
        Controls::Tuning tuning;
        tuning.opacitySteps = n;
        const int byte = Controls::AlphaByte(rules, tuning, 163);
        if (byte < last) ++backwards;
        last = byte;
    }
    CHECK_MSG(backwards == 0, "a brighter step is never a dimmer byte");
    // The idle scale is the one lever on the base look: 0.53 of the 120 is the ghost look of Magic Rampage.
    Controls::Rules ghost = rules;
    ghost.idleScale = 0.53;
    CHECK_EQ(Controls::AlphaByte(ghost, kNoTuning, 120), 64);
    CHECK_EQ(Controls::AlphaByte(ghost, bright, 120), 114);
}

void TheRingFollowsTheArrowsCentre() {
    const Hud::Rules hud = Load();
    const Controls::Rules rules = LoadModern();
    for (const double age : {100.0, 400.0, 1000.0, 2000.0, 3000.0}) {
        const Hud::Ring ring = Hud::RingAt(hud, age, true);
        const Controls::Layout layout = Controls::LayOut(Controls::Style::Modern, rules, hud, kNoTuning, kView169, kNoInsets, age);
        CHECK_MSG(ring.shown, "1-01's pads ring at " + Num(age) + " ms");
        for (const Controls::Id id : kWalks) {
            const Controls::Frame& pad = layout.At(id);
            const Controls::RingPlace place = Controls::RingFor(rules, hud, ring, pad);
            const std::string at = std::string(Controls::KeyOf(id)) + " at " + Num(age) + " ms: ";
            // Centred on the box's centre, wherever it has slid to, and not on the screen corner the classic ring is centred on.
            CHECK_MSG(place.shown && place.centre.x == pad.rect.Centre().x && place.centre.y == pad.rect.Centre().y, at + "the ring is about the box's centre " + At(place.centre));
            const glm::dvec2 classicCentre = Controls::ClassicRingCentre(hud, id, kView169, age);
            CHECK_MSG(Near(glm::distance(place.centre, classicCentre), glm::distance(pad.rect.Centre(), classicCentre), 1e-9) && glm::distance(place.centre, classicCentre) > 10.0, at + "and not the pad's screen corner");
            // 3.125 boxes across at its fullest, in the classic ring's own growth.
            CHECK_MSG(Near(place.sizeUnits, 3.125 * pad.rect.size.x * (ring.sizeUnits / hud.pads.ringSizeUnits), 1e-9), at + "its size " + Num(place.sizeUnits));
            CHECK_MSG(place.alpha == ring.alpha, at + "its alpha is the classic ring's");
        }
    }
    // At its fullest the ring is 3.125 boxes: 99.56 across at size 0.7 and 142.22 at 1.0.
    Hud::Ring full;
    full.shown = true;
    full.sizeUnits = hud.pads.ringSizeUnits;
    full.alpha = 1.0;
    CHECK_MSG(Near(Controls::RingFor(rules, hud, full, Controls::LayOut(Controls::Style::Modern, rules, hud, kNoTuning, kView169, kNoInsets, 5000.0).At(Controls::Id::Left)).sizeUnits, 99.5556, 1e-3), "99.56 across at size 0.7");
    Controls::Tuning big;
    big.sizeSteps = 10;
    CHECK_MSG(Near(Controls::RingFor(rules, hud, full, Controls::LayOut(Controls::Style::Modern, rules, hud, big, kView169, kNoInsets, 5000.0).At(Controls::Id::Right)).sizeUnits, 142.2222, 1e-3), "142.22 across at size 1.0");
    // Not shown outside the tutorial or once the pulse has stopped, whatever the arrows do.
    const Controls::Layout laid = Controls::LayOut(Controls::Style::Modern, rules, hud, kNoTuning, kView169, kNoInsets, 5000.0);
    CHECK(!Controls::RingFor(rules, hud, Hud::RingAt(hud, 100.0, false), laid.At(Controls::Id::Left)).shown);
    CHECK(!Controls::RingFor(rules, hud, Hud::RingAt(hud, 90000.0, true), laid.At(Controls::Id::Left)).shown);
    CHECK(Controls::RingFor(rules, hud, Hud::RingAt(hud, 100.0, false), laid.At(Controls::Id::Left)).sizeUnits == 0.0);
    // A moved arrow takes its ring with it. Split: walk_right stands against the view's right edge, so it is moved LEFT (a move to the right would be
    // pulled back inside); grouped: it is moved right, as it always was.
    for (const Controls::Arrows arrows : kArrangements) {
        const double dx = arrows == Controls::Arrows::Split ? -120.0 : 120.0;
        Controls::Tuning moved = TunedFor(arrows);
        moved.move[static_cast<std::size_t>(Controls::Id::Right)] = glm::dvec2(dx, -40.0);
        const Controls::Frame home = Controls::LayOut(Controls::Style::Modern, rules, hud, TunedFor(arrows), kView169, kNoInsets, 5000.0).At(Controls::Id::Right);
        const Controls::Frame movedPad = Controls::LayOut(Controls::Style::Modern, rules, hud, moved, kView169, kNoInsets, 5000.0).At(Controls::Id::Right);
        CHECK_MSG(Near(Controls::RingFor(rules, hud, full, movedPad).centre.x - home.rect.Centre().x, dx, 1e-9),
                  std::string(ArrangementName(arrows)) + ": a moved arrow takes its ring with it");
    }
    // The ring is centred on each arrow's own box: the right arrow's, split, in the bottom-right corner (407.6 + 15.9 = 423.5 across, 224.4 down).
    const Controls::Frame splitRight = Controls::LayOut(Controls::Style::Modern, rules, hud, kNoTuning, kView169, kNoInsets, 5000.0).At(Controls::Id::Right);
    const Controls::RingPlace splitRing = Controls::RingFor(rules, hud, full, splitRight);
    CHECK_MSG(Near(splitRing.centre.x, 423.5378, 1e-3) && Near(splitRing.centre.y, 224.4267, 1e-3), "split: the right arrow's ring is about its box's centre " + At(splitRing.centre));
}

void HintLeftClearsThePairAndLeavesClassicAlone() {
    const Hud::Rules hud = Load();
    const Controls::Rules rules = LoadModern();
    Hint::Rules hint;
    std::string error;
    CHECK_MSG(Hint::LoadRules(kData + "/camera.json", hint, error), error);
    // The text's width measured from Matura84_shadow.fnt: 639 font px of advance x 0.4 = 255.6, and 258.4 to the last glyph's edge
    // (design 1.6). A fixture here, because the font is the original's and this suite reads none. Its row is where camera.json puts it:
    // from 32 units above the bottom, a line of the font taller than a few units.
    const double extent = 258.4;
    const double rowTop = kHeight - hint.aboveBottomUnits;
    const Controls::HintRow row{hint.leftUnits, extent, rowTop, 33.6};
    const std::array<bool, 5> allShown = {true, true, true, true, true};
    struct Case {
        const char* name;
        double ratio;
        double atSeven; // size 0.7
        double atFourteen; // size 1.4
    };
    // GROUPED, the design's own results (1.6): the pair's hit right edge, 90.74 and 154.45, or the right clamp 8 units inside the view.
    const std::array<Case, 5> grouped = {{{"16:9", 16.0 / 9.0, 90.74, 154.45},
                                          {"16:10", 1.6, 90.74, 143.20},
                                          {"3:2", 1.5, 90.74, 117.60},
                                          {"4:3", 4.0 / 3.0, 74.93, 74.93},
                                          {"5:4", 1.25, 53.60, 53.60}}};
    // SPLIT (planning step 186): only the left arrow is one the text can be started clear of. Its hit ends at 47.50 at size 0.7, under the classic 50,
    // so the text does not move at the default at any shape; at size 1.4 it ends at 79.36, which the right clamp (W - 258.4 - 8) takes back to 74.93 at
    // 4:3 and 53.60 at 5:4, where the view's edge wins over the arrow's.
    const std::array<Case, 5> split = {{{"16:9", 16.0 / 9.0, 50.0, 79.36},
                                        {"16:10", 1.6, 50.0, 79.36},
                                        {"3:2", 1.5, 50.0, 79.36},
                                        {"4:3", 4.0 / 3.0, 50.0, 74.93},
                                        {"5:4", 1.25, 50.0, 53.60}}};
    for (const Controls::Arrows arrows : kArrangements) {
        for (const Case& c : arrows == Controls::Arrows::Split ? split : grouped) {
            const glm::dvec2 view = ViewOf(c.ratio);
            for (const int steps : {7, 14}) {
                const Controls::Tuning tuning = TunedFor(arrows, steps);
                const Controls::Layout layout = Controls::LayOut(Controls::Style::Modern, rules, hud, tuning, view, kNoInsets, 5000.0);
                const double left = Controls::HintLeft(Controls::Style::Modern, rules, layout, row, view, allShown);
                const double want = steps == 7 ? c.atSeven : c.atFourteen;
                const std::string at = std::string(ArrangementName(arrows)) + " " + c.name + " size " + Num(static_cast<double>(steps) / 10.0) + ": ";
                CHECK_MSG(Near(left, want, 0.006), at + "the hint starts at " + Num(left) + ", the design's " + Num(want));
                // However far right the arrows push it, its last glyph is 8 units inside the view's right edge or further in.
                CHECK_MSG(left + extent <= view.x - 8.0 + 1e-9 || left == hint.leftUnits, at + "the hint ends inside the view");
                // The classic style leaves it at 50, whatever the layout under it says.
                CHECK_MSG(Controls::HintLeft(Controls::Style::Classic, rules, layout, row, view, allShown) == 50.0, at + "classic is 50");
                // Only ever to the right of the classic place.
                CHECK_MSG(left >= hint.leftUnits - 1e-9 || left == view.x - extent - 8.0, at + "the hint is never moved left of the classic 50 unless the view's edge takes it");
            }
        }
    }

    // The whole sweep, the shapes 5:4 to 32:9 by the eleven sizes, from the design's formula written with its own literals (the left arrow's hit ends at
    // the margin 15.6444 plus its box 45.5111 x size; the text, 258.4 wide, ends 8 units inside the view): split moves nothing at 0.7 or below and shifts
    // only for the left arrow; grouped keeps the old answers.
    long sweep = 0;
    for (const Aspect& aspect : kAspects) {
        const glm::dvec2 view = ViewOf(aspect.ratio);
        for (int steps = 4; steps <= 14; ++steps) {
            const double arrowBox = 128.0 / kPxPerUnit * (static_cast<double>(steps) / 10.0);
            const double margin = 44.0 / kPxPerUnit;
            const double clamp = view.x - extent - 8.0;
            const Controls::Layout splitLayout = Controls::LayOut(Controls::Style::Modern, rules, hud, TunedFor(Controls::Arrows::Split, steps), view, kNoInsets, 5000.0);
            const double splitWant = std::min(std::max(50.0, margin + arrowBox), clamp);
            const double splitGot = Controls::HintLeft(Controls::Style::Modern, rules, splitLayout, row, view, allShown);
            CHECK_MSG(Near(splitGot, splitWant, 1e-9), std::string("split ") + aspect.name + " size " + Num(static_cast<double>(steps) / 10.0) + ": " + Num(splitGot) + ", the formula's " + Num(splitWant));
            const Controls::Layout groupedLayout = Controls::LayOut(Controls::Style::Modern, rules, hud, TunedFor(Controls::Arrows::Grouped, steps), view, kNoInsets, 5000.0);
            const double groupedWant = std::min(std::max(50.0, margin + arrowBox + 32.0 / kPxPerUnit + arrowBox), clamp);
            const double groupedGot = Controls::HintLeft(Controls::Style::Modern, rules, groupedLayout, row, view, allShown);
            CHECK_MSG(Near(groupedGot, groupedWant, 1e-9), std::string("grouped ") + aspect.name + " size " + Num(static_cast<double>(steps) / 10.0) + ": " + Num(groupedGot) + ", the formula's " + Num(groupedWant));
            // Split: at size 0.7 and below, at every shape, nothing moves (the left arrow ends under the classic 50).
            if (steps <= 7) CHECK_MSG(splitGot == 50.0, std::string("split ") + aspect.name + " size " + Num(static_cast<double>(steps) / 10.0) + ": the hint stays at the classic 50");
            ++sweep;
        }
    }
    CHECK_EQ(static_cast<int>(sweep), 9 * 11);

    // The arrows' hit right edge is what the text clears: grouped, the pair's (90.7378 at size 0.7); split, the LEFT arrow's alone (52.0533 at 0.8).
    const Controls::Layout groupedSeven = Controls::LayOut(Controls::Style::Modern, rules, hud, kGrouped, kView169, kNoInsets, 5000.0);
    CHECK_MSG(Controls::HintLeft(Controls::Style::Modern, rules, groupedSeven, row, kView169, allShown) == groupedSeven.At(Controls::Id::Right).hit.rect.Max().x, "grouped: it starts on the pair's hit right edge");
    const Controls::Layout splitEight = Controls::LayOut(Controls::Style::Modern, rules, hud, TunedFor(Controls::Arrows::Split, 8), kView169, kNoInsets, 5000.0);
    CHECK_MSG(Controls::HintLeft(Controls::Style::Modern, rules, splitEight, row, kView169, allShown) == splitEight.At(Controls::Id::Left).hit.rect.Max().x &&
                  Near(splitEight.At(Controls::Id::Left).hit.rect.Max().x, 52.0533, kTable),
              "split: at size 0.8 it starts on the left arrow's hit right edge, 52.0533");
    // The right arrow, in the corner, is never what moves the text: not at any size, and not when it is the only arrow shown.
    for (int steps = 4; steps <= 14; ++steps) {
        const Controls::Layout laid = Controls::LayOut(Controls::Style::Modern, rules, hud, TunedFor(Controls::Arrows::Split, steps), kView169, kNoInsets, 5000.0);
        std::array<bool, 5> rightOnly = {false, true, true, true, true};
        CHECK_MSG(Controls::HintLeft(Controls::Style::Modern, rules, laid, row, kView169, rightOnly) == 50.0, "split: with only the right arrow shown the text stays at 50, size " + Num(static_cast<double>(steps) / 10.0));
    }

    // Nothing to clear: the arrows are not shown (a weightless level), or the row is above them.
    const Controls::Layout seven = Controls::LayOut(Controls::Style::Modern, rules, hud, kGrouped, kView169, kNoInsets, 5000.0);
    std::array<bool, 5> none = allShown;
    none[static_cast<std::size_t>(Controls::Id::Left)] = false;
    none[static_cast<std::size_t>(Controls::Id::Right)] = false;
    CHECK(Controls::HintLeft(Controls::Style::Modern, rules, seven, row, kView169, none) == 50.0);
    CHECK(Controls::HintLeft(Controls::Style::Modern, rules, seven, Controls::HintRow{50.0, extent, 100.0, 33.6}, kView169, allShown) == 50.0);
    // Grouped, only the right arrow shown: its edge; only the left one: its own. (Both stand in the left half.)
    std::array<bool, 5> rightOnly = none;
    rightOnly[static_cast<std::size_t>(Controls::Id::Right)] = true;
    CHECK(Near(Controls::HintLeft(Controls::Style::Modern, rules, seven, row, kView169, rightOnly), 90.7378, kTable));
    std::array<bool, 5> leftOnly = none;
    leftOnly[static_cast<std::size_t>(Controls::Id::Left)] = true;
    CHECK(Near(Controls::HintLeft(Controls::Style::Modern, rules, seven, row, kView169, leftOnly), 53.1911, kTable));
    // Split, size 1.0 (the left arrow's hit ends at 61.1556): the left arrow alone moves the text there, and both shown give the same.
    const Controls::Layout splitTen = Controls::LayOut(Controls::Style::Modern, rules, hud, TunedFor(Controls::Arrows::Split, 10), kView169, kNoInsets, 5000.0);
    CHECK(Near(Controls::HintLeft(Controls::Style::Modern, rules, splitTen, row, kView169, leftOnly), 61.1556, kTable));
    CHECK(Near(Controls::HintLeft(Controls::Style::Modern, rules, splitTen, row, kView169, allShown), 61.1556, kTable));
    // A hint that is already clear of the arrows is not pushed: a row starting at 100 stays at 100, in both.
    CHECK(Controls::HintLeft(Controls::Style::Modern, rules, seven, Controls::HintRow{100.0, extent, rowTop, 33.6}, kView169, allShown) == 100.0);
    CHECK(Controls::HintLeft(Controls::Style::Modern, rules, splitTen, Controls::HintRow{100.0, extent, rowTop, 33.6}, kView169, allShown) == 100.0);

    // Where the free span between the two split arrows is narrower than the text (4:3 and 5:4 at the default size), the text cannot clear both: it clears the
    // left one, which the rule asks, and is drawn over the right one as the classic text crosses its disc. The numbers are the Step's.
    for (const auto& [name, ratio, overlap] : {std::tuple<const char*, double, double>{"5:4", 1.25, 35.9}, std::tuple<const char*, double, double>{"4:3", 4.0 / 3.0, 14.6},
                                              std::tuple<const char*, double, double>{"3:2", 1.5, 0.0}}) {
        const glm::dvec2 view = ViewOf(ratio);
        const Controls::Layout laid = Controls::LayOut(Controls::Style::Modern, rules, hud, kNoTuning, view, kNoInsets, 5000.0);
        const double left = Controls::HintLeft(Controls::Style::Modern, rules, laid, row, view, allShown);
        const double crosses = std::max(0.0, left + extent - laid.At(Controls::Id::Right).hit.rect.min.x);
        CHECK_MSG(Near(crosses, overlap, 0.05), std::string("split ") + name + " size 0.7: the text ends " + Num(crosses) + " units inside the right arrow's hit, the Step's " + Num(overlap));
        CHECK_MSG(left >= laid.At(Controls::Id::Left).hit.rect.Max().x, std::string("split ") + name + ": and it clears the left arrow");
    }
}

void TheHintsArrowEndsAboveThePairsHitRect() {
    const Hud::Rules hud = Load();
    const Controls::Rules rules = LoadModern();
    // The lesson's arrow, in the view at the start of 1-21 (design 1.6): x 85 to 110 and y 64 to 197. A fixture, because its
    // picture and its place are the original's (level20's census: up_down_arrow.ent at (100, 394), its box 25 x 133).
    const Hud::Rect arrow{glm::dvec2(85.0, 64.0), glm::dvec2(25.0, 133.0)};
    CHECK_MSG(arrow.Max().y == 197.0, "the arrow's end is 197");

    // GROUPED: the arrow is over the right arrow's columns, so the clearance is a vertical one.
    const Controls::Layout layout = Controls::LayOut(Controls::Style::Modern, rules, hud, kGrouped, kView169, kNoInsets, 5000.0);
    const Hud::Rect pair = layout.At(Controls::Id::Right).hit.rect;
    CHECK_MSG(arrow.min.x < pair.Max().x && arrow.Max().x > pair.min.x, "grouped: the arrow is over the right arrow's columns, so the clearance is a vertical one");
    CHECK_MSG(Near(pair.min.y - arrow.Max().y, 11.4978, 1e-3), "grouped: the arrow ends " + Num(pair.min.y - arrow.Max().y) + " units above the pair's hit rectangle");
    CHECK_MSG(arrow.Max().y < pair.min.y, "grouped: a drag that followed the arrow down does not reach the right arrow");
    CHECK_MSG(Overlap(arrow, pair) == 0.0 && Overlap(arrow, layout.At(Controls::Id::Left).hit.rect) == 0.0, "grouped: the arrow and the pair's hits do not meet");
    // The negative control: at Magic Rampage's own size 1.0 the grouped pair's top is 194.84 and the arrow's end is inside it.
    const Hud::Rect bigPair = Controls::LayOut(Controls::Style::Modern, rules, hud, TunedFor(Controls::Arrows::Grouped, 10), kView169, kNoInsets, 5000.0).At(Controls::Id::Right).hit.rect;
    CHECK_MSG(Near(bigPair.min.y, 194.8444, 1e-3) && Overlap(arrow, bigPair) > 0.0, "grouped: at size 1.0 the arrow's end is under the pair: that is why the default is 0.7");

    // SPLIT (planning step 186): the arrows stand in the two bottom corners, so the lesson's arrow (x 85 to 110) is over neither of them at ANY size: the left
    // arrow's hit ends at 79.36 even at size 1.4 and the right one's begins at 375.75 at 16:9. The arrow no longer limits the size.
    long checked = 0;
    for (const Aspect& aspect : kAspects) {
        const glm::dvec2 view = ViewOf(aspect.ratio);
        for (int steps = 4; steps <= 14; ++steps) {
            const Controls::Layout split = Controls::LayOut(Controls::Style::Modern, rules, hud, TunedFor(Controls::Arrows::Split, steps), view, kNoInsets, 5000.0);
            ++checked;
            CHECK_MSG(Overlap(arrow, split.At(Controls::Id::Left).hit.rect) == 0.0 && Overlap(arrow, split.At(Controls::Id::Right).hit.rect) == 0.0,
                      std::string("split ") + aspect.name + " size " + Num(static_cast<double>(steps) / 10.0) + ": the lesson's arrow meets neither arrow's hit");
            CHECK_MSG(split.At(Controls::Id::Left).hit.rect.Max().x <= arrow.min.x && split.At(Controls::Id::Right).hit.rect.min.x >= arrow.Max().x,
                      std::string("split ") + aspect.name + " size " + Num(static_cast<double>(steps) / 10.0) + ": and it stands between them");
        }
    }
    CHECK_EQ(static_cast<int>(checked), 9 * 11);
    // The left arrow's hit is nearest at size 1.4: 79.36, which is 5.64 units left of the arrow's left side.
    const Controls::Layout largest = Controls::LayOut(Controls::Style::Modern, rules, hud, TunedFor(Controls::Arrows::Split, 14), kView169, kNoInsets, 5000.0);
    CHECK_MSG(Near(arrow.min.x - largest.At(Controls::Id::Left).hit.rect.Max().x, 5.64, 1e-3), "split: at size 1.4 the left arrow's hit ends " + Num(arrow.min.x - largest.At(Controls::Id::Left).hit.rect.Max().x) + " units left of the arrow");
}

// ---- which style a run plays ---------------------------------------------------------------------------------

struct PolicyRow {
    const char* flag; // nullptr: no --controls
    bool fixedStep;
    bool route;
    bool visit;
    bool available;
    int forced; // -1 none, 0 classic, 1 modern
    bool save;
};

void ResolvePolicyTruthTable() {
    // Design 2.1's table, every row, with every combination of the three rig flags that make a run a developer's.
    long rows = 0;
    for (int bits = 0; bits < 8; ++bits) {
        const bool fixedStep = (bits & 1) != 0;
        const bool route = (bits & 2) != 0;
        const bool visit = (bits & 4) != 0;
        const bool dev = bits != 0;
        const std::array<PolicyRow, 4> table = {{{"classic", fixedStep, route, visit, false, 0, false},
                                                 {"modern", fixedStep, route, visit, true, 1, false},
                                                 {"modern,preset=mr", fixedStep, route, visit, true, 1, false},
                                                 {nullptr, fixedStep, route, visit, !dev, -1, !dev}}};
        for (const PolicyRow& row : table) {
            const std::optional<std::string> flag = row.flag != nullptr ? std::optional<std::string>(row.flag) : std::nullopt;
            const Controls::Policy policy = Controls::ResolvePolicy(flag, row.fixedStep, row.route, row.visit);
            const std::string at = std::string(row.flag != nullptr ? row.flag : "no flag") + " rig " + std::to_string(bits) + ": ";
            ++rows;
            CHECK_MSG(policy.valid && policy.error.empty(), at + "is readable");
            CHECK_MSG(policy.available == row.available, at + "available");
            CHECK_MSG(policy.saveAllowed == row.save, at + "save allowed");
            CHECK_MSG(row.forced < 0 ? !policy.forced.has_value() : (policy.forced.has_value() && *policy.forced == (row.forced == 1 ? Controls::Style::Modern : Controls::Style::Classic)), at + "forced");
            CHECK_MSG(policy.flag.Any() == (row.flag != nullptr && std::string(row.flag) == "modern,preset=mr"), at + "the flag's keys");
        }
    }
    CHECK_EQ(static_cast<int>(rows), 32);
    // The preset the flag carried.
    const Controls::Policy mr = Controls::ResolvePolicy(std::string("modern,preset=mr"), false, false, false);
    CHECK(mr.flag.preset == Controls::Preset::Mr && !mr.flag.size && !mr.flag.opacity && !mr.flag.arrows);
    // And the arrangement of the walk arrows (planning step 186): `arrows=grouped` is a key like `preset=`, a capture's and a test's, on the policy of a
    // forced modern run on any kind of run, never saved.
    for (int bits : {0, 1, 7}) {
        const Controls::Policy groupedPolicy = Controls::ResolvePolicy(std::string("modern,arrows=grouped"), (bits & 1) != 0, (bits & 2) != 0, (bits & 4) != 0);
        CHECK_MSG(groupedPolicy.valid && groupedPolicy.available && groupedPolicy.forced == Controls::Style::Modern && !groupedPolicy.saveAllowed &&
                      groupedPolicy.flag.arrows == Controls::Arrows::Grouped && groupedPolicy.flag.Any() && !groupedPolicy.flag.preset,
                  "arrows=grouped rides the policy of a forced modern run, rig " + std::to_string(bits));
        const Controls::Policy splitPolicy = Controls::ResolvePolicy(std::string("modern,arrows=split,preset=balanced"), (bits & 1) != 0, (bits & 2) != 0, (bits & 4) != 0);
        CHECK_MSG(splitPolicy.valid && splitPolicy.flag.arrows == Controls::Arrows::Split && splitPolicy.flag.preset == Controls::Preset::Balanced, "arrows=split and a preset together");
    }
    CHECK(!Controls::ResolvePolicy(std::string("classic,arrows=grouped"), false, false, false).valid);

    // A bare Paths (every suite, every route) is a default Policy: classic, nothing.
    const Controls::Policy bare;
    CHECK(bare.valid && !bare.available && !bare.forced.has_value() && !bare.saveAllowed && !bare.flag.Any());

    // What cannot be read: the style is a word, spelt as the table spells it, and classic takes no keys.
    const std::array<const char*, 19> bad = {"bogus", "", "Modern", "CLASSIC", "classic,size=1", "classic,", "modern,", "modern,bogus=1", "modern,size=x",
                                             "modern,preset=big", "modern,walk_left=1", "modern,size=1,size=1", "modern,,", "modern,size=1,",
                                             "modern,arrows=big", "modern,arrows=", "modern,arrows=Split", "modern,arrows=split,arrows=grouped", "modern,arrows=grouped,arrows=grouped"};
    for (const char* text : bad) {
        for (int bits : {0, 1, 7}) {
            const Controls::Policy policy = Controls::ResolvePolicy(std::string(text), (bits & 1) != 0, (bits & 2) != 0, (bits & 4) != 0);
            CHECK_MSG(!policy.valid && !policy.error.empty() && !policy.available && !policy.saveAllowed, std::string("--controls '") + text + "' is refused with a message: " + policy.error);
        }
    }
    CHECK_MSG(Controls::ResolvePolicy(std::string("bogus"), false, false, false).error.find("--controls") != std::string::npos, "the refusal carries the usage");
}

void ResolveStyleTruthTable() {
    using Controls::Style;
    // Explicit rows first, each from the design's table, so that the generic sweep below cannot agree with a wrong rule.
    const auto policy = [](const char* flag, bool dev) {
        return Controls::ResolvePolicy(flag != nullptr ? std::optional<std::string>(flag) : std::nullopt, dev, false, false);
    };
    CHECK(Controls::ResolveStyle(policy("modern", false), Style::Classic, Style::Classic) == Style::Modern);       // a force wins over a save
    CHECK(Controls::ResolveStyle(policy("modern", true), std::nullopt, Style::Classic) == Style::Modern);          // on a developer run too
    CHECK(Controls::ResolveStyle(policy("classic", false), Style::Modern, Style::Modern) == Style::Classic);       // classic forces classic
    CHECK(Controls::ResolveStyle(policy(nullptr, true), Style::Modern, Style::Modern) == Style::Classic);          // a developer run ignores a save and the default
    CHECK(Controls::ResolveStyle(policy(nullptr, false), Style::Modern, Style::Classic) == Style::Modern);         // a player's save
    CHECK(Controls::ResolveStyle(policy(nullptr, false), Style::Classic, Style::Modern) == Style::Classic);        // and it beats the default
    CHECK(Controls::ResolveStyle(policy(nullptr, false), std::nullopt, Style::Classic) == Style::Classic);         // no save: the shipped default
    CHECK(Controls::ResolveStyle(policy(nullptr, false), std::nullopt, Style::Modern) == Style::Modern);
    CHECK(Controls::ResolveStyle(Controls::Policy{}, Style::Modern, Style::Modern) == Style::Classic);             // a bare Paths

    // And every policy, saved and default, against the rule stated once: not available, classic; forced, the forced one; the save; the default.
    long rows = 0;
    for (const char* flag : {static_cast<const char*>(nullptr), "classic", "modern"}) {
        for (const bool dev : {false, true}) {
            const Controls::Policy p = policy(flag, dev);
            for (const std::optional<Style> saved : {std::optional<Style>(), std::optional<Style>(Style::Classic), std::optional<Style>(Style::Modern)}) {
                for (const Style shipped : {Style::Classic, Style::Modern}) {
                    Style want = shipped;
                    if (saved) want = *saved;
                    if (flag != nullptr) want = std::string(flag) == "modern" ? Style::Modern : Style::Classic;
                    if (flag == nullptr && dev) want = Style::Classic;
                    ++rows;
                    CHECK_MSG(Controls::ResolveStyle(p, saved, shipped) == want, std::string(flag != nullptr ? flag : "no flag") + (dev ? " on a developer run" : "") + ": the style is " + Controls::NameOf(want));
                }
            }
        }
    }
    CHECK_EQ(static_cast<int>(rows), 36);
}

// ---- the file ------------------------------------------------------------------------------------------------

std::filesystem::path ScratchDir() {
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "supersonic-test-mp-controls";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir;
}

// A JSON document written back out. Magic Rampage's numbers are plain doubles; an infinity is written as the text 1e999, which a
// reader takes back as one.
std::string Write(const Supersonic::Json::Value& value) {
    namespace Json = Supersonic::Json;
    switch (value.GetType()) {
    case Json::Type::Null:
        return "null";
    case Json::Type::Bool:
        return value.AsBool() ? "true" : "false";
    case Json::Type::Number: {
        const double n = value.AsNumber();
        if (!std::isfinite(n)) return n < 0.0 ? "-1e999" : "1e999";
        char buffer[64];
        const auto [end, error] = std::to_chars(buffer, buffer + sizeof(buffer), n);
        return error == std::errc{} ? std::string(buffer, end) : std::string("0");
    }
    case Json::Type::String:
        return "\"" + Json::Escape(value.AsString()) + "\"";
    case Json::Type::Array: {
        std::string text = "[";
        for (const Json::Value& item : value.AsArray()) text += (text.size() > 1 ? "," : "") + Write(item);
        return text + "]";
    }
    case Json::Type::Object: {
        std::string text = "{";
        for (const std::string& key : value.OrderedKeys()) text += (text.size() > 1 ? "," : "") + std::string("\"") + Json::Escape(key) + "\":" + Write(value[key]);
        return text + "}";
    }
    }
    return "null";
}

using Path = std::vector<std::string>;

// The document with the value at `path` replaced, or removed when `replacement` is empty. The other keys keep their values.
Supersonic::Json::Value Edit(const Supersonic::Json::Value& node, const Path& path, std::size_t depth, const std::optional<Supersonic::Json::Value>& replacement) {
    Supersonic::Json::Object object = node.AsObject();
    if (depth + 1 == path.size()) {
        if (replacement) {
            object.insert_or_assign(path[depth], *replacement);
        } else {
            object.erase(path[depth]);
        }
    } else {
        object.insert_or_assign(path[depth], Edit(node[path[depth]], path, depth + 1, replacement));
    }
    return Supersonic::Json::Value(std::move(object));
}

std::string Dotted(const Path& path) {
    std::string text;
    for (const std::string& part : path) text += (text.empty() ? "" : ".") + part;
    return text;
}

// Every key that does not begin with an underscore, as a path, objects before what they hold.
void Collect(const Supersonic::Json::Value& node, Path& prefix, std::vector<Path>& out) {
    for (const std::string& key : node.OrderedKeys()) {
        if (!key.empty() && key[0] == '_') continue;
        prefix.push_back(key);
        out.push_back(prefix);
        if (node[key].IsObject()) Collect(node[key], prefix, out);
        prefix.pop_back();
    }
}

Supersonic::Json::Value ReadControls() {
    std::ifstream file(kControls, std::ios::binary);
    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string text = buffer.str();
    Supersonic::Json::Value root;
    std::string error;
    CHECK_MSG(Supersonic::Json::Parse(text, root, error), "controls.json parses: " + error);
    return root;
}

bool LoadScratch(const std::string& name, const std::string& text, Controls::Rules& rules, std::string& error) {
    const std::filesystem::path path = ScratchDir() / name;
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file << text;
    }
    return Controls::LoadRules(path.string(), rules, error);
}

void WhatIsRefused() {
    namespace Json = Supersonic::Json;
    // The scratch copies this case writes go when it ends, and any that a run which crashed left go before it starts.
    struct Clean {
        static void Run() {
            std::error_code ec;
            std::filesystem::remove_all(ScratchDir(), ec);
        }
        Clean() { Run(); }
        ~Clean() { Run(); }
    } clean;
    const Json::Value root = ReadControls();
    std::vector<Path> paths;
    Path prefix;
    Collect(root, prefix, paths);
    // The `options` block is the Options screen's, read by Options::LoadRules and refused key by key in test_mp_options; this table is
    // Controls::LoadRules', which leaves that block to it (a key it does not know is ignored), so it is not in this table.
    paths.erase(std::remove_if(paths.begin(), paths.end(), [](const Path& path) { return path.front() == "options"; }), paths.end());
    CHECK_MSG(paths.size() > 100, "the file has " + std::to_string(paths.size()) + " keys of the table");

    // The shipped file loads, and so does its own rewrite through the writer used below (so a refusal is the edit's, not the writer's).
    Controls::Rules rules;
    std::string error;
    CHECK_MSG(LoadScratch("whole.json", Write(root), rules, error), "the file rewritten loads: " + error);

    // Each key removed, one at a time, in a scratch copy: refused, and the message names it. Every key, the three looks' bottom margins included
    // (they were the one optional key, which fell back to layout.margin_px.bottom, and a misspelt one then loaded cleanly at the wrong height).
    long refused = 0;
    for (const Path& path : paths) {
        Controls::Rules r;
        std::string why;
        const bool loaded = LoadScratch("removed.json", Write(Edit(root, path, 0, std::nullopt)), r, why);
        const bool named = !loaded && why.find(path.back()) != std::string::npos;
        if (named) ++refused;
        CHECK_MSG(named, "removing " + Dotted(path) + " is refused, naming it: " + why);
    }
    CHECK_MSG(refused > 90 && refused == static_cast<long>(paths.size()), std::to_string(refused) + " keys refused of " + std::to_string(paths.size()));
    // A look's bottom margin misspelt is a missing one: refused, naming the key and the look, whichever look and however it is misspelt - and it
    // is no longer rescued by layout.margin_px.bottom, which is still read (so a file that drops it is refused too, above).
    for (const Controls::Preset preset : Controls::kPresets) {
        for (const char* wrong : {"margin_botom_px", "margin_bottom", "Margin_bottom_px", "margin_bottom_px "}) {
            const std::string look = Controls::NameOf(preset);
            Json::Value edited = Edit(root, Path{"layout", "presets", look, "margin_bottom_px"}, 0, std::nullopt);
            edited = Edit(edited, Path{"layout", "presets", look, wrong}, 0, Json::Value(30.0));
            Controls::Rules r;
            std::string why;
            const bool loaded = LoadScratch("misspelt.json", Write(edited), r, why);
            CHECK_MSG(!loaded && why.find("margin_bottom_px") != std::string::npos && why.find(look) != std::string::npos,
                      std::string("'") + wrong + "' for the " + look + " look is refused, naming margin_bottom_px and the look: " + why);
        }
    }

    // A typo in a `_source`, a note, or a key this reader does not know is not refused: provenance is not read.
    {
        Json::Value edited = Edit(root, Path{"_sourse"}, 0, Json::Value(std::string("typo")));
        edited = Edit(edited, Path{"layout", "extra"}, 0, Json::Value(5.0));
        edited = Edit(edited, Path{"layout", "_source"}, 0, Json::Value(std::string("not even an array")));
        edited = Edit(edited, Path{"hit", "grow_px", "walk_diagonal"}, 0, Json::Value(7.0));
        Controls::Rules r;
        std::string why;
        CHECK_MSG(LoadScratch("typo.json", Write(edited), r, why), "a typo in a note, and unknown keys, are not refused: " + why);
    }

    // A value out of its type or its range is refused, and the three values the mutation gate sets are not.
    struct Edit1 {
        Path path;
        Json::Value value;
        bool loads;
    };
    const auto number = [](double n) { return Json::Value(n); };
    const auto text = [](const char* s) { return Json::Value(std::string(s)); };
    const std::vector<Edit1> edits = {
        // 2 is the number of the arrangement this code lays out (planning step 186); 1 was the grouped pair's (planning step 180) and is another's now.
        {{"layout", "version"}, number(1.0), false},
        {{"layout", "version"}, number(3.0), false},
        {{"layout", "version"}, text("2"), false},
        {{"layout", "version"}, number(1.5), false},
        {{"reference", "height_px"}, number(0.0), false},
        {{"reference", "height_px"}, number(-720.0), false},
        {{"layout", "margin_px", "side"}, number(-1.0), false},
        {{"layout", "margin_px", "side"}, number(40.0), true},
        {{"layout", "margin_px", "side"}, number(0.0), true},
        {{"layout", "margin_px", "side"}, text("44"), false},
        {{"layout", "margin_px", "top"}, number(-0.5), false},
        {{"layout", "gap_px"}, number(-1.0), false},
        {{"layout", "box_px"}, number(0.0), false},
        {{"layout", "face_inset"}, number(0.5), false},
        {{"layout", "face_inset"}, number(-0.1), false},
        {{"layout", "corner_box_factor"}, number(0.0), false},
        {{"layout", "corner_min_size"}, number(-1.0), false},
        {{"layout", "presets", "uniform", "size"}, number(0.75), false},
        {{"layout", "presets", "uniform", "size"}, number(0.3), false},
        {{"layout", "presets", "uniform", "size"}, number(1.5), false},
        {{"layout", "presets", "uniform", "size"}, number(1.4), true},
        {{"layout", "presets", "uniform", "size"}, number(0.4), true},
        {{"layout", "presets", "mr", "margin_bottom_px"}, number(-5.0), false},
        {{"layout", "default_preset"}, text("bogus"), false},
        {{"layout", "default_preset"}, text("Uniform"), false},
        {{"layout", "default_preset"}, text("balanced"), true},
        {{"layout", "default_arrows"}, text("bogus"), false},
        {{"layout", "default_arrows"}, text("Split"), false},
        {{"layout", "default_arrows"}, text(""), false},
        {{"layout", "default_arrows"}, number(1.0), false},
        {{"layout", "default_arrows"}, text("grouped"), true},
        {{"layout", "default_arrows"}, text("split"), true},
        {{"hit", "seam_px"}, number(-1.0), false},
        {{"hit", "edge_reach_px"}, number(-1.0), false},
        {{"hit", "edge_reach_px"}, number(0.0), true},
        {{"hit", "grow_px", "walk_left"}, number(16.0), true},
        {{"hit", "grow_px", "walk_left"}, number(-1.0), false},
        {{"hit", "grow_px", "pause"}, text("x"), false},
        {{"corner", "act_on"}, text("hold"), false},
        {{"corner", "act_on"}, text("press"), true},
        // The switch for a second finger (planning step 191): a true or a false, nothing else, and the block has to be there.
        {{"input", "two_contacts"}, Json::Value(false), true},
        {{"input", "two_contacts"}, Json::Value(true), true},
        {{"input", "two_contacts"}, text("true"), false},
        {{"input", "two_contacts"}, number(1.0), false},
        {{"input"}, number(1.0), false},
        {{"size", "step"}, number(0.3), false},
        {{"size", "step"}, number(0.0), false},
        {{"size", "max"}, number(1.45), false},
        {{"size", "min"}, number(1.5), false},
        {{"opacity", "default"}, number(1.1), false},
        {{"opacity", "default"}, number(2.0), false},
        {{"opacity", "default"}, number(0.2), true},
        {{"opacity", "base_alpha_byte"}, number(256.0), false},
        {{"opacity", "base_alpha_byte"}, number(120.5), false},
        {{"opacity", "idle_scale"}, number(0.0), false},
        {{"pressed", "boost_byte"}, number(300.0), false},
        {{"ring", "size_over_box"}, number(0.0), false},
        {{"hint", "margin_units"}, number(-1.0), false},
        {{"art", "mr", "walk_left", "body_inset"}, number(0.5), false},
        {{"art", "mr", "walk_left", "body_inset"}, number(0.6), false},
        // A picture the port draws itself carries a margin sim/PadArt can draw, and no more: what loads, draws. A picture read from a file is
        // not PadArt's, and may carry a larger margin up to the loader's own bound.
        {{"art", "generated", "walk_left", "body_inset"}, number(0.25), true},
        {{"art", "generated", "walk_left", "body_inset"}, number(0.0), true},
        {{"art", "generated", "walk_left", "body_inset"}, number(0.2501), false},
        {{"art", "generated", "walk_left", "body_inset"}, number(0.3), false},
        {{"art", "generated", "pause", "body_inset"}, number(0.4), false},
        {{"art", "generated", "clear_portals", "body_inset"}, number(-0.01), false},
        {{"art", "mr", "walk_left", "body_inset"}, number(0.3), true},
        {{"art", "original", "pause", "body_inset"}, number(0.3), true},
        {{"art", "original", "pause", "body_inset"}, number(0.5), false},
        {{"art", "mr", "walk_left", "file"}, text(""), false},
        {{"art", "mr", "walk_left", "file"}, number(3.0), false},
        {{"art", "original", "pause"}, number(3.0), false},
        // A picture's file is joined to the folder of its tier, and the generated tier WRITES there (planning step 184): it is ONE file in it
        // (Controls::IsBareFileName), in every tier. A folder, a ".." or a drive is refused at load, naming the key; a plain other name loads.
        {{"art", "mr", "walk_left", "file"}, text("../walk_left.png"), false},
        {{"art", "mr", "pause", "file"}, text("..\\pause.png"), false},
        {{"art", "mr", "walk_left", "file"}, text("left_arrow.png"), true},
        {{"art", "original", "restart", "file"}, text("sprites/restart_level_button.png"), false},
        {{"art", "original", "clear_portals", "file"}, text("C:/clear_portals_button.png"), false},
        {{"art", "original", "pause", "file"}, text("pause_button.png"), true},
        {{"art", "generated", "walk_left", "file"}, text("../modern_walk_left.png"), false},
        {{"art", "generated", "walk_right", "file"}, text("/tmp/modern_walk_right.png"), false},
        {{"art", "generated", "restart", "file"}, text("C:\\modern_restart.png"), false},
        {{"art", "generated", "pause", "file"}, text("C:modern_pause.png"), false},
        {{"art", "generated", "clear_portals", "file"}, text(".."), false},
        {{"art", "generated", "clear_portals", "file"}, text("."), false},
        {{"art", "generated", "walk_left", "file"}, text("a..b.png"), false},
        {{"art", "generated", "walk_left", "file"}, text("modern_walk_left.png "), false},
        {{"art", "generated", "walk_left", "file"}, text("modern_walk_left."), false},
        {{"art", "generated", "walk_left", "file"}, text("renamed_walk_left.png"), true},
        {{"census", "em_units"}, number(-1.0), false},
        {{"default_mode"}, text("retro"), false},
        {{"default_mode"}, text("modern"), true},
        {{"clearance_px"}, number(std::numeric_limits<double>::infinity()), false},
        {{"hit"}, number(5.0), false},
        {{"art"}, text("x"), false},
    };
    for (const Edit1& edit : edits) {
        Controls::Rules r;
        std::string why;
        const bool loaded = LoadScratch("edited.json", Write(Edit(root, edit.path, 0, edit.value)), r, why);
        CHECK_MSG(loaded == edit.loads, Dotted(edit.path) + " = " + Write(edit.value) + (edit.loads ? " loads" : " is refused") + (loaded ? "" : ": " + why));
        if (!edit.loads) CHECK_MSG(!loaded && why.find(edit.path.back()) != std::string::npos, Dotted(edit.path) + ": the message names it: " + why);
    }
    // A grid whose limits are more steps than an int counts is refused whole, naming it, and not converted out of range: a step of a millionth
    // under a limit of a million is 10^12 steps. (Two numbers at once, which the single-edit table above cannot say.) The step alone, a millionth,
    // is no trouble: it is the limit that overflows.
    for (const char* grid : {"size", "opacity"}) {
        Json::Value tooMany = Edit(root, Path{grid, "step"}, 0, number(0.000001));
        tooMany = Edit(tooMany, Path{grid, "max"}, 0, number(1000000.0));
        Controls::Rules r;
        std::string why;
        CHECK_MSG(!LoadScratch("big_grid.json", Write(tooMany), r, why) && why.find(grid) != std::string::npos && why.find("max") != std::string::npos,
                  std::string(grid) + " with 10^12 steps is refused, naming it: " + why);
    }

    // The values the mutation gate sets are read as set: the file is read, not copied.
    {
        Controls::Rules r;
        std::string why;
        CHECK(LoadScratch("m1.json", Write(Edit(root, Path{"layout", "margin_px", "side"}, 0, number(40.0))), r, why) && r.marginSidePx == 40.0);
        CHECK(LoadScratch("m2.json", Write(Edit(root, Path{"hit", "edge_reach_px"}, 0, number(0.0))), r, why) && r.hit.edgeReachPx == 0.0);
        CHECK(LoadScratch("m3.json", Write(Edit(root, Path{"hit", "grow_px", "walk_left"}, 0, number(16.0))), r, why) && r.hit.growPx[0] == 16.0 && r.hit.growPx[1] == 0.0);
        // The shipped switch is on (the owner's answers of 2026-10-15 and 2026-10-16), and the file is read, not copied: false is false.
        CHECK_MSG(LoadScratch("m4.json", Write(root), r, why) && r.twoContacts, "the shipped data reads two fingers: " + why);
        CHECK_MSG(LoadScratch("m5.json", Write(Edit(root, Path{"input", "two_contacts"}, 0, Json::Value(false))), r, why) && !r.twoContacts, "and false is read as false: " + why);
    }

    // Not a document: nothing there, not an object, not JSON.
    {
        Controls::Rules r;
        std::string why;
        CHECK(!Controls::LoadRules((ScratchDir() / "missing-controls.json").string(), r, why) && why.find("cannot open") != std::string::npos);
        CHECK(!LoadScratch("array.json", "[]", r, why) && !why.empty());
        CHECK(!LoadScratch("broken.json", "{\"layout\": ", r, why) && !why.empty());
        CHECK(!LoadScratch("empty.json", "{}", r, why) && !why.empty());
    }
}

// A number leaf needs the object that holds it to say where it came from: a `_source` that names it, or a `_guess` (of the object or
// of the key, `_guess_<key>`) that says it is the port's own. A source line is a string.
void EveryNumberInControlsJsonSaysWhereItCameFrom() {
    namespace Json = Supersonic::Json;
    const Json::Value root = ReadControls();
    long leaves = 0;
    long guesses = 0;
    long sourced = 0;
    std::vector<std::pair<const Json::Value*, std::string>> stack = {{&root, ""}};
    while (!stack.empty()) {
        const auto [node, where] = stack.back();
        stack.pop_back();
        std::string sourceText;
        bool wellFormed = true;
        if (node->Has("_source")) {
            const Json::Value& source = (*node)["_source"];
            wellFormed = source.IsArray() && !source.AsArray().empty();
            for (const Json::Value& line : source.AsArray()) {
                wellFormed = wellFormed && line.IsString() && !line.AsString().empty();
                sourceText += line.AsString() + "\n";
            }
        }
        CHECK_MSG(wellFormed, (where.empty() ? std::string("the document") : where) + ": _source is a non-empty array of lines");
        if (node->Has("_guess")) CHECK_MSG((*node)["_guess"].IsBool() && (*node)["_guess"].AsBool(), where + "._guess is true");
        for (const std::string& key : node->OrderedKeys()) {
            const Json::Value& child = (*node)[key];
            if (!key.empty() && key[0] == '_') continue;
            if (child.IsObject()) {
                stack.push_back({&child, where.empty() ? key : where + "." + key});
                continue;
            }
            if (!child.IsNumber()) continue;
            ++leaves;
            const bool guessed = node->Has("_guess") && (*node)["_guess"].AsBool();
            const bool keyGuessed = node->Has("_guess_" + key) && (*node)["_guess_" + key].AsBool();
            const bool named = sourceText.find(key) != std::string::npos;
            if (guessed || keyGuessed) ++guesses;
            if (named) ++sourced;
            CHECK_MSG(guessed || keyGuessed || named, (where.empty() ? key : where + "." + key) + " has neither a _source that names it nor a _guess");
        }
    }
    CHECK_MSG(leaves > 45, "the walk met " + std::to_string(leaves) + " numbers");
    CHECK_MSG(guesses > 15 && sourced > 40, std::to_string(guesses) + " guessed, " + std::to_string(sourced) + " sourced");
}

// ---- the saved file --------------------------------------------------------------------------------------------

Supersonic::Json::Value ParseText(const std::string& text) {
    Supersonic::Json::Value value;
    std::string error;
    CHECK_MSG(Supersonic::Json::Parse(text, value, error), "parses: " + text + ": " + error);
    return value;
}

bool SameTuning(const Controls::Tuning& a, const Controls::Tuning& b) {
    if (a.sizeSteps != b.sizeSteps || a.opacitySteps != b.opacitySteps) return false;
    for (std::size_t i = 0; i < 5; ++i) {
        if (a.move[i].x != b.move[i].x || a.move[i].y != b.move[i].y) return false;
    }
    return true;
}

void TheDefaultsOfAnAbsentTuningFileAreTheBase() {
    const Controls::Rules rules = LoadModern();
    // Nothing, an empty object, a file with only the mode, a file of another shape altogether: the default tuning.
    for (const char* text : {"{}", "{\"mode\":\"modern\",\"layout\":2}", "{\"mode\":\"modern\",\"layout\":1}", "{\"size\":null}", "[]", "5", "\"x\"", "null",
                             "{\"size\":\"big\",\"opacity\":[1],\"move\":\"no\"}", "{\"move\":{\"walk_left\":5,\"pause\":[1],\"restart\":[\"a\",\"b\"]}}"}) {
        const Controls::Tuning t = Controls::Tuning::ReadFrom(ParseText(text), rules);
        CHECK_MSG(SameTuning(t, kNoTuning) && t.IsDefault(rules), std::string("'") + text + "' reads as the default");
        CHECK_MSG(t.ToJson(rules) == "{}", std::string("'") + text + "' writes as {}");
    }
    CHECK(SameTuning(Controls::Tuning::ReadFrom(Supersonic::Json::Value(), rules), kNoTuning));
    // A size and an opacity and two moves, as the design's example writes them.
    const Controls::Tuning t = Controls::Tuning::ReadFrom(
        ParseText("{\"mode\":\"modern\",\"layout\":2,\"size\":1.2,\"opacity\":0.6,\"move\":{\"walk_left\":[2,-3.5],\"pause\":[-10.04,5],\"future\":[1,2]},\"later\":true}"), rules);
    CHECK(t.sizeSteps && *t.sizeSteps == 12 && t.Size(rules) == 1.2);
    CHECK(t.opacitySteps && *t.opacitySteps == 3 && t.Opacity(rules) == 0.6);
    CHECK(t.move[0].x == 2.0 && t.move[0].y == -3.5 && t.move[3].x == -10.0 && t.move[3].y == 5.0);
    CHECK(t.move[1].x == 0.0 && t.move[2].x == 0.0 && t.move[4].x == 0.0 && !t.preset);
    CHECK_MSG(t.ToJson(rules) == "{\"size\":1.2,\"opacity\":0.6,\"move\":{\"walk_left\":[2,-3.5],\"pause\":[-10,5]}}", "written: " + t.ToJson(rules));

    // A number off its grid is snapped, a number out of it held in it, and one that is not finite is the default.
    const auto sizeOf = [&rules](const char* text) { return Controls::Tuning::ReadFrom(ParseText(text), rules); };
    CHECK(sizeOf("{\"size\":1.25}").SizeStepsIn(rules) == 13);
    CHECK(sizeOf("{\"size\":1.24}").SizeStepsIn(rules) == 12);
    CHECK(sizeOf("{\"size\":0.1}").SizeStepsIn(rules) == 4 && sizeOf("{\"size\":0.1}").Size(rules) == 0.4);
    CHECK(sizeOf("{\"size\":9}").SizeStepsIn(rules) == 14 && sizeOf("{\"size\":-3}").SizeStepsIn(rules) == 4);
    CHECK(!sizeOf("{\"size\":1e999}").sizeSteps && sizeOf("{\"size\":1e999}").IsDefault(rules));
    CHECK(!sizeOf("{\"size\":-1e999}").sizeSteps);
    CHECK(sizeOf("{\"size\":0.7}").IsDefault(rules) && !sizeOf("{\"size\":0.7}").sizeSteps);
    CHECK(sizeOf("{\"opacity\":0.5}").OpacityStepsIn(rules) == 3 && sizeOf("{\"opacity\":5}").OpacityStepsIn(rules) == 9 && sizeOf("{\"opacity\":0}").OpacityStepsIn(rules) == 1);
    CHECK(sizeOf("{\"opacity\":1}").IsDefault(rules));
    // A move: whole tenths, held to 1,024; a pair that is not two numbers, or that holds one that is not finite, is no move.
    CHECK(sizeOf("{\"move\":{\"walk_left\":[5000,-5000]}}").move[0].x == 1024.0 && sizeOf("{\"move\":{\"walk_left\":[5000,-5000]}}").move[0].y == -1024.0);
    CHECK(sizeOf("{\"move\":{\"walk_left\":[1e999,3]}}").move[0].x == 0.0 && sizeOf("{\"move\":{\"walk_left\":[1e999,3]}}").move[0].y == 0.0);
    CHECK(sizeOf("{\"move\":{\"walk_left\":[1,2,3]}}").IsDefault(rules));
    CHECK(sizeOf("{\"move\":{\"walk_left\":[0.04,-0.04]}}").IsDefault(rules));

    // Moves made against another arrangement are dropped, silently; the size and the opacity are kept. A file with no number is the current one.
    // The number is the arrangement the moves were made against: 1 was the grouped pair's (planning step 180), 2 is the split arrows' (planning step 186).
    // A controls.json saved at layout 1 keeps its size and its opacity and drops its moves.
    const Controls::Tuning old = sizeOf("{\"layout\":1,\"size\":1.2,\"opacity\":0.6,\"move\":{\"walk_left\":[2,-3.5]}}");
    CHECK_MSG(old.sizeSteps && *old.sizeSteps == 12 && old.opacitySteps && *old.opacitySteps == 3 && old.move[0].x == 0.0 && old.move[0].y == 0.0, "another layout number drops the moves and keeps the rest");
    const Controls::Tuning future = sizeOf("{\"layout\":3,\"size\":1.2,\"opacity\":0.6,\"move\":{\"walk_left\":[2,-3.5]}}");
    CHECK_MSG(future.sizeSteps && *future.sizeSteps == 12 && future.opacitySteps && *future.opacitySteps == 3 && future.move[0].x == 0.0, "so does a number from the future");
    CHECK(sizeOf("{\"layout\":0,\"move\":{\"walk_left\":[2,-3.5]}}").IsDefault(rules));
    CHECK(sizeOf("{\"layout\":\"2\",\"move\":{\"walk_left\":[2,-3.5]}}").IsDefault(rules));
    CHECK(sizeOf("{\"layout\":2,\"move\":{\"walk_left\":[2,-3.5]}}").move[0].x == 2.0);
    CHECK(sizeOf("{\"layout\":1,\"move\":{\"walk_left\":[2,-3.5]}}").IsDefault(rules));
    CHECK(sizeOf("{\"move\":{\"walk_left\":[2,-3.5]}}").move[0].x == 2.0);
}

void ATuningIsWrittenOnOneLineAndReadBackExactly() {
    const Controls::Rules rules = LoadModern();
    // The default writes nothing; a value equal to the default is not written; a negative zero never is; NaN never is.
    CHECK(kNoTuning.ToJson(rules) == "{}");
    Controls::Tuning sameAsDefault;
    sameAsDefault.sizeSteps = 7;
    sameAsDefault.opacitySteps = 5;
    sameAsDefault.move[0] = glm::dvec2(-0.0, 0.04);
    CHECK_MSG(sameAsDefault.ToJson(rules) == "{}", "written: " + sameAsDefault.ToJson(rules));
    Controls::Tuning noisy;
    noisy.move[2] = glm::dvec2(std::nan(""), -0.04);
    noisy.move[3] = glm::dvec2(-0.04, 3.0);
    CHECK_MSG(noisy.ToJson(rules) == "{\"move\":{\"pause\":[0,3]}}", "written: " + noisy.ToJson(rules));
    CHECK(noisy.ToJson(rules).find("nan") == std::string::npos && noisy.ToJson(rules).find("-0") == std::string::npos && noisy.ToJson(rules).find('\n') == std::string::npos);
    // The look is the capture's, not the file's: it is not written, and the size it carries is judged against the data's default look.
    Controls::Tuning look;
    look.preset = Controls::Preset::Mr;
    CHECK(look.IsDefault(rules) && look.ToJson(rules) == "{}");
    look.sizeSteps = 7;
    CHECK_MSG(!look.IsDefault(rules) && look.ToJson(rules) == "{\"size\":0.7}", "under mr, 0.7 is a change: " + look.ToJson(rules));
    // Round trips: 3,000 tunings written, parsed and read give the same tuning, and the second writing is the first.
    Rng rng(777);
    long differ = 0;
    long nondefault = 0;
    for (int i = 0; i < 3000; ++i) {
        Controls::Tuning t;
        if (rng.Below(2) == 0) t.sizeSteps = rng.Range(4, 14);
        if (rng.Below(2) == 0) t.opacitySteps = rng.Range(1, 9);
        for (glm::dvec2& m : t.move) {
            if (rng.Below(3) == 0) m = glm::dvec2(static_cast<double>(rng.Range(-10240, 10240)) / 10.0, static_cast<double>(rng.Range(-10240, 10240)) / 10.0);
        }
        const Controls::Tuning clean = t.Clamped(rules);
        const std::string text = t.ToJson(rules);
        const Controls::Tuning back = Controls::Tuning::ReadFrom(ParseText(text), rules);
        if (!SameTuning(clean, back) || back.ToJson(rules) != text) ++differ;
        if (!clean.IsDefault(rules)) ++nondefault;
    }
    CHECK_MSG(differ == 0, std::to_string(differ) + " of 3,000 tunings did not read back as they were written");
    CHECK_MSG(nondefault > 2000, "the round trips met " + std::to_string(nondefault) + " tunings that were not the default");
}

void TheFlagGrammar() {
    const Controls::Rules rules = LoadModern();
    Controls::FlagItems items;
    std::string error;
    // Nothing is nothing; each key on its own; all together.
    CHECK(Controls::Tuning::ParseFlag("", items, error) && !items.Any());
    CHECK(Controls::Tuning::ParseFlag("preset=balanced", items, error) && items.preset == Controls::Preset::Balanced && items.Any());
    CHECK(Controls::Tuning::ParseFlag("size=1.2", items, error) && items.size == 1.2 && !items.preset);
    CHECK(Controls::Tuning::ParseFlag("opacity=0.6", items, error) && items.opacity == 0.6);
    CHECK(Controls::Tuning::ParseFlag("walk_left=10:-5", items, error) && items.move[0] && items.move[0]->x == 10.0 && items.move[0]->y == -5.0);
    CHECK(Controls::Tuning::ParseFlag("clear_portals=0.5:1e1", items, error) && items.move[4] && items.move[4]->x == 0.5 && items.move[4]->y == 10.0);
    CHECK(Controls::Tuning::ParseFlag("preset=mr,size=1.4,opacity=1.8,walk_left=1:2,walk_right=3:4,restart=-5:6,pause=7:-8,clear_portals=0:0", items, error));
    CHECK(items.preset == Controls::Preset::Mr && items.size == 1.4 && items.opacity == 1.8 && items.move[2] && items.move[2]->x == -5.0 && items.move[3] && items.move[3]->y == -8.0);
    // The arrangement of the walk arrows (planning step 186): a key like the look, either word, alone or among the rest, and nothing is a move or a size.
    CHECK(Controls::Tuning::ParseFlag("arrows=grouped", items, error) && items.arrows == Controls::Arrows::Grouped && items.Any() && !items.preset && !items.size && !items.opacity);
    CHECK(Controls::Tuning::ParseFlag("arrows=split", items, error) && items.arrows == Controls::Arrows::Split && items.Any());
    CHECK(Controls::Tuning::ParseFlag("preset=mr,arrows=grouped,size=1.4", items, error) && items.arrows == Controls::Arrows::Grouped && items.preset == Controls::Preset::Mr && items.size == 1.4);
    CHECK(Controls::Tuning::ParseFlag("size=1.4", items, error) && !items.arrows);
    // What is refused, each with a message that carries the usage.
    const std::array<const char*, 33> bad = {"arrows", "arrows=", "arrows=Grouped", "arrows=both", "arrows=split,arrows=split",
                                             "size", "=1", "size=", "size=abc", "size=1.2x", "size=1,size=2", "opacity=1,opacity=1", "preset=Mr", "preset=", "preset=big",
                                             "preset=mr,preset=mr", "walk_left=10", "walk_left=1:2:3", "walk_left=a:b", "walk_left=2000:0", "walk_left=0:-2000",
                                             "walk_left=1:2,walk_left=3:4", "bogus=1", "size=1,,opacity=1", ",size=1", "size=1,", "size=inf", "size=nan", "walk_left=1:inf",
                                             // A number beyond what a double holds is not a number; one inside it, however large, parses and is the grid's to refuse (below).
                                             "size=1e999", "size=-1e999", "opacity=1e999", "walk_left=1e999:0"};
    for (const char* text : bad) {
        Controls::FlagItems refused;
        std::string why;
        CHECK_MSG(!Controls::Tuning::ParseFlag(text, refused, why) && why.find("--controls") != std::string::npos, std::string("'") + text + "' is refused with the usage: " + why);
    }
    CHECK(std::string(Controls::FlagUsage()).find("preset=uniform|balanced|mr") != std::string::npos);
    CHECK_MSG(std::string(Controls::FlagUsage()).find("arrows=split|grouped") != std::string::npos, std::string("the usage names the arrangements: ") + Controls::FlagUsage());
    CHECK(std::string(Controls::FlagUsage()).find("walk_left") != std::string::npos && std::string(Controls::FlagUsage()).find("clear_portals") != std::string::npos);

    // Applied to the data's grids: a size or an opacity that is not a step is refused, one that is becomes steps; a move is snapped.
    Controls::Tuning tuning;
    Controls::FlagItems flag;
    flag.size = 1.2;
    CHECK(Controls::Tuning::FromFlag(flag, rules, tuning, error) && tuning.sizeSteps && *tuning.sizeSteps == 12);
    // A number that parses and is nowhere near the grid is refused by the grid, whole, and never converted to a count of steps first: 1e30 and 1e308 are
    // far past an int (and 1e308 times the grid's divisor is an infinity), and 214748364.8 is the first size whose tenths an int cannot hold.
    for (const char* huge : {"size=1e30", "size=-1e30", "size=1e308", "size=-1e308", "opacity=1e30", "opacity=-1e30", "opacity=1e308", "opacity=-1e308"}) {
        Controls::FlagItems parsed;
        std::string why;
        CHECK_MSG(Controls::Tuning::ParseFlag(huge, parsed, why) && parsed.Any(), std::string("'") + huge + "' is a number and parses");
        Controls::Tuning applied;
        const std::string key = std::string(huge).substr(0, std::string(huge).find('='));
        CHECK_MSG(!Controls::Tuning::FromFlag(parsed, rules, applied, why) && why.find(key) != std::string::npos, std::string("'") + huge + "' is refused by the grid, naming it: " + why);
    }
    for (const double bad_size : {1.25, 0.3, 1.5, 0.0, -1.0, 10.0, 1e30, -1e30, 1e308, -1e308, 214748364.7, 214748364.8, -214748364.8, 4294967296.0}) {
        Controls::FlagItems f;
        f.size = bad_size;
        CHECK_MSG(!Controls::Tuning::FromFlag(f, rules, tuning, error) && error.find("size") != std::string::npos, "size " + Num(bad_size) + " is refused: " + error);
    }
    for (const double good_size : {0.4, 0.5, 1.0, 1.4}) {
        Controls::FlagItems f;
        f.size = good_size;
        CHECK_MSG(Controls::Tuning::FromFlag(f, rules, tuning, error) && Near(tuning.Size(rules), good_size, 1e-12), "size " + Num(good_size));
    }
    for (const double bad_opacity : {1.1, 0.1, 1.9, 2.0, 0.0, 1e30, -1e30, 1e308, -1e308, 429496729.4, 429496729.6, -429496729.6, 4294967296.0}) {
        Controls::FlagItems f;
        f.opacity = bad_opacity;
        CHECK_MSG(!Controls::Tuning::FromFlag(f, rules, tuning, error) && error.find("opacity") != std::string::npos, "opacity " + Num(bad_opacity) + " is refused: " + error);
    }
    for (const double good_opacity : {0.2, 0.6, 1.0, 1.8}) {
        Controls::FlagItems f;
        f.opacity = good_opacity;
        CHECK_MSG(Controls::Tuning::FromFlag(f, rules, tuning, error) && Near(tuning.Opacity(rules), good_opacity, 1e-12), "opacity " + Num(good_opacity));
    }
    Controls::FlagItems moved;
    moved.move[0] = glm::dvec2(3.14159, -2.718);
    CHECK(Controls::Tuning::FromFlag(moved, rules, tuning, error) && tuning.move[0].x == 3.1 && tuning.move[0].y == -2.7);
    // A size that is the look's own is no size; under another look it is one.
    Controls::FlagItems same;
    same.size = 0.7;
    CHECK(Controls::Tuning::FromFlag(same, rules, tuning, error) && !tuning.sizeSteps && tuning.IsDefault(rules));
    same.preset = Controls::Preset::Mr;
    CHECK(Controls::Tuning::FromFlag(same, rules, tuning, error) && tuning.sizeSteps && *tuning.sizeSteps == 7 && tuning.Size(rules) == 0.7);
    // The arrangement is carried, applies to nothing a player saves, and a tuning that only asks for one is still the default.
    Controls::FlagItems asksGrouped;
    asksGrouped.arrows = Controls::Arrows::Grouped;
    CHECK(Controls::Tuning::FromFlag(asksGrouped, rules, tuning, error) && tuning.arrows == Controls::Arrows::Grouped && tuning.ArrowsIn(rules) == Controls::Arrows::Grouped &&
          tuning.IsDefault(rules) && tuning.ToJson(rules) == "{}");
    CHECK(Controls::Tuning::FromFlag(Controls::FlagItems{}, rules, tuning, error) && !tuning.arrows && tuning.ArrowsIn(rules) == Controls::Arrows::Split);
    // The tuning FromFlag makes carries the flag's own keys and nothing else: it takes no saved tuning to merge with. That the flag REPLACES what was
    // saved, rather than merging with it, is the caller's, and the layer's case pins it (planning step 187, AFlagTuningReplacesNeverMergesAndWritesNothing); what is
    // held here is that one key gives exactly one key.
    Controls::FlagItems onlyOpacity;
    onlyOpacity.opacity = 0.6;
    Controls::Tuning fromFlag;
    CHECK(Controls::Tuning::FromFlag(onlyOpacity, rules, fromFlag, error) && !fromFlag.sizeSteps && fromFlag.opacitySteps && *fromFlag.opacitySteps == 3 && !fromFlag.preset && !fromFlag.arrows &&
          std::all_of(fromFlag.move.begin(), fromFlag.move.end(), [](const glm::dvec2& m) { return m == glm::dvec2(0.0); }));
}

void APresetKeyIsParsedAndAnUnknownOneIsRefused() {
    const Controls::Rules rules = LoadModern();
    std::string error;
    for (const Controls::Preset preset : Controls::kPresets) {
        Controls::FlagItems one;
        CHECK_MSG(Controls::Tuning::ParseFlag(std::string("preset=") + Controls::NameOf(preset), one, error) && one.preset == preset, std::string("preset=") + Controls::NameOf(preset));
        Controls::Tuning tuning;
        CHECK(Controls::Tuning::FromFlag(one, rules, tuning, error) && tuning.preset == preset);
        const Controls::Policy policy = Controls::ResolvePolicy(std::string("modern,preset=") + Controls::NameOf(preset), false, false, false);
        CHECK_MSG(policy.valid && policy.flag.preset == preset, std::string("the policy carries preset=") + Controls::NameOf(preset));
    }

    // An unknown preset, or a known one in another case, is refused whether it comes alone or with the rest.
    for (const char* text : {"preset=huge", "preset=UNIFORM", "preset=", "size=1,preset=big", "preset=uniform,preset=uniform"}) {
        Controls::FlagItems refused;
        CHECK_MSG(!Controls::Tuning::ParseFlag(text, refused, error), std::string("'") + text + "' is refused");
        const Controls::Policy policy = Controls::ResolvePolicy(std::string("modern,") + text, false, false, false);
        CHECK_MSG(!policy.valid && !policy.available, std::string("--controls modern,") + text + " is refused by the policy");
    }
    // The look is the flag's: it rides the policy, and the tuning applied from it names it.
    const Controls::Policy balanced = Controls::ResolvePolicy(std::string("modern,preset=balanced,size=1.2"), false, false, false);
    Controls::Tuning tuning;
    CHECK(balanced.valid && balanced.flag.preset == Controls::Preset::Balanced && Controls::Tuning::FromFlag(balanced.flag, rules, tuning, error));
    CHECK(tuning.PresetIn(rules) == Controls::Preset::Balanced && tuning.Size(rules) == 1.2);
    CHECK(Controls::Tuning::FromFlag(Controls::ResolvePolicy(std::string("modern,preset=balanced"), false, false, false).flag, rules, tuning, error) && tuning.Size(rules) == 0.8);
}

void TheClassicStyleIsTheClassicFunctions() {
    const Hud::Rules hud = Load();
    const Controls::Rules rules = LoadModern();
    Controls::Tuning odd;
    odd.sizeSteps = 12;
    odd.move[0] = glm::dvec2(30.0, -10.0);
    long differ = 0;
    for (const glm::dvec2& view : kViews) {
        for (const double age : kAges) {
            const Controls::Layout layout = Controls::LayOut(Controls::Style::Classic, rules, hud, odd, view, Controls::Insets{5.0, 5.0, 5.0, 5.0}, age);
            if (layout.style != Controls::Style::Classic) ++differ;
            for (const Controls::Id id : kWalks) {
                const Controls::Frame want = Controls::ClassicWalkFrame(hud, id, view, age);
                const Controls::Frame& got = layout.At(id);
                if (!SameBits(got.rect, want.rect) || !SameBits(got.hit, want.hit)) ++differ;
            }
            for (const Controls::Id id : kCorners) {
                const Controls::Frame want = Controls::ClassicCornerFrame(Controls::ClassicPlace(hud, id, view));
                const Controls::Frame& got = layout.At(id);
                if (!SameBits(got.rect, want.rect) || !SameBits(got.hit, want.hit)) ++differ;
            }
        }
    }
    CHECK_MSG(differ == 0, "the classic layout differs from the Classic functions in " + std::to_string(differ) + " frames");
    // FollowDrawn moves a hit with its picture, whatever its shape.
    const Controls::Frame disc = Controls::ClassicWalkFrame(hud, Controls::Id::Left, kView169, 5000.0);
    const Hud::Rect moved{disc.rect.min + glm::dvec2(7.0, -3.0), disc.rect.size};
    const Controls::Frame follows = Controls::FollowDrawn(disc, moved);
    CHECK(SameBits(follows.rect, moved) && follows.hit.centre.x == disc.hit.centre.x + 7.0 && follows.hit.centre.y == disc.hit.centre.y - 3.0 && follows.hit.radius == disc.hit.radius);
    const Controls::Frame closed = Controls::ClassicCornerFrame(Controls::ClassicPlace(hud, Controls::Id::Reset, kView169));
    const Controls::Frame closedFollows = Controls::FollowDrawn(closed, Hud::Rect{closed.rect.min + glm::dvec2(-4.0, 9.0), closed.rect.size});
    CHECK(closedFollows.hit.rect.min.x == closed.hit.rect.min.x - 4.0 && closedFollows.hit.rect.min.y == closed.hit.rect.min.y + 9.0 && closedFollows.hit.rect.size == closed.hit.rect.size);
}

// ---- the tuning's keys, spelt in one place ---------------------------------------------------------------------------

void TheTuningKeysAreOnePlaceAndTheMembersSpliceIntoAnObject() {
    namespace Key = Controls::TuningKey;
    const Controls::Rules rules = LoadModern();
    // The eight spellings, written out once here: the flag's parser, the saved file's reader and its writer take them from the constants, so a
    // key that changes changes in all three and a test that still says the old one fails.
    CHECK(std::string(Key::kPreset) == "preset" && std::string(Key::kSize) == "size" && std::string(Key::kOpacity) == "opacity" && std::string(Key::kMove) == "move");
    CHECK(std::string(Key::kArrows) == "arrows");
    CHECK(std::string(Key::kLayout) == "layout" && std::string(Key::kMode) == "mode" && std::string(Key::kVersion) == "version" && std::string(Key::kControls) == "controls");
    // The flag takes the keys by those spellings, and its usage text (which main prints) names them.
    Controls::FlagItems items;
    std::string error;
    CHECK(Controls::Tuning::ParseFlag(std::string(Key::kPreset) + "=mr," + Key::kSize + "=1.2," + Key::kOpacity + "=0.6", items, error) && items.preset == Controls::Preset::Mr &&
          items.size == 1.2 && items.opacity == 0.6);
    for (const char* key : {Key::kPreset, Key::kArrows, Key::kSize, Key::kOpacity}) {
        CHECK_MSG(std::string(Controls::FlagUsage()).find(std::string(key) + "=") != std::string::npos, std::string("the usage names ") + key);
    }
    // The saved file's reader and writer take them too: a tuning of every kind, written and read back by the constants' spellings.
    Controls::Tuning tuning;
    tuning.sizeSteps = 12;
    tuning.opacitySteps = 3;
    tuning.move[0] = glm::dvec2(2.0, -3.5);
    tuning.move[3] = glm::dvec2(-10.0, 5.0);
    const std::string members = tuning.ToJsonMembers(rules);
    CHECK_MSG(members == std::string("\"") + Key::kSize + "\":1.2,\"" + Key::kOpacity + "\":0.6,\"" + Key::kMove + "\":{\"walk_left\":[2,-3.5],\"pause\":[-10,5]}",
              "the members: " + members);
    CHECK_MSG(tuning.ToJson(rules) == "{" + members + "}", "ToJson is the members in braces: " + tuning.ToJson(rules));
    // A default tuning has no members, and its object is empty.
    CHECK(kNoTuning.ToJsonMembers(rules).empty() && kNoTuning.ToJson(rules) == "{}");
    // The members are not closed, so a writer with members of its own splices them without stripping braces: the saved file's `controls` object
    // (the mode and the layout number beside the tuning) and the file around it, read back as the same tuning.
    for (const Controls::Tuning& t : {kNoTuning, tuning}) {
        const std::string inner = t.ToJsonMembers(rules);
        const std::string object = std::string("{\"") + Key::kMode + "\":\"modern\",\"" + Key::kLayout + "\":" + std::to_string(Controls::kLayoutVersion) + (inner.empty() ? "" : "," + inner) + "}";
        const std::string file = std::string("{\"") + Key::kVersion + "\":1,\"" + Key::kControls + "\":" + object + "}";
        const Supersonic::Json::Value root = ParseText(file);
        CHECK_MSG(root.IsObject() && root[Key::kControls].IsObject(), "the spliced file parses as an object holding the controls: " + file);
        const Controls::Tuning back = Controls::Tuning::ReadFrom(root[Key::kControls], rules);
        CHECK_MSG(SameTuning(back, t.Clamped(rules)), "the spliced controls read back as the tuning: " + object);
        CHECK_MSG(back.ToJsonMembers(rules) == inner, "and write the same members again: " + back.ToJsonMembers(rules));
    }
}

// ---- what a policy carries beside what the command line said -----------------------------------------------------------------

bool NoInsets(const Controls::Insets& insets) {
    return insets.left == 0.0 && insets.top == 0.0 && insets.right == 0.0 && insets.bottom == 0.0;
}

void ThePolicyCarriesTheSeamsTheCommandLineHasNoWordFor() {
    // Classic, nothing: a bare layer's policy, which every suite uses, meets none of the four.
    const Controls::Policy braces{};
    const Controls::Policy plainDefault;
    for (const Controls::Policy* bare : {&braces, &plainDefault}) {
        CHECK(bare->valid && bare->error.empty() && !bare->available && !bare->forced && !bare->saveAllowed && !bare->flag.Any());
        CHECK_MSG(!bare->classicClaims && NoInsets(bare->testInsets) && bare->artDir.empty() && !bare->withoutOriginalArt && !bare->tuning,
                  "a default policy has no test seam set");
    }
    // The command line has no word for them: whatever it says, and whatever kind of run it is, ResolvePolicy leaves the four where they were.
    long resolved = 0;
    const std::vector<std::optional<std::string>> flags = {std::nullopt, std::string("classic"), std::string("modern"),
                                                           std::string("modern,preset=mr,size=1.2,opacity=0.6,walk_left=1:2")};
    for (const std::optional<std::string>& flag : flags) {
        for (int kind = 0; kind < 8; ++kind) {
            const Controls::Policy policy = Controls::ResolvePolicy(flag, (kind & 1) != 0, (kind & 2) != 0, (kind & 4) != 0);
            // `classic` takes a developer's run and a player's the same way, and a flag with keys on a developer's run is still a flag.
            CHECK_MSG(policy.valid, flag.value_or("(none)") + " resolves: " + policy.error);
            CHECK_MSG(!policy.classicClaims && NoInsets(policy.testInsets) && policy.artDir.empty() && !policy.withoutOriginalArt && !policy.tuning,
                      "--controls " + flag.value_or("(none)") + " leaves the test seams alone, kind " + std::to_string(kind));
            ++resolved;
        }
    }
    CHECK_EQ(static_cast<int>(resolved), 32);
    // A test sets them by hand, on the policy it resolved, and they ride the copy the layer is given.
    Controls::Policy given = Controls::ResolvePolicy(std::string("modern"), false, false, false);
    given.classicClaims = true;
    given.testInsets = Controls::Insets{3.0, 4.0, 5.0, 6.0};
    given.artDir = "some/where";
    given.withoutOriginalArt = true;
    Controls::Tuning injected;
    injected.sizeSteps = 12;
    given.tuning = injected;
    const Controls::Policy copy = given;
    CHECK(copy.classicClaims && copy.testInsets.left == 3.0 && copy.testInsets.top == 4.0 && copy.testInsets.right == 5.0 && copy.testInsets.bottom == 6.0);
    CHECK(copy.artDir == "some/where" && copy.withoutOriginalArt && copy.tuning && copy.tuning->sizeSteps && *copy.tuning->sizeSteps == 12);
    // And they are not what decides the style: the same policy without them plays the same.
    Controls::Policy plain = given;
    plain.classicClaims = false;
    plain.testInsets = Controls::Insets{};
    plain.artDir.clear();
    plain.withoutOriginalArt = false;
    plain.tuning.reset();
    const std::array<std::optional<Controls::Style>, 3> saveds = {std::nullopt, Controls::Style::Classic, Controls::Style::Modern};
    for (const std::optional<Controls::Style>& saved : saveds) {
        for (const Controls::Style shipped : {Controls::Style::Classic, Controls::Style::Modern}) {
            CHECK(Controls::ResolveStyle(given, saved, shipped) == Controls::ResolveStyle(plain, saved, shipped));
        }
    }
}

// ---- the pictures a modern run draws (planning step 184) ------------------------------------------------------------------------------

// A name controls.json may give a picture is ONE file in the folder of its tier. The rows are written out, with the reason each is refused,
// because a name that climbs out of the folder is a file written (the generated tier) or read (the others) somewhere the data did not mean.
void ABareFileNameIsOneFileInOneFolder() {
    const std::vector<std::string> bare = {"walk_left.png", "modern_walk_left.png", "a.png", "x", "restart_level_button.png",
                                           "dots.in.name.png", ".hidden", "-dash", "UPPER.PNG", "with space.png", "(1).png"};
    for (const std::string& name : bare) CHECK_MSG(Controls::IsBareFileName(name), "'" + name + "' is a bare file name");
    struct Refused {
        std::string name;
        const char* why;
    };
    const std::vector<Refused> refused = {
        {"", "empty"},
        {".", "the folder itself"},
        {"..", "the folder above"},
        {"../x.png", "climbs out"},
        {"..\\x.png", "climbs out, the other separator"},
        {"a/../x.png", "climbs out in the middle"},
        {"sub/x.png", "a folder"},
        {"sub\\x.png", "a folder, the other separator"},
        {"/x.png", "rooted"},
        {"\\x.png", "rooted, the other separator"},
        {"\\\\server\\share\\x.png", "a network path"},
        {"C:/x.png", "a drive and a folder"},
        {"C:\\x.png", "a drive and a folder, the other separator"},
        {"C:x.png", "a drive's own working directory"},
        {"x.png:stream", "an NTFS stream"},
        {"a..b.png", "a pair of dots anywhere"},
        {"x.png ", "Windows reads a trailing space as nothing"},
        {"x.png.", "Windows reads a trailing dot as nothing"},
        {std::string("x\0y.png", 7), "a NUL"},
        {"x\ty.png", "a control character"},
        {"x\ny.png", "a line break"},
        {std::string("x\x7f.png"), "DEL"},
        {"x<y.png", "a character Windows refuses"},
        {"CON", "a Windows device name"},
        {"nul.txt", "a Windows device name with an extension"},
    };
    for (const Refused& row : refused) CHECK_MSG(!Controls::IsBareFileName(row.name), std::string("a name that is ") + row.why + " is refused");
    // The shipped names are bare in every tier (LoadRules would have refused the file), and the generated ones are the layer's write targets.
    Controls::Rules rules;
    std::string error;
    CHECK_MSG(Controls::LoadRules(kControls, rules, error), "controls.json reads: " + error);
    int checked = 0;
    for (std::size_t tier = 0; tier < Controls::kArtTierCount; ++tier) {
        for (const Controls::Id id : Controls::kClaimOrder) {
            const std::optional<Controls::ArtEntry>& entry = rules.art[tier][static_cast<std::size_t>(id)];
            if (!entry) continue;
            ++checked;
            CHECK_MSG(Controls::IsBareFileName(entry->file), "art file '" + entry->file + "' is bare");
        }
    }
    CHECK_EQ(checked, 13); // five of Magic Rampage's, three of the original's, five of the generated
}

// The sizes the Magic Rampage tool recorded, and what a picture must measure.
void TheArtManifestGivesTheSizesAPictureMustHave() {
    struct Clean {
        static void Run() {
            std::error_code ec;
            std::filesystem::remove_all(ScratchDir(), ec);
        }
        Clean() { Run(); }
        ~Clean() { Run(); }
    } clean;
    const auto write = [](const char* name, const std::string& text) {
        const std::filesystem::path path = ScratchDir() / name;
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file << text;
        return path.string();
    };
    Controls::ArtManifest manifest;
    std::string why;
    // A folder dropped in by hand has no manifest: nothing is wrong, and the default size is what is held to.
    CHECK_MSG(Controls::ReadArtManifest((ScratchDir() / "no-manifest.json").string(), manifest, why) && manifest.sizes.empty() && why.empty(),
              "an absent manifest is no manifest and no error");
    CHECK(Controls::ExpectedArtSize(manifest, "walk_left.png", 128) == glm::ivec2(128, 128));

    // The tool's own shape (tools/art/make_mr_controls.py): `outputs.<file>.size` = [w, h], beside members that are not read.
    const std::string whole = R"({"body_rect": [14, 14, 114, 114], "canvas": [128, 128], "outputs": {
        "walk_left.png": {"size": [128, 128], "sha256": "x", "body": [14, 14, 114, 114]},
        "restart.png": {"size": [64, 96]},
        "pause.png": {"sha256": "no size recorded"},
        "not-an-entry.png": 7}})";
    CHECK_MSG(Controls::ReadArtManifest(write("whole.json", whole), manifest, why), "a manifest in the tool's shape reads: " + why);
    CHECK_EQ(static_cast<int>(manifest.sizes.size()), 2); // walk_left and restart: an entry with no size, or not an entry, names none
    CHECK(Controls::ExpectedArtSize(manifest, "walk_left.png", 128) == glm::ivec2(128, 128));
    CHECK_MSG(Controls::ExpectedArtSize(manifest, "restart.png", 128) == glm::ivec2(64, 96), "the manifest's size wins over the default");
    CHECK_MSG(Controls::ExpectedArtSize(manifest, "pause.png", 128) == glm::ivec2(128, 128), "a picture it records no size for is held to the default");
    CHECK_MSG(Controls::ExpectedArtSize(manifest, "clear_portals.png", 72) == glm::ivec2(72, 72),
              "and the default is the caller's, layout.box_px, not a literal here");
    CHECK_MSG(Controls::ReadArtManifest(write("no-outputs.json", R"({"canvas": [128, 128]})"), manifest, why) && manifest.sizes.empty(),
              "a manifest with no outputs names no size");

    // A manifest that is there and cannot be trusted is refused whole, naming what is wrong, with nothing half-read left behind.
    struct Bad {
        const char* what;
        std::string text;
        const char* mentions;
    };
    const std::vector<Bad> bad = {
        {"not JSON", "{\"outputs\": ", "bad.json"},
        {"not an object", "[]", "not an object"},
        {"one number", R"({"outputs": {"a.png": {"size": [128]}}})", "a.png"},
        {"three numbers", R"({"outputs": {"a.png": {"size": [128, 128, 4]}}})", "a.png"},
        {"a string", R"({"outputs": {"a.png": {"size": [128, "x"]}}})", "a.png"},
        {"zero", R"({"outputs": {"a.png": {"size": [0, 128]}}})", "a.png"},
        {"negative", R"({"outputs": {"a.png": {"size": [-128, 128]}}})", "a.png"},
        {"fractional", R"({"outputs": {"a.png": {"size": [128.5, 128]}}})", "a.png"},
        {"huge", R"({"outputs": {"a.png": {"size": [128, 99999]}}})", "a.png"},
        {"an object", R"({"outputs": {"a.png": {"size": {"w": 128, "h": 128}}}})", "a.png"},
    };
    for (const Bad& row : bad) {
        Controls::ArtManifest out;
        out.sizes["stale.png"] = glm::ivec2(1, 1);
        std::string message;
        const bool read = Controls::ReadArtManifest(write("bad.json", row.text), out, message);
        CHECK_MSG(!read && out.sizes.empty() && message.find(row.mentions) != std::string::npos,
                  std::string(row.what) + " is refused, naming it, with nothing kept: " + message);
    }
}

// The INFO line and the WARN a resolution says, from the five tiers it found.
void TheArtLineSaysWhereEachPictureCameFrom() {
    using Tier = Controls::ArtTier;
    CHECK(std::string(Controls::NameOf(Controls::ArtSource(Tier::MagicRampage))) == "mr");
    CHECK(std::string(Controls::NameOf(Controls::ArtSource(Tier::Original))) == "original");
    CHECK(std::string(Controls::NameOf(Controls::ArtSource(Tier::Generated))) == "generated");
    CHECK(std::string(Controls::NameOf(Controls::ArtSource{})) == "flat");

    Controls::ArtSources all;
    all.fill(Tier::MagicRampage);
    CHECK_MSG(Controls::ArtLine(all) == "modern controls art: walk_left=mr walk_right=mr restart=mr pause=mr clear_portals=mr", Controls::ArtLine(all));
    CHECK(!Controls::ArtMixed(all));
    all.fill(Tier::Generated);
    CHECK(Controls::ArtLine(all) == "modern controls art: walk_left=generated walk_right=generated restart=generated pause=generated clear_portals=generated");
    CHECK(!Controls::ArtMixed(all));
    all.fill(Controls::ArtSource{});
    CHECK(Controls::ArtLine(all) == "modern controls art: walk_left=flat walk_right=flat restart=flat pause=flat clear_portals=flat");
    CHECK_MSG(!Controls::ArtMixed(all), "five flat quads are one look");

    // No Magic Rampage picture on a machine that has the original's: the arrows drawn, the corners the original's own - a mixed look.
    Controls::ArtSources fallback;
    fallback.fill(Tier::Original);
    fallback[static_cast<std::size_t>(Controls::Id::Left)] = Tier::Generated;
    fallback[static_cast<std::size_t>(Controls::Id::Right)] = Tier::Generated;
    CHECK_MSG(Controls::ArtLine(fallback) ==
                  "modern controls art: walk_left=generated walk_right=generated restart=original pause=original clear_portals=original",
              Controls::ArtLine(fallback));
    CHECK(Controls::ArtMixed(fallback));

    // One control of five from another tier is a mixed look, whichever it is.
    for (const Controls::Id odd : Controls::kClaimOrder) {
        Controls::ArtSources one;
        one.fill(Tier::MagicRampage);
        one[static_cast<std::size_t>(odd)] = Controls::ArtSource{};
        CHECK_MSG(Controls::ArtMixed(one), std::string(Controls::KeyOf(odd)) + " drawn flat among four Magic Rampage pictures is mixed");
    }
}

// The window's safe area is in WINDOW PIXELS (Supersonic::SafeArea::Get) and the layout's insets are in design units (planning step 183): one
// pixel is the view's height over the window's, on both axes, as the pointer is converted. Literals written out: 256 units over 720 px is
// 0.35555...
void TheSafeAreaInPixelsBecomesInsetsInUnitsByThePointersFactor() {
    constexpr double kUnitsPerPx = 256.0 / 720.0;
    const Controls::Insets notch = Controls::InsetsFromWindowPx(80.0, 36.0, 40.0, 72.0, 720.0, 256.0);
    CHECK_MSG(std::fabs(notch.left - 80.0 * kUnitsPerPx) < 1e-12 && std::fabs(notch.left - 28.44444444444444) < 1e-9, "80 px of a 720 px window are 28.444 units: " + std::to_string(notch.left));
    CHECK_MSG(std::fabs(notch.top - 12.8) < 1e-12, "36 px are 12.8 units, the same factor along the other axis: " + std::to_string(notch.top));
    CHECK_MSG(std::fabs(notch.right - 40.0 * kUnitsPerPx) < 1e-12, "40 px at the right: " + std::to_string(notch.right));
    CHECK_MSG(std::fabs(notch.bottom - 25.6) < 1e-12, "72 px at the bottom are 25.6 units: " + std::to_string(notch.bottom));

    // The same fraction of any window is the same units: a window twice as tall covers twice the pixels for the same notch.
    const Controls::Insets twice = Controls::InsetsFromWindowPx(160.0, 72.0, 80.0, 144.0, 1440.0, 256.0);
    CHECK_MSG(std::fabs(twice.left - notch.left) < 1e-9 && std::fabs(twice.top - notch.top) < 1e-9 && std::fabs(twice.right - notch.right) < 1e-9 &&
                  std::fabs(twice.bottom - notch.bottom) < 1e-9,
              "a notch twice the pixels in a window twice as tall is the same in units");
    // And it does not depend on the window's width, which the view is stretched to: the factor is the HEIGHTS'.
    const Controls::Insets wide = Controls::InsetsFromWindowPx(80.0, 36.0, 40.0, 72.0, 720.0, 256.0);
    CHECK(wide.left == notch.left && wide.bottom == notch.bottom);

    // Nothing is a notch of nothing: all zero in, all zero out (a desktop), whatever the window.
    for (const double height : {720.0, 1080.0, 1440.0, 100.0}) {
        const Controls::Insets none = Controls::InsetsFromWindowPx(0.0, 0.0, 0.0, 0.0, height, 256.0);
        CHECK_MSG(NoInsets(none), "no notch in a " + std::to_string(height) + " px window is no inset");
    }
    // A window with no height has no safe area to speak of; a side that is negative or not a number is zero, the others stand.
    CHECK(NoInsets(Controls::InsetsFromWindowPx(80.0, 36.0, 40.0, 72.0, 0.0, 256.0)));
    CHECK(NoInsets(Controls::InsetsFromWindowPx(80.0, 36.0, 40.0, 72.0, -720.0, 256.0)));
    CHECK(NoInsets(Controls::InsetsFromWindowPx(80.0, 36.0, 40.0, 72.0, std::numeric_limits<double>::quiet_NaN(), 256.0)));
    CHECK(NoInsets(Controls::InsetsFromWindowPx(80.0, 36.0, 40.0, 72.0, 720.0, 0.0)));
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    const Controls::Insets mixed = Controls::InsetsFromWindowPx(-5.0, nan, inf, 72.0, 720.0, 256.0);
    CHECK_MSG(mixed.left == 0.0 && mixed.top == 0.0 && mixed.right == 0.0 && std::fabs(mixed.bottom - 25.6) < 1e-12,
              "a negative, a NaN and an infinite side are zero; the valid one stands: " + std::to_string(mixed.bottom));

    // And the layout moves by them, which is the whole of the wiring: the arrows' box is measured from the safe rectangle's edges. A notch at
    // the left and a home bar at the bottom of a 1280 x 720 window put the left arrow where table 1.2 puts it plus the inset.
    Controls::Rules rules;
    std::string error;
    const std::string file = PortDataDir() + "/controls.json";
    if (!Controls::LoadRules(file, rules, error)) {
        CHECK_MSG(false, "controls.json reads: " + error);
        return;
    }
    const glm::dvec2 view(256.0 * 1280.0 / 720.0, 256.0);
    const Controls::Tuning base;
    const Controls::Insets device = Controls::InsetsFromWindowPx(80.0, 0.0, 0.0, 34.0, 720.0, 256.0);
    const std::array<Hud::Rect, 5> flat = Controls::Boxes(rules, base, view, Controls::Insets{});
    const std::array<Hud::Rect, 5> moved = Controls::Boxes(rules, base, view, device);
    const std::size_t left = static_cast<std::size_t>(Controls::Id::Left);
    CHECK_MSG(std::fabs(moved[left].min.x - (flat[left].min.x + 80.0 * kUnitsPerPx)) < 1e-9, "the left arrow's box is 80 px (28.44 units) further in: " + std::to_string(moved[left].min.x));
    CHECK_MSG(std::fabs(moved[left].min.y - (flat[left].min.y - 34.0 * kUnitsPerPx)) < 1e-9, "and 34 px higher: " + std::to_string(moved[left].min.y));
    // The top-right corner buttons are not under the notch at the left: restart does not move sideways.
    const std::size_t reset = static_cast<std::size_t>(Controls::Id::Reset);
    CHECK_MSG(std::fabs(moved[reset].min.x - flat[reset].min.x) < 1e-9, "restart, at the right, stays where it was");
}

// ---- the names a picture may carry, hardened (planning step 185) -------------------------------------------------------------------------------

// Windows' own device names and the characters it refuses in a name are not bare file names either: a file called CON is the console, and
// the generated tier WRITES to the name it is given. The names below are the ones Windows documents as reserved (CON, PRN, AUX, NUL, COM1 to
// COM9, LPT1 to LPT9, with or without an extension, in any case); nothing is asserted of names it leaves open (COM0, COM10).
void TheNamesWindowsKeepAreNotBare() {
    const std::vector<std::string> refused = {
        std::string("x\x7f.png"), std::string("\x7f"),  "a<b.png", "a>b.png", "a\"b.png", "a|b.png", "a?b.png", "a*b.png", "*.png", "x.png?",
        "CON", "con", "Con.png", "con.png", "PRN", "prn.txt", "AUX", "aux.dat", "NUL", "nul", "NUL.txt", "COM1", "com1.png", "COM9", "com9.x.y",
        "LPT1", "lpt1.png", "LPT9", "Lpt9.PNG", "CONIN$", "conout$.txt",
    };
    for (const std::string& name : refused) CHECK_MSG(!Controls::IsBareFileName(name), "'" + name + "' is refused (a character Windows refuses, or one of its device names)");
    // The look-alikes are plain names: a device name is the WHOLE stem, and the data's own are never among them.
    const std::vector<std::string> bare = {"console.png", "communism.png", "com.png", "lpt.png", "nulls.png", "prn_x.png", "auxiliary.png",
                                           "icon.png",    "context.png",   "co.png",  "modern_gear.png", "options_gear.png"};
    for (const std::string& name : bare) CHECK_MSG(Controls::IsBareFileName(name), "'" + name + "' is a bare file name");
}

// The generated tier writes its five files into ONE folder: two controls that name one file would overwrite each other, and each run would
// draw the later glyph on both with nothing said. Windows folds case, so "A.png" and "a.png" are one file.
void TheFiveGeneratedNamesDiffer() {
    namespace Json = Supersonic::Json;
    const Controls::Rules rules = LoadModern();
    std::vector<std::string> names;
    for (const Controls::Id id : Controls::kClaimOrder) {
        const auto& entry = rules.art[static_cast<std::size_t>(Controls::ArtTier::Generated)][static_cast<std::size_t>(id)];
        CHECK(entry.has_value());
        if (!entry) continue;
        std::string lower = entry->file;
        for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        names.push_back(lower);
    }
    CHECK_EQ(names.size(), 5u);
    std::sort(names.begin(), names.end());
    CHECK_MSG(std::adjacent_find(names.begin(), names.end()) == names.end(), "the shipped five generated files differ, with case folded");

    const Json::Value root = ReadControls();
    struct Row {
        const char* second;
        const char* name;
    };
    // Each later control given the file of an earlier one, spelt as it is and in another case: refused, naming both controls.
    for (const Row& row : {Row{"walk_right", "modern_walk_left.png"}, Row{"pause", "modern_restart.png"}, Row{"clear_portals", "MODERN_WALK_LEFT.PNG"},
                           Row{"restart", "Modern_Pause.png"}}) {
        const Json::Value edited = Edit(root, Path{"art", "generated", row.second, "file"}, 0, Json::Value(std::string(row.name)));
        Controls::Rules r;
        std::string why;
        const bool loaded = LoadScratch("same_name.json", Write(edited), r, why);
        CHECK_MSG(!loaded && why.find(row.second) != std::string::npos && why.find("same file") != std::string::npos,
                  std::string(row.second) + " given " + row.name + " is refused, naming it and the other: " + why);
    }
    // The other tiers may share a name between controls (they only READ): Magic Rampage's two arrows given one file load.
    {
        Controls::Rules r;
        std::string why;
        const Json::Value edited = Edit(root, Path{"art", "mr", "walk_right", "file"}, 0, Json::Value(std::string("walk_left.png")));
        CHECK_MSG(LoadScratch("mr_same.json", Write(edited), r, why), "two controls reading one Magic Rampage file load (it is only read): " + why);
    }
}

// A mix of pictures is a WARN only where a Magic Rampage picture is part of it (a folder that is half there); the ordinary mix of a machine
// without the folder - the arrows drawn beside the original's corner sprites - is an INFO (the owner's question 2 of step 184, as recommended).
void ArtHalfDressedNeedsAMagicRampagePictureInAMix() {
    using Tier = Controls::ArtTier;
    Controls::ArtSources all;
    for (const Tier tier : {Tier::MagicRampage, Tier::Original, Tier::Generated}) {
        all.fill(tier);
        CHECK_MSG(!Controls::ArtHalfDressed(all), std::string("five of ") + Controls::NameOf(tier) + " is not a mix");
    }
    all.fill(Controls::ArtSource{});
    CHECK(!Controls::ArtHalfDressed(all));

    // The no-folder machine: the arrows drawn, the corners the original's: mixed, and no Magic Rampage in it.
    Controls::ArtSources ordinary;
    ordinary.fill(Tier::Original);
    ordinary[static_cast<std::size_t>(Controls::Id::Left)] = Tier::Generated;
    ordinary[static_cast<std::size_t>(Controls::Id::Right)] = Tier::Generated;
    CHECK(Controls::ArtMixed(ordinary) && !Controls::ArtHalfDressed(ordinary));
    // Or with nothing to write: the arrows flat beside the original's corners.
    ordinary[static_cast<std::size_t>(Controls::Id::Left)] = Controls::ArtSource{};
    CHECK(Controls::ArtMixed(ordinary) && !Controls::ArtHalfDressed(ordinary));

    // A Magic Rampage picture IN a mix, whichever it is and whatever it is mixed with.
    for (const Controls::Id odd : Controls::kClaimOrder) {
        for (const Tier other : {Tier::Original, Tier::Generated}) {
            Controls::ArtSources half;
            half.fill(other);
            half[static_cast<std::size_t>(odd)] = Tier::MagicRampage;
            CHECK_MSG(Controls::ArtHalfDressed(half), std::string(Controls::KeyOf(odd)) + " from Magic Rampage among five of " + Controls::NameOf(other) + " is a half-dressed folder");
        }
        Controls::ArtSources missing;
        missing.fill(Tier::MagicRampage);
        missing[static_cast<std::size_t>(odd)] = Tier::Generated;
        CHECK_MSG(Controls::ArtHalfDressed(missing), std::string(Controls::KeyOf(odd)) + " refused out of five Magic Rampage pictures is a half-dressed folder");
    }
}

// ---- the saved choice (planning step 185) ----------------------------------------------------------------------------------------------------------

// The store cases' scratch goes when each case ends (and any that a run which crashed left goes before it starts): there were nine directories, each
// holding a controls.json, left in %TEMP%\supersonic-test-mp-controls\store by every run of this suite (planning step 185's LEFT FOR LATER).
struct StoreClean {
    static void Run() {
        std::error_code ec;
        const std::filesystem::path root = ScratchDir(); // the one name the suite's scratch has (it was spelt twice)
        std::filesystem::remove_all(root / "store", ec);
        std::filesystem::remove(root, ec); // only if nothing else of the suite is in it
    }
    StoreClean() { Run(); }
    ~StoreClean() { Run(); }
    StoreClean(const StoreClean&) = delete;
    StoreClean& operator=(const StoreClean&) = delete;
};

// A fresh directory under the suite's scratch for each case, emptied first.
std::filesystem::path StoreDir(const char* name) {
    const std::filesystem::path dir = ScratchDir() / "store" / name;
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);
    return dir;
}

std::string StoreRead(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

void StoreWrite(const std::filesystem::path& file, const std::string& text) {
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    out << text;
}

// The directory's entries, by name, sorted: what a store left behind.
std::vector<std::string> StoreEntries(const std::filesystem::path& dir) {
    std::vector<std::string> names;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) names.push_back(entry.path().filename().string());
    std::sort(names.begin(), names.end());
    return names;
}

void ControlsStoreRoundTrips() {
    const StoreClean clean;
    const Controls::Rules rules = LoadModern();
    const std::filesystem::path dir = StoreDir("round");
    const std::filesystem::path file = dir / Controls::kStoreFileName;
    CHECK_MSG(std::string(Controls::kStoreFileName) == "controls.json", "its own file, beside the other saves");

    Controls::Store store;
    std::string error;
    CHECK_MSG(store.Open(dir.string(), rules, error) && error.empty(), "a directory with no file opens: " + error);
    CHECK_MSG(!store.Mode() && !store.FromFile() && store.Persists(), "nothing chosen, nothing read, somewhere to write");
    CHECK_MSG(store.Path() == file.string(), "the path is the directory's controls.json: " + store.Path());
    CHECK_MSG(!std::filesystem::exists(file), "opening writes nothing");

    // The first choice: Modern. The bytes the design names (2.4, G3): one line, the version, the mode and the layout, and no newline.
    store.SetMode(Controls::Style::Modern);
    CHECK_MSG(store.Save(error) && error.empty(), "a choice is saved: " + error);
    const std::string modern = "{\"version\":1,\"controls\":{\"mode\":\"modern\",\"layout\":2}}";
    CHECK_MSG(StoreRead(file) == modern, "the file reads back byte for byte: " + StoreRead(file));
    CHECK_MSG(store.Text() == modern, "and Text says what Save wrote");
    CHECK_MSG(StoreEntries(dir) == std::vector<std::string>{"controls.json"}, "no temporary is left beside it");

    Controls::Store again;
    CHECK_MSG(again.Open(dir.string(), rules, error), "and it opens again: " + error);
    CHECK_MSG(again.Mode() == std::optional<Controls::Style>(Controls::Style::Modern) && again.FromFile(), "Modern, read from the file");
    CHECK_MSG(again.SavedTuning().IsDefault(rules), "with no tuning in it, the default tuning");

    // Changed back, Classic is a choice too and is written: it is not the absence of one.
    again.SetMode(Controls::Style::Classic);
    CHECK_MSG(again.Save(error), "Classic is saved: " + error);
    CHECK_MSG(StoreRead(file) == "{\"version\":1,\"controls\":{\"mode\":\"classic\",\"layout\":2}}", "as classic: " + StoreRead(file));
    Controls::Store third;
    CHECK(third.Open(dir.string(), rules, error) && third.Mode() == std::optional<Controls::Style>(Controls::Style::Classic));

    // A size, an opacity and a move already in the file are KEPT and written back unchanged beside the new mode (this case is about the mode; the tuning has its own, planning step 187).
    const std::string tuned = "{\"version\":1,\"controls\":{\"mode\":\"modern\",\"layout\":2,\"size\":1.2,\"opacity\":0.6,\"move\":{\"walk_left\":[2,-3.5],\"pause\":[-10,5]}}}";
    StoreWrite(file, tuned);
    Controls::Store keep;
    CHECK_MSG(keep.Open(dir.string(), rules, error) && keep.FromFile(), "a tuned file opens: " + error);
    CHECK(keep.SavedTuning().sizeSteps && *keep.SavedTuning().sizeSteps == 12 && keep.SavedTuning().opacitySteps && *keep.SavedTuning().opacitySteps == 3);
    CHECK(keep.SavedTuning().move[0].x == 2.0 && keep.SavedTuning().move[0].y == -3.5 && keep.SavedTuning().move[3].x == -10.0);
    keep.SetMode(Controls::Style::Classic);
    CHECK_MSG(keep.Save(error), "saved: " + error);
    const std::string kept = "{\"version\":1,\"controls\":{\"mode\":\"classic\",\"layout\":2,\"size\":1.2,\"opacity\":0.6,\"move\":{\"walk_left\":[2,-3.5],\"pause\":[-10,5]}}}";
    CHECK_MSG(StoreRead(file) == kept, "only the mode changed: " + StoreRead(file));

    // A gesture repeated is one write: the same bytes again are not written (Penumbra L9). The first press of a tile IS written even when that
    // tile is already the mode in force, because nothing of this store's was ever on disk: Classic chosen on a fresh install is kept.
    Controls::Store repeat;
    const std::filesystem::path dir2 = StoreDir("repeat");
    CHECK(repeat.Open(dir2.string(), rules, error));
    repeat.SetMode(Controls::Style::Classic);
    CHECK(repeat.Save(error) && std::filesystem::exists(dir2 / Controls::kStoreFileName));
    std::filesystem::remove(dir2 / Controls::kStoreFileName);
    CHECK_MSG(repeat.Save(error) && !std::filesystem::exists(dir2 / Controls::kStoreFileName), "the same bytes are not written twice");
    repeat.SetMode(Controls::Style::Modern);
    CHECK_MSG(repeat.Save(error) && std::filesystem::exists(dir2 / Controls::kStoreFileName), "a different choice is");
}

void TheFileHoldsDeltasOnly() {
    const StoreClean clean;
    const Controls::Rules rules = LoadModern();
    const std::filesystem::path dir = StoreDir("deltas");
    const std::filesystem::path file = dir / Controls::kStoreFileName;
    std::string error;
    // The defaults written out in full - the look's own size (0.7 at the default preset), the default opacity (1), no move - are not kept: a
    // later change of the shipped default then reaches this player. A move that snaps to nothing is no move.
    StoreWrite(file, "{\"version\":1,\"controls\":{\"mode\":\"modern\",\"layout\":2,\"size\":0.7,\"opacity\":1.0,\"move\":{\"walk_left\":[0,0],\"pause\":[0.04,-0.03]}}}");
    Controls::Store store;
    CHECK_MSG(store.Open(dir.string(), rules, error) && store.FromFile(), "opens: " + error);
    CHECK_MSG(!store.SavedTuning().sizeSteps && !store.SavedTuning().opacitySteps, "a size and an opacity equal to the default are not kept");
    for (const Controls::Id id : Controls::kClaimOrder) {
        CHECK_MSG(store.SavedTuning().move[static_cast<std::size_t>(id)] == glm::dvec2(0.0), std::string(Controls::KeyOf(id)) + ": a move that snaps to nothing is none");
    }
    store.SetMode(Controls::Style::Modern);
    CHECK_MSG(store.Save(error), "saved: " + error);
    CHECK_MSG(StoreRead(file) == "{\"version\":1,\"controls\":{\"mode\":\"modern\",\"layout\":2}}", "the file is the bare choice: " + StoreRead(file));
    for (const char* key : {"size", "opacity", "move"}) {
        CHECK_MSG(StoreRead(file).find(std::string("\"") + key + "\"") == std::string::npos, std::string("no ") + key + " key is written for a default");
    }
    // A player who never chose has no mode in the file either: the shipped default reaches them.
    Controls::Store never;
    const std::filesystem::path dir2 = StoreDir("deltas_never");
    CHECK(never.Open(dir2.string(), rules, error));
    CHECK_MSG(never.Text() == "{\"version\":1,\"controls\":{\"layout\":2}}", "a store nobody chose in says no mode: " + never.Text());
    // One that differs is written, and only it.
    StoreWrite(file, "{\"version\":1,\"controls\":{\"layout\":2,\"size\":1.4}}");
    Controls::Store big;
    CHECK(big.Open(dir.string(), rules, error));
    big.SetMode(Controls::Style::Classic);
    CHECK(big.Save(error));
    CHECK_MSG(StoreRead(file) == "{\"version\":1,\"controls\":{\"mode\":\"classic\",\"layout\":2,\"size\":1.4}}", "a size that differs is the one key kept: " + StoreRead(file));
}

void AMissingFileIsTheShippedDefault() {
    const StoreClean clean;
    const Controls::Rules rules = LoadModern();
    const std::filesystem::path dir = StoreDir("missing");
    Controls::Store store;
    std::string error;
    CHECK_MSG(store.Open(dir.string(), rules, error) && error.empty(), "no file is not an error: " + error);
    CHECK_MSG(!store.Mode() && !store.FromFile() && store.SavedTuning().IsDefault(rules), "no mode, no file read, the default tuning");
    // A directory that is not there yet is no file either, and opening it makes nothing.
    const std::filesystem::path later = dir / "not" / "yet";
    Controls::Store elsewhere;
    CHECK_MSG(elsewhere.Open(later.string(), rules, error) && !elsewhere.Mode() && !std::filesystem::exists(later), "a directory that is not there: nothing, and none made");
    // The style is then the data's default: the policy's chain (ResolveStyle) takes nothing saved.
    Controls::Policy player = Controls::ResolvePolicy(std::nullopt, false, false, false);
    CHECK(player.available && player.saveAllowed);
    CHECK_MSG(Controls::ResolveStyle(player, store.Mode(), rules.defaultMode) == Controls::Style::Classic, "a player's run with nothing saved plays the shipped default (classic)");
    CHECK_MSG(Controls::ResolveStyle(player, std::optional<Controls::Style>(Controls::Style::Modern), rules.defaultMode) == Controls::Style::Modern, "and the saved choice when there is one");
    // Saving makes the directory, as the other stores do.
    elsewhere.SetMode(Controls::Style::Modern);
    CHECK_MSG(elsewhere.Save(error) && std::filesystem::exists(later / Controls::kStoreFileName), "a save makes the directory it needs: " + error);
}

void AnUnreadableFileIsDefaultedAndOverwrittenAtTheNextSave() {
    const StoreClean clean;
    const Controls::Rules rules = LoadModern();
    const std::filesystem::path dir = StoreDir("unreadable");
    const std::filesystem::path file = dir / Controls::kStoreFileName;
    std::string error;
    // Text that is not JSON, JSON that is not an object, nothing at all: the defaults, one error that names the file, kept to be written over.
    for (const char* text : {"{not json", "[]", "5", "\"x\"", "", "{\"version\":1,"}) {
        StoreWrite(file, text);
        Controls::Store store;
        const bool opened = store.Open(dir.string(), rules, error);
        CHECK_MSG(!opened && error.find("controls.json") != std::string::npos, std::string("'") + text + "' does not read, and the error names the file: " + error);
        CHECK_MSG(!store.Mode() && !store.FromFile() && store.Persists() && store.SavedTuning().IsDefault(rules), std::string("'") + text + "': the defaults are kept, with somewhere to write");
        store.SetMode(Controls::Style::Modern);
        CHECK_MSG(store.Save(error), std::string("'") + text + "': the next save writes: " + error);
        CHECK_MSG(StoreRead(file) == "{\"version\":1,\"controls\":{\"mode\":\"modern\",\"layout\":2}}", std::string("'") + text + "': and overwrote it: " + StoreRead(file));
    }
    // A file that parses but says little is not unreadable: field by field, the defaults for what it lacks, a mode that is none.
    for (const char* text : {"{}", "{\"controls\":5}", "{\"controls\":{\"mode\":\"retro\"}}", "{\"controls\":{\"mode\":3}}", "{\"controls\":{\"mode\":null}}",
                             "{\"controls\":{\"size\":\"big\",\"opacity\":[1],\"move\":\"no\"}}"}) {
        StoreWrite(file, text);
        Controls::Store store;
        CHECK_MSG(store.Open(dir.string(), rules, error) && error.empty() && store.FromFile(), std::string("'") + text + "' reads: " + error);
        CHECK_MSG(!store.Mode() && store.SavedTuning().IsDefault(rules), std::string("'") + text + "': no mode, the default tuning");
    }
    // A mode of its own in a file that says nothing else.
    StoreWrite(file, "{\"controls\":{\"mode\":\"classic\"}}");
    Controls::Store classic;
    CHECK(classic.Open(dir.string(), rules, error) && classic.Mode() == std::optional<Controls::Style>(Controls::Style::Classic));
}

void AMemoryOnlyStoreWritesNothing() {
    const StoreClean clean;
    const Controls::Rules rules = LoadModern();
    const std::filesystem::path dir = StoreDir("memory");
    std::string error;
    // An empty directory is memory-only, which is how the layer opens it when the policy forbids saving (a forced style) and when the run has no
    // save directory.
    Controls::Store store;
    CHECK_MSG(store.Open("", rules, error) && error.empty(), "an empty directory opens");
    CHECK_MSG(!store.Persists() && !store.FromFile() && store.Path().empty() && !store.Mode(), "memory-only: nowhere to write, nothing read");
    store.SetMode(Controls::Style::Modern);
    CHECK_MSG(store.Mode() == std::optional<Controls::Style>(Controls::Style::Modern), "the choice is kept for the run");
    CHECK_MSG(store.Save(error) && error.empty(), "saving succeeds");
    CHECK_MSG(StoreEntries(dir).empty() && !std::filesystem::exists(std::filesystem::current_path() / Controls::kStoreFileName),
              "and no file was written, here or in the working directory");
    // Opening memory-only again forgets the run's choice: the store is built afresh each time.
    CHECK(store.Open("", rules, error) && !store.Mode());
}

void ALayoutNumberFromAnotherVersionDropsMovesAndKeepsSizeAndOpacity() {
    const StoreClean clean;
    const Controls::Rules rules = LoadModern();
    const std::filesystem::path dir = StoreDir("layout");
    const std::filesystem::path file = dir / Controls::kStoreFileName;
    std::string error;
    // The number the file was saved at is the arrangement its moves were made against: 1 was the grouped pair's (planning step 180, what the owner's own
    // controls.json would have if he had saved one) and the code's is 2 (planning step 186, the arrows split). "1" below IS that file: it keeps its
    // size and opacity, drops its moves, and the next save writes layout 2. The byte-for-byte pin of CTL-9 moved from layout 1 to layout 2 with it.
    for (const char* layout : {"1", "3", "0", "1.5", "\"2\"", "null"}) {
        StoreWrite(file, std::string("{\"version\":1,\"controls\":{\"mode\":\"modern\",\"layout\":") + layout +
                             ",\"size\":1.2,\"opacity\":0.6,\"move\":{\"walk_left\":[2,-3.5],\"restart\":[4,4]}}}");
        Controls::Store store;
        CHECK_MSG(store.Open(dir.string(), rules, error) && store.FromFile(), std::string("layout ") + layout + " reads: " + error);
        CHECK_MSG(store.Mode() == std::optional<Controls::Style>(Controls::Style::Modern), std::string("layout ") + layout + ": the mode is kept");
        CHECK_MSG(store.SavedTuning().sizeSteps && *store.SavedTuning().sizeSteps == 12 && store.SavedTuning().opacitySteps && *store.SavedTuning().opacitySteps == 3,
                  std::string("layout ") + layout + ": the size and the opacity are kept");
        for (const Controls::Id id : Controls::kClaimOrder) {
            CHECK_MSG(store.SavedTuning().move[static_cast<std::size_t>(id)] == glm::dvec2(0.0), std::string("layout ") + layout + ": " + Controls::KeyOf(id) + "'s move was made against another arrangement and is dropped");
        }
        store.SetMode(Controls::Style::Modern);
        CHECK(store.Save(error));
        CHECK_MSG(StoreRead(file) == "{\"version\":1,\"controls\":{\"mode\":\"modern\",\"layout\":2,\"size\":1.2,\"opacity\":0.6}}",
                  std::string("layout ") + layout + ": the next save carries this layout's number and no move: " + StoreRead(file));
    }
    // The current number keeps the moves.
    StoreWrite(file, "{\"version\":1,\"controls\":{\"layout\":2,\"move\":{\"restart\":[4,4]}}}");
    Controls::Store same;
    CHECK(same.Open(dir.string(), rules, error));
    CHECK_MSG(same.SavedTuning().move[static_cast<std::size_t>(Controls::Id::Reset)] == glm::dvec2(4.0, 4.0), "the current layout number keeps its moves");
}

void ASaveThatCannotBeMadeSaysSoAndLeavesNothingHalfWritten() {
    const StoreClean clean;
    const Controls::Rules rules = LoadModern();
    const std::filesystem::path dir = StoreDir("blocked");
    std::string error;
    Controls::Store store;
    CHECK(store.Open(dir.string(), rules, error));
    store.SetMode(Controls::Style::Modern);
    // The name the file is to have taken by a directory that holds something: the temporary is written, the rename onto it is refused, and the save says
    // so, naming the file, and leaves no temporary behind. (The temporary has a name of its own for every save, so the way to a failure that
    // planning step 185 had - the temporary's name taken - is gone; this is the one that remains.)
    std::error_code ec;
    std::filesystem::create_directories(dir / Controls::kStoreFileName, ec);
    StoreWrite(dir / Controls::kStoreFileName / "keep.txt", "kept");
    CHECK_MSG(!store.Save(error) && error.find("controls.json") != std::string::npos, "a save that cannot be put in place says so, naming it: " + error);
    CHECK_MSG(std::filesystem::is_directory(dir / Controls::kStoreFileName) && StoreRead(dir / Controls::kStoreFileName / "keep.txt") == "kept",
              "and what stood in its way is where it was, whole");
    CHECK_MSG(StoreEntries(dir) == std::vector<std::string>{"controls.json"}, "and no temporary is left beside it");
    std::filesystem::remove_all(dir / Controls::kStoreFileName, ec);
    CHECK_MSG(store.Save(error) && std::filesystem::is_regular_file(dir / Controls::kStoreFileName), "the next save, with the way clear, writes (a failure is not remembered as a write): " + error);
    CHECK_MSG(StoreRead(dir / Controls::kStoreFileName) == "{\"version\":1,\"controls\":{\"mode\":\"modern\",\"layout\":2}}", "the bytes it holds: " + StoreRead(dir / Controls::kStoreFileName));
}

// The other stores are not this one's business: audio.json is Switches::Store's, and a save of either leaves the other's bytes alone.
void TheSwitchesStoreIsNotTouched() {
    const StoreClean clean;
    const Controls::Rules rules = LoadModern();
    const std::filesystem::path dir = StoreDir("switches");
    std::string error;
    Switches::Defaults defaults;
    Switches::Store switches;
    CHECK(switches.Open(dir.string(), defaults, error));
    switches.SetSound(false);
    switches.SetMusic(false);
    CHECK_MSG(switches.Save(error), "audio.json is written: " + error);
    const std::string audio = StoreRead(dir / Switches::kFileName);
    CHECK_MSG(!audio.empty(), "and has bytes");

    Controls::Store store;
    CHECK(store.Open(dir.string(), rules, error));
    store.SetMode(Controls::Style::Modern);
    CHECK(store.Save(error));
    CHECK_MSG(StoreRead(dir / Switches::kFileName) == audio, "a toggle of the controls leaves audio.json's bytes as they were");
    CHECK_MSG(StoreEntries(dir) == (std::vector<std::string>{"audio.json", "controls.json"}), "two files, side by side");

    const std::string controls = StoreRead(dir / Controls::kStoreFileName);
    switches.SetSound(true);
    CHECK(switches.Save(error));
    CHECK_MSG(StoreRead(dir / Controls::kStoreFileName) == controls, "and a switch leaves controls.json's");
    // Neither reads the other's: audio.json holds no controls key, controls.json no audio one.
    CHECK_MSG(audio.find("controls") == std::string::npos && controls.find("audio") == std::string::npos, "no key of one is in the other");
}

// ---- the tuning, kept (planning step 187, CTL-10) ---------------------------------------------------------------------------------------------------

// The tuning a player keeps goes into the file as deltas on one line, and reads back as it was. Every field is off the default here, the moves are in
// claim order with a fractional one, one at each bound of the move, and the look and the arrangement ride on the tuning handed in but are NOT kept.
void TheTuningRoundTripsThroughTheFile() {
    const StoreClean clean;
    const Controls::Rules rules = LoadModern();
    const std::filesystem::path dir = StoreDir("tuning");
    const std::filesystem::path file = dir / Controls::kStoreFileName;
    std::string error;
    Controls::Store store;
    CHECK(store.Open(dir.string(), rules, error));

    Controls::Tuning kept;
    kept.sizeSteps = 14;
    kept.opacitySteps = 9;
    kept.move[0] = glm::dvec2(2.0, -3.5);
    kept.move[1] = glm::dvec2(-40.0, 20.5);
    kept.move[2] = glm::dvec2(0.1, -0.1);
    kept.move[3] = glm::dvec2(-1024.0, 1024.0);
    kept.move[4] = glm::dvec2(300.0, -0.4);
    kept.preset = Controls::Preset::Mr;           // a capture's look: not a player's, never kept
    kept.arrows = Controls::Arrows::Grouped;      // a capture's arrangement: likewise
    store.SetTuning(kept);
    CHECK_MSG(!store.SavedTuning().preset && !store.SavedTuning().arrows, "the look and the arrangement are not held by the store");
    CHECK_MSG(store.SavedTuning().sizeSteps == std::optional<int>(14) && store.SavedTuning().opacitySteps == std::optional<int>(9), "the size 1.4 and the opacity 1.8 are held");
    CHECK_MSG(!std::filesystem::exists(file), "setting writes nothing");
    store.SetMode(Controls::Style::Modern);
    CHECK_MSG(store.Save(error), "saved: " + error);
    const std::string want =
        "{\"version\":1,\"controls\":{\"mode\":\"modern\",\"layout\":2,\"size\":1.4,\"opacity\":1.8,\"move\":{\"walk_left\":[2,-3.5],\"walk_right\":[-40,20.5],"
        "\"restart\":[0.1,-0.1],\"pause\":[-1024,1024],\"clear_portals\":[300,-0.4]}}}";
    CHECK_MSG(StoreRead(file) == want, "the file is one line, the deltas in claim order: " + StoreRead(file));
    CHECK_MSG(StoreRead(file).find("preset") == std::string::npos && StoreRead(file).find("arrows") == std::string::npos, "and holds no look and no arrangement");
    CHECK_MSG(StoreEntries(dir) == std::vector<std::string>{"controls.json"}, "and nothing beside it");

    Controls::Store again;
    CHECK_MSG(again.Open(dir.string(), rules, error) && again.FromFile(), "it opens again: " + error);
    CHECK_MSG(SameTuning(again.SavedTuning(), store.SavedTuning()), "and the tuning is the one that was kept, field for field");
    CHECK_MSG(again.SavedTuning().move[3] == glm::dvec2(-1024.0, 1024.0) && again.SavedTuning().move[4] == glm::dvec2(300.0, -0.4), "the moves at the bound and the fractional one are exact");
    CHECK_MSG(Near(again.SavedTuning().Size(rules), 1.4, 1e-12) && Near(again.SavedTuning().Opacity(rules), 1.8, 1e-12), "the size and the opacity in force are 1.4 and 1.8");
    CHECK_MSG(again.Text() == want, "and Text says the same bytes again");

    // Moved back to the defaults the keys go: the file is the bare choice again, so a later change of the shipped default reaches this player.
    again.SetTuning(Controls::Tuning{});
    CHECK(again.Save(error));
    CHECK_MSG(StoreRead(file) == "{\"version\":1,\"controls\":{\"mode\":\"modern\",\"layout\":2}}", "set back to the default, the keys are gone: " + StoreRead(file));

    // A number that is not a number is never written: a NaN or an infinity is no move (0), a finite one past the bound is held at it.
    Controls::Tuning odd;
    odd.move[1] = glm::dvec2(std::numeric_limits<double>::quiet_NaN(), 5.0);
    odd.move[2] = glm::dvec2(5000.0, -5000.0);
    odd.move[3] = glm::dvec2(std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity());
    again.SetTuning(odd);
    CHECK(again.Save(error));
    CHECK_MSG(StoreRead(file) == "{\"version\":1,\"controls\":{\"mode\":\"modern\",\"layout\":2,\"move\":{\"walk_right\":[0,5],\"restart\":[1024,-1024]}}}",
              "NaN and infinity are 0, 5000 is the bound 1024, and no 'nan' or 'inf' is written: " + StoreRead(file));

    // A memory-only store keeps the tuning for the run and writes nothing, as it does the mode.
    Controls::Store memory;
    CHECK(memory.Open("", rules, error));
    memory.SetTuning(kept);
    CHECK_MSG(memory.SavedTuning().sizeSteps == std::optional<int>(14) && memory.Save(error) && !memory.Persists(), "a memory-only store holds it and saves nothing");
    CHECK_MSG(StoreEntries(dir) == std::vector<std::string>{"controls.json"}, "and wrote no file anywhere it was pointed");
}

// Nothing is written for nothing: a size and an opacity that equal the default, and a move that snaps to zero, are no keys - on a store that was
// handed them and on one that read them from a file - and a player who never chose has no mode either.
void TheDefaultTuningWritesNoMoveAndNoSurprise() {
    const StoreClean clean;
    const Controls::Rules rules = LoadModern();
    const std::filesystem::path dir = StoreDir("defaults_written");
    const std::filesystem::path file = dir / Controls::kStoreFileName;
    std::string error;
    Controls::Store store;
    CHECK(store.Open(dir.string(), rules, error));
    CHECK_MSG(store.Text() == "{\"version\":1,\"controls\":{\"layout\":2}}", "a store nobody touched: " + store.Text());
    store.SetTuning(Controls::Tuning{});
    CHECK_MSG(store.Text() == "{\"version\":1,\"controls\":{\"layout\":2}}", "the default tuning adds nothing: " + store.Text());

    Controls::Tuning spelt;
    spelt.sizeSteps = 7;      // the look's own 0.7
    spelt.opacitySteps = 5;   // the default 1.0
    for (glm::dvec2& m : spelt.move) m = glm::dvec2(0.04, -0.03); // under half a tenth: nothing
    store.SetTuning(spelt);
    store.SetMode(Controls::Style::Modern);
    CHECK_MSG(store.Text() == "{\"version\":1,\"controls\":{\"mode\":\"modern\",\"layout\":2}}", "the default spelt out in full is not written: " + store.Text());
    CHECK_MSG(!store.SavedTuning().sizeSteps && !store.SavedTuning().opacitySteps && store.SavedTuning().IsDefault(rules), "and is not held either");
    CHECK(store.Save(error));
    CHECK_MSG(StoreRead(file).find("move") == std::string::npos && StoreRead(file).find("size") == std::string::npos && StoreRead(file).find("opacity") == std::string::npos,
              "no move, no size, no opacity in the file: " + StoreRead(file));

    // The key of ONE field is the only key: a size alone, an opacity alone, one move alone.
    const std::array<std::pair<Controls::Tuning, const char*>, 3> single = {{
        {[] { Controls::Tuning t; t.sizeSteps = 5; return t; }(), "{\"version\":1,\"controls\":{\"layout\":2,\"size\":0.5}}"},
        {[] { Controls::Tuning t; t.opacitySteps = 1; return t; }(), "{\"version\":1,\"controls\":{\"layout\":2,\"opacity\":0.2}}"},
        {[] { Controls::Tuning t; t.move[4] = glm::dvec2(0.0, 0.1); return t; }(), "{\"version\":1,\"controls\":{\"layout\":2,\"move\":{\"clear_portals\":[0,0.1]}}}"},
    }};
    for (const auto& [tuning, text] : single) {
        Controls::Store one;
        CHECK(one.Open(StoreDir("defaults_single").string(), rules, error));
        one.SetTuning(tuning);
        CHECK_MSG(one.Text() == text, std::string("one field, one key: ") + one.Text());
    }
}

// Design 2.4: only what differs from the data's default is written, so a default that changes later reaches a player who never moved a slider. A player who did
// keeps what they chose.
void ADefaultThatChangesReachesAPlayerWhoNeverMovedASlider() {
    const StoreClean clean;
    const Controls::Rules rules = LoadModern();
    const std::filesystem::path dir = StoreDir("later_default");
    const std::filesystem::path file = dir / Controls::kStoreFileName;
    std::string error;
    // This release's: the player chose Modern and never touched a slider, or moved the size to the default and back. No key.
    Controls::Store player;
    CHECK(player.Open(dir.string(), rules, error));
    player.SetMode(Controls::Style::Modern);
    Controls::Tuning backAtTheDefault;
    backAtTheDefault.sizeSteps = rules.presets[0].sizeSteps;
    player.SetTuning(backAtTheDefault);
    CHECK(player.Save(error));
    CHECK_MSG(StoreRead(file) == "{\"version\":1,\"controls\":{\"mode\":\"modern\",\"layout\":2}}", "the file of a player who never moved a slider has no size: " + StoreRead(file));
    CHECK_MSG(Near(player.SavedTuning().Size(rules), 0.7, 1e-12) && Near(player.SavedTuning().Opacity(rules), 1.0, 1e-12), "this release plays 0.7 and 1.0");

    // A later release changes the shipped size (0.7 to 0.9) and opacity (1.0 to 0.8): the same file now plays the new defaults.
    Controls::Rules later = rules;
    later.presets[0].sizeSteps = 9;
    later.opacityDefaultSteps = 4;
    Controls::Store next;
    CHECK(next.Open(dir.string(), later, error));
    CHECK_MSG(!next.SavedTuning().sizeSteps && !next.SavedTuning().opacitySteps, "the file still says nothing about either");
    CHECK_MSG(Near(next.SavedTuning().Size(later), 0.9, 1e-12) && Near(next.SavedTuning().Opacity(later), 0.8, 1e-12),
              "and the new defaults reach this player: " + Num(next.SavedTuning().Size(later)) + ", " + Num(next.SavedTuning().Opacity(later)));
    CHECK_MSG(next.SavedTuning().IsDefault(later), "who is still on the default");

    // A player who chose a size and an opacity keeps them whatever the default becomes.
    StoreWrite(file, "{\"version\":1,\"controls\":{\"mode\":\"modern\",\"layout\":2,\"size\":1.2,\"opacity\":0.6}}");
    Controls::Store chose;
    CHECK(chose.Open(dir.string(), later, error));
    CHECK_MSG(Near(chose.SavedTuning().Size(later), 1.2, 1e-12) && Near(chose.SavedTuning().Opacity(later), 0.6, 1e-12), "a player who chose 1.2 and 0.6 keeps them");
    CHECK_MSG(!chose.SavedTuning().IsDefault(later), "and is no longer on the default");
    // A player who chose exactly the NEW default's value under the old release is a player on the old default: the file is read against the rules it is opened
    // with, so 0.9 under this release's rules is a size they chose, and stays 0.9 when the default is 0.9 and when it moves again.
    StoreWrite(file, "{\"version\":1,\"controls\":{\"layout\":2,\"size\":0.9}}");
    Controls::Store nine;
    CHECK(nine.Open(dir.string(), rules, error));
    CHECK_MSG(Near(nine.SavedTuning().Size(rules), 0.9, 1e-12), "a size of 0.9 chosen against this release's 0.7 is 0.9");
    Controls::Store nineLater;
    CHECK(nineLater.Open(dir.string(), later, error));
    CHECK_MSG(!nineLater.SavedTuning().sizeSteps && Near(nineLater.SavedTuning().Size(later), 0.9, 1e-12), "and is the later default when that is 0.9 too (a choice equal to the default is none)");
}

// A file with the look or the arrangement in it - a hand edit, a future version - is read without them: they are a capture's, never a player's, so a saved
// file cannot strand a player on a look or an arrangement (planning step 186's hygiene item 2).
void AFileWithStrayLookKeysIsReadWithoutThem() {
    const StoreClean clean;
    const Controls::Rules rules = LoadModern();
    const std::filesystem::path dir = StoreDir("stray");
    const std::filesystem::path file = dir / Controls::kStoreFileName;
    std::string error;
    StoreWrite(file, "{\"version\":1,\"controls\":{\"mode\":\"modern\",\"layout\":2,\"preset\":\"mr\",\"arrows\":\"grouped\",\"size\":1.2,\"opacity\":0.6}}");
    Controls::Store store;
    CHECK_MSG(store.Open(dir.string(), rules, error) && store.FromFile(), "opens: " + error);
    CHECK_MSG(!store.SavedTuning().preset && !store.SavedTuning().arrows, "neither key is read onto the tuning");
    CHECK_MSG(store.SavedTuning().PresetIn(rules) == rules.defaultPreset && store.SavedTuning().ArrowsIn(rules) == rules.defaultArrows, "the look and the arrangement in force are the data's");
    CHECK_MSG(Near(store.SavedTuning().Size(rules), 1.2, 1e-12) && Near(store.SavedTuning().Opacity(rules), 0.6, 1e-12), "while the size and the opacity beside them are kept");
    store.SetMode(Controls::Style::Classic);
    CHECK(store.Save(error));
    CHECK_MSG(StoreRead(file) == "{\"version\":1,\"controls\":{\"mode\":\"classic\",\"layout\":2,\"size\":1.2,\"opacity\":0.6}}", "and the next save does not write them back: " + StoreRead(file));
    // The same through Tuning::ReadFrom, the reader the store uses.
    const Controls::Tuning read = Controls::Tuning::ReadFrom(ParseText("{\"preset\":\"balanced\",\"arrows\":\"split\",\"size\":0.5}"), rules);
    CHECK_MSG(!read.preset && !read.arrows && Near(read.Size(rules), 0.5, 1e-12), "ReadFrom ignores them too");
}

// The flag's keys that TUNE - a size, an opacity, a move - replace what a player kept; the look and the arrangement do not (the layer's rule, planning step 187).
void TheFlagTunesOnlyWithASizeAnOpacityOrAMove() {
    const std::array<std::pair<const char*, bool>, 12> rows = {{{"", false},
                                                               {"preset=mr", false},
                                                               {"arrows=grouped", false},
                                                               {"preset=balanced,arrows=split", false},
                                                               {"size=1.2", true},
                                                               {"opacity=0.6", true},
                                                               {"walk_left=1:2", true},
                                                               {"walk_right=0:0", true},
                                                               {"restart=1:1", true},
                                                               {"pause=-1:-1", true},
                                                               {"clear_portals=5:5", true},
                                                               {"preset=mr,arrows=grouped,size=1.4", true}}};
    for (const auto& [text, tunes] : rows) {
        Controls::FlagItems items;
        std::string error;
        CHECK_MSG(Controls::Tuning::ParseFlag(text, items, error), std::string("'") + text + "' parses: " + error);
        CHECK_MSG(items.Tunes() == tunes, std::string("'") + text + "' " + (tunes ? "tunes" : "does not tune"));
        CHECK_MSG(items.Any() == (std::string(text) != ""), std::string("'") + text + "': Any() is every key, tuning or not");
    }
}

// Design 2.4 and the open items of Steps 183 to 186: the replace is one step and the live file is never removed. The scenarios: a leftover temporary of the
// first version's fixed name that something holds open (where the old code removed the live file and then could not rename: the choice was LOST), a live file
// another handle holds (the replace is refused or lands whole, never half), two stores saving alternately, and no temporary left on any path.
// NOT run against the old code (it is not here to run): the first scenario fails there by reading, since its temporary was that very name.
void ASaveReplacesInOneStepAndNeverRemovesTheLiveFile() {
    const StoreClean clean;
    const Controls::Rules rules = LoadModern();
    const std::filesystem::path dir = StoreDir("replace");
    const std::filesystem::path file = dir / Controls::kStoreFileName;
    std::string error;
    const std::string modern = "{\"version\":1,\"controls\":{\"mode\":\"modern\",\"layout\":2}}";
    const std::string classic = "{\"version\":1,\"controls\":{\"mode\":\"classic\",\"layout\":2}}";
    Controls::Store store;
    CHECK(store.Open(dir.string(), rules, error));
    store.SetMode(Controls::Style::Modern);
    CHECK(store.Save(error) && StoreRead(file) == modern);
    CHECK_MSG(StoreEntries(dir) == std::vector<std::string>{"controls.json"}, "a save leaves no temporary");

    // 1. The first version's temporary name, held open by another writer: it is not this save's temporary, so it neither blocks the save nor is touched by it.
    {
        std::ofstream holder(dir / "controls.json.tmp", std::ios::binary | std::ios::trunc);
        holder << "HELD";
        holder.flush();
        store.SetMode(Controls::Style::Classic);
        CHECK_MSG(store.Save(error) && error.empty(), "a leftover of the old fixed temporary name, held open, does not stop a save: " + error);
        CHECK_MSG(std::filesystem::is_regular_file(file) && StoreRead(file) == classic, "and the live file is there and holds the new bytes: " + StoreRead(file));
        CHECK_MSG(StoreRead(dir / "controls.json.tmp") == "HELD", "while the other writer's file is as it left it");
    }
    std::error_code ec;
    std::filesystem::remove(dir / "controls.json.tmp", ec);

    // 2. The live file held open by another handle (another window's read, a scanner): the rename is refused on Windows and lands whole where it is allowed.
    {
        std::ifstream holder(file, std::ios::binary);
        CHECK_MSG(holder.is_open(), "another handle holds the live file open");
        store.SetMode(Controls::Style::Modern);
        const bool saved = store.Save(error);
        CHECK_MSG(std::filesystem::is_regular_file(file), "the live file is there whether the save was refused or not");
        const std::string now = StoreRead(file);
        CHECK_MSG(now == classic || now == modern, "wholly the old bytes or wholly the new, never a mixture: " + now);
        CHECK_MSG(saved == (now == modern), "and the save says which");
        if (!saved) CHECK_MSG(error.find("controls.json") != std::string::npos, "a refused save names the file: " + error);
        CHECK_MSG(StoreEntries(dir) == std::vector<std::string>{"controls.json"}, "no temporary is left beside it on either path");
        std::printf("  a handle on the live file: the replace was %s on this machine\n", saved ? "allowed (the new bytes landed whole)" : "refused (the old bytes stayed whole)");
    }
    // With the handle gone, the next save makes it right (a refusal is not remembered as a write).
    CHECK_MSG(store.Save(error) && StoreRead(file) == modern, "the handle closed, the next save writes the bytes: " + error);

    // 3. Two stores (two windows) saving alternately: after every save the file is whole and is the saver's bytes, and nothing is left.
    Controls::Store a;
    Controls::Store b;
    CHECK(a.Open(dir.string(), rules, error) && b.Open(dir.string(), rules, error));
    int whole = 0;
    for (int round = 0; round < 6; ++round) {
        const bool even = round % 2 == 0;
        a.SetMode(even ? Controls::Style::Classic : Controls::Style::Modern);
        CHECK(a.Save(error));
        whole += StoreRead(file) == a.Text() ? 1 : 0;
        b.SetMode(even ? Controls::Style::Modern : Controls::Style::Classic);
        CHECK(b.Save(error));
        whole += StoreRead(file) == b.Text() ? 1 : 0;
    }
    CHECK_MSG(whole == 12, "twelve alternating saves, each one whole: " + std::to_string(whole));
    CHECK_MSG(StoreEntries(dir) == std::vector<std::string>{"controls.json"}, "and no temporary was left by any of them");
}

// ---- two contacts (planning step 191, CTL-14) -------------------------------------------------------------------------------

// Which finger works what, as a pure function of this tick's fingers (in the order the layer lists them: its pointer first) and of the roles the last tick
// gave. The rules are in the header; each row is one of them, and the layer's cases (test_mp_layer) are what plays them.
void AssignContactsTruthTable() {
    using Controls::AssignContacts;
    using Controls::ContactRoles;
    using Controls::LiveContact;
    constexpr int none = Controls::kNoContact;
    const auto at = [](int id, bool onControl) { return LiveContact{id, onControl}; };
    struct Row {
        const char* why;
        std::vector<LiveContact> live;
        ContactRoles before;
        ContactRoles want;
    };
    const std::vector<Row> rows = {
        {"nobody down", {}, {}, {none, none}},
        {"nobody down: the roles of the last tick are gone with their fingers", {}, {1, 2}, {none, none}},
        {"one finger in the world is the world's alone", {at(1, false)}, {}, {none, 1}},
        {"one finger on a control is both: it works the controls and is the world's", {at(1, true)}, {}, {1, 1}},
        {"one finger that held the controls and is off its control now is the world's alone", {at(1, false)}, {1, 1}, {none, 1}},
        {"two: the finger on a control works the controls, the other is the world's", {at(1, true), at(2, false)}, {}, {1, 2}},
        {"two: the same whichever is listed first", {at(1, false), at(2, true)}, {}, {2, 1}},
        {"two in the world: the first is the world's and the second has no role", {at(1, false), at(2, false)}, {}, {none, 1}},
        {"two on controls: the first works the controls, the second has no role and the world is empty", {at(1, true), at(2, true)}, {}, {1, none}},
        {"the controls finger keeps the role wherever it goes (it began on a control)", {at(1, false), at(2, false)}, {1, 2}, {1, 2}},
        {"the world finger keeps the role over a finger listed before it", {at(1, false), at(2, false)}, {none, 2}, {none, 2}},
        {"the world finger slides onto a control, the controls being free: it takes them, the other the world", {at(1, true), at(2, false)}, {none, 1}, {1, 2}},
        {"the world finger slides onto a control while another holds them: it has no role, the world is empty", {at(1, true), at(2, true)}, {2, 1}, {2, none}},
        {"the controls finger lifted: a finger on a control takes the role", {at(2, true), at(3, false)}, {1, 3}, {2, 3}},
        {"the world finger lifted: the other finger in the world takes the role", {at(2, false), at(3, false)}, {none, 1}, {none, 2}},
        {"three: a third has no role", {at(1, true), at(2, false), at(3, false)}, {}, {1, 2}},
        {"from one to two: the lone finger was on a control (both); a second in the world takes the world", {at(1, true), at(2, false)}, {1, 1}, {1, 2}},
        {"from one to two: the lone finger was in the world; a second on a control takes the controls and the first keeps the world", {at(1, false), at(2, true)}, {none, 1}, {2, 1}},
    };
    for (const Row& row : rows) {
        const ContactRoles got = AssignContacts(row.live, row.before);
        CHECK_MSG(got == row.want, std::string(row.why) + ": controls " + std::to_string(got.controls) + ", world " + std::to_string(got.world) + " (wanted " +
                                       std::to_string(row.want.controls) + ", " + std::to_string(row.want.world) + ")");
    }
}

// `contacts=one|two` is a capture's choice of how many fingers are read, like the look and the arrangement: it parses alone or among the rest, is not a
// tuning, is refused on classic, and the shipped data reads two.
void TheContactsKeyIsACapturesChoiceAndNotATuning() {
    const Controls::Rules rules = LoadModern();
    CHECK_MSG(rules.twoContacts, "the shipped data reads two fingers (input.two_contacts)");
    Controls::FlagItems items;
    std::string error;
    CHECK(Controls::Tuning::ParseFlag("contacts=one", items, error) && items.twoContacts == std::optional<bool>(false) && items.Any() && !items.Tunes() && !items.preset && !items.arrows);
    CHECK(Controls::Tuning::ParseFlag("contacts=two", items, error) && items.twoContacts == std::optional<bool>(true) && items.Any() && !items.Tunes());
    CHECK(Controls::Tuning::ParseFlag("preset=mr,contacts=one,size=1.4", items, error) && items.twoContacts == std::optional<bool>(false) && items.preset == Controls::Preset::Mr && items.size == 1.4);
    CHECK(Controls::Tuning::ParseFlag("size=1.4", items, error) && !items.twoContacts);
    for (const char* text : {"contacts", "contacts=", "contacts=Two", "contacts=three", "contacts=1", "contacts=one,contacts=one", "contacts=two,contacts=one", "contact=one"}) {
        Controls::FlagItems refused;
        std::string why;
        CHECK_MSG(!Controls::Tuning::ParseFlag(text, refused, why) && why.find("--controls") != std::string::npos, std::string("'") + text + "' is refused with the usage: " + why);
    }
    CHECK_MSG(std::string(Controls::FlagUsage()).find("contacts=one|two") != std::string::npos, std::string("the usage names the key: ") + Controls::FlagUsage());
    // Applied: it tunes nothing, so a flag with only this key leaves the look and the kept tuning as they are, and nothing of it is ever written.
    Controls::FlagItems only;
    only.twoContacts = true;
    Controls::Tuning tuning;
    CHECK(Controls::Tuning::FromFlag(only, rules, tuning, error) && tuning.IsDefault(rules) && tuning.ToJson(rules) == "{}");
    // Through the policy: modern carries it; classic takes no keys at all.
    const Controls::Policy modern = Controls::ResolvePolicy("modern,contacts=one", false, false, false);
    CHECK_MSG(modern.valid && modern.available && modern.flag.twoContacts == std::optional<bool>(false) && !modern.flag.Tunes(), "--controls modern,contacts=one resolves and carries the key: " + modern.error);
    const Controls::Policy classic = Controls::ResolvePolicy("classic,contacts=one", false, false, false);
    CHECK_MSG(!classic.valid && classic.error.find("classic takes no keys") != std::string::npos, "--controls classic,contacts=one is refused: " + classic.error);
    const Controls::Policy none = Controls::ResolvePolicy("modern", false, false, false);
    CHECK_MSG(none.valid && !none.flag.twoContacts && !none.flag.Any(), "with no key the data decides");
}

void runTests() {
    ClassicHitEqualsTheHudFunctionsEverywhere();
    ClassicFramesAreTheHudsRectsExactly();
    ClassicWalkClaimEqualsTheTwoOnPadCalls();
    TheEndByteIsThePulsesByte();
    AWalkPadsClaimIsACircleOfSixtyFourUnitsAboutItsCorner();
    ThePadsSlideAndTheRingFollowsTheCorner();
    ACornerButtonsClaimIsItsDrawnRectangleAndAHalfOpenOneLosesItsFarEdges();
    AControlNotLaidOutYetClaimsNothing();
    TheCornerButtonsSitWhereUiJsonPutsThem();
    TheIdsAreInTheClaimOrderAndOnlyThePadsWalk();

    // The modern controls (planning step 180).
    TheModernRulesAreTheFilesNumbers();
    TheNamesAreOnePlaceAndRoundTrip();
    EveryNumberInControlsJsonSaysWhereItCameFrom();
    WhatIsRefused();
    TheDefaultPlacesArePinnedBesideTheLayoutNumber();
    ThePresetsKeepThePairTopAtOrBelowTheCharacterRow();
    TheSizeStepsEndExactlyOnTheLimits();
    MarginsAndGapsDoNotGrowWithSize();
    ACornerButtonNeverShrinksBelowItsMinimumSize();
    ThePlansSizeTableAndItsNarrowWindowRowsArePinned();
    NoTwoDefaultBoxesOverlapAtAnyAspectOrSize();
    TheSeamIsSplitAtItsMiddleAndNothingOverlaps();
    TheEdgeReachGoesToTheViewEdgeOnlyWithinTheReach();
    ThereIsNoGrowthTowardTheWorldByDefault();
    AHitRectNeverLeavesTheView();
    TheModernHudIsNoMoreThanOneAndAHalfTimesTheClassicsAtDefaultSize();
    TheSlideMovesEachArrowFromItsNearerEdgeAndTheHitFollows();
    TheCornerPlacementsGiveTheOriginalsEntranceAtTheInsetCorners();
    DefaultTuningIsTheBaseBitForBit();
    AMoveIsInUnitsFromTheAnchorAndNotScaledBySize();
    MoveForIsTheInverseOfPlace();
    ClampKeepsEveryBoxWhollyInsideTheSafeRectangle();
    SafeInsetsMoveTheDefaultsInward();
    TheDrawRectPutsEveryTiersBodyOnTheSameFace();
    TheOpacityIsAMultiplierOnTheByteAndTheDefaultIsUnchanged();
    TheRingFollowsTheArrowsCentre();
    HintLeftClearsThePairAndLeavesClassicAlone();
    TheHintsArrowEndsAboveThePairsHitRect();
    ResolvePolicyTruthTable();
    ResolveStyleTruthTable();
    TheDefaultsOfAnAbsentTuningFileAreTheBase();
    ATuningIsWrittenOnOneLineAndReadBackExactly();
    TheFlagGrammar();
    TheTuningKeysAreOnePlaceAndTheMembersSpliceIntoAnObject();
    ThePolicyCarriesTheSeamsTheCommandLineHasNoWordFor();
    APresetKeyIsParsedAndAnUnknownOneIsRefused();
    TheClassicStyleIsTheClassicFunctions();

    // The safe area (planning step 183).
    TheSafeAreaInPixelsBecomesInsetsInUnitsByThePointersFactor();

    // The pictures a modern run draws (planning step 184).
    ABareFileNameIsOneFileInOneFolder();
    TheArtManifestGivesTheSizesAPictureMustHave();
    TheArtLineSaysWhereEachPictureCameFrom();

    // The names hardened, the mix of pictures, and the saved choice (planning step 185).
    TheNamesWindowsKeepAreNotBare();
    TheFiveGeneratedNamesDiffer();
    ArtHalfDressedNeedsAMagicRampagePictureInAMix();
    ControlsStoreRoundTrips();
    TheFileHoldsDeltasOnly();
    AMissingFileIsTheShippedDefault();
    AnUnreadableFileIsDefaultedAndOverwrittenAtTheNextSave();
    AMemoryOnlyStoreWritesNothing();
    ALayoutNumberFromAnotherVersionDropsMovesAndKeepsSizeAndOpacity();
    ASaveThatCannotBeMadeSaysSoAndLeavesNothingHalfWritten();
    TheSwitchesStoreIsNotTouched();

    // The tuning, kept (planning step 187).
    TheTuningRoundTripsThroughTheFile();
    TheDefaultTuningWritesNoMoveAndNoSurprise();
    ADefaultThatChangesReachesAPlayerWhoNeverMovedASlider();
    AFileWithStrayLookKeysIsReadWithoutThem();
    TheFlagTunesOnlyWithASizeAnOpacityOrAMove();
    ASaveReplacesInOneStepAndNeverRemovesTheLiveFile();

    // A second finger (planning step 191).
    AssignContactsTruthTable();
    TheContactsKeyIsACapturesChoiceAndNotATuning();
}

} // namespace

MP_TEST_MAIN("test_mp_controls", 700)
