#include "sim/Cutscene.hpp"

#include "sim/Ease.hpp"
#include "sim/LevelBuilder.hpp"
#include "sim/Roles.hpp"
#include "sim/Units.hpp"

#include "core/Components.hpp"
#include "core/Json.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <utility>

namespace MagicPortals::Cutscene {

namespace {

namespace Json = Supersonic::Json;
using Supersonic::RigidBodyComponent;
using Supersonic::TransformComponent;

// The first node of this exact entity name, in file order: SeekEntity's answer.
const Tscn::Node* Seek(const Tscn::Scene& scene, const std::string& entityName) {
    for (const Tscn::Node& node : scene.nodes) {
        if (node.parent == "." && Roles::EntityName(node) == entityName) return &node;
    }
    return nullptr;
}

bool PositionOf(const Tscn::Node& node, glm::dvec2& out) {
    const Tscn::Value* position = node.Find("position");
    if (position == nullptr || position->kind != Tscn::Value::Kind::Vector2) return false;
    out = glm::dvec2(position->numbers[0], position->numbers[1]);
    return true;
}

// interpolate(a, b, bias) = a + (b - a) x bias (bytes 314380..314472), with no clamp.
double Interpolate(double a, double b, double bias) { return a + (b - a) * bias; }

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

    const auto number = [&](const char* group, const char* key, double& into) -> bool {
        const Json::Value& block = root[group];
        if (!block.Has(key) || !block[key].IsNumber()) {
            error = path + ": " + std::string(group) + "." + key + " is missing or not a number";
            return false;
        }
        into = block[key].AsNumber();
        return true;
    };
    const auto whole = [&](const char* group, const char* key, int& into) -> bool {
        double read = 0.0;
        if (!number(group, key, read)) return false;
        if (read != std::floor(read) || read < 0.0) {
            error = path + ": " + std::string(group) + "." + key + " is not a whole number of at least 0";
            return false;
        }
        into = static_cast<int>(read);
        return true;
    };
    const auto string = [&](const char* group, const char* key, std::string& into) -> bool {
        const Json::Value& block = root[group];
        if (!block.Has(key) || !block[key].IsString() || block[key].AsString().empty()) {
            error = path + ": " + std::string(group) + "." + key + " is missing, not a string or empty";
            return false;
        }
        into = block[key].AsString();
        return true;
    };
    const auto numbers = [&](const char* group, const char* key, std::size_t count, double* into) -> bool {
        const Json::Value& block = root[group];
        bool ok = block.Has(key) && block[key].IsArray() && block[key].AsArray().size() == count;
        for (std::size_t i = 0; ok && i < count; ++i) {
            ok = block[key].AsArray()[i].IsNumber();
            if (ok) into[i] = block[key].AsArray()[i].AsNumber();
        }
        if (!ok) error = path + ": " + std::string(group) + "." + key + " is not " + std::to_string(count) + " numbers";
        return ok;
    };
    const auto pair = [&](const char* group, const char* key, glm::dvec2& into) -> bool {
        double read[2] = {0.0, 0.0};
        if (!numbers(group, key, 2, read)) return false;
        into = glm::dvec2(read[0], read[1]);
        return true;
    };
    const auto triple = [&](const char* group, const char* key, glm::dvec3& into) -> bool {
        double read[3] = {0.0, 0.0, 0.0};
        if (!numbers(group, key, 3, read)) return false;
        into = glm::dvec3(read[0], read[1], read[2]);
        return true;
    };

    Rules read;
    if (!string("end_delay", "entity", read.endDelayEntity) || !string("end_delay", "time_key", read.endDelayKey)) {
        return false;
    }

    Rules::Launch& launch = read.launch;
    if (!string("rocket_launch", "entity", launch.entity) || !string("rocket_launch", "door", launch.door) ||
        !string("rocket_launch", "bridge", launch.bridge) || !number("rocket_launch", "launch_ms", launch.launchMs) ||
        !pair("rocket_launch", "door_travel_px", launch.doorTravelPx) ||
        !pair("rocket_launch", "bridge_travel_px", launch.bridgeTravelPx) ||
        !number("rocket_launch", "bridge_span_ms", launch.bridgeSpanMs) ||
        !number("rocket_launch", "quake_rearm_above_ms", launch.quakeRearmAboveMs) ||
        !number("rocket_launch", "quake_before_ms", launch.quakeBeforeMs) ||
        !string("rocket_launch", "track", launch.track)) {
        return false;
    }

