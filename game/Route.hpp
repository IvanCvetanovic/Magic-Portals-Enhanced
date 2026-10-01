#pragma once

// A WIN ROUTE: a level played from its start to its door by scheduled input alone.
//
// A route is a hand-authored file, tests/routes/<level>.json: the walks held over
// spans of the layer's ticks, the taps pressed at points of the level on stated
// ticks, and the tick by which the level must be WON - goals.completed, reached by
// play. Nothing else: no PutPlayer, no DEV flag, no portal placed by hand. The
// program's definition of done (docs/parity-backlog.md, section 6) asks for one per
// level, and step 113 of the remaster record built this to hold them.
//
// It is played through the DEV schedule main's --tap and --hold already use
// (MagicPortalsLayer::ScheduleDevTap / ScheduleDevHold), which step 100 made a real
// press: a scheduled tap is the tick's touch, read once by readInput and handed to
// the controls, a help block or the shot exactly as a press at that point would be.
// So test_mp_routes, which replays every route headless, and main's --route, which
// replays one in the real window, drive the layer by the same calls on the same
// ticks, and the one can be checked against the other by its log's "door reached
// on tick N".
//
// A TAP IS GIVEN AS A POINT OF THE LEVEL, not of the screen: the camera that turns
// one into the other is followed, eased and held, and a route written in screen
// pixels would move whenever the camera's numbers did. The driver turns it into a
// point of the view one tick ahead, after tick N-1, from the camera that tick left
// - which is the camera tick N's input is read against, because the layer moves
// the camera only after readInput (stepLevel). A point the view does not show on
// its tick is not tapped: a player can only touch what the screen shows, and the
// route fails there.
//
// The driver also checks each tick for what a route must not do: a shot tap that
// fires nothing or is refused, or lands anywhere but its point; a death; leaving
// the level. Each is a problem, and a route with a problem has failed.

#include <string>
#include <vector>

#include <entt/entt.hpp>
#include <glm/glm.hpp>

#include "core/EngineLayer.hpp"
#include "MagicPortalsLayer.hpp"
#include "sim/Portals.hpp"

namespace MagicPortals::Route {

// A walk held from tick `from` to `to` inclusive, as ScheduleDevHold takes it.
struct Hold {
    int from = 0;
    int to = 0;
    float direction = 0.0f; // -1 left, 1 right
};

// A touch down at a point of the level on `tick`, up on `releaseTick`.
struct Tap {
    int tick = 0;
    int releaseTick = 0;  // the tick after `tick` unless the file says later
    glm::dvec2 atPx{0.0}; // the level's pixels, +y down, as the .tscn places things
    // It fires the portal shot. False: a touch that must fire nothing - the one that
    // closes 1-02's and 1-03's tutorial as they load.
    bool shot = true;
};

struct Plan {
    std::string file;
    std::string level; // as chapters.json names it
    int winBy = 0;     // the layer's tick by which goals.completed must be set
    std::vector<Hold> holds;
    std::vector<Tap> taps; // by tick, each up before the next goes down
};

// Reads a route file. False with `error` saying what is wrong with it: a key the
// format does not have (other than a "_" comment), a tick below 1, two taps down at
// once, a walk that is neither left nor right.
bool Load(const std::string& path, Plan& out, std::string& error);

// How near its point a shot tap's shot must be aimed. The point goes to a fraction of
// the view here and back through the viewport's float pixels and the camera's float
// metres in the layer (ViewportToLevelPx), which costs thousandths of a pixel; a tap
// read a tick late is caught by the tick, not by this.
inline constexpr double kTapTolerancePx = 0.05;

// Feeds a plan into a layer and watches what it does. Begin once the layer has
// attached and loaded the level, then AfterTick after every one of the layer's
// ticks - which is where main's replay layer, pushed after the game's, runs.
class Driver {
public:
    explicit Driver(Plan plan);

    void Begin(MagicPortalsLayer& layer);
    void AfterTick(MagicPortalsLayer& layer);

    const Plan& Planned() const { return m_plan; }
    // The tick goals.completed was first seen set; 0 while it is not.
    int WonTick() const { return m_wonTick; }
    // Shots the route's taps fired, as the level counted them.
    int Shots() const { return m_shots; }
    // Entries the platform gate held on the way (Portals::State::transitsHeld), as
    // the last tick left the count: a route that wins with some held was re-timed by
    // the gate, one with none never met it.
    int Held() const { return m_held; }
    const std::vector<std::string>& Problems() const { return m_problems; }
    // Won, failed, or past win_by: nothing more to drive.
    bool Finished() const { return m_wonTick > 0 || !m_problems.empty() || m_ticks >= m_plan.winBy; }

private:
    struct Record {
        bool level = false;
        int shotsFired = 0;
        Portals::Tap lastTap;
    };

    Record record(const MagicPortalsLayer& layer) const;
    void schedule(MagicPortalsLayer& layer, int tick);
    void check(const MagicPortalsLayer& layer, const Tap& tap);
    void problem(const std::string& what);

    Plan m_plan;
    int m_ticks = 0;
    int m_deaths = 0;
    int m_wonTick = 0;
    int m_shots = 0;
    int m_held = 0;
    Record m_before; // the level's shot record as the last tick left it
    std::vector<std::string> m_problems;
};

// One line of what a route is doing, on stdout: the player, the camera, the shots
// and portals, every other traveller, the buttons, the switched doors and lifts,
// the fire (each fireball, where one went off this tick, what burns, the bombs), a
// death, and 1-32's beholder. test_mp_routes --trace prints it, and so does main's
// --route-trace, so a route can be written from the window when the suite cannot
// run (planning step 114).
void Trace(const MagicPortalsLayer& layer, entt::registry& registry, const char* why);

// DEV ONLY: --route. The driver as a layer, pushed after the game's so each of its
// ticks follows the game's, saying in the log and on exit how the route went.
// With `traceEvery` above 0 (--route-trace) it also prints Trace every that many
// ticks, on every tap's tick and on the tick it ends.
class ReplayLayer final : public Supersonic::EngineLayer {
public:
    ReplayLayer(MagicPortalsLayer& game, Plan plan, int traceEvery = 0);

    const char* Name() const override { return "Magic Portals route replay (DEV)"; }

    void OnAttach(entt::registry& registry) override;
    void OnFixedUpdate(entt::registry& registry, float fixedDelta) override;
    void OnDetach(entt::registry& registry) override;

private:
    MagicPortalsLayer& m_game;
    Driver m_driver;
    int m_traceEvery = 0;
    bool m_traced = false; // the ending's line, once
    bool m_said = false;
};

} // namespace MagicPortals::Route
