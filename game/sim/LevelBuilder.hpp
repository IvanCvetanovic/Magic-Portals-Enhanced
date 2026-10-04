#pragma once

// A converted level, as bodies in an engine registry.
//
// What the Godot project's scene tree gives Godot's 2D physics, given to the engine's
// 3D solver instead, one entity per body:
//
//   StaticBody2D + rectangle   a box collider and no rigid body
//   StaticBody2D + polygon     a convex hull, from an OBJ prism (Prism.hpp)
//   StaticBody2D + circle      a sphere collider
//   RigidBody2D  + any of them the same collider, with a rigid body
//   Area2D       + any of them the same collider, as a trigger
//
// The entity is placed at its node's position and turned by its rotation, and a
// shape's own offset (30 of them, all on carranca and wood_piece_small) goes
// into the collider - a box's or sphere's centre, or the prism's points.
//
// Given the role table, it also does the one thing the Godot project's LevelRuntime
// does to a body: a StaticBody2D whose role moves (Roles::Moves) becomes a
// kinematic body, as the Godot project makes it an AnimatableBody2D, so it can carry
// what stands on it. Without the table - the spike - every StaticBody2D stays
// static. The rest of LevelRuntime (triggers, behaviours) is the caller's.

#include "sim/Roles.hpp"
#include "sim/Tscn.hpp"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace MagicPortals::LevelBuilder {

// How far a body reaches along z, the axis a 2D level does not have. Static
// geometry is deeper than what moves over it, so a crate's footprint lies
// wholly inside the face it rests on: edge-on-edge contact is the case most
// likely to push a body out of the plane, and the drift measurement should see
// the solver rather than a coincidence of extents. Chosen for the spike, and
// recorded in its doc - nothing in the original says what these should be.
inline constexpr double kStaticDepthMetres = 2.0;
inline constexpr double kBodyDepthMetres = 1.0;

// Rigid bodies get Godot's PhysicsMaterial defaults, which is what the Godot project's
// RigidBody2D crates run with: friction 1, bounce 0. Its movers, AnimatableBody2D
// with no material either, get the same.
inline constexpr float kBodyFriction = 1.0f;
inline constexpr float kBodyRestitution = 0.0f;

// Statics get the same, through PhysicsMaterialComponent: the Godot project's
// StaticBody2D floors carry no material either, so they run on Godot's defaults.
// Left without one they would take the engine's fallback for a collider with no
// rigid body - bounce 0.3, grip 0.4 - and every landing would come back up.
inline constexpr float kStaticFriction = 1.0f;
inline constexpr float kStaticRestitution = 0.0f;

// What an entity's bodies are made of: the friction and restitution its .ent gives the fixture
// ETHPhysicsSimulator::CreateBody builds (entity->GetFriction(idx), GetRestitution(idx):
// ETHPhysicsSimulator.cpp:102-109) - decoded per entity where game/data/materials.json holds the
// entity, and the Godot project's constants above where it does not (an entity whose .ent is not in the
// extracted assets). The solver mixes two surfaces as Box2D does: friction by the square root of the
// product, restitution by the larger.
//
// `restitution` is what a body is BUILT with; `decodedRestitution` is the .ent's own. They are the same
// while materials.json says `restitution_applied` (it does not, today: the engine bounces a pair a step's
// travel BEFORE it touches, which stops a stone short of the wall it breaks - Materials::restitutionApplied).
//
// `friction` is likewise what a body is BUILT with and `decodedFriction` the .ent's own: the same while
// materials.json says `friction_applied` (it does not, today: the engine's friction solve walks a resting box
// sideways at a combined friction of 0.4 to 0.6, which is every crate or barrel on a platform - the
// Materials::frictionApplied note), and otherwise the table's `held_friction`, except for a group that says
// `apply_decoded_friction` (a friction of 0 is 0 against any surface, and no friction impulse is solved).
//
// `density` is the .ent's, which Box2D multiplies by the shape's area for the body's MASS (b2Body::ResetMassData):
// 0 is an entity with none (a static, or one the table does not hold), whose body keeps the engine's mass of 1.
// `areaM2` is the area of the ORIGINAL's shape where the port's collider is a simpler one (a minion's two
// circles of radius 9.1 are one circle here): 0 takes the collider's own.
struct Material {
    float friction = kBodyFriction;
    float decodedFriction = kBodyFriction;
    float restitution = kBodyRestitution;
    float decodedRestitution = kBodyRestitution;
    float density = 0.0f;
    float areaM2 = 0.0f;
};

