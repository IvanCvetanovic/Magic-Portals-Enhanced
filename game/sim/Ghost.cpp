#include "sim/Ghost.hpp"

#include "sim/Units.hpp"

#include "core/Components.hpp"
#include "core/DetMath.hpp"
#include "core/Json.hpp"
#include "core/PhysicsSystem.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>

namespace MagicPortals::Ghost {

namespace {

namespace Json = Supersonic::Json;
using Supersonic::TransformComponent;

constexpr const char* kAtSpawn = "spawn";
constexpr const char* kAtQuarter = "quarter";
constexpr const char* kAtAnchor = "anchor";
constexpr const char* kFilterLinear = "linear";
constexpr const char* kFilterSmoothEnd = "smooth_end";
constexpr const char* kScaleSpawn = "spawn";

glm::dvec2 PxOf(entt::registry& registry, entt::entity body) {
    return Units::ToPixels(registry.get<TransformComponent>(body).position);
}

double Squared(const glm::dvec2& v) {
    return v.x * v.x + v.y * v.y;
}

double Length(const glm::dvec2& v) {
    return std::sqrt(Squared(v));
}

// NOT called Alive: State has a member of that name, and class scope would hide
// this one inside Tick. Diamonds.cpp made exactly that mistake with Standing.
bool BodyValid(entt::registry& registry, entt::entity body) {
    return body != entt::null && registry.valid(body) && registry.all_of<TransformComponent>(body);
}

bool PositionOf(const Tscn::Node& node, glm::dvec2& out) {
    const Tscn::Value* position = node.Find("position");
    if (position == nullptr || position->kind != Tscn::Value::Kind::Vector2) return false;
    out = glm::dvec2(position->numbers[0], position->numbers[1]);
    return true;
}

// The converter QUOTES the original's custom data - metadata/holdTime = "4000" -
// and Tscn::Value::AsNumber reads that as readily as a bare one.
double MetaNumber(const Tscn::Node& node, const char* key, double fallback) {
    const Tscn::Value* value = node.Meta(key);
    double read = 0.0;
    if (value == nullptr || !value->AsNumber(read)) return fallback;
    return read;
}

// smoothEnd(v) = sin(v x PIb) (bytes 313899..313971), through DetMath as every
// trigonometric call on the simulation's side is: the point it eases is where the
// ghost shoots from.
double SmoothEnd(double bias) {
    return static_cast<double>(Supersonic::DetMath::sin(static_cast<float>(bias) * 1.570796327f));
}

double Filtered(Filter filter, double bias) {
    return filter == Filter::SmoothEnd ? SmoothEnd(bias) : bias;
}

// One placed waypoint, before the prefix has claimed it - Minions::Find's shape,
// because the patrol this boss hands its escort is an ordinary minion patrol.
struct Placed {
    std::string entityName;
    glm::dvec2 atPx{0.0, 0.0};
    double holdMs = 0.0;
};

// FloatColor(uint) (bytes 86246..86521): a = (c >> 24) / 255, r = (c >> 16 & 255)
// / 255, g and b likewise. Written in ghost.json as the constant the script
// pushes, "0xAARRGGBB".
bool Argb(const Json::Value& value, glm::dvec4& out) {
    if (!value.IsString()) return false;
    const std::string text = value.AsString();
    if (text.size() != 10 || text.compare(0, 2, "0x") != 0) return false;
    char* end = nullptr;
    const unsigned long argb = std::strtoul(text.c_str() + 2, &end, 16);
    if (end == nullptr || *end != '\0') return false;
    const auto channel = [argb](int shift) { return static_cast<double>((argb >> shift) & 0xFFu) / 255.0; };
    out = glm::dvec4(channel(16), channel(8), channel(0), channel(24));
    return true;
}

bool ReadBounce(const Json::Value& value, Bounce& out) {
    if (!value.IsObject() || !value["from"].IsNumber() || !value["to"].IsNumber() ||
        !value["stride_ms"].IsNumber() || !(value["stride_ms"].AsNumber() > 0.0) ||
        !(value["from"].AsNumber() > 0.0) || !(value["to"].AsNumber() > 0.0)) {
        return false;
    }
    out.from = value["from"].AsNumber();
    out.to = value["to"].AsNumber();
    out.strideMs = value["stride_ms"].AsNumber();
    return true;
}

bool UnitInterval(const Json::Value& value) {
    return value.IsNumber() && value.AsNumber() >= 0.0 && value.AsNumber() <= 1.0;
}

// linearMotion (motions.json ghost), as SHOOTING and RAGE_MODE call it: its first
// call writes originalPos from where the ghost stands, and every call puts the
// ghost at originalPos plus the swing.
void Hover(State& ghost, const Motion::Rules& motion, double frameMs) {
    if (!ghost.hover.started) ghost.hoverOriginPx = ghost.atPx;
    Motion::Advance(motion, ghost.hover, frameMs);
    ghost.atPx = ghost.hoverOriginPx + Motion::OffsetPx(ghost.hover);
}

} // namespace

double Bounce::ScaleAt(double elapsedMs) const {
    // bounce() #55-102: which leg (elapsed / stride), which way it runs (that count
    // odd), how far along (the remainder over the stride), eased by smoothEnd.
    const double strides = std::floor(elapsedMs / strideMs);
    double bias = (elapsedMs - strides * strideMs) / strideMs;
    if (std::fmod(strides, 2.0) == 1.0) bias = 1.0 - bias;
    return from + (to - from) * SmoothEnd(bias);
}

void Appear::Update(double frameMs) {
    if (points.empty()) return;
    // FrameTimer::set (bytes 23254..23650): #3-4 the stride, #6-11 the time added,
    // #13-43 a changed first or last starts again at first with no time, else
    // #45-76 ONE frame once the time reaches the stride, the stride taken off, and
    // past last - this manager does not repeat - held on last with no time.
    const int wantLast = static_cast<int>(points.size()) - 1;
    strideMs = points[static_cast<std::size_t>(frame)].timeMs;
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
    strideMs = points[static_cast<std::size_t>(frame)].timeMs;
}

Appear::Point Appear::Current() const {
    if (points.empty()) return Point{};
    // getCurrentPoint #3-28: the next waypoint, or this one again on the last.
    const Point& current = points[static_cast<std::size_t>(frame)];
    const Point& next = points[static_cast<std::size_t>(IsLastFrame() ? frame : frame + 1)];
    // FrameTimer::getBias (23716..23810): min(stride, time) / max(stride, 1).
    const double bias = std::min(strideMs, timeMs) / std::max(strideMs, 1.0);
    // #54-123: the CURRENT waypoint's filter, and interpolate (a + (b - a) x f) for
    // the scale, the colour (rgb and alpha each) and the point.
    const double f = Filtered(current.filter, bias);
    Point blended = current;
    blended.atPx = current.atPx + (next.atPx - current.atPx) * f;
    blended.colour = current.colour + (next.colour - current.colour) * f;
    blended.scale = current.scale + (next.scale - current.scale) * f;
    return blended;
}

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
    const auto text_ = [&](const char* group, const char* key, std::string& into) -> bool {
        const Json::Value& block = root[group];
        if (!block.Has(key) || !block[key].IsString()) {
            error = path + ": " + std::string(group) + "." + key + " is missing or not a string";
            return false;
        }
        into = block[key].AsString();
        return true;
    };

