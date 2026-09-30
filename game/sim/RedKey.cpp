#include "sim/RedKey.hpp"

#include "core/Json.hpp"
#include "sim/Ease.hpp"

#include <cmath>
#include <cstddef>
#include <fstream>
#include <sstream>

namespace MagicPortals::RedKey {

namespace Json = Supersonic::Json;

namespace {

constexpr const char* kBlock = "red_key_state";

bool PositionOf(const Tscn::Node& node, glm::dvec2& out) {
    const Tscn::Value* position = node.Find("position");
    if (position == nullptr || position->kind != Tscn::Value::Kind::Vector2) return false;
    out = glm::dvec2(position->numbers[0], position->numbers[1]);
    return true;
}

} // namespace

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
    if (!root.IsObject() || !root.Has(kBlock) || !root[kBlock].IsObject()) {
        error = path + ": the red_key_state block is missing";
        return false;
    }
    const Json::Value& block = root[kBlock];
    const auto fail = [&](const std::string& why) {
        error = path + ": red_key_state." + why;
        return false;
    };
    const auto nameOf = [&](const char* key, std::string& into) {
        if (!block.Has(key) || !block[key].IsString() || block[key].AsString().empty()) {
            return fail(std::string(key) + " is missing or not a name");
        }
        into = block[key].AsString();
        return true;
    };
    const auto number = [&](const char* key, double& into) {
        if (!block.Has(key) || !block[key].IsNumber() || !(block[key].AsNumber() > 0.0)) {
            return fail(std::string(key) + " is missing or not above zero");
        }
        into = block[key].AsNumber();
        return true;
    };
    const auto numbers = [&](const char* key, std::size_t count, double* into) {
        if (!block.Has(key) || !block[key].IsArray() || block[key].AsArray().size() != count) {
            return fail(std::string(key) + " is missing or not " + std::to_string(count) + " numbers");
        }
        for (std::size_t i = 0; i < count; ++i) {
            const Json::Value& value = block[key].AsArray()[i];
            if (!value.IsNumber() || value.AsNumber() < 0.0) return fail(std::string(key) + " holds a bad number");
            into[i] = value.AsNumber();
        }
        return true;
    };

    Rules read;
    double colour[3] = {0.0, 0.0, 0.0};
    double scale[2] = {0.0, 0.0};
    if (!nameOf("key_entity", read.keyEntity) || !nameOf("wall_entity", read.wallEntity) ||
        !number("fade_ms", read.fadeMs) || !numbers("colour", 3, colour) || !numbers("scale", 2, scale) ||
        !number("bucket_units", read.bucketUnits)) {
        return false;
    }
    // A halo scaled to nothing is a picture the level never shows.
    if (scale[0] <= 0.0 || scale[1] <= 0.0) return fail("scale is not above zero");
    read.colour = glm::dvec3(colour[0], colour[1], colour[2]);
    read.scale = glm::dvec2(scale[0], scale[1]);
    out = std::move(read);
    return true;
}

glm::ivec2 State::BucketOf(const glm::dvec2& point) const {
    return glm::ivec2(static_cast<int>(std::floor(point.x / rules.bucketUnits)),
                      static_cast<int>(std::floor(point.y / rules.bucketUnits)));
}

double State::BiasAt(double ms) const {
    return Ease::SmoothBeginning(Ease::SmoothEnd(1.0 - ms / rules.fadeMs));
}

State::Turn State::Tick(const Keys::State& keys, double ms) {
    Turn turn;
    if (!present || gone) return turn;

    if (!destroyed) {
        // seekEntityFromBucket: the first key of that exact name in its bucket, and
        // only that one is asked.
        const glm::ivec2 bucket = BucketOf(atPx);
        for (const Keys::Key& key : keys.keys) {
            bool named = false;
            for (const std::string& node : keyNodes) named = named || node == key.name;
            if (!named || BucketOf(key.atPx) != bucket) continue;
            if (key.owner != entt::null) {
                destroyed = true;
                takenBy = key.name;
                turn.taken = true;
            }
            break;
        }
        return turn;
    }

    // Destroyed on an earlier tick: the callback's other branch, from the next.
    elapsedMs += ms;
    bias = BiasAt(elapsedMs);
    if (elapsedMs >= rules.fadeMs) {
        gone = true;
        turn.destroyWall = true;
    }
    return turn;
}

bool Find(const Tscn::Scene& scene, const Roles::Table& roles, const Rules& rules, State& out, std::string& error) {
    out = State{};
    out.rules = rules;
    for (const Tscn::Node& node : scene.nodes) {
        if (node.parent != ".") continue;
        if (Roles::EntityName(node) == rules.keyEntity) out.keyNodes.push_back(node.name);
        if (Roles::RoleOf(roles, node) != Roles::kRedKeyState) continue;
        if (out.present) {
            error = node.name + " is a second red_key_state, and the game places one";
            return false;
        }
        if (!PositionOf(node, out.atPx)) {
            error = node.name + " has no position";
            return false;
        }
        out.present = true;
        out.name = node.name;
    }
    return true;
}

} // namespace MagicPortals::RedKey
