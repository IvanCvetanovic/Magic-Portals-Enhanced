#include "sim/Demolish.hpp"

#include "core/Components.hpp"
#include "core/Json.hpp"
#include "sim/Units.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace MagicPortals::Demolish {

namespace {

// The datum ETHBeginContactCallback_breakable_wall #42-60 reads on the stone, and entity_launcher #200-209 sets.
constexpr const char* kDestroyableKey = "destroyable";

// GetUInt('destroyable') == 1 (#52-53: CMPIu against the immediate 1, JNZ skips): exactly 1, which is not "set".
bool IsOne(const Tscn::Node& node, const char* key) {
    const Tscn::Value* value = node.Meta(key);
    if (value == nullptr) return false;
    double number = 0.0;
    if (value->kind == Tscn::Value::Kind::String) {
        char* end = nullptr;
        number = std::strtod(value->text.c_str(), &end);
        return end != value->text.c_str() && *end == '\0' && number == 1.0;
    }
    return value->AsNumber(number) && number == 1.0;
}

// hazards.gd:62-66: a flag is set when it is there and is neither "" nor "0".
bool Flagged(const Tscn::Node& node, const char* key) {
    const Tscn::Value* value = node.Meta(key);
    if (value == nullptr) return false;
    if (value->kind == Tscn::Value::Kind::String) return !value->text.empty() && value->text != "0";
    double number = 0.0;
    return value->AsNumber(number) && number != 0.0;
}

} // namespace

bool LoadRules(const std::string& path, Rules& out, std::string& error) {
    namespace Json = Supersonic::Json;
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
    const auto has = [&](const char* block, const char* key) {
        if (!root.IsObject() || !root.Has(block) || !root[block].IsObject() || !root[block].Has(key)) {
            error = path + ": " + block + "." + key + " is missing";
            return false;
        }
        return true;
    };

    Rules read;
    if (!has("breakable", "names")) return false;
    const Json::Value& names = root["breakable"]["names"];
    if (!names.IsArray()) {
        error = path + ": breakable.names is not an array";
        return false;
    }
    for (const Json::Value& name : names.AsArray()) {
        if (!name.IsString()) {
            error = path + ": breakable.names lists something that is not a string";
            return false;
        }
        read.breakableNames.push_back(name.AsString());
    }
    if (!has("contact", "margin_px")) return false;
    if (!root["contact"]["margin_px"].IsNumber()) {
        error = path + ": contact.margin_px is not a number";
        return false;
    }
    read.contactMarginPx = root["contact"]["margin_px"].AsNumber();
    if (!has("blast", "entities")) return false;
    const Json::Value& entities = root["blast"]["entities"];
    if (!entities.IsArray() || entities.AsArray().empty()) {
        error = path + ": blast.entities is not an array naming at least one entity";
        return false;
    }
    for (const Json::Value& entity : entities.AsArray()) {
        if (!entity.IsString()) {
            error = path + ": blast.entities lists something that is not a string";
            return false;
        }
        read.blastEntities.push_back(entity.AsString());
    }
    if (!has("stone", "kills_player_above")) return false;
    if (!root["stone"]["kills_player_above"].IsNumber() || !(root["stone"]["kills_player_above"].AsNumber() > 0.0)) {
        error = path + ": stone.kills_player_above is not a number above 0";
        return false;
    }
    read.killsPlayerAbove = root["stone"]["kills_player_above"].AsNumber();
    if (!has("wall", "breaks_above_m_s") || !has("wall", "entities")) return false;
    if (!root["wall"]["breaks_above_m_s"].IsNumber() || !(root["wall"]["breaks_above_m_s"].AsNumber() > 0.0)) {
        error = path + ": wall.breaks_above_m_s is not a number above 0";
        return false;
    }
    read.wallBreaksAboveMs = root["wall"]["breaks_above_m_s"].AsNumber();
    const Json::Value& wallEntities = root["wall"]["entities"];
    if (!wallEntities.IsArray() || wallEntities.AsArray().empty()) {
        error = path + ": wall.entities is not an array naming at least one entity";
        return false;
    }
    for (const Json::Value& entity : wallEntities.AsArray()) {
        if (!entity.IsString()) {
            error = path + ": wall.entities lists something that is not a string";
            return false;
        }
        read.wallEntities.push_back(entity.AsString());
    }
    out = std::move(read);
    return true;
}

