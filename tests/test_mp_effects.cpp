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

#include "sim/Earthquake.hpp"
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

// ---- the shot and what its tap and its end leave (the visuals plan's 10.1) -------

void TheShotSideRowsAreTheDecodedCalls() {
    const Effects::Rules rules = TheRules();
    struct Row {
        const Effects::Rule* rule;
        const char* entity;
        double offset;
        double z;
        bool zScaled;
    };
    // Every one AddEntity or AddScaledEntity at the script's own size and angle
    // 0. The marks, the hourglass, the waves and the violet end stand at
    // g_scale.scale(32); the flash and the red end at the projectile's own bare 0.
    const Row rows[] = {
        {&rules.projectileShoot, "projectile_shoot.ent", 24.0, 0.0, false},
        {&rules.projectileShootReflected, "projectile_shoot.ent", 0.0, 0.0, false},
        {&rules.portalMark, "portal_mark.ent", 0.0, 32.0, true},
        {&rules.redCross, "red_cross.ent", 0.0, 32.0, true},
        {&rules.hourglass, "hourglass.ent", 0.0, 32.0, true},
        {&rules.redWaves, "red_waves.ent", 0.0, 32.0, true},
        {&rules.portalFail, "portal_fail.ent", 0.0, 32.0, true},
        {&rules.portalFailInField, "portal_fail_in_antiportal.ent", 0.0, 0.0, false},
    };
    for (const Row& row : rows) {
        const Effects::Rule& rule = *row.rule;
        CHECK_MSG(rule.entity == row.entity && rule.offsetUnits == row.offset && rule.z == row.z &&
                      rule.zScaled == row.zScaled && rule.turn == Effects::Turn::Fixed && rule.angleDeg == 0.0 &&
                      rule.scale == 1.0,
                  std::string(row.entity) + ": offset " + Num(rule.offsetUnits) + ", z " + Num(rule.z) +
                      (rule.zScaled ? " scaled" : " bare"));
    }
    // The point rows are added where they are given.
    const glm::dvec2 at(260.0, 194.8);
    const Effects::Spawn mark = Effects::At(rules.portalMark, at);
    CHECK_MSG(mark.entity == "portal_mark.ent" && mark.atPx == at && mark.z == 32.0 && mark.zScaled &&
                  mark.angleDeg == 0.0f && mark.scale == 1.0,
              "portal_mark at the tap, z scale(32): " + At(mark.atPx));
    const Effects::Spawn failed = Effects::At(rules.portalFailInField, at);
    CHECK_MSG(failed.atPx == at && failed.z == 0.0 && !failed.zScaled, "the red end at the point, z a bare 0");
    // And the flash 24 units out along the shot's way, unturned: a shot from
    // (41, 196) at a tap (160, 112) flashes at 24 x normalize(119, -84).
    const glm::dvec2 from(41.0, 196.0);
    const glm::dvec2 tap(160.0, 112.0);
    const glm::dvec2 d = Direction(tap, from);
    const Effects::Spawn flash = Effects::ProjectileShoot(rules, from, tap);
    CHECK_MSG(Near(flash.atPx.x, from.x + 24.0 * d.x, 1e-5) && Near(flash.atPx.y, from.y + 24.0 * d.y, 1e-5),
              "projectile_shoot 24 out along the shot: " + At(flash.atPx));
    CHECK_MSG(Near(glm::distance(flash.atPx, from), 24.0, 1e-5), "24 from where it left");
    CHECK(flash.angleDeg == 0.0f && flash.z == 0.0 && !flash.zScaled);
    // A shot with nowhere to go flashes where it is (normalize of zero is zero).
    CHECK(Effects::ProjectileShoot(rules, from, from).atPx == from);
    std::printf("  the shot's flash: %s for a shot from %s to %s\n", At(flash.atPx).c_str(), At(from).c_str(),
                At(tap).c_str());
}

// ---- what a crystal leaves where it was (the visuals plan's 10.2) -----------------

