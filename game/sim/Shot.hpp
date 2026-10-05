#pragma once

// The portal shot's geometry: what a segment in the plane meets first.
//
// The owner, 11 September: tapping fires a shot from the character toward the
// tap, and the portal opens where the shot arrives. A wall on the way stops it
// and no portal opens. So does a crate or a stone: "it would hit them before
// reaching the tapped location". A failed shot costs nothing, "since it would
// never appear". The original's script names the parts: portal_launch,
// computeProjectileOrigin, projectileDestiny, computePortalFinalPos,
// destroyOnStaticHit. Portals flies the shot; this is the test it flies by.
//
// The test is the port's own and exact in the plane, as Trigger's is, because
// the engine's queries see a box or a hull only by its bounds. It reads box
// and sphere colliders, turned with their entity, and the polygon LevelBuilder
// records for each hull (LevelBuilder::PlanePolygon). Every solid body stops a
// shot, whether static, moving or loose. A trigger never does, and neither does
// the character the shot leaves from, nor what the caller names beside it (a
// gravity well's agent: 00_order R2). A capsule, which only the character has,
// is not read.

#include "sim/Trigger.hpp"

#include <optional>
#include <span>
#include <string>
#include <vector>

#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace MagicPortals::Shot {

// The port's shot.json.
struct Rules {
    // The flight, DECODED (ETHCallback_projectile; planning step 149): the shot jumps `jumpUnits`
    // on its first callback and then advances speedUnits x min(frameCapMs, the script's whole
    // milliseconds) / 1000 a callback.
    double speedUnits = 0.0;
    double jumpUnits = 0.0;
    double frameCapMs = 0.0;
    double reflectRadiusPx = 0.0; // _guess: how near a reflector a shot has to pass
    int maxReflections = 0;       // _guess: how many reflectors one shot comes off
    bool defaultPlaneVertical = true; // _guess: the plane of a reflector whose level gives none
};

bool LoadRules(const std::string& path, Rules& out, std::string& error);

struct Hit {
    entt::entity body = entt::null;
    double along = 0.0; // how far along the segment, from 0 to 1
};

// The first solid body the segment from `fromPx` to `toPx` meets, in the
// port's pixels, other than `ignore`. None when it meets nothing. A segment
// that starts inside a body meets it at once.
std::optional<Hit> FirstBody(entt::registry& registry, const glm::dvec2& fromPx, const glm::dvec2& toPx,
                             entt::entity ignore);

// The first body the segment meets as the original's GetClosestContact finds it
// (ETHPhysicsSimulator.cpp:251-268): every body but `ignore`, a trigger as well
// as a solid one - Box2D's world ray cast asks no fixture whether it is a sensor
// (b2World.cpp:978-995) - and never one the segment starts inside or on, which
// Box2D's shape ray casts do not report (b2PolygonShape.cpp:189-258,
// b2CircleShape.cpp:47-82).
std::optional<Hit> ClosestContact(entt::registry& registry, const glm::dvec2& fromPx, const glm::dvec2& toPx,
                                  entt::entity ignore);

// The same, passing through every body in `ignore`: the list GetClosestContact
// is given by name (the tap-time ray of computePortalFinalPos, placement.json
// final_pos), which the caller has already turned into the bodies it names.
std::optional<Hit> ClosestContact(entt::registry& registry, const glm::dvec2& fromPx, const glm::dvec2& toPx,
                                  std::span<const entt::entity> ignore);

// Every body whose shape holds the point, a trigger as well as a solid one, in
// the registry's order - boxes, then spheres, then hulls: the original's
// EntityGrabber(point, 0, grabAllObstacles) (EntityGrabber::EntityGrabber,
// bytes 23924..24990 #66-212), which the tap-time ray's head-height retry asks
// twice. A point on an edge is held. The capsule is not read, as above.
std::vector<entt::entity> BodiesAt(entt::registry& registry, const glm::dvec2& atPx);

// Every SOLID body a circle overlaps - never a trigger - other than the bodies in
// `ignore`, in the registry's order: what a fireball's own body begins a contact
// with. fireball.ent is a sensor, and Box2D begins a sensor's contact on the step
// that finds the two shapes overlapping (b2Contact::Update's b2TestOverlap), so a
// circle meets a polygon it has its centre in or an edge nearer than its radius,
// and a circle nearer than the two radii. Touching is not overlapping. A trigger
// is the original's sensor (the converter's Area2D), which the callback passes
// by. The capsule is not read, as above: the character is Turrets' own test.
std::vector<entt::entity> SolidBodiesIn(entt::registry& registry, const glm::dvec2& centrePx, double radiusPx,
                                        std::span<const entt::entity> ignore);

// How far along the segment it first enters the box, or none.
std::optional<double> Enters(const glm::dvec2& fromPx, const glm::dvec2& toPx, const Trigger::Box& box);

// Whether a point is inside the box, its edges included: isPointInRect's closed rectangle
// (utilIntersections 319410..319788, which is false only for a point past a side), as
// ETHCallback_anti_projectile_wall asks it of a shot's position (#32-68).
bool InsideBox(const glm::dvec2& pointPx, const Trigger::Box& box);

// How far along the segment it first comes within `radiusPx` of `centrePx`, or
// none. Zero for a segment that starts within it.
std::optional<double> EntersCircle(const glm::dvec2& fromPx, const glm::dvec2& toPx, const glm::dvec2& centrePx,
                                   double radiusPx);

} // namespace MagicPortals::Shot
