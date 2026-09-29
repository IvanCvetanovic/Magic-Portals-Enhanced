#pragma once

// Fire agents, burning, and barrel bombs - the three that chapter 2 leaves inert,
// built together because the original writes them as one mechanism: a flag on an
// entity, set by whatever reaches it, read back by that entity's own callback.
//
// Decoded from the original's AngelScript rather than taken from the remake, and
// the two differ in every number that matters. Byte offsets into android_game.bin
// are given so the reading can be checked; no disassembly is reproduced here.
//
// WHAT REACHES WHAT
//
// - A fire agent (role fire_source, fire_agent.png) gathers its own spatial
//   bucket and the four beside it and tests, per entity,
//       squaredDistance(other, self) < size.x * size.x
//   so size.x is a RADIUS and the test is CENTRE TO CENTRE, not shape against
//   shape. GetSize() is the sprite's frame times its scale - the engine hands it
//   to IsPointOnSprite as the sprite's extent (ETHBucketManager.cpp:289) and to
//   ComputeAbsoluteOrigin as its origin (ETHScene.cpp:375), and GetCollisionBox
//   is a separate method - so for fire_agent.png at 32 x 32, SpriteCut 1 x 1 and
//   Scale 1.0 the reach is 32. The remake's hazards.json reads the entity's
//   38 x 38 <Collision> instead and halves it to 24, marked _guess; that box is
//   not what GetSize returns, and the .ent's shape is 0 - no body at all.
//   (ETHCallback_fire_agent, bytes 350254..351485.)
// - It then runs FOUR INDEPENDENT TESTS in this order, not a chain of else:
//   explosive requests a blast, burnable-and-not-yet-burned burns, a character
//   dies and is given flames, and projectile.ent is put out. A minion is burnable
//   AND a character, so it takes the second and the third both.
// - A blast (explode, MiscCallbacks, bytes 342023..342895) is the other shape: an
//   EntityGrabber of radius 80 over the entity's REAL collider, circle against
//   circle or sphere against box, and it skips anything with no physics body at
//   all (EntityGrabber.angelscript, bytes 23924..24990). Its filter is
//   isBreakableOrExplosiveOrBurnable, which does NOT include characters, and no
//   main_char placement carries any of those three flags - so a bomb cannot touch
//   the player, while fire kills them. That asymmetry is the original's, and
//   test_mp_fire pins it in both directions.
// - The blast also checks LINE OF SIGHT: it casts from the blast to each entity
//   and drops it unless the first thing hit is that entity, so a wall shields
//   what stands behind it.
//
// WHAT IS NOT BUILT, SAID HERE RATHER THAN LEFT TO BE NOTICED
//
// - The fourth branch, burnProjectile, does NOT put a flying projectile.ent out:
//   it CONVERTS one. It is killProjectile - a bare DeleteEntity - followed by
//   addFireball along the same direction, so the shot is replaced by a fireball
//   rather than extinguished. (An earlier wording here said "puts out", which read
//   as extinguishes and was wrong.) Its one caller is the fire diamond's
//   turnProjectilesIntoFireBalls, and Diamonds owns that: Game runs it every tick
//   the carrier holds one. What is still not built is a FIRE AGENT reaching a
//   shot - a shot fired THROUGH a flame keeps flying here - and that belongs in
//   Shot's segment test, not in this file.
// - A blast also sets `destroy` on every entity it grabs (explode #111-122). This
//   file does not act on that: it REPORTS what each blast reached with a clear line
//   (State::grabbedNow), and Game hands it to Demolish, the one owner of a
//   breakable's breaking, which takes the breakable walls away on the same tick
//   (planning step 115). What else reads `destroy` in the original - a carranca, a
//   shock agent, a minion, a light wall - is not built; demolish.json says so.
//
// WHAT BURNING DOES, AND TO WHAT
//
// burn() only sets `burned` and plays a sound. The fade to black, and the removal
// that makes burning a PUZZLE rather than a decoration, live in manageBurnable
// (bytes 420664..421145), which has exactly ONE caller: crateCallback. So only
// crates fade over BURN_TIME and are taken away; a shock_agent carries the
// burnable flag too and merely holds `burned` when lit. Building "everything
// burnable dies" would delete level 2-12's shock agent and change its puzzle, so
// fire.json lists the names whose callback is crateCallback and nothing else
// fades.
//
// THE FLAG AND THE POLL ARE DELIBERATELY A TICK APART
//
// Fire and a blast do not detonate a bomb; they set `explode` on it. The bomb's
// own per-frame callback (ETHCallback_barrel_bomb, bytes 339851..340212) reads
// that flag, removes itself, and only then blasts. A chain of bombs therefore
// ripples one tick at a time rather than collapsing into a single frame, and the
// port keeps that split rather than recursing.
//
// AND THE LAVA, WHICH IS A FLAG TOO
//
// ETHBeginContactCallback_lava (MiscCallbacks, listing L68416, bytes 345993..346228)
// is bound to `lava` by name and runs for whatever begins a contact with it: #1-19
// burn() if the other is burnable, then #21-30 SetUInt('explode', 1) on it, whatever
// it is. So a barrel that falls into the lava goes off - its own callback reads the
// flag, as it reads a fireball's - and 4-32's restock can then put it back (planning
// step 116). It kills nothing: no line of it touches a character's hp. The port reads
// a contact as the stones' is read (demolish.json contact): the body's collider
// overlapping the lava's box grown by fire.json `lava.margin_px`.
//
// WHERE IT RUNS, AND WHO CLEARS UP
//
// After the step, with the hazards, and before the portals - what it removes is
// handed back so Game::AfterStep can pass it through Forget, because a crate is
// teleportable and may be in portals.travellers when it burns. Killing the player
// is reported here and folded into Hazards by Game::AfterStep, as the boss's and
// the carrancas' kills are, so there is one death in the port and not three.