struct Materials {
    // Whether a body is built with its entity's restitution (see Material). materials.json's top-level flag.
    bool restitutionApplied = false;
    // Whether a body is built with its entity's friction (see Material); materials.json's top-level flag. False says every
    // friction is `heldFriction` but a group's that applies its own, and a rig built with no table is on the constants above.
    bool frictionApplied = false;
    float heldFriction = kBodyFriction;
    // By the entity's name WITHOUT ".ent": a node's entity_name carries it in some levels and not in others.
    std::unordered_map<std::string, Material> byEntity;
    // The material of `entityName` (with or without ".ent"), or the default.
    Material Of(const std::string& entityName) const;
    bool Holds(const std::string& entityName) const;
};

// Reads materials.json: `groups`, each a list of `entities` that share a `friction` and a `restitution`.
// False, with `error`, for a missing or non-numeric value, or an entity named in two groups.
bool LoadMaterials(const std::string& path, Materials& out, std::string& error);

// A body's mass, in the units the engine takes (density x area, metres): the entity's density times the shape's
// area - `colliderAreaM2` unless the material states the original's own - or the engine's default of 1 where the
// entity has no density.
float MassOf(const Material& material, double colliderAreaM2);

// The material `entityName`'s bodies are built with in this registry: the table Game::Start put in its
// context, or the default where there is none (a rig built with no level behind it).
Material MaterialOf(entt::registry& registry, const std::string& entityName);

// The 2D port's locks: every rigid body stays in the plane - no motion along z -
// and turns about z only. Unlocked, the spike measured a resting crate 30 px out
// of the plane in a minute and a pushed one leaving the level. S1, recorded in
// the spike doc.
inline const glm::bvec3 kPlaneLockPosition{false, false, true};
inline const glm::bvec3 kPlaneLockRotation{true, true, false};

struct Options {
    std::filesystem::path prismDirectory;
    // False leaves out every StaticBody2D, movers included: the mutation a
    // landing check must fail, since a body over no geometry has nothing to
    // land on.
    bool withStatics = true;
    // Every rigid body locked to the plane (kPlaneLock*). Off only to measure
    // what the solver does without the locks: MagicPortalsSpike --unlocked.
    bool lockToPlane = true;
    // The role table, when the level is to be played rather than measured: its
    // movers are then built kinematic. Not owned.
    const Roles::Table* roles = nullptr;
};

// A hull's polygon in the plane: its points in its entity's frame, in metres
// with +y up, the shape's offset included. The engine's queries see a hull only
// by its bounds, so the port's own segment test (Shot.hpp) reads this.
struct PlanePolygon {
    std::vector<glm::vec2> points;
};

struct Built {
    std::unordered_map<std::string, entt::entity> entities; // by the entity node's name
    int statics = 0;
    int rigids = 0;
    int areas = 0;
    int movers = 0;
    int hulls = 0;
};

bool Build(const Tscn::Scene& scene, entt::registry& registry, const Options& options, Built& out,
           std::string& error);

// One entity node, by name, added to `out` - a level's piece on its own, for a
// rig that must not have the rest of the level in the way. entt::null, with
// `error` set, when the node is not an entity with a body.
entt::entity BuildEntity(const Tscn::Scene& scene, const std::string& nodeName, entt::registry& registry,
                         const Options& options, Built& out, std::string& error);

// The box an entity's body takes up, in the port's pixels, relative to its
// node and before the node's rotation: a rectangle's size, a circle's diameter,
// or a polygon's bounds, centred where the shape's own offset puts it. False for
// a node with no Body/Shape.
bool ShapeBoundsPx(const Tscn::Scene& scene, const Tscn::Node& node, glm::dvec2& offsetPx, glm::dvec2& sizePx);

// A rigid circle with no node behind it, built as a RigidBody2D with a
// CircleShape2D is: what a launcher throws (Launchers.hpp). Not added to any
// Built; the caller owns it.
// `entity` names the .ent it stands for (minion.ent, rolling_stone.ent, ...), which its material is read
// by; empty is the default material.
entt::entity BuildRigidCircle(entt::registry& registry, const std::string& tag, const glm::dvec2& atPx,
                              double radiusPx, const Options& options, const std::string& entity = std::string());

// A STATIC box with no node behind it, which is what an entity added at run time
// needs: the original's AddScaledEntity makes a body from the .ent itself, and
// BuildEntity above can only build a node the scene already holds. The dark
// dragon's death adds a platform this way (DarkDragon.hpp). Not added to any
// Built; the caller owns it. A static carries no RigidBodyComponent, exactly as
// BuildNode leaves one out for a StaticBody2D.
entt::entity BuildStaticBox(entt::registry& registry, const std::string& tag, const glm::dvec2& atPx,
                            const glm::dvec2& sizePx, const Options& options);

// A trigger box from an entity's metadata/trigger_size and trigger_offset, the
// way LevelRuntime._attach_trigger builds one (level_runtime.gd:243-265).
// entt::null, with `error` set, when the entity has no trigger_size.
entt::entity AddTriggerFromMetadata(const Tscn::Scene& scene, const std::string& entityName,
                                    entt::registry& registry, std::string& error);

} // namespace MagicPortals::LevelBuilder
