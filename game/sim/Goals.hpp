#pragma once

// A level's crystals and its exit: LevelRuntime's _on_trigger_entered, _collect
// and exit_is_open (level_runtime.gd:270-304), and what main.gd does with an exit
// (:161-165).
//
// - Only the player triggers them. Crates get pushed through crystals, and in the
//   Godot project they must neither collect them nor finish the level.
// - Both fire on entry, as an Area2D's body_entered does, not while the player is
//   inside. A crystal is collected once, the tick the player first overlaps it,
//   and the exit reports every time the player comes into it.
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

namespace MagicPortals::Goals {

// The "level" object of Enhanced's portals.json. Its one switch is marked
// UNVERIFIED there, not _guess: nobody knows whether the original gates its exit
// on the crystals at all. So it is carried as data, and the tests run it both
// ways. A missing key is an error, where the Godot project falls back to false.
struct Rules {
    bool exitRequiresAllCrystals = false;
};

bool LoadRules(const std::string& path, Rules& out, std::string& error);

struct Crystal {
    std::string name;
    Trigger::Box box;
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
    // the tick the last crystal goes finds the exit open.
    void Tick(entt::registry& registry, entt::entity player, double dt);

    const Crystal* FindCrystal(const std::string& name) const;
};

// Every collectible and the exit_door of a level. False, with `error`, when there
// is not exactly one exit.
bool Find(const Tscn::Scene& scene, const Roles::Table& roles, const Rules& rules, State& out, std::string& error);

} // namespace MagicPortals::Goals