bool KillsPlayer(const Rules& rules, double hitIntensity) {
    return hitIntensity > rules.killsPlayerAbove;
}

bool State::HitPlayer(const std::string& stone, double hitIntensity) {
    if (playerKilled || !KillsPlayer(rules, hitIntensity)) return false;
    playerKilled = true;
    killedBy = stone;
    return true;
}

std::vector<entt::entity> State::Tick(entt::registry& registry, const Impacts::Watch& watch,
                                      const std::vector<Impacts::Begin>& begins, double simS) {
    contactsNow.clear();
    takenNow.clear();
    const float margin = Units::ToMetres(rules.contactMarginPx);
    using Pair = std::pair<entt::entity, entt::entity>;
    std::vector<Pair> now;            // the wall pairs within reach this tick
    std::vector<entt::entity> doomed; // destroyable stones that broke a wall, taken once after every contact of the step
    for (Breakable& breakable : breakables) {
        if (breakable.broken) continue;
        Trigger::Box reach = breakable.box;
        reach.half += glm::vec2(margin);
        if (!breakable.wall) {
            // Not this callback's: a stone breaks what it touches, at any speed (a carranca: its own row of the backlog).
            for (const Stone& stone : stones) {
                if (!registry.valid(stone.body) || !Trigger::Overlaps(registry, stone.body, reach)) continue;
                breakable.broken = true;
                breakable.brokenBy = stone.name;
                if (registry.valid(breakable.body)) registry.destroy(breakable.body);
                break;
            }
            continue;
        }
        // ETHBeginContactCallback_breakable_wall: every stone that BEGINS to touch the wall on this step is read, so two
        // stones that begin together both go and the wall goes once.
        bool wallBroke = false;
        for (const Stone& stone : stones) {
            if (!registry.valid(stone.body)) continue;
            // The first look: the stone comes within reach of the wall. A pair that stood within reach after the last Tick has
            // had its begin: a slow touch followed by a fast push breaks nothing.
            bool atMargin = false;
            bool braked = false;
            bool within = Trigger::Overlaps(registry, stone.body, reach);
            if (!within && simS > 0.0 && registry.valid(breakable.body)) {
                // Stopped short by the engine's brake: where the stone's incoming approach (the velocity it carried into this step,
                // along the line to the wall) would take it within one step of where the step left it.
                Impacts::Shape stoneShape;
                Impacts::Shape wallShape;
                Impacts::Begin carried;
                if (Impacts::ShapeOf(registry, stone.body, stoneShape) && Impacts::ShapeOf(registry, breakable.body, wallShape) &&
                    watch.Carried(stone.body, carried)) {
                    const Impacts::Gap gap = Impacts::Between(stoneShape, wallShape);
                    const double approachPx = glm::dot(carried.velocityPx, gap.normal) * simS;
                    within = braked = approachPx > 0.0 && gap.px <= rules.contactMarginPx + approachPx;
                }
            }
            if (within) {
                const Pair pair(breakable.body, stone.body);
                now.push_back(pair);
                atMargin = std::find(touching.begin(), touching.end(), pair) == touching.end();
            }
            // The second: the stones' watch finds the pair begun within Box2D's skin, which comes on the same step or a later one.
            const auto skin = std::find_if(begins.begin(), begins.end(), [&](const Impacts::Begin& begin) {
                return begin.body == stone.body && begin.other == breakable.body;
            });
            const bool atSkin = skin != begins.end();
            if (!atMargin && !atSkin) continue;
            Contact contact;
            contact.stone = stone.name;
            contact.wall = breakable.name;
            contact.atMargin = atMargin;
            contact.atSkin = atSkin;
            contact.braked = atMargin && braked;
            if (atMargin) {
                // #16-33: the whole velocity's length, the fastest the stone came along the line to the wall (Impacts::Begin). The
                // watch's velocities are the original's own (px/s over SimRatio): a stone it does not hold reads as at rest.
                glm::dvec2 along(0.0);
                Impacts::Shape stoneShape;
                Impacts::Shape wallShape;
                if (registry.valid(breakable.body) && Impacts::ShapeOf(registry, stone.body, stoneShape) &&
                    Impacts::ShapeOf(registry, breakable.body, wallShape)) {
                    along = Impacts::Between(stoneShape, wallShape).normal;
                }
                Impacts::Begin read;
                if (watch.Carried(stone.body, read)) {
                    contact.historyMs = glm::length(read.HistoryApproaching(along)) / Units::kPixelsPerMetre;
                }
            }
            if (atSkin) {
                contact.replayMs = glm::length(skin->Approaching(skin->normal)) / Units::kPixelsPerMetre;
                contact.freeFlight = skin->freeFlight;
                contact.replayedSteps = skin->replayedSteps;
            }
            contact.speedMs = std::max(contact.historyMs, contact.replayMs);
            if (contact.speedMs > rules.wallBreaksAboveMs) {
                contact.broke = true;
                if (!wallBroke) breakable.brokenBy = stone.name;
                wallBroke = true;
                // #42-60.
                if (stone.destroyable) {
                    contact.stoneDied = true;
                    if (std::find(doomed.begin(), doomed.end(), stone.body) == doomed.end()) doomed.push_back(stone.body);
                }
            } else if (!atMargin) {
                // The second look of a contact the first held, and it holds again: nothing new to say of it.
                continue;
            }
            contactsNow.push_back(std::move(contact));
        }
        if (wallBroke) {
            breakable.broken = true;
            if (registry.valid(breakable.body)) registry.destroy(breakable.body);
        }
    }
    // The pairs that stand within reach after this tick; a wall that has gone has none.
    touching.clear();
    for (const Pair& pair : now) {
        if (registry.valid(pair.first)) touching.push_back(pair);
    }
    std::vector<entt::entity> gone;
    for (const entt::entity body : doomed) {
        const auto found = std::find_if(stones.begin(), stones.end(), [body](const Stone& s) { return s.body == body; });
        if (found == stones.end()) continue;
        Taken taken;
        taken.stone = *found;
        if (const auto* transform = registry.try_get<Supersonic::TransformComponent>(body)) {
            taken.atPx = Units::ToPixels(transform->position);
        }
        takenNow.push_back(std::move(taken));
        stones.erase(found);
        registry.destroy(body);
        gone.push_back(body);
    }
    return gone;
}

