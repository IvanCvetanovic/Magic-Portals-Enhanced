#include "sim/Impacts.hpp"

#include "core/Components.hpp"
#include "core/DetMath.hpp"
#include "core/PhysicsSettings.hpp"
#include "sim/LevelBuilder.hpp"
#include "sim/Units.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace MagicPortals::Impacts {

namespace {

using glm::dvec2;
using Supersonic::BoxColliderComponent;
using Supersonic::CapsuleColliderComponent;
using Supersonic::ConvexHullColliderComponent;
using Supersonic::RigidBodyComponent;
using Supersonic::SphereColliderComponent;
using Supersonic::TransformComponent;

double Cross(const dvec2& a, const dvec2& b) { return a.x * b.y - a.y * b.x; }

// An entity's frame in the plane, in world metres (+y up), turned about z as the
// solver turns it; DetMath, as Shot's frame is, so every C runtime agrees.
struct Frame {
    dvec2 origin{0.0};
    double c = 1.0;
    double s = 0.0;

    explicit Frame(const TransformComponent& transform)
        : origin(transform.position.x, transform.position.y),
          c(Supersonic::DetMath::cos(transform.rotation.z)),
          s(Supersonic::DetMath::sin(transform.rotation.z)) {}

    // A point in the entity's frame, metres, to the port's pixels.
    dvec2 Px(const dvec2& local) const {
        const dvec2 world = origin + dvec2(c * local.x - s * local.y, s * local.x + c * local.y);
        return dvec2(world.x, -world.y) * Units::kPixelsPerMetre;
    }
};

dvec2 NearestOnSegment(const dvec2& p, const dvec2& a, const dvec2& b) {
    const dvec2 ab = b - a;
    const double lengthSquared = glm::dot(ab, ab);
    const double t = lengthSquared > 0.0 ? std::clamp(glm::dot(p - a, ab) / lengthSquared, 0.0, 1.0) : 0.0;
    return a + ab * t;
}

// Whether two segments pass through each other.
bool SegmentsCross(const dvec2& a0, const dvec2& a1, const dvec2& b0, const dvec2& b1) {
    const double d1 = Cross(a1 - a0, b0 - a0);
    const double d2 = Cross(a1 - a0, b1 - a0);
    const double d3 = Cross(b1 - b0, a0 - b0);
    const double d4 = Cross(b1 - b0, a1 - b0);
    return ((d1 > 0.0 && d2 < 0.0) || (d1 < 0.0 && d2 > 0.0)) && ((d3 > 0.0 && d4 < 0.0) || (d3 < 0.0 && d4 > 0.0));
}

// Whether a point is inside a convex polygon of three or more points, either way round.
bool Inside(const std::vector<dvec2>& polygon, const dvec2& p) {
    if (polygon.size() < 3) return false;
    int side = 0;
    for (std::size_t i = 0; i < polygon.size(); ++i) {
        const dvec2& a = polygon[i];
        const dvec2& b = polygon[(i + 1) % polygon.size()];
        const double c = Cross(b - a, p - a);
        if (c == 0.0) continue;
        const int now = c > 0.0 ? 1 : -1;
        if (side == 0) side = now;
        if (now != side) return false;
    }
    return true;
}

// A core's edges: a point is one edge of no length, a segment one edge, and a
// polygon its closed outline.
std::vector<std::pair<dvec2, dvec2>> Edges(const std::vector<dvec2>& core) {
    std::vector<std::pair<dvec2, dvec2>> edges;
    if (core.size() == 1) {
        edges.emplace_back(core[0], core[0]);
    } else if (core.size() == 2) {
        edges.emplace_back(core[0], core[1]);
    } else {
        for (std::size_t i = 0; i < core.size(); ++i) edges.emplace_back(core[i], core[(i + 1) % core.size()]);
    }
    return edges;
}

dvec2 Centre(const std::vector<dvec2>& core) {
    dvec2 sum(0.0);
    for (const dvec2& p : core) sum += p;
    return core.empty() ? sum : sum / static_cast<double>(core.size());
}

struct Solid {
    entt::entity entity = entt::null;
    Shape shape;
    dvec2 lo{0.0};
    dvec2 hi{0.0};
    std::uint32_t layer = 0;
    std::uint32_t collidesWith = 0;
};

template <class Collider>
void Masks(const Collider& collider, Solid& solid) {
    solid.layer = collider.layer;
    solid.collidesWith = collider.collidesWith;
}

bool SolidOf(entt::registry& registry, entt::entity entity, Solid& out) {
    out = Solid{};
    out.entity = entity;
    if (!ShapeOf(registry, entity, out.shape) || out.shape.core.empty()) return false;
    if (const auto* box = registry.try_get<BoxColliderComponent>(entity)) {
        Masks(*box, out);
    } else if (const auto* sphere = registry.try_get<SphereColliderComponent>(entity)) {
        Masks(*sphere, out);
    } else if (const auto* capsule = registry.try_get<CapsuleColliderComponent>(entity)) {
        Masks(*capsule, out);
    } else if (const auto* hull = registry.try_get<ConvexHullColliderComponent>(entity)) {
        Masks(*hull, out);
    }
    out.lo = out.hi = out.shape.core.front();
    for (const dvec2& p : out.shape.core) {
        out.lo = glm::min(out.lo, p);
        out.hi = glm::max(out.hi, p);
    }
    out.lo -= dvec2(out.shape.radiusPx);
    out.hi += dvec2(out.shape.radiusPx);
    return true;
}

// The pair as the solver would take it: the one's layer in the other's mask
// both ways round (PhysicsSystem's broadphase).
bool Collide(const Solid& a, const Solid& b) { return (a.collidesWith & b.layer) != 0 && (b.collidesWith & a.layer) != 0; }

bool Near(const Solid& a, const Solid& b, double touchPx) {
    return a.lo.x - touchPx <= b.hi.x && b.lo.x - touchPx <= a.hi.x && a.lo.y - touchPx <= b.hi.y &&
           b.lo.y - touchPx <= a.hi.y;
}

// Where Between found the two cores themselves meeting: no line between nearest
// points, only the one between the cores' centres.
bool CoresMeet(const Gap& gap, const Shape& a, const Shape& b) { return gap.px <= -(a.radiusPx + b.radiusPx); }

// The engine's gravity on `body` in a step, px/s^2, as PhysicsSystem::Update
// takes it: the world's (PhysicsSettings in the registry's context, or its
// defaults), on a body that is neither kinematic nor let off it.
double GravityPx(entt::registry& registry, entt::entity body) {
    const auto* rigid = registry.try_get<RigidBodyComponent>(body);
    if (rigid == nullptr || rigid->isKinematic || !rigid->useGravity) return 0.0;
    static const Supersonic::PhysicsSettings kDefaults;
    const Supersonic::PhysicsSettings* stored = registry.ctx().find<Supersonic::PhysicsSettings>();
    const glm::vec3 gravity = (stored != nullptr ? *stored : kDefaults).gravity;
    return glm::length(dvec2(gravity.x, gravity.y)) * Units::kPixelsPerMetre;
}

// What a step of `dtS` seconds adds to the velocity of `body`, as the engine's gravity does and GravityPx's
// magnitude says: px a second, +y down, none for a body that is kinematic or let off gravity.
dvec2 GravityStepPx(entt::registry& registry, entt::entity body, double dtS) {
    const auto* rigid = registry.try_get<RigidBodyComponent>(body);
    if (rigid == nullptr || rigid->isKinematic || !rigid->useGravity) return dvec2(0.0);
    static const Supersonic::PhysicsSettings kDefaults;
    const Supersonic::PhysicsSettings* stored = registry.ctx().find<Supersonic::PhysicsSettings>();
    const glm::vec3 gravity = (stored != nullptr ? *stored : kDefaults).gravity;
    return dvec2(gravity.x, -gravity.y) * Units::kPixelsPerMetre * dtS;
}

// Whether a step left the body where its velocity alone takes it, and gave it what gravity does: the float state
// of the engine's integration, not a game number (a velocity of 1000 px/s is 1e-4 px/s of rounding, a step's
// gravity 33 px/s, and a brake is as much as the approach it takes out). The two guesses of the replay - how far past
// its reach the engine still brakes a pair, and how much of a change may lie along the surfaces - are FreeFlightRules',
// from earthquake.json.
constexpr double kFreePositionPx = 0.01;
constexpr double kFreeVelocityPxS = 0.05;

} // namespace

