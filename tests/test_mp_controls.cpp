// The five on-screen controls' geometry: sim/Controls, the one place that says where each is
// drawn and where a touch counts as on it (planning step 179).
//
// The classic functions there FORWARD to sim/Hud's pad arithmetic, so a suite that only compared
// them with sim/Hud would pass whatever Hud said. Two kinds of check keep it honest. The
// equivalences walk a grid of points and ask the module and Hud the same question, because the
// layer now asks the module and the 128 sweeps, the claim map and the HUD digest were taken of
// Hud's answers; and the literals are written out in the units ui.json states them in (a hit
// circle of 64 units, a slide of 128 over 700 ms, the pulse's 163 at 1-01's door), so a change to
// either side, or to the data under both, fails here and not only in a level.
//
// Pure: no window, no registry, no level, no original art. ui.json is the port's own and
// committed, so this runs anywhere and never skips.

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "sim/Controls.hpp"
#include "sim/Hud.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <string>

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

const std::string kUi = PortDataDir() + "/ui.json";

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
}

} // namespace

MP_TEST_MAIN("test_mp_controls", 120)
