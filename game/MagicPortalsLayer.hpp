#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <entt/entt.hpp>
#include <glm/glm.hpp>

#include "core/Components.hpp"
#include "core/EngineLayer.hpp"

namespace Supersonic {
class ScreenOverlay; // core/ScreenOverlay.hpp: the popups' pieces are sent to it (emitPopup)
}

#include <random>

#include "core/AudioEngine.hpp"
#include "core/BitmapFont.hpp"

#include "sim/Achievements.hpp"
#include "sim/Camera.hpp"
#include "sim/Chapters.hpp"
#include "sim/Credits.hpp"
#include "sim/Cutscene.hpp"
#include "sim/Dashboard.hpp"
#include "sim/Earthquake.hpp"
#include "sim/Effects.hpp"
#include "sim/Game.hpp"
#include "sim/Art.hpp"
#include "sim/Hint.hpp"
#include "sim/Hud.hpp"
#include "sim/Impacts.hpp"
#include "sim/LevelEnd.hpp"
#include "sim/Lighting.hpp"
#include "sim/Loading.hpp"
#include "sim/Locking.hpp"
#include "sim/MainMenu.hpp"
#include "sim/MenuState.hpp"
#include "sim/Motion.hpp"
#include "sim/Notificator.hpp"
#include "sim/Particles.hpp"
#include "sim/Pause.hpp"
#include "sim/Pictures.hpp"
#include "sim/Popup.hpp"
#include "sim/Scores.hpp"
#include "sim/Selector.hpp"
#include "sim/Sky.hpp"
#include "sim/Sounds.hpp"
#include "sim/Sprites.hpp"
#include "sim/Switches.hpp"
#include "sim/Tiers.hpp"

namespace MagicPortals {

// Magic Portals as the port plays it: the levels in the original's order, drawn
// with the levels' own art.
//
// Deliberately plain, like HUSK's first view. What it has to get right is the
// seam:
//
//  - The level is the layer's (Game::Level), and it runs on the port's 60 Hz
//    tick, set on the engine's clock. The app steps physics before each
//    OnFixedUpdate, so the layer runs Game::AfterStep then Game::BeforeStep.
//    That is the suites' tick, with the app's step between the halves.
//  - Input is read on the tick. Left and right held are the Godot project's two-button
//    pad, and a tap fires a portal shot at the point in the level under it. There
//    is one tap per tick at most, so a frame cannot spend the budget twice in one
//    step. R retries and N skips, on the tick as well.
//  - The levels come in chapters.json's order. Reaching the exit loads the next
//    level of the world at once, as the Godot project advances (main.gd:161-168). Retry
//    is instant: the level is rebuilt from what was read when it loaded, and the
//    disk is not touched (level_manager.gd:5-13). A level the port refuses says
//    why, and N skips it, the Godot project's own development shortcut. At a world's end
//    the Godot project opens its menu; the port has none yet, so it says the chapter is
//    complete.
//  - The camera is orthographic and follows the player: Camera::Follow, moved on
//    the tick and drawn between ticks. It shows view.json's height of the level,
//    256 px, at the window's shape. And a finger drags it, as the original's does
//    (camera.json `drag`, step 138): the pointer held off the walk pads pans it at
//    twice its move, holds it 4000 ms after a drag away, and stops the follow while
//    it is held. camera.json's drag_pan_enabled false restores the owner's
//    2026-09-11 camera, which only follows.
//  - Every sprite the level names (Sprites.hpp) is a textured quad, unlit and
//    blended as the level says, in Godot's canvas order. A sprite follows its
//    node's body, or the crystal, static portal, no-portal zone, key, keyhole or
//    diamond it pictures, and goes when that goes; its entity's particle systems
//    follow and go with it (sim/Particles.hpp). The sky follows none of those:
//    the original pins it to the camera every frame (sim/Sky.hpp), and so does
//    the port. The bodies are also boxes in the colour of what they are, hidden
//    behind the art until B shows them - and shown anyway when the art cannot be
//    read, which is a level still played, drawn plainly.
//  - The scene holds display values, as the original's 8-bit framebuffer did
//    (RenderSettings::SceneEncoding::DisplayEncoded, set at attach): a texture
//    is sampled as the bytes in its file, tints and blends work on those bytes,
//    and nothing is tone-mapped or bloomed, on a flat black ground.
//  - Every sprite of a level is its colour times min(1, ambient + emissive), as
//    the original draws it (Lighting::AmbientTerm): the ambient the level file
//    gives, or lighting.json's where the script replaces it - a `darkest` level,
//    a lit torch - and the emissive its node or its .ent gives. The layer keeps
//    each sprite's own colour (a crystal's fade, the beholder's red) apart from
//    that factor, and hands both to the engine's 2D sprite path, premultiplied.
//    A static sprite that applies light adds its baked lightmap over that.
//  - A level's <Light>s are the engine's 2D point lights, and their halos added
//    quads, following their owners; the shot carries a light of its own. A
//    sprite that applies light takes, per pixel through its normal map, every
//    light that is not static when it is static itself - its static lights are
//    in its lightmap - and every light when it is not, which is how the player
//    is lit by a torch it walks past.
//  - What the levels do not picture, the game draws as the original's own
//    entities draw it (Art.hpp): the portals a shot opens, and the shot. Their
//    images are read from the original's extracted assets; without them they
//    are boxes. So is the player: dark_mage.ent's sheet, turned as it walks. So
//    is chapter 1's boss: beholder.ent, its eye open or shut, reddening as it
//    is hurt and pulsing as it goes, with the spikes it fires and the rocks it
//    drops.
class MagicPortalsLayer final : public Supersonic::EngineLayer {
public:
    // The engine's tick: 60 Hz, the physics step, and the layer clock's step (sys:GetTime). The SCRIPTS
    // see the floor of it, 16 of its 16.667 ms (sim/ScriptClock, game/data/clock.json): TickStep().
    static constexpr float kTick = 1.0f / 60.0f;

    // The actions this layer binds at attach, named so tests and a recording
    // can drive them without knowing which buttons they sit on.
    static constexpr const char* kLeft = "mp.left";          // Left arrow
    static constexpr const char* kLeftAlt = "mp.left.alt";   // A
    static constexpr const char* kRight = "mp.right";        // Right arrow
    static constexpr const char* kRightAlt = "mp.right.alt"; // D
    static constexpr const char* kTap = "mp.tap";            // left mouse: fire a portal shot
    static constexpr const char* kRetry = "mp.retry";        // R
    static constexpr const char* kSkip = "mp.skip";          // N
    static constexpr const char* kBoxes = "mp.boxes";        // B: the bodies' boxes, over the art
    // Escape: the pause over a level being played and back out of it, as the
    // original's back key (GameState::handleBackButton); up a screen in the menu.
    static constexpr const char* kBack = "mp.back";
    // G: write down what the game believes it is drawing, right now.
    //
    // A diagnostic rather than a control, and it exists because a sprite that
    // vanishes on screen has four possible explanations that look identical to
    // a player and to a screenshot: the layer took its quad away, the renderer
    // culled it, a pass refused the draw, or it was drawn and produced nothing
    // visible. This says which.
    static constexpr const char* kDump = "mp.dump";

    // Where the port reads from. The defaults are where the build was told
    // Enhanced's files are, and the port's own data beside its source.
    struct Paths {
        std::string levels = MAGICPORTALS_LEVELS_DIR;
        std::string chapters = MAGICPORTALS_CHAPTERS_FILE;
        // The original's achievements, extracted beside chapters.json: never in this
        // repository (sim/Achievements.hpp). The dashboard draws no rows without it.
        std::string achievements = MAGICPORTALS_ACHIEVEMENTS_FILE;
        std::string data = MAGICPORTALS_DATA_DIR;
        std::string portData = MAGICPORTALS_PORT_DATA_DIR;
        // Where LevelBuilder may write, and the multiplies AM_MODULATE is drawn
        // with (modulateImage). Nothing is written when it is empty.
        std::filesystem::path prisms;
        // What res:// stands for: the converter writes the levels' art beside
        // the levels, so the directory above them (Sprites.hpp).
        std::string art = MAGICPORTALS_LEVELS_DIR "/..";
        // The original's extracted assets, whose entities/ holds the images of
        // what no level places (Art.hpp).
        std::string original = MAGICPORTALS_ORIGINAL_DIR;

        // WHERE THE PLAYER'S MEDALS ARE KEPT, and empty by default - which
        // means this layer keeps no save on the filesystem.
        //
        // Injected rather than resolved here, which is WolfBrigadeLayer's
        // contract and exists for the reason its header states: "where may I
        // write" is a question about the machine, not about this game, and a
        // layer that resolved its own path could not be built by a test
        // without one. Every suite builds the layer bare, and a hardcoded path
        // would have eighteen of them writing the same filenames into the
        // ctest working directory and reading each other's - order-dependent
        // failure that reads as a flake.
        //
        // Only the shipped binary passes one, from UserDataDirectory. The sound
        // and music switches are kept there too (audio.json, sim/Switches.hpp).
        std::string saveDir;

        // A --fixed-step run: its frames are the tick's own, so nothing about the
        // display is measured and every picture is drawn at its tick's place whatever
        // pictures.json says (planning step 150). Set by main from the engine's flag.
        bool fixedStep = false;
    };

    // The last level cleared: the portals spent against its golden score, which
    // is what the Godot project records (level_manager.gd:116-121).
    struct Cleared {
        std::string name;
        std::string label;
        int portalsUsed = 0;
        int goldenScore = 0;
        int traversals = 0;
        int crystals = 0;
        int crystalsTotal = 0;

        bool Gold() const { return goldenScore >= 0 && portalsUsed <= goldenScore; }
    };

    // Which screen is up. `None` is a level, played exactly as before: a layer
    // built with a level's name never sees the menu at all, which is how every
    // suite, every headless render and --level enter the game.
    //
    // The original's own menu STATES: LoadingScreen, PortalMainMenu,
    // WorldSelector, LevelSelector, CreditsScreen and ScoreDashboard. Each opens
    // under its own black (menu_state.fade) and presses its buttons as
    // Button::update does (menu_state.press). The loading screen and the main menu
    // are Enhanced's ui3 spec section 2 (sim/Loading.hpp, sim/MainMenu.hpp),
    // credits and the achievements dashboard its sections 3 and 4
    // (sim/Credits.hpp, sim/Dashboard.hpp), all drawn by EmitMenu; chapter select
    // and the grid are still the port's own layout of the original's art, which
    // ui3 sections 5 and 6 are to replace.
    // `Finished` and `Dead` are the odd two: each sits OVER the level it ended,
    // which stays loaded, drawn AND RUNNING - neither screen stops game time
    // (Enhanced's ui2 spec, D7) - and both are drawn through the screen
    // overlay on the view, as the pause is (sim/LevelEnd.hpp). Everything that
    // treats a screen as "the level is gone" has to name both.
    enum class Screen { None, Loading, Main, Worlds, Levels, Credits, Achievements, Finished, Dead };

    // A button the menu drew, in the menu's pixel box (MenuBoxPx). Kept as
    // data so a click is tested against exactly what was drawn, and so a test
    // can press one without a pointer or a camera.
    struct MenuButton {
        enum class Kind { Play, World, Level, Back, Forward, Retry, Next, List };
        Kind kind{Kind::Play};
        glm::dvec2 centrePx{0.0};
        glm::dvec2 sizePx{0.0};
        int world{0};  // World and Level
        int level{-1}; // Level: its place in chapters.levels
    };

    MagicPortalsLayer(Paths paths, std::string startLevel);

    const char* Name() const override { return "Magic Portals"; }

    void OnAttach(entt::registry& registry) override;
    void OnDetach(entt::registry& registry) override;
    void OnFixedUpdate(entt::registry& registry, float fixedDelta) override;
    void OnUpdate(entt::registry& registry, float deltaTime) override;

    // Why the level being shown did not load, or why nothing could start; empty
    // when it did. A level that fails leaves the layer saying so, rather than
    // throwing out of OnAttach.
    const std::string& LoadError() const { return m_loadError; }

    // Why the level's art could not be drawn, when it could not - the level is
    // then drawn as boxes - or empty.
    const std::string& ArtError() const { return m_artError; }

    // Why the level's lighting could not be read (Lighting::Read), when it could
    // not - its sprites are then drawn in their own colours, unlit, as before
    // lighting existed - or empty.
    const std::string& LightingError() const { return m_lightingError; }

    // The ambient light the last tick coloured the level's sprites with
    // (Lighting::Ambient). (1, 1, 1) when there is no level or its lighting did
    // not read, which multiplies nothing.
    glm::dvec3 AmbientNow() const { return m_ambient; }

    // The lightmap files of the level being drawn, which the layer hands back to
    // the engine's texture registry when the level goes (TextureRegistry::
    // Invalidate). Kept across a retry, which draws the same level again, and
    // given back by anything else that unloads it: the next level, the menu,
    // detaching. Each is the overlay of the sprite it was baked for (since step
    // 47), so giving them back gives back a texture and a material set apiece.
    const std::vector<std::string>& HeldLightmaps() const { return m_heldLightmaps; }
    // How many lightmap paths the layer has handed back over its life, whether or
    // not a registry was there to take them. For the suites, which have none to
    // watch: it is how a retry is seen to keep them.
    std::size_t LightmapsHandedBack() const { return m_lightmapsHandedBack; }

    // Whether B has the bodies' boxes shown over the art.
    bool ShowingBoxes() const { return m_showBoxes; }

    // DEV ONLY: every sprite's 2D light mask forced to 0, so no light reaches any
    // sprite while the lights, the halos and everything else are drawn as before.
    // It exists for one measurement: a capture with the lights and the same capture
    // without them differ by exactly what the lights add (step 49's torch pass).
    // Takes effect on the next tick's colouring.
    void ForceLightMasksOff(bool off) { m_lightMasksOff = off; }
    bool LightMasksForcedOff() const { return m_lightMasksOff; }

    // The level being played, or null when it did not load.
    const Game::Level* SimLevel() const { return m_loaded ? &m_level : nullptr; }

    // The entry in chapters.json being shown, or null when nothing could start.
    const Chapters::Level* Current() const;

    // The menu, when one is up.
    Screen MenuScreen() const { return m_screen; }
    const std::vector<MenuButton>& MenuButtons() const { return m_menuButtons; }
    // The box the menu is laid out in: view.json's height at the window's
    // shape, with (0, 0) at its top-left corner, so the original's normalized
    // positions multiply straight into it.
    glm::dvec2 MenuBoxPx() const;
    // Press a button, as a click on it does. False when it led nowhere.
    //
    // BY VALUE, and that is not a style choice: pressing one lays the screen
    // out again, which clears the very vector the caller's button lives in -
    // the click loop below iterates that vector - so a reference would dangle
    // before the level it names could be read out of it.
    bool PressMenu(entt::registry& registry, MenuButton button);

    // The sound events latched and not yet played, oldest first.
    //
    // For the suites, and it is the only way to test this half at all: a sound
    // is played on the FRAME, so a run with no audio device - which is every
    // suite, every headless render, and Linux - plays nothing and would
    // otherwise leave nothing to assert. This is what OnUpdate would play.
    std::vector<std::string> LatchedSounds() const;
    // And the volume each was latched with, in the same order: the caller's, for
    // a hook that takes one (a landing's hitIntensity / 18), or -1.
    std::vector<double> LatchedVolumes() const;
    // The crates whose begin-contacts the impact watch reads for their sound.
    const std::vector<entt::entity>& WatchedCrates() const { return m_crates; }

    // Why there is no sound, when there is none.
    const std::string& SoundsError() const { return m_soundsError; }

    // How many times the player has died this session. Each death is a retry.
    int Deaths() const { return m_deaths; }
    const std::optional<Cleared>& LastCleared() const { return m_lastCleared; }

    // The camera as the last tick left it, and what it shows, in the level's
    // pixels; and the level's extent.
    glm::dvec2 CameraCentrePx() const { return m_follow.centrePx; }
    glm::dvec2 ViewPx() const;
    glm::dvec2 BoundsPx() const { return m_boundsPx; }

    // The level's camera-pinned sky (sim/Sky.hpp), as the level's art built it:
    // not running on a space level, or where the art was not read.
    const Sky::Controller& SkyController() const { return m_sky; }
    // And the space levels' (sim/Sky.hpp SpaceSky): not running off them.
    const Sky::Space& SpaceController() const { return m_space; }
    const Sky::SpaceRules& SpaceSkyRules() const { return m_spaceRules; }

    // THE EARTHQUAKE (sim/Earthquake.hpp, earthquake.json): the original's one
    // camera shake. The generator as the last tick left it, and the move it gave
    // the DRAWN camera on that tick, in the level's units along y, down positive:
    // 0 on a tick with no tremble. Only the drawn camera moves - OnUpdate adds it
    // after the engine has drawn the camera between ticks - so the camera a tick
    // reads, and every tap mapped through it, stays at rest (D11 section 3).
    const Earthquake::Generator& Quake() const { return m_quake; }
    double QuakeOffsetUnits() const { return m_quakeOffsetUnits; }
    // The move the next frame draws: the last tremble until one frame has drawn
    // it, however many ticks came after it without one; then the last tick's.
    double QuakeFrameUnits() const { return m_quakeFrameUnits; }
    // The pairs that began to touch in the last step, as the impact rows read
    // them (sim/Impacts.hpp): every begin of the player, a stone and a minion,
    // whether or not it called.
    const std::vector<Impacts::Begin>& ImpactBegins() const { return m_impactBegins; }
    // The entity name of the node a body was built from; empty for a body no
    // node places (the player, a thrown stone, a rock, a minion).
    std::string EntityNameOf(entt::entity body) const;
    // A startEarthquake call, landed on its row's side of this tick's update
    // (Earthquake::When). Every wired event reaches the generator through here
    // (latchSimQuakes, latchDoors); public so that a suite can shake a level
    // with no event to hand. `scale` multiplies the row's intensity: a wall's
    // destroyEffectScale.
    void QueueQuake(Earthquake::Call call, double scale = 1.0);

    // What each particle system is doing, for the suites, in the order they were
    // built: whom it follows, where it draws, and the place its newest life began.
    struct EmitterReport {
        std::string node;   // its owner's node; empty for one no level sprite carries
        std::string bitmap; // Particles::System::bitmap
        std::string image;  // the file drawn for it: its tier file where one exists (drawnImage)
        double systemSize = 0.0; // Particles::System::size as built: its entity's scale applied
        // One cell of its bitmap in units as its quads sample it, which a quad
        // stretches square: split evenly, or where the units do not divide by the
        // cut, the whole-unit frame SetupSpriteRects cuts (Emitter::wholeUnitCut).
        glm::dvec2 cellPx{0.0};
        int slot = 0;       // which of its entity's systems
        int systems = 0;    // of how many
        bool additive = false;
        Particles::Owner owner; // as this frame's update took it
        bool ownerGone = false; // its picture was taken away, and it with it
        float z = 0.0f;
        float ownerZ = 0.0f; // its owner's picture's
        // The depth the original sorts it at, on the level's axis: its owner's
        // eth_z plus its own <StartPoint> z at its entity's scale. `lifted` when
        // that z is not 0, and it is drawn at the depth rather than in its
        // owner's slot (Slot::LiftedSystems).
        double depth = 0.0;
        bool lifted = false;
        int count = 0;       // the pool
        int active = 0;      // GetNumActiveParticles
        int drawn = 0;       // particles with a quad this frame
        double largestDrawnSize = 0.0; // the largest of those, in units
        bool anyReleased = false;
        glm::dvec2 newestBornPx{0.0}; // the released particle with the youngest life
        double newestAgeMs = 0.0;     // and its age: 0 when it began this frame
    };
    std::vector<EmitterReport> EmitterReports() const;

    // Each of the level's own pictures in its slot, in drawing order, for the
    // suites: what a lifted system's depth is measured against (step 92).
    struct PictureReport {
        std::string node;
        int zIndex = 0;     // its node's z_index, which orders the slots
        double lookZ = 0.0; // its node's eth_z, unrounded
        float z = 0.0f;     // its slot's z, where its quad is drawn
        entt::entity quad{entt::null}; // that quad; null once the picture is gone
    };
    std::vector<PictureReport> PictureReports() const;

