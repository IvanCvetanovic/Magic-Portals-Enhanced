// What keeps a player's saves whole, and what keeps a start from ending in silence (planning step 189).
//
// Four stores keep what a player earned or chose (scores.json, audio.json, achievements.json, controls.json). Three of them wrote
// the same way: a temporary named after the file, the stream tested BEFORE it was flushed or closed, a refused rename answered by
// REMOVING the live file and renaming again, the temporary left behind on a failure. sim/Files::WriteAtomic is the one write now
// (controls.json's from planning step 187, taken by all four), and this suite pins what it promises through the stores that use it:
//
//   - the replace is one step and the live file is never removed, whatever stands at the temporary's old fixed name;
//   - a write that cannot be put in place says so, naming the file, and leaves what was there and no temporary;
//   - the last good file is kept beside the live one as <file>.bak, and a file that failed to read NEVER replaces it;
//   - a directory to keep a log in is found without throwing when %TEMP% names nothing;
//   - which runs may put a message box up is a rule over the parsed command line, and an unattended run never does.
//
// NOT TESTED, and why: that a write the disk refuses at the last byte (a full disk, a quota, a scanner holding the file) is seen. The
// check is on fflush and fclose, which is exactly where the old writers did not look, but no portable way makes a small write fail at
// the close, so a case for it would pass whatever the code did. The write-through to the disk (_commit, fsync) cannot be observed
// from outside the process either. Both are read, not run.
//
// Free of the engine's window and of the renderer, like the rest of sim/: a scratch directory and no window.

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "RunKind.hpp"
#include "core/LaunchOptions.hpp"
#include "sim/Achievements.hpp"
#include "sim/Files.hpp"
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

// A directory of this suite's own, emptied first: a store that read what the last run left behind would pass or fail for reasons
// nothing here states.
std::filesystem::path Scratch(const char* name) {
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "supersonic-test-mp-robust" / name;
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);
    return dir;
}

std::string ReadAll(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

void Put(const std::filesystem::path& file, const std::string& text) {
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    out << text;
}

std::vector<std::string> Entries(const std::filesystem::path& dir) {
    std::vector<std::string> names;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) names.push_back(entry.path().filename().string());
    std::sort(names.begin(), names.end());
    return names;
}

using Names = std::vector<std::string>;

// ---- the write -------------------------------------------------------------------------------------------------------------------

void AWriteReplacesInOneStepAndLeavesNoTemporary() {
    const std::filesystem::path dir = Scratch("write") / "made" / "on" / "the" / "way";
    const std::string file = (dir / "data.json").string();
    std::string error;
    std::string warning;

    // A directory that does not exist yet is made: the first save of a player whose folder was never made.
    CHECK_MSG(Files::WriteAtomic(file, "first", Files::WriteOptions{}, error), "the first write: " + error);
    CHECK_MSG(ReadAll(file) == "first" && Entries(dir) == (Names{"data.json"}), "the file holds it, and nothing is beside it");
    CHECK_MSG(Files::WriteAtomic(file, "second, longer", Files::WriteOptions{}, error) && ReadAll(file) == "second, longer",
              "a second write replaces the first");
    CHECK_MSG(Files::WriteAtomic(file, "", Files::WriteOptions{}, error) && ReadAll(file).empty() && Entries(dir) == (Names{"data.json"}),
              "nothing is a thing to write too, and the file is the empty one");

    // With the backup asked for: the file that was there is kept beside it, whole.
    Files::WriteOptions backup;
    backup.keepBackup = true;
    CHECK(Files::WriteAtomic(file, "third", backup, error, &warning));
    CHECK_MSG(ReadAll(file) == "third" && ReadAll(file + ".bak").empty() && Entries(dir) == (Names{"data.json", "data.json.bak"}),
              "the copy before it (the empty one) is the .bak, and there is no temporary");
    CHECK(Files::WriteAtomic(file, "fourth", backup, error, &warning));
    CHECK_MSG(ReadAll(file + ".bak") == "third" && warning.empty(), "and the next write keeps the one before: " + warning);
    CHECK_MSG(Files::BackupPath(file) == file + ".bak", "BackupPath is the name");
}

