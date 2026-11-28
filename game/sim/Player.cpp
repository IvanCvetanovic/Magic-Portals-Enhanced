#include "sim/Player.hpp"

#include "core/Components.hpp"
#include "core/Json.hpp"
#include "core/PhysicsSystem.hpp"
#include "sim/Impacts.hpp"
#include "sim/LevelBuilder.hpp"
#include "sim/Shot.hpp"
#include "sim/Units.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <optional>
#include <sstream>
#include <vector>

namespace MagicPortals::Player {

using Supersonic::CapsuleColliderComponent;
using Supersonic::RigidBodyComponent;
using Supersonic::TagComponent;
using Supersonic::TransformComponent;

namespace {

// The spellings of movement.air_brake_form: the one table the loader reads and its refusal quotes.
struct AirBrakeFormName {
    const char* name;
    AirBrakeForm form;
};
constexpr AirBrakeFormName kAirBrakeForms[] = {
    {"tick", AirBrakeForm::Tick},
    {"travel", AirBrakeForm::Travel},
    {"root", AirBrakeForm::Root},
};

// movement.ground_lag_frames is refused above this many frames: the original's own lag is two, and a flag older than eight frames is
// not a reading the steering could be playing with.
constexpr double kMaxGroundLagFrames = 8.0;

} // namespace

bool LoadTuning(const std::string& path, Tuning& out, std::string& error) {
    namespace Json = Supersonic::Json;
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

    const struct {
        const char* section;
        const char* key;
        double Tuning::*field;
    } fields[] = {
        {"movement", "speed", &Tuning::speed},
        {"movement", "elapsed_cap_ms", &Tuning::elapsedCapMs},
        {"movement", "air_decay", &Tuning::airDecay},
        {"movement", "air_decay_fps", &Tuning::airDecayFps},
        {"gravity", "world_m_s2", &Tuning::worldGravity},
        {"gravity", "scale", &Tuning::gravityScale},
        {"gravity", "max_translation_m", &Tuning::maxTranslationM},
        {"gravity", "reference_px_per_unit", &Tuning::referencePxPerUnit},
        {"body", "box_width_px", &Tuning::boxWidthPx},
        {"body", "box_height_px", &Tuning::boxHeightPx},
        {"body", "circle_radius_share", &Tuning::circleRadiusShare},
        {"body", "circle_pos_share", &Tuning::circlePosShare},
        {"body", "width_px", &Tuning::widthPx},
        {"body", "height_px", &Tuning::heightPx},
        {"feet", "width_px", &Tuning::feetWidthPx},
        {"feet", "height_px", &Tuning::feetHeightPx},
        {"feet", "chamfer", &Tuning::feetChamfer},
        {"feet", "offset_box_share", &Tuning::feetOffsetShare},
        {"feet", "touch_px", &Tuning::feetTouchPx},
        {"hold", "friction", &Tuning::holdFriction},
        {"hold", "probe_offset_px", &Tuning::holdProbeOffsetPx},
        {"hold", "probe_reach_px", &Tuning::holdProbeReachPx},
        {"hold", "probe_line_tol_px", &Tuning::holdProbeLineTolPx},
    };
    Tuning read;
    for (const auto& field : fields) {
        const Json::Value& value = root[field.section][field.key];
        if (!value.IsNumber()) {
            error = path + ": " + field.section + "." + field.key + " is not a number";
            return false;
        }
        // A walk of nothing, a world without gravity or a rate of nothing is a file that
        // describes another game: the weightless levels take their gravity from zerog.json,
        // never from a zero here.
        if (!(value.AsNumber() > 0.0)) {
            error = path + ": " + field.section + "." + field.key + " is above zero";
            return false;
        }
        read.*(field.field) = value.AsNumber();
    }
    // gravity.clamp_follows_screen (planning step 283): optional, absent is false. One that is there but is not true or false is refused.
    if (root["gravity"].Has("clamp_follows_screen")) {
        const Json::Value& follows = root["gravity"]["clamp_follows_screen"];
        if (!follows.IsBool()) {
            error = path + ": gravity.clamp_follows_screen is not true or false";
            return false;
        }
        read.clampFollowsScreen = follows.AsBool();
    }
    // movement.air_decay_rate_hz (planning step 213): optional since the pace knob. A file with it evaluates the air brake's formula at that
    // rate; a file without it leaves the rate to the clock (AirShare reads Step::fps), which is what lets the shipped data drop the key.
    // One that is there but is a rate of nothing, or not a number, is refused rather than read as "none given".
    if (root["movement"].Has("air_decay_rate_hz")) {
        const Json::Value& rateHz = root["movement"]["air_decay_rate_hz"];
        if (!rateHz.IsNumber() || !(rateHz.AsNumber() > 0.0)) {
            error = path + ": movement.air_decay_rate_hz is not a number above zero";
            return false;
        }
        read.airRateHz = rateHz.AsNumber();
    }
    // movement.air_decay_rate_hz_by_level (planning step 252): optional, a level's name to the rate its air brake is evaluated at. A
    // file without it plays every level at air_decay_rate_hz; one that names a level a rate of nothing, or a name of nothing, is
    // refused rather than read as "no brake" or as a level that cannot be found.
    if (root["movement"].Has("air_decay_rate_hz_by_level")) {
        const Json::Value& byLevel = root["movement"]["air_decay_rate_hz_by_level"];
        if (!byLevel.IsObject()) {
            error = path + ": movement.air_decay_rate_hz_by_level is not an object";
            return false;
        }
        for (const std::string& level : byLevel.OrderedKeys()) {
            const Json::Value& rate = byLevel[level];
            if (level.empty() || level.front() == '_') {
                error = path + ": movement.air_decay_rate_hz_by_level names a level '" + level + "'; a comment belongs in the key beside the table";
                return false;
            }
            if (!rate.IsNumber() || !(rate.AsNumber() > 0.0)) {
                error = path + ": movement.air_decay_rate_hz_by_level." + level + " is not a number above zero";
                return false;
            }
            read.airRateByLevel[level] = rate.AsNumber();
        }
    }
    // movement.air_brake_form: optional, how the air brake's share is spread over the ticks of a frame (AirBrakeForm). Absent is Tick, the
    // share as it stands; a spelling that is not in kAirBrakeForms, or a value that is not a string, is refused naming the key.
    if (root["movement"].Has("air_brake_form")) {
        const std::string given = root["movement"]["air_brake_form"].AsString();
        bool known = false;
        for (const AirBrakeFormName& entry : kAirBrakeForms) {
            if (given == entry.name) {
                read.airBrakeForm = entry.form;
                known = true;
            }
        }
        if (!known) {
            std::string names;
            for (const AirBrakeFormName& entry : kAirBrakeForms) names += std::string(names.empty() ? "" : ", ") + entry.name;
            error = path + ": movement.air_brake_form is not one of " + names;
            return false;
        }
    }
    // movement.ground_lag_frames: optional, read outside the table above because that table refuses a zero and a lag of nothing is the
    // flag as it stands, the shipped value. From 0 to kMaxGroundLagFrames, a fraction allowed (FrameDelayTicks rounds it to whole ticks).
    if (root["movement"].Has("ground_lag_frames")) {
        const Json::Value& lagFrames = root["movement"]["ground_lag_frames"];
        if (!lagFrames.IsNumber() || !(lagFrames.AsNumber() >= 0.0) || !(lagFrames.AsNumber() <= kMaxGroundLagFrames)) {
            error = path + ": movement.ground_lag_frames is not a number from 0 to " + std::to_string(static_cast<int>(kMaxGroundLagFrames));
            return false;
        }
        read.groundLagFrames = lagFrames.AsNumber();
    }
    // The capsule is the hull of dark_mage.ent's two circles, and the two numbers a reader sees are what they come to: a
    // circle's radius is its share of half the box's width, its centre its share of half the box's height (ETHCompoundShape.cpp:
    // 65-71), so the hull is 2 x radius wide and 2 x (centre + radius) tall. A file whose width and height say otherwise is
    // not the original's body and is refused, naming the key.
    const double radiusPx = read.circleRadiusShare * read.boxWidthPx * 0.5;
    const double centrePx = read.circlePosShare * read.boxHeightPx * 0.5;
    constexpr double kSameTo = 1e-9;
    if (std::fabs(read.widthPx - 2.0 * radiusPx) > kSameTo) {
        error = path + ": body.width_px is not the two circles' width, 2 x circle_radius_share x box_width_px / 2";
        return false;
    }
    if (std::fabs(read.heightPx - 2.0 * (centrePx + radiusPx)) > kSameTo) {
        error = path + ": body.height_px is not the two circles' height, 2 x (circle_pos_share x box_height_px / 2 + the radius)";
        return false;
    }
    // Until a screen says otherwise the clamp is judged at the reference's.
    read.screenPxPerUnit = read.referencePxPerUnit;
    out = read;
    return true;
}

void ForLevel(Tuning& tuning, const std::string& level) {
    const auto own = tuning.airRateByLevel.find(level);
    if (own != tuning.airRateByLevel.end()) tuning.airRateHz = own->second;
}

double WalkOriginal(const Tuning& tuning, const ScriptClock::Step& step) {
    // applyForces #15-36: elapsedTime = float(min(66, getLastFrameElapsedTime())) - the script's
    // floored milliseconds - and speedScale = scale(speed) x elapsedTime / GetCurrentPhysicsTimeStepMS.
    // scale(x) is x metres here: the port's metre is 50 units, as the original's is 50 of the
    // screen's pixels, and the 256-unit design height cancels (listing 8.12a). The same expression
    // walks a minion and a knight (ScriptClock::ScriptSpeed).
    return ScriptClock::ScriptSpeed(tuning.speed, tuning.elapsedCapMs, step);
}

double WalkCarried(const Tuning& tuning, const ScriptClock::Step& step) {
    return ScriptClock::CarriedVelocity(WalkOriginal(tuning, step), step);
}

double AirShareAt(const Tuning& tuning, double fps) {
    // applyForces #71-84: min(1, GetFPSRate() / 60 x 0.86), the 60 and the 0.86 immediates.
    return std::min(1.0, tuning.airDecay * fps / tuning.airDecayFps);
}

double AirShare(const Tuning& tuning, const ScriptClock::Step& step) {
    // The rate is the file's ruling (player.json movement._air_decay_rate_hz) or a level's own, not the tick's; a file with none leaves it to
    // the clock's frame rate of the original, which is what the formula's GetFPSRate is.
    const double rateHz = tuning.airRateHz > 0.0 ? tuning.airRateHz : step.fps;
    // The share PER ORIGINAL FRAME. Where a frame is one tick it is the share a tick as it stands, in every form (the helpers return their
    // input at one tick a frame); where it is more, the form says how the ticks of a frame share it out.
    const double share = AirShareAt(tuning, rateHz);
    switch (tuning.airBrakeForm) {
    case AirBrakeForm::Travel:
        return ScriptClock::FrameTravel(share, step.frameTicks);
    case AirBrakeForm::Root:
        return ScriptClock::FrameRoot(share, step.frameTicks);
    case AirBrakeForm::Tick:
        break;
    }
    return share;
}

double FallPerTick(const Tuning& tuning, const ScriptClock::Step& step) {
    // Box2D adds gravity x gravityScale x the step to the velocity each step (b2Body.cpp's
    // integration; ETHPhysicsController::SetGravityScale); the port's tick carries it at SimRatio.
    return tuning.worldGravity * tuning.gravityScale * step.simS * ScriptClock::SimRatio(step);
}

double FeetOffsetPx(const Tuning& tuning) { return tuning.boxHeightPx * tuning.feetOffsetShare; }

double ClampLimitM(const Tuning& tuning) { return tuning.maxTranslationM / tuning.screenPxPerUnit; }

entt::entity Spawn(entt::registry& registry, const glm::dvec2& atPx, const Tuning& tuning) {
    const entt::entity entity = registry.create();
    registry.emplace<TransformComponent>(entity).position = Units::ToWorld(atPx.x, atPx.y);
    // The hull of the original's two circles (player.json body): 9.1 of radius, 36.2 in all, the entity's position its middle.
    auto& capsule = registry.emplace<CapsuleColliderComponent>(entity);
    capsule.radius = Units::ToMetres(tuning.widthPx * 0.5);
    capsule.height = Units::ToMetres(tuning.heightPx);
    auto& rigid = registry.emplace<RigidBodyComponent>(entity);
    rigid.freezeRotation = true;
    rigid.lockPosition = LevelBuilder::kPlaneLockPosition;
    rigid.friction = kFriction;
    rigid.restitution = LevelBuilder::kBodyRestitution;
    // Its mass is the .ent's: dark_mage.ent's density 0.5 over its two circles of radius 9.1 (materials.json), 0.104, where
    // the engine's default 1 weighed it as much as a barrel (4.15 times its own weight in the original: 2-24's idle player).
    rigid.mass = LevelBuilder::MassOf(LevelBuilder::MaterialOf(registry, "dark_mage"), 0.0);
    rigid.useGravity = false; // its own, applied in Steer
    rigid.allowSleep = false; // steered every tick
    registry.emplace<TagComponent>(entity, TagComponent{"player"});
    return entity;
}

bool Grounded(entt::registry& registry, entt::entity player, const Tuning& tuning) {
    // char_feet.ent's octagon (Size 14 x 10, vertices (+-chamfer, +-1) and (+-1, +-chamfer) of its half
    // size), where addFeetOffset puts it: the entity's position + (0, collisionBox.size.y x 0.5), 18 below it.
    const glm::dvec2 bottomPx = Units::ToPixels(registry.get<TransformComponent>(player).position) +
                                glm::dvec2(0.0, FeetOffsetPx(tuning));
    const glm::dvec2 half(tuning.feetWidthPx * 0.5, tuning.feetHeightPx * 0.5);
    Impacts::Shape feet;
    for (const glm::dvec2 corner : {glm::dvec2(-tuning.feetChamfer, 1.0), glm::dvec2(-1.0, tuning.feetChamfer),
                                    glm::dvec2(-1.0, -tuning.feetChamfer), glm::dvec2(-tuning.feetChamfer, -1.0),
                                    glm::dvec2(tuning.feetChamfer, -1.0), glm::dvec2(1.0, -tuning.feetChamfer),
                                    glm::dvec2(1.0, tuning.feetChamfer), glm::dvec2(tuning.feetChamfer, 1.0)}) {
        feet.core.push_back(bottomPx + corner * half);
    }
    // The engine's own broad phase names what stands near the sensor (solid colliders only: a trigger is a
    // sensor, which numTouches ignores - and no sensor of the original is built as anything else here, a fireball is
    // no body and the player's own is passed as the one to leave out: shouldBeIgnoredByFeet's four refusals); each is
    // then tested by its shape, a hull by its outline, against the octagon - the broad phase answers by bounds, so it
    // can only name more than touch. Everything else counts, a minion's body included.
    std::vector<entt::entity> candidates;
    const double reachPx = std::hypot(half.x, half.y) + tuning.feetTouchPx + 1.0;
    Supersonic::PhysicsSystem::OverlapSphere(registry, Units::ToWorld(bottomPx.x, bottomPx.y),
                                             Units::ToMetres(reachPx), candidates, player, false);
    for (const entt::entity entity : candidates) {
        Impacts::Shape solid;
        if (!Impacts::ShapeOf(registry, entity, solid)) continue;
        if (Impacts::Between(feet, solid).px <= tuning.feetTouchPx) return true;
    }
    return false;
}

bool GroundSlope(entt::registry& registry, entt::entity player, const Tuning& tuning, double& slope) {
    const glm::dvec2 centrePx = Units::ToPixels(registry.get<TransformComponent>(player).position);
    const double bottomPx = centrePx.y + tuning.heightPx * 0.5;
    // Left, centre, right.
    double hitY[3];
    entt::entity hitBody[3];
    for (int probe = 0; probe < 3; ++probe) {
        const double x = centrePx.x + (probe - 1) * tuning.holdProbeOffsetPx;
        const glm::dvec2 from(x, bottomPx - tuning.holdProbeReachPx);
        const glm::dvec2 to(x, bottomPx + tuning.holdProbeReachPx);
        const std::optional<Shot::Hit> hit = Shot::FirstBody(registry, from, to, player);
        if (!hit.has_value()) return false;
        hitY[probe] = from.y + (to.y - from.y) * hit->along;
        hitBody[probe] = hit->body;
    }
    if (hitBody[0] != hitBody[1] || hitBody[1] != hitBody[2]) return false;
    // One plane: the centre hit on the line of the outer two. A chamfer's edge or a round top between them is
    // not, and the engine's contact there is not along any line these probes could draw.
    if (std::fabs(hitY[1] - 0.5 * (hitY[0] + hitY[2])) > tuning.holdProbeLineTolPx) return false;
    slope = (hitY[2] - hitY[0]) / (2.0 * tuning.holdProbeOffsetPx);
    return true;
}

double HoldTan(const Tuning& tuning) {
    return std::sqrt(tuning.holdFriction * static_cast<double>(LevelBuilder::kStaticFriction));
}

glm::dvec2 PushAim(entt::registry& registry, entt::entity player, const Tuning& tuning, float direction,
                   const ScriptClock::Step& step) {
    const auto* rigid = registry.try_get<RigidBodyComponent>(player);
    // The body's velocity in the original's own units: what GetLinearVelocity gives applyForces.
    const glm::dvec2 velocityPx =
        rigid != nullptr ? Units::ToPixels(rigid->velocity) / ScriptClock::SimRatio(step) : glm::dvec2(0.0);
    // applyForces instructions 38-56: the walk across the vertical velocity;
    // 63-84: where isTouchingGround is not set, the body's own x instead.
    if (Grounded(registry, player, tuning)) {
        return glm::dvec2(direction * WalkOriginal(tuning, step) * Units::kPixelsPerMetre, velocityPx.y);
    }
    return glm::dvec2(velocityPx.x * AirShare(tuning, step), velocityPx.y);
}

bool Pushing(entt::registry& registry, entt::entity player, const glm::dvec2& aimPx, double reachPx,
             const glm::dvec2& offsetPx) {
    // detectPushing instructions 3-6: no x, no push, whatever lies ahead.
    if (aimPx.x == 0.0) return false;
    const glm::dvec2 fromPx = Units::ToPixels(registry.get<TransformComponent>(player).position);
    const glm::dvec2 toPx = fromPx + aimPx / std::sqrt(aimPx.x * aimPx.x + aimPx.y * aimPx.y) * reachPx + offsetPx;
    return Shot::ClosestContact(registry, fromPx, toPx, player).has_value();
}

void Steer(entt::registry& registry, entt::entity player, const Tuning& tuning, float direction,
           const ScriptClock::Step& step) {
    const bool grounded = Grounded(registry, player, tuning);
    auto& rigid = registry.get<RigidBodyComponent>(player);

    // Box2D integrates the velocity before it moves the body: gravity first.
    rigid.velocity.y -= static_cast<float>(FallPerTick(tuning, step));

    if (grounded) {
        // applyForces #38-56: set outright, the pad's -1, 0 or 1 times speedScale: no ramp, and no pad
        // is a velocity of zero - but for the hold below.
        rigid.velocity.x = direction * static_cast<float>(WalkCarried(tuning, step));
        if (direction == 0.0f) {
            // G10-5: the engine moves a body by its velocity before it solves the contacts, so the tick of gravity
            // above sinks an idle player straight down into a slope and the contact pushes him out along its
            // normal, 1.111 x sin x cos units a tick downhill (0.278 on 1-24's ramp) - a friction on the body
            // is too late to stop it (Box2D solves first). Pressed along the normal instead, the sink and the
            // push-out are the same line and nothing is left along the slope.
            double slope = 0.0;
            if (GroundSlope(registry, player, tuning, slope) && slope != 0.0 && std::fabs(slope) <= HoldTan(tuning)) {
                rigid.velocity.x = static_cast<float>(-slope * FallPerTick(tuning, step));
            }
        }
    } else {
        // #63-84: the pad is ignored and the body keeps min(1, GetFPSRate() / 60 x 0.86) of its x - evaluated at the file's
        // air_decay_rate_hz (120: all of it, planning step 213; the level's own in 2-17 and 3-21, planning steps 252 and 253), not at the tick's rate.
        rigid.velocity.x *= static_cast<float>(AirShare(tuning, step));
    }

    // Box2D's clamp, on the velocity this tick's own gravity and walk have made (the engine adds none of
    // the player's: useGravity is off).
    ClampTranslation(rigid, tuning, step);
}

void ClampTranslation(Supersonic::RigidBodyComponent& rigid, const Tuning& tuning, const ScriptClock::Step& step,
                      const glm::vec3& addedByTheEngine) {
    // b2Island::Solve (b2Island.cpp:286): a step's translation above b2_maxTranslation scales the whole
    // velocity down to it. In the port's metres a tick that is 2 / the screen's pixels a unit: 0.711 m, 35.6
    // units at the 720-line reference (23.7 at 1080: G10-6): a fall of about 590 units (586.7) reaches the
    // reference's, so only a body that goes on falling does - the hidden body of a dead player. The engine adds a body's gravity AFTER the tick's writes, so the velocity the
    // translation is judged on (and the clamp scales) is the one it will integrate: written + added.
    // The limit is per STEP OF THE ORIGINAL's physics, and the translation is what the body covers in one: a tick while a frame is one tick
    // (the number this always read), frameTicks of them otherwise, where judging a single tick would let a body go twice as fast as the original's.
    const double limitM = ClampLimitM(tuning);
    const glm::vec3 integrated = rigid.velocity + addedByTheEngine;
    const double translationM = std::hypot(static_cast<double>(integrated.x), static_cast<double>(integrated.y)) *
                                ScriptClock::OriginalStepCarriedS(step);
    if (translationM > limitM) rigid.velocity = integrated * static_cast<float>(limitM / translationM) - addedByTheEngine;
}

} // namespace MagicPortals::Player
