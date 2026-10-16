// Magic Portals as the port plays it. MagicPortalsLayer.hpp says what it is and
// what it is not.

#include "MagicPortalsLayer.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>

#include "core/Application.hpp"
#include "core/DetMath.hpp"
#include "core/Input.hpp"
#include "core/Light2D.hpp"
#include "core/Log.hpp"
#include "core/Raycast.hpp"
#include "core/RenderSettings.hpp"
// For RenderSystem::Stats, which the app publishes into the registry context:
// what the last frame actually drew, culled and refused.
#include "core/RenderSystem.hpp"
#include "core/ScreenOverlay.hpp"
#include "core/SimulationClock.hpp"
#include "core/SpriteAnimationSystem.hpp"
#include "core/ViewportInfo.hpp"
// For handing a level's lightmaps back when it goes. Only through the pointer the
// app publishes in the registry's context, which a suite's bare registry has not.
#include "renderer/TextureRegistry.hpp"

#include "sim/PadArt.hpp"
#include "sim/Player.hpp"
#include "sim/Roles.hpp"
#include "sim/Units.hpp"

#include "stb_image.h"
#include "stb_image_write.h"

namespace MagicPortals {

namespace {

// How far in front of the level the camera stands. It is orthographic, so only
// the ordering matters: everything drawn lies between it and the far plane.
constexpr float kCameraDistance = 20.0f;

// The window's shape until a viewport says otherwise: the 16:9 at which the
// view is 455 px wide.
constexpr float kDefaultAspect = 16.0f / 9.0f;
// And its width in the frame's own pixels: main.cpp's 1280 x 720 window, which a
// sky's cover snaps its edges to (placeSkyCover).
constexpr double kDefaultFrameWidthPx = 1280.0;
// And its height, for the edge strips (syncEdgeCovers).
constexpr double kDefaultFrameHeightPx = 720.0;

// sounds.json's tracks, by the calls that ask for them: playMenuMusic,
// playGameMusic(false) and playGameMusic(true), which is getBossMusicName's.
constexpr const char* kMenuTrack = "menu";
constexpr const char* kGameTrack = "game";
constexpr const char* kBossTrack = "boss";
// The `properties` datum that silences a level: Game::readProperties (bytes
// 116676..118260) #166-181 sets noMusic on its presence (CheckCustomData).
constexpr const char* kNoMusicFlag = "no_music";

// The modern controls' table, and the INFO lines a run says which controls it plays with
// (planning steps 181 and 182). Whoever greps a capture's log for `controls: modern` (the design's N1)
// finds the attach line (which style, and why) and, once per level start, the rectangles' line
// (kControlsModernRectsPrefix): both begin with the words the grep names.
constexpr const char* kControlsFile = "/controls.json";
constexpr const char* kControlsClassicFlag = "controls: classic (--controls classic)";
constexpr const char* kControlsModernFlag = "controls: modern (--controls modern)";
constexpr const char* kControlsClassicDefault = "controls: classic (the shipped default)";
constexpr const char* kControlsModernDefault = "controls: modern (the shipped default)";
constexpr const char* kControlsClassicUnavailable = "controls: classic (modern is unavailable: see the error above)";
constexpr const char* kControlsModernRectsPrefix = "controls: modern, ";
// What the flag's size, opacity and move keys cost until CTL-10 applies them: said once at attach, so a capture
// that asked for one does not take its absence for a bug.
constexpr const char* kControlsKeysHeld =
    "controls: the flag's size, opacity and move keys are checked and held, not applied before CTL-10";

// Markers - buttons, crystals, the exit, portals - stand in front of the bodies.
constexpr float kMarkerZ = 0.5f;
constexpr float kMarkerDepth = 0.1f;

// No-portal zones lie behind everything, thin: shown where nothing covers them,
// and never over the player.
constexpr float kZoneZ = -0.35f;
constexpr float kZoneDepth = 0.02f;
const glm::vec3 kZoneColour(0.45f, 0.14f, 0.16f);
const glm::vec3 kHazardColour(1.00f, 0.25f, 0.10f);

// The level's art, one slot per sprite from the back: flat quads a hair apart,
// in the order Godot draws the canvas (Sprites.hpp), all of it behind the
// markers. The player takes the slot after the last sprite at z_index 0 or
// below, which is where the Godot project's player - added to the level after its
// nodes, at z_index 0 - is drawn. A thrown stone goes just behind it.
constexpr float kSpriteBackZ = -1.5f;
constexpr float kSpriteSlotZ = 0.004f;

float SlotZ(int slot) { return kSpriteBackZ + kSpriteSlotZ * static_cast<float>(slot); }

const glm::vec3 kStaticColour(0.42f, 0.44f, 0.50f);
const glm::vec3 kDoorColour(0.30f, 0.45f, 0.75f);
const glm::vec3 kCrateColour(0.72f, 0.50f, 0.26f);      // teleportable
const glm::vec3 kFixedCrateColour(0.42f, 0.28f, 0.16f); // teleportable 0
const glm::vec3 kStoneColour(0.58f, 0.58f, 0.62f);      // a rolling stone
const glm::vec3 kBreakableColour(0.86f, 0.74f, 0.48f);  // what a stone breaks: sandy, as the walls are
const glm::vec3 kPlayerColour(1.00f, 0.78f, 0.25f);
const glm::vec3 kButtonUpColour(0.80f, 0.22f, 0.18f);
const glm::vec3 kButtonDownColour(0.25f, 0.85f, 0.30f);
const glm::vec3 kCrystalColour(0.35f, 0.90f, 1.00f);
const glm::vec3 kExitColour(0.25f, 0.70f, 0.35f);
const glm::vec3 kExitReachedColour(0.60f, 1.00f, 0.60f);
const glm::vec3 kPortalColour(0.90f, 0.30f, 0.90f);
const glm::vec3 kShotColour(0.60f, 0.60f, 1.00f); // projectile.ent's light is this blue
constexpr double kShotSizePx = 8.0;
const glm::vec3 kStaticRedColour(0.90f, 0.30f, 0.25f);  // a static portal the level colours red
const glm::vec3 kStaticBlueColour(0.30f, 0.50f, 1.00f); // and blue
const glm::vec3 kBeholderColour(0.55f, 0.20f, 0.60f);   // the beholder's reach
const glm::vec3 kSpikeColour(0.85f, 0.85f, 0.70f);
constexpr double kSpikeBoxPx = 6.0;
// A fireball's PLACEHOLDER box, in the colour of its own Light (r 1, g 0.5, b
// 0.2) and at the size of its own Collision (16 x 16), both fireball.ent's. Not
// its picture since step 85: what the original shows is the entity itself, which
// the effects runtime now adds for every fireball in flight, and this is drawn
// only where a level has no art or the boxes are turned on.
const glm::vec3 kFireballColour(1.00f, 0.50f, 0.20f);
constexpr double kFireballBoxPx = 16.0;
// Where the original adds a spike: at z -4 (ETHCallback_beholder).
constexpr int kSpikeZIndex = -4;

bool IsTrigger(const entt::registry& registry, entt::entity entity) {
    using namespace Supersonic;
    if (const auto* box = registry.try_get<BoxColliderComponent>(entity)) return box->isTrigger;
    if (const auto* sphere = registry.try_get<SphereColliderComponent>(entity)) return sphere->isTrigger;
    if (const auto* hull = registry.try_get<ConvexHullColliderComponent>(entity)) return hull->isTrigger;
    return false;
}

std::string Count(int n) { return std::to_string(n); }

// The first top-level node of a role, or null.
const Tscn::Node* FirstOfRole(const Game::Data& data, const char* role) {
    for (const Tscn::Node& node : data.scene.nodes) {
        if (node.parent == "." && Roles::RoleOf(data.roles, node) == role) return &node;
    }
    return nullptr;
}

bool PositionOf(const Tscn::Node* node, glm::dvec2& out) {
    const Tscn::Value* at = node != nullptr ? node->Find("position") : nullptr;
    if (at == nullptr || at->kind != Tscn::Value::Kind::Vector2) return false;
    out = glm::dvec2(at->numbers[0], at->numbers[1]);
    return true;
}

// WHAT THE SCENE'S NUMBERS ARE: display values, as the original's were.
//
// The original drew into an 8-bit GLES2 framebuffer. Every tint, fade and glow
// in its art was multiplied and blended on the encoded bytes, and an additive
// sum clipped at 255. Through the engine's default chain (sRGB decode, bloom,
// Reinhard, encode) a white texel stopped at 186 and 1-1's torch flame at
// (197, 158, 127), where the original's is (253, 252, 159). Enhanced's fit of
// the original's pixels settles which arithmetic it was (its lighting
// design_port.md section 0, fit.md section 4): blending in linear light scores
// 13.59 of 255 over 1,359 unlit blocks against 0.28 on encoded values, and
// 11.90 against 0.92 over 1,042 lightmapped ones. Everything the lighting adds
// after this multiplies those same bytes, so this is not a look to tune but the
// space the rest is defined in.
//
// A flat black behind everything, which also means no sky pass: Ethanon clears
// to black (ETHEngine.cpp:114, and GLES2Video.cpp:87's default), and the
// original's own LoadingScreen::preLoop sets SetBackgroundColor(0xFF000000). A
// display-encoded scene left on the sky is incoherent (RenderSettings.hpp).
//
// No bloom, which the original never drew. The engine records no bloom chain
// for this encoding whatever the intensity says (BloomPass::RunsBloomChain);
// zero makes the scene's own settings say the same.
//
// Quantised to 5/6/5 as the original's 16-bit surface when lighting.json says so
// (step 55, the design's G6): OnAttach reads it, since the rules come with a level.
Supersonic::RenderSettings SceneRendering() {
    Supersonic::RenderSettings rendering;
    rendering.encoding = Supersonic::RenderSettings::SceneEncoding::DisplayEncoded;
    rendering.background = Supersonic::RenderSettings::Background::Color;
    rendering.backgroundColor[0] = 0.0f;
    rendering.backgroundColor[1] = 0.0f;
    rendering.backgroundColor[2] = 0.0f;
    rendering.bloomIntensity = 0.0f;
    return rendering;
}

// An entity's name as its .ent file is named. A level names its entities both
// ways: "portal_static" and "sky.ent".
std::string BareEntityName(const Tscn::Node& node) {
    std::string entity = Roles::EntityName(node);
    if (entity.size() > 4 && entity.compare(entity.size() - 4, 4, ".ent") == 0) entity.resize(entity.size() - 4);
    return entity;
}

// One cell of a particle system's bitmap in units, split evenly by the system's
// SpriteCut: the engine's SpriteAnimationComponent, and the cell wherever the
// bitmap's units divide by the cut. A quad is drawn at the particle's own size
// either way (Particles::QuadPx); only where a cell is sampled differs.
glm::dvec2 ParticleCellUnits(const Sprites::Image& image, const Particles::System& system) {
    return glm::dvec2(image.texels) / static_cast<double>(image.density) /
           glm::dvec2(system.columns, system.rows);
}

// Whether a system's cut sheet is one the even split would sample past, and the
// original's own cut of it when so. SetupSpriteRects strides a cut sheet in whole
// units (gs2d Sprite.cpp:119-130) and DrawParticleSystem sets each particle's rect
// from it (ETHParticleManager.cpp:397-401): where the units divide by the cut the
// flipbook's even split is that same rect, and where they do not - shock_strike.png,
// 512 over 5, and tesla_shock_black_bg.png, 256 over 5 - the quad takes the
// original's (Emitter::wholeUnitCut). One rule for a level's emitters, an effect's
// and the loading screen's.
bool WholeUnitCut(const Sprites::Image& image, const Particles::System& system, Tiers::Cut& out) {
    if (system.Frames() <= 1) return false;
    Tiers::Cut cut;
    std::string why;
    if (!Tiers::FrameCut(image.texels, image.density, system.columns, system.rows, cut, why)) return false;
    if (cut.frameUnits.x * system.columns == cut.units.x && cut.frameUnits.y * system.rows == cut.units.y) {
        return false;
    }
    out = cut;
    return true;
}

} // namespace

MagicPortalsLayer::MagicPortalsLayer(Paths paths, std::string startLevel)
    : m_paths(std::move(paths)), m_startLevel(std::move(startLevel)) {}

const Chapters::Level* MagicPortalsLayer::Current() const {
    return m_current >= 0 ? &m_chapters.levels[static_cast<std::size_t>(m_current)] : nullptr;
}

glm::dvec2 MagicPortalsLayer::ViewPx() const {
    return glm::dvec2(m_viewHeightPx * static_cast<double>(m_aspect), m_viewHeightPx);
}

// ---- attach and detach ------------------------------------------------------

void MagicPortalsLayer::OnAttach(entt::registry& registry) {
    // The port thinks at 60 Hz, and the layer states it: there is no scene to
    // author the clock in.
    auto& clock = registry.ctx().contains<Supersonic::SimulationClock>()
                      ? registry.ctx().get<Supersonic::SimulationClock>()
                      : registry.ctx().emplace<Supersonic::SimulationClock>();
    clock.fixedDelta = kTick;
    // And the scene's encoding (SceneRendering), before anything is drawn, for
    // the menu as for a level. Assigned rather than emplaced: a --scene load
    // may already have put a RenderSettings of its own in the context.
    registry.ctx().insert_or_assign<Supersonic::RenderSettings>(SceneRendering());
    {
        Lighting::Rules lighting;
        std::string why;
        if (Lighting::LoadRules(m_paths.portData + "/lighting.json", lighting, why) && lighting.framebufferRgb565) {
            registry.ctx().get<Supersonic::RenderSettings>().quantize =
                Supersonic::RenderSettings::OutputQuantize::Rgb565;
        }
    }

    bindInput();
    loadSounds();

    // The medals earned before this run, if this build was told where they are
    // kept. An empty saveDir is the ordinary case for a test and means this
    // never opens a file; a file that exists and will not parse is reported and
    // then left alone rather than overwritten.
    if (std::string why; !m_scores.Open(m_paths.saveDir, why)) {
        SUPERSONIC_LOG_WARN("Magic Portals") << "medals not loaded: " << why << std::endl;
    }

    // And the two switches, as main sets the saved volume before anything plays
    // (#103-108) and getState reads the music's. From the same directory, so a
    // suite's bare layer reads and writes nothing and starts with both on - the
    // defaults sounds.json carries, which a table that will not read leaves at
    // Defaults' own, said once.
    {
        Switches::Defaults defaults;
        if (std::string why; !Switches::LoadDefaults(m_paths.portData + "/sounds.json", defaults, why)) {
            SUPERSONIC_LOG_ERROR("Magic Portals") << "switch defaults not read: " << why << std::endl;
        }
        if (std::string why; !m_switches.Open(m_paths.saveDir, defaults, why)) {
            SUPERSONIC_LOG_WARN("Magic Portals") << "switches not read, the defaults kept: " << why << std::endl;
        }
        m_soundOn = m_switches.SoundOn();
        m_musicOn = m_switches.MusicOn();
        if (m_switches.Persists()) {
            SUPERSONIC_LOG_INFO("Magic Portals")
                << "switches: sound " << (m_soundOn ? "on" : "off") << ", music " << (m_musicOn ? "on" : "off")
                << (m_switches.FromFile() ? " (read from " : " (the defaults; a toggle saves them to ")
                << m_switches.Path() << ")" << std::endl;
        }
    }

    // The original's achievements (sim/Achievements.hpp). They are not this
    // repository's, so a machine without Enhanced's out/ has none: the dashboard
    // then draws no rows, and this says why, once. Not a reason to refuse the start.
    if (std::string why; Achievements::Load(m_paths.achievements, m_achievements, why)) {
        m_achievementsLoaded = true;
    } else {
        m_achievementsError = why;
        SUPERSONIC_LOG_WARN("Magic Portals") << "no achievement rows: " << why << std::endl;
    }
    // And the port's own rules for unlocking them (planning step 143), the set the
    // player has unlocked - beside the medals, memory-only for a bare suite - and
    // the Notificator that shows a card. Rules that will not read unlock nothing;
    // they are the port's own and committed, so that is said loudly.
    if (std::string why; Achievements::LoadRules(m_paths.portData + "/achievements.json", m_achievementRules, why)) {
        m_achievementRulesReady = true;
        if (std::string opened; !m_achievementStore.Open(m_paths.saveDir, m_achievementRules.save, opened)) {
            SUPERSONIC_LOG_WARN("Magic Portals") << "achievements not read, none kept: " << opened << std::endl;
        } else if (m_achievementStore.HashRefused()) {
            // loadList #138-202: the list is dropped and the run starts with none.
            SUPERSONIC_LOG_WARN("Magic Portals") << "achievements' hash is not their list's, none kept: "
                                                 << m_achievementStore.Path() << std::endl;
        } else if (m_achievementStore.Persists()) {
            SUPERSONIC_LOG_INFO("Magic Portals") << "achievements: " << m_achievementStore.Ids().size()
                                                 << " unlocked (" << m_achievementStore.Path() << ")" << std::endl;
        }
    } else {
        m_achievementRulesError = why;
        SUPERSONIC_LOG_ERROR("Magic Portals") << "achievement rules not read: " << why << std::endl;
    }
    if (std::string why; Notificator::LoadRules(m_paths.portData + "/achievements.json",
                                                m_paths.portData + "/gravitywell.json", m_notificatorRules, why)) {
        m_notificatorReady = true;
    } else {
        SUPERSONIC_LOG_ERROR("Magic Portals") << "no achievement cards: " << why << std::endl;
    }

    std::string error;
    const bool read = Chapters::Load(m_paths.chapters, m_chapters, error) &&
                      ScriptClock::LoadRules(m_paths.portData + "/clock.json", m_clockRules, error) &&
                      Pictures::LoadRules(m_paths.portData + "/pictures.json", m_pictureRules, error) &&
                      Camera::LoadRules(m_paths.data + "/portals.json", m_cameraRules, error) &&
                      Camera::LoadDrag(m_paths.portData + "/camera.json", m_cameraRules, error) &&
                      Hint::LoadRules(m_paths.portData + "/camera.json", m_hintRules, error) &&
                      Camera::LoadViewHeight(m_paths.portData + "/view.json", m_viewHeightPx, error) &&
                      Art::LoadRules(m_paths.portData + "/art.json", m_artRules, error) &&
                      Sky::LoadRules(m_paths.portData + "/sky.json", m_skyRules, error) &&
                      Sky::LoadSpaceRules(m_paths.portData + "/sky.json", m_spaceRules, error) &&
                      EdgeCover::LoadRules(m_paths.portData + "/edge_cover.json", m_edgeRules, error) &&
                      Tiers::LoadRules(m_paths.portData + "/tiers.json", m_tierRules, error) &&
                      Motion::LoadRules(m_paths.portData + "/motions.json", m_motionRules, error) &&
                      Effects::LoadRules(m_paths.portData + "/effects.json", m_effectRules, error) &&
                      Earthquake::LoadRules(m_paths.portData + "/earthquake.json", m_quakeRules, error) &&
                      Cutscene::LoadRules(m_paths.portData + "/cutscenes.json", m_cutsceneRules, error) &&
                      Hud::LoadRules(m_paths.portData + "/ui.json", m_hudRules, error) &&
                      Pause::LoadRules(m_paths.portData + "/ui.json", m_pauseRules, error) &&
                      LevelEnd::LoadRules(m_paths.portData + "/ui.json", m_levelEndRules, error) &&
                      Popup::LoadRules(m_paths.portData + "/ui.json", m_popupRules, error) &&
                      MainMenu::LoadRules(m_paths.portData + "/ui.json", m_mainMenuRules, error) &&
                      Loading::LoadRules(m_paths.portData + "/ui.json", m_loadingRules, error) &&
                      Credits::LoadRules(m_paths.portData + "/ui.json", m_creditsRules, error) &&
                      Selector::LoadRules(m_paths.portData + "/ui.json", m_selectorRules, error) &&
                      Dashboard::LoadRules(m_paths.portData + "/ui.json", m_dashboardRules, error) &&
                      Locking::LoadRules(m_paths.portData + "/ui.json", m_lockingRules, error);
    // ui.json is the port's own and committed, so a HUD that will not read is a
    // fault in this repository and stops the start as loudly as a missing level.
    m_hudReady = read;
    // Which controls this run plays: after the chain and outside it, before any level is built.
    resolveControls();
    // The tick's two deltas before the first tick runs: what a constructor that moves
    // by "a frame" (the space sky's planet, the loading screen) is handed.
    m_step = ScriptClock::TickStep(m_clockRules, kTick);
    m_tickMs = ScriptClock::Ms(m_step);
    // As EarthquakeGenerator's constructor leaves it, once for the whole run.
    m_quake.Reset(m_quakeRules);
    // The edge strips' pool, before the camera or anything else is made: see buildEdgeCoverPool.
    if (read) buildEdgeCoverPool(registry);
    if (!read) {
        m_loadError = error;
        m_hudError = error;
    } else if (m_startLevel.empty()) {
        // No level named: the loading screen and then the menu, which is what the
        // game itself opens with (owner ruling R4). A named level is entered
        // directly, so --level and every suite reach the game exactly as they did
        // before the menu existed.
        buildCamera(registry);
        openMenu(registry, Screen::Loading);
    } else if (const int start = m_chapters.Find(m_startLevel); start < 0) {
        m_loadError = m_startLevel + " is not a level of " + m_paths.chapters;
    } else {
        buildCamera(registry);
        // The main menu's silent pass (checkPreviouslyUnlockAchievements), which a
        // named level never passes through: run where the menu would have run it,
        // so the first door raises no card for a badge the save had earned before.
        achievementsMenuPass();
        loadLevel(registry, start);
    }
    if (read) {
        // The pictures the menu states draw through the overlay, each resolved to
        // its hd twin once: the backgrounds are entities, named with their
        // directory; the rest are sprites. One that cannot be read says so, once,
        // and is not drawn.
        const auto resolve = [this](const std::string& file, bool named) {
            if (m_menuPictures.find(file) != m_menuPictures.end()) return;
            const std::string path = named ? originalAsset(file) : menuImage(file);
            if (imageSizePx(path).y > 0.0) {
                m_menuPictures[file] = path;
            } else {
                SUPERSONIC_LOG_WARN("Magic Portals") << "menu picture could not be read: " << path << std::endl;
                m_menuPictures[file] = std::string();
            }
        };
        const MainMenu::Rules& menu = m_mainMenuRules;
        resolve(menu.background.sprite, true);
        const std::initializer_list<const std::string*> sprites = {
            &menu.play.sprite,  &menu.title.sprite,     &menu.credits.sprite,   &menu.achievements.sprite,
            &menu.sound.sprite, &menu.soundOffSprite,   &menu.music.sprite,     &menu.music.offSprite,
            &m_loadingRules.logo.sprite};
        for (const std::string* file : sprites) {
            resolve(*file, false);
        }
        // Credits and the dashboard: the backgrounds are entities again, and the
        // achievements' icons live in their own directory, with no hd twins.
        const Credits::Rules& credits = m_creditsRules;
        const Dashboard::Rules& board = m_dashboardRules;
        resolve(credits.background.sprite, true);
        resolve(board.background.sprite, true);
        const std::initializer_list<const std::string*> more = {
            &credits.back.sprite,     &credits.papyrus.sprite,  &credits.strip.sprite, &board.back.sprite,
            &board.rows.barSprite,    &board.rows.lockSprite,   &board.plaque.sprite,  &board.bar.sprite,
            &board.start.sprite};
        for (const std::string* file : more) {
            resolve(*file, false);
        }
        // Chapter select and the grid: the background an entity, the rest sprites.
        const Selector::Rules& selector = m_selectorRules;
        resolve(selector.background.sprite, true);
        const Selector::Rules::Chapters& c = selector.chapters;
        const Selector::Rules::Levels& l = selector.levels;
        for (const std::string* file :
             {&c.title.sprite, &c.back.sprite, &c.forward.sprite, &c.lockSprite, &c.medalGold, &c.medalSilver,
              &c.medalBronze, &c.counter.sprite, &l.back.sprite, &l.forward.sprite, &l.tileSprite,
              &l.lockedTileSprite, &l.bossSprite, &l.medalGold, &l.medalSilver, &l.medalBronze}) {
            resolve(*file, false);
        }
        int worlds = 0;
        for (const Chapters::Level& level : m_chapters.levels) worlds = std::max(worlds, level.world + 1);
        for (int w = 0; w < worlds; ++w) resolve(c.iconPrefix + std::to_string(w) + ".png", false);
        if (m_achievementsLoaded) {
            for (const Achievements::Entry& entry : m_achievements.entries) {
                resolve(board.rows.iconDirectory + entry.icon, true);
            }
            resolve(board.rows.iconDirectory + m_achievements.secret.icon, true);
        }
    }
    if (read && m_notificatorReady && m_achievementRulesReady) {
        // The cards' pictures and their font, and the pause's popup's tiles, each
        // resolved once for the run: a card may come up on any screen.
        const auto card = [this](const std::string& file) {
            if (m_cardImages.find(file) != m_cardImages.end()) return;
            const std::string path = originalAsset(file);
            m_cardImages[file] = imageSizePx(path).y > 0.0 ? path : std::string();
            if (m_cardImages[file].empty()) {
                SUPERSONIC_LOG_WARN("Magic Portals") << "card picture could not be read: " << path << std::endl;
            }
        };
        card(m_achievementRules.card.background);
        card(m_achievementRules.locked.background);
        card(m_achievementRules.popup.tiles);
        if (m_achievementsLoaded) {
            for (const Achievements::Entry& entry : m_achievements.entries) {
                card(m_achievementRules.card.iconDirectory + entry.icon);
            }
        }
        loadUiFont(m_notificatorRules.font);
        loadUiFont(m_achievementRules.popup.font);
    }
    if (m_current < 0 && m_screen == Screen::None) {
        SUPERSONIC_LOG_ERROR("Magic Portals") << "Could not start: " << m_loadError << std::endl;
    }
    buildHud(registry);
    updateHud(registry);
}

void MagicPortalsLayer::resolveControls() {
    const Controls::Policy& policy = m_paths.controls;
    m_controlsRules = Controls::Rules{};
    m_controlsTuning = Controls::Tuning{};
    m_controlsApplied = Controls::Tuning{};
    m_controlsStyle = Controls::Style::Classic;
    m_modernReady = false;
    m_controlsLine.clear();
    m_controlsError.clear();
    m_controlsRectsLine.clear();
    m_controlsRectsPending = false;
    for (std::string& image : m_modernImages) image.clear();

    // The feature does not exist in this run (design 2.1): a bare layer, a developer's run, --controls
    // classic. Nothing is read, so a table that will not read cannot touch such a run. Only the flag
    // says anything, because it was asked for; the other two say nothing, so their log is the log
    // they always had.
    if (!policy.available) {
        if (policy.forced == Controls::Style::Classic) {
            m_controlsLine = kControlsClassicFlag;
            SUPERSONIC_LOG_INFO("Magic Portals") << m_controlsLine << std::endl;
        }
        return;
    }

    // A run that can play modern needs the table and the flag's keys on the table's grids. Either
    // failing costs the modern controls and the run plays classic, with one error that says why.
    std::string why;
    const std::string file = m_paths.portData + kControlsFile;
    if (!Controls::LoadRules(file, m_controlsRules, why)) {
        // `why` names the file and the key, as LoadRules says them.
        m_controlsError = "modern controls are unavailable: " + why;
    } else if (!Controls::Tuning::FromFlag(policy.flag, m_controlsRules, m_controlsTuning, why)) {
        m_controlsTuning = Controls::Tuning{};
        m_controlsError = "modern controls are unavailable: --controls is refused: " + why;
    }
    if (!m_controlsError.empty()) {
        // Not ready means no table at all, never half of one.
        m_controlsRules = Controls::Rules{};
        SUPERSONIC_LOG_ERROR("Magic Portals") << m_controlsError << std::endl;
        m_controlsLine = kControlsClassicUnavailable;
        SUPERSONIC_LOG_INFO("Magic Portals") << m_controlsLine << std::endl;
        return;
    }

    m_modernReady = true;
    // Nothing is saved yet (the store is CTL-9's), so a player's run takes the shipped default.
    m_controlsStyle = Controls::ResolveStyle(policy, std::nullopt, m_controlsRules.defaultMode);
    // THE LAYOUT APPLIES THE LOOK AND NOTHING ELSE until CTL-10: the flag's size, opacity and moves are on
    // m_controlsTuning (read, on the table's grids, held), and the plan applies them there. `preset=` is the
    // one key a capture needs now (the owner's comparison of the three looks).
    m_controlsApplied.preset = m_controlsTuning.preset;
    // WHY the style is what it is: the flag when there was one (it forces its style, a hand-built
    // `{available, forced = Classic}` policy included), otherwise the shipped default.
    const bool modern = m_controlsStyle == Controls::Style::Modern;
    if (policy.forced) {
        m_controlsLine = modern ? kControlsModernFlag : kControlsClassicFlag;
    } else {
        m_controlsLine = modern ? kControlsModernDefault : kControlsClassicDefault;
    }
    SUPERSONIC_LOG_INFO("Magic Portals") << m_controlsLine << std::endl;
    if (modern) {
        const Controls::FlagItems& flag = policy.flag;
        const bool keysHeld = flag.size || flag.opacity ||
                              std::any_of(flag.move.begin(), flag.move.end(),
                                          [](const std::optional<glm::dvec2>& move) { return move.has_value(); });
        if (keysHeld) SUPERSONIC_LOG_INFO("Magic Portals") << kControlsKeysHeld << std::endl;
        // The pictures the five controls are drawn with, once for the run.
        resolveModernImages();
    }
}

void MagicPortalsLayer::resolveModernImages() {
    for (const Control control : Controls::kClaimOrder) {
        m_modernImages[static_cast<std::size_t>(control)] = modernGlyphImage(control);
    }
}

std::string MagicPortalsLayer::modernGlyphImage(Control control) {
    // THE GENERATED TIER (design 4.2, tier 3): the glyph sim/PadArt draws, written as a PNG into the directory the layer
    // is given for what it generates (Paths::prisms: the multiplies' own, a temporary directory in main and in the suites).
    // Never into a tracked path and never next to the saves: that is the hard rule's reason for drawing the glyphs instead
    // of keeping them. With no such directory the control is a flat quad (tier 4), which is what every suite draws.
    if (m_paths.prisms.empty()) return {};
    const auto& entry = m_controlsRules.art[static_cast<std::size_t>(Controls::ArtTier::Generated)][static_cast<std::size_t>(control)];
    if (!entry) return {};
    const int px = static_cast<int>(std::lround(m_controlsRules.boxPx));
    const PadArt::Bitmap bitmap = PadArt::Draw(PadArt::GlyphOf(control), px, entry->bodyInset);
    if (bitmap.Empty()) {
        SUPERSONIC_LOG_WARN("Magic Portals") << "the modern " << Controls::KeyOf(control) << " could not be drawn at " << px
                                             << " px: it is a flat quad" << std::endl;
        return {};
    }
    std::error_code ec;
    std::filesystem::create_directories(m_paths.prisms, ec);
    // Rewritten at every attach and never skipped for being there: the picture is a function of the data and of
    // sim/PadArt, so a file left by an older build would otherwise be drawn in their place.
    const std::filesystem::path target = m_paths.prisms / entry->file;
    if (stbi_write_png(target.string().c_str(), bitmap.size, bitmap.size, 4, bitmap.rgba.data(), bitmap.size * 4) == 0) {
        SUPERSONIC_LOG_WARN("Magic Portals") << "a modern control's picture could not be written: " << target.string()
                                             << " - it is a flat quad" << std::endl;
        return {};
    }
    return target.string();
}

void MagicPortalsLayer::OnDetach(entt::registry& registry) {
    // Before the entities go: a looping voice is never "finished", so nothing
    // else will ever free it.
    stopMusic(registry);
    unloadLevel(registry);
    unloadMenu(registry);
    unloadEdgeCovers(registry);
    auto destroy = [&registry](entt::entity& e) {
        if (e != entt::null && registry.valid(e)) registry.destroy(e);
        e = entt::null;
    };
    destroy(m_camera);
    destroy(m_light);
    destroy(m_hud.status);
    destroy(m_hud.result);
    destroy(m_hud.controls);
    m_current = -1;
}

void MagicPortalsLayer::bindInput() {
    using namespace Supersonic;
    Input::BindActionKey(kLeft, Key::Left);
    Input::BindActionKey(kLeftAlt, Key::A);
    Input::BindActionKey(kRight, Key::Right);
    Input::BindActionKey(kRightAlt, Key::D);
    Input::BindActionMouseButton(kTap, MouseButton::Left);
    Input::BindActionKey(kRetry, Key::R);
    Input::BindActionKey(kSkip, Key::N);
    Input::BindActionKey(kBoxes, Key::B);
    Input::BindActionKey(kBack, Key::Escape);
    // G, because Key has no function keys at all - it stops at the letters,
    // the arrows and the modifiers.
    Input::BindActionKey(kDump, Key::G);
}

// ---- levels -----------------------------------------------------------------

bool MagicPortalsLayer::loadLevel(entt::registry& registry, int index) {
    // A retry draws the same level again at once, so its lightmaps stay where
    // they are: a retry does not touch the disk (level_manager.gd:5-13).
    unloadLevel(registry, m_loaded && index == m_current);
    m_current = index;
    m_loadError.clear();
    // Every level begins here - the first one, a retry, a skip, and going on
    // from a medal all come through - so this is the one place the finish beat
    // has to be forgotten. Left set, the NEXT level would open already on its
    // way to being scored and would put its medal up 1400 ms in, having been
    // played by nobody.
    m_finishing = false;
    m_finishClockMs = 0.0;
    // And the death beat, for the same reason: left set, the next level would
    // open already dying and put the lost screen up 1400 ms in.
    m_dying = false;
    m_dyingClockMs = 0.0;
    // And the cutscenes that play in the beat: a retry plays its level's again
    // from the start, and a level without one plays none (planning step 123).
    m_wonDelayMs = m_levelEndRules.wonDelayMs;
    m_launchPlaced = false;
    m_landingPlaced = false;
    m_launch = Cutscene::Launch{};
    m_landing = Cutscene::Landing{};
    m_knight = Cutscene::Knight{};
    m_launchStartedTick = -1;
    m_landingStartedTick = -1;
    m_launchQuakes = 0;
    m_knightSpawnedTick = -1;
    m_knightEnteredTick = -1;
    m_knightEnteredAtPx = glm::dvec2(0.0);
    m_cameraTargetPx = glm::dvec2(0.0);
    // And the screen either raises, with its counters, and the HUD's last byte.
    m_end = EndScreen{};
    m_padEndByte = 0;
    m_clearShownAtEnd = false;
    // And the level's age, which everything it opens with is timed by. A retry
    // is a new level to the original - its restart button builds a fresh Game
    // state, whose preLoop starts the black, "Part N" and the plaque again
    // (GameLayer::update, bytes 100358..100663) - so it is zero here too.
    m_levelAgeMs = 0.0;
    m_levelStepMs = 0.0;
    // And the time a pause or a popup stood it still, which the new level has had
    // none of, and the plaque's dismissal, which it has not reached.
    m_stoppedMs = 0.0;
    m_levelFrameWallMs = 0.0;
    m_plaqueDismissAgeMs = -1.0;
    m_plaqueAlpha = 0.0;
    const Chapters::Level& entry = m_chapters.levels[static_cast<std::size_t>(index)];

    // A refused level says why on the HUD, which a player reads, and in the log,
    // which is all a run leaves behind afterwards. Without the second, a chapter
    // that stops at its fourteenth level leaves nothing to say which one that
    // was: the message below is the only record. It covers the first level too,
    // which `m_current` being set above keeps out of OnAttach's own report.
    const auto refuse = [this, &entry](std::string why) {
        m_loadError = std::move(why);
        SUPERSONIC_LOG_ERROR("Magic Portals") << entry.name << " refused: " << m_loadError << std::endl;
        return false;
    };

    std::string error;
    if (index != m_dataIndex) {
        m_dataIndex = -1;
        m_lit = false;
        m_look = Lighting::Scene{};
        if (!Game::LoadData(m_paths.levels + "/" + entry.name + ".tscn", m_paths.data, m_paths.prisms, m_data,
                            error, m_paths.portData)) {
            return refuse(error);
        }
        m_dataIndex = index;
        // Its lighting, read once with it. A level whose lighting will not read
        // is still played, its sprites in their own colours - as a level whose
        // art will not read is still played as boxes - and says why. The same
        // read carries each node's eth_particles, so such a level draws none of
        // its placements' particle systems either (planning step 91): one reader
        // of the key, and a refused read says what it cost. No shipped level is
        // refused (test_mp_lighting: 128 of 128 read).
        m_lit = Lighting::Read(m_data.scene, m_paths.art, m_tierRules, m_look, m_lightingError);
        if (m_lit) {
            m_lightingError.clear();
        } else {
            SUPERSONIC_LOG_WARN("Magic Portals") << entry.name << " drawn unlit, and without its placements' particle "
                                                 << "systems: " << m_lightingError << std::endl;
        }
    }
    if (!Game::Start(m_data, registry, m_level, error)) {
        // Start may have built some of the level before it refused.
        unloadLevel(registry);
        return refuse(error);
    }
    // The world's gravity in the carried frame of THIS layer's tick (ScriptClock.hpp), before the engine's first
    // physics step: Start wrote it at ratio 1, and every BeforeStep rewrites it from the tick's own step.
    if (!m_level.noGravity) Game::UseWorldGravity(registry, m_data.tuning, m_step);
    if (!PositionOf(FirstOfRole(m_data, Roles::kLevelBounds), m_boundsPx) || m_boundsPx.x <= 0.0 ||
        m_boundsPx.y <= 0.0) {
        unloadLevel(registry);
        return refuse(entry.name + " has no level_bounds");
    }
    // Where the camera starts, or the spawn in a level that places none
    // (level_runtime.gd:214-215).
    glm::dvec2 cameraStartPx(0.0);
    if (!PositionOf(FirstOfRole(m_data, "camera_start"), cameraStartPx)) {
        PositionOf(FirstOfRole(m_data, Roles::kPlayerSpawn), cameraStartPx);
    }

    // WHAT IT PLAYS BETWEEN ITS DOOR AND ITS MEDAL (cutscenes.json): its end_delay,
    // and which of the three cutscenes it places - each by its entity, as the
    // original's callbacks are, and none by the level's name.
    if (const std::optional<double> delay = Cutscene::EndDelayMs(m_cutsceneRules, m_data.scene)) {
        m_wonDelayMs = *delay;
        SUPERSONIC_LOG_INFO("Magic Portals") << entry.name << " sets end_delay " << m_wonDelayMs
                                             << " ms from its door to its medal" << std::endl;
    }
    m_launchPlaced = Cutscene::Places(m_data.scene, m_cutsceneRules.launch.entity);
    m_landingPlaced = Cutscene::Places(m_data.scene, m_cutsceneRules.landing.entity);
    if (std::string why; !Cutscene::FindKnight(m_cutsceneRules, m_data.scene, m_knight, why)) {
        // Played without its knight rather than refused: it is presentation after
        // the level is finished, and the level plays the same without it.
        SUPERSONIC_LOG_WARN("Magic Portals") << entry.name << " plays no knight: " << why << std::endl;
        m_knight = Cutscene::Knight{};
    }

    m_loaded = true;
    // Its crates, whose begin-contacts are crateContactCallback's (sounds.json
    // impacts.crate; planning step 142), in the scene's order so that two begun on
    // one tick are latched in the same order every run.
    m_crates.clear();
    {
        const std::vector<std::string>& crates = m_soundRules.impacts.crate.entities;
        for (const Tscn::Node& node : m_data.scene.nodes) {
            if (node.parent != "." ||
                std::find(crates.begin(), crates.end(), Roles::EntityName(node)) == crates.end()) {
                continue;
            }
            const auto built = m_level.built.entities.find(node.name);
            if (built != m_level.built.entities.end()) m_crates.push_back(built->second);
        }
    }
    // Its music, as Game::preLoop starts it - a retry and a skip included, each a
    // fresh Game state - and the bosses' calls watched from nothing.
    m_musicWatch = MusicWatch{};
    startLevelMusic(index);
    // The lightmaps this level names, held until it goes. The same list again
    // after a retry, which kept the last one.
    m_heldLightmaps.clear();
    if (m_lit) {
        for (const auto& [node, look] : m_look.nodes) {
            if (!look.lightmap.empty()) m_heldLightmaps.push_back(look.lightmap);
        }
        // The nodes are hashed: sorted, so the list does not depend on the map.
        std::sort(m_heldLightmaps.begin(), m_heldLightmaps.end());
        m_heldLightmaps.erase(std::unique(m_heldLightmaps.begin(), m_heldLightmaps.end()), m_heldLightmaps.end());
    }
    // The player's depth, where it is drawn among the art and its lighting height:
    // GameCharacter's 4, in the world's units, over the scale factor the level's own
    // nodes were multiplied by (art.json character.z and depth; R10). Its marker
    // gives only x and y (GetPositionXY): before step 76 this was the marker's
    // eth_z, 0 but for 2-09's 2.
    m_playerZ = m_artRules.LevelDepth(m_artRules.character.z);
    buildDrawables(registry);
    m_aspect = viewportAspect(registry);
    // THE LEVEL OPENS ON THE PLAYER, by the owner's choice.
    //
    // The original puts its camera on camera_start - which sits near a level's
    // exit (Game::readCameraStartPos) - and eases to the player from its first
    // frame: there is no opening hold. cameraHoldTime is what a drag away from the
    // player starts, and the constructor leaves it spent (step 138; before it, this
    // note read it as a hold on camera_start, and the port carried a guessed
    // hold_time_s it zeroed here). Shown a level, the owner asked for the camera to
    // begin already framed on the character rather than pan in from the goal, so
    // Start takes the player's own place. A level that wants the pan in from
    // camera_start back needs only this line.
    glm::dvec2 openOnPx = cameraStartPx;
    if (m_level.player != entt::null && registry.valid(m_level.player)) {
        openOnPx = Units::ToPixels(registry.get<Supersonic::TransformComponent>(m_level.player).position);
    }
    m_follow.Start(m_cameraRules, openOnPx, ViewPx(), m_boundsPx);
    // AFTER buildDrawables, which is what sets m_artReady at the end of
    // buildSprites: built before it, the controls would find the flag still
    // false and quietly make nothing at all.
    buildControls();
    placeCamera(registry);
    layOutControls();
    logModernControls(registry);
    // The sign starts from where the level put it, aimed at this camera.
    tickNoPortalSign(0.0);
    // A new level is a cut, not a pan: there is nothing to draw the camera
    // coming from.
    if (m_camera != entt::null && registry.valid(m_camera)) {
        if (auto* interpolated = registry.try_get<Supersonic::InterpolatedCameraComponent>(m_camera)) {
            interpolated->captured = false;
        }
    }
    syncDrawables(registry);
    // The help blocks it places, and the popup it opens as it loads, which
    // Game::preLoop raises in the load frame (Game::managePopups, bytes
    // 115157..115478): a retry of 1-02 raises it again. Only where the HUD is
    // drawn at all - a level drawn as boxes has no UI to put one in.
    findHelpBlocks();
    if (m_artReady && m_hudReady) {
        if (const Popup::Class* popup = Popup::LevelStartClass(m_popupRules, entry.name)) {
            openPopup(registry, *popup, false);
        }
    }
    return true;
}

void MagicPortalsLayer::unloadLevel(entt::registry& registry, bool keepLightmaps) {
    m_achievementKeyOwners.clear();
    restoreEdgeCulls(registry);
    // THE LIGHTMAPS GO BACK WITH THE LEVEL. Each lightmapped sprite holds a
    // texture and a material set of its own (730 across the game), and the
    // registry keeps both until told to drop them; a session walking the chapters
    // would otherwise hold every level's at once (the lighting design's section
    // 6, and step 42's walk). A retry keeps them: it draws them again at once.
    if (!keepLightmaps) releaseLightmaps(registry);
    auto destroy = [&registry](entt::entity& e) {
        if (e != entt::null && registry.valid(e)) registry.destroy(e);
        e = entt::null;
    };
    for (Drawn& drawn : m_bodies) destroy(drawn.box);
    m_bodies.clear();
    for (auto& e : m_buttons) destroy(e);
    m_buttons.clear();
    for (auto& e : m_crystals) destroy(e);
    m_crystals.clear();
    for (auto& e : m_portals) destroy(e);
    m_portals.clear();
    for (auto& e : m_statics) destroy(e);
    m_statics.clear();
    for (auto& e : m_zones) destroy(e);
    m_zones.clear();
    for (auto& e : m_agents) destroy(e);
    m_agents.clear();
    for (auto& e : m_hazards) destroy(e);
    m_hazards.clear();
    for (ThrownBox& thrown : m_thrown) {
        destroy(thrown.box);
        destroy(thrown.quad);
    }
    m_thrown.clear();
    for (DrawnSprite& drawn : m_sprites) {
        destroy(drawn.quad);
        destroy(drawn.cover);
    }
    m_sprites.clear();
    hideEdgeCovers(registry);
    clearEdgeCoverLooks(registry);
    m_edgeSeen.clear();
    m_edgeLogged = 0;
    m_motions.clear();
    m_placed.clear();
    m_antiportalTurn = Motion::Turn{};
    for (TimerDial& dial : m_timers) destroy(dial.quad);
    m_timers.clear();
    destroy(m_wallGhost.quad);
    m_wallGhost = WallGhost{};
    for (BlackBarQuad& bar : m_blackBars) destroy(bar.quad);
    m_blackBars.clear();
    for (ShockRingQuad& ring : m_shockRings) destroy(ring.quad);
    m_shockRings.clear();
    for (MinionQuad& minion : m_minions) destroy(minion.quad);
    m_minions.clear();
    destroy(m_ghost.quad);
    m_ghost = GhostQuad{};
    m_ghostAppearQueued = false;
    m_darkDragonQueued = false;
    // 2-32's knight, its picture and the body its spawn added: neither belongs to
    // the level's Built. Its aura is the effects runtime's, which goes below.
    destroy(m_knightQuad);
    destroy(m_knight.body);
    m_knightAuraQueued = false;
    m_bridgeSprite = -1;
    m_landingKeySprite = -1;
    // The walk arrows and the corner buttons go with the level they were built
    // for, and so does what the level opened with.
    unloadControls();
    // And a pause or a popup over it: whatever it held goes with the bodies it
    // held, and so do the help blocks and a touch on one.
    m_pause = PauseScreen{};
    m_popup = PopupScreen{};
    m_frozen = Frozen{};
    m_helpBlocks.clear();
    m_helpTouch = HelpTouch{};
    // And the touch a shot waits on: a press that restarted the level fires
    // nothing in the new one when it comes up.
    m_shotTouch = Portals::TouchTrack{};
    m_handTouch = Portals::TouchTrack{};
    m_pressedControl.reset();
    // And the camera's record of it: a touch still held into the next level is
    // taken up there where it is, and moves nothing until it moves.
    m_cameraTouch = Camera::TouchRecord{};
    m_cameraFeel = Camera::Feel{};
    m_cameraPadMoving = false;
    // And 1-21's lesson, whose callbacks start again with a new level.
    m_hintHand = Hint::Hand{};
    m_hintArrow = Hint::Arrow{};
    m_hintHandPlaced = false;
    m_hintArrowPlaced = false;
    // And 2-20's, and the handles' reading of a touch.
    m_lesson = Handle::Lesson{};
    m_lessonHandle = -1;
    m_handleHitPx.reset();
    // The particles the level's entities were emitting go with them; a retry
    // would otherwise pile a second pool on the first. So do its lights, and the
    // shot's, and everything the script added while it ran.
    unloadEmitters(registry);
    unloadEffects(registry);
    unloadLights(registry);
    // The next level starts its own comparison. Without this, the first tick
    // of a level would hear every counter fall back to zero as if it had
    // happened - a retry would play the whole level's sounds at once. And the
    // sounds' one-shot data and clocks, which are the level's entities'.
    m_watch = Watch{};
    m_soundCues = SoundCues{};
    m_crates.clear();
    // And the earthquake's two, for the same reason. The generator itself is
    // NOT reset: the original's is one for the game (m_quake).
    m_quakeWatch = QuakeWatch{};
    m_doorWatch = DoorWatch{};
    // And the contacts: the next level's bodies are looked at afresh, and only
    // what begins to touch after that first look is an impact.
    m_impacts.Reset();
    m_impactBegins.clear();
    // The next level is compared against itself, not against this one. Without
    // this, its first frame reports every sprite of the level just unloaded as
    // having changed - the same stale-baseline fault the sound watch above has.
    m_onScreenLast.clear();
    m_reportedOnce = false;
    m_artReady = false;
    m_portalDraws.clear();
    destroy(m_shotQuad);
    destroy(m_playerQuad);
    destroy(m_beholderBox);
    destroy(m_beholderQuad);
    m_beholderColour = glm::vec4(1.0f);
    for (auto& e : m_spikes) destroy(e);
    m_spikes.clear();
    for (auto& e : m_fireballs) destroy(e);
    m_fireballs.clear();
    destroy(m_shot);
    destroy(m_player);
    destroy(m_exit);
    // And the level's own bodies, with what its launchers threw.
    for (auto& [name, entity] : m_level.built.entities) {
        entt::entity e = entity;
        destroy(e);
    }
    for (const Launchers::Thrown& thrown : m_level.launchers.live) {
        entt::entity e = thrown.body;
        destroy(e);
    }
    for (const Boss::Rock& rock : m_level.boss.rocks) {
        entt::entity e = rock.body;
        destroy(e);
    }
    // The platform the dark dragon's death added, which belongs to no Built and
    // so is not in the loop above.
    {
        entt::entity e = m_level.darkDragon.platformBody;
        destroy(e);
    }
    m_platformDrawn = false;
    m_platformBoxed = false;
    m_restockDrawn = 0;
    m_restockBoxed = 0;
    m_restockRefusalSaid = false;
    destroy(m_level.player);
    m_level = Game::Level{};
    m_loaded = false;
    m_ambient = glm::dvec3(1.0);
}

void MagicPortalsLayer::releaseLightmaps(entt::registry& registry) {
    if (auto* const* textures = registry.ctx().find<Supersonic::TextureRegistry*>();
        textures != nullptr && *textures != nullptr) {
        // A path never acquired is simply not there; Invalidate says false and
        // does nothing. Since step 47 each is acquired by the sprite that draws
        // it, the first frame the level is drawn. Before the sprites go, which
        // is safe: nothing renders between here and their destruction below.
        for (const std::string& path : m_heldLightmaps) (*textures)->Invalidate(path);
    }
    m_lightmapsHandedBack += m_heldLightmaps.size();
    m_heldLightmaps.clear();
}

void MagicPortalsLayer::goTo(entt::registry& registry, int next) {
    if (next >= 0) {
        loadLevel(registry, next);
        return;
    }
    // goToNextLevel (LevelFinishedLayer bytes 408734..409098): a chapter's last
    // level goes to WorldSelector, never on to the next chapter's first level, and
    // chapter select opens on the page of the chapter just played (its preLoop
    // #6-21). After the last chapter's last level it opens GameFinishedPopup over
    // it (#68-72, preLoop #69-101). Read before openMenu, which forgets the level.
    const int at = m_current;
    const bool finished = m_chapters.FinishesTheGame(at);
    if (at >= 0) m_menuWorld = m_chapters.levels[static_cast<std::size_t>(at)].world;
    openMenu(registry, Screen::Worlds);
    if (!finished) return;
    if (const Popup::Class* cls = m_popupRules.FindClass(m_popupRules.gameFinished)) openMenuPopup(*cls);
}

// Defined below. Declared here because recordWin records the FINAL medal and
// comes first in this file.
int MedalFor(const MagicPortalsLayer::Cleared& cleared);

void MagicPortalsLayer::recordWin() {
    const Chapters::Level& entry = m_chapters.levels[static_cast<std::size_t>(m_current)];
    Cleared cleared;
    cleared.name = entry.name;
    cleared.label = Chapters::Label(entry);
    cleared.portalsUsed = m_level.portals.portalsUsed;
    cleared.goldenScore = entry.goldenScore;
    cleared.traversals = m_level.portals.traversals;
    cleared.crystalsTotal = static_cast<int>(m_level.goals.crystals.size());
    cleared.crystals = cleared.crystalsTotal - m_level.goals.Remaining();
    m_lastCleared = cleared;

    // THE FINAL MEDAL, not the one the screen will count up to.
    //
    // The finished screen's medal follows its counter as it rises, which is a
    // picture (LevelEnd::ShownScore); this is the result, written from a
    // different object - GameStateController::writeScore, not the layer that
    // animates it - for the same reason.
    //
    // ON THE DOOR'S TICK, as the original writes it: checkGameEnd calls
    // writeScore (#75) on the frame the character goes in, its setScore (#48)
    // writes the file there and then, and the LevelFinishedLayer it builds with
    // this play's counts (#50-69) is made current only once gameWonDelay has
    // passed. So nothing in the beat - R, N, the window closed - loses the medal,
    // on 4-19's 14.5 s as on the 1.4 s of every other level; and the screen counts
    // up to the door's tick's numbers, which m_lastCleared keeps for it.
    //
    // Saved only when it changed. ScoreManager::setScore writes only when the
    // new score beats the old, so a replay that goes worse takes nothing away
    // and costs no write at all.
    const int medal = MedalFor(cleared);
    if (m_scores.Record(entry.world, entry.index, medal)) {
        if (std::string why; !m_scores.Save(why)) {
            SUPERSONIC_LOG_WARN("Magic Portals") << "medals not saved: " << why << std::endl;
        } else {
            SUPERSONIC_LOG_INFO("Magic Portals") << cleared.label << " medal " << medal
                                                 << (m_scores.Persists() ? " saved" : " kept (no save directory)")
                                                 << " on tick " << m_ticks << std::endl;
        }
    } else {
        SUPERSONIC_LOG_INFO("Magic Portals") << cleared.label << " medal " << medal << " on tick " << m_ticks
                                             << ": the save's " << m_scores.Get(entry.world, entry.index)
                                             << " is as good" << std::endl;
    }
}

// The medal a play earns, as computeScore has it (ScoreManager.angelscript,
// bytes 363979..364204): 3 is gold, 2 silver, anything else bronze. One
// implementation, LevelEnd::ComputeScore, which the finished screen's live medal
// uses too.
int MedalFor(const MagicPortalsLayer::Cleared& cleared) {
    return LevelEnd::ComputeScore(cleared.portalsUsed, cleared.goldenScore, cleared.crystals, cleared.crystalsTotal);
}

// ---- achievements (planning step 143) ------------------------------------------------

namespace {

// What checkForLevelAchievements asks of the level, answered from the simulation
// at the door's tick (achievements.json level_world).
class LevelWorld final : public Achievements::World {
public:
    LevelWorld(const entt::registry& registry, const Game::Data& data, const Game::Level& level,
               const Achievements::Rules& rules)
        : m_registry(registry), m_data(data), m_level(level), m_rules(rules) {}

    int Count(const std::vector<std::string>& names) const override {
        int count = 0;
        // What the level placed, by its entity_name, while it is still in the world.
        for (const Tscn::Node& node : m_data.scene.nodes) {
            if (node.parent != ".") continue;
            const std::string* entity = EntityName(node);
            if (entity == nullptr || !Named(names, *entity)) continue;
            if (Alive(node.name)) ++count;
        }
        // And what was added since: a launcher's stones, the markers' minions.
        for (const Launchers::Thrown& thrown : m_level.launchers.live) {
            if (thrown.launcher >= m_level.launchers.launchers.size()) continue;
            if (!Named(names, m_level.launchers.launchers[thrown.launcher].entity)) continue;
            if (thrown.body != entt::null && m_registry.valid(thrown.body)) ++count;
        }
        for (const Minions::Minion& minion : m_level.minions.minions) {
            if (!Named(names, minion.entity) || minion.gone) continue;
            if (minion.body != entt::null && m_registry.valid(minion.body)) ++count;
        }
        return count;
    }

    bool Exists(const std::string& name) const override {
        // A torch: unlit, under that name or put back out by a signal, which adds
        // a fresh signal_torch whatever the first was called.
        const auto& torchNames = m_level.torch.rules.torchNames;
        if (std::find(torchNames.begin(), torchNames.end(), name) != torchNames.end()) {
            for (const Torch::Light& light : m_level.torch.lights) {
                if (light.lit) continue;
                const Tscn::Node* node = m_data.scene.FindNode(light.name);
                const std::string* entity = node != nullptr ? EntityName(*node) : nullptr;
                if ((entity != nullptr && *entity == name) || (light.switched && name == m_rules.signalTorch)) {
                    return true;
                }
            }
            return false;
        }
        return Count({name}) > 0;
    }

    bool LiftDoorsShut(const std::vector<std::string>& names) const override {
        for (const Tscn::Node& node : m_data.scene.nodes) {
            if (node.parent != ".") continue;
            const std::string* entity = EntityName(node);
            if (entity == nullptr || !Named(names, *entity)) continue;
            // GetInt('status') == 0, the datum ETHCallback_door_lift keeps and step
            // 133's Mover::Door keeps as it does: Shut is set on the frame after the
            // door is back at its original place, and left on the frame a press starts
            // it rising (wave 2's merge, step 144). A lift door no switch moves keeps 0.
            const Puzzle::SwitchedDoor* door = m_level.channels.FindDoor(node.name);
            if (door != nullptr && door->motion.status != Mover::DoorStatus::Shut) return false;
        }
        return true;
    }

    int SpottedCount() const override {
        // ScoreDashboard's spottedCount (401234..401465): every live minion.ent's enemySpotCount, summed - the sightings
        // Minions::State::Tick counts (G1a, planning step 158), the black knight's not among them. hasBeenUnnoticed (the
        // seven stealth achievements: 24, 26, 28, 32, 42, 66, 76) asks for it to be 0 (G1b, planning step 159).
        return m_level.minions.SpotCount();
    }

    int Laps() const override {
        // SeekEntity('gravity_area') is the level's first well; the body is the
        // player's.
        if (m_level.wells.wells.empty()) return 0;
        for (const GravityWell::Sweep& sweep : m_level.wells.sweeps) {
            if (sweep.body == m_level.player && sweep.well == 0) return sweep.cw + sweep.ccw;
        }
        return 0;
    }

private:
    static const std::string* EntityName(const Tscn::Node& node) {
        const Tscn::Value* entity = node.Meta("entity_name");
        if (entity == nullptr || entity->kind != Tscn::Value::Kind::String) return nullptr;
        return &entity->text;
    }

    static bool Named(const std::vector<std::string>& names, const std::string& name) {
        return std::find(names.begin(), names.end(), name) != names.end();
    }

    bool Alive(const std::string& node) const {
        for (const Portals::Agent& agent : m_level.portals.agents) {
            if (agent.name == node) return !agent.gone;
        }
        const auto found = m_level.built.entities.find(node);
        if (found != m_level.built.entities.end()) return m_registry.valid(found->second);
        return true; // no body, and nothing that takes it away
    }

    const entt::registry& m_registry;
    const Game::Data& m_data;
    const Game::Level& m_level;
    const Achievements::Rules& m_rules;
};

} // namespace

void MagicPortalsLayer::unlockAchievement(int id, bool notify, double delayMs, const char* why) {
    if (!m_achievementRulesReady) return;
    // unlock #3-8: an id already unlocked is nothing at all - no card either.
    if (!m_achievementStore.Add(id)) return;
    m_unlocks.push_back(Unlock{id, m_ticks, notify});
    // #184-185 saveList, notify or not.
    std::string saved;
    if (std::string why2; !m_achievementStore.Save(why2)) {
        SUPERSONIC_LOG_WARN("Magic Portals") << "achievements not saved: " << why2 << std::endl;
        saved = " (not saved)";
    } else if (!m_achievementStore.Persists()) {
        saved = " (kept, no save directory)";
    }
    SUPERSONIC_LOG_INFO("Magic Portals") << "achievement " << id << " unlocked on tick " << m_ticks << " by " << why
                                         << saved << (notify ? ", its card on the stack" : "") << std::endl;
    if (!notify || !m_notificatorReady) return;
    // #77-180: the card, with the entry's own title and icon - none to show
    // without Enhanced's list, which says why once at attach.
    const Achievements::Entry* entry = m_achievementsLoaded ? m_achievements.Find(id) : nullptr;
    if (entry == nullptr) return;
    Notificator::Card card;
    card.id = id;
    card.background = m_achievementRules.card.background;
    card.message = m_achievementRules.card.messagePrefix + entry->title;
    card.icon = m_achievementRules.card.iconDirectory + entry->icon;
    card.sound = m_achievementRules.card.soundEvent;
    card.durationMs = m_achievementRules.card.durationMs;
    card.delayMs = delayMs;
    m_notificator.Notify(std::move(card));
}

void MagicPortalsLayer::achievementsForChapter(int world, bool notify) {
    if (!m_achievementRulesReady) return;
    // registerMedalAchievements #1-5: computeWorldAccomplishment over the saved medals.
    const int percent = Locking::ChapterCompletion(m_lockingRules, m_scores, m_chapters, world);
    const std::string why = "chapter " + std::to_string(world + 1) + " at " + std::to_string(percent) + "%";
    for (const int id : Achievements::CompletionIds(m_achievementRules, world, percent)) {
        unlockAchievement(id, notify, m_achievementRules.completionDelayMs, why.c_str());
    }
}

void MagicPortalsLayer::achievementsMenuPass() {
    if (!m_achievementRulesReady || !m_hudReady) return;
    // checkPreviouslyUnlockAchievements (202712..202943): every world's badges, then
    // its levels with the saved medal, 99999 portals, time 0 and not in the game.
    int worlds = 0;
    for (const Chapters::Level& level : m_chapters.levels) worlds = std::max(worlds, level.world + 1);
    for (int world = 0; world < worlds; ++world) {
        achievementsForChapter(world, false);
        for (int index = 0; index < Locking::LevelsIn(m_chapters, world); ++index) {
            Achievements::Play play;
            play.world = world;
            play.level = index;
            play.medal = m_scores.Get(world, index);
            play.portals = m_achievementRules.menuPortals;
            play.gameTimeMs = m_achievementRules.menuGameTimeMs;
            play.game = false;
            for (const int id : Achievements::Dispatches(m_achievementRules, play, nullptr)) {
                unlockAchievement(id, false, m_achievementRules.levelDelayMs, "the main menu's pass");
            }
        }
    }
}

void MagicPortalsLayer::achievementsAtDoor(const entt::registry& registry, double gameTimeMs) {
    if (!m_achievementRulesReady || !m_lastCleared || m_current < 0) return;
    const Chapters::Level& entry = m_chapters.levels[static_cast<std::size_t>(m_current)];
    // LevelFinishedLayer::LevelFinishedLayer #499-502: the chapter's badges, the
    // medal just saved among its medals...
    achievementsForChapter(entry.world, true);
    // ...then #504-516 this play's: the medal it earned, its portals, the game's
    // time, in the game.
    Achievements::Play play;
    play.world = entry.world;
    play.level = entry.index;
    play.medal = MedalFor(*m_lastCleared);
    play.portals = m_lastCleared->portalsUsed;
    play.gameTimeMs = gameTimeMs;
    play.game = true;
    m_lastDoorPlay = play;
    SUPERSONIC_LOG_INFO("Magic Portals") << m_lastCleared->label << " achievements asked on tick " << m_ticks
                                         << ": medal " << play.medal << ", " << play.portals << " portals, "
                                         << play.gameTimeMs << " ms" << std::endl;
    const LevelWorld world(registry, m_data, m_level, m_achievementRules);
    for (const int id : Achievements::Dispatches(m_achievementRules, play, &world)) {
        unlockAchievement(id, true, m_achievementRules.levelDelayMs, "the door");
    }
}

void MagicPortalsLayer::achievementsForKeys() {
    if (!m_achievementRulesReady || !m_loaded) return;
    const std::vector<Keys::Key>& keys = m_level.keys.keys;
    if (m_achievementKeyOwners.size() != keys.size()) m_achievementKeyOwners.assign(keys.size(), entt::null);
    for (std::size_t i = 0; i < keys.size(); ++i) {
        const entt::entity owner = keys[i].owner;
        // ETHCallback_key #822-851: a carrier takes it and its colour is red.
        if (owner != entt::null && owner != m_achievementKeyOwners[i] &&
            keys[i].colour == m_achievementRules.key.colour) {
            unlockAchievement(m_achievementRules.key.id, true, m_achievementRules.key.delayMs, "the red key's pick");
        }
        m_achievementKeyOwners[i] = owner;
    }
}

void MagicPortalsLayer::tickNotificator(double dtMs) {
    if (!m_notificatorReady) return;
    // An achievement's card by its id - its words are the original's list's, kept
    // out of the log - and the dashboard's by its own message.
    const auto named = [](const Notificator::Card& card) {
        return card.id >= 0 ? "achievement " + std::to_string(card.id) + "'s card" : "card \"" + card.message + "\"";
    };
    for (const Notificator::Event& event : m_notificator.Update(dtMs)) {
        if (event.kind == Notificator::Event::Kind::Shown) {
            // updateStack #52-102: its sound, and notificationCallback.
            if (!event.card.sound.empty()) latch(event.card.sound);
            setMusicVolume(m_notificatorRules.musicVolumeShown);
            SUPERSONIC_LOG_INFO("Magic Portals") << named(event.card) << " shown on tick " << m_ticks << std::endl;
        } else {
            // update #52-83: notificationDismissCallback.
            setDefaultMusicVolume();
            SUPERSONIC_LOG_INFO("Magic Portals") << named(event.card) << " gone on tick " << m_ticks << std::endl;
        }
    }
}

void MagicPortalsLayer::setMusicVolume(double volume) {
    // AudioManager::setMusicVolume (194987..195179): the current track's, if there is one.
    if (m_music.current.empty()) return;
    m_music.volumes[m_music.current] = volume;
}

void MagicPortalsLayer::setDefaultMusicVolume() {
    // setDefaultMusicVolume (195179..195443): 0.6 for g_musicNames[0], 0.7 for any other.
    if (m_music.current.empty()) return;
    m_music.volumes[m_music.current] = m_music.current == m_notificatorRules.musicFirstTrack
                                           ? m_notificatorRules.musicVolumeDefaultFirstTrack
                                           : m_notificatorRules.musicVolumeDefaultOther;
}

double MagicPortalsLayer::MusicVolumeNow() const {
    if (m_music.current.empty()) return -1.0;
    const auto set = m_music.volumes.find(m_music.current);
    if (set != m_music.volumes.end()) return set->second;
    const Sounds::Track* track = m_soundRules.FindTrack(m_music.current);
    return track != nullptr ? track->volume : -1.0;
}


// ---- the entities' particles --------------------------------------------------
//
// What makes the original's scenery move: each entity's own <ParticleSystem>
// (sim/Particles.hpp). A crystal sparkles, a torch burns, a static portal turns.
// Read from the sidecar its node's eth_particles names - the block the level file
// carries inline for it (planning steps 89, 90) - and from nothing else: a
// placement whose node names none carries no system (step 91).
//
// Drawn the way everything else in this port is: one textured quad per live
// particle, added or mixed as its system's alphaMode says. Carried on the FRAME,
// in OnUpdate, because a particle is a picture: it must not reach Game::Level, the
// fixed tick or the state hash.
//
// The arithmetic is Ethanon's own, in sim/Particles (UpdateParticleSystem,
// ResetParticle, PositionParticle and DrawParticleSystem), kept in its units: a
// frame-speed unit is a sixtieth of a second, and every rate is per one of those.
// What this half decides is WHERE a system is and WHAT draws it (step 59):
//   - it is carried by its entity's POSITION and angle, not the centre of the
//     picture, and follows whatever moves the picture: a body, a patrolling zone, a
//     carried key or diamond. It goes when the picture goes.
//   - it draws in its entity's own slot, after the picture and the halo and before
//     the next entity (Particles::SlotFraction) - unless its <StartPoint> z lifts
//     it past other pictures, which 25 of them do (step 92, buildEmitters).
//   - its quad is square, turned counter-clockwise by a positive angle, and an
//     added one's alpha is 1 (Particles::DrawColour).

double MagicPortalsLayer::particleRandom(double from, double to) {
    // Randomizer::Float(min, max) takes the two either way round.
    const double lo = std::min(from, to);
    const double hi = std::max(from, to);
    if (!(hi > lo)) return lo;
    std::uniform_real_distribution<double> spread(lo, hi);
    return spread(m_particleRandom);
}

void MagicPortalsLayer::buildEmitters(entt::registry& registry) {
    unloadEmitters(registry);
    // Without the level's art there is nothing to decorate: the level is drawn
    // as boxes and says so.
    if (!m_artReady) return;

    int fromSidecars = 0;
    for (std::size_t index = 0; index < m_sprites.size(); ++index) {
        const DrawnSprite& drawn = m_sprites[index];
        // THE LEVEL'S OWN SYSTEMS, and only those (planning step 91): the
        // <Particles> block the level file carries inline for this placement,
        // which on 45 of the 840 carriers is not what its entity's .ent holds
        // (systems_9_10 9c). A node that names no sidecar states no system, so
        // it draws none: its entity's .ent is not read for it. The engine does
        // not read it for a placement either - a scene entity takes its
        // properties from its own <Entity> body (ETHScene.cpp:226 ->
        // ETHEntity.cpp:192-207 -> ETHEntityProperties.cpp:202-224, particles
        // :304-333), and from the .ent only when that body names a <FileName>,
        // which none of the 4,072 in the 133 .esc files does. On the shipped
        // levels no node without a key has an .ent that holds a system either
        // (step 91's census), so dropping it moves nothing.
        //
        // drawn.particles comes only from the lighting read (Lighting::Look), so
        // a level whose lighting is refused draws none of its placements'
        // systems either, and loadLevel says so beside "drawn unlit".
        if (drawn.particles.empty()) continue;
        // THE SPLIT: the sidecar is the converter's, read under m_paths.art like
        // the rest of what it writes, while the bitmaps it names are still the
        // original's, read from m_paths.original's particles/ below - the
        // converter copies none. A sidecar that will not load, or holds no
        // system, is said: the node's own data said what it carries.
        std::vector<Particles::System> systems;
        std::string error;
        if (!Particles::Load(drawn.particles, systems, error) || systems.empty()) {
            SUPERSONIC_LOG_WARN("Magic Portals")
                << "particles not drawn: " << drawn.sprite.node << "'s eth_particles " << drawn.particles
                << (error.empty() ? std::string(" holds no particle system") : ": " + error) << std::endl;
            continue;
        }
        ++fromSidecars;
        // Scaled as its entity is, before its pools are made (Particles::Scale), in
        // one call: by the mean of its <Scale>, which the engine makes the entity's
        // managers at (ETHSpriteEntity.cpp:316-319, ETHParticleManager.cpp:90) and
        // no carrier of the 840 states (step 93), and by what its script scales it
        // by after that - a static portal's script shrinks its rings and its iris
        // with its halo. Its light and halo are not: the converter has scaled them
        // already (00_order K7, syncLights).
        const double scale = drawn.scale * (drawn.sprite.scale.x + drawn.sprite.scale.y) / 2.0;
        if (scale != 1.0) {
            for (Particles::System& system : systems) Particles::Scale(system, scale);
        }
        // Its entity's position and angle (ETHSpriteEntity.cpp:576): the node, not
        // Sprites::CentrePx. A torch's picture hangs 16 below its node (PivotAdjust
        // 0, -16) and its flame starts 12 above the NODE, not above the picture.
        const Particles::Owner owner{drawn.ownerPx, drawn.ownerAngleDeg};
        for (std::size_t slot = 0; slot < systems.size(); ++slot) {
            const Particles::System& system = systems[slot];
            if (!Particles::Drawable(system)) {
                // No level places one (step 59's census); refused by name rather
                // than drawn as a mix it is not.
                SUPERSONIC_LOG_WARN("Magic Portals")
                    << "particle system not drawn: " << drawn.sprite.node << "'s " << drawn.particles
                    << " system " << slot << " asks for alphaMode " << system.alphaMode
                    << ", which the port has no blend for" << std::endl;
                continue;
            }
            // The bitmap's tier file where one exists (ETHParticleManager.cpp:107 loads
            // it through AddFile too).
            const Sprites::Image& image = drawnImage(m_paths.original + "/particles/" + system.bitmap);
            if (image.units.x <= 0 || image.units.y <= 0) {
                SUPERSONIC_LOG_WARN("Magic Portals") << "particle image could not be read: " << image.path << std::endl;
                continue;
            }
            // What it is drawn with: its bitmap, or the multiply an AM_MODULATE
            // one is (particleTexture) - none of the levels' systems is one today.
            const std::string texture = particleTexture(system, image.path, drawn.sprite.node);
            if (texture.empty()) continue;
            Emitter emitter;
            emitter.system = system;
            emitter.owner = owner;
            // ITS DEPTH (step 92; the visuals plan's 8.4, the half of plan_port 2d
            // step 59 deferred). Ethanon sorts an entity's particle piece at its
            // system's start point z plus the entity's z
            // (ETHEntityRenderingManager.cpp:98-102; GetZPosition is
            // startPoint.z, ETHParticleManager.cpp:424-427). That z is NOT a bare
            // literal: Scale multiplies the whole start point with the rest of the
            // system (ETHParticleSystem.cpp:27-33), and SGlobalScale::scaleEntity
            // scales a placement's systems and its position by the one getScale
            // (the listing, 6072..6337 #30-53), so on the level's axis it is the
            // file's number at the entity's own scale - which Particles::Scale has
            // applied above. So:
            //   - at 0, which is 872 of the levels' 897 systems, in its entity's own
            //     slot: after its picture and its halo, before the next entity, and
            //     each system after the one before it (Slot::Systems);
            //   - otherwise after every picture at or below its owner's eth_z plus
            //     that z, and after the player where the player stands at or below
            //     it (Slot::LiftedSystems): the five blue lamps' orbs, 24 in front
            //     of their bowls, and the ten unlit torches' sparkles, 4 in front.
            // An ET_VERTICAL owner's piece also takes the engine's own +10
            // (_ETH_PARTICLE_DEPTH_SHIFT, ETHParticleManager.h:30, .cpp:332-335),
            // a raw number, 3.56 on the level's axis. It is NOT added here: the
            // port is not told a placement's type (the converter writes none), and
            // the ten torches are the only vertical carriers (step 92's census).
            emitter.depth = drawn.lookZ + system.startPointZ;
            emitter.lifted = system.startPointZ != 0.0;
            emitter.z = emitter.lifted ? systemZ(slotAfter(emitter.depth, Slot::Picture), Slot::LiftedSystems,
                                                 static_cast<int>(slot), static_cast<int>(systems.size()))
                                       : systemZ(drawn.z, Slot::Systems, static_cast<int>(slot),
                                                 static_cast<int>(systems.size()));
            emitter.image = texture;
            emitter.cellPx = ParticleCellUnits(image, system);
            // A shock agent's and a shock diamond's tesla_shock_black_bg.png, cut as
            // the original cuts it (step 67's recorded deviation, the visuals plan's
            // 13.1). It moves no draw: the stream is the placement one as before.
            if (WholeUnitCut(image, system, emitter.cut)) {
                emitter.wholeUnitCut = true;
                emitter.cellPx = glm::dvec2(emitter.cut.frameUnits);
            }
            // Which entity and which of its systems: a light's brightness follows
            // the live share of its owner's FIRST system (syncLights).
            emitter.sprite = static_cast<int>(index);
            emitter.slot = static_cast<int>(slot);
            emitter.particles = Particles::MakePool(system, std::min(system.count, kMaxParticles), owner,
                                                    [this](double from, double to) { return particleRandom(from, to); });
            emitter.quads.assign(emitter.particles.size(), entt::null);
            m_emitters.push_back(std::move(emitter));
        }
    }
    // Said only once a level names a sidecar: a level that names none draws no
    // placement's system, and says nothing about it.
    if (fromSidecars > 0) {
        SUPERSONIC_LOG_INFO("Magic Portals")
            << fromSidecars << " placement(s) drew the particle systems their eth_particles names" << std::endl;
    }
}

bool MagicPortalsLayer::addEntityEmitters(const std::string& entity, const glm::dvec2& atPx, float z,
                                          double angleDeg) {
    std::vector<Particles::System> systems;
    std::string error;
    if (!Particles::Load(m_paths.original + "/entities/" + entity, systems, error) || systems.empty()) return false;
    const Particles::Owner owner{atPx, angleDeg};
    for (std::size_t slot = 0; slot < systems.size(); ++slot) {
        const Particles::System& system = systems[slot];
        if (!Particles::Drawable(system)) {
            SUPERSONIC_LOG_WARN("Magic Portals") << "particle system not drawn: " << entity << " system " << slot
                                                 << " asks for alphaMode " << system.alphaMode
                                                 << ", which the port has no blend for" << std::endl;
            continue;
        }
        const Sprites::Image& image = drawnImage(m_paths.original + "/particles/" + system.bitmap);
        if (image.units.x <= 0 || image.units.y <= 0) {
            SUPERSONIC_LOG_WARN("Magic Portals") << "particle image could not be read: " << image.path << std::endl;
            continue;
        }
        const std::string texture = particleTexture(system, image.path, entity);
        if (texture.empty()) continue;
        Emitter emitter;
        emitter.system = system;
        emitter.owner = owner;
        // A later system of the same entity draws over an earlier one.
        emitter.z = z + 0.001f * static_cast<float>(m_emitters.size());
        emitter.image = texture;
        emitter.cellPx = ParticleCellUnits(image, system);
        if (WholeUnitCut(image, system, emitter.cut)) {
            emitter.wholeUnitCut = true;
            emitter.cellPx = glm::dvec2(emitter.cut.frameUnits);
        }
        emitter.slot = static_cast<int>(slot);
        emitter.particles = Particles::MakePool(system, std::min(system.count, kMaxParticles), owner,
                                                [this](double from, double to) { return particleRandom(from, to); });
        emitter.quads.assign(emitter.particles.size(), entt::null);
        m_emitters.push_back(std::move(emitter));
    }
    return true;
}

std::string MagicPortalsLayer::modulateImage(const std::string& source) {
    // Only into a directory the caller named: an empty Paths::prisms would put the
    // file in the working directory, so the multiply is then left undrawn instead.
    if (m_paths.prisms.empty()) return {};
    // Named for where it came from under the original's files - entities/
    // black_halo.bmp is entities_black_halo_multiply.png - so two images of one
    // stem in two directories, a tier's and its 1x file's, never share a file.
    const std::filesystem::path from(source);
    std::error_code ec;
    std::filesystem::path relative = std::filesystem::relative(from, std::filesystem::path(m_paths.original), ec);
    if (ec || relative.empty() || relative.native().front() == '.') relative = from.filename();
    std::string name = relative.replace_extension().generic_string();
    std::replace(name.begin(), name.end(), '/', '_');
    std::filesystem::create_directories(m_paths.prisms, ec);
    const std::filesystem::path target = m_paths.prisms / (name + "_multiply.png");
    if (std::filesystem::exists(target, ec) && std::filesystem::last_write_time(target, ec) >=
                                                    std::filesystem::last_write_time(from, ec)) {
        return target.string();
    }
    int width = 0;
    int height = 0;
    int channels = 0;
    // Three channels whatever the file holds: an 8-bit grey BMP comes back with its
    // grey in all three, and a texel's own alpha is dropped, as GL_ZERO,
    // GL_SRC_COLOR never reads it.
    stbi_uc* rgb = stbi_load(source.c_str(), &width, &height, &channels, 3);
    if (rgb == nullptr) {
        SUPERSONIC_LOG_WARN("Magic Portals") << "a multiply's image could not be read: " << source << std::endl;
        return {};
    }
    std::vector<unsigned char> rgba;
    const bool grey = Particles::MultiplyTexels(
        rgb, static_cast<std::size_t>(width) * static_cast<std::size_t>(height), rgba);
    stbi_image_free(rgb);
    if (!grey) {
        SUPERSONIC_LOG_WARN("Magic Portals")
            << "not drawn as a multiply: " << source
            << " is not grey, and one alpha cannot multiply three channels by three numbers" << std::endl;
        return {};
    }
    if (stbi_write_png(target.string().c_str(), width, height, 4, rgba.data(), width * 4) == 0) {
        SUPERSONIC_LOG_WARN("Magic Portals") << "a multiply could not be written: " << target.string() << std::endl;
        return {};
    }
    return target.string();
}

std::string MagicPortalsLayer::particleTexture(const Particles::System& system, const std::string& image,
                                               const std::string& owner) {
    if (system.alphaMode != Particles::kAlphaModulate) return image;
    // AM_MODULATE, drawn as the multiply it is (Particles.hpp): exact for a white
    // colour over a grey bitmap, and refused by name otherwise rather than drawn
    // as a guess.
    if (!Particles::MultipliesExactly(system)) {
        SUPERSONIC_LOG_WARN("Magic Portals")
            << "particle system not drawn: " << owner << "'s " << system.bitmap
            << " is a multiply whose colour is not white, which a black texel cannot carry" << std::endl;
        return {};
    }
    return modulateImage(image);
}

void MagicPortalsLayer::unloadEmitters(entt::registry& registry) {
    for (Emitter& emitter : m_emitters) {
        for (entt::entity& quad : emitter.quads) {
            if (quad != entt::null && registry.valid(quad)) registry.destroy(quad);
            quad = entt::null;
        }
    }
    m_emitters.clear();
}

void MagicPortalsLayer::stepEmitter(entt::registry& registry, Emitter& emitter, double elapsedMs,
                                   const Particles::Random& random, bool pooled, double scaleAfterStep) {
    using namespace Supersonic;
    const Particles::System& system = emitter.system;
    // What becomes of the quad of a particle this frame does not draw. A level's
    // emitter destroys it, as step 59 built it and the suites count it; an
    // effect's keeps it and turns it off, because a burst of sparkles would
    // otherwise make and unmake entities every frame (system_7 7a-2).
    const auto put_away = [&registry, pooled](entt::entity& quad) {
        if (quad == entt::null || !registry.valid(quad)) {
            quad = entt::null;
            return;
        }
        if (pooled) {
            registry.get<RenderableComponent>(quad).isVisible = false;
            return;
        }
        registry.destroy(quad);
        quad = entt::null;
    };

    const int frames = std::max(1, system.Frames());
    const int pool = static_cast<int>(emitter.particles.size());
    emitter.active = 0;
    for (int i = 0; i < pool; ++i) {
        Particles::Particle& particle = emitter.particles[static_cast<std::size_t>(i)];
        entt::entity& quad = emitter.quads[static_cast<std::size_t>(i)];
        if (Particles::Step(system, particle, i, pool, emitter.owner, elapsedMs, emitter.killed, random)) {
            ++emitter.active;
        }
        // An ETHEntity::Scale made after this update and before the draw: the
        // live particle's size and velocity (PARTICLE::Scale,
        // ETHParticleManager.h:160-164), so it is drawn at the new size.
        if (scaleAfterStep != 1.0) {
            particle.size *= scaleAfterStep;
            particle.velocityPx *= scaleAfterStep;
        }
        if (!Particles::Drawn(system, particle, emitter.killed)) {
            put_away(quad);
            continue;
        }

        if (quad == entt::null) {
            quad = makeSprite(registry, emitter.tag, emitter.image, system.additive);
            if (emitter.wholeUnitCut) {
                // The original's rect, written here rather than by a flipbook.
                registry.get<MaterialComponent>(quad).uvScale = glm::vec2(emitter.cut.uvScale);
            } else if (frames > 1) {
                auto& animation = registry.emplace<SpriteAnimationComponent>(quad);
                animation.columns = static_cast<uint32_t>(system.columns);
                animation.rows = static_cast<uint32_t>(system.rows);
                animation.frameCount = 1;
                animation.playing = false; // the frame is Step's, by age or at random
            }
        }
        if (pooled) registry.get<RenderableComponent>(quad).isVisible = true;
        if (emitter.wholeUnitCut) {
            const int columns = std::max(1, system.columns);
            registry.get<MaterialComponent>(quad).uvOffset =
                glm::vec2(Tiers::UvOffset(emitter.cut, particle.frame % columns, particle.frame / columns));
        } else if (frames > 1) {
            registry.get<SpriteAnimationComponent>(quad).firstFrame = static_cast<uint32_t>(particle.frame);
        }
        const glm::dvec4 colour = Particles::DrawColour(system, particle, m_ambient);
        registry.get<MaterialComponent>(quad).albedoColor =
            glm::vec4(static_cast<float>(colour.r), static_cast<float>(colour.g), static_cast<float>(colour.b),
                      static_cast<float>(colour.a));
        placeSprite(registry, quad, particle.atPx, Particles::QuadPx(particle), emitter.z,
                    Particles::WorldRotation(particle));
    }
    // And the system, which the next update resets and renews from
    // (ETHParticleManager.cpp:444-451, ETHParticleSystem.cpp:27-40).
    if (scaleAfterStep != 1.0) Particles::Scale(emitter.system, scaleAfterStep);
}

void MagicPortalsLayer::updateEmitters(entt::registry& registry, float deltaTime) {
    if (m_emitters.empty()) return;
    const double elapsedMs = static_cast<double>(deltaTime) * 1000.0;
    const Particles::Random random = [this](double from, double to) { return particleRandom(from, to); };
    // And the effects' stream, for the one draw here that is not a placement:
    // 2-32's dragon re-pooling its own mouth (00_order K8).
    const Particles::Random effects = [this](double from, double to) { return effectRandom(from, to); };

    for (Emitter& emitter : m_emitters) {
        // Its owner, as the last tick left it. An entity the level took away - a
        // crystal taken, a static portal spent, a wall broken, a diamond spent -
        // takes its particles with it, as a deleted entity's go with it.
        if (emitter.sprite >= 0 && emitter.sprite < static_cast<int>(m_sprites.size())) {
            const DrawnSprite& owner = m_sprites[static_cast<std::size_t>(emitter.sprite)];
            if (owner.quad == entt::null) {
                for (entt::entity& quad : emitter.quads) {
                    if (quad != entt::null && registry.valid(quad)) registry.destroy(quad);
                    quad = entt::null;
                }
                emitter.active = 0;
                continue;
            }
            emitter.owner = Particles::Owner{owner.ownerPx, owner.ownerAngleDeg};
            // 2-32'S DRAGON PLAYS AND KILLS ITS OWN MOUTH (sim/Dragon.hpp Look, the
            // same two windows that drive its cell). KillParticleSystem stops the
            // renewals and lets what is alive live out; PlayParticleSystem is
            // Kill(false) followed by Play, which resets EVERY particle to the
            // entity (ETHSpriteEntity.cpp:795-811, ETHParticleManager.cpp:274-286) -
            // and the callback calls it on every tick of the open window, so the
            // burst sits at the mouth for its 400 ms rather than streaming away.
            //
            // ON THE EFFECTS' STREAM, not the placement one (00_order K8). Step 77
            // drew these resets from m_particleRandom and said so: a draw that
            // happens because the dragon opened its mouth shifted every later
            // placement draw on level31a, and on that level alone. Step 83's
            // m_effectRandom is the generator K8 asks for, and the merge of the two
            // tracks moved this consumer onto it: the placement stream is now
            // untouched by anything the dragon does, so level31a's other emitters
            // draw what every other level's do.
            //
            // WHAT IT COSTS is that the effects' stream now has two consumers whose
            // ORDER matters, and the order is OnUpdate's: updateEmitters runs before
            // updateEffects, so on level31a - and on no other level - a draw taken
            // here moves the eyes' renewals (step 84) later in the same frame. That
            // is the one difference the merge's own sweep found against either
            // track's last capture.
            if (owner.dragon && m_level.dragon.present) {
                if (m_level.dragon.systemRestarted) {
                    emitter.particles =
                        Particles::MakePool(emitter.system, static_cast<int>(emitter.particles.size()),
                                            emitter.owner, effects);
                }
                emitter.killed = m_level.dragon.systemKilled;
            }
        }
        // 4-19'S KEY, WHICH THE LANDING SHRINKS (cutscenes.json rocket_landing):
        // Scale(0.9) every frame from the door's, and ETHEntity::Scale(float) scales
        // the entity's systems with it (ETHEntity.cpp:591-599, through
        // ETHSpriteEntity.cpp:512-518), so its sparkles shrink to nothing with the
        // picture rather than glowing on in the doorway (step 123's deviation 1).
        // After this frame's step and before the draw: the key is a dynamic entity
        // with a callback, updated with its particles in UpdateAlwaysActiveEntities,
        // and rocket_landing a static one whose callback runs after, in
        // UpdateCurrentFrameEntities (ETHScene.cpp:467-488). As many calls as the
        // landing has made since this emitter last took one, on the same clock
        // as the picture's keyScale.
        double scaleAfterStep = 1.0;
        if (m_landing.running && emitter.sprite >= 0 && emitter.sprite == m_landingKeySprite) {
            for (; emitter.scalingsTaken < m_landing.keyScalings; ++emitter.scalingsTaken) {
                scaleAfterStep *= m_cutsceneRules.landing.keyScalePerFrame;
            }
        }
        stepEmitter(registry, emitter, elapsedMs, random, false, scaleAfterStep);
    }
}

std::vector<MagicPortalsLayer::PictureReport> MagicPortalsLayer::PictureReports() const {
    std::vector<PictureReport> reports;
    reports.reserve(m_sprites.size());
    for (const DrawnSprite& drawn : m_sprites) {
        PictureReport report;
        report.node = drawn.sprite.node;
        report.zIndex = drawn.sprite.zIndex;
        report.lookZ = drawn.lookZ;
        report.z = drawn.z;
        report.quad = drawn.quad;
        reports.push_back(std::move(report));
    }
    return reports;
}

std::vector<MagicPortalsLayer::EmitterReport> MagicPortalsLayer::EmitterReports() const {
    std::vector<EmitterReport> reports;
    reports.reserve(m_emitters.size());
    for (const Emitter& emitter : m_emitters) {
        EmitterReport report;
        report.bitmap = emitter.system.bitmap;
        report.image = emitter.image;
        report.systemSize = emitter.system.size;
        report.cellPx = emitter.cellPx;
        report.slot = emitter.slot;
        report.additive = emitter.system.additive;
        report.owner = emitter.owner;
        report.z = emitter.z;
        report.depth = emitter.depth;
        report.lifted = emitter.lifted;
        report.count = static_cast<int>(emitter.particles.size());
        report.active = emitter.active;
        report.drawn = static_cast<int>(
            std::count_if(emitter.quads.begin(), emitter.quads.end(), [](entt::entity q) { return q != entt::null; }));
        for (std::size_t p = 0; p < emitter.particles.size() && p < emitter.quads.size(); ++p) {
            if (emitter.quads[p] != entt::null) {
                report.largestDrawnSize = std::max(report.largestDrawnSize, emitter.particles[p].size);
            }
        }
        if (emitter.sprite >= 0 && emitter.sprite < static_cast<int>(m_sprites.size())) {
            const DrawnSprite& owner = m_sprites[static_cast<std::size_t>(emitter.sprite)];
            report.node = owner.sprite.node;
            report.ownerGone = owner.quad == entt::null;
            report.ownerZ = owner.z;
            report.systems = static_cast<int>(std::count_if(m_emitters.begin(), m_emitters.end(), [&](const Emitter& e) {
                return e.sprite == emitter.sprite;
            }));
        } else {
            report.systems = 1;
        }
        double youngest = 0.0;
        for (const Particles::Particle& particle : emitter.particles) {
            if (!particle.released) continue;
            if (!report.anyReleased || particle.elapsedMs < youngest) {
                youngest = particle.elapsedMs;
                report.newestBornPx = particle.bornPx;
                report.newestAgeMs = particle.elapsedMs;
                report.anyReleased = true;
            }
        }
        reports.push_back(std::move(report));
    }
    return reports;
}

// ---- THE EFFECTS THE SCRIPT ADDS ---------------------------------------------
//
// The original's script spawns entities by name: a burst where something
// teleports, sparkles where a crystal is taken, a fireball's explosion, the
// player's red eyes. Every one of them is an AddEntity call, and sim/Effects
// holds the call sites' arithmetic (effects.json, step 82). This is what becomes
// of one once it is added - the visuals plan's 6.3, system_7 7a-2.
//
// PER FRAME, like the particles and for the same reason: an effect is a picture.
// The TICK only queues (latchSimEffects); nothing below reaches Game::Level, the
// simulation's clock or the state hash. An effect is
//   - a picture, when its .ent carries a <Sprite>, standing on its pivot;
//   - a light, and a halo when that light names a <HaloBitmap>, both drawn at the
//     owner's scale (Lighting::OwnerScaled, 00_order K7);
//   - one emitter per <ParticleSystem>, made at the mean of that scale's axes
//     (Particles::Scaled), drawing on m_effectRandom;
// and it is freed when the engine would free it: a temporary one once its pools
// are over (Particles::Finished), one bound to something the level took away when
// that goes. The player is the exception: ETHEntity::Hide is not a delete, so an
// effect the player carries is hidden with it and comes back with it.
//
// ITS DEPTH is the table's (00_order K13): the picture at Slot::Effect, the halo
// a quarter in front of it, and its systems over the quarter after that - and a
// system's own <StartPoint> z is added to the entity's before the slot is
// counted, at the entity's scale as its sizes are (Particles::Scaled). A z the
// script writes as a bare literal is divided by the art's scale factor first
// (effectAdd, below); the start point is not such a literal (step 92), because
// AddEntity scales the entity's systems by the Scale() it is given, which every
// call site here gives as g_scale.getScale() or a multiple of it. So
// appear_effect's flash draws at 0.711 + 10 = 10.71 and not at the 0.711 its
// own picture takes: in front of
// every node under 10.71 rather than only of those under 0.711.
// (system_7 7a-2 says "in front of the player at 0"; since R10 the player is at
// 1.42 in the level's units, so it is the ten that carries the flash past the
// player and past the level's nearer decor.)
//
// ITS QUADS ARE POOLED. A particle this frame does not draw keeps its quad and is
// turned off, where a level emitter's is destroyed: a burst of sparkles would
// otherwise make and unmake entities every frame, and every one of them is walked
// by the state hash.
//
// NOT BUILT HERE. The placed portal's kill - its halo fading by 0.9 a frame and
// portal_death 400 ms later - belongs to the step that spawns portals as effects
// (the plan's 7.4). This step builds the two free rules the engine itself has.

double MagicPortalsLayer::effectRandom(double from, double to) {
    // Randomizer::Float(min, max) takes the two either way round, as particleRandom does.
    const double lo = std::min(from, to);
    const double hi = std::max(from, to);
    if (!(hi > lo)) return lo;
    std::uniform_real_distribution<double> spread(lo, hi);
    return spread(m_effectRandom);
}

bool MagicPortalsLayer::effectOwnerPoint(const entt::registry& registry, const EffectOwner& owner, glm::dvec2& atPx,
                                         bool& hidden) const {
    using namespace Supersonic;
    hidden = false;
    switch (owner.kind) {
    case EffectOwner::Kind::None:
        return true; // where it was added, and it stays there
    case EffectOwner::Kind::Player:
        if (m_level.player == entt::null || !registry.valid(m_level.player)) return false;
        atPx = Units::ToPixels(registry.get<TransformComponent>(m_level.player).position) + owner.offsetPx;
        // Hide(), not a delete: the character in the doorway is hidden while the
        // level runs on behind it (OnFixedUpdate), so what it carries waits.
        hidden = m_playerQuad == entt::null || !registry.valid(m_playerQuad);
        return true;
    case EffectOwner::Kind::Sprite: {
        if (owner.index < 0 || owner.index >= static_cast<int>(m_sprites.size())) return false;
        const DrawnSprite& drawn = m_sprites[static_cast<std::size_t>(owner.index)];
        if (drawn.quad == entt::null) return false; // its picture was taken away, and it with it
        atPx = drawn.ownerPx + owner.offsetPx;
        return true;
    }
    case EffectOwner::Kind::Fireball:
        // BY NAME and not by place in the list, for the same reason a portal is
        // by id: one going takes every one after it along one place, and a
        // fireball goes the moment it touches anything. Every producer names its
        // own - "<carranca>#<n>", the dragons' and "fire_diamond#<n>" - and a
        // level's counters never repeat one.
        for (const Turrets::Fireball& ball : m_level.turrets.fireballs) {
            if (ball.name != owner.name) continue;
            atPx = ball.atPx + owner.offsetPx;
            return true;
        }
        return false;
    case EffectOwner::Kind::PlacedPortal:
        // By the portal's id and not its place in the list: a pair going takes
        // the ones after it along one place. And against the LAYER's list rather
        // than the simulation's, because the entity outlives the manager's array:
        // killPortal takes the portal out of play, and its own callback then
        // spends 400 ms fading it before it deletes it (syncPlacedPortals).
        for (const DrawnPortal& portal : m_portalDraws) {
            if (portal.id != owner.index) continue;
            atPx = portal.atPx + owner.offsetPx;
            return true;
        }
        return false;
    case EffectOwner::Kind::Minion: {
        // WHERE ITS PICTURE STANDS THIS FRAME. The original has one point: the
        // callback moves the entity, and the frame releases its particles from, and
        // draws its sprite at, that point. The picture is drawn at the tick's own place
        // (drawPicturesAtTick), so its centre, back out through the pivot syncMinions
        // placed it by, is that entity point as it is drawn. Where the display outruns
        // the tick and the engine draws a body between its last two ticks
        // (pictures.json), the body's own point runs up to a tick ahead of the knight on
        // the screen - 0.65 units at the patrol's 39 a second - and a system released from
        // it would lead its knight: the picture's centre is what follows it.
        if (owner.index < 0 || owner.index >= static_cast<int>(m_minions.size())) return false;
        const MinionQuad& drawn = m_minions[static_cast<std::size_t>(owner.index)];
        // Taken away (a killer floor, a strike): DeleteEntity takes its systems too.
        if (drawn.quad == entt::null || !registry.valid(drawn.quad)) return false;
        const Art::Minion& art = m_artRules.minion;
        atPx = Units::ToPixels(registry.get<TransformComponent>(drawn.quad).position) +
               glm::dvec2(art.pivotXPx, art.pivotYPx) + owner.offsetPx;
        return true;
    }
    case EffectOwner::Kind::DarkDragon:
        // The summon's dark_dragon.ent, while it is there: its DYING arm's last
        // waypoint deletes it, and its picture and its mouth go with it.
        if (!m_level.darkDragon.Alive()) return false;
        atPx = m_level.darkDragon.atPx + owner.offsetPx;
        return true;
    case EffectOwner::Kind::Ghost:
        // SeekEntity('ghost.ent'): while the knight's death has added it and it has
        // not deleted itself.
        if (!m_level.ghost.Alive()) return false;
        atPx = m_level.ghost.atPx + owner.offsetPx;
        return true;
    case EffectOwner::Kind::GhostShot:
        // By the id the sim gave it, which it never gives again: a shot going takes
        // the ones after it along one place in the list.
        for (const Ghost::Shot& shot : m_level.ghost.shots) {
            if (shot.id != owner.index) continue;
            atPx = shot.atPx + owner.offsetPx;
            return true;
        }
        return false;
    case EffectOwner::Kind::MinionShot:
        // By the id the sim gave it, as a ghost's is.
        for (const Minions::EnemyShot& shot : m_level.minions.shots) {
            if (shot.id != owner.index) continue;
            atPx = shot.atPx + owner.offsetPx;
            return true;
        }
        return false;
    case EffectOwner::Kind::Key: {
        // Where the carry left it and its bob put it, as a level key's picture is
        // (syncSprites): it goes, sparkles and all, when the keyhole it opened goes.
        if (owner.index < 0 || owner.index >= static_cast<int>(m_level.keys.keys.size())) return false;
        const Keys::Key& key = m_level.keys.keys[static_cast<std::size_t>(owner.index)];
        if (key.spent && key.opened >= 0 && key.opened < static_cast<int>(m_level.keys.keyholes.size()) &&
            m_level.keys.keyholes[static_cast<std::size_t>(key.opened)].gone) {
            return false;
        }
        atPx = key.atPx + owner.offsetPx;
        for (const DroppedKey& dropped : m_droppedKeys) {
            if (dropped.key == owner.index && key.owner == entt::null && !key.spent) atPx += Motion::OffsetPx(dropped.bob);
        }
        return true;
    }
    case EffectOwner::Kind::FireSignal:
        // ETHCallback_fire_signal's SetPositionXY every frame, onto the circle
        // Torch keeps; DeleteEntity when it is shot, when Torch clears it.
        if (!m_level.torch.signal.present) return false;
        atPx = m_level.torch.signal.atPx + owner.offsetPx;
        return true;
    case EffectOwner::Kind::LitTorch: {
        // deleteLightFromProjectile, when the signal is shot: Torch puts it out.
        const std::vector<Torch::Light>& lights = m_level.torch.lights;
        if (owner.index < 0 || owner.index >= static_cast<int>(lights.size())) return false;
        const Torch::Light& light = lights[static_cast<std::size_t>(owner.index)];
        if (!light.lit) return false;
        atPx = light.atPx + owner.offsetPx;
        return true;
    }
    case EffectOwner::Kind::UnlitTorch: {
        // Its own `delete` branch, on the tick after it is lit again (TorchWatch).
        const std::vector<Torch::Light>& lights = m_level.torch.lights;
        if (owner.index < 0 || owner.index >= static_cast<int>(lights.size()) ||
            owner.index >= static_cast<int>(m_torchWatch.size()) ||
            m_torchWatch[static_cast<std::size_t>(owner.index)].unlitGone) {
            return false;
        }
        atPx = lights[static_cast<std::size_t>(owner.index)].atPx + owner.offsetPx;
        return true;
    }
    case EffectOwner::Kind::LightWall:
        // ETHCallback_light_wall's DeleteEntity at the end of its 1500 ms.
        if (!m_level.torch.wall.present || m_level.torch.wall.gone) return false;
        return true; // it does not move
    case EffectOwner::Kind::Knight:
        // 2-32's knight where its PICTURE stands this frame, back out through its
        // pivot, as a minion's (Kind::Minion). Its DeleteEntity in the door takes
        // its aura with it.
        if (m_knightQuad == entt::null || !registry.valid(m_knightQuad)) return false;
        atPx = Units::ToPixels(registry.get<TransformComponent>(m_knightQuad).position) +
               m_cutsceneRules.knight.pivotPx + owner.offsetPx;
        return true;
    case EffectOwner::Kind::Character:
        // ETHCallback_fire32's SeekEntity(targetID) finds the character hidden or
        // not, and SetPositionXY puts the flames on its point every frame: the body,
        // which stays in the world when the port hides the dead character's picture.
        if (m_level.player == entt::null || !registry.valid(m_level.player)) return false;
        atPx = Units::ToPixels(registry.get<TransformComponent>(m_level.player).position) + owner.offsetPx;
        return true;
    }
    return false;
}

namespace {

// ETHSpriteEntity::ComputeInScreenSpriteCenter (ETHSpriteEntity.cpp:690-709): the
// pivot, at the entity's scale, turned by -DegreeToRadian(angle) in the screen's
// y-down axes and taken off the entity's point - which for an angle of 0 is the
// point less the pivot, the rule every unturned picture is drawn by. Art::Shot::
// CentrePx, for a picture the effects runtime draws.
glm::dvec2 TurnedPictureCentre(const glm::dvec2& atPx, const glm::dvec2& scaledPivot, double angleDeg) {
    float s = 0.0f;
    float c = 1.0f;
    Supersonic::DetMath::sincos(-static_cast<float>(angleDeg) * (3.141592654f / 180.0f), s, c);
    const double sd = static_cast<double>(s);
    const double cd = static_cast<double>(c);
    return atPx - glm::dvec2(cd * scaledPivot.x - sd * scaledPivot.y, sd * scaledPivot.x + cd * scaledPivot.y);
}

} // namespace

int MagicPortalsLayer::SpawnEffect(entt::registry& registry, const EffectSpawn& spawn) {
    using namespace Supersonic;
    // With no art there is nothing to draw it with: such a level is drawn as
    // boxes, and an effect has no box.
    if (!m_artReady) return -1;
    Art::Entity art;
    std::string error;
    if (!Art::LoadEnt(m_paths.original + "/entities/" + spawn.entity, art, error)) {
        SUPERSONIC_LOG_WARN("Magic Portals") << "effect not added: " << error << std::endl;
        return -1;
    }

    Effect effect;
    effect.entity = spawn.entity;
    effect.art = art;
    effect.atPx = spawn.atPx;
    effect.angleDeg = spawn.angleDeg;
    effect.scriptScale = spawn.scale;
    effect.scale = art.ScaleAt(spawn.scale);
    effect.zUnits = spawn.zUnits;
    effect.follow = spawn.follow;
    effect.colour = spawn.colour;
    effect.emissive = art.emissive;
    effect.followUp = spawn.followUp;
    effect.followUpTarget = spawn.followUpTarget;
    effect.scalePerFrame = spawn.scalePerFrame;
    effect.alpha = spawn.alpha;
    effect.alphaPerSecond = spawn.alphaPerSecond;
    effect.bounces = spawn.bounces;
    effect.bounce = spawn.bounce;
    effect.temporary = art.Temporary();
    // ShouldEntityBeAlwaysActive (ETHActiveEntityHandler.cpp:70-74): temporary, or
    // dynamic with a callback or a simulated body. The first arm is read off the
    // .ent; the second is the caller's to say, since the .ent reader reads neither
    // a shape nor a callback: red_eyes has neither (no ETHCallback_ of its own in
    // the listing, no shape), and a minion has both (ETHCallback_ghost_minion and
    // shape 4, planning step 97). It decides only whether a HIDDEN effect is
    // stepped: the 11.2 entities that DO name callbacks - fire_signal, light_off,
    // light_wall, and light_from_projectile's - are never hidden (none has the
    // player as its owner), so their callers leave it false.
    effect.alwaysActive = effect.temporary || spawn.alwaysActive;
    effect.z = slotAfter(std::round(spawn.zUnits), spawn.pictureSlot);
    effect.haloZ = slotAfter(std::round(spawn.zUnits), Slot::EffectHalo);
    // What it is to follow, before it is counted as added: an effect added to
    // something that is already gone is no effect, and it takes no number.
    if (!effectOwnerPoint(registry, effect.follow, effect.atPx, effect.hidden)) return -1;
    effect.id = ++m_effectsSpawned;

    // ITS PICTURE, on its pivot: a torch's hangs 16 below its node from a
    // <PivotAdjust> of (0, -16), so a picture's centre is its entity's point less
    // the pivot, at the entity's scale - the rule the player's picture is drawn
    // by. Most effects carry none at all (fireball.ent, red_eyes.ent). A minion's
    // is drawn by syncMinions, so it has none here (EffectSpawn::picture).
    if (!art.sprite.empty() && spawn.picture) {
        const Sprites::Image& sheet = drawnImage(originalImage(art.sprite));
        std::string why;
        if (sheet.units.x <= 0 || sheet.units.y <= 0 ||
            !Tiers::FrameCut(sheet.texels, sheet.density, art.columns, art.rows, effect.cut, why)) {
            SUPERSONIC_LOG_WARN("Magic Portals") << "effect picture could not be read or cut: " << sheet.path << " "
                                                 << why << std::endl;
        } else {
            effect.cellPx = glm::dvec2(effect.cut.frameUnits) * effect.scale;
            effect.quad = makeSprite(registry, "Magic Portals Effect", sheet.path, art.additive);
            // A sheet of more than one cell is sampled a cell at a time, cut where
            // SetupSpriteRects cuts it (00_order E1): its cell is its script's, as
            // the shot's is, and the uv is written for it in updateEffects.
        }
    }

    // ITS LIGHT, and its halo when the light names a <HaloBitmap>. Under the
    // engine's own cap: one frame carries 64 lights, and a barrel chain adds one
    // per blast.
    if (art.light) {
        const std::uint32_t lit = static_cast<std::uint32_t>(registry.view<Light2DComponent>().size());
        if (lit >= kMaxLights2D) {
            SUPERSONIC_LOG_WARN("Magic Portals")
                << "effect added with no light: " << spawn.entity << " would be number " << (lit + 1) << ", past the "
                << kMaxLights2D << " one frame carries" << std::endl;
        } else {
            effect.light = registry.create();
            registry.emplace<TagComponent>(effect.light, "Magic Portals Effect Light");
            registry.emplace<TransformComponent>(effect.light);
            registry.emplace<Light2DComponent>(effect.light);
            if (!art.light->halo.empty()) {
                const Sprites::Image& halo = drawnImage(originalImage(art.light->halo));
                if (halo.units != glm::ivec2(0)) {
                    effect.halo = makeSprite(registry, "Magic Portals Effect Halo", halo.path, true);
                }
            }
        }
    }

    // ITS PARTICLE SYSTEMS, each made at the mean of its scale's two axes, on the
    // effects' own stream.
    const Particles::Owner owner{effect.atPx, effect.angleDeg};
    const int systems = static_cast<int>(art.systems.size());
    for (int slot = 0; slot < systems; ++slot) {
        // The entity's own scale, and on the FIRST system the caller's extra one
        // where it gave one: ETHSpriteEntity::SetScale scales every system by the
        // mean of the two axes (:586), and addFireball then calls
        // ScaleParticleSystem(0, fireballScale) on top of that (#77-83), which is
        // the same multiply again on system 0 alone.
        const double made =
            art.ParticleScale(spawn.scale) * (slot == 0 ? spawn.firstSystemScale : 1.0);
        const Particles::System system = Particles::Scaled(art.systems[static_cast<std::size_t>(slot)], made);
        if (!Particles::Drawable(system)) {
            SUPERSONIC_LOG_WARN("Magic Portals") << "particle system not drawn: " << spawn.entity << " system " << slot
                                                 << " asks for alphaMode " << system.alphaMode
                                                 << ", which the port has no blend for" << std::endl;
            continue;
        }
        const Sprites::Image& image = drawnImage(m_paths.original + "/particles/" + system.bitmap);
        if (image.units.x <= 0 || image.units.y <= 0) {
            SUPERSONIC_LOG_WARN("Magic Portals") << "particle image could not be read: " << image.path << std::endl;
            continue;
        }
        // blood.ent's black burst is the first AM_MODULATE system anything adds:
        // drawn as its multiply (particleTexture), with its colour's alpha set
        // aside (Particles::DrawColour).
        const std::string texture = particleTexture(system, image.path, spawn.entity);
        if (texture.empty()) continue;
        Emitter emitter;
        emitter.system = system;
        emitter.owner = owner;
        // The system's own <StartPoint> z on top of the entity's, before the slot
        // is counted, at the scale `made` gave it with its sizes: an .ent is a
        // spawned effect's only source for it, and a level placement's is its
        // sidecar (buildEmitters, which lifts it by the same rule since step 92).
        // So the slot is taken per system and not once for the effect.
        //
        // systemZ's base is THE SLOT'S OWN ZERO, not the effect's picture: the
        // table's fraction is measured from there, and Slot::Picture is that zero
        // (fraction 0), which is why the level's caller can pass its picture's z.
        // Passing Slot::Effect here counted the -0.5 twice and put an effect's
        // systems in (-0.75, -0.5) of the slot, BEHIND the picture and halo they
        // belong in front of.
        emitter.z = systemZ(slotAfter(std::round(spawn.zUnits + system.startPointZ), Slot::Picture),
                            Slot::EffectSystems, slot, systems);
        emitter.image = texture;
        emitter.cellPx = ParticleCellUnits(image, system);
        // SetupSpriteRects strides a cut sheet in whole units: shock_strike.png, 512
        // over 5, takes the original's own rect (WholeUnitCut), and reports its frame.
        if (WholeUnitCut(image, system, emitter.cut)) {
            emitter.wholeUnitCut = true;
            emitter.cellPx = glm::dvec2(emitter.cut.frameUnits);
        }
        emitter.sprite = -1; // no level sprite carries it: this effect does
        emitter.slot = slot;
        emitter.tag = "Magic Portals Effect Particle";
        emitter.particles = Particles::MakePool(system, std::min(system.count, kMaxParticles), owner,
                                                [this](double from, double to) { return effectRandom(from, to); });
        emitter.quads.assign(emitter.particles.size(), entt::null);
        effect.emitters.push_back(std::move(emitter));
    }

    const int id = effect.id;
    m_effects.push_back(std::move(effect));
    return id;
}

void MagicPortalsLayer::freeEffect(entt::registry& registry, Effect& effect) {
    const auto destroy = [&registry](entt::entity& e) {
        if (e != entt::null && registry.valid(e)) registry.destroy(e);
        e = entt::null;
    };
    for (Emitter& emitter : effect.emitters) {
        for (entt::entity& quad : emitter.quads) destroy(quad);
    }
    effect.emitters.clear();
    destroy(effect.quad);
    destroy(effect.light);
    destroy(effect.halo);
    ++m_effectsFreed;
}

void MagicPortalsLayer::unloadEffects(entt::registry& registry) {
    // What the level added goes with the level, as its emitters do: nothing it
    // added may outlive it, and a retry would otherwise pile a second set on the
    // first.
    for (Effect& effect : m_effects) freeEffect(registry, effect);
    m_effects.clear();
    m_effectQueue.clear();
    m_eyes = -1;
    m_fireballEffects.clear();
    m_ghostShotEffects.clear();
    m_ghostShotCells.clear();
    m_minionShotEffects.clear();
    m_minionShotCells.clear();
    m_droppedKeys.clear();
    m_ghostAppearQueued = false;
    m_darkDragonQueued = false;
    m_crystalEffects.clear();
    m_keyEffects.clear();
    m_keyholeEffects.clear();
    m_diamondEffects.clear();
    m_fieldEffects.clear();
    m_wallEffects.clear();
    m_ringKillSeen = false;
    m_playerDeadSeen = false;
    m_signalsPutOut = 0;
    m_signalSeen = false;
    m_signalOriginPx = glm::dvec2(0.0);
    m_torchWatch.clear();
    m_wallsGoneSeen = 0;
    m_lightWallRebuilt = false;
    m_trailMs = 0.0;
    m_wellSpins.clear();
    m_bossPictures = BossPictures{};
}

void MagicPortalsLayer::updateEffects(entt::registry& registry, float deltaTime) {
    using namespace Supersonic;
    // What the tick queued, added now: a frame adds and draws, a tick only says
    // that something happened.
    if (!m_effectQueue.empty()) {
        std::vector<EffectSpawn> queued;
        queued.swap(m_effectQueue);
        for (const EffectSpawn& spawn : queued) SpawnEffect(registry, spawn);
    }
    if (m_effects.empty()) return;

    // The frame's wall milliseconds step the entities' PARTICLE systems and their lifetimes -
    // the C++ side, which ETHScene::Update hands the float (ETHScene.cpp:459-473). Their
    // scripts read the whole-millisecond floor (planning step 147): a frame that is a
    // tick gives exactly its 16 ms, any other is scaled to match.
    const double elapsedMs = static_cast<double>(deltaTime) * 1000.0;
    const double scriptMs = ScriptClock::OfFrameMs(m_clockRules, elapsedMs, ScriptClock::WallMs(m_step));
    const Particles::Random random = [this](double from, double to) { return effectRandom(from, to); };
    for (std::size_t i = 0; i < m_effects.size();) {
        Effect& effect = m_effects[i];
        glm::dvec2 atPx = effect.atPx;
        bool hidden = false;
        if (!effectOwnerPoint(registry, effect.follow, atPx, hidden)) {
            freeEffect(registry, effect);
            m_effects.erase(m_effects.begin() + static_cast<std::ptrdiff_t>(i));
            continue;
        }
        effect.atPx = atPx;
        effect.hidden = hidden;
        // CHAPTER 4'S BOSS (planning step 99): what its callback wrote this tick -
        // SetFrame, SetAngle, SetEmissiveColor. Its mouth's Kill/Play waits until
        // its particles have been stepped, below.
        const bool darkDragon = effect.follow.kind == EffectOwner::Kind::DarkDragon;
        if (darkDragon) {
            const DarkDragon::Look& look = m_level.darkDragon.look;
            effect.cell = look.frame;
            effect.angleDeg = look.angleDeg;
            effect.emissive = look.emissive;
        }
        // A TORCH A PUT-OUT ADDED BACK (art.json unlit_torch, planning step 146):
        // its callback's AddToAngle(unitsPerSecond(190)) since AddEntity's 0f, which
        // its systems are turned by and its vertical picture is not.
        if (effect.follow.kind == EffectOwner::Kind::UnlitTorch && effect.follow.index >= 0 &&
            effect.follow.index < static_cast<int>(m_torchWatch.size())) {
            effect.angleDeg = m_torchWatch[static_cast<std::size_t>(effect.follow.index)].spin.turnedDeg;
        }

        // ITS OWN CALLBACK, before the frame draws it (00_order 8.0 rule 6): the
        // light wall's SetAlpha(min(GetAlpha() + unitsPerSecond(1), 1)), and the lit
        // torch's bounce. Both clocks are the frame's; the first frame an entity is
        // drawn on is one frame into them, as its callback has run once by then.
        if (effect.alphaPerSecond > 0.0) {
            effect.alpha = std::min(1.0, effect.alpha + effect.alphaPerSecond * scriptMs / 1000.0);
        }
        if (effect.bounces) {
            effect.bounceMs += scriptMs;
            effect.bounceScale = effect.bounce.ScaleAt(effect.bounceMs);
        }
        const glm::dvec2 drawnScale = effect.scale * effect.bounceScale;

        if (effect.quad != entt::null) {
            // A shot of the chapter-3 ghost is on the cell _projectileFrameTimer
            // gave it on the last tick (stepLevel).
            if (effect.follow.kind == EffectOwner::Kind::GhostShot) {
                if (const auto cell = m_ghostShotCells.find(effect.follow.index); cell != m_ghostShotCells.end()) {
                    effect.cell = cell->second;
                }
            }
            if (effect.follow.kind == EffectOwner::Kind::MinionShot) {
                if (const auto cell = m_minionShotCells.find(effect.follow.index); cell != m_minionShotCells.end()) {
                    effect.cell = cell->second;
                }
            }
            if (effect.art.Frames() > 1) {
                const int columns = std::max(1, effect.art.columns);
                const int cell = std::clamp(effect.cell, 0, effect.art.Frames() - 1);
                auto& material = registry.get<MaterialComponent>(effect.quad);
                material.uvScale = glm::vec2(effect.cut.uvScale);
                material.uvOffset = glm::vec2(Tiers::UvOffset(effect.cut, cell % columns, cell / columns));
            }
            // Ethanon's angle turns counter-clockwise on the screen, which is the
            // engine's rotation about +z, exactly as the shot's is. ON ITS ORIGIN,
            // which is its type's (ETHEntity.cpp:25-43, :268-296): the centre for
            // ET_HORIZONTAL, so the picture's centre is the point less the pivot, and
            // the pivot turns with the picture (TurnedPictureCentre); the centre of its
            // BOTTOM edge for ET_VERTICAL - light_off.ent - so it stands half its height
            // higher, and a vertical entity's picture is never turned
            // (ETHRenderEntity.cpp:105).
            const bool vertical = effect.art.type == Art::EntityType::Vertical;
            const glm::dvec2 cellPx = effect.cellPx * effect.bounceScale;
            const double drawnDeg = vertical ? 0.0 : effect.angleDeg;
            glm::dvec2 centrePx = TurnedPictureCentre(effect.atPx, effect.art.pivotPx * drawnScale, drawnDeg);
            if (vertical) centrePx.y -= cellPx.y * 0.5;
            placeSprite(registry, effect.quad, centrePx, cellPx, effect.z,
                        static_cast<float>(drawnDeg * 3.14159265358979323846 / 180.0));
            registry.get<RenderableComponent>(effect.quad).isVisible = !hidden;
            // AT THE COLOUR SetColor LEFT ON IT, here and not in syncLighting: an
            // effect is added and faded on the FRAME, and a tint that waited for
            // the next tick would draw a dying portal at its old brightness for a
            // frame and a new one white. Its particles take none of this
            // (ETHParticleManager.cpp:381-389 reads the ambient alone), nor of the
            // alpha SetAlpha leaves on it. Its emissive is SetEmissiveColor's as it
            // stands: the .ent's own, or what its script last wrote (the dark dragon).
            tint(registry, effect.quad, glm::vec4(glm::vec3(effect.colour), static_cast<float>(effect.alpha)),
                 effect.emissive, {}, effectReceiver(effect));
        }
        if (effect.light != entt::null || effect.halo != entt::null) {
            // At the live share of its FIRST system, as a level light follows its
            // owner's first (syncLights).
            const double ratio = effect.emitters.empty() ? Lighting::ParticleRatio(0, 0)
                                                         : Lighting::ParticleRatio(effect.emitters.front().active,
                                                                                   effect.emitters.front().system.count);
            // The halo takes the bounce (DrawHalo sizes it by the entity's scale);
            // the light does not. A STATIC light is baked where the original's
            // lights it - GenerateLightmaps, on the frame after the torch is lit -
            // and the port lights with it every frame, so a range swung with the
            // bounce would pulse walls the original's lightmaps hold still (L1:
            // 4-29's floor std 0.1 over 3 s of a bouncing torch).
            placeLight(registry, effect.light, entt::null, *effect.art.light, effect.atPx, effect.zUnits,
                       effect.art.isStatic, ratio, effect.haloZ, !hidden, effect.scale);
            placeLight(registry, entt::null, effect.halo, *effect.art.light, effect.atPx, effect.zUnits,
                       effect.art.isStatic, ratio, effect.haloZ, !hidden, drawnScale);
        }

        // A HIDDEN entity is not drawn (ETHScene.cpp:568, ETHRenderEntity.cpp:396),
        // and only an ALWAYS-ACTIVE one is still stepped while it is: the handler
        // walks its own list whatever each entry's state (:119-134), but anything
        // else reaches the frame list from the bucket walk, which skips a hidden
        // entity (ETHScene.cpp:568-572). So the player's eyes in a doorway freeze
        // where they are and release nothing more, which is what 00_order's 7.1
        // asks of them; a temporary effect the player carried would go on burning
        // down unseen.
        const bool steps = !hidden || effect.alwaysActive;
        bool over = effect.temporary;
        for (Emitter& emitter : effect.emitters) {
            if (steps) {
                emitter.owner = Particles::Owner{effect.atPx, effect.angleDeg};
                stepEmitter(registry, emitter, elapsedMs, random, true);
            }
            if (hidden) {
                for (entt::entity& quad : emitter.quads) {
                    if (quad != entt::null && registry.valid(quad)) {
                        registry.get<RenderableComponent>(quad).isVisible = false;
                    }
                }
            }
            if (!Particles::Finished(emitter.system, emitter.particles)) over = false;
        }
        // THE DARK DRAGON'S MOUTH, as its callback leaves it AFTER the step: the
        // engine updates an entity's particles and then runs its callback
        // (ETHActiveEntityHandler.cpp:134-138, ETHSpriteEntity.cpp:455-459), and
        // draws after both (ETHScene.cpp:459-497, then RenderScene). So this
        // frame's step ran on the kill the LAST update left, and now the
        // callback's KillParticleSystem(0) or PlayParticleSystem(0) lands - Play
        // being Kill(false) and a pool reset to the entity with nothing released
        // (ETHSpriteEntity.cpp:795-811, ETHParticleManager.cpp:274-286), on the
        // effects' stream (00_order K8). Then the draw's own test on every
        // particle (:371-378): on an update that restarts it NOTHING of the fire
        // is drawn, which is F16's mouth while Play is called every update, and the
        // first update that does not restart it releases particle 0 at the mouth.
        if (darkDragon && steps && !effect.emitters.empty()) {
            const DarkDragon::Look& look = m_level.darkDragon.look;
            Emitter& mouth = effect.emitters.front();
            if (look.systemRestarted) {
                mouth.particles = Particles::MakePool(mouth.system, static_cast<int>(mouth.particles.size()),
                                                      mouth.owner, random);
            }
            mouth.killed = look.systemKilled;
            for (std::size_t p = 0; p < mouth.particles.size() && p < mouth.quads.size(); ++p) {
                const entt::entity quad = mouth.quads[p];
                if (quad == entt::null || !registry.valid(quad)) continue;
                if (!Particles::Drawn(mouth.system, mouth.particles[p], mouth.killed)) {
                    registry.get<RenderableComponent>(quad).isVisible = false;
                }
            }
        }
        // AND ITS OWN CALLBACK, after its particles were stepped at the point the
        // last frame left it (ETHActiveEntityHandler.cpp:157-162): followUp moves
        // it toward what it follows, and Scale grows its systems and every live
        // particle (ghost_appear_effect.ent, planning step 98).
        if (effect.followUp) {
            glm::dvec2 targetPx(0.0);
            bool targetHidden = false;
            if (effectOwnerPoint(registry, effect.followUpTarget, targetPx, targetHidden)) {
                effect.atPx = effect.followUpState.Tick(*effect.followUp, effect.atPx, targetPx, scriptMs);
            }
        }
        if (effect.scalePerFrame != 1.0) {
            const double grow = effect.scalePerFrame;
            effect.scale *= grow;
            for (Emitter& emitter : effect.emitters) {
                Particles::Scale(emitter.system, grow);
                for (Particles::Particle& particle : emitter.particles) {
                    particle.size *= grow;
                    particle.velocityPx *= grow;
                }
            }
        }
        // ETHActiveEntityHandler: only a temporary entity is the engine's to
        // delete, and only once every one of its pools is over. Anything else is
        // its script's, and goes when what it was added to goes.
        if (over) {
            freeEffect(registry, effect);
            m_effects.erase(m_effects.begin() + static_cast<std::ptrdiff_t>(i));
            continue;
        }
        ++i;
    }
}

MagicPortalsLayer::Effect* MagicPortalsLayer::effectById(int id) {
    if (id < 0) return nullptr;
    for (Effect& effect : m_effects) {
        if (effect.id == id) return &effect;
    }
    return nullptr;
}

MagicPortalsLayer::Receiver MagicPortalsLayer::effectReceiver(const Effect& effect) {
    Receiver receiver;
    receiver.applyLight = effect.art.applyLight;
    receiver.isStatic = effect.art.isStatic;
    if (!effect.art.normal.empty()) {
        receiver.normal = drawnImage(originalImage("normalmaps/" + effect.art.normal)).path;
    }
    receiver.z = effect.zUnits;
    receiver.runtimeBake = m_lit && Lighting::RuntimeBake(m_level.torch);
    return receiver;
}

// ---- a portal the player opened (the visuals plan's 7.4) ---------------------
//
// PortalManager::insertPortal adds TWO entities for every portal a shot opens
// (bytes 142281..142841): portal_summon_effect.ent at the point, and then
// portal.ent - which IS the portal, its halo sprite and its spiral and its iris.
// From there the portal runs its own callback, ETHCallback_portal
// (359141..359885), and the manager recolours its array every frame
// (138545..139692 #165-300). Four arms, and this is all four:
//
//   1. IT HOVERS. #1-10 linearMotion(thisEntity, vertical, 0, 0), with the .ent's
//      own speed 1 and stride 2: two units up and down on a 6.28 s cosine from
//      where the shot opened it. That arm is the SIMULATION's (Portals::Tick),
//      because linearMotion ends in SetPosition and doTeleporting reads that same
//      point - one point and not two - and this only follows it.
//   2. IT PERISHES. #12-86: 20,000 ms after it opened, unless dontPerish, which
//      nothing sets. Also the simulation's, and the owner's standing ruling R14.
//   3. IT IS COLOURED, and the array it is coloured in is not the list of portals
//      in play. update POPS A KILLED FRONT FIRST (#164-180, one conditional and
//      no loop), THEN reads the size (#181-188), THEN colours - portals[0] red
//      and portals[1] blue where it holds more than one (#189-228), a lone one
//      red where the level grants exactly one and V3_ZERO where it grants more
//      (#248-301) - and only then pops a killed portals[1] (#230-246). A lone
//      killed portal is NOT popped there: #255 leaves that branch before the
//      pops, so it is the next frame's front-pop that takes it, after this
//      frame's colouring has already written V3_ZERO into it. That is what
//      blacks out the surviving end of a pair a teleport consumed.
//   4. IT DIES. Once its systems are killed (killPortal, 357278..357358, which
//      KillParticleSystem(0) and (1) so the live particles live out their lives
//      and no more are released), #87-185 store killTime, fade the colour by 0.9
//      a frame, and after 400 ms add portal_death.ent and delete the entity.
//      killTime is READ BEFORE IT IS WRITTEN (#99-101 against #113-123), so the
//      frame that sets it neither fades nor counts.
//
// The port has one list of live portals, so "its systems were killed" is "its id
// left Portals::State::placed" - whether it carried something through, was
// recycled at the cap, was cleared by the button, or perished. The layer's own
// list outlives the simulation's by exactly the kill sequence.
void MagicPortalsLayer::syncPlacedPortals(entt::registry& registry, double frameMs) {
    const std::vector<Portals::Placed>& placed = m_level.portals.placed;
    const Effects::PlacedPortal& rules = m_effectRules.placedPortal;

    // WHAT IS NEW: the summon bubble first and the portal itself second, which is
    // the order insertPortal adds them in and so the order they draw their random
    // in. Added straight rather than queued, because this is already the frame.
    for (const Portals::Placed& portal : placed) {
        // A DYING ENTRY IS NOT THIS PORTAL, whatever id it carries. Portals::State
        // hands out an id once and never again, so this cannot fire today; saying
        // it here keeps the layer's drawing right on its own rather than on an
        // invariant kept in another file.
        const bool known = std::any_of(m_portalDraws.begin(), m_portalDraws.end(), [&portal](const DrawnPortal& drawn) {
            return drawn.id == portal.id && !drawn.killed;
        });
        if (known) continue;
        DrawnPortal drawn;
        drawn.id = portal.id;
        drawn.atPx = portal.atPx;
        // It has portal.ent's own white until the manager colours it, which is
        // this same frame, below.
        m_portalDraws.push_back(drawn);
        SpawnEffect(registry, effectAdd(Effects::Summon(m_effectRules, portal.atPx)));
        EffectSpawn add = effectAdd(Effects::Portal(m_effectRules, portal.atPx));
        // The between-slot table names a portal the player opened in its own right
        // (00_order K13), so its picture keeps that class rather than a plain
        // effect's; its two systems take Slot::EffectSystems like any other's.
        add.pictureSlot = Slot::PlacedPortal;
        add.follow.kind = EffectOwner::Kind::PlacedPortal;
        add.follow.index = portal.id;
        m_portalDraws.back().effect = SpawnEffect(registry, add);
    }

    // AND WHAT HAS LEFT THE SIMULATION'S LIST, which is killPortal: its two
    // systems release nothing more, and each particle already out lives the life
    // it has. A portal still in play is moved to where the hover left it.
    for (DrawnPortal& drawn : m_portalDraws) {
        if (drawn.killed) continue;
        const auto at = std::find_if(placed.begin(), placed.end(),
                                     [&drawn](const Portals::Placed& portal) { return portal.id == drawn.id; });
        if (at != placed.end()) {
            drawn.atPx = at->atPx; // its hover, which the simulation moved
            continue;
        }
        drawn.killed = true;
        if (Effect* effect = effectById(drawn.effect); effect != nullptr) {
            for (Emitter& emitter : effect->emitters) emitter.killed = true;
        }
    }

    // THEN THE MANAGER'S OWN UPDATE, over the array it still holds and in its own
    // order: pop a killed front, colour what is left, pop a killed portals[1].
    // A portal it has popped keeps the colour it had and fades from there.
    std::vector<DrawnPortal*> array;
    array.reserve(m_portalDraws.size());
    for (DrawnPortal& drawn : m_portalDraws) {
        if (!drawn.popped) array.push_back(&drawn);
    }
    if (!array.empty() && array.front()->killed) {
        array.front()->popped = true;
        array.erase(array.begin());
    }
    const int held = static_cast<int>(array.size());
    for (int i = 0; i < held; ++i) {
        glm::dvec3 colour(1.0);
        if (Effects::ManagerColour(rules, i, held, m_level.portals.budget, colour)) {
            array[static_cast<std::size_t>(i)]->colour = colour;
        }
    }
    // #230-246 tests portals[1] and calls popBack, which is that same entry
    // wherever the port can reach: once a killed front has gone the array holds
    // at most the level's budget, and no level grants more than two.
    if (held > 1 && array[1]->killed) array[1]->popped = true;

    // THEN EACH DRAWN PORTAL'S OWN CALLBACK.
    for (std::size_t i = 0; i < m_portalDraws.size();) {
        DrawnPortal& drawn = m_portalDraws[i];
        // ParticlesKilled(0) is the flag Kill(true) set and not a spent pool
        // (ETHSpriteEntity.cpp:812-828), so it is true from the kill on. The read
        // of killTime comes FIRST: the frame that sets it does not fade.
        const bool killTimeSet = drawn.killTimeSet;
        if (drawn.killed && !drawn.killTimeSet) {
            drawn.killTimeSet = true;
            drawn.sinceKillMs = 0.0;
        }
        bool gone = false;
        if (killTimeSet) {
            drawn.colour = Effects::KillFade(rules, drawn.colour, frameMs);
            drawn.sinceKillMs += frameMs;
            gone = drawn.sinceKillMs > rules.deathAfterKillMs;
        }
        if (Effect* effect = effectById(drawn.effect); effect != nullptr) {
            effect->colour = drawn.colour;
        }
        if (!gone) {
            ++i;
            continue;
        }
        // portal_death.ent where it stood, and the entity itself gone: #159-185,
        // in that order. The entity is taken out of the list FIRST here, because
        // adding one holds no reference into the other; the two draw the same
        // random either way, since freeing an effect draws none.
        const glm::dvec2 stood = drawn.atPx;
        const int effectId = drawn.effect;
        m_portalDraws.erase(m_portalDraws.begin() + static_cast<std::ptrdiff_t>(i));
        for (std::size_t e = 0; e < m_effects.size(); ++e) {
            if (m_effects[e].id != effectId) continue;
            freeEffect(registry, m_effects[e]);
            m_effects.erase(m_effects.begin() + static_cast<std::ptrdiff_t>(e));
            break;
        }
        SpawnEffect(registry, effectAdd(Effects::Death(m_effectRules, stood)));
    }
}

std::vector<MagicPortalsLayer::EffectReport> MagicPortalsLayer::EffectReports() const {
    std::vector<EffectReport> reports;
    reports.reserve(m_effects.size());
    for (const Effect& effect : m_effects) {
        EffectReport report;
        report.id = effect.id;
        report.entity = effect.entity;
        report.atPx = effect.atPx;
        report.zUnits = effect.zUnits;
        report.z = effect.z;
        report.haloZ = effect.haloZ;
        report.scriptScale = effect.scriptScale;
        report.angleDeg = effect.angleDeg;
        report.cell = effect.cell;
        report.cellPx = effect.cellPx;
        // Where its picture is centred, as updateEffects places it: the turned pivot at
        // its drawn scale, and an ET_VERTICAL entity half its height higher and unturned.
        const bool vertical = effect.art.type == Art::EntityType::Vertical;
        report.centrePx = TurnedPictureCentre(effect.atPx, effect.art.pivotPx * effect.scale * effect.bounceScale,
                                              vertical ? 0.0 : effect.angleDeg);
        if (vertical) report.centrePx.y -= effect.cellPx.y * effect.bounceScale.y * 0.5;
        if (effect.art.Frames() > 1) {
            const int columns = std::max(1, effect.art.columns);
            const int cell = std::clamp(effect.cell, 0, effect.art.Frames() - 1);
            report.uvScale = effect.cut.uvScale;
            report.uvOffset = Tiers::UvOffset(effect.cut, cell % columns, cell / columns);
        }
        report.colour = effect.colour;
        report.alpha = effect.alpha;
        report.scale = effect.scale;
        report.bounceScale = effect.bounceScale;
        report.quad = effect.quad;
        report.light = effect.light;
        report.halo = effect.halo;
        report.temporary = effect.temporary;
        report.alwaysActive = effect.alwaysActive;
        report.hidden = effect.hidden;
        report.drawn = effect.quad != entt::null;
        report.emissive = effect.emissive;
        report.firstSystemKilled = !effect.emitters.empty() && effect.emitters.front().killed;
        report.hasLight = effect.light != entt::null;
        report.hasHalo = effect.halo != entt::null;
        report.followKind = effect.follow.kind;
        report.followIndex = effect.follow.index;
        if (effect.art.light) {
            const Lighting::Light lit = Lighting::OwnerScaled(*effect.art.light, effect.scale);
            report.lightRange = lit.range;
            // The halo as it is drawn: with a lit torch's bounce, which its light
            // does not take (updateEffects).
            report.haloPx = Lighting::OwnerScaled(*effect.art.light, effect.scale * effect.bounceScale).haloSize;
        }
        report.systems = static_cast<int>(effect.emitters.size());
        report.systemZ.reserve(effect.emitters.size());
        report.systemSizePx.reserve(effect.emitters.size());
        for (const Emitter& emitter : effect.emitters) {
            report.systemZ.push_back(emitter.z);
            report.systemSizePx.push_back(emitter.system.size);
            report.systemImage.push_back(emitter.image);
            report.active += emitter.active;
            report.pool += static_cast<int>(emitter.particles.size());
            for (const Particles::Particle& particle : emitter.particles) {
                if (!particle.released) continue;
                ++report.released;
                report.ageSumMs += particle.elapsedMs;
                if (particle.elapsedMs == 0.0) report.newbornPx.push_back(particle.bornPx);
            }
            for (const entt::entity quad : emitter.quads) {
                if (quad == entt::null) continue;
                ++report.quads;
                report.particleQuads.push_back(quad);
            }
        }
        reports.push_back(std::move(report));
    }
    return reports;
}

// ---- the player's red eyes (effects.json red_eyes; the visuals plan's 7.1) ----
//
// The one effect every level shows, and the only one the original both adds at a
// level's start and moves every frame. MainCharacter's constructor runs
// AddScaledEntity("red_eyes.ent", entity.GetPosition(), getScale()) (#85-101) and
// keeps the handle SeekEntity gives it (#105-119); MainCharacter::update then runs
// updateEyes first thing, before its own pause test (#4), and updateEyes puts the
// entity at GetPosition() + g_scale.scale(vector3(eyeX, eyeY, 1)) - eyeX the sheet
// side the character faces, eyeY -14.25 or -15.65 where gravity is off - or hides
// it while the character is hidden.
//
// It has no <Sprite> and one <ParticleSystem> of 12 red hard_halo.png at
// alphaMode 1 and repeat 0, so what is drawn is a trail of added red specks that
// live 120 +- 100 ms in WORLD space: the eyes glimmer, and a walk leaves a short
// wake. Repeat 0 also makes it not temporary, so the engine never deletes it
// (Art::Entity::Temporary); it goes when the level goes.
//
// DEPTH is the table's, as every effect's is: the entity's own z is the
// CHARACTER's plus the offset's 1, and its system's <StartPoint> z 2 goes on top
// of that before the slot is counted, so the specks stand in front of the player
// and of everything the level puts between them.
//
// THE STREAM is m_effectRandom (00_order K8). This is the step the conflict was
// written for: an emitter on all 128 levels drawing from the placement stream
// would move every particle of every level.

int MagicPortalsLayer::playerDrawnFrame(const entt::registry& registry) const {
    // ETHEntity::GetFrame: the cell in the 4 x 4 sheet, which for the port's quad
    // is the run's first cell plus how far into the run it has played. Kept from
    // the last frame that had one, because the quad is destroyed when the
    // character is hidden and updateEyes still reads a frame on that frame.
    if (m_playerQuad == entt::null || !registry.valid(m_playerQuad)) return m_playerFrame;
    const auto* animation = registry.try_get<Supersonic::SpriteAnimationComponent>(m_playerQuad);
    if (animation == nullptr) return m_playerFrame;
    return static_cast<int>(animation->firstFrame + animation->frame);
}

void MagicPortalsLayer::buildEyes(entt::registry& registry) {
    using namespace Supersonic;
    m_eyes = -1;
    m_playerFrame = 0;
    if (m_level.player == entt::null || !registry.valid(m_level.player)) return;
    // The cell the level starts on: initial_direction's row on the idle column,
    // which buildDrawables has already written onto the quad.
    m_playerFrame = playerDrawnFrame(registry);
    const glm::dvec2 atPx = Units::ToPixels(registry.get<TransformComponent>(m_level.player).position);
    // Where the constructor's entity stands once the first updateEyes has moved
    // it. The port adds it there rather than at the bare GetPosition() of the
    // constructor: the original's first frame runs update before it draws, so no
    // frame of the original's ever shows the eyes at the character's own point.
    const Effects::Spawn spawn =
        Effects::RedEyes(m_effectRules, atPx, m_playerZ, m_playerFrame, m_level.noGravity);
    EffectSpawn add = effectAdd(spawn);
    // Carried by the player, which is what updateEyes' SetPosition every frame and
    // its Hide(true) together are: the port's player owner follows AND hides, and
    // effects.json's hide_with_player is refused unless it says so.
    add.follow.kind = EffectOwner::Kind::Player;
    add.follow.offsetPx = spawn.atPx - atPx;
    m_eyes = SpawnEffect(registry, add);
}

void MagicPortalsLayer::syncEyes(entt::registry& registry) {
    if (m_eyes < 0) return;
    m_playerFrame = playerDrawnFrame(registry);
    const glm::dvec3 offset = Effects::EyeOffset(m_effectRules.eyes, m_playerFrame, m_level.noGravity);
    for (Effect& effect : m_effects) {
        if (effect.id != m_eyes) continue;
        // Only the offset: the z is the same number every frame, and the point it
        // is measured from is the owner's, which updateEffects reads.
        effect.follow.offsetPx = glm::dvec2(offset);
        return;
    }
    m_eyes = -1; // freed with its owner, so there is nothing left to move
}

std::vector<MagicPortalsLayer::MinionReport> MagicPortalsLayer::MinionReports() const {
    std::vector<MinionReport> reports;
    const std::vector<Minions::Minion>& minions = m_level.minions.minions;
    for (std::size_t i = 0; i < m_minions.size() && i < minions.size(); ++i) {
        const MinionQuad& drawn = m_minions[i];
        MinionReport report;
        report.name = minions[i].name;
        report.quad = drawn.quad;
        report.black = drawn.black;
        report.column = drawn.pose.column;
        report.row = drawn.pose.row;
        report.z = drawn.z;
        report.lookZ = drawn.lookZ;
        report.centrePx = drawn.centrePx;
        report.sizePx = glm::dvec2(m_minionCut.frameUnits);
        report.uvScale = m_minionCut.uvScale;
        report.uvOffset = Tiers::UvOffset(m_minionCut, drawn.pose.column, drawn.pose.row);
        reports.push_back(std::move(report));
    }
    return reports;
}

// ---- the between-slot table (00_order K13) ----------------------------------
//
// One rule and one table for everything the port adds between the level's own
// slots. Until this step each caller carried its own fraction: the beholder, a
// spike, a minion, an escort, a dial and a well ring each wrote "- 0.5 x a slot"
// beside their own call, the placed portals "- 0.25", the halos "+ 0.25". The
// fractions have not changed; where they are written has.

float MagicPortalsLayer::slotAt(int slot, Slot klass) const {
    return SlotZ(slot) + static_cast<float>(SlotFractionOf(klass)) * kSpriteSlotZ;
}

float MagicPortalsLayer::slotAfter(double levelZ, Slot klass) const {
    // After the sprites at or below this depth - and after the player too, but
    // only where the player stands at that depth or deeper. Before step 76 this
    // asked `below < m_playerSlot`, which took the player's depth for 0; with the
    // player at 1.42 the two agree for every depth passed below but the minions'.
    const int below = static_cast<int>(std::count_if(
        m_sprites.begin(), m_sprites.end(),
        [levelZ](const DrawnSprite& d) { return static_cast<double>(d.sprite.zIndex) <= levelZ; }));
    return slotAt(m_playerZ <= levelZ ? below + 1 : below, klass);
}

float MagicPortalsLayer::slotOver(float pictureZ, Slot klass) const {
    // A picture is its slot (Slot::Picture is 0), so what hangs off it is that
    // picture's z and the class's fraction.
    return pictureZ + static_cast<float>(SlotFractionOf(klass)) * kSpriteSlotZ;
}

float MagicPortalsLayer::systemZ(float base, Slot klass, int slot, int systems) const {
    // The quarter the class starts at, shared evenly by the entity's systems so
    // that none of them ties with another or with what bounds the quarter
    // (Particles::SlotFraction, which is in (1/4, 1/2) - hence the - 0.25, which
    // makes the share start at the class's own fraction).
    //
    // `base` IS THE SLOT'S OWN ZERO, and nothing else: the table's fractions are
    // measured from there. A level's picture is its slot's zero (Slot::Picture is
    // fraction 0), so buildEmitters passes drawn.z; a spawned effect has no
    // picture at the zero, so its caller passes slotAfter(..., Slot::Picture).
    // Handing this the class's own z instead counts the fraction twice.
    return base + static_cast<float>(SlotFractionOf(klass) - 0.25 + Particles::SlotFraction(slot, systems)) *
                      kSpriteSlotZ;
}

float MagicPortalsLayer::PlayerSlotZ() const { return slotAt(m_playerSlot, Slot::Picture); }

float MagicPortalsLayer::pictureZ(int slot) const {
    // The player's slot is kept free.
    return slotAt(slot < m_playerSlot ? slot : slot + 1, Slot::Picture);
}

void MagicPortalsLayer::rankSprites() {
    if (m_rankedSprites == 0 || m_rankedSprites > m_sprites.size()) return;
    const Sprites::DrawOrder& rule = m_artRules.drawOrder;
    // Where each entity stands, as the last frame left it. A sky, a satellite and the space sky's four are
    // pinned to the camera by the port: the original moves them every frame, and a bucket that changed on
    // every frame would be no order at all, so they keep the node's own bucket.
    const auto standsAt = [](const DrawnSprite& drawn) {
        const bool pinned = drawn.sky >= 0 || drawn.satellite || drawn.space != SpaceRole::None;
        return pinned ? drawn.sprite.atPx : drawn.ownerPx;
    };
    bool crossed = false;
    for (std::size_t i = 0; i < m_rankedSprites && !crossed; ++i) {
        crossed = Sprites::BucketOf(standsAt(m_sprites[i]), rule) != m_sprites[i].bucket;
    }
    if (!crossed) return;
    std::vector<Sprites::Standing> standing;
    standing.reserve(m_rankedSprites);
    for (std::size_t i = 0; i < m_rankedSprites; ++i) {
        const DrawnSprite& drawn = m_sprites[i];
        standing.push_back(Sprites::Standing{drawn.sprite.zIndex, drawn.sprite.depth, standsAt(drawn), drawn.sprite.order});
    }
    Sprites::Rank(standing, rule);
    for (std::size_t i = 0; i < m_rankedSprites; ++i) {
        DrawnSprite& drawn = m_sprites[i];
        drawn.bucket = Sprites::BucketOf(standsAt(drawn), rule);
        if (standing[i].slot == drawn.sprite.order) continue;
        drawn.sprite.order = standing[i].slot;
        const float was = drawn.z;
        drawn.z = pictureZ(drawn.sprite.order);
        // What hangs off the picture's slot at build moves with it: its light's halo, and every system
        // of its own that stands in the slot (a lifted one stands at a depth, which did not change).
        const float by = drawn.z - was;
        for (Emitter& emitter : m_emitters) {
            if (emitter.sprite == static_cast<int>(i) && !emitter.lifted) emitter.z += by;
        }
        for (PlacedLight& placed : m_placedLights) {
            if (placed.sprite == static_cast<int>(i)) placed.haloZ += by;
        }
    }
}

std::vector<MagicPortalsLayer::TimerReport> MagicPortalsLayer::TimerReports() const {
    std::vector<TimerReport> reports;
    reports.reserve(m_timers.size());
    for (const TimerDial& dial : m_timers) {
        TimerReport report;
        report.crystal = m_level.goals.crystals[static_cast<std::size_t>(dial.crystal)].name;
        report.drawn = dial.quad != entt::null;
        report.shrinking = dial.shrinking;
        report.z = dial.z;
        report.crystalZ = m_sprites[static_cast<std::size_t>(dial.sprite)].z;
        report.frame = dial.frame;
        report.scale = dial.scale;
        report.alpha = dial.alpha;
        report.atPx = dial.atPx;
        reports.push_back(std::move(report));
    }
    return reports;
}

// ---- the menu ---------------------------------------------------------------
//
// The original's three screens, drawn with its own art: MainMenu, then
// WorldSelector, then LevelSelector.
//
// They are quads in the level's own pixel space rather than UI components,
// because the engine's UIImageComponent takes an uploaded texture handle and
// its UIButtonComponent is a coloured rounded rectangle with a text label -
// neither can show a PNG named by path, which is what every button here is. So
// the menu is drawn the way the levels are, on the same orthographic camera,
// and clicked through the same screen-to-plane mapping.
//
// What the port leaves out, and why. The original locks worlds and levels
// behind a save file its ScoreManager keeps, pages the grid by swiping
// (Swyper), and draws a page counter. The port keeps no save, so NOTHING IS
// LOCKED - a port decision, not the original's rule - and the grid pages with
// the original's own two buttons instead of a swipe.

std::string MagicPortalsLayer::menuImage(const std::string& file) const {
    // The original keeps its menu art in two places: the title, the buttons and
    // the icons under sprites/, but the two screen BACKGROUNDS among its
    // entities, beside the .ent files that place them. Asking in the wrong one
    // used to fail silently - the quad was simply not made - which is how the
    // main screen first shipped with no background at all.
    // THE HD SET, ALWAYS, which is the owner's call and not the original's rule.
    //
    // The APK ships sprites/hd/ beside sprites/: the same art at exactly twice
    // the size - level_button 64 -> 128, medal_gold_m 32 -> 64, game_main_title
    // 256 -> 512 - drawn by the artists rather than resampled. The original
    // chooses between them at run time: isHd() is GetScreenSize().y > 480 and
    // getHdSpriteDensity() returns 2 above it, so a 2013 phone got the small set
    // and a large screen the big one. That choice was a memory budget, and the
    // owner's word is that it has outlived its reason - so the port takes the
    // HD art whatever the window is doing.
    //
    // THE FALLBACK IS NOT DECORATION: 69 of the 80 files in sprites/ have an hd
    // twin and eleven do not, so a miss here is normal and must fall through
    // rather than fail. Nothing that reads a size may assume which one it got;
    // menuTick's medal is measured against the button's own size for exactly
    // that reason.
    std::error_code ec;
    const std::string hd = m_paths.original + "/sprites/hd/" + file;
    if (std::filesystem::exists(hd, ec)) return hd;
    const std::string sprites = m_paths.original + "/sprites/" + file;
    if (std::filesystem::exists(sprites, ec)) return sprites;
    return m_paths.original + "/entities/" + file;
}

glm::dvec2 MagicPortalsLayer::MenuBoxPx() const {
    // view.json's height, at the window's shape: the same box a level is shown
    // in, so the menu's art is the size the original drew it at.
    const double height = m_viewHeightPx > 0.0 ? m_viewHeightPx : 256.0;
    return glm::dvec2(height * static_cast<double>(m_aspect), height);
}

void MagicPortalsLayer::layOutMenu() {
    m_menuButtons.clear();
    if (m_screen == Screen::None) return;
    const glm::dvec2 box = MenuBoxPx();
    const auto at = [&box](double nx, double ny) { return glm::dvec2(nx * box.x, ny * box.y); };

    // A button is drawn at ITS OWN shape. Sizing one by the box alone stretches
    // whatever is not square, which is how the chapter icons - 84 x 128 - first
    // went out looking squashed.
    const auto sized = [this, &box](const char* file, double heightFraction) {
        const double height = box.y * heightFraction;
        const glm::dvec2 image = imageSizePx(menuImage(file));
        const double aspect = image.y > 0.0 ? image.x / image.y : 1.0;
        return glm::dvec2(height * aspect, height);
    };

    // The loading screen has no button; credits and the dashboard say what a touch
    // is on themselves (sim/Credits.hpp, sim/Dashboard.hpp).
    if (m_screen == Screen::Loading || m_screen == Screen::Credits || m_screen == Screen::Achievements) return;
    if (m_screen == Screen::Main) {
        // DRAWN THROUGH THE OVERLAY by EmitMenu from sim/MainMenu, which also says
        // what a touch is on. This is TAP START as it sits once its entrance is
        // over, for PressMenu and the suites: the view is the menu's box.
        const Hud::Rect settled = MainMenu::SettledPlayRect(m_mainMenuRules, box);
        MenuButton play;
        play.kind = MenuButton::Kind::Play;
        play.centrePx = settled.Centre();
        play.sizePx = settled.size;
        m_menuButtons.push_back(play);
        return;
    }

    if (m_screen == Screen::Finished || m_screen == Screen::Dead) {
        // OVER THE LEVEL, laid out by sim/LevelEnd.hpp on the camera's view and
        // drawn through the overlay (EmitHud): these are the buttons as they sit
        // once their entrance is over, in the level's pixels, which is what
        // PressMenu and the suites read. A tap is tested against where each one
        // is on its tick (endScreenTick), entrance and all.
        //
        // The finished screen's three are ONE COLUMN at x 0.75 and the lost
        // screen's two ONE ROW at y 0.6: AngelScript pushes a call's arguments
        // last-first, so LevelFinishedLayer's `PshC4 A; PshC4 B; vector2()` is
        // vector2(B, A), and the veil in the same constructor - one and a half
        // screens WIDE - is what settles the order. ui.json carries the pairs
        // already turned round.
        constexpr double kSettledMs = 1.0e9;
        const glm::dvec2 view = ViewPx();
        const glm::dvec2 corner = m_follow.centrePx - view * 0.5;
        for (const LevelEnd::Piece& piece : endPieces(view, kSettledMs)) {
            if (piece.element != LevelEnd::Element::Button) continue;
            MenuButton button;
            switch (piece.button) {
            case LevelEnd::Button::Restart:
                button.kind = MenuButton::Kind::Retry;
                break;
            case LevelEnd::Button::Next:
                button.kind = MenuButton::Kind::Next;
                break;
            case LevelEnd::Button::List:
                button.kind = MenuButton::Kind::List;
                break;
            }
            button.centrePx = corner + piece.rect.Centre();
            button.sizePx = piece.rect.size;
            m_menuButtons.push_back(button);
        }
        return;
    }

    // CHAPTER SELECT AND THE GRID are drawn through the overlay by EmitMenu from
    // sim/Selector, which also says what a touch is on (selectorInput). These are
    // their tiles at rest on their own pages and the two arrows once their entrance
    // is over, for PressMenu and the suites: the view is the menu's box.
    if (m_screen == Screen::Worlds || m_screen == Screen::Levels) {
        const Selector::Board& board = m_selectorBoard;
        for (int item = 0; item < board.items; ++item) {
            const Hud::Rect rect = Selector::ItemRect(m_selectorRules, board, item, box);
            MenuButton button;
            button.centrePx = rect.Centre();
            button.sizePx = rect.size;
            if (board.levels) {
                button.kind = MenuButton::Kind::Level;
                button.world = board.world;
                button.level = board.tiles[static_cast<std::size_t>(item)].level;
            } else {
                button.kind = MenuButton::Kind::World;
                button.world = item;
            }
            m_menuButtons.push_back(button);
        }
        constexpr double kSettledMs = 1.0e9;
        const bool levels = m_screen == Screen::Levels;
        for (const auto& [kind, placed] :
             {std::pair<MenuButton::Kind, const UiLayer::Placed*>{
                  MenuButton::Kind::Back, levels ? &m_selectorRules.levels.back : &m_selectorRules.chapters.back},
              std::pair<MenuButton::Kind, const UiLayer::Placed*>{
                  MenuButton::Kind::Forward,
                  levels ? &m_selectorRules.levels.forward : &m_selectorRules.chapters.forward}}) {
            const Hud::Rect rect = UiLayer::RectAt(
                *placed, UiLayer::ButtonAnchorAt(m_selectorRules.layer, UiLayer::Anchor(*placed, box), box, kSettledMs));
            MenuButton button;
            button.kind = kind;
            button.world = m_selectorBoard.world;
            button.centrePx = rect.Centre();
            button.sizePx = rect.size;
            m_menuButtons.push_back(button);
        }
    }
}

void MagicPortalsLayer::buildMenu(entt::registry& registry) {
    using namespace Supersonic;
    unloadMenuDrawables(registry);
    if (m_screen == Screen::None) return;

    // An image that cannot be read SAYS SO. Skipping it quietly is what hid the
    // missing backgrounds: a menu with no background looks like a menu someone
    // designed that way, and nothing anywhere said the file had not been found.
    const auto quadFor = [this, &registry](const char* tag, const std::string& path) {
        if (imageSizePx(path).y > 0.0) return makeSprite(registry, tag, path, false);
        SUPERSONIC_LOG_WARN("Magic Portals") << "menu image could not be read: " << path << std::endl;
        return entt::entity{entt::null};
    };

    // The finished and lost screens put nothing in the registry: they are drawn
    // over their level through the screen overlay (EmitHud). Nor do the main
    // menu, credits and the dashboard, which EmitMenu draws, backgrounds and all,
    // in display values.
    if (m_screen == Screen::Finished || m_screen == Screen::Dead || m_screen == Screen::Main ||
        m_screen == Screen::Credits || m_screen == Screen::Achievements) {
        return;
    }
    if (m_screen == Screen::Loading) {
        // scenes/loading_screen.esc: its background, its character and its portal,
        // in the level's space as a level's art is. The background is sized by
        // ui.json and drawn with its hd twin (sim/Loading.hpp); the character and the
        // portal's halo are the level's pictures, each the file the loader draws at
        // its units (drawnImage).
        m_loadingBg = quadFor("Magic Portals Loading Background", originalAsset(m_loadingRules.background.sprite));
        const Sprites::Image& halo = drawnImage(originalImage(m_artRules.portal.sprite));
        if (halo.units.y > 0) {
            m_loadingPortal =
                makeSprite(registry, "Magic Portals Loading Portal", halo.path, m_artRules.portal.additive);
        }
        // black_halo.ent under it, as the multiply it is (ui.json loading.portal):
        // its picture as the loader finds it (drawnImage) - black_halo.bmp has no
        // tier file, so it is the file named.
        if (const std::string multiply =
                modulateImage(drawnImage(m_paths.original + "/" + m_loadingRules.haloSprite).path);
            !multiply.empty()) {
            m_loadingHalo = makeSprite(registry, "Magic Portals Loading Halo", multiply, false);
        }
        // portal.ent's particle systems, the spiral and portal.png, just in front of
        // its halo and in the order the .ent states them.
        unloadEmitters(registry);
        // And nothing a level added is left standing over the loading screen.
        unloadEffects(registry);
        m_loadingVanished = false;
        const glm::dvec2 portalAt = Loading::PortalAt(m_loadingRules, MenuBoxPx());
        addEntityEmitters(m_loadingRules.portalEntity, portalAt, 0.32f, 0.0);
        m_loadingPortalEmitters = m_emitters.size();
        const Art::Character& mage = m_artRules.character;
        const Sprites::Image& sheet = drawnImage(originalImage(mage.sprite));
        if (frameUnits(sheet, mage.columns, mage.rows) != glm::dvec2(0.0)) {
            m_loadingCharacter = makeSprite(registry, "Magic Portals Loading Character", sheet.path, mage.additive);
            auto& animation = registry.emplace<SpriteAnimationComponent>(m_loadingCharacter);
            animation.columns = static_cast<uint32_t>(mage.columns);
            animation.rows = static_cast<uint32_t>(mage.rows);
            animation.firstFrame = static_cast<uint32_t>(m_loadingRules.firstFrame);
            animation.frameCount = 1;
            animation.playing = false; // the frame is the loop's (Loading::CharacterFrame)
        }
        return;
    }
    // Chapter select and the grid put nothing in the registry either: EmitMenu
    // draws them through the screen overlay (sim/Selector).
}

void MagicPortalsLayer::unloadMenuDrawables(entt::registry& registry) {
    auto destroy = [&registry](entt::entity& e) {
        if (e != entt::null && registry.valid(e)) registry.destroy(e);
        e = entt::null;
    };
    for (auto& e : m_menuQuads) destroy(e);
    m_menuQuads.clear();
    for (auto& e : m_menuLabels) destroy(e);
    m_menuLabels.clear();
    for (auto& e : m_menuMedals) destroy(e);
    m_menuMedals.clear();
    destroy(m_menuBg);
    destroy(m_menuTitle);
    destroy(m_loadingBg);
    destroy(m_loadingCharacter);
    destroy(m_loadingPortal);
    destroy(m_loadingHalo);
}

void MagicPortalsLayer::unloadMenu(entt::registry& registry) {
    unloadMenuDrawables(registry);
    m_menuButtons.clear();
    // A popup over a menu state goes with the state.
    m_menuPopup = MenuPopupScreen{};
    m_selectorLayerFromMs = 0.0;
}

void MagicPortalsLayer::openFinished(entt::registry& registry) {
    // setCurrentLayer('levelFinishedLayer') and playVictorySound.
    latch("medal_shown");
    // The screen's clock starts here, on the tick it becomes current, and its
    // counters from nothing: ScoreCounter(0, numPortals, 100) and the crystals'
    // own. Set once, here - a window resize lays the screen out again and must
    // not restart the count.
    m_end.clockMs = 0.0;
    if (m_lastCleared) {
        m_end.play.portalsUsed = m_lastCleared->portalsUsed;
        m_end.play.goldenScore = m_lastCleared->goldenScore;
        m_end.play.crystals = m_lastCleared->crystals;
        m_end.play.crystalsTotal = m_lastCleared->crystalsTotal;
    }
    m_end.portals = LevelEnd::Counter{0, m_end.play.portalsUsed, 0.0};
    m_end.crystals = LevelEnd::Counter{0, m_end.play.crystals, 0.0};
    // The level STAYS, and goes on running: neither end screen stops game time
    // (spec D7). The HUD it has left is laid out by layOutControls as a level
    // that has ended.
    m_screen = Screen::Finished;
    m_aspect = viewportAspect(registry);
    layOutMenu();
    SUPERSONIC_LOG_INFO("Magic Portals") << "finished screen current on tick " << m_ticks << std::endl;
}

void MagicPortalsLayer::openDead(entt::registry& registry) {
    // The SCREEN'S cue, and not the moment of death: checkGameEnd plays
    // playDeathSound here, where it raises levelLostLayer. A fall's own sound
    // played 1400 ms ago and an hp death made no sound at all.
    latch("player_died");
    m_end.clockMs = 0.0;
    // The level STAYS, and goes on running, as it does behind the medal.
    m_screen = Screen::Dead;
    m_aspect = viewportAspect(registry);
    layOutMenu();
    SUPERSONIC_LOG_INFO("Magic Portals") << "lost screen current on tick " << m_ticks << std::endl;
}

void MagicPortalsLayer::openMenu(entt::registry& registry, Screen screen) {
    // A level and a menu are never both in the registry.
    unloadLevel(registry);
    m_loaded = false;
    m_current = -1;
    m_loadError.clear();
    // A STATE CHANGE opens under a black of its own, with every clock from
    // nothing (BaseState::preLoop). Another page of the same grid is not one.
    const bool changed = screen != m_screen;
    if (changed) {
        beginMenuState();
        // A popup over the last state goes with it; a new one is the caller's.
        m_menuPopup = MenuPopupScreen{};
        m_selectorLayerFromMs = 0.0;
    }
    m_screen = screen;
    m_aspect = viewportAspect(registry);
    // PortalMainMenu::preLoop #368 playMenuMusic: the ONE state that changes the
    // track. Chapter and level select, credits and the dashboard call no music
    // hook, so what played before them - a level's track, or the silence of a
    // no_music level or a beaten boss - goes on under them.
    if (changed && screen == Screen::Main) {
        musicPlay(kMenuTrack);
        // #395-396 checkPreviouslyUnlockAchievements, on every visit, silently.
        achievementsMenuPass();
    }
    if (changed && screen == Screen::Worlds) {
        // WorldSelector::WorldSelector #265-291: every chapter's badges, with a card.
        int worlds = 0;
        for (const Chapters::Level& level : m_chapters.levels) worlds = std::max(worlds, level.world + 1);
        for (int world = 0; world < worlds; ++world) achievementsForChapter(world, true);
    }
    if (changed && screen == Screen::Credits) {
        // CreditsScreenLayer's constructor: the strip's top on the bottom edge.
        m_credits = Credits::Start(MenuBoxPx());
        m_creditsTouch = CreditsTouch{};
    }
    if (changed && screen == Screen::Achievements) {
        // ScoreDashboard::preLoop: a fresh list at the top, and what the save has
        // unlocked of it now.
        // AchievementManager::isUnlocked reads the saved list (planning step 143).
        const Achievements::Content* content = m_achievementsLoaded ? &m_achievements : nullptr;
        m_dashboardBoard = m_achievementRulesReady
                               ? Dashboard::Build(content, m_lockingRules, m_scores, m_chapters,
                                                  m_achievementStore.Ids())
                               : Dashboard::Build(content, m_lockingRules, m_scores, m_chapters);
        m_dashboardState = Dashboard::State{};
        for (const std::string* font :
             {&m_dashboardRules.header.font, &m_dashboardRules.title.font, &m_dashboardRules.description.font,
              &m_dashboardRules.points.font, &m_dashboardRules.total.font, &m_dashboardRules.newLabel.text.font}) {
            loadUiFont(*font);
        }
    }
    if (changed && (screen == Screen::Worlds || screen == Screen::Levels)) {
        // LevelSelector::preLoop: what the save has opened, and setCurrentPage from
        // page 0 to the opening page, which slides in under the black.
        m_selectorBoard = screen == Screen::Worlds
                              ? Selector::BuildChapters(m_selectorRules, m_lockingRules, m_scores, m_chapters)
                              : Selector::BuildLevels(m_selectorRules, m_lockingRules, m_scores, m_chapters, m_menuWorld);
        m_selectorState = Selector::Open(m_selectorBoard, Selector::OpeningPage(m_selectorBoard, m_menuWorld));
        m_selectorDown = Selector::Hit{};
        m_selectorHeld = Selector::Hit{};
        const Selector::Rules& selector = m_selectorRules;
        for (const std::string* font :
             {&selector.chapters.percent.font, &selector.chapters.warning.font, &selector.levels.number.font,
              &selector.levels.cornerChapter.font, &selector.levels.cornerPercent.font}) {
            loadUiFont(*font);
        }
    }
    layOutMenu();
    buildMenu(registry);
    if (screen == Screen::Loading) loadUiFont(m_loadingRules.dots.font);
}

bool MagicPortalsLayer::PressMenu(entt::registry& registry, MenuButton button) {
    // Every button makes a noise, and not the same one: the menu's own is the
    // only thing in the game that plays button.mp3, while the buttons a level
    // puts up - retry, next, the list - are all a teleport. So are the world and
    // level tiles: Page::setupItemButton #127-143 gives each one
    // getItemSelectButtonSoundName (planning step 142; button.mp3 before), which
    // it plays on the press whether or not the tile is open.
    switch (button.kind) {
    case MenuButton::Kind::Play:
    case MenuButton::Kind::Back:
    case MenuButton::Kind::Forward:
        latch("menu_button");
        break;
    case MenuButton::Kind::World:
    case MenuButton::Kind::Level:
    case MenuButton::Kind::Retry:
    case MenuButton::Kind::Next:
    case MenuButton::Kind::List:
        latch("level_button");
        break;
    }

    switch (button.kind) {
    case MenuButton::Kind::Play:
        openMenu(registry, Screen::Worlds);
        return true;
    case MenuButton::Kind::World:
        // WorldChooser::validateItem (owner ruling R3): a locked chapter does nothing.
        if (m_screen == Screen::Worlds &&
            !Locking::ChapterUnlocked(m_lockingRules, m_scores, m_chapters, button.world)) {
            return false;
        }
        m_menuWorld = button.world;
        m_menuPage = 0;
        openMenu(registry, Screen::Levels);
        return true;
    case MenuButton::Kind::Level: {
        if (button.level < 0 || button.level >= static_cast<int>(m_chapters.levels.size())) return false;
        // LevelChooser::validateItem: a locked level does nothing.
        const Chapters::Level& chosen = m_chapters.levels[static_cast<std::size_t>(button.level)];
        if (!Locking::LevelUnlocked(m_lockingRules, m_scores, m_chapters, chosen.world, chosen.index)) return false;
        unloadMenu(registry);
        m_screen = Screen::None;
        loadLevel(registry, button.level);
        return true;
    }
    case MenuButton::Kind::Back:
        // PageManager::update: the previous page, or from the first page a state
        // up - chapter select from the grid, the main menu from chapter select.
        if (m_screen != Screen::Worlds && m_screen != Screen::Levels) return false;
        if (m_selectorState.page > 0) {
            Selector::SetPage(m_selectorState, m_selectorState.page - 1);
            return true;
        }
        openMenu(registry, m_screen == Screen::Levels ? Screen::Worlds : Screen::Main);
        return true;
    case MenuButton::Kind::Forward:
        if (m_screen != Screen::Worlds && m_screen != Screen::Levels) return false;
        if (m_selectorState.page >= m_selectorBoard.Pages() - 1) return false;
        Selector::SetPage(m_selectorState, m_selectorState.page + 1);
        return true;
    case MenuButton::Kind::Retry:
        // The level is still loaded behind the medal; loadLevel rebuilds it.
        if (m_current < 0) return false;
        unloadMenu(registry);
        m_screen = Screen::None;
        loadLevel(registry, m_current);
        return true;
    case MenuButton::Kind::Next:
        if (m_current < 0) return false;
        unloadMenu(registry);
        m_screen = Screen::None;
        goTo(registry, m_chapters.Next(m_current));
        return true;
    case MenuButton::Kind::List:
        if (m_current >= 0) m_menuWorld = m_chapters.levels[static_cast<std::size_t>(m_current)].world;
        openMenu(registry, Screen::Levels);
        return true;
    }
    return false;
}

void MagicPortalsLayer::menuTick(entt::registry& registry, const ScriptClock::Step& step) {
    using namespace Supersonic;
    // WHAT THE LAST TICK ASKED FOR, before anything else: a release, the loading
    // screen's hold or the back key. The original's setState swaps between
    // frames, so its release frame is drawn in the old state untinted and the new
    // state's black comes on the next (ui3 spec 0.4, A-S5).
    if (m_pendingMenu.kind != PendingMenu::Kind::None) {
        const PendingMenu pending = m_pendingMenu;
        m_pendingMenu = PendingMenu{};
        switch (pending.kind) {
        case PendingMenu::Kind::MainButton:
            PressMainMenu(registry, pending.main);
            break;
        case PendingMenu::Kind::Button:
            PressMenu(registry, pending.button);
            break;
        case PendingMenu::Kind::Screen:
            openMenu(registry, pending.screen);
            break;
        case PendingMenu::Kind::Achievement:
            openAchievement(registry, pending.world, pending.level);
            break;
        case PendingMenu::Kind::None:
            break;
        }
        // A level was entered: this was the tick it loaded on, and its first tick
        // is the next.
        if (m_screen == Screen::None || m_screen == Screen::Finished || m_screen == Screen::Dead) return;
    }

    // The state's clock: its first tick is its zero, and it goes on from there.
    if (m_menuClock.fresh) {
        m_menuClock.fresh = false;
    } else {
        ++m_menuClock.ticks;
        m_menuClock.ms += ScriptClock::Ms(step);
        m_menuClock.wallMs += ScriptClock::WallMs(step);
    }

    m_aspect = viewportAspect(registry);
    layOutMenu(); // the window may have changed shape since the last tick
    const glm::dvec2 box = MenuBoxPx();

    // Only the menu screens: the finished and lost screens are over a running
    // level and never take this tick (OnFixedUpdate).
    if (m_camera != entt::null && registry.valid(m_camera)) {
        auto& camera = registry.get<CameraComponent>(m_camera);
        const glm::vec3 centre = Units::ToWorld(box.x * 0.5, box.y * 0.5);
        camera.position = glm::vec3(centre.x, centre.y, kCameraDistance);
        camera.aspect = m_aspect;
        camera.orthoHeight = Units::ToMetres(box.y);
        registry.get<TransformComponent>(m_camera).position = camera.position;
        // A screen is a cut, not a pan.
        if (auto* interpolated = registry.try_get<InterpolatedCameraComponent>(m_camera)) {
            interpolated->captured = false;
        }
    }

    if (m_screen == Screen::Loading) {
        loadingTick(registry);
        return;
    }
    if (m_screen == Screen::Main) {
        mainMenuInput(registry);
        return;
    }
    if (m_screen == Screen::Credits) {
        creditsInput(registry, step);
        return;
    }
    if (m_screen == Screen::Achievements) {
        dashboardInput(registry);
        return;
    }

    if (m_screen == Screen::Worlds || m_screen == Screen::Levels) {
        if (m_menuPopup.open) {
            menuPopupTick(registry, step);
        } else {
            selectorInput(registry);
        }
    }
}

void MagicPortalsLayer::openMenuPopup(const Popup::Class& cls) {
    // Its pictures, resolved as buildControls resolves a level's popups' - which
    // went with the level (unloadControls): there is none under a menu state.
    for (const std::string* file : {&cls.card.sprite, &cls.closeButton.sprite}) {
        if (m_popupImages.find(*file) != m_popupImages.end()) continue;
        const std::string path = originalAsset(*file);
        m_popupImages[*file] = imageSizePx(path).y > 0.0 ? path : std::string();
    }
    // As openPopup: one whose card or close button cannot be read is said once and
    // not raised, rather than standing the chapter select still under nothing.
    const auto drawable = [this](const std::string& file) {
        const auto found = m_popupImages.find(file);
        return found != m_popupImages.end() && !found->second.empty();
    };
    if (!drawable(cls.card.sprite) || !drawable(cls.closeButton.sprite)) {
        SUPERSONIC_LOG_WARN("Magic Portals") << "popup " << cls.name << " not raised: its card or close button "
                                             << "could not be read" << std::endl;
        return;
    }
    // Raised in the state's preLoop, so its clock is the state's: zero on the
    // state's first tick. Popup::Popup pauses g_timeManager, which nothing on a
    // menu state reads.
    m_menuPopup = MenuPopupScreen{};
    m_menuPopup.open = true;
    m_menuPopup.cls = &cls;
    m_menuPopup.state = Popup::Start(cls);
    m_menuTouch = MenuTouch{};
    m_selectorDown = Selector::Hit{};
    m_selectorHeld = Selector::Hit{};
    SUPERSONIC_LOG_INFO("Magic Portals") << "popup " << cls.name << " opened over the menu on tick " << m_ticks
                                         << std::endl;
}

namespace {

// Popup::hasReceivedCloseCommand's touch half: where the class taps to close, a
// touch down anywhere; else a press of 'close', which Button::update makes of a
// release inside the button whose touch went down inside it (bytes 15892..16865).
// The back key is the caller's.
bool TouchClosesPopup(const Popup::Rules& rules, const Popup::Class& cls, const Popup::Open& open,
                      const glm::dvec2& view, bool pressed, bool released, bool over, const glm::dvec2& at,
                      bool& downOnClose) {
    if (Popup::Closing(open)) {
        downOnClose = false;
        return false;
    }
    if (cls.tapScreenToClose && pressed && over) return true;
    const Hud::Rect button = Popup::CloseButtonRect(rules, cls, open, view);
    if (pressed) downOnClose = over && button.Contains(at);
    if (!released) return false;
    const bool press = downOnClose && over && button.Contains(at);
    downOnClose = false;
    return press;
}

} // namespace

void MagicPortalsLayer::menuPopupTick(entt::registry& registry, const ScriptClock::Step& step) {
    using Supersonic::Input;
    // LevelSelector::loop reads the back key whatever layer is current: the state
    // goes, on the next tick as ever, and the popup with it.
    if (Input::TickWasPressed(kBack) || devPressDue(DevPress::Back)) {
        m_menuPopup.downOnClose = false;
        m_pendingMenu.kind = PendingMenu::Kind::Screen;
        m_pendingMenu.screen = m_screen == Screen::Levels ? Screen::Worlds : Screen::Main;
        return;
    }
    // UI frame time from the state's first tick, which is the popup's zero. The
    // PageManager is not the current layer, so its clock stands (SelectorLayerMs).
    const double dtMs = m_menuClock.ticks > 0 ? ScriptClock::Ms(step) : 0.0;
    Popup::Tick(*m_menuPopup.cls, m_menuPopup.state, dtMs);
    // Read once a tick: a scheduled tap steps as it is read.
    const Touch touch = touchThisTick(registry);
    const bool fingerDown = (touch.pressed || touch.held) && !touch.released;
    // The pager's slide goes on under it: PageManager::draw decays it, through
    // Swyper::getOffset (bytes 21658..21861), which reads touch 0's state itself -
    // no decay while a finger is down (KS_HIT or KS_DOWN), wherever it is. The
    // swyper's own touch is not tracked meanwhile, so `touching` is put back after.
    // Its update's fadeForwardArrows does not run, so the arrow's fade is put back.
    const int forward = m_selectorState.forwardAlphaByte;
    const bool touching = m_selectorState.touching;
    m_selectorState.touching = fingerDown;
    Selector::Step(m_selectorRules, m_selectorBoard, m_selectorState);
    m_selectorState.touching = touching;
    m_selectorState.forwardAlphaByte = forward;

    Popup::Open& open = m_menuPopup.state;
    const bool closes = TouchClosesPopup(m_popupRules, *m_menuPopup.cls, open, MenuBoxPx(), touch.pressed,
                                         touch.released, touch.over, touch.atView, m_menuPopup.downOnClose);
    // Button::update #206-233, in UILayer::update before the close is read: while
    // the touch that went down on the button is on it, 0xFFCCCCCC. A release is not
    // held, so the press that closes leaves it untinted for the dismissal.
    if (!Popup::Closing(open)) {
        const Hud::Rect button = Popup::CloseButtonRect(m_popupRules, *m_menuPopup.cls, open, MenuBoxPx());
        open.closeTinted = fingerDown && m_menuPopup.downOnClose && touch.over && button.Contains(touch.atView);
    }
    if (closes) {
        Popup::Close(open);
        SUPERSONIC_LOG_INFO("Magic Portals") << "popup closed over the menu on tick " << m_ticks << std::endl;
    }
    // Popup::update: once everything is dismissed, the last layer is current
    // again, and the PageManager is updated from the next tick.
    if (!Popup::Gone(m_popupRules, m_menuPopup.state)) return;
    m_menuPopup = MenuPopupScreen{};
    m_selectorLayerFromMs = m_menuClock.ms + ScriptClock::Ms(step);
    SUPERSONIC_LOG_INFO("Magic Portals") << "popup gone over the menu on tick " << m_ticks << std::endl;
}

void MagicPortalsLayer::beginMenuState() {
    m_menuClock = MenuClock{};
    m_menuTouch = MenuTouch{};
    // SoundPanelLayer is built afresh with the state: its music switch comes in
    // with the rest, if the sound is on.
    m_mainMusicAddedMs = 0.0;
    m_mainMusicDismissedMs = -1.0;
}

void MagicPortalsLayer::loadingTick(entt::registry& registry) {
    // Nothing on the loading screen takes a touch; a DEV tap due now is spent.
    (void)touchThisTick(registry);
    const Loading::Rules& rules = m_loadingRules;
    const glm::dvec2 view = MenuBoxPx();
    // LoadingScreen::loop's frame: 1 on the state's first tick.
    const int frame = m_menuClock.ticks + 1;
    // LoadingScreen::loop reads sys:GetLastFrameElapsedTime directly: the script's floor.
    const double tickMs = ScriptClock::Ms(m_step);

    if (m_loadingBg != entt::null && registry.valid(m_loadingBg)) {
        placeSprite(registry, m_loadingBg, rules.background.centreOfScreen * view, rules.background.sizeUnits, -1.0f,
                    0.0f);
    }
    const glm::dvec2 portal = Loading::PortalAt(rules, view);
    if (m_loadingPortal != entt::null && registry.valid(m_loadingPortal)) {
        placeSprite(registry, m_loadingPortal, portal,
                    glm::dvec2(drawnImage(originalImage(m_artRules.portal.sprite)).units), 0.3f, 0.0f);
    }
    if (m_loadingHalo != entt::null && registry.valid(m_loadingHalo)) {
        // black_halo.ent, OVER the portal's halo and under its particles. The
        // decode adds it at the portal's z less 4, which Ethanon's draw hash would
        // sort under the halo; the recording says otherwise - launch_gold frame 183,
        // once the spiral is killed, is a black blob with no trace of the added
        // halo, and frames 74-155 show the spiral light on dark - so the port draws
        // what the frames show. Why is not settled.
        placeSprite(registry, m_loadingHalo, portal,
                    glm::dvec2(drawnImage(m_paths.original + "/" + rules.haloSprite).units) * rules.haloScale, 0.31f,
                    0.0f);
    }
    if (!Loading::CharacterShown(rules, frame) && !m_loadingVanished) {
        // The frame the last texture loads: vanishEffect and killPortal.
        m_loadingVanished = true;
        for (std::size_t i = 0; i < m_loadingPortalEmitters && i < m_emitters.size(); ++i) m_emitters[i].killed = true;
        addEntityEmitters(rules.vanish.suckEntity, Loading::SuckAt(rules, view), 0.6f,
                          Loading::SuckAngleDeg(rules, view));
        addEntityEmitters(rules.vanish.sparklesEntity, Loading::SparklesAt(rules, view), 0.61f, 0.0);
    }
    if (m_loadingCharacter != entt::null && registry.valid(m_loadingCharacter)) {
        if (Loading::CharacterShown(rules, frame)) {
            const Art::Character& mage = m_artRules.character;
            const glm::dvec2 cell = frameUnits(drawnImage(originalImage(mage.sprite)), mage.columns, mage.rows);
            registry.get<Supersonic::SpriteAnimationComponent>(m_loadingCharacter).firstFrame =
                static_cast<uint32_t>(Loading::CharacterFrame(rules, frame, tickMs));
            // The image stands with its pivot on the entity, as the level's player.
            placeSprite(registry, m_loadingCharacter,
                        Loading::CharacterAt(rules, view, frame) - glm::dvec2(mage.pivotXPx, mage.pivotYPx), cell,
                        0.5f, 0.0f);
        } else {
            // ETHEntity::Hide, on the frame the last texture loads.
            registry.destroy(m_loadingCharacter);
            m_loadingCharacter = entt::null;
        }
    }
    if (Loading::HoldOver(rules, frame, tickMs)) {
        m_pendingMenu.kind = PendingMenu::Kind::Screen;
        m_pendingMenu.screen = Screen::Main;
        SUPERSONIC_LOG_INFO("Magic Portals") << "loading held on tick " << m_ticks << " (frame " << frame
                                             << "); the main menu is next" << std::endl;
    }
}

void MagicPortalsLayer::mainMenuInput(entt::registry& registry) {
    using Supersonic::Input;
    // OWNER RULING R2: the back key on the main menu leaves the game, as
    // MainMenuLayer::update does (GetKeyState(14), then Exit). The engine's
    // latch stops the run loop after this frame.
    if (Input::TickWasPressed(kBack) || devPressDue(DevPress::Back)) {
        SUPERSONIC_LOG_INFO("Magic Portals") << "back key on the main menu on tick " << m_ticks << ": quitting"
                                             << std::endl;
        Supersonic::Application::RequestQuit();
        return;
    }
    const Touch touch = touchThisTick(registry);
    const glm::dvec2 view = MenuBoxPx();
    const Pause::Switches switches = MainMenuSwitches();
    if (touch.pressed) {
        MenuState::TouchDown(m_menuTouch.touch, touch.atView);
        m_menuTouch.mainDown =
            MainMenu::ButtonsAt(m_mainMenuRules, switches, view, m_menuClock.ms, touch.atView);
    }
    const unsigned inside =
        touch.over ? MainMenu::ButtonsAt(m_mainMenuRules, switches, view, m_menuClock.ms, touch.atView) : 0u;
    m_menuTouch.mainHeld = touch.held && !touch.released ? (m_menuTouch.mainDown & inside) : 0u;
    if (!touch.released) return;
    const unsigned pressed = m_menuTouch.mainDown & inside;
    m_menuTouch = MenuTouch{};
    const std::optional<MainMenu::Button> first = MainMenu::FirstActed(pressed);
    if (!first) return;
    if (*first == MainMenu::Button::Sound || *first == MainMenu::Button::Music) {
        // A switch changes on its own update, so on the release frame itself.
        PressMainMenu(registry, *first);
        return;
    }
    m_pendingMenu.kind = PendingMenu::Kind::MainButton;
    m_pendingMenu.main = *first;
}

void MagicPortalsLayer::selectorInput(entt::registry& registry) {
    using Supersonic::Input;
    const Selector::Rules& rules = m_selectorRules;
    // The back key goes up a state, on the next tick as a release does: from
    // either page of the grid to chapter select, and from chapter select to the
    // main menu (LevelSelector::loop, ui3 spec 5.6, 6.6).
    if (Input::TickWasPressed(kBack) || devPressDue(DevPress::Back)) {
        m_menuTouch = MenuTouch{};
        m_selectorDown = Selector::Hit{};
        m_selectorHeld = Selector::Hit{};
        m_pendingMenu.kind = PendingMenu::Kind::Screen;
        m_pendingMenu.screen = m_screen == Screen::Levels ? Screen::Worlds : Screen::Main;
        return;
    }
    const Touch touch = touchThisTick(registry);
    const glm::dvec2 view = MenuBoxPx();
    const auto same = [](const Selector::Hit& a, const Selector::Hit& b) {
        return a.control == b.control && a.item == b.item;
    };
    if (touch.pressed) {
        MenuState::TouchDown(m_menuTouch.touch, touch.atView);
        m_selectorDown =
            Selector::HitAt(rules, m_selectorBoard, m_selectorState, view, SelectorLayerMs(), touch.atView);
        Selector::TouchDown(m_selectorState, touch.atView.x);
    }
    if (touch.held || touch.released) {
        MenuState::TouchHeld(m_menuTouch.touch, touch.atView);
        Selector::TouchMove(m_selectorState, touch.atView.x, view.x);
    }
    const Selector::Hit on = touch.over ? Selector::HitAt(rules, m_selectorBoard, m_selectorState, view,
                                                          SelectorLayerMs(), touch.atView)
                                        : Selector::Hit{};
    const bool inside = m_selectorDown.control != Selector::Control::None && same(on, m_selectorDown);
    m_selectorHeld = inside && touch.held && !touch.released ? m_selectorDown : Selector::Hit{};
    if (touch.released) {
        const Selector::Hit down = m_selectorDown;
        // Page::update's TouchGapDetector: a tile refuses a finger that travelled.
        const bool takes =
            down.control != Selector::Control::Item || MenuState::TileTakes(rules.state, m_menuTouch.touch);
        Selector::TouchUp(rules, m_selectorBoard, m_selectorState);
        m_menuTouch = MenuTouch{};
        m_selectorDown = Selector::Hit{};
        if (inside && takes) {
            MenuButton button;
            button.world = m_selectorBoard.world;
            switch (down.control) {
            case Selector::Control::Back:
                button.kind = MenuButton::Kind::Back;
                break;
            case Selector::Control::Forward:
                button.kind = MenuButton::Kind::Forward;
                break;
            case Selector::Control::Item:
                if (m_selectorBoard.levels) {
                    button.kind = MenuButton::Kind::Level;
                    button.level = m_selectorBoard.tiles[static_cast<std::size_t>(down.item)].level;
                } else {
                    button.kind = MenuButton::Kind::World;
                    button.world = down.item;
                }
                break;
            case Selector::Control::None:
                break;
            }
            const bool pageTurn =
                (button.kind == MenuButton::Kind::Back && m_selectorState.page > 0) ||
                button.kind == MenuButton::Kind::Forward;
            if (down.control != Selector::Control::None) {
                if (pageTurn) {
                    // A page of the same state: drawn from the release frame.
                    PressMenu(registry, button);
                } else {
                    m_pendingMenu.kind = PendingMenu::Kind::Button;
                    m_pendingMenu.button = button;
                }
            }
        }
    }
    Selector::Step(rules, m_selectorBoard, m_selectorState);
}

void MagicPortalsLayer::creditsInput(entt::registry& registry, const ScriptClock::Step& step) {
    using Supersonic::Input;
    // BackButtonLayer::isBackButtonPressed's key: the main menu, on the next tick.
    if (Input::TickWasPressed(kBack) || devPressDue(DevPress::Back)) {
        m_creditsTouch = CreditsTouch{};
        m_pendingMenu.kind = PendingMenu::Kind::Screen;
        m_pendingMenu.screen = Screen::Main;
        return;
    }
    const Touch touch = touchThisTick(registry);
    const glm::dvec2 view = MenuBoxPx();
    const double ms = m_menuClock.ms;
    // GetTouchState 1 or 2: pressed this tick, or still held.
    const bool touching = touch.pressed || (touch.held && !touch.released);
    if (touch.pressed) {
        m_creditsTouch.down = true;
        m_creditsTouch.lastAt = touch.atView;
        m_creditsTouch.downOnBack = Credits::BackHitRect(m_creditsRules, view, ms).Contains(touch.atView);
    }
    // GetTouchMove: how far it went since the last tick, nothing on the tick it went down.
    double move = 0.0;
    if (touching) {
        move = touch.atView.y - m_creditsTouch.lastAt.y;
        m_creditsTouch.lastAt = touch.atView;
    }
    // The state's first tick is where the constructor put the strip; it moves from
    // the next, a tick's worth each.
    if (m_menuClock.ticks > 0) {
        Credits::Step(m_creditsRules, m_credits, view, ScriptClock::Ms(step), touching, move);
    }
    const bool inside = touch.over && Credits::BackHitRect(m_creditsRules, view, ms).Contains(touch.atView);
    m_creditsTouch.backHeld = m_creditsTouch.down && m_creditsTouch.downOnBack && touching && inside;
    if (!touch.released) return;
    const bool pressed = m_creditsTouch.down && m_creditsTouch.downOnBack && inside;
    m_creditsTouch = CreditsTouch{};
    if (!pressed) return;
    // getButtonSoundName, and the menu state on the next tick.
    latch("menu_button");
    m_pendingMenu.kind = PendingMenu::Kind::Screen;
    m_pendingMenu.screen = Screen::Main;
}

void MagicPortalsLayer::dashboardInput(entt::registry& registry) {
    using Supersonic::Input;
    // DashboardLayer::update: the back key goes where the back button goes.
    if (Input::TickWasPressed(kBack) || devPressDue(DevPress::Back)) {
        m_pendingMenu.kind = PendingMenu::Kind::Screen;
        m_pendingMenu.screen = Screen::Main;
        return;
    }
    const Touch touch = touchThisTick(registry);
    Dashboard::Touch input;
    input.pressed = touch.pressed;
    input.held = touch.held;
    input.released = touch.released;
    input.over = touch.over;
    input.at = touch.atView;
    input.wheelNotches = static_cast<double>(Input::TickScroll());
    const Dashboard::Outcome outcome =
        Dashboard::Tick(m_dashboardRules, m_dashboardBoard, m_dashboardState, MenuBoxPx(), m_menuClock.ms, input);
    if (outcome.pick) latch("achievement_pick");
    // ScoreDashboard::loop #780-786: the denial heard, and
    // displayLevelLockedNotification's card on the Notificator (planning step 143).
    if (outcome.denied) {
        latch("achievement_denied");
        if (m_notificatorReady && m_achievementRulesReady) {
            Notificator::Card card;
            card.background = m_achievementRules.locked.background;
            card.message = m_achievementRules.locked.message;
            card.durationMs = m_achievementRules.locked.durationMs;
            card.delayMs = m_achievementRules.locked.delayMs;
            m_notificator.Notify(std::move(card));
        }
    }
    if (outcome.back) {
        latch("menu_button");
        m_pendingMenu.kind = PendingMenu::Kind::Screen;
        m_pendingMenu.screen = Screen::Main;
        return;
    }
    if (outcome.start) {
        // getStartAchievementSoundName, and openState on the next tick.
        latch("level_button");
        m_pendingMenu.kind = PendingMenu::Kind::Achievement;
        m_pendingMenu.world = outcome.world;
        m_pendingMenu.level = outcome.level;
    }
}

void MagicPortalsLayer::openAchievement(entt::registry& registry, int world, int level) {
    // openState (bytes 312702..312880): the level, where there is one and it is
    // open; otherwise the chapter's grid, where the chapter is open.
    if (world >= 0 && level >= 0) {
        if (!Locking::LevelUnlocked(m_lockingRules, m_scores, m_chapters, world, level)) return;
        for (std::size_t i = 0; i < m_chapters.levels.size(); ++i) {
            const Chapters::Level& entry = m_chapters.levels[i];
            if (entry.world != world || entry.index != level) continue;
            unloadMenu(registry);
            m_screen = Screen::None;
            loadLevel(registry, static_cast<int>(i));
            return;
        }
        return;
    }
    if (world >= 0 && Locking::ChapterUnlocked(m_lockingRules, m_scores, m_chapters, world)) {
        m_menuWorld = world;
        m_menuPage = 0;
        openMenu(registry, Screen::Levels);
    }
}

bool MagicPortalsLayer::menuStateUp() const {
    switch (m_screen) {
    case Screen::Loading:
    case Screen::Main:
    case Screen::Worlds:
    case Screen::Levels:
    case Screen::Credits:
    case Screen::Achievements:
        return true;
    case Screen::None:
    case Screen::Finished:
    case Screen::Dead:
        return false;
    }
    return false;
}

const std::string& MagicPortalsLayer::menuPicture(const std::string& file) const {
    static const std::string none;
    const auto found = m_menuPictures.find(file);
    return found != m_menuPictures.end() ? found->second : none;
}

void MagicPortalsLayer::loadUiFont(const std::string& name) {
    if (name == m_hudRules.caption.font) {
        if (m_captionFontTried) return;
        m_captionFontTried = true;
        std::string why;
        if (!m_captionFont.Load(m_paths.original + "/data/" + name, why)) {
            SUPERSONIC_LOG_WARN("Magic Portals") << "no text in " << name << ": " << why << std::endl;
        }
        return;
    }
    if (m_uiFonts.find(name) != m_uiFonts.end()) return;
    std::string why;
    if (!m_uiFonts[name].Load(m_paths.original + "/data/" + name, why)) {
        SUPERSONIC_LOG_WARN("Magic Portals") << "no text in " << name << ": " << why << std::endl;
    }
}

Pause::Switches MagicPortalsLayer::MainMenuSwitches() const {
    Pause::Switches switches;
    switches.soundOn = m_soundOn;
    switches.musicOn = m_musicOn;
    switches.musicAddedMs = m_mainMusicAddedMs;
    switches.musicDismissedMs = m_mainMusicDismissedMs;
    return switches;
}

bool MagicPortalsLayer::PressMainMenu(entt::registry& registry, MainMenu::Button button) {
    if (m_screen != Screen::Main) return false;
    switch (button) {
    case MainMenu::Button::Play:
    case MainMenu::Button::Title:
        // MainMenuLayer::update's play_button and PortalMainMenu::loop's title:
        // createLevelSelectState, which is chapter select from the menu.
        latch("menu_button");
        openMenu(registry, Screen::Worlds);
        return true;
    case MainMenu::Button::Credits:
        // PortalMainMenu::loop: the CreditsScreen state (ui3 spec 3), with
        // getItemSelectButtonSoundName, which sounds.json maps with the level's own
        // buttons.
        latch("level_button");
        openMenu(registry, Screen::Credits);
        return true;
    case MainMenu::Button::Achievements:
        // And the ScoreDashboard state (ui3 spec 4), with the same noise.
        latch("level_button");
        openMenu(registry, Screen::Achievements);
        return true;
    case MainMenu::Button::Sound: {
        // GlobalSoundSwitch::manageSoundSwitch, and SoundPanelLayer's music switch
        // dismissed while the sound is off and added afresh when it is back: the
        // pause's arithmetic on this state's clock.
        const double now = m_menuClock.ms;
        const double dismissMs = m_mainMenuRules.layer.buttonDismissMs;
        const bool dismissing = m_mainMusicDismissedMs >= 0.0 && now - m_mainMusicDismissedMs < dismissMs;
        m_soundOn = !m_soundOn;
        if (!m_soundOn) {
            if (!dismissing && now >= m_mainMusicAddedMs) m_mainMusicDismissedMs = now;
        } else {
            m_mainMusicAddedMs = dismissing ? m_mainMusicDismissedMs + dismissMs : now;
        }
        // Silent: a UISwitch, which no setSound reaches (GlobalSoundSwitch
        // #23; planning step 142 - it played button.mp3 before).
        switchedSound();
        return true;
    }
    case MainMenu::Button::Music: {
        const bool shown = m_soundOn && m_menuClock.ms >= m_mainMusicAddedMs &&
                           !(m_mainMusicDismissedMs >= 0.0 &&
                             m_menuClock.ms - m_mainMusicDismissedMs < m_mainMenuRules.layer.buttonDismissMs);
        if (!shown) return false;
        m_musicOn = !m_musicOn;
        switchedMusic();
        return true;
    }
    }
    return false;
}

void MagicPortalsLayer::EmitMenu(entt::registry& registry) const {
    using Supersonic::ScreenOverlay;
    auto* const* slot = registry.ctx().find<ScreenOverlay*>();
    if (slot == nullptr || *slot == nullptr) return;
    ScreenOverlay& overlay = **slot;
    if (!m_hudReady || !menuStateUp()) return;
    const glm::dvec2 view = MenuBoxPx();
    if (view.x <= 0.0 || view.y <= 0.0) return;

    const auto add = [&overlay, &view](const Hud::Rect& rect, const std::string& image, const glm::vec4& colour,
                                       const glm::dvec2& uvMin = glm::dvec2(0.0),
                                       const glm::dvec2& uvMax = glm::dvec2(1.0)) {
        if (colour.a <= 0.0f) return;
        ScreenOverlay::Quad quad;
        quad.min = glm::vec2(rect.min / view);
        quad.max = glm::vec2(rect.Max() / view);
        quad.uvMin = glm::vec2(uvMin);
        quad.uvMax = glm::vec2(uvMax);
        quad.color = colour;
        quad.texture = image;
        quad.clampToEdge = true; // as makeSprite's pictures, and for its reason
        overlay.Add(std::move(quad));
    };
    const auto write = [&](const std::string& fontName, const std::string& words, const glm::dvec2& centre,
                           double unitsPerFontPx, double alpha) {
        const Supersonic::BitmapFont* font = uiFont(fontName);
        if (font == nullptr) return;
        const auto& pages = font->Pages();
        for (const Hud::Glyph& glyph : Hud::LayOutText(*font, words, centre, unitsPerFontPx)) {
            if (glyph.page < 0 || static_cast<std::size_t>(glyph.page) >= pages.size()) continue;
            add(glyph.rect, pages[static_cast<std::size_t>(glyph.page)],
                glm::vec4(1.0f, 1.0f, 1.0f, static_cast<float>(alpha)), glyph.uvOffset,
                glyph.uvOffset + glyph.uvScale);
        }
    };
    const Hud::Rect whole{glm::dvec2(0.0), view};
    const float black =
        static_cast<float>(MenuState::FadeAlphaByte(m_mainMenuRules.state, m_menuClock.wallMs)) / 255.0f;

    if (m_screen == Screen::Main) {
        // The background, then the layer's buttons in the order they were added,
        // each at its bounce, bob, press tint, blink and entrance (sim/MainMenu).
        for (const MainMenu::Piece& piece :
             MainMenu::Pieces(m_mainMenuRules, MainMenuSwitches(), view, m_menuClock.ms, m_layerClockMs,
                              m_menuTouch.mainHeld)) {
            const std::string& image = menuPicture(piece.file);
            if (image.empty()) continue;
            const float grey = static_cast<float>(piece.rgbByte) / 255.0f;
            add(piece.rect, image, glm::vec4(grey, grey, grey, static_cast<float>(piece.alphaByte) / 255.0f));
        }
        // The state's black, drawn after the layer manager: over all of it.
        add(whole, std::string(), glm::vec4(0.0f, 0.0f, 0.0f, black));
        return;
    }
    if (m_screen == Screen::Credits) {
        // The background, the back button (UILayer::draw), then the papyrus and the
        // strip (CreditsScreenLayer::draw), and the state's black over all of it.
        for (const Credits::Piece& piece :
             Credits::Pieces(m_creditsRules, view, m_menuClock.ms, m_credits, m_creditsTouch.backHeld)) {
            const std::string& image = menuPicture(piece.file);
            if (image.empty()) continue;
            const float grey = static_cast<float>(piece.rgbByte) / 255.0f;
            add(piece.rect, image, glm::vec4(grey, grey, grey, static_cast<float>(piece.alphaByte) / 255.0f));
        }
        add(whole, std::string(), glm::vec4(0.0f, 0.0f, 0.0f, black));
        return;
    }
    if (m_screen == Screen::Achievements) {
        // Every picture and word in ScoreDashboard::loop's order (sim/Dashboard.hpp),
        // then the state's black.
        for (const Dashboard::Piece& piece : Dashboard::Pieces(m_dashboardRules, m_dashboardBoard, m_dashboardState,
                                                               view, m_menuClock.ms, m_layerClockMs)) {
            const glm::vec4 colour(glm::vec3(piece.rgbBytes) / 255.0f, static_cast<float>(piece.alphaByte) / 255.0f);
            if (piece.kind == Dashboard::Kind::Sprite) {
                const std::string& image = menuPicture(piece.file);
                if (!image.empty()) add(piece.rect, image, colour);
                continue;
            }
            const Supersonic::BitmapFont* font = uiFont(piece.file);
            if (font == nullptr) continue;
            const auto& pages = font->Pages();
            // drawCenteredText centres the summed box; DrawText draws from its top-left.
            const std::vector<Hud::Glyph> glyphs =
                piece.centred ? Hud::LayOutText(*font, piece.words, piece.at, piece.unitsPerFontPx)
                              : Hud::LayOutTextFrom(*font, piece.words, piece.at, piece.unitsPerFontPx);
            for (const Hud::Glyph& glyph : glyphs) {
                if (glyph.page < 0 || static_cast<std::size_t>(glyph.page) >= pages.size()) continue;
                add(glyph.rect, pages[static_cast<std::size_t>(glyph.page)], colour, glyph.uvOffset,
                    glyph.uvOffset + glyph.uvScale);
            }
        }
        add(whole, std::string(), glm::vec4(0.0f, 0.0f, 0.0f, black));
        return;
    }
    if (m_screen == Screen::Loading) {
        // BaseState::loop draws the black first; LoadingScreen::loop then writes
        // the dots, while it is still loading, and the logo on every frame.
        add(whole, std::string(), glm::vec4(0.0f, 0.0f, 0.0f, black));
        const Loading::Rules& rules = m_loadingRules;
        const int frame = m_menuClock.ticks + 1;
        if (Loading::IsLoading(rules, frame)) {
            const glm::dvec2 at = rules.dots.centreOfScreen * view;
            write(rules.dots.font, Loading::DotsText(rules, Loading::LoadedBefore(rules, frame)), at,
                  rules.dots.unitsPerFontPx, 1.0);
            write(rules.dots.font, Loading::TrackText(rules), at, rules.dots.unitsPerFontPx,
                  static_cast<double>(rules.dots.trackAlphaByte) / 255.0);
        }
        if (const std::string& logo = menuPicture(rules.logo.sprite); !logo.empty()) {
            add(UiLayer::RectAt(rules.logo, UiLayer::Anchor(rules.logo, view)), logo, glm::vec4(1.0f));
        }
        return;
    }
    if (m_screen == Screen::Worlds || m_screen == Screen::Levels) {
        // Every picture and word in the original's order (sim/Selector.hpp), then a
        // popup over it, then the state's black. The layer's own entrance and bounces
        // run on the PageManager's clock, which a popup over it stands still.
        for (const Selector::Piece& piece :
             Selector::Pieces(m_selectorRules, m_selectorBoard, m_selectorState, view, SelectorLayerMs(),
                              m_layerClockMs, m_selectorHeld.control, m_selectorHeld.item)) {
            const glm::vec4 colour(glm::vec3(piece.rgbBytes) / 255.0f, static_cast<float>(piece.alphaByte) / 255.0f);
            if (piece.kind == Selector::Kind::Sprite) {
                const std::string& image = menuPicture(piece.file);
                if (!image.empty()) add(piece.rect, image, colour, piece.uvMin, piece.uvMax);
                continue;
            }
            const Supersonic::BitmapFont* font = uiFont(piece.file);
            if (font == nullptr) continue;
            const auto& pages = font->Pages();
            const std::vector<Hud::Glyph> glyphs =
                piece.centred ? Hud::LayOutText(*font, piece.words, piece.at, piece.unitsPerFontPx)
                              : Hud::LayOutTextFrom(*font, piece.words, piece.at, piece.unitsPerFontPx);
            for (const Hud::Glyph& glyph : glyphs) {
                if (glyph.page < 0 || static_cast<std::size_t>(glyph.page) >= pages.size()) continue;
                add(glyph.rect, pages[static_cast<std::size_t>(glyph.page)], colour, glyph.uvOffset,
                    glyph.uvOffset + glyph.uvScale);
            }
        }
        // UILayerManager::draw draws the popup's layer after the PageManager's, and
        // so over the tiles and the page counter; BaseState's black over it all.
        if (m_menuPopup.open && m_menuPopup.cls != nullptr) {
            emitPopup(overlay, view, *m_menuPopup.cls, m_menuPopup.state);
        }
    }
    add(whole, std::string(), glm::vec4(0.0f, 0.0f, 0.0f, black));
}

// ---- the camera -------------------------------------------------------------

float MagicPortalsLayer::viewportAspect(const entt::registry& registry) const {
    const auto* viewport = registry.ctx().find<Supersonic::ViewportInfo>();
    if (viewport == nullptr) return kDefaultAspect;
    const glm::vec2 size = viewport->Size();
    return size.x > 0.0f && size.y > 0.0f ? size.x / size.y : kDefaultAspect;
}

void MagicPortalsLayer::buildCamera(entt::registry& registry) {
    using namespace Supersonic;
    m_camera = registry.create();
    registry.emplace<TagComponent>(m_camera, "Magic Portals Camera");
    auto& camera = registry.emplace<CameraComponent>(m_camera);
    camera.projection = CameraComponent::Projection::Orthographic;
    camera.nearPlane = 0.1f;
    camera.farPlane = 100.0f;
    camera.isPrimary = true;
    camera.flyControlsEnabled = false;
    // Looking down -z at the plane the level lies in, with the engine's +y up
    // the screen. Where it looks is the tick's (placeCamera).
    camera.yaw = -90.0f;
    camera.pitch = 0.0f;
    camera.updateCameraVectors();
    camera.aspect = kDefaultAspect;
    registry.emplace<TransformComponent>(m_camera);
    // Moved on the tick and drawn at the tick's own place (drawPicturesAtTick), or between the
    // last two ticks where the display outruns the tick, as HUSK's is.
    registry.emplace<InterpolatedCameraComponent>(m_camera);

    m_light = registry.create();
    registry.emplace<TagComponent>(m_light, "Magic Portals Light");
    auto& light = registry.emplace<LightComponent>(m_light);
    light.type = 0;
    light.direction = glm::vec3(0.35f, 0.6f, 1.0f);
    light.intensity = 1.3f;
}

void MagicPortalsLayer::placeCamera(entt::registry& registry) {
    if (m_camera == entt::null || !registry.valid(m_camera)) return;
    auto& camera = registry.get<Supersonic::CameraComponent>(m_camera);
    const glm::vec3 centre = Units::ToWorld(m_follow.centrePx.x, m_follow.centrePx.y);
    camera.position = glm::vec3(centre.x, centre.y, kCameraDistance);
    camera.aspect = m_aspect;
    camera.orthoHeight = Units::ToMetres(m_viewHeightPx);
    registry.get<Supersonic::TransformComponent>(m_camera).position = camera.position;
    // Written whole, so whatever shake a frame left on it is gone with it.
    m_quakeDrawn.onCamera = false;
}

bool MagicPortalsLayer::ScreenToLevelPx(const entt::registry& registry, const glm::vec2& screenPoint,
                                        glm::dvec2& outPx) const {
    const auto* viewport = registry.ctx().find<Supersonic::ViewportInfo>();
    if (viewport == nullptr) return false;
    return ViewportToLevelPx(registry, viewport->ToLocal(screenPoint), outPx);
}

bool MagicPortalsLayer::ViewportToLevelPx(const entt::registry& registry, const glm::vec2& localPoint,
                                          glm::dvec2& outPx) const {
    const auto* viewport = registry.ctx().find<Supersonic::ViewportInfo>();
    // Not gated on a level being loaded: the menu is drawn in this same plane
    // and clicked through this same mapping.
    if (viewport == nullptr || m_camera == entt::null || !registry.valid(m_camera)) return false;
    const glm::vec2 size = viewport->Size();
    if (size.x <= 0.0f || size.y <= 0.0f) return false;
    // The camera as THIS viewport would draw it, whatever shape it last had -
    // and AT REST: a frame's shake is the picture's, and the original maps every
    // portal tap before its generator moves the camera (earthquake.json _about).
    Supersonic::CameraComponent camera = registry.get<Supersonic::CameraComponent>(m_camera);
    if (m_quakeDrawn.onCamera) camera.position.y = m_quakeDrawn.cameraRestY;
    camera.aspect = size.x / size.y;
    const Supersonic::Ray ray = Supersonic::Raycast::ScreenPointToRay(localPoint, size, camera);
    // The level is the plane z = 0. The ray is met there, rather than trusting
    // its origin to lie on it.
    if (std::fabs(ray.direction.z) < 1e-6f) return false;
    const float t = -ray.origin.z / ray.direction.z;
    outPx = Units::ToPixels(ray.origin + ray.direction * t);
    return true;
}

// ---- the picture ------------------------------------------------------------

entt::entity MagicPortalsLayer::makeBox(entt::registry& registry, const char* tag, const glm::vec3& centre,
                                        const glm::vec3& size, const glm::vec3& colour) {
    using namespace Supersonic;
    const entt::entity e = registry.create();
    registry.emplace<TagComponent>(e, tag);
    auto& transform = registry.emplace<TransformComponent>(e);
    transform.position = centre;
    transform.scale = size; // the box primitive is one unit on a side
    // "Box", not "Cube": the cube is the engine's rainbow debug primitive.
    registry.emplace<MeshComponent>(e).primitiveType = "Box";
    auto& material = registry.emplace<MaterialComponent>(e);
    material.albedoColor = glm::vec4(colour, 1.0f);
    material.roughness = 0.85f;
    material.metallic = 0.0f;
    registry.emplace<RenderableComponent>(e);
    return e;
}

void MagicPortalsLayer::placeBox(entt::registry& registry, entt::entity box, const glm::dvec2& centrePx,
                                 const glm::dvec2& sizePx, float z, float depth, float rotation) const {
    auto& transform = registry.get<Supersonic::TransformComponent>(box);
    const glm::vec3 centre = Units::ToWorld(centrePx.x, centrePx.y);
    transform.position = glm::vec3(centre.x, centre.y, z);
    transform.scale = glm::vec3(Units::ToMetres(sizePx.x), Units::ToMetres(sizePx.y), depth);
    transform.rotation = glm::vec3(0.0f, 0.0f, rotation);
}

void MagicPortalsLayer::buildDrawables(entt::registry& registry) {
    using namespace Supersonic;
    for (const auto& [name, entity] : m_level.built.entities) {
        const Tscn::Node* node = m_data.scene.FindNode(name);
        if (node == nullptr || IsTrigger(registry, entity)) continue;
        Drawn drawn;
        if (!LevelBuilder::ShapeBoundsPx(m_data.scene, *node, drawn.offsetPx, drawn.sizePx)) continue;
        drawn.body = entity;
        glm::vec3 colour = kStaticColour;
        drawn.depth = 0.6f;
        if (const auto* rigid = registry.try_get<RigidBodyComponent>(entity)) {
            drawn.depth = 0.4f;
            if (rigid->isKinematic) {
                colour = kDoorColour;
            } else {
                const bool travels = std::find(m_level.portals.travellers.begin(), m_level.portals.travellers.end(),
                                               entity) != m_level.portals.travellers.end();
                colour = travels ? kCrateColour : kFixedCrateColour;
            }
        }
        // What a stone breaks, and the stones, told apart from the rest.
        const Demolish::State& demolish = m_level.demolish;
        if (std::any_of(demolish.breakables.begin(), demolish.breakables.end(),
                        [entity](const Demolish::Breakable& b) { return b.body == entity; })) {
            colour = kBreakableColour;
        } else if (std::any_of(demolish.stones.begin(), demolish.stones.end(),
                               [entity](const Demolish::Stone& s) { return s.body == entity; })) {
            colour = kStoneColour;
        }
        drawn.box = makeBox(registry, "Magic Portals Body", glm::vec3(0.0f), glm::vec3(1.0f), colour);
        // Bodies move on the tick and are drawn at its place (drawPicturesAtTick).
        registry.emplace<InterpolatedTransformComponent>(drawn.box);
        m_bodies.push_back(drawn);
    }

    m_player = makeBox(registry, "Magic Portals Player", glm::vec3(0.0f), glm::vec3(1.0f), kPlayerColour);
    registry.emplace<InterpolatedTransformComponent>(m_player);

    for (const Puzzle::Button& button : m_level.channels.buttons) {
        (void)button;
        m_buttons.push_back(makeBox(registry, "Magic Portals Button", glm::vec3(0.0f), glm::vec3(1.0f), kButtonUpColour));
    }
    for (const Goals::Crystal& crystal : m_level.goals.crystals) {
        (void)crystal;
        m_crystals.push_back(makeBox(registry, "Magic Portals Crystal", glm::vec3(0.0f), glm::vec3(1.0f), kCrystalColour));
    }
    m_exit = makeBox(registry, "Magic Portals Exit", glm::vec3(0.0f), glm::vec3(1.0f), kExitColour);
    for (const Portals::Static& portal : m_level.portals.statics) {
        m_statics.push_back(makeBox(registry, "Magic Portals Static Portal", glm::vec3(0.0f), glm::vec3(1.0f),
                                    portal.colour == "red" ? kStaticRedColour : kStaticBlueColour));
    }
    for (std::size_t i = 0; i < m_level.portals.zones.size(); ++i) {
        m_zones.push_back(makeBox(registry, "Magic Portals No-Portal Zone", glm::vec3(0.0f), glm::vec3(1.0f), kZoneColour));
    }
    // An anti-portal agent is no zone (step 126); its box is the square round the
    // circle a shot dies in, in the zones' colour.
    for (std::size_t i = 0; i < m_level.portals.agents.size(); ++i) {
        m_agents.push_back(makeBox(registry, "Magic Portals Anti-Portal Agent", glm::vec3(0.0f), glm::vec3(1.0f),
                                   kZoneColour));
    }
    for (std::size_t i = 0; i < m_level.hazards.hazards.size(); ++i) {
        m_hazards.push_back(makeBox(registry, "Magic Portals Hazard", glm::vec3(0.0f), glm::vec3(1.0f), kHazardColour));
    }
    buildSprites(registry);

    // The minions' sheet: minion.png's tier file, hd/minion.png, 310 x 434 texels at
    // density 2, cut 4 x 4 as SetupSpriteRects cuts it - frames of 38 x 54 units
    // (76 x 108 texels) that cover 152 of its 155 units across and 216 of 217 down
    // (00_order E1). Their quads come with the first sync, an escort's when it is
    // called; a sheet that cannot be read or cut draws none, and says so once.
    m_minions.clear();
    m_minionSheet.clear();
    m_minionCut = Tiers::Cut{};
    // And 2-32's knight, dragon_knight.ent on the same sheet (cutscenes.json).
    if (!m_level.minions.minions.empty() || m_level.ghost.present || m_knight.placed) {
        const Art::Minion& art = m_artRules.minion;
        const Sprites::Image& sheet = drawnImage(originalImage(art.sprite));
        std::string why;
        if (sheet.units.x > 0 && sheet.units.y > 0 &&
            Tiers::FrameCut(sheet.texels, sheet.density, art.columns, art.rows, m_minionCut, why)) {
            m_minionSheet = sheet.path;
        } else if (m_artReady) {
            SUPERSONIC_LOG_WARN("Magic Portals")
                << "the minions are not drawn: " << sheet.path << " could not be cut " << why << std::endl;
        }
    }

    // CHAPTER 3'S BOSS (art.json ghost): ghost.png's tier file, hd/ghost.png, 973 x
    // 256 at density 2, cut 4 x 1 as SetupSpriteRects cuts it - frames of 121 x 128
    // units. Its quad comes when the black knight's death adds it (syncGhost). Its
    // depth: the knight's raw 2 over the factor, less the one level unit
    // scale(-1) puts it behind (ghost.json spawn; 00_order 8.13 K13).
    m_ghost = GhostQuad{};
    m_ghostAppearQueued = false;
    if (m_level.ghost.present) {
        const Art::Ghost& art = m_artRules.ghost;
        const Sprites::Image& sheet = drawnImage(originalImage(art.sprite));
        std::string why;
        if (sheet.units.x > 0 && sheet.units.y > 0 &&
            Tiers::FrameCut(sheet.texels, sheet.density, art.columns, art.rows, m_ghost.cut, why)) {
            m_ghost.sheet = sheet.path;
        } else if (m_artReady) {
            SUPERSONIC_LOG_WARN("Magic Portals")
                << "the ghost is not drawn: " << sheet.path << " could not be cut " << why << std::endl;
        }
        m_ghost.lookZ = m_artRules.LevelDepth(m_artRules.minion.z) + m_level.ghost.rules.spawnZOffsetUnits;
        m_ghost.z = slotAfter(m_ghost.lookZ, Slot::Ghost);
    }

    // The player, as dark_mage.ent draws it, standing on the idle column of the
    // way GameCharacter faces it when it is made (Art.hpp): each level starts
    // there, and its walk's column and time start from nothing. On a no_gravity
    // level in the suit MainCharacter's constructor swaps in, with its timer and
    // hover not yet moved.
    const Art::Character& mage = m_artRules.character;
    // Its sheet's tier file (drawnImage): hd/magic_portals_hd.png, or the suit's
    // hd/dark_mage_in_space.png, each 320 x 448 at density 2, cut 4 x 4 into the
    // 40 x 56 frames the 1x file's even split gave (playerCellPx).
    const Sprites::Image& sheet = drawnImage(originalImage(Art::PlayerSheet(mage, m_level.noGravity)));
    m_facingRight = mage.initialDirection == mage.rightRow;
    m_direction = 0.0f;
    m_pushing = false; // SideScrollerCharacter's constructor
    m_noGravityPlayer = Art::NoGravityPlayer{};
    m_noGravityColumn = mage.idleColumn;
    if (m_artReady && playerCellPx(m_level.noGravity) != glm::dvec2(0.0)) {
        m_playerQuad = makeSprite(registry, "Magic Portals Player Sprite", sheet.path, mage.additive);
        registry.emplace<InterpolatedTransformComponent>(m_playerQuad);
        auto& animation = registry.emplace<SpriteAnimationComponent>(m_playerQuad);
        animation.columns = static_cast<uint32_t>(mage.columns);
        animation.rows = static_cast<uint32_t>(mage.rows);
        animation.framesPerSecond = static_cast<float>(mage.framesPerSecond);
        animation.firstFrame = static_cast<uint32_t>(mage.initialDirection * mage.columns + mage.idleColumn);
        animation.frameCount = 1;
        animation.playing = false;
    }

    // AND THE ONE ENTITY THE CHARACTER'S OWN CONSTRUCTOR ADDS on every level, last
    // of all: the eyes read the cell the player quad was just given, so they must
    // be added after it and not with the level's own emitters.
    buildEyes(registry);
}

// ---- the level's art ----------------------------------------------------------

entt::entity MagicPortalsLayer::makeSprite(entt::registry& registry, const char* tag, const std::string& texture,
                                           bool additive) {
    using namespace Supersonic;
    const entt::entity e = registry.create();
    registry.emplace<TagComponent>(e, tag);
    registry.emplace<TransformComponent>(e);
    registry.emplace<MeshComponent>(e).primitiveType = "Quad";
    auto& material = registry.emplace<MaterialComponent>(e);
    // Unlit, as the Godot project draws its canvas: the engine's PBR lights never reach
    // it. What lights a level sprite is its 2D record, written by tint.
    material.unlit = true;
    material.transparent = true;
    material.blend = additive ? MaterialComponent::BlendMode::Additive : MaterialComponent::BlendMode::Alpha;
    material.albedoTexturePath = texture;
    // Every map read clamped to its edges, as the original reads every texture it
    // loads: GLES2Texture::LoadTexture creates it through SOIL without
    // SOIL_FLAG_TEXTURE_REPEATS, so SOIL sets GL_CLAMP_TO_EDGE (GLES2Texture.cpp:306,
    // soil/SOIL.c:1341-1360). Under the engine's repeat, a quad's edge takes half a
    // texel of the picture's opposite edge: 2-32's claw drew its arm as a line down
    // its right edge, 2-01's ramp a dark line along its top. The normal map and the
    // lightmap are this material's too, and the original clamps them alike.
    material.clampToEdge = true;
    registry.emplace<RenderableComponent>(e).castsShadow = false;
    return e;
}

void MagicPortalsLayer::placeSprite(entt::registry& registry, entt::entity quad, const glm::dvec2& centrePx,
                                    const glm::dvec2& sizePx, float z, float rotation) const {
    auto& transform = registry.get<Supersonic::TransformComponent>(quad);
    const glm::vec3 centre = Units::ToWorld(centrePx.x, centrePx.y);
    transform.position = glm::vec3(centre.x, centre.y, z);
    // The quad primitive is one unit on a side, facing the camera.
    transform.scale = glm::vec3(Units::ToMetres(sizePx.x), Units::ToMetres(sizePx.y), 1.0f);
    transform.rotation = glm::vec3(0.0f, 0.0f, rotation);
}

void MagicPortalsLayer::placeSkyCover(entt::registry& registry, const DrawnSprite& drawn, const glm::dvec2& centrePx,
                                      const glm::dvec2& sizePx, float rotation) const {
    using Supersonic::MaterialComponent;
    if (drawn.cover == entt::null || !registry.valid(drawn.cover) || !(sizePx.x > 0.0)) return;
    // Across, in the view's own pixels as the sky was just placed in them. A turned
    // sky - none is: StaticSky sets no angle, and no level turns its sky - keeps its
    // own span, and its cover stays wholly behind it.
    const glm::dvec2 view = ViewPx();
    const auto* viewport = registry.ctx().find<Supersonic::ViewportInfo>();
    const double framePx = viewport != nullptr && viewport->Size().x > 0.0f ? static_cast<double>(viewport->Size().x)
                                                                            : kDefaultFrameWidthPx;
    const double skyLowPx = centrePx.x - sizePx.x * 0.5;
    const double skyHighPx = centrePx.x + sizePx.x * 0.5;
    Sky::Span span{skyLowPx, skyHighPx};
    if (rotation == 0.0f) {
        span = Sky::PixelCover(skyLowPx, skyHighPx, m_follow.centrePx.x - view.x * 0.5, view.x, framePx);
    }
    const double widthPx = span.highPx - span.lowPx;
    placeSprite(registry, drawn.cover, glm::dvec2((span.lowPx + span.highPx) * 0.5, centrePx.y),
                glm::dvec2(widthPx, sizePx.y), slotOver(drawn.z, Slot::SkyCover), rotation);
    // Down, the sky's own rows: it is one view tall and on the frame's edges. And the
    // uv the stretch across asks for, so every texel lands where the sky draws it - u
    // runs 0 to 1 across a quad, left to right (ModelLoader::GenerateQuad). Every
    // pixel centre the cover shades is one the sky covers, so it shades the sky's own
    // colour there: the stretch adds samples, never a pixel the sky does not draw.
    auto& material = registry.get<MaterialComponent>(drawn.cover);
    material.uvScale = glm::vec2(static_cast<float>(widthPx / sizePx.x), 1.0f);
    material.uvOffset = glm::vec2(static_cast<float>((span.lowPx - skyLowPx) / sizePx.x), 0.0f);
}

// THE STRIPS' POOL, made before any other entity of the layer. The renderer breaks a tie between two
// draws of one depth and key by the order the registry stores them in (RenderSystem's `gathered`), and
// a level has such ties: the particles of one system share a depth and a key, and the glow they add
// is summed in half floats, so the order they are drawn in moves a pixel by a step. EnTT keeps each
// store packed: destroying an entity moves the LAST one into its place. A strip made on demand in the
// middle of a run is the last one of every store, so the next particle destroyed would be replaced
// by it and the others would keep their old places: the order of the particles would not be the
// order they were drawn in without strips (the first sweep of step 165 moved 133 px on five levels
// so). Made first, the strips stand in front of every other entity in every store, never the last
// while anything else is, and what is made and destroyed after them moves exactly as it moved
// before. Hidden, they are not gathered, so they cost the frame nothing.
void MagicPortalsLayer::buildEdgeCoverPool(entt::registry& registry) {
    if (!m_edgeRules.enabled) return;
    m_edgeStrips.assign(m_edgeRules.poolSize, entt::null);
    for (entt::entity& strip : m_edgeStrips) {
        strip = makeSprite(registry, kEdgeCoverTag, std::string(), false);
        registry.get<Supersonic::RenderableComponent>(strip).isVisible = false;
    }
}

void MagicPortalsLayer::hideEdgeCovers(entt::registry& registry) {
    for (const entt::entity strip : m_edgeStrips) {
        if (strip != entt::null && registry.valid(strip)) registry.get<Supersonic::RenderableComponent>(strip).isVisible = false;
    }
    m_edgeReports.clear();
}

// A HIDDEN STRIP IS NOT GATHERED BUT IT IS STILL AN ENTITY WITH A MATERIAL, and its material is
// the copy of the last picture it drew: the albedo, normal, orm, overlay and gloss paths of
// that picture, a lightmap among them. A level's unload invalidates its lightmaps' paths
// (releaseLightmaps), and the renderer re-resolves every entity whose texture generation moved,
// hidden or not, so a hidden strip that still names a lightmap loads it again and holds a material
// set for it: the next level then draws the last one's lightmaps, and the unload that follows
// gives back fewer sets than it was told (the level walk's 259 failures on the first build of
// step 165). So the strips' looks are cleared when a level goes. The ENTITIES stay: the pool is
// first in the registry's stores for a reason (buildEdgeCoverPool), and a strip is rewritten, whole,
// from its picture the next frame it is used.
void MagicPortalsLayer::clearEdgeCoverLooks(entt::registry& registry) {
    for (const entt::entity strip : m_edgeStrips) {
        if (strip == entt::null || !registry.valid(strip)) continue;
        if (auto* material = registry.try_get<Supersonic::MaterialComponent>(strip)) {
            *material = Supersonic::MaterialComponent{};
        }
    }
}

// What the last frame's cull hid is shown again before anything else of the next frame runs
// (the first line of OnFixedUpdate and of OnUpdate, and the level's unload): the game's own code
// writes and reads a picture's visibility - a pooled particle is put away and brought back,
// reportSprites says what is on screen - and every one of them must see the state the game
// left, never the cull's. A quad the game has destroyed since is simply not there.
void MagicPortalsLayer::restoreEdgeCulls(entt::registry& registry) {
    for (const entt::entity quad : m_edgeCulled) {
        if (quad == entt::null || !registry.valid(quad)) continue;
        if (auto* renderable = registry.try_get<Supersonic::RenderableComponent>(quad)) renderable->isVisible = true;
    }
    m_edgeCulled.clear();
}

void MagicPortalsLayer::unloadEdgeCovers(entt::registry& registry) {
    for (entt::entity& strip : m_edgeStrips) {
        if (strip != entt::null && registry.valid(strip)) registry.destroy(strip);
        strip = entt::null;
    }
    m_edgeStrips.clear();
    m_edgeReports.clear();
}

// THE PIXEL-CENTRE RULE AT THE FRAME'S EDGE (edge_cover.json; sim/EdgeCover.hpp). It runs
// last in OnUpdate, after the engine's interpolation has written the pictures and the
// camera and shakeView has moved the camera, so the rectangles it reads are the ones the
// renderer is about to draw - at the tick's own place or between two ticks, shaken or not,
// in whatever window - and each frame decides afresh which pictures are at the frame's edge.
//
// A strip is the picture's own material over the gap its edge leaves, with the uv map the
// picture has carried on across it. Written after every tint, so whatever a picture was
// given this frame - its colour, light, lightmap, fade or cell - the strip has too.
//
// THE COMPLEMENTARY HALF (edge_cover.json complement): a picture whose part on the frame lies
// wholly in one outer pixel and covers that pixel's centre nowhere is left out of the frame for
// this frame, its quad hidden (the original draws none of it where the MSAA would draw the samples
// it covers), and the next frame's first line of code shows it again (restoreEdgeCulls). The
// strips of a picture's INWARD edge are the same strips, found by the same rule (EdgeCover::Snap).
void MagicPortalsLayer::syncEdgeCovers(entt::registry& registry) {
    using namespace Supersonic;
    restoreEdgeCulls(registry);
    hideEdgeCovers(registry);
    if (!m_edgeRules.enabled || !levelShown() || m_camera == entt::null || !registry.valid(m_camera)) return;

    // The frame, and where the quads land on it: the orthographic camera shows
    // orthoHeight metres down the frame and that times the aspect across it, centred on the
    // camera, y up the world and down the frame.
    const auto* viewport = registry.ctx().find<ViewportInfo>();
    const glm::vec2 size = viewport != nullptr ? viewport->Size() : glm::vec2(0.0f);
    const double frameW = size.x > 0.0f ? static_cast<double>(size.x) : kDefaultFrameWidthPx;
    const double frameH = size.y > 0.0f ? static_cast<double>(size.y) : kDefaultFrameHeightPx;
    const auto& camera = registry.get<CameraComponent>(m_camera);
    const double halfH = static_cast<double>(camera.orthoHeight) * 0.5;
    const double halfW = halfH * static_cast<double>(viewportAspect(registry));
    if (!(halfH > 0.0) || !(halfW > 0.0)) return;
    const double perMetreX = frameW / (2.0 * halfW);
    const double perMetreY = frameH / (2.0 * halfH);
    const double leftM = static_cast<double>(camera.position.x) - halfW;
    const double topM = static_cast<double>(camera.position.y) + halfH;

    // A sky has its cover already (sky.json pixel_cover), which is the whole of its remedy.
    std::vector<entt::entity> covered;
    for (const DrawnSprite& drawn : m_sprites) {
        if (drawn.cover != entt::null && drawn.quad != entt::null) covered.push_back(drawn.quad);
    }

    // Found first and built after: a strip is an entity with the components this walks.
    struct Wanted {
        entt::entity picture{entt::null};
        EdgeCover::Rect rect;
        EdgeCover::Strips strips;
        glm::dvec2 uvScale{1.0};
        glm::dvec2 uvOffset{0.0};
    };
    // What the log is to say of a picture that is culled or kept: the cull (kEdgeCullCode) or a
    // side (kEdgeSliverCode + side).
    struct Note {
        entt::entity picture{entt::null};
        int code{0};
        EdgeCover::Rect rect;
    };
    std::vector<Wanted> wanted;
    std::vector<entt::entity> culls;
    std::vector<Note> notes;
    for (auto [entity, mesh, transform, material, renderable] :
         registry.view<MeshComponent, TransformComponent, MaterialComponent, RenderableComponent>().each()) {
        if (!renderable.isVisible || mesh.primitiveType != "Quad") continue;
        // An axis-aligned picture: a turned one has no rows and columns of the frame's, and
        // a transparent one shows nothing.
        if (std::fabs(transform.rotation.x) > m_edgeRules.turnRad ||
            std::fabs(transform.rotation.y) > m_edgeRules.turnRad ||
            std::fabs(transform.rotation.z) > m_edgeRules.turnRad) {
            continue;
        }
        if (!(material.albedoColor.a > 0.0f) || !(std::fabs(transform.scale.x) > 0.0f) ||
            !(std::fabs(transform.scale.y) > 0.0f)) {
            continue;
        }
        // The right way up, with a uv the strip can carry on: a mirrored picture runs its uv
        // backwards and a sheet with a turned uv has no rows and columns of its own. Such a
        // picture is still a rectangle on the frame, which the cull may leave out.
        const bool plain = transform.scale.x > 0.0f && transform.scale.y > 0.0f && material.uvRotation == 0.0f;
        const double halfWidthM = static_cast<double>(std::fabs(transform.scale.x)) * 0.5;
        const double halfHeightM = static_cast<double>(std::fabs(transform.scale.y)) * 0.5;
        const double x = static_cast<double>(transform.position.x);
        const double y = static_cast<double>(transform.position.y);
        EdgeCover::Rect rect;
        rect.x = EdgeCover::Interval{(x - halfWidthM - leftM) * perMetreX, (x + halfWidthM - leftM) * perMetreX};
        rect.y = EdgeCover::Interval{(topM - (y + halfHeightM)) * perMetreY, (topM - (y - halfHeightM)) * perMetreY};
        if (!std::isfinite(rect.x.lo) || !std::isfinite(rect.x.hi) || !std::isfinite(rect.y.lo) ||
            !std::isfinite(rect.y.hi)) {
            continue;
        }
        const bool left = EdgeCover::Culled(m_edgeRules, rect, frameW, frameH);
        const EdgeCover::Strips strips = plain && !left ? EdgeCover::StripsOf(m_edgeRules, rect, frameW, frameH)
                                                        : EdgeCover::Strips{};
        const EdgeCover::Slivers slivers =
            left ? EdgeCover::Slivers{} : EdgeCover::SliversOf(m_edgeRules, rect, frameW, frameH);
        if (!left && strips.count == 0 && !slivers.Any()) continue;
        if (const auto* tag = registry.try_get<TagComponent>(entity);
            tag != nullptr && (tag->tag == kEdgeCoverTag || tag->tag == kSkyCoverTag)) {
            continue;
        }
        if (std::find(covered.begin(), covered.end(), entity) != covered.end()) continue;
        if (left) {
            culls.push_back(entity);
            notes.push_back(Note{entity, kEdgeCullCode, rect});
            continue;
        }
        for (int side = 0; side < 4; ++side) {
            if (slivers.side[static_cast<std::size_t>(side)]) notes.push_back(Note{entity, kEdgeSliverCode + side, rect});
        }
        if (strips.count == 0) continue;
        Wanted found;
        found.picture = entity;
        found.rect = rect;
        found.strips = strips;
        found.uvScale = glm::dvec2(material.uvScale);
        found.uvOffset = glm::dvec2(material.uvOffset);
        // A flipbook's cell is written by the engine after the layers update, from the
        // component's frame as the tick left it: the same cell, by the same arithmetic.
        if (const auto* animation = registry.try_get<SpriteAnimationComponent>(entity)) {
            const std::uint32_t count = animation->resolvedFrameCount();
            if (count != 0) {
                glm::vec2 cellScale(1.0f);
                glm::vec2 cellOffset(0.0f);
                SpriteAnimationSystem::CellTransform(animation->columns, animation->rows,
                                                     animation->firstFrame + (animation->frame % count), cellScale,
                                                     cellOffset);
                found.uvScale = glm::dvec2(cellScale);
                found.uvOffset = glm::dvec2(cellOffset);
            }
        }
        wanted.push_back(found);
    }

    // The pictures the rule leaves out of the frame: hidden here, shown again by restoreEdgeCulls.
    for (const entt::entity quad : culls) {
        registry.get<RenderableComponent>(quad).isVisible = false;
        m_edgeCulled.push_back(quad);
    }

    std::size_t used = 0;
    for (const Wanted& found : wanted) {
        const MaterialComponent look = registry.get<MaterialComponent>(found.picture);
        const float z = registry.get<TransformComponent>(found.picture).position.z;
        const auto pictureSortKey = registry.get<RenderableComponent>(found.picture).sortKey;
        const std::string tag =
            registry.all_of<TagComponent>(found.picture) ? registry.get<TagComponent>(found.picture).tag : std::string();
        for (std::size_t i = 0; i < found.strips.count; ++i) {
            const EdgeCover::Strip& strip = found.strips.strip[i];
            if (used == m_edgeStrips.size()) m_edgeStrips.push_back(entt::null);
            const std::size_t at = used++;
            if (m_edgeStrips[at] == entt::null || !registry.valid(m_edgeStrips[at])) {
                m_edgeStrips[at] = makeSprite(registry, kEdgeCoverTag, look.albedoTexturePath, false);
            }
            const entt::entity quad = m_edgeStrips[at];
            // Where it lies, back from the frame's pixels to the world's metres.
            auto& transform = registry.get<TransformComponent>(quad);
            transform.position =
                glm::vec3(static_cast<float>(leftM + (strip.rect.x.lo + strip.rect.x.hi) * 0.5 / perMetreX),
                          static_cast<float>(topM - (strip.rect.y.lo + strip.rect.y.hi) * 0.5 / perMetreY), z);
            transform.scale = glm::vec3(static_cast<float>((strip.rect.x.hi - strip.rect.x.lo) / perMetreX),
                                        static_cast<float>((strip.rect.y.hi - strip.rect.y.lo) / perMetreY), 1.0f);
            transform.rotation = glm::vec3(0.0f);
            // The picture's own material - its colour, light, lightmap, fade, blend - with the
            // uv map carried on across the gap.
            auto& material = registry.get<MaterialComponent>(quad);
            material = look;
            const EdgeCover::Uv uv = EdgeCover::UvOf(found.rect, strip.rect, found.uvScale, found.uvOffset);
            material.uvScale = glm::vec2(uv.scale);
            material.uvOffset = glm::vec2(uv.offset);
            material.uvRotation = 0.0f;
            auto& renderable = registry.get<RenderableComponent>(quad);
            renderable.sortKey = pictureSortKey;
            renderable.isVisible = true;
            EdgeCoverReport report;
            report.strip = quad;
            report.picture = found.picture;
            report.tag = tag;
            report.texture = look.albedoTexturePath;
            report.side = strip.side;
            report.pictureRect = found.rect;
            report.stripRect = strip.rect;
            m_edgeReports.push_back(std::move(report));
        }
    }

    // What is new this frame is said once, so the log names the picture of each strip, cull and kept
    // edge a capture drew.
    std::vector<std::pair<entt::entity, int>> now;
    now.reserve(m_edgeReports.size() + notes.size());
    const auto isNew = [this](const std::pair<entt::entity, int>& key) {
        // log_every_frame is a measuring switch: every one of them every frame, with no cap.
        if (m_edgeRules.logEveryFrame) return true;
        return m_edgeLogged < kEdgeCoverLogCap &&
               std::find(m_edgeSeen.begin(), m_edgeSeen.end(), key) == m_edgeSeen.end();
    };
    static constexpr const char* sides[] = {"left", "right", "top", "bottom"};
    for (const EdgeCoverReport& report : m_edgeReports) {
        const std::pair<entt::entity, int> key(report.picture, static_cast<int>(report.side));
        now.push_back(key);
        if (!isNew(key)) continue;
        ++m_edgeLogged;
        SUPERSONIC_LOG_INFO("Magic Portals")
            << "edge strip " << sides[static_cast<int>(report.side)] << " on tick " << m_ticks << ": " << report.tag << " "
            << report.texture << " picture x " << report.pictureRect.x.lo << ".." << report.pictureRect.x.hi << " y "
            << report.pictureRect.y.lo << ".." << report.pictureRect.y.hi << " px, strip x " << report.stripRect.x.lo
            << ".." << report.stripRect.x.hi << " y " << report.stripRect.y.lo << ".." << report.stripRect.y.hi << std::endl;
    }
    for (const Note& note : notes) {
        const std::pair<entt::entity, int> key(note.picture, note.code);
        now.push_back(key);
        if (!isNew(key)) continue;
        ++m_edgeLogged;
        const auto* tag = registry.try_get<TagComponent>(note.picture);
        const auto* look = registry.try_get<MaterialComponent>(note.picture);
        SUPERSONIC_LOG_INFO("Magic Portals")
            << (note.code == kEdgeCullCode ? std::string("edge cull")
                                           : std::string("edge sliver ") + sides[note.code - kEdgeSliverCode])
            << " on tick " << m_ticks << ": " << (tag != nullptr ? tag->tag : std::string()) << " "
            << (look != nullptr ? look->albedoTexturePath : std::string()) << " picture x " << note.rect.x.lo << ".."
            << note.rect.x.hi << " y " << note.rect.y.lo << ".." << note.rect.y.hi << " px" << std::endl;
    }
    m_edgeSeen = std::move(now);
}

glm::dvec2 MagicPortalsLayer::imageSizePx(const std::string& path) {
    if (const auto known = m_imageSizes.find(path); known != m_imageSizes.end()) return known->second;
    int width = 0;
    int height = 0;
    std::string error;
    const glm::dvec2 size =
        Sprites::ImageSize(path, width, height, error) ? glm::dvec2(width, height) : glm::dvec2(0.0);
    m_imageSizes.emplace(path, size);
    return size;
}

std::string MagicPortalsLayer::originalImage(const std::string& sprite) const {
    return m_paths.original + "/entities/" + sprite;
}

const Sprites::Image& MagicPortalsLayer::drawnImage(const std::string& named) {
    // THE FILE THE ORIGINAL DRAWS, as for a level's art (step 66): every image the
    // original loads goes through AddFile and ChooseSpriteVersion, the pictures its
    // script adds and their particle bitmaps as much as the level's sprites
    // (ETHSpriteEntity.cpp:95-104, ETHParticleManager.cpp:107). The APK ships hd
    // files for the player's two sheets, the beholder, the rolling stone, a gravity
    // well's ring (white_ring.png) and two particle bitmaps no built effect names
    // yet; each is exactly twice its 1x file, so each is drawn at the same units and
    // only its sampling changes. The portal's halo, the shot, the spike, a timed
    // crystal's dial (timer.png), halo.bmp and the normal maps have no tier file and
    // come back as named, at density 1.
    if (const auto known = m_drawnImages.find(named); known != m_drawnImages.end()) return known->second;
    // One that cannot be read keeps units 0, and each caller draws nothing for it
    // (a particle system's says so by name, as before).
    Sprites::Image image;
    std::string error;
    if (!Sprites::Drawn(m_tierRules, named, image, error)) image.texels = image.units = glm::ivec2(0);
    return m_drawnImages.emplace(named, std::move(image)).first->second;
}

glm::dvec2 MagicPortalsLayer::frameUnits(const Sprites::Image& sheet, int columns, int rows) {
    Tiers::Cut cut;
    std::string error;
    if (sheet.units.x <= 0 || sheet.units.y <= 0 ||
        !Tiers::FrameCut(sheet.texels, sheet.density, columns, rows, cut, error)) {
        return glm::dvec2(0.0);
    }
    return glm::dvec2(cut.frameUnits);
}

glm::dvec2 MagicPortalsLayer::playerCellPx(bool noGravity) {
    // The sheet it was built with (buildDrawables), whose frame is cut from it.
    // Units, not texels: the push ray's reach is a share of this width (getSize,
    // the frame times the entity's scale), so the hd sheet (320 x 448 at density
    // 2) keeps its frame at int(texels / D) / 4 = 40 x 56, the 1x file's, and the
    // reach does not double.
    const Art::Character& mage = m_artRules.character;
    return frameUnits(drawnImage(originalImage(Art::PlayerSheet(mage, noGravity))), mage.columns, mage.rows);
}

void MagicPortalsLayer::buildSprites(entt::registry& registry) {
    using namespace Supersonic;
    std::vector<Sprites::Sprite> sprites;
    m_artError.clear();
    m_rankedSprites = 0;
    m_sign = NoPortalSign{};
    m_sky = Sky::Controller{};
    m_space = Sky::Space{};
    m_dragonSprite = -1;
    m_clawSprite = -1;
    m_dragonCut = Tiers::Cut{};
    m_doorSprite = -1;
    m_doorBgSprite = -1;
    m_doorHighlight = -1;
    m_bridgeSprite = -1;
    m_landingKeySprite = -1;
    // From the constant start angle Game::loop passes, not from the level's phase
    // stream: nothing here draws a phase (00_order K8), so no crystal's or key's
    // moves because a door bobs.
    m_doorBob = Motion::Start(m_motionRules.door, m_motionRules.door.startFrom);
    // THE LEVEL'S OWN PROPERTIES, as Game::readProperties reads them before
    // anything is scaled (art.json `door`): `shadowless_door` draws the exit door
    // on another sheet and, on a level that also sets `no_gravity`, hangs a glow
    // behind it; `no_gravity` alone bobs the door. The first `properties` node, as
    // SeekEntity finds it, and PRESENCE alone, which is what CheckCustomData tests:
    // the datum's value is never read.
    const Tscn::Node* properties = FirstOfRole(m_data, Roles::kLevelProperties);
    m_shadowlessDoor =
        properties != nullptr && properties->Meta(m_artRules.door.shadowlessFlag.c_str()) != nullptr;
    m_doorBobRuns = m_level.noGravity;
    // `ignore_emissive` on the same node, and the world the level is in: what the script sets the door's and the
    // character's emissive to (lighting.json script_emissive). Read before the art, so a level drawn as boxes holds
    // it too, and reset by it, so a level that sets none does not inherit the last one's.
    m_scriptEmissive = Lighting::ScriptEmissive(
        m_data.lighting, properties,
        m_current >= 0 ? m_chapters.levels[static_cast<std::size_t>(m_current)].world : -1);
    if (!Sprites::Find(m_data.scene, m_paths.art, m_tierRules, sprites, m_artError)) {
        // Played anyway, as boxes: the art is the original's, and a machine
        // without it can still play the port.
        SUPERSONIC_LOG_WARN("Magic Portals") << "Drawing the level as boxes: " << m_artError << std::endl;
        m_playerSlot = 0;
        return;
    }
    // Which tier each image was drawn from, once a load. A converter output
    // without its hd/ and fullhd/ copies draws every image at 1x and says
    // nothing else, so this line is where that shows, and where a sweep's logs
    // can count the tiers without a suite.
    {
        std::vector<std::string> names;
        for (const Tiers::Tier& tier : m_tierRules.search) names.push_back(tier.folder);
        names.emplace_back();
        std::vector<int> placements(names.size(), 0);
        std::vector<std::vector<std::string>> textures(names.size());
        for (const Sprites::Sprite& sprite : sprites) {
            const auto at = static_cast<std::size_t>(std::find(names.begin(), names.end(), sprite.tier) - names.begin());
            if (at == names.size()) continue; // Find only ever returns a searched folder or none
            ++placements[at];
            if (std::find(textures[at].begin(), textures[at].end(), sprite.texture) == textures[at].end()) {
                textures[at].push_back(sprite.texture);
            }
        }
        std::ostringstream line;
        for (std::size_t i = 0; i < names.size(); ++i) {
            line << (i == 0 ? "" : ", ") << placements[i] << " " << (names[i].empty() ? "1x" : names[i]);
        }
        line << " (placements); ";
        for (std::size_t i = 0; i < names.size(); ++i) {
            line << (i == 0 ? "" : ", ") << textures[i].size() << " " << (names[i].empty() ? "1x" : names[i]);
        }
        SUPERSONIC_LOG_INFO("Magic Portals") << "level art tiers: " << line.str() << " (textures)" << std::endl;
    }
    // THE NO-PORTAL SIGN IS NOT DRAWN WHERE THE LEVEL PUTS IT. Its node sits off
    // the level at (-61, -24), and its script eases it to the camera's corner
    // every frame (ui.json, hud.no_portal_sign), so it is taken out of the
    // level's sprites and drawn with the HUD. Found by its entity name, as
    // PortalManager's SeekEntity finds it.
    if (m_hudReady && !m_hudRules.sign.entity.empty()) {
        for (auto it = sprites.begin(); it != sprites.end(); ++it) {
            const Tscn::Node* node = m_data.scene.FindNode(it->node);
            if (node == nullptr || Roles::EntityName(*node) != m_hudRules.sign.entity) continue;
            m_sign.present = true;
            m_sign.sizeUnits = it->sizePx;
            m_sign.follow.at = Sprites::CentrePx(*it);
            // Its sprite's own file, which Sprites::Find resolved to the hd tier
            // the original draws on any screen over 480 px (the captures match
            // it: 0.977 against 0.818 for the 1x art at twice its size), at its
            // units, 64 x 64.
            m_sign.image = it->texture;
            sprites.erase(it);
            break;
        }
    }
    // THE SKY IS NOT DRAWN WHERE THE LEVEL PUTS IT EITHER. StaticSky moves it to
    // the camera every frame (sim/Sky.hpp), and it needs each sky's picture size
    // to do it, so it is built here, from the same sprites. A level whose skies
    // cannot be sized keeps them where the level put them, and says why.
    if (std::string why; !Sky::Build(m_skyRules, m_data.scene, sprites, m_viewHeightPx, m_sky, why)) {
        SUPERSONIC_LOG_WARN("Magic Portals") << "the sky is drawn where the level put it: " << why << std::endl;
        m_sky = Sky::Controller{};
    }
    // AND ON A SPACE LEVEL, SPACESKY (sim/Sky.hpp, sky.json space_sky): the space
    // sky, the spiral, the planet and the eclipse, each moved from the camera every
    // frame. The spiral is scaled to the diagonal of the screen as it is now - the
    // constructor's GetScreenSize - which is read here, where m_aspect may still be
    // the last level's. Its constructor's linearMotion call steps the planet by the
    // frame before the level's first update, taken as one tick.
    {
        const glm::dvec2 view(m_viewHeightPx * static_cast<double>(viewportAspect(registry)), m_viewHeightPx);
        if (std::string why;
            !Sky::BuildSpace(m_spaceRules, m_skyRules, m_motionRules, m_data.scene, view,
                             ScriptClock::Ms(m_step), m_space, why)) {
            SUPERSONIC_LOG_WARN("Magic Portals") << "the space sky is drawn where the level put it: " << why
                                                 << std::endl;
            m_space = Sky::Space{};
        } else if (m_space.running) {
            SUPERSONIC_LOG_INFO("Magic Portals")
                << "space sky: " << (m_space.skyNode.empty() ? "no sky" : m_space.skyNode) << ", "
                << (m_space.spiralNode.empty() ? std::string("no spiral")
                                               : m_space.spiralNode + " " + std::to_string(m_space.spiralSidePx) +
                                                     " u colour " + std::to_string(m_space.spiralColour))
                << ", " << (m_space.planetNode.empty() ? "no planet" : m_space.planetNode) << ", "
                << (m_space.eclipseNode.empty() ? "no eclipse" : m_space.eclipseNode) << std::endl;
        }
    }
    // THE PLAYER'S SLOT: after every sprite at or below its depth, which it was
    // added after (R10, art.json character.z). At 4 / 2.8125 = 1.42 in the level's
    // units that is after the nodes at eth_z 1 or below: one more than the slot after
    // z_index 0 the port gave it before step 76 on level22a, whose platform at 1 is
    // the only node in 128 levels between the two, and the same slot everywhere else.
    m_playerSlot = static_cast<int>(std::count_if(sprites.begin(), sprites.end(), [this](const Sprites::Sprite& s) {
        return static_cast<double>(s.zIndex) <= m_playerZ;
    }));
    // THE SLOTS OF PICTURES AT ONE DEPTH are dealt by the bucket their entities stand in (art.json
    // draw_order, Sprites::Rank): the original draws a tie in the order it walks the buckets, the lower
    // row and then the higher column last, so that one is over the other. Only the slot a picture holds
    // changes: `sprites` keeps Find's order, which the phases below, the sky and the emitters' stream
    // are all drawn in, and every run of one z_index keeps its set of slots, so nothing counted by
    // z_index (the player's slot, slotAfter) moves.
    {
        std::vector<Sprites::Standing> standing;
        standing.reserve(sprites.size());
        for (const Sprites::Sprite& sprite : sprites) {
            standing.push_back(Sprites::Standing{sprite.zIndex, sprite.depth, sprite.atPx, sprite.order});
        }
        Sprites::Rank(standing, m_artRules.drawOrder);
        for (std::size_t i = 0; i < sprites.size(); ++i) sprites[i].order = standing[i].slot;
    }
    const auto indexOf = [](const auto& list, const std::string& name) {
        for (std::size_t i = 0; i < list.size(); ++i) {
            if (list[i].name == name) return static_cast<int>(i);
        }
        return -1;
    };
    // The start angles of the crystals' and keys' bobs, drawn in the level's drawing
    // order (Sprites::Find's: z_index, then the file's order) from a stream seeded
    // afresh for this level, from kMotionSeed and its name.
    m_motions.clear();
    m_placed.clear();
    // And the rings' turn, from nothing, at the rate their callback adds
    // (art.json antiportal).
    m_antiportalTurn = Motion::Turn{};
    m_antiportalTurn.degPerS = m_artRules.antiportal.spinDegPerS;
    Motion::Phases phases(Motion::LevelSeed(
        kMotionSeed, m_current >= 0 ? m_chapters.levels[static_cast<std::size_t>(m_current)].name : std::string()));
    // THE SHEET A `shadowless_door` LEVEL'S EXIT DOOR IS DRAWN ON (art.json door):
    // the file the original's loader finds for the name, as for every picture the
    // port adds for itself (drawnImage). window01_shadowless.png has an hd twin and
    // no fullhd one and is the same size as window01.png, so the swap changes the
    // picture and not its units. One that cannot be read leaves the door on its own
    // sheet and says so once.
    std::string shadowlessTexture;
    if (m_shadowlessDoor) {
        const Sprites::Image& sheet = drawnImage(originalImage(m_artRules.door.shadowlessSprite));
        if (sheet.units.x <= 0 || sheet.units.y <= 0) {
            SUPERSONIC_LOG_WARN("Magic Portals")
                << "the door keeps its own sheet: " << sheet.path << " could not be read" << std::endl;
        } else {
            shadowlessTexture = sheet.path;
        }
    }
    for (Sprites::Sprite& sprite : sprites) {
        // ETHEntity::SetSprite, which readProperties calls before the level's first
        // frame, so the picture is made from the swapped file. The first `door` node
        // is the one SeekEntity finds, and every one of the 128 levels has exactly
        // one (art.json door's census).
        const bool isDoor = m_doorSprite < 0 && [this, &sprite] {
            const Tscn::Node* node = m_data.scene.FindNode(sprite.node);
            return node != nullptr && BareEntityName(*node) == m_artRules.door.entity;
        }();
        if (isDoor && !shadowlessTexture.empty()) sprite.texture = shadowlessTexture;
        // A FIRE AGENT, AND A BLUE OR RED KEY, KEYHOLE OR LOCKED DOOR, ARE DRAWN
        // FROM THE FILE THEIR SCRIPTS SET (art.json fire_agent, key_colour):
        // ETHCallback_fire_agent's SetSprite on every update and setKeyColor's once,
        // each before the entity is first drawn, so the picture the level saved is
        // never seen. Matched by the callback's name, the entity less its .ent, and
        // never by the saved file: an anti_portal_agent keeps its own face. The file
        // is found as SetSprite finds it, through the tiers; one of other units, or
        // one that cannot be read, leaves the saved picture and says so.
        std::string scripted;
        bool keyPainted = false;
        if (const Tscn::Node* node = m_data.scene.FindNode(sprite.node); node != nullptr) {
            const std::string bare = BareEntityName(*node);
            if (bare == m_artRules.fireAgent.entity) {
                scripted = m_artRules.fireAgent.sprite;
            } else if (m_artRules.keyColour.Paints(bare)) {
                const Tscn::Value* colour = node->Meta(m_artRules.keyColour.datum);
                scripted = m_artRules.keyColour.SpriteFor(
                    Roles::EntityName(*node),
                    colour != nullptr && colour->kind == Tscn::Value::Kind::String ? colour->text : std::string());
                keyPainted = !scripted.empty();
            }
        }
        if (!scripted.empty()) {
            const Sprites::Image& image = drawnImage(originalImage(scripted));
            if (image.units.x <= 0 || image.units.y <= 0 ||
                image.units != Tiers::Units(sprite.texels, sprite.density)) {
                SUPERSONIC_LOG_WARN("Magic Portals")
                    << sprite.node << " keeps the picture its level saved: " << image.path
                    << (image.units.x <= 0 || image.units.y <= 0 ? " could not be read" : " is not its size")
                    << std::endl;
            } else {
                sprite.texture = image.path;
                sprite.tier = image.tier;
                sprite.density = image.density;
                sprite.texels = image.texels;
            }
        }
        DrawnSprite drawn;
        drawn.quad = makeSprite(registry, "Magic Portals Sprite", sprite.texture, sprite.additive);
        drawn.z = pictureZ(sprite.order);
        drawn.bucket = Sprites::BucketOf(sprite.atPx, m_artRules.drawOrder);
        // THE EXIT DOOR AND THE DOORWAY'S BLACK, each the first node of its entity,
        // as readProperties' two SeekEntity calls find them (art.json door). Both
        // bob on a `no_gravity` level, with the glow this function adds at its end.
        if (isDoor) {
            m_doorSprite = static_cast<int>(m_sprites.size());
            drawn.doorBob = m_doorBobRuns;
        } else if (m_doorBgSprite < 0) {
            const Tscn::Node* node = m_data.scene.FindNode(sprite.node);
            if (node != nullptr && BareEntityName(*node) == m_artRules.door.backgroundEntity) {
                m_doorBgSprite = static_cast<int>(m_sprites.size());
                drawn.doorBob = m_doorBobRuns;
            }
        }
        // THE CUTSCENES' OTHER PICTURES (cutscenes.json), each the first node of its
        // exact name, as its callback's SeekEntity finds it: 4-01's rocket_bridge and
        // the key 4-19's landing shrinks.
        if (const Tscn::Node* node = m_data.scene.FindNode(sprite.node); node != nullptr) {
            const std::string name = Roles::EntityName(*node);
            if (m_bridgeSprite < 0 && m_launchPlaced && name == m_cutsceneRules.launch.bridge) {
                m_bridgeSprite = static_cast<int>(m_sprites.size());
            }
            if (m_landingKeySprite < 0 && m_landingPlaced && name == m_cutsceneRules.landing.key) {
                m_landingKeySprite = static_cast<int>(m_sprites.size());
            }
        }
        // Its instance colour and emissive, as its node gives them. A node the
        // look does not hold keeps the engine's defaults, colour 1 and emissive 0,
        // and carries no particle system; Lighting::Read holds every entity node,
        // so that is only ever a level whose lighting did not read, which is drawn
        // unlit anyway.
        if (const auto look = m_look.nodes.find(sprite.node); m_lit && look != m_look.nodes.end()) {
            drawn.colour = glm::vec4(look->second.colour);
            drawn.emissive = look->second.emissive;
            // THE EXIT DOOR OF AN `ignore_emissive` LEVEL has the emissive its level's door_emissive says instead of
            // door.ent's 0.7: readProperties #285-300, on the door SeekEntity found (lighting.json script_emissive).
            if (isDoor && m_scriptEmissive.ignore) drawn.emissive = m_scriptEmissive.door;
            // Its baked light, when the original draws one for it: static and
            // applying light (the design's section 3.1). Lighting::Read
            // already refuses a lightmap anywhere else; the condition is kept
            // so this line says what the engine does rather than what the file
            // happens to hold.
            if (look->second.isStatic && look->second.applyLight) drawn.lightmap = look->second.lightmap;
            // And what it is to the lights (design section 5.2).
            drawn.isStatic = look->second.isStatic;
            drawn.applyLight = look->second.applyLight;
            drawn.normal = look->second.normal;
            drawn.lookZ = look->second.z;
            drawn.particles = look->second.particles;
        }
        // THE SCALE ITS NODE'S OWN DATA GIVES IT (LevelBuilder::kScaleDatum): Game::preLoop's scaleEntities
        // runs scaleEntity on every entity the scene places, which calls ETHEntity::Scale(getScale() x the
        // `scale` datum, 1 where there is none), and that multiplies the picture (and the particle systems)
        // as the script's own Scale() calls below do - they multiply on top of it. 72 nodes of the 128 levels
        // carry the datum: the antiportals', which the manager halves below, and 2-01's crate_no_emissive,
        // 0.75, whose body LevelBuilder builds at the same scale (planning step 172). An unreadable datum is a
        // node LevelBuilder has refused; the picture keeps 1 and says so.
        if (const Tscn::Node* node = m_data.scene.FindNode(sprite.node); node != nullptr) {
            std::string why;
            if (!LevelBuilder::ReadCustomScale(*node, drawn.scale, why)) {
                drawn.scale = 1.0;
                SUPERSONIC_LOG_WARN("Magic Portals") << "picture not scaled: " << why << std::endl;
            }
        }
        // A STATIC PORTAL IS DRAWN AS ITS SCRIPT REDRAWS IT (art.json static_portal):
        // once, ETHCallback_portal_static scales the entity by 0.8 - its picture here,
        // its particle systems in buildEmitters - and sets its colour red or blue by
        // its node's `color`, over whatever colour the look gave it (SetColor writes
        // the rgb of m_v4Color and keeps its alpha). Keyed by the entity, not by
        // Portals' statics, which leave out one the level made inactive: its script
        // still runs.
        if (const Tscn::Node* node = m_data.scene.FindNode(sprite.node);
            node != nullptr && BareEntityName(*node) == m_artRules.staticPortal.entity) {
            const Tscn::Value* colour = node->Meta("color");
            const std::string named =
                colour != nullptr && colour->kind == Tscn::Value::Kind::String ? colour->text : std::string();
            drawn.colour = glm::vec4(glm::vec3(m_artRules.staticPortal.TintFor(named)), drawn.colour.a);
            drawn.scale *= m_artRules.staticPortal.scale;
        }
        // 4-19'S RED KEY STATE IS DRAWN AS ITS FIRST CALL REDRAWS IT (keys.json
        // red_key_state, #14-73): Scale(vector2(0.8, 1.5)) and SetColor(1, 0.1, 0.1)
        // over its portal_halo.png, once. The fade multiplies that colour from the
        // key's taking on (syncLighting), and the picture goes with the entity.
        if (m_level.redKey.present && sprite.node == m_level.redKey.name) {
            drawn.redKeyState = true;
            drawn.colour = glm::vec4(glm::vec3(m_level.redKey.rules.colour), drawn.colour.a);
        }
        // setKeyColor's SetColor(V3_ONE) after its swap: the rgb, the alpha kept.
        if (keyPainted) drawn.colour = glm::vec4(glm::vec3(m_artRules.keyColour.colour), drawn.colour.a);
        // A CRATE NO PORTAL CARRIES BLINKS RED (art.json non_teleportable):
        // manageNonTeleportable, which crateCallback runs first, for a crate whose
        // teleportable datum is 0 or absent - GetUInt reads an absent one as 0, as
        // Portals reads it - and not burned. Which of the two it blinks is its
        // callback's; syncLighting writes it every tick over the node's own.
        if (const Tscn::Node* node = m_data.scene.FindNode(sprite.node); node != nullptr) {
            double datum = 0.0;
            const Tscn::Value* flag = node->Meta(m_artRules.nonTeleportable.datum);
            const bool teleportable = flag != nullptr && flag->AsNumber(datum) && datum != 0.0;
            // crateCallback #16-36's hasBlinkFlag: a teleportable crate that carries it takes the
            // 200 ms branch, which is not built (art.json non_teleportable _not_built).
            double flagValue = 0.0;
            const Tscn::Value* blinkMeta = node->Meta(m_artRules.nonTeleportable.blinkDatum);
            const bool blinkFlag = blinkMeta != nullptr && blinkMeta->AsNumber(flagValue) && flagValue != 0.0;
            drawn.blinks = m_artRules.nonTeleportable.For(BareEntityName(*node), teleportable, blinkFlag);
            if (drawn.blinks != Art::NonTeleportable::Blinks::Nothing) {
                const std::vector<Fire::Burnable>& burnables = m_level.fire.burnables;
                for (std::size_t b = 0; b < burnables.size(); ++b) {
                    if (burnables[b].name == sprite.node) drawn.burnable = static_cast<int>(b);
                }
            }
        }
        // THE SCRIPT'S OTHER BLINKS (planning steps 169 and 170), each named by its callback, the
        // entity less its .ent: a barrel blinks its emissive every frame (art.json barrel_bomb),
        // from its minEmissive datum - the converter wrote the datum's value as an empty string, so
        // its presence says it is the one value the scenes hold - to 0.2; a breakable wall that
        // carries the uint `blink` blinks its colour and its emissive on one clock run twice a
        // frame (breakable_wall; _no_emissive and _sand have no callback); and a fire agent pulses
        // its picture (fire_agent pulse), and a help block is drawn at alpha 0.76 and breathes
        // (help_block). syncLighting writes the first two and syncSprites the other two, every tick.
        if (const Tscn::Node* node = m_data.scene.FindNode(sprite.node); node != nullptr) {
            const std::string bare = BareEntityName(*node);
            if (bare == m_artRules.barrelBomb.entity) {
                drawn.scriptBlink = DrawnSprite::ScriptBlink::Barrel;
                drawn.blinkFloor = m_artRules.barrelBomb.FloorFor(node->Meta(m_artRules.barrelBomb.datum) != nullptr);
            } else if (bare == m_artRules.breakableWall.entity) {
                double flagValue = 0.0;
                const Tscn::Value* flag = node->Meta(m_artRules.breakableWall.datum);
                if (flag != nullptr && flag->AsNumber(flagValue) && flagValue != 0.0) {
                    drawn.scriptBlink = DrawnSprite::ScriptBlink::Wall;
                }
            } else if (bare == m_artRules.fireAgent.entity) {
                drawn.firePulse = true;
            } else if (bare == m_artRules.helpBlock.entity) {
                drawn.helpBlock = true;
            }
        }
        drawn.ownerPx = sprite.atPx;
        drawn.centrePx = Sprites::CentrePx(sprite);
        // Godot's rotation is clockwise and the converter wrote it as Ethanon's
        // angle negated (tscn.py:433).
        drawn.ownerAngleDeg = -sprite.rotation * 180.0 / 3.14159265358979323846;
        drawn.key = indexOf(m_level.keys.keys, sprite.node);
        drawn.keyhole = indexOf(m_level.keys.keyholes, sprite.node);
        drawn.diamond = indexOf(m_level.diamonds.diamonds, sprite.node);
        drawn.crystal = indexOf(m_level.goals.crystals, sprite.node);
        drawn.staticPortal = indexOf(m_level.portals.statics, sprite.node);
        drawn.zone = indexOf(m_level.portals.zones, sprite.node);
        drawn.agent = indexOf(m_level.portals.agents, sprite.node);
        drawn.torch = indexOf(m_level.torch.lights, sprite.node);
        drawn.well = indexOf(m_level.wells.wells, sprite.node);
        drawn.field = indexOf(m_level.fields.fields, sprite.node);
        // A draggable handle, or the fire agent or reflector that follows one
        // (handle.json): drawn from sim/Handle's point, not the node's.
        for (std::size_t h = 0; h < m_level.handles.handles.size(); ++h) {
            if (m_level.handles.handles[h].node == sprite.node) drawn.handle = static_cast<int>(h);
        }
        for (std::size_t f = 0; f < m_level.handles.followers.size(); ++f) {
            if (m_level.handles.followers[f].node == sprite.node) drawn.follower = static_cast<int>(f);
        }
        // A lift (movers.json `lift`): its callback blinks it, both spellings.
        for (std::size_t l = 0; l < m_level.movers.lifts.size(); ++l) {
            if (m_level.movers.lifts[l].name == sprite.node) drawn.lift = static_cast<int>(l);
        }
        // AN ANTIPORTAL IS DRAWN AT HALF ITS NODE'S SCALE (placement.json
        // antiportal.manager): Game::preLoop scales the entity by its node's `scale`
        // and AntiPortalManager's constructor then calls Scale(0.5) on every entity
        // it collects, so white_ring.png's 128 units are drawn 64 x scale across.
        // Keyed by the entity, as the manager collects it: an anti_portal_agent or a
        // no_portal zone is not one of its fields and keeps its own size.
        //
        // The node's `scale` is its custom data, which SGlobalScale::scaleEntity
        // applies (CheckCustomData, GetFloat, Scale) and the lines above have put in
        // drawn.scale (step 172: before it, only a zone's picture read it). It is not
        // the Sprite2D `scale` that step 9.1 (00_order, K9) has Sprites::Find apply:
        // that one is the entity's <Scale> element, 1 for antiportal.ent, and
        // multiplies on top of this line, not in place of it.
        if (const Tscn::Node* node = m_data.scene.FindNode(sprite.node);
            drawn.zone >= 0 && node != nullptr && BareEntityName(*node) == m_level.portals.rules.antiportalEntity) {
            drawn.scale *= m_level.portals.rules.antiportalManagerScale;
        }
        // AND IT BLINKS AND TURNS (art.json antiportal): ETHCallback_antiportal is
        // named for the entity less its .ent, so it runs for every placement of it,
        // either spelling, whatever the port made of the node. The size above is the
        // manager's and this is the callback's: two facts about one entity, each
        // where the file it was decoded from keeps it.
        if (const Tscn::Node* node = m_data.scene.FindNode(sprite.node);
            node != nullptr && BareEntityName(*node) == m_artRules.antiportal.entity) {
            drawn.antiportal = true;
        }
        // 2-32'S BOSS AND ITS CLAW, matched by entity name as Dragon::Find matches
        // them, and only where the sim actually found a dragon. Both pictures are
        // drawn from Dragon::State from here on: the dragon's node sits off the
        // left edge of the level at (-164, -84) and its callback flies it, and the
        // claw's node at (-185, 184) is never where the claw is - its waypoints are
        // measured from the camera's left edge, which on this one level IS the
        // dragon (dragon.json camera).
        if (const Tscn::Node* node = m_data.scene.FindNode(sprite.node); node != nullptr && m_level.dragon.present) {
            const std::string entity = Roles::EntityName(*node);
            if (entity == m_data.dragon.entityName) {
                drawn.dragon = true;
                m_dragonSprite = static_cast<int>(m_sprites.size());
            } else if (entity == m_data.dragon.clawName) {
                drawn.claw = true;
                m_clawSprite = static_cast<int>(m_sprites.size());
            }
        }
        // ETHCallback_crystal and ETHCallback_key each call linearMotion with their
        // row's arguments; the callback is named by the entity less its .ent, so both
        // spellings of a placement bob. A crystal or key under any other entity runs
        // another script and does not.
        if (const Tscn::Node* node = m_data.scene.FindNode(sprite.node); node != nullptr) {
            const std::string entity = BareEntityName(*node);
            const Motion::Row* row = nullptr;
            if (drawn.crystal >= 0 && m_motionRules.crystal.Names(entity)) row = &m_motionRules.crystal;
            if (drawn.key >= 0 && m_motionRules.key.Names(entity)) row = &m_motionRules.key;
            if (row != nullptr) {
                drawn.motion = static_cast<int>(m_motions.size());
                m_motions.push_back(Motion::Start(*row, phases.Next(*row)));
            }
            // A PLACED PICTURE'S OWN CALLBACK (motions.json `placed`): the hint arrow,
            // the dashed circle and square, the tapping hand. Its swing takes the
            // node's speed and stride and a constant start angle, so it draws nothing
            // from `phases`; its SetAlpha writes the colour's alpha, as Ethanon's
            // writes m_v4Color.w (no node of these carries an eth_color); its turn is
            // syncSprites'.
            if (const Motion::Placed* placed = m_motionRules.FindPlaced(entity); placed != nullptr) {
                PlacedScript script;
                script.row = static_cast<std::size_t>(placed - m_motionRules.placed.data());
                if (placed->moves) {
                    std::string why;
                    script.moves = Motion::FromNode(*placed, *node, script.motion, why);
                    // GetFloat of a datum a node lacks is 0, and a speed of 0 moves nothing.
                    if (!script.moves) {
                        SUPERSONIC_LOG_WARN("Magic Portals") << "drawn without its swing: " << why << std::endl;
                    }
                }
                script.turn.degPerS = placed->spinDegPerS;
                if (placed->setsAlpha) drawn.colour.a = static_cast<float>(placed->alpha);
                drawn.placed = static_cast<int>(m_placed.size());
                m_placed.push_back(script);
            }
            // 1-21'S LESSON (camera.json `hint`): ETHCallback_hand_waypoint1 and
            // ETHCallback_up_down_arrow, named for the entity less its .ent, so either
            // spelling of a placement runs them. The hand's first call also draws the
            // line of text (emitHud).
            if (entity == m_hintRules.handEntity) {
                drawn.hint = HintRole::Hand;
                m_hintHandPlaced = true;
            } else if (entity == m_hintRules.arrowEntity) {
                drawn.hint = HintRole::Arrow;
                m_hintArrowPlaced = true;
            } else if (entity == m_data.handle.lessonEntity) {
                // 2-20'S LESSON (handle.json `lesson`): ETHCallback_hand_waypoint0, from
                // where the level put it, watching the handle its name finds - the
                // nearest, as seekNeighbourEntity does; 2-20 places one.
                drawn.hint = HintRole::Lesson;
                m_lesson = Handle::Lesson{};
                m_lesson.placed = true;
                m_lesson.node = sprite.node;
                m_lesson.originalPx = sprite.atPx;
                m_lessonHandle = -1;
                double nearest = 0.0;
                for (std::size_t h = 0; h < m_level.handles.handles.size(); ++h) {
                    const Handle::Handle& handle = m_level.handles.handles[h];
                    if (handle.entityName != m_data.handle.lessonHandleName) continue;
                    const double distance = glm::length(handle.atPx - sprite.atPx);
                    if (m_lessonHandle < 0 || distance < nearest) {
                        m_lessonHandle = static_cast<int>(h);
                        nearest = distance;
                    }
                }
            }
        }
        if (m_sky.running) {
            for (std::size_t t = 0; t < m_sky.skies.size(); ++t) {
                if (m_sky.skies[t].node == sprite.node) drawn.sky = static_cast<int>(t);
            }
            drawn.satellite = m_sky.hasSatellite && m_sky.satelliteNode == sprite.node;
            // A SKY NARROWER THAN THE VIEW (sky.json pixel_cover): dark_sky.png, the
            // one sky with no fullhd twin, is 455 u across a view of 455.11. Its
            // cover is its own picture, drawn behind it to the frame's pixel edges
            // (placeSkyCover), so the MSAA samples it leaves at those edges are the
            // sky's and not the black clear's, as the original's pixel-centre frame
            // draws them. A sky as wide as the view or wider gets none. The view as
            // the viewport is now: m_aspect may still be the last level's here.
            if (drawn.sky >= 0 && m_skyRules.pixelCover &&
                Sky::DrawnSizePx(m_sky, static_cast<std::size_t>(drawn.sky)).x <
                    m_viewHeightPx * static_cast<double>(viewportAspect(registry))) {
                drawn.cover = makeSprite(registry, kSkyCoverTag, sprite.texture, sprite.additive);
            }
        }
        if (m_space.running) {
            if (sprite.node == m_space.skyNode) drawn.space = SpaceRole::Sky;
            if (sprite.node == m_space.spiralNode) drawn.space = SpaceRole::Spiral;
            if (sprite.node == m_space.planetNode) drawn.space = SpaceRole::Planet;
            if (sprite.node == m_space.eclipseNode) drawn.space = SpaceRole::Eclipse;
            // SpaceSky's SetColor(c, c, c) on the spiral, once: the rgb, over the look's,
            // with its alpha kept (ETHEntity.cpp:493-498).
            if (drawn.space == SpaceRole::Spiral) {
                drawn.colour = glm::vec4(glm::vec3(static_cast<float>(m_space.spiralColour)), drawn.colour.a);
            }
        }
        if (const auto body = m_level.built.entities.find(sprite.node); body != m_level.built.entities.end()) {
            drawn.body = body->second;
            // Bodies move on the tick and are drawn at its place, as their boxes are.
            registry.emplace<InterpolatedTransformComponent>(drawn.quad);
        }
        if (drawn.key >= 0 || drawn.diamond >= 0) {
            // Carried on the tick, as a body is moved, and drawn at its place.
            registry.emplace_or_replace<InterpolatedTransformComponent>(drawn.quad);
        }
        if (drawn.sky >= 0 || drawn.satellite || drawn.space != SpaceRole::None) {
            // Placed on the tick from the camera the tick left, and drawn as the
            // camera is: both at the tick's place, or - where the display outruns the
            // tick - both between ticks with the same alpha, so the sky keeps its place
            // on the screen on the frames between.
            if (!registry.all_of<InterpolatedTransformComponent>(drawn.quad)) {
                registry.emplace<InterpolatedTransformComponent>(drawn.quad);
            }
            // And a sky's cover with it, placed on the same ticks from the same camera.
            if (drawn.cover != entt::null) registry.emplace<InterpolatedTransformComponent>(drawn.cover);
        }
        drawn.sprite = std::move(sprite);
        m_sprites.push_back(std::move(drawn));
    }
    m_rankedSprites = m_sprites.size();
    // 2-32'S DRAGON IS DRAWN A CELL AT A TIME (dragon.json picture): dragon.ent
    // cuts dragon_boss.png 2 x 2, and the port cuts it through Tiers::FrameCut, the
    // one cut rule (00_order K5). The file has no hd twin, so a frame is 226 x 180
    // of its 452 x 360 units. A sheet that cannot be cut keeps the picture whole
    // and says so once, as the minions' does.
    if (m_dragonSprite >= 0) {
        const Sprites::Sprite& sheet = m_sprites[static_cast<std::size_t>(m_dragonSprite)].sprite;
        std::string why;
        if (!Tiers::FrameCut(sheet.texels, sheet.density, m_data.dragon.columns, m_data.dragon.rows, m_dragonCut,
                             why)) {
            m_dragonCut = Tiers::Cut{};
            SUPERSONIC_LOG_WARN("Magic Portals")
                << "the dragon is drawn on its whole sheet: " << sheet.texture << " could not be cut " << why
                << std::endl;
        }
    }

    // The beholder at its adder's z_index and a spike at -4: after the sprites
    // drawn at or below it and before the next - and after the player only where
    // it stands at the player's depth or in front of it. The original orders by
    // depth alone and keeps the order things were added in at equal depth
    // (ETHEntityRenderingManager.cpp:70-106), and the player is added after the
    // level's nodes. The rule and the fraction each class stands at are the
    // between-slot table's now (slotAfter, MagicPortalsLayer.hpp; 00_order K13);
    // what each caller asks for is which class it is.
    int adderZ = 0;
    if (m_level.boss.beholder) {
        if (const Tscn::Node* adder = m_data.scene.FindNode(m_level.boss.beholder->name)) {
            double z = 0.0;
            if (const Tscn::Value* value = adder->Find("z_index"); value != nullptr && value->AsNumber(z)) {
                adderZ = static_cast<int>(z);
            }
        }
    }
    m_beholderZ = slotAfter(adderZ, Slot::Beholder);
    m_spikeZ = slotAfter(kSpikeZIndex, Slot::Spike);
    // THE MINIONS (art.json minion z): minion_spawn adds each at a raw 2, which is
    // 2 / 2.8125 = 0.71 in the level's units - after every node at 0 or below and,
    // being behind the player at 1.42, before it. An escort the chapter-3 boss calls
    // stands at its anchor's own depth (minion_spawn_point, eth_z 0 in level31b), a
    // placed node's and so already in the level's units. The two share a slot here
    // where the original draws the marker's minion in front (2 > 0): a tie the
    // between-slot table of the visuals plan's 6.3 (00_order K13) is to order.
    m_minionZ = slotAfter(m_artRules.LevelDepth(m_artRules.minion.z), Slot::Minion);
    // 2-32's knight, at the depth its spawn's GetPosition() gives AddScaledEntity:
    // level31a's node at 0 (cutscenes.json dragon_knight._z), behind the player.
    m_knightZ = slotAfter(0.0, Slot::Minion);
    // The anchor as Ghost::Find takes it: by entity name, the last such node.
    m_escortLookZ = 0.0;
    for (const Tscn::Node& node : m_data.scene.nodes) {
        if (!m_level.ghost.present || node.parent != "." ||
            Roles::EntityName(node) != m_level.ghost.rules.summonAnchorName) {
            continue;
        }
        double z = 0.0;
        const Tscn::Value* eth = node.Meta("eth_z");
        const Tscn::Value* index = node.Find("z_index");
        m_escortLookZ = eth != nullptr && eth->AsNumber(z) ? z : (index != nullptr && index->AsNumber(z) ? z : 0.0);
    }
    m_escortZ = slotAfter(m_escortLookZ, Slot::Escort);
    // THE GHOST OF THE LEVEL'S LIGHT WALL (art.json light_wall_ghost), which
    // seekLightWallToDestroy adds where the wall stands when a torch is lit:
    // dashed_door.png at the wall's own point, size and angle, `zOffset` raw
    // pixels behind it. Resolved here, with the wall's picture in hand and before
    // any quad is made for it - the ghost is made when the wall is flagged, which
    // may be never. Its depth is a RAW one, like the player's 4 and a minion's 2,
    // so it goes through LevelDepth: 4 / 2.8125 = 1.42 of the level's units behind
    // a wall the level file placed, which is a slot of its own behind the wall's
    // and ahead of everything at or below it. Slot::WallGhost in the between-slot
    // table (00_order K13), which the merge of the two tracks moved it into: the
    // lambda it was written against stood at the same -0.5, so the z is the one
    // step 80 measured.
    m_wallGhost = WallGhost{};
    if (m_level.torch.wall.present) {
        const Art::LightWallGhost& rules = m_artRules.lightWallGhost;
        const Sprites::Image& image = drawnImage(originalImage(rules.sprite));
        const DrawnSprite* wall = nullptr;
        for (const DrawnSprite& drawn : m_sprites) {
            if (drawn.sprite.node == m_level.torch.wall.name) {
                wall = &drawn;
                break;
            }
        }
        if (image.units.x <= 0 || image.units.y <= 0) {
            SUPERSONIC_LOG_WARN("Magic Portals")
                << "no ghost where the light wall stood: " << image.path << " could not be read" << std::endl;
        } else if (wall == nullptr) {
            SUPERSONIC_LOG_WARN("Magic Portals") << "no ghost where the light wall stood: " << m_level.torch.wall.name
                                                 << " has no picture to take a point from" << std::endl;
        } else {
            m_wallGhost.texture = image.path;
            // AddEntity's position is the wall's ENTITY point and dashed_door.ent's
            // PivotAdjust is (0, 0), so the ghost is centred on it, and its angle is
            // the wall's - 90 degrees on 4-32, the one turned wall in the game.
            m_wallGhost.atPx = wall->sprite.atPx;
            m_wallGhost.rotation = Units::ToWorldRotation(wall->sprite.rotation);
            m_wallGhost.sizePx = glm::dvec2(image.units);
            m_wallGhost.lookZ = wall->lookZ + m_artRules.LevelDepth(rules.zOffset);
            m_wallGhost.z = slotAfter(m_wallGhost.lookZ, Slot::WallGhost);
            m_wallGhost.turn.degPerS = rules.spinDegPerSec;
        }
    }
    // A TIMED CRYSTAL'S DIAL (art.json timer), which addTimerToCrystal adds
    // `zOffset` behind its crystal: after the sprites at or below the crystal's
    // z_index + zOffset and before the next, so what stands between the two
    // covers it, as depth would in the original. One per timed crystal whose
    // picture is drawn; the crystal's box has none. A `time` of 0 never runs
    // down in Goals, so it has no dial to count.
    //
    // Its sheet is the tier file the original draws (drawnImage, as every picture
    // the port adds for itself since step 67) and its cell a frame of that file's
    // units (frameUnits). timer.png has no hd or fullhd twin, so it comes back as
    // named at density 1 and the cell is 32 x 32 units, as before.
    const Art::Timer& timer = m_artRules.timer;
    const Sprites::Image& timerSheet = drawnImage(originalImage(timer.sprite));
    const glm::dvec2 timerCellPx = frameUnits(timerSheet, timer.columns, timer.rows);
    for (std::size_t i = 0; i < m_sprites.size(); ++i) {
        const DrawnSprite& drawn = m_sprites[i];
        if (drawn.crystal < 0) continue;
        const Goals::Crystal& crystal = m_level.goals.crystals[static_cast<std::size_t>(drawn.crystal)];
        if (!crystal.timed || !(crystal.lifeS > 0.0)) continue;
        if (timerCellPx == glm::dvec2(0.0)) {
            SUPERSONIC_LOG_WARN("Magic Portals") << "no dial behind the timed crystals: " << timerSheet.path
                                                 << " could not be read" << std::endl;
            break;
        }
        TimerDial dial;
        dial.crystal = drawn.crystal;
        dial.sprite = static_cast<int>(i);
        dial.quad = makeSprite(registry, "Magic Portals Timer", timerSheet.path, timer.additive);
        auto& animation = registry.emplace<SpriteAnimationComponent>(dial.quad);
        animation.columns = static_cast<uint32_t>(timer.columns);
        animation.rows = static_cast<uint32_t>(timer.rows);
        animation.frameCount = 1;
        animation.playing = false;
        dial.cellPx = timerCellPx;
        dial.z = slotAfter(drawn.sprite.zIndex + timer.zOffset, Slot::TimerDial);
        dial.lookZ = drawn.lookZ + static_cast<double>(timer.zOffset);
        dial.atPx = drawn.centrePx;
        dial.alpha = timer.alpha;
        m_timers.push_back(dial);
    }
    // A GRAVITY WELL'S GREEN RING (gravitywell.json `ring`), which no level file
    // pictures: ETHCallback_gravity_agent adds an antiportal.ent at the agent's own
    // position the first time it runs (a static agent's first frame on screen),
    // sizes it to radius x 2 - 24, its edge 12 units inside the well's reach, and
    // paints it green. Built here with the level instead; step 73 records why.
    // A picture with no node, like the dark dragon's dropped platform, and the ten
    // of them across five levels are the only wells in the game.
    //
    // Added after every slotAfter above has been taken, because that lambda counts
    // what is in m_sprites: a ring pushed before the dials would put a dial a slot
    // deeper. All ten sit at one depth, the -5 AddEntity gives them, and they are
    // added, so the order between them cannot show.
    //
    // Its picture is the tier file the original draws (drawnImage, as every
    // picture the port adds for itself since step 67): hd/white_ring.png, 256 x
    // 256 texels at density 2, so the same 128 units as the 1x file, and the file
    // the levels' own antiportal rings draw (step 66). Only its sampling changes:
    // its size is scaleToSize's, below. A level with no well resolves nothing.
    const GravityWell::Ring& ring = m_level.wells.rules.ring;
    const Sprites::Image* ringImage =
        m_level.wells.wells.empty() ? nullptr : &drawnImage(originalImage(ring.sprite));
    const int ringZIndex = static_cast<int>(std::floor(ring.z));
    if (ringImage != nullptr && ringImage->units == glm::ivec2(0)) {
        SUPERSONIC_LOG_WARN("Magic Portals")
            << "no ring round the gravity wells: " << ringImage->path << " could not be read" << std::endl;
    } else if (ringImage != nullptr) {
        const float ringZ = slotAfter(ringZIndex, Slot::WellRing);
        for (const GravityWell::Well& well : m_level.wells.wells) {
            DrawnSprite made;
            made.sprite.node = well.name + "#ring";
            made.sprite.texture = ringImage->path;
            made.sprite.tier = ringImage->tier;
            made.sprite.density = ringImage->density;
            made.sprite.texels = ringImage->texels;
            made.sprite.atPx = well.atPx;
            // The SIZE scaleToSize gives it - radius x 2 less the shrink - and not
            // white_ring.png's own 128, which is only what that scale is measured
            // from. The manager's 0.5 is not in it: this entity is called
            // `gravity_area` before the manager ever looks for an antiportal.
            made.sprite.sizePx = glm::dvec2(well.RingSizePx(m_level.wells.rules));
            made.sprite.additive = ring.additive;
            made.sprite.zIndex = ringZIndex;
            made.z = ringZ;
            made.colour = glm::vec4(static_cast<glm::vec3>(ring.colour), 1.0f);
            made.emissive = ring.emissive;
            made.isStatic = ring.isStatic;
            made.applyLight = ring.applyLight;
            made.lookZ = ring.z;
            made.centrePx = well.atPx;
            made.ownerPx = well.atPx;
            // No lightmap: a bake belongs to an entity the level file placed, named
            // by its id, and this one is added while the level runs.
            made.quad = makeSprite(registry, "Magic Portals Well Ring", ringImage->path, ring.additive);
            m_sprites.push_back(std::move(made));
        }
    }
    // THE RING EACH SHOCK AGENT ADDS ROUND ITSELF (art.json shock_ring), at the
    // absolute -10. Resolved HERE, for slotAfter's sake: it counts what is in
    // m_sprites at or below the depth asked, so the door glow below (z_index -22,
    // on 4-04 to 4-08, 4-12, 4-14 and 4-15, which place rings too) must not be in
    // it yet, or every ring on those levels would stand a slot too deep; the well
    // rings above stand at -5 and are not counted at -10. The rings' own quads are
    // not m_sprites', so nothing after them counts them.
    buildShockRings(registry);
    // THE GLOW BEHIND A `shadowless_door` LEVEL'S EXIT DOOR (art.json door), which
    // no level file places: Game::readProperties adds a door_highlight.ent at the
    // door's own point on the 17 levels that set `shadowless_door` and
    // `no_gravity`, and the pass that scales the level then scales it with
    // everything else, so its -22 is a depth in the level's units. A picture with
    // no node, like the well rings and the dark dragon's dropped platform.
    //
    // Added last, after every slotAfter above has been taken, for the reason the
    // rings are: that lambda counts what is in m_sprites, and a picture at -22
    // pushed before them would be counted by every one of their higher depths and
    // put a dial or a ring a slot too deep. Its own count cannot move the other
    // way - no picture added here stands at or below -22.
    const Art::Door& doorRules = m_artRules.door;
    if (m_doorSprite >= 0 && m_shadowlessDoor && m_doorBobRuns) {
        const Sprites::Image& glow = drawnImage(originalImage(doorRules.highlight.sprite));
        if (glow.units.x <= 0 || glow.units.y <= 0) {
            SUPERSONIC_LOG_WARN("Magic Portals")
                << "no glow behind the door: " << glow.path << " could not be read" << std::endl;
        } else {
            const DrawnSprite& door = m_sprites[static_cast<std::size_t>(m_doorSprite)];
            const int glowZIndex = static_cast<int>(std::floor(doorRules.highlightZ));
            DrawnSprite made;
            made.sprite.node = door.sprite.node + "#highlight";
            made.sprite.texture = glow.path;
            made.sprite.tier = glow.tier;
            made.sprite.density = glow.density;
            made.sprite.texels = glow.texels;
            // AddEntity's position is the door's ENTITY point, and the .ent's
            // PivotAdjust is (0, 0), so the picture is centred on it.
            made.sprite.atPx = door.ownerPx;
            made.sprite.sizePx = glm::dvec2(glow.units);
            made.sprite.additive = doorRules.highlight.additive;
            made.sprite.zIndex = glowZIndex;
            made.z = slotAfter(doorRules.highlightZ, Slot::DoorHighlight);
            made.emissive = doorRules.highlight.emissive;
            made.isStatic = doorRules.highlight.isStatic;
            made.applyLight = doorRules.highlight.applyLight;
            made.lookZ = doorRules.highlightZ;
            made.centrePx = made.sprite.atPx;
            made.ownerPx = made.sprite.atPx;
            made.doorBob = true;
            // No lightmap: applyLight 0 is what ETHLightmapGen refuses to bake for,
            // and a bake belongs to an entity the level file placed anyway.
            made.quad = makeSprite(registry, "Magic Portals Door Highlight", glow.path, made.sprite.additive);
            m_doorHighlight = static_cast<int>(m_sprites.size());
            m_sprites.push_back(std::move(made));
            SUPERSONIC_LOG_INFO("Magic Portals")
                << "the door is drawn shadowless, with " << glow.path << " behind it" << std::endl;
        }
    } else if (m_shadowlessDoor && m_doorSprite >= 0) {
        // readProperties' ins 327-330 gate: the swap without the glow. No shipped
        // level reaches it - `shadowless_door` is on 17 levels and every one of them
        // sets `no_gravity` too - so it is logged rather than drawn.
        SUPERSONIC_LOG_INFO("Magic Portals")
            << "the door is drawn shadowless, with no glow: the level does not set no_gravity" << std::endl;
    }
    buildBlackBars(registry);
    m_artReady = true;
    // And what the level's own art does not show: the entities' particles.
    buildEmitters(registry);
    // Then its lights, whose brightness their owners' particles set.
    buildLights(registry);
}

// THE FOUR BLACK BARS ROUND THE SCENE (art.json black_bar): Game::manageSceneBounds
// adds black_bar.ent at four points measured from the `max` marker, on every level
// and with no condition, after the scaling pass. The port adds them with the level's
// art, from the same marker (m_boundsPx, the level_bounds role, which is `max`).
//
// Their DEPTH is the entity's own: black_bar.ent is layerable at depth 1.0, which
// Ethanon draws in front of every piece of the scene whatever its z - so the bars
// stand at kLayerableTopZ, which nothing else reaches, and not in a slot. An .ent
// that said anything else would not be the one the decode read, and the bars are
// then left out and the log says why, rather than drawn somewhere the port chose.
//
// They are placed ONCE and never moved: the earthquake moves the drawn camera and
// not the level (shakeView), so what a tremble uncovers past the clamp is exactly
// the band these hide - C and D below the scene on a forth, A and B above it on the
// back that overshoots - and at rest none of them is on the view.
void MagicPortalsLayer::buildBlackBars(entt::registry& registry) {
    m_blackBars.clear();
    m_blackBarEntity = Art::Entity{};
    const Art::BlackBars& rules = m_artRules.blackBars;
    std::string error;
    if (!Art::LoadEnt(m_paths.original + "/entities/" + rules.entity, m_blackBarEntity, error)) {
        SUPERSONIC_LOG_WARN("Magic Portals") << "no black bars round the scene: " << error << std::endl;
        return;
    }
    const Art::Entity& entity = m_blackBarEntity;
    if (entity.type != Art::EntityType::Layerable || entity.LayerableDepth() < 1.0) {
        SUPERSONIC_LOG_WARN("Magic Portals")
            << "no black bars round the scene: " << rules.entity << " is not layerable at the front (type "
            << static_cast<int>(entity.type) << ", layerDepth " << entity.layerDepth << ")" << std::endl;
        return;
    }
    const Sprites::Image& image = drawnImage(originalImage(entity.sprite));
    if (image.units.x <= 0 || image.units.y <= 0) {
        SUPERSONIC_LOG_WARN("Magic Portals")
            << "no black bars round the scene: " << image.path << " could not be read" << std::endl;
        return;
    }
    // AddScaledEntity at getScale(), which is 1 in the level's units: the picture
    // is its image's units at the entity's own <Scale>, centred on its point
    // (layerable, so EO_CENTER, and PivotAdjust (0, 0)).
    const glm::dvec2 sizePx = glm::dvec2(image.units) * entity.ScaleAt(1.0);
    for (const glm::dvec2& centrePx : rules.Centres(m_boundsPx)) {
        BlackBarQuad bar;
        bar.texture = image.path;
        bar.centrePx = centrePx;
        bar.sizePx = sizePx;
        bar.quad = makeSprite(registry, "Magic Portals Black Bar", image.path, entity.additive);
        placeSprite(registry, bar.quad, bar.centrePx, bar.sizePx, kLayerableTopZ, 0.0f);
        m_blackBars.push_back(std::move(bar));
    }
    SUPERSONIC_LOG_INFO("Magic Portals") << "black bars: " << m_blackBars.size() << " of " << image.path << " ("
                                         << sizePx.x << " x " << sizePx.y << ") round a scene to max ("
                                         << m_boundsPx.x << ", " << m_boundsPx.y << ")" << std::endl;
}

std::vector<MagicPortalsLayer::BlackBarReport> MagicPortalsLayer::BlackBars() const {
    std::vector<BlackBarReport> reports;
    reports.reserve(m_blackBars.size());
    for (const BlackBarQuad& bar : m_blackBars) {
        BlackBarReport report;
        report.quad = bar.quad;
        report.texture = bar.texture;
        report.centrePx = bar.centrePx;
        report.sizePx = bar.sizePx;
        report.z = kLayerableTopZ;
        reports.push_back(std::move(report));
    }
    return reports;
}

// THE RING EACH SHOCK AGENT ADDS ROUND ITSELF (art.json shock_ring; the visuals plan's
// 13.1, decode D6 and its check). ETHCallback_shock_agent adds shock_area.ent at its
// own point on its first frame, at the ABSOLUTE depth g_scale.scale(-10) - not the
// agent's z less 10, and six agents stand at another z than 0 - sizes it to the
// lethal diameter with scaleToSize and colours it (0.4, 0.5, 0.5). The port builds
// one per field with the level's art. The picture is the .ent's own, read at run
// time: white_ring.png, whose 720 px tier is hd/white_ring.png, 256 texels at density
// 2 (drawnImage), drawn at scaleToSize's size and not at the file's 128 units.
//
// An .ent the port cannot draw as the original draws this one - a vertical or
// layerable type, a pivot, particles or a light the decode did not read - leaves the
// rings out and says why, as the black bars do.
void MagicPortalsLayer::buildShockRings(entt::registry& registry) {
    m_shockRings.clear();
    m_shockRingEntity = Art::Entity{};
    const std::vector<Fields::Field>& fields = m_level.fields.fields;
    if (fields.empty()) return;
    const Art::ShockRing& rules = m_artRules.shockRing;
    std::string error;
    if (!Art::LoadEnt(m_paths.original + "/entities/" + rules.entity, m_shockRingEntity, error)) {
        SUPERSONIC_LOG_WARN("Magic Portals") << "no ring round the shock agents: " << error << std::endl;
        return;
    }
    const Art::Entity& entity = m_shockRingEntity;
    if (entity.sprite.empty() || entity.type != Art::EntityType::Horizontal || entity.pivotPx != glm::dvec2(0.0) ||
        !entity.systems.empty() || entity.light.has_value()) {
        SUPERSONIC_LOG_WARN("Magic Portals")
            << "no ring round the shock agents: " << rules.entity
            << " is not the plain horizontal picture the decode read (a sprite, type 0, no pivot, no particles, "
               "no light)"
            << std::endl;
        return;
    }
    const Sprites::Image& image = drawnImage(originalImage(entity.sprite));
    if (image.units.x <= 0 || image.units.y <= 0) {
        SUPERSONIC_LOG_WARN("Magic Portals")
            << "no ring round the shock agents: " << image.path << " could not be read" << std::endl;
        return;
    }
    // After the level's pictures at or below -10 and before the next: the art at
    // -70 to -14 behind it on 3-24, the platforms at 0 and the agent itself in front.
    const double lookZ = rules.LevelZ(m_artRules.scaleFactor);
    const float z = slotAfter(lookZ, Slot::ShockRing);
    for (std::size_t i = 0; i < fields.size(); ++i) {
        const Fields::Field& field = fields[i];
        ShockRingQuad ring;
        ring.field = static_cast<int>(i);
        ring.texture = image.path;
        ring.atPx = field.atPx;
        ring.baseUnits = rules.SizeUnits(field.radiusPx);
        ring.turn.degPerS = rules.spinDegPerS;
        ring.z = z;
        ring.lookZ = lookZ;
        // No lightmap: a bake belongs to an entity the level file placed, and this
        // one is added while the level runs.
        if (!field.gone) {
            ring.quad = makeSprite(registry, "Magic Portals Shock Ring", image.path, entity.additive);
            placeShockRing(registry, ring);
        }
        m_shockRings.push_back(std::move(ring));
    }
    SUPERSONIC_LOG_INFO("Magic Portals") << "shock rings: " << m_shockRings.size() << " of " << image.path
                                         << " at z " << lookZ << ", " << (entity.additive ? "added" : "blended")
                                         << ", colour (" << rules.colour.x << ", " << rules.colour.y << ", "
                                         << rules.colour.z << ")" << std::endl;
}

void MagicPortalsLayer::placeShockRing(entt::registry& registry, const ShockRingQuad& ring) {
    if (ring.quad == entt::null || !registry.valid(ring.quad)) return;
    // AddToAngle turns it counter-clockwise on the screen from the 0 AddEntity gave
    // it, as the antiportal's does; its picture is symmetric to 1 %, so this moves
    // almost no pixel. Uniform on both axes: SetScale(vector2(s x f, s x f)).
    const double turnedRad = ring.turn.turnedDeg * 3.14159265358979323846 / 180.0;
    placeSprite(registry, ring.quad, ring.atPx, glm::dvec2(ring.baseUnits * ring.jitter), ring.z,
                Units::ToWorldRotation(-turnedRad));
}

void MagicPortalsLayer::syncShockRings(entt::registry& registry) {
    const std::vector<Fields::Field>& fields = m_level.fields.fields;
    for (ShockRingQuad& ring : m_shockRings) {
        if (ring.quad == entt::null) continue;
        if (ring.field < 0 || ring.field >= static_cast<int>(fields.size())) continue;
        const Fields::Field& field = fields[static_cast<std::size_t>(ring.field)];
        // deleteEntity(childID) takes the ring with the agent a fireball destroyed
        // (ETHCallback_shock_agent's isDestroyed arm), on the tick Fields marks it.
        if (field.gone) {
            if (registry.valid(ring.quad)) registry.destroy(ring.quad);
            ring.quad = entt::null;
            SUPERSONIC_LOG_INFO("Magic Portals")
                << "the ring round " << field.name << " goes with its agent on tick " << m_ticks << std::endl;
            continue;
        }
        // The area's own linearMotion, which Fields plays for the kill: the ring is
        // where its lethal centre is.
        ring.atPx = field.atPx;
        placeShockRing(registry, ring);
    }
}

void MagicPortalsLayer::jitterShockRings(entt::registry& registry) {
    // ETHCallback_shock_area #51-66, every frame: SetScale(base x (0.993 +
    // randF(0.014))), fresh about the base the agent stored and never about the last
    // frame's. It reads no time manager, and the engine runs the scene's callbacks on
    // every frame whatever the pause (ETHEngine.cpp:165-166), so it runs here outside
    // OnUpdate's pause gate. The draw is the effects' stream's (00_order K8): the
    // original's comes from one generator re-seeded from its clock every frame
    // (ETHScene.cpp:496), so no sequence of it is a parity value, and the placement
    // stream the level's own particles draw from is left as it was.
    const Art::ShockRing& rules = m_artRules.shockRing;
    for (ShockRingQuad& ring : m_shockRings) {
        if (ring.quad == entt::null || !registry.valid(ring.quad)) continue;
        ring.jitter = rules.JitterAt(effectRandom(0.0, rules.jitterSpan));
        ++ring.jitters;
        placeShockRing(registry, ring);
    }
}

std::vector<MagicPortalsLayer::ShockRingReport> MagicPortalsLayer::ShockRings() const {
    std::vector<ShockRingReport> reports;
    reports.reserve(m_shockRings.size());
    for (const ShockRingQuad& ring : m_shockRings) {
        ShockRingReport report;
        if (ring.field >= 0 && ring.field < static_cast<int>(m_level.fields.fields.size())) {
            report.field = m_level.fields.fields[static_cast<std::size_t>(ring.field)].name;
        }
        report.quad = ring.quad;
        report.texture = ring.texture;
        report.additive = m_shockRingEntity.additive;
        report.atPx = ring.atPx;
        report.baseUnits = ring.baseUnits;
        report.jitter = ring.jitter;
        report.jitters = ring.jitters;
        report.turnedDeg = ring.turn.turnedDeg;
        report.z = ring.z;
        report.lookZ = ring.lookZ;
        reports.push_back(std::move(report));
    }
    return reports;
}

bool MagicPortalsLayer::templatePicture(const Restock::Added& added, DrawnSprite& made) const {
    // The template's picture as buildSprites would make a placed one's: its sprite
    // by the reader the level's go through (the tier file, its units, its scale),
    // and its look - emissive, lit, its normal map - by Lighting::Read, over the
    // table's scene, whose root is this level's (Restock::LoadTemplates). Read when
    // the first one is added, not at the level's load, so no level that never
    // restocks draws or reads anything more. A table that will not read draws
    // nothing, and says why.
    std::vector<Sprites::Sprite> sprites;
    std::string why;
    if (!Sprites::Find(m_data.templates, m_paths.art, m_tierRules, sprites, why)) {
        SUPERSONIC_LOG_WARN("Magic Portals") << "a restocked " << added.templateNode
                                             << " is not drawn: its template's picture does not read: " << why
                                             << std::endl;
        return false;
    }
    const auto sprite = std::find_if(sprites.begin(), sprites.end(), [&added](const Sprites::Sprite& one) {
        return one.node == added.templateNode;
    });
    if (sprite == sprites.end()) return false;
    made.sprite = *sprite;
    made.sprite.node = added.name;
    made.sprite.atPx = added.atPx;
    made.sprite.rotation = 0.0;
    made.body = added.body;
    made.ownerPx = added.atPx;
    made.centrePx = Sprites::CentrePx(made.sprite);
    // AddEntity at the marker's vector3, so at its depth: an entity the script adds,
    // in the slot an effect at that depth takes (Slot::Effect, round(z)).
    made.z = slotAfter(std::round(added.z), Slot::Effect);
    made.lookZ = added.z;
    Lighting::Scene look;
    if (m_lit && Lighting::Read(m_data.templates, m_paths.art, m_tierRules, look, why)) {
        if (const auto found = look.nodes.find(added.templateNode); found != look.nodes.end()) {
            made.colour = glm::vec4(found->second.colour);
            made.emissive = found->second.emissive;
            made.isStatic = found->second.isStatic;
            made.applyLight = found->second.applyLight;
            made.normal = found->second.normal;
        }
    }
    return true;
}

void MagicPortalsLayer::syncSprites(entt::registry& registry) {
    using namespace Supersonic;
    rankSprites();
    // A restock the port could not build, said once a level (the original prints
    // 'Spawn entity not found ' for a missing marker, addEntityAtSpawnPoint #85-101;
    // the port also cannot build a template it has no node for).
    if (m_level.restock.refusals > 0 && !m_restockRefusalSaid) {
        m_restockRefusalSaid = true;
        SUPERSONIC_LOG_WARN("Magic Portals") << "a boss's restock was refused (" << m_level.restock.refusals
                                             << " so far): " << m_level.restock.firstRefusal << std::endl;
    }
    // THE BARRELS A BOSS PUTS BACK (sim/Restock), which buildSprites cannot have
    // made pictures for: each is added mid-fight under a name no node has. 4-32's
    // borrows the picture of the node it was built from - the placed barrel's,
    // whose entry stays in m_sprites after its own quad has gone. 3-32's has no
    // placed barrel to borrow from, so its picture is made from the port's template
    // (templatePicture). Each follows its own body as a placed one does. Made BEFORE
    // the loop below, which places it on the tick it appears, and by value, since
    // pushing into m_sprites moves it.
    while (m_artReady && m_restockDrawn < m_level.restock.added.size()) {
        const Restock::Added& added = m_level.restock.added[m_restockDrawn];
        ++m_restockDrawn;
        if (!registry.valid(added.body)) continue;
        if (added.templateFromPort) {
            DrawnSprite made;
            if (!templatePicture(added, made)) continue;
            made.quad = makeSprite(registry, "Magic Portals Restocked Sprite", made.sprite.texture, made.sprite.additive);
            registry.emplace<InterpolatedTransformComponent>(made.quad);
            m_sprites.push_back(std::move(made));
            continue;
        }
        int templateAt = -1;
        for (std::size_t i = 0; i < m_sprites.size(); ++i) {
            if (m_sprites[i].sprite.node == added.templateNode) {
                templateAt = static_cast<int>(i);
                break;
            }
        }
        if (templateAt < 0) continue;
        DrawnSprite made = m_sprites[static_cast<std::size_t>(templateAt)];
        made.sprite.node = added.name;
        made.sprite.atPx = added.atPx;
        made.sprite.rotation = 0.0;
        made.body = added.body;
        made.ownerPx = added.atPx;
        // Neither a bake nor a particle sidecar: a bake is the light that fell where
        // the template stood, read per entity already in the scene by its id; and
        // barrel_bomb.ent's <Particles /> is empty.
        made.lightmap.clear();
        made.particles.clear();
        // And never its template's quads: a barrel has no sky's cover, and a copy
        // that kept one would destroy it twice.
        made.cover = entt::null;
        // A barrel it adds blinks on its own clock, from the tick it is added (its blinkElapsedTime is
        // the entity's own), and from the floor the dark dragon sets on every one: SetVector3(
        // 'minEmissive', V3_ONE) (darkdragon.json restock, ins 1038-1063; art.json barrel_bomb).
        made.blinkBornMs = m_levelAgeMs;
        if (made.scriptBlink == DrawnSprite::ScriptBlink::Barrel) made.blinkFloor = m_artRules.barrelBomb.floorPresent;
        made.quad = makeSprite(registry, "Magic Portals Restocked Sprite", made.sprite.texture, made.sprite.additive);
        // A body, moved on the tick and drawn at its place, as buildSprites gives one.
        registry.emplace<InterpolatedTransformComponent>(made.quad);
        m_sprites.push_back(std::move(made));
    }
    for (DrawnSprite& drawn : m_sprites) {
        if (drawn.quad == entt::null) continue;
        const Sprites::Sprite& sprite = drawn.sprite;
        glm::dvec2 centrePx = Sprites::CentrePx(sprite);
        // Where its ENTITY stands, which is what a light it owns is placed from:
        // the sprite hangs off that point by its offset.
        glm::dvec2 ownerPx = sprite.atPx;
        glm::dvec2 sizePx = sprite.sizePx;
        float rotation = Units::ToWorldRotation(sprite.rotation);
        bool gone = false;
        double ownerAngleDeg = -sprite.rotation * 180.0 / 3.14159265358979323846;
        if (drawn.sky >= 0) {
            // Where StaticSky::position puts it from this tick's camera, whatever
            // the level said: the sprite's own offset kept, the pivot a scrolling
            // strip takes subtracted, and at the size scaleSky gave it.
            const auto t = static_cast<std::size_t>(drawn.sky);
            const glm::dvec2 view = ViewPx();
            ownerPx = Sky::PositionPx(m_skyRules, m_sky, t, m_follow.centrePx, view);
            centrePx += Sky::DrawnCentrePx(m_skyRules, m_sky, t, m_follow.centrePx, view) - sprite.atPx;
            sizePx = Sky::DrawnSizePx(m_sky, t);
        } else if (drawn.satellite) {
            ownerPx = Sky::SatellitePx(m_sky, m_follow.centrePx, ViewPx());
            centrePx += ownerPx - sprite.atPx;
        } else if (drawn.space != SpaceRole::None) {
            // Where SpaceSky::update puts its entity from this tick's camera, whatever
            // the level said, the picture hanging off it by its own offset: the planet's
            // 60 u up, the eclipse's 30. The spiral is the square scaleToSize made and
            // turned by what AddToAngle has added, about its own point (its pivot is 0).
            const glm::dvec2 view = ViewPx();
            switch (drawn.space) {
            case SpaceRole::Sky:
                ownerPx = Sky::SpaceSkyPx(m_spaceRules, m_follow.centrePx, view);
                break;
            case SpaceRole::Spiral: {
                ownerPx = Sky::SpiralPx(m_spaceRules, m_follow.centrePx, view);
                sizePx = glm::dvec2(m_space.spiralSidePx);
                const double turnedRad = m_space.spiralTurn.turnedDeg * 3.14159265358979323846 / 180.0;
                rotation = Units::ToWorldRotation(sprite.rotation - turnedRad);
                ownerAngleDeg += m_space.spiralTurn.turnedDeg;
                // 4-19's landing dims it (cutscenes.json rocket_landing #413-455): the
                // colour its first frame kept, SpaceSky's c, times the brightness.
                if (m_landing.running) {
                    const double dimmed =
                        m_space.spiralColour * Cutscene::SpiralBrightness(m_cutsceneRules, m_landing.bias);
                    drawn.colour = glm::vec4(glm::vec3(static_cast<float>(dimmed)), drawn.colour.a);
                }
                break;
            }
            case SpaceRole::Planet:
                ownerPx = Sky::PlanetPx(m_spaceRules, m_space, m_follow.centrePx, view);
                break;
            case SpaceRole::Eclipse:
                ownerPx = Sky::EclipsePx(m_spaceRules, m_space, m_follow.centrePx, view);
                break;
            case SpaceRole::None:
                break;
            }
            centrePx += ownerPx - sprite.atPx;
        } else if (drawn.crystal >= 0) {
            // A timed crystal does not fade: the original goes at its time, and
            // counts it down on the dial behind it (syncTimers).
            const Goals::Crystal& crystal = m_level.goals.crystals[static_cast<std::size_t>(drawn.crystal)];
            gone = crystal.collected || crystal.expired;
            // Where its bob has put it (advanceMotions, this tick): picture and
            // sparkles together, and its dial after them. The pick-up box stays on
            // the node: the picture moves, not the crystal Goals takes.
            if (!gone && drawn.motion >= 0) {
                const glm::dvec2 bob = Motion::OffsetPx(m_motions[static_cast<std::size_t>(drawn.motion)]);
                centrePx += bob;
                ownerPx += bob;
            }
        } else if (drawn.key >= 0) {
            // A key is not a body (Keys.hpp): where the carry left it, picture and
            // sparkle together. The fly-in it plays once spent is not built
            // (keys.json _not_built), so a spent key holds where it opened its
            // keyhole and goes, sparkles and all, when that keyhole goes: no key is
            // left lying where a door stood.
            const Keys::Key& key = m_level.keys.keys[static_cast<std::size_t>(drawn.key)];
            gone = key.spent && key.opened >= 0 && key.opened < static_cast<int>(m_level.keys.keyholes.size()) &&
                   m_level.keys.keyholes[static_cast<std::size_t>(key.opened)].gone;
            centrePx += key.atPx - sprite.atPx;
            // The light its callback adds stands where the PREVIOUS call left the key (AddLight is before
            // linearMotion and the carry, #119-186 against #666-679 and #932-976), and drawn.ownerPx still
            // holds that: it is written at the end of this loop.
            if (drawn.keyLight >= 0) m_keyLights[static_cast<std::size_t>(drawn.keyLight)].atPx = drawn.ownerPx;
            ownerPx = key.atPx;
            // Lying unowned, it bobs about where it lies: where the level put it, or
            // where it was dropped (ETHCallback_key rewrites originalPos on every
            // carried frame). Carried or spent, it does not.
            if (!gone && motionRuns(drawn)) {
                const glm::dvec2 bob = Motion::OffsetPx(m_motions[static_cast<std::size_t>(drawn.motion)]);
                centrePx += bob;
                ownerPx += bob;
            }
        } else if (drawn.diamond >= 0) {
            const Diamonds::Diamond& diamond = m_level.diamonds.diamonds[static_cast<std::size_t>(drawn.diamond)];
            gone = diamond.gone;
            centrePx += diamond.atPx - sprite.atPx;
            ownerPx = diamond.atPx;
            // CARRIED, NEARLY INVISIBLE (art.json carried_diamond): the carried arm's
            // SetAlpha(0.1f), from the update after the pick - whose free arm drew it
            // whole - until it goes or a boss puts it back as a new entity. Its
            // systems are not faded: SetAlpha is the sprite's alone.
            const auto index = static_cast<std::size_t>(drawn.diamond);
            const bool faded = index < m_diamondEffects.size() && m_diamondEffects[index].faded;
            drawn.fade = faded ? static_cast<float>(m_artRules.carriedDiamond.alpha) : 1.0f;
        } else if (drawn.keyhole >= 0) {
            // It holds still, fades over keys.json's fade and deletes itself.
            const Keys::Keyhole& keyhole = m_level.keys.keyholes[static_cast<std::size_t>(drawn.keyhole)];
            gone = keyhole.gone;
            drawn.fade = static_cast<float>(keyhole.alpha);
        } else if (drawn.staticPortal >= 0) {
            gone = !m_level.portals.statics[static_cast<std::size_t>(drawn.staticPortal)].live;
        } else if (drawn.zone >= 0) {
            // A patrolling zone carries its picture with it, and a ring an agent's
            // death deleted takes it away (step 126).
            const Portals::NoPortalZone& zone = m_level.portals.zones[static_cast<std::size_t>(drawn.zone)];
            gone = zone.gone;
            centrePx += zone.CentreNowPx() - zone.centrePx;
            ownerPx += zone.CentreNowPx() - zone.centrePx;
        } else if (drawn.agent >= 0) {
            // AN ANTI-PORTAL AGENT: where it patrols, and gone - its particles with
            // it - once a shot has killed it (ETHCallback_anti_portal_agent
            // #134-142 DeleteEntity(thisEntity)).
            const Portals::Agent& agent = m_level.portals.agents[static_cast<std::size_t>(drawn.agent)];
            gone = agent.gone;
            centrePx += agent.CentreNowPx() - agent.centrePx;
            ownerPx += agent.CentreNowPx() - agent.centrePx;
        } else if (drawn.placed >= 0) {
            // Where its own callback put it this tick (advanceMotions): swung by its
            // linearMotion, and turned by AddToAngle about its own point,
            // counter-clockwise as Ethanon's angle and the engine's +z both turn.
            const PlacedScript& script = m_placed[static_cast<std::size_t>(drawn.placed)];
            // A tutorial hand a release took away (tapHandsAway): DeleteEntity.
            gone = script.tapped;
            if (script.moves) {
                const glm::dvec2 swing = Motion::OffsetPx(script.motion);
                centrePx += swing;
                ownerPx += swing;
            }
            if (script.turn.turnedDeg != 0.0) {
                const double turnedRad = script.turn.turnedDeg * 3.14159265358979323846 / 180.0;
                rotation = Units::ToWorldRotation(sprite.rotation - turnedRad);
                ownerAngleDeg += script.turn.turnedDeg;
            }
        } else if (drawn.handle >= 0) {
            // A DRAGGABLE HANDLE (sim/Handle.hpp): where its drag has put it, at its
            // callback's alpha - SetAlpha(0.3) at rest and 1 while moved, over a
            // colour its node does not give. Its bounce is its size, below.
            const Handle::Handle& handle = m_level.handles.handles[static_cast<std::size_t>(drawn.handle)];
            centrePx += handle.atPx - sprite.atPx;
            ownerPx = handle.atPx;
            if (handle.bounced) drawn.fade = static_cast<float>(m_level.handles.Alpha(handle));
        } else if (drawn.follower >= 0) {
            // AND WHAT FOLLOWS ONE: a fire agent or a reflector where its followUp has
            // eased it - picture, flame, halo and light together, as they hang off the
            // entity's point.
            const Handle::Follower& follower = m_level.handles.followers[static_cast<std::size_t>(drawn.follower)];
            centrePx += follower.atPx - sprite.atPx;
            ownerPx += follower.atPx - sprite.atPx;
        } else if (drawn.hint == HintRole::Lesson) {
            // 2-20's lesson hand (handle.json `lesson`): along its round from where the
            // level put it, at its waypoints' alpha once its callback has run, until
            // its handle is first held.
            gone = m_lesson.gone;
            const glm::dvec2 offset = m_lesson.OffsetUnits(m_data.handle.lessonWaypoints);
            centrePx += offset;
            ownerPx += offset;
            if (m_lesson.ticked) drawn.fade = static_cast<float>(m_lesson.Alpha(m_data.handle));
        } else if (drawn.hint != HintRole::None) {
            // 1-21's lesson (sim/Hint.hpp): the hand from where the level put it along
            // its waypoints, each picture at its callback's alpha - SetAlpha, over a
            // colour neither node gives - until it deletes itself. The arrow's pulse
            // is its size, below.
            if (drawn.hint == HintRole::Hand) {
                gone = m_hintHand.gone;
                const glm::dvec2 offset = m_hintHand.OffsetUnits(m_hintRules);
                centrePx += offset;
                ownerPx += offset;
                drawn.fade = static_cast<float>(m_hintHand.Alpha(m_hintRules));
            } else {
                gone = m_hintArrow.gone;
                drawn.fade = static_cast<float>(m_hintArrow.Alpha(m_hintRules));
            }
        } else if (drawn.dragon) {
            // 2-32'S BOSS, drawn where the sim flies it and turned the way its
            // callback turns it (sim/Dragon.hpp). Its picture hangs off the entity
            // by PivotAdjust, and Ethanon rotates a sprite about its ORIGIN - which
            // is the entity's own point - so the offset turns with the angle.
            // `gone` never: the original keeps drawing it after it has taken off.
            const Dragon::State& dragon = m_level.dragon;
            ownerPx = dragon.atPx;
            ownerAngleDeg = dragon.angleDeg;
            centrePx = ownerPx + Particles::Turn(sprite.offsetPx, ownerAngleDeg);
            rotation = Units::ToWorldRotation(-ownerAngleDeg * 3.14159265358979323846 / 180.0);
            if (m_dragonCut.frameUnits.x > 0 && m_dragonCut.frameUnits.y > 0) {
                sizePx = glm::dvec2(m_dragonCut.frameUnits);
                // The cell its callback last set, as the original's rect for it:
                // frame f is column f % columns, row f / columns
                // (gs2d Sprite.cpp:104-136), and the uv is written here rather than
                // by a flipbook's even split, so one rule cuts every sheet.
                const int columns = std::max(1, m_data.dragon.columns);
                auto& material = registry.get<MaterialComponent>(drawn.quad);
                material.uvScale = glm::vec2(m_dragonCut.uvScale);
                material.uvOffset =
                    glm::vec2(Tiers::UvOffset(m_dragonCut, dragon.frame % columns, dragon.frame / columns));
            }
        } else if (drawn.claw) {
            // AND ITS CLAW, which is never where its node is: its waypoints are
            // measured from the camera's left edge, and on this one level the
            // camera is the dragon. dragon.json's `claw` has the six points.
            const Dragon::State& dragon = m_level.dragon;
            ownerPx = dragon.claw.AtPx(dragon.rules, dragon.CameraLeftPx());
            ownerAngleDeg = dragon.claw.AngleDeg(dragon.rules);
            centrePx = ownerPx + Particles::Turn(sprite.offsetPx, ownerAngleDeg);
            rotation = Units::ToWorldRotation(-ownerAngleDeg * 3.14159265358979323846 / 180.0);
        } else if (drawn.body != entt::null) {
            // A body the level took away - a wall a stone broke - takes its picture.
            gone = !registry.valid(drawn.body);
            if (!gone) {
                const auto& body = registry.get<TransformComponent>(drawn.body);
                rotation = body.rotation.z;
                // The offset turned with the body, its y flipped on the way to metres.
                const glm::vec2 offset(Units::ToMetres(sprite.offsetPx.x), Units::ToMetres(-sprite.offsetPx.y));
                const glm::vec2 turned(offset.x * std::cos(rotation) - offset.y * std::sin(rotation),
                                       offset.x * std::sin(rotation) + offset.y * std::cos(rotation));
                centrePx =
                    Units::ToPixels(glm::vec3(body.position.x + turned.x, body.position.y + turned.y, 0.0f));
                ownerPx = Units::ToPixels(body.position);
                // The engine's +z turn is counter-clockwise on the screen, as Ethanon's angle is.
                ownerAngleDeg = static_cast<double>(rotation) * 180.0 / 3.14159265358979323846;
            }
        }
        // WHAT ITS OWN CALLBACK HAS TURNED IT BY (art.json antiportal): a ring turns
        // about its own point, over whatever angle the level gave it,
        // counter-clockwise as Ethanon's angle and the engine's +z both turn. Its
        // picture is symmetric, so this moves almost no pixel; it is still what the
        // original does, and a sheet or a tier that is not symmetric would show it.
        if (drawn.antiportal) {
            const double turnedRad = m_antiportalTurn.turnedDeg * 3.14159265358979323846 / 180.0;
            rotation = Units::ToWorldRotation(sprite.rotation - turnedRad);
            ownerAngleDeg += m_antiportalTurn.turnedDeg;
        }
        // AND WHERE GAME::LOOP'S LINEAR MOTION HAS PUT IT (motions.json door): the
        // exit door of a `no_gravity` level, the black behind it and the glow behind
        // that, one unit either way on a 5.24 s cycle. Picture and particles
        // together - the doorway's fire hangs off its entity's point - and after the
        // branches above, so it lands on whatever put the picture there. The port
        // leaves the exit's own sensor where the level put it (`door._not_built`).
        if (drawn.doorBob) {
            const glm::dvec2 bob = Motion::OffsetPx(m_doorBob);
            centrePx += bob;
            ownerPx += bob;
        }
        // AND WHERE A WIN CUTSCENE HAS PUT IT (cutscenes.json): 4-01's rocket door
        // and bridge where rocket_launch set them, and 4-19's door at the originalPos
        // rocket_landing writes, which the bob above rides on. Picture and systems
        // together - the exhaust hangs off the door's point. Only the pictures: the
        // exit's sensor and the bridge's box stay where the level put them, as the
        // bob leaves the sensor, and nothing after the win walks on either.
        const int drawnIndex = static_cast<int>(&drawn - m_sprites.data());
        if (drawnIndex == m_doorSprite) {
            glm::dvec2 moved(0.0);
            if (m_launch.running) moved = m_launch.pose.doorOffsetPx;
            if (m_landing.running) moved = Cutscene::LandingDoorOffsetPx(m_cutsceneRules, m_landing.bias);
            centrePx += moved;
            ownerPx += moved;
        }
        if (drawnIndex == m_bridgeSprite && m_launch.running) {
            centrePx += m_launch.pose.bridgeOffsetPx;
            ownerPx += m_launch.pose.bridgeOffsetPx;
        }
        // And the key the landing's SeekEntity('key') finds: Scale(0.9) a frame about
        // its own point, and its alpha that scale (#185-227). Its systems shrink with
        // it in updateEmitters.
        if (drawnIndex == m_landingKeySprite && m_landing.running) {
            centrePx = ownerPx + (centrePx - ownerPx) * m_landing.keyScale;
            sizePx *= m_landing.keyScale;
            drawn.fade = static_cast<float>(std::clamp(m_landing.keyScale, 0.0, 1.0));
        }
        // A TORCH THE LEVEL PLACED goes on the tick after a shot lights it, its
        // picture and its spark with it: its callback's `delete` branch deletes the
        // entity then (the visuals plan's 11.2). The torch a put-out adds back is a
        // new entity, the effects runtime's, and never this picture again.
        if (drawn.torch >= 0 && drawn.torch < static_cast<int>(m_torchWatch.size()) &&
            m_torchWatch[static_cast<std::size_t>(drawn.torch)].unlitGone) {
            gone = true;
        }
        // And while it stands, its callback's AddToAngle(unitsPerSecond(190)) over
        // its node's angle (art.json unlit_torch): light_off.ent is ET_VERTICAL, so
        // its picture is drawn unturned and only its systems - starting 12 units out
        // - go round (ETHRenderEntity.cpp:105, ETHSpriteEntity.cpp:576).
        if (drawn.torch >= 0 && drawn.torch < static_cast<int>(m_torchWatch.size())) {
            ownerAngleDeg += m_torchWatch[static_cast<std::size_t>(drawn.torch)].spin.turnedDeg;
        }
        // AND A GRAVITY WELL'S AGENT, as its callback sets its angle every frame
        // (gravitywell.json roundabout): a whole turn at rest, which draws as none,
        // and one turn and a settle over 3000 ms in the way the player last lapped
        // it. Counter-clockwise on the screen, as Ethanon's angle and the engine's
        // +z both turn; its particles with it.
        if (drawn.well >= 0) {
            const double turnedDeg = std::fmod(WellSpinDegrees(drawn.well), 360.0);
            if (turnedDeg != 0.0) {
                rotation += static_cast<float>(turnedDeg * 3.14159265358979323846 / 180.0);
                ownerAngleDeg += turnedDeg;
            }
        }
        // 4-19's red key state goes when its fade ends: DeleteEntity(thisEntity), #190.
        if (drawn.redKeyState && m_level.redKey.gone) gone = true;
        // THE HELP BLOCK'S SetAlpha (art.json help_block; ETHCallback_help_block #1-3, every update): the
        // sprite's alpha, over the colour its node gives (none) and drawn by syncLighting as every fade
        // is. Its bounce is its size, below.
        if (drawn.helpBlock) drawn.fade = static_cast<float>(m_artRules.helpBlock.alpha);
        if (gone) {
            registry.destroy(drawn.quad);
            drawn.quad = entt::null;
            if (drawn.cover != entt::null && registry.valid(drawn.cover)) registry.destroy(drawn.cover);
            drawn.cover = entt::null;
            continue;
        }
        // At the scale a script gave its entity: 1 but for a static portal's 0.8 and
        // an antiportal's 0.5 times its node's scale - and per axis for 4-19's red key
        // state, whose Scale(vector2) stretches it (0.8, 1.5).
        glm::dvec2 drawnSizePx = sizePx * drawn.scale;
        if (drawn.redKeyState) drawnSizePx *= m_level.redKey.rules.scale;
        // And 1-21's arrow at bounce()'s scale per axis (sim/Hint.hpp).
        if (drawn.hint == HintRole::Arrow) drawnSizePx *= m_hintArrow.Scale(m_hintRules);
        // And a handle at bounce()'s (sim/Handle.hpp).
        if (drawn.handle >= 0) {
            drawnSizePx *= m_level.handles.Scale(m_level.handles.handles[static_cast<std::size_t>(drawn.handle)]);
        }
        // And a shock agent at its own bounce's (art.json shock_ring agent_pulse): 1
        // to 0.95 and back, 300 ms a leg, on the level's age as the antiportal's
        // blink - about its own point, its pivot 0, and its sparks untouched.
        if (drawn.field >= 0) drawnSizePx *= m_artRules.shockRing.agentPulse.ScaleAt(m_levelAgeMs);
        // And a fire agent at its own (art.json fire_agent pulse): bounce(this, 1.02, 0.98, 300) SETS
        // the picture's scale to the pulse, its flames untouched (its light and halo take the same scale: syncLights).
        // fire_agent.ent's <Scale> is 1 and no placement carries a scale datum, so the picture's own size times the
        // pulse is it.
        if (drawn.firePulse) drawnSizePx *= m_artRules.fireAgent.pulse.ScaleAt(m_levelAgeMs - drawn.blinkBornMs);
        // And a help block at its own (art.json help_block): bounce(this, 1.13, 1.05, 250) SETS the
        // picture's scale, 1.13 at 0 ms and 1.05 at 250, on the level's age. help_block.ent's <Scale> is 1
        // and no placement carries one, so the picture's own size times the pulse is it; its tap rect
        // (HelpBlockRects) is made from the node and trigger_size, never from this size.
        if (drawn.helpBlock) drawnSizePx *= m_artRules.helpBlock.pulse.ScaleAt(m_levelAgeMs - drawn.blinkBornMs);
        drawn.drawnSizePx = drawnSizePx;
        placeSprite(registry, drawn.quad, centrePx, drawnSizePx, drawn.z, rotation);
        if (drawn.cover != entt::null) placeSkyCover(registry, drawn, centrePx, drawnSizePx, rotation);
        drawn.centrePx = centrePx;
        drawn.ownerPx = ownerPx;
        drawn.ownerAngleDeg = ownerAngleDeg;
    }

    // THE PLATFORM THE DARK DRAGON'S DEATH ADDS, which buildSprites cannot have
    // made a picture for: it has no node, and Sprites::Find reads the scene.
    // entities/ holds single_block_plat_no_emissive.ENT and no .png, so the
    // picture is borrowed from the sibling the level already places - the same
    // node Game sized the box from. Added AFTER the loop above, and by index,
    // because pushing into m_sprites while ranging over it would invalidate it.
    const entt::entity platform = m_level.darkDragon.platformBody;
    if (!m_platformDrawn && m_artReady && platform != entt::null && registry.valid(platform)) {
        int templateAt = -1;
        for (std::size_t i = 0; i < m_sprites.size(); ++i) {
            if (m_sprites[i].sprite.node == m_level.darkDragon.platformNode) {
                templateAt = static_cast<int>(i);
                break;
            }
        }
        if (templateAt >= 0) {
            const DrawnSprite& from = m_sprites[static_cast<std::size_t>(templateAt)];
            DrawnSprite made;
            made.sprite = from.sprite;
            made.sprite.node = m_level.darkDragon.name + "#platform";
            // Where the body actually is, and no offset: Game already folded the
            // template's shape offset into the body's own position.
            made.sprite.atPx = Units::ToPixels(registry.get<TransformComponent>(platform).position);
            made.sprite.offsetPx = glm::dvec2(0.0);
            made.sprite.rotation = 0.0;
            made.z = from.z;
            // And its look: the platform the template's .ent is, lit as it is.
            made.colour = from.colour;
            made.emissive = from.emissive;
            made.isStatic = from.isStatic;
            made.applyLight = from.applyLight;
            made.normal = from.normal;
            made.lookZ = from.lookZ;
            made.ownerPx = made.sprite.atPx;
            // But NOT its lightmap, which `made` leaves empty. A bake is the light
            // that fell where the template stands, and the original reads one per
            // entity already in the scene, named by its id (ETHScene.cpp:315-334,
            // add<id> at ETHSpriteEntity.cpp:437); this platform is added when the
            // dragon dies, long after, under an id no file names.
            made.quad = makeSprite(registry, "Magic Portals Dropped Platform", made.sprite.texture,
                                   made.sprite.additive);
            m_sprites.push_back(std::move(made));
            m_platformDrawn = true;
        }
    }
}

bool MagicPortalsLayer::motionRuns(const DrawnSprite& drawn) const {
    if (drawn.motion < 0 || drawn.quad == entt::null) return false;
    if (drawn.crystal >= 0) {
        // ETHCallback_crystal runs while the crystal is there; taken or expired, it
        // is deleted.
        const Goals::Crystal& crystal = m_level.goals.crystals[static_cast<std::size_t>(drawn.crystal)];
        return !crystal.collected && !crystal.expired;
    }
    if (drawn.key >= 0) {
        // ETHCallback_key ins 204-220 send a key that found its keyhole to its fly-in,
        // and ins 653-665 a carried one to the carried branch: neither bobs.
        const Keys::Key& key = m_level.keys.keys[static_cast<std::size_t>(drawn.key)];
        return key.owner == entt::null && !key.spent;
    }
    return false;
}

void MagicPortalsLayer::advanceMotions(const ScriptClock::Step& step) {
    // The tick is the frame the callbacks run on. A paused or popped-up level is not
    // stepped, which is the original's m_factor 0: the angle holds.
    const double frameMs = ScriptClock::Ms(step);
    // ETHCallback_gravity_agent's effectAngleTime, getLastFrameElapsedTime a frame
    // (gravitywell.json roundabout); a lap zeroes it (latchSimEffects).
    for (WellSpin& spin : m_wellSpins) spin.ms += frameMs;
    for (const DrawnSprite& drawn : m_sprites) {
        if (!motionRuns(drawn)) continue;
        Motion::Advance(m_motionRules, m_motions[static_cast<std::size_t>(drawn.motion)], frameMs);
    }
    // The placed pictures' callbacks run every frame their picture is there: the
    // arrow's, the circle's and the hand's linearMotion, and the circle's AddToAngle.
    for (const DrawnSprite& drawn : m_sprites) {
        if (drawn.placed < 0 || drawn.quad == entt::null) continue;
        PlacedScript& script = m_placed[static_cast<std::size_t>(drawn.placed)];
        if (script.moves) Motion::Advance(m_motionRules, script.motion, frameMs);
        Motion::Advance(m_motionRules, script.turn, frameMs);
    }
    // And each unlit torch's AddToAngle(unitsPerSecond(190)), while that entity
    // stands - the level's own until the tick after it is lit, then the one a
    // put-out adds (art.json unlit_torch; latchTorches starts it again at 0).
    for (TorchWatch& watch : m_torchWatch) {
        if (!watch.unlitGone) Motion::Advance(m_motionRules, watch.spin, frameMs);
    }
    // 1-21's hand and arrow (sim/Hint.hpp), each on its own callback's clock until
    // it deletes itself.
    if (m_hintHandPlaced) m_hintHand.Tick(m_hintRules, frameMs);
    if (m_hintArrowPlaced) m_hintArrow.Tick(m_hintRules, frameMs);
    // 2-20's lesson hand (handle.json `lesson`), after the handles' callbacks (an
    // always-active one earlier in the walk): its round, and gone - with
    // smoke_vanish 32 above it - the tick its handle is first held.
    if (m_lesson.placed && !m_lesson.gone) {
        const bool held = m_lessonHandle >= 0 && m_lessonHandle < static_cast<int>(m_level.handles.handles.size()) &&
                          m_level.handles.handles[static_cast<std::size_t>(m_lessonHandle)].moving;
        m_lesson.Tick(m_data.handle, held, frameMs);
        if (m_lesson.vanishedNow) {
            double lookZ = 0.0;
            for (const DrawnSprite& drawn : m_sprites) {
                if (drawn.hint == HintRole::Lesson) lookZ = drawn.lookZ;
            }
            m_effectQueue.push_back(
                effectAdd(Effects::OnEntity(m_effectRules.smokeVanish, m_lesson.AtPx(m_data.handle), lookZ)));
            SUPERSONIC_LOG_INFO("Magic Portals") << "the handle lesson goes on tick " << m_ticks << std::endl;
        }
    }
    // AND GAME::LOOP'S OWN CALL on a `no_gravity` level (motions.json door): one
    // step a tick for the door, the black behind it and the glow, which share an
    // angle because the original steps all three in that one loop with that one
    // frame. It runs whether or not the level has a glow or a background to bob.
    if (m_doorBobRuns) Motion::Advance(m_motionRules, m_doorBob, frameMs);
    // AND SPACESKY'S UPDATE on a space level (sky.json space_sky), from the camera
    // this tick left: the planet's linearMotion step, the spiral's AddToAngle and
    // the eclipse's y, before the pictures are placed from them.
    Sky::AdvanceSpace(m_motionRules, m_space, m_follow.centrePx, ViewPx(), frameMs);
    // And 4-19's landing, a static callback after SpaceSky's update in the frame, over
    // the eclipse's y: from its own height below the view's bottom up to it, as
    // smoothEnd(bias) (cutscenes.json rocket_landing #343-411). Its x stays SpaceSky's.
    if (m_landing.running && !m_space.eclipseNode.empty()) {
        double eclipseHeightPx = 0.0;
        for (const DrawnSprite& drawn : m_sprites) {
            if (drawn.space == SpaceRole::Eclipse) eclipseHeightPx = drawn.sprite.sizePx.y * drawn.scale;
        }
        const glm::dvec2 view = ViewPx();
        m_space.eclipseYPx = Cutscene::LandingEclipseYPx(m_cutsceneRules, m_landing.bias,
                                                         (m_follow.centrePx - view * 0.5).y, view.y, eclipseHeightPx);
    }
    // A key no node pictures bobs as a level key does: ETHCallback_key while no one
    // carries it and it has not found its keyhole.
    for (DroppedKey& dropped : m_droppedKeys) {
        if (dropped.key < 0 || dropped.key >= static_cast<int>(m_level.keys.keys.size())) continue;
        const Keys::Key& key = m_level.keys.keys[static_cast<std::size_t>(dropped.key)];
        if (key.owner == entt::null && !key.spent) Motion::Advance(m_motionRules, dropped.bob, frameMs);
    }
    // And ETHCallback_antiportal's AddToAngle, once for every ring the level placed:
    // they are turned by the same step on the same frames, so the port keeps one
    // angle for them (m_antiportalTurn). Its step is unitsPerSecond's, capped at
    // motions.json's frame_cap_ms, where the blink's clock (the level's age) is not:
    // getLastFrameElapsedTime is not capped at 200 - only at the loop's own 1000 ms
    // (Min(1000.0f, ComputeElapsedTimeF(video)), android/main.cpp:180-181; art.json
    // antiportal).
    Motion::Advance(m_motionRules, m_antiportalTurn, frameMs);
    // And ETHCallback_shock_area's AddToAngle(unitsPerSecond(20)) for every ring
    // still standing: game time, so it holds under a pause, where no tick runs. Its
    // scale is the FRAME's (jitterShockRings), which a pause does not hold.
    for (ShockRingQuad& ring : m_shockRings) {
        if (ring.quad != entt::null) Motion::Advance(m_motionRules, ring.turn, frameMs);
    }
    // AND ETHCallback_light_wall_ghost's two clocks, while the wall it replaces is
    // flagged or gone (art.json light_wall_ghost). The tick the torch is lit only
    // adds the entity - the original's callback cannot have run before the frame
    // that maps it - so the first step is taken on the tick after, and that is the
    // first tick its picture is drawn.
    //
    // The two clocks do not run at one rate. The angle is AddToFloat(
    // unitsPerSecond(300)), which caps a long frame at motions.json's frame_cap_ms;
    // bounce's blinkElapsedTime adds getLastFrameElapsedTime, which is not capped
    // (art.json antiportal). At the port's fixed tick they are the same number.
    const Torch::Wall& wall = m_level.torch.wall;
    if (!wall.present || !(wall.going || wall.gone)) {
        m_wallGhost.added = false;
        m_wallGhost.drawn = false;
    } else if (!m_wallGhost.added) {
        m_wallGhost.added = true;
    } else {
        Motion::Advance(m_motionRules, m_wallGhost.turn, frameMs);
        m_wallGhost.pulseMs += frameMs;
        m_wallGhost.drawn = true;
    }
}

std::vector<MagicPortalsLayer::PlacedReport> MagicPortalsLayer::PlacedReports() const {
    std::vector<PlacedReport> reports;
    reports.reserve(m_placed.size());
    for (const DrawnSprite& drawn : m_sprites) {
        if (drawn.placed < 0) continue;
        const PlacedScript& script = m_placed[static_cast<std::size_t>(drawn.placed)];
        PlacedReport report;
        report.node = drawn.sprite.node;
        report.entity = m_motionRules.placed[script.row].entity;
        report.drawn = drawn.quad != entt::null;
        report.quad = drawn.quad;
        report.moves = script.moves;
        report.motion = script.motion;
        report.offsetPx = script.moves ? Motion::OffsetPx(script.motion) : glm::dvec2(0.0);
        report.turnedDeg = script.turn.turnedDeg;
        report.alpha = drawn.colour.a * drawn.fade;
        report.centrePx = drawn.centrePx;
        report.ownerPx = drawn.ownerPx;
        report.ownerAngleDeg = drawn.ownerAngleDeg;
        reports.push_back(std::move(report));
    }
    return reports;
}

std::vector<MagicPortalsLayer::HandleReport> MagicPortalsLayer::HandleReports() const {
    std::vector<HandleReport> reports;
    for (const DrawnSprite& drawn : m_sprites) {
        if (drawn.handle < 0) continue;
        HandleReport report;
        report.node = drawn.sprite.node;
        report.drawn = drawn.quad != entt::null;
        report.centrePx = drawn.centrePx;
        report.sizePx = drawn.sprite.sizePx * drawn.scale *
                        m_level.handles.Scale(m_level.handles.handles[static_cast<std::size_t>(drawn.handle)]);
        report.alpha = drawn.colour.a * drawn.fade;
        reports.push_back(report);
    }
    return reports;
}

std::vector<MagicPortalsLayer::FollowerReport> MagicPortalsLayer::FollowerReports() const {
    std::vector<FollowerReport> reports;
    for (const DrawnSprite& drawn : m_sprites) {
        if (drawn.follower < 0) continue;
        FollowerReport report;
        report.node = drawn.sprite.node;
        report.drawn = drawn.quad != entt::null;
        report.centrePx = drawn.centrePx;
        report.ownerPx = drawn.ownerPx;
        reports.push_back(report);
    }
    return reports;
}

MagicPortalsLayer::LessonReport MagicPortalsLayer::LessonState() const {
    LessonReport report;
    report.placed = m_lesson.placed;
    report.lesson = m_lesson;
    for (const DrawnSprite& drawn : m_sprites) {
        if (drawn.hint != HintRole::Lesson) continue;
        report.drawn = drawn.quad != entt::null;
        report.centrePx = drawn.centrePx;
        report.alpha = drawn.colour.a * drawn.fade;
    }
    return report;
}

MagicPortalsLayer::HintReport MagicPortalsLayer::HintState() const {
    HintReport report;
    report.handPlaced = m_hintHandPlaced;
    report.arrowPlaced = m_hintArrowPlaced;
    report.hand = m_hintHand;
    report.arrow = m_hintArrow;
    for (const DrawnSprite& drawn : m_sprites) {
        if (drawn.hint == HintRole::Hand) {
            report.handDrawn = drawn.quad != entt::null;
            report.handCentrePx = drawn.centrePx;
            report.handAlpha = drawn.colour.a * drawn.fade;
        } else if (drawn.hint == HintRole::Arrow) {
            report.arrowDrawn = drawn.quad != entt::null;
            report.arrowAlpha = drawn.colour.a * drawn.fade;
        }
    }
    return report;
}

std::vector<MagicPortalsLayer::MotionReport> MagicPortalsLayer::MotionReports() const {
    std::vector<MotionReport> reports;
    reports.reserve(m_motions.size());
    for (const DrawnSprite& drawn : m_sprites) {
        if (drawn.motion < 0) continue;
        MotionReport report;
        report.node = drawn.sprite.node;
        report.crystal = drawn.crystal >= 0;
        report.drawn = drawn.quad != entt::null;
        report.quad = drawn.quad;
        report.moving = motionRuns(drawn);
        report.motion = m_motions[static_cast<std::size_t>(drawn.motion)];
        report.offsetPx = report.moving ? Motion::OffsetPx(report.motion) : glm::dvec2(0.0);
        report.centrePx = drawn.centrePx;
        report.ownerPx = drawn.ownerPx;
        reports.push_back(std::move(report));
    }
    return reports;
}

// ETHCallback_timer, once a tick (art.json timer has the bytecode). The port's
// tick is the frame the script runs on, and the crystal's clock is Goals', whose
// time left counts down from the level's first tick as the dial's elapsedTime
// counts up from the frame it is added on. While the time lasts: the crystal's
// drawn centre, the cell of the time elapsed, and the pulse whose leg shortens as
// it runs out. Once the time is up, on the tick Goals expires the crystal, the
// dial keeps its last place, cell and scale, shrinks and fades a frame's worth a
// tick, and goes once its scale as the original stores it, x m_scaleFactor, is
// below goneBelowScale (Art::Timer::Gone). A crystal taken early gives the dial one
// more live tick first (below), and shrinks it from the next. The crystal_vanish
// burst at the time is latchSimEffects' (the visuals plan's 10.2), and a crystal
// taken early gets crystal_pick there and no burst.
void MagicPortalsLayer::syncTimers(entt::registry& registry) {
    using namespace Supersonic;
    const Art::Timer& timer = m_artRules.timer;
    // ETHCallback_timer's elapsedTime adds getLastFrameElapsedTime (ins 1-13): the script's floor.
    const double tickMs = ScriptClock::Ms(m_step);
    for (TimerDial& dial : m_timers) {
        if (dial.quad == entt::null) continue;
        const Goals::Crystal& crystal = m_level.goals.crystals[static_cast<std::size_t>(dial.crystal)];
        const double timeMs = crystal.lifeMs;
        double elapsedMs = crystal.elapsedMs;
        if (crystal.expired || dial.taken) {
            dial.shrinking = true;
        } else if (crystal.collected) {
            // TAKEN THIS TICK. ETHCallback_timer adds the frame to elapsedTime (ins
            // 1-13) and copies it into a local (ins 24-27) before it looks for the
            // crystal. Finding it gone, it adds the whole time to the datum (ins
            // 80-91), skips SetPositionXY (ins 92) and compares the LOCAL (ins
            // 102-103): this frame still pulses and sets its cell (ins 204-252)
            // where the dial stood, and the x 0.9 starts on the next. Goals stops
            // a crystal's clock on the tick it is taken, where the script's local
            // already holds this frame: its elapsed is Goals' and one tick more.
            // Taken on the tick its time runs out, that local is past the time,
            // and the dial shrinks at once (ins 102 is strict).
            dial.taken = true;
            elapsedMs += tickMs;
            if (elapsedMs > timeMs) dial.shrinking = true;
        }
        if (dial.shrinking) {
            const double decay = timer.DecayOver(kTick);
            dial.scale *= decay;
            dial.alpha *= decay;
            if (timer.Gone(dial.scale.x)) {
                registry.destroy(dial.quad);
                dial.quad = entt::null;
                continue;
            }
        } else {
            if (!dial.taken) dial.atPx = m_sprites[static_cast<std::size_t>(dial.sprite)].centrePx;
            dial.frame = timer.FrameAt(elapsedMs, timeMs);
            dial.scale = timer.PulseAt(elapsedMs, timeMs);
            dial.alpha = timer.alpha;
        }
        auto& animation = registry.get<SpriteAnimationComponent>(dial.quad);
        animation.firstFrame = static_cast<uint32_t>(dial.frame);
        animation.frame = 0;
        placeSprite(registry, dial.quad, dial.atPx, dial.cellPx * dial.scale, dial.z, 0.0f);
    }
}

// THE GHOST OF A TAKEN LIGHT WALL (art.json light_wall_ghost, sim/Torch). The
// original's seekLightWallToDestroy adds a dashed_door.ent named
// `light_wall_ghost` at the flagged wall's position and angle, and addLightWall
// deletes it when the orbiting fire signal is shot and the wall comes back. So
// the ghost stands for exactly as long as the wall is going or gone, and the
// port makes and unmakes its quad on those two ticks rather than hiding it: a
// torch put out and lit again gets a fresh entity in the original, with both its
// clocks at zero, and the same here.
//
// IT IS NOT DRAWN ON THE TICK THE TORCH IS LIT. The entity is added inside the
// torch's own callback, which the engine runs from UpdateCurrentFrameEntities -
// AFTER MapEntitiesToBeRendered built the list that frame draws
// (Scene/ETHScene.cpp:482-511, D5 section 3c). Its first drawn frame is the next
// one, and by then its callback has run once: the alpha is one step of the angle
// off 0.3 and the pulse one frame into its first leg (00_order 8.0 rule 6, the
// rule step 78's button is drawn by). `added` is that first tick, `drawn` every
// tick after it.
void MagicPortalsLayer::syncWallGhost(entt::registry& registry) {
    WallGhost& ghost = m_wallGhost;
    const Torch::Wall& wall = m_level.torch.wall;
    const bool flagged = wall.present && (wall.going || wall.gone);
    if (!flagged || ghost.sizePx == glm::dvec2(0.0)) {
        if (ghost.quad != entt::null) {
            if (registry.valid(ghost.quad)) registry.destroy(ghost.quad);
            ghost.quad = entt::null;
            SUPERSONIC_LOG_INFO("Magic Portals")
                << "the light wall is back, and its ghost gone on tick " << m_ticks << std::endl;
        }
        ghost.added = false;
        ghost.drawn = false;
        ghost.turn.turnedDeg = 0.0;
        ghost.pulseMs = 0.0;
        ghost.alpha = 0.0;
        ghost.scale = glm::dvec2(1.0);
        return;
    }
    if (!ghost.drawn) return; // the tick it was added: the original's frame cannot draw it
    const Art::LightWallGhost& rules = m_artRules.lightWallGhost;
    ghost.alpha = rules.AlphaAt(ghost.turn.turnedDeg);
    ghost.scale = rules.pulse.ScaleAt(ghost.pulseMs);
    if (ghost.quad == entt::null) {
        ghost.quad = makeSprite(registry, "Magic Portals Wall Ghost", ghost.texture, rules.additive);
        SUPERSONIC_LOG_INFO("Magic Portals") << "the light wall is going, with " << ghost.texture
                                             << " left where it stood on tick " << m_ticks << std::endl;
    }
    placeSprite(registry, ghost.quad, ghost.atPx, ghost.sizePx * ghost.scale, ghost.z, ghost.rotation);
}

void MagicPortalsLayer::syncDrawables(entt::registry& registry) {
    using namespace Supersonic;
    for (Drawn& drawn : m_bodies) {
        // A body the level took away - a wall a stone broke - takes its box with it.
        if (!registry.valid(drawn.body)) {
            if (drawn.box != entt::null && registry.valid(drawn.box)) registry.destroy(drawn.box);
            drawn.box = entt::null;
            continue;
        }
        const auto& body = registry.get<TransformComponent>(drawn.body);
        const float angle = body.rotation.z;
        // The shape's offset, turned with its body. Pixels run +y down, so the
        // offset's y flips on the way to metres, as Units::ToWorld flips a point.
        const glm::vec2 offset(Units::ToMetres(drawn.offsetPx.x), Units::ToMetres(-drawn.offsetPx.y));
        const glm::vec2 turned(offset.x * std::cos(angle) - offset.y * std::sin(angle),
                               offset.x * std::sin(angle) + offset.y * std::cos(angle));
        const glm::dvec2 centrePx = Units::ToPixels(glm::vec3(body.position.x + turned.x, body.position.y + turned.y, 0.0f));
        placeBox(registry, drawn.box, centrePx, drawn.sizePx, 0.0f, drawn.depth, angle);
    }

    // One box and one sprite per body a launcher threw or the beholder dropped,
    // made and unmade to match.
    for (ThrownBox& drawn : m_thrown) {
        if (registry.valid(drawn.body)) continue;
        if (drawn.box != entt::null && registry.valid(drawn.box)) registry.destroy(drawn.box);
        if (drawn.quad != entt::null && registry.valid(drawn.quad)) registry.destroy(drawn.quad);
        drawn.box = entt::null;
        drawn.quad = entt::null;
    }
    std::erase_if(m_thrown, [](const ThrownBox& drawn) { return drawn.box == entt::null; });
    struct Loose {
        entt::entity body;
        double radiusPx;
        std::string sprite;
        glm::dvec3 emissive;
    };
    std::vector<Loose> loose;
    for (const Launchers::Thrown& thrown : m_level.launchers.live) {
        loose.push_back({thrown.body, thrown.is.radiusPx, thrown.is.sprite, thrown.is.emissive});
    }
    for (const Boss::Rock& rock : m_level.boss.rocks) {
        loose.push_back({rock.body, m_level.boss.rock.radiusPx, m_level.boss.rock.sprite, m_level.boss.rock.emissive});
    }
    for (const Loose& thrown : loose) {
        if (!registry.valid(thrown.body)) continue;
        auto drawn = std::find_if(m_thrown.begin(), m_thrown.end(),
                                  [&thrown](const ThrownBox& d) { return d.body == thrown.body; });
        if (drawn == m_thrown.end()) {
            ThrownBox made;
            made.body = thrown.body;
            made.emissive = thrown.emissive;
            made.box = makeBox(registry, "Magic Portals Thrown", glm::vec3(0.0f), glm::vec3(1.0f), kStoneColour);
            registry.emplace<InterpolatedTransformComponent>(made.box);
            // Drawn as what it is, with the image the converter copied for its .ent:
            // its tier file where the converter copied one (drawnImage), as the
            // level's own stones are, hd/rolling_stone.png at 64 units.
            if (m_artReady && !thrown.sprite.empty()) {
                const Sprites::Image& image = drawnImage(m_paths.art + "/assets/entities/" + thrown.sprite);
                if (image.units != glm::ivec2(0)) {
                    made.quad = makeSprite(registry, "Magic Portals Thrown Sprite", image.path, false);
                    registry.emplace<InterpolatedTransformComponent>(made.quad);
                }
            }
            m_thrown.push_back(made);
            drawn = m_thrown.end() - 1;
        }
        const auto& body = registry.get<TransformComponent>(thrown.body);
        placeBox(registry, drawn->box, Units::ToPixels(body.position), glm::dvec2(thrown.radiusPx * 2.0), 0.0f, 0.4f,
                 body.rotation.z);
        if (drawn->quad != entt::null) {
            placeSprite(registry, drawn->quad, Units::ToPixels(body.position),
                        glm::dvec2(drawnImage(m_paths.art + "/assets/entities/" + thrown.sprite).units),
                        slotAt(m_playerSlot, Slot::ThrownStone), body.rotation.z);
        }
    }

    // THE PLATFORM THE DARK DRAGON'S DEATH ADDS, as a box. Its picture is
    // borrowed in syncSprites, but that is gated on m_artReady - and the box
    // view is exactly the mode used when there is NO art - so the box is made
    // here and carries a flag of its own. Every other body in the game has one,
    // and for a while this did not.
    //
    // Sized from the COLLIDER and not from the sprite: a box stands for the
    // body. Added after the loops above, which have finished walking m_bodies.
    const entt::entity platformBody = m_level.darkDragon.platformBody;
    if (!m_platformBoxed && platformBody != entt::null && registry.valid(platformBody)) {
        if (const auto* collider = registry.try_get<BoxColliderComponent>(platformBody)) {
            Drawn made;
            made.body = platformBody;
            made.offsetPx = glm::dvec2(0.0);
            made.sizePx = glm::dvec2(collider->size.x, collider->size.y) * Units::kPixelsPerMetre;
            made.depth = 0.6f; // a static's, as buildDrawables gives one
            made.box = makeBox(registry, "Magic Portals Body", glm::vec3(0.0f), glm::vec3(1.0f), kStaticColour);
            registry.emplace<InterpolatedTransformComponent>(made.box);
            m_bodies.push_back(made);
            m_platformBoxed = true;
        }
    }

    // AND THE BARRELS A RESTOCK ADDED (sim/Restock), one box each, as buildDrawables
    // gives the placed one: its template's shape, a traveller's colour.
    while (m_restockBoxed < m_level.restock.added.size()) {
        const Restock::Added& added = m_level.restock.added[m_restockBoxed];
        ++m_restockBoxed;
        const Tscn::Scene& from = Restock::TemplateScene(m_data.scene, m_data.templates, added.templateFromPort);
        const Tscn::Node* node = from.FindNode(added.templateNode);
        if (!registry.valid(added.body) || node == nullptr) continue;
        Drawn made;
        made.body = added.body;
        if (!LevelBuilder::ShapeBoundsPx(from, *node, made.offsetPx, made.sizePx)) continue;
        made.depth = 0.4f; // a rigid body's, as buildDrawables gives one
        made.box = makeBox(registry, "Magic Portals Body", glm::vec3(0.0f), glm::vec3(1.0f), kCrateColour);
        registry.emplace<InterpolatedTransformComponent>(made.box);
        m_bodies.push_back(made);
    }

    // The player in its slot among the art - or, with the boxes shown or no art
    // to show, where the boxes are.
    const bool artOnly = m_artReady && !m_showBoxes;
    // m_player and m_playerQuad are BOTH gone once the character has stepped into
    // the door (OnFixedUpdate), and the level goes on running for the 1400 ms
    // before it is scored - so this has to tolerate their absence rather than
    // place a box that is no longer there. placeBox does not guard its entity.
    if (m_level.player != entt::null && registry.valid(m_level.player) && m_player != entt::null &&
        registry.valid(m_player)) {
        const glm::dvec2 at = Units::ToPixels(registry.get<TransformComponent>(m_level.player).position);
        placeBox(registry, m_player, at, glm::dvec2(m_data.tuning.widthPx, m_data.tuning.heightPx),
                 artOnly ? slotAt(m_playerSlot, Slot::Picture) : 0.1f, artOnly ? 0.5f * kSpriteSlotZ : 0.4f,
                 0.0f);
        if (m_playerQuad != entt::null) {
            const Art::Character& mage = m_artRules.character;
            // It turns as it walks, which is the owner's word; the row each way
            // walks is read from the original's DIRECTION enum (art.json).
            if (m_direction > 0.0f) m_facingRight = true;
            if (m_direction < 0.0f) m_facingRight = false;
            const int facing = m_facingRight ? mage.rightRow : mage.leftRow;
            // findFinalDirection: while the last step's ray met something, the
            // arm out - row 0 facing left, row 3 any other way - walking or
            // standing, on the column either would draw. m_pushing is the step
            // before's (stepLevel sets this step's after this sync), which is
            // updateFrame running before applyForces.
            const int row = m_pushing ? (facing == mage.leftRow ? mage.push.leftRow : mage.push.otherRow) : facing;
            const bool weightless = m_level.noGravity;
            // On a no_gravity level the flipbook stands still on the column
            // MainCharacter's timer gives this step (stepLevel), walking or not.
            const bool walking = m_direction != 0.0f && !weightless;
            const int column = weightless ? m_noGravityColumn : (walking ? 0 : mage.idleColumn);
            const uint32_t first = static_cast<uint32_t>(row * mage.columns + column);
            const uint32_t count = walking ? static_cast<uint32_t>(mage.columns) : 1u;
            // The walk's column and its time are NOT reset by a stop or a turn:
            // the original's FrameTimer is always set(0, 3), so it resets only
            // on the first walk, which the frame and time a level starts with
            // already are. Standing, the flipbook is a still - the cell is
            // firstFrame + frame % 1 - and SpriteAnimationSystem::Advance leaves a
            // stopped one's frame and time alone, as updateFrame adds no time to a
            // timer it does not call. The app advances it before this sync, so
            // the tick a stop begins adds one tick the original does not and the
            // tick a walk resumes adds none: the time at the end of that tick is
            // the original's, and the stop between shows the idle column.
            auto& animation = registry.get<SpriteAnimationComponent>(m_playerQuad);
            animation.firstFrame = first;
            animation.frameCount = count;
            animation.playing = walking;
            const glm::dvec2 cellPx = playerCellPx(weightless);
            // The image stands with its pivot on the entity, as Ethanon draws it
            // (ETHSpriteEntity::ComputeInScreenSpriteCenter). linearMotion hovers
            // that pivot once it has run: a larger y draws the image higher.
            double pivotYPx = mage.pivotYPx;
            if (weightless && m_noGravityPlayer.moved) {
                pivotYPx += mage.noGravity.HoverUnits(m_noGravityPlayer.angle, ViewPx().y);
            }
            placeSprite(registry, m_playerQuad, at - glm::dvec2(mage.pivotXPx, pivotYPx), cellPx,
                        slotAt(m_playerSlot, Slot::Picture), 0.0f);
        }
    }

    syncBoss(registry);
    syncMinions(registry);
    syncKnight(registry);
    syncGhost(registry);
    syncTurrets(registry);

    const auto boxPx = [](const Trigger::Box& box, glm::dvec2& centrePx, glm::dvec2& sizePx) {
        centrePx = Units::ToPixels(glm::vec3(box.centre, 0.0f));
        sizePx = glm::dvec2(box.half) * 2.0 * Units::kPixelsPerMetre;
    };
    for (std::size_t i = 0; i < m_buttons.size() && i < m_level.channels.buttons.size(); ++i) {
        const Puzzle::Button& button = m_level.channels.buttons[i];
        glm::dvec2 centrePx, sizePx;
        boxPx(button.box, centrePx, sizePx);
        placeBox(registry, m_buttons[i], centrePx, sizePx, kMarkerZ, kMarkerDepth, 0.0f);
        registry.get<MaterialComponent>(m_buttons[i]).albedoColor =
            glm::vec4(button.pressed ? kButtonDownColour : kButtonUpColour, 1.0f);
    }
    for (std::size_t i = 0; i < m_crystals.size() && i < m_level.goals.crystals.size(); ++i) {
        if (m_crystals[i] == entt::null) continue;
        const Goals::Crystal& crystal = m_level.goals.crystals[i];
        if (crystal.collected || crystal.expired) {
            registry.destroy(m_crystals[i]);
            m_crystals[i] = entt::null;
            continue;
        }
        glm::dvec2 centrePx, sizePx;
        boxPx(crystal.box, centrePx, sizePx);
        placeBox(registry, m_crystals[i], centrePx, glm::dvec2(14.0), kMarkerZ, kMarkerDepth, 0.785398f);
        // A timed crystal's box does not blink either: the Godot project's guessed fade
        // is gone with its picture's (syncSprites), and a box has no dial.
        registry.get<MaterialComponent>(m_crystals[i]).albedoColor = glm::vec4(kCrystalColour, 1.0f);
    }
    {
        glm::dvec2 centrePx, sizePx;
        boxPx(m_level.goals.exit, centrePx, sizePx);
        placeBox(registry, m_exit, centrePx, sizePx, kMarkerZ, kMarkerDepth, 0.0f);
        registry.get<MaterialComponent>(m_exit).albedoColor =
            glm::vec4(m_level.goals.completed ? kExitReachedColour : kExitColour, 1.0f);
    }

    // One box per placed portal, made and unmade to match.
    const std::vector<Portals::Placed>& placed = m_level.portals.placed;
    while (m_portals.size() > placed.size()) {
        if (registry.valid(m_portals.back())) registry.destroy(m_portals.back());
        m_portals.pop_back();
    }
    while (m_portals.size() < placed.size()) {
        m_portals.push_back(makeBox(registry, "Magic Portals Portal", glm::vec3(0.0f), glm::vec3(1.0f), kPortalColour));
    }
    const double diameterPx = m_level.portals.rules.entryRadiusPx * 2.0;
    for (std::size_t i = 0; i < placed.size(); ++i) {
        placeBox(registry, m_portals[i], placed[i].atPx, glm::dvec2(diameterPx), kMarkerZ, kMarkerDepth, 0.0f);
    }
    // The PICTURE of a placed portal is portal.ent itself - its sprite and its
    // two particle systems - which insertPortal adds like any other entity and
    // syncPlacedPortals draws through the effects runtime, on the FRAME (the
    // visuals plan's 7.4). Nothing of it is here any more.

    // The shot in flight, made when one is fired and unmade when it lands or fails:
    // a box, and projectile.ent's sheet when the original's image is there, at the
    // z 0 addProjectile adds it at - over the level's pictures at 0 or below and
    // BEHIND the player it leaves, who stands at R10's 4 (00_order K14, the
    // between-slot table's Slot::ShotPicture) - as addProjectile and
    // ETHCallback_projectile draw it (art.json shot): Scale(0.8) on its cell and
    // its pivot, turned so the head of the bolt leads along its flight, on the
    // cell the one timer gives.
    if (m_level.portals.flight) {
        if (m_shot == entt::null) {
            m_shot = makeBox(registry, "Magic Portals Shot", glm::vec3(0.0f), glm::vec3(1.0f), kShotColour);
        }
        placeBox(registry, m_shot, m_level.portals.flight->atPx, glm::dvec2(kShotSizePx), kMarkerZ, kMarkerDepth,
                 0.0f);
        const Art::Shot& bolt = m_artRules.shot;
        const Sprites::Image& sheet = drawnImage(originalImage(bolt.sprite));
        const glm::dvec2 cellPx = frameUnits(sheet, bolt.columns, bolt.rows);
        if (m_shotQuad == entt::null && m_artReady && cellPx != glm::dvec2(0.0)) {
            m_shotQuad = makeSprite(registry, "Magic Portals Shot Sprite", sheet.path, bolt.additive);
            // A still: the cell is chosen, not played (stepLevel steps the timer).
            auto& animation = registry.emplace<SpriteAnimationComponent>(m_shotQuad);
            animation.columns = static_cast<uint32_t>(bolt.columns);
            animation.rows = static_cast<uint32_t>(bolt.rows);
            animation.frameCount = 1;
            animation.playing = false;
        }
        if (m_shotQuad != entt::null) {
            registry.get<SpriteAnimationComponent>(m_shotQuad).firstFrame = static_cast<uint32_t>(m_shotCell);
            // Its 'direction': toward where it opens a portal, which a reflector
            // mirrors (Portals::Flight).
            const Portals::Flight& flight = *m_level.portals.flight;
            const float angle = bolt.AngleDegrees(flight.toPx - flight.atPx);
            placeSprite(registry, m_shotQuad, bolt.CentrePx(flight.atPx, angle), cellPx * bolt.scale,
                        slotAfter(bolt.z, Slot::ShotPicture), angle * (3.14159265f / 180.0f));
        }
    } else {
        if (m_shot != entt::null && registry.valid(m_shot)) registry.destroy(m_shot);
        m_shot = entt::null;
        if (m_shotQuad != entt::null && registry.valid(m_shotQuad)) registry.destroy(m_shotQuad);
        m_shotQuad = entt::null;
    }

    // Static portals, at their trigger boxes. One that is spent - only when
    // portals.json says static portals do not persist - goes.
    const std::vector<Portals::Static>& statics = m_level.portals.statics;
    for (std::size_t i = 0; i < m_statics.size() && i < statics.size(); ++i) {
        if (m_statics[i] == entt::null) continue;
        if (!statics[i].live) {
            registry.destroy(m_statics[i]);
            m_statics[i] = entt::null;
            continue;
        }
        glm::dvec2 centrePx, sizePx;
        boxPx(statics[i].trigger, centrePx, sizePx);
        placeBox(registry, m_statics[i], centrePx, sizePx, kMarkerZ, kMarkerDepth, 0.0f);
    }

    // No-portal zones, as the square round the circle a tap is refused in, where
    // each is now: a patrolling one moves. The circle the port PLAYS, which is not
    // the ring's half until the refusal radius is the original's (placement.json).
    const std::vector<Portals::NoPortalZone>& zones = m_level.portals.zones;
    for (std::size_t i = 0; i < m_zones.size() && i < zones.size(); ++i) {
        const double sizePx = m_level.portals.rules.antiportalRadiusPx * zones[i].scale * 2.0;
        placeBox(registry, m_zones[i], zones[i].CentreNowPx(), glm::dvec2(sizePx), kZoneZ, kZoneDepth, 0.0f);
    }
    const std::vector<Portals::Agent>& agents = m_level.portals.agents;
    for (std::size_t i = 0; i < m_agents.size() && i < agents.size(); ++i) {
        const double sizePx = m_level.portals.AgentReachPx(agents[i]) * 2.0;
        placeBox(registry, m_agents[i], agents[i].CentreNowPx(), glm::dvec2(sizePx), kZoneZ, kZoneDepth, 0.0f);
    }

    // Hazards at their box: a destroyier's 16 px trigger, a death area's polygon
    // bounds (it kills on the polygon itself, Hazards.hpp).
    const std::vector<Hazards::Hazard>& hazards = m_level.hazards.hazards;
    for (std::size_t i = 0; i < m_hazards.size() && i < hazards.size(); ++i) {
        glm::dvec2 centrePx, sizePx;
        boxPx(hazards[i].box, centrePx, sizePx);
        placeBox(registry, m_hazards[i], centrePx, sizePx, kMarkerZ, kMarkerDepth, 0.0f);
    }

    syncSprites(registry);
    syncTimers(registry);
    syncWallGhost(registry);
    syncShockRings(registry);

    // The art in place of the boxes, unless B asks for them or there is no art.
    // What no level pictures - the player, the portals a shot opens, the shot -
    // is a box either way.
    const auto show = [&registry](entt::entity e, bool visible) {
        if (e != entt::null && registry.valid(e)) registry.get<RenderableComponent>(e).isVisible = visible;
    };
    for (const Drawn& drawn : m_bodies) show(drawn.box, !artOnly);
    for (const ThrownBox& drawn : m_thrown) show(drawn.box, !artOnly);
    for (const entt::entity e : m_buttons) show(e, !artOnly);
    for (const entt::entity e : m_crystals) show(e, !artOnly);
    for (const entt::entity e : m_statics) show(e, !artOnly);
    for (std::size_t i = 0; i < m_zones.size(); ++i) {
        show(m_zones[i], !artOnly && !(i < zones.size() && zones[i].gone));
    }
    for (std::size_t i = 0; i < m_agents.size(); ++i) {
        show(m_agents[i], !artOnly && !(i < agents.size() && agents[i].gone));
    }
    for (const entt::entity e : m_hazards) show(e, !artOnly);
    show(m_exit, !artOnly);
    // A placed portal and the shot, behind the original's pictures once they
    // stand in for them.
    // A placed portal stands behind portal.ent's own picture once there is one to
    // draw, and is the picture itself where the original's art is not there.
    // Tested against the CLASS - whether that image can be read at all - and not
    // against the instance, as a fireball's below is: the picture is added on the
    // FRAME, one frame after the tick that opened the portal, and a box that
    // waited for the instance would flash for that frame on every portal opened.
    const bool portalDrawn = m_artReady && drawnImage(originalImage(m_artRules.portal.sprite)).units != glm::ivec2(0);
    for (const entt::entity e : m_portals) show(e, !(artOnly && portalDrawn));
    show(m_shot, !(artOnly && m_shotQuad != entt::null));
    show(m_player, !(artOnly && m_playerQuad != entt::null));
    show(m_beholderBox, !(artOnly && m_beholderQuad != entt::null));
    // A fireball's box goes with the level's own boxes and not with the shot's:
    // the fireball.ent that stands in for it is added on the FRAME, one frame
    // after the tick that spat it, and a box that waited for it would flash tan
    // for that frame on every fireball fired. Tested against the class and not
    // the instance, as a crystal's is.
    for (const entt::entity e : m_fireballs) show(e, !artOnly);

    // Last: every quad this tick made is there to colour.
    syncLighting(registry);
}

// ---- the ambient light ----------------------------------------------------------
//
// The original draws every sprite as texel x colour x min(1, ambient + emissive)
// before it adds anything (ETHRenderEntity.cpp:113-117), whatever its blend and
// whether or not it applies light: pass 1 of every draw. Enhanced's fit of the
// original's pixels holds that to 0.28 of 255 over 1,359 blocks of sprites
// without a lightmap, where drawing them full bright is 45.30 (fit.md 4). With the
// scene holding display values (SceneRendering), the multiply lands on the bytes,
// as it did in the original.
//
// THROUGH THE ENGINE'S 2D SPRITE PATH (MaterialComponent::sprite2D), since step
// 47, the lighting design's G4. Step 45 folded the factor into the albedo colour;
// the engine now takes it apart from the colour, which the lights will need (a
// lamp is not dimmed by the room it shines in), and multiplies the two on the
// CPU exactly as the fold did, so a sprite without a lightmap draws the same.
//
// AND ITS LIGHTMAP, added after the multiply and dimmed by nothing: the baked
// add<id>.png of a static, light-applying sprite (design decisions 3 and 4, from
// the fit: multiplying it scores 26.95 of 255 where adding it scores 0.92, and
// the shipped PNG beats the ETC1 file on 11 of 11 entities). Every level that
// places a torch is a `darkest` level and ships no lightmap (torch.json's census).
// Once a torch is lit the level bakes at run time (step 55, the design's G6): every
// static sprite takes the static lights live, and a file lightmap would be dropped.
//
// PREMULTIPLIED, every mixed sprite of a lit level. The original adds a sprite's
// live light at full weight where its base is weighted by alpha, which one draw
// can only do premultiplied (design section 4.5). Where no light adds anything it
// is the straight mix to within a rounding. The added sprites stay added: no
// blendMode-1 instance applies light.
//
// AND WHAT REACHES IT (since step 49, the design's G5): its height, the original's
// depth; its normal map, when it applies light; and its light mask
// (Lighting::ReceiverMask), which lets a static sprite take only the lights that
// are not static - the shot's - and the player every light, the torch's included.
// The lights themselves, and their halos, are syncLights'.
//
// What is NOT coloured, and why:
//   - the particles. Ethanon multiplies a particle system by
//     min(1, luminance + ambient) only when it is alpha-blended
//     (ETHParticleManager.cpp:381-389). 887 of the levels' 897 placed systems
//     (their own <Particles>, planning step 91) are added and take nothing; the
//     other ten (portal_static's portal.png, luminance 1, and gutter_mouth's
//     water.png, 0.45) take that factor in Particles::DrawColour, from this
//     ambient, and no light: a particle never applies one.
//   - the boxes: placeholders the PBR path draws, which stand for things rather
//     than being them. At a dark level's 0.01 they would vanish.
//   - the menu, the medal screens and the HUD, none of which is in a level.
void MagicPortalsLayer::syncLighting(entt::registry& registry) {
    m_ambient =
        m_lit ? Lighting::Ambient(m_data.lighting, m_look.ambient, m_level.darkest, m_level.torch, m_level.redKey)
              : glm::dvec3(1.0);

    const bool runtimeBake = m_lit && Lighting::RuntimeBake(m_level.torch);
    static const std::string noLightmap;
    for (DrawnSprite& drawn : m_sprites) {
        Receiver receiver;
        receiver.applyLight = drawn.applyLight;
        receiver.isStatic = drawn.isStatic;
        receiver.normal = drawn.normal;
        receiver.z = drawn.lookZ;
        receiver.runtimeBake = runtimeBake;
        glm::vec4 colour = drawn.colour * glm::vec4(1.0f, 1.0f, 1.0f, drawn.fade);
        // A RING'S BLINK (art.json antiportal): its callback writes the entity's rgb
        // every frame and keeps its alpha (SetColor, ETHEntity.cpp:493-498), over
        // whatever colour the level gave it - no placement gives one. Its clock is
        // the level's age, the game time since its first tick, which a pause stands
        // still as the original's m_factor 0 stands blinkElapsedTime still.
        if (drawn.antiportal) {
            colour = glm::vec4(glm::vec3(m_artRules.antiportal.ColourAt(m_levelAgeMs)), colour.a);
        }
        // A LIFT'S BLINK (movers.json lift.blink): ETHCallback_lift #368-393, every
        // frame whatever the lift is doing, on the same clock as the ring's. Its
        // emissive 1 keeps min(1, ambient + emissive) whole, so the grey shows as is.
        if (drawn.lift >= 0) {
            const Mover::Rules& movers = m_data.movers;
            const Art::Blink blink{movers.liftBlinkFrom, movers.liftBlinkTo, movers.liftBlinkStrideMs};
            colour = glm::vec4(glm::vec3(blink.At(m_levelAgeMs)), colour.a);
        }
        // 4-19'S RED KEY STATE, faded with the ambient once its key is taken:
        // SetColor(GetVector3('color') x bias) every frame (#140-159), rgb only.
        // Its emissive 1 keeps min(1, ambient + emissive) whole, so the halo fades
        // by this alone while the ambient takes the lit sprites down with it.
        if (drawn.redKeyState) {
            colour = glm::vec4(glm::vec3(m_level.redKey.rules.colour * m_level.redKey.bias), colour.a);
        }
        // A CRATE NO PORTAL CARRIES (art.json non_teleportable), on the same clock:
        // blinkColor writes the rgb as the ring's does, blinkEmissive the emissive
        // min(1, ambient + emissive) takes. Once its crate is burned the script stops
        // writing either: manageBurnable takes the colour from 1 (its fade is not
        // drawn, so the node's white stands), and the emissive keeps its last blink.
        glm::dvec3 emissive = drawn.emissive;
        // A BARREL'S BLINK (art.json barrel_bomb): ETHCallback_barrel_bomb's blinkEmissive every frame
        // writes the emissive over the node's own - its floor, minEmissive, to 0.2 and back on
        // 300 ms legs - on the barrel's own clock, one call a frame.
        if (drawn.scriptBlink == DrawnSprite::ScriptBlink::Barrel) {
            emissive = m_artRules.barrelBomb.Triangle(drawn.blinkFloor).At(m_levelAgeMs - drawn.blinkBornMs);
        }
        // A WALL WITH `blink` (art.json breakable_wall): blinkColor then blinkEmissive, 1 to 0.8 and
        // back on 300 ms legs, both adding the frame's time to ONE blinkElapsedTime, so the colour
        // reads the clock one add behind the emissive and both run at twice the level's rate.
        if (drawn.scriptBlink == DrawnSprite::ScriptBlink::Wall) {
            const Art::BreakableWall& wall = m_artRules.breakableWall;
            const double sinceMs = m_levelAgeMs - drawn.blinkBornMs;
            colour = glm::vec4(glm::vec3(wall.colour.At(wall.ClockMs(sinceMs, m_levelStepMs, 1))), colour.a);
            emissive = wall.emissive.At(wall.ClockMs(sinceMs, m_levelStepMs, wall.callsPerFrame));
        }
        if (drawn.blinks != Art::NonTeleportable::Blinks::Nothing) {
            const std::vector<Fire::Burnable>& burnables = m_level.fire.burnables;
            const bool burned = drawn.burnable >= 0 && static_cast<std::size_t>(drawn.burnable) < burnables.size() &&
                                burnables[static_cast<std::size_t>(drawn.burnable)].burned;
            if (!burned) {
                drawn.blinkMs = m_levelAgeMs;
                drawn.blinked = true;
            }
            const Art::NonTeleportable& crates = m_artRules.nonTeleportable;
            if (drawn.blinks == Art::NonTeleportable::Blinks::Colour && !burned) {
                colour = glm::vec4(glm::vec3(crates.colour.At(drawn.blinkMs)), colour.a);
            } else if (drawn.blinks == Art::NonTeleportable::Blinks::Emissive && drawn.blinked) {
                emissive = crates.emissive.At(drawn.blinkMs);
            } else if (drawn.blinks == Art::NonTeleportable::Blinks::TeleportableEmissive && drawn.blinked) {
                // crateCallback's own, for a teleportable crate (#70-93): 0.2 to 0 on 600 ms legs.
                emissive = crates.teleportableEmissive.At(drawn.blinkMs);
            }
        }
        tint(registry, drawn.quad, colour, emissive, runtimeBake ? noLightmap : drawn.lightmap, receiver);
        // A sky's cover in its sky's colour and light: the samples it fills are the sky's.
        if (drawn.cover != entt::null) {
            tint(registry, drawn.cover, colour, emissive, runtimeBake ? noLightmap : drawn.lightmap, receiver);
        }
    }
    // What no level places, with its .ent's emissive (art.json, launchers.json).
    // None has a lightmap: a bake belongs to an entity the level file placed.
    const glm::vec4 white(1.0f);
    // And its .ent's lighting facts. Its normal map is the original's, beside the
    // image it is drawn with; its height, for the player, its marker's depth.
    const auto receiverOf = [this](const Art::Picture& picture, double z) {
        Receiver receiver;
        receiver.applyLight = picture.applyLight;
        receiver.isStatic = picture.isStatic;
        if (!picture.normal.empty()) receiver.normal = drawnImage(originalImage("normalmaps/" + picture.normal)).path;
        receiver.z = z;
        return receiver;
    };
    // The player at dark_mage.ent's emissive 1, or, on an `ignore_emissive` level, the one Game::preLoop sets it to
    // (#355-393: 0, or 0.2 in chapter 3), which is what makes it a silhouette in a dark room.
    tint(registry, m_playerQuad, white, m_scriptEmissive.ignore ? m_scriptEmissive.player : m_artRules.character.emissive,
         {}, receiverOf(m_artRules.character, m_playerZ));
    // A timed crystal's dial at its alpha, which the sprite path weights its base
    // by (shader.frag:477-478), as a keyhole's fade is. timer.ent's emissive 1
    // makes min(1, ambient + 1) whole on every level, chapter 4's dark ones too.
    for (const TimerDial& dial : m_timers) {
        tint(registry, dial.quad, glm::vec4(1.0f, 1.0f, 1.0f, static_cast<float>(dial.alpha)),
             m_artRules.timer.emissive, {}, receiverOf(m_artRules.timer, dial.lookZ));
    }
    // The ghost of a taken light wall at the alpha its callback wrote this tick,
    // which the sprite path weights its texels by as it does the dial's.
    // dashed_door.ent's emissive 1 makes min(1, ambient + 1) whole, so it is as
    // bright on these `darkest` levels as the flame that took the wall away.
    if (m_wallGhost.quad != entt::null) {
        tint(registry, m_wallGhost.quad, glm::vec4(1.0f, 1.0f, 1.0f, static_cast<float>(m_wallGhost.alpha)),
             m_artRules.lightWallGhost.emissive, {},
             receiverOf(m_artRules.lightWallGhost, m_wallGhost.lookZ));
    }
    // The shock rings at the colour their agent gave them (art.json shock_ring), with
    // shock_area.ent's own facts: emissive 1 makes min(1, ambient + 1) whole, so each
    // is (0.4, 0.5, 0.5) added on every level, chapter 4's dark ones too.
    for (const ShockRingQuad& ring : m_shockRings) {
        if (ring.quad == entt::null) continue;
        tint(registry, ring.quad, glm::vec4(glm::vec3(m_artRules.shockRing.colour), 1.0f), m_shockRingEntity.emissive,
             {}, receiverOf(m_shockRingEntity, ring.lookZ));
    }
    // The black bars, with black_bar.ent's own facts: emissive 0, static, no light.
    // Their texels are black, so any ambient leaves them black; what this keeps is
    // the one lit path every picture of the scene is drawn on.
    for (const BlackBarQuad& bar : m_blackBars) {
        tint(registry, bar.quad, white, m_blackBarEntity.emissive, {}, receiverOf(m_blackBarEntity, 0.0));
    }
    tint(registry, m_shotQuad, white, m_artRules.shot.emissive, {}, receiverOf(m_artRules.shot, m_artRules.shot.z));
    tint(registry, m_beholderQuad, m_beholderColour, m_artRules.beholder.emissive, {},
         receiverOf(m_artRules.beholder, 0.0));
    // A spike is a box when its image is not there, and a box is not coloured.
    for (const entt::entity spike : m_spikes) {
        if (spike == entt::null || !registry.valid(spike)) continue;
        if (!registry.get<Supersonic::MaterialComponent>(spike).unlit) continue;
        tint(registry, spike, white, m_artRules.spike.emissive, {}, receiverOf(m_artRules.spike, kSpikeZIndex));
    }
    // A thrown stone takes no light yet: rolling_stone.ent applies light, and
    // launchers.json does not carry that or its normal map (step 49's open items).
    for (const ThrownBox& thrown : m_thrown) tint(registry, thrown.quad, white, thrown.emissive);
    // The minions (art.json minion). Every update ETHCallback_minion sets its
    // emissive to minion.ent's, or to 0 while the ambient is DARKEST_AMBIENT_LIGHT
    // (canSeeAnything): a knight on a dark level is a silhouette, and lights up
    // again with its room once a torch is lit. The black knight's script sets its
    // colour to 0 every frame over that, and its own emissive is 0.
    const Art::Minion& minionArt = m_artRules.minion;
    const bool blind = m_lit && minionArt.darkestZeroesEmissive && m_ambient == m_data.lighting.darkestAmbient;
    for (const MinionQuad& minion : m_minions) {
        if (minion.quad == entt::null) continue;
        const glm::dvec3 emissive = blind ? glm::dvec3(0.0) : (minion.black ? minionArt.blackEmissive : minionArt.emissive);
        const glm::vec4 colour = minion.black ? glm::vec4(glm::vec3(minionArt.blackColour), 1.0f) : white;
        tint(registry, minion.quad, colour, emissive, {}, receiverOf(minionArt, minion.lookZ));
    }
    // 2-32's knight: SetColor(V3_ZERO) in its first run and dragon_knight.ent's own
    // emissive 0, applyLight 0 as minion.ent (cutscenes.json dragon_knight).
    if (m_knightQuad != entt::null) {
        const Cutscene::Rules::Knight& knight = m_cutsceneRules.knight;
        tint(registry, m_knightQuad, glm::vec4(glm::vec3(knight.colour), 1.0f), knight.emissive, {},
             receiverOf(minionArt, 0.0));
    }
    // CHAPTER 3'S BOSS at the colour and the alpha its callback wrote this tick
    // (SetColor, SetAlpha), which the sprite path weights its texels by as it does
    // the dial's. ghost.ent's emissive 1 makes min(1, ambient + 1) whole, so the
    // black it flies in as is the colour's and not the room's.
    if (m_ghost.quad != entt::null) {
        const Ghost::Look& look = m_level.ghost.look;
        tint(registry, m_ghost.quad, glm::vec4(glm::vec3(look.colour), static_cast<float>(look.alpha)),
             m_artRules.ghost.emissive, {}, receiverOf(m_artRules.ghost, m_ghost.lookZ));
    }

    // The lights, placed after everything that owns one.
    syncLights(registry);
}

void MagicPortalsLayer::tint(entt::registry& registry, entt::entity quad, const glm::vec4& colour,
                             const glm::dvec3& emissive, const std::string& lightmap, const Receiver& receiver) const {
    using Supersonic::MaterialComponent;
    if (quad == entt::null || !registry.valid(quad)) return;
    auto& material = registry.get<MaterialComponent>(quad);

    // A level whose lighting did not read is drawn as before lighting existed:
    // the plain unlit path, its colour alone, mixed straight.
    MaterialComponent::Sprite2DLight sprite;
    MaterialComponent::BlendMode blend = material.blend;
    std::string normal;
    if (m_lit) {
        sprite.enabled = true;
        sprite.ambient = glm::vec3(Lighting::AmbientTerm(m_ambient, emissive));
        if (blend == MaterialComponent::BlendMode::Alpha) blend = MaterialComponent::BlendMode::Premultiplied;
        // What reaches it (design section 5.2). The height is the original's own
        // depth, not the slot the port draws it in: a torch drawn in front of a
        // wall is not nearer to it, and a light 24 units above its owner at z -18
        // is 6 above the player at 0.
        sprite.height = Units::ToMetres(receiver.z);
        sprite.normalYDown = m_data.lighting.normalMapGreenDown;
        sprite.lightMask = m_lightMasksOff ? std::uint8_t{0} : Lighting::ReceiverMask(receiver.isStatic, receiver.applyLight, receiver.runtimeBake);
        // A sprite that takes no light has no use for a normal map, and naming none
        // keeps it in the material set it shares with its image's other copies.
        // Without one the engine samples the flat map: a sprite that applies light
        // and names no <Normal> is lit face-on, as Ethanon's default_nm.png lights it.
        if (receiver.applyLight) normal = receiver.normal;
    } else if (blend == MaterialComponent::BlendMode::Premultiplied) {
        blend = MaterialComponent::BlendMode::Alpha;
    }

    // Each written only when it changes: a still level writes nothing a tick,
    // and the overlay's and the normal map's paths are in SyncResources'
    // signature, which a rewrite of the same path would not move but a churn of
    // it would.
    if (material.albedoColor != colour) material.albedoColor = colour;
    if (material.sprite2D != sprite) material.sprite2D = sprite;
    // Empty on an unlit level: buildSprites takes a lightmap only from a look
    // that read.
    if (material.overlayTexturePath != lightmap) material.overlayTexturePath = lightmap;
    if (material.normalTexturePath != normal) material.normalTexturePath = normal;
    if (material.blend != blend) material.blend = blend;
}

// ---- the lights and their halos ---------------------------------------------------
//
// THE LIGHTS (design section 5.3). Each <Light> a level places is a Light2DComponent
// at its owner plus the light's offset, not turned with the owner
// (BuildChildLight, ETHEntityRenderingManager.cpp:174-184), with:
//   - its height the owner's depth plus the offset's z, in the original's units
//     like every sprite's (tint), so the one scale to metres cancels in the facing
//     and the falloff;
//   - its colour <Color> x the level's lightIntensity, and x the live share of the
//     owner's first particle system when the owner is not static
//     (Lighting::LightColour);
//   - its layer its owner's staticness (Lighting::LightLayer), which is what keeps
//     a torch out of the walls whose lightmaps already hold it;
//   - off while its owner is not drawn: a light on a hidden entity emits nothing
//     (ETHEntityRenderingManager.cpp:110-116).
//
// THE HALOS (design section 5.5, plan_port System 8). An added quad of the halo's
// size in world units at the owner plus the light's UNSCALED offset, coloured
// <Color> x haloBrightness x the owner's live particle share for any owner
// (ETHRenderEntity.cpp:354-388), x lighting.json's scale, with no ambient and no
// intensity, so it is not on the 2D sprite path at all. Drawn a quarter slot in
// front of its owner's picture: over what is behind the owner (the arches and sky
// of 1-1, which Enhanced's fit finds it adds over), under what is in front of
// it (1-1's wall04, where it finds none), and behind the owner's particles, which
// Ethanon draws after the halo at the same depth.
//
// THE SHOT carries projectile.ent's light and halo (art.json), made with the shot
// and gone with it. Not static, so it reaches every sprite that applies light,
// static walls included.
//
// All of it is presentation: nothing here reads back into Game::Level.

void MagicPortalsLayer::buildLights(entt::registry& registry) {
    using namespace Supersonic;
    unloadLights(registry);
    if (!m_lit || !m_artReady) return;
    // In the file's order, so a level's lights are gathered in the same order
    // every run; m_look's nodes are hashed.
    for (const Tscn::Node& node : m_data.scene.nodes) {
        if (node.parent != ".") continue;
        const auto look = m_look.nodes.find(node.name);
        if (look == m_look.nodes.end() || !look->second.light) continue;
        PlacedLight placed;
        placed.node = node.name;
        placed.light = *look->second.light;
        placed.ownerStatic = look->second.isStatic;
        placed.ownerZ = look->second.z;
        PositionOf(&node, placed.atPx);
        for (std::size_t i = 0; i < m_sprites.size(); ++i) {
            if (m_sprites[i].sprite.node == node.name) {
                placed.sprite = static_cast<int>(i);
                break;
            }
        }
        for (std::size_t i = 0; placed.sprite >= 0 && i < m_emitters.size(); ++i) {
            if (m_emitters[i].sprite == placed.sprite && m_emitters[i].slot == 0) {
                placed.emitter = static_cast<int>(i);
                break;
            }
        }
        if (placed.sprite >= 0) {
            placed.haloZ = slotOver(m_sprites[static_cast<std::size_t>(placed.sprite)].z, Slot::Halo);
        } else {
            // No picture to stand in front of: after the sprites at or below its
            // z_index, as the beholder is placed (the table's Slot::HaloNoPicture).
            //
            // NO LEVEL REACHES THIS. Every one of the 128 levels' 72 lights hangs
            // on a node that also draws a sprite, so this is the guard for a light
            // whose picture the port could not read. The counting rule it used to
            // write out by hand ("below < m_playerSlot") is the one step 76
            // replaced for the minions; it takes the table's now, which orders it
            // against the player by depth.
            int zIndex = 0;
            double z = 0.0;
            if (const Tscn::Value* value = node.Find("z_index"); value != nullptr && value->AsNumber(z)) {
                zIndex = static_cast<int>(z);
            }
            placed.haloZ = slotAfter(zIndex, Slot::HaloNoPicture);
        }
        placed.entity = registry.create();
        registry.emplace<TagComponent>(placed.entity, "Magic Portals 2D Light");
        registry.emplace<TransformComponent>(placed.entity);
        registry.emplace<Light2DComponent>(placed.entity);
        if (!placed.light.halo.empty() && imageSizePx(placed.light.halo) != glm::dvec2(0.0)) {
            placed.halo = makeSprite(registry, "Magic Portals Halo", placed.light.halo, true);
        }
        m_placedLights.push_back(std::move(placed));
    }

    // THE KEYS' OWN LIGHT (keys.json red_light, planning step 168): ETHCallback_key's AddLight for a key of the
    // colour the rule names - the red keys of 4-19 and 4-20. Not a <Light> of the node, so not in m_look: made from
    // the rule, at the key's picture, over the key node's own depth.
    const Keys::Rules::OwnLight& ownLight = m_level.keys.rules.ownLight;
    for (std::size_t i = 0; i < m_sprites.size(); ++i) {
        DrawnSprite& drawn = m_sprites[i];
        if (drawn.key < 0 || drawn.key >= static_cast<int>(m_level.keys.keys.size())) continue;
        if (!Keys::HasOwnLight(m_level.keys.rules, m_level.keys.keys[static_cast<std::size_t>(drawn.key)])) continue;
        KeyLight lit;
        lit.sprite = static_cast<int>(i);
        lit.light.offset = glm::dvec3(0.0, 0.0, ownLight.heightUnits);
        lit.light.colour = ownLight.colour;
        lit.light.range = ownLight.rangeUnits;
        lit.ownerZ = drawn.lookZ;
        // Where the key stands before any call has moved it: the first call's AddLight reads the node's position.
        lit.atPx = drawn.ownerPx;
        lit.entity = registry.create();
        registry.emplace<TagComponent>(lit.entity, "Magic Portals Key Light");
        registry.emplace<TransformComponent>(lit.entity);
        registry.emplace<Light2DComponent>(lit.entity);
        drawn.keyLight = static_cast<int>(m_keyLights.size());
        m_keyLights.push_back(std::move(lit));
    }
}

void MagicPortalsLayer::unloadLights(entt::registry& registry) {
    auto destroy = [&registry](entt::entity& e) {
        if (e != entt::null && registry.valid(e)) registry.destroy(e);
        e = entt::null;
    };
    for (PlacedLight& placed : m_placedLights) {
        destroy(placed.entity);
        destroy(placed.halo);
    }
    m_placedLights.clear();
    for (KeyLight& lit : m_keyLights) destroy(lit.entity);
    m_keyLights.clear();
    for (DrawnSprite& drawn : m_sprites) drawn.keyLight = -1;
    destroy(m_shotLight);
    destroy(m_shotHalo);
}

double MagicPortalsLayer::particleRatioOf(int emitter) const {
    if (emitter < 0 || emitter >= static_cast<int>(m_emitters.size())) return Lighting::ParticleRatio(0, 0);
    const Emitter& from = m_emitters[static_cast<std::size_t>(emitter)];
    // GetNumActiveParticles, as this frame's update counted it (Particles::Step):
    // released and bigger than nothing as the frame before left it. Not the quads
    // drawn, which a colour whose alpha is spent takes away from a count the
    // original keeps.
    return Lighting::ParticleRatio(from.active, from.system.count);
}

// One light and its halo where they stand, in the colours `ratio` gives them.
// `ownerScale` is the owner's m_properties.scale, which draws its light and halo
// (Lighting::OwnerScaled): 1 for a level placement, whose <Scale> the converter
// has already written into its light (00_order K7), and <Scale> x the script's
// Scale() for what the script ADDS - the shot's 0.8, and every effect the
// effects runtime spawns.
void MagicPortalsLayer::placeLight(entt::registry& registry, entt::entity light, entt::entity halo,
                                   const Lighting::Light& unscaled, const glm::dvec2& ownerPx, double ownerZ,
                                   bool ownerStatic, double ratio, float haloZ, bool present,
                                   const glm::dvec2& ownerScale) const {
    using namespace Supersonic;
    const double intensity = m_look.intensity;
    const double haloScale = m_data.lighting.haloBrightnessScale;
    const Lighting::Light from = Lighting::OwnerScaled(unscaled, ownerScale);
    if (light != entt::null && registry.valid(light)) {
        const glm::vec3 at = Units::ToWorld(ownerPx.x + from.offset.x, ownerPx.y + from.offset.y);
        registry.get<TransformComponent>(light).position = glm::vec3(at.x, at.y, 0.0f);
        auto& component = registry.get<Light2DComponent>(light);
        component.color = glm::vec3(Lighting::LightColour(from, intensity, ownerStatic, ratio));
        component.intensity = 1.0f;
        component.range = Units::ToMetres(from.range);
        component.height = Units::ToMetres(ownerZ + from.offset.z);
        component.layers = Lighting::LightLayer(ownerStatic);
        component.enabled = present;
    }
    if (halo != entt::null && registry.valid(halo)) {
        placeSprite(registry, halo, ownerPx + from.haloOffset, from.haloSize, haloZ, 0.0f);
        registry.get<RenderableComponent>(halo).isVisible = present;
        const glm::vec4 colour(glm::vec3(Lighting::HaloColour(from, ratio, haloScale)), 1.0f);
        auto& material = registry.get<MaterialComponent>(halo);
        if (material.albedoColor != colour) material.albedoColor = colour;
    }
}

void MagicPortalsLayer::syncLights(entt::registry& registry) {
    using namespace Supersonic;
    if (!m_loaded || !m_lit) return;

    // A light, its halo and where they stand, in the colours `ratio` gives them:
    // placeLight, which the effects runtime calls with the same rule.
    const auto place = [&](entt::entity light, entt::entity halo, const Lighting::Light& unscaled,
                           const glm::dvec2& ownerPx, double ownerZ, bool ownerStatic, double ratio, float haloZ,
                           bool present, const glm::dvec2& ownerScale) {
        placeLight(registry, light, halo, unscaled, ownerPx, ownerZ, ownerStatic, ratio, haloZ, present, ownerScale);
    };

    for (const PlacedLight& placed : m_placedLights) {
        bool present = true;
        glm::dvec2 ownerPx = placed.atPx;
        // A level placement: its <Scale> is in its light already (K7).
        glm::dvec2 ownerScale(1.0);
        if (placed.sprite >= 0) {
            const DrawnSprite& owner = m_sprites[static_cast<std::size_t>(placed.sprite)];
            present = owner.quad != entt::null && registry.valid(owner.quad);
            ownerPx = owner.ownerPx;
            // A FIRE AGENT'S BOUNCE SETS ITS SCALE, and its scale draws its light and its halo as it draws its
            // picture (art.json fire_agent pulse): the light's offset and range x scale.y (BuildChildLight,
            // ETHEntityRenderingManager.cpp:174-183), the halo's size x scale (DrawHalo, ETHRenderEntity.cpp:376-386).
            // The pulse is the level placement's scale on top of the 1 its <Scale> left in the light.
            if (owner.firePulse) ownerScale = m_artRules.fireAgent.pulse.ScaleAt(m_levelAgeMs - owner.blinkBornMs);
        }
        place(placed.entity, placed.halo, placed.light, ownerPx, placed.ownerZ, placed.ownerStatic,
              particleRatioOf(placed.emitter), placed.haloZ, present, ownerScale);
    }

    // THE KEYS' OWN: a script's light (AddLight), so the live layer, no particle share and no owner scale - the
    // key's sparkles do not dim it (ETHEntityRenderingManager.cpp:111-114 is for a light an entity owns) - while
    // the key's picture is there, wherever the carry has put it. Before the shot's block, which returns.
    for (const KeyLight& lit : m_keyLights) {
        const DrawnSprite& owner = m_sprites[static_cast<std::size_t>(lit.sprite)];
        const bool present = owner.quad != entt::null && registry.valid(owner.quad);
        place(lit.entity, entt::null, lit.light, lit.atPx, lit.ownerZ, false, Lighting::ParticleRatio(0, 0), 0.0f,
              present, glm::dvec2(1.0));
    }

    // (light_from_projectile.ent's light and halo stand at a lit torch as the
    // effects runtime's since the visuals plan's 11.2: updateEffects places them,
    // the halo at the light's own (0, -12).)

    // The shot's, while it flies.
    const Art::Shot& shot = m_artRules.shot;
    if (!m_level.portals.flight || !shot.light) {
        auto destroy = [&registry](entt::entity& e) {
            if (e != entt::null && registry.valid(e)) registry.destroy(e);
            e = entt::null;
        };
        destroy(m_shotLight);
        destroy(m_shotHalo);
        return;
    }
    if (m_shotLight == entt::null) {
        m_shotLight = registry.create();
        registry.emplace<TagComponent>(m_shotLight, "Magic Portals Shot Light");
        registry.emplace<TransformComponent>(m_shotLight);
        registry.emplace<Light2DComponent>(m_shotLight);
    }
    const Sprites::Image& haloImage = drawnImage(originalImage(shot.light->halo));
    if (m_shotHalo == entt::null && m_artReady && haloImage.units != glm::ivec2(0)) {
        m_shotHalo = makeSprite(registry, "Magic Portals Shot Halo", haloImage.path, true);
    }
    // projectile.ent has no particle system, so no ratio dims either. Scale(0.8)
    // does draw them smaller: a range of 56, a height of -9.6, a halo of 40 (K7).
    // Its halo just after its own picture, both behind the player (00_order K14);
    // at the entity's point, which the pivot does not move.
    place(m_shotLight, m_shotHalo, *shot.light, m_level.portals.flight->atPx, shot.z, shot.isStatic,
          Lighting::ParticleRatio(0, 0), slotAfter(shot.z, Slot::ShotHalo), true, glm::dvec2(shot.scale));
}

void MagicPortalsLayer::syncTurrets(entt::registry& registry) {
    using namespace Supersonic;
    const Turrets::State& turrets = m_level.turrets;
    auto destroy = [&registry](entt::entity& e) {
        if (e != entt::null && registry.valid(e)) registry.destroy(e);
        e = entt::null;
    };

    // One quad per fireball in flight, at its own Collision. A PLACEHOLDER since
    // step 85, not the picture: what the original shows is fireball.ent itself,
    // which latchSimEffects now adds for every fireball - its particle system and
    // its light's halo, since the .ent has no <Sprite> at all. The box is left
    // for the two cases every other placeholder is left for, a level with no art
    // to draw and the boxes turned on, and syncLighting hides it otherwise.
    while (m_fireballs.size() > turrets.fireballs.size()) {
        destroy(m_fireballs.back());
        m_fireballs.pop_back();
    }
    while (m_fireballs.size() < turrets.fireballs.size()) {
        m_fireballs.push_back(
            makeBox(registry, "Magic Portals Fireball", glm::vec3(0.0f), glm::vec3(1.0f), kFireballColour));
    }
    for (std::size_t i = 0; i < m_fireballs.size(); ++i) {
        placeBox(registry, m_fireballs[i], turrets.fireballs[i].atPx, glm::dvec2(kFireballBoxPx), kMarkerZ,
                 kMarkerDepth, 0.0f);
    }
}

void MagicPortalsLayer::syncBoss(entt::registry& registry) {
    using namespace Supersonic;
    const Boss::State& boss = m_level.boss;
    auto destroy = [&registry](entt::entity& e) {
        if (e != entt::null && registry.valid(e)) registry.destroy(e);
        e = entt::null;
    };
    if (!boss.beholder || boss.beholder->gone) {
        destroy(m_beholderBox);
        destroy(m_beholderQuad);
    } else {
        const Boss::Beholder& beholder = *boss.beholder;
        // Its reach - where a rising rock hurts it and the player dies - as a box.
        if (m_beholderBox == entt::null) {
            m_beholderBox =
                makeBox(registry, "Magic Portals Beholder", glm::vec3(0.0f), glm::vec3(1.0f), kBeholderColour);
            registry.emplace<InterpolatedTransformComponent>(m_beholderBox);
        }
        placeBox(registry, m_beholderBox, beholder.atPx, glm::dvec2(boss.rules.radiusPx * 2.0), kMarkerZ, kMarkerDepth,
                 0.0f);
        // beholder.ent's sheet: its eye open or shut, going red as it is hurt,
        // and pulsing as bounce() has it, but for while it throws rocks (art.json).
        const Art::Beholder& picture = m_artRules.beholder;
        // hd/beholder.png, 512 x 256 at density 2: two frames of 128 x 128 units,
        // as the 1x file's (drawnImage, frameUnits).
        const Sprites::Image& sheet = drawnImage(originalImage(picture.sprite));
        const glm::dvec2 cellPx = frameUnits(sheet, picture.columns, picture.rows);
        if (m_beholderQuad == entt::null && m_artReady && cellPx != glm::dvec2(0.0)) {
            m_beholderQuad = makeSprite(registry, "Magic Portals Beholder Sprite", sheet.path, picture.additive);
            registry.emplace<InterpolatedTransformComponent>(m_beholderQuad);
            auto& animation = registry.emplace<SpriteAnimationComponent>(m_beholderQuad);
            animation.columns = static_cast<uint32_t>(picture.columns);
            animation.rows = static_cast<uint32_t>(picture.rows);
            animation.frameCount = 1;
            animation.playing = false;
        }
        if (m_beholderQuad != entt::null) {
            auto& animation = registry.get<SpriteAnimationComponent>(m_beholderQuad);
            animation.firstFrame = static_cast<uint32_t>(beholder.frame);
            animation.frame = 0;
            switch (beholder.phase) {
            case Boss::Phase::Seeking: m_beholderScale = picture.seeking.ScaleAt(beholder.pulseMs); break;
            case Boss::Phase::GotDamage: m_beholderScale = picture.hurt.ScaleAt(beholder.pulseMs); break;
            case Boss::Phase::Dead: m_beholderScale = picture.dead.ScaleAt(beholder.pulseMs); break;
            case Boss::Phase::ThrowRock: break;
            }
            placeSprite(registry, m_beholderQuad, beholder.atPx, cellPx * m_beholderScale, m_beholderZ, 0.0f);
            const float left = static_cast<float>(std::max(beholder.hp, 0)) / static_cast<float>(boss.rules.maxHp);
            m_beholderColour = glm::vec4(1.0f, left, left, 1.0f);
        }
    }

    // Its spikes: beholder_spike.ent turned to where each flies and standing on
    // its pivot, or small boxes without the image.
    const Art::Spike& spike = m_artRules.spike;
    const Sprites::Image& image = drawnImage(originalImage(spike.sprite));
    const glm::dvec2 imagePx(image.units);
    const bool pictured = m_artReady && imagePx != glm::dvec2(0.0);
    while (m_spikes.size() > boss.spikes.size()) {
        destroy(m_spikes.back());
        m_spikes.pop_back();
    }
    while (m_spikes.size() < boss.spikes.size()) {
        m_spikes.push_back(pictured ? makeSprite(registry, "Magic Portals Spike", image.path, spike.additive)
                                    : makeBox(registry, "Magic Portals Spike", glm::vec3(0.0f), glm::vec3(1.0f),
                                              kSpikeColour));
    }
    for (std::size_t i = 0; i < m_spikes.size(); ++i) {
        const Boss::Spike& flying = boss.spikes[i];
        const glm::dvec2 d = flying.directionPx;
        // The image's down is turned onto its way, and so its right onto
        // (dy, -dx). The picture stands with its pivot on the spike.
        const glm::dvec2 pivot = spike.pivotXPx * glm::dvec2(d.y, -d.x) + spike.pivotYPx * d;
        const float rotation = std::atan2(static_cast<float>(-d.y), static_cast<float>(d.x)) + 1.5707964f;
        if (pictured) {
            placeSprite(registry, m_spikes[i], flying.atPx - pivot, imagePx, m_spikeZ, rotation);
        } else {
            placeBox(registry, m_spikes[i], flying.atPx, glm::dvec2(kSpikeBoxPx), kMarkerZ, kMarkerDepth, rotation);
        }
    }
}

void MagicPortalsLayer::matchMinions() {
    // The sim's list only grows - a summoned escort is pushed onto it - so one
    // entry each, in its order, added as they come.
    const std::vector<Minions::Minion>& minions = m_level.minions.minions;
    const Art::Minion& art = m_artRules.minion;
    while (m_minions.size() < minions.size()) {
        const Minions::Minion& minion = minions[m_minions.size()];
        MinionQuad made;
        made.pose.row = art.startRow;
        // ghost_minion.ent, the entity level31b's marker adds (R16); the boss's
        // escorts are minion.ent (ETHCallback_ghost ins 1137, 'minion.ent').
        made.black = minion.entity == art.blackEntity;
        // And whether that entity carries particle systems of its own, read off its
        // .ent once per entity: ghost_minion.ent's aura, and nothing on a minion.ent.
        auto known = m_entityCarriesSystems.find(minion.entity);
        if (known == m_entityCarriesSystems.end()) {
            std::vector<Particles::System> systems;
            std::string error;
            const bool carries =
                !m_paths.original.empty() &&
                Particles::Load(m_paths.original + "/entities/" + minion.entity, systems, error) && !systems.empty();
            known = m_entityCarriesSystems.emplace(minion.entity, carries).first;
        }
        made.systems = known->second;
        made.z = minion.summoned ? m_escortZ : m_minionZ;
        made.lookZ = minion.summoned ? m_escortLookZ : m_artRules.LevelDepth(art.z);
        m_minions.push_back(made);
    }
}

void MagicPortalsLayer::advanceMinions(const ScriptClock::Step& step) {
    matchMinions();
    const Art::Minion& art = m_artRules.minion;
    const std::vector<Minions::Minion>& minions = m_level.minions.minions;
    const double ms = ScriptClock::Ms(step);
    for (std::size_t i = 0; i < minions.size(); ++i) {
        const Minions::Minion& minion = minions[i];
        // The sim's arms, one for one. A knight that holds with something in sight is NOT given its standing
        // frame (ETHCallback_minion #1184-1190: SetFrame only `if (entityInSight == null)`), so Watch sets nothing,
        // as the lone guard's Settled does; and a knight whose way is blocked searches (#1229-1321), the arm
        // art.json minion.search was written for.
        Art::MinionArm arm = Art::MinionArm::None;
        switch (minion.arm) {
        case Minions::Arm::Walk: arm = Art::MinionArm::Walk; break;
        case Minions::Arm::Hold: arm = Art::MinionArm::Hold; break;
        case Minions::Arm::Settled: arm = Art::MinionArm::Settled; break;
        case Minions::Arm::Watch: arm = Art::MinionArm::Settled; break;
        case Minions::Arm::Search: arm = Art::MinionArm::Search; break;
        case Minions::Arm::None: break;
        }
        Art::UpdateMinion(art, m_minions[i].pose, arm, minion.facing == Minions::Facing::Right, ms, minion.elapsedMs);
    }
}

void MagicPortalsLayer::syncMinions(entt::registry& registry) {
    using namespace Supersonic;
    matchMinions();
    const Art::Minion& art = m_artRules.minion;
    const std::vector<Minions::Minion>& minions = m_level.minions.minions;
    const glm::dvec2 framePx(m_minionCut.frameUnits);
    for (std::size_t i = 0; i < minions.size(); ++i) {
        const Minions::Minion& minion = minions[i];
        MinionQuad& drawn = m_minions[i];
        const bool standing = !minion.gone && minion.body != entt::null && registry.valid(minion.body) &&
                              registry.all_of<TransformComponent>(minion.body);
        if (!standing || !m_artReady || m_minionSheet.empty()) {
            if (drawn.quad != entt::null && registry.valid(drawn.quad)) registry.destroy(drawn.quad);
            drawn.quad = entt::null;
            continue;
        }
        if (drawn.quad == entt::null) {
            drawn.quad = makeSprite(registry, "Magic Portals Minion", m_minionSheet, art.additive);
            // Bodies move on the tick and are drawn at its place.
            registry.emplace<InterpolatedTransformComponent>(drawn.quad);
        }
        // Its feet on what the port's one circle stands on, the entity the
        // compound's feet bottom below them, and the picture on its pivot, as
        // Ethanon draws it (the player's rule). The body turns on nothing: it is
        // fixedRotation.
        const glm::dvec2 bodyPx = Units::ToPixels(registry.get<TransformComponent>(minion.body).position);
        const glm::dvec2 entityPx =
            bodyPx + glm::dvec2(0.0, m_level.minions.rules.bodyRadiusPx - art.feetBelowEntityPx);
        drawn.centrePx = entityPx - glm::dvec2(art.pivotXPx, art.pivotYPx);
        placeSprite(registry, drawn.quad, drawn.centrePx, framePx, drawn.z, 0.0f);
        // The frame as the original's rect for it: its stride in whole units, so
        // the uv is written here and not by a flipbook's even split.
        auto& material = registry.get<MaterialComponent>(drawn.quad);
        material.uvScale = glm::vec2(m_minionCut.uvScale);
        material.uvOffset = glm::vec2(Tiers::UvOffset(m_minionCut, drawn.pose.column, drawn.pose.row));
    }
}

// ---- chapter 3's boss (the visuals plan's 12.2, planning step 98) ----------------
//
// ghost.ent as ETHCallback_ghost draws it: added by the black knight's death at
// the knight's point (R4, Game), at the .ent's own white and alpha 1 and the
// spawn's scale 0.3 until its first callback writes its Look, which the sim keeps
// - the waypoints' colour, alpha and scale as it flies to ghost_pos, cell 0 in
// SHOOTING and 3 at alpha 0.5 in RAGE_MODE, the bounces in DAMAGE and DEATH. Drawn
// with its pivot on its point at that scale, the cell cut as the original cuts
// it, one level unit behind its knight (Slot::Ghost), and coloured in
// syncLighting. Made when it is added, unmade when it deletes itself.
void MagicPortalsLayer::syncGhost(entt::registry& registry) {
    using namespace Supersonic;
    const Ghost::State& ghost = m_level.ghost;
    if (!ghost.Alive() || !m_artReady || m_ghost.sheet.empty()) {
        if (m_ghost.quad != entt::null && registry.valid(m_ghost.quad)) registry.destroy(m_ghost.quad);
        m_ghost.quad = entt::null;
        return;
    }
    const Art::Ghost& art = m_artRules.ghost;
    if (m_ghost.quad == entt::null) {
        m_ghost.quad = makeSprite(registry, "Magic Portals Ghost", m_ghost.sheet, art.additive);
        // It moves on the tick - its flight, its hover - and is drawn at its place.
        registry.emplace<InterpolatedTransformComponent>(m_ghost.quad);
        SUPERSONIC_LOG_INFO("Magic Portals") << "the ghost is added at " << ghost.atPx.x << ", " << ghost.atPx.y
                                             << " on tick " << m_ticks << std::endl;
    }
    const int frames = std::max(1, art.Frames());
    m_ghost.cell = std::clamp(ghost.look.frame, 0, frames - 1);
    const double scale = ghost.look.scale;
    m_ghost.sizePx = glm::dvec2(m_ghost.cut.frameUnits) * scale;
    m_ghost.centrePx = ghost.atPx - glm::dvec2(art.pivotXPx, art.pivotYPx) * scale;
    placeSprite(registry, m_ghost.quad, m_ghost.centrePx, m_ghost.sizePx, m_ghost.z, 0.0f);
    const int columns = std::max(1, art.columns);
    auto& material = registry.get<MaterialComponent>(m_ghost.quad);
    material.uvScale = glm::vec2(m_ghost.cut.uvScale);
    material.uvOffset = glm::vec2(Tiers::UvOffset(m_ghost.cut, m_ghost.cell % columns, m_ghost.cell / columns));
}

MagicPortalsLayer::GhostReport MagicPortalsLayer::GhostDrawing() const {
    GhostReport report;
    const Ghost::State& ghost = m_level.ghost;
    report.present = ghost.present;
    report.spawned = ghost.spawned;
    report.drawn = m_ghost.quad != entt::null;
    report.quad = m_ghost.quad;
    report.texture = m_ghost.sheet;
    report.cell = m_ghost.cell;
    report.colour = ghost.look.colour;
    report.alpha = ghost.look.alpha;
    report.scale = ghost.look.scale;
    report.atPx = ghost.atPx;
    report.centrePx = m_ghost.centrePx;
    report.sizePx = m_ghost.sizePx;
    report.frameUnits = glm::dvec2(m_ghost.cut.frameUnits);
    report.uvScale = m_ghost.cut.uvScale;
    const int columns = std::max(1, m_artRules.ghost.columns);
    report.uvOffset = Tiers::UvOffset(m_ghost.cut, m_ghost.cell % columns, m_ghost.cell / columns);
    report.z = m_ghost.z;
    report.lookZ = m_ghost.lookZ;
    return report;
}

MagicPortalsLayer::DragonReport MagicPortalsLayer::DragonPicture() const {
    DragonReport report;
    const Dragon::State& dragon = m_level.dragon;
    if (!dragon.present) return report;
    report.present = true;
    report.frame = dragon.frame;
    report.angleDeg = dragon.angleDeg;
    report.atPx = dragon.atPx;
    report.systemKilled = dragon.systemKilled;
    report.clawWaypoint = dragon.claw.waypoint;
    report.clawAngleDeg = dragon.claw.AngleDeg(dragon.rules);
    report.sizePx = glm::dvec2(m_dragonCut.frameUnits);
    report.uvScale = m_dragonCut.uvScale;
    const int columns = std::max(1, m_data.dragon.columns);
    report.uvOffset = Tiers::UvOffset(m_dragonCut, dragon.frame % columns, dragon.frame / columns);
    if (m_dragonSprite >= 0 && m_dragonSprite < static_cast<int>(m_sprites.size())) {
        const DrawnSprite& drawn = m_sprites[static_cast<std::size_t>(m_dragonSprite)];
        report.quad = drawn.quad;
        report.centrePx = drawn.centrePx;
    }
    if (m_clawSprite >= 0 && m_clawSprite < static_cast<int>(m_sprites.size())) {
        const DrawnSprite& drawn = m_sprites[static_cast<std::size_t>(m_clawSprite)];
        report.clawQuad = drawn.quad;
        report.clawCentrePx = drawn.centrePx;
    }
    return report;
}

MagicPortalsLayer::DoorReport MagicPortalsLayer::DoorPicture() const {
    DoorReport report;
    report.shadowless = m_shadowlessDoor;
    report.bobs = m_doorBobRuns;
    report.motion = m_doorBob;
    report.offsetPx = Motion::OffsetPx(m_doorBob);
    const auto at = [this](int index) -> const DrawnSprite* {
        return index >= 0 && index < static_cast<int>(m_sprites.size())
                   ? &m_sprites[static_cast<std::size_t>(index)]
                   : nullptr;
    };
    if (const DrawnSprite* door = at(m_doorSprite); door != nullptr) {
        report.quad = door->quad;
        report.texture = door->sprite.texture;
        report.atPx = door->ownerPx;
    }
    if (const DrawnSprite* background = at(m_doorBgSprite); background != nullptr) {
        report.bgQuad = background->quad;
        report.bgAtPx = background->ownerPx;
    }
    if (const DrawnSprite* glow = at(m_doorHighlight); glow != nullptr) {
        report.glowQuad = glow->quad;
        report.glowTexture = glow->sprite.texture;
        report.glowCentrePx = glow->centrePx;
        report.glowSizePx = glow->sprite.sizePx;
        report.glowZ = glow->z;
        report.glowAdditive = glow->sprite.additive;
    }
    return report;
}

std::vector<MagicPortalsLayer::TorchReport> MagicPortalsLayer::TorchReports() const {
    std::vector<TorchReport> reports;
    for (std::size_t i = 0; i < m_level.torch.lights.size(); ++i) {
        TorchReport report;
        for (const DrawnSprite& drawn : m_sprites) {
            if (drawn.torch == static_cast<int>(i)) report.levelPicture = drawn.quad != entt::null;
        }
        if (i < m_torchWatch.size()) {
            report.unlitGone = m_torchWatch[i].unlitGone;
            report.z = m_torchWatch[i].z;
            report.turnedDeg = m_torchWatch[i].spin.turnedDeg;
        }
        for (const DrawnSprite& drawn : m_sprites) {
            if (drawn.torch == static_cast<int>(i)) report.systemsAngleDeg = drawn.ownerAngleDeg;
        }
        reports.push_back(report);
    }
    return reports;
}

double MagicPortalsLayer::WellSpinDegrees(int well) const {
    const GravityWell::Roundabout& rules = m_level.wells.rules.roundabout;
    if (well < 0 || well >= static_cast<int>(m_wellSpins.size())) {
        return GravityWell::SpinDegrees(rules, rules.spinMs, 1.0);
    }
    const WellSpin& spin = m_wellSpins[static_cast<std::size_t>(well)];
    return GravityWell::SpinDegrees(rules, spin.ms, spin.sign);
}

MagicPortalsLayer::LightWallGhostReport MagicPortalsLayer::LightWallGhost() const {
    LightWallGhostReport report;
    report.possible = m_level.torch.wall.present && m_wallGhost.sizePx != glm::dvec2(0.0);
    report.added = m_wallGhost.added;
    report.drawn = m_wallGhost.drawn && m_wallGhost.quad != entt::null;
    report.quad = m_wallGhost.quad;
    report.texture = m_wallGhost.texture;
    report.atPx = m_wallGhost.atPx;
    report.sizePx = m_wallGhost.sizePx;
    report.scale = m_wallGhost.scale;
    report.alpha = m_wallGhost.alpha;
    report.angleDeg = m_wallGhost.turn.turnedDeg;
    report.pulseMs = m_wallGhost.pulseMs;
    report.z = m_wallGhost.z;
    report.rotation = m_wallGhost.rotation;
    report.lookZ = m_wallGhost.lookZ;
    return report;
}

void MagicPortalsLayer::buildHud(entt::registry& registry) {
    using namespace Supersonic;
    auto label = [&registry](const char* name, UIAnchor anchor, glm::vec2 offset, float size) {
        const entt::entity e = registry.create();
        registry.emplace<TagComponent>(e, name);
        auto& text = registry.emplace<UITextComponent>(e);
        text.anchor = anchor;
        text.offset = offset;
        text.fontSize = size;
        text.text = "";
        return e;
    };
    m_hud.status = label("Magic Portals Status", UIAnchor::TopLeft, glm::vec2(24.0f, 18.0f), 26.0f);
    m_hud.result = label("Magic Portals Result", UIAnchor::TopLeft, glm::vec2(24.0f, 56.0f), 20.0f);
    m_hud.controls = label("Magic Portals Controls", UIAnchor::BottomLeft, glm::vec2(24.0f, 24.0f), 20.0f);
}

void MagicPortalsLayer::updateHud(entt::registry& registry) {
    using namespace Supersonic;
    auto set = [&registry](entt::entity e, std::string text) {
        if (e != entt::null && registry.valid(e)) registry.get<UITextComponent>(e).text = std::move(text);
    };
    std::string status;
    if (menuStateUp()) {
        // NO TEXT on a menu state (ui3 spec D7): every picture the original draws
        // there is its own art, and its own fonts where it writes anything.
    } else if (m_current < 0) {
        status = "Magic Portals could not start: " + m_loadError;
    } else {
        // A chapter's end is chapter select now (goTo), which is a menu state; only a
        // level the port refused is left with no level and no menu.
        const Chapters::Level& entry = m_chapters.levels[static_cast<std::size_t>(m_current)];
        if (!m_loaded) status = Chapters::Label(entry) + " is not playable yet: " + m_loadError;
        // A level being played says NOTHING. The original draws no text over a
        // level at all - no level number, no portal or crystal count - and that
        // is measured rather than remembered: across the settled frames of 41
        // different levels the only screen-fixed regions are the two button
        // corners and the two pads (static_hud.md section 6). The counts this line
        // carried are on the medal screen, where the original puts them.
    }
    set(m_hud.status, status);

    std::string result;
    // Nothing over a level, the finished and lost screens included: those draw
    // the original's own count and medal over it. Nor over a menu state (ui3 spec
    // D7) - chapter select after a chapter's last level, the grid after the list
    // button - which this line was written before, for the black a chapter's end
    // used to leave.
    if (m_lastCleared && !m_loaded && !menuStateUp()) {
        const Cleared& c = *m_lastCleared;
        result = c.label + " cleared with " + Count(c.portalsUsed) + (c.portalsUsed == 1 ? " portal" : " portals") +
                 (c.crystalsTotal > 0 ? ", " + Count(c.crystals) + "/" + Count(c.crystalsTotal) + " crystals" : "") +
                 (c.Gold() ? " - gold" : "");
    }
    set(m_hud.result, result);
    // The keys are the port's, and the menus and a level have the original's own
    // buttons on them now, so the help goes with the rest of the text.
    set(m_hud.controls, "");
}

// ---- the tick ----------------------------------------------------------------

void MagicPortalsLayer::buildControls() {
    unloadControls();
    // THE MODERN FIVE come first and need no art: a modern control is never dropped for a picture that is
    // missing (a flat quad stands in), where classic's rule is "no picture, no control". They still need the
    // HUD's rules - the slide, the pulse and the ring are ui.json's, which the module takes as an argument.
    if (playsModern() && m_hudReady && m_current >= 0) buildModernControls();
    // No art, no HUD: the level is being drawn as boxes, and the keys still work.
    if (!m_artReady || !m_hudReady || m_current < 0) return;
    const Chapters::Level& entry = m_chapters.levels[static_cast<std::size_t>(m_current)];

    // A picture the HUD cannot read is said once, here, rather than drawn as
    // the white the renderer falls back to.
    const auto readable = [this](const std::string& image) -> std::string {
        if (imageSizePx(image) == glm::dvec2(0.0)) {
            SUPERSONIC_LOG_WARN("Magic Portals") << "HUD image could not be read: " << image << std::endl;
            return {};
        }
        return image;
    };

    const struct Made {
        Control kind;
        const std::string& file;
    } made[] = {
        {Control::Left, m_hudRules.pads.leftSprite},
        {Control::Right, m_hudRules.pads.rightSprite},
        {Control::Reset, m_hudRules.restart.sprite},
        {Control::Menu, m_hudRules.pause.sprite},
        {Control::Clear, m_hudRules.clearPortals.sprite},
    };
    for (const Made& one : made) {
        // The modern controls were built above, from their own pictures.
        if (playsModern()) break;
        ControlButton button;
        button.kind = one.kind;
        // menuImage prefers sprites/hd/, which is what the original draws on any
        // screen over 480 px tall, and falls back for a file with no hd twin.
        button.image = readable(menuImage(one.file));
        // A control whose picture is missing is not a control either: a tap
        // must not be eaten by something nobody can see.
        if (!button.image.empty()) m_controls.push_back(button);
    }

    // The ring, only where it is ever drawn. ring_sprite.png has no hd twin,
    // and needs none: it is drawn at a stated size, not its own.
    if (tutorialPads()) m_ringImage = readable(menuImage(m_hudRules.pads.ringSprite));

    // The plaque, only for a level with a medal recorded: addCurrentMedalSprite
    // adds nothing where getScore is 0, so a fresh save shows no plaque at all.
    if (const std::string medal = Hud::MedalSprite(m_hudRules, m_scores.Get(entry.world, entry.index));
        !medal.empty()) {
        m_plaqueImage = readable(menuImage(m_hudRules.plaque.sprite));
        m_medalImage = readable(menuImage(medal));
    }

    // "Part N", over the font's own pages. Read once, the first time a level
    // wants it: the font is the original's and lives beside its other data, so
    // a machine without it plays with no caption and says so.
    if (!m_captionFontTried) {
        m_captionFontTried = true;
        std::string why;
        const std::string font = m_paths.original + "/data/" + m_hudRules.caption.font;
        if (!m_captionFont.Load(font, why)) {
            SUPERSONIC_LOG_WARN("Magic Portals") << "no level caption: " << why << std::endl;
        }
    }
    m_captionText = Hud::CaptionText(m_hudRules, entry.index);

    // The pause's pictures, each resolved once for the level: any of them may be
    // drawn the moment the pause opens.
    for (const std::string* file :
         {&m_pauseRules.goldenPlaque.sprite, &m_pauseRules.currentPlaque.sprite, &m_pauseRules.medalBronze,
          &m_pauseRules.medalSilver, &m_pauseRules.medalGold, &m_pauseRules.levels.sprite,
          &m_pauseRules.resume.sprite, &m_pauseRules.skip.sprite, &m_pauseRules.achievements.sprite,
          &m_pauseRules.sound.sprite, &m_pauseRules.soundOffSprite, &m_pauseRules.music.sprite,
          &m_pauseRules.musicOffSprite}) {
        if (m_pauseImages.find(*file) == m_pauseImages.end()) m_pauseImages[*file] = readable(menuImage(*file));
    }
    // And the finished and lost screens', with each one's size in texels: the
    // veil is drawn as a clamped texture, which is split by its texels. The
    // crystal is an entity's picture, whose hd twin sits beside it.
    const LevelEnd::Rules::Finished& finished = m_levelEndRules.finished;
    const LevelEnd::Rules::Lost& lost = m_levelEndRules.lost;
    for (const std::string* file :
         {&finished.veil.sprite, &finished.title.sprite, &finished.portalsPlaque.sprite, &finished.goldenPlaque.sprite,
          &finished.restart.sprite, &finished.next.sprite, &finished.list.sprite, &finished.medalBronze,
          &finished.medalSilver, &finished.medalGold, &lost.veil.sprite, &lost.title.sprite, &lost.restart.sprite,
          &lost.list.sprite}) {
        if (m_endImages.find(*file) == m_endImages.end()) m_endImages[*file] = readable(menuImage(*file));
    }
    if (m_endImages.find(finished.crystalSprite) == m_endImages.end()) {
        std::error_code ec;
        const std::string hd = m_paths.original + "/entities/hd/" + finished.crystalSprite;
        m_endImages[finished.crystalSprite] =
            readable(std::filesystem::exists(hd, ec) ? hd : originalImage(finished.crystalSprite));
    }
    for (const auto& [file, image] : m_endImages) {
        m_endTexels[file] = image.empty() ? glm::ivec2(0) : glm::ivec2(imageSizePx(image));
    }
    // And the popups': the framework's and every class's, named within the
    // original's assets rather than by file alone - the hand and the stone are
    // entities, the smoke a particle - each the hd twin where one exists.
    const auto popupImage = [&](const std::string& file) {
        if (m_popupImages.find(file) == m_popupImages.end()) m_popupImages[file] = readable(originalAsset(file));
    };
    popupImage(m_popupRules.card.sprite);
    popupImage(m_popupRules.closeButton.sprite);
    for (const Popup::Class& popup : m_popupRules.classes) {
        popupImage(popup.card.sprite);
        for (const Popup::Item& item : popup.items) popupImage(item.sprite);
    }

    // And their fonts, where they are not the caption's, read once for the run.
    const Pause::Rules& pause = m_pauseRules;
    for (const std::string* name : {&pause.title.font, &pause.goldenNumber.font, &finished.counter.font,
                                    &finished.goldenNumber.font, &finished.crystalCount.font}) {
        if (*name == m_hudRules.caption.font || m_uiFonts.find(*name) != m_uiFonts.end()) continue;
        std::string why;
        if (!m_uiFonts[*name].Load(m_paths.original + "/data/" + *name, why)) {
            SUPERSONIC_LOG_WARN("Magic Portals") << "no text in " << *name << ": " << why << std::endl;
        }
    }
}

void MagicPortalsLayer::buildModernControls() {
    // All five, in the claim order, which is the order a touch is asked in (controlUnderPointer).
    for (const Control kind : Controls::kClaimOrder) {
        ControlButton button;
        button.kind = kind;
        button.image = m_modernImages[static_cast<std::size_t>(kind)];
        const auto& entry = m_controlsRules.art[static_cast<std::size_t>(Controls::ArtTier::Generated)][static_cast<std::size_t>(kind)];
        button.bodyInset = entry ? entry->bodyInset : 0.0;
        m_controls.push_back(button);
    }
}

void MagicPortalsLayer::unloadControls() {
    // The modern pictures (m_modernImages) are the run's, not the level's: not cleared here.
    m_controls.clear();
    m_ringImage.clear();
    m_plaqueImage.clear();
    m_medalImage.clear();
    m_captionText.clear();
    m_pauseImages.clear();
    m_endImages.clear();
    m_endTexels.clear();
    m_popupImages.clear();
}

const Supersonic::BitmapFont* MagicPortalsLayer::uiFont(const std::string& name) const {
    if (name == m_hudRules.caption.font) return &m_captionFont;
    const auto found = m_uiFonts.find(name);
    return found != m_uiFonts.end() ? &found->second : nullptr;
}

bool MagicPortalsLayer::tutorialPads() const {
    const Chapters::Level* level = Current();
    return m_hudReady && level != nullptr && level->name == m_hudRules.pads.tutorialLevel;
}

void MagicPortalsLayer::layOutControls() {
    const glm::dvec2 view = ViewPx();
    // Only while a level is being played: the menu screens have their own
    // buttons.
    const bool playing = m_loaded && m_screen == Screen::None && !m_finishing && !m_dying;
    // AND AS A LEVEL ENDS, from the door or the death on, finished and lost
    // screens included (spec 3.4, 4.1, D8): the pads decay from the byte the
    // pulse left them at, and nothing is pressed - the tick reads no input for
    // an ended level.
    const bool ended = m_loaded && (m_finishing || m_dying);
    // A weightless level has no walk pads at all: MainCharacter neither updates
    // nor draws them when noGravity is set. Nor are they drawn while a pause has
    // game time stopped (spec 2.2, measured gain 0.010 / -0.025), where restart,
    // pause and clear-portals stay drawn under its dim, frozen and unpressed:
    // the tick reads no input for a level while a pause is up.
    // A popup stops game time the same way, and the pads go with it (spec 5.2).
    const bool pads = (playing || (ended && m_padEndByte > 0)) && !m_level.portals.noGravity && !GameTimeStopped();
    const double padAlpha = playing ? Controls::ClassicWalkOpacity(m_hudRules, m_levelAgeMs, tutorialPads())
                                    : static_cast<double>(m_padEndByte) / 255.0;
    // Restart, pause and clear-portals: CUT at the door, in one frame (gain
    // 0.4705 -> -0.034), and DISMISSED at a death as UIButtons, 700 ms out along
    // their rays. Both are measurements and the decode explains only the second
    // (spec U1); both are followed.
    // While playing, restart and pause are GameLayer's UIButtons, and come IN as
    // UIButtons on GameLayer's updates - the level's age, which a popup raised as
    // the level loads holds at zero: under 1-02's and 1-03's popups they are not
    // there to see, and they slide in once it has gone (spec 5.4, A-H9). On any
    // other level the entrance is over at 700 ms, under the opening's black.
    // Clear-portals keeps the flat 120 it had: PortalManager adds it with each
    // portal, and no capture shows its entrance. It is told from restart and pause
    // by which control it is, not by which of the rules' members it was handed.
    const auto corner = [&](Control kind, bool present) {
        const Hud::Placement& placement = Controls::ClassicPlacement(m_hudRules, kind);
        if (playing && kind != Control::Clear) {
            const LevelEnd::Dismissed in =
                LevelEnd::HudEntered(m_levelEndRules, placement, m_hudRules.alphaByte, view, m_levelAgeMs);
            return std::make_tuple(in.rect, present && in.shown, in.alpha);
        }
        if (playing) {
            return std::make_tuple(Controls::ClassicPlace(m_hudRules, kind, view), present, Hud::Opacity(m_hudRules));
        }
        if (m_dying && present) {
            const LevelEnd::Dismissed out =
                LevelEnd::HudDismissed(m_levelEndRules, placement, m_hudRules.alphaByte, view, m_dyingClockMs);
            return std::make_tuple(out.rect, out.shown, out.alpha);
        }
        return std::make_tuple(Controls::ClassicPlace(m_hudRules, kind, view), false, 0.0);
    };

    // THE MODERN CONTROLS (planning step 182), BESIDE the classic code below, which is not on this path and is not
    // touched: what is shown, and when, is the same rule (the pads while the level plays or decays, a weightless level
    // none, a pause and a popup none; restart and pause entering and being dismissed; clear while a portal lives), and
    // what differs is where each is, what it claims and the byte it is drawn at - all from sim/Controls.
    if (playsModern()) {
        const Controls::Rules& rules = m_controlsRules;
        // Only the look of the tuning is applied until CTL-10 (resolveControls).
        const Controls::Tuning& tuning = m_controlsApplied;
        const Controls::Insets& insets = m_paths.controls.testInsets;
        const Controls::Layout layout =
            Controls::LayOut(Controls::Style::Modern, rules, m_hudRules, tuning, view, insets, m_levelAgeMs);
        // The pads' byte: the pulse's while playing, and what a level's end left them at as they decay. The decay's start
        // byte is the classic byte in both styles (endHud), and the opacity and the idle scale multiply at the draw.
        const int padByte = playing ? Controls::ClassicWalkAlphaByte(m_hudRules, m_levelAgeMs, tutorialPads()) : m_padEndByte;
        const double padAlphaModern = static_cast<double>(Controls::AlphaByte(rules, tuning, padByte)) / 255.0;
        // The same multiplier on the corner buttons' own alpha, which is a fraction (their entrance is a ramp). At the
        // defaults it is 1.0 and the alpha is the classic one to the bit.
        const double shade = rules.idleScale * tuning.Opacity(rules);
        for (ControlButton& button : m_controls) {
            const Controls::Frame& settled = layout.At(button.kind);
            Controls::Frame frame = settled;
            switch (button.kind) {
            case Control::Left:
            case Control::Right:
                // LayOut has slid each arrow in from its nearer side, with its hit rectangle.
                button.shown = pads;
                button.alpha = padAlphaModern;
                break;
            case Control::Reset:
            case Control::Menu:
            case Control::Clear: {
                // As Hud::Placement, so LevelEnd's entrance and dismissal run on it unchanged (the classic ones' own).
                const Hud::Placement placement =
                    Controls::PlacementFor(rules, m_hudRules, tuning, button.kind, view, insets);
                const bool clear = button.kind == Control::Clear;
                // PortalManager::update: none in a level that grants no portal, and otherwise exactly while a placed one is
                // alive - and at a death, dismissed with the other two if it was up.
                const bool present = clear ? (playing ? m_level.portals.budget > 0 && !m_level.portals.placed.empty()
                                                      : m_clearShownAtEnd)
                                           : true;
                Hud::Rect drawn = settled.rect;
                double alpha = 0.0;
                bool shown = false;
                if (playing && !clear) {
                    const LevelEnd::Dismissed in =
                        LevelEnd::HudEntered(m_levelEndRules, placement, m_hudRules.alphaByte, view, m_levelAgeMs);
                    drawn = in.rect;
                    shown = present && in.shown;
                    alpha = in.alpha;
                } else if (playing) {
                    shown = present;
                    alpha = Hud::Opacity(m_hudRules);
                } else if (m_dying && present) {
                    const LevelEnd::Dismissed out =
                        LevelEnd::HudDismissed(m_levelEndRules, placement, m_hudRules.alphaByte, view, m_dyingClockMs);
                    drawn = out.rect;
                    shown = out.shown;
                    alpha = out.alpha;
                }
                // The hit rectangle goes with the picture while it comes in or goes out.
                frame = Controls::FollowDrawn(settled, drawn);
                button.shown = shown;
                button.alpha = std::min(1.0, alpha * shade);
                break;
            }
            }
            button.rect = frame.rect;
            button.hit = frame.hit;
            // A picture is drawn so that its body fills the box's face; a flat quad IS the face.
            button.picture = Controls::DrawRect(frame.rect, rules.faceInset, button.image.empty() ? 0.0 : button.bodyInset);
            if (claimsClassic()) {
                // THE TEST VARIANT: the claim is the classic one - the pads' discs about the screen corners, the corner
                // buttons' drawn classic rectangle - under the modern layout and draw. One answer to "what does a touch
                // work", the original's, so that the routes' census can tell a claim that took a tap from a layout, a
                // draw or a policy that moved a number.
                switch (button.kind) {
                case Control::Left:
                case Control::Right:
                    button.hit = Controls::ClassicWalkFrame(m_hudRules, button.kind, view, m_levelAgeMs).hit;
                    break;
                case Control::Reset:
                case Control::Menu:
                case Control::Clear: {
                    const bool present = button.kind == Control::Clear
                                             ? (playing ? m_level.portals.budget > 0 && !m_level.portals.placed.empty()
                                                        : m_clearShownAtEnd)
                                             : true;
                    button.hit = Controls::ClassicCornerFrame(std::get<0>(corner(button.kind, present))).hit;
                    break;
                }
                }
            }
        }
        return;
    }

    for (ControlButton& button : m_controls) {
        switch (button.kind) {
        case Control::Left:
        case Control::Right: {
            const Controls::Frame frame = Controls::ClassicWalkFrame(m_hudRules, button.kind, view, m_levelAgeMs);
            button.rect = frame.rect;
            button.hit = frame.hit;
            button.shown = pads;
            button.alpha = padAlpha;
            break;
        }
        case Control::Reset:
        case Control::Menu:
            std::tie(button.rect, button.shown, button.alpha) = corner(button.kind, true);
            button.hit = Controls::ClassicCornerFrame(button.rect).hit;
            break;
        case Control::Clear:
            // PortalManager::update: none in a level that grants no portal, and
            // otherwise exactly while a placed one is alive - and at a death,
            // dismissed with the other two if it was up (INFERRED: the spec's
            // recordings had no portal placed).
            std::tie(button.rect, button.shown, button.alpha) = corner(
                Control::Clear,
                playing ? m_level.portals.budget > 0 && !m_level.portals.placed.empty() : m_clearShownAtEnd);
            button.hit = Controls::ClassicCornerFrame(button.rect).hit;
            break;
        }
    }
}

void MagicPortalsLayer::logModernControls(const entt::registry& registry) {
    m_controlsRectsLine.clear();
    m_controlsRectsPending = false;
    if (!playsModern() || !m_hudReady) return;
    // The window is known only once a frame has published it, which the game's first level (loaded at attach) precedes: it says
    // so on its first tick instead, so that the line never names a window it is not in.
    const auto* viewport = registry.ctx().find<Supersonic::ViewportInfo>();
    if (viewport == nullptr || !(viewport->Size().x > 0.0f) || !(viewport->Size().y > 0.0f)) {
        m_controlsRectsPending = true;
        return;
    }
    // The settled boxes and hit rectangles (Controls::Boxes / HitRects), not the frames of this tick, which have the
    // pair still sliding in: these are the numbers of the design's table 1.2, in the window's pixels. The layout is
    // in units of the view's height (256), so a pixel is the window's height over it, whatever the window's width.
    const glm::dvec2 view = ViewPx();
    const glm::dvec2 windowPx(viewport->Size());
    const double toPx = windowPx.y / view.y;
    const Controls::Insets& insets = m_paths.controls.testInsets;
    const std::array<Hud::Rect, 5> boxes = Controls::Boxes(m_controlsRules, m_controlsApplied, view, insets);
    const std::array<Hud::Rect, 5> hits = Controls::HitRects(m_controlsRules, boxes, view, insets);
    std::ostringstream line;
    line << kControlsModernRectsPrefix << std::fixed << std::setprecision(1);
    const auto rect = [&](const Hud::Rect& r) {
        const glm::dvec2 max = r.Max();
        line << "(" << r.min.x * toPx << "," << r.min.y * toPx << ")-(" << max.x * toPx << "," << max.y * toPx << ") px";
    };
    bool first = true;
    for (const Control control : Controls::kClaimOrder) {
        const std::size_t at = static_cast<std::size_t>(control);
        line << (first ? "" : ", ") << Controls::KeyOf(control) << " ";
        first = false;
        rect(boxes[at]);
        line << " hit ";
        rect(hits[at]);
    }
    line << ", in a " << std::setprecision(0) << windowPx.x << "x" << windowPx.y << " px window (view " << std::setprecision(2)
         << view.x << "x" << view.y << " units)";
    m_controlsRectsLine = line.str();
    SUPERSONIC_LOG_INFO("Magic Portals") << m_controlsRectsLine << std::endl;
}

bool MagicPortalsLayer::ControlRect(Control control, Hud::Rect& out) const {
    for (const ControlButton& button : m_controls) {
        if (button.kind != control || !button.shown) continue;
        out = button.rect;
        return true;
    }
    return false;
}

std::optional<MagicPortalsLayer::Control> MagicPortalsLayer::ControlAt(const glm::dvec2& viewPoint) const {
    Touch touch;
    touch.over = true;
    touch.atView = viewPoint;
    const ControlButton* under = controlUnderPointer(touch);
    return under != nullptr ? std::optional<Control>(under->kind) : std::nullopt;
}

void MagicPortalsLayer::PlaceCameraForTest(entt::registry& registry, const glm::dvec2& centrePx) {
    m_follow.centrePx = centrePx;
    placeCamera(registry);
}

bool MagicPortalsLayer::NoPortalSignRect(Hud::Rect& out) const {
    if (!m_sign.present) return false;
    out = m_sign.onView;
    return true;
}

void MagicPortalsLayer::tickNoPortalSign(double dtMs) {
    if (!m_sign.present || !m_hudReady) return;
    const glm::dvec2 view = ViewPx();
    // Ethanon's GetCameraPos is the screen's top-left corner in the world.
    const glm::dvec2 corner = m_follow.centrePx - view * 0.5;
    m_sign.follow.Tick(m_hudRules, Hud::SignTarget(m_hudRules, corner, m_sign.sizeUnits), dtMs);
    m_sign.onView.size = m_sign.sizeUnits;
    m_sign.onView.min = m_sign.follow.at - m_sign.sizeUnits * 0.5 - corner;
}

void MagicPortalsLayer::emitPopup(Supersonic::ScreenOverlay& overlay, const glm::dvec2& view,
                                  const Popup::Class& cls, const Popup::Open& open) const {
    using Supersonic::ScreenOverlay;
    // The pieces are sim/Popup's: the dim, the card, the close button, then the
    // class's own draw() - turned sprites turned in the view's square units, which
    // the overlay is told in fractions of this view's shape.
    const float aspect = static_cast<float>(view.x / view.y);
    for (const Popup::Piece& piece : Popup::Pieces(m_popupRules, cls, open, view)) {
        const glm::vec4 colour(glm::vec3(piece.rgb), static_cast<float>(piece.alphaByte) / 255.0f);
        if (colour.a <= 0.0f) continue;
        ScreenOverlay::Quad quad;
        quad.min = glm::vec2(piece.rect.min / view);
        quad.max = glm::vec2(piece.rect.Max() / view);
        quad.uvMin = glm::vec2(piece.uvMin);
        quad.uvMax = glm::vec2(piece.uvMax);
        quad.color = colour;
        quad.clampToEdge = true; // as makeSprite's pictures, and for its reason
        if (piece.element == Popup::Element::Dim) {
            // eth_framework_square.png is opaque white in every texel, so tinted
            // black it is exactly a plain black: no texture.
            quad.uvMin = glm::vec2(0.0f);
            quad.uvMax = glm::vec2(1.0f);
            overlay.Add(std::move(quad));
            continue;
        }
        const auto image = m_popupImages.find(piece.sprite);
        if (image == m_popupImages.end() || image->second.empty()) continue;
        quad.texture = image->second;
        if (piece.angleDeg != 0.0) {
            quad.basis = ScreenOverlay::Rotation(glm::radians(static_cast<float>(piece.angleDeg)), aspect);
        }
        overlay.Add(std::move(quad));
    }
}

bool MagicPortalsLayer::openAchievementsPopup() {
    if (!m_pause.open || m_achievementsPopup.open || !m_achievementRulesReady || !m_achievementsLoaded) return false;
    const Chapters::Level* level = Current();
    if (level == nullptr) return false;
    // getAchievementsFromLevel: none named, no popup - and in the original no
    // button either (CustomGameMenuLayer #229-280).
    const std::vector<const Achievements::Entry*> entries = Achievements::FromLevel(m_achievements, level->world,
                                                                                    level->index);
    if (entries.empty()) {
        SUPERSONIC_LOG_INFO("Magic Portals") << "achievements pressed on tick " << m_ticks
                                             << ": no achievement names this level, nothing opens" << std::endl;
        return false;
    }
    const Achievements::Rules::Popup& rules = m_achievementRules.popup;
    AchievementsPopupScreen popup;
    popup.open = true;
    // Popup::Popup with no picture of its own, the help popups' close button,
    // tapScreenToClose and the close's highlight (AchievementsPopup #5-60).
    popup.cls.name = "AchievementsPopup";
    popup.cls.card.sprite.clear();
    popup.cls.closeButton.sprite = rules.closeSprite;
    popup.cls.closeButton.atScreen = glm::dvec2(rules.closeAtX, rules.closeAtY);
    popup.cls.closeButton.origin = glm::dvec2(rules.closeOriginX, rules.closeOriginY);
    popup.cls.closeButton.sizeUnits = glm::dvec2(rules.closeUnitsX, rules.closeUnitsY);
    popup.cls.tapScreenToClose = true;
    popup.cls.closeButtonHighlight = true;
    popup.state = Popup::Start(popup.cls);
    popup.text = Achievements::PopupText(m_achievementRules, m_achievements, entries, m_achievementStore.Sorted());
    m_achievementsPopup = std::move(popup);
    loadUiFont(rules.font);
    SUPERSONIC_LOG_INFO("Magic Portals") << "AchievementsPopup opened over the pause on tick " << m_ticks << " with "
                                         << entries.size() << " achievement(s)" << std::endl;
    return true;
}

void MagicPortalsLayer::achievementsPopupTick(entt::registry& registry, double dtMs) {
    using Supersonic::Input;
    AchievementsPopupScreen& popup = m_achievementsPopup;
    Popup::Tick(popup.cls, popup.state, dtMs);
    // Popup::hasReceivedCloseCommand: a touch down anywhere, the close button, or
    // the back key; the touch reaches nothing under it.
    const Touch touch = touchThisTick(registry);
    const bool touched = TouchClosesPopup(m_popupRules, popup.cls, popup.state, ViewPx(), touch.pressed,
                                          touch.released, touch.over, touch.atView, popup.downOnClose);
    if ((touched || Input::TickWasPressed(kBack) || devPressDue(DevPress::Back)) && !Popup::Closing(popup.state)) {
        Popup::Close(popup.state);
        SUPERSONIC_LOG_INFO("Magic Portals") << "AchievementsPopup closed on tick " << m_ticks << std::endl;
    }
    // Popup::update #28-67: everything dismissed, the pause current again.
    if (Popup::Gone(m_popupRules, popup.state)) {
        m_achievementsPopup = AchievementsPopupScreen{};
        SUPERSONIC_LOG_INFO("Magic Portals") << "AchievementsPopup gone, the pause current on tick " << m_ticks
                                             << std::endl;
    }
}

void MagicPortalsLayer::emitAchievementsPopup(Supersonic::ScreenOverlay& overlay, const glm::dvec2& view) const {
    using Supersonic::ScreenOverlay;
    const AchievementsPopupScreen& popup = m_achievementsPopup;
    // UILayer::draw first: the dim and the close button, as a help popup's.
    emitPopup(overlay, view, popup.cls, popup.state);
    // AchievementsPopup::draw #28-58: the frame bounces with the close button's
    // alpha, eased in by elastic and out by smoothBeginning; the text does not.
    const Popup::CloseButtonState button = Popup::CloseButtonAt(m_popupRules, popup.cls, popup.state, view);
    const double bias = static_cast<double>(button.alphaByte) / 255.0;
    if (button.alphaByte <= 0) return;
    const double scale = Popup::Closing(popup.state) ? Ease::SmoothBeginning(bias)
                                                     : Notificator::Elastic(m_notificatorRules, bias);
    const Achievements::Rules::Popup& rules = m_achievementRules.popup;
    const Supersonic::BitmapFont* font = uiFont(rules.font);
    if (font == nullptr || !font->IsLoaded()) return;
    const auto tiles = m_cardImages.find(rules.tiles);
    // The button's colour: its alpha, and the press tint Button::update writes into it.
    const glm::vec4 colour(glm::vec3(static_cast<float>(button.tint)), static_cast<float>(bias));
    const Notificator::Box box =
        Notificator::LayOutBox(glm::dvec2(rules.tileUnitsX, rules.tileUnitsY), rules.tileColumns, rules.tileRows, scale,
                               rules.unitsPerFontPx, view * 0.5, glm::dvec2(0.5), glm::dvec2(font->Measure(popup.text)));
    const auto add = [&overlay, &view, &colour](const Hud::Rect& rect, const std::string& image,
                                                const glm::dvec2& uvMin, const glm::dvec2& uvMax) {
        ScreenOverlay::Quad quad;
        quad.min = glm::vec2(rect.min / view);
        quad.max = glm::vec2(rect.Max() / view);
        quad.uvMin = glm::vec2(uvMin);
        quad.uvMax = glm::vec2(uvMax);
        quad.color = colour;
        quad.texture = image;
        quad.clampToEdge = true;
        overlay.Add(std::move(quad));
    };
    if (tiles != m_cardImages.end() && !tiles->second.empty()) {
        for (int i = 0; i < 9; ++i) add(box.pieces[i], tiles->second, box.uvMin[i], box.uvMax[i]);
    }
    // TextBoxFrame::draw #93-114: DrawText from the box's top-left.
    const auto& pages = font->Pages();
    for (const Hud::Glyph& glyph : Hud::LayOutTextFrom(*font, popup.text, box.textTopLeft, rules.unitsPerFontPx)) {
        if (glyph.page < 0 || static_cast<std::size_t>(glyph.page) >= pages.size()) continue;
        add(glyph.rect, pages[static_cast<std::size_t>(glyph.page)], glyph.uvOffset, glyph.uvOffset + glyph.uvScale);
    }
}

void MagicPortalsLayer::EmitNotifications(entt::registry& registry) const {
    using Supersonic::ScreenOverlay;
    auto* const* slot = registry.ctx().find<ScreenOverlay*>();
    if (slot == nullptr || *slot == nullptr) return;
    ScreenOverlay& overlay = **slot;
    if (!m_notificatorReady || !m_hudReady) return;
    const Notificator::Card* card = m_notificator.Front();
    if (card == nullptr) return;
    // Over whatever is on the screen: the menu's box on a menu state, the level's
    // view over a level.
    const glm::dvec2 view = menuStateUp() ? MenuBoxPx() : ViewPx();
    if (view.x <= 0.0 || view.y <= 0.0) return;
    const Supersonic::BitmapFont* font = uiFont(m_notificatorRules.font);
    const glm::dvec2 textPx = font != nullptr && font->IsLoaded() ? glm::dvec2(font->Measure(card->message))
                                                                   : glm::dvec2(0.0);
    const Notificator::Layout layout =
        Notificator::LayOut(m_notificatorRules, *card, m_notificator.FrontAgeMs(), view, textPx);
    // computeEffectScale's colour: white, at the fade's alpha.
    const glm::vec4 colour(1.0f, 1.0f, 1.0f, static_cast<float>(layout.alpha));
    if (colour.a <= 0.0f) return;
    const auto add = [&overlay, &view, &colour](const Hud::Rect& rect, const std::string& image,
                                                const glm::dvec2& uvMin, const glm::dvec2& uvMax) {
        ScreenOverlay::Quad quad;
        quad.min = glm::vec2(rect.min / view);
        quad.max = glm::vec2(rect.Max() / view);
        quad.uvMin = glm::vec2(uvMin);
        quad.uvMax = glm::vec2(uvMax);
        quad.color = colour;
        quad.texture = image;
        quad.clampToEdge = true;
        overlay.Add(std::move(quad));
    };
    // Notificator::draw: the three pieces, the text, then the icon.
    if (const auto background = m_cardImages.find(card->background);
        background != m_cardImages.end() && !background->second.empty()) {
        for (int c = 0; c < 3; ++c) add(layout.pieces[c], background->second, layout.uvMin[c], layout.uvMax[c]);
    }
    if (font != nullptr && font->IsLoaded()) {
        const auto& pages = font->Pages();
        for (const Hud::Glyph& glyph :
             Hud::LayOutText(*font, card->message, layout.textCentre, layout.unitsPerFontPx)) {
            if (glyph.page < 0 || static_cast<std::size_t>(glyph.page) >= pages.size()) continue;
            add(glyph.rect, pages[static_cast<std::size_t>(glyph.page)], glyph.uvOffset,
                glyph.uvOffset + glyph.uvScale);
        }
    }
    if (layout.hasIcon) {
        if (const auto icon = m_cardImages.find(card->icon); icon != m_cardImages.end() && !icon->second.empty()) {
            add(layout.icon, icon->second, glm::dvec2(0.0), glm::dvec2(1.0));
        }
    }
}

void MagicPortalsLayer::EmitHud(entt::registry& registry) const {
    using Supersonic::ScreenOverlay;
    auto* const* slot = registry.ctx().find<ScreenOverlay*>();
    if (slot == nullptr || *slot == nullptr) return;
    ScreenOverlay& overlay = **slot;
    // Nothing over a menu screen, which is drawn in the level's space. The
    // finished and lost screens are over a level, and drawn here.
    const bool overLevel = m_screen == Screen::Finished || m_screen == Screen::Dead;
    if (!m_hudReady || !m_loaded || (m_screen != Screen::None && !overLevel)) return;
    const glm::dvec2 view = ViewPx();
    if (view.x <= 0.0 || view.y <= 0.0) return;

    // A rectangle on the view, in design units, as fractions of the image the
    // view fills; a picture with nothing to show is not sent at all.
    const auto add = [&overlay, &view](const Hud::Rect& rect, const std::string& image, const glm::vec4& colour,
                                       const glm::dvec2& uvMin = glm::dvec2(0.0),
                                       const glm::dvec2& uvMax = glm::dvec2(1.0)) {
        if (colour.a <= 0.0f) return;
        ScreenOverlay::Quad quad;
        quad.min = glm::vec2(rect.min / view);
        quad.max = glm::vec2(rect.Max() / view);
        quad.uvMin = glm::vec2(uvMin);
        quad.uvMax = glm::vec2(uvMax);
        quad.color = colour;
        quad.texture = image;
        quad.clampToEdge = true; // as makeSprite's pictures, and for its reason
        overlay.Add(std::move(quad));
    };
    const auto white = [](double alpha) { return glm::vec4(1.0f, 1.0f, 1.0f, static_cast<float>(alpha)); };
    const auto find = [this](Control kind) -> const ControlButton* {
        for (const ControlButton& button : m_controls) {
            if (button.kind == kind && button.shown) return &button;
        }
        return nullptr;
    };
    // A control's quad. Classic: the picture in its rectangle, white at its alpha. Modern (planning step 182): the
    // picture in the rectangle derived from the box, or, where there is none to draw, a flat quad on the box's face
    // in the plate's own light rim colour, which reads at the HUD's alpha on a bright level and a dark one.
    const bool modern = playsModern();
    const auto addControl = [&](const ControlButton& button) {
        if (!modern) {
            add(button.rect, button.image, white(button.alpha));
            return;
        }
        if (button.image.empty()) {
            const PadArt::Rgb rim = PadArt::kPlateRim;
            add(button.picture, button.image,
                glm::vec4(static_cast<float>(rim.r) / 255.0f, static_cast<float>(rim.g) / 255.0f,
                          static_cast<float>(rim.b) / 255.0f, static_cast<float>(button.alpha)));
            return;
        }
        add(button.picture, button.image, white(button.alpha));
    };
    // Text on the view, centred on `at` or from it as its top-left, a quad a
    // letter over the font's own pages.
    const auto write = [&](const std::string& fontName, const std::string& words, const glm::dvec2& at,
                           double unitsPerFontPx, bool centred, double alpha) {
        const Supersonic::BitmapFont* font = uiFont(fontName);
        if (font == nullptr) return;
        const auto& pages = font->Pages();
        const std::vector<Hud::Glyph> glyphs = centred ? Hud::LayOutText(*font, words, at, unitsPerFontPx)
                                                       : Hud::LayOutTextFrom(*font, words, at, unitsPerFontPx);
        for (const Hud::Glyph& glyph : glyphs) {
            if (glyph.page < 0 || static_cast<std::size_t>(glyph.page) >= pages.size()) continue;
            add(glyph.rect, pages[static_cast<std::size_t>(glyph.page)], white(alpha), glyph.uvOffset,
                glyph.uvOffset + glyph.uvScale);
        }
    };

    // IN THE ORIGINAL'S ORDER OF DRAWING, which is the whole of the layering:
    // there is no depth here, and a quad added later is drawn over one before.

    // 1. The no-portal sign: a level entity, so drawn with the scene, under all
    //    of the UI and both blacks. At full opacity - its see-through look is
    //    the PNG's own alpha.
    if (m_sign.present && !m_sign.image.empty()) {
        // An entity in the original's world, so a frame's shake moves it with
        // the world: up the view on a forth, as the camera goes down.
        Hud::Rect sign = m_sign.onView;
        sign.min.y -= m_quakeDrawn.units;
        add(sign, m_sign.image, white(1.0));
    }

    // 2. The tutorial's rings, which ScreenPad draws from its UPDATE and so
    //    ahead of every draw; centred on the pads' corners, sliding with them.
    const bool tutorial = tutorialPads();
    const bool padsShown = find(Control::Left) != nullptr || find(Control::Right) != nullptr;
    // The pulse stops when a level ends, and the ring with it.
    if (const Hud::Ring ring = Hud::RingAt(m_hudRules, m_levelAgeMs, tutorial);
        ring.shown && padsShown && !m_finishing && !m_dying && !m_ringImage.empty()) {
        for (const Control walk : {Control::Left, Control::Right}) {
            Hud::Rect rect;
            if (modern) {
                // On the arrow's box centre, as many boxes across as controls.json says, sliding in with it.
                const ControlButton* pad = nullptr;
                for (const ControlButton& button : m_controls) {
                    if (button.kind == walk) pad = &button;
                }
                if (pad == nullptr) continue;
                const Controls::RingPlace place =
                    Controls::RingFor(m_controlsRules, m_hudRules, ring, Controls::Frame{pad->rect, pad->hit});
                rect.size = glm::dvec2(place.sizeUnits);
                rect.min = place.centre - rect.size * 0.5;
                add(rect, m_ringImage, white(place.alpha));
                continue;
            }
            rect.size = glm::dvec2(ring.sizeUnits);
            rect.min = Controls::ClassicRingCentre(m_hudRules, walk, view, m_levelAgeMs) - rect.size * 0.5;
            add(rect, m_ringImage, white(ring.alpha));
        }
    }

    // 3. The UI layer, in the order its sprites were added: restart and pause
    //    (GameLayer), the plaque and then its medal over it (Game::preLoop), and
    //    the clear-portals button, which PortalManager adds while playing.
    // Each at the alpha layOutControls left it: its 120, or what a death's
    // dismiss has left of it.
    for (const Control kind : {Control::Reset, Control::Menu}) {
        if (const ControlButton* button = find(kind)) addControl(*button);
    }
    if (!m_plaqueImage.empty() && !m_medalImage.empty()) {
        // As GameLayer's last update left it (tickPlaqueDismissal).
        const double plaque = m_plaqueAlpha;
        const auto centred = [](const glm::dvec2& centre, const glm::dvec2& size) {
            return Hud::Rect{centre - size * 0.5, size};
        };
        add(centred(m_hudRules.plaque.centreUnits, m_hudRules.plaque.sizeUnits), m_plaqueImage, white(plaque));
        add(centred(m_hudRules.plaque.medalCentreUnits, m_hudRules.plaque.medalSizeUnits), m_medalImage,
            white(plaque));
    }
    if (const ControlButton* clear = find(Control::Clear)) addControl(*clear);

    // 4. The pause, when one is up: CustomGameMenuLayer, the CURRENT UI layer,
    //    which UILayerManager::draw draws after GameLayer - so over restart,
    //    pause, the plaque and clear-portals, each frozen at what it was - and,
    //    being the UI layer manager's, before either black (spec 0.5, 2.2). The
    //    pictures and alphas are sim/Pause's; the two texts come last, as
    //    GameMenuLayer::draw and CustomGameMenuLayer::draw draw them after
    //    UILayer::draw.
    if (m_pause.open) {
        for (const Pause::Sprite& sprite :
             Pause::Sprites(m_pauseRules, m_pause.level, PauseSwitches(), view, m_pause.clockMs)) {
            const float alpha = static_cast<float>(sprite.alphaByte) / 255.0f;
            if (sprite.element == Pause::Element::Dim) {
                // square.png is opaque white in every texel, so tinted black it is
                // exactly a plain black.
                add(sprite.rect, std::string(), glm::vec4(0.0f, 0.0f, 0.0f, alpha));
                continue;
            }
            const auto image = m_pauseImages.find(sprite.file);
            if (image == m_pauseImages.end() || image->second.empty()) continue;
            add(sprite.rect, image->second, glm::vec4(1.0f, 1.0f, 1.0f, alpha));
        }
        const double text = static_cast<double>(Pause::TextAlphaByte(m_pauseRules, m_pause.clockMs)) / 255.0;
        write(m_pauseRules.title.font, Pause::TitleText(m_pauseRules, m_pause.level),
              Pause::TitleCentre(m_pauseRules, view), m_pauseRules.title.unitsPerFontPx, true, text);
        write(m_pauseRules.goldenNumber.font, Pause::GoldenText(m_pause.level),
              Pause::GoldenCentre(m_pauseRules, view), m_pauseRules.goldenNumber.unitsPerFontPx, true, text);
    }

    // 4a. A popup, when one is up: the CURRENT UI layer, drawn where the pause is,
    //     over restart, pause and the plaque (frozen, or not yet come in at a level's
    //     start) and under both blacks and the caption (spec 5.2's order). The
    //     pieces are sim/Popup's: the dim, the card, the close button, then the
    //     class's own draw() - turned sprites turned in the view's square units,
    //     which the overlay is told in fractions of this view's shape.
    if (m_popup.open && m_popup.cls != nullptr) emitPopup(overlay, view, *m_popup.cls, m_popup.state);
    // 4a'. The pause's AchievementsPopup, the current layer over it.
    if (m_pause.open && m_achievementsPopup.open) emitAchievementsPopup(overlay, view);

    // 4b. The finished or the lost screen, when one is up: the CURRENT UI layer
    //     over the running level and whatever the HUD has left, and under the
    //     walk pads, which fade on through its veil (spec 3.4's order, 3.2). The
    //     pieces are sim/LevelEnd's, in LevelFinishedLayer's and LevelLostLayer's
    //     own order. The veil is fade_edge.png stretched a screen and a half (or
    //     0.9 of one) wide: drawn as the three strips of a CLAMPED texture, since
    //     the overlay's sampler repeats, and a repeat would blend the opaque first
    //     texel with the clear last one across the screen's left edge.
    if (overLevel) {
        const auto image = [this](const std::string& file) -> const std::string& {
            static const std::string none;
            const auto found = m_endImages.find(file);
            return found != m_endImages.end() ? found->second : none;
        };
        for (const LevelEnd::Piece& piece : endPieces(view, m_end.clockMs)) {
            const double alpha = static_cast<double>(piece.alphaByte) / 255.0;
            switch (piece.element) {
            case LevelEnd::Element::Counter:
            case LevelEnd::Element::GoldenNumber:
            case LevelEnd::Element::CrystalCount:
                write(piece.file, piece.words, piece.at, piece.unitsPerFontPx, piece.centred, alpha);
                break;
            case LevelEnd::Element::Veil: {
                const std::string& veil = image(piece.file);
                const auto texels = m_endTexels.find(piece.file);
                if (veil.empty() || texels == m_endTexels.end()) break;
                for (const LevelEnd::Strip& strip : LevelEnd::ClampedStrips(piece.rect, texels->second)) {
                    add(strip.rect, veil, white(alpha), strip.uvMin, strip.uvMax);
                }
                break;
            }
            case LevelEnd::Element::Title:
            case LevelEnd::Element::PortalsPlaque:
            case LevelEnd::Element::GoldenPlaque:
            case LevelEnd::Element::Button:
            case LevelEnd::Element::Medal:
            case LevelEnd::Element::Crystal:
                if (const std::string& file = image(piece.file); !file.empty()) add(piece.rect, file, white(alpha));
                break;
            }
        }
    }

    // 5. The two blacks, with the pads between them: BaseState's FadeInController,
    //    the character's pads, then Game's own FadeInController. In display
    //    values, as the original drew them, so each is its decoded alpha as it
    //    stands - two stacked blacks leave (t / 700)^2 of the picture, t being
    //    their own age: the level's less overlay.startAfterMs (step 40). On the
    //    WALL frame clock (LevelFrameWallMs: the engine's 16.667 ms a tick, a pause
    //    and a popup counted), because FadeInController reads sys:GetTime(), which a
    //    pause does not stop and the script's floor of each frame does not touch
    //    (planning step 147, fix 1); the pads run on game time, which a pause does stop.
    const Hud::Rect whole{glm::dvec2(0.0), view};
    const int over = m_hudRules.overlay.layersOverPads;
    const int under = m_hudRules.overlay.layers - over;
    const auto black = [](double alpha) { return glm::vec4(0.0f, 0.0f, 0.0f, static_cast<float>(alpha)); };
    add(whole, std::string(), black(Hud::OverlayLayersAlpha(m_hudRules, LevelFrameWallMs(), under)));
    for (const Control kind : {Control::Left, Control::Right}) {
        if (const ControlButton* button = find(kind)) addControl(*button);
    }
    add(whole, std::string(), black(Hud::OverlayLayersAlpha(m_hudRules, LevelFrameWallMs(), over)));

    // 6. "Part N", over everything, laid out for this view's width. On the frame
    //    clock too, but the SCRIPT's: ETHTextDrawer::Draw adds the unsigned long
    //    ETHEngine::DrawTopLayer hands it (ETHEngine.cpp:205, GetLastFrameElapsedTime():
    //    the floored whole milliseconds), so a caption still fading when a pause opens
    //    goes on fading above it, 16 ms a tick. The popups' captures show that; no
    //    capture of a pause does (spec 2.2, U3).
    const double caption = Hud::CaptionAlpha(m_hudRules, LevelFrameMs());
    if (caption > 0.0 && !m_captionText.empty()) {
        const auto& pages = m_captionFont.Pages();
        for (const Hud::Glyph& glyph : Hud::LayOutCaption(m_hudRules, m_captionFont, m_captionText, view)) {
            // Matura84 keeps "Part 1"'s P and r on page 0 and its a, t and 1 on
            // page 2, so each letter names its own page.
            if (glyph.page < 0 || static_cast<std::size_t>(glyph.page) >= pages.size()) continue;
            add(glyph.rect, pages[static_cast<std::size_t>(glyph.page)], white(caption), glyph.uvOffset,
                glyph.uvOffset + glyph.uvScale);
        }
    }

    // 7. 1-21's "move the camera around" (camera.json `hint`): the DrawFadingText the
    //    hand's first call makes, which the engine's drawable manager draws after the
    //    caption it was handed after - over everything, on the screen and not the
    //    level, from the view's bottom-left, and on the caption's frame clock, as
    //    both are ETHTextDrawers timed from the level's first frame.
    if (const Supersonic::BitmapFont* font = m_hintHandPlaced ? uiFont(m_hintRules.font) : nullptr;
        font != nullptr && font->IsLoaded()) {
        const double hint = Hint::TextAlpha(m_hintRules, LevelFrameMs());
        if (hint > 0.0) {
            const auto& pages = font->Pages();
            for (const Hud::Glyph& glyph : Hud::LayOutTextFrom(*font, m_hintRules.text, Hint::TextTopLeft(m_hintRules, view),
                                                               m_hintRules.unitsPerFontPx)) {
                if (glyph.page < 0 || static_cast<std::size_t>(glyph.page) >= pages.size()) continue;
                add(glyph.rect, pages[static_cast<std::size_t>(glyph.page)], white(hint), glyph.uvOffset,
                    glyph.uvOffset + glyph.uvScale);
            }
        }
    }
}

const MagicPortalsLayer::ControlButton* MagicPortalsLayer::controlUnderPointer(const Touch& touch) const {
    // Off the game - a panel, another window, no viewport - there is no control
    // under it: touchThisTick leaves `over` false on exactly those.
    if (!touch.over) return nullptr;
    // The pointer as a place on the VIEW: the viewport is the view, stretched
    // to it, so no camera is needed to say which control a tap is on. Each control
    // answers with the area it was laid out with, at the age of the layout this tick
    // began with, and the first of them in build order - the claim order - wins.
    const glm::dvec2 at = touch.atView;
    for (const ControlButton& button : m_controls) {
        if (!button.shown) continue;
        if (button.hit.Contains(at)) return &button;
    }
    return nullptr;
}

bool MagicPortalsLayer::onWalkControl(const glm::dvec2& viewPoint) const {
    if (playsModern() && m_hudReady && !claimsClassic()) {
        // THE MODERN PAIR'S HIT RECTANGLES at this age, asked as classic's discs are: whether or not the pair is shown
        // (a weightless level has none and a touch resting where it would be still counts - design D12), and whether or
        // not any control was built. From the data and the view alone, so the camera, the lift gate and the hand gate
        // ask the very rectangles the claim does.
        const Controls::Layout layout = Controls::LayOut(Controls::Style::Modern, m_controlsRules, m_hudRules, m_controlsApplied,
                                                         ViewPx(), m_paths.controls.testInsets, m_levelAgeMs);
        return layout.At(Control::Left).hit.Contains(viewPoint) || layout.At(Control::Right).hit.Contains(viewPoint);
    }
    return Controls::ClassicOnWalkControl(m_hudRules, ViewPx(), m_levelAgeMs, viewPoint);
}

float MagicPortalsLayer::keyDirection() const {
    using Supersonic::Input;
    float direction = 0.0f;
    if (Input::IsDown(kLeft) || Input::IsDown(kLeftAlt)) direction -= 1.0f;
    if (Input::IsDown(kRight) || Input::IsDown(kRightAlt)) direction += 1.0f;
    // DEV ONLY: a held walk a capture scheduled for this tick.
    for (const DevHold& hold : m_devHolds) {
        if (m_ticks >= hold.from && m_ticks <= hold.to) direction += hold.direction;
    }
    return std::clamp(direction, -1.0f, 1.0f);
}

float MagicPortalsLayer::readInput(entt::registry& registry) {
    using Supersonic::Input;
    float direction = keyDirection();

    // The on-screen controls, laid out and asked about before anything else
    // reads the pointer.
    layOutControls();

    // THE BACK KEY PAUSES, where the pause control could be pressed:
    // GameState::handleBackButton (bytes 98331..98935) shows the pause while
    // GameLayer is current. It used to leave the level for its grid, which is
    // now the pause's own first button.
    if (pauseAllowed() && (Input::TickWasPressed(kBack) || devPressDue(DevPress::Pause))) {
        openPause(registry);
        return direction;
    }
    // THE TICK'S TOUCH, read ONCE and handed to everything below: the controls, a
    // help block and the shot. It is the ONE pointer path - a real press, or a
    // tap a capture scheduled, which owns the pointer on its ticks wherever the
    // real cursor rests - so a scheduled tap reaches each of the three exactly
    // as a real press at the same point does. Reading it twice would press and
    // release a scheduled tap inside one tick.
    Touch touch = touchThisTick(registry);
    // THE CAMERA'S READING OF IT, first and whatever takes it below: the original's
    // CameraController reads the engine's input raw (camera.json `drag`), at the
    // place the touch is this tick, and asks the walk pads of that place. And
    // whether the pads walk the player - ScreenPad::isMoving, which a walk key sets
    // as the pad does (arrowKeysMove) and which no_gravity levels never set, as the
    // original skips the pad's update there. The camera's tick takes both.
    const bool onPad = touch.over && onWalkControl(touch.atView);
    m_cameraFeel = Camera::Track(m_cameraTouch, touch.pressed, touch.held, touch.over, touch.atView, onPad);
    m_cameraPadMoving = !m_level.noGravity && direction != 0.0f;
    // AND THE HANDLES' (handle.json, sim/Handle.hpp), whatever takes the touch below:
    // ETHCallback_arrows reads ETHInput raw too, and runs before the camera in the
    // frame (arrows.ent is always active). Its GetTouchPos + GetCameraPos is the
    // touch through the camera the last tick left; getRelativeHitPos is where it
    // went down through the camera the tick after that left, as the portal manager
    // sets it after the camera on the HIT frame. A handle it moves locks the
    // camera's finger for this tick (shouldLockCamera).
    {
        const glm::dvec2 corner = m_follow.centrePx - ViewPx() * 0.5;
        Handle::Touch handleTouch;
        handleTouch.hit = touch.over && touch.held && touch.pressed;
        handleTouch.down = m_cameraFeel.down;
        handleTouch.atPx = corner + touch.atView;
        handleTouch.moveUnits = m_cameraFeel.moveUnits;
        if (!m_cameraTouch.down || handleTouch.hit) m_handleHitPx.reset();
        if (handleTouch.down && !m_handleHitPx) m_handleHitPx = corner + m_cameraTouch.hitView;
        handleTouch.hitPx = m_handleHitPx.value_or(glm::dvec2(0.0));
        m_level.handles.Drag(handleTouch, m_tickMs);
        m_cameraFeel.locked = m_level.handles.AnyMoving();
    }
    // AND THE LIFTS' (movers.json `lift`, sim/Mover.hpp), whatever takes the touch
    // below: ETHCallback_lift asks getTouchStateInRect, which reads the engine's own
    // touch state and not the manager's enabled one. On the frame a touch goes down
    // inside a lift's picture, off the walk pads (ScreenPad::isValidPoint), it sends
    // that lift and disables every touch - so the shot that touch would fire on its
    // release is never fired (the manager's getTouchState reads 0 until it is up).
    // Through the camera the last tick left, as the shot's point; the lift moves on
    // this tick's step, as the callback moves it on the frame it is sent.
    bool liftTook = false;
    if (touch.pressed && touch.over && !onPad) {
        const glm::dvec2 corner = m_follow.centrePx - ViewPx() * 0.5;
        liftTook = m_level.movers.Press(corner + touch.atView) > 0;
    }
    // AND THE TOUCH A SHOT IS FIRED BY, followed before anything below reads it,
    // as PortalManager::update runs TouchInputManager::update before either tap
    // path (placement.json `release`). On the tick a touch comes up, every reader
    // takes the last point it was HELD at - getReleasePos, which the original's
    // controls, help blocks and shot all read on that frame - not where it came up.
    Portals::TouchRelease release =
        Portals::TrackTouch(m_shotTouch, touch.pressed, touch.held, touch.released, touch.over, touch.atView,
                            touch.atPx);
    // And the tutorial hands' own record of it (m_handTouch), which nothing below
    // spends.
    const Portals::TouchRelease handRelease =
        Portals::TrackTouch(m_handTouch, touch.pressed, touch.held, touch.released, touch.over, touch.atView,
                            touch.atPx);
    if (release.released) {
        touch.atView = release.atView;
        touch.atPx = release.atPx;
    }
    // A TUTORIAL HAND THE RELEASE TAKES AWAY (effects.json smoke_vanish_hand_tap):
    // ETHCallback_hand_tap asks every touch hasPortalRequestInput(t, true), its
    // disabled status ignored - so a touch a lift took on its way down (below, on
    // that tick) or a help block took still counts - whatever the shot does with
    // the same release.
    if (Portals::RequestsPortal(m_level.portals.rules, handRelease)) tapHandsAway(registry, handRelease);
    // A touch a lift took is followed no further: it fires no shot when it comes
    // up, on a later tick or on this one (a press and a release inside one tick).
    if (liftTook) {
        m_shotTouch = Portals::TouchTrack{};
        release = Portals::TouchRelease{};
    }
    // MODERN CORNER BUTTONS ACT ON THE RELEASE inside their hit rectangle after a press inside it (design D11: the
    // original's own Button rule, which classic does not follow: it acts on the press, a recorded deviation). `began` is the
    // control THIS touch's press began on, from an earlier tick; a press anywhere starts a record, and a touch that is up
    // (or off the game) ends it. The data can ask for the press instead (controls.json corner.act_on); the test variant
    // with the classic claims acts on the press as classic does.
    const bool actsOnRelease = playsModern() && !claimsClassic() && m_controlsRules.cornerActOn == Controls::ActOn::Release;
    const std::optional<Control> began = touch.pressed ? std::nullopt : m_pressedControl;
    if (touch.pressed || !touch.held || !touch.over) m_pressedControl.reset();
    // On the tick a touch comes up every reader takes the last point it was HELD at (above). A press that went down on a
    // corner button spent the shot's record, so the release tick's own point is the raw one: read where it was last held.
    Touch claimTouch = touch;
    if (actsOnRelease && handRelease.released) {
        claimTouch.atView = handRelease.atView;
        claimTouch.atPx = handRelease.atPx;
    }
    const ControlButton* under = controlUnderPointer(claimTouch);
    const bool held = touch.held;
    const bool pressed = touch.pressed;

    // A HELP BLOCK takes a touch that goes down on it - no portal, no control -
    // and opens its popup when that touch comes up; the popup stops the level on
    // this tick, as the pause control does. The controls are asked first: they
    // are GameLayer's buttons, which the block's disabled touches do not reach.
    if (under == nullptr && helpBlockInput(registry, touch)) return direction;

    if (under != nullptr) {
        // Read before the switch: a restart unloads the controls `under` points into.
        const Control kind = under->kind;
        // Whether a corner control is worked on this tick: classic, on the press; modern, on the release over the control
        // the press began on (a press and a release inside one tick are both of them).
        const bool isCorner = kind != Control::Left && kind != Control::Right;
        bool act = pressed;
        if (actsOnRelease) {
            if (isCorner && pressed && held) m_pressedControl = kind;
            act = isCorner && handRelease.released && (pressed || began == kind);
        }
        switch (kind) {
        // HELD, not tapped: a walk button is leaned on, and IsDown is what a
        // replayed tick feeds (Input.hpp lists it among the queries a replay
        // diverts), so the suites can drive these exactly as they drive a key.
        case Control::Left:
            if (held) direction -= 1.0f;
            if (held && !m_level.noGravity) m_cameraPadMoving = true;
            break;
        case Control::Right:
            if (held) direction += 1.0f;
            if (held && !m_level.noGravity) m_cameraPadMoving = true;
            break;
        // And these are EDGES: held down, a reset would restart the level every
        // tick for as long as the button was pressed.
        // Its sound is getRestartLevelButtonSound's teleport (GameLayer::GameLayer
        // #207-216, Game::Game #74; planning step 142 - silent before).
        case Control::Reset:
            if (act && m_current >= 0) {
                latch("level_button");
                loadLevel(registry, m_current);
            }
            break;
        // GameLayer's menu button calls GameState::showMenuPopup (bytes
        // 98935..99036): the pause, over this level, rather than out of it.
        case Control::Menu:
            if (act) openPause(registry);
            break;
        // PortalManager::killAll(true): the placed portals go, those that never
        // carried anything are given back, and it plays playPortalKilledSound at
        // its entry (#5). The button is not there with none placed (update
        // #62-72), and a press that finds none - the port's own case - is silent.
        case Control::Clear:
            if (act && m_level.portals.KillAll(true) > 0) latch("portal_spent");
            break;
        }
        // A TAP THAT WORKED A CONTROL IS SPENT. Without this, leaning on the
        // walk arrow also fires a portal into the floor beneath it every time
        // the cooldown allows - the control would work and quietly cost the
        // player their portal budget.
        //
        // And a corner control spends the whole touch on the way down: these act on
        // the press here, where the original's Button acts on the release, which is
        // then on the button and fires nothing (PortalManager::isValidPoint). Clear
        // hides itself as it clears, so without this its touch would come up on
        // nothing and fire into the corner.
        if (pressed && kind != Control::Left && kind != Control::Right) m_shotTouch = Portals::TouchTrack{};
        return direction;
    }

    // A pointer over a panel or another window is not the game's - and a tap a
    // capture scheduled owns the pointer whatever the real cursor is doing, which
    // is what lets a --tap open a portal (the same seam --hold walks by).
    //
    // THE SHOT LEAVES ON THE RELEASE (hasPortalRequestInput): on the tick a touch
    // that went down in the level comes up, if it strayed no more than scale(18)
    // from where it went down, toward the last point it was held at, through the
    // camera this tick reads. Its tick is the release's, so the first-portal gate
    // and the 400 ms between taps are judged there; a touch that goes down and
    // stays down fires nothing, and neither does one dragged further than that.
    if (touch.over && Portals::RequestsPortal(m_level.portals.rules, release)) {
        glm::dvec2 atPx(0.0);
        if (ViewportToLevelPx(registry, glm::vec2(touch.atPx), atPx)) {
            // With a fire diamond carried the method casts no ray (#21-64) and the mark follows the leftover low
            // byte of absTouchPos.x in the original's screen pixels (D3 section 3), which only the layer knows.
            std::optional<bool> fireDiamondWillDie;
            if (m_level.diamonds.carrierHasFire) {
                fireDiamondWillDie = Portals::State::FireDiamondWillDie(atPx.x * m_artRules.scaleFactor);
            }
            if (m_level.portals.Shoot(registry, atPx, ScriptClock::SimRatio(m_step), fireDiamondWillDie) &&
                m_level.player != entt::null && registry.valid(m_level.player)) {
                // managePortalInsertion #101-124, on a tap it takes with no projectile alive: the character
                // is turned toward the tap - setDirection(1), left, where its x is above the tap's, else
                // setDirection(2), right (F10 3.2: faces the tap and glides away from it). A walking update
                // sets its direction again from the pad, which the picture's sync does after this.
                const double playerX = Units::ToPixels(registry.get<Supersonic::TransformComponent>(m_level.player).position).x;
                m_facingRight = !(playerX > atPx.x);
            }
            // What the tap leaves behind, read off the record Shoot has just
            // written - a picture, whatever the shot does next.
            queueTapEffects();
        }
    }
    return direction;
}

// ---- what the game thinks it is drawing --------------------------------------
//
// Written down as it CHANGES, and in full when G asks.
//
// The point is one distinction and nothing else. When a sprite disappears from
// the screen there are four explanations and a player cannot tell them apart:
//
//   1. this layer took the quad away        - a "gone" line appears below
//   2. the renderer culled it               - drawn falls, culled rises
//   3. a pass refused the draw              - dropped rises
//   4. it was drawn and produced no pixels  - nothing changes anywhere
//
// A screenshot cannot separate those and neither can any test in this
// repository: every headless run agrees the transforms, the interpolation and
// the frustum are correct. So the game says what it believes, the counters say
// what the frame did, and the difference between them is the answer.
void MagicPortalsLayer::reportSprites(entt::registry& registry) {
    using namespace Supersonic;
    if (!m_loaded || m_sprites.empty()) return;
    if (m_camera == entt::null || !registry.valid(m_camera)) return;

    // What the camera shows, in world metres, exactly as the renderer's own
    // cull will judge it.
    const auto& camera = registry.get<CameraComponent>(m_camera);
    const float halfHeight = camera.orthoHeight * 0.5f;
    const float halfWidth = halfHeight * camera.aspect;
    const glm::vec2 viewMin(camera.position.x - halfWidth, camera.position.y - halfHeight);
    const glm::vec2 viewMax(camera.position.x + halfWidth, camera.position.y + halfHeight);

    m_onScreenLast.resize(m_sprites.size(), char{0});

    const Supersonic::RenderSystem::Stats** slot =
        registry.ctx().find<const Supersonic::RenderSystem::Stats*>();
    const Supersonic::RenderSystem::Stats* stats = slot != nullptr ? *slot : nullptr;

    const auto counters = [stats]() -> std::string {
        if (stats == nullptr) return std::string(" (no counters)");
        return " [drawn " + std::to_string(stats->drawn) + ", culled " + std::to_string(stats->culled) +
               ", blended " + std::to_string(stats->transparentDrawn) + ", dropped " +
               std::to_string(stats->dropped) + "]";
    };

    int onScreen = 0;
    for (std::size_t i = 0; i < m_sprites.size(); ++i) {
        const DrawnSprite& drawn = m_sprites[i];

        bool showing = false;
        glm::vec3 centre(0.0f);
        glm::vec3 half(0.0f);
        if (drawn.quad != entt::null && registry.valid(drawn.quad)) {
            const auto& transform = registry.get<TransformComponent>(drawn.quad);
            const auto* renderable = registry.try_get<RenderableComponent>(drawn.quad);
            // The quad primitive is one unit on a side and centred, so its box
            // is its own scale about its own position.
            centre = transform.position;
            half = transform.scale * 0.5f;
            showing = renderable != nullptr && renderable->isVisible &&
                      centre.x + half.x >= viewMin.x && centre.x - half.x <= viewMax.x &&
                      centre.y + half.y >= viewMin.y && centre.y - half.y <= viewMax.y;
        }
        if (showing) ++onScreen;

        const char was = m_onScreenLast[i];
        const char now = showing ? char{1} : char{0};
        m_onScreenLast[i] = now;
        if (was == now && m_reportedOnce && !m_dumpRequested) continue;

        // Only what changed, unless G asked for everything. A level draws
        // twenty of these and a walk crosses an edge every few seconds, so the
        // log stays short enough to read.
        if (m_dumpRequested || was != now) {
            const auto* renderable = drawn.quad != entt::null && registry.valid(drawn.quad)
                                         ? registry.try_get<RenderableComponent>(drawn.quad)
                                         : nullptr;
            const auto* material = drawn.quad != entt::null && registry.valid(drawn.quad)
                                       ? registry.try_get<MaterialComponent>(drawn.quad)
                                       : nullptr;
            SUPERSONIC_LOG_INFO("Magic Portals")
                << (drawn.quad == entt::null ? "GONE    " : (showing ? "on      " : "off     "))
                << drawn.sprite.texture
                << "  at (" << centre.x << ", " << centre.y << ") size (" << half.x * 2.0f << " x "
                << half.y * 2.0f << ")"
                << "  visible=" << (renderable != nullptr && renderable->isVisible ? 1 : 0)
                << " mesh=" << (renderable != nullptr ? renderable->meshID : 0u)
                << " albedo=" << (renderable != nullptr ? renderable->albedoTextureID : 0u)
                << " alpha=" << (material != nullptr ? material->albedoColor.a : -1.0f)
                << counters() << std::endl;
        }
    }

    // A frame that refused a draw says so once, loudly, whatever else changed:
    // it is the one outcome that was invisible to every counter until now.
    if (stats != nullptr && stats->dropped > 0) {
        SUPERSONIC_LOG_ERROR("Magic Portals")
            << "the renderer REFUSED " << stats->dropped
            << " draw(s) this frame; its instance buffer was full." << std::endl;
    }

    if (m_dumpRequested) {
        SUPERSONIC_LOG_INFO("Magic Portals")
            << "-- " << onScreen << " of " << m_sprites.size() << " sprite(s) on screen"
            << counters() << std::endl;
    }
    m_dumpRequested = false;
    m_reportedOnce = true;
}

// ---- sound ------------------------------------------------------------------
//
// The original's AudioManager, read from the port's sounds.json
// (sim/Sounds.hpp) and fired from what the port can watch its own simulation
// do: a counter that went up, a flag that turned over.
//
// The division is the particles', for the particles' reason. The TICK only
// latches an event's NAME; the FRAME plays it. So no clip, no random draw and
// no missing file can reach Game::Level, the simulation's clock or the state
// hash, and a run with no audio device at all takes exactly the same path
// through the simulation - which is how every suite runs, and what keeps a
// replay a replay.

void MagicPortalsLayer::loadSounds() {
    std::string error;
    if (!Sounds::LoadRules(m_paths.portData + "/sounds.json", m_soundRules, error)) {
        // A game with no sound is still a game, so this does not refuse to
        // start - but it says so once, because the alternative is silence that
        // looks exactly like silence nobody asked about.
        m_soundsError = error;
        SUPERSONIC_LOG_ERROR("Magic Portals") << "no sound: " << error << std::endl;
    }
}

std::vector<std::string> MagicPortalsLayer::LatchedSounds() const {
    std::vector<std::string> events;
    events.reserve(m_latched.size());
    for (const Latched& latched : m_latched) events.push_back(latched.event);
    return events;
}

std::vector<double> MagicPortalsLayer::LatchedVolumes() const {
    std::vector<double> volumes;
    volumes.reserve(m_latched.size());
    for (const Latched& latched : m_latched) volumes.push_back(latched.volume);
    return volumes;
}

void MagicPortalsLayer::latch(const std::string& event, double doorStrideMs, double volume) {
    // Only what the table names. An event it leaves silent, or does not know,
    // costs nothing here and says nothing.
    if (m_soundRules.ForEvent(event) == nullptr) return;
    m_latched.push_back(Latched{event, doorStrideMs, volume});
    // Said with the tick, so a run's log is the census of what it latched, not of what
    // played: a hook's timer drops some, and a hook may ignore the volume (planning step 142).
    SUPERSONIC_LOG_INFO("Magic Portals") << "sound " << event
                                         << (volume >= 0.0 ? " at volume " + std::to_string(volume) : std::string())
                                         << " on tick " << m_ticks << std::endl;
}

double MagicPortalsLayer::soundRandom(double from, double to) {
    if (to <= from) return from;
    std::uniform_real_distribution<double> spread(from, to);
    return spread(m_soundRandom);
}

float MagicPortalsLayer::pitchFor(const Sounds::Hook& hook, double doorStrideMs) {
    double speed = hook.speed;
    if (hook.speedIsRandom) {
        speed = soundRandom(hook.speedFrom, hook.speedTo);
    } else if (hook.speedFromDoorStride) {
        // playDoorOpenSound scales by 3000 / the door's own stride, so a slow
        // door is a slow sound.
        speed = doorStrideMs > 0.0 ? 3000.0 / doorStrideMs : 1.0;
    }
    // XAudio2 allows a source voice a frequency ratio of 2 unless it was
    // created to allow more, and the engine creates them plainly. A door
    // quicker than 1500 ms asks for more than that, so this clamps rather than
    // letting the voice fail - a divergence, and stated: the original has no
    // such ceiling.
    if (speed < 0.25) speed = 0.25;
    if (speed > 2.0) speed = 2.0;
    return static_cast<float>(speed);
}

// THE DEPTH A CALL GIVES ITS ENTITY, on the level's own axis (the visuals plan's
// 7.3).
//
// The script writes a z two ways, and they are not the same number. Through
// g_scale.scale it is already in the level's units, because the levels' own
// eth_z are scaled by the same factor - a node at eth_z 2 stands at 2 - so
// scale(32) is 32. A BARE LITERAL is in the engine's world units and does not
// grow with the screen (00_order 8.12b), so it is divided by the scale factor,
// exactly as the character's own AddScaledEntity z of 4 is: 4 / 2.8125 = 1.42
// (art.json depth, step 76). appear_effect's z is the one such number the port
// draws with - a bare 2, which is 0.71 on the level's axis and still in front of
// the player - and fireball.ent's and small_explosion.ent's bare 0 is 0 either
// way.
//
// A SYSTEM'S OWN <StartPoint> z IS NOT A RAW LITERAL, and is not divided here
// (step 92, correcting what step 86 handed on). ETHParticleSystem::Scale
// multiplies the whole start point, z included (ETHParticleSystem.cpp:27-33), by
// the scale the particle manager is made at (ETHParticleManager.cpp:90), which is
// the entity's own (ETHSpriteEntity.cpp:316-319) - and AddEntity makes it
// Scale() x the engine's global scale (ETHScriptWrapper.Scene.cpp:146-148), a
// global scale this game never sets, so 1. Every call site the port draws passes
// g_scale.getScale() or g_scale.scale(k) as that Scale() (appearEffect #2-6 and
// #26, vanishEffect, insertPortal, addFireball, explode #1-23, the character's
// constructor), so the start point is on the level's axis at the effect's own
// scale, which Particles::Scaled gives it in SpawnEffect: the appear ring's flash
// at 0.71 + 10, the eyes' 2 and the fireball's 4 x its scale, as steps 84-86 drew
// them.
MagicPortalsLayer::EffectSpawn MagicPortalsLayer::effectAdd(const Effects::Spawn& spawn) const {
    EffectSpawn add;
    add.entity = spawn.entity;
    add.atPx = spawn.atPx;
    // The factor is art.json's and is refused below 0 when it is read, so the
    // guard is for a level drawn with no art at all - which draws no effect.
    add.zUnits = spawn.zBaseUnits +
                 (spawn.zScaled || !(m_artRules.scaleFactor > 0.0) ? spawn.z : m_artRules.LevelDepth(spawn.z));
    add.angleDeg = spawn.angleDeg;
    add.scale = spawn.scale;
    return add;
}

// WHERE THE EFFECTS ARE TRIGGERED (system_7 7a-2), beside the sounds and for the
// same reason: the tick says what happened, and the frame draws it.
//
// EACH EFFECT'S RULE ARRIVES WITH THE STEP THAT DRAWS IT - the fireballs and
// their explosions (7.2) and the teleport's three (7.3) are here; the pickups'
// are 10.x - and each reads the same kind of thing the sound watch reads: a
// counter that went up, a flag that turned over, a record the tick left behind
// (Portals::State's transits, Turrets::State::spentPx, Fire::State::blastsPx).
// The rule is that queuing happens on the TICK while adding and drawing happen on
// the FRAME, so that an effect cannot reach Game::Level or the state hash by the
// back door.
void MagicPortalsLayer::latchSimEffects(const entt::registry& registry, const ScriptClock::Step& step) {
    if (!m_loaded) return;

    // A MINION'S OWN SYSTEMS (the visuals plan's 11.4, planning step 97). The
    // entity a marker adds is ghost_minion.ent on 3-32 (R16), and that .ent
    // carries 24 black_portal_particle.png at alphaMode 4: the knights' dark
    // aura, a multiply (particleTexture). spawnMinion's AddEntity adds them with
    // the entity, so they are queued once, when the minion is matched, and are
    // carried by its PICTURE (EffectOwner::Kind::Minion) until it is taken away,
    // when they go with it. The runtime draws no picture for it: the knight is
    // syncMinions'. At the minion's own depth - a raw 2 over the factor for a
    // marker's - whose system's <StartPoint> z of -2 puts them behind it
    // (SpawnEffect). On the effects' stream (00_order K8): nothing the level
    // places draws a number more or less for them.
    //
    // A minion.ent carries none, so nothing is added for the 61 other markers or
    // an escort. dragon_knight.ent carries the same system, and comes only once
    // 2-32 is finished: syncKnight queues its own (planning step 123).
    matchMinions();
    for (std::size_t i = 0; i < m_minions.size() && i < m_level.minions.minions.size(); ++i) {
        MinionQuad& drawn = m_minions[i];
        const Minions::Minion& minion = m_level.minions.minions[i];
        if (!drawn.systems || drawn.systemsQueued || minion.gone || minion.body == entt::null) continue;
        EffectSpawn add;
        add.entity = minion.entity;
        add.atPx = minion.spawnPx; // where the marker added it; its picture's point replaces it
        add.zUnits = drawn.lookZ;
        // spawnMinion's AddEntity angle and getScale() (minions.json spawn).
        add.angleDeg = m_level.minions.rules.spawnAngleDeg;
        add.scale = m_level.minions.rules.spawnScale;
        add.follow.kind = EffectOwner::Kind::Minion;
        add.follow.index = static_cast<int>(i);
        add.picture = false;
        add.alwaysActive = true;
        m_effectQueue.push_back(add);
        drawn.systemsQueued = true;
    }

    // THE FIREBALLS IN FLIGHT. Each one is an entity of the original's -
    // addFireball's fireball.ent - and the entity is the whole of what is seen:
    // it has no <Sprite> at all, so what a player meets is its particle system
    // and its light's halo. The effect is given the fireball to FOLLOW, so it
    // travels with it and goes when it goes; fireball.ent repeats forever and is
    // therefore never temporary, which is exactly the original's rule that only
    // its own script deletes it.
    //
    // A LIST WALK rather than an event, because a fireball can be spat on a tick
    // this is not called on - BeforeStep runs under a pause that this returns
    // before (OnFixedUpdate), and Turrets::Fire is in BeforeStep. Whatever is in
    // the air and has no effect yet gets one; what is queued this tick is added
    // on the next frame, and if the fireball is gone by then SpawnEffect refuses
    // it, which is a fireball nobody ever saw.
    std::vector<std::string> flying;
    flying.reserve(m_level.turrets.fireballs.size());
    for (const Turrets::Fireball& ball : m_level.turrets.fireballs) {
        flying.push_back(ball.name);
        if (std::find(m_fireballEffects.begin(), m_fireballEffects.end(), ball.name) != m_fireballEffects.end()) {
            continue;
        }
        EffectSpawn add = effectAdd(Effects::Fireball(m_effectRules, ball.atPx));
        // ScaleParticleSystem(0, fireballScale), which only the dark dragon's fire
        // is given: 2 there and 1 everywhere else (darkdragon.json).
        add.firstSystemScale = ball.particleScale;
        add.follow.kind = EffectOwner::Kind::Fireball;
        add.follow.name = ball.name;
        m_effectQueue.push_back(add);
        m_fireballEffects.push_back(ball.name);
    }
    // And the names of the ones that have gone, so a level that spits hundreds
    // does not keep every name it ever used.
    m_fireballEffects.erase(std::remove_if(m_fireballEffects.begin(), m_fireballEffects.end(),
                                           [&flying](const std::string& name) {
                                               return std::find(flying.begin(), flying.end(), name) == flying.end();
                                           }),
                            m_fireballEffects.end());

    // WHERE ONE WAS DESTROYED: small_explosion.ent at the point it stood, which
    // is what ETHCallback_fireball adds on the frame after the contact sets
    // `destroy`. The port is that one frame early and about two units short of
    // the original's point, because the original's callback moves the entity one
    // more frame before it reads the flag (130 px/s over a tick).
    for (const glm::dvec2& atPx : m_level.turrets.spentPx) {
        m_effectQueue.push_back(effectAdd(Effects::SmallExplosion(m_effectRules, atPx)));
    }
    // AND WHERE A BOMB WENT OFF: explode()'s explosion_large.ent, at the bomb and
    // 32 units in front of it. Both are temporary - no sprite, and every system
    // repeating a set number of times - so the runtime frees each when its pools
    // are over, which is the engine's own rule and needs no owner.
    for (const glm::dvec2& atPx : m_level.fire.blastsPx) {
        m_effectQueue.push_back(effectAdd(Effects::LargeExplosion(m_effectRules, atPx)));
    }

    // WHAT WENT THROUGH A PORTAL, and the three entities the original adds for
    // each: the suck effect on the rim of the end it went in by, on the side it
    // came from; the sparkles where it stood; and the flash at the end it comes
    // out of (system_7 7c, effects.json's three rows). Whatever the traveller is
    // - the player, a crate, a thrown stone - because vanishEffect is called for
    // any entityToTeleport, and on the static route as well as the placed one:
    // ETHCallback_portal_static #144/#165 calls the same pair that
    // teleportToOther does, and the port's one Tick loop is both.
    //
    // IN THE ORIGINAL'S OWN ORDER, which is the order they are added in and so
    // the order they draw in and draw their random from: vanishEffect's suck
    // effect (#36-69) before its sparkles (#83-112), then appearEffect (#71-75).
    // That a fireball's effect above is queued before a teleport's is the port's
    // order and not a decoded one - the original's two callbacks run in whatever
    // order its scene walk reaches them - and it shows only in the stream, never
    // in a placement.
    for (const Portals::Traversal& went : m_level.portals.transits) {
        m_effectQueue.push_back(effectAdd(Effects::Suck(m_effectRules, went.travellerPx, went.entryPx)));
        m_effectQueue.push_back(effectAdd(Effects::Sparkles(m_effectRules, went.travellerPx, went.entryPx)));
        m_effectQueue.push_back(effectAdd(Effects::Appear(m_effectRules, went.exitPx)));
    }

    // THE SHOT'S OWN, from what Portals::Tick recorded this tick (the visuals
    // plan's 10.1, effects.json's shot-side rows), in the order the original's
    // callbacks add them: a leg's first tick, a turn, an end.
    //
    // A LEG'S FIRST TICK: ETHCallback_projectile's first frame jumps the shot 24
    // along its direction and flashes projectile_shoot there - before it moves
    // on, so a leg that ends on this tick has flashed too.
    for (const Portals::Launch& leg : m_level.portals.launches) {
        m_effectQueue.push_back(effectAdd(Effects::ProjectileShoot(m_effectRules, leg.fromPx, leg.towardPx)));
    }
    // A TURN OFF A REFLECTOR: ETHCallback_reflect_agent runs computePortalFinalPos
    // for the new leg as a reflected shot - red_waves (A) at the new destination
    // where the ray meets nothing and that point is in a field (#198-215) - adds
    // the new projectile with its mark at the clipped destination (#216-235), and
    // flashes where the old one was (#246-260). The new leg flashes again on its
    // own first tick, which the launches above give on the next.
    for (const Portals::Reflection& turn : m_level.portals.reflected) {
        if (turn.said.redWaves) m_effectQueue.push_back(effectAdd(Effects::At(m_effectRules.redWaves, turn.toPx)));
        const Effects::Rule& mark = turn.said.willDie ? m_effectRules.redCross : m_effectRules.portalMark;
        m_effectQueue.push_back(effectAdd(Effects::At(mark, turn.said.atPx)));
        m_effectQueue.push_back(effectAdd(Effects::At(m_effectRules.projectileShootReflected, turn.atPx)));
    }
    // AN END WITH NO PORTAL, where the port's shot ended it: an anti_projectile_wall
    // kills it with portal_fail_in_antiportal wherever it is
    // (ETHCallback_anti_projectile_wall); anything else - a body, or the tap it
    // could open nothing at - is ETHCallback_projectile's arrival of a doomed
    // shot, which is portal_fail_in_antiportal inside a field and portal_fail
    // outside one. Their shakes are latchSimQuakes'.
    for (const Portals::Failure& ended : m_level.portals.failures) {
        const bool field = ended.kind == Portals::FailKind::Blocker || ended.inField;
        const Effects::Rule& rule = field ? m_effectRules.portalFailInField : m_effectRules.portalFail;
        m_effectQueue.push_back(effectAdd(Effects::At(rule, ended.atPx)));
        // Said with the tick, so a capture can find the end it measures.
        SUPERSONIC_LOG_INFO("Magic Portals") << "shot ended on tick " << m_ticks << " at (" << ended.atPx.x << ", "
                                             << ended.atPx.y << ") by "
                                             << (ended.kind == Portals::FailKind::Blocker  ? "a blocker"
                                                 : ended.kind == Portals::FailKind::Body ? "a body"
                                                                                         : "a refusal")
                                             << ": " << rule.entity << std::endl;
    }
    // A SHOT AN ANTI-PORTAL AGENT KILLED (step 126): ETHCallback_anti_portal_agent's
    // antiportal_explosion at the agent's point and scale(32), with no owner - the
    // agent is deleted in the same call. Its shake is latchSimQuakes', its sound
    // latchSimSounds'.
    for (const Portals::AgentKill& kill : m_level.portals.agentKills) {
        m_effectQueue.push_back(effectAdd(Effects::At(m_effectRules.antiportalExplosion, kill.atPx)));
        SUPERSONIC_LOG_INFO("Magic Portals") << "shot killed " << kill.agent << " on tick " << m_ticks << " at ("
                                             << kill.shotPx.x << ", " << kill.shotPx.y << "); ring "
                                             << (kill.ring.empty() ? std::string("none") : kill.ring)
                                             << " deleted" << std::endl;
    }

    // WHAT A CRYSTAL LEAVES WHERE IT WAS (the visuals plan's 10.2; effects.json
    // _crystals): pickCrystalEffect's crystal_pick where the character takes one,
    // and ETHCallback_timer's crystal_vanish, at 1.3, where a timed one's time runs
    // out while it is still there. Each is added at the crystal's GetPosition() and
    // 20 in front of the crystal's own depth, with no owner - the crystal is
    // deleted in the same call - so it stays where it was added and is freed when
    // its particles are over.
    //
    // THE POINT is the crystal's entity where its picture was last drawn: the tick
    // before this one placed it, bob and all (syncSprites, which runs after this),
    // and linearMotion's SetPosition is what GetPosition returns - 00_order K2's
    // and K11's final-centre read. Its depth is the node's eth_z (lookZ), which
    // linearMotion keeps. The original's crystal and character callbacks run in an
    // order the scene walk gives, so its point may be this frame's bob rather than
    // the last one's: at 2 rad/s and 1.5 units, at most 0.05 of a unit apart.
    //
    // Goals makes the two exclusive - a crystal taken on the tick its time runs out
    // is taken - and so does the original: the timer finds the crystal deleted by
    // the pick (#53-79) and adds nothing (#127-133), so a crystal taken early has
    // its dial shrink away with no burst. Queued after every other rule above: the
    // order two callbacks add their entities in is the scene walk's, undecoded, and
    // it shows only in the effects' stream (00_order K8), never in a placement.
    const std::vector<Goals::Crystal>& crystals = m_level.goals.crystals;
    if (m_crystalEffects.size() != crystals.size()) m_crystalEffects.assign(crystals.size(), 0);
    for (std::size_t i = 0; i < crystals.size(); ++i) {
        const Goals::Crystal& crystal = crystals[i];
        if (m_crystalEffects[i] != 0 || !(crystal.collected || crystal.expired)) continue;
        m_crystalEffects[i] = 1;
        const auto drawn = std::find_if(m_sprites.begin(), m_sprites.end(), [i](const DrawnSprite& sprite) {
            return sprite.crystal == static_cast<int>(i);
        });
        // No picture of it, so no entity point of it: a level drawn as boxes, which
        // draws no effect at all.
        if (drawn == m_sprites.end()) continue;
        const Effects::Rule& rule = crystal.collected ? m_effectRules.crystalPick : m_effectRules.crystalVanish;
        const Effects::Spawn spawn = Effects::OnEntity(rule, drawn->ownerPx, drawn->lookZ);
        m_effectQueue.push_back(effectAdd(spawn));
        // Said with the tick, so a capture can find what it measures.
        SUPERSONIC_LOG_INFO("Magic Portals") << "crystal " << crystal.name
                                             << (crystal.collected ? " taken" : " gone at its time") << " on tick "
                                             << m_ticks << " at (" << spawn.atPx.x << ", " << spawn.atPx.y << ") z "
                                             << spawn.z << ": " << rule.entity << " x " << rule.scale << std::endl;
    }

    // WHAT A KEY AND ITS KEYHOLE LEAVE (the visuals plan's 10.3; effects.json _keys),
    // each where its callback adds it and with no owner, after the crystals for the
    // reason those are after the rest: the scene walk orders two callbacks, and it
    // shows only in the effects' stream.
    const auto pictureOf = [this](auto&& is) {
        const auto drawn = std::find_if(m_sprites.begin(), m_sprites.end(), is);
        return drawn == m_sprites.end() ? nullptr : &*drawn;
    };
    const std::vector<Keys::Key>& keys = m_level.keys.keys;
    const std::vector<Keys::Keyhole>& keyholes = m_level.keys.keyholes;
    if (m_keyEffects.size() < keys.size()) m_keyEffects.resize(keys.size());
    for (std::size_t i = 0; i < keys.size(); ++i) {
        const Keys::Key& key = keys[i];
        KeyWatch& watch = m_keyEffects[i];
        // TAKEN: ETHCallback_key's free branch adds key_pick at the key's own
        // GetPosition() - its bob of this frame, its eth_z - plus scale(32) as a
        // carrier takes it, the player or a minion alike. The point is where the
        // last tick drew the key: this tick's Keys::Tick has already trailed it
        // toward the carrier. A key a boss dropped has no level picture: its point
        // is where the runtime last drew its key.ent (EffectOwner::Kind::Key, its
        // bob with it) and its depth that entity's (planning step 146). A level
        // drawn as boxes has neither, and adds none, as a crystal's has none.
        if (key.owner != entt::null && key.owner != watch.owner) {
            const int index = static_cast<int>(i);
            std::optional<Effects::Spawn> spawn;
            if (const DrawnSprite* drawn = pictureOf([index](const DrawnSprite& s) { return s.key == index; })) {
                spawn = Effects::OnEntity(m_effectRules.keyPick, drawn->ownerPx, drawn->lookZ);
            } else {
                for (const Effect& dropped : m_effects) {
                    if (dropped.follow.kind != EffectOwner::Kind::Key || dropped.follow.index != index) continue;
                    spawn = Effects::OnEntity(m_effectRules.keyPick, dropped.atPx, dropped.zUnits);
                    break;
                }
            }
            if (spawn) {
                m_effectQueue.push_back(effectAdd(*spawn));
                SUPERSONIC_LOG_INFO("Magic Portals") << "key " << key.name << " taken on tick " << m_ticks << " at ("
                                                     << spawn->atPx.x << ", " << spawn->atPx.y << ") z " << spawn->z
                                                     << ": " << spawn->entity << std::endl;
            }
        }
        watch.owner = key.owner;
        // SPENT: the fly-in's clock, whose frame 1 adds key_unlock_halo at
        // vector3(keyholePos, scale(32)) - the keyhole it found, at a depth of its
        // own - 41 ticks after the unlock at 60 Hz. The flight itself is not built.
        const bool haloNow =
            Effects::AdvanceFlyIn(m_effectRules, watch.flyIn, key.spent, ScriptClock::Ms(step));
        if (haloNow && key.opened >= 0 && key.opened < static_cast<int>(keyholes.size())) {
            const Keys::Keyhole& found = keyholes[static_cast<std::size_t>(key.opened)];
            const Effects::Spawn spawn = Effects::At(m_effectRules.keyUnlockHalo, found.atPx);
            m_effectQueue.push_back(effectAdd(spawn));
            SUPERSONIC_LOG_INFO("Magic Portals") << "key " << key.name << " into " << found.name << ": halo on tick "
                                                 << m_ticks << " (" << watch.flyIn.ms << " ms of its fly-in) at ("
                                                 << spawn.atPx.x << ", " << spawn.atPx.y << ") z " << spawn.z << ": "
                                                 << spawn.entity << std::endl;
        }
    }
    // GONE: ETHCallback_keyhole, once its fade has left nothing, adds in this order
    // wall_destroy at its door's point and depth, door_locked_vanish 2 raw units in
    // front of the door, and keyhole_vanish scale(32) in front of itself - then
    // shakes (latchSimQuakes' door_unlocked) and deletes both. The door's point is
    // its NODE's, which is its entity's: the picture's ownerPx is its body's. If
    // the keyhole or its door has no picture, none of the three is queued, though
    // the burst needs only the keyhole's, the rest only the door's; no level lacks one.
    if (m_keyholeEffects.size() != keyholes.size()) m_keyholeEffects.assign(keyholes.size(), 0);
    for (std::size_t j = 0; j < keyholes.size(); ++j) {
        const Keys::Keyhole& keyhole = keyholes[j];
        if (m_keyholeEffects[j] != 0 || !keyhole.gone) continue;
        m_keyholeEffects[j] = 1;
        const int index = static_cast<int>(j);
        const DrawnSprite* hole = pictureOf([index](const DrawnSprite& s) { return s.keyhole == index; });
        const DrawnSprite* door =
            pictureOf([&keyhole](const DrawnSprite& s) { return s.sprite.node == keyhole.doorName; });
        if (hole == nullptr || door == nullptr) continue;
        const Effects::Spawn dust = Effects::OnEntity(m_effectRules.wallDestroyAtDoor, door->sprite.atPx, door->lookZ);
        const Effects::Spawn sparkles = Effects::OnEntityBare(m_effectRules.doorLockedVanish, door->sprite.atPx,
                                                              door->lookZ, m_artRules.scaleFactor);
        const Effects::Spawn burst = Effects::OnEntity(m_effectRules.keyholeVanish, hole->sprite.atPx, hole->lookZ);
        for (const Effects::Spawn* spawn : {&dust, &sparkles, &burst}) m_effectQueue.push_back(effectAdd(*spawn));
        SUPERSONIC_LOG_INFO("Magic Portals")
            << "keyhole " << keyhole.name << " gone on tick " << m_ticks << " with " << keyhole.doorName << ": "
            << dust.entity << " at (" << dust.atPx.x << ", " << dust.atPx.y << ") z " << dust.z << ", "
            << sparkles.entity << " z " << sparkles.z << ", " << burst.entity << " at (" << burst.atPx.x << ", "
            << burst.atPx.y << ") z " << burst.z << std::endl;
    }

    // WHAT A DIAMOND, A SHOCK RING AND A FIRE SIGNAL LEAVE (the visuals plan's 10.4;
    // effects.json _shock_burn_extinguish), each where its callback adds it and with
    // no owner, after the keys for the reason those are after the rest. Every
    // number is said with its tick, so a capture can find what it measures.
    const auto logged = [this](const char* what, const Effects::Spawn& spawn) {
        SUPERSONIC_LOG_INFO("Magic Portals") << what << " on tick " << m_ticks << ": " << spawn.entity << " at ("
                                             << spawn.atPx.x << ", " << spawn.atPx.y << ") z " << spawn.z
                                             << (spawn.zScaled ? "" : " bare") << " angle " << spawn.angleDeg
                                             << std::endl;
    };
    const double factor = m_artRules.scaleFactor;
    // PER DIAMOND, in Diamonds' order, as each one's callback adds them. A diamond's
    // point is the simulation's (an unowned one does not move, and Carry::Acquire
    // does not move one as it is taken); its depth is its node's eth_z, the
    // picture's lookZ. A diamond with no picture (a level drawn as boxes) has no
    // depth to add them at, and adds none.
    const std::vector<Diamonds::Diamond>& diamonds = m_level.diamonds.diamonds;
    if (m_diamondEffects.size() < diamonds.size()) m_diamondEffects.resize(diamonds.size());
    for (std::size_t i = 0; i < diamonds.size(); ++i) {
        const Diamonds::Diamond& diamond = diamonds[i];
        DiamondWatch& watch = m_diamondEffects[i];
        const bool held = !diamond.gone && diamond.owner != entt::null;
        const bool heldBefore = watch.held;
        const bool goneBefore = watch.gone;
        watch.held = held;
        watch.gone = diamond.gone;
        // Its carried arm's SetAlpha(0.1f) (art.json carried_diamond), on the second
        // tick in a row it is held - the pick's own update ran the free arm - and
        // kept, as nothing sets it back, until it goes or 3-32's boss puts it back as
        // the new entity addEntityAtSpawnPoint adds.
        watch.heldTicks = held ? watch.heldTicks + 1 : 0;
        if (watch.heldTicks >= 2) watch.faded = true;
        if (goneBefore && !diamond.gone) watch.faded = false;
        const int index = static_cast<int>(i);
        const DrawnSprite* drawn = pictureOf([index](const DrawnSprite& s) { return s.diamond == index; });
        if (drawn == nullptr) continue;
        // TAKEN: the free arm's AddScaledEntity at the diamond's own GetPosition() -
        // shock_diamond_pick for a shock diamond; for a fire diamond burn_projectile,
        // a bare 10 in front of it (ETHCallback_fire_diamond #159-186).
        if (held && !heldBefore) {
            if (diamond.fire) {
                const Effects::Spawn burn =
                    Effects::OnEntityBare(m_effectRules.burnAtPickup, diamond.atPx, drawn->lookZ, factor);
                m_effectQueue.push_back(effectAdd(burn));
                logged(("fire diamond " + diamond.name + " taken").c_str(), burn);
            } else {
                const Effects::Spawn pick = Effects::OnEntity(m_effectRules.shockDiamondPick, diamond.atPx, drawn->lookZ);
                m_effectQueue.push_back(effectAdd(pick));
                logged(("shock diamond " + diamond.name + " taken").c_str(), pick);
            }
        }
        // ITS STRIKE, from the tick's record: the minion is destroyed by now.
        // shock_death at the MINION's GetPosition() - its entity point, the body's
        // centre less EntityAboveCentrePx (R25's point), at its own depth - then
        // shock_strike at the midpoint of the two, turned by the radians
        // getAngle gives (Effects::Strike). The 30 it was struck within is still
        // measured to the body's centre (Diamonds; the backlog's prey-point row).
        for (const Diamonds::Strike& strike : m_level.diamonds.strikes) {
            if (strike.diamond != i) continue;
            const glm::dvec2 enemyPx =
                strike.preyCentrePx - glm::dvec2(0.0, m_level.minions.rules.EntityAboveCentrePx());
            double enemyZ = m_artRules.LevelDepth(m_artRules.minion.z);
            const std::vector<Minions::Minion>& minions = m_level.minions.minions;
            for (std::size_t j = 0; j < minions.size() && j < m_minions.size(); ++j) {
                if (minions[j].body != strike.prey) continue;
                enemyZ = m_minions[j].lookZ;
                break;
            }
            const Effects::Spawn death = Effects::OnEntity(m_effectRules.shockDeath, enemyPx, enemyZ);
            const Effects::Spawn bolt =
                Effects::Strike(m_effectRules.shockStrike, strike.diamondPx, drawn->lookZ, enemyPx, enemyZ);
            m_effectQueue.push_back(effectAdd(death));
            m_effectQueue.push_back(effectAdd(bolt));
            logged(("shock diamond " + diamond.name + " struck").c_str(), death);
            logged(("shock diamond " + diamond.name + " struck").c_str(), bolt);
        }
        // DOWN A DRAIN: a fire diamond destroyed - only a gutter mouth destroys one -
        // adds extinguish_fire at its own GetPosition() (#427-449).
        if (diamond.fire && diamond.gone && !goneBefore) {
            const Effects::Spawn water = Effects::OnEntity(m_effectRules.extinguishAtDiamond, diamond.atPx, drawn->lookZ);
            m_effectQueue.push_back(effectAdd(water));
            logged(("fire diamond " + diamond.name + " drained").c_str(), water);
        }
    }
    // EACH SHOT A CARRIED FIRE DIAMOND TURNED: burnProjectile's burn_projectile at
    // vector3(the shot's point, 10), a depth of its own (Diamonds::State::burnedPx).
    for (const glm::dvec2& atPx : m_level.diamonds.burnedPx) {
        const Effects::Spawn burn = Effects::At(m_effectRules.burnAtShot, atPx);
        m_effectQueue.push_back(effectAdd(burn));
        logged("shot burned", burn);
    }
    // AND EACH SHOT A FIRE AGENT'S FLAME TURNED: the same burnProjectile, called by
    // ETHCallback_fire_agent #265, at the point the shot first came within reach
    // (Portals::State::burns; planning step 134).
    for (const Portals::Burn& burned : m_level.portals.burns) {
        const Effects::Spawn burn = Effects::At(m_effectRules.burnAtShot, burned.atPx);
        m_effectQueue.push_back(effectAdd(burn));
        logged(("shot burned by " + burned.flame).c_str(), burn);
    }
    // A SHOCK AGENT A FIREBALL DESTROYED: ETHCallback_shock_agent's destroy arm adds
    // shock_death at the agent's GetPosition() plus a bare 10 (#301-327). Its
    // small_explosion (#331-345) is not this step's.
    const std::vector<Fields::Field>& fields = m_level.fields.fields;
    if (m_fieldEffects.size() < fields.size()) m_fieldEffects.resize(fields.size(), 0);
    for (std::size_t i = 0; i < fields.size(); ++i) {
        const Fields::Field& field = fields[i];
        const bool goneBefore = m_fieldEffects[i] != 0;
        m_fieldEffects[i] = field.gone ? 1 : 0;
        if (!field.gone || goneBefore) continue;
        const DrawnSprite* drawn = pictureOf([&field](const DrawnSprite& s) { return s.sprite.node == field.name; });
        if (drawn == nullptr) continue;
        const Effects::Spawn death =
            Effects::OnEntityBare(m_effectRules.shockDeathAtAgent, field.atPx, drawn->lookZ, factor);
        m_effectQueue.push_back(effectAdd(death));
        logged(("shock agent " + field.name + " destroyed").c_str(), death);
    }
    // A RING'S KILL: ETHCallback_shock_area adds shock_death at the character's
    // GetPosition() as it sets its hp to 0 (#136-167), and only when the character
    // is not dead already (isCharacterDead, #125-134). Fields claims a kill
    // whether or not the player died first, so the rule is: the ring claimed it
    // this tick, the player was alive at the last latch, and the death Hazards
    // records is the ring's.
    const bool ringKilled = m_level.fields.playerKilled;
    const bool dead = m_level.hazards.playerDied;
    if (ringKilled && !m_ringKillSeen && !m_playerDeadSeen && dead &&
        m_level.hazards.killedBy == m_level.fields.killedBy && m_level.player != entt::null &&
        registry.valid(m_level.player) && registry.all_of<Supersonic::TransformComponent>(m_level.player)) {
        const glm::dvec2 atPx = Units::ToPixels(registry.get<Supersonic::TransformComponent>(m_level.player).position);
        const Effects::Spawn death = Effects::OnEntity(m_effectRules.shockDeath, atPx, m_playerZ);
        m_effectQueue.push_back(effectAdd(death));
        logged(("player killed by " + m_level.fields.killedBy).c_str(), death);
    }
    m_ringKillSeen = ringKilled;
    m_playerDeadSeen = dead;
    // CHAPTER 4'S TORCHES (the visuals plan's 11.2; effects.json
    // _torches_trail_roundabout), each call's entities in its callback's own order.
    latchTorches();

    // WHAT A WALL AND A STONE LEAVE (the visuals plan's 11.1; effects.json
    // _walls_stones_doors), after the rest for the reason those are: the scene walk
    // orders two callbacks, and it shows only in the effects' stream. The lift
    // doors' dust is latchDoors', at the tick's end, where Puzzle's doors turn.
    //
    // A BREAKABLE WALL THE TICK TOOK - by a stone, or by a script's destroy() (4-32's
    // summon) - on the walls latchSimQuakes shakes for: ETHCallback_breakable_wall
    // adds wall_destroy at its own GetPosition(), the NODE's point and eth_z (the
    // picture's `sprite` and lookZ, kept after its quad goes), turned by its
    // GetAngle() - the node's rotation, clockwise radians, as Ethanon's
    // counter-clockwise degrees - and at its destroyEffectScale. A wall with no
    // picture (a level drawn as boxes) adds none.
    const std::vector<Demolish::Breakable>& breakables = m_level.demolish.breakables;
    if (m_wallEffects.size() != breakables.size()) m_wallEffects.assign(breakables.size(), 0);
    for (std::size_t i = 0; i < breakables.size(); ++i) {
        const Demolish::Breakable& breakable = breakables[i];
        if (m_wallEffects[i] != 0 || !breakable.broken) continue;
        m_wallEffects[i] = 1;
        // A CARRANCA TAKEN (effects.json carranca_destroy, planning step 146): one of
        // Turrets' - carranca and carranca_right, either spelling, whose callbacks
        // both run carrancaCallback - adds carranca_destroy at its own point and
        // depth, turned by its own angle, as its isDestroyed arm does (#123-157).
        const bool carranca =
            std::any_of(m_level.turrets.turrets.begin(), m_level.turrets.turrets.end(),
                        [&breakable](const Turrets::Turret& turret) { return turret.name == breakable.name; });
        if (carranca) {
            const DrawnSprite* stone =
                pictureOf([&breakable](const DrawnSprite& s) { return s.sprite.node == breakable.name; });
            if (stone == nullptr) continue;
            const double angleDeg = -stone->sprite.rotation * 180.0 / 3.14159265358979323846;
            const Effects::Spawn dust =
                Effects::OnEntityTurned(m_effectRules.carrancaDestroy, stone->sprite.atPx, stone->lookZ, angleDeg);
            m_effectQueue.push_back(effectAdd(dust));
            SUPERSONIC_LOG_INFO("Magic Portals") << "carranca " << breakable.name << " destroyed on tick " << m_ticks
                                                 << ": " << dust.entity << " at (" << dust.atPx.x << ", "
                                                 << dust.atPx.y << ") z " << dust.z << " angle " << dust.angleDeg
                                                 << std::endl;
            continue;
        }
        if (!isBreakableWall(breakable.name)) continue;
        const DrawnSprite* wall =
            pictureOf([&breakable](const DrawnSprite& s) { return s.sprite.node == breakable.name; });
        if (wall == nullptr) continue;
        double datum = 0.0;
        if (const Tscn::Node* node = m_data.scene.FindNode(breakable.name)) {
            const Tscn::Value* value = node->Meta(m_effectRules.wallDestroyScaleKey);
            if (value == nullptr || !value->AsNumber(datum)) datum = 0.0;
        }
        const double angleDeg = -wall->sprite.rotation * 180.0 / 3.14159265358979323846;
        const Effects::Spawn dust =
            Effects::WallDestroy(m_effectRules, wall->sprite.atPx, wall->lookZ, angleDeg, Effects::EffectScale(datum));
        m_effectQueue.push_back(effectAdd(dust));
        SUPERSONIC_LOG_INFO("Magic Portals") << "breakable wall " << breakable.name << " destroyed on tick " << m_ticks
                                             << ": " << dust.entity << " at (" << dust.atPx.x << ", " << dust.atPx.y
                                             << ") z " << dust.z << " angle " << dust.angleDeg << " x " << dust.scale
                                             << std::endl;
    }
    // A STONE DESTROYED - its `destroy` datum set by a despawner (Launchers), or on
    // the world or rising into the beholder (Boss) - where the step left it:
    // ETHCallback_rolling_stone adds rolling_stone_destroy at vector3(its
    // GetPositionXY(), scale(32)), a depth of its own, at 1.3. Only rolling_stone's
    // own callback does, so a stone the level placed must be one, in either
    // spelling; one the simulation made - a thrown stone, a rock - has no node and
    // is the rolling_stone.ent launchers.json and boss.json make it (impactBodies'
    // rule; test_mp_layer pins both).
    const auto stoneGone = [this](const Launchers::StoneGone& gone, const char* by) {
        const Tscn::Node* node = m_data.scene.FindNode(gone.name);
        const std::string& entity = m_quakeRules.entity[static_cast<std::size_t>(Earthquake::Call::StoneImpact)];
        if (node != nullptr && !Earthquake::IsEntity(Roles::EntityName(*node), entity)) return;
        const Effects::Spawn puff = Effects::At(m_effectRules.stoneDestroy, gone.atPx);
        m_effectQueue.push_back(effectAdd(puff));
        SUPERSONIC_LOG_INFO("Magic Portals") << "stone " << gone.name << " destroyed by " << by << " on tick "
                                             << m_ticks << ": " << puff.entity << " at (" << puff.atPx.x << ", "
                                             << puff.atPx.y << ") z " << puff.z << " x " << puff.scale << std::endl;
    };
    for (const Launchers::StoneGone& gone : m_level.launchers.despawnedNow) stoneGone(gone, "a despawner");
    // ETHBeginContactCallback_breakable_wall #42-60: the wall a destroyable stone breaks destroys it, and its own callback
    // adds the puff where the step left it (planning step 175).
    for (const Demolish::Taken& taken : m_level.demolish.takenNow) stoneGone(Launchers::StoneGone{taken.stone.name, taken.atPx}, "a wall it broke");
    for (const Launchers::StoneGone& gone : m_level.boss.brokenNow) stoneGone(gone, "the beholder's level");

    // THE TRAIL A WEIGHTLESS PLAYER LEAVES (effects.json movement_trail):
    // MainCharacter::addTrail, from its update - which runs unpaused and not once
    // the level is finished (MainCharacter::update #6-21), so not once the player
    // has gone into the door or died here - only where the character's noGravity is
    // set. Its clock runs every such tick; a trail is added at the character's own
    // point and depth, and rides on it (ETHCallback_movement_trail) - the player
    // owner, which hides with it too, where the original's has nothing to hide.
    if (m_level.noGravity && !m_finishing && !m_dying && m_level.player != entt::null &&
        registry.valid(m_level.player) && registry.all_of<Supersonic::RigidBodyComponent>(m_level.player)) {
        const glm::vec3 velocity = registry.get<Supersonic::RigidBodyComponent>(m_level.player).velocity;
        // addTrail reads GetLinearVelocity of the original's own world, a ratio the player is carried above
        // (Player.hpp): the port's metres a second over SimRatio.
        const double speed = std::sqrt(static_cast<double>(velocity.x) * velocity.x +
                                       static_cast<double>(velocity.y) * velocity.y) /
                             ScriptClock::SimRatio(step);
        if (Effects::TrailDue(m_effectRules, m_trailMs, speed, ScriptClock::Ms(step))) {
            const glm::dvec2 atPx =
                Units::ToPixels(registry.get<Supersonic::TransformComponent>(m_level.player).position);
            const Effects::Spawn trail = Effects::OnEntity(m_effectRules.movementTrail, atPx, m_playerZ);
            EffectSpawn add = effectAdd(trail);
            add.follow.kind = EffectOwner::Kind::Player;
            m_effectQueue.push_back(add);
            SUPERSONIC_LOG_INFO("Magic Portals") << "movement trail on tick " << m_ticks << " at " << speed
                                                 << " m/s: " << trail.entity << " at (" << trail.atPx.x << ", "
                                                 << trail.atPx.y << ") z " << trail.z << std::endl;
        }
    }

    // A LAP ROUND A WELL (gravitywell.json roundabout; effects.json
    // roundabout_effect): computeRoundabout #332-419, for the character alone and
    // only while it is not hidden - the flash at the well's area, the sound, and
    // the agent's clock zeroed in the lap's way round.
    const GravityWell::State& wells = m_level.wells;
    if (m_wellSpins.size() != wells.wells.size()) {
        m_wellSpins.assign(wells.wells.size(), WellSpin{wells.rules.roundabout.spinMs, 1.0});
    }
    for (const GravityWell::Lap& lap : wells.lapsNow) {
        if (lap.body != m_level.player || m_playerQuad == entt::null) continue;
        if (lap.well < 0 || lap.well >= static_cast<int>(wells.wells.size())) continue;
        const GravityWell::Well& well = wells.wells[static_cast<std::size_t>(lap.well)];
        const Effects::Spawn flash = Effects::At(m_effectRules.roundabout, well.atPx);
        m_effectQueue.push_back(effectAdd(flash));
        latch("roundabout");
        m_wellSpins[static_cast<std::size_t>(lap.well)] = WellSpin{0.0, lap.degrees < 0.0 ? -1.0 : 1.0};
        SUPERSONIC_LOG_INFO("Magic Portals") << "lap of " << lap.degrees << " degrees round " << well.name
                                             << " on tick " << m_ticks << ": " << flash.entity << " at ("
                                             << flash.atPx.x << ", " << flash.atPx.y << ") z " << flash.z
                                             << " bare" << std::endl;
    }

    // WHAT THE OTHER TRACK'S STEPS 96 TO 99 ADD, after the rest (the merge of the two
    // tracks, planning step 112): a destroyed minion's blood comes from ITS OWN
    // callback, after the one that destroyed it has added its own - a shock
    // diamond's shock_death and shock_strike above (ETHCallback_shock_diamond
    // #542-555, then ETHCallback_minion #1478-1504) - and the order two callbacks
    // add their entities in shows only in the effects' stream (00_order K8).
    //
    // THE KNIGHTS' MARKS (planning step 158, G1a): what ETHCallback_minion's sight and search arms added this
    // tick, in the order they came. An exclamation is the first frame of a sighting once its timer has passed 4000,
    // with playEnemySpottedSound (#751-754); a question is the search arm's, with playMinionQuestionMarkSound only if
    // its point is on screen (isPointInScreen of the mark's point, #1408-1436) and the knight's way had been clear
    // since its last (the sim's `sound`). Each is a particle-only entity at the knight's point less (0, 36), 32 behind
    // its depth (effects.json exclamation, question).
    if (!m_level.minions.marks.empty()) {
        matchMinions();
        for (const Minions::Mark& mark : m_level.minions.marks) {
            const double knightZ = mark.minion < m_minions.size() ? m_minions[mark.minion].lookZ
                                                                  : m_artRules.LevelDepth(m_artRules.minion.z);
            const bool question = mark.kind == Minions::Mark::Kind::Question;
            const Effects::Spawn spawn = question ? Effects::QuestionMark(m_effectRules, mark.entityPx, knightZ)
                                                  : Effects::ExclamationMark(m_effectRules, mark.entityPx, knightZ);
            m_effectQueue.push_back(effectAdd(spawn));
            bool heard = mark.sound;
            if (question && heard) {
                // isPointInScreen (utilIntersections 318801..319089): the point less the camera's corner is not
                // past a side of the screen, the edges inside.
                const glm::dvec2 at = spawn.atPx - cameraCornerPx();
                const glm::dvec2 view = ViewPx();
                heard = !(at.x < 0.0 || at.y < 0.0 || at.x > view.x || at.y > view.y);
            }
            if (heard) latch(question ? "minion_question" : "enemy_spotted");
            SUPERSONIC_LOG_INFO("Magic Portals") << "knight " << m_level.minions.minions[mark.minion].name
                                                 << (question ? " asks" : " spots") << " on tick " << m_ticks << ": "
                                                 << spawn.entity << " at (" << spawn.atPx.x << ", " << spawn.atPx.y
                                                 << ") z " << spawn.z << (heard ? ", heard" : ", silent") << std::endl;
        }
    }

    // A MINION THE TICK DESTROYED (the visuals plan's 11.3), in the order it went
    // (Minions::State::deaths). A killer floor's contact callback adds a skull at
    // the view's foot under it before anything else, and the minion's own callback
    // then finds itself destroyed and adds blood.ent at its point and deletes
    // itself (ETHCallback_minion #1478-1504); a strike does only the second. The
    // original's skull comes on the contact and the blood on the minion's next
    // update, a frame apart; here both come from the one tick, skull first.
    const std::vector<Minions::Death>& died = m_level.minions.deaths;
    if (!died.empty()) {
        const glm::dvec2 cameraPx = cameraCornerPx();
        // Each minion's own depth - a marker's at the raw 2 minion_spawn adds it
        // at, an escort at its anchor's - is the one matchMinions reads.
        matchMinions();
        for (const Minions::Death& death : died) {
            if (death.byKiller) {
                m_effectQueue.push_back(effectAdd(Effects::Skull(
                    m_effectRules, Effects::KillerSkullPoint(m_effectRules, death.atPx.x, cameraPx, ViewPx()))));
            }
            const double minionZ =
                death.minion < m_minions.size() ? m_minions[death.minion].lookZ
                                                : m_artRules.LevelDepth(m_artRules.minion.z);
            // BURNED: its own callback's addFlames before its blood (ETHCallback_minion
            // #1456-1471, planning step 146) - fire32.ent on its point and depth at its
            // frame's width over 64, and left there, as its target is deleted on the
            // same update.
            if (death.byFire) {
                const Effects::Spawn flames = Effects::Flames(m_effectRules, death.atPx, minionZ,
                                                              static_cast<double>(m_minionCut.frameUnits.x));
                m_effectQueue.push_back(effectAdd(flames));
                SUPERSONIC_LOG_INFO("Magic Portals") << "minion burned on tick " << m_ticks << ": " << flames.entity
                                                     << " at (" << flames.atPx.x << ", " << flames.atPx.y << ") z "
                                                     << flames.z << " x " << flames.scale << std::endl;
            }
            m_effectQueue.push_back(effectAdd(Effects::Blood(m_effectRules, death.atPx, minionZ)));
        }
    }

    // CHAPTER 3'S BOSS (the visuals plan's 12.2, planning step 98). Its init block
    // - the tick after the knight's death added it (R4) - adds ghost_appear_effect.ent
    // at its point as it was added and a bare 1 over its own depth; that entity's
    // own callback chases the ghost in (followUp) and grows its dark puffs (Scale).
    const Ghost::State& ghost = m_level.ghost;
    if (ghost.present && ghost.initialised && !m_ghostAppearQueued) {
        EffectSpawn add = effectAdd(Effects::GhostAppear(m_effectRules, ghost.spawnPx, m_ghost.lookZ));
        add.followUp = m_effectRules.ghostAppearFollow;
        add.followUpTarget.kind = EffectOwner::Kind::Ghost;
        add.scalePerFrame = m_effectRules.ghostAppearScalePerFrame;
        m_effectQueue.push_back(add);
        m_ghostAppearQueued = true;
    }
    // ITS SHOTS. addEnemyShoot adds the shot and the portal_mark.ent where it is
    // going, and the shot's own first call adds projectile_shoot.ent where the nudge
    // left it; the port nudges it as it is fired, so all three come on the tick it
    // appears. A LIST WALK, as the fireballs' is: each once, by the sim's id.
    const double shotZ =
        m_effectRules.enemyShoot.zScaled || !(m_artRules.scaleFactor > 0.0)
            ? m_effectRules.enemyShoot.z
            : m_artRules.LevelDepth(m_effectRules.enemyShoot.z);
    std::vector<int> flyingShots;
    flyingShots.reserve(ghost.shots.size());
    for (const Ghost::Shot& shot : ghost.shots) {
        flyingShots.push_back(shot.id);
        if (std::find(m_ghostShotEffects.begin(), m_ghostShotEffects.end(), shot.id) != m_ghostShotEffects.end()) {
            continue;
        }
        EffectSpawn add = effectAdd(Effects::EnemyShoot(m_effectRules, shot.atPx, shot.directionPx));
        add.follow.kind = EffectOwner::Kind::GhostShot;
        add.follow.index = shot.id;
        m_effectQueue.push_back(add);
        // Its mark is the player's shot's own rule: addEnemyShoot #127-156 adds
        // portal_mark.ent with the arguments addProjectile gives it, at finalPos -
        // the player's point as it was fired (effects.json portal_mark, one row for
        // both callers since planning step 112).
        m_effectQueue.push_back(effectAdd(Effects::At(m_effectRules.portalMark, shot.destinyPx)));
        m_effectQueue.push_back(effectAdd(Effects::EnemyShootFlash(m_effectRules, shot.atPx, shotZ)));
        m_ghostShotEffects.push_back(shot.id);
    }
    m_ghostShotEffects.erase(std::remove_if(m_ghostShotEffects.begin(), m_ghostShotEffects.end(),
                                            [&flyingShots](int id) {
                                                return std::find(flyingShots.begin(), flyingShots.end(), id) ==
                                                       flyingShots.end();
                                            }),
                             m_ghostShotEffects.end());
    // And where one was destroyed, in its range or on the player:
    // portal_fail_in_antiportal.ent at its point, a bare 0.
    for (const glm::dvec2& atPx : ghost.spentPx) {
        m_effectQueue.push_back(effectAdd(Effects::EnemyShootBurst(m_effectRules, atPx)));
    }
    // THE KNIGHTS' SHOTS (planning step 159): the same three entities addEnemyShoot and the shot's first call add, once each
    // by the sim's id - the shot, the portal_mark where it is going (the sighted entity's point as it was fired) and the
    // projectile_shoot flash where the nudge left it - and the same burst where one ends.
    std::vector<int> flyingMinionShots;
    flyingMinionShots.reserve(m_level.minions.shots.size());
    for (const Minions::EnemyShot& shot : m_level.minions.shots) {
        flyingMinionShots.push_back(shot.id);
        if (std::find(m_minionShotEffects.begin(), m_minionShotEffects.end(), shot.id) != m_minionShotEffects.end()) {
            continue;
        }
        EffectSpawn add = effectAdd(Effects::EnemyShoot(m_effectRules, shot.atPx, shot.directionPx));
        add.follow.kind = EffectOwner::Kind::MinionShot;
        add.follow.index = shot.id;
        m_effectQueue.push_back(add);
        m_effectQueue.push_back(effectAdd(Effects::At(m_effectRules.portalMark, shot.destinyPx)));
        m_effectQueue.push_back(effectAdd(Effects::EnemyShootFlash(m_effectRules, shot.atPx, shotZ)));
        m_minionShotEffects.push_back(shot.id);
    }
    m_minionShotEffects.erase(std::remove_if(m_minionShotEffects.begin(), m_minionShotEffects.end(),
                                             [&flyingMinionShots](int id) {
                                                 return std::find(flyingMinionShots.begin(), flyingMinionShots.end(),
                                                                  id) == flyingMinionShots.end();
                                             }),
                              m_minionShotEffects.end());
    for (const glm::dvec2& atPx : m_level.minions.spentPx) {
        m_effectQueue.push_back(effectAdd(Effects::EnemyShootBurst(m_effectRules, atPx)));
    }
    // CHAPTER 4'S BOSS (the visuals plan's 12.1, planning step 99): the
    // dark_dragon.ent the summon adds (AddScaledEntity at the spawn, getScale()),
    // drawn whole by the runtime - dragon_boss.png cut 2 x 2 on its pivot, lit
    // through dragon_normal.png, and its mouth's explosion.JPG system - and carried
    // by the sim's point, turned, framed and lit by its Look every frame
    // (updateEffects). At the z 0 its hover's SetPosition keeps it at from its
    // second update (darkdragon.json look). Dynamic, with a callback and a body.
    const DarkDragon::State& darkDragon = m_level.darkDragon;
    if (darkDragon.Alive() && !m_darkDragonQueued && m_artReady) {
        EffectSpawn add;
        add.entity = darkDragon.rules.entityName;
        add.atPx = darkDragon.atPx;
        add.zUnits = darkDragon.rules.zUnits;
        add.angleDeg = darkDragon.look.angleDeg;
        add.follow.kind = EffectOwner::Kind::DarkDragon;
        add.alwaysActive = true;
        m_effectQueue.push_back(add);
        m_darkDragonQueued = true;
    }
    // THE BOSSES' OWN PICTURES (planning step 146): what each dying arm scatters and
    // its death's explode, and what a boss puts back - before the key a boss drops,
    // as the ghost's DEATH arm adds its explosion before its key.
    latchBossPictures(step);
    // THE KEY A BOSS DROPS, which no node pictures: the key.ent the ghost's DEATH arm
    // adds at its own vector3 (AddScaledEntity('key.ent', GetPosition(), getScale()),
    // #1354-1368), and the dark dragon's DYING arm at key_pos's (ins 1851-1878),
    // drawn by the runtime - key.png and its two sparkle systems - where the carry
    // leaves it, bobbing while it lies (ETHCallback_key's linearMotion, the level
    // keys' row, from a start angle on the effects' stream).
    for (std::size_t k = 0; k < m_level.keys.keys.size() && m_artReady; ++k) {
        const int index = static_cast<int>(k);
        const std::string& keyName = m_level.keys.keys[k].name;
        const bool ghosts = ghost.present && keyName == ghost.keyName;
        const bool dragons = darkDragon.present && keyName == darkDragon.keyName;
        if (!ghosts && !dragons) continue;
        const bool known = std::any_of(m_droppedKeys.begin(), m_droppedKeys.end(),
                                       [index](const DroppedKey& dropped) { return dropped.key == index; });
        if (known) continue;
        DroppedKey dropped;
        dropped.key = index;
        const Motion::Row& row = m_motionRules.key;
        dropped.bob = Motion::Start(row, row.startTo > row.startFrom ? effectRandom(row.startFrom, row.startTo)
                                                                      : row.startFrom);
        m_droppedKeys.push_back(dropped);
        EffectSpawn add;
        add.entity = ghosts ? ghost.rules.keyEntityName : darkDragon.rules.keyEntity;
        add.atPx = m_level.keys.keys[k].atPx;
        add.zUnits = ghosts ? m_ghost.lookZ : darkDragon.keyZUnits;
        add.follow.kind = EffectOwner::Kind::Key;
        add.follow.index = index;
        m_effectQueue.push_back(add);
    }
    // And the dark dragon's last waypoint after its key: ghost_utility_spawn beside
    // it, then explode (planning step 146).
    latchDarkDragonDeath();
}

glm::dvec2 MagicPortalsLayer::cameraCornerPx() const {
    // The follow keeps the view's CENTRE (Camera.hpp); Ethanon's camera is its
    // top-left corner.
    return m_follow.centrePx - ViewPx() / 2.0;
}

void MagicPortalsLayer::latchFinishEffects(entt::registry& registry) {
    // checkGameEnd #34-47 on the frame the character and the final door collide:
    // levelFinishedEffect(character.GetPositionXY(), finalDoor.GetPositionXY()) -
    // the suck at the door's own point, 16 below it, then the sparkles at the
    // character. The door is the one SeekEntity finds (m_doorSprite), at its
    // ENTITY point as the last frame left it, bob and all; the character is its
    // body, which the doorway's Hide leaves standing where it went in.
    if (m_doorSprite >= 0 && m_doorSprite < static_cast<int>(m_sprites.size())) {
        const glm::dvec2 doorPx = m_sprites[static_cast<std::size_t>(m_doorSprite)].ownerPx;
        m_effectQueue.push_back(effectAdd(Effects::FinishSuck(m_effectRules, doorPx)));
    }
    if (m_level.player != entt::null && registry.valid(m_level.player)) {
        const glm::dvec2 characterPx =
            Units::ToPixels(registry.get<Supersonic::TransformComponent>(m_level.player).position);
        m_effectQueue.push_back(effectAdd(Effects::FinishSparkles(m_effectRules, characterPx)));
    }
}

void MagicPortalsLayer::latchDeathEffects(entt::registry& registry) {
    if (m_level.player == entt::null || !registry.valid(m_level.player)) return;
    const glm::dvec2 characterPx =
        Units::ToPixels(registry.get<Supersonic::TransformComponent>(m_level.player).position);
    if (m_level.hazards.diedByFalling) {
        // checkGameLost's fall branch (#117-389): the skull held inside the view.
        m_effectQueue.push_back(effectAdd(Effects::Skull(
            m_effectRules,
            Effects::FallSkullPoint(m_effectRules, characterPx, cameraCornerPx(), ViewPx(), m_level.noGravity))));
        return;
    }
    // A DEATH BY FIRE (planning step 146): addFlames(character) before the hp
    // branch below - the fireball's contact (ETHBeginContactCallback_fireball
    // #123-145) or a fire agent's flame (ETHCallback_fire_agent #222-251) - at the
    // character's point and depth, at its frame's width over 64, and riding its
    // body from then on (EffectOwner::Kind::Character).
    const bool byFireball = m_level.turrets.playerKilled && m_level.hazards.killedBy == m_level.turrets.killedBy;
    const bool byFlame = m_level.fire.playerKilled && m_level.hazards.killedBy == m_level.fire.killedBy;
    if (byFireball || byFlame) {
        EffectSpawn flames =
            effectAdd(Effects::Flames(m_effectRules, characterPx, m_playerZ, playerCellPx(m_level.noGravity).x));
        flames.follow.kind = EffectOwner::Kind::Character;
        m_effectQueue.push_back(flames);
        SUPERSONIC_LOG_INFO("Magic Portals") << "player burned by " << m_level.hazards.killedBy << " on tick "
                                             << m_ticks << ": " << flames.entity << " at (" << characterPx.x << ", "
                                             << characterPx.y << ") z " << flames.zUnits << " x " << flames.scale
                                             << std::endl;
    }
    // And its hp branch (#3-63), which every other death the port has goes through:
    // blood at the character's whole vector3, its depth the character's own.
    m_effectQueue.push_back(effectAdd(Effects::Blood(m_effectRules, characterPx, m_playerZ)));
}

// ---- THE SMALLER PICTURES (the program's UI, planning step 146) -----------------
//
// effects.json _bosses_flames_hands. Each queued on the tick that decides it, from
// what the simulation left and what the last latch saw (m_bossPictures), and added
// on the frame as every effect is. The sound's watch reads the same arms for its
// boss_explosion and its deaths, on its own clock from sounds.json's stride.
void MagicPortalsLayer::latchBossPictures(const ScriptClock::Step& step) {
    const double tickMs = ScriptClock::Ms(step);
    BossPictures& seen = m_bossPictures;
    // ONE UPDATE OF AN ARM THAT CALLS overTimeEntityAdder: its two draws, made
    // whatever the clock says (randF runs before the call), from the effects'
    // stream (00_order K8); and the add only on the update the clock reaches the
    // stride.
    const auto scatter = [this, tickMs](const Effects::Scatter& rule, double& clockMs, const glm::dvec2& centrePx,
                                        const char* who) {
        const double direction = effectRandom(0.0, rule.directionSpanRad);
        const double length = effectRandom(0.0, rule.lengthSpan);
        if (!Effects::AdderDue(rule, clockMs, tickMs)) return;
        const Effects::Spawn spawn = Effects::ScatterAt(rule, centrePx, direction, length);
        m_effectQueue.push_back(effectAdd(spawn));
        SUPERSONIC_LOG_INFO("Magic Portals") << who << " scatters an explosion on tick " << m_ticks << ": "
                                             << spawn.entity << " at (" << spawn.atPx.x << ", " << spawn.atPx.y
                                             << ") from (" << centrePx.x << ", " << centrePx.y << ")" << std::endl;
    };
    // explode()'s explosion_large at the caller's superScale (explode_super_scale).
    const auto explode = [this](const glm::dvec2& atPx, double superScale, const char* who) {
        const Effects::Spawn spawn = Effects::Explode(m_effectRules, atPx, superScale);
        m_effectQueue.push_back(effectAdd(spawn));
        SUPERSONIC_LOG_INFO("Magic Portals") << who << " explodes on tick " << m_ticks << ": " << spawn.entity
                                             << " at (" << spawn.atPx.x << ", " << spawn.atPx.y << ") z " << spawn.z
                                             << " x " << spawn.scale << std::endl;
    };
    // addEntityAtSpawnPoint's effect, or the AddScaledEntity of it, on a marker's
    // own point and depth.
    const auto utility = [this](const glm::dvec2& atPx, double z, const std::string& why) {
        const Effects::Spawn spawn = Effects::OnEntity(m_effectRules.ghostUtilitySpawn, atPx, z);
        m_effectQueue.push_back(effectAdd(spawn));
        SUPERSONIC_LOG_INFO("Magic Portals") << why << " on tick " << m_ticks << ": " << spawn.entity << " at ("
                                             << spawn.atPx.x << ", " << spawn.atPx.y << ") z " << spawn.z << std::endl;
    };

    // 1-32'S BEHOLDER. DEAD's every update calls the adder (#1163-1253), the last
    // included - each tick that ends dead and began with it there - and the last
    // explodes it at 2 (#1257-1274), at its point: it does not move while dead.
    if (m_level.boss.beholder.has_value()) {
        const Boss::Beholder& beholder = *m_level.boss.beholder;
        if (seen.valid && beholder.phase == Boss::Phase::Dead && !seen.beholderGone) {
            scatter(m_effectRules.scatterBeholder, seen.beholderMs, beholder.atPx, "the beholder");
        }
        if (beholder.gone && !seen.beholderGone) explode(beholder.atPx, m_effectRules.explodeBeholder, "the beholder");
        seen.beholderGone = beholder.gone;
    }

    // 3-32'S GHOST. The arm a tick runs is the phase the last one left: DAMAGE calls
    // the adder while its hp is above 0 - at 0 it goes on to DEATH with no call - and
    // DEATH on every update, the last included (#786-929, #1245-1331). DEATH's end
    // explodes it at 2 where it stands (#1335-1352), before its key.
    const Ghost::State& ghost = m_level.ghost;
    if (seen.valid && seen.ghostSpawned && !seen.ghostGone &&
        (seen.ghostPhase == Ghost::Phase::Death ||
         (seen.ghostPhase == Ghost::Phase::Damage && ghost.phase != Ghost::Phase::Death))) {
        scatter(m_effectRules.scatterGhost, seen.ghostMs, ghost.atPx, "the ghost");
    }
    if (ghost.present && ghost.gone && !seen.ghostGone) explode(ghost.atPx, m_effectRules.explodeGhost, "the ghost");

    // WHAT A BOSS PUTS BACK, ghost_utility_spawn where addEntityAtSpawnPoint adds
    // it, before the entity: each barrel a restock added (the dark dragon's after a
    // shot, the ghost's on SHOOTING's beat, both at barrel_spawn), a fire diamond the
    // ghost put back (at fire_diamond_spawn: SHOOTING's beat after its barrel, and
    // RAGE_MODE's every update), and the escort RAGE_MODE calls after that, at
    // minion_spawn_point (#1096-1123).
    const std::vector<Restock::Added>& added = m_level.restock.added;
    for (std::size_t i = seen.restocksSeen; i < added.size(); ++i) {
        utility(added[i].atPx, added[i].z, "barrel " + added[i].name + " put back");
    }
    seen.restocksSeen = added.size();
    const std::vector<Diamonds::Diamond>& diamonds = m_level.diamonds.diamonds;
    if (seen.fireDiamondsGone.size() < diamonds.size()) seen.fireDiamondsGone.resize(diamonds.size(), 0);
    for (std::size_t i = 0; i < diamonds.size(); ++i) {
        const Diamonds::Diamond& diamond = diamonds[i];
        if (ghost.present && diamond.fire && seen.fireDiamondsGone[i] != 0 && !diamond.gone) {
            utility(ghost.diamondSpawnPx, markerDepth(ghost.rules.diamondSpawnName),
                    "fire diamond " + diamond.name + " put back");
        }
        seen.fireDiamondsGone[i] = diamond.fire && diamond.gone ? char{1} : char{0};
    }
    if (ghost.present) {
        for (int n = seen.summonsSeen; n < ghost.summons; ++n) {
            utility(ghost.summonPx, markerDepth(ghost.rules.summonAnchorName), "the ghost calls an escort");
        }
        seen.summonsSeen = ghost.summons;
    }

    // 4-32'S DARK DRAGON. DAMAGE's and DYING's every update call
    // darkDragonOvertimeSmallExplosionAdder (#1563, #1818), off its own point.
    const DarkDragon::State& dark = m_level.darkDragon;
    if (seen.valid && seen.darkAlive &&
        (seen.darkPhase == DarkDragon::Phase::Damage || seen.darkPhase == DarkDragon::Phase::Dying)) {
        scatter(m_effectRules.scatterDarkDragon, seen.darkMs, dark.atPx, "the dark dragon");
    }

    seen.ghostSpawned = ghost.present && ghost.spawned;
    seen.ghostGone = ghost.gone;
    seen.ghostPhase = ghost.phase;
    seen.darkAlive = dark.Alive();
    seen.darkPhase = dark.phase;
    seen.valid = true;
}

void MagicPortalsLayer::latchDarkDragonDeath() {
    // DYING's last waypoint (#1851-1931): key.ent at key_pos - queued just before
    // this - then ghost_utility_spawn at key_pos's own GetPosition(), the platform,
    // and explode at its own point at 1.
    const DarkDragon::State& dark = m_level.darkDragon;
    BossPictures& seen = m_bossPictures;
    if (dark.present && dark.gone && !seen.darkGone) {
        for (const Keys::Key& key : m_level.keys.keys) {
            if (key.name != dark.keyName) continue;
            const Effects::Spawn spawn = Effects::OnEntity(m_effectRules.ghostUtilitySpawn, key.atPx, dark.keyZUnits);
            m_effectQueue.push_back(effectAdd(spawn));
            SUPERSONIC_LOG_INFO("Magic Portals") << "the dark dragon's key dropped on tick " << m_ticks << ": "
                                                 << spawn.entity << " at (" << spawn.atPx.x << ", " << spawn.atPx.y
                                                 << ") z " << spawn.z << std::endl;
            break;
        }
        const Effects::Spawn burst = Effects::Explode(m_effectRules, dark.atPx, m_effectRules.explodeDarkDragon);
        m_effectQueue.push_back(effectAdd(burst));
        SUPERSONIC_LOG_INFO("Magic Portals") << "the dark dragon explodes on tick " << m_ticks << ": " << burst.entity
                                             << " at (" << burst.atPx.x << ", " << burst.atPx.y << ") z " << burst.z
                                             << " x " << burst.scale << std::endl;
    }
    seen.darkGone = dark.gone;
}

void MagicPortalsLayer::tapHandsAway(entt::registry& registry, const Portals::TouchRelease& release) {
    // ScreenPad::isValidPoint (#48-58): a release on a walk pad takes no hand.
    if (onWalkControl(release.atView)) return;
    // getReleasePos(t) + GetCameraPos(): the last point it was held at, through the
    // camera the shot reads the same release through.
    glm::dvec2 releasePx(0.0);
    if (!ViewportToLevelPx(registry, glm::vec2(release.atPx), releasePx)) return;
    for (DrawnSprite& drawn : m_sprites) {
        if (drawn.placed < 0 || drawn.quad == entt::null) continue;
        PlacedScript& script = m_placed[static_cast<std::size_t>(drawn.placed)];
        if (script.tapped || m_motionRules.placed[script.row].entity != m_effectRules.handTapEntity) continue;
        // Its own GetPositionXY(): where its swing last put it.
        if (!Effects::HandTapped(m_effectRules, releasePx, drawn.ownerPx)) continue;
        script.tapped = true;
        const Effects::Spawn smoke =
            Effects::HandTapSmoke(m_effectRules, drawn.ownerPx, drawn.lookZ, drawn.ownerAngleDeg);
        m_effectQueue.push_back(effectAdd(smoke));
        SUPERSONIC_LOG_INFO("Magic Portals") << "hand " << drawn.sprite.node << " tapped away on tick " << m_ticks
                                             << " by a release at (" << releasePx.x << ", " << releasePx.y
                                             << "): " << smoke.entity << " at (" << smoke.atPx.x << ", "
                                             << smoke.atPx.y << ") z " << smoke.z << std::endl;
    }
}

double MagicPortalsLayer::markerDepth(const std::string& entity) const {
    for (const Tscn::Node& node : m_data.scene.nodes) {
        if (node.parent != "." || Roles::EntityName(node) != entity) continue;
        const Tscn::Value* z = node.Meta("eth_z");
        double read = 0.0;
        return z != nullptr && z->AsNumber(read) ? read : 0.0;
    }
    return 0.0;
}

MagicPortalsLayer::DrawnPictureReport MagicPortalsLayer::DrawnPicture(const std::string& node) const {
    DrawnPictureReport report;
    for (const DrawnSprite& drawn : m_sprites) {
        if (drawn.sprite.node != node) continue;
        report.drawn = drawn.quad != entt::null;
        report.quad = drawn.quad;
        report.centrePx = drawn.centrePx;
        report.sizePx = drawn.drawnSizePx;
        report.alpha = drawn.colour.a * drawn.fade;
        report.ownerPx = drawn.ownerPx;
        report.ownerAngleDeg = drawn.ownerAngleDeg;
        report.lookZ = drawn.lookZ;
        break;
    }
    return report;
}

// ---- THE WIN CUTSCENES (the program's P2, planning step 123) ----------------------
//
// cutscenes.json holds the decode; sim/Cutscene the arithmetic. From the tick the
// level ends, once a tick and behind the finished screen as well (the level goes
// on running under it, spec D7):
//   - rocket_launch and rocket_landing while the level is WON, from the door's own
//     tick - both are static entities, whose callbacks run after the game state
//     that decided it - each playing its track on its first update;
//   - dragon_knight_spawn once it is FINISHED, a death as well as the door, on the
//     same tick; the knight it adds updates from the next, a dynamic entity's
//     callback, and goes into the door as the player does: levelFinishedEffect's
//     suck and sparkles and the door's sound.
void MagicPortalsLayer::stepCutscenes(entt::registry& registry, double frameMs) {
    if (m_finishing && m_launchPlaced) {
        const Cutscene::LaunchStep step = Cutscene::StepLaunch(m_cutsceneRules, m_launch, frameMs);
        if (step.started) {
            m_launchStartedTick = m_ticks;
            musicPlay(m_cutsceneRules.launch.track.c_str());
            SUPERSONIC_LOG_INFO("Magic Portals") << "rocket_launch from tick " << m_ticks << std::endl;
        }
        if (step.quake) {
            ++m_launchQuakes;
            QueueQuake(Earthquake::Call::RocketLaunch);
        }
    }
    if (m_finishing && m_landingPlaced) {
        const Cutscene::LandingStep step = Cutscene::StepLanding(m_cutsceneRules, m_landing, frameMs);
        if (step.started) {
            m_landingStartedTick = m_ticks;
            musicPlay(m_cutsceneRules.landing.track.c_str());
            SUPERSONIC_LOG_INFO("Magic Portals") << "rocket_landing from tick " << m_ticks << std::endl;
        }
    }
    if (!m_knight.placed) return;
    if (!m_knight.spawned) {
        if (Cutscene::SpawnKnight(m_cutsceneRules, m_data.minions, registry, m_knight) != entt::null) {
            m_knightSpawnedTick = m_ticks;
            SUPERSONIC_LOG_INFO("Magic Portals") << "dragon_knight added at " << m_knight.spawnPx.x << ","
                                                 << m_knight.spawnPx.y << " on tick " << m_ticks << std::endl;
        }
        return;
    }
    if (m_knight.gone) return;
    Cutscene::StepKnight(m_cutsceneRules, m_data.minions, registry, m_knight, m_step);
    if (!m_knight.entered) return;
    // levelFinishedEffect(its point, its point + scale(16, -16)): the suck 16 below
    // that door point, the sparkles at the knight, playFinalDoorSound.
    m_knightEnteredTick = m_ticks;
    m_knightEnteredAtPx = m_knight.enteredAtPx;
    m_effectQueue.push_back(effectAdd(
        Effects::FinishSuck(m_effectRules, m_knight.enteredAtPx + m_cutsceneRules.knight.finishOffsetPx)));
    m_effectQueue.push_back(effectAdd(Effects::FinishSparkles(m_effectRules, m_knight.enteredAtPx)));
    latch("level_finished");
    SUPERSONIC_LOG_INFO("Magic Portals") << "dragon_knight went in at " << m_knight.enteredAtPx.x << ","
                                         << m_knight.enteredAtPx.y << " on tick " << m_ticks << std::endl;
}

void MagicPortalsLayer::syncKnight(entt::registry& registry) {
    using namespace Supersonic;
    const bool standing = m_knight.spawned && !m_knight.gone && m_knight.body != entt::null &&
                          registry.valid(m_knight.body) && registry.all_of<TransformComponent>(m_knight.body);
    if (!standing || !m_artReady || m_minionSheet.empty()) {
        if (m_knightQuad != entt::null && registry.valid(m_knightQuad)) registry.destroy(m_knightQuad);
        m_knightQuad = entt::null;
        return;
    }
    if (m_knightQuad == entt::null) {
        m_knightQuad = makeSprite(registry, "Magic Portals Knight", m_minionSheet, m_artRules.minion.additive);
        // It falls and walks on the tick and is drawn at its place, as a minion.
        registry.emplace<InterpolatedTransformComponent>(m_knightQuad);
    }
    // minion.png's sheet as the minions', on its own pivot (0, 0): the picture's
    // centre is its entity point.
    const glm::dvec2 entityPx = Cutscene::KnightEntityPx(m_data.minions, registry, m_knight);
    m_knightCentrePx = entityPx - m_cutsceneRules.knight.pivotPx;
    placeSprite(registry, m_knightQuad, m_knightCentrePx, glm::dvec2(m_minionCut.frameUnits), m_knightZ, 0.0f);
    auto& material = registry.get<MaterialComponent>(m_knightQuad);
    material.uvScale = glm::vec2(m_minionCut.uvScale);
    material.uvOffset = glm::vec2(Tiers::UvOffset(m_minionCut, m_knight.column, m_knight.row));
    // dragon_knight.ent's 24 black_portal_particle.png at alphaMode 4, added with
    // it: queued once, carried by its picture (Kind::Knight), at the spawn's depth.
    if (!m_knightAuraQueued) {
        EffectSpawn add;
        add.entity = m_cutsceneRules.knight.entity;
        add.atPx = entityPx;
        add.zUnits = 0.0;
        add.follow.kind = EffectOwner::Kind::Knight;
        add.picture = false;
        add.alwaysActive = true;
        m_effectQueue.push_back(add);
        m_knightAuraQueued = true;
    }
}

glm::dvec2 MagicPortalsLayer::doorPointPx() const {
    if (m_doorSprite < 0 || m_doorSprite >= static_cast<int>(m_sprites.size())) return glm::dvec2(0.0);
    const DrawnSprite& door = m_sprites[static_cast<std::size_t>(m_doorSprite)];
    glm::dvec2 atPx = door.sprite.atPx;
    if (door.doorBob) atPx += Motion::OffsetPx(m_doorBob);
    if (m_launch.running) atPx += m_launch.pose.doorOffsetPx;
    if (m_landing.running) atPx += Cutscene::LandingDoorOffsetPx(m_cutsceneRules, m_landing.bias);
    return atPx;
}

MagicPortalsLayer::CutsceneReport MagicPortalsLayer::CutscenePicture(entt::registry& registry) const {
    CutsceneReport report;
    report.wonDelayMs = m_wonDelayMs;
    report.launchPlaced = m_launchPlaced;
    report.landingPlaced = m_landingPlaced;
    report.launch = m_launch;
    report.landing = m_landing;
    report.launchStartedTick = m_launchStartedTick;
    report.landingStartedTick = m_landingStartedTick;
    report.quakes = m_launchQuakes;
    const auto at = [this](int index) -> const DrawnSprite* {
        return index >= 0 && index < static_cast<int>(m_sprites.size()) ? &m_sprites[static_cast<std::size_t>(index)]
                                                                          : nullptr;
    };
    if (const DrawnSprite* door = at(m_doorSprite); door != nullptr) {
        report.doorAtPx = door->ownerPx;
        report.doorCentrePx = door->centrePx;
    }
    if (const DrawnSprite* bridge = at(m_bridgeSprite); bridge != nullptr) {
        report.bridgeQuad = bridge->quad;
        report.bridgeCentrePx = bridge->centrePx;
        if (bridge->body != entt::null && registry.valid(bridge->body)) {
            report.bridgeRestPx = Units::ToPixels(registry.get<Supersonic::TransformComponent>(bridge->body).position);
        }
    }
    report.keyScale = m_landing.running && at(m_landingKeySprite) != nullptr ? m_landing.keyScale : 1.0;
    for (const DrawnSprite& drawn : m_sprites) {
        if (drawn.space == SpaceRole::Eclipse) report.eclipseAtPx = drawn.ownerPx;
        if (drawn.space == SpaceRole::Spiral) report.spiralColour = drawn.colour;
    }
    report.cameraTargetPx = m_cameraTargetPx;
    report.knight = m_knight;
    report.knightAtPx = Cutscene::KnightEntityPx(m_data.minions, registry, m_knight);
    report.knightQuad = m_knightQuad;
    report.knightCentrePx = m_knightCentrePx;
    report.knightAuraQueued = m_knightAuraQueued;
    report.knightSpawnedTick = m_knightSpawnedTick;
    report.knightEnteredTick = m_knightEnteredTick;
    report.knightEnteredAtPx = m_knightEnteredAtPx;
    return report;
}


// CHAPTER 4'S TORCHES, THEIR SIGNAL AND THEIR WALL (the visuals plan's 11.2;
// effects.json _torches_trail_roundabout; sim/Torch). Per torch, on the tick its
// state turns over:
//
//   LIT (ETHCallback_light_off, the frame a shot or a fireball comes within 24):
//   light_from_projectile - the lit torch, its flame, its light and its halo - and
//   light_on_sparkles, both on the torch's own point and depth; portal_death where
//   the spent shot was; and, where the level has a light wall, fire_signal, riding
//   the signal - and then the lit torch bounces. The level's own picture is taken
//   on the NEXT tick, where the original's `delete` branch deletes the entity.
//
//   PUT OUT (ETHCallback_fire_signal, when the signal is shot): a fresh
//   light_off.ent and light_off_sparkles at vector3(originalPos, 10);
//   portal_death at the shot; extinguish_fire (the visuals plan's 10.4); and
//   addLightWall's light_wall_create and the new light_wall at the ghost - its
//   point and angle, and its depth, the wall's less 4 raw pixels - the wall faded
//   in from 0.
//
// And the wall, 1500 ms after a lighting (ETHCallback_light_wall): wall_destroy at
// the wall that stands - the level's, or the one addLightWall built - turned by it.
void MagicPortalsLayer::latchTorches() {
    const auto pictureOf = [this](auto&& is) {
        const auto drawn = std::find_if(m_sprites.begin(), m_sprites.end(), is);
        return drawn == m_sprites.end() ? nullptr : &*drawn;
    };
    const auto logged = [this](const char* what, const Effects::Spawn& spawn) {
        SUPERSONIC_LOG_INFO("Magic Portals") << what << " on tick " << m_ticks << ": " << spawn.entity << " at ("
                                             << spawn.atPx.x << ", " << spawn.atPx.y << ") z " << spawn.z
                                             << (spawn.zScaled ? "" : " bare") << " angle " << spawn.angleDeg
                                             << std::endl;
    };
    const Torch::State& torch = m_level.torch;
    if (m_torchWatch.size() != torch.lights.size()) {
        TorchWatch fresh;
        fresh.spin.degPerS = m_artRules.unlitTorch.spinDegPerS;
        m_torchWatch.assign(torch.lights.size(), fresh);
        for (std::size_t i = 0; i < torch.lights.size(); ++i) {
            const DrawnSprite* picture = pictureOf(
                [&torch, i](const DrawnSprite& drawn) { return drawn.sprite.node == torch.lights[i].name; });
            m_torchWatch[i].z = picture != nullptr ? picture->lookZ : 0.0;
        }
    }
    // Where the light wall standing now is: its node's point and angle, and its
    // depth - the node's eth_z, or the ghost's once addLightWall has built it there.
    glm::dvec2 wallPx(0.0);
    double wallZ = 0.0;
    double wallAngleDeg = 0.0;
    bool wallDrawn = false;
    if (torch.wall.present) {
        const std::string& name = torch.wall.name;
        if (const DrawnSprite* wall = pictureOf([&name](const DrawnSprite& drawn) { return drawn.sprite.node == name; })) {
            wallDrawn = true;
            wallPx = wall->sprite.atPx;
            wallAngleDeg = -wall->sprite.rotation * 180.0 / 3.14159265358979323846;
            wallZ = m_lightWallRebuilt ? m_wallGhost.lookZ : wall->lookZ;
        }
    }

    for (std::size_t i = 0; i < torch.lights.size(); ++i) {
        const Torch::Light& light = torch.lights[i];
        TorchWatch& watch = m_torchWatch[i];
        if (light.lit && !watch.lit) {
            EffectSpawn lit = effectAdd(Effects::OnEntity(m_effectRules.lightFromProjectile, light.atPx, watch.z));
            lit.follow.kind = EffectOwner::Kind::LitTorch;
            lit.follow.index = static_cast<int>(i);
            // addFireSignalIfNecessary sets its `bounce` where it adds a signal:
            // this torch's, added this tick.
            const bool signalled = torch.signal.present && torch.signal.fromTorch == light.name && !m_signalSeen;
            if (signalled) {
                const Effects::Bounce& bounce = m_effectRules.litTorchBounce;
                lit.bounces = true;
                lit.bounce.fromScale = glm::dvec2(bounce.from);
                lit.bounce.toScale = glm::dvec2(bounce.to);
                lit.bounce.strideMs = bounce.strideMs;
            }
            m_effectQueue.push_back(lit);
            const Effects::Spawn sparkles = Effects::OnEntity(m_effectRules.lightOnSparkles, light.atPx, watch.z);
            m_effectQueue.push_back(effectAdd(sparkles));
            logged(("torch " + light.name + " lit").c_str(), sparkles);
            if (torch.spentAtPx.has_value()) {
                const Effects::Spawn burst = Effects::At(m_effectRules.portalDeathAtLitTorch, *torch.spentAtPx);
                m_effectQueue.push_back(effectAdd(burst));
                logged("the shot that lit it spent", burst);
            }
            if (signalled) {
                EffectSpawn signal = effectAdd(Effects::OnEntity(m_effectRules.fireSignal, light.atPx, watch.z));
                signal.follow.kind = EffectOwner::Kind::FireSignal;
                m_effectQueue.push_back(signal);
                SUPERSONIC_LOG_INFO("Magic Portals") << "fire signal added on tick " << m_ticks << " at ("
                                                     << light.atPx.x << ", " << light.atPx.y << ") z " << watch.z
                                                     << ", and the lit torch bounces" << std::endl;
            }
        } else if (light.lit && watch.lit && !watch.unlitGone) {
            watch.unlitGone = true;
            SUPERSONIC_LOG_INFO("Magic Portals") << "torch " << light.name << " deletes its unlit self on tick "
                                                 << m_ticks << std::endl;
        } else if (!light.lit && watch.lit) {
            // A FIRE SIGNAL PUT OUT, and with it this torch. ETHCallback_fire_signal's
            // originalPos is its own first point, which is its torch's
            // (addFireSignalIfNecessary); Torch clears the signal on the tick it goes,
            // so its origin is the last latch's.
            const glm::dvec2 originPx = m_signalSeen ? m_signalOriginPx : light.atPx;
            watch.unlitGone = false;
            // A new light_off.ent, added at AddEntity's 0f: its turn starts again.
            watch.spin.turnedDeg = 0.0;
            EffectSpawn back = effectAdd(Effects::At(m_effectRules.lightOffAtSignal, originPx));
            back.follow.kind = EffectOwner::Kind::UnlitTorch;
            back.follow.index = static_cast<int>(i);
            watch.z = back.zUnits;
            m_effectQueue.push_back(back);
            const Effects::Spawn sparkles = Effects::At(m_effectRules.lightOffSparkles, originPx);
            m_effectQueue.push_back(effectAdd(sparkles));
            logged(("torch " + light.name + " put back").c_str(), sparkles);
            if (torch.spentAtPx.has_value()) {
                const Effects::Spawn burst = Effects::At(m_effectRules.portalDeathAtSignal, *torch.spentAtPx);
                m_effectQueue.push_back(effectAdd(burst));
                logged("the shot that put it out spent", burst);
            }
            // extinguish_fire at vector3(originalPos, 10), a depth of its own
            // (#130-146, #258-269; the visuals plan's 10.4).
            if (torch.putOut > m_signalsPutOut && m_signalSeen) {
                const Effects::Spawn water = Effects::At(m_effectRules.extinguishAtSignal, m_signalOriginPx);
                m_effectQueue.push_back(effectAdd(water));
                logged("fire signal put out", water);
            }
            // addLightWall: its smoke, and the wall itself at 0 alpha, both at the
            // ghost - where the wall stood, 4 raw pixels further back.
            if (torch.wall.present && wallDrawn && m_wallGhost.sizePx != glm::dvec2(0.0)) {
                const Effects::Spawn smoke =
                    Effects::OnEntityTurned(m_effectRules.lightWallCreate, wallPx, m_wallGhost.lookZ, wallAngleDeg);
                m_effectQueue.push_back(effectAdd(smoke));
                logged("the light wall comes back", smoke);
                EffectSpawn wall = effectAdd(
                    Effects::OnEntityTurned(m_effectRules.lightWallAtGhost, wallPx, m_wallGhost.lookZ, wallAngleDeg));
                wall.follow.kind = EffectOwner::Kind::LightWall;
                wall.alpha = m_effectRules.lightWallAlphaFrom;
                wall.alphaPerSecond = m_effectRules.lightWallAlphaPerSecond;
                m_effectQueue.push_back(wall);
                m_lightWallRebuilt = true;
            }
        }
        watch.lit = light.lit;
    }
    m_signalsPutOut = torch.putOut;
    m_signalSeen = torch.signal.present;
    if (torch.signal.present) m_signalOriginPx = torch.signal.originPx;

    // THE WALL GOING, 1500 ms after the lighting that flagged it: ETHCallback_light_wall
    // adds wall_destroy at its own point and depth, turned by it.
    if (torch.wallsGone > m_wallsGoneSeen && wallDrawn) {
        const Effects::Spawn dust =
            Effects::OnEntityTurned(m_effectRules.wallDestroyLightWall, wallPx, wallZ, wallAngleDeg);
        m_effectQueue.push_back(effectAdd(dust));
        logged(("light wall " + torch.wall.name + " gone").c_str(), dust);
    }
    m_wallsGoneSeen = torch.wallsGone;
}

bool MagicPortalsLayer::isBreakableWall(const std::string& node) const {
    const Tscn::Node* found = m_data.scene.FindNode(node);
    return found != nullptr && Earthquake::IsEntity(Roles::EntityName(*found), m_quakeRules.wallEntity);
}

void MagicPortalsLayer::queueTapEffects() {
    if (!m_loaded) return;
    const Portals::State& portals = m_level.portals;
    const Portals::Tap& tap = portals.lastTap;
    switch (tap.reason) {
    case Portals::TapReason::Fired: {
        if (!portals.flight) return;
        // What computePortalFinalPos said at the tap, as managePortalInsertion calls it (#127-145) and Shoot
        // stored it: the shot flies by the same answer, so the mark and the shot's end cannot disagree
        // (planning step 156, R23). With a fire diamond carried it is the method's own branch (#21-64), which
        // casts no ray and leaves willDie the leftover low byte of absTouchPos.x.
        const Portals::FinalPos& said = tap.said;
        // red_waves (A) is added inside computePortalFinalPos, so before the
        // mark addProjectile adds after it (#167-243); both at absTouchPos, z
        // scale(32).
        if (said.redWaves) m_effectQueue.push_back(effectAdd(Effects::At(m_effectRules.redWaves, tap.atPx)));
        const Effects::Rule& mark = said.willDie ? m_effectRules.redCross : m_effectRules.portalMark;
        m_effectQueue.push_back(effectAdd(Effects::At(mark, tap.atPx)));
        SUPERSONIC_LOG_INFO("Magic Portals") << "shot fired on tick " << m_ticks << " toward (" << tap.atPx.x << ", "
                                             << tap.atPx.y << "), to (" << said.atPx.x << ", " << said.atPx.y
                                             << "): " << mark.entity
                                             << (said.redWaves ? " and red_waves (A)" : "")
                                             << (said.retried ? ", after the head-height retry" : "")
                                             << (m_level.diamonds.carrierHasFire ? ", a fire diamond's" : "")
                                             << std::endl;
        return;
    }
    case Portals::TapReason::InFlight:
        // managePortalInsertion #181-210: past the 400 ms, a shot still flying. The tap restarted the 400 ms
        // clock on its way here (#75-78; Shoot, planning step 156).
        m_effectQueue.push_back(effectAdd(Effects::At(m_effectRules.hourglass, tap.atPx)));
        SUPERSONIC_LOG_INFO("Magic Portals") << "tap refused on tick " << m_ticks << " at (" << tap.atPx.x << ", "
                                             << tap.atPx.y << "): a shot still flies, hourglass" << std::endl;
        return;
    case Portals::TapReason::NoBudget:
        // blockPortalEffects #80-133, only while the sign is there: the waves and
        // startEarthquake(7, 40), whose vibration the desktop port has no motor for.
        if (!portals.noPortalSign) return;
        m_effectQueue.push_back(effectAdd(Effects::At(m_effectRules.redWaves, tap.atPx)));
        QueueQuake(Earthquake::Call::NoPortalTap);
        SUPERSONIC_LOG_INFO("Magic Portals") << "tap refused on tick " << m_ticks << " at (" << tap.atPx.x << ", "
                                             << tap.atPx.y << "): no portal here, red_waves (B)" << std::endl;
        return;
    case Portals::TapReason::NoShooter:
    case Portals::TapReason::TooEarly:
    case Portals::TapReason::Cooldown:
        // Silent in the original: nothing added, nothing played.
        return;
    }
}

// ---- THE EARTHQUAKE -------------------------------------------------------------
//
// EarthquakeGenerator (sim/Earthquake.hpp): every startEarthquake call whose event
// the simulation has, landed on its side of one update a tick, and the camera the
// frame draws moved by what that update gave. earthquake.json has the decode of
// the generator and of every row; D11 and its check are where it came from.
//
// WHERE A CALL LANDS. The original's frame runs the physics step and its contact
// callbacks, then the dynamic entities' callbacks, then the controllers - the
// camera, the portal manager, THE GENERATOR, the sky, the game state - and last
// the static entities' callbacks. So a call from before the generator trembles
// the frame it is made (when one is due), and one from after it the next frame at
// the earliest; and a call between a forth and its back is halved before it is
// shown. The port's tick is that frame: its events are read after the step, each
// is queued with its row's side, and quakeTick runs the update between the sides.
//
// WHAT MOVES. The camera the frame is drawn from, after the engine has drawn it
// between ticks - so the move is the whole tick's and never smeared - and the sky
// and satellite, which StaticSky places from the camera the generator has moved,
// and so hold still on the screen while the level moves under them. The HUD is
// the screen's and never moves. The camera the TICK reads stays at rest, so a tap
// lands where it would without the shake, as the original's portal taps do.

bool MagicPortalsLayer::levelShown() const {
    // Where the original's game state runs its controllers: a level, under its
    // pause and its popups, and under the finished and lost screens it keeps
    // running behind. Not a menu: that is another state, which has no generator.
    return m_loaded && (m_screen == Screen::None || m_screen == Screen::Finished || m_screen == Screen::Dead);
}

void MagicPortalsLayer::QueueQuake(Earthquake::Call call, double scale) {
    m_quakeCalls.push_back(QuakeCall{call, scale});
    // Said with the tick, so a capture's shake can be traced to its event.
    SUPERSONIC_LOG_INFO("Magic Portals") << "earthquake " << Earthquake::KeyOf(call) << " at "
                                         << m_quakeRules.Of(call).intensity * scale << " on tick " << m_ticks
                                         << std::endl;
}

void MagicPortalsLayer::latchSimQuakes() {
    if (!m_loaded) return;
    using Earthquake::Call;
    const Game::Level& level = m_level;

    QuakeWatch now;
    now.valid = true;
    now.fell = level.hazards.playerDied && level.hazards.diedByFalling;
    now.blasts = level.fire.blasts;
    if (level.boss.beholder.has_value()) {
        now.beholderGone = level.boss.beholder->gone;
        now.beholderHits = level.boss.beholder->hits;
    }
    now.rocksBroken = level.boss.rocksBroken;
    now.darkDragonGone = level.darkDragon.gone;
    now.darkDragonFired = level.darkDragon.fired;
    now.darkDragonPhase = level.darkDragon.phase;
    now.ghostGone = level.ghost.gone;
    // A ghost's shot ends by striking or by reaching its range, and either way
    // leaves the list it was fired into.
    now.ghostShotsEnded = level.ghost.shotsFired - static_cast<int>(level.ghost.shots.size());
    now.minionShotsEnded = level.minions.shotsEnded;
    now.breakablesBroken.reserve(level.demolish.breakables.size());
    for (const Demolish::Breakable& breakable : level.demolish.breakables) {
        now.breakablesBroken.push_back(breakable.broken ? char{1} : char{0});
    }
    now.despawned = level.launchers.despawned;
    now.minionsKilled = level.minions.killed;
    now.doorsUnlocked = level.keys.opened;
    now.diamondsHeld.reserve(level.diamonds.diamonds.size());
    for (const Diamonds::Diamond& diamond : level.diamonds.diamonds) {
        now.diamondsHeld.push_back(!diamond.gone && diamond.owner != entt::null ? char{1} : char{0});
    }
    now.struck = level.diamonds.struck;
    now.wallsGone = level.torch.wallsGone;

    // The first latch of a level sets the mark, as the sound watch's does.
    if (!m_quakeWatch.valid) {
        m_quakeWatch = std::move(now);
        return;
    }
    const QuakeWatch& was = m_quakeWatch;

    // Which breakables this tick took, and of those the breakable_walls: the one
    // entity both wall rows are callbacks of.
    std::vector<const Demolish::Breakable*> wallsBroken;
    std::vector<const Demolish::Breakable*> struck; // by a stone's touch
    if (now.breakablesBroken.size() == was.breakablesBroken.size()) {
        for (std::size_t i = 0; i < now.breakablesBroken.size(); ++i) {
            if (now.breakablesBroken[i] == 0 || was.breakablesBroken[i] != 0) continue;
            const Demolish::Breakable& breakable = level.demolish.breakables[i];
            if (level.demolish.FindStone(breakable.brokenBy) != nullptr) struck.push_back(&breakable);
            if (isBreakableWall(breakable.name)) wallsBroken.push_back(&breakable);
        }
    }

    // BEFORE THE UPDATE, in the frame's order: the begin-contact callbacks inside
    // the physics step first. The wall's before the stone's own in one contact
    // (Box2D makes the polygon fixture A, and A's callback runs first: D11_check
    // C59), and a stone's own before what the same callback calls after it - a
    // destroyier's 6, a beholder rock's 5 - so the last word is the original's.
    for (const Demolish::Breakable* wall : wallsBroken) {
        // By a stone's touch, which is the wall's contact callback; a script's
        // destroy() (the dark dragon's summon) sets the same datum with no contact.
        if (level.demolish.FindStone(wall->brokenBy) != nullptr) QueueQuake(Call::WallStruck);
    }
    latchImpacts(struck);
    for (int i = was.despawned; i < now.despawned; ++i) QueueQuake(Call::StoneDespawned);
    for (int i = was.rocksBroken; i < now.rocksBroken; ++i) QueueQuake(Call::BossRockBroken);
    for (int i = was.minionsKilled; i < now.minionsKilled; ++i) QueueQuake(Call::MinionKilled);
    // ...then the dynamic entities' own callbacks.
    for (std::size_t i = 0; i < level.turrets.spentPx.size(); ++i) QueueQuake(Call::FireballEnd);
    for (int i = was.blasts; i < now.blasts; ++i) QueueQuake(Call::BarrelBlast);
    for (int i = was.beholderHits; i < now.beholderHits; ++i) QueueQuake(Call::BeholderHurt);
    if (now.beholderGone && !was.beholderGone) QueueQuake(Call::BeholderDeath);
    if (level.darkDragon.Alive() && level.darkDragon.phase == DarkDragon::Phase::Appearing &&
        Earthquake::InMouthLeg(m_quakeRules, level.darkDragon.path.frame, level.darkDragon.path.Bias())) {
        // Every tick of the leg, re-armed whole each time.
        QueueQuake(Call::DarkDragonMouth);
    }
    // A shot while it was FIGHTING is one at the player. The DAMAGE arm's last
    // waypoint shoots the torch as it goes back to fighting, and that has none.
    if (now.darkDragonFired > was.darkDragonFired && was.darkDragonPhase == DarkDragon::Phase::Fighting) {
        QueueQuake(Call::DarkDragonShot);
    }
    if (now.darkDragonGone && !was.darkDragonGone) QueueQuake(Call::DarkDragonDeath);
    for (int i = was.ghostShotsEnded; i < now.ghostShotsEnded; ++i) QueueQuake(Call::EnemyShotEnd);
    for (int i = was.minionShotsEnded; i < now.minionShotsEnded; ++i) QueueQuake(Call::EnemyShotEnd);
    // A doomed shot's arrival inside a field (ETHCallback_projectile #211-215; a
    // dynamic entity's). Tick's own record, which is this tick's alone.
    for (const Portals::Failure& ended : level.portals.failures) {
        if (ended.kind != Portals::FailKind::Blocker && ended.inField) QueueQuake(Call::ShotFailedInField);
    }

    // AFTER IT: the game-state controller, then the static entities' callbacks.
    if (now.fell && !was.fell) QueueQuake(level.noGravity ? Call::FallNoGravity : Call::Fall);
    for (int i = was.doorsUnlocked; i < now.doorsUnlocked; ++i) QueueQuake(Call::DoorUnlocked);
    if (now.diamondsHeld.size() == was.diamondsHeld.size()) {
        for (std::size_t i = 0; i < now.diamondsHeld.size(); ++i) {
            if (now.diamondsHeld[i] == 0 || was.diamondsHeld[i] != 0) continue;
            QueueQuake(level.diamonds.diamonds[i].fire ? Call::FireDiamondPicked : Call::ShockDiamondPicked);
        }
    }
    for (int i = was.struck; i < now.struck; ++i) QueueQuake(Call::ShockStrike);
    for (int i = was.wallsGone; i < now.wallsGone; ++i) QueueQuake(Call::LightWallGone);
    // A shot an anti_projectile_wall killed (its static callback, #70-72).
    for (const Portals::Failure& ended : level.portals.failures) {
        if (ended.kind == Portals::FailKind::Blocker) QueueQuake(Call::ShotBlocked);
    }
    // A shot an anti-portal agent killed (its static callback, #179-181; step 126).
    for (std::size_t i = 0; i < level.portals.agentKills.size(); ++i) QueueQuake(Call::AgentKilled);
    for (const Demolish::Breakable* wall : wallsBroken) {
        // destroyEffectScale, where the node sets it above 0 (#16-31); else 1.
        double scale = 1.0;
        if (const Tscn::Node* node = m_data.scene.FindNode(wall->name)) {
            double datum = 0.0;
            const Tscn::Value* value = node->Meta(m_quakeRules.wallScaleKey);
            if (value != nullptr && value->AsNumber(datum) && datum > 0.0) scale = datum;
        }
        QueueQuake(Call::WallDestroyed, scale);
    }
    if (now.ghostGone && !was.ghostGone) QueueQuake(Call::GhostDeath);

    m_quakeWatch = std::move(now);
}

void MagicPortalsLayer::latchDoors() {
    if (!m_loaded) return;
    DoorWatch now;
    now.valid = true;
    for (const Puzzle::SwitchedDoor& door : m_level.channels.doors) {
        now.opening.push_back(door.motion.opening ? char{1} : char{0});
        now.progress.push_back(door.motion.progress);
        now.landings.push_back(door.motion.landings);
    }
    // ETHCallback_door_lift's door_lift_particles as a door starts to open (#434)
    // or to close (#580), each just before its startEarthquake(4, 0): at the
    // door's GetPosition() where the frame BEGAN - SetPositionXY comes at #624 -
    // which is where the last tick left it, not where this tick's BeforeStep has
    // already moved it, less or plus half its own drawn height (GetSize().y, the
    // picture's units times its entity scale), on its eth_z. A door starts a leg
    // only from shut or from fully raised (Mover::Door, planning step 133), so
    // this is the shut door's top edge or the raised door's bottom edge. A door
    // with no picture (a level drawn as boxes) adds none.
    const auto dust = [this](std::size_t i, bool opening, float fromProgress) {
        const Puzzle::SwitchedDoor& door = m_level.channels.doors[i];
        const auto drawn = std::find_if(m_sprites.begin(), m_sprites.end(),
                                        [&door](const DrawnSprite& s) { return s.sprite.node == door.name; });
        if (drawn == m_sprites.end()) return;
        const glm::dvec2 doorPx = Units::ToPixels(glm::mix(door.motion.closed, door.motion.open, fromProgress));
        const Effects::Spawn spawn = Effects::DoorLiftDust(m_effectRules, opening, doorPx, drawn->lookZ,
                                                           drawn->sprite.sizePx.y * drawn->scale,
                                                           m_artRules.scaleFactor);
        m_effectQueue.push_back(effectAdd(spawn));
        SUPERSONIC_LOG_INFO("Magic Portals") << "door " << door.name << (opening ? " opening" : " closing")
                                             << " on tick " << m_ticks << " from (" << doorPx.x << ", " << doorPx.y
                                             << "): " << spawn.entity << " at (" << spawn.atPx.x << ", "
                                             << spawn.atPx.y << ") z " << spawn.z << std::endl;
    };
    if (m_doorWatch.valid && now.opening.size() == m_doorWatch.opening.size()) {
        for (std::size_t i = 0; i < now.opening.size(); ++i) {
            // Back shut from a fall, which the callback sees on the frame after the
            // door gets there (#282-315), and before it can start to rise again on
            // that same frame (#330-440).
            for (int n = m_doorWatch.landings[i]; n < now.landings[i]; ++n) QueueQuake(Earthquake::Call::DoorLanded);
            if (now.opening[i] != m_doorWatch.opening[i]) {
                dust(i, now.opening[i] != 0, m_doorWatch.progress[i]);
                QueueQuake(now.opening[i] != 0 ? Earthquake::Call::DoorOpening : Earthquake::Call::DoorClosing);
            }
        }
    }
    m_doorWatch = std::move(now);
}

std::string MagicPortalsLayer::EntityNameOf(entt::entity body) const {
    for (const auto& [name, entity] : m_level.built.entities) {
        if (entity != body) continue;
        const Tscn::Node* node = m_data.scene.FindNode(name);
        return node != nullptr ? Roles::EntityName(*node) : std::string();
    }
    return {};
}

std::vector<Impacts::Watched> MagicPortalsLayer::impactBodies() const {
    using Earthquake::Call;
    std::vector<Impacts::Watched> bodies;
    if (!m_loaded) return bodies;
    const auto entityOf = [this](Call call) -> const std::string& {
        return m_quakeRules.entity[static_cast<std::size_t>(call)];
    };
    // EVERY body's velocity is carried at SimRatio times the original's (ScriptClock.hpp, planning step 152), and
    // every callback below reads hitIntensity of the original's: so a begin reports it by 1 / SimRatio of the
    // body's own, for the player, a stone, a minion and a crate alike.
    const double velocityScale = 1.0 / ScriptClock::SimRatio(m_step);
    const auto watch = [&bodies, velocityScale](Impacts::Watched watched) {
        watched.velocityScale = velocityScale;
        bodies.push_back(watched);
    };
    // ETHBeginContactCallback_dark_mage: the player's body.
    if (m_level.player != entt::null) watch(Impacts::Watched{m_level.player, Impacts::Kind::Player, {}});
    // ETHBeginContactCallback_rolling_stone: every rolling stone - placed, or
    // thrown, or the beholder's rock (both rolling_stone.ent, launchers.json and
    // boss.json). A placed stone of another entity has no such callback.
    for (const Demolish::Stone& stone : m_level.demolish.stones) {
        const Tscn::Node* node = m_data.scene.FindNode(stone.name);
        if (node != nullptr && !Earthquake::IsEntity(Roles::EntityName(*node), entityOf(Call::StoneImpact))) continue;
        // Read as Box2D's step reads a free fall (planning step 162, STK-9): a stone is the one body of the four
        // that nothing but the engine's gravity and its contacts moves.
        Impacts::Watched watched{stone.body, Impacts::Kind::Stone, {}};
        watched.freeFlight = true;
        watch(watched);
    }
    // ETHBeginContactCallback_minion: minion.ent's, which is every minion but a
    // ghost_minion.ent. Aimed from its entity point, which stands above its circle.
    //
    // And made where the original's is not. spawnMinion adds minion.ent with its
    // ENTITY POINT on the marker (ETHCallback_minion_spawn #12-19, #51; spawnMinion
    // #25-39), its feet circle's bottom entity_above_feet_px under it; the port
    // centres its one circle on the marker (Minions::State::Spawn), so its body
    // stands EntityAboveCentrePx higher. 51 of the 62 markers put the original's
    // feet into the floor under them (Q.2 spawn_census): a new body at rest, its
    // pair begun on its first step (b2World::Step's FindNewContacts, then Collide)
    // with hitIntensity 0, which the callback's `> 0` (#124-125) never calls on -
    // where the port's knight falls onto that floor. So the watch looks at a new
    // minion where the original's stands until its own circle comes down.
    const glm::dvec2 minionPoint(0.0, -m_level.minions.rules.EntityAboveCentrePx());
    const glm::dvec2 minionDrop(0.0, m_level.minions.rules.EntityAboveCentrePx());
    for (const Minions::Minion& minion : m_level.minions.minions) {
        if (minion.gone) continue;
        // The entity the marker ADDS (R16, planning step 97): minion.ent unless it names
        // another, and never empty - so level31b's ghost_minion.ent, which has no
        // ETHBeginContactCallback_ of its own, stays out (the merge, planning step 112).
        if (!Earthquake::IsEntity(minion.entity, entityOf(Call::MinionImpact))) continue;
        watch(Impacts::Watched{minion.body, Impacts::Kind::Minion, minionPoint, minionDrop});
    }
    // crateContactCallback's six callers: every crate, whose begins are a sound
    // and no shake (planning step 142). Each pair a watched body touches is its
    // own, so watching a crate changes nothing about the player's or a stone's.
    for (const entt::entity crate : m_crates) watch(Impacts::Watched{crate, Impacts::Kind::Crate, {}});
    return bodies;
}

void MagicPortalsLayer::latchImpacts(const std::vector<const Demolish::Breakable*>& struck) {
    using Earthquake::Call;
    // A stone Demolish found touching a breakable, which it then took away with
    // its body before the watch could look: the stone's own callback in that
    // same contact, read from what the stone carried into the step and the
    // breakable's node - kept with the watch's begins, after them.
    for (const Demolish::Breakable* breakable : struck) {
        const Demolish::Stone* stone = m_level.demolish.FindStone(breakable->brokenBy);
        if (stone == nullptr) continue;
        const bool seen = std::any_of(m_impactBegins.begin(), m_impactBegins.end(), [&](const Impacts::Begin& b) {
            return b.body == stone->body && b.other == breakable->body;
        });
        const Tscn::Node* node = m_data.scene.FindNode(breakable->name);
        const Tscn::Value* position = node != nullptr ? node->Find("position") : nullptr;
        Impacts::Begin begin;
        // Only a stone the watch keeps - a rolling_stone - has the callback.
        if (seen || !m_impacts.Carried(stone->body, begin) || position == nullptr ||
            position->kind != Tscn::Value::Kind::Vector2) {
            continue;
        }
        begin.other = breakable->body;
        begin.normal = glm::dvec2(0.0);
        begin.otherAtPx = glm::dvec2(position->numbers[0], position->numbers[1]);
        m_impactBegins.push_back(begin);
    }
    // A beholder rock Boss::State::Contacts broke on something static before the
    // step brought the two into touch (it breaks one within its margin of a
    // static, or where the step would have taken it): the rock's own callback in
    // that contact, whose first arm runs before destroyOnStaticHit's (#58-71, then
    // #170-208), read from what the rock carried into the step. Not where the
    // watch has a begin of the rock this tick, nor where it has the pair touching
    // already - met on an earlier step, so its begin is past (planning step 142's
    // second fix round).
    for (const Boss::StaticHit& hit : m_level.boss.staticHitsNow) {
        const bool begun = std::any_of(m_impactBegins.begin(), m_impactBegins.end(),
                                       [&](const Impacts::Begin& b) { return b.body == hit.rock; });
        Impacts::Begin begin;
        if (begun || m_impacts.Touching(hit.rock, hit.other) || !m_impacts.Carried(hit.rock, begin)) continue;
        begin.other = hit.other;
        begin.normal = glm::dvec2(0.0);
        begin.otherAtPx = hit.otherAtPx;
        m_impactBegins.push_back(begin);
    }
    // Whose begin it is, for the log: the player, or the stone's or the minion's
    // own node name (a minion answers to its marker's).
    const auto nameOf = [this](const Impacts::Begin& begin) -> std::string {
        if (begin.kind == Impacts::Kind::Player) return "the player";
        for (const Demolish::Stone& stone : m_level.demolish.stones) {
            if (stone.body == begin.body) return stone.name;
        }
        // A destroyable stone the wall it broke has taken this tick (planning step 175): its begin is still this tick's.
        for (const Demolish::Taken& taken : m_level.demolish.takenNow) {
            if (taken.stone.body == begin.body) return taken.stone.name;
        }
        for (const Minions::Minion& minion : m_level.minions.minions) {
            if (minion.body == begin.body) return minion.name;
        }
        return "a body";
    };
    // Each begin, turned into its row's call where the row calls on it - and its
    // callback's sound, on a gate of its own (sounds.json impacts; planning step
    // 142).
    const Sounds::Impacts& sounds = m_soundRules.impacts;
    for (const Impacts::Begin& begin : m_impactBegins) {
        double volume = 0.0;
        if (begin.kind == Impacts::Kind::Crate) {
            // crateContactCallback: its own velocity along the contact's normal,
            // taken absolute (#6-29) - no line between entity points - and the
            // drag against the character, the knock against anything else.
            const double hit = Earthquake::HitIntensity(m_quakeRules, begin.Approaching(begin.normal), begin.normal);
            if (Sounds::ImpactVolume(sounds.crate, hit, volume)) {
                latch(begin.other == m_level.player ? sounds.crate.characterEvent : sounds.crate.event, 0.0, volume);
            }
            continue;
        }
        Call call = Call::PlayerImpact;
        glm::dvec2 along = begin.normal;
        if (begin.kind != Impacts::Kind::Player) {
            // The stone's and the minion's callbacks overwrite the normal with the
            // line between the two entity points (D11_check C55).
            call = begin.kind == Impacts::Kind::Stone ? Call::StoneImpact : Call::MinionImpact;
            const glm::dvec2 apart = begin.otherAtPx - begin.atPx;
            const double length = glm::length(apart);
            along = length > 0.0 ? apart / length : glm::dvec2(0.0);
        }
        const std::string other = EntityNameOf(begin.other);
        // What the callback reads along that line (Begin::Approaching): a stone's
        // free flight, as Box2D's own step would have read it (planning step 162);
        // any other body at what it came along that line fastest since it last met
        // or left anything, because the engine's solver brakes a closing pair
        // before it meets (Impacts::kRecentSteps).
        const double hit = Earthquake::HitIntensity(m_quakeRules, begin.Approaching(along), along);
        const bool calls = Earthquake::ImpactCalls(m_quakeRules, call, hit, other);
        SUPERSONIC_LOG_INFO("Magic Portals") << "impact " << Earthquake::KeyOf(call) << " of " << nameOf(begin)
                                             << " on " << (other.empty() ? std::string("a body") : other) << " hit " << hit
                                             << (calls ? "" : ", no call") << " on tick " << m_ticks << std::endl;
        if (begin.freeFlight) {
            // What the history alone would have read (planning step 162): the replay's reading is the one above.
            SUPERSONIC_LOG_INFO("Magic Portals") << "free flight read of " << nameOf(begin) << " on "
                                                 << (other.empty() ? std::string("a body") : other) << ": the history's "
                                                 << Earthquake::HitIntensity(m_quakeRules, begin.HistoryApproaching(along), along)
                                                 << ", replayed " << begin.replayedSteps << " step(s) past the brake, on tick "
                                                 << m_ticks << std::endl;
        }
        if (calls) QueueQuake(call, hit);
        // The same callback's sound, after its shake: the player's on no crate
        // test (#82-90), the minion's past the same enemy_killer test (#132-140),
        // the stone's on the same gate (#58-71).
        const Sounds::Impact& sound = begin.kind == Impacts::Kind::Player  ? sounds.player
                                      : begin.kind == Impacts::Kind::Stone ? sounds.stone
                                                                           : sounds.minion;
        if (std::find(sound.spares.begin(), sound.spares.end(), other) == sound.spares.end() &&
            Sounds::ImpactVolume(sound, hit, volume)) {
            latch(sound.event, 0.0, volume);
        }
        // ETHBeginContactCallback_rolling_stone #131-168, after its own row above: the knight it crushed is destroyed
        // with playMinionSmashSound and startEarthquake(25, 0) (stoneHits took it).
        if (begin.kind == Impacts::Kind::Stone &&
            std::find(m_crushedNow.begin(), m_crushedNow.end(), begin.other) != m_crushedNow.end()) {
            QueueQuake(Call::MinionCrushed);
            latch("minion_smashed");
        }
    }
}

void MagicPortalsLayer::stoneHits(entt::registry& registry) {
    using Earthquake::Call;
    m_crushedNow.clear();
    for (const Impacts::Begin& begin : m_impactBegins) {
        if (begin.kind != Impacts::Kind::Stone) continue;
        const bool thePlayer = m_level.player != entt::null && begin.other == m_level.player;
        const bool aKnight = !thePlayer && std::any_of(m_level.minions.minions.begin(), m_level.minions.minions.end(),
                                                       [&begin](const Minions::Minion& minion) {
                                                           return !minion.gone && minion.body == begin.other;
                                                       });
        if (!thePlayer && !aKnight) continue;
        if (aKnight && std::find(m_crushedNow.begin(), m_crushedNow.end(), begin.other) != m_crushedNow.end()) continue;
        // The stone's callback overwrites the contact normal with the line between the two entity points (D11_check
        // C55) and reads hitIntensity along it: the number its own impact row is called on.
        const glm::dvec2 apart = begin.otherAtPx - begin.atPx;
        const double length = glm::length(apart);
        const glm::dvec2 along = length > 0.0 ? apart / length : glm::dvec2(0.0);
        const double hit = Earthquake::HitIntensity(m_quakeRules, begin.Approaching(along), along);
        if (thePlayer) {
            // #107-129: isCharacter(other) - the player alone - and hitIntensity above 2.5 set his hp to 0. His death is
            // a death area's (Hazards), the stone's shake and hit sound the row's own, latchImpacts'.
            if (Game::StoneHitsPlayer(m_level, begin.body, hit)) {
                SUPERSONIC_LOG_INFO("Magic Portals") << "stone kill of the player by " << m_level.demolish.killedBy << " at hit "
                                                     << hit << " on tick " << m_ticks << std::endl;
            }
            continue;
        }
        if (Minions::Crushes(m_level.minions.rules, hit)) m_crushedNow.push_back(begin.other);
    }
    if (m_crushedNow.empty()) return;
    SUPERSONIC_LOG_INFO("Magic Portals") << "crush of " << m_crushedNow.size() << " knight(s) by a stone on tick " << m_ticks
                                         << std::endl;
    Game::Crush(registry, m_level, m_crushedNow);
}

void MagicPortalsLayer::quakeTick() {
    if (!levelShown()) {
        // A menu is another state: the generator waits for the next level, and
        // nothing is drawn.
        m_quakeCalls.clear();
        m_quakeOffsetUnits = 0.0;
        m_quakeFrameUnits = 0.0;
        m_quakeFramePending = false;
        return;
    }
    latchDoors();
    for (const QuakeCall& made : m_quakeCalls) {
        const Earthquake::CallRule& rule = m_quakeRules.Of(made.call);
        if (rule.when == Earthquake::When::BeforeUpdate) m_quake.Start(m_quakeRules, rule.intensity * made.scale);
    }
    // On the layer's clock, which runs on under a pause as the original's wall
    // clock does (earthquake.json generator._clock).
    const double movePx = m_quake.Update(m_quakeRules, m_layerClockMs);
    m_quakeOffsetUnits = Earthquake::ToUnits(m_quakeRules, movePx);
    // The original trembles once per frame it draws, so every tremble is drawn.
    // The port trembles on its tick, and a frame may run several: a tremble is
    // kept for the next frame until one has drawn it, and a tick with none
    // clears only what a frame has already drawn.
    if (m_quakeOffsetUnits != 0.0) {
        m_quakeFrameUnits = m_quakeOffsetUnits;
        m_quakeFramePending = true;
    } else if (!m_quakeFramePending) {
        m_quakeFrameUnits = 0.0;
    }
    for (const QuakeCall& made : m_quakeCalls) {
        const Earthquake::CallRule& rule = m_quakeRules.Of(made.call);
        if (rule.when == Earthquake::When::AfterUpdate) m_quake.Start(m_quakeRules, rule.intensity * made.scale);
    }
    m_quakeCalls.clear();
    if (movePx != 0.0) {
        SUPERSONIC_LOG_INFO("Magic Portals") << "earthquake tremble " << movePx << " px (" << m_quakeOffsetUnits
                                             << " units) on tick " << m_ticks << std::endl;
    }
}

float MagicPortalsLayer::quakeWorldDy(double units) {
    // The level's y runs down the screen and the world's up it (Units::ToWorld).
    return Units::ToMetres(-units);
}

void MagicPortalsLayer::liftQuake(entt::registry& registry) {
    using namespace Supersonic;
    // The engine puts back what it interpolates - the tick's pose at the tick's
    // start, the drawn pose at every frame - so only what it does not is put
    // back by hand: everything, when a suite runs the layer alone.
    if (m_quakeDrawn.onCamera && m_camera != entt::null && registry.valid(m_camera)) {
        const auto* interpolated = registry.try_get<InterpolatedCameraComponent>(m_camera);
        if (interpolated == nullptr || !interpolated->captured) {
            registry.get<CameraComponent>(m_camera).position.y = m_quakeDrawn.cameraRestY;
        }
    }
    for (const auto& [quad, restY] : m_quakeDrawn.quads) {
        if (!registry.valid(quad)) continue;
        const auto* interpolated = registry.try_get<InterpolatedTransformComponent>(quad);
        if (interpolated != nullptr && interpolated->captured) continue;
        if (auto* transform = registry.try_get<TransformComponent>(quad)) transform->position.y = restY;
    }
    m_quakeDrawn = QuakeDrawn{};
}

void MagicPortalsLayer::shakeView(entt::registry& registry) {
    using namespace Supersonic;
    liftQuake(registry);
    // This frame draws the held tremble, and that is it drawn: the next tick
    // with none may clear it.
    m_quakeFramePending = false;
    if (m_quakeFrameUnits == 0.0 || !levelShown()) return;
    if (m_camera == entt::null || !registry.valid(m_camera)) return;
    const float dy = quakeWorldDy(m_quakeFrameUnits);
    auto& camera = registry.get<CameraComponent>(m_camera);
    m_quakeDrawn.cameraRestY = camera.position.y;
    camera.position.y += dy;
    m_quakeDrawn.units = m_quakeFrameUnits;
    m_quakeDrawn.onCamera = true;
    // StaticSky places every sky and the satellite from GetCameraPos() AFTER the
    // generator has moved it (Game::preLoop #281-287 against #272-279), so they
    // go with the camera and stay where they were on the screen.
    for (const DrawnSprite& drawn : m_sprites) {
        // SpaceSky's four with them, from the same GetCameraPos: the eclipse's y as
        // well, whose frame adds the shaken corner's y (sim/Sky.hpp) - drawn here, and
        // not kept, as step 122 records.
        if ((drawn.sky < 0 && !drawn.satellite && drawn.space == SpaceRole::None) || drawn.quad == entt::null ||
            !registry.valid(drawn.quad)) {
            continue;
        }
        if (auto* transform = registry.try_get<TransformComponent>(drawn.quad)) {
            m_quakeDrawn.quads.emplace_back(drawn.quad, transform->position.y);
            transform->position.y += dy;
        }
        // A sky's cover goes with its sky.
        if (drawn.cover == entt::null || !registry.valid(drawn.cover)) continue;
        if (auto* transform = registry.try_get<TransformComponent>(drawn.cover)) {
            m_quakeDrawn.quads.emplace_back(drawn.cover, transform->position.y);
            transform->position.y += dy;
        }
    }
}

void MagicPortalsLayer::latchSimSounds(const ScriptClock::Step& step) {
    if (!m_loaded) return;
    const double tickMs = ScriptClock::Ms(step);

    Watch now;
    now.valid = true;
    now.portalsUsed = m_level.portals.portalsUsed;
    now.traversals = m_level.portals.traversals;
    now.shotsFired = m_level.portals.shotsFired;
    now.shotsFailed = m_level.portals.shotsFailed;
    now.reflections = m_level.portals.reflections;
    for (const Goals::Crystal& crystal : m_level.goals.crystals) {
        if (crystal.collected) ++now.crystalsCollected;
        if (crystal.expired) ++now.crystalsExpired;
    }
    for (const Portals::Static& portal : m_level.portals.statics) {
        if (portal.live) ++now.staticsLive;
    }
    now.wallsBroken = m_level.demolish.Broken();
    now.redKeyTaken = m_level.redKey.destroyed;
    for (const Launchers::Launcher& launcher : m_level.launchers.launchers) now.stonesThrown += launcher.thrown;
    // EVERY FIREBALL, whoever made it, as addFireball #87 plays it for each
    // (step 142; a carranca's alone before): summed across the carrancas, the
    // shots a fire diamond or a flame turned, and the two dragons'.
    for (const Turrets::Turret& turret : m_level.turrets.turrets) now.fireballsSpat += turret.fired;
    now.fireballsSpat += m_level.turrets.converted + m_level.dragon.fired + m_level.darkDragon.fired;
    if (m_level.boss.beholder.has_value()) {
        now.bossHits = m_level.boss.beholder->hits;
        now.bossVolleys = m_level.boss.beholder->volleys;
        now.bossFrame = m_level.boss.beholder->frame;
        now.bossGone = m_level.boss.beholder->gone;
        now.bossRocksThrown = m_level.boss.beholder->rocksThrown;
    }
    now.bossRocksBroken = m_level.boss.rocksBroken;
    now.bossButtonRaised = m_level.boss.buttonRaised;
    now.bossSpikeHits = m_level.boss.spikeHits;
    now.blasts = m_level.fire.blasts;
    now.despawned = m_level.launchers.despawned;
    now.minionsKilled = m_level.minions.killed;
    now.minionsBurned = m_level.minions.burned;
    now.doorsOpened = m_level.keys.opened;
    now.struck = m_level.diamonds.struck;
    now.torchesLit = m_level.torch.lit;
    now.torchesPutOut = m_level.torch.putOut;
    now.crushed = m_level.dragon.crushed;
    now.dragonTookOff = m_level.dragon.present && m_level.dragon.tookOff;
    now.ringKilled = m_level.fields.playerKilled;
    now.playerDead = m_level.hazards.playerDied;
    for (const Fire::Burnable& burnable : m_level.fire.burnables) {
        now.burnables.push_back(burnable.burned ? char{1} : char{0});
    }
    for (const Fields::Field& field : m_level.fields.fields) now.fieldsGone.push_back(field.gone ? char{1} : char{0});
    for (const Diamonds::Diamond& diamond : m_level.diamonds.diamonds) {
        now.diamondsHeld.push_back(!diamond.gone && diamond.owner != entt::null ? char{1} : char{0});
        now.diamondsGone.push_back(diamond.gone ? char{1} : char{0});
    }
    for (const Keys::Key& key : m_level.keys.keys) now.keyOwners.push_back(key.owner);
    const Ghost::State& ghost = m_level.ghost;
    now.ghostSpawned = ghost.present && ghost.spawned;
    now.ghostGone = ghost.gone;
    now.ghostPhase = ghost.phase;
    now.ghostBeats = ghost.beats;
    now.ghostSummons = ghost.summons;
    now.ghostHits = ghost.hits;
    now.ghostShotsFired = ghost.shotsFired;
    now.ghostShotsEnded = ghost.shotsFired - static_cast<int>(ghost.shots.size());
    now.minionShotsFired = m_level.minions.shotsFired;
    now.minionShotsEnded = m_level.minions.shotsEnded;
    const DarkDragon::State& dark = m_level.darkDragon;
    now.darkSummoned = dark.present && dark.summoned;
    now.darkAlive = dark.Alive();
    now.darkGone = dark.gone;
    now.darkPhase = dark.phase;
    now.darkHits = dark.hits;

    // THE ONE-SHOTS AND CLOCKS, which run from the level's first tick: each is a
    // test the original makes on every frame, not an edge.
    //
    // A timed crystal's init block adds its dial and plays the alert once, and
    // the port starts every dial on the level's first tick: one latch each there,
    // which the shared 100 ms crystal timer lets the first of through
    // (ETHCallback_crystal #60-82).
    if (!m_watch.valid) {
        for (const Goals::Crystal& crystal : m_level.goals.crystals) {
            if (crystal.timed && !crystal.collected && !crystal.expired) latch("crystal_alert");
        }
    }
    // A key's fly-in, on the clock latchSimEffects keeps for its halo: the frame
    // it builds its WaypointManager plays playDoorUnlockSound (ETHCallback_key
    // #221-550), and its frame 1 playKeyUnlockSound with the halo (#593-635).
    if (m_soundCues.keyFlyIns.size() < m_level.keys.keys.size()) m_soundCues.keyFlyIns.resize(m_level.keys.keys.size());
    for (std::size_t i = 0; i < m_level.keys.keys.size(); ++i) {
        Effects::FlyIn& flyIn = m_soundCues.keyFlyIns[i];
        const bool builtBefore = flyIn.built;
        const bool halo = Effects::AdvanceFlyIn(m_effectRules, flyIn, m_level.keys.keys[i].spent, tickMs);
        if (flyIn.built && !builtBefore) latch("door_unlocked");
        if (halo) latch("key_unlocked");
    }
    // A light wall's switch, once into its fade (ETHCallback_light_wall #57-97;
    // sounds.json cues). A wall put back and flagged again is a new entity, with
    // a new datum.
    if (!m_level.torch.wall.going) {
        m_soundCues.lightSwitched = false;
    } else if (!m_soundCues.lightSwitched && m_level.torch.wall.goingMs > m_soundRules.cues.lightSwitchAfterMs) {
        m_soundCues.lightSwitched = true;
        latch("light_switch");
    }
    latchBossCues(tickMs);
    now.doorsOpening.reserve(m_level.channels.doors.size());
    for (const Puzzle::SwitchedDoor& door : m_level.channels.doors) {
        now.doorsOpening.push_back(door.motion.opening ? char{1} : char{0});
    }
    for (const Mover::Lift& lift : m_level.movers.lifts) now.liftsSent += lift.motion.sends;

    // The first tick of a level sets the mark rather than playing against a
    // zeroed one: otherwise a level that begins with two static portals and a
    // standing door would announce all of it in its opening frame.
    if (!m_watch.valid) {
        // Step 142's own edges are read against the level as it is made - every
        // count at 0, nothing burned, held, taken or gone - and so on its first tick
        // too: 4-29's key, which the player is spawned on, is taken on that tick.
        latchStepSounds(FreshWatch(now), now);
        m_watch = std::move(now);
        return;
    }

    for (int i = m_watch.portalsUsed; i < now.portalsUsed; ++i) latch("portal_placed");
    for (int i = m_watch.traversals; i < now.traversals; ++i) latch("traversal");
    for (int i = m_watch.shotsFired; i < now.shotsFired; ++i) latch("shot_fired");
    for (int i = m_watch.shotsFailed; i < now.shotsFailed; ++i) latch("shot_failed");
    // A shot an agent killed: playExplosionSound(true), from this tick's own
    // record (step 126). Not a failed shot: the agent deletes it, so the
    // projectile's own callback never plays playPortalFailedSound for it.
    for (std::size_t i = 0; i < m_level.portals.agentKills.size(); ++i) latch("agent_killed");
    for (int i = m_watch.reflections; i < now.reflections; ++i) latch("shot_reflected");
    for (int i = m_watch.crystalsCollected; i < now.crystalsCollected; ++i) latch("crystal_collected");
    for (int i = m_watch.crystalsExpired; i < now.crystalsExpired; ++i) latch("crystal_expired");
    // A static portal goes when it is spent, so this count FALLS.
    for (int i = now.staticsLive; i < m_watch.staticsLive; ++i) latch("portal_spent");
    for (int i = m_watch.wallsBroken; i < now.wallsBroken; ++i) latch("wall_broken");
    // playRedKeyGotSound, on the tick 4-19's red key state saw its key taken (#268-270).
    if (now.redKeyTaken && !m_watch.redKeyTaken) {
        latch("red_key_got");
        SUPERSONIC_LOG_INFO("Magic Portals") << "red key state " << m_level.redKey.name << ": "
                                             << m_level.redKey.takenBy << " taken on tick " << m_ticks
                                             << ", the light fades over " << m_level.redKey.rules.fadeMs << " ms"
                                             << std::endl;
    }
    // Silent since step 142: ETHCallback_entity_launcher calls no audio function.
    for (int i = m_watch.stonesThrown; i < now.stonesThrown; ++i) latch("stone_thrown");
    for (int i = m_watch.fireballsSpat; i < now.fireballsSpat; ++i) latch("fireball_spat");
    // 1-32's beholder (sounds.json _note_beholder): a stone rising into it is
    // playBeholderHitSound and playBeholderDamageSound together (#561, #565).
    for (int i = m_watch.bossHits; i < now.bossHits; ++i) {
        latch("boss_struck");
        latch("boss_hurt");
    }
    for (int i = m_watch.bossVolleys; i < now.bossVolleys; ++i) latch("boss_spikes");
    // Silent since step 142: a rock's break is its landing's stone_hit.
    for (int i = m_watch.bossRocksBroken; i < now.bossRocksBroken; ++i) latch("boss_rock_broken");
    if (now.bossFrame == 1 && m_watch.bossFrame != 1) latch("boss_eyes_shut");
    if (now.bossGone && !m_watch.bossGone) latch("boss_dead");
    // Silent since step 142: the button moves with no sound (#1276-1315).
    if (now.bossButtonRaised && !m_watch.bossButtonRaised) latch("boss_button_raised");

    latchStepSounds(m_watch, now);

    // A door that started moving, with its own stride: the hook's speed is
    // 3000 / it, so a slow door sounds slow.
    const std::size_t doors = now.doorsOpening.size() < m_watch.doorsOpening.size()
                                  ? now.doorsOpening.size()
                                  : m_watch.doorsOpening.size();
    for (std::size_t i = 0; i < doors; ++i) {
        if (now.doorsOpening[i] == m_watch.doorsOpening[i]) continue;
        const double strideMs = m_level.channels.doors[i].motion.strideMs;
        latch(now.doorsOpening[i] != 0 ? "door_opened" : "door_closed", strideMs);
    }
    // And a lift each time a touch sends it: ETHCallback_lift #259-262 calls
    // playElevatorSound right after sendLiftToOtherPoint, and nothing on arrival.
    for (int i = m_watch.liftsSent; i < now.liftsSent; ++i) latch("lift_sent");

    m_watch = std::move(now);
}

MagicPortalsLayer::Watch MagicPortalsLayer::FreshWatch(const Watch& now) {
    // The level as it is made: every count 0, every flag clear, every carrier none,
    // each list as long as the tick's.
    Watch fresh;
    fresh.burnables.assign(now.burnables.size(), char{0});
    fresh.fieldsGone.assign(now.fieldsGone.size(), char{0});
    fresh.diamondsHeld.assign(now.diamondsHeld.size(), char{0});
    fresh.diamondsGone.assign(now.diamondsGone.size(), char{0});
    fresh.keyOwners.assign(now.keyOwners.size(), entt::null);
    return fresh;
}

void MagicPortalsLayer::latchStepSounds(const Watch& was, const Watch& now) {
    // 1-32's beholder: each rock it drops (spawnStone #1-3) and each frame a
    // spike stands in the player's box (ETHCallback_beholder_spike #178-185).
    for (int i = was.bossRocksThrown; i < now.bossRocksThrown; ++i) latch("boss_rock_dropped");
    for (int i = was.bossSpikeHits; i < now.bossSpikeHits; ++i) latch("boss_spike_hit");
    // THE REST OF STEP 142'S, each at the moment its call site plays it
    // (sounds.json carries every citation).
    //
    // A stone destroyed on a destroyier: playStoneHitSound(1) (#91-94).
    for (int i = was.despawned; i < now.despawned; ++i) {
        latch(m_soundRules.impacts.stone.destroyierEvent, 0.0, m_soundRules.impacts.stone.destroyierVolume);
    }
    // A bomb's blast: explode #27-31. A fireball destroyed on anything: its
    // small explosion (#153-158), from this tick's own record.
    for (int i = was.blasts; i < now.blasts; ++i) latch("bomb_blast");
    for (std::size_t i = 0; i < m_level.turrets.spentPx.size(); ++i) latch("fireball_burst");
    // A shock agent a fireball destroyed - Fields' only destroyer: the huge one
    // first (#99-118), the burn of a burnable shock_agent.ent (#159-173), and the
    // agent's own strike as it goes (ETHCallback_shock_agent #289-299).
    if (now.fieldsGone.size() == was.fieldsGone.size()) {
        for (std::size_t i = 0; i < now.fieldsGone.size(); ++i) {
            if (now.fieldsGone[i] == 0 || was.fieldsGone[i] != 0) continue;
            latch("fireball_on_shock_agent");
            latch("burned");
            latch("shock_struck");
        }
    }
    // What burns: a burnable set alight by a blast, a flame, the lava or a
    // fireball, once - and a minion a fire took (burn, #16).
    if (now.burnables.size() == was.burnables.size()) {
        for (std::size_t i = 0; i < now.burnables.size(); ++i) {
            if (now.burnables[i] != 0 && was.burnables[i] == 0) latch("burned");
        }
    }
    for (int i = was.minionsBurned; i < now.minionsBurned; ++i) latch("burned");
    // A minion on the killer floor (ETHBeginContactCallback_enemy_killer #22).
    for (int i = was.minionsKilled; i < now.minionsKilled; ++i) latch("minion_fell");
    // A key taken as its free arm takes it (ETHCallback_key #805), the carrier
    // changed as key_pick's rule reads it; and a keyhole gone with its door
    // (ETHCallback_keyhole #235).
    if (now.keyOwners.size() == was.keyOwners.size()) {
        for (std::size_t i = 0; i < now.keyOwners.size(); ++i) {
            if (now.keyOwners[i] != entt::null && now.keyOwners[i] != was.keyOwners[i]) latch("key_picked");
        }
    }
    for (int i = was.doorsOpened; i < now.doorsOpened; ++i) latch("keyhole_vanished");
    // Diamonds (ETHCallback_shock_diamond #180 and #447; ETHCallback_fire_diamond
    // #192 and #455): a shock one taken, a fire one taken - its burn - a strike,
    // and a fire one a gutter drains.
    if (now.diamondsHeld.size() == was.diamondsHeld.size()) {
        for (std::size_t i = 0; i < now.diamondsHeld.size(); ++i) {
            const bool fire = m_level.diamonds.diamonds[i].fire;
            if (now.diamondsHeld[i] != 0 && was.diamondsHeld[i] == 0) latch(fire ? "burned" : "shock_picked");
            if (fire && now.diamondsGone[i] != 0 && was.diamondsGone[i] == 0) latch("fire_diamond_drained");
        }
    }
    for (int i = was.struck; i < now.struck; ++i) latch("shock_struck");
    // A ring's kill (ETHCallback_shock_area #136-151), on the rule its
    // shock_death reads: claimed this tick, of a player alive at the last tick,
    // and the death Hazards records is the ring's.
    if (now.ringKilled && !was.ringKilled && !was.playerDead && now.playerDead &&
        m_level.hazards.killedBy == m_level.fields.killedBy) {
        latch("shock_struck");
    }
    // The torches (ETHCallback_light_off #145, ETHCallback_fire_signal #275).
    for (int i = was.torchesLit; i < now.torchesLit; ++i) latch("light_on");
    for (int i = was.torchesPutOut; i < now.torchesPutOut; ++i) latch("light_off");
    // 2-32's dragon: its take-off, once, and each platform its claw takes.
    if (now.dragonTookOff && !was.dragonTookOff) latch("dragon_took_off");
    for (int i = was.crushed; i < now.crushed; ++i) latch("claw_crushed");
    // The ghost and the dark dragon: the edges of their states.
    latchLateBossSounds(was, now);
}

void MagicPortalsLayer::latchBossCues(double tickMs) {
    const Sounds::Cues& cues = m_soundRules.cues;
    // overTimeEntityAdder's clock for small_explosion_no_light on a boss's own
    // entity, on an update its arm calls it: at the stride or more, one
    // spawnExplosion - playExplosionSound(false) - and the clock back to 0.
    const auto adder = [this, tickMs, &cues](double& clockMs) {
        clockMs += tickMs;
        if (clockMs < cues.bossExplosionStrideMs) return;
        clockMs = 0.0;
        latch("boss_explosion");
    };

    // 1-32'S BEHOLDER. The first time it stands inside the screen grown by
    // scale(48, 32) on each side, once (ETHCallback_beholder #286-360):
    // GetCameraPos() is the view's top-left corner, and isPointInRect takes both
    // edges in.
    if (m_level.boss.beholder.has_value()) {
        const Boss::Beholder& beholder = *m_level.boss.beholder;
        if (!beholder.gone && !m_soundCues.beholderAppeared) {
            const glm::dvec2 view = ViewPx();
            const glm::dvec2 margin(cues.beholderAppearMarginXPx, cues.beholderAppearMarginYPx);
            const glm::dvec2 low = m_follow.centrePx - view * 0.5 - margin;
            const glm::dvec2 high = low + view + margin * 2.0;
            if (beholder.atPx.x >= low.x && beholder.atPx.y >= low.y && beholder.atPx.x <= high.x &&
                beholder.atPx.y <= high.y) {
                m_soundCues.beholderAppeared = true;
                latch("boss_appeared");
            }
        }
        // Its DEAD arm calls the adder on every update (#1163-1253), the last
        // included: each tick that ends dead and began with it there.
        if (m_watch.valid && beholder.phase == Boss::Phase::Dead && !m_watch.bossGone) {
            adder(m_soundCues.beholderAdderMs);
        }
    }

    // 2-32'S DRAGON, once its elapsedTime - the firing clock - is past the cue
    // (ETHCallback_dragon #326-367).
    if (m_level.dragon.present && !m_soundCues.dragonAppeared &&
        m_level.dragon.elapsedMs > cues.dragonAppearAfterMs) {
        m_soundCues.dragonAppeared = true;
        latch("dragon_appeared");
    }

    if (!m_watch.valid) return;
    // 3-32'S GHOST. The arm a tick runs is the phase the last one left, and the
    // adder is called by DAMAGE while its hp is above 0 - it goes on to DEATH
    // otherwise, with no call - and by DEATH (ETHCallback_ghost #786-929,
    // #1245-1331).
    const Ghost::State& ghost = m_level.ghost;
    if (m_watch.ghostSpawned && !m_watch.ghostGone &&
        (m_watch.ghostPhase == Ghost::Phase::Death ||
         (m_watch.ghostPhase == Ghost::Phase::Damage && ghost.phase != Ghost::Phase::Death))) {
        adder(m_soundCues.ghostAdderMs);
    }
    // 4-32'S DARK DRAGON: DAMAGE's and DYING's every update call the adder
    // (darkDragonOvertimeSmallExplosionAdder, #1563, #1818); and APPEARING's jaw
    // at its widest cell plays playDarkDragonOpenMouthSound once (#608-643),
    // which is where the port restarts the mouth's system.
    const DarkDragon::State& dark = m_level.darkDragon;
    if (m_watch.darkAlive &&
        (m_watch.darkPhase == DarkDragon::Phase::Damage || m_watch.darkPhase == DarkDragon::Phase::Dying)) {
        adder(m_soundCues.darkAdderMs);
    }
    if (dark.Alive() && dark.phase == DarkDragon::Phase::Appearing && dark.look.systemRestarted &&
        !m_soundCues.darkMouth) {
        m_soundCues.darkMouth = true;
        latch("dark_dragon_mouth");
    }
}

void MagicPortalsLayer::latchLateBossSounds(const Watch& was, const Watch& now) {
    // 3-32's ghost (sounds.json _note_ghost): added by the dying knight; each
    // SHOOTING beat, and RAGE_MODE's summon, playGhostItemAppearSound; burned into
    // DAMAGE and RAGE_MODE back to SHOOTING, playGhostDamageSound; DAMAGE at hp 0
    // into DEATH, playGhostDeathSound; gone, explode's huge one. Its shots, as
    // they leave and as they end.
    if (now.ghostSpawned && !was.ghostSpawned) latch("ghost_appeared");
    for (int i = was.ghostShotsFired; i < now.ghostShotsFired; ++i) latch("enemy_shot");
    // A knight's shot, as it leaves and as it ends: addEnemyShoot #172 and ETHCallback_enemy_shoot #354-356, the same two
    // sounds (planning step 159).
    for (int i = was.minionShotsFired; i < now.minionShotsFired; ++i) latch("enemy_shot");
    for (int i = was.ghostBeats; i < now.ghostBeats; ++i) latch("ghost_item");
    for (int i = was.ghostHits; i < now.ghostHits; ++i) latch("ghost_hurt");
    for (int i = was.ghostSummons; i < now.ghostSummons; ++i) latch("ghost_item");
    if (was.ghostPhase == Ghost::Phase::Rage && now.ghostPhase == Ghost::Phase::Shooting) latch("ghost_hurt");
    if (was.ghostPhase == Ghost::Phase::Damage && now.ghostPhase == Ghost::Phase::Death) latch("ghost_dead");
    if (now.ghostGone && !was.ghostGone) latch("ghost_burst");
    for (int i = was.ghostShotsEnded; i < now.ghostShotsEnded; ++i) latch("enemy_shot_ended");
    for (int i = was.minionShotsEnded; i < now.minionShotsEnded; ++i) latch("enemy_shot_ended");
    // 4-32's dark dragon (sounds.json _note_dark_dragon): added by its spawn;
    // burned, playDarkDragonDamageSound, playGhostItemAppearSound and killAll's
    // playPortalKilledSound; DAMAGE's last waypoint back to FIGHTING, the damage
    // sound again; into DYING, playDarkDragonDeathSound; gone, explode's huge one.
    if (now.darkSummoned && !was.darkSummoned) latch("dark_dragon_appeared");
    for (int i = was.darkHits; i < now.darkHits; ++i) {
        latch("dark_dragon_hurt");
        latch("dark_dragon_item");
        latch("portal_spent");
    }
    if (was.darkPhase == DarkDragon::Phase::Damage && now.darkPhase == DarkDragon::Phase::Fighting) {
        latch("dark_dragon_hurt");
    }
    if (now.darkSummoned && now.darkPhase == DarkDragon::Phase::Dying && was.darkPhase != DarkDragon::Phase::Dying) {
        latch("dark_dragon_dead");
    }
    if (now.darkGone && !was.darkGone) latch("dark_dragon_burst");
}

void MagicPortalsLayer::playLatched(entt::registry& registry, float deltaTime) {
    // The frames' own clock, which the shared timers are kept on: the AudioManager's Timers
    // (AudioManager::update -> Timer::update), which add the script's whole milliseconds, so the
    // frame's floor (sim/ScriptClock, planning step 147).
    m_soundClockMs += ScriptClock::OfFrameMs(m_clockRules, static_cast<double>(deltaTime) * 1000.0,
                                             ScriptClock::WallMs(m_step));
    if (m_latched.empty()) return;
    // The sound switch off is SetGlobalVolume(0): nothing is heard, and what the
    // tick latched is dropped rather than kept for later.
    if (!m_soundOn) {
        m_latched.clear();
        return;
    }

    Supersonic::AudioEngine** slot = registry.ctx().find<Supersonic::AudioEngine*>();
    Supersonic::AudioEngine* audio = slot != nullptr ? *slot : nullptr;
    if (audio == nullptr || !audio->IsAvailable()) {
        // No device - a suite, a headless render, or Linux, where the original's
        // mp3s do not decode. The events are DROPPED rather than kept: a queue
        // that grows while nothing plays it is a leak with a delay on it.
        m_latched.clear();
        return;
    }

    const std::string directory = Sounds::Directory(m_paths.original) + "/";
    for (const Latched& latched : m_latched) {
        const Sounds::Hook* hook = m_soundRules.ForEvent(latched.event);
        if (hook == nullptr || hook->files.empty()) continue;

        // The shared timers. The original keeps one Timer per group rather
        // than one per hook, so the two crystal sounds hold EACH OTHER off -
        // which is what stops a run of pickups from stacking into a chord.
        if (!hook->timer.empty() && hook->minIntervalMs > 0.0) {
            const auto at = m_timerAtMs.find(hook->timer);
            if (at != m_timerAtMs.end() && m_soundClockMs - at->second < hook->minIntervalMs) continue;
            m_timerAtMs[hook->timer] = m_soundClockMs;
        }

        // The hook's own volume, or its caller's (a landing's hitIntensity / 18),
        // which the original's SoundEffectManager caps below 1 (its line 86) and
        // this caps at 1.
        const double asked = hook->volumeFromArgument ? std::clamp(latched.volume, 0.0, 1.0) : hook->volume;
        const float volume = static_cast<float>(asked);
        const float pitch = pitchFor(*hook, latched.doorStrideMs);
        if (hook->both || hook->files.size() == 1) {
            // Two samples TOGETHER, which is what playLightOnSound is.
            for (const std::string& file : hook->files) audio->Play(directory + file, false, volume, pitch);
        } else {
            // Or one of the two, drawn: the crystal gathers, the wood knocks.
            const auto which = static_cast<std::size_t>(soundRandom(0.0, static_cast<double>(hook->files.size())));
            audio->Play(directory + hook->files[which < hook->files.size() ? which : 0], false, volume, pitch);
        }
    }
    m_latched.clear();
}

std::string MagicPortalsLayer::WantedTrack() const {
    // NOT what is on screen: what the original's calls have left (m_music), and
    // heard only with both switches on - either off is pauseMusic, and the one
    // that comes back on resumes currentTrack (switchedSound, switchedMusic).
    if (!m_soundOn || !m_musicOn || m_music.stopped) return std::string();
    return m_music.current;
}

void MagicPortalsLayer::noteMusic() {
    const std::string wanted = WantedTrack();
    if (wanted == m_musicNoted) return;
    m_musicNoted = wanted;
    SUPERSONIC_LOG_INFO("Magic Portals") << "music: " << (wanted.empty() ? std::string("none") : wanted)
                                         << " from tick " << m_ticks << std::endl;
}

void MagicPortalsLayer::musicPlay(const char* track) {
    // AudioManager::playMusic (bytes 195443..195631): currentTrack first (#3-8),
    // then - only with the music switch on (#10-18) - LoadMusic, LoopSample,
    // SetSampleVolume and PlaySample. The one MediaPlayer keeps a track it is
    // already playing and stops any other first (MediaStreamManager.java:90-104),
    // which is updateMusic leaving the voice alone while WantedTrack holds. With a
    // switch off nothing is heard, and the switch's resume plays this.
    m_music.current = track;
    m_music.stopped = false;
    // #3-8 and SetSampleVolume: the track at its own volume, whatever a card left.
    m_music.volumes.erase(m_music.current);
    noteMusic();
}

void MagicPortalsLayer::musicStop() {
    // pauseMusic (186631..186849): StopSample(currentTrack). And a boss's
    // StopSample(getBossMusicName()), guarded by SampleExists. Both reach
    // MediaStreamManager.stop(), which stops the player whatever it plays
    // (MediaStreamListener.java:55-56, MediaStreamManager.java:64-68).
    // currentTrack is left as it was, for a resume.
    m_music.stopped = true;
    noteMusic();
}

void MagicPortalsLayer::musicResume() {
    // resumeMusic (186849..187005): LoadMusic, LoopSample and PlaySample of
    // currentTrack, whatever had stopped it - a no_music level, a boss's death.
    m_music.stopped = false;
    noteMusic();
}

void MagicPortalsLayer::switchedSound() {
    // GlobalSoundSwitch::manageSoundSwitch -> GlobalVolumeManager::saveVolume
    // (28964..29196): the volume written (#3-15), then #33-47
    // turnGlobalVolumeOff (pauseMusic) or turnGlobalVolumeOn, which resumes only
    // while the music switch is on.
    saveSwitches();
    if (!m_soundOn) {
        musicStop();
    } else if (m_musicOn) {
        musicResume();
    }
}

void MagicPortalsLayer::switchedMusic() {
    // MusicSwitchManager::saveMusicState (29772..30017): the state written
    // (#3-18), then #36-52 the listener's turnMusicOff (pauseMusic) or
    // turnMusicOn (resumeMusic).
    saveSwitches();
    if (m_musicOn) {
        musicResume();
    } else {
        musicStop();
    }
}

void MagicPortalsLayer::saveSwitches() {
    m_switches.SetSound(m_soundOn);
    m_switches.SetMusic(m_musicOn);
    if (std::string why; !m_switches.Save(why)) {
        SUPERSONIC_LOG_WARN("Magic Portals") << "switches not saved: " << why << std::endl;
    }
}

void MagicPortalsLayer::startLevelMusic(int index) {
    // Game::preLoop (112536..114414) #485-509: isBossFight = m_levelIndex == 31
    // (#485-491, CMPIu v31 the constant), then playGameMusic(isBossFight) unless
    // noMusic, which calls turnMusicOff instead - pauseMusic: the track that was
    // playing stops and stays currentTrack. 31 is the last level of a world of 32,
    // and every world in chapters.json has 32, so the chapters table says it: the
    // world's last level. Boss or not, 3-32 and 4-32 also set no_music, and so
    // open silent until their boss starts the track.
    const Tscn::Node* properties = FirstOfRole(m_data, Roles::kLevelProperties);
    if (properties != nullptr && properties->Meta(kNoMusicFlag) != nullptr) {
        musicStop();
        return;
    }
    musicPlay(m_chapters.Next(index) < 0 ? kBossTrack : kGameTrack);
}

void MagicPortalsLayer::latchMusic() {
    if (!m_loaded) return;
    MusicWatch now;
    // Chapter 3's ghost: playGameMusic(bossFight = true) from its init block
    // (ETHCallback_ghost #320-324), once; and its DEATH arm's StopSample
    // (#1210-1243, `CMPIu v26, v4`: the constant 4, DEATH), on every update it
    // plays until the ghost is gone.
    const Ghost::State& ghost = m_level.ghost;
    now.ghostInitialised = ghost.present && ghost.initialised;
    if (now.ghostInitialised && !m_musicWatch.ghostInitialised) musicPlay(kBossTrack);
    if (ghost.present && ghost.phase == Ghost::Phase::Death && !ghost.gone) musicStop();
    // Chapter 1's beholder: BEHOLDER_STATE_DEAD's StopSample (ETHCallback_beholder
    // #1139-1159), on every update while it is dead and not yet gone. The
    // beholder is never cleared from the level (its optional stays set), so this
    // is read off its phase.
    if (m_level.boss.beholder.has_value() && m_level.boss.beholder->phase == Boss::Phase::Dead &&
        !m_level.boss.beholder->gone) {
        musicStop();
    }
    // 2-32's dragon: the take-off's StopSample (ETHCallback_dragon #409-431, inside
    // `if (!tookOff)`), once.
    now.dragonTookOff = m_level.dragon.present && m_level.dragon.tookOff;
    if (now.dragonTookOff && !m_musicWatch.dragonTookOff) musicStop();
    // 4-32's dark dragon: playGameMusic(true) as its spawn adds it
    // (ETHCallback_dark_dragon_spawn #103-118), and its death's StopSample
    // (ETHCallback_dark_dragon #1958-2012), once, on the way into DYING
    // (`hp <= 0 && state != 4`).
    const DarkDragon::State& dark = m_level.darkDragon;
    now.darkDragonSummoned = dark.present && dark.summoned;
    now.darkDragonDying = now.darkDragonSummoned && dark.phase == DarkDragon::Phase::Dying;
    if (now.darkDragonSummoned && !m_musicWatch.darkDragonSummoned) musicPlay(kBossTrack);
    if (now.darkDragonDying && !m_musicWatch.darkDragonDying) musicStop();
    m_musicWatch = now;
}

void MagicPortalsLayer::updateMusic(entt::registry& registry) {
    const std::string wanted = WantedTrack();
    Supersonic::AudioEngine** slot = registry.ctx().find<Supersonic::AudioEngine*>();
    Supersonic::AudioEngine* audio = slot != nullptr ? *slot : nullptr;
    // The track's volume as SetSampleVolume last left it: a card turns it down and
    // back (planning step 143).
    const auto volumeOf = [this](const std::string& name, double own) {
        const auto set = m_music.volumes.find(name);
        return static_cast<float>(set != m_music.volumes.end() ? set->second : own);
    };
    if (wanted == m_track) {
        if (audio != nullptr && audio->IsAvailable() && m_musicVoice != Supersonic::AudioEngine::kInvalidVoice) {
            if (const Sounds::Track* track = m_soundRules.FindTrack(m_track)) {
                const float volume = volumeOf(m_track, track->volume);
                if (volume != m_musicVoiceVolume) {
                    audio->SetVoiceParameters(m_musicVoice, volume, 1.0f, 0.0f);
                    m_musicVoiceVolume = volume;
                }
            }
        }
        return;
    }
    // No device: m_track is left alone, so the right track starts if one ever
    // appears, rather than the layer believing it already did.
    if (audio == nullptr || !audio->IsAvailable()) return;

    if (m_musicVoice != Supersonic::AudioEngine::kInvalidVoice) {
        audio->Stop(m_musicVoice);
        m_musicVoice = Supersonic::AudioEngine::kInvalidVoice;
    }
    m_track = wanted;
    if (m_track.empty()) return;
    const Sounds::Track* track = m_soundRules.FindTrack(m_track);
    if (track == nullptr) return;
    m_musicVoiceVolume = volumeOf(m_track, track->volume);
    m_musicVoice = audio->Play(Sounds::Directory(m_paths.original) + "/" + track->file, track->loop,
                               m_musicVoiceVolume, 1.0f);
}

void MagicPortalsLayer::stopMusic(entt::registry& registry) {
    if (m_musicVoice == Supersonic::AudioEngine::kInvalidVoice) {
        m_track.clear();
        return;
    }
    Supersonic::AudioEngine** slot = registry.ctx().find<Supersonic::AudioEngine*>();
    if (slot != nullptr && *slot != nullptr) (*slot)->Stop(m_musicVoice);
    m_musicVoice = Supersonic::AudioEngine::kInvalidVoice;
    m_track.clear();
}

void MagicPortalsLayer::OnFixedUpdate(entt::registry& registry, float fixedDelta) {
    // What the last frame's edge cull hid is shown again before any of this tick's code reads or
    // writes a picture's visibility (restoreEdgeCulls).
    restoreEdgeCulls(registry);
    // `fixedDelta` is the engine's tick, which the physics integrates and the layer
    // does not touch. What the SCRIPTS see of it is the original's floor: the whole
    // milliseconds of a frame, 16 of the tick's 16.667 (sim/ScriptClock.hpp).
    m_step = ScriptClock::TickStep(m_clockRules, fixedDelta);
    const ScriptClock::Step& step = m_step;
    // What the last frame shook off the camera first, so the tick reads it at
    // rest: a tap is mapped before the generator moves anything, as the original's
    // portal manager maps one before g_earthquakeGen runs.
    liftQuake(registry);
    fixedTick(registry, step);
    // main's loop updates the Notificator after the state's loop, whatever is up.
    tickNotificator(ScriptClock::Ms(step));
    // Then the generator's update, between what this tick's events called it
    // with from before it and what they called it with from after.
    quakeTick();
    // And last, after every write the tick made to a picture or the camera: the frame
    // draws them where the tick left them, as the original's frame draws its entities.
    drawPicturesAtTick(registry);
    // A modern run whose first level began before the viewport was published says its rectangles now (logModernControls).
    if (m_controlsRectsPending) logModernControls(registry);
}

void MagicPortalsLayer::drawPicturesAtTick(entt::registry& registry) {
    using namespace Supersonic;
    // The display outruns the tick (pictures.json), so the engine draws between the last
    // two ticks for smoothness, as it did before planning step 150: the pictures then
    // stand up to one tick behind what the lights, the effects and the HUD read from the
    // sim, which is the price, and under 17 ms at 60 Hz ticks.
    if (m_pictureRate.Smoothing()) return;
    // WHY THIS IS AN HONEST USE OF THE ENGINE'S INTERPOLATION and not a bypass of it:
    // BeginTick put each transform back at the last tick's end and recorded that as
    // `previous`; the tick has since written this one's. Both ends at this one, and
    // EndTick makes `current` the same, so Apply - a + (a - a) t - returns the tick's own
    // place at any alpha, a --fixed-step frame's 0 and a real frame's anything between. The
    // original has no interpolation: a body that moved in a frame is drawn moved in that
    // frame (SetPositionXY is a SetTransform inside Update; the draw reads GetPosition at
    // draw time, ETHRenderEntity.cpp:106; D10_check 4a).
    // An entity that has not run a whole tick yet is left to EndTick, which seeds both
    // ends from it.
    for (auto [entity, interpolated, transform] :
         registry.view<InterpolatedTransformComponent, TransformComponent>().each()) {
        (void)entity;
        if (!interpolated.captured) continue;
        interpolated.previousPosition = transform.position;
        interpolated.previousRotation = transform.rotation;
        interpolated.previousScale = transform.scale;
    }
    // THE CAMERA, by the same rule and for a reason of its own: the pictures and the camera
    // are one picture. A body drawn at the tick's place against a camera drawn a tick back
    // stands at body(t) - camera(t - 1), which is neither what the engine drew before nor
    // what the original draws (it reads GetCameraPos at draw time too). A tap is mapped
    // through the camera the last tick left (BeginTick puts it back), which is now also the
    // camera the last frame drew.
    if (m_camera == entt::null || !registry.valid(m_camera)) return;
    auto* camera = registry.try_get<CameraComponent>(m_camera);
    auto* drawn = registry.try_get<InterpolatedCameraComponent>(m_camera);
    if (camera == nullptr || drawn == nullptr || !drawn->captured) return;
    drawn->previous.position = camera->position;
    drawn->previous.yaw = camera->yaw;
    drawn->previous.pitch = camera->pitch;
    drawn->previous.fov = camera->fov;
    drawn->previous.orthoHeight = camera->orthoHeight;
}

void MagicPortalsLayer::fixedTick(entt::registry& registry, const ScriptClock::Step& step) {
    using Supersonic::Input;
    ++m_ticks;
    // The layer's own clock: what the original's absolute GetTimeF reads give.
    // WALL milliseconds, which sys:GetTime and sys:GetTimeF read: the engine's own tick, not
    // the floor. Every script reader of the floor takes m_tickMs (planning step 147).
    m_layerClockMs += ScriptClock::WallMs(step);
    m_tickMs = ScriptClock::Ms(step);
    // Asked for on the TICK, where a key press is an edge, and answered on the
    // frame, where the picture is.
    if (Input::TickWasPressed(kDump)) m_dumpRequested = true;
    const bool overLevel = m_screen == Screen::Finished || m_screen == Screen::Dead;
    // A menu is up instead of a level: it takes the tick, and nothing below
    // runs. The two are never both in the registry.
    if (m_screen != Screen::None && !overLevel) {
        menuTick(registry, step);
        updateHud(registry);
        return;
    }
    // A pause is up over the level: it takes the tick, and the level does not
    // step. Only on the tick it resumes does the level take up its tick again,
    // from exactly where the tap left it.
    if (m_pause.open) {
        if (pauseTick(registry, step) && m_loaded && m_screen == Screen::None) {
            // The tick the pause was opened on read its input and stopped short
            // of BeforeStep; this one runs that half now, with the walk the keys
            // ask for - the pointer is on the button that resumed it, and a tap
            // spent on a button is spent.
            stepLevel(registry, keyDirection(), step);
        }
        tickPlaqueDismissal(false);
        updateHud(registry);
        return;
    }
    // A popup the same way. One a help block raised mid-tick owes that tick's
    // BeforeStep half, as the pause does; one raised as the level loaded owes
    // nothing, and the next tick is the level's first.
    if (m_popup.open) {
        if (popupTick(registry, step) && m_loaded && m_screen == Screen::None) {
            stepLevel(registry, keyDirection(), step);
        }
        tickPlaqueDismissal(false);
        updateHud(registry);
        return;
    }
    // The finished or lost screen is up over a level that GOES ON RUNNING (spec
    // D7): the screen's clock, counters and buttons first - a button that leaves
    // the level leaves it before it is stepped again - and then the level's own
    // tick below, which reads no input once it has ended.
    if (overLevel && endScreenTick(registry, step)) {
        updateHud(registry);
        return;
    }
    // The bodies' boxes over the art, or not: the picture only.
    if (Input::TickWasPressed(kBoxes)) m_showBoxes = !m_showBoxes;
    if (!overLevel) {
        // Out of a level that is not being played - one the port refused, which
        // otherwise has nowhere to go - to its grid. A level being played takes the
        // back key as the pause (readInput).
        if (m_current >= 0 && !m_loaded && Input::TickWasPressed(kBack)) {
            m_menuWorld = m_chapters.levels[static_cast<std::size_t>(m_current)].world;
            openMenu(registry, Screen::Levels);
            updateHud(registry);
            return;
        }
        // Skip and retry first, so the tick that asks plays the level it lands on.
        // A skip from a chapter's last level is goToNextLevel's chapter select,
        // whose first tick is the next.
        if (m_current >= 0 && Input::TickWasPressed(kSkip)) {
            goTo(registry, m_chapters.Next(m_current));
            if (menuStateUp()) {
                updateHud(registry);
                return;
            }
        } else if (m_current >= 0 && Input::TickWasPressed(kRetry)) {
            loadLevel(registry, m_current);
        }
        // A level that raised its tutorial as it loaded stops HERE, as one loaded
        // from a menu does: not a tick older and not stepped under its popup, so a
        // retried 1-02 stands at age 0 as the first load of it does.
        if (GameTimeStopped()) {
            updateHud(registry);
            return;
        }
    }

    const double dtMs = ScriptClock::Ms(step);
    if (m_loaded) {
        // DEV ONLY: a burn a capture scheduled for this tick, set as Game's blast
        // test sets it, for the step's own AfterStep to read on this same tick.
        if (m_level.darkDragon.Alive() &&
            std::find(m_devBurns.begin(), m_devBurns.end(), m_ticks) != m_devBurns.end()) {
            m_level.darkDragon.burned = true;
            SUPERSONIC_LOG_INFO("Magic Portals") << "DEV burn of the dark dragon on tick " << m_ticks << std::endl;
        }
        // DEV ONLY: a breakable a suite asked broken on this tick, as a stone's touch
        // breaks it, for this tick's latch to see.
        for (const auto& [tick, node] : m_devBreaks) {
            if (tick != m_ticks || m_level.demolish.FindBreakable(node) == nullptr) continue;
            m_level.demolish.Break(registry, node, "a DEV break");
            SUPERSONIC_LOG_INFO("Magic Portals") << "DEV break of " << node << " on tick " << m_ticks << std::endl;
        }
        // DEV ONLY: the player put where a capture asked, for this tick's AfterStep
        // to judge - a door opened for the camera, never a route.
        for (const auto& [tick, atPx] : m_devPlaces) {
            if (tick != m_ticks || m_level.player == entt::null || !registry.valid(m_level.player)) continue;
            auto& transform = registry.get<Supersonic::TransformComponent>(m_level.player);
            const glm::vec3 world = Units::ToWorld(atPx.x, atPx.y);
            transform.position = glm::vec3(world.x, world.y, transform.position.z);
            if (auto* rigid = registry.try_get<Supersonic::RigidBodyComponent>(m_level.player)) {
                rigid->velocity = glm::vec3(0.0f);
            }
            SUPERSONIC_LOG_INFO("Magic Portals") << "DEV place of the player at " << atPx.x << "," << atPx.y
                                                 << " on tick " << m_ticks << std::endl;
        }
        // The app has just stepped physics. The pairs it has just brought
        // together first, read before anything after the step moves or breaks a
        // body - Box2D calls a begin-contact callback inside the step itself.
        // The free-flight replay's judgments are earthquake.json's: the circle skin in screen px (converted as touchPx
        // is), the two guesses in the watch's own units.
        const Impacts::FreeFlightRules flight{Earthquake::ToUnits(m_quakeRules, m_quakeRules.circleTouchPx),
                                              m_quakeRules.brakeReachSlackUnits, m_quakeRules.brakeFrictionCone};
        m_impactBegins = m_impacts.AfterStep(registry, Earthquake::ToUnits(m_quakeRules, m_quakeRules.touchPx),
                                             static_cast<double>(step.physicsS), flight);
        // A stone that began to touch a knight hard enough crushes it in that same callback (planning step 159), before
        // anything after the step reads the knights.
        stoneHits(registry);
        // So first what follows a step...
        Game::AfterStep(m_data, registry, m_level, step);
        // The wall's callback (ETHBeginContactCallback_breakable_wall, planning step 175), as Game::AfterStep judged it: read at the
        // margin (the history) and/or at the watch's begin (the free flight's replay), the larger reading judging.
        for (const Demolish::Contact& contact : m_level.demolish.contactsNow) {
            std::string read = contact.atMargin ? "first look at the margin, the history reads " + std::to_string(contact.historyMs) + " m/s"
                                                : std::string("second look, at the watch's begin");
            if (contact.atSkin) {
                read += "; the watch's begin reads " + std::to_string(contact.replayMs) + " m/s" +
                        (contact.freeFlight ? ", replayed " + std::to_string(contact.replayedSteps) + " step(s) past the brake"
                                            : std::string(", from the history"));
            }
            SUPERSONIC_LOG_INFO("Magic Portals") << "wall contact: " << contact.stone << " begins on " << contact.wall << " at "
                                                 << contact.speedMs << " m/s (the gate " << m_level.demolish.rules.wallBreaksAboveMs
                                                 << "): " << (contact.broke ? "the wall breaks" : "the wall holds")
                                                 << (contact.stoneDied ? ", the stone is destroyed" : "") << " on tick " << m_ticks
                                                 << " (" << read << ")" << std::endl;
        }
        // What the tick just did, remembered for the frame to play. Before the
        // branches below, which may put a screen up.
        latchSimSounds(step);
        latchMusic();
        achievementsForKeys();
        latchSimEffects(registry, step);
        latchSimQuakes();
        // ...and the moment the exit reports, the beat before the medal; the
        // moment the player dies, the beat before the lost screen. Should both
        // come on one tick, reaching the exit wins: the Godot project's order of two
        // triggers in a frame is not defined. And once either has begun, the
        // other cannot: the level runs on behind the beat and the screen, and a
        // hidden player neither finishes nor dies again.
        if (m_finishing) {
            // Already in the door: the level keeps running behind the effect,
            // as the original's does, and the screen comes when the beat is up
            // (the medal was saved on the door's tick, recordWin).
            m_finishClockMs += dtMs;
            m_padEndByte = LevelEnd::PadDecayByte(m_levelEndRules, m_padEndByte, 1);
            // The original does not go straight on: once gameWonDelay is up,
            // checkGameEnd makes the LevelFinishedLayer writeScore built on the
            // door's frame current - the medal the play earned, and buttons to
            // play it again, go on, or pick another.
            if (m_screen == Screen::None && m_finishClockMs >= m_wonDelayMs) openFinished(registry);
        } else if (m_dying) {
            // Killed, and the lost screen not up yet: the level goes on running
            // behind it exactly as it does behind the door's effect.
            m_dyingClockMs += dtMs;
            m_padEndByte = LevelEnd::PadDecayByte(m_levelEndRules, m_padEndByte, 1);
            if (m_screen == Screen::None && m_dyingClockMs >= m_levelEndRules.lostDelayMs) openDead(registry);
        } else if (m_level.goals.completed) {
            // GOING IN. Its sound is the door's, not the medal's - `level_finished`
            // now maps to playFinalDoorSound, and sounds.json carries the decode
            // of why the cue this once had is called by nothing in the original.
            latch("level_finished");
            // And levelFinishedEffect's two, which checkGameEnd calls BEFORE it
            // hides the character: read while the body still stands in the door.
            latchFinishEffects(registry);
            // Hide(), and the velocity zeroed with it: the character is gone from
            // the doorway and cannot be left walking on the spot. Destroying the
            // two drawables IS the port's Hide - syncDrawables skips what is not
            // there - and the body stays in the world so nothing else that holds
            // it has to care.
            if (m_player != entt::null && registry.valid(m_player)) registry.destroy(m_player);
            m_player = entt::null;
            if (m_playerQuad != entt::null && registry.valid(m_playerQuad)) registry.destroy(m_playerQuad);
            m_playerQuad = entt::null;
            if (m_level.player != entt::null && registry.valid(m_level.player)) {
                if (auto* rigid = registry.try_get<Supersonic::RigidBodyComponent>(m_level.player)) {
                    rigid->velocity = glm::vec3(0.0f);
                }
            }
            m_direction = 0.0f;
            m_finishing = true;
            m_finishClockMs = 0.0;
            endHud();
            // Said with the tick, so a capture can be dated from the door.
            SUPERSONIC_LOG_INFO("Magic Portals") << "door reached on tick " << m_ticks << ", pads at "
                                                 << m_padEndByte << std::endl;
            // And writeScore, on this same tick (checkGameEnd #75) - and the
            // LevelFinishedLayer it builds, whose constructor asks for the
            // achievements, with GameStateController's time: this tick's included.
            recordWin();
            achievementsAtDoor(registry, m_levelAgeMs + dtMs);
        } else if (m_level.hazards.playerDied) {
            // DYING, which is not the same moment as the lost screen, and was
            // the whole of this branch before: death was an instant retry, so
            // there was no screen, no beat and no sound but one played at the
            // wrong instant.
            //
            // Only a FALL has a cue here. checkGameLost plays
            // playDieByFallSound when the player leaves the level and plays
            // nothing at all for an hp death; playDeathSound belongs to the
            // screen, 1400 ms later. sounds.json carries that decode.
            if (m_level.hazards.diedByFalling) latch("player_fell");
            // And what checkGameLost adds where it happened: a skull held inside
            // the view for a fall, blood at the character for anything else.
            latchDeathEffects(registry);
            ++m_deaths;
            // Hide(), as the finish does it and as checkGameLost does it too -
            // it hides the character before the delay. Left drawn, a corpse
            // stood in the pit for 1400 ms, which is the same fault as the mage
            // walking on the spot in the doorway.
            if (m_player != entt::null && registry.valid(m_player)) registry.destroy(m_player);
            m_player = entt::null;
            if (m_playerQuad != entt::null && registry.valid(m_playerQuad)) registry.destroy(m_playerQuad);
            m_playerQuad = entt::null;
            if (m_level.player != entt::null && registry.valid(m_level.player)) {
                if (auto* rigid = registry.try_get<Supersonic::RigidBodyComponent>(m_level.player)) {
                    rigid->velocity = glm::vec3(0.0f);
                }
            }
            m_direction = 0.0f;
            m_dying = true;
            m_dyingClockMs = 0.0;
            endHud();
            SUPERSONIC_LOG_INFO("Magic Portals") << "player died on tick " << m_ticks << ", pads at "
                                                 << m_padEndByte << std::endl;
        }
        // AND THE WIN CUTSCENES' CALLBACKS, from the tick the level ends: static
        // entities' callbacks, run after the game state that decided it, so from
        // the door's own tick (cutscenes.json).
        if (m_finishing || m_dying) stepCutscenes(registry, dtMs);
    }
    if (m_loaded) {
        // The level is a tick older. BEFORE the input, because the pads slide
        // in and a tap is tested against where they are on this tick.
        m_levelAgeMs += dtMs;
        m_levelStepMs = dtMs;
        m_levelFrameWallMs += ScriptClock::WallMs(step);
        // GameLayer is updated on this tick: the plaque's colour is written.
        tickPlaqueDismissal(true);
        if (m_finishing || m_dying) {
            // NOTHING IS PRESSABLE from the door or the death on (spec 3.5):
            // GameLayer is dismissed and no screen is current yet, and once one
            // is its buttons are its own (endScreenTick). The level still runs,
            // with nobody walking it - and no handle held, though each pulses on.
            m_level.handles.Rest(dtMs);
            stepLevel(registry, 0.0f, step);
            updateHud(registry);
            return;
        }
        // Then this tick's input, and what comes before the next step.
        const float direction = readInput(registry);
        // A control may have opened a pause, or a help block a popup, which stops
        // the level HERE, after the step it has just taken and before the next:
        // the tick it resumes on runs the rest. Or it opened a screen that
        // unloaded the level, and nothing below has a level to step any more.
        if (!m_loaded || m_screen != Screen::None || GameTimeStopped()) {
            updateHud(registry);
            return;
        }
        stepLevel(registry, direction, step);
    }
    updateHud(registry);
}

void MagicPortalsLayer::stepLevel(entt::registry& registry, float direction, const ScriptClock::Step& step) {
    m_direction = direction; // for the picture: which way the player walks this tick
    m_aspect = viewportAspect(registry);
    // G10-6: b2_maxTranslation is 100 SCREEN pixels, so the original's clamp on a step's travel goes with the screen
    // it runs on - 35.6 units at 720 lines, 23.7 at 1080 - and the sim is told the screen's own, one level unit being
    // height / 256 pixels. Left at the reference where there is no viewport (the suites, a headless run), so a route
    // replays the same on every window.
    if (const auto* viewport = registry.ctx().find<Supersonic::ViewportInfo>()) {
        const float screenHeightPx = viewport->Size().y;
        if (screenHeightPx > 0.0f && m_viewHeightPx > 0.0) {
            m_data.tuning.screenPxPerUnit = static_cast<double>(screenHeightPx) / m_viewHeightPx;
        }
    }
    // SideScrollerCharacter::detectPushing, asked of the body as the physics step
    // left it and before Steer sets its velocity, as applyForces reads it. Only
    // where the update that runs it runs: never on a no_gravity level
    // (forceNoImpulse skips applyForces), and not once the player is gone into
    // the door or dead. Kept for the NEXT step's picture (syncDrawables).
    bool pushing = false;
    if (!m_level.noGravity && m_playerQuad != entt::null && !m_finishing && !m_dying &&
        m_level.player != entt::null && registry.valid(m_level.player)) {
        const Art::Push& push = m_artRules.character.push;
        pushing = Player::Pushing(
            registry, m_level.player,
            Player::PushAim(registry, m_level.player, m_data.tuning, direction, step),
            push.reachFrameWidthShare * playerCellPx(false).x, push.offsetPx);
    }
    // BEFORE the tick, not after it: the dragon's claw measures everything
    // from the camera's left edge, and the camera's left edge depends on how
    // wide the window is. Dragon.hpp says why that width is the only piece of
    // the camera the sim takes from outside.
    m_level.dragon.viewWidthPx = ViewPx().x;
    Game::BeforeStep(m_data, registry, m_level, direction, step);
    // The sleepers whose support the tick took away or moved, woken at the end of BeforeStep (Resting::Watch, planning step 163).
    for (const Resting::Looked& looked : m_level.restingLooked) {
        std::ostringstream rests;
        for (const auto& [name, gap] : looked.supports) rests << (rests.tellp() > 0 ? ", " : "") << name << " (gap " << gap << ")";
        SUPERSONIC_LOG_INFO("Magic Portals") << "resting: " << looked.sleeperName << " asleep on tick " << m_ticks << ", resting on "
                                             << (looked.supports.empty() ? std::string("nothing") : rests.str()) << std::endl;
    }
    for (const Resting::Woken& woken : m_level.restingWoken) {
        SUPERSONIC_LOG_INFO("Magic Portals") << "resting: " << woken.sleeperName << " woken on tick " << m_ticks << ", its support "
                                             << woken.supportName << " " << Resting::CauseName(woken.cause) << std::endl;
    }
    // The dragon's callback runs HERE, before the step, and the other bosses'
    // after it (AfterStep): its take-off's stop is read on its own tick.
    latchMusic();
    if (m_level.dragon.present) {
        // level31a is the ONE level in all 128 that sets auto_camera, and it
        // sets it to dragon.ent. AutoCameraController::update is a hard lock
        // to that entity's x - no lag, no hold, y pinned to 0 - so the
        // follow's easing does not run here at all. dragon.json has the
        // decode, and Camera::Clamp is the same arithmetic as the original's
        // camMin/camMax expressed for a centre rather than a corner. It has no
        // dragMoveCamera, so no finger moves it (camera.json `drag`).
        const glm::dvec2 view = ViewPx();
        m_follow.centrePx =
            Camera::Clamp(glm::dvec2(m_level.dragon.atPx.x + view.x * 0.5, view.y * 0.5), view, m_boundsPx);
    } else {
        glm::dvec2 targetPx = Units::ToPixels(registry.get<Supersonic::TransformComponent>(m_level.player).position);
        // 4-19's landing sets the character on the door every frame (cutscenes.json
        // rocket_landing #335-341), and the camera follows the character. The port
        // gives the camera that point and leaves the hidden body where it went in:
        // a body teleported through the floor each tick would push what it met.
        if (m_landing.running && m_doorSprite >= 0) targetPx = doorPointPx();
        m_cameraTargetPx = targetPx;
        // CameraController::update with this tick's touch (camera.json `drag`): what
        // readInput read, and whether anything went through a portal in the half
        // of the tick before it (Game::AfterStep's portals, the frame before's
        // teleportToOther in the original's order).
        Camera::Unlocks unlocks;
        unlocks.teleported = !m_level.portals.transits.empty();
        unlocks.padMoving = m_cameraPadMoving;
        m_follow.Tick(m_cameraRules, targetPx, ViewPx(), m_boundsPx, step.scriptS, m_cameraFeel, unlocks);
    }
    // Taken: a tick that reads no input drags nothing.
    m_cameraFeel = Camera::Feel{};
    m_cameraPadMoving = false;
    placeCamera(registry);
    layOutControls();
    // AFTER the camera: the sign is aimed at the corner this tick left it at,
    // with its frame time held back while the blacks are whole and handed
    // over in one piece when they start, as the original's load hands it.
    tickNoPortalSign(Hud::HandOver(m_hudRules, m_levelAgeMs, ScriptClock::Ms(step),
                                   m_sign.heldMs));
    // MainCharacter::update on a no_gravity level, once a step: never under a
    // pause or a popup, where no step runs, and not once the player has gone into
    // the door or died, where its picture is gone (the original skips the update
    // when finished).
    if (m_level.noGravity && m_playerQuad != entt::null && !m_finishing && !m_dying) {
        m_noGravityColumn = Art::UpdateNoGravity(m_artRules.character, m_noGravityPlayer, m_direction != 0.0f,
                                                 ScriptClock::Ms(step));
    }
    // ETHCallback_projectile's SetFrame(_projectileFrameTimer.set(0, 5, 30, true)),
    // once a step while a shot flies: the one timer, so a shot starts on the cell
    // the last one left. No step runs under a pause or popup, so the cell holds there,
    // where the original's keeps cycling (a stated divergence, art.json shot.animation).
    // Only a shot whose callback ran this tick calls it: one fired on this very tick, or a leg a
    // reflector turned, has had no callback yet (Flight::launched; planning step 149).
    if (m_level.portals.flight && m_level.portals.flight->launched) {
        const Art::Shot& bolt = m_artRules.shot;
        m_shotCell = m_shotFrames.Set(0, bolt.Frames() - 1, bolt.strideMs, true, ScriptClock::Ms(step));
    }
    // And ETHCallback_enemy_shoot's SetFrame(_projectileFrameTimer.set(0, 5, 30, true))
    // (#126-137), once a step for every shot of the chapter-3 ghost in flight: the
    // SAME timer, so each call adds its step to it, and a player's shot and a ghost's
    // in the air together step it twice. After the player's, in the sim's order: the
    // original's order among entity callbacks is its scene walk's, INFERRED.
    m_ghostShotCells.clear();
    for (const Ghost::Shot& shot : m_level.ghost.shots) {
        const Ghost::Rules& rules = m_level.ghost.rules;
        m_ghostShotCells[shot.id] = m_shotFrames.Set(rules.shotFirstCell, rules.shotLastCell, rules.shotCellStrideMs,
                                                     true, ScriptClock::Ms(step));
    }
    // And a knight's shot's, on the same timer (the ghost's before them: the sim's order among the dynamic entities is
    // INFERRED), stepped from the tick the shot is made as the ghost's are.
    m_minionShotCells.clear();
    for (const Minions::EnemyShot& shot : m_level.minions.shots) {
        const Minions::ShotRules& rules = m_level.minions.rules.shot;
        m_minionShotCells[shot.id] = m_shotFrames.Set(rules.firstCell, rules.lastCell, rules.cellStrideMs, true,
                                                      ScriptClock::Ms(step));
    }
    // linearMotion's calls BEFORE the pictures are placed: a call steps the angle and
    // writes the position in one, and the frame draws what it wrote (motions.json).
    advanceMotions(step);
    // ETHCallback_minion sets a minion's velocity and its frame in one update:
    // Minions::Tick, in BeforeStep above, chose each one's arm, and this frames it.
    advanceMinions(step);
    syncDrawables(registry);
    m_pushing = pushing;
    // StaticSky::update's order: every sky placed (syncSprites, just now), and
    // only then its scroll moved on, so the next tick draws what this one left.
    if (m_sky.running) Sky::Advance(m_skyRules, m_sky, step.scriptS);
    // Last: what each body the impact rows watch carries into the step, as the
    // tick has steered it (the original's callbacks read the velocity the last
    // frame left). Read only.
    m_impacts.BeforeStep(registry, impactBodies());
}

// ---- the pause ----------------------------------------------------------------

bool MagicPortalsLayer::pauseAllowed() const {
    // Where the pause control is drawn and can be pressed: a level being played,
    // not going into its door and not dying.
    return m_hudReady && m_loaded && m_screen == Screen::None && !m_finishing && !m_dying && !GameTimeStopped();
}

Pause::Switches MagicPortalsLayer::PauseSwitches() const {
    Pause::Switches switches;
    switches.soundOn = m_soundOn;
    switches.musicOn = m_musicOn;
    switches.musicAddedMs = m_pause.musicAddedMs;
    switches.musicDismissedMs = m_pause.musicDismissedMs;
    return switches;
}

void MagicPortalsLayer::openPause(entt::registry& registry) {
    using namespace Supersonic;
    if (!pauseAllowed() || m_current < 0) return;
    const Chapters::Level& entry = m_chapters.levels[static_cast<std::size_t>(m_current)];
    m_pause = PauseScreen{};
    m_pause.open = true;
    // What CustomGameMenuLayer's constructor reads: the score recorded for this
    // level, which gates the current-score plaque, its medal and skip, and the
    // golden score its plaque names.
    m_pause.level.savedMedal = m_scores.Get(entry.world, entry.index);
    m_pause.level.goldenScore = entry.goldenScore;
    m_pause.level.index = entry.index;
    // The touch a shot waits on is forgotten: the pause's buttons act on the way
    // down, so one that went down on Resume would otherwise fire a shot where it
    // came up, back in the level (the port's own divergence, not the original's).
    m_shotTouch = Portals::TouchTrack{};
    m_handTouch = Portals::TouchTrack{};
    m_pressedControl.reset();
    // And so is the camera's record of it, which the pause does not follow: a touch
    // still held when the level resumes is taken up there.
    m_cameraTouch = Camera::TouchRecord{};
    // And no handle is held under it (game time stops: no pulse either).
    m_level.handles.Rest(0.0);
    m_handleHitPx.reset();

    // GAME TIME STOPS (STimeManager::pause).
    freezeWorld(registry);
    // The pads go with game time.
    layOutControls();
}

void MagicPortalsLayer::freezeWorld(entt::registry& registry) {
    using namespace Supersonic;
    m_frozen = Frozen{};
    // The level is not stepped from here, but the app steps physics before every
    // tick regardless, so every body is held as it stands now and put back after
    // each of those steps: velocities, sleep and all, so the level resumes as
    // though game time had never stopped.
    for (auto [entity, transform, body] : registry.view<TransformComponent, RigidBodyComponent>().each()) {
        m_frozen.held.push_back(Frozen::Held{entity, transform, body});
    }
    // And the flipbooks, which the app turns on the tick: the world under a
    // popup measured still (temporal std 0.001, spec 0.3), so nothing animates.
    for (auto [entity, animation] : registry.view<SpriteAnimationComponent>().each()) {
        if (!animation.playing) continue;
        animation.playing = false;
        m_frozen.stoppedFlipbooks.push_back(entity);
    }
}

void MagicPortalsLayer::holdWorld(entt::registry& registry) {
    using namespace Supersonic;
    for (const Frozen::Held& held : m_frozen.held) {
        if (!registry.valid(held.entity)) continue;
        if (auto* transform = registry.try_get<TransformComponent>(held.entity)) *transform = held.transform;
        if (auto* body = registry.try_get<RigidBodyComponent>(held.entity)) *body = held.body;
    }
}

void MagicPortalsLayer::thawWorld(entt::registry& registry) {
    using namespace Supersonic;
    // The world as game time left it, this tick's physics step undone too.
    holdWorld(registry);
    for (const entt::entity entity : m_frozen.stoppedFlipbooks) {
        if (!registry.valid(entity)) continue;
        if (auto* animation = registry.try_get<SpriteAnimationComponent>(entity)) animation->playing = true;
    }
    m_frozen = Frozen{};
}

void MagicPortalsLayer::closePause(entt::registry& registry) {
    if (!m_pause.open) return;
    thawWorld(registry);
    // UILayer::hide(true): gone on this frame, no fade, and every element reset,
    // so the next pause plays its whole entrance again (spec 2.4).
    m_pause = PauseScreen{};
    m_achievementsPopup = AchievementsPopupScreen{};
    layOutControls();
}

// ---- the popups ------------------------------------------------------------------

std::string MagicPortalsLayer::originalAsset(const std::string& relative) const {
    // THE HD TWIN, as the rest of the port's UI takes it (menuImage), but beside
    // the file wherever it lives: the hand and the stone are entities, the smoke a
    // particle, and those keep their twins in entities/hd/, not sprites/hd/.
    std::error_code ec;
    const std::filesystem::path path(relative);
    const std::string hd = m_paths.original + "/" + (path.parent_path() / "hd" / path.filename()).generic_string();
    if (std::filesystem::exists(hd, ec)) return hd;
    return m_paths.original + "/" + relative;
}

void MagicPortalsLayer::findHelpBlocks() {
    m_helpBlocks.clear();
    if (!m_loaded) return;
    for (const Tscn::Node& node : m_data.scene.nodes) {
        if (node.parent != ".") continue;
        const Tscn::Value* entity = node.Meta("entity_name");
        if (entity == nullptr || entity->kind != Tscn::Value::Kind::String || entity->text != m_popupRules.helpBlock.entity) {
            continue;
        }
        HelpBlock block;
        const Tscn::Value* box = node.Meta("trigger_size");
        if (!PositionOf(&node, block.atUnits) || box == nullptr || box->kind != Tscn::Value::Kind::Vector2) continue;
        block.boxUnits = glm::dvec2(box->numbers[0], box->numbers[1]);
        m_helpBlocks.push_back(block);
    }
}

std::vector<Hud::Rect> MagicPortalsLayer::HelpBlockRects() const {
    std::vector<Hud::Rect> rects;
    const glm::dvec2 corner = m_follow.centrePx - ViewPx() * 0.5;
    for (const HelpBlock& block : m_helpBlocks) {
        rects.push_back(Popup::HelpBlockRect(m_popupRules, block.atUnits, block.boxUnits, corner));
    }
    return rects;
}

void MagicPortalsLayer::ScheduleDevTap(int tick, std::optional<glm::dvec2> viewFraction, int releaseTick) {
    DevTap tap;
    tap.tick = tick;
    tap.releaseTick = std::max(releaseTick, tick + 1);
    tap.onHelpBlock = !viewFraction.has_value();
    tap.viewFraction = viewFraction.value_or(glm::dvec2(0.0));
    m_devTaps.push_back(tap);
}

void MagicPortalsLayer::ScheduleDevDrag(int tick, const glm::dvec2& from, const glm::dvec2& to, int releaseTick) {
    DevTap tap;
    tap.tick = tick;
    tap.releaseTick = std::max(releaseTick, tick + 1);
    tap.viewFraction = from;
    tap.toFraction = to;
    m_devTaps.push_back(tap);
}

MagicPortalsLayer::Touch MagicPortalsLayer::touchThisTick(const entt::registry& registry) {
    using Supersonic::Input;
    Touch touch;
    const glm::dvec2 view = ViewPx();
    const auto* viewport = registry.ctx().find<Supersonic::ViewportInfo>();
    // A scheduled tap's place in the VIEWPORT's pixels, which is what a real
    // press carries and what the shot maps into the level: the tap is given as a
    // fraction of the view, so it lands on the same point of it in a window of
    // any size. With no viewport nothing maps it, and it keeps the 1280x720 its
    // pixels were given in (main.cpp's --tap).
    const glm::dvec2 viewportPx = viewport != nullptr && viewport->Size().x > 0.0f && viewport->Size().y > 0.0f
                                      ? glm::dvec2(viewport->Size())
                                      : glm::dvec2(1280.0, 720.0);
    // DEV ONLY first: a scheduled tap is pressed on its tick and released on the
    // next, where it stood.
    for (auto it = m_devTaps.begin(); it != m_devTaps.end();) {
        if (it->tick > m_ticks) {
            ++it;
            continue;
        }
        glm::dvec2 at = it->viewFraction * view;
        if (it->toFraction && it->pressed) {
            // A drag, at an even pace from where it went down to where it is let go.
            const double span = static_cast<double>(std::max(it->releaseTick - it->pressedTick, 1));
            const double along = std::clamp(static_cast<double>(m_ticks - it->pressedTick) / span, 0.0, 1.0);
            at = (it->viewFraction + (*it->toFraction - it->viewFraction) * along) * view;
        }
        if (it->onHelpBlock) {
            // Only a block the view shows: a player cannot touch one off the
            // screen. A tap with none is dropped, so it does not hold the pointer
            // for the rest of the run.
            const std::vector<Hud::Rect> rects = HelpBlockRects();
            if (!rects.empty()) at = rects.front().Centre();
            if (rects.empty() || at.x < 0.0 || at.y < 0.0 || at.x > view.x || at.y > view.y) {
                SUPERSONIC_LOG_WARN("Magic Portals") << "DEV tap on tick " << m_ticks
                                                     << " dropped: no help block on the view" << std::endl;
                it = m_devTaps.erase(it);
                continue;
            }
        }
        touch.over = true;
        touch.atView = at;
        touch.atPx = at / view * viewportPx;
        if (!it->pressed) {
            it->pressed = true;
            it->viewFraction = at / view;
            it->onHelpBlock = false; // released where it went down
            touch.pressed = true;
            touch.held = true;
            // Its release is counted from the tick it went down on, which may be
            // later than the one it asked for; a drag keeps its length.
            const int late = std::max(m_ticks - it->tick, 0);
            it->releaseTick = std::max(it->releaseTick + (it->toFraction ? late : 0), m_ticks + 1);
            it->pressedTick = m_ticks;
            SUPERSONIC_LOG_INFO("Magic Portals") << "DEV tap down on tick " << m_ticks << " at (" << at.x << ", "
                                                 << at.y << ") units" << std::endl;
        } else if (m_ticks < it->releaseTick) {
            // Held where it went down, until its release tick.
            touch.held = true;
        } else {
            touch.released = true;
            SUPERSONIC_LOG_INFO("Magic Portals") << "DEV tap up on tick " << m_ticks << std::endl;
            m_devTaps.erase(it);
        }
        return touch;
    }
    if (viewport == nullptr || !viewport->pointerOverGame) return touch;
    const glm::vec2 size = viewport->Size();
    if (size.x <= 0.0f || size.y <= 0.0f) return touch;
    const glm::vec2 local = viewport->ToLocal(Input::MousePosition());
    touch.over = true;
    touch.atPx = glm::dvec2(local);
    touch.atView = glm::dvec2(local / size) * view;
    touch.pressed = Input::TickWasPressed(kTap);
    touch.released = Input::TickWasReleased(kTap);
    touch.held = Input::IsDown(kTap);
    return touch;
}

bool MagicPortalsLayer::helpBlockInput(entt::registry& registry, const Touch& touch) {
    // HelpBlockController::update (bytes 226317..228622): a touch down in a
    // block's rectangle disables the level's touches, so it fires no portal; the
    // move it makes is kept; its release, not having moved 12, opens the level's
    // popup. Not while game time is stopped, which the caller already is not.
    if (m_helpBlocks.empty()) {
        m_helpTouch = HelpTouch{};
        return false;
    }
    const std::vector<Hud::Rect> rects = HelpBlockRects();
    bool taken = false;
    if (touch.pressed && touch.over) {
        for (std::size_t i = 0; i < rects.size(); ++i) {
            if (!rects[i].Contains(touch.atView)) continue;
            m_helpTouch = HelpTouch{true, static_cast<int>(i), touch.atPx, 0.0};
            taken = true;
            break;
        }
    }
    if (!m_helpTouch.armed) return taken;
    taken = true;
    if (touch.over) m_helpTouch.maxMovePx = std::max(m_helpTouch.maxMovePx, glm::length(touch.atPx - m_helpTouch.downPx));
    if (!touch.released && touch.held) return taken;
    // Released, or gone without a release this tick saw: either way it is over.
    const HelpTouch done = m_helpTouch;
    m_helpTouch = HelpTouch{};
    if (!touch.released || done.block < 0 || static_cast<std::size_t>(done.block) >= rects.size() ||
        !rects[static_cast<std::size_t>(done.block)].Contains(touch.atView) ||
        !(done.maxMovePx < m_popupRules.helpBlock.maxMovePx)) {
        return taken;
    }
    const Chapters::Level* level = Current();
    // HelpBlockController::update #462-517: 4-01's and 4-02's blocks dispatch an
    // achievement before they open their popup - 4-02's whether or not the port
    // builds it.
    if (level != nullptr && m_achievementRulesReady) {
        if (const int id = Achievements::HelpBlockId(m_achievementRules, level->world, level->index); id >= 0) {
            unlockAchievement(id, true, m_achievementRules.helpBlockDelayMs, "a help block");
        }
    }
    bool listed = false;
    const Popup::Class* popup = level != nullptr ? Popup::HelpBlockClass(m_popupRules, level->name, listed) : nullptr;
    if (popup == nullptr) {
        // 4-02's SpaceEasterEggHelpPopup, which the port does not build (spec U6).
        SUPERSONIC_LOG_INFO("Magic Portals") << "help block released on tick " << m_ticks << ": "
                                             << (listed ? "its popup is not built" : "no popup is listed") << std::endl;
        return taken;
    }
    openPopup(registry, *popup, true);
    return taken;
}

void MagicPortalsLayer::openPopup(entt::registry& registry, const Popup::Class& cls, bool stepOnResume) {
    if (m_popup.open || m_pause.open) return;
    // A popup nobody can see must not stop the level: without its card and its
    // close button - the original's assets absent - it is said once and not
    // raised, as a HUD control whose picture is missing is not built.
    const auto drawable = [this](const std::string& file) {
        const auto found = m_popupImages.find(file);
        return found != m_popupImages.end() && !found->second.empty();
    };
    if (!drawable(cls.card.sprite) || !drawable(m_popupRules.closeButton.sprite)) {
        SUPERSONIC_LOG_WARN("Magic Portals") << "popup " << cls.name << " not raised: its card or close button "
                                             << "could not be read" << std::endl;
        return;
    }
    m_popup = PopupScreen{};
    m_popup.open = true;
    m_popup.cls = &cls;
    m_popup.state = Popup::Start(cls);
    m_popup.stepOnResume = stepOnResume;
    m_helpTouch = HelpTouch{};
    // Popup::Popup pauses g_timeManager (instructions 147..166).
    freezeWorld(registry);
    layOutControls();
    SUPERSONIC_LOG_INFO("Magic Portals") << "popup " << cls.name << " opened on tick " << m_ticks << std::endl;
}

bool MagicPortalsLayer::ClosePopup() {
    if (!m_popup.open || Popup::Closing(m_popup.state)) return false;
    Popup::Close(m_popup.state);
    SUPERSONIC_LOG_INFO("Magic Portals") << "popup closed on tick " << m_ticks << std::endl;
    return true;
}

bool MagicPortalsLayer::popupTick(entt::registry& registry, const ScriptClock::Step& step) {
    using Supersonic::Input;
    // The step the app just ran is undone before anything looks at the world.
    holdWorld(registry);
    const double dtMs = ScriptClock::Ms(step);
    m_stoppedMs += dtMs;
    m_levelFrameWallMs += ScriptClock::WallMs(step);
    Popup::Tick(*m_popup.cls, m_popup.state, dtMs);
    m_aspect = viewportAspect(registry);
    layOutControls();

    // Popup::hasReceivedCloseCommand: for a HelpPopup - every class a level raises
    // - a touch DOWN anywhere, the close button included; for any popup a press of
    // the close button, or the back key. The touch is spent on the popup and
    // reaches nothing under it.
    const Touch touch = touchThisTick(registry);
    // The touch a shot is fired by is followed under the popup as well:
    // PortalManager::update runs TouchInputManager::update before it asks whether
    // game time is stopped. So a touch that closed the popup and comes up while it
    // is still up fires nothing (the level is stopped on that frame), and one held
    // until the level has resumed fires on its release, as the original's does.
    (void)Portals::TrackTouch(m_shotTouch, touch.pressed, touch.held, touch.released, touch.over, touch.atView,
                              touch.atPx);
    (void)Portals::TrackTouch(m_handTouch, touch.pressed, touch.held, touch.released, touch.over, touch.atView,
                              touch.atPx);
    // And the camera's record of it, which the engine's input keeps under a popup
    // too: a touch held through one moves the camera, when the level resumes, by
    // what it moved on that tick alone. The camera does not run here.
    (void)Camera::Track(m_cameraTouch, touch.pressed, touch.held, touch.over, touch.atView, false);
    const bool touched = TouchClosesPopup(m_popupRules, *m_popup.cls, m_popup.state, ViewPx(), touch.pressed,
                                          touch.released, touch.over, touch.atView, m_popup.downOnClose);
    if (touched || Input::TickWasPressed(kBack)) ClosePopup();

    // Popup::update: once every sprite and the button are dismissed, the last layer
    // is current again and game time resumes - on this tick.
    if (!Popup::Gone(m_popupRules, m_popup.state)) return false;
    const bool stepsOnResume = m_popup.stepOnResume;
    thawWorld(registry);
    m_popup = PopupScreen{};
    layOutControls();
    SUPERSONIC_LOG_INFO("Magic Portals") << "popup gone, game time resumed on tick " << m_ticks << std::endl;
    return stepsOnResume;
}

void MagicPortalsLayer::tickPlaqueDismissal(bool updated) {
    // Game::loop -> dismissCurrentMedalSprite (bytes 115930..116151): once
    // getUiTime() > 2000 - a frame clock neither a pause nor a popup stops - the
    // plaque and its medal are dismissed. The fade that follows runs on GameLayer's
    // updates, so it starts from the age the level had when that happened, which
    // under a stop is the age the stop holds.
    if (m_plaqueDismissAgeMs < 0.0 && LevelFrameMs() > m_hudRules.plaque.dismissAfterMs) {
        m_plaqueDismissAgeMs =
            GameTimeStopped() ? m_levelAgeMs
                              : std::max(0.0, m_levelAgeMs - (LevelFrameMs() - m_hudRules.plaque.dismissAfterMs));
    }
    if (updated) m_plaqueAlpha = Hud::PlaqueAlphaFrom(m_hudRules, m_levelAgeMs, m_plaqueDismissAgeMs);
}

bool MagicPortalsLayer::devPressDue(DevPress press) {
    for (auto it = m_devPresses.begin(); it != m_devPresses.end(); ++it) {
        if (it->second != press || it->first > m_ticks) continue;
        SUPERSONIC_LOG_INFO("Magic Portals") << "DEV press " << static_cast<int>(press) << " taken on tick "
                                             << m_ticks << " (asked for " << it->first << ")" << std::endl;
        m_devPresses.erase(it);
        return true;
    }
    return false;
}

void MagicPortalsLayer::ScheduleDevPress(int tick, DevPress press) {
    m_devPresses.emplace_back(tick, press);
}

void MagicPortalsLayer::ScheduleDevHold(int from, int to, float direction) {
    m_devHolds.push_back(DevHold{from, to, direction});
}

void MagicPortalsLayer::ScheduleDevBurn(int tick) { m_devBurns.push_back(tick); }

void MagicPortalsLayer::ScheduleDevBreak(int tick, const std::string& node) { m_devBreaks.emplace_back(tick, node); }

void MagicPortalsLayer::ScheduleDevPlace(int tick, const glm::dvec2& atPx) { m_devPlaces.emplace_back(tick, atPx); }

// ---- how a level ends ------------------------------------------------------------

void MagicPortalsLayer::endHud() {
    // The pads decay from the byte they were last drawn with: the pulse stops
    // where it was (ScreenPad::draw, a <- uint(a * 0.98)). A weightless level
    // has none to decay.
    m_padEndByte =
        m_level.portals.noGravity ? 0 : Controls::ClassicWalkAlphaByte(m_hudRules, m_levelAgeMs, tutorialPads());
    m_clearShownAtEnd = m_level.portals.budget > 0 && !m_level.portals.placed.empty();
}

std::vector<LevelEnd::Piece> MagicPortalsLayer::endPieces(const glm::dvec2& viewUnits, double ms) const {
    if (m_screen == Screen::Finished) {
        return LevelEnd::Finished(m_levelEndRules, m_end.play, m_end.portals.current, m_end.crystals.current,
                                  viewUnits, ms);
    }
    if (m_screen == Screen::Dead) return LevelEnd::Lost(m_levelEndRules, viewUnits, ms);
    return {};
}

bool MagicPortalsLayer::endScreenTick(entt::registry& registry, const ScriptClock::Step& step) {
    using Supersonic::Input;
    // UI frame time, which this layer being current is what advances; and the
    // ScoreCounters, which count only while it is.
    const double dtMs = ScriptClock::Ms(step);
    m_end.clockMs += dtMs;
    if (m_screen == Screen::Finished) {
        m_end.portals.Tick(dtMs, m_levelEndRules.finished.counterStrideMs);
        m_end.crystals.Tick(dtMs, m_levelEndRules.finished.crystalStrideMs);
    }
    // The window may have changed shape, and the camera moved with the level.
    m_aspect = viewportAspect(registry);
    layOutMenu();

    // A tap, tested against where each button is on this tick, in view space as
    // the controls are: Button::isPointInButton, its sprite's rectangle - a
    // button still sliding in is pressable where it is.
    const auto* viewport = registry.ctx().find<Supersonic::ViewportInfo>();
    if (viewport == nullptr || !viewport->pointerOverGame || !Input::TickWasPressed(kTap)) return false;
    const glm::vec2 size = viewport->Size();
    if (size.x <= 0.0f || size.y <= 0.0f) return false;
    const glm::dvec2 view = ViewPx();
    const glm::dvec2 at = glm::dvec2(viewport->ToLocal(Input::MousePosition()) / size) * view;
    const std::optional<LevelEnd::Button> pressed = LevelEnd::ButtonAt(endPieces(view, m_end.clockMs), at);
    if (!pressed) return false;
    MenuButton button;
    switch (*pressed) {
    case LevelEnd::Button::Restart:
        button.kind = MenuButton::Kind::Retry;
        break;
    case LevelEnd::Button::Next:
        button.kind = MenuButton::Kind::Next;
        break;
    case LevelEnd::Button::List:
        button.kind = MenuButton::Kind::List;
        break;
    }
    PressMenu(registry, button);
    return true;
}

bool MagicPortalsLayer::pauseTick(entt::registry& registry, const ScriptClock::Step& step) {
    using Supersonic::Input;
    // The step the app just ran is undone before anything looks at the world.
    holdWorld(registry);
    // The AchievementsPopup is the current layer over the pause while it is up
    // (UILayerManager::openPopup): it takes the tick and every touch, and the
    // pause stands as it was under it.
    if (m_achievementsPopup.open) {
        const double popupMs = ScriptClock::Ms(step);
        m_stoppedMs += popupMs;
        m_levelFrameWallMs += ScriptClock::WallMs(step);
        m_aspect = viewportAspect(registry);
        layOutControls();
        achievementsPopupTick(registry, popupMs);
        return false;
    }
    // UI frame time runs; game time does not, and the level's frame clock counts
    // the difference.
    const double dtMs = ScriptClock::Ms(step);
    m_pause.clockMs += dtMs;
    m_stoppedMs += dtMs;
    m_levelFrameWallMs += ScriptClock::WallMs(step);
    // The window may change shape under a pause; restart and pause, drawn frozen
    // under it, stay on their corner.
    m_aspect = viewportAspect(registry);
    layOutControls();

    // The back key resumes: handleBackButton hides the pause when GameMenuLayer
    // is current.
    std::optional<Pause::Button> pressed;
    if (Input::TickWasPressed(kBack)) pressed = Pause::Button::Resume;
    const struct {
        DevPress dev;
        Pause::Button button;
    } devs[] = {{DevPress::Resume, Pause::Button::Resume},
                {DevPress::Levels, Pause::Button::Levels},
                {DevPress::Skip, Pause::Button::Skip},
                {DevPress::Achievements, Pause::Button::Achievements},
                {DevPress::Sound, Pause::Button::Sound},
                {DevPress::Music, Pause::Button::Music}};
    for (const auto& dev : devs) {
        if (!pressed && devPressDue(dev.dev)) pressed = dev.button;
    }

    // A tap, tested against where each button is on this tick, in view space as
    // the controls are: Button::isPointInButton, its sprite's rectangle.
    const auto* viewport = registry.ctx().find<Supersonic::ViewportInfo>();
    if (!pressed && viewport != nullptr && viewport->pointerOverGame && Input::TickWasPressed(kTap)) {
        const glm::vec2 size = viewport->Size();
        if (size.x > 0.0f && size.y > 0.0f) {
            const glm::dvec2 view = ViewPx();
            const glm::dvec2 at = glm::dvec2(viewport->ToLocal(Input::MousePosition()) / size) * view;
            pressed = Pause::ButtonAt(m_pauseRules, m_pause.level, PauseSwitches(), view, m_pause.clockMs, at);
        }
    }
    if (!pressed) return false;
    const bool resuming = *pressed == Pause::Button::Resume;
    PressPause(registry, *pressed);
    return resuming && !m_pause.open;
}

bool MagicPortalsLayer::PressPause(entt::registry& registry, Pause::Button button) {
    // Under the AchievementsPopup the pause is not the current layer: its buttons
    // take nothing.
    if (!m_pause.open || m_achievementsPopup.open) return false;
    switch (button) {
    case Pause::Button::Resume:
        // GameState::hideMenuPopup: the layer hidden, GameLayer current, game time
        // resumed. Silent: GameMenuLayer::GameMenuLayer #74-112 adds it with no
        // setSound (planning step 142; it played button.mp3 before).
        closePause(registry);
        return true;
    case Pause::Button::Levels:
        // returnToLevelSelect: createLevelSelectState, which is the grid this
        // level's world is on - the medal screen's list button does exactly that.
        closePause(registry);
        return PressMenu(registry, MenuButton{MenuButton::Kind::List});
    case Pause::Button::Skip:
        // goToNextLevel(levelIndex), and only on a level already finished: the
        // button is not there otherwise.
        if (m_pause.level.savedMedal <= 0) return false;
        closePause(registry);
        return PressMenu(registry, MenuButton{MenuButton::Kind::Next});
    case Pause::Button::Achievements:
        // CustomGameMenuLayer::update #38-94: the AchievementsPopup over the pause
        // (planning step 143). OWNER RULING: the button is kept on every level; on a
        // level no achievement names - where the original has no button - it opens
        // nothing. Silent, as the resume is (planning step 142).
        return openAchievementsPopup();
    case Pause::Button::Sound: {
        // GlobalSoundSwitch::manageSoundSwitch: SetGlobalVolume 1 or 0. And
        // SoundPanelLayer::manageMusicSwitch dismisses the music switch while the
        // sound is off, and adds a fresh one - a fresh entrance - once it is on
        // and the old one is gone.
        const double now = m_pause.clockMs;
        const double dismissMs = m_pauseRules.layer.buttonDismissMs;
        const bool dismissing = m_pause.musicDismissedMs >= 0.0 && now - m_pause.musicDismissedMs < dismissMs;
        m_soundOn = !m_soundOn;
        if (!m_soundOn) {
            if (!dismissing && now >= m_pause.musicAddedMs) m_pause.musicDismissedMs = now;
        } else {
            m_pause.musicAddedMs = dismissing ? m_pause.musicDismissedMs + dismissMs : now;
        }
        // Silent: a UISwitch, as on the main menu (planning step 142).
        switchedSound();
        return true;
    }
    case Pause::Button::Music: {
        const Pause::Switches switches = PauseSwitches();
        const bool shown = switches.soundOn && m_pause.clockMs >= switches.musicAddedMs &&
                           !(switches.musicDismissedMs >= 0.0 &&
                             m_pause.clockMs - switches.musicDismissedMs < m_pauseRules.layer.buttonDismissMs);
        if (!shown) return false;
        m_musicOn = !m_musicOn;
        switchedMusic();
        return true;
    }
    }
    return false;
}

// ---- per frame: nothing the level or a replay depends on --------------------

void MagicPortalsLayer::OnUpdate(entt::registry& registry, float deltaTime) {
    // As OnFixedUpdate's first line, for a frame that ran no tick.
    restoreEdgeCulls(registry);
    // What the display's rate is, from the frame the engine has just drawn: the next ticks
    // keep the engine's smoothing only where it outruns the tick (pictures.json). Nothing
    // is measured in a --fixed-step run, whose frames are the tick's own.
    if (!m_paths.fixedStep) {
        m_pictureRate.Frame(m_pictureRules, static_cast<double>(deltaTime), static_cast<double>(kTick));
    }
    // The entities' particles. They belong to the FRAME and not to the tick,
    // because a particle is a picture: nothing below may reach Game::Level,
    // the simulation's clock or the state hash. Carried before the camera's
    // early return, so a level drawn without one does not freeze them.
    if (!GameTimeStopped()) updateEmitters(registry, deltaTime);
    // And the effects the script added, which the tick queued and this frame
    // adds: the same frame, for the same reason. The eyes move first, as
    // MainCharacter::update runs updateEyes before anything else it does.
    if (!GameTimeStopped()) {
        syncEyes(registry);
        // A portal the player opened is an entity the script added, like the rest
        // of them: what is new gets its two, what has gone runs its death, and the
        // manager's colour is written before the frame draws any of it.
        syncPlacedPortals(registry, static_cast<double>(deltaTime) * 1000.0);
        updateEffects(registry, deltaTime);
    }
    // And the shock rings' scale, which is the one thing here a pause does NOT
    // hold: ETHCallback_shock_area draws it every frame, off any clock (the visuals
    // plan's 13.1). After the effects, so a frame's draws on the effects' stream come
    // in the order the frame adds them.
    jitterShockRings(registry);
    // And the lights and halos those particles brighten and dim.
    syncLights(registry);
    // And the sounds the tick latched, played here for the same reason: a
    // sound is a picture with a speaker. Both are before the camera's early
    // return, so a level drawn without one is not also silent.
    playLatched(registry, deltaTime);
    updateMusic(registry);
    // And what this frame believes it is drawing. Before the camera's early
    // return, like the rest of it.
    reportSprites(registry);
    // And the earthquake: this tick's move added to the camera the engine has
    // just drawn, and to the sky pinned to it. After everything above, which
    // places the world and never the camera; before the HUD, whose sign it moves.
    shakeView(registry);
    // And the pixel-centre rule at the frame's edge: the strips of the pictures whose edge lies in
    // its outer half pixel, from the camera and the quads exactly as they are drawn - so after the
    // shake, which is the last thing to move either.
    syncEdgeCovers(registry);
    // And the HUD, once a FRAME: the screen overlay is drawn and cleared every
    // frame, so a HUD emitted on the tick would vanish from a frame with no tick
    // and double on one with two. It reads the last tick's state and changes
    // nothing.
    EmitHud(registry);
    // And the menu states', which never share a frame with the HUD.
    EmitMenu(registry);
    // And the Notificator's card over both: main's loop draws it after the state's.
    EmitNotifications(registry);
    // Only the camera's SHAPE, for the viewport this frame is drawn into.
    if (m_camera == entt::null || !registry.valid(m_camera)) return;
    registry.get<Supersonic::CameraComponent>(m_camera).aspect = viewportAspect(registry);
}

} // namespace MagicPortals
