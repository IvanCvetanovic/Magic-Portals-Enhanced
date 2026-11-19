// What the original does ONCE A FRAME, when a frame of the original is more than one tick of the port: the pace knob
// (clock.json original_frame_hz) as sim/ScriptClock's Rules holds it, the frame phase a tick's Step carries, and the
// helpers a once-a-frame quantity is written with (FrameTravel, FrameRoot, FrameDelayTicks). What is pinned:
//
//   the identity  every helper returns its own input at one tick a frame (and at a count below one, which is read as one): an
//                 early return, so the pace the port has always had reads bit for bit what it read - FrameTravel(0.3, 1) is
//                 0.3, where 1 - (1 - 0.3) / 1 would be 0.30000000000000004.
//   the travel    FrameTravel(s, n) = n s / (1 + (n - 1) s): the per-tick share whose summed glide after a release equals the
//                 original's per-frame glide. 0.43 at two ticks a frame is 0.601399, the glide after a release at 2.4 units a
//                 tick 3.621 against the original's 3.734 per 4.95 units a frame, the 3 % being the walk's 144 against
//                 148.5 units a second; the ends 0 and 1 stay put.
//   the root      FrameRoot(s, n) = s^(1/n): n ticks of it make s again (0.43 -> 0.655744; 0.9 -> 0.948683).
//   the delay     FrameDelayTicks(frames, step) = lround(frames x frameTicks), half away from zero (2.25 frames at two ticks
//                 is 5, 2.5 is 5; Python's round would say 4 for the first), and nothing for a count not above zero.
//   the pace      Rules::SetOriginalFrameHz sets the hertz and the ticks a frame TOGETHER, and only for a whole number of
//                 hertz from 1 to matchedHz that divides it: every hertz of 1..60 is tried at 60, and a refusal changes
//                 nothing.
//   the phase     TickStep(rules, tick, levelStep) gives framePhase = levelStep % frameTicks: 0,1,0,1 at 30 Hz, 0,1,2 at 20,
//                 always 0 at 60; a Step built by hand is one tick a frame, phase 0.
//   the scratch   tests/DataDirs.hpp: a copy of the port's data with one clock key set (the pace a case that starts the layer
//                 at 30 asks for), the key rewritten or added by its own pure function, and the copy gone when its owner is.
//
// Pure arithmetic and a scratch copy of the port's own committed data: no level, no window, so it never skips.

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "DataDirs.hpp"

#include "sim/ScriptClock.hpp"

#include <cmath>
#include <cstddef>
#include <filesystem>
#include <limits>
#include <string>
#include <system_error>

using namespace MagicPortals;

