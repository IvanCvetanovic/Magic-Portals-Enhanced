#pragma once

// THE FIRST-RUN UNPACK (planning step 203, PORT-5; the platform research's F5, PL-08).
//
// WHY IT EXISTS. An Android app's data is inside its APK, and the engine cannot read an APK: every open in the engine and in this game is by
// path (`grep AAssetManager engine/src` finds nothing). So on its first start the app copies the game's data out of the APK into its own private
// folder, once, and the game is then pointed at the copy with --root, as a desktop package is. This file is ALL of that which is not a call into
// Android: the format of the list the APK carries, the rules a name in it must pass, the decision whether to unpack at all, the copy, and the
// order in which things are written. It is PURE: the APK and the folder are handed in as a Source and a Sink, so the suite builds a hostile list,
// a source that dies at file k and a disk that fills, with no phone and no APK. game/android/AndroidMain.cpp is the glue: AAssetManager as the
// Source, FsSink (below) as the Sink.
//
// THE LIST (the packager writes it, tools/package/android_package.py: `assets/gamedata.list` in the APK).
//     line 1      the STAMP: letters, digits, '.', '_' and '-' only, not empty. The packager makes it from the size and the sha256 of every
//                 listed file, so it moves when any byte of the data moves.
//     line 2...   `<size in bytes, decimal digits><TAB><path>` for each file; the path is relative to the app's files folder.
//     Blank lines are skipped and a CR before the LF is tolerated. There is NO comment syntax (a file name may begin with anything).
//   It is data from an APK, and a corrupt or hostile one must not write outside the folders the unpack owns, so every path is held to SafePath.
//
// THE ORDER IS THE POINT.
//   1. the stored stamp equals the list's AND every listed file is there at the listed size  -> nothing is done (UpToDate).
//      A stamp with a file missing or short means somebody (the system's cleaner, a full disk, a user) damaged the copy: it is made again.
//   2. otherwise FREE SPACE IS CHECKED FIRST, while the old copy is still whole: the new copy needs its bytes, of which the old copy's own are
//      free to be taken (they are about to be removed), and a disk that cannot hold the difference fails the start with the old copy and its stamp
//      untouched (planning step 204, PORT5-10 (b): the check used to come after the old copy was gone). Then the stored stamp is REMOVED, then the
//      folders the unpack owns, then every file is copied, and the stamp is written LAST. A process killed at any point from the first removal on
//      leaves no stamp, so the next start unpacks again from the top; a stamp that exists is therefore the word of a copy that finished.
//   3. the copy is checked against the list file by file: a source that gives more or fewer bytes than the list says is a failure, and the
//      half-written file is removed. The check for MORE does not wait for the end: the chunk that would pass the listed size is refused, and the
//      source is told to stop (planning step 204: an asset of any length used to be read to its end first).
//   4. the event pump is called after every `pumpEvery` files (the platform holds its UI thread until the game thread reads its events, and a
//      thread that does not for about five seconds is "not responding"). If the pump says the app was asked to finish, the unpack STOPS: no
//      further file, no stamp, and the outcome is Cancelled, which is not a failure and must not be told to the player as one.
//
// WHAT IT WILL NOT DO: write a file outside the owned folders (the player's saves, the log, the developer's flags file and the stamp itself are
// all beside them, not in them), follow a name that climbs, or take a list with two lines for one path.

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace MagicPortals::Unpack {

// What the packager and this agree on. The packager (tools/package/android_package.py) spells the first three too, because it is Python; the tie is
// tools/package/test_android_package.py, which reads those lines out of this file and holds the packager's to them, and the list the packager makes
// for invented files is parsed by this code in test_mp_unpack (planning step 204, D2).
inline constexpr char kSizeSeparator = '\t';
// A size in the list is at most this many decimal digits: 10^15 bytes is a petabyte, far past any APK.
inline constexpr std::size_t kMostSizeDigits = 15;
// The most a stamp may be: a sha256 in hex is 64.
inline constexpr std::size_t kMostStampChars = 128;
// The most the TEXT of the list may be, in bytes, enforced where its bytes arrive (AppendListChunk) and again by Run on whatever a Source handed back.
// The real list is about 70 bytes a file, so 900 files are about 63 KB; this is 4 MiB.
inline constexpr std::size_t kMostListTextBytes = 4 * 1024 * 1024;
// How much of an asset the glue reads from the APK at a time.
inline constexpr std::size_t kReadChunkBytes = 64 * 1024;
// How many files between two calls of the pump. 16 is the sibling port's number (its AndroidMain, which pumps every 16 and says why: the
// activity's UI thread waits for this thread to read its lifecycle commands).
inline constexpr std::size_t kPumpEveryFiles = 16;
// The allocation unit the free-space estimate rounds each file up to: the page and block size of Android's file systems. An estimate, and stated as
// one in the message the player reads.
inline constexpr std::uint64_t kBlockBytes = 4096;

struct Entry {
    std::string path;       // relative to the destination folder, '/'-separated, one of the owned folders first
    std::uint64_t size = 0;
};

struct List {
    std::string stamp;
    std::vector<Entry> entries;
    std::uint64_t bytes = 0; // the sum of the sizes
};

struct Parsed {
    bool ok = false;
    List list;
    std::string error; // in words, naming the line
};

// Whether `path` may be written under the destination. `ownedRoots` are the folder names the unpack owns; the first segment must be one of them.
// Refused, each with its own words in `why`: empty; a NUL or another control character (a TAB would break the list's own format); a backslash; a
// colon (a drive letter, or a stream name on Windows); a leading '/'; a trailing '/'; an empty segment ("a//b"); a "." segment; a ".." segment;
// a first segment that is not an owned folder; a path that is only an owned folder's name (nothing under it).
bool SafePath(const std::string& path, const std::vector<std::string>& ownedRoots, std::string& why);

// The list text, parsed. Ends on text with no final newline. Never throws.
// THE SUM OF THE SIZES MUST FIT 64 BITS, ROUNDED UP TO BLOCKS (the allocation estimate, which is what the free-space check compares); a list for which
// it would not is refused, naming the line (planning step 204, D4). The verifier of step 203 made 18,446 lines of 999,999,999,999,999 bytes and one of
// 744,073,709,570,061: they add to exactly 2^64 - 1 as they are, which the old test (the plain sum against 64 bits) let through, and rounded up to blocks
// they add to 2^64 + 20,480, which the estimate wrapped to 20,480 bytes, so the free-space check was skipped. The rounded sum is never below the plain one
// for a size of at most kMostSizeDigits digits, so one check bounds both.
Parsed ParseList(const std::string& text, const std::vector<std::string>& ownedRoots);

// What the copy will take from the disk, as an estimate: each file's size rounded up to the allocation unit, added up. SATURATING: a list built by hand
// whose estimate would pass 64 bits answers the largest number, never a small one (ParseList refuses such a list, so it cannot reach this; the second
// guard is for a List made some other way).
std::uint64_t NeededBytes(const List& list);

// `size` rounded up to a whole number of allocation units, saturating at the largest 64-bit multiple instead of wrapping.
std::uint64_t RoundUpToBlocks(std::uint64_t size);

// Appends a piece of the list's text, or refuses (false, with `why`) the piece that would take the text past kMostListTextBytes, appending nothing of it.
// What a Source's ReadList calls for each chunk it reads, so the cap is enforced as the bytes arrive and a hostile APK cannot make the reader hold a
// list of any length.
bool AppendListChunk(std::string& text, const char* data, std::size_t size, std::string& why);

// Whole megabytes for a sentence the player reads. A NEED is rounded UP and a FREE figure DOWN, so the sentence never says the phone has the space
// it lacks: "needs about 24 MB and 24 MB are free" cannot be printed. Never says "0 MB" of a need, and says "less than 1 MB" of a free figure under one.
std::string MegabytesNeeded(std::uint64_t bytes);
std::string MegabytesFree(std::uint64_t bytes);

// ---- The two sides ---------------------------------------------------------------------------------------------------------------------

// The APK. `chunk` is handed the bytes of an asset in pieces and answers false to stop (a full disk); ReadAsset then returns false.
using ChunkFn = std::function<bool(const char* data, std::size_t size)>;

class Source {
public:
    virtual ~Source() = default;
    // The list's text. A source that reads it piece by piece appends each piece with AppendListChunk, which stops at kMostListTextBytes; Run refuses
    // a text longer than that whatever the source did.
    virtual bool ReadList(std::string& text, std::string& why) = 0;
    virtual bool ReadAsset(const std::string& path, const ChunkFn& chunk, std::string& why) = 0;
};

// The folder the data is copied into, and the place the stamp is kept. `path` is always a path SafePath passed.
class Sink {
public:
    virtual ~Sink() = default;
    virtual bool ReadStamp(std::string& stamp) = 0;                         // false: there is none
    virtual bool WriteStamp(const std::string& stamp, std::string& why) = 0;
    virtual bool RemoveStamp(std::string& why) = 0;                         // true also when there was none
    virtual bool RemoveTree(const std::string& root, std::string& why) = 0; // true also when it was not there
    virtual bool SizeOf(const std::string& path, std::uint64_t& size) = 0;  // false: no such file
    virtual std::optional<std::uint64_t> FreeBytes() = 0;                   // nullopt: not known, and then nothing is refused for it
    // What the folder `root` holds now, in the same unit NeededBytes uses (each file rounded up to a block): the space the removal of the old copy gives
    // back. nullopt: not known, and then none is counted as given back.
    virtual std::optional<std::uint64_t> HeldBytes(const std::string& root) = 0;
    virtual bool Begin(const std::string& path, std::string& why) = 0;      // makes the folders, opens the file for writing from nothing
    virtual bool Write(const char* data, std::size_t size, std::string& why) = 0;
    virtual bool End(bool keep, std::string& why) = 0;                      // closes; with keep false the half-written file is removed
};

// ---- The run ---------------------------------------------------------------------------------------------------------------------------

enum class Outcome {
    UpToDate,  // the stamp and every file were as the list says; nothing was written
    Done,      // unpacked, and the stamp written
    Cancelled, // the pump said the app was asked to finish; no stamp; NOT a failure
    Failed,    // `error` says why, in words for a player
};

struct Options {
    std::vector<std::string> ownedRoots;     // the folders the unpack owns: removed before a copy, and the first segment of every path
    std::size_t pumpEvery = kPumpEveryFiles; // 0: never pump
    std::function<bool()> pump;              // false: the app is to finish
    // Called at each pump point and at the end: files copied so far of the whole, bytes so far. For a log line; the platform has no window to show it in.
    std::function<void(std::size_t done, std::size_t total, std::uint64_t bytes)> progress;
};

struct Result {
    Outcome outcome = Outcome::Failed;
    std::string error;
    std::string stamp;           // the list's, once it was read
    std::size_t files = 0;       // listed
    std::uint64_t bytes = 0;     // listed
    std::size_t copied = 0;      // files written by this call
    std::uint64_t copiedBytes = 0;
    std::size_t pumps = 0;       // times the pump was called
    std::uint64_t needed = 0;    // the free-space estimate (NeededBytes)
    std::uint64_t reclaimable = 0; // what the old copy holds and its removal gives back (Sink::HeldBytes), counted before the check
};

Result Run(Source& source, Sink& sink, const Options& options);

// ---- The folder on a disk --------------------------------------------------------------------------------------------------------------

// A Sink over std::filesystem, portable: the app's files folder on a phone, a temporary folder in a suite. The stamp is a file of its own beside the
// owned folders (never inside one: the unpack removes those), written through Files::WriteAtomic, so a kill during its write cannot leave a stamp
// that is a different, complete-looking word.
class FsSink : public Sink {
public:
    FsSink(std::string root, std::string stampName);
    ~FsSink() override;
    bool ReadStamp(std::string& stamp) override;
    bool WriteStamp(const std::string& stamp, std::string& why) override;
    bool RemoveStamp(std::string& why) override;
    bool RemoveTree(const std::string& root, std::string& why) override;
    bool SizeOf(const std::string& path, std::uint64_t& size) override;
    std::optional<std::uint64_t> FreeBytes() override;
    std::optional<std::uint64_t> HeldBytes(const std::string& root) override;
    bool Begin(const std::string& path, std::string& why) override;
    bool Write(const char* data, std::size_t size, std::string& why) override;
    bool End(bool keep, std::string& why) override;

private:
    std::string root_;
    std::string stampName_;
    std::string openPath_;
    std::ofstream out_;
};

// ---- The developer's flags file --------------------------------------------------------------------------------------------------------

// `magicportals_args.txt` in the app's files folder: a phone has no command line, so a scripted run leaves its flags here. WHITESPACE-SEPARATED:
// one flag per line, or several on a line, as the writer likes; a token with a space in it is wrapped in double quotes (the quotes are not part of
// it, and "" is an empty token). There is no escape and no comment. An unclosed quote, or a NUL, is an error and the file is not used.
struct ArgsParsed {
    bool ok = false;
    std::vector<std::string> args;
    std::string error;
};
ArgsParsed ParseArgsText(const std::string& text);

// ---- What the Android entry decides ----------------------------------------------------------------------------------------------------
// Planning step 204 (D3 of the verification of step 203): these rules were in game/android/AndroidMain.cpp, which no host compiler builds and no suite
// reaches, so they were read and never run. They are here, over a folder, and AndroidMain.cpp only calls them with Android's answers.

// The developer's flags file TAKEN: read, parsed and consumed.
struct FlagsFile {
    bool present = false;       // a file of that name stood in the folder: the run is a SCRIPTED one, whether or not it could be read or understood
    std::vector<std::string> args;
    std::string info;           // for the log, when flags were taken: "Flags from <file>: a b c"
    std::string error;          // the file was ignored (it could not be read, or does not parse): why
    std::string warning;        // the file could not be renamed, so the next launch reads it again: why
};

// Reads a whole file. false: it could not be read (the file exists and is not for this process, or is held, or is not a file at all).
using ReadFileFn = std::function<bool(const std::string& path, std::string& text)>;

// `folder`/`name`, if it is a regular file, is read (with `read`, else Files::ReadWhole), parsed (ParseArgsText) and RENAMED to `folder`/`usedName`, replacing
// an earlier one, whatever it held: a broken file does not repeat either, and a scripted run does not become every later launch. A file that is not
// there, or is not a regular file, is `present` false and nothing else. A file that cannot be read is present, ignored, consumed and says why. Never throws.
FlagsFile TakeFlagsFile(const std::string& folder, const std::string& name, const std::string& usedName, const ReadFileFn& read = {});

// Whether a start that stopped on a failure is told to the player in a dialog: a player's own launch is, a scripted one (a flags file was there) is not,
// because nobody is at it to close the dialog.
bool ShowsFailureDialog(bool scripted);

// What the entry does after an unpack: go on to the game, end quietly (the app was closed: not a failure), or end with an error.
enum class Next { Continue, ExitSuccess, ExitFailure };
Next NextAfter(Outcome outcome);

// The line for the log that says how an unpack that did not fail went (counts and the stamp), and what a cancel was.
std::string Summary(const Result& result);

// The argument list the game's main is called with: the program's name, `--root` and the folder the unpacked data is in, then the developer's flags.
// A missing or empty program name is kDefaultProgramName.
inline constexpr const char* kDefaultProgramName = "MagicPortals";
std::vector<std::string> LaunchArguments(const char* argv0, const std::string& root, const std::vector<std::string>& flags);

// The log of a start that stopped before the game could open its own (a failed unpack): the last log moved to `previousPath` (Scratch::KeepPreviousLog),
// then `text` written to `logPath` in one step (the folders made). Without it a failed start would be in logcat only and the report Android shows at the
// next start (supersonic.reportLog in the manifest) would show the last start that got as far as the game (PORT5-10 (a)). The game's own log is not
// opened here: its sink truncates, and the game rotates and opens it itself. False, with `why`, when it could not be written; never throws.
bool RecordFailure(const std::string& logPath, const std::string& previousPath, const std::string& text, std::string& why);

} // namespace MagicPortals::Unpack
