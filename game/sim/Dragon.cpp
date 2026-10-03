#include "sim/Dragon.hpp"

#include "core/Components.hpp"
#include "core/DetMath.hpp"
#include "core/Json.hpp"
#include "sim/Shot.hpp"
#include "sim/Units.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <optional>
#include <sstream>
#include <utility>

namespace MagicPortals::Dragon {

namespace {

namespace Json = Supersonic::Json;

constexpr std::size_t kWaypoints = 6;
constexpr double kPi = 3.14159265358979323846;

glm::dvec2 PxOf(const entt::registry& registry, entt::entity entity) {
    return Units::ToPixels(registry.get<Supersonic::TransformComponent>(entity).position);
}

} // namespace

bool LoadRules(const std::string& path, Rules& out, std::string& error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = path + ": cannot read";
        return false;
    }
    std::ostringstream text;
    text << file.rdbuf();

    // Named, not a temporary: the parser keeps a reference to what it reads.
    const std::string content = text.str();
    Json::Parser parser(content);
    Json::Value root;
    if (!parser.Parse(root)) {
        error = path + ": " + parser.Error();
        return false;
    }

    const auto number = [&](const char* group, const char* key, double& into) -> bool {
        const Json::Value& block = root[group];
        if (!block.Has(key) || !block[key].IsNumber()) {
            error = path + ": " + std::string(group) + "." + key + " is missing or not a number";
            return false;
        }
        into = block[key].AsNumber();
        return true;
    };
    const auto string = [&](const char* group, const char* key, std::string& into) -> bool {
        const Json::Value& block = root[group];
        if (!block.Has(key) || !block[key].IsString()) {
            error = path + ": " + std::string(group) + "." + key + " is missing or not a string";
            return false;
        }
        into = block[key].AsString();
        return true;
    };
    const auto numbers = [&](const char* group, const char* key, std::vector<double>& into) -> bool {
        const Json::Value& block = root[group];
        if (!block.Has(key) || !block[key].IsArray()) {
            error = path + ": " + std::string(group) + "." + key + " is missing or not an array";
            return false;
        }
        into.clear();
        for (const Json::Value& one : block[key].AsArray()) {
            if (!one.IsNumber()) {
                error = path + ": " + std::string(group) + "." + key + " holds something that is not a number";
                return false;
            }
            into.push_back(one.AsNumber());
        }
        return true;
    };

    const auto pair = [&](const char* group, const char* key, glm::dvec2& into) -> bool {
        const Json::Value& block = root[group];
        if (!block.Has(key) || !block[key].IsArray() || block[key].AsArray().size() != 2 ||
            !block[key].AsArray()[0].IsNumber() || !block[key].AsArray()[1].IsNumber()) {
            error = path + ": " + std::string(group) + "." + key + " is missing or not a pair of numbers";
            return false;
        }
        into = glm::dvec2(block[key].AsArray()[0].AsNumber(), block[key].AsArray()[1].AsNumber());
        return true;
    };
    // A whole number at or above `least`: the counts and cells of the sheet, which
    // a fractional value would silently truncate.
    const auto whole = [&](const char* group, const char* key, int least, int& into) -> bool {
        double value = 0.0;
        if (!number(group, key, value)) return false;
        if (value != std::floor(value) || value < static_cast<double>(least)) {
            error = path + ": " + std::string(group) + "." + key + " is a whole number of at least " +
                    std::to_string(least);
            return false;
        }
        into = static_cast<int>(value);
        return true;
    };

    Rules read;
    if (!string("flight", "entity", read.entityName)) return false;
    if (!string("flight", "take_off", read.takeOffName)) return false;
    if (!string("knight", "spawn", read.knightSpawnName)) return false;
    if (!number("flight", "advance_px_s", read.advancePxS)) return false;
    if (!number("flight", "climb_px_s", read.climbPxS)) return false;
    if (!number("flight", "sink_px_s", read.sinkPxS)) return false;
    if (!number("flight", "hover_ceiling_px", read.hoverCeilingPx)) return false;
    if (!number("flight", "bob_amplitude_px", read.bobAmplitudePx)) return false;
    if (!number("flight", "bob_frequency", read.bobFrequency)) return false;
    if (!number("fire", "interval_ms", read.fireIntervalMs)) return false;
    if (!number("fire", "muzzle_px", read.muzzlePx)) return false;
    if (!number("fire", "muzzle_sub_deg", read.muzzleSubDeg)) return false;
    if (!number("fire", "muzzle_add_deg", read.muzzleAddDeg)) return false;

