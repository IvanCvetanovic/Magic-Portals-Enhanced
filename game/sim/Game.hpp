#pragma once

// A level as the port plays it, and the tick it plays on. Both live here, so
// the suites and the layer run the same thing.
//
// The tick is in two halves around the physics step:
//   before the step: the buttons and their doors (Puzzle), the moving platforms
//                    and lifts (Mover), what the launchers throw (Launchers),
//                    what the carrancas spit (Turrets), how fast the beholder's
//                    rocks go (Boss), then the player's steering (Player);
//   after the step:  the crystals and the exit, where the step left the player
//                    (Goals), the hazards (Hazards), where the fireballs flew
//                    and whom they burned (Turrets), what the stones touched
//                    (Demolish), the thrown bodies the launchers take back
//                    (Launchers), what the beholder's rocks ran into (Boss),
//                    then the portals (Portals), and last the beholder's own turn
//                    (Boss). The portals come after what judges where the step
//                    left things, because they move what goes through them. The
//                    beholder looks at where they left its rocks.
// A test runs Tick, which is the two halves with the step between them. The
// engine's app steps physics itself, before each layer's OnFixedUpdate. So a
// layer runs AfterStep and then BeforeStep, and the app's step falls between
// one OnFixedUpdate and the next. It is the same sequence, begun half a tick
// later.

#include "sim/Boss.hpp"
#include "sim/Bounce.hpp"
#include "sim/DarkDragon.hpp"
#include "sim/Demolish.hpp"
#include "sim/Diamonds.hpp"
#include "sim/Dragon.hpp"
#include "sim/Fields.hpp"
#include "sim/Fire.hpp"
#include "sim/Ghost.hpp"
#include "sim/Goals.hpp"
#include "sim/GravityWell.hpp"
#include "sim/Handle.hpp"
#include "sim/Hazards.hpp"
#include "sim/Hinge.hpp"
#include "sim/Launchers.hpp"
#include "sim/Keys.hpp"
#include "sim/LevelBuilder.hpp"
#include "sim/Lighting.hpp"
#include "sim/Minions.hpp"
#include "sim/Player.hpp"
#include "sim/Portals.hpp"
#include "sim/Puzzle.hpp"
#include "sim/RedKey.hpp"
#include "sim/Restock.hpp"
#include "sim/Resting.hpp"
#include "sim/ScriptClock.hpp"
#include "sim/Roles.hpp"
#include "sim/Torch.hpp"
#include "sim/Tscn.hpp"
#include "sim/Turrets.hpp"
#include "sim/Zerog.hpp"

#include <filesystem>
#include <string>

#include <entt/entt.hpp>

