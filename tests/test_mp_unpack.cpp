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
#include <iterator>
#include <map>
#include <optional>
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
    // A stamp left by the earlier, good copy is gone after a re-copy that failed.
    {
        MemSource good = SourceFor("real-2", files);
        Unpack::FsSink sink(scratch.dir.string(), stampName);
        CHECK(Unpack::Run(good, sink, Plain()).outcome == Unpack::Outcome::Done);
        CHECK(fs::exists(scratch.dir / stampName));
        MemSource next = SourceFor("real-3", files);
        next.failAt = files[2].path;
        CHECK(Unpack::Run(next, sink, Plain()).outcome == Unpack::Outcome::Failed);
        CHECK_MSG(!fs::exists(scratch.dir / stampName), "the old stamp does not outlive a failed re-copy");
    }
    // The sink alone: an empty stamp file reads as no match, a free-space figure is a figure, a missing file has no size.
    Unpack::FsSink sink(scratch.dir.string(), stampName);
    std::uint64_t size = 99;
    CHECK_MSG(!sink.SizeOf("GameData/does/not/exist", size), "a file that is not there has no size");
    const std::optional<std::uint64_t> free = sink.FreeBytes();
    CHECK_MSG(free.has_value() && *free > 0, "the scratch disk says how much is free");
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
}

} // namespace

MP_TEST_MAIN("test_mp_unpack", 300)