namespace {

constexpr float kTick = 1.0f / 60.0f;

// Doubles, which the harness's CHECK_NEAR (floats) would narrow, within an absolute `eps`.
#define CHECK_WITHIN(a, b, eps)                                                                                            \
    do {                                                                                                                   \
        const double withinA = (a);                                                                                        \
        const double withinB = (b);                                                                                        \
        CHECK_MSG(std::fabs(withinA - withinB) <= (eps),                                                                   \
                  std::string(#a) + " is " + std::to_string(withinA) + ", expected " + std::to_string(withinB));           \
    } while (false)

// The Rules of a clock.json of the shape the shipped one has, at `hz` an original frame, as a pure suite builds one in code.
ScriptClock::Rules RulesAt(int hz) {
    ScriptClock::Rules rules;
    CHECK_MSG(rules.SetOriginalFrameHz(hz), std::to_string(hz) + " Hz is a pace the 60 Hz rules take");
    return rules;
}

void EveryHelperIsTheIdentityAtOneTickAFrame() {
    // A count of one, and a count below one (read as one): the input itself, bit for bit - 0.3 is the share a form that
    // computed 1 - (1 - s) / n would not give back.
    for (const double share : {0.0, 0.1, 0.3, 0.43, 0.5, 0.86, 0.9, 1.0}) {
        for (const int ticks : {1, 0, -3}) {
            CHECK_MSG(ScriptClock::FrameTravel(share, ticks) == share,
                      "FrameTravel(" + std::to_string(share) + ", " + std::to_string(ticks) + ") is its input");
            CHECK_MSG(ScriptClock::FrameRoot(share, ticks) == share,
                      "FrameRoot(" + std::to_string(share) + ", " + std::to_string(ticks) + ") is its input");
        }
    }
    CHECK(ScriptClock::FrameTravel(0.3, 1) == 0.3);
    // The delay at one tick a frame is the whole frames themselves.
    const ScriptClock::Step one(kTick);
    CHECK_EQ(ScriptClock::FrameDelayTicks(2.0, one), 2);
    CHECK_EQ(ScriptClock::FrameDelayTicks(0.0, one), 0);
}

void FrameTravelKeepsTheDistanceTheOriginalGlides() {
    // The worked number of the plan: 0.43 over two ticks a frame is 0.86 / 1.43.
    CHECK_WITHIN(ScriptClock::FrameTravel(0.43, 2), 0.601399, 1e-6);
    CHECK_WITHIN(ScriptClock::FrameTravel(0.43, 2), 2.0 * 0.43 / (1.0 + 0.43), 1e-15);
    // The defining equation, over a grid: the per-tick share r with r / (1 - r) = n s / (1 - s).
    for (const double share : {0.1, 0.3, 0.43, 0.5, 0.86}) {
        for (const int ticks : {2, 3, 4, 5}) {
            const double r = ScriptClock::FrameTravel(share, ticks);
            const double want = static_cast<double>(ticks) * share / (1.0 - share);
            CHECK_MSG(std::fabs(r / (1.0 - r) - want) <= 1e-9 * want,
                      "FrameTravel(" + std::to_string(share) + ", " + std::to_string(ticks) + ") is " + std::to_string(r));
        }
    }
    // The ends stay put at every count: nothing braked, everything braked.
    for (const int ticks : {2, 3, 5, 30}) {
        CHECK_MSG(ScriptClock::FrameTravel(1.0, ticks) == 1.0, "a share of 1 is 1 at " + std::to_string(ticks) + " ticks");
        CHECK_MSG(ScriptClock::FrameTravel(0.0, ticks) == 0.0, "a share of 0 is 0 at " + std::to_string(ticks) + " ticks");
    }
    // More ticks a frame, more of the velocity kept each tick.
    CHECK(ScriptClock::FrameTravel(0.43, 3) > ScriptClock::FrameTravel(0.43, 2));
    CHECK(ScriptClock::FrameTravel(0.43, 2) > 0.43);

    // THE TAILS. A key released at 2.4 units a tick (the walk's 144 units a second) glides r / (1 - r) of it; the original
    // releases at 4.95 units a frame (148.5 units a second at 30 frames) and glides s / (1 - s) of that.
    const double r = ScriptClock::FrameTravel(0.43, 2);
    const double portTail = 2.4 * r / (1.0 - r);
    const double originalTail = 4.95 * 0.43 / (1.0 - 0.43);
    CHECK_WITHIN(portTail, 3.621, 1e-3);
    CHECK_WITHIN(originalTail, 3.734, 1e-3);
    // The 3 % between them is the speed of the walk and nothing in the rule: 144 against 148.5.
    CHECK_WITHIN(portTail / originalTail, 144.0 / 148.5, 1e-6);
}

void FrameRootComposesBackToTheShareAtEachFrameBoundary() {
    CHECK_WITHIN(ScriptClock::FrameRoot(0.43, 2), 0.655744, 1e-6);
    CHECK_WITHIN(ScriptClock::FrameRoot(0.43, 2) * ScriptClock::FrameRoot(0.43, 2), 0.43, 1e-12);
    CHECK_WITHIN(ScriptClock::FrameRoot(0.9, 2), 0.948683, 1e-6);
    for (const double share : {0.1, 0.43, 0.5, 0.9, 0.995}) {
        for (const int ticks : {2, 3, 5}) {
            const double root = ScriptClock::FrameRoot(share, ticks);
            CHECK_MSG(std::fabs(std::pow(root, static_cast<double>(ticks)) - share) <= 1e-12,
                      std::to_string(ticks) + " ticks of FrameRoot(" + std::to_string(share) + ") do not make the share");
        }
    }
    for (const int ticks : {2, 3}) {
        CHECK_MSG(ScriptClock::FrameRoot(1.0, ticks) == 1.0, "nothing braked is nothing braked per tick");
        CHECK_MSG(ScriptClock::FrameRoot(0.0, ticks) == 0.0, "everything braked is everything braked per tick");
    }
}

void FrameDelayTicksRoundsHalfAwayFromZeroAndAnythingNotAboveZeroIsNoDelay() {
    const ScriptClock::Step one = ScriptClock::TickStep(RulesAt(60), kTick);
    const ScriptClock::Step two = ScriptClock::TickStep(RulesAt(30), kTick);
    const ScriptClock::Step three = ScriptClock::TickStep(RulesAt(20), kTick);
    CHECK_EQ(one.frameTicks, 1);
    CHECK_EQ(two.frameTicks, 2);
    CHECK_EQ(three.frameTicks, 3);
    // The ground flag's decoded lag: two frames, which is four ticks at 30 Hz (a mechanical latency of two ticks is another thing).
    CHECK_EQ(ScriptClock::FrameDelayTicks(2.0, two), 4);
    // C++ lround, not Python's round: 4.5 ticks is 5 (Python's round makes it 4), and 5 ticks stay 5.
    CHECK_EQ(ScriptClock::FrameDelayTicks(2.25, two), 5);
    CHECK_EQ(ScriptClock::FrameDelayTicks(2.5, two), 5);
    CHECK_EQ(ScriptClock::FrameDelayTicks(1.25, two), 3);
    CHECK_EQ(ScriptClock::FrameDelayTicks(0.25, two), 1);
    CHECK_EQ(ScriptClock::FrameDelayTicks(0.2, two), 0);
    CHECK_EQ(ScriptClock::FrameDelayTicks(1.0, two), 2);
    CHECK_EQ(ScriptClock::FrameDelayTicks(8.0, two), 16);
    // At one tick a frame the frames are the ticks, rounded the same way.
    CHECK_EQ(ScriptClock::FrameDelayTicks(2.0, one), 2);
    CHECK_EQ(ScriptClock::FrameDelayTicks(2.25, one), 2);
    CHECK_EQ(ScriptClock::FrameDelayTicks(2.5, one), 3);
    CHECK_EQ(ScriptClock::FrameDelayTicks(0.5, one), 1);
    // Three ticks a frame.
    CHECK_EQ(ScriptClock::FrameDelayTicks(2.0, three), 6);
    CHECK_EQ(ScriptClock::FrameDelayTicks(2.25, three), 7);
    // No delay for what is not above zero: nothing, a negative count, and not a number.
    CHECK_EQ(ScriptClock::FrameDelayTicks(0.0, two), 0);
    CHECK_EQ(ScriptClock::FrameDelayTicks(-1.0, two), 0);
    CHECK_EQ(ScriptClock::FrameDelayTicks(std::numeric_limits<double>::quiet_NaN(), two), 0);
    // A Step whose count was poked below one is read as one tick a frame.
    ScriptClock::Step poked;
    poked.frameTicks = 0;
    CHECK_EQ(ScriptClock::FrameDelayTicks(2.0, poked), 2);
}

void ThePaceIsAHertzAndItsTicksSetTogetherAndOnlyToADivisor() {
    // The hertz the plan names, at the 60 the shipped clock matches.
    {
        ScriptClock::Rules rules;
        CHECK_EQ(rules.originalFrameHz, 60);
        CHECK_EQ(rules.frameTicks, 1);
        CHECK(rules.SetOriginalFrameHz(30));
        CHECK_EQ(rules.originalFrameHz, 30);
        CHECK_EQ(rules.frameTicks, 2);
        CHECK(rules.SetOriginalFrameHz(60));
        CHECK_EQ(rules.originalFrameHz, 60);
        CHECK_EQ(rules.frameTicks, 1);
        CHECK(rules.SetOriginalFrameHz(20));
        CHECK_EQ(rules.frameTicks, 3);
        CHECK(rules.SetOriginalFrameHz(1));
        CHECK_EQ(rules.frameTicks, 60);
        // A refusal changes nothing: the pair stays what it was (1 Hz, 60 ticks).
        for (const int refused : {45, 0, -30, 61, 120, 7}) {
            CHECK_MSG(!rules.SetOriginalFrameHz(refused), std::to_string(refused) + " Hz is refused at 60");
            CHECK_MSG(rules.originalFrameHz == 1 && rules.frameTicks == 60,
                      "and the refusal of " + std::to_string(refused) + " left the pair alone");
        }
    }
    // Every whole hertz of 1..60: taken exactly when it divides 60, and then hertz x ticks is 60.
    for (int hz = 1; hz <= 60; ++hz) {
        ScriptClock::Rules rules;
        const bool taken = rules.SetOriginalFrameHz(hz);
        CHECK_MSG(taken == (60 % hz == 0), std::to_string(hz) + " Hz is taken exactly when it divides 60");
        if (taken) {
            CHECK_MSG(rules.originalFrameHz == hz && rules.frameTicks * hz == 60, std::to_string(hz) + " Hz is whole ticks of the 60");
        } else {
            CHECK_MSG(rules.originalFrameHz == 60 && rules.frameTicks == 1, "a refused " + std::to_string(hz) + " Hz leaves the default");
        }
    }
    // The ticks follow the matched rate the Rules hold: at 30 Hz matched, 60 is above it and 15 is two ticks.
    ScriptClock::Rules matched30;
    matched30.matchedHz = 30;
    CHECK(!matched30.SetOriginalFrameHz(60));
    CHECK(matched30.SetOriginalFrameHz(15));
    CHECK_EQ(matched30.frameTicks, 2);
    CHECK(matched30.SetOriginalFrameHz(30));
    CHECK_EQ(matched30.frameTicks, 1);
}

void ATicksStepCarriesItsPhaseInTheFrame() {
    // Two ticks a frame: 0, 1, 0, 1 from the level's first step; three: 0, 1, 2; one: always 0.
    for (unsigned levelStep = 0; levelStep < 12; ++levelStep) {
        const ScriptClock::Step two = ScriptClock::TickStep(RulesAt(30), kTick, levelStep);
        CHECK_EQ(two.frameTicks, 2);
        CHECK_EQ(two.framePhase, static_cast<int>(levelStep % 2u));
        const ScriptClock::Step three = ScriptClock::TickStep(RulesAt(20), kTick, levelStep);
        CHECK_EQ(three.frameTicks, 3);
        CHECK_EQ(three.framePhase, static_cast<int>(levelStep % 3u));
        const ScriptClock::Step one = ScriptClock::TickStep(RulesAt(60), kTick, levelStep);
        CHECK_EQ(one.frameTicks, 1);
        CHECK_EQ(one.framePhase, 0);
    }
    // The count is an unsigned and the phase its remainder, whatever its size.
    CHECK_EQ(ScriptClock::TickStep(RulesAt(30), kTick, 4294967295u).framePhase, 1);
    CHECK_EQ(ScriptClock::TickStep(RulesAt(20), kTick, 4294967295u).framePhase, 0);
    // The default argument is the level's first step.
    CHECK_EQ(ScriptClock::TickStep(RulesAt(30), kTick).framePhase, 0);
    // What a Step built by hand means: one tick a frame, phase 0, whichever form built it.
    const ScriptClock::Step none;
    const ScriptClock::Step bare(kTick);
    const ScriptClock::Step lengths(0.016, kTick);
    const ScriptClock::Step four(0.016, kTick, 1.0 / 30.0, 60.0);
    const ScriptClock::Step six(0.016, kTick, 1.0 / 60.0, 30.0, 2, 1);
    CHECK(none.frameTicks == 1 && none.framePhase == 0);
    CHECK(bare.frameTicks == 1 && bare.framePhase == 0);
    CHECK(lengths.frameTicks == 1 && lengths.framePhase == 0);
    CHECK(four.frameTicks == 1 && four.framePhase == 0);
    CHECK(six.frameTicks == 2 && six.framePhase == 1);
    // The new fields change nothing of the old ones: a four-argument Step has the values it was given.
    CHECK(four.scriptS == 0.016 && four.physicsS == kTick && four.simS == 1.0 / 30.0 && four.fps == 60.0);
}

void TheScratchDataCarriesThePaceACaseAsksFor() {
    // The pure function first: the key's number rewritten, the key added when it is absent, nothing made of what is not an object.
    CHECK(DataDirs::WithOriginalFrameHz("{\"matched_hz\": 60, \"original_frame_hz\": 60, \"snap_ms\": 0.001}", 30) ==
          "{\"matched_hz\": 60, \"original_frame_hz\": 30, \"snap_ms\": 0.001}");
    CHECK(DataDirs::WithOriginalFrameHz("{\"original_frame_hz\":60}", 20) == "{\"original_frame_hz\":20}");
    CHECK(DataDirs::WithOriginalFrameHz("{\n  \"original_frame_hz\"  :  60\n}", 30) == "{\n  \"original_frame_hz\"  :  30\n}");
    CHECK(DataDirs::WithOriginalFrameHz("{\"matched_hz\": 60}", 30) == "{\n  \"original_frame_hz\": 30,\"matched_hz\": 60}");
    CHECK(DataDirs::WithOriginalFrameHz("not json", 30).empty());
    CHECK(DataDirs::WithOriginalFrameHz("{\"original_frame_hz\"}", 30).empty());

    namespace fs = std::filesystem;
    std::error_code ec;
    ScriptClock::Rules shipped;
    std::string error;
    CHECK_MSG(ScriptClock::LoadRules(DataDirs::PortDataDir() + "/clock.json", shipped, error), error);
    std::string gonePath;
    {
        DataDirs::ScratchData scratch = DataDirs::PortDataWithClock(30);
        CHECK_MSG(scratch.Ok(), "a scratch copy of the port's data at 30 Hz is made");
        gonePath = scratch.Path();
        CHECK_MSG(fs::is_directory(fs::path(gonePath), ec), "the copy is a directory");
        // The whole tree came, a folder of the data included, and clock.json is the shipped file with its one number changed.
        CHECK_MSG(fs::is_regular_file(fs::path(gonePath) / "clock.json", ec), "with its clock.json");
        // Counted, not compared file by file: the port's data folder has no player.json (that table lives in the repository's data/), so a
        // check on one name would hold whether or not the copy came.
        auto countFiles = [&ec](const fs::path& dir) {
            std::size_t n = 0;
            fs::recursive_directory_iterator last;
            for (fs::recursive_directory_iterator it(dir, ec); !ec && it != last; it.increment(ec)) {
                if (it->is_regular_file(ec)) ++n;
            }
            return n;
        };
        CHECK_MSG(countFiles(fs::path(gonePath)) == countFiles(fs::path(DataDirs::PortDataDir())), "and as many files as the source has, its folders' too");
        CHECK_MSG(countFiles(fs::path(gonePath)) > 1, "more than the clock file alone");
        CHECK_MSG(fs::is_regular_file(fs::path(gonePath) / "art.json", ec) && fs::is_regular_file(fs::path(gonePath) / "lang" / "en.json", ec),
                  "a table and a file of a folder are in it");
        ScriptClock::Rules read;
        CHECK_MSG(ScriptClock::LoadRules(scratch.Path() + "/clock.json", read, error), error);
        CHECK_EQ(read.originalFrameHz, 30);
        CHECK_EQ(read.frameTicks, 2);
        CHECK_EQ(read.matchedHz, shipped.matchedHz);
        CHECK(read.physicsStepS == shipped.physicsStepS);
        CHECK_EQ(read.frameMs, shipped.frameMs);
        // Two copies asked for at once are two directories.
        DataDirs::ScratchData second = DataDirs::PortDataWithClock(60);
        CHECK(second.Ok() && second.Path() != scratch.Path());
        ScriptClock::Rules readSecond;
        CHECK_MSG(ScriptClock::LoadRules(second.Path() + "/clock.json", readSecond, error), error);
        CHECK_EQ(readSecond.frameTicks, 1);
        // A pace the file refuses is copied faithfully and refused on its reading, naming the key.
        DataDirs::ScratchData refused = DataDirs::PortDataWithClock(45);
        CHECK(refused.Ok());
        ScriptClock::Rules readRefused;
        CHECK(!ScriptClock::LoadRules(refused.Path() + "/clock.json", readRefused, error));
        CHECK_MSG(error.find("original_frame_hz") != std::string::npos, error);
    }
    // And the copies are gone with their owners.
    CHECK_MSG(!fs::exists(fs::path(gonePath), ec), "the scratch copy is removed when its owner goes");
    // A source that is not a data directory makes no copy.
    CHECK(!DataDirs::PortDataWithClock(30, (fs::temp_directory_path(ec) / "supersonic-test-mp-frameclock-no-such-data").string()).Ok());
}

void runTests() {
    EveryHelperIsTheIdentityAtOneTickAFrame();
    FrameTravelKeepsTheDistanceTheOriginalGlides();
    FrameRootComposesBackToTheShareAtEachFrameBoundary();
    FrameDelayTicksRoundsHalfAwayFromZeroAndAnythingNotAboveZeroIsNoDelay();
    ThePaceIsAHertzAndItsTicksSetTogetherAndOnlyToADivisor();
    ATicksStepCarriesItsPhaseInTheFrame();
    TheScratchDataCarriesThePaceACaseAsksFor();
}

} // namespace

MP_TEST_MAIN("test_mp_frameclock", 60)
