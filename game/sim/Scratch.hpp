#pragma once

// WHERE THE GAME MAY WRITE ITS LOG AND ITS GENERATED FILES (planning step 200, PORT-2; the platform research's PL-01 and PL-15).
//
// WHAT GOES THERE. The log (magicportals.log and the previous run's), the collision prisms (the polygon colliders' OBJ files, which the
// engine's hull cache reads from disk: no writable folder means no level plays), the pad glyphs the layer generates and the multiplies
// it draws AM_MODULATE with. None of it is the player's: it can all be made again, and the prisms are named by a hash of their contents,
// so a folder that outlives a run is fine.
//
// WHY IT IS A RULE AND NOT A CALL. The port used the system's temporary folder and fell back only if asking for it ERRORED. Two places
// where that is not enough:
//   - Android (PL-01): the NDK's libc++ names /data/local/tmp as the temporary folder when TMPDIR is unset. The call succeeds, the
//     folder exists, and an app cannot write there: every level was "not playable yet" and the log did not even open, so the report
//     that would have said so had nowhere to go.
//   - a shared /tmp (PL-15): /tmp/supersonic-magicportals made by one account is unwritable by the next account to start the game.
// The fix is to ask the folder, not the call: a candidate counts only if a file can really be made in it, and the next candidate is
// tried if not. PURE like AssetRoot: the candidates and the probe are handed in, so the order is tested with a probe that fails on
// demand and a candidate that cannot exist, with no machine in the way.
//
// THE CANDIDATES, in main's order: (a) the system's temporary folder, which is what a desktop has always used, so a Windows run writes
// where it always did; (b) the user's data folder's scratch folder, which is the answer wherever (a) is unwritable and is the only one
// that MAKES a folder (the engine's UserDataDirectory creates the player's data folder), so it is asked for only when (a) failed;
// (c) the older last resorts (the user's data folder itself, the executable's folder, the working directory).

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace MagicPortals::Scratch {

// The folder inside a candidate that holds the prisms and the generated pictures. The log sits in the candidate itself (README: the
// log is %TEMP%\magicportals.log), so a run on a desktop writes exactly the names it always did.
inline constexpr const char* kPrismFolder = "supersonic-magicportals";

struct Candidate {
    std::string label; // for the log: "the system's temporary folder"
    // Evaluated only when the candidates before it all failed. An empty path is "not available here". May make a folder (the user's
    // data folder is made by asking for it); `makes` says so, and the note then says it.
    std::function<std::filesystem::path()> base;
    bool makes = false;
};

// Whether a file can be made in `dir` (made, with its parents, if it is not there). `why` says what stopped it.
using Probe = std::function<bool(const std::filesystem::path& dir, std::string& why)>;

struct Chosen {
    bool ok = false;
    std::filesystem::path base;   // the log goes here
    std::filesystem::path prisms; // base / kPrismFolder: the prisms, the generated glyphs, the multiplies
    std::string label;
    std::size_t index = 0;        // which candidate answered
    std::string note;             // one line for the log: which, and why each earlier one was passed over
    std::vector<std::string> passedOver; // "<label>: <why>", in order
};

// The first candidate whose base AND whose prism folder pass the probe. Never throws.
Chosen Choose(const std::vector<Candidate>& candidates, const Probe& probe);

// The probe the game uses: makes `dir` if it is missing, writes a small file of a name no other process uses into it, and removes the
// file, so nothing is left behind but the folder. Never throws.
bool ProbeWritable(const std::filesystem::path& dir, std::string& why);

} // namespace MagicPortals::Scratch