void TheCrystalRowsAddToTheCrystalsOwnPoint() {
    const Effects::Rules rules = TheRules();
    // pickCrystalEffect #1-32: AddScaledEntity('crystal_pick.ent', crystal.GetPosition() +
    // vector3(0, 0, scale(20)), getScale()); ETHCallback_timer #135-167: the same point,
    // AddScaledEntity('crystal_vanish.ent', ..., scale(1.3)).
    const Effects::Rule& pick = rules.crystalPick;
    const Effects::Rule& vanish = rules.crystalVanish;
    CHECK_MSG(pick.entity == "crystal_pick.ent" && pick.offsetUnits == 0.0 && pick.z == 20.0 && pick.zScaled &&
                  pick.turn == Effects::Turn::Fixed && pick.angleDeg == 0.0 && pick.scale == 1.0,
              "crystal_pick: z scale(20) on the crystal's own, angle 0, at the script's size: " + pick.entity);
    CHECK_MSG(vanish.entity == "crystal_vanish.ent" && vanish.offsetUnits == 0.0 && vanish.z == 20.0 &&
                  vanish.zScaled && vanish.turn == Effects::Turn::Fixed && vanish.angleDeg == 0.0 &&
                  vanish.scale == 1.3,
              "crystal_vanish: the same point, at scale(1.3): " + vanish.entity + " x " + Num(vanish.scale));

    // THE Z IS ADDED to the crystal's, not put in place of it: 1-22's crystal_ent_878
    // stands at (300, 210) with an eth_z of -6, so its burst is at 14; one at 0 at 20;
    // and one at the deepest a level places (-20) at 0.
    const glm::dvec2 at(300.0, 210.0);
    const Effects::Spawn taken = Effects::OnEntity(pick, at, -6.0);
    CHECK_MSG(taken.entity == "crystal_pick.ent" && taken.atPx == at && taken.z == 14.0 && taken.zScaled &&
                  taken.angleDeg == 0.0f && taken.scale == 1.0,
              "crystal_pick at the crystal, z -6 + 20 = 14: " + At(taken.atPx) + " z " + Num(taken.z));
    CHECK(Effects::OnEntity(pick, at, 0.0).z == 20.0);
    CHECK(Effects::OnEntity(pick, at, -20.0).z == 0.0);
    const Effects::Spawn gone = Effects::OnEntity(vanish, glm::dvec2(400.0, 208.0), 0.0);
    CHECK_MSG(gone.entity == "crystal_vanish.ent" && gone.atPx == glm::dvec2(400.0, 208.0) && gone.z == 20.0 &&
                  gone.zScaled && gone.angleDeg == 0.0f && gone.scale == 1.3,
              "crystal_vanish at 1-27's crystal_1264, z 20, x 1.3: " + At(gone.atPx) + " z " + Num(gone.z));
    // And At, which every other point row uses, would have put it at a flat 20:
    // the difference is the crystal's own depth.
    CHECK(Effects::At(pick, at).z == 20.0 && taken.z != Effects::At(pick, at).z);
    std::printf("  crystal_pick at %s z %s; crystal_vanish x %s\n", At(taken.atPx).c_str(), Num(taken.z).c_str(),
                Num(gone.scale).c_str());
}

// ---- what a key and its keyhole leave (the visuals plan's 10.3) -------------------

