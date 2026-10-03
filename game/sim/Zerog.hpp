#pragma once

// Zero gravity: the levels whose level_properties set `no_gravity`.
//
// It is a MOVEMENT MODE and not a switch, which is why this port refused those
// levels for so long. One flag turns on three separate things in the original,
// and a level started with any of them missing looks like a level that works:
//
//   - the world's gravity becomes V2_ZERO, against (0, scale(10)) otherwise, so
//     NOTHING in the level falls - not the player, not a crate, not a minion;
//   - MainCharacter::update skips ScreenPad::update entirely, so the two walking
//     buttons do nothing. There is no walking at all in these levels;
//   - PortalManager::managePortalInsertion calls applyImpulse, and THAT is how
//     the player moves. Every tap the manager accepts shoves the character away
//     from where it aimed.
//
// So there is no module here that owns a body or a tick. What the port needs is
// a flag on the level, two branches in Game, and the arithmetic below.
//
// WHAT THE RECOIL IS, and what unit it is in. applyImpulse (bytes 142841..143178) reads
//
//     SetLinearVelocity(GetLinearVelocity() + normalize(playerPos - finalPos) * scale(1.4))
//
// - an ADD onto the existing velocity, so shots accumulate and drift is the mechanic. `scale` is
// SGlobalScale::scale, one multiply by m_scaleFactor, which is GetScreenSize().y / 256 (main and
// preLoop call updateScaleFactor with 256: briefs 00_order 8.12a; 480 is only the pre-main
// constructor value). So 1.4 is 70 UNITS a second of the original's physics, on every screen. That
// physics steps a FIXED 1/30 s a frame (clock.json), so the shove carries a body 2.33 units a
// frame: 140 units a second of wall time at 60 Hz, and the 40-44 F10 measured at 17-19 frames a
// second (70 x fps / 30). The metres_per_second below is the decoded 1.4; Portals::State::Shoot
// gives the shooter it at ScriptClock::SimRatio times over, as the player's whole velocity is
// carried (Player.hpp). zerog.json says it at length.

#include <string>

#include <glm/glm.hpp>

namespace MagicPortals::Zerog {

struct Rules {
    std::string flagName;                // the level property that turns it on
    double recoilMetresPerSecond = 0.0;  // applyImpulse's scale(1.4)
    double worldGravityPx = 0.0;         // V2_ZERO: nothing in the level falls
};

// From the port's own zerog.json. False, with `error`, for a file that does not
// read - and for one whose world gravity is not zero, because the mode IS zero
// gravity and a file saying otherwise is describing something else.
bool LoadRules(const std::string& path, Rules& out, std::string& error);

// applyImpulse as a velocity to ADD to the shooter's own: a unit vector from
// where the shot was aimed back to the player, times the recoil. You are pushed
// AWAY from where you tapped.
//
// Turned over in y, because the port's pixels count down and the engine's
// metres count up. The magnitude is carried straight through, in the port's metres
// a second: the caller gives it at the shooter's SimRatio (see above).
//
// A tap exactly ON the player is the one case normalize cannot answer, and this
// gives nothing rather than a NaN that would poison the body's velocity.
glm::vec3 Impulse(const glm::dvec2& playerPx, const glm::dvec2& aimedPx, double metresPerSecond);

} // namespace MagicPortals::Zerog
