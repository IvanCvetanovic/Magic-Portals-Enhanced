// Every level a win route wins, won by it.
//
// A route (game/Route.hpp) is a hand-authored tests/routes/<level>.json: walks held
// over spans of the layer's ticks, taps at points of the level on stated ticks, and
// the tick by which the level must be won. This suite finds every file there at run
// time - a track adds a route without touching this code - and plays each through
// the layer as the game does: attached to a bare registry, the app's physics step
// before each of its ticks, its frame after, the pointer published over a 1280x720
// viewport, and the route's input fed through the same DEV schedule main's --route
// and --tap use, which step 100 made a real press: each tap down on its tick and up
// on its release, where its shot leaves (step 137). A route passes when
// goals.completed is set - the player in the open door - by its win_by, with no
// death, no tap that fired nothing on its release or was refused, and no tap on a
// point the view did not show. Each route's line gives its shots' largest aim drift:
// how far the point under a finger was from the point the file names, where the
// camera moved while the finger was down (Route.hpp).
//
// For writing a route, the same binary replays one and says what it did:
//   test_mp test_mp_routes --only <level or file> [--trace <every n ticks>]
// prints the player, the camera, the shots and the portals every n ticks and on
// the tick every tap comes up, and stops at the first problem. The planning record's step 113
// has the route format, and out/parity/program/W.1/ the attempts behind the seeds.
//
// THE CONTROLS CENSUS (planning step 182, CTL-6, design 5.0 G3). With `--controls modern` each route is played three times, and what the modern
// controls cost it is printed and held:
//   the CLASSIC reference run (the one above), whose every tick is recorded: the player, the camera, the shots, the portals, what else moves;
//   (b) the modern layout and draw with CLASSIC claims (`Paths::controls.classicClaims`): it must be IDENTICAL to the reference on every tick and win on
//       the same tick, with no exemption - it separates "the layout, the draw or the policy moved a simulation number" from "a claim took a tap" -
//       and, because its simulation is the classic one, it is the run in which every touch of the route can be asked what the MODERN claim would say
//       (the pure layout at that tick's age and the controls the layer shows): a swallowed touch is one that lands on a modern control, and the table
//       per route and per control is printed;
//   (a) the modern run proper: identical to the reference up to the tick before its first swallowed touch goes down, and a swallowed shot tap must
//       change something (the route stops on its problem, or the record differs from there).
// `--controls modern --classic-claims` runs only (b). `style=` is printed per route and per run and a run that did not play the style it was
// built for fails (the design's N1); the swallowed set is non-empty overall and holds the census's lower bound (tests/golden/controls_census.json:
// the levels whose pair or restart no camera can clear at 16:9). `--controls classic` runs the reference alone, with its census line.
//
// No window and no Vulkan. Skipped with 77 without the converted levels.

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "MagicPortalsLayer.hpp"
#include "Route.hpp"

#include "core/Components.hpp"
#include "core/Input.hpp"
#include "core/Json.hpp"
#include "core/PhysicsSystem.hpp"
#include "core/ViewportInfo.hpp"

#include "sim/Chapters.hpp"
#include "sim/Game.hpp"
#include "sim/LevelEnd.hpp"
#include "sim/ScriptClock.hpp"
#include "sim/Units.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

using namespace Supersonic;
using MagicPortals::MagicPortalsLayer;
namespace Route = MagicPortals::Route;
namespace Controls = MagicPortals::Controls;

namespace {

const std::string kLevels = MAGICPORTALS_LEVELS_DIR;
const std::string kData = MAGICPORTALS_DATA_DIR;
const std::string kChapters = MAGICPORTALS_CHAPTERS_FILE;
const std::string kRoutes = MAGICPORTALS_ROUTES_DIR;
// tests/golden/ (a per-source definition, as test_mp_layer's and test_mp_census's): the census's accepted list, read at run time.
const std::string kGolden = MAGICPORTALS_GOLDEN_DIR;
// Where the pointer rests between taps, as test_mp_layer rests it: a scheduled tap
// owns the pointer on its own ticks wherever this is.
const glm::vec2 kRest(640.0f, 360.0f);

// The floor on how many routes there are, so a lost directory or a bad glob fails
// rather than passing on nothing, and the three the program named first.
constexpr std::size_t kMinRoutes = 10;
const char* const kSeedLevels[] = {"level0", "level8", "level31"};

struct Options {
    std::vector<std::string> only;       // level names or route files; empty: every route
    int trace = 0;                        // print every n ticks; 0: only the outcome
    std::optional<std::string> controls;  // --controls <classic|modern>: the controls census (planning step 182)
    bool classicClaims = false;           // --classic-claims: with modern, only the variant that claims classic
    std::optional<Controls::Arrows> arrows; // --arrows <split|grouped>: the arrangement of the walk arrows the census plays (planning step 186); none: the data's, split
    std::optional<bool> twoContacts;        // --contacts <one|two>: whether the modern run reads a second finger (planning step 191); none: the data's (input.two_contacts)
};

// The flag the census plays modern under: `modern`, with the arrangement it was asked for. The shipped arrangement needs no key.
std::string ModernFlag(const Options& options) {
    std::string flag = "modern";
    if (options.arrows) flag += std::string(",arrows=") + Controls::NameOf(*options.arrows);
    if (options.twoContacts) flag += std::string(",") + Controls::TuningKey::kContacts + "=" + (*options.twoContacts ? Controls::TuningKey::kContactsTwo : Controls::TuningKey::kContactsOne);
    return flag;
}

MagicPortalsLayer::Paths RoutePaths() {
    MagicPortalsLayer::Paths paths;
    paths.prisms = std::filesystem::temp_directory_path() / "supersonic-test-mp-routes";
    return paths;
}

// The paths a census variant plays under: the policy main builds for `--controls <flag>` on a rig's `--route` run, through the real ResolvePolicy.
MagicPortalsLayer::Paths ControlsPaths(const std::string& flag, bool classicClaims) {
    MagicPortalsLayer::Paths paths = RoutePaths();
    paths.controls = Controls::ResolvePolicy(flag, false, true, false);
    CHECK_MSG(paths.controls.valid, "--controls " + flag + " resolves: " + paths.controls.error);
    paths.controls.classicClaims = classicClaims;
    return paths;
}

void PublishViewport(entt::registry& registry) {
    ViewportInfo viewport;
    viewport.rect.min = glm::vec2(0.0f, 0.0f);
    viewport.rect.max = glm::vec2(1280.0f, 720.0f);
    viewport.pointerOverGame = true;
    registry.ctx().insert_or_assign(viewport);
}

// Every sound a route's run latched, by event (planning step 142).
using Census = std::map<std::string, int>;

// ETHCallback_light_wall #57-97: its switch once its elapsedTime is above 800
// (CMPIu v6, v800), sounds.json cues.light_switch_after_ms.
constexpr double kLightSwitchAfterMs = 800.0;

// Two latches whose tick a count cannot show (planning step 142's second fix
// round): light_switch on the first tick of a light wall's fade past 800 ms, and
// fire_diamond_drained on the tick a gutter takes a fire diamond
// (ETHCallback_fire_diamond #427-455). Each tick's latches against what the level
// did on that tick; a tick where they differ is kept for the route's check.
struct Moments {
    bool wallPast = false;      // the wall's fade was past 800 ms at the last tick
    std::vector<char> fireGone; // per diamond: a fire one, gone, at the last tick
    bool marked = false;        // the first tick has been read
    int switches = 0;           // due on their tick, and so heard
    int drains = 0;
    std::string off;            // each tick whose latches were not what was due

