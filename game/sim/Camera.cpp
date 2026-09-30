#include "sim/Camera.hpp"

#include "core/Json.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace MagicPortals::Camera {

namespace {

bool ReadRoot(const std::string& path, Supersonic::Json::Value& root, std::string& error) {
    namespace Json = Supersonic::Json;
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = path + ": cannot open";
        return false;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string text = buffer.str();
    Json::Parser parser(text);
    if (!parser.Parse(root)) {
        error = path + ": " + parser.Error();
        return false;
    }
    return true;
}

} // namespace

bool LoadRules(const std::string& path, Rules& out, std::string& error) {
    namespace Json = Supersonic::Json;
    Json::Value root;
    if (!ReadRoot(path, root, error)) return false;
    if (!root.IsObject() || !root.Has("camera") || !root["camera"].IsObject()) {
        error = path + ": no \"camera\" object";
        return false;
    }
    const Json::Value& camera = root["camera"];
    if (!camera.Has("follow_lag_s") || !camera["follow_lag_s"].IsNumber()) {
        error = path + ": camera.follow_lag_s is missing or not a number";
        return false;
    }
    out.followLagS = camera["follow_lag_s"].AsNumber();
    return true;
}

bool LoadDrag(const std::string& path, Rules& out, std::string& error) {
    namespace Json = Supersonic::Json;
    Json::Value root;
    if (!ReadRoot(path, root, error)) return false;
    if (!root.IsObject() || !root.Has("drag") || !root["drag"].IsObject()) {
        error = path + ": no \"drag\" object";
        return false;
    }
    const Json::Value& drag = root["drag"];
    if (!drag.Has("drag_pan_enabled") || !drag["drag_pan_enabled"].IsBool()) {
        error = path + ": drag.drag_pan_enabled is missing or not true or false";
        return false;
    }
    Drag read;
    read.enabled = drag["drag_pan_enabled"].AsBool();
    double maxFramePx = 0.0;
    double pxPerUnit = 0.0;
    const struct {
        const char* key;
        double* to;
    } numbers[] = {{"pan_factor", &read.panFactor},
                   {"max_frame_move_screen_px", &maxFramePx},
                   {"screen_px_per_unit", &pxPerUnit},
                   {"release_move_units", &read.releaseMoveUnits},
                   {"drag_lock_units", &read.dragLockUnits},
                   {"hold_ms", &read.holdMs}};
    for (const auto& number : numbers) {
        if (!drag.Has(number.key) || !drag[number.key].IsNumber()) {
            error = path + ": drag." + number.key + " is missing or not a number";
            return false;
        }
        *number.to = drag[number.key].AsNumber();
    }
    if (read.panFactor <= 0.0 || maxFramePx <= 0.0 || pxPerUnit <= 0.0 || read.releaseMoveUnits < 0.0 ||
        read.holdMs < 0.0) {
        error = path + ": drag's pan_factor, max_frame_move_screen_px and screen_px_per_unit must be above 0, and "
                       "release_move_units and hold_ms not below it";
        return false;
    }
    // The 100 is a bare screen-pixel literal: taken at R12's screen, as
    // earthquake.json takes its own.
    read.maxFrameMoveUnits = maxFramePx / pxPerUnit;
    out.drag = read;
    return true;
}

bool LoadViewHeight(const std::string& path, double& heightPx, std::string& error) {
    namespace Json = Supersonic::Json;
    Json::Value root;
    if (!ReadRoot(path, root, error)) return false;
    if (!root.IsObject() || !root.Has("view") || !root["view"].IsObject() || !root["view"].Has("height_px") ||
        !root["view"]["height_px"].IsNumber() || root["view"]["height_px"].AsNumber() <= 0.0) {
        error = path + ": view.height_px is missing, not a number, or not above 0";
        return false;
    }
    heightPx = root["view"]["height_px"].AsNumber();
    return true;
}

glm::dvec2 Clamp(const glm::dvec2& wantPx, const glm::dvec2& viewPx, const glm::dvec2& boundsPx) {
    glm::dvec2 centre(0.0);
    for (int axis = 0; axis < 2; ++axis) {
        const double half = viewPx[axis] * 0.5;
        centre[axis] = viewPx[axis] >= boundsPx[axis] ? boundsPx[axis] * 0.5
                                                      : std::clamp(wantPx[axis], half, boundsPx[axis] - half);
    }
    return centre;
}