    Rules read;
    double maxHp = 0.0;
    double deletesId = 0.0;
    if (!text_("anchor", "entity_name", read.anchorName)) return false;
    if (!number("body", "max_hp", maxHp)) return false;
    if (!text_("spawn", "by_entity", read.spawnedBy)) return false;
    if (!number("spawn", "z_offset_units", read.spawnZOffsetUnits)) return false;
    if (!number("spawn", "scale", read.spawnScale)) return false;
    if (!number("spawn", "deletes_entity_id", deletesId)) return false;
    if (!number("appear", "mid_fraction", read.midFraction)) return false;
    if (!number("appear", "seed_elapsed_ms", read.seedElapsedMs)) return false;
    if (!number("shooting", "interval_ms", read.intervalMs)) return false;
    if (!number("shooting", "muzzle_x", read.muzzlePx.x)) return false;
    if (!number("shooting", "muzzle_y", read.muzzlePx.y)) return false;
    if (!text_("shooting", "red_line_entity_name", read.redLineName)) return false;
    if (!text_("shooting", "diamond_spawn_entity_name", read.diamondSpawnName)) return false;
    if (!number("shot", "speed_px_s", read.shotSpeedPxS)) return false;
    if (!number("shot", "hit_px", read.shotHitPx)) return false;
    if (!number("shot", "nudge_px", read.shotNudgePx)) return false;
    if (!number("damage", "to_rage_ms", read.toRageMs)) return false;
    if (!number("rage", "summon_window_ms", read.summonWindowMs)) return false;
    if (!text_("rage", "waypoint_prefix", read.waypointPrefix)) return false;
    if (!text_("rage", "anchor_entity_name", read.summonAnchorName)) return false;
    if (!number("death", "burst_ms", read.burstMs)) return false;
    if (!text_("death", "key_colour", read.keyColour)) return false;
    if (!text_("death", "key_entity_name", read.keyEntityName)) return false;
    read.maxHp = static_cast<int>(maxHp);

