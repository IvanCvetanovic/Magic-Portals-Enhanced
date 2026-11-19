// The first-run unpack (planning step 203, PORT-5): sim/Unpack.hpp, everything the Android app does with its APK's data that is not a call into Android.
//
// WHAT IS PINNED, over invented lists, an in-memory APK and an in-memory disk (no original file, no binary, no phone):
//   - the list's format: the stamp line, `size TAB path` lines, blank lines, CRLF, no comment syntax, a text with no final newline, and every way a line can be wrong;
//   - SafePath: every refusal on its own (empty, NUL and other control characters, backslash, colon, absolute, trailing slash, empty segment, "." and "..", a
//     first segment that is not an owned folder, an owned folder's bare name), and the names that must pass;
//   - the ORDER of a run: the stamp is removed first, the owned folders next, space is checked, every file is copied, the stamp is written LAST; a source that dies
//     at file k leaves no stamp and no half file, and a second call completes; a stamp left by an earlier copy does not survive a failed re-copy;
//   - the decision to do nothing: the same stamp and every listed file there at its size is UpToDate; the same stamp with a file missing or short is not;
//   - the copy is checked against the list (more or fewer bytes than listed fails and removes the half file);
//   - the pump: how often, after which file, none when the cadence is 0, and a pump that says "finish" stops the unpack as CANCELLED -- no further file, no stamp,
//     no error text (it is not a failure and the player must not be told it was);
//   - free space: from the list's sizes in allocation blocks, refused one byte short, accepted exactly, and not asked-of when the disk cannot say;
//   - a hostile list never touches the disk (not even the removal of the old copy);
//   - FsSink on a real folder: the files and the stamp are the bytes, the player's own files beside the owned folders survive a re-copy;
//   - the developer's flags file: whitespace-separated, double quotes, the two errors.
// Planning step 204 added, each from a finding of the verification of step 203 or from the first run on an emulator:
//   - D4: a list whose sizes, rounded up to blocks, would wrap 64 bits is refused when it is read (the verifier's own probe), and the estimate saturates;
//   - D5/D6: the list's text is capped where its bytes arrive, an asset past its listed size is stopped at the chunk that passes it, and the free-space
//     sentence rounds the need up and the free figure down;
//   - D1: every call of the sink that can fail (RemoveStamp, RemoveTree, Begin, End(keep), WriteStamp) ends the run, says why, and stops what follows;
//   - PORT5-10 (b): space is checked before anything is cleared, and the old copy's own bytes count as given back;
//   - D2: the list the PACKAGER makes for four invented files (tools/package/test_android_package.py regenerates it) is parsed and unpacked;
//   - D3: the flags file taken (read once, consumed, unreadable, unparsable, a rename that fails), the dialog rule, the exit after the unpack, the argument
//     list, and PORT5-10 (a), the log written by a start that stopped in the unpack.
// ENHANCEMENT E49 (planning step 260, the sibling game's fix for a first start that the phone's system stops, again and again, before it finishes) added the
// RESUME MARKER, pinned over MarkerSink (a sink that keeps one) and FsSink (a real folder), while MemSink stays a sink that knows none, so that every check
// above and below that is made over MemSink is also a check that such a sink gets what Run did before: all or nothing.
//   - a start stopped at EVERY file (the source dies, the pump says finish), and with the stamp that cannot be written, is continued: the files whole are kept
//     (counted from the disk, not from the run), only the rest are copied, the tree equals an uninterrupted unpack's byte for byte, the stamp is there and
//     the marker is not;
//   - the process KILLED before every mutating call of a start (a sink that throws, so that nothing in Run tidies up: a half file stays under its name), over
//     an empty disk and over a finished tree of another build with files of the same sizes, then the next start (and the one after a second kill) ends in the
//     right tree, and a start of the other build ends in its own;
//   - a marker of another stamp is a clean start (a file of the right size from the other build is never kept), a file short, long, empty or missing under the
//     right marker is copied again, a sink without a marker starts from the top, the free-space check serves a resume, and a marker left beside a finished copy
//     is removed without failing anything;
//   - FsSink on a real folder: the marker is a file beside the stamp and outside the owned folders, it is there exactly while the unpack is unfinished, and a
//     second sink over the same folder (a second process) continues what the first left.
//   The "mutation:" lines of this E49 part were written by reasoning; ten of them were then applied one at a time to sim/Unpack.cpp and the suite was
//   run (step 260's record has the list): nine were caught on the first run, and the one that was not (a kept file not counting as in place for the pump)
//   is pinned now by AContinuationAsksThePumpAndTheProgressForKeptFilesToo. The others were not run.
// Each pin says in "mutation:" the change of sim/Unpack.cpp that turns it red; the Step records that they were run.
//
// NOT TESTED, and why: AAssetManager and the dialog and the real pump (game/android/AndroidMain.cpp is the thin glue over them; step 204 ran them on an
// emulator, which proves startup and the code paths and nothing about a phone). Free of the engine and of the renderer; the only files are a scratch folder's.

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "sim/Files.hpp"
#include "sim/Unpack.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

using namespace MagicPortals;
namespace fs = std::filesystem;

