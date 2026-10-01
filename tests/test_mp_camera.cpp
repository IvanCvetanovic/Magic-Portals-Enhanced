// The port's camera as arithmetic: Camera::Clamp keeps a view inside a level,
// Camera::Follow eases to the player from its first tick - there is no opening
// hold - and a finger drags it as the original's CameraController::dragMoveCamera
// does (game/data/camera.json `drag`, step 138): twice its move, a 4000 ms hold after
// a drag away, no follow while a touch is held off the pads.
//
// Pure: no level. portals.json's follow lag is a _guess, so it is not pinned; this
// suite eases with numbers of its own and checks the shape of what they do. The
// drag's numbers are decoded and the port's own (camera.json), so they ARE pinned,
// both settings of its switch with them, and so is 1-21's lesson (camera.json
// `hint`, sim/Hint). The view is level0's shape, 455 x 256 in a level 768 x 256.
//
// And the shake the original gives that camera (sim/Earthquake): earthquake.json
// is the port's own and decoded, so its numbers ARE pinned, and the curve is
// checked against the decode's own table (D11 section 1c, D11_check C11) and the
// sequences the original's footage shows. And the impacts that call it (step
// 104): each row's gate and factor, and where the port finds a contact beginning
// (sim/Impacts), on shapes and a registry of its own.

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "core/Components.hpp"
#include "sim/Camera.hpp"
#include "sim/DarkDragon.hpp"
#include "sim/Earthquake.hpp"
#include "sim/Hint.hpp"
#include "sim/Impacts.hpp"
#include "sim/Units.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

using namespace MagicPortals;

