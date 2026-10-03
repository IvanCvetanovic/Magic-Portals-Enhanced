#pragma once

// Carrancas: the wall-mounted stone gargoyles that spit fireballs along their
// own (dirX, dirY), which Enhanced's role table calls `turret`.
//
// WHY THIS EXISTS. Nineteen of chapter 2's levels carry one, and ten of them -
// level0a through level8a and level24a - carry nothing else the port cannot
// play. So this is the single role that turns the most levels from "starts" into
// "plays", which is what test_mp_start counts.
//
// The original is ETHCallback_carranca.angelscript. Two things live in its
// callback and they are unrelated:
//   - FIRING, which is what this is: a stride clock, and addFireball.
//   - BEING DESTROYED, which spawns carranca_destroy.ent, shakes the screen
//     (startEarthquake) and deletes the entity. That is the burnable side, it
//     belongs with fire_source and explosive, and it is not built here.
//
// What a fireball is, from its own fireball.ent: a dynamic sensor with density 0
// travelling at 130 px/s, with gravityScale set to 0 at spawn, whose body is a
// CIRCLE of radius 9.6 (turrets.json). A sensor is not pushed by what it meets,
// but it still BEGINS A CONTACT with it, and ETHBeginContactCallback_fireball
// (listing L85862) decides what that contact does:
//   - a character, when the fireball's killMainCharacter is clear: nothing (#1-31);
//   - a carranca, or a sensor: nothing (#41-60, #120-121) - but a shock_agent,
//     the one sensor it names, and itself are both destroyed (#61-119);
//   - a character it may kill: flames and hp 0 (#123-145), and on to
//   - EVERYTHING ELSE - a wall, a closed door, a crate, a stone, a barrel, a
//     minion: destroy(thisEntity) (#146-152) with the small explosion's sound
//     (#153-158), THEN burn() the body if it is burnable (#159-173) and explode()
//     it if it is explosive (#174-188).
// So a fireball dies on the first solid thing it touches, and a closed door
// stops a carranca's fire (footage F6 #72, on 2-12). Game::AfterStep plays the
// callback; Shot::SolidBodiesIn is the touch.
//
// THROUGH A PORTAL PAIR (planning step 135, the backlog's G6). fireball.ent
// carries teleportable 1, and PortalManager names fireball.ent in two places of
// its teleport path: mayTeleport (bytes 140557..140728) lets one through only
// while its teleportedOnce is clear, and doesItDestroyPortals (145873..145999) is
// false for it, so teleportToOther (144944..145873) keeps the pair, sets
// hasTeleportedSomething on both ends (#125-150), turns the fireball's 'dir' back
// (#152-176) and sets teleportedOnce (#178-207). So a fireball goes through a
// placed pair ONCE, comes out of the partner flying back the way it came, and
// the pair stays for the next one. Portals::State::CarryFireball is that path;
// Game carries each fireball after the portals' tick. The static portals'
// callback and the one-placement path would carry one too, without the once
// test, and are not built: no level places a fireball source with either.
//
// NOT BUILT, and named here rather than found later:
//   - The picture. fireball.ent has no <Sprite> at all: what is seen is its
//     ParticleSystem and its Light, so there is no image for the layer to draw.
//   (Its speed is BUILT since planning step 149: ETHCallback_fireball moves it by
//   'dir' x its speed, a carranca's 'dir' is its node's (dirX, dirY) as written,
//   1.2 on level0a, and the port flies it at that length x 130 - turrets.json's
//   direction note.)

#include "sim/LevelBuilder.hpp"
#include "sim/Roles.hpp"
#include "sim/Tscn.hpp"

#include <string>
#include <vector>

#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace MagicPortals::Turrets {

// The port's turrets.json. Decoded but for the cull margin.
struct Rules {
    double speedPx = 0.0;  // fireball.ent's own speed, 130
    double radiusPx = 0.0; // its body's circle: (16 + 16) / 4 x its <Scale> 1.2
    // Its share of the grab that finds it in a portal: half its UNSCALED 16 x 16
    // box, 8 (turrets.json portal_grab_px), added to the portal's own 14.
    double portalGrabPx = 0.0;
    double defaultStrideMs = 0.0;
    glm::dvec2 cullMarginPx{0.0}; // _guess
};

bool LoadRules(const std::string& path, Rules& out, std::string& error);

