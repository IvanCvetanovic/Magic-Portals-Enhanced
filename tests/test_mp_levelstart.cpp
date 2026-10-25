// How a level is started on a phone (planning step 205, PORT-7): sim/LevelStart.hpp, the pure state machine behind the cover and the contact rule.
//
// WHAT IS PINNED, over scripts of the machine's own five inputs and nothing else (no engine, no layer, no window, no file):
//   - THE ORDER. A start asked for does not load in its own tick, nor in any tick of the frame that asked (the engine runs up to 5 fixed ticks between two draws,
//     so a "later tick" rule can load in the very frame that asked and never show the cover); the draw side is told to draw a COVER; the first tick AFTER a cover
//     was issued loads, once; the new level's first frame is then issued; the tick after that is the SETTLING tick; the one after it is the player's.
//   - THE CONTACT RULE, keyed on identity: a contact down when the start is asked for, pressed or found down at any time from then until the end of the settling
//     tick is ignored, stays ignored until it lifts, and its release is swallowed; a press the machine sees once it is Idle is a new contact and passes; a finger that
//     lifted unseen and pressed again at Idle passes; two fingers are two identities; reading a contact twice in one tick answers the same twice (a release swallowed
//     once is not delivered by the second reading).
//   - A level the port refuses ends the start (the cover stops); a second ask during a start is refused and changes nothing; the events come out in the order of
//     the order above, with the draw counts that show "cover drawn before load started"; the machine keeps no more than kMostEvents events.
//   - With the machine Idle and nothing ignored every contact passes untouched: a run that never asks for a start sees no difference at all.
// Each scenario is a SCRIPT of tokens; the trace of answers is compared with the one written beside it:
//     begin B/b (asked / refused)   tick t (nothing) or L (load now)   draw C (cover) or N (the frame)   ok o / refuse r (the load returned true / false)
//     <id>+ press  <id>= held  <id>- release  <id>+- press and release in one tick  <id>. nothing there   each answers P (passes) or I (ignored), '.' for nothing
// Each pin says in "mutation:" the change of sim/LevelStart.cpp that turns it red; the Step records that they were run (M1 ... M24).
//
// NOT TESTED, and why: the layer's glue (it holds the machine and calls it at the points above: game/MagicPortalsLayer.cpp, the scripted runs of the Step prove it on
// the real exe with --tap from a menu, under --profile mobile --fixed-step) and the cover's picture (a plain black overlay quad).

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "sim/LevelStart.hpp"

#include <string>
#include <vector>

using namespace MagicPortals;
using LevelStart::Machine;