bool ShapeOf(entt::registry& registry, entt::entity entity, Shape& out) {
    out = Shape{};
    const auto* transform = registry.try_get<TransformComponent>(entity);
    if (transform == nullptr) return false;
    const Frame frame(*transform);
    if (const auto* box = registry.try_get<BoxColliderComponent>(entity)) {
        if (box->isTrigger) return false;
        const dvec2 centre(box->center.x, box->center.y);
        const dvec2 half(box->size.x * 0.5, box->size.y * 0.5);
        for (const dvec2 corner : {dvec2(-1.0, -1.0), dvec2(1.0, -1.0), dvec2(1.0, 1.0), dvec2(-1.0, 1.0)}) {
            out.core.push_back(frame.Px(centre + half * corner));
        }
        return true;
    }
    if (const auto* sphere = registry.try_get<SphereColliderComponent>(entity)) {
        if (sphere->isTrigger) return false;
        out.core.push_back(frame.Px(dvec2(sphere->center.x, sphere->center.y)));
        out.radiusPx = static_cast<double>(sphere->radius) * Units::kPixelsPerMetre;
        return true;
    }
    if (const auto* capsule = registry.try_get<CapsuleColliderComponent>(entity)) {
        if (capsule->isTrigger) return false;
        // Its height is the whole capsule, caps included (Trigger.cpp reads it the same way).
        const double reach = std::max(static_cast<double>(capsule->height) * 0.5 - capsule->radius, 0.0);
        const dvec2 centre(capsule->center.x, capsule->center.y);
        out.core.push_back(frame.Px(centre - dvec2(0.0, reach)));
        out.core.push_back(frame.Px(centre + dvec2(0.0, reach)));
        out.radiusPx = static_cast<double>(capsule->radius) * Units::kPixelsPerMetre;
        return true;
    }
    const auto* hull = registry.try_get<ConvexHullColliderComponent>(entity);
    const auto* outline = registry.try_get<LevelBuilder::PlanePolygon>(entity);
    if (hull == nullptr || outline == nullptr || hull->isTrigger || outline->points.empty()) return false;
    for (const glm::vec2& point : outline->points) out.core.push_back(frame.Px(dvec2(point)));
    return true;
}