entt::entity State::Break(entt::registry& registry, const std::string& name, const std::string& by) {
    for (Breakable& breakable : breakables) {
        if (breakable.name != name || breakable.broken) continue;
        breakable.broken = true;
        breakable.brokenBy = by;
        const entt::entity gone = breakable.body;
        breakable.body = entt::null;
        if (!registry.valid(gone)) return entt::null;
        registry.destroy(gone);
        return gone;
    }
    return entt::null;
}

void State::Restore(const std::string& name, entt::entity body) {
    for (Breakable& breakable : breakables) {
        if (breakable.name != name) continue;
        breakable.broken = false;
        breakable.brokenBy.clear();
        breakable.destroy = false;
        breakable.destroyBy.clear();
        breakable.body = body;
        return;
    }
}

bool State::Grab(entt::entity body, const std::string& by) {
    if (body == entt::null) return false;
    for (Breakable& breakable : breakables) {
        if (breakable.body != body || breakable.broken || !breakable.blastBreaks) continue;
        // The first blast to reach it names it; a second on the same tick sets the
        // same datum again.
        if (!breakable.destroy) breakable.destroyBy = by;
        breakable.destroy = true;
        return true;
    }
    return false;
}

std::string State::SeekAndDestroy(const std::string& entity, const std::string& by) {
    for (Breakable& breakable : breakables) {
        if (breakable.entity != entity || breakable.broken) continue;
        if (!breakable.destroy) breakable.destroyBy = by;
        breakable.destroy = true;
        return breakable.name;
    }
    return std::string();
}

std::vector<entt::entity> State::Destroyed(entt::registry& registry) {
    std::vector<entt::entity> gone;
    for (Breakable& breakable : breakables) {
        if (!breakable.destroy || breakable.broken) continue;
        breakable.destroy = false;
        breakable.broken = true;
        breakable.brokenBy = breakable.destroyBy;
        const entt::entity body = breakable.body;
        breakable.body = entt::null;
        if (!registry.valid(body)) continue;
        registry.destroy(body);
        gone.push_back(body);
    }
    return gone;
}

