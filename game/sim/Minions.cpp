#include "sim/Minions.hpp"

#include "sim/Shot.hpp"
#include "sim/Units.hpp"

#include "core/Components.hpp"
#include "core/Json.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <optional>
#include <span>
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

bool Listed(const std::vector<std::string>& names, const std::string& name) {
    return std::find(names.begin(), names.end(), name) != names.end();
}

// A segment against a circle as Box2D's b2CircleShape::RayCast answers it: the first root of the line against the
// circle, inside the segment; a segment that STARTS inside the circle meets nothing (its first root is behind it).
bool RayMeetsCircle(const glm::dvec2& from, const glm::dvec2& to, const glm::dvec2& centre, double radius) {
    const glm::dvec2 s = from - centre;
    const double b = glm::dot(s, s) - radius * radius;
    const glm::dvec2 r = to - from;
    const double c = glm::dot(s, r);
    const double rr = glm::dot(r, r);
    const double sigma = c * c - rr * b;
    if (sigma < 0.0 || rr < 1e-12) return false;
    const double a = -(c + std::sqrt(sigma));
    return 0.0 <= a && a <= rr;
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
    if (!number("patrol", "speed", read.speed)) return false;
    if (!number("patrol", "elapsed_cap_ms", read.elapsedCapMs)) return false;
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

    // The sight (planning step 158): every number named, none defaulted - a knight that sees by a number nobody
    // read would be a guess in the one place the original is exact.
    const Json::Value& sight = root["sight"];
    if (!sight.IsObject()) {
        error = path + ": sight is missing or not an object";
        return false;
    }
    const auto sightNumber = [&](const char* key, double& into) -> bool {
        if (!sight.Has(key) || !sight[key].IsNumber()) {
            error = path + ": sight." + key + " is missing or not a number";
            return false;
        }
        into = sight[key].AsNumber();
        return true;
    };
    const auto sightText = [&](const char* key, std::string& into) -> bool {
        if (!sight.Has(key) || !sight[key].IsString() || sight[key].AsString().empty()) {
            error = path + ": sight." + key + " is missing or not a name";
            return false;
        }
        into = sight[key].AsString();
        return true;
    };
    const auto sightNames = [&](const char* key, std::vector<std::string>& into) -> bool {
        if (!sight.Has(key) || !sight[key].IsArray()) {
            error = path + ": sight." + key + " is missing or not a list";
            return false;
        }
        for (const Json::Value& name : sight[key].AsArray()) {
            if (!name.IsString() || name.AsString().empty()) {
                error = path + ": sight." + key + " names an entity in each entry";
                return false;
            }
            into.push_back(name.AsString());
        }
        return true;
    };
    if (!sightNumber("fov_cos", read.sight.fovCos) || !sightNumber("bucket_units", read.sight.bucketUnits) ||
        !sightText("target_player", read.sight.targetPlayer) || !sightText("target_portal", read.sight.targetPortal) ||
        !sightNames("sight_ray_ignores", read.sight.sightRayIgnores) ||
        !sightNames("way_ray_ignores", read.sight.wayRayIgnores) ||
        !sightNames("way_ray_ignores_when_seeing", read.sight.wayRayIgnoresWhenSeeing) ||
        !sightNumber("exclamation_start_ms", read.sight.exclamationStartMs) ||
        !sightNumber("exclamation_gate_ms", read.sight.exclamationGateMs) ||
        !sightNumber("question_start_ms", read.sight.questionStartMs) ||
        !sightNumber("question_gate_ms", read.sight.questionGateMs) ||
        !sightNumber("search_period_ms", read.sight.searchPeriodMs) ||
        !sightNumber("search_right_after_ms", read.sight.searchRightAfterMs) ||
        !sightNumber("mark_above_units", read.sight.markAboveUnits) ||
        !sightNumber("mark_behind_units", read.sight.markBehindUnits)) {
        return false;
    }
    const Json::Value& shoot = sight["shoot"];
    const auto shootNumber = [&](const char* key, double& into) -> bool {
        if (!shoot.IsObject() || !shoot.Has(key) || !shoot[key].IsNumber()) {
            error = path + ": sight.shoot." + key + " is missing or not a number";
            return false;
        }
        into = shoot[key].AsNumber();
        return true;
    };
    if (!shootNumber("wait_near_ms", read.sight.waitNearMs) || !shootNumber("wait_far_ms", read.sight.waitFarMs) ||
        !shootNumber("near_units", read.sight.nearUnits) || !shootNumber("moving_speed", read.sight.movingSpeed) ||
        !shootNumber("moving_add_ms", read.sight.movingAddMs) ||
        !shootNumber("min_interval_ms", read.sight.minIntervalMs)) {
        return false;
    }
    // A cone above 1 sees nothing and one below -1 everything; a bucket of nothing divides by zero; a search period
    // that does not hold its own half is no 2000-and-2000 look.
    if (!(read.sight.fovCos >= -1.0 && read.sight.fovCos < 1.0)) {
        error = path + ": sight.fov_cos is a cosine, from -1 up to but not including 1";
        return false;
    }
    if (!(read.sight.bucketUnits > 0.0) || !(read.sight.searchPeriodMs > 0.0) ||
        !(read.sight.searchRightAfterMs >= 0.0 && read.sight.searchRightAfterMs < read.sight.searchPeriodMs)) {
        error = path + ": sight.bucket_units and search_period_ms are above 0, and search_right_after_ms inside the period";
        return false;
    }
    if (!(read.sight.exclamationGateMs >= 0.0) || !(read.sight.questionGateMs >= 0.0) ||
        !(read.sight.waitNearMs >= 0.0) || !(read.sight.waitFarMs >= 0.0) || !(read.sight.nearUnits >= 0.0) ||
        !(read.sight.movingAddMs >= 0.0) || !(read.sight.minIntervalMs >= 0.0)) {
        error = path + ": sight's gates, waits and distances are not below 0";
        return false;
    }

    // Each of these would look like a level that works. A speed of nothing is a
    // minion that stands where it spawned; a step of nothing divides by zero
    // turning that speed into one per second; an arrival of nothing is never
    // reached, so the walk goes past its waypoint and never turns round. And a
    // settle inside the arrival inverts the original's two tests, which is the
    // one mistake this module exists to avoid making.
    if (read.speed <= 0.0) {
        error = path + ": patrol.speed is above zero";
        return false;
    }
    if (read.elapsedCapMs <= 0.0) {
        error = path + ": patrol.elapsed_cap_ms is above zero";
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

double WalkOriginal(const Rules& rules, const ScriptClock::Step& step) {
    return ScriptClock::ScriptSpeed(rules.speed, rules.elapsedCapMs, step);
}

double WalkCarried(const Rules& rules, const ScriptClock::Step& step) {
    return ScriptClock::CarriedVelocity(WalkOriginal(rules, step), step);
}

std::vector<entt::entity> State::Spawn(entt::registry& registry) {
    std::vector<entt::entity> made;
    for (Minion& minion : minions) {
        if (minion.body != entt::null) continue;
        minion.body = LevelBuilder::BuildRigidCircle(registry, minion.name, minion.spawnPx, rules.bodyRadiusPx,
                                                     LevelBuilder::Options{}, minion.entity);
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
    minion.exclamationMs = rules.sight.exclamationStartMs;
    minion.questionMs = rules.sight.questionStartMs;
    // Built exactly as Spawn builds one. The locked rotation is not a detail
    // here either: minion.ent is fixedRotation="1", and a circle left free to
    // turn rolls under friction through the post it is meant to hold.
    minion.body =
        LevelBuilder::BuildRigidCircle(registry, minion.name, minion.spawnPx, rules.bodyRadiusPx,
                                       LevelBuilder::Options{}, minion.entity);
    if (minion.body == entt::null) return entt::null;
    registry.get<RigidBodyComponent>(minion.body).lockRotation = glm::bvec3(true, true, true);

    minions.push_back(minion);
    return minion.body;
}

glm::ivec2 BucketOf(const glm::dvec2& point, double bucketUnits) {
    return glm::ivec2(static_cast<int>(std::floor(point.x / bucketUnits)),
                      static_cast<int>(std::floor(point.y / bucketUnits)));
}

bool RayClear(entt::registry& registry, const glm::dvec2& fromPx, const glm::dvec2& toPx,
              const std::vector<entt::entity>& ignore, const Senses& senses, bool playerBlocks) {
    // ETHPhysicsSimulator::GetClosestContact (ETHPhysicsSimulator.cpp:251-268) returns nothing for a == b.
    if (fromPx == toPx) return true;
    if (Shot::ClosestContact(registry, fromPx, toPx, std::span<const entt::entity>(ignore))) return false;
    if (playerBlocks && senses.playerPresent) {
        for (const Senses::Circle& circle : senses.playerBodyCircles) {
            if (circle.radiusPx > 0.0 && RayMeetsCircle(fromPx, toPx, circle.centrePx, circle.radiusPx)) return false;
        }
    }
    return true;
}

std::optional<Seen> Look(entt::registry& registry, const Rules& rules, const glm::dvec2& agentPx,
                         const glm::dvec2& moveVec, const Senses& senses, const std::vector<entt::entity>& ignore) {
    const Sight& sight = rules.sight;
    // #15-55: a facing that is not a unit vector (dp2 of it with itself is not 1) is 'Warning: enemy aims nowhere'.
    if (glm::dot(moveVec, moveVec) != 1.0) return std::nullopt;

    // #56-84: the agent's bucket, the next along the facing and the one after.
    const glm::dvec2 agentBucket(BucketOf(agentPx, sight.bucketUnits));
    const std::array<glm::dvec2, 3> buckets{agentBucket, agentBucket + moveVec, agentBucket + moveVec + moveVec};

    struct Candidate {
        glm::dvec2 atPx;
        const std::string* name;
        int portalIndex;
    };
    std::vector<Candidate> candidates;
    // #578-602: 'portal.ent', and the player's name only while the knight can see anything.
    if (senses.canSeeAnything && senses.playerPresent) {
        candidates.push_back(Candidate{senses.playerPx, &sight.targetPlayer, -1});
    }
    for (std::size_t i = 0; i < senses.portalsPx.size(); ++i) {
        candidates.push_back(Candidate{senses.portalsPx[i], &sight.targetPortal, static_cast<int>(i)});
    }

    for (const glm::dvec2& bucket : buckets) {
        for (const Candidate& candidate : candidates) {
            if (glm::dvec2(BucketOf(candidate.atPx, sight.bucketUnits)) != bucket) continue;
            // #167-187: normalize(target - agent), a zero vector staying zero (GameMath.h:755-761).
            const glm::dvec2 toward = candidate.atPx - agentPx;
            const double length = glm::length(toward);
            const glm::dvec2 direction = length > 0.0 ? toward / length : glm::dvec2(0.0);
            // #188-204: above the cone's cosine, never at it.
            if (!(glm::dot(moveVec, direction) > sight.fovCos)) continue;
            // #205-252: nothing between them but what the sight ray's list names.
            if (!RayClear(registry, agentPx, candidate.atPx, ignore, senses, false)) continue;
            Seen seen;
            seen.atPx = candidate.atPx;
            seen.name = *candidate.name;
            seen.portalIndex = candidate.portalIndex;
            return seen;
        }
    }
    return std::nullopt;
}

void State::BindSight(const Tscn::Scene& scene, const LevelBuilder::Built& built) {
    sightIgnoredPlaced.clear();
    wayIgnoredPlaced.clear();
    for (const Tscn::Node& node : scene.nodes) {
        if (node.parent != ".") continue;
        // By the name the node is spelled with: ETHEntityNameArrayChooser compares GetEntityName() exactly, so
        // 'crate_small' is passed and 'crate_small.ent' is not.
        const std::string name = Roles::EntityName(node);
        const auto found = built.entities.find(node.name);
        if (found == built.entities.end()) continue;
        if (Listed(rules.sight.sightRayIgnores, name)) sightIgnoredPlaced.push_back(found->second);
        if (Listed(rules.sight.wayRayIgnores, name)) wayIgnoredPlaced.push_back(found->second);
    }
}

int State::SpotCount() const {
    int total = 0;
    for (const Minion& minion : minions) {
        if (!minion.gone && minion.entity == rules.defaultEntity) total += minion.spotCount;
    }
    return total;
}

void State::Tick(entt::registry& registry, const ScriptClock::Step& step) {
    Tick(registry, step, Senses{});
}

void State::Tick(entt::registry& registry, const ScriptClock::Step& step, const Senses& senses) {
    // Taken first, as Turrets::Tick takes its spent fireballs: what the tick
    // after this one destroys is what the layer reads next, and the last is gone.
    deaths.clear();
    marks.clear();
    shotsDue.clear();
    const double ms = ScriptClock::Ms(step);
    // Metres a second the body carries: the original's walk at the tick's SimRatio (WalkCarried).
    const float speed = static_cast<float>(WalkCarried(rules, step));
    const Sight& sight = rules.sight;

    // The bodies the two rays pass through this tick: what the level places under those names, and every knight
    // that stands (their entity names are on both lists), and the player where a list names it.
    std::vector<entt::entity> sightIgnore = sightIgnoredPlaced;
    std::vector<entt::entity> wayIgnore = wayIgnoredPlaced;
    for (const Minion& other : minions) {
        if (other.gone || other.body == entt::null || !registry.valid(other.body)) continue;
        if (Listed(sight.sightRayIgnores, other.entity)) sightIgnore.push_back(other.body);
        if (Listed(sight.wayRayIgnores, other.entity)) wayIgnore.push_back(other.body);
    }
    // #917-928: the walk's list gains the player's name only while the knight can see anything; a knight that cannot
    // is stopped by the player's body standing in its way.
    const bool playerOnWayList =
        Listed(sight.wayRayIgnores, sight.targetPlayer) ||
        (senses.canSeeAnything && Listed(sight.wayRayIgnoresWhenSeeing, sight.targetPlayer));
    if (senses.playerBody != entt::null && registry.valid(senses.playerBody)) {
        if (Listed(sight.sightRayIgnores, sight.targetPlayer)) sightIgnore.push_back(senses.playerBody);
        if (playerOnWayList) wayIgnore.push_back(senses.playerBody);
    }

    for (std::size_t index = 0; index < minions.size(); ++index) {
        Minion& minion = minions[index];
        minion.arm = Arm::None;
        minion.seeing = false;
        if (minion.gone || minion.body == entt::null || !registry.valid(minion.body)) continue;
        if (minion.waypoints.empty()) continue;
        if (!registry.all_of<RigidBodyComponent, TransformComponent>(minion.body)) continue;

        // #326-367: the three clocks that run every frame.
        minion.elapsedMs += ms;
        minion.exclamationMs += ms;
        minion.shootMs += ms;

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
        const glm::dvec2 destPx = minion.waypoints[minion.dest].atPx;

        // #562-572: the dwell the walk is judged by is read HERE, before a sighting below resets the datum.
        const double elapsedBefore = minion.elapsedMs;

        auto& rigid = registry.get<RigidBodyComponent>(minion.body);

        // THE LOOK (#578-898). findEntityInSight reads the facing the last walk or search left.
        const std::optional<Seen> seen = Look(registry, rules, entityPx, minion.moveVec, senses, sightIgnore);
        minion.seeing = seen.has_value();
        if (seen && !senses.finished) {
            if (!minion.spotted) {
                // #646-684: a first sighting stands the knight still (the dwell restarts), counts, and - once its
                // timer has passed the gate - shows the mark with the sound.
                minion.spotted = true;
                minion.elapsedMs = 0.0;
                ++minion.spotCount;
                if (minion.exclamationMs > sight.exclamationGateMs) {
                    minion.exclamationMs = 0.0;
                    Mark mark;
                    mark.minion = index;
                    mark.kind = Mark::Kind::Exclamation;
                    mark.entityPx = entityPx;
                    mark.sound = true;
                    marks.push_back(mark);
                }
            }
            // #755-884: the shot's gate, which is read on the first frame of a sighting too.
            const double away = Length(seen->atPx - entityPx);
            double wait = away < sight.nearUnits ? sight.waitNearMs : sight.waitFarMs;
            // #791-811: the KNIGHT's own velocity, in the original's units: the carried frame is SimRatio times it.
            const float vx = rigid.velocity.x;
            const float vy = rigid.velocity.y;
            const double own = ScriptClock::OriginalVelocity(std::sqrt(static_cast<double>(vx) * vx + static_cast<double>(vy) * vy), step);
            if (own > sight.movingSpeed) wait += sight.movingAddMs;
            if (minion.elapsedMs > wait && minion.shootMs > sight.minIntervalMs) {
                minion.elapsedMs = 0.0;
                minion.shootMs = 0.0;
                ShotDue due;
                due.minion = index;
                due.casterPx = entityPx;
                due.finalPx = seen->atPx;
                due.targetName = seen->name;
                due.portalIndex = seen->portalIndex;
                shotsDue.push_back(due);
            }
        } else {
            minion.spotted = false;
        }

        // THE WAY (#899-975): a ray from the knight to the waypoint it is walking to.
        minion.wayClear = RayClear(registry, entityPx, destPx, wayIgnore, senses, !playerOnWayList);
        if (minion.wayClear) minion.wayWasClear = true;

        // The dwell gate, and it is the original's whole gate on moving, with the way: it walks
        // only once its timer has PASSED the hold it adopted, and only down a clear way. Failing it is not a
        // stop but a BRAKE - the original multiplies the velocity by (0.3, 1), so
        // x decays over a few frames while y is left to gravity. A lone guard on
        // the 999999999 sentinel fails this gate for ever, and the brake is what
        // parks it at its post.
        if (!(minion.wayClear && elapsedBefore > minion.holdMs)) {
            rigid.velocity.x *= static_cast<float>(rules.brake);
            minion.arm = minion.seeing ? Arm::Watch : Arm::Hold;
            // #1206-1228: a blocked way and nothing in sight is the search.
            if (!minion.wayClear && !minion.spotted) {
                minion.arm = Arm::Search;
                // #1229-1277: elapsedTime % 4000 above 2000 looks right, else left. The datum is a uint.
                const double phase = std::fmod(std::floor(std::max(0.0, minion.elapsedMs)), sight.searchPeriodMs);
                minion.moveVec = glm::dvec2(phase > sight.searchRightAfterMs ? 1.0 : -1.0, 0.0);
                // #1322-1437: the question mark, on its own clock, which only this arm adds to.
                minion.questionMs += ms;
                if (minion.questionMs > sight.questionGateMs) {
                    minion.questionMs = 0.0;
                    Mark mark;
                    mark.minion = index;
                    mark.kind = Mark::Kind::Question;
                    mark.entityPx = entityPx;
                    mark.sound = minion.wayWasClear;
                    mark.soundNeedsScreen = true;
                    marks.push_back(mark);
                    minion.wayWasClear = false;
                }
            }
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
        //
        // Horizontal only: the original's moveVec is (+-1, 0), and it takes -1
        // when the two x's are exactly equal. What it falls at is the world's,
        // which is why nothing here touches y. Its sign is also the row the
        // picture walks on, which is why the facing is set here and nowhere else.
        // And moveVec - the way it LOOKS - is written before the lone guard's test (#1028 against #1070-1084).
        const bool right = destPx.x > entityPx.x;
        minion.moveVec = glm::dvec2(right ? 1.0 : -1.0, 0.0);
        if (minion.waypoints.size() == 1 && Length(destPx - entityPx) < rules.settlePx) {
            minion.arm = Arm::Settled;
            continue;
        }
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
        minion.exclamationMs = rules.sight.exclamationStartMs;
        minion.questionMs = rules.sight.questionStartMs;
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
