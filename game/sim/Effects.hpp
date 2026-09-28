#pragma once

// The effects the original's script adds, as each call adds its one: the port's
// effects.json, and the arithmetic of where.
//
// An effect is an entity the script spawns by name - a burst when something
// teleports, a crystal is taken, a barrel goes off - and every one of them is an
// AddEntity call whose arguments are decoded: which .ent, the vector3 it is added
// at, its angle and its scale. effects.json holds those call-site facts, one row
// per call; what the entity itself carries (its picture, particle systems, light
// and <Scale>) is its .ent's, read by Art::LoadEnt when it is needed.
//
// This is the pure half, and renderer-free: a row and the points a call is given
// in, and back comes what AddEntity receives. Nothing here spawns, draws or reaches
// Game::Level; the layer (the visuals plan's step 6.3) places the pieces.
//
// THE ROWS BUILT. vanishEffect and appearEffect (Portal.angelscript): a traveller
// leaving through a portal is sucked in at the portal's rim on its own side and
// sparkles where it stood, and the exit portal flashes. Their drawing is step
// 7.3's; each later effect brings its own row with the step that draws it. And
// red_eyes (MainCharacter), the one effect every level shows: added once at the
// level's start and moved onto the character's face every frame. The shot's
// marks and ends (the visuals plan's 10.1). And what a crystal leaves where it
// was (10.2): crystal_pick where one is taken, crystal_vanish where a timed one
// runs out - both added to the CRYSTAL's own point, its depth included. And what a
// key and its keyhole leave (10.3): key_pick where a key is taken, the unlock halo
// at the keyhole a spent key flies into, and the three the keyhole adds when it
// goes - the dust at its door, the door's sparkles and its own burst. And what a
// diamond, a shock ring and a fire signal leave (10.4): shock_diamond_pick where a
// shock diamond is taken, shock_death at what its strike or a ring kills (and 10
// in front of a shock agent a fireball destroys), shock_strike between the diamond
// and its minion, burn_projectile where a fire diamond is taken and where each
// shot it turns is, and extinguish_fire where it goes down a drain and where a
// fire signal is put out.

#include <string>

#include <glm/glm.hpp>