void AWriteThatCannotBeDoneSaysSoAndThrowsNothing() {
    const std::filesystem::path dir = Scratch("cannot");
    // A regular file where the folder should be: the temporary cannot be opened, and nothing is made.
    Put(dir / "blocker", "a file");
    std::string error;
    CHECK_MSG(!Files::WriteAtomic((dir / "blocker" / "inside" / "data.json").string(), "x", Files::WriteOptions{}, error),
              "a write under a file is refused");
    CHECK_MSG(error.find("data.json") != std::string::npos, "naming the file: " + error);
    CHECK_MSG(Entries(dir) == (Names{"blocker"}) && ReadAll(dir / "blocker") == "a file", "and nothing is made or changed");

    // The target's name taken by a directory that holds something: the temporary is written, the rename onto it is refused, and the
    // temporary goes. What stood in the way is whole.
    const std::filesystem::path taken = dir / "taken.json";
    std::filesystem::create_directories(taken);
    Put(taken / "keep.txt", "kept");
    CHECK_MSG(!Files::WriteAtomic(taken.string(), "x", Files::WriteOptions{}, error) && error.find("taken.json") != std::string::npos,
              "a write that cannot be put in place says so, naming the file: " + error);
    CHECK_MSG(ReadAll(taken / "keep.txt") == "kept" && Entries(dir) == (Names{"blocker", "taken.json"}),
              "what stood in the way is whole, and no temporary is left beside it");
}

// ---- the stores ------------------------------------------------------------------------------------------------------------------

// THE CASE THE OLD CODE LOST A SAVE TO. Each store used to write "<file>.tmp", a name every writer shared. Another window's, a scanner's
// or an earlier run's copy of that name held open could not be renamed over; the old code then REMOVED the live file and renamed again
// (and failed again): the player's file was gone. The new write has a temporary of its own for every call, so what holds the old name
// is irrelevant, and is not touched. (Not mutation-run: the old code is not here to run. By reading, the old Save opened that very name
// and its second rename was the failure.)
void AnOldFixedTemporaryHeldOpenDoesNotStopASaveInAnyStore() {
    // scores.json
    {
        const std::filesystem::path dir = Scratch("held-scores");
        Scores::Store store;
        std::string error;
        CHECK_MSG(store.Open(dir.string(), error), error);
        store.Record(0, 0, Scores::kGold);
        CHECK_MSG(store.Save(error), error);
        std::ofstream holder(dir / "scores.json.tmp", std::ios::binary | std::ios::trunc);
        holder << "HELD";
        holder.flush();
        store.Record(0, 1, Scores::kSilver);
        CHECK_MSG(store.Save(error) && error.empty(), "scores.json: a leftover of the old fixed name, held open, stops nothing: " + error);
        Scores::Store reopened;
        CHECK_MSG(reopened.Open(dir.string(), error) && reopened.Count() == 2, "the live file holds both medals: " + error);
        CHECK_MSG(ReadAll(dir / "scores.json.tmp") == "HELD", "and the other writer's file is as it left it");
        CHECK_MSG(Entries(dir) == (Names{"scores.json", "scores.json.bak", "scores.json.tmp"}), "with no other temporary");
    }
    // audio.json
    {
        const std::filesystem::path dir = Scratch("held-switches");
        Switches::Store store;
        std::string error;
        CHECK_MSG(store.Open(dir.string(), Switches::Defaults{}, error), error);
        store.SetSound(false);
        CHECK_MSG(store.Save(error), error);
        std::ofstream holder(dir / "audio.json.tmp", std::ios::binary | std::ios::trunc);
        holder << "HELD";
        holder.flush();
        store.SetMusic(false);
        CHECK_MSG(store.Save(error) && error.empty(), "audio.json: a leftover of the old fixed name, held open, stops nothing: " + error);
        Switches::Store reopened;
        CHECK_MSG(reopened.Open(dir.string(), Switches::Defaults{}, error) && !reopened.SoundOn() && !reopened.MusicOn(),
                  "the live file holds both switches: " + error);
        CHECK_MSG(ReadAll(dir / "audio.json.tmp") == "HELD", "and the other writer's file is as it left it");
        CHECK_MSG(Entries(dir) == (Names{"audio.json", "audio.json.bak", "audio.json.tmp"}), "with no other temporary");
    }
    // achievements.json
    {
        Achievements::Rules rules;
        std::string error;
        CHECK_MSG(Achievements::LoadRules(std::string(MAGICPORTALS_PORT_DATA_DIR) + "/achievements.json", rules, error), error);
        const std::filesystem::path dir = Scratch("held-achievements");
        Achievements::Store store;
        CHECK_MSG(store.Open(dir.string(), rules.save, error), error);
        store.Add(45);
        CHECK_MSG(store.Save(error), error);
        const std::string name = rules.save.file;
        std::ofstream holder(dir / (name + ".tmp"), std::ios::binary | std::ios::trunc);
        holder << "HELD";
        holder.flush();
        store.Add(6);
        CHECK_MSG(store.Save(error) && error.empty(), name + ": a leftover of the old fixed name, held open, stops nothing: " + error);
        Achievements::Store reopened;
        CHECK_MSG(reopened.Open(dir.string(), rules.save, error) && reopened.Ids() == std::vector<int>({45, 6}),
                  "the live file holds both ids: " + error);
        CHECK_MSG(ReadAll(dir / (name + ".tmp")) == "HELD", "and the other writer's file is as it left it");
        CHECK_MSG(Entries(dir) == (Names{name, name + ".bak", name + ".tmp"}), "with no other temporary");
    }
}