    void Tick(const MagicPortals::Game::Level& level, const std::vector<std::string>& latched, int tick) {
        const auto heard = [&latched](const char* event) {
            return static_cast<int>(std::count(latched.begin(), latched.end(), std::string(event)));
        };
        // A wall put back and flagged again fades afresh, with its own switch.
        const bool past = level.torch.wall.going && level.torch.wall.goingMs > kLightSwitchAfterMs;
        const int switchDue = past && !wallPast ? 1 : 0;
        wallPast = past;
        // A level's first tick reads its diamonds against the level as it is made:
        // none gone.
        std::vector<char> gone;
        for (const auto& diamond : level.diamonds.diamonds) {
            gone.push_back(diamond.fire && diamond.gone ? char{1} : char{0});
        }
        if (!marked) fireGone.assign(gone.size(), char{0});
        marked = true;
        int drainDue = 0;
        if (gone.size() == fireGone.size()) {
            for (std::size_t i = 0; i < gone.size(); ++i) drainDue += gone[i] != 0 && fireGone[i] == 0 ? 1 : 0;
        }
        fireGone = std::move(gone);
        const int switched = heard("light_switch");
        const int drained = heard("fire_diamond_drained");
        if (switched != switchDue || drained != drainDue) {
            off += " tick " + std::to_string(tick) + ": light_switch " + std::to_string(switched) + " of " +
                   std::to_string(switchDue) + " due, fire_diamond_drained " + std::to_string(drained) + " of " +
                   std::to_string(drainDue) + ";";
        }
        switches += switchDue;
        drains += drainDue;
    }
};

// One frame as the app runs it under --fixed-step: the physics step, the game's tick
// with nothing pressed but what the route scheduled, the route's own tick after it,
// then the game's frame - which plays, and so clears, what the tick latched: counted
// into `census` first, and read against the tick's moments.
void Frame(MagicPortalsLayer& layer, Route::Driver& driver, entt::registry& registry, Census& census,
           Moments& moments) {
    PhysicsSystem::Update(registry, MagicPortalsLayer::kTick);
    Input::TickInput input;
    input.mousePosition = kRest;
    Input::BeginReplayedTick(input);
    layer.OnFixedUpdate(registry, MagicPortalsLayer::kTick);
    Input::EndReplayedTick();
    driver.AfterTick(layer);
    const std::vector<std::string> latched = layer.LatchedSounds();
    for (const std::string& event : latched) ++census[event];
    if (layer.SimLevel() != nullptr) moments.Tick(*layer.SimLevel(), latched, layer.LayerTicks());
    layer.OnUpdate(registry, MagicPortalsLayer::kTick);
}

// What a won route's run heard, against what its level's own counters say
// happened: each of step 142's latches at its event, once per event (sounds.json
// carries the decode). A relation that holds for every route, or for the level
// whose mechanism it is. And two latches on their very tick (Moments).
void CheckCensus(const std::string& label, const Census& census, const Moments& moments,
                 const MagicPortals::Game::Level& level) {
    const auto heard = [&census](const char* event) {
        const auto found = census.find(event);
        return found == census.end() ? 0 : found->second;
    };
    const auto pin = [&label](bool holds, const std::string& what) { CHECK_MSG(holds, label + ": " + what); };
    pin(moments.off.empty(), "light_switch on its wall's first tick past 800 ms (" + std::to_string(moments.switches) +
                                 " due) and fire_diamond_drained on its diamond's (" +
                                 std::to_string(moments.drains) + " due), on no other:" + moments.off);
    // Silent in the original, so never latched.
    for (const char* silent : {"stone_thrown", "boss_rock_broken", "boss_button_raised", "button_pressed"}) {
        pin(heard(silent) == 0, std::string(silent) + " is silent");
    }
    int timed = 0;
    for (const auto& crystal : level.goals.crystals) timed += crystal.timed ? 1 : 0;
    pin(heard("crystal_alert") == timed, "one crystal_alert per timed crystal: " +
                                             std::to_string(heard("crystal_alert")) + " of " + std::to_string(timed));
    pin(heard("bomb_blast") == level.fire.blasts, "a bomb_blast per blast");
    int spat = level.turrets.converted + level.dragon.fired + level.darkDragon.fired;
    for (const auto& turret : level.turrets.turrets) spat += turret.fired;
    pin(heard("fireball_spat") == spat, "a fireball_spat per fireball made: " + std::to_string(heard("fireball_spat")) +
                                            " of " + std::to_string(spat));
    int spent = 0;
    for (const auto& key : level.keys.keys) spent += key.spent ? 1 : 0;
    pin(heard("keyhole_vanished") == level.keys.opened, "a keyhole_vanished per door opened");
    pin(heard("door_unlocked") <= spent && heard("key_unlocked") <= heard("door_unlocked"),
        "a key's fly-in: door_unlocked then key_unlocked, for spent keys");
    pin(heard("key_picked") >= (level.keys.picked > 0 ? 1 : 0), "a key_picked where a key was taken");
    pin(heard("minion_fell") == level.minions.killed, "a minion_fell per minion the killer floor took");
    pin(heard("claw_crushed") == level.dragon.crushed, "a claw_crushed per platform the claw took");
    if (level.boss.beholder.has_value()) {
        const auto& beholder = *level.boss.beholder;
        pin(heard("boss_appeared") == 1, "the beholder appears once");
        pin(heard("boss_struck") == beholder.hits && heard("boss_hurt") == beholder.hits,
            "boss_struck and boss_hurt together, per hit: " + std::to_string(beholder.hits));
        pin(heard("boss_rock_dropped") == beholder.rocksThrown, "a boss_rock_dropped per rock");
        pin(heard("boss_dead") == (beholder.gone ? 1 : 0), "boss_dead as it goes");
        // DEAD for 3550 ms and a tick: small explosions at 400, 800 ... 3200 ms.
        if (beholder.gone) pin(heard("boss_explosion") == 8, "eight small explosions: " +
                                                                 std::to_string(heard("boss_explosion")));
    }
    if (level.dragon.present) {
        pin(heard("dragon_appeared") == 1, "the dragon appears once");
        pin(heard("dragon_took_off") == (level.dragon.tookOff ? 1 : 0), "dragon_took_off as it takes off");
    }
    // An enemy_shot as each shot leaves and an enemy_shot_ended as it ends: the chapter-3 ghost's and, since planning step 159,
    // every knight's (the same two sounds), on every level.
    pin(heard("enemy_shot") == level.ghost.shotsFired + level.minions.shotsFired, "an enemy_shot per shot");
    if (level.ghost.present && level.ghost.spawned) {
        const auto& ghost = level.ghost;
        pin(heard("ghost_appeared") == 1, "the ghost appears once");
        pin(heard("ghost_item") == ghost.beats + ghost.summons, "a ghost_item per beat and summon");
        pin(heard("ghost_hurt") >= ghost.hits, "a ghost_hurt per burn, and per rage left");
        pin(heard("ghost_burst") == (ghost.gone ? 1 : 0), "its burst as it goes");
        pin(heard("ghost_dead") == (ghost.phase == MagicPortals::Ghost::Phase::Death ? 1 : 0), "its death, once");
    }
    if (level.darkDragon.present && level.darkDragon.summoned) {
        const auto& dark = level.darkDragon;
        pin(heard("dark_dragon_appeared") == 1 && heard("dark_dragon_mouth") == 1, "it appears and opens its jaw once");
        pin(heard("dark_dragon_item") == dark.hits, "a dark_dragon_item per wound");
        pin(heard("dark_dragon_dead") == (dark.phase == MagicPortals::DarkDragon::Phase::Dying ? 1 : 0),
            "its death, once");
        pin(heard("dark_dragon_burst") == (dark.gone ? 1 : 0), "its burst as it goes");
        // Each wound's damage sound at its start and at DAMAGE's end, but the last,
        // which goes straight into DYING.
        if (dark.gone) {
            pin(heard("dark_dragon_hurt") == 2 * dark.hits - 1,
                "dark_dragon_hurt twice a wound but the last: " + std::to_string(heard("dark_dragon_hurt")));
        }
    }
}

std::string Describe(const Census& census) {
    std::string text;
    for (const auto& [event, count] : census) text += " " + event + " x" + std::to_string(count);
    return text.empty() ? " none" : text;
}

// The trace a route is written by, which main's --route-trace prints too (Route.hpp).
void Trace(const MagicPortalsLayer& layer, entt::registry& registry, const char* why) {
    Route::Trace(layer, registry, why);
}

// ---- the controls census (planning step 182) ----

// One tick of what a route's run did, as text: the player, the camera, the shots, the portals, what else moves. Two runs of the same simulation give
// the same text on every tick (the numbers are the doubles' own, printed to 12 digits); it is the `--trace 1` record, taken in process.
std::string RecordOf(const MagicPortalsLayer& layer, entt::registry& registry) {
    std::string text;
    char piece[256];
    const MagicPortals::Game::Level* level = layer.SimLevel();
    std::snprintf(piece, sizeof piece, "t %d age %.6f", layer.LayerTicks(), layer.LevelAgeMs());
    text += piece;
    if (level == nullptr) return text + " no level";
    if (level->player != entt::null && registry.valid(level->player)) {
        const auto& transform = registry.get<Supersonic::TransformComponent>(level->player);
        const glm::dvec2 at = MagicPortals::Units::ToPixels(transform.position);
        std::snprintf(piece, sizeof piece, " player (%.12g, %.12g)", at.x, at.y);
        text += piece;
        if (const auto* body = registry.try_get<Supersonic::RigidBodyComponent>(level->player)) {
            std::snprintf(piece, sizeof piece, " v (%.12g, %.12g)", static_cast<double>(body->velocity.x), static_cast<double>(body->velocity.y));
            text += piece;
        }
    }
    const glm::dvec2 camera = layer.CameraCentrePx();
    std::snprintf(piece, sizeof piece, " camera (%.12g, %.12g) shots %d/%d/%d", camera.x, camera.y, level->portals.shotsFired,
                  level->portals.shotsFailed, level->portals.shotsBurned);
    text += piece;
    if (level->portals.flight) text += " FLYING";
    for (const auto& placed : level->portals.placed) {
        std::snprintf(piece, sizeof piece, " [%.12g, %.12g]", placed.atPx.x, placed.atPx.y);
        text += piece;
    }
    for (const entt::entity body : level->portals.travellers) {
        if (body == level->player || !registry.valid(body)) continue;
        const glm::dvec2 at = MagicPortals::Units::ToPixels(registry.get<Supersonic::TransformComponent>(body).position);
        std::snprintf(piece, sizeof piece, " body (%.12g, %.12g)", at.x, at.y);
        text += piece;
    }
    for (const auto& door : level->channels.doors) {
        std::snprintf(piece, sizeof piece, " door %d %.9g", static_cast<int>(door.motion.status), static_cast<double>(door.motion.progress));
        text += piece;
    }
    for (const auto& lift : level->movers.lifts) {
        if (!registry.valid(lift.entity)) continue;
        const glm::dvec2 at = MagicPortals::Units::ToPixels(registry.get<Supersonic::TransformComponent>(lift.entity).position);
        std::snprintf(piece, sizeof piece, " lift (%.12g, %.12g)", at.x, at.y);
        text += piece;
    }
    for (const auto& ball : level->turrets.fireballs) {
        std::snprintf(piece, sizeof piece, " fire (%.12g, %.12g)", ball.atPx.x, ball.atPx.y);
        text += piece;
    }
    if (level->goals.completed) text += " DOOR";
    if (layer.Dying()) text += " DYING";
    return text;
}

// The control the MODERN claim would give a touch at `at` (view units) on the tick whose level age is `ageMs`: what the layer's own claim is on a
// modern layer - the five frames the module lays out, the corner buttons following their entrance, the controls the layer SHOWS, the first in claim
// order that holds the point (half-open). Asked of a layer whose simulation is the classic one (variant b), so that a route's every touch can be
// asked, not only those before the first that a modern claim changes.
std::optional<MagicPortalsLayer::Control> ModernClaim(const MagicPortalsLayer& layer, const glm::dvec2& at, double ageMs) {
    const glm::dvec2 view = layer.ViewPx();
    const Controls::Insets insets = layer.ControlsInsets();
    const Controls::Layout layout = Controls::LayOut(Controls::Style::Modern, layer.ControlsRules(), layer.HudRules(), layer.ControlsApplied(), view, insets, ageMs);
    for (const Controls::Id id : Controls::kClaimOrder) {
        MagicPortals::Hud::Rect shown;
        if (!layer.ControlRect(id, shown)) continue;
        Controls::Frame frame = layout.At(id);
        if (id == Controls::Id::Reset || id == Controls::Id::Menu) {
            const MagicPortals::Hud::Placement placement =
                Controls::PlacementFor(layer.ControlsRules(), layer.HudRules(), layer.ControlsApplied(), id, view, insets);
            const MagicPortals::LevelEnd::Dismissed in =
                MagicPortals::LevelEnd::HudEntered(layer.LevelEndRules(), placement, layer.HudRules().alphaByte, view, ageMs);
            frame = Controls::FollowDrawn(frame, in.rect);
        }
        if (frame.hit.Contains(at)) return id;
    }
    return std::nullopt;
}

// A touch of a route that the modern claim would take.
struct Swallowed {
    std::string label; // "1-20"
    std::string level;
    std::size_t tap = 0;
    int tick = 0;     // the tick it goes down on
    const char* kind = "tap";
    std::string control; // the control that takes it (walk_left ...), or "gate": no control takes it but the walk controls' gate (camera, lift, hand) differs
    bool claimed = true;
    glm::dvec2 at{0.0}; // on the view, units
    std::string camera; // where the camera stands against the level's edges
};

// Which edges of the level the view touches, as the census speaks of a camera's clamp: "free" or any of L R T B.
std::string ClampState(const MagicPortalsLayer& layer) {
    const glm::dvec2 centre = layer.CameraCentrePx();
    const glm::dvec2 half = layer.ViewPx() * 0.5;
    const glm::dvec2 bounds = layer.BoundsPx();
    constexpr double eps = 1e-6;
    std::string state;
    if (centre.x - half.x <= eps) state += "L";
    if (centre.x + half.x >= bounds.x - eps) state += "R";
    if (centre.y - half.y <= eps) state += "T";
    if (centre.y + half.y >= bounds.y - eps) state += "B";
    return state.empty() ? "free" : state;
}

// What a census run keeps of a route's play.
struct Capture {
    std::vector<std::string> records; // after each tick, in order: records[0] is tick 1
    std::vector<Swallowed> swallowed; // when classifying: every touch the modern claim would take, in the order they go down
    Controls::Style style = Controls::Style::Classic;
    bool ready = false;
    std::optional<Controls::Arrows> arrows; // the arrangement of the walk arrows the layer actually laid out (planning step 187), when it plays modern
    bool twoContacts = false;               // whether the layer read a second finger (planning step 191): the modern run with the data's switch on, never the classic-claims variant
};

// The touches of the plan that go down on `tick`, asked what the modern claim says of each (a tap at its place; a drag at every tick it is held),
// from the state the previous tick left - the state Driver::schedule reads them in. Not while a popup, a pause or an ending holds the controls.
void Classify(const MagicPortalsLayer& layer, const Route::Plan& plan, const std::string& label, int tick, Capture& capture) {
    if (layer.PopupOpen() || layer.Paused() || layer.Finishing() || layer.Dying()) return;
    const glm::dvec2 view = layer.ViewPx();
    const glm::dvec2 corner = layer.CameraCentrePx() - view * 0.5;
    for (std::size_t i = 0; i < plan.taps.size(); ++i) {
        const Route::Tap& tap = plan.taps[i];
        if (tap.tick != tick) continue;
        // The fractions as Driver::schedule takes them.
        const glm::dvec2 fraction = (tap.atPx - corner) / view;
        if (!(fraction.x > 0.0 && fraction.x < 1.0 && fraction.y > 0.0 && fraction.y < 1.0)) continue;
        std::vector<glm::dvec2> path;
        if (tap.toPx) {
            const glm::dvec2 toFraction = (*tap.toPx - corner) / view;
            const double held = static_cast<double>(tap.releaseTick - tap.tick);
            const glm::dvec2 end = fraction + (toFraction - fraction) * (held / (held - 1.0));
            for (int k = 0; k < tap.releaseTick - tap.tick; ++k) {
                const double along = static_cast<double>(k) / held;
                path.push_back((fraction + (end - fraction) * along) * view);
            }
        } else {
            path.push_back(fraction * view);
        }
        for (std::size_t k = 0; k < path.size(); ++k) {
            // The layout of the tick the touch is read on: the level is a script tick older for each tick from the one the state is of. The script's
            // tick (the floor of 16.667 ms, sim/ScriptClock) the layer itself advances the level's age by, asked of the layer and not written here.
            const double age = layer.LevelAgeMs() + MagicPortals::ScriptClock::Ms(layer.TickStep()) * static_cast<double>(1 + static_cast<int>(k));
            const std::optional<MagicPortalsLayer::Control> claim = ModernClaim(layer, path[k], age);
            // No control takes it, but the walk controls' gate - the camera's `onPad`, the lift's and the hand's - is asked of the rectangles under the
            // modern controls and of the discs under the classic ones, whether or not the pair is shown (design D12): a touch where the two differ
            // moves the camera, a lift or a hand differently on a weightless level, where no pair claims anything.
            const Controls::Layout modernLayout =
                Controls::LayOut(Controls::Style::Modern, layer.ControlsRules(), layer.HudRules(), layer.ControlsApplied(), view, layer.ControlsInsets(), age);
            const bool modernGate = modernLayout.At(Controls::Id::Left).hit.Contains(path[k]) || modernLayout.At(Controls::Id::Right).hit.Contains(path[k]);
            const bool classicGate = Controls::ClassicOnWalkControl(layer.HudRules(), view, age, path[k]);
            if (!claim && modernGate == classicGate) continue;
            Swallowed one;
            one.label = label;
            one.level = plan.level;
            one.tap = i;
            one.tick = tap.tick;
            one.kind = tap.toPx ? "drag" : (tap.shot ? "shot tap" : "touch");
            one.claimed = claim.has_value();
            one.control = claim ? std::string(Controls::KeyOf(*claim)) : std::string("gate");
            one.at = path[k];
            one.camera = ClampState(layer);
            capture.swallowed.push_back(one);
            break; // one entry per touch: the first tick of it that a control takes
        }
    }
}

struct Outcome {
    int wonTick = 0;
    int shots = 0;
    int held = 0;
    double driftPx = 0.0;
    std::vector<std::string> problems;
    Census census;
    bool censusChecked = false;
    // The achievements the run unlocked, and what its door asked (planning step 143).
    std::vector<MagicPortalsLayer::Unlock> unlocks;
    std::optional<MagicPortals::Achievements::Play> door;
};

// Every unlock of a won route is one its own level's rules or its chapter's
// badges name, at its door with a card - or the red key's, before it - and every
// rule of its level whose terms are all the medal's fires exactly when the door's
// medal passes them: the evaluator against the plain arithmetic, in play.
void CheckAchievements(const std::string& label, const Outcome& outcome,
                       const MagicPortals::Achievements::Rules& rules) {
    namespace Achievements = MagicPortals::Achievements;
    CHECK_MSG(outcome.door.has_value(), label + ": its door asked for achievements");
    if (!outcome.door) return;
    const Achievements::Play& door = *outcome.door;
    const auto unlocked = [&outcome](int id) {
        return std::any_of(outcome.unlocks.begin(), outcome.unlocks.end(),
                           [id](const MagicPortalsLayer::Unlock& unlock) { return unlock.id == id; });
    };
    for (const MagicPortalsLayer::Unlock& unlock : outcome.unlocks) {
        bool named = unlock.id == rules.key.id && unlock.tick <= outcome.wonTick;
        for (const Achievements::LevelRule& rule : rules.level) {
            named = named || (rule.id == unlock.id && rule.world == door.world && rule.level == door.level &&
                              unlock.tick == outcome.wonTick);
        }
        if (static_cast<std::size_t>(door.world) < rules.completion.size()) {
            for (const Achievements::CompletionStep& step : rules.completion[static_cast<std::size_t>(door.world)]) {
                named = named || (step.id == unlock.id && unlock.tick == outcome.wonTick);
            }
        }
        CHECK_MSG(named && unlock.notify, label + ": achievement " + std::to_string(unlock.id) + " on tick " +
                                              std::to_string(unlock.tick) + " is its own, with a card");
    }
    for (const Achievements::LevelRule& rule : rules.level) {
        if (rule.world != door.world || rule.level != door.level) continue;
        bool medalOnly = true;
        bool passes = true;
        for (const Achievements::Term& term : rule.terms) {
            if (term.kind == Achievements::TermKind::MedalAtLeast) {
                passes = passes && door.medal >= static_cast<int>(term.value);
            } else if (term.kind == Achievements::TermKind::MedalAbove) {
                passes = passes && door.medal > static_cast<int>(term.value);
            } else {
                medalOnly = false;
            }
        }
        if (!medalOnly) continue;
        CHECK_MSG(unlocked(rule.id) == passes, label + ": achievement " + std::to_string(rule.id) +
                                                   (passes ? " unlocked" : " not unlocked") + " at medal " +
                                                   std::to_string(door.medal));
    }
}

std::string DescribeUnlocks(const Outcome& outcome) {
    std::string text;
    for (const MagicPortalsLayer::Unlock& unlock : outcome.unlocks) {
        text += " " + std::to_string(unlock.id) + "@" + std::to_string(unlock.tick);
    }
    if (text.empty()) text = " none";
    if (outcome.door) {
        char play[96];
        std::snprintf(play, sizeof play, " (door: medal %d, %d portals, %.1f ms)", outcome.door->medal,
                      outcome.door->portals, outcome.door->gameTimeMs);
        text += play;
    }
    return text;
}

Outcome Play(const Route::Plan& plan, const Options& options, const std::string& label,
             const MagicPortalsLayer::Paths& paths = RoutePaths(), Capture* capture = nullptr, bool classify = false) {
    entt::registry registry;
    PublishViewport(registry);
    MagicPortalsLayer layer(paths, plan.level);
    layer.OnAttach(registry);
    Route::Driver driver(plan);
    driver.Begin(layer);
    int lastFailed = -1;
    Census census;
    Moments moments;
    if (capture != nullptr) {
        capture->style = layer.ControlsStyle();
        capture->ready = layer.ModernReady();
        if (capture->ready) capture->arrows = layer.ControlsApplied().ArrowsIn(layer.ControlsRules());
        capture->twoContacts = layer.TwoContactsOn();
        if (classify) Classify(layer, plan, label, layer.LayerTicks() + 1, *capture);
    }
    while (!driver.Finished()) {
        Frame(layer, driver, registry, census, moments);
        if (capture != nullptr) {
            capture->records.push_back(RecordOf(layer, registry));
            if (classify) Classify(layer, plan, label, layer.LayerTicks() + 1, *capture);
        }
        if (options.trace <= 0) continue;
        const int tick = layer.LayerTicks();
        // The tick a tap comes up on, where its shot has just left.
        const bool tapped = Route::ReleasesOn(plan, tick);
        const MagicPortals::Game::Level* level = layer.SimLevel();
        const int failed = level != nullptr ? level->portals.shotsFailed : lastFailed;
        if (tapped) {
            Trace(layer, registry, "TAP");
            if (!driver.Aims().empty() && driver.Aims().back().tick == tick) {
                std::printf("          the aim's drift %.3f px\n", driver.Aims().back().driftPx);
            }
        } else if (failed != lastFailed && lastFailed >= 0) {
            Trace(layer, registry, "FAIL");
        } else if (tick % options.trace == 0 || driver.Finished()) {
            Trace(layer, registry, driver.WonTick() > 0 ? "WON" : "");
        }
        lastFailed = failed;
    }
    Outcome out;
    out.wonTick = driver.WonTick();
    out.shots = driver.Shots();
    out.held = driver.Held();
    out.driftPx = driver.MaxDriftPx();
    out.problems = driver.Problems();
    out.census = std::move(census);
    out.unlocks = layer.Unlocks();
    out.door = layer.LastDoorPlay();
    if (out.wonTick > 0 && layer.SimLevel() != nullptr) {
        CheckCensus(label + " " + plan.level, out.census, moments, *layer.SimLevel());
        out.censusChecked = true;
    }
    layer.OnDetach(registry);
    return out;
}

std::vector<std::filesystem::path> Discover() {
    std::vector<std::filesystem::path> files;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(kRoutes, ec)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") files.push_back(entry.path());
    }
    std::sort(files.begin(), files.end());
    return files;
}