    // Each of these would look like a boss that works. No hp is a boss that is
    // already dead; no interval divides
    // the fight into an infinity of shots in one tick; and an empty prefix would
    // resolve every waypoint in the level into its escort's patrol.
    if (read.maxHp <= 0) {
        error = path + ": body.max_hp is above zero";
        return false;
    }
    // explode's two boxes (ghost.json blast): one the grabber finds it in, and
    // the body its ray has to meet. A box of nothing is a ghost no blast can find.
    {
        const Json::Value& blast = root["blast"];
        const auto pair = [&blast](const char* key, glm::dvec2& into, bool positive) {
            const Json::Value& value = blast[key];
            if (!value.IsArray() || value.AsArray().size() != 2 || !value.AsArray()[0].IsNumber() ||
                !value.AsArray()[1].IsNumber()) {
                return false;
            }
            into = glm::dvec2(value.AsArray()[0].AsNumber(), value.AsArray()[1].AsNumber());
            return !positive || (into.x > 0.0 && into.y > 0.0);
        };
        if (!blast.IsObject() || !pair("grab_box_px", read.grabBoxPx, true) ||
            !pair("body_box_px", read.bodyBoxPx, true) || !pair("body_offset_px", read.bodyOffsetPx, false)) {
            error = path + ": blast needs grab_box_px and body_box_px (two numbers above 0) and body_offset_px";
            return false;
        }
    }
    // ins 594-630's five arguments and the port's template (sim/Restock).
    {
        const auto restockName = [&](const char* key, std::string& into) { return text_("restock", key, into); };
        if (!restockName("search", read.restock.search) || !restockName("add", read.restock.add) ||
            !restockName("spawn", read.restock.spawn) || !restockName("effect", read.restock.effect) ||
            !restockName("alias", read.restock.alias) || !restockName("template", read.restock.templateEntity)) {
            return false;
        }
        // A search for nothing finds nothing and restocks every beat; a spawn or a
        // template of nothing is a restock that can never be made.
        if (read.restock.search.empty() || read.restock.spawn.empty() || read.restock.templateEntity.empty()) {
            error = path + ": restock.search, restock.spawn and restock.template name an entity";
            return false;
        }
    }
    if (read.intervalMs <= 0.0) {
        error = path + ": shooting.interval_ms is above zero";
        return false;
    }
    if (read.shotSpeedPxS <= 0.0 || read.shotHitPx <= 0.0 || read.shotNudgePx < 0.0) {
        error = path + ": shot.speed_px_s and shot.hit_px are above zero, and shot.nudge_px is not below it";
        return false;
    }
    if (read.anchorName.empty() || read.waypointPrefix.empty() || read.summonAnchorName.empty()) {
        error = path + ": anchor, rage.waypoint_prefix and rage.anchor_entity_name all name something";
        return false;
    }
    if (read.keyColour.empty() || read.keyEntityName.empty()) {
        error = path + ": death.key_colour is the colour key.ent's own custom data carries, and death.key_entity_name "
                       "the entity DEATH adds";
        return false;
    }
    // What adds it: a knight that adds nothing, or at a scale of nothing, would be a
    // ghost that never comes or cannot be seen - and neither is the script's.
    if (read.spawnedBy.empty() || !(read.spawnScale > 0.0) || deletesId < 0.0 ||
        deletesId != std::floor(deletesId)) {
        error = path + ": spawn.by_entity names the entity whose death adds the ghost, spawn.scale is above zero "
                       "and spawn.deletes_entity_id is a whole number from 0";
        return false;
    }
    read.deletesEntityId = static_cast<int>(deletesId);
    if (!(read.midFraction > 0.0 && read.midFraction < 1.0)) {
        error = path + ": appear.mid_fraction lies between the two ends it is the fraction of";
        return false;
    }

