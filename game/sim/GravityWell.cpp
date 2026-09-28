#include "sim/GravityWell.hpp"

#include "core/Components.hpp"
#include "core/DetMath.hpp"
#include "core/Json.hpp"
#include "sim/Units.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>

namespace MagicPortals::GravityWell {

namespace {

namespace Json = Supersonic::Json;

using Supersonic::RigidBodyComponent;
using Supersonic::TransformComponent;

// PI/2. INFERRED rather than decoded: PIb is a global the decoder renders by
// name and never defines, so every appearance of it is a use. Three independent
// ones fix it - smoothEnd is sin(v * PIb) and must reach 1 at v = 1, `PI + PIb`
// is a half-turn plus a quarter, and getAngle brackets its quadrants with
// PIb/2 and PI + PIb/2. gravitywell.json says so where the number is recorded.
constexpr double kPiOverTwo = 1.57079632679489661923;

// Interpolator.angelscript's smoothEnd: an ease-out that is 0 at 0 and 1 at 1,
// steep at the start and flat at the end. DetMath, so a well pulls the same on
// every C runtime - the contract Mover::Oscillation and Bounce::Bob keep.
double SmoothEnd(double v) {
    return static_cast<double>(Supersonic::DetMath::sin(static_cast<float>(v * kPiOverTwo)));
}

// Three finite numbers, none below zero: a colour the original could paint, or an
// .ent's <EmissiveColor>. The same rule Art.cpp reads its own by.
bool Colour(const Json::Value& value, glm::dvec3& out) {
    if (!value.IsArray() || value.AsArray().size() != 3) return false;
    for (std::size_t i = 0; i < 3; ++i) {
        const Json::Value& channel = value.AsArray()[i];
        if (!channel.IsNumber() || !std::isfinite(channel.AsNumber()) || channel.AsNumber() < 0.0) return false;
        out[static_cast<glm::length_t>(i)] = channel.AsNumber();
    }
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
    if (!number("force", "length_zero_gravity", read.forceZeroGravity)) return false;
    if (!number("force", "length_with_gravity", read.forceWithGravity)) return false;
    if (!number("force", "reference_frame_ms", read.referenceFrameMs)) return false;
    if (!number("zone", "shrink_px", read.zoneShrinkPx)) return false;

    // THE RING, required in full. Every field of it is a fact about the entity the
    // agent's callback adds - antiportal.ent, renamed - and a default for any of
    // them would be a guess drawn on five levels.
    const Json::Value& ring = root["ring"];
    if (!ring.IsObject()) {
        error = path + ": ring is missing";
        return false;
    }
    if (!ring["sprite"].IsString() || ring["sprite"].AsString().empty()) {
        error = path + ": ring.sprite is a file among the original's entities";
        return false;
    }
    read.ring.sprite = ring["sprite"].AsString();
    for (const char* key : {"additive", "apply_light", "static"}) {
        if (!ring[key].IsBool()) {
            error = path + ": ring." + std::string(key) + " is true or false";
            return false;
        }
    }
    read.ring.additive = ring["additive"].AsBool();
    read.ring.applyLight = ring["apply_light"].AsBool();
    read.ring.isStatic = ring["static"].AsBool();
    if (!Colour(ring["colour"], read.ring.colour)) {
        error = path + ": ring.colour is three numbers, none below zero";
        return false;
    }
    if (!Colour(ring["emissive"], read.ring.emissive)) {
        error = path + ": ring.emissive is three numbers, none below zero";
        return false;
    }
    if (!ring["z"].IsNumber() || !std::isfinite(ring["z"].AsNumber())) {
        error = path + ": ring.z is a finite number";
        return false;
    }
    read.ring.z = ring["z"].AsNumber();

    // THE LAPS, required in full, each a number the listing states (the
    // `roundabout` block cites them). A default for any would draw an effect or
    // spin a picture on a guess.
    if (!root["roundabout"].IsObject()) {
        error = path + ": roundabout is missing";
        return false;
    }
    if (!number("roundabout", "step_dot_below", read.roundabout.stepDotBelow)) return false;
    if (!number("roundabout", "lap_degrees", read.roundabout.lapDegrees)) return false;
    if (!number("roundabout", "spin_ms", read.roundabout.spinMs)) return false;
    if (!number("roundabout", "spin_degrees", read.roundabout.spinDegrees)) return false;
    if (!number("roundabout", "elastic_period", read.roundabout.elasticPeriod)) return false;
    if (!number("roundabout", "elastic_shift_divisor", read.roundabout.elasticShiftDivisor)) return false;
    if (!number("roundabout", "elastic_decay", read.roundabout.elasticDecay)) return false;
    // A step threshold outside (-1, 1) counts every step or none; a lap of nothing
    // fires every step; a spin over no time divides by zero; an elastic with no
    // period or shift divides by zero, and one that does not decay never settles.
    const Roundabout& lap = read.roundabout;
    if (!(lap.stepDotBelow > -1.0 && lap.stepDotBelow < 1.0) || !(lap.lapDegrees > 0.0) || !(lap.spinMs > 0.0) ||
        !(lap.elasticPeriod > 0.0) || !(lap.elasticShiftDivisor > 0.0) || !(lap.elasticDecay < 0.0)) {
        error = path + ": roundabout needs step_dot_below inside (-1, 1), lap_degrees, spin_ms, elastic_period and "
                       "elastic_shift_divisor above 0 and elastic_decay below 0";
        return false;
    }

    // A well with no pull is a solid circle with a no-portal zone round it, and
    // five levels would look built and play wrong.
    if (read.forceZeroGravity <= 0.0 || read.forceWithGravity <= 0.0) {
        error = path + ": force.length_zero_gravity and force.length_with_gravity are above zero";
        return false;
    }
    // A reference frame of nothing divides by zero.
    if (read.referenceFrameMs <= 0.0) {
        error = path + ": force.reference_frame_ms is above zero";
        return false;
    }
    // The shrink takes SIZE off the antiportal, so it may not be negative, and a
    // shrink wider than the smallest well in the game (64 px radius, so 128 of
    // size) would give that well a zone of nothing at all.
    if (read.zoneShrinkPx < 0.0) {
        error = path + ": zone.shrink_px is not negative";
        return false;
    }

    out = std::move(read);
    return true;
}

glm::vec3 Pull(const Well& well, const Rules& rules, const glm::dvec2& bodyPx, bool noGravity, float dt) {
    const glm::dvec2 toCentre = well.atPx - bodyPx;
    const double squaredDist = toCentre.x * toCentre.x + toCentre.y * toCentre.y;
    const double squaredRadius = well.radiusPx * well.radiusPx;

    // A strict less-than, as the original's CMPf/TS is: a body exactly on the
    // rim is outside.
    if (!(squaredDist < squaredRadius)) return glm::vec3(0.0f);
    const double length = std::sqrt(squaredDist);
    // Dead centre, which normalize cannot answer. Nothing, rather than a NaN
    // that would poison the body's velocity for the rest of the level.
    if (length <= 0.0) return glm::vec3(0.0f);

    const glm::dvec2 direction = toCentre / length;
    const double bias = SmoothEnd(1.0 - squaredDist / squaredRadius);
    const double strength = noGravity ? rules.forceZeroGravity : rules.forceWithGravity;
    const double fpsFix = (static_cast<double>(dt) * 1000.0) / rules.referenceFrameMs;
    const double force = bias * strength * fpsFix;

    // Turned over in y: the remake's pixels count down, the engine's metres up.
    // The magnitude is already metres per second - gravitywell.json says why it
    // needs no conversion, as the portal recoil does not.
    return glm::vec3(static_cast<float>(direction.x * force), static_cast<float>(-direction.y * force), 0.0f);
}

float Elastic(const Roundabout& rules, float n) {
    // #1-13: n itself at 0 and at 1.
    if (n == 0.0f || n == 1.0f) return n;
    // #15-19: p and s are doubles - p the float 0.3f widened (SetV8's raw
    // 0x3fd3333340000000), s = p / 4.
    const double p = static_cast<double>(static_cast<float>(rules.elasticPeriod));
    const double s = p / rules.elasticShiftDivisor;
    // #21-25: pow(2, -10 n), in floats (arguments pushed last-first).
    const float decay = Supersonic::DetMath::pow(2.0f, static_cast<float>(rules.elasticDecay) * n);
    // #26-36: sin(float(((double)n - s) x (double)(PI x 2) / p)), PI the float global.
    constexpr float kPi = 3.141592654f;
    const double argument = (static_cast<double>(n) - s) * static_cast<double>(kPi * 2.0f) / p;
    const float wave = Supersonic::DetMath::sin(static_cast<float>(argument));
    // #38-40.
    return decay * wave + 1.0f;
}

double SpinDegrees(const Roundabout& rules, double ms, double sign) {
    // ETHCallback_gravity_agent #191-230: min(3000f, t) / 3000f, smoothEnd, elastic,
    // x 360, x effectAngleSign. smoothEnd(1) is sin(1.5707964f), which rounds to
    // exactly 1.0f, so at rest elastic returns its 1 and the turn is a whole 360.
    const float spin = static_cast<float>(rules.spinMs);
    const float bias = std::min(spin, static_cast<float>(ms)) / spin;
    constexpr float kPiOverTwoF = 1.570796327f; // PIb (00_order 8.10, D9)
    const float eased = Supersonic::DetMath::sin(bias * kPiOverTwoF);
    return static_cast<double>(Elastic(rules, eased) * static_cast<float>(rules.spinDegrees) *
                               static_cast<float>(sign));
}

bool StepSweep(const Roundabout& rules, Sweep& sweep, bool first, const glm::vec2& toWell, bool inRange, float wellY,
               float bodyY, double& lapDegrees) {
    // #93-146: the first time a body is met, its sum is 0 and its last direction
    // this one - so the step below cannot count on that call.
    if (first) {
        sweep.roundSum = 0.0f;
        sweep.lastVector = toWell;
        sweep.cw = 0;
        sweep.ccw = 0;
    }
    const glm::vec2 last = sweep.lastVector;
    // #148-169: dp2, in floats.
    const float product = toWell.x * last.x + toWell.y * last.y;
    // #171-190: counted only in range, only when the direction moved, and only once
    // it has moved far enough - the last direction is the one the last COUNTED
    // step ended on (#265-270), so a slow drift adds up until it passes. Out of
    // range nothing is counted and nothing is forgotten.
    if (inRange && toWell != last && product < static_cast<float>(rules.stepDotBelow)) {
        // #192-252: +1, or -1 where the body is below the well and the direction's
        // x grew, or above it and the x shrank - which makes a counter-clockwise
        // sweep on the screen (+y down) positive.
        float valueSign = 1.0f;
        const float dx = toWell.x - last.x;
        if (wellY < bodyY && dx > 0.0f) valueSign = -1.0f;
        if (wellY > bodyY && dx < 0.0f) valueSign = -1.0f;
        // #254-263: acos(product), as atan2(sqrt(1 - p^2), p) so that it is the
        // same bits on every machine. The product of two unit vectors can round a
        // hair past -1, where acos has no answer; the original's would poison the
        // sum with a NaN for the rest of the level, and the port's takes the half
        // turn.
        const float clamped = std::clamp(product, -1.0f, 1.0f);
        const float turned =
            Supersonic::DetMath::atan2(std::sqrt((1.0f - clamped) * (1.0f + clamped)), clamped);
        sweep.roundSum += turned * valueSign;
        sweep.lastVector = toWell;
    }
    // #272-286: radianToDegree (GameMath.h:717-720) and a lap strictly beyond 360.
    constexpr float kPi = 3.141592654f;
    const float degrees = sweep.roundSum / (kPi * 2.0f) * 360.0f;
    if (!(std::fabs(degrees) > static_cast<float>(rules.lapDegrees))) return false;
    // #288-330: the sum starts again, and the lap is counted its way round.
    sweep.roundSum = 0.0f;
    if (degrees < 0.0f) {
        ++sweep.cw;
    } else {
        ++sweep.ccw;
    }
    lapDegrees = static_cast<double>(degrees);
    return true;
}

void State::Tick(entt::registry& registry, float dt) {
    lapsNow.clear();
    if (wells.empty() || dt <= 0.0f) return;

    // Every DYNAMIC body: the port's reading of DynamicBodyChooser, which
    // rejects IsStatic() and anything without a physics controller. Here that is
    // a RigidBodyComponent that is not kinematic - a kinematic body is moved by
    // code each tick and a well would only fight whatever moves it.
    for (auto [entity, transform, rigid] : registry.view<TransformComponent, RigidBodyComponent>().each()) {
        if (rigid.isKinematic) continue;
        const glm::dvec2 bodyPx(transform.position.x * Units::kPixelsPerMetre,
                                -transform.position.y * Units::kPixelsPerMetre);
        for (std::size_t i = 0; i < wells.size(); ++i) {
            const Well& well = wells[i];
            rigid.velocity += Pull(well, rules, bodyPx, noGravity, dt);

            // And the sweep, from the same point: ETHCallback_gravity_area's
            // forceDir and inRange (#82-98), normalize in floats (GameMath.h:755-761).
            const float dx = static_cast<float>(well.atPx.x) - static_cast<float>(bodyPx.x);
            const float dy = static_cast<float>(well.atPx.y) - static_cast<float>(bodyPx.y);
            const float length = std::sqrt(dx * dx + dy * dy);
            const glm::vec2 toWell = length > 0.0f ? glm::vec2(dx / length, dy / length) : glm::vec2(0.0f);
            const glm::dvec2 apart = well.atPx - bodyPx;
            const bool inRange = apart.x * apart.x + apart.y * apart.y < well.radiusPx * well.radiusPx;
            const int wellIndex = static_cast<int>(i);
            auto found = std::find_if(sweeps.begin(), sweeps.end(), [&](const Sweep& sweep) {
                return sweep.body == entity && sweep.well == wellIndex;
            });
            const bool first = found == sweeps.end();
            if (first) {
                Sweep made;
                made.body = entity;
                made.well = wellIndex;
                sweeps.push_back(made);
                found = sweeps.end() - 1;
            }
            double degrees = 0.0;
            if (StepSweep(rules.roundabout, *found, first, toWell, inRange, static_cast<float>(well.atPx.y),
                          static_cast<float>(bodyPx.y), degrees)) {
                lapsNow.push_back(Lap{entity, wellIndex, degrees});
            }
        }
    }
}

const Well* State::Find(const std::string& name) const {
    for (const Well& well : wells) {
        if (well.name == name) return &well;
    }
    return nullptr;
}

bool Find(const Tscn::Scene& scene, const Roles::Table& roles, const Rules& rules, State& out, std::string& error) {
    out = State{};
    out.rules = rules;

    for (const Tscn::Node& node : scene.nodes) {
        if (node.parent != ".") continue;
        if (Roles::RoleOf(roles, node) != Roles::kGravityWell) continue;

        const Tscn::Value* position = node.Find("position");
        if (position == nullptr || position->kind != Tscn::Value::Kind::Vector2) {
            error = node.name + " is a gravity well with no position";
            return false;
        }
        const Tscn::Value* radius = node.Meta("radius");
        double radiusPx = 0.0;
        if (radius == nullptr || !radius->AsNumber(radiusPx) || radiusPx <= 0.0) {
            error = node.name + " is a gravity well with no radius";
            return false;
        }

        Well well;
        well.name = node.name;
        well.atPx = glm::dvec2(position->numbers[0], position->numbers[1]);
        well.radiusPx = radiusPx;
        out.wells.push_back(well);
    }
    return true;
}

} // namespace MagicPortals::GravityWell
