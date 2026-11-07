// Seesaws and spinning platforms: the bodies the original pins to an anchor.
//
// The one Magic Portals role with no tick of its own. Hinge::Find attaches a
// JointComponent when the level is built and the engine's solver does the rest,
// so what is pinned here is the ARITHMETIC that turns the original's numbers into
// that component - and one thing about the engine, which is that a joint stops
// its two ends colliding (test_physics owns that check; without it a platform
// climbs out of the anchor it hangs from and none of this would hold).
//
// THE ANGLE IS THE PART THAT CAN SILENTLY BE WRONG. Box2D measures the joint from
// the pose the level was BUILT at, and the engine measures from where the two
// bodies' reference directions coincide, so the authored range is measured from
// the rest angle rather than used as it is. It is ADDED: a body turned about the
// engine's +z RAISES the hinge angle, so the engine's sense runs with the
// original's. That was briefly disbelieved on the strength of a reading taken
// three seconds in with gravity on - by then gravity had dragged the bar back
// past rest, which is indistinguishable from an inverted sign - so the direction
// is read SIX TICKS in here, before anything else can touch it.
//
// level30a is the only level that could say so. Its seesaw runs from -1.1325 to
// 0.4382; every other placement in the game is a symmetric quarter turn and would
// pass either way, which is the whole reason the uneven one is under test.
//
// The bar is 254 px long on a 127 px arm, and a level built around one has
// geometry above it and below: swung the CW way it meets the T-stem near 1.81
// before its lower stop. Swung the CCW way it reaches its UPPER stop, 3.1958 (past
// pi), and is held there (ASeesawNeverJumpsAcrossPi; planning step 226: it used to
// read 2.2 to 2.4 and was called "the level's own geometry", and was the engine's
// angle wrapping at pi). What is pinned is the direction it turns, that it never
// passes either stop, that the upper stop holds, and the arithmetic that puts the
// stops where they are.
//
// Reads the converted levels and Enhanced's data from outside this repository,
// and skips, saying where it looked, when either is absent.

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "core/Components.hpp"
#include "core/PhysicsSettings.hpp"
#include "sim/Game.hpp"
#include "sim/Hinge.hpp"
#include "sim/Units.hpp"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>

using namespace MagicPortals;
using Supersonic::JointComponent;
using Supersonic::RigidBodyComponent;