    // The five waypoints, each stated in full: a leg nobody decoded would fly the
    // ghost a path the original does not.
    const Json::Value& waypoints = root["appear"]["waypoints"];
    if (!waypoints.IsArray() || waypoints.AsArray().empty()) {
        error = path + ": appear.waypoints is a list of the init block's addWaypoint calls";
        return false;
    }
    for (const Json::Value& entry : waypoints.AsArray()) {
        const std::string where = path + ": appear.waypoints[" + std::to_string(read.waypoints.size()) + "]";
        WaypointRule rule;
        const std::string at = entry["at"].AsString("");
        if (at == kAtSpawn) {
            rule.at = WaypointAt::Spawn;
        } else if (at == kAtQuarter) {
            rule.at = WaypointAt::Quarter;
        } else if (at == kAtAnchor) {
            rule.at = WaypointAt::Anchor;
        } else {
            error = where + ".at is spawn, quarter or anchor";
            return false;
        }
        const std::string filter = entry["filter"].AsString("");
        if (filter == kFilterLinear) {
            rule.filter = Filter::Linear;
        } else if (filter == kFilterSmoothEnd) {
            rule.filter = Filter::SmoothEnd;
        } else {
            error = where + ".filter is linear or smooth_end";
            return false;
        }
        if (!entry["time_ms"].IsNumber() || entry["time_ms"].AsNumber() < 0.0) {
            error = where + ".time_ms is a number from 0";
            return false;
        }
        rule.timeMs = entry["time_ms"].AsNumber();
        if (!Argb(entry["argb"], rule.colour)) {
            error = where + ".argb is the FloatColor constant, \"0xAARRGGBB\"";
            return false;
        }
        const Json::Value& scale = entry["scale"];
        if (scale.IsString() && scale.AsString() == kScaleSpawn) {
            rule.scaleIsSpawn = true;
        } else if (scale.IsNumber() && scale.AsNumber() > 0.0) {
            rule.scale = scale.AsNumber();
        } else {
            error = where + ".scale is \"spawn\" or a number above zero";
            return false;
        }
        read.waypoints.push_back(rule);
    }

    // What the arms write on the entity: none defaulted, since a cell or an alpha
    // nobody decoded would be drawn as if it were the script's.
    const Json::Value& look = root["look"];
    const auto frame = [&look](const char* key, int& into) {
        const Json::Value& value = look[key];
        if (!value.IsNumber() || value.AsNumber() < 0.0 || value.AsNumber() != std::floor(value.AsNumber())) {
            return false;
        }
        into = static_cast<int>(value.AsNumber());
        return true;
    };
    if (!look.IsObject() || !frame("shooting_frame", read.shootingFrame) || !frame("rage_frame", read.rageFrame) ||
        !UnitInterval(look["rage_alpha"]) || !UnitInterval(look["alpha_leaving_rage"]) ||
        !UnitInterval(look["damage_alpha"]) || !UnitInterval(look["death_alpha"]) ||
        !ReadBounce(look["damage_bounce"], read.damageBounce) || !ReadBounce(look["death_bounce"], read.deathBounce)) {
        error = path + ": look needs shooting_frame and rage_frame (whole numbers from 0), rage_alpha, "
                       "alpha_leaving_rage, damage_alpha and death_alpha (each 0..1), and damage_bounce and "
                       "death_bounce (from and to above 0, stride_ms above 0)";
        return false;
    }
    read.rageAlpha = look["rage_alpha"].AsNumber();
    read.alphaLeavingRage = look["alpha_leaving_rage"].AsNumber();
    read.damageAlpha = look["damage_alpha"].AsNumber();
    read.deathAlpha = look["death_alpha"].AsNumber();

