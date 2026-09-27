#include "sim/Earthquake.hpp"

#include "core/Json.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <sstream>

namespace MagicPortals::Earthquake {

namespace {

namespace Json = Supersonic::Json;

// In Call's order, which the enum states; KeyOf and the loader both index it.
constexpr const char* kKeys[] = {
    "fall",
    "fall_no_gravity",
    "barrel_blast",
    "beholder_death",
    "dark_dragon_death",
    "ghost_death",
    "wall_struck",
    "wall_destroyed",
    "stone_despawned",
    "boss_rock_broken",
    "beholder_hurt",
    "dark_dragon_mouth",
    "dark_dragon_shot",
    "door_landed",
    "door_opening",
    "door_closing",
    "fireball_end",
    "enemy_shot_end",
    "minion_killed",
    "door_unlocked",
    "shock_diamond_picked",
    "fire_diamond_picked",
    "shock_strike",
    "light_wall_gone",
};
static_assert(std::size(kKeys) == kCalls, "one earthquake.json key per Call");

bool Number(const Json::Value& object, const std::string& where, const char* key, double& to, std::string& error) {
    const Json::Value& value = object[key];
    if (!value.IsNumber() || !std::isfinite(value.AsNumber())) {
        error = where + "." + key + " is missing or not a number";
        return false;
    }
    to = value.AsNumber();
    return true;
}

bool ReadGenerator(const Json::Value& root, Rules& read, std::string& error) {
    const Json::Value& generator = root["generator"];
    if (!generator.IsObject()) {
        error = "no \"generator\" object";
        return false;
    }
    const std::string where = "generator";
    const struct {
        const char* key;
        double* into;
    } numbers[] = {{"force_scale", &read.forceScale},
                   {"cap_px", &read.capPx},
                   {"stop_px", &read.stopPx},
                   {"tremble_interval_ms", &read.trembleIntervalMs},
                   {"back_scale", &read.backScale},
                   {"screen_px_per_unit", &read.screenPxPerUnit}};
    for (const auto& number : numbers) {
        if (!Number(generator, where, number.key, *number.into, error)) return false;
        if (!(*number.into > 0.0)) {
            error = where + "." + number.key + " is not above 0";
            return false;
        }
    }
    // The back HALVES the amplitude; a scale of 1 or more would never let a
    // shake reach its stop.
    if (read.backScale >= 1.0) {
        error = where + ".back_scale is the 0.5 a back multiplies by: it must shrink the shake";
        return false;
    }
    if (read.stopPx >= read.capPx) {
        error = where + ".stop_px must be under cap_px, or no shake would ever tremble twice";
        return false;
    }
    if (!generator["forth_first"].IsBool()) {
        error = where + ".forth_first is missing or not true/false";
        return false;
    }
    read.forthFirst = generator["forth_first"].AsBool();
    return true;
}

bool ReadCalls(const Json::Value& root, Rules& read, std::string& error) {
    const Json::Value& calls = root["calls"];
    if (!calls.IsObject()) {
        error = "no \"calls\" object";
        return false;
    }
    for (std::size_t i = 0; i < kCalls; ++i) {
        const std::string where = std::string("calls.") + kKeys[i];
        const Json::Value& row = calls[kKeys[i]];
        if (!row.IsObject()) {
            error = where + " is missing";
            return false;
        }
        CallRule rule;
        if (!Number(row, where, "intensity", rule.intensity, error)) return false;
        if (!(rule.intensity > 0.0)) {
            error = where + ".intensity is not above 0";
            return false;
        }
        const std::string when = row["when"].AsString();
        if (when == "before_update") {
            rule.when = When::BeforeUpdate;
        } else if (when == "after_update") {
            rule.when = When::AfterUpdate;
        } else {
            error = where + ".when is \"" + when + "\", not before_update or after_update";
            return false;
        }
        read.calls[i] = rule;
    }

    const Json::Value& wall = calls[kKeys[static_cast<std::size_t>(Call::WallDestroyed)]];
    read.wallEntity = wall["entity"].AsString();
    read.wallScaleKey = wall["scale_key"].AsString();
    if (read.wallEntity.empty() || read.wallScaleKey.empty()) {
        error = "calls.wall_destroyed needs `entity`, whose callbacks the two wall rows are, and `scale_key`, the "
                "datum its shake is multiplied by";
        return false;
    }

    const Json::Value& mouth = calls[kKeys[static_cast<std::size_t>(Call::DarkDragonMouth)]];
    double leg = 0.0;
    if (!Number(mouth, "calls.dark_dragon_mouth", "leg", leg, error) ||
        !Number(mouth, "calls.dark_dragon_mouth", "bias_above", read.mouthBiasAbove, error)) {
        return false;
    }
    if (leg < 0.0 || leg != std::floor(leg)) {
        error = "calls.dark_dragon_mouth.leg is a waypoint's index: a whole number, 0 or more";
        return false;
    }
    read.mouthLeg = static_cast<int>(leg);
    if (read.mouthBiasAbove < 0.0 || read.mouthBiasAbove >= 1.0) {
        error = "calls.dark_dragon_mouth.bias_above is a share of the leg: 0 up to 1";
        return false;
    }
    return true;
}

} // namespace

const char* KeyOf(Call call) {
    const auto index = static_cast<std::size_t>(call);
    return index < kCalls ? kKeys[index] : "unknown";
}

bool LoadRules(const std::string& path, Rules& out, std::string& error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = path + ": cannot open";
        return false;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    // Named, not a temporary: the parser keeps a reference to what it reads.
    const std::string text = buffer.str();
    Json::Parser parser(text);
    Json::Value root;
    if (!parser.Parse(root)) {
        error = path + ": " + parser.Error();
        return false;
    }
    Rules read;
    std::string why;
    if (!root.IsObject() || !ReadGenerator(root, read, why) || !ReadCalls(root, read, why)) {
        error = path + ": " + (why.empty() ? std::string("is not an object") : why);
        return false;
    }
    out = std::move(read);
    return true;
}

void Generator::Reset(const Rules& rules) {
    *this = Generator{};
    forth = rules.forthFirst;
}

void Generator::Start(const Rules& rules, double intensity) {
    // #3-8 g_scale.scale(force) x 0.35, #10-16 min(..., 50) then max(0, ...),
    // #18-19 written over whatever was there.
    amplitudePx = std::clamp(intensity * rules.screenPxPerUnit * rules.forceScale, 0.0, rules.capPx);
}

double Generator::Update(const Rules& rules, double nowMs) {
    double movePx = 0.0;
    if (amplitudePx != 0.0 && nowMs - lastTrembleMs >= rules.trembleIntervalMs) {
        if (forth) {
            movePx = amplitudePx;
            backPx = amplitudePx;
        } else {
            // Back by what the forth moved, which a call in between does not
            // change; the halving is of the amplitude as it now stands.
            movePx = -backPx;
            amplitudePx *= rules.backScale;
        }
        forth = !forth;
        lastTrembleMs = nowMs;
    }
    // #75-85, on every update and not only on a tremble's.
    if (std::fabs(amplitudePx) < rules.stopPx) amplitudePx = 0.0;
    return movePx;
}

bool IsEntity(const std::string& nodeEntityName, const std::string& entity) {
    return !entity.empty() && (nodeEntityName == entity || nodeEntityName == entity + ".ent");
}

double ToUnits(const Rules& rules, double px) {
    return rules.screenPxPerUnit > 0.0 ? px / rules.screenPxPerUnit : 0.0;
}

bool InMouthLeg(const Rules& rules, const std::vector<double>& legHoldsMs, double phaseMs) {
    double startMs = 0.0;
    for (std::size_t leg = 0; leg < legHoldsMs.size(); ++leg) {
        const double holdMs = legHoldsMs[leg];
        if (static_cast<int>(leg) == rules.mouthLeg) {
            if (!(holdMs > 0.0)) return false;
            const double bias = (phaseMs - startMs) / holdMs;
            // Past the leg's end the manager is on the next waypoint.
            return bias > rules.mouthBiasAbove && bias < 1.0;
        }
        startMs += holdMs;
    }
    return false;
}

} // namespace MagicPortals::Earthquake
