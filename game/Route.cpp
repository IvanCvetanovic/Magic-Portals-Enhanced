#include "Route.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <sstream>
#include <utility>

#include "core/Components.hpp"
#include "core/Json.hpp"
#include "core/Log.hpp"

#include "sim/Chapters.hpp"
#include "sim/Game.hpp"
#include "sim/Units.hpp"

namespace MagicPortals::Route {

namespace {

namespace Json = Supersonic::Json;

// The file's keys. A key starting "_" is a comment and is never read.
namespace Key {
constexpr const char* kLevel = "level";
constexpr const char* kWinBy = "win_by";
constexpr const char* kHolds = "holds";
constexpr const char* kTaps = "taps";
constexpr const char* kWalk = "walk";
constexpr const char* kFrom = "from";
constexpr const char* kTo = "to";
constexpr const char* kTick = "tick";
constexpr const char* kRelease = "release";
constexpr const char* kAt = "at";
constexpr const char* kShot = "shot";
} // namespace Key

constexpr const char* kLeft = "left";
constexpr const char* kRight = "right";

bool OnlyKeys(const Json::Value& object, std::initializer_list<const char*> keys, const std::string& where,
              std::string& error) {
    for (const auto& [key, value] : object.AsObject()) {
        (void)value;
        if (!key.empty() && key.front() == '_') continue;
        if (std::none_of(keys.begin(), keys.end(), [&](const char* known) { return key == known; })) {
            error = where + ": no key \"" + key + "\" in a route";
            return false;
        }
    }
    return true;
}

// A whole number of at least `least`, where the file says one.
bool Whole(const Json::Value& object, const char* key, int least, int& out, const std::string& where,
           std::string& error) {
    if (!object.Has(key) || !object[key].IsNumber()) {
        error = where + ": \"" + key + "\" is not a number";
        return false;
    }
    const double value = object[key].AsNumber();
    if (value != std::floor(value) || value < least || value > 1e7) {
        error = where + ": \"" + key + "\" is not a whole number of at least " + std::to_string(least);
        return false;
    }
    out = static_cast<int>(value);
    return true;
}

std::string Px(const glm::dvec2& p) {
    std::ostringstream text;
    text.setf(std::ios::fixed);
    text.precision(2);
    text << "(" << p.x << ", " << p.y << ")";
    return text.str();
}

const char* ReasonName(Portals::TapReason reason) {
    switch (reason) {
    case Portals::TapReason::Fired:
        return "fired";
    case Portals::TapReason::NoShooter:
        return "no shooter";
    case Portals::TapReason::TooEarly:
        return "too early in the level";
    case Portals::TapReason::NoBudget:
        return "no portal left to place";
    case Portals::TapReason::Cooldown:
        return "the 400 ms between taps";
    case Portals::TapReason::InFlight:
        return "a shot still in flight";
    }
    return "?";
}

} // namespace

bool Load(const std::string& path, Plan& out, std::string& error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = path + ": cannot open";
        return false;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string text = buffer.str();
    Json::Parser parser(text);
    Json::Value root;
    if (!parser.Parse(root)) {
        error = path + ": " + parser.Error();
        return false;
    }
    if (!root.IsObject()) {
        error = path + ": not an object";
        return false;
    }
    if (!OnlyKeys(root, {Key::kLevel, Key::kWinBy, Key::kHolds, Key::kTaps}, path, error)) return false;

    Plan plan;
    plan.file = path;
    if (!root.Has(Key::kLevel) || !root[Key::kLevel].IsString() || root[Key::kLevel].AsString().empty()) {
        error = path + ": \"level\" is not a level's name";
        return false;
    }
    plan.level = root[Key::kLevel].AsString();
    if (!Whole(root, Key::kWinBy, 1, plan.winBy, path, error)) return false;

    if (root.Has(Key::kHolds)) {
        if (!root[Key::kHolds].IsArray()) {
            error = path + ": \"holds\" is not a list";
            return false;
        }
        int index = 0;
        for (const Json::Value& entry : root[Key::kHolds].AsArray()) {
            const std::string where = path + ": holds[" + std::to_string(index++) + "]";
            if (!entry.IsObject() || !OnlyKeys(entry, {Key::kWalk, Key::kFrom, Key::kTo}, where, error)) {
                if (error.empty()) error = where + ": not an object";
                return false;
            }
            Hold hold;
            const std::string walk = entry.Has(Key::kWalk) ? entry[Key::kWalk].AsString() : "";
            hold.direction = walk == kLeft ? -1.0f : walk == kRight ? 1.0f : 0.0f;
            if (hold.direction == 0.0f) {
                error = where + ": \"walk\" is neither \"left\" nor \"right\"";
                return false;
            }
            if (!Whole(entry, Key::kFrom, 1, hold.from, where, error) ||
                !Whole(entry, Key::kTo, hold.from, hold.to, where, error)) {
                return false;
            }
            plan.holds.push_back(hold);
        }
    }

    if (root.Has(Key::kTaps)) {
        if (!root[Key::kTaps].IsArray()) {
            error = path + ": \"taps\" is not a list";
            return false;
        }
        int index = 0;
        for (const Json::Value& entry : root[Key::kTaps].AsArray()) {
            const std::string where = path + ": taps[" + std::to_string(index++) + "]";
            if (!entry.IsObject() ||
                !OnlyKeys(entry, {Key::kTick, Key::kRelease, Key::kAt, Key::kShot}, where, error)) {
                if (error.empty()) error = where + ": not an object";
                return false;
            }
            Tap tap;
            if (!Whole(entry, Key::kTick, 1, tap.tick, where, error)) return false;
            tap.releaseTick = tap.tick + 1;
            if (entry.Has(Key::kRelease) && !Whole(entry, Key::kRelease, tap.tick + 1, tap.releaseTick, where, error)) {
                return false;
            }
            const bool point = entry.Has(Key::kAt) && entry[Key::kAt].IsArray() &&
                               entry[Key::kAt].AsArray().size() == 2 && entry[Key::kAt].AsArray()[0].IsNumber() &&
                               entry[Key::kAt].AsArray()[1].IsNumber();
            if (!point) {
                error = where + ": \"at\" is not a point [x, y] of the level";
                return false;
            }
            tap.atPx = glm::dvec2(entry[Key::kAt].AsArray()[0].AsNumber(), entry[Key::kAt].AsArray()[1].AsNumber());
            if (entry.Has(Key::kShot)) {
                if (!entry[Key::kShot].IsBool()) {
                    error = where + ": \"shot\" is not true or false";
                    return false;
                }
                tap.shot = entry[Key::kShot].AsBool();
            }
            plan.taps.push_back(tap);
        }
        std::stable_sort(plan.taps.begin(), plan.taps.end(), [](const Tap& a, const Tap& b) { return a.tick < b.tick; });
        // The layer takes one scheduled touch at a time, oldest first, and holds it
        // down until its release: a second pressed before the first is up would be
        // read late, on another camera, and land somewhere else.
        for (std::size_t i = 1; i < plan.taps.size(); ++i) {
            if (plan.taps[i].tick <= plan.taps[i - 1].releaseTick) {
                error = path + ": the tap on tick " + std::to_string(plan.taps[i].tick) +
                        " goes down before the one on tick " + std::to_string(plan.taps[i - 1].tick) +
                        " is up (tick " + std::to_string(plan.taps[i - 1].releaseTick) + ")";
                return false;
            }
        }
    }
    out = std::move(plan);
    return true;
}

// ---- the driver ---------------------------------------------------------------

Driver::Driver(Plan plan) : m_plan(std::move(plan)) {}

Driver::Record Driver::record(const MagicPortalsLayer& layer) const {
    Record out;
    if (const Game::Level* level = layer.SimLevel()) {
        out.level = true;
        out.shotsFired = level->portals.shotsFired;
        out.lastTap = level->portals.lastTap;
    }
    return out;
}

void Driver::problem(const std::string& what) {
    m_problems.push_back(what);
    SUPERSONIC_LOG_WARN("Magic Portals") << "route " << m_plan.level << ": " << what << std::endl;
}

void Driver::Begin(MagicPortalsLayer& layer) {
    m_ticks = layer.LayerTicks();
    m_deaths = layer.Deaths();
    if (layer.Current() == nullptr || layer.Current()->name != m_plan.level || layer.SimLevel() == nullptr) {
        problem("the level " + m_plan.level + " is not loaded" +
                (layer.LoadError().empty() ? std::string() : ": " + layer.LoadError()));
        return;
    }
    for (const Hold& hold : m_plan.holds) layer.ScheduleDevHold(hold.from, hold.to, hold.direction);
    m_before = record(layer);
    schedule(layer, m_ticks + 1);
}

void Driver::schedule(MagicPortalsLayer& layer, int tick) {
    for (const Tap& tap : m_plan.taps) {
        if (tap.tick != tick) continue;
        // The view as this tick leaves it is the view the next tick's input is read
        // against: the camera moves after readInput (MagicPortalsLayer::stepLevel).
        const glm::dvec2 view = layer.ViewPx();
        const glm::dvec2 corner = layer.CameraCentrePx() - view * 0.5;
        const glm::dvec2 fraction = (tap.atPx - corner) / view;
        if (!(fraction.x > 0.0 && fraction.x < 1.0 && fraction.y > 0.0 && fraction.y < 1.0)) {
            problem("the tap on tick " + std::to_string(tap.tick) + " at " + Px(tap.atPx) +
                    " is off the view, which shows " + Px(corner) + " to " + Px(corner + view));
            continue;
        }
        layer.ScheduleDevTap(tap.tick, fraction, tap.releaseTick);
    }
}

void Driver::check(const MagicPortalsLayer& layer, const Tap& tap) {
    const std::string which = "the tap on tick " + std::to_string(tap.tick) + " at " + Px(tap.atPx);
    const Game::Level* level = layer.SimLevel();
    if (level == nullptr) {
        problem(which + " has no level to land in");
        return;
    }
    const Portals::State& portals = level->portals;
    const bool shoot = !m_before.level || portals.shotsFired != m_before.shotsFired ||
                       portals.lastTap.atPx != m_before.lastTap.atPx ||
                       portals.lastTap.reason != m_before.lastTap.reason;
    if (!tap.shot) {
        if (shoot) problem(which + " was to fire nothing, and fired a shot");
        return;
    }
    if (!shoot) {
        problem(which + " fired nothing: a control, a help block, a popup or a pause took it");
    } else if (portals.lastTap.reason != Portals::TapReason::Fired) {
        problem(which + " was refused: " + ReasonName(portals.lastTap.reason));
    } else if (glm::length(portals.lastTap.atPx - tap.atPx) > kTapTolerancePx) {
        problem(which + " was aimed at " + Px(portals.lastTap.atPx));
    } else {
        ++m_shots;
    }
}

void Driver::AfterTick(MagicPortalsLayer& layer) {
    if (Finished()) return;
    m_ticks = layer.LayerTicks();
    if (layer.Current() == nullptr || layer.Current()->name != m_plan.level) {
        problem("left the level on tick " + std::to_string(m_ticks));
        return;
    }
    if (layer.Deaths() != m_deaths) {
        problem("the player died on tick " + std::to_string(m_ticks));
        return;
    }
    for (const Tap& tap : m_plan.taps) {
        if (tap.tick == m_ticks) check(layer, tap);
    }
    const Game::Level* level = layer.SimLevel();
    if (level != nullptr && level->goals.completed) {
        m_wonTick = m_ticks;
        SUPERSONIC_LOG_INFO("Magic Portals") << "route " << m_plan.level << ": won on tick " << m_wonTick
                                             << " (win_by " << m_plan.winBy << ")" << std::endl;
        return;
    }
    if (m_ticks >= m_plan.winBy) {
        problem("not won by tick " + std::to_string(m_plan.winBy));
        return;
    }
    m_before = record(layer);
    schedule(layer, m_ticks + 1);
}

// ---- the replay layer ---------------------------------------------------------

void Trace(const MagicPortalsLayer& layer, entt::registry& registry, const char* why) {
    const Game::Level* level = layer.SimLevel();
    std::printf("    t %5d %-5s", layer.LayerTicks(), why);
    if (level == nullptr) {
        std::printf(" no level\n");
        return;
    }
    if (level->player != entt::null && registry.valid(level->player)) {
        const glm::dvec2 at =
            Units::ToPixels(registry.get<Supersonic::TransformComponent>(level->player).position);
        std::printf(" player (%.1f, %.1f)", at.x, at.y);
        if (const auto* body = registry.try_get<Supersonic::RigidBodyComponent>(level->player)) {
            std::printf(" v (%.0f, %.0f)", body->velocity.x * Units::kPixelsPerMetre,
                        -body->velocity.y * Units::kPixelsPerMetre);
        }
    }
    const glm::dvec2 camera = layer.CameraCentrePx();
    std::printf(" camera (%.1f, %.1f) shots %d/%d", camera.x, camera.y, level->portals.shotsFired,
                level->portals.shotsFailed);
    if (level->portals.flight) std::printf(" FLYING");
    for (const auto& placed : level->portals.placed) std::printf(" [%.1f, %.1f]", placed.atPx.x, placed.atPx.y);
    if (!level->portals.lastFailure.empty()) std::printf(" last fail: %s", level->portals.lastFailure.c_str());
    if (level->goals.completed) std::printf(" DOOR");
    // What else moves: every other traveller by its node's name, what the buttons
    // hold, and 1-32's beholder with its rocks.
    for (const entt::entity body : level->portals.travellers) {
        if (body == level->player || !registry.valid(body)) continue;
        const char* name = "?";
        for (const auto& [node, entity] : level->built.entities) {
            if (entity == body) name = node.c_str();
        }
        const glm::dvec2 at = Units::ToPixels(registry.get<Supersonic::TransformComponent>(body).position);
        std::printf("\n          %s (%.1f, %.1f)", name, at.x, at.y);
    }
    for (const auto& button : level->channels.buttons) {
        std::printf("\n          button %s channel %d %s", button.name.c_str(), button.channel,
                    level->channels.Pressed(button.channel) ? "PRESSED" : "up");
    }
    // A switched door's travel, and where each lift is: what a route waits on.
    for (const auto& door : level->channels.doors) {
        // Its status, ETHCallback_door_lift's: -1 closing, 0 shut, 1 opening, 2 open.
        std::printf("\n          door %s status %d %.3f", door.name.c_str(), static_cast<int>(door.motion.status),
                    static_cast<double>(door.motion.progress));
        if (registry.valid(door.entity)) {
            const glm::dvec2 at = Units::ToPixels(registry.get<Supersonic::TransformComponent>(door.entity).position);
            std::printf(" at (%.1f, %.1f)", at.x, at.y);
        }
    }
    for (const auto& lift : level->movers.lifts) {
        if (!registry.valid(lift.entity)) continue;
        const glm::dvec2 at = Units::ToPixels(registry.get<Supersonic::TransformComponent>(lift.entity).position);
        std::printf("\n          lift %s (%.1f, %.1f)", lift.name.c_str(), at.x, at.y);
    }
    // The fire (step 114): every fireball in the air, where one went off this
    // tick, what is burning or gone, and the bombs.
    for (const auto& ball : level->turrets.fireballs) {
        std::printf("\n          fireball %s (%.1f, %.1f)", ball.name.c_str(), ball.atPx.x, ball.atPx.y);
    }
    for (const glm::dvec2& atPx : level->turrets.spentPx) {
        std::printf("\n          fireball OFF at (%.1f, %.1f)", atPx.x, atPx.y);
    }
    for (const auto& burnable : level->fire.burnables) {
        if (burnable.burned || burnable.gone) {
            std::printf("\n          burnable %s %s", burnable.name.c_str(), burnable.gone ? "GONE" : "burning");
        }
    }
    for (const auto& bomb : level->fire.bombs) {
        if (bomb.requested || bomb.blown) {
            std::printf("\n          bomb %s %s", bomb.name.c_str(), bomb.blown ? "BLOWN" : "asked");
        }
    }
    // What a blast, a stone, a torch or a dragon broke (step 115), and by what.
    for (const auto& breakable : level->demolish.breakables) {
        if (breakable.broken) {
            std::printf("\n          breakable %s BROKEN by %s", breakable.name.c_str(), breakable.brokenBy.c_str());
        }
    }
    // What the player can carry: a diamond (a fire one turns every tap into a
    // fireball) and a key - where each is, and whether the player holds it.
    const auto held = [level](entt::entity owner) { return owner != entt::null && owner == level->player; };
    for (const auto& diamond : level->diamonds.diamonds) {
        std::printf("\n          %s diamond %s (%.1f, %.1f)%s", diamond.fire ? "fire" : "shock", diamond.name.c_str(),
                    diamond.atPx.x, diamond.atPx.y, diamond.gone ? " GONE" : (held(diamond.owner) ? " HELD" : ""));
    }
    // A key another body holds (a knight's: step 125, for 4-28), and a keyhole
    // whose door has gone with it.
    for (const auto& key : level->keys.keys) {
        std::printf("\n          key %s (%.1f, %.1f)%s", key.name.c_str(), key.atPx.x, key.atPx.y,
                    key.spent ? " SPENT"
                              : (held(key.owner) ? " HELD" : (key.owner != entt::null ? " held by another" : "")));
    }
    for (const auto& keyhole : level->keys.keyholes) {
        if (keyhole.unlocked) {
            std::printf("\n          keyhole %s UNLOCKED%s", keyhole.name.c_str(), keyhole.gone ? ", door gone" : "");
        }
    }
    // 4-19's red key state (step 118): once its key is taken, the fade's clock and
    // bias, and the wall it broke at the end.
    if (level->redKey.present && level->redKey.destroyed) {
        const auto& red = level->redKey;
        std::printf("\n          red key state %s TAKEN by %s, %.0f ms, bias %.3f%s%s", red.name.c_str(),
                    red.takenBy.c_str(), red.elapsedMs, red.bias, red.gone ? " GONE, wall " : "",
                    red.gone ? (red.wallName.empty() ? "(none)" : red.wallName.c_str()) : "");
    }
    // An anti-portal agent a shot killed, and the ring it took with it (step 126).
    for (const auto& agent : level->portals.agents) {
        if (agent.gone) std::printf("\n          agent %s KILLED", agent.name.c_str());
    }
    for (const auto& zone : level->portals.zones) {
        if (zone.gone) std::printf("\n          ring %s GONE", zone.name.c_str());
    }
    if (level->hazards.playerDied) std::printf("\n          PLAYER DIED: %s", level->hazards.killedBy.c_str());
    if (level->boss.beholder) {
        const auto& boss = *level->boss.beholder;
        std::printf("\n          beholder (%.1f, %.1f) phase %d hits %d", boss.atPx.x, boss.atPx.y,
                    static_cast<int>(boss.phase), boss.hits);
        for (const auto& rock : level->boss.rocks) {
            if (!registry.valid(rock.body)) continue;
            const glm::dvec2 at = Units::ToPixels(registry.get<Supersonic::TransformComponent>(rock.body).position);
            std::printf(" rock %s (%.1f, %.1f)", rock.name.c_str(), at.x, at.y);
        }
        if (level->boss.buttonRaised) std::printf(" BUTTON RAISED");
    }
    // 4-32's dragon (step 116): where its point is, its arm and hp, its fire clock
    // and angle, the shots it has fired, and the torches lit so far.
    if (level->darkDragon.present) {
        const auto& dragon = level->darkDragon;
        std::printf("\n          dark dragon (%.1f, %.1f) phase %d hp %d clock %.0f angle %.1f fired %d torch lit %d%s",
                    dragon.atPx.x, dragon.atPx.y, static_cast<int>(dragon.phase), dragon.hp, dragon.elapsedMs,
                    dragon.look.angleDeg, dragon.fired, level->torch.lit, dragon.alive ? "" : " (not here)");
    }
    // 3-32's ghost (step 117): its point, its arm and hp, whether a blast has burned
    // it this tick, its beat clock, its shots and burns, and its shots in the air.
    if (level->ghost.present) {
        const auto& ghost = level->ghost;
        std::printf("\n          ghost (%.1f, %.1f) phase %d hp %d%s clock %.0f fired %d hits %d summons %d%s",
                    ghost.atPx.x, ghost.atPx.y, static_cast<int>(ghost.phase), ghost.hp,
                    ghost.burned ? " BURNED" : "", ghost.elapsedMs, ghost.shotsFired, ghost.hits, ghost.summons,
                    ghost.Alive() ? "" : (ghost.gone ? " (gone)" : " (not here)"));
        for (const auto& shot : ghost.shots) {
            std::printf(" shot %d (%.1f, %.1f) to (%.1f, %.1f)", shot.id, shot.atPx.x, shot.atPx.y, shot.destinyPx.x,
                        shot.destinyPx.y);
        }
    }
    for (const auto& added : level->restock.added) {
        std::printf("\n          restocked %s at (%.1f, %.1f)%s", added.name.c_str(), added.atPx.x, added.atPx.y,
                    registry.valid(added.body) ? "" : " GONE");
        if (registry.valid(added.body)) {
            const glm::dvec2 at = Units::ToPixels(registry.get<Supersonic::TransformComponent>(added.body).position);
            std::printf(" now (%.1f, %.1f)", at.x, at.y);
        }
    }
    if (level->restock.refusals > 0) {
        std::printf("\n          restock REFUSED %d: %s", level->restock.refusals, level->restock.firstRefusal.c_str());
    }
    for (const auto& minion : level->minions.minions) {
        if (minion.gone || !registry.valid(minion.body)) continue;
        const glm::dvec2 at = Units::ToPixels(registry.get<Supersonic::TransformComponent>(minion.body).position);
        std::printf("\n          minion %s (%.1f, %.1f)", minion.name.c_str(), at.x, at.y);
    }
    std::printf("\n");
}

ReplayLayer::ReplayLayer(MagicPortalsLayer& game, Plan plan, int traceEvery)
    : m_game(game), m_driver(std::move(plan)), m_traceEvery(traceEvery) {}

void ReplayLayer::OnAttach(entt::registry& registry) {
    (void)registry;
    m_driver.Begin(m_game);
}

void ReplayLayer::OnFixedUpdate(entt::registry& registry, float fixedDelta) {
    (void)fixedDelta;
    const bool wasFinished = m_driver.Finished();
    m_driver.AfterTick(m_game);
    if (m_traceEvery <= 0 || m_traced) return;
    const int tick = m_game.LayerTicks();
    const std::vector<Tap>& taps = m_driver.Planned().taps;
    const bool tapped =
        std::any_of(taps.begin(), taps.end(), [tick](const Tap& tap) { return tap.tick == tick; });
    if (m_driver.Finished() && !wasFinished) {
        m_traced = true;
        Trace(m_game, registry, m_driver.WonTick() > 0 ? "WON" : "END");
    } else if (tapped) {
        Trace(m_game, registry, "TAP");
    } else if (tick % m_traceEvery == 0) {
        Trace(m_game, registry, "");
    }
}

void ReplayLayer::OnDetach(entt::registry& registry) {
    (void)registry;
    if (m_said) return;
    m_said = true;
    const Plan& plan = m_driver.Planned();
    std::cout << "[Magic Portals] route " << plan.file << ": ";
    if (m_driver.WonTick() > 0) {
        std::cout << plan.level << " won on tick " << m_driver.WonTick() << " (win_by " << plan.winBy << "), "
                  << m_driver.Shots() << " shot(s)";
    } else if (!m_driver.Problems().empty()) {
        std::cout << plan.level << " FAILED: " << m_driver.Problems().front();
    } else {
        std::cout << plan.level << " not won when the run ended, on tick " << m_game.LayerTicks();
    }
    std::cout << std::endl;
}

} // namespace MagicPortals::Route