namespace MagicPortals::Effects {

// How a call turns what it adds (AddEntity's angle).
enum class Turn {
    Fixed,                  // the literal `angleDeg`
    TowardTravellerMinus90, // radianToDegree(getAngle(normalize(traveller - portal))) - 90
    // getAngle(normalize(enemy - diamond)) handed over UNCONVERTED: radians in
    // [0, 2 pi), which the engine reads as degrees (D4 section 5), so the entity is
    // turned by at most 6.28 degrees. ETHCallback_shock_diamond #496-498 alone.
    RadiansAsDegrees,
};

// One call site: effects.json's row.
struct Rule {
    std::string entity;       // the .ent it adds
    double offsetUnits = 0.0; // g_scale.scale(n) along the call's direction: n
    double z = 0.0;           // the vector3's z
    // Whether the z went through g_scale.scale (level units) or is a bare literal
    // (the engine's units, which do not grow with the screen: 00_order 8.12b). The
    // port's unit is one level unit and it reads both as they are; the flag keeps
    // the decode's difference for the depth table (00_order K13).
    bool zScaled = true;
    Turn turn = Turn::Fixed;
    double angleDeg = 0.0;    // when Fixed
    double scale = 1.0;       // in g_scale.getScale()s: 1 is the entity at its own size
};

// The player's red eyes: MainCharacter's own effect, and the one effect every
// level shows. Not a call site of the shape above - AddScaledEntity takes no
// angle (ETHScriptWrapper.Scene.cpp:176-178 passes 0f) and the offset is not
// along any direction but the character's facing - so it has a row of its own.
//
// The constructor adds it once at the character's point; updateEyes then moves
// it every frame onto the face, and hides it with the character.
struct EyeRule {
    std::string entity;
    // g_scale.scale(vector3(eyeX, eyeY, z)) - the vector3 overload multiplies all
    // THREE components by the scale factor (SGlobalScale::scale, bytes
    // 5975..6072: vector3::opMul_r), so every number here is in level units.
    double xFrameAboveSeven = 0.0; // GetFrame() > 7: the sheet's right-facing half
    double xElse = 0.0;
    double y = 0.0;
    double yNoGravity = 0.0;
    double z = 0.0;                // above the CHARACTER's own depth, not the level's zero
    double scale = 1.0;            // AddScaledEntity's g_scale.getScale(): 1 in the port's units
    bool hideWithPlayer = false;   // updateEyes' redEyes.Hide(true) while the character is hidden
};

// What PortalManager::update and ETHCallback_portal do to the entity a placed
// portal IS (effects.json placed_portal): the colour the manager gives it while
// it is in its array, and the fade and the delay its own callback runs once its
// particle systems have been killed. Colours are Ethanon's (0..1), and only the
// entity's SPRITE reads them.
struct PlacedPortal {
    glm::dvec3 colourFirst{1.0};  // PORTAL_RED, portals[0] of a pair
    glm::dvec3 colourSecond{1.0}; // PORTAL_BLUE, portals[1]
    // V3_ZERO: the one portal of a level that grants more than one. Its sprite is
    // additive, so this is a halo that does not draw. A level that grants exactly
    // one colours its lone portal colourFirst instead.
    glm::dvec3 colourAlone{0.0};
    double killFadePerFrame = 0.0; // SetColor(GetColor() x this) once killTime is set
    double killFadeFrameMs = 0.0;  // of frame time: the original's rendered frame, INFERRED as 1/60 s
    double deathAfterKillMs = 0.0; // then portal_death.ent and DeleteEntity
};

struct Rules {
    Rule suck;     // vanishEffect's suck_effect.ent, at the entry portal
    Rule sparkles; // vanishEffect's sparkles.ent, where the traveller stood
    Rule appear;   // appearEffect's appear_effect.ent, at the exit portal
    // addFireball's fireball.ent, which every fireball in the game is: what a
    // carranca spits, what a fire diamond turns a portal shot into, and what both
    // dragons breathe. It is added at the point it is fired from and then follows
    // its own `dir` and `speed`, so its row is a point and nothing else.
    Rule fireball;
    // ETHCallback_fireball's small_explosion.ent, where a fireball was destroyed,
    // and explode()'s explosion_large.ent, where a bomb went off.
    Rule smallExplosion;
    Rule largeExplosion;
    // The two entities PortalManager::insertPortal adds for every portal the
    // player opens - the summon bubble, then the portal itself - and the burst
    // ETHCallback_portal leaves where it dies.
    Rule summon;
    Rule portal;
    Rule death;
    PlacedPortal placedPortal;
    EyeRule eyes; // MainCharacter's red_eyes.ent, on the character's face
    // The shot and what its tap and its end leave behind (the visuals plan's
    // 10.1): the flash on a projectile's first frame, 24 units out along its
    // direction, and ETHCallback_reflect_agent's where the old one was; the mark
    // at the tap, portal_mark or red_cross; the hourglass on a tap while a shot
    // flies; red_waves; and the two ends a doomed shot has.
    Rule projectileShoot;
    Rule projectileShootReflected;
    Rule portalMark;
    Rule redCross;
    Rule hourglass;
    Rule redWaves;
    Rule portalFail;
    Rule portalFailInField;
    // What a crystal leaves where it was (the visuals plan's 10.2):
    // pickCrystalEffect's crystal_pick where the character takes one, and
    // ETHCallback_timer's crystal_vanish, 1.3 times the size, where a timed one's
    // time runs out while it is still there. Each `z` is added to the crystal's
    // own (OnEntity).
    Rule crystalPick;
    Rule crystalVanish;
    // What a key and its keyhole leave (the visuals plan's 10.3; effects.json
    // _keys). ETHCallback_key's key_pick where a carrier takes a key, on the KEY's
    // own depth; its key_unlock_halo at the keyhole, at a depth of its own, once
    // the spent key's fly-in clock reaches `keyUnlockHaloAfterMs`; and
    // ETHCallback_keyhole's three when the keyhole goes, in its order: wall_destroy
    // at the door, door_locked_vanish on the door's depth - its z the one BARE
    // literal of the five (OnEntityBare) - and keyhole_vanish on the keyhole's.
    Rule keyPick;
    Rule keyUnlockHalo;
    double keyUnlockHaloAfterMs = 0.0; // the fly-in's w0: its time before frame 1
    Rule keyholeVanish;
    Rule doorLockedVanish;
    Rule wallDestroyAtDoor;
    // What a diamond, a shock ring and a fire signal leave (the visuals plan's
    // 10.4; effects.json _shock_burn_extinguish). Every one AddScaledEntity's, so
    // unturned, but the strike: ETHCallback_shock_diamond's shock_diamond_pick at
    // the diamond it takes, on the diamond's own depth; shock_death at the minion
    // its strike kills and ETHCallback_shock_area's at the character a ring kills,
    // each on the victim's depth, and ETHCallback_shock_agent's a BARE 10 in front
    // of an agent a fireball destroys (OnEntityBare); the strike between the two
    // (Strike); ETHCallback_fire_diamond's burn_projectile a bare 10 in front of
    // the diamond it takes, and burnProjectile's at each shot it turns, at a bare
    // 10 of its own; extinguish_fire at a diamond a drain takes, on its depth, and
    // ETHCallback_fire_signal's at a bare 10 of its own where its torch stands.
    Rule shockDiamondPick;
    Rule shockDeath;
    Rule shockDeathAtAgent;
    Rule shockStrike;
    Rule burnAtPickup;
    Rule burnAtShot;
    Rule extinguishAtDiamond;
    Rule extinguishAtSignal;
};

// False, with `error`, when the file cannot be read or any row is missing, has a
// field missing, or has one that is not what it must be. No field has a default.
bool LoadRules(const std::string& path, Rules& out, std::string& error);

// What AddEntity receives from one call: the entity, where (its xy in level
// units, +y down, and its z as the rule says), its angle and its scale.
struct Spawn {
    std::string entity;
    glm::dvec2 atPx{0.0};
    double z = 0.0;
    bool zScaled = true;
    float angleDeg = 0.0f;
    double scale = 1.0;
};

// vanishEffect's direction: normalize(traveller - portal), in floats as the
// script's vector2 is, and zero when the two coincide (GameMath.h:755-761).
glm::dvec2 VanishDirection(const glm::dvec2& travellerPx, const glm::dvec2& portalPx);

// Ethanon's angle for `rule` along `direction`: the literal, or
// radianToDegree(getAngle(direction)) - 90, getAngle(v) being atan2f(v.x, v.y)
// wrapped into [0, 2 pi) (GameMath.h:711-715) - in floats, through DetMath, as
// Art::Shot::AngleDegrees computes the shot's - or that getAngle itself, the
// radians given back as the number of degrees (Turn::RadiansAsDegrees).
float AngleDegrees(const Rule& rule, const glm::dvec2& direction);

// vanishEffect(travellerPx, portalPx), called with where the traveller stands
// before it is moved and the portal it enters: the suck effect at the portal
// plus the direction times its offset, and the sparkles at the traveller (plus
// the direction times theirs, which is 0), both turned by the direction.
Spawn Suck(const Rules& rules, const glm::dvec2& travellerPx, const glm::dvec2& portalPx);
Spawn Sparkles(const Rules& rules, const glm::dvec2& travellerPx, const glm::dvec2& portalPx);

// appearEffect(exitPortalPx): at the exit portal.
Spawn Appear(const Rules& rules, const glm::dvec2& exitPortalPx);

// addFireball(pos, dir, killMainCharacter, fireballScale): the entity, at the
// point it is fired from. The three arguments the row does not carry are not the
// entity's placement - `dir` and `killMainCharacter` are custom data the sim
// already holds (Turrets::Fireball), and `fireballScale` scales its first
// particle system alone, which is the caller's number and rides with the
// fireball (Turrets::Fireball::particleScale).
Spawn Fireball(const Rules& rules, const glm::dvec2& atPx);

// ETHCallback_fireball's AddScaledEntity, at the fireball's own vector3 - whose
// z is the 0 addFireball gave it - and explode()'s, at the bomb with z
// g_scale.scale(32). Neither is given a direction or an angle.
Spawn SmallExplosion(const Rules& rules, const glm::dvec2& atPx);
Spawn LargeExplosion(const Rules& rules, const glm::dvec2& atPx);

// insertPortal's two AddEntity calls, in its order, and ETHCallback_portal's
// AddScaledEntity: all three at the portal's own point and given no direction.
Spawn Summon(const Rules& rules, const glm::dvec2& atPx);
Spawn Portal(const Rules& rules, const glm::dvec2& atPx);
Spawn Death(const Rules& rules, const glm::dvec2& atPx);

// A row given a point and nothing else - no direction, no turn - at that point:
// every shot-side row but projectile_shoot (the loader refuses one that says
// otherwise).
Spawn At(const Rule& rule, const glm::dvec2& atPx);

// AddScaledEntity at an ENTITY's own GetPosition() plus vector3(0, 0, z): the
// row's z added to the entity's own depth rather than standing for one of the
// level's (vector3::opAdd), and no direction. `entityZ` is that entity's depth on
// the level's axis (a node's eth_z), so the sum is as scaled as the row's z is:
// crystal_pick and crystal_vanish, whose scale(20) goes on the crystal's own z;
// key_pick and keyhole_vanish, whose scale(32) goes on the key's and the
// keyhole's; and the door's wall_destroy, at the door's own depth.
Spawn OnEntity(const Rule& rule, const glm::dvec2& entityPx, double entityZ);

// OnEntity for a row whose z is a BARE literal: door_locked_vanish's 2, which is
// in the engine's world units and does not grow with the screen (00_order 8.12b).
// Only the literal is divided by `scaleFactor` (art.json's 720 / 256) before it
// goes on the entity's depth - the entity's own is already on the level's axis -
// so the sum comes back as a level depth (zScaled true). A factor not above 0
// adds the literal as it stands, as the layer does for a level with no art.
Spawn OnEntityBare(const Rule& rule, const glm::dvec2& entityPx, double entityZ, double scaleFactor);

// ETHCallback_shock_diamond #466-524: AddEntity(shock_strike, (diamond.GetPosition()
// + enemy.GetPosition()) x 0.5, getAngle(normalize(enemy - diamond)), ...). The
// point is the midpoint of the two entities' points and of their depths - both on
// the level's axis, so the sum is too - plus the row's z; the angle is the rule's
// along normalize(enemy - diamond), in the script's floats, which for this row is
// the radians unconverted (Turn::RadiansAsDegrees).
Spawn Strike(const Rule& rule, const glm::dvec2& diamondPx, double diamondZ, const glm::dvec2& enemyPx, double enemyZ);

// A spent key's fly-in, as far as the port draws it: the clock of the
// WaypointManager ETHCallback_key builds for it, which says when key_unlock_halo
// is added. The flight itself - the key swung to the keyhole, slid in and faded -
// is not built (keys.json _not_built); nor is the key deleted at its last frame.
struct FlyIn {
    bool found = false;     // the key found its keyhole (Keys::Key::spent turned over)
    bool built = false;     // the next frame: the manager built and its clock at 0
    double ms = 0.0;        // the manager's m_time since
    bool haloAdded = false; // frame 1 reached, and the halo added: once
};

// One tick of it, given the key's state after the tick and the tick's length:
// true on the one tick key_unlock_halo is added. The tick `spent` first reads
// true is the frame foundKeyhole is set on; the next builds the manager with its
// clock at 0; every later one adds its ms, and the first to reach
// keyUnlockHaloAfterMs steps the manager to frame 1 - the 41st tick after the
// unlock at 60 Hz.
bool AdvanceFlyIn(const Rules& rules, FlyIn& clock, bool spent, double tickMs);

// ETHCallback_projectile's first frame: projectile_shoot where the shot has
// jumped to, `offsetUnits` out from `fromPx` along normalize(towardPx - fromPx),
// in the script's floats; unturned.
Spawn ProjectileShoot(const Rules& rules, const glm::dvec2& fromPx, const glm::dvec2& towardPx);

// The colour PortalManager::update gives the portal at `index` of an array of
// `placed` portals on a level whose maxPortals is `maxPortals`, or nothing when
// it colours that index at all (2 and beyond, which no level reaches today).
// #185-300, read in its own order.
bool ManagerColour(const PlacedPortal& rules, int index, int placed, int maxPortals, glm::dvec3& out);

// One frame of the kill fade: GetColor() x killFadePerFrame per killFadeFrameMs
// of frame time.
glm::dvec3 KillFade(const PlacedPortal& rules, const glm::dvec3& colour, double frameMs);

// updateEyes' g_scale.scale(vector3(eyeX, eyeY, z)), in level units: what is
// added to the CHARACTER's own position, its z included. `drawnFrame` is the
// character's cell in its 4 x 4 sheet - ETHEntity::GetFrame, which the port reads
// as SpriteAnimationComponent.firstFrame + frame - so rows 2 and 3 (frames 8-15,
// walking right and pushing right) take the right-hand x.
glm::dvec3 EyeOffset(const EyeRule& rule, int drawnFrame, bool noGravity);

// MainCharacter's constructor AddScaledEntity, as updateEyes leaves it on the
// frame: at the character's point plus that offset, at the character's own depth
// plus its z, turned 0 and at the rule's scale.
Spawn RedEyes(const Rules& rules, const glm::dvec2& characterPx, double characterZ, int drawnFrame, bool noGravity);

} // namespace MagicPortals::Effects
