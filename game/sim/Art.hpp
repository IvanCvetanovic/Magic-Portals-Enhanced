#pragma once

// What the port draws for what no level pictures: the port's art.json.
//
// The levels picture everything placed in them (Sprites.hpp). What the game
// itself adds - the portals a shot opens, the shot, the player, chapter 1's
// boss and its spikes - the original draws with its own entities, and art.json
// says which image each of those .ent files names, whether it is added, and how
// its sheet is cut. Those are the .ent's facts, and test_mp_sprites pins them.
// How fast a sheet plays is mostly not decoded, so it is carried as a _guess and
// pinned by nothing - except the player's walk, whose stride and standing
// frame are decoded from its script (Character), and the shot's cells, stepped
// by the one timer its script keeps (Shot).
//
// Each also carries its .ent's <EmissiveColor>: the original draws every sprite
// at min(1, ambient + emissive) of its colour (Lighting::AmbientTerm), what no
// level places as much as what it does, so the layer needs it for these too.
//
// And its lighting facts (step 49): static and applyLight, which say which 2D
// lights reach it (Lighting::ReceiverMask), its <Normal> when it names one, and
// its <Light> when it has one - the shot's is the port's one live light. Each is
// the .ent's and required where a default would be a guess: static and
// applyLight on every picture, and a light's every number.
//
// The images are the original's, read from the extracted APK beside the port
// (MAGICPORTALS_ORIGINAL_DIR) and never committed.

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "sim/Lighting.hpp"
#include "sim/Particles.hpp"

namespace MagicPortals::Art {

struct Picture {
    std::string sprite;           // an image among the original's entities
    bool additive = false;        // the .ent's blendMode 1
    int columns = 1;              // its SpriteCut
    int rows = 1;
    double framesPerSecond = 0.0; // _guess, for a sheet of more than one frame that plays
    glm::dvec3 emissive{0.0};     // the .ent's <EmissiveColor>, rgb; required, never defaulted
    bool isStatic = false;        // the .ent's static; required
    bool applyLight = false;      // the .ent's applyLight; required
    std::string normal;           // its <Normal>, a file under the original's entities/normalmaps/; empty = none
    // Its <Light>, as the .ent writes it: before any scale. None of these .ent
    // files carries a <Scale> other than 1, but the script can scale what it adds
    // - the shot's Scale(0.8) - and the layer applies that through
    // Lighting::OwnerScaled. `halo` is a file among the original's entities, not
    // yet a path.
    std::optional<Lighting::Light> light;
    // The depth its light's height is measured from, read with a light; and for the
    // player and a minion, the depth its script adds it at, in the world's units
    // (Rules::scaleFactor turns it into the level's).
    double z = 0.0;

    int Frames() const { return columns * rows; }
};

// What MainCharacter::linearMotion does to the player on a no_gravity level,
// every update (art.json character.no_gravity_motion): turns the standing column
// on the character's own FrameTimer, and hovers the picture on its pivot.
struct NoGravityMotion {
    double columnStrideMs = 0.0;       // set(0, 3, 280, true)
    double hoverScreenPx = 0.0;        // cos(angle) times this, added to the pivot's y in screen pixels
    double hoverAtScreenPx = 0.0;      // the screen height the port takes those pixels at
    double hoverRadiansPerSecond = 0.0; // unitsPerSecond's argument
    double hoverStartRadians = 0.0;    // PI + PIb, set on the first update
    double hoverWrapRadians = 0.0;     // PI * 2, taken off once the angle is past it

