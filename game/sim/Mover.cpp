#include "sim/Mover.hpp"

#include "core/Components.hpp"
#include "core/DetMath.hpp"
#include "core/Json.hpp"
#include "sim/Units.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

namespace MagicPortals::Mover {

void MoveKinematic(entt::registry& registry, entt::entity entity, const glm::vec3& target, float physicsDt) {
    auto& transform = registry.get<Supersonic::TransformComponent>(entity);
    auto& rigid = registry.get<Supersonic::RigidBodyComponent>(entity);
    // Over the PHYSICS step - the solver moves the body by velocity x its own step, so the
    // velocity that lands it on `target` is measured against that, whatever the script's
    // frame is (planning step 147).
    rigid.velocity = physicsDt > 0.0f ? (target - transform.position) / physicsDt : glm::vec3(0.0f);
    transform.position = target;
}

void PlaceKinematic(entt::registry& registry, entt::entity entity, const glm::vec3& target) {
    registry.get<Supersonic::TransformComponent>(entity).position = target;
    registry.get<Supersonic::RigidBodyComponent>(entity).velocity = glm::vec3(0.0f);
}

glm::dvec2 Door::InterpolatedPx() const {
    // getCurrentPos (20104 #3-18): m_b when m_elapsedTime > m_time; getBias clamps
    // the fraction before the filter (18855 #9-31). As the button reads it
    // (Puzzle::Button::DepthPx), on the port's millisecond clock.
    if (strideMs <= 0.0 || elapsedMs > strideMs) return toPx;
    const double bias = std::sin(easeRad * std::clamp(elapsedMs / strideMs, 0.0, 1.0));
    return fromPx + (toPx - fromPx) * bias;
}

void Door::Tick(entt::registry& registry, entt::entity entity, bool active, const ScriptClock::Step& step) {
    // Every status read is of currentPos, where the last frame's SetPositionXY left
    // it (#1-6), so each arrival is seen a frame after it happens.
    if (atPx == closedPx) {
        // #282-328: back shut. The shake is the layer's, off `landings`.
        if (status != DoorStatus::Shut) ++landings;
        status = DoorStatus::Shut;
    }
    if (active && status == DoorStatus::Shut) {
        // #330-400: reset(originalPos, openPos, stride), status 1.
        fromPx = closedPx;
        toPx = openPx;
        elapsedMs = 0.0;
        status = DoorStatus::Opening;
        opening = true;
    }
    // #442-471. Not an else: a door that arrived last frame is open on this one,
    // and #473 can start its fall at once.
    if (atPx == openPx) status = DoorStatus::Open;
    if (!active && status == DoorStatus::Open) {
        // #473-541: reset(openPos, originalPos, stride), status -1.
        fromPx = openPx;
        toPx = closedPx;
        elapsedMs = 0.0;
        status = DoorStatus::Closing;
        opening = false;
    }
    // #587-624: update() - InterpolationTimer adds the frame's milliseconds - and
    // SetPositionXY(getCurrentPos()).
    elapsedMs += ScriptClock::Ms(step);
    atPx = InterpolatedPx();
    const double rise = closedPx.y - openPx.y;
    progress = rise != 0.0 ? static_cast<float>((closedPx.y - atPx.y) / rise) : 0.0f;
    MoveKinematic(registry, entity, Units::ToWorld(atPx.x, atPx.y), step.physicsS);
}

bool DoorFromNode(const Tscn::Node& node, const DoorRules& rules, Door& out, std::string& error) {
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
    out.closedPx = at;
    // Up the screen, which is -y in the level's pixels. The script turns the rise
    // by the door's angle; no placement is turned (movers.json `door`).
    out.openPx = glm::dvec2(at.x, at.y - rules.RisePx());
    out.closed = Units::ToWorld(out.closedPx.x, out.closedPx.y);
    out.open = Units::ToWorld(out.openPx.x, out.openPx.y);
    out.strideMs = milliseconds;
    out.easeRad = rules.easeRad;
    out.fromPx = at;
    out.toPx = at;
    out.atPx = at;
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
    // linearMotion's two calls: three numbers each, and the direction that turns the agent's axis. Read
    // strictly, as the lift's are: a missing one is an error, not a default (a default is a guess in code).
    const auto call = [&](const char* name, const char* axisKey, LinearCall& into) {
        const std::string where = std::string("linear_motion.") + name;
        const Json::Value* block = root.IsObject() && root.Has("linear_motion") && root["linear_motion"].IsObject() &&
                                           root["linear_motion"].Has(name) && root["linear_motion"][name].IsObject()
                                       ? &root["linear_motion"][name]
                                       : nullptr;
        if (block == nullptr || !block->Has("vertical") || !(*block)["vertical"].IsBool() || !block->Has(axisKey) ||
            !(*block)[axisKey].IsNumber() || !block->Has("start_angle") || !(*block)["start_angle"].IsNumber()) {
            error = path + ": " + where + " needs vertical (true or false), " + axisKey + " and start_angle (numbers)";
            return false;
        }
        into.vertical = (*block)["vertical"].AsBool();
        into.axisDeg = (*block)[axisKey].AsNumber();
        into.startAngle = (*block)["start_angle"].AsNumber();
        return true;
    };
    if (!call("platform", "axis_deg", read.platform) || !call("agent", "axis_deg", read.agent) ||
        !call("agent", "axis_deg_sideways", read.agentSideways)) {
        return false;
    }
    {
        const Json::Value& agent = root["linear_motion"]["agent"];
        if (!agent.Has("direction_sideways") || !agent["direction_sideways"].IsString() ||
            agent["direction_sideways"].AsString().empty()) {
            error = path + ": linear_motion.agent.direction_sideways is missing or not a name";
            return false;
        }
        read.directionSideways = agent["direction_sideways"].AsString();
    }
    if (!number("lift", "speed_units_per_s", read.liftSpeedUnitsPerS) ||
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

void Oscillation::Advance(const Motion::Rules& rules, double frameMs) {
    // linearMotion ins 1-98: the first call writes the start angle, every call steps it and takes off one
    // turn once past it. The original's m_factor 0 (a pause) is a frame of 0.
    Motion::Advance(rules, linear, frameMs);
}

glm::dvec2 Oscillation::AtPx() const {
    // Until the first call the entity is where the level put it.
    if (!linear.started) return originPx;
    // DetMath rather than the platform's cos, so that a swing comes out the same on every C runtime, as
    // the transit's turn does (Portal.cpp); the rest of linearMotion's arithmetic is Motion's one copy.
    const double cosine = static_cast<double>(Supersonic::DetMath::cos(static_cast<float>(linear.angle)));
    return originPx + Motion::OffsetPxWithCosine(linear, cosine);
}

bool OscillationFromNode(const Tscn::Node& node, const LinearCall& call, Oscillation& out, std::string& error) {
    Oscillation read;
    // linearMotion ins 41-51 and 100-110: GetFloat('speed') and GetFloat('stride'), the node's own data.
    const Tscn::Value* speed = node.Meta("speed");
    const Tscn::Value* stride = node.Meta("stride");
    if (speed == nullptr || !speed->AsNumber(read.linear.speed) || stride == nullptr ||
        !stride->AsNumber(read.linear.stride)) {
        error = node.name + " has no speed and stride to swing by";
        return false;
    }
    const Tscn::Value* position = node.Find("position");
    if (position == nullptr || position->kind != Tscn::Value::Kind::Vector2) {
        error = node.name + " has no position";
        return false;
    }
    read.originPx = glm::dvec2(position->numbers[0], position->numbers[1]);
    read.linear.vertical = call.vertical;
    read.linear.axisDeg = call.axisDeg;
    read.linear.startAngle = call.startAngle;
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

void Movers::Tick(entt::registry& registry, const ScriptClock::Step& step) {
    for (Platform& platform : platforms) {
        // ETHCallback_moving_platform calls linearMotion once a frame, with the script's whole milliseconds.
        const bool first = !platform.motion.Started();
        platform.motion.Advance(motion, ScriptClock::Ms(step));
        const glm::dvec2 at = platform.motion.AtPx();
        const glm::vec3 target = Units::ToWorld(at.x, at.y);
        // The first call jumps it a stride from where the level placed it: a SetTransform of a static
        // body, which has no velocity (ETHPhysicsEntityController.cpp:98-103). Written as the velocity
        // that gets it there it would throw whatever touches it a stride in one step.
        if (first) {
            PlaceKinematic(registry, platform.entity, target);
        } else {
            MoveKinematic(registry, platform.entity, target, step.physicsS);
        }
    }
    for (Lift& lift : lifts) {
        lift.motion.Advance(step.scriptS);
        MoveKinematic(registry, lift.entity, Units::ToWorld(lift.motion.atPx.x, lift.motion.atPx.y), step.physicsS);
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
    out.motion = rules.motion;
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
            // ETHCallback_moving_platform's call to linearMotion: vertical, axis 0, start angle 0.
            if (!OscillationFromNode(node, rules.platform, platform.motion, error) ||
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
