#pragma once

// WHERE THE GAME FINDS ITS DATA AT RUN TIME (planning step 200, PORT-2).
//
// WHY THIS EXISTS. Until this step the port found its data through six absolute paths that the build baked in from the machine that built
// it (game/sim/CMakeLists.txt, game/CMakeLists.txt), and three of them (the original's assets, the chapters table, the achievements
// table) had no command-line flag at all. A game moved off the machine that built it, an Android app whose files were unpacked into its
// private folder, an iOS bundle, a release package: none of them could say where its data is. This file is the rule that can, and it is
// PURE like the engine's own ChooseAssetRoot: the one question it asks of the filesystem ("does this folder hold the game?") is handed
// in, so the order is tested without a disk and a test cannot create a folder by asking it.
//
// THE ORDER. The first folder that holds the game wins (it has the levels and the chapters table):
//   1. --root <absolute folder>. STRICT, like --padart: a folder that does not hold the game is an error that names it, never a
//      fall-through to a later tier (a capture that meant one tree and ran another says nothing about either).
//   2. a folder next to the executable: <exe folder>/content, then the executable's folder itself (a released game).
//   3. the application bundle's folder (a mobile or macOS shim passes it; nothing on a desktop does).
//   4. the user's data folder: <the platform's data folder>/Magic Portals/content, which main asks the ENGINE for
//      (Supersonic::UserDataDirectory) and only when tiers 1 to 3 did not answer. Only READ and tested for existence here: nothing in
//      this file or around it creates a folder, and the player's save folder holds the saves and nothing a resolver put there.
//   5. the paths this build was configured with (MAGICPORTALS_* in CMake): today's behaviour, kept for a developer's build. It is
//      accepted without asking whether it holds the game, exactly as the port has always started with missing data (boxes, and a
//      suite that skips and says so). A build configured with MAGICPORTALS_BAKED_PATHS=OFF has nothing here, and a run that reaches this
//      tier then fails with a message instead of opening a stray relative path.
// There is NO environment-variable lookup (an audit greps game/ for one); a mobile shim passes the root as a parameter.
//
// TWO SHAPES OF A ROOT. A tier's folder may be laid out like this repository (Layout::Repo: out/levels, out/data/chapters.json, data,
// game/data, reference/extracted/assets, reference/magic-rampage/controls) or like the released folder the content packager writes
// (Layout::Packaged, documented in docs/packaging.md). Packaged is tried first, then Repo, and the shape that matched is reported.
//
// NO ".." IN WHAT IS HANDED BACK. The baked paths spelled their parents lexically (LEVELS "/.."), and a POSIX file system resolves ".."
// physically, so a symlinked levels folder would have changed the answer. Every string Resolve returns is spelled out.
//
// HOST-INDEPENDENT BY CONSTRUCTION: strings only. Input may use either separator, output uses '/', and the suite runs the same on
// every platform (a lexically_normal() or is_absolute() here would give Windows and Linux different answers, as test_mp_tiers once
// found).
//
// The suites never call Resolve: MagicPortalsLayer::Paths keeps its baked defaults, so every suite reads what it always read. Only
// main (and later the platform shims) builds a Paths from a Resolved.

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace MagicPortals::AssetRoot {

// Where each thing the game reads sits under a root: relative, '/'-separated, "." for the root itself.
struct Layout {
    std::string name; // "packaged" or "repo", for the log
    std::string levels;       // the converted levels (*.tscn)
    std::string art;          // what res:// stands for: the folder that holds assets/
    std::string chapters;     // chapters.json
    std::string achievements; // achievements.json
    std::string data;         // Enhanced's tables (data/ in the repository)
    std::string portData;     // the port's own tables (game/data/ in the repository)
    std::string original;     // the original's extracted assets
    std::string padArt;       // Magic Rampage's controls folder; its editor folder is beside it
    // This repository's shape, relative to its root.
    static Layout Repo();
    // The released folder's shape (docs/packaging.md).
    static Layout Packaged();
};

// What the game reads, as explicit strings. Empty means "none given": the baked defaults of a build with MAGICPORTALS_BAKED_PATHS=OFF
// are all empty, and so is a path a tier does not have.
struct Paths {
    std::string levels;
    std::string art;
    std::string chapters;
    std::string achievements;
    std::string data;
    std::string portData;
    std::string original;
    std::string padArt;
    bool Empty() const;
};

// Which tier answered.
enum class Source { Flag, BesideExecutable, Bundle, UserData, CompiledIn, None };
// The words the log and the messages use for a tier: "--root", "beside the executable", "the application bundle", "the user's data
// folder", "compiled in", "none".
const char* SourceName(Source source);

struct Inputs {
    std::string flagRoot;      // --root, as given ("" when not given)
    std::string exeDir;        // the executable's folder ("" when the platform will not say)
    std::string bundleContent; // a mobile/macOS shim's content folder ("" on a desktop)
    // <the user's data folder>/content, or empty where the platform names none; unset is the same as empty. A function, because the
    // engine's UserDataDirectory MAKES the player's data folder when it answers, so it is asked only when it is needed: after a
    // --root, a folder next to the executable and a bundle have all failed to answer.
    std::function<std::string()> userDataContent;
    Paths compiledIn;          // the MAGICPORTALS_* of this build; all empty for MAGICPORTALS_BAKED_PATHS=OFF
};

// Whether `dir` holds the game as `layout` lays it out: its levels folder and its chapters table. The only question asked of a
// filesystem, and it must only LOOK.
using HoldsGame = std::function<bool(const std::string& dir, const Layout& layout)>;

struct Resolved {
    bool ok = false;
    Paths paths;               // explicit, no ".."
    Source source = Source::None;
    std::string root;          // the folder that answered; empty for CompiledIn and None
    std::string layout;        // "packaged", "repo", or empty for CompiledIn and None
    std::string why;           // the log line: the root and the tier
    std::string error;         // when !ok: what to tell the person, naming what was looked for
    std::vector<std::string> tried; // every folder asked about, in order, with the shape: for the message and the tests
};

Resolved Resolve(const Inputs& inputs, const HoldsGame& holdsGame);

// The predicate the game hands to Resolve: `dir` holds a levels folder and a chapters file as `layout` places them. The one place in
// this file that touches a disk, and it only LOOKS: the std::filesystem overloads that take an error_code neither throw nor create, and
// the suite pins that a call leaves a tree as it found it.
bool HoldsGameOnDisk(const std::string& dir, const Layout& layout);

// A per-path flag over what a tier answered: each replaces ITS path and nothing else, with one exception that is today's behaviour of
// --levels: the art follows the levels' parent unless --art was given too. Values are used as given.
struct Overrides {
    std::optional<std::string> levels;
    std::optional<std::string> art;
    std::optional<std::string> chapters;
    std::optional<std::string> achievements;
    std::optional<std::string> data;
    std::optional<std::string> portData;
    std::optional<std::string> original;
    std::optional<std::string> padArt;
};
Paths Apply(const Paths& base, const Overrides& overrides);

// ---- the string helpers (public so the suite pins them) ----------------------------------------------------------------------------

// '\' becomes '/', repeated separators collapse, "." segments go, "x/.." pairs go, a ".." above an absolute root is dropped, and a
// trailing separator goes except on a root. Empty in, empty out. Never touches the filesystem.
std::string Normalize(const std::string& text);
// A folder joined to a relative path of a Layout ("." is the folder itself), normalised.
std::string Join(const std::string& dir, const std::string& relative);
// Whether the text names a place from the root of a drive or of the file system: "/x", "\x", "C:/x", "C:\x", "//server/x". Judged on
// the text alone so that every host gives one answer.
bool IsAbsolute(const std::string& text);

} // namespace MagicPortals::AssetRoot