    // The hover's offset to the pivot's y, in units of a view `viewUnitsTall` tall.
    double HoverUnits(float angle, double viewUnitsTall) const;
};

// When the player draws its arm out (art.json character.push):
// SideScrollerCharacter::detectPushing's ray, and the rows findFinalDirection
// swaps in while it meets something.
struct Push {
    double reachFrameWidthShare = 0.0; // 0.6 of getSize().x, the frame's width
    glm::dvec2 offsetPx{0.0};          // scale(-6) on y: added to the ray's far end, in level units
    int leftRow = 0;                   // the row pushing left draws
    int otherRow = 0;                  // the row pushing any other way draws
};

// The player, as dark_mage.ent draws it and its script frames it: a sheet whose
// rows are directions and whose columns are a walk. The row it shows IS its
// direction, and it starts facing initialDirection - not on the .ent's
// startFrame, which its constructor sets and its first update replaces. The
// column is a FrameTimer over the whole row, stepped a stride at a time only
// on the updates it walks, so it carries across a stop and a turn (art.json) -
// except on a no_gravity level, where it wears another sheet and the same timer
// turns the column it stands on (NoGravityMotion).
struct Character : Picture {
    double pivotXPx = 0.0;     // its PivotAdjust: the point of the image, from its
    double pivotYPx = 0.0;     // centre, that stands on the entity
    int initialDirection = 0;  // GameCharacter's direction when it is made: a row
    int leftRow = 0;           // derived from the decoded DIRECTION enum, not testimony
    int rightRow = 0;
    int idleColumn = 0;        // the column it stands on
    double strideMs = 0.0;     // frameStride: a column's time; framesPerSecond is 1000 / strideMs
    Push push;                 // the arm out against what it walks into
    // The sheet MainCharacter's constructor swaps in on a no_gravity level. Only
    // the image: the cut, the frame, the pivot and the normal map stay dark_mage.ent's.
    std::string noGravitySprite;
    NoGravityMotion noGravity;
};

// The sheet the player wears on a level: the suit where it sets no_gravity.
const std::string& PlayerSheet(const Character& mage, bool noGravity);

// The ETHFramework FrameTimer (FrameTimer.angelscript) a character keeps, from
// its constructor's first and last of 0. Set is FrameTimer::set (bytes
// 23254..23650): the time is added; a changed first or last resets it to first
// with no time; else it steps ONE frame once the time reaches the stride, the
// stride taken off, past last back to first when it repeats and held on last
// with no time when it does not.
struct FrameTimer {
    int first = 0;
    int last = 0;
    int frame = 0;
    double timeMs = 0.0;

    int Set(int firstFrame, int lastFrame, double strideMs, bool repeat, double elapsedMs);
};

// The player's own state on a no_gravity level, as MainCharacter keeps it.
struct NoGravityPlayer {
    FrameTimer timer;
    int idleColumn = 0;   // the column the last update left for the next to stand on
    float angle = 0.0f;   // linearMotionAngle, a float as the entity's custom data is
    bool moved = false;   // linearMotion has run: the angle is set, and the pivot hovers
};

// One update of MainCharacter on a no_gravity level, from updateFrame to
// linearMotion, `elapsedMs` of frame time. Returns the column the update draws:
// the walk's, set(0, 3, stride, true), on a walking update; else the idle column
// the update before left. Then linearMotion turns the angle and sets the idle
// column, set(0, 3, columnStrideMs, true).
int UpdateNoGravity(const Character& mage, NoGravityPlayer& player, bool walking, double elapsedMs);

// The shot, as projectile.ent draws it and addProjectile and ETHCallback_projectile
// run it (art.json's shot). Added with Scale(0.8), which the engine applies to
// its cell, its pivot, its halo and its light; turned every frame so the head of
// the bolt leads along its flight; and its cell stepped by _projectileFrameTimer,
// ONE FrameTimer for the whole game, set(0, last cell, strideMs, true) each frame
// a shot is alive.
struct Shot : Picture {
    double scale = 1.0;         // <Scale> x the script's Scale(): m_properties.scale
    double pivotXPx = 0.0;      // its PivotAdjust, in the .ent's units, before the scale
    double pivotYPx = 0.0;
    double strideMs = 0.0;      // _projectileFrameTimer's stride: a cell's time
    bool turnsToFlight = false; // SetAngle(radianToDegree(getAngle(dir)) - 90), every frame