const Stone* State::FindStone(const std::string& name) const {
    for (const Stone& stone : stones) {
        if (stone.name == name) return &stone;
    }
    // One the wall's callback destroyed on this very tick: the tick's latches still ask who broke what.
    for (const Taken& taken : takenNow) {
        if (taken.stone.name == name) return &taken.stone;
    }
    return nullptr;
}

const Breakable* State::FindBreakable(const std::string& name) const {
    for (const Breakable& breakable : breakables) {
        if (breakable.name == name) return &breakable;
    }
    return nullptr;
}

int State::Broken() const {
    return static_cast<int>(
        std::count_if(breakables.begin(), breakables.end(), [](const Breakable& b) { return b.broken; }));
}

bool Find(const Tscn::Scene& scene, const Roles::Table& roles, const LevelBuilder::Built& built, const Rules& rules,
          State& out, std::string& error) {
    out = State{};
    out.rules = rules;
    for (const Tscn::Node& node : scene.nodes) {
        if (node.parent != ".") continue;
        const auto found = built.entities.find(node.name);

        if (Roles::RoleOf(roles, node) == Roles::kDemolisher) {
            if (found == built.entities.end()) {
                error = node.name + " is a rolling stone with no body";
                return false;
            }
            out.stones.push_back(Stone{node.name, found->second, IsOne(node, kDestroyableKey)});
            continue;
        }

        const bool flagged = Flagged(node, "breakable");
        const std::string entityName = Roles::EntityName(node);
        const bool byName = std::find(rules.breakableNames.begin(), rules.breakableNames.end(), entityName) !=
                            rules.breakableNames.end();
        if ((!flagged && !byName) || found == built.entities.end()) continue;

        glm::dvec2 offsetPx(0.0);
        glm::dvec2 sizePx(0.0);
        if (!LevelBuilder::ShapeBoundsPx(scene, node, offsetPx, sizePx)) {
            error = node.name + " is breakable, and its shape has no bounds";
            return false;
        }
        // Quarter turns, and nothing between: a turned box is not its shape.
        double radians = 0.0;
        if (const Tscn::Value* rotation = node.Find("rotation")) rotation->AsNumber(radians);
        const double quarters = radians / (3.14159265358979323846 * 0.5);
        const double nearest = std::round(quarters);
        if (std::fabs(quarters - nearest) > 1e-3) {
            error = node.name + " is breakable and turned by " + std::to_string(radians) +
                    " radians, and only quarter turns are ported";
            return false;
        }
        // Turned in the port's pixels, +y down: a quarter turn takes (x, y) to
        // (-y, x).
        const int turns = ((static_cast<int>(nearest) % 4) + 4) % 4;
        for (int i = 0; i < turns; ++i) {
            offsetPx = glm::dvec2(-offsetPx.y, offsetPx.x);
            sizePx = glm::dvec2(sizePx.y, sizePx.x);
        }
        glm::dvec2 atPx(0.0);
        if (const Tscn::Value* position = node.Find("position");
            position != nullptr && position->kind == Tscn::Value::Kind::Vector2) {
            atPx = glm::dvec2(position->numbers[0], position->numbers[1]);
        }

        Breakable breakable;
        breakable.name = node.name;
        breakable.entity = entityName;
        breakable.body = found->second;
        breakable.flagged = flagged;
        breakable.wall =
            std::find(rules.wallEntities.begin(), rules.wallEntities.end(), entityName) != rules.wallEntities.end();
        // isBreakable reads the flag and nothing else, so a wall that breaks by
        // name alone is never in a blast's grab.
        breakable.blastBreaks = flagged && std::find(rules.blastEntities.begin(), rules.blastEntities.end(),
                                                     entityName) != rules.blastEntities.end();
        const glm::vec3 centre = Units::ToWorld(atPx.x + offsetPx.x, atPx.y + offsetPx.y);
        breakable.box.centre = glm::vec2(centre.x, centre.y);
        breakable.box.half = glm::vec2(Units::ToMetres(sizePx.x * 0.5), Units::ToMetres(sizePx.y * 0.5));
        out.breakables.push_back(std::move(breakable));
    }
    return true;
}

} // namespace MagicPortals::Demolish
