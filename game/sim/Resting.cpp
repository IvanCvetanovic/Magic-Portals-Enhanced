#include "sim/Resting.hpp"

#include "core/Components.hpp"
#include "core/Json.hpp"
#include "sim/Impacts.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

namespace MagicPortals::Resting {

namespace Json = Supersonic::Json;
using Supersonic::RigidBodyComponent;
using Supersonic::TagComponent;
using Supersonic::TransformComponent;

namespace {

std::string NameOf(const entt::registry& registry, entt::entity entity) {
    const auto* tag = registry.valid(entity) ? registry.try_get<TagComponent>(entity) : nullptr;
    return tag != nullptr ? tag->tag : std::string();
}

bool Listed(const Supports& supports, entt::entity entity) {
    return std::any_of(supports.list.begin(), supports.list.end(),
                       [entity](const Support& support) { return support.entity == entity; });
}

void Add(entt::registry& registry, Supports& supports, entt::entity entity, double gapUnits = 0.0) {
    if (Listed(supports, entity)) return;
    Support support;
    support.entity = entity;
    support.name = NameOf(registry, entity);
    support.gapUnits = gapUnits;
    if (const auto* transform = registry.try_get<TransformComponent>(entity)) support.position = transform->position;
    supports.list.push_back(std::move(support));
}

// What the sleeper touches now, by Box2D's own test of a contact: a pair that collides by its masks and whose surfaces are within
// the skin of each other (a sensor makes none: ShapeOf is false for a trigger). Added to what the level named for it.
void Scan(entt::registry& registry, const Rules& rules, entt::entity sleeper, Supports& supports) {
    Impacts::Shape own;
    if (!Impacts::ShapeOf(registry, sleeper, own)) return;
    // What the level named, with how far off the port leaves it (where the gap is a number a reader can check against the still).
    for (Support& support : supports.list) {
        Impacts::Shape named;
        if (registry.valid(support.entity) && Impacts::ShapeOf(registry, support.entity, named)) {
            support.gapUnits = Impacts::Between(own, named).px;
        }
    }
    for (const entt::entity entity : registry.view<TransformComponent>()) {
        if (entity == sleeper || Listed(supports, entity)) continue;
        Impacts::Shape other;
        if (!Impacts::ShapeOf(registry, entity, other)) continue;
        const double gap = Impacts::Between(own, other).px;
        if (gap > rules.touchUnits) continue;
        if (!Impacts::Collides(registry, sleeper, entity)) continue;
        Add(registry, supports, entity, gap);
    }
}

void Report(const entt::registry& registry, entt::entity sleeper, const Supports& supports, std::vector<Looked>* looked) {
    if (looked == nullptr) return;
    Looked one;
    one.sleeper = sleeper;
    one.sleeperName = NameOf(registry, sleeper);
    for (const Support& support : supports.list) one.supports.emplace_back(support.name, support.gapUnits);
    looked->push_back(std::move(one));
}

// What the engine writes when it wakes a body that was put somewhere else: it is awake, and its timer starts again.
void WakeBody(entt::registry& registry, entt::entity entity) {
    if (auto* rigid = registry.try_get<RigidBodyComponent>(entity)) {
        rigid->isSleeping = false;
        rigid->sleepTimer = 0.0f;
    }
    registry.remove<Supports>(entity);
}

// A loose body the engine is simulating: it moves by the solver, which wakes what rests on it when it stirs, so its place is not
// a thing the port moved.
bool IsAwakeLoose(const entt::registry& registry, entt::entity entity) {
    const auto* rigid = registry.try_get<RigidBodyComponent>(entity);
    return rigid != nullptr && !rigid->isKinematic && !rigid->isSleeping;
}

} // namespace

const char* CauseName(Cause cause) {
    switch (cause) {
    case Cause::Gone: return "destroyed";
    case Cause::NotSolid: return "no longer solid";
    case Cause::Moved: return "moved";
    }
    return "?";
}

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
    if (!root["touch_units"].IsNumber() || !(root["touch_units"].AsFloat() >= 0.0f)) {
        error = path + ": touch_units is a number of at least 0";
        return false;
    }
    if (!root["moved_m"].IsNumber() || !(root["moved_m"].AsFloat() > 0.0f)) {
        error = path + ": moved_m is a number above 0";
        return false;
    }
    Rules read;
    read.touchUnits = root["touch_units"].AsNumber();
    read.movedMetres = root["moved_m"].AsNumber();
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
        if (!entry.Has("rests_on")) continue;
        if (!entry["rests_on"].IsObject()) {
            error = path + ": " + level + ": rests_on is an object of body to the nodes it rests on";
            return false;
        }
        for (const auto& [body, supports] : entry["rests_on"].AsObject()) {
            if (std::find(names.begin(), names.end(), body) == names.end()) {
                error = path + ": " + level + ": rests_on names " + body + ", which is not one of its bodies";
                return false;
            }
            if (!supports.IsArray() || supports.AsArray().empty()) {
                error = path + ": " + level + ": " + body + " rests on a list of nodes";
                return false;
            }
            std::vector<std::string>& held = read.restsOn[level][body];
            for (const Json::Value& support : supports.AsArray()) {
                if (!support.IsString() || support.AsString().empty()) {
                    error = path + ": " + level + ": " + body + ": a support is a node's name";
                    return false;
                }
                held.push_back(support.AsString());
            }
        }
    }
    out = std::move(read);
    return true;
}

