#include "sim/Controls.hpp"

namespace MagicPortals::Controls {

namespace {

// The Hud function's own name for a walk pad's side. Only ever asked of a walk pad.
Hud::Side SideOf(Id walk) {
    return walk == Id::Left ? Hud::Side::Left : Hud::Side::Right;
}

} // namespace

HitShape HitShape::Closed(const Hud::Rect& rect) {
    HitShape shape;
    shape.kind = Kind::Rect;
    shape.rect = rect;
    shape.halfOpen = false;
    return shape;
}

HitShape HitShape::HalfOpen(const Hud::Rect& rect) {
    HitShape shape = Closed(rect);
    shape.halfOpen = true;
    return shape;
}

HitShape HitShape::Disc(const glm::dvec2& centre, double radius) {
    HitShape shape;
    shape.kind = Kind::Disc;
    shape.centre = centre;
    shape.radius = radius;
    return shape;
}

bool HitShape::Contains(const glm::dvec2& point) const {
    if (kind == Kind::Disc) {
        // Strictly inside, written as Hud::OnPad writes it: the squared distance against the
        // squared radius, so a point exactly on the circle is off the pad and the decision is
        // the same one the original's isPointInSphere makes.
        const glm::dvec2 d = point - centre;
        return glm::dot(d, d) < radius * radius;
    }
    if (!halfOpen) return rect.Contains(point);
    const glm::dvec2 max = rect.Max();
    return point.x >= rect.min.x && point.y >= rect.min.y && point.x < max.x && point.y < max.y;
}

Frame ClassicWalkFrame(const Hud::Rules& rules, Id walk, const glm::dvec2& viewUnits, double ageMs) {
    Frame frame;
    if (!IsWalk(walk)) return frame;
    const Hud::Side side = SideOf(walk);
    frame.rect = Hud::PadRect(rules, side, viewUnits, ageMs);
    frame.hit = HitShape::Disc(Hud::PadCorner(rules, side, viewUnits, ageMs), rules.pads.hitRadiusUnits);
    return frame;
}

Frame ClassicCornerFrame(const Hud::Rect& drawn) {
    Frame frame;
    frame.rect = drawn;
    frame.hit = HitShape::Closed(drawn);
    return frame;
}

double ClassicSlideUnits(const Hud::Rules& rules, double ageMs) {
    return Hud::PadSlideUnits(rules, ageMs);
}

glm::dvec2 ClassicRingCentre(const Hud::Rules& rules, Id walk, const glm::dvec2& viewUnits, double ageMs) {
    if (!IsWalk(walk)) return glm::dvec2(0.0);
    return Hud::PadCorner(rules, SideOf(walk), viewUnits, ageMs);
}

bool ClassicOnWalkControl(const Hud::Rules& rules, const glm::dvec2& viewUnits, double ageMs, const glm::dvec2& point) {
    return Hud::OnPad(rules, Hud::Side::Left, viewUnits, ageMs, point) ||
           Hud::OnPad(rules, Hud::Side::Right, viewUnits, ageMs, point);
}

double ClassicWalkOpacity(const Hud::Rules& rules, double ageMs, bool tutorial) {
    return Hud::PadOpacity(rules, ageMs, tutorial);
}

int ClassicWalkAlphaByte(const Hud::Rules& rules, double ageMs, bool tutorial) {
    return Hud::PadAlphaByte(rules, ageMs, tutorial);
}

const Hud::Placement& ClassicPlacement(const Hud::Rules& rules, Id corner) {
    static const Hud::Placement none{};
    switch (corner) {
    case Id::Reset:
        return rules.restart;
    case Id::Menu:
        return rules.pause;
    case Id::Clear:
        return rules.clearPortals;
    case Id::Left:
    case Id::Right:
        break;
    }
    return none;
}

Hud::Rect ClassicPlace(const Hud::Rules& rules, Id corner, const glm::dvec2& viewUnits) {
    return Hud::Place(ClassicPlacement(rules, corner), viewUnits);
}

} // namespace MagicPortals::Controls