Gap Between(const Shape& a, const Shape& b) {
    Gap gap;
    if (a.core.empty() || b.core.empty()) {
        gap.px = std::numeric_limits<double>::infinity();
        return gap;
    }
    const auto edgesA = Edges(a.core);
    const auto edgesB = Edges(b.core);
    bool overlap = Inside(b.core, a.core.front()) || Inside(a.core, b.core.front());
    for (std::size_t i = 0; !overlap && i < edgesA.size(); ++i) {
        for (std::size_t j = 0; !overlap && j < edgesB.size(); ++j) {
            overlap = SegmentsCross(edgesA[i].first, edgesA[i].second, edgesB[j].first, edgesB[j].second);
        }
    }
    if (!overlap) {
        // Two convex sets apart: their nearest pair is a corner of one against an
        // edge of the other.
        double best = std::numeric_limits<double>::infinity();
        dvec2 onA(0.0);
        dvec2 onB(0.0);
        for (const dvec2& p : a.core) {
            for (const auto& [e0, e1] : edgesB) {
                const dvec2 q = NearestOnSegment(p, e0, e1);
                const double d = glm::length(q - p);
                if (d < best) {
                    best = d;
                    onA = p;
                    onB = q;
                }
            }
        }
        for (const dvec2& p : b.core) {
            for (const auto& [e0, e1] : edgesA) {
                const dvec2 q = NearestOnSegment(p, e0, e1);
                const double d = glm::length(q - p);
                if (d < best) {
                    best = d;
                    onA = q;
                    onB = p;
                }
            }
        }
        if (best > 1e-9) {
            gap.px = best - a.radiusPx - b.radiusPx;
            gap.normal = (onB - onA) / best;
            return gap;
        }
    }
    // The cores themselves meet: deeper than both radii, and touching whichever
    // way it is taken.
    gap.px = -(a.radiusPx + b.radiusPx);
    const dvec2 apart = Centre(b.core) - Centre(a.core);
    const double length = glm::length(apart);
    gap.normal = length > 0.0 ? apart / length : dvec2(0.0, 1.0);
    return gap;
}

