#include "sim/LevelBuilder.hpp"

#include "core/Components.hpp"
#include "core/Json.hpp"
#include "sim/Prism.hpp"
#include "sim/Units.hpp"

#include <cmath>
#include <fstream>
#include <sstream>
#include <vector>

namespace MagicPortals::LevelBuilder {

using Supersonic::BoxColliderComponent;
using Supersonic::ConvexHullColliderComponent;
using Supersonic::PhysicsMaterialComponent;
using Supersonic::RigidBodyComponent;
using Supersonic::SphereColliderComponent;
using Supersonic::TagComponent;
using Supersonic::TransformComponent;

namespace {

constexpr double kPi = 3.14159265358979323846;

glm::dvec2 VectorOf(const Tscn::Value* v) {
    if (v == nullptr || v->kind != Tscn::Value::Kind::Vector2) return glm::dvec2(0.0);
    return glm::dvec2(v->numbers[0], v->numbers[1]);
}

double NumberOf(const Tscn::Value* v) {
    double x = 0.0;
    return v != nullptr && v->AsNumber(x) ? x : 0.0;
}

std::vector<const Tscn::Node*> ChildrenOf(const Tscn::Scene& scene, const Tscn::Node& parent) {
    std::vector<const Tscn::Node*> children;
    for (const Tscn::Node& node : scene.nodes)
        if (node.parent == parent.path) children.push_back(&node);
    return children;
}

entt::entity Place(entt::registry& registry, const Tscn::Node& node, const std::string& tag) {
    const entt::entity entity = registry.create();
    auto& transform = registry.emplace<TransformComponent>(entity);
    const glm::dvec2 position = VectorOf(node.Find("position"));
    transform.position = Units::ToWorld(position.x, position.y);
    transform.rotation.z = Units::ToWorldRotation(NumberOf(node.Find("rotation")));
    registry.emplace<TagComponent>(entity, TagComponent{tag});
    return entity;
}

bool IsEntityNode(const Tscn::Node& node) { return node.parent == "." && node.type == "Node2D"; }

// A RigidBody2D's body: its entity's material, no damping, and the plane's locks.
void MakeRigid(entt::registry& registry, entt::entity entity, const Options& options, const Material& material,
               double colliderAreaM2) {
    auto& rigid = registry.emplace<RigidBodyComponent>(entity);
    rigid.friction = material.friction;
    rigid.restitution = material.restitution;
    rigid.mass = MassOf(material, colliderAreaM2);
    // ETHPhysicsSimulator::CreateBody sets no damping on its b2BodyDef (Box2D's linear and angular damping
    // are 0): the engine's default of 0.05 a second on spin is not the original's, and a stone rolls on.
    rigid.linearDamping = 0.0f;
    rigid.angularDamping = 0.0f;
    if (options.lockToPlane) {
        rigid.lockPosition = kPlaneLockPosition;
        rigid.lockRotation = kPlaneLockRotation;
    }
}

// One entity node. `built` is false when it is skipped: scenery or a marker (no
// Body), or a static that withStatics leaves out.
bool BuildNode(const Tscn::Scene& scene, const Tscn::Node& node, entt::registry& registry, const Options& options,
               Built& out, bool& built, std::string& error) {
    built = false;
    const Tscn::Node* body = scene.Child(node, "Body");
    if (body == nullptr) return true; // scenery and markers

    const bool isStatic = body->type == "StaticBody2D";
    const bool isRigid = body->type == "RigidBody2D";
    const bool isArea = body->type == "Area2D";
    if (!isStatic && !isRigid && !isArea) {
        error = node.name + ": its Body is a " + body->type;
        return false;
    }
    const std::vector<const Tscn::Node*> shapes = ChildrenOf(scene, *body);
    if (shapes.size() != 1) {
        error = node.name + ": " + std::to_string(shapes.size()) +
                " shapes under its Body - the builder takes exactly one, which is every body the converter writes";
        return false;
    }
    if (isStatic && !options.withStatics) return true;
    const bool isMover =
        isStatic && options.roles != nullptr && Roles::Moves(Roles::RoleOf(*options.roles, node));

    const Tscn::Node& shape = *shapes.front();
    const double depth = isRigid ? kBodyDepthMetres : kStaticDepthMetres;
    const Material material = MaterialOf(registry, Roles::EntityName(node));
    double areaM2 = 0.0; // the collider's, for a body's mass
    const glm::dvec2 offset = VectorOf(shape.Find("position"));
    const entt::entity entity = Place(registry, node, node.name);

    if (shape.type == "CollisionPolygon2D") {
        const Tscn::Value* polygon = shape.Find("polygon");
        if (polygon == nullptr) {
            error = node.name + ": a CollisionPolygon2D with no polygon";
            return false;
        }
        std::vector<double> points = polygon->numbers;
        for (std::size_t i = 0; i + 1 < points.size(); i += 2) {
            points[i] += offset.x;
            points[i + 1] += offset.y;
        }
        std::string why;
        const std::string path = Prism::Write(points, depth, options.prismDirectory, why);
        if (path.empty()) {
            error = node.name + ": " + why;
            return false;
        }
        auto& hull = registry.emplace<ConvexHullColliderComponent>(entity);
        hull.sourcePath = path;
        hull.isTrigger = isArea;
        auto& outline = registry.emplace<PlanePolygon>(entity);
        for (std::size_t i = 0; i + 1 < points.size(); i += 2) {
            outline.points.emplace_back(Units::ToMetres(points[i]), Units::ToMetres(-points[i + 1]));
        }
        // The shoelace area of the outline.
        double twice = 0.0;
        for (std::size_t i = 0; i < outline.points.size(); ++i) {
            const glm::vec2& a = outline.points[i];
            const glm::vec2& b = outline.points[(i + 1) % outline.points.size()];
            twice += static_cast<double>(a.x) * b.y - static_cast<double>(b.x) * a.y;
        }
        areaM2 = std::fabs(twice) * 0.5;
        ++out.hulls;
    } else if (shape.type == "CollisionShape2D") {
        const Tscn::Value* ref = shape.Find("shape");
        const Tscn::Resource* resource = ref != nullptr ? scene.Embedded(ref->text) : nullptr;
        if (resource == nullptr) {
            error = node.name + ": a CollisionShape2D with no shape";
            return false;
        }
        const glm::vec3 centre = Units::ToWorld(offset.x, offset.y);
        if (resource->type == "RectangleShape2D") {
            const glm::dvec2 size = VectorOf(resource->Find("size"));
            auto& box = registry.emplace<BoxColliderComponent>(entity);
            box.size = glm::vec3(Units::ToMetres(size.x), Units::ToMetres(size.y), static_cast<float>(depth));
            box.center = centre;
            box.isTrigger = isArea;
            areaM2 = static_cast<double>(Units::ToMetres(size.x)) * static_cast<double>(Units::ToMetres(size.y));
        } else if (resource->type == "CircleShape2D") {
            auto& sphere = registry.emplace<SphereColliderComponent>(entity);
            sphere.radius = Units::ToMetres(NumberOf(resource->Find("radius")));
            sphere.center = centre;
            sphere.isTrigger = isArea;
            areaM2 = kPi * static_cast<double>(sphere.radius) * static_cast<double>(sphere.radius);
        } else {
            error = node.name + ": a " + resource->type + " is not a collision shape";
            return false;
        }
    } else {
        error = node.name + ": a " + shape.type + " under its Body is not a collision shape";
        return false;
    }

    if (isRigid) {
        MakeRigid(registry, entity, options, material, areaM2);
        ++out.rigids;
    } else if (isMover) {
        // Moved by the port and carrying what stands on it: kinematic, so the
        // contact solve reads the velocity Mover::MoveKinematic writes (F1). A
        // kinematic body is moved by code and reads no locks.
        auto& rigid = registry.emplace<RigidBodyComponent>(entity);
        rigid.isKinematic = true;
        rigid.friction = material.friction;
        rigid.restitution = material.restitution;
        ++out.movers;
    } else if (isArea) {
        ++out.areas;
    } else {
        auto& surface = registry.emplace<PhysicsMaterialComponent>(entity);
        surface.friction = material.friction;
        surface.restitution = material.restitution;
        ++out.statics;
    }
    out.entities[node.name] = entity;
    built = true;
    return true;
}

} // namespace

namespace {

// "crate.ent" and "crate" are the same entity: a node's entity_name carries the extension in some levels
// and not in others.
std::string WithoutExtension(const std::string& entityName) {
    const std::string extension = ".ent";
    if (entityName.size() > extension.size() &&
        entityName.compare(entityName.size() - extension.size(), extension.size(), extension) == 0) {
        return entityName.substr(0, entityName.size() - extension.size());
    }
    return entityName;
}

} // namespace

Material Materials::Of(const std::string& entityName) const {
    const auto found = byEntity.find(WithoutExtension(entityName));
    return found == byEntity.end() ? Material{} : found->second;
}

bool Materials::Holds(const std::string& entityName) const {
    return byEntity.find(WithoutExtension(entityName)) != byEntity.end();
}

bool LoadMaterials(const std::string& path, Materials& out, std::string& error) {
    namespace Json = Supersonic::Json;
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = path + ": cannot read";
        return false;
    }
    std::ostringstream text;
    text << file.rdbuf();
    // Named, not a temporary: the parser keeps a reference to what it reads.
    const std::string content = text.str();
    Json::Parser parser(content);
    Json::Value root;
    if (!parser.Parse(root)) {
        error = path + ": " + parser.Error();
        return false;
    }
    if (!root["groups"].IsArray() || root["groups"].AsArray().empty()) {
        error = path + ": groups is a list of at least one group";
        return false;
    }
    if (!root["restitution_applied"].IsBool()) {
        error = path + ": restitution_applied is true or false";
        return false;
    }
    if (!root["friction_applied"].IsBool()) {
        error = path + ": friction_applied is true or false";
        return false;
    }
    if (!root["held_friction"].IsNumber() || !(root["held_friction"].AsFloat() >= 0.0f)) {
        error = path + ": held_friction is a number of at least 0";
        return false;
    }
    Materials read;
    read.restitutionApplied = root["restitution_applied"].AsBool();
    read.frictionApplied = root["friction_applied"].AsBool();
    read.heldFriction = root["held_friction"].AsFloat();
    for (const Json::Value& group : root["groups"].AsArray()) {
        if (!group["friction"].IsNumber() || !group["restitution"].IsNumber() || !group["entities"].IsArray()) {
            error = path + ": every group has a friction, a restitution and a list of entities";
            return false;
        }
        Material material;
        material.decodedFriction = group["friction"].AsFloat();
        // The .ent's friction where the table applies it, and where this group says it is safe to while the rest is held.
        material.friction = (read.frictionApplied || group["apply_decoded_friction"].AsBool(false)) ? material.decodedFriction
                                                                                                   : read.heldFriction;
        material.density = group["density"].AsFloat(0.0f);
        material.areaM2 = group["area_m2"].AsFloat(0.0f);
        if (!(material.density >= 0.0f) || !(material.areaM2 >= 0.0f)) {
            error = path + ": a density and an area are at least 0";
            return false;
        }
        material.decodedRestitution = group["restitution"].AsFloat();
        material.restitution = read.restitutionApplied ? material.decodedRestitution : kBodyRestitution;
        // A friction below zero or a restitution outside [0, 1] describes no Box2D fixture.
        if (!(material.decodedFriction >= 0.0f) || !(material.decodedRestitution >= 0.0f) ||
            !(material.decodedRestitution <= 1.0f)) {
            error = path + ": a friction is at least 0 and a restitution is between 0 and 1";
            return false;
        }
        for (const Json::Value& name : group["entities"].AsArray()) {
            if (!name.IsString() || name.AsString().empty()) {
                error = path + ": an entity is a name";
                return false;
            }
            if (!read.byEntity.emplace(WithoutExtension(name.AsString()), material).second) {
                error = path + ": " + name.AsString() + " is in two groups";
                return false;
            }
        }
    }
    out = std::move(read);
    return true;
}

