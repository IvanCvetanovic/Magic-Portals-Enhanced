// The player's medals, and the sound and music switches, kept between runs.
//
// The port kept no score at all, so the level grid had nothing to draw a medal
// from and every button was bare - which is what the owner reported. This is
// the store behind it, and what is pinned here is the handful of rules that
// make a save file safe rather than merely present:
//
//   - a worse replay never takes a better medal away (ScoreManager::setScore
//     writes only when the new score beats the old);
//   - an empty save directory touches no filesystem at all, which is the
//     contract every other suite depends on;
//   - a file that will not parse is NEVER deleted or written over: it is moved
//     aside as scores.json.bad-<n>, the medals it still holds are kept and saved
//     at once, and one that cannot be OPENED is waited for and merged when it
//     opens (planning step 189; before it, the file was refused and left alone and
//     the store went memory-only, so the session's medals vanished at exit).
//
// The switches (sim/Switches.hpp, planning step 124) are the other thing the
// original saves between runs, and the same rules hold for them but one: a file
// that will not parse is written over at the next toggle, because it holds
// nothing but the two switches - which is what the original's saveValue does.
//
// Free of the engine, like the rest of sim/, so all of it runs with a temporary
// directory and no window.

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "sim/Scores.hpp"
#include "sim/Switches.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#if defined(_WIN32)
#include <share.h>
#endif

using namespace MagicPortals;

namespace {

// A directory of this suite's own, emptied first: a store that read what the
// last run left behind would pass or fail for reasons nothing here states.
std::filesystem::path Scratch(const char* name) {
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "supersonic-test-mp-scores" / name;
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);
    return dir;
}

void AnEmptyDirectoryTouchesNothing() {
    Scores::Store store;
    std::string error;
    CHECK_MSG(store.Open("", error), "a memory-only store opens: " + error);
    CHECK_MSG(!store.Persists(), "and has nowhere to write");

    // It still REMEMBERS - it is only the file that is absent. A store that
    // forgot as well would make every suite's medals invisible to itself.
    CHECK(store.Record(0, 3, Scores::kSilver));
    CHECK_EQ(store.Get(0, 3), Scores::kSilver);

    CHECK_MSG(store.Save(error), "saving a memory-only store is not a failure: " + error);
}

void AWorsePlayTakesNothingAway() {
    Scores::Store store;
    std::string error;
    CHECK(store.Open("", error));

    CHECK(store.Record(1, 5, Scores::kGold));
    CHECK_MSG(!store.Record(1, 5, Scores::kBronze), "a worse replay must not overwrite a gold");
    CHECK_EQ(store.Get(1, 5), Scores::kGold);

    // Improving one does take.
    CHECK(store.Record(1, 7, Scores::kBronze));
    CHECK_MSG(store.Record(1, 7, Scores::kSilver), "but a better play does replace it");
    CHECK_EQ(store.Get(1, 7), Scores::kSilver);

    // kUnplayed is the ABSENCE of a medal, not a medal: recording it would put
    // rows in the file that mean "no row", and the grid's guard is score != 0.
    CHECK_MSG(!store.Record(2, 0, Scores::kUnplayed), "an unplayed level is not recorded");
    CHECK_EQ(store.Get(2, 0), Scores::kUnplayed);

    // A level nobody has finished reads as unplayed rather than as bronze.
    CHECK_EQ(store.Get(3, 30), Scores::kUnplayed);
}

void MedalsSurviveTheRun() {
    const std::filesystem::path dir = Scratch("roundtrip");
    std::string error;
    {
        Scores::Store store;
        CHECK_MSG(store.Open(dir.string(), error), error);
        CHECK_MSG(store.Persists(), "a store given a directory has somewhere to write");
        store.Record(0, 0, Scores::kGold);
        store.Record(0, 1, Scores::kSilver);
        store.Record(3, 31, Scores::kBronze);
        CHECK_MSG(store.Save(error), error);
    }
    {
        Scores::Store reopened;
        CHECK_MSG(reopened.Open(dir.string(), error), error);
        CHECK_EQ(reopened.Get(0, 0), Scores::kGold);
        CHECK_EQ(reopened.Get(0, 1), Scores::kSilver);
        CHECK_EQ(reopened.Get(3, 31), Scores::kBronze);
        CHECK_EQ(reopened.Get(2, 2), Scores::kUnplayed);
        CHECK_EQ(static_cast<int>(reopened.Count()), 3);
    }
}