    // The one timer every shot steps (shot.cells).
    const Json::Value& cells = root["shot"]["cells"];
    if (!cells.IsObject() || !cells["first"].IsNumber() || !cells["last"].IsNumber() ||
        !cells["stride_ms"].IsNumber() || cells["first"].AsNumber() < 0.0 ||
        cells["last"].AsNumber() < cells["first"].AsNumber() || !(cells["stride_ms"].AsNumber() > 0.0)) {
        error = path + ": shot.cells needs first, last from first, and a stride_ms above 0";
        return false;
    }
    read.shotFirstCell = static_cast<int>(cells["first"].AsNumber());
    read.shotLastCell = static_cast<int>(cells["last"].AsNumber());
    read.shotCellStrideMs = cells["stride_ms"].AsNumber();

    out = std::move(read);
    return true;
}

bool Plays(const Rules& rules, const Tscn::Node& node) {
    // By NAME. Boss::Plays reads an adder's metadata/entity, and ghost_pos has no
    // such field - the entity name is the whole of it. ghost.json says why.
    return Roles::EntityName(node) == rules.anchorName;
}

void State::Spawn(const glm::dvec2& knightPx) {
    if (!present || spawned) return;
    spawned = true;
    initialised = false;
    spawnPx = knightPx;
    atPx = knightPx;
    // AddEntity's scale(0.3), on an entity whose own colour is the .ent's white at
    // alpha 1 until its callback writes one.
    look = Look{};
    look.scale = rules.spawnScale;
    blinkMs = 0.0;
    hover = Motion::Linear{};
}