    // ---- WHAT STANDS BETWEEN TWO SLOTS, in one table (00_order K13) ---------
    //
    // A level's own pictures are drawn one to a slot, deepest first, in the order
    // their z_index gives them (SlotZ). Everything the port adds for a script
    // stands BETWEEN two of those slots, and the fraction of a slot it stands at
    // is what orders it against its neighbours. Each caller used to write its own
    // fraction beside its own call; they are this one table now, read through
    // slotAfter, slotAt and slotOver, and a new class joins it rather than
    // inventing a fraction of its own.
    //
    // THE ORDER AT EQUAL DEPTH. The fraction belongs to the class and not to the
    // instance, so two classes at one z_index land on exactly one z. Ethanon draws
    // such a pair in the order the entities were ADDED - one map keyed by depth,
    // and at equal depth the sprite, then the halo, then each system in order
    // (ETHEntityRenderingManager.cpp:74, 85, 106) - and sharing a z is not by
    // itself wrong: the original's escort is spawned by a dying minion and shares
    // its depth.
    //
    // The rows that share a fraction are in the order the layer ADDS their
    // quads, which is the nearest thing the port has to the same rule: the
    // dials, then the rings,
    // then a weightless room's door glow in buildSprites, the halos in
    // buildLights after it, then a tick's syncDrawables (a thrown stone),
    // syncBoss (the beholder, its spikes), syncMinions (a minion, then an
    // escort), syncGhost (chapter 3's boss, planning step 98) and syncWallGhost
    // (the ghost of a taken light wall), and an effect on the frame. What the renderer does with an exact tie is NOT
    // pinned by this step - it is one z, and nothing here makes the two draw in
    // a stated order - so the table states the order and test_mp_layer pins the
    // table, which is what 00_order K13 asks for ("pins that order, not
    // uniqueness"). Two pairs can tie today and both are named: a dial with a
    // ring, and a minion with an escort (the tie step 76's census found). A step
    // that needs a tie broken on the screen rather than stated is the step to
    // give the two rows their own fractions.
    //
    // THE MERGE OF THE TWO TRACKS added the last two rows. The door glow (step
    // 79) and the light wall's ghost (step 80) were written against the lambda
    // this table replaced, on the track that did not have it; both stood at the
    // lambda's own -0.5, so joining the table moves neither of them a unit, and
    // the 128-level sweep measures that (step 88's gate table).
    //
    // A PLACEMENT'S SYSTEMS ITS <StartPoint> z LIFTS (step 92, the visuals plan's
    // 8.4) are the one row that is not added by a script: a level's own system
    // whose start point stands off its entity's depth draws after every picture
    // at or below the sum, not in its owner's slot (buildEmitters). They spread
    // over (-1/2, -1/4) of the gap they land in - the one quarter no other row
    // stands in or spreads over - so they tie with nothing: after every picture
    // the table puts at -1/2 of that gap, before its placed portals, effect
    // halos and effect systems. What that costs is the table's own limit: in a
    // gap two classes share, the original orders them by DEPTH and the table by
    // CLASS, so the picture of an effect deeper than a lifted system but in its
    // gap still draws behind it, and the halo and systems of an effect shallower
    // than it still draw in front of it.
    //
    // NOT IN THE TABLE. The loading screen draws in a space of its own (0.32 for
    // the portal's systems, 0.6 and 0.61 for the vanish), because no level's
    // slots exist while it is up. And the black bars round the scene stand at
    // kLayerableTopZ, below: in front of everything, not between two slots.
    //
    // THE SHOT (00_order K14, the visuals plan's 10.1) stands at the FRONT of the
    // gap after the level's pictures at or below its z of 0 and BEHIND everything
    // else in it: addProjectile adds it at z 0 (#16-40), after the level, so it
    // is drawn over every picture of the level at 0 or below; but the player R10
    // puts at 4 (1.42), and a minion at its raw 2 (0.711), stand at larger z and
    // are drawn over it, and they share its gap wherever no node stands between
    // 0 and them - so the shot takes a fraction of its own below their -0.5, and
    // its halo one after its picture (Ethanon draws an entity's sprite, then its
    // halo, at the entity's own z: ETHEntityRenderingManager.cpp:74, 85). Both
    // stay in front of the previous slot's own halo (-0.75) and particle systems
    // (up to -0.5 - 0.25 / (n + 1) for n of them; the most any entity carries is
    // 2, whose last stands at -0.583), which is pinned in test_mp_layer. It was a
    // quarter IN FRONT of the player's slot until step 103: K14. The price of a
    // table that orders by class and not by z: a -0.5 class whose own z is BELOW
    // 0 - a dial, a ring, a door glow, a spike, a wall's ghost - is drawn over
    // the shot where the two share a gap, where the original draws it under.
    //
    // THE SHOCK RING (the visuals plan's 13.1, 00_order 8.13's K13 row) is the one
    // class that pays none of that price: it stands at the absolute -10 its agent
    // gives it, deeper than every other class the table holds but the door glow's
    // -22, so it takes a fraction of its own BEHIND all of them - behind the shot,
    // a minion and an effect wherever it shares their gap, as the original draws
    // it - and still in front of the previous slot's halo and of its deepest
    // system for up to 6 of them (the most any .ent carries is 2). Where it shares
    // a gap with a door glow (4-04 to 4-08, 4-12, 4-14 and 4-15 place both) the two
    // are drawn the other way round from the original's -22 behind -10; both are
    // added (AM_ADD, GL_ONE, GL_ONE), and an add commutes, so no pixel shows it.
    //
    // THE SKY'S COVER (sky.json pixel_cover, step 145) is the sky's own picture
    // drawn again out to the frame's pixel edges, and it shows only in the MSAA
    // samples the sky leaves uncovered at those edges, where the original's
    // pixel-centre frame shows the sky over whatever is behind it. So it stands
    // immediately behind its sky - a sixty-fourth of a slot, in front of every
    // other row but the picture, and of an effect's last system for up to 14 of
    // them - and the sky, opaque, draws every one of its own samples over it.
    enum class Slot {
        ShockRing,      // a shock agent's ring, at the absolute -10 its AddEntity gives it
        ShotPicture,    // the portal shot, at the z 0 addProjectile gives it, behind the player
        ShotHalo,       // the shot's halo, after its picture and still behind the -0.5 classes
        TimerDial,      // a timed crystal's dial, its crystal's z_index - 2 (art.json timer)
        WellRing,       // a gravity well's green ring, at the -5 its AddEntity gives it
        DoorHighlight,  // the glow behind a weightless room's shadowless door, at its own -22
        HaloNoPicture,  // a light whose owner draws no picture, and a torch with none
        ThrownStone,    // a stone in flight, at the player's slot
        Beholder,       // chapter 1's boss, at its adder's z_index
        Spike,          // and its spikes, at -4
        Minion,         // a minion or knight, minion_spawn's raw 2 in the level's units
        Escort,         // the escort the chapter-3 boss calls, at its anchor's depth
        Ghost,          // chapter 3's boss, one level unit behind the knight whose death adds it
        WallGhost,      // the ghost a lit torch leaves where its light wall stood, the wall's depth - 1.42
        Effect,         // an entity the script ADDS (Effects::Spawn), at round(z)
        LiftedSystems,  // a placement's systems whose <StartPoint> z is not 0, at its owner's z plus it
        PlacedPortal,   // a portal the player opened: portal.ent, at the z 0 insertPortal gives it
        EffectHalo,     // that entity's halo, a quarter in front of its own picture
        EffectSystems,  // and its particle systems, spread over the quarter after that halo
        SkyCover,       // a sky's cover, its own picture to the frame's pixel edges, just behind it
        Picture,        // a level's own picture, and the player's: the slot itself
        Halo,           // a light's halo, a quarter in front of its owner's picture
        Systems,        // an entity's particle systems, spread over the quarter after its halo
        Count
    };
    struct SlotRow {
        Slot slot;
        const char* name;
        // Of one slot (kSpriteSlotZ), from the slot's own zero - which is the
        // slot's own picture, since Slot::Picture is fraction 0. `spreads` says
        // the real fraction is Particles::SlotFraction's share of the quarter
        // that starts here, so an entity's systems never tie with each other.
        //
        // `spreads` is a statement of the row, for a reader and for the suite;
        // no arithmetic branches on it, because the only three rows that carry it
        // (Slot::LiftedSystems, Slot::EffectSystems and Slot::Systems) are the
        // only three systemZ is ever called with, and systemZ spreads
        // unconditionally. A class that needs the plain fraction goes through
        // slotAt/slotAfter/slotOver.
        double fraction;
        bool spreads;
    };
    static constexpr std::array<SlotRow, static_cast<std::size_t>(Slot::Count)> kSlotTable{{
        {Slot::ShockRing, "shock ring", -0.53125, false},
        {Slot::ShotPicture, "shot", -0.515625, false},
        {Slot::ShotHalo, "shot halo", -0.5078125, false},
        {Slot::TimerDial, "timer dial", -0.5, false},
        {Slot::WellRing, "gravity well ring", -0.5, false},
        {Slot::DoorHighlight, "door highlight", -0.5, false},
        {Slot::HaloNoPicture, "halo with no picture", -0.5, false},
        {Slot::ThrownStone, "thrown stone", -0.5, false},
        {Slot::Beholder, "beholder", -0.5, false},
        {Slot::Spike, "beholder spike", -0.5, false},
        {Slot::Minion, "minion", -0.5, false},
        {Slot::Escort, "escort", -0.5, false},
        {Slot::Ghost, "ghost", -0.5, false},
        {Slot::WallGhost, "light wall ghost", -0.5, false},
        {Slot::Effect, "effect", -0.5, false},
        {Slot::LiftedSystems, "lifted particle systems", -0.5, true},
        {Slot::PlacedPortal, "placed portal", -0.25, false},
        {Slot::EffectHalo, "effect halo", -0.25, false},
        {Slot::EffectSystems, "effect particle systems", -0.25, true},
        {Slot::SkyCover, "sky cover", -0.015625, false},
        {Slot::Picture, "picture", 0.0, false},
        {Slot::Halo, "halo", 0.25, false},
        {Slot::Systems, "particle systems", 0.25, true},
    }};
    static constexpr double SlotFractionOf(Slot slot) {
        return kSlotTable[static_cast<std::size_t>(slot)].fraction;
    }

    // THE TOP OF THE SCENE (00_order K13's row for black_bar, 8.13): where a
    // layerable entity at depth 1.0 is drawn. Ethanon draws a layerable entity at
    // Max(0.001, layerDepth) whatever its z (ETHSpriteEntity.cpp:34, :626-628), and
    // 1.0 is the front of the range every other piece of the scene is normalised
    // into, particles included (ETHEntityRenderingManager.cpp:37, 70-71). So it is
    // not between two slots and not a row of the table: it is one z in front of every
    // z the layer gives anything else - the level's slots from kSpriteBackZ, what
    // stands between them, and the boxes a player turns on over the art at the
    // markers' 0.5 - and behind the camera's near plane at 20 - 0.1. The screen
    // overlay (the HUD) is drawn after the scene and stays over it, as the original's
    // top layer is. test_mp_layer pins the value and that nothing else reaches it.
    static constexpr float kLayerableTopZ = 1.0f;

    // The tag of a sky's cover (Slot::SkyCover), which is not one of the level's
    // pictures: a suite finds it by this, and a count of "Magic Portals Sprite"
    // does not see it.
    static constexpr const char* kSkyCoverTag = "Magic Portals Sky Cover";

    // ---- the effects the script ADDS (system_7 7a-2) ------------------------
    //
    // One AddEntity call of the original's, as the layer draws it: an entity read
    // from its .ent when it is spawned, at a point, turned, at a scale, and
    // optionally carried by something the level moves. sim/Effects holds the call
    // sites' arithmetic (effects.json, step 82); this is what becomes of one.
    //
    // WHAT SPAWNS ONE TODAY. The level's start adds the player's red eyes
    // (buildEyes, the visuals plan's 7.1), and latchSimEffects - the place the
    // TICK queues from - adds every fireball in flight, a small_explosion where
    // one was destroyed and an explosion_large where a bomb went off (7.2). The
    // steps that draw the rest wire their own rules there (7.3 teleport, 10.1 the
    // shot's marks, 10.2 a crystal's pick and vanish, 10.3 to 11.2 what a key, a
    // diamond, a wall and a torch leave, and 11.3's blood and skull for a minion
    // that dies); the level's end and the player's death queue theirs from the
    // tick that decides them (latchFinishEffects, latchDeathEffects). And a minion
    // whose entity carries particle systems - the ghost_minion's aura, 3-32's
    // black knight - has them added once, carried by its picture, with no picture
    // of the runtime's own (latchSimEffects, planning step 97). The suites also
    // spawn them by hand, which is how the light cap and the free rules are
    // measured without a level that asks for one.
    struct EffectOwner {
        enum class Kind {
            None,         // it stays where it was added
            Player,       // the player's point: HIDDEN with the player, not freed
            Sprite,       // a level picture's entity point (a crystal, a key, a torch)
            Fireball,     // a carranca's fireball while it flies
            PlacedPortal, // a portal the player opened
            // A minion's entity point where its PICTURE is drawn this frame
            // (m_minions, by place: that list only grows). The systems of the
            // entity a marker added: the ghost_minion's aura (planning step 97).
            Minion,
            // Chapter 3's boss's point (Ghost::State::atPx) while it is there: what
            // ghost_appear_effect's followUp chases (planning step 98).
            Ghost,
            // One of its shots in flight, by the id the sim gave it (`index`).
            GhostShot,
            // A key with no node of its own - the one the ghost or the dark dragon
            // drops - by its place in Keys::State::keys (`index`), where the carry left
            // it and its bob put it (m_droppedKeys).
            Key,
            // Chapter 4's boss (DarkDragon::State::atPx) while it is there: the
            // dark_dragon.ent the summon adds, whose cell, angle, emissive and mouth
            // the runtime takes from the sim's Look every frame (planning step 99).
            DarkDragon,
            // Chapter 4's torch and wall (the visuals plan's 11.2), each by what the
            // original deletes it with. The orbiting fire signal, while Torch has
            // one (its callback moves it every frame and it is deleted when shot);
            // the lit torch `index`, while Torch says it is lit
            // (deleteLightFromProjectile); the torch `index` put back unlit, until
            // the tick after it is lit again (its `delete` branch); and the light
            // wall addLightWall built, until the wall is gone again.
            FireSignal,
            LitTorch,
            UnlitTorch,
            LightWall,
            // 2-32's knight, the dragon_knight.ent its spawn adds once the level is
            // finished, where its PICTURE is drawn this frame (m_knightQuad), as a
            // minion's: its aura (planning step 123).
            Knight,
            // The character's own body, never hidden: the flames addFlames puts on it
            // (effects.json fire32), which ETHCallback_fire32 moves onto it every
            // frame - SeekEntity finds a hidden entity, and Hide is a flag, so a
            // dead character's flames ride its body down (planning step 146).
            Character,
        };
        Kind kind = Kind::None;
        int index = -1;           // which one, for the kinds that key by place or id
        std::string name;         // or by name: a fireball's, which is unique for a level
        glm::dvec2 offsetPx{0.0}; // where on it, in the level's pixels
    };
    struct EffectSpawn {
        std::string entity;    // the .ent, among the original's entities
        glm::dvec2 atPx{0.0};  // where AddEntity puts it, in the level's pixels
        double zUnits = 0.0;   // the vector3's z, in the level's units
        double angleDeg = 0.0; // Ethanon's angle, counter-clockwise on the screen
        double scale = 1.0;    // the script's Scale(), which the .ent's <Scale> multiplies
        // ETHEntity::ScaleParticleSystem(0, n): a further scale on the FIRST
        // particle system alone, over the entity's own, touching neither the
        // picture nor the light nor the halo. addFireball is the one caller in
        // the game (effects.json fireball), and it passes 1 for everything but
        // the dark dragon's fire.
        double firstSystemScale = 1.0;
        EffectOwner follow;
        // THE DEPTH CLASS ITS PICTURE TAKES, for an entity the between-slot table
        // names in its own right: a portal the player opens is Slot::PlacedPortal
        // (00_order K13), and everything else the script adds is Slot::Effect. Its
        // halo and its systems keep their own classes either way.
        Slot pictureSlot = Slot::Effect;
        // WHETHER THE RUNTIME DRAWS ITS PICTURE. False for an entity whose picture
        // the layer already draws as an actor, by its own sync on its own sheet: a
        // minion's (syncMinions, cut as the original cuts it, framed by the sim's
        // arm). Its light and its systems are still the runtime's, carried by it
        // (planning step 97: ghost_minion.ent's aura).
        bool picture = true;
        // ShouldEntityBeAlwaysActive's second arm (ETHActiveEntityHandler.cpp:70-74):
        // dynamic with a callback or a simulated body, which the caller knows and
        // the .ent reader does not. A minion is both. It decides only whether a
        // HIDDEN effect is still stepped, and a minion's never is hidden.
        bool alwaysActive = false;
        // ETHEntity::SetColor, which PortalManager::update gives a placed portal
        // and ETHCallback_portal then fades: ONLY the entity's sprite reads it
        // (ETHRenderEntity.cpp:112-117 against ETHParticleManager.cpp:381-389), so
        // its particles are never tinted by it.
        glm::dvec3 colour{1.0};
        // ITS OWN CALLBACK'S followUp, where it has one: it eases toward what
        // `followUpTarget` names, every frame after its particles are stepped
        // (ghost_appear_effect.ent, planning step 98).
        std::optional<Effects::FollowUpRule> followUp;
        EffectOwner followUpTarget;
        // And its ETHEntity::Scale(n) every frame: its systems' lengths and every
        // live particle's size and speed times this. 1 for everything else.
        double scalePerFrame = 1.0;
        // ETHEntity::SetAlpha, which only its sprite reads either, and what its own
        // callback adds to it each second of frame time up to 1: the light wall
        // addLightWall builds is added at 0 and fades in (effects.json
        // light_wall_at_ghost). 1 and 0 for everything else.
        double alpha = 1.0;
        double alphaPerSecond = 0.0;
        // bounce(), where its callback runs one (effects.json light_from_projectile's
        // `bounce`, on a level with a light wall): the entity's SetScale swung over
        // its own, which moves its picture and its halo and never its particles
        // (ETHEntity.cpp:601-608). Its clock starts at the entity's first frame.
        bool bounces = false;
        Art::Pulse bounce;
    };
    // AddEntity, on the FRAME and never on the tick: the effect's id, or -1 when
    // the level has no art, the .ent cannot be read, or what it is to follow has
    // already gone.
    //
    // NOT FROM INSIDE updateEffects' loop. It pushes onto m_effects, which is a
    // vector, and that loop holds a reference into it - a rule that spawns one
    // effect as another is freed (7.4's portal_death) queues it instead, which is
    // what m_effectQueue and latchSimEffects are for.
    int SpawnEffect(entt::registry& registry, const EffectSpawn& spawn);
    // What each live effect is doing, for the suites, in the order they were added.
    struct EffectReport {
        int id = 0;
        std::string entity;
        glm::dvec2 atPx{0.0};   // where it stands now: its owner's point, where it has one
        // The depth the call gave it, on the LEVEL's axis: effectAdd's reading of
        // Effects::Spawn (a scaled z as it stands, a bare literal over the scale
        // factor), before any slot is counted.
        double zUnits = 0.0;
        float z = 0.0f;         // its picture's slot (Slot::Effect)
        float haloZ = 0.0f;
        // Each of its emitters' z (Slot::EffectSystems), in the .ent's order: the
        // slot its own <StartPoint> z puts it in, and that slot's quarter after
        // the halo shared out by Particles::SlotFraction. These are the numbers
        // the particle quads are placed with, so the suite can hold the table's
        // three new rows to what is drawn.
        std::vector<float> systemZ;
        // And each emitter's particle size as it was made, in the level's units:
        // the .ent's own, times the entity's particle scale, times the first
        // system's own extra scale where a caller gave one (ScaleParticleSystem).
        std::vector<double> systemSizePx;
        // And the texture each is drawn with (particleTexture): its bitmap's file,
        // or for an AM_MODULATE system the multiply made from it.
        std::vector<std::string> systemImage;
        double scriptScale = 1.0; // the Scale() it was added with
        // The angle its AddEntity was given, or its script last set, in Ethanon's
        // degrees (counter-clockwise on the screen): what its particles are turned
        // by (Particles::Owner). A radian number on shock_strike, which the script
        // hands over unconverted (Effects::Turn::RadiansAsDegrees).
        double angleDeg = 0.0;
        int cell = 0;             // the cell of its picture's sheet it is drawn on
        glm::dvec2 centrePx{0.0}; // where its picture is centred: its point less its turned pivot
        glm::dvec2 cellPx{0.0};   // and its size there
        glm::dvec2 uvScale{0.0};  // the share of its sheet a cell samples, 0 for a one-cell picture
        glm::dvec2 uvOffset{0.0};
        glm::dvec3 colour{1.0}; // ETHEntity::SetColor, on its sprite alone
        // ETHEntity::SetEmissiveColor as it stands: the .ent's own, or what its
        // script last wrote (the dark dragon's flash).
        glm::dvec3 emissive{0.0};
        double alpha = 1.0;     // ETHEntity::SetAlpha, on its sprite alone
        glm::dvec2 scale{1.0};  // times its <Scale>: the one its light and picture take
        // What bounce() has its SetScale at this frame, over `scale`: 1 but for a lit
        // torch on a level with a light wall. Its picture and halo take it.
        glm::dvec2 bounceScale{1.0};
        entt::entity quad{entt::null};  // its picture, where it has one
        entt::entity light{entt::null}; // its light and its halo, where it has them
        entt::entity halo{entt::null};
        // Its light and halo as the owner's scale leaves them (Lighting::OwnerScaled,
        // K7): the range in the level's units, and the halo quad's size in them.
        // Zero where it has neither.
        double lightRange = 0.0;
        glm::dvec2 haloPx{0.0};
        bool temporary = false; // ETHEntity::IsTemporary: the engine frees it when its pools are over
        bool hidden = false;    // its owner is hidden (the player in a door), so nothing of it draws
        bool drawn = false;     // it has a picture quad (`quad` above)
        // Whether its first system is killed (KillParticleSystem): it releases no more.
        bool firstSystemKilled = false;
        bool hasLight = false;
        bool hasHalo = false;
        // The registry entities its particles are drawn on, the live ones in
        // emitter order: what a suite needs to ask the registry what became of
        // THIS effect's specks rather than of every effect's at once. They are
        // pooled, so a handle here outlives the frame that drew it.
        std::vector<entt::entity> particleQuads;
        // What it follows (EffectOwner): the kind, and which one by place or id.
        EffectOwner::Kind followKind = EffectOwner::Kind::None;
        int followIndex = -1;
        // Where each particle the last step released or renewed was born - one
        // whose age is still 0 - in emitter order: the point its owner stood at
        // on that step, plus its start point and spread.
        std::vector<glm::dvec2> newbornPx;
        int systems = 0;        // its emitters
        int pool = 0;           // the particles they were made with, summed
        int active = 0;         // GetNumActiveParticles, summed over them
        int quads = 0;          // the pooled particle quads it holds, drawn or not
        int released = 0;       // the particles that have been released at all, summed
        // Every particle's elapsed life, summed over every emitter: ONE number
        // that moves on any frame this effect is stepped and does not move on one
        // it is not, whatever the pool does. What "frozen" is measured with.
        double ageSumMs = 0.0;
        // Whether the engine would go on updating it while it is hidden
        // (ETHActiveEntityHandler::ShouldEntityBeAlwaysActive).
        bool alwaysActive = false;
    };
    std::vector<EffectReport> EffectReports() const;
    // How many effects have been added and freed since the layer was attached,
    // for the suites: a temporary one is freed when its pools are over.
    int EffectsSpawned() const { return m_effectsSpawned; }
    int EffectsFreed() const { return m_effectsFreed; }

    // Each timed crystal's dial (art.json timer), for the suites, in the crystals'
    // order: as the last tick left it.
    struct TimerReport {
        std::string crystal;     // the crystal's node
        bool drawn = false;      // its quad is there
        bool shrinking = false;  // the time is up, or the crystal was taken a tick ago or more
        float z = 0.0f;
        float crystalZ = 0.0f;   // its crystal's picture's
        int frame = 0;           // the cell it shows
        glm::dvec2 scale{1.0};   // its pulse or its shrink
        double alpha = 0.0;      // the alpha tint is given
        glm::dvec2 atPx{0.0};    // where it is drawn, in the level's pixels
    };
    std::vector<TimerReport> TimerReports() const;

