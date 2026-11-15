#include "sim/Goals.hpp"

#include "core/Components.hpp"
#include "core/Json.hpp"
#include "sim/Units.hpp"

#include <cmath>
#include <fstream>
#include <sstream>

namespace MagicPortals::Goals {

bool InPickupRange(const Pickup& pickup, const glm::dvec2& playerPx, const glm::dvec2& crystalPx) {
    const double range2 = pickup.rangePx * pickup.rangePx;
    // Squared distances against the squared range, STRICTLY: crystalPicker's CMPf; JNS (ins 145-146) and CMPf; TS (ins 172-173).
    const auto within = [&](const glm::dvec2& point) {
        const glm::dvec2 gap = playerPx - point;
        return gap.x * gap.x + gap.y * gap.y < range2;
    };
    return within(crystalPx - pickup.offsetPx) || within(crystalPx + pickup.offsetPx);
}

bool LoadRules(const std::string& path, Rules& out, std::string& error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = path + ": cannot open";
        return false;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string text = buffer.str();

    Supersonic::Json::Parser parser(text);
    Supersonic::Json::Value root;
    if (!parser.Parse(root)) {
        error = path + ": " + parser.Error();
        return false;
    }
    if (!root.IsObject() || !root.Has("level") || !root["level"].IsObject()) {
        error = path + ": no \"level\" object";
        return false;
    }
    const Supersonic::Json::Value& level = root["level"];
    if (!level.Has("exit_requires_all_crystals") || !level["exit_requires_all_crystals"].IsBool()) {
        error = path + ": level.exit_requires_all_crystals is missing or not a bool";
        return false;
    }
    out = Rules{};
    out.exitRequiresAllCrystals = level["exit_requires_all_crystals"].AsBool();
    return true;
}

bool LoadPickup(const std::string& path, Rules& out, std::string& error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = path + ": cannot open";
        return false;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    // Named, not a temporary: the parser keeps a reference to what it reads.
    const std::string text = buffer.str();

    Supersonic::Json::Parser parser(text);
    Supersonic::Json::Value root;
    if (!parser.Parse(root)) {
        error = path + ": " + parser.Error();
        return false;
    }
    const auto fail = [&](const std::string& why) {
        error = path + ": " + why;
        return false;
    };
    if (!root.IsObject() || !root.Has("pickup") || !root["pickup"].IsObject()) return fail("the pickup block is missing");
    const Supersonic::Json::Value& block = root["pickup"];
    if (!block.Has("range_px") || !block["range_px"].IsNumber()) return fail("pickup.range_px is missing or not a number");
    const double range = block["range_px"].AsNumber();
    // A range of nothing takes nothing, which would read as a rule that is built and never fires.
    if (!std::isfinite(range) || range <= 0.0) return fail("pickup.range_px is not a number above 0");
    if (!block.Has("offset_px") || !block["offset_px"].IsArray() || block["offset_px"].AsArray().size() != 2) {
        return fail("pickup.offset_px is missing or not two numbers");
    }
    glm::dvec2 offset(0.0);
    for (std::size_t i = 0; i < 2; ++i) {
        const Supersonic::Json::Value& part = block["offset_px"].AsArray()[i];
        if (!part.IsNumber() || !std::isfinite(part.AsNumber()) || part.AsNumber() < 0.0) {
            return fail("pickup.offset_px[" + std::to_string(i) + "] is not a number from 0 up");
        }
        offset[static_cast<glm::length_t>(i)] = part.AsNumber();
    }
    out.pickup.rangePx = range;
    out.pickup.offsetPx = offset;
    return true;
}

int State::Remaining() const {
    int remaining = 0;
    for (const Crystal& crystal : crystals) {
        if (!crystal.collected) ++remaining;
    }
    return remaining;
}

bool State::ExitOpen() const {
    return !rules.exitRequiresAllCrystals || Remaining() == 0;
}

void State::Tick(entt::registry& registry, entt::entity player, double dt) {
    if (player != entt::null && registry.valid(player)) {
        // crystalPicker measures from the character's entity point, which is the capsule's centre: the transform, with no
        // collider asked (the original's test is a distance poll, and crystal.ent has no body to touch).
        if (const auto* transform = registry.try_get<Supersonic::TransformComponent>(player)) {
            const glm::dvec2 at = Units::ToPixels(transform->position);
            for (Crystal& crystal : crystals) {
                if (!crystal.collected && !crystal.expired && InPickupRange(rules.pickup, at, crystal.pointPx)) {
                    crystal.collected = true;
                }
            }
        }
        const bool inExit = Trigger::Overlaps(registry, player, exit);
        if (inExit && !playerInExit) {
            ++exitEntries;
            if (ExitOpen()) completed = true;
        }
        playerInExit = inExit;
    }
    // Then the timed crystals run down (behaviours.gd:219-225). One with no time
    // left to begin with never goes, as in the Godot project. STRICTLY above its time,
    // as ETHCallback_timer's own test is (ins 102-103, CMPu elapsedTime, time; JNP skips
    // the vanish unless elapsedTime > time): a 10,000 ms crystal sums exactly 10000 on
    // its 625th tick and is still there, and goes on the 626th, 10016 (planning step
    // 147, fix 1).
    for (Crystal& crystal : crystals) {
        if (!crystal.timed || crystal.collected || crystal.expired || crystal.lifeMs <= 0.0) continue;
        crystal.elapsedMs += dt * 1000.0;
        if (crystal.elapsedMs > crystal.lifeMs) crystal.expired = true;
    }
}

const Crystal* State::FindCrystal(const std::string& name) const {
    for (const Crystal& crystal : crystals) {
        if (crystal.name == name) return &crystal;
    }
    return nullptr;
}

bool Find(const Tscn::Scene& scene, const Roles::Table& roles, const Rules& rules, State& out, std::string& error) {
    out = State{};
    out.rules = rules;
    int exits = 0;
    for (const Tscn::Node& node : scene.nodes) {
        if (node.parent != ".") continue;
        const std::string role = Roles::RoleOf(roles, node);
        if (role == Roles::kCollectible) {
            Crystal crystal;
            crystal.name = node.name;
            if (const Tscn::Value* time = node.Meta("time")) {
                double ms = 0.0;
                if (!time->AsNumber(ms)) {
                    error = node.name + "'s time is not a number";
                    return false;
                }
                crystal.timed = true;
                crystal.lifeMs = ms;
                crystal.lifeS = ms / 1000.0;
            }
            if (!Trigger::FromNode(node, crystal.box, error)) return false;
            // The placed point itself, in doubles: what the pickup measures from (Crystal::pointPx).
            if (const Tscn::Value* position = node.Find("position");
                position != nullptr && position->kind == Tscn::Value::Kind::Vector2) {
                crystal.pointPx = glm::dvec2(position->numbers[0], position->numbers[1]);
            }
            out.crystals.push_back(crystal);
        } else if (role == Roles::kExitDoor) {
            if (!Trigger::FromNode(node, out.exit, error)) return false;
            ++exits;
        }
    }
    if (exits != 1) {
        error = std::to_string(exits) + " exits, where a level has one";
        return false;
    }
    return true;
}

} // namespace MagicPortals::Goals