// "1-09", as the backlog and the captures name a level.
std::string Label(const MagicPortals::Chapters::Table& chapters, const std::string& level) {
    const int at = chapters.Find(level);
    if (at < 0) return "?";
    const auto& entry = chapters.levels[static_cast<std::size_t>(at)];
    char text[16];
    std::snprintf(text, sizeof text, "%d-%02d", entry.world + 1, entry.index + 1);
    return text;
}

// ---- the census of a route: the two modern variants against the classic reference ----

// The first tick on which two records differ (1-based), or 0 when `a` and `b` agree on `upTo` ticks; the texts of both where they differ.
int FirstDifference(const std::vector<std::string>& a, const std::vector<std::string>& b, std::size_t upTo, std::string& said) {
    for (std::size_t tick = 0; tick < upTo; ++tick) {
        const bool haveA = tick < a.size();
        const bool haveB = tick < b.size();
        if (!haveA || !haveB || a[tick] != b[tick]) {
            said = "tick " + std::to_string(tick + 1) + ": reference [" + (haveA ? a[tick] : std::string("(none)")) + "] against [" +
                   (haveB ? b[tick] : std::string("(none)")) + "]";
            return static_cast<int>(tick) + 1;
        }
    }
    return 0;
}

// What the whole census holds.
struct CensusTable {
    std::vector<Swallowed> all;
    std::map<std::string, std::map<std::string, int>> byRoute; // label -> control -> count
    int routesRun = 0;
    int modernRan = 0;
    int classicClaimsIdentical = 0;
    int modernIdenticalToTheEnd = 0;
    int modernDiverged = 0;
    std::optional<Controls::Arrows> played; // the arrangement the layer laid out, which every run of the census must agree on (planning step 187)
    std::optional<bool> twoContacts;        // whether the modern run read a second finger (planning step 191): the census says which it played
};

