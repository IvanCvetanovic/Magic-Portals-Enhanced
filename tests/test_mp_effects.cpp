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

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

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
    refused("no_eyes.json", "\"red_eyes\": {", "\"red_eyes_gone\": {", "red_eyes");
    refused("no_eye_x.json", "\"x_frame_gt_7\": 7,", "", "red_eyes.offset_units.x_frame_gt_7");
    refused("no_eye_zerog.json", "\"y_zero_g\": -15.65,", "", "red_eyes.offset_units.y_zero_g");
    refused("no_eye_z.json", "\"z\": 1\n    },", "\"z\": \"one\"\n    },", "red_eyes.offset_units.z");
    refused("eye_scale.json", "\"scale\": 1,\n    \"hide_with_player\"", "\"scale\": -1,\n    \"hide_with_player\"",
            "red_eyes.scale");
    refused("no_eye_hide.json", "\"hide_with_player\": true,", "", "red_eyes.hide_with_player");
    refused("eye_shown.json", "\"hide_with_player\": true,", "\"hide_with_player\": false,",
            "red_eyes.hide_with_player");

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
    ARowMissingAFactIsRefused();
    return ::test::summary("test_mp_effects", 60);
}
