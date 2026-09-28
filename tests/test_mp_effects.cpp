// The effects the original's script adds: effects.json and sim/Effects.
//
// Each row of effects.json is one AddEntity call, decoded: which .ent, how far
// along the call's direction, at what depth, turned how and at what scale. The
// rows are pinned as read, and the arithmetic of vanishEffect and appearEffect -
// where the teleport's suck effect, sparkles and flash land and how they turn -
// is pinned against the same sums done here in doubles, on a real teleport of
// level 1-01 and on the four compass directions. The loading screen calls the
// same vanishEffect, and ui.json carries it for sim/Loading: the two are pinned
// to agree.
//
// Pure: it reads only the port's own committed data, so it runs anywhere and
// never skips.

#include "TestHarness.hpp"

#include "sim/Effects.hpp"
#include "sim/Loading.hpp"
#include "sim/Particles.hpp"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <vector>

using namespace MagicPortals;

namespace {

const std::string kEffects = std::string(MAGICPORTALS_PORT_DATA_DIR) + "/effects.json";
const std::string kUi = std::string(MAGICPORTALS_PORT_DATA_DIR) + "/ui.json";

constexpr double kPi = 3.14159265358979323846;

bool Near(double a, double b, double eps) {
    return std::fabs(a - b) <= eps;
}

std::string Num(double v) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.6f", v);
    return buffer;
}

std::string At(const glm::dvec2& p) {
    return "(" + Num(p.x) + ", " + Num(p.y) + ")";
}

Effects::Rules TheRules() {
    Effects::Rules rules;
    std::string error;
    CHECK_MSG(Effects::LoadRules(kEffects, rules, error), "effects.json reads: " + error);
    return rules;
}

// The sums the script does, in doubles: normalize(traveller - portal), and
// radianToDegree(getAngle(d)) - 90 with getAngle(v) = atan2(v.x, v.y) in [0, 2 pi).
glm::dvec2 Direction(const glm::dvec2& traveller, const glm::dvec2& portal) {
    const glm::dvec2 d = traveller - portal;
    const double length = std::sqrt(d.x * d.x + d.y * d.y);
    return length > 0.0 ? d / length : glm::dvec2(0.0);
}

double AngleOf(const glm::dvec2& direction) {
    double radians = std::atan2(direction.x, direction.y);
    if (radians < 0.0) radians += 2.0 * kPi;
    return radians * 180.0 / kPi - 90.0;
}

// ---- the rows -----------------------------------------------------------------

void TheRowsAreTheDecodedCalls() {
    const Effects::Rules rules = TheRules();

    // vanishEffect #36-69: AddEntity('suck_effect.ent', vector3(portal + dir x scale(32),
    // scale(32)), getAngle(dir) - 90, ..., getScale()).
    const Effects::Rule& suck = rules.suck;
    CHECK_MSG(suck.entity == "suck_effect.ent", "suck: " + suck.entity);
    CHECK_EQ(suck.offsetUnits, 32.0);
    CHECK_MSG(suck.z == 32.0 && suck.zScaled, "suck: z scale(32)");
    CHECK(suck.turn == Effects::Turn::TowardTravellerMinus90);
    CHECK_EQ(suck.scale, 1.0);

    // #83-112: AddEntity('sparkles.ent', vector3(objectPos, scale(32)), the same angle, ...).
    const Effects::Rule& sparkles = rules.sparkles;
    CHECK_MSG(sparkles.entity == "sparkles.ent", "sparkles: " + sparkles.entity);
    CHECK_EQ(sparkles.offsetUnits, 0.0);
    CHECK_MSG(sparkles.z == 32.0 && sparkles.zScaled, "sparkles: z scale(32)");
    CHECK(sparkles.turn == Effects::Turn::TowardTravellerMinus90);
    CHECK_EQ(sparkles.scale, 1.0);

    // appearEffect #2-26: AddEntity('appear_effect.ent', vector3(pos, 2), 0, ..., getScale()):
    // the 2 a bare literal, not through g_scale.scale.
    const Effects::Rule& appear = rules.appear;
    CHECK_MSG(appear.entity == "appear_effect.ent", "appear: " + appear.entity);
    CHECK_EQ(appear.offsetUnits, 0.0);
    CHECK_MSG(appear.z == 2.0 && !appear.zScaled, "appear: z a bare 2");
    CHECK(appear.turn == Effects::Turn::Fixed);
    CHECK_EQ(appear.angleDeg, 0.0);
    CHECK_EQ(appear.scale, 1.0);

    // addFireball #12-26: AddEntity('fireball.ent', vector3(pos, 0f), 0, ...,
    // getScale()) - the angle and the z both bare literals, the scale the
    // script's own 1. The 1.2 it is drawn at is the .ent's <Scale> and is not
    // here; nor is fireballScale, which scales its first particle system alone
    // and travels with the fireball.
    const Effects::Rule& fireball = rules.fireball;
    CHECK_MSG(fireball.entity == "fireball.ent", "fireball: " + fireball.entity);
    CHECK_EQ(fireball.offsetUnits, 0.0);
    CHECK_MSG(fireball.z == 0.0 && !fireball.zScaled, "fireball: z a bare 0");
    CHECK(fireball.turn == Effects::Turn::Fixed);
    CHECK_EQ(fireball.angleDeg, 0.0);
    CHECK_EQ(fireball.scale, 1.0);

    // ETHCallback_fireball #66-83: AddScaledEntity('small_explosion.ent',
    // thisEntity.GetPosition(), getScale()) - the fireball's own vector3, whose z
    // is the 0 addFireball gave it.
    const Effects::Rule& small = rules.smallExplosion;
    CHECK_MSG(small.entity == "small_explosion.ent", "small explosion: " + small.entity);
    CHECK_EQ(small.offsetUnits, 0.0);
    CHECK_MSG(small.z == 0.0 && !small.zScaled, "small explosion: the fireball's own z");
    CHECK(small.turn == Effects::Turn::Fixed);
    CHECK_EQ(small.angleDeg, 0.0);
    CHECK_EQ(small.scale, 1.0);

    // explode #1-23: AddScaledEntity('explosion_large.ent', vector3(pos,
    // scale(32)), scale(superScale)) - the z THROUGH g_scale.scale, unlike the
    // two above, and a barrel's superScale of 1.
    const Effects::Rule& large = rules.largeExplosion;
    CHECK_MSG(large.entity == "explosion_large.ent", "large explosion: " + large.entity);
    CHECK_EQ(large.offsetUnits, 0.0);
    CHECK_MSG(large.z == 32.0 && large.zScaled, "large explosion: z scale(32)");
    CHECK(large.turn == Effects::Turn::Fixed);
    CHECK_EQ(large.angleDeg, 0.0);
    CHECK_EQ(large.scale, 1.0);

    // And each is added where it is given and nowhere else: no offset, no turn.
    const glm::dvec2 at(123.5, -47.25);
    for (const Effects::Spawn& spawn :
         {Effects::Fireball(rules, at), Effects::SmallExplosion(rules, at), Effects::LargeExplosion(rules, at)}) {
        CHECK_MSG(spawn.atPx == at, spawn.entity + " is added at the point it is given");
        CHECK_EQ(static_cast<double>(spawn.angleDeg), 0.0);
        CHECK_EQ(spawn.scale, 1.0);
    }
    CHECK_EQ(Effects::Fireball(rules, at).z, 0.0);
    CHECK_EQ(Effects::SmallExplosion(rules, at).z, 0.0);
    CHECK_EQ(Effects::LargeExplosion(rules, at).z, 32.0);

    // MainCharacter's own: the constructor's AddScaledEntity (#85-101) and
    // updateEyes' offset (#15-31), every number in level units because the
    // vector3 overload of g_scale.scale multiplies all three components.
    const Effects::EyeRule& eyes = rules.eyes;
    CHECK_MSG(eyes.entity == "red_eyes.ent", "eyes: " + eyes.entity);
    CHECK_EQ(eyes.xFrameAboveSeven, 7.0);
    CHECK_EQ(eyes.xElse, -7.0);
    CHECK_EQ(eyes.y, -14.25);
    CHECK_EQ(eyes.yNoGravity, -15.65);
    CHECK_EQ(eyes.z, 1.0);
    CHECK_EQ(eyes.scale, 1.0);
    CHECK(eyes.hideWithPlayer);

    // The level's end and a death (the visuals plan's 11.3). levelFinishedEffect
    // #12-39: AddEntity('red_suck_effect.ent', vector3(doorPos + vector2(0f,
    // scale(16)), 0f), 0f, ..., getScale()) - the one fixed offset any row carries,
    // and not along a direction.
    const Effects::Rule& finishSuck = rules.finishSuck;
    CHECK_MSG(finishSuck.entity == "red_suck_effect.ent", "finish suck: " + finishSuck.entity);
    CHECK_EQ(finishSuck.offsetUnits, 0.0);
    CHECK_MSG(finishSuck.offsetFixedUnits == glm::dvec2(0.0, 16.0),
              "16 units below the door: " + At(finishSuck.offsetFixedUnits));
    CHECK_MSG(finishSuck.z == 0.0 && !finishSuck.zScaled, "finish suck: z a bare 0");
    CHECK(finishSuck.turn == Effects::Turn::Fixed && finishSuck.angleDeg == 0.0 && finishSuck.scale == 1.0);
    // #53-77: AddEntity('red_sparkles.ent', vector3(charPos, 0f), 0f, ..., getScale()).
    const Effects::Rule& finishSparkles = rules.finishSparkles;
    CHECK_MSG(finishSparkles.entity == "red_sparkles.ent", "finish sparkles: " + finishSparkles.entity);
    CHECK(finishSparkles.offsetUnits == 0.0 && finishSparkles.offsetFixedUnits == glm::dvec2(0.0));
    CHECK_MSG(finishSparkles.z == 0.0 && !finishSparkles.zScaled, "finish sparkles: z a bare 0");
    CHECK(finishSparkles.turn == Effects::Turn::Fixed && finishSparkles.angleDeg == 0.0 && finishSparkles.scale == 1.0);
    // AddScaledEntity('blood.ent', victim.GetPosition(), getScale()): the victim's own
    // vector3, so the row's z is 0 over the victim's depth.
    const Effects::Rule& blood = rules.blood;
    CHECK_MSG(blood.entity == "blood.ent", "blood: " + blood.entity);
    CHECK(blood.offsetUnits == 0.0 && blood.offsetFixedUnits == glm::dvec2(0.0));
    CHECK_MSG(blood.z == 0.0 && blood.zScaled, "blood: 0 over the victim's own depth");
    CHECK(blood.turn == Effects::Turn::Fixed && blood.angleDeg == 0.0 && blood.scale == 1.0);
    // AddScaledEntity('skull.ent', vector3(point, 0f), getScale()), the point held
    // g_scale.scale(12) inside the view.
    const Effects::Rule& skull = rules.skull;
    CHECK_MSG(skull.entity == "skull.ent", "skull: " + skull.entity);
    CHECK(skull.offsetUnits == 0.0 && skull.offsetFixedUnits == glm::dvec2(0.0));
    CHECK_MSG(skull.z == 0.0 && !skull.zScaled, "skull: z a bare 0");
    CHECK(skull.turn == Effects::Turn::Fixed && skull.angleDeg == 0.0 && skull.scale == 1.0);
    CHECK_EQ(rules.skullInsetUnits, 12.0);
}

