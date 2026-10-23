#include "sim/Files.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <exception>
#include <fstream>
#include <random>
#include <sstream>
#include <system_error>
#include <thread>

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

namespace MagicPortals::Files {

// A name no other writer - another window, an earlier run - and no earlier call has used for its half-written copy.
std::string UniqueSuffix() {
    static std::atomic<unsigned> counter{0};
    unsigned long long random = 0;
    try {
        std::random_device device;
        random = device();
    } catch (const std::exception&) {
        // A machine with no entropy source: the clock stands in, and the counter still tells two calls apart.
        random = static_cast<unsigned long long>(std::chrono::steady_clock::now().time_since_epoch().count());
    }
    return std::to_string(random) + "-" + std::to_string(counter.fetch_add(1));
}

namespace {

// Opened by its WIDE name on Windows, so that a path the narrow functions would mangle still opens.
std::FILE* OpenForWrite(const std::filesystem::path& file) {
#if defined(_WIN32)
    std::FILE* handle = nullptr;
    if (_wfopen_s(&handle, file.c_str(), L"wb") != 0) return nullptr;
    return handle;
#else
    return std::fopen(file.c_str(), "wb");
#endif
}

// What the stream's flush leaves in the system's cache, written through to the disk. A platform or a file system that
// cannot do it (a network share, a drive that refuses) is not a failed save: the bytes are written and the rename is
// still one step, only a power cut right after it could still find the old file.
void WriteThrough(std::FILE* file) {
#if defined(_WIN32)
    (void)_commit(_fileno(file));
#else
    (void)fsync(fileno(file));
#endif
}

void KeepBackup(const std::filesystem::path& live, const std::string& target, std::string* warning) {
    std::error_code ec;
    if (!std::filesystem::is_regular_file(live, ec) || ec) return; // nothing to keep: the first save of a file
    const std::string backup = BackupPath(target);
    const std::string stage = backup + "." + UniqueSuffix() + ".tmp";
    std::filesystem::copy_file(live, stage, std::filesystem::copy_options::overwrite_existing, ec);
    if (!ec) std::filesystem::rename(stage, backup, ec);
    if (ec) {
        std::error_code ignored;
        std::filesystem::remove(stage, ignored);
        if (warning != nullptr) *warning = backup + ": the last good copy was not kept: " + ec.message();
    }
}

} // namespace

std::string BackupPath(const std::string& path) { return path + kBackupSuffix; }

bool WriteAtomic(const std::string& target, const std::string& bytes, const WriteOptions& options, std::string& error,
                 std::string* warning) {
    error.clear();
    if (warning != nullptr) warning->clear();

    const std::filesystem::path path(target);
    std::error_code ec;
    // A directory handed in by a test or a flag need not exist yet.
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path(), ec);

    const std::string tempText = target + "." + UniqueSuffix() + ".tmp";
    const std::filesystem::path temp(tempText);
    std::FILE* file = OpenForWrite(temp);
    if (file == nullptr) {
        error = tempText + ": cannot open for writing";
        return false;
    }
    const bool written = bytes.empty() || std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
    const bool flushed = written && std::fflush(file) == 0;
    if (flushed) WriteThrough(file);
    // AFTER the close, and always closed: a failure of the last buffered bytes is first seen here, which is where the old
    // writers never looked.
    const bool closed = std::fclose(file) == 0;
    if (!written || !flushed || !closed) {
        std::filesystem::remove(temp, ec);
        error = tempText + ": write failed";
        return false;
    }

    if (options.keepBackup) KeepBackup(path, target, warning);

    // The replace is the rename, one step. Where it is refused the live file is left exactly as it was - it is NOT removed to
    // make room, which on a failing second rename was the only copy of the player's progress - and the caller is told, so the
    // next save tries again with the bytes it still holds.
    ec.clear();
    std::filesystem::rename(temp, path, ec);
    if (ec) {
        error = target + ": " + ec.message();
        std::error_code ignored;
        std::filesystem::remove(temp, ignored);
        return false;
    }
    return true;
}

ReadResult ReadWhole(const std::string& path, std::string& text, int attempts, int waitMs, std::string* why) {
    text.clear();
    if (attempts < 1) attempts = 1;
    std::string reason;
    for (int attempt = 1;; ++attempt) {
        std::error_code ec;
        const bool exists = std::filesystem::exists(path, ec);
        if (!ec && !exists) return ReadResult::Missing;
        if (ec) {
            // Not "no such file": the answer itself failed (a denied folder, a share that dropped). A file that cannot be
            // told from a missing one is not treated as missing, for a save written over it would be a guess.
            reason = path + ": " + ec.message();
        } else {
            // A directory opens as an empty stream on Linux and macOS (ifstream's open succeeds, the read finds nothing), where
            // Windows refuses it: it is not a document on either, so both answer Unreadable with the same reason. Only a
            // directory is asked about, so what Windows opens today it opens still.
            std::error_code askedAbout; // an answer that fails is "not a directory": the open below then says what it always said
            const bool isDirectory = std::filesystem::is_directory(path, askedAbout);
            std::ifstream file;
            if (!isDirectory) file.open(path, std::ios::binary);
            if (file.is_open() && file) { // is_open: a stream that was never opened is "good", and a directory is not opened
                std::ostringstream buffer;
                buffer << file.rdbuf();
                text = buffer.str();
                return ReadResult::Read;
            }
            reason = path + ": cannot open";
        }
        if (attempt >= attempts) break;
        if (waitMs > 0) std::this_thread::sleep_for(std::chrono::milliseconds(waitMs));
    }
    if (why != nullptr) *why = reason;
    return ReadResult::Unreadable;
}

bool MoveAside(const std::string& path, std::string& movedTo, std::string& error) {
    movedTo.clear();
    error.clear();
    constexpr int kMostCopies = 1000;
    for (int n = 1; n <= kMostCopies; ++n) {
        const std::string candidate = path + kQuarantineSuffix + std::to_string(n);
        std::error_code ec;
        const bool taken = std::filesystem::exists(candidate, ec);
        if (ec) {
            error = candidate + ": " + ec.message();
            return false;
        }
        if (taken) continue;
        std::filesystem::rename(path, candidate, ec);
        if (ec) {
            error = path + ": " + ec.message();
            return false;
        }
        movedTo = candidate;
        return true;
    }
    error = path + ": " + std::to_string(kMostCopies) + " copies of it are kept already";
    return false;
}

std::filesystem::path UsableDirectory(const std::vector<std::filesystem::path>& candidates) {
    for (const std::filesystem::path& candidate : candidates) {
        if (candidate.empty()) continue;
        std::error_code ec;
        if (std::filesystem::is_directory(candidate, ec) && !ec) return candidate;
        ec.clear();
        std::filesystem::create_directories(candidate, ec);
        if (ec) continue;
        if (std::filesystem::is_directory(candidate, ec) && !ec) return candidate;
    }
    return {};
}

} // namespace MagicPortals::Files