#include "sim/LevelBuilder.hpp"
#include "sim/Roles.hpp"
#include "sim/Trigger.hpp"
#include "sim/Tscn.hpp"

#include <string>
#include <vector>

#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace MagicPortals::Fire {

// The port's fire.json.
struct Rules {
    double reachPx = 0.0;   // a fire agent's GetSize().x, centre to centre
    double burnMs = 0.0;    // BURN_TIME: how long a crate burns before it goes
    double blastPx = 0.0;   // explode's EntityGrabber radius, against real colliders
    // The entity names whose callback is crateCallback, and so the only ones that
    // fade and are removed when burned. Everything else flagged burnable just
    // holds the flag.
    std::vector<std::string> fadeNames;
    // The entities ETHBeginContactCallback_lava is bound to (both spellings), and
    // how near a body must come to one's box to touch it (a threshold, _guess).
    std::vector<std::string> lavaEntities;
    double lavaMarginPx = 0.0;
};

bool LoadRules(const std::string& path, Rules& out, std::string& error);

// A fire_source placement. It never moves and has no body, so its position is all
// there is of it.
struct Agent {
    std::string name;
    glm::dvec2 atPx{0.0};
};

// Anything carrying metadata/burnable.
struct Burnable {
    std::string name;
    entt::entity body = entt::null;
    bool fades = false;   // its name is in Rules::fadeNames
    bool burned = false;  // the original's `burned`
    double burningMs = 0.0;
    bool gone = false;
};

// Anything carrying metadata/explosive.
struct Bomb {
    std::string name;
    entt::entity body = entt::null;
    bool requested = false; // the original's `explode` flag, set this tick
    bool blown = false;
};

// A body whose begin-contact callback is the lava's: its box, in world metres.
struct Lava {
    std::string name;
    entt::entity body = entt::null;
    Trigger::Box box;
};

// A body one blast reached with a clear line, and the bomb whose blast it was.
struct Grab {
    std::string by;
    entt::entity body = entt::null;
};

struct State {
    Rules rules;
    std::vector<Agent> agents;
    std::vector<Burnable> burnables;
    std::vector<Bomb> bombs;
    std::vector<Lava> lavas;

    // Reported, not acted on: Game::AfterStep folds this into Hazards, as it does
    // the boss's and the carrancas'.
    bool playerKilled = false;
    std::string killedBy;

    int lit = 0;      // burnables set alight
    int blasts = 0;   // bombs that went off

    // WHERE the bombs went off this tick, cleared and refilled by every Tick.
    // Blast() reaches what it grabs by BODY, and chapter 4's boss has none - it is
    // a sensor the level never places, as the ghost is - so Game tests these
    // against it by hand, exactly as it tests a fireball against the ghost.
    std::vector<glm::dvec2> blastsPx;
    // And WHAT they reached with a clear line, whatever it is, cleared and refilled
    // by every Tick: the original's grabbed set, which Game hands to Demolish.
    std::vector<Grab> grabbedNow;

    // One tick, after the physics step. Returns the bodies it took away, which
    // the caller must pass through Forget: a burnt crate can be a portal
    // traveller, and a blasted one a demolisher's target.
    std::vector<entt::entity> Tick(entt::registry& registry, entt::entity player, float dt);

    const Burnable* FindBurnable(const std::string& name) const;
    const Bomb* FindBomb(const std::string& name) const;

    // A body the level did not place but a script added (sim/Restock), taken on as
    // Find takes a placed one: a bomb when its node is explosive, a burnable when
    // it is burnable, under the added entity's own name.
    void Adopt(const Tscn::Node& node, const std::string& name, entt::entity body);
};

// A level's fire agents, burnables, bombs and lava, each with the body LevelBuilder
// built for it. A flagged entity with no body built is passed over, which is what
// a level started without its statics leaves. False, with `error`, for a turned
// lava, whose box would turn with it.
bool Find(const Tscn::Scene& scene, const Roles::Table& roles, const LevelBuilder::Built& built, const Rules& rules,
          State& out, std::string& error);

} // namespace MagicPortals::Fire
