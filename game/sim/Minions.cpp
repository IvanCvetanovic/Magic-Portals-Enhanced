#include "sim/Minions.hpp"

#include "sim/Units.hpp"

#include "core/Components.hpp"
#include "core/Json.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>

namespace MagicPortals::Minions {

namespace {

namespace Json = Supersonic::Json;
using Supersonic::RigidBodyComponent;
using Supersonic::TransformComponent;

glm::dvec2 PxOf(entt::registry& registry, entt::entity body) {
    return Units::ToPixels(registry.get<TransformComponent>(body).position);
}

// The original's entity point of a minion's body, the one its GetPosition() reads:
// the centre less EntityAboveCentrePx, as the patrol measures it. A body with no
// transform has no point, and is recorded at its centre's zero rather than read.
glm::dvec2 EntityPointOf(entt::registry& registry, entt::entity body, const Rules& rules) {
    const auto* transform = registry.try_get<TransformComponent>(body);
    if (transform == nullptr) return glm::dvec2(0.0, 0.0);
    return Units::ToPixels(transform->position) - glm::dvec2(0.0, rules.EntityAboveCentrePx());
}

// A node's metadata as a number. The converter QUOTES the original's custom data
// - metadata/holdTime = "4000" - and Value::AsNumber reads that as readily as a
// bare one, so a hold needs no unquoting of its own.
double MetaNumber(const Tscn::Node& node, const char* key, double fallback) {
    const Tscn::Value* value = node.Meta(key);
    double read = 0.0;
    if (value == nullptr || !value->AsNumber(read)) return fallback;
    return read;
}

std::string MetaText(const Tscn::Node& node, const char* key) {
    const Tscn::Value* value = node.Meta(key);
    if (value == nullptr || value->kind != Tscn::Value::Kind::String) return std::string();
    return value->text;
}

bool PositionOf(const Tscn::Node& node, glm::dvec2& out) {
    const Tscn::Value* position = node.Find("position");
    if (position == nullptr || position->kind != Tscn::Value::Kind::Vector2) return false;
    out = glm::dvec2(position->numbers[0], position->numbers[1]);
    return true;
}

double Length(const glm::dvec2& v) {
    return std::sqrt(v.x * v.x + v.y * v.y);
}

// One placed waypoint node, before any prefix has claimed it.
struct Placed {
    std::string entityName;
    glm::dvec2 atPx{0.0, 0.0};
    double holdMs = 0.0;
};

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

    Rules read;
    if (!number("patrol", "speed_px_per_step", read.speedPxPerStep)) return false;
    if (!number("patrol", "step_ms", read.stepMs)) return false;
    if (!number("patrol", "arrive_px", read.arrivePx)) return false;
    if (!number("patrol", "settle_px", read.settlePx)) return false;
    if (!number("patrol", "brake", read.brake)) return false;
    if (!number("body", "radius_px", read.bodyRadiusPx)) return false;
    if (!number("body", "entity_above_feet_px", read.entityAboveFeetPx)) return false;

    const Json::Value& killer = root["killer"];
    if (!killer.Has("entity_name") || !killer["entity_name"].IsString()) {
        error = path + ": killer.entity_name is missing or not a string";
        return false;
    }
    read.killerName = killer["entity_name"].AsString();

    // What a marker adds (R16). Refused when missing rather than taken as
    // minion.ent: a marker naming an entity is the whole of the rule.
    const Json::Value& spawn = root["spawn"];
    if (!spawn.Has("default_entity") || !spawn["default_entity"].IsString()) {
        error = path + ": spawn.default_entity is missing or not a string";
        return false;
    }
    read.defaultEntity = spawn["default_entity"].AsString();
    if (!spawn.Has("entities") || !spawn["entities"].IsArray()) {
        error = path + ": spawn.entities is missing or not a list";
        return false;
    }
    if (!number("spawn", "angle_deg", read.spawnAngleDeg)) return false;
    if (!number("spawn", "scale", read.spawnScale)) return false;
    for (const Json::Value& named : spawn["entities"].AsArray()) {
        if (!named.IsString() || named.AsString().empty()) {
            error = path + ": spawn.entities names an entity in each entry";
            return false;
        }
        read.entities.push_back(named.AsString());
    }