    // The lagged point (D7), and the angle it is read as.
    if (!pair("follower", "start_offset_px", read.followerStartPx)) return false;
    if (!pair("follower", "after_take_off_px", read.followerTakeOffPx)) return false;
    {
        int stride = 0;
        int rate = 0;
        if (!whole("follower", "stride_ms", 1, stride)) return false;
        if (!whole("follower", "rate_ms", 0, rate)) return false;
        read.followerStrideMs = static_cast<unsigned int>(stride);
        read.followerRateMs = static_cast<unsigned int>(rate);
    }
    if (!number("angle", "fix_deg", read.angleFixDeg)) return false;
    if (!number("angle", "sub_deg", read.angleSubDeg)) return false;
    if (!number("angle", "min_deg", read.angleMinDeg)) return false;
    if (!number("angle", "max_deg", read.angleMaxDeg)) return false;

    // The picture its callback drives.
    if (!whole("picture", "columns", 1, read.columns)) return false;
    if (!whole("picture", "rows", 1, read.rows)) return false;
    if (!whole("picture", "start_frame", 0, read.startFrame)) return false;
    if (!whole("picture", "frames", 1, read.animFrames)) return false;
    if (!number("picture", "open_from_ms", read.animOpenFromMs)) return false;
    if (!number("picture", "close_from_ms", read.animCloseFromMs)) return false;
    if (!number("picture", "span_ms", read.animSpanMs)) return false;

    if (!string("claw", "entity", read.clawName)) return false;
    if (!number("claw", "scan_ms", read.scanMs)) return false;
    if (!number("claw", "arm_px", read.armPx)) return false;
    if (!number("claw", "crush_px", read.crushPx)) return false;
    if (!number("claw", "lead_px", read.leadPx)) return false;
    if (!number("claw", "lift_px", read.liftPx)) return false;
    if (!number("claw", "bob_amplitude_px", read.clawBobAmplitudePx)) return false;
    if (!number("claw", "bob_frequency", read.clawBobFrequency)) return false;
    if (!numbers("claw", "hold_ms", read.holdMs)) return false;
    if (!numbers("claw", "angles_deg", read.anglesDeg)) return false;
    if (!number("camera", "view_width_px", read.viewWidthPx)) return false;

    {
        const Json::Value& path2 = root["claw"]["path_px"];
        if (!path2.IsArray()) {
            error = path + ": claw.path_px is missing or not an array";
            return false;
        }
        for (const Json::Value& one : path2.AsArray()) {
            if (!one.IsArray() || one.AsArray().size() != 2 || !one.AsArray()[0].IsNumber() ||
                !one.AsArray()[1].IsNumber()) {
                error = path + ": claw.path_px holds something that is not a pair of numbers";
                return false;
            }
            read.pathPx.emplace_back(one.AsArray()[0].AsNumber(), one.AsArray()[1].AsNumber());
        }
    }
    {
        const Json::Value& names = root["claw"]["crushes"];
        if (!names.IsArray()) {
            error = path + ": claw.crushes is missing or not an array";
            return false;
        }
        for (const Json::Value& one : names.AsArray()) {
            if (!one.IsString()) {
                error = path + ": claw.crushes holds something that is not a string";
                return false;
            }
            read.crushes.push_back(one.AsString());
        }
    }

    // SIX waypoints, and the count is asserted rather than tolerated. Five would
    // leave the claw resting mid-swing, because the callback parks it on
    // getNumWaypoints() - 2 and relies on the last two coinciding; dragon.json has
    // the decode. A table of the wrong length here is a data error, not a shrug.
    if (read.holdMs.size() != kWaypoints || read.pathPx.size() != kWaypoints ||
        read.anglesDeg.size() != kWaypoints) {
        error = path + ": claw.hold_ms, claw.path_px and claw.angles_deg each need " + std::to_string(kWaypoints) +
                " entries";
        return false;
    }
    if (read.crushes.empty()) {
        error = path + ": claw.crushes names what the claw may take and is empty";
        return false;
    }
    // A claw that never sweeps is a level whose floor never goes, and a dragon
    // that never advances never reaches take_off: both would look built and play
    // as scenery.
    if (read.scanMs <= 0.0) {
        error = path + ": claw.scan_ms is above zero";
        return false;
    }
    if (read.advancePxS <= 0.0) {
        error = path + ": flight.advance_px_s is above zero";
        return false;
    }
    if (read.fireIntervalMs <= 0.0) {
        error = path + ": fire.interval_ms is above zero";
        return false;
    }
    if (read.viewWidthPx <= 0.0) {
        error = path + ": camera.view_width_px is above zero";
        return false;
    }
    // A band the wrong way round would clamp every angle to its own floor, and the
    // head would never move; a span of 0 would divide by zero on every cell.
    if (read.angleMinDeg > read.angleMaxDeg) {
        error = path + ": angle.min_deg is at or below angle.max_deg";
        return false;
    }
    if (read.animSpanMs <= 0.0) {
        error = path + ": picture.span_ms is above zero";
        return false;
    }
    if (read.startFrame >= read.columns * read.rows || read.animFrames > read.columns * read.rows) {
        error = path + ": picture.start_frame and picture.frames are inside the sheet's cut";
        return false;
    }
    // The crush line must not sit outside the arm line, or the claw would destroy
    // a platform it never swung at.
    if (read.crushPx > read.armPx) {
        error = path + ": claw.crush_px is within claw.arm_px";
        return false;
    }