void AFreshFolderIsAPlayerWhoHasFinishedNothing() {
    const std::filesystem::path dir = Scratch("fresh");
    Scores::Store store;
    std::string error;
    CHECK_MSG(store.Open(dir.string(), error), "no scores.json yet is not an error: " + error);
    CHECK_MSG(store.Persists(), "and the first Save is what creates it");
    CHECK_EQ(static_cast<int>(store.Count()), 0);
}

// A file that is there and is not what the store wrote is NEVER deleted and never written over (planning step 189). It is moved aside
// as scores.json.bad-<n>, every medal it still holds is kept, and the store carries on from them and saves them at once. The cases are
// the ways such a file comes to be: a write cut short, a file emptied, a row gone wrong, something that is not a scores document at all.
std::string ReadAll(const std::filesystem::path& file);

std::vector<std::string> Entries(const std::filesystem::path& dir) {
    std::vector<std::string> names;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) names.push_back(entry.path().filename().string());
    std::sort(names.begin(), names.end());
    return names;
}

void Put(const std::filesystem::path& file, const std::string& text) {
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    out << text;
}

bool EndsWith(const std::string& text, const std::string& tail) {
    return text.size() >= tail.size() && text.compare(text.size() - tail.size(), tail.size(), tail) == 0;
}

void AnUnreadableFileIsKeptAsABadCopyAndNeverDeleted() {
    const std::filesystem::path dir = Scratch("corrupt");
    const std::filesystem::path file = dir / "scores.json";
    const std::string garbage = "{ this is not json";
    Put(file, garbage);

    Scores::Store store;
    std::string error;
    CHECK_MSG(!store.Open(dir.string(), error), "a file that will not parse is reported, so the log and the player can be told");
    CHECK_MSG(error.find("scores.json.bad-1") != std::string::npos, "and the report says where the file went: " + error);
    CHECK_MSG(store.WhatOpened() == Scores::Store::Opened::Quarantined, "it was quarantined");
    CHECK_MSG(EndsWith(store.QuarantinedTo(), "scores.json.bad-1"), "to the first free name: " + store.QuarantinedTo());
    CHECK_MSG(!store.Dirty(), "and the whole file written at once leaves the store level with it (planning step 190)");

    // NOT memory-only any more: the session's medals reach a file. (Before this step one bad byte ended saving for good, silently.)
    CHECK_MSG(store.Persists() && !store.Waiting(), "the store has somewhere to write");
    CHECK_MSG(store.Record(0, 4, Scores::kSilver), "a medal earned now");
    std::string why;
    CHECK_MSG(store.Save(why), "is saved: " + why);

    // AND THE UNREADABLE FILE IS KEPT, byte for byte: starting from nothing and discarding it would erase a player's progress.
    CHECK_MSG(ReadAll(dir / "scores.json.bad-1") == garbage, "the unreadable file is still there, untouched, under its new name");
    Scores::Store reopened;
    CHECK_MSG(reopened.Open(dir.string(), error), "and scores.json is a whole file again: " + error);
    CHECK_MSG(reopened.WhatOpened() == Scores::Store::Opened::Read && reopened.Get(0, 4) == Scores::kSilver &&
                  reopened.Count() == 1,
              "holding the medal");

    // A second bad file takes the next free number, and the first copy is not replaced.
    Put(file, "[1, 2");
    Scores::Store second;
    CHECK(!second.Open(dir.string(), error));
    CHECK_MSG(EndsWith(second.QuarantinedTo(), "scores.json.bad-2"), "the next bad file is -2: " + second.QuarantinedTo());
    CHECK_MSG(ReadAll(dir / "scores.json.bad-1") == garbage && ReadAll(dir / "scores.json.bad-2") == "[1, 2",
              "and each copy holds its own bytes");
}

