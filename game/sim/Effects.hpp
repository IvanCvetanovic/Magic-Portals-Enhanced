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
// fire signal is put out. And what a wall, a stone and a lift door leave (11.1):
// wall_destroy where a breakable wall goes, turned by the wall and at its
// destroyEffectScale; rolling_stone_destroy where a stone is destroyed; and
// door_lift_particles at the edge of a switched door as it starts to open or to
// close. And what a torch, its light wall, a weightless player and a gravity
// well add (11.2): the lit torch itself, light_from_projectile, with the sparkles
// and the burst its lighting adds and the signal that orbits it; the torch put
// back, its sparkles and its burst when the signal is shot; the light wall's dust
// as it goes and its smoke and itself as it comes back; the trail a weightless
// player leaves; and the flash where it laps a well. And the level's end and a
// death (11.3): the door's red suck and sparkles, blood where the character or a
// minion was killed, and a skull where either fell. And chapter 3's ghost (12.2):
// the shots it fires, what each adds, and the dark puffs that follow it in.

#include <string>

#include <glm/glm.hpp>

namespace MagicPortals::Effects {

// How a call turns what it adds (AddEntity's angle).
enum class Turn {
    Fixed,                  // the literal `angleDeg`
    TowardTravellerMinus90, // radianToDegree(getAngle(normalize(traveller - portal))) - 90
    // radianToDegree(getAngle(its own direction)) - 90, which the entity's callback
    // sets every frame: the chapter-3 ghost's shot (ETHCallback_enemy_shoot
    // #139-148), whose direction never changes.
    AlongFlightMinus90,
    // getAngle(normalize(enemy - diamond)) handed over UNCONVERTED: radians in
    // [0, 2 pi), which the engine reads as degrees (D4 section 5), so the entity is
    // turned by at most 6.28 degrees. ETHCallback_shock_diamond #496-498 alone.
    RadiansAsDegrees,
    // The angle of the entity it is added at, in degrees as the engine keeps it:
    // ETHCallback_breakable_wall #45-47 hands AddEntity its wall's GetAngle(). It
    // is along no direction, so AngleDegrees gives 0 and the caller the angle
    // (WallDestroy).
    EntityAngle,
};

// bounce(entity, from, to, strideMs) as bounceLightEntity calls it: the entity's
// scale from `from` to `to` and back, one leg a stride, eased by smoothEnd
// (Art::Pulse is the same curve).
struct Bounce {
    double from = 1.0;
    double to = 1.0;
    double strideMs = 0.0;
};

// One call site: effects.json's row.
struct Rule {
    std::string entity;       // the .ent it adds
    double offsetUnits = 0.0; // g_scale.scale(n) along the call's direction: n
    // Or g_scale.scale of a fixed (x, y), added along no direction at all:
    // levelFinishedEffect's (0, 16) below the door. A row states one or the other
    // (offset_units is a number or a pair), never both.
    glm::dvec2 offsetFixedUnits{0.0};
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

// followUp(entity, destPos, interpStride, updateRate, dontPause)
// (utilEntityEffect.angelscript, bytes 328407..329130), which an entity's own
// callback calls every frame: it eases the entity toward a point by smoothEnd over
// `strideMs`, and aims again at most every `rateMs`. The destination is another
// entity's point plus `offsetUnits`, in the level's units (g_scale.scale).
struct FollowUpRule {
    glm::dvec2 offsetUnits{0.0};
    double strideMs = 0.0;
    double rateMs = 0.0;
    bool dontPause = false; // it counts GetLastFrameElapsedTime, which a pause does not stop
};

// What followUp keeps on the entity: its PositionInterpolator and switchTime.
struct FollowUp {
    glm::dvec2 from{0.0};
    glm::dvec2 to{0.0};
    double elapsedMs = 0.0;
    double sinceAimedMs = 0.0; // switchTime
    bool started = false;