// A save that cannot be put in place is told and loses nothing: the target's name taken by a directory that holds something.
void ASaveThatCannotBePutInPlaceIsToldAndLeavesWhatWasThere() {
    // scores.json
    {
        const std::filesystem::path dir = Scratch("blocked-scores");
        Scores::Store store;
        std::string error;
        CHECK_MSG(store.Open(dir.string(), error) && store.Persists(), error);
        store.Record(2, 3, Scores::kSilver);
        std::filesystem::create_directories(dir / "scores.json");
        Put(dir / "scores.json" / "keep.txt", "kept");
        CHECK_MSG(!store.Save(error) && error.find("scores.json") != std::string::npos, "scores.json: a save that cannot be placed says so: " + error);
        CHECK_MSG(ReadAll(dir / "scores.json" / "keep.txt") == "kept" && Entries(dir) == (Names{"scores.json"}),
                  "and what stood in the way is whole, with no temporary beside it");
        CHECK_MSG(store.Get(2, 3) == Scores::kSilver, "the medal is still held");
        std::error_code ec;
        std::filesystem::remove_all(dir / "scores.json", ec);
        CHECK_MSG(store.Save(error), "the way clear, the next save writes it: " + error);
        Scores::Store reopened;
        CHECK_MSG(reopened.Open(dir.string(), error) && reopened.Get(2, 3) == Scores::kSilver, "and it reads back: " + error);
    }
    // audio.json
    {
        const std::filesystem::path dir = Scratch("blocked-switches");
        Switches::Store store;
        std::string error;
        CHECK_MSG(store.Open(dir.string(), Switches::Defaults{}, error), error);
        store.SetSound(false);
        std::filesystem::create_directories(dir / "audio.json");
        Put(dir / "audio.json" / "keep.txt", "kept");
        CHECK_MSG(!store.Save(error) && error.find("audio.json") != std::string::npos, "audio.json: a save that cannot be placed says so: " + error);
        CHECK_MSG(ReadAll(dir / "audio.json" / "keep.txt") == "kept" && Entries(dir) == (Names{"audio.json"}),
                  "and what stood in the way is whole, with no temporary beside it");
    }
}