// The arrangement a run played is the layer's own, not the one the flag was asked for: a data file whose default is grouped plays grouped under a flag with no
// `arrows=` key, and a census that read the answer from the flag would then check the wrong arrangement's lists. Every run agrees with the first and with the flag
// where the flag named one (N1 for the arrangement).
void NoteArrangement(const std::string& name, const char* run, const Capture& capture, const Options& options, CensusTable& table) {
    CHECK_MSG(capture.arrows.has_value(), name + ": the " + run + " run reports the arrangement it played");
    if (!capture.arrows) return;
    if (options.arrows) CHECK_MSG(*capture.arrows == *options.arrows, name + ": the " + run + " run lays the arrows out as asked: " + Controls::NameOf(*capture.arrows));
    if (!table.played) table.played = capture.arrows;
    CHECK_MSG(*capture.arrows == *table.played, name + ": the " + run + " run plays the arrangement every run plays: " + Controls::NameOf(*capture.arrows));
}

void CensusRoute(const Route::Plan& plan, const Options& options, const std::string& label, const Capture& reference, const Outcome& referenceOutcome,
                 CensusTable& table) {
    const std::string name = label + " " + plan.level;
    ++table.routesRun;
    CHECK_MSG(reference.style == Controls::Style::Classic, name + ": the reference run plays the classic controls: style=" + Controls::NameOf(reference.style));
    if (!options.controls || *options.controls == "classic") return;
    const std::size_t ticks = reference.records.size();

    // (b) the modern layout and draw, the classic claims: identical to the reference, whole, with no exemption.
    Capture claims;
    const Outcome classicClaims = Play(plan, Options{}, label, ControlsPaths(ModernFlag(options), true), &claims, true);
    // (And it reads ONE finger whatever the switch says: the variant must stay the classic simulation to the last tick.)
    const bool bPlayedModern = claims.style == Controls::Style::Modern && claims.ready && !claims.twoContacts;
    CHECK_MSG(bPlayedModern, name + ": the classic-claims variant plays the MODERN layout (N1): style=" + Controls::NameOf(claims.style));
    NoteArrangement(name, "classic-claims", claims, options, table);
    std::string said;
    const int bDiffers = FirstDifference(reference.records, claims.records, std::max(ticks, claims.records.size()), said);
    CHECK_MSG(bDiffers == 0 && classicClaims.problems.empty() && classicClaims.wonTick == referenceOutcome.wonTick,
              name + ": modern layout and draw with classic claims is the reference on every tick and wins on tick " + std::to_string(referenceOutcome.wonTick) +
                  " (won on " + std::to_string(classicClaims.wonTick) + "), " + said + (classicClaims.problems.empty() ? std::string() : "; problem: " + classicClaims.problems.front()));
    if (bDiffers == 0 && classicClaims.wonTick == referenceOutcome.wonTick) ++table.classicClaimsIdentical;

    // What the modern claim would take of this route, touch by touch, in the identical simulation.
    std::string tableText;
    for (const Swallowed& one : claims.swallowed) {
        table.all.push_back(one);
        ++table.byRoute[name][one.control];
        char piece[192];
        std::snprintf(piece, sizeof piece, "%s %s down on tick %d at (%.1f, %.1f) units, camera %s -> %s", one.kind, plan.level.c_str(), one.tick, one.at.x, one.at.y,
                      one.camera.c_str(), one.claimed ? one.control.c_str() : "no control, the walk gate differs");
        tableText += std::string("    swallowed: ") + piece + "\n";
    }
    if (options.classicClaims) {
        std::printf("  %s: classic claims identical (%zu ticks), style=%s; the modern claim would take %zu touch(es)\n%s", name.c_str(), ticks,
                    Controls::NameOf(claims.style), claims.swallowed.size(), tableText.c_str());
        return;
    }

    // (a) the modern run proper.
    ++table.modernRan;
    Capture modern;
    const Outcome modernOutcome = Play(plan, Options{}, label, ControlsPaths(ModernFlag(options), false), &modern, false);
    // (And, when --contacts asked for one, it reads the fingers it was asked to: a census of "the switch off" that ran with it on proves nothing.)
    CHECK_MSG(modern.style == Controls::Style::Modern && modern.ready && (!options.twoContacts || modern.twoContacts == *options.twoContacts),
              name + ": the modern run plays the MODERN controls (N1): style=" + Controls::NameOf(modern.style) + ", two contacts " + (modern.twoContacts ? "on" : "off"));
    NoteArrangement(name, "modern", modern, options, table);
    table.twoContacts = modern.twoContacts;
    const Swallowed* first = claims.swallowed.empty() ? nullptr : &claims.swallowed.front();
    const std::size_t safe = first != nullptr ? static_cast<std::size_t>(std::max(first->tick - 1, 0)) : std::max(ticks, modern.records.size());
    std::string saidModern;
    const int firstDifference = FirstDifference(reference.records, modern.records, safe, saidModern);
    CHECK_MSG(firstDifference == 0, name + ": the modern run is the reference on every tick before its first swallowed touch goes down (" +
                                        (first != nullptr ? "tick " + std::to_string(first->tick) : std::string("it has none")) + "): " + saidModern);
    std::string wholeSaid;
    const int wholeDifference = FirstDifference(reference.records, modern.records, std::max(ticks, modern.records.size()), wholeSaid);
    const bool identicalToTheEnd = wholeDifference == 0 && modernOutcome.problems.empty() && modernOutcome.wonTick == referenceOutcome.wonTick;
    if (first == nullptr) {
        CHECK_MSG(identicalToTheEnd, name + ": it has no swallowed touch, so the modern run wins on tick " + std::to_string(referenceOutcome.wonTick) + " with the reference's every tick: " +
                                         wholeSaid + (modernOutcome.problems.empty() ? std::string() : "; problem: " + modernOutcome.problems.front()));
        if (identicalToTheEnd) ++table.modernIdenticalToTheEnd;
    } else {
        ++table.modernDiverged;
        // A swallowed SHOT tap changes the route: it stops on its problem, or the record differs from the tick it goes down on.
        if (first->claimed && std::string(first->kind) == "shot tap") {
            CHECK_MSG(!identicalToTheEnd, name + ": a swallowed shot tap (tick " + std::to_string(first->tick) + ", " + first->control +
                                              ") changed nothing in the modern run: the claim and the model disagree");
        }
    }
    char line[512];
    std::snprintf(line, sizeof line, "  %s %s: classic won on tick %d; classic claims identical; modern: %s", label.c_str(), plan.level.c_str(), referenceOutcome.wonTick,
                  first == nullptr ? "no touch taken, identical to the end"
                                   : (std::string("first touch taken on tick ") + std::to_string(first->tick) + " by " + first->control + "; " +
                                      (modernOutcome.wonTick > 0 ? "still won on tick " + std::to_string(modernOutcome.wonTick)
                                                                 : "stopped" + (modernOutcome.problems.empty() ? std::string() : ": " + modernOutcome.problems.front())))
                                         .c_str());
    std::printf("%s; style=%s\n%s", line, Controls::NameOf(modern.style), tableText.c_str());
}

