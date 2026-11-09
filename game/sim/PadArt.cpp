#include "sim/PadArt.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace MagicPortals::PadArt {

namespace {

// ---- a little 2D arithmetic -------------------------------------------------------------------------------------------

struct V {
    double x = 0.0;
    double y = 0.0;
};

constexpr V operator+(V a, V b) {
    return {a.x + b.x, a.y + b.y};
}
constexpr V operator-(V a, V b) {
    return {a.x - b.x, a.y - b.y};
}
constexpr V operator*(V a, double s) {
    return {a.x * s, a.y * s};
}
constexpr double Dot(V a, V b) {
    return a.x * b.x + a.y * b.y;
}
constexpr double Cross(V a, V b) {
    return a.x * b.y - a.y * b.x;
}
double Length(V v) {
    return std::sqrt(Dot(v, v));
}

// ---- signed distances: negative inside, zero on the edge --------------------------------------------------------------

// A box of half-extent `half` about the origin whose corners are rounded by `radius`.
double RoundBox(V p, V half, double radius) {
    const V q{std::fabs(p.x) - (half.x - radius), std::fabs(p.y) - (half.y - radius)};
    return Length({std::max(q.x, 0.0), std::max(q.y, 0.0)}) + std::min(std::max(q.x, q.y), 0.0) - radius;
}

// The distance to the segment a-b: a capsule is "this at most its radius".
double Segment(V p, V a, V b) {
    const V pa = p - a;
    const V ba = b - a;
    const double h = std::clamp(Dot(pa, ba) / Dot(ba, ba), 0.0, 1.0);
    return Length(pa - ba * h);
}

// The signed distance to the triangle p0 p1 p2, whichever way round it is wound.
double Triangle(V p, V p0, V p1, V p2) {
    const V e0 = p1 - p0;
    const V e1 = p2 - p1;
    const V e2 = p0 - p2;
    const V v0 = p - p0;
    const V v1 = p - p1;
    const V v2 = p - p2;
    const V q0 = v0 - e0 * std::clamp(Dot(v0, e0) / Dot(e0, e0), 0.0, 1.0);
    const V q1 = v1 - e1 * std::clamp(Dot(v1, e1) / Dot(e1, e1), 0.0, 1.0);
    const V q2 = v2 - e2 * std::clamp(Dot(v2, e2) / Dot(e2, e2), 0.0, 1.0);
    const double winding = Cross(e0, e2) > 0.0 ? 1.0 : -1.0;
    // The nearest edge by distance (squared), and on which side of it the point is.
    double nearest = Dot(q0, q0);
    double side = winding * Cross(v0, e0);
    if (Dot(q1, q1) < nearest) nearest = Dot(q1, q1);
    if (Dot(q2, q2) < nearest) nearest = Dot(q2, q2);
    side = std::min({side, winding * Cross(v1, e1), winding * Cross(v2, e2)});
    return -std::sqrt(nearest) * (side > 0.0 ? 1.0 : -1.0);
}

// ---- the plate every glyph sits on ------------------------------------------------------------------------------------
//
// Body coordinates: the body is [-1, 1] on both axes, +y down. The plate is the body, its corners rounded; its rim is the
// outer slice of it.
constexpr V kBodyHalf{1.0, 1.0};
constexpr double kPlateCorner = 0.30;
constexpr double kRimWidth = 0.075;

// ---- the six shapes, each as "is this point on the glyph" -------------------------------------------------------------

// A triangle that points left, its corners rounded. Right is the same shape with x negated, which is how the two are
// mirror images by construction and not by a copy.
constexpr V kArrowA{-0.38, 0.0};
constexpr V kArrowB{0.30, -0.46};
constexpr V kArrowC{0.30, 0.46};
constexpr double kArrowRound = 0.10;

bool Arrow(V p) {
    return Triangle(p, kArrowA, kArrowB, kArrowC) <= kArrowRound;
}

// A clockwise circular arrow: a ring with a gap in its upper right, a round cap where the arc starts and a head where it
// ends. The gap's two edges and the head's axes are written out (75 and 15 degrees above the +x axis) so that no
// trigonometry runs: a sine is the one function whose last bit differs from one library to the next.
constexpr double kCos75 = 0.25881904510252074;
constexpr double kSin75 = 0.9659258262890683;
constexpr double kRingRadius = 0.46;
constexpr double kRingHalfWidth = 0.085;
constexpr double kRestartDown = 0.05; // the head rises above the ring, so the whole glyph sits this much lower
constexpr V kGapEdgeA{kCos75, -kSin75};
constexpr V kGapEdgeB{kSin75, -kCos75};
constexpr V kArcStart{kRingRadius * kSin75, -kRingRadius * kCos75};
constexpr V kArcEnd{kRingRadius * kCos75, -kRingRadius * kSin75};
constexpr V kArcTangent{kSin75, kCos75}; // clockwise on the screen, at the end
constexpr V kArcRadial{kCos75, -kSin75};
constexpr V kHeadTip = kArcEnd + kArcTangent * 0.27;
constexpr V kHeadBase = kArcEnd - kArcTangent * 0.04;
constexpr V kHeadA = kHeadBase + kArcRadial * 0.22;
constexpr V kHeadB = kHeadBase - kArcRadial * 0.22;

bool Restart(V p) {
    p.y -= kRestartDown;
    const bool inGap = Cross(kGapEdgeA, p) > 0.0 && Cross(p, kGapEdgeB) > 0.0;
    if (std::fabs(Length(p) - kRingRadius) <= kRingHalfWidth && !inGap) return true;
    if (Length(p - kArcStart) <= kRingHalfWidth) return true;
    return Triangle(p, kHeadTip, kHeadA, kHeadB) <= 0.0;
}

// Two upright bars.
bool Pause(V p) {
    return RoundBox({std::fabs(p.x) - 0.22, p.y}, {0.13, 0.42}, 0.06) <= 0.0;
}

// A ring with a cross inside it.
bool ClearPortals(V p) {
    if (std::fabs(Length(p) - 0.46) <= 0.06) return true;
    return Segment(p, {-0.19, -0.19}, {0.19, 0.19}) <= 0.075 || Segment(p, {-0.19, 0.19}, {0.19, -0.19}) <= 0.075;
}

// A hub with a hole and eight teeth. The teeth are at every 45 degrees, so a point is folded into the first eighth of
// the circle (to its absolute values, then swapped to put it under the diagonal) and tested against a tooth on the axis
// and one on the diagonal.
constexpr double kHalfRoot2 = 0.7071067811865476;

bool Gear(V p) {
    const double r = Length(p);
    if (r <= 0.16) return false;
    if (r <= 0.37) return true;
    double qx = std::fabs(p.x);
    double qy = std::fabs(p.y);
    if (qy > qx) std::swap(qx, qy);
    constexpr V kToothHalf{0.11, 0.10};
    constexpr double kToothRound = 0.03;
    constexpr double kToothCentre = 0.44;
    if (RoundBox({qx - kToothCentre, qy}, kToothHalf, kToothRound) <= 0.0) return true;
    const V onDiagonal{(qx + qy) * kHalfRoot2, (qy - qx) * kHalfRoot2};
    return RoundBox({onDiagonal.x - kToothCentre, onDiagonal.y}, kToothHalf, kToothRound) <= 0.0;
}

// A globe, for the language entry: a ring, an equator, two parallels and one meridian (an ellipse). The ellipse's distance is the first-order
// one, (f - 1) / |grad f| of f = (x/a)^2 + (y/b)^2, which is exact enough for a line a twelfth of the glyph wide, and needs only + - * / and sqrt.
constexpr double kGlobeRadius = 0.46;
constexpr double kGlobeLineHalf = 0.045;
constexpr double kGlobeParallel = 0.25;
constexpr double kGlobeMeridianA = 0.23; // the meridian's half width; its half height is the ring's radius

bool Globe(V p) {
    const double r = Length(p);
    if (std::fabs(r - kGlobeRadius) <= 0.06) return true;
    if (r > kGlobeRadius) return false;
    if (std::fabs(p.y) <= kGlobeLineHalf) return true;                                   // the equator
    if (std::fabs(std::fabs(p.y) - kGlobeParallel) <= kGlobeLineHalf * 0.9) return true; // the parallels, a touch finer
    const double f = (p.x * p.x) / (kGlobeMeridianA * kGlobeMeridianA) + (p.y * p.y) / (kGlobeRadius * kGlobeRadius);
    const double gx = 2.0 * p.x / (kGlobeMeridianA * kGlobeMeridianA);
    const double gy = 2.0 * p.y / (kGlobeRadius * kGlobeRadius);
    const double slope = std::sqrt(gx * gx + gy * gy);
    // The centre is where the gradient vanishes; the equator already covers it.
    return slope > 0.0 && std::fabs(f - 1.0) / slope <= kGlobeLineHalf;
}

bool OnGlyph(Glyph glyph, V p) {
    switch (glyph) {
    case Glyph::WalkLeft:
        return Arrow(p);
    case Glyph::WalkRight:
        return Arrow({-p.x, p.y});
    case Glyph::Restart:
        return Restart(p);
    case Glyph::Pause:
        return Pause(p);
    case Glyph::ClearPortals:
        return ClearPortals(p);
    case Glyph::Gear:
        return Gear(p);
    case Glyph::Globe:
        return Globe(p);
    }
    return false;
}

// What one sample of the picture is: nothing, or a colour.
struct Sample {
    bool covered = false;
    Rgb colour;
};

// The plate every picture sits on, at one sample: nothing outside it, the glyph where `onGlyph`, else its rim or its fill.
Sample PlateSample(V p, Rgb fill, Rgb rim, bool onGlyph) {
    const double plate = RoundBox(p, kBodyHalf, kPlateCorner);
    if (plate > 0.0) return {};
    if (onGlyph) return {true, kGlyphColour};
    return {true, plate > -kRimWidth ? rim : fill};
}

Sample SampleAt(Glyph glyph, V p) {
    return PlateSample(p, kPlateFill, kPlateRim, OnGlyph(glyph, p));
}

// ---- the eight editor shapes ------------------------------------------------------------------------------------------

// The padlock's body, and its shackle: a half ring on two legs when it is shut; lifted clear of the body, with only its right leg still
// reaching it, when it is open.
constexpr V kLockBodyCentre{0.0, 0.22};
constexpr V kLockBodyHalf{0.40, 0.30};
constexpr double kShackleRadius = 0.26;
constexpr double kShackleHalfWidth = 0.075;

bool LockBody(V p) {
    return RoundBox(p - kLockBodyCentre, kLockBodyHalf, 0.07) <= 0.0;
}

bool ShutShackle(V p) {
    const V centre{0.0, -0.08};
    const bool arc = std::fabs(Length(p - centre) - kShackleRadius) <= kShackleHalfWidth && p.y <= centre.y;
    const bool legs = std::fabs(std::fabs(p.x) - kShackleRadius) <= kShackleHalfWidth && p.y >= centre.y && p.y <= kLockBodyCentre.y - kLockBodyHalf.y;
    return arc || legs;
}

bool OpenShackle(V p) {
    const V centre{0.0, -0.28};
    const bool arc = std::fabs(Length(p - centre) - kShackleRadius) <= kShackleHalfWidth && p.y <= centre.y;
    const bool leg = std::fabs(p.x - kShackleRadius) <= kShackleHalfWidth && p.y >= centre.y && p.y <= kLockBodyCentre.y - kLockBodyHalf.y;
    return arc || leg;
}

bool OnEditorGlyph(EditorGlyph glyph, V p) {
    switch (glyph) {
    case EditorGlyph::SizeMinus: // a small square: the size comes down
        return RoundBox(p, {0.22, 0.22}, 0.06) <= 0.0;
    case EditorGlyph::SizePlus: // a large one: it goes up
        return RoundBox(p, {0.52, 0.52}, 0.10) <= 0.0;
    case EditorGlyph::OpacityMinus: // an empty square: the controls fade
        return std::fabs(RoundBox(p, {0.46, 0.46}, 0.10)) <= 0.06;
    case EditorGlyph::OpacityPlus: // a full one: they firm
        return RoundBox(p, {0.46, 0.46}, 0.10) <= 0.0;
    case EditorGlyph::LockLocked:
        return LockBody(p) || ShutShackle(p);
    case EditorGlyph::LockUnlocked:
        return LockBody(p) || OpenShackle(p);
    case EditorGlyph::Restore:
        return Restart(p);
    case EditorGlyph::Back:
        return Arrow(p);
    }
    return false;
}

Sample EditorSampleAt(EditorGlyph glyph, V p) {
    const bool open = glyph == EditorGlyph::LockUnlocked;
    return PlateSample(p, open ? kEditingFill : kPlateFill, open ? kEditingRim : kPlateRim, OnEditorGlyph(glyph, p));
}

constexpr int kGrid = 4; // samples a side of a pixel

} // namespace