bool Apply(entt::registry& registry, const Rules& rules, const std::string& level,
           const std::unordered_map<std::string, entt::entity>& built, std::string& error) {
    const auto listed = rules.byLevel.find(level);
    if (listed == rules.byLevel.end()) return true;
    const auto resting = rules.restsOn.find(level);
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
        // What the still shows under it, which a gap the port leaves where the level places the body would hide from a scan.
        Supports supports;
        const std::vector<std::string>* named = nullptr;
        if (resting != rules.restsOn.end()) {
            const auto held = resting->second.find(name);
            if (held != resting->second.end()) named = &held->second;
        }
        if (named != nullptr) {
            for (const std::string& node : *named) {
                const auto support = built.find(node);
                if (support == built.end() || !registry.valid(support->second)) {
                    error = "resting.json: " + level + ": " + name + " rests on " + node + ", which the level did not build";
                    return false;
                }
                Add(registry, supports, support->second);
            }
        }
        registry.emplace_or_replace<Supports>(found->second, std::move(supports));
    }
    return true;
}

std::vector<Woken> Watch(entt::registry& registry, std::vector<Looked>* looked) {
    std::vector<Woken> woken;
    const Rules* rules = registry.ctx().find<Rules>();
    if (rules == nullptr) return woken;

    // What a body that woke by any other way no longer rests on is forgotten, so the next time it sleeps it is looked at again.
    std::vector<entt::entity> forgotten;
    for (const entt::entity entity : registry.view<Supports>()) {
        const auto* rigid = registry.try_get<RigidBodyComponent>(entity);
        if (rigid == nullptr || !rigid->isSleeping) forgotten.push_back(entity);
    }
    for (const entt::entity entity : forgotten) registry.remove<Supports>(entity);

    std::vector<entt::entity> sleepers;
    for (auto [entity, rigid] : registry.view<RigidBodyComponent>().each()) {
        if (rigid.isSleeping && !rigid.isKinematic) sleepers.push_back(entity);
    }
    const float movedMetres = static_cast<float>(rules->movedMetres);
    for (const entt::entity sleeper : sleepers) {
        Supports* supports = registry.try_get<Supports>(sleeper);
        if (supports == nullptr) supports = &registry.emplace<Supports>(sleeper);
        if (!supports->scanned) {
            // First seen asleep: what it touches now is what it rests on, and nothing has moved since.
            supports->scanned = true;
            Scan(registry, *rules, sleeper, *supports);
            Report(registry, sleeper, *supports, looked);
            continue;
        }
        for (Support& support : supports->list) {
            Woken why;
            why.sleeper = sleeper;
            why.sleeperName = NameOf(registry, sleeper);
            why.supportName = support.name;
            bool leaves = false;
            Impacts::Shape shape;
            if (!registry.valid(support.entity)) {
                leaves = true;
                why.cause = Cause::Gone;
            } else if (!Impacts::ShapeOf(registry, support.entity, shape)) {
                leaves = true;
                why.cause = Cause::NotSolid;
            } else if (const auto* transform = registry.try_get<TransformComponent>(support.entity)) {
                if (IsAwakeLoose(registry, support.entity)) {
                    support.position = transform->position;
                } else {
                    const glm::vec3 drift = glm::abs(transform->position - support.position);
                    if (drift.x > movedMetres || drift.y > movedMetres || drift.z > movedMetres) {
                        leaves = true;
                        why.cause = Cause::Moved;
                    }
                }
            }
            if (!leaves) continue;
            woken.push_back(std::move(why));
            WakeBody(registry, sleeper);
            break;
        }
    }
    return woken;
}

int WakeAround(entt::registry& registry, entt::entity body) {
    const Rules* rules = registry.ctx().find<Rules>();
    if (rules == nullptr || !registry.valid(body)) return 0;
    std::vector<entt::entity> sleepers;
    for (auto [entity, rigid] : registry.view<RigidBodyComponent>().each()) {
        if (entity != body && rigid.isSleeping && !rigid.isKinematic) sleepers.push_back(entity);
    }
    int woken = 0;
    for (const entt::entity sleeper : sleepers) {
        Supports* supports = registry.try_get<Supports>(sleeper);
        if (supports == nullptr) supports = &registry.emplace<Supports>(sleeper);
        if (!supports->scanned) {
            supports->scanned = true;
            Scan(registry, *rules, sleeper, *supports);
        }
        if (!Listed(*supports, body)) continue;
        WakeBody(registry, sleeper);
        ++woken;
    }
    return woken;
}

} // namespace MagicPortals::Resting