    // Each minion's and knight's picture (art.json minion), for the suites, in
    // Minions::State's order: as the last tick left it.
    struct MinionReport {
        std::string name;          // the minion's (its marker's node, or the boss's escort)
        entt::entity quad{entt::null}; // its picture; null while it has none
        bool black = false;        // drawn as ghost_minion.ent, SetColor(0)
        int column = 0;            // the frame it shows
        int row = 0;
        float z = 0.0f;            // its slot among the art
        double lookZ = 0.0;        // its depth in the level's units
        glm::dvec2 centrePx{0.0};  // where its picture is centred, in the level's pixels
        glm::dvec2 sizePx{0.0};    // its frame, in units
        glm::dvec2 uvScale{0.0};   // the share of minion.png its frame samples
        glm::dvec2 uvOffset{0.0};  // and where
    };
    std::vector<MinionReport> MinionReports() const;

    // Chapter 3's boss (art.json ghost, ghost.json look), for the suites: as the
    // last tick left it. `present` is false on the 127 levels with no ghost_pos, and
    // `drawn` false until the black knight's death adds it (R4).
    struct GhostReport {
        bool present = false;
        bool spawned = false;
        bool drawn = false;
        entt::entity quad{entt::null};
        std::string texture;       // the tier file the original draws
        int cell = 0;              // the frame of ghost.png it shows
        glm::dvec3 colour{1.0};    // SetColor
        double alpha = 1.0;        // SetAlpha
        double scale = 1.0;        // SetScale
        glm::dvec2 atPx{0.0};      // the ghost's point: Ghost::State::atPx
        glm::dvec2 centrePx{0.0};  // where its picture is centred: that point less the pivot, scaled
        glm::dvec2 sizePx{0.0};    // one frame, in units, times the scale
        glm::dvec2 frameUnits{0.0};
        glm::dvec2 uvScale{0.0};
        glm::dvec2 uvOffset{0.0};
        float z = 0.0f;            // its slot among the art (Slot::Ghost)
        double lookZ = 0.0;        // its depth in the level's units
    };
    GhostReport GhostDrawing() const;
    // Which of sounds.json's tracks the original would be playing now, whether or
    // not a device plays it: AudioManager.currentTrack, unless its one player has
    // been stopped since that track was asked for or a switch is off. "" for none.
    std::string WantedTrack() const;

    // 2-32's dragon and its claw (dragon.json), for the suites: as the last tick
    // left them. `present` is false on the 127 levels that place neither.
    struct DragonReport {
        bool present = false;
        entt::entity quad{entt::null};     // the dragon's picture
        entt::entity clawQuad{entt::null}; // and the claw's
        glm::dvec2 centrePx{0.0};          // where the dragon's picture is centred
        glm::dvec2 sizePx{0.0};            // one cell of dragon_boss.png, in units
        glm::dvec2 uvScale{0.0};           // the share of the sheet that cell samples
        glm::dvec2 uvOffset{0.0};          // and where
        int frame = 0;                     // the cell its callback last set
        double angleDeg = 0.0;             // Ethanon's angle, counter-clockwise on the screen
        glm::dvec2 atPx{0.0};              // the entity's own point: Dragon::State::atPx
        glm::dvec2 clawCentrePx{0.0};      // and the claw's picture, where its waypoints put it
        double clawAngleDeg = 0.0;
        int clawWaypoint = 0;
        bool systemKilled = false; // its mouth's particle system, as the windows left it
    };
    DragonReport DragonPicture() const;

    // The exit door of a chapter-4 room, for the suites: which sheet it is drawn
    // on, the glow door_highlight.ent puts behind it on a `shadowless_door` level,
    // and the bob Game::loop gives all three on a `no_gravity` one (art.json
    // `door`, motions.json `door`). As the last tick left it.
    struct DoorReport {
        bool shadowless = false;           // the level's properties carry `shadowless_door`
        bool bobs = false;                 // and `no_gravity`, so Game::loop bobs the three
        Motion::Linear motion;             // the one angle the three share
        glm::dvec2 offsetPx{0.0};          // what the last call put on them
        entt::entity quad{entt::null};     // the door's picture
        std::string texture;               // and the file it is drawn from
        glm::dvec2 atPx{0.0};              // where its entity stands, bob and all
        entt::entity bgQuad{entt::null};   // the doorway's black, where a level has one
        glm::dvec2 bgAtPx{0.0};
        entt::entity glowQuad{entt::null}; // the glow, on the 17 levels that have one
        std::string glowTexture;
        glm::dvec2 glowCentrePx{0.0};      // its centre, which is the door's own point
        glm::dvec2 glowSizePx{0.0};        // its units: door_highlight.png's 128 x 192
        float glowZ{0.0f};                 // the slot it is drawn in, behind the door's
        bool glowAdditive = false;
    };
    DoorReport DoorPicture() const;

    // THE WIN CUTSCENES (sim/Cutscene.hpp, cutscenes.json; planning step 123), as the
    // last tick left them: the delay before the medal, 4-01's rocket and bridge,
    // 4-19's landing - the door, the key, the eclipse, the spiral and the camera's
    // target - and 2-32's knight.
    struct CutsceneReport {
        double wonDelayMs = 0.0;       // the level's end_delay, or level_end's 1400
        bool launchPlaced = false;     // the level places rocket_launch
        bool landingPlaced = false;    // and rocket_landing
        Cutscene::Launch launch;
        Cutscene::Landing landing;
        int launchStartedTick = -1;    // the layer's tick its first update ran on
        int landingStartedTick = -1;
        int quakes = 0;                // rocket_launch's startEarthquake calls
        glm::dvec2 doorAtPx{0.0};      // the door's entity point as drawn: node, bob and cutscene
        glm::dvec2 doorCentrePx{0.0};  // and its picture's centre
        entt::entity bridgeQuad{entt::null};
        glm::dvec2 bridgeCentrePx{0.0};
        glm::dvec2 bridgeRestPx{0.0};  // where the level's body holds it
        double keyScale = 1.0;         // the landing's key: its size and alpha
        glm::dvec2 eclipseAtPx{0.0};   // its entity point as drawn
        glm::vec4 spiralColour{1.0f};  // the spiral's drawn colour
        glm::dvec2 cameraTargetPx{0.0}; // what the follow was given this tick
        Cutscene::Knight knight;
        glm::dvec2 knightAtPx{0.0};    // its entity point
        entt::entity knightQuad{entt::null};
        glm::dvec2 knightCentrePx{0.0};
        bool knightAuraQueued = false;
        int knightSpawnedTick = -1;
        int knightEnteredTick = -1;
        glm::dvec2 knightEnteredAtPx{0.0};
    };
    CutsceneReport CutscenePicture(entt::registry& registry) const;

    // The ghost of the wall a lit torch takes away (art.json `light_wall_ghost`,
    // sim/Torch): dashed_door.ent, added at the wall's own point and angle on the
    // tick the torch is lit and deleted on the tick a fire signal puts the wall
    // back. As the last tick left it.
    struct LightWallGhostReport {
        bool possible = false;             // the level has a wall and the picture resolved
        bool added = false;                // the wall is flagged: the ghost is there
        bool drawn = false;                // and a frame has stepped it, so its picture is there
        entt::entity quad{entt::null};
        std::string texture;               // the tier file the original draws
        glm::dvec2 atPx{0.0};              // the wall's point, which the ghost never leaves
        glm::dvec2 sizePx{0.0};            // dashed_door.png's units: 32 x 128
        glm::dvec2 scale{1.0};             // the pulse this tick, 1.006 to 0.994
        double alpha = 0.0;                // its SetAlpha this tick, 0.1 to 0.3
        double angleDeg = 0.0;             // the `angle` datum the alpha's cos reads
        double pulseMs = 0.0;              // bounce's blinkElapsedTime
        float z = 0.0f;                    // the slot it is drawn in, behind the wall's
        float rotation = 0.0f;             // the wall's angle, as the engine turns a quad
        double lookZ = 0.0;                // its depth in the level's units
    };
    LightWallGhostReport LightWallGhost() const;

    // Chapter 4's torches as the layer draws them (the visuals plan's 11.2), per
    // torch of Torch's: whether the level's own unlit picture still stands, and the
    // depth the torch entity is at now.
    struct TorchReport {
        bool levelPicture = false; // its node's quad is there
        bool unlitGone = false;    // the `delete` branch has run since it was last lit
        double z = 0.0;
        // What its unlit self's AddToAngle(unitsPerSecond(190)) has turned it by
        // (art.json unlit_torch, planning step 146), from the entity's start - the
        // level's own, or the one a put-out added - and the angle its level
        // picture's systems are turned by now, Ethanon's.
        double turnedDeg = 0.0;
        double systemsAngleDeg = 0.0;
    };
    std::vector<TorchReport> TorchReports() const;
    // A level picture by its node, for the suites, as the last frame drew it:
    // whether it is there, where, at what size and alpha, and its entity's angle
    // (planning step 146: a shock agent's pulse, a carried diamond's alpha, a hand
    // tapped away). `drawn` false and the rest empty for a node with no picture.
    struct DrawnPictureReport {
        bool drawn = false;
        entt::entity quad{entt::null};
        glm::dvec2 centrePx{0.0};
        glm::dvec2 sizePx{0.0};
        float alpha = 1.0f;
        glm::dvec2 ownerPx{0.0};
        double ownerAngleDeg = 0.0;
        double lookZ = 0.0; // its node's eth_z, its depth in the level's units
    };
    DrawnPictureReport DrawnPicture(const std::string& node) const;
    // A gravity well agent's turn, in Ethanon's degrees, as its callback sets it
    // this tick (gravitywell.json roundabout): a whole turn at rest.
    double WellSpinDegrees(int well) const;

    // The four black bars round the scene (art.json black_bar): black_bar.ent as
    // Game::manageSceneBounds adds it, placed once with the level at its max
    // marker and never moved. Empty where the art did not read. In the order the
    // script adds them: A and B above the scene, C and D below it.
    struct BlackBarReport {
        entt::entity quad{entt::null};
        std::string texture;       // the tier file the original draws: black_bar.png, sd only
        glm::dvec2 centrePx{0.0};  // its centre, in the level's units
        glm::dvec2 sizePx{0.0};    // black_bar.png's units at the entity's scale: 512 x 32
        float z{0.0f};             // kLayerableTopZ
    };
    std::vector<BlackBarReport> BlackBars() const;

    // The ring each shock agent adds round itself (art.json shock_ring, the visuals
    // plan's 13.1): shock_area.ent at the field's point, the lethal diameter across,
    // turned by game time on the tick and scaled by a fresh factor every frame, pause
    // or not. One per Fields::State::fields entry and in its order; empty where the
    // art or the .ent did not read. As the last frame left it.
    struct ShockRingReport {
        std::string field;             // the agent's node, Fields::Field::name
        entt::entity quad{entt::null}; // null once its agent is gone
        std::string texture;           // the tier file the original draws: hd/white_ring.png
        bool additive = false;
        glm::dvec2 atPx{0.0};          // the field's point, where the tick left it
        double baseUnits = 0.0;        // scaleToSize's size: 2 x areaRadius
        double jitter = 1.0;           // this frame's factor, in [from, from + span]
        int jitters = 0;               // the factors drawn so far: one a frame
        double turnedDeg = 0.0;        // AddToAngle's, counter-clockwise
        float z{0.0f};                 // Slot::ShockRing after the art at or below -10
        double lookZ = 0.0;            // its depth in the level's units: -10
    };
    std::vector<ShockRingReport> ShockRings() const;

    // The slot the player's picture is drawn in, and its depth in the level's units:
    // GameCharacter's 4 over the scale factor (art.json character.z, depth).
    float PlayerSlotZ() const;
    double PlayerDepth() const { return m_playerZ; }

    // Each crystal's and key's linearMotion (sim/Motion, motions.json), for the
    // suites, in the level's drawing order (m_sprites'): as the last tick left it.
    struct MotionReport {
        std::string node;           // the entity's node
        bool crystal = false;       // a crystal's, or else a key's
        bool drawn = false;         // its picture is there
        entt::entity quad{entt::null}; // that picture
        bool moving = false;        // its script calls linearMotion now: a live crystal, an unowned unspent key
        Motion::Linear motion;      // the start angle, the angle, whether the first call has run
        glm::dvec2 offsetPx{0.0};   // what the last call put on its picture, 0 while it does not move
        glm::dvec2 centrePx{0.0};   // where its picture was last drawn
        glm::dvec2 ownerPx{0.0};    // where its entity stands: its particles' origin
    };
    std::vector<MotionReport> MotionReports() const;

    // Each placed picture whose own callback motions.json's `placed` names (the hint
    // arrow, the dashed circle and square, the tapping hand), for the suites, in the
    // level's drawing order: as the last tick left it.
    struct PlacedReport {
        std::string node;              // the entity's node
        std::string entity;            // its row's entity, without .ent
        bool drawn = false;            // its picture is there
        entt::entity quad{entt::null}; // that picture
        bool moves = false;            // its linearMotion runs (its row calls one and its node has speed and stride)
        Motion::Linear motion;         // that call's arguments and angle
        glm::dvec2 offsetPx{0.0};      // what the last call put on its picture
        double turnedDeg = 0.0;        // what AddToAngle has turned it by, counter-clockwise
        float alpha = 1.0f;            // the alpha its colour is drawn at
        glm::dvec2 centrePx{0.0};      // where its picture was last drawn
        glm::dvec2 ownerPx{0.0};       // where its entity stands
        double ownerAngleDeg = 0.0;    // and its angle, Ethanon's
    };
    std::vector<PlacedReport> PlacedReports() const;

    // 1-21's lesson (camera.json `hint`), for the suites, as the last tick left it:
    // whether the level places each picture, the two callbacks, and each picture -
    // whether it is still there, where the hand was put and is drawn, and the alpha
    // each is drawn at.
    struct HintReport {
        bool handPlaced = false;
        bool arrowPlaced = false;
        Hint::Hand hand;
        Hint::Arrow arrow;
        bool handDrawn = false;
        bool arrowDrawn = false;
        glm::dvec2 handCentrePx{0.0};
        float handAlpha = 0.0f;
        float arrowAlpha = 0.0f;
    };
    HintReport HintState() const;

    // THE HANDLES (handle.json, sim/Handle.hpp), for the suites, as the last tick
    // left them: each handle's picture - where it is drawn, its alpha and its size -
    // each follower's picture and its light's owner point, and 2-20's lesson hand.
    struct HandleReport {
        std::string node;
        bool drawn = false;
        glm::dvec2 centrePx{0.0};
        glm::dvec2 sizePx{0.0};
        float alpha = 0.0f;
    };
    std::vector<HandleReport> HandleReports() const;
    struct FollowerReport {
        std::string node;
        bool drawn = false;
        glm::dvec2 centrePx{0.0};
        glm::dvec2 ownerPx{0.0};
    };
    std::vector<FollowerReport> FollowerReports() const;
    struct LessonReport {
        bool placed = false;
        Handle::Lesson lesson;
        bool drawn = false;
        glm::dvec2 centrePx{0.0};
        float alpha = 0.0f;
    };
    LessonReport LessonState() const;
    const Hint::Rules& HintRules() const { return m_hintRules; }
    const Camera::Rules& CameraRules() const { return m_cameraRules; }
    // Whether the camera is being held where a drag left it (camera.json `drag`).
    bool CameraHeld() const { return m_follow.Held(m_cameraRules); }

    // A screen point (Input's coordinates) as the point in the level under it,
    // in the port's pixels. False when there is no viewport or camera.
    bool ScreenToLevelPx(const entt::registry& registry, const glm::vec2& screenPoint, glm::dvec2& outPx) const;
    // The same from a point already inside the viewport, which is what a touch
    // carries: a scheduled tap has no cursor to read.
    bool ViewportToLevelPx(const entt::registry& registry, const glm::vec2& localPoint, glm::dvec2& outPx) const;

    // ---- the HUD, and the seconds a level opens with ------------------------
    //
    // The on-screen controls, in the original's order of drawing: the walk pads
    // in the bottom corners, restart and pause flush in the top-right, and the
    // clear-portals button in the top-left while a placed portal is alive.
    enum class Control { Left, Right, Reset, Menu, Clear };

    // ui.json, as read at attach. HudError says why nothing could start when
    // the port's data - ui.json among it - would not read; empty otherwise.
    const Hud::Rules& HudRules() const { return m_hudRules; }
    const std::string& HudError() const { return m_hudError; }

    // How long the level being shown has been loaded, in milliseconds of the
    // TICK's clock: zero on the tick it loads, a retry included. Everything the
    // level opens with - the black, "Part N", the plaque, the pads' slide and
    // pulse - is a function of this and nothing else.
    double LevelAgeMs() const { return m_levelAgeMs; }

    // A control's rectangle on the view as the last tick laid it out, (0, 0)
    // the view's top-left, in design units; false when it is not shown.
    bool ControlRect(Control control, Hud::Rect& out) const;

    // The no-portal sign's rectangle on the view as the last tick left it, in
    // design units; false in a level that places none.
    bool NoPortalSignRect(Hud::Rect& out) const;

    // ---- the menu states: the loading screen, the main menu and the black -----
    //
    // Every menu state's clock runs on the tick (ui.json menu_state._about): zero
    // on the state's first tick, which is the tick after the one whose release -
    // or whose loading hold, or back key - asked for it, as the original's
    // setState swaps between frames. The layer's own clock, which the title's bob
    // reads, runs from attach.
    const MenuState::Rules& MenuStateRules() const { return m_mainMenuRules.state; }
    const MainMenu::Rules& MainMenuRules() const { return m_mainMenuRules; }
    const Loading::Rules& LoadingRules() const { return m_loadingRules; }
    // How long the menu state that is up has been current: ticks and milliseconds.
    int MenuStateTicks() const { return m_menuClock.ticks; }
    double MenuStateMs() const { return m_menuClock.ms; }
    // The same, in WALL milliseconds: the clock the state's black (a FadeInController,
    // which reads sys:GetTime) fades on.
    double MenuStateWallMs() const { return m_menuClock.wallMs; }
    // The layer's own clock since it attached, in milliseconds of the tick.
    // The layer clock: WALL milliseconds, the engine's tick a tick, which sys:GetTime and
    // sys:GetTimeF read (the earthquake generator, the portal's 400 ms kill, the
    // choosers' blink). The scripts' own clock is the floor of it, LevelAgeMs's step.
    double LayerClockMs() const { return m_layerClockMs; }
    // The last tick's two deltas: the script's whole milliseconds and the physics step the
    // engine integrated (sim/ScriptClock.hpp, planning step 147).
    const ScriptClock::Step& TickStep() const { return m_step; }
    const ScriptClock::Rules& ClockRules() const { return m_clockRules; }
    // Whether the engine's interpolation is kept for the frames being drawn: a real-time
    // run whose display outruns the tick (pictures.json). False in a --fixed-step run and at
    // the matched 60 Hz, where every picture - the camera too - is drawn at its tick's own
    // place, as the original draws it (planning step 150).
    bool PicturesSmoothed() const { return m_pictureRate.Smoothing(); }
    const Pictures::Rules& PictureRules() const { return m_pictureRules; }
    // The mean rate of the frames OnUpdate was handed, in hertz (0 before the first).
    double FrameHz() const { return m_pictureRate.Hz(); }
    // The main menu's buttons a held touch that went down inside them is still
    // inside (MainMenu::Bit): drawn at the press tint.
    unsigned MainMenuHeld() const { return m_menuTouch.mainHeld; }
    // The main menu's two switches, and the music switch's clock on this state.
    Pause::Switches MainMenuSwitches() const;
    // Press one of the main menu's buttons, as a release on it does - at once: a
    // touch's release asks for it on the tick after. False when it led nowhere: no
    // main menu is up, or the music switch is not there.
    bool PressMainMenu(entt::registry& registry, MainMenu::Button button);

    // ---- credits and the achievements dashboard -------------------------------
    //
    // The main menu's info and Achievements buttons open them; each back button,
    // and the back key, goes back to the main menu on the tick after. Both are
    // drawn by EmitMenu through the overlay, and keep their scroll across the
    // per-tick layout and a resize.
    const Credits::Rules& CreditsRules() const { return m_creditsRules; }
    // Chapter select and the level grid (sim/Selector.hpp, step 56): what the save
    // opened, laid out as the state opened, and the pager as the last tick left it.
    const Selector::Rules& SelectorRules() const { return m_selectorRules; }
    const Chapters::Table& ChapterTable() const { return m_chapters; }
    const Selector::Board& SelectorBoard() const { return m_selectorBoard; }
    const Selector::State& SelectorState() const { return m_selectorState; }
    // A popup over a menu state: GameFinishedPopup, which chapter select raises
    // after the last level of the last chapter (planning step 119). Null when none
    // is up. It is the current UI layer, so the PageManager under it is not updated
    // (UILayerManager::update): its title and arrows neither enter nor bounce, and
    // take no touch, until the popup is gone; SelectorLayerMs is that layer's clock.
    const Popup::Class* MenuPopupClass() const { return m_menuPopup.open ? m_menuPopup.cls : nullptr; }
    const Popup::Open* MenuPopup() const { return m_menuPopup.open ? &m_menuPopup.state : nullptr; }
    double SelectorLayerMs() const {
        return m_menuPopup.open ? 0.0 : std::max(0.0, m_menuClock.ms - m_selectorLayerFromMs);
    }
    // The credits' strip as the last tick left it.
    const Credits::Scroll& CreditsScroll() const { return m_credits; }
    // Whether the credits' back button is held down inside: the press tint.
    bool CreditsBackHeld() const { return m_creditsTouch.backHeld; }
    const Dashboard::Rules& DashboardRules() const { return m_dashboardRules; }
    const Locking::Rules& LockingRules() const { return m_lockingRules; }
    // The rows as the dashboard opened with them, and its scroll and buttons.
    const Dashboard::Board& DashboardBoard() const { return m_dashboardBoard; }
    const Dashboard::State& DashboardState() const { return m_dashboardState; }
    // The original's achievements, when Paths::achievements could be read; null
    // otherwise, and AchievementsError says why.
    const Achievements::Content* AchievementsContent() const {
        return m_achievementsLoaded ? &m_achievements : nullptr;
    }
    const std::string& AchievementsError() const { return m_achievementsError; }