// The accepted list's lower bound: the levels whose walk arrows or restart no camera can clear at 16:9 (tests/golden/controls_census.json,
// `arrangements.<name>.shapes`). Grouped (planning step 180) the arrows cost 1-20 and 3-27; split (planning step 186) they cost none, so the lower bound
// for the walk arrows is empty and the census has no forced arrow touch to find.
struct ForcedList {
    bool read = false;
    std::vector<std::string> walk; // the pair's list (grouped) or the union of walk_left's and walk_right's (split)
    std::vector<std::string> restart;
    std::vector<std::string> clearPortals; // an upper bound: clear-portals exists only while a portal does
};

ForcedList ReadForced(Controls::Arrows arrows) {
    ForcedList forced;
    std::ifstream file(kGolden + "/controls_census.json", std::ios::binary);
    if (!file) return forced;
    std::ostringstream text;
    text << file.rdbuf();
    const std::string content = text.str();
    Supersonic::Json::Value root;
    Supersonic::Json::Parser parser(content);
    if (!parser.Parse(root) || !root.IsObject() || !root["arrangements"].IsObject() || !root["arrangements"][Controls::NameOf(arrows)].IsObject()) return forced;
    const Supersonic::Json::Value& block = root["arrangements"][Controls::NameOf(arrows)];
    if (!block["shapes"].IsArray()) return forced;
    for (const Supersonic::Json::Value& shape : block["shapes"].AsArray()) {
        if (!shape.IsObject() || !shape["name"].IsString() || shape["name"].AsString() != "16:9" || !shape["blocked"].IsObject()) continue;
        const auto labels = [&shape](const char* key) {
            std::vector<std::string> out;
            if (shape["blocked"][key].IsArray()) {
                for (const Supersonic::Json::Value& value : shape["blocked"][key].AsArray()) {
                    if (value.IsString()) out.push_back(value.AsString());
                }
            }
            return out;
        };
        for (const char* key : {"pair", "walk_left", "walk_right"}) {
            for (const std::string& label : labels(key)) {
                if (std::find(forced.walk.begin(), forced.walk.end(), label) == forced.walk.end()) forced.walk.push_back(label);
            }
        }
        forced.restart = labels("restart");
        forced.clearPortals = labels("clear_portals");
        forced.read = true;
    }
    return forced;
}