namespace MagicPortals::Game {

// What a level needs from outside the port: the converted level and
// Enhanced's own data. Read once and shared.
struct Data {
    Tscn::Scene scene;
    // The port's template table (templates.json, sim/Restock): the entities a boss
    // adds that no level places, as nodes of a scene of their own, whose root holds
    // this level's root lighting keys. Built only when a restock asks.
    Tscn::Scene templates;
    Roles::Table roles;
    Player::Tuning tuning;
    // The friction and restitution of every entity's bodies (the port's materials.json), which Start
    // puts in the registry for LevelBuilder and everything that builds a body at run time.
    LevelBuilder::Materials materials;
    Goals::Rules goals;
    Portals::Rules portals;       // Enhanced's portals.json, with the port's transit.json
    Motion::Rules motion;         // and motions.json: linearMotion's own arithmetic
    Mover::Rules movers;          // the port's own movers.json
    Puzzle::Rules puzzle;         // and its `button` and `door` blocks, which sim/Puzzle reads
    Demolish::Rules demolish;     // and demolish.json
    Launchers::Rules launchers;   // and launchers.json
    Shot::Rules shot;             // and shot.json
    Boss::Rules boss;             // and boss.json
    Turrets::Rules turrets;       // and turrets.json
    Fire::Rules fire;             // and fire.json
    Hinge::Rules hinge;           // and hinge.json
    Minions::Rules minions;       // and minions.json
    Keys::Rules keys;             // and keys.json
    RedKey::Rules redKey;         // and its red_key_state block
    Diamonds::Rules diamonds;     // and diamonds.json
    Fields::Rules fields;         // and fields.json
    Ghost::Rules ghost;           // and ghost.json
    Dragon::Rules dragon;         // and dragon.json
    DarkDragon::Rules darkDragon; // and darkdragon.json
    Torch::Rules torch;           // and torch.json
    Lighting::Rules lighting;     // and lighting.json: the ambient lights the script sets (presentation only)
    Zerog::Rules zerog;           // and zerog.json
    Bounce::Rules bounce;         // and bounce.json
    GravityWell::Rules wells;     // and gravitywell.json
    Handle::Rules handle;         // and handle.json: the draggable handle and what follows it
    Hazards::Rules hazards;       // and hazards.json, which is what the role table got wrong
    Resting::Rules resting;       // and resting.json: the piles the original shows at rest, put to sleep at the start
    std::string levelName;        // the level's file name without its extension, which resting.json is keyed by
    std::filesystem::path prisms; // where LevelBuilder writes the platforms' prisms
};

// A converted level (.tscn), Enhanced's data directory with its
// entity_roles.json, player.json and portals.json, and the port's own data with
// its movers.json, demolish.json, launchers.json, shot.json, transit.json and boss.json. False, with
// `error`, when any of them is missing or malformed.
bool LoadData(const std::string& levelPath, const std::string& dataDirectory,
              const std::filesystem::path& prismDirectory, Data& out, std::string& error,
              const std::string& portDataDirectory = MAGICPORTALS_PORT_DATA_DIR);

// The original's world gravity as the registry's PhysicsSettings, in the carried frame of `step`
// (ScriptClock.hpp): the 10 m/s^2 its Game::preLoop sets (player.json gravity.world_m_s2: 500 units a
// second squared of ITS physics) at SimRatio squared - 2000 units a second squared at the decoded 60 Hz.
// A bare-float Step is a tick whose physics is one length with the tick's (SimRatio 1), and gets 500.
// Game::BeforeStep writes it every tick from the tick's own Step; Start writes it once at ratio 1.
void UseWorldGravity(entt::registry& registry, const Player::Tuning& tuning, const ScriptClock::Step& step);

// And none at all, for a level that sets `no_gravity`: the original's
// setGravity(V2_ZERO). Zerog.hpp says what else that flag turns on.
void UseNoGravity(entt::registry& registry);

struct Level {
    LevelBuilder::Built built;
    Puzzle::Channels channels;
    Mover::Movers movers;
    Goals::State goals;
    Hazards::State hazards;
    Demolish::State demolish;
    Launchers::State launchers;
    Portals::State portals;
    Boss::State boss;
    Turrets::State turrets;
    Fire::State fire;
    // The draggable handles (arrows.ent) and the fire agents and reflector that
    // follow them (Handle.hpp): dragged by the layer's touch, followed after the
    // fire has burned (AfterStep), each follower writing its point into fire.agents
    // or portals.reflectors.
    Handle::State handles;
    // Attached when the level is built and then left to the solver: Hinge.hpp
    // says why this one has no tick.
    Hinge::State hinge;
    // Spawned when the level starts, not built with it: Minions.hpp says why the
    // marker is all a level places.
    Minions::State minions;
    // Keys carried by the player or by a minion, and the doors they open.
    Keys::State keys;
    // 4-19's red key state: once its key is taken, the level's light fades for
    // 6000 ms and then the wall to the rocket breaks (RedKey.hpp).
    RedKey::State redKey;
    // Shock diamonds, which only the player may carry and which kill a minion.
    Diamonds::State diamonds;
    // The lethal rings, which swing about the node that placed them.
    Fields::State fields;
    // Chapter 3's boss, which only a barrel's blast hurts: the barrel it puts back
    // itself, set off by the fire diamond's fireballs.
    Ghost::State ghost;
    // Chapter 2's boss, which is the one that cannot be killed: its level is won
    // by reaching the exit while it strafes the player and its claw eats the
    // floor. It owns a camera of its own, because its level is the only one in
    // the game that sets auto_camera - and sets it to the dragon.
    Dragon::State dragon;
    // Chapter 4's boss, and the last level of the game: summoned by the player's
    // own light, hurt only by a barrel bomb its own fireball sets off, and the
    // holder of the only key its level has.
    DarkDragon::State darkDragon;
    // What a boss put back mid-fight (addEntityIfItCantBeFound): the barrel bombs
    // of 3-32 and 4-32, each built at `barrel_spawn` - from 4-32's placed barrel, or
    // from the port's template table for 3-32, which places none - and entered in
    // `built` under a name of its own, and in Fire and the portals as a placed one is.
    Restock::State restock;
    // The sleepers Resting::Watch woke at the end of the last BeforeStep, because what they rested on was destroyed, stopped being
    // solid or was moved (planning step 163): kept for the layer's log line, and cleared by the next BeforeStep.
    std::vector<Resting::Woken> restingWoken;
    // And the sleepers it looked at for the first time (what each rests on and how far off), for the same line.
    std::vector<Resting::Looked> restingLooked;
    // Chapter 4's torches, and the wall of light one of them drops.
    Torch::State torch;
    // The slabs that bob in chapter 4's weightless rooms. Kinematic, so each
    // carries what stands on it - Bounce.hpp says why that is the whole step.
    Bounce::State bounce;
    // Chapter 4's gravity wells: a solid circle that pulls every dynamic body
    // in, and refuses a portal anywhere inside its reach.
    GravityWell::State wells;
    // The level sets `darkest`: its ambient light is DARKEST_AMBIENT_LIGHT,
    // (0.01, 0.01, 0.01), in place of its file's. The layer draws with it
    // (Lighting::Ambient, lighting.json), and that is presentation, off the state
    // hash. Its gameplay consequence is that a minion in such a level is blind
    // (planning step 158): BeforeStep hands Minions `canSeeAnything`, which is the
    // ambient Lighting::Ambient gives - the one function the layer draws with - not
    // being the darkest value, so a lit torch gives the knights their sight and the
    // player back, and a torch put out takes it again.
    bool darkest = false;
    // The level file's own <Ambient> (the root's eth_ambient), which that function starts from.
    glm::dvec3 fileAmbient{1.0};
    // The level sets `no_gravity`: the world's gravity is zero, the walking
    // buttons do nothing, and the recoil of a portal shot is how the player
    // moves. Zerog.hpp holds the decode. Unlike `darkest` this one is ACTED ON,
    // in three places - the gravity at Start, the steering BeforeStep does not
    // do, and the shove Portals::Shoot applies.
    bool noGravity = false;
    entt::entity player = entt::null;
};

// A body the simulation has taken away, dropped from what tracked it: the portals' travellers and the stones that break
// walls. Every module that removes a body of the level's goes through it; the layer's stone crush (below) does too.
void Forget(Level& level, entt::entity body);

// What a rolling stone's crush does to the knights it hit (ETHBeginContactCallback_rolling_stone #131-168: hp 0 and
// destroy(), which the minion's own callback turns into its blood): each body that is a minion of the level is taken
// (Minions::Take) and forgotten. The layer calls it with the minions its impact watch found a stone beginning to touch
// at a hitIntensity above minions.json `crush.hit_above`.
void Crush(entt::registry& registry, Level& level, const std::vector<entt::entity>& bodies);

// A rolling stone's begin against the PLAYER (ETHBeginContactCallback_rolling_stone #107-129, planning step 160): where its
// hitIntensity is above demolish.json `stone.kills_player_above` it sets his hp to 0, which Demolish::State::HitPlayer records
// and AfterStep folds into Hazards. `stone` is the stone's body, which must be one of the level's stones; the beholder's rocks
// are the same callback and Boss::Contacts kills with them by its own reading of it, so a rock of the boss is NOT judged here.
// Returns whether this call killed him.
bool StoneHitsPlayer(Level& level, entt::entity stone, double hitIntensity);

// Puts the level into the registry: the Godot project's gravity, the bodies built with
// the role table, and the player at the level's player_spawn. Then its buttons,
// doors, crystals, exit and portals are found. withStatics false leaves the
// statics out, which is the landing check's mutation. That leaves out the doors
// too, so there is nothing to wire.
bool Start(const Data& data, entt::registry& registry, Level& out, std::string& error, bool withStatics = true);

// What ETHCallback_minion asks the engine each frame (planning step 158): whether a knight can see anything (the
// ambient is not the darkest value), whether the game is finished, and where the player and the placed portals stand.
// BeforeStep hands it to Minions::State::Tick; a suite asks it directly.
Minions::Senses SensesOf(const Data& data, entt::registry& registry, const Level& level);

void BeforeStep(const Data& data, entt::registry& registry, Level& level, float direction,
                const ScriptClock::Step& step);
// Takes the Data, as BeforeStep does. A torch that puts its light wall BACK has
// to rebuild that wall's body, and LevelBuilder::BuildEntity needs the scene to
// do it - the node's body and shape are its children. The alternative was for
// Level to borrow a scene pointer, which is a lifetime hazard introduced for one
// feature; there were three call sites, so the signature moved instead.
void AfterStep(const Data& data, entt::registry& registry, Level& level, const ScriptClock::Step& step);

// One whole tick, for a caller that steps physics itself.
void Tick(const Data& data, entt::registry& registry, Level& level, float direction,
                const ScriptClock::Step& step);

} // namespace MagicPortals::Game
