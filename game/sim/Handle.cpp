#include "sim/Handle.hpp"

#include "core/Json.hpp"
#include "sim/Roles.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <limits>
#include <sstream>

namespace MagicPortals::Handle {

namespace Json = Supersonic::Json;

namespace {

// The callback an entity runs is ETHCallback_ + its name less its .ent
// (ETHASUtil.cpp:63-67), so both spellings of a placement run it.
std::string Bare(const std::string& entity) {
    constexpr const char* kExtension = ".ent";
    const std::size_t length = std::char_traits<char>::length(kExtension);
    if (entity.size() > length && entity.compare(entity.size() - length, length, kExtension) == 0) {
        return entity.substr(0, entity.size() - length);
    }
    return entity;
}

bool PositionOf(const Tscn::Node& node, glm::dvec2& out) {
    const Tscn::Value* position = node.Find("position");
    if (position == nullptr || position->kind != Tscn::Value::Kind::Vector2) return false;
    out = glm::dvec2(position->numbers[0], position->numbers[1]);
    return true;
}

bool Pair(const Json::Value& value, glm::dvec2& out) {
    if (!value.IsArray()) return false;
    const Json::Array& items = value.AsArray();
    if (items.size() != 2 || !items[0].IsNumber() || !items[1].IsNumber()) return false;
    out = glm::dvec2(items[0].AsNumber(), items[1].AsNumber());
    return true;
}

// The handle a name finds: by that exact name, the nearest to `fromPx` - doFollow's
// and the lesson's seekNeighbourEntity, then SeekEntity. Every level places one.
int Seek(const std::vector<Handle>& handles, const std::string& name, const glm::dvec2& fromPx) {
    int found = -1;
    double best = std::numeric_limits<double>::infinity();
    for (std::size_t i = 0; i < handles.size(); ++i) {
        if (handles[i].entityName != name) continue;
        const glm::dvec2 d = handles[i].atPx - fromPx;
        const double distance = d.x * d.x + d.y * d.y;
        if (distance < best) {
            best = distance;
            found = static_cast<int>(i);
        }
    }
    return found;
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
    const char* const blocks[] = {"handle", "follow", "lesson"};
    for (const char* block : blocks) {
        if (!root.IsObject() || !root.Has(block) || !root[block].IsObject()) {
            error = path + ": the " + std::string(block) + " block is missing";
            return false;
        }
    }
    const Json::Value& handle = root["handle"];
    const Json::Value& follow = root["follow"];
    const Json::Value& lesson = root["lesson"];
    const auto fail = [&](const std::string& why) {
        error = path + ": " + why;
        return false;
    };
    const auto name = [&](const Json::Value& block, const char* where, const char* key, std::string& into) {
        if (!block.Has(key) || !block[key].IsString() || block[key].AsString().empty()) {
            return fail(std::string(where) + "." + key + " is missing or not a name");
        }
        into = block[key].AsString();
        return true;
    };
    const auto number = [&](const Json::Value& block, const char* where, const char* key, double& into) {
        if (!block.Has(key) || !block[key].IsNumber()) {
            return fail(std::string(where) + "." + key + " is missing or not a number");
        }
        into = block[key].AsNumber();
        return true;
    };

    Rules read;
    if (!name(handle, "handle", "entity", read.entity) ||
        !number(handle, "handle", "max_frame_move_units", read.maxFrameMoveUnits) ||
        !number(handle, "handle", "idle_alpha", read.idleAlpha) ||
        !number(handle, "handle", "moving_alpha", read.movingAlpha) ||
        !number(handle, "handle", "bounce_stride_ms", read.bounce.strideMs)) {
        return false;
    }
    if (!Pair(handle["size_units"], read.sizeUnits) || !Pair(handle["bounce_from"], read.bounce.fromScale) ||
        !Pair(handle["bounce_to"], read.bounce.toScale)) {
        return fail("handle.size_units, bounce_from and bounce_to are each two numbers");
    }
    // A handle of no size is never touched, and one that moves nothing a frame is
    // never dragged: each would look like a level that works.
    if (!(read.sizeUnits.x > 0.0 && read.sizeUnits.y > 0.0) || !(read.maxFrameMoveUnits > 0.0) ||
        !(read.bounce.strideMs > 0.0)) {
        return fail("handle.size_units, max_frame_move_units and bounce_stride_ms are above zero");
    }
    if (read.idleAlpha < 0.0 || read.idleAlpha > 1.0 || read.movingAlpha < 0.0 || read.movingAlpha > 1.0) {
        return fail("handle.idle_alpha and moving_alpha are each 0 to 1");
    }

    double strideMs = 0.0;
    double rateMs = 0.0;
    if (!name(follow, "follow", "datum", read.followDatum) ||
        !number(follow, "follow", "interp_stride_ms", strideMs) ||
        !number(follow, "follow", "update_rate_ms", rateMs)) {
        return false;
    }
    if (!follow.Has("dont_pause") || !follow["dont_pause"].IsBool()) return fail("follow.dont_pause is true or false");
    if (!(strideMs > 0.0) || rateMs < 0.0) return fail("follow.interp_stride_ms is above zero, update_rate_ms not below");
    read.follow.strideMs = strideMs;
    read.follow.rateMs = rateMs;
    read.follow.dontPause = follow["dont_pause"].AsBool();
    if (!follow.Has("callers") || !follow["callers"].IsArray() || follow["callers"].AsArray().empty()) {
        return fail("follow.callers names at least one entity");
    }
    for (const Json::Value& caller : follow["callers"].AsArray()) {
        if (!caller.IsString() || caller.AsString().empty()) return fail("follow.callers lists a name that is not one");
        read.followers.push_back(caller.AsString());
    }

    if (!name(lesson, "lesson", "hand_entity", read.lessonEntity) ||
        !name(lesson, "lesson", "handle_name", read.lessonHandleName)) {
        return false;
    }
    if (!lesson.Has("hand_waypoints") ||
        !Hint::ParseWaypoints(lesson["hand_waypoints"], true, path + ": lesson.hand_waypoints", read.lessonWaypoints,
                              error)) {
        if (error.empty()) error = path + ": lesson.hand_waypoints is missing";
        return false;
    }
    out = std::move(read);
    return true;
}

bool InRect(const glm::dvec2& atPx, const glm::dvec2& centrePx, const glm::dvec2& sizeUnits) {
    // isPointInRect (319410..319788): posRelative = pos - size x origin; outside when
    // p.x < left, p.x > left + size.x, p.y < top or p.y > top + size.y - so both
    // edges are in.
    const glm::dvec2 corner = centrePx - sizeUnits * 0.5;
    return !(atPx.x < corner.x || atPx.x > corner.x + sizeUnits.x || atPx.y < corner.y ||
             atPx.y > corner.y + sizeUnits.y);
}

glm::dvec2 Cap(const glm::dvec2& v, double length) {
    // capVector (317037..317257): dp2(v, v) > length^2 -> normalize(v) x length.
    const double squared = v.x * v.x + v.y * v.y;
    if (squared > length * length) return v / std::sqrt(squared) * length;
    return v;
}

glm::dvec2 State::Scale(const Handle& handle) const {
    // Before its first bounce() the entity is at its own scale.
    return handle.bounced ? rules.bounce.ScaleAt(handle.bounceMs) : glm::dvec2(1.0);
}

glm::dvec2 State::SizeUnits(const Handle& handle) const { return rules.sizeUnits * Scale(handle); }

void State::Drag(const Touch& touch, double frameMs) {
    for (Handle& handle : handles) {
        // #4-16: 'moving' 0 at the start of every call.
        handle.moving = false;
        // GetSize() as the last call's bounce left the entity's scale.
        const glm::dvec2 size = SizeUnits(handle);
        // #21-86: a touch that goes down on it remembers where the handle stood.
        if (touch.hit && InRect(touch.atPx, handle.atPx, size)) handle.onHitPx = handle.atPx;
        // #89-173: held, and gone down on it - the point of the level it went down
        // on, in the rect of where the handle stood then - moves it by the finger's
        // move, capped, and it is moving, whether or not the finger moved.
        if (touch.down && InRect(touch.hitPx, handle.onHitPx, size)) {
            const glm::dvec2 move = Cap(touch.moveUnits, rules.maxFrameMoveUnits);
            handle.atPx += move;
            handle.moving = true;
            if (move != glm::dvec2(0.0)) ++drags;
        }
        // #201-220: bounce's clock, on the game's frame time.
        handle.bounceMs += frameMs;
        handle.bounced = true;
    }
}

void State::Rest(double frameMs) {
    for (Handle& handle : handles) {
        handle.moving = false;
        if (frameMs > 0.0) {
            handle.bounceMs += frameMs;
            handle.bounced = true;
        }
    }
}

bool State::AnyMoving() const {
    return std::any_of(handles.begin(), handles.end(), [](const Handle& handle) { return handle.moving; });
}

void State::Follow(double frameMs) {
    for (Follower& follower : followers) {
        if (follower.handle < 0 || follower.handle >= static_cast<int>(handles.size())) continue;
        follower.atPx = follower.follow.Tick(rules.follow, follower.atPx,
                                             handles[static_cast<std::size_t>(follower.handle)].atPx, frameMs);
    }
}

const Handle* State::FindHandle(const std::string& node) const {
    for (const Handle& handle : handles) {
        if (handle.node == node) return &handle;
    }
    return nullptr;
}

const Follower* State::FindFollower(const std::string& node) const {
    for (const Follower& follower : followers) {
        if (follower.node == node) return &follower;
    }
    return nullptr;
}

void Lesson::Tick(const Rules& rules, bool handleMoving, double frameMs) {
    vanishedNow = false;
    if (!placed || gone) return;
    // #277-328: the round, then the point and the alpha it gives (AtPx, Alpha).
    Hint::Round::Tick(rules.lessonWaypoints, frameMs);
    ticked = true;
    // #330-441: the handle it finds held - smoke_vanish above it, and it is deleted.
    if (handleMoving) {
        gone = true;
        vanishedNow = true;
    }
}

bool Find(const Tscn::Scene& scene, const Rules& rules, State& out, std::string& error) {
    out = State{};
    out.rules = rules;
    for (const Tscn::Node& node : scene.nodes) {
        if (node.parent != ".") continue;
        const std::string entity = Roles::EntityName(node);
        if (Bare(entity) != rules.entity) continue;
        Handle handle;
        handle.node = node.name;
        handle.entityName = entity;
        if (!PositionOf(node, handle.startPx)) {
            error = node.name + " is a handle with no position";
            return false;
        }
        handle.atPx = handle.startPx;
        out.handles.push_back(handle);
    }
    for (const Tscn::Node& node : scene.nodes) {
        if (node.parent != ".") continue;
        const std::string bare = Bare(Roles::EntityName(node));
        const auto caller = std::find(rules.followers.begin(), rules.followers.end(), bare);
        if (caller == rules.followers.end()) continue;
        // #1-32: nothing when the datum is '' - GetString of a name never set is too.
        const Tscn::Value* datum = node.Meta(rules.followDatum.c_str());
        if (datum == nullptr || datum->kind != Tscn::Value::Kind::String || datum->text.empty()) continue;
        Follower follower;
        follower.node = node.name;
        follower.follows = datum->text;
        if (!PositionOf(node, follower.startPx)) {
            error = node.name + " follows a handle and has no position";
            return false;
        }
        follower.atPx = follower.startPx;
        follower.handle = Seek(out.handles, follower.follows, follower.startPx);
        out.followers.push_back(follower);
    }
    return true;
}

} // namespace MagicPortals::Handle