namespace {

// The velocity Box2D's callback reads of a body that came to `other` in a free flight (Watched::freeFlight), which
// `velocities` and `centres` - newest first, the velocity it carried into each step and where that step began, back
// to when it last met or left something - say it did.
//
// Box2D steps the world (b2World.cpp:879-930) by finding each contact's manifold at the positions the last step
// left and calling BeginContact on a pair just come within its skin (b2Contact.cpp:154-229), at once and with the
// body's velocity as that last step's Solve left it (ETHContactListener.cpp:86-105, ETHPhysicsController.cpp:117-123):
// v += g dt, then x += v dt, and no braking. So the callback reads the velocity after the first step k whose FREE
// position is within the skin of the other - a free fall from rest reads k g dt, 0.333 a step in hitIntensity.
// The engine's solver instead moves the body first and then takes a closing pair's approach out before the pair
// meets, in stages, so the history holds a speed from a step or two earlier and the begin comes later; but the
// engine's integration is the same as Box2D's, so up to the first step whose end it altered the body's path is the
// free flight's, and the rest of it can be replayed against where the other stands.
//
// Steps are told apart by what they did: a step is free where it left the body at its centre plus its velocity
// (after gravity) times dt, and carried that velocity on. One that moved it as a free step does and carried a
// different velocity on was altered at its end - the position is the free flight's still, the solver having moved the
// body before it changed it - and it is the engine's BRAKE of this pair where the pair was inside its reach then
// (PhysicsSystem.cpp:1142-1164: a pair apart but closer than the two bodies' sweeps) and what it took away was
// approach toward the other (`normal`, from the body to it), and no more than a friction cone's worth of anything
// else (`rules.brakeFrictionCone` and the reach's slack are guesses, in earthquake.json). Any other step - one that
// left the body elsewhere (a glancing contact the watch never saw touch, a push-out), or a velocity changed by
// something else (a contact with another body, a gravity well's pull) - is a push the body took as it was: the state
// it carried on is real, and a free flight begins again from it. A body pushed on the step before the begin's own,
// which says nothing of whether that one was free as well, is not replayed (false).
//
// The scan stops at the OLDEST brake, so a push that comes AFTER it is not looked at: the replay flies on past it with
// the engine's gravity alone. A stone a gravity well pulls after its first brake (4-21's, tick 159: 1.21 replayed where
// the history read 2.20, neither checked against the well's script) is replayed as if nothing had pulled it (STK9-8). A
// push BEFORE the brake is harmless: the replay starts from the braked step's own velocity.
//
// The replay is from the start of the oldest braked step: one free step after another, each carrying `gravityStepPx`
// more, up to and including the begin's own step, and ends at the first whose position has the body's surface within
// `skinPx` of the other's - the one Box2D's Collide would have found touching - or at the begin's own step, which
// is the only one where nothing braked it. `replayed` is how many steps the begin came after the braked one.
bool FreeFlightVelocity(const std::vector<dvec2>& velocities, const std::vector<dvec2>& centres,
                        const dvec2& gravityStepPx, double dtS, const Shape& self, const dvec2& selfAtPx, const Shape& other,
                        double otherSpeedPxS, double skinPx, const FreeFlightRules& rules, const dvec2& normal, dvec2& out,
                        int& replayed) {
    const std::size_t steps = velocities.size();
    if (steps == 0 || centres.size() != steps || !(glm::dot(gravityStepPx, gravityStepPx) > 0.0)) return false;
    const auto placed = [&](const dvec2& centre) {
        Shape ghost = self;
        for (dvec2& point : ghost.core) point += centre - selfAtPx;
        return ghost;
    };
    std::size_t braked = 0;  // the oldest step the other's brake altered; the begin's own, none
    std::size_t pushed = 0;  // the newest step something else altered; 0, none
    bool brakedIt = false;
    for (std::size_t i = steps - 1; i > 0; --i) {
        const dvec2 flown = velocities[i] + gravityStepPx;
        if (glm::length(centres[i] + flown * dtS - centres[i - 1]) > kFreePositionPx) {
            pushed = i;
            continue;
        }
        const dvec2 changed = velocities[i - 1] - flown;
        if (!(glm::length(changed) > kFreeVelocityPxS)) continue;
        const double closing = glm::dot(changed, normal);
        bool brake = closing < 0.0 && glm::length(changed - normal * closing) <= -closing * rules.brakeFrictionCone;
        if (brake) {
            // Inside the reach the engine brakes a pair from: the two bodies' travel in the step, the other's a step
            // more for each since (it has moved), and the guessed slack of earthquake.json's impacts.free_flight.
            const double reachPx =
                glm::length(flown) * dtS + otherSpeedPxS * dtS * static_cast<double>(i + 1) + rules.brakeReachSlackPx;
            brake = Between(placed(centres[i - 1]), other).px <= reachPx;
        }
        if (brake) {
            braked = i;
            brakedIt = true;
            break;
        }
        pushed = i;
    }
    if (!brakedIt && pushed == 1) return false;
    dvec2 centre = centres[braked];
    for (std::size_t after = 0; after <= braked; ++after) {
        // After the step `after` places on from the braked one: carried gravity's `after + 1` steps.
        const dvec2 velocity = velocities[braked] + gravityStepPx * static_cast<double>(after + 1);
        centre += velocity * dtS;
        if (after == braked || Between(placed(centre), other).px <= skinPx) {
            out = velocity;
            replayed = static_cast<int>(braked);
            return true;
        }
    }
    return false;
}

} // namespace

