#include "sim/Achievements.hpp"

#include "core/Json.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>
#include <utility>

namespace MagicPortals::Achievements {

namespace {

namespace Json = Supersonic::Json;

bool Integer(const Json::Value& object, const char* key, int& out) {
    const Json::Value& value = object[key];
    if (!value.IsNumber() || value.AsNumber() != std::floor(value.AsNumber()) || std::fabs(value.AsNumber()) > 1.0e6) {
        return false;
    }
    out = static_cast<int>(value.AsNumber());
    return true;
}

bool String(const Json::Value& object, const char* key, std::string& out) {
    const Json::Value& value = object[key];
    if (!value.IsString()) return false;
    out = value.AsString();
    return true;
}

bool Bool(const Json::Value& object, const char* key, bool& out) {
    const Json::Value& value = object[key];
    if (!value.IsBool()) return false;
    out = value.AsBool();
    return true;
}

} // namespace

bool Load(const std::string& path, Content& out, std::string& error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = path + ": cannot open";
        return false;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    // Named, not a temporary: the parser keeps a reference to what it reads.
    const std::string text = buffer.str();
    Json::Parser parser(text);
    Json::Value root;
    if (!parser.Parse(root)) {
        error = path + ": " + parser.Error();
        return false;
    }
    if (!root.IsObject() || !root["format"].IsNumber() || root["format"].AsNumber() != 1.0 ||
        !root["achievements"].IsArray() || !root["secret"].IsObject() || !root["unlock"].IsObject() ||
        !root["unlock"]["from_completion"].IsArray() || !root["unlock"]["from_level_medal"].IsArray()) {
        error = path + ": not tools/asbc/achievements.py's format 1 (achievements, secret, unlock)";
        return false;
    }

    Content read;
    for (const Json::Value& item : root["achievements"].AsArray()) {
        Entry entry;
        if (!item.IsObject() || !Integer(item, "id", entry.id) || !Integer(item, "world", entry.world) ||
            !Integer(item, "level", entry.level) || !Integer(item, "points", entry.points) ||
            !String(item, "title", entry.title) || !String(item, "description", entry.description) ||
            !String(item, "icon", entry.icon) || !Bool(item, "is_new", entry.isNew) ||
            !Bool(item, "is_secret", entry.isSecret) || entry.world < 0 || entry.level < -1) {
            error = path + ": achievement " + std::to_string(read.entries.size()) +
                    " lacks an id, a world, a level of -1 or more, points, a title, a description, an icon, "
                    "is_new or is_secret";
            return false;
        }
        read.entries.push_back(std::move(entry));
    }

    const Json::Value& secret = root["secret"];
    if (!String(secret, "title", read.secret.title) ||
        !String(secret, "description_prefix", read.secret.descriptionPrefix) ||
        !String(secret, "description_separator", read.secret.descriptionSeparator) ||
        !String(secret, "icon", read.secret.icon)) {
        error = path + ": secret lacks a title, description_prefix, description_separator or icon";
        return false;
    }

    for (const Json::Value& item : root["unlock"]["from_completion"].AsArray()) {
        CompletionRule rule;
        if (!item.IsObject() || !Integer(item, "world", rule.world) || !Integer(item, "min_percent", rule.minPercent) ||
            !Integer(item, "id", rule.id)) {
            error = path + ": a from_completion rule lacks a world, a min_percent or an id";
            return false;
        }
        read.fromCompletion.push_back(rule);
    }
    for (const Json::Value& item : root["unlock"]["from_level_medal"].AsArray()) {
        MedalRule rule;
        if (!item.IsObject() || !Integer(item, "world", rule.world) || !Integer(item, "level", rule.level) ||
            !Integer(item, "min_medal", rule.minMedal) || !Integer(item, "id", rule.id)) {
            error = path + ": a from_level_medal rule lacks a world, a level, a min_medal or an id";
            return false;
        }
        read.fromLevelMedal.push_back(rule);
    }
    out = std::move(read);
    return true;
}

std::vector<int> Unlocked(const Content& content, const Locking::Rules& locking, const Scores::Store& scores,
                          const Chapters::Table& chapters) {
    std::vector<int> ids;
    for (const CompletionRule& rule : content.fromCompletion) {
        if (Locking::ChapterCompletion(locking, scores, chapters, rule.world) >= rule.minPercent) ids.push_back(rule.id);
    }
    for (const MedalRule& rule : content.fromLevelMedal) {
        if (scores.Get(rule.world, rule.level) >= rule.minMedal) ids.push_back(rule.id);
    }
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    return ids;
}

bool IsUnlocked(const std::vector<int>& unlocked, int id) {
    return std::binary_search(unlocked.begin(), unlocked.end(), id);
}

int Points(const Content& content, const std::vector<int>& unlocked) {
    int points = 0;
    for (const Entry& entry : content.entries) {
        if (IsUnlocked(unlocked, entry.id)) points += entry.points;
    }
    return points;
}

std::string TitleOf(const Content& content, const Entry& entry, bool unlocked) {
    return !entry.isSecret || unlocked ? entry.title : content.secret.title;
}

std::string DescriptionOf(const Content& content, const Entry& entry, bool unlocked) {
    if (!entry.isSecret || unlocked) return entry.description;
    return content.secret.descriptionPrefix + std::to_string(entry.world + 1) + content.secret.descriptionSeparator +
           std::to_string(entry.level + 1);
}

std::string IconOf(const Content& content, const Entry& entry, bool unlocked) {
    return !entry.isSecret || unlocked ? entry.icon : content.secret.icon;
}

const Entry* Content::Find(int id) const {
    for (const Entry& entry : entries) {
        if (entry.id == id) return &entry;
    }
    return nullptr;
}

std::vector<const Entry*> FromLevel(const Content& content, int world, int level) {
    std::vector<const Entry*> found;
    // #6-20: a negative level or world has none.
    if (world < 0 || level < 0) return found;
    for (const Entry& entry : content.entries) {
        if (entry.world == world && entry.level == level) found.push_back(&entry);
    }
    return found;
}

// ---- the rules ------------------------------------------------------------------

namespace {

bool Number(const Json::Value& object, const char* key, double& out) {
    const Json::Value& value = object[key];
    if (!value.IsNumber()) return false;
    out = value.AsNumber();
    return true;
}

bool Text(const Json::Value& object, const char* key, std::string& out) {
    const Json::Value& value = object[key];
    if (!value.IsString() || value.AsString().empty()) return false;
    out = value.AsString();
    return true;
}

bool Names(const Json::Value& value, std::vector<std::string>& out) {
    if (!value.IsArray() || value.AsArray().empty()) return false;
    for (const Json::Value& name : value.AsArray()) {
        if (!name.IsString() || name.AsString().empty()) return false;
        out.push_back(name.AsString());
    }
    return true;
}

// One term: an object whose one key names its kind (a count's second key its bound).
bool ReadTerm(const Json::Value& item, Term& out) {
    if (!item.IsObject()) return false;
    const auto number = [&item](const char* key, double& value) {
        return item.Has(key) && Number(item, key, value);
    };
    if (number("medal_at_least", out.value)) {
        out.kind = TermKind::MedalAtLeast;
    } else if (number("medal_above", out.value)) {
        out.kind = TermKind::MedalAbove;
    } else if (number("portals_at_most", out.value)) {
        out.kind = TermKind::PortalsAtMost;
    } else if (number("portals_exactly", out.value)) {
        out.kind = TermKind::PortalsExactly;
    } else if (item.Has("game")) {
        if (!item["game"].IsBool() || !item["game"].AsBool()) return false;
        out.kind = TermKind::Game;
    } else if (number("time_at_most_ms", out.value)) {
        out.kind = TermKind::TimeAtMostMs;
    } else if (item.Has("count")) {
        if (!Names(item["count"], out.names)) return false;
        if (number("at_least", out.value)) {
            out.kind = TermKind::CountAtLeast;
        } else if (number("exactly", out.value)) {
            out.kind = TermKind::CountExactly;
        } else {
            return false;
        }
    } else if (item.Has("exists") || item.Has("absent")) {
        const bool exists = item.Has("exists");
        std::string name;
        if (!Text(item, exists ? "exists" : "absent", name)) return false;
        out.kind = exists ? TermKind::Exists : TermKind::Absent;
        out.names = {name};
    } else if (item.Has("lift_doors_shut")) {
        if (!Names(item["lift_doors_shut"], out.names)) return false;
        out.kind = TermKind::LiftDoorsShut;
    } else if (number("unnoticed", out.value)) {
        out.kind = TermKind::Unnoticed;
    } else if (number("laps_at_least", out.value)) {
        out.kind = TermKind::LapsAtLeast;
    } else {
        return false;
    }
    return true;
}

bool NeedsWorld(TermKind kind) {
    switch (kind) {
    case TermKind::CountAtLeast:
    case TermKind::CountExactly:
    case TermKind::Exists:
    case TermKind::Absent:
    case TermKind::LiftDoorsShut:
    case TermKind::Unnoticed:
    case TermKind::LapsAtLeast:
        return true;
    case TermKind::MedalAtLeast:
    case TermKind::MedalAbove:
    case TermKind::PortalsAtMost:
    case TermKind::PortalsExactly:
    case TermKind::Game:
    case TermKind::TimeAtMostMs:
        return false;
    }
    return true;
}

// The entity hasBeenUnnoticed counts (401465..401641 #4-11).
constexpr const char* kMinionEntity = "minion.ent";

bool Holds(const Term& term, const Play& play, const World* world) {
    if (NeedsWorld(term.kind) && world == nullptr) return false;
    const auto whole = [&term]() { return static_cast<long long>(term.value); };
    switch (term.kind) {
    case TermKind::MedalAtLeast:
        return play.medal >= whole();
    case TermKind::MedalAbove:
        return play.medal > whole();
    case TermKind::PortalsAtMost:
        return play.portals <= whole();
    case TermKind::PortalsExactly:
        return play.portals == whole();
    case TermKind::Game:
        return play.game;
    case TermKind::TimeAtMostMs:
        return play.gameTimeMs <= term.value;
    case TermKind::CountAtLeast:
        return world->Count(term.names) >= whole();
    case TermKind::CountExactly:
        return world->Count(term.names) == whole();
    case TermKind::Exists:
        return world->Exists(term.names.front());
    case TermKind::Absent:
        return !world->Exists(term.names.front());
    case TermKind::LiftDoorsShut:
        return world->LiftDoorsShut(term.names);
    case TermKind::Unnoticed:
        // #13-17 exactly so many minions, and #20-25 spottedCount() == 0.
        return world->Count({kMinionEntity}) == whole() && world->SpottedCount() == 0;
    case TermKind::LapsAtLeast:
        return world->Laps() >= whole();
    }
    return false;
}

} // namespace

std::vector<int> Rules::AllIds() const {
    std::vector<int> ids;
    for (const std::vector<CompletionStep>& steps : completion) {
        for (const CompletionStep& step : steps) ids.push_back(step.id);
    }
    for (const LevelRule& rule : level) ids.push_back(rule.id);
    ids.push_back(key.id);
    for (const HelpBlock& block : helpBlocks) ids.push_back(block.id);
    return ids;
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

    Rules read;
    const Json::Value& save = root["save"];
    double offset = 0.0, subtract = 0.0, multiply = 0.0, add = 0.0;
    if (!Text(save, "file", read.save.file) || !Text(save, "entity", read.save.entity) ||
        !Text(save, "list_key", read.save.listKey) || !Text(save, "hash_key", read.save.hashKey) ||
        !Number(save, "hash_first_multiplier_offset", offset) || !Number(save, "hash_subtract", subtract) ||
        !Number(save, "hash_multiply", multiply) || !Number(save, "hash_add", add)) {
        error = path + ": save lacks a file, entity, list_key, hash_key or one of the hash's four numbers";
        return false;
    }
    read.save.firstMultiplierOffset = static_cast<long long>(offset);
    read.save.subtract = static_cast<long long>(subtract);
    read.save.multiply = static_cast<long long>(multiply);
    read.save.add = static_cast<long long>(add);

    const Json::Value& completion = root["completion"];
    if (!Number(completion, "delay_ms", read.completionDelayMs) || !completion["worlds"].IsArray()) {
        error = path + ": completion lacks delay_ms or worlds";
        return false;
    }
    for (const Json::Value& item : completion["worlds"].AsArray()) {
        int world = -1;
        if (!item.IsObject() || !Integer(item, "world", world) || world < 0 || world > 64 || !item["steps"].IsArray()) {
            error = path + ": a completion world lacks a world or steps";
            return false;
        }
        if (read.completion.size() <= static_cast<std::size_t>(world)) {
            read.completion.resize(static_cast<std::size_t>(world) + 1);
        }
        for (const Json::Value& step : item["steps"].AsArray()) {
            CompletionStep made;
            if (!step.IsObject() || !Integer(step, "min_percent", made.minPercent) || !Integer(step, "id", made.id)) {
                error = path + ": a completion step lacks min_percent or id";
                return false;
            }
            read.completion[static_cast<std::size_t>(world)].push_back(made);
        }
    }

    const Json::Value& level = root["level"];
    if (!Number(level, "delay_ms", read.levelDelayMs) || !level["rules"].IsArray()) {
        error = path + ": level lacks delay_ms or rules";
        return false;
    }
    for (const Json::Value& item : level["rules"].AsArray()) {
        LevelRule rule;
        if (!item.IsObject() || !Integer(item, "id", rule.id) || !Integer(item, "world", rule.world) ||
            !Integer(item, "level", rule.level) || !item["terms"].IsArray() || item["terms"].AsArray().empty()) {
            error = path + ": level rule " + std::to_string(read.level.size()) + " lacks an id, world, level or terms";
            return false;
        }
        for (const Json::Value& term : item["terms"].AsArray()) {
            Term made;
            if (!ReadTerm(term, made)) {
                error = path + ": achievement " + std::to_string(rule.id) + " has a term this does not read";
                return false;
            }
            rule.terms.push_back(std::move(made));
        }
        read.level.push_back(std::move(rule));
    }

    const Json::Value& key = root["key"];
    if (!Text(key, "colour", read.key.colour) || !Integer(key, "id", read.key.id) ||
        !Number(key, "delay_ms", read.key.delayMs)) {
        error = path + ": key lacks a colour, id or delay_ms";
        return false;
    }
    const Json::Value& help = root["help_block"];
    if (!Number(help, "delay_ms", read.helpBlockDelayMs) || !help["levels"].IsArray()) {
        error = path + ": help_block lacks delay_ms or levels";
        return false;
    }
    for (const Json::Value& item : help["levels"].AsArray()) {
        Rules::HelpBlock block;
        if (!item.IsObject() || !Integer(item, "world", block.world) || !Integer(item, "level", block.level) ||
            !Integer(item, "id", block.id)) {
            error = path + ": a help block lacks a world, level or id";
            return false;
        }
        read.helpBlocks.push_back(block);
    }
    const Json::Value& menu = root["menu_pass"];
    if (!Integer(menu, "num_portals", read.menuPortals) || !Number(menu, "game_time_ms", read.menuGameTimeMs)) {
        error = path + ": menu_pass lacks num_portals or game_time_ms";
        return false;
    }

    if (!Text(root["level_world"], "signal_torch", read.signalTorch)) {
        error = path + ": level_world lacks signal_torch";
        return false;
    }

    const Json::Value& card = root["card"];
    if (!Text(card, "background", read.card.background) || !Text(card, "message_prefix", read.card.messagePrefix) ||
        !Text(card, "icon_directory", read.card.iconDirectory) || !Number(card, "duration_ms", read.card.durationMs) ||
        !Text(card, "sound_event", read.card.soundEvent)) {
        error = path + ": card lacks a background, message_prefix, icon_directory, duration_ms or sound_event";
        return false;
    }
    const Json::Value& locked = root["locked_card"];
    if (!Text(locked, "background", read.locked.background) || !Text(locked, "message", read.locked.message) ||
        !Number(locked, "duration_ms", read.locked.durationMs) || !Number(locked, "delay_ms", read.locked.delayMs)) {
        error = path + ": locked_card lacks a background, message, duration_ms or delay_ms";
        return false;
    }
    const Json::Value& popup = root["popup"];
    const Json::Value& close = popup["close_button"];
    const auto pair = [](const Json::Value& value, double& x, double& y) {
        if (!value.IsArray() || value.AsArray().size() != 2 || !value.AsArray()[0].IsNumber() ||
            !value.AsArray()[1].IsNumber()) {
            return false;
        }
        x = value.AsArray()[0].AsNumber();
        y = value.AsArray()[1].AsNumber();
        return true;
    };
    if (!Text(popup, "tiles", read.popup.tiles) ||
        !pair(popup["tile_units"], read.popup.tileUnitsX, read.popup.tileUnitsY) ||
        !Integer(popup, "tile_columns", read.popup.tileColumns) || !Integer(popup, "tile_rows", read.popup.tileRows) ||
        !Text(popup, "font", read.popup.font) || !Number(popup, "units_per_font_px", read.popup.unitsPerFontPx) ||
        !Text(popup, "unlocked_mark", read.popup.unlockedMark) || !Text(popup, "locked_mark", read.popup.lockedMark) ||
        !Text(close, "sprite", read.popup.closeSprite) ||
        !pair(close["at_screen"], read.popup.closeAtX, read.popup.closeAtY) ||
        !pair(close["origin"], read.popup.closeOriginX, read.popup.closeOriginY) ||
        !pair(close["size_units"], read.popup.closeUnitsX, read.popup.closeUnitsY) || read.popup.tileColumns <= 0 ||
        read.popup.tileRows <= 0) {
        error = path + ": popup lacks its tiles, their size and cut, a font, its scale, the two marks or its close button";
        return false;
    }

    // Each id once: the original names every one in exactly one place but the
    // two that a level dispatches twice in no order that matters - none here.
    std::vector<int> ids = read.AllIds();
    std::sort(ids.begin(), ids.end());
    if (std::adjacent_find(ids.begin(), ids.end()) != ids.end()) {
        error = path + ": an achievement id is named by two rules";
        return false;
    }
    out = std::move(read);
    return true;
}

std::vector<int> Dispatches(const Rules& rules, const Play& play, const World* world) {
    std::vector<int> ids;
    for (const LevelRule& rule : rules.level) {
        if (rule.world != play.world || rule.level != play.level) continue;
        bool all = true;
        for (const Term& term : rule.terms) {
            if (!Holds(term, play, world)) {
                all = false;
                break;
            }
        }
        if (all) ids.push_back(rule.id);
    }
    return ids;
}

std::vector<int> CompletionIds(const Rules& rules, int world, int percent) {
    std::vector<int> ids;
    if (world < 0 || static_cast<std::size_t>(world) >= rules.completion.size()) return ids;
    for (const CompletionStep& step : rules.completion[static_cast<std::size_t>(world)]) {
        if (percent >= step.minPercent) ids.push_back(step.id);
    }
    return ids;
}

int HelpBlockId(const Rules& rules, int world, int level) {
    for (const Rules::HelpBlock& block : rules.helpBlocks) {
        if (block.world == world && block.level == level) return block.id;
    }
    return -1;
}

std::string HashOf(const Rules::Save& rules, const std::string& list) {
    // #3-24: no list, no hash.
    if (list.empty()) return std::string();
    // #44-88: each ParseUInt'd id times (t + 2), the product in 32 bits and
    // widened unsigned (uTOi64), summed in 64.
    std::int64_t hash = 0;
    std::size_t start = 0;
    std::uint32_t t = 0;
    while (start <= list.size()) {
        std::size_t end = list.find(',', start);
        if (end == std::string::npos) end = list.size();
        const std::string piece = list.substr(start, end - start);
        std::uint32_t id = 0;
        for (const char c : piece) {
            if (c < '0' || c > '9') break;
            id = id * 10u + static_cast<std::uint32_t>(c - '0');
        }
        const std::uint32_t product = id * (t + static_cast<std::uint32_t>(rules.firstMultiplierOffset));
        hash += static_cast<std::int64_t>(product);
        ++t;
        start = end + 1;
        if (end == list.size()) break;
    }
    // #90-100: less 27, times 3, plus 2; #103 cut to an int, #104-117 in decimal.
    hash = (hash - rules.subtract) * rules.multiply + rules.add;
    const auto cut = static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<std::uint64_t>(hash)));
    return std::to_string(cut);
}