// ---- the level's end and a death (the visuals plan's 11.3) ------------------------

void TheLevelsEndAndADeathLandWhereTheScriptPutsThem() {
    const Effects::Rules rules = TheRules();

    // levelFinishedEffect(charPos, doorPos): the suck 16 below the door, the
    // sparkles on the character, both unturned at the script's size.
    const glm::dvec2 door(736.0, 184.0);
    const glm::dvec2 character(712.5, 196.25);
    const Effects::Spawn suck = Effects::FinishSuck(rules, door);
    CHECK_MSG(suck.entity == "red_suck_effect.ent" && suck.atPx == glm::dvec2(736.0, 200.0) && suck.z == 0.0 &&
                  !suck.zScaled && suck.angleDeg == 0.0f && suck.scale == 1.0,
              "the suck at the door plus (0, 16): " + At(suck.atPx));
    const Effects::Spawn sparkles = Effects::FinishSparkles(rules, character);
    CHECK_MSG(sparkles.entity == "red_sparkles.ent" && sparkles.atPx == character && sparkles.z == 0.0 &&
                  !sparkles.zScaled && sparkles.angleDeg == 0.0f,
              "the sparkles on the character: " + At(sparkles.atPx));

    // Blood at the victim's own vector3: the character at z 4 / 2.8125 (art.json),
    // a marker's minion at 2 / 2.8125.
    for (const double victimZ : {4.0 / 2.8125, 2.0 / 2.8125}) {
        const Effects::Spawn blood = Effects::Blood(rules, character, victimZ);
        CHECK_MSG(blood.entity == "blood.ent" && blood.atPx == character && blood.z == victimZ && blood.zScaled &&
                      blood.angleDeg == 0.0f && blood.scale == 1.0,
                  "blood at the victim, its depth the victim's: z " + Num(blood.z));
    }

    // THE FALL, with gravity (checkGameLost #340-361): straight under the character
    // at the view's foot less 12 - on 1-06, a view 256 u tall whose top is at 0,
    // that is 244 u, 686.25 px at 720 (footage F13: 685.5).
    const glm::dvec2 screen(1280.0 / 2.8125, 256.0);
    const glm::dvec2 camera(0.0, 0.0);
    const glm::dvec2 fell(84.3, 402.0);
    const glm::dvec2 under = Effects::FallSkullPoint(rules, fell, camera, screen, false);
    CHECK_MSG(under == glm::dvec2(84.3, 244.0), "the skull under the fall: " + At(under));
    CHECK_MSG(Near(under.y * 2.8125, 686.25, 1e-9), "686.25 px at 720: " + Num(under.y * 2.8125));
    // Its x is the character's own and is NOT held inside the view.
    const glm::dvec2 wide = Effects::FallSkullPoint(rules, glm::dvec2(-30.0, 300.0), glm::dvec2(10.0, 20.0), screen, false);
    CHECK_MSG(wide == glm::dvec2(-30.0, 264.0), "x as it fell, y the view's foot: " + At(wide));

    // WEIGHTLESS (#273-339): the point held inside the view, then moved the inset
    // toward the view's middle along normalize(p - middle).
    const glm::dvec2 middle = camera + screen / 2.0;
    const auto weightless = [&](const glm::dvec2& at) {
        const glm::dvec2 held = glm::max(glm::min(at, camera + screen), camera);
        const glm::dvec2 d = at - middle;
        const double length = std::sqrt(d.x * d.x + d.y * d.y);
        return held - (length > 0.0 ? d / length : glm::dvec2(0.0)) * 12.0;
    };
    for (const glm::dvec2& at : {glm::dvec2(500.0, 128.0), glm::dvec2(100.0, 300.0), glm::dvec2(-40.0, -60.0),
                                 glm::dvec2(200.0, 100.0)}) {
        const glm::dvec2 got = Effects::FallSkullPoint(rules, at, camera, screen, true);
        const glm::dvec2 want = weightless(at);
        CHECK_MSG(Near(got.x, want.x, 1e-4) && Near(got.y, want.y, 1e-4),
                  "weightless from " + At(at) + ": " + At(got) + " for " + At(want));
    }
    // Off the right edge at the middle's height: held to the edge, then 12 back in.
    const glm::dvec2 right = Effects::FallSkullPoint(rules, glm::dvec2(500.0, 128.0), camera, screen, true);
    CHECK_MSG(Near(right.x, screen.x - 12.0, 1e-4) && Near(right.y, 128.0, 1e-4), "12 in from the edge: " + At(right));
    // At the middle itself normalize gives zero, so it is not moved at all.
    CHECK(Effects::FallSkullPoint(rules, middle, camera, screen, true) == middle);

    // A KILLER FLOOR's (enemy_killer #24-54): the minion's x and the view's foot,
    // whatever the level's gravity.
    const glm::dvec2 killer = Effects::KillerSkullPoint(rules, 321.5, glm::dvec2(100.0, 40.0), screen);
    CHECK_MSG(killer == glm::dvec2(321.5, 284.0), "under the minion, 12 over the view's foot: " + At(killer));
    const Effects::Spawn skull = Effects::Skull(rules, killer);
    CHECK_MSG(skull.entity == "skull.ent" && skull.atPx == killer && skull.z == 0.0 && !skull.zScaled &&
                  skull.angleDeg == 0.0f && skull.scale == 1.0,
              "skull.ent at its point, z a bare 0");
    std::printf("  the end and a death: suck %s, skull under a fall %s (%.2f px), weightless off the edge %s\n",
                At(suck.atPx).c_str(), At(under).c_str(), under.y * 2.8125, At(right).c_str());
}