    // One call, `frameMs` into the frame, with the entity at `atPx` and the one it
    // follows at `targetPx`: where the call puts the entity.
    glm::dvec2 Tick(const FollowUpRule& rule, const glm::dvec2& atPx, const glm::dvec2& targetPx, double frameMs);
    // PositionInterpolator::getCurrentPos: the end once the time is past, and
    // smoothEnd of the fraction before it.
    glm::dvec2 Current(const FollowUpRule& rule) const;
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
    // THE LEVEL'S END AND A DEATH (the visuals plan's 11.3). levelFinishedEffect's
    // two, in its order: red_suck_effect.ent at the door plus a fixed offset, then
    // red_sparkles.ent where the character stood.
    Rule finishSuck;
    Rule finishSparkles;
    // blood.ent, at the victim's own vector3: the character killed by anything
    // but a fall (checkGameLost), and a minion destroyed (ETHCallback_minion).
    Rule blood;
    // skull.ent: where the character fell out of the level, or under a minion a
    // killer floor took - each held inside the view by skullInsetUnits.
    Rule skull;
    double skullInsetUnits = 0.0; // g_scale.scale(12)
    // CHAPTER 3'S GHOST AND ITS SHOTS (the visuals plan's 12.2, planning step 98).
    // addEnemyShoot's enemy_shoot.ent, the shot itself (the portal_mark.ent it
    // adds where the shot is going is portalMark below, the player's shot's own
    // row: the same arguments, one row for both callers since planning step 112);
    // ETHCallback_enemy_shoot's projectile_shoot.ent on its first call and
    // portal_fail_in_antiportal.ent where it is destroyed; and
    // ETHCallback_ghost's ghost_appear_effect.ent, with the followUp and the
    // Scale(1.005) its own callback runs every frame.
    Rule enemyShoot;
    Rule enemyShootFlash;
    Rule enemyShootBurst;
    Rule ghostAppear;
    FollowUpRule ghostAppearFollow;
    double ghostAppearScalePerFrame = 1.0;
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
    // What a wall, a stone and a lift door leave (the visuals plan's 11.1;
    // effects.json _walls_stones_doors). ETHCallback_breakable_wall's
    // wall_destroy at the wall's own point and depth, turned by the wall and
    // scaled by its float `wallDestroyScaleKey` where that is above 0;
    // ETHCallback_rolling_stone's rolling_stone_destroy at 1.3 where a stone is
    // destroyed, at a depth of its own; and ETHCallback_door_lift's two
    // door_lift_particles, at the door's point plus its own height times
    // `doorLift...SizeY` down the screen, on its depth - a bare 0 there.
    Rule wallDestroy;
    std::string wallDestroyScaleKey;
    Rule stoneDestroy;
    Rule doorLiftOpen;
    Rule doorLiftClose;
    double doorLiftOpenSizeY = 0.0;  // x GetSize().y: -0.5, the shut door's top edge
    double doorLiftCloseSizeY = 0.0; // +0.5, the raised door's bottom edge
    // What a torch, its light wall, a weightless player and a gravity well add
    // (the visuals plan's 11.2; effects.json _torches_trail_roundabout).
    // ETHCallback_light_off's three, in its order, when a shot lights a torch:
    // the lit torch itself on the torch's own point and depth, its sparkles
    // there, and portal_death at the spent shot; addFireSignalIfNecessary's
    // signal at the torch, where the level has a light wall - and the lit torch
    // then bounces (litTorchBounce). ETHCallback_fire_signal's when the signal is
    // shot: the torch put back and its sparkles at vector3(originalPos, 10), a
    // bare 10 (At), and portal_death at the shot. addLightWall's smoke and the new
    // wall at the ghost, turned by it, the wall fading in from lightWallAlphaFrom
    // at lightWallAlphaPerSecond; ETHCallback_light_wall's dust at the wall as it
    // goes, turned by it. addTrail's movement_trail on the character, and
    // computeRoundabout's flash at the well's area, vector3(xy, -5) bare.
    Rule lightFromProjectile;
    Bounce litTorchBounce;
    Rule lightOnSparkles;
    Rule portalDeathAtLitTorch;
    Rule fireSignal;
    Rule lightOffAtSignal;
    Rule lightOffSparkles;
    Rule portalDeathAtSignal;
    Rule lightWallCreate;
    Rule lightWallAtGhost;
    double lightWallAlphaFrom = 0.0;
    double lightWallAlphaPerSecond = 0.0;
    Rule wallDestroyLightWall;
    Rule movementTrail;
    double trailMinSpeed = 0.0; // |GetLinearVelocity()| / getScale(), the port's metres a second, strictly above
    double trailEveryMs = 0.0;  // trailElapsedTime strictly above
    Rule roundabout;
    // ETHCallback_anti_portal_agent's antiportal_explosion.ent, where a shot killed
    // an agent: at the agent's point and scale(32), unturned (the remaster record's
    // step 126).
    Rule antiportalExplosion;
    // ETHCallback_hand_waypoint0's smoke_vanish.ent, where 2-20's lesson hand is
    // deleted as its handle is first held: the hand's point 32 up, on its depth,
    // unturned (the remaster record's step 152).
    Rule smokeVanish;
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
    // A depth already on the level's axis that `z` is added to, whichever way `z`
    // is written: another entity's own GetPositionZ(), to which a call adds a BARE
    // literal (ETHCallback_ghost's `GetPositionZ() + 1f`). 0 for every call that
    // gives its z whole.
    double zBaseUnits = 0.0;
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

// ETHCallback_breakable_wall #14-31: effectScale, the wall's float where it is
// above 0 - a missing one reads 0 (ETHEntity.cpp:851-856) - and 1 otherwise. The
// same number multiplies the wall's earthquake (#73-76).
double EffectScale(double datum);

// ETHCallback_breakable_wall #33-59: AddEntity('wall_destroy.ent',
// wall.GetPosition(), wall.GetAngle(), @e, '', g_scale.scale(effectScale)) - at
// the wall's own point and depth (its z plus the row's 0), turned by the wall's
// Ethanon angle as it is, and at effectScale times the row's scale.
Spawn WallDestroy(const Rules& rules, const glm::dvec2& wallPx, double wallZ, double wallAngleDeg, double effectScale);

// AddEntity at an entity's own GetPosition() - its z plus the row's - and
// turned by its GetAngle() as it is: a row of Turn::EntityAngle, which is
// light_wall_create and light_wall at the ghost addLightWall seeks, and the light
// wall's own dust (ETHCallback_light_wall #109-134). WallDestroy is this with a
// breakable wall's effectScale on top.
Spawn OnEntityTurned(const Rule& rule, const glm::dvec2& entityPx, double entityZ, double entityAngleDeg);

// MainCharacter::addTrail's clock, one frame of it: `tickMs` added whatever the
// speed (#8-24), and true - the clock back at 0 - where `speed` (the body's, in
// the port's metres a second) is above trailMinSpeed and the clock above
// trailEveryMs, both strictly (#44-60, #82-95).
bool TrailDue(const Rules& rules, double& clockMs, double speed, double tickMs);

// ETHCallback_door_lift #402-434 (`opening`) and #548-580: AddScaledEntity(
// 'door_lift_particles.ent', door.GetPosition() -/+ vector3(0, 0.5 x GetSize().y,
// 0), getScale()) - at the door's point moved down the screen by the row's share
// of `doorHeightUnits`, the door's own drawn height, and never turned; its z the
// door's plus the row's bare 0 (OnEntityBare).
Spawn DoorLiftDust(const Rules& rules, bool opening, const glm::dvec2& doorPx, double doorZ, double doorHeightUnits,
                   double scaleFactor);

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

// ---- the level's end and a death (the visuals plan's 11.3) ---------------------

// levelFinishedEffect(charPos, doorPos), called by checkGameEnd #34-47 with the
// character's and the final door's GetPositionXY on the frame the two collide:
// #2-39 the suck at the door plus g_scale.scale(vector2(0, 16)), #53-77 the
// sparkles at the character. Both at a bare z 0, turned 0, at the script's size.
Spawn FinishSuck(const Rules& rules, const glm::dvec2& doorPx);
Spawn FinishSparkles(const Rules& rules, const glm::dvec2& characterPx);

// AddScaledEntity('blood.ent', victim.GetPosition(), getScale()): the victim's
// whole vector3, so its z is the victim's own depth (on the level's axis) plus the
// row's 0, and AddScaledEntity's angle is 0.
Spawn Blood(const Rules& rules, const glm::dvec2& victimPx, double victimZ);

// Where checkGameLost puts the skull of a character that left the level
// (#255-361), from the character's GetPositionXY, the camera's top-left corner
// (GetCameraPos) and the screen's size (GetScreenSize), all on the level's axis:
//   - with gravity, straight under it at the view's foot: (x, camera.y + screen.y
//     - inset);
//   - weightless, its point held inside the view (max(min(p, camera + screen),
//     camera)) and then moved the inset toward the view's middle, along
//     normalize(p - middle).
glm::dvec2 FallSkullPoint(const Rules& rules, const glm::dvec2& characterPx, const glm::dvec2& cameraPx,
                          const glm::dvec2& screenPx, bool noGravity);

// Where ETHBeginContactCallback_enemy_killer puts one under a minion it takes
// (#24-54): at the minion's x and the view's foot less the inset - the gravity
// branch's point, whatever the level.
glm::dvec2 KillerSkullPoint(const Rules& rules, double minionX, const glm::dvec2& cameraPx,
                            const glm::dvec2& screenPx);

// AddScaledEntity('skull.ent', vector3(point, 0), getScale()).
Spawn Skull(const Rules& rules, const glm::dvec2& atPx);

// ---- chapter 3's ghost and its shots (the visuals plan's 12.2) ------------------

// addEnemyShoot #16-45: AddEntity('enemy_shoot.ent', vector3(casterPos, scale(10)),
// 0, ...) - drawn at the shot's own point from its first call, turned along its
// direction by that call (#139-148).
Spawn EnemyShoot(const Rules& rules, const glm::dvec2& shotPx, const glm::dvec2& direction);
// addEnemyShoot #127-156's AddEntity('portal_mark.ent', vector3(finalPos, scale(32)),
// 0, ..., getScale()) is the player's shot's call with the same arguments: the
// layer adds it as At(rules.portalMark, finalPos), one row for both callers
// (planning step 112).
// ETHCallback_enemy_shoot #57-71, its first call, after the nudge:
// AddScaledEntity('projectile_shoot.ent', GetPosition(), getScale()) - the shot's
// own vector3, its depth `shotZUnits` included.
Spawn EnemyShootFlash(const Rules& rules, const glm::dvec2& shotPx, double shotZUnits);
// #324-350, once it is destroyed: AddScaledEntity('portal_fail_in_antiportal.ent',
// vector3(GetPositionXY(), 0), getScale()).
Spawn EnemyShootBurst(const Rules& rules, const glm::dvec2& shotPx);
// ETHCallback_ghost's init #150-169: AddScaledEntity('ghost_appear_effect.ent',
// vector3(currentPos, GetPositionZ() + 1f), getScale()) - at the ghost's point as it
// is added, the knight's, a BARE 1 over the ghost's own depth `ghostZUnits`.
Spawn GhostAppear(const Rules& rules, const glm::dvec2& ghostPx, double ghostZUnits);

} // namespace MagicPortals::Effects
