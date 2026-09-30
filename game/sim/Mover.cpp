#include "sim/Mover.hpp"

#include "core/Components.hpp"
#include "core/DetMath.hpp"
#include "core/Json.hpp"
#include "sim/Units.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace MagicPortals::Mover {

void MoveKinematic(entt::registry& registry, entt::entity entity, const glm::vec3& target, float dt) {
    auto& transform = registry.get<Supersonic::TransformComponent>(entity);
    auto& rigid = registry.get<Supersonic::RigidBodyComponent>(entity);
    rigid.velocity = dt > 0.0f ? (target - transform.position) / dt : glm::vec3(0.0f);
    transform.position = target;
}

void Door::Tick(entt::registry& registry, entt::entity entity, float dt) {
    const float step = dt / durationS;
    progress = std::clamp(progress + (opening ? step : -step), 0.0f, 1.0f);
    MoveKinematic(registry, entity, glm::mix(closed, open, progress), dt);
}

bool DoorFromNode(const Tscn::Node& node, Door& out, std::string& error) {
    const Tscn::Value* stride = node.Meta("stride");
    double milliseconds = 0.0;
    if (stride == nullptr || !stride->AsNumber(milliseconds)) {
        error = node.name + " has no stride";
        return false;
    }
    const Tscn::Value* position = node.Find("position");
    glm::dvec2 at(0.0);
    if (position != nullptr && position->kind == Tscn::Value::Kind::Vector2) {
        at = glm::dvec2(position->numbers[0], position->numbers[1]);
    }

    out = Door{};
    out.closed = Units::ToWorld(at.x, at.y);
    // Up the screen, which is -y in the Godot project's pixels (behaviours.gd:145).
    out.open = Units::ToWorld(at.x, at.y - kDoorRisePx);
    // Floored at 10 ms, as behaviours.gd floors it (:147).
    out.durationS = static_cast<float>(std::max(milliseconds / 1000.0, 0.01));
    return true;
}

bool LoadRules(const std::string& path, Rules& out, std::string& error) {
    namespace Json = Supersonic::Json;
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = path + ": cannot open";
        return false;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string text = buffer.str();
    Json::Parser parser(text);
    Json::Value root;
    if (!parser.Parse(root)) {
        error = path + ": " + parser.Error();
        return false;
    }
    const auto number = [&](const char* block, const char* key, double& to) {
        if (!root.IsObject() || !root.Has(block) || !root[block].IsObject() || !root[block].Has(key) ||
            !root[block][key].IsNumber()) {
            error = path + ": " + block + "." + key + " is missing or not a number";
            return false;
        }
        to = root[block][key].AsNumber();
        return true;
    };
    // A list of `count` numbers under `block`, or under `block`.`inner` when one is named.
    const auto numbers = [&](const char* block, const char* inner, const char* key, std::size_t count,
                             double* to) {
        const std::string where = std::string(block) + "." + (inner != nullptr ? std::string(inner) + "." : "") + key;
        const Json::Value* owner = root.IsObject() && root.Has(block) && root[block].IsObject() ? &root[block] : nullptr;
        if (owner != nullptr && inner != nullptr) {
            owner = owner->Has(inner) && (*owner)[inner].IsObject() ? &(*owner)[inner] : nullptr;
        }
        if (owner == nullptr || !owner->Has(key) || !(*owner)[key].IsArray() ||
            (*owner)[key].AsArray().size() != count) {
            error = path + ": " + where + " is not a list of " + std::to_string(count) + " numbers";
            return false;
        }
        for (std::size_t i = 0; i < count; ++i) {
            const Json::Value& value = (*owner)[key].AsArray()[i];
            if (!value.IsNumber()) {
                error = path + ": " + where + " is not a list of " + std::to_string(count) + " numbers";
                return false;
            }
            to[i] = value.AsNumber();
        }
        return true;
    };
    Rules read;
    double sizeRead[2] = {0.0, 0.0};
    double fromRead[3] = {0.0, 0.0, 0.0};
    double toRead[3] = {0.0, 0.0, 0.0};
    if (!number("oscillation", "rate_scale", read.oscillationRateScale) ||
        !number("lift", "speed_units_per_s", read.liftSpeedUnitsPerS) ||
        !numbers("lift", nullptr, "size_units", 2, sizeRead) || !numbers("lift", "blink", "from", 3, fromRead) ||
        !numbers("lift", "blink", "to", 3, toRead)) {
        return false;
    }
    const Json::Value& blink = root["lift"]["blink"];
    if (!blink.Has("stride_ms") || !blink["stride_ms"].IsNumber()) {
        error = path + ": lift.blink.stride_ms is missing or not a number";
        return false;
    }
    read.liftBlinkStrideMs = blink["stride_ms"].AsNumber();
    if (!(read.liftSpeedUnitsPerS > 0.0) || !(sizeRead[0] > 0.0) || !(sizeRead[1] > 0.0) ||
        !(read.liftBlinkStrideMs > 0.0)) {
        error = path + ": lift's speed, size and blink stride must be above 0";
        return false;
    }
    read.liftSizeUnits = glm::dvec2(sizeRead[0], sizeRead[1]);
    read.liftBlinkFrom = glm::dvec3(fromRead[0], fromRead[1], fromRead[2]);
    read.liftBlinkTo = glm::dvec3(toRead[0], toRead[1], toRead[2]);
    out = read;
    return true;
}