std::string ListOf(const std::vector<int>& ids) {
    std::string list;
    for (const int id : ids) {
        if (!list.empty()) list += ',';
        list += std::to_string(id);
    }
    return list;
}

std::string PopupText(const Rules& rules, const Content& content, const std::vector<const Entry*>& entries,
                      const std::vector<int>& unlocked) {
    std::string text;
    for (std::size_t t = 0; t < entries.size(); ++t) {
        const Entry& entry = *entries[t];
        const bool open = IsUnlocked(unlocked, entry.id);
        text += (open ? rules.popup.unlockedMark : rules.popup.lockedMark) + " " + TitleOf(content, entry, open) +
                ":\n" + DescriptionOf(content, entry, open);
        if (t + 1 < entries.size()) text += "\n\n";
    }
    return text;
}

// ---- the saved set ---------------------------------------------------------------

namespace {

bool ReadFile(const std::string& path, std::string& text) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    std::ostringstream buffer;
    buffer << file.rdbuf();
    text = buffer.str();
    return true;
}

constexpr int kSaveVersion = 1;

// isUnlocked's ParseInt of each piece: the digits it starts with.
bool ParseIds(const std::string& list, std::vector<int>& out) {
    out.clear();
    if (list.empty()) return true;
    std::size_t start = 0;
    while (true) {
        std::size_t end = list.find(',', start);
        if (end == std::string::npos) end = list.size();
        const std::string piece = list.substr(start, end - start);
        if (piece.empty() || piece.size() > 9 ||
            !std::all_of(piece.begin(), piece.end(), [](char c) { return c >= '0' && c <= '9'; })) {
            return false;
        }
        out.push_back(std::stoi(piece));
        if (end == list.size()) break;
        start = end + 1;
    }
    return true;
}

} // namespace