State::Turn State::Tick(entt::registry& registry, entt::entity player, bool playerAlive,
                        std::size_t minionsStanding, const Motion::Rules& motion, float dt) {
    Turn turn;
    spentPx.clear();
    if (!present) return turn;

    const double ms = static_cast<double>(dt) * 1000.0;
    const double seconds = static_cast<double>(dt);
    const bool playerThere = BodyValid(registry, player);
    const glm::dvec2 playerPx = playerThere ? PxOf(registry, player) : glm::dvec2(0.0);

    // The shots fly first, so one fired last tick is judged where this step left
    // the player - the same moment every other module judges a position at.
    {
        const double hit2 = rules.shotHitPx * rules.shotHitPx;
        std::vector<Shot> living;
        living.reserve(shots.size());
        for (Shot& shot : shots) {
            shot.atPx += shot.velocityPx * seconds;
            shot.flownPx += Length(shot.velocityPx) * seconds;

            // fakeRadius: within scale(16) of its target an isCharacter has hp
            // set to 0. The ghost only ever targets the player, so killPortal -
            // the branch minions.json defers - is unreachable from here.
            if (playerThere && !playerKilled && Squared(playerPx - shot.atPx) < hit2) {
                playerKilled = true;
                killedBy = name;
                spentPx.push_back(shot.atPx);
                continue; // and the shot destroys itself
            }
            if (shot.flownPx >= shot.rangePx) {
                spentPx.push_back(shot.atPx);
                continue;
            }
            living.push_back(shot);
        }
        shots = std::move(living);
    }

    // R4: nothing of it runs before the black knight's death adds it, and its first
    // callback is the tick after that (Spawn).
    if (!spawned || gone) return turn;

    if (!initialised) {
        // THE INIT BLOCK (#26-324): elapsedTime, state and shotsFired 0, hp 3, the
        // red line, and the five waypoints from where it stands now and ghost_pos -
        // then playGameMusic(bossFight = true), which the layer plays.
        initialised = true;
        turn.initialised = true;
        hp = rules.maxHp;
        phase = Phase::Appearing;
        elapsedMs = 0.0;
        shotsFired = 0;
        const glm::dvec2 current = atPx;
        const glm::dvec2 mid = current + (anchorPx - current) * rules.midFraction;
        appear = Appear{};
        for (const WaypointRule& rule : rules.waypoints) {
            Appear::Point point;
            point.atPx = rule.at == WaypointAt::Spawn ? current : (rule.at == WaypointAt::Quarter ? mid : anchorPx);
            point.timeMs = rule.timeMs;
            point.colour = rule.colour;
            point.filter = rule.filter;
            // GetScale().x: the entity's own scale, which AddEntity gave it.
            point.scale = rule.scaleIsSpawn ? look.scale : rule.scale;
            appear.points.push_back(point);
        }
        hover = Motion::Start(motion.ghost, motion.ghost.startFrom);
    }

    elapsedMs += ms;

    switch (phase) {
        case Phase::Appearing: {
            // #369-494: update, getCurrentPoint, and the point, the colour, the
            // scale and the alpha written; the last frame goes to SHOOTING.
            appear.Update(ms);
            const Appear::Point point = appear.Current();
            atPx = point.atPx;
            look.colour = glm::dvec3(point.colour);
            look.alpha = point.colour.a;
            look.scale = point.scale;
            if (appear.IsLastFrame()) {
                phase = Phase::Shooting;
                // NOT zero: the original seeds the clock at the shot interval, so
                // the first shot comes on the first frame of SHOOTING.
                elapsedMs = rules.seedElapsedMs;
                shotsFired = 0;
            }
            break;
        }

        case Phase::Shooting: {
            // #499-512: linearMotion, then SetFrame(0).
            Hover(*this, motion, ms);
            look.frame = rules.shootingFrame;
            if (elapsedMs > rules.intervalMs) {
                elapsedMs = 0.0;
                ++beats;

                // The restocks ride the same beat, on whichever side of the red
                // line the player stands (ins 593: the shot's two gates jump here):
                // addEntityIfItCantBeFound puts a barrel bomb back at barrel_spawn
                // (ins 594-630), the ghost's one weakness, and a fire diamond at its
                // spawn (ins 632-667), which is what makes dropping one down the
                // gutter mouth survivable. Game asks whether either is still there.
                turn.restock = true;
                turn.restockDiamond = hasDiamondSpawn;

                // And it shoots only at a player past the red line, from its own
                // point - wherever the hover has put it - plus the muzzle.
                if (playerThere && (!hasRedLine || playerPx.x > redLineX)) {
                    const glm::dvec2 from = atPx + rules.muzzlePx;
                    const glm::dvec2 along = playerPx - from;
                    const double reach = Length(along);
                    if (reach > 0.0) {
                        Shot made;
                        made.id = ++shotsMade;
                        made.directionPx = along / reach;
                        made.fromPx = from;
                        made.destinyPx = playerPx;
                        // ETHCallback_enemy_shoot's first call moves it nudge_px
                        // along its way before it flies, and measures its range from
                        // the muzzle (ghost.json shot._nudge_px).
                        made.atPx = from + made.directionPx * rules.shotNudgePx;
                        made.flownPx = rules.shotNudgePx;
                        made.velocityPx = made.directionPx * rules.shotSpeedPxS;
                        made.rangePx = reach;
                        shots.push_back(made);
                        ++shotsFired;
                    }
                }
            }

            // isBurned / healBurn. Only this arm reads it in the original, so a
            // blast that reaches it in another arm - appearing, hurt or raging -
            // leaves the flag set, and it is counted here, on its next SHOOTING
            // update.
            if (burned) {
                burned = false;
                --hp;
                ++hits;
                phase = Phase::Damage;
                elapsedMs = 0.0;
            }
            break;
        }

        case Phase::Damage: {
            // hp first: the original tests it before anything else in this arm.
            if (hp <= 0) {
                phase = Phase::Death;
                elapsedMs = 0.0;
            } else {
                // #789-816: SetAlpha(0.5) and bounce(1 -> 1.1, 500), in place.
                look.alpha = rules.damageAlpha;
                blinkMs += ms;
                look.scale = rules.damageBounce.ScaleAt(blinkMs);
                if (elapsedMs > rules.toRageMs) {
                    phase = Phase::Rage;
                    elapsedMs = 0.0;
                    summonedThisRage = false;
                }
            }
            break;
        }

        case Phase::Rage: {
            // #981-998: linearMotion, SetFrame(3), SetAlpha(0.5).
            Hover(*this, motion, ms);
            look.frame = rules.rageFrame;
            look.alpha = rules.rageAlpha;
            // #1013-1083: while the player stands with hp above 0, a fire diamond
            // back at its spawn on EVERY update if none is left - so in rage one
            // drowned in the gutter comes straight back.
            if (playerThere && playerAlive) turn.restockDiamond = hasDiamondSpawn;
            // It waits on its escort. While none stands it calls one, inside the
            // first window; once one stands it STAYS here until the player kills
            // it, and only then has the window passed and it goes back to
            // shooting.
            if (minionsStanding == 0) {
                if (elapsedMs < rules.summonWindowMs) {
                    if (!summonedThisRage && hasSummonAnchor) {
                        turn.summon = true;
                        summonedThisRage = true;
                        ++summons;
                    }
                } else {
                    phase = Phase::Shooting;
                    elapsedMs = 0.0;
                    // #1199-1201: SetAlpha(1) on the way back out.
                    look.alpha = rules.alphaLeavingRage;
                }
            }
            break;
        }

        case Phase::Death: {
            // #1220-1263: SetAlpha(1) and bounce(1.1 -> 1.25, 400), in place.
            look.alpha = rules.deathAlpha;
            blinkMs += ms;
            look.scale = rules.deathBounce.ScaleAt(blinkMs);
            if (elapsedMs > rules.burstMs) {
                gone = true;
                turn.droppedKey = true;
                turn.keyPx = atPx;
            }
            break;
        }
    }

    return turn;
}