glm::dvec2 Oscillation::AtPx() const {
    // DetMath rather than the platform's sin, so that a swing comes out the same
    // on every C runtime, as the transit's turn does (Portal.cpp).
    const double wave = static_cast<double>(Supersonic::DetMath::sin(static_cast<float>(rateScale * speed * t)));
    return originPx + axis * (wave * stridePx * 0.5);
}

bool OscillationFromNode(const Tscn::Node& node, const glm::dvec2& axis, double rateScale, Oscillation& out,
                         std::string& error) {
    Oscillation read;
    const Tscn::Value* speed = node.Meta("speed");
    const Tscn::Value* stride = node.Meta("stride");
    if (speed == nullptr || !speed->AsNumber(read.speed) || stride == nullptr || !stride->AsNumber(read.stridePx)) {
        error = node.name + " has no speed and stride to swing by";
        return false;
    }
    const Tscn::Value* position = node.Find("position");
    if (position == nullptr || position->kind != Tscn::Value::Kind::Vector2) {
        error = node.name + " has no position";
        return false;
    }
    read.originPx = glm::dvec2(position->numbers[0], position->numbers[1]);
    read.axis = axis;
    read.rateScale = rateScale;
    out = read;
    return true;
}

void Travel::Send() {
    // sendLiftToOtherPoint #1-175: its point against 'a', then 'b', each an exact
    // vector2 equality; anywhere else it is sent back to a, measured from b.
    if (atPx == aPx) {
        destPx = bPx;
        originPx = aPx;
    } else if (atPx == bPx) {
        destPx = aPx;
        originPx = bPx;
    } else {
        originPx = bPx;
        destPx = aPx;
    }
    // #176-211 'dir' = normalize(dest - its point). A lift whose markers stand on one
    // point has nowhere to go; no placement has one, and it is held still here.
    const glm::dvec2 way = destPx - atPx;
    const double length = glm::length(way);
    dir = length > 0.0 ? way / length : glm::dvec2(0.0);
    ++sends;
}

void Travel::Advance(double dt) {
    // #263-321: only while 'dest' is not its point.
    if (!Moving()) return;
    atPx += dir * (speedPx * dt);
    // #322-367: squaredDistance(origin, its point) >= 'squaredDistance' puts it on
    // 'dest' - the frame it reaches or passes the far end, it is there exactly.
    const glm::dvec2 gone = atPx - originPx;
    if (glm::dot(gone, gone) >= legSquared) atPx = destPx;
}

bool Travel::Holds(const glm::dvec2& pointPx, const glm::dvec2& sizeUnits) const {
    // isPointInRect (utilIntersections 319410..319788): outside only when the point
    // lies below the rect's low edge or above its high one on either axis.
    const glm::dvec2 low = atPx - sizeUnits * 0.5;
    const glm::dvec2 high = low + sizeUnits;
    return pointPx.x >= low.x && pointPx.x <= high.x && pointPx.y >= low.y && pointPx.y <= high.y;
}

