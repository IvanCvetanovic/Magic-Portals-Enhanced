#pragma once

// The camera the port plays with: the Godot project's follow (main.gd:123-137), the
// view held inside the level, and the original's finger on it (step 138).
//
// - It eases toward the player, closing dt / follow_lag_s of the gap each tick
//   (portals.json's camera block, a _guess: the original's is Follower(1000, 100),
//   not built). It eases per tick, where the Godot project eases per frame, so a tap
//   resolved on the tick meets the camera a replay meets. InterpolatedCameraComponent
//   draws it between ticks.
// - The view never shows past (0, 0) or the level_bounds marker. That is
//   clampCameraPos in the original, and the marker's own role note.
// - AND A FINGER MOVES IT, as the original's CameraController::dragMoveCamera does
//   (game/data/camera.json `drag`, decoded): a touch held off the walk pads pans the
//   view by twice its move each tick; one dragged more than scale(24) from where it
//   went down, and not fast back toward the player, holds the view where it was left
//   for cameraHoldTime, 4000 ms, before the follow takes it again; and while any
//   touch is held off the pads the follow waits. Walking on the pads, or anything
//   going through a portal, lets the hold go.
//
// cameraHoldTime IS NOT AN OPENING SHOT. Before step 138 this note read the 4000 ms
// as a hold on camera_start before the camera first went to the player, and gave
// the port a guessed hold_time_s for it, which the layer then zeroed. The
// constructor sets cameraHoldElapsedTime = cameraHoldTime (#88-91) and update adds
// the frame's time before it compares (#3-10, #24-29), so the hold starts spent and
// the original's camera follows from its first frame. The only hold is the one a
// drag starts.
//
// camera.json `drag_pan_enabled` false turns all of the finger off: then Tick is the
// clamp-and-ease every tick, exactly as it was before step 138 (the owner's
// 2026-09-11 ruling, "the camera follows the player, with no dragging").
//
// camera_start is taken as the view's centre, as the Godot project takes it. Ethanon
// places its own camera by the top-left corner, but read either way the view is
// nearly the same once held inside the level: every camera_start sits at or near
// y 0, and near a level's far end.

#include <string>

#include <glm/glm.hpp>

namespace MagicPortals::Camera {

// camera.json `drag`: the original's finger on the camera, decoded.
struct Drag {
    bool enabled = false;          // drag_pan_enabled: false is the owner's 2026-09-11 ruling
    double panFactor = 0.0;        // the view moves this many times the finger's move
    double maxFrameMoveUnits = 0.0; // a frame's move this long or longer pans nothing
    double releaseMoveUnits = 0.0; // the hold is judged only past this far from the touch's start
    double dragLockUnits = 0.0;    // a frame's move toward the player this fast lets the hold go
    double holdMs = 0.0;           // cameraHoldTime
};

struct Rules {
    double followLagS = 0.0; // portals.json camera.follow_lag_s, _guess
    Drag drag;               // camera.json drag, decoded
};

// From portals.json's camera block, strictly: the follow's lag.
bool LoadRules(const std::string& path, Rules& out, std::string& error);

// From camera.json's drag block, strictly, into `out.drag`.
bool LoadDrag(const std::string& path, Rules& out, std::string& error);

// How much of a level the view shows, as its height in the level's pixels: the
// port's own view.json, whose note gives the evidence. The width follows the
// window's shape.
bool LoadViewHeight(const std::string& path, double& heightPx, std::string& error);

// The centre a view of `viewPx` may take in a level running from (0, 0) to
// `boundsPx`: as near `wantPx` as keeps the whole view inside, or the level's
// centre on an axis the view is wider than.
glm::dvec2 Clamp(const glm::dvec2& wantPx, const glm::dvec2& viewPx, const glm::dvec2& boundsPx);

// THE ONE POINTER AS THE CAMERA READS IT, a tick at a time. The original's
// CameraController reads the engine's input raw (ETHInput), whatever else takes the
// touch - a control, a help block, the shot - so the camera keeps its own record of
// it: gs2d MobileInput's last position, and TouchInputManager's start and farthest
// move, which the camera reads as the manager's LAST update left them (the camera
// runs first in the original's frame).
struct TouchRecord {
    bool down = false;
    glm::dvec2 lastView{0.0};  // where it was on the last tick it was held, in view units
    glm::dvec2 hitView{0.0};   // where it went down
    double maxMoveUnits = 0.0; // the farthest it has been from hitView, as of the last tick
};

// What a tick's touch is to the camera.
struct Feel {
    // GSKS_DOWN: held, and not the tick it went down. Only such a tick moves.
    bool down = false;
    glm::dvec2 moveUnits{0.0}; // GetTouchMove: from the last tick's place, on a DOWN tick; else 0
    bool onPad = false;        // its place this tick is on a walk pad (getTargetButton != 0)
    double maxMoveUnits = 0.0; // getMaxMoveLength, as the last tick left it
    // shouldLockCamera: a draggable handle (arrows.ent) is moving this tick, as its
    // callback - always active, so before the camera in the frame - left it
    // (sim/Handle). Then dragMoveCamera does nothing at all (#3-7): no pan, and no
    // hold judged. The follow still waits while the touch is held (hasAnyTouch).
    bool locked = false;
};

// One tick of the record, given the tick's edges, where the touch is (in the view's
// units) and whether that place is on a walk pad. A touch the record did not see go
// down but finds held - one held through a stop - is taken up from here, still.
Feel Track(TouchRecord& record, bool pressed, bool held, bool over, const glm::dvec2& atView, bool onPad);

// What else lets a hold go, besides a drag back toward the player.
struct Unlocks {
    // Something went through a portal since the last tick's camera
    // (teleportToOther #211-217): before this tick's follow, as the original's
    // portal manager runs after the camera in the frame before.
    bool teleported = false;
    // The walk pads are moving the player this tick (ScreenPad::isMoving, which
    // unlocks AFTER this tick's follow, #85-92). Never on a no_gravity level, where
    // the original does not update the pad at all.
    bool padMoving = false;
};

struct Follow {
    glm::dvec2 centrePx{0.0};
    // cameraHoldElapsedTime. A drag away zeroes it, and the follow waits until a
    // tick's time has taken it past drag.holdMs.
    double holdElapsedMs = 0.0;

    // The view at `cameraStartPx`, held inside the level, and the hold spent.
    void Start(const Rules& rules, const glm::dvec2& cameraStartPx, const glm::dvec2& viewPx,
               const glm::dvec2& boundsPx);

    // One tick with no touch. The view is passed each time because a window can
    // change shape under it, and the camera is held inside the level again when it
    // does.
    void Tick(const Rules& rules, const glm::dvec2& playerPx, const glm::dvec2& viewPx, const glm::dvec2& boundsPx,
              double dt);
    // One tick with this tick's touch and unlocks: CameraController::update. With
    // drag.enabled false, or no touch and nothing to let go, it is the tick above.
    void Tick(const Rules& rules, const glm::dvec2& playerPx, const glm::dvec2& viewPx, const glm::dvec2& boundsPx,
              double dt, const Feel& touch, const Unlocks& unlocks);

    // Whether the view is being held where a drag left it: the hold's clock has
    // not yet run to its end (spent, as Start and a let-go leave it, is not held).
    bool Held(const Rules& rules) const { return rules.drag.enabled && holdElapsedMs < rules.drag.holdMs; }
};

} // namespace MagicPortals::Camera