bool Store::Open(const std::string& directory, const Rules::Save& rules, std::string& error) {
    m_rules = rules;
    m_ids.clear();
    m_path.clear();
    m_fromFile = false;
    m_hashRefused = false;
    error.clear();
    if (directory.empty()) return true;
    m_path = (std::filesystem::path(directory) / rules.file).string();

    std::error_code ec;
    if (!std::filesystem::exists(m_path, ec) || ec) return true; // nothing unlocked yet

    std::string text;
    if (!ReadFile(m_path, text)) {
        error = m_path + ": cannot open";
        return false;
    }
    Json::Parser parser(text);
    Json::Value root;
    if (!parser.Parse(root)) {
        error = m_path + ": " + parser.Error();
        return false;
    }
    const Json::Value& entity = root[rules.entity];
    const Json::Value& list = entity[rules.listKey];
    const Json::Value& hash = entity[rules.hashKey];
    if (!list.IsString() || !hash.IsString()) {
        error = m_path + ": " + rules.entity + " lacks a " + rules.listKey + " or a " + rules.hashKey + " string";
        return false;
    }
    m_fromFile = true;
    // loadList #125-137: kept only when its hash is the list's own.
    if (HashOf(rules, list.AsString()) != hash.AsString()) {
        m_hashRefused = true;
        return true;
    }
    std::vector<int> ids;
    if (!ParseIds(list.AsString(), ids)) {
        error = m_path + ": " + rules.listKey + " is not ids separated by commas";
        return false;
    }
    m_ids = std::move(ids);
    return true;
}