    // The angle Ethanon is given for a shot flying along `directionPx` (level
    // units, y down), counter-clockwise on the screen in degrees: getAngle is
    // atan2(x, y) wrapped into [0, 2 PI) (GameMath.h:711-715), in floats as the
    // script computes it. 0 when it does not turn.
    float AngleDegrees(const glm::dvec2& directionPx) const;
    // Where its picture's centre is drawn for the entity at `atPx` turned by
    // `angleDegrees`: the pivot, times the scale, turned with the picture and
    // taken off the entity's point (ETHSpriteEntity::ComputeInScreenSpriteCenter,
    // ETHSpriteEntity.cpp:699-709).
    glm::dvec2 CentrePx(const glm::dvec2& atPx, float angleDegrees) const;
};

// How a picture pulses: from one scale to the other and back, each way a
// stride, eased by smoothEnd - the original's bounce().
struct Pulse {
    glm::dvec2 fromScale{1.0};
    glm::dvec2 toScale{1.0};
    double strideMs = 0.0;

    // Its scale `elapsedMs` in.
    glm::dvec2 ScaleAt(double elapsedMs) const;
};

// Chapter 1's boss, as beholder.ent draws it: frame 0 its eye open, frame 1
// shut. Drawn at its frame's size times the pulse of what it is doing; while it
// throws rocks it keeps the last.
struct Beholder : Picture {
    Pulse seeking;
    Pulse hurt;
    Pulse dead;
};

// What it fires, as beholder_spike.ent draws it: the image points down, and it
// is turned to where it flies, standing on its pivot.
struct Spike : Picture {
    double pivotXPx = 0.0;
    double pivotYPx = 0.0;
};

// A static portal as its script redraws it (ETHCallback_portal_static, art.json's
// static_portal). The level pictures the entity; the callback, once, scales it -
// its picture AND its particle systems, which is what ETHEntity::Scale does - and
// tints its picture red or blue by the node's `color`. Its particles are not
// tinted: a particle is drawn in its own colour (ETHParticleManager.cpp:381-389).
struct StaticPortal {
    std::string entity;            // the entity name the callback is named for
    double scale = 1.0;            // ETHEntity::Scale, applied once
    std::string red;               // the `color` value that takes tintRed
    glm::dvec3 tintRed{1.0};       // SetColor when it is
    glm::dvec3 tintOtherwise{1.0}; // and when it is anything else, or absent

    // GetString("color") == red ? tintRed : tintOtherwise. An absent value reads
    // as "" there, which is not red.
    glm::dvec3 TintFor(const std::string& colour) const { return colour == red ? tintRed : tintOtherwise; }
};

// The ring a level places round a no-portal field, as its own callback redraws it
// every frame (art.json's antiportal; ETHCallback_antiportal). The callback is
// named for the entity less its .ent, so both spellings of a placement run it.
// It blinks the picture between two colours - a triangle wave, each leg a stride
// long - and turns it spinDegPerS degrees a second. The picture's size is not
// this callback's: the manager scales it once (placement.json antiportal.manager).
struct Antiportal {
    std::string entity;    // the entity name, without .ent, whose callback it is
    glm::dvec3 from{1.0};  // blinkColor's colorA, at the start of every leg
    glm::dvec3 to{1.0};    // and its colorB, at the end of one
    double strideMs = 0.0; // a leg: the period is two of them
    double spinDegPerS = 0.0; // AddToAngle(unitsPerSecond(this)), counter-clockwise

    // blinkColor's colour `elapsedMs` into the blink: the leg count says which way
    // this leg runs, and the remainder how far along it is. SetColor writes the rgb
    // alone (ETHEntity.cpp:493-498), so the alpha is the caller's to keep.
    glm::dvec3 ColourAt(double elapsedMs) const;
};

// A fire agent as its callback redraws it (art.json's fire_agent;
// ETHCallback_fire_agent #1-7): whatever picture the level saved for it, every
// update sets `sprite`. Both spellings of the entity run the callback.
struct FireAgent {
    std::string entity; // the entity name, without .ent, whose callback it is
    std::string sprite; // the file SetSprite names, found in entities/
};

// A key, a keyhole or a locked door as setKeyColor repaints it before it is first
// drawn (art.json's key_colour): by its `datum`, a colour `sprites` names has its
// picture swapped for its entity name as the level spells it plus that suffix, and
// its colour's rgb set to `colour`; any other keeps its picture and its colour.
struct KeyColour {
    std::vector<std::string> entities; // the callers, by entity name without .ent
    std::string datum;                 // the string custom data it reads
    std::vector<std::pair<std::string, std::string>> sprites; // a colour, and its suffix
    glm::dvec3 colour{1.0};            // SetColor after the swap

