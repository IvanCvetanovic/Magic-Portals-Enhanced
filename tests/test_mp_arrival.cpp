// A traveller put down close under a platform is not pushed up into it (planning step 229: the beta tester's and the owner's 1-21, where the player
// ended inside two grey slabs after a portal was opened near a ceiling and walked into).
//
// What the engine does, which the suite shows and the port's Portals.cpp works round: a capsule whose AXIS runs through a hull's surface has no
// direction in the engine's contact (CollisionHull.cpp, CollideCapsuleHull, the two fallback normals), and it answers the world's UP whichever face
// that is. On a platform's top that is right; on its UNDERSIDE it pushes the capsule up into the platform. A portal opens in free air wherever the
// tap was, up to the platform's face, so a traveller can be put down with the top of his axis (9 above his centre) through the face. The fix lowers
// him to just under the face and the engine's ordinary contact does the rest (game/sim/Portals.cpp, LowerOutOfOverhead).
//
// level20 (1-21): single_block_plat_ent_862 (a hull, x 256-320, y 128-160, its corners chamfered 9.6) touches bridge01_ent_818 (a hull, x 320-576,
// y 128-192) at x 320, and under the first lies the pocket the plate is in: its floor single_block_plat_ent_828 has its top at y 224, where a
// player stands with his centre at y 206.4. level19 (1-20): platform_ent_1058, the plank over the alcove (x 220-476, y 112-144); the recorded
// route aims its third pair at (300, 168) because (300, 150) put him inside it.
//
// Run against the code before the fix, this suite fails on the arrivals whose exit is 0.5 to about 9 units under an underside (the 1-21 rows from
// y 160.5 to 169, the 1-20 rows from 144.5 to 153) and shows the tester's picture: he ends at (314.10, 142.22), his centre inside both slabs.
//
// Reads the converted levels and Enhanced's data from outside this repository, and skips (77), saying where it looked, when either is absent.

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "core/Components.hpp"
#include "sim/Game.hpp"
#include "sim/Portals.hpp"
#include "sim/Units.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <string>
#include <system_error>

using namespace MagicPortals;
using Supersonic::TransformComponent;