// ---- the eyes -----------------------------------------------------------------

// updateEyes #15-31, cell by cell of dark_mage.ent's 4 x 4 sheet: rows 0 and 1
// (frames 0-7, pushing left and walking left) take -7, rows 2 and 3 (frames 8-15,
// walking right and pushing right) take +7. The y is the character's noGravity
// flag and nothing else, and the z is the same 1 either way.
void TheEyeSideIsTheCellTheCharacterIsDrawnOn() {
    const Effects::Rules rules = TheRules();
    for (int frame = 0; frame <= 15; ++frame) {
        const glm::dvec3 offset = Effects::EyeOffset(rules.eyes, frame, false);
        const double want = frame > 7 ? 7.0 : -7.0;
        CHECK_MSG(offset.x == want, "frame " + Num(frame) + " puts the eye at x " + Num(offset.x));
        CHECK_MSG(offset.y == -14.25 && offset.z == 1.0, "frame " + Num(frame) + " at y " + Num(offset.y));
        const glm::dvec3 weightless = Effects::EyeOffset(rules.eyes, frame, true);
        CHECK_MSG(weightless.y == -15.65, "frame " + Num(frame) + " weightless at y " + Num(weightless.y));
        CHECK_MSG(weightless.x == want && weightless.z == 1.0, "weightless takes the same x and z");
    }
    // The four cells the port actually draws the player on: the idle row 2 column
    // 0 a level starts with (art.json initial_direction), walking left, and the
    // two pushing rows, 0 facing left and 3 any other way (findFinalDirection).
    CHECK_EQ(Effects::EyeOffset(rules.eyes, 8, false).x, 7.0);   // idle and walking right
    CHECK_EQ(Effects::EyeOffset(rules.eyes, 4, false).x, -7.0);  // walking left
    CHECK_EQ(Effects::EyeOffset(rules.eyes, 3, false).x, -7.0);  // pushing left
    CHECK_EQ(Effects::EyeOffset(rules.eyes, 12, false).x, 7.0);  // pushing right
}

// The whole call, as the layer spawns it: the character's point plus the offset,
// the character's own depth plus the offset's z, turned 0 (AddScaledEntity passes
// 0f) and at the script's scale.
void TheEyesAreAddedOnTheCharacter() {
    const Effects::Rules rules = TheRules();
    const glm::dvec2 character(125.9, 83.8);
    // art.json's player depth: the decoded 4 over the level's scale factor at
    // 720 p (R10), which is what MagicPortalsLayer passes.
    const double characterZ = 4.0 / 2.8125;

    const Effects::Spawn right = Effects::RedEyes(rules, character, characterZ, 8, false);
    CHECK_MSG(right.entity == "red_eyes.ent", right.entity);
    CHECK_MSG(Near(right.atPx.x, character.x + 7.0, 1e-9) && Near(right.atPx.y, character.y - 14.25, 1e-9),
              "the eyes at " + At(right.atPx));
    CHECK_MSG(Near(right.z, characterZ + 1.0, 1e-9) && right.zScaled, "at z " + Num(right.z));
    CHECK_EQ(right.angleDeg, 0.0f);
    CHECK_EQ(right.scale, 1.0);

    const Effects::Spawn left = Effects::RedEyes(rules, character, characterZ, 4, false);
    CHECK_MSG(Near(left.atPx.x, character.x - 7.0, 1e-9), "facing left at x " + Num(left.atPx.x));
    CHECK_MSG(Near(left.atPx.x - right.atPx.x, -14.0, 1e-9), "the turn moves it 14 units");

    const Effects::Spawn weightless = Effects::RedEyes(rules, character, characterZ, 8, true);
    CHECK_MSG(Near(weightless.atPx.y, character.y - 15.65, 1e-9), "weightless at y " + Num(weightless.atPx.y));
    CHECK_MSG(Near(weightless.z, right.z, 1e-9), "and at the same depth");
}

// ---- the arithmetic -----------------------------------------------------------