// THE BACKUP IS OF A FILE THAT WAS GOOD. audio.json and achievements.json are written over when they will not read (their old policy,
// unchanged), and that bad file must not become the .bak: it would replace the last good copy, the one thing the .bak is for.
void ABadFileNeverReplacesTheLastGoodBackup() {
    // audio.json
    {
        const std::filesystem::path dir = Scratch("bak-switches");
        const std::filesystem::path live = dir / "audio.json";
        std::string error;
        Switches::Store store;
        CHECK_MSG(store.Open(dir.string(), Switches::Defaults{}, error), error);
        store.SetSound(false);
        CHECK(store.Save(error));
        const std::string first = ReadAll(live);
        CHECK_MSG(Entries(dir) == (Names{"audio.json"}), "the first save has nothing to keep");
        store.SetMusic(false);
        CHECK(store.Save(error));
        CHECK_MSG(ReadAll(dir / "audio.json.bak") == first, "the second keeps the first beside it");

        // The live file goes bad (a torn write, a hand edit). A new run reads it, fails, and saves a good one over it.
        Put(live, "{ this is not json");
        Switches::Store next;
        CHECK_MSG(!next.Open(dir.string(), Switches::Defaults{}, error), "the bad file is reported (the old policy: it is written over)");
        next.SetSound(true);
        CHECK(next.Save(error));
        CHECK_MSG(ReadAll(dir / "audio.json.bak") == first,
                  "the last GOOD copy is still the .bak: the bad file did not replace it: " + ReadAll(dir / "audio.json.bak"));
        Switches::Store again;
        CHECK_MSG(again.Open(dir.string(), Switches::Defaults{}, error) && again.SoundOn(), "and the live file is the good new one: " + error);
        next.SetMusic(true);
        CHECK(next.Save(error));
        CHECK_MSG(ReadAll(dir / "audio.json.bak") != first, "once the live file is a good one again, the backup follows it");
    }
    // achievements.json
    {
        Achievements::Rules rules;
        std::string error;
        CHECK_MSG(Achievements::LoadRules(std::string(MAGICPORTALS_PORT_DATA_DIR) + "/achievements.json", rules, error), error);
        const std::filesystem::path dir = Scratch("bak-achievements");
        const std::string name = rules.save.file;
        Achievements::Store store;
        CHECK_MSG(store.Open(dir.string(), rules.save, error), error);
        store.Add(45);
        CHECK(store.Save(error));
        const std::string first = ReadAll(dir / name);
        store.Add(6);
        CHECK(store.Save(error));
        CHECK_MSG(ReadAll(dir / (name + ".bak")) == first, "the second save keeps the first beside it");

        Put(dir / name, "{ not json");
        Achievements::Store next;
        CHECK_MSG(!next.Open(dir.string(), rules.save, error), "the bad file is reported (the old policy: it is written over)");
        next.Add(7);
        CHECK(next.Save(error));
        CHECK_MSG(ReadAll(dir / (name + ".bak")) == first, "the last GOOD copy is still the .bak: " + ReadAll(dir / (name + ".bak")));

        // A file that parses but whose hash is not its list's opens EMPTY (loadList), and is no good copy either.
        Put(dir / name, "{\"version\": 1, \"achievements\": {\"list\": \"45,6\", \"hash\": \"1\"}}");
        Achievements::Store tampered;
        CHECK_MSG(tampered.Open(dir.string(), rules.save, error) && tampered.HashRefused(), "a tampered list is refused");
        tampered.Add(8);
        CHECK(tampered.Save(error));
        CHECK_MSG(ReadAll(dir / (name + ".bak")) == first, "and does not replace the last good copy either");
    }
}