void AWriteCutShortKeepsTheMedalsBeforeTheCut() {
    const std::filesystem::path dir = Scratch("torn");
    const std::filesystem::path file = dir / "scores.json";
    // What a power cut leaves of this store's own format: whole rows, then a row that stops.
    const std::string torn = "{\n  \"version\": 1,\n  \"scores\": [\n    {\"world\": 0, \"level\": 0, \"medal\": 3},\n"
                             "    {\"world\": 0, \"level\": 1, \"medal\": 2},\n    {\"world\": 1, \"level\": 7, \"medal\": 1},\n"
                             "    {\"world\": 2, \"lev";
    Put(file, torn);
    Scores::Store store;
    std::string error;
    CHECK(!store.Open(dir.string(), error));
    CHECK_MSG(store.WhatOpened() == Scores::Store::Opened::Quarantined && store.Recovered() == 3, "three medals recovered: " + error);
    CHECK_MSG(store.Get(0, 0) == Scores::kGold && store.Get(0, 1) == Scores::kSilver && store.Get(1, 7) == Scores::kBronze &&
                  store.Get(2, 0) == Scores::kUnplayed && store.Count() == 3,
              "the rows before the cut, and nothing past it");
    CHECK_MSG(ReadAll(dir / "scores.json.bad-1") == torn, "the torn file is kept whole");

    // SAVED AT ONCE, not at the next medal: a player who closes the game now still has them at the next start.
    Scores::Store next;
    CHECK_MSG(next.Open(dir.string(), error), "the next start reads a whole file: " + error);
    CHECK_MSG(next.Count() == 3 && next.Get(1, 7) == Scores::kBronze, "with the three medals in it");
}

void ABadRowCostsThatRowAndNotTheFile() {
    const std::filesystem::path dir = Scratch("badrow");
    // Valid JSON, one row outside the medals (a 7 would draw a bronze), one with a field missing, one past int's range.
    Put(dir / "scores.json",
        "{\"version\": 1, \"scores\": [{\"world\": 0, \"level\": 0, \"medal\": 3}, {\"world\": 0, \"level\": 1, \"medal\": 7},"
        " {\"world\": 0, \"level\": 2}, {\"world\": 1e300, \"level\": 0, \"medal\": 1}, {\"world\": 3, \"level\": 31, \"medal\": 2}]}");
    Scores::Store store;
    std::string error;
    CHECK(!store.Open(dir.string(), error));
    CHECK_MSG(!error.empty(), "and says why: " + error);
    CHECK_MSG(store.Recovered() == 2 && store.Get(0, 0) == Scores::kGold && store.Get(3, 31) == Scores::kSilver &&
                  store.Get(0, 1) == Scores::kUnplayed,
              "the two good rows are kept; the seven is not read as a bronze");
    CHECK_MSG(std::filesystem::exists(dir / "scores.json.bad-1"), "the file as it was is kept");
}

void AnEmptyFileOrAnotherDocumentIsQuarantinedToo() {
    for (const char* text : {"", "[1, 2, 3]", "{}", "{\"scores\": 5}", "null"}) {
        const std::filesystem::path dir = Scratch("other");
        Put(dir / "scores.json", text);
        Scores::Store store;
        std::string error;
        CHECK_MSG(!store.Open(dir.string(), error), std::string("'") + text + "' is not a scores file");
        CHECK_MSG(store.WhatOpened() == Scores::Store::Opened::Quarantined && store.Persists() && store.Count() == 0,
                  std::string("'") + text + "' is moved aside and the store starts clean: " + error);
        CHECK_MSG(ReadAll(dir / "scores.json.bad-1") == text, std::string("'") + text + "' is kept as it was");
        Scores::Store reopened;
        CHECK_MSG(reopened.Open(dir.string(), error), std::string("'") + text + "': a whole file stands where it was: " + error);
    }
}

// A file at the name that cannot be opened is not a bad file: a directory stands in for any lock, on every platform. The store
// waits, tells the player's log, keeps the session's medals and writes them in when the name is free.
void AFileOutOfReachIsWaitedForNotQuarantined() {
    const std::filesystem::path dir = Scratch("outofreach");
    const std::filesystem::path file = dir / "scores.json";
    std::filesystem::create_directories(file);
    Put(file / "keep.txt", "kept");

    Scores::Store store;
    std::string error;
    const auto before = std::chrono::steady_clock::now();
    CHECK_MSG(!store.Open(dir.string(), error), "a name that cannot be opened is reported");
    const auto waited = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - before).count();
    CHECK_MSG(waited >= static_cast<long long>(Scores::kOpenAttempts - 1) * Scores::kOpenWaitMs - 20 && waited < 5000,
              "after the bounded wait (" + std::to_string(Scores::kOpenAttempts) + " tries, " + std::to_string(Scores::kOpenWaitMs) +
                  " ms apart), not at once and not for ever: " + std::to_string(waited) + " ms");
    CHECK_MSG(store.WhatOpened() == Scores::Store::Opened::OutOfReach && !store.Persists() && store.Waiting(),
              "memory-only for now, waiting");
    CHECK_MSG(error.find("scores.json") != std::string::npos, "and it names the file: " + error);
    CHECK_MSG(std::filesystem::is_directory(file) && ReadAll(file / "keep.txt") == "kept", "what stood at the name is untouched");
    CHECK_MSG(Entries(dir) == std::vector<std::string>{"scores.json"}, "and nothing was quarantined or written beside it");

    CHECK_MSG(!store.Dirty(), "nothing earned yet: nothing is ahead of the file");
    CHECK(store.Record(1, 2, Scores::kGold));
    CHECK_MSG(store.Dirty(), "a medal earned while the file is out of reach: the store is ahead of it");
    std::string saved;
    CHECK_MSG(!store.Save(saved) && !saved.empty(), "a save while it is still out of reach says so and loses nothing: " + saved);
    CHECK_MSG(store.Dirty(), "and is still ahead of it");
    CHECK_MSG(store.Get(1, 2) == Scores::kGold, "the medal is still in memory");

    // The way clears: the next save (or Retry, as the game ends) merges and writes.
    std::error_code ec;
    std::filesystem::remove_all(file, ec);
    CHECK_MSG(store.Save(saved), "the name is free, and the save writes: " + saved);
    CHECK_MSG(store.Persists() && !store.Waiting() && store.WhatOpened() == Scores::Store::Opened::Merged, "the store persists now");
    CHECK_MSG(!store.Dirty(), "and is level with its file");
    Scores::Store reopened;
    CHECK_MSG(reopened.Open(dir.string(), error) && reopened.Get(1, 2) == Scores::kGold, "and the medal is on disk: " + error);
}

