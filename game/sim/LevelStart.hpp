#pragma once

// HOW A LEVEL IS STARTED ON A PHONE (planning step 205, PORT-7; the platform research's PL-04).
//
// THE PROBLEM. A press on a level tile used to run `loadLevel` inside the input tick that read the press: the level's data, its music track's decode, its
// textures, its lightmap bakes, all before the next picture. On the desktop that is 187 to 198 ms (step 190, step 196); on a weak phone it is a stall of an
// unknown length (the research's estimate is 1 to 3 s and is only an estimate) with the menu frozen on its last picture. Two things follow from it:
//   - the player sees a frozen menu and not a game that is starting, and
//   - a finger that lands during the stall waits in the system's queue and is delivered to the NEW level as its first touch: a portal shot (a tap), a walk
//     (a finger held on an arrow) or a pause. Mashing Retry or Next after a win is the likeliest way to do it.
//
// THE RULE, as a state machine with no engine, no renderer and no clock, so that a table can drive it. A phone's run (the phone profile, nothing else) starts a
// level in four steps:
//   1. Begin: the press asks for a start. The tick that asked does NOT load. Every contact (finger) that is down at that moment is marked IGNORED.
//   2. Draw: the next frame the draw side makes is a COVER, a plain black frame (the original's LoadingScreen background is black), and nothing else. The draw
//      side tells the machine with Draw(), which answers what the frame is. A frame is the unit, not a tick: the engine runs up to 5 fixed ticks between two
//      draws (SupersonicApp.cpp, kMaxPhysicsStepsPerFrame), so a rule of "load on a later tick" can load in the very frame that asked and never show the cover.
//   3. Load: the first Tick() AFTER a cover was issued says Load. Since OnUpdate is the last layer call of a frame and the engine draws after it, a tick that
//      comes after a cover was issued is in a later frame than the one that issued it: the cover has been drawn. The layer runs the load in that tick (the same
//      call the old path made, so the level it starts is the level the desktop starts) and reports Loaded.
//   4. Open: the new level's first frame is issued (Draw() again), and the NEXT tick, whose input was read from the system's queue AFTER that frame, is the SETTLING
//      tick: a press that first shows up in it is a press that was made during the stall. After it the machine is Idle and a press is the player's.
//
// THE CONTACT RULE, keyed on a contact's IDENTITY and never on a window of ticks (a finger held through a 2 s load would otherwise arrive after a window as a
// release with no press, or as a latched walk or shot): from Begin until the end of the settling tick, any contact that is first seen, pressed, or found down is
// ignored, and it STAYS ignored until it lifts (released, or no longer down). What the layer is told for an ignored contact is "nothing there": no press, no hold, no
// release. Its release is swallowed (it must not fire a shot, close a popup or press a button) and the identity is forgotten when it lifts. A press that is seen
// once the machine is Idle is a new contact and passes, whether or not the same id was ignored before. With the machine Idle and no contact ignored, everything
// passes untouched: a run that never calls Begin sees no difference at all.
//
// WHAT IS NOT HERE. The level track's decode and the lightmap bake on a worker thread are NOT built: see docs/parity-backlog.md, PORT7-1.
//
// Free of the engine and of the renderer, like the rest of sim/.

#include <cstdint>
#include <limits>
#include <set>
#include <string>
#include <vector>