const char* NameOf(Glyph glyph) {
    switch (glyph) {
    case Glyph::WalkLeft:
        return "walk_left";
    case Glyph::WalkRight:
        return "walk_right";
    case Glyph::Restart:
        return "restart";
    case Glyph::Pause:
        return "pause";
    case Glyph::ClearPortals:
        return "clear_portals";
    case Glyph::Gear:
        return "gear";
    case Glyph::Globe:
        return "globe";
    }
    return "?";
}

Pixel Bitmap::At(int x, int y) const {
    if (x < 0 || y < 0 || x >= size || y >= size) return Pixel{};
    const std::size_t at = (static_cast<std::size_t>(y) * static_cast<std::size_t>(size) + static_cast<std::size_t>(x)) * 4;
    return Pixel{rgba[at], rgba[at + 1], rgba[at + 2], rgba[at + 3]};
}

int BodyMarginPx(int size, double bodyInset) {
    return static_cast<int>(std::lround(static_cast<double>(size) * bodyInset));
}

namespace {

// Every picture of this file is rendered by this one function, from a sampler: `sample(p)` says what one sample at the body coordinate p is.
template <class Sampler>
Bitmap Render(int size, double bodyInset, const Sampler& sampleAt) {
    // Written so that a NaN fails the test and is refused.
    if (size < kMinSize || size > kMaxSize || !(bodyInset >= 0.0 && bodyInset <= kMaxBodyInset)) return {};

    const int margin = BodyMarginPx(size, bodyInset);
    const int body = size - 2 * margin;

    Bitmap bitmap;
    bitmap.size = size;
    bitmap.rgba.assign(static_cast<std::size_t>(size) * static_cast<std::size_t>(size) * 4, 0);

    // A sample's place in body coordinates is a whole number over 4 x body: sample `i` of the whole picture's 4 x size
    // lies at (i + 1/2) / 4 px, the body starts at `margin` px and is `body` px across, so
    //     p = ((2 i + 1) / 8 - margin) / body * 2 - 1 = (2 i + 1 - 8 margin - 4 body) / (4 body).
    // Mirroring the picture (i -> 4 size - 1 - i) negates the numerator exactly, so a mirrored pixel sees the mirror
    // image of the points, to the last bit.
    const double denominator = 4.0 * static_cast<double>(body);
    const auto coordinate = [&](int sampleIndex) {
        const long long numerator = 2LL * sampleIndex + 1 - 8LL * margin - 4LL * body;
        return static_cast<double>(numerator) / denominator;
    };

    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            int covered = 0;
            int sumR = 0;
            int sumG = 0;
            int sumB = 0;
            for (int sy = 0; sy < kGrid; ++sy) {
                for (int sx = 0; sx < kGrid; ++sx) {
                    const Sample sample = sampleAt(V{coordinate(kGrid * x + sx), coordinate(kGrid * y + sy)});
                    if (!sample.covered) continue;
                    ++covered;
                    sumR += sample.colour.r;
                    sumG += sample.colour.g;
                    sumB += sample.colour.b;
                }
            }
            if (covered == 0) continue; // (0, 0, 0, 0): a transparent pixel has no colour to bleed into a filter
            // The colour of the samples that are there, not diluted by the ones that are not: straight alpha.
            const std::size_t at = (static_cast<std::size_t>(y) * static_cast<std::size_t>(size) + static_cast<std::size_t>(x)) * 4;
            bitmap.rgba[at] = static_cast<std::uint8_t>((sumR + covered / 2) / covered);
            bitmap.rgba[at + 1] = static_cast<std::uint8_t>((sumG + covered / 2) / covered);
            bitmap.rgba[at + 2] = static_cast<std::uint8_t>((sumB + covered / 2) / covered);
            bitmap.rgba[at + 3] = static_cast<std::uint8_t>((covered * 255 + kGrid * kGrid / 2) / (kGrid * kGrid));
        }
    }
    return bitmap;
}

} // namespace

