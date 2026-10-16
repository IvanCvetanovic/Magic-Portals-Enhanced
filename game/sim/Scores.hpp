#pragma once

// The medal each level was cleared with, kept between runs.
//
// WHY THIS EXISTS. The level grid draws a small medal on every level the player
// has finished - LevelChooser::itemDrawCallback asks ScoreManager::getScore for
// a level and, when it is not zero, draws getSmallSpriteMedalName(score) at the
// button's corner. The port kept no score at all, so there was nothing for that
// to read and every button was bare. That is what the owner saw and reported as
// "i dont see any mdels at the level select".
//
// The original keeps this in scores.enml beside its golden scores, written
// through ScoreManager::setScore. This is the same information in the shape the
// port already uses for everything else - JSON, read with core/Json.hpp - and
// deliberately not the original's format: nothing reads the original's file, and
// a second enml parser would exist only to be compatible with a game that is no
// longer installed.
//
// Free of the renderer and of the engine, like the rest of sim/, so a test can
// exercise the whole of it with a temporary directory and no window.

#include <cstddef>
#include <string>
#include <vector>

namespace MagicPortals::Scores {

// A medal, as ScoreManager::computeScore returns one (bytes 363979..364204).
//
// Zero is the important one: getSmallSpriteMedalName maps 3 to gold, 2 to
// silver and ANYTHING ELSE to bronze, so bronze is what an unrecognised value
// becomes - and the grid's guard is `score != 0`, which is what makes zero mean
// "never finished" rather than "finished badly". A level cleared at the worst
// medal still stores 1.
inline constexpr int kUnplayed = 0;
inline constexpr int kBronze = 1;
inline constexpr int kSilver = 2;
inline constexpr int kGold = 3;

// How a scores.json that cannot be OPENED is tried: three attempts, 100 ms apart, which is two waits and about a fifth of a second
// at the worst. Another program holding the file for a moment (a scanner, a sync client, a second window) is the likely cause and a
// lock is not a corrupt file; a longer wait would be felt as a hang at the start, and a locked file is tried again at every save and
// as the game ends in any case. Robustness constants, not gameplay numbers: nothing in the original corresponds to them.
inline constexpr int kOpenAttempts = 3;
inline constexpr int kOpenWaitMs = 100;

// The medals, by world and level.
//
// Addressed by (world, level) rather than by a flat level index because that is
// how the original addresses them - getScore(levelIdx, world), over a ScoreList
// per world - and because a flat index would silently renumber every saved
// medal if a chapter ever gained or lost a level.
class Store {
public:
    // Reads `directory`/scores.json, if there is one.
    //
    // An EMPTY directory is not an error and not a failure to load: it means
    // this store is memory-only and will never touch the filesystem. That is
    // WolfBrigadeLayer's contract for its own save directory, and it exists so
    // that every suite can build a layer bare without eighteen of them writing
    // the same filename into the ctest working directory.
    //
    // A directory with no scores.json in it is not an error either - that is
    // simply a player who has not finished a level yet.
    //
    // THE POLICY (planning step 189), for a file that is there and is not what
    // this store wrote - cut short by a power failure, emptied, or hand-edited:
    //
    //   - the file is NEVER deleted and never written over. It is renamed to
    //     scores.json.bad-<n> (the first free n), every medal it still holds is
    //     taken from it row by row, and the store starts from those and saves
    //     them at once, so the next start reads a whole file. Starting from
    //     nothing and leaving the bad file where it was would have been the end
    //     of saving for good, silently: this store used to go memory-only for the
    //     session and the player's medals vanished at exit;
    //   - a file that cannot be OPENED (another program holds it for a moment: a
    //     scanner, a sync client) is tried a few times, a short bounded wait
    //     apart, and is then left alone: the store is memory-only for the session
    //     and tries again when a medal is saved and at Retry, which the layer
    //     calls as it ends. What the session earned is merged into the file by
    //     then, the better of each level, and nothing is lost.
    //
    // False, with `error` saying what happened, whenever there is something to
    // tell the player or the log: the file was moved aside (Persists() is then
    // true), or it was out of reach (Persists() is false). True for a fresh
    // folder and for a file that read whole.
    bool Open(const std::string& directory, std::string& error);

    // What the last Open (or Retry) found, for a caller that wants to say it.
    enum class Opened {
        MemoryOnly,  // no directory was given
        Fresh,       // no scores.json yet
        Read,        // a whole file, read
        Quarantined, // an unreadable file, moved aside; its medals recovered
        OutOfReach,  // a file that could not be opened (or moved aside): memory-only for now
        Merged,      // a Retry reached a file that was out of reach, and wrote the merge
    };
    Opened WhatOpened() const { return m_opened; }
    // Where the unreadable file was moved to, and how many medals it still held (Quarantined).
    const std::string& QuarantinedTo() const { return m_quarantinedTo; }
    int Recovered() const { return m_recovered; }

    // Whether the file was out of reach and the store is waiting to try again.
    bool Waiting() const { return m_path.empty() && !m_file.empty(); }
    // Tries the file again (the same bounded wait) and, if it opens, merges what this session earned with what it holds and
    // writes the result. True when there was nothing to wait for or the merge was written; false, with `error`, when the
    // file is still out of reach.
    bool Retry(std::string& error);

    // The medal this level was cleared with, or kUnplayed.
    int Get(int world, int level) const;

    // Keeps the BETTER of the two, which is what ScoreManager::setScore does:
    // it writes only `if (score > getScore(level, world) || overwrite)`. A
    // replay that goes worse must not take a gold away.
    //
    // Returns whether anything changed, so a caller can skip a write that would
    // rewrite the file with what it already says.
    bool Record(int world, int level, int medal);

    // Writes scores.json in one step (Files::WriteAtomic) and keeps the last
    // good one beside it as scores.json.bak. A file truncated by a process
    // killed mid-write is worse than no file, and this one is the player's
    // progress. A memory-only store saves nothing and reports success - it has
    // nowhere to write, which is not a failure. A store that is Waiting tries
    // the file again first; false, with `error`, if it is still out of reach.
    bool Save(std::string& error);

    // Whether this store has somewhere to write.
    bool Persists() const { return !m_path.empty(); }

    // How many levels have a medal. For the tests and the log line.
    std::size_t Count() const { return m_entries.size(); }

private:
    struct Entry {
        int world = 0;
        int level = 0;
        int medal = kUnplayed;
    };

    // One read of the file into the entries, the quarantine included. Open clears first; Retry does not.
    bool Load(std::string& error);
    // The file written, with no look at whether the store is waiting.
    bool Write(std::string& error);

    std::vector<Entry> m_entries;

    // The file this store keeps, or empty for a memory-only store.
    std::string m_file;
    // The file, once it is safe to write: empty while the store is memory-only or waiting for a file that is out of reach.
    std::string m_path;

    Opened m_opened = Opened::MemoryOnly;
    std::string m_quarantinedTo;
    int m_recovered = 0;
};

} // namespace MagicPortals::Scores
