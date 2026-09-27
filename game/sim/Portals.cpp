#include "sim/Portals.hpp"

#include "core/Components.hpp"
#include "core/Json.hpp"
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

// portal_system.gd:230-261, in the remake's space: pixels, +y down. Placed
// portals are never turned, so rotate_to_exit would turn through nothing here.
// The port plays the original's invert (transit.json): the traveller comes out
// at the exit itself, turned back, and the player in y only.
void Teleport(entt::registry& registry, entt::entity body, const glm::dvec2& exitPx, const Portal::Transit& transit,
              bool character) {
    auto& transform = registry.get<TransformComponent>(body);
    auto& rigid = registry.get<RigidBodyComponent>(body);
    const glm::dvec2 velocityPx(rigid.velocity.x * Units::kPixelsPerMetre, -rigid.velocity.y * Units::kPixelsPerMetre);
    const glm::dvec2 exitVelocityPx = Portal::ExitVelocity(velocityPx, 0.0, 0.0, transit, character);
    const glm::dvec2 destinationPx = Portal::ExitPosition(exitPx, exitVelocityPx, transit);
    const glm::vec3 destination = Units::ToWorld(destinationPx.x, destinationPx.y);
    transform.position = glm::vec3(destination.x, destination.y, transform.position.z);
    rigid.velocity =
        glm::vec3(Units::ToMetres(exitVelocityPx.x), Units::ToMetres(-exitVelocityPx.y), rigid.velocity.z);
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
    // READ AND CHECKED, then dropped. The remake marks its placement block
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
    // Zero is allowed - it means no cooldown at all, which is what the remake
    // carried for the first one - but a negative is a file saying nothing
    // sensible.
    if (firstMs < 0.0 || nextMs < 0.0) {
        error = path + ": neither cooldown is below zero";
        return false;
    }
    out.firstPortalMinMs = firstMs;
    out.nextPortalMinMs = nextMs;

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
        !finalPos["retry_from_units"].IsArray() || finalPos["retry_from_units"].AsArray().size() != 2 ||
        !finalPos["retry_from_units"].AsArray()[0].IsNumber() || !finalPos["retry_from_units"].AsArray()[1].IsNumber()) {
        error = path + ": final_pos needs ray_start_units, ray_ignores, push_units, retry_from_units [x, y] and "
                       "retry_back_units";
        return false;
    }
    Rules::FinalPosRules readFinal;
    readFinal.rayStartUnits = finalPos["ray_start_units"].AsNumber(-1.0);
    readFinal.pushUnits = finalPos["push_units"].AsNumber(-1.0);
    readFinal.retryBackUnits = finalPos["retry_back_units"].AsNumber(-1.0);
    readFinal.retryFromUnits = glm::dvec2(finalPos["retry_from_units"].AsArray()[0].AsNumber(0.0),
                                          finalPos["retry_from_units"].AsArray()[1].AsNumber(0.0));
    if (!(readFinal.rayStartUnits > 0.0) || !(readFinal.pushUnits > 0.0) || !(readFinal.retryBackUnits > 0.0) ||
        readFinal.retryFromUnits == glm::dvec2(0.0)) {
        error = path + ": final_pos's distances are each above zero, and its retry starts somewhere else";
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

    out.entryRadiusPx = entry;
    out.antiportalRadiusPx = field;
    out.antiportalEntity = managed;
    out.antiportalManagerScale = managerScale;
    out.finalPos = readFinal;
    out.noPortalSignEntity = noBudget["sign_entity"].AsString();
    return true;
}

bool State::InField(const glm::dvec2& atPx) const {
    for (const NoPortalZone& zone : zones) {
        // The ANTIPORTAL's radius, which is half its own sprite times the node's
        // scale - not the portal's own 14. placement.json has the decode.
        //
        // A zone carrying its OWN radius uses that instead: a gravity well's
        // antiportal is sized from the agent's radius rather than from
        // white_ring.png, so no constant here could express it.
        const double reachPx = zone.radiusPx > 0.0 ? zone.radiusPx : rules.antiportalRadiusPx * zone.scale;
        if (glm::distance(atPx, zone.CentreNowPx()) <= reachPx) return true;
    }
    return false;
}

