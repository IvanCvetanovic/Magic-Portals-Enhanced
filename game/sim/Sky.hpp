#pragma once

// The sky the original pins to the camera: StaticSky (Sky.angelscript), decoded
// in data/sky.json.
//
// A level places its sky where its editor left it. The original overwrites that
// position on every frame, so the level's number is never drawn: the sky sits at
// the camera's corner plus half a screen, which is the view's centre, and is one
// screen tall. The port drew the level's number, and wherever its camera moved -
// right along a long level, down a tall one - the black behind the scene showed
// (step 44 counted 34 levels). The original's own frames put its sky at the
// screen's centre on every level measured, whatever the camera had done.
//
// WHAT IS BUILT, and when. Game::preLoop builds StaticSky on every level whose
// `properties` does not set `space_bg`; the 19 that do (4-01 to 4-19) build
// SpaceSky instead, at the end of this file. StaticSky finds every entity named
// `sky` or `sky.ent` and one named `satellite`.
//
// THE ARITHMETIC, per sky t, in the level's own pixels (sky.json's _units):
//   scale        view height / picture height, once, at build
//   width        the LAST sky's picture width times its scale (m_width)
//   position     camera corner + pitch + (width * t, 0) + (scrollX, 0), every frame,
//                pitch being view * 0.5 for a still sky and size * 0.5 moved by
//                one size for the first and last of a scrolling strip
//   drawn centre position - pivot, the pivot a scrolling strip's ends are given
//   scroll       scrollX -= scroll * frame, and back to 0 once |scrollX| >= width
//   satellite    camera corner + where the level put it, every frame
//
// Every level places at most one sky, whose picture is as tall as the view, and
// none scrolls: in play this is "the sky at the view's centre". The rest is
// carried because it is decoded and cheap, and tests/test_mp_sky.cpp pins it.
//
// Renderer-free, and free of the registry: the camera is passed in, and what
// comes back is where to draw. The layer places the quads.

#include "sim/Motion.hpp"
#include "sim/Sprites.hpp"
#include "sim/Tscn.hpp"

#include <cstddef>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace MagicPortals::Sky {

// The port's sky.json, static_sky.
struct Rules {
    std::string propertiesEntity;       // properties: where space_bg is read
    std::string spaceBgKey;             // space_bg: set, and SpaceSky runs instead
    std::vector<std::string> skyNames;  // sky, sky.ent
    std::string satelliteName;          // satellite
    std::string scrollKey;              // scroll: a sky's own float
    double screenPitch = 0.0;           // 0.5
    double frameCapMs = 0.0;            // 200: unitsPerSecond's min
};

bool LoadRules(const std::string& path, Rules& out, std::string& error);

// One sky as scaleSky leaves it.
struct Layer {
    std::string node;          // its entity node
    glm::dvec2 imagePx{0.0};   // its picture's own size
    double scale = 1.0;        // view height / picture height
    double scrollValue = 0.0;  // its `scroll`; 0 when it has none, as GetFloat answers
    double scrollX = 0.0;      // scrollPos.x; its y is never anything but 0
    double pivotX = 0.0;       // the pivot adjust a scrolling strip's ends take, in level px
};

struct Controller {
    // False on a level that sets space_bg, and on one that places neither a sky
    // nor a satellite, where there is nothing for it to move.
    bool running = false;
    std::vector<Layer> skies;  // in the level's file order
    double widthPx = 0.0;      // m_width
    double scrollValue = 0.0;  // m_scrollValue, the last sky's
    bool scroll = false;       // m_scroll: the last sky's scroll above 0

    bool hasSatellite = false;
    std::string satelliteNode;
    glm::dvec2 satelliteOriginalPx{0.0}; // where the level put it: its originalPos
};

// StaticSky's constructor and scaleSky, over a level's scene and the sprites the
// layer draws for it (for each sky's picture size). `viewHeightPx` is the view's
// height in the level's pixels. False, with `error`, only for a sky the scene
// places without a picture, which the original could not have sized either.
bool Build(const Rules& rules, const Tscn::Scene& scene, const std::vector<Sprites::Sprite>& sprites,
           double viewHeightPx, Controller& out, std::string& error);

// Where StaticSky::position puts sky t this frame: its entity's position, in the
// level's pixels. `viewCentrePx` and `viewPx` are the camera as it is drawn.
glm::dvec2 PositionPx(const Rules& rules, const Controller& sky, std::size_t t, const glm::dvec2& viewCentrePx,
                      const glm::dvec2& viewPx);

// Where its picture's centre is drawn: the position less the pivot adjust.
glm::dvec2 DrawnCentrePx(const Rules& rules, const Controller& sky, std::size_t t, const glm::dvec2& viewCentrePx,
                         const glm::dvec2& viewPx);

// Its picture's drawn size: the picture times its scale.
glm::dvec2 DrawnSizePx(const Controller& sky, std::size_t t);

// Where the satellite is drawn this frame: the camera's corner plus where the
// level put it. Only meaningful when hasSatellite.
glm::dvec2 SatellitePx(const Controller& sky, const glm::dvec2& viewCentrePx, const glm::dvec2& viewPx);