// scores.json: the file the store wrote or read whole is always a good one, so the .bak follows every save.
void ScoresKeepsTheFileBeforeEachSave() {
    const std::filesystem::path dir = Scratch("bak-scores");
    Scores::Store store;
    std::string error;
    CHECK_MSG(store.Open(dir.string(), error), error);
    store.Record(0, 0, Scores::kBronze);
    CHECK(store.Save(error));
    const std::string first = ReadAll(dir / "scores.json");
    CHECK_MSG(Entries(dir) == (Names{"scores.json"}), "the first save has nothing to keep");
    store.Record(0, 0, Scores::kGold);
    CHECK(store.Save(error));
    CHECK_MSG(ReadAll(dir / "scores.json.bak") == first && ReadAll(dir / "scores.json") != first,
              "the second keeps the first: the bronze before the gold");
}

// ---- finding a place for the log ----------------------------------------------------------------------------------------------------

void ADirectoryIsFoundWithoutThrowing() {
    const std::filesystem::path root = Scratch("directory");
    const std::filesystem::path good = root / "good";
    std::filesystem::create_directories(good);
    Put(root / "afile", "x");

    // What temp_directory_path(ec) says when %TEMP% names nothing: an empty path. It is skipped.
    CHECK_MSG(Files::UsableDirectory({std::filesystem::path(), good}) == good, "an empty candidate is skipped");
    // A candidate that is a regular file, or sits under one, cannot be a directory or be made one.
    CHECK_MSG(Files::UsableDirectory({root / "afile", root / "afile" / "below", good}) == good, "a file, and a place under a file, are skipped");
    // The first that will do wins, and a later one is not touched.
    const std::filesystem::path other = root / "other";
    CHECK_MSG(Files::UsableDirectory({good, other}) == good && !std::filesystem::exists(other), "the first usable wins, and the rest are left alone");
    // One that does not exist yet is made (a profile's own folder on a first run).
    const std::filesystem::path made = root / "made" / "deeper";
    CHECK_MSG(Files::UsableDirectory({root / "afile", made}) == made && std::filesystem::is_directory(made), "a candidate that can be made is made");
    // None will do: empty, not an exception.
    CHECK_MSG(Files::UsableDirectory({}).empty() && Files::UsableDirectory({std::filesystem::path(), root / "afile"}).empty(),
              "none usable is the empty path");
}

void AFileIsReadWholeOrSaidNot() {
    const std::filesystem::path dir = Scratch("read");
    std::string text = "stale";
    std::string why;
    CHECK_MSG(Files::ReadWhole((dir / "none").string(), text, 3, 10, &why) == Files::ReadResult::Missing && text.empty(),
              "no such file is Missing, at once, and the text is cleared");
    Put(dir / "empty", "");
    CHECK_MSG(Files::ReadWhole((dir / "empty").string(), text) == Files::ReadResult::Read && text.empty(), "an empty file is Read, empty");
    Put(dir / "some", std::string("a\r\nb\0c", 6));
    CHECK_MSG(Files::ReadWhole((dir / "some").string(), text) == Files::ReadResult::Read && text == std::string("a\r\nb\0c", 6),
              "and a file is read byte for byte");

    // Something at the name that cannot be opened is Unreadable after the attempts, a wait apart. (A directory stands for it: Windows will
    // not open one as a file; this port is a Windows program and its other suites are written to that.)
    std::filesystem::create_directories(dir / "dir");
    const auto before = std::chrono::steady_clock::now();
    CHECK_MSG(Files::ReadWhole((dir / "dir").string(), text, 3, 40, &why) == Files::ReadResult::Unreadable, "a directory cannot be read");
    const auto waited = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - before).count();
    CHECK_MSG(waited >= 70 && waited < 5000, "after three tries, 40 ms apart: " + std::to_string(waited) + " ms");
    CHECK_MSG(!why.empty(), "and it says why: " + why);
    CHECK_MSG(Files::ReadWhole((dir / "dir").string(), text, 0, 1000) == Files::ReadResult::Unreadable, "a count of 0 is one try, with no wait");