Feel Track(TouchRecord& record, bool pressed, bool held, bool over, const glm::dvec2& atView, bool onPad) {
    Feel feel;
    // Up, coming up, or off the game: MobileInput zeroes the move on RELEASE and UP,
    // and a pointer the game cannot see is no touch.
    if (!over || !held) {
        record = TouchRecord{};
        return feel;
    }
    if (pressed) {
        // GSKS_HIT: the last place is the touch, and the move kept is the frame
        // before's, which was up - so none. TouchInputManager starts again at 0.
        record = TouchRecord{true, atView, atView, 0.0};
        return feel;
    }
    feel.down = true;
    feel.onPad = onPad;
    if (!record.down) {
        // Held, and not seen going down: taken up here, not moving yet.
        record = TouchRecord{true, atView, atView, 0.0};
        return feel;
    }
    // GSKS_DOWN: the move since the last tick, and the manager's farthest as its
    // last update left it; then this tick's update of both.
    feel.moveUnits = atView - record.lastView;
    feel.maxMoveUnits = record.maxMoveUnits;
    record.lastView = atView;
    record.maxMoveUnits = std::max(record.maxMoveUnits, glm::length(atView - record.hitView));
    return feel;
}

void Follow::Start(const Rules& rules, const glm::dvec2& cameraStartPx, const glm::dvec2& viewPx,
                   const glm::dvec2& boundsPx) {
    centrePx = Clamp(cameraStartPx, viewPx, boundsPx);
    // #88-91: cameraHoldElapsedTime = cameraHoldTime. Spent: no opening hold.
    holdElapsedMs = rules.drag.holdMs;
}

void Follow::Tick(const Rules& rules, const glm::dvec2& playerPx, const glm::dvec2& viewPx,
                  const glm::dvec2& boundsPx, double dt) {
    centrePx = Clamp(centrePx, viewPx, boundsPx);
    // main.gd:134-137, per tick. Eased toward where the view may go, so the
    // camera never leaves the level on its way.
    const double share = std::clamp(dt / std::max(rules.followLagS, 0.001), 0.0, 1.0);
    centrePx += (Clamp(playerPx, viewPx, boundsPx) - centrePx) * share;
}

void Follow::Tick(const Rules& rules, const glm::dvec2& playerPx, const glm::dvec2& viewPx,
                  const glm::dvec2& boundsPx, double dt, const Feel& touch, const Unlocks& unlocks) {
    const Drag& drag = rules.drag;
    if (!drag.enabled) {
        // The owner's 2026-09-11 camera: it follows, and nothing else moves it.
        Tick(rules, playerPx, viewPx, boundsPx, dt);
        return;
    }
    // A teleport after the last tick's camera lets the hold go before this tick's.
    if (unlocks.teleported) holdElapsedMs = drag.holdMs;
    // CameraController::update #3-10: the hold's clock first.
    holdElapsedMs += dt * 1000.0;
    // #12-13 dragMoveCamera. The move is (0, 0) on any tick but a DOWN one (#17-36).
    if (touch.down && (touch.moveUnits.x != 0.0 || touch.moveUnits.y != 0.0)) {
        // GetCameraPos() + screen / 2 (#94-119): the view as the last tick left it.
        const glm::dvec2 shownPx = centrePx;
        // #38-39 clampVirtualCamPos.
        centrePx = Clamp(centrePx, viewPx, boundsPx);
        // #41-80: off the pads, and under the frame's limit, the view goes the
        // other way at twice the finger's move - the level follows the finger.
        if (!touch.onPad && glm::length(touch.moveUnits) < drag.maxFrameMoveUnits) {
            centrePx -= touch.moveUnits * drag.panFactor;
        }
        // #82-152: once the touch has strayed past scale(24), a move not fast back
        // toward the player starts the hold again, and one that is lets it go.
        if (touch.maxMoveUnits > drag.releaseMoveUnits) {
            glm::dvec2 toward = shownPx - playerPx;
            const double length = glm::length(toward);
            toward = length > 0.0 ? toward / length : glm::dvec2(0.0);
            const double product = glm::dot(toward, touch.moveUnits);
            holdElapsedMs = product < drag.dragLockUnits ? 0.0 : drag.holdMs;
        }
    }
    // #24-38: the follow runs once the hold is strictly past and no touch is held
    // off the pads (hasAnyTouch); otherwise the view stays where it is, held inside
    // the level (#94-95 clampCameraPos).
    if (holdElapsedMs > drag.holdMs && !(touch.down && !touch.onPad)) {
        Tick(rules, playerPx, viewPx, boundsPx, dt);
    } else {
        centrePx = Clamp(centrePx, viewPx, boundsPx);
    }
    // #85-92: the walk pads let the hold go after this tick's follow.
    if (unlocks.padMoving) holdElapsedMs = drag.holdMs;
}

} // namespace MagicPortals::Camera