    // Whether an entity, without its .ent, runs setKeyColor.
    bool Paints(const std::string& bareEntity) const;
    // The file setKeyColor names for an entity the level spells `entityName` whose
    // datum reads `hue`: GetEntityName() plus the suffix, or "" for a colour it
    // leaves alone.
    std::string SpriteFor(const std::string& entityName, const std::string& hue) const;
};

// blinkColor's and blinkEmissive's triangle (utilEntityEffect.angelscript): `from`
// at 0, `to` a stride later, `from` again a stride after that.
struct Blink {
    glm::dvec3 from{1.0};
    glm::dvec3 to{1.0};
    double strideMs = 0.0;

    glm::dvec3 At(double elapsedMs) const;
};

// How manageNonTeleportable marks a crate no portal carries (art.json's
// non_teleportable): while it is not burned, a crateCallback crate whose `datum`
// is 0 or absent blinks its emissive when its callback passes zeroEmissive (the
// _no_emissive crates) and its colour when it does not. The two are written over
// the node's own, never added to them.
struct NonTeleportable {
    enum class Blinks { Nothing, Colour, Emissive };

    // crateCallback's callers, by entity name without .ent, and the zeroEmissive
    // each passes it.
    std::vector<std::pair<std::string, bool>> callbacks;
    std::string datum; // the uint isTeleportable reads
    Blink colour;      // blinkColor's, SetColor
    Blink emissive;    // blinkEmissive's, SetEmissiveColor

    // What a placement of `bareEntity` blinks, `teleportable` being whether its
    // datum is there and not 0 (GetUInt reads an absent one as 0).
    Blinks For(const std::string& bareEntity, bool teleportable) const;
};

// The dial the original adds behind a timed crystal, as timer.ent draws it and
// its script runs it (art.json's timer; addTimerToCrystal and ETHCallback_timer).
// It is added `zOffset` behind the crystal at alpha `alpha`, and every frame
// follows it. While the time lasts it shows one of `frames` cells by the time
// elapsed, and pulses from pulseFrom to pulseTo and back by bounce(), each way
// a leg of max(pulseMinLegMs, the time left): the pulse quickens as time runs
// out. From the frame the time is up, or from the frame after the one that
// finds the crystal taken, it shrinks and fades by shrinkPerFrame a frame and is
// deleted when its scale, as the original stores it, drops below goneBelowScale
// (Gone). The crystal never fades: it goes at its time.
struct Timer : Picture {
    int frames = 1;                 // the cells the time is divided into
    double alpha = 1.0;             // SetAlpha when it is added
    int zOffset = 0;                // its depth from the crystal's
    double pulseFrom = 1.0;         // bounce()'s two scales
    double pulseTo = 1.0;
    double pulseMinLegMs = 0.0;     // the shortest leg of the pulse
    double shrinkPerFrame = 1.0;    // scale and alpha, a frame, once the time is up
    double decayFramesPerSecond = 0.0; // the frames that shrink is counted in
    double goneBelowScale = 0.0;    // deleted once its stored scale x drops below this
    double screenPxPerUnit = 0.0;   // m_scaleFactor: its stored scale over the port's