// A real teleport: the player stepping into 1-01's red static portal 603, at
// (150, 74) (test_mp_statics), from (125.9, 83.8) - system_7's worked example,
// whose suck quad the visuals plan's step 7.3 gates at (120.4, 86.1) units.
void AVanishLandsWhereTheScriptPutsIt() {
    const Effects::Rules rules = TheRules();
    const glm::dvec2 traveller(125.9, 83.8);
    const glm::dvec2 portal(150.0, 74.0);
    const glm::dvec2 dir = Direction(traveller, portal);

    const glm::dvec2 got = Effects::VanishDirection(traveller, portal);
    CHECK_MSG(Near(got.x, dir.x, 1e-6) && Near(got.y, dir.y, 1e-6), "the direction " + At(got) + ", not " + At(dir));
    CHECK_MSG(Near(glm::length(got), 1.0, 1e-6), "of length 1");

    const Effects::Spawn suck = Effects::Suck(rules, traveller, portal);
    const glm::dvec2 wantSuck = portal + dir * 32.0;
    CHECK_MSG(suck.entity == "suck_effect.ent", suck.entity);
    CHECK_MSG(Near(suck.atPx.x, wantSuck.x, 1e-4) && Near(suck.atPx.y, wantSuck.y, 1e-4),
              "the suck effect at " + At(suck.atPx) + ", not " + At(wantSuck));
    CHECK_MSG(Near(suck.atPx.x, 120.357, 5e-4) && Near(suck.atPx.y, 86.054, 5e-4),
              "32 units from the portal toward where the player stood: " + At(suck.atPx));
    CHECK_MSG(glm::length(suck.atPx - portal) > 31.999 && glm::length(suck.atPx - portal) < 32.001,
              "on the portal's rim");
    CHECK_MSG(Near(suck.atPx.x, 120.4, 0.05) && Near(suck.atPx.y, 86.1, 0.05), "system_7's (120.4, 86.1)");
    const double angle = AngleOf(dir);
    CHECK_MSG(Near(suck.angleDeg, angle, 1e-3), "turned " + Num(suck.angleDeg) + ", not " + Num(angle));
    CHECK_MSG(Near(suck.angleDeg, 202.1286, 1e-3), "turned " + Num(suck.angleDeg));
    CHECK_MSG(suck.z == 32.0 && suck.zScaled && suck.scale == 1.0, "at z scale(32), scale 1");

    // The sparkles where the player stood - exactly, not near - turned the same.
    const Effects::Spawn sparkles = Effects::Sparkles(rules, traveller, portal);
    CHECK_MSG(sparkles.entity == "sparkles.ent", sparkles.entity);
    CHECK_MSG(sparkles.atPx == traveller, "the sparkles at " + At(sparkles.atPx));
    CHECK_MSG(sparkles.angleDeg == suck.angleDeg, "turned as the suck effect");
    CHECK_MSG(sparkles.z == 32.0 && sparkles.zScaled && sparkles.scale == 1.0, "at z scale(32), scale 1");

    // And the flash at the exit portal, unturned, a bare 2 deep.
    const glm::dvec2 exit(466.0, 150.0);
    const Effects::Spawn appear = Effects::Appear(rules, exit);
    CHECK_MSG(appear.entity == "appear_effect.ent", appear.entity);
    CHECK_MSG(appear.atPx == exit, "the flash at " + At(appear.atPx));
    CHECK_MSG(appear.angleDeg == 0.0f && appear.z == 2.0 && !appear.zScaled && appear.scale == 1.0,
              "unturned, at a bare z 2, scale 1");
}

// The four ways a traveller can stand from its portal, in Ethanon's angle
// (counter-clockwise on the screen, +y down): right 0, below -90, left 180,
// above 90. And one standing ON the portal: normalize gives zero, getAngle(0, 0)
// is 0, so the suck effect sits on the portal turned -90.
void TheTurnFollowsTheSideTheTravellerCameFrom() {
    const Effects::Rules rules = TheRules();
    const glm::dvec2 portal(200.0, 100.0);
    struct Case {
        glm::dvec2 offset;
        double angle;
        const char* name;
    };
    const Case cases[] = {
        {{10.0, 0.0}, 0.0, "right"},
        {{0.0, 10.0}, -90.0, "below"},
        {{-10.0, 0.0}, 180.0, "left"},
        {{0.0, -10.0}, 90.0, "above"},
    };
    for (const Case& c : cases) {
        const Effects::Spawn suck = Effects::Suck(rules, portal + c.offset, portal);
        const glm::dvec2 want = portal + c.offset / 10.0 * 32.0;
        CHECK_MSG(Near(suck.angleDeg, c.angle, 1e-4), std::string(c.name) + ": turned " + Num(suck.angleDeg));
        CHECK_MSG(Near(suck.atPx.x, want.x, 1e-9) && Near(suck.atPx.y, want.y, 1e-9),
                  std::string(c.name) + ": at " + At(suck.atPx));
    }
    const Effects::Spawn onIt = Effects::Suck(rules, portal, portal);
    CHECK_MSG(Effects::VanishDirection(portal, portal) == glm::dvec2(0.0), "no direction");
    CHECK_MSG(onIt.atPx == portal && onIt.angleDeg == -90.0f, "on the portal, turned -90: " + Num(onIt.angleDeg));
}

// The loading screen runs the same vanishEffect (LoadingScreen::loop #241), and
// ui.json's loading.vanish carries it for sim/Loading, which computes the angle in
// doubles where this computes it in the script's floats. Both files must say the
// same, and both sums must land in the same place.
void TheLoadingScreenIsTheSameCall() {
    const Effects::Rules rules = TheRules();
    Loading::Rules loading;
    std::string error;
    CHECK_MSG(Loading::LoadRules(kUi, loading, error), "ui.json's loading screen reads: " + error);
    CHECK_MSG(loading.vanish.suckEntity == rules.suck.entity, "the suck effect: " + loading.vanish.suckEntity);
    CHECK_MSG(loading.vanish.sparklesEntity == rules.sparkles.entity, "the sparkles: " + loading.vanish.sparklesEntity);
    CHECK_EQ(loading.vanish.suckOffsetUnits, rules.suck.offsetUnits);
    CHECK_EQ(loading.vanish.suckAngleOffsetDeg, -90.0); // the named rule's - 90

    const glm::dvec2 view(1280.0 / 2.8125, 720.0 / 2.8125);
    const glm::dvec2 character = Loading::CharacterAt(loading, view, Loading::LastLoadingFrame(loading));
    const glm::dvec2 portal = Loading::PortalAt(loading, view);
    const Effects::Spawn suck = Effects::Suck(rules, character, portal);
    const glm::dvec2 theirs = Loading::SuckAt(loading, view);
    CHECK_MSG(Near(suck.atPx.x, theirs.x, 1e-4) && Near(suck.atPx.y, theirs.y, 1e-4),
              "the suck effect at " + At(suck.atPx) + ", sim/Loading's " + At(theirs));
    CHECK_MSG(Near(suck.angleDeg, Loading::SuckAngleDeg(loading, view), 1e-4),
              "turned " + Num(suck.angleDeg) + ", sim/Loading's " + Num(Loading::SuckAngleDeg(loading, view)));
    CHECK(Effects::Sparkles(rules, character, portal).atPx == Loading::SparklesAt(loading, view));
}

// ---- the ring the exit portal flashes -----------------------------------------

