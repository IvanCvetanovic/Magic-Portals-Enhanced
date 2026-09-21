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
// level's start and moved onto the character's face every frame.

#include <string>

#include <glm/glm.hpp>

namespace MagicPortals::Effects {

// How a call turns what it adds (AddEntity's angle).
enum class Turn {
    Fixed,                  // the literal `angleDeg`
    TowardTravellerMinus90, // radianToDegree(getAngle(normalize(traveller - portal))) - 90
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
// Art::Shot::AngleDegrees computes the shot's.
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