    // ---- unlocking them (planning step 143) --------------------------------------
    //
    // What unlocks each (game/data/achievements.json, sim/Achievements.hpp's
    // Rules), the set the player has unlocked, kept in Paths::saveDir beside the
    // medals (Achievements::Store), and the original's Notificator, which shows an
    // unlock's card - and the dashboard's "This level is locked!" - over every
    // screen (sim/Notificator.hpp). AchievementRulesError is empty when the rules
    // read; without them nothing unlocks.
    const Achievements::Rules& AchievementRules() const { return m_achievementRules; }
    const std::string& AchievementRulesError() const { return m_achievementRulesError; }
    const Achievements::Store& AchievementStore() const { return m_achievementStore; }
    // Every unlock this run, in order: the id, the layer's tick, and whether a card
    // was asked for.
    struct Unlock {
        int id = -1;
        int tick = 0;
        bool notify = false;
    };
    const std::vector<Unlock>& Unlocks() const { return m_unlocks; }
    const Notificator::Rules& NotificatorRules() const { return m_notificatorRules; }
    const Notificator::Queue& Notifications() const { return m_notificator; }
    // What the last door's tick asked of the level (checkForLevelAchievements'
    // arguments), for the suites; empty before a door.
    std::optional<Achievements::Play> LastDoorPlay() const { return m_lastDoorPlay; }
    // The music's volume as the original's calls have left it for the current
    // track: the track's own, or what a card set (setMusicVolume and
    // setDefaultMusicVolume); a negative value for no current track.
    double MusicVolumeNow() const;
    // Puts the Notificator's card into the screen overlay, over everything else this
    // frame. OnUpdate calls it once a frame, after EmitHud and EmitMenu.
    void EmitNotifications(entt::registry& registry) const;
    // Puts this frame's menu into the screen overlay: the main menu's pictures, the
    // loading screen's logo and dots, and every menu state's black. Nothing when no
    // menu state is up. OnUpdate calls it once a frame.
    void EmitMenu(entt::registry& registry) const;

    // Puts this frame's HUD into the engine's screen overlay, in the original's
    // order of drawing. OnUpdate calls it once a frame; it is public so a suite
    // can ask for exactly the HUD without the rest of a frame. Nothing is drawn
    // when the registry publishes no overlay, which a bare suite registry does
    // not until it inserts one.
    void EmitHud(entt::registry& registry) const;

    // ---- the pause screen -----------------------------------------------------
    //
    // CustomGameMenuLayer, over the level it pauses: the in-level pause control
    // and Escape open it, and it STOPS GAME TIME - the level is not stepped, the
    // world does not move, the pads are not drawn - while its own entrance runs
    // on a clock of its own. Laid out by sim/Pause.hpp from ui.json and drawn
    // through the screen overlay after restart, pause and the blacks. The level
    // stays loaded under it and MenuScreen() stays Screen::None: the pause is a
    // layer over a level, not a screen that replaces one.
    bool Paused() const { return m_pause.open; }
    // How long it has been up, in milliseconds of the tick's clock: zero on the
    // tick it opened.
    double PauseClockMs() const { return m_pause.clockMs; }
    const Pause::Rules& PauseRules() const { return m_pauseRules; }
    // What the open pause was raised over, read when it opened.
    const Pause::Level& PausedLevel() const { return m_pause.level; }
    // The two switches, and the music switch's clock on this pause.
    Pause::Switches PauseSwitches() const;
    // Whether the game makes a sound at all, and plays its music: the pause's
    // two switches, kept between runs where Paths::saveDir is given.
    bool SoundOn() const { return m_soundOn; }
    bool MusicOn() const { return m_musicOn; }
    // Press one of the pause's buttons, as a tap on it does. False when it led
    // nowhere: no pause is up, the button is not there, or it is Achievements on a
    // level no achievement names (the owner's ruling keeps the button there; the
    // original has none to press).
    bool PressPause(entt::registry& registry, Pause::Button button);
    // The pause's AchievementsPopup (planning step 143): up over the pause, its text,
    // and its popup's clock. Closed by a touch down anywhere, its close button or
    // the back key, as a help popup is; the pause is current again once it is gone.
    bool AchievementsPopupOpen() const { return m_achievementsPopup.open; }
    const std::string& AchievementsPopupText() const { return m_achievementsPopup.text; }
    const Popup::Open* AchievementsPopupState() const {
        return m_achievementsPopup.open ? &m_achievementsPopup.state : nullptr;
    }

    // The level's FRAME clock: its age plus every millisecond game time stood
    // still under a pause or a popup, in the script's milliseconds. What the original
    // times by a frame clock neither stops and reads from GetLastFrameElapsedTime - the
    // level-start caption and the hint (ETHTextDrawer::Draw, handed the floored uint by
    // ETHEngine::DrawTopLayer, ETHEngine.cpp:205), the plaque's dismissal (m_uiTime) -
    // reads this; what runs on game time reads LevelAgeMs.
    double LevelFrameMs() const { return m_levelAgeMs + m_stoppedMs; }
    // The same ticks in WALL milliseconds, a pause and a popup counted too: what the
    // level's two blacks (FadeInControllers, which read sys:GetTime) fade on. Not the
    // script's: a tick is the engine's 16.667 here, where LevelFrameMs gains 16.
    double LevelFrameWallMs() const { return m_levelFrameWallMs; }

    // Whether game time is stopped: a pause or a popup is up.
    bool GameTimeStopped() const { return m_pause.open || m_popup.open; }

    // ---- the tutorial and help popups -------------------------------------------
    //
    // ETHFramework's Popup over the level, laid out by sim/Popup.hpp from ui.json's
    // popups block: 1-02 and 1-03 raise one as they load, and thirteen levels from
    // a help block, on a touch released inside it. It STOPS GAME TIME as the pause
    // does, for as long as it is up and through its fade out, while its own
    // entrance and demonstration run on its clock; a touch down anywhere, or the
    // back key, closes it. MenuScreen() stays Screen::None under it.
    bool PopupOpen() const { return m_popup.open; }
    const Popup::Rules& PopupRules() const { return m_popupRules; }
    // The popup that is up, and its class; null when none is.
    const Popup::Open* OpenPopup() const { return m_popup.open ? &m_popup.state : nullptr; }
    const Popup::Class* OpenPopupClass() const { return m_popup.open ? m_popup.cls : nullptr; }
    // Close the popup that is up, as a touch down does. False when none is up or
    // it is already closing.
    bool ClosePopup();
    // The level's help blocks' touch rectangles on the view, as the last tick
    // left the camera, in design units; empty in a level with none.
    std::vector<Hud::Rect> HelpBlockRects() const;

    // DEV ONLY: a touch at a point of the view - given as fractions of it - pressed
    // on the layer's tick `tick` and released on `releaseTick` (the next, when that
    // is not after `tick`), held where it went down in between, for a --fixed-step
    // capture. With no point it lands on the centre of the level's first help
    // block, wherever the camera has it on that tick. The popups and the menu
    // states read it: it opens a help block's popup, closes one that is up, and
    // presses a menu's buttons as a touch does. During play it is the tick's touch
    // readInput hands on, so it works a HUD control, presses a help block or - on
    // its RELEASE, as the original fires (placement.json `release`) - fires the
    // shot, exactly as a real touch down and up at that point of the view would.
    // It never moves, so it always fires if nothing takes it; a drag (below) that
    // strays more than scale(18) does not. The pause and the end screens still
    // read the mouse alone; the pause's buttons have ScheduleDevPress.
    void ScheduleDevTap(int tick, std::optional<glm::dvec2> viewFraction, int releaseTick = 0);
    // DEV ONLY: a drag - a touch down at `from` on tick `tick`, moved at an even pace
    // to reach `to` on `releaseTick`, and released there. Both are fractions of the
    // view. The credits' and the dashboard's scroll captures need a finger that moves.
    // In a level, its release fires a shot only if it strayed no more than scale(18),
    // aimed where it was on the tick before its release (getReleasePos).
    void ScheduleDevDrag(int tick, const glm::dvec2& from, const glm::dvec2& to, int releaseTick);

    // DEV ONLY: press one of these on the layer's tick `tick` (1 the first
    // OnFixedUpdate), as a tap would, for a --fixed-step capture that has no
    // input. `Pause` is the in-level pause control; `Back` the back key on a menu
    // state; the rest are the pause's own buttons. A press is taken on the first
    // tick at or after its own on which what it presses is there to press.
    enum class DevPress { Pause, Levels, Resume, Skip, Achievements, Sound, Music, Back };
    void ScheduleDevPress(int tick, DevPress press);

    // DEV ONLY: hold a walk from tick `from` to tick `to` inclusive, as a held
    // arrow would: -1 left, 1 right. A --fixed-step capture has no input, and the
    // finished and lost screens need a walk into a door or a hazard.
    void ScheduleDevHold(int from, int to, float direction);

    // DEV ONLY: burn 4-32's dark dragon on the layer's tick `tick`, as a barrel's
    // blast within its reach does (Game's blast test sets the same flag). Its
    // wound and its death are reached only by portalling the level's one barrel
    // into its own fireball's path, which no scripted input does, and its flash
    // and its white death are captures the visuals plan asks for. Ignored on a
    // tick the dragon is not there to burn.
    void ScheduleDevBurn(int tick);
    // DEV ONLY: break the breakable `node` on the layer's tick `tick`, as a stone's
    // touch breaks it (Demolish::Break) - for a suite: a carranca, which no level's
    // play breaks in the port and whose carranca_destroy (planning step 146) is
    // drawn wherever one goes. Ignored for a node Demolish does not hold.
    void ScheduleDevBreak(int tick, const std::string& node);
    // DEV ONLY: the player's body put at a point of the level, its velocity zeroed,
    // on the layer's tick `tick`, after that tick's physics step and before what
    // judges where the step left it - so a capture can open a level's door and film
    // what comes after (planning step 123's cutscenes). A capture aid, never a route:
    // a win it makes is not a win a player made. Ignored with no level loaded.
    void ScheduleDevPlace(int tick, const glm::dvec2& atPx);
    // The layer's own tick count, which every DEV schedule above is written in: 1 on
    // the first OnFixedUpdate.
    int LayerTicks() const { return m_ticks; }

    // ---- how a level ends -----------------------------------------------------
    //
    // The beat after the door or the death, the HUD going, and the finished and
    // lost screens over the level that goes on running (sim/LevelEnd.hpp, from
    // ui.json's level_end block).
    const LevelEnd::Rules& LevelEndRules() const { return m_levelEndRules; }
    // Whether the door was reached, or the player killed, on this level.
    bool Finishing() const { return m_finishing; }
    // The door to the medal on this level: its end_delay's time where it places one
    // (Cutscene::EndDelayMs), level_end's won_delay_ms otherwise.
    double WonDelayMs() const { return m_wonDelayMs; }
    bool Dying() const { return m_dying; }
    // Game time since the door or the death: what the beat is counted in.
    double EndedMs() const { return m_dying ? m_dyingClockMs : m_finishClockMs; }
    // The pads' alpha byte as it decays from the end on, once a tick.
    int EndPadAlphaByte() const { return m_padEndByte; }
    // How long the finished or lost screen has been current, in milliseconds of
    // the tick's clock: zero on the tick it came up.
    double EndScreenClockMs() const { return m_end.clockMs; }
    // What the finished screen scores, and where its two counters have got to.
    const LevelEnd::Play& EndPlay() const { return m_end.play; }
    int PortalsCounted() const { return m_end.portals.current; }
    int CrystalsCounted() const { return m_end.crystals.current; }

private:
    // A body the level built, and the box standing for it. The box sits at the
    // body's shape, offset from its entity in the body's own frame.
    struct Drawn {
        entt::entity body{entt::null};
        entt::entity box{entt::null};
        glm::dvec2 offsetPx{0.0};
        glm::dvec2 sizePx{0.0};
        float depth{0.4f};
    };

    // A body a launcher threw, and the box and the sprite standing for it.
    struct ThrownBox {
        entt::entity body{entt::null};
        entt::entity box{entt::null};
        entt::entity quad{entt::null};
        glm::dvec3 emissive{0.0}; // the thrown .ent's, from launchers.json
    };

    // SpaceSky's four (sim/Sky.hpp): the space sky, the spiral, the planet and the
    // eclipse.
    enum class SpaceRole { None, Sky, Spiral, Planet, Eclipse };
    // 1-21's two lesson pictures (sim/Hint.hpp).
    enum class HintRole { None, Hand, Arrow, Lesson };

    // One of the level's sprites as a textured quad, and what it follows: the
    // crystal, static portal or no-portal zone it pictures, or else its node's
    // body. Scenery follows nothing.
    struct DrawnSprite {
        Sprites::Sprite sprite;
        entt::entity quad{entt::null};
        float z{0.0f};
        // C, the instance colour, as its node gives it (eth_color), and a
        // keyhole's fade, which multiplies its alpha. What the ambient does to
        // them is syncLighting's. A timed crystal does not fade: its dial counts
        // it down (m_timers).
        glm::vec4 colour{1.0f};
        float fade{1.0f};
        // Where its picture was last drawn, in the level's pixels.
        glm::dvec2 centrePx{0.0};
        // The scale a script gave its entity (ETHEntity::Scale), which multiplies its
        // picture and its particle systems: a static portal's 0.8, an antiportal's
        // manager's 0.5 times its node's scale, and 1 otherwise.
        double scale{1.0};
        glm::dvec3 emissive{0.0}; // its node's eth_emissive
        std::string lightmap;     // its node's eth_lightmap, on disk; empty = none
        // Which lights reach it and how (Lighting::ReceiverMask): its node's
        // eth_static, eth_apply_light, eth_normal (on disk; empty = none), and its
        // eth_z, the original's unrounded depth, which is its lighting height.
        bool isStatic{false};
        bool applyLight{false};
        std::string normal;
        double lookZ{0.0};
        // Its node's eth_particles: the sidecar its own <Particles> were written to,
        // on disk; empty = none, and it carries no particle system (planning step
        // 91: its entity's .ent is not read in its place).
        std::string particles;
        // Where its entity stands now, in the level's pixels: its node's position,
        // or its body's, or its patrolling zone's. What a light it owns follows.
        glm::dvec2 ownerPx{0.0};
        // And its angle, Ethanon's (counter-clockwise positive): its node's, or its
        // body's. What its particles are turned by.
        double ownerAngleDeg{0.0};
        entt::entity body{entt::null};
        int key{-1};          // in keys.keys: drawn where the key is, carried or not
        int keyhole{-1};      // in keys.keyholes: faded as it fades, and gone with it
        int diamond{-1};      // in diamonds.diamonds: drawn where it is, gone once spent
        int crystal{-1};      // in goals.crystals
        int staticPortal{-1}; // in portals.statics
        int zone{-1};         // in portals.zones: gone with its zone
        int agent{-1};        // in portals.agents: where it patrols, gone once a shot kills it
        int sky{-1};          // in m_sky.skies: pinned to the camera, not where the level put it
        // A sky narrower than the view: its picture again, behind it, out to the
        // frame's pixel edges (sky.json pixel_cover, placeSkyCover). Null otherwise.
        entt::entity cover{entt::null};
        bool satellite{false}; // m_sky's satellite, pinned the same way
        // Which of SpaceSky's four it is, each moved from the camera every frame.
        SpaceRole space{SpaceRole::None};
        int motion{-1};       // in m_motions: a crystal or key its script bobs
        bool antiportal{false}; // ETHCallback_antiportal blinks and turns it (art.json)
        int placed{-1};       // in m_placed: a picture its own callback sways, fades or turns
        // 1-21's hand or arrow (sim/Hint.hpp): moved, faded, pulsed and deleted by
        // m_hintHand's or m_hintArrow's callback; or 2-20's lesson hand, by m_lesson's.
        HintRole hint{HintRole::None};
        // A draggable handle, in m_level.handles.handles: drawn where it is dragged,
        // at its callback's alpha and bounce; or a fire agent or reflector that
        // follows one, in m_level.handles.followers: picture, light and particles
        // where it has got to (sim/Handle.hpp).
        int handle{-1};
        int follower{-1};
        // A lift, in m_level.movers.lifts: ETHCallback_lift blinks its colour every
        // frame (movers.json lift.blink), whatever it is doing.
        int lift{-1};
        // Chapter 4's torch, in torch.lights: its picture goes on the tick after the
        // torch is lit, as the original's entity deletes itself (m_torchWatch).
        int torch{-1};
        // A gravity well's agent, in wells.wells: a lap turns it (m_wellSpins).
        int well{-1};
        // A shock agent, in fields.fields: its callback's bounce pulses its picture
        // (art.json shock_ring agent_pulse, planning step 146).
        int field{-1};
        // The size it was last drawn at, in the level's pixels (DrawnPicture).
        glm::dvec2 drawnSizePx{0.0};
        // 4-19's red key state (sim/RedKey.hpp): drawn at its first call's scale per
        // axis and colour, that colour faded once its key is taken, gone with it.
        bool redKeyState{false};
        // 2-32's boss: its node stands off the left edge of the level and its
        // callback flies it, turns it and cuts its sheet (sim/Dragon.hpp), and the
        // claw's rides the camera. Both are drawn from the sim, not from the node.
        bool dragon{false};
        bool claw{false};
        // Game::loop bobs it on a `no_gravity` level: the exit door, the black
        // behind it, and the glow added behind that (m_doorBob, art.json `door`).
        bool doorBob{false};
        // A crate manageNonTeleportable blinks (art.json non_teleportable): which of
        // its two it writes; the crate's place in fire.burnables, whose `burned`
        // stops it (-1: nothing burns it); and the level's age it last blinked at,
        // which a burnt crate's emissive keeps.
        Art::NonTeleportable::Blinks blinks{Art::NonTeleportable::Blinks::Nothing};
        int burnable{-1};
        bool blinked{false};
        double blinkMs{0.0};
    };
    // A placed picture's callback (motions.json `placed`): its row, its
    // linearMotion when it has one, and its turn.
    struct PlacedScript {
        std::size_t row{0}; // in m_motionRules.placed
        bool moves{false};
        Motion::Linear motion;
        Motion::Turn turn;
        // A tutorial hand a release came within reach of: its DeleteEntity
        // (ETHCallback_hand_tap #165-171), its smoke queued (tapHandsAway).
        bool tapped{false};
    };

    void bindInput();
    void buildCamera(entt::registry& registry);
    void buildDrawables(entt::registry& registry);
    void buildSprites(entt::registry& registry);
    // The four black bars round the scene (m_blackBars), with the level's art.
    void buildBlackBars(entt::registry& registry);
    void buildHud(entt::registry& registry);

    // The menu: opened onto a screen, drawn from its buttons, and taken down
    // again. A level is unloaded first, so the two are never both in the
    // registry.
    void openMenu(entt::registry& registry, Screen screen);
    // The medal screen, over the level that was just finished: unlike every
    // other screen this KEEPS the level, which goes on running under it.
    void openFinished(entt::registry& registry);
    void layOutMenu();
    void buildMenu(entt::registry& registry);
    // A menu state becomes current: its clock from nothing, no touch followed.
    void beginMenuState();
    // The loading screen's quads placed for this tick, and its hold counted.
    void loadingTick(entt::registry& registry);
    // The main menu's touch, as Button::update reads it.
    void mainMenuInput(entt::registry& registry);
    // Chapter select's and the grid's touch, the same way, on their world quads.
    // Chapter select and the grid: a touch on a tile or an arrow, the finger on
    // the pager, the back key, and one tick of the pager.
    void selectorInput(entt::registry& registry);
    // A popup of `cls` over the menu state that is up (chapter select's
    // GameFinishedPopup), its clock at the state's.
    void openMenuPopup(const Popup::Class& cls);
    // One tick of it, in selectorInput's place while it is up: its close, and the
    // pager's slide, which PageManager::draw decays and no layer switch stops.
    void menuPopupTick(entt::registry& registry, const ScriptClock::Step& step);
    // The credits' touch and the strip's tick (CreditsScreenLayer::update).
    void creditsInput(entt::registry& registry, const ScriptClock::Step& step);
    // The dashboard's touch, wheel and tick (ScoreDashboard::loop, DashboardLayer).
    void dashboardInput(entt::registry& registry);
    // openState(world, level), from the dashboard's start button.
    void openAchievement(entt::registry& registry, int world, int level);
    // Whether a menu state is up, instead of a level or over one.
    bool menuStateUp() const;
    // A picture of the original's that the menu states draw, resolved once; empty
    // when it cannot be read.
    const std::string& menuPicture(const std::string& file) const;
    // A font of the original's, read once for the run.
    void loadUiFont(const std::string& name);
    void unloadMenu(entt::registry& registry);
    // Just the drawables, which a rebuilt screen replaces but a click on a
    // level takes away for good.
    void unloadMenuDrawables(entt::registry& registry);
    void menuTick(entt::registry& registry, const ScriptClock::Step& step);
    // One of the original's own menu images, which live beside its entities.
    std::string menuImage(const std::string& file) const;

    // The entities' particle systems: built with a level's art, carried per
    // FRAME, and taken away with the level.
    // Both are defined with the members that hold them, further down.
    struct Emitter;
    struct Effect;

    void buildEmitters(entt::registry& registry);
    void unloadEmitters(entt::registry& registry);
    void updateEmitters(entt::registry& registry, float deltaTime);
    // One emitter's frame: its pool stepped and each particle's quad placed or
    // put away. `pool` says what becomes of the quad of a particle that is not
    // drawn this frame - kept and made invisible for an effect's emitter, which
    // spawns and spends particles by the dozen, destroyed for a level's, as step
    // 59 built them and the suites count them.
    //
    // `scaleAfterStep` is an ETHEntity::Scale another entity's callback makes this
    // frame, after the owner's own update stepped its particles and before the
    // draw (ETHScene.cpp:467-488): each particle is stepped on the old system, then
    // its size and velocity scaled, then drawn; the system is scaled last.
    void stepEmitter(entt::registry& registry, Emitter& emitter, double elapsedMs, const Particles::Random& random,
                     bool pooled, double scaleAfterStep = 1.0);
    // A number in [from, to) from the layer's OWN generator.
    double particleRandom(double from, double to);
    // And from the one the EFFECTS draw on (00_order K8): every non-placement
    // draw, so that adding an effect mid-level cannot move a single particle of
    // the level's own emitters.
    double effectRandom(double from, double to);