bool State::BlastReaches(entt::registry& registry, const glm::dvec2& blastPx, double blastRadiusPx) const {
    if (!Alive()) return false;

    // EntityGrabber (bytes 23924..24990, #126-194): a box-shaped entity is taken
    // when Shape(GetPositionXY(), GetCollisionBox().size x getScale(), GetAngle())
    // overlaps the blast's sphere. The ghost is never turned.
    const glm::dvec2 grabHalf = rules.grabBoxPx * 0.5;
    const glm::dvec2 outside = glm::max(glm::abs(blastPx - atPx) - grabHalf, glm::dvec2(0.0));
    if (Squared(outside) > blastRadiusPx * blastRadiusPx) return false;

    // getFirstContactExcept(pos, the entity's point, NonProjectileChooser) (#63-109):
    // the closest contact along the ray must be the ghost itself. Its body is
    // ghost.ent's box at the scale it was added with (CreateBody at AddEntity,
    // ETHPhysicsSimulator.cpp:72-73; SetScale does not rebuild it, ETHEntity.cpp:
    // 601-608), about its point plus the Collision offset.
    const glm::dvec2 bodyHalf = rules.bodyBoxPx * rules.spawnScale * 0.5;
    const glm::dvec2 bodyCentre = atPx + rules.bodyOffsetPx * rules.spawnScale;
    const glm::dvec2 along = atPx - blastPx;
    // Where the segment enters the body, as a fraction of it (the slab test). A
    // blast inside the body enters nothing: b2PolygonShape::RayCast returns false
    // from inside (box2d b2PolygonShape.cpp:199-258), so the closest contact is
    // something else or nothing, and either way not the ghost (#86-109).
    double enter = 0.0;
    double leave = 1.0;
    bool inside = true;
    for (int axis = 0; axis < 2; ++axis) {
        const double lo = bodyCentre[axis] - bodyHalf[axis];
        const double hi = bodyCentre[axis] + bodyHalf[axis];
        const double from = blastPx[axis];
        if (from < lo || from > hi) inside = false;
        if (along[axis] == 0.0) {
            if (from < lo || from > hi) return false;
            continue;
        }
        double t0 = (lo - from) / along[axis];
        double t1 = (hi - from) / along[axis];
        if (t0 > t1) std::swap(t0, t1);
        enter = std::max(enter, t0);
        leave = std::min(leave, t1);
    }
    if (inside || enter > leave) return false;

    // And nothing solid before that entry: the port's ray, as Fire::Blast casts it
    // for every other entity a blast grabs (solid bodies; the original's also stops
    // on a sensor, which the port's Fire does not do anywhere).
    const glm::vec3 from = Units::ToWorld(blastPx.x, blastPx.y);
    const glm::vec3 to = Units::ToWorld(atPx.x, atPx.y);
    const float length = glm::length(to - from);
    if (length <= 1e-6f) return false;
    const float reach = static_cast<float>(enter) * length;
    if (reach <= 1e-6f) return true;
    const Supersonic::PhysicsSystem::RayHit seen =
        Supersonic::PhysicsSystem::Raycast(registry, from, (to - from) / length, reach, entt::null, false);
    return !seen.hit;
}

