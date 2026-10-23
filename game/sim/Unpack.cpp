#include "sim/Unpack.hpp"

#include <algorithm>
#include <filesystem>
#include <limits>
#include <set>
#include <system_error>
#include <utility>

#include "sim/Files.hpp"

namespace MagicPortals::Unpack {

namespace {

bool IsDigit(char c) { return c >= '0' && c <= '9'; }

bool StampChar(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '.' || c == '_' || c == '-';
}

// Whole megabytes, rounded UP, for a sentence: "about 25 MB". Never says 0 for something.
std::string Megabytes(std::uint64_t bytes) {
    constexpr std::uint64_t kMb = 1024 * 1024;
    return std::to_string((bytes + kMb - 1) / kMb) + " MB";
}

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
        if (parsed.list.bytes > std::numeric_limits<std::uint64_t>::max() - size) return fail(lineNo, "the sizes add up to more than 64 bits");
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
    for (const Entry& entry : list.entries) total += (entry.size + kBlockBytes - 1) / kBlockBytes * kBlockBytes;
    return total;
}

Result Run(Source& source, Sink& sink, const Options& options) {
    Result result;
    std::string text;
    std::string why;
    if (!source.ReadList(text, why)) {
        return Failed(result, "The game's files could not be found inside the app (" + Reason(why) + "). Installing the app again may fix this.");
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

    // 2. A copy is to be made. The stamp goes FIRST, so that a process killed from here on leaves none, then the folders this owns.
    if (!sink.RemoveStamp(why)) return Failed(result, "The game's old files could not be cleared (" + Reason(why) + ").");
    for (const std::string& root : options.ownedRoots) {
        if (!sink.RemoveTree(root, why)) return Failed(result, "The game's old files could not be cleared (" + Reason(why) + ").");
    }
    if (const std::optional<std::uint64_t> free = sink.FreeBytes(); free && *free < result.needed) {
        return Failed(result, "There is not enough free space on this device to unpack the game: it needs about " + Megabytes(result.needed) + " and " +
                                  Megabytes(*free) + " are free. Free some space and open the game again.");
    }

    // 3. The copy, file by file.
    std::size_t done = 0;
    for (const Entry& entry : list.entries) {
        if (!sink.Begin(entry.path, why)) return Failed(result, "The game file " + entry.path + " could not be unpacked (" + Reason(why) + ").");
        std::uint64_t got = 0;
        std::string writeWhy;
        bool writeFailed = false;
        std::string readWhy;
        const bool read = source.ReadAsset(
            entry.path,
            [&](const char* data, std::size_t size) {
                if (!sink.Write(data, size, writeWhy)) {
                    writeFailed = true;
                    return false;
                }
                got += size;
                return true;
            },
            readWhy);
        std::string endWhy;
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

} // namespace MagicPortals::Unpack