void TheKeyRowsAreTheDecodedCalls() {
    const Effects::Rules rules = TheRules();
    struct Row {
        const Effects::Rule* rule;
        const char* entity;
        double z;
        bool scaled;
    };
    // ETHCallback_key #768-799 and #599-629; ETHCallback_keyhole #150-164, #168-194
    // and #198-229: five AddScaledEntity calls, so none turned and none offset.
    const Row rows[] = {{&rules.keyPick, "key_pick.ent", 32.0, true},
                        {&rules.keyUnlockHalo, "key_unlock_halo.ent", 32.0, true},
                        {&rules.keyholeVanish, "keyhole_vanish.ent", 32.0, true},
                        {&rules.doorLockedVanish, "door_locked_vanish.ent", 2.0, false},
                        {&rules.wallDestroyAtDoor, "wall_destroy.ent", 0.0, true}};
    for (const Row& row : rows) {
        const Effects::Rule& rule = *row.rule;
        CHECK_MSG(rule.entity == row.entity && rule.offsetUnits == 0.0 && rule.z == row.z &&
                      rule.zScaled == row.scaled && rule.turn == Effects::Turn::Fixed && rule.angleDeg == 0.0 &&
                      rule.scale == 1.0,
                  std::string(row.entity) + ": z " + Num(rule.z) + (rule.zScaled ? " scaled" : " bare") +
                      ", angle 0, at getScale()");
    }
    CHECK_MSG(rules.keyUnlockHaloAfterMs == 666.0, "the halo on the fly-in's frame 1, w0's 666 ms: " +
                                                       Num(rules.keyUnlockHaloAfterMs));

    // 3-09 (level8b), whose five depths are all different: key_ent at (32, 32) z -6,
    // keyhole at (582, 198) z -14, its door at (624, 160) z -8.
    const glm::dvec2 key(32.0, 32.0);
    const glm::dvec2 hole(582.0, 198.0);
    const glm::dvec2 door(624.0, 160.0);
    const Effects::Spawn pick = Effects::OnEntity(rules.keyPick, key, -6.0);
    CHECK_MSG(pick.entity == "key_pick.ent" && pick.atPx == key && pick.z == 26.0 && pick.zScaled &&
                  pick.angleDeg == 0.0f,
              "key_pick at the key, -6 + 32 = 26: " + At(pick.atPx) + " z " + Num(pick.z));
    const Effects::Spawn burst = Effects::OnEntity(rules.keyholeVanish, hole, -14.0);
    CHECK_MSG(burst.atPx == hole && burst.z == 18.0, "keyhole_vanish at the keyhole, -14 + 32 = 18: " + Num(burst.z));
    const Effects::Spawn dust = Effects::OnEntity(rules.wallDestroyAtDoor, door, -8.0);
    CHECK_MSG(dust.entity == "wall_destroy.ent" && dust.atPx == door && dust.z == -8.0 && dust.angleDeg == 0.0f,
              "wall_destroy at the door's own point and depth, unturned: z " + Num(dust.z));
    // THE HALO'S Z IS ITS OWN: vector3(keyholePos, scale(32)), not added to the
    // keyhole's -14 - At, where OnEntity would have said 18.
    const Effects::Spawn halo = Effects::At(rules.keyUnlockHalo, hole);
    CHECK_MSG(halo.entity == "key_unlock_halo.ent" && halo.atPx == hole && halo.z == 32.0 && halo.zScaled,
              "the halo at the keyhole, z 32 of its own: " + Num(halo.z));
    CHECK(Effects::OnEntity(rules.keyUnlockHalo, hole, -14.0).z != halo.z);

    // THE DOOR'S SPARKLES: its -(-2) is a bare literal, so ONLY the 2 comes onto the
    // level's axis - 2 / 2.8125 = 0.7111 - and the door's own -8 stays as it is.
    constexpr double kFactor = 720.0 / 256.0;
    const Effects::Spawn sparkles = Effects::OnEntityBare(rules.doorLockedVanish, door, -8.0, kFactor);
    CHECK_MSG(sparkles.entity == "door_locked_vanish.ent" && sparkles.atPx == door && sparkles.zScaled &&
                  Near(sparkles.z, -8.0 + 2.0 / kFactor, 1e-12),
              "door_locked_vanish at the door, -8 + 0.7111: z " + Num(sparkles.z));
    // Dividing the WHOLE sum instead - what effectAdd does with a bare z - would have
    // put it at -2.13, six units in front of the door it belongs to.
    CHECK(!Near(sparkles.z, (-8.0 + 2.0) / kFactor, 1e-3));
    CHECK(Effects::OnEntityBare(rules.doorLockedVanish, door, -8.0, 0.0).z == -6.0);
    // Over the 41 doors' depths (D4_check: -8, -6, -4, -2, 0, +4) the sparkles stand
    // at door + 0.711, whose rounded slots are one in front of each door's: 00_order
    // 8.13's {-6 .. +6} is the same +2 read in raw units.
    const double doorZ[] = {-8.0, -6.0, -4.0, -2.0, 0.0, 4.0};
    const double slot[] = {-7.0, -5.0, -3.0, -1.0, 1.0, 5.0};
    for (std::size_t i = 0; i < 6; ++i) {
        const double z = Effects::OnEntityBare(rules.doorLockedVanish, door, doorZ[i], kFactor).z;
        CHECK_MSG(std::round(z) == slot[i], "a door at " + Num(doorZ[i]) + ": its sparkles at " + Num(z) +
                                                ", slot " + Num(std::round(z)));
    }
    std::printf("  3-09: key_pick z %s, halo z %s, keyhole_vanish z %s, door_locked_vanish z %s, wall_destroy z %s\n",
                Num(pick.z).c_str(), Num(halo.z).c_str(), Num(burst.z).c_str(), Num(sparkles.z).c_str(),
                Num(dust.z).c_str());
}

// The halo comes on the fly-in's frame 1: the tick the key finds its keyhole sets
// foundKeyhole, the next builds the manager with its clock at 0, and every later
// one adds its ms until w0's 666 are reached.
void TheUnlockHaloComesOnTheFlyInsFrameOne() {
    const Effects::Rules rules = TheRules();
    const auto haloTick = [&rules](double tickMs) {
        Effects::FlyIn clock;
        int added = 0;
        int on = -1;
        // Tick 0 is the unlock's own; the tick is counted from it.
        for (int tick = 0; tick < 400; ++tick) {
            if (Effects::AdvanceFlyIn(rules, clock, true, tickMs)) {
                ++added;
                if (on < 0) on = tick;
            }
        }
        CHECK_MSG(added == 1, "added once: " + std::to_string(added));
        return on;
    };
    const int at60 = haloTick(1000.0 / 60.0);
    CHECK_MSG(at60 == 41, "at 60 Hz, the 41st tick after the unlock: " + std::to_string(at60));
    const int at30 = haloTick(1000.0 / 30.0);
    CHECK_MSG(at30 == 21, "at 30 Hz, the 21st: " + std::to_string(at30));
    // The stride's test is m_time >= 666 (CMPu, JS): 6 frames of 111 ms reach it
    // exactly, 6 of 110 do not.
    CHECK_MSG(haloTick(111.0) == 1 + 6, "666 of 666 is frame 1");
    CHECK_MSG(haloTick(110.0) == 1 + 7, "660 is not");
    // A key not spent has no fly-in; its clock does not start.
    Effects::FlyIn idle;
    for (int tick = 0; tick < 100; ++tick) CHECK(!Effects::AdvanceFlyIn(rules, idle, false, 1000.0 / 60.0));
    CHECK(!idle.found && !idle.built && idle.ms == 0.0 && !idle.haloAdded);
    std::printf("  the unlock halo: %d ticks after the unlock at 60 Hz, %d at 30 Hz\n", at60, at30);
}