void Movers::Tick(entt::registry& registry, float dt) {
    for (Platform& platform : platforms) {
        platform.motion.t += dt;
        const glm::dvec2 at = platform.motion.AtPx();
        MoveKinematic(registry, platform.entity, Units::ToWorld(at.x, at.y), dt);
    }
    for (Lift& lift : lifts) {
        lift.motion.Advance(dt);
        MoveKinematic(registry, lift.entity, Units::ToWorld(lift.motion.atPx.x, lift.motion.atPx.y), dt);
    }
}

int Movers::Press(const glm::dvec2& pointPx) {
    int sent = 0;
    for (Lift& lift : lifts) {
        if (!lift.motion.Holds(pointPx, lift.sizeUnits)) continue;
        lift.motion.Send();
        ++sent;
    }
    return sent;
}

const Platform* Movers::FindPlatform(const std::string& name) const {
    for (const Platform& platform : platforms) {
        if (platform.name == name) return &platform;
    }
    return nullptr;
}

const Lift* Movers::FindLift(const std::string& name) const {
    for (const Lift& lift : lifts) {
        if (lift.name == name) return &lift;
    }
    return nullptr;
}

bool Wire(const Tscn::Scene& scene, const Roles::Table& roles, const LevelBuilder::Built& built, const Rules& rules,
          Movers& out, std::string& error) {
    out = Movers{};
    const auto positionOf = [](const Tscn::Node& node, glm::dvec2& to) {
        const Tscn::Value* position = node.Find("position");
        if (position == nullptr || position->kind != Tscn::Value::Kind::Vector2) return false;
        to = glm::dvec2(position->numbers[0], position->numbers[1]);
        return true;
    };
    const auto entityOf = [&](const Tscn::Node& node, entt::entity& to) {
        const auto found = built.entities.find(node.name);
        if (found == built.entities.end()) {
            error = node.name + " was not built";
            return false;
        }
        to = found->second;
        return true;
    };
    // level_runtime.gd:157-163: a marker, by the entity name a lift gives it.
    const auto markerAt = [&](const std::string& name, const glm::dvec2& fallback) {
        if (name.empty()) return fallback;
        for (const Tscn::Node& node : scene.nodes) {
            glm::dvec2 at(0.0);
            if (node.parent == "." && Roles::RoleOf(roles, node) == "lift_marker" && Roles::EntityName(node) == name &&
                positionOf(node, at)) {
                return at;
            }
        }
        return fallback;
    };

    for (const Tscn::Node& node : scene.nodes) {
        if (node.parent != ".") continue;
        const std::string role = Roles::RoleOf(roles, node);
        if (role == Roles::kMovingPlatform) {
            Platform platform;
            platform.name = node.name;
            // Vertical, always (behaviours.gd:105-108).
            if (!OscillationFromNode(node, glm::dvec2(0.0, 1.0), rules.oscillationRateScale, platform.motion, error) ||
                !entityOf(node, platform.entity)) {
                return false;
            }
            out.platforms.push_back(platform);
        } else if (role == Roles::kLift) {
            Lift lift;
            lift.name = node.name;
            glm::dvec2 at(0.0);
            if (!positionOf(node, at)) {
                error = node.name + " has no position";
                return false;
            }
            const auto nameOf = [&node](const char* key) {
                const Tscn::Value* value = node.Meta(key);
                return value != nullptr && value->kind == Tscn::Value::Kind::String ? value->text : std::string();
            };
            lift.motion.aPx = markerAt(nameOf("a"), at);
            lift.motion.bPx = markerAt(nameOf("b"), at);
            // ETHCallback_lift's first frame (#83-143): put on a, at rest there, and the
            // leg's length kept squared. A node's own `speed` is read by nothing.
            lift.motion.atPx = lift.motion.aPx;
            lift.motion.destPx = lift.motion.aPx;
            lift.motion.originPx = lift.motion.aPx;
            const glm::dvec2 leg = lift.motion.bPx - lift.motion.aPx;
            lift.motion.legSquared = glm::dot(leg, leg);
            lift.motion.speedPx = rules.liftSpeedUnitsPerS;
            lift.sizeUnits = rules.liftSizeUnits;
            if (!entityOf(node, lift.entity)) return false;
            out.lifts.push_back(lift);
        }
    }
    return true;
}

} // namespace MagicPortals::Mover