// appear_effect.ent's one particle, which IS the flash: the entity carries no
// <Sprite> at all, so what a player sees at the exit portal is a single ring of
// onda.png shrinking from 133 units to 16 over half a second.
//
// The .ent states particles 1, allAtOnce 0, alphaMode 1, repeat 1, animationMode
// 1, lifeTime 500 with no spread, size 133 with no spread, growth -6.4, minSize
// 16, maxSize 991, its <StartPoint> at (0, 0, 10) and its colour (0.9, 0.7, 1) to
// black. The numbers are written here rather than read, so that this suite stays
// pure; the layer reads the file itself through Art::LoadEnt, and test_mp_layer
// pins the same numbers against the real one.
Particles::System TheAppearRing() {
    Particles::System ring;
    ring.bitmap = "onda.png";
    ring.count = 1;
    ring.allAtOnce = false;
    ring.alphaMode = Particles::kAlphaAdd;
    ring.additive = true;
    ring.animationMode = 1;
    ring.repeat = 1;
    ring.lifeTimeMs = 500.0;
    ring.size = 133.0;
    ring.growth = -6.4;
    ring.minSize = 16.0;
    ring.maxSize = 991.0;
    ring.startPointZ = 10.0;
    ring.colour0 = glm::dvec4(0.9, 0.7, 1.0, 1.0);
    ring.colour1 = glm::dvec4(0.0, 0.0, 0.0, 0.0);
    return ring;
}

// Every spread of this system is zero, so the middle of each range is the range.
double Midpoint(double from, double to) {
    return (from + to) / 2.0;
}

// The size at +29, +62, +96, +131 and +166 ms - the five frames of the recording
// rec11 that show the flash - which ETHParticleManager shrinks as
// 133 + growth x elapsed x 60/1000, clamped at minSize (:200-262, Particles::Step).
//
// MEASURED off rec11 (30 fps, variable frame times): 122, 112, 98, 85 and 76
// units, which system_7 G.4 gates at +/- 8. The recording confirms; the
// arithmetic is what is pinned.
void TheExitFlashShrinksAsTheEngineShrinksIt() {
    const Particles::System ring = TheAppearRing();
    const Particles::Owner owner{glm::dvec2(466.0, 150.0), 0.0};

    struct Frame {
        double atMs;
        double want;     // 133 - 6.4 x t/16.67
        double recorded; // rec11
    };
    const Frame frames[] = {{29.0, 121.9, 122.0}, {62.0, 109.2, 112.0}, {96.0, 96.1, 98.0},
                            {131.0, 82.7, 85.0}, {166.0, 69.3, 76.0}};
    for (const Frame& frame : frames) {
        std::vector<Particles::Particle> pool = Particles::MakePool(ring, ring.count, owner, Midpoint);
        CHECK_MSG(pool.size() == 1 && Near(pool.front().size, 133.0, 1e-9), "born at 133 units");
        Particles::Step(ring, pool.front(), 0, 1, owner, frame.atMs, false, Midpoint);
        const double size = pool.front().size;
        const double sum = 133.0 - 6.4 * frame.atMs * 60.0 / 1000.0;
        CHECK_MSG(Near(size, sum, 1e-9), "at +" + Num(frame.atMs) + " ms: " + Num(size) + ", not " + Num(sum));
        CHECK_MSG(Near(size, frame.want, 0.05), "at +" + Num(frame.atMs) + " ms: " + Num(size));
        CHECK_MSG(Near(size, frame.recorded, 8.0),
                  "within 8 of rec11's " + Num(frame.recorded) + ": " + Num(size));
        CHECK_MSG(Particles::Drawn(ring, pool.front(), false), "and drawn");
    }

    // The same shrink frame by frame: ten frames of a sixtieth take 6.4 off each,
    // so a step of ten frames' worth and ten steps of one agree to the bit.
    const double tick = 1000.0 / 60.0;
    std::vector<Particles::Particle> walked = Particles::MakePool(ring, ring.count, owner, Midpoint);
    for (int frame = 0; frame < 10; ++frame) {
        Particles::Step(ring, walked.front(), 0, 1, owner, tick, false, Midpoint);
    }
    std::vector<Particles::Particle> once = Particles::MakePool(ring, ring.count, owner, Midpoint);
    Particles::Step(ring, once.front(), 0, 1, owner, 10.0 * tick, false, Midpoint);
    CHECK_MSG(Near(walked.front().size, 69.0, 1e-9), "ten frames in: " + Num(walked.front().size));
    CHECK_MSG(Near(walked.front().size, once.front().size, 1e-9), "one step of ten frames says the same");

    // It stops at 16 rather than shrinking away - 133 - 6.4 x 19 is 11.4, and
    // minSize holds it - and it is over before the capture's +0.55 s: its one
    // life is 500 ms, and the frame that spends it draws nothing (Drawn refuses a
    // spent particle) and finishes the pool, so the runtime frees the entity. The
    // engine renews the particle on that frame even though it was its last
    // (:259), so its SIZE goes back to 133 there and the next frame's first test
    // spends it; nothing of it is drawn either frame.
    std::vector<Particles::Particle> life = Particles::MakePool(ring, ring.count, owner, Midpoint);
    int lastDrawn = 0;
    double clamped = 0.0;
    for (int frame = 1; frame <= 33; ++frame) { // 33 frames = 550 ms
        Particles::Step(ring, life.front(), 0, 1, owner, tick, false, Midpoint);
        if (frame == 19) clamped = life.front().size;
        if (Particles::Drawn(ring, life.front(), false)) lastDrawn = frame;
    }
    CHECK_MSG(Near(clamped, 16.0, 1e-9), "clamped at minSize 16 nineteen frames in: " + Num(clamped));
    CHECK_MSG(lastDrawn > 0 && lastDrawn * tick <= 550.0,
              "last drawn at frame " + std::to_string(lastDrawn) + ", " + Num(lastDrawn * tick) + " ms");
    CHECK_MSG(Particles::Finished(ring, life), "and the pool is over by +0.55 s");
    std::printf("  appear_effect: 133 u at birth, %.1f u ten frames in, last drawn at %.0f ms\n",
                walked.front().size, lastDrawn * tick);
}

// ---- a portal the player opened (the visuals plan's 7.4) -----------------------

void ThePortalsOwnThreeCallsArePointsAndNothingElse() {
    const Effects::Rules rules = TheRules();

    // insertPortal's two, in its order: the summon bubble at g_scale.scale(32)
    // and the portal itself at a bare 0. Both at the portal's point, both
    // unturned, both at the script's own size.
    const glm::dvec2 at(150.0, 74.0);
    const Effects::Spawn summon = Effects::Summon(rules, at);
    CHECK_MSG(summon.entity == "portal_summon_effect.ent" && summon.atPx == at && summon.z == 32.0 &&
                  summon.zScaled && summon.angleDeg == 0.0f && summon.scale == 1.0,
              "portal_summon_effect at the portal, z scale(32): " + At(summon.atPx) + " z " + Num(summon.z));
    const Effects::Spawn portal = Effects::Portal(rules, at);
    CHECK_MSG(portal.entity == "portal.ent" && portal.atPx == at && portal.z == 0.0 && !portal.zScaled &&
                  portal.angleDeg == 0.0f && portal.scale == 1.0,
              "portal.ent at the portal, z a bare 0: " + At(portal.atPx) + " z " + Num(portal.z));
    // And ETHCallback_portal's, at the entity's own vector3 - whose z is that
    // same bare 0 - on the frame it dies.
    const Effects::Spawn death = Effects::Death(rules, at);
    CHECK_MSG(death.entity == "portal_death.ent" && death.atPx == at && death.z == 0.0 && !death.zScaled &&
                  death.angleDeg == 0.0f && death.scale == 1.0,
              "portal_death at the portal, z a bare 0: " + At(death.atPx) + " z " + Num(death.z));
}

