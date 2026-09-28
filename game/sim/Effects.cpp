#include "sim/Effects.hpp"

#include "core/DetMath.hpp"
#include "core/Json.hpp"

#include <cmath>
#include <fstream>
#include <sstream>
#include <utility>

namespace MagicPortals::Effects {

namespace {

namespace Json = Supersonic::Json;

constexpr const char* kTowardTravellerMinus90 = "toward_traveller_minus_90";

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
    if (!Finite(row["offset_units"])) {
        error = where + ".offset_units is not a number";
        return false;
    }
    read.offsetUnits = row["offset_units"].AsNumber();
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
    } else {
        error = where + ".angle is a number or " + kTowardTravellerMinus90;
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
    spawn.atPx = fromPx + direction * rule.offsetUnits;
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
        !ReadEyes(root, "red_eyes", read.eyes, why) ||
        !ReadRule(root, "projectile_shoot", read.projectileShoot, why) ||
        !ReadRule(root, "projectile_shoot_reflected", read.projectileShootReflected, why) ||
        !ReadRule(root, "portal_mark", read.portalMark, why) || !ReadRule(root, "red_cross", read.redCross, why) ||
        !ReadRule(root, "hourglass", read.hourglass, why) || !ReadRule(root, "red_waves", read.redWaves, why) ||
        !ReadRule(root, "portal_fail", read.portalFail, why) ||
        !ReadRule(root, "portal_fail_in_antiportal", read.portalFailInField, why) ||
        !ReadRule(root, "crystal_pick", read.crystalPick, why) ||
        !ReadRule(root, "crystal_vanish", read.crystalVanish, why) ||
        !ReadRule(root, "key_pick", read.keyPick, why) ||
        !ReadRule(root, "key_unlock_halo", read.keyUnlockHalo, why) ||
        !ReadRule(root, "keyhole_vanish", read.keyholeVanish, why) ||
        !ReadRule(root, "door_locked_vanish", read.doorLockedVanish, why) ||
        !ReadRule(root, "wall_destroy_at_door", read.wallDestroyAtDoor, why)) {
        error = path + ": " + (why.empty() ? std::string("is not an object") : why);
        return false;
    }
    // The halo's one number beside its call: w0's time on the fly-in's clock. A
    // halo at no time would be added on the frame the manager is built, which is
    // a different frame from the one the waypoints say.
    const Json::Value& halo = root["key_unlock_halo"]["after_fly_in_ms"];
    if (!Finite(halo) || !(halo.AsNumber() > 0.0)) {
        error = path + ": key_unlock_halo.after_fly_in_ms is not a number above 0";
        return false;
    }
    read.keyUnlockHaloAfterMs = halo.AsNumber();
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
                     {"projectile_shoot_reflected", &read.projectileShootReflected},
                     {"portal_mark", &read.portalMark},
                     {"red_cross", &read.redCross},
                     {"hourglass", &read.hourglass},
                     {"red_waves", &read.redWaves},
                     {"portal_fail", &read.portalFail},
                     {"portal_fail_in_antiportal", &read.portalFailInField},
                     {"crystal_pick", &read.crystalPick},
                     {"crystal_vanish", &read.crystalVanish},
                     {"key_pick", &read.keyPick},
                     {"key_unlock_halo", &read.keyUnlockHalo},
                     {"keyhole_vanish", &read.keyholeVanish},
                     {"door_locked_vanish", &read.doorLockedVanish},
                     {"wall_destroy_at_door", &read.wallDestroyAtDoor}};
    for (const auto& row : pointOnly) {
        if (row.rule->turn != Turn::Fixed || row.rule->offsetUnits != 0.0) {
            error = path + ": " + row.name + " has no direction: its angle is a number and its offset_units 0";
            return false;
        }
    }
    // The key's five are AddScaledEntity calls, whose angle is a flat 0f
    // (ETHScriptWrapper.Scene.cpp:176-178): the door's wall_destroy in particular
    // is NOT turned by the door, as breakable_wall's call of the same entity is by
    // its wall.
    const struct {
        const char* name;
        const Rule* rule;
    } unturned[] = {{"key_pick", &read.keyPick},
                    {"key_unlock_halo", &read.keyUnlockHalo},
                    {"keyhole_vanish", &read.keyholeVanish},
                    {"door_locked_vanish", &read.doorLockedVanish},
                    {"wall_destroy_at_door", &read.wallDestroyAtDoor}};
    for (const auto& row : unturned) {
        if (row.rule->angleDeg != 0.0) {
            error = path + ": " + row.name + " is added by AddScaledEntity, whose angle is 0";
            return false;
        }
    }
    // The crystal's two rows add their z to the crystal's own depth, which is on
    // the level's axis: a bare literal there would be a sum of two units, and the
    // decode has g_scale.scale(20) for both, so a row that said otherwise is
    // refused rather than read half one way.
    const struct {
        const char* name;
        const Rule* rule;
    } onEntity[] = {{"crystal_pick", &read.crystalPick}, {"crystal_vanish", &read.crystalVanish}};
    for (const auto& row : onEntity) {
        if (!row.rule->zScaled) {
            error = path + ": " + row.name + ".z is added to the crystal's own depth, so it goes through "
                                              "g_scale.scale: scaled true";
            return false;
        }
    }
    // The key's rows each read their z one way, and the layer calls the reading
    // the decode has: OnEntity for the three whose z is g_scale.scale's (or none,
    // at the door) and At for the halo, whose scale(32) is a depth of its own;
    // OnEntityBare for door_locked_vanish's bare 2 (#173, no g_scale.scale between
    // #172 and #177). A row that said the other would be read half one way.
    const struct {
        const char* name;
        const Rule* rule;
        bool scaled;
    } keyDepths[] = {{"key_pick", &read.keyPick, true},
                     {"key_unlock_halo", &read.keyUnlockHalo, true},
                     {"keyhole_vanish", &read.keyholeVanish, true},
                     {"door_locked_vanish", &read.doorLockedVanish, false},
                     {"wall_destroy_at_door", &read.wallDestroyAtDoor, true}};
    for (const auto& row : keyDepths) {
        if (row.rule->zScaled != row.scaled) {
            error = path + ": " + row.name + ".z is " +
                    (row.scaled ? "g_scale.scale's in the decode: scaled true"
                                : "a bare literal in the decode (PshC4 -2f, no g_scale.scale): scaled false");
            return false;
        }
    }
    // projectile_shoot is offset along the shot's direction but never turned by
    // it: AddScaledEntity passes a flat 0f.
    if (read.projectileShoot.turn != Turn::Fixed) {
        error = path + ": projectile_shoot is added by AddScaledEntity, whose angle is 0: a number, not a rule";
        return false;
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
    // steps as Art::Shot::AngleDegrees (ETHCallback_projectile #102-111).
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

Spawn At(const Rule& rule, const glm::dvec2& atPx) { return SpawnOf(rule, atPx, glm::dvec2(0.0)); }

Spawn OnEntity(const Rule& rule, const glm::dvec2& entityPx, double entityZ) {
    // thisEntity.GetPosition() + vector3(0, 0, z): the xy the entity's own, and
    // the z the entity's plus the row's (pickCrystalEffect #6-23, ETHCallback_timer
    // #141-160). The entity's depth is already on the level's axis, and the row's
    // goes through g_scale.scale, so the sum keeps the row's own reading.
    Spawn spawn = SpawnOf(rule, entityPx, glm::dvec2(0.0));
    spawn.z = entityZ + rule.z;
    return spawn;
}

Spawn OnEntityBare(const Rule& rule, const glm::dvec2& entityPx, double entityZ, double scaleFactor) {
    // ETHCallback_keyhole #173-185: door.GetPosition() - vector3(0, 0, -2), the -2
    // a raw PshC4 with no g_scale.scale. The door's own z was scaled with the
    // level (its eth_z is on the level's axis); the literal was not, so it alone
    // comes onto that axis: 2 / 2.8125 = 0.711 at the 720 px screen of R12.
    Spawn spawn = SpawnOf(rule, entityPx, glm::dvec2(0.0));
    const double literal = scaleFactor > 0.0 ? rule.z / scaleFactor : rule.z;
    spawn.z = entityZ + literal;
    spawn.zScaled = true;
    return spawn;
}

bool AdvanceFlyIn(const Rules& rules, FlyIn& clock, bool spent, double tickMs) {
    if (!spent || clock.haloAdded) return false;
    // ETHCallback_key #1084-1093: the frame the key finds its keyhole only sets
    // foundKeyhole; #204-220 read it on the next.
    if (!clock.found) {
        clock.found = true;
        return false;
    }
    // #223-259 build the manager on that next frame, and FrameTimer::set leaves
    // its m_time at 0 there: a changed last frame resets it (#13-44).
    if (!clock.built) {
        clock.built = true;
        clock.ms = 0.0;
        return false;
    }
    // Every later frame adds its ms (#6-11), and m_time >= w0's stride steps it to
    // frame 1 (#46-57), on which #593-629 add the halo.
    clock.ms += tickMs;
    if (clock.ms < rules.keyUnlockHaloAfterMs) return false;
    clock.haloAdded = true;
    return true;
}

Spawn ProjectileShoot(const Rules& rules, const glm::dvec2& fromPx, const glm::dvec2& towardPx) {
    // The `direction` addProjectile stores: normalize(destiny - casterPos), in the
    // script's floats - vanishEffect's normalize, with the two points the other
    // way round.
    return SpawnOf(rules.projectileShoot, fromPx, VanishDirection(towardPx, fromPx));
}

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

} // namespace MagicPortals::Effects
