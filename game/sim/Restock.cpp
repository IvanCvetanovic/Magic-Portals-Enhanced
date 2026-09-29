#include "sim/Restock.hpp"

#include "core/Components.hpp"
#include "sim/Roles.hpp"
#include "sim/Units.hpp"

namespace MagicPortals::Restock {

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
            }
        }
        if (site.templateNode.empty() && entity == rule.templateEntity) site.templateNode = node.name;
    }
    return site;
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

entt::entity State::Add(const Tscn::Scene& scene, entt::registry& registry, const LevelBuilder::Options& options,
                        const Rule& rule, const Site& site, LevelBuilder::Built& built, std::string& error) {
    if (!site.Ready()) {
        error = site.spawnFound ? "no " + rule.templateEntity + " in the level to build " + rule.add + " from"
                                : "no " + rule.spawn + " in the level to add " + rule.add + " at";
        return entt::null;
    }
    // Built into a scratch table, so the template's own entry - the placed body,
    // whether or not it still stands - is not written over.
    LevelBuilder::Built scratch;
    const entt::entity body = LevelBuilder::BuildEntity(scene, site.templateNode, registry, options, scratch, error);
    if (body == entt::null) return entt::null;

    Added made;
    made.name = site.templateNode + "#" + std::to_string(++count);
    made.alias = rule.alias;
    made.templateNode = site.templateNode;
    made.body = body;
    made.atPx = site.spawnPx;
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

} // namespace MagicPortals::Restock