// A SAVE THAT FAILED AFTER A GOOD OPEN (planning step 190). The store was level with a file that read whole; a medal is earned; the save
// fails (a directory holding something stands at the name, which the atomic write cannot replace on any platform); the medal is in memory
// only. Dirty() is the store's word for that, and what the game asks as it ends: Waiting() knows only a file that never opened, so
// before this a medal whose save failed was lost at exit with no second try. A save that works clears it; a Record that changes nothing
// does not set it.
void ASaveThatFailedLeavesTheStoreAheadOfItsFileUntilOneWorks() {
    {
        Scores::Store memory;
        std::string error;
        CHECK(memory.Open("", error));
        CHECK(memory.Record(0, 0, Scores::kGold));
        CHECK_MSG(!memory.Dirty(), "a memory-only store has no file to be ahead of");
    }
    const std::filesystem::path dir = Scratch("dirty");
    const std::filesystem::path file = dir / "scores.json";
    std::string error;
    {
        Scores::Store first;
        CHECK_MSG(first.Open(dir.string(), error) && first.WhatOpened() == Scores::Store::Opened::Fresh, error);
        CHECK_MSG(!first.Dirty(), "a fresh folder: nothing is ahead of a file that is not there yet");
        CHECK(first.Record(0, 0, Scores::kGold));
        CHECK_MSG(first.Dirty(), "a medal not yet written");
        CHECK_MSG(!first.Record(0, 0, Scores::kBronze) && first.Dirty(), "a worse replay changes nothing and clears nothing");
        CHECK_MSG(first.Save(error), error);
        CHECK_MSG(!first.Dirty(), "a save that worked: level with the file");
    }
    Scores::Store store;
    CHECK_MSG(store.Open(dir.string(), error) && store.WhatOpened() == Scores::Store::Opened::Read, error);
    CHECK_MSG(!store.Dirty(), "a whole file read: what the file says is not news to it");

    // The name now stands a directory that holds something: the replace is refused, and the file that was there is gone from the name
    // (this is the stand-in; nothing is lost that the first store had not written).
    std::error_code ec;
    std::filesystem::remove(file, ec);
    std::filesystem::create_directories(file);
    Put(file / "keep.txt", "kept");
    CHECK(store.Record(0, 1, Scores::kSilver));
    CHECK_MSG(store.Dirty(), "a medal earned");
    CHECK_MSG(!store.Save(error) && !error.empty(), "the save is refused and says so: " + error);
    CHECK_MSG(store.Dirty(), "and the store is still ahead of its file: the game's end must try again");
    CHECK_MSG(!store.Record(0, 1, Scores::kBronze) && store.Dirty(), "a Record that changes nothing does not clear it");

    std::filesystem::remove_all(file, ec);
    CHECK_MSG(store.Save(error), "the name is free, and the next save writes: " + error);
    CHECK_MSG(!store.Dirty(), "level with the file");
    Scores::Store reopened;
    CHECK_MSG(reopened.Open(dir.string(), error) && reopened.Get(0, 0) == Scores::kGold && reopened.Get(0, 1) == Scores::kSilver &&
                  reopened.Count() == 2,
              "and both medals are on disk: " + error);
}