    out = std::move(read);
    return true;
}

double AngleFor(const Rules& rules, const glm::dvec2& aim) {
    // getAngle(v) is atan2f(v.x, v.y) - X FIRST - wrapped into [0, 2 PI).
    double radians = static_cast<double>(
        Supersonic::DetMath::atan2(static_cast<float>(aim.x), static_cast<float>(aim.y)));
    if (radians < 0.0) radians += 2.0 * kPi;
    const double degrees = radians / (2.0 * kPi) * 360.0;
    return std::clamp(degrees + rules.angleFixDeg - rules.angleSubDeg, rules.angleMinDeg, rules.angleMaxDeg);
}

Look LookAt(const Rules& rules, double elapsedMs, bool tookOff, bool completed) {
    Look look;
    // ins 531-534: the whole animation block is inside `if (!tookOff)`.
    if (tookOff) return look;
    const int last = rules.animFrames - 1;
    const auto cell = [&](double from, bool forward) {
        // fTOi truncates toward zero, which matters only for the `completed` arm,
        // where the elapsed time can sit before the window. The outer clamp
        // saturates well inside the bound held here, which is only against a cast
        // of an absurd elapsed time.
        const double steps = std::clamp(
            (elapsedMs - from) / rules.animSpanMs * static_cast<double>(rules.animFrames), -1000.0, 1000.0);
        const int step = static_cast<int>(steps);
        return std::clamp(forward ? step : last - step, 0, last);
    };
    // ins 536-574: the mouth opens over the open window, with its system playing.
    if (elapsedMs > rules.animOpenFromMs && elapsedMs < rules.animOpenFromMs + rules.animSpanMs) {
        look.plays = true;
        look.setsFrame = true;
        look.frame = cell(rules.animOpenFromMs, true);
        return look;
    }
    // ins 575-625: and closes over the close window, or at once when the level is
    // finished - the three-way `||`, whose third leg (tookOff) the guard above
    // makes unreachable.
    const bool closing = elapsedMs > rules.animCloseFromMs && elapsedMs < rules.animCloseFromMs + rules.animSpanMs;
    if (closing || completed) {
        look.kills = true;
        look.setsFrame = true;
        look.frame = cell(rules.animCloseFromMs, false);
    }
    return look;
}

bool Plays(const Rules& rules, const Tscn::Node& node) {
    const std::string name = Roles::EntityName(node);
    return name == rules.entityName || name == rules.knightSpawnName;
}

void Claw::Park(const Rules& rules) {
    // setCurrentWaypoint(getNumWaypoints() - 2). The last two waypoints coincide,
    // so this is the motionless idle pose rather than a point mid-swing.
    waypoint = static_cast<int>(rules.holdMs.size()) - 2;
    if (waypoint < 0) waypoint = 0;
    inWaypointMs = 0.0;
}

void Claw::Reset() {
    waypoint = 0;
    inWaypointMs = 0.0;
}

void Claw::Advance(const Rules& rules, double ms) {
    const int last = static_cast<int>(rules.holdMs.size()) - 1;
    if (last < 0) return;
    inWaypointMs += ms;
    while (waypoint < last && inWaypointMs >= rules.holdMs[static_cast<std::size_t>(waypoint)]) {
        inWaypointMs -= rules.holdMs[static_cast<std::size_t>(waypoint)];
        ++waypoint;
    }
    if (waypoint >= last) inWaypointMs = 0.0;
}