// The scroll half of StaticSky::update, run AFTER the frame's positions: the
// original places each sky and only then moves its scrollPos, so a frame draws
// the scroll the previous one left. Nothing on a still sky.
void Advance(const Rules& rules, Controller& sky, double dtS);

// ---- SpaceSky ---------------------------------------------------------------
//
// The sky of the 19 space levels (sky.json space_sky; planning step 122). Four
// entities, each found by its exact name and each optional, all moved from the
// camera every frame:
//   space sky   camera corner + view x (0.5, 0.5), at its own size
//   spiral      camera corner + view x (0.5, 1), the bottom-centre, a square as
//               wide as the view's diagonal, coloured c (its `alpha`, else 0.7)
//               and turned 1.7 degrees a second clockwise
//   planet      the bottom-centre plus its linearMotion's bob: 1 u up and down,
//               0.5 radians a second, from PI
//   eclipse     x at the view's centre; y its OWN y plus the camera corner's y,
//               added every frame
// Positions are the ENTITIES' points, as the original sets them; each picture
// hangs off its point by its own offset (the planet's 60 u up, the eclipse's 30).

// The port's sky.json, space_sky.
struct SpaceRules {
    std::string planetName;            // planet_bg.ent
    std::string spiralName;            // spiral
    std::string skyName;               // space_sky
    std::string eclipseName;           // eclipse.ent
    double spiralDiagonalScale = 0.0;  // 1: the side is the view's diagonal times this
    glm::dvec2 spiralPitch{0.0};       // (0.5, 1) of the view from its corner
    double spiralTurnDegPerS = 0.0;    // -1.7: Ethanon's degrees, counter-clockwise positive
    std::string spiralColourKey;       // alpha: the spiral's own float, its rgb when present
    double spiralColourDefault = 0.0;  // 0.7
    glm::dvec2 skyPitch{0.0};          // V2_HALF
    glm::dvec2 planetPitch{0.0};       // (0.5, 1)
    Motion::Row planet;                // speed 0.5, stride 1, vertical, axis 0, start PI
    double eclipsePitchX = 0.0;        // 0.5
};

bool LoadSpaceRules(const std::string& path, SpaceRules& out, std::string& error);

// SpaceSky as its constructor leaves it, and what its update carries between
// frames: the spiral's turn, the planet's angle and the eclipse's y.
struct Space {
    // True on a level whose first `properties` carries `space_bg` at all. The
    // node names are empty for an entity SeekEntity would not find.
    bool running = false;
    std::string skyNode;
    std::string spiralNode;
    std::string planetNode;
    std::string eclipseNode;
    double spiralSidePx = 0.0;   // scaleToSize's side, in the level's pixels
    double spiralColour = 1.0;   // SetColor(c, c, c): the rgb, the alpha kept
    Motion::Turn spiralTurn;     // what AddToAngle has added to the level's angle
    Motion::Linear planetBob;    // the planet's linearMotion, first called by the constructor
    double eclipseYPx = 0.0;     // the eclipse entity's y, which update adds the corner's y to
};

// SpaceSky's constructor over a level's scene. `viewPx` is the view in the
// level's pixels, whose diagonal the spiral is scaled to. `constructorFrameMs` is
// the frame its first linearMotion call steps the planet by: the frame before the
// level's first update, which the port takes to be one tick (sky.json). A level
// without space_bg builds nothing and says so with running false. False, with
// `error`, only for a spiral whose `alpha` is there and not a number.
bool BuildSpace(const SpaceRules& rules, const Rules& skyRules, const Motion::Rules& motion,
                const Tscn::Scene& scene, const glm::dvec2& viewPx, double constructorFrameMs, Space& out,
                std::string& error);

// The half of one SpaceSky::update that carries over, over a frame of `frameMs`:
// the planet's linearMotion step and the spiral's AddToAngle (both
// unitsPerSecond's, capped at motions.json's frame cap), and the eclipse's y plus
// the corner's y of the camera this frame reads. Run before the frame is placed,
// on every tick: a paused level runs none, which is the original's m_factor 0 for
// the first two, and its camera does not move for the third.
void AdvanceSpace(const Motion::Rules& motion, Space& space, const glm::dvec2& viewCentrePx, const glm::dvec2& viewPx,
                  double frameMs);

// Where update puts each entity's point this frame, in the level's pixels, from
// the camera as it is drawn.
glm::dvec2 SpaceSkyPx(const SpaceRules& rules, const glm::dvec2& viewCentrePx, const glm::dvec2& viewPx);
glm::dvec2 SpiralPx(const SpaceRules& rules, const glm::dvec2& viewCentrePx, const glm::dvec2& viewPx);
glm::dvec2 PlanetPx(const SpaceRules& rules, const Space& space, const glm::dvec2& viewCentrePx,
                    const glm::dvec2& viewPx);
glm::dvec2 EclipsePx(const SpaceRules& rules, const Space& space, const glm::dvec2& viewCentrePx,
                     const glm::dvec2& viewPx);

} // namespace MagicPortals::Sky
