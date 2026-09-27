// The port's camera as arithmetic: Camera::Clamp keeps a view inside a level,
// and Camera::Follow starts at camera_start, holds, then eases to the player.
//
// Pure: no level and no data. portals.json's camera numbers are _guess, so they
// are not pinned; this suite runs with numbers of its own and checks the shape of
// what they do. The view is level0's shape, 455 x 256 in a level 768 x 256.
//
// And the shake the original gives that camera (sim/Earthquake): earthquake.json
// is the port's own and decoded, so its numbers ARE pinned, and the curve is
// checked against the decode's own table (D11 section 1c, D11_check C11) and the
// sequences the original's footage shows.

#include "TestHarness.hpp"

#include "sim/Camera.hpp"
#include "sim/Earthquake.hpp"

#include <cmath>
#include <iterator>
#include <string>
#include <vector>

using namespace MagicPortals;

namespace {

constexpr double kDt = 1.0 / 60.0;
const glm::dvec2 kView(455.0, 256.0);
const glm::dvec2 kBounds(768.0, 256.0);

bool Near(const glm::dvec2& a, const glm::dvec2& b, double eps) {
    return std::fabs(a.x - b.x) <= eps && std::fabs(a.y - b.y) <= eps;
}

std::string Px(const glm::dvec2& p) {
    return "(" + std::to_string(p.x) + ", " + std::to_string(p.y) + ")";
}

void TheViewStaysInsideTheLevel() {
    CHECK(Camera::Clamp({0.0, 0.0}, kView, kBounds) == glm::dvec2(227.5, 128.0));
    CHECK(Camera::Clamp({768.0, 256.0}, kView, kBounds) == glm::dvec2(540.5, 128.0));
    CHECK(Camera::Clamp({400.0, 50.0}, kView, kBounds) == glm::dvec2(400.0, 128.0));
    // A view wider than the level, as a wide window gives one, sits at its centre.
    CHECK(Camera::Clamp({0.0, 0.0}, {1000.0, 256.0}, kBounds) == glm::dvec2(384.0, 128.0));
    // And a taller level lets it travel up and down as well.
    CHECK(Camera::Clamp({100.0, 400.0}, kView, {512.0, 512.0}) == glm::dvec2(227.5, 384.0));
}

void ItHoldsThenEasesToThePlayer() {
    const Camera::Rules rules{0.12, 0.5};
    Camera::Follow follow;
    // level0: camera_start (651, 0) is held inside the level, at its right end.
    follow.Start(rules, {651.0, 0.0}, kView, kBounds);
    const glm::dvec2 start = follow.centrePx;
    CHECK(start == glm::dvec2(540.5, 128.0));

    const glm::dvec2 player(36.0, 86.0); // level0's spawn
    int tick = 0;
    for (; tick < 27; ++tick) follow.Tick(rules, player, kView, kBounds, kDt); // 0.45 s
    CHECK_MSG(follow.centrePx == start, "held for hold_time_s, now at " + Px(follow.centrePx));
    for (; tick < 36; ++tick) follow.Tick(rules, player, kView, kBounds, kDt); // 0.6 s
    CHECK_MSG(follow.centrePx.x < start.x, "moving by 0.6 s, at " + Px(follow.centrePx));

    // Each tick closes dt / follow_lag_s of the gap to where the view may go.
    const glm::dvec2 target = Camera::Clamp(player, kView, kBounds);
    const double before = follow.centrePx.x - target.x;
    follow.Tick(rules, player, kView, kBounds, kDt);
    const double after = follow.centrePx.x - target.x;
    CHECK_NEAR(static_cast<float>(after), static_cast<float>(before * (1.0 - kDt / rules.followLagS)));

    for (int i = 0; i < 180; ++i) follow.Tick(rules, player, kView, kBounds, kDt);
    CHECK_MSG(Near(follow.centrePx, glm::dvec2(227.5, 128.0), 0.5),
              "settled with the level's left edge at the view's, at " + Px(follow.centrePx));
}

void ItNeverShowsPastTheLevel() {
    const Camera::Rules rules{0.12, 0.0};
    Camera::Follow follow;
    follow.Start(rules, {384.0, 0.0}, kView, kBounds);
    const glm::dvec2 far[] = {{-500.0, -500.0}, {2000.0, 900.0}};
    for (const glm::dvec2& player : far) {
        bool inside = true;
        for (int i = 0; i < 240; ++i) {
            follow.Tick(rules, player, kView, kBounds, kDt);
            const glm::dvec2 c = follow.centrePx;
            inside = inside && c.x - kView.x / 2 >= 0.0 && c.x + kView.x / 2 <= kBounds.x &&
                     c.y - kView.y / 2 >= 0.0 && c.y + kView.y / 2 <= kBounds.y;
        }
        CHECK_MSG(inside, "following a player at " + Px(player));
    }
    // A window reshaped under it: the camera is held inside again on the next tick.
    follow.centrePx = glm::dvec2(700.0, 128.0);
    follow.Tick(rules, glm::dvec2(700.0, 128.0), kView, kBounds, kDt);
    CHECK(follow.centrePx.x <= 540.5);
}

void ThePortsViewIsTwoHundredFiftySixTall() {
    // view.json is the port's own and committed, so it is always there. Its
    // number is an inference its note gives the evidence for, so it is pinned
    // here: a change to it is a decision, and should be seen.
    double height = 0.0;
    std::string error;
    CHECK_MSG(Camera::LoadViewHeight(std::string(MAGICPORTALS_PORT_DATA_DIR) + "/view.json", height, error), error);
    CHECK(height == 256.0);
}

// ---- the earthquake ------------------------------------------------------------

// The layer's tick, as its clock counts it: a float 1/60 s in milliseconds.
constexpr double kTickMs = static_cast<double>(1.0f / 60.0f) * 1000.0;

struct Tremble {
    int tick = 0;
    double px = 0.0;
};

// `ticks` updates on the layer's clock from `clockMs`, which is carried on; each
// tremble with the tick it fell on, counted from `firstTick`.
std::vector<Tremble> Shake(const Earthquake::Rules& rules, Earthquake::Generator& quake, int ticks, double& clockMs,
                           int firstTick = 0, double tickMs = kTickMs) {
    std::vector<Tremble> out;
    for (int tick = firstTick; tick < firstTick + ticks; ++tick) {
        const double px = quake.Update(rules, clockMs);
        if (px != 0.0) out.push_back(Tremble{tick, px});
        clockMs += tickMs;
    }
    return out;
}

Earthquake::Rules LoadQuake() {
    Earthquake::Rules rules;
    std::string error;
    const bool read =
        Earthquake::LoadRules(std::string(MAGICPORTALS_PORT_DATA_DIR) + "/earthquake.json", rules, error);
    CHECK_MSG(read, error);
    return rules;
}

std::string Sequence(const std::vector<Tremble>& trembles) {
    std::string out;
    for (const Tremble& t : trembles) out += " " + std::to_string(t.tick) + ":" + std::to_string(t.px);
    return out;
}

void TheEarthquakeIsTheDecodedGenerator() {
    const Earthquake::Rules rules = LoadQuake();
    // EarthquakeGenerator's own literals (bytes 109623..110425) and R12's screen.
    CHECK(rules.forceScale == 0.35);
    CHECK(rules.capPx == 50.0);
    CHECK(rules.stopPx == 1.0);
    CHECK(rules.trembleIntervalMs == 70.0);
    CHECK(rules.backScale == 0.5);
    CHECK(rules.forthFirst);
    CHECK(rules.screenPxPerUnit == 2.8125);
    CHECK(rules.wallScaleKey == "destroyEffectScale");
    CHECK(rules.wallEntity == "breakable_wall");
    // Both spellings a level uses, and not the torch's light wall.
    CHECK(Earthquake::IsEntity("breakable_wall", rules.wallEntity));
    CHECK(Earthquake::IsEntity("breakable_wall.ent", rules.wallEntity));
    CHECK(!Earthquake::IsEntity("light_wall.ent", rules.wallEntity));
    CHECK(rules.mouthLeg == 3);
    CHECK(rules.mouthBiasAbove == 0.2);

    // Every call site's intensity and the side of the update it lands on, as
    // the listing pushes it and as its caller's .ent says it runs.
    using Earthquake::Call;
    using Earthquake::When;
    const struct {
        Call call;
        double intensity;
        When when;
    } rows[] = {{Call::Fall, 20.0, When::AfterUpdate},
                {Call::FallNoGravity, 5.0, When::AfterUpdate},
                {Call::BarrelBlast, 50.0, When::BeforeUpdate},
                {Call::BeholderDeath, 50.0, When::BeforeUpdate},
                {Call::DarkDragonDeath, 50.0, When::BeforeUpdate},
                {Call::GhostDeath, 50.0, When::AfterUpdate},
                {Call::WallStruck, 30.0, When::BeforeUpdate},
                {Call::WallDestroyed, 20.0, When::AfterUpdate},
                {Call::StoneDespawned, 6.0, When::BeforeUpdate},
                {Call::BossRockBroken, 5.0, When::BeforeUpdate},
                {Call::BeholderHurt, 24.0, When::BeforeUpdate},
                {Call::DarkDragonMouth, 10.0, When::BeforeUpdate},
                {Call::DarkDragonShot, 15.0, When::BeforeUpdate},
                {Call::DoorLanded, 4.0, When::AfterUpdate},
                {Call::DoorOpening, 4.0, When::AfterUpdate},
                {Call::DoorClosing, 4.0, When::AfterUpdate},
                {Call::FireballEnd, 5.0, When::BeforeUpdate},
                {Call::EnemyShotEnd, 10.0, When::BeforeUpdate},
                {Call::MinionKilled, 20.0, When::BeforeUpdate},
                {Call::DoorUnlocked, 20.0, When::AfterUpdate},
                {Call::ShockDiamondPicked, 15.0, When::AfterUpdate},
                {Call::FireDiamondPicked, 15.0, When::AfterUpdate},
                {Call::ShockStrike, 25.0, When::AfterUpdate},
                {Call::LightWallGone, 20.0, When::AfterUpdate}};
    CHECK(std::size(rows) == Earthquake::kCalls);
    for (const auto& row : rows) {
        const Earthquake::CallRule& rule = rules.Of(row.call);
        CHECK_MSG(rule.intensity == row.intensity && rule.when == row.when, Earthquake::KeyOf(row.call));
    }
}

void ATwentyIsFivePairsFiveTicksApart() {
    // The light wall's 20, as L1 measured it on 4-29: -20, +20, -10, +10, -5, +5,
    // -2, +2, -1, +1 screen px (the footage's dy is the world's, so a forth - the
    // camera down - reads negative there). 0.35 x 20 x 2.8125 = 19.6875 px, 7 u.
    const Earthquake::Rules rules = LoadQuake();
    Earthquake::Generator quake;
    quake.Reset(rules);
    quake.Start(rules, 20.0);
    CHECK(quake.amplitudePx == 19.6875);
    double clockMs = 1000.0;
    const std::vector<Tremble> got = Shake(rules, quake, 120, clockMs);
    const double want[] = {19.6875, -19.6875, 9.84375, -9.84375, 4.921875, -4.921875,
                           2.4609375, -2.4609375, 1.23046875, -1.23046875};
    CHECK_MSG(got.size() == std::size(want), Sequence(got));
    bool same = got.size() == std::size(want);
    for (std::size_t i = 0; same && i < got.size(); ++i) {
        // Due at 70 ms: the 5th tick of 16.67 ms (83.3), never the 4th (66.7).
        same = got[i].px == want[i] && got[i].tick == static_cast<int>(i) * 5;
    }
    CHECK_MSG(same, "the forth first, back by the saved value, halved on the back only:" + Sequence(got));
    // In the level's units, the first pair is 7 u each way.
    CHECK(Earthquake::ToUnits(rules, want[0]) == 7.0);
    // Over after the tenth: the stop took 0.615 px to 0.
    CHECK(!quake.Running());
    CHECK(!got.empty() && got.back().tick == 45);
    // And it leaves the phase where it found it, forth next.
    CHECK(quake.forth);
}

void EveryShakeRunsTheDecodedPairs() {
    // D11 section 1c's table at 720p (D11_check C11): pairs before the stop, and
    // the opening amplitude, capped at 50 px.
    const Earthquake::Rules rules = LoadQuake();
    const struct {
        double intensity;
        std::size_t pairs;
        double firstPx;
    } table[] = {{3.0, 2, 2.953125},  {4.0, 2, 3.9375},   {5.0, 3, 4.921875},  {6.0, 3, 5.90625},
                 {7.0, 3, 6.890625},  {10.0, 4, 9.84375}, {15.0, 4, 14.765625}, {20.0, 5, 19.6875},
                 {24.0, 5, 23.625},   {25.0, 5, 24.609375}, {30.0, 5, 29.53125}, {40.0, 6, 39.375},
                 {50.0, 6, 49.21875}, {60.0, 6, 50.0}};
    for (const auto& row : table) {
        Earthquake::Generator quake;
        quake.Reset(rules);
        quake.Start(rules, row.intensity);
        double clockMs = 0.0;
        const std::vector<Tremble> got = Shake(rules, quake, 200, clockMs);
        // Near rather than equal: 0.35 has no exact double, so 4, 15 and 30 land
        // a last bit under the decimal the table prints.
        const bool ok = got.size() == row.pairs * 2 && !got.empty() && std::fabs(got.front().px - row.firstPx) < 1e-9 &&
                        got.back().px < 0.0 && !quake.Running();
        CHECK_MSG(ok, "intensity " + std::to_string(row.intensity) + ":" + Sequence(got));
    }
}

void ACallOverwritesAndTheBackUndoesTheOldForth() {
    // F6 (INDEX), decoded (D11_check C21): a fireball's 5 trembles forth, then a
    // barrel's 50 and the wall's 20 land before its back. The back undoes the 5's
    // saved 4.92 and halves the 20, which is therefore never shown whole:
    // -5, +5, -10, +10, -5, +5, -2, +2, -1, +1 on the footage.
    const Earthquake::Rules rules = LoadQuake();
    Earthquake::Generator quake;
    quake.Reset(rules);
    double clockMs = 0.0;
    quake.Start(rules, 5.0);
    std::vector<Tremble> got = Shake(rules, quake, 2, clockMs);
    quake.Start(rules, 50.0);
    quake.Start(rules, 20.0); // over the 50, not the larger of the two
    const std::vector<Tremble> rest = Shake(rules, quake, 200, clockMs, 2);
    got.insert(got.end(), rest.begin(), rest.end());
    const double want[] = {4.921875, -4.921875, 9.84375, -9.84375, 4.921875, -4.921875,
                           2.4609375, -2.4609375, 1.23046875, -1.23046875};
    bool same = got.size() == std::size(want);
    for (std::size_t i = 0; same && i < got.size(); ++i) same = got[i].px == want[i];
    CHECK_MSG(same, Sequence(got));
}

void ASubPixelCallLeavesTheNextShakeOpeningOnABack() {
    // D11 section 1e.2, D11_check C23: a call under 1 px trembles forth once and
    // is stopped, leaving the phase on a back with a sub-pixel saved value. The
    // next shake then opens with that invisible back and halves before it is seen
    // - one reading of F16's half-size light wall. Nothing in the port calls
    // under 1 px today (the impact rows are not wired); the machine keeps it.
    const Earthquake::Rules rules = LoadQuake();
    Earthquake::Generator quake;
    quake.Reset(rules);
    double clockMs = 0.0;
    quake.Start(rules, 0.3); // 0.295 px
    const std::vector<Tremble> small = Shake(rules, quake, 10, clockMs);
    CHECK(small.size() == 1 && std::fabs(small.front().px - 0.2953125) < 1e-12);
    CHECK(!quake.Running() && !quake.forth);
    quake.Start(rules, 20.0);
    const std::vector<Tremble> next = Shake(rules, quake, 200, clockMs, 10);
    CHECK_MSG(next.size() >= 2 && std::fabs(next[0].px + 0.2953125) < 1e-12 && next[1].px == 9.84375, Sequence(next));
}

void ACallEveryTickHoldsTheShakeWhole() {
    // The dark dragon's mouth leg (row 16): 10 re-armed on every frame, so every
    // forth is the whole 9.84 and every back undoes it; F16 shows -10, +10 four
    // pairs running, then the decay once the leg is over.
    const Earthquake::Rules rules = LoadQuake();
    Earthquake::Generator quake;
    quake.Reset(rules);
    double clockMs = 0.0;
    std::vector<Tremble> held;
    for (int tick = 0; tick < 48; ++tick) {
        quake.Start(rules, 10.0);
        const double px = quake.Update(rules, clockMs);
        if (px != 0.0) held.push_back(Tremble{tick, px});
        clockMs += kTickMs;
    }
    bool whole = held.size() == 10;
    for (std::size_t i = 0; whole && i < held.size(); ++i) whole = std::fabs(held[i].px) == 9.84375;
    CHECK_MSG(whole, Sequence(held));
    // Once the calls stop, the last one's whole forth and back, and only then
    // the halving.
    const std::vector<Tremble> decay = Shake(rules, quake, 200, clockMs, 48);
    CHECK_MSG(decay.size() >= 3 && decay[0].px == 9.84375 && decay[1].px == -9.84375 && decay[2].px == 4.921875,
              Sequence(decay));
}

void TheIntervalIsSeventyMilliseconds() {
    // Every 5th frame at 60 fps and every 3rd at 30 (D11 section 1d): the 70 ms
    // is a floor, met on the first frame past it.
    const Earthquake::Rules rules = LoadQuake();
    Earthquake::Generator quake;
    quake.Reset(rules);
    quake.Start(rules, 20.0);
    double clockMs = 0.0;
    const std::vector<Tremble> slow = Shake(rules, quake, 60, clockMs, 0, 1000.0 / 30.0);
    CHECK_MSG(slow.size() >= 3 && slow[1].tick == 3 && slow[2].tick == 6, Sequence(slow));
    // And exactly 70 is due: TNS is "not less than" (D11_check C8).
    Earthquake::Generator exact;
    exact.Reset(rules);
    exact.Start(rules, 20.0);
    CHECK(exact.Update(rules, 0.0) > 0.0);
    CHECK(exact.Update(rules, 69.9) == 0.0);
    CHECK(exact.Update(rules, 70.0) < 0.0);
}

void TheMouthLegIsTheFourthWaypoint() {
    // darkdragon.json appearing.hold_ms: legs 0..6 hold 2500, 0, 1000, 1000, 700,
    // 2200, 0, so leg 3 runs 3500..4500 ms and its bias passes 0.2 at 3700.
    const Earthquake::Rules rules = LoadQuake();
    const std::vector<double> holds{2500.0, 0.0, 1000.0, 1000.0, 700.0, 2200.0, 0.0};
    CHECK(!Earthquake::InMouthLeg(rules, holds, 1000.0));
    CHECK(!Earthquake::InMouthLeg(rules, holds, 3500.0));
    CHECK(!Earthquake::InMouthLeg(rules, holds, 3700.0));
    CHECK(Earthquake::InMouthLeg(rules, holds, 3700.1));
    CHECK(Earthquake::InMouthLeg(rules, holds, 4499.9));
    CHECK(!Earthquake::InMouthLeg(rules, holds, 4500.0));
    CHECK(!Earthquake::InMouthLeg(rules, holds, 6000.0));
}

void runTests() {
    ThePortsViewIsTwoHundredFiftySixTall();
    TheViewStaysInsideTheLevel();
    ItHoldsThenEasesToThePlayer();
    ItNeverShowsPastTheLevel();
    TheEarthquakeIsTheDecodedGenerator();
    ATwentyIsFivePairsFiveTicksApart();
    EveryShakeRunsTheDecodedPairs();
    ACallOverwritesAndTheBackUndoesTheOldForth();
    ASubPixelCallLeavesTheNextShakeOpeningOnABack();
    ACallEveryTickHoldsTheShakeWhole();
    TheIntervalIsSeventyMilliseconds();
    TheMouthLegIsTheFourthWaypoint();
}

} // namespace

TEST_MAIN("test_mp_camera", 80)