namespace {

bool Has(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

const std::vector<std::string> kOwned = {"GameData", "engine"};

// ---- An invented APK --------------------------------------------------------------------------------------------------------------------

struct Mem {
    std::string path;
    std::string bytes;
};

// The list text for invented files: the stamp, then `size TAB path` for each.
std::string ListFor(const std::string& stamp, const std::vector<Mem>& files, const char* newline = "\n") {
    std::string text = stamp + newline;
    for (const Mem& file : files) text += std::to_string(file.bytes.size()) + "\t" + file.path + newline;
    return text;
}

class MemSource : public Unpack::Source {
public:
    std::string list;
    bool listReadable = true;
    std::map<std::string, std::string> assets;
    std::string failAt;      // an asset that gives half its bytes and then fails
    std::string shortBy;     // an asset that gives one byte fewer than the list says
    std::string longBy;      // an asset that gives one byte more
    std::string endless;     // an asset that never ends: it gives chunks until it is told to stop (at most kMostChunks, so a reader that never stops fails a check)
    std::size_t chunk = 5;
    int reads = 0;
    std::size_t chunksGiven = 0; // chunks handed to the reader, over all assets
    std::size_t endlessChunks = 0; // of them, the ones of the endless asset
    static constexpr std::size_t kMostChunks = 100000;
    std::function<void(const std::string&)> onRead; // called with the path at the start of every asset read: a test looks at the disk while the unpack runs

    bool ReadList(std::string& text, std::string& why) override {
        if (!listReadable) {
            why = "no gamedata.list";
            return false;
        }
        text = list;
        return true;
    }
    bool ReadAsset(const std::string& path, const Unpack::ChunkFn& give, std::string& why) override {
        ++reads;
        if (onRead) onRead(path);
        const auto found = assets.find(path);
        if (found == assets.end()) {
            why = "no such asset";
            return false;
        }
        if (path == endless) {
            for (std::size_t i = 0; i < kMostChunks; ++i) {
                ++chunksGiven;
                ++endlessChunks;
                if (!give("abcde", 5)) {
                    why = "stopped";
                    return false;
                }
            }
            return true;
        }
        std::string bytes = found->second;
        if (path == shortBy && !bytes.empty()) bytes.pop_back();
        if (path == longBy) bytes += "X";
        const std::size_t stop = path == failAt ? bytes.size() / 2 : bytes.size();
        for (std::size_t at = 0; at < stop; at += chunk) {
            ++chunksGiven;
            if (!give(bytes.data() + at, std::min(chunk, stop - at))) {
                why = "stopped";
                return false;
            }
        }
        if (path == failAt) {
            why = "the APK could not be read";
            return false;
        }
        return true;
    }
};

// An in-memory disk that remembers everything done to it, in order.
class MemSink : public Unpack::Sink {
public:
    std::map<std::string, std::string> files;
    std::optional<std::string> stamp;
    std::vector<std::string> ops;
    std::optional<std::uint64_t> free = std::uint64_t{1} << 40;
    std::uint64_t diskHolds = ~std::uint64_t{0}; // bytes in all the disk takes before Write fails (a full disk)
    std::uint64_t written = 0;
    bool stampPresentAtABegin = false; // a stamp that was there when a file was begun: the stamp was not out of the way
    bool stampRemovalFails = false;
    std::string treeRemovalFails;        // the owned folder whose removal fails
    std::string beginFails;              // the file whose Begin fails
    std::string endKeepFails;            // the file whose End(keep) fails
    bool writeStampFails = false;
    bool heldUnknown = false;            // HeldBytes cannot say
    std::uint64_t peak = 0;              // the most bytes any one open file held
    std::string open;

    bool ReadStamp(std::string& s) override {
        if (!stamp) return false;
        s = *stamp;
        return true;
    }
    bool WriteStamp(const std::string& s, std::string& why) override {
        ops.push_back("WriteStamp");
        if (writeStampFails) {
            why = "the stamp's file is read-only";
            return false;
        }
        stamp = s;
        return true;
    }
    bool RemoveStamp(std::string& why) override {
        ops.push_back("RemoveStamp");
        if (stampRemovalFails) {
            why = "read-only";
            return false;
        }
        stamp.reset();
        return true;
    }
    bool RemoveTree(const std::string& root, std::string& why) override {
        ops.push_back("RemoveTree:" + root);
        if (root == treeRemovalFails) {
            why = root + " is held by another process";
            return false;
        }
        for (auto it = files.begin(); it != files.end();) it = it->first.compare(0, root.size() + 1, root + "/") == 0 ? files.erase(it) : std::next(it);
        return true;
    }
    bool SizeOf(const std::string& path, std::uint64_t& size) override {
        const auto found = files.find(path);
        if (found == files.end()) return false;
        size = found->second.size();
        return true;
    }
    std::optional<std::uint64_t> FreeBytes() override {
        ops.push_back("FreeBytes");
        return free;
    }
    std::optional<std::uint64_t> HeldBytes(const std::string& root) override {
        ops.push_back("HeldBytes:" + root);
        if (heldUnknown) return std::nullopt;
        std::uint64_t total = 0;
        for (const auto& [path, bytes] : files)
            if (path.compare(0, root.size() + 1, root + "/") == 0) total += Unpack::RoundUpToBlocks(bytes.size());
        return total;
    }
    bool Begin(const std::string& path, std::string& why) override {
        ops.push_back("Begin:" + path);
        if (path == beginFails) {
            why = "cannot open the file for writing";
            return false;
        }
        if (stamp) stampPresentAtABegin = true;
        files[path].clear();
        open = path;
        return true;
    }
    bool Write(const char* data, std::size_t size, std::string& why) override {
        if (written + size > diskHolds) {
            why = "no space left on the disk";
            return false;
        }
        written += size;
        files[open].append(data, size);
        peak = std::max<std::uint64_t>(peak, files[open].size());
        return true;
    }
    bool End(bool keep, std::string& why) override {
        ops.push_back(std::string("End:") + open + (keep ? ":keep" : ":drop"));
        const bool fails = keep && open == endKeepFails;
        if (!keep || fails) files.erase(open);
        open.clear();
        if (fails) {
            why = "the disk gave up at the close";
            return false;
        }
        return true;
    }

    bool Did(const std::string& op) const { return std::find(ops.begin(), ops.end(), op) != ops.end(); }
    std::size_t Count(const std::string& prefix) const {
        return static_cast<std::size_t>(std::count_if(ops.begin(), ops.end(), [&](const std::string& op) { return op.compare(0, prefix.size(), prefix) == 0; }));
    }
    std::ptrdiff_t IndexOf(const std::string& op) const {
        const auto found = std::find(ops.begin(), ops.end(), op);
        return found == ops.end() ? -1 : found - ops.begin();
    }
};

// What the phone's system does to a first start, and a power cut does to any: the process is gone BETWEEN two calls, with no cleanup. A test throws it from a
// sink and catches it outside Run, which catches nothing, so nothing in Run gets to tidy up: a file half written stays under its own name, as on a phone.
struct Killed {};

// An in-memory disk that keeps the RESUME MARKER as FsSink does (E49), and records how Run uses it. MemSink is left without one on purpose: its three marker
// calls are the base class's defaults, so a check over MemSink is a check of a sink that does not know the marker.
class MarkerSink : public MemSink {
public:
    static constexpr std::size_t kNever = ~std::size_t{0};

    std::optional<std::string> marker;
    bool markerWriteFails = false;
    bool markerRemovalFails = false;     // a marker that STANDS cannot be removed (removing none is nothing, as on a real disk)
    bool markerReadBack = true;          // false: the marker is written and cannot be read again
    std::size_t killAt = kNever;         // the mutating call, counted in `mutations`, that never happens: the process is gone before it
    std::size_t mutations = 0;           // mutating calls made so far: RemoveStamp, RemoveMarker, RemoveTree, WriteMarker, Begin, every Write, End, WriteStamp
    std::set<std::string> markersAtABegin;   // the marker that stood at each Begin ("<none>" for no marker)
    bool treeRemovedWhileStamped = false;    // an owned folder was removed while a stamp stood
    bool treeRemovedWhileMarked = false;     // an owned folder was removed while a marker stood

    bool ReadMarker(std::string& m) override {
        if (!marker || !markerReadBack) return false;
        m = *marker;
        return true;
    }
    bool WriteMarker(const std::string& m, std::string& why) override {
        Mutating();
        ops.push_back("WriteMarker");
        if (markerWriteFails) {
            why = "the marker's file is read-only";
            return false;
        }
        marker = m;
        return true;
    }
    bool RemoveMarker(std::string& why) override {
        Mutating();
        ops.push_back("RemoveMarker");
        if (markerRemovalFails && marker) {
            why = "the marker's file is held by another process";
            return false;
        }
        marker.reset();
        return true;
    }
    bool RemoveStamp(std::string& why) override {
        Mutating();
        return MemSink::RemoveStamp(why);
    }
    bool WriteStamp(const std::string& s, std::string& why) override {
        Mutating();
        return MemSink::WriteStamp(s, why);
    }
    bool RemoveTree(const std::string& root, std::string& why) override {
        Mutating();
        if (stamp) treeRemovedWhileStamped = true;
        if (marker) treeRemovedWhileMarked = true;
        return MemSink::RemoveTree(root, why);
    }
    bool Begin(const std::string& path, std::string& why) override {
        Mutating();
        markersAtABegin.insert(marker ? *marker : std::string("<none>"));
        return MemSink::Begin(path, why);
    }
    bool Write(const char* data, std::size_t size, std::string& why) override {
        Mutating();
        return MemSink::Write(data, size, why);
    }
    bool End(bool keep, std::string& why) override {
        Mutating();
        return MemSink::End(keep, why);
    }

    // The next start, with nothing killed: the same disk, the counters of this life cleared.
    void NextLife() {
        killAt = kNever;
        mutations = 0;
        ops.clear();
        stampPresentAtABegin = false;
        markersAtABegin.clear();
        treeRemovedWhileStamped = false;
        treeRemovedWhileMarked = false;
    }

private:
    void Mutating() {
        if (mutations++ == killAt) throw Killed{};
    }
};

// A tidy invented set: `n` files under the two owned folders, each a few bytes of its own.
std::vector<Mem> Invented(int n) {
    std::vector<Mem> files;
    for (int i = 0; i < n; ++i) {
        const std::string root = i % 5 == 4 ? "engine/assets/shaders" : "GameData/part" + std::to_string(i % 3);
        files.push_back({root + "/file" + std::to_string(i) + ".dat", std::string(static_cast<std::size_t>(3 + i % 11), static_cast<char>('a' + i % 26)) + std::to_string(i)});
    }
    return files;
}

MemSource SourceFor(const std::string& stamp, const std::vector<Mem>& files) {
    MemSource source;
    source.list = ListFor(stamp, files);
    for (const Mem& file : files) source.assets[file.path] = file.bytes;
    return source;
}

Unpack::Options Plain() {
    Unpack::Options options;
    options.ownedRoots = kOwned;
    return options;
}

// ---- What the resume marker's suites compare against (E49) ------------------------------------------------------------------------------------
// The oracle is never the run under test: it is the invented files themselves, and what a finished unpack leaves.

// Empty when `sink` holds exactly `files` (every listed path with its bytes, and nothing else), the stamp `stamp` and no marker; else what is wrong.
std::string TreeProblem(const MarkerSink& sink, const std::vector<Mem>& files, const std::string& stamp) {
    if (sink.files.size() != files.size()) return "the disk holds " + std::to_string(sink.files.size()) + " files, the list has " + std::to_string(files.size());
    for (const Mem& file : files) {
        const auto found = sink.files.find(file.path);
        if (found == sink.files.end()) return file.path + " is missing";
        if (found->second != file.bytes) return file.path + " has other bytes than the list's";
    }
    if (!sink.stamp || *sink.stamp != stamp) return "the stamp is not " + stamp;
    if (sink.marker) return "the marker is still there";
    return {};
}

// The files of `files` that are on the disk with exactly their bytes: what a continued unpack may keep, counted from the disk and not from the run.
std::size_t WholeCount(const MemSink& sink, const std::vector<Mem>& files) {
    std::size_t whole = 0;
    for (const Mem& file : files) {
        const auto found = sink.files.find(file.path);
        if (found != sink.files.end() && found->second == file.bytes) ++whole;
    }
    return whole;
}

// The paths the run began, in order: the files it copied.
std::vector<std::string> Begun(const MemSink& sink) {
    std::vector<std::string> begun;
    for (const std::string& op : sink.ops)
        if (op.compare(0, 6, "Begin:") == 0) begun.push_back(op.substr(6));
    return begun;
}

// The same paths and sizes, other bytes: another build of the game with files of the sizes of this one's. A kept file of the first build would show.
std::vector<Mem> OtherBuild(const std::vector<Mem>& files) {
    std::vector<Mem> other = files;
    for (Mem& file : other) file.bytes = std::string(file.bytes.size(), 'B');
    return other;
}

// A disk on which an uninterrupted unpack of `files` under `stamp` was made.
MarkerSink Finished(const std::vector<Mem>& files, const std::string& stamp) {
    MemSource source = SourceFor(stamp, files);
    MarkerSink sink;
    const Unpack::Result result = Unpack::Run(source, sink, Plain());
    CHECK_MSG(result.outcome == Unpack::Outcome::Done, result.error);
    sink.NextLife();
    return sink;
}

// THE NEXT START, with nothing wrong with it, over whatever the disk holds. Whatever was done to the disk before, it must end in the tree an uninterrupted
// unpack makes. A disk that already holds the finished copy of this stamp is UpToDate (and loses a marker left beside it); any other is Done, and continues
// exactly when the marker is the list's stamp, then keeping exactly the files that are whole (counted here, from the disk before the run); a fresh source is
// asked for no more assets than the files copied.
Unpack::Result TheNextStartFinishes(MarkerSink& sink, const std::vector<Mem>& files, const std::string& stamp, const std::string& at) {
    const bool markedWithThisStamp = sink.marker && *sink.marker == stamp;
    const bool finishedAlready = sink.stamp && *sink.stamp == stamp && WholeCount(sink, files) == files.size();
    const std::size_t wholeBefore = markedWithThisStamp ? WholeCount(sink, files) : std::size_t{0};
    sink.NextLife();
    MemSource source = SourceFor(stamp, files);
    const Unpack::Result result = Unpack::Run(source, sink, Plain());
    if (finishedAlready) {
        CHECK_MSG(result.outcome == Unpack::Outcome::UpToDate && result.copied == 0 && result.kept == 0 && !result.resumed, at + ": a finished copy is up to date");
    } else {
        CHECK_MSG(result.outcome == Unpack::Outcome::Done, at + ": " + result.error);
        CHECK_MSG(result.resumed == markedWithThisStamp, at + ": it continues exactly when the marker is the list's stamp");
        CHECK_MSG(result.kept == wholeBefore && result.copied == files.size() - wholeBefore,
                  at + ": kept " + std::to_string(result.kept) + " and copied " + std::to_string(result.copied) + " of " + std::to_string(files.size()) +
                      ", the disk held " + std::to_string(wholeBefore) + " whole");
    }
    CHECK_MSG(TreeProblem(sink, files, stamp).empty(), at + ": " + TreeProblem(sink, files, stamp));
    CHECK_MSG(source.reads == static_cast<int>(result.copied), at + ": the APK was asked for exactly the files copied");
    CHECK_MSG(!sink.stampPresentAtABegin && !sink.treeRemovedWhileStamped && !sink.treeRemovedWhileMarked, at + ": no stamp over a copy in progress, no wipe under a marker");
    const std::set<std::string> only = {stamp};
    CHECK_MSG(sink.markersAtABegin.empty() || sink.markersAtABegin == only, at + ": the marker, when a file was begun, was the list's stamp");
    return result;
}

// ---- The list ---------------------------------------------------------------------------------------------------------------------------

void AListOfSizesAndPathsReadsBack() {
    const std::vector<Mem> files = {{"GameData/levels/a.tscn", "12345"}, {"GameData/odd name/b c.png", ""}, {"engine/assets/shaders/x.spv", "zz"}};
    const Unpack::Parsed parsed = Unpack::ParseList(ListFor("0123abcd", files), kOwned);
    // mutation: swap the size and the path, read the size from the wrong end of the line, or drop the last line.
    CHECK_MSG(parsed.ok, parsed.error);
    CHECK_MSG(parsed.list.stamp == "0123abcd", "the stamp is the first line");
    CHECK_EQ(parsed.list.entries.size(), std::size_t{3});
    CHECK_MSG(parsed.list.entries.size() == 3 && parsed.list.entries[0].path == "GameData/levels/a.tscn" && parsed.list.entries[0].size == 5, "the first line");
    CHECK_MSG(parsed.list.entries.size() == 3 && parsed.list.entries[1].path == "GameData/odd name/b c.png" && parsed.list.entries[1].size == 0, "a path may hold spaces, and a file may be empty");
    CHECK_MSG(parsed.list.entries.size() == 3 && parsed.list.entries[2].path == "engine/assets/shaders/x.spv" && parsed.list.entries[2].size == 2, "the last line");
    // mutation: add the sizes wrongly (sum of the paths' lengths, or only the first).
    CHECK_EQ(parsed.list.bytes, std::uint64_t{7});
    // The allocation estimate: each file in whole blocks. 5 -> 4096, 0 -> 0, 2 -> 4096.
    // mutation: no rounding up (the sum of the sizes), or rounding down.
    CHECK_EQ(Unpack::NeededBytes(parsed.list), std::uint64_t{8192});
    Unpack::List big;
    big.entries = {{"GameData/a", Unpack::kBlockBytes}, {"GameData/b", Unpack::kBlockBytes + 1}, {"GameData/c", 1}};
    CHECK_EQ(Unpack::NeededBytes(big), 4 * Unpack::kBlockBytes);
}

void BlankLinesAndCarriageReturnsAreToleratedAndNothingIsAComment() {
    const std::string crlf = ListFor("s1", {{"GameData/a", "xx"}, {"GameData/b", "y"}}, "\r\n");
    const Unpack::Parsed a = Unpack::ParseList(crlf, kOwned);
    // mutation: keep the CR in the path or in the stamp.
    CHECK_MSG(a.ok, a.error);
    CHECK_MSG(a.ok && a.list.stamp == "s1" && a.list.entries.size() == 2 && a.list.entries[1].path == "GameData/b", "CRLF line ends read as LF");
    const Unpack::Parsed b = Unpack::ParseList("s2\n\n2\tGameData/a\n\n\n1\tGameData/b\n\n", kOwned);
    // mutation: a blank line is an error.
    CHECK_MSG(b.ok && b.list.entries.size() == 2, "blank lines are skipped");
    // A '#' begins no comment: a file may be called that, and a line that is only a note is therefore a line with no TAB, which is refused.
    const Unpack::Parsed c = Unpack::ParseList("s3\n1\tGameData/#odd\n", kOwned);
    // mutation: skip lines that begin with '#' (the file would vanish from the list).
    CHECK_MSG(c.ok && c.list.entries.size() == 1 && c.list.entries[0].path == "GameData/#odd", "a name may begin with #");
    const Unpack::Parsed d = Unpack::ParseList("s4\n# a note\n1\tGameData/a\n", kOwned);
    CHECK_MSG(!d.ok, "there is no comment syntax: a note is a malformed line");
    // No final newline: the walk must END. (A walk that did not, on Step 202's help text, exhausted a VM.)
    // mutation: loop while a newline is found, or never advance past the last line.
    const Unpack::Parsed e = Unpack::ParseList("s5\n3\tGameData/a", kOwned);
    CHECK_MSG(e.ok && e.list.entries.size() == 1 && e.list.entries[0].path == "GameData/a", "the last line needs no newline");
    const Unpack::Parsed f = Unpack::ParseList("s6", kOwned);
    CHECK_MSG(!f.ok, "a stamp alone names no file");
}

void EveryWayAListCanBeWrongIsRefusedWithTheLine() {
    const auto refused = [&](const std::string& text, const std::string& part, const std::string& what) {
        const Unpack::Parsed parsed = Unpack::ParseList(text, kOwned);
        CHECK_MSG(!parsed.ok, what + ": " + text);
        CHECK_MSG(Has(parsed.error, part), what + ": the words '" + parsed.error + "' should hold '" + part + "'");
        CHECK_MSG(parsed.list.entries.empty() && parsed.list.bytes == 0, what + ": a refused list hands nothing on");
    };
    // mutation: accept an empty text.
    refused("", "empty", "an empty text");
    // mutation: take the first non-blank line as the stamp.
    refused("\n1\tGameData/a\n", "stamp", "a blank first line");
    // An absent stamp file reads as "" in the sink, so an empty stamp must never be a list's: it would equal the nothing that is stored.
    // mutation: accept an empty stamp.
    refused("\r\n1\tGameData/a\n", "stamp", "a first line that is only a CR");
    refused("a b\n1\tGameData/a\n", "stamp", "a stamp with a space");
    refused("a/b\n1\tGameData/a\n", "stamp", "a stamp with a slash");
    refused(std::string(Unpack::kMostStampChars + 1, 'a') + "\n1\tGameData/a\n", "stamp", "a stamp too long");
    // mutation: no length limit on the stamp.
    CHECK_MSG(Unpack::ParseList(std::string(Unpack::kMostStampChars, 'a') + "\n1\tGameData/a\n", kOwned).ok, "a stamp of the longest length is fine");
    refused("s\nGameData/a\n", "TAB", "no TAB");
    refused("s\n 1 GameData/a\n", "TAB", "a space where the TAB belongs");
    refused("s\n\tGameData/a\n", "size", "no size");
    refused("s\n-1\tGameData/a\n", "decimal", "a negative size");
    refused("s\n+1\tGameData/a\n", "decimal", "a plus sign");
    refused("s\n1x\tGameData/a\n", "decimal", "a letter in the size");
    refused("s\n0x10\tGameData/a\n", "decimal", "a hex size");
    refused("s\n1.5\tGameData/a\n", "decimal", "a fractional size");
    // mutation: no limit on the digits (a 20-digit size wraps).
    refused("s\n" + std::string(Unpack::kMostSizeDigits + 1, '9') + "\tGameData/a\n", "digits", "a size too long to be a size");
    CHECK_MSG(Unpack::ParseList("s\n" + std::string(Unpack::kMostSizeDigits, '9') + "\tGameData/a\n", kOwned).ok, "the longest size is fine");
    refused("s\n1\tGameData/a\n1\tGameData/a\n", "twice", "a path listed twice");
    // mutation: detect a duplicate only on the line right after the first.
    refused("s\n1\tGameData/a\n1\tGameData/b\n1\tGameData/a\n", "twice", "a path listed twice, not on adjacent lines");
    refused("s\n1\tGameData/a\t2\n", "control", "a TAB inside the path");
    // The sum of the sizes cannot wrap: 20,000 lines of the largest size are more than 64 bits hold.
    // mutation: add without the check.
    std::string overflow = "s\n";
    for (int i = 0; i < 20000; ++i) overflow += std::string(Unpack::kMostSizeDigits, '9') + "\tGameData/f" + std::to_string(i) + "\n";
    refused(overflow, "64 bits", "sizes that add up past 64 bits");
    // The line number is in the words.
    const Unpack::Parsed third = Unpack::ParseList("s\n1\tGameData/a\n1\tGameData/../b\n", kOwned);
    CHECK_MSG(!third.ok && Has(third.error, "line 3"), "the words name the line: " + third.error);
}

// ---- The names --------------------------------------------------------------------------------------------------------------------------

void EveryRefusalOfAPathStandsOnItsOwn() {
    const auto refused = [&](const std::string& path, const std::string& part, const std::string& what) {
        std::string why;
        const bool ok = Unpack::SafePath(path, kOwned, why);
        CHECK_MSG(!ok, what);
        CHECK_MSG(Has(why, part), what + ": '" + why + "' should hold '" + part + "'");
    };
    // mutation: delete the refusal named; each is a separate branch of SafePath.
    refused("", "empty", "an empty path");
    refused(std::string("GameData/a") + '\0' + "b", "NUL", "a NUL");
    refused("GameData/a\nb", "control", "a newline");
    refused("GameData/a\tb", "control", "a TAB (it would break the list's own format)");
    refused("GameData/a\x7f", "control", "DEL");
    refused("GameData\\a", "backslash", "a backslash");
    refused("GameData/..\\x", "backslash", "a backslash that climbs");
    refused("C:/GameData/a", "colon", "a drive letter");
    refused("C:GameData/a", "colon", "a drive letter without a slash");
    refused("GameData/a:stream", "colon", "a stream name");
    refused("/GameData/a", "absolute", "an absolute path");
    refused("/etc/passwd", "absolute", "an absolute path outside");
    refused("GameData/a/", "folder", "a trailing slash");
    refused("GameData//a", "empty segment", "an empty segment");
    refused("GameData/./a", "'.'", "a '.' segment");
    refused("./GameData/a", "'.'", "a leading '.' segment");
    refused("GameData/../a", "..", "a '..' segment");
    refused("../GameData/a", "..", "a leading '..'");
    refused("GameData/a/..", "..", "a trailing '..'");
    refused("GameData/..", "..", "a '..' that would name the owned folder itself");
    refused("GameData/a/../../etc/x", "..", "a climb past the folder");
    // Beside the owned folders: the player's saves, the log, the flags file, the stamp, anything.
    // mutation: no check of the first segment (any relative path passes).
    refused("Magic Portals/scores.json", "owns", "the player's saves");
    refused("magicportals_gamedata.stamp", "owns", "the stamp's own name");
    refused("magicportals_gamedata.stamp.partial", "owns", "the resume marker's own name (E49): no list can name it, so no copy can write over it");
    refused("magicportals_args.txt", "owns", "the developer's flags file");
    refused("gamedata/a", "owns", "an owned folder's name in another case");
    refused("GameDataX/a", "owns", "a name that merely begins like an owned folder");
    refused("engine2/a", "owns", "another such name");
    // mutation: accept the bare name of an owned folder (it would be a FILE where the folder is).
    refused("GameData", "nothing under it", "an owned folder's bare name");
    refused("engine", "nothing under it", "the other owned folder's bare name");
    // What must pass.
    for (const std::string& path : {std::string("GameData/a"), std::string("GameData/levels/level0.tscn"), std::string("GameData/odd name/b c.png"),
                                    std::string("engine/assets/shaders/bloom_blur.spv"), std::string("GameData/a.b.c"), std::string("GameData/.hidden"),
                                    std::string("GameData/caf\xc3\xa9.png"), std::string("GameData/..a"), std::string("GameData/a..b")}) {
        std::string why;
        const bool ok = Unpack::SafePath(path, kOwned, why);
        CHECK_MSG(ok, path + " should pass: " + why);
    }
    // The interface languages (planning step 239) put the first SUB-FOLDERS under the port's own data: the string tables in port-data/lang, the faces and their
    // licence texts in port-data/fonts, the picker's table beside the others and the achievements' translation in tables/. Every file of the content package
    // goes through SafePath, so each name the package holds must pass - at any depth, whatever the extension (.ttf, .txt, .md). The same names are in
    // tools/package/test_android_package.py (ThePathRules.LANGUAGE_FILES), which holds the packager to the same answer.
    for (const std::string& path : {std::string("GameData/port-data/lang/languages.json"), std::string("GameData/port-data/lang/en.json"), std::string("GameData/port-data/lang/ar.json"),
                                    std::string("GameData/port-data/fonts/Kurale-Regular.ttf"), std::string("GameData/port-data/fonts/NotoSansJP-Bold.ttf"),
                                    std::string("GameData/port-data/fonts/OFL-NotoSansArabic.txt"), std::string("GameData/port-data/fonts/README.md"),
                                    std::string("GameData/port-data/picker.json"), std::string("GameData/tables/achievements.de.json"), std::string("GameData/tables/achievements.ja.json")}) {
        std::string why;
        const bool ok = Unpack::SafePath(path, kOwned, why);
        CHECK_MSG(ok, path + " should pass: " + why);
    }
    // And what is still refused beside them: a climb out of the sub-folder.
    refused("GameData/port-data/lang/../../../etc/x", "..", "a climb out of the language folder");
    refused("GameData/port-data/fonts/../../..", "..", "a climb out of the fonts folder");
}

// ---- The run ----------------------------------------------------------------------------------------------------------------------------

void AFreshUnpackCopiesEveryFileAndWritesTheStampLast() {
    const std::vector<Mem> files = Invented(23);
    MemSource source = SourceFor("stamp-one", files);
    MemSink sink;
    const Unpack::Result result = Unpack::Run(source, sink, Plain());
    CHECK_MSG(result.outcome == Unpack::Outcome::Done, result.error);
    CHECK_EQ(result.files, std::size_t{23});
    CHECK_EQ(result.copied, std::size_t{23});
    std::uint64_t listed = 0;
    for (const Mem& file : files) listed += file.bytes.size();
    // mutation: count the chunks, not the bytes; or the sizes from the list instead of what was written.
    CHECK_EQ(result.bytes, listed);
    CHECK_EQ(result.copiedBytes, listed);
    CHECK_MSG(result.stamp == "stamp-one", "the result carries the stamp");
    bool every = sink.files.size() == files.size();
    for (const Mem& file : files) every = every && sink.files.count(file.path) == 1 && sink.files[file.path] == file.bytes;
    CHECK_MSG(every, "every file is on the disk with exactly its bytes");
    CHECK_MSG(sink.stamp && *sink.stamp == "stamp-one", "the stamp is the list's");
    // THE ORDER. mutation: write the stamp first (before the copy), or at the start of the run.
    CHECK_MSG(!sink.stampPresentAtABegin, "no file was begun while a stamp stood");
    CHECK_MSG(!sink.ops.empty() && sink.ops.back() == "WriteStamp", "and it is the last thing done");
    CHECK_EQ(sink.Count("WriteStamp"), std::size_t{1});
    // mutation: do not remove the stamp before the copy; do not remove the owned folders; clear them after the copy.
    const std::ptrdiff_t removeStamp = sink.IndexOf("RemoveStamp");
    const std::ptrdiff_t removeGameData = sink.IndexOf("RemoveTree:GameData");
    const std::ptrdiff_t removeEngine = sink.IndexOf("RemoveTree:engine");
    const std::ptrdiff_t freeAsked = sink.IndexOf("FreeBytes");
    const std::ptrdiff_t heldAsked = sink.IndexOf("HeldBytes:GameData");
    const std::ptrdiff_t firstBegin = sink.IndexOf("Begin:" + files[0].path);
    CHECK_MSG(removeStamp >= 0 && removeGameData > removeStamp && removeEngine > removeStamp, "the stamp goes before the folders");
    // mutation: check the space after the old copy is out of the way (the order step 203 had, and PORT5-10 (b) was about).
    CHECK_MSG(freeAsked >= 0 && freeAsked < removeStamp && freeAsked < removeGameData, "space is checked BEFORE anything is removed, so a disk that cannot hold the new copy leaves the old one whole");
    CHECK_MSG(heldAsked >= 0 && heldAsked < freeAsked && sink.IndexOf("HeldBytes:engine") >= 0, "what the old copy would give back is asked first, for both owned folders");
    CHECK_MSG(firstBegin > removeEngine, "and the first file is written after every removal");
    // The files come in the list's order, each ended before the next begins.
    std::vector<std::string> begins;
    for (const std::string& op : sink.ops)
        if (op.compare(0, 6, "Begin:") == 0) begins.push_back(op.substr(6));
    std::vector<std::string> listedOrder;
    for (const Mem& file : files) listedOrder.push_back(file.path);
    CHECK_MSG(begins == listedOrder, "the list's order");
    CHECK_EQ(sink.Count("End:"), std::size_t{23});
    CHECK_EQ(static_cast<std::size_t>(std::count_if(sink.ops.begin(), sink.ops.end(), [](const std::string& op) { return Has(op, ":keep"); })), std::size_t{23});
}

void TheSameStampWithEveryFileThereDoesNothing() {
    const std::vector<Mem> files = Invented(12);
    MemSource source = SourceFor("same", files);
    MemSink sink;
    CHECK(Unpack::Run(source, sink, Plain()).outcome == Unpack::Outcome::Done);
    sink.ops.clear();
    source.reads = 0;
    const Unpack::Result again = Unpack::Run(source, sink, Plain());
    // mutation: compare nothing (always unpack), or unpack when the stamps are equal.
    CHECK_MSG(again.outcome == Unpack::Outcome::UpToDate, again.error);
    CHECK_EQ(again.copied, std::size_t{0});
    CHECK_EQ(source.reads, 0);
    // mutation: touch the disk on an up-to-date start (remove the stamp, the folders).
    CHECK_MSG(sink.ops.empty(), "an up-to-date start writes and removes nothing");
    CHECK_EQ(again.files, std::size_t{12});
    CHECK_EQ(again.pumps, std::size_t{0});
}

void AStampWithAFileMissingOrShortIsNotTheWordOfAWholeCopy() {
    const std::vector<Mem> files = Invented(12);
    for (int damage = 0; damage < 3; ++damage) {
        MemSource source = SourceFor("same", files);
        MemSink sink;
        CHECK(Unpack::Run(source, sink, Plain()).outcome == Unpack::Outcome::Done);
        const std::string victim = files[7].path;
        if (damage == 0) sink.files.erase(victim);
        if (damage == 1) sink.files[victim].pop_back();
        if (damage == 2) sink.files[victim] += "!";
        sink.ops.clear();
        source.reads = 0;
        const Unpack::Result again = Unpack::Run(source, sink, Plain());
        // mutation: trust the stamp alone; check only that the file exists; check only the first file; compare sizes with <=.
        CHECK_MSG(again.outcome == Unpack::Outcome::Done, std::string(damage == 0 ? "a missing file" : damage == 1 ? "a short file" : "a long file") + " is made again");
        CHECK_EQ(again.copied, std::size_t{12});
        CHECK_MSG(sink.files[victim] == files[7].bytes, "and is whole afterwards");
    }
    // A stamp that is not the list's, with every file there: a new APK. It is copied afresh, and a file the old one had and this one has not goes.
    MemSource source = SourceFor("old", files);
    MemSink sink;
    CHECK(Unpack::Run(source, sink, Plain()).outcome == Unpack::Outcome::Done);
    sink.files["GameData/part0/stale.dat"] = "left by the old APK";
    source = SourceFor("new", files);
    const Unpack::Result fresh = Unpack::Run(source, sink, Plain());
    // mutation: skip when the stamps differ but the files are all there.
    CHECK_MSG(fresh.outcome == Unpack::Outcome::Done && fresh.copied == 12, "a new stamp is a new copy");
    // mutation: leave the owned folders alone.
    CHECK_MSG(sink.files.count("GameData/part0/stale.dat") == 0, "a file the new APK does not have is gone");
    CHECK_MSG(sink.stamp && *sink.stamp == "new", "and the stamp is the new one");
}

// OVER A SINK THAT KEEPS NO MARKER (MemSink): the all-or-nothing unpack as it was, every file copied again by the second call. Nothing in this test moved when
// the resume marker was added (E49): that is the point of it. The same stop over a sink that keeps the marker, which KEEPS the first k files, is
// AStartThatDiesAtEveryFileIsContinuedAndKeepsWhatWasDone, in the resume marker's section below.
void AnUnpackThatDiesAtFileKLeavesNoStampAndASecondCallCompletes() {
    const std::vector<Mem> files = Invented(20);
    for (const std::size_t k : {std::size_t{0}, std::size_t{9}, std::size_t{19}}) {
        MemSource source = SourceFor("one", files);
        source.failAt = files[k].path;
        MemSink sink;
        const Unpack::Result first = Unpack::Run(source, sink, Plain());
        // mutation: write the stamp first; write it after a failure too; ignore the source's failure.
        CHECK_MSG(first.outcome == Unpack::Outcome::Failed, "the source died at file " + std::to_string(k));
        CHECK_MSG(!sink.stamp.has_value(), "no stamp: a copy that did not finish says nothing");
        CHECK_MSG(Has(first.error, files[k].path) && Has(first.error, "could not be unpacked"), "and the words name the file: " + first.error);
        // mutation: leave the half-written file (do not drop it).
        CHECK_MSG(sink.files.count(files[k].path) == 0, "the half-written file is removed");
        CHECK_EQ(first.copied, k);
        // mutation: carry on to the files after k.
        CHECK_MSG(sink.Count("Begin:") == k + 1, "and no file after it is begun");
        // The second call, from a source that works.
        MemSource fixed = SourceFor("one", files);
        const Unpack::Result second = Unpack::Run(fixed, sink, Plain());
        CHECK_MSG(second.outcome == Unpack::Outcome::Done, "a second call completes: " + second.error);
        CHECK_EQ(second.copied, std::size_t{20});
        CHECK_MSG(sink.stamp && *sink.stamp == "one", "and then there is a stamp");
        // E49. This sink has no marker to find, so the second call is a clean start and says nothing of a start that was cut short.
        // mutation: take a stop for a marker without asking the sink (resume on any partial tree).
        CHECK_MSG(!second.resumed && second.kept == 0 && !second.resumable, "a sink without a marker starts again from the top");
    }
}

void AStampLeftByAnEarlierCopyDoesNotSurviveAFailedRecopy() {
    const std::vector<Mem> files = Invented(10);
    MemSource old = SourceFor("old", files);
    MemSink sink;
    CHECK(Unpack::Run(old, sink, Plain()).outcome == Unpack::Outcome::Done);
    MemSource next = SourceFor("new", files);
    next.failAt = files[4].path;
    const Unpack::Result failed = Unpack::Run(next, sink, Plain());
    // mutation: do not remove the old stamp before the copy (it would stand over a half-replaced copy, and a start that read it would take that copy as whole).
    CHECK_MSG(failed.outcome == Unpack::Outcome::Failed, "the re-copy failed");
    CHECK_MSG(!sink.stamp.has_value(), "the old copy's stamp is gone: nothing vouches for what is there now");
    CHECK_MSG(!sink.stampPresentAtABegin, "it went before the first file was begun");
}

void TheCopyIsCheckedAgainstTheList() {
    const std::vector<Mem> files = Invented(8);
    for (int kind = 0; kind < 2; ++kind) {
        MemSource source = SourceFor("s", files);
        (kind == 0 ? source.shortBy : source.longBy) = files[3].path;
        MemSink sink;
        const Unpack::Result result = Unpack::Run(source, sink, Plain());
        // mutation: do not compare the bytes given with the size listed (or compare only when fewer).
        CHECK_MSG(result.outcome == Unpack::Outcome::Failed, kind == 0 ? "one byte fewer than the list says" : "one byte more than the list says");
        CHECK_MSG(Has(result.error, files[3].path) && Has(result.error, "damaged"), "the words say the file is damaged: " + result.error);
        CHECK_MSG(sink.files.count(files[3].path) == 0, "and the half is removed");
        CHECK_MSG(!sink.stamp.has_value(), "and no stamp is written");
    }
}

void TheSourceOrTheListCanBeBroken() {
    MemSource none;
    none.listReadable = false;
    MemSink sink;
    const Unpack::Result a = Unpack::Run(none, sink, Plain());
    CHECK_MSG(a.outcome == Unpack::Outcome::Failed && Has(a.error, "inside the app") && Has(a.error, "no gamedata.list"), "a list that cannot be read: " + a.error);
    CHECK_MSG(sink.ops.empty(), "and nothing is touched");
    MemSource bad;
    bad.list = "stamp\nnot a list\n";
    const Unpack::Result b = Unpack::Run(bad, sink, Plain());
    CHECK_MSG(b.outcome == Unpack::Outcome::Failed && Has(b.error, "cannot be used") && Has(b.error, "line 2"), "a list that is not one: " + b.error);
    CHECK_MSG(sink.ops.empty(), "and nothing is touched");
    // An asset the list names and the APK lacks.
    const std::vector<Mem> files = Invented(5);
    MemSource lacking = SourceFor("s", files);
    lacking.assets.erase(files[2].path);
    MemSink sink2;
    const Unpack::Result c = Unpack::Run(lacking, sink2, Plain());
    CHECK_MSG(c.outcome == Unpack::Outcome::Failed && Has(c.error, files[2].path) && Has(c.error, "no such asset"), "an asset that is not there: " + c.error);
    CHECK_MSG(!sink2.stamp.has_value(), "and no stamp");
}

void AHostileListNeverTouchesTheDisk() {
    const std::vector<std::string> hostile = {"../escape", "GameData/../../escape", "/abs/path", "C:/x", "GameData\\x", "Magic Portals/scores.json",
                                              "magicportals_args.txt", "GameData//x", "GameData/./x", "GameData/x/", "GameData", ""};
    for (const std::string& path : hostile) {
        MemSource source;
        source.list = "stamp\n1\tGameData/ok.dat\n1\t" + path + "\n";
        source.assets["GameData/ok.dat"] = "a";
        source.assets[path] = "b";
        MemSink sink;
        sink.files["Magic Portals/scores.json"] = "the player's medals";
        const Unpack::Result result = Unpack::Run(source, sink, Plain());
        // mutation: accept '..' (or any one of these) in SafePath.
        CHECK_MSG(result.outcome == Unpack::Outcome::Failed, "refused: '" + path + "'");
        CHECK_MSG(Has(result.error, "cannot be used"), "in words: " + result.error);
        // mutation: parse after the removal of the old copy (a bad list would then cost the old copy).
        CHECK_MSG(sink.ops.empty(), "nothing was removed or written for '" + path + "'");
        CHECK_MSG(sink.files.size() == 1 && sink.files["Magic Portals/scores.json"] == "the player's medals", "the player's file is as it was");
        CHECK_EQ(source.reads, 0);
    }
    // A bad line late in a long list still stops it before the first file: the whole list is read before anything is done.
    MemSource late;
    late.list = ListFor("s", Invented(30)) + "1\tGameData/../x\n";
    MemSink sink;
    CHECK(Unpack::Run(late, sink, Plain()).outcome == Unpack::Outcome::Failed);
    CHECK_MSG(sink.ops.empty(), "a bad line at the end costs the old copy nothing");
}

void ThePumpIsCalledEverySoOftenAndWhenItSaysFinishTheUnpackStops() {
    const std::vector<Mem> files = Invented(40);
    // Cadence 16, the shipped one: after file 16 and after file 32, not after 40.
    {
        MemSource source = SourceFor("p", files);
        MemSink sink;
        Unpack::Options options = Plain();
        std::vector<std::size_t> copiedAtPump;
        options.pump = [&] {
            copiedAtPump.push_back(sink.Count("End:"));
            return true;
        };
        const Unpack::Result result = Unpack::Run(source, sink, options);
        // mutation: never call the pump; call it before the first file; call it every file.
        CHECK_MSG(result.outcome == Unpack::Outcome::Done, result.error);
        CHECK_EQ(options.pumpEvery, Unpack::kPumpEveryFiles);
        CHECK_EQ(Unpack::kPumpEveryFiles, std::size_t{16});
        CHECK_EQ(copiedAtPump.size(), std::size_t{2});
        CHECK_EQ(result.pumps, std::size_t{2});
        CHECK_MSG(copiedAtPump.size() == 2 && copiedAtPump[0] == 16 && copiedAtPump[1] == 32, "after the 16th and the 32nd file, each already copied");
    }
    // Every file; and a cadence of 0 is none at all.
    for (const std::size_t every : {std::size_t{1}, std::size_t{7}, std::size_t{0}}) {
        MemSource source = SourceFor("p", files);
        MemSink sink;
        Unpack::Options options = Plain();
        options.pumpEvery = every;
        std::size_t calls = 0;
        options.pump = [&] {
            ++calls;
            return true;
        };
        const Unpack::Result result = Unpack::Run(source, sink, options);
        CHECK(result.outcome == Unpack::Outcome::Done);
        CHECK_EQ(calls, every == 0 ? std::size_t{0} : 40 / every);
        CHECK_EQ(result.pumps, calls);
    }
    // No pump handed in at all: the run goes on.
    {
        MemSource source = SourceFor("p", files);
        MemSink sink;
        CHECK(Unpack::Run(source, sink, Plain()).outcome == Unpack::Outcome::Done);
    }
    // THE APP IS CLOSED during the unpack.
    {
        MemSource source = SourceFor("p", files);
        MemSink sink;
        Unpack::Options options = Plain();
        int calls = 0;
        options.pump = [&] { return ++calls < 2; };
        const Unpack::Result result = Unpack::Run(source, sink, options);
        // mutation: count a cancel as Done, as Failed, or as UpToDate.
        CHECK_MSG(result.outcome == Unpack::Outcome::Cancelled, "a pump that says finish cancels the unpack");
        // mutation: put the cancel's reason in `error` (it would be shown as a failure).
        CHECK_MSG(result.error.empty(), "and says nothing: it is not a failure and the player is told nothing");
        // mutation: carry on after the pump said finish.
        CHECK_EQ(result.copied, std::size_t{32});
        CHECK_EQ(sink.Count("Begin:"), std::size_t{32});
        CHECK_EQ(calls, 2);
        // mutation: write the stamp on a cancel.
        CHECK_MSG(!sink.stamp.has_value() && !sink.Did("WriteStamp"), "no stamp: the copy is not whole");
        // The next start unpacks again from the top and completes.
        MemSource again = SourceFor("p", files);
        const Unpack::Result next = Unpack::Run(again, sink, Plain());
        CHECK_MSG(next.outcome == Unpack::Outcome::Done && next.copied == 40, "and the next start finishes it");
    }
    // The progress callback is told at each pump point and at the end, with files and bytes so far.
    {
        MemSource source = SourceFor("p", files);
        MemSink sink;
        Unpack::Options options = Plain();
        std::vector<std::size_t> seen;
        std::uint64_t lastBytes = 0;
        options.progress = [&](std::size_t done, std::size_t total, std::uint64_t bytes) {
            seen.push_back(done);
            CHECK_EQ(total, std::size_t{40});
            CHECK(bytes >= lastBytes);
            lastBytes = bytes;
        };
        CHECK(Unpack::Run(source, sink, options).outcome == Unpack::Outcome::Done);
        CHECK_MSG(seen == (std::vector<std::size_t>{16, 32, 40}), "at 16, at 32 and at the end");
    }
}

void FreeSpaceIsCheckedFromTheListInBlocksAndSaidInWords() {
    // Three files that take 4096, 4096 and 8192 bytes of blocks: 16384.
    const std::vector<Mem> files = {{"GameData/a", "x"}, {"GameData/b", std::string(4096, 'y')}, {"GameData/c", std::string(4097, 'z')}};
    {
        MemSource source = SourceFor("f", files);
        MemSink sink;
        sink.free = 16383;
        const Unpack::Result result = Unpack::Run(source, sink, Plain());
        // mutation: skip the check; compare with <=; use the sum of the sizes (8194) instead of the blocks.
        CHECK_EQ(result.needed, std::uint64_t{16384});
        CHECK_MSG(result.outcome == Unpack::Outcome::Failed, "one byte short is refused");
        CHECK_MSG(Has(result.error, "not enough free space") && Has(result.error, "about 1 MB") && Has(result.error, "free"), "in words that say how much: " + result.error);
        // mutation: check after the copy has begun.
        CHECK_MSG(sink.Count("Begin:") == 0, "and before any file is written");
        CHECK_MSG(!sink.stamp.has_value(), "no stamp");
    }
    {
        MemSource source = SourceFor("f", files);
        MemSink sink;
        sink.free = 16384;
        CHECK_MSG(Unpack::Run(source, sink, Plain()).outcome == Unpack::Outcome::Done, "exactly enough is enough");
    }
    {
        MemSource source = SourceFor("f", files);
        MemSink sink;
        sink.free = std::nullopt;
        // mutation: treat an unknown amount as zero.
        CHECK_MSG(Unpack::Run(source, sink, Plain()).outcome == Unpack::Outcome::Done, "a disk that cannot say how much is free is not refused for it");
    }
    {
        // The megabytes in the words are rounded up from the real figures.
        std::vector<Mem> many;
        for (int i = 0; i < 300; ++i) many.push_back({"GameData/f" + std::to_string(i), std::string(10000, 'q')});
        MemSource source = SourceFor("f", many);
        MemSink sink;
        sink.free = 2 * 1024 * 1024;
        const Unpack::Result result = Unpack::Run(source, sink, Plain());
        CHECK(result.outcome == Unpack::Outcome::Failed);
        CHECK_EQ(result.needed, std::uint64_t{300} * 12288);
        CHECK_MSG(Has(result.error, "about 4 MB") && Has(result.error, "2 MB are free"), "3,686,400 bytes are about 4 MB and 2,097,152 are 2 MB: " + result.error);
    }
}

void AFullDiskMidFileIsAFailureThatNamesTheFile() {
    const std::vector<Mem> files = Invented(10);
    MemSource source = SourceFor("d", files);
    MemSink sink;
    sink.diskHolds = 40; // a few files in
    const Unpack::Result result = Unpack::Run(source, sink, Plain());
    // mutation: ignore a Write that fails.
    CHECK_MSG(result.outcome == Unpack::Outcome::Failed, "the disk filled");
    CHECK_MSG(Has(result.error, "no space left"), "and the words say why: " + result.error);
    CHECK_MSG(!sink.stamp.has_value(), "no stamp");
    bool leftover = false;
    for (const auto& [path, bytes] : sink.files) {
        const auto listed = std::find_if(files.begin(), files.end(), [&](const Mem& m) { return m.path == path; });
        if (listed == files.end() || listed->bytes != bytes) leftover = true;
    }
    CHECK_MSG(!leftover, "and what is on the disk is whole files only: the half-written one is gone");
}

void TheSameInputsGiveTheSameOperationsEveryTime() {
    const std::vector<Mem> files = Invented(37);
    std::vector<std::string> first;
    for (int run = 0; run < 3; ++run) {
        MemSource source = SourceFor("det", files);
        source.chunk = run == 2 ? 1 : 5; // the size of the pieces the APK gives does not change what the disk is told to keep
        MemSink sink;
        Unpack::Options options = Plain();
        options.pump = [] { return true; };
        const Unpack::Result result = Unpack::Run(source, sink, options);
        CHECK(result.outcome == Unpack::Outcome::Done);
        if (run == 0) first = sink.ops;
        CHECK_MSG(sink.ops == first, "run " + std::to_string(run) + " did what run 0 did");
    }
}

// ---- The resume marker (ENHANCEMENT E49) ----------------------------------------------------------------------------------------------------

// The order of a clean start over a sink that keeps the marker: the stamp goes, then the marker (of another build, if any), then the owned folders, THEN the
// marker is written, and only then the first file; the stamp is written last and the marker removed after it.
void ACleanStartWritesTheMarkerAfterTheWipeAndRemovesItAfterTheStamp() {
    const std::vector<Mem> files = Invented(23);
    MemSource source = SourceFor("m-one", files);
    MarkerSink sink;
    const Unpack::Result result = Unpack::Run(source, sink, Plain());
    CHECK_MSG(result.outcome == Unpack::Outcome::Done, result.error);
    CHECK_MSG(!result.resumed && result.kept == 0, "nothing was found to continue");
    CHECK_MSG(result.resumable, "the marker was written and reads back: a stop from here on is continued by the next start");
    CHECK_EQ(result.copied, std::size_t{23});
    CHECK_MSG(TreeProblem(sink, files, "m-one").empty(), TreeProblem(sink, files, "m-one"));
    const std::ptrdiff_t freeAsked = sink.IndexOf("FreeBytes");
    const std::ptrdiff_t removeStamp = sink.IndexOf("RemoveStamp");
    const std::ptrdiff_t removeMarker = sink.IndexOf("RemoveMarker");
    const std::ptrdiff_t removeGameData = sink.IndexOf("RemoveTree:GameData");
    const std::ptrdiff_t removeEngine = sink.IndexOf("RemoveTree:engine");
    const std::ptrdiff_t writeMarker = sink.IndexOf("WriteMarker");
    const std::ptrdiff_t firstBegin = sink.IndexOf("Begin:" + files[0].path);
    // mutation: check the space after the removals (PORT5-10 (b) again, now with the marker's calls among them).
    CHECK_MSG(freeAsked >= 0 && freeAsked < removeStamp && freeAsked < writeMarker, "space is checked before anything is removed or written");
    // mutation: write the marker before the wipe (a marker over files of another build); remove an old marker after the wipe (a wipe cut short under it).
    CHECK_MSG(removeStamp >= 0 && removeMarker > removeStamp && removeGameData > removeMarker && removeEngine > removeGameData, "the stamp goes, then the marker, then the folders");
    CHECK_MSG(writeMarker > removeEngine && firstBegin > writeMarker, "the marker is written after the wipe and before the first file");
    CHECK_EQ(sink.Count("WriteMarker"), std::size_t{1});
    // mutation: remove the marker before the stamp is written (a stamp that cannot be written would leave nothing to continue), or write the stamp before the files.
    CHECK_MSG(sink.ops.size() >= 2 && sink.ops[sink.ops.size() - 2] == "WriteStamp" && sink.ops.back() == "RemoveMarker", "the stamp is written last, and THEN the marker goes");
    CHECK_EQ(sink.Count("RemoveMarker"), std::size_t{2}); // the clean start's, which found none, and the end's
    const std::set<std::string> only = {"m-one"};
    CHECK_MSG(sink.markersAtABegin == only, "the marker stood at every file begun, and it was the list's stamp");
    CHECK_MSG(!sink.stampPresentAtABegin && !sink.treeRemovedWhileStamped && !sink.treeRemovedWhileMarked, "no stamp over a copy in progress, no folder removed under a stamp or a marker");
}

// A sink that keeps no marker is what Run was before: a stop leaves nothing to continue, and the next start begins from the top.
void ASinkThatKeepsNoMarkerStartsAgainFromTheTopAfterAStop() {
    const std::vector<Mem> files = Invented(20);
    for (const std::size_t k : {std::size_t{0}, std::size_t{9}, std::size_t{19}}) {
        MemSource dying = SourceFor("plain", files);
        dying.failAt = files[k].path;
        MemSink sink;
        const Unpack::Result first = Unpack::Run(dying, sink, Plain());
        CHECK_MSG(first.outcome == Unpack::Outcome::Failed && !first.resumable, "the source died at file " + std::to_string(k) + ", and the sink cannot be continued");
        sink.ops.clear();
        MemSource fixed = SourceFor("plain", files);
        const Unpack::Result second = Unpack::Run(fixed, sink, Plain());
        // mutation: take any partial tree of the list's stamp for a continuation, without asking the sink.
        CHECK_MSG(second.outcome == Unpack::Outcome::Done, second.error);
        CHECK_MSG(!second.resumed && second.kept == 0 && second.copied == files.size(), "every file is copied again");
        CHECK_MSG(sink.Did("RemoveTree:GameData") && sink.Did("RemoveTree:engine") && sink.Count("Begin:") == files.size(), "the folders are wiped and every file begun again");
        CHECK_MSG(!Has(Unpack::Summary(second), "kept"), "and the line says nothing of a start that was cut short: " + Unpack::Summary(second));
    }
}

// THE CORE. The source dies at file k, for every k; the next start, with a source that works, finishes. Over a sink that keeps the marker.
void AStartThatDiesAtEveryFileIsContinuedAndKeepsWhatWasDone() {
    const std::vector<Mem> files = Invented(20);
    const MarkerSink reference = Finished(files, "one");
    for (std::size_t k = 0; k < files.size(); ++k) {
        const std::string at = "the source died at file " + std::to_string(k);
        MemSource dying = SourceFor("one", files);
        dying.failAt = files[k].path;
        MarkerSink sink;
        const Unpack::Result first = Unpack::Run(dying, sink, Plain());
        CHECK_MSG(first.outcome == Unpack::Outcome::Failed, at);
        CHECK_MSG(!sink.stamp.has_value(), at + ": no stamp, a copy that did not finish says nothing");
        // mutation: write the marker after the first file, or remove it when a copy fails.
        CHECK_MSG(sink.marker && *sink.marker == "one", at + ": the marker stands, with the list's stamp");
        CHECK_MSG(sink.files.count(files[k].path) == 0, at + ": the half-written file is removed, as before");
        CHECK_EQ(first.copied, k);
        CHECK_MSG(!first.resumed && first.resumable, at + ": a first start is no continuation, and the next one can continue it");
        CHECK_EQ(WholeCount(sink, files), k);

        sink.NextLife();
        MemSource fixed = SourceFor("one", files);
        const Unpack::Result second = Unpack::Run(fixed, sink, Plain());
        CHECK_MSG(second.outcome == Unpack::Outcome::Done, at + ": " + second.error);
        // mutation: wipe the owned folders on a resume (nothing kept); keep a file without looking at its size; copy a kept file again.
        CHECK_MSG(second.resumed && second.kept == k && second.copied == files.size() - k,
                  at + ": kept " + std::to_string(second.kept) + ", copied " + std::to_string(second.copied));
        std::vector<std::string> rest;
        std::uint64_t restBytes = 0;
        for (std::size_t i = k; i < files.size(); ++i) {
            rest.push_back(files[i].path);
            restBytes += files[i].bytes.size();
        }
        CHECK_MSG(Begun(sink) == rest, at + ": only the files that were not whole are begun again, in the list's order");
        CHECK_EQ(fixed.reads, static_cast<int>(files.size() - k));
        CHECK_EQ(second.copiedBytes, restBytes);
        CHECK_MSG(sink.files == reference.files, at + ": the tree is an uninterrupted unpack's, byte for byte");
        CHECK_MSG(TreeProblem(sink, files, "one").empty(), at + ": " + TreeProblem(sink, files, "one"));
        // mutation: remove the marker before the stamp, write another marker, or wipe on a resume.
        CHECK_MSG(sink.Count("RemoveTree:") == 0 && sink.Count("WriteMarker") == 0 && sink.Count("RemoveMarker") == 1, at + ": a continuation wipes nothing and writes no marker");
        CHECK_MSG(sink.ops.size() >= 2 && sink.ops[sink.ops.size() - 2] == "WriteStamp" && sink.ops.back() == "RemoveMarker", at + ": the stamp, then the marker's removal, last");
        // mutation: leave the old stamp standing on a resume, or begin a file with another marker standing.
        const std::set<std::string> only = {"one"};
        CHECK_MSG(!sink.stampPresentAtABegin && sink.markersAtABegin == only, at + ": no stamp over a copy in progress, the list's marker at every file");
        CHECK_MSG(Has(Unpack::Summary(second), std::to_string(k) + " more were kept from a start that was cut short"), at + ": " + Unpack::Summary(second));
    }
}

// The same, with the app finishing instead of the source failing: the pump says "finish" after the asked-th file in place. asked == the number of files is the
// state in which every file is in place and only the stamp is missing (the pump is asked after the last file too).
void AStartTheAppFinishesAtEveryFileIsContinuedToo() {
    const std::vector<Mem> files = Invented(20);
    for (std::size_t asked = 1; asked <= files.size(); ++asked) {
        const std::string at = "the app finished after " + std::to_string(asked) + " files";
        MemSource source = SourceFor("pump", files);
        MarkerSink sink;
        Unpack::Options options = Plain();
        options.pumpEvery = 1;
        std::size_t calls = 0;
        options.pump = [&] { return ++calls < asked; };
        const Unpack::Result first = Unpack::Run(source, sink, options);
        // mutation: take a cancel for a failure, or remove the marker when the app finishes.
        CHECK_MSG(first.outcome == Unpack::Outcome::Cancelled && first.error.empty(), at);
        CHECK_EQ(first.copied, asked);
        CHECK_EQ(first.pumps, asked);
        CHECK_MSG(!sink.stamp.has_value() && sink.marker && *sink.marker == "pump", at + ": no stamp, and the marker stands");
        CHECK_MSG(first.resumable && Has(Unpack::Summary(first), "next start goes on from there"), at + ": " + Unpack::Summary(first));
        const Unpack::Result second = TheNextStartFinishes(sink, files, "pump", at);
        CHECK_EQ(second.kept, asked);
        CHECK_EQ(second.copied, files.size() - asked);
    }
}

// The pump and the progress count FILES IN PLACE, the kept ones too: a continuation that finds most of its files whole must still ask the platform's event
// pump now and then, or a phone that stops a first start a second time, in a long run of kept files, would call the app "not responding". 40 files; the
// first start died at file 33, so the next one keeps 33 and copies 7, and both of the cadence's two asks (after the 16th and the 32nd) fall on KEPT files.
void AContinuationAsksThePumpAndTheProgressForKeptFilesToo() {
    const std::vector<Mem> files = Invented(40);
    MemSource dying = SourceFor("kept", files);
    dying.failAt = files[33].path;
    MarkerSink sink;
    CHECK_MSG(Unpack::Run(dying, sink, Plain()).outcome == Unpack::Outcome::Failed, "the first start dies at file 33");
    CHECK_EQ(WholeCount(sink, files), std::size_t{33});
    const MarkerSink afterTheFirstStart = sink;

    // The shipped cadence (16): two asks, then progress at each ask and once at the end, all counting the kept files.
    {
        sink.NextLife();
        MemSource source = SourceFor("kept", files);
        Unpack::Options options = Plain();
        std::size_t pumps = 0;
        std::vector<std::size_t> told;
        std::vector<std::size_t> totals;
        options.pump = [&] {
            ++pumps;
            return true;
        };
        options.progress = [&](std::size_t done, std::size_t total, std::uint64_t) {
            told.push_back(done);
            totals.push_back(total);
        };
        const Unpack::Result result = Unpack::Run(source, sink, options);
        // mutation: a kept file does not count as in place (the asks would come after the 7 copied files only: none).
        CHECK_MSG(result.outcome == Unpack::Outcome::Done, result.error);
        CHECK_MSG(result.resumed && result.kept == 33 && result.copied == 7, "33 kept, 7 copied");
        CHECK_EQ(pumps, std::size_t{2});
        CHECK_EQ(result.pumps, std::size_t{2});
        const std::vector<std::size_t> expectedTold = {16, 32, 40};
        CHECK_MSG(told == expectedTold, "progress is told 16, 32 (the kept files count) and 40 at the end");
        CHECK_MSG(totals == std::vector<std::size_t>(3, files.size()), "progress always names the 40 files of the list");
        CHECK_MSG(TreeProblem(sink, files, "kept").empty(), TreeProblem(sink, files, "kept"));
    }
    // The pump says "finish" at its first ask, which falls on the 16th file in place, a kept one: the unpack stops with nothing copied, the marker and the
    // files stay, and the start after that still finishes with the same 33 kept.
    {
        sink = afterTheFirstStart;
        sink.NextLife();
        MemSource source = SourceFor("kept", files);
        Unpack::Options options = Plain();
        options.pump = [] { return false; };
        const Unpack::Result stopped = Unpack::Run(source, sink, options);
        CHECK_MSG(stopped.outcome == Unpack::Outcome::Cancelled && stopped.error.empty(), "the app finishing is a quiet stop");
        CHECK_MSG(stopped.resumed && stopped.kept == 16 && stopped.copied == 0, "stopped on the 16th kept file: 16 kept, nothing copied");
        CHECK_EQ(stopped.pumps, std::size_t{1});
        CHECK_MSG(!sink.stamp.has_value() && sink.marker && *sink.marker == "kept", "no stamp, and the marker stands");
        CHECK_EQ(source.reads, 0);
        CHECK_MSG(stopped.resumable && Has(Unpack::Summary(stopped), "next start goes on from there"), Unpack::Summary(stopped));
        const Unpack::Result third = TheNextStartFinishes(sink, files, "kept", "after a stop on a kept file");
        CHECK_MSG(third.kept == 33 && third.copied == 7, "the files that were whole are still whole: 33 kept, 7 copied");
    }
}

// A start that is stopped again and again: every life ends after copying three files, and each adds to the last. Every file is copied exactly once over all of them.
void EachShortLifeAddsToTheLastAndEveryFileIsCopiedOnce() {
    const std::vector<Mem> files = Invented(20);
    MarkerSink sink;
    std::size_t copiedInAll = 0;
    std::size_t lives = 0;
    Unpack::Result last;
    do {
        ++lives;
        MemSource source = SourceFor("lives", files);
        Unpack::Options options = Plain();
        options.pumpEvery = 1;
        const std::size_t before = sink.Count("Begin:");
        options.pump = [&] { return sink.Count("Begin:") - before < std::size_t{3}; };
        last = Unpack::Run(source, sink, options);
        copiedInAll += last.copied;
        // mutation: wipe on a resume (the lives would copy the first three files again and again, and never finish).
        CHECK_MSG(last.copied <= 3, "life " + std::to_string(lives) + " copied at most its three");
        CHECK_MSG(last.kept == copiedInAll - last.copied, "life " + std::to_string(lives) + " kept what the lives before it copied");
    } while (last.outcome == Unpack::Outcome::Cancelled && lives < std::size_t{40});
    CHECK_MSG(last.outcome == Unpack::Outcome::Done, last.error);
    CHECK_EQ(lives, std::size_t{7}); // 20 files, three a life: six lives of three, the seventh copies the last two and is not stopped
    CHECK_EQ(copiedInAll, files.size());
    CHECK_MSG(TreeProblem(sink, files, "lives").empty(), TreeProblem(sink, files, "lives"));
}

// A stamp that cannot be written, when every file is in place: the marker stays, and the next start only writes the stamp.
void AStampThatCannotBeWrittenLeavesEveryFileAndTheMarkerForAOnePassFinish() {
    const std::vector<Mem> files = Invented(20);
    MemSource source = SourceFor("ws", files);
    MarkerSink sink;
    sink.writeStampFails = true;
    const Unpack::Result first = Unpack::Run(source, sink, Plain());
    CHECK_MSG(first.outcome == Unpack::Outcome::Failed && Has(first.error, "could not be recorded") && Has(first.error, "the stamp's file is read-only"), first.error);
    CHECK_EQ(first.copied, files.size());
    // mutation: remove the marker before the stamp is written, or when the stamp cannot be.
    CHECK_MSG(!sink.stamp.has_value() && sink.marker && *sink.marker == "ws", "no stamp, and the marker stands");
    CHECK_EQ(WholeCount(sink, files), files.size());
    sink.writeStampFails = false;
    sink.NextLife();
    MemSource fixed = SourceFor("ws", files);
    const Unpack::Result second = Unpack::Run(fixed, sink, Plain());
    CHECK_MSG(second.outcome == Unpack::Outcome::Done && second.resumed && second.kept == files.size() && second.copied == 0, second.error);
    CHECK_MSG(fixed.reads == 0 && sink.Count("Begin:") == 0 && sink.Count("RemoveTree:") == 0, "the next start only writes the stamp: no asset read, no file begun, no folder wiped");
    CHECK_MSG(sink.IndexOf("WriteStamp") >= 0 && sink.IndexOf("RemoveMarker") > sink.IndexOf("WriteStamp"), "the stamp, then the marker's removal");
    CHECK_MSG(TreeProblem(sink, files, "ws").empty(), TreeProblem(sink, files, "ws"));
}

// THE KILL. The process is gone before the m-th mutating call of a start, for every m: nothing in Run tidies up, so a file half written stays under its name.
// Whatever the call, the next start must end in the right tree, continuing exactly when the marker is the list's stamp; a start of the build before ends in
// ITS tree. Over an empty disk, and over the finished tree of another build with files of the same sizes and other bytes (which a kept file would show).
void AProcessKilledBeforeAnyCallOfAStartLeavesAStartTheNextOneFinishes() {
    const std::vector<Mem> a = Invented(9);
    const std::vector<Mem> b = OtherBuild(a);
    // What a clean start makes of mutating calls, counted from the list and the chunk size, not from the run: RemoveStamp, RemoveMarker, two RemoveTree,
    // WriteMarker; for each file Begin, one Write a chunk of five, End; WriteStamp, RemoveMarker.
    std::size_t expected = 5 + 2;
    for (const Mem& file : a) expected += 2 + (file.bytes.size() + 4) / 5;
    for (int scenario = 0; scenario < 2; ++scenario) {
        const bool overAnotherBuild = scenario == 1;
        const MarkerSink start = overAnotherBuild ? Finished(b, "B") : MarkerSink();
        MarkerSink probe = start;
        probe.NextLife();
        MemSource counted = SourceFor("A", a);
        CHECK(Unpack::Run(counted, probe, Plain()).outcome == Unpack::Outcome::Done);
        const std::size_t calls = probe.mutations;
        CHECK_EQ(calls, expected);
        for (std::size_t m = 0; m < calls; ++m) {
            const std::string at = std::string(overAnotherBuild ? "over build B" : "over an empty disk") + ", killed before call " + std::to_string(m) + " of " + std::to_string(calls);
            MarkerSink sink = start;
            sink.NextLife();
            sink.killAt = m;
            MemSource dying = SourceFor("A", a);
            bool killed = false;
            try {
                Unpack::Run(dying, sink, Plain());
            } catch (const Killed&) {
                killed = true;
            }
            CHECK_MSG(killed, at + ": the start reached that call");

            // The player puts the build before back (it was never finished here, or was, and the new one only began): its own tree, or still up to date.
            MarkerSink back = sink;
            back.NextLife();
            MemSource old = SourceFor("B", b);
            const Unpack::Result again = Unpack::Run(old, back, Plain());
            const bool bStillStands = sink.stamp && *sink.stamp == "B";
            // mutation: take the other build's tree for continuable (a marker written before the wipe, or kept through it).
            CHECK_MSG(again.outcome == (bStillStands ? Unpack::Outcome::UpToDate : Unpack::Outcome::Done), at + ": going back to B: " + again.error);
            CHECK_MSG(TreeProblem(back, b, "B").empty(), at + ": going back to B: " + TreeProblem(back, b, "B"));

            TheNextStartFinishes(sink, a, "A", at);
        }
    }
}

// Killed twice: a start killed before call m1, the one after it killed before call m2, a third finishes.
void AProcessKilledTwiceIsStillContinuedAndFinishes() {
    const std::vector<Mem> a = Invented(6);
    MarkerSink empty;
    MarkerSink probe1 = empty;
    MemSource counted1 = SourceFor("A", a);
    CHECK(Unpack::Run(counted1, probe1, Plain()).outcome == Unpack::Outcome::Done);
    const std::size_t calls1 = probe1.mutations;
    CHECK_MSG(calls1 > 20, "the count of calls is a real one: " + std::to_string(calls1));
    for (std::size_t m1 = 0; m1 < calls1; ++m1) {
        MarkerSink life1 = empty;
        life1.killAt = m1;
        MemSource dying1 = SourceFor("A", a);
        bool killed1 = false;
        try {
            Unpack::Run(dying1, life1, Plain());
        } catch (const Killed&) {
            killed1 = true;
        }
        CHECK_MSG(killed1, "life 1 was killed before call " + std::to_string(m1));
        MarkerSink probe2 = life1;
        probe2.NextLife();
        MemSource counted2 = SourceFor("A", a);
        const Unpack::Result whole2 = Unpack::Run(counted2, probe2, Plain());
        CHECK_MSG(whole2.outcome == Unpack::Outcome::Done || whole2.outcome == Unpack::Outcome::UpToDate, "life 2, left alone, ends well");
        const std::size_t calls2 = probe2.mutations;
        for (std::size_t m2 = 0; m2 < calls2; ++m2) {
            const std::string at = "killed before call " + std::to_string(m1) + ", then before call " + std::to_string(m2) + " of the next start's " + std::to_string(calls2);
            MarkerSink sink = life1;
            sink.NextLife();
            sink.killAt = m2;
            MemSource dying2 = SourceFor("A", a);
            bool killed2 = false;
            try {
                Unpack::Run(dying2, sink, Plain());
            } catch (const Killed&) {
                killed2 = true;
            }
            CHECK_MSG(killed2, at + ": the second start reached that call");
            TheNextStartFinishes(sink, a, "A", at);
        }
    }
}

// A marker of another stamp is a clean start: another build's files, even of the sizes this build's have, are never kept, and nothing of that build stays.
void AMarkerOfAnotherStampIsACleanStartAndNothingOfThatBuildSurvives() {
    const std::vector<Mem> a = Invented(14);
    std::vector<Mem> b = OtherBuild(a);
    b.erase(b.begin() + 3); // a file build A has and build B has not
    MemSource dying = SourceFor("build-a", a);
    dying.failAt = a[9].path;
    MarkerSink sink;
    CHECK(Unpack::Run(dying, sink, Plain()).outcome == Unpack::Outcome::Failed);
    CHECK_MSG(sink.marker && *sink.marker == "build-a" && sink.files.count(a[3].path) == 1 && WholeCount(sink, a) == 9, "build A's start stands: its marker and nine of its files");
    sink.NextLife();
    MemSource fresh = SourceFor("build-b", b);
    const Unpack::Result result = Unpack::Run(fresh, sink, Plain());
    CHECK_MSG(result.outcome == Unpack::Outcome::Done, result.error);
    // mutation: continue on any marker, not only the list's stamp (eight of A's files have the sizes of B's: they would be kept, and be A's bytes).
    CHECK_MSG(!result.resumed && result.kept == 0 && result.copied == b.size(), "another build's marker is not continued: nothing of its files is kept");
    CHECK_MSG(TreeProblem(sink, b, "build-b").empty(), TreeProblem(sink, b, "build-b"));
    CHECK_MSG(sink.files.count(a[3].path) == 0, "the file build B lacks is gone");
    // mutation: write the new marker before the wipe, remove the old one after it, or leave it standing through the wipe.
    const std::ptrdiff_t removeMarker = sink.IndexOf("RemoveMarker");
    CHECK_MSG(removeMarker >= 0 && removeMarker < sink.IndexOf("RemoveTree:GameData") && sink.IndexOf("RemoveTree:engine") < sink.IndexOf("WriteMarker"),
              "the old marker goes before the folders, the new one is written after them");
    const std::set<std::string> only = {"build-b"};
    CHECK_MSG(sink.markersAtABegin == only && !sink.treeRemovedWhileMarked && !sink.stampPresentAtABegin, "every file of B was begun under B's marker, and no folder was wiped under a marker");
    CHECK_MSG(Unpack::Summary(result) == "Unpacked " + std::to_string(b.size()) + " files, " + std::to_string(result.copiedBytes) + " bytes (stamp build-b).",
              "and the line says nothing of a continuation: " + Unpack::Summary(result));
}

// Under the right marker a file that is not whole is copied again, whatever is wrong with it, and one that is whole is not.
void AFileThatIsShortLongEmptyHalfOrMissingUnderTheRightMarkerIsCopiedAgain() {
    const std::vector<Mem> files = Invented(20);
    MemSource dying = SourceFor("dam", files);
    dying.failAt = files[12].path;
    MarkerSink sink;
    CHECK(Unpack::Run(dying, sink, Plain()).outcome == Unpack::Outcome::Failed);
    // What a stopped copy and a damaged disk leave under a name.
    sink.files[files[2].path].pop_back();                                                          // one byte short
    sink.files[files[5].path].clear();                                                             // the empty file a copy stopped between its open and its first write leaves
    sink.files[files[7].path] += "X";                                                              // one byte long
    sink.files.erase(files[9].path);                                                               // gone
    sink.files[files[10].path] = files[10].bytes.substr(0, files[10].bytes.size() / 2);            // half
    // mutation: keep a file that is there at any size, or at least as long as the list says (the long one), or that is not empty (the empty one).
    const Unpack::Result second = TheNextStartFinishes(sink, files, "dam", "five files damaged under the right marker");
    CHECK_MSG(second.resumed && second.kept == 7 && second.copied == 13, "twelve were copied, five of them are damaged now: seven kept, 5 + 8 copied");
    std::vector<std::string> rest;
    for (const std::size_t i : {std::size_t{2}, std::size_t{5}, std::size_t{7}, std::size_t{9}, std::size_t{10}}) rest.push_back(files[i].path);
    for (std::size_t i = 12; i < files.size(); ++i) rest.push_back(files[i].path);
    CHECK_MSG(Begun(sink) == rest, "exactly those are begun again, in the list's order");
}

// The free-space check serves a resume with the arithmetic it has for an old copy, and a refusal removes nothing, so the work already done is not lost.
void TheFreeSpaceCheckServesAResumeAndARefusalLosesNothing() {
    const std::vector<Mem> files = Invented(20);
    MemSource dying = SourceFor("sp", files);
    dying.failAt = files[12].path;
    MarkerSink sink;
    CHECK(Unpack::Run(dying, sink, Plain()).outcome == Unpack::Outcome::Failed);
    std::uint64_t held = 0;
    std::uint64_t needed = 0;
    for (std::size_t i = 0; i < files.size(); ++i) {
        const std::uint64_t blocks = Unpack::RoundUpToBlocks(files[i].bytes.size());
        needed += blocks;
        if (i < 12) held += blocks;
    }
    CHECK_MSG(needed > held && held > 0, "twelve of the twenty files are on the disk");
    const auto filesBefore = sink.files;

    // One byte short even with what is on the disk counted: refused, with its own words.
    sink.NextLife();
    sink.free = needed - held - 1;
    MemSource again = SourceFor("sp", files);
    const Unpack::Result refused = Unpack::Run(again, sink, Plain());
    // mutation: leave what is on the disk out of the sum (the refusal then comes sooner), or count it twice (later).
    CHECK_MSG(refused.outcome == Unpack::Outcome::Failed && Has(refused.error, "not enough free space"), refused.error);
    CHECK_MSG(Has(refused.error, "more than the part already copied takes") && !Has(refused.error, "old copy"), "in words that name what is counted: " + refused.error);
    CHECK_MSG(refused.resumed, "it was a continuation that was refused");
    CHECK_EQ(refused.reclaimable, held);
    CHECK_EQ(refused.needed, needed);
    // mutation: check after the stamp's removal, or after the marker's.
    CHECK_MSG(!sink.Did("RemoveStamp") && !sink.Did("RemoveMarker") && !sink.Did("WriteMarker") && sink.Count("RemoveTree:") == 0 && sink.Count("Begin:") == 0, "nothing was removed or written");
    CHECK_MSG(sink.files == filesBefore && !sink.stamp.has_value() && sink.marker && *sink.marker == "sp", "the files and the marker are as they were: the work done is not lost");
    CHECK_EQ(again.reads, 0);

    // Exactly enough with what is on the disk counted: the continuation goes through.
    sink.free = needed - held;
    const Unpack::Result done = TheNextStartFinishes(sink, files, "sp", "exactly enough free space");
    CHECK_EQ(done.kept, std::size_t{12});
    CHECK_EQ(done.copied, std::size_t{8});
}

// A marker left beside a finished copy never fails a start and is removed when it can be; a start with nothing left over writes and removes nothing.
void AMarkerBesideAFinishedCopyIsRemovedAndNeverFailsAStart() {
    const std::vector<Mem> files = Invented(12);
    // (a) The fast path, over a sink that keeps the marker, touches nothing when nothing is left over.
    {
        MarkerSink sink = Finished(files, "stale");
        MemSource source = SourceFor("stale", files);
        const Unpack::Result again = Unpack::Run(source, sink, Plain());
        // mutation: remove the marker without asking whether there is one (an unlink on every start).
        CHECK_MSG(again.outcome == Unpack::Outcome::UpToDate && sink.ops.empty() && source.reads == 0, "an up-to-date start with no marker left over does nothing");
    }
    // (b) The marker that cannot be removed after the stamp is written: the unpack is still done; the next start is up to date and tries again, best effort.
    MarkerSink stuck;
    stuck.markerRemovalFails = true;
    {
        MemSource source = SourceFor("stale", files);
        const Unpack::Result first = Unpack::Run(source, stuck, Plain());
        // mutation: fail the unpack when the marker cannot be removed.
        CHECK_MSG(first.outcome == Unpack::Outcome::Done, first.error);
        CHECK_MSG(stuck.stamp && *stuck.stamp == "stale" && stuck.marker && *stuck.marker == "stale", "the stamp is written, and the marker the failed removal left stands");
    }
    {
        stuck.NextLife();
        MemSource source = SourceFor("stale", files);
        const Unpack::Result again = Unpack::Run(source, stuck, Plain());
        CHECK_MSG(again.outcome == Unpack::Outcome::UpToDate && again.copied == 0 && source.reads == 0, "the copy is whole and its stamp stands: up to date, whatever the marker");
        CHECK_MSG(stuck.Count("RemoveMarker") == 1 && stuck.marker.has_value(), "it tried to remove the marker, and a failure changed nothing");
        CHECK_MSG(!stuck.Did("RemoveStamp") && stuck.Count("RemoveTree:") == 0 && stuck.Count("Begin:") == 0 && stuck.Count("WriteMarker") == 0, "and touched nothing else");
    }
    {
        stuck.markerRemovalFails = false;
        stuck.NextLife();
        MemSource source = SourceFor("stale", files);
        CHECK(Unpack::Run(source, stuck, Plain()).outcome == Unpack::Outcome::UpToDate);
        // mutation: leave the marker standing on the fast path.
        CHECK_MSG(!stuck.marker.has_value() && TreeProblem(stuck, files, "stale").empty(), "a start that can remove the stale marker does");
        stuck.NextLife();
        MemSource quiet = SourceFor("stale", files);
        CHECK_MSG(Unpack::Run(quiet, stuck, Plain()).outcome == Unpack::Outcome::UpToDate && stuck.ops.empty(), "and the start after it has nothing left to do");
    }
    // (c) A marker of the list's stamp beside a stamp whose copy was damaged since (a start stopped between the stamp and the marker's removal, then a cleaner):
    //     the marker says the tree is this build's, so the damage is mended by a continuation. The stamp is removed first, as on any continuation.
    {
        MarkerSink damaged = Finished(files, "stale");
        damaged.marker = "stale";
        damaged.files.erase(files[4].path);
        damaged.files[files[8].path].pop_back();
        // mutation: leave the stamp standing on a continuation (a stamp over a copy in progress), or wipe the tree.
        const Unpack::Result mended = TheNextStartFinishes(damaged, files, "stale", "a stamp, a marker of it, and two damaged files");
        CHECK_MSG(mended.resumed && mended.kept == files.size() - 2 && mended.copied == 2, "ten files kept, two copied again");
    }
}

// Each call of the marker that can fail: what it ends, and what it does not.
void EveryFailingCallOfTheMarkerSaysWhichAndStopsWhereItShould() {
    const std::vector<Mem> files = Invented(12);
    // The marker cannot be written: after the wipe and before the first file, so no file is begun without it.
    {
        MemSource source = SourceFor("new", files);
        MarkerSink sink;
        sink.markerWriteFails = true;
        const Unpack::Result r = Unpack::Run(source, sink, Plain());
        // mutation: carry on without the marker (all or nothing, said nowhere), or write it after the first file.
        CHECK_MSG(r.outcome == Unpack::Outcome::Failed && Has(r.error, "could not be marked") && Has(r.error, "the marker's file is read-only"), r.error);
        CHECK_MSG(sink.Count("Begin:") == 0 && source.reads == 0 && !sink.stamp.has_value() && !sink.marker.has_value(), "no file is begun, no asset read, no stamp, no marker");
        CHECK_EQ(sink.Count("RemoveTree:"), std::size_t{2});
    }
    // An old marker that cannot be removed: the start stops before a folder is touched, and the old build's marker and files stand.
    {
        MarkerSink sink;
        sink.marker = "old";
        sink.files["GameData/part0/leftover.dat"] = "of the old build";
        sink.markerRemovalFails = true;
        MemSource source = SourceFor("new", files);
        const Unpack::Result r = Unpack::Run(source, sink, Plain());
        // mutation: wipe first and remove the marker after.
        CHECK_MSG(r.outcome == Unpack::Outcome::Failed && Has(r.error, "could not be cleared") && Has(r.error, "held by another process"), r.error);
        CHECK_MSG(sink.Count("RemoveTree:") == 0 && sink.Count("Begin:") == 0, "no folder is removed, no file begun");
        CHECK_MSG(sink.marker && *sink.marker == "old" && sink.files.count("GameData/part0/leftover.dat") == 1, "the old marker and the old files stand");
    }
    // A marker that is written and cannot be read back: the unpack goes on as all or nothing, and does not say a stop is continuable.
    {
        MarkerSink sink;
        sink.markerReadBack = false;
        MemSource dying = SourceFor("new", files);
        dying.failAt = files[7].path;
        const Unpack::Result first = Unpack::Run(dying, sink, Plain());
        // mutation: take the write for the read (resumable without asking the sink).
        CHECK_MSG(first.outcome == Unpack::Outcome::Failed && !first.resumable, "a sink that cannot read its marker back is not continuable: " + first.error);
        MemSource fixed = SourceFor("new", files);
        const Unpack::Result second = Unpack::Run(fixed, sink, Plain());
        CHECK_MSG(second.outcome == Unpack::Outcome::Done && !second.resumed && second.kept == 0 && second.copied == files.size(), "and the next start copies everything: " + second.error);
    }
    // The stamp that cannot be removed on a continuation: the start stops before anything else is done, and the marker and the files stand.
    {
        MarkerSink sink;
        MemSource dying = SourceFor("new", files);
        dying.failAt = files[7].path;
        CHECK(Unpack::Run(dying, sink, Plain()).outcome == Unpack::Outcome::Failed);
        const auto filesBefore = sink.files;
        sink.NextLife();
        sink.stampRemovalFails = true;
        MemSource fixed = SourceFor("new", files);
        const Unpack::Result r = Unpack::Run(fixed, sink, Plain());
        CHECK_MSG(r.outcome == Unpack::Outcome::Failed && Has(r.error, "could not be cleared") && Has(r.error, "read-only"), r.error);
        CHECK_MSG(sink.Count("Begin:") == 0 && sink.files == filesBefore && sink.marker && *sink.marker == "new", "nothing was copied or removed, and the marker stands");
    }
}

// What the log line says of a continuation, and of a stop: counts that add up, and whether the next start goes on or begins again.
void TheSummaryTellsWhatWasKeptAndWhatTheNextStartWillDo() {
    Unpack::Result r;
    r.stamp = "abc";
    r.files = 900;
    r.copied = 31;
    r.copiedBytes = 4096;
    r.outcome = Unpack::Outcome::Done;
    CHECK_MSG(Unpack::Summary(r) == "Unpacked 31 files, 4096 bytes (stamp abc).", "an unpack that continued nothing is worded as it was: " + Unpack::Summary(r));
    r.resumed = true;
    r.kept = 8;
    // mutation: leave the kept files out of the line, or say it of a start that continued nothing.
    CHECK_MSG(Unpack::Summary(r) == "Unpacked 31 files, 4096 bytes (stamp abc); 8 more were kept from a start that was cut short.", Unpack::Summary(r));
    r.outcome = Unpack::Outcome::Cancelled;
    r.resumed = false;
    r.kept = 0;
    CHECK_MSG(Unpack::Summary(r) == "The app was closed during the unpack after 31 files; the next start unpacks again.", Unpack::Summary(r));
    r.resumable = true;
    CHECK_MSG(Unpack::Summary(r) == "The app was closed during the unpack after 31 files; the next start goes on from there.", Unpack::Summary(r));
    r.resumed = true;
    r.kept = 8;
    CHECK_MSG(Unpack::Summary(r) == "The app was closed during the unpack after 39 files (8 of them kept from a start that was cut short); the next start goes on from there.",
              Unpack::Summary(r));
    // Through a run: the same stop over a sink that keeps no marker and one that does.
    const std::vector<Mem> files = Invented(40);
    for (int keeps = 0; keeps < 2; ++keeps) {
        MemSource source = SourceFor("sum", files);
        Unpack::Options options = Plain();
        int calls = 0;
        options.pump = [&] { return ++calls < 2; };
        Unpack::Result stopped;
        if (keeps == 1) {
            MarkerSink sink;
            stopped = Unpack::Run(source, sink, options);
        } else {
            MemSink sink;
            stopped = Unpack::Run(source, sink, options);
        }
        CHECK_MSG(stopped.outcome == Unpack::Outcome::Cancelled, "the app finished");
        CHECK_MSG(Has(Unpack::Summary(stopped), "after 32 files") && Has(Unpack::Summary(stopped), keeps == 1 ? "goes on from there" : "unpacks again"), Unpack::Summary(stopped));
    }
}

// ---- A real folder ----------------------------------------------------------------------------------------------------------------------

struct Folder {
    fs::path dir;
    Folder() {
        std::error_code ec;
        dir = fs::temp_directory_path(ec) / ("mp-unpack-" + Files::UniqueSuffix());
        fs::create_directories(dir, ec);
    }
    ~Folder() {
        std::error_code ec;
        fs::remove_all(dir, ec);
    }
};

std::string Slurp(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

void TheFolderSinkWritesBytesAndSurvivesTheOwnersNeighbours() {
    Folder scratch;
    CHECK_MSG(fs::is_directory(scratch.dir), "a scratch folder");
    const std::vector<Mem> files = Invented(30);
    // The player's own files, beside the owned folders.
    fs::create_directories(scratch.dir / "Magic Portals");
    std::ofstream(scratch.dir / "Magic Portals" / "scores.json", std::ios::binary) << "the medals";
    std::ofstream(scratch.dir / "magicportals_args.txt", std::ios::binary) << "--level level0";
    const std::string stampName = "magicportals_gamedata.stamp";
    {
        MemSource source = SourceFor("real-1", files);
        Unpack::FsSink sink(scratch.dir.string(), stampName);
        const Unpack::Result result = Unpack::Run(source, sink, Plain());
        CHECK_MSG(result.outcome == Unpack::Outcome::Done, result.error);
        bool every = true;
        for (const Mem& file : files) every = every && Slurp(scratch.dir / file.path) == file.bytes;
        // mutation: write the files in text mode, or one byte short.
        CHECK_MSG(every, "every file is on the real disk with exactly its bytes");
        CHECK_MSG(Slurp(scratch.dir / stampName) == "real-1", "the stamp file holds the stamp and nothing else");
        // E49: the marker is there while the files are copied and gone once the stamp stands, so a finished unpack leaves none behind.
        CHECK_MSG(!fs::exists(scratch.dir / (stampName + Unpack::kMarkerSuffix)), "a finished unpack leaves no marker");
        // What the old copy holds, as the space check counts it: each file in whole blocks, a folder that is not there as nothing (PORT5-10 (b)).
        std::uint64_t heldGame = 0;
        std::uint64_t heldEngine = 0;
        for (const Mem& file : files) (file.path.compare(0, 9, "GameData/") == 0 ? heldGame : heldEngine) += Unpack::RoundUpToBlocks(file.bytes.size());
        const std::optional<std::uint64_t> game = sink.HeldBytes("GameData");
        const std::optional<std::uint64_t> engine = sink.HeldBytes("engine");
        const std::optional<std::uint64_t> nothing = sink.HeldBytes("neverthere");
        // mutation: sum the sizes without the blocks; count the stamp or the neighbours; answer "not known" for a folder that is not there.
        CHECK_MSG(game && *game == heldGame && game > 0, "the content folder holds its files in blocks");
        CHECK_MSG(engine && *engine == heldEngine && engine > 0, "and so does the engine's");
        CHECK_MSG(nothing && *nothing == 0, "a folder that is not there holds nothing");
    }
    // A second start: nothing to do. (A sink is made again, as a second process would.)
    {
        MemSource source = SourceFor("real-1", files);
        Unpack::FsSink sink(scratch.dir.string(), stampName);
        CHECK(Unpack::Run(source, sink, Plain()).outcome == Unpack::Outcome::UpToDate);
        CHECK_EQ(source.reads, 0);
    }
    // A file the cleaner took, and one cut short: made again. A stray file in an owned folder goes with the re-copy; the player's files do not.
    // (E49 did not move this: a stamp over a damaged copy has NO marker beside it, so this is a clean start, which wipes the stray file and copies every file.)
    fs::remove(scratch.dir / files[3].path);
    {
        std::ofstream cut(scratch.dir / files[6].path, std::ios::binary | std::ios::trunc);
        cut << "c";
    }
    std::ofstream(scratch.dir / "GameData" / "stray.dat", std::ios::binary) << "left over";
    {
        MemSource source = SourceFor("real-1", files);
        Unpack::FsSink sink(scratch.dir.string(), stampName);
        const Unpack::Result result = Unpack::Run(source, sink, Plain());
        CHECK_MSG(result.outcome == Unpack::Outcome::Done, "a damaged copy is made again: " + result.error);
        CHECK_MSG(Slurp(scratch.dir / files[3].path) == files[3].bytes && Slurp(scratch.dir / files[6].path) == files[6].bytes, "whole again");
        // mutation (E49): continue on a stamp's say-so, without a marker (the 28 whole files would be kept and the stray file left).
        CHECK_MSG(!result.resumed && result.kept == 0 && result.copied == files.size(), "no marker stood, so every file was copied: a clean start");
    }
    CHECK_MSG(!fs::exists(scratch.dir / "GameData" / "stray.dat"), "a stray file in an owned folder is gone");
    // mutation: remove the whole destination, or anything that is not an owned folder.
    CHECK_MSG(Slurp(scratch.dir / "Magic Portals" / "scores.json") == "the medals", "the player's medals are untouched");
    CHECK_MSG(Slurp(scratch.dir / "magicportals_args.txt") == "--level level0", "and so is the flags file");
}

void AFolderSinkThatLosesItsSourceLeavesNoStampAndNoHalfFile() {
    Folder scratch;
    const std::vector<Mem> files = Invented(18);
    const std::string stampName = "magicportals_gamedata.stamp";
    MemSource broken = SourceFor("real-2", files);
    broken.failAt = files[11].path;
    {
        Unpack::FsSink sink(scratch.dir.string(), stampName);
        const Unpack::Result result = Unpack::Run(broken, sink, Plain());
        CHECK(result.outcome == Unpack::Outcome::Failed);
    }
    CHECK_MSG(!fs::exists(scratch.dir / stampName), "no stamp file");
    CHECK_MSG(!fs::exists(scratch.dir / files[11].path), "no half file");
    CHECK_MSG(fs::exists(scratch.dir / files[10].path), "the files before it are there");
    // E49: and the marker the failed start leaves, with the list's stamp, which is what the next start goes on from.
    CHECK_MSG(Slurp(scratch.dir / (stampName + Unpack::kMarkerSuffix)) == "real-2", "the failed start's marker holds its stamp");
    // A stamp left by the earlier, good copy is gone after a re-copy that failed.
    {
        MemSource good = SourceFor("real-2", files);
        Unpack::FsSink sink(scratch.dir.string(), stampName);
        // CHANGED BY E49, deliberately: this run used to be a clean start that copied all 18 files over the 11 the failed start had left. It is now a CONTINUATION of
        // that start (the marker of the same stamp stands), keeps the 11 and copies the 7, and the outcome this test needed (Done, then a stamp) is the same.
        const Unpack::Result mended = Unpack::Run(good, sink, Plain());
        CHECK(mended.outcome == Unpack::Outcome::Done);
        CHECK_MSG(mended.resumed && mended.kept == 11 && mended.copied == 7, "continues the failed start: 11 kept, 7 copied");
        CHECK_MSG(!fs::exists(scratch.dir / (stampName + Unpack::kMarkerSuffix)), "and leaves no marker once the stamp stands");
        CHECK(fs::exists(scratch.dir / stampName));
        MemSource next = SourceFor("real-3", files);
        next.failAt = files[2].path;
        CHECK(Unpack::Run(next, sink, Plain()).outcome == Unpack::Outcome::Failed);
        CHECK_MSG(!fs::exists(scratch.dir / stampName), "the old stamp does not outlive a failed re-copy");
        // A build of another stamp over a finished tree is a clean start, so its failed start leaves ITS marker, not the old one.
        CHECK_MSG(Slurp(scratch.dir / (stampName + Unpack::kMarkerSuffix)) == "real-3", "the marker is the new build's");
    }
    // The sink alone: an empty stamp file reads as no match, a free-space figure is a figure, a missing file has no size.
    Unpack::FsSink sink(scratch.dir.string(), stampName);
    std::uint64_t size = 99;
    CHECK_MSG(!sink.SizeOf("GameData/does/not/exist", size), "a file that is not there has no size");
    const std::optional<std::uint64_t> free = sink.FreeBytes();
    CHECK_MSG(free.has_value() && *free > 0, "the scratch disk says how much is free");
}

// ---- The resume marker on a real folder (E49) -------------------------------------------------------------------------------------------------

// The names directly inside `dir`.
std::set<std::string> NamesIn(const fs::path& dir) {
    std::set<std::string> names;
    std::error_code ec;
    for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) names.insert(it->path().filename().string());
    return names;
}

// The sink alone: the marker is a file beside the stamp, named from the stamp's own name, read, written and removed as the stamp is, and never in an owned folder.
void TheFolderSinkKeepsItsMarkerBesideTheStampAndOutsideTheOwnedFolders() {
    Folder scratch;
    const std::string stampName = "magicportals_gamedata.stamp";
    const fs::path markerPath = scratch.dir / (stampName + ".partial");
    CHECK_MSG(std::string(Unpack::kMarkerSuffix) == ".partial", "the suffix");
    Unpack::FsSink sink(scratch.dir.string(), stampName);
    std::string read = "unchanged";
    std::string why;
    CHECK_MSG(!sink.ReadMarker(read), "no marker, no answer");
    // mutation: a marker that is not there is an error to remove.
    CHECK_MSG(sink.RemoveMarker(why), "removing none is nothing: " + why);
    CHECK_MSG(sink.WriteMarker("m-1", why), why);
    CHECK_MSG(fs::is_regular_file(markerPath) && markerPath.parent_path() == scratch.dir, "a file beside the stamp's place, in the files folder itself");
    // mutation: end the marker's line, or append to what is there.
    CHECK_MSG(Slurp(markerPath) == "m-1", "it holds the stamp and nothing else");
    CHECK_MSG(sink.ReadMarker(read) && read == "m-1", "and reads back");
    CHECK_MSG(sink.WriteMarker("m-2", why) && Slurp(markerPath) == "m-2", "a second write replaces it");
    // Outside the owned folders: removing them takes files in them and not the marker; the stamp's own writes and removals leave it alone too.
    fs::create_directories(scratch.dir / "GameData" / "x");
    std::ofstream(scratch.dir / "GameData" / "x" / "f.dat", std::ios::binary) << "f";
    CHECK_MSG(sink.RemoveTree("GameData", why) && sink.RemoveTree("engine", why), why);
    // mutation: keep the marker inside an owned folder.
    CHECK_MSG(fs::exists(markerPath) && !fs::exists(scratch.dir / "GameData"), "the owned folder goes and the marker, which is not in it, stays");
    CHECK_MSG(sink.WriteStamp("s", why) && sink.RemoveStamp(why) && fs::exists(markerPath), "the stamp's write and removal leave the marker");
    // An empty marker file reads as the empty text, which is no list's stamp.
    {
        std::ofstream emptied(markerPath, std::ios::binary | std::ios::trunc);
    }
    CHECK_MSG(sink.ReadMarker(read) && read.empty(), "an empty marker is the empty text");
    CHECK_MSG(sink.RemoveMarker(why) && !fs::exists(markerPath), "removed");
    CHECK_MSG(sink.RemoveMarker(why), "and removing it again is nothing");
    // Something that is not a file at the marker's name: it is no marker, and a removal that cannot be done says why.
    fs::create_directories(markerPath / "occupied");
    CHECK_MSG(!sink.ReadMarker(read), "a folder of that name is not a marker");
    why.clear();
    CHECK_MSG(!sink.RemoveMarker(why) && Has(why, ".partial"), "a removal that cannot be done is a false that names the file: " + why);
    // A first start's files folder does not exist yet: writing the marker makes it.
    const fs::path deeper = scratch.dir / "new" / "deeper";
    Unpack::FsSink fresh(deeper.string(), stampName);
    CHECK_MSG(fresh.WriteMarker("m-3", why), why);
    CHECK_MSG(Slurp(deeper / (stampName + ".partial")) == "m-3", "the folder was made");
}

// An unpack on a real folder: the marker stands while the copy is unfinished and not otherwise, and a second sink over the same folder, as a second process
// would be, continues what the first left, including a file half written under its own name.
void AnUnpackOnARealFolderHoldsTheMarkerWhileItIsUnfinishedAndAnotherProcessContinuesIt() {
    Folder scratch;
    const std::vector<Mem> files = Invented(18);
    const std::string stampName = "magicportals_gamedata.stamp";
    const fs::path markerPath = scratch.dir / (stampName + ".partial");
    const fs::path stampPath = scratch.dir / stampName;
    fs::create_directories(scratch.dir / "Magic Portals");
    std::ofstream(scratch.dir / "Magic Portals" / "scores.json", std::ios::binary) << "the medals";
    std::ofstream(scratch.dir / "magicportals_args.txt", std::ios::binary) << "--level level0";
    // Looked at from inside the unpack, at every asset it reads: the marker stands with the list's stamp, and no stamp stands over the copy.
    const auto whileCopying = [&](const std::string&) {
        CHECK_MSG(fs::exists(markerPath) && Slurp(markerPath) == "fs-1", "the marker stands, with the list's stamp, while the files are copied");
        CHECK_MSG(!fs::exists(stampPath), "and no stamp stands over a copy in progress");
    };
    // 1. A first start is stopped at file 11 (the source dies).
    {
        MemSource dying = SourceFor("fs-1", files);
        dying.failAt = files[11].path;
        dying.onRead = whileCopying;
        Unpack::FsSink sink(scratch.dir.string(), stampName);
        const Unpack::Result first = Unpack::Run(dying, sink, Plain());
        CHECK_MSG(first.outcome == Unpack::Outcome::Failed && first.resumable && !first.resumed, first.error);
    }
    CHECK_MSG(Slurp(markerPath) == "fs-1" && !fs::exists(stampPath), "what stands: the marker, no stamp");
    CHECK_MSG(markerPath.parent_path() == scratch.dir, "the marker is beside the stamp, in the files folder");
    CHECK_MSG(fs::exists(scratch.dir / files[10].path) && !fs::exists(scratch.dir / files[11].path), "eleven files, no half one");
    // What a killed process leaves that a failed copy does not: a file half written under its own name.
    std::ofstream(scratch.dir / files[11].path, std::ios::binary) << files[11].bytes.substr(0, 2);
    // 2. Another process: the unpack goes on from where it was.
    {
        MemSource fixed = SourceFor("fs-1", files);
        fixed.onRead = whileCopying;
        Unpack::FsSink sink(scratch.dir.string(), stampName);
        const Unpack::Result second = Unpack::Run(fixed, sink, Plain());
        // mutation: wipe on a continuation, keep the half file, or copy every file again.
        CHECK_MSG(second.outcome == Unpack::Outcome::Done && second.resumed && second.kept == 11 && second.copied == 7, "eleven kept, the half file and the six after it copied: " + second.error);
        CHECK_EQ(fixed.reads, 7);
        bool every = true;
        for (const Mem& file : files) every = every && Slurp(scratch.dir / file.path) == file.bytes;
        CHECK_MSG(every, "every file is on the real disk with exactly its bytes");
        CHECK_MSG(Slurp(stampPath) == "fs-1" && !fs::exists(markerPath), "the stamp stands and the marker is gone");
        const std::set<std::string> expected = {"GameData", "engine", "Magic Portals", "magicportals_args.txt", stampName};
        CHECK_MSG(NamesIn(scratch.dir) == expected, "and nothing else is in the files folder: no temporary, no marker");
    }
    // 3. A marker left beside the finished copy (a start stopped between the stamp and the marker's removal) is removed by the next start, which does nothing else.
    std::ofstream(markerPath, std::ios::binary) << "fs-1";
    {
        MemSource again = SourceFor("fs-1", files);
        Unpack::FsSink sink(scratch.dir.string(), stampName);
        const Unpack::Result up = Unpack::Run(again, sink, Plain());
        CHECK_MSG(up.outcome == Unpack::Outcome::UpToDate && again.reads == 0 && !fs::exists(markerPath), "up to date, and the stale marker is gone");
    }
    // 4. The same marker beside a copy a cleaner has damaged since: it is the list's stamp, so the damage is mended by a continuation.
    std::ofstream(markerPath, std::ios::binary) << "fs-1";
    {
        std::ofstream cut(scratch.dir / files[3].path, std::ios::binary | std::ios::trunc);
        cut << "c";
    }
    fs::remove(scratch.dir / files[5].path);
    {
        MemSource mend = SourceFor("fs-1", files);
        Unpack::FsSink sink(scratch.dir.string(), stampName);
        const Unpack::Result mended = Unpack::Run(mend, sink, Plain());
        CHECK_MSG(mended.outcome == Unpack::Outcome::Done && mended.resumed && mended.kept == 16 && mended.copied == 2 && mend.reads == 2, mended.error);
        CHECK_MSG(Slurp(scratch.dir / files[3].path) == files[3].bytes && Slurp(scratch.dir / files[5].path) == files[5].bytes, "both whole again");
        CHECK_MSG(Slurp(stampPath) == "fs-1" && !fs::exists(markerPath), "stamp, no marker");
    }
    CHECK_MSG(Slurp(scratch.dir / "Magic Portals" / "scores.json") == "the medals" && Slurp(scratch.dir / "magicportals_args.txt") == "--level level0",
              "the player's files, beside the owned folders, were never touched");
}

// Another build over a start that was cut short: a clean start on a real folder. A file of the right size from the other build is not kept, a file the new build
// lacks is gone, and the player's files stand.
void AnotherBuildOverACutShortStartOnARealFolderIsACleanStart() {
    Folder scratch;
    const std::vector<Mem> a = Invented(18);
    std::vector<Mem> b = OtherBuild(a);
    b.erase(b.begin() + 3);
    const std::string stampName = "magicportals_gamedata.stamp";
    const fs::path markerPath = scratch.dir / (stampName + ".partial");
    fs::create_directories(scratch.dir / "Magic Portals");
    std::ofstream(scratch.dir / "Magic Portals" / "scores.json", std::ios::binary) << "the medals";
    {
        MemSource dying = SourceFor("fs-a", a);
        dying.failAt = a[11].path;
        Unpack::FsSink sink(scratch.dir.string(), stampName);
        CHECK(Unpack::Run(dying, sink, Plain()).outcome == Unpack::Outcome::Failed);
    }
    CHECK_MSG(Slurp(markerPath) == "fs-a" && fs::exists(scratch.dir / a[3].path), "build A's start stands, with a file build B lacks");
    std::ofstream(scratch.dir / "GameData" / "stray.dat", std::ios::binary) << "left over";
    {
        MemSource fresh = SourceFor("fs-b", b);
        Unpack::FsSink sink(scratch.dir.string(), stampName);
        const Unpack::Result result = Unpack::Run(fresh, sink, Plain());
        // mutation: continue on any marker; wipe after writing the new marker; keep files by size alone.
        CHECK_MSG(result.outcome == Unpack::Outcome::Done && !result.resumed && result.kept == 0 && result.copied == b.size(), result.error);
        bool every = true;
        for (const Mem& file : b) every = every && Slurp(scratch.dir / file.path) == file.bytes;
        CHECK_MSG(every, "every file is build B's, bytes and all: none of build A's was kept");
        CHECK_MSG(!fs::exists(scratch.dir / a[3].path) && !fs::exists(scratch.dir / "GameData" / "stray.dat"), "the file B lacks and the stray file are gone");
        CHECK_MSG(Slurp(scratch.dir / stampName) == "fs-b" && !fs::exists(markerPath), "B's stamp stands and no marker");
    }
    CHECK_MSG(Slurp(scratch.dir / "Magic Portals" / "scores.json") == "the medals", "the player's medals stand");
}

// ---- The developer's flags file ---------------------------------------------------------------------------------------------------------

void TheFlagsFileIsWhitespaceSeparatedWithDoubleQuotes() {
    const auto args = [&](const std::string& text) {
        const Unpack::ArgsParsed parsed = Unpack::ParseArgsText(text);
        CHECK_MSG(parsed.ok, "'" + text + "': " + parsed.error);
        return parsed.args;
    };
    using V = std::vector<std::string>;
    // mutation: split on newlines only (a line of several flags would be one argument), or on spaces only.
    CHECK_MSG(args("--level level0 --frames 300") == (V{"--level", "level0", "--frames", "300"}), "several to a line");
    CHECK_MSG(args("--level\nlevel0\n--frames\n300\n") == (V{"--level", "level0", "--frames", "300"}), "one to a line");
    CHECK_MSG(args("--level level0\r\n--frames\t300\r\n\r\n") == (V{"--level", "level0", "--frames", "300"}), "CRLF, tabs and blank lines");
    CHECK_MSG(args("").empty() && args("  \n\t\r\n ").empty(), "nothing, or only space, is no flags");
    // mutation: keep the quotes in the token; split inside quotes.
    CHECK_MSG(args("--screenshot \"/data/user/0/x y/files/a b.png\" --frames 3") == (V{"--screenshot", "/data/user/0/x y/files/a b.png", "--frames", "3"}), "a quoted token holds spaces");
    CHECK_MSG(args("\"two\nlines\" a") == (V{"two\nlines", "a"}), "and a newline");
    CHECK_MSG(args("a \"\" b") == (V{"a", "", "b"}), "an empty pair of quotes is an empty token");
    CHECK_MSG(args("x\"y z\"w") == (V{"xy zw"}), "a quote opens and closes inside a token");
    // The two errors: the file is then not used (the glue logs it).
    // mutation: accept an unclosed quote (the rest of the file becomes one flag).
    const Unpack::ArgsParsed open = Unpack::ParseArgsText("--level \"level 0 --frames 3");
    CHECK_MSG(!open.ok && Has(open.error, "quote") && open.args.empty(), "an unclosed quote: " + open.error);
    const Unpack::ArgsParsed nul = Unpack::ParseArgsText(std::string("--level a") + '\0' + "b");
    CHECK_MSG(!nul.ok && Has(nul.error, "NUL") && nul.args.empty(), "a NUL: " + nul.error);
}

// ---- Planning step 204: what the verification of step 203 found (D1 to D6, PORT5-10) and what the glue decided without a test (D3) ------------------------------

// D4. The verifier's probe: 18,446 lines of the largest size and one of 744,073,709,570,061 add up to exactly 2^64 - 1 as they are, which the old check (the
// raw sum against 64 bits) let through; rounded up to blocks they add to 2^64 + 20,480, and the estimate wrapped to 20,480, so the free-space check was skipped.
void ASumThatWouldWrapTheEstimateIsRefusedWhenTheListIsRead() {
    std::string probe = "s\n";
    std::uint64_t raw = 0;
    for (int i = 0; i < 18446; ++i) {
        probe += "999999999999999\tGameData/f" + std::to_string(i) + "\n";
        raw += 999999999999999ull;
    }
    probe += "744073709570061\tGameData/last\n";
    raw += 744073709570061ull;
    CHECK_MSG(raw == ~std::uint64_t{0}, "the probe's premise: the sizes add up to exactly 2^64 - 1 as they are");
    const Unpack::Parsed parsed = Unpack::ParseList(probe, kOwned);
    // mutation: leave the rounded sum unchecked (only the raw one), or check it with a wrapping addition.
    CHECK_MSG(!parsed.ok, "a list whose estimate would wrap is refused");
    CHECK_MSG(Has(parsed.error, "allocation blocks") && Has(parsed.error, "line 18448"), "in words that name the line: " + parsed.error);
    CHECK_MSG(parsed.list.entries.empty(), "and hands nothing on");
    // The same through the run: nothing on the disk is touched.
    MemSource source;
    source.list = probe;
    MemSink sink;
    const Unpack::Result result = Unpack::Run(source, sink, Plain());
    CHECK_MSG(result.outcome == Unpack::Outcome::Failed && Has(result.error, "cannot be used"), "the run refuses it: " + result.error);
    CHECK_MSG(sink.ops.empty(), "before anything is removed");
    // One line fewer fits, in both senses: the estimate is then the real, large, sum.
    std::string fits = "s\n";
    for (int i = 0; i < 18445; ++i) fits += "999999999999999\tGameData/f" + std::to_string(i) + "\n";
    const Unpack::Parsed fine = Unpack::ParseList(fits, kOwned);
    // mutation: refuse any list whose rounded sum passes 2^63.
    CHECK_MSG(fine.ok, fine.error);
    CHECK_EQ(Unpack::NeededBytes(fine.list), std::uint64_t{18445} * 1000000000000000ull);
    // A List made by hand (not through ParseList) whose estimate would pass 64 bits answers the largest number, not a wrapped one.
    Unpack::List hand;
    hand.entries = {{"GameData/a", std::uint64_t{1} << 63}, {"GameData/b", std::uint64_t{1} << 63}};
    // mutation: add without the saturation (the sum is then 0).
    CHECK_EQ(Unpack::NeededBytes(hand), ~std::uint64_t{0});
    // mutation: round with (size + block - 1) / block * block, which wraps for a size near 2^64 (the answer is then 0).
    CHECK_EQ(Unpack::RoundUpToBlocks(0), std::uint64_t{0});
    CHECK_EQ(Unpack::RoundUpToBlocks(1), Unpack::kBlockBytes);
    CHECK_EQ(Unpack::RoundUpToBlocks(Unpack::kBlockBytes), Unpack::kBlockBytes);
    CHECK_EQ(Unpack::RoundUpToBlocks(Unpack::kBlockBytes + 1), 2 * Unpack::kBlockBytes);
    CHECK_EQ(Unpack::RoundUpToBlocks(~std::uint64_t{0}), ~std::uint64_t{0} - (Unpack::kBlockBytes - 1));
}

// D5. The list's text is capped where its bytes arrive, and again by the run on whatever a source hands back.
void TheListTextIsCappedWhereItsBytesArriveAndAgainByTheRun() {
    std::string text = "abc";
    std::string why;
    const std::string rest(Unpack::kMostListTextBytes - 3, 'x');
    // mutation: no cap; or a cap one byte early (< for <=).
    CHECK_MSG(Unpack::AppendListChunk(text, rest.data(), rest.size(), why), "a chunk that brings the text exactly to the cap is taken");
    CHECK_EQ(text.size(), Unpack::kMostListTextBytes);
    CHECK_MSG(!Unpack::AppendListChunk(text, "y", 1, why) && Has(why, "longer than"), "one byte past the cap is refused, in words: " + why);
    CHECK_EQ(text.size(), Unpack::kMostListTextBytes);
    std::string near(Unpack::kMostListTextBytes - 2, 'a');
    CHECK_MSG(!Unpack::AppendListChunk(near, "abc", 3, why), "a chunk that does not fit whole is refused whole");
    // mutation: append the part that fits.
    CHECK_EQ(near.size(), Unpack::kMostListTextBytes - 2);
    CHECK_MSG(Unpack::AppendListChunk(near, "ab", 2, why) && near.size() == Unpack::kMostListTextBytes, "and the part that does fit is a chunk of its own");

    // The run. A list padded with blank lines (which the parser skips) to exactly the cap is read; one byte more is refused before it is parsed.
    const std::vector<Mem> files = Invented(6);
    MemSource exact = SourceFor("cap", files);
    exact.list += std::string(Unpack::kMostListTextBytes - exact.list.size(), '\n');
    CHECK_EQ(exact.list.size(), Unpack::kMostListTextBytes);
    MemSink okSink;
    CHECK_MSG(Unpack::Run(exact, okSink, Plain()).outcome == Unpack::Outcome::Done, "a list at the cap is unpacked");
    MemSource over = SourceFor("cap", files);
    over.list += std::string(Unpack::kMostListTextBytes + 1 - over.list.size(), '\n');
    MemSink sink;
    const Unpack::Result result = Unpack::Run(over, sink, Plain());
    // mutation: no check in the run (the blank lines parse, and the unpack is Done).
    CHECK_MSG(result.outcome == Unpack::Outcome::Failed && Has(result.error, "longer than"), "one byte over is refused: " + result.error);
    CHECK_MSG(sink.ops.empty() && over.reads == 0, "and nothing is touched");
}

// D5. An asset longer than its listed size is stopped at the chunk that passes the size, not read to its end.
void AnAssetPastItsListedSizeIsRefusedAtTheChunkThatPassesIt() {
    const std::vector<Mem> files = Invented(8);
    MemSource source = SourceFor("e", files);
    source.endless = files[3].path;
    source.assets[files[3].path] = files[3].bytes; // the list still says its real size
    MemSink sink;
    const Unpack::Result result = Unpack::Run(source, sink, Plain());
    const std::size_t listed = files[3].bytes.size();
    // mutation: count the bytes only when the source returns (the endless source then gives all its chunks, and the disk is given every one).
    CHECK_MSG(result.outcome == Unpack::Outcome::Failed, "an asset that never ends fails the unpack");
    CHECK_MSG(Has(result.error, files[3].path) && Has(result.error, "damaged") && Has(result.error, "longer"), "in words that name the file: " + result.error);
    CHECK_MSG(source.endlessChunks <= listed / source.chunk + 1, "the source was asked for no more than one chunk past the size: " + std::to_string(source.endlessChunks));
    // mutation: write the chunk that passes the size before refusing it.
    CHECK_MSG(sink.peak <= listed, "and the disk was never given a byte past the listed size: " + std::to_string(sink.peak) + " of " + std::to_string(listed));
    CHECK_MSG(sink.files.count(files[3].path) == 0, "the half file is removed");
    CHECK_MSG(!sink.stamp.has_value() && sink.Count("Begin:") == 4, "no stamp, and no file after it is begun");
}

// D1. Each call of the sink that can fail ends the run, with the sink's own reason in the words, and nothing after it is done.
void EveryFailingCallOfTheSinkEndsTheRunAndSaysWhich() {
    const std::vector<Mem> files = Invented(23);
    // mutation (each): ignore the failure of that call.
    {
        MemSource source = SourceFor("old", files);
        MemSink sink;
        CHECK(Unpack::Run(source, sink, Plain()).outcome == Unpack::Outcome::Done);
        sink.ops.clear();
        sink.stampRemovalFails = true;
        source = SourceFor("new", files);
        const Unpack::Result r = Unpack::Run(source, sink, Plain());
        CHECK_MSG(r.outcome == Unpack::Outcome::Failed && Has(r.error, "could not be cleared") && Has(r.error, "read-only"), "the old stamp that cannot be removed: " + r.error);
        CHECK_MSG(sink.Count("Begin:") == 0 && sink.Count("RemoveTree:") == 0, "no folder is removed and no copy begins while the old stamp stands");
        CHECK_MSG(sink.stamp && *sink.stamp == "old" && source.reads == 0, "and the old copy is as it was");
    }
    for (const std::string& root : {std::string("GameData"), std::string("engine")}) {
        MemSource source = SourceFor("new", files);
        MemSink sink;
        sink.treeRemovalFails = root;
        const Unpack::Result r = Unpack::Run(source, sink, Plain());
        CHECK_MSG(r.outcome == Unpack::Outcome::Failed && Has(r.error, "could not be cleared") && Has(r.error, root + " is held"), "an owned folder that cannot be removed (" + root + "): " + r.error);
        CHECK_MSG(sink.Count("Begin:") == 0 && !sink.stamp.has_value(), "no copy begins, and no stamp is written");
        CHECK_MSG(root == "engine" ? sink.Did("RemoveTree:GameData") : !sink.Did("RemoveTree:engine"), "the folders are removed in order, and the walk stops at the one that failed");
    }
    {
        MemSource source = SourceFor("new", files);
        MemSink sink;
        sink.beginFails = files[4].path;
        const Unpack::Result r = Unpack::Run(source, sink, Plain());
        CHECK_MSG(r.outcome == Unpack::Outcome::Failed && Has(r.error, files[4].path) && Has(r.error, "cannot open the file for writing"), "a file that cannot be begun: " + r.error);
        CHECK_EQ(r.copied, std::size_t{4});
        CHECK_MSG(sink.Count("Begin:") == 5 && !sink.stamp.has_value() && sink.files.count(files[4].path) == 0, "no file after it is begun, no stamp");
        CHECK_MSG(source.reads == 4, "and the source was not asked for it");
    }
    {
        MemSource source = SourceFor("new", files);
        MemSink sink;
        sink.endKeepFails = files[4].path;
        const Unpack::Result r = Unpack::Run(source, sink, Plain());
        CHECK_MSG(r.outcome == Unpack::Outcome::Failed && Has(r.error, files[4].path) && Has(r.error, "the disk gave up at the close"), "a file that cannot be closed whole: " + r.error);
        CHECK_EQ(r.copied, std::size_t{4});
        CHECK_MSG(sink.Count("Begin:") == 5 && !sink.stamp.has_value(), "it is not counted as copied, no file after it is begun, no stamp");
    }
    {
        MemSource source = SourceFor("new", files);
        MemSink sink;
        sink.writeStampFails = true;
        const Unpack::Result r = Unpack::Run(source, sink, Plain());
        CHECK_MSG(r.outcome == Unpack::Outcome::Failed && Has(r.error, "could not be recorded") && Has(r.error, "the stamp's file is read-only"), "a stamp that cannot be written: " + r.error);
        CHECK_MSG(r.copied == files.size() && !sink.stamp.has_value(), "every file was copied, and there is no stamp, so the next start copies again");
    }
}

// PORT5-10 (b). Space is checked before anything is cleared; what the old copy holds counts as given back.
void SpaceIsCheckedBeforeAnythingIsClearedAndTheOldCopyCountsAsGivenBack() {
    const std::vector<Mem> oldFiles = Invented(12);
    const std::vector<Mem> newFiles = Invented(30);
    MemSource oldSource = SourceFor("old", oldFiles);
    MemSink sink;
    CHECK(Unpack::Run(oldSource, sink, Plain()).outcome == Unpack::Outcome::Done);
    std::uint64_t held = 0;
    for (const Mem& file : oldFiles) held += Unpack::RoundUpToBlocks(file.bytes.size());
    std::uint64_t needed = 0;
    for (const Mem& file : newFiles) needed += Unpack::RoundUpToBlocks(file.bytes.size());
    CHECK_MSG(needed > held, "the update is the bigger one");
    const auto filesBefore = sink.files;
    const auto stampBefore = sink.stamp;

    // One byte short even with the old copy counted: refused, and the old copy is whole, with its stamp.
    sink.ops.clear();
    sink.free = needed - held - 1;
    MemSource newSource = SourceFor("new", newFiles);
    const Unpack::Result refused = Unpack::Run(newSource, sink, Plain());
    // mutation: clear first and check after (the old order); ignore what the old copy gives back; count it twice.
    CHECK_MSG(refused.outcome == Unpack::Outcome::Failed && Has(refused.error, "not enough free space"), "one byte short is refused: " + refused.error);
    CHECK_MSG(Has(refused.error, "more than the old copy of the game already takes"), "and the words say the old copy was counted: " + refused.error);
    CHECK_EQ(refused.reclaimable, held);
    CHECK_EQ(refused.needed, needed);
    CHECK_MSG(!sink.Did("RemoveStamp") && sink.Count("RemoveTree:") == 0 && sink.Count("Begin:") == 0 && !sink.Did("WriteStamp"), "nothing was removed or written");
    CHECK_MSG(sink.files == filesBefore && sink.stamp == stampBefore, "the old copy and its stamp are as they were");
    CHECK_EQ(newSource.reads, 0);

    // Exactly enough with the old copy counted: the update goes through.
    sink.ops.clear();
    sink.free = needed - held;
    const Unpack::Result done = Unpack::Run(newSource, sink, Plain());
    CHECK_MSG(done.outcome == Unpack::Outcome::Done && sink.stamp && *sink.stamp == "new", "exactly enough, counting the old copy's bytes, is enough: " + done.error);

    // A sink that cannot say what it holds counts none of it.
    MemSink blind;
    MemSource first = SourceFor("old", oldFiles);
    CHECK(Unpack::Run(first, blind, Plain()).outcome == Unpack::Outcome::Done);
    blind.heldUnknown = true;
    blind.free = needed - 1;
    MemSource second = SourceFor("new", newFiles);
    const Unpack::Result blindRefused = Unpack::Run(second, blind, Plain());
    // mutation: treat an unknown holding as the whole disk.
    CHECK_MSG(blindRefused.outcome == Unpack::Outcome::Failed && blindRefused.reclaimable == 0 && !Has(blindRefused.error, "old copy"), "nothing is counted as given back that the sink did not state: " + blindRefused.error);
    CHECK_MSG(blind.stamp && *blind.stamp == "old", "and the old stamp stands");
}

// D6. The sentence rounds the need UP and the free figure DOWN.
void TheFreeSpaceSentenceNeverSaysThePhoneHasWhatItLacks() {
    // mutation: round the free figure up (the old sentence), or the need down.
    CHECK_MSG(Unpack::MegabytesNeeded(1) == std::string("1 MB"), Unpack::MegabytesNeeded(1));
    CHECK_MSG(Unpack::MegabytesNeeded(1048576) == std::string("1 MB"), Unpack::MegabytesNeeded(1048576));
    CHECK_MSG(Unpack::MegabytesNeeded(1048577) == std::string("2 MB"), Unpack::MegabytesNeeded(1048577));
    CHECK_MSG(Unpack::MegabytesFree(0) == std::string("less than 1 MB"), Unpack::MegabytesFree(0));
    CHECK_MSG(Unpack::MegabytesFree(1048575) == std::string("less than 1 MB"), Unpack::MegabytesFree(1048575));
    CHECK_MSG(Unpack::MegabytesFree(1048576) == std::string("1 MB"), Unpack::MegabytesFree(1048576));
    CHECK_MSG(Unpack::MegabytesFree(2097151) == std::string("1 MB"), Unpack::MegabytesFree(2097151));
    // Whatever is short, the two numbers differ: the need is strictly above the free figure.
    bool never = true;
    for (const std::uint64_t need : {std::uint64_t{1}, std::uint64_t{4096}, std::uint64_t{1048576}, std::uint64_t{1048577}, std::uint64_t{24707748}, std::uint64_t{3686400}}) {
        for (const std::uint64_t spare : {std::uint64_t{0}, std::uint64_t{1}, need / 2, need - 1}) {
            if (spare >= need) continue;
            never = never && Unpack::MegabytesFree(spare) != Unpack::MegabytesNeeded(need);
        }
    }
    CHECK_MSG(never, "'needs about 24 MB and 24 MB are free' cannot be printed");
    // From a run: 300 files of 10,000 bytes take 3,686,400 bytes (3.52 MB) in blocks, and 3,686,399 are free (3.52 MB): the old sentence said "4 MB and 4 MB".
    std::vector<Mem> many;
    for (int i = 0; i < 300; ++i) many.push_back({"GameData/f" + std::to_string(i), std::string(10000, 'q')});
    MemSource source = SourceFor("f", many);
    MemSink sink;
    sink.free = 3686399;
    const Unpack::Result result = Unpack::Run(source, sink, Plain());
    CHECK(result.outcome == Unpack::Outcome::Failed);
    CHECK_MSG(Has(result.error, "about 4 MB and 3 MB are free"), result.error);
    sink.free = 700000;
    MemSource again = SourceFor("f", many);
    CHECK_MSG(Has(Unpack::Run(again, sink, Plain()).error, "about 4 MB and less than 1 MB are free"), "a disk with under a megabyte says so");
}

// D2. The list the PACKAGER makes (tools/package/android_package.py make_list) for four invented files, as text: tools/package/test_android_package.py
// regenerates it from the same four files with the packager's own function and holds it equal to this, so the packager and this parser cannot drift apart
// unseen. The lines are escaped so that no line ending of a checkout can change them.
// BEGIN PACKAGER LIST
const char* const kPackagerList =
    "ed16440fba8916323446bd8f95d91887\n"
    "7\tGameData/a/#b\n"
    "1234\tGameData/levels/level0.tscn\n"
    "0\tGameData/odd name/b c.png\n"
    "4096\tengine/assets/shaders/x.spv\n";
// END PACKAGER LIST

void TheListThePackagerMakesIsTheListThisParses() {
    const Unpack::Parsed parsed = Unpack::ParseList(kPackagerList, kOwned);
    // mutation: change the separator, the digit cap or the stamp rule in the parser (the packager's side is tied by the Python test).
    CHECK_MSG(parsed.ok, parsed.error);
    CHECK_MSG(parsed.list.stamp == "ed16440fba8916323446bd8f95d91887", "the stamp: 32 hex digits");
    CHECK_EQ(parsed.list.entries.size(), std::size_t{4});
    CHECK_EQ(parsed.list.bytes, std::uint64_t{1234 + 7 + 4096});
    bool every = parsed.list.entries.size() == 4;
    if (every) {
        every = parsed.list.entries[0].path == "GameData/a/#b" && parsed.list.entries[0].size == 7 && parsed.list.entries[1].path == "GameData/levels/level0.tscn" &&
                parsed.list.entries[1].size == 1234 && parsed.list.entries[2].path == "GameData/odd name/b c.png" && parsed.list.entries[2].size == 0 &&
                parsed.list.entries[3].path == "engine/assets/shaders/x.spv" && parsed.list.entries[3].size == 4096;
    }
    CHECK_MSG(every, "every line: a path with a space, a '#', an empty file, a shader");
    // And the run over it, with invented bytes of the listed sizes: the whole way, as the phone's unpack goes.
    MemSource source;
    source.list = kPackagerList;
    for (const Unpack::Entry& entry : parsed.list.entries) source.assets[entry.path] = std::string(static_cast<std::size_t>(entry.size), 'z');
    MemSink sink;
    const Unpack::Result result = Unpack::Run(source, sink, Plain());
    CHECK_MSG(result.outcome == Unpack::Outcome::Done && result.copied == 4 && sink.stamp && *sink.stamp == "ed16440fba8916323446bd8f95d91887", "unpacked: " + result.error);
}

// D3. The developer's flags file TAKEN: the rules that were in the Android glue.
void TheFlagsFileIsReadOnceAndConsumedWhateverItHeld() {
    const std::string argsName = "magicportals_args.txt";
    const std::string usedName = "magicportals_args.used";
    {
        Folder scratch;
        const std::string dir = scratch.dir.string();
        const Unpack::FlagsFile none = Unpack::TakeFlagsFile(dir, argsName, usedName);
        // mutation: call a missing file present (every launch would then be a scripted one).
        CHECK_MSG(!none.present && none.args.empty() && none.info.empty() && none.error.empty() && none.warning.empty(), "no file: not a scripted run, nothing to say");
        CHECK_MSG(!fs::exists(scratch.dir / usedName), "and nothing is made");

        std::ofstream(scratch.dir / argsName, std::ios::binary) << "--level level0 --frames 3\r\n--screenshot \"/a b/c.png\"\r\n";
        const Unpack::FlagsFile taken = Unpack::TakeFlagsFile(dir, argsName, usedName);
        CHECK_MSG(taken.present && taken.error.empty() && taken.warning.empty(), "a file there: a scripted run: " + taken.error + taken.warning);
        using V = std::vector<std::string>;
        CHECK_MSG(taken.args == (V{"--level", "level0", "--frames", "3", "--screenshot", "/a b/c.png"}), "its flags, quotes and all");
        CHECK_MSG(Has(taken.info, (scratch.dir / argsName).string()) && Has(taken.info, "--frames 3"), "the log line names the file and the flags: " + taken.info);
        // mutation: leave the file where it is (the next launch would run it again), or lose its bytes.
        CHECK_MSG(!fs::exists(scratch.dir / argsName), "the file is consumed");
        CHECK_MSG(Slurp(scratch.dir / usedName) == "--level level0 --frames 3\r\n--screenshot \"/a b/c.png\"\r\n", "and kept as .used, byte for byte");
        CHECK_MSG(!Unpack::TakeFlagsFile(dir, argsName, usedName).present, "so the next launch is not scripted");

        // A second scripted run replaces the first one's .used.
        std::ofstream(scratch.dir / argsName, std::ios::binary) << "--level level1";
        const Unpack::FlagsFile second = Unpack::TakeFlagsFile(dir, argsName, usedName);
        CHECK_MSG(second.present && second.args == (V{"--level", "level1"}) && second.warning.empty(), "a second file is taken: " + second.warning);
        CHECK_MSG(Slurp(scratch.dir / usedName) == "--level level1", "and replaces the earlier .used");

        // An empty file is a scripted run with no flags.
        std::ofstream(scratch.dir / argsName, std::ios::binary) << "";
        const Unpack::FlagsFile empty = Unpack::TakeFlagsFile(dir, argsName, usedName);
        CHECK_MSG(empty.present && empty.args.empty() && empty.error.empty() && !fs::exists(scratch.dir / argsName), "an empty file: scripted, no flags, no complaint, consumed");
    }
    {
        // A file that does not parse is ignored, SAID, present and consumed.
        Folder scratch;
        std::ofstream(scratch.dir / argsName, std::ios::binary) << "--level \"level 0 --frames 3";
        const Unpack::FlagsFile broken = Unpack::TakeFlagsFile(scratch.dir.string(), argsName, usedName);
        // mutation: take the arguments of a file that did not parse; do not consume a broken file.
        CHECK_MSG(broken.present && broken.args.empty() && Has(broken.error, "ignored") && Has(broken.error, "quote"), "an unclosed quote: " + broken.error);
        CHECK_MSG(!fs::exists(scratch.dir / argsName) && fs::exists(scratch.dir / usedName), "and it does not repeat either");
    }
    {
        // A file that cannot be read is still present (the run is a scripted one), ignored, said and consumed. The reader is handed in: a permission cannot be
        // made the same way on every system.
        Folder scratch;
        std::ofstream(scratch.dir / argsName, std::ios::binary) << "--level level0";
        const Unpack::FlagsFile blind =
            Unpack::TakeFlagsFile(scratch.dir.string(), argsName, usedName, [](const std::string&, std::string&) { return false; });
        // mutation: call an unreadable file absent; or parse the empty text as if it were the file.
        CHECK_MSG(blind.present, "an unreadable file is still a scripted run");
        CHECK_MSG(blind.args.empty() && Has(blind.error, "could not be read") && blind.info.empty(), "its flags are not taken, and the words say why: " + blind.error);
        CHECK_MSG(!fs::exists(scratch.dir / argsName) && Slurp(scratch.dir / usedName) == "--level level0", "and it is consumed");
    }
    {
        // Something that is not a regular file at the name is not a flags file.
        Folder scratch;
        fs::create_directories(scratch.dir / argsName);
        const Unpack::FlagsFile folder = Unpack::TakeFlagsFile(scratch.dir.string(), argsName, usedName);
        CHECK_MSG(!folder.present && folder.args.empty() && fs::is_directory(scratch.dir / argsName), "a folder of that name is not a flags file and is left alone");
    }
    {
        // The rename that cannot be done: the flags are still delivered, the file stays, and the words say that it will be read again.
        Folder scratch;
        std::ofstream(scratch.dir / argsName, std::ios::binary) << "--level level0";
        fs::create_directories(scratch.dir / usedName / "occupied");
        const Unpack::FlagsFile stuck = Unpack::TakeFlagsFile(scratch.dir.string(), argsName, usedName);
        // mutation: say nothing of a rename that failed; or drop the flags with it.
        CHECK_MSG(stuck.present && stuck.args == (std::vector<std::string>{"--level", "level0"}), "the flags are delivered");
        CHECK_MSG(Has(stuck.warning, "could not be renamed") && Has(stuck.warning, "read again"), "and the words say it will repeat: " + stuck.warning);
        CHECK_MSG(fs::exists(scratch.dir / argsName), "the file is still there");
    }
}

// D3. The decisions after the unpack and the argument list.
void TheEntrysDecisionsAreRulesThatCanBeRun() {
    // mutation (each): the other answer.
    CHECK_MSG(Unpack::ShowsFailureDialog(false), "a player's launch is told in a dialog");
    CHECK_MSG(!Unpack::ShowsFailureDialog(true), "a scripted one is not: nobody is at it to close the dialog");
    CHECK_MSG(Unpack::NextAfter(Unpack::Outcome::UpToDate) == Unpack::Next::Continue, "up to date: on to the game");
    CHECK_MSG(Unpack::NextAfter(Unpack::Outcome::Done) == Unpack::Next::Continue, "unpacked: on to the game");
    CHECK_MSG(Unpack::NextAfter(Unpack::Outcome::Cancelled) == Unpack::Next::ExitSuccess, "the app was closed: a quiet, successful end, not a failure");
    CHECK_MSG(Unpack::NextAfter(Unpack::Outcome::Failed) == Unpack::Next::ExitFailure, "failed: an error exit");

    Unpack::Result r;
    r.stamp = "abc";
    r.files = 900;
    r.copied = 31;
    r.copiedBytes = 4096;
    r.outcome = Unpack::Outcome::UpToDate;
    CHECK_MSG(Has(Unpack::Summary(r), "already unpacked") && Has(Unpack::Summary(r), "900 files") && Has(Unpack::Summary(r), "abc"), Unpack::Summary(r));
    r.outcome = Unpack::Outcome::Done;
    CHECK_MSG(Has(Unpack::Summary(r), "Unpacked 31 files") && Has(Unpack::Summary(r), "4096 bytes") && Has(Unpack::Summary(r), "abc"), Unpack::Summary(r));
    r.outcome = Unpack::Outcome::Cancelled;
    CHECK_MSG(Has(Unpack::Summary(r), "closed during the unpack after 31 files") && Has(Unpack::Summary(r), "next start unpacks again"), Unpack::Summary(r));
    r.outcome = Unpack::Outcome::Failed;
    r.error = "the words of the failure";
    CHECK_MSG(Unpack::Summary(r) == "the words of the failure", "a failure is its own words");

    using V = std::vector<std::string>;
    CHECK_MSG(Unpack::LaunchArguments("prog", "/files/GameData", {"--level", "level0", "a b"}) == (V{"prog", "--root", "/files/GameData", "--level", "level0", "a b"}),
              "the program, --root and the folder, then the flags, each as one argument");
    CHECK_MSG(Unpack::LaunchArguments("prog", "/r", {}) == (V{"prog", "--root", "/r"}), "no flags: just the root");
    CHECK_MSG(Unpack::LaunchArguments(nullptr, "/r", {}) == (V{Unpack::kDefaultProgramName, "--root", "/r"}), "no program name: the default");
    CHECK_MSG(Unpack::LaunchArguments("", "/r", {}) == (V{Unpack::kDefaultProgramName, "--root", "/r"}), "an empty one too");
    CHECK_MSG(std::string(Unpack::kDefaultProgramName) == "MagicPortals", "the default program name");
}

// PORT5-10 (a). The log of a start that stopped in the unpack.
void AStartThatStoppedInTheUnpackLeavesItsWordsInTheLogAndKeepsTheLastLog() {
    Folder scratch;
    const fs::path folder = scratch.dir / "Magic Portals" / "scratch";
    const std::string log = (folder / "magicportals.log").string();
    const std::string prev = (folder / "magicportals.prev.log").string();
    std::string why;
    // No folder yet, no earlier log: the folder is made and the words are written, with a final newline.
    CHECK_MSG(Unpack::RecordFailure(log, prev, "There is not enough free space.", why), why);
    // mutation: write nothing; do not make the folders; do not end the line.
    CHECK_MSG(Slurp(log) == "There is not enough free space.\n", "the words are in the file, one line");
    CHECK_MSG(!fs::exists(prev), "and there was no earlier log to keep");
    // An earlier log is kept beside it, the one before that goes.
    CHECK_MSG(Unpack::RecordFailure(log, prev, "second\n", why), why);
    CHECK_MSG(Slurp(log) == "second\n" && Slurp(prev) == "There is not enough free space.\n", "the last log is the previous one, and the new words are the log");
    // mutation: double the final newline when the text has one already.
    CHECK_MSG(Unpack::RecordFailure(log, prev, "third\n", why), why);
    CHECK_MSG(Slurp(log) == "third\n" && Slurp(prev) == "second\n", "the one before is replaced");
    // A log that cannot be written: said, not thrown. (A FILE stands where the log's folder should be.)
    Folder blocked;
    std::ofstream(blocked.dir / "notafolder", std::ios::binary) << "a file where the log's folder should be";
    std::string blockedWhy;
    CHECK_MSG(!Unpack::RecordFailure((blocked.dir / "notafolder" / "magicportals.log").string(), (blocked.dir / "magicportals.prev.log").string(), "x", blockedWhy), "a log that cannot be written is a false");
    CHECK_MSG(!blockedWhy.empty(), "with a reason");
}

void runTests() {
    AListOfSizesAndPathsReadsBack();
    BlankLinesAndCarriageReturnsAreToleratedAndNothingIsAComment();
    EveryWayAListCanBeWrongIsRefusedWithTheLine();
    EveryRefusalOfAPathStandsOnItsOwn();
    AFreshUnpackCopiesEveryFileAndWritesTheStampLast();
    TheSameStampWithEveryFileThereDoesNothing();
    AStampWithAFileMissingOrShortIsNotTheWordOfAWholeCopy();
    AnUnpackThatDiesAtFileKLeavesNoStampAndASecondCallCompletes();
    AStampLeftByAnEarlierCopyDoesNotSurviveAFailedRecopy();
    TheCopyIsCheckedAgainstTheList();
    TheSourceOrTheListCanBeBroken();
    AHostileListNeverTouchesTheDisk();
    ThePumpIsCalledEverySoOftenAndWhenItSaysFinishTheUnpackStops();
    FreeSpaceIsCheckedFromTheListInBlocksAndSaidInWords();
    AFullDiskMidFileIsAFailureThatNamesTheFile();
    TheSameInputsGiveTheSameOperationsEveryTime();
    TheFolderSinkWritesBytesAndSurvivesTheOwnersNeighbours();
    AFolderSinkThatLosesItsSourceLeavesNoStampAndNoHalfFile();
    TheFlagsFileIsWhitespaceSeparatedWithDoubleQuotes();
    ASumThatWouldWrapTheEstimateIsRefusedWhenTheListIsRead();
    TheListTextIsCappedWhereItsBytesArriveAndAgainByTheRun();
    AnAssetPastItsListedSizeIsRefusedAtTheChunkThatPassesIt();
    EveryFailingCallOfTheSinkEndsTheRunAndSaysWhich();
    SpaceIsCheckedBeforeAnythingIsClearedAndTheOldCopyCountsAsGivenBack();
    TheFreeSpaceSentenceNeverSaysThePhoneHasWhatItLacks();
    TheListThePackagerMakesIsTheListThisParses();
    TheFlagsFileIsReadOnceAndConsumedWhateverItHeld();
    TheEntrysDecisionsAreRulesThatCanBeRun();
    AStartThatStoppedInTheUnpackLeavesItsWordsInTheLogAndKeepsTheLastLog();
    // Enhancement E49 (planning step 260): the resume marker.
    ACleanStartWritesTheMarkerAfterTheWipeAndRemovesItAfterTheStamp();
    ASinkThatKeepsNoMarkerStartsAgainFromTheTopAfterAStop();
    AStartThatDiesAtEveryFileIsContinuedAndKeepsWhatWasDone();
    AStartTheAppFinishesAtEveryFileIsContinuedToo();
    AContinuationAsksThePumpAndTheProgressForKeptFilesToo();
    EachShortLifeAddsToTheLastAndEveryFileIsCopiedOnce();
    AStampThatCannotBeWrittenLeavesEveryFileAndTheMarkerForAOnePassFinish();
    AProcessKilledBeforeAnyCallOfAStartLeavesAStartTheNextOneFinishes();
    AProcessKilledTwiceIsStillContinuedAndFinishes();
    AMarkerOfAnotherStampIsACleanStartAndNothingOfThatBuildSurvives();
    AFileThatIsShortLongEmptyHalfOrMissingUnderTheRightMarkerIsCopiedAgain();
    TheFreeSpaceCheckServesAResumeAndARefusalLosesNothing();
    AMarkerBesideAFinishedCopyIsRemovedAndNeverFailsAStart();
    EveryFailingCallOfTheMarkerSaysWhichAndStopsWhereItShould();
    TheSummaryTellsWhatWasKeptAndWhatTheNextStartWillDo();
    TheFolderSinkKeepsItsMarkerBesideTheStampAndOutsideTheOwnedFolders();
    AnUnpackOnARealFolderHoldsTheMarkerWhileItIsUnfinishedAndAnotherProcessContinuesIt();
    AnotherBuildOverACutShortStartOnARealFolderIsACleanStart();
}

} // namespace

MP_TEST_MAIN("test_mp_unpack", 300)