    // The effects runtime (system_7 7a-2). Spawned on the frame, updated on the
    // frame, and gone with the level - nothing here reaches Game::Level, the
    // tick or the state hash, exactly as the particles do not.
    void updateEffects(entt::registry& registry, float deltaTime);
    void unloadEffects(entt::registry& registry);
    void freeEffect(entt::registry& registry, Effect& effect);
    // Where an effect's owner stands this frame: false when the owner is gone
    // (the effect is freed), with `hidden` for the player in a door, which the
    // original hides its effects with rather than deleting them.
    bool effectOwnerPoint(const entt::registry& registry, const EffectOwner& owner, glm::dvec2& atPx,
                          bool& hidden) const;
    // What the TICK adds to the frame's queue, each rule wired by the step that
    // draws it. `fixedDelta` is the tick's length, which a spent key's fly-in
    // clock counts (Effects::AdvanceFlyIn); `registry` is read for the player's
    // point, where a shock ring's kill adds shock_death.
    void latchSimEffects(const entt::registry& registry, const ScriptClock::Step& step);
    // Chapter 4's torches, their signal and their wall (the visuals plan's 11.2),
    // from latchSimEffects: what lighting a torch, putting it out and the wall
    // going add, each on the tick its state turns over.
    void latchTorches();
    // THE SMALLER PICTURES (the program's UI, planning step 146), from
    // latchSimEffects: the three bosses' dying arms' scattered
    // small_explosion_no_light on each one's own overTimeEntityAdder clock, the
    // beholder's and the ghost's deaths' explode, and ghost_utility_spawn wherever
    // a boss puts its weapon back or calls an escort - each in its callback's own
    // order, before the key a boss drops is queued. And the dark dragon's death,
    // after that key: ghost_utility_spawn beside it, then its explode.
    void latchBossPictures(const ScriptClock::Step& step);
    void latchDarkDragonDeath();
    // ETHCallback_hand_tap's test of a release (effects.json smoke_vanish_hand_tap),
    // from readInput on the tick a touch comes up: each tutorial hand within reach
    // of where it was last held, off the walk pads, goes with its smoke.
    void tapHandsAway(entt::registry& registry, const Portals::TouchRelease& release);
    // The depth a marker of the level stands at: the first top-level node whose
    // entity is `entity`, its eth_z (0 where it states none, or there is none).
    double markerDepth(const std::string& entity) const;
    // What a tap the shot was offered leaves behind (the visuals plan's 10.1),
    // queued the moment Portals::State::Shoot returns: the mark a fired shot
    // leaves at the tap - red_cross or portal_mark by computePortalFinalPos's
    // willDie (R15), and red_waves (A) where that adds them - the hourglass for a
    // tap refused while a shot flies, and red_waves (B) with its shake for one
    // refused for a budget of 0 on a level with the no-portal sign.
    void queueTapEffects(entt::registry& registry);
    // THE LEVEL'S END AND A DEATH (the visuals plan's 11.3), queued by the tick
    // that ends the level: levelFinishedEffect's suck at the door and sparkles at
    // the character, or checkGameLost's blood (killed) or skull (fallen). Called
    // from OnFixedUpdate's two branches, which are where the port decides either,
    // while the body still stands where it went.
    void latchFinishEffects(entt::registry& registry);
    void latchDeathEffects(entt::registry& registry);
    // THE WIN CUTSCENES' CALLBACKS, once a tick from the tick the level ends: the
    // launch and the landing while it is won, the knight's spawn once it is
    // finished and the knight's own update from the tick after (Cutscene.hpp). Their
    // tracks, their shake and the knight's door come from here; what they move is
    // placed by syncSprites, advanceMotions and syncKnight.
    void stepCutscenes(entt::registry& registry, double frameMs);
    // 2-32's knight: minion.png on its own cut, black, at its body.
    void syncKnight(entt::registry& registry);
    // Where the door's entity is this tick: its node, the zero-g bob and the
    // cutscene's offset - the point the landing puts the character on.
    glm::dvec2 doorPointPx() const;
    // The view's top-left corner and its size, on the level's axis - the
    // original's GetCameraPos() and GetScreenSize(), which the skull is held
    // inside - as the last tick left the follow.
    glm::dvec2 cameraCornerPx() const;
    // Every placed portal, drawn as insertPortal adds it and killed as
    // ETHCallback_portal kills it: the summon bubble and the portal itself on a
    // new id, the manager's colour each frame, and on an id's disappearance the
    // fade, portal_death.ent and the end of it. Per FRAME, beside updateEffects.
    void syncPlacedPortals(entt::registry& registry, double frameMs);
    // A live effect by the id SpawnEffect returned, or null once it has been freed.
    Effect* effectById(int id);
    // One decoded call site turned into something to add: sim/Effects says which
    // .ent, where, turned how and at what scale, and this says at what depth on
    // the LEVEL's axis (Effects::Spawn::zScaled; the visuals plan's 7.3).
    EffectSpawn effectAdd(const Effects::Spawn& spawn) const;
    // The player's red eyes (effects.json red_eyes; the visuals plan's 7.1).
    // MainCharacter's constructor adds the entity at the level's start, so
    // buildEyes runs at the end of buildDrawables - after the player quad, whose
    // cell it reads - and updateEyes moves it onto the face every frame, so
    // syncEyes runs beside updateEffects.
    void buildEyes(entt::registry& registry);
    void syncEyes(entt::registry& registry);
    // The character's cell in its 4 x 4 sheet - ETHEntity::GetFrame, which is the
    // player quad's firstFrame + frame. The last one seen while the quad is gone.
    int playerDrawnFrame(const entt::registry& registry) const;
    // A light and its halo placed the one way (Lighting::OwnerScaled): a level's
    // placement passes an owner scale of 1, and what a script adds passes its
    // <Scale> times the script's (00_order K7).
    void placeLight(entt::registry& registry, entt::entity light, entt::entity halo, const Lighting::Light& unscaled,
                    const glm::dvec2& ownerPx, double ownerZ, bool ownerStatic, double ratio, float haloZ,
                    bool present, const glm::dvec2& ownerScale) const;

    // ---- the between-slot table (00_order K13), read by every user ----------
    //
    // `levelZ` is a depth in the LEVEL's units (art.json depth). The class stands
    // after every sprite at or below it - and after the player only where the
    // player is at that depth or deeper, which is the rule step 76 wrote when the
    // player moved to z 4.
    float slotAfter(double levelZ, Slot klass) const;
    // The same, for a slot already counted: the player's.
    float slotAt(int slot, Slot klass) const;
    // And against a picture's own z, for what hangs off it: a halo over its owner.
    float slotOver(float pictureZ, Slot klass) const;
    // An entity's system `slot` of `systems`, which share the quarter that
    // `klass` starts at so that none of them ties with another
    // (Particles::SlotFraction).
    float systemZ(float base, Slot klass, int slot, int systems) const;
    float readInput(entt::registry& registry);
    void syncDrawables(entt::registry& registry);
    void syncSprites(entt::registry& registry);
    // The picture of a barrel a restock built from the port's template table (3-32's,
    // whose level places none to borrow one from). False when the table's picture
    // does not read.
    bool templatePicture(const Restock::Added& added, DrawnSprite& made) const;
    // The timed crystals' dials, after syncSprites has placed their crystals: on
    // the tick, as ETHCallback_timer runs a frame.
    void syncTimers(entt::registry& registry);
    // The ghost of a flagged light wall, made and unmade as the original adds and
    // deletes the entity, and placed at the pulse advanceMotions left it at.
    void syncWallGhost(entt::registry& registry);
    // The shock rings (m_shockRings): made with the level's art, after every
    // slotAfter the pictures in m_sprites take and before the door glow is pushed.
    // ShockRingQuad is defined with the member that holds them, further down.
    struct ShockRingQuad;
    void buildShockRings(entt::registry& registry);
    // On the tick: each ring to its field's point at the turn advanceMotions left,
    // and gone with its agent.
    void syncShockRings(entt::registry& registry);
    // On the frame, pause or not: each standing ring's scale drawn afresh about its
    // base, from m_effectRandom (ETHCallback_shock_area #51-66).
    void jitterShockRings(entt::registry& registry);
    // One ring's quad where its field, its turn and this frame's factor put it.
    void placeShockRing(entt::registry& registry, const ShockRingQuad& ring);
    // One linearMotion call for every crystal and key whose script makes one this
    // tick (sim/Motion), and one call of every placed picture's callback, BEFORE
    // syncSprites draws where they leave them.
    void advanceMotions(const ScriptClock::Step& step);
    // Whether a picture's script calls linearMotion now: a crystal while it lives,
    // a key while no one carries it and it has not found its keyhole.
    bool motionRuns(const DrawnSprite& drawn) const;
    void syncBoss(entt::registry& registry);
    // The minions' frames, once a level step after Minions::Tick has chosen each
    // one's arm (Art::UpdateMinion), and their pictures where their bodies stand.
    void advanceMinions(const ScriptClock::Step& step);
    void syncMinions(entt::registry& registry);
    // Chapter 3's boss, as the tick left its Look: made when the knight's death adds
    // it, unmade when it has gone (planning step 98).
    void syncGhost(entt::registry& registry);
    // One MinionQuad per minion the sim holds, a summoned escort's added when it is.
    void matchMinions();
    // The carrancas' fireballs. Its own function rather than a block inside
    // syncBoss: they belong to a turret, not to the beholder, and every level
    // of chapter 2 that has one has no boss at all. Their PICTURES are effects
    // (latchSimEffects); what this places is the placeholder box behind them.
    void syncTurrets(entt::registry& registry);
    // Every sprite of the level coloured for the ambient light now: after
    // everything above has made, unmade and placed this tick's quads.
    void syncLighting(entt::registry& registry);
    // What a sprite is to the lights: whether it applies light and is static
    // (Lighting::ReceiverMask), its normal map on disk, and its lighting height
    // in the original's units. The default takes no light.
    struct Receiver {
        bool applyLight{false};
        bool isStatic{false};
        std::string normal;
        double z{0.0};
        bool runtimeBake{false}; // the level has baked at run time (Lighting::RuntimeBake)
    };
    // What an added entity's picture takes from the level's light, as
    // syncLighting's own receiverOf gives one to a picture the level placed.
    Receiver effectReceiver(const Effect& effect);
    // A quad's light: its colour C as albedoColor, min(1, ambient + emissive) as
    // its 2D sprite's ambient, its lightmap as the overlay, its height, normal map
    // and light mask, and a mixed blend made premultiplied - or, on a level whose
    // lighting did not read, C alone on the plain unlit path. Each written only
    // when it changes. The one place a level sprite's colour is written.
    void tint(entt::registry& registry, entt::entity quad, const glm::vec4& colour, const glm::dvec3& emissive,
              const std::string& lightmap = {}, const Receiver& receiver = {}) const;
    // The level's lights and their halos, one per Lighting::Look::light, made with
    // the sprites and their particles.
    void buildLights(entt::registry& registry);
    // The level's lights, their halos, and the shot's light and halo, taken away.
    void unloadLights(entt::registry& registry);
    // Every light and halo placed at its owner, on or off as its owner is there
    // or not, in the colour its owner's particles give it now; and the shot's
    // made, placed and unmade with the shot. On the tick, after the sprites, so
    // a suite that only ticks sees them; and on the frame, after the particles,
    // which is when their count changes.
    void syncLights(entt::registry& registry);
    // The live share of the particles of the emitter at `emitter` in m_emitters
    // (Lighting::ParticleRatio): 1 when there is none.
    double particleRatioOf(int emitter) const;
    // Hands the held lightmaps back to the texture registry, when there is one.
    void releaseLightmaps(entt::registry& registry);
    void updateHud(entt::registry& registry);

    // The level at `index` in chapters.json, in place of whatever was there.
    // False, with LoadError, when it is refused; nothing of it is left then.
    bool loadLevel(entt::registry& registry, int index);
    // `keepLightmaps` for a retry, which draws the same level again at once.
    void unloadLevel(entt::registry& registry, bool keepLightmaps = false);
    // The level at `next`, or, when it is -1, goToNextLevel's chapter end: chapter
    // select on the chapter's page, and GameFinishedPopup over it after the last
    // chapter's last level.
    void goTo(entt::registry& registry, int next);
    // The exit reached: the medal computed and saved on the door's own tick, as
    // checkGameEnd's writeScore does it; the screen comes later (openFinished).
    void recordWin();

    // ---- achievements (planning step 143) -----------------------------------------
    // AchievementManager::unlock: nothing for an id the store has; else the id
    // added and saved, and with `notify` its card onto the Notificator's stack.
    void unlockAchievement(int id, bool notify, double delayMs, const char* why);
    // The door's tick, after recordWin's medal: registerMedalAchievements(world,
    // true) and checkForLevelAchievements with this play's numbers.
    void achievementsAtDoor(const entt::registry& registry, double gameTimeMs);
    // registerMedalAchievements(world, notify) over the saved medals.
    void achievementsForChapter(int world, bool notify);
    // checkPreviouslyUnlockAchievements: every chapter and level, silently.
    void achievementsMenuPass();
    // The red key's pick (ETHCallback_key #822-851), on the edge of its carrier.
    void achievementsForKeys();
    // The Notificator's update, and what a card shown or gone asks of the music.
    void tickNotificator(double dtMs);
    void setMusicVolume(double volume);
    void setDefaultMusicVolume();
    // The pause's AchievementsPopup: opened over it, ticked in its place, drawn over it.
    bool openAchievementsPopup();
    void achievementsPopupTick(entt::registry& registry, double dtMs);
    void emitAchievementsPopup(Supersonic::ScreenOverlay& overlay, const glm::dvec2& view) const;

    float viewportAspect(const entt::registry& registry) const;
    void placeCamera(entt::registry& registry);

    entt::entity makeBox(entt::registry& registry, const char* tag, const glm::vec3& centre, const glm::vec3& size,
                         const glm::vec3& colour);
    void placeBox(entt::registry& registry, entt::entity box, const glm::dvec2& centrePx, const glm::dvec2& sizePx,
                  float z, float depth, float rotation) const;
    entt::entity makeSprite(entt::registry& registry, const char* tag, const std::string& texture, bool additive);
    void placeSprite(entt::registry& registry, entt::entity quad, const glm::dvec2& centrePx,
                     const glm::dvec2& sizePx, float z, float rotation) const;
    // A sky's cover, from where its sky was just placed (sky.json pixel_cover):
    // across, the sky's span moved out to the frame's pixel edges (Sky::PixelCover),
    // with its uv stretched to match, so every texel lands where the sky draws it;
    // down, the sky's own rows; just behind it (Slot::SkyCover).
    void placeSkyCover(entt::registry& registry, const DrawnSprite& drawn, const glm::dvec2& centrePx,
                       const glm::dvec2& sizePx, float rotation) const;
    // An image's size, read once per file (Sprites::ImageSize); zero when it
    // cannot be read.
    glm::dvec2 imageSizePx(const std::string& path);
    // An image the port draws for itself, by the name its data gives: the file the
    // original's loader draws for it (the tier file where one exists, Sprites::Drawn
    // over m_tierRules), and its size in whole units. Read once per name; texels and
    // units 0 when it cannot be read, with `path` the file tried. The player, the
    // portal, the shot, the beholder and its spikes, a thrown stone, the loading
    // screen's character, portal and halo, their halos and normal maps, and every
    // particle bitmap (00_order 2.3). The menus and the HUD keep their own lookups
    // (menuImage, originalAsset).
    const Sprites::Image& drawnImage(const std::string& named);
    // One frame of a sheet in units, as the original cuts it (Tiers::FrameCut:
    // whole units, truncated after the divide); zero when it cannot be cut.
    static glm::dvec2 frameUnits(const Sprites::Image& sheet, int columns, int rows);
    // The player's frame in units: its sheet's tier file cut by its columns and rows.
    glm::dvec2 playerCellPx(bool noGravity);
    // An image among the original's entities, by name (drawnImage finds the file drawn).
    std::string originalImage(const std::string& sprite) const;

    Paths m_paths;
    std::string m_startLevel;

    Chapters::Table m_chapters;
    int m_current{-1}; // the entry in m_chapters being shown
    std::optional<Cleared> m_lastCleared;
    int m_deaths{0};

    Camera::Rules m_cameraRules;
    double m_viewHeightPx{0.0};
    float m_aspect{16.0f / 9.0f}; // the window's shape as of the last tick
    Camera::Follow m_follow;
    // THE POINTER AS THE CAMERA READS IT (camera.json `drag`): its own record of the
    // one touch, followed on every tick the touch is read - the level's and a
    // popup's - because the original's camera reads the engine's input raw, whatever
    // takes the touch. readInput leaves the tick's feel and whether the walk pads
    // moved; the camera's tick takes them and clears them, so a tick that read no
    // input (a stop's resuming half, the door's beat) drags nothing.
    Camera::TouchRecord m_cameraTouch;
    Camera::Feel m_cameraFeel;
    bool m_cameraPadMoving{false};

    // 1-21'S LESSON (camera.json `hint`, sim/Hint.hpp): the hand's and the arrow's
    // callbacks, and whether the level places the hand, whose first call draws the
    // line of text.
    Hint::Rules m_hintRules;
    Hint::Hand m_hintHand;
    Hint::Arrow m_hintArrow;
    bool m_hintHandPlaced{false};
    bool m_hintArrowPlaced{false};

    // 2-20'S LESSON (handle.json `lesson`): hand_waypoint0's callback, and the
    // handle it watches (in m_level.handles.handles; -1 none).
    Handle::Lesson m_lesson;
    int m_lessonHandle{-1};
    // The handles' reading of the touch (ETHCallback_arrows): getRelativeHitPos,
    // the point of the level it went down on through the camera the tick after it
    // went down left - the portal manager sets it after the camera on the HIT
    // frame. Empty from a release until the next DOWN tick.
    std::optional<glm::dvec2> m_handleHitPx;
    // The script's milliseconds of this tick (ScriptClock::Ms of m_step), for what
    // readInput hands a callback's clock.
    double m_tickMs{0.0};
    // The original's script clock (clock.json) and this tick's two deltas: the sim's `dt`
    // is the floor of a frame's milliseconds and the physics step is the engine's own
    // tick; m_layerClockMs below advances by the engine's tick, as sys:GetTime does
    // (sim/ScriptClock.hpp, planning step 147).
    ScriptClock::Rules m_clockRules;
    ScriptClock::Step m_step;
    // Where a picture is drawn between ticks (pictures.json), and what the layer has seen of
    // the display's rate to decide it (sim/Pictures.hpp, planning step 150).
    Pictures::Rules m_pictureRules;
    Pictures::RateWatch m_pictureRate;

    Game::Data m_data;
    int m_dataIndex{-1}; // whose level m_data holds, so a retry reuses it
    // The level's lighting, read with m_data and reused with it.
    Lighting::Scene m_look;
    bool m_lit{false}; // m_look read, so sprites are coloured for the ambient
    std::string m_lightingError;
    glm::dvec3 m_ambient{1.0};
    std::vector<std::string> m_heldLightmaps;
    std::size_t m_lightmapsHandedBack{0};
    Game::Level m_level;
    bool m_loaded{false};
    std::string m_loadError;
    glm::dvec2 m_boundsPx{0.0}; // the level_bounds marker: the level runs from (0, 0) to here

    entt::entity m_camera{entt::null};
    entt::entity m_light{entt::null};
    std::vector<Drawn> m_bodies;
    entt::entity m_player{entt::null};
    std::vector<entt::entity> m_buttons;  // one per Puzzle button, in its order
    std::vector<entt::entity> m_crystals; // one per crystal, null once collected
    entt::entity m_exit{entt::null};
    std::vector<entt::entity> m_portals;  // one per placed portal
    std::vector<entt::entity> m_statics;  // one per static portal, null once spent
    std::vector<entt::entity> m_zones;    // one per no-portal zone, hidden once gone
    std::vector<entt::entity> m_agents;   // one per anti-portal agent, at its reach; hidden once killed
    std::vector<entt::entity> m_hazards;  // one per hazard, at the box that kills
    std::vector<ThrownBox> m_thrown;      // one per thrown body not yet taken back
    entt::entity m_shot{entt::null};      // the portal shot in flight, when there is one

    std::vector<DrawnSprite> m_sprites; // the level's art, in drawing order
    bool m_artReady{false};             // the level's art was read, and is drawn
    std::string m_artError;
    bool m_showBoxes{false};
    int m_playerSlot{0}; // the drawing slot the player takes among the sprites
    std::map<std::string, glm::dvec2> m_imageSizes;
    std::map<std::string, Sprites::Image> m_drawnImages; // drawnImage's, by the name asked for
    Art::Rules m_artRules;                  // the port's art.json
    Sky::Rules m_skyRules;                  // the port's sky.json
    Tiers::Rules m_tierRules;               // the port's tiers.json: which file of a level's image is drawn
    Sky::Controller m_sky;                  // the level's StaticSky, built with its sprites
    Sky::SpaceRules m_spaceRules;           // the port's sky.json, space_sky
    Sky::Space m_space;                     // the level's SpaceSky, on the 19 space_bg levels
    Cutscene::Rules m_cutsceneRules;        // the port's cutscenes.json
    Motion::Rules m_motionRules;            // the port's motions.json
    Effects::Rules m_effectRules;           // the port's effects.json: the calls that add an entity
    // One per crystal and key whose picture is drawn, in the level's drawing order
    // (DrawnSprite::motion). Picture-side only: never Game::Level or the state hash.
    std::vector<Motion::Linear> m_motions;
    // One per drawn picture a `placed` row names (DrawnSprite::placed). Picture-side
    // only, like m_motions; its start angles are constants and draw from no stream.
    std::vector<PlacedScript> m_placed;
    // What ETHCallback_antiportal's AddToAngle has turned the level's rings by. One
    // for all of them: the original turns each ring by its own callback, and they
    // run on the same frames with the same step, so they hold one angle between
    // them. Reset with the level, like the level's age, which the blink reads.
    Motion::Turn m_antiportalTurn;
    // THE ZERO-G DOOR BOB (motions.json `door`). Game::loop calls linearMotion on
    // the exit door, the black behind it and the glow readProperties added, in that
    // order, on every frame of a `no_gravity` level. One state for all three: they
    // start at the same constant angle of 0 and are stepped in one loop with one
    // frame, so they never come apart. Picture-side only, like m_motions: the
    // original's SetPosition moves the door's sensor with it and the port's does
    // not (motions.json `door._not_built`).
    Motion::Linear m_doorBob;
    bool m_doorBobRuns{false}; // the level sets `no_gravity`: the loop's gate
    // The level's properties carry `shadowless_door`: its door is drawn on the
    // other sheet and, where the bob runs too, a glow is added behind it.
    bool m_shadowlessDoor{false};
    int m_doorSprite{-1};      // in m_sprites: the exit door, as SeekEntity finds it
    int m_doorBgSprite{-1};    // and the doorway's black
    int m_doorHighlight{-1};   // and door_highlight.ent, which no level places
    // The start angles' stream, re-seeded for every level it builds with this mixed
    // with the level's name (Motion::LevelSeed), so a level's phases are its own,
    // do not depend on what was played before it, and no other generator's draws
    // move (00_order K8). The original's are randF(PI) off a clock-seeded generator
    // and cannot be recovered; this is the port's determinism choice.
    static constexpr std::uint32_t kMotionSeed = 20260917u;
    // ONE PLACED PORTAL AS THE ORIGINAL DRAWS IT (the visuals plan's 7.4), by the
    // id the sim gives it and never by its place in the list: a pair going takes
    // the ones after it along one place, and this list OUTLIVES the sim's by the
    // length of the kill sequence.
    //
    // What insertPortal adds is two entities - portal_summon_effect.ent and
    // portal.ent - and portal.ent IS the picture and the two particle systems, so
    // the effects runtime draws it like any other entity the script adds
    // (`effect` below is its id). What the layer keeps is what the ORIGINAL keeps
    // on the entity: the colour PortalManager::update gives it, and
    // ETHCallback_portal's killTime.
    struct DrawnPortal {
        int id = 0;
        glm::dvec2 atPx{0.0};   // where it stands: cached, so the death still knows once the sim's record is gone
        int effect = -1;        // its portal.ent effect, or -1 where the level has no art to draw it with
        glm::dvec3 colour{1.0}; // ETHEntity::SetColor, as the manager last left it
        bool killed = false;      // killPortal has run: its two systems release nothing more
        bool killTimeSet = false; // and its callback has noticed, which is the frame AFTER
        double sinceKillMs = 0.0; // GetTime() - killTime
        // AND WHETHER THE MANAGER STILL HOLDS IT. killAll and killPortal take a
        // portal out of play without touching the array (killAll #28-82 pops
        // nothing); update's own popFront and popBack are what remove it, and
        // until they do it is still one of the entries the colouring counts.
        bool popped = false;
    };
    std::vector<DrawnPortal> m_portalDraws;
    entt::entity m_shotQuad{entt::null};    // the shot in flight, when its image is there
    // _projectileFrameTimer: ONE timer for the whole run, as the original's is a
    // script global. Stepped on each tick a shot flies (stepLevel) and never
    // reset, so a shot joins the cycle where the last one left it (art.json
    // shot.animation).
    Art::FrameTimer m_shotFrames;
    int m_shotCell{0};                      // the cell the last step gave the shot
    entt::entity m_playerQuad{entt::null};  // the player, when its image is there
    // The player's depth in the level's units: GameCharacter's 4 over the scale
    // factor (art.json character.z, depth; R10). Its slot among the art is after
    // every sprite at or below it, and it is its lighting height.
    double m_playerZ{0.0};