    // Each of these would look like a level that works. A speed of nothing is a
    // minion that stands where it spawned; a step of nothing divides by zero
    // turning that speed into one per second; an arrival of nothing is never
    // reached, so the walk goes past its waypoint and never turns round. And a
    // settle inside the arrival inverts the original's two tests, which is the
    // one mistake this module exists to avoid making.
    if (read.speedPxPerStep <= 0.0) {
        error = path + ": patrol.speed_px_per_step is above zero";
        return false;
    }
    if (read.stepMs <= 0.0) {
        error = path + ": patrol.step_ms is above zero";
        return false;
    }
    if (read.arrivePx <= 0.0) {
        error = path + ": patrol.arrive_px is above zero";
        return false;
    }
    if (read.settlePx < read.arrivePx) {
        error = path + ": patrol.settle_px is at least patrol.arrive_px";
        return false;
    }
    // One brakes nothing and stops a minion dead; the other never stops it at all.
    if (read.brake <= 0.0 || read.brake >= 1.0) {
        error = path + ": patrol.brake is between zero and one";
        return false;
    }
    if (read.bodyRadiusPx <= 0.0) {
        error = path + ": body.radius_px is above zero";
        return false;
    }
    // An entity inside its own feet circle would put the point the walk measures
    // from below the body's centre, which no minion.ent does.
    if (read.entityAboveFeetPx < read.bodyRadiusPx) {
        error = path + ": body.entity_above_feet_px is at least body.radius_px";
        return false;
    }
    if (read.killerName.empty()) {
        error = path + ": killer.entity_name names an entity";
        return false;
    }
    // The fallback is itself something a marker may add, or a marker that names
    // nothing would be refused by the list it fell back past.
    if (read.defaultEntity.empty() ||
        std::find(read.entities.begin(), read.entities.end(), read.defaultEntity) == read.entities.end()) {
        error = path + ": spawn.default_entity is one of spawn.entities";
        return false;
    }
    // The body is built from body{} as it stands; a spawn at another scale would
    // need that body scaled with it, which nothing here does.
    if (read.spawnScale != 1.0) {
        error = path + ": spawn.scale is 1, the scale body{} is stated at";
        return false;
    }