    Rules::Landing& landing = read.landing;
    if (!string("rocket_landing", "entity", landing.entity) || !string("rocket_landing", "door", landing.door) ||
        !string("rocket_landing", "eclipse", landing.eclipse) || !string("rocket_landing", "spiral", landing.spiral) ||
        !string("rocket_landing", "key", landing.key) || !number("rocket_landing", "launch_ms", landing.launchMs) ||
        !pair("rocket_landing", "door_travel_px", landing.doorTravelPx) ||
        !pair("rocket_landing", "eclipse_pitch", landing.eclipsePitch) ||
        !number("rocket_landing", "key_scale_per_frame", landing.keyScalePerFrame) ||
        !number("rocket_landing", "spiral_brightness_from", landing.spiralBrightnessFrom) ||
        !number("rocket_landing", "spiral_brightness_to", landing.spiralBrightnessTo) ||
        !string("rocket_landing", "track", landing.track)) {
        return false;
    }

    Rules::Knight& knight = read.knight;
    if (!string("dragon_knight", "spawn", knight.spawn) || !string("dragon_knight", "entity", knight.entity) ||
        !string("dragon_knight", "waypoint_prefix", knight.waypointPrefix) ||
        !string("dragon_knight", "hold_key", knight.holdKey) ||
        !number("dragon_knight", "speed_px_per_step", knight.speedPxPerStep) ||
        !number("dragon_knight", "arrive_px", knight.arrivePx) ||
        !pair("dragon_knight", "finish_offset_px", knight.finishOffsetPx) ||
        !whole("dragon_knight", "walk_first_column", knight.walkFirstColumn) ||
        !whole("dragon_knight", "walk_last_column", knight.walkLastColumn) ||
        !number("dragon_knight", "walk_stride_ms", knight.walkStrideMs) ||
        !whole("dragon_knight", "start_frame", knight.startFrame) ||
        !whole("dragon_knight", "columns", knight.columns) || !whole("dragon_knight", "left_row", knight.leftRow) ||
        !whole("dragon_knight", "right_row", knight.rightRow) || !pair("dragon_knight", "pivot_px", knight.pivotPx) ||
        !triple("dragon_knight", "colour", knight.colour) || !triple("dragon_knight", "emissive", knight.emissive)) {
        return false;
    }