namespace {

// Runs a script and returns the trace of answers, tokens joined by single spaces.
std::string Run(Machine& machine, const std::string& script) {
    std::string trace;
    const auto add = [&trace](const std::string& token) {
        if (!trace.empty()) trace += ' ';
        trace += token;
    };
    std::size_t at = 0;
    while (at < script.size()) {
        while (at < script.size() && script[at] == ' ') ++at;
        if (at >= script.size()) break;
        const std::size_t end = script.find(' ', at);
        const std::string token = script.substr(at, end == std::string::npos ? std::string::npos : end - at);
        at = end == std::string::npos ? script.size() : end;
        if (token == "begin") {
            LevelStart::Request request;
            request.kind = 1;
            request.index = 17;
            add(machine.Begin(request) ? "B" : "b");
        } else if (token == "tick") {
            add(machine.Tick().load ? "L" : "t");
        } else if (token == "draw") {
            add(machine.Draw() == LevelStart::Frame::Cover ? "C" : "N");
        } else if (token == "ok") {
            machine.Loaded(true);
            add("o");
        } else if (token == "refuse") {
            machine.Loaded(false);
            add("r");
        } else {
            // A contact: its id (digits), then the marks.
            std::size_t digits = 0;
            while (digits < token.size() && token[digits] >= '0' && token[digits] <= '9') ++digits;
            CHECK_MSG(digits > 0 && digits < token.size(), "a contact token: " + token);
            LevelStart::Contact contact;
            contact.id = std::stoi(token.substr(0, digits));
            const std::string marks = token.substr(digits);
            if (marks == "+") {
                contact.pressed = contact.held = true;
            } else if (marks == "=") {
                contact.held = true;
            } else if (marks == "-") {
                contact.released = true;
            } else if (marks == "+-") {
                contact.pressed = contact.released = true;
            } else if (marks == ".") {
                // nothing there
            } else {
                CHECK_MSG(false, "unknown marks in " + token);
            }
            const LevelStart::Verdict verdict = machine.Filter(contact);
            add(marks == "." ? "." : (verdict == LevelStart::Verdict::Pass ? "P" : "I"));
        }
    }
    return trace;
}

struct Scenario {
    const char* name;
    const char* script;
    const char* expected;
    LevelStart::Phase finalPhase;
};

using LevelStart::Phase;

// mutation: the ones named in each row's comment (see the header).
const Scenario kScenarios[] = {
    // An idle machine touches nothing: every contact passes, every frame is the normal one. M17 (ignore in Idle too).
    {"idle passes everything", "tick 0+ tick 0= tick 0- tick draw draw", "t P t P t P t N N", Phase::Idle},
    // The ticks of the frame that asked never load. M1 (load in CoverWanted: the tick-count rule), M2 (Draw does not issue the cover: never loads).
    {"ticks of the asking frame never load", "begin tick tick tick tick tick", "B t t t t t", Phase::CoverWanted},
    {"even five ticks then a draw then a tick: the cover came first", "begin tick tick tick tick tick draw tick", "B t t t t t C L", Phase::Loading},
    {"the cover first, then the load", "begin draw tick", "B C L", Phase::Loading},
    // A frame with no tick draws the cover again; the load waits for the tick. M16 (the second cover frame is the normal one).
    {"covers repeat until a tick", "begin draw draw draw tick", "B C C C L", Phase::Loading},
    // After the load: the same frame's remaining ticks do nothing; the first frame; the settling tick; Idle. M6, M7.
    {"load, first frame, settling tick, idle", "begin draw tick ok tick draw tick tick", "B C L o t N t t", Phase::Idle},
    {"after the settling tick the machine is settled but not before", "begin draw tick ok draw tick", "B C L o N t", Phase::Settling},
    // A press made while the cover is up is ignored, so is its hold, and its release is swallowed; a later press is the player's. M4, M5, M18.
    {"a press under the cover is ignored to its release", "begin tick 0+ draw tick 0= ok draw tick 0- tick tick 0+", "B t I C L I o N t I t t P", Phase::Idle},
    // A finger down when the start was asked for stays ignored through the load, and its release is swallowed. M3, M10.
    {"a finger held through the load: ignored hold, swallowed release", "0+ tick 0= begin draw tick ok draw tick tick 0= tick 0- tick 0+",
     "P t P B C L o N t t I t I t P", Phase::Idle},
    // What the system queued during the stall is read in the settling tick: a tap in it is ignored, one the tick after is the player's. M6, M7.
    {"the press queued during the stall is read on the settling tick and ignored", "begin draw tick ok draw tick 1+- tick 1+", "B C L o N t I t P", Phase::Idle},
    {"a held press queued during the stall is ignored until it lifts", "begin draw tick ok draw tick 1+ tick 1= tick 1- tick 1+", "B C L o N t I t I t I t P", Phase::Idle},
    // A level the port refuses ends the start and the cover with it. M11.
    {"a refused level ends the start", "begin draw tick refuse draw tick 0+", "B C L r N t P", Phase::Idle},
    // A second ask while one is in progress is refused, at every phase but Idle. M12.
    {"a second ask is refused while a start is in progress", "begin begin draw begin tick begin", "B b C b L b", Phase::Loading},
    {"an ask in the settling phase is refused, one after it is not", "begin draw tick ok draw begin tick tick begin", "B C L o N b t t B", Phase::CoverWanted},
    {"a start can follow a finished start", "begin draw tick ok draw tick tick begin draw tick", "B C L o N t t B C L", Phase::Loading},
    // An ignored id that lifted unseen and presses again once Idle is a new contact. M8, M19.
    {"a lifted-unseen id that presses again at idle passes", "begin tick 5+ draw tick ok draw tick tick 5. tick 5+", "B t I C L o N t t . t P", Phase::Idle},
    {"a press edge on a still-marked id is a new contact at idle", "begin tick 5+ draw tick ok draw tick tick 5+ tick 5=", "B t I C L o N t t P t P", Phase::Idle},
    // Two fingers are two identities. M10.
    {"two fingers: one ignored, one the player's", "0+ tick begin draw tick ok draw tick tick 0= 1+ tick 0- 1=", "P t B C L o N t t I P t I P", Phase::Idle},
    // A contact found down, without a press edge, in flight was pressed before. M13.
    {"a contact found down in flight is ignored until it lifts", "begin tick 7= draw tick ok draw tick 7= tick tick 7= 7- tick 7+", "B t I C L o N t I t t I I t P", Phase::Idle},
    {"a release with no press seen in flight is swallowed and forgotten", "begin tick 3- draw tick ok draw tick tick 3+", "B t I C L o N t t P", Phase::Idle},
    // Reading a contact twice in one tick answers the same: a release swallowed once is not delivered by the second reading. M9.
    {"a swallowed release is not delivered by a second reading in the tick", "0+ tick begin draw tick ok draw tick tick tick 0- 0-", "P t B C L o N t t t I I", Phase::Idle},
    {"a press read twice in one tick under the cover is ignored twice", "begin tick 0+ 0+ draw", "B t I I C", Phase::CoverIssued},
    // A press edge in the same tick as Begin, read after it, is ignored. M4.
    {"a press on the tick of the ask is ignored", "begin 0+ draw tick", "B I C L", Phase::Loading},
    // Loaded is meaningless outside a load. Nothing changes.
    {"Loaded outside a load changes nothing", "ok draw 0+", "o N P", Phase::Idle},
    {"Loaded true twice leaves the first frame to the draw side", "begin draw tick ok ok draw tick", "B C L o o N t", Phase::Settling},
    // A contact with nothing there is not a contact. M19.
    {"nothing there is nothing", "begin tick 4. draw tick 4.", "B t . C L .", Phase::Loading},
};

void EveryScenarioRunsAsWritten() {
    for (const Scenario& scenario : kScenarios) {
        Machine machine;
        const std::string trace = Run(machine, scenario.script);
        CHECK_MSG(trace == scenario.expected, std::string(scenario.name) + ": '" + scenario.script + "' answered '" + trace + "', expected '" + scenario.expected + "'");
        CHECK_MSG(machine.phase() == scenario.finalPhase, std::string(scenario.name) + ": ends " + LevelStart::Name(machine.phase()) + ", expected " + LevelStart::Name(scenario.finalPhase));
    }
}

void TheOrderIsCoverDrawnThenLoadThenFirstFrameThenSettled() {
    // mutation: M1, M2, M14 (the counts), M15 (no LoadStarted event), M6.
    Machine machine;
    Run(machine, "begin tick tick draw tick ok tick draw tick tick");
    const std::vector<LevelStart::Event> events = machine.TakeEvents();
    std::vector<LevelStart::EventKind> kinds;
    for (const LevelStart::Event& event : events) kinds.push_back(event.kind);
    const std::vector<LevelStart::EventKind> expected = {LevelStart::EventKind::Begun, LevelStart::EventKind::CoverIssued, LevelStart::EventKind::LoadStarted,
                                                         LevelStart::EventKind::Loaded, LevelStart::EventKind::FirstFrameIssued, LevelStart::EventKind::Settled};
    CHECK_MSG(kinds == expected, "the events come out in the order of the rule: " + std::to_string(kinds.size()) + " of them");
    if (kinds != expected || events.size() != 6) return;
    const LevelStart::Event& begun = events[0];
    const LevelStart::Event& cover = events[1];
    const LevelStart::Event& load = events[2];
    CHECK_MSG(cover.draw > begun.draw, "the cover was issued by a draw that came after the ask");
    CHECK_MSG(load.draw >= cover.draw && load.tick > begun.tick + 2, "and the load began after that draw, on a tick after the ones of the asking frame: draw " + std::to_string(load.draw) + ", tick " + std::to_string(load.tick));
    CHECK_MSG(begun.draw == 0 && cover.draw == 1 && load.draw == 1, "the draw counts a log would show: ask at draw 0, cover at 1, load after 1");
    CHECK_MSG(events[4].draw == 2, "the level's first frame is the next draw (2)");
    CHECK_EQ(machine.draws(), 2);
    CHECK_EQ(machine.ticks(), 6);
    CHECK_EQ(machine.coverFrames(), 1);
    CHECK_MSG(machine.TakeEvents().empty(), "taking the events empties the list");
}

void TheCoverIsDrawnOnEveryFrameUntilTheTickThatLoads() {
    // mutation: M16 (a second cover frame comes out as the normal frame), M14 (cover frames not counted).
    Machine machine;
    Run(machine, "begin draw draw draw");
    CHECK_EQ(machine.coverFrames(), 3);
    CHECK_MSG(machine.Covering(), "covering until the load returns");
    Run(machine, "tick");
    CHECK_MSG(machine.Covering() && machine.phase() == Phase::Loading, "the tick that loads is still the cover's");
    Run(machine, "ok");
    CHECK_MSG(!machine.Covering() && machine.phase() == Phase::AwaitFirstFrame, "and once the load returned the level is drawn");
}

void TheRequestComesBackWithTheLoad() {
    // mutation: M20 (the load carries an empty request).
    Machine machine;
    LevelStart::Request request;
    request.kind = 4;
    request.index = 99;
    request.extra = 3;
    CHECK(machine.Begin(request));
    CHECK_MSG(!machine.Tick().load, "not in the asking frame");
    machine.Draw();
    const LevelStart::Action action = machine.Tick();
    CHECK_MSG(action.load && action.request.kind == 4 && action.request.index == 99 && action.request.extra == 3, "the load is handed the request that was asked for");
    CHECK_MSG(!machine.Tick().load, "and says Load once");
}

void TheEventsAreCappedAndDescribed() {
    // mutation: M21 (no cap).
    Machine machine;
    LevelStart::Request request;
    request.kind = 1;
    request.index = 1;
    machine.Begin(request);
    for (int i = 0; i < 400; ++i) machine.Begin(request); // every one refused, every one an event
    const std::vector<LevelStart::Event> events = machine.TakeEvents();
    CHECK_MSG(events.size() == Machine::kMostEvents, "at most kMostEvents are kept: " + std::to_string(events.size()));
    CHECK_MSG(!events.empty() && events.back().kind == LevelStart::EventKind::BeginRefused, "the newest are the ones kept");
    // Describe: every event has a line that names the level start, and a contact's names the contact.
    Machine small;
    Run(small, "begin tick 6+ draw tick ok draw tick 6- tick tick");
    bool sawIgnored = false;
    bool sawLifted = false;
    for (const LevelStart::Event& event : small.TakeEvents()) {
        const std::string line = LevelStart::Describe(event);
        CHECK_MSG(line.rfind("level start: ", 0) == 0, "the line begins with the subject: " + line);
        if (event.kind == LevelStart::EventKind::ContactIgnored) {
            sawIgnored = true;
            CHECK_MSG(line.find("contact 6 ignored") != std::string::npos && line.find("pressed while the start was") != std::string::npos, line);
        }
        if (event.kind == LevelStart::EventKind::ContactLifted) {
            sawLifted = true;
            CHECK_MSG(line.find("contact 6 lifted") != std::string::npos && line.find("swallowed") != std::string::npos, line);
        }
    }
    CHECK_MSG(sawIgnored && sawLifted, "the ignored press and the swallowed release are both in the log");
    CHECK_MSG(std::string(LevelStart::Name(Phase::Idle)) == "idle" && std::string(LevelStart::Name(Phase::Settling)) == "settling", "the phases have names");
}

void TheIgnoredContactsAreVisible() {
    // mutation: M18 (a lifted contact is not forgotten), M3 (a contact down at the ask is not marked).
    Machine machine;
    Run(machine, "0+ 3+ tick begin");
    CHECK_MSG(machine.IgnoredContacts() == (std::vector<int>{0, 3}), "both fingers that were down are ignored the moment the start is asked for");
    Run(machine, "draw tick ok draw tick tick");
    CHECK_MSG(machine.Idle() && machine.IgnoredContacts() == (std::vector<int>{0, 3}), "and still, once Idle: identity and not a window");
    Run(machine, "tick 0- tick 3.");
    CHECK_MSG(machine.IgnoredContacts().empty(), "until each lifts: the release swallowed, the absent one forgotten");
    Run(machine, "tick 0+");
    CHECK_MSG(machine.IgnoredContacts().empty(), "and then a press passes");
}

void OnlyTheNoContactIdIsNoContact() {
    // mutation: M22 (every id below 0 is "no contact": the layer's own pointer has the id -2 and would never be ignored).
    Machine machine;
    Run(machine, "begin tick");
    LevelStart::Contact none;
    none.pressed = none.held = true;
    CHECK_MSG(none.id == LevelStart::kNoContact, "a contact made with no id has the no-contact id");
    CHECK_MSG(machine.Filter(none) == LevelStart::Verdict::Pass, "which is never ignored");
    CHECK_MSG(machine.IgnoredContacts().empty(), "and marks nothing");
    // The layer's pointer is -2 and a scheduled tap is far above: both are contacts like any other.
    LevelStart::Contact pointer;
    pointer.id = -2;
    pointer.pressed = pointer.held = true;
    CHECK_MSG(machine.Filter(pointer) == LevelStart::Verdict::Ignore, "a press of the pointer (id -2) under the cover is ignored");
    LevelStart::Contact tap;
    tap.id = (1 << 24) + 3;
    tap.pressed = tap.held = true;
    CHECK_MSG(machine.Filter(tap) == LevelStart::Verdict::Ignore, "and so is a scheduled tap's");
    CHECK_MSG(machine.IgnoredContacts() == (std::vector<int>{-2, (1 << 24) + 3}), "both identities are held");
}

void runTests() {
    EveryScenarioRunsAsWritten();
    TheOrderIsCoverDrawnThenLoadThenFirstFrameThenSettled();
    TheCoverIsDrawnOnEveryFrameUntilTheTickThatLoads();
    TheRequestComesBackWithTheLoad();
    TheEventsAreCappedAndDescribed();
    TheIgnoredContactsAreVisible();
    OnlyTheNoContactIdIsNoContact();
}

} // namespace

MP_TEST_MAIN("test_mp_levelstart", 60)
