#include "sim/Sounds.hpp"

#include "core/Json.hpp"

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <utility>

namespace MagicPortals::Sounds {

namespace {

namespace Json = Supersonic::Json;

// A key beginning with an underscore is a note to whoever reads the file, not
// data. Nothing below needs one today - every note in sounds.json sits INSIDE
// the entry it explains - but a reader that refuses a comment is a reader that
// makes the next person delete the comment.
bool IsNote(const std::string& key) { return !key.empty() && key[0] == '_'; }

// "random 0.5 to 2.0", which is the only shape the original draws a speed in.
bool ReadRandomSpeed(const std::string& text, Hook& hook) {
    const std::string prefix = "random ";
    const std::string middle = " to ";
    if (text.rfind(prefix, 0) != 0) return false;
    const std::size_t at = text.find(middle, prefix.size());
    if (at == std::string::npos) return false;

    const std::string fromText = text.substr(prefix.size(), at - prefix.size());
    const std::string toText = text.substr(at + middle.size());
    char* end = nullptr;
    const double from = std::strtod(fromText.c_str(), &end);
    if (end == fromText.c_str() || *end != '\0') return false;
    const double to = std::strtod(toText.c_str(), &end);
    if (end == toText.c_str() || *end != '\0') return false;
    if (from <= 0.0 || to < from) return false;

    hook.speedIsRandom = true;
    hook.speedFrom = from;
    hook.speedTo = to;
    return true;
}

bool ReadHook(const std::string& name, const Json::Value& entry, Hook& out, std::string& error) {
    if (!entry.IsObject()) {
        error = name + " is an object";
        return false;
    }
    Hook read;
    read.name = name;

    const Json::Value& files = entry["files"];
    if (!files.IsArray() || files.AsArray().empty() || files.AsArray().size() > 2) {
        error = name + " names one or two files";
        return false;
    }
    for (const Json::Value& file : files.AsArray()) {
        if (!file.IsString() || file.AsString("").empty()) {
            error = name + "'s files are non-empty strings";
            return false;
        }
        read.files.push_back(file.AsString(""));
    }

    // Two files mean one of two things and the file has to say which, because
    // the original does both: playLightOnSound plays them together,
    // playCrystalPickSound picks one.
    if (entry.Has("pick")) {
        const std::string pick = entry["pick"].AsString("");
        if (pick != "both" && pick != "random") {
            error = name + "'s pick is \"both\" or \"random\"";
            return false;
        }
        if (read.files.size() != 2) {
            error = name + " has a pick but does not name two files";
            return false;
        }
        read.both = pick == "both";
    } else if (read.files.size() != 1) {
        error = name + " names two files and does not say whether to play both or one";
        return false;
    }

    // A number the hook sets, or "argument": the one its caller hands it
    // (playFallSound's hitIntensity / 18), which comes with the event.
    if (entry.Has("volume")) {
        const Json::Value& volume = entry["volume"];
        if (volume.IsString() && volume.AsString("") == "argument") {
            read.volumeFromArgument = true;
        } else if (!volume.IsNumber() || volume.AsNumber() <= 0.0 || volume.AsNumber() > 1.0) {
            error = name + "'s volume is above zero and at most one, or \"argument\"";
            return false;
        } else {
            read.volume = volume.AsNumber();
        }
    }

    // Three shapes, because the original has three: a number, a range it draws
    // from, and one computed from the door that is opening.
    if (entry.Has("speed")) {
        const Json::Value& speed = entry["speed"];
        if (speed.IsNumber()) {
            if (speed.AsNumber() <= 0.0) {
                error = name + "'s speed is above zero";
                return false;
            }
            read.speed = speed.AsNumber();
        } else if (speed.IsString()) {
            const std::string text = speed.AsString("");
            if (text == "3000 / doorOpenStride") {
                read.speedFromDoorStride = true;
            } else if (!ReadRandomSpeed(text, read)) {
                error = name + "'s speed is a number, \"random <from> to <to>\" or \"3000 / doorOpenStride\"";
                return false;
            }
        } else {
            error = name + "'s speed is a number or a string";
            return false;
        }
    }

    if (entry.Has("min_interval_ms")) {
        const Json::Value& interval = entry["min_interval_ms"];
        if (!interval.IsNumber() || interval.AsNumber() < 0.0) {
            error = name + "'s min_interval_ms is not negative";
            return false;
        }
        read.minIntervalMs = interval.AsNumber();
    }
    read.timer = entry["timer"].AsString("");
    // A rate limit with no timer would be a limit on nothing in particular. The
    // original's limits are per shared Timer, and which hooks share one is the
    // whole point (the two crystal sounds hold each other off), so the file has
    // to name it.
    if (read.minIntervalMs > 0.0 && read.timer.empty()) {
        error = name + " has a min_interval_ms and no timer to keep it on";
        return false;
    }

    out = std::move(read);
    return true;
}

bool ReadTrack(const std::string& name, const Json::Value& entry, Track& out, std::string& error) {
    if (!entry.IsObject() || !entry["file"].IsString() || entry["file"].AsString("").empty()) {
        error = name + " names a file";
        return false;
    }
    Track read;
    read.file = entry["file"].AsString("");
    if (!entry["volume"].IsNumber() || entry["volume"].AsNumber() <= 0.0 || entry["volume"].AsNumber() > 1.0) {
        error = name + "'s volume is above zero and at most one";
        return false;
    }
    read.volume = entry["volume"].AsNumber();
    if (!entry["loop"].IsBool()) {
        error = name + " says whether it loops";
        return false;
    }
    read.loop = entry["loop"].AsBool();
    out = std::move(read);
    return true;
}

// An event an impact latches: named, and one of `events` - a typo would be a
// contact nobody ever hears.
bool KnownEvent(const std::map<std::string, std::string>& events, const std::string& owner, const char* key,
                const std::string& event, std::string& error) {
    if (event.empty() || events.find(event) == events.end()) {
        error = owner + "'s " + key + " names \"" + event + "\", which is not an event";
        return false;
    }
    return true;
}

bool ReadNames(const Json::Value& entry, const char* key, std::vector<std::string>& out) {
    out.clear();
    if (!entry.Has(key)) return true;
    if (!entry[key].IsArray()) return false;
    for (const Json::Value& name : entry[key].AsArray()) {
        if (!name.IsString() || name.AsString("").empty()) return false;
        out.push_back(name.AsString(""));
    }
    return true;
}

bool ReadImpact(const std::string& name, const Json::Value& entry, const std::map<std::string, std::string>& events,
                Impact& out, std::string& error) {
    const std::string owner = "impacts." + name;
    if (!entry.IsObject()) {
        error = owner + " is an object";
        return false;
    }
    Impact read;
    read.event = entry["event"].AsString("");
    if (!KnownEvent(events, owner, "event", read.event, error)) return false;
    if (!entry["hit_above"].IsNumber() || entry["hit_above"].AsNumber() < 0.0) {
        error = owner + "'s hit_above is a number, not negative";
        return false;
    }
    read.hitAbove = entry["hit_above"].AsNumber();
    if (!entry["volume_divisor"].IsNumber() || !(entry["volume_divisor"].AsNumber() > 0.0)) {
        error = owner + "'s volume_divisor is above zero";
        return false;
    }
    read.volumeDivisor = entry["volume_divisor"].AsNumber();
    read.absolute = entry["absolute"].AsBool(false);
    if (!ReadNames(entry, "spares", read.spares) || !ReadNames(entry, "entities", read.entities)) {
        error = owner + "'s spares and entities are lists of names";
        return false;
    }
    if (entry.Has("character_event")) {
        read.characterEvent = entry["character_event"].AsString("");
        if (!KnownEvent(events, owner, "character_event", read.characterEvent, error)) return false;
    }
    if (entry.Has("destroyier_event")) {
        read.destroyierEvent = entry["destroyier_event"].AsString("");
        if (!KnownEvent(events, owner, "destroyier_event", read.destroyierEvent, error)) return false;
        const Json::Value& volume = entry["destroyier_volume"];
        if (!volume.IsNumber() || volume.AsNumber() <= 0.0 || volume.AsNumber() > 1.0) {
            error = owner + "'s destroyier_volume is above zero and at most one";
            return false;
        }
        read.destroyierVolume = volume.AsNumber();
    }
    out = std::move(read);
    return true;
}

bool ReadCues(const Json::Value& entry, Cues& out, std::string& error) {
    Cues read;
    const Json::Value& margin = entry["beholder_appear_margin_px"];
    if (!margin.IsArray() || margin.AsArray().size() != 2 || !margin.AsArray()[0].IsNumber() ||
        !margin.AsArray()[1].IsNumber() || margin.AsArray()[0].AsNumber() < 0.0 ||
        margin.AsArray()[1].AsNumber() < 0.0) {
        error = "cues.beholder_appear_margin_px is two numbers, not negative";
        return false;
    }
    read.beholderAppearMarginXPx = margin.AsArray()[0].AsNumber();
    read.beholderAppearMarginYPx = margin.AsArray()[1].AsNumber();
    const auto positive = [&entry, &error](const char* key, double& into) {
        if (!entry[key].IsNumber() || !(entry[key].AsNumber() > 0.0)) {
            error = std::string("cues.") + key + " is above zero";
            return false;
        }
        into = entry[key].AsNumber();
        return true;
    };
    if (!positive("dragon_appear_after_ms", read.dragonAppearAfterMs) ||
        !positive("boss_explosion_stride_ms", read.bossExplosionStrideMs) ||
        !positive("light_switch_after_ms", read.lightSwitchAfterMs)) {
        return false;
    }
    out = read;
    return true;
}

} // namespace

bool ImpactVolume(const Impact& rule, double hit, double& volume) {
    const double read = rule.absolute ? std::fabs(hit) : hit;
    if (rule.event.empty() || !(read > rule.hitAbove) || !(rule.volumeDivisor > 0.0)) return false;
    volume = read / rule.volumeDivisor;
    return true;
}

const Hook* Rules::FindHook(const std::string& name) const {
    const auto it = hooks.find(name);
    return it == hooks.end() ? nullptr : &it->second;
}

const Hook* Rules::ForEvent(const std::string& event) const {
    const auto it = events.find(event);
    if (it == events.end() || it->second.empty()) return nullptr;
    return FindHook(it->second);
}

const Track* Rules::FindTrack(const std::string& name) const {
    const auto it = music.find(name);
    return it == music.end() ? nullptr : &it->second;
}

bool LoadRules(const std::string& path, Rules& out, std::string& error) {
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
    if (!root.IsObject() || !root["hooks"].IsObject() || !root["music"].IsObject() ||
        !root["events"].IsObject()) {
        error = path + ": hooks, music and events are each an object";
        return false;
    }

    Rules read;
    std::string why;
    for (const auto& [name, entry] : root["hooks"].AsObject()) {
        if (IsNote(name)) continue;
        Hook hook;
        if (!ReadHook(name, entry, hook, why)) {
            error = path + ": " + why;
            return false;
        }
        read.hooks.emplace(name, std::move(hook));
    }
    if (read.hooks.empty()) {
        error = path + ": hooks names none";
        return false;
    }

    for (const auto& [name, entry] : root["music"].AsObject()) {
        if (IsNote(name)) continue;
        Track track;
        if (!ReadTrack(name, entry, track, why)) {
            error = path + ": " + why;
            return false;
        }
        read.music.emplace(name, std::move(track));
    }

    for (const auto& [event, entry] : root["events"].AsObject()) {
        if (IsNote(event)) continue;
        if (!entry.IsString()) {
            error = path + ": " + event + " names a hook, or \"\" for silence";
            return false;
        }
        const std::string hook = entry.AsString("");
        // An empty name is deliberate silence - the port names what it can see
        // happen even where the original has no sound for it. A name that is
        // not a hook is a typo, and a typo here is a sound nobody ever hears,
        // which is the quietest kind of bug there is. Refused at load.
        if (!hook.empty() && read.hooks.find(hook) == read.hooks.end()) {
            error = path + ": " + event + " names " + hook + ", which is not a hook";
            return false;
        }
        read.events.emplace(event, hook);
    }

    // The contacts' sounds and the moments' numbers (planning step 142), read
    // after the events they name.
    if (root.Has("impacts")) {
        const Json::Value& impacts = root["impacts"];
        const std::pair<const char*, Impact*> rows[] = {{"player", &read.impacts.player},
                                                         {"minion", &read.impacts.minion},
                                                         {"stone", &read.impacts.stone},
                                                         {"crate", &read.impacts.crate}};
        for (const auto& [key, into] : rows) {
            if (!ReadImpact(key, impacts[key], read.events, *into, why)) {
                error = path + ": " + why;
                return false;
            }
        }
        // A crate's sound is its callback's, and a callback is bound by name.
        if (read.impacts.crate.entities.empty() || read.impacts.crate.characterEvent.empty()) {
            error = path + ": impacts.crate names its entities and its character_event";
            return false;
        }
        if (read.impacts.stone.destroyierEvent.empty()) {
            error = path + ": impacts.stone names its destroyier_event";
            return false;
        }
    }
    if (root.Has("cues") && !ReadCues(root["cues"], read.cues, why)) {
        error = path + ": " + why;
        return false;
    }

    out = std::move(read);
    return true;
}

std::string Directory(const std::string& originalDirectory) { return originalDirectory + "/soundfx"; }

} // namespace MagicPortals::Sounds