namespace MagicPortals::LevelStart {

// What a start asks for, opaque to the machine: the layer maps `kind` to the call the old path made (a tile, Retry, Next, the dashboard's Start, the in-level
// restart control) and `index` and `extra` to its arguments.
struct Request {
    int kind = 0;
    int index = -1;
    int extra = -1;
};

enum class Phase {
    Idle,            // no start in progress
    CoverWanted,     // asked; the cover has not been issued: ticks do nothing
    CoverIssued,     // a cover was issued to the draw: the next tick loads
    Loading,         // inside the tick that loads
    AwaitFirstFrame, // loaded; the new level's first frame is not issued yet
    Settling         // that frame is issued; the next tick is the settling tick, the one after it is the first of the player's
};

// What the draw side makes of a frame.
enum class Frame {
    Normal, // the frame the layer would have drawn
    Cover   // the plain black frame
};

// What a tick must do.
struct Action {
    bool load = false; // run the request now
    Request request;
};

// The identity of "no contact". Not -1 or any small negative number: the layer's own pointer is a contact with a negative id (MagicPortalsLayer::kPointerFinger).
inline constexpr int kNoContact = std::numeric_limits<int>::min();

// One contact as the layer reads it this tick. `id` is the contact's identity (the pointer, an engine contact, a scheduled tap).
struct Contact {
    int id = kNoContact;
    bool pressed = false;  // its press edge is this tick's
    bool held = false;     // it is down
    bool released = false; // its release edge is this tick's
};

enum class Verdict { Pass, Ignore };

// What happened, for the run's log and for the table that pins the order. `tick` and `draw` are the machine's own counts of Tick() and Draw() calls.
enum class EventKind {
    Begun,            // a start was asked for
    BeginRefused,     // a start was asked for while one was in progress
    CoverIssued,      // the first cover frame
    LoadStarted,      // the tick that loads
    Loaded,           // the load returned
    LoadRefused,      // the load returned false (the level was refused): the start is over
    FirstFrameIssued, // the new level's first frame
    Settled,          // the settling tick is over: the player's presses count again
    ContactIgnored,   // a contact was ignored (the `contact` field); `detail` says why
    ContactLifted     // an ignored contact lifted (its release was swallowed)
};

struct Event {
    EventKind kind = EventKind::Begun;
    std::int64_t tick = 0;
    std::int64_t draw = 0;
    int contact = -1;
    std::string detail;
};

class Machine {
public:
    // Asks for a start. False (and BeginRefused) when one is already in progress. Marks every contact that was passed and is still down as ignored.
    bool Begin(const Request& request);

    // THE DRAW SIDE, once per frame, at the point the layer chooses what to emit. Counts the frame. Answers Cover while a start is waiting for its cover and
    // until the tick that loads; the first answer moves CoverWanted to CoverIssued. Answers Normal otherwise, and moves AwaitFirstFrame to Settling (the new
    // level's first frame is issued by that very answer).
    Frame Draw();

    // THE TICK SIDE, first thing in every tick of the layer. Counts the tick. Says Load once, on the first tick after a cover was issued. Moves Settling to Idle on
    // its second call (the first is the settling tick itself).
    Action Tick();

    // The layer ran the load: `ok` false is a level the port refused (the HUD says why, as on the old path) and ends the start; true waits for the first frame.
    void Loaded(bool ok);

    // Every contact the layer reads, every tick, before the layer uses it. Idempotent within a tick (a second call for the same id answers what the first did, so
    // a release swallowed once is not delivered by a second reading).
    Verdict Filter(const Contact& contact);

    Phase phase() const { return m_phase; }
    // The ticks do nothing but ask the machine: the start has been asked for and the level is not loaded yet.
    bool Covering() const { return m_phase == Phase::CoverWanted || m_phase == Phase::CoverIssued || m_phase == Phase::Loading; }
    bool Idle() const { return m_phase == Phase::Idle; }
    const Request& request() const { return m_request; }
    std::int64_t ticks() const { return m_ticks; }
    std::int64_t draws() const { return m_draws; }
    std::int64_t coverFrames() const { return m_coverFrames; }
    // The contacts now ignored (their identities), for the tests and the log.
    std::vector<int> IgnoredContacts() const { return std::vector<int>(m_ignored.begin(), m_ignored.end()); }

    // The events since the last call, oldest first; the machine keeps at most kMostEvents (the oldest are dropped, so a run that never drains cannot grow).
    std::vector<Event> TakeEvents();
    static constexpr std::size_t kMostEvents = 256;

private:
    void Note(EventKind kind, int contact = -1, std::string detail = std::string());

    Phase m_phase = Phase::Idle;
    Request m_request;
    std::int64_t m_ticks = 0;
    std::int64_t m_draws = 0;
    std::int64_t m_coverFrames = 0;
    bool m_settlingTickOpen = false; // the settling tick has begun (the first Tick() of Settling)
    std::set<int> m_ignored;         // contacts ignored until they lift
    std::set<int> m_down;            // contacts passed while Idle that are still down: Begin marks them
    std::vector<std::pair<int, Verdict>> m_cache; // this tick's verdicts, by id
    std::int64_t m_cacheTick = -1;
    std::vector<Event> m_events;
};

// One line for the log of an event: "level start: contact 3 ignored on tick 41 (pressed while the cover was up)".
std::string Describe(const Event& event);

// The phase's name, for the log and the tests.
const char* Name(Phase phase);

} // namespace MagicPortals::LevelStart