Bitmap Draw(Glyph glyph, int size, double bodyInset) {
    return Render(size, bodyInset, [glyph](V p) { return SampleAt(glyph, p); });
}

Bitmap DrawEditor(EditorGlyph glyph, int size, double bodyInset) {
    return Render(size, bodyInset, [glyph](V p) { return EditorSampleAt(glyph, p); });
}

const char* NameOf(EditorGlyph glyph) {
    switch (glyph) {
    case EditorGlyph::SizeMinus:
        return "size_minus";
    case EditorGlyph::SizePlus:
        return "size_plus";
    case EditorGlyph::OpacityMinus:
        return "opacity_minus";
    case EditorGlyph::OpacityPlus:
        return "opacity_plus";
    case EditorGlyph::LockLocked:
        return "lock_locked";
    case EditorGlyph::LockUnlocked:
        return "lock_unlocked";
    case EditorGlyph::Restore:
        return "restore";
    case EditorGlyph::Back:
        return "back";
    }
    return "?";
}

Glyph GlyphOf(Controls::Id id) {
    switch (id) {
    case Controls::Id::Left:
        return Glyph::WalkLeft;
    case Controls::Id::Right:
        return Glyph::WalkRight;
    case Controls::Id::Reset:
        return Glyph::Restart;
    case Controls::Id::Menu:
        return Glyph::Pause;
    case Controls::Id::Clear:
        return Glyph::ClearPortals;
    }
    return Glyph::Gear;
}

} // namespace MagicPortals::PadArt