#if defined(_WIN32)
// The real thing: another program holds the file open and refuses to share it. Not a directory standing in for a lock, but the lock.
void AFileLockedByAnotherProgramIsMergedWhenItOpens() {
    const std::filesystem::path dir = Scratch("locked");
    const std::filesystem::path file = dir / "scores.json";
    std::string error;
    {
        Scores::Store first;
        CHECK_MSG(first.Open(dir.string(), error), error);
        first.Record(0, 0, Scores::kGold);
        first.Record(0, 1, Scores::kSilver);
        CHECK_MSG(first.Save(error), error);
    }
    const std::string original = ReadAll(file);

    std::FILE* holder = _wfsopen(file.c_str(), L"rb", _SH_DENYRW);
    CHECK_MSG(holder != nullptr, "another program holds scores.json and shares nothing");
    if (holder == nullptr) return;

    Scores::Store store;
    CHECK_MSG(!store.Open(dir.string(), error) && store.WhatOpened() == Scores::Store::Opened::OutOfReach && store.Waiting(),
              "a locked file is waited for, and then left alone: " + error);
    CHECK(store.Record(0, 2, Scores::kBronze));
    CHECK_MSG(!store.Save(error), "a save while the lock is held is refused, with the medal kept: " + error);
    std::fclose(holder);

    // The end of the game: Retry reaches the file and merges what the session earned with what it held.
    CHECK_MSG(store.Retry(error), "the lock is gone, and Retry writes the merge: " + error);
    Scores::Store merged;
    CHECK_MSG(merged.Open(dir.string(), error), error);
    CHECK_MSG(merged.Get(0, 0) == Scores::kGold && merged.Get(0, 1) == Scores::kSilver && merged.Get(0, 2) == Scores::kBronze &&
                  merged.Count() == 3,
              "the file holds what it did AND the new medal");
    CHECK_MSG(ReadAll(dir / "scores.json.bak") == original, "and the file before is kept beside it as the last good copy");
}
#endif

// The decode's defaults, from the port's own table: loadVolume's 1 and
// loadMusicState's true. A fresh save hears everything.
void TheSwitchesStartOn() {
    Switches::Defaults defaults;
    defaults.globalVolume = -1.0;
    defaults.musicEnabled = false;
    std::string error;
    CHECK_MSG(Switches::LoadDefaults(std::string(MAGICPORTALS_PORT_DATA_DIR) + "/sounds.json", defaults, error),
              error);
    CHECK_MSG(defaults.globalVolume == 1.0, "globalVolume defaults to 1");
    CHECK_MSG(defaults.musicEnabled, "musicEnabled defaults to true");

    Switches::Store store;
    CHECK_MSG(store.Open("", defaults, error), "a memory-only store opens: " + error);
    CHECK_MSG(!store.Persists() && !store.FromFile(), "and has nowhere to write");
    CHECK(store.SoundOn() && store.MusicOn());
    store.SetSound(false);
    CHECK_MSG(!store.SoundOn(), "it still remembers");
    CHECK_MSG(store.Save(error), "saving a memory-only store is not a failure: " + error);
}

// A table without both values is refused rather than read as code's fallback.
void AnIncompleteDefaultsTableIsRefused() {
    const std::filesystem::path dir = Scratch("switch-table");
    const std::filesystem::path table = dir / "sounds.json";
    {
        std::ofstream out(table, std::ios::trunc);
        out << "{ \"switches\": { \"global_volume_default\": 1.0 } }";
    }
    Switches::Defaults defaults;
    std::string error;
    CHECK_MSG(!Switches::LoadDefaults(table.string(), defaults, error), "no music_enabled_default: refused");
    CHECK_MSG(!error.empty(), "and says why");
}

