#pragma once

// The five on-screen controls, and the one place that says where each is drawn and
// where a touch counts as on it.
//
// WHY A MODULE. Before this the pads' geometry was asked of sim/Hud in six places of the
// layer: the draw, the claim, the camera's `onPad`, the lift gate, the tutorial hand's gate
// and the ring - each calling Hud::PadRect, PadCorner or OnPad with the same four
// arguments, and the corner buttons told apart from clear-portals by the address of their
// rules. A second layout (the modern one, docs/planning/2026-10-15-modern-controls.md) would
// have had to be threaded through all six, and one left behind is a pad that draws in one
// place and presses in another. Here they all ask once.
//
// WHAT IS HERE. The classic functions only (planning step 179): each CALLS the Hud function
// it stands for and returns what that returned, so the original's arithmetic is not written
// a second time and cannot drift by an ulp from the one the 128 sweeps and the claim-map
// digest pin. `Hud.*` and ui.json are untouched. There is no style yet and no second layout;
// a later step adds `Style` and its functions beside these, and the names say which they are.
//
// Pure: no registry, no window, no file. Units are the HUD's design units, in VIEW space,
// (0, 0) the view's top-left, +y down.

#include <array>

#include <glm/glm.hpp>

#include "sim/Hud.hpp"

namespace MagicPortals::Controls {

// The controls, in the order a touch is claimed: where two of them could both be under a
// point, the earlier wins (a walk pad over a corner button, restart over pause over clear).
enum class Id { Left, Right, Reset, Menu, Clear };

inline constexpr std::array<Id, 5> kClaimOrder = {Id::Left, Id::Right, Id::Reset, Id::Menu, Id::Clear};

// The two walk pads. Everything else is a corner button, which a press works and which is
// placed by a Hud::Placement rather than hung from a screen corner.
constexpr bool IsWalk(Id id) {
    return id == Id::Left || id == Id::Right;
}

// Where a touch counts as on a control. A pad's is a disc about its screen corner - strictly
// inside, as the original's isPointInSphere - and a corner button's is its drawn rectangle,
// closed on every edge, as Hud::Rect::Contains. A rectangle may also be half-open
// ([min, max): a point on a shared edge belongs to the later control), which the classic
// controls never use.
//
// The default shape is an empty disc, so a control that has not been laid out yet claims
// nothing, not the one point (0, 0) a zero-sized closed rectangle would.
struct HitShape {
    enum class Kind { Rect, Disc };

    Kind kind = Kind::Disc;
    Hud::Rect rect;               // Rect: the area
    bool halfOpen = false;        // Rect: the max edges are outside
    glm::dvec2 centre{0.0};       // Disc
    double radius = 0.0;          // Disc: points strictly closer than this are inside

    static HitShape Closed(const Hud::Rect& rect);
    static HitShape HalfOpen(const Hud::Rect& rect);
    static HitShape Disc(const glm::dvec2& centre, double radius);

    bool Contains(const glm::dvec2& point) const;
};

// One control as laid out for a view and a level's age: the rectangle it is drawn in and the
// area a touch on it counts in. Whether it is shown, and at what alpha, are the layer's to
// say - they depend on the level's state, not on where a control sits.
struct Frame {
    Hud::Rect rect;
    HitShape hit;
};

// ---- the classic controls ---------------------------------------------------------------

// A walk pad, slid in as far as the level's age says: Hud::PadRect for the drawn rectangle
// and a disc of Hud::Rules::Pads::hitRadiusUnits about Hud::PadCorner for the claim. For
// an id that is not a walk pad, an empty frame.
Frame ClassicWalkFrame(const Hud::Rules& rules, Id walk, const glm::dvec2& viewUnits, double ageMs);

// A corner button's frame wherever it is drawn - flat in its corner, coming in, or being
// dismissed: the claim is the rectangle it is drawn in.
Frame ClassicCornerFrame(const Hud::Rect& drawn);

// How far outside its corner a pad still is: Hud::PadSlideUnits.
double ClassicSlideUnits(const Hud::Rules& rules, double ageMs);

// The point a walk pad's ring is centred on: the pad's screen corner, sliding with it
// (Hud::PadCorner). Not the middle of the drawn square. A zero point for any other id.
glm::dvec2 ClassicRingCentre(const Hud::Rules& rules, Id walk, const glm::dvec2& viewUnits, double ageMs);

// Whether a point is on EITHER walk pad, whether or not the pads are shown: Hud::OnPad
// for Left, then for Right. The camera's `onPad`, the lift gate and the tutorial hand's gate
// all read this, and it is not gated by the pads being drawn - a touch resting in the corner
// of a weightless level, which has no pads, still counts as on one.
bool ClassicOnWalkControl(const Hud::Rules& rules, const glm::dvec2& viewUnits, double ageMs, const glm::dvec2& point);

// The pads' alpha as a fraction and as the byte it is made from (Hud::PadOpacity and
// Hud::PadAlphaByte). The byte is what a level's end decays the pads from, so the pulse and
// the decay share one function and cannot start a byte apart.
double ClassicWalkOpacity(const Hud::Rules& rules, double ageMs, bool tutorial);
int ClassicWalkAlphaByte(const Hud::Rules& rules, double ageMs, bool tutorial);

// A corner button's placement: restart, pause and clear-portals as ui.json has them. An
// empty placement for a walk pad, which has none.
const Hud::Placement& ClassicPlacement(const Hud::Rules& rules, Id corner);

// The rectangle a corner button sits in once it is in place (Hud::Place of its placement).
Hud::Rect ClassicPlace(const Hud::Rules& rules, Id corner, const glm::dvec2& viewUnits);

} // namespace MagicPortals::Controls