bool Store::Has(int id) const { return std::find(m_ids.begin(), m_ids.end(), id) != m_ids.end(); }

bool Store::Add(int id) {
    if (Has(id)) return false;
    m_ids.push_back(id);
    return true;
}

std::vector<int> Store::Sorted() const {
    std::vector<int> ids = m_ids;
    std::sort(ids.begin(), ids.end());
    return ids;
}

bool Store::Save(std::string& error) const {
    error.clear();
    if (m_path.empty()) return true;
    const std::filesystem::path path(m_path);
    std::error_code ec;
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path(), ec);

    const std::string list = ListOf(m_ids);
    const std::filesystem::path temp = path.string() + ".tmp";
    {
        std::ofstream file(temp, std::ios::binary | std::ios::trunc);
        if (!file) {
            error = temp.string() + ": cannot open for writing";
            return false;
        }
        file << "{\n";
        file << "  \"version\": " << kSaveVersion << ",\n";
        file << "  \"" << m_rules.entity << "\": {\n";
        file << "    \"" << m_rules.listKey << "\": \"" << list << "\",\n";
        file << "    \"" << m_rules.hashKey << "\": \"" << HashOf(m_rules, list) << "\"\n";
        file << "  }\n";
        file << "}\n";
        if (!file.good()) {
            error = temp.string() + ": write failed";
            return false;
        }
    }
    ec.clear();
    std::filesystem::rename(temp, path, ec);
    if (ec) {
        // Some filesystems refuse a rename onto an existing file.
        ec.clear();
        std::filesystem::remove(path, ec);
        ec.clear();
        std::filesystem::rename(temp, path, ec);
    }
    if (ec) {
        error = m_path + ": " + ec.message();
        return false;
    }
    return true;
}

} // namespace MagicPortals::Achievements