    // One of the level's <Light>s: the Light2DComponent standing for it, its halo's
    // quad when it has one, and the sprite of the entity that owns it. Every light
    // in the 128 levels has an owner that draws a sprite (72 of 72, counted in
    // step 49), and a light whose owner draws none stands at its node.
    struct PlacedLight {
        std::string node;
        Lighting::Light light;
        bool ownerStatic{false};
        double ownerZ{0.0};
        int sprite{-1};        // its owner's picture in m_sprites, or -1
        int emitter{-1};       // its owner's first <ParticleSystem> in m_emitters, when one was built
        glm::dvec2 atPx{0.0};  // its node's position, for an owner with no picture
        float haloZ{0.0f};     // just in front of its owner's picture, behind its particles
        entt::entity entity{entt::null};
        entt::entity halo{entt::null};
    };
    std::vector<PlacedLight> m_placedLights;
    entt::entity m_shotLight{entt::null}; // the shot's own light while it flies
    entt::entity m_shotHalo{entt::null};  // and its halo, when its image is there
    bool m_lightMasksOff{false};          // ForceLightMasksOff
    // (light_from_projectile.ent's light and halo, which step 55 built here, are the
    // effects runtime's since the visuals plan's 11.2: the whole entity is added
    // where a shot lights a torch, its light and its halo with it.)
    float m_direction{0.0f};                // this tick's walk: -1 left, 1 right, 0 standing
    bool m_facingRight{false};              // which way the player last walked
    // SideScrollerCharacter.pushing: whether the last step's ray met something
    // (Player::Pushing). The next step draws with it, as updateFrame runs before
    // applyForces; never set where gravity is off.
    bool m_pushing{false};
    // On a no_gravity level the column is not the flipbook's: MainCharacter's own
    // timer and hover, stepped once a level step (Art::UpdateNoGravity), and the
    // column this step draws.
    Art::NoGravityPlayer m_noGravityPlayer;
    int m_noGravityColumn{0};
    // Chapter 1's boss: its reach as a box, beholder.ent's sheet when the image
    // is there, and its spikes. The beholder is drawn at its adder's z_index
    // among the art, and a spike at the -4 the original adds it at.
    entt::entity m_beholderBox{entt::null};
    entt::entity m_beholderQuad{entt::null};
    glm::dvec2 m_beholderScale{1.0}; // its pulse as last set, which it keeps while it throws rocks
    glm::vec4 m_beholderColour{1.0f}; // C: (1, hp / max, hp / max), reddening as it is hurt
    std::vector<entt::entity> m_spikes; // one per spike in flight
    float m_beholderZ{0.5f};
    float m_spikeZ{0.5f};

    // A timed crystal's dial (art.json timer), which the original adds behind the
    // crystal and which outlives it: once the time is up or the crystal is taken,
    // the crystal's picture goes and the dial shrinks away where it last stood -
    // from the tick the time is up, or from the tick after the one it is taken on.
    // Its own quads, not m_sprites', whose indices are the drawing slots.
    struct TimerDial {
        int crystal{-1};          // in goals.crystals
        int sprite{-1};           // the crystal's picture in m_sprites, whose drawn centre it takes
        entt::entity quad{entt::null};
        glm::dvec2 cellPx{0.0};   // one cell of timer.png
        float z{0.0f};            // after the sprites at or below the crystal's z_index + zOffset
        double lookZ{0.0};        // the crystal's eth_z + zOffset: its lighting height
        glm::dvec2 atPx{0.0};     // where it is drawn: the crystal's centre, kept once it is gone
        glm::dvec2 scale{1.0};    // the pulse, then the shrink
        double alpha{0.0};
        int frame{0};
        bool taken{false};        // the crystal was taken: its last live tick is drawn, then it shrinks
        bool shrinking{false};
    };
    std::vector<TimerDial> m_timers;

    // THE GHOST OF A TAKEN LIGHT WALL (art.json light_wall_ghost): dashed_door.ent,
    // which seekLightWallToDestroy adds at the wall's own point and angle on the
    // tick a torch is lit and addLightWall deletes when the orbiting fire signal is
    // shot. A picture with no node and, unlike the dial or the well's ring, one
    // that comes and goes with the level's play, so its quad is made and unmade
    // rather than hidden - which is what the original does to the entity.
    struct WallGhost {
        entt::entity quad{entt::null};
        std::string texture;      // the tier file the original draws
        glm::dvec2 atPx{0.0};     // the wall's own point: the ghost never moves off it
        glm::dvec2 sizePx{0.0};   // dashed_door.png's units, 32 x 128; 0 when nothing resolved
        glm::dvec2 scale{1.0};    // bounceLightEntity's pulse this tick
        double alpha{0.0};        // its SetAlpha this tick
        double pulseMs{0.0};      // bounce's blinkElapsedTime, from the tick it was added
        Motion::Turn turn;        // the `angle` datum incrementAngleCustomData adds to
        float z{0.0f};            // the slot behind the wall's
        float rotation{0.0f};     // the wall's angle, as the engine turns a quad
        double lookZ{0.0};        // its depth in the level's units
        bool added{false};        // the wall is flagged, so the entity exists
        bool drawn{false};        // and a frame has stepped it: the original's first mapped frame
    };
    WallGhost m_wallGhost;

    // THE BLACK BARS ROUND THE SCENE (art.json black_bar): black_bar.ent, four
    // times, as Game::manageSceneBounds adds it on every level. Pictures with no
    // node, placed once in the level's space and never moved, at kLayerableTopZ;
    // off the view at rest, and uncovered only by a tremble, which moves the drawn
    // camera past the clamp and the level with it. Their own quads, not m_sprites',
    // whose indices are the drawing slots the between-slot table counts.
    struct BlackBarQuad {
        entt::entity quad{entt::null};
        std::string texture;      // the tier file the original draws
        glm::dvec2 centrePx{0.0}; // BlackBars::Centres, in the level's units
        glm::dvec2 sizePx{0.0};   // black_bar.png's units at the entity's scale
    };
    std::vector<BlackBarQuad> m_blackBars;
    // black_bar.ent as LoadEnt read it with the level: its emissive and lighting
    // facts, which syncLighting tints the bars with.
    Art::Entity m_blackBarEntity;

    // THE SHOCK RINGS (art.json shock_ring): shock_area.ent, which each shock agent
    // adds round itself on its first frame. Pictures with no node, one per field in
    // Fields' order, their own quads and not m_sprites' - whose indices are the
    // drawing slots slotAfter counts - made with the level's art and taken away with
    // the agent. Picture-side only, never Game::Level or the state hash.
    //
    // TWO CLOCKS, as the original's one callback has them: the TURN is game time,
    // stepped in advanceMotions, so a pause holds it; the JITTER is drawn every
    // FRAME in OnUpdate, from m_effectRandom, and a pause does not stop it.
    struct ShockRingQuad {
        int field{-1};             // in fields.fields
        entt::entity quad{entt::null};
        std::string texture;       // the tier file the original draws
        glm::dvec2 atPx{0.0};      // the field's point, as the last tick left it
        double baseUnits{0.0};     // scaleToSize's size: the diameter
        double jitter{1.0};        // this frame's factor
        int jitters{0};            // the factors drawn so far
        Motion::Turn turn;         // AddToAngle(unitsPerSecond(spin))
        float z{0.0f};
        double lookZ{0.0};
    };
    std::vector<ShockRingQuad> m_shockRings;
    // shock_area.ent as LoadEnt read it with the level: its picture, blend,
    // emissive and lighting facts.
    Art::Entity m_shockRingEntity;

    // The minions and knights (art.json minion), one per Minions::State::minions
    // entry and in its order: that list only grows, by an escort the boss calls, and
    // a minion taken is marked gone rather than removed. The frame is its script's,
    // stepped once a level step; picture-side only, never Game::Level or the state
    // hash. No flipbook component: its even split would cut minion.png where the
    // original does not (00_order E1), so the frame's uv is written from m_minionCut.
    struct MinionQuad {
        entt::entity quad{entt::null};
        Art::MinionPose pose;
        bool black{false};  // ghost_minion.ent: SetColor(0) every frame
        // Its entity's .ent carries a particle system (ghost_minion.ent's aura), and
        // whether latchSimEffects has queued them: once, since they go with it.
        bool systems{false};
        bool systemsQueued{false};
        float z{0.0f};      // its slot among the art
        double lookZ{0.0};  // its depth in the level's units, and its lighting height
        glm::dvec2 centrePx{0.0}; // where its picture was last centred
    };
    std::vector<MinionQuad> m_minions;
    // Whether an entity a minion is carries particle systems, by its .ent's name:
    // read once, since the original's files do not change under a run.
    std::map<std::string, bool> m_entityCarriesSystems;
    Tiers::Cut m_minionCut;    // minion.png's tier file, cut as SetupSpriteRects cuts it
    std::string m_minionSheet; // that file; empty when it cannot be read or cut

    // 2-32's dragon: dragon_boss.png cut 2 x 2 as SetupSpriteRects cuts it
    // (dragon.json picture), and the two sprites the sim drives. A sheet that
    // cannot be cut leaves the cut empty and the picture whole, and says so once.
    // CHAPTER 3'S BOSS (art.json ghost): ghost.png's tier file cut as the original
    // cuts it, its quad while the sim holds a ghost that has been added, and its
    // depth - one level unit behind the knight whose death adds it (Slot::Ghost).
    struct GhostQuad {
        entt::entity quad{entt::null};
        std::string sheet; // the file; empty when it cannot be read or cut
        Tiers::Cut cut;
        float z{0.0f};
        double lookZ{0.0};
        glm::dvec2 centrePx{0.0};
        glm::dvec2 sizePx{0.0};
        int cell{0};
    };
    GhostQuad m_ghost;
    // Its appear effect, queued once, on the tick its init block runs.
    bool m_ghostAppearQueued{false};
    // Its shots that already have their entities (enemy_shoot.ent, portal_mark.ent,
    // projectile_shoot.ent), by the sim's id, and the cell _projectileFrameTimer gave
    // each on the last tick (stepLevel).
    std::vector<int> m_ghostShotEffects;
    std::map<int, int> m_ghostShotCells;
    // A key with no node - the one the ghost drops - drawn as the key.ent the script
    // adds: its place in Keys::State::keys, the effect that draws it, and the bob its
    // callback gives it while it lies unowned (ETHCallback_key's linearMotion, from a
    // start angle drawn on the effects' stream, 00_order K8).
    struct DroppedKey {
        int key{-1};
        Motion::Linear bob;
    };
    std::vector<DroppedKey> m_droppedKeys;
    // CHAPTER 4'S BOSS: its dark_dragon.ent, queued once, on the tick the summon adds it.
    bool m_darkDragonQueued{false};

    Tiers::Cut m_dragonCut;
    int m_dragonSprite{-1}; // in m_sprites
    int m_clawSprite{-1};
    // A marker's minion, at the 2 minion_spawn adds it at, and an escort the boss
    // calls, at its anchor's depth: each after the art at or below it, and both
    // behind the player.
    float m_minionZ{0.5f};
    float m_escortZ{0.5f};
    double m_escortLookZ{0.0};

    // The carrancas' fireballs, one PLACEHOLDER box per fireball in flight
    // (Turrets.hpp). fireball.ent carries no <Sprite> at all - what the original
    // shows is its ParticleSystem and its Light - so the entity itself is the
    // picture, and latchSimEffects adds one per fireball. These are what a level
    // with no art to draw shows instead, and what the boxes key turns on.
    std::vector<entt::entity> m_fireballs;

    // One <ParticleSystem> of one placement: the system as the .ent states it,
    // who carries it, and its pool, moved by sim/Particles' arithmetic.
    //
    // PER FRAME, and never on the tick. Nothing here touches Game::Level, the
    // simulation's clock or the state hash - a particle is a picture, and the
    // port's determinism is the thing most easily broken by pretending
    // otherwise. The generator is the layer's own for the same reason: the
    // engine's is one process-global stream that every other drawer shares.
    struct Emitter {
        Particles::System system;
        // Where it was made. An emitter of a level's sprite takes its owner's place
        // and angle again every frame from m_sprites (ownerPx, ownerAngleDeg), so it
        // follows what moves the picture; one with no sprite stays here.
        Particles::Owner owner;
        // In front of its owner's picture and its halo (Particles::SlotFraction) -
        // or, for a level's system whose <StartPoint> z is not 0, after every
        // picture at or below `depth` (Slot::LiftedSystems).
        float z = 0.0f;
        double depth = 0.0; // its owner's z plus its <StartPoint> z, on the level's axis
        bool lifted = false;
        glm::dvec2 cellPx{0.0};
        std::string image;
        // A cut sheet whose units do not divide by its cut, cut as SetupSpriteRects
        // cuts it - its stride in WHOLE units (gs2d Sprite.cpp:119-130) - where a
        // flipbook's even split would sample past it: its quads take their uv from
        // `cut` rather than from a SpriteAnimationComponent. Two sheets the game
        // draws are such: an effect's shock_strike.png (512 over 5 is 102, not
        // 102.4), and tesla_shock_black_bg.png (256 over 5 is 51, not 51.2), which a
        // level's shock agents and shock diamonds carry (the visuals plan's 13.1,
        // step 67's recorded deviation). One rule for every emitter: WholeUnitCut.
        bool wholeUnitCut = false;
        Tiers::Cut cut;
        int sprite = -1; // the sprite in m_sprites whose entity it belongs to: it goes when that goes
        int slot = 0;    // which of that entity's <ParticleSystem>s it is, from 0
        // ETHEntity::KillParticleSystem: no particle is renewed, and each lives out
        // the life it has.
        bool killed = false;
        // ETHParticleManager::GetNumActiveParticles as of this frame's update: what a
        // light and a halo take the share of.
        int active = 0;
        // How many of 4-19's landing's Scale(0.9) calls on its owner it has taken:
        // the key's, whose ETHEntity::Scale scales its systems too (updateEmitters).
        int scalingsTaken = 0;
        std::vector<Particles::Particle> particles;
        std::vector<entt::entity> quads; // one per particle, null while it is not drawn
        // What its quads are tagged with, so a level's particles and a spawned
        // effect's can be told apart in the registry and by the suites.
        const char* tag = "Magic Portals Particle";
    };

    // One entity the script ADDED (SpawnEffect), while it lives.
    //
    // Its emitters are ITS OWN, drawn on m_effectRandom and stepped here, so that
    // the level's placed emitters keep their stream bit for bit however many
    // effects come and go (00_order K8). Its particle quads are POOLED - hidden
    // rather than destroyed - because a burst of sparkles would otherwise make
    // and unmake entities every frame.
    struct Effect {
        int id = 0;
        std::string entity; // the .ent it was read from
        Art::Entity art;    // as Art::LoadEnt read it, before any scale
        glm::dvec2 atPx{0.0};
        double angleDeg = 0.0;
        double scriptScale = 1.0; // the Scale() its AddEntity was given
        // ETHEntity::SetColor, which only its SPRITE reads: the manager's colour
        // for a placed portal, white for everything else, and the kill fade's
        // 0.9 a frame once one is dying.
        glm::dvec3 colour{1.0};
        // ETHEntity::SetEmissiveColor: the .ent's <EmissiveColor> when it is added,
        // and what its script writes after (the dark dragon's DAMAGE and DYING).
        glm::dvec3 emissive{0.0};
        // ETHEntity::SetAlpha, which only its sprite reads, and what its callback
        // adds to it a second up to 1 (EffectSpawn::alphaPerSecond).
        double alpha = 1.0;
        double alphaPerSecond = 0.0;
        // bounce() on its SetScale, over `scale` (EffectSpawn::bounces): its clock
        // in ms and what it gives this frame.
        bool bounces = false;
        Art::Pulse bounce;
        double bounceMs = 0.0;
        glm::dvec2 bounceScale{1.0};
        glm::dvec2 scale{1.0};    // times its <Scale>: what its picture, halo and light take
        double zUnits = 0.0;
        bool temporary = false; // the engine frees it once its pools are over
        // Whether the engine updates it while it is HIDDEN. A hidden entity is not
        // drawn either way (ETHScene.cpp:568, ETHRenderEntity.cpp:396), but only an
        // always-active one goes on being stepped: the handler's own list is walked
        // whatever each entry's state (ETHActiveEntityHandler.cpp:119-134), while
        // anything else is put on the frame list by the bucket walk, which skips a
        // hidden entity (ETHScene.cpp:568-572, handler :151-157). So a hidden effect
        // that is not always active FREEZES - it releases nothing more.
        bool alwaysActive = false;
        bool hidden = false;    // its owner is hidden, so nothing of it draws
        glm::dvec2 cellPx{0.0}; // its picture's frame, scaled; zero when it has none
        // A sheet of more than one cell, cut as SetupSpriteRects cuts it
        // (Tiers::FrameCut), and the cell its script puts it on: the uv is written
        // from these, never an even split (00_order E1).
        Tiers::Cut cut;
        int cell = 0;
        float z = 0.0f;         // Slot::Effect
        float haloZ = 0.0f;     // Slot::EffectHalo
        entt::entity quad{entt::null};
        entt::entity light{entt::null};
        entt::entity halo{entt::null};
        EffectOwner follow;
        // Its own callback's followUp and Scale, where it has them (EffectSpawn).
        std::optional<Effects::FollowUpRule> followUp;
        EffectOwner followUpTarget;
        Effects::FollowUp followUpState;
        double scalePerFrame = 1.0;
        std::vector<Emitter> emitters;
    };
    // Every particle system of an entity of the original's, at a place: false when
    // the .ent carries none or cannot be read.
    bool addEntityEmitters(const std::string& entity, const glm::dvec2& atPx, float z, double angleDeg);
    // The texture a system's particles are drawn with, from `image`, the file the
    // loader draws for its bitmap: that file for an added or a mixed system, and
    // its multiply (modulateImage) for an AM_MODULATE one - the one place the
    // three builders (buildEmitters, addEntityEmitters, SpawnEffect) choose it, so
    // no builder can hand a multiply's own bitmap to a mixing quad. Empty, and
    // said by `owner`'s name, for a multiply that would not be exact
    // (Particles::MultipliesExactly) or whose image cannot be made.
    std::string particleTexture(const Particles::System& system, const std::string& image, const std::string& owner);

    // No emitter draws more than this, whatever its .ent asks for. Chapter 1
    // places 189 of them and the largest asks for 32, so this is a guard
    // against a later chapter rather than a limit anything here reaches.
    static constexpr int kMaxParticles = 64;

    std::vector<Emitter> m_emitters;
    // Seeded, so a run looks the same twice and a screenshot can be compared.
    std::mt19937 m_particleRandom{20260912u};
    // And the effects' own stream (00_order K8). The level's placed emitters draw
    // from the one above in the order they were built; an effect the script adds
    // mid-level would shift every later draw of theirs if it shared it, and every
    // capture of every level would move beyond the effect itself. The loading
    // screen's own entities (addEntityEmitters) stay on the placement stream:
    // they are all there is while it is up, and no level's emitters are alive to
    // be moved.
    std::mt19937 m_effectRandom{20260919u};

