#pragma once

// The particle systems an .ent carries: what makes the original's scenery move.
//
// Nothing in a converted level moves. Enhanced's converter writes a level's
// sprites and drops the rest of each entity, so the port draws a still picture
// of a game whose crystals sparkle, whose torches burn and whose portals turn.
// The motion is not sprite animation - it is a PARTICLE SYSTEM held in each
// entity's own .ent, which the converter never read.
//
// 81 of the original's 190 .ent files carry one, and 21 of those carry TWO -
// portal_static among them, which is why this returns all of an entity's rather
// than its first. Every system states the SAME seventeen attributes and ten
// child elements, so this reader demands all of them and refuses a file missing
// one rather than quietly taking a default.
//
// Two numbers are enumerations of the engine's, decoded rather than guessed:
//   - alphaMode is gs2d's Video::ALPHA_MODE (Video.h): AM_PIXEL 0, AM_ADD 1,
//     AM_ALPHA_TEST 2, AM_NONE 3, AM_MODULATE 4. Of the 102 systems, 75 are 1,
//     23 are 0 and 4 are 4 (step 59's census). Every AM_ADD bitmap is alpha-less
//     and every AM_PIXEL one carries alpha - which is why sparkles.bmp works at
//     all, a bitmap with no alpha channel whose black ground adds nothing. The
//     four AM_MODULATE systems (black_fade, blood, dragon_knight, ghost_minion)
//     belong to entities no level places; the port draws them as a multiply
//     (MultiplyTexels, planning step 96).
//   - animationMode is ETHParticleSystem::FRAME_ANIMATION_MODE: PLAY_ANIMATION
//     1, PICK_RANDOM_FRAME 2.
//
// The bitmaps are named bare and live in the original's `particles/` directory
// - a third place, beside its `entities/` and `sprites/` - and, like every
// other image of the original's, they stay outside this repository.
//
// THE ARITHMETIC IS ETHANON'S, line by line (ETHParticleManager.cpp), and pure:
// Reset, Release and Step move one particle, and Drawn, DrawColour, QuadPx,
// WorldRotation and SlotFraction say how it is drawn. What a particle is drawn
// WITH - a quad, a z, a frame of the sheet - is the layer's, per frame, and never
// the simulation's: nothing here reaches Game::Level or the state hash.

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace MagicPortals::Particles {

// gs2d's Video::ALPHA_MODE.
inline constexpr int kAlphaPixel = 0;    // AM_PIXEL: GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA
inline constexpr int kAlphaAdd = 1;      // AM_ADD: GL_ONE, GL_ONE (GLES2Video.cpp:792-795)
inline constexpr int kAlphaTest = 2;     // AM_ALPHA_TEST: blending disabled on GLES2 (GLES2Video.cpp:800-804);
                                         // no .ent uses it, and Drawable refuses it
inline constexpr int kAlphaModulate = 4; // AM_MODULATE: GL_ZERO, GL_SRC_COLOR (GLES2Video.cpp:796-799)

// One <ParticleSystem>, exactly as the .ent states it. Times are milliseconds
// and lengths are the original's pixels.
struct System {
    std::string bitmap; // <Bitmap>, an image among the original's particles/

    int count = 0;              // particles: how many the system holds at once
    bool allAtOnce = false;     // else they are released staggered across a lifetime
    int alphaMode = kAlphaAdd;  // Video::ALPHA_MODE, as the file says it
    bool additive = true;       // alphaMode 1, AM_ADD
    int animationMode = 1;      // 1 plays the sheet by age, 2 picks a frame at random
    int repeat = 0;             // how many lives each particle gets; 0 is forever

    double lifeTimeMs = 0.0;
    double randomLifeTimeMs = 0.0; // spread, applied as +/- half

    double size = 0.0;
    double randomizeSize = 0.0; // spread, +/- half
    double growth = 0.0;        // added to the size each frame-speed unit
    double minSize = 0.0;
    double maxSize = 0.0;

    double angleStart = 0.0;     // degrees
    double randAngleStart = 0.0; // spread, applied as 0..this
    double angleDir = 0.0;       // degrees added each frame-speed unit
    double randAngle = 0.0;      // spread on angleDir, +/- half

    glm::dvec2 gravity{0.0};        // added to the direction each frame-speed unit
    glm::dvec2 direction{0.0};      // the velocity a particle starts with
    glm::dvec2 randomizeDir{0.0};   // spread on it, +/- half
    glm::dvec2 startPoint{0.0};     // from the entity's own position
    // <StartPoint>'s z: Ethanon reads the start point as a vector3 (ETHParticleSystem.cpp:130)
    // and the system's pieces draw at the entity's depth plus it. Scaled with the rest
    // (ETHParticleSystem.cpp:33), so it is on the level's axis at the entity's scale, not a
    // bare literal. The layer draws a level placement's system at its owner's eth_z plus
    // it (planning step 92) and a spawned effect's from its Scaled copy (steps 83-86).
    double startPointZ = 0.0;
    glm::dvec2 randStartPoint{0.0}; // spread on it, +/- half

    glm::dvec4 colour0{1.0}; // at birth
    glm::dvec4 colour1{1.0}; // at death, lerped by age
    // <Luminance>, ETHParticleSystem's `emissive` (ETHParticleSystem.cpp:134):
    // what lifts an alpha-blended particle over the scene's ambient.
    glm::dvec3 luminance{1.0};

    int columns = 1; // <SpriteCut>, the grid its bitmap is cut into
    int rows = 1;

    int Frames() const { return columns * rows; }
};

// Reads every particle system out of an .ent, in the order the file states
// them.
//
// An empty result is the ordinary case - 109 of the 190 files carry none - and
// is not an error. False, with `error`, when the file cannot be read, is not
// the UTF-16 the original writes, or states a system missing a field.
bool Load(const std::string& path, std::vector<System>& out, std::string& error);

// Load's two halves, for a reader that wants the rest of the file too
// (Art::LoadEnt): the .ent's text, decoded from the UTF-16 LE every one of the
// original's is written in (false, with `error`, for anything else), and the
// systems out of that text, `path` naming the file in an error.
bool ReadEnt(const std::string& path, std::string& text, std::string& error);
bool Parse(const std::string& text, const std::string& path, std::vector<System>& out, std::string& error);

// Whether the port can draw a system's blend: AM_ADD and AM_PIXEL, the two the
// levels' 897 placed systems use (887 and 10, their own <Particles>: planning
// step 91, pinned in test_mp_sprites), and AM_MODULATE, which the engine has no
// pipeline for and the layer draws as the multiply it is (MultipliesExactly,
// MultiplyTexels; planning step 96). AM_ALPHA_TEST and AM_NONE, which no .ent
// uses, are refused by name rather than drawn as something they are not.
bool Drawable(const System& system);

// ---- AM_MODULATE, as a multiply ----------------------------------------------
//
// AM_MODULATE blends glBlendFunc(GL_ZERO, GL_SRC_COLOR) (GLES2Video.cpp:796-799):
// what is under the particle is multiplied by the texel times the particle's
// colour, and NOTHING reads the alpha - neither the texel's nor the colour's.
// The port's engine has no such blend. What it has is the ordinary mix, and a
// texel that is black at an alpha of (255 - grey) mixes to exactly the same
// result: 0 x a + d x (1 - a) = d x grey / 255. That is the loading screen's
// black halo since planning step 53, and it is exact under two conditions only:
//   - the bitmap is GREY, r = g = b at every texel, because one alpha cannot
//     multiply three channels by three different numbers;
//   - the particle's colour is WHITE at both ends of its lerp, because the
//     colour's RGB multiplies the texel's in the original and has nowhere to go
//     in a black texel.
// All four AM_MODULATE systems the original ships meet both (00_order E6, K12:
// blackexplosion.png and black_portal_particle.png are grey to the last texel,
// and every one of the four is (1, 1, 1) at birth and at death); the layer
// refuses by name a system that does not, rather than drawing a guess.
//
// Whether a system's colour lets the multiply be exact: both ends' RGB white.
// The alpha is not asked - the blend never reads it (DrawColour).
bool MultipliesExactly(const System& system);

// The image a multiply is drawn with, from `texels` RGB texels (3 bytes each, as
// read with its alpha dropped: the blend never reads a texel's alpha either):
// black, at an alpha of 255 less the texel's grey, 4 bytes a texel in `rgba`.
// False, and `rgba` left empty, at the first texel that is not grey.
bool MultiplyTexels(const unsigned char* rgb, std::size_t texels, std::vector<unsigned char>& rgba);

// What ETHEntity::Scale(s) does to each system its entity carries (ETHEntity.cpp:591-599,
// through ETHSpriteEntity.cpp:512-518 and ETHParticleManager.cpp:444-451):
// ETHParticleSystem::Scale (ETHParticleSystem.cpp:27-40) multiplies every length -
// gravity, direction and its spread, the start point and its spread, the size, its
// spread, its growth and its bounds - and no time, angle or colour. (Its bounding
// sphere too, which the port does not read.) The manager also multiplies each live
// particle's size and velocity (PARTICLE::Scale, ETHParticleManager.h:160-164).
//
// A pool MADE from the scaled system is that pool: Reset's spreads are drawn over
// ranges the scale multiplied, from the same stream, and a first release only
// positions. What the original's order adds is its first frame, moved at the old
// scale before the callback runs (ETHActiveEntityHandler.cpp:157-162): the only
// placement a script scales is portal_static, whose start point, spreads and direction
// are all 0, and for those the two are the same frame. A placement's own <Scale> is no
// such case: the engine MAKES its managers at the mean of it (ETHSpriteEntity.cpp:316-319,
// ETHParticleManager.cpp:90), and the layer folds it into the same one call (step 93).
void Scale(System& system, double scale);

// The same, on a copy: what a spawned entity's system is made from. AddEntity's
// entity carries <Scale> x the script's scale x the engine's global scale (1 in
// this game, 00_order 8.0 rule 5; ETHScriptWrapper.Scene.cpp:146-148,
// ETHSpriteEntity.cpp:56-65), and each of its systems is created at the mean of
// that scale's two axes (ETHSpriteEntity.cpp:316-319, ETHParticleManager.cpp:
// 70-90) - explosion_large.ent's 1.4 at a script scale of 1.
System Scaled(const System& system, double scale);

// ---- a particle, moved as ETHParticleManager moves it -------------------------

// Where the entity that carries a system stands this frame: GetPosition() and
// GetAngle() (ETHSpriteEntity.cpp:576). The POSITION, not the centre of its
// picture: a pivot adjust moves the picture and never the emitter.
struct Owner {
    glm::dvec2 atPx{0.0};  // the original's pixels, +y down
    double angleDeg = 0.0; // Ethanon's angle: positive turns counter-clockwise on the screen
};

// ETHParticleManager's PARTICLE, in the port's units.
struct Particle {
    glm::dvec2 atPx{0.0};
    glm::dvec2 velocityPx{0.0}; // `dir`, per frame-speed unit
    glm::dvec2 bornPx{0.0};     // where its current life began (the suites read it)
    double angleDeg = 0.0;      // Ethanon's angle, counter-clockwise on the screen
    double angleDirDeg = 0.0;
    double size = 0.0;
    double lifeMs = 0.0; // its own, spread from the system's
    double elapsedMs = 0.0;
    int repeats = 0;
    int frame = 0;
    bool released = false;
    glm::dvec4 colour{1.0};
};

// Uniform in [min(from, to), max(from, to)], as gs2d's Randomizer::Float(min, max)
// is (Randomizer.cpp). The layer's own seeded generator stands behind it.
using Random = std::function<double(double from, double to)>;

// A point turned by Ethanon's angle, as Multiply(Vector2, RotateZ(DegreeToRadian(a)))
// turns it (GameMath.h:829-838, 897-905): (x cos + y sin, -x sin + y cos) in the
// original's +y-down pixels, so a positive angle turns counter-clockwise on the
// screen.
glm::dvec2 Turn(const glm::dvec2& px, double angleDeg);

// ETHParticleManager::ResetParticle (ETHParticleManager.cpp:473-505): a new
// life - its turn rate, lifetime, size and velocity (direction plus its spread,
// turned by the owner's angle), its colour at birth, its frame - then Release.
// Both draw their random numbers in the original's statement order, x before y,
// so one seed gives one sequence whatever the compiler.
void Reset(const System& system, Particle& particle, const Owner& owner, const Random& random);

// ETHParticleManager::PositionParticle (:507-523): its angle (angleStart, its
// spread, plus the owner's) and its place (startPoint plus its spread, turned by
// the owner's angle, from the owner's position). A particle released for the
// first time takes only this; the rest was drawn when its pool was made.
void Release(const System& system, Particle& particle, const Owner& owner, const Random& random);

// A pool of `count` as CreateParticleSystem makes it (:119-127): each Reset at
// the owner, none released.
std::vector<Particle> MakePool(const System& system, int count, const Owner& owner, const Random& random);

// One frame of one particle, UpdateParticleSystem's loop body (:200-262), with
// `index` its place in a pool of `poolSize`. The elapsed time is the frame's own;
// the motion is capped at 250 ms (:195-196). `killed` is ETHParticleManager::Kill.
// Returns whether it counts as ACTIVE for this frame (:208-213): released and
// bigger than nothing as the last frame left it, before this frame moves it -
// the count a light's brightness and a halo's are the share of.
bool Step(const System& system, Particle& particle, int index, int poolSize, const Owner& owner, double elapsedMs,
          bool killed, const Random& random);

// Whether DrawParticleSystem draws it (:371-378): not spent, released, bigger
// than nothing, its colour's alpha above 0, and not past its life once killed.
bool Drawn(const System& system, const Particle& particle, bool killed);

// Whether a pool is over: ETHParticleManager's m_finished, what AreParticlesOver
// asks of each of an entity's systems (ETHSpriteEntity.cpp:547-567). The update
// loop skips a particle past its last life (:204-206) and marks the pool drawn for
// any other (:215), so a pool is finished once EVERY particle is past its last
// life - which a system of repeat 0 never is, killed or not: a killed particle is
// only not renewed (:259), and "past its lives" asks repeat > 0. So a fireball's
// or a pair of red eyes' pool never ends, and a placed portal's kill needs its own
// clock (ETHCallback_portal's 400 ms), not this.
//
// When. m_finished is stored by an update, from what each particle was as that
// update reached it, and a temporary entity is removed at the START of the frame
// after (ETHActiveEntityHandler.cpp:123, 169-171): so the original deletes it on
// the second frame after the one that spent its last particle. Asked of the pool
// before a frame's Step, this answers true a frame earlier - a frame on which
// neither draws anything, since Drawn refuses a spent particle by the same test.
// An empty pool of a repeating system is over, as a loop over nothing marks
// nothing drawn. The repeat test comes first, so an empty pool of repeat 0 reads
// NOT over where the engine's !anythingDrawn would read over; no pool can be
// empty, since Parse refuses a count below 1 and CreateParticleSystem one of
// nParticles <= 0.
bool Finished(const System& system, const std::vector<Particle>& pool);

// The colour it is drawn in (:381-389). An alpha-blended particle is multiplied
// by min(1, luminance + ambient); an added one is not. And AN ADDED ONE'S ALPHA
// IS 1: AM_ADD blends GL_ONE, GL_ONE, so the original never reads the alpha its
// colour lerps (a crystal's sparkle goes 1 -> 0) - the port's added pipeline
// weights by alpha, and would fade it twice.
//
// A MULTIPLIED ONE IS (0, 0, 0, 1), for the same reason taken one step further
// (00_order K12). Its multiply image's alpha IS the weight, so the colour's alpha
// must not scale it again: blood.ent lerps its black burst 0.2 -> 1, which the
// original's GL_ZERO, GL_SRC_COLOR never reads, and a 0.2 at birth would draw the
// darkest texel at a fifth of its strength. Its RGB goes to 0 with it, which the
// black texel makes no difference to and which keeps anything else from adding.
// Whether it is drawn at all is still the lerped alpha's (Drawn): black_fade.ent
// starts at alpha 0, and the original skips it on that frame (:375).
glm::dvec4 DrawColour(const System& system, const Particle& particle, const glm::dvec3& ambient);

// The quad it is drawn on: SQUARE, size by size, whatever the cell's shape
// (DrawOptimal(pos, colour, angle, Vector2(size, size)), :413). The cell is
// stretched to it.
glm::dvec2 QuadPx(const Particle& particle);

// Its angle as the engine's rotation about +z: Ethanon's positive angle turns
// counter-clockwise on the screen, the converter writes a node's as the negated
// Godot rotation (tscn.py:433), and Units::ToWorldRotation takes a Godot one.
float WorldRotation(const Particle& particle);

// Where system `slot` of an entity's `systems` draws, as a fraction of one
// sprite slot in front of the entity's own picture. Ethanon adds an entity's
// pieces to one map keyed by depth, sprite, halo, then each system in order
// (ETHEntityRenderingManager.cpp:74, 85, 106), so at equal depth they draw in
// that order and before the next entity. The halo takes a quarter (step 49), and
// the half is where the layer puts what stands between two slots (the beholder,
// a thrown stone, a light with no owner): the systems share what lies between,
// evenly, so none of them ties with either.
double SlotFraction(int slot, int systems);

} // namespace MagicPortals::Particles
