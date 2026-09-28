#pragma once

// The torches of chapter 4's dark levels, and the wall of light one drops.
//
// entity_roles.json files light_off / light_off.ent under the role `torch` and
// says it is "what makes the dark levels navigable". That is right about the
// importance and wrong about the shape: what makes them navigable is not the
// light, it is that lighting a torch TAKES A WALL AWAY.
//
// IT IS A TOGGLE, NOT AN UNLOCK. Shoot the unlit torch and it lights, spending
// the shot, and the level's light wall begins to go. Shoot the orbiting flame
// that appears in its place and the torch goes back out and the WALL COMES BACK.
// A port that built only the first half would play all ten levels correctly and
// still be wrong about the mechanism, which is why both halves are here.
//
// WHAT WORKS A TORCH. hasProjectileAround accepts an entity named projectile.ent
// or - the call site passes the flag set - fireball.ent, within scale(24). So the
// player's portal shot works one, and so does a FIRE DIAMOND'S FIREBALL. That is
// the third place step 27's mechanism turns out to be load-bearing, after the
// ghost being burnable and the diamond restock.
//
// THE SHOT IS SPENT EITHER WAY. Lighting a torch calls killProjectile - the same
// bare DeleteEntity the fire diamond's conversion uses - so the shot that worked
// the switch never goes on to open a portal.
//
// THE WALL TAKES 1500 ms TO GO, and that is load-bearing rather than a flourish.
// destroy() only sets a flag; the wall's own callback counts to 1500 ms before it
// deletes itself. addFireSignalIfNecessary runs immediately after the destroy and
// only spawns a signal while a light_wall.ent can still be FOUND - which it can,
// because the wall is flagged and not yet gone. Were the wall taken away on the
// frame it was flagged, no signal could ever appear and the toggle would collapse
// into a one-way unlock. Every level places one torch and at most one wall, so
// there is exactly one signal.
//
// THIS MODULE OWNS NO BODIES AND NEVER TOUCHES THE REGISTRY. The light wall
// carries metadata/breakable, so it is already a Demolish::Breakable with a body
// and a box that Demolish walks every frame. Tick REPORTS that the wall should go
// or come back, and Game funnels it through Demolish - the same rule that makes a
// struck minion go through Minions::Take rather than being destroyed behind the
// list that is still walking it.
//
// WHAT IS SEEN is the layer's, read from this state: the entities lighting a
// torch, putting it out and the wall going and coming back add (effects.json
// _torches, the visuals plan's 11.2), the lit torch's light and the ambient
// (Lighting), and the earthquake when the wall goes (earthquake.json, from
// `wallsGone`). Not built, and recorded in data/torch.json: the sounds but the
// wall's demolition, the unlit torch's spin, and the background lighten/darken.

#include "sim/Roles.hpp"
#include "sim/Tscn.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace MagicPortals::Torch {

// The port's torch.json.
struct Rules {
    std::vector<std::string> torchNames; // light_off.ent and light_off
    double reachPx = 0.0;                // scale(24), and the signal's too

    std::string wallName; // light_wall.ent
    double wallFadeMs = 0.0; // 1500: how long a flagged wall stands before it goes

    double orbitPx = 0.0;    // scale(18): how far the signal circles from its torch
    double spinDegPerSec = 0.0; // 300
};

bool LoadRules(const std::string& path, Rules& out, std::string& error);

struct Light {
    std::string name;      // the node's name
    glm::dvec2 atPx{0.0};  // where it stands, and where a signal circles
    bool lit = false;      // shot, and gone as an entity
    bool switched = false; // put back out by a signal: the original's `switch` = 1
};

// The one wall a level has, if it has one. Four of the ten torch levels have none.
struct Wall {
    std::string name;    // the node's name, which is also how Game rebuilds it
    bool present = false;
    bool going = false;  // flagged by a torch, still standing
    double goingMs = 0.0;
    bool gone = false;
};

// The orbiting flame a lit torch leaves behind, and the way back. It is added
// on the torch's point and circles it clockwise on the screen from above,
// scale(18) out (ETHCallback_fire_signal, Tick).
struct Signal {
    bool present = false;
    glm::dvec2 originPx{0.0}; // the torch it came from
    glm::dvec2 atPx{0.0};     // where it is now, on its circle
    double angleDeg = 0.0;
    std::string fromTorch;
};

struct State {
    Rules rules;
    std::vector<Light> lights;
    Wall wall;
    Signal signal;

    int lit = 0;      // torches lit, over the level's life
    int putOut = 0;   // and put back out by a signal
    int wallsGone = 0;
    int wallsBack = 0;

    // WHERE THE SHOT A SWITCH SPENT WAS, on the tick it was spent and on no other
    // (Tick empties it first): the player's flight or a fireball, at the point it
    // was handed in at. Write-only, as Fire::State::blastsPx is - nothing in the
    // simulation reads it. The layer adds portal_death.ent there, which both
    // ETHCallback_light_off (#125-139) and ETHCallback_fire_signal (#228-242) add at
    // the projectile's GetPosition() once killProjectile has taken it.
    std::optional<glm::dvec2> spentAtPx;

    // What a tick asks Game to do on its behalf. The shot indices refer to what
    // was passed in, because this module cannot see a flight or a fireball.
    struct Turn {
        bool spentFlight = false;   // the player's shot worked a switch and is gone
        int spentFireball = -1;     // or this fireball did, by index
        bool takeWallAway = false;  // its 1500 ms are up: Demolish should break it
        bool putWallBack = false;   // a signal was shot: Game should rebuild it
    };

    // One tick, after the physics step. `flightPx` is where the player's shot is,
    // when one is flying; `fireballPx` is every fireball, in the order Game holds
    // them, so an index can be handed back.
    //
    // At most ONE switch is worked per tick: the original's poll takes the first
    // projectile it finds in the bucket and spends it, and a single shot cannot
    // light two torches.
    Turn Tick(const std::optional<glm::dvec2>& flightPx, const std::vector<glm::dvec2>& fireballPx, float dt);

    const Light* Find(const std::string& name) const;

    // Torches still unlit, which is what a level has left to switch.
    std::size_t Unlit() const;
};

// A level's torches and its one light wall. A level with neither gets an empty
// State, which is 118 of the 128. False, with `error`, for a torch or a wall
// carrying no position.
bool Find(const Tscn::Scene& scene, const Roles::Table& roles, const Rules& rules, State& out, std::string& error);

} // namespace MagicPortals::Torch