    // The cell shown `elapsedMs` into a time of `timeMs`: int(elapsed / time x
    // frames) in floats, as the script computes it, clamped to the cells.
    int FrameAt(double elapsedMs, double timeMs) const;
    // A leg of the pulse at that moment: max(pulseMinLegMs, time - elapsed).
    double LegMs(double elapsedMs, double timeMs) const;
    // Its scale at that moment: bounce() with this moment's leg, whose count of
    // legs so far - the elapsed time over the leg - says which way it goes.
    glm::dvec2 PulseAt(double elapsedMs, double timeMs) const;
    // What its scale and alpha are multiplied by over `seconds` of shrinking.
    double DecayOver(double seconds) const;
    // Whether a dial of `scaleX` in the port's units is deleted: ins 184-199 test
    // the entity's stored scale, which bounce set as the pulse x m_scaleFactor.
    bool Gone(double scaleX) const;
};

// The level's exit door where the level's properties carry `shadowless_door`,
// and the glow the script hangs behind it (art.json's door; Game::readProperties,
// which runs before the level is scaled). The door is drawn on `shadowlessSprite`
// instead of its own, and on a level that also sets `no_gravity` a
// door_highlight.ent is added at the door's own point, at `highlightZ`. All three
// bob (motions.json's `door` row).
struct Door {
    std::string entity;           // the exit door's entity, without .ent
    std::string backgroundEntity; // door_bg.ent, the doorway's black
    std::string shadowlessFlag;   // the level property that swaps the sprite and adds the glow
    std::string shadowlessSprite; // what SetSprite swaps the door's picture for
    Picture highlight;            // door_highlight.ent
    // Its AddEntity depth, IN THE LEVEL'S UNITS and not the world's: it is added
    // before SGlobalScale::scaleEntities, which multiplies its position by the
    // scale factor exactly as it does every node the level file places. So
    // Rules::LevelDepth is not applied to it.
    double highlightZ = 0.0;
};

// The ghost of the wall a lit torch takes away: dashed_door.ent, added by
// seekLightWallToDestroy at the wall's own point and angle and deleted by
// addLightWall when the fire signal is shot (art.json's light_wall_ghost).
// Its own callback does two things a frame and nothing else - an alpha that
// breathes and a pulse that barely moves - so the picture holds still where the
// wall stood, at the wall's size, outlining what is no longer there.
struct LightWallGhost : Picture {
    // seekLightWallToDestroy's vector3(0, 0, -4), added to a position the level's
    // scaling pass has already been through: a RAW depth, so Rules::LevelDepth
    // turns it into the level's units, unlike Door::highlightZ.
    double zOffset = 0.0;
    double alphaBase = 0.0;      // SetAlpha(cos(angle) x alphaAmplitude + alphaBase)
    double alphaAmplitude = 0.0;
    double spinDegPerSec = 0.0;  // AddToFloat('angle', unitsPerSecond(this)), the cos's argument
    Pulse pulse;                 // bounceLightEntity's bounce(1.006, 0.994, 160)

