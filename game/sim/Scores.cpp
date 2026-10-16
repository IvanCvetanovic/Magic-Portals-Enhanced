#include "sim/Scores.hpp"

#include "core/Json.hpp"
#include "sim/Files.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>

namespace MagicPortals::Scores {

namespace {

namespace Json = Supersonic::Json;

// The file, inside whichever directory the game was handed.
std::string FileIn(const std::string& directory) {
    return (std::filesystem::path(directory) / "scores.json").string();
}

// kUnplayed through kGold and nothing else.
//
// A value outside that range is not merely odd: getLargeSpriteMedalName maps
// everything it does not recognise to BRONZE, so a 7 in the file would draw a
// bronze medal on a level and look like a real result.
bool IsMedal(int medal) { return medal >= kUnplayed && medal <= kGold; }

// A world or a level number. Bounded as well as non-negative: a double outside int's range cast to int is undefined, and a hand-edit
// or a torn write can put one there.
constexpr double kMostWorldsOrLevels = 100000.0;
bool ReadIndex(const Json::Value& value, int& out) {
    if (!value.IsNumber()) return false;
    const double number = value.AsNumber();
    if (!std::isfinite(number) || number < 0.0 || number > kMostWorldsOrLevels) return false;
    out = static_cast<int>(number);
    return true;
}

struct Row {
    int world = 0;
    int level = 0;
    int medal = kUnplayed;
};

// One score, as the file spells it. False for an object that is not one: a field missing, not a number, or outside the medals.
bool RowOf(const Json::Value& entry, Row& row) {
    if (!entry.IsObject() || !entry.Has("world") || !entry.Has("level") || !entry.Has("medal")) return false;
    if (!ReadIndex(entry["world"], row.world) || !ReadIndex(entry["level"], row.level)) return false;
    if (!entry["medal"].IsNumber()) return false;
    // Judged before the cast, for the same reason as the indices.
    const double medal = entry["medal"].AsNumber();
    if (!(medal >= static_cast<double>(kUnplayed) && medal <= static_cast<double>(kGold))) return false;
    row.medal = static_cast<int>(medal);
    return IsMedal(row.medal);
}

// What a file holds: every score that reads, and whether that was ALL of it.
struct RowSet {
    std::vector<Row> rows;
    bool whole = false; // a document of this store's shape, every score of it good
    std::string why;    // when not whole
};

// The scores of a document that does not parse (a write cut short, a file emptied): every object in it that is a score, found by
// looking for the innermost braces, which is how this store writes a score (one flat object on its own line). It reads what a torn
// file still holds of the first rows, and nothing past the tear.
void ScanRows(const std::string& text, std::vector<Row>& rows) {
    std::size_t at = 0;
    while ((at = text.find('{', at)) != std::string::npos) {
        const std::size_t close = text.find('}', at + 1);
        if (close == std::string::npos) break;
        const std::size_t inner = text.find('{', at + 1);
        if (inner != std::string::npos && inner < close) {
            at = inner; // an enclosing brace, not a score
            continue;
        }
        // NAMED, not a temporary: Json::Parser holds its text by reference.
        const std::string candidate = text.substr(at, close - at + 1);
        Json::Parser parser(candidate);
        Json::Value value;
        Row row;
        if (parser.Parse(value) && RowOf(value, row)) rows.push_back(row);
        at = close + 1;
    }
}

RowSet ReadRows(const std::string& text) {
    RowSet set;
    // NAMED, not a temporary: Json::Parser holds its text by reference and
    // deletes the rvalue constructor precisely to stop `Parser(read(path))`.
    Json::Parser parser(text);
    Json::Value root;
    if (!parser.Parse(root)) {
        set.why = parser.Error();
        ScanRows(text, set.rows);
        return set;
    }
    if (!root.IsObject() || !root["scores"].IsArray()) {
        set.why = "scores is not an array";
        ScanRows(text, set.rows);
        return set;
    }
    // Field by field, as Controls::Store reads its file: a bad score costs that score, not the others.
    int dropped = 0;
    for (const Json::Value& entry : root["scores"].AsArray()) {
        Row row;
        if (RowOf(entry, row)) {
            set.rows.push_back(row);
        } else {
            ++dropped;
        }
    }
    set.whole = dropped == 0;
    if (!set.whole) {
        set.why = std::to_string(dropped) + " score(s) are not a world, a level and a medal of 0 to 3";
    }
    return set;
}

} // namespace

bool Store::Open(const std::string& directory, std::string& error) {
    m_entries.clear();
    m_path.clear();
    m_file.clear();
    m_opened = Opened::MemoryOnly;
    m_quarantinedTo.clear();
    m_recovered = 0;
    error.clear();

    // Memory only, and deliberately silent about it: this is how every suite
    // builds the layer.
    if (directory.empty()) return true;

    m_file = FileIn(directory);
    return Load(error);
}

bool Store::Load(std::string& error) {
    error.clear();
    std::string text;
    std::string why;
    const Files::ReadResult read = Files::ReadWhole(m_file, text, kOpenAttempts, kOpenWaitMs, &why);

    // A directory with nothing saved in it is a player who has not finished a
    // level, not a failure. The path is kept so the first Save creates the file.
    if (read == Files::ReadResult::Missing) {
        m_path = m_file;
        m_opened = Opened::Fresh;
        return true;
    }

    // A file that is there and cannot be opened is not a file that is bad. It is left exactly as it is, and the store waits.
    if (read == Files::ReadResult::Unreadable) {
        m_path.clear();
        m_opened = Opened::OutOfReach;
        error = why + " after " + std::to_string(kOpenAttempts) + " tries; this session's medals are kept in memory and saved when it opens";
        return false;
    }

    const RowSet set = ReadRows(text);
    if (set.whole) {
        // Through Record, so a file naming the same level twice keeps the
        // better of the two rather than whichever line happened to be last.
        for (const Row& row : set.rows) Record(row.world, row.level, row.medal);
        m_path = m_file;
        m_opened = Opened::Read;
        return true;
    }

    // NOT A FILE THIS STORE WROTE. It is never deleted and never written over: it is moved aside, the medals it still holds are
    // kept, and the store carries on from them. (Leaving it in place and going memory-only, as this did before planning step 189,
    // lost every medal earned from then on, silently, until a player deleted the file by hand.)
    std::string movedTo;
    std::string moveError;
    for (const Row& row : set.rows) Record(row.world, row.level, row.medal);
    if (!Files::MoveAside(m_file, movedTo, moveError)) {
        m_path.clear();
        m_opened = Opened::OutOfReach;
        error = m_file + " is not a scores file (" + set.why + ") and could not be moved aside (" + moveError +
                "); it is left as it is, and this session's medals are kept in memory";
        return false;
    }
    m_path = m_file;
    m_opened = Opened::Quarantined;
    m_quarantinedTo = movedTo;
    m_recovered = static_cast<int>(set.rows.size());
    error = m_file + " is not a scores file (" + set.why + "); kept as " + movedTo + ", " + std::to_string(m_recovered) +
            " medal(s) recovered from it";
    // At once: the medals recovered live only in memory and in the bad copy until a whole file holds them.
    if (std::string saveError; !Write(saveError)) {
        error += "; they could not be saved yet: " + saveError;
    }
    return false;
}

bool Store::Retry(std::string& error) {
    error.clear();
    if (!Waiting()) return true;
    // Merges with what the session holds: Load does not clear.
    Load(error);
    if (m_path.empty()) return false;
    // A quarantine wrote its own result.
    if (m_opened == Opened::Quarantined) return true;
    std::string writeError;
    if (!Write(writeError)) {
        error = writeError;
        return false;
    }
    error.clear();
    m_opened = Opened::Merged;
    return true;
}

int Store::Get(int world, int level) const {
    for (const Entry& entry : m_entries) {
        if (entry.world == world && entry.level == level) return entry.medal;
    }
    return kUnplayed;
}

bool Store::Record(int world, int level, int medal) {
    // kUnplayed is not recorded: it is the absence of a medal, and writing it
    // would put rows in the file that mean "no row".
    if (world < 0 || level < 0 || !IsMedal(medal) || medal == kUnplayed) return false;

    for (Entry& entry : m_entries) {
        if (entry.world != world || entry.level != level) continue;
        if (medal <= entry.medal) return false; // a worse replay takes nothing away
        entry.medal = medal;
        return true;
    }

    Entry added;
    added.world = world;
    added.level = level;
    added.medal = medal;
    m_entries.push_back(added);
    return true;
}

bool Store::Save(std::string& error) {
    error.clear();
    if (m_file.empty()) return true; // memory only: nowhere to write is not a failure
    if (m_path.empty()) {
        // The file was out of reach: try it again, and what the session earned is merged into it.
        return Retry(error);
    }
    return Write(error);
}

bool Store::Write(std::string& error) {
    // Sorted, so the file does not reshuffle itself every time a level is
    // cleared: a save that rewrites its whole order on each play is one nobody
    // can diff, and this one is small enough that sorting costs nothing.
    std::vector<Entry> ordered = m_entries;
    std::sort(ordered.begin(), ordered.end(), [](const Entry& a, const Entry& b) {
        if (a.world != b.world) return a.world < b.world;
        return a.level < b.level;
    });

    std::string text;
    text += "{\n";
    text += "  \"version\": 1,\n";
    text += "  \"scores\": [\n";
    for (std::size_t i = 0; i < ordered.size(); ++i) {
        const Entry& entry = ordered[i];
        text += "    {\"world\": " + std::to_string(entry.world) + ", \"level\": " + std::to_string(entry.level) +
                ", \"medal\": " + std::to_string(entry.medal) + "}";
        if (i + 1 < ordered.size()) text += ",";
        text += "\n";
    }
    text += "  ]\n";
    text += "}\n";

    // This file is the player's progress, and it is only ever left in place when it is one this store wrote or read whole, so the
    // copy kept beside it is always a good one.
    Files::WriteOptions options;
    options.keepBackup = true;
    return Files::WriteAtomic(m_path, text, options, error);
}

} // namespace MagicPortals::Scores
