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

    Effects::Rules rules;
    std::string why;
    CHECK(!Effects::LoadRules((dir / "no-such-file.json").string(), rules, why));
    CHECK(!why.empty());
}

} // namespace

int main() {
    TheRowsAreTheDecodedCalls();
    AVanishLandsWhereTheScriptPutsIt();
    TheTurnFollowsTheSideTheTravellerCameFrom();
    TheLoadingScreenIsTheSameCall();
    ARowMissingAFactIsRefused();
    return ::test::summary("test_mp_effects", 60);
}
