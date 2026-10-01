#pragma once

// THE DRAGGABLE HANDLE, arrows.ent, and the flame or reflector that follows it: the
// program's B4, decoded from ETHCallback_arrows (listing L67305), doFollow (L67201)
// and ETHCallback_hand_waypoint0 (game/data/handle.json holds every number and its
// citation; planning step 139).
//
// - A touch that goes down on the handle and stays down drags it, one for one with
//   the finger on the screen, at most scale(64) a frame. While it is held the handle
//   is `moving` - even a frame the finger stands still - and drawn whole; at rest it
//   is drawn at 0.3. It pulses (1.1, 1) <-> (1, 1.1) of its size, 300 ms a leg, and
//   the rect a touch is tested against pulses with it.
// - A fire_agent or reflect_agent whose node carries `follow` = the handle's name
//   eases after it: followUp's PositionInterpolator, smoothEnd over 600 ms, aimed
//   again from where it has got to every 100 ms while the handle is elsewhere
//   (Effects::FollowUp, which ghost_appear_effect's chase already is). The fire
//   agent moves after its flame has burned for the frame, the reflector before it
//   turns a shot. Every one of the six starts on its handle's point, so a handle
//   never dragged moves nothing.
// - While a handle is moving the camera's finger does nothing (shouldLockCamera:
//   Camera::Feel::locked), so a drag on the handle pans no view and holds none.
// - 2-20's hand_waypoint0 teaches the gesture: a repeating round from the handle,
//   fading in, pressing, dragging right and fading out, until the handle is first
//   held - then it adds smoke_vanish above itself and is deleted (Lesson).
//
// WHAT IS NOT BUILT: the original reads two touches (t 0 and 1); the port has one
// pointer. A static follower's callback runs only while its bucket is on the view
// (00_order 8.12c); the port moves it every tick. The handle is read only in play:
// under a popup or the pause, after the door or a death, it is at rest.

#include "sim/Effects.hpp"
#include "sim/Hint.hpp"
#include "sim/Art.hpp"
#include "sim/Tscn.hpp"

#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace MagicPortals::Handle {

// game/data/handle.json.
struct Rules {
    std::string entity;              // the callback's entity, less its .ent: 'arrows'
    glm::dvec2 sizeUnits{0.0};       // GetSize() at scale 1: arrows.png's 64 x 64
    double maxFrameMoveUnits = 0.0;  // capVector(GetTouchMove, scale(64))
    double idleAlpha = 0.0;          // SetAlpha when not moving: 0.3
    double movingAlpha = 0.0;        // and while it is: 1
    Art::Pulse bounce;               // bounce(this, (1.1, 1), (1, 1.1), 300)

    std::string followDatum;             // doFollow's GetString('follow')
    std::vector<std::string> followers;  // the callbacks that call doFollow, less .ent
    Effects::FollowUpRule follow;        // followUp(this, dest, 600, 100, true)

    // 2-20's lesson: ETHCallback_hand_waypoint0.
    std::string lessonEntity;               // 'hand_waypoint0'
    std::string lessonHandleName;           // what it seeks by name: 'arrows.ent'
    std::vector<Hint::Waypoint> lessonWaypoints;
};

// From handle.json, strictly. False, with `error`, for a missing block or number,
// a size, move or stride not above zero, an alpha outside 0 to 1, or no caller.
bool LoadRules(const std::string& path, Rules& out, std::string& error);

// One arrows handle the level places.
struct Handle {
    std::string node;         // the level's node
    std::string entityName;   // its entity as the level names it ('arrows.ent'), which doFollow seeks
    glm::dvec2 startPx{0.0};  // where the level put it
    glm::dvec2 atPx{0.0};     // where it is now
    // 'onHitPos': where it stood when a touch last went down on it; never set is
    // (0, 0), as GetVector2 of a missing name reads.
    glm::dvec2 onHitPx{0.0};
    bool moving = false;      // 'moving', as this tick's callback left it
    // bounce()'s 'blinkElapsedTime', and whether it has been called yet: before its
    // first call the entity is at its own scale.
    double bounceMs = 0.0;
    bool bounced = false;
};

