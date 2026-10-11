#pragma once

// The bodies a level starts with at rest, as the original shows them (planning step 152's amendment, R3b fix 1), and what
// wakes a sleeper when what it rests on goes (planning step 163, R3b-4 and R3c-5).
//
// A crate or a stone the original places is simply at rest by the time its first frame is on screen: the library
// stills of 3-12 at 2 and 8 seconds (out/parity/original/levels/3-12_t2.0.png, _t8.0.png) show the same stone
// cradled between the small crate's corner and the locked door's, the crate on its ledge, and nothing else. The port's
// solver cannot hold that pile: the stone (1.131) rests on two corners and presses the crate (0.144) sideways every
// tick, and the engine's eight iterations never settle it, so from tick 4 the crate creeps off its ledge and the
// stone follows (measured: even at equal weights, or at a fifth of the world's gravity, the pair still drifts).
// Which of the original's piles those are is data (game/data/resting.json), named from the stills: a body listed
// there is put to sleep where the level places it, and wakes by the engine's own rules - a body it touches that
// moves, or the port moving it (a portal's exit) - so every other thing about it is the solver's.
//
// WHAT WAKES A SLEEPER (Watch, WakeAround). Box2D 2.2.1, the original's, wakes a body whose contact goes: DestroyBody
// destroys the body's contacts (b2World.cpp:158-166, b2ContactManager::Destroy, b2ContactManager.cpp:37-98) and
// b2Contact::Destroy wakes both bodies of each that had manifold points (b2Contact.cpp:101-109); a contact whose touching
// changes wakes both (b2Contact::Update, :154-229, the wake at :210-214); a body the script moves is moved by
// b2Body::SetTransform and woken by ETHPhysicsEntityController's SetPosXY, AddToPosXY, AddToAngle and SetAngle
// (ETHPhysicsEntityController.cpp:98-103, 136-140, 148-152, 158-162), and the contacts it had with a sleeper then end or
// are updated by the next step's Collide (b2ContactManager.cpp:105-168), or the island that the awake body seeds takes
// the sleeper in and wakes it (b2World.cpp:431). The engine wakes a sleeper only by a contact that begins with a body that
// is moving, or with a kinematic one (PhysicsSystem.cpp:1080-1121), or when the sleeper is itself written to (:504-524);
// it knows nothing of a body that simply is not there any more. So the port does what Box2D's contact list does: it
// remembers what each sleeper touches (a Supports component, made when it is first seen asleep) and wakes it, before the step
// that would leave it hanging, when one of those bodies is destroyed, stops being solid, or is put somewhere else by the
// port. It is one rule for every way the port removes or moves a support - a wall broken, a crate burnt, a knight killed, a
// door opened, a lift or platform moved - and no site that does so has to remember to call it. The original's three scripted
// movers also wake everything non-static in the buckets round them each frame they move (awakeEntitiesAround, listing L69717,
// called from ETHCallback_lift, _door_lift and _moving_platform); that is a distance and not a contact, and it is not built
// (backlog G3-6).

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace MagicPortals::Resting {

struct Rules {
    // By the level's file name without its extension ("level11b"): the nodes that start asleep.
    std::unordered_map<std::string, std::vector<std::string>> byLevel;
    // And, by the same name, the nodes a listed body rests on (resting.json `rests_on`): what its still shows under it
    // and what the port, which places it at rest where the level puts it, cannot find by looking at the gap between them.
    // Each name is a node of the level; a body that is not here rests on whatever touches it.
    std::unordered_map<std::string, std::unordered_map<std::string, std::vector<std::string>>> restsOn;
    // How near a body must lie to a sleeper to be what it rests on, in the units of Impacts::Between (level units): resting.json
    // `touch_units`. Box2D has a contact between two bodies while their manifold has points, which is within the shapes'
    // skins; the port's engine rests a body overlapping its support by up to its slop. Game::Start puts the rules in the
    // registry's context, where Watch and WakeAround read them; without them neither wakes anything.
    double touchUnits = 0.0;
    // How far a support has to be moved for the sleeper to be left without it, in the engine's metres: resting.json `moved_m`,
    // the distance past which the engine itself takes a sleeping body to have been put somewhere else.
    double movedMetres = 0.0;
};

bool LoadRules(const std::string& path, Rules& out, std::string& error);

struct Support {
    entt::entity entity = entt::null;
    std::string name;                 // its tag, for the log: it is not there to be asked when it is gone
    glm::vec3 position{0.0f};         // where the sleeper last saw it
    double gapUnits = 0.0;            // the gap between the two surfaces when it first looked, in Impacts::Between's units (below 0: overlapping)
};

// What a sleeping body rests on: the bodies whose contacts with it Box2D would hold. Made by Watch the first tick the body is
// seen asleep (or by Apply, with the supports a level names), and taken away when it wakes.
struct Supports {
    std::vector<Support> list;
    bool scanned = false;             // the geometric scan has been done (Apply's named supports are in `list` before it)
};

// Puts the level's listed bodies to sleep where they stand, from the nodes the builder made (node name to entity), each with the
// supports the table names for it. False, with `error`, when a listed node was not built or has no rigid body: a typo must not
// leave a pile awake.
bool Apply(entt::registry& registry, const Rules& rules, const std::string& level,
           const std::unordered_map<std::string, entt::entity>& built, std::string& error);

enum class Cause { Gone, NotSolid, Moved };

// One sleeper Watch looked at for the first time since it fell asleep, for the log: what it rests on, and how far off each was.
struct Looked {
    entt::entity sleeper = entt::null;
    std::string sleeperName;
    std::vector<std::pair<std::string, double>> supports; // name and gap
};

// One sleeper Watch woke, for the log.
struct Woken {
    entt::entity sleeper = entt::null;
    std::string sleeperName;
    std::string supportName;
    Cause cause = Cause::Gone;
};

const char* CauseName(Cause cause);

// Wakes every sleeping body that rests on something that is no longer there as it was: the body destroyed, no longer solid, or
// moved by more than the rules' moved_m since the sleeper last saw it (an awake loose body is the engine's, which wakes what
// rests on it itself when it stirs: its place is only kept up to date). Run where the tick has done everything that removes or
// moves a body and the physics step has not yet run: the end of Game::BeforeStep, which every driver of the tick runs. A sleeper
// woken here is integrated by that step, as Box2D's is by the step after the script that destroyed its support. Without rules in
// the registry's context it does nothing: a rig that never loaded resting.json is the engine's own.
std::vector<Woken> Watch(entt::registry& registry, std::vector<Looked>* looked = nullptr);

// Wakes every sleeping body that rests on `body`, where it stands now: what the port calls before it moves a body somewhere
// else (a portal's entry), so what lay on it or against it is not left asleep in the air. It is the same contact rule as Watch,
// asked for the one body, and covers a body that is awake and so is not one Watch compares. Returns how many it woke.
int WakeAround(entt::registry& registry, entt::entity body);

} // namespace MagicPortals::Resting