struct Turret {
    std::string name;
    glm::dvec2 atPx{0.0};
    glm::dvec2 directionPx{0.0}; // the node's (dirX, dirY) as written, +y down: NOT a unit vector
    // Its own StaticBody2D, which every fireball passes (isCarranca, #41-50):
    // the one it spits starts inside it. entt::null for a level built without
    // its statics.
    entt::entity body = entt::null;

    double strideMs = 0.0;
    // Seeded from the node's startStride, as carrancaCallback seeds elapsedTime,
    // so the first shot comes stride - startStride ms in rather than at once.
    double elapsedMs = 0.0;
    int fired = 0;
};

struct Fireball {
    std::string name; // "<carranca>#<n>", counting from 1
    glm::dvec2 atPx{0.0};
    glm::dvec2 velocityPx{0.0};

    // addFireball's killMainCharacter, which is a PARAMETER and not a property of
    // the entity: a carranca passes it set, and burnProjectile - the fire
    // diamond's conversion of a portal shot - passes it clear. The contact
    // callback tests it first and bails out on a character when it is clear, so
    // one of these is lethal and the other cannot touch the player, from the same
    // fireball.ent. Diamonds.hpp has the decode.
    bool killsPlayer = true;

    // addFireball's OTHER parameter, fireballScale, which is 1 for a carranca, a
    // converted shot and level31a's dragon and 2 for the dark dragon. #77-83
    // passes it to ScaleParticleSystem(0, fireballScale), so it scales the first
    // particle system of fireball.ent alone, on top of the entity's own <Scale>;
    // it moves nothing this file simulates. It rides here because the call is
    // over long before the fireball is - effects.json's fireball row and
    // darkdragon.json's fight.fireball_particle_scale carry the decode.
    double particleScale = 1.0;

    // Its teleportedOnce, set by the one portal pair it has gone through
    // (teleportToOther #178-207), after which mayTeleport refuses it every
    // portal: it flies through the next one unmoved.
    bool teleportedOnce = false;
};

struct State {
    Rules rules;
    std::vector<Turret> turrets;
    std::vector<Fireball> fireballs;
    glm::dvec2 boundsPx{0.0}; // the level's, for the cull

    bool playerKilled = false;
    std::string killedBy; // the fireball the player died in

    // WHERE a fireball was DESTROYED this tick, cleared and refilled by every
    // Tick, as Fire::State::blastsPx is. The original's `destroy` flag and
    // nothing else: ETHBeginContactCallback_fireball sets it, and the fireball's
    // own callback then deletes the entity and adds small_explosion.ent at the
    // point it stood (effects.json's small_explosion row). So the two removals
    // that are NOT a destroy leave nothing here - the cull at the level's edge,
    // which is the port's own safety net for a fireball that leaves the level
    // without touching anything (the original's flies on until it does), and a
    // torch's switch, which goes through killProjectile, a bare DeleteEntity
    // that sets no flag.
    //
    // The layer reads it on the tick that fills it (latchSimEffects) and draws
    // the explosion on the next frame; nothing in this file acts on it.
    std::vector<glm::dvec2> spentPx;

    // Portal shots a fire diamond turned into fireballs, which names them apart
    // from a carranca's. Not a carranca's doing at all - Game does the converting
    // (Diamonds.hpp) - but the fireballs land in this list because they are the
    // same entity and fly by the same rules.
    int converted = 0;

    // Before the step: every carranca whose stride has passed spits one.
    //
    // Returns what was fired this tick, which is also in `fireballs`. The caller
    // takes that to play the sound - AudioManager::playFireballSound, which
    // sounds.json already names - because a sound is the layer's and never the
    // tick's.
    std::vector<Fireball> Fire(double dt);

    // After the step: the fireballs fly, leave by the level's edge, and kill.
    void Tick(entt::registry& registry, entt::entity player, double dt);

    // Every carranca's own body: what a fireball's touch passes by.
    std::vector<entt::entity> Bodies() const;
};

// Every carranca of a level, each with the body LevelBuilder built for it, and
// the level's bounds to cull against. False, with `error`, for one whose stride
// does not read or is not positive.
bool Find(const Tscn::Scene& scene, const Roles::Table& roles, const LevelBuilder::Built& built, const Rules& rules,
          State& out, std::string& error);

} // namespace MagicPortals::Turrets
