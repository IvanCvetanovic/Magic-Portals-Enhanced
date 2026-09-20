#pragma once

// A level's buttons and the doors they open: behaviours.gd's Switch (:179-206)
// and LevelRuntime._wire_channels (level_runtime.gd:184-200).
//
// A button is pressed while any dynamic body is on its trigger. It keeps a count,
// and it is not a latch. When it changes it sets every door on its channel,
// because button `idx` and door_lift `switchIdx` are one numbering. As through the
// remake's EventBus, the last change wins. So of two buttons on one channel, the
// door follows whichever changed last rather than whichever is held. level30 has
// one button per channel.
//
// AND A PRESSED BUTTON SINKS (movers.json `button`, decoded in D10). It slides
// `sink_units` down over its node's `stride` milliseconds, eased by sin(PIb * x),
// and back up the same way when the last body leaves. Each slide starts from its
// STORED endpoint, never from where the slide is, so a reversal inside 300 ms
// snaps (D10 section 4.1).
//
// The SENSOR moves with the picture, because the original has one point and not
// two: ETHCallback_button calls SetPositionXY, which is ETHPhysicsEntityController
// ::SetPosXY -> b2Body::SetTransform (ETHEntity.cpp:370-375;
// ETHPhysicsEntityController.cpp:98-107), and the draw reads GetPosition at draw
// time (ETHRenderEntity.cpp:106). That is the owner's standing ruling R21. So the
// sink moves this Button's trigger box and its built entity's transform, and the
// layer's picture follows the transform as any body's picture does: it needs no
// branch of its own.

#include "sim/LevelBuilder.hpp"
#include "sim/Mover.hpp"
#include "sim/Roles.hpp"
#include "sim/Trigger.hpp"
#include "sim/Tscn.hpp"

#include <string>
#include <vector>

#include <entt/entt.hpp>

namespace MagicPortals::Puzzle {

// The port's button numbers, game/data/movers.json `button`. Decoded, not guessed.
struct Rules {
    double sinkPx = 0.0;  // scale(6), in the level's units, down
    double easeRad = 0.0; // PIb: smoothEnd is sin(bias * this)
};

bool LoadRules(const std::string& path, Rules& out, std::string& error);

struct Button {
    std::string name;
    int channel = -1;
    Trigger::Box box; // where it presses from THIS tick: it sinks with the picture
    bool pressed = false;

    // The sink. `restCentre` and `restPosition` are where the level put it - the
    // script's originalPos - and the beholder's rebase moves both (Rebase).
    entt::entity entity = entt::null; // its built sensor and picture, or null
    glm::vec2 restCentre{0.0f};
    glm::vec3 restPosition{0.0f};
    double sinkPx = 0.0;    // movers.json button.sink_units
    double easeRad = 0.0;   // movers.json button.ease_rad
    double strideMs = 0.0;  // the node's `stride`, which ETHCallback_button reads
    double slideFromPx = 0.0; // the interpolator's m_a and m_b, as a depth below rest
    double slideToPx = 0.0;
    double slideMs = 0.0;   // its m_elapsedTime
    double sunkPx = 0.0;    // how far below rest it is drawn this tick

    // PositionInterpolator::getCurrentPos for this slide: m_b once the clock is
    // past the stride, and a + (b - a) * sin(PIb * clamp(t / stride, 0, 1)) before.
    double DepthPx() const;

    // computeButtonInterpolator again, at a new place: the two stored points move,
    // the clock stops at rest, and the count is zeroed. The beholder's death calls
    // it (ETHCallback_beholder #1302-1315) after moving the button to button_dest.
    void Rebase(entt::registry& registry, const glm::vec2& centre);
};

struct SwitchedDoor {
    std::string name;
    int channel = -1;
    entt::entity entity = entt::null;
    Mover::Door motion;
};

struct Channels {
    std::vector<Button> buttons;
    std::vector<SwitchedDoor> doors;

    // One tick, before the physics step. Every button counts what is on it, a
    // button that changed sets its channel's doors, and every door travels.
    void Tick(entt::registry& registry, float dt);

    // Whether any button on the channel is pressed.
    bool Pressed(int channel) const;
    const Button* FindButton(const std::string& name) const;
    const SwitchedDoor* FindDoor(const std::string& name) const;
};

// Every switch and switched_door in a built level. False, with `error`, when a
// button has no idx or no stride, a door has no switchIdx or stride, or a door was
// not built. A button's own body may be missing without refusing the level: then
// only its trigger sinks, and there is no picture to follow it.
bool Wire(const Tscn::Scene& scene, const Roles::Table& roles, const LevelBuilder::Built& built, const Rules& rules,
          Channels& out, std::string& error);

} // namespace MagicPortals::Puzzle
