#include "sim/Fire.hpp"

#include "sim/Units.hpp"

#include "core/Components.hpp"
#include "core/Json.hpp"
#include "core/PhysicsSystem.hpp"

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <sstream>

namespace MagicPortals::Fire {

namespace {

namespace Json = Supersonic::Json;
using Supersonic::TransformComponent;

// A node's metadata as a flag. The converter writes the original's custom data as
// STRINGS - metadata/burnable = "1" - which Tscn::Value::AsNumber reads.
bool Flagged(const Tscn::Node& node, const char* key) {
    const Tscn::Value* value = node.Meta(key);
    double read = 0.0;
    if (value == nullptr || !value->AsNumber(read)) return false;
    return read != 0.0;
}

glm::dvec2 PxOf(entt::registry& registry, entt::entity body) {
    return Units::ToPixels(registry.get<TransformComponent>(body).position);
}

double SquaredPx(const glm::dvec2& v) {
    return v.x * v.x + v.y * v.y;
}

// One bomb going off, at the point where it stood. Its own body is already gone,
// which is also what keeps the line-of-sight ray from stopping on it.
//
// The grabber is a sphere against each candidate's REAL collider, and then a ray:
// an entity whose first obstruction is not itself is behind something, and the
// blast does not reach it.
void Blast(State& state, entt::registry& registry, const glm::dvec2& atPx, const std::string& by) {
    const glm::vec3 centre = Units::ToWorld(atPx.x, atPx.y);

    std::vector<entt::entity> reached;
    Supersonic::PhysicsSystem::OverlapSphere(registry, centre, Units::ToMetres(state.rules.blastPx), reached,
                                             entt::null, false);

    for (const entt::entity other : reached) {
        if (!registry.valid(other) || !registry.all_of<TransformComponent>(other)) continue;

        const glm::vec3 to = registry.get<TransformComponent>(other).position - centre;
        const float distance = glm::length(to);
        if (distance > 1e-4f) {
            const Supersonic::PhysicsSystem::RayHit seen =
                Supersonic::PhysicsSystem::Raycast(registry, centre, to / distance, distance, entt::null, false);
            if (seen.hit && seen.entity != other) continue;
        }
        state.grabbedNow.push_back(Grab{by, other});

        // A bomb is ASKED, not detonated: it goes off in its own turn, next tick.
        for (Bomb& bomb : state.bombs) {
            if (bomb.body == other && !bomb.blown) bomb.requested = true;
        }
        for (Burnable& burnable : state.burnables) {
            if (burnable.body != other || burnable.burned || burnable.gone) continue;
            burnable.burned = true;
            ++state.lit;
        }
    }
}

} // namespace

bool LoadRules(const std::string& path, Rules& out, std::string& error) {
    out = Rules{};

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = path + ": cannot open";
        return false;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();

    // Named, not a temporary: Json::Parser holds its text by reference.
    const std::string text = buffer.str();
    Json::Parser parser(text);
    Json::Value root;
    if (!parser.Parse(root)) {
        error = path + ": " + parser.Error();
        return false;
    }

    const Json::Value& fire = root["fire"];
    const Json::Value& burn = root["burn"];
    const Json::Value& blast = root["blast"];
    const Json::Value& lava = root["lava"];
    if (!fire.IsObject() || !burn.IsObject() || !blast.IsObject() || !lava.IsObject()) {
        error = path + ": fire, burn, blast and lava are each an object";
        return false;
    }

    Rules read;
    read.reachPx = fire["reach_px"].AsNumber(0.0);
    read.burnMs = burn["time_ms"].AsNumber(0.0);
    read.blastPx = blast["radius_px"].AsNumber(0.0);

    const Json::Value& names = burn["fade_names"];
    if (!names.IsArray()) {
        error = path + ": burn.fade_names is an array";
        return false;
    }
    for (const Json::Value& name : names.AsArray()) {
        if (!name.IsString()) {
            error = path + ": burn.fade_names lists a name that is not a string";
            return false;
        }
        read.fadeNames.push_back(name.AsString());
    }
    const Json::Value& lavaNames = lava["entities"];
    if (!lavaNames.IsArray() || lavaNames.AsArray().empty()) {
        error = path + ": lava.entities is an array naming at least one entity";
        return false;
    }
    for (const Json::Value& name : lavaNames.AsArray()) {
        if (!name.IsString()) {
            error = path + ": lava.entities lists a name that is not a string";
            return false;
        }
        read.lavaEntities.push_back(name.AsString());
    }
    if (!lava["margin_px"].IsNumber() || lava["margin_px"].AsNumber() < 0.0) {
        error = path + ": lava.margin_px is a number, not below zero";
        return false;
    }
    read.lavaMarginPx = lava["margin_px"].AsNumber();

    // Each of these would look like a level that works. A reach of nothing is a
    // fire agent that never lights anything; a burn of nothing takes a crate away
    // the instant it catches, which is a different puzzle; a blast of nothing is a
    // bomb that goes off and touches not one thing; and an empty fade list makes
    // burning a flag and nothing more, everywhere.
    if (read.reachPx <= 0.0) {
        error = path + ": fire.reach_px is above zero";
        return false;
    }
    if (read.burnMs <= 0.0) {
        error = path + ": burn.time_ms is above zero";
        return false;
    }
    if (read.blastPx <= 0.0) {
        error = path + ": blast.radius_px is above zero";
        return false;
    }
    if (read.fadeNames.empty()) {
        error = path + ": burn.fade_names names at least one entity";
        return false;
    }

    out = std::move(read);
    return true;
}

std::vector<entt::entity> State::Tick(entt::registry& registry, entt::entity player, float dt) {
    std::vector<entt::entity> removed;
    blastsPx.clear();
    grabbedNow.clear();

    // The bombs asked for BEFORE this tick go off now, and the set is taken first:
    // a blast below may ask another bomb, and that one waits its own turn. This is
    // the flag-and-poll split the original has by construction, and it is what
    // makes a row of bombs ripple rather than all go off in one frame.
    std::vector<std::size_t> going;
    for (std::size_t index = 0; index < bombs.size(); ++index) {
        if (bombs[index].requested && !bombs[index].blown) going.push_back(index);
    }
    for (const std::size_t index : going) {
        Bomb& bomb = bombs[index];
        bomb.blown = true;
        if (!registry.valid(bomb.body)) continue;
        const glm::dvec2 atPx = PxOf(registry, bomb.body);
        registry.destroy(bomb.body);
        removed.push_back(bomb.body);
        ++blasts;
        blastsPx.push_back(atPx);
        Blast(*this, registry, atPx, bomb.name);
    }

    // Then the fire agents, which never move and have no body of their own.
    const double reach2 = rules.reachPx * rules.reachPx;
    for (const Agent& agent : agents) {
        if (!playerKilled && player != entt::null && registry.valid(player) &&
            registry.all_of<TransformComponent>(player)) {
            if (SquaredPx(PxOf(registry, player) - agent.atPx) < reach2) {
                playerKilled = true;
                killedBy = agent.name;
            }
        }
        for (Bomb& bomb : bombs) {
            if (bomb.blown || bomb.requested || !registry.valid(bomb.body)) continue;
            if (SquaredPx(PxOf(registry, bomb.body) - agent.atPx) < reach2) bomb.requested = true;
        }
        for (Burnable& burnable : burnables) {
            if (burnable.burned || burnable.gone || !registry.valid(burnable.body)) continue;
            if (SquaredPx(PxOf(registry, burnable.body) - agent.atPx) >= reach2) continue;
            burnable.burned = true;
            ++lit;
        }
    }

    // And what the lava touches (ETHBeginContactCallback_lava #1-30): burned when it
    // is burnable, then asked to go off whatever it is - which only a bomb's own
    // callback reads, on its next turn, as it reads a fireball's.
    const float lavaMargin = Units::ToMetres(rules.lavaMarginPx);
    for (const Lava& lava : lavas) {
        if (!registry.valid(lava.body)) continue;
        Trigger::Box reach = lava.box;
        reach.half += glm::vec2(lavaMargin);
        for (Burnable& burnable : burnables) {
            if (burnable.burned || burnable.gone || !registry.valid(burnable.body)) continue;
            if (!Trigger::Overlaps(registry, burnable.body, reach)) continue;
            burnable.burned = true;
            ++lit;
        }
        for (Bomb& bomb : bombs) {
            if (bomb.blown || bomb.requested || !registry.valid(bomb.body)) continue;
            if (Trigger::Overlaps(registry, bomb.body, reach)) bomb.requested = true;
        }
    }

    // And what is alight burns down. Only a crate does: burn() sets the flag, and
    // the fade that ends in removal is manageBurnable's, whose one caller is
    // crateCallback. Anything else flagged burnable just carries `burned`.
    const double ms = static_cast<double>(dt) * 1000.0;
    for (Burnable& burnable : burnables) {
        if (!burnable.burned || burnable.gone || !burnable.fades) continue;
        burnable.burningMs += ms;
        if (burnable.burningMs < rules.burnMs) continue;
        burnable.gone = true;
        if (!registry.valid(burnable.body)) continue;
        registry.destroy(burnable.body);
        removed.push_back(burnable.body);
    }

    return removed;
}

const Agent* State::Reaching(const glm::dvec2& pointPx) const {
    const double reach2 = rules.reachPx * rules.reachPx;
    for (const Agent& agent : agents) {
        if (SquaredPx(pointPx - agent.atPx) < reach2) return &agent;
    }
    return nullptr;
}

const Burnable* State::FindBurnable(const std::string& name) const {
    for (const Burnable& burnable : burnables) {
        if (burnable.name == name) return &burnable;
    }
    return nullptr;
}

const Bomb* State::FindBomb(const std::string& name) const {
    for (const Bomb& bomb : bombs) {
        if (bomb.name == name) return &bomb;
    }
    return nullptr;
}

void State::Adopt(const Tscn::Node& node, const std::string& name, entt::entity body) {
    if (body == entt::null) return;
    if (Flagged(node, "explosive")) {
        Bomb bomb;
        bomb.name = name;
        bomb.body = body;
        bombs.push_back(bomb);
    }
    if (Flagged(node, "burnable")) {
        const std::string entityName = Roles::EntityName(node);
        Burnable made;
        made.name = name;
        made.body = body;
        made.fades = std::find(rules.fadeNames.begin(), rules.fadeNames.end(), entityName) != rules.fadeNames.end();
        burnables.push_back(made);
    }
}

bool Find(const Tscn::Scene& scene, const Roles::Table& roles, const LevelBuilder::Built& built, const Rules& rules,
          State& out, std::string& error) {
    out = State{};
    out.rules = rules;

    for (const Tscn::Node& node : scene.nodes) {
        if (node.parent != ".") continue;

        if (Roles::RoleOf(roles, node) == Roles::kFireSource) {
            const Tscn::Value* position = node.Find("position");
            if (position == nullptr || position->kind != Tscn::Value::Kind::Vector2) {
                error = node.name + " is a fire source with no position";
                return false;
            }
            Agent agent;
            agent.name = node.name;
            agent.atPx = glm::dvec2(position->numbers[0], position->numbers[1]);
            out.agents.push_back(agent);
        }

        // The lava, by the name its contact callback is bound to.
        if (std::find(rules.lavaEntities.begin(), rules.lavaEntities.end(), Roles::EntityName(node)) !=
            rules.lavaEntities.end()) {
            const auto body = built.entities.find(node.name);
            glm::dvec2 offsetPx(0.0);
            glm::dvec2 sizePx(0.0);
            const Tscn::Value* position = node.Find("position");
            if (body != built.entities.end() && position != nullptr &&
                position->kind == Tscn::Value::Kind::Vector2 &&
                LevelBuilder::ShapeBoundsPx(scene, node, offsetPx, sizePx)) {
                double turned = 0.0;
                const Tscn::Value* rotation = node.Find("rotation");
                if (rotation != nullptr && rotation->AsNumber(turned) && turned != 0.0) {
                    error = node.name + " is a turned lava, whose box would turn with it";
                    return false;
                }
                const glm::dvec2 centrePx = glm::dvec2(position->numbers[0], position->numbers[1]) + offsetPx;
                const glm::vec3 centre = Units::ToWorld(centrePx.x, centrePx.y);
                Lava lava;
                lava.name = node.name;
                lava.body = body->second;
                lava.box.centre = glm::vec2(centre.x, centre.y);
                lava.box.half = glm::vec2(Units::ToMetres(sizePx.x * 0.5), Units::ToMetres(sizePx.y * 0.5));
                out.lavas.push_back(lava);
            }
        }

        // Flags, not roles: the original tests custom data, and a burnable is a
        // crate or a shock agent or a minion rather than a role of its own.
        const bool explosive = Flagged(node, "explosive");
        const bool burnable = Flagged(node, "burnable");
        if (!explosive && !burnable) continue;

        // A flagged entity with no body built is passed over, which is every
        // static one when the level is started without its statics.
        const auto found = built.entities.find(node.name);
        if (found == built.entities.end()) continue;

        if (explosive) {
            Bomb bomb;
            bomb.name = node.name;
            bomb.body = found->second;
            out.bombs.push_back(bomb);
        }
        if (burnable) {
            const std::string entityName = Roles::EntityName(node);
            Burnable made;
            made.name = node.name;
            made.body = found->second;
            made.fades = std::find(rules.fadeNames.begin(), rules.fadeNames.end(), entityName) != rules.fadeNames.end();
            out.burnables.push_back(made);
        }
    }
    return true;
}

} // namespace MagicPortals::Fire