void TheManagerColoursTheFirstTwoAndBlanksALonePortal() {
    const Effects::PlacedPortal& rules = TheRules().placedPortal;
    // The decoded globals, which are also what 00_order 8.2 reads.
    CHECK_MSG(rules.colourFirst == glm::dvec3(1.0, 0.3, 0.4), "PORTAL_RED is (1, 0.3, 0.4)");
    CHECK_MSG(rules.colourSecond == glm::dvec3(0.3, 0.4, 1.0), "PORTAL_BLUE is (0.3, 0.4, 1)");
    CHECK_MSG(rules.colourAlone == glm::dvec3(0.0), "and a lone portal's is V3_ZERO");

    glm::dvec3 colour(-1.0);
    CHECK(Effects::ManagerColour(rules, 0, 2, 2, colour) && colour == rules.colourFirst);
    CHECK(Effects::ManagerColour(rules, 1, 2, 2, colour) && colour == rules.colourSecond);
    // #185-246 colour the first two of an array and no more.
    CHECK_MSG(!Effects::ManagerColour(rules, 2, 3, 3, colour), "nothing colours a third portal");
    // #282-300: one portal where the level grants two is black, which on an added
    // sprite is a halo that does not draw - what the original's 1-3 still shows.
    CHECK(Effects::ManagerColour(rules, 0, 1, 2, colour) && colour == rules.colourAlone);
    // #248-279: one portal where the level grants exactly one is red instead.
    CHECK(Effects::ManagerColour(rules, 0, 1, 1, colour) && colour == rules.colourFirst);
    CHECK_MSG(!Effects::ManagerColour(rules, 0, 0, 2, colour), "and an empty array colours nothing");
}

void TheKillFadeIsNineTenthsAFrame() {
    const Effects::PlacedPortal& rules = TheRules().placedPortal;
    CHECK_MSG(rules.killFadePerFrame == 0.9, "SetColor(GetColor() x 0.9) per frame");
    CHECK_MSG(Near(rules.killFadeFrameMs, 1000.0 / 60.0, 1e-9), "of 1/60 s of frame time (INFERRED)");
    CHECK_MSG(rules.deathAfterKillMs == 400.0, "and portal_death 400 ms after killTime");

    const glm::dvec3 red = rules.colourFirst;
    const glm::dvec3 one = Effects::KillFade(rules, red, rules.killFadeFrameMs);
    CHECK_MSG(Near(one.r, red.r * 0.9, 1e-12) && Near(one.g, red.g * 0.9, 1e-12) && Near(one.b, red.b * 0.9, 1e-12),
              "one frame of it takes nine tenths: " + Num(one.r));
    // Twenty-four frames is the 400 ms the death waits, and 0.9^24 is what is
    // left of the portal when its burst is added: about 8 % of what it was.
    glm::dvec3 walked = red;
    for (int i = 0; i < 24; ++i) walked = Effects::KillFade(rules, walked, rules.killFadeFrameMs);
    const glm::dvec3 once = Effects::KillFade(rules, red, 24.0 * rules.killFadeFrameMs);
    CHECK_MSG(Near(walked.r, once.r, 1e-9), "and a long frame is the same as its frames: " + Num(walked.r) + " for " +
                                                Num(once.r));
    CHECK_MSG(Near(walked.r / red.r, 0.079766, 1e-6), "0.9 ^ 24 is " + Num(walked.r / red.r));
    std::printf("  the kill fade: %.4f of its colour left when portal_death is added\n", walked.r / red.r);
    // A frame of nothing - a paused one - fades nothing.
    CHECK(Effects::KillFade(rules, red, 0.0) == red);
}

// ---- chapter 3's ghost and its shots (the visuals plan's 12.2) ------------------