// What a diamond, a shock ring and a fire signal leave (the visuals plan's 10.4):
// eight calls, each pinned as read, and the arithmetic of each reading - on an
// entity's own depth, a bare 10 on one, a bare 10 of its own, and the strike's
// midpoint and its radians handed over as degrees.
void TheShockBurnAndExtinguishRowsAreTheDecodedCalls() {
    const Effects::Rules rules = TheRules();
    struct Row {
        const Effects::Rule* rule;
        const char* entity;
        double z;
        bool scaled;
    };
    // Seven AddScaledEntity calls - unturned, unoffset, at getScale() - and the
    // strike's AddEntity, below.
    const Row rows[] = {{&rules.shockDiamondPick, "shock_diamond_pick.ent", 0.0, true},
                        {&rules.shockDeath, "shock_death.ent", 0.0, true},
                        {&rules.shockDeathAtAgent, "shock_death.ent", 10.0, false},
                        {&rules.burnAtPickup, "burn_projectile.ent", 10.0, false},
                        {&rules.burnAtShot, "burn_projectile.ent", 10.0, false},
                        {&rules.extinguishAtDiamond, "extinguish_fire.ent", 0.0, true},
                        {&rules.extinguishAtSignal, "extinguish_fire.ent", 10.0, false}};
    for (const Row& row : rows) {
        const Effects::Rule& rule = *row.rule;
        CHECK_MSG(rule.entity == row.entity && rule.offsetUnits == 0.0 && rule.z == row.z &&
                      rule.zScaled == row.scaled && rule.turn == Effects::Turn::Fixed && rule.angleDeg == 0.0 &&
                      rule.scale == 1.0,
                  std::string(row.entity) + ": z " + Num(rule.z) + (rule.zScaled ? " scaled" : " bare") +
                      ", angle 0, at getScale()");
    }
    const Effects::Rule& strike = rules.shockStrike;
    CHECK_MSG(strike.entity == "shock_strike.ent" && strike.offsetUnits == 0.0 && strike.z == 0.0 &&
                  strike.zScaled && strike.turn == Effects::Turn::RadiansAsDegrees && strike.scale == 1.0,
              "shock_strike: at the midpoint, turned by getAngle's radians as they are, at getScale()");

    // On the entity's own depth: 3-16's diamond (204, 124), eth_z 0, and a knight
    // struck at the entity point of a body centred at (212, 124) - 2 / 2.8125 deep.
    constexpr double kFactor = 720.0 / 256.0;
    const glm::dvec2 diamond(204.0, 124.0);
    const double minionZ = 2.0 / kFactor;
    const Effects::Spawn pick = Effects::OnEntity(rules.shockDiamondPick, diamond, 0.0);
    CHECK_MSG(pick.entity == "shock_diamond_pick.ent" && pick.atPx == diamond && pick.z == 0.0 && pick.zScaled &&
                  pick.angleDeg == 0.0f,
              "shock_diamond_pick at the diamond, on its own depth: z " + Num(pick.z));
    const glm::dvec2 knight(212.0, 124.0 - 11.1);
    const Effects::Spawn death = Effects::OnEntity(rules.shockDeath, knight, minionZ);
    CHECK_MSG(death.atPx == knight && Near(death.z, 0.711111, 1e-6) && death.angleDeg == 0.0f,
              "shock_death at the knight, on its 0.711: " + Num(death.z));
    // THE STRIKE: the midpoint of the two points and of the two depths, turned by
    // atan2f(d.x, d.y) of d = normalize(enemy - diamond), wrapped into [0, 2 pi), and
    // handed over AS DEGREES - so at most 6.28 of them.
    const Effects::Spawn bolt = Effects::Strike(strike, diamond, 0.0, knight, minionZ);
    CHECK_MSG(bolt.entity == "shock_strike.ent" && Near(bolt.atPx.x, 208.0, 1e-12) &&
                  Near(bolt.atPx.y, 124.0 - 5.55, 1e-12) && Near(bolt.z, minionZ * 0.5, 1e-12) && bolt.zScaled,
              "shock_strike halfway, z halfway: " + At(bolt.atPx) + " z " + Num(bolt.z));
    // d = (8, -11.1): up and to the right, atan2(8, -11.1) = 2.5163 in (pi / 2, pi).
    double wantRadians = std::atan2(8.0, -11.1);
    if (wantRadians < 0.0) wantRadians += 2.0 * kPi;
    CHECK_MSG(Near(bolt.angleDeg, wantRadians, 1e-5) && bolt.angleDeg < 6.2832f,
              "turned by the radians, as degrees: " + Num(bolt.angleDeg));
    // The compass, as D4 section 5.3 gives it: an enemy below (+y) 0, to the right
    // pi / 2, above pi, to the left 3 pi / 2 - each a number of DEGREES.
    struct Compass {
        glm::dvec2 enemy;
        double want;
    };
    const Compass compass[] = {{{204.0, 150.0}, 0.0},
                               {{230.0, 124.0}, kPi * 0.5},
                               {{204.0, 100.0}, kPi},
                               {{180.0, 124.0}, kPi * 1.5}};
    for (const Compass& c : compass) {
        const float got = Effects::Strike(strike, diamond, 0.0, c.enemy, 0.0).angleDeg;
        CHECK_MSG(Near(got, c.want, 1e-5), "an enemy at " + At(c.enemy) + ": " + Num(got) + ", want " + Num(c.want));
    }
    // And the one it is not: the script's own "along the line" is
    // radianToDegree(getAngle(d)) - 90, which would turn a strike at an enemy to
    // the right by 0 degrees and one below by -90.
    Effects::Rule along = strike;
    along.turn = Effects::Turn::TowardTravellerMinus90;
    CHECK(Near(Effects::Strike(along, diamond, 0.0, glm::dvec2(204.0, 150.0), 0.0).angleDeg, -90.0, 1e-4));
    // Two points on one spot: normalize gives zero, getAngle 0.
    CHECK(Effects::Strike(strike, diamond, 0.0, diamond, 0.0).angleDeg == 0.0f);

    // A BARE 10 on an entity's depth: only the 10 comes onto the level's axis - 3-32's
    // fire diamond at eth_z -10 takes its burn at -10 + 3.5556 = -6.4444, and a shock
    // agent at 0 its shock_death at 3.5556.
    const Effects::Spawn burn = Effects::OnEntityBare(rules.burnAtPickup, glm::dvec2(356.0, 128.0), -10.0, kFactor);
    CHECK_MSG(burn.entity == "burn_projectile.ent" && burn.zScaled && Near(burn.z, -10.0 + 10.0 / kFactor, 1e-12),
              "burn_projectile at the fire diamond: " + Num(burn.z));
    const Effects::Spawn agent = Effects::OnEntityBare(rules.shockDeathAtAgent, glm::dvec2(224.0, 384.0), 0.0, kFactor);
    CHECK_MSG(agent.entity == "shock_death.ent" && Near(agent.z, 3.555556, 1e-6), "at the agent: " + Num(agent.z));
    // A BARE 10 of its own: At keeps the literal and says it is bare, for the
    // layer's effectAdd to bring onto the level's axis - burnProjectile's and the
    // fire signal's.
    const Effects::Spawn shot = Effects::At(rules.burnAtShot, glm::dvec2(100.0, 60.0));
    const Effects::Spawn water = Effects::At(rules.extinguishAtSignal, glm::dvec2(247.0, 141.0));
    CHECK_MSG(shot.z == 10.0 && !shot.zScaled && water.z == 10.0 && !water.zScaled &&
                  water.entity == "extinguish_fire.ent",
              "burn_projectile and the signal's water at a bare 10 of their own");
    const Effects::Spawn drained = Effects::OnEntity(rules.extinguishAtDiamond, glm::dvec2(384.0, 150.0), 0.0);
    CHECK_MSG(drained.entity == "extinguish_fire.ent" && drained.z == 0.0 && drained.zScaled,
              "the drained diamond's water on its own depth");
    std::printf("  10.4: strike at %s z %s turned %s; burn at pickup z %s; agent z %s\n", At(bolt.atPx).c_str(),
                Num(bolt.z).c_str(), Num(bolt.angleDeg).c_str(), Num(burn.z).c_str(), Num(agent.z).c_str());
}