float MassOf(const Material& material, double colliderAreaM2) {
    if (!(material.density > 0.0f)) return 1.0f;
    const double area = material.areaM2 > 0.0f ? static_cast<double>(material.areaM2) : colliderAreaM2;
    return area > 0.0 ? static_cast<float>(static_cast<double>(material.density) * area) : 1.0f;
}

Material MaterialOf(entt::registry& registry, const std::string& entityName) {
    const Materials* table = registry.ctx().find<Materials>();
    return table != nullptr ? table->Of(entityName) : Material{};
}

bool Build(const Tscn::Scene& scene, entt::registry& registry, const Options& options, Built& out,
           std::string& error) {
    out = Built{};
    for (const Tscn::Node& node : scene.nodes) {
        if (!IsEntityNode(node)) continue;
        bool built = false;
        if (!BuildNode(scene, node, registry, options, out, built, error)) return false;
    }
    return true;
}

bool ShapeBoundsPx(const Tscn::Scene& scene, const Tscn::Node& node, glm::dvec2& offsetPx, glm::dvec2& sizePx) {
    const Tscn::Node* body = scene.Child(node, "Body");
    const Tscn::Node* shape = body != nullptr ? scene.Child(*body, "Shape") : nullptr;
    if (shape == nullptr) return false;
    offsetPx = VectorOf(shape->Find("position"));
    if (shape->type == "CollisionPolygon2D") {
        const Tscn::Value* polygon = shape->Find("polygon");
        if (polygon == nullptr || polygon->numbers.size() < 2) return false;
        glm::dvec2 lo(polygon->numbers[0], polygon->numbers[1]);
        glm::dvec2 hi = lo;
        for (std::size_t i = 0; i + 1 < polygon->numbers.size(); i += 2) {
            const glm::dvec2 point(polygon->numbers[i], polygon->numbers[i + 1]);
            lo = glm::min(lo, point);
            hi = glm::max(hi, point);
        }
        offsetPx += (lo + hi) * 0.5;
        sizePx = hi - lo;
        return true;
    }
    const Tscn::Value* ref = shape->Find("shape");
    const Tscn::Resource* resource = ref != nullptr ? scene.Embedded(ref->text) : nullptr;
    if (resource == nullptr) return false;
    if (resource->type == "RectangleShape2D") {
        const Tscn::Value* size = resource->Find("size");
        if (size == nullptr || size->kind != Tscn::Value::Kind::Vector2) return false;
        sizePx = VectorOf(size);
        return true;
    }
    if (resource->type == "CircleShape2D") {
        double radius = 0.0;
        const Tscn::Value* value = resource->Find("radius");
        if (value == nullptr || !value->AsNumber(radius)) return false;
        sizePx = glm::dvec2(radius * 2.0);
        return true;
    }
    return false;
}