    // Its alpha at `angleDeg` of that datum, in Ethanon's degrees.
    double AlphaAt(double angleDeg) const;
};

// A minion or a knight of worlds 3 and 4, as minion.ent draws it and
// ETHCallback_minion frames it (art.json's minion). Its row is the way it last
// walked; its column is the walk's FrameTimer while it walks and the hold column
// while it does not, and the timer's column and time carry across a hold and a
// turn, as the player's do. It stands with its feet, not its centre, on what the
// port's one-circle body stands on.
struct Minion : Picture {
    double pivotXPx = 0.0;          // its PivotAdjust
    double pivotYPx = 0.0;
    double feetBelowEntityPx = 0.0; // its compound's feet circle's bottom, below the entity
    int firstColumn = 0;            // the walk: timer.set(first, last, stride, true)
    int lastColumn = 0;
    double strideMs = 0.0;
    int holdColumn = 0;             // SetFrame(hold, dirRow) while it does not walk
    int startRow = 0;               // dirRow before its first walk
    int leftRow = 0;                // dirRow walking left
    int rightRow = 0;               // and right
    double searchPeriodMs = 0.0;    // the blocked arm looks right while elapsedTime % period > rightAfter
    double searchRightAfterMs = 0.0;
    bool darkestZeroesEmissive = false; // emissive 0 while the ambient is DARKEST_AMBIENT_LIGHT
    std::string blackEntity;        // the entity drawn black: ghost_minion.ent
    glm::dvec3 blackColour{0.0};    // its SetColor, every frame
    glm::dvec3 blackEmissive{0.0};  // its .ent's <EmissiveColor>
};

// Chapter 3's boss, as ghost.ent draws it (art.json's ghost): a 4 x 1 sheet whose
// cell, colour, alpha and scale ETHCallback_ghost writes (sim/Ghost's Look), drawn
// with its pivot on the ghost's point.
struct Ghost : Picture {
    double pivotXPx = 0.0; // its PivotAdjust
    double pivotYPx = 0.0;
};

// Which of ETHCallback_minion's arms an update took, as far as its frame goes.
enum class MinionArm {
    None,    // no update ran for it: nothing written
    Walk,    // it walked: its row, and the walk's column
    Hold,    // it did not walk, its way clear: the hold column of the row it last walked
    Search,  // its way blocked and nothing spotted: the hold column, looking one way then the other
    Settled, // a lone guard inside minDist x 2 (shouldntMove): nothing written
};

// A minion's frame and what its script keeps for it.
struct MinionPose {
    FrameTimer timer; // the walk's, from the constructor's (0, 0)
    int column = 0;   // the frame shown: frame 0 until a write, the entity's default
    int row = 0;
    int dirRow = -1;  // its custom data's dirRow; -1 until written, which reads as startRow
};

// One update's frame writes. `right` is which way a Walk went (moveVec.x > 0);
// `elapsedMs` the update's time, which only a Walk adds to the timer;
// `sinceArrivalMs` its elapsedTime, which the Search arm reads.
void UpdateMinion(const Minion& art, MinionPose& pose, MinionArm arm, bool right, double elapsedMs,
                  double sinceArrivalMs);

// ---- the four black bars round the scene (art.json black_bar) -------------------
//
// Game::manageSceneBounds (bytes 118788..119489, 00_order 8.6, decode D5 section 2
// and its check) adds black_bar.ent four times, unconditionally, once the level's
// scaling pass has run: two bars just above the scene's top edge and two just below
// its `max` marker's y, the second of each pair half a bar further right, so that
// together they cover x 0 to 768 of the level's units. The entity is layerable at
// depth 1.0, in front of everything the scene draws. The camera never shows past
// the scene at rest, so the bars are seen only while the earthquake has moved the
// drawn camera past its clamp (decode D11 section 2, measured on the footage).
enum class BarSide {
    AboveTop,    // y = -half the bar's height: the band above y 0
    BelowBottom, // y = the max marker's y + half the bar's height: the band below it
};

struct BlackBar {
    int xHalfWidths = 0;        // its centre's x in half-widths of the script's bar: 1 or 2
    BarSide side = BarSide::AboveTop;
};

struct BlackBars {
    std::string entity;         // black_bar.ent, read at run time by LoadEnt
    // The script's own vector2(512, 32), which g_scale.scale takes to the level's
    // units and every centre is measured in: not the picture's size, which is the
    // image's (the two agree, and each is pinned where it is read).
    glm::dvec2 sizeUnits{0.0};
    std::vector<BlackBar> bars; // in the order manageSceneBounds adds them: A, B, C, D

    // Each bar's centre, in the level's units, for a scene whose max marker stands
    // at `sceneMax` (manageSceneBounds' sceneMax; sceneMin is V2_ZERO and never read).
    std::vector<glm::dvec2> Centres(const glm::dvec2& sceneMax) const;
};

// ---- the ring a shock agent adds round itself (art.json shock_ring) ------------
//
// ETHCallback_shock_agent adds shock_area.ent at its own point once, at the absolute
// depth g_scale.scale(-10), sizes it to the lethal diameter and colours it; the ring's
// own callback, ETHCallback_shock_area, then turns it by game time and sets its scale
// every frame to its fixed base times a fresh random factor (decode D6 and its check).
// The entity's picture, blend, emissive and lighting are its .ent's, read at run time
// by LoadEnt; what is here is what the two scripts do to it.
struct ShockRing {
    std::string entity;       // shock_area.ent
    glm::dvec3 colour{1.0};   // SetColor(vector3(0.8, 1, 1) x 0.5): its rgb, alpha kept at 1
    double z = 0.0;           // AddEntity's depth, as the script writes it
    bool zScaled = false;     // g_scale.scale()'d (the level's units as is) or a bare literal
    double spinDegPerS = 0.0; // AddToAngle(unitsPerSecond(this)): counter-clockwise, game time
    double jitterFrom = 1.0;  // SetScale(base x (from + randF(span))), every frame
    double jitterSpan = 0.0;
    // And its AGENT's own pulse, the shock_agent's sprite and not this ring's:
    // bounce(this, V2_ONE, vector2(0.95, 0.95), 300) on every update (art.json
    // shock_ring agent_pulse, planning step 146), on the level's age.
    Pulse agentPulse;

