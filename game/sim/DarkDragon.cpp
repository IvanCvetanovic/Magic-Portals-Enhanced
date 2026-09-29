#include "sim/DarkDragon.hpp"

#include "core/Components.hpp"
#include "core/DetMath.hpp"
#include "core/Json.hpp"
#include "sim/Shot.hpp"
#include "sim/Units.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <sstream>
#include <utility>

namespace MagicPortals::DarkDragon {

namespace {

namespace Json = Supersonic::Json;

constexpr double kPi = 3.14159265358979323846;
// The engine's registered PIb (GameMath.h:40), single precision as the script has it.
constexpr float kPIb = 1.570796327f;

constexpr std::size_t kAppearWaypoints = 7;

constexpr const char* kAtHere = "here";
constexpr const char* kAtShout = "shout";
constexpr const char* kAtShoutMoved = "shout_moved";
constexpr const char* kAtFighting = "fighting";

glm::dvec2 PxOf(const entt::registry& registry, entt::entity entity) {
    return Units::ToPixels(registry.get<Supersonic::TransformComponent>(entity).position);
}

double Length(const glm::dvec2& v) { return std::sqrt(v.x * v.x + v.y * v.y); }

double DegToRad(double degrees) { return degrees * kPi / 180.0; }
double RadToDeg(double radians) { return radians * 180.0 / kPi; }

// FloatColor(uint) (bytes 86246..86521), written in darkdragon.json as the constant
// the script pushes, "0xAARRGGBB". Only rgb is kept: SetEmissiveColor takes
// getVector3().
bool Argb(const Json::Value& value, glm::dvec3& out) {
    if (!value.IsString()) return false;
    const std::string text = value.AsString();
    if (text.size() != 10 || text.compare(0, 2, "0x") != 0) return false;
    char* end = nullptr;
    const unsigned long argb = std::strtoul(text.c_str() + 2, &end, 16);
    if (end == nullptr || *end != '\0') return false;
    const auto channel = [argb](int shift) { return static_cast<double>((argb >> shift) & 0xFFu) / 255.0; };
    out = glm::dvec3(channel(16), channel(8), channel(0));
    return true;
}

// clamp(uint(value x steps), 0, 3): the jaw's cell from an eased bias (ins 593-606,
// 1568-1583, 1596-1609, 1823-1838). fTOu of a value that is never negative here.
int MouthCell(const Rules& rules, double eased, double steps) {
    const double scaled = eased * steps;
    const unsigned int whole = scaled > 0.0 ? static_cast<unsigned int>(scaled) : 0u;
    return static_cast<int>(std::min(whole, static_cast<unsigned int>(rules.lastFrame)));
}

// PlayParticleSystem(0): Kill(false), then Play.
void Restart(Look& look) {
    look.systemKilled = false;
    look.systemRestarted = true;
}

// linearMotion's offset (utilEntityEffect bytes 327527..328407 ins 112-208):
// cos(angle) x stride x sign(speed), along y - the call is (this, vertical = true,
// angle = 0, PIb) - times getScale(), which is 1 in the port's units. Through
// DetMath: the point it moves is where a blast is measured to.
glm::dvec2 HoverOffsetPx(const Motion::Linear& hover) {
    if (!hover.started) return glm::dvec2(0.0);
    const double sign = hover.speed > 0.0 ? 1.0 : (hover.speed < 0.0 ? -1.0 : 0.0);
    return glm::dvec2(
        0.0, static_cast<double>(Supersonic::DetMath::cos(static_cast<float>(hover.angle))) * hover.stride * sign);
}

// FIGHTING's shot at the player, once the fire clock is past the interval (ins
// 849-1065): whether it leaves, and from where. Its own function so that a shot
// that does not leave still reaches the wound the callback reads after it.
void ShootAtPlayer(State& dragon, entt::registry& registry, entt::entity player, State::Turn& turn) {
    if (player == entt::null || !registry.valid(player)) return;
    const glm::dvec2 playerPx = PxOf(registry, player);
    if (Length(playerPx - dragon.atPx) <= 0.0) return;
    // The same sight gate as level31a's dragon, asked the same way round and for
    // the same reason: Shot::FirstBody cannot return a capsule, so the question
    // is "nothing solid before the endpoint". dragon.json decodes it.
    const std::optional<Shot::Hit> blocked = Shot::FirstBody(registry, dragon.atPx, playerPx, entt::null);
    if (blocked.has_value() && blocked->along < 1.0) return;

    // FROM THE MOUTH, AT THE LIVE PLAYER (ins 913-978): the muzzle is turned with
    // the head, and the shot aims from there, not from the dragon.
    const glm::dvec2 fromPx = MuzzlePx(dragon.rules, dragon.atPx, dragon.look.angleDeg);
    const glm::dvec2 along = playerPx - fromPx;
    const double length = Length(along);
    if (length <= 0.0) return;
    turn.fired = true;
    turn.firePx = fromPx;
    turn.aimPx = along / length;
    ++dragon.fired;
    dragon.elapsedMs = 0.0;
    // ins 997-1064, inside the branch the shot left by: after addFireball, the
    // quake and elapsedTime = 0, the barrel bomb put back if none is left.
    turn.restock = true;
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
    const auto whole = [&](const char* group, const char* key, int least, int& into) -> bool {
        double read = 0.0;
        if (!number(group, key, read)) return false;
        if (read != std::floor(read) || read < static_cast<double>(least)) {
            error = path + ": " + std::string(group) + "." + key + " is a whole number of at least " +
                    std::to_string(least);
            return false;
        }
        into = static_cast<int>(read);
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
    const auto pair = [&](const char* group, const char* key, glm::dvec2& into) -> bool {
        const Json::Value& block = root[group];
        if (!block.Has(key) || !block[key].IsArray() || block[key].AsArray().size() != 2 ||
            !block[key].AsArray()[0].IsNumber() || !block[key].AsArray()[1].IsNumber()) {
            error = path + ": " + std::string(group) + "." + key + " is a pair of numbers";
            return false;
        }
        into = glm::dvec2(block[key].AsArray()[0].AsNumber(), block[key].AsArray()[1].AsNumber());
        return true;
    };
    const auto triple = [&](const char* group, const char* key, glm::dvec3& into) -> bool {
        const Json::Value& block = root[group];
        if (!block.Has(key) || !block[key].IsArray() || block[key].AsArray().size() != 3) {
            error = path + ": " + std::string(group) + "." + key + " is three numbers";
            return false;
        }
        for (int i = 0; i < 3; ++i) {
            const Json::Value& one = block[key].AsArray()[static_cast<std::size_t>(i)];
            if (!one.IsNumber()) {
                error = path + ": " + std::string(group) + "." + key + " is three numbers";
                return false;
            }
            into[i] = one.AsNumber();
        }
        return true;
    };
    const auto colour = [&](const char* group, const char* key, glm::dvec3& into) -> bool {
        const Json::Value& block = root[group];
        if (!block.Has(key) || !Argb(block[key], into)) {
            error = path + ": " + std::string(group) + "." + key + " is a colour written \"0xAARRGGBB\"";
            return false;
        }
        return true;
    };
    const auto filter = [&](const char* group, const char* key, Ease::Filter& into) -> bool {
        std::string name;
        if (!string(group, key, name)) return false;
        if (!Ease::ParseFilter(name, into)) {
            error = path + ": " + std::string(group) + "." + key + " names no filter the port knows: " + name;
            return false;
        }
        return true;
    };
    const auto milliseconds = [&](const char* group, const char* key, unsigned int& into) -> bool {
        int read = 0;
        if (!whole(group, key, 0, read)) return false;
        into = static_cast<unsigned int>(read);
        return true;
    };

    Rules read;
    if (!string("summon", "spawn", read.spawnName)) return false;
    if (!string("summon", "entity", read.entityName)) return false;
    if (!string("summon", "wall", read.wallName)) return false;
    if (!number("summon", "arm_ms", read.armMs)) return false;
    if (!number("summon", "summon_ms", read.summonMs)) return false;

    if (!number("body", "radius_px", read.radiusPx)) return false;
    if (!whole("body", "max_hp", 0, read.maxHp)) return false;

    if (!number("fight", "fire_interval_ms", read.fireIntervalMs)) return false;
    if (!number("fight", "fire_interval_step_ms", read.fireIntervalStepMs)) return false;
    if (!number("fight", "muzzle_px", read.muzzlePx)) return false;
    if (!number("fight", "muzzle_sub_deg", read.muzzleSubDeg)) return false;
    if (!number("fight", "muzzle_add_deg", read.muzzleAddDeg)) return false;
    if (!number("fight", "fireball_particle_scale", read.fireballParticleScale)) return false;
    if (!string("fight", "shout", read.shoutName)) return false;
    if (!string("fight", "fighting", read.fightingName)) return false;
    {
        int divisor = 0;
        int quarters = 0;
        if (!whole("fight", "kill_below_divisor", 1, divisor)) return false;
        if (!whole("fight", "play_above_quarters", 0, quarters)) return false;
        read.killBelowDivisor = static_cast<unsigned int>(divisor);
        read.playAboveQuarters = static_cast<unsigned int>(quarters);
    }
    if (!milliseconds("fight", "follower_stride_ms", read.followerStrideMs)) return false;
    if (!milliseconds("fight", "follower_rate_ms", read.followerRateMs)) return false;
    if (!number("fight", "aim_offset_pib", read.aimOffsetPib)) return false;
    if (!number("fight", "seed_add_deg", read.seedAddDeg)) return false;
    if (!number("fight", "neck_height_px", read.neckHeightPx)) return false;
    if (!number("fight", "neck_top_deg", read.neckTopDeg)) return false;
    if (!number("fight", "neck_span_deg", read.neckSpanDeg)) return false;
    if (!number("fight", "neck_ramp_ms", read.neckRampMs)) return false;

    if (!number("hover", "speed", read.hoverSpeed)) return false;
    if (!number("hover", "stride_px", read.hoverStridePx)) return false;
    if (!number("hover", "start_angle", read.hoverStartAngle)) return false;

    if (!whole("look", "start_frame", 0, read.startFrame)) return false;
    if (!whole("look", "last_frame", 0, read.lastFrame)) return false;
    if (!triple("look", "emissive", read.emissive)) return false;
    if (!number("look", "z_units", read.zUnits)) return false;

    if (!pair("appearing", "shout_move_px", read.shoutMovePx)) return false;
    if (!whole("appearing", "mouth_leg", 0, read.mouthLeg)) return false;
    if (!number("appearing", "mouth_steps", read.appearMouthSteps)) return false;
    {
        const Json::Value& waypoints = root["appearing"]["waypoints"];
        if (!waypoints.IsArray()) {
            error = path + ": appearing.waypoints is a list of the init block's addWaypoint calls";
            return false;
        }
        for (const Json::Value& one : waypoints.AsArray()) {
            if (!one.IsObject() || !one["at"].IsString() || !one["ms"].IsNumber() || !one["filter"].IsString() ||
                !one["angle_deg"].IsNumber() || one["ms"].AsNumber() < 0.0) {
                error = path + ": appearing.waypoints holds an entry without its at, ms, filter and angle_deg";
                return false;
            }
            AppearWaypoint made;
            const std::string at = one["at"].AsString();
            if (at == kAtHere) {
                made.at = WaypointAt::Here;
            } else if (at == kAtShout) {
                made.at = WaypointAt::Shout;
            } else if (at == kAtShoutMoved) {
                made.at = WaypointAt::ShoutMoved;
            } else if (at == kAtFighting) {
                made.at = WaypointAt::Fighting;
            } else {
                error = path + ": appearing.waypoints stands one at a place the port does not know: " + at;
                return false;
            }
            if (!Ease::ParseFilter(one["filter"].AsString(), made.filter)) {
                error = path + ": appearing.waypoints names a filter the port does not know: " +
                        one["filter"].AsString();
                return false;
            }
            made.ms = one["ms"].AsNumber();
            made.angleDeg = one["angle_deg"].AsNumber();
            read.appear.push_back(made);
        }
    }

    if (!number("damage", "first_ms", read.damageFirstMs)) return false;
    if (!number("damage", "flash_ms", read.damageFlashMs)) return false;
    if (!whole("damage", "flashes", 0, read.damageFlashes)) return false;
    if (!pair("damage", "shout_move_px", read.damageMovePx)) return false;
    if (!number("damage", "angle_add_deg", read.damageAngleAddDeg)) return false;
    if (!filter("damage", "filter", read.damageFilter)) return false;
    if (!colour("damage", "red", read.damageRed)) return false;
    if (!number("damage", "bright_times", read.damageBrightTimes)) return false;
    if (!number("damage", "mouth_steps", read.damageMouthSteps)) return false;
    if (!string("damage", "torch_entity", read.torchEntity)) return false;

    if (!number("dying", "hover_ms", read.dyingHoverMs)) return false;
    if (!number("dying", "drift_ms", read.dyingDriftMs)) return false;
    if (!pair("dying", "fall_move_px", read.fallMovePx)) return false;
    if (!number("dying", "fall_px", read.fallPx)) return false;
    if (!number("dying", "angle_add_deg", read.dyingAngleAddDeg)) return false;
    if (!filter("dying", "filter", read.dyingFilter)) return false;
    if (!colour("dying", "emissive", read.dyingEmissive)) return false;
    if (!number("dying", "mouth_steps", read.dyingMouthSteps)) return false;
    if (!string("dying", "key_pos", read.keyPosName)) return false;
    if (!string("dying", "platform_pos", read.platformPosName)) return false;
    if (!string("dying", "key_colour", read.keyColour)) return false;
    if (!string("dying", "key_entity", read.keyEntity)) return false;
    if (!string("dying", "platform_entity", read.platformEntity)) return false;

    if (!string("restock", "search", read.restock.search)) return false;
    if (!string("restock", "add", read.restock.add)) return false;
    if (!string("restock", "spawn", read.restock.spawn)) return false;
    if (!string("restock", "effect", read.restock.effect)) return false;
    if (!string("restock", "alias", read.restock.alias)) return false;
    if (!string("restock", "template", read.restock.templateEntity)) return false;
    // A search for nothing finds nothing and restocks every shot; a spawn or a
    // template of nothing is a restock that can never be made.
    if (read.restock.search.empty() || read.restock.spawn.empty() || read.restock.templateEntity.empty()) {
        error = path + ": restock.search, restock.spawn and restock.template name an entity";
        return false;
    }

    // SEVEN waypoints, asserted rather than tolerated: the appearing path is what
    // carries the boss from off the left edge to where it fights, and a table of
    // the wrong length would leave it somewhere no decode says it should be - and
    // would move the frame FIGHTING begins on, which is its last.
    if (read.appear.size() != kAppearWaypoints) {
        error = path + ": appearing.waypoints needs " + std::to_string(kAppearWaypoints) + " entries";
        return false;
    }
    // The jaw opens on one of them, and not the last: the last is FIGHTING's.
    if (read.mouthLeg >= static_cast<int>(kAppearWaypoints) - 1) {
        error = path + ": appearing.mouth_leg is a waypoint before the last";
        return false;
    }
    // Each of these would be a boss that looks built. No hp is one that dies to
    // the first blast; no interval is one that fires every tick; no radius is one
    // a blast can never reach, which is a fight that cannot be won.
    if (read.maxHp <= 0) {
        error = path + ": body.max_hp is above zero";
        return false;
    }
    if (read.radiusPx <= 0.0) {
        error = path + ": body.radius_px is above zero";
        return false;
    }
    if (read.fireIntervalMs <= 0.0) {
        error = path + ": fight.fire_interval_ms is above zero";
        return false;
    }
    // And a step that outruns the interval would make the last wound give it a
    // zero or negative stride, which is every tick.
    if (read.fireIntervalStepMs < 0.0 ||
        read.fireIntervalStepMs * (read.maxHp - 1) >= read.fireIntervalMs) {
        error = path + ": fight.fire_interval_step_ms leaves an interval above zero at 1 hp";
        return false;
    }
    // A scale of 0 or less would draw no fire at all, which is the failure this
    // reader exists to refuse: the number is a multiple of the .ent's own size.
    if (read.fireballParticleScale <= 0.0) {
        error = path + ": fight.fireball_particle_scale is above zero";
        return false;
    }
    if (read.armMs < 0.0 || read.summonMs < 0.0) {
        error = path + ": summon.arm_ms and summon.summon_ms are not negative";
        return false;
    }
    // A Follower with no stride never moves, and the neck's two divisors cannot be 0.
    if (read.followerStrideMs == 0 || !(read.neckSpanDeg > 0.0) || !(read.neckRampMs > 0.0)) {
        error = path + ": fight.follower_stride_ms, neck_span_deg and neck_ramp_ms are above zero";
        return false;
    }
    // A DAMAGE with no flash is no wound the player can see, and the arm reads its
    // path's second-last waypoint - which needs one.
    if (read.damageFlashes < 1) {
        error = path + ": damage.flashes is at least one";
        return false;
    }
    if (read.startFrame > read.lastFrame) {
        error = path + ": look.start_frame is within look.last_frame";
        return false;
    }

    out = std::move(read);
    return true;
}

bool Plays(const Rules& rules, const Tscn::Node& node) {
    return Roles::EntityName(node) == rules.spawnName;
}

void Path::Update(double frameMs) {
    if (points.empty()) return;
    // FrameTimer::set (bytes 23254..23650): #3-4 the stride, #6-11 the time added,
    // #13-43 a changed first or last starts again at first with no time, else
    // #45-76 ONE frame once the time reaches the stride, the stride taken off, and
    // past last - this manager does not repeat - held on last with no time.
    const int wantLast = static_cast<int>(points.size()) - 1;
    strideMs = points[static_cast<std::size_t>(frame)].ms;
    timeMs += frameMs;
    if (first != 0 || last != wantLast) {
        first = 0;
        last = wantLast;
        frame = 0;
        timeMs = 0.0;
    } else if (timeMs >= strideMs) {
        ++frame;
        timeMs -= strideMs;
        if (frame > last) {
            frame = last;
            timeMs = 0.0;
        }
    }
    // WaypointManager::update #39-59: setCurrentStride(waypoints[get()].time).
    strideMs = points[static_cast<std::size_t>(frame)].ms;
}

double Path::Bias() const { return std::min(strideMs, timeMs) / std::max(strideMs, 1.0); }

Path::Point Path::Current() const {
    if (points.empty()) return Point{};
    // #3-28: the next waypoint, or this one again on the last.
    const Point& current = points[static_cast<std::size_t>(frame)];
    const Point& next = points[static_cast<std::size_t>(IsLastFrame() ? frame : frame + 1)];
    // #54-123: the CURRENT waypoint's filter on the raw bias, and interpolate
    // (a + (b - a) x f) for the angle, the colour and the point alike.
    const double f = Ease::Apply(current.filter, Bias());
    Point blended = current;
    blended.atPx = current.atPx + (next.atPx - current.atPx) * f;
    blended.colour = current.colour + (next.colour - current.colour) * f;
    blended.angleDeg = current.angleDeg + (next.angleDeg - current.angleDeg) * f;
    return blended;
}

double State::FireIntervalMs() const {
    // Tested against the LIVE hp every tick, not stamped when it is wounded.
    double interval = rules.fireIntervalMs;
    for (int at = rules.maxHp - 1; at >= 1; --at) {
        if (hp <= at) interval -= rules.fireIntervalStepMs;
    }
    return interval;
}

double AimRadians(const Rules& rules, const glm::dvec2& dragonPx, const glm::dvec2& characterPx) {
    // normalize(dragon - character): the DRAGON LESS the character (ins 5-11 of
    // computeDarkDragonAngle, D7 section 4.5). gs2d's Normalize answers (0, 0) for
    // a zero vector (GameMath.h:755-761), and getAngle((0,0)) is then 0.
    glm::dvec2 v = dragonPx - characterPx;
    const double length = Length(v);
    v = length > 0.0 ? v / length : glm::dvec2(0.0);
    double radians = static_cast<double>(
        Supersonic::DetMath::atan2(static_cast<float>(v.x), static_cast<float>(v.y)));
    if (radians < 0.0) radians += 2.0 * kPi;
    return radians + static_cast<double>(kPIb * static_cast<float>(rules.aimOffsetPib));
}

glm::dvec2 MuzzlePx(const Rules& rules, const glm::dvec2& atPx, double angleDeg) {
    const double a = DegToRad(-angleDeg - rules.muzzleSubDeg + rules.muzzleAddDeg);
    const glm::dvec2 out(static_cast<double>(Supersonic::DetMath::sin(static_cast<float>(a))),
                         -static_cast<double>(Supersonic::DetMath::cos(static_cast<float>(a))));
    return atPx - out * rules.muzzlePx;
}

double NeckLiftPx(const Rules& rules, double angleDeg, double neckMs) {
    const double ramp = std::min(neckMs / rules.neckRampMs, 1.0);
    const double bias = std::max(std::min((rules.neckTopDeg - angleDeg) / rules.neckSpanDeg, 1.0), 0.0);
    return (Ease::SmoothBothSides(bias) * 2.0 - 1.0) * rules.neckHeightPx * ramp;
}

State::Turn State::Tick(entt::registry& registry, entt::entity player, bool torchLit, const glm::dvec2& torchPx,
                        bool hasTorch, bool completed, const Motion::Rules& motion, float dt) {
    Turn turn;
    if (!present || gone) return turn;

    const double ms = static_cast<double>(dt) * 1000.0;
    // The Follower's clock is the script's whole milliseconds (Follower.hpp).
    const unsigned int scriptMs = Follower::FrameMs(static_cast<double>(dt));

    // THE SUMMON, and the marker's own two arms. `destroy` deletes nothing: it
    // only records that the light has been seen.
    if (!alive) {
        if (!armed) {
            if (!torchLit) return turn;
            armingMs += ms;
            if (armingMs <= rules.armMs) return turn;
            armed = true;
            summoningMs = 0.0;
            return turn;
        }
        summoningMs += ms;
        if (summoningMs <= rules.summonMs || completed) return turn;

        alive = true;
        summoned = true;
        initialised = false;
        phase = Phase::Appearing;
        elapsedMs = 0.0;
        hp = rules.maxHp;
        atPx = spawnPx;
        originPx = spawnPx;
        // AddScaledEntity's dark_dragon.ent before its callback has run: its
        // startFrame, no angle, its own emissive and its particle system playing.
        look = Look{};
        look.frame = rules.startFrame;
        look.emissive = rules.emissive;
        hover = Motion::Linear{};
        hover.speed = rules.hoverSpeed;
        hover.stride = rules.hoverStridePx;
        hover.vertical = true;
        hover.axisDeg = 0.0;
        hover.startAngle = rules.hoverStartAngle;
        // breakDarkDragonWall, on the tick it arrives.
        turn.brokeWall = true;
        return turn;
    }

    look.systemRestarted = false;

    // THE INIT BLOCK (ins 15-375), on its first update. It builds the appearing
    // path from where it stands, which is where the summon added it.
    if (!initialised) {
        initialised = true;
        elapsedMs = 0.0;                    // ins 28-39
        neckMs = 0.0;                       // ins 41-52
        look.systemKilled = true;           // ins 54-56 KillParticleSystem(0)
        phase = Phase::Appearing;           // ins 107-118
        hp = rules.maxHp;                   // ins 120-131
        look.frame = 0;                     // ins 148-150 SetFrame(0)
        path = Path{};
        const glm::dvec2 movedPx = shoutPx + rules.shoutMovePx;
        for (const AppearWaypoint& rule : rules.appear) {
            Path::Point point;
            switch (rule.at) {
            case WaypointAt::Here:
                point.atPx = atPx;
                break;
            case WaypointAt::Shout:
                point.atPx = shoutPx;
                break;
            case WaypointAt::ShoutMoved:
                point.atPx = movedPx;
                break;
            case WaypointAt::Fighting:
                point.atPx = fightingPx;
                break;
            }
            point.ms = rule.ms;
            point.filter = rule.filter;
            point.angleDeg = rule.angleDeg;
            path.points.push_back(point);
        }
        // Follower(V2_ZERO, 800, 150) (ins 343-356). It is not updated until FIGHTING.
        follower = Follower::Make(glm::dvec2(0.0), rules.followerStrideMs, rules.followerRateMs);
    }

    // linearMotion (ins 390-399), EVERY update and before the state is read: its
    // first call writes originalPos from where the entity stands, and every call
    // stands it at the originalPos the last update's arm wrote, plus the swing.
    if (!hover.started) originPx = atPx;
    Motion::Advance(motion, hover, ms);
    atPx = originPx + HoverOffsetPx(hover);

    elapsedMs += ms; // ins 413-425

    // ins 505-527: the path's update - under the original's !isPaused, which the
    // port's tick, not running under a pause, keeps - and its current point, whose
    // position every arm writes as the next originalPos (ins 529-537).
    path.Update(ms);
    const Path::Point point = path.Current();
    originPx = point.atPx;
    const int leg = path.frame;
    const int legs = static_cast<int>(path.points.size());

    switch (phase) {
    case Phase::Appearing: {
        look.angleDeg = point.angleDeg; // ins 539-546
        // THE JAW OPENS ON ONE LEG (ins 570-647), and the fire clock is held at 0
        // through it (ins 580-591). startEarthquake(10) past bias 0.2 is not built.
        if (leg == rules.mouthLeg) {
            elapsedMs = 0.0;
            const int cell = MouthCell(rules, Ease::SmoothBeginning(path.Bias()), rules.appearMouthSteps);
            if (cell == rules.lastFrame) Restart(look); // ins 608-613, every update on the widest cell
            look.frame = cell;
        }
        // THE LAST WAYPOINT IS FIGHTING (ins 649-682): the Follower forced to the
        // angle it stands at plus a turn, so -12 is written 348 and the head does
        // not swing through a full circle toward a target near 360.
        if (path.IsLastFrame()) {
            phase = Phase::Fighting;
            follower.Force(glm::dvec2(DegToRad(look.angleDeg + rules.seedAddDeg), 0.0));
            follower.Update(scriptMs);
        }
        break;
    }

    case Phase::Fighting: {
        // ins 688-732: the Follower first, THEN the new destination, so a retarget
        // takes the one the last update set; then the angle it holds now.
        follower.Update(scriptMs);
        if (player != entt::null && registry.valid(player)) {
            follower.SetDestination(glm::dvec2(AimRadians(rules, atPx, PxOf(registry, player)), 0.0));
        }
        look.angleDeg = RadToDeg(follower.At().x);
        // THE NECK (ins 734-818), on the point the next update stands at.
        neckMs += ms;
        originPx = point.atPx + glm::dvec2(0.0, NeckLiftPx(rules, look.angleDeg, neckMs));

        // Everything after the neck only while the level is not finished (ins
        // 821-827): the mouth, the shot and the wound alike.
        if (completed) break;

        // THE MOUTH (ins 829-847): killed below half the interval, restarted every
        // update above three quarters of it - uint division, as the script's.
        const unsigned int interval = static_cast<unsigned int>(FireIntervalMs());
        if (elapsedMs < static_cast<double>(interval / rules.killBelowDivisor)) {
            look.systemKilled = true;
        } else if (elapsedMs > static_cast<double>(interval / 4u * rules.playAboveQuarters)) {
            Restart(look);
        }
        // THE SHOT (ins 849-1065).
        if (elapsedMs > FireIntervalMs()) ShootAtPlayer(*this, registry, player, turn);

        // THE WOUND, READ AFTER THE SHOT (ins 1067), as the callback orders it: a
        // blast that reached it on the update it fires lets that shot leave first,
        // and a wound taken below half the interval enters DAMAGE with its mouth
        // killed. isBurned then healBurn.
        if (burned) {
            burned = false;
            ++hits;
            --hp;
            phase = Phase::Damage;
            elapsedMs = 0.0;
            neckMs = 0.0; // ins 1491-1502
            // Every portal the player has placed, taken away (ins 1508-1511).
            turn.killedPortals = true;
            // A fresh WaypointManager (ins 1125-1489): from where it stands and the
            // angle it holds, to shout + move at that angle + 27 over eight flash
            // legs (R20), and back to `fighting`.
            const double hitDeg = look.angleDeg;
            const double destDeg = hitDeg + rules.damageAngleAddDeg;
            const glm::dvec3 bright = rules.emissive * rules.damageBrightTimes;
            const glm::dvec2 retreatPx = shoutPx + rules.damageMovePx;
            path = Path{};
            path.points.push_back({atPx, rules.damageFirstMs, rules.emissive, rules.damageFilter, hitDeg});
            for (int flash = 0; flash < rules.damageFlashes; ++flash) {
                // Red first, then the .ent's emissive x 3, alternating (ins 1275-1465).
                const glm::dvec3 flashColour = flash % 2 == 0 ? rules.damageRed : bright;
                path.points.push_back({retreatPx, rules.damageFlashMs, flashColour, rules.damageFilter, destDeg});
            }
            path.points.push_back({fightingPx, 0.0, rules.emissive, rules.damageFilter, destDeg});
        }
        break;
    }

    case Phase::Damage: {
        look.angleDeg = point.angleDeg; // ins 1517-1524
        look.emissive = point.colour;   // ins 1548-1556: the flash IS the waypoints' colours
        // The jaw shuts over the first leg and opens again over the last flash
        // (ins 1565-1613).
        if (leg == 0) {
            look.frame = MouthCell(rules, Ease::SmoothBeginning(1.0 - path.Bias()), rules.damageMouthSteps);
        }
        if (leg == legs - 2) {
            look.frame = MouthCell(rules, Ease::SmoothBeginning(path.Bias()), rules.damageMouthSteps);
        }
        if (leg == legs - 1) {
            // AT THE LAST WAYPOINT, A SHOT AT THE TORCH (ins 1615-1720): from its
            // mouth, along the line from its own point to the light's. The
            // original fires it ONLY when SeekEntity('light_off.ent') finds one
            // (ins 1675-1694): an unlit torch its fire signal re-added, which the
            // fireball then relights (hasProjectileAround counts fireball.ent).
            // A lit torch has deleted itself and level31c's own is named bare
            // `light_off`, so the original never fires at a lit one. The port
            // fires whenever the level has a torch, and against a lit one Torch
            // puts it out and brings its light wall back: a gameplay divergence
            // filed for the owner (planning step 99), not built here.
            if (hasTorch) {
                const glm::dvec2 along = torchPx - atPx;
                const double length = Length(along);
                if (length > 0.0) {
                    turn.firedAtTorch = true;
                    turn.fired = true;
                    turn.aimPx = along / length;
                    turn.firePx = MuzzlePx(rules, atPx, look.angleDeg);
                    ++fired;
                }
            }
            elapsedMs = 0.0;
            phase = Phase::Fighting;
            // Forced to where DAMAGE left it, with NO turn added (ins 1752-1768).
            follower.Force(glm::dvec2(DegToRad(look.angleDeg), 0.0));
            follower.Update(scriptMs);
        }
        break;
    }

    case Phase::Dying: {
        look.emissive = point.colour;   // ins 1797-1805: white
        look.angleDeg = point.angleDeg; // ins 1807-1811
        if (leg == 0) {
            look.frame = MouthCell(rules, Ease::SmoothBeginning(1.0 - path.Bias()), rules.dyingMouthSteps);
        }
        if (leg == legs - 1) {
            gone = true;
            alive = false;
            // The key is the level's only one, and the lock its fight stands for.
            if (hasKeyPos) {
                turn.droppedKey = true;
                turn.keyPx = keyPosPx;
            }
            // And the platform, added at the same waypoint and from the same
            // .ent. Without it level31c's key sits in a hole with no floor: the
            // light wall that floored x 1..127 is `breakable` and lighting the
            // torch - which is what summons this boss - takes it away.
            if (hasPlatformPos) {
                turn.droppedPlatform = true;
                turn.platformPx = platformPosPx;
            }
        }
        break;
    }
    }

    // DEATH, WHATEVER THE ARM (ins 1947-2171): hp at 0 in any state but DYING, and a
    // fresh WaypointManager from where it stands and the angle it holds, over to
    // shout + fallMove at that angle + 30, then down, all of it white.
    if (!gone && hp <= 0 && phase != Phase::Dying) {
        phase = Phase::Dying;
        elapsedMs = 0.0;
        const double hitDeg = look.angleDeg;
        const double destDeg = hitDeg + rules.dyingAngleAddDeg;
        const glm::dvec2 restPx = shoutPx + rules.fallMovePx;
        path = Path{};
        path.points.push_back({atPx, rules.dyingHoverMs, rules.dyingEmissive, rules.dyingFilter, hitDeg});
        path.points.push_back({restPx, rules.dyingDriftMs, rules.dyingEmissive, rules.dyingFilter, destDeg});
        path.points.push_back(
            {restPx + glm::dvec2(0.0, rules.fallPx), 0.0, rules.dyingEmissive, rules.dyingFilter, destDeg});
    }
    return turn;
}

bool Find(const Tscn::Scene& scene, const Roles::Table& roles, const Rules& rules, State& out, std::string& error) {
    out = State{};
    out.rules = rules;

    for (const Tscn::Node& node : scene.nodes) {
        if (node.parent != ".") continue;
        const std::string name = Roles::EntityName(node);
        const Tscn::Value* position = node.Find("position");
        const bool placed = position != nullptr && position->kind == Tscn::Value::Kind::Vector2;
        const glm::dvec2 atPx = placed ? glm::dvec2(position->numbers[0], position->numbers[1]) : glm::dvec2(0.0);

        // By entity name AND by role: `dark_dragon_spawn` is what the level calls
        // it, and `boss_spawn` is what the role table makes of it. Asking both
        // means a level that renamed one without the other is not half found.
        if (name == rules.spawnName && Roles::RoleOf(roles, node) == Roles::kBossSpawn) {
            if (!placed) {
                error = node.name + " is a dark dragon spawn with no position";
                return false;
            }
            out.present = true;
            out.name = node.name;
            out.spawnPx = atPx;
            out.atPx = atPx;
            out.originPx = atPx;
        } else if (name == rules.shoutName && placed) {
            out.shoutPx = atPx;
        } else if (name == rules.fightingName && placed) {
            out.fightingPx = atPx;
        } else if (name == rules.keyPosName && placed) {
            out.hasKeyPos = true;
            out.keyPosPx = atPx;
            // The depth AddScaledEntity gives the key: key_pos's own GetPosition(),
            // a placed node's z and so already in the level's units.
            double z = 0.0;
            const Tscn::Value* eth = node.Meta("eth_z");
            const Tscn::Value* index = node.Find("z_index");
            out.keyZUnits = eth != nullptr && eth->AsNumber(z) ? z : (index != nullptr && index->AsNumber(z) ? z : 0.0);
        } else if (name == rules.platformPosName && placed) {
            out.hasPlatformPos = true;
            out.platformPosPx = atPx;
        } else if (name == rules.platformEntity && out.platformNode.empty()) {
            // A sibling of the one the death adds, kept as its template: the
            // entity has no .png of its own, so both its box and its picture
            // come from a node the level already places.
            out.platformNode = node.name;
        } else if (name == rules.wallName) {
            // By NODE, because that is how Demolish holds it.
            out.wallNode = node.name;
        }
    }
    if (!out.present) return true;
    out.keyName = out.name + "#key";
    out.restockSite = Restock::Resolve(scene, rules.restock);

    // Without these two it has nowhere to appear from and nowhere to fight, which
    // is a boss that hovers at the origin rather than one that is built.
    if (out.shoutPx == glm::dvec2(0.0) || out.fightingPx == glm::dvec2(0.0)) {
        error = out.name + " is a dark dragon in a level missing " + rules.shoutName + " or " + rules.fightingName;
        return false;
    }
    // And without a key it drops nothing, in a level whose only lock its fight is.
    if (!out.hasKeyPos) {
        error = out.name + " is a dark dragon in a level with no " + rules.keyPosName;
        return false;
    }
    return true;
}

} // namespace MagicPortals::DarkDragon