void TheGhostsCallsAreItsOwn() {
    const Effects::Rules rules = TheRules();

    // addEnemyShoot #16-45: AddEntity('enemy_shoot.ent', vector3(casterPos, scale(10)),
    // ...), turned along its way by its own callback (#139-148).
    const Effects::Rule& shot = rules.enemyShoot;
    CHECK_MSG(shot.entity == "enemy_shoot.ent", "shot: " + shot.entity);
    CHECK_MSG(shot.offsetUnits == 0.0 && shot.offsetFixedUnits == glm::dvec2(0.0), "shot: no offset");
    CHECK_MSG(shot.z == 10.0 && shot.zScaled, "shot: z scale(10)");
    CHECK(shot.turn == Effects::Turn::AlongFlightMinus90 && shot.scale == 1.0);
    // #127-156: portal_mark.ent at vector3(finalPos, scale(32)), unturned.
    const Effects::Rule& mark = rules.enemyShootMark;
    CHECK_MSG(mark.entity == "portal_mark.ent" && mark.z == 32.0 && mark.zScaled && mark.turn == Effects::Turn::Fixed &&
                  mark.angleDeg == 0.0 && mark.scale == 1.0 && mark.offsetUnits == 0.0,
              "mark: portal_mark.ent at scale(32), unturned");
    // ETHCallback_enemy_shoot #57-71: projectile_shoot.ent at the shot's own vector3.
    const Effects::Rule& flash = rules.enemyShootFlash;
    CHECK_MSG(flash.entity == "projectile_shoot.ent" && flash.z == 0.0 && flash.zScaled &&
                  flash.turn == Effects::Turn::Fixed && flash.scale == 1.0,
              "flash: projectile_shoot.ent, 0 over the shot's depth");
    // #324-350: portal_fail_in_antiportal.ent at vector3(its point, 0f).
    const Effects::Rule& burst = rules.enemyShootBurst;
    CHECK_MSG(burst.entity == "portal_fail_in_antiportal.ent" && burst.z == 0.0 && !burst.zScaled &&
                  burst.turn == Effects::Turn::Fixed && burst.scale == 1.0,
              "burst: portal_fail_in_antiportal.ent at a bare 0");
    // ETHCallback_ghost's init #150-169: a BARE 1 over the ghost's own depth, and the
    // effect's own callback: followUp(ghost - scale(8, 8), 600, 100, true), Scale(1.005f).
    const Effects::Rule& appear = rules.ghostAppear;
    CHECK_MSG(appear.entity == "ghost_appear_effect.ent" && appear.z == 1.0 && !appear.zScaled &&
                  appear.turn == Effects::Turn::Fixed && appear.scale == 1.0,
              "appear: ghost_appear_effect.ent, a bare 1 over the ghost");
    CHECK_MSG(rules.ghostAppearFollow.offsetUnits == glm::dvec2(-8.0, -8.0) && rules.ghostAppearFollow.strideMs == 600.0 &&
                  rules.ghostAppearFollow.rateMs == 100.0 && rules.ghostAppearFollow.dontPause,
              "its followUp: 8 up and left of the ghost, 600 ms, every 100 ms, whatever the pause");
    CHECK_MSG(rules.ghostAppearScalePerFrame == static_cast<double>(1.005f), "and Scale(1.005f) every frame");

    // Where each lands.
    const glm::dvec2 at(612.25, 205.5);
    const glm::dvec2 dir(-0.6, -0.8);
    const Effects::Spawn fired = Effects::EnemyShoot(rules, at, dir);
    CHECK_MSG(fired.atPx == at && fired.z == 10.0 && fired.zScaled, "the shot at its own point, z 10");
    CHECK_MSG(std::fabs(static_cast<double>(fired.angleDeg) - AngleOf(dir)) < 1e-4,
              "turned getAngle(dir) - 90: " + Num(fired.angleDeg) + " for " + Num(AngleOf(dir)));
    const Effects::Spawn marked = Effects::EnemyShootMark(rules, glm::dvec2(400.0, 208.0));
    CHECK_MSG(marked.atPx == glm::dvec2(400.0, 208.0) && marked.z == 32.0 && marked.angleDeg == 0.0f,
              "the mark where the shot is going");
    const Effects::Spawn flashed = Effects::EnemyShootFlash(rules, at, 10.0);
    CHECK_MSG(flashed.atPx == at && flashed.z == 10.0 && flashed.zScaled && flashed.zBaseUnits == 0.0,
              "the flash at the shot's own vector3");
    const Effects::Spawn burst0 = Effects::EnemyShootBurst(rules, at);
    CHECK_MSG(burst0.atPx == at && burst0.z == 0.0 && !burst0.zScaled, "the burst at its point, a bare 0");
    const Effects::Spawn appeared = Effects::GhostAppear(rules, at, 2.0 / 2.8125 - 1.0);
    CHECK_MSG(appeared.atPx == at && appeared.z == 1.0 && !appeared.zScaled &&
                  std::fabs(appeared.zBaseUnits - (2.0 / 2.8125 - 1.0)) < 1e-12,
              "the puffs at the ghost's point, a bare 1 over its own depth");

    // followUp, a call at a time: the first eases from where it stands to the
    // destination by smoothEnd over 600 ms; it is not aimed again until more than
    // 100 ms have passed since, and then from where it is now.
    const Effects::FollowUpRule& follow = rules.ghostAppearFollow;
    Effects::FollowUp up;
    const glm::dvec2 start(640.0, 206.0);
    const glm::dvec2 ghost(640.0, 206.0);
    glm::dvec2 now = up.Tick(follow, start, ghost, 16.0);
    const glm::dvec2 dest = ghost + glm::dvec2(-8.0, -8.0);
    const double eased = std::sin(16.0 / 600.0 * kPi / 2.0);
    CHECK_MSG(glm::distance(now, start + (dest - start) * eased) < 1e-4,
              "one frame toward 8 up and left: " + At(now));
    // The ghost moves off; for 100 ms and one frame more the old ease runs on.
    const glm::dvec2 moved(620.0, 200.0);
    for (int frame = 0; frame < 6; ++frame) now = up.Tick(follow, now, moved, 16.0);
    const double eased7 = std::sin(7.0 * 16.0 / 600.0 * kPi / 2.0);
    CHECK_MSG(glm::distance(now, start + (dest - start) * eased7) < 1e-4, "still easing to the first destination");
    now = up.Tick(follow, now, moved, 16.0); // switchTime 112 > 100: aimed again from here
    CHECK_MSG(up.to == moved + glm::dvec2(-8.0, -8.0) && up.elapsedMs == 16.0,
              "aimed again after more than 100 ms, at the ghost's new point");
    // Aimed again every 112 ms while it has not arrived, each time from where it is:
    // a 600 ms ease restarted before it ends, so it closes in on the point rather
    // than landing on it - sin(112 / 600 x PI / 2) of what is left, each time.
    const glm::dvec2 there = moved + glm::dvec2(-8.0, -8.0);
    const double gap = glm::distance(now, there);
    bool closing = true;
    double last = gap;
    for (int frame = 0; frame < 63; ++frame) {
        now = up.Tick(follow, now, moved, 16.0);
        const double left = glm::distance(now, there);
        closing = closing && left < last;
        last = left;
    }
    CHECK_MSG(closing && last < gap * 0.1, "and closes on it, frame by frame: " + Num(gap) + " -> " + Num(last));
}

// ---- what is refused ----------------------------------------------------------