// The table, per route and per control, and the checks that hold the whole census.
void ReportCensus(const Options& options, const CensusTable& table) {
    std::map<std::string, int> perControl;
    for (const Swallowed& one : table.all) ++perControl[one.control];
    if (table.twoContacts) std::printf("  the modern run reads %s (input.two_contacts / --contacts)\n", *table.twoContacts ? "two fingers" : "one finger");
    std::printf("  controls census, --controls %s%s: %d route(s); the classic claims were identical on %d; ", options.controls ? options.controls->c_str() : "?",
                options.classicClaims ? " --classic-claims" : "", table.routesRun, table.classicClaimsIdentical);
    if (!options.classicClaims) {
        std::printf("the modern run: %d with no touch taken (identical to the end: %d), %d with one taken; ", table.modernRan - table.modernDiverged,
                    table.modernIdenticalToTheEnd, table.modernDiverged);
    }
    std::printf("%zu touch(es) the modern claim takes in %zu route(s):", table.all.size(), table.byRoute.size());
    for (const auto& [control, count] : perControl) std::printf(" %s %d", control.c_str(), count);
    std::printf("\n");
    for (const auto& [route, controls] : table.byRoute) {
        std::printf("    %s:", route.c_str());
        for (const auto& [control, count] : controls) std::printf(" %s x%d", control.c_str(), count);
        std::printf("\n");
    }
    CHECK_MSG(table.classicClaimsIdentical == table.routesRun, "the classic-claims variant is identical to the reference on every route, with no exemption: " +
                                                                  std::to_string(table.classicClaimsIdentical) + " of " + std::to_string(table.routesRun));
    // N1: the swallowed set is not empty (a census that finds nothing may be a census that ran classic).
    CHECK_MSG(!table.all.empty(), "the swallowed set is non-empty over all the routes");
    if (!options.only.empty()) return;
    // The lower bound: what the census proves unavoidable at 16:9 must appear (the walk arrows' levels by an arrow, restart's by restart). The split
    // arrows have no forced level, so for them the list is empty and only restart's is required.
    // The arrangement the layer PLAYED, which a data file's default may make other than the flag's: read from the runs, so that the lists are the right ones.
    CHECK_MSG(table.played.has_value(), "the census saw the arrangement the layer played");
    const Controls::Arrows played = table.played.value_or(options.arrows.value_or(Controls::Arrows::Split));
    const ForcedList forced = ReadForced(played);
    CHECK_MSG(forced.read && !forced.restart.empty() && (played == Controls::Arrows::Split || !forced.walk.empty()),
              std::string("tests/golden/controls_census.json holds the 16:9 lists of the ") + Controls::NameOf(played) + " arrangement");
    const auto takenBy = [&table](const std::string& label, std::initializer_list<const char*> controls) {
        for (const Swallowed& one : table.all) {
            if (one.label != label || !one.claimed) continue;
            for (const char* control : controls) {
                if (one.control == control) return true;
            }
        }
        return false;
    };
    for (const std::string& label : forced.walk) {
        CHECK_MSG(takenBy(label, {"walk_left", "walk_right"}), label + ": the census's forced list, the walk arrows, appears in the swallowed set");
    }
    // The headline of Step 186, ASSERTED (it was only printed): the split arrows take no recorded shot of the 128 routes, and the left arrow takes none in either
    // arrangement - the grouped pair's touches are all its right arrow's.
    int byLeft = 0;
    int byRight = 0;
    for (const Swallowed& one : table.all) {
        if (!one.claimed) continue;
        byLeft += one.control == "walk_left" ? 1 : 0;
        byRight += one.control == "walk_right" ? 1 : 0;
    }
    CHECK_MSG(byLeft == 0, std::string("no recorded touch is taken by walk_left in the ") + Controls::NameOf(played) + " arrangement: " + std::to_string(byLeft));
    if (played == Controls::Arrows::Split) {
        CHECK_MSG(byRight == 0, "and none by walk_right: the split arrows cost the 128 recorded winning routes nothing: " + std::to_string(byRight));
    } else {
        CHECK_MSG(byRight > 0, "the grouped pair's right arrow takes the touches the split arrangement gave back: " + std::to_string(byRight));
    }
    std::printf("  the census's forced list for the %s walk arrows at 16:9: %zu level(s)\n", Controls::NameOf(played), forced.walk.size());
    for (const std::string& label : forced.restart) {
        CHECK_MSG(takenBy(label, {"restart"}), label + ": the census's forced list, restart, appears in the swallowed set");
    }
    // clear-portals' levels are an upper bound (it exists only while a portal is alive): reported, not required.
    for (const std::string& label : forced.clearPortals) {
        std::printf("  %s: on the census's clear-portals list (an upper bound): %s in the swallowed set\n", label.c_str(),
                    takenBy(label, {"clear_portals"}) ? "IS" : "is not");
    }
}

