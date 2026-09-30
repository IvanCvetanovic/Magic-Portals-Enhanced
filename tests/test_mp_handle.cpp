// The draggable handle, arrows.ent, and what follows it (planning step 152;
// game/data/handle.json, sim/Handle.hpp): ETHCallback_arrows, doFollow and 2-20's
// ETHCallback_hand_waypoint0, decoded.
//
// Pinned here: the decode's numbers as read; the census - six handles, one on each
// of 2-20 to 2-24 and 2-27, each on the point of the one node that follows it, and
// no other entity carrying `follow`; the callback's arithmetic - the hit remembered,
// the drag one for one and capped at 64, `moving` on every held tick, the alpha and
// the pulse; followUp's ease after a handle; the lesson's round; and on the levels
// themselves, 2-20's flame dragged onto its crates burns them and 2-27's reflector
// follows its handle, while a handle no touch holds moves nothing.
//
// Reads the converted levels and Enhanced's data from outside this repository,
// and skips, saying where it looked, when either is absent.

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "sim/Game.hpp"
#include "sim/Handle.hpp"
#include "sim/Roles.hpp"
#include "sim/Tscn.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <map>
#include <string>
#include <system_error>
#include <vector>

using namespace MagicPortals;

namespace {

const std::string kLevels = MAGICPORTALS_LEVELS_DIR;
const std::string kData = MAGICPORTALS_DATA_DIR;
const std::string kPortData = MAGICPORTALS_PORT_DATA_DIR;
constexpr float kStep = 1.0f / 60.0f;
const double kTickMs = static_cast<double>(kStep) * 1000.0;

std::string Px(const glm::dvec2& p) {
    char text[64];
    std::snprintf(text, sizeof(text), "(%.3f, %.3f)", p.x, p.y);
    return text;
}

bool Near(const glm::dvec2& a, const glm::dvec2& b, double eps) { return glm::length(a - b) <= eps; }

Handle::Rules LoadedRules() {
    Handle::Rules rules;
    std::string error;
    CHECK_MSG(Handle::LoadRules(kPortData + "/handle.json", rules, error), error);
    return rules;
}

void HandleJsonIsTheDecode() {
    const Handle::Rules rules = LoadedRules();
    CHECK(rules.entity == "arrows");
    CHECK(rules.sizeUnits == glm::dvec2(64.0, 64.0));
    CHECK(rules.maxFrameMoveUnits == 64.0);
    CHECK_MSG(rules.idleAlpha == 0.3 && rules.movingAlpha == 1.0, "faint at rest, whole while held (#179-200)");
    CHECK(rules.bounce.fromScale == glm::dvec2(1.1, 1.0) && rules.bounce.toScale == glm::dvec2(1.0, 1.1));
    CHECK(rules.bounce.strideMs == 300.0);
    CHECK(rules.followDatum == "follow");
    CHECK(rules.followers == (std::vector<std::string>{"fire_agent", "reflect_agent"}));
    CHECK_MSG(rules.follow.strideMs == 600.0 && rules.follow.rateMs == 100.0 && rules.follow.dontPause,
              "followUp(this, dest, 600, 100, true)");
    CHECK(rules.follow.offsetUnits == glm::dvec2(0.0));
    CHECK(rules.lessonEntity == "hand_waypoint0" && rules.lessonHandleName == "arrows.ent");
    CHECK_MSG(rules.lessonWaypoints.size() == 6u, "six waypoints");
    if (rules.lessonWaypoints.size() != 6u) return;
    const glm::dvec2 at[] = {{0, 0}, {0, 0}, {0, 0}, {0, 25}, {70, 10}, {70, 10}};
    const double ms[] = {100, 0, 1200, 1500, 400, 0};
    const double alpha[] = {0, 1, 1, 1, 1, 0};
    const Ease::Filter filter[] = {Ease::Filter::SmoothBeginning, Ease::Filter::SmoothBeginning,
                                   Ease::Filter::SmoothBeginning, Ease::Filter::SmoothEnd,
                                   Ease::Filter::SmoothEnd,       Ease::Filter::SmoothEnd};
    for (std::size_t i = 0; i < 6; ++i) {
        const Hint::Waypoint& w = rules.lessonWaypoints[i];
        CHECK_MSG(w.atUnits == at[i] && w.ms == ms[i] && w.alpha == alpha[i] && w.filter == filter[i],
                  "waypoint " + std::to_string(i) + " as the callback adds it");
    }
}

// Every level, both spellings: where a handle is, what follows it, and what else
// carries `follow` - which only the two callers read.
void TheSixHandlesAndWhatFollowsThem() {
    const Handle::Rules rules = LoadedRules();
    std::map<std::string, std::vector<std::string>> expected = {
        {"level19a", {"fire_agent_1086"}}, {"level20a", {"fire_agent_1086"}}, {"level21a", {"fire_agent_1086"}},
        {"level22a", {"fire_agent_1086"}}, {"level23a", {"fire_agent_1086"}}, {"level26a", {"reflect_agent_ent_961"}}};
    int levels = 0;
    int handles = 0;
    int followers = 0;
    int otherFollow = 0;
    int lessons = 0;
    std::string lessonLevel;
    std::error_code ec;
    for (const auto& file : std::filesystem::directory_iterator(kLevels, ec)) {
        if (file.path().extension() != ".tscn") continue;
        Tscn::Scene scene;
        std::string error;
        if (!Tscn::Load(file.path().string(), scene, error)) continue;
        ++levels;
        const std::string level = file.path().stem().string();
        Handle::State state;
        CHECK_MSG(Handle::Find(scene, rules, state, error), level + ": " + error);
        for (const Tscn::Node& node : scene.nodes) {
            if (node.parent != ".") continue;
            std::string bare = Roles::EntityName(node);
            if (bare.size() > 4 && bare.compare(bare.size() - 4, 4, ".ent") == 0) bare.resize(bare.size() - 4);
            if (bare == rules.lessonEntity) {
                ++lessons;
                lessonLevel = level;
            }
            if (node.Meta(rules.followDatum.c_str()) != nullptr &&
                std::find(rules.followers.begin(), rules.followers.end(), bare) == rules.followers.end()) {
                ++otherFollow;
            }
        }
        handles += static_cast<int>(state.handles.size());
        followers += static_cast<int>(state.followers.size());
        const auto want = expected.find(level);
        if (want == expected.end()) {
            CHECK_MSG(state.handles.empty() && state.followers.empty(), level + " places no handle and no follower");
            continue;
        }
        CHECK_MSG(state.handles.size() == 1u, level + ": one handle");
        CHECK_MSG(state.followers.size() == 1u, level + ": one follower");
        if (state.handles.size() != 1u || state.followers.size() != 1u) continue;
        const Handle::Follower& follower = state.followers.front();
        CHECK_MSG(follower.node == want->second.front(), level + ": " + follower.node + " follows");
        CHECK_MSG(follower.handle == 0 && follower.follows == "arrows.ent", level + ": it follows its handle");
        CHECK_MSG(follower.startPx == state.handles.front().startPx,
                  level + ": the follower starts on its handle's point " + Px(state.handles.front().startPx));
        CHECK_MSG(state.handles.front().entityName == "arrows.ent", level + ": the handle is arrows.ent");
    }
    CHECK_MSG(levels == 128, "the 128 converted levels read: " + std::to_string(levels));
    CHECK_MSG(handles == 6 && followers == 6, "six handles and six followers: " + std::to_string(handles) + ", " +
                                                   std::to_string(followers));
    CHECK_MSG(otherFollow == 0, "no other entity carries `follow`: " + std::to_string(otherFollow));
    CHECK_MSG(lessons == 1 && lessonLevel == "level19a", "2-20 alone places the lesson hand");
}

void TheRectAndTheCap() {
    const glm::dvec2 size(64.0, 64.0);
    // isPointInRect: both edges in, a hair past either out.
    CHECK(Handle::InRect({132.0, 100.0}, {100.0, 100.0}, size) && Handle::InRect({68.0, 132.0}, {100.0, 100.0}, size));
    CHECK(!Handle::InRect({132.001, 100.0}, {100.0, 100.0}, size) &&
          !Handle::InRect({100.0, 67.999}, {100.0, 100.0}, size));
    // capVector: 64 long at most, its direction kept.
    CHECK(Handle::Cap({30.0, 40.0}, 64.0) == glm::dvec2(30.0, 40.0));
    CHECK(Near(Handle::Cap({60.0, 80.0}, 64.0), glm::dvec2(38.4, 51.2), 1e-12));
}

Handle::State OneHandle(const glm::dvec2& atPx) {
    Handle::State state;
    state.rules = LoadedRules();
    Handle::Handle handle;
    handle.node = "arrows_ent_1";
    handle.entityName = "arrows.ent";
    handle.startPx = handle.atPx = atPx;
    state.handles.push_back(handle);
    return state;
}

void ADragIsTheCallbacksArithmetic() {
    Handle::State state = OneHandle({100.0, 100.0});
    const Handle::Handle& handle = state.handles.front();
    CHECK_MSG(state.Scale(handle) == glm::dvec2(1.0) && state.Alpha(handle) == 0.3,
              "before its first call: its own scale");

    // No touch: at rest, faint, pulsing.
    state.Drag(Handle::Touch{}, kTickMs);
    CHECK(!handle.moving && handle.atPx == glm::dvec2(100.0, 100.0) && state.Alpha(handle) == 0.3);
    CHECK_MSG(std::fabs(handle.bounceMs - kTickMs) < 1e-12 && handle.bounced, "bounce's clock on");

    // Down on it (HIT): remembered, nothing moves yet.
    Handle::Touch hit;
    hit.hit = true;
    hit.atPx = {120.0, 90.0};
    state.Drag(hit, kTickMs);
    CHECK_MSG(handle.onHitPx == glm::dvec2(100.0, 100.0) && !handle.moving, "onHitPos: where it stood");

    // Held (DOWN), from where it went down: one for one with the finger.
    Handle::Touch held;
    held.down = true;
    held.hitPx = {120.0, 90.0};
    held.moveUnits = {5.0, -3.0};
    state.Drag(held, kTickMs);
    CHECK_MSG(handle.atPx == glm::dvec2(105.0, 97.0) && handle.moving && state.Alpha(handle) == 1.0,
              "dragged by the move, and whole: " + Px(handle.atPx));
    CHECK(state.AnyMoving());
    // Still: moving all the same (#161-173 sets it on every such frame).
    held.moveUnits = {0.0, 0.0};
    state.Drag(held, kTickMs);
    CHECK_MSG(handle.atPx == glm::dvec2(105.0, 97.0) && handle.moving, "held still: moving, and nothing moved");
    // A fast finger: 64 units a frame at most.
    held.moveUnits = {100.0, 0.0};
    state.Drag(held, kTickMs);
    CHECK_MSG(Near(handle.atPx, glm::dvec2(169.0, 97.0), 1e-12), "capped at scale(64): " + Px(handle.atPx));
    // The rect tested is where it stood at the hit, not where it is: the finger can
    // run ahead of it and it still follows.
    held.moveUnits = {10.0, 10.0};
    state.Drag(held, kTickMs);
    CHECK_MSG(Near(handle.atPx, glm::dvec2(179.0, 107.0), 1e-12) && handle.moving, "still dragged");

    // Up: at rest again, faint.
    state.Drag(Handle::Touch{}, kTickMs);
    CHECK(!handle.moving && state.Alpha(handle) == 0.3 && !state.AnyMoving());

    // A touch that goes down off it and slides over it drags nothing.
    Handle::State other = OneHandle({100.0, 100.0});
    Handle::Touch off;
    off.hit = true;
    off.atPx = {300.0, 100.0};
    other.Drag(off, kTickMs);
    Handle::Touch over;
    over.down = true;
    over.hitPx = {300.0, 100.0};
    over.moveUnits = {-50.0, 0.0};
    other.Drag(over, kTickMs);
    CHECK_MSG(other.handles.front().atPx == glm::dvec2(100.0, 100.0) && !other.handles.front().moving,
              "a touch that went down off the handle does not take it");

    // Its pulse: (1.1, 1) to (1, 1.1) over 300 ms, and the rect with it.
    Handle::State pulse = OneHandle({0.0, 0.0});
    for (int tick = 0; tick < 18; ++tick) pulse.Drag(Handle::Touch{}, kTickMs);
    const glm::dvec2 leg = pulse.Scale(pulse.handles.front());
    CHECK_MSG(Near(leg, glm::dvec2(1.0, 1.1), 1e-3), "at 300 ms, (1, 1.1): " + Px(leg));
    CHECK_MSG(Near(pulse.SizeUnits(pulse.handles.front()), glm::dvec2(64.0, 70.4), 0.1), "and so is its rect");

    // The door's beat, a death's: at rest, pulsing on; the pause: at rest, still.
    Handle::State rest = OneHandle({0.0, 0.0});
    rest.handles.front().moving = true;
    rest.Rest(kTickMs);
    CHECK(!rest.handles.front().moving && rest.handles.front().bounced);
    rest.handles.front().moving = true;
    const double clock = rest.handles.front().bounceMs;
    rest.Rest(0.0);
    CHECK(!rest.handles.front().moving && rest.handles.front().bounceMs == clock);
}

void AFollowerEasesAfterItsHandle() {
    Handle::State state = OneHandle({100.0, 100.0});
    Handle::Follower follower;
    follower.node = "fire_agent_1";
    follower.follows = "arrows.ent";
    follower.handle = 0;
    follower.startPx = follower.atPx = {100.0, 100.0};
    state.followers.push_back(follower);
    for (int tick = 0; tick < 120; ++tick) state.Follow(kTickMs);
    CHECK_MSG(state.followers.front().atPx == glm::dvec2(100.0, 100.0), "a handle never dragged moves nothing");

    // The handle goes 100 right at once. followUp aims its interpolator again from
    // where it has got to each time switchTime passes 100 ms while the handle is
    // elsewhere, each aim a fresh 600 ms of smoothEnd: so it closes in, a share of
    // the gap every 100 ms, and never overshoots.
    state.handles.front().atPx = {200.0, 100.0};
    std::vector<double> xs;
    for (int tick = 1; tick <= 180; ++tick) {
        state.Follow(kTickMs);
        xs.push_back(state.followers.front().atPx.x);
        CHECK_MSG(state.followers.front().atPx.y == 100.0, "along the line it was dragged");
    }
    const double first = 100.0 + 100.0 * static_cast<double>(std::sin(static_cast<float>(kTickMs / 600.0) *
                                                                       1.570796327f));
    CHECK_MSG(std::fabs(xs[0] - first) < 1e-3, "on the first tick, smoothEnd of 1/36 of its stride: " +
                                                     std::to_string(xs[0]) + " against " + std::to_string(first));
    bool closing = true;
    for (std::size_t i = 1; i < xs.size(); ++i) closing = closing && xs[i] >= xs[i - 1] && xs[i] <= 200.0;
    CHECK_MSG(closing, "it closes in and never passes the handle");
    CHECK_MSG(200.0 - xs[29] > 15.0, "half a second on it is still well short: " + std::to_string(xs[29]));
    CHECK_MSG(200.0 - xs[119] < 1.0, "two seconds on, within a unit: " + std::to_string(xs[119]));
    CHECK_MSG(200.0 - xs[179] < 0.05, "three, all but there: " + std::to_string(xs[179]));
}

void TheLessonIsTheDecodedRound() {
    const Handle::Rules rules = LoadedRules();
    Handle::Lesson lesson;
    lesson.placed = true;
    lesson.originalPx = {165.0, 191.0};
    lesson.Tick(rules, false, kTickMs);
    CHECK_MSG(lesson.AtPx(rules) == glm::dvec2(165.0, 191.0) && lesson.Alpha(rules) == 0.0 && lesson.ticked,
              "its first call: at its place, unseen");
    for (int tick = 2; tick <= 7; ++tick) lesson.Tick(rules, false, kTickMs);
    CHECK_MSG(std::fabs(lesson.Alpha(rules) - 1.0) < 1e-9, "whole 100 ms on: " + std::to_string(lesson.Alpha(rules)));
    double mostRight = 0.0;
    double mostDown = 0.0;
    int backAt = -1;
    bool left = false;
    for (int tick = 8; tick <= 400; ++tick) {
        lesson.Tick(rules, false, kTickMs);
        const glm::dvec2 offset = lesson.AtPx(rules) - lesson.originalPx;
        mostRight = std::max(mostRight, offset.x);
        mostDown = std::max(mostDown, offset.y);
        left = left || lesson.frame != 0;
        if (backAt < 0 && left && lesson.frame == 0) backAt = tick;
    }
    CHECK_MSG(std::fabs(mostRight - 70.0) < 1e-5 && std::fabs(mostDown - 25.0) < 1e-5,
              "it presses 25 down and drags 70 right: " + std::to_string(mostRight) + ", " + std::to_string(mostDown));
    CHECK_MSG(backAt > 0 && std::fabs((backAt - 1) * kTickMs - 3200.0) <= 3.0 * kTickMs,
              "a round of 3200 ms: back at its first waypoint " + std::to_string((backAt - 1) * kTickMs) + " ms on");
    CHECK(!lesson.gone);
    lesson.Tick(rules, true, kTickMs);
    CHECK_MSG(lesson.gone && lesson.vanishedNow, "the tick its handle is held, it goes");
    lesson.Tick(rules, true, kTickMs);
    CHECK_MSG(lesson.gone && !lesson.vanishedNow, "once");
}

struct Run {
    Game::Data data;
    entt::registry registry;
    Game::Level level;
};

bool Begin(const std::string& levelName, Run& run) {
    std::string error;
    const bool ok = Game::LoadData(kLevels + "/" + levelName + ".tscn", kData,
                                   std::filesystem::temp_directory_path() / "supersonic-test-mp-handle", run.data,
                                   error) &&
                    Game::Start(run.data, run.registry, run.level, error);
    CHECK_MSG(ok, levelName + ": " + error);
    return ok;
}

// A finger down on the handle's point on one tick, then moved `move` a tick for
// `ticks` ticks, through the handle's callback, each followed by the game's tick.
void DragOnLevel(Run& run, const glm::dvec2& move, int ticks) {
    Handle::State& handles = run.level.handles;
    const glm::dvec2 from = handles.handles.front().atPx;
    Handle::Touch hit;
    hit.hit = true;
    hit.atPx = from;
    handles.Drag(hit, kTickMs);
    Game::Tick(run.data, run.registry, run.level, 0.0f, kStep);
    for (int tick = 0; tick < ticks; ++tick) {
        Handle::Touch held;
        held.down = true;
        held.hitPx = from;
        held.moveUnits = move;
        handles.Drag(held, kTickMs);
        Game::Tick(run.data, run.registry, run.level, 0.0f, kStep);
    }
    handles.Drag(Handle::Touch{}, kTickMs);
}

void OnTwoTwentyTheFlameIsDraggedOntoTheCrates() {
    Run run;
    if (!Begin("level19a", run)) return;
    CHECK(run.level.handles.handles.size() == 1u && run.level.handles.followers.size() == 1u);
    if (run.level.handles.followers.size() != 1u) return;
    const Handle::Follower& follower = run.level.handles.followers.front();
    CHECK_MSG(follower.fireAgent >= 0 && follower.reflector < 0, "it moves a fire agent");
    if (follower.fireAgent < 0) return;
    const Fire::Agent& agent = run.level.fire.agents[static_cast<std::size_t>(follower.fireAgent)];

    // Untouched for five seconds: the flame stands where the level put it, and the
    // crates 200 away stand too.
    for (int tick = 0; tick < 300; ++tick) Game::Tick(run.data, run.registry, run.level, 0.0f, kStep);
    CHECK_MSG(agent.atPx == glm::dvec2(162.0, 192.0), "an untouched handle: the flame stays " + Px(agent.atPx));
    CHECK(run.level.fire.lit == 0);

    // Dragged (183, -87) over 30 ticks, to (345, 105) between the two crates.
    DragOnLevel(run, glm::dvec2(6.1, -2.9), 30);
    const glm::dvec2 handleAt = run.level.handles.handles.front().atPx;
    CHECK_MSG(Near(handleAt, glm::dvec2(345.0, 105.0), 1e-9), "the handle one for one: " + Px(handleAt));
    for (int tick = 0; tick < 120; ++tick) Game::Tick(run.data, run.registry, run.level, 0.0f, kStep);
    CHECK_MSG(Near(agent.atPx, handleAt, 1.0), "the flame has followed it there: " + Px(agent.atPx));
    const Fire::Burnable* small = run.level.fire.FindBurnable("crate_small_no_emissive_ent_1129");
    const Fire::Burnable* big = run.level.fire.FindBurnable("crate_no_emissive_1128");
    CHECK_MSG(small != nullptr && big != nullptr && small->burned && big->burned, "both crates burn");
    for (int tick = 0; tick < 60; ++tick) Game::Tick(run.data, run.registry, run.level, 0.0f, kStep);
    CHECK_MSG(small != nullptr && big != nullptr && small->gone && big->gone, "and are gone after their second");
    CHECK(!run.level.hazards.playerDied);
}

void OnTwoTwentySevenTheReflectorFollows() {
    Run run;
    if (!Begin("level26a", run)) return;
    CHECK(run.level.handles.followers.size() == 1u);
    if (run.level.handles.followers.size() != 1u) return;
    const Handle::Follower& follower = run.level.handles.followers.front();
    CHECK_MSG(follower.reflector >= 0 && follower.fireAgent < 0, "it moves the reflector");
    if (follower.reflector < 0) return;
    const Portals::Reflector& reflector = run.level.portals.reflectors[static_cast<std::size_t>(follower.reflector)];
    CHECK(reflector.centrePx == glm::dvec2(260.0, 256.0));
    DragOnLevel(run, glm::dvec2(0.0, -4.0), 20);
    for (int tick = 0; tick < 180; ++tick) Game::Tick(run.data, run.registry, run.level, 0.0f, kStep);
    CHECK_MSG(Near(reflector.centrePx, glm::dvec2(260.0, 176.0), 0.5),
              "the reflector follows: " + Px(reflector.centrePx));
    // And the fixed fire agent on the same level, which names no handle, stays.
    CHECK(run.level.fire.agents.size() == 1u && run.level.fire.agents.front().atPx == glm::dvec2(235.0, 123.0));
}

void runTests() {
    HandleJsonIsTheDecode();
    TheSixHandlesAndWhatFollowsThem();
    TheRectAndTheCap();
    ADragIsTheCallbacksArithmetic();
    AFollowerEasesAfterItsHandle();
    TheLessonIsTheDecodedRound();
    OnTwoTwentyTheFlameIsDraggedOntoTheCrates();
    OnTwoTwentySevenTheReflectorFollows();
}

} // namespace

MP_SUITE_MAIN("test_mp_handle") {
    std::error_code ec;
    if (!std::filesystem::is_directory(kLevels, ec) ||
        !std::filesystem::is_regular_file(kData + "/entity_roles.json", ec)) {
        std::printf("test_mp_handle SKIPPED - needs the converted levels at %s and Enhanced's data at %s\n",
                    kLevels.c_str(), kData.c_str());
        return 77;
    }
    runTests();
    return ::test::summary("test_mp_handle", 60);
}
