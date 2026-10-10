#include "sim/Portals.hpp"

#include "core/Components.hpp"
#include "core/Json.hpp"
#include "sim/Resting.hpp"
#include "sim/Units.hpp"
#include "sim/Zerog.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <optional>
#include <sstream>

namespace MagicPortals::Portals {

using Supersonic::RigidBodyComponent;
using Supersonic::TransformComponent;

namespace {

// portal_system.gd:230-261, in the Godot project's space: pixels, +y down. Placed
// portals are never turned, so rotate_to_exit would turn through nothing here.
// The port plays the original's invert (transit.json): the traveller comes out
// at the exit itself, turned back, and the player in y only.
void Teleport(entt::registry& registry, entt::entity body, const glm::dvec2& exitPx, const Portal::Transit& transit,
              bool character) {
    auto& transform = registry.get<TransformComponent>(body);
    auto& rigid = registry.get<RigidBodyComponent>(body);
    // What lies on or against the traveller wakes before it goes (sim/Resting): a sleeper whose support leaves would hang in the air.
    Resting::WakeAround(registry, body);
    const glm::dvec2 velocityPx(rigid.velocity.x * Units::kPixelsPerMetre, -rigid.velocity.y * Units::kPixelsPerMetre);
    const glm::dvec2 exitVelocityPx = Portal::ExitVelocity(velocityPx, 0.0, 0.0, transit, character);
    const glm::dvec2 destinationPx = Portal::ExitPosition(exitPx, exitVelocityPx, transit);
    const glm::vec3 destination = Units::ToWorld(destinationPx.x, destinationPx.y);
    transform.position = glm::vec3(destination.x, destination.y, transform.position.z);
    rigid.velocity =
        glm::vec3(Units::ToMetres(exitVelocityPx.x), Units::ToMetres(-exitVelocityPx.y), rigid.velocity.z);
}

// A static callback's own test of a shot's position: squaredDistance(position, the agent's) < reach ^ 2,
// STRICT (ETHCallback_anti_portal_agent #110-124, ETHCallback_fire_agent #109-117). The original
// samples the position once a frame; State::Tick says which points.
bool Reaches(const glm::dvec2& shotPx, const glm::dvec2& centrePx, double reachPx) {
    const glm::dvec2 gap = shotPx - centrePx;
    return glm::dot(gap, gap) < reachPx * reachPx;
}

} // namespace

bool LoadRules(const std::string& path, Rules& out, std::string& error) {
    Rules read;
    if (!Portal::LoadTransit(path, read.transit, error)) return false;

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
    const auto field = [&](const char* block, const char* key) -> const Supersonic::Json::Value* {
        if (!root.IsObject() || !root.Has(block) || !root[block].IsObject() || !root[block].Has(key)) {
            error = path + ": " + block + "." + key + " is missing";
            return nullptr;
        }
        return &root[block][key];
    };
    const auto number = [&](const char* block, const char* key, double& to) {
        const Supersonic::Json::Value* value = field(block, key);
        if (value == nullptr) return false;
        if (!value->IsNumber()) {
            error = path + ": " + block + "." + key + " is not a number";
            return false;
        }
        to = value->AsNumber();
        return true;
    };
    const auto flag = [&](const char* block, const char* key, bool& to) {
        const Supersonic::Json::Value* value = field(block, key);
        if (value == nullptr) return false;
        if (!value->IsBool()) {
            error = path + ": " + block + "." + key + " is not a bool";
            return false;
        }
        to = value->AsBool();
        return true;
    };

    double maxPortals = 0.0;
    // READ AND CHECKED, then dropped. The Godot project marks its placement block
    // _guess and its own _radius_note says the value was unknown; the decoded
    // radii come from the port's placement.json instead (LoadPlacement). It is
    // still required here so a malformed portals.json is refused rather than
    // half-read.
    double remakeRadiusPx = 0.0;
    if (!number("placement", "default_max_portals", maxPortals) ||
        !number("placement", "collision_radius_px", remakeRadiusPx) ||
        !flag("consumption", "consume_on_traverse", read.consumeOnTraverse) ||
        !flag("consumption", "static_portals_persist", read.staticPortalsPersist) ||
        !flag("consumption", "on_cap_recycle_oldest", read.recycleOldestAtCap)) {
        return false;
    }
    read.defaultMaxPortals = static_cast<int>(maxPortals);
    out = read;
    return true;
}

bool LoadPlacement(const std::string& path, Rules& out, std::string& error) {
    namespace Json = Supersonic::Json;

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
    const Json::Value& portal = root["portal"];
    const Json::Value& antiportal = root["antiportal"];
    if (!portal.IsObject() || !antiportal.IsObject()) {
        error = path + ": portal and antiportal are each an object";
        return false;
    }
    if (!portal["entry_radius_px"].IsNumber() || !antiportal["radius_px"].IsNumber()) {
        error = path + ": portal.entry_radius_px and antiportal.radius_px are each a number";
        return false;
    }

    const Json::Value& cooldowns = root["cooldowns"];
    if (!cooldowns.IsObject() || !cooldowns["first_ms"].IsNumber() || !cooldowns["next_ms"].IsNumber()) {
        error = path + ": cooldowns.first_ms and cooldowns.next_ms are each a number";
        return false;
    }
    const double firstMs = cooldowns["first_ms"].AsNumber(-1.0);
    const double nextMs = cooldowns["next_ms"].AsNumber(-1.0);
    // Zero is allowed - it means no cooldown at all, which is what the Godot project
    // carried for the first one - but a negative is a file saying nothing
    // sensible.
    if (firstMs < 0.0 || nextMs < 0.0) {
        error = path + ": neither cooldown is below zero";
        return false;
    }
    out.firstPortalMinMs = firstMs;
    out.nextPortalMinMs = nextMs;

    // WHEN A TOUCH FIRES: on its release, within scale(18). Required, and above zero:
    // zero would let only a touch that never moved at all fire, which no file says.
    const Json::Value& release = root["release"];
    if (!release.IsObject() || !release["max_move_units"].IsNumber() ||
        !(release["max_move_units"].AsNumber(0.0) > 0.0)) {
        error = path + ": release.max_move_units is a number above zero";
        return false;
    }
    out.releaseMaxMoveUnits = release["max_move_units"].AsNumber(0.0);

    // HOW LONG A PORTAL LASTS, and how it hovers while it does: ETHCallback_portal's
    // own two arms, both required. Zero perish is legal and means "never", which is
    // what a suite that wants a portal to stay for a long run asks for.
    if (!portal["perish_ms"].IsNumber() || portal["perish_ms"].AsNumber(-1.0) < 0.0) {
        error = path + ": portal.perish_ms is a number at or above zero";
        return false;
    }
    out.perishMs = portal["perish_ms"].AsNumber(0.0);
    const Json::Value& bob = portal["bob"];
    if (!bob.IsObject() || !bob["speed"].IsNumber() || !bob["stride"].IsNumber() || !bob["vertical"].IsBool() ||
        !bob["axis_deg"].IsNumber() || !bob["start_angle"].IsNumber()) {
        error = path + ": portal.bob needs speed, stride, vertical, axis_deg and start_angle";
        return false;
    }
    Motion::Row row;
    row.speed = bob["speed"].AsNumber(0.0);
    row.stride = bob["stride"].AsNumber(0.0);
    row.vertical = bob["vertical"].AsBool();
    row.axisDeg = bob["axis_deg"].AsNumber(0.0);
    // linearMotion takes ONE start angle here, not a drawn one: the call passes a
    // literal 0f, so both ends of the row are it.
    row.startFrom = bob["start_angle"].AsNumber(0.0);
    row.startTo = row.startFrom;
    out.bob = row;

    const double entry = portal["entry_radius_px"].AsNumber(0.0);
    const double field = antiportal["radius_px"].AsNumber(0.0);
    // A radius of nothing is a portal nobody can walk into and a field that
    // refuses nothing, and each would look like a level that works.
    if (entry <= 0.0 || field <= 0.0) {
        error = path + ": both radii are above zero";
        return false;
    }

    // The manager's entity and its Scale: both required. Without them the ring
    // would be drawn at some size the file never said.
    const Json::Value& manager = antiportal["manager"];
    if (!manager.IsObject() || !manager["entity"].IsString() || !manager["scale"].IsNumber()) {
        error = path + ": antiportal.manager is an object with a string entity and a number scale";
        return false;
    }
    const std::string managed = manager["entity"].AsString();
    const double managerScale = manager["scale"].AsNumber(0.0);
    // The name the manager collects, without .ent: it collects both spellings.
    if (managed.empty() || managed.find('.') != std::string::npos || managerScale <= 0.0) {
        error = path + ": antiportal.manager.entity is a name without a dot, and its scale is above zero";
        return false;
    }

    // computePortalFinalPos's constants, every one required: the ray and its
    // retry decide the mark a tap leaves, and a default here would be a guess.
    const Json::Value& finalPos = root["final_pos"];
    if (!finalPos.IsObject() || !finalPos["ray_start_units"].IsNumber() || !finalPos["push_units"].IsNumber() ||
        !finalPos["retry_back_units"].IsNumber() || !finalPos["ray_ignores"].IsArray() ||
        !finalPos["fire_diamond_reach"].IsNumber() ||
        !finalPos["retry_from_units"].IsArray() || finalPos["retry_from_units"].AsArray().size() != 2 ||
        !finalPos["retry_from_units"].AsArray()[0].IsNumber() || !finalPos["retry_from_units"].AsArray()[1].IsNumber()) {
        error = path + ": final_pos needs ray_start_units, ray_ignores, push_units, retry_from_units [x, y], "
                       "retry_back_units and fire_diamond_reach";
        return false;
    }
    Rules::FinalPosRules readFinal;
    readFinal.rayStartUnits = finalPos["ray_start_units"].AsNumber(-1.0);
    readFinal.pushUnits = finalPos["push_units"].AsNumber(-1.0);
    readFinal.retryBackUnits = finalPos["retry_back_units"].AsNumber(-1.0);
    readFinal.fireDiamondReach = finalPos["fire_diamond_reach"].AsNumber(-1.0);
    readFinal.retryFromUnits = glm::dvec2(finalPos["retry_from_units"].AsArray()[0].AsNumber(0.0),
                                          finalPos["retry_from_units"].AsArray()[1].AsNumber(0.0));
    if (!(readFinal.rayStartUnits > 0.0) || !(readFinal.pushUnits > 0.0) || !(readFinal.retryBackUnits > 0.0) ||
        !(readFinal.fireDiamondReach > 1.0) || readFinal.retryFromUnits == glm::dvec2(0.0)) {
        error = path + ": final_pos's distances are each above zero, its fire-diamond reach is above 1, and its "
                       "retry starts somewhere else";
        return false;
    }
    for (const Json::Value& name : finalPos["ray_ignores"].AsArray()) {
        if (!name.IsString() || name.AsString().empty()) {
            error = path + ": final_pos.ray_ignores holds entity names";
            return false;
        }
        readFinal.rayIgnores.push_back(name.AsString());
    }
    const Json::Value& noBudget = root["no_budget"];
    if (!noBudget.IsObject() || !noBudget["sign_entity"].IsString() || noBudget["sign_entity"].AsString().empty()) {
        error = path + ": no_budget.sign_entity is the entity a budget-0 tap looks for";
        return false;
    }

    // What kills an anti-portal agent and which ring goes with it: every number
    // required, since each is the decode's and a default would be a guess.
    const Json::Value& agent = root["anti_portal_agent"];
    if (!agent.IsObject() || !agent["size_units"].IsNumber() || !agent["kill_share_of_size"].IsNumber() ||
        !agent["ring_entity"].IsString() || !agent["bucket_units"].IsNumber() || !agent["seek_neighbours"].IsArray()) {
        error = path + ": anti_portal_agent needs size_units, kill_share_of_size, ring_entity, bucket_units and "
                       "seek_neighbours";
        return false;
    }
    Rules::AgentRules readAgent;
    readAgent.sizeUnits = agent["size_units"].AsNumber(0.0);
    readAgent.killShare = agent["kill_share_of_size"].AsNumber(0.0);
    readAgent.ringEntity = agent["ring_entity"].AsString();
    readAgent.bucketUnits = agent["bucket_units"].AsNumber(0.0);
    if (!(readAgent.sizeUnits > 0.0) || !(readAgent.killShare > 0.0) || readAgent.ringEntity.empty() ||
        !(readAgent.bucketUnits > 0.0)) {
        error = path + ": anti_portal_agent's size, share and bucket are above zero and it names a ring";
        return false;
    }
    for (const Json::Value& offset : agent["seek_neighbours"].AsArray()) {
        if (!offset.IsArray() || offset.AsArray().size() != 2 || !offset.AsArray()[0].IsNumber() ||
            !offset.AsArray()[1].IsNumber()) {
            error = path + ": anti_portal_agent.seek_neighbours holds [x, y] bucket offsets";
            return false;
        }
        readAgent.seekNeighbours.emplace_back(offset.AsArray()[0].AsNumber(0.0), offset.AsArray()[1].AsNumber(0.0));
    }

    out.entryRadiusPx = entry;
    out.antiportalRadiusPx = field;
    out.antiportalEntity = managed;
    out.antiportalManagerScale = managerScale;
    out.finalPos = readFinal;
    out.noPortalSignEntity = noBudget["sign_entity"].AsString();
    out.agent = readAgent;
    return true;
}

bool LoadGate(const std::string& path, Rules& out, std::string& error) {
    namespace Json = Supersonic::Json;

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
    const Json::Value& gate = root["platform_gate"];
    if (!gate.IsObject() || !gate["entities"].IsArray() || !gate["radius_share"].IsNumber()) {
        error = path + ": platform_gate holds an entities list and a radius_share";
        return false;
    }
    Rules::GateRules read;
    for (const Json::Value& name : gate["entities"].AsArray()) {
        if (!name.IsString() || name.AsString().empty()) {
            error = path + ": platform_gate.entities holds a value that is not a name";
            return false;
        }
        read.entities.push_back(name.AsString());
    }
    read.radiusShare = gate["radius_share"].AsNumber(0.0);
    if (read.entities.empty() || !(read.radiusShare > 0.0)) {
        error = path + ": platform_gate names at least one entity, and its radius_share is above zero";
        return false;
    }
    out.gate = read;
    return true;
}

bool State::GrabsBox(const glm::dvec2& centrePx, const glm::dvec2& halfPx, const glm::dvec2& atPx,
                     double radiusPx) {
    // How far the point lies past each side: overlapSphere refuses once one plane's
    // inward distance is at or below -radius, so each must stay short of it - the
    // four sides, and the corner plane through the nearest corner at 45 degrees.
    const glm::dvec2 past = glm::abs(atPx - centrePx) - halfPx;
    return past.x < radiusPx && past.y < radiusPx && past.x + past.y < radiusPx * std::sqrt(2.0);
}

bool State::PlatformInAnyPortal(entt::registry& registry) const {
    const double radius = rules.gate.radiusShare * rules.entryRadiusPx;
    for (const Placed& portal : placed) {
        for (const GateBody& body : gateBodies) {
            if (!registry.valid(body.entity)) continue;
            const auto* transform = registry.try_get<TransformComponent>(body.entity);
            if (transform == nullptr) continue;
            if (GrabsBox(Units::ToPixels(transform->position), body.halfPx, portal.atPx, radius)) return true;
        }
    }
    return false;
}

bool State::Holds(const NoPortalZone& zone, const glm::dvec2& atPx) const {
    // A deleted one holds nothing (isPointInAntiPortalField #1-10, IsAlive).
    if (zone.gone) return false;
    // The ANTIPORTAL's radius, which is half its own sprite times the node's
    // scale - not the portal's own 14. placement.json has the decode.
    //
    // A zone carrying its OWN radius uses that instead. A gravity well's ring is
    // not a zone at all: the area its agent adds is renamed gravity_area on the
    // way in, after AntiPortalManager has gathered its fields by name, so the
    // original never tests a tap against it (00_order R2).
    const double reachPx = zone.radiusPx > 0.0 ? zone.radiusPx : rules.antiportalRadiusPx * zone.scale;
    return glm::distance(atPx, zone.CentreNowPx()) <= reachPx;
}

bool State::InField(const glm::dvec2& atPx) const {
    return std::any_of(zones.begin(), zones.end(), [&](const NoPortalZone& zone) { return Holds(zone, atPx); });
}

std::string State::DeleteRingAt(const glm::dvec2& atPx) {
    // deleteAntiPortalInThisBucket (bytes 335206..336011): the point's own bucket
    // first, then each neighbour once, in the decode's order. A bucket is
    // floor(pos / bucketUnits) of an entity's own position (ETHBucketManager.cpp:
    // 47-50), and inside one the level's own order stands for the bucket's: the
    // zones are read in the scene's order and every one of them is static.
    const double size = rules.agent.bucketUnits;
    const auto bucketOf = [size](const glm::dvec2& p) { return glm::floor(p / size); };
    const glm::dvec2 home = bucketOf(atPx);
    std::vector<glm::dvec2> order{home};
    for (const glm::dvec2& offset : rules.agent.seekNeighbours) order.push_back(home + offset);
    for (const glm::dvec2& bucket : order) {
        for (NoPortalZone& zone : zones) {
            // The bare spelling alone: GetEntityName() == 'antiportal' (#13-33).
            if (zone.entity != rules.agent.ringEntity || bucketOf(zone.CentreNowPx()) != bucket) continue;
            if (!Holds(zone, atPx)) continue;
            zone.gone = true;
            return zone.name;
        }
    }
    return std::string();
}

double State::AgentReachPx(const Agent& agent) const {
    return rules.agent.killShare * rules.agent.sizeUnits * agent.scale;
}

const Agent* State::FindAgent(const std::string& name) const {
    for (const Agent& agent : agents) {
        if (agent.name == name) return &agent;
    }
    return nullptr;
}

bool State::TryPlace(const glm::dvec2& atPx) {
    if (budget <= 0) return false;
    if (InField(atPx)) return false;
    return Insert(atPx);
}

bool State::Insert(const glm::dvec2& atPx) {
    if (budget <= 0) return false;
    if (static_cast<int>(placed.size()) >= budget) {
        if (!rules.recycleOldestAtCap) return false;
        placed.erase(placed.begin());
    }
    Placed portal;
    // ITS OWN NAME, AND NEVER THE COUNT: nextPortalId is handed out once and
    // never given back, where portalsUsed is the golden score's and KillAll
    // refunds it. State::nextPortalId has what tying the two together cost.
    portal.id = nextPortalId++;
    // linearMotion's originalPos is the position of its FIRST call, which is the
    // frame the entity was added on and before anything moved it: where the shot
    // opened it. The hover below is measured from there.
    portal.basePx = atPx;
    portal.atPx = atPx;
    portal.bob = Motion::Start(rules.bob, rules.bob.startFrom);
    // And the PORTAL's own, g_portalCollisionRadius.
    portal.trigger = Trigger::Circle{glm::vec2(0.0f), Units::ToMetres(rules.entryRadiusPx)};
    Centre(portal);
    placed.push_back(portal);
    ++portalsUsed;
    return true;
}

TouchRelease TrackTouch(TouchTrack& track, bool pressed, bool held, bool released, bool over,
                        const glm::dvec2& atView, const glm::dvec2& atPx) {
    TouchRelease out;
    // A pointer that left the game is not a touch the game can follow: whatever it
    // does out there is not measured, so it fires nothing when it comes back.
    if (!over) {
        track = TouchTrack{};
        return out;
    }
    if (pressed) {
        // GSKS_HIT: maxMoveLength 0, hitPos and lastTouchPos the touch.
        track = TouchTrack{true, atView, atView, atPx, 0.0};
    } else if (track.down && held) {
        // GSKS_DOWN: lastTouchPos the touch, and the farthest from hitPos kept.
        track.lastView = atView;
        track.lastPx = atPx;
        track.maxMoveUnits = std::max(track.maxMoveUnits, glm::length(atView - track.hitView));
    }
    if (track.down && released) {
        // GSKS_RELEASE: nothing updated on this frame; the record is what it was.
        out.released = true;
        out.atView = track.lastView;
        out.atPx = track.lastPx;
        out.maxMoveUnits = track.maxMoveUnits;
        track = TouchTrack{};
    } else if (track.down && !held) {
        // Up, and this record never saw it come up (a tick nobody tracked took the
        // release): it is over, and fires nothing.
        track = TouchTrack{};
    }
    return out;
}

bool RequestsPortal(const Rules& rules, const TouchRelease& release) {
    // #19-20 CMPf maxMoveLength, scale(18); TNP: at most, so a move of exactly 18 fires.
    return release.released && release.maxMoveUnits <= rules.releaseMaxMoveUnits;
}

bool State::Shoot(entt::registry& registry, const glm::dvec2& atPx, double velocityRatio,
                  std::optional<bool> fireDiamondWillDie) {
    // The two clocks' verdicts. update's first gate is a strict "less than" and managePortalInsertion's
    // is a "more than", so a tap exactly at either limit is refused by the second and passes the first.
    const bool tooEarly = rules.firstPortalMinMs > 0.0 && sinceStartMs < rules.firstPortalMinMs;
    const bool tooSoon = rules.nextPortalMinMs > 0.0 && sinceTapMs <= rules.nextPortalMinMs;
    lastTap = Tap{};
    lastTap.atPx = atPx;

    // THE ORIGINAL'S ORDER OF TESTS (PortalManager::update 138545..139692, then managePortalInsertion
    // 143178..144166 or blockPortalEffects 137862..138545), planning step 156.
    //
    // No character to shoot from: the port's own case.
    if (shooter == entt::null || !registry.valid(shooter)) {
        lastTap.reason = TapReason::NoShooter;
        return false;
    }
    // update #16-46: the level's first FIRST_PORTAL_MIN_TIME. Nothing is drawn and no clock moves.
    if (tooEarly) {
        lastTap.reason = TapReason::TooEarly;
        return false;
    }
    // update #48-56: a budget of 0 hands the tap to blockPortalEffects, which never reads the 400 ms
    // clock (lastPortalTimer) and never restarts it: red_waves (B) and a shake where the level places
    // the sign (the layer's), every tap.
    if (budget <= 0) {
        lastTap.reason = TapReason::NoBudget;
        return false;
    }
    // managePortalInsertion #43-73: a tap inside 400 ms of the last it took draws nothing and moves no
    // clock - it is the next touch's turn - so this refusal is silent and the clock runs on.
    if (tooSoon) {
        lastTap.reason = TapReason::Cooldown;
        return false;
    }

    // THE TAP IS TAKEN, and "taken" is all that has happened. lastPortalTimer.reset() (#75-78) comes
    // before anything is known of the shot - before the test for a projectile that is still alive - so
    // the 400 ms is spent by a tap that goes on to show the hourglass or to hit a wall, and
    // placement.json says why that does not contradict "a failed shot costs nothing" (that is about
    // the budget). THE SPEC OF THIS STEP SAID EVERY REFUSAL RESTARTS IT; the decode says this one does
    // and the two above do not.
    sinceTapMs = 0.0;
    // #80-99 and #181-210: a projectile.ent exists, so the tap shows the hourglass and fires nothing
    // (the character is not turned toward it either: #101-124 are on the other arm).
    if (flight) {
        lastTap.reason = TapReason::InFlight;
        return false;
    }
    lastTap.reason = TapReason::Fired;

    Flight fired;
    fired.fromPx = Units::ToPixels(registry.get<TransformComponent>(shooter).position);
    fired.atPx = fired.fromPx;
    // #127-145: computePortalFinalPos(absTouchPos, ...) from the character. Its answer is the whole of
    // the shot: where it goes (finalPos), and whether it is doomed (hasFailed).
    //
    // A fire diamond carried is the method's own branch (#21-64): no ray, finalPos 64 x scale out along
    // the aim, and hasFailed is the caller's leftover byte - which the layer reads off the tap's x and
    // hands in. The shot never arrives: Game turns it into a fireball on its first tick.
    if (fireDiamondWillDie.has_value()) {
        fired.said.atPx = fired.fromPx + (atPx - fired.fromPx) * rules.finalPos.fireDiamondReach;
        fired.said.willDie = *fireDiamondWillDie;
    } else {
        fired.said = Predict(registry, fired.fromPx, atPx);
    }
    fired.toPx = fired.said.atPx;
    lastTap.said = fired.said;
    // The tap's AddEntity is in `loop`, after this frame's dynamic callbacks: the static callbacks of
    // this frame see the shot where it was spawned, and its own first callback is the next frame's
    // (Tick's first pass, then its second). The layer takes the tap after its tick's AfterStep, so
    // the next Tick IS the next frame.
    fired.fresh = true;
    flight = fired;
    ++shotsFired;

    // THE RECOIL, in a zero-gravity level, and it is how the player moves there.
    //
    // Here rather than where the shot arrives, because the original puts it
    // here: managePortalInsertion calls applyImpulse immediately after the tap
    // is taken, on the joined path that a failed shot comes down too. So the
    // shove is spent on a tap that goes on to hit a wall, exactly as it is on
    // one that opens a portal - the flight below has not resolved yet.
    //
    // IT AIMS AT finalPos, NOT AT THE TAP (#158-166: `applyImpulse(finalPos)`): a tap beyond a wall
    // pushes the shooter away from the wall's face, and a tap the head-height retry clears pushes it
    // away from the point the retry chose.
    //
    // ADDED to the velocity, never set: applyImpulse is
    // SetLinearVelocity(GetLinearVelocity() + impulse), so shots accumulate and
    // drift is the mechanic.
    //
    // The shove is scale(1.4) of the ORIGINAL's physics, which steps 1/30 s a frame; the shooter is
    // carried at velocityRatio times that velocity, so a tick of the port's 1/60 s covers the distance
    // the original's frame does (Player.hpp, clock.json physics_step_s). 140 units a second at 60 Hz,
    // where F10 measured 40-44 on an emulator at 17-19 frames a second (zerog.json).
    if (noGravity && recoilMps > 0.0) {
        registry.get<RigidBodyComponent>(shooter).velocity +=
            Zerog::Impulse(fired.fromPx, fired.toPx, recoilMps * velocityRatio);
    }
    return true;
}

// ---- computePortalFinalPos: where the tap sends the shot, and whether it is doomed ----
//
// The original works out at the tap where its shot is to go and whether it is
// already doomed, and never looks again: its projectile has no body and flies a
// straight line to that point (ETHCallback_projectile). Shoot calls this, and the
// shot flies by its answer (planning step 156, R23) - which is also the mark the tap
// leaves, red_cross for a doomed shot and portal_mark otherwise, and red_waves (A).
//
// It runs the original's ray over the PORT'S world. The bodies are the port's
// colliders, a trigger as well as a solid one, less the shooter and the bodies
// the ray's ignore list names (Find); "static" is a body with no dynamic rigid
// body (a StaticBody2D, a sensor, or a static the port moves kinematically), and
// "teleportable" is a body Tick carries through a portal (travellers). The
// fields are the zones the port plays (InField): antiportals at the original's radius
// (R1, step 127) and nothing else (R2, step 125: a well is no field; R17: an agent is none).

FinalPos State::FinalPosFrom(entt::registry& registry, const glm::dvec2& destPx, const glm::dvec2& casterPx,
                             bool hasBeenReflected) const {
    FinalPos out;
    // #5-28: normalize(destPos - casterPos) x scale(8), the ray's head start.
    // normalize of a zero vector is zero (GameMath.h:755-761).
    const glm::dvec2 towardPx = destPx - casterPx;
    const double length = glm::length(towardPx);
    const glm::dvec2 aim = length > 0.0 ? towardPx / length : glm::dvec2(0.0);
    const glm::dvec2 startPx = casterPx + aim * rules.finalPos.rayStartUnits;

    // #30-51: GetClosestContact(start, destPos, the ignore list). The shooter is
    // char_feet.ent and dark_mage.ent in the original, both on the list.
    std::vector<entt::entity> ignore = rayIgnored;
    ignore.push_back(shooter);
    const std::optional<Shot::Hit> hit = Shot::ClosestContact(registry, startPx, destPx, ignore);
    if (!hit) {
        // #73-134: nothing met. The aim point in a field dooms it and adds
        // red_waves (A) there (#81-115); anywhere else it is a clear shot.
        out.atPx = destPx;
        out.willDie = InField(destPx);
        out.redWaves = out.willDie;
        return out;
    }
    const glm::dvec2 pointPx = startPx + (destPx - startPx) * hit->along;
    out.closest = hit->body;
    const auto* rigid = registry.try_get<RigidBodyComponent>(hit->body);
    const bool isStatic = rigid == nullptr || rigid->isKinematic;
    const bool teleportable = std::find(travellers.begin(), travellers.end(), hit->body) != travellers.end();
    // #136-175: a static body, or one nothing carries through a portal, dooms an
    // unreflected shot where the ray met it.
    const bool blocking = isStatic ? true : !teleportable;
    if (blocking && !hasBeenReflected) {
        out.atPx = pointPx;
        out.willDie = true;
        return out;
    }
    out.preferred = true; // #249-253: preferedTeleportEntity = the body's id
    // #177-191: a contact inside a field dooms it too, at the contact itself -
    // the jump to #249 skips jumpForth, which is still zero.
    if (InField(pointPx)) {
        out.atPx = pointPx;
        out.willDie = true;
        return out;
    }
    // #194-248: past a body a portal may open on, short of one it may not.
    const bool pushOut = (hasBeenReflected ? isStatic : false) ? true : !teleportable;
    out.atPx = pointPx + aim * (pushOut ? -rules.finalPos.pushUnits : rules.finalPos.pushUnits);
    out.willDie = false;
    return out;
}

FinalPos State::Predict(entt::registry& registry, const glm::dvec2& casterPx, const glm::dvec2& destPx) const {
    // #66-87: the global, unreflected, from the character.
    const FinalPos first = FinalPosFrom(registry, destPx, casterPx, false);
    // #89-101: an aim inside a field is returned as it stands - which is also why
    // red_waves (A) can only come from that first call.
    if (InField(destPx)) return first;
    // #103-121: a retry when the first ray failed, or when it ended on a body a
    // portal may open on and nothing stands at the tap
    // (shouldTryToAvoidTeleportableEntity, 140931..141090 #3-24).
    const bool avoid = first.preferred && Shot::BodiesAt(registry, destPx).empty();
    if (!first.willDie && !avoid) return first;
    // #123-189: from over the character's head, if nothing is there but what the
    // first ray met - the grabber's first body is compared, in its own order.
    const glm::dvec2 overPx = casterPx + rules.finalPos.retryFromUnits;
    const std::vector<entt::entity> there = Shot::BodiesAt(registry, overPx);
    if (!there.empty() && there.front() == first.closest) return first;
    // #194-217: stepped back off its aim.
    const glm::dvec2 back = destPx - overPx;
    const double backLength = glm::length(back);
    const glm::dvec2 retryPx =
        overPx - (backLength > 0.0 ? back / backLength : glm::dvec2(0.0)) * rules.finalPos.retryBackUnits;
    const FinalPos second = FinalPosFrom(registry, destPx, retryPx, false);
    // #231-236 write the retry's hasFailed whatever it says; #242-251 keep its
    // position only when it did not fail (D3_check B).
    FinalPos out = second.willDie ? first : second;
    out.willDie = second.willDie;
    out.preferred = second.preferred;
    out.redWaves = false;
    out.retried = true;
    return out;
}

bool State::FireDiamondWillDie(double touchXScreenPx) {
    const auto bits = std::bit_cast<std::uint32_t>(static_cast<float>(touchXScreenPx));
    return (bits & 0xFFu) != 0u;
}

const NoPortalZone* State::FindZone(const std::string& name) const {
    for (const NoPortalZone& zone : zones) {
        if (zone.name == name) return &zone;
    }
    return nullptr;
}

void State::Centre(Placed& portal) {
    const glm::vec3 centre = Units::ToWorld(portal.atPx.x, portal.atPx.y);
    portal.trigger.centre = glm::vec2(centre.x, centre.y);
}

void State::Tick(entt::registry& registry, const ScriptClock::Step& step) {
    // What went through last tick has been drawn; this tick starts with none -
    // and nor has it launched, turned or lost a shot yet.
    transits.clear();
    launches.clear();
    reflected.clear();
    failures.clear();
    agentKills.clear();
    burns.clear();
    // The port's own guess, on the engine's tick: nothing in the original reads it.
    lockoutS = std::max(0.0f, lockoutS - step.physicsS);
    // The cooldown clocks run UP, as the original's two Timers do (Timer::update reads
    // the script's clock).
    const double elapsedMs = ScriptClock::Ms(step);
    sinceStartMs += elapsedMs;
    sinceTapMs += elapsedMs;

    // EVERY PLACED PORTAL'S OWN CALLBACK, before anything reads where it is
    // (ETHCallback_portal, the visuals plan's 7.4). Its first arm hovers it, and
    // its second counts the 20 seconds it has to live.
    //
    // The clock is the original's own shape: the first call WRITES ZERO and adds
    // nothing (#22-37), every later one adds the frame (#38-53), and the portal
    // goes once the total is ABOVE the limit (#65, a strict compare). What the
    // original then does is kill its particle systems and let its manager pop it
    // on the next frame; the port has one list, so it leaves it here and the
    // layer runs the death the frame it sees the id go.
    for (Placed& portal : placed) {
        Motion::Advance(rules.motion, portal.bob, elapsedMs);
        portal.atPx = portal.basePx + Motion::OffsetPx(portal.bob);
        Centre(portal);
        if (!portal.aged) portal.aged = true;
        else portal.ageMs += elapsedMs;
    }
    if (rules.perishMs > 0.0) {
        const double limit = rules.perishMs;
        placed.erase(std::remove_if(placed.begin(), placed.end(),
                                    [limit](const Placed& portal) { return portal.ageMs > limit; }),
                     placed.end());
    }

    // THE SHOT FLIES ON AS ETHCallback_projectile DOES (shot.json `flight`, planning step 149).
    //
    // Its callback runs once a frame. The first run stores where the shot is (initialPos), jumps it
    // `jumpUnits` along its direction and flashes projectile_shoot there (#14-71); every run, that
    // one included, then advances it speedUnits x min(frameCapMs, the script's milliseconds) / 1000
    // (#113-125) and asks whether it has gone FARTHER than its destiny (#127-156, a strict compare:
    // the shot overshoots, and the portal opens at the destiny, never at the shot).
    //
    // THE DESTINY IS THE TAP-TIME RAY'S finalPos (planning step 156, R23), which Shoot stored: the
    // shot has NO BODY (projectile.ent is shape 0) and meets no wall, crate, platform or placed
    // portal on its way - the ray decided all of those at the tap, and a doomed shot simply flies
    // to where the ray ended and fails there. What it does meet are the other entities' STATIC
    // callbacks, which see its position once a frame.
    //
    // WHEN, and what the other callbacks see. ETHScene::Update runs the dynamic callbacks (the
    // shot's), then `loop` (the tap and its AddEntity), then the static ones (an agent, a flame, a
    // reflector, a blocker). So on the frame a tap fires it the shot has no callback yet - it exists,
    // and the static callbacks of that frame see it at its spawn point - and its first callback is
    // the next frame's. The layer takes a tap AFTER this half of its tick (readInput follows
    // Game::AfterStep), so the first Tick after Shoot IS that next frame: no tick is added, and the
    // spawn pass of the tap's own frame, which falls between the two, is run as that Tick's first
    // pass (`Flight::fresh`). A static callback of a frame the shot's own callback ended on finds
    // nothing: DeleteEntity erases it from its bucket at once. And a static callback samples the
    // shot's POSITION once a frame, never the way between two: the spawn, then the point after the
    // jump and first advance, then each advance. The blockers, reflectors, agents and flames below
    // are all tested so (a reflector's catch is G9's, planning step 157).
    const auto flyOnePass = [&](bool callback) {
        const glm::dvec2 startPx = flight->atPx; // initialPos on the first callback
        glm::dvec2 next = startPx;
        glm::dvec2 positionPx = startPx; // GetPositionXY once the callback has advanced it: past the destiny on an arrival
        bool arrives = false;
        bool inFlight = false; // short of the end of its way when the static callbacks look
        if (callback) {
            const glm::dvec2 left = flight->toPx - startPx;
            const double length = glm::length(left);
            const glm::dvec2 heading = length > 0.0 ? left / length : glm::dvec2(0.0);
            double remaining = length;
            glm::dvec2 afterJumpPx = startPx; // where the advance begins
            if (!flight->launched) {
                // The leg's first callback, for the picture too: projectile_shoot is added after the
                // jump and before the shot moves on, so a leg that ends on this tick has flashed.
                flight->launched = true;
                launches.push_back(Launch{startPx, flight->toPx});
                afterJumpPx = startPx + heading * shot.jumpUnits;
                remaining -= shot.jumpUnits;
            }
            const double reach =
                shot.speedUnits * std::min(shot.frameCapMs, std::max(0.0, ScriptClock::Ms(step))) / 1000.0;
            arrives = reach > remaining;
            inFlight = reach < remaining;
            positionPx = afterJumpPx + heading * reach;
            next = arrives ? flight->toPx : positionPx;
        } else {
            // s = 0 < the stored squaredDistance: in flight, unless it was fired at its own spawn point.
            inFlight = glm::length(flight->toPx - startPx) > 0.0;
        }
        // What every static callback below reads is ONE sample of the shot's position: the pass's end,
        // `next` (or, with no callback, the spawn point). `first` and `sampleAlong` only say which of the
        // two samples this pass is, and a tie between two callbacks that catch the same one is settled in
        // the order below (the original's is its scene walk's, undecoded).
        const double sampleAlong = callback ? 1.0 : 0.0;
        const auto pointAt = [&](double along) { return startPx + (next - startPx) * along; };

        double first = 2.0; // along the stretch, from 0 to 1: past its end until something is met
        std::string stoppedBy;
        // AN ANTI-PROJECTILE WALL KILLS A SHOT whose position its rectangle holds when its static callback
        // looks (ETHCallback_anti_projectile_wall #1-115): isPointInRect(projectile.GetPositionXY(), the wall's
        // position, collisionBox.size x scale, V2_HALF) - a closed rectangle - for every projectile.ent, with no
        // in-flight test and no memory of the way between two frames. It sees the shot where the callback and
        // the jump left it, so a shot that crosses a box between two samples is not seen; a shot that arrives
        // is deleted before it looks; and a shot fired from inside the rectangle is killed at its spawn.
        for (const Blocker& blocker : blockers) {
            if (arrives) break;
            if (Shot::InsideBox(next, blocker.box) && sampleAlong < first) {
                stoppedBy = blocker.name;
                first = sampleAlong;
            }
        }
        // A REFLECTOR CATCHES A SHOT BY ITS POSITION (ETHCallback_reflect_agent #1-84, planning step 157): the
        // first projectile.ent in its bucket and the eight round it, if its `reflect` is set (#39-54: the
        // tap's shot has it and the one a reflector adds does not, so a shot is turned once), is caught when
        // squaredDistance(its position, the agent's) is STRICTLY below size.x x size.x (#55-84, centre to
        // centre, size.x = the sprite's 32 x the placement's scale). It is the static callback's sample, like
        // the blocker's and the agent's: the spawn on the frame the tap fires from beside a reflector, then
        // where the callback and the jump left it, then each advance - never the way between two - and never
        // a shot that has arrived, which deleted itself in its own callback before this one looked.
        int turnedBy = -1;
        if (!arrives && flight->reflections < shot.maxReflections) {
            for (std::size_t i = 0; i < reflectors.size(); ++i) {
                const Reflector& reflector = reflectors[i];
                if (Reaches(next, reflector.centrePx, shot.reflectSizeUnits * reflector.scale) && sampleAlong < first) {
                    turnedBy = static_cast<int>(i);
                    first = sampleAlong;
                    stoppedBy.clear();
                    break;
                }
            }
        }
        // AN ANTI-PORTAL AGENT KILLS A SHOT STILL IN FLIGHT whose position is within its reach when its
        // static callback looks (ETHCallback_anti_portal_agent #61-124): it names only an entity called
        // projectile.ent (#61-77), asks that the shot be short of the end of its way, squaredDistance(
        // its position, its casterPos) < its squaredDistance (#79-108, strict), and that its position be
        // within 0.7 x size.x (#110-124, strict), before the shot meets anything later on its way. It
        // sees the shot where the callback and the jump left it: on the frame the tap fires it, at the
        // player's centre (so an agent within reach of the player dies at once); on its first callback,
        // 24 + 3.2 units out and never in between; then each advance. A shot that arrives, or is
        // stopped, first is the projectile's own callback's and never reaches the agent's.
        //
        // WHERE THE AGENT IS when it looks: the position its previous frame's linearMotion wrote. The
        // callback pass is the frame after the one the agents last advanced in (Tick moves them last), so
        // it reads where they stand; the spawn pass is the tap's own frame, whose static callback ran
        // before that advance, so it reads where they stood before it.
        int killedBy = -1;
        glm::dvec2 killedAtPx{0.0};
        for (std::size_t i = 0; i < agents.size() && inFlight; ++i) {
            const Agent& agent = agents[i];
            if (agent.gone || arrives) continue;
            const glm::dvec2 agentAt = callback ? agent.CentreNowPx() : agent.BeforeLastMovePx();
            if (Reaches(next, agentAt, AgentReachPx(agent)) && sampleAlong < first) {
                killedBy = static_cast<int>(i);
                killedAtPx = agentAt;
                first = sampleAlong;
                turnedBy = -1;
                stoppedBy.clear();
            }
        }
        // A FIRE AGENT'S FLAME BURNS A SHOT whose position is within its reach (ETHCallback_fire_agent
        // #97-117, #158-177, #253-265): squaredDistance < size.x ^ 2, centre to centre, and no
        // in-flight test of its own - but the shot deletes itself in its own callback on the frame it
        // reaches the end of its way (ETHCallback_projectile #127-156, #265-270), before the static
        // agent's callback looks for it. Whichever of the two it meets first has it.
        int burnedBy = -1;
        for (std::size_t i = 0; i < flames.size() && !arrives; ++i) {
            const Flame& flame = flames[i];
            if (Reaches(next, flame.centrePx, flame.reachPx) && sampleAlong < first) {
                burnedBy = static_cast<int>(i);
                first = sampleAlong;
                killedBy = -1;
                turnedBy = -1;
                stoppedBy.clear();
            }
        }
        if (burnedBy >= 0) {
            // burnProjectile: killProjectile, then addFireball(projectilePos, projectileDir, false, 1) -
            // Game's, from this record. No portal, no failure and no sound of the shot's own.
            Burn burn;
            burn.flame = flames[static_cast<std::size_t>(burnedBy)].name;
            burn.atPx = pointAt(first);
            const glm::dvec2 leg = flight->toPx - flight->atPx;
            const double length = glm::length(leg);
            if (length > 0.0) burn.heading = leg / length;
            burns.push_back(burn);
            ++shotsBurned;
            flight.reset();
        } else if (killedBy >= 0) {
            // #126-142 both deleted, and #168-177 the ring round the agent with them. The explosion,
            // the shake and the sound (#144-166, #179-187) are the frame's, from this record.
            Agent& agent = agents[static_cast<std::size_t>(killedBy)];
            AgentKill kill;
            kill.agent = agent.name;
            kill.atPx = killedAtPx;
            kill.shotPx = pointAt(first);
            agent.gone = true;
            kill.ring = DeleteRingAt(kill.atPx);
            agentKills.push_back(kill);
            ++agentsKilled;
            flight.reset();
        } else if (turnedBy >= 0) {
            // ETHCallback_reflect_agent #92-243. The new shot leaves the REFLECTOR'S point, not where the old one
            // was caught: its heading is the old shot's stored direction (the unit vector from its casterPos to
            // its destiny, #58-66) mirrored by the plane's normal (computeReflectVector, reflect: #107-146); its
            // length is the whole length that shot was fired, or the way from where its leg began to the reflector,
            // whichever is less (#148-180: min(sqrt(squaredDistance), distance(casterPos, agent))); and newDest is
            // the agent's point that far along it (#182-196), which computePortalFinalPos then answers for as a
            // REFLECTED shot from the agent (#198-221: a static body is pushOut, 10 short, never a doom).
            const Reflector& agent = reflectors[static_cast<std::size_t>(turnedBy)];
            const glm::dvec2 caught = next; // projectilePos: the old shot's position, where its flash is (#245-260)
            const glm::dvec2 along = flight->toPx - flight->fromPx;
            const double legLength = glm::length(along); // sqrt(squaredDistance)
            const glm::dvec2 dir = legLength > 0.0 ? along / legLength : glm::dvec2(0.0);
            const glm::dvec2 gap = caught - agent.centrePx;
            const double gapLength = glm::length(gap);
            const glm::dvec2 toShot = gapLength > 0.0 ? gap / gapLength : glm::dvec2(0.0); // normalize: zero stays zero
            const glm::dvec2 newDir = Shot::Reflect(dir, Shot::ReflectNormal(agent.plane, toShot));
            const double length = std::min(legLength, glm::distance(flight->fromPx, agent.centrePx));
            const glm::dvec2 newDest = agent.centrePx + newDir * length;
            const FinalPos leg = FinalPosFrom(registry, newDest, agent.centrePx, true);
            // #224-235 addProjectile(newDest, hasFailed, newDest, agentPos, ...) spawns it AT the agent: its own
            // first callback (the next tick's: a shot a static callback adds is always one frame behind) stores
            // that as initialPos and jumps 24 from it.
            flight->fromPx = agent.centrePx;
            flight->atPx = agent.centrePx;
            flight->toPx = leg.atPx;
            flight->said = leg;
            ++flight->reflections;
            ++reflections;
            // For the pictures: the new leg's mark is at its clipped end, and the new leg is a new
            // projectile, which flashes on its own first callback.
            reflected.push_back(Reflection{caught, newDest, agent.centrePx, leg});
            flight->launched = false;
        } else if (!stoppedBy.empty()) {
            // The blocker's callback: portal_fail_in_antiportal at the position it found the shot at,
            // the failed sound and DeleteEntity (#69-115).
            const glm::dvec2 endPx = pointAt(first);
            failures.push_back(Failure{endPx, FailKind::Blocker, InField(endPx)});
            ++shotsFailed;
            lastFailure = stoppedBy;
            flight.reset();
        } else if (arrives) {
            // ETHCallback_projectile #127-272: it has gone past its destiny, so it is the shot's own callback
            // that ends it, and the tap-time ray's verdict is what happens.
            if (flight->said.willDie) {
                // #201-263: portal_fail at the destiny - or portal_fail_in_antiportal, with a shake of 5,
                // when the shot's POSITION (up to one advance past the destiny: positionPx) is in a field
                // (G4-8: the port asked the destiny) - and the failed sound; the entity is deleted.
                const bool metBody = flight->said.closest != entt::null;
                const auto* tag = metBody && registry.valid(flight->said.closest)
                                      ? registry.try_get<Supersonic::TagComponent>(flight->said.closest)
                                      : nullptr;
                failures.push_back(
                    Failure{flight->toPx, metBody ? FailKind::Body : FailKind::Refused, InField(positionPx)});
                ++shotsFailed;
                lastFailure = tag != nullptr ? tag->tag
                              : metBody      ? std::string("a body")
                                             : std::string("the tap, where no portal may open");
            } else if (!Insert(flight->toPx)) {
                // The port's own cap with recycling off (portals.json's guess): the original always recycles.
                failures.push_back(Failure{flight->toPx, FailKind::Refused, InField(positionPx)});
                ++shotsFailed;
                lastFailure = "no portal left to open";
            }
            flight.reset();
        } else {
            flight->atPx = next;
        }
    };
    if (flight && flight->fresh) {
        // The static callbacks of the frame the tap added it on, which saw it where it was spawned.
        flight->fresh = false;
        flyOnePass(false);
    }
    if (flight) flyOnePass(true);

    // THE AGENTS MOVE LAST. ETHCallback_anti_portal_agent flies no shot and tests one before its
    // linearMotion (#200-250): the agent a shot meets is where the frame before wrote it, and this
    // frame's move is the position the picture is drawn at and the next frame's shots meet. One call
    // a frame, over the script's whole milliseconds, for an agent a shot has not killed.
    for (Agent& agent : agents) {
        if (agent.moving && !agent.gone) agent.Advance(rules.motion, elapsedMs);
    }

    // An end of a traversal: a static portal by its place in `statics`, which
    // never moves, or a placed one by its id, since placed portals come and go.
    struct End {
        bool isStatic = false;
        int key = 0;
    };
    struct Entry {
        End end;
        entt::entity body;
    };

    // Every portal's entries are found before any is acted on, as a physics step
    // finds its overlaps before it reports them.
    std::vector<Entry> entries;
    const auto sweep = [&](std::vector<entt::entity>& inside, const auto& trigger, End end) {
        std::vector<entt::entity> now;
        for (const entt::entity body : travellers) {
            if (registry.valid(body) && Trigger::Overlaps(registry, body, trigger)) now.push_back(body);
        }
        for (const entt::entity body : now) {
            if (std::find(inside.begin(), inside.end(), body) == inside.end()) entries.push_back({end, body});
        }
        inside = std::move(now);
    };
    for (std::size_t i = 0; i < statics.size(); ++i) {
        if (statics[i].live) sweep(statics[i].inside, statics[i].trigger, End{true, static_cast<int>(i)});
    }
    for (Placed& portal : placed) sweep(portal.inside, portal.trigger, End{false, portal.id});

    const auto placedWithId = [this](int id) -> const Placed* {
        for (const Placed& portal : placed) {
            if (portal.id == id) return &portal;
        }
        return nullptr;
    };

    for (const Entry& entry : entries) {
        // portal_system.gd:183-191: an end a traversal before it spent leads
        // nowhere, and neither does a portal with no partner; and nothing goes
        // through while the lockout runs (below, after the platform gate).
        std::optional<End> exit;
        if (entry.end.isStatic) {
            const Static& from = statics[static_cast<std::size_t>(entry.end.key)];
            if (!from.live) continue;
            // By destiny first (:216-223). A static portal that names itself
            // leads nowhere (:189), and a spent one is no partner.
            bool named = false;
            for (std::size_t i = 0; i < statics.size() && from.hasDestiny; ++i) {
                if (statics[i].index != from.destiny) continue;
                named = true;
                if (static_cast<int>(i) != entry.end.key && statics[i].live) exit = End{true, static_cast<int>(i)};
                break;
            }
            // Then the placed set (:224-227), when no static portal has the index.
            if (!named && !placed.empty()) exit = End{false, placed.front().id};
        } else {
            if (placedWithId(entry.end.key) == nullptr) continue;
            // Placed portals pair with each other (:208-214): with a pair, the other.
            for (const Placed& portal : placed) {
                if (portal.id != entry.end.key) {
                    exit = End{false, portal.id};
                    break;
                }
            }
            // With no second placement to pair with, the original sends the
            // traveller to the level's first portal_static - but only where the
            // level grants exactly one placement. PortalManager::doTeleporting
            // (bytes 139692..140259) takes the pair above when maxPortals is 2,
            // and teleportToFirstStaticPortal (bytes 145999..146550) when it is
            // 1; that one resolves its exit by SeekEntity('portal_static'), so
            // it is the first in the level's own order. Level 1-2 is the case:
            // one static portal, one placement, and the placement's partner is
            // the static. Without this the player walks through their own
            // portal, which is how this was found - by playing it.
            if (!exit && budget == 1) {
                for (std::size_t i = 0; i < statics.size(); ++i) {
                    if (!statics[i].live) continue;
                    exit = End{true, static_cast<int>(i)};
                    break;
                }
            }
        }
        if (!exit) continue;

        // THE PLATFORM GATE (planning step 136): teleportToOther, the placed pair's
        // one path, teleports nothing while a moving platform or a lift is at any
        // placed portal (#36-42), before it asks whether the end is active (#43-50),
        // which the lockout stands in for. The original asks again every frame the
        // traveller is still in its portal, so a held entry is taken back out of the
        // portal's `inside` and offered again on the next tick. A static end's
        // paths never ask (ETHCallback_portal_static, teleportToFirstStaticPortal).
        if (!entry.end.isStatic && !exit->isStatic && PlatformInAnyPortal(registry)) {
            for (Placed& portal : placed) {
                if (portal.id != entry.end.key) continue;
                portal.inside.erase(std::remove(portal.inside.begin(), portal.inside.end(), entry.body),
                                    portal.inside.end());
            }
            ++transitsHeld;
            continue;
        }
        if (lockoutS > 0.0f) continue;

        const glm::dvec2 exitPx =
            exit->isStatic ? statics[static_cast<std::size_t>(exit->key)].atPx : placedWithId(exit->key)->atPx;
        // The two points the original's vanishEffect is given, READ BEFORE the
        // traveller is moved: the end it went in by, and where it stood. The exit
        // is the PORTAL's own point and not where the body lands, which
        // Portal::ExitPosition may push off it - appearEffect is called with the
        // portal (teleportToOther #71-75).
        const glm::dvec2 entryPx = entry.end.isStatic
                                       ? statics[static_cast<std::size_t>(entry.end.key)].atPx
                                       : placedWithId(entry.end.key)->atPx;
        const glm::dvec2 travellerPx = Units::ToPixels(registry.get<TransformComponent>(entry.body).position);
        Teleport(registry, entry.body, exitPx, rules.transit, entry.body == shooter);
        transits.push_back(Traversal{travellerPx, entryPx, exitPx});
        ++traversals;
        // A placed pair carried something, so neither end is refunded if it is
        // cleared later. Only the pair: teleportToOther is the one route that
        // sets the flag, and a placement whose exit is the level's first static
        // portal goes through teleportToFirstStaticPortal instead.
        if (!entry.end.isStatic && !exit->isStatic) {
            for (Placed& portal : placed) {
                if (portal.id == entry.end.key || portal.id == exit->key) portal.teleported = true;
            }
        }
        lockoutS = static_cast<float>(rules.transit.reentryLockoutS);

        // Both ends of a pair go (:270-282), except a static end, which stays
        // unless portals.json says static portals do not persist.
        if (!rules.consumeOnTraverse) continue;
        for (const End& end : {entry.end, *exit}) {
            if (end.isStatic) {
                if (!rules.staticPortalsPersist) statics[static_cast<std::size_t>(end.key)].live = false;
            } else {
                placed.erase(std::remove_if(placed.begin(), placed.end(),
                                            [&end](const Placed& portal) { return portal.id == end.key; }),
                             placed.end());
            }
        }
    }
}

int State::KillAll(bool refund) {
    const int killed = static_cast<int>(placed.size());
    if (refund) {
        for (const Placed& portal : placed) {
            if (!portal.teleported && portalsUsed > 0) --portalsUsed;
        }
    }
    placed.clear();
    return killed;
}

std::optional<glm::dvec2> State::CarryFireball(entt::registry& registry, const glm::dvec2& atPx, double grabPx) {
    // getOther: with fewer than two placed there is no partner, and teleportToOther
    // does nothing (#15-24).
    if (placed.size() < 2) return std::nullopt;
    // EntityGrabber's test for a circle body (#65-105): closer than the two radii
    // together, strictly.
    const double reach = grabPx + rules.entryRadiusPx;
    for (std::size_t i = 0; i < placed.size(); ++i) {
        const glm::dvec2 gap = atPx - placed[i].atPx;
        if (gap.x * gap.x + gap.y * gap.y >= reach * reach) continue;
        // teleportToOther's platform gate (#36-42), after mayTeleport - which the
        // caller has asked - and before anything moves: held, and asked again on
        // the next tick while the fireball is still in reach.
        if (PlatformInAnyPortal(registry)) {
            ++transitsHeld;
            return std::nullopt;
        }
        // The pair is the first two placed, which is every pair a budget of two
        // holds: getOther returns the first portal that is not this one.
        const std::size_t other = i == 0 ? 1 : 0;
        const glm::dvec2 exitPx = placed[other].atPx;
        // vanishEffect with where it stood and the end it went in by, appearEffect
        // with the end it comes out of (#52-75), and doTeleporting's sound.
        transits.push_back(Traversal{atPx, placed[i].atPx, exitPx});
        ++traversals;
        placed[i].teleported = true;
        placed[other].teleported = true;
        return exitPx;
    }
    return std::nullopt;
}

bool Find(const Tscn::Scene& scene, const Roles::Table& roles, const LevelBuilder::Built& built,
          entt::registry& registry, entt::entity player, const Rules& rules, const Mover::Rules& movers,
          const Shot::Rules& shot, State& out, std::string& error) {
    out = State{};
    out.rules = rules;
    out.shot = shot;
    out.shooter = player;
    int maxPortals = rules.defaultMaxPortals;
    if (player != entt::null) out.travellers.push_back(player);

    for (const Tscn::Node& node : scene.nodes) {
        if (node.parent != ".") continue;
        // For the pictures a tap leaves (the visuals plan's 10.1): the bodies the
        // tap-time ray passes through, named exactly as the original's list
        // spells them, and the sign a budget-0 tap looks for, by its name.
        const std::string entityName = Roles::EntityName(node);
        if (std::find(rules.finalPos.rayIgnores.begin(), rules.finalPos.rayIgnores.end(), entityName) !=
            rules.finalPos.rayIgnores.end()) {
            if (const auto found = built.entities.find(node.name); found != built.entities.end()) {
                out.rayIgnored.push_back(found->second);
            }
        }
        if (!rules.noPortalSignEntity.empty() && entityName == rules.noPortalSignEntity) out.noPortalSign = true;
        // What the platform gate looks for, by the name isMovingPlatform compares
        // (Rules::gate): its collision box about its own point. EntityGrabber turns
        // the box's sides with the entity but not its corner planes, and takes the
        // box unscaled, so a turned or scaled one is refused rather than guessed at;
        // none of the game's 29 is either. Not built when the statics are left out.
        if (std::find(rules.gate.entities.begin(), rules.gate.entities.end(), entityName) !=
            rules.gate.entities.end()) {
            double angle = 0.0;
            const Tscn::Value* rotation = node.Find("rotation");
            if ((rotation != nullptr && rotation->AsNumber(angle) && angle != 0.0) || node.Find("scale") != nullptr) {
                error = node.name + ": a moving platform or lift that is turned or scaled, which the gate does not model";
                return false;
            }
            glm::dvec2 offsetPx(0.0);
            glm::dvec2 sizePx(0.0);
            if (!LevelBuilder::ShapeBoundsPx(scene, node, offsetPx, sizePx)) {
                error = node.name + ": a moving platform or lift with no collision shape";
                return false;
            }
            if (const auto found = built.entities.find(node.name); found != built.entities.end()) {
                out.gateBodies.push_back(GateBody{node.name, found->second, sizePx * 0.5});
            }
        }
        const std::string role = Roles::RoleOf(roles, node);
        if (role == Roles::kStaticPortal) {
            // portal_system.gd:79-87. An inactive one is not a portal, and one with
            // no index cannot be named, so the Godot project passes both over.
            double active = 1.0;
            if (const Tscn::Value* value = node.Meta("active"); value != nullptr && !value->AsNumber(active)) {
                error = node.name + "'s active is not a number";
                return false;
            }
            const Tscn::Value* index = node.Meta("index");
            if (active == 0.0 || index == nullptr) continue;
            Static portal;
            portal.name = node.name;
            double number = 0.0;
            if (!index->AsNumber(number)) {
                error = node.name + "'s index is not a number";
                return false;
            }
            portal.index = static_cast<int>(number);
            if (const Tscn::Value* destiny = node.Meta("destiny")) {
                if (!destiny->AsNumber(number)) {
                    error = node.name + "'s destiny is not a number";
                    return false;
                }
                portal.destiny = static_cast<int>(number);
                portal.hasDestiny = true;
            }
            if (const Tscn::Value* colour = node.Meta("color");
                colour != nullptr && colour->kind == Tscn::Value::Kind::String) {
                portal.colour = colour->text;
            }
            const Tscn::Value* position = node.Find("position");
            if (position == nullptr || position->kind != Tscn::Value::Kind::Vector2) {
                error = node.name + " has no position";
                return false;
            }
            portal.atPx = glm::dvec2(position->numbers[0], position->numbers[1]);
            if (!Trigger::FromNode(node, portal.trigger, error)) return false;
            out.statics.push_back(std::move(portal));
            continue;
        }
        if (role == Roles::kProjectileBlocker) {
            Blocker blocker;
            blocker.name = node.name;
            if (!Trigger::FromNode(node, blocker.box, error)) return false;
            out.blockers.push_back(blocker);
            continue;
        }
        if (role == Roles::kReflector) {
            Reflector reflector;
            reflector.name = node.name;
            const Tscn::Value* position = node.Find("position");
            const Tscn::Value* plane = node.Meta("plane");
            if (position == nullptr || position->kind != Tscn::Value::Kind::Vector2 ||
                (plane != nullptr && (plane->kind != Tscn::Value::Kind::String ||
                                      (plane->text != "vertical" && plane->text != "horizontal")))) {
                error = node.name + " needs a position, and a plane that is vertical or horizontal";
                return false;
            }
            reflector.centrePx = glm::dvec2(position->numbers[0], position->numbers[1]);
            // computeReflectVector reads GetString('plane'): '' is a placement that gives none (2-27's
            // reflect_agent_ent_961, the handle's), 'vertical' is a vertical mirror and any other string the
            // horizontal arm (the check above admits only the two the levels use).
            reflector.plane = plane == nullptr            ? Shot::Plane::Snap
                              : plane->text == "vertical" ? Shot::Plane::Vertical
                                                          : Shot::Plane::Horizontal;
            if (const Tscn::Value* scale = node.Meta("scale"); scale != nullptr && !scale->AsNumber(reflector.scale)) {
                error = node.name + "'s scale is not a number";
                return false;
            }
            out.reflectors.push_back(reflector);
            continue;
        }
        if (role == Roles::kLevelProperties) {
            // An absent max_portals is the default; an explicit 0 grants none.
            if (const Tscn::Value* value = node.Meta("max_portals")) {
                double number = 0.0;
                if (!value->AsNumber(number)) {
                    error = node.name + "'s max_portals is not a number";
                    return false;
                }
                maxPortals = static_cast<int>(number);
            }
        } else if (role == Roles::kAntiPortalAgent) {
            // No zone: it refuses nothing, and its own reach is the shot's (Tick).
            Agent agent;
            agent.name = node.name;
            if (const Tscn::Value* position = node.Find("position");
                position != nullptr && position->kind == Tscn::Value::Kind::Vector2) {
                agent.centrePx = glm::dvec2(position->numbers[0], position->numbers[1]);
            }
            if (const Tscn::Value* scale = node.Meta("scale"); scale != nullptr && !scale->AsNumber(agent.scale)) {
                error = node.name + "'s scale is not a number";
                return false;
            }
            // ETHCallback_anti_portal_agent #200-250: linearMotion where the node has a speed, its axis
            // turned 90 degrees where its `direction` is "horizontal" (movers.json linear_motion.agent).
            if (node.Meta("speed") != nullptr && node.Meta("stride") != nullptr) {
                const Tscn::Value* direction = node.Meta("direction");
                const std::string heading =
                    direction != nullptr && direction->kind == Tscn::Value::Kind::String ? direction->text : "";
                agent.moving = true;
                if (!Mover::OscillationFromNode(node, movers.AgentCall(heading), agent.motion, error)) return false;
            }
            out.agents.push_back(agent);
        } else if (role == Roles::kNoPortalZone) {
            NoPortalZone zone;
            zone.name = node.name;
            zone.entity = entityName;
            if (const Tscn::Value* position = node.Find("position");
                position != nullptr && position->kind == Tscn::Value::Kind::Vector2) {
                zone.centrePx = glm::dvec2(position->numbers[0], position->numbers[1]);
            }
            if (const Tscn::Value* scale = node.Meta("scale"); scale != nullptr && !scale->AsNumber(zone.scale)) {
                error = node.name + "'s scale is not a number";
                return false;
            }
            // A ring stands where the level places it: no antiportal callback moves it, whatever speed
            // and stride its node carries (none does).
            out.zones.push_back(zone);
        }

        // portal_system.gd:194-203: a body travels when its entity says so.
        double teleportable = 0.0;
        const Tscn::Value* flag = node.Meta("teleportable");
        if (flag == nullptr || !flag->AsNumber(teleportable) || teleportable == 0.0) continue;
        const auto found = built.entities.find(node.name);
        if (found == built.entities.end()) continue;
        const auto* rigid = registry.try_get<RigidBodyComponent>(found->second);
        if (rigid != nullptr && !rigid->isKinematic) out.travellers.push_back(found->second);
    }
    out.budget = std::min(maxPortals, kPairSize);
    return true;
}

} // namespace MagicPortals::Portals