// ---- what a wall, a stone and a lift door leave (the visuals plan's 11.1) -------

void TheWallStoneAndDoorRowsAreTheDecodedCalls() {
    const Effects::Rules rules = TheRules();
    constexpr double kFactor = 720.0 / 256.0;
    // THE WALL'S: the 6-argument AddEntity at the wall's own point and depth, turned
    // by the wall, at effectScale x getScale().
    const Effects::Rule& wall = rules.wallDestroy;
    CHECK_MSG(wall.entity == "wall_destroy.ent" && wall.offsetUnits == 0.0 && wall.z == 0.0 && wall.zScaled &&
                  wall.turn == Effects::Turn::EntityAngle && wall.scale == 1.0,
              "wall_destroy: on the wall's own GetPosition(), turned by its GetAngle(), at getScale()");
    CHECK_MSG(rules.wallDestroyScaleKey == "destroyEffectScale",
              "the wall's float: '" + rules.wallDestroyScaleKey + "'");
    // One arm computes effectScale for the dust and the shake (#14-31, #33-38 and
    // #73-76): earthquake.json names the same float.
    Earthquake::Rules quake;
    std::string quakeError;
    CHECK_MSG(Earthquake::LoadRules(std::string(MAGICPORTALS_PORT_DATA_DIR) + "/earthquake.json", quake, quakeError),
              "earthquake.json reads: " + quakeError);
    CHECK_MSG(quake.wallScaleKey == rules.wallDestroyScaleKey,
              "the shake's key is the dust's: '" + quake.wallScaleKey + "'");
    // #14-31: 1 unless the float is above 0.
    CHECK(Effects::EffectScale(0.0) == 1.0 && Effects::EffectScale(-3.0) == 1.0 && Effects::EffectScale(2.0) == 2.0 &&
          Effects::EffectScale(0.5) == 0.5);
    // 1-09's wall (320, 124), eth_z -2, unturned; 1-13's (192, 282) turned 1.5708
    // clockwise in the .tscn, the original's -90; 4-32's 2217 (32, 128), -2, at 2.
    const Effects::Spawn upright = Effects::WallDestroy(rules, glm::dvec2(320.0, 124.0), -2.0, 0.0, 1.0);
    CHECK_MSG(upright.entity == "wall_destroy.ent" && upright.atPx == glm::dvec2(320.0, 124.0) && upright.z == -2.0 &&
                  upright.zScaled && upright.angleDeg == 0.0f && upright.scale == 1.0,
              "an upright wall's dust at it, z -2, unturned, x1: " + At(upright.atPx) + " z " + Num(upright.z));
    const double turnedDeg = -1.5708 * 180.0 / kPi;
    const Effects::Spawn lying = Effects::WallDestroy(rules, glm::dvec2(192.0, 282.0), -2.0, turnedDeg, 1.0);
    CHECK_MSG(Near(lying.angleDeg, -90.0, 1e-3) && lying.atPx == glm::dvec2(192.0, 282.0),
              "a wall turned a quarter turns its dust with it: " + Num(lying.angleDeg));
    const Effects::Spawn big = Effects::WallDestroy(rules, glm::dvec2(32.0, 128.0), -2.0, 0.0, Effects::EffectScale(2.0));
    CHECK_MSG(big.scale == 2.0, "4-32's wall 2217 at twice the size: " + Num(big.scale));
    // Turned as the engine turns a particle (ETHParticleManager.cpp:489, :520): the
    // start line RandStartPoint (0, 101) of an upright wall runs down the screen; at
    // -90 it runs across it.
    const glm::dvec2 top = Particles::Turn(glm::dvec2(0.0, -50.5), lying.angleDeg);
    CHECK_MSG(Near(std::fabs(top.x), 50.5, 1e-3) && Near(top.y, 0.0, 1e-3),
              "the dust's line lies across the screen on a -90 wall: " + At(top));

    // THE STONE'S: AddScaledEntity at vector3(its GetPositionXY(), scale(32)), a depth
    // of its own, at scale(1.3).
    const Effects::Rule& stone = rules.stoneDestroy;
    CHECK_MSG(stone.entity == "rolling_stone_destroy.ent" && stone.offsetUnits == 0.0 && stone.z == 32.0 &&
                  stone.zScaled && stone.turn == Effects::Turn::Fixed && stone.angleDeg == 0.0 && stone.scale == 1.3,
              "rolling_stone_destroy: z 32 of its own, unturned, at 1.3");
    const Effects::Spawn puff = Effects::At(stone, glm::dvec2(64.0, 380.0));
    CHECK_MSG(puff.atPx == glm::dvec2(64.0, 380.0) && puff.z == 32.0 && puff.zScaled && puff.scale == 1.3,
              "at the stone, z 32: " + Num(puff.z));

    // THE DOOR'S TWO: AddScaledEntity at the door's GetPosition() -/+ (0, 0.5 h, 0),
    // unturned, its z the door's plus a bare 0.
    const Effects::Rule& open = rules.doorLiftOpen;
    const Effects::Rule& close = rules.doorLiftClose;
    for (const Effects::Rule* rule : {&open, &close}) {
        CHECK_MSG(rule->entity == "door_lift_particles.ent" && rule->offsetUnits == 0.0 && rule->z == 0.0 &&
                      !rule->zScaled && rule->turn == Effects::Turn::Fixed && rule->angleDeg == 0.0 &&
                      rule->scale == 1.0,
                  "door_lift_particles: a bare 0 on the door's depth, unturned, at getScale()");
    }
    CHECK_MSG(rules.doorLiftOpenSizeY == -0.5 && rules.doorLiftCloseSizeY == 0.5,
              "half the door's height up as it opens, down as it closes: " + Num(rules.doorLiftOpenSizeY) + ", " +
                  Num(rules.doorLiftCloseSizeY));
    // 3-15's door, 128 high, shut at (288, 176) eth_z -2 and raised 126 by the port.
    const Effects::Spawn opening = Effects::DoorLiftDust(rules, true, glm::dvec2(288.0, 176.0), -2.0, 128.0, kFactor);
    const Effects::Spawn closing =
        Effects::DoorLiftDust(rules, false, glm::dvec2(288.0, 176.0 - 126.0), -2.0, 128.0, kFactor);
    CHECK_MSG(opening.entity == "door_lift_particles.ent" && opening.atPx == glm::dvec2(288.0, 112.0) &&
                  opening.z == -2.0 && opening.zScaled && opening.angleDeg == 0.0f && opening.scale == 1.0,
              "opening: at its top edge, on its depth: " + At(opening.atPx) + " z " + Num(opening.z));
    CHECK_MSG(closing.atPx == glm::dvec2(288.0, 50.0 + 64.0) && closing.z == -2.0,
              "closing: at the raised door's bottom edge: " + At(closing.atPx));
    std::printf("  11.1: a -90 wall's dust turned %s, its line %s; 2217 x%s; the door's dust %s and %s\n",
                Num(lying.angleDeg).c_str(), At(top).c_str(), Num(big.scale).c_str(), At(opening.atPx).c_str(),
                At(closing.atPx).c_str());
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
    // The shot's rows (10.1): each required, the point rows refused a direction,
    // and the flash refused a turn it is never given.
    refused("no_mark.json", "\"portal_mark\": {", "\"portal_mark_gone\": {", "portal_mark");
    refused("no_fail_field.json", "\"portal_fail_in_antiportal\": {", "\"fail_field_gone\": {",
            "portal_fail_in_antiportal");
    refused("mark_offset.json", "\"entity\": \"portal_mark.ent\",\n    \"offset_units\": 0,",
            "\"entity\": \"portal_mark.ent\",\n    \"offset_units\": 3,", "portal_mark");
    refused("flash_turned.json", "\"offset_units\": 24,\n    \"z\": {\"value\": 0, \"scaled\": false},\n    \"angle\": 0,",
            "\"offset_units\": 24,\n    \"z\": {\"value\": 0, \"scaled\": false},\n    \"angle\": "
            "\"toward_traveller_minus_90\",",
            "projectile_shoot");

    // The crystal's two rows (10.2): each required, neither given a direction, and
    // neither z a bare literal - it is added to the crystal's own depth.
    refused("no_pick.json", "\"crystal_pick\": {", "\"crystal_pick_gone\": {", "crystal_pick");
    refused("no_vanish.json", "\"crystal_vanish\": {", "\"crystal_vanish_gone\": {", "crystal_vanish");
    refused("vanish_offset.json", "\"entity\": \"crystal_vanish.ent\",\n    \"offset_units\": 0,",
            "\"entity\": \"crystal_vanish.ent\",\n    \"offset_units\": 4,", "crystal_vanish");
    refused("pick_bare_z.json",
            "\"entity\": \"crystal_pick.ent\",\n    \"offset_units\": 0,\n    \"z\": {\"value\": 20, \"scaled\": true},",
            "\"entity\": \"crystal_pick.ent\",\n    \"offset_units\": 0,\n    \"z\": {\"value\": 20, \"scaled\": false},",
            "crystal_pick.z");

    // The key's five (10.3): each required, none given a direction or a turn, each
    // z read the one way the decode has it, and the halo's time required.
    refused("no_key_pick.json", "\"key_pick\": {", "\"key_pick_gone\": {", "key_pick");
    refused("no_door_vanish.json", "\"door_locked_vanish\": {", "\"door_locked_vanish_gone\": {",
            "door_locked_vanish");
    refused("no_door_dust.json", "\"wall_destroy_at_door\": {", "\"wall_destroy_gone\": {", "wall_destroy_at_door");
    refused("hole_offset.json", "\"entity\": \"keyhole_vanish.ent\",\n    \"offset_units\": 0,",
            "\"entity\": \"keyhole_vanish.ent\",\n    \"offset_units\": 4,", "keyhole_vanish");
    refused("door_dust_turned.json",
            "\"entity\": \"wall_destroy.ent\",\n    \"offset_units\": 0,\n    \"z\": {\"value\": 0, \"scaled\": true},\n"
            "    \"angle\": 0,",
            "\"entity\": \"wall_destroy.ent\",\n    \"offset_units\": 0,\n    \"z\": {\"value\": 0, \"scaled\": true},\n"
            "    \"angle\": 90,",
            "wall_destroy_at_door");
    refused("key_pick_bare_z.json",
            "\"entity\": \"key_pick.ent\",\n    \"offset_units\": 0,\n    \"z\": {\"value\": 32, \"scaled\": true},",
            "\"entity\": \"key_pick.ent\",\n    \"offset_units\": 0,\n    \"z\": {\"value\": 32, \"scaled\": false},",
            "key_pick.z");
    refused("door_vanish_scaled.json",
            "\"entity\": \"door_locked_vanish.ent\",\n    \"offset_units\": 0,\n    \"z\": {\"value\": 2, \"scaled\": false},",
            "\"entity\": \"door_locked_vanish.ent\",\n    \"offset_units\": 0,\n    \"z\": {\"value\": 2, \"scaled\": true},",
            "door_locked_vanish.z");
    refused("no_halo_time.json", "\"after_fly_in_ms\": 666,", "", "key_unlock_halo.after_fly_in_ms");
    // The 10.4 rows: each required, the strike turned by nothing but its radians,
    // the rest never turned, and each z read the one way the decode has it.
    refused("no_shock_pick.json", "\"shock_diamond_pick\": {", "\"shock_pick_gone\": {", "shock_diamond_pick");
    refused("no_strike.json", "\"shock_strike\": {", "\"shock_strike_gone\": {", "shock_strike");
    refused("no_signal_water.json", "\"extinguish_fire_at_signal\": {", "\"signal_water_gone\": {",
            "extinguish_fire_at_signal");
    refused("strike_along.json", "\"angle\": \"get_angle_radians_as_degrees\",",
            "\"angle\": \"toward_traveller_minus_90\",", "shock_strike");
    refused("strike_flat.json", "\"angle\": \"get_angle_radians_as_degrees\",", "\"angle\": 0,", "shock_strike");
    refused("death_turned.json",
            "\"entity\": \"shock_death.ent\",\n    \"offset_units\": 0,\n    \"z\": {\"value\": 0, \"scaled\": true},\n"
            "    \"angle\": 0,",
            "\"entity\": \"shock_death.ent\",\n    \"offset_units\": 0,\n    \"z\": {\"value\": 0, \"scaled\": true},\n"
            "    \"angle\": 90,",
            "shock_death");
    refused("burn_scaled.json",
            "\"entity\": \"burn_projectile.ent\",\n    \"offset_units\": 0,\n    \"z\": {\"value\": 10, \"scaled\": false},",
            "\"entity\": \"burn_projectile.ent\",\n    \"offset_units\": 0,\n    \"z\": {\"value\": 10, \"scaled\": true},",
            "burn_projectile_at_pickup.z");
    refused("pick_raised.json",
            "\"entity\": \"shock_diamond_pick.ent\",\n    \"offset_units\": 0,\n    \"z\": {\"value\": 0, \"scaled\": true},",
            "\"entity\": \"shock_diamond_pick.ent\",\n    \"offset_units\": 0,\n    \"z\": {\"value\": 32, \"scaled\": true},",
            "shock_diamond_pick.z");
    refused("halo_at_once.json", "\"after_fly_in_ms\": 666,", "\"after_fly_in_ms\": 0,",
            "key_unlock_halo.after_fly_in_ms");
    // The 11.1 rows: each required; the wall's turned by its wall and nothing
    // else, and its float named; the stone's and the door's never turned; each z
    // read the one way the decode has it; the door's shares signed as decoded.
    refused("no_wall_dust.json", "\"wall_destroy\": {", "\"wall_destroy_gone\": {", "wall_destroy");
    refused("no_stone_puff.json", "\"rolling_stone_destroy\": {", "\"stone_puff_gone\": {", "rolling_stone_destroy");
    refused("no_door_open.json", "\"door_lift_particles_open\": {", "\"door_open_gone\": {", "door_lift_particles_open");
    refused("no_door_close.json", "\"door_lift_particles_close\": {", "\"door_close_gone\": {",
            "door_lift_particles_close");
    refused("wall_flat.json", "\"angle\": \"entity_angle\",", "\"angle\": 0,", "wall_destroy");
    refused("no_wall_key.json", "\"scale_key\": \"destroyEffectScale\",", "", "wall_destroy.scale_key");
    refused("stone_turned.json", "\"z\": {\"value\": 32, \"scaled\": true},\n    \"angle\": 0,\n    \"scale\": 1.3,",
            "\"z\": {\"value\": 32, \"scaled\": true},\n    \"angle\": \"entity_angle\",\n    \"scale\": 1.3,",
            "rolling_stone_destroy");
    refused("stone_bare.json",
            "\"entity\": \"rolling_stone_destroy.ent\",\n    \"offset_units\": 0,\n    \"z\": {\"value\": 32, \"scaled\": "
            "true},",
            "\"entity\": \"rolling_stone_destroy.ent\",\n    \"offset_units\": 0,\n    \"z\": {\"value\": 32, \"scaled\": "
            "false},",
            "rolling_stone_destroy.z");
    refused("door_scaled.json",
            "\"entity\": \"door_lift_particles.ent\",\n    \"offset_units\": 0,\n    \"z\": {\"value\": 0, \"scaled\": false},",
            "\"entity\": \"door_lift_particles.ent\",\n    \"offset_units\": 0,\n    \"z\": {\"value\": 0, \"scaled\": true},",
            "door_lift_particles_open.z");
    refused("door_turned.json", "\"angle\": 0,\n    \"scale\": 1,\n    \"offset_size_y\": -0.5,",
            "\"angle\": 90,\n    \"scale\": 1,\n    \"offset_size_y\": -0.5,", "door_lift_particles_open");
    refused("door_open_down.json", "\"offset_size_y\": -0.5,", "\"offset_size_y\": 0.5,",
            "door_lift_particles_open.offset_size_y");
    refused("door_close_up.json", "\"offset_size_y\": 0.5,", "\"offset_size_y\": -0.5,",
            "door_lift_particles_close.offset_size_y");
    refused("no_door_share.json", "\"offset_size_y\": 0.5,", "", "door_lift_particles_close.offset_size_y");

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
    TheShotSideRowsAreTheDecodedCalls();
    TheCrystalRowsAddToTheCrystalsOwnPoint();
    TheKeyRowsAreTheDecodedCalls();
    TheUnlockHaloComesOnTheFlyInsFrameOne();
    TheShockBurnAndExtinguishRowsAreTheDecodedCalls();
    TheWallStoneAndDoorRowsAreTheDecodedCalls();
    ARowMissingAFactIsRefused();
    return ::test::summary("test_mp_effects", 60);
}