glm::dvec2 Begin::Approaching(const glm::dvec2& along) const {
    return freeFlight ? freeFlightPx : HistoryApproaching(along);
}

glm::dvec2 Begin::HistoryApproaching(const glm::dvec2& along) const {
    if (recentPx.empty()) return velocityPx;
    glm::dvec2 best = recentPx.front();
    for (const glm::dvec2& velocity : recentPx) {
        if (glm::dot(velocity, along) > glm::dot(best, along)) best = velocity;
    }
    return best;
}

void Watch::BeforeStep(entt::registry& registry, const std::vector<Watched>& bodies) {
    std::vector<Pre> pre;
    for (const Watched& watched : bodies) {
        if (watched.body == entt::null || !registry.valid(watched.body)) continue;
        const auto* transform = registry.try_get<TransformComponent>(watched.body);
        const auto* rigid = registry.try_get<RigidBodyComponent>(watched.body);
        if (transform == nullptr || rigid == nullptr) continue;
        Pre now;
        now.body = watched.body;
        now.kind = watched.kind;
        now.velocityPx = Units::ToPixels(rigid->velocity);
        now.velocityScale = watched.velocityScale;
        now.centrePx = Units::ToPixels(transform->position);
        now.atPx = now.centrePx + watched.pointOffsetPx;
        now.pointOffsetPx = watched.pointOffsetPx;
        const auto was = std::find_if(m_pre.begin(), m_pre.end(), [&now](const Pre& p) { return p.body == now.body; });
        // Moved since the step left it: nothing but a teleport writes a watched
        // body's transform in the tick, and nothing else touches it, so any
        // change at all is one.
        const bool put = was != m_pre.end() && was->stepped && now.centrePx != was->afterPx;
        // The spawn drop: taken by a body the watch has never looked at, carried
        // while it holds, and over once the body is put somewhere.
        if (was != m_pre.end()) {
            now.dropPx = put ? dvec2(0.0) : was->dropPx;
            now.standing = !put && was->standing;
            now.standYPx = was->standYPx;
            now.stepped = was->stepped;
            now.afterPx = was->afterPx;
        } else if (!std::binary_search(m_seen.begin(), m_seen.end(), now.body)) {
            now.dropPx = watched.spawnDropPx;
        }
        // This step's velocity first, then what it carried into the steps before.
        // Where the original's stands on its floor while the port's comes down to
        // it, the original's carries none of that fall.
        now.recentPx.push_back(now.standing ? dvec2(now.velocityPx.x, 0.0) : now.velocityPx);
        now.recentCentresPx.push_back(now.centrePx);
        now.freeFlight = watched.freeFlight;
        if (was != m_pre.end() && !put) {
            for (std::size_t i = 0; i < was->recentPx.size() && i < was->recentCentresPx.size(); ++i) {
                if (now.recentPx.size() >= kRecentSteps) break;
                now.recentPx.push_back(was->recentPx[i]);
                now.recentCentresPx.push_back(was->recentCentresPx[i]);
            }
        }
        now.readPx = now.recentPx;
        pre.push_back(now);
    }
    m_pre = std::move(pre);
}

