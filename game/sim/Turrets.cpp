#include "sim/Turrets.hpp"

#include "sim/Trigger.hpp"
#include "sim/Units.hpp"

#include "core/Json.hpp"

#include <cmath>
#include <fstream>
#include <sstream>
#include <vector>

namespace MagicPortals::Turrets {

namespace {

namespace Json = Supersonic::Json;

// A [x, y] pair, as turrets.json writes both of its.
bool ReadPair(const Json::Value& value, glm::dvec2& out) {
    if (!value.IsArray() || value.AsArray().size() != 2) return false;
    if (!value.AsArray()[0].IsNumber() || !value.AsArray()[1].IsNumber()) return false;
    out = glm::dvec2(value.AsArray()[0].AsNumber(0.0), value.AsArray()[1].AsNumber(0.0));
    return true;
}

// A node's metadata as a number, or `fallback`. The converter writes metadata as
// STRINGS - metadata/dirX = "1.2" - which Tscn::Value::AsNumber reads.
double Meta(const Tscn::Node& node, const char* key, double fallback) {
    const Tscn::Value* value = node.Meta(key);
    double read = 0.0;
    if (value == nullptr || !value->AsNumber(read)) return fallback;
    return read;
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

    const Json::Value& fireball = root["fireball"];
    const Json::Value& stride = root["stride"];
    const Json::Value& cull = root["cull"];
    if (!fireball.IsObject() || !stride.IsObject() || !cull.IsObject()) {
        error = path + ": fireball, stride and cull are each an object";
        return false;
    }

    Rules read;
    read.speedPx = fireball["speed_px_s"].AsNumber(0.0);
    read.radiusPx = fireball["radius_px"].AsNumber(0.0);
    read.portalGrabPx = fireball["portal_grab_px"].AsNumber(0.0);
    read.defaultStrideMs = stride["default_ms"].AsNumber(0.0);
    if (!ReadPair(cull["margin_px"], read.cullMarginPx)) {
        error = path + ": margin_px is two numbers";
        return false;
    }
    // A speed of nothing is a fireball that never leaves the gargoyle's mouth,
    // and a stride of nothing is a carranca that fires every tick. Both would
    // look like a level that works.
    if (read.speedPx <= 0.0 || read.defaultStrideMs <= 0.0) {
        error = path + ": speed_px_s and default_ms are both above zero";
        return false;
    }
    // A fireball of no size touches nothing, and would fly through every wall.
    if (read.radiusPx <= 0.0) {
        error = path + ": radius_px is above zero";
        return false;
    }
    // And one with no share of the grab would go through a portal only once its
    // centre was inside the portal's own 14, so a portal would miss every fireball
    // that passes it between 14 and 22 off its centre.
    if (read.portalGrabPx <= 0.0) {
        error = path + ": portal_grab_px is above zero";
        return false;
    }

    out = read;
    return true;
}

std::vector<Fireball> State::Fire(double dt) {
    std::vector<Fireball> now;
    const double ms = static_cast<double>(dt) * 1000.0;
    for (Turret& turret : turrets) {
        turret.elapsedMs += ms;
        // PASSES the stride, as carrancaCallback's CMPu does, rather than
        // reaching it.
        if (turret.elapsedMs <= turret.strideMs) continue;

        // RESET, not a subtraction. The original writes zero, so a long frame's
        // overshoot is not carried into the next shot; subtracting the stride
        // would make a carranca fire twice in quick succession to catch up.
        turret.elapsedMs = 0.0;

        Fireball made;
        made.name = turret.name + "#" + std::to_string(++turret.fired);
        // At the carranca's own position. The Godot project spawns one 24 px along the
        // direction (hazards.gd:365); addFireball uses GetPositionXY and nothing
        // else, so that offset is the Godot project's own.
        made.atPx = turret.atPx;
        made.velocityPx = turret.directionPx * rules.speedPx;
        fireballs.push_back(made);
        now.push_back(made);
    }
    return now;
}

void State::Tick(entt::registry& registry, entt::entity player, double dt) {
    // Taken first, as Fire::Tick takes its blasts: what this tick destroys is
    // what the layer reads after it, and last tick's is gone.
    spentPx.clear();

    const glm::dvec2 lowPx = -rules.cullMarginPx;
    const glm::dvec2 highPx = boundsPx + rules.cullMarginPx;

    for (Fireball& ball : fireballs) ball.atPx += ball.velocityPx * static_cast<double>(dt);
    std::erase_if(fireballs, [&lowPx, &highPx](const Fireball& ball) {
        return ball.atPx.x < lowPx.x || ball.atPx.y < lowPx.y || ball.atPx.x > highPx.x || ball.atPx.y > highPx.y;
    });

    if (player == entt::null || !registry.valid(player) || playerKilled) return;

    // The fireball's own circle against the player's own collider: Trigger is
    // exact in the plane, and a capsule is what the character has.
    const float radius = Units::ToMetres(rules.radiusPx);
    for (std::size_t i = 0; i < fireballs.size(); ++i) {
        const Fireball& ball = fireballs[i];
        // addFireball's killMainCharacter. A fire diamond's conversion passes it
        // clear and the contact callback bails out on a character when it is, so
        // one of these kills and the other cannot. Turrets.hpp has the decode.
        if (!ball.killsPlayer) continue;
        const glm::vec3 centre = Units::ToWorld(ball.atPx.x, ball.atPx.y);
        const Trigger::Circle circle{glm::vec2(centre.x, centre.y), radius};
        if (!Trigger::Overlaps(registry, player, circle)) continue;
        playerKilled = true;
        killedBy = ball.name;
        // And the fireball GOES, with a destroy recorded. The one that kills sets
        // flames on the character and its hp to 0
        // (ETHBeginContactCallback_fireball #123-145) and then falls through to
        // #146-152 destroy(thisEntity) like any other contact - it is not a
        // special case there, and the player is never the end of a fireball's
        // flight in the original.
        spentPx.push_back(ball.atPx);
        fireballs.erase(fireballs.begin() + static_cast<std::ptrdiff_t>(i));
        return;
    }
}

std::vector<entt::entity> State::Bodies() const {
    std::vector<entt::entity> bodies;
    bodies.reserve(turrets.size());
    for (const Turret& turret : turrets) {
        if (turret.body != entt::null) bodies.push_back(turret.body);
    }
    return bodies;
}

bool Find(const Tscn::Scene& scene, const Roles::Table& roles, const LevelBuilder::Built& built, const Rules& rules,
          State& out, std::string& error) {
    out = State{};
    out.rules = rules;

    for (const Tscn::Node& node : scene.nodes) {
        if (node.parent != ".") continue;
        const std::string role = Roles::RoleOf(roles, node);
        const Tscn::Value* position = node.Find("position");
        const bool placed = position != nullptr && position->kind == Tscn::Value::Kind::Vector2;
        const glm::dvec2 atPx = placed ? glm::dvec2(position->numbers[0], position->numbers[1]) : glm::dvec2(0.0);

        if (role == Roles::kLevelBounds && placed) out.boundsPx = atPx;
        if (role != Roles::kTurret) continue;

        Turret turret;
        turret.name = node.name;
        turret.atPx = atPx;
        if (const auto found = built.entities.find(node.name); found != built.entities.end()) {
            turret.body = found->second;
        }

        // The Godot project's defaults for a node that names neither (hazards.gd:340-344).
        glm::dvec2 direction(Meta(node, "dirX", -1.0), Meta(node, "dirY", 0.0));
        if (direction.x == 0.0 && direction.y == 0.0) direction = glm::dvec2(-1.0, 0.0);
        // As written, NOT normalised: ETHCallback_fireball moves a fireball by 'dir' x
        // scaledUnitsPerSecond(speed), and addFireball stores the carranca's own vector2(dirX, dirY)
        // as that 'dir' (turrets.json `direction`; planning step 149). Its length is the fireball's
        // share of the speed: 1.2 on level0a's carranca_right_ent_792.
        turret.directionPx = direction;

        turret.strideMs = Meta(node, "stride", rules.defaultStrideMs);
        if (turret.strideMs <= 0.0) {
            error = node.name + " has a stride of " + std::to_string(turret.strideMs);
            return false;
        }
        turret.elapsedMs = Meta(node, "startStride", 0.0);

        out.turrets.push_back(turret);
    }
    return true;
}

} // namespace MagicPortals::Turrets