bool Claw::IsLastFrame(const Rules& rules) const {
    return waypoint + 1 >= static_cast<int>(rules.holdMs.size());
}

glm::dvec2 Claw::AtPx(const Rules& rules, const glm::dvec2& cameraLeftPx) const {
    const std::size_t at = static_cast<std::size_t>(waypoint);
    if (at >= rules.pathPx.size()) return cameraLeftPx;
    glm::dvec2 along = rules.pathPx[at];
    // LINEAR between the two, deliberately: dragon.json records smoothEnd and
    // smoothBeginning per waypoint and neither is applied, because the claw's
    // position has no gameplay consequence - both of its tests are against the
    // camera - and smoothBeginning is not decoded anywhere in this port.
    if (at + 1 < rules.pathPx.size() && rules.holdMs[at] > 0.0) {
        const double through = std::clamp(inWaypointMs / rules.holdMs[at], 0.0, 1.0);
        along += (rules.pathPx[at + 1] - rules.pathPx[at]) * through;
    }
    glm::dvec2 out = cameraLeftPx + along + glm::dvec2(rules.leadPx, heightPx - rules.liftPx);
    const double wave = static_cast<double>(
        Supersonic::DetMath::sin(static_cast<float>(cameraLeftPx.x * rules.clawBobFrequency)));
    out.y += wave * rules.clawBobAmplitudePx;
    return out;
}

double Claw::AngleDeg(const Rules& rules) const {
    const std::size_t at = static_cast<std::size_t>(waypoint);
    return at < rules.anglesDeg.size() ? rules.anglesDeg[at] : 0.0;
}

glm::dvec2 State::CameraLeftPx() const {
    // AutoCameraController::update: clamp(master.x, camMin.x, camMax.x), y = 0,
    // with camMin (0,0) and camMax the level's far corner less the screen.
    const double most = std::max(0.0, boundsX - viewWidthPx);
    return glm::dvec2(std::clamp(atPx.x, 0.0, most), 0.0);
}