void RunRoutes(const Options& options) {
    MagicPortals::Chapters::Table chapters;
    std::string error;
    CHECK_MSG(MagicPortals::Chapters::Load(kChapters, chapters, error), error);
    MagicPortals::Achievements::Rules achievementRules;
    const bool rulesRead = MagicPortals::Achievements::LoadRules(
        std::string(MAGICPORTALS_PORT_DATA_DIR) + "/achievements.json", achievementRules, error);
    CHECK_MSG(rulesRead, error);

    std::vector<std::filesystem::path> files;
    if (options.only.empty()) {
        files = Discover();
        std::printf("  %zu route(s) in %s\n", files.size(), kRoutes.c_str());
        CHECK_MSG(files.size() >= kMinRoutes, "at least " + std::to_string(kMinRoutes) + " routes, found " +
                                                  std::to_string(files.size()));
        for (const char* seed : kSeedLevels) {
            const bool there = std::any_of(files.begin(), files.end(), [&](const std::filesystem::path& file) {
                return file.stem().string() == seed;
            });
            CHECK_MSG(there, std::string("the seed route ") + seed + ".json is there");
        }
    } else {
        for (const std::string& what : options.only) {
            const std::filesystem::path given(what);
            files.push_back(given.has_extension() ? given : std::filesystem::path(kRoutes) / (what + ".json"));
        }
    }

    CensusTable table;
    for (const std::filesystem::path& file : files) {
        Route::Plan plan;
        if (!Route::Load(file.string(), plan, error)) {
            CHECK_MSG(false, error);
            continue;
        }
        CHECK_MSG(file.stem().string() == plan.level,
                  file.filename().string() + " is named for its level, " + plan.level);
        const std::string label = Label(chapters, plan.level);
        Capture reference;
        const Outcome outcome = Play(plan, options, label, RoutePaths(), options.controls ? &reference : nullptr, false);
        // With the entries the platform gate held on the way, where it held any (step 136).
        const std::string held = outcome.held > 0 ? ", held " + std::to_string(outcome.held) : std::string();
        if (outcome.wonTick > 0) {
            std::printf("  %s %s: won on tick %d (win_by %d), %d shot(s), the aim's drift at most %.3f px%s\n",
                        label.c_str(), plan.level.c_str(), outcome.wonTick, plan.winBy, outcome.shots,
                        outcome.driftPx, held.c_str());
        } else {
            std::printf("  %s %s: NOT WON%s\n", label.c_str(), plan.level.c_str(), held.c_str());
        }
        // What it heard (planning step 142), on a line of its own.
        std::printf("  %s %s sounds:%s\n", label.c_str(), plan.level.c_str(), Describe(outcome.census).c_str());
        // And what it unlocked (planning step 143).
        std::printf("  %s %s achievements:%s\n", label.c_str(), plan.level.c_str(), DescribeUnlocks(outcome).c_str());
        if (outcome.wonTick > 0 && rulesRead) CheckAchievements(label + " " + plan.level, outcome, achievementRules);
        std::string problems;
        for (const std::string& problem : outcome.problems) problems += (problems.empty() ? "" : "; ") + problem;
        CHECK_MSG(outcome.problems.empty(), label + " " + plan.level + ": " + problems);
        CHECK_MSG(outcome.wonTick > 0 && outcome.wonTick <= plan.winBy,
                  label + " " + plan.level + " is won by tick " + std::to_string(plan.winBy));
        if (options.controls) CensusRoute(plan, options, label, reference, outcome, table);
    }
    if (options.controls && *options.controls == "modern") ReportCensus(options, table);
}

} // namespace