    // The effects the script has added, in the order they were added, and what
    // the TICK has queued for the next frame to add.
    std::vector<Effect> m_effects;
    std::vector<EffectSpawn> m_effectQueue;
    int m_effectsSpawned{0};
    int m_effectsFreed{0};
    // The player's red eyes, by id: the one effect a level starts with, and the
    // only one the port moves every frame rather than leaving where it was added.
    // -1 while the level has none (no art, or the .ent could not be read).
    int m_eyes{-1};
    // Every fireball in flight that has already been given its fireball.ent, by
    // the name Turrets gives it. latchSimEffects walks the sim's list against
    // this one, so a fireball spat on a tick that did not reach the latch - one
    // under a pause, whose BeforeStep still runs - is picked up on the next.
    std::vector<std::string> m_fireballEffects;
    // Per crystal of Goals', in its order: whether what it leaves where it was -
    // crystal_pick when taken, crystal_vanish when its time ran out - has been
    // queued. A state walk rather than a count, so the one queued is the one
    // that went, at its own point; sized by the first latch of a level.
    std::vector<char> m_crystalEffects;
    // Per key of Keys', in its order (the visuals plan's 10.3): the carrier it
    // had at the last latch - key_pick is queued when that CHANGES to one; a
    // drop and re-take by one carrier between latches is no change - and its
    // fly-in clock once spent, which queues key_unlock_halo. Grown, never
    // snapshotted: a key the level starts with is unowned until a carrier takes
    // it, so a pick on the very first tick (3-18's knight) is a pick; and a key a
    // boss drops joins the end of Keys' list.
    struct KeyWatch {
        entt::entity owner{entt::null};
        Effects::FlyIn flyIn;
    };
    std::vector<KeyWatch> m_keyEffects;
    // Per keyhole of Keys': whether the three it adds as it goes - wall_destroy at
    // its door, door_locked_vanish and keyhole_vanish - have been queued.
    std::vector<char> m_keyholeEffects;
    // What a diamond, a shock ring and a fire signal leave (the visuals plan's
    // 10.4). Per diamond of Diamonds', in its order: whether it was held and
    // whether it was gone at the last latch - a pick is queued where `held` turns
    // on, and a fire diamond's water where `gone` does. Edges and not one-shot
    // flags, because 3-32's boss puts a drained fire diamond back. Grown, never
    // snapshotted: a diamond starts lying and standing.
    struct DiamondWatch {
        bool held = false;
        bool gone = false;
        // The ticks it has been held in a row, and whether its carried arm has run
        // since it was last made: SetAlpha(0.1f) from the update after the pick
        // (art.json carried_diamond), kept until it goes or a boss puts it back.
        int heldTicks = 0;
        bool faded = false;
    };
    std::vector<DiamondWatch> m_diamondEffects;
    // Per shock agent of Fields', in its order: gone at the last latch (a
    // fireball destroyed it: shock_death 10 in front of it).
    std::vector<char> m_fieldEffects;
    // Per breakable of Demolish', in its order (the visuals plan's 11.1): whether
    // its breaking has been seen - a breakable_wall's wall_destroy is queued then.
    // One-way, as a wall does not come back; sized by the first latch of a level.
    std::vector<char> m_wallEffects;
    // Whether a shock ring had claimed the player, and the player was dead, at
    // the last latch: the ring's shock_death goes where it is the kill.
    bool m_ringKillSeen{false};
    bool m_playerDeadSeen{false};
    // The torch's fire signal as the last latch saw it, and how many were put
    // out: extinguish_fire goes at its originalPos - its torch's point - when the
    // count goes up, by which time Torch has cleared the signal.
    int m_signalsPutOut{0};
    bool m_signalSeen{false};
    glm::dvec2 m_signalOriginPx{0.0};
    // Per torch of Torch's, in its order (the visuals plan's 11.2): lit at the last
    // latch; whether the unlit torch is deleted - on the tick AFTER it is lit,
    // which is when its `delete` branch deletes the entity, and back with the
    // fresh light_off.ent a put-out adds; and the depth the torch entity stands at
    // now - its node's eth_z, and after a put-out the bare 10
    // ETHCallback_fire_signal adds the new one at, which the next lighting's
    // entities are added on.
    //
    // ONE FRAME OF BOTH. The latch that sets unlitGone runs before the tick's
    // syncSprites, so the level's own picture is last drawn on the frame after the
    // lighting tick - the frame its lit torch is first drawn on - and the one a
    // put-out added (EffectOwner::Kind::UnlitTorch) is let go on the same latch.
    // The original's delete branch runs on the frame the lit torch is first drawn,
    // after that frame's list of what to draw was made (Scene/ETHScene.cpp:482-511;
    // 00_order 8.0 rule 6), so it too draws the two together once.
    struct TorchWatch {
        bool lit = false;
        bool unlitGone = false;
        double z = 0.0;
        // Its unlit entity's AddToAngle(unitsPerSecond(190)), stepped on the tick
        // while it stands (art.json unlit_torch) and from 0 again for the one a
        // put-out adds: its systems turn by it, its vertical picture does not.
        Motion::Turn spin;
    };
    std::vector<TorchWatch> m_torchWatch;
    // THE BOSSES' OWN PICTURES (planning step 146): each dying arm's
    // overTimeEntityAdder clock for small_explosion_no_light - one per boss entity,
    // as the original keeps it under 'small_explosion_no_light.entAdder' - run on
    // the tick from effects.json's stride, so the picture needs nothing of
    // sounds.json (whose boss_explosion runs the same clock beside it); and what the
    // last latch saw, to tell which arm this tick ran and what a boss has put back.
    struct BossPictures {
        bool valid = false;
        bool beholderGone = false;
        bool ghostSpawned = false;
        bool ghostGone = false;
        Ghost::Phase ghostPhase = Ghost::Phase::Appearing;
        bool darkAlive = false;
        bool darkGone = false;
        DarkDragon::Phase darkPhase = DarkDragon::Phase::Appearing;
        double beholderMs = 0.0;
        double ghostMs = 0.0;
        double darkMs = 0.0;
        std::size_t restocksSeen = 0;       // Restock::State::added: each a barrel put back
        int summonsSeen = 0;                // the ghost's escorts called
        std::vector<char> fireDiamondsGone; // per diamond of Diamonds': a fire one gone
    };
    BossPictures m_bossPictures;
    // The light wall's going as the last latch saw it (Torch::State::wallsGone),
    // and whether the wall standing now is one addLightWall built at the ghost -
    // its point the ghost's, its depth the ghost's - rather than the level's.
    int m_wallsGoneSeen{0};
    bool m_lightWallRebuilt{false};
    // MainCharacter::addTrail's trailElapsedTime (effects.json movement_trail).
    double m_trailMs{0.0};
    // ETHCallback_gravity_agent's effectAngleTime and effectAngleSign, per well of
    // GravityWell's in its order: a whole turn at rest, from spin_ms; a lap by the
    // visible player zeroes the clock and sets the sign (gravitywell.json
    // roundabout).
    struct WellSpin {
        double ms = 0.0;
        double sign = 1.0;
    };
    std::vector<WellSpin> m_wellSpins;
    // The cell the player was last drawn on, kept across the door: updateEyes
    // reads GetFrame every frame, and the quad it comes from is destroyed when the
    // character is hidden.
    int m_playerFrame{0};

    // ---- sound: the original's AudioManager, carried as data ----------------
    //
    // PER FRAME, exactly like the particles above and for the same reason. The
    // TICK only latches what happened - a counter that went up, a flag that
    // turned over - and OnUpdate plays it. Nothing below reaches Game::Level,
    // the simulation's clock or the state hash, so a level plays identically
    // with no audio device at all. That is not a nicety: it is how every suite
    // and every headless render runs, and what keeps a replay a replay.
    void loadSounds();
    // Remember that `event` happened on this tick. Named for sounds.json's
    // events table; one the file leaves silent costs nothing here. A door
    // brings its own stride: its hook's speed is 3000 / it. A hook that takes
    // its caller's volume (playFallSound's hitIntensity / 18) is given it here.
    void latch(const std::string& event, double doorStrideMs = 0.0, double volume = -1.0);
    // What the simulation did this tick, against what it looked like last.
    void latchSimSounds(const ScriptClock::Step& step);
    // The bosses' calls (planning step 142): the tests their callbacks make on
    // every frame - a first sight, a clock - and the edges of their states.
    void latchBossCues(double tickMs);
    struct Watch;
    // Step 142's edges between two ticks' watches, and the late bosses' among
    // them; on a level's first tick against FreshWatch, the level as it is made.
    void latchStepSounds(const Watch& was, const Watch& now);
    void latchLateBossSounds(const Watch& was, const Watch& now);
    static Watch FreshWatch(const Watch& now);
    void playLatched(entt::registry& registry, float deltaTime);
    // WantedTrack on the device: started and stopped as it changes.
    void updateMusic(entt::registry& registry);
    // The original's music calls, on the state below: AudioManager::playMusic
    // (currentTrack, and the player going), a stop of its one player
    // (pauseMusic, and a boss's StopSample(getBossMusicName())) and resumeMusic.
    void musicPlay(const char* track);
    void musicStop();
    void musicResume();
    // A switch's toggle, as its listener (the AudioManager) takes it: off is
    // pauseMusic; the sound back on resumes only with the music on, the music
    // back on always (turnGlobalVolumeOn / turnMusicOn, bytes 186452..186631).
    // Each saves the switches first, as saveVolume and saveMusicState write the
    // value before they call the listener.
    void switchedSound();
    void switchedMusic();
    // The switches as they stand, into audio.json (sim/Switches.hpp).
    void saveSwitches();
    // Game::preLoop #485-509: the level's track, the boss's, or pauseMusic.
    void startLevelMusic(int index);
    // The calls the bosses' callbacks make, read off the tick's level.
    void latchMusic();
    // The log's line, each time WantedTrack changes: a run's log says what played.
    void noteMusic();
    void stopMusic(entt::registry& registry);
    double soundRandom(double from, double to);
    // A hook's sample speed as the engine's pitch: a number it states, a draw
    // from its range, or the opening door's own 3000 / stride.
    float pitchFor(const Sounds::Hook& hook, double doorStrideMs);

    // What the simulation looked like on the previous tick, so this one can
    // tell what changed. Counters and one-way flags only, and nothing here is
    // ever read back into the simulation.
    struct Watch {
        bool valid = false;
        int portalsUsed = 0;
        int traversals = 0;
        int shotsFired = 0;
        int shotsFailed = 0;
        int reflections = 0;
        int crystalsCollected = 0;
        int crystalsExpired = 0;
        int staticsLive = 0;
        int wallsBroken = 0;
        int stonesThrown = 0;
        int fireballsSpat = 0;
        int bossHits = 0;
        int bossVolleys = 0;
        int bossRocksBroken = 0;
        int bossFrame = 0;
        bool bossGone = false;
        bool bossButtonRaised = false;
        bool redKeyTaken = false; // 4-19's red key state saw its key taken
        int liftsSent = 0;        // touches that sent a lift, summed over the lifts
        // char rather than bool: std::vector<bool> is not a container of bools,
        // and these are only ever compared with the last tick's.
        std::vector<char> doorsOpening;
        // Planning step 142's, each compared with the last tick's in the same way.
        int blasts = 0;            // Fire: bombs gone off
        int despawned = 0;         // Launchers: a stone on a destroyier
        int minionsKilled = 0;     // Minions: the killer floor
        int minionsBurned = 0;     // Minions: burned by fire
        int doorsOpened = 0;       // Keys: keyholes gone with their doors
        int struck = 0;            // Diamonds: shock strikes
        int torchesLit = 0;        // Torch
        int torchesPutOut = 0;
        int crushed = 0;           // 2-32's claw
        bool dragonTookOff = false;
        int bossRocksThrown = 0;
        int bossSpikeHits = 0;
        bool ringKilled = false;   // Fields claimed the player...
        bool playerDead = false;   // ...and whether the player was dead
        std::vector<char> burnables;    // Fire's burnables: burned
        std::vector<char> fieldsGone;   // Fields: a shock agent destroyed
        std::vector<char> diamondsHeld; // Diamonds: carried
        std::vector<char> diamondsGone;
        std::vector<entt::entity> keyOwners; // Keys: each key's carrier
        // The two late bosses, whose sounds are their states' edges.
        bool ghostSpawned = false;
        bool ghostGone = false;
        Ghost::Phase ghostPhase = Ghost::Phase::Appearing;
        int ghostBeats = 0;
        int ghostSummons = 0;
        int ghostHits = 0;
        int ghostShotsFired = 0;
        int ghostShotsEnded = 0;
        bool darkSummoned = false;
        bool darkAlive = false;
        bool darkGone = false;
        DarkDragon::Phase darkPhase = DarkDragon::Phase::Appearing;
        int darkHits = 0;
    };

    // The sound's own state across ticks, where a count going up is not the
    // moment (planning step 142): the original's one-shot data and clocks, each
    // named for the datum it stands for. Reset with each level, as the watch is.
    struct SoundCues {
        bool beholderAppeared = false; // ETHCallback_beholder's hasAppeared
        bool dragonAppeared = false;   // ETHCallback_dragon's appearSoundPlayed
        bool darkMouth = false;        // ETHCallback_dark_dragon's appearSoundPlayed
        bool lightSwitched = false;    // the light wall's soundPlayed, for this wall's fade
        // overTimeEntityAdder's small_explosion_no_light clock on each boss's own
        // entity: started by its first call, zeroed only by an add.
        double beholderAdderMs = 0.0;
        double ghostAdderMs = 0.0;
        double darkAdderMs = 0.0;
        // Each key's fly-in clock, as latchSimEffects runs its own (Effects::FlyIn).
        std::vector<Effects::FlyIn> keyFlyIns;
    };

    // One event the tick saw, with what playing it needs to know.
    struct Latched {
        std::string event;
        double doorStrideMs = 0.0;
        double volume = -1.0; // the caller's volume, for a hook that takes one; below 0 none
    };

    // ---- the drawing diagnostic ---------------------------------------------
    //
    // What the game BELIEVES it is drawing, written down as it changes, so that
    // a sprite which vanishes on screen can be told from one the game stopped
    // drawing. If the picture loses a sprite and nothing is logged for it, the
    // game still thinks it is drawing it - and the fault is below this layer,
    // in the renderer or the GPU. That one distinction is what this is for.
    void reportSprites(entt::registry& registry);

    bool m_dumpRequested{false};    // G was pressed on the tick
    std::vector<char> m_onScreenLast; // one per sprite, as of the last frame
    bool m_reportedOnce{false};     // so the first frame states the whole set

    Sounds::Rules m_soundRules;
    std::string m_soundsError;      // why there is no sound, when there is none
    std::vector<Latched> m_latched; // this tick's events, in the order they happened
    Watch m_watch;
    SoundCues m_soundCues;
    // The level's crates, whose callback is crateContactCallback (sounds.json
    // impacts.crate.entities), in the scene's order: made at the load, for the
    // impact watch.
    std::vector<entt::entity> m_crates;
    double m_soundClockMs = 0.0;               // the frames' own clock, for the shared timers
    std::map<std::string, double> m_timerAtMs; // when each shared timer last let one through
    // Seeded and the layer's OWN, for the reason the particles' generator is:
    // the engine's is one process-global stream every other drawer shares, and
    // a run has to sound the same twice.
    std::mt19937 m_soundRandom{20260913u};
    // THE ORIGINAL'S MUSIC, as its scripts leave it and with or without a device:
    // AudioManager.currentTrack, and whether the one MediaPlayer has been stopped
    // since (MediaStreamManager.java: one player; a stop stops whatever it plays).
    struct Music {
        std::string current;  // the last playMusic's track, "" as the AudioManager starts it
        bool stopped = false; // stopped since, until a playMusic or a resumeMusic
        // A track's SetSampleVolume since its playMusic: what a card's
        // setMusicVolume and setDefaultMusicVolume leave it at (planning step 143).
        // A track not here plays at its own volume.
        std::map<std::string, double> volumes;
    };
    Music m_music;
    // What the level looked like on the last tick, for the calls made once.
    // Reset with each level, to the fresh level's values: nothing is set yet.
    struct MusicWatch {
        bool ghostInitialised = false;
        bool dragonTookOff = false;
        bool darkDragonSummoned = false;
        bool darkDragonDying = false;
    };
    MusicWatch m_musicWatch;
    std::string m_musicNoted{"-"}; // WantedTrack as the log last had it
    std::string m_track; // which of sounds.json's tracks is playing, or empty
    // A looping voice is never "finished", so ReapFinishedVoices leaves it
    // alone for ever: the layer holds this and stops it itself, or menu to
    // level to menu leaves two tracks playing over each other.
    Supersonic::AudioEngine::VoiceId m_musicVoice{Supersonic::AudioEngine::kInvalidVoice};
    float m_musicVoiceVolume{-1.0f}; // what the voice was last given

    // The menu. Screen::None while a level is played.
    Screen m_screen{Screen::None};

    // ---- the menu states ---------------------------------------------------
    MainMenu::Rules m_mainMenuRules;
    Loading::Rules m_loadingRules;
    // The state's clock: `fresh` until its first tick, which is its zero.
    struct MenuClock {
        bool fresh{true};
        int ticks{0};
        // The script's milliseconds (a tick's 16): what Button::update reads.
        double ms{0.0};
        // The same ticks in WALL milliseconds (the engine's 16.667): what the state's
        // FadeInController reads, through sys:GetTime (planning step 147, fix 1).
        double wallMs{0.0};
    };
    MenuClock m_menuClock;
    double m_layerClockMs{0.0};
    // The one touch a menu follows (Button::update): where it went down and how
    // far it has travelled; which main-menu buttons it went down inside and is
    // still inside; and which of chapter select's or the grid's quads.
    struct MenuTouch {
        MenuState::Touch touch;
        unsigned mainDown{0u};
        unsigned mainHeld{0u};
        std::optional<MenuButton> downOn;
        bool heldInside{false};
    };
    MenuTouch m_menuTouch;
    // What a release - or the loading screen's hold, or the back key - asked for,
    // done on the next tick: the original's release frame is drawn in the old
    // state, and the new one's black from the frame after (ui3 spec 0.4).
    struct PendingMenu {
        enum class Kind { None, MainButton, Button, Screen, Achievement };
        Kind kind{Kind::None};
        MainMenu::Button main{MainMenu::Button::Play};
        MenuButton button;
        Screen screen{Screen::None};
        int world{-1}; // Achievement: openState's
        int level{-1};
    };
    PendingMenu m_pendingMenu;
    // The main menu's music switch, on the state's clock: when its entrance began
    // and when it was last dismissed (SoundPanelLayer::manageMusicSwitch).
    double m_mainMusicAddedMs{0.0};
    double m_mainMusicDismissedMs{-1.0};
    // The pictures the menu states draw through the overlay, by the name ui.json
    // gives them, each resolved to its hd twin once.
    std::map<std::string, std::string> m_menuPictures;
    // Credits: its rules, the strip, and the one touch it follows.
    Credits::Rules m_creditsRules;
    Credits::Scroll m_credits;
    struct CreditsTouch {
        bool down{false};
        glm::dvec2 lastAt{0.0};
        bool downOnBack{false};
        bool backHeld{false};
    };
    CreditsTouch m_creditsTouch;
    // The dashboard: its rules, the rows it opened with, and its scroll and buttons.
    Dashboard::Rules m_dashboardRules;
    Dashboard::Board m_dashboardBoard;
    Dashboard::State m_dashboardState;
    Selector::Rules m_selectorRules;
    Selector::Board m_selectorBoard;
    Selector::State m_selectorState;
    Selector::Hit m_selectorDown; // what a touch went down on
    Selector::Hit m_selectorHeld; // and is still inside, drawn at the press tint
    // The popup over chapter select, and whether the touch that is down went down
    // on its close button (Button::update's press is a release inside after that).
    struct MenuPopupScreen {
        bool open{false};
        const Popup::Class* cls{nullptr};
        Popup::Open state;
        bool downOnClose{false};
    };
    MenuPopupScreen m_menuPopup;
    // Where on the state's clock the PageManager's own reads zero: the state's
    // first tick, or the tick after a popup over it was gone - its first update,
    // as a state's first tick is its own.
    double m_selectorLayerFromMs{0.0};
    Locking::Rules m_lockingRules;
    // The original's achievements, read once at attach from Paths::achievements.
    Achievements::Content m_achievements;
    bool m_achievementsLoaded{false};
    std::string m_achievementsError;
    // And the port's own rules for unlocking them, the saved set, this run's
    // unlocks, and the Notificator (planning step 143).
    Achievements::Rules m_achievementRules;
    bool m_achievementRulesReady{false};
    std::string m_achievementRulesError;
    Achievements::Store m_achievementStore;
    std::vector<Unlock> m_unlocks;
    std::optional<Achievements::Play> m_lastDoorPlay;
    // Each key's carrier as the last tick left it, for the red key's pick.
    std::vector<entt::entity> m_achievementKeyOwners;
    Notificator::Rules m_notificatorRules;
    bool m_notificatorReady{false};
    Notificator::Queue m_notificator;
    // Each card's picture, resolved once: the background and the achievements'
    // icons, by the name the card gives them; empty for one that cannot be read.
    std::map<std::string, std::string> m_cardImages;
    // The pause's AchievementsPopup.
    struct AchievementsPopupScreen {
        bool open{false};
        Popup::Class cls;
        Popup::Open state;
        std::string text;
        bool downOnClose{false};
    };
    AchievementsPopupScreen m_achievementsPopup;
    // The loading screen's scene, in the level's space.
    entt::entity m_loadingBg{entt::null};
    entt::entity m_loadingCharacter{entt::null};
    entt::entity m_loadingPortal{entt::null};
    entt::entity m_loadingHalo{entt::null};
    // How many emitters the portal's own are, at the front of m_emitters.
    std::size_t m_loadingPortalEmitters{0};
    bool m_loadingVanished{false};
    // An image of the original's as the multiply AM_MODULATE draws it with:
    // black, at an alpha of 255 less its grey (Particles::MultiplyTexels), written
    // once beside the prisms and named for where it came from. Empty, and said,
    // when the image is not grey, cannot be read or written, or Paths::prisms
    // names no directory. The loading screen's black_halo.bmp since step 53, and
    // every AM_MODULATE particle system since step 96 (particleTexture).
    std::string modulateImage(const std::string& source);
    int m_menuWorld{0}; // whose levels the grid shows
    int m_menuPage{0};  // which page of that grid
    std::vector<MenuButton> m_menuButtons;
    std::vector<entt::entity> m_menuQuads;  // one per button, in its order
    std::vector<entt::entity> m_menuLabels; // one per button, null but for a level's number
    // One per button too, and null for all but a LEVEL THE PLAYER HAS CLEARED:
    // the original draws a medal on a level button only where getScore is not
    // zero, so an unfinished level carries no medal rather than a bronze one.
    std::vector<entt::entity> m_menuMedals;
    entt::entity m_menuBg{entt::null};      // the screen's background
    entt::entity m_menuTitle{entt::null};   // the game's title, on the main screen