std::string Read(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

void ARowMissingAFactIsRefused() {
    const std::string good = Read(kEffects);
    CHECK_MSG(!good.empty(), "effects.json is where the build says: " + kEffects);
    std::error_code ec;
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "supersonic-test-mp-effects";
    std::filesystem::create_directories(dir, ec);
    const auto refused = [&](const char* name, const std::string& from, const std::string& to, const char* named) {
        std::string text = good;
        const std::size_t at = text.find(from);
        CHECK_MSG(at != std::string::npos, std::string(name) + ": the fixture finds '" + from + "'");
        if (at == std::string::npos) return;
        text.replace(at, from.size(), to);
        const std::string path = (dir / name).string();
        {
            std::ofstream out(path, std::ios::binary);
            out << text;
        }
        Effects::Rules rules;
        std::string why;
        const bool loaded = Effects::LoadRules(path, rules, why);
        CHECK_MSG(!loaded && why.find(named) != std::string::npos, std::string(name) + " is refused by name: " + why);
    };
    refused("no_sparkles.json", "\"sparkles\": {", "\"sparkles_gone\": {", "sparkles");
    refused("no_entity.json", "\"entity\": \"suck_effect.ent\",", "", "suck_effect.entity");
    refused("empty_entity.json", "\"entity\": \"suck_effect.ent\",", "\"entity\": \"\",", "suck_effect.entity");
    refused("no_offset.json", "\"offset_units\": 32,", "", "suck_effect.offset_units");
    refused("no_scaled.json", "\"z\": {\"value\": 32, \"scaled\": true},", "\"z\": {\"value\": 32},", "suck_effect.z");
    refused("bare_z.json", "\"z\": {\"value\": 2, \"scaled\": false},", "\"z\": 2,", "appear_effect.z");
    refused("sideways.json", "\"angle\": \"toward_traveller_minus_90\",", "\"angle\": \"sideways\",",
            "suck_effect.angle");
    refused("no_scale.json", "\"scale\": 1,\n    \"_source\": [\n      \"vanishEffect (Portal",
            "\"scale\": 0,\n    \"_source\": [\n      \"vanishEffect (Portal", "suck_effect.scale");
    refused("appear_turned.json", "\"angle\": 0,", "\"angle\": \"toward_traveller_minus_90\",", "appear_effect");
    refused("appear_offset.json", "\"offset_units\": 0,\n    \"z\": {\"value\": 2,",
            "\"offset_units\": 3,\n    \"z\": {\"value\": 2,", "appear_effect");
    refused("no_fireball.json", "\"fireball\": {", "\"fireball_gone\": {", "fireball");
    refused("no_small.json", "\"small_explosion\": {", "\"small_gone\": {", "small_explosion");
    refused("no_large.json", "\"explosion_large\": {", "\"large_gone\": {", "explosion_large");
    // The three point-only rows, each refused for the same reason appear_effect
    // is: a direction they are never given.
    refused("fireball_turned.json", "\"angle\": 0,\n    \"scale\": 1,\n    \"_source\": [\n      \"addFireball",
            "\"angle\": \"toward_traveller_minus_90\",\n    \"scale\": 1,\n    \"_source\": [\n      \"addFireball",
            "fireball");
    refused("large_offset.json", "\"entity\": \"explosion_large.ent\",\n    \"offset_units\": 0,",
            "\"entity\": \"explosion_large.ent\",\n    \"offset_units\": 5,", "explosion_large");
    refused("no_summon.json", "\"portal_summon_effect\": {", "\"summon_gone\": {", "portal_summon_effect");
    refused("no_portal.json", "\"portal\": {", "\"portal_gone\": {", "portal");
    refused("no_death.json", "\"portal_death\": {", "\"death_gone\": {", "portal_death");
    refused("no_placed.json", "\"placed_portal\": {", "\"placed_gone\": {", "placed_portal");
    refused("two_channels.json", "\"colour_first\": [1.0, 0.3, 0.4],", "\"colour_first\": [1.0, 0.3],",
            "placed_portal.colour_first");
    refused("no_fade.json", "\"kill_fade_per_frame\": 0.9,", "", "placed_portal.kill_fade_per_frame");
    refused("brightening.json", "\"kill_fade_per_frame\": 0.9,", "\"kill_fade_per_frame\": 1.0,",
            "placed_portal.kill_fade_per_frame");
    refused("no_eyes.json", "\"red_eyes\": {", "\"red_eyes_gone\": {", "red_eyes");
    refused("no_eye_x.json", "\"x_frame_gt_7\": 7,", "", "red_eyes.offset_units.x_frame_gt_7");
    refused("no_eye_zerog.json", "\"y_zero_g\": -15.65,", "", "red_eyes.offset_units.y_zero_g");
    refused("no_eye_z.json", "\"z\": 1\n    },", "\"z\": \"one\"\n    },", "red_eyes.offset_units.z");
    refused("eye_scale.json", "\"scale\": 1,\n    \"hide_with_player\"", "\"scale\": -1,\n    \"hide_with_player\"",
            "red_eyes.scale");
    refused("no_eye_hide.json", "\"hide_with_player\": true,", "", "red_eyes.hide_with_player");
    refused("eye_shown.json", "\"hide_with_player\": true,", "\"hide_with_player\": false,",
            "red_eyes.hide_with_player");
    // The level's end and a death (11.3): each row there, the pair only where the
    // decode put one, and the skull's inset.
    refused("no_finish_suck.json", "\"red_suck_effect\": {", "\"red_suck_gone\": {", "red_suck_effect");
    refused("no_finish_sparkles.json", "\"red_sparkles\": {", "\"red_sparkles_gone\": {", "red_sparkles");
    refused("no_blood.json", "\"blood\": {", "\"blood_gone\": {", "blood");
    refused("no_skull.json", "\"skull\": {", "\"skull_gone\": {", "skull");
    refused("short_pair.json", "\"offset_units\": [0, 16],", "\"offset_units\": [16],", "red_suck_effect.offset_units");
    refused("suck_turned.json", "\"entity\": \"red_suck_effect.ent\",\n    \"offset_units\": [0, 16],\n    \"z\": "
                                "{\"value\": 0, \"scaled\": false},\n    \"angle\": 0,",
            "\"entity\": \"red_suck_effect.ent\",\n    \"offset_units\": [0, 16],\n    \"z\": {\"value\": 0, "
            "\"scaled\": false},\n    \"angle\": \"toward_traveller_minus_90\",",
            "red_suck_effect");
    refused("paired_blood.json", "\"entity\": \"blood.ent\",\n    \"offset_units\": 0,",
            "\"entity\": \"blood.ent\",\n    \"offset_units\": [0, 16],", "blood");
    refused("paired_suck.json", "\"entity\": \"suck_effect.ent\",\n    \"offset_units\": 32,",
            "\"entity\": \"suck_effect.ent\",\n    \"offset_units\": [0, 32],", "suck_effect");
    // Blood's depth is its victim's own (planning step 97, 11.3's left-for-later):
    // the row's z is 0 and scaled, and neither a number nor a bare literal is taken.
    refused("blood_deeper.json", "\"entity\": \"blood.ent\",\n    \"offset_units\": 0,\n    \"z\": {\"value\": 0,",
            "\"entity\": \"blood.ent\",\n    \"offset_units\": 0,\n    \"z\": {\"value\": 2,", "blood.z");
    refused("blood_bare.json",
            "\"entity\": \"blood.ent\",\n    \"offset_units\": 0,\n    \"z\": {\"value\": 0, \"scaled\": true},",
            "\"entity\": \"blood.ent\",\n    \"offset_units\": 0,\n    \"z\": {\"value\": 0, \"scaled\": false},",
            "blood.z");
    // Chapter 3's ghost (12.2): each row, the shot's turn, the flash's depth over the
    // shot's, and the appear effect's own callback.
    refused("no_enemy_shoot.json", "\"enemy_shoot\": {", "\"enemy_shoot_gone\": {", "enemy_shoot");
    refused("no_enemy_mark.json", "\"enemy_shoot_mark\": {", "\"enemy_mark_gone\": {", "enemy_shoot_mark");
    refused("no_enemy_flash.json", "\"enemy_shoot_flash\": {", "\"enemy_flash_gone\": {", "enemy_shoot_flash");
    refused("no_enemy_burst.json", "\"enemy_shoot_burst\": {", "\"enemy_burst_gone\": {", "enemy_shoot_burst");
    refused("no_ghost_appear.json", "\"ghost_appear_effect\": {", "\"ghost_appear_gone\": {", "ghost_appear_effect");
    refused("shot_unturned.json", "\"angle\": \"along_flight_minus_90\",", "\"angle\": 0,", "enemy_shoot");
    refused("flash_bare.json",
            "\"entity\": \"projectile_shoot.ent\",\n    \"offset_units\": 0,\n    \"z\": {\"value\": 0, \"scaled\": true},",
            "\"entity\": \"projectile_shoot.ent\",\n    \"offset_units\": 0,\n    \"z\": {\"value\": 0, \"scaled\": false},",
            "enemy_shoot_flash.z");
    refused("no_follow.json", "\"follow_up\": {", "\"follow_gone\": {", "ghost_appear_effect");
    refused("no_grow.json", "\"scale_per_frame\": 1.0049999952316284,", "", "ghost_appear_effect");
    refused("no_inset.json", "\"inset_units\": 12,", "", "skull.inset_units");
    refused("no_room.json", "\"inset_units\": 12,", "\"inset_units\": 0,", "skull.inset_units");

    Effects::Rules rules;
    std::string why;
    CHECK(!Effects::LoadRules((dir / "no-such-file.json").string(), rules, why));
    CHECK(!why.empty());
}

} // namespace

int main() {
    TheRowsAreTheDecodedCalls();
    TheEyeSideIsTheCellTheCharacterIsDrawnOn();
    TheEyesAreAddedOnTheCharacter();
    AVanishLandsWhereTheScriptPutsIt();
    TheTurnFollowsTheSideTheTravellerCameFrom();
    TheLoadingScreenIsTheSameCall();
    TheExitFlashShrinksAsTheEngineShrinksIt();
    ThePortalsOwnThreeCallsArePointsAndNothingElse();
    TheManagerColoursTheFirstTwoAndBlanksALonePortal();
    TheKillFadeIsNineTenthsAFrame();
    TheLevelsEndAndADeathLandWhereTheScriptPutsThem();
    TheGhostsCallsAreItsOwn();
    ARowMissingAFactIsRefused();
    return ::test::summary("test_mp_effects", 60);
}
