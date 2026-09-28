#include "sim/Effects.hpp"

#include "core/DetMath.hpp"
#include "core/Json.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <utility>

namespace MagicPortals::Effects {

namespace {

namespace Json = Supersonic::Json;

constexpr const char* kTowardTravellerMinus90 = "toward_traveller_minus_90";
constexpr const char* kAlongFlightMinus90 = "along_flight_minus_90";

bool Finite(const Json::Value& value) {
    return value.IsNumber() && std::isfinite(value.AsNumber());
}

// One row, every field stated: a row that left one out would be a call whose
// facts nobody decoded.
bool ReadRule(const Json::Value& root, const char* name, Rule& out, std::string& error) {
    const Json::Value& row = root[name];
    const std::string where = std::string(name);
    if (!row.IsObject()) {
        error = where + " is not an object";
        return false;
    }
    Rule read;
    if (!row["entity"].IsString() || row["entity"].AsString().empty()) {
        error = where + ".entity is not a file name";
        return false;
    }
    read.entity = row["entity"].AsString();
    // A number, along the call's direction; or a pair, a fixed (x, y) added along
    // none (levelFinishedEffect's vector2(0, 16)).
    const Json::Value& offset = row["offset_units"];
    if (Finite(offset)) {
        read.offsetUnits = offset.AsNumber();
    } else if (offset.IsArray() && offset.AsArray().size() == 2 && Finite(offset.AsArray()[0]) &&
               Finite(offset.AsArray()[1])) {
        read.offsetFixedUnits = glm::dvec2(offset.AsArray()[0].AsNumber(), offset.AsArray()[1].AsNumber());
    } else {
        error = where + ".offset_units is not a number or a pair [x, y] of numbers";
        return false;
    }
    const Json::Value& z = row["z"];
    if (!z.IsObject() || !Finite(z["value"]) || !z["scaled"].IsBool()) {
        error = where + ".z needs a value and scaled, true or false";
        return false;
    }
    read.z = z["value"].AsNumber();
    read.zScaled = z["scaled"].AsBool();
    const Json::Value& angle = row["angle"];
    if (Finite(angle)) {
        read.turn = Turn::Fixed;
        read.angleDeg = angle.AsNumber();
    } else if (angle.IsString() && angle.AsString() == kTowardTravellerMinus90) {
        read.turn = Turn::TowardTravellerMinus90;
    } else if (angle.IsString() && angle.AsString() == kAlongFlightMinus90) {
        read.turn = Turn::AlongFlightMinus90;
    } else {
        error = where + ".angle is a number, " + kTowardTravellerMinus90 + " or " + kAlongFlightMinus90;
        return false;
    }
    if (!Finite(row["scale"]) || !(row["scale"].AsNumber() > 0.0)) {
        error = where + ".scale is not a number above 0";
        return false;
    }
    read.scale = row["scale"].AsNumber();
    out = std::move(read);
    return true;
}

// The eyes' row, every field stated, as the rows above are.
bool ReadEyes(const Json::Value& root, const char* name, EyeRule& out, std::string& error) {
    const Json::Value& row = root[name];
    const std::string where = std::string(name);
    if (!row.IsObject()) {
        error = where + " is not an object";
        return false;
    }
    EyeRule read;
    if (!row["entity"].IsString() || row["entity"].AsString().empty()) {
        error = where + ".entity is not a file name";
        return false;
    }
    read.entity = row["entity"].AsString();
    const Json::Value& offset = row["offset_units"];
    if (!offset.IsObject()) {
        error = where + ".offset_units is not an object";
        return false;
    }
    struct Field {
        const char* key;
        double* into;
    };
    const Field fields[] = {{"x_frame_gt_7", &read.xFrameAboveSeven},
                            {"x_else", &read.xElse},
                            {"y", &read.y},
                            {"y_zero_g", &read.yNoGravity},
                            {"z", &read.z}};
    for (const Field& field : fields) {
        if (!Finite(offset[field.key])) {
            error = where + ".offset_units." + field.key + " is not a number";
            return false;
        }
        *field.into = offset[field.key].AsNumber();
    }
    if (!Finite(row["scale"]) || !(row["scale"].AsNumber() > 0.0)) {
        error = where + ".scale is not a number above 0";
        return false;
    }
    read.scale = row["scale"].AsNumber();
    if (!row["hide_with_player"].IsBool()) {
        error = where + ".hide_with_player is not true or false";
        return false;
    }
    read.hideWithPlayer = row["hide_with_player"].AsBool();
    out = std::move(read);
    return true;
}

// The placed portal's own block: three colours, the fade and the delay.
bool ReadPlacedPortal(const Json::Value& root, const char* name, PlacedPortal& out, std::string& error) {
    const Json::Value& row = root[name];
    const std::string where = std::string(name);
    if (!row.IsObject()) {
        error = where + " is not an object";
        return false;
    }
    PlacedPortal read;
    const struct {
        const char* key;
        glm::dvec3* into;
    } colours[] = {{"colour_first", &read.colourFirst},
                   {"colour_second", &read.colourSecond},
                   {"colour_alone", &read.colourAlone}};
    for (const auto& colour : colours) {
        const Json::Value& value = row[colour.key];
        if (!value.IsArray() || value.AsArray().size() != 3) {
            error = where + "." + colour.key + " is not three numbers";
            return false;
        }
        for (std::size_t i = 0; i < 3; ++i) {
            const Json::Value& channel = value.AsArray()[i];
            if (!Finite(channel) || channel.AsNumber() < 0.0) {
                error = where + "." + colour.key + " is not three numbers at or above 0";
                return false;
            }
            (*colour.into)[static_cast<glm::length_t>(i)] = channel.AsNumber();
        }
    }
    const struct {
        const char* key;
        double* into;
    } numbers[] = {{"kill_fade_per_frame", &read.killFadePerFrame},
                   {"kill_fade_frame_ms", &read.killFadeFrameMs},
                   {"death_after_kill_ms", &read.deathAfterKillMs}};
    for (const auto& number : numbers) {
        if (!Finite(row[number.key]) || !(row[number.key].AsNumber() > 0.0)) {
            error = where + "." + number.key + " is not a number above 0";
            return false;
        }
        *number.into = row[number.key].AsNumber();
    }
    if (read.killFadePerFrame >= 1.0) {
        error = where + ".kill_fade_per_frame is the 0.9 a dying portal is multiplied by: it fades";
        return false;
    }
    out = read;
    return true;
}

Spawn SpawnOf(const Rule& rule, const glm::dvec2& fromPx, const glm::dvec2& direction) {
    Spawn spawn;
    spawn.entity = rule.entity;
    spawn.atPx = fromPx + direction * rule.offsetUnits + rule.offsetFixedUnits;
    spawn.z = rule.z;
    spawn.zScaled = rule.zScaled;
    spawn.angleDeg = AngleDegrees(rule, direction);
    spawn.scale = rule.scale;
    return spawn;
}

} // namespace

bool LoadRules(const std::string& path, Rules& out, std::string& error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = path + ": cannot read";
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
    if (!root.IsObject() || !ReadRule(root, "suck_effect", read.suck, why) ||
        !ReadRule(root, "sparkles", read.sparkles, why) || !ReadRule(root, "appear_effect", read.appear, why) ||
        !ReadRule(root, "fireball", read.fireball, why) ||
        !ReadRule(root, "small_explosion", read.smallExplosion, why) ||
        !ReadRule(root, "explosion_large", read.largeExplosion, why) ||
        !ReadRule(root, "portal_summon_effect", read.summon, why) || !ReadRule(root, "portal", read.portal, why) ||
        !ReadRule(root, "portal_death", read.death, why) ||
        !ReadPlacedPortal(root, "placed_portal", read.placedPortal, why) ||
        !ReadEyes(root, "red_eyes", read.eyes, why) || !ReadRule(root, "red_suck_effect", read.finishSuck, why) ||
        !ReadRule(root, "red_sparkles", read.finishSparkles, why) || !ReadRule(root, "blood", read.blood, why) ||
        !ReadRule(root, "skull", read.skull, why) || !ReadRule(root, "enemy_shoot", read.enemyShoot, why) ||
        !ReadRule(root, "enemy_shoot_mark", read.enemyShootMark, why) ||
        !ReadRule(root, "enemy_shoot_flash", read.enemyShootFlash, why) ||
        !ReadRule(root, "enemy_shoot_burst", read.enemyShootBurst, why) ||
        !ReadRule(root, "ghost_appear_effect", read.ghostAppear, why)) {
        error = path + ": " + (why.empty() ? std::string("is not an object") : why);
        return false;
    }
    // The skull's one number of its own: how far inside the view it is held.
    if (!Finite(root["skull"]["inset_units"]) || !(root["skull"]["inset_units"].AsNumber() > 0.0)) {
        error = path + ": skull.inset_units is not a number above 0";
        return false;
    }
    read.skullInsetUnits = root["skull"]["inset_units"].AsNumber();
    // Blood's depth is its VICTIM's own: both callers pass GetPosition()'s whole
    // vector3 (checkGameLost #27-43, ETHCallback_minion #1478-1504), a number
    // already on the level's axis, which Blood adds this row's z to and keeps
    // scaled. So the row says 0, scaled: a bare literal there would be added
    // undivided by the factor, and any other number is a depth nobody decoded
    // (11.3's third check; built with the next step to relink the simulation).
    if (read.blood.z != 0.0 || !read.blood.zScaled) {
        error = path + ": blood.z is {\"value\": 0, \"scaled\": true}: its depth is its victim's own";
        return false;
    }
    // The ghost's appear effect's own callback: followUp and Scale, every frame.
    {
        const Json::Value& follow = root["ghost_appear_effect"]["follow_up"];
        const Json::Value& offset = follow["offset_units"];
        const bool followOk = follow.IsObject() && offset.IsArray() && offset.AsArray().size() == 2 &&
                              Finite(offset.AsArray()[0]) && Finite(offset.AsArray()[1]) &&
                              Finite(follow["stride_ms"]) && follow["stride_ms"].AsNumber() > 0.0 &&
                              Finite(follow["rate_ms"]) && follow["rate_ms"].AsNumber() >= 0.0 &&
                              follow["dont_pause"].IsBool();
        const Json::Value& grow = root["ghost_appear_effect"]["scale_per_frame"];
        if (!followOk || !Finite(grow) || !(grow.AsNumber() > 0.0)) {
            error = path + ": ghost_appear_effect needs follow_up (offset_units [x, y], stride_ms above 0, rate_ms "
                           "from 0, dont_pause) and a scale_per_frame above 0";
            return false;
        }
        read.ghostAppearFollow.offsetUnits = glm::dvec2(offset.AsArray()[0].AsNumber(), offset.AsArray()[1].AsNumber());
        read.ghostAppearFollow.strideMs = follow["stride_ms"].AsNumber();
        read.ghostAppearFollow.rateMs = follow["rate_ms"].AsNumber();
        read.ghostAppearFollow.dontPause = follow["dont_pause"].AsBool();
        read.ghostAppearScalePerFrame = grow.AsNumber();
    }
    // The shot's flash, like blood, is added at its OWNER's own vector3: 0 over it.
    if (read.enemyShootFlash.z != 0.0 || !read.enemyShootFlash.zScaled) {
        error = path + ": enemy_shoot_flash.z is {\"value\": 0, \"scaled\": true}: its depth is the shot's own";
        return false;
    }
    // And the one row that is turned along a flight is the shot itself.
    if (read.enemyShoot.turn != Turn::AlongFlightMinus90 || read.enemyShoot.offsetUnits != 0.0) {
        error = path + ": enemy_shoot is turned along_flight_minus_90 and offset 0: its callback turns it";
        return false;
    }

    // The rows that are given a point and nothing else: no direction to be offset
    // along or turned by. appearEffect, addFireball - whose `dir` is the entity's
    // own custom data and not the angle it is added at - and both explosions,
    // which AddScaledEntity adds at a flat 0f.
    const struct {
        const char* name;
        const Rule* rule;
    } pointOnly[] = {{"appear_effect", &read.appear},
                     {"fireball", &read.fireball},
                     {"small_explosion", &read.smallExplosion},
                     {"explosion_large", &read.largeExplosion},
                     {"portal_summon_effect", &read.summon},
                     {"portal", &read.portal},
                     {"portal_death", &read.death},
                     {"red_suck_effect", &read.finishSuck},
                     {"red_sparkles", &read.finishSparkles},
                     {"blood", &read.blood},
                     {"skull", &read.skull},
                     {"enemy_shoot_mark", &read.enemyShootMark},
                     {"enemy_shoot_flash", &read.enemyShootFlash},
                     {"enemy_shoot_burst", &read.enemyShootBurst},
                     {"ghost_appear_effect", &read.ghostAppear}};
    for (const auto& row : pointOnly) {
        if (row.rule->turn != Turn::Fixed || row.rule->offsetUnits != 0.0) {
            error = path + ": " + row.name + " has no direction: its angle is a number and its offset_units 0";
            return false;
        }
    }
    // And the fixed offset is levelFinishedEffect's alone: no other call adds one,
    // so a pair on any other row is a fact nobody decoded.
    const struct {
        const char* name;
        const Rule* rule;
    } unshifted[] = {{"suck_effect", &read.suck},
                     {"sparkles", &read.sparkles},
                     {"appear_effect", &read.appear},
                     {"fireball", &read.fireball},
                     {"small_explosion", &read.smallExplosion},
                     {"explosion_large", &read.largeExplosion},
                     {"portal_summon_effect", &read.summon},
                     {"portal", &read.portal},
                     {"portal_death", &read.death},
                     {"red_sparkles", &read.finishSparkles},
                     {"blood", &read.blood},
                     {"skull", &read.skull},
                     {"enemy_shoot", &read.enemyShoot},
                     {"enemy_shoot_mark", &read.enemyShootMark},
                     {"enemy_shoot_flash", &read.enemyShootFlash},
                     {"enemy_shoot_burst", &read.enemyShootBurst},
                     {"ghost_appear_effect", &read.ghostAppear}};
    for (const auto& row : unshifted) {
        if (row.rule->offsetFixedUnits != glm::dvec2(0.0)) {
            error = path + ": " + row.name + " adds no fixed offset: its offset_units is a number";
            return false;
        }
    }
    // updateEyes both moves the eyes onto the character and hides them with it,
    // and the port has one owner that does both. A row that said otherwise would
    // need an owner of its own, so it is refused rather than half-honoured.
    if (!read.eyes.hideWithPlayer) {
        error = path + ": red_eyes.hide_with_player is the decoded redEyes.Hide(true), and the port's player owner "
                       "hides what it carries: a row that says false needs an owner of its own";
        return false;
    }
    out = std::move(read);
    return true;
}

glm::dvec2 VanishDirection(const glm::dvec2& travellerPx, const glm::dvec2& portalPx) {
    // #2-13: objectPos - portalPos, then Normalize (GameMath.h:755-761) - sqrtf of
    // x x + y y, and zero where that is not above zero - in the script's floats.
    const float dx = static_cast<float>(travellerPx.x) - static_cast<float>(portalPx.x);
    const float dy = static_cast<float>(travellerPx.y) - static_cast<float>(portalPx.y);
    const float length = std::sqrt(dx * dx + dy * dy);
    if (!(length > 0.0f)) return glm::dvec2(0.0);
    return glm::dvec2(static_cast<double>(dx / length), static_cast<double>(dy / length));
}

float AngleDegrees(const Rule& rule, const glm::dvec2& direction) {
    if (rule.turn == Turn::Fixed) return static_cast<float>(rule.angleDeg);
    // #28-34: getAngle(direction), radianToDegree, SUBIf 90f - the same float
    // steps as Art::Shot::AngleDegrees (ETHCallback_projectile #102-111), and as
    // ETHCallback_enemy_shoot's #139-148 for along_flight_minus_90.
    constexpr float kPi = 3.141592654f; // gs2d::math::constant::PI
    float radians = Supersonic::DetMath::atan2(static_cast<float>(direction.x), static_cast<float>(direction.y));
    if (radians < 0.0f) radians += 2.0f * kPi;
    return radians / (kPi * 2.0f) * 360.0f - 90.0f;
}

Spawn Suck(const Rules& rules, const glm::dvec2& travellerPx, const glm::dvec2& portalPx) {
    return SpawnOf(rules.suck, portalPx, VanishDirection(travellerPx, portalPx));
}

Spawn Sparkles(const Rules& rules, const glm::dvec2& travellerPx, const glm::dvec2& portalPx) {
    return SpawnOf(rules.sparkles, travellerPx, VanishDirection(travellerPx, portalPx));
}

Spawn Appear(const Rules& rules, const glm::dvec2& exitPortalPx) {
    // appearEffect has no direction: its row's offset is 0 and its angle a literal.
    return SpawnOf(rules.appear, exitPortalPx, glm::dvec2(0.0));
}

Spawn Fireball(const Rules& rules, const glm::dvec2& atPx) {
    return SpawnOf(rules.fireball, atPx, glm::dvec2(0.0));
}

Spawn SmallExplosion(const Rules& rules, const glm::dvec2& atPx) {
    return SpawnOf(rules.smallExplosion, atPx, glm::dvec2(0.0));
}

Spawn LargeExplosion(const Rules& rules, const glm::dvec2& atPx) {
    return SpawnOf(rules.largeExplosion, atPx, glm::dvec2(0.0));
}

Spawn Summon(const Rules& rules, const glm::dvec2& atPx) { return SpawnOf(rules.summon, atPx, glm::dvec2(0.0)); }

Spawn Portal(const Rules& rules, const glm::dvec2& atPx) { return SpawnOf(rules.portal, atPx, glm::dvec2(0.0)); }

Spawn Death(const Rules& rules, const glm::dvec2& atPx) { return SpawnOf(rules.death, atPx, glm::dvec2(0.0)); }

bool ManagerColour(const PlacedPortal& rules, int index, int placed, int maxPortals, glm::dvec3& out) {
    // #185-188: more than one portal in the array colours the first two and
    // nothing else. #248-259: exactly one colours it red where the level grants
    // exactly one, and black - a halo that adds nothing - where it grants more.
    if (placed > 1) {
        if (index == 0) out = rules.colourFirst;
        else if (index == 1) out = rules.colourSecond;
        else return false;
        return true;
    }
    if (placed == 1 && index == 0) {
        out = maxPortals == 1 ? rules.colourFirst : rules.colourAlone;
        return true;
    }
    return false;
}

glm::dvec3 KillFade(const PlacedPortal& rules, const glm::dvec3& colour, double frameMs) {
    if (!(frameMs > 0.0) || !(rules.killFadeFrameMs > 0.0)) return colour;
    return colour * std::pow(rules.killFadePerFrame, frameMs / rules.killFadeFrameMs);
}

glm::dvec3 EyeOffset(const EyeRule& rule, int drawnFrame, bool noGravity) {
    // updateEyes #15-22: GetFrame() > 7 picks the right-hand x - the sheet's rows
    // 2 and 3, which are walking right and pushing right (art.json's DIRECTION
    // enum) - and #24-31 picks the y by the character's own noGravity flag.
    return glm::dvec3(drawnFrame > 7 ? rule.xFrameAboveSeven : rule.xElse, noGravity ? rule.yNoGravity : rule.y,
                      rule.z);
}

Spawn RedEyes(const Rules& rules, const glm::dvec2& characterPx, double characterZ, int drawnFrame, bool noGravity) {
    const glm::dvec3 offset = EyeOffset(rules.eyes, drawnFrame, noGravity);
    Spawn spawn;
    spawn.entity = rules.eyes.entity;
    // #33-64: SetPosition(entity.GetPosition() + g_scale.scale(vector3(eyeX, eyeY,
    // z))), so the z is the CHARACTER's own plus the offset's - not a depth of the
    // level's own.
    spawn.atPx = characterPx + glm::dvec2(offset);
    spawn.z = characterZ + offset.z;
    spawn.zScaled = true;
    spawn.angleDeg = 0.0f; // AddScaledEntity's own 0f
    spawn.scale = rules.eyes.scale;
    return spawn;
}

Spawn FinishSuck(const Rules& rules, const glm::dvec2& doorPx) {
    // #12-31: doorPos + vector2(0, scale(16)), in vector3(that, 0f), angle 0f.
    return SpawnOf(rules.finishSuck, doorPx, glm::dvec2(0.0));
}

Spawn FinishSparkles(const Rules& rules, const glm::dvec2& characterPx) {
    // #63-69: vector3(charPos, 0f), angle 0f.
    return SpawnOf(rules.finishSparkles, characterPx, glm::dvec2(0.0));
}

Spawn Blood(const Rules& rules, const glm::dvec2& victimPx, double victimZ) {
    Spawn spawn = SpawnOf(rules.blood, victimPx, glm::dvec2(0.0));
    // GetPosition() is the victim's vector3, so the depth is the victim's own - a
    // number already on the level's axis - and nothing the call writes.
    spawn.z = victimZ + rules.blood.z;
    spawn.zScaled = true;
    return spawn;
}

glm::dvec2 FallSkullPoint(const Rules& rules, const glm::dvec2& characterPx, const glm::dvec2& cameraPx,
                          const glm::dvec2& screenPx, bool noGravity) {
    const double inset = rules.skullInsetUnits;
    if (!noGravity) {
        // #341-361: vector2(charPos.x, screenSize.y + cameraPos.y - scale(12)).
        return glm::dvec2(characterPx.x, screenPx.y + cameraPx.y - inset);
    }
    // #278-289 screenMiddle = cameraPos + screenSize / 2; #291-311 skullPos =
    // max(min(charPos, cameraPos + screenSize), cameraPos), per component (min and
    // max are the script's own vector2 helpers); #313-336 skullPos -=
    // normalize(charPos - screenMiddle) x scale(12). normalize is the script's
    // float one, which VanishDirection already is.
    const glm::dvec2 middle = cameraPx + screenPx / 2.0;
    const glm::dvec2 held = glm::max(glm::min(characterPx, cameraPx + screenPx), cameraPx);
    return held - VanishDirection(characterPx, middle) * inset;
}

glm::dvec2 KillerSkullPoint(const Rules& rules, double minionX, const glm::dvec2& cameraPx,
                            const glm::dvec2& screenPx) {
    // #30-54: vector3(other.GetPosition().x, screenSize.y + cameraPos.y -
    // scale(12), 0f).
    return glm::dvec2(minionX, screenPx.y + cameraPx.y - rules.skullInsetUnits);
}

Spawn Skull(const Rules& rules, const glm::dvec2& atPx) {
    return SpawnOf(rules.skull, atPx, glm::dvec2(0.0));
}

// ---- chapter 3's ghost and its shots (the visuals plan's 12.2) ------------------

Spawn EnemyShoot(const Rules& rules, const glm::dvec2& shotPx, const glm::dvec2& direction) {
    return SpawnOf(rules.enemyShoot, shotPx, direction);
}

Spawn EnemyShootMark(const Rules& rules, const glm::dvec2& finalPx) {
    return SpawnOf(rules.enemyShootMark, finalPx, glm::dvec2(0.0));
}

Spawn EnemyShootFlash(const Rules& rules, const glm::dvec2& shotPx, double shotZUnits) {
    // GetPosition() is the shot's vector3: its depth is the shot's own, as blood's
    // is its victim's.
    Spawn spawn = SpawnOf(rules.enemyShootFlash, shotPx, glm::dvec2(0.0));
    spawn.z = shotZUnits + rules.enemyShootFlash.z;
    spawn.zScaled = true;
    return spawn;
}

Spawn EnemyShootBurst(const Rules& rules, const glm::dvec2& shotPx) {
    return SpawnOf(rules.enemyShootBurst, shotPx, glm::dvec2(0.0));
}

Spawn GhostAppear(const Rules& rules, const glm::dvec2& ghostPx, double ghostZUnits) {
    // vector3(currentPos, GetPositionZ() + 1f): the ghost's own depth, on the
    // level's axis, and the row's bare 1 over it.
    Spawn spawn = SpawnOf(rules.ghostAppear, ghostPx, glm::dvec2(0.0));
    spawn.zBaseUnits = ghostZUnits;
    return spawn;
}

glm::dvec2 FollowUp::Current(const FollowUpRule& rule) const {
    // PositionInterpolator::getCurrentPos (bytes 20104..20325): the far end once
    // the timer is over (elapsed > time), else the filter of the clamped fraction -
    // smoothEnd, which followUp sets on it (#29-38).
    if (!(rule.strideMs > 0.0) || elapsedMs > rule.strideMs) return to;
    const double bias = std::clamp(elapsedMs / rule.strideMs, 0.0, 1.0);
    const double eased = static_cast<double>(Supersonic::DetMath::sin(static_cast<float>(bias) * 1.570796327f));
    return from + (to - from) * eased;
}

glm::dvec2 FollowUp::Tick(const FollowUpRule& rule, const glm::dvec2& atPx, const glm::dvec2& targetPx,
                          double frameMs) {
    const glm::dvec2 destination = targetPx + rule.offsetUnits;
    // #1-67, the first call: an interpolator from where the entity stands to the
    // destination, and switchTime 0.
    if (!started) {
        from = atPx;
        to = destination;
        elapsedMs = 0.0;
        sinceAimedMs = 0.0;
        started = true;
    }
    // #93-143: aimed again from where it is now only when the destination is not
    // where the entity stands, and strictly after updateRate.
    if (destination != atPx && sinceAimedMs > rule.rateMs) {
        from = Current(rule);
        to = destination;
        elapsedMs = 0.0;
        sinceAimedMs = 0.0;
    }
    // #147-179: the frame's time to both clocks, then SetPositionXY where the
    // interpolator now says.
    const double step = std::max(frameMs, 0.0);
    sinceAimedMs += step;
    elapsedMs += step;
    return Current(rule);
}

} // namespace MagicPortals::Effects