    // Its depth in the level's units: a scaled depth as it stands, a bare literal
    // over the scale factor (step 86's rule, Rules::LevelDepth).
    double LevelZ(double scaleFactor) const { return zScaled ? z : z / scaleFactor; }
    // scaleToSize's size for a ring of `radiusUnits`: the diameter, both ways.
    double SizeUnits(double radiusUnits) const { return radiusUnits * 2.0; }
    // The factor one frame's draw gives: `drawn` is randF(span), in [0, span].
    double JitterAt(double drawn) const { return jitterFrom + drawn; }
};

// A carried diamond's SetAlpha(0.1f), shock or fire, on every update it is
// carried: the sprite's alpha alone (art.json carried_diamond, planning step 146).
struct CarriedDiamond {
    double alpha = 1.0;
};

// light_off.ent's AddToAngle(unitsPerSecond(190)) while it stands unlit: its
// systems turn, its vertical picture does not (art.json unlit_torch, step 146).
struct UnlitTorch {
    double spinDegPerS = 0.0;
};

struct Rules {
    Picture portal;      // portal.ent
    Shot shot;           // projectile.ent
    // (light_from_projectile.ent, the lit torch, was a picture here until the visuals
    // plan's 11.2: it is now added whole, from its own .ent, by the effects runtime -
    // effects.json light_from_projectile.)
    Character character; // dark_mage.ent, the player
    Beholder beholder;   // beholder.ent, chapter 1's boss
    Spike spike;         // beholder_spike.ent, its spikes
    // portal_static: placed by the levels, redrawn by its script
    StaticPortal staticPortal;
    // antiportal: placed by the levels, blinked and turned by its script
    Antiportal antiportal;
    // fire_agent, key/keyhole/door_locked and the crates: placed by the levels,
    // their pictures swapped or blinked by their scripts
    FireAgent fireAgent;
    KeyColour keyColour;
    NonTeleportable nonTeleportable;
    Timer timer;         // timer.ent, the dial behind a timed crystal
    Minion minion;       // minion.ent, the minions and knights
    Ghost ghost;         // ghost.ent, chapter 3's boss
    Door door;           // the exit door's second sheet, and door_highlight.ent behind it
    // dashed_door.ent, which a lit torch leaves where its light wall stood
    LightWallGhost lightWallGhost;
    // black_bar.ent, four times round the scene
    BlackBars blackBars;
    // shock_area.ent, the ring each shock agent adds round itself
    ShockRing shockRing;
    // what a carried diamond's and an unlit torch's own callbacks do to them
    CarriedDiamond carriedDiamond;
    UnlitTorch unlitTorch;
    // What a depth a script adds something at is in the level's units: that depth
    // over this (art.json depth). A level's nodes were multiplied by it at load and
    // what a script adds at a literal z was not.
    double scaleFactor = 0.0;

