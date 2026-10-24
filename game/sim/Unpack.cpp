#include "sim/Unpack.hpp"

#include <algorithm>
#include <filesystem>
#include <limits>
#include <set>
#include <system_error>
#include <utility>

#include "sim/Files.hpp"
#include "sim/Scratch.hpp"

namespace MagicPortals::Unpack {

namespace {

bool IsDigit(char c) { return c >= '0' && c <= '9'; }

bool StampChar(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '.' || c == '_' || c == '-';
}

constexpr std::uint64_t kMegabyte = 1024 * 1024;

// a + b, or the largest 64-bit number where the sum would not fit.
std::uint64_t SaturatingAdd(std::uint64_t a, std::uint64_t b) { return a > std::numeric_limits<std::uint64_t>::max() - b ? std::numeric_limits<std::uint64_t>::max() : a + b; }

// What a side said, or that it said nothing: a sentence never ends in empty parentheses.
std::string Reason(const std::string& why) { return why.empty() ? std::string("no reason was given") : why; }

Result Failed(Result result, std::string error) {
    result.outcome = Outcome::Failed;
    result.error = std::move(error);
    return result;
}

} // namespace

bool SafePath(const std::string& path, const std::vector<std::string>& ownedRoots, std::string& why) {
    why.clear();
    if (path.empty()) {
        why = "the path is empty";
        return false;
    }
    for (const char c : path) {
        const unsigned char u = static_cast<unsigned char>(c);
        if (u < 0x20 || u == 0x7F) {
            why = u == 0 ? "the path holds a NUL" : "the path holds a control character";
            return false;
        }
        if (c == '\\') {
            why = "the path holds a backslash";
            return false;
        }
        if (c == ':') {
            why = "the path holds a colon (a drive letter, or a stream name on Windows)";
            return false;
        }
    }
    if (path.front() == '/') {
        why = "the path is absolute";
        return false;
    }
    if (path.back() == '/') {
        why = "the path names a folder, not a file";
        return false;
    }
    // Segment by segment. The loop ends on the last segment (no final '/', checked above), so a path with no separator at all is one segment.
    std::vector<std::string> segments;
    std::size_t at = 0;
    for (;;) {
        const std::size_t slash = path.find('/', at);
        const std::size_t end = slash == std::string::npos ? path.size() : slash;
        segments.push_back(path.substr(at, end - at));
        if (slash == std::string::npos) break;
        at = slash + 1;
    }
    for (const std::string& segment : segments) {
        if (segment.empty()) {
            why = "the path has an empty segment (a//b)";
            return false;
        }
        if (segment == ".") {
            why = "the path has a '.' segment";
            return false;
        }
        if (segment == "..") {
            why = "the path climbs out of its folder with '..'";
            return false;
        }
    }
    if (std::find(ownedRoots.begin(), ownedRoots.end(), segments.front()) == ownedRoots.end()) {
        why = "the path is not under a folder the unpack owns";
        return false;
    }
    if (segments.size() < 2) {
        why = "the path is an owned folder's own name, with nothing under it";
        return false;
    }
    return true;
}

std::uint64_t RoundUpToBlocks(std::uint64_t size) {
    // size / block, plus one when anything is left over: no addition of the size and a block, which would wrap for a size near 2^64.
    const std::uint64_t whole = size / kBlockBytes + (size % kBlockBytes != 0 ? 1 : 0);
    constexpr std::uint64_t kMostBlocks = std::numeric_limits<std::uint64_t>::max() / kBlockBytes;
    return whole > kMostBlocks ? kMostBlocks * kBlockBytes : whole * kBlockBytes;
}

bool AppendListChunk(std::string& text, const char* data, std::size_t size, std::string& why) {
    if (size > kMostListTextBytes - std::min(text.size(), kMostListTextBytes)) {
        why = "the list is longer than " + std::to_string(kMostListTextBytes) + " bytes";
        return false;
    }
    text.append(data, size);
    return true;
}

std::string MegabytesNeeded(std::uint64_t bytes) {
    // Rounded UP: the sentence never says the game needs less than it does. A need of nothing is "0 MB", which no caller prints (a need of nothing is no failure).
    return std::to_string(bytes / kMegabyte + (bytes % kMegabyte != 0 ? 1 : 0)) + " MB";
}

std::string MegabytesFree(std::uint64_t bytes) {
    // Rounded DOWN: the sentence never says the phone has more than it has, so "needs about 24 MB and 24 MB are free" cannot be printed.
    if (bytes < kMegabyte) return "less than 1 MB";
    return std::to_string(bytes / kMegabyte) + " MB";
}

Parsed ParseList(const std::string& text, const std::vector<std::string>& ownedRoots) {
    Parsed parsed;
    const auto fail = [&](std::size_t line, const std::string& what) {
        parsed.ok = false;
        parsed.list = List{};
        parsed.error = "line " + std::to_string(line) + ": " + what;
        return parsed;
    };

    std::set<std::string> seen;
    std::size_t at = 0;
    std::size_t lineNo = 0;
    bool haveStamp = false;
    std::uint64_t blocksTotal = 0; // the sizes rounded up to blocks, added: the estimate's own sum, kept from wrapping (it bounds the plain sum too)
    // `at` moves past every line, the last included, so the loop ends on text with no final newline (the walk of Step 202's H4 did not, and
    // exhausted a VM's memory).
    while (at <= text.size()) {
        const std::size_t newline = text.find('\n', at);
        const std::size_t end = newline == std::string::npos ? text.size() : newline;
        std::string line = text.substr(at, end - at);
        at = end + 1;
        ++lineNo;
        if (!line.empty() && line.back() == '\r') line.pop_back();

        if (!haveStamp) {
            // The first line is the stamp, whatever it is: a list that begins with a blank line has none.
            if (line.empty()) return fail(lineNo, "the first line must be the stamp, and it is empty");
            if (line.size() > kMostStampChars) return fail(lineNo, "the stamp is longer than " + std::to_string(kMostStampChars) + " characters");
            if (!std::all_of(line.begin(), line.end(), StampChar)) return fail(lineNo, "the stamp holds a character other than a letter, a digit, '.', '_' or '-'");
            parsed.list.stamp = line;
            haveStamp = true;
            continue;
        }
        if (line.empty()) continue;

        const std::size_t tab = line.find(kSizeSeparator);
        if (tab == std::string::npos) return fail(lineNo, "no TAB between the size and the path");
        if (tab == 0) return fail(lineNo, "no size before the TAB");
        if (tab > kMostSizeDigits) return fail(lineNo, "the size has more than " + std::to_string(kMostSizeDigits) + " digits");
        std::uint64_t size = 0;
        for (std::size_t i = 0; i < tab; ++i) {
            if (!IsDigit(line[i])) return fail(lineNo, "the size is not a number of decimal digits");
            size = size * 10 + static_cast<std::uint64_t>(line[i] - '0');
        }
        std::string path = line.substr(tab + 1);
        std::string why;
        if (!SafePath(path, ownedRoots, why)) return fail(lineNo, why + ": '" + path + "'");
        if (seen.count(path) != 0) return fail(lineNo, "the path is listed twice: '" + path + "'");
        // The sum must fit 64 bits ROUNDED UP TO BLOCKS, which is what NeededBytes adds (Unpack.hpp, ParseList, says why). That one check bounds the plain
        // sum too: a size of at most kMostSizeDigits digits is never above its rounded-up size, so `bytes` cannot wrap and has no check of its own.
        const std::uint64_t blocks = RoundUpToBlocks(size);
        if (blocksTotal > std::numeric_limits<std::uint64_t>::max() - blocks) return fail(lineNo, "the sizes, rounded up to allocation blocks, add up to more than 64 bits");
        blocksTotal += blocks;
        parsed.list.bytes += size;
        seen.insert(path);
        parsed.list.entries.push_back(Entry{std::move(path), size});
    }
    if (!haveStamp) return fail(1, "the list is empty");
    if (parsed.list.entries.empty()) return fail(lineNo, "the list names no file");
    parsed.ok = true;
    return parsed;
}

std::uint64_t NeededBytes(const List& list) {
    std::uint64_t total = 0;
    for (const Entry& entry : list.entries) total = SaturatingAdd(total, RoundUpToBlocks(entry.size));
    return total;
}

Result Run(Source& source, Sink& sink, const Options& options) {
    Result result;
    std::string text;
    std::string why;
    if (!source.ReadList(text, why)) {
        return Failed(result, "The game's files could not be found inside the app (" + Reason(why) + "). Installing the app again may fix this.");
    }
    // The cap holds for whatever a Source handed back (the Android one stops reading at it, AppendListChunk): a list that long is not this app's.
    if (text.size() > kMostListTextBytes) {
        return Failed(result, "The game's file list inside the app is longer than " + std::to_string(kMostListTextBytes) + " bytes and cannot be used. Installing the app again may fix this.");
    }
    const Parsed parsed = ParseList(text, options.ownedRoots);
    if (!parsed.ok) {
        return Failed(result, "The game's file list inside the app cannot be used (" + parsed.error + "). Installing the app again may fix this.");
    }
    const List& list = parsed.list;
    result.stamp = list.stamp;
    result.files = list.entries.size();
    result.bytes = list.bytes;
    result.needed = NeededBytes(list);

    // 1. Already done: the word of a finished copy, and the copy still whole.
    std::string stored;
    if (sink.ReadStamp(stored) && stored == list.stamp) {
        bool whole = true;
        for (const Entry& entry : list.entries) {
            std::uint64_t size = 0;
            if (!sink.SizeOf(entry.path, size) || size != entry.size) {
                whole = false;
                break;
            }
        }
        if (whole) {
            result.outcome = Outcome::UpToDate;
            return result;
        }
    }

    // 2. A copy is to be made. SPACE IS CHECKED FIRST, while the old copy is whole and its stamp stands: a disk that cannot hold the new copy fails the start
    //    with nothing removed (planning step 204, PORT5-10 (b); the check used to follow the removal, so an update on a nearly full phone cost the
    //    working copy and then said to free space). What the old copy holds is given back by its own removal, so it counts: the new copy needs its bytes
    //    less those. A disk that cannot say how much is free is not refused for it.
    for (const std::string& root : options.ownedRoots) {
        if (const std::optional<std::uint64_t> held = sink.HeldBytes(root)) result.reclaimable = SaturatingAdd(result.reclaimable, *held);
    }
    const std::uint64_t mustBeFree = result.needed > result.reclaimable ? result.needed - result.reclaimable : 0;
    if (const std::optional<std::uint64_t> free = sink.FreeBytes(); free && *free < mustBeFree) {
        return Failed(result, "There is not enough free space on this device to unpack the game: it needs about " + MegabytesNeeded(mustBeFree) +
                                  (result.reclaimable > 0 ? " more than the old copy of the game already takes," : "") + " and " + MegabytesFree(*free) +
                                  " are free. Free some space and open the game again.");
    }

    //    Then the stamp goes, so that a process killed from here on leaves none, and the folders this owns.
    if (!sink.RemoveStamp(why)) return Failed(result, "The game's old files could not be cleared (" + Reason(why) + ").");
    for (const std::string& root : options.ownedRoots) {
        if (!sink.RemoveTree(root, why)) return Failed(result, "The game's old files could not be cleared (" + Reason(why) + ").");
    }

    // 3. The copy, file by file.
    std::size_t done = 0;
    for (const Entry& entry : list.entries) {
        if (!sink.Begin(entry.path, why)) return Failed(result, "The game file " + entry.path + " could not be unpacked (" + Reason(why) + ").");
        std::uint64_t got = 0;
        std::string writeWhy;
        bool writeFailed = false;
        bool tooLong = false;
        std::string readWhy;
        const bool read = source.ReadAsset(
            entry.path,
            [&](const char* data, std::size_t size) {
                // The chunk that would take the file past its listed size is refused UNWRITTEN and the source is told to stop: a damaged asset of any length
                // is not read to its end, and the disk is never given a byte more than the list allows (got never passes entry.size, so the subtraction is safe).
                if (size > entry.size - got) {
                    tooLong = true;
                    return false;
                }
                if (!sink.Write(data, size, writeWhy)) {
                    writeFailed = true;
                    return false;
                }
                got += size;
                return true;
            },
            readWhy);
        std::string endWhy;
        if (tooLong) {
            sink.End(false, endWhy);
            return Failed(result, "The game file " + entry.path + " is damaged inside the app: it is longer than the " + std::to_string(entry.size) +
                                      " bytes the list says. Installing the app again may fix this.");
        }
        if (!read || writeFailed) {
            sink.End(false, endWhy);
            return Failed(result, "The game file " + entry.path + " could not be unpacked (" + Reason(writeFailed ? writeWhy : readWhy) + ").");
        }
        if (got != entry.size) {
            sink.End(false, endWhy);
            return Failed(result, "The game file " + entry.path + " is damaged inside the app: it is " + std::to_string(got) + " bytes and the list says " +
                                      std::to_string(entry.size) + ". Installing the app again may fix this.");
        }
        if (!sink.End(true, endWhy)) return Failed(result, "The game file " + entry.path + " could not be unpacked (" + Reason(endWhy) + ").");
        ++result.copied;
        result.copiedBytes += got;
        ++done;
        if (options.pumpEvery > 0 && done % options.pumpEvery == 0) {
            if (options.progress) options.progress(done, list.entries.size(), result.copiedBytes);
            if (options.pump) {
                ++result.pumps;
                if (!options.pump()) {
                    result.outcome = Outcome::Cancelled;
                    return result;
                }
            }
        }
    }

    // 4. The stamp, LAST.
    if (!sink.WriteStamp(list.stamp, why)) return Failed(result, "The game's files were unpacked but the unpack could not be recorded (" + Reason(why) + ").");
    if (options.progress) options.progress(done, list.entries.size(), result.copiedBytes);
    result.outcome = Outcome::Done;
    return result;
}

// ---- FsSink --------------------------------------------------------------------------------------------------------------------------

FsSink::FsSink(std::string root, std::string stampName) : root_(std::move(root)), stampName_(std::move(stampName)) {}

FsSink::~FsSink() {
    // A sink dropped with a file open (an exception through the caller) leaves no half file behind.
    if (out_.is_open()) {
        out_.close();
        std::error_code ec;
        std::filesystem::remove(std::filesystem::path(root_) / openPath_, ec);
    }
}

bool FsSink::ReadStamp(std::string& stamp) {
    std::string text;
    if (Files::ReadWhole((std::filesystem::path(root_) / stampName_).string(), text) != Files::ReadResult::Read) return false;
    stamp = std::move(text);
    return true;
}

bool FsSink::WriteStamp(const std::string& stamp, std::string& why) {
    return Files::WriteAtomic((std::filesystem::path(root_) / stampName_).string(), stamp, Files::WriteOptions{}, why);
}

bool FsSink::RemoveStamp(std::string& why) {
    std::error_code ec;
    std::filesystem::remove(std::filesystem::path(root_) / stampName_, ec);
    if (ec) why = ec.message();
    return !ec;
}

bool FsSink::RemoveTree(const std::string& root, std::string& why) {
    std::error_code ec;
    std::filesystem::remove_all(std::filesystem::path(root_) / root, ec);
    if (ec) why = root + ": " + ec.message();
    return !ec;
}

bool FsSink::SizeOf(const std::string& path, std::uint64_t& size) {
    std::error_code ec;
    const std::filesystem::path file = std::filesystem::path(root_) / path;
    if (!std::filesystem::is_regular_file(file, ec) || ec) return false;
    const std::uintmax_t bytes = std::filesystem::file_size(file, ec);
    if (ec) return false;
    size = static_cast<std::uint64_t>(bytes);
    return true;
}

std::optional<std::uint64_t> FsSink::FreeBytes() {
    std::error_code ec;
    std::filesystem::create_directories(root_, ec);
    const std::filesystem::space_info info = std::filesystem::space(root_, ec);
    if (ec) return std::nullopt;
    return static_cast<std::uint64_t>(info.available);
}

std::optional<std::uint64_t> FsSink::HeldBytes(const std::string& root) {
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path dir = fs::path(root_) / root;
    if (!fs::exists(dir, ec)) {
        if (ec) return std::nullopt;
        return std::uint64_t{0}; // a folder that is not there holds nothing
    }
    std::uint64_t total = 0;
    for (fs::recursive_directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
        // Links are not followed and not counted: removing the owned folder removes the link, never what it points at.
        std::error_code entryError;
        if (it->is_symlink(entryError) || entryError) continue;
        if (!it->is_regular_file(entryError) || entryError) continue;
        const std::uintmax_t bytes = it->file_size(entryError);
        if (entryError) return std::nullopt;
        total = SaturatingAdd(total, RoundUpToBlocks(static_cast<std::uint64_t>(bytes)));
    }
    if (ec) return std::nullopt;
    return total;
}

bool FsSink::Begin(const std::string& path, std::string& why) {
    if (out_.is_open()) {
        // A Begin with another file still open is the caller's mistake; it is not carried on over a leaked handle.
        out_.close();
    }
    const std::filesystem::path file = std::filesystem::path(root_) / path;
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    if (ec) {
        why = "cannot make " + file.parent_path().string() + ": " + ec.message();
        return false;
    }
    out_.clear();
    out_.open(file, std::ios::binary | std::ios::trunc);
    if (!out_.is_open()) {
        why = "cannot open " + file.string() + " for writing";
        return false;
    }
    openPath_ = path;
    return true;
}

bool FsSink::Write(const char* data, std::size_t size, std::string& why) {
    out_.write(data, static_cast<std::streamsize>(size));
    if (!out_) {
        why = "cannot write to " + openPath_ + " (the disk may be full)";
        return false;
    }
    return true;
}

bool FsSink::End(bool keep, std::string& why) {
    bool good = out_.is_open();
    if (good) {
        out_.flush();
        good = static_cast<bool>(out_);
        out_.close();
        good = good && !out_.fail();
    }
    if (!keep || !good) {
        std::error_code ec;
        std::filesystem::remove(std::filesystem::path(root_) / openPath_, ec);
        if (keep && !good) why = "cannot finish writing " + openPath_ + " (the disk may be full)";
        return !keep;
    }
    return true;
}

// ---- The developer's flags file --------------------------------------------------------------------------------------------------------

ArgsParsed ParseArgsText(const std::string& text) {
    ArgsParsed parsed;
    std::string current;
    bool quoted = false;
    bool any = false; // a token has begun (a "" is a token with nothing in it)
    for (const char c : text) {
        if (c == '\0') {
            parsed.error = "the file holds a NUL";
            parsed.args.clear();
            return parsed;
        }
        if (c == '"') {
            quoted = !quoted;
            any = true;
        } else if (!quoted && (c == ' ' || c == '\t' || c == '\n' || c == '\r')) {
            if (any) parsed.args.push_back(current);
            current.clear();
            any = false;
        } else {
            current += c;
            any = true;
        }
    }
    if (quoted) {
        parsed.error = "a double quote is opened and never closed";
        parsed.args.clear();
        return parsed;
    }
    if (any) parsed.args.push_back(current);
    parsed.ok = true;
    return parsed;
}

// ---- What the Android entry decides ----------------------------------------------------------------------------------------------------

FlagsFile TakeFlagsFile(const std::string& folder, const std::string& name, const std::string& usedName, const ReadFileFn& read) {
    FlagsFile flags;
    try {
        namespace fs = std::filesystem;
        std::error_code ec;
        const fs::path file = fs::path(folder) / name;
        if (!fs::is_regular_file(file, ec) || ec) return flags;
        flags.present = true;
        std::string text;
        const bool readable = read ? read(file.string(), text) : Files::ReadWhole(file.string(), text) == Files::ReadResult::Read;
        if (!readable) {
            flags.error = "Flags file " + file.string() + " ignored: it could not be read";
        } else {
            const ArgsParsed parsed = ParseArgsText(text);
            if (parsed.ok) {
                flags.args = parsed.args;
                flags.info = "Flags from " + file.string() + ":";
                for (const std::string& arg : flags.args) flags.info += " " + arg;
            } else {
                flags.error = "Flags file " + file.string() + " ignored: " + parsed.error;
            }
        }
        // Consumed whatever it held, so that a broken file does not repeat either. The rename replaces an earlier consumed file.
        const fs::path used = fs::path(folder) / usedName;
        fs::rename(file, used, ec);
        if (ec) {
            // Not consumed: the next launch would run them again. Said, because a scripted run that repeats is a puzzle otherwise.
            flags.warning = "The flags file could not be renamed to " + used.string() + " (" + ec.message() + "); it will be read again.";
        }
    } catch (const std::exception& e) {
        flags.error = std::string("Flags file ignored: ") + e.what();
    }
    return flags;
}

bool ShowsFailureDialog(bool scripted) { return !scripted; }

Next NextAfter(Outcome outcome) {
    switch (outcome) {
    case Outcome::UpToDate:
    case Outcome::Done:
        return Next::Continue;
    case Outcome::Cancelled:
        return Next::ExitSuccess; // the app was asked to finish: not a failure
    case Outcome::Failed:
        break;
    }
    return Next::ExitFailure;
}

std::string Summary(const Result& result) {
    switch (result.outcome) {
    case Outcome::UpToDate:
        return "The game's files are already unpacked (" + std::to_string(result.files) + " files, stamp " + result.stamp + ").";
    case Outcome::Done:
        return "Unpacked " + std::to_string(result.copied) + " files, " + std::to_string(result.copiedBytes) + " bytes (stamp " + result.stamp + ").";
    case Outcome::Cancelled:
        return "The app was closed during the unpack after " + std::to_string(result.copied) + " files; the next start unpacks again.";
    case Outcome::Failed:
        break;
    }
    return result.error;
}

std::vector<std::string> LaunchArguments(const char* argv0, const std::string& root, const std::vector<std::string>& flags) {
    std::vector<std::string> args;
    args.emplace_back(argv0 != nullptr && argv0[0] != '\0' ? argv0 : kDefaultProgramName);
    args.emplace_back("--root");
    args.push_back(root);
    args.insert(args.end(), flags.begin(), flags.end());
    return args;
}

bool RecordFailure(const std::string& logPath, const std::string& previousPath, const std::string& text, std::string& why) {
    try {
        const std::filesystem::path log(logPath);
        std::string keepWhy;
        Scratch::KeepPreviousLog(log, std::filesystem::path(previousPath), keepWhy); // a log that cannot be kept never stops the record, as in main
        std::string body = text;
        if (body.empty() || body.back() != '\n') body += '\n';
        return Files::WriteAtomic(logPath, body, Files::WriteOptions{}, why);
    } catch (const std::exception& e) {
        why = e.what();
        return false;
    }
}

} // namespace MagicPortals::Unpack