namespace {

constexpr double kDt = 1.0 / 60.0;
const glm::dvec2 kView(455.0, 256.0);
const glm::dvec2 kBounds(768.0, 256.0);

bool Near(const glm::dvec2& a, const glm::dvec2& b, double eps) {
    return std::fabs(a.x - b.x) <= eps && std::fabs(a.y - b.y) <= eps;
}

std::string Px(const glm::dvec2& p) {
    return "(" + std::to_string(p.x) + ", " + std::to_string(p.y) + ")";
}

void TheViewStaysInsideTheLevel() {
    CHECK(Camera::Clamp({0.0, 0.0}, kView, kBounds) == glm::dvec2(227.5, 128.0));
    CHECK(Camera::Clamp({768.0, 256.0}, kView, kBounds) == glm::dvec2(540.5, 128.0));
    CHECK(Camera::Clamp({400.0, 50.0}, kView, kBounds) == glm::dvec2(400.0, 128.0));
    // A view wider than the level, as a wide window gives one, sits at its centre.
    CHECK(Camera::Clamp({0.0, 0.0}, {1000.0, 256.0}, kBounds) == glm::dvec2(384.0, 128.0));
    // And a taller level lets it travel up and down as well.
    CHECK(Camera::Clamp({100.0, 400.0}, kView, {512.0, 512.0}) == glm::dvec2(227.5, 384.0));
}

// The follow alone, drag off: the lag this suite eases with, and camera.json's
// switch false.
Camera::Rules FollowOnly() {
    Camera::Rules rules;
    rules.followLagS = 0.12;
    return rules;
}

// The decoded drag, read from camera.json, with this suite's lag.
Camera::Rules Decoded() {
    Camera::Rules rules = FollowOnly();
    std::string error;
    if (!Camera::LoadDrag(std::string(MAGICPORTALS_PORT_DATA_DIR) + "/camera.json", rules, error)) {
        CHECK_MSG(false, error);
    }
    return rules;
}

// The layer's tick as the camera takes it: fixedDelta, a float 1/60 s.
constexpr double kTickS = static_cast<double>(1.0f / 60.0f);
// A level with room to pan both ways, and the view in the middle of it.
const glm::dvec2 kWide(1600.0, 800.0);
const glm::dvec2 kMiddle(800.0, 400.0);

// One tick of the pointer as the layer hands it to the camera: down at `at` on the
// tick it is pressed, held there, or up (held false).
Camera::Feel Touch(Camera::TouchRecord& record, bool pressed, bool held, const glm::dvec2& at, bool onPad = false) {
    return Camera::Track(record, pressed, held, true, at, onPad);
}

void ThereIsNoOpeningHold() {
    const Camera::Rules rules = Decoded();
    Camera::Follow follow;
    // level0: camera_start (651, 0) is held inside the level, at its right end.
    follow.Start(rules, {651.0, 0.0}, kView, kBounds);
    const glm::dvec2 start = follow.centrePx;
    CHECK(start == glm::dvec2(540.5, 128.0));
    // The constructor's cameraHoldElapsedTime = cameraHoldTime (#88-91): spent.
    CHECK_MSG(follow.holdElapsedMs == rules.drag.holdMs && !follow.Held(rules),
              "the hold starts spent: " + std::to_string(follow.holdElapsedMs));

    const glm::dvec2 player(36.0, 86.0); // level0's spawn
    follow.Tick(rules, player, kView, kBounds, kDt, Camera::Feel{}, Camera::Unlocks{});
    CHECK_MSG(follow.centrePx.x < start.x, "and it moves on the first tick, to " + Px(follow.centrePx));

    // Each tick closes dt / follow_lag_s of the gap to where the view may go.
    const glm::dvec2 target = Camera::Clamp(player, kView, kBounds);
    const double before = follow.centrePx.x - target.x;
    follow.Tick(rules, player, kView, kBounds, kDt, Camera::Feel{}, Camera::Unlocks{});
    const double after = follow.centrePx.x - target.x;
    CHECK_NEAR(static_cast<float>(after), static_cast<float>(before * (1.0 - kDt / rules.followLagS)));

    for (int i = 0; i < 180; ++i) follow.Tick(rules, player, kView, kBounds, kDt, Camera::Feel{}, Camera::Unlocks{});
    CHECK_MSG(Near(follow.centrePx, glm::dvec2(227.5, 128.0), 0.5),
              "settled with the level's left edge at the view's, at " + Px(follow.centrePx));
}

void CameraJsonPinsTheDragAndItsSwitch() {
    // camera.json is the port's own and decoded: every number pinned, and the
    // switch at the original's setting.
    const std::string path = std::string(MAGICPORTALS_PORT_DATA_DIR) + "/camera.json";
    Camera::Rules rules;
    std::string error;
    CHECK_MSG(Camera::LoadDrag(path, rules, error), error);
    const Camera::Drag& drag = rules.drag;
    CHECK_MSG(drag.enabled, "drag_pan_enabled is true: the original's camera");
    CHECK(drag.panFactor == 2.0);
    CHECK_MSG(drag.maxFrameMoveUnits == 100.0 / 2.8125,
              "100 screen px a frame at R12's 2.8125 px a unit: " + std::to_string(drag.maxFrameMoveUnits));
    CHECK(drag.releaseMoveUnits == 24.0);
    CHECK(drag.dragLockUnits == 10.0);
    CHECK(drag.holdMs == 4000.0);

    // AND ITS OTHER SETTING, the owner's 2026-09-11 ruling: the same file with the
    // switch false reads with the drag off and every other number as it was.
    std::ifstream in(path, std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    std::string off = text.str();
    const std::string on = "\"drag_pan_enabled\": true";
    const std::size_t at = off.find(on);
    CHECK_MSG(at != std::string::npos, "camera.json sets drag_pan_enabled to true");
    if (at == std::string::npos) return;
    off.replace(at, on.size(), "\"drag_pan_enabled\": false");
    const std::filesystem::path offPath = std::filesystem::temp_directory_path() / "test_mp_camera_drag_off.json";
    {
        std::ofstream out(offPath, std::ios::binary);
        out << off;
    }
    Camera::Rules owner;
    CHECK_MSG(Camera::LoadDrag(offPath.string(), owner, error), error);
    CHECK_MSG(!owner.drag.enabled && owner.drag.panFactor == 2.0 && owner.drag.holdMs == 4000.0,
              "drag_pan_enabled false turns the drag off and nothing else");
    std::filesystem::remove(offPath);
}

void TheSwitchOffIsTheOldCamera() {
    // With the switch false, a finger dragged across the view, held, walked and
    // teleported through moves nothing: every centre is the old follow's, bit for bit.
    Camera::Rules rules = Decoded();
    rules.drag.enabled = false;
    Camera::Follow old;
    Camera::Follow dragged;
    old.Start(rules, kMiddle, kView, kWide);
    dragged.Start(rules, kMiddle, kView, kWide);
    Camera::TouchRecord record;
    bool same = true;
    for (int tick = 0; tick < 400; ++tick) {
        const glm::dvec2 player = kMiddle + glm::dvec2(tick * 1.5, tick * 0.5);
        const glm::dvec2 finger(200.0 - tick * 0.3, 100.0 + tick * 0.2);
        const Camera::Feel feel = Touch(record, tick == 10, tick >= 10 && tick < 300, finger);
        Camera::Unlocks unlocks;
        unlocks.padMoving = tick % 3 == 0;
        unlocks.teleported = tick % 50 == 0;
        old.Tick(rules, player, kView, kWide, kTickS);
        dragged.Tick(rules, player, kView, kWide, kTickS, feel, unlocks);
        same = same && dragged.centrePx == old.centrePx;
    }
    CHECK_MSG(same, "switched off, a dragged camera is the follow alone at every tick");
}

void NoTouchIsTheOldFollow() {
    // Switched ON, a tick with no touch is still the old follow bit for bit, walks
    // and teleports or not: the hold starts spent and only a drag starts it.
    const Camera::Rules rules = Decoded();
    Camera::Follow old;
    Camera::Follow now;
    old.Start(rules, {651.0, 0.0}, kView, kBounds);
    now.Start(rules, {651.0, 0.0}, kView, kBounds);
    bool same = true;
    for (int tick = 0; tick < 400; ++tick) {
        const glm::dvec2 player(36.0 + tick * 1.7, 86.0);
        Camera::Unlocks unlocks;
        unlocks.padMoving = tick % 2 == 0;
        unlocks.teleported = tick % 7 == 0;
        old.Tick(rules, player, kView, kBounds, kTickS);
        now.Tick(rules, player, kView, kBounds, kTickS, Camera::Feel{}, unlocks);
        same = same && now.centrePx == old.centrePx;
    }
    CHECK_MSG(same, "with no touch, the camera is the follow alone at every tick");
}

void TheCamerasTouchIsTheEnginesInput() {
    // gs2d's MobileInput: no move on the tick it goes down, current - last on every
    // held tick after, none on the tick it comes up; TouchInputManager's farthest
    // move as the LAST tick left it.
    Camera::TouchRecord record;
    Camera::Feel feel = Touch(record, true, true, {100.0, 100.0});
    CHECK_MSG(!feel.down && feel.moveUnits == glm::dvec2(0.0), "the tick it goes down: GSKS_HIT, no move");
    feel = Touch(record, false, true, {110.0, 100.0});
    CHECK_MSG(feel.down && feel.moveUnits == glm::dvec2(10.0, 0.0) && feel.maxMoveUnits == 0.0,
              "the next: GSKS_DOWN, 10 across, and the manager's farthest still its HIT's 0");
    feel = Touch(record, false, true, {130.0, 100.0});
    CHECK_MSG(feel.moveUnits == glm::dvec2(20.0, 0.0) && feel.maxMoveUnits == 10.0,
              "then 20 more, the farthest 10 as the last tick left it");
    feel = Touch(record, false, true, {130.0, 100.0}, true);
    CHECK_MSG(feel.down && feel.onPad && feel.moveUnits == glm::dvec2(0.0) && feel.maxMoveUnits == 30.0,
              "held still, on a pad: no move, the farthest 30");
    feel = Touch(record, false, false, {160.0, 100.0});
    CHECK_MSG(!feel.down && feel.moveUnits == glm::dvec2(0.0) && !record.down, "the tick it comes up: nothing");
    feel = Touch(record, false, true, {50.0, 50.0});
    CHECK_MSG(feel.down && feel.moveUnits == glm::dvec2(0.0), "a touch held the record never saw go down: taken up");
    feel = Touch(record, false, true, {60.0, 50.0});
    CHECK_MSG(feel.moveUnits == glm::dvec2(10.0, 0.0), "and followed from there");
    feel = Camera::Track(record, false, true, false, {70.0, 50.0}, false);
    CHECK_MSG(!feel.down && !record.down, "a pointer off the game is no touch");
}

void ADragPansTheViewAtTwiceItsMove() {
    const Camera::Rules rules = Decoded();
    Camera::Follow follow;
    follow.Start(rules, kMiddle, kView, kWide);
    Camera::TouchRecord record;
    // The player stands at the view's centre, so the follow moves nothing.
    follow.Tick(rules, kMiddle, kView, kWide, kTickS, Touch(record, true, true, {200.0, 100.0}), {});
    CHECK_MSG(follow.centrePx == kMiddle, "the tick it goes down pans nothing");
    glm::dvec2 finger(200.0, 100.0);
    bool doubled = true;
    for (int tick = 0; tick < 4; ++tick) {
        const glm::dvec2 before = follow.centrePx;
        finger += glm::dvec2(-5.0, 3.0);
        follow.Tick(rules, kMiddle, kView, kWide, kTickS, Touch(record, false, true, finger), {});
        doubled = doubled && follow.centrePx == before + glm::dvec2(10.0, -6.0);
    }
    CHECK_MSG(doubled, "each held tick moves the view twice the finger's move, the other way: the level follows "
                       "the finger at twice its pace");
    // Down on a walk pad, the finger pans nothing - and the follow runs, since a
    // touch on a pad is not hasAnyTouch's; so the player is put where the view is,
    // for the follow to move nothing either.
    glm::dvec2 before = follow.centrePx;
    follow.Tick(rules, before, kView, kWide, kTickS, Touch(record, false, true, finger + glm::dvec2(5.0, 0.0), true),
                {});
    CHECK_MSG(follow.centrePx == before, "a move on a walk pad pans nothing");
    // A frame's move of 100 screen px or more pans nothing; just under it pans. A
    // new touch from (0, 0), so the move is the limit to the bit.
    const double limit = rules.drag.maxFrameMoveUnits;
    Camera::TouchRecord fresh;
    follow.Tick(rules, kMiddle, kView, kWide, kTickS, Touch(fresh, true, true, {0.0, 0.0}), {});
    before = follow.centrePx;
    follow.Tick(rules, kMiddle, kView, kWide, kTickS, Touch(fresh, false, true, {limit, 0.0}), {});
    CHECK_MSG(follow.centrePx == before, "a frame's move of exactly 100 px (" + std::to_string(limit) +
                                             " units) pans nothing: the limit is strict");
    follow.Tick(rules, kMiddle, kView, kWide, kTickS, Touch(fresh, false, true, {limit * 2.0 - 0.5, 0.0}), {});
    CHECK_MSG(Near(follow.centrePx, before - glm::dvec2((limit - 0.5) * 2.0, 0.0), 1e-9),
              "and one just under it pans twice itself, to " + Px(follow.centrePx));
    // Past the level's edge it is held inside, as clampCameraPos holds it.
    finger = glm::dvec2(limit * 2.0 - 0.5, 0.0);
    for (int tick = 0; tick < 60; ++tick) {
        finger += glm::dvec2(30.0, 0.0);
        follow.Tick(rules, kMiddle, kView, kWide, kTickS, Touch(fresh, false, true, finger), {});
    }
    CHECK_MSG(follow.centrePx.x == kView.x * 0.5, "dragged far, the view stops at the level's edge: " +
                                                      Px(follow.centrePx));
}

void AHeldTouchStopsTheFollow() {
    // hasAnyTouch: a touch held (DOWN) off the pads, moving or not, and the follow
    // waits; on a pad, or on the tick it goes down, it does not.
    const Camera::Rules rules = Decoded();
    Camera::Follow follow;
    follow.Start(rules, kMiddle, kView, kWide);
    const glm::dvec2 player = kMiddle + glm::dvec2(300.0, 100.0);
    Camera::TouchRecord record;
    follow.Tick(rules, player, kView, kWide, kTickS, Touch(record, true, true, {100.0, 100.0}), {});
    CHECK_MSG(follow.centrePx.x > kMiddle.x, "the tick it goes down, the follow runs");
    const glm::dvec2 held = follow.centrePx;
    for (int tick = 0; tick < 90; ++tick) {
        follow.Tick(rules, player, kView, kWide, kTickS, Touch(record, false, true, {100.0, 100.0}), {});
    }
    CHECK_MSG(follow.centrePx == held, "held still off the pads for 1.5 s, the view does not follow");
    follow.Tick(rules, player, kView, kWide, kTickS, Touch(record, false, true, {100.0, 100.0}, true), {});
    CHECK_MSG(follow.centrePx.x > held.x, "held on a walk pad, it does");
    const glm::dvec2 onPad = follow.centrePx;
    follow.Tick(rules, player, kView, kWide, kTickS, Touch(record, false, false, {100.0, 100.0}), {});
    CHECK_MSG(follow.centrePx.x > onPad.x, "and let go, it follows on the tick it comes up");
}

// shouldLockCamera (planning step 139): while a handle is moving, dragMoveCamera
// does nothing at all - no pan, and no hold judged - and the follow still waits on
// the held touch (hasAnyTouch).
void AMovingHandleLocksTheFinger() {
    const Camera::Rules rules = Decoded();
    Camera::Follow follow;
    follow.Start(rules, kMiddle, kView, kWide);
    const glm::dvec2 player = kMiddle + glm::dvec2(300.0, 100.0);
    Camera::TouchRecord record;
    follow.Tick(rules, player, kView, kWide, kTickS, Touch(record, true, true, {100.0, 100.0}), {});
    const glm::dvec2 atDown = follow.centrePx;
    glm::dvec2 finger(100.0, 100.0);
    for (int tick = 0; tick < 30; ++tick) {
        finger += glm::dvec2(-6.0, 0.0);
        Camera::Feel feel = Touch(record, false, true, finger);
        feel.locked = true;
        follow.Tick(rules, player, kView, kWide, kTickS, feel, {});
    }
    CHECK_MSG(follow.centrePx == atDown, "a finger dragging a handle 180 away pans nothing and the view waits");
    // Let go: no hold was judged, so the follow takes it on the tick it comes up.
    follow.Tick(rules, player, kView, kWide, kTickS, Touch(record, false, false, finger), {});
    CHECK_MSG(follow.centrePx.x > atDown.x && !follow.Held(rules), "and it follows at once, held by nothing");
}

// A drag of `ticks` held ticks, `perTick` a tick, from `at` - down, held, up - on a
// camera whose player stands at `player`; the tick it comes up is the last.
void Drag(const Camera::Rules& rules, Camera::Follow& follow, const glm::dvec2& player, glm::dvec2 at,
          const glm::dvec2& perTick, int ticks) {
    Camera::TouchRecord record;
    follow.Tick(rules, player, kView, kWide, kTickS, Touch(record, true, true, at), {});
    for (int tick = 0; tick < ticks; ++tick) {
        at += perTick;
        follow.Tick(rules, player, kView, kWide, kTickS, Touch(record, false, true, at), {});
    }
    follow.Tick(rules, player, kView, kWide, kTickS, Touch(record, false, false, at), {});
}

// Ticks with no touch until the view moves, up to `most`: how many it held.
int HeldFor(const Camera::Rules& rules, Camera::Follow& follow, const glm::dvec2& player, int most) {
    for (int tick = 1; tick <= most; ++tick) {
        const glm::dvec2 before = follow.centrePx;
        follow.Tick(rules, player, kView, kWide, kTickS, Camera::Feel{}, {});
        if (follow.centrePx != before) return tick;
    }
    return most + 1;
}

void ADragAwayHoldsTheViewFourSeconds() {
    const Camera::Rules rules = Decoded();
    // The finger right 6 a tick for 10 ticks: the view goes left, away from the
    // player - normalize(view - player) . move = -6, under scale(10) - so each held
    // tick past scale(24) from where it went down starts the hold again.
    Camera::Follow follow;
    follow.Start(rules, kMiddle, kView, kWide);
    Drag(rules, follow, kMiddle, {100.0, 100.0}, {6.0, 0.0}, 10);
    CHECK_MSG(Near(follow.centrePx, kMiddle - glm::dvec2(120.0, 0.0), 1e-9),
              "panned 120 left for the finger's 60 right: " + Px(follow.centrePx));
    CHECK_MSG(follow.Held(rules), "and held there");
    // The last held tick zeroed the hold's clock; the tick it came up added one
    // tick; the follow runs once the clock is past 4000 ms: 240 ticks of a float
    // 1/60 s from the last held one, so 239 after the one it came up on.
    const int held = HeldFor(rules, follow, kMiddle, 400);
    CHECK_MSG(held == 239, "held 4000 ms: the follow's first move is the 239th tick after the release (" +
                               std::to_string(held) + ")");
    // A drag that never strays past scale(24) holds nothing: 4 a tick for 6 ticks
    // strays 24 at most, and the view follows on the tick after it comes up.
    Camera::Follow small;
    small.Start(rules, kMiddle, kView, kWide);
    Drag(rules, small, kMiddle, {100.0, 100.0}, {4.0, 0.0}, 6);
    CHECK_MSG(!small.Held(rules) && HeldFor(rules, small, kMiddle, 400) == 1,
              "a drag that strays no more than scale(24) holds nothing");
}

void AFastDragBackTowardThePlayerLetsItGo() {
    const Camera::Rules rules = Decoded();
    Camera::Follow follow;
    follow.Start(rules, kMiddle, kView, kWide);
    Camera::TouchRecord record;
    glm::dvec2 at(100.0, 100.0);
    follow.Tick(rules, kMiddle, kView, kWide, kTickS, Touch(record, true, true, at), {});
    for (int tick = 0; tick < 10; ++tick) {
        at += glm::dvec2(6.0, 0.0);
        follow.Tick(rules, kMiddle, kView, kWide, kTickS, Touch(record, false, true, at), {});
    }
    CHECK_MSG(follow.Held(rules), "a drag away holds the view");
    // Back toward the player at 8 a tick: the view comes back at 16, but 8 is under
    // scale(10), so the hold starts again.
    at += glm::dvec2(-8.0, 0.0);
    follow.Tick(rules, kMiddle, kView, kWide, kTickS, Touch(record, false, true, at), {});
    CHECK_MSG(follow.Held(rules) && follow.holdElapsedMs == 0.0, "back toward the player at 8 a tick: still held");
    // At 10 a tick - dragLockValue itself - it lets go.
    at += glm::dvec2(-10.0, 0.0);
    follow.Tick(rules, kMiddle, kView, kWide, kTickS, Touch(record, false, true, at), {});
    CHECK_MSG(!follow.Held(rules), "at 10 a tick, scale(10), the hold is let go");
    follow.Tick(rules, kMiddle, kView, kWide, kTickS, Touch(record, false, false, at), {});
    CHECK_MSG(HeldFor(rules, follow, kMiddle, 400) == 1, "and the view follows as soon as the finger is up");
}

void AWalkOrATeleportLetsTheHoldGo() {
    const Camera::Rules rules = Decoded();
    const auto held = [&rules] {
        Camera::Follow follow;
        follow.Start(rules, kMiddle, kView, kWide);
        Drag(rules, follow, kMiddle, {100.0, 100.0}, {6.0, 0.0}, 10);
        return follow;
    };
    // ScreenPad::isMoving unlocks AFTER the tick's follow: the next tick follows.
    Camera::Follow walked = held();
    glm::dvec2 before = walked.centrePx;
    Camera::Unlocks walk;
    walk.padMoving = true;
    walked.Tick(rules, kMiddle, kView, kWide, kTickS, Camera::Feel{}, walk);
    CHECK_MSG(walked.centrePx == before, "the tick the pads walk, the view is still held");
    CHECK_MSG(HeldFor(rules, walked, kMiddle, 400) == 1, "and it follows from the next");
    // A teleport since the last tick's camera lets it go before this one's.
    Camera::Follow teleported = held();
    before = teleported.centrePx;
    Camera::Unlocks through;
    through.teleported = true;
    teleported.Tick(rules, kMiddle, kView, kWide, kTickS, Camera::Feel{}, through);
    CHECK_MSG(teleported.centrePx != before, "a teleport: the view follows on that tick");
}

void OneTwentyOneLessonIsTheDecodedCallbacks() {
    Hint::Rules rules;
    std::string error;
    CHECK_MSG(Hint::LoadRules(std::string(MAGICPORTALS_PORT_DATA_DIR) + "/camera.json", rules, error), error);
    CHECK(rules.handEntity == "hand_waypoint1" && rules.arrowEntity == "up_down_arrow");
    CHECK(rules.text == "move the camera around" && rules.font == "Matura84_shadow.fnt");
    CHECK(rules.unitsPerFontPx == 0.4 && rules.leftUnits == 50.0 && rules.aboveBottomUnits == 32.0);
    CHECK(rules.fadeMs == 10000.0 && rules.lifeMs == 20000.0 && rules.handAlphaGain == 2.0);
    CHECK(rules.arrowAlpha == 0.3 && rules.arrowAlphaGain == 2.0 && rules.arrowBounce.strideMs == 300.0);
    CHECK(rules.arrowBounce.fromScale == glm::dvec2(1.1, 1.0) && rules.arrowBounce.toScale == glm::dvec2(1.0, 1.1));
    CHECK_MSG(rules.handWaypoints.size() == 4u, "four waypoints");
    if (rules.handWaypoints.size() != 4u) return;
    const glm::dvec2 at[] = {{0.0, 0.0}, {10.0, 0.0}, {10.0, 0.0}, {10.0, 100.0}};
    const double ms[] = {400.0, 0.0, 2600.0, 300.0};
    const Ease::Filter filter[] = {Ease::Filter::SmoothEnd, Ease::Filter::SmoothEnd, Ease::Filter::SmoothBeginning,
                                   Ease::Filter::SmoothEnd};
    for (std::size_t i = 0; i < 4; ++i) {
        CHECK_MSG(rules.handWaypoints[i].atUnits == at[i] && rules.handWaypoints[i].ms == ms[i] &&
                      rules.handWaypoints[i].filter == filter[i],
                  "waypoint " + std::to_string(i) + " as the callback adds it");
    }

    // The hand, a tick at a time on the layer's clock.
    const double frameMs = kTickS * 1000.0;
    Hint::Hand hand;
    hand.Tick(rules, frameMs);
    CHECK_MSG(hand.OffsetUnits(rules) == glm::dvec2(0.0) && hand.Alpha(rules) == 1.0,
              "its first call: where the level put it, whole");
    double mostRight = 0.0;
    double mostDown = 0.0;
    int backAt = -1;
    bool left = false;
    for (int tick = 2; tick <= 400; ++tick) {
        hand.Tick(rules, frameMs);
        const glm::dvec2 offset = hand.OffsetUnits(rules);
        mostRight = std::max(mostRight, offset.x);
        mostDown = std::max(mostDown, offset.y);
        left = left || hand.frame != 0;
        if (backAt < 0 && left && hand.frame == 0) backAt = tick;
    }
    CHECK_MSG(std::fabs(mostRight - 10.0) < 1e-9 && mostDown > 99.0 && mostDown <= 100.0,
              "it steps 10 right and goes 100 down: " + std::to_string(mostRight) + ", " + std::to_string(mostDown));
    // A round is 400 + 0 + 2600 + 300 ms: back where it began 3300 ms after the first
    // call, give or take the tick the zero-length waypoint takes.
    CHECK_MSG(backAt > 0 && std::fabs((backAt - 1) * frameMs - 3300.0) <= 2.0 * frameMs,
              "and is back where it began " + std::to_string((backAt - 1) * frameMs) + " ms after its first call");
    // Whole for 10 s, half at 15, gone past 20: the tick that passes 20000 ms deletes it.
    Hint::Hand aging;
    int goneAt = -1;
    double atTen = -1.0;
    double atFifteen = -1.0;
    for (int tick = 1; tick <= 1300 && goneAt < 0; ++tick) {
        aging.Tick(rules, frameMs);
        if (tick == 599) atTen = aging.Alpha(rules);
        if (tick == 900) atFifteen = aging.Alpha(rules);
        if (aging.gone) goneAt = tick;
    }
    CHECK_MSG(atTen == 1.0, "whole just before 10 s: " + std::to_string(atTen));
    CHECK_MSG(std::fabs(atFifteen - 0.5) < 0.002, "half at 15 s: " + std::to_string(atFifteen));
    CHECK_MSG(goneAt == 1200, "deleted on the tick its life passes 20000 ms: " + std::to_string(goneAt));

    // The arrow: 0.6 fading to 0 over 20 s, its size swinging (1.1, 1) -> (1, 1.1)
    // and back, 300 ms a leg.
    Hint::Arrow arrow;
    CHECK(arrow.Alpha(rules) == 0.6 && arrow.Scale(rules) == glm::dvec2(1.1, 1.0));
    for (int tick = 0; tick < 18; ++tick) arrow.Tick(rules, frameMs);
    const glm::dvec2 leg = arrow.Scale(rules);
    CHECK_MSG(std::fabs(leg.x - 1.0) < 1e-3 && std::fabs(leg.y - 1.1) < 1e-3, "at 300 ms, (1, 1.1): " + Px(leg));
    CHECK_MSG(std::fabs(arrow.Alpha(rules) - 0.6 * (1.0 - 300.0 / 20000.0)) < 1e-6, "its alpha 0.6 x (1 - t / 20000)");

    // The text: ETHTextDrawer's byte, from the view's bottom-left.
    CHECK(Hint::TextAlpha(rules, 0.0) == 1.0);
    CHECK(Hint::TextAlpha(rules, 5000.0) == 127.0 / 255.0);
    CHECK(Hint::TextAlpha(rules, 10000.0) == 0.0);
    CHECK(Hint::TextTopLeft(rules, kView) == glm::dvec2(50.0, 224.0));
}

void ItNeverShowsPastTheLevel() {
    const Camera::Rules rules = FollowOnly();
    Camera::Follow follow;
    follow.Start(rules, {384.0, 0.0}, kView, kBounds);
    const glm::dvec2 far[] = {{-500.0, -500.0}, {2000.0, 900.0}};
    for (const glm::dvec2& player : far) {
        bool inside = true;
        for (int i = 0; i < 240; ++i) {
            follow.Tick(rules, player, kView, kBounds, kDt);
            const glm::dvec2 c = follow.centrePx;
            inside = inside && c.x - kView.x / 2 >= 0.0 && c.x + kView.x / 2 <= kBounds.x &&
                     c.y - kView.y / 2 >= 0.0 && c.y + kView.y / 2 <= kBounds.y;
        }
        CHECK_MSG(inside, "following a player at " + Px(player));
    }
    // A window reshaped under it: the camera is held inside again on the next tick.
    follow.centrePx = glm::dvec2(700.0, 128.0);
    follow.Tick(rules, glm::dvec2(700.0, 128.0), kView, kBounds, kDt);
    CHECK(follow.centrePx.x <= 540.5);
}

void ThePortsViewIsTwoHundredFiftySixTall() {
    // view.json is the port's own and committed, so it is always there. Its
    // number is an inference its note gives the evidence for, so it is pinned
    // here: a change to it is a decision, and should be seen.
    double height = 0.0;
    std::string error;
    CHECK_MSG(Camera::LoadViewHeight(std::string(MAGICPORTALS_PORT_DATA_DIR) + "/view.json", height, error), error);
    CHECK(height == 256.0);
}

// ---- the earthquake ------------------------------------------------------------

// The layer's tick, as its clock counts it: a float 1/60 s in milliseconds.
constexpr double kTickMs = static_cast<double>(1.0f / 60.0f) * 1000.0;

struct Tremble {
    int tick = 0;
    double px = 0.0;
};

// `ticks` updates on the layer's clock from `clockMs`, which is carried on; each
// tremble with the tick it fell on, counted from `firstTick`.
std::vector<Tremble> Shake(const Earthquake::Rules& rules, Earthquake::Generator& quake, int ticks, double& clockMs,
                           int firstTick = 0, double tickMs = kTickMs) {
    std::vector<Tremble> out;
    for (int tick = firstTick; tick < firstTick + ticks; ++tick) {
        const double px = quake.Update(rules, clockMs);
        if (px != 0.0) out.push_back(Tremble{tick, px});
        clockMs += tickMs;
    }
    return out;
}

Earthquake::Rules LoadQuake() {
    Earthquake::Rules rules;
    std::string error;
    const bool read =
        Earthquake::LoadRules(std::string(MAGICPORTALS_PORT_DATA_DIR) + "/earthquake.json", rules, error);
    CHECK_MSG(read, error);
    return rules;
}

std::string Sequence(const std::vector<Tremble>& trembles) {
    std::string out;
    for (const Tremble& t : trembles) out += " " + std::to_string(t.tick) + ":" + std::to_string(t.px);
    return out;
}

void TheEarthquakeIsTheDecodedGenerator() {
    const Earthquake::Rules rules = LoadQuake();
    // EarthquakeGenerator's own literals (bytes 109623..110425) and R12's screen.
    CHECK(rules.forceScale == 0.35);
    CHECK(rules.capPx == 50.0);
    CHECK(rules.stopPx == 1.0);
    CHECK(rules.trembleIntervalMs == 70.0);
    CHECK(rules.backScale == 0.5);
    CHECK(rules.forthFirst);
    CHECK(rules.screenPxPerUnit == 2.8125);
    CHECK(rules.wallScaleKey == "destroyEffectScale");
    CHECK(rules.wallEntity == "breakable_wall");
    // Both spellings a level uses, and not the torch's light wall.
    CHECK(Earthquake::IsEntity("breakable_wall", rules.wallEntity));
    CHECK(Earthquake::IsEntity("breakable_wall.ent", rules.wallEntity));
    CHECK(!Earthquake::IsEntity("light_wall.ent", rules.wallEntity));
    CHECK(rules.mouthLeg == 3);
    CHECK(rules.mouthBiasAbove == 0.2);

    // Every call site's intensity and the side of the update it lands on, as
    // the listing pushes it and as its caller's .ent says it runs.
    using Earthquake::Call;
    using Earthquake::When;
    const struct {
        Call call;
        double intensity;
        When when;
    } rows[] = {{Call::Fall, 20.0, When::AfterUpdate},
                {Call::FallNoGravity, 5.0, When::AfterUpdate},
                {Call::BarrelBlast, 50.0, When::BeforeUpdate},
                {Call::BeholderDeath, 50.0, When::BeforeUpdate},
                {Call::DarkDragonDeath, 50.0, When::BeforeUpdate},
                {Call::GhostDeath, 50.0, When::AfterUpdate},
                {Call::WallStruck, 30.0, When::BeforeUpdate},
                {Call::WallDestroyed, 20.0, When::AfterUpdate},
                {Call::StoneDespawned, 6.0, When::BeforeUpdate},
                {Call::BossRockBroken, 5.0, When::BeforeUpdate},
                {Call::BeholderHurt, 24.0, When::BeforeUpdate},
                {Call::DarkDragonMouth, 10.0, When::BeforeUpdate},
                {Call::DarkDragonShot, 15.0, When::BeforeUpdate},
                {Call::DoorLanded, 4.0, When::AfterUpdate},
                {Call::DoorOpening, 4.0, When::AfterUpdate},
                {Call::DoorClosing, 4.0, When::AfterUpdate},
                {Call::FireballEnd, 5.0, When::BeforeUpdate},
                {Call::EnemyShotEnd, 10.0, When::BeforeUpdate},
                {Call::MinionKilled, 20.0, When::BeforeUpdate},
                {Call::DoorUnlocked, 20.0, When::AfterUpdate},
                {Call::ShockDiamondPicked, 15.0, When::AfterUpdate},
                {Call::FireDiamondPicked, 15.0, When::AfterUpdate},
                {Call::ShockStrike, 25.0, When::AfterUpdate},
                {Call::LightWallGone, 20.0, When::AfterUpdate},
                // The shot's three (step 103): blockPortalEffects is PortalManager::update's,
                // a controller before the generator; anti_projectile_wall.ent is static;
                // projectile.ent is not.
                {Call::NoPortalTap, 7.0, When::BeforeUpdate},
                {Call::ShotBlocked, 5.0, When::AfterUpdate},
                {Call::ShotFailedInField, 5.0, When::BeforeUpdate},
                // A shot an anti-portal agent killed (step 126): the agent's own
                // callback, and anti_portal_agent.ent is static.
                {Call::AgentKilled, 20.0, When::AfterUpdate},
                // The impact rows (step 104): the factor of hitIntensity each
                // callback multiplies, inside the physics step.
                {Call::PlayerImpact, 0.6, When::BeforeUpdate},
                {Call::StoneImpact, 2.0, When::BeforeUpdate},
                {Call::MinionImpact, 0.8, When::BeforeUpdate},
                // 4-01's rocket (step 123): rocket_launch is a static entity.
                {Call::RocketLaunch, 3.0, When::AfterUpdate}};
    CHECK(std::size(rows) == Earthquake::kCalls);
    for (const auto& row : rows) {
        const Earthquake::CallRule& rule = rules.Of(row.call);
        CHECK_MSG(rule.intensity == row.intensity && rule.when == row.when, Earthquake::KeyOf(row.call));
    }
}

// ---- the impact rows (step 104) ----------------------------------------------------

// The three begin-contact callbacks as the listing has them (earthquake.json
// player_impact, stone_impact, minion_impact): each gate, what each passes over,
// and hitIntensity as the approach speed in level px a second over 50.
void TheImpactRowsAreTheDecodedCallbacks() {
    const Earthquake::Rules rules = LoadQuake();
    using Earthquake::Call;
    CHECK(rules.hitPxPerUnit == 50.0);
    CHECK(rules.touchPx == 0.5);
    // b2_polygonRadius, 0.5 screen px, at R12's 720: 0.178 level units.
    CHECK(std::fabs(Earthquake::ToUnits(rules, rules.touchPx) - 0.5 / 2.8125) < 1e-15);
    CHECK(rules.hitAbove[static_cast<std::size_t>(Call::PlayerImpact)] == 0.0);
    CHECK(rules.hitAbove[static_cast<std::size_t>(Call::StoneImpact)] == 2.0);
    CHECK(rules.hitAbove[static_cast<std::size_t>(Call::MinionImpact)] == 0.0);
    CHECK(rules.entity[static_cast<std::size_t>(Call::PlayerImpact)] == "dark_mage");
    CHECK(rules.entity[static_cast<std::size_t>(Call::StoneImpact)] == "rolling_stone");
    CHECK(rules.entity[static_cast<std::size_t>(Call::MinionImpact)] == "minion");
    // A 300 px/s fall onto a floor: hitIntensity 6.
    CHECK(Earthquake::HitIntensity(rules, glm::dvec2(0.0, 300.0), glm::dvec2(0.0, 1.0)) == 6.0);
    // Moving away is below 0, and along the surface 0.
    CHECK(Earthquake::HitIntensity(rules, glm::dvec2(0.0, -300.0), glm::dvec2(0.0, 1.0)) == -6.0);
    CHECK(Earthquake::HitIntensity(rules, glm::dvec2(300.0, 0.0), glm::dvec2(0.0, 1.0)) == 0.0);
    // The player: above 0, and not against isCrate's six names - which leave out
    // the small crates.
    CHECK(!Earthquake::ImpactCalls(rules, Call::PlayerImpact, 0.0, ""));
    CHECK(Earthquake::ImpactCalls(rules, Call::PlayerImpact, 0.001, ""));
    for (const char* crate : {"crate", "crate.ent", "crate_no_emissive", "crate_no_emissive.ent", "crate_low_emissive",
                              "crate_low_emissive.ent"}) {
        CHECK_MSG(!Earthquake::ImpactCalls(rules, Call::PlayerImpact, 5.0, crate), crate);
    }
    CHECK(Earthquake::ImpactCalls(rules, Call::PlayerImpact, 5.0, "crate_small.ent"));
    CHECK(Earthquake::ImpactCalls(rules, Call::PlayerImpact, 5.0, "crate_small_no_emissive"));
    // The stone: above 2, against anything.
    CHECK(!Earthquake::ImpactCalls(rules, Call::StoneImpact, 2.0, ""));
    CHECK(Earthquake::ImpactCalls(rules, Call::StoneImpact, 2.001, "crate"));
    // The minion: above 0, and not against the killer floor by its bare name.
    CHECK(!Earthquake::ImpactCalls(rules, Call::MinionImpact, 5.0, "enemy_killer"));
    CHECK(Earthquake::ImpactCalls(rules, Call::MinionImpact, 5.0, "enemy_killer.ent"));
    CHECK(!Earthquake::ImpactCalls(rules, Call::MinionImpact, 0.0, ""));
    // No other row is an impact row.
    CHECK(!Earthquake::ImpactCalls(rules, Call::Fall, 100.0, ""));
}

// An impact's shake against D11's table (section 1c, and section 4's notes): the
// call is factor x hitIntensity, so its amplitude is 0.35 x factor x v / 50 x
// 2.8125 px. The player's goes sub-pixel below 84.7 px/s of approach and the
// minion's below 63.5; the stone's never calls at 100 or under. F14's landings
// (about 5 px, 3 pairs, forth first) are a player landing at about 417 px/s.
void AnImpactShakesByItsApproachSpeed() {
    const Earthquake::Rules rules = LoadQuake();
    using Earthquake::Call;
    const auto shake = [&rules](Call call, double speedPx, double& firstPx) {
        const double hit = Earthquake::HitIntensity(rules, glm::dvec2(0.0, speedPx), glm::dvec2(0.0, 1.0));
        firstPx = 0.0;
        if (!Earthquake::ImpactCalls(rules, call, hit, "")) return std::size_t{0};
        Earthquake::Generator quake;
        quake.Reset(rules);
        quake.Start(rules, rules.Of(call).intensity * hit);
        double clockMs = 0.0;
        const std::vector<Tremble> got = Shake(rules, quake, 400, clockMs);
        firstPx = got.empty() ? 0.0 : got.front().px;
        return got.size();
    };
    const struct {
        Call call;
        double speedPx;
        std::size_t trembles; // two a pair; a sub-pixel call trembles forth once and stops
        double firstPx;
    } table[] = {
        // D11 section 4's worked example: a 300 px/s landing, 1.26 u = 3.54 px, 2 pairs.
        {Call::PlayerImpact, 300.0, 4, 0.35 * 0.6 * 6.0 * 2.8125},
        // F14: about 5 px, 3 pairs.
        {Call::PlayerImpact, 417.0, 6, 0.35 * 0.6 * 8.34 * 2.8125},
        // The sub-pixel edge, C23: 84.7 px/s is one px.
        {Call::PlayerImpact, 84.0, 1, 0.35 * 0.6 * 1.68 * 2.8125},
        {Call::PlayerImpact, 86.0, 2, 0.35 * 0.6 * 1.72 * 2.8125},
        {Call::MinionImpact, 63.0, 1, 0.35 * 0.8 * 1.26 * 2.8125},
        {Call::MinionImpact, 200.0, 4, 0.35 * 0.8 * 4.0 * 2.8125},
        // The stone: nothing at 100 px/s (hitIntensity 2), and above it 2 x.
        {Call::StoneImpact, 100.0, 0, 0.0},
        {Call::StoneImpact, 150.0, 6, 0.35 * 2.0 * 3.0 * 2.8125},
        {Call::StoneImpact, 300.0, 8, 0.35 * 2.0 * 6.0 * 2.8125},
        // A hard one caps: 2 x 40 is 78.75 px, held to 50, and 6 pairs.
        {Call::StoneImpact, 2000.0, 12, 50.0},
    };
    for (const auto& row : table) {
        double firstPx = 0.0;
        const std::size_t trembles = shake(row.call, row.speedPx, firstPx);
        CHECK_MSG(trembles == row.trembles && std::fabs(firstPx - row.firstPx) < 1e-9,
                  std::string(Earthquake::KeyOf(row.call)) + " at " + std::to_string(row.speedPx) + " px/s: " +
                      std::to_string(trembles) + " trembles from " + std::to_string(firstPx) + " px");
    }
    // Where the sub-pixel edges fall: 1 / (0.35 x factor x 2.8125 / 50) px/s.
    const double playerEdge = 1.0 / (0.35 * 0.6 * 2.8125 / 50.0);
    const double minionEdge = 1.0 / (0.35 * 0.8 * 2.8125 / 50.0);
    CHECK_MSG(std::fabs(playerEdge - 84.656) < 1e-3 && std::fabs(minionEdge - 63.492) < 1e-3,
              std::to_string(playerEdge) + ", " + std::to_string(minionEdge));
}

// ---- where a contact begins (sim/Impacts) -------------------------------------------

Impacts::Shape Capsule(const glm::dvec2& centre, double reach, double radius) {
    return Impacts::Shape{{centre - glm::dvec2(0.0, reach), centre + glm::dvec2(0.0, reach)}, radius};
}

Impacts::Shape Box(const glm::dvec2& lo, const glm::dvec2& hi) {
    return Impacts::Shape{{lo, glm::dvec2(hi.x, lo.y), hi, glm::dvec2(lo.x, hi.y)}, 0.0};
}

bool Unit(const glm::dvec2& got, const glm::dvec2& want) { return Near(got, want, 1e-12); }

// The normal is the line between the two surfaces' nearest points, from the
// first shape to the second, and the gap the distance along it less both radii:
// the player's capsule (10 px round a 24 px core) on a floor, against a wall, at
// a corner, and two circles.
void TheContactNormalIsBetweenTheNearestPoints() {
    const Impacts::Shape player = Capsule({0.0, 0.0}, 12.0, 10.0);
    Impacts::Gap gap = Impacts::Between(player, Box({-100.0, 22.1}, {100.0, 40.0}));
    CHECK_MSG(std::fabs(gap.px - 0.1) < 1e-9 && Unit(gap.normal, {0.0, 1.0}), "floor " + std::to_string(gap.px));
    gap = Impacts::Between(player, Box({10.5, -50.0}, {30.0, 50.0}));
    CHECK_MSG(std::fabs(gap.px - 0.5) < 1e-9 && Unit(gap.normal, {1.0, 0.0}), "wall " + std::to_string(gap.px));
    // A corner at (3, 23), 3 across and 11 down from the core's end.
    gap = Impacts::Between(player, Box({3.0, 23.0}, {50.0, 60.0}));
    const double reach = std::sqrt(3.0 * 3.0 + 11.0 * 11.0);
    CHECK_MSG(std::fabs(gap.px - (reach - 10.0)) < 1e-9 && Unit(gap.normal, glm::dvec2(3.0, 11.0) / reach),
              "corner " + std::to_string(gap.px) + " " + Px(gap.normal));
    // Resting a little into the floor still touches, below 0.
    gap = Impacts::Between(player, Box({-100.0, 21.75}, {100.0, 40.0}));
    CHECK(std::fabs(gap.px + 0.25) < 1e-9 && Unit(gap.normal, {0.0, 1.0}));
    // Two circles: centres 10 apart along (0.8, 0.6).
    gap = Impacts::Between(Impacts::Shape{{{0.0, 0.0}}, 5.0}, Impacts::Shape{{{8.0, 6.0}}, 4.0});
    CHECK(std::fabs(gap.px - 1.0) < 1e-9 && Unit(gap.normal, {0.8, 0.6}));
    // And the other way round, the normal turned with it.
    gap = Impacts::Between(Box({-100.0, 22.1}, {100.0, 40.0}), player);
    CHECK(std::fabs(gap.px - 0.1) < 1e-9 && Unit(gap.normal, {0.0, -1.0}));
    // A core inside a box: touching, along the line between their centres.
    gap = Impacts::Between(Impacts::Shape{{{0.0, 5.0}}, 3.0}, Box({-10.0, 0.0}, {10.0, 40.0}));
    CHECK(gap.px < 0.0 && Unit(gap.normal, {0.0, 1.0}));
}

entt::entity SolidBox(entt::registry& registry, const glm::dvec2& centrePx, const glm::dvec2& sizePx,
                      bool trigger = false) {
    const entt::entity entity = registry.create();
    registry.emplace<Supersonic::TransformComponent>(entity).position = Units::ToWorld(centrePx.x, centrePx.y);
    auto& box = registry.emplace<Supersonic::BoxColliderComponent>(entity);
    box.size = glm::vec3(Units::ToMetres(sizePx.x), Units::ToMetres(sizePx.y), 1.0f);
    box.isTrigger = trigger;
    return entity;
}

// A body's first look only marks what it touches; a pair new since the last look
// is a begin, carrying what the body took into the step and the steps before it,
// back to when it last met or left something; a pair still touching is not; and a
// trigger is never touched. A stone of 10 px over a floor whose top is at y 100,
// one step of 1/60 s at a time: from where it was, at the velocity it carried, to
// where the step left it. The engine's solver takes a closing pair's approach out
// before it meets, so the landing step's own velocity is gravity's alone and the
// speed it came in at is an earlier step's.
void APairIsABeginOnceUntilItParts() {
    entt::registry registry;
    const entt::entity floor = SolidBox(registry, {0.0, 120.0}, {400.0, 40.0});
    SolidBox(registry, {0.0, 60.0}, {400.0, 100.0}, true); // a trigger the stone moves through
    const entt::entity stone = registry.create();
    registry.emplace<Supersonic::TransformComponent>(stone).position = Units::ToWorld(0.0, 50.0);
    registry.emplace<Supersonic::SphereColliderComponent>(stone).radius = Units::ToMetres(10.0);
    registry.emplace<Supersonic::RigidBodyComponent>(stone);
    const std::vector<Impacts::Watched> watched{{stone, Impacts::Kind::Stone, {0.0, 0.0}}};
    const double touch = 0.5 / 2.8125;
    const double dt = 1.0 / 60.0;
    Impacts::Watch watch;
    // A step: the stone at `fromY` carrying `downPx` a second into it, and where
    // the step left it.
    const auto step = [&](double fromY, double downPx, double toY) {
        registry.get<Supersonic::TransformComponent>(stone).position = Units::ToWorld(0.0, fromY);
        registry.get<Supersonic::RigidBodyComponent>(stone).velocity = glm::vec3(0.0f, -Units::ToMetres(downPx), 0.0f);
        watch.BeforeStep(registry, watched);
        registry.get<Supersonic::TransformComponent>(stone).position = Units::ToWorld(0.0, toY);
        return watch.AfterStep(registry, touch, dt);
    };
    // The first look, resting on the floor: marked, not a begin.
    CHECK_MSG(step(90.0, 0.0, 90.0).empty(), "a first look marks what it touches");
    // Lifted off and falling: nothing.
    CHECK(step(60.0, 0.0, 60.0).empty());
    CHECK(step(80.0, 250.0, 84.1667).empty());
    // The step before the landing: 250 px/s from where the last step left it, left
    // at 89.8 just out of touch, its approach taken out by the solver - not yet a
    // begin. (From where the last step left it: a body the tick itself has moved
    // is a teleported one, and keeps nothing it carried.)
    CHECK_MSG(step(84.1667, 250.0, 89.8).empty(), "apart is not touching");
    // The landing: it arrives carrying gravity's 20 px/s, and the begin keeps the
    // 250 of the step before, which is what came along the normal fastest.
    std::vector<Impacts::Begin> begins = step(89.8, 20.0, 90.1);
    CHECK_MSG(begins.size() == 1, std::to_string(begins.size()) + " begins on the landing step");
    if (begins.size() == 1) {
        const Impacts::Begin& begin = begins.front();
        CHECK(begin.body == stone && begin.other == floor && begin.kind == Impacts::Kind::Stone);
        CHECK_MSG(Unit(begin.normal, {0.0, 1.0}), Px(begin.normal));
        CHECK_MSG(Near(begin.velocityPx, {0.0, 20.0}, 1e-4) && Near(begin.atPx, {0.0, 89.8}, 1e-4),
                  Px(begin.velocityPx) + " " + Px(begin.atPx));
        // Kept since it left the floor at y 60: the landing's 20, then 250 twice.
        CHECK_MSG(begin.recentPx.size() == 3 && Near(begin.recentPx[1], {0.0, 250.0}, 1e-4) &&
                      Near(begin.recentPx[2], {0.0, 250.0}, 1e-4),
                  std::to_string(begin.recentPx.size()) + " steps kept");
        CHECK_MSG(Near(begin.Approaching({0.0, 1.0}), {0.0, 250.0}, 1e-4), Px(begin.Approaching({0.0, 1.0})));
        // Along a line it never came along, the newest.
        CHECK(Near(begin.Approaching({1.0, 0.0}), {0.0, 20.0}, 1e-4));
        CHECK_MSG(std::fabs(begin.gapPx + 0.1) < 1e-4, std::to_string(begin.gapPx));
        CHECK(Near(begin.otherAtPx, {0.0, 120.0}, 1e-4));
        Impacts::Begin carried;
        CHECK(watch.Carried(stone, carried) && Near(carried.Approaching({0.0, 1.0}), {0.0, 250.0}, 1e-4));
    }
    // Resting on it, gravity pressing it in each step: nothing new.
    CHECK(step(90.1, 20.0, 90.1).empty());
    CHECK(step(90.1, 20.0, 90.1).empty());
    // Thrown up off it: parted. Then put down within the skin: a begin where it
    // is, with nothing carried.
    CHECK(step(90.1, -300.0, 85.1).empty());
    begins = step(85.1, 0.0, 89.95);
    // (The steps before it went up, away from the floor: nothing came down faster.)
    CHECK_MSG(begins.size() == 1 && Near(begins.front().velocityPx, {0.0, 0.0}, 1e-9) &&
                  Unit(begins.front().normal, {0.0, 1.0}),
              std::to_string(begins.size()) + " begins put down");
    // Put there from 30 px away between the steer and the step - farther than the
    // step carries it - it is looked at afresh: marked, and not a begin.
    CHECK(step(89.95, -300.0, 84.95).empty());
    CHECK_MSG(step(60.0, 0.0, 89.95).empty(), "a body put somewhere is looked at afresh");
    CHECK(step(89.95, 20.0, 90.1).empty());
    // Reset: looked at afresh, so marked again.
    watch.Reset();
    CHECK(step(60.0, 0.0, 60.0).empty());
    CHECK(step(89.9, 0.0, 89.9).size() == 1);
}

// A step of the watch over a body on a registry of its own: the body at `from`
// carrying `velocityPx` into the step, and where the step left it. Between two
// such steps the "tick" is whatever the caller does; setting the body somewhere
// other than where the last step left it is what a portal's teleport does.
struct WatchedStep {
    entt::registry& registry;
    Impacts::Watch& watch;
    std::vector<Impacts::Watched> watched;
    std::vector<Impacts::Begin> operator()(const glm::dvec2& from, const glm::dvec2& velocityPx, const glm::dvec2& to) {
        const entt::entity body = watched.front().body;
        registry.get<Supersonic::TransformComponent>(body).position = Units::ToWorld(from.x, from.y);
        registry.get<Supersonic::RigidBodyComponent>(body).velocity =
            glm::vec3(Units::ToMetres(velocityPx.x), -Units::ToMetres(velocityPx.y), 0.0f);
        watch.BeforeStep(registry, watched);
        registry.get<Supersonic::TransformComponent>(body).position = Units::ToWorld(to.x, to.y);
        return watch.AfterStep(registry, 0.5 / 2.8125, 1.0 / 60.0);
    }
};

entt::entity SolidCircle(entt::registry& registry, const glm::dvec2& centrePx, double radiusPx) {
    const entt::entity entity = registry.create();
    registry.emplace<Supersonic::TransformComponent>(entity).position = Units::ToWorld(centrePx.x, centrePx.y);
    registry.emplace<Supersonic::SphereColliderComponent>(entity).radius = Units::ToMetres(radiusPx);
    registry.emplace<Supersonic::RigidBodyComponent>(entity);
    return entity;
}

// A body a portal takes keeps nothing it carried toward the portal. A stone of
// 10 px falls at 600 px/s into a floor portal and comes out of a wall portal
// sideways at 300 px/s - the tick puts it there after the step, as
// Portals.cpp's Teleport does - then drops a few px onto a floor at 60 px/s.
// That landing is read from what it carried since it came out: 60 down, where
// the 600 it fell into the portal at would read ten times as hard.
void ATeleportedBodyKeepsNothingItCarriedIn() {
    entt::registry registry;
    const entt::entity floor = SolidBox(registry, {0.0, 120.0}, {800.0, 40.0});
    const entt::entity stone = SolidCircle(registry, {0.0, 0.0}, 10.0);
    Impacts::Watch watch;
    WatchedStep step{registry, watch, {{stone, Impacts::Kind::Stone, {0.0, 0.0}}}};
    CHECK(step({0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}).empty());
    // Falling at 600, into the portal: nothing touched.
    CHECK(step({0.0, 0.0}, {0.0, 600.0}, {0.0, 10.0}).empty());
    CHECK(step({0.0, 10.0}, {0.0, 600.0}, {0.0, 20.0}).empty());
    CHECK(step({0.0, 20.0}, {0.0, 600.0}, {0.0, 30.0}).empty());
    // Out of the wall portal at (200, 80), going sideways, and down onto the floor.
    CHECK(step({200.0, 80.0}, {300.0, 0.0}, {205.0, 80.5}).empty());
    CHECK(step({205.0, 80.5}, {300.0, 30.0}, {210.0, 84.0}).empty());
    CHECK(step({210.0, 84.0}, {300.0, 60.0}, {215.0, 89.0}).empty());
    std::vector<Impacts::Begin> begins = step({215.0, 89.0}, {300.0, 60.0}, {220.0, 90.05});
    CHECK_MSG(begins.size() == 1, std::to_string(begins.size()) + " begins on the landing");
    if (begins.size() == 1) {
        const Impacts::Begin& begin = begins.front();
        CHECK(begin.other == floor && Unit(begin.normal, {0.0, 1.0}));
        // Kept since it came out: this step's and the three before it, and none of the 600s.
        CHECK_MSG(begin.recentPx.size() == 4, std::to_string(begin.recentPx.size()) + " steps kept");
        CHECK_MSG(Near(begin.Approaching({0.0, 1.0}), {300.0, 60.0}, 1e-4),
                  "the landing came in at " + Px(begin.Approaching({0.0, 1.0})));
    }
    // And a body the tick leaves where the step left it keeps what it carried:
    // the same fall, no portal, lands at the 600.
    watch.Reset();
    CHECK(step({0.0, 50.0}, {0.0, 0.0}, {0.0, 50.0}).empty());
    CHECK(step({0.0, 50.0}, {0.0, 600.0}, {0.0, 60.0}).empty());
    CHECK(step({0.0, 60.0}, {0.0, 600.0}, {0.0, 70.0}).empty());
    CHECK(step({0.0, 70.0}, {0.0, 600.0}, {0.0, 80.0}).empty());
    begins = step({0.0, 80.0}, {0.0, 60.0}, {0.0, 90.05});
    CHECK_MSG(begins.size() == 1 && Near(begins.front().Approaching({0.0, 1.0}), {0.0, 600.0}, 1e-4),
              std::to_string(begins.size()) + " begins, the fall kept");
}

// A minion's spawn drop (Watched::spawnDropPx): the port centres its 7 px circle
// on the marker, the original's entity point is there and its feet reach 18.1 px
// below it, 11.1 lower than the port's (minions.json). A knight whose marker
// stands 16 px over its floor - 51 of the game's 62 - has the original's feet 2.1
// px into it at once: that pair is marked at the first look and the port's 9 px
// fall onto it is no begin. One whose marker stands higher begins where the
// original's lands, aimed from the original's entity point, and not again when
// the port's own circle arrives 11.1 px later.
void ASpawnedMinionIsLookedAtWhereTheOriginalsStands() {
    const glm::dvec2 point(0.0, -11.1);
    const glm::dvec2 drop(0.0, 11.1);
    {
        entt::registry registry;
        SolidBox(registry, {0.0, 240.0}, {400.0, 32.0}); // its top at y 224
        const entt::entity knight = SolidCircle(registry, {0.0, 208.0}, 7.0);
        Impacts::Watch watch;
        WatchedStep step{registry, watch, {{knight, Impacts::Kind::Minion, point, drop}}};
        std::size_t begins = step({0.0, 208.0}, {0.0, 0.0}, {0.0, 208.0}).size();
        begins += step({0.0, 208.0}, {0.0, 120.0}, {0.0, 210.0}).size();
        begins += step({0.0, 210.0}, {0.0, 180.0}, {0.0, 213.0}).size();
        begins += step({0.0, 213.0}, {0.0, 240.0}, {0.0, 216.95}).size(); // its own circle arrives
        begins += step({0.0, 216.95}, {0.0, 20.0}, {0.0, 217.0}).size();
        begins += step({0.0, 217.0}, {0.0, 20.0}, {0.0, 217.0}).size();
        CHECK_MSG(begins == 0, std::to_string(begins) + " begins from a marker 16 px over its floor");
        // Its drop is over: lifted off and put back down, it begins as itself.
        CHECK(step({0.0, 217.0}, {0.0, -120.0}, {0.0, 215.0}).empty());
        CHECK_MSG(step({0.0, 215.0}, {0.0, 60.0}, {0.0, 216.95}).size() == 1, "the drop ends with the landing");
    }
    {
        // Standing where the original's stands, it walks into a step on its floor
        // (a corner 2.9 px under the dropped circle's centre, 7.5 px along) while
        // the port's circle is still coming down at 120 px/s: the begin carries
        // the walk and none of that fall, which the original's never had.
        entt::registry registry;
        SolidBox(registry, {0.0, 240.0}, {400.0, 32.0});
        const entt::entity corner = SolidBox(registry, {33.75, 223.0}, {52.5, 2.0}); // x 7.5 to 60, y 222 to 224
        const entt::entity knight = SolidCircle(registry, {0.0, 208.0}, 7.0);
        Impacts::Watch watch;
        WatchedStep step{registry, watch, {{knight, Impacts::Kind::Minion, point, drop}}};
        CHECK(step({0.0, 208.0}, {0.0, 0.0}, {0.0, 208.0}).empty());
        const std::vector<Impacts::Begin> begins = step({0.0, 208.0}, {60.0, 120.0}, {1.0, 210.0});
        CHECK_MSG(begins.size() == 1, std::to_string(begins.size()) + " begins at the step");
        if (begins.size() == 1) {
            CHECK(begins.front().other == corner);
            CHECK_MSG(begins.front().recentPx.size() == 1 && Near(begins.front().recentPx.front(), {60.0, 0.0}, 1e-4) &&
                          Near(begins.front().velocityPx, {60.0, 120.0}, 1e-4),
                      "carried " + Px(begins.front().recentPx.front()) + " where the port's body carried " +
                          Px(begins.front().velocityPx));
        }
    }
    {
        entt::registry registry;
        const entt::entity floor = SolidBox(registry, {0.0, 240.0}, {400.0, 32.0});
        const entt::entity knight = SolidCircle(registry, {0.0, 180.0}, 7.0);
        Impacts::Watch watch;
        WatchedStep step{registry, watch, {{knight, Impacts::Kind::Minion, point, drop}}};
        CHECK(step({0.0, 180.0}, {0.0, 0.0}, {0.0, 180.0}).empty());
        CHECK(step({0.0, 180.0}, {0.0, 200.0}, {0.0, 190.0}).empty());
        CHECK(step({0.0, 190.0}, {0.0, 300.0}, {0.0, 200.0}).empty());
        // The original's feet at 218.1, then 224.1: its landing.
        std::vector<Impacts::Begin> begins = step({0.0, 200.0}, {0.0, 360.0}, {0.0, 206.0});
        CHECK_MSG(begins.size() == 1, std::to_string(begins.size()) + " begins where the original's lands");
        if (begins.size() == 1) {
            CHECK(begins.front().other == floor);
            CHECK_MSG(Near(begins.front().atPx, {0.0, 200.0}, 1e-4), "aimed from " + Px(begins.front().atPx));
            CHECK(Near(begins.front().Approaching({0.0, 1.0}), {0.0, 360.0}, 1e-4));
        }
        std::size_t again = step({0.0, 206.0}, {0.0, 360.0}, {0.0, 212.0}).size();
        again += step({0.0, 212.0}, {0.0, 360.0}, {0.0, 216.95}).size(); // the port's own circle arrives
        again += step({0.0, 216.95}, {0.0, 20.0}, {0.0, 217.0}).size();
        CHECK_MSG(again == 0, std::to_string(again) + " begins when the port's own circle lands");
    }
    // A body the watch has looked at before is not dropped: the stone's own case
    // above, and a minion put somewhere, which is looked at afresh as itself.
    {
        entt::registry registry;
        SolidBox(registry, {0.0, 240.0}, {400.0, 32.0});
        const entt::entity knight = SolidCircle(registry, {0.0, 100.0}, 7.0);
        Impacts::Watch watch;
        WatchedStep step{registry, watch, {{knight, Impacts::Kind::Minion, point, drop}}};
        CHECK(step({0.0, 100.0}, {0.0, 0.0}, {0.0, 100.0}).empty());
        // Put 100 px lower between the steer and the step: looked at afresh.
        CHECK(step({0.0, 100.0}, {0.0, 0.0}, {0.0, 200.0}).empty());
        // Its own circle 10.5 px over the floor, where the dropped one would be
        // 0.6 into it: nothing.
        CHECK_MSG(step({0.0, 200.0}, {0.0, 300.0}, {0.0, 206.5}).empty(), "a minion put somewhere is not dropped");
        CHECK_MSG(step({0.0, 206.5}, {0.0, 300.0}, {0.0, 216.95}).size() == 1, "as itself");
    }
}

void ATwentyIsFivePairsFiveTicksApart() {
    // The light wall's 20, as L1 measured it on 4-29: -20, +20, -10, +10, -5, +5,
    // -2, +2, -1, +1 screen px (the footage's dy is the world's, so a forth - the
    // camera down - reads negative there). 0.35 x 20 x 2.8125 = 19.6875 px, 7 u.
    const Earthquake::Rules rules = LoadQuake();
    Earthquake::Generator quake;
    quake.Reset(rules);
    quake.Start(rules, 20.0);
    CHECK(quake.amplitudePx == 19.6875);
    double clockMs = 1000.0;
    const std::vector<Tremble> got = Shake(rules, quake, 120, clockMs);
    const double want[] = {19.6875, -19.6875, 9.84375, -9.84375, 4.921875, -4.921875,
                           2.4609375, -2.4609375, 1.23046875, -1.23046875};
    CHECK_MSG(got.size() == std::size(want), Sequence(got));
    bool same = got.size() == std::size(want);
    for (std::size_t i = 0; same && i < got.size(); ++i) {
        // Due at 70 ms: the 5th tick of 16.67 ms (83.3), never the 4th (66.7).
        same = got[i].px == want[i] && got[i].tick == static_cast<int>(i) * 5;
    }
    CHECK_MSG(same, "the forth first, back by the saved value, halved on the back only:" + Sequence(got));
    // In the level's units, the first pair is 7 u each way.
    CHECK(Earthquake::ToUnits(rules, want[0]) == 7.0);
    // Over after the tenth: the stop took 0.615 px to 0.
    CHECK(!quake.Running());
    CHECK(!got.empty() && got.back().tick == 45);
    // And it leaves the phase where it found it, forth next.
    CHECK(quake.forth);
}

void EveryShakeRunsTheDecodedPairs() {
    // D11 section 1c's table at 720p (D11_check C11): pairs before the stop, and
    // the opening amplitude, capped at 50 px.
    const Earthquake::Rules rules = LoadQuake();
    const struct {
        double intensity;
        std::size_t pairs;
        double firstPx;
    } table[] = {{3.0, 2, 2.953125},  {4.0, 2, 3.9375},   {5.0, 3, 4.921875},  {6.0, 3, 5.90625},
                 {7.0, 3, 6.890625},  {10.0, 4, 9.84375}, {15.0, 4, 14.765625}, {20.0, 5, 19.6875},
                 {24.0, 5, 23.625},   {25.0, 5, 24.609375}, {30.0, 5, 29.53125}, {40.0, 6, 39.375},
                 {50.0, 6, 49.21875}, {60.0, 6, 50.0}};
    for (const auto& row : table) {
        Earthquake::Generator quake;
        quake.Reset(rules);
        quake.Start(rules, row.intensity);
        double clockMs = 0.0;
        const std::vector<Tremble> got = Shake(rules, quake, 200, clockMs);
        // Near rather than equal: 0.35 has no exact double, so 4, 15 and 30 land
        // a last bit under the decimal the table prints.
        const bool ok = got.size() == row.pairs * 2 && !got.empty() && std::fabs(got.front().px - row.firstPx) < 1e-9 &&
                        got.back().px < 0.0 && !quake.Running();
        CHECK_MSG(ok, "intensity " + std::to_string(row.intensity) + ":" + Sequence(got));
    }
}

void ACallOverwritesAndTheBackUndoesTheOldForth() {
    // F6 (INDEX), decoded (D11_check C21): a fireball's 5 trembles forth, then a
    // barrel's 50 and the wall's 20 land before its back. The back undoes the 5's
    // saved 4.92 and halves the 20, which is therefore never shown whole:
    // -5, +5, -10, +10, -5, +5, -2, +2, -1, +1 on the footage.
    const Earthquake::Rules rules = LoadQuake();
    Earthquake::Generator quake;
    quake.Reset(rules);
    double clockMs = 0.0;
    quake.Start(rules, 5.0);
    std::vector<Tremble> got = Shake(rules, quake, 2, clockMs);
    quake.Start(rules, 50.0);
    quake.Start(rules, 20.0); // over the 50, not the larger of the two
    const std::vector<Tremble> rest = Shake(rules, quake, 200, clockMs, 2);
    got.insert(got.end(), rest.begin(), rest.end());
    const double want[] = {4.921875, -4.921875, 9.84375, -9.84375, 4.921875, -4.921875,
                           2.4609375, -2.4609375, 1.23046875, -1.23046875};
    bool same = got.size() == std::size(want);
    for (std::size_t i = 0; same && i < got.size(); ++i) same = got[i].px == want[i];
    CHECK_MSG(same, Sequence(got));
}

void ASubPixelCallLeavesTheNextShakeOpeningOnABack() {
    // D11 section 1e.2, D11_check C23: a call under 1 px trembles forth once and
    // is stopped, leaving the phase on a back with a sub-pixel saved value. The
    // next shake then opens with that invisible back and halves before it is seen
    // - one reading of F16's half-size light wall. The player's and a minion's
    // soft impacts call under 1 px (rows 4 and 25, step 104); the machine keeps
    // what follows.
    const Earthquake::Rules rules = LoadQuake();
    Earthquake::Generator quake;
    quake.Reset(rules);
    double clockMs = 0.0;
    quake.Start(rules, 0.3); // 0.295 px
    const std::vector<Tremble> small = Shake(rules, quake, 10, clockMs);
    CHECK(small.size() == 1 && std::fabs(small.front().px - 0.2953125) < 1e-12);
    CHECK(!quake.Running() && !quake.forth);
    quake.Start(rules, 20.0);
    const std::vector<Tremble> next = Shake(rules, quake, 200, clockMs, 10);
    CHECK_MSG(next.size() >= 2 && std::fabs(next[0].px + 0.2953125) < 1e-12 && next[1].px == 9.84375, Sequence(next));
}

void ACallEveryTickHoldsTheShakeWhole() {
    // The dark dragon's mouth leg (row 16): 10 re-armed on every frame, so every
    // forth is the whole 9.84 and every back undoes it; F16 shows -10, +10 four
    // pairs running, then the decay once the leg is over.
    const Earthquake::Rules rules = LoadQuake();
    Earthquake::Generator quake;
    quake.Reset(rules);
    double clockMs = 0.0;
    std::vector<Tremble> held;
    for (int tick = 0; tick < 48; ++tick) {
        quake.Start(rules, 10.0);
        const double px = quake.Update(rules, clockMs);
        if (px != 0.0) held.push_back(Tremble{tick, px});
        clockMs += kTickMs;
    }
    bool whole = held.size() == 10;
    for (std::size_t i = 0; whole && i < held.size(); ++i) whole = std::fabs(held[i].px) == 9.84375;
    CHECK_MSG(whole, Sequence(held));
    // Once the calls stop, the last one's whole forth and back, and only then
    // the halving.
    const std::vector<Tremble> decay = Shake(rules, quake, 200, clockMs, 48);
    CHECK_MSG(decay.size() >= 3 && decay[0].px == 9.84375 && decay[1].px == -9.84375 && decay[2].px == 4.921875,
              Sequence(decay));
}

void TheIntervalIsSeventyMilliseconds() {
    // Every 5th frame at 60 fps and every 3rd at 30 (D11 section 1d): the 70 ms
    // is a floor, met on the first frame past it.
    const Earthquake::Rules rules = LoadQuake();
    Earthquake::Generator quake;
    quake.Reset(rules);
    quake.Start(rules, 20.0);
    double clockMs = 0.0;
    const std::vector<Tremble> slow = Shake(rules, quake, 60, clockMs, 0, 1000.0 / 30.0);
    CHECK_MSG(slow.size() >= 3 && slow[1].tick == 3 && slow[2].tick == 6, Sequence(slow));
    // And exactly 70 is due: TNS is "not less than" (D11_check C8).
    Earthquake::Generator exact;
    exact.Reset(rules);
    exact.Start(rules, 20.0);
    CHECK(exact.Update(rules, 0.0) > 0.0);
    CHECK(exact.Update(rules, 69.9) == 0.0);
    CHECK(exact.Update(rules, 70.0) < 0.0);
}

void TheMouthLegIsTheFourthWaypoint() {
    // The callback re-arms the shake on waypoint 3 past raw bias 0.2 (#548-551 the
    // getFrame, #570 its compare with 3, #573 the bias's with 0.2), both read from
    // the WaypointManager the sim plays - DarkDragon::Path (planning step 99), since
    // the merge of the two tracks (planning step 112) the leg the layer asks about.
    const Earthquake::Rules rules = LoadQuake();
    CHECK(!Earthquake::InMouthLeg(rules, 2, 0.9));
    CHECK(!Earthquake::InMouthLeg(rules, 3, 0.2));
    CHECK(Earthquake::InMouthLeg(rules, 3, 0.2001));
    CHECK(Earthquake::InMouthLeg(rules, 3, 0.999));
    CHECK(!Earthquake::InMouthLeg(rules, 4, 0.5));
    CHECK(!Earthquake::InMouthLeg(rules, 0, 0.5));
    // Played as the sim plays it, a 60th of a second an update: darkdragon.json
    // appearing.waypoints hold 2500, 0, 1000, 1000, 700, 2200 and 0 ms. The first
    // update restarts the timer with no time and the 0 ms waypoint takes an update
    // of its own, so leg 3 past its bias is updates 224 to 271 - 48 updates, the
    // 800 ms the shake is held whole for.
    MagicPortals::DarkDragon::Path path;
    for (const double ms : {2500.0, 0.0, 1000.0, 1000.0, 700.0, 2200.0, 0.0}) {
        MagicPortals::DarkDragon::Path::Point point;
        point.ms = ms;
        path.points.push_back(point);
    }
    int first = -1;
    int last = -1;
    int due = 0;
    for (int update = 1; update <= 600; ++update) {
        path.Update(1000.0 / 60.0);
        if (!Earthquake::InMouthLeg(rules, path.frame, path.Bias())) continue;
        if (first < 0) first = update;
        last = update;
        ++due;
    }
    CHECK_MSG(first == 224 && last == 271 && due == 48,
              "the mouth leg past its bias: updates " + std::to_string(first) + " to " + std::to_string(last) + ", " +
                  std::to_string(due) + " of them");
}

void runTests() {
    ThePortsViewIsTwoHundredFiftySixTall();
    TheViewStaysInsideTheLevel();
    ThereIsNoOpeningHold();
    ItNeverShowsPastTheLevel();
    CameraJsonPinsTheDragAndItsSwitch();
    TheSwitchOffIsTheOldCamera();
    NoTouchIsTheOldFollow();
    TheCamerasTouchIsTheEnginesInput();
    ADragPansTheViewAtTwiceItsMove();
    AHeldTouchStopsTheFollow();
    AMovingHandleLocksTheFinger();
    ADragAwayHoldsTheViewFourSeconds();
    AFastDragBackTowardThePlayerLetsItGo();
    AWalkOrATeleportLetsTheHoldGo();
    OneTwentyOneLessonIsTheDecodedCallbacks();
    TheEarthquakeIsTheDecodedGenerator();
    ATwentyIsFivePairsFiveTicksApart();
    EveryShakeRunsTheDecodedPairs();
    ACallOverwritesAndTheBackUndoesTheOldForth();
    ASubPixelCallLeavesTheNextShakeOpeningOnABack();
    ACallEveryTickHoldsTheShakeWhole();
    TheIntervalIsSeventyMilliseconds();
    TheMouthLegIsTheFourthWaypoint();
    TheImpactRowsAreTheDecodedCallbacks();
    AnImpactShakesByItsApproachSpeed();
    TheContactNormalIsBetweenTheNearestPoints();
    APairIsABeginOnceUntilItParts();
    ATeleportedBodyKeepsNothingItCarriedIn();
    ASpawnedMinionIsLookedAtWhereTheOriginalsStands();
}

} // namespace

MP_TEST_MAIN("test_mp_camera", 80)