State::Turn State::Tick(entt::registry& registry, entt::entity player, bool completed, double dt) {
    Turn turn;
    if (!present) return turn;

    const double ms = static_cast<double>(dt) * 1000.0;
    const double seconds = static_cast<double>(dt);
    elapsedMs += ms;

    // THE HEAD TURNS BEFORE THE BODY MOVES, which is the whole reason this arm is
    // written out in the callback's order (ins 149-261 against ins 488-529). The
    // aim reads GetPositionXY() while it still holds the position the LAST tick
    // drew, bob included, and only ins 529 writes this tick's. A tidier order -
    // move, then aim - would turn the head a frame early and change every angle by
    // the 0.75 u a frame of flight is worth.
    {
        // The destination is the live player while it has not taken off, and a
        // point off its own shoulder afterwards (ins 178-215).
        glm::dvec2 destination = atPx + rules.followerTakeOffPx;
        if (!tookOff && player != entt::null && registry.valid(player)) {
            destination = PxOf(registry, player);
        }
        follower.SetDestination(destination);
        // Not guarded by the pause: ins 217-218 call update() outright. The port's
        // tick does not run under a pause at all, so the two agree.
        follower.Update(Follower::FrameMs(seconds));
        // normalize(GetPositionXY() - follower.getPos()): the DRAGON LESS the
        // lagged point, which is the reading five independent lines of evidence
        // rest on (D7 section 3.5) and which the fire code confirms by negating it
        // before it shoots. gs2d's Normalize answers (0, 0) for a zero vector
        // (GameMath.h:755-761), and getAngle((0,0)) is atan2f(0,0) = 0, so the
        // angle then clamps to its floor rather than going NaN.
        aim = atPx - follower.At();
        const double length = std::sqrt(aim.x * aim.x + aim.y * aim.y);
        aim = length > 0.0 ? aim / length : glm::dvec2(0.0);
        angleDeg = AngleFor(rules, aim);
    }

    // THE ONE-FRAME LAG IS THE ORIGINAL'S. Both branches below test virtualPos as
    // it was at the top of the tick while the adds write the live one, and the
    // drawn bob reads the same snapshot. dragon.json cites the instructions.
    const glm::dvec2 previous = virtualPx;
    virtualPx.x += rules.advancePxS * seconds;
    if (hasTakeOff && previous.x > takeOffPx.x) {
        virtualPx.y += rules.climbPxS * seconds;
        tookOff = true;
    } else if (previous.y < rules.hoverCeilingPx) {
        virtualPx.y += rules.sinkPxS * seconds;
    }
    const double wave =
        static_cast<double>(Supersonic::DetMath::sin(static_cast<float>(previous.x * rules.bobFrequency)));
    atPx = virtualPx + glm::dvec2(0.0, wave * rules.bobAmplitudePx);

    // THE PICTURE, in the callback's place: after the move and before the shot, so
    // the cell a frame shows is the one the elapsed time that fired it had. Drawn
    // only - nothing below reads `frame` - but kept here because the same two
    // windows also play and kill the mouth's particle system, and one rule for
    // both is what stops the two drifting apart.
    {
        systemRestarted = false;
        const Look look = LookAt(rules, elapsedMs, tookOff, completed);
        if (look.setsFrame) frame = look.frame;
        if (look.plays) {
            // ETHSpriteEntity::PlayParticleSystem: Kill(false) and then Play,
            // which resets every particle to the entity - every tick of the
            // window, as the callback calls it.
            systemKilled = false;
            systemRestarted = true;
        }
        if (look.kills) systemKilled = true;
    }

    // THE CLAW. Its sweep is on a 250 ms clock of its own, not the frame's.
    if (hasClaw) {
        claw.Advance(rules, ms);
        claw.scanMs += ms;
        if (claw.scanMs > rules.scanMs) {
            const glm::dvec2 camera = CameraLeftPx();
            for (Crushable& one : crushables) {
                if (one.gone) continue;
                // The 94 px line re-arms the swing and remembers the height to
                // swing at; only when the swing has finished.
                if (one.atPx.x < camera.x + rules.armPx && claw.IsLastFrame(rules)) {
                    claw.heightPx = one.atPx.y;
                    claw.Reset();
                }
                // And the 64 px line destroys, every sweep, whatever the
                // animation is doing.
                if (one.atPx.x < camera.x + rules.crushPx) {
                    one.gone = true;
                    ++crushed;
                    turn.crushedNames.push_back(one.name);
                    if (registry.valid(one.entity)) turn.crushed.push_back(one.entity);
                    one.entity = entt::null;
                }
            }
            claw.scanMs = 0.0;
        }
    }

    // THE FIREBALLS. Before take-off only - ins 634-637, and dragon.json says why
    // that reading was checked twice - and only with the player in sight.
    if (!completed && !tookOff && elapsedMs > rules.fireIntervalMs && player != entt::null &&
        registry.valid(player)) {
        const glm::dvec2 playerPx = PxOf(registry, player);
        const glm::dvec2 along = playerPx - atPx;
        const double length = std::sqrt(along.x * along.x + along.y * along.y);
        if (length > 0.0) {
            // GetClosestContact(dragonPos -> charPos), and fire only if the
            // closest thing on that segment IS the character. Geometry between
            // the two stops the shot, which is the whole of the gate.
            //
            // ASKED THE OTHER WAY ROUND HERE, and it has to be. Shot::FirstBody
            // walks box, sphere and hull colliders; the player is a CAPSULE
            // (Player::Spawn), so it is not in that set and FirstBody can never
            // return it. Teaching it capsules would make every portal shot in the
            // game stop on the player, which is not what the original does.
            //
            // The segment ENDS at the player, so "the first thing hit is the
            // character" and "nothing solid lies before the endpoint" are the
            // same statement about the same world - and the second is the one
            // this port's primitive can answer. dragon.json records the swap.
            const std::optional<Shot::Hit> blocked = Shot::FirstBody(registry, atPx, playerPx, entt::null);
            if (!blocked.has_value() || blocked->along >= 1.0) {
                turn.fired = true;
                // IT SHOOTS AT THE LAGGED POINT, NOT AT THE PLAYER (the owner's
                // standing ruling R19; decode D7 section 3.4, ins 708-714). The
                // script hands addFireball `dragonToChar x (-1)`, and dragonToChar
                // is the dragon LESS the follower - so the shot flies from the
                // dragon toward the trailing point. That negation is itself the
                // strongest evidence for the subtraction order: a vector already
                // built as target - dragon would never be flipped to shoot at the
                // target. Against a still player the point converges on it; against
                // a moving one the shot goes about 1.2 s behind. The gate above is
                // untouched, and is still asked of the LIVE player.
                turn.aimPx = -aim;
                // The muzzle (ins 697-749): a = rad(-GetAngle() - 110 + 29), then
                // the dragon LESS scale(95) x (sin a, -cos a). At A = 360 that is
                // (93.8, 14.9) from the entity, which is dragon.ent's own mouth
                // StartPoint (96, 13); as A falls toward 340 the muzzle swings
                // down with the head. The position it is measured from is THIS
                // tick's, the one the move above just wrote.
                const double muzzleRad =
                    (-angleDeg - rules.muzzleSubDeg + rules.muzzleAddDeg) * kPi / 180.0;
                const glm::dvec2 out(
                    static_cast<double>(Supersonic::DetMath::sin(static_cast<float>(muzzleRad))),
                    -static_cast<double>(Supersonic::DetMath::cos(static_cast<float>(muzzleRad))));
                turn.firePx = atPx - out * rules.muzzlePx;
                ++fired;
                elapsedMs = 0.0;
            }
        }
    }
    return turn;
}