#if defined(_WIN32)
    // A real lock: another program holds the file and shares nothing.
    std::FILE* holder = _wfsopen((dir / "some").c_str(), L"rb", _SH_DENYRW);
    CHECK_MSG(holder != nullptr, "the file is locked");
    if (holder != nullptr) {
        CHECK_MSG(Files::ReadWhole((dir / "some").string(), text, 2, 20, &why) == Files::ReadResult::Unreadable, "a locked file is Unreadable");
        std::fclose(holder);
        CHECK_MSG(Files::ReadWhole((dir / "some").string(), text, 2, 20, &why) == Files::ReadResult::Read, "and Read once the lock is gone");
    }
#endif
}

void ABadFileIsMovedToTheFirstFreeName() {
    const std::filesystem::path dir = Scratch("aside");
    const std::string file = (dir / "data.json").string();
    std::string moved;
    std::string error;
    Put(file, "one");
    CHECK_MSG(Files::MoveAside(file, moved, error) && moved == file + ".bad-1" && !std::filesystem::exists(file) && ReadAll(moved) == "one",
              "the first goes to -1, and the name is free: " + error);
    Put(file, "two");
    CHECK_MSG(Files::MoveAside(file, moved, error) && moved == file + ".bad-2" && ReadAll(file + ".bad-1") == "one", "the next to -2, and -1 is not replaced");
    Put(file, "three");
    std::filesystem::remove(file + ".bad-1");
    CHECK_MSG(Files::MoveAside(file, moved, error) && moved == file + ".bad-1", "a number that was freed is the first free again");
    CHECK_MSG(!Files::MoveAside((dir / "absent.json").string(), moved, error) && !error.empty() && moved.empty(),
              "a file that is not there cannot be moved, and the report says so: " + error);
}

// ---- which runs may open a message box ---------------------------------------------------------------------------------------------

Supersonic::LaunchOptions Parsed(std::vector<const char*> flags) {
    flags.insert(flags.begin(), "MagicPortals.exe");
    return Supersonic::LaunchOptions::Parse(static_cast<int>(flags.size()), flags.data());
}

// The box waits for a click, and a capture, a route, a level walk or any script that launches the exe has nobody to give it: THE ONLY WAY
// TO BLOCK AN UNATTENDED RUN is for its flag to be missing from this table. Each row is one flag, alone, on the command line it is
// parsed from; the dialog itself (MessageBoxW) is not run here, since a test cannot click it.
void OnlyAPlayersRunMayOpenAMessageBox() {
    struct Row {
        std::vector<const char*> flags;
        MagicPortals::DevFlags dev;
        bool attended;
        const char* what;
    };
    MagicPortals::DevFlags none;
    MagicPortals::DevFlags saves;
    saves.saves = true;
    MagicPortals::DevFlags route;
    route.route = true;
    MagicPortals::DevFlags visit;
    visit.visit = true;
    MagicPortals::DevFlags scripted;
    scripted.scripted = true;
    MagicPortals::DevFlags masks;
    masks.lightMasksOff = true;
    const std::vector<Row> rows = {
        {{}, none, true, "a plain double-click"},
        {{"--window", "1280x720"}, none, true, "--window: a player may size the window"},
        {{"--windowed"}, none, true, "--windowed"},
        {{"--fullscreen"}, none, true, "--fullscreen"},
        {{"--frames", "30"}, none, false, "--frames: a capture"},
        {{"--fixed-step"}, none, false, "--fixed-step"},
        {{"--fixed-step", "0.02"}, none, false, "--fixed-step with a step"},
        {{"--screenshot", "x.png"}, none, false, "--screenshot"},
        {{"--screenshot-ui", "x.png"}, none, false, "--screenshot-ui"},
        {{"--frames", "10", "--screenshot", "x.png", "--screenshot-every", "5"}, none, false, "--screenshot-every"},
        {{"--hidden"}, none, false, "--hidden"},
        {{"--import-assets"}, none, false, "--import-assets"},
        {{"--record", "x.rec"}, none, false, "--record"},
        {{"--replay", "x.rec"}, none, false, "--replay"},
        {{}, saves, false, "--saves: the medals somewhere else"},
        {{}, route, false, "--route"},
        {{}, visit, false, "--visit-levels"},
        {{}, scripted, false, "--press, --hold, --tap, --drag, --burn-dragon or --place"},
        {{}, masks, false, "--light-masks-off"},
    };
    for (const Row& row : rows) {
        const Supersonic::LaunchOptions options = Parsed(row.flags);
        CHECK_MSG(options.ok, std::string(row.what) + ": the flags parse: " + options.error);
        CHECK_MSG(MagicPortals::AttendedRun(options, row.dev) == row.attended,
                  std::string(row.what) + (row.attended ? ": is a player's run" : ": is unattended, and never opens a message box"));
    }
    // Every combination of the port's own flags with an engine one stays unattended: one flag is enough.
    MagicPortals::DevFlags all;
    all.saves = all.route = all.visit = all.scripted = all.lightMasksOff = true;
    CHECK_MSG(!MagicPortals::AttendedRun(Parsed({"--frames", "1"}), all) && !MagicPortals::AttendedRun(Parsed({}), all),
              "the flags together are unattended");
}

