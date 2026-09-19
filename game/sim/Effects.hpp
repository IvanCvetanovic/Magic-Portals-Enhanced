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
// 7.3's; each later effect brings its own row with the step that draws it.

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

struct Rules {
    Rule suck;     // vanishEffect's suck_effect.ent, at the entry portal
    Rule sparkles; // vanishEffect's sparkles.ent, where the traveller stood
    Rule appear;   // appearEffect's appear_effect.ent, at the exit portal
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

} // namespace MagicPortals::Effects
