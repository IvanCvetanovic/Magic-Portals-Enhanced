#include "sim/StoneGrip.hpp"

#include "core/Components.hpp"
#include "core/Json.hpp"
#include "sim/Impacts.hpp"
#include "sim/Units.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

namespace MagicPortals::StoneGrip {

namespace Json = Supersonic::Json;

using Supersonic::RigidBodyComponent;

double Rules::Mix() const { return std::sqrt(playerFriction * stoneFriction); }

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
    const auto fail = [&](const std::string& why) {
        error = path + ": " + why;
        return false;
    };
    if (!root.IsObject() || !root.Has("grip") || !root["grip"].IsObject()) return fail("the grip block is missing");
    const Json::Value& grip = root["grip"];
    const auto number = [&](const char* key, double& into) {
        if (!grip.Has(key) || !grip[key].IsNumber()) return fail(std::string("grip.") + key + " is missing or not a number");
        into = grip[key].AsNumber();
        return true;
    };
    Rules read;
    if (!number("player_friction", read.playerFriction) || !number("stone_friction", read.stoneFriction) ||
        !number("touch_px", read.touchPx)) {
        return false;
    }
    if (read.playerFriction < 0.0 || read.stoneFriction <= 0.0) return fail("a friction is not a positive number");
    if (read.touchPx < 0.0) return fail("grip.touch_px is negative");
    out = read;
    return true;
}

bool Apply(entt::registry& registry, entt::entity player, const Rules& rules, const std::vector<entt::entity>& stones,
           double physicsS, State& state) {
    auto* rigid = player != entt::null && registry.valid(player) ? registry.try_get<RigidBodyComponent>(player) : nullptr;
    if (rigid == nullptr) {
        state = State{};
        return false;
    }

    // The friction the player must have for THIS stone, so that the engine's mix of the two (sqrt of the product, read per
    // contact from each body's own friction) is the decoded pair's: mix^2 over the stone's own. The largest wins where he
    // touches two.
    double wanted = 0.0;
    Impacts::Shape playerShape;
    if (!stones.empty() && Impacts::ShapeOf(registry, player, playerShape)) {
        const double mix = rules.Mix();
        for (const entt::entity stone : stones) {
            if (stone == entt::null || !registry.valid(stone)) continue;
            const auto* stoneRigid = registry.try_get<RigidBodyComponent>(stone);
            if (stoneRigid == nullptr || stoneRigid->friction <= 0.0f) continue;
            Impacts::Shape stoneShape;
            if (!Impacts::ShapeOf(registry, stone, stoneShape)) continue;
            // As far as the two could close in the step the engine is about to take: a landing's first step already has it.
            const glm::vec3 closing = rigid->velocity - stoneRigid->velocity;
            const double reachPx = rules.touchPx + std::hypot(static_cast<double>(closing.x), static_cast<double>(closing.y)) *
                                                       physicsS * Units::kPixelsPerMetre;
            if (Impacts::Between(stoneShape, playerShape).px > reachPx) continue;
            wanted = std::max(wanted, mix * mix / static_cast<double>(stoneRigid->friction));
        }
    }

    if (wanted > 0.0) {
        if (!state.on) {
            state.on = true;
            state.savedFriction = rigid->friction;
        }
        rigid->friction = static_cast<float>(wanted);
        ++state.ticks;
        return true;
    }
    if (state.on) {
        // Put back what he had before the grip, and nothing else: a suite that gave him a friction of its own keeps it.
        rigid->friction = state.savedFriction;
        state.on = false;
    }
    return false;
}

} // namespace MagicPortals::StoneGrip