// The box a run that stopped on an error shows (planning step 191; the rule was main.cpp's own and no suite reached it): none for a run nobody is at,
// and for a player's run one that says what happened and names the log ONLY if the log was written - a path handed over before the file sink opened
// would send a player to a file that is not there.
void TheFatalBoxNamesTheLogOnlyIfItWasWritten() {
    const std::string reason = "the level could not be started";
    const std::string log = "C:\\Users\\someone\\AppData\\Local\\Temp\\magicportals.log";
    CHECK_MSG(MagicPortals::FatalText(reason, log, false).empty() && MagicPortals::FatalText(reason, "", false).empty(),
              "a run nobody is at opens no box, with a log or without");
    const std::string written = MagicPortals::FatalText(reason, log, true);
    CHECK_MSG(written.find(reason) != std::string::npos, "a player's box says what happened: " + written);
    CHECK_MSG(written.find("The details are in the log:") != std::string::npos && written.find(log) != std::string::npos,
              "and names the log that was written: " + written);
    const std::string unwritten = MagicPortals::FatalText(reason, "", true);
    CHECK_MSG(unwritten.find(reason) != std::string::npos, "with no log written it still says what happened: " + unwritten);
    CHECK_MSG(unwritten.find("log") == std::string::npos && unwritten.find("details") == std::string::npos,
              "and names no log, there being none: " + unwritten);
    CHECK_MSG(written.rfind(unwritten, 0) == 0 && written.size() > unwritten.size(), "the log line is an addition to the same text");
}

void runTests() {
    AWriteReplacesInOneStepAndLeavesNoTemporary();
    AWriteThatCannotBeDoneSaysSoAndThrowsNothing();
    AnOldFixedTemporaryHeldOpenDoesNotStopASaveInAnyStore();
    ASaveThatCannotBePutInPlaceIsToldAndLeavesWhatWasThere();
    ABadFileNeverReplacesTheLastGoodBackup();
    ScoresKeepsTheFileBeforeEachSave();
    ADirectoryIsFoundWithoutThrowing();
    AFileIsReadWholeOrSaidNot();
    ABadFileIsMovedToTheFirstFreeName();
    OnlyAPlayersRunMayOpenAMessageBox();
    TheFatalBoxNamesTheLogOnlyIfItWasWritten();
}

} // namespace

MP_TEST_MAIN("test_mp_robust", 100)
