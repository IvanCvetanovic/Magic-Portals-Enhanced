#include "sim/Resting.hpp"

#include "core/Components.hpp"
#include "core/Json.hpp"
#include "sim/Impacts.hpp"

#include <fstream>
#include <sstream>

namespace MagicPortals::Resting {

namespace Json = Supersonic::Json;

bool LoadRules(const std::string& path, Rules& out, std::string& error) {
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
    if (!root["levels"].IsObject()) {
        error = path + ": no \"levels\" object";
        return false;
    }
    if (!root["wake_reach_px"].IsNumber() || !(root["wake_reach_px"].AsFloat() >= 0.0f)) {
        error = path + ": wake_reach_px is a number of at least 0";
        return false;
    }
    Rules read;
    read.wakeReachPx = root["wake_reach_px"].AsNumber();
    for (const auto& [level, entry] : root["levels"].AsObject()) {
        if (!entry.IsObject() || !entry["bodies"].IsArray() || entry["bodies"].AsArray().empty()) {
            error = path + ": " + level + " is an object with a list of bodies";
            return false;
        }
        std::vector<std::string>& names = read.byLevel[level];
        for (const Json::Value& name : entry["bodies"].AsArray()) {
            if (!name.IsString() || name.AsString().empty()) {
                error = path + ": " + level + ": a body is a node's name";
                return false;
            }
            names.push_back(name.AsString());
        }
    }
    out = std::move(read);
    return true;
}

bool Apply(entt::registry& registry, const Rules& rules, const std::string& level,
           const std::unordered_map<std::string, entt::entity>& built, std::string& error) {
    const auto listed = rules.byLevel.find(level);
    if (listed == rules.byLevel.end()) return true;
    for (const std::string& name : listed->second) {
        const auto found = built.find(name);
        auto* rigid = found != built.end() && registry.valid(found->second)
                          ? registry.try_get<Supersonic::RigidBodyComponent>(found->second)
                          : nullptr;
        const auto* transform = rigid != nullptr ? registry.try_get<Supersonic::TransformComponent>(found->second) : nullptr;
        if (rigid == nullptr || transform == nullptr || rigid->isKinematic) {
            error = "resting.json: " + level + " lists " + name + ", which the level does not build as a loose body";
            return false;
        }
        // What the engine itself writes when a body has been still long enough (PhysicsSystem, the sleep test): no velocity,
        // and the position it fell asleep at, which it compares each step to tell a body that was put somewhere else.
        rigid->isSleeping = true;
        rigid->sleepTimer = 0.0f;
        rigid->velocity = glm::vec3(0.0f);
        rigid->angularVelocity = glm::vec3(0.0f);
        rigid->sleepPosition = transform->position;
    }
    return true;
}

int WakeAround(entt::registry& registry, entt::entity body) {
    const Rules* rules = registry.ctx().find<Rules>();
    Impacts::Shape moved;
    if (rules == nullptr || !registry.valid(body) || !Impacts::ShapeOf(registry, body, moved)) return 0;
    const double reachPx = rules->wakeReachPx;
    int woken = 0;
    for (auto [entity, rigid] : registry.view<Supersonic::RigidBodyComponent>().each()) {
        if (entity == body || !rigid.isSleeping) continue;
        Impacts::Shape other;
        if (!Impacts::ShapeOf(registry, entity, other) || Impacts::Between(moved, other).px > reachPx) continue;
        // What the engine writes when it wakes a body that was put somewhere else: it is awake, and its timer starts again.
        rigid.isSleeping = false;
        rigid.sleepTimer = 0.0f;
        ++woken;
    }
    return woken;
}

} // namespace MagicPortals::Resting