std::vector<Begin> Watch::AfterStep(entt::registry& registry, double touchPx, double dtS, const FreeFlightRules& flight) {
    std::vector<Begin> begins;
    // Every solid collider, once.
    std::vector<Solid> solids;
    const auto gather = [&](entt::entity entity) {
        Solid solid;
        if (SolidOf(registry, entity, solid)) solids.push_back(std::move(solid));
    };
    for (const entt::entity entity : registry.view<TransformComponent, BoxColliderComponent>()) gather(entity);
    for (const entt::entity entity : registry.view<TransformComponent, SphereColliderComponent>()) {
        if (!registry.all_of<BoxColliderComponent>(entity)) gather(entity);
    }
    for (const entt::entity entity : registry.view<TransformComponent, CapsuleColliderComponent>()) {
        if (!registry.any_of<BoxColliderComponent, SphereColliderComponent>(entity)) gather(entity);
    }
    for (const entt::entity entity :
         registry.view<TransformComponent, ConvexHullColliderComponent, LevelBuilder::PlanePolygon>()) {
        if (!registry.any_of<BoxColliderComponent, SphereColliderComponent, CapsuleColliderComponent>(entity)) {
            gather(entity);
        }
    }

    std::vector<Pair> touching;
    std::vector<entt::entity> seen;
    for (Pre& pre : m_pre) {
        if (!registry.valid(pre.body)) continue;
        const auto real = std::find_if(solids.begin(), solids.end(), [&](const Solid& s) { return s.entity == pre.body; });
        if (real == solids.end()) continue;
        const dvec2 nowCentre = Units::ToPixels(registry.get<TransformComponent>(pre.body).position);
        pre.stepped = true;
        pre.afterPx = nowCentre;
        // Put somewhere between the tick that steered it and the step - farther
        // than the step could carry it, its own radius of push-out allowed - it
        // is looked at afresh where it now is, and what it carried before was
        // carried elsewhere. (A portal moves a body after the step, before the
        // next is steered: BeforeStep finds a teleport.)
        double reachPx = glm::length(pre.velocityPx) * dtS + real->shape.radiusPx + touchPx;
        // A crate's box has no radius, and the step's own gravity carries it
        // g dt^2 past its velocity going in - 0.556 px at the layer's 2000 px/s^2 and 60 Hz,
        // over the touching distance - so without that every step of a fall was
        // a put: it lost the speed it fell at, and the step that brings it to its
        // floor, taken from rest after the solver's brake, was never looked at
        // (planning step 142's fix round). The round bodies' begins stay as
        // steps 101-104 measured them.
        if (pre.kind == Kind::Crate) reachPx += GravityPx(registry, pre.body) * dtS * dtS;
        const bool carried = glm::length(pre.centrePx - nowCentre) <= reachPx;
        if (!carried) {
            pre.recentPx.assign(1, pre.velocityPx);
            pre.recentCentresPx.assign(1, pre.centrePx);
            pre.dropPx = dvec2(0.0);
            pre.standing = false;
        }
        pre.readPx = pre.recentPx;
        const bool looked = carried && std::binary_search(m_seen.begin(), m_seen.end(), pre.body);
        // Touching nothing since its velocities began: the contact set is what clears them (below), and a
        // body in one is not in the free flight the replay needs.
        const bool airborne = std::none_of(m_touching.begin(), m_touching.end(),
                                           [&pre](const Pair& pair) { return pair.first == pre.body; });
        seen.push_back(pre.body);
        // Where the watch looks at it: its own shape, or while its spawn drop
        // holds, that shape where the original's body stands - which, once it
        // stands on something, stays there while the port's comes down to it.
        if (pre.standing) pre.dropPx.y = std::max(pre.standYPx - nowCentre.y, 0.0);
        Solid self = *real;
        const bool dropped = pre.dropPx != dvec2(0.0);
        if (dropped) {
            for (dvec2& point : self.shape.core) point += pre.dropPx;
            self.lo += pre.dropPx;
            self.hi += pre.dropPx;
        }
        bool reached = false; // its own shape has reached a pair the dropped one holds
        bool stood = false;   // the dropped shape is on something under it
        for (const Solid& other : solids) {
            if (other.entity == pre.body || !Collide(self, other) || !Near(self, other, touchPx)) continue;
            const Gap gap = Between(self.shape, other.shape);
            if (!(gap.px <= touchPx)) continue;
            const Pair pair(pre.body, other.entity);
            touching.push_back(pair);
            if (dropped && gap.normal.y > 0.0) stood = true;
            if (dropped && Near(*real, other, touchPx) && Between(real->shape, other.shape).px <= touchPx) reached = true;
            if (!looked || std::binary_search(m_touching.begin(), m_touching.end(), pair)) continue;
            Begin begin;
            begin.body = pre.body;
            begin.kind = pre.kind;
            begin.other = other.entity;
            begin.normal = gap.normal;
            // A crate's callback reads the contact's normal, the face it met
            // (crateContactCallback #6-29). Where the step has taken its box into
            // the other's - the solver lets a resting pair overlap by its slop -
            // Between has only the line between the two centres, which over a wide
            // floor leans far from the face; looked at where the step found it,
            // still apart, the line between their nearest points is that face's.
            if (pre.kind == Kind::Crate && CoresMeet(gap, self.shape, other.shape)) {
                Shape before = self.shape;
                for (dvec2& point : before.core) point += pre.centrePx - nowCentre;
                const Gap apart = Between(before, other.shape);
                if (!CoresMeet(apart, before, other.shape)) begin.normal = apart.normal;
            }
            begin.velocityPx = pre.velocityPx * pre.velocityScale;
            begin.atPx = pre.atPx + pre.dropPx;
            begin.gapPx = gap.px;
            begin.recentPx = pre.recentPx;
            for (dvec2& velocity : begin.recentPx) velocity *= pre.velocityScale;
            // A body that came to the pair through the air is read as Box2D's step reads it, not as the engine's solver
            // left its history. The skin is a polygon's (touchPx) against anything but another circle's; a polygon
            // against a polygon has two of them, which no body that is replayed - a circle - is.
            if (pre.freeFlight && pre.dropPx == dvec2(0.0) && airborne) {
                const double skinPx = (real->shape.radiusPx > 0.0 && other.shape.radiusPx > 0.0) ? flight.circleTouchPx : touchPx;
                dvec2 velocity(0.0);
                int replayed = 0;
                // How fast the other travelled in the steps the replay looks back over: what it carried into this one if the
                // watch has it (the step's own contact has since thrown it about), else the velocity it has.
                const auto* otherRigid = registry.try_get<RigidBodyComponent>(other.entity);
                double otherSpeedPxS = otherRigid != nullptr ? glm::length(Units::ToPixels(otherRigid->velocity)) : 0.0;
                for (const Pre& watched : m_pre) {
                    if (watched.body == other.entity) otherSpeedPxS = glm::length(watched.velocityPx);
                }
                if (FreeFlightVelocity(pre.recentPx, pre.recentCentresPx, GravityStepPx(registry, pre.body, dtS), dtS,
                                       real->shape, nowCentre, other.shape, otherSpeedPxS, skinPx, flight, begin.normal,
                                       velocity, replayed)) {
                    begin.freeFlight = true;
                    begin.freeFlightPx = velocity * pre.velocityScale;
                    begin.replayedSteps = replayed;
                }
            }
            begin.otherAtPx = Units::ToPixels(registry.get<TransformComponent>(other.entity).position);
            // A watched body is aimed at by its own point, as the original's
            // GetPositionXY is its entity's.
            for (const Pre& watched : m_pre) {
                if (watched.body == other.entity) begin.otherAtPx += watched.pointOffsetPx;
            }
            begins.push_back(begin);
        }
        // The original's has met what is under it, and stays there.
        if (stood && !pre.standing) {
            pre.standing = true;
            pre.standYPx = nowCentre.y + pre.dropPx.y;
        }
        // Its own shape has come down onto what the original's was on: from the
        // next step it is looked at as it is, and that pair goes on touching.
        if (reached || (pre.standing && pre.dropPx.y <= 0.0)) {
            pre.dropPx = dvec2(0.0);
            pre.standing = false;
        }
    }
    std::sort(touching.begin(), touching.end());
    // A body that has met or left something starts its velocities afresh: what
    // it carried toward that is spent, and is not what it carries toward the next.
    for (Pre& pre : m_pre) {
        const auto changed = [&pre](const std::vector<Pair>& from, const std::vector<Pair>& in) {
            return std::any_of(from.begin(), from.end(), [&](const Pair& pair) {
                return pair.first == pre.body && !std::binary_search(in.begin(), in.end(), pair);
            });
        };
        if (changed(m_touching, touching) || changed(touching, m_touching)) {
            pre.recentPx.clear();
            pre.recentCentresPx.clear();
        }
    }
    std::sort(seen.begin(), seen.end());
    m_touching = std::move(touching);
    m_seen = std::move(seen);
    return begins;
}

bool Watch::Carried(entt::entity body, Begin& out) const {
    for (const Pre& pre : m_pre) {
        if (pre.body != body) continue;
        out.body = pre.body;
        out.kind = pre.kind;
        out.velocityPx = pre.velocityPx * pre.velocityScale;
        out.atPx = pre.atPx;
        out.recentPx = pre.readPx;
        for (dvec2& velocity : out.recentPx) velocity *= pre.velocityScale;
        return true;
    }
    return false;
}

bool Watch::Touching(entt::entity body, entt::entity other) const {
    return std::binary_search(m_touching.begin(), m_touching.end(), Pair(body, other));
}

void Watch::Reset() {
    m_pre.clear();
    m_touching.clear();
    m_seen.clear();
}

} // namespace MagicPortals::Impacts