MP_SUITE_MAIN_ARGS("test_mp_routes", argc, argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--only" && i + 1 < argc) {
            options.only.push_back(argv[++i]);
        } else if (arg == "--trace" && i + 1 < argc) {
            options.trace = std::max(1, std::atoi(argv[++i]));
        } else if (arg == "--controls" && i + 1 < argc) {
            options.controls = argv[++i];
            if (*options.controls != "classic" && *options.controls != "modern") {
                std::printf("test_mp_routes: --controls wants classic or modern, not '%s'\n", options.controls->c_str());
                return 2;
            }
        } else if (arg == "--classic-claims") {
            options.classicClaims = true;
        } else if (arg == "--arrows" && i + 1 < argc) {
            options.arrows = Controls::ArrowsOfName(argv[++i]);
            if (!options.arrows) {
                std::printf("test_mp_routes: --arrows wants split or grouped\n");
                return 2;
            }
        } else if (arg == "--contacts" && i + 1 < argc) {
            const std::string which = argv[++i];
            if (which != Controls::TuningKey::kContactsOne && which != Controls::TuningKey::kContactsTwo) {
                std::printf("test_mp_routes: --contacts wants one or two\n");
                return 2;
            }
            options.twoContacts = which == Controls::TuningKey::kContactsTwo;
        } else {
            std::printf("test_mp test_mp_routes [--only <level or route file>]... [--trace <every n ticks>] [--controls <classic|modern> [--classic-claims] [--arrows <split|grouped>] [--contacts <one|two>]]\n");
            return 2;
        }
    }
    std::error_code ec;
    if (!std::filesystem::is_directory(kLevels, ec) || !std::filesystem::is_regular_file(kChapters, ec) ||
        !std::filesystem::is_regular_file(kData + "/entity_roles.json", ec)) {
        std::printf("test_mp_routes: SKIPPED - needs the converted levels at %s,\n"
                    "  chapters.json at %s and Enhanced's data at %s.\n",
                    kLevels.c_str(), kChapters.c_str(), kData.c_str());
        return 77;
    }
    if (options.classicClaims && (!options.controls || *options.controls != "modern")) {
        std::printf("test_mp_routes: --classic-claims is the variant of --controls modern\n");
        return 2;
    }
    if (options.arrows && (!options.controls || *options.controls != "modern")) {
        std::printf("test_mp_routes: --arrows is the arrangement of --controls modern\n");
        return 2;
    }
    if (options.twoContacts && (!options.controls || *options.controls != "modern")) {
        std::printf("test_mp_routes: --contacts is the reading of fingers of --controls modern\n");
        return 2;
    }
    RunRoutes(options);
    return ::test::summary("test_mp_routes", options.only.empty() ? static_cast<int>(kMinRoutes) * 3 : 1);
}