entt::entity BuildRigidCircle(entt::registry& registry, const std::string& tag, const glm::dvec2& atPx,
                              double radiusPx, const Options& options, const std::string& entityName) {
    const entt::entity entity = registry.create();
    registry.emplace<TransformComponent>(entity).position = Units::ToWorld(atPx.x, atPx.y);
    registry.emplace<TagComponent>(entity, TagComponent{tag});
    registry.emplace<SphereColliderComponent>(entity).radius = Units::ToMetres(radiusPx);
    const double radiusM = static_cast<double>(Units::ToMetres(radiusPx));
    MakeRigid(registry, entity, options, MaterialOf(registry, entityName), kPi * radiusM * radiusM);
    return entity;
}

entt::entity BuildStaticBox(entt::registry& registry, const std::string& tag, const glm::dvec2& atPx,
                            const glm::dvec2& sizePx, const Options& options) {
    // No MakeRigid: a StaticBody2D gets a collider and no RigidBodyComponent,
    // which is what BuildNode does for one and what makes this a floor rather
    // than something that falls the moment it is made.
    (void)options;
    const entt::entity entity = registry.create();
    registry.emplace<TransformComponent>(entity).position = Units::ToWorld(atPx.x, atPx.y);
    registry.emplace<TagComponent>(entity, TagComponent{tag});
    auto& box = registry.emplace<BoxColliderComponent>(entity);
    box.size = glm::vec3(Units::ToMetres(sizePx.x), Units::ToMetres(sizePx.y),
                         static_cast<float>(kStaticDepthMetres));
    return entity;
}