std::string ReadAll(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

void SwitchesSurviveTheRun() {
    const std::filesystem::path dir = Scratch("switches");
    const std::filesystem::path file = dir / Switches::kFileName;
    const Switches::Defaults defaults;
    std::string error;
    {
        Switches::Store store;
        CHECK_MSG(store.Open(dir.string(), defaults, error), "no audio.json yet is not an error: " + error);
        CHECK_MSG(store.Persists() && !store.FromFile(), "a fresh folder: the defaults, and somewhere to write");
        CHECK(store.SoundOn() && store.MusicOn());
        CHECK_MSG(!std::filesystem::exists(file), "opening writes nothing: only a toggle saves");
        store.SetSound(false);
        CHECK_MSG(store.Save(error), error);
        // The original's names and values: the global volume 0, as
        // manageSoundSwitch sets it.
        const std::string text = ReadAll(file);
        CHECK_MSG(text.find("\"audio\"") != std::string::npos &&
                      text.find("\"globalVolume\": 0") != std::string::npos &&
                      text.find("\"musicEnabled\": true") != std::string::npos,
                  "audio.json keeps globalVolume and musicEnabled under audio: " + text);
    }
    {
        Switches::Store reopened;
        CHECK_MSG(reopened.Open(dir.string(), defaults, error), error);
        CHECK_MSG(reopened.FromFile(), "the next run reads the file");
        CHECK_MSG(!reopened.SoundOn() && reopened.MusicOn(), "the sound off, the music on");
        reopened.SetSound(true);
        reopened.SetMusic(false);
        CHECK_MSG(reopened.Save(error), error);
    }
    {
        Switches::Store again;
        CHECK_MSG(again.Open(dir.string(), defaults, error), error);
        CHECK_MSG(again.SoundOn() && !again.MusicOn(), "and the run after that, the sound on and the music off");
    }
}

// loadValue gives the default for a value the file lacks, one by one; and the
// switch reads only 0 as off (GlobalSoundSwitch::GlobalSoundSwitch #25-39).
void WhatTheFileDoesNotSayIsTheDefault() {
    const std::filesystem::path dir = Scratch("switches-partial");
    {
        std::ofstream out(dir / Switches::kFileName, std::ios::trunc);
        out << "{ \"version\": 1, \"audio\": { \"musicEnabled\": false } }";
    }
    Switches::Store store;
    std::string error;
    CHECK_MSG(store.Open(dir.string(), Switches::Defaults{}, error), error);
    CHECK_MSG(store.SoundOn() && !store.MusicOn(), "the volume's default, the file's music");
    {
        std::ofstream out(dir / Switches::kFileName, std::ios::trunc);
        out << "{ \"version\": 1, \"audio\": { \"globalVolume\": 0.5 } }";
    }
    CHECK_MSG(store.Open(dir.string(), Switches::Defaults{}, error), error);
    CHECK_MSG(store.SoundOn() && store.MusicOn(), "a volume that is not 0 is on");
}

// Unlike scores.json: the file holds only the switches, so a toggle writes a
// good one over it - as saveValue writes back whatever it could parse.
void AnUnreadableSwitchFileIsWrittenOver() {
    const std::filesystem::path dir = Scratch("switches-corrupt");
    {
        std::ofstream out(dir / Switches::kFileName, std::ios::trunc);
        out << "{ this is not json";
    }
    Switches::Store store;
    std::string error;
    CHECK_MSG(!store.Open(dir.string(), Switches::Defaults{}, error) && !error.empty(),
              "a file that will not parse is reported");
    CHECK_MSG(store.SoundOn() && store.MusicOn() && store.Persists() && !store.FromFile(),
              "the defaults are kept, and the store can still write");
    store.SetMusic(false);
    CHECK_MSG(store.Save(error), error);
    Switches::Store reopened;
    CHECK_MSG(reopened.Open(dir.string(), Switches::Defaults{}, error) && reopened.FromFile(), error);
    CHECK_MSG(reopened.SoundOn() && !reopened.MusicOn(), "the toggle wrote a good file over it");
}

void runTests() {
    AnEmptyDirectoryTouchesNothing();
    AWorsePlayTakesNothingAway();
    MedalsSurviveTheRun();
    AFreshFolderIsAPlayerWhoHasFinishedNothing();
    AnUnreadableFileIsKeptAsABadCopyAndNeverDeleted();
    AWriteCutShortKeepsTheMedalsBeforeTheCut();
    ABadRowCostsThatRowAndNotTheFile();
    AnEmptyFileOrAnotherDocumentIsQuarantinedToo();
    AFileOutOfReachIsWaitedForNotQuarantined();
    ASaveThatFailedLeavesTheStoreAheadOfItsFileUntilOneWorks();
#if defined(_WIN32)
    AFileLockedByAnotherProgramIsMergedWhenItOpens();
#endif
    TheSwitchesStartOn();
    AnIncompleteDefaultsTableIsRefused();
    SwitchesSurviveTheRun();
    WhatTheFileDoesNotSayIsTheDefault();
    AnUnreadableSwitchFileIsWrittenOver();
}

} // namespace

MP_TEST_MAIN("test_mp_scores", 50)