    // A script's raw depth in the level's units, where the port orders its art.
    double LevelDepth(double worldZ) const { return worldZ / scaleFactor; }
};

bool LoadRules(const std::string& path, Rules& out, std::string& error);

// ETHEntityProperties::ENTITY_TYPE (ETHEntityProperties.h:82-89), the entity's
// `type`: how the engine orders and turns it. Every value the engine names, the
// three deprecated ones too, so a file is read for what it says.
enum class EntityType {
    Horizontal = 0,  // ET_HORIZONTAL: ordered by its z (ComputeDepth)
    GroundDecal = 1, // ET_GROUND_DECAL, deprecated: its z and a hair
    Vertical = 2,    // ET_VERTICAL: its z, never turned, its particles INDIVIDUAL_OFFSET
    Overall = 3,     // ET_OVERALL, deprecated: always depth 1
    OpaqueDecal = 4, // ET_OPAQUE_DECAL, deprecated: its z and a hair
    Layerable = 5,   // ET_LAYERABLE: its layerDepth, whatever its z
};

// An entity the script ADDS - an effect, spawned by name with AddEntity - read
// out of its .ent when it is needed, not copied into art.json (Effects.hpp says
// where each call adds it). art.json pins the handful the port draws for good;
// the script adds some fifty more, and hand copies of fifty files would drift.
//
// Read as ETHEntityProperties::ReadFromXMLFile reads it (ETHEntityProperties.cpp:
// 226-413): the first element inside the file's root is the entity, and each of
// its facts is the first child of that name, or the engine's own default where
// the file does not state it - no <Scale> is (1, 1), no <Sprite> is none, no
// <EmissiveColor> is black, no blendMode is AM_PIXEL (ETHEntityProperties.cpp:
// 79-109, 166-180), and a <Light> takes ETHLight's (ETHLight.cpp:27-38). Those
// defaults are the engine's reading of the same file, not a guess of the port's:
// portal.ent states no <Scale> and fireball.ent no <Sprite>. What IS refused is
// what the engine would misread in silence: a file that is not the original's
// UTF-16, a stated number that is not one, a cut below one, a system missing a
// field (Particles::Load), or a third system, which Ethanon would drop
// (ETH_MAX_PARTICLE_SYS_PER_ENTITY is 2, ETHTypes.h:33).
//
// The Picture's rows mean what they mean in art.json - its framesPerSecond and
// z are not the .ent's and stay 0: the call site gives the depth, and a sheet's
// rate is the script's - and its light is kept only when the engine keeps it,
// active with a range above 0 (ETHEntityProperties.cpp:343), with no halo when
// it names no <HaloBitmap> (ETHEntityRenderingManager.cpp:78). The light, the
// pivot and the systems are as the file states them, before any scale.
struct Entity : Picture {
    int blendMode = 0;            // Video::ALPHA_MODE; `additive` is blendMode 1
    glm::dvec2 pivotPx{0.0};      // its <PivotAdjust>
    glm::dvec2 scale{1.0};        // its <Scale>, per axis
    std::vector<Particles::System> systems; // in the file's order, unscaled
    // Its `type` and `layerDepth` (ETHEntityProperties.cpp:228, :237), each the
    // engine's Reset value where the file states none: ET_HORIZONTAL and 0
    // (:176-177). The engine stores any whole number as the type and switches on it
    // with no default (ETHSpriteEntity.cpp:611-632), so one outside the enum is
    // refused rather than misread in silence.
    EntityType type = EntityType::Horizontal;
    double layerDepth = 0.0;

    // A layerable entity's depth among the scene's, 0 the back and 1 the front of
    // the normalised range: Max(m_layrableMinimumDepth, layerDepth), the minimum
    // 0.001 (ETHSpriteEntity.cpp:34, :626-628). Its z plays no part in it.
    double LayerableDepth() const;

    // ETHEntity::IsTemporary (ETHEntity.cpp:697-716): no sprite, at least one
    // system, and every system repeats a set number of times. Such an entity is
    // deleted by the engine once its particles are over (Particles::Finished,
    // ETHActiveEntityHandler.cpp:169-171); any other is its script's to delete.
    bool Temporary() const;
    // m_properties.scale once AddEntity has made it: <Scale> x the script's
    // scale x the engine's global scale, which is 1 in this game
    // (ETHScriptWrapper.Scene.cpp:146-148, ETHSpriteEntity.cpp:56-65). Its
    // picture, pivot, halo and light take this (Lighting::OwnerScaled).
    glm::dvec2 ScaleAt(double scriptScale) const { return scale * scriptScale; }
    // And the one number each of its systems is made at: the mean of that
    // scale's two axes (ETHSpriteEntity.cpp:316-319; Particles::Scaled).
    double ParticleScale(double scriptScale) const;
};

// Reads the .ent at `path`. False, with `error`, for anything refused above;
// `out` is then unchanged.
bool LoadEnt(const std::string& path, Entity& out, std::string& error);

} // namespace MagicPortals::Art