bool State::TryPlace(const glm::dvec2& atPx) {
    if (budget <= 0) return false;
    if (InField(atPx)) return false;
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

bool State::Shoot(entt::registry& registry, const glm::dvec2& atPx) {
    // The two clocks' verdicts, asked first so that the record below can give
    // the original's reason whichever of the port's tests refuses the tap. Only
    // the record reads them before the port's own order does, and they change
    // nothing.
    const bool tooEarly = rules.firstPortalMinMs > 0.0 && sinceStartMs < rules.firstPortalMinMs;
    const bool tooSoon = rules.nextPortalMinMs > 0.0 && sinceTapMs <= rules.nextPortalMinMs;
    lastTap = Tap{};
    lastTap.atPx = atPx;
    if (budget <= 0 || flight || shooter == entt::null || !registry.valid(shooter)) {
        // The original's order (Tap): update's first gate, then the budget, then
        // the 400 ms, and the hourglass last.
        if (shooter == entt::null || !registry.valid(shooter)) lastTap.reason = TapReason::NoShooter;
        else if (tooEarly) lastTap.reason = TapReason::TooEarly;
        else if (budget <= 0) lastTap.reason = TapReason::NoBudget;
        else if (tooSoon) lastTap.reason = TapReason::Cooldown;
        else lastTap.reason = TapReason::InFlight;
        return false;
    }

    // THE COOLDOWNS, and they gate the TAP rather than the placement.
    //
    // The original holds a whole level's taps off for FIRST_PORTAL_MIN_TIME -
    // PortalManager::update skips managePortalInsertion entirely until its
    // gameTimer passes it - and then holds each tap off for
    // NEXT_PORTAL_MIN_TIME since the last one it took.
    //
    // Here rather than in TryPlace on purpose. TryPlace is what an arriving shot
    // calls AND what the suites call to put a portal where a test needs one; a
    // clock there would refuse them for a reason none of them is about. The
    // layer's only tap path is this function (readInput), so this is the seam
    // the original gates.
    //
    // The comparisons are the original's own, and they differ: update refuses
    // while gameTimer is BELOW FIRST_PORTAL_MIN_TIME (a strict less-than), while
    // managePortalInsertion proceeds only when lastPortalTimer is ABOVE
    // NEXT_PORTAL_MIN_TIME - so it refuses on equal.
    // ZERO MEANS OFF, stated rather than implied. sinceTapMs starts at zero, so
    // a bare `sinceTapMs <= nextPortalMinMs` refuses the very first tap even
    // when the cooldown is nothing - which silently disarmed the suites that
    // turn it off and cost a whole build to find. (tooEarly and tooSoon are
    // worked out at the top, for the record.)
    if (tooEarly || tooSoon) {
        lastTap.reason = tooEarly ? TapReason::TooEarly : TapReason::Cooldown;
        return false;
    }
    lastTap.reason = TapReason::Fired;

    // Taken, so it is spent - even if the shot goes on to hit a wall and open
    // nothing. The original resets lastPortalTimer before it has any idea
    // whether the shot will land. placement.json says why that does not
    // contradict "a failed shot costs nothing", which is about the budget.
    sinceTapMs = 0.0;
    Flight fired;
    fired.fromPx = Units::ToPixels(registry.get<TransformComponent>(shooter).position);
    fired.toPx = atPx;
    fired.atPx = fired.fromPx;
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
    // ADDED to the velocity, never set: applyImpulse is
    // SetLinearVelocity(GetLinearVelocity() + impulse), so shots accumulate and
    // drift is the mechanic.
    if (noGravity && recoilMps > 0.0) {
        registry.get<RigidBodyComponent>(shooter).velocity += Zerog::Impulse(fired.fromPx, fired.toPx, recoilMps);
    }
    return true;
}

// ---- computePortalFinalPos, for the pictures (the visuals plan's 10.1) --------
//
// The original works out at the tap where its shot is to go and whether it is
// already doomed, and never looks again: its projectile has no body and flies a
// straight line to that point. The port's shot flies and ends by Tick, which
// this does not touch. What this is read for is the mark the tap leaves -
// red_cross for a doomed shot, portal_mark otherwise - and red_waves (A), both of
// them pictures: the owner's standing ruling R15.
//
// It runs the original's ray over the PORT'S world. The bodies are the port's
// colliders, a trigger as well as a solid one, less the shooter and the bodies
// the ray's ignore list names (Find); "static" is a body with no dynamic rigid
// body (a StaticBody2D, a sensor, or a static the port moves kinematically), and
// "teleportable" is a body Tick carries through a portal (travellers). The
// fields are the zones the port plays (InField), so a tap's picture tells the
// truth about the port's own refusal - R1's radius and R2's wells not being
// built, the original's fields are not the port's.

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

void State::Tick(entt::registry& registry, float dt) {
    // What went through last tick has been drawn; this tick starts with none -
    // and nor has it launched, turned or lost a shot yet.
    transits.clear();
    launches.clear();
    reflected.clear();
    failures.clear();
    lockoutS = std::max(0.0f, lockoutS - dt);
    // The cooldown clocks run UP, as the original's two Timers do.
    const double elapsedMs = static_cast<double>(dt) * 1000.0;
    sinceStartMs += elapsedMs;
    sinceTapMs += elapsedMs;
    for (NoPortalZone& zone : zones) {
        if (zone.moving) zone.motion.t += dt;
    }

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

    // The shot flies on, through this tick's stretch of its way, against the
    // world as the step left it. What the stretch meets first decides: a body or
    // a blocker ends it, and a reflector turns it, mirrored across its plane, for
    // the rest of its way. Reaching the end of its way opens a portal there, or
    // fails where none may open.
    if (flight) {
        // The leg's first tick, for the picture: ETHCallback_projectile's first
        // frame flashes projectile_shoot before it moves the shot at all, so a
        // leg that ends on this very tick has flashed too.
        if (!flight->launched) {
            flight->launched = true;
            launches.push_back(Launch{flight->atPx, flight->toPx});
        }
        const glm::dvec2 left = flight->toPx - flight->atPx;
        const double remaining = glm::length(left);
        const double reach = shot.speedPx * static_cast<double>(dt);
        const bool arrives = remaining <= reach;
        const glm::dvec2 next = arrives ? flight->toPx : flight->atPx + left * (reach / remaining);
        double first = 2.0; // along the stretch, from 0 to 1: past its end until something is met
        std::string stoppedBy;
        FailKind stopKind = FailKind::Body; // what stoppedBy is, for the picture
        if (const auto hit = Shot::FirstBody(registry, flight->atPx, next, shooter)) {
            const auto* tag = registry.try_get<Supersonic::TagComponent>(hit->body);
            stoppedBy = tag != nullptr ? tag->tag : std::string("a body");
            first = hit->along;
        }
        for (const Blocker& blocker : blockers) {
            if (const auto along = Shot::Enters(flight->atPx, next, blocker.box); along && *along < first) {
                stoppedBy = blocker.name;
                first = *along;
                stopKind = FailKind::Blocker;
            }
        }
        int turnedBy = -1;
        if (flight->reflections < shot.maxReflections) {
            for (std::size_t i = 0; i < reflectors.size(); ++i) {
                if (static_cast<int>(i) == flight->lastReflector) continue;
                const auto along =
                    Shot::EntersCircle(flight->atPx, next, reflectors[i].centrePx, shot.reflectRadiusPx);
                if (along && *along < first) {
                    turnedBy = static_cast<int>(i);
                    first = *along;
                }
            }
        }
        if (turnedBy >= 0) {
            // Off the reflector where the shot reached it, mirrored, for the rest
            // of its way.
            const glm::dvec2 at = flight->atPx + (next - flight->atPx) * first;
            glm::dvec2 onward = flight->toPx - at;
            if (reflectors[static_cast<std::size_t>(turnedBy)].vertical) {
                onward.x = -onward.x;
            } else {
                onward.y = -onward.y;
            }
            flight->atPx = at;
            flight->toPx = at + onward;
            flight->lastReflector = turnedBy;
            ++flight->reflections;
            ++reflections;
            // For the pictures: ETHCallback_reflect_agent works the new leg out as
            // a reflected shot and marks its end, and the new leg is a new
            // projectile, which flashes on its own first tick.
            reflected.push_back(Reflection{at, flight->toPx, FinalPosFrom(registry, flight->toPx, at, true)});
            flight->launched = false;
        } else {
            bool refused = false;
            if (stoppedBy.empty() && arrives && !TryPlace(flight->toPx)) {
                stoppedBy = "the tap, where no portal may open";
                refused = true;
            }
            if (!stoppedBy.empty()) {
                // Where it ended, for the picture: the point it met a body or a
                // blocker's box at, or the tap it could open nothing at.
                const glm::dvec2 endPx = refused ? flight->toPx : flight->atPx + (next - flight->atPx) * first;
                failures.push_back(Failure{endPx, refused ? FailKind::Refused : stopKind, InField(endPx)});
                ++shotsFailed;
                lastFailure = stoppedBy;
                flight.reset();
            } else if (arrives) {
                flight.reset();
            } else {
                flight->atPx = next;
            }
        }
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
        // portal_system.gd:183-191: nothing goes through while the lockout runs,
        // an end a traversal before it spent leads nowhere, and neither does a
        // portal with no partner.
        if (lockoutS > 0.0f) continue;
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
        const std::string role = Roles::RoleOf(roles, node);
        if (role == Roles::kStaticPortal) {
            // portal_system.gd:79-87. An inactive one is not a portal, and one with
            // no index cannot be named, so the remake passes both over.
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
            // One placement in the game gives no plane (level26a's); it takes
            // shot.json's default, the remake's reading.
            reflector.vertical = plane != nullptr ? plane->text == "vertical" : shot.defaultPlaneVertical;
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
        } else if (role == Roles::kNoPortalZone) {
            NoPortalZone zone;
            zone.name = node.name;
            if (const Tscn::Value* position = node.Find("position");
                position != nullptr && position->kind == Tscn::Value::Kind::Vector2) {
                zone.centrePx = glm::dvec2(position->numbers[0], position->numbers[1]);
            }
            if (const Tscn::Value* scale = node.Meta("scale"); scale != nullptr && !scale->AsNumber(zone.scale)) {
                error = node.name + "'s scale is not a number";
                return false;
            }
            // Only a zone with both a speed and a stride patrols; a plain antiportal
            // stands still (behaviours.gd:48-51).
            if (node.Meta("speed") != nullptr && node.Meta("stride") != nullptr) {
                const Tscn::Value* direction = node.Meta("direction");
                const bool sideways = direction != nullptr && direction->kind == Tscn::Value::Kind::String &&
                                      direction->text == "horizontal";
                zone.moving = true;
                if (!Mover::OscillationFromNode(node, sideways ? glm::dvec2(1.0, 0.0) : glm::dvec2(0.0, 1.0),
                                                movers.oscillationRateScale, zone.motion, error)) {
                    return false;
                }
            }
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