namespace {

const std::string kLevels = MAGICPORTALS_LEVELS_DIR;
const std::string kData = MAGICPORTALS_DATA_DIR;
constexpr float kStep = 1.0f / 60.0f;
// Long enough for a fall from the platform's underside to the pocket's floor, and a rest.
constexpr int kTicksAfter = 240;
// The most a traveller is put lower than the exit: the half of his axis (9 units) he can have inside the hull, and the margin Portals.cpp leaves under the face.
constexpr double kMostLoweredPx = 9.0 + 0.5;
// What the world's float metres lose on the way from pixels and back.
constexpr double kRoundingPx = 1e-3;

struct Run {
    Game::Data data;
    entt::registry registry;
    Game::Level level;
};

bool Begin(const std::string& levelName, Run& run) {
    std::string error;
    const bool ok = Game::LoadData(kLevels + "/" + levelName + ".tscn", kData,
                                   std::filesystem::temp_directory_path() / "supersonic-test-mp-arrival", run.data,
                                   error) &&
                    Game::Start(run.data, run.registry, run.level, error);
    CHECK_MSG(ok, levelName + ": " + error);
    return ok;
}

void Tick(Run& run) { Game::Tick(run.data, run.registry, run.level, 0.0f, kStep); }

glm::dvec2 PlayerPx(const Run& run) {
    return Units::ToPixels(run.registry.get<TransformComponent>(run.level.player).position);
}

std::string Px(const glm::dvec2& at) {
    char text[48];
    std::snprintf(text, sizeof text, "(%.2f, %.2f)", at.x, at.y);
    return text;
}

// One arrival, `x` and `y` being where the exit portal is opened: the player, settled at his spawn, is taken through a pair whose far end is there.
struct Outcome {
    bool began = false;
    int traversals = 0;
    double leastY = std::numeric_limits<double>::infinity(); // the highest his centre gets from the traversal on (+y down)
    glm::dvec2 exitPx{0.0};                                   // the exit as the traversal tick had it (the portal hovers a little)
    glm::dvec2 arrival{0.0};                                  // where he is on the tick of the traversal
    glm::dvec2 end{0.0};                                      // where he is kTicksAfter ticks on
};

Outcome Arrive(const std::string& levelName, double x, double y) {
    Outcome out;
    Run run;
    if (!Begin(levelName, run)) return out;
    out.began = true;
    for (int tick = 0; tick < 30; ++tick) Tick(run); // settled at the spawn
    CHECK_MSG(run.level.portals.TryPlace(glm::dvec2(x, y)), levelName + ": the exit portal opens at " + Px(glm::dvec2(x, y)));
    // A portal on the player: he is in it, and goes to the first.
    CHECK_MSG(run.level.portals.TryPlace(PlayerPx(run)), levelName + ": the portal on the player opens");
    bool seen = false;
    for (int tick = 0; tick < kTicksAfter; ++tick) {
        Tick(run);
        if (run.level.portals.traversals == 0) continue;
        const glm::dvec2 at = PlayerPx(run);
        if (!seen) {
            seen = true;
            out.arrival = at;
            if (!run.level.portals.transits.empty()) out.exitPx = run.level.portals.transits.front().exitPx;
        }
        out.leastY = std::min(out.leastY, at.y);
    }
    out.traversals = run.level.portals.traversals;
    out.end = PlayerPx(run);
    return out;
}

std::string Label(const char* what, double x, double y) {
    char text[64];
    std::snprintf(text, sizeof text, "%s exit at (%.1f, %.1f): ", what, x, y);
    return text;
}

bool StandsInThePocket(const Outcome& out) {
    return out.end.y > 200.0 && out.end.y < 212.0 && out.end.x > 256.0 && out.end.x < 320.0;
}

void AnArrivalUnderAPlatformIsNotPushedUpIntoIt() {
    // Over the pocket, from its left end to where slab 862 meets the bridge (x 320), 0.5 to 10 units under the slab's underside (y 160; his axis's top
    // end is 9 above his centre, so any y under 169 puts it through the face, and the chamfered corner at x 310 to 320 reaches lower). Before the fix
    // the engine sent him UP into the slab and held him there: the owner's and the tester's picture, his centre about (314, 140), inside both grey
    // slabs. After it his centre stays under the underside and he falls to the pocket's floor.
    for (double y = 160.5; y < 170.0; y += 1.5) {
        for (double x = 266.0; x < 320.0; x += 2.0) {
            const Outcome out = Arrive("level20", x, y);
            if (!out.began) return;
            const std::string at = Label("1-21", x, y);
            CHECK_MSG(out.traversals == 1, at + "he went through the pair");
            CHECK_MSG(out.leastY >= 160.0, at + "his centre never went up into the platform (least y " + std::to_string(out.leastY) + ")");
            CHECK_MSG(StandsInThePocket(out), at + "he stands on the pocket's floor, at " + Px(out.end));
        }
    }
}

void TheSeamOfTheSlabAndTheBridge() {
    // The corner the owner's picture is of: slab 862's chamfered underside runs up to x 320, where the bridge's side face (x 320, down to y 188.8)
    // stands beside it, 1 to 10 units from the exit. Both hulls are in play, and the order the engine takes the pair in is the engine's, so the arrivals
    // are taken at one unit steps along the last 10 units before the seam, at three heights.
    for (const double y : {162.0, 165.0, 168.0}) {
        for (double x = 310.0; x <= 319.0; x += 1.0) {
            const Outcome out = Arrive("level20", x, y);
            if (!out.began) return;
            const std::string at = Label("1-21 seam", x, y);
            CHECK_MSG(out.traversals == 1, at + "he went through the pair");
            CHECK_MSG(out.leastY >= 160.0, at + "his centre never went up into the platform (least y " + std::to_string(out.leastY) + ")");
            CHECK_MSG(StandsInThePocket(out), at + "he stands on the pocket's floor, at " + Px(out.end));
            // What the fix did to the arrival is the recorded deviation and no more: lowered, by at most his half axis and the margin. The portal hovers a
            // little (about 2 units), so an exit opened at 168 is sometimes under the reach of the axis and sometimes not, and then he is put at the exit
            // itself (to the float rounding of the world's metres); at 162 the axis is inside the face whatever the hover, and he is lowered.
            const double lowered = out.arrival.y - out.exitPx.y;
            CHECK_MSG(lowered >= -kRoundingPx && lowered <= kMostLoweredPx + kRoundingPx,
                      at + "put down lower than the exit by " + std::to_string(lowered) + " units");
            if (y == 162.0) CHECK_MSG(lowered > 0.4, at + "he was lowered, by " + std::to_string(lowered) + " units");
        }
    }
}

void The1_20ArrivalThatWalkedThroughThePlankStaysUnderIt() {
    // The recorded route's third pair is aimed at (300, 168), not (300, 150): the exit 6 units under the plank's underside (y 144) put the capsule INTO the
    // plank, and he walked left through it and the post. From exits 1 to 12 units under it he is now put under it and falls to the alcove's floor.
    for (const double y : {145.0, 150.0, 152.0, 156.0}) {
        for (const double x : {240.0, 300.0, 360.0, 428.0}) {
            const Outcome out = Arrive("level19", x, y);
            if (!out.began) return;
            const std::string at = Label("1-20", x, y);
            CHECK_MSG(out.traversals == 1, at + "he went through the pair");
            CHECK_MSG(out.leastY >= 144.0, at + "his centre never went up into the plank (least y " + std::to_string(out.leastY) + ")");
            CHECK_MSG(out.end.y > 144.0, at + "and he ends under it, at " + Px(out.end));
        }
    }
}

void AnArrivalClearOfEveryHullIsExactlyWhereThePortalIs() {
    // The fix acts only where the top of his axis is inside a hull and his centre is not. Everywhere else he is put at the exit itself, to the hundredth
    // of a unit: in the open air, in the pocket under the underside by more than his axis, where the bottom of his axis is in 1-20's floor (the engine's
    // UP is right there, and he ends standing on it), and in 1-20 clear under the plank (the route's own aim).
    struct Case {
        const char* level;
        glm::dvec2 at;
        const char* why;
    };
    const Case cases[] = {
        {"level20", {290.0, 195.0}, "in the pocket's open air"},
        {"level19", {330.0, 218.0}, "on the alcove's floor (the bottom of his axis in it)"},
        {"level20", {250.0, 100.0}, "in open air, clear of every platform"},
        {"level19", {300.0, 170.0}, "1-20's own aim, clear under the plank"},
    };
    for (const Case& c : cases) {
        const Outcome out = Arrive(c.level, c.at.x, c.at.y);
        if (!out.began) return;
        const std::string at = std::string(c.level) + " " + Px(c.at) + " " + c.why + ": ";
        CHECK_MSG(out.traversals == 1, at + "he went through the pair");
        CHECK_MSG(glm::length(out.arrival - out.exitPx) < 0.01,
                  at + "put down at the exit itself: at " + Px(out.arrival) + ", exit " + Px(out.exitPx));
        if (c.at.y == 218.0) {
            // On the floor he ends standing on it, as before: the alcove's floor top is y 224, where a player's centre is 206.4.
            CHECK_MSG(out.end.y > 200.0 && out.end.y < 212.0, at + "and he ends standing on the floor, at " + Px(out.end));
        }
    }
}

void runTests() {
    AnArrivalUnderAPlatformIsNotPushedUpIntoIt();
    TheSeamOfTheSlabAndTheBridge();
    The1_20ArrivalThatWalkedThroughThePlankStaysUnderIt();
    AnArrivalClearOfEveryHullIsExactlyWhereThePortalIs();
}

} // namespace

MP_SUITE_MAIN("test_mp_arrival") {
    std::error_code ec;
    if (!std::filesystem::is_directory(kLevels, ec) ||
        !std::filesystem::is_regular_file(kData + "/entity_roles.json", ec)) {
        std::printf("test_mp_arrival: SKIPPED - needs the converted levels at %s\n"
                    "  and Enhanced's data at %s.\n"
                    "  Both live outside this repository; configure with\n"
                    "  -DSUPERSONIC_MAGICPORTALS_LEVELS=... and -DSUPERSONIC_MAGICPORTALS_DATA=...\n",
                    kLevels.c_str(), kData.c_str());
        return 77;
    }
    runTests();
    return ::test::summary("test_mp_arrival", 400);
}