    // ---- the finished and lost screens ------------------------------------
    //
    // Drawn through the screen overlay by EmitHud, laid out by sim/LevelEnd.hpp;
    // nothing of them is in the registry. What the layer holds is what a
    // picture cannot be a pure function of: the screen's clock, what the level
    // ended with, and the ScoreCounters, which step on the tick.
    LevelEnd::Rules m_levelEndRules;
    struct EndScreen {
        double clockMs{0.0}; // UI frame time since the screen became current
        LevelEnd::Play play;
        LevelEnd::Counter portals;  // ScoreCounter(0, portalsUsed, 100)
        LevelEnd::Counter crystals; // and the crystals collected
    };
    EndScreen m_end;
    // The walk pads' alpha byte from the door or the death on, decayed a tick at a
    // time; and whether clear-portals was up to be dismissed with restart and pause.
    int m_padEndByte{0};
    bool m_clearShownAtEnd{false};
    // Each end-screen picture's file resolved once per level, and its size in
    // texels; empty and zero for one that cannot be read, which is not drawn.
    std::map<std::string, std::string> m_endImages;
    std::map<std::string, glm::ivec2> m_endTexels;
    // The screen up over the level as pieces, `ms` into it.
    std::vector<LevelEnd::Piece> endPieces(const glm::dvec2& viewUnits, double ms) const;
    // One tick of the screen: its clock, its counters and its buttons. True when
    // a button was pressed, which may have taken the level away.
    bool endScreenTick(entt::registry& registry, const ScriptClock::Step& step);
    // What the HUD keeps once a level has ended: the pads' last byte, and
    // whether clear-portals goes with restart and pause.
    void endHud();

    // THE BEAT BETWEEN GOING IN AND BEING SCORED.
    //
    // GameStateController::checkGameEnd does not put the medal up when the exit
    // is reached. In order it runs levelFinishedEffect - the red_suck_effect at
    // the door, the red_sparkles on the character, and playFinalDoorSound - then
    // Hide()s the character and zeroes its velocity, then writes the score; and
    // only once gameEndElapsedTime passes gameWonDelay does it raise the
    // levelFinishedLayer and play playVictorySound.
    //
    // gameWonDelay is 1400 ms (ui.json's level_end.beats, decoded): the
    // constructor writes 1400 to gameLostDelay and copies the same register into
    // gameWonDelay. A level may override it - the constructor's #143-175 read an
    // `end_delay` entity's `time` - and THREE LEVELS DO: 4-01 5000, 4-19 14500 and
    // 2-32 6600, each the level of a cutscene that fills it (m_wonDelayMs,
    // cutscenes.json; planning step 123). An earlier note here said no level placed
    // one; the converted levels carry the three as `metadata/time`.
    //
    // This port showed the medal and played both sounds on the tick the exit
    // reported, with the character still standing in the doorway walking on the
    // spot. That is what the owner saw.
    bool m_finishing = false;      // gone into the door, not yet scored
    double m_finishClockMs = 0.0;  // how long since
    // The level's own gameWonDelay: level_end's, or its end_delay's (cutscenes.json).
    double m_wonDelayMs = 0.0;

    // THE WIN CUTSCENES (planning step 123). Each is played where its level places
    // its entity; what they move are pictures, never Game::Level - but for the
    // knight, a body the spawn adds only once the level is finished.
    bool m_launchPlaced = false;
    bool m_landingPlaced = false;
    Cutscene::Launch m_launch;
    Cutscene::Landing m_landing;
    Cutscene::Knight m_knight;
    int m_launchStartedTick = -1;
    int m_landingStartedTick = -1;
    int m_launchQuakes = 0;
    int m_knightSpawnedTick = -1;
    int m_knightEnteredTick = -1;
    glm::dvec2 m_knightEnteredAtPx{0.0};
    int m_bridgeSprite = -1;       // in m_sprites: the rocket_bridge SeekEntity finds
    int m_landingKeySprite = -1;   // and the key the landing's SeekEntity('key') finds
    glm::dvec2 m_cameraTargetPx{0.0};
    // The knight's picture: minion.png as m_minionCut cuts it, black, and its aura
    // queued once through the effects runtime (EffectOwner::Kind::Knight).
    entt::entity m_knightQuad{entt::null};
    glm::dvec2 m_knightCentrePx{0.0};
    float m_knightZ{0.5f};
    bool m_knightAuraQueued = false;

    // AND THE SAME BEAT BEFORE THE LOST SCREEN, which this port had none of:
    // death was an instant retry, so a player who walked into level5's
    // death_area was simply back at the spawn with no screen and no sound, and
    // one who walked off a ledge was not killed at all.
    //
    // checkGameEnd counts gameEndElapsedTime against gameLostDelay and only past
    // it raises the levelLostLayer and plays playDeathSound. gameLostDelay is
    // the register gameWonDelay is copied FROM, so the two beats are the same
    // 1400 ms - ui.json carries both, with the one decode.
    bool m_dying = false;         // killed, the lost screen not yet up
    double m_dyingClockMs = 0.0;  // how long since

    // Whether the platform the dark dragon's death adds has been given its
    // picture yet. It has no node, so buildSprites cannot have made one for it:
    // syncSprites borrows the template sibling's when the body appears, once.
    bool m_platformDrawn = false;
    // And whether it has been given its BOX, which is a separate question: the
    // picture above is gated on there being art at all, and the box view is the
    // mode used when there is none.
    bool m_platformBoxed = false;
    // How many of the barrels a restock added (m_level.restock.added) have their
    // picture and their box: each is made once, by index, the tick it appears.
    std::size_t m_restockDrawn = 0;
    std::size_t m_restockBoxed = 0;
    // Whether a refused restock (m_level.restock.refusals) has been said, once a level.
    bool m_restockRefusalSaid = false;

    // Puts the lost screen up, over the level that is still loaded behind it.
    // Follows openFinished and NOT openMenu: openMenu unloads the level and
    // sets m_current to -1, after which the restart button's own guard
    // (`if (m_current < 0) return false`) would make it silently do nothing.
    void openDead(entt::registry& registry);

    // WHAT THE PLAYER HAS EARNED, across runs.
    //
    // The level grid draws a small medal on every level already finished, and
    // the port kept no score at all - so there was nothing to draw and every
    // button was bare. Memory-only unless Paths::saveDir says otherwise, which
    // is what keeps eighteen suites off the filesystem.
    Scores::Store m_scores;

    // The engine's own text lines: what a refused level, a chapter's end and the
    // menu screens say. Not the original's, which draws no text over a level.
    struct HudText {
        entt::entity status{entt::null};
        entt::entity result{entt::null};
        entt::entity controls{entt::null};
    };
    HudText m_hud;

    // ---- the on-screen controls ---------------------------------------------
    //
    // What the original draws over a level, placed and timed by ui.json through
    // sim/Hud.hpp: this layer only says where those functions put things.
    //
    // DRAWN THROUGH THE ENGINE'S SCREEN OVERLAY (core/ScreenOverlay.hpp), not as
    // quads in the level. The original blended its HUD straight onto display
    // values; the scene target then blended in linear light and was tone-mapped
    // after, which capped white at 186 and made a translucent button's contrast
    // depend on what was behind it (step 39). The overlay is drawn after the
    // composite, in screen fractions, so it needs no camera, no interpolation
    // and no z. The level has held display values too since step 44; the HUD
    // stays here, after the composite, where no scene setting reaches it
    // (whether the original's 5/6/5 should is the lighting design's step G6).
    //
    // HIT-TESTED IN VIEW SPACE, straight from the pointer's place in the
    // viewport, so a tap on a control does not depend on the camera at all.
    struct ControlButton {
        Control kind{Control::Left};
        std::string image; // the picture, empty when it could not be read
        Hud::Rect rect;    // on the view, as last laid out
        bool shown{false};
        double alpha{0.0}; // display-space, as last laid out
    };
    std::vector<ControlButton> m_controls;

    Hud::Rules m_hudRules;
    bool m_hudReady{false};
    std::string m_hudError;
    double m_levelAgeMs{0.0};

    // The tutorial's ring, drawn on each pad's corner, under the pads; empty
    // outside the tutorial.
    std::string m_ringImage;

    // The current-score plaque and its medal, where a medal is recorded; both
    // empty on a fresh save, as the original adds nothing where getScore is 0.
    std::string m_plaqueImage;
    std::string m_medalImage;

    // "Part N", a quad a letter over the font's own pages, above the black.
    std::string m_captionText;
    Supersonic::BitmapFont m_captionFont;
    bool m_captionFontTried{false};

    // The no-portal sign: taken out of the level's sprites, because the level
    // places it off the level and its script pins it to the camera's corner.
    struct NoPortalSign {
        bool present{false};
        std::string image;         // the file its sprite is drawn from: the tier file (hd) where there is one
        glm::dvec2 sizeUnits{0.0}; // the entity's size, as its .ent draws it
        Hud::Follow follow;        // its centre, in the level's units
        Hud::Rect onView;          // as the last tick left it
        double heldMs{0.0};        // frame time owed to the follow (Hud::HandOver)
    };
    NoPortalSign m_sign;

    // ---- the pause ------------------------------------------------------------
    Pause::Rules m_pauseRules;
    struct PauseScreen {
        bool open{false};
        double clockMs{0.0}; // UI frame time since the tap, on the tick
        Pause::Level level;
        double musicAddedMs{0.0};
        double musicDismissedMs{-1.0};
    };
    PauseScreen m_pause;

    // GAME TIME STOPPED, by a pause or a popup: the world as it stood when it
    // stopped - every simulated body's transform and state, put back after each
    // physics step the app runs regardless - and the flipbooks that were playing,
    // stopped for as long as it is.
    struct Frozen {
        struct Held {
            entt::entity entity{entt::null};
            Supersonic::TransformComponent transform;
            Supersonic::RigidBodyComponent body;
        };
        std::vector<Held> held;
        std::vector<entt::entity> stoppedFlipbooks;
    };
    Frozen m_frozen;
    void freezeWorld(entt::registry& registry);
    void thawWorld(entt::registry& registry);

    // ---- the popups -------------------------------------------------------------
    Popup::Rules m_popupRules;
    struct PopupScreen {
        bool open{false};
        const Popup::Class* cls{nullptr};
        Popup::Open state;
        // Whether the level still owes the half of its tick that follows its input
        // (BeforeStep): true for a popup a tap raised mid-tick, as a pause is; false
        // for one raised as the level loads, whose first tick has not begun.
        bool stepOnResume{false};
        // Whether the touch that is down went down on the close button: what a
        // press of it is made of for a class that does not tap to close.
        bool downOnClose{false};
    };
    PopupScreen m_popup;
    // Each popup picture's file resolved once per level, the hd twin where one
    // exists; empty for one that cannot be read, which is then not drawn.
    std::map<std::string, std::string> m_popupImages;
    // The level's help blocks: each entity's place and collision box, in units.
    struct HelpBlock {
        glm::dvec2 atUnits{0.0};
        glm::dvec2 boxUnits{0.0};
    };
    std::vector<HelpBlock> m_helpBlocks;
    // A touch that went down on a help block: which, where in the window's pixels,
    // and the furthest it has moved since (HelpBlockController's touchMoveLength).
    struct HelpTouch {
        bool armed{false};
        int block{-1};
        glm::dvec2 downPx{0.0};
        double maxMovePx{0.0};
    };
    HelpTouch m_helpTouch;
    // THE TOUCH A SHOT IS FIRED BY (Portals::TouchTrack): where the pointer went
    // down, where it was last held and how far it strayed, followed by readInput
    // and, under a popup, by popupTick, as TouchInputManager::update follows it
    // whether or not the game is stopped. A shot leaves on its release. Forgotten
    // as a level loads and as the pause opens, whose buttons the port presses on
    // the way down: a touch that went down on Resume must not fire when it comes up.
    Portals::TouchTrack m_shotTouch;
    // AND THE TUTORIAL HANDS' READING OF THE SAME TOUCH (planning step 146):
    // ETHCallback_hand_tap asks hasPortalRequestInput(t, true), its touch's disabled
    // status ignored, so a touch a lift or a help block took still takes a hand on
    // its release, and so does one a corner control spent. Followed and forgotten
    // as m_shotTouch is, and never spent by what spends that one.
    Portals::TouchTrack m_handTouch;
    // The plaque's dismissal, on GameLayer's clock (the level's age), once the
    // frame clock has passed its dismissAfterMs; negative before.
    double m_plaqueDismissAgeMs{-1.0};
    // Its alpha as GameLayer's last update wrote it: a UISprite's colour is only
    // written when its layer is updated, so under a stop it holds, and under a
    // popup raised as the level loads it is still the nothing it was built with.
    double m_plaqueAlpha{0.0};
    // The pointer this tick as the popups, the controls, a help block and the shot
    // read it - a real touch or a DEV one, the port's one pointer path.
    struct Touch {
        bool pressed{false};
        bool released{false};
        bool held{false};
        bool over{false};         // on the game at all
        glm::dvec2 atView{0.0};   // design units on the view
        glm::dvec2 atPx{0.0};     // the viewport's pixels: the move a touch makes, and where the shot goes
    };
    Touch touchThisTick(const entt::registry& registry);
    // A popup of `cls` over the level, game time stopped.
    void openPopup(entt::registry& registry, const Popup::Class& cls, bool stepOnResume);
    // One tick under a popup. True when it resumed the level on this tick and the
    // level owes the rest of its tick.
    bool popupTick(entt::registry& registry, const ScriptClock::Step& step);
    // A popup's pieces (sim/Popup) into the screen overlay on a view of `view`
    // design units: the level's, from EmitHud, and chapter select's, from EmitMenu.
    void emitPopup(Supersonic::ScreenOverlay& overlay, const glm::dvec2& view, const Popup::Class& cls,
                   const Popup::Open& open) const;
    // The help blocks the level places, read as it loads.
    void findHelpBlocks();
    // A tap on a help block, read with the rest of the level's input: true when it
    // took the touch, which then fires no portal.
    // The TICK's touch, read once by readInput and handed on: a scheduled tap
    // read twice in one tick would go down and come up inside it.
    bool helpBlockInput(entt::registry& registry, const Touch& touch);
    // A file among the original's assets, the hd twin beside it where one exists.
    std::string originalAsset(const std::string& relative) const;
    // Whether the frame clock has passed the plaque's dismissal, checked on every
    // tick a level is loaded, stopped or not; and, where GameLayer is updated
    // (`updated`), the plaque's alpha.
    void tickPlaqueDismissal(bool updated);
    // The switches: GlobalSoundSwitch sets the global volume, GlobalMusicSwitch
    // the music. Read from m_switches at attach, and saved into it on every
    // toggle, as saveVolume and saveMusicState save them (planning step 124).
    bool m_soundOn{true};
    bool m_musicOn{true};
    Switches::Store m_switches;
    // Game time stood still this level, for LevelFrameMs.
    double m_stoppedMs{0.0};
    // Every tick since the level loaded, stopped or not, in wall milliseconds, for
    // LevelFrameWallMs.
    double m_levelFrameWallMs{0.0};
    // Each pause picture's file resolved to the hd art once per level; empty
    // for one that cannot be read, which is then not drawn.
    std::map<std::string, std::string> m_pauseImages;
    // The fonts the pause and the end screens write in, where they are not the
    // caption's own; read once for the run.
    std::map<std::string, Supersonic::BitmapFont> m_uiFonts;
    const Supersonic::BitmapFont* uiFont(const std::string& name) const;

    // DEV ONLY: the scheduled presses and holds, and the layer's own tick count.
    std::vector<std::pair<int, DevPress>> m_devPresses;
    struct DevHold {
        int from{0};
        int to{0};
        float direction{0.0f};
    };
    std::vector<DevHold> m_devHolds;
    struct DevTap {
        int tick{0};
        int releaseTick{0}; // held from its tick until this one, which releases it
        bool onHelpBlock{false};
        glm::dvec2 viewFraction{0.0};
        bool pressed{false};
        // A drag: where it goes, reached on the release tick, from the tick it went down.
        std::optional<glm::dvec2> toFraction;
        int pressedTick{0};
    };
    std::vector<DevTap> m_devTaps;
    std::vector<int> m_devBurns;
    std::vector<std::pair<int, std::string>> m_devBreaks;
    std::vector<std::pair<int, glm::dvec2>> m_devPlaces;
    int m_ticks{0};

    void buildControls();
    void unloadControls();

    // The pause. Opened only where the pause control could be pressed; closed
    // as UILayer::hide(true) closes it, at once and forgetting its entrance.
    bool pauseAllowed() const;
    void openPause(entt::registry& registry);
    void closePause(entt::registry& registry);
    // Puts every body back where the tap left it, undoing the physics step the
    // app runs before every tick whether or not the game's time is stopped.
    void holdWorld(entt::registry& registry);
    // One tick under the pause. True when it resumed the level on this tick.
    bool pauseTick(entt::registry& registry, const ScriptClock::Step& step);
    // The rest of a level's tick once its input is read: what comes before the
    // next physics step, the camera, the controls and the drawables.
    void stepLevel(entt::registry& registry, float direction, const ScriptClock::Step& step);
    // The walk the keys alone ask for, with no pointer.
    float keyDirection() const;
    // Whether a scheduled DEV press of this kind is due, taking it if so.
    bool devPressDue(DevPress press);
    // The control rectangles for this view and this age, and whether each is
    // shown. Pure layout.
    void layOutControls();
    // The no-portal sign a tick on, against the camera as this tick left it.
    void tickNoPortalSign(double dtMs);
    // Which control the tick's touch is on, or nullptr. Asked once a tick, before
    // a shot is fired, because a tap that works a control must not also open a
    // portal under it - a scheduled tap's as much as a real press's.
    const ControlButton* controlUnderPointer(const Touch& touch) const;
    // Whether the level being played is the one whose pads pulse long and ring.
    bool tutorialPads() const;

    // ---- the earthquake --------------------------------------------------------
    //
    // The generator is TICK state and the shake is FRAME state. Each tick the
    // calls its events made land on their sides of one update (quakeTick), which
    // gives the camera's move for that tick; each frame adds the move to the
    // camera the engine has just drawn between ticks, and to the sky and the
    // satellite pinned to it (shakeView). A tremble is held until a frame has
    // drawn it, so every one is drawn however many ticks a frame runs - for one
    // frame at 60 fps or below, as the original's is - and it is never smeared by
    // the interpolation. Nothing here reaches Game::Level, the tick's camera or
    // the state hash.
    //
    // The level's tick itself: what OnFixedUpdate ran before the earthquake,
    // which now wraps it.
    void fixedTick(entt::registry& registry, const ScriptClock::Step& step);
    // The last thing a tick does (OnFixedUpdate): the tick's end is published as both ends
    // of every interpolated picture and of the camera, so the frame draws each at the
    // tick's own place, as the original does (planning step 150). Not when the display
    // outruns the tick and pictures.json keeps the engine's smoothing.
    void drawPicturesAtTick(entt::registry& registry);
    // What the tick after the physics step did, against what it looked like
    // before: every call site whose event is AfterStep's. Beside latchSimEffects.
    void latchSimQuakes();
    // And the switched doors, which Puzzle moves in BeforeStep: read at the
    // tick's end, so a door's call lands on the tick it starts or lands, and the
    // dust ETHCallback_door_lift adds with the call as it starts is queued there
    // too (the visuals plan's 11.1).
    void latchDoors();
    // Whether a node is one of the entity both wall rows are callbacks of
    // (earthquake.json wall_destroyed.entity, in either spelling): the walls a
    // break shakes the view for and leaves wall_destroy at.
    bool isBreakableWall(const std::string& node) const;
    // The bodies' own impacts (rows 4, 8 and 25): the begins the step made, and
    // a stone's touch of a breakable that Demolish took this tick (`struck`),
    // which the watch cannot see once the breakable is gone.
    void latchImpacts(const std::vector<const Demolish::Breakable*>& struck);
    // Who the impact rows watch, as the step is about to be taken.
    std::vector<Impacts::Watched> impactBodies() const;
    // The update, between the tick's two sides of calls; on a tick with no level
    // shown, nothing (the generator waits, and nothing is drawn).
    void quakeTick();
    // Take off what the last frame added, where the engine has not already put
    // the tick's pose back; then add this tick's move (shakeView).
    void liftQuake(entt::registry& registry);
    void shakeView(entt::registry& registry);
    // The drawn camera's move as the world's y, which runs up.
    static float quakeWorldDy(double units);
    bool levelShown() const;

    Earthquake::Rules m_quakeRules; // the port's earthquake.json
    // g_earthquakeGen: one for the game, and kept across a restart and into the
    // next level, as the script module the original keeps it in is never rebuilt
    // (earthquake.json generator._state; LEFT FOR THE OWNER).
    Earthquake::Generator m_quake;
    double m_quakeOffsetUnits{0.0}; // the move this tick gave the drawn camera
    // What the next frame draws. A tremble is kept until a frame has drawn it,
    // so a frame that runs two ticks or more draws the one its ticks made; the
    // 70 ms beat is five ticks, so no frame the engine runs (five at most) holds
    // two.
    double m_quakeFrameUnits{0.0};
    bool m_quakeFramePending{false};
    // The impact rows' contacts: who carried what into the step, and the pairs
    // that began to touch in it.
    Impacts::Watch m_impacts;
    std::vector<Impacts::Begin> m_impactBegins;
    struct QuakeCall {
        Earthquake::Call call{Earthquake::Call::Fall};
        double scale{1.0};
    };
    std::vector<QuakeCall> m_quakeCalls; // this tick's, in the order they were made
    // What the simulation looked like at the last latch, for the calls that are
    // an edge or a count going up. Counters and one-way flags only; reset with
    // the level, and its first latch sets the mark rather than firing.
    struct QuakeWatch {
        bool valid = false;
        bool fell = false;
        int blasts = 0;
        bool beholderGone = false;
        int beholderHits = 0;
        int rocksBroken = 0;
        bool darkDragonGone = false;
        int darkDragonFired = 0;
        DarkDragon::Phase darkDragonPhase = DarkDragon::Phase::Appearing;
        bool ghostGone = false;
        int ghostShotsEnded = 0;
        std::vector<char> breakablesBroken; // char, not bool: see Watch
        int despawned = 0;
        int minionsKilled = 0;
        int doorsUnlocked = 0;
        std::vector<char> diamondsHeld;
        int struck = 0;
        int wallsGone = 0;
    };
    QuakeWatch m_quakeWatch;
    struct DoorWatch {
        bool valid = false;
        std::vector<char> opening;
        std::vector<float> progress;
        std::vector<int> landings;
    };
    DoorWatch m_doorWatch;
    // What the last frame moved and where each thing stood before it, so the
    // next can put it back where the engine does not: outside its loop, as a
    // suite runs the layer. Put back, not moved back, so nothing drifts by a
    // float's rounding however many frames one tick draws.
    struct QuakeDrawn {
        double units = 0.0;
        bool onCamera = false;
        float cameraRestY = 0.0f;
        std::vector<std::pair<entt::entity, float>> quads;
    };
    QuakeDrawn m_quakeDrawn;
};

} // namespace MagicPortals
