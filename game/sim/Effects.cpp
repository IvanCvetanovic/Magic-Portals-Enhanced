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
constexpr const char* kRadiansAsDegrees = "get_angle_radians_as_degrees";
constexpr const char* kEntityAngle = "entity_angle";

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
    } else if (angle.IsString() && angle.AsString() == kRadiansAsDegrees) {
        read.turn = Turn::RadiansAsDegrees;
    } else if (angle.IsString() && angle.AsString() == kEntityAngle) {
        read.turn = Turn::EntityAngle;
    } else {
        error = where + ".angle is a number, " + kTowardTravellerMinus90 + ", " + kAlongFlightMinus90 + ", " +
                kRadiansAsDegrees + " or " + kEntityAngle;
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

// A boss's scatter row: the call's own facts as a Rule reads them, and the
// arithmetic of its random point and its clock, every one stated.
bool ReadScatter(const Json::Value& root, const char* name, Scatter& out, std::string& error) {
    Scatter read;
    if (!ReadRule(root, name, read.rule, error)) return false;
    const Json::Value& row = root[name];
    const std::string where = std::string(name);
    const auto pair = [](const Json::Value& value, glm::dvec2& into) {
        if (!value.IsArray() || value.AsArray().size() != 2 || !Finite(value.AsArray()[0]) ||
            !Finite(value.AsArray()[1])) {
            return false;
        }
        into = glm::dvec2(value.AsArray()[0].AsNumber(), value.AsArray()[1].AsNumber());
        return true;
    };
    if (!Finite(row["stride_ms"]) || !(row["stride_ms"].AsNumber() > 0.0) || !Finite(row["direction_span_rad"]) ||
        !(row["direction_span_rad"].AsNumber() > 0.0) || !Finite(row["length_from"]) ||
        row["length_from"].AsNumber() < 0.0 || !Finite(row["length_span"]) || row["length_span"].AsNumber() < 0.0 ||
        !pair(row["squash"], read.squash) || !(read.squash.x > 0.0 && read.squash.y > 0.0) ||
        !pair(row["from_units"], read.fromUnits)) {
        error = where + " needs stride_ms and direction_span_rad above 0, length_from and length_span from 0, a "
                        "squash of two numbers above 0 and from_units [x, y]";
        return false;
    }
    read.strideMs = row["stride_ms"].AsNumber();
    read.directionSpanRad = row["direction_span_rad"].AsNumber();
    read.lengthFrom = row["length_from"].AsNumber();
    read.lengthSpan = row["length_span"].AsNumber();
    // overTimeEntityAdder's AddEntity: the random point at a bare 0f, angle 0, and
    // Scale(getScale()); the radius along the random direction is offset_units.
    if (read.rule.turn != Turn::Fixed || read.rule.angleDeg != 0.0 || !(read.rule.offsetUnits > 0.0) ||
        read.rule.offsetFixedUnits != glm::dvec2(0.0) || read.rule.z != 0.0 || read.rule.zScaled ||
        read.rule.scale != 1.0) {
        error = where + " is AddEntity(name, vector3(pos, 0f), @e) and Scale(getScale()): angle 0, z 0 bare, "
                        "scale 1, and offset_units the radius g_scale.scale gives the random direction, above 0";
        return false;
    }
    out = std::move(read);
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
        !ReadRule(root, "enemy_shoot_flash", read.enemyShootFlash, why) ||
        !ReadRule(root, "enemy_shoot_burst", read.enemyShootBurst, why) ||
        !ReadRule(root, "ghost_appear_effect", read.ghostAppear, why) ||
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
        !ReadRule(root, "wall_destroy_at_door", read.wallDestroyAtDoor, why) ||
        !ReadRule(root, "shock_diamond_pick", read.shockDiamondPick, why) ||
        !ReadRule(root, "shock_death", read.shockDeath, why) ||
        !ReadRule(root, "shock_death_at_agent", read.shockDeathAtAgent, why) ||
        !ReadRule(root, "shock_strike", read.shockStrike, why) ||
        !ReadRule(root, "burn_projectile_at_pickup", read.burnAtPickup, why) ||
        !ReadRule(root, "burn_projectile", read.burnAtShot, why) ||
        !ReadRule(root, "extinguish_fire", read.extinguishAtDiamond, why) ||
        !ReadRule(root, "extinguish_fire_at_signal", read.extinguishAtSignal, why) ||
        !ReadRule(root, "wall_destroy", read.wallDestroy, why) ||
        !ReadRule(root, "rolling_stone_destroy", read.stoneDestroy, why) ||
        !ReadRule(root, "door_lift_particles_open", read.doorLiftOpen, why) ||
        !ReadRule(root, "door_lift_particles_close", read.doorLiftClose, why) ||
        !ReadRule(root, "light_from_projectile", read.lightFromProjectile, why) ||
        !ReadRule(root, "light_on_sparkles", read.lightOnSparkles, why) ||
        !ReadRule(root, "portal_death_at_lit_torch", read.portalDeathAtLitTorch, why) ||
        !ReadRule(root, "fire_signal", read.fireSignal, why) ||
        !ReadRule(root, "light_off_at_signal", read.lightOffAtSignal, why) ||
        !ReadRule(root, "light_off_sparkles", read.lightOffSparkles, why) ||
        !ReadRule(root, "portal_death_at_signal", read.portalDeathAtSignal, why) ||
        !ReadRule(root, "light_wall_create", read.lightWallCreate, why) ||
        !ReadRule(root, "light_wall_at_ghost", read.lightWallAtGhost, why) ||
        !ReadRule(root, "wall_destroy_light_wall", read.wallDestroyLightWall, why) ||
        !ReadRule(root, "movement_trail", read.movementTrail, why) ||
        !ReadRule(root, "roundabout_effect", read.roundabout, why) ||
        !ReadRule(root, "antiportal_explosion", read.antiportalExplosion, why) ||
        !ReadRule(root, "smoke_vanish", read.smokeVanish, why) ||
        !ReadScatter(root, "boss_scatter_beholder", read.scatterBeholder, why) ||
        !ReadScatter(root, "boss_scatter_ghost", read.scatterGhost, why) ||
        !ReadScatter(root, "boss_scatter_dark_dragon", read.scatterDarkDragon, why) ||
        !ReadRule(root, "ghost_utility_spawn", read.ghostUtilitySpawn, why) ||
        !ReadRule(root, "fire32", read.flames, why) ||
        !ReadRule(root, "carranca_destroy", read.carrancaDestroy, why) ||
        !ReadRule(root, "smoke_vanish_hand_tap", read.handTapSmoke, why)) {
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

    // The halo's one number beside its call: w0's time on the fly-in's clock. A
    // halo at no time would be added on the frame the manager is built, which is
    // a different frame from the one the waypoints say.
    const Json::Value& halo = root["key_unlock_halo"]["after_fly_in_ms"];
    if (!Finite(halo) || !(halo.AsNumber() > 0.0)) {
        error = path + ": key_unlock_halo.after_fly_in_ms is not a number above 0";
        return false;
    }
    read.keyUnlockHaloAfterMs = halo.AsNumber();
    // The wall's datum that scales its dust: the key ETHCallback_breakable_wall
    // reads (#16-26). Named in the data, not here, as earthquake.json names it for
    // the same arm's shake.
    const Json::Value& scaleKey = root["wall_destroy"]["scale_key"];
    if (!scaleKey.IsString() || scaleKey.AsString().empty()) {
        error = path + ": wall_destroy.scale_key is not the name of the wall's float";
        return false;
    }
    read.wallDestroyScaleKey = scaleKey.AsString();
    // The door's two: a share of its own height, up the screen as it opens
    // (vector3::opSub, #425) and down it as it closes (opAdd, #571). The signs are
    // the decode's finding, so a row that reads one the other way is refused.
    const struct {
        const char* name;
        double* into;
        double sign;
    } lifts[] = {{"door_lift_particles_open", &read.doorLiftOpenSizeY, -1.0},
                 {"door_lift_particles_close", &read.doorLiftCloseSizeY, 1.0}};
    for (const auto& lift : lifts) {
        const Json::Value& share = root[lift.name]["offset_size_y"];
        if (!Finite(share) || !(share.AsNumber() * lift.sign > 0.0)) {
            error = path + ": " + lift.name + ".offset_size_y is the share of the door's height it is moved by, " +
                    (lift.sign < 0.0 ? "up the screen as it opens (below 0)" : "down the screen as it closes (above 0)");
            return false;
        }
        *lift.into = share.AsNumber();
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
                     {"enemy_shoot_flash", &read.enemyShootFlash},
                     {"enemy_shoot_burst", &read.enemyShootBurst},
                     {"ghost_appear_effect", &read.ghostAppear},
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
                     {"wall_destroy_at_door", &read.wallDestroyAtDoor},
                     {"shock_diamond_pick", &read.shockDiamondPick},
                     {"shock_death", &read.shockDeath},
                     {"shock_death_at_agent", &read.shockDeathAtAgent},
                     {"burn_projectile_at_pickup", &read.burnAtPickup},
                     {"burn_projectile", &read.burnAtShot},
                     {"extinguish_fire", &read.extinguishAtDiamond},
                     {"extinguish_fire_at_signal", &read.extinguishAtSignal},
                     {"rolling_stone_destroy", &read.stoneDestroy},
                     {"door_lift_particles_open", &read.doorLiftOpen},
                     {"door_lift_particles_close", &read.doorLiftClose},
                     {"light_from_projectile", &read.lightFromProjectile},
                     {"light_on_sparkles", &read.lightOnSparkles},
                     {"portal_death_at_lit_torch", &read.portalDeathAtLitTorch},
                     {"fire_signal", &read.fireSignal},
                     {"light_off_at_signal", &read.lightOffAtSignal},
                     {"light_off_sparkles", &read.lightOffSparkles},
                     {"portal_death_at_signal", &read.portalDeathAtSignal},
                     {"movement_trail", &read.movementTrail},
                     {"roundabout_effect", &read.roundabout},
                     {"antiportal_explosion", &read.antiportalExplosion},
                     {"smoke_vanish", &read.smokeVanish}};
    for (const auto& row : pointOnly) {
        if (row.rule->turn != Turn::Fixed || row.rule->offsetUnits != 0.0) {
            error = path + ": " + row.name + " has no direction: its angle is a number and its offset_units 0";
            return false;
        }
    }
    // And a fixed offset is levelFinishedEffect's and 2-20's lesson hand's alone (its
    // smoke_vanish, 32 up): no other call adds one, so a pair on any other row is a
    // fact nobody decoded.
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
                     {"enemy_shoot_flash", &read.enemyShootFlash},
                     {"enemy_shoot_burst", &read.enemyShootBurst},
                     {"ghost_appear_effect", &read.ghostAppear},
                     {"projectile_shoot", &read.projectileShoot},
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
                     {"wall_destroy_at_door", &read.wallDestroyAtDoor},
                     {"shock_diamond_pick", &read.shockDiamondPick},
                     {"shock_death", &read.shockDeath},
                     {"shock_death_at_agent", &read.shockDeathAtAgent},
                     {"shock_strike", &read.shockStrike},
                     {"burn_projectile_at_pickup", &read.burnAtPickup},
                     {"burn_projectile", &read.burnAtShot},
                     {"extinguish_fire", &read.extinguishAtDiamond},
                     {"extinguish_fire_at_signal", &read.extinguishAtSignal},
                     {"wall_destroy", &read.wallDestroy},
                     {"rolling_stone_destroy", &read.stoneDestroy},
                     {"door_lift_particles_open", &read.doorLiftOpen},
                     {"door_lift_particles_close", &read.doorLiftClose},
                     {"light_from_projectile", &read.lightFromProjectile},
                     {"light_on_sparkles", &read.lightOnSparkles},
                     {"portal_death_at_lit_torch", &read.portalDeathAtLitTorch},
                     {"fire_signal", &read.fireSignal},
                     {"light_off_at_signal", &read.lightOffAtSignal},
                     {"light_off_sparkles", &read.lightOffSparkles},
                     {"portal_death_at_signal", &read.portalDeathAtSignal},
                     {"light_wall_create", &read.lightWallCreate},
                     {"light_wall_at_ghost", &read.lightWallAtGhost},
                     {"wall_destroy_light_wall", &read.wallDestroyLightWall},
                     {"movement_trail", &read.movementTrail},
                     {"roundabout_effect", &read.roundabout},
                     {"antiportal_explosion", &read.antiportalExplosion}};
    for (const auto& row : unshifted) {
        if (row.rule->offsetFixedUnits != glm::dvec2(0.0)) {
            error = path + ": " + row.name + " adds no fixed offset: its offset_units is a number";
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
                    {"wall_destroy_at_door", &read.wallDestroyAtDoor},
                    {"shock_diamond_pick", &read.shockDiamondPick},
                    {"shock_death", &read.shockDeath},
                    {"shock_death_at_agent", &read.shockDeathAtAgent},
                    {"burn_projectile_at_pickup", &read.burnAtPickup},
                    {"burn_projectile", &read.burnAtShot},
                    {"extinguish_fire", &read.extinguishAtDiamond},
                    {"extinguish_fire_at_signal", &read.extinguishAtSignal},
                    {"rolling_stone_destroy", &read.stoneDestroy},
                    {"door_lift_particles_open", &read.doorLiftOpen},
                    {"door_lift_particles_close", &read.doorLiftClose},
                    {"light_from_projectile", &read.lightFromProjectile},
                    {"light_on_sparkles", &read.lightOnSparkles},
                    {"portal_death_at_lit_torch", &read.portalDeathAtLitTorch},
                    {"fire_signal", &read.fireSignal},
                    {"light_off_at_signal", &read.lightOffAtSignal},
                    {"light_off_sparkles", &read.lightOffSparkles},
                    {"portal_death_at_signal", &read.portalDeathAtSignal},
                    {"movement_trail", &read.movementTrail},
                    {"roundabout_effect", &read.roundabout},
                    {"antiportal_explosion", &read.antiportalExplosion},
                    {"smoke_vanish", &read.smokeVanish}};
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
    // The 10.4 rows each read their z one way, and the layer calls the reading
    // the decode has: OnEntity where the z (0) goes on a diamond's or a victim's
    // own GetPosition(); OnEntityBare where a bare PshC4 10f goes on one (the
    // agent, the diamond a fire diamond is taken at); At where vector3(xy, 10)
    // is a depth of its own (burnProjectile, ETHCallback_fire_signal); and the
    // strike's midpoint, whose z is the two points' own. A row that said the
    // other would be read half one way.
    const struct {
        const char* name;
        const Rule* rule;
        bool scaled;
        double z;
    } burnDepths[] = {{"shock_diamond_pick", &read.shockDiamondPick, true, 0.0},
                      {"shock_death", &read.shockDeath, true, 0.0},
                      {"shock_death_at_agent", &read.shockDeathAtAgent, false, 10.0},
                      {"shock_strike", &read.shockStrike, true, 0.0},
                      {"burn_projectile_at_pickup", &read.burnAtPickup, false, 10.0},
                      {"burn_projectile", &read.burnAtShot, false, 10.0},
                      {"extinguish_fire", &read.extinguishAtDiamond, true, 0.0},
                      {"extinguish_fire_at_signal", &read.extinguishAtSignal, false, 10.0}};
    for (const auto& row : burnDepths) {
        if (row.rule->zScaled != row.scaled || row.rule->z != row.z) {
            error = path + ": " + row.name + ".z is " +
                    (row.scaled ? std::string("the entity's own GetPosition() z: 0, scaled true")
                                : std::string("a bare PshC4 10f in the decode: 10, scaled false"));
            return false;
        }
    }
    // The 11.1 rows each read their z one way, as the 10.4 rows do: the wall's
    // dust on the wall's own GetPosition() (no z of its own: OnEntity); the
    // stone's at vector3(xy, g_scale.scale(32)), a depth of its own (At); the
    // door's a bare PshC4 0f on the door's GetPosition() (OnEntityBare).
    const struct {
        const char* name;
        const Rule* rule;
        bool scaled;
        double z;
    } wallDepths[] = {{"wall_destroy", &read.wallDestroy, true, 0.0},
                      {"rolling_stone_destroy", &read.stoneDestroy, true, 32.0},
                      {"door_lift_particles_open", &read.doorLiftOpen, false, 0.0},
                      {"door_lift_particles_close", &read.doorLiftClose, false, 0.0}};
    for (const auto& row : wallDepths) {
        if (row.rule->zScaled != row.scaled || row.rule->z != row.z) {
            error = path + ": " + row.name + ".z is " +
                    (row.z == 32.0 ? std::string("g_scale.scale(32) of its own: 32, scaled true")
                     : row.scaled  ? std::string("the wall's own GetPosition() z: 0, scaled true")
                                   : std::string("a bare PshC4 0f on the door's GetPosition(): 0, scaled false"));
            return false;
        }
    }
    // The 11.2 rows each read their z one way: on the entity's own GetPosition()
    // (the torch's, the ghost's, the wall's, the character's: OnEntity or
    // OnEntityTurned, a 0 of g_scale's); at the spent shot's GetPosition(), whose z
    // is the bare 0 addProjectile and addFireball give it (At); at
    // vector3(originalPos, 10) - a bare PshC4 10f, a depth of its own (At); and at
    // the well's area, vector3(xy, -5) with a bare -5 (At). A row that said the
    // other would be read half one way.
    const struct {
        const char* name;
        const Rule* rule;
        bool scaled;
        double z;
    } torchDepths[] = {{"light_from_projectile", &read.lightFromProjectile, true, 0.0},
                       {"light_on_sparkles", &read.lightOnSparkles, true, 0.0},
                       {"portal_death_at_lit_torch", &read.portalDeathAtLitTorch, false, 0.0},
                       {"fire_signal", &read.fireSignal, true, 0.0},
                       {"light_off_at_signal", &read.lightOffAtSignal, false, 10.0},
                       {"light_off_sparkles", &read.lightOffSparkles, false, 10.0},
                       {"portal_death_at_signal", &read.portalDeathAtSignal, false, 0.0},
                       {"light_wall_create", &read.lightWallCreate, true, 0.0},
                       {"light_wall_at_ghost", &read.lightWallAtGhost, true, 0.0},
                       {"wall_destroy_light_wall", &read.wallDestroyLightWall, true, 0.0},
                       {"movement_trail", &read.movementTrail, true, 0.0},
                       {"roundabout_effect", &read.roundabout, false, -5.0}};
    for (const auto& row : torchDepths) {
        if (row.rule->zScaled != row.scaled || row.rule->z != row.z) {
            error = path + ": " + row.name + ".z is " +
                    (row.scaled ? std::string("the entity's own GetPosition() z: 0, scaled true")
                                : "a bare literal in the decode: " + std::to_string(static_cast<int>(row.z)) +
                                      ", scaled false");
            return false;
        }
    }
    // The three addLightWall and ETHCallback_light_wall add at an entity are each
    // turned by that entity's GetAngle() and offset by nothing.
    const struct {
        const char* name;
        const Rule* rule;
    } turned[] = {{"light_wall_create", &read.lightWallCreate},
                  {"light_wall_at_ghost", &read.lightWallAtGhost},
                  {"wall_destroy_light_wall", &read.wallDestroyLightWall}};
    for (const auto& row : turned) {
        if (row.rule->turn != Turn::EntityAngle || row.rule->offsetUnits != 0.0) {
            error = path + ": " + std::string(row.name) + " is turned by the entity's GetAngle() (" + kEntityAngle +
                    ") and offset by nothing";
            return false;
        }
    }
    // Every 11.2 call is at getScale(), one of the port's units: the torch's, the
    // wall's and the trail's pictures and systems are the .ent's own size.
    const Rule* const unscaled[] = {&read.lightFromProjectile, &read.lightOnSparkles, &read.portalDeathAtLitTorch,
                                    &read.fireSignal,          &read.lightOffAtSignal, &read.lightOffSparkles,
                                    &read.portalDeathAtSignal, &read.lightWallCreate, &read.lightWallAtGhost,
                                    &read.wallDestroyLightWall, &read.movementTrail, &read.roundabout};
    for (const Rule* rule : unscaled) {
        if (rule->scale != 1.0) {
            error = path + ": " + rule->entity + " is added at g_scale.getScale(): scale 1";
            return false;
        }
    }
    // The lit torch's bounce (bounceLightEntity(this, 1f)): from above 1 to below
    // it and back, on a stride of some time - a bounce that went nowhere, or
    // divided by no time, would be a picture of nothing decoded.
    {
        const Json::Value& bounce = root["light_from_projectile"]["bounce"];
        if (!bounce.IsObject() || !Finite(bounce["from"]) || !Finite(bounce["to"]) || !Finite(bounce["stride_ms"])) {
            error = path + ": light_from_projectile.bounce needs from, to and stride_ms";
            return false;
        }
        read.litTorchBounce.from = bounce["from"].AsNumber();
        read.litTorchBounce.to = bounce["to"].AsNumber();
        read.litTorchBounce.strideMs = bounce["stride_ms"].AsNumber();
        if (!(read.litTorchBounce.from > 1.0) || !(read.litTorchBounce.to < 1.0) ||
            !(read.litTorchBounce.to > 0.0) || !(read.litTorchBounce.strideMs > 0.0)) {
            error = path + ": light_from_projectile.bounce runs from above 1 to below it, above 0, on a stride above 0";
            return false;
        }
    }
    // The rebuilt wall's fade: SetAlpha(0), then up by unitsPerSecond(1).
    {
        const Json::Value& from = root["light_wall_at_ghost"]["alpha_from"];
        const Json::Value& rate = root["light_wall_at_ghost"]["alpha_per_second"];
        if (!Finite(from) || !(from.AsNumber() >= 0.0 && from.AsNumber() < 1.0) || !Finite(rate) ||
            !(rate.AsNumber() > 0.0)) {
            error = path + ": light_wall_at_ghost fades in: alpha_from in [0, 1) and alpha_per_second above 0";
            return false;
        }
        read.lightWallAlphaFrom = from.AsNumber();
        read.lightWallAlphaPerSecond = rate.AsNumber();
    }
    // The trail's two thresholds.
    {
        const Json::Value& speed = root["movement_trail"]["min_speed"];
        const Json::Value& every = root["movement_trail"]["every_ms"];
        if (!Finite(speed) || speed.AsNumber() < 0.0 || !Finite(every) || !(every.AsNumber() > 0.0)) {
            error = path + ": movement_trail needs min_speed at or above 0 and every_ms above 0";
            return false;
        }
        read.trailMinSpeed = speed.AsNumber();
        read.trailEveryMs = every.AsNumber();
    }
    // The wall's is the one call of the four that passes an angle - its wall's -
    // and it is offset by nothing; the other three are in the lists above.
    if (read.wallDestroy.turn != Turn::EntityAngle || read.wallDestroy.offsetUnits != 0.0) {
        error = path + ": wall_destroy is turned by its wall's GetAngle() (" + kEntityAngle + ") and offset by nothing";
        return false;
    }
    // The strike is the one AddEntity of them, at a midpoint and turned by the
    // radians getAngle gives: no offset, and no other turn.
    if (read.shockStrike.turn != Turn::RadiansAsDegrees || read.shockStrike.offsetUnits != 0.0) {
        error = path + ": shock_strike is turned by getAngle(normalize(enemy - diamond)) unconverted (" +
                kRadiansAsDegrees + ") and offset by nothing";
        return false;
    }
    // projectile_shoot is offset along the shot's direction but never turned by
    // it: AddScaledEntity passes a flat 0f.
    if (read.projectileShoot.turn != Turn::Fixed) {
        error = path + ": projectile_shoot is added by AddScaledEntity, whose angle is 0: a number, not a rule";
        return false;
    }
    // THE SMALLER PICTURES (planning step 146). explode()'s superScale for each of
    // its three boss callers: a scale of something, above 0.
    {
        const Json::Value& super = root["explode_super_scale"];
        const struct {
            const char* key;
            double* into;
        } callers[] = {{"beholder", &read.explodeBeholder},
                       {"ghost", &read.explodeGhost},
                       {"dark_dragon", &read.explodeDarkDragon}};
        for (const auto& caller : callers) {
            if (!super.IsObject() || !Finite(super[caller.key]) || !(super[caller.key].AsNumber() > 0.0)) {
                error = path + ": explode_super_scale." + caller.key + " is the superScale its explode() passes, above 0";
                return false;
            }
            *caller.into = super[caller.key].AsNumber();
        }
    }
    // ghost_utility_spawn, fire32 and carranca_destroy each stand on an entity's
    // own GetPosition() - the marker's, the target's, the carranca's - so their z
    // is that entity's, 0 of g_scale's over it, and none is offset; the first two
    // are unturned at the script's size, the third turned by its carranca.
    const struct {
        const char* name;
        const Rule* rule;
        Turn turn;
    } onOwn[] = {{"ghost_utility_spawn", &read.ghostUtilitySpawn, Turn::Fixed},
                 {"fire32", &read.flames, Turn::Fixed},
                 {"carranca_destroy", &read.carrancaDestroy, Turn::EntityAngle}};
    for (const auto& row : onOwn) {
        if (row.rule->turn != row.turn || row.rule->angleDeg != 0.0 || row.rule->offsetUnits != 0.0 ||
            row.rule->offsetFixedUnits != glm::dvec2(0.0) || row.rule->z != 0.0 || !row.rule->zScaled ||
            row.rule->scale != 1.0) {
            error = path + ": " + row.name + " stands on its entity's own GetPosition() (z 0, scaled true), offset by "
                                             "nothing, at scale 1, " +
                    (row.turn == Turn::Fixed ? std::string("unturned (angle 0)")
                                             : std::string("turned by its entity (") + kEntityAngle + ")");
            return false;
        }
    }
    {
        const Json::Value& width = root["fire32"]["width_units_per_scale"];
        if (!Finite(width) || !(width.AsNumber() > 0.0)) {
            error = path + ": fire32.width_units_per_scale is addFlames' 64, above 0";
            return false;
        }
        read.flamesWidthUnits = width.AsNumber();
    }
    // The hand's smoke: a fixed pair, turned by the hand at the call (not by the
    // row), on the hand's own depth, unturned itself; and the reach of a release.
    {
        const Rule& smoke = read.handTapSmoke;
        const Json::Value& reach = root["smoke_vanish_hand_tap"]["reach_units"];
        if (smoke.turn != Turn::Fixed || smoke.angleDeg != 0.0 || smoke.offsetUnits != 0.0 ||
            smoke.offsetFixedUnits == glm::dvec2(0.0) || smoke.z != 0.0 || !smoke.zScaled || smoke.scale != 1.0 ||
            !Finite(reach) || !(reach.AsNumber() > 0.0)) {
            error = path + ": smoke_vanish_hand_tap is a fixed pair turned by the hand, on its own depth (z 0, scaled "
                           "true), unturned, at scale 1, with a reach_units above 0";
            return false;
        }
        read.handTapReachUnits = reach.AsNumber();
        // ETHCallback_ + the entity less its .ent: whose placements it takes away.
        const Json::Value& entity = root["smoke_vanish_hand_tap"]["hand_entity"];
        if (!entity.IsString() || entity.AsString().empty() || entity.AsString().find('.') != std::string::npos) {
            error = path + ": smoke_vanish_hand_tap.hand_entity is the hand's entity, without .ent";
            return false;
        }
        read.handTapEntity = entity.AsString();
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
    // Along no direction: the caller hands over the entity's own (WallDestroy).
    if (rule.turn == Turn::EntityAngle) return 0.0f;
    // getAngle(direction): atan2f(v.x, v.y), and 2 pi more below 0
    // (GameMath.h:711-715; the .so's literal 6.2831855, which 2 x PI is exactly).
    constexpr float kPi = 3.141592654f; // gs2d::math::constant::PI
    float radians = Supersonic::DetMath::atan2(static_cast<float>(direction.x), static_cast<float>(direction.y));
    if (radians < 0.0f) radians += 2.0f * kPi;
    // ETHCallback_shock_diamond #496-498: pushed as the angle as it is.
    if (rule.turn == Turn::RadiansAsDegrees) return radians;
    // #28-34: radianToDegree, SUBIf 90f - the same float steps as
    // Art::Shot::AngleDegrees (ETHCallback_projectile #102-111), and as
    // ETHCallback_enemy_shoot's #139-148 for along_flight_minus_90.
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

Spawn Strike(const Rule& rule, const glm::dvec2& diamondPx, double diamondZ, const glm::dvec2& enemyPx, double enemyZ) {
    // #478-494: normalize(enemy.GetPositionXY() - diamond.GetPositionXY()), the
    // enemy the object of the subtraction (#486), in the script's floats; #499-513
    // (diamond.GetPosition() + enemy.GetPosition()) x 0.5, all three components.
    Spawn spawn = SpawnOf(rule, (diamondPx + enemyPx) * 0.5, glm::dvec2(0.0));
    spawn.z = (diamondZ + enemyZ) * 0.5 + rule.z;
    spawn.angleDeg = AngleDegrees(rule, VanishDirection(enemyPx, diamondPx));
    return spawn;
}

double EffectScale(double datum) {
    // #14 effectScale = 1; #28-31 CMPIf datum, 0f then JNP: taken only above 0.
    return datum > 0.0 ? datum : 1.0;
}

Spawn WallDestroy(const Rules& rules, const glm::dvec2& wallPx, double wallZ, double wallAngleDeg, double effectScale) {
    // #45-51: the wall's GetAngle() and GetPosition(), its depth included, and
    // #33-38 g_scale.scale(effectScale), which is effectScale of the port's units.
    Spawn spawn = OnEntity(rules.wallDestroy, wallPx, wallZ);
    spawn.angleDeg = static_cast<float>(wallAngleDeg);
    spawn.scale = rules.wallDestroy.scale * effectScale;
    return spawn;
}

Spawn OnEntityTurned(const Rule& rule, const glm::dvec2& entityPx, double entityZ, double entityAngleDeg) {
    // The 6-argument AddEntity with the entity's GetPosition() and GetAngle():
    // addLightWall #22-48 and #62-87 at the ghost, ETHCallback_light_wall #109-134
    // at the wall. The angle is Ethanon's degrees, handed over as they are.
    Spawn spawn = OnEntity(rule, entityPx, entityZ);
    spawn.angleDeg = static_cast<float>(entityAngleDeg);
    return spawn;
}

bool TrailDue(const Rules& rules, double& clockMs, double speed, double tickMs) {
    // #8-24: the clock runs every frame the update does, whatever the speed.
    clockMs += tickMs;
    // #44-60: both strictly above - the speed as the script's float against 0.4f -
    // and #82-95 the clock back to 0 on the one that adds.
    if (!(static_cast<float>(speed) > static_cast<float>(rules.trailMinSpeed)) || !(clockMs > rules.trailEveryMs)) {
        return false;
    }
    clockMs = 0.0;
    return true;
}

Spawn DoorLiftDust(const Rules& rules, bool opening, const glm::dvec2& doorPx, double doorZ, double doorHeightUnits,
                   double scaleFactor) {
    // #407-425 / #553-571: vector3(0, 0.5 x GetSize().y, 0) taken from or added to
    // the door's GetPosition() - screen y only, never rotated - its z a bare 0.
    const Rule& rule = opening ? rules.doorLiftOpen : rules.doorLiftClose;
    const double share = opening ? rules.doorLiftOpenSizeY : rules.doorLiftCloseSizeY;
    return OnEntityBare(rule, doorPx + glm::dvec2(0.0, share * doorHeightUnits), doorZ, scaleFactor);
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

bool AdderDue(const Scatter& scatter, double& clockMs, double tickMs) {
    // #34-44 AddToUInt(getLastFrameElapsedTime()), then #53-54 CMPu clock, stride
    // and JS past the add while it is below; #99-105 SetUInt(name, 0) on the add.
    clockMs += tickMs;
    if (clockMs < scatter.strideMs) return false;
    clockMs = 0.0;
    return true;
}

Spawn ScatterAt(const Scatter& scatter, const glm::dvec2& centrePx, double directionDraw, double lengthDraw) {
    // vector2(sin(randDir), -cos(randDir)) x g_scale.scale(radius) x randLen [x
    // squash], left to right as the script multiplies, in its floats; then added
    // to the centre - to the dark dragon's fixed offset from it first.
    const float direction = static_cast<float>(directionDraw);
    const float length = static_cast<float>(scatter.lengthFrom) + static_cast<float>(lengthDraw);
    float s = 0.0f;
    float c = 1.0f;
    Supersonic::DetMath::sincos(direction, s, c);
    const float radius = static_cast<float>(scatter.rule.offsetUnits);
    const float x = s * radius * length * static_cast<float>(scatter.squash.x);
    const float y = -c * radius * length * static_cast<float>(scatter.squash.y);
    // Given no direction, SpawnOf adds none of the row's offset_units (the radius,
    // used above): the point is the one worked out here.
    return SpawnOf(scatter.rule, centrePx + scatter.fromUnits + glm::dvec2(x, y), glm::dvec2(0.0));
}

Spawn Explode(const Rules& rules, const glm::dvec2& atPx, double superScale) {
    // #1-23: g_scale.scale(superScale), which is superScale of the port's units.
    Spawn spawn = LargeExplosion(rules, atPx);
    spawn.scale = rules.largeExplosion.scale * superScale;
    return spawn;
}

Spawn Flames(const Rules& rules, const glm::dvec2& targetPx, double targetZ, double targetWidthUnits) {
    // #2-39: getScale() x size / (getScale() x 64) - the width in units over 64,
    // the getScale()s cancelling into the port's one unit.
    Spawn spawn = OnEntity(rules.flames, targetPx, targetZ);
    spawn.scale = rules.flames.scale * targetWidthUnits / rules.flamesWidthUnits;
    return spawn;
}

bool HandTapped(const Rules& rules, const glm::dvec2& releasePx, const glm::dvec2& handPx) {
    // isPointInCircle: squaredDistance(p, pos) < radius x radius, TS - strictly.
    const glm::dvec2 d = releasePx - handPx;
    return d.x * d.x + d.y * d.y < rules.handTapReachUnits * rules.handTapReachUnits;
}

Spawn HandTapSmoke(const Rules& rules, const glm::dvec2& handPx, double handZ, double handAngleDeg) {
    // multiply(v, rotateZ(t)): (v.x a11 + v.y a12, v.x a21 + v.y a22) with a11 = cos,
    // a12 = sin, a21 = -sin, a22 = cos (GameMath.h:829-838, 857-870), in floats;
    // degreeToRadian of the hand's GetAngle().
    const float radians = static_cast<float>(handAngleDeg) * (3.141592654f / 180.0f);
    float s = 0.0f;
    float c = 1.0f;
    Supersonic::DetMath::sincos(radians, s, c);
    const float vx = static_cast<float>(rules.handTapSmoke.offsetFixedUnits.x);
    const float vy = static_cast<float>(rules.handTapSmoke.offsetFixedUnits.y);
    const glm::dvec2 turned(static_cast<double>(vx * c + vy * s), static_cast<double>(-vx * s + vy * c));
    Spawn spawn = OnEntity(rules.handTapSmoke, handPx, handZ);
    // OnEntity added the row's pair as it stands; the call adds it turned.
    spawn.atPx = handPx + turned;
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
