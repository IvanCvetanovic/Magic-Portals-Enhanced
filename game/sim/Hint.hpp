#pragma once

// 1-21's lesson in the camera drag (game/data/camera.json `hint`, decoded; planning
// step 151): what ETHCallback_hand_waypoint1 and ETHCallback_up_down_arrow do to the
// two pictures the level places, and the line of text the hand's first call draws.
//
// - The hand runs a repeating WaypointManager of four waypoints - a step right, then
//   down the arrow, then back, 3300 ms a round - from where the level put it, at
//   alpha (1 - life / 20000) x 2, clamped: whole for ten seconds, gone by twenty,
//   and deleted past 20000 ms.
// - The arrow fades from 0.6 the same way, 0.3 x 2, and pulses between (1.1, 1)
//   and (1, 1.1) of its size, 300 ms a leg (bounce, as Art::Pulse); deleted past
//   20000 ms too.
// - "move the camera around" is drawn on the SCREEN - scale(50) from the left and
//   scale(0.8) x 40 above the bottom - in the caption's font at 0.8 of its scale,
//   fading to nothing over 10000 ms as ETHTextDrawer fades.
//
// Each is advanced by the frame's time on the level's own ticks, as the port runs
// every placed picture's callback (motions.json `_placed_notes`). Pure: no registry,
// no renderer; the layer draws what this says.

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "core/Json.hpp"

#include "sim/Art.hpp"
#include "sim/Ease.hpp"

namespace MagicPortals::Hint {

struct Waypoint {
    glm::dvec2 atUnits{0.0}; // scale(vector2): from the hand's first place, in the level's units
    double ms = 0.0;         // how long the move to the next one takes
    Ease::Filter filter = Ease::Filter::Linear;
    // Its FloatColor's alpha, which getCurrentPoint mixes into the next one's as it
    // mixes the point. 1-21's four are all FloatColor(-1), alpha 1; 2-20's handle
    // lesson (handle.json `lesson`) fades in and out on its first and last.
    double alpha = 1.0;
};

// A list of waypoints as a callback adds them, read strictly from a JSON array:
// each needs at_units (two numbers), ms (not below 0) and a filter Ease names, and
// `alpha` (0 to 1) where `withAlpha` - 1 otherwise. False with `error` naming `where`.
bool ParseWaypoints(const Supersonic::Json::Value& list, bool withAlpha, const std::string& where,
                    std::vector<Waypoint>& out, std::string& error);

struct Rules {
    std::string handEntity;  // without .ent: its callback is ETHCallback_ + that name
    std::string arrowEntity;
    std::string text;
    std::string font;
    double unitsPerFontPx = 0.0;
    double leftUnits = 0.0;       // from the view's left edge
    double aboveBottomUnits = 0.0; // the text's top, above the view's bottom edge
    double fadeMs = 0.0;
    double lifeMs = 0.0;          // both pictures are deleted once their life passes this
    double handAlphaGain = 0.0;
    std::vector<Waypoint> handWaypoints;
    double arrowAlpha = 0.0;
    double arrowAlphaGain = 0.0;
    Art::Pulse arrowBounce;
};

// From camera.json's hint block, strictly.
bool LoadRules(const std::string& path, Rules& out, std::string& error);

// A repeating WaypointManager (ETHFramework/WaypointManager.angelscript
// 40760..42634): a FrameTimer that steps at most ONE waypoint a call, its first call
// starting again at waypoint 0 with no time, and back to the first after the last.
// 1-21's hand runs one (Hand, below) and so does 2-20's handle lesson (sim/Handle).
struct Round {
    int first = 0;
    int last = 0;
    int frame = 0;
    double timeMs = 0.0;
    double strideMs = 0.0;

    // WaypointManager::update, `frameMs` into the call.
    void Tick(const std::vector<Waypoint>& points, double frameMs);
    // getCurrentPoint's point: this waypoint mixed into the next by its filter.
    glm::dvec2 OffsetUnits(const std::vector<Waypoint>& points) const;
    // And its colour's alpha, mixed the same way.
    double Alpha(const std::vector<Waypoint>& points) const;
};

// The hand's callback: its life, and its repeating WaypointManager.
struct Hand : Round {
    double lifeMs = 0.0;
    bool gone = false; // DeleteEntity: past the rules' life

    // One call, `frameMs` into it.
    void Tick(const Rules& rules, double frameMs);
    // SetPositionXY(originalPos + this): getCurrentPoint's point.
    glm::dvec2 OffsetUnits(const Rules& rules) const;
    // SetAlpha's.
    double Alpha(const Rules& rules) const;
};

// The arrow's callback: its life, which bounce's own clock keeps pace with.
struct Arrow {
    double lifeMs = 0.0;
    bool gone = false;

    void Tick(const Rules& rules, double frameMs);
    double Alpha(const Rules& rules) const;
    // bounce()'s SetScale, over the picture's own size.
    glm::dvec2 Scale(const Rules& rules) const;
};

// ETHTextDrawer's alpha `ageMs` after the text was added: 1 - age / fade, clamped,
// as the byte its colour takes; 0 from the fade's end, when it is dropped.
double TextAlpha(const Rules& rules, double ageMs);

// Where the text's box starts on a view `viewUnits` in size.
glm::dvec2 TextTopLeft(const Rules& rules, const glm::dvec2& viewUnits);

} // namespace MagicPortals::Hint