entt::entity BuildEntity(const Tscn::Scene& scene, const std::string& nodeName, entt::registry& registry,
                         const Options& options, Built& out, std::string& error) {
    const Tscn::Node* node = scene.FindNode(nodeName);
    if (node == nullptr || !IsEntityNode(*node)) {
        error = nodeName + " is not an entity of the scene";
        return entt::null;
    }
    bool built = false;
    if (!BuildNode(scene, *node, registry, options, out, built, error)) return entt::null;
    if (!built) {
        error = nodeName + " has no body to build";
        return entt::null;
    }
    return out.entities.at(nodeName);
}

entt::entity AddTriggerFromMetadata(const Tscn::Scene& scene, const std::string& entityName,
                                    entt::registry& registry, std::string& error) {
    const Tscn::Node* node = scene.FindNode(entityName);
    if (node == nullptr) {
        error = entityName + " is not in the scene";
        return entt::null;
    }
    const Tscn::Value* size = node->Meta("trigger_size");
    if (size == nullptr || size->kind != Tscn::Value::Kind::Vector2) {
        error = entityName + " has no trigger_size";
        return entt::null;
    }
    // The Area2D is the node's child at trigger_offset, so the node's rotation
    // turns the offset too - which the collider's centre gets for free.
    const glm::dvec2 offset = VectorOf(node->Meta("trigger_offset"));
    const entt::entity entity = Place(registry, *node, entityName + "/Trigger");
    auto& box = registry.emplace<BoxColliderComponent>(entity);
    box.size = glm::vec3(Units::ToMetres(size->numbers[0]), Units::ToMetres(size->numbers[1]),
                         static_cast<float>(kStaticDepthMetres));
    box.center = Units::ToWorld(offset.x, offset.y);
    box.isTrigger = true;
    return entity;
}

} // namespace MagicPortals::LevelBuilder
