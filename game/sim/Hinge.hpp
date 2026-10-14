#pragma once

// Seesaws and spinning platforms: the bodies the original pins to an anchor with
// a revolute joint.
//
// The one role so far with NO per-tick work in it. Every other system here ticks
// something; this one attaches a JointComponent when the level is built and the
// engine's solver does the rest. That is the whole point - a hinge that a script
// drove by writing transforms would not be a physical object, it would be a body
// that ignores what it touches, and the player has to be able to stand on these
// and tip them.
//
// NOT AN ENGINE GAP. The engine already had joints: JointComponent with a Hinge
// type, Joints::HingeAngle, and a limit split into a position and a velocity half
// so a stop absorbs instead of bouncing. Nothing was added for this.
//
// WHERE THE NUMBERS COME FROM
//
// The converter carries the joint through: eight bare metadata/joint_* keys (A's
// attach point and the limits), metadata/revoluteJoint, a STRING naming the other
// body by its entity_name, and since planning step 174 (CMP-2) the other end
// itself: metadata/joint_other, the node that is B, and joint_anchor_b_x/y, B's
// attach point in B's own frame. In every one of the ten placements the entity
// is "anchor" and the node a static body, so this is body to body and never
// body to world.
//
// The original's attachPoints are FRACTIONS OF THE HALF COLLISION BOX
// (ETHRevoluteJoint::ComputeAnchorPosition: anchorPoint * size * scale * 0.5),
// each of ITS OWN entity's box: A's against A's, B's against B's
// (ETHRevoluteJoint.cpp:35-44, :60-64). The converter multiplies both out to
// pixels.
//
// B'S POINT IS NOT A'S. It was once thought to cost nothing: "Box2D's Initialize
// puts both anchors on the same world point, so B's anchor is simply where A's
// lands". But the constructor overwrites what Initialize set with the two
// authored local anchors (:39-40), and those two points are NOT on one world
// point where the level places the bodies: 8 of the 10 are 0.4 to 31.8 px
// apart (2-31's seesaw 31.8, 3-19's plank 8, 2-28 2.1 - this file's first
// author saw level27a's 357.6 against the platform's 357 and let the 0.6 px
// go). Box2D's position constraint takes the whole error out in the first step,
// and A is the dynamic body, so A moves to where its anchor meets B's: the
// body's centre ends on B's world point (A's own attach point is its centre in
// all ten). Find puts it there before the first tick, and gives the engine's
// joint B's point as the connected anchor, so the solver holds it there.
//
// THE ANGLE, WHICH IS THE PART THAT BITES
//
// Box2D measures `angleB - angleA - referenceAngle`, and Initialize sets that
// reference from the pose AT CREATION, so the authored lower/upper are relative
// to how the level was built. The engine's hinge angle is zero where the two
// bodies' reference directions coincide, which its own header calls an arbitrary
// configuration. So the authored range is added to the angle the joint rests at
// rather than used as-is, or every rotated placement is wrong by its rotation -
// level18b rests at -1.5708 and level21b at 0.3316.
//
// The SIGN, and how it was got wrong before it was got right.
//
// Box2D reads B relative to A, the engine reads A relative to B, and the y-flip
// negates each body's angle again, so the two flips cancel: H = (-angleA) -
// (-angleB) + c = (angleB - angleA) + c. A +1 coefficient, and the limits are
// simply ADDED to the rest angle. That is the answer.
//
// It was then negated instead, on the strength of a measurement - and the
// MEASUREMENT was the thing at fault. The reading was taken three seconds into a
// swing with gravity still on, by which time gravity had pulled the bar down past
// rest, which looks exactly like an inverted sign. Read six ticks in, before the
// bar can reach anything, +z plainly RAISES the angle.
//
// Two things hide this and are worth knowing. The solver ORDERS the pair itself
// (constraint.minAngle = min(authored min, max)), so swapping the two does
// nothing whatever and only a negation was ever doing anything. And a 254 px
// seesaw on a 127 px arm meets the level's own geometry before it reaches EITHER
// stop - level30a's settles at 2.11 turned one way and 1.67 the other, neither of
// them a limit - so "it stopped early" is the ordinary case and says nothing at
// all about which way the angle runs.
//
// level30a is the only level that can tell a wrong sign from a right one: it
// swings 1.1325 one way and 0.4382 the other, where every other placement in the
// game is a symmetric quarter turn that passes either way.
//
// The rest angle is measured from the level's own rotations rather than from the
// registry, because Find runs right after the bodies are built and nothing has
// stepped yet - a world transform may not have been resolved. It still goes
// through Joints::HingeAngle, so there is one answer to the question and not two.
//
// WHAT IS NOT BUILT
//
// - Motors. All ten placements carry enable_motor 0, and
//   ETHCallback_spinning_platform sets the joint's motor speed to zero every
//   frame besides. level21b's motor_seesaw carries a motorSpeed of "-4" in its
//   custom data and nothing in the bytecode reads it.
// - ETHCallback_spinning_cross spins its body at 0.6 rad/s, and no level places a
//   spinning_cross. Dead data, recorded in hinge.json rather than built.

#include "sim/LevelBuilder.hpp"
#include "sim/Roles.hpp"
#include "sim/Tscn.hpp"

#include <string>
#include <vector>

#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace MagicPortals::Hinge {

// The port's hinge.json.
struct Rules {
    // How much of the joint's error the solver takes out per step. The engine's
    // own default; carried as data so a level that jitters can be retuned without
    // touching code.
    double stiffness = 0.0;
};

bool LoadRules(const std::string& path, Rules& out, std::string& error);

// One pinned body and the anchor it turns about.
struct Hinged {
    std::string name;       // the swinging body's node
    std::string anchorName; // the node its revoluteJoint names
    entt::entity body = entt::null;
    entt::entity anchor = entt::null;

    // Where the level placed the body's centre, and where its joint holds it (px, +y down): B's centre plus attach point B
    // turned by B's angle, less A's own attach point turned by A's. Equal when the two ends already meet (2-30, 3-22).
    glm::dvec2 placedPx{0.0};
    glm::dvec2 heldPx{0.0};

    bool limited = false;
    double restRad = 0.0;  // the hinge angle the level is built at
    double lowerRad = 0.0; // as authored, relative to rest
    double upperRad = 0.0;
    double minRad = 0.0; // rest + lower, which is what the joint carries
    double maxRad = 0.0;
};

struct State {
    Rules rules;
    std::vector<Hinged> hinges;

    const Hinged* Find(const std::string& name) const;
};

// A level's hinges, with a JointComponent attached to each swinging body. A
// placement whose body or whose anchor was not built is passed over, which is
// what a level started without its statics leaves - the anchor is static.
// False, with `error`, for a revoluteJoint naming an entity the level does not
// place, because that is a level that would swing about nothing; for a level
// whose conversion lacks attach point B, which would hold every body at the
// middle of its anchor; and for a joint_other that is not the entity the
// custom datum names.
bool Find(const Tscn::Scene& scene, const Roles::Table& roles, const LevelBuilder::Built& built,
          entt::registry& registry, const Rules& rules, State& out, std::string& error);

// The live hinge angle, measured the way the SOLVER measures it - through
// Joints::HingeAngle, for the reason the editor's own readout does: two answers
// to one question is how a number disagrees with the stop a body actually hits.
double AngleOf(entt::registry& registry, const Hinged& hinged);

} // namespace MagicPortals::Hinge