namespace {

const std::string kLevels = MAGICPORTALS_LEVELS_DIR;
const std::string kData = MAGICPORTALS_DATA_DIR;
const std::string kPortData = MAGICPORTALS_PORT_DATA_DIR;
constexpr float kStep = 1.0f / 60.0f;

// 2-28 pins a spinning platform; 2-31 a seesaw, and the only one in the game
// whose range is uneven.
constexpr const char* kPlatformLevel = "level27a";
constexpr const char* kSeesawLevel = "level30a";

struct Run {
    Game::Data data;
    entt::registry registry;
    Game::Level level;
};

bool Begin(const std::string& levelName, Run& run, bool withStatics = true) {
    std::string error;
    const bool ok = Game::LoadData(kLevels + "/" + levelName + ".tscn", kData,
                                   std::filesystem::temp_directory_path() / "supersonic-test-mp-hinge", run.data,
                                   error) &&
                    Game::Start(run.data, run.registry, run.level, error, withStatics);
    CHECK_MSG(ok, levelName + ": " + error);
    return ok;
}

void Tick(Run& run) {
    Game::Tick(run.data, run.registry, run.level, 0.0f, kStep);
}

std::filesystem::path Scratch() {
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "supersonic-test-mp-hinge";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir;
}

std::string WriteRules(const char* name, const std::string& body) {
    const std::filesystem::path path = Scratch() / name;
    std::ofstream file(path, std::ios::trunc);
    file << body;
    return path.string();
}

void TheReaderRefusesWhatWouldBeSilent() {
    Hinge::Rules rules;
    std::string error;
    CHECK_MSG(Hinge::LoadRules(kPortData + "/hinge.json", rules, error), kPortData + "/hinge.json: " + error);
    CHECK_MSG(::test::nearly(static_cast<float>(rules.stiffness), 0.8f), "the port runs at the engine's own 0.8");

    // Nothing taken out is a hinge that drifts apart under load; more than all of
    // it over-corrects and shakes. Both look like a level that works until
    // something stands on the platform.
    const std::string none = WriteRules("no-stiffness.json", R"({"joint": {"stiffness": 0.0}})");
    CHECK_MSG(!Hinge::LoadRules(none, rules, error), "a stiffness of nothing is refused");

    const std::string over = WriteRules("over-stiffness.json", R"({"joint": {"stiffness": 1.5}})");
    CHECK_MSG(!Hinge::LoadRules(over, rules, error), "and so is more than all of it");
}

void WhatTheLevelsCarry() {
    Run run;
    if (!Begin(kPlatformLevel, run)) return;

    CHECK_EQ(static_cast<int>(run.level.hinge.hinges.size()), 1);
    if (run.level.hinge.hinges.empty()) return;

    const Hinge::Hinged& hinged = run.level.hinge.hinges.front();
    CHECK_MSG(hinged.limited, "level27a's platform is limited");
    CHECK_MSG(::test::nearly(static_cast<float>(hinged.lowerRad), -1.5708f, 1e-3f) &&
                  ::test::nearly(static_cast<float>(hinged.upperRad), 1.5708f, 1e-3f),
              "a quarter turn either way, as its .ent says");
    CHECK_MSG(run.registry.valid(hinged.anchor), "and it is pinned to the anchor the level places");
    CHECK_MSG(run.registry.all_of<JointComponent>(hinged.body),
              "the body carries the joint the solver reads");

    const JointComponent& joint = run.registry.get<JointComponent>(hinged.body);
    CHECK_MSG(joint.type == JointComponent::Type::Hinge, "as a hinge");
    CHECK_MSG(joint.connectedBody == hinged.anchor, "to that anchor");
    CHECK_MSG(!joint.useMotor, "with no motor: every placement disables it, and the original zeroes it besides");
    CHECK_MSG(!joint.collideConnected,
              "and not colliding with what it hangs from, which in this level overlaps it by 89 px");
    std::printf("  %s: %s pinned to %s\n", kPlatformLevel, hinged.name.c_str(), hinged.anchorName.c_str());
}

void TheAuthoredRangeIsMeasuredFromTheRestAngle() {
    Run run;
    if (!Begin(kSeesawLevel, run)) return;
    if (run.level.hinge.hinges.empty()) {
        CHECK_MSG(false, "level30a pins a seesaw");
        return;
    }

    // Added to the rest angle, each bound keeping its place: the engine's hinge
    // angle runs with the original's.
    const Hinge::Hinged& hinged = run.level.hinge.hinges.front();
    CHECK_MSG(::test::nearly(static_cast<float>(hinged.minRad),
                             static_cast<float>(hinged.restRad + hinged.lowerRad), 1e-4f),
              "the range is measured from where the level was built");
    CHECK_MSG(::test::nearly(static_cast<float>(hinged.maxRad),
                             static_cast<float>(hinged.restRad + hinged.upperRad), 1e-4f),
              "at both ends");

    // The uneven one. If this reads as symmetric, the level was misread.
    CHECK_MSG(std::fabs(hinged.lowerRad + hinged.upperRad) > 0.5,
              "and level30a's own range is uneven, which is what makes it the test");
    std::printf("  %s: %.4f to %.4f, resting at %.4f\n", kSeesawLevel, hinged.minRad, hinged.maxRad, hinged.restRad);
}

// THE sign test. Turned one way it must stop at the near limit, and the other
// way at the far one - and those are 1.13 and 0.44 radians from rest, so a
// swapped or negated range lands against the wrong stop by a wide margin.
void ASeesawStopsAtItsOwnEnds() {
    // GRAVITY OFF, so the joint is the only thing that can stop it. Left on, the
    // plank is dragged back across its rest angle by gravity and the reading says
    // nothing about where the stops are. (An earlier comment here said it settles
    // at 2.1113; the plank at rest stays at 2.7576 until something lands on it.)
    // outStart is the angle BEFORE anything is stepped. Everything here rests on
    // restRad - computed from the level's rotations - being the angle the solver
    // actually sees, and that has never been measured. If the two differ, the
    // limits are offset from reality and the solver drags the body to a limit at
    // once, which looks exactly like a body swinging.
    const auto swing = [](double spin, double& outAngle, double& outRest, double& outMin, double& outMax,
                          double& outStart, double& outEarly) {
        Run run;
        if (!Begin(kSeesawLevel, run)) return false;
        if (run.level.hinge.hinges.empty()) return false;
        const Hinge::Hinged& hinged = run.level.hinge.hinges.front();
        outMin = hinged.minRad;
        outMax = hinged.maxRad;
        outRest = hinged.restRad;
        outStart = Hinge::AngleOf(run.registry, hinged);

        Supersonic::PhysicsSettings settings;
        settings.gravity = glm::vec3(0.0f);
        run.registry.ctx().insert_or_assign<Supersonic::PhysicsSettings>(std::move(settings));

        if (!run.registry.all_of<RigidBodyComponent>(hinged.body)) return false;
        run.registry.get<RigidBodyComponent>(hinged.body).angularVelocity =
            glm::vec3(0.0f, 0.0f, static_cast<float>(spin));

        for (int tick = 0; tick < 6; ++tick) Tick(run);
        outEarly = Hinge::AngleOf(run.registry, hinged);
        for (int tick = 0; tick < 234; ++tick) Tick(run);
        outAngle = Hinge::AngleOf(run.registry, hinged);
        return true;
    };

    double angle = 0.0;
    double rest = 0.0;
    double minRad = 0.0;
    double maxRad = 0.0;
    double start = 0.0;
    double early = 0.0;

    // Up first, and the direction is read six ticks in - the only window in which
    // nothing but the joint has touched the bar.
    if (!swing(3.0, angle, rest, minRad, maxRad, start, early)) return;
    std::printf("  %s: +z from %.4f (rest says %.4f), after 6 ticks %.4f, stopped %.4f, ends %.4f..%.4f\n",
                kSeesawLevel, start, rest, early, angle, minRad, maxRad);
    CHECK_MSG(::test::nearly(static_cast<float>(start), static_cast<float>(rest), 1e-3f),
              "the angle the solver sees at rest is the one the limits were built from, got " +
                  std::to_string(start) + " against " + std::to_string(rest));
    CHECK_MSG(early > start + 0.1, "turning about +z RAISES the hinge angle, got " + std::to_string(early) +
                                       " from " + std::to_string(start));
    // NOTE: the reading is an atan2 and never exceeds pi, while this stop is at 3.1958: this bound CANNOT fail for it.
    // That the stop HOLDS is pinned by ASeesawNeverJumpsAcrossPi, which follows the bar's travel round the circle.
    CHECK_MSG(angle <= maxRad + 0.08, "and it never passes the upper stop, got " + std::to_string(angle) +
                                          " against " + std::to_string(maxRad));

    // And down. The CW end is NOT reached - the bar meets the level's own T-stem near
    // 1.81 first - so what is checked is that it goes the right way and never passes the
    // stop. (The +z end IS reached: see ASeesawNeverJumpsAcrossPi.)
    if (!swing(-3.0, angle, rest, minRad, maxRad, start, early)) return;
    std::printf("  %s: -z from %.4f (rest says %.4f), after 6 ticks %.4f, stopped %.4f, ends %.4f..%.4f\n",
                kSeesawLevel, start, rest, early, angle, minRad, maxRad);
    CHECK_MSG(early < start - 0.1, "and about -z it falls, got " + std::to_string(early) + " from " +
                                       std::to_string(start));
    CHECK_MSG(angle >= minRad - 0.08, "stopping at the lower end rather than past it, got " +
                                          std::to_string(angle) + " against " + std::to_string(minRad));
    CHECK_MSG(std::fabs((rest - minRad) - 1.1325) < 1e-3 && std::fabs((maxRad - rest) - 0.4382) < 1e-3,
              "with the long 1.1325 below rest and the short 0.4382 above, as level30a authors it");
}

// ---- ATTACH POINT B (planning step 174, CMP-2) -------------------------------------------------------------------------
//
// ETHRevoluteJoint.cpp:35-44 puts the joint's two ends on `attachPointA x half of A's box` and `attachPointB x half of B's
// box` (:60-64), and Box2D pulls the DYNAMIC body until the two world points meet. A is the swinging body, B the static
// `anchor`, and A's attach point is its own centre in all ten placements, so the body's centre ends on B's world point:
// B's centre plus B's attach point turned by B's angle.
//
// THE CONVENTION. A node's tscn `rotation` is Box2D's body angle (-radians of the .esc's: ETHPhysicsSimulator.cpp:76); a
// local offset is turned by it in the level's own space (+y down), x' = c x - s y, y' = s x + c y. The tests below do that
// arithmetic THEMSELVES, from the scene file, so they do not borrow Hinge.cpp's.

// One placement per row, from the census (out/parity/program/CMP2/census.md, derived from the original's own .esc by census.py):
// the move of the body's centre from where the level places it to where its joint holds it, in px (+x right, +y down).
struct Pull {
    const char* level;
    const char* name; // 1-based, as the game names it
    double dx;
    double dy;
};
constexpr Pull kPulls[] = {
    {"level27a", "2-28", +0.6, +2.0},       // anchor 885 (460, 166), B (-102.4, 0); plank placed (357, 164)
    {"level28a", "2-29", +0.6, 0.0},        // anchor 885 (460, 144), B (-102.4, 0)
    {"level29a", "2-30", 0.0, 0.0},         // anchor 916 (256, 221) at -90 deg: (-32, 0) turned to (0, -32) = the plank's own centre
    {"level30a", "2-31", -14.037, +28.496}, // anchor 957 (280, 270) at 187 deg, B (-64, 12.8): the seesaw 31.8 px away
    {"level14b", "3-15", +4.0, 0.0},        // anchor 1011 (588, 216) at -90 deg
    {"level18b", "3-19", 0.0, +8.0},        // anchor 1553 (128, 112) 256 x 32, B (32, 8); the plank is turned 90 deg
    {"level19b", "3-20", -2.0, 0.0},        // anchor 1553 (448, 160) 256 x 32, B (-128, 0)
    {"level21b", "3-22", 0.0, 0.0},         // anchor 1011 (386, 224) at -90 deg
    {"level10c", "4-11", -0.4, 0.0},        // anchor 2131 (262, 128) 16 x 16, B (-6.4, 0)
    {"level30c", "4-31", 0.0, -3.0},        // anchor 2095 (472, 80) 128 x 32, B (64, 0)
};

glm::dvec2 Turn(const glm::dvec2& v, double radians) {
    const double c = std::cos(radians);
    const double s = std::sin(radians);
    return glm::dvec2(v.x * c - v.y * s, v.x * s + v.y * c);
}

const Tscn::Node* NodeNamed(const Tscn::Scene& scene, const std::string& name) {
    for (const Tscn::Node& node : scene.nodes) {
        if (node.parent == "." && node.name == name) return &node;
    }
    return nullptr;
}

double Number(const Tscn::Node& node, const char* key) {
    const Tscn::Value* value = node.Meta(key);
    double read = 0.0;
    if (value == nullptr || !value->AsNumber(read)) {
        CHECK_MSG(false, node.name + " carries no metadata/" + key);
        return 0.0;
    }
    return read;
}

glm::dvec2 PlacedAt(const Tscn::Node& node) {
    const Tscn::Value* value = node.Find("position");
    if (value == nullptr || value->kind != Tscn::Value::Kind::Vector2) return glm::dvec2(0.0);
    return glm::dvec2(value->numbers[0], value->numbers[1]);
}

double TurnedBy(const Tscn::Node& node) {
    const Tscn::Value* value = node.Find("rotation");
    double read = 0.0;
    return value != nullptr && value->AsNumber(read) ? read : 0.0;
}

glm::dvec2 BodyAtPx(Run& run, const Hinge::Hinged& hinged) {
    return Units::ToPixels(run.registry.get<Supersonic::TransformComponent>(hinged.body).position);
}

// Where the joint of `swinging` holds A's centre, from the scene file alone: B's centre plus attach point B turned by B's
// angle, less A's own attach point turned by A's (zero in all ten, so the centre; the term stays so the rule is whole).
bool HeldAt(const Tscn::Scene& scene, const Tscn::Node& swinging, glm::dvec2& placed, glm::dvec2& held) {
    const Tscn::Value* other = swinging.Meta("joint_other");
    if (other == nullptr || other->kind != Tscn::Value::Kind::String) {
        CHECK_MSG(false, swinging.name + " names no joint_other");
        return false;
    }
    const Tscn::Node* anchor = NodeNamed(scene, other->text);
    CHECK_MSG(anchor != nullptr, swinging.name + "'s joint_other " + other->text + " is placed");
    if (anchor == nullptr) return false;
    const glm::dvec2 attachB(Number(swinging, "joint_anchor_b_x"), Number(swinging, "joint_anchor_b_y"));
    const glm::dvec2 attachA(Number(swinging, "joint_anchor_x"), Number(swinging, "joint_anchor_y"));
    placed = PlacedAt(swinging);
    held = PlacedAt(*anchor) + Turn(attachB, TurnedBy(*anchor)) - Turn(attachA, TurnedBy(swinging));
    return true;
}

// WHERE EACH HINGED BODY STANDS. The ten census levels, at the start, a tick in, and well on: the body's centre is on its
// joint's world point (to 0.01 px at the start, and the solver then adds nothing: 0.0003 px at the worst over 420 ticks,
// under whatever the level puts on it), and the move from where the level placed it is the census's.
void EveryHingedBodyStandsWhereItsJointHoldsIt() {
    int moved = 0;
    for (const Pull& pull : kPulls) {
        const std::string level = std::string(pull.name) + " (" + pull.level + ")";
        Run run;
        if (!Begin(pull.level, run)) continue;
        if (run.level.hinge.hinges.size() != 1) {
            CHECK_MSG(false, level + " pins exactly one body");
            continue;
        }
        const Hinge::Hinged& hinged = run.level.hinge.hinges.front();
        const Tscn::Node* node = NodeNamed(run.data.scene, hinged.name);
        if (node == nullptr) {
            CHECK_MSG(false, level + ": " + hinged.name + " is a node of the scene");
            continue;
        }
        glm::dvec2 placed(0.0);
        glm::dvec2 held(0.0);
        if (!HeldAt(run.data.scene, *node, placed, held)) continue;

        // The arithmetic from the file against the census's numbers: the two come from different places (the original's
        // own .esc, and this level's converted file).
        CHECK_MSG(std::fabs((held.x - placed.x) - pull.dx) < 0.01 && std::fabs((held.y - placed.y) - pull.dy) < 0.01,
                  level + ": the joint pulls the body (" + std::to_string(held.x - placed.x) + ", " +
                      std::to_string(held.y - placed.y) + "), the census says (" + std::to_string(pull.dx) + ", " +
                      std::to_string(pull.dy) + ")");

        const glm::dvec2 start = BodyAtPx(run, hinged);
        CHECK_MSG(std::fabs(start.x - held.x) < 0.01 && std::fabs(start.y - held.y) < 0.01,
                  level + ": the body starts at (" + std::to_string(start.x) + ", " + std::to_string(start.y) +
                      ") where its joint holds it (" + std::to_string(held.x) + ", " + std::to_string(held.y) + ")");
        if (std::fabs(pull.dx) + std::fabs(pull.dy) > 0.0) ++moved;

        glm::dvec2 worst(0.0);
        const int stops[] = {1, 60, 420};
        int tick = 0;
        for (const int stop : stops) {
            for (; tick < stop; ++tick) Tick(run);
            const glm::dvec2 at = BodyAtPx(run, hinged);
            worst = glm::max(worst, glm::abs(at - held));
        }
        CHECK_MSG(worst.x < 0.01 && worst.y < 0.01,
                  level + ": the joint keeps the body on its point for 420 ticks, off by at most (" +
                      std::to_string(worst.x) + ", " + std::to_string(worst.y) + ") px");
        std::printf("  %s: placed (%.3f, %.3f) -> held (%.3f, %.3f), pull (%+.3f, %+.3f), start (%.3f, %.3f), off by at most (%.4f, %.4f)\n",
                    level.c_str(), placed.x, placed.y, held.x, held.y, held.x - placed.x, held.y - placed.y, start.x,
                    start.y, worst.x, worst.y);
    }
    // 8 of the 10 move: 2-30 and 3-22 are the two whose B anchor already is the plank's centre.
    CHECK_EQ(moved, 8);
}

// THE TWO THAT DO NOT MOVE. 2-30 and 3-22 have an attach point B that already lands on the plank's centre: the old
// arithmetic (the plank's pin seen from the anchor, turned by minus the anchor's angle) was verified on them by the level's
// own play, so the new connectedAnchor, which is the attach point itself, has to agree with it - which is also what pins the
// sense of B's turn, for a wrong one sends B's point 64 px away on both.
void AJointWhoseEndsAlreadyMeetMovesNothing() {
    for (const char* level : {"level29a", "level21b"}) {
        Run run;
        if (!Begin(level, run)) continue;
        if (run.level.hinge.hinges.empty()) {
            CHECK_MSG(false, std::string(level) + " pins a body");
            continue;
        }
        const Hinge::Hinged& hinged = run.level.hinge.hinges.front();
        const Tscn::Node* node = NodeNamed(run.data.scene, hinged.name);
        const Tscn::Node* anchor = NodeNamed(run.data.scene, hinged.anchorName);
        if (node == nullptr || anchor == nullptr) {
            CHECK_MSG(false, std::string(level) + ": both ends are nodes");
            continue;
        }
        const glm::dvec2 before = BodyAtPx(run, hinged);
        CHECK_MSG(glm::length(before - PlacedAt(*node)) < 1e-3, std::string(level) + ": the body stays where it was placed");

        // The old rule: the pin (the body's centre) in B's own frame.
        const glm::dvec2 oldLocal = Turn(PlacedAt(*node) - PlacedAt(*anchor), -TurnedBy(*anchor));
        const JointComponent& joint = run.registry.get<JointComponent>(hinged.body);
        const glm::dvec2 newLocal(joint.connectedAnchor.x * Units::kPixelsPerMetre,
                                  -joint.connectedAnchor.y * Units::kPixelsPerMetre);
        CHECK_MSG(glm::length(newLocal - oldLocal) < 1e-3,
                  std::string(level) + ": connectedAnchor (" + std::to_string(newLocal.x) + ", " +
                      std::to_string(newLocal.y) + ") is where the old rule put it (" + std::to_string(oldLocal.x) +
                      ", " + std::to_string(oldLocal.y) + ")");
    }
}

// A level's file with each of its lines passed through `rewrite` (which edits the line in place and says whether to keep
// it), written to the scratch folder under the level's own name and started: the scene a stale or edited conversion
// would be.
struct Scratched {
    Game::Data data;
    entt::registry registry;
    Game::Level level;
    std::string error;
    bool loaded = false;
    bool started = false;
};

template <typename Rewrite>
void StartRewritten(const char* levelName, Scratched& out, Rewrite rewrite) {
    std::ifstream file(kLevels + "/" + levelName + ".tscn", std::ios::binary);
    if (!file) {
        CHECK_MSG(false, std::string(levelName) + ".tscn is readable");
        return;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    std::istringstream lines(buffer.str());
    std::string text;
    for (std::string line; std::getline(lines, line);) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (rewrite(line)) text += line + '\n';
    }
    const std::filesystem::path path = Scratch() / (std::string(levelName) + ".tscn");
    {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        stream << text;
    }
    out.loaded = Game::LoadData(path.string(), kData, Scratch(), out.data, out.error);
    CHECK_MSG(out.loaded, std::string(levelName) + ": the rewritten scene still loads: " + out.error);
    if (!out.loaded) return;
    out.started = Game::Start(out.data, out.registry, out.level, out.error, true);
}

// A level converted before the converter carried attach point B would silently hold every plank at the anchor's centre
// (the key reads as absent, so as zero, 100 px off on a real level): it is refused, saying what to do. The keys are
// required, not defaulted, so a file that has the node but lost the numbers (an edited one) is refused as well.
void ALevelWithoutItsAttachPointBIsRefused() {
    for (const bool dropTheNodeToo : {true, false}) {
        Scratched stale;
        int dropped = 0;
        StartRewritten("level30a", stale, [&dropped, dropTheNodeToo](std::string& line) {
            const bool node = line.starts_with("metadata/joint_other");
            const bool numbers = line.starts_with("metadata/joint_anchor_b_");
            if ((dropTheNodeToo && node) || numbers) {
                ++dropped;
                return false;
            }
            return true;
        });
        const std::string which = dropTheNodeToo ? "no attach point B at all" : "joint_other but not its numbers";
        CHECK_EQ(dropped, dropTheNodeToo ? 3 : 2);
        if (!stale.loaded) continue;
        CHECK_MSG(!stale.started, "a level with " + which + " does not start");
        CHECK_MSG(stale.error.find("attach point B") != std::string::npos, "and says why: " + stale.error);
    }
}

// The converter's choice of B and the custom datum have to name the same entity (`revoluteJoint = "anchor"`): a
// joint_other that is another kind of node is a stale or edited file, and is refused rather than pinned to it.
void ANodeThatIsNotTheEntityNamedIsRefused() {
    Scratched edited;
    int swapped = 0;
    StartRewritten("level30a", edited, [&swapped](std::string& line) {
        if (line.starts_with("metadata/joint_other = ")) {
            ++swapped;
            // The level's first platform, placed at (52, 160): not an `anchor`.
            line = "metadata/joint_other = \"double_block_plat_no_emissive_ent_956\"";
        }
        return true;
    });
    CHECK_EQ(swapped, 1);
    if (!edited.loaded) return;
    CHECK_MSG(!edited.started, "a joint_other that is not the entity the datum names does not start");
    CHECK_MSG(edited.error.find("is pinned to the entity 'anchor'") != std::string::npos,
              "and says why: " + edited.error);
}

// A's OWN attach point, which is its centre in all ten placements and so moves nothing there: the rule is whole, though.
// level30a with attach point A (20, -10) px: the body's centre stands where its anchor, (20, -10) turned with the body,
// lands on B's point, which is B's point less that turned offset: 22 px from B's point itself.
void ABodysOwnAttachPointStandsOnTheSamePoint() {
    Scratched edited;
    StartRewritten("level30a", edited, [](std::string& line) {
        if (line.starts_with("metadata/joint_anchor_x = ")) line = "metadata/joint_anchor_x = 20";
        if (line.starts_with("metadata/joint_anchor_y = ")) line = "metadata/joint_anchor_y = -10";
        return true;
    });
    if (!edited.started || edited.level.hinge.hinges.empty()) {
        CHECK_MSG(false, "level30a with an attach point A starts: " + edited.error);
        return;
    }
    const Hinge::Hinged& hinged = edited.level.hinge.hinges.front();
    const Tscn::Node* node = NodeNamed(edited.data.scene, hinged.name);
    glm::dvec2 placed(0.0);
    glm::dvec2 held(0.0);
    if (node == nullptr || !HeldAt(edited.data.scene, *node, placed, held)) return;
    const glm::dvec2 at = Units::ToPixels(edited.registry.get<Supersonic::TransformComponent>(hinged.body).position);
    CHECK_MSG(std::fabs(at.x - held.x) < 0.01 && std::fabs(at.y - held.y) < 0.01,
              "the body stands at (" + std::to_string(at.x) + ", " + std::to_string(at.y) +
                  "), where its own attach point is on B's: (" + std::to_string(held.x) + ", " + std::to_string(held.y) +
                  ")");
    // B's world point is (341.962, 249.494) (the census); the body's centre is 22 px from it, not on it.
    CHECK_MSG(std::hypot(held.x - 341.962, held.y - 249.494) > 20.0,
              "and that is not B's point itself: A's attach point was subtracted");
}

// 2-29's STONE (WT-10, CMP-18): the stone sits on the hinged plank until the plank tips it. Attach B moves that plank 0.6 px
// right, which is all this step may say about the stone's clock; what is printed is the plank's turn off its rest angle and
// the stone's centre every 20 ticks, to be read before and after the rule and nothing more - nothing is pinned.
// STEP 197 (CMP-18): game/data/contact_order.json steps this level's stone and plank in Box2D's order (move after the contact solve),
// and this table is built through Game::LoadData like every other, so it now reads the flagged pair: the plank first turns past 0.05
// rad on tick 210 (it was 254 on the engine's own order). It is printed and still not pinned, so the suite passes on an empty table as
// well, which is the control the exes are first compared on; the flagged world's numbers are pinned by test_mp_contact_order.
void TheStoneOf2_29RidesThePlank() {
    Run run;
    if (!Begin("level28a", run)) return;
    if (run.level.hinge.hinges.empty()) return;
    const Hinge::Hinged& hinged = run.level.hinge.hinges.front();
    entt::entity stone = entt::null;
    for (const auto& [name, entity] : run.level.built.entities) {
        if (name.starts_with("rolling_stone")) stone = entity;
    }
    if (stone == entt::null) {
        CHECK_MSG(false, "2-29 places a rolling stone");
        return;
    }
    int firstTurned = -1;
    for (int tick = 0; tick <= 460; ++tick) {
        const double turned = Hinge::AngleOf(run.registry, hinged) - hinged.restRad;
        const glm::dvec2 stoneAt = Units::ToPixels(run.registry.get<Supersonic::TransformComponent>(stone).position);
        if (firstTurned < 0 && std::fabs(turned) > 0.05) firstTurned = tick;
        if (tick % 20 == 0) {
            std::printf("  2-29 tick %3d: plank turned %+.4f rad, stone (%.2f, %.2f)\n", tick, turned, stoneAt.x, stoneAt.y);
        }
        Tick(run);
    }
    std::printf("  2-29: the plank first turns past 0.05 rad on tick %d\n", firstTurned);
}

// THE WRAP (planning step 226, the beta tester's 2-31 on a phone). Joints::HingeAngle is an atan2 and cannot read above pi, and this seesaw's
// window runs 1.6251 to 3.1958: its upper stop is 0.054 past it. A bar turned that far read as about -3.1, 4.7 radians below its own lower
// stop, and the position pass turned it 270 degrees in one step to put it back - onto the far stop, from 10 degrees past level one way to
// 65 degrees the other in two ticks. ASeesawStopsAtItsOwnEnds could not see it: it bounds the angle from above only, and the sawtooth it
// produced (the bar flipped every 31 ticks) ended at 2.3675, which that test printed and the notes called "the level's own geometry".
// GRAVITY OFF and the bar spun at 3 rad/s, so a turn of 0.05 a tick is all that is physical; the turn is read the SHORT way round the circle,
// so the honest crossing of pi (3.14 to -3.14) is 0.05 and the snap (4.7 radians, 1.58 the short way) is far over the bound.
void ASeesawNeverJumpsAcrossPi() {
    Run run;
    if (!Begin(kSeesawLevel, run)) return;
    if (run.level.hinge.hinges.empty() || !run.registry.all_of<RigidBodyComponent>(run.level.hinge.hinges.front().body)) {
        CHECK_MSG(false, "level30a pins a seesaw with a rigid body");
        return;
    }
    const Hinge::Hinged& hinged = run.level.hinge.hinges.front();

    Supersonic::PhysicsSettings settings;
    settings.gravity = glm::vec3(0.0f);
    run.registry.ctx().insert_or_assign<Supersonic::PhysicsSettings>(std::move(settings));
    run.registry.get<RigidBodyComponent>(hinged.body).angularVelocity = glm::vec3(0.0f, 0.0f, 3.0f);

    constexpr double kPi = 3.14159265358979323846;
    // The turn between two readings, signed, the SHORT way round the circle.
    const auto shortWay = [kPi](double now, double before) {
        double d = now - before;
        while (d > kPi) d -= 2.0 * kPi;
        while (d < -kPi) d += 2.0 * kPi;
        return d;
    };
    const double startAngle = Hinge::AngleOf(run.registry, hinged);
    double before = startAngle;
    double worst = 0.0;
    int worstTick = 0;
    double travelled = 0.0;   // the bar's turn from where it began, followed round the circle (an atan2 reading cannot say it)
    for (int tick = 1; tick <= 240; ++tick) {
        Tick(run);
        const double now = Hinge::AngleOf(run.registry, hinged);
        const double step = shortWay(now, before);
        if (std::fabs(step) > worst) {
            worst = std::fabs(step);
            worstTick = tick;
        }
        travelled += step;
        before = now;
    }
    std::printf("  %s: spun at 3 rad/s, the largest turn in one tick %.4f rad (tick %d), the reading ends at %.4f, travelled %.4f of %.4f to the stop\n",
                kSeesawLevel, worst, worstTick, before, travelled, hinged.maxRad - startAngle);
    CHECK_MSG(worst < 0.2, "no tick turns the bar by more than a few times its 0.05 a tick, got " + std::to_string(worst) + " on tick " +
                               std::to_string(worstTick));
    // And the stop HOLDS: a bar with no stop at all would turn 12 radians in these 240 ticks, one that was flipped back 270 degrees every 31
    // ticks would not sit on it, and the old reading could not tell either (it never exceeds pi).
    CHECK_MSG(travelled <= (hinged.maxRad - startAngle) + 0.1,
              "the bar never goes past its upper stop, travelled " + std::to_string(travelled) + " against " +
                  std::to_string(hinged.maxRad - startAngle));
    CHECK_MSG(std::fabs(shortWay(before, hinged.maxRad)) < 0.05,
              "and it ends held on it, the reading " + std::to_string(before) + " against the stop " + std::to_string(hinged.maxRad));
}

void WithoutItsAnchorThereIsNoHinge() {
    // The anchor is static, so a level started without its statics has none, and
    // a joint attached to nothing would be worse than no joint at all.
    Run run;
    if (!Begin(kPlatformLevel, run, false)) return;
    CHECK_MSG(run.level.hinge.hinges.empty(), "a level with its statics left out pins nothing");
}

void runTests() {
    TheReaderRefusesWhatWouldBeSilent();
    WhatTheLevelsCarry();
    TheAuthoredRangeIsMeasuredFromTheRestAngle();
    ASeesawStopsAtItsOwnEnds();
    ASeesawNeverJumpsAcrossPi();
    WithoutItsAnchorThereIsNoHinge();
    EveryHingedBodyStandsWhereItsJointHoldsIt();
    AJointWhoseEndsAlreadyMeetMovesNothing();
    ALevelWithoutItsAttachPointBIsRefused();
    ANodeThatIsNotTheEntityNamedIsRefused();
    ABodysOwnAttachPointStandsOnTheSamePoint();
    TheStoneOf2_29RidesThePlank();
}

} // namespace

MP_SUITE_MAIN("test_mp_hinge") {
    std::error_code ec;
    if (!std::filesystem::is_directory(kLevels, ec) ||
        !std::filesystem::is_regular_file(kData + "/entity_roles.json", ec)) {
        std::printf("test_mp_hinge SKIPPED - needs the converted levels at %s and Enhanced's data at %s\n",
                    kLevels.c_str(), kData.c_str());
        return 77;
    }
    runTests();
    return ::test::summary("test_mp_hinge", 80);
}
