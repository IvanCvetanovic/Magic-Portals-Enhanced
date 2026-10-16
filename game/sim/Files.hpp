#pragma once

// The files a player's save lives in, written and read so that a bad moment costs the player nothing.
//
// WHY THIS EXISTS (planning step 189). Four stores keep what a player has earned or chosen - scores.json, audio.json,
// achievements.json and controls.json - and three of them each carried their own copy of a write that tested the stream
// BEFORE it was flushed or closed (for a file this small nothing has reached the disk until then, so a full disk or a
// scanner holding the file surfaced at the destructor, where nobody looked, and an empty temporary was renamed over the
// good file), named its temporary after the file (two windows share it), left that temporary behind on a failure, and
// answered a refused rename by REMOVING the live file and renaming again. controls.json had been given the right shape
// (planning step 187); this is that shape, once, for all four.
//
// Free of the engine and of the renderer, like the rest of sim/, so a test needs only a scratch directory.

#include <filesystem>
#include <string>
#include <vector>

namespace MagicPortals::Files {

// The one earlier copy kept beside a file that was replaced: "<file>.bak".
inline constexpr const char* kBackupSuffix = ".bak";
// What an unreadable file is renamed to, with the first free number after it: "<file>.bad-1".
inline constexpr const char* kQuarantineSuffix = ".bad-";

struct WriteOptions {
    // Keep what is being replaced as "<file>.bak" first. Only for a caller that KNOWS the live file is a good one: a file
    // that failed to read must never replace the last good copy.
    bool keepBackup = false;
};

// Replaces `path` with `bytes` in one step, or changes nothing.
//
//   - the bytes go to a temporary of this call's own name beside the file ("<file>.<random>-<n>.tmp"), so another window,
//     an earlier run and a leftover can never be the file this writes;
//   - the stream is flushed, written through to the disk where the platform allows, and checked AFTER it is closed, which
//     is the point where a short write is first seen;
//   - the temporary is renamed over the file, which replaces it in one step: the live file is whole, old or new, at every
//     instant, and it is NEVER removed to make room;
//   - the temporary is removed on every failure path, and a failure says so in `error`, naming the file.
//
// The backup is a courtesy: it is copied through a temporary of its own and a failure to make it never fails the write
// (`warning`, when given, says so).
bool WriteAtomic(const std::string& path, const std::string& bytes, const WriteOptions& options, std::string& error,
                 std::string* warning = nullptr);

// "<path>.bak".
std::string BackupPath(const std::string& path);

enum class ReadResult {
    Read,       // `text` holds the file (it may be empty)
    Missing,    // there is no such file
    Unreadable, // there is a file, or something at its name, that could not be opened or looked at, after every attempt
};

// The whole of a file. `attempts` tries (at least one), `waitMs` apart, for a file that cannot be opened: another program
// holds it for a moment (a scanner, a sync client, a second window), and a lock is not a corrupt file. A missing file is
// answered at once. `why`, when given, says what stopped an Unreadable.
ReadResult ReadWhole(const std::string& path, std::string& text, int attempts = 1, int waitMs = 0,
                     std::string* why = nullptr);

// Renames `path` to "<path>.bad-<n>" for the first n from 1 that is free, so an unreadable file is kept for a human and
// no copy of it is ever replaced. False, with `error`, when it cannot be moved; the file is then where it was. (Two
// processes quarantining in the same instant could pick the same n; the rename would then replace the earlier copy of one
// of the two bad files. Not guarded: the live file is never at risk, and a second window starting in the same millisecond
// over the same unreadable file is not a case worth a lock.)
bool MoveAside(const std::string& path, std::string& movedTo, std::string& error);

// The first of `candidates` that is a directory, or can be made one, skipping empty entries; empty when none is. Never
// throws, which the throwing overloads of std::filesystem do: temp_directory_path() throws when %TEMP% names nothing, and
// a game that dies there dies before it has a log to say why. A caller asks for the system's with the error_code overload
// (an empty path on failure) and lists the places to fall back to after it.
std::filesystem::path UsableDirectory(const std::vector<std::filesystem::path>& candidates);

} // namespace MagicPortals::Files
