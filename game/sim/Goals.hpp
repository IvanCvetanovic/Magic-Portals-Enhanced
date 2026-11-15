#pragma once

// A level's crystals and its exit: LevelRuntime's _on_trigger_entered, _collect
// and exit_is_open (level_runtime.gd:270-304), and what main.gd does with an exit
// (:161-165).
//
// - Only the player triggers them. Crates get pushed through crystals, and in the
//   Godot project they must neither collect them nor finish the level.
// - Both fire on entry, as an Area2D's body_entered does, not while the player is
//   inside: the exit reports every time the player comes into it, and a crystal is
//   collected once, the first tick the player is within its reach.
// - A crystal's reach is NOT the body-overlap the Godot project gave it. It is the
//   original's: MainCharacter::crystalPicker takes a crystal when the player's point
//   (his capsule's centre) is strictly within 26 units of either of two points, the
//   crystal's own shifted 14 up and 14 down (Pickup below; crystals.json has the decode,
//   planning step 243). The box on a Crystal is still what the layer draws and what the
//   Godot project's overlap used, and is no longer what takes it. Two things of the
//   original's are not built: the crystal's 1.5 unit bob (the original measures from the
//   bobbed point, the port from the placed one, which the layer's motion never feeds
//   back) and its one pick a frame (two crystals in reach go on the same tick here).
// - An entry completes the level only if the exit is open. It is open unless
//   portals.json's level.exit_requires_all_crystals asks for every crystal first.
//   So with that switch on, a player already standing in the exit when the last
//   crystal goes does not finish. It has to leave and come back in, as in the
//   Godot project.
// - A crystal the level times goes that long after the level starts, as the
//   Godot project's TimedCollectible frees it (behaviours.gd:209-230). Only the inline
//   `crystal` carries a `time`, 3.9 to 25 s in the data; `crystal.ent` never
//   does. A gone crystal cannot be collected, and it still counts against the
//   exit's switch, as in the Godot project, whose crystals_remaining never drops for
//   one. Within a tick a crystal is collected before its time runs down, as
//   Godot runs a frame's physics before its _process.

#include "sim/Roles.hpp"
#include "sim/Trigger.hpp"
#include "sim/Tscn.hpp"

#include <string>
#include <vector>

#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace MagicPortals::Goals {

// What takes a crystal (crystals.json, "pickup"): the player's point must be STRICTLY
// within rangePx of the crystal's point shifted by offsetPx or by minus offsetPx. In the
// port's pixels, y down, the original's own axes. A Pickup that was not loaded has no
// range, so it takes nothing: a suite that builds a State by hand and ticks it with a
// player has to say what a crystal is taken by.
struct Pickup {
    double rangePx = 0.0;
    glm::dvec2 offsetPx{0.0};
};

// crystalPicker's two tests (ins 112-177), apart from any registry so that a case can
// state a boundary in exact doubles: strict, as the listing's CMPf; JNS / TS are.
bool InPickupRange(const Pickup& pickup, const glm::dvec2& playerPx, const glm::dvec2& crystalPx);

// The "level" object of Enhanced's portals.json. Its one switch is marked
// UNVERIFIED there, not _guess: nobody knows whether the original gates its exit
// on the crystals at all. So it is carried as data, and the tests run it both
// ways. A missing key is an error, where the Godot project falls back to false.
struct Rules {
    bool exitRequiresAllCrystals = false;
    Pickup pickup;
};

bool LoadRules(const std::string& path, Rules& out, std::string& error);

// The port's own crystals.json, into `out.pickup` and nothing else, so it is read AFTER
// LoadRules (which starts the Rules afresh). A missing key, a range of nothing, a
// vector that is not two numbers and any negative number are errors, and a refused file
// leaves `out` as it was.
bool LoadPickup(const std::string& path, Rules& out, std::string& error);

struct Crystal {
    std::string name;
    Trigger::Box box;
    // Where the original's crystalPicker measures from: the node's own point, in pixels, y
    // down, as a double (the box's centre has been through a float of metres). A crystal
    // carries no trigger_offset in any level, so it is also the box's centre.
    glm::dvec2 pointPx{0.0};
    bool collected = false;
    bool timed = false;
    double lifeS = 0.0;       // the level's `time`, in seconds (lifeMs / 1000)
    double lifeMs = 0.0;      // the level's `time` itself: the number the node states
    // The dial's elapsedTime (ETHCallback_timer ins 1-13): the script's whole
    // milliseconds, summed as the original's uint is, and tested strictly above `lifeMs`
    // (ins 102-103), so a 10,000 ms crystal is whole through its 625th tick of 16 ms
    // (the sum is exactly 10000) and runs out on the 626th, not on a float's 600th
    // (planning step 147, fix 1).
    double elapsedMs = 0.0;
    bool expired = false;
};

struct State {
    std::vector<Crystal> crystals;
    Trigger::Box exit;
    Rules rules;
    int exitEntries = 0;        // how many times the player has come into the exit
    bool completed = false;     // an entry found the exit open
    bool playerInExit = false;  // as of the last tick, to find the next entry

    int Remaining() const;
    bool ExitOpen() const;

    // One tick, after the physics step, covering what the player has come into,
    // then how long the timed crystals have left. The Godot project does not define the
    // order of two triggers in one frame. Crystals go first here, so an entry on
    // the tick the last crystal goes finds the exit open. A crystal is taken by the
    // player's TransformComponent position alone (Pickup), so a player with no collider
    // takes one; the exit still needs his collider.
    void Tick(entt::registry& registry, entt::entity player, double dt);

    const Crystal* FindCrystal(const std::string& name) const;
};

// Every collectible and the exit_door of a level. False, with `error`, when there
// is not exactly one exit.
bool Find(const Tscn::Scene& scene, const Roles::Table& roles, const Rules& rules, State& out, std::string& error);

} // namespace MagicPortals::Goals
