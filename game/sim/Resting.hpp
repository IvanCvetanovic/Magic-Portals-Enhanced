#pragma once

// The bodies a level starts with at rest, as the original shows them (planning step 152's amendment, R3b fix 1).
//
// A crate or a stone the original places is simply at rest by the time its first frame is on screen: the library
// stills of 3-12 at 2 and 8 seconds (out/parity/original/levels/3-12_t2.0.png, _t8.0.png) show the same stone
// cradled between the small crate's corner and the column's, the crate on its ledge, and nothing else. The port's
// solver cannot hold that pile: the stone (1.131) rests on two corners and presses the crate (0.144) sideways every
// tick, and the engine's eight iterations never settle it, so from tick 4 the crate creeps off its ledge and the
// stone follows (measured: even at equal weights, or at a fifth of the world's gravity, the pair still drifts).
// Which of the original's piles those are is data (game/data/resting.json), named from the stills: a body listed
// there is put to sleep where the level places it, and wakes by the engine's own rules - a body it touches that
// moves, or the port moving it (a portal's exit) - so every other thing about it is the solver's.

#include <string>
#include <unordered_map>
#include <vector>

#include <entt/entt.hpp>

namespace MagicPortals::Resting {

struct Rules {
    // By the level's file name without its extension ("level11b"): the nodes that start asleep.
    std::unordered_map<std::string, std::vector<std::string>> byLevel;
    // How near a sleeper must lie to a body the port is about to move away for it to wake (WakeAround): resting.json's
    // wake_reach_px. Game::Start puts the rules in the registry's context, where WakeAround reads them; without them it wakes none.
    double wakeReachPx = 0.0;
};

bool LoadRules(const std::string& path, Rules& out, std::string& error);

// Puts the level's listed bodies to sleep where they stand, from the nodes the builder made (node name to entity).
// False, with `error`, when a listed node was not built or has no rigid body: a typo must not leave a pile awake.
bool Apply(entt::registry& registry, const Rules& rules, const std::string& level,
           const std::unordered_map<std::string, entt::entity>& built, std::string& error);

// Wakes every sleeping body that touches `body` (within the rules' wake_reach_px of it), where it stands now: what the port calls before it
// moves a body somewhere else (a portal's entry), so what lay on it or against it is not left asleep in the air. It is Box2D 2.2.1's own rule,
// which the engine's does not carry: the original wakes both bodies of a contact when it begins and when it ends (b2Contact::Update wakes both when
// a pair's touching changes, b2Contact.cpp:210-214; b2Contact::Destroy wakes both of a contact that had manifold points, :105-109), so a stone
// asleep on a crate that a portal takes away is woken as the contact goes, and rolls. The engine wakes a sleeper only by a contact beginning
// or a stirring neighbour, so without this the stone would hang. The level needs it: 3-12's stone is held inside a permanent antiportal's
// field by the small crate, and taking the crate away is the only way to free it. Returns how many it woke.
int WakeAround(entt::registry& registry, entt::entity body);

} // namespace MagicPortals::Resting