bool Find(const Tscn::Scene& scene, const Roles::Table& roles, const LevelBuilder::Built& built, const Rules& rules,
          State& out, std::string& error) {
    out = State{};
    out.rules = rules;
    out.viewWidthPx = rules.viewWidthPx;

    for (const Tscn::Node& node : scene.nodes) {
        if (node.parent != ".") continue;
        const std::string name = Roles::EntityName(node);
        const Tscn::Value* position = node.Find("position");
        const bool placed = position != nullptr && position->kind == Tscn::Value::Kind::Vector2;
        const glm::dvec2 atPx = placed ? glm::dvec2(position->numbers[0], position->numbers[1]) : glm::dvec2(0.0);

        if (name == rules.entityName) {
            if (!placed) {
                error = node.name + " is the dragon and has no position";
                return false;
            }
            out.present = true;
            out.name = node.name;
            out.virtualPx = atPx;
            out.atPx = atPx;
        } else if (name == rules.takeOffName && placed) {
            out.hasTakeOff = true;
            out.takeOffPx = atPx;
        } else if (name == rules.clawName && placed) {
            out.hasClaw = true;
        } else if (Roles::RoleOf(roles, node) == Roles::kLevelBounds && placed) {
            out.boundsX = atPx.x;
        }
    }
    if (!out.present) return true;

    // The level's far corner is what the camera clamps against, so a dragon level
    // without one would scroll past its own end.
    if (out.boundsX <= 0.0) {
        error = out.name + " is a dragon in a level with no level_bounds";
        return false;
    }
    // And with nothing to fly toward it would never take off, never stop firing,
    // and never end its part of the level.
    if (!out.hasTakeOff) {
        error = out.name + " is a dragon in a level with no " + rules.takeOffName;
        return false;
    }

    for (const Tscn::Node& node : scene.nodes) {
        if (node.parent != ".") continue;
        const std::string name = Roles::EntityName(node);
        if (std::find(rules.crushes.begin(), rules.crushes.end(), name) == rules.crushes.end()) continue;
        const Tscn::Value* position = node.Find("position");
        if (position == nullptr || position->kind != Tscn::Value::Kind::Vector2) continue;
        const auto found = built.entities.find(node.name);
        // Skipped rather than refused: Game::Start's withStatics false builds no
        // static bodies at all, and there is then nothing for the claw to take.
        if (found == built.entities.end()) continue;

        Crushable one;
        one.name = node.name;
        one.entity = found->second;
        one.atPx = glm::dvec2(position->numbers[0], position->numbers[1]);
        out.crushables.push_back(one);
    }
    // THE FOLLOWER IS BUILT WHERE THE DRAGON STANDS, plus the raw (2, 0) of ins
    // 44-45 (dragon.json `follower`). The callback builds it on its first tick,
    // before that tick sets a destination, so a point made here and a point made
    // on the first tick hold the same state: both sit at the start point with
    // switchMs 0, and neither can retarget for a whole 200 ms period. The first
    // frame therefore aims along (-2, 0) - straight behind the dragon - which
    // getAngle reads as 270 degrees and the clamp draws at the 370 ceiling.
    out.follower = Follower::Make(out.atPx + rules.followerStartPx, rules.followerStrideMs, rules.followerRateMs);
    {
        glm::dvec2 aim = out.atPx - out.follower.At();
        const double length = std::sqrt(aim.x * aim.x + aim.y * aim.y);
        out.aim = length > 0.0 ? aim / length : glm::dvec2(0.0);
        out.angleDeg = AngleFor(rules, out.aim);
    }
    // The cell level31a gives its placement (spriteFrame="2"), which stands until
    // the callback's first SetFrame - the close window at 500 ms.
    out.frame = rules.startFrame;
    out.claw.Park(rules);
    return true;
}

} // namespace MagicPortals::Dragon