bool Find(const Tscn::Scene& scene, const Roles::Table& roles, const Rules& rules, State& out, std::string& error) {
    out = State{};
    out.rules = rules;

    std::vector<Placed> placed;
    const std::string deletesSuffix = "_" + std::to_string(rules.deletesEntityId);

    for (const Tscn::Node& node : scene.nodes) {
        if (node.parent != ".") continue;

        const std::string entity = Roles::EntityName(node);
        const std::string role = Roles::RoleOf(roles, node);

        // The entity the knight's death deletes, by its id: the converter names a
        // node <entity>_<id> (tscn.py _node_name), so the id is its name's suffix.
        if (rules.deletesEntityId > 0 && out.deletesNode.empty() && node.name.size() > deletesSuffix.size() &&
            node.name.compare(node.name.size() - deletesSuffix.size(), deletesSuffix.size(), deletesSuffix) == 0) {
            out.deletesNode = node.name;
        }

        if (role == Roles::kWaypoint) {
            Placed one;
            one.entityName = entity;
            if (!PositionOf(node, one.atPx)) {
                error = node.name + " is a waypoint with no position";
                return false;
            }
            one.holdMs = MetaNumber(node, "holdTime", 0.0);
            placed.push_back(one);
            continue;
        }

        // The three markers this boss reads BY NAME. None of them carries a role
        // the port plays - minion_spawn_point carries no role at all - so none is
        // reachable any other way.
        if (entity == rules.redLineName) {
            glm::dvec2 atPx(0.0, 0.0);
            if (PositionOf(node, atPx)) {
                out.hasRedLine = true;
                out.redLineX = atPx.x;
            }
            continue;
        }
        if (entity == rules.summonAnchorName) {
            glm::dvec2 atPx(0.0, 0.0);
            if (PositionOf(node, atPx)) {
                out.hasSummonAnchor = true;
                out.summonPx = atPx;
            }
            continue;
        }
        if (entity == rules.diamondSpawnName) {
            glm::dvec2 atPx(0.0, 0.0);
            if (PositionOf(node, atPx)) {
                out.hasDiamondSpawn = true;
                out.diamondSpawnPx = atPx;
            }
            continue;
        }

        if (entity != rules.anchorName) continue;

        glm::dvec2 atPx(0.0, 0.0);
        if (!PositionOf(node, atPx)) {
            error = node.name + " is a ghost with no position";
            return false;
        }
        out.present = true;
        out.name = node.name;
        out.keyName = node.name + "#key";
        out.anchorPx = atPx;
        out.atPx = atPx;
    }

    if (!out.present) {
        // A level with no ghost has nothing for a knight's death to delete either.
        out.deletesNode.clear();
        return true;
    }

    out.hp = rules.maxHp;
    out.phase = Phase::Appearing;

    // The escort's patrol, resolved the way the original resolves one: seek
    // prefix + n until one is missing. Done HERE rather than at summon time
    // because Game::AfterStep has no scene to resolve it from.
    for (std::size_t index = 0;; ++index) {
        const std::string want = rules.waypointPrefix + std::to_string(index);
        const Placed* found = nullptr;
        for (const Placed& one : placed) {
            if (one.entityName == want) {
                found = &one;
                break;
            }
        }
        if (found == nullptr) break;
        Minions::Waypoint made;
        made.name = want;
        made.atPx = found->atPx;
        made.holdMs = found->holdMs;
        out.patrol.push_back(made);
    }

    return true;
}

} // namespace MagicPortals::Ghost
