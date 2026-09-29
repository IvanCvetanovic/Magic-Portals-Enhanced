#include "sim/Restock.hpp"

#include "core/Components.hpp"
#include "core/Json.hpp"
#include "sim/Roles.hpp"
#include "sim/Units.hpp"

#include <fstream>
#include <sstream>

namespace MagicPortals::Restock {

namespace {

namespace Json = Supersonic::Json;

// The node's eth_z, in level units, or 0 where it states none.
double DepthOf(const Tscn::Node& node) {
    const Tscn::Value* z = node.Meta("eth_z");
    double read = 0.0;
    if (z == nullptr || !z->AsNumber(read)) return 0.0;
    return read;
}

// The first node of `scene` whose entity is `entity`, by name; empty for none.
std::string FirstNodeOf(const Tscn::Scene& scene, const std::string& entity) {
    for (const Tscn::Node& node : scene.nodes) {
        if (node.parent == "." && Roles::EntityName(node) == entity) return node.name;
    }
    return std::string();
}

// A number as the converter writes one: no trailing zeros, no exponent.
std::string Number(double value) {
    std::ostringstream text;
    text.precision(10);
    text << value;
    return text.str();
}

// A string the table holds, quoted for the scene: a quote or a backslash in one
// would not survive, and none belongs in a name, a path or a flag.
bool Quoted(const std::string& text, std::string& out) {
    if (text.find_first_of("\"\\\n") != std::string::npos) return false;
    out = "\"" + text + "\"";
    return true;
}

} // namespace

Site Resolve(const Tscn::Scene& scene, const Rule& rule) {
    Site site;
    for (const Tscn::Node& node : scene.nodes) {
        if (node.parent != ".") continue;
        const std::string entity = Roles::EntityName(node);
        if (!site.spawnFound && entity == rule.spawn) {
            const Tscn::Value* position = node.Find("position");
            if (position != nullptr && position->kind == Tscn::Value::Kind::Vector2) {
                site.spawnFound = true;
                site.spawnPx = glm::dvec2(position->numbers[0], position->numbers[1]);
                site.spawnZ = DepthOf(node);
            }
        }
        if (site.templateNode.empty() && entity == rule.templateEntity) site.templateNode = node.name;
    }
    return site;
}

Site Resolve(const Tscn::Scene& scene, const Tscn::Scene& templates, const Rule& rule) {
    Site site = Resolve(scene, rule);
    if (site.templateNode.empty()) {
        site.templateNode = FirstNodeOf(templates, rule.templateEntity);
        site.templateFromPort = !site.templateNode.empty();
    }
    return site;
}

const Tscn::Scene& TemplateScene(const Tscn::Scene& level, const Tscn::Scene& templates, bool fromPort) {
    return fromPort ? templates : level;
}

bool State::Found(const Tscn::Scene& scene, const LevelBuilder::Built& built, const entt::registry& registry,
                  const std::string& search) const {
    for (const Added& one : added) {
        if (one.alias == search && registry.valid(one.body)) return true;
    }
    for (const auto& [name, body] : built.entities) {
        if (!registry.valid(body)) continue;
        const Tscn::Node* node = scene.FindNode(name);
        if (node != nullptr && Roles::EntityName(*node) == search) return true;
    }
    return false;
}

entt::entity State::Add(const Tscn::Scene& scene, const Tscn::Scene& templates, entt::registry& registry,
                        const LevelBuilder::Options& options, const Rule& rule, const Site& site,
                        LevelBuilder::Built& built, std::string& error) {
    const auto refuse = [this, &error](const std::string& why) {
        error = why;
        if (refusals++ == 0) firstRefusal = why;
        return entt::null;
    };
    if (!site.Ready()) {
        return refuse(site.spawnFound ? "no " + rule.templateEntity + " in the level or the port's templates to build " +
                                            rule.add + " from"
                                      : "no " + rule.spawn + " in the level to add " + rule.add + " at");
    }
    // Built into a scratch table, so the template's own entry - the placed body,
    // whether or not it still stands - is not written over.
    LevelBuilder::Built scratch;
    std::string why;
    const entt::entity body = LevelBuilder::BuildEntity(TemplateScene(scene, templates, site.templateFromPort),
                                                        site.templateNode, registry, options, scratch, why);
    if (body == entt::null) return refuse(why);

    Added made;
    made.name = site.templateNode + "#" + std::to_string(++count);
    made.alias = rule.alias;
    made.templateNode = site.templateNode;
    made.templateFromPort = site.templateFromPort;
    made.body = body;
    made.atPx = site.spawnPx;
    made.z = site.spawnZ;
    // AddEntity(entityToAdd, the marker's point, angle 0, ...): at the marker and
    // unturned, whatever the template's node says.
    auto& transform = registry.get<Supersonic::TransformComponent>(body);
    transform.position = Units::ToWorld(site.spawnPx.x, site.spawnPx.y);
    transform.rotation.z = 0.0f;
    registry.get<Supersonic::TagComponent>(body) = Supersonic::TagComponent{made.name};

    built.entities[made.name] = body;
    added.push_back(made);
    return body;
}

bool LoadTemplates(const std::string& path, const Tscn::Scene& level, Tscn::Scene& out, std::string& error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = path + ": cannot read";
        return false;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    // Named, not a temporary: the parser keeps a reference to what it reads.
    const std::string content = buffer.str();
    Json::Parser parser(content);
    Json::Value root;
    if (!parser.Parse(root)) {
        error = path + ": " + parser.Error();
        return false;
    }
    const Json::Value& table = root["templates"];
    if (!table.IsObject()) {
        error = path + ": templates is an object of entities by name";
        return false;
    }

    // The scene as the converter writes one: resources first, then the root, then
    // each entity with its Sprite, Body and Shape - the node shape LevelBuilder,
    // Sprites and Lighting already read from every level.
    std::ostringstream resources;
    std::ostringstream nodes;
    int steps = 1;
    int index = 0;

    // The root, with the level's own root lighting keys and nothing else.
    const Tscn::Node* levelRoot = nullptr;
    for (const Tscn::Node& node : level.nodes) {
        if (node.path == ".") levelRoot = &node;
    }
    nodes << "[node name=\"templates\" type=\"Node2D\"]\n";
    if (levelRoot != nullptr) {
        for (const Tscn::Property& property : levelRoot->properties) {
            if (!property.key.starts_with("metadata/eth_") || property.value.kind != Tscn::Value::Kind::String) {
                continue;
            }
            std::string quoted;
            if (!Quoted(property.value.text, quoted)) continue;
            nodes << property.key << " = " << quoted << "\n";
        }
    }
    nodes << "\n";

    for (const auto& [entity, entry] : table.AsObject()) {
        if (entity.starts_with("_")) continue; // a note
        const std::string where = path + ": templates[\"" + entity + "\"]";
        const std::string node = entry["node"].AsString("");
        const std::string body = entry["body"].AsString("");
        const std::string sprite = entry["sprite"].AsString("");
        const Json::Value& size = entry["size_px"];
        const Json::Value& scale = entry["sprite_scale"];
        const Json::Value& metadata = entry["metadata"];
        std::string quotedEntity;
        if (node.empty() || node.find_first_of("\"/#.") != std::string::npos || !Quoted(entity, quotedEntity)) {
            error = where + ".node names the template's node, with no quote, slash, dot or #";
            return false;
        }
        if (body != "RigidBody2D" && body != "StaticBody2D") {
            error = where + ".body is RigidBody2D or StaticBody2D";
            return false;
        }
        if (!size.IsArray() || size.AsArray().size() != 2 || !size.AsArray()[0].IsNumber() ||
            !size.AsArray()[1].IsNumber() || !(size.AsArray()[0].AsNumber() > 0.0) ||
            !(size.AsArray()[1].AsNumber() > 0.0)) {
            error = where + ".size_px is its box, two numbers above zero, at its Scale";
            return false;
        }
        if (!scale.IsNumber() || !(scale.AsNumber() > 0.0)) {
            error = where + ".sprite_scale is its <Scale>, above zero";
            return false;
        }
        std::string quotedSprite;
        if (sprite.empty() || !Quoted(sprite, quotedSprite)) {
            error = where + ".sprite is its picture's res:// path";
            return false;
        }
        if (!metadata.IsObject()) {
            error = where + ".metadata is its custom data and eth_ keys, by name";
            return false;
        }

        const std::string texture = "tex_" + std::to_string(index);
        const std::string shape = "shape_" + std::to_string(index);
        resources << "[ext_resource type=\"Texture2D\" path=" << quotedSprite << " id=\"" << texture << "\"]\n";
        resources << "[sub_resource type=\"RectangleShape2D\" id=\"" << shape << "\"]\n";
        resources << "size = Vector2(" << Number(size.AsArray()[0].AsNumber()) << ", "
                  << Number(size.AsArray()[1].AsNumber()) << ")\n\n";
        steps += 2;

        nodes << "[node name=\"" << node << "\" type=\"Node2D\" parent=\".\"]\n";
        nodes << "position = Vector2(0, 0)\n";
        nodes << "metadata/entity_name = " << quotedEntity << "\n";
        for (const auto& [key, value] : metadata.AsObject()) {
            std::string quoted;
            if (key.starts_with("_")) continue; // a note
            if (key == "entity_name" || !value.IsString() || !Quoted(value.AsString(), quoted) ||
                key.find_first_of(" =\"") != std::string::npos) {
                error = where + ".metadata." + key + " is a quoted value under a plain key, and not entity_name";
                return false;
            }
            nodes << "metadata/" << key << " = " << quoted << "\n";
        }
        nodes << "\n[node name=\"Sprite\" type=\"Sprite2D\" parent=\"" << node << "\"]\n";
        nodes << "texture = ExtResource(\"" << texture << "\")\n";
        nodes << "scale = Vector2(" << Number(scale.AsNumber()) << ", " << Number(scale.AsNumber()) << ")\n\n";
        nodes << "[node name=\"Body\" type=\"" << body << "\" parent=\"" << node << "\"]\n\n";
        nodes << "[node name=\"Shape\" type=\"CollisionShape2D\" parent=\"" << node << "/Body\"]\n";
        nodes << "shape = SubResource(\"" << shape << "\")\n\n";
        ++index;
    }

    std::ostringstream scene;
    scene << "[gd_scene load_steps=" << steps << " format=3]\n\n" << resources.str() << nodes.str();
    Tscn::Scene read;
    std::string why;
    if (!Tscn::Parse(scene.str(), read, why)) {
        error = path + ": the templates do not read as a scene: " + why;
        return false;
    }
    out = std::move(read);
    return true;
}

} // namespace MagicPortals::Restock