    // A clock that never runs would hold a cutscene on its first frame, and a span
    // of 0 would divide by it: both are refused rather than drawn.
    if (!(launch.launchMs > 0.0) || !(launch.bridgeSpanMs > 0.0) || !(landing.launchMs > 0.0) ||
        !(knight.speedPxPerStep > 0.0) || !(knight.arrivePx > 0.0) || !(knight.walkStrideMs > 0.0) ||
        knight.columns < 1 || knight.walkLastColumn < knight.walkFirstColumn || !(landing.keyScalePerFrame > 0.0)) {
        error = path + ": a span, a speed, a radius, a stride, a column count or a key scale is not above 0";
        return false;
    }
    out = std::move(read);
    return true;
}

std::optional<double> EndDelayMs(const Rules& rules, const Tscn::Scene& scene) {
    const Tscn::Node* delay = Seek(scene, rules.endDelayEntity);
    if (delay == nullptr) return std::nullopt;
    // GetUInt('time'): the converter quotes the custom data, which AsNumber reads.
    const Tscn::Value* time = delay->Meta(rules.endDelayKey);
    double ms = 0.0;
    if (time == nullptr || !time->AsNumber(ms) || ms < 0.0) return std::nullopt;
    return ms;
}

bool Places(const Tscn::Scene& scene, const std::string& entityName) { return Seek(scene, entityName) != nullptr; }

// ---- 4-01 -----------------------------------------------------------------------

LaunchPose LaunchAt(const Rules& rules, double ms) {
    const Rules::Launch& launch = rules.launch;
    LaunchPose pose;
    pose.doorOffsetPx = launch.doorTravelPx * Ease::SmoothBeginning(ms / launch.launchMs);
    pose.bridgeOffsetPx = launch.bridgeTravelPx * Ease::SmoothBeginning(ms / launch.bridgeSpanMs);
    return pose;
}

LaunchStep StepLaunch(const Rules& rules, Launch& launch, double frameMs) {
    const Rules::Launch& rule = rules.launch;
    LaunchStep step;
    if (!launch.running) {
        // #46-174: both counters from 0, the starts and destinies kept, the track.
        launch.running = true;
        launch.elapsedMs = 0.0;
        launch.quakeCounterMs = 0.0;
        launch.pose = LaunchPose{};
        step.started = true;
    }
    // #176-206, on the first frame as on every other.
    launch.elapsedMs += frameMs;
    launch.quakeCounterMs += frameMs;
    // #220-256: re-armed past 100 while the launch is younger than 6000.
    if (launch.quakeCounterMs > rule.quakeRearmAboveMs && launch.elapsedMs < rule.quakeBeforeMs) {
        launch.quakeCounterMs = 0.0;
        step.quake = true;
    }
    // #258-357: moved only while below 7600, so each keeps the last pose written.
    if (launch.elapsedMs < rule.launchMs) launch.pose = LaunchAt(rules, launch.elapsedMs);
    return step;
}

// ---- 4-19 -----------------------------------------------------------------------

double LandingBias(const Rules& rules, double ms) { return Ease::SmoothBeginning(ms / rules.landing.launchMs); }

glm::dvec2 LandingDoorOffsetPx(const Rules& rules, double bias) { return rules.landing.doorTravelPx * bias; }

double LandingEclipseYPx(const Rules& rules, double bias, double cornerYPx, double viewHeightPx,
                         double eclipseHeightPx) {
    const double destiny = cornerYPx + viewHeightPx * rules.landing.eclipsePitch.y;
    return Interpolate(destiny + eclipseHeightPx, destiny, Ease::SmoothEnd(bias));
}

double SpiralBrightness(const Rules& rules, double bias) {
    const Rules::Landing& landing = rules.landing;
    const double eased = Interpolate(landing.spiralBrightnessFrom, landing.spiralBrightnessTo, Ease::SmoothEnd(bias));
    return std::max(0.0, std::min(eased, 1.0));
}

LandingStep StepLanding(const Rules& rules, Landing& landing, double frameMs) {
    LandingStep step;
    if (!landing.running) {
        // #72-183: endElapsedTime 0, the starts kept, the spiral's colour, the track.
        landing.running = true;
        landing.elapsedMs = 0.0;
        landing.keyScale = 1.0;
        landing.keyScalings = 0;
        step.started = true;
    }
    // #185-227: the key, every frame while there is one. The layer applies it, to
    // the picture and, as ETHEntity::Scale does (ETHEntity.cpp:591-599), its systems.
    landing.keyScale *= rules.landing.keyScalePerFrame;
    ++landing.keyScalings;
    // #229-256: the clock stops at the first value that reaches the launch time.
    if (landing.elapsedMs < rules.landing.launchMs) landing.elapsedMs += frameMs;
    landing.bias = LandingBias(rules, landing.elapsedMs);
    return step;
}

// ---- 2-32 -----------------------------------------------------------------------

bool FindKnight(const Rules& rules, const Tscn::Scene& scene, Knight& out, std::string& error) {
    Knight found;
    const Tscn::Node* spawn = Seek(scene, rules.knight.spawn);
    if (spawn == nullptr || !PositionOf(*spawn, found.spawnPx)) {
        out = std::move(found);
        return true;
    }
    found.placed = true;
    // waypoint0, waypoint1, ... up to the first SeekEntity does not find: the
    // callback's own test for the end of its walk (#258-291).
    for (std::size_t n = 0;; ++n) {
        const std::string name = rules.knight.waypointPrefix + std::to_string(n);
        const Tscn::Node* node = Seek(scene, name);
        if (node == nullptr) break;
        Waypoint point;
        point.name = name;
        if (!PositionOf(*node, point.atPx)) break;
        double hold = 0.0;
        if (const Tscn::Value* value = node->Meta(rules.knight.holdKey); value != nullptr && value->AsNumber(hold)) {
            point.holdMs = hold;
        }
        found.waypoints.push_back(point);
    }
    if (found.waypoints.empty()) {
        error = rules.knight.spawn + " is placed with no " + rules.knight.waypointPrefix + "0 to walk to";
        return false;
    }
    found.column = rules.knight.startFrame % rules.knight.columns;
    found.row = rules.knight.startFrame / rules.knight.columns;
    out = std::move(found);
    return true;
}

entt::entity SpawnKnight(const Rules& rules, const Minions::Rules& body, entt::registry& registry, Knight& knight) {
    if (!knight.placed || knight.spawned) return knight.body;
    knight.spawned = true;
    // Its ENTITY point on the spawn: the body's centre that far below it.
    const glm::dvec2 centrePx = knight.spawnPx + glm::dvec2(0.0, body.EntityAboveCentrePx());
    knight.body =
        LevelBuilder::BuildRigidCircle(registry, rules.knight.entity, centrePx, body.bodyRadiusPx, LevelBuilder::Options{});
    // dragon_knight.ent is fixedRotation="1": a circle left to turn would roll.
    if (knight.body != entt::null) registry.get<RigidBodyComponent>(knight.body).lockRotation = glm::bvec3(true);
    return knight.body;
}

glm::dvec2 KnightEntityPx(const Minions::Rules& body, entt::registry& registry, const Knight& knight) {
    if (knight.body == entt::null || !registry.valid(knight.body)) return glm::dvec2(0.0);
    const auto* transform = registry.try_get<TransformComponent>(knight.body);
    if (transform == nullptr) return glm::dvec2(0.0);
    return Units::ToPixels(transform->position) - glm::dvec2(0.0, body.EntityAboveCentrePx());
}

void StepKnight(const Rules& rules, const Minions::Rules& body, entt::registry& registry, Knight& knight,
                double frameMs) {
    const Rules::Knight& rule = rules.knight;
    knight.walked = false;
    knight.entered = false;
    if (!knight.spawned || knight.gone || knight.body == entt::null || !registry.valid(knight.body) ||
        !registry.all_of<RigidBodyComponent, TransformComponent>(knight.body) || knight.waypoints.empty()) {
        return;
    }
    if (!knight.initialised) {
        // #14-142: hasStarted 0, elapsedTime 0, dest 0, a fresh FrameTimer.
        knight.initialised = true;
        knight.hasStarted = false;
        knight.elapsedMs = 0.0;
        knight.dest = 0;
        knight.timer = Art::FrameTimer{};
    }
    // #165-179.
    knight.elapsedMs += frameMs;
    const glm::dvec2 atPx = KnightEntityPx(body, registry, knight);
    glm::dvec2 destPx = knight.waypoints[knight.dest].atPx;
    // #220-231: within scale(16) of where it is going.
    if (glm::length(destPx - atPx) < rule.arrivePx) {
        knight.currentDest = knight.dest;
        ++knight.dest;
        if (knight.dest >= knight.waypoints.size()) {
            // #293-339: no next waypoint - it deletes itself and goes in as the
            // player does, the door's effect at its point.
            knight.dest = 0;
            knight.gone = true;
            knight.entered = true;
            knight.enteredAtPx = atPx;
            registry.destroy(knight.body);
            knight.body = entt::null;
            return;
        }
        // #341-437.
        knight.hasStarted = true;
        knight.elapsedMs = 0.0;
        knight.holdMs = knight.waypoints[knight.currentDest].holdMs;
        destPx = knight.waypoints[knight.dest].atPx;
    }
    // #451-480: driven only once it has started and outlasted its hold; else nothing
    // is set at all, and gravity and friction have it.
    if (!knight.hasStarted || !(knight.elapsedMs > knight.holdMs)) return;
    auto& rigid = registry.get<RigidBodyComponent>(knight.body);
    // #555-570, read before #534-553 sets the new one: the row of the LAST step.
    const bool wentLeft = rigid.velocity.x < 0.0f;
    // #484-496: -1 when the two x's are equal.
    const double direction = atPx.x < destPx.x ? 1.0 : -1.0;
    // #513-532: 1.7 px a physics step. The 66 ms cap never bites at 60 Hz.
    const double pxPerSecond = rule.speedPxPerStep * 1000.0 / body.stepMs;
    rigid.velocity.x = Units::ToMetres(direction * pxPerSecond);
    knight.row = wentLeft ? rule.leftRow : rule.rightRow;
    knight.column = knight.timer.Set(rule.walkFirstColumn, rule.walkLastColumn, rule.walkStrideMs, true, frameMs);
    knight.walked = true;
}

} // namespace MagicPortals::Cutscene