    out = std::move(read);
    return true;
}

double State::SpeedPxPerSecond() const {
    if (rules.stepMs <= 0.0) return 0.0;
    return rules.speedPxPerStep * 1000.0 / rules.stepMs;
}

std::vector<entt::entity> State::Spawn(entt::registry& registry) {
    std::vector<entt::entity> made;
    for (Minion& minion : minions) {
        if (minion.body != entt::null) continue;
        minion.body = LevelBuilder::BuildRigidCircle(registry, minion.name, minion.spawnPx, rules.bodyRadiusPx,
                                                     LevelBuilder::Options{});
        if (minion.body == entt::null) continue;

        // minion.ent carries fixedRotation="1", and here that is not a detail. The
        // port builds a CIRCLE, and kPlaneLockRotation leaves z free, so friction
        // turns a slide into a ROLL: a guard that stops being driven inside the
        // wider threshold would coast straight through its post and keep going.
        // The original cannot do that, and with z locked neither can this. The
        // suite pins it, with a guard given three times a walk to carry.
        registry.get<RigidBodyComponent>(minion.body).lockRotation = glm::bvec3(true, true, true);
        made.push_back(minion.body);
    }
    return made;
}

entt::entity State::Summon(entt::registry& registry, const std::string& name, const glm::dvec2& atPx,
                           const std::vector<Waypoint>& patrol) {
    Minion minion;
    minion.name = name;
    minion.entity = rules.defaultEntity;
    minion.summoned = true;
    minion.spawnPx = atPx;
    minion.waypoints = patrol;
    // Built exactly as Spawn builds one. The locked rotation is not a detail
    // here either: minion.ent is fixedRotation="1", and a circle left free to
    // turn rolls under friction through the post it is meant to hold.
    minion.body =
        LevelBuilder::BuildRigidCircle(registry, minion.name, minion.spawnPx, rules.bodyRadiusPx,
                                       LevelBuilder::Options{});
    if (minion.body == entt::null) return entt::null;
    registry.get<RigidBodyComponent>(minion.body).lockRotation = glm::bvec3(true, true, true);

    minions.push_back(minion);
    return minion.body;
}

void State::Tick(entt::registry& registry, double dt) {
    // Taken first, as Turrets::Tick takes its spent fireballs: what the tick
    // after this one destroys is what the layer reads next, and the last is gone.
    deaths.clear();
    const double ms = static_cast<double>(dt) * 1000.0;
    const float speed = Units::ToMetres(SpeedPxPerSecond());

    for (Minion& minion : minions) {
        minion.arm = Arm::None;
        if (minion.gone || minion.body == entt::null || !registry.valid(minion.body)) continue;
        if (minion.waypoints.empty()) continue;
        if (!registry.all_of<RigidBodyComponent, TransformComponent>(minion.body)) continue;

        minion.elapsedMs += ms;

        // Both thresholds are measured from the original's ENTITY point, as its
        // waypoints are placed: GetPositionXY (ins 369-374) against a waypoint's own
        // GetPositionXY. That point stands entity_above_feet_px over the feet, and the
        // port's one circle has its centre radius_px over them, so it is the centre
        // less the difference. Measured from the centre, 9 below the waypoints the
        // levels place, no patroller ever came within arrivePx: it crossed its first
        // waypoint and turned back on every tick after (step 76).
        const glm::dvec2 atPx = PxOf(registry, minion.body);
        const glm::dvec2 entityPx = atPx - glm::dvec2(0.0, rules.EntityAboveCentrePx());
        const double distance = Length(minion.waypoints[minion.dest].atPx - entityPx);

        // Arrival, at minDist. The walk takes the NEXT waypoint, and adopts the
        // hold of the one it just reached - the dwell belongs to where it is
        // standing, not to where it is going. Wrapping is the original's test for
        // the next index having no data at all, which is this modulo.
        //
        // The timer restarts only when there is more than one waypoint. A lone
        // guard must keep its elapsed time running, or it would reset the wait it
        // is serving every tick and never finish serving it - which is exactly
        // how a sentinel hold of 999999999 stands still without a special case.
        if (distance < rules.arrivePx) {
            minion.arrivedAt = minion.dest;
            minion.dest = (minion.dest + 1) % minion.waypoints.size();
            if (minion.waypoints.size() > 1) minion.elapsedMs = 0.0;
            minion.holdMs = minion.waypoints[minion.arrivedAt].holdMs;
        }

        auto& rigid = registry.get<RigidBodyComponent>(minion.body);

        // The dwell gate, and it is the original's whole gate on moving: it walks
        // only once its timer has PASSED the hold it adopted. Failing it is not a
        // stop but a BRAKE - the original multiplies the velocity by (0.3, 1), so
        // x decays over a few frames while y is left to gravity. A lone guard on
        // the 999999999 sentinel fails this gate for ever, and the brake is what
        // parks it at its post.
        if (minion.elapsedMs <= minion.holdMs) {
            rigid.velocity.x *= static_cast<float>(rules.brake);
            minion.arm = Arm::Hold;
            continue;
        }

        // The wider threshold is NOT a second gate on every walk. The original
        // computes shouldntMove only when numWaypoints is 1 - the branch compares
        // it against 1 and takes the flag as false otherwise - so it is what stops
        // a LONE guard being driven the last stretch to its post.
        //
        // It has to be that way round. A minion that stopped 16 px out could never
        // reach the 8 px it advances at, so it would freeze one step short of its
        // own waypoint for good, never arriving and never turning round. This port
        // did exactly that until test_mp_minions caught it.
        //
        // And when it holds, NO velocity is set: the original leaves the body to
        // carry on and to friction, which is how a frictionless minion coasts the
        // last stretch in. This port's bodies grip (kBodyFriction 1), so its guard
        // stops about where the drive stops, some 15 px out, and never arrives.
        // The difference is written down in minions.json rather than papered over
        // with a zero the original never writes.
        if (minion.waypoints.size() == 1 && distance < rules.settlePx) {
            minion.arm = Arm::Settled;
            continue;
        }

        // Horizontal only: the original's moveVec is (+-1, 0), and it takes -1
        // when the two x's are exactly equal. What it falls at is the world's,
        // which is why nothing here touches y. Its sign is also the row the
        // picture walks on, which is why the facing is set here and nowhere else.
        const double destX = minion.waypoints[minion.dest].atPx.x;
        const bool right = destX > atPx.x;
        rigid.velocity.x = right ? speed : -speed;
        minion.facing = right ? Facing::Right : Facing::Left;
        minion.arm = Arm::Walk;
    }
}

std::vector<entt::entity> State::Cull(entt::registry& registry) {
    std::vector<entt::entity> removed;
    for (std::size_t i = 0; i < minions.size(); ++i) {
        Minion& minion = minions[i];
        if (minion.gone || minion.body == entt::null || !registry.valid(minion.body)) continue;
        for (const Killer& killer : killers) {
            if (!Trigger::Overlaps(registry, minion.body, killer.box)) continue;
            minion.gone = true;
            ++killed;
            deaths.push_back(Death{i, EntityPointOf(registry, minion.body, rules), true});
            registry.destroy(minion.body);
            removed.push_back(minion.body);
            break;
        }
    }
    return removed;
}

std::vector<entt::entity> State::Take(entt::registry& registry, const std::vector<entt::entity>& bodies) {
    std::vector<entt::entity> removed;
    for (const entt::entity body : bodies) {
        if (body == entt::null) continue;
        for (std::size_t i = 0; i < minions.size(); ++i) {
            Minion& minion = minions[i];
            if (minion.gone || minion.body != body) continue;
            minion.gone = true;
            ++taken;
            if (registry.valid(minion.body)) {
                deaths.push_back(Death{i, EntityPointOf(registry, minion.body, rules), false});
                registry.destroy(minion.body);
            }
            removed.push_back(minion.body);
            break;
        }
    }
    return removed;
}

std::vector<entt::entity> State::Burn(entt::registry& registry, const std::vector<entt::entity>& bodies) {
    std::vector<entt::entity> removed;
    for (const entt::entity body : bodies) {
        if (body == entt::null) continue;
        for (std::size_t i = 0; i < minions.size(); ++i) {
            Minion& minion = minions[i];
            if (minion.gone || minion.body != body) continue;
            minion.gone = true;
            ++burned;
            if (registry.valid(minion.body)) {
                Death death{i, EntityPointOf(registry, minion.body, rules), false};
                death.byFire = true;
                deaths.push_back(death);
                registry.destroy(minion.body);
            }
            removed.push_back(minion.body);
            break;
        }
    }
    return removed;
}

const Minion* State::FindMinion(const std::string& name) const {
    for (const Minion& minion : minions) {
        if (minion.name == name) return &minion;
    }
    return nullptr;
}

bool Find(const Tscn::Scene& scene, const Roles::Table& roles, const Rules& rules, State& out, std::string& error) {
    out = State{};
    out.rules = rules;

    std::vector<Placed> placed;

    for (const Tscn::Node& node : scene.nodes) {
        if (node.parent != ".") continue;

        const std::string entityName = Roles::EntityName(node);
        const std::string role = Roles::RoleOf(roles, node);

        if (role == Roles::kWaypoint) {
            Placed one;
            // By ENTITY name - wayA1 - not by the node name, which the converter
            // makes unique per placement (wayA1_1818). A marker's prefix is
            // matched against this.
            one.entityName = entityName;
            if (!PositionOf(node, one.atPx)) {
                error = node.name + " is a waypoint with no position";
                return false;
            }
            one.holdMs = MetaNumber(node, "holdTime", 0.0);
            placed.push_back(one);
            continue;
        }

        // The killer floor, by entity name: the role table files it among the
        // hazards, where it does not belong, so its role is not what finds it.
        // Its box is the shape the level gives it rather than a trigger box -
        // the 16 px fallback would miss everything that fell past it.
        if (entityName == rules.killerName) {
            glm::dvec2 offsetPx(0.0, 0.0);
            glm::dvec2 sizePx(0.0, 0.0);
            glm::dvec2 atPx(0.0, 0.0);
            if (!PositionOf(node, atPx)) {
                error = node.name + " is a killer floor with no position";
                return false;
            }
            if (!LevelBuilder::ShapeBoundsPx(scene, node, offsetPx, sizePx)) {
                error = node.name + " is a killer floor with no shape";
                return false;
            }
            Killer killer;
            killer.name = node.name;
            const glm::vec3 centre = Units::ToWorld(atPx.x + offsetPx.x, atPx.y + offsetPx.y);
            killer.box.centre = glm::vec2(centre.x, centre.y);
            killer.box.half = glm::vec2(Units::ToMetres(sizePx.x * 0.5), Units::ToMetres(sizePx.y * 0.5));
            out.killers.push_back(killer);
            continue;
        }

        if (role != Roles::kEnemySpawn) continue;

        Minion minion;
        minion.name = node.name;
        minion.waypointName = MetaText(node, "waypointName");
        // ETHCallback_minion_spawn #31-51 hands GetString('entityName') to
        // spawnMinion, whose #1-23 make an empty one 'minion.ent' before AddEntity
        // (#25-39) adds it. So the marker says what is added, and the sim holds
        // that (R16). An entity minions.json does not know is refused, not built as
        // a minion.ent's body in silence.
        minion.entity = MetaText(node, "entityName");
        if (minion.entity.empty()) minion.entity = rules.defaultEntity;
        if (std::find(rules.entities.begin(), rules.entities.end(), minion.entity) == rules.entities.end()) {
            error = node.name + " adds " + minion.entity + ", which is not among minions.json's spawn.entities";
            return false;
        }
        if (!PositionOf(node, minion.spawnPx)) {
            error = node.name + " is a minion marker with no position";
            return false;
        }
        out.minions.push_back(minion);
    }

    // And the prefixes resolved, the way the original resolves them: seek
    // waypointName + n until one is missing, and stamp what is found onto the
    // minion. The order is the index's, not the file's.
    for (Minion& minion : out.minions) {
        // Defensive rather than exercised, and an earlier wording here claimed
        // otherwise. No marker in any of the 128 levels lacks a waypointName:
        // level31b's minion_spawn_908 carries "wayB" like the rest, and the node
        // that carries none - minion_spawn_point - has no role at all and is
        // never collected above. That one is the chapter-3 boss's summon anchor,
        // which Ghost reads by name. data/minions.json had this right.
        //
        // Kept because an empty list is still the safe answer: Tick passes over a
        // minion that has no waypoints, so it would stand where it spawned.
        if (minion.waypointName.empty()) continue;
        for (std::size_t index = 0;; ++index) {
            const std::string want = minion.waypointName + std::to_string(index);
            const Placed* found = nullptr;
            for (const Placed& one : placed) {
                if (one.entityName == want) {
                    found = &one;
                    break;
                }
            }
            if (found == nullptr) break;
            Waypoint made;
            made.name = want;
            made.atPx = found->atPx;
            made.holdMs = found->holdMs;
            minion.waypoints.push_back(made);
        }
        if (minion.waypoints.empty()) {
            error = minion.name + " patrols " + minion.waypointName + ", which resolves no waypoint";
            return false;
        }
    }

    return true;
}

} // namespace MagicPortals::Minions