// A node that calls doFollow and names a handle in `follow`.
struct Follower {
    std::string node;
    std::string follows;      // its `follow` datum
    int handle = -1;          // in State::handles: what SeekEntity finds by that name; -1 none
    glm::dvec2 startPx{0.0};  // where the level put it
    glm::dvec2 atPx{0.0};     // where it is now
    Effects::FollowUp follow; // followUp's interpolator and switchTime
    // What it moves, bound by Game::Start: its place in fire.agents, or in
    // portals.reflectors; -1 where it is not one.
    int fireAgent = -1;
    int reflector = -1;
};

// The tick's touch as ETHCallback_arrows reads it.
struct Touch {
    bool hit = false;           // GSKS_HIT: it went down this tick
    bool down = false;          // GSKS_DOWN: held, and not the tick it went down
    glm::dvec2 atPx{0.0};       // GetTouchPos + GetCameraPos: its point in the level this tick
    glm::dvec2 hitPx{0.0};      // getRelativeHitPos: the point of the level it went down on
    glm::dvec2 moveUnits{0.0};  // GetTouchMove: its move since the last tick, on a DOWN tick
};

// Whether `atPx` lies in the rect of `sizeUnits` centred on `centrePx`, both edges
// in (isPointInRect with V2_HALF).
bool InRect(const glm::dvec2& atPx, const glm::dvec2& centrePx, const glm::dvec2& sizeUnits);

// capVector: `v`, or `v` shortened to `length` when it is longer.
glm::dvec2 Cap(const glm::dvec2& v, double length);

// 2-20's hand, hand_waypoint0's callback.
struct Lesson : Hint::Round {
    bool placed = false;
    std::string node;
    glm::dvec2 originalPx{0.0}; // 'originalPos': where the level put it
    bool gone = false;          // DeleteEntity: the handle was held
    bool vanishedNow = false;   // on the tick it went: smoke_vanish is added
    bool ticked = false;        // its callback has run: its SetAlpha is the picture's

    // One call, `frameMs` into it, with whether the handle it seeks is moving.
    void Tick(const Rules& rules, bool handleMoving, double frameMs);
    glm::dvec2 AtPx(const Rules& rules) const { return originalPx + OffsetUnits(rules.lessonWaypoints); }
    double Alpha(const Rules& rules) const { return Hint::Round::Alpha(rules.lessonWaypoints); }
};

struct State {
    Rules rules;
    std::vector<Handle> handles;
    std::vector<Follower> followers;
    int drags = 0; // DOWN ticks that moved a handle: for the record

    // ETHCallback_arrows for every handle, with this tick's touch: 'moving' from
    // nothing, the hit remembered, the drag, and the pulse's clock. `frameMs` is
    // g_timeManager's frame time.
    void Drag(const Touch& touch, double frameMs);
    // A tick no touch is read on (the door's beat, a death's; the pause, with no
    // time): every handle at rest, as a callback with no touch leaves it, and the
    // pulse's clock `frameMs` on.
    void Rest(double frameMs);
    // shouldLockCamera: some handle is moving.
    bool AnyMoving() const;
    // doFollow for every follower, `frameMs` into the frame.
    void Follow(double frameMs);
    // The handle's size as its callback tests a touch against it: the picture's,
    // at the scale the last bounce() call left.
    glm::dvec2 SizeUnits(const Handle& handle) const;
    // Its alpha and its scale as drawn this tick.
    double Alpha(const Handle& handle) const { return handle.moving ? rules.movingAlpha : rules.idleAlpha; }
    glm::dvec2 Scale(const Handle& handle) const;

    const Handle* FindHandle(const std::string& node) const;
    const Follower* FindFollower(const std::string& node) const;
};

// The level's handles - every node whose entity, less its .ent, is rules.entity -
// and its followers - every node of a follower callback's entity whose `follow`
// datum is not empty - each follower bound to the handle its name finds. False,
// with `error`, for either with no position.
bool Find(const Tscn::Scene& scene, const Rules& rules, State& out, std::string& error);

} // namespace MagicPortals::Handle
