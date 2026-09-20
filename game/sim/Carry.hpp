#pragma once

// The carry path: how a thing the original lets a character pick up finds an
// owner, follows it, and loses it again.
//
// key.ent and shock_diamond.ent do this identically. The decoded carried branch
// of ETHCallback_shock_diamond is the key's instruction for instruction: alpha
// 0.1, GetInt('ownerID'), ByIDChooser, a seekNeighbourEntity over the buckets
// around itself with a global SeekEntity behind it, and the same scale(40)
// leash. What differs between the two is deliberately NOT in here:
//
// - RANGE. A key polls scale(26) and a shock diamond scale(30), so it is passed
//   in rather than held.
// - WHO may take one. The key's candidate loop admits isCharacter OR isMinion;
//   the diamond's tests isCharacter and skips anything else before it ever
//   measures a distance. That is not a flag here either - the CALLER builds the
//   list of carriers, so a key is offered the player and every minion still
//   standing, and a diamond only the player; and the caller says where on each
//   body its entity point is (Carrier).
// - What happens on ARRIVAL: a key opens a keyhole of its colour, a diamond
//   strikes a minion. Neither is a carry concern.
// - What a LOST owner means. Both go back to being unowned - the original writes
//   ownerID = -1 and nothing else - which is Drop below. The diamond's OTHER
//   self-deletion, a candidate that shallLeave's, has no equivalent here: it is
//   tested before any distance is measured, and the port hands in only carriers
//   that are alive, so the branch cannot be reached.
//
// Carried is a base rather than a member so that a key is still a thing with an
// atPx and an owner of its own, and every read of one stays where it was.

#include <vector>

#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace MagicPortals::Carry {

// The numbers the carry path itself uses. A module that carries something owns
// these under its own name in its own json - keys.json calls them key.range_px
// and key.leash_px - and inherits this so the shared path can read them.
struct Rules {
    double rangePx = 0.0;  // picked up within this: a key 26, a shock diamond 30
    double leashPx = 0.0;  // further behind its owner than this and it snaps
    double reaimMs = 0.0;  // how often the trail takes a new destination
    double strideMs = 0.0; // how long one of those takes to travel
};

// Something that may take a carried thing, and the point it is taken and led
// from. The original measures the pickup from the carrier's GetPositionXY and
// trails that same point (ETHCallback_key ins 744-753, 932-957, 959-976): the
// ENTITY's point, which is not where the port's body has its centre. A minion's
// stands 11.1 above its body's centre (Minions::Rules::EntityAboveCentrePx), so
// a key it carries rides at its chest, behind the picture, and not at its feet.
// Which body is which is the caller's to know, as who may carry is.
struct Carrier {
    entt::entity body = entt::null;
    glm::dvec2 pointFromCentrePx{0.0, 0.0}; // the entity's point less the body's centre, +y down
};

// Where a carried thing is, and who has it. Not a body: the original touches no
// velocity anywhere in this path, which is why the port does not build one and
// then fight gravity for it.
struct Carried {
    glm::dvec2 atPx{0.0, 0.0};
    glm::dvec2 fromPx{0.0, 0.0}; // the trail's last destination, and the next
    glm::dvec2 toPx{0.0, 0.0};
    double sinceAimMs = 0.0;
    entt::entity owner = entt::null;          // who is carrying it, or null
    glm::dvec2 ownerPointFromCentrePx{0.0, 0.0}; // that owner's Carrier point, kept from the pickup
};

// Lets go of an owner that has ceased to be: the original's shallLeave, which
// here is a dead character or a destroyed minion - a burnt one, a crushed one,
// one taken by a killer floor. True if it was holding one and now is not.
bool Drop(Carried& carried, entt::registry& registry);

// The FIRST carrier within rangePx of its point takes it, not the nearest: the
// original scans the buckets around itself and stops at the first that fits.
// True if one just did, leaving the trail aimed at where the thing already lies.
bool Acquire(Carried& carried, entt::registry& registry, const std::vector<Carrier>& carriers, double rangePx);

// One tick of following whoever holds it, at that owner's point. Beyond the
// leash both ends of the interpolation and the position are slammed onto the
// point - which is also what carries a thing through a portal, since its owner
// arrives somewhere new - and inside it the trail re-aims every reaimMs and
// travels over strideMs, so a carried thing sits perpetually just behind it.
void Trail(Carried& carried, entt::registry& registry, const Rules& rules, double ms);

} // namespace MagicPortals::Carry
