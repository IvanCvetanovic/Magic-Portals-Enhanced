// Where the game finds its data and where it may write (planning step 200, PORT-2): sim/AssetRoot's order of tiers and sim/Scratch's order
// of folders, and the prism writer's half-written name that the shared folder made a hazard.
//
// WHY A SUITE. Until this step the port found its data through paths baked in at build time, and the log, the collision prisms and the
// generated pictures went to the system's temporary folder without asking whether anything could be written there. Both are now RULES,
// and both are pure: the filesystem question is handed in, so what is pinned here is the order and the refusals, with no machine in the
// way. The suite NEVER skips and reads no converted level: it uses opaque fixture folders ("/flag", "/app/content") that exist nowhere,
// and a scratch folder of its own for the few cases that must touch a disk.
//
// THE MUTATION EACH PIN FAILS UNDER is written beside it ("MUTATION:"). They were each run on a copy of the tree (the step's record lists
// the runs); a pin with no mutation would be a check that passes whatever the code does.
//
// Host-independent on purpose: every string here is spelled with '/' or an explicit '\\' and compared as a string. A lexically_normal()
// or is_absolute() in the code under test would give Windows and Linux different answers, as test_mp_tiers once found; the rule under test
// is spelled in strings so that it gives one.

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "sim/AssetRoot.hpp"
#include "sim/Files.hpp"
#include "sim/Prism.hpp"
#include "sim/Scratch.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

using namespace MagicPortals;
namespace AR = MagicPortals::AssetRoot;
namespace fs = std::filesystem;

namespace {

// ---- fixtures -------------------------------------------------------------------------------------------------------------------

// A filesystem that is a set of "<dir>|<shape>" the game is in, and that writes down every question it is asked: the resolver's only
// door to a machine, so the list of questions IS everything it did.
struct FakeDisk {
    std::set<std::string> holds;
    mutable std::vector<std::string> asked;
    AR::HoldsGame Fn() const {
        return [this](const std::string& dir, const AR::Layout& layout) {
            const std::string key = dir + "|" + layout.name;
            asked.push_back(key);
            return holds.count(key) != 0;
        };
    }
};

AR::Inputs NothingBaked() { return {}; }

AR::Paths Baked() {
    AR::Paths baked;
    baked.levels = "/build/out/levels";
    baked.art = "/build/out/levels/..";
    baked.chapters = "/build/out/levels/../data/chapters.json";
    baked.achievements = "/build/out/levels/../data/achievements.json";
    baked.data = "/build/data";
    baked.portData = "/build/game/sim/../data";
    baked.original = "/build/out/levels/../../reference/extracted/assets";
    baked.padArt = "/build/reference/magic-rampage/controls";
    return baked;
}

bool HasDotDot(const std::string& path) {
    // A ".." segment, not the substring (a file may be called "a..b").
    std::string s = path;
    std::replace(s.begin(), s.end(), '\\', '/');
    return ("/" + s + "/").find("/../") != std::string::npos;
}

bool NoDotDot(const AR::Paths& p) {
    return !HasDotDot(p.levels) && !HasDotDot(p.art) && !HasDotDot(p.chapters) && !HasDotDot(p.achievements) && !HasDotDot(p.data) &&
           !HasDotDot(p.portData) && !HasDotDot(p.original) && !HasDotDot(p.padArt);
}

std::string Joined(const std::vector<std::string>& v) {
    std::string s;
    for (const std::string& item : v) s += (s.empty() ? "" : "; ") + item;
    return s.empty() ? "(none)" : s;
}

fs::path Folder(const char* name) {
    const fs::path dir = fs::temp_directory_path() / "supersonic-test-mp-assetroot" / name;
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    return dir;
}

void Touch(const fs::path& file) {
    std::error_code ec;
    fs::create_directories(file.parent_path(), ec);
    std::ofstream(file, std::ios::binary) << "x";
}

// Every file and folder under `root`, relative and sorted: what a call left behind is what this differs from.
std::vector<std::string> Tree(const fs::path& root) {
    std::vector<std::string> names;
    std::error_code ec;
    for (fs::recursive_directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec)) {
        names.push_back(fs::relative(it->path(), root).generic_string());
    }
    std::sort(names.begin(), names.end());
    return names;
}

// ---- the layouts ----------------------------------------------------------------------------------------------------------------

// MUTATION: Packaged() given Repo()'s levels or chapters (two shapes that answer alike cannot tell a root's shape); a ".." put into a
// layout's field; Repo()'s art "out/levels" (res:// would then stand for the levels folder).
void TheTwoLayoutsAreTheRepositoryAndThePackage() {
    const AR::Layout repo = AR::Layout::Repo();
    CHECK_MSG(repo.name == "repo" && repo.levels == "out/levels" && repo.art == "out" && repo.chapters == "out/data/chapters.json" &&
                  repo.achievements == "out/data/achievements.json" && repo.data == "data" && repo.portData == "game/data" &&
                  repo.original == "reference/extracted/assets" && repo.padArt == "reference/magic-rampage/controls",
              "Repo() mirrors this repository");
    const AR::Layout pack = AR::Layout::Packaged();
    CHECK_MSG(pack.name == "packaged" && pack.levels == "levels" && pack.art == "." && pack.chapters == "tables/chapters.json" &&
                  pack.achievements == "tables/achievements.json" && pack.data == "data" && pack.portData == "port-data" &&
                  pack.original == "original" && pack.padArt == "pad-art/controls",
              "Packaged() is the folder docs/packaging.md describes");
    CHECK_MSG(repo.levels != pack.levels && repo.chapters != pack.chapters && repo.original != pack.original && repo.portData != pack.portData,
              "the two shapes differ where a folder is asked about, so one root is never taken for the other");
    for (const AR::Layout& layout : {repo, pack}) {
        for (const std::string& field : {layout.levels, layout.art, layout.chapters, layout.achievements, layout.data, layout.portData,
                                         layout.original, layout.padArt}) {
            CHECK_MSG(!HasDotDot(field) && field.find('\\') == std::string::npos && !AR::IsAbsolute(field),
                      layout.name + ": '" + field + "' is relative, '/'-separated and has no '..'");
        }
    }
}

// The interface languages (planning step 239) read three things by a path built from a layout's fields: the string tables at <portData>/lang, the picker's table at
// <portData>/picker.json, and the achievements' translation as a SIBLING of the English list (<folder of achievements>/achievements.<code>.json). The last is the
// one that depends on the layouts agreeing: the packaged folder holds the English list and the translations in tables/, this repository in out/data/.
//
// MUTATION: Packaged() with its achievements in a folder of its own (the translations would then be looked for where the packager does not put them).
void TheLanguageFilesAreWhereBothLayoutsKeepThem() {
    const auto folderOf = [](const std::string& path) {
        const std::size_t slash = path.rfind('/');
        return slash == std::string::npos ? std::string(".") : path.substr(0, slash);
    };
    const auto nameOf = [](const std::string& path) {
        const std::size_t slash = path.rfind('/');
        return slash == std::string::npos ? path : path.substr(slash + 1);
    };
    for (const AR::Layout& layout : {AR::Layout::Repo(), AR::Layout::Packaged()}) {
        CHECK_MSG(nameOf(layout.achievements) == "achievements.json", layout.name + ": the English list is achievements.json: " + layout.achievements);
        CHECK_MSG(folderOf(layout.achievements) == folderOf(layout.chapters),
                  layout.name + ": it is beside chapters.json, where the packager keeps both (tables/) and this repository's converter writes them (out/data/)");
        CHECK_MSG(!layout.portData.empty() && layout.portData != "." && layout.portData.find("..") == std::string::npos,
                  layout.name + ": the port data is a folder (game/data or port-data), under which lang/, fonts/ and picker.json are: " + layout.portData);
    }
    CHECK_MSG(folderOf(AR::Layout::Packaged().achievements) == "tables" && folderOf(AR::Layout::Repo().achievements) == "out/data",
              "tables/ in the package, out/data/ in the repository");
}

// ---- the string rules -----------------------------------------------------------------------------------------------------------

// MUTATION: Normalize that leaves a "x/.." pair, keeps a backslash, keeps a trailing separator, lets ".." climb above an absolute root,
// or turns an empty string into ".".
void NormalizeSpellsAPathOut() {
    struct Row {
        const char* in;
        const char* out;
    };
    const Row rows[] = {
        {"", ""},
        {"/a/b", "/a/b"},
        {"/a//b/", "/a/b"},
        {"/a/./b", "/a/b"},
        {"/a/b/..", "/a"},
        {"/a/b/../c", "/a/c"},
        {"/a/..", "/"},
        {"/a/../..", "/"},
        {"/", "/"},
        {"C:\\x\\y\\..\\z", "C:/x/z"},
        {"C:/", "C:/"},
        {"C:\\", "C:/"},
        {"C:/x/..", "C:/"},
        {"C:/a/b/c/../../..", "C:/"},
        {"C:/a/../../..", "C:/"},
        {"//server/share/x/../y", "//server/share/y"},
        {"a/b/..", "a"},
        {"a/..", "."},
        {".", "."},
        {"../a", "../a"},
        {"a/../../b", "../b"},
        {"/build/out/levels/..", "/build/out"},
        {"/build/out/levels/../data/chapters.json", "/build/out/data/chapters.json"},
        {"/build/out/levels/../../reference/extracted/assets", "/build/reference/extracted/assets"},
        {"/build/game/sim/../data", "/build/game/data"},
        {"/a/b../c", "/a/b../c"},
        {"/a/..b/c", "/a/..b/c"},
    };
    for (const Row& row : rows) {
        const std::string got = AR::Normalize(row.in);
        CHECK_MSG(got == row.out, std::string("Normalize('") + row.in + "') is '" + got + "', want '" + row.out + "'");
    }
}

// MUTATION: IsAbsolute that asks the host (std::filesystem::path::is_absolute: "/x" is not absolute on Windows, "C:/x" is not on Linux),
// that takes a bare drive letter or a relative path, or that refuses the backslash forms.
void WhatIsAbsoluteIsJudgedOnTheTextAlone() {
    for (const char* yes : {"/x", "\\x", "C:/x", "C:\\x", "c:/", "//server/x", "/"}) {
        CHECK_MSG(AR::IsAbsolute(yes), std::string("'") + yes + "' names a place from a root");
    }
    for (const char* no : {"", "x", "x/y", "./x", "../x", "C:", "C:x", "content", ".", ".."}) {
        CHECK_MSG(!AR::IsAbsolute(no), std::string("'") + no + "' does not");
    }
}

// MUTATION: Join that does not spell its answer out (a doubled separator, a trailing one, a "." segment), or answers for an empty folder.
void JoinPutsARelativePathUnderAFolder() {
    CHECK(AR::Join("/a/b", "levels") == "/a/b/levels");
    CHECK(AR::Join("/a/b/", "out/levels") == "/a/b/out/levels");
    CHECK(AR::Join("/a/b/", ".") == "/a/b");
    CHECK(AR::Join("/a/b", "") == "/a/b");
    CHECK(AR::Join("C:\\a\\b", "tables/chapters.json") == "C:/a/b/tables/chapters.json");
    CHECK_MSG(AR::Join("", "x").empty(), "no folder, no path");
    CHECK(AR::Join("/a/b/c/..", "x") == "/a/b/x");
    // The file system's root (planning step 203, PORT2-3 (b)): a root of "/" used to join as "//content", a network name to Normalize.
    // MUTATION: join a slash-only folder as dir + "/" + relative again.
    CHECK_MSG(AR::Join("/", "content") == "/content", "the root joins as itself: " + AR::Join("/", "content"));
    CHECK_MSG(AR::Join("/", "a/b") == "/a/b", "and with a longer path");
    CHECK_MSG(AR::Join("//", "x") == "/x" && AR::Join("///", "x") == "/x", "any number of slashes is the root, as it has always been for two");
    CHECK_MSG(AR::Join("/", ".") == "/" && AR::Join("/", "") == "/", "and the root itself stays the root");
    CHECK_MSG(AR::Join("C:/", "x") == "C:/x" && AR::Join("C:", "x") == "C:/x" && AR::Join("C:\\", "x") == "C:/x", "a drive's root is not touched");
    CHECK_MSG(AR::Join("/a", "content") == "/a/content" && AR::Join("/a/", "content") == "/a/content", "nor is any other folder");
    CHECK_MSG(AR::Join("/", "../x") == "/x", "and a climb out of the root stays at the root");
}

// ---- the order of the tiers -----------------------------------------------------------------------------------------------------

// MUTATION: --root asked after the folder next to the executable; the first of two shapes that both hold not being Packaged.
void TheFlagBeatsEveryOtherTier() {
    FakeDisk disk;
    disk.holds = {"/flag|packaged", "/app/content|packaged", "/app|packaged", "/bundle|packaged", "/user/content|packaged"};
    AR::Inputs in = NothingBaked();
    in.flagRoot = "/flag";
    in.exeDir = "/app";
    in.bundleContent = "/bundle";
    in.userDataContent = [] { return std::string("/user/content"); };
    in.compiledIn = Baked();
    const AR::Resolved r = AR::Resolve(in, disk.Fn());
    CHECK_MSG(r.ok && r.source == AR::Source::Flag && r.root == "/flag" && r.layout == "packaged", r.error + " " + r.why);
    CHECK_MSG(r.paths.levels == "/flag/levels" && r.paths.chapters == "/flag/tables/chapters.json", r.paths.levels);
    CHECK_MSG(disk.asked == std::vector<std::string>{"/flag|packaged"}, "no other tier was asked once the flag answered: " + Joined(disk.asked));
}

// A flag that does not hold the game is an error that names it, never a fall-through (as --padart is strict).
// MUTATION: a failed --root falling through to the next tier; the message not naming the folder or the flag; --root accepting a
// relative path; the filesystem being asked about a relative --root.
void AFlagThatDoesNotHoldTheGameIsAnErrorNotAFallThrough() {
    FakeDisk disk;
    disk.holds = {"/app/content|packaged", "/user/content|packaged"}; // everything else would answer
    AR::Inputs in = NothingBaked();
    in.flagRoot = "/not/a/game";
    in.exeDir = "/app";
    in.userDataContent = [] { return std::string("/user/content"); };
    in.compiledIn = Baked();
    const AR::Resolved r = AR::Resolve(in, disk.Fn());
    CHECK_MSG(!r.ok && r.source == AR::Source::None, "a bad --root is not answered by a later tier");
    CHECK_MSG(r.error.find("--root") != std::string::npos && r.error.find("/not/a/game") != std::string::npos &&
                  r.error.find("does not hold the game") != std::string::npos,
              "the message names the flag and the folder: " + r.error);
    CHECK_MSG(r.error.find("levels/") != std::string::npos && r.error.find("chapters.json") != std::string::npos,
              "and what it must hold: " + r.error);
    CHECK_MSG(disk.asked == (std::vector<std::string>{"/not/a/game|packaged", "/not/a/game|repo"}),
              "only the flag's folder was asked about, in both shapes: " + Joined(disk.asked));

    FakeDisk none;
    AR::Inputs relative = NothingBaked();
    relative.flagRoot = "content/here";
    relative.exeDir = "/app";
    const AR::Resolved bad = AR::Resolve(relative, none.Fn());
    CHECK_MSG(!bad.ok && bad.error.find("absolute") != std::string::npos && bad.error.find("content/here") != std::string::npos, bad.error);
    CHECK_MSG(none.asked.empty(), "a relative --root is refused before the disk is asked: " + Joined(none.asked));
}

// MUTATION: the executable's own folder tried before its content folder; the content folder not tried; the executable's tier skipped
// when --root is empty.
void NextToTheExecutableIsContentFirstThenTheFolderItself() {
    {
        FakeDisk disk;
        disk.holds = {"/app/content|packaged", "/app|packaged"};
        AR::Inputs in = NothingBaked();
        in.exeDir = "/app";
        const AR::Resolved r = AR::Resolve(in, disk.Fn());
        CHECK_MSG(r.ok && r.source == AR::Source::BesideExecutable && r.root == "/app/content", r.root);
        CHECK_MSG(disk.asked == std::vector<std::string>{"/app/content|packaged"}, Joined(disk.asked));
    }
    {
        FakeDisk disk;
        disk.holds = {"/app|packaged"};
        AR::Inputs in = NothingBaked();
        in.exeDir = "/app";
        const AR::Resolved r = AR::Resolve(in, disk.Fn());
        CHECK_MSG(r.ok && r.source == AR::Source::BesideExecutable && r.root == "/app", r.root + " " + r.error);
        CHECK_MSG(disk.asked == (std::vector<std::string>{"/app/content|packaged", "/app/content|repo", "/app|packaged"}), Joined(disk.asked));
    }
}

// MUTATION: the bundle tier after the user's data folder; the user's data folder before the executable's.
void TheBundleComesBeforeTheUsersDataFolderWhichComesBeforeTheBuild() {
    FakeDisk disk;
    disk.holds = {"/bundle|packaged", "/user/content|packaged"};
    AR::Inputs in = NothingBaked();
    in.exeDir = "/app";
    in.bundleContent = "/bundle";
    in.userDataContent = [] { return std::string("/user/content"); };
    in.compiledIn = Baked();
    const AR::Resolved r = AR::Resolve(in, disk.Fn());
    CHECK_MSG(r.ok && r.source == AR::Source::Bundle && r.root == "/bundle", r.root);

    FakeDisk userOnly;
    userOnly.holds = {"/user/content|repo"};
    in.bundleContent.clear();
    const AR::Resolved u = AR::Resolve(in, userOnly.Fn());
    CHECK_MSG(u.ok && u.source == AR::Source::UserData && u.root == "/user/content" && u.layout == "repo", u.root + " " + u.layout);
    CHECK_MSG(AR::SourceName(u.source) == std::string("the user's data folder"), AR::SourceName(u.source));
    CHECK_MSG(u.paths.levels == "/user/content/out/levels" && u.paths.padArt == "/user/content/reference/magic-rampage/controls",
              "a repository-shaped folder is read as one: " + u.paths.levels);
}

// The user's data folder is the one the engine MAKES when it answers, so it is asked for only when nothing before it did.
// MUTATION: userDataContent evaluated whenever Resolve runs (called before the tiers above it, or when --root answered).
void TheUsersDataFolderIsAskedForOnlyWhenNothingBeforeItAnswered() {
    int asked = 0;
    FakeDisk disk;
    disk.holds = {"/app/content|packaged"};
    AR::Inputs in = NothingBaked();
    in.exeDir = "/app";
    in.userDataContent = [&asked] {
        ++asked;
        return std::string("/user/content");
    };
    in.compiledIn = Baked();
    CHECK(AR::Resolve(in, disk.Fn()).ok);
    CHECK_MSG(asked == 0, "the folder next to the executable answered, so the user's data folder was never named: " + std::to_string(asked));

    FakeDisk flag;
    flag.holds = {"/flag|packaged"};
    in.flagRoot = "/flag";
    CHECK(AR::Resolve(in, flag.Fn()).ok);
    CHECK_MSG(asked == 0, "nor when --root answered");

    FakeDisk empty;
    in.flagRoot.clear();
    const AR::Resolved r = AR::Resolve(in, empty.Fn());
    CHECK_MSG(asked == 1 && r.source == AR::Source::CompiledIn, "named once, when it was needed, and then the build's paths answered");
    // An unset provider, and one that answers nothing, are the same: no such tier.
    in.userDataContent = nullptr;
    CHECK(AR::Resolve(in, empty.Fn()).source == AR::Source::CompiledIn);
    in.userDataContent = [] { return std::string(); };
    CHECK(AR::Resolve(in, empty.Fn()).source == AR::Source::CompiledIn);
}

// Packaged is tried before Repo, and the shape that matched is reported with the paths that shape lays out.
// MUTATION: Repo tried first; Repo's paths built with Packaged's layout; the matched shape not recorded.
void BothShapesAreTriedPackagedFirst() {
    FakeDisk both;
    both.holds = {"/r|packaged", "/r|repo"};
    AR::Inputs in = NothingBaked();
    in.flagRoot = "/r";
    const AR::Resolved a = AR::Resolve(in, both.Fn());
    CHECK_MSG(a.ok && a.layout == "packaged" && a.paths.levels == "/r/levels", a.layout);

    FakeDisk repo;
    repo.holds = {"/r|repo"};
    const AR::Resolved b = AR::Resolve(in, repo.Fn());
    CHECK_MSG(b.ok && b.layout == "repo", b.layout + " " + b.error);
    CHECK(repo.asked == (std::vector<std::string>{"/r|packaged", "/r|repo"}));
    CHECK_MSG(b.paths.levels == "/r/out/levels" && b.paths.art == "/r/out" && b.paths.chapters == "/r/out/data/chapters.json" &&
                  b.paths.achievements == "/r/out/data/achievements.json" && b.paths.data == "/r/data" && b.paths.portData == "/r/game/data" &&
                  b.paths.original == "/r/reference/extracted/assets" && b.paths.padArt == "/r/reference/magic-rampage/controls",
              "Repo's paths");
    CHECK_MSG(a.paths.art == "/r" && a.paths.achievements == "/r/tables/achievements.json" && a.paths.data == "/r/data" &&
                  a.paths.portData == "/r/port-data" && a.paths.original == "/r/original" && a.paths.padArt == "/r/pad-art/controls",
              "Packaged's paths");
    CHECK(NoDotDot(a.paths) && NoDotDot(b.paths));
}

// The build's own paths are the last resort, taken as they are (no question to the disk), with the ".." they were spelled with removed.
// MUTATION: the build's paths asked about (a missing out/ would then fail a start that has always run as boxes); a ".." left in; an
// empty path turned into "." (Normalize's own rule for "", pinned in NormalizeSpellsAPathOut too).
void TheBuildsOwnPathsAreTheLastResort() {
    FakeDisk disk;
    AR::Inputs in = NothingBaked();
    in.exeDir = "/app";
    in.compiledIn = Baked();
    const AR::Resolved r = AR::Resolve(in, disk.Fn());
    CHECK_MSG(r.ok && r.source == AR::Source::CompiledIn && r.root.empty() && r.layout.empty(), r.error);
    CHECK_MSG(disk.asked == (std::vector<std::string>{"/app/content|packaged", "/app/content|repo", "/app|packaged", "/app|repo"}),
              "only the tiers above it were asked: " + Joined(disk.asked));
    CHECK_MSG(r.paths.levels == "/build/out/levels" && r.paths.art == "/build/out" && r.paths.chapters == "/build/out/data/chapters.json" &&
                  r.paths.achievements == "/build/out/data/achievements.json" && r.paths.data == "/build/data" &&
                  r.paths.portData == "/build/game/data" && r.paths.original == "/build/reference/extracted/assets" &&
                  r.paths.padArt == "/build/reference/magic-rampage/controls",
              "spelled out: " + r.paths.chapters);
    CHECK_MSG(NoDotDot(r.paths), "no '..' in what is handed back");
    CHECK_MSG(r.why == "asset root: the paths this build was configured with (tier: compiled in)", r.why);

    // A path the build did not bake stays empty.
    AR::Inputs partial = NothingBaked();
    partial.compiledIn.levels = "/b/levels";
    const AR::Resolved p = AR::Resolve(partial, disk.Fn());
    CHECK_MSG(p.ok && p.paths.levels == "/b/levels" && p.paths.chapters.empty() && p.paths.padArt.empty() && p.paths.original.empty(),
              "an empty path is not turned into '.'");
}

// A build configured with nothing baked, and no root anywhere: an error that says where it looked and how to say where the data is.
// MUTATION: CompiledIn accepted when nothing is baked (a stray relative open); the message naming neither --root nor where it looked.
void NothingBakedAndNoRootIsAnErrorThatSaysWhereToLook() {
    FakeDisk disk;
    AR::Inputs in = NothingBaked();
    in.exeDir = "/app";
    in.userDataContent = [] { return std::string("/user/content"); };
    const AR::Resolved r = AR::Resolve(in, disk.Fn());
    CHECK_MSG(!r.ok && r.source == AR::Source::None && r.root.empty(), "nothing answered");
    CHECK_MSG(r.error.find("--root") != std::string::npos && r.error.find("/app/content") != std::string::npos &&
                  r.error.find("/user/content") != std::string::npos && r.error.find("content") != std::string::npos,
              r.error);
    CHECK_MSG(r.paths.Empty(), "and no path is handed back to be opened by mistake");
    CHECK_MSG(r.tried.size() == 6, "every folder asked about is listed: " + Joined(r.tried));

    // The same with no executable folder and no user folder named: still an error, with the way out.
    const AR::Resolved bare = AR::Resolve(NothingBaked(), disk.Fn());
    CHECK_MSG(!bare.ok && bare.error.find("--root") != std::string::npos && bare.tried.empty(), bare.error);
}

// The log's line for each tier (the first lines of a log name the root and the tier).
// MUTATION: a line that does not name the root or the tier; the words of a tier changed.
void EachTierSaysItselfInOneLine() {
    FakeDisk disk;
    disk.holds = {"/flag|packaged"};
    AR::Inputs in = NothingBaked();
    in.flagRoot = "/flag";
    CHECK_MSG(AR::Resolve(in, disk.Fn()).why == "asset root: /flag (tier: --root, layout: packaged)", AR::Resolve(in, disk.Fn()).why);
    FakeDisk beside;
    beside.holds = {"/app/content|repo"};
    AR::Inputs b = NothingBaked();
    b.exeDir = "/app";
    CHECK_MSG(AR::Resolve(b, beside.Fn()).why == "asset root: /app/content (tier: beside the executable, layout: repo)",
              AR::Resolve(b, beside.Fn()).why);
    CHECK(AR::SourceName(AR::Source::Bundle) == std::string("the application bundle"));
    CHECK(AR::SourceName(AR::Source::CompiledIn) == std::string("compiled in"));
    CHECK(AR::SourceName(AR::Source::None) == std::string("none"));
}

// A root typed with backslashes, doubled separators or a ".." is spelled out before anything is joined to it.
// MUTATION: the root joined as typed (a ".." in the result, a mixed separator).
void ARootIsSpelledOutBeforeItIsUsed() {
    FakeDisk disk;
    disk.holds = {"C:/games/mp/content|packaged"};
    AR::Inputs in = NothingBaked();
    in.flagRoot = "C:\\games\\mp\\old\\..\\content\\";
    const AR::Resolved r = AR::Resolve(in, disk.Fn());
    CHECK_MSG(r.ok && r.root == "C:/games/mp/content" && r.paths.levels == "C:/games/mp/content/levels", r.root + " " + r.error);
    CHECK_MSG(NoDotDot(r.paths), "no '..'");
}

// ---- the flags over a root ------------------------------------------------------------------------------------------------------

AR::Paths Marked() {
    AR::Paths p;
    p.levels = "/m/levels";
    p.art = "/m";
    p.chapters = "/m/ch.json";
    p.achievements = "/m/ach.json";
    p.data = "/m/data";
    p.portData = "/m/pd";
    p.original = "/m/orig";
    p.padArt = "/m/pad";
    return p;
}

// Each per-path flag replaces its own path and no other, as given.
// MUTATION: a flag replacing a neighbour's path (--chapters writing achievements, --data writing portData); a value normalised or
// altered; --levels moving chapters or original.
void EachPathFlagReplacesItsOwnPathAndNoOther() {
    struct Case {
        const char* name;
        std::optional<std::string> AR::Overrides::*member;
        std::string AR::Paths::*field;
    };
    const Case cases[] = {
        {"levels", &AR::Overrides::levels, &AR::Paths::levels},
        {"art", &AR::Overrides::art, &AR::Paths::art},
        {"chapters", &AR::Overrides::chapters, &AR::Paths::chapters},
        {"achievements", &AR::Overrides::achievements, &AR::Paths::achievements},
        {"data", &AR::Overrides::data, &AR::Paths::data},
        {"portData", &AR::Overrides::portData, &AR::Paths::portData},
        {"original", &AR::Overrides::original, &AR::Paths::original},
        {"padArt", &AR::Overrides::padArt, &AR::Paths::padArt},
    };
    const AR::Paths base = Marked();
    for (const Case& c : cases) {
        AR::Overrides ov;
        ov.*(c.member) = std::string("C:\\given\\..\\as\\typed");
        const AR::Paths got = AR::Apply(base, ov);
        CHECK_MSG(got.*(c.field) == "C:\\given\\..\\as\\typed", std::string(c.name) + " is used as given: " + got.*(c.field));
        for (const Case& other : cases) {
            if (&other == &c) continue;
            // --levels moves the art (below); every other pair is untouched.
            if (std::string(c.name) == "levels" && std::string(other.name) == "art") continue;
            CHECK_MSG(got.*(other.field) == base.*(other.field), std::string("--") + c.name + " moved " + other.name);
        }
    }
    CHECK_MSG(AR::Apply(base, AR::Overrides{}).levels == base.levels && AR::Apply(base, AR::Overrides{}).original == base.original,
              "no flag, no change");
}

// --levels brings its art with it (the parent of the levels) unless --art says otherwise; it never moves the chapters table.
// MUTATION: the art not following --levels; --art ignored when --levels is given; the art taking the levels' own path; a ".." left in.
void LevelsBringTheirArtWithThemUnlessArtIsGiven() {
    const AR::Paths base = Marked();
    AR::Overrides levels;
    levels.levels = "/elsewhere/out/levels";
    const AR::Paths a = AR::Apply(base, levels);
    CHECK_MSG(a.levels == "/elsewhere/out/levels" && a.art == "/elsewhere/out" && !HasDotDot(a.art), a.art);
    CHECK_MSG(a.chapters == base.chapters && a.original == base.original && a.data == base.data, "--levels does not move the other tables");
    levels.art = "/art/only";
    const AR::Paths b = AR::Apply(base, levels);
    CHECK_MSG(b.levels == "/elsewhere/out/levels" && b.art == "/art/only", "--art wins over the parent: " + b.art);
    AR::Overrides emptyLevels;
    emptyLevels.levels = "";
    CHECK_MSG(AR::Apply(base, emptyLevels).art == base.art, "no levels, no parent");
}

// ---- the disk: it only looks ----------------------------------------------------------------------------------------------------

// The real predicate main hands in, and a tree left exactly as it was found: nothing is created, `content` included.
// MUTATION: HoldsGameOnDisk (or Resolve) making the folder it asks about (create_directories on the candidate); a predicate that
// accepts a file for the levels folder or a folder for the chapters table.
void ResolvingAgainstARealTreeCreatesNothing() {
    const fs::path root = Folder("looks");
    const std::string exe = (root / "program").generic_string();
    fs::create_directories(root / "program");
    fs::create_directories(root / "userdata");
    Touch(root / "program" / "unrelated.txt");
    const std::vector<std::string> before = Tree(root);

    AR::Inputs in = NothingBaked();
    in.exeDir = exe;
    int named = 0;
    in.userDataContent = [&] {
        ++named;
        return (root / "userdata" / "content").generic_string();
    };
    const AR::Resolved none = AR::Resolve(in, AR::HoldsGameOnDisk);
    CHECK_MSG(!none.ok && named == 1, none.error);
    CHECK_MSG(Tree(root) == before, "a resolve that found nothing left the tree as it was: " + Joined(Tree(root)));
    std::error_code ec;
    CHECK_MSG(!fs::exists(root / "program" / "content", ec) && !fs::exists(root / "userdata" / "content", ec), "no content folder was made");

    // A packaged folder next to the program is found, and again nothing is made.
    Touch(root / "program" / "content" / "levels" / "level0.tscn");
    Touch(root / "program" / "content" / "tables" / "chapters.json");
    const std::vector<std::string> withGame = Tree(root);
    const AR::Resolved found = AR::Resolve(in, AR::HoldsGameOnDisk);
    CHECK_MSG(found.ok && found.source == AR::Source::BesideExecutable && found.layout == "packaged" &&
                  found.root == (root / "program" / "content").generic_string(),
              found.error + " " + found.root);
    CHECK_MSG(Tree(root) == withGame, "and a resolve that found it made nothing either");

    // A repository-shaped root given by --root.
    const fs::path repo = Folder("repo-shape");
    Touch(repo / "out" / "levels" / "a.tscn");
    Touch(repo / "out" / "data" / "chapters.json");
    AR::Inputs flag = NothingBaked();
    flag.flagRoot = repo.generic_string();
    const AR::Resolved r = AR::Resolve(flag, AR::HoldsGameOnDisk);
    CHECK_MSG(r.ok && r.layout == "repo" && r.paths.levels == (repo / "out" / "levels").generic_string(), r.error + " " + r.layout);

    // The two things it asks for must be the right kinds: a FILE called levels is no levels folder, a FOLDER called chapters.json is no table.
    const fs::path fileForLevels = Folder("wrong-kinds-levels");
    Touch(fileForLevels / "levels"); // a file
    Touch(fileForLevels / "tables" / "chapters.json");
    CHECK_MSG(!AR::HoldsGameOnDisk(fileForLevels.generic_string(), AR::Layout::Packaged()), "a file called levels is no levels folder");
    const fs::path folderForChapters = Folder("wrong-kinds-chapters");
    fs::create_directories(folderForChapters / "levels");
    fs::create_directories(folderForChapters / "tables" / "chapters.json"); // a folder
    CHECK_MSG(!AR::HoldsGameOnDisk(folderForChapters.generic_string(), AR::Layout::Packaged()), "a folder called chapters.json is no table");
    const fs::path wrong = folderForChapters;
    CHECK_MSG(!AR::HoldsGameOnDisk((wrong / "nowhere").generic_string(), AR::Layout::Packaged()), "a folder that is not there does not");
    CHECK_MSG(!AR::HoldsGameOnDisk("", AR::Layout::Packaged()), "an empty folder name does not");
}

// ---- the scratch folder ---------------------------------------------------------------------------------------------------------

// A probe that fails on demand and writes down what it was asked.
struct FakeProbe {
    std::set<std::string> failing;
    std::vector<std::string> asked;
    Scratch::Probe Fn() {
        return [this](const fs::path& dir, std::string& why) {
            asked.push_back(dir.generic_string());
            if (failing.count(dir.generic_string()) != 0) {
                why = "refused by the fake";
                return false;
            }
            return true;
        };
    }
};

Scratch::Candidate At(const std::string& label, const std::string& path, int* evaluated = nullptr, bool makes = false) {
    return {label, [path, evaluated] {
                if (evaluated != nullptr) ++*evaluated;
                return fs::path(path);
            },
            makes};
}

// MUTATION: the candidates tried in another order; the first not taken when it passes; the prism folder not under the candidate.
void TheFirstFolderThatCanBeWrittenWins() {
    FakeProbe probe;
    const Scratch::Chosen a = Scratch::Choose({At("temp", "/t"), At("user", "/u/scratch", nullptr, true)}, probe.Fn());
    CHECK_MSG(a.ok && a.index == 0 && a.base == fs::path("/t") && a.prisms == fs::path("/t") / Scratch::kPrismFolder && a.label == "temp",
              a.note);
    CHECK_MSG(std::string(Scratch::kPrismFolder) == "supersonic-magicportals", "the prism folder keeps the name it has always had");
    CHECK_MSG(a.note.find("/t") != std::string::npos && a.note.find("temp") != std::string::npos && a.passedOver.empty(), a.note);
}

// The second candidate is evaluated only when the first failed: it is the one that makes a folder.
// MUTATION: every candidate evaluated up front (the user's data folder made on every start).
void ALaterCandidateIsNeverEvaluatedUnlessEarlierOnesFailed() {
    FakeProbe probe;
    int first = 0;
    int second = 0;
    const Scratch::Chosen a = Scratch::Choose({At("temp", "/t", &first), At("user", "/u", &second, true)}, probe.Fn());
    CHECK_MSG(a.ok && first == 1 && second == 0, "the second was never asked for: " + std::to_string(second));

    probe.failing = {"/t"};
    first = second = 0;
    probe.asked.clear();
    const Scratch::Chosen b = Scratch::Choose({At("temp", "/t", &first), At("user", "/u", &second, true)}, probe.Fn());
    CHECK_MSG(b.ok && b.index == 1 && b.base == fs::path("/u") && first == 1 && second == 1, b.note);
    CHECK_MSG(b.passedOver.size() == 1 && b.passedOver[0].find("temp") != std::string::npos &&
                  b.passedOver[0].find("refused by the fake") != std::string::npos,
              "and says why the first was passed over: " + Joined(b.passedOver));
    CHECK_MSG(b.note.find("made for it") != std::string::npos && b.note.find("passed over temp") != std::string::npos,
              "and that it made the folder: " + b.note);
    CHECK_MSG(probe.asked == (std::vector<std::string>{"/t", "/u", "/u/supersonic-magicportals"}),
              "the folder is asked first, then the prism folder in it, then the next candidate: " + Joined(probe.asked));
}

// A folder that can be written while the prism folder in it cannot is passed over: another account's folder in a shared /tmp.
// MUTATION: only the candidate itself probed.
void AFolderWhosePrismFolderIsRefusedIsPassedOver() {
    FakeProbe probe;
    probe.failing = {"/tmp/supersonic-magicportals"};
    const Scratch::Chosen c = Scratch::Choose({At("temp", "/tmp"), At("user", "/u/scratch", nullptr, true)}, probe.Fn());
    CHECK_MSG(c.ok && c.index == 1 && c.base == fs::path("/u/scratch"), c.note);
}

// MUTATION: an empty path or a candidate that throws ending the search; none usable reported as ok.
void NothingUsableIsSaidAndNothingThrows() {
    FakeProbe probe;
    int evaluated = 0;
    Scratch::Candidate empty{"unset", [] { return fs::path(); }, false};
    Scratch::Candidate throws{"throws", []() -> fs::path { throw std::runtime_error("no temp"); }, false};
    Scratch::Candidate noFunction{"none", nullptr, false};
    const Scratch::Chosen a = Scratch::Choose({empty, throws, noFunction, At("temp", "/t", &evaluated)}, probe.Fn());
    CHECK_MSG(a.ok && a.index == 3 && a.passedOver.size() == 3, a.note);
    CHECK_MSG(a.passedOver.size() == 3 && a.passedOver[1].find("no temp") != std::string::npos,
              "a throw is a reason, not a crash: " + Joined(a.passedOver));

    probe.failing = {"/t"};
    const Scratch::Chosen none = Scratch::Choose({empty, At("temp", "/t")}, probe.Fn());
    CHECK_MSG(!none.ok && none.base.empty() && none.prisms.empty() && none.note.find("no scratch folder") != std::string::npos, none.note);
    CHECK_MSG(none.passedOver.size() == 2, "every reason is kept: " + Joined(none.passedOver));
    CHECK(!Scratch::Choose({}, probe.Fn()).ok);
}

// The real probe, against real folders: a folder it can make, one under a regular file (the stand-in for a folder an app cannot write),
// and one it must leave clean.
// MUTATION: ProbeWritable that reports true for a path under a file; that leaves its test file behind; that does not make a missing folder.
void TheRealProbeMakesTheFolderTriesAFileAndLeavesNothingBehind() {
    const fs::path root = Folder("probe");
    std::string why;
    CHECK_MSG(Scratch::ProbeWritable(root / "made" / "deep", why), why);
    std::error_code ec;
    CHECK_MSG(fs::is_directory(root / "made" / "deep", ec), "a missing folder is made");
    CHECK_MSG(fs::is_empty(root / "made" / "deep", ec), "and the probe's file is gone again");
    CHECK_MSG(Scratch::ProbeWritable(root / "made" / "deep", why), "twice is fine");

    Touch(root / "afile");
    why.clear();
    CHECK_MSG(!Scratch::ProbeWritable(root / "afile" / "below", why) && !why.empty(), "a place under a regular file cannot be made: " + why);
    why.clear();
    CHECK_MSG(!Scratch::ProbeWritable(root / "afile", why) && !why.empty(), "a regular file is not a folder: " + why);
}

// The whole rule on a real disk: the first candidate cannot be made (under a regular file), so it falls to the second, which is the
// only one that is made, and the third is never touched.
// MUTATION: the failed candidate's folder created anyway; the third candidate made; the second not made.
void AnUnwritableFirstFolderFallsToTheSecondAndOnlyThenIsItMade() {
    const fs::path root = Folder("fallback");
    Touch(root / "afile");
    const fs::path second = root / "made" / "scratch";
    const fs::path third = root / "never";
    int thirdEvaluated = 0;
    std::error_code ec;
    CHECK(!fs::exists(second, ec));
    const Scratch::Chosen chosen = Scratch::Choose(
        {At("under a file", (root / "afile" / "temp").generic_string()), At("user", second.generic_string(), nullptr, true),
         At("third", third.generic_string(), &thirdEvaluated)},
        Scratch::ProbeWritable);
    CHECK_MSG(chosen.ok && chosen.index == 1 && chosen.base == second && chosen.prisms == second / "supersonic-magicportals", chosen.note);
    CHECK_MSG(fs::is_directory(second / "supersonic-magicportals", ec), "the second folder was made, with the prism folder in it");
    CHECK_MSG(!fs::exists(root / "afile" / "temp", ec), "nothing was made under the file");
    CHECK_MSG(thirdEvaluated == 0 && !fs::exists(third, ec), "and the third was not evaluated or made");
    CHECK_MSG(Tree(second) == std::vector<std::string>{"supersonic-magicportals"}, "the probe left nothing in either folder: " + Joined(Tree(second)));
    CHECK_MSG(chosen.passedOver.size() == 1 && chosen.passedOver[0].find("under a file") != std::string::npos, Joined(chosen.passedOver));

    // The prism folder is a regular file (another account's, in a shared folder): the folder itself passes, the prism folder does not.
    const fs::path shared = Folder("shared");
    Touch(shared / Scratch::kPrismFolder);
    const Scratch::Chosen sharedChosen =
        Scratch::Choose({At("shared", shared.generic_string()), At("own", (root / "own").generic_string(), nullptr, true)}, Scratch::ProbeWritable);
    CHECK_MSG(sharedChosen.ok && sharedChosen.index == 1, sharedChosen.note);
}

// ---- the prism's half-written file ----------------------------------------------------------------------------------------------

// Two windows, or two accounts on a shared folder, building one level wrote ONE fixed "<prism>.partial". Something already standing at that
// name (a folder here; a half-written file of another process in life) stopped every later prism, and a level then refused to start.
// MUTATION: the half-written copy named "<prism>.partial" again; the half-written copy left behind.
void APrismIsWrittenWhateverStandsAtTheOldFixedPartialName() {
    const fs::path dir = Folder("prisms");
    const std::vector<double> triangle = {0, -30, 30, 30, -30, 30};
    std::string error;
    const std::string first = Prism::Write(triangle, 1.0, dir, error);
    CHECK_MSG(!first.empty(), error);
    std::error_code ec;
    fs::remove(first, ec);
    fs::create_directories(first + ".partial", ec); // the old fixed name, held
    const std::string again = Prism::Write(triangle, 1.0, dir, error);
    CHECK_MSG(again == first && fs::is_regular_file(again, ec), "the prism is written although the old fixed name is taken: " + error);
    std::vector<std::string> left;
    for (const std::string& name : Tree(dir)) {
        if (name.size() > 8 && name.compare(name.size() - 8, 8, ".partial") == 0 && name != fs::path(first).filename().string() + ".partial") {
            left.push_back(name);
        }
    }
    CHECK_MSG(left.empty(), "and no half-written copy of its own is left behind: " + Joined(left));
}

// PORT2-3 (a), the part that can be pinned (planning step 203): a prism whose own name is held by a non-empty folder cannot be moved into place. The
// half-written copy is removed and the failure is the writer's return, not an exception or a silent success. (The twin claim, that a failed WRITE
// removes the copy, is not pinned: a write that fails after the open succeeded needs a full disk or a fake file system, and the branch is three lines
// read, not run. The Step says so.)
// MUTATION: the half-written copy left behind after a refused rename; the refusal ignored (the path returned all the same).
void APrismWhoseNameIsHeldByAFolderFailsAndLeavesNoHalfFile() {
    const fs::path dir = Folder("prism-held");
    const std::vector<double> triangle = {0, -30, 30, 30, -30, 30};
    std::string error;
    const std::string first = Prism::Write(triangle, 1.0, dir, error);
    CHECK_MSG(!first.empty(), error);
    std::error_code ec;
    fs::remove(first, ec);
    fs::create_directories(fs::path(first) / "inner", ec); // a non-empty folder at the prism's own name: no file can be renamed over it
    error.clear();
    const std::string again = Prism::Write(triangle, 1.0, dir, error);
    CHECK_MSG(again.empty(), "a refused move is a failure, not a path: '" + again + "'");
    CHECK_MSG(error.find("cannot move") != std::string::npos, "and it says what it could not do: " + error);
    std::vector<std::string> left;
    for (const std::string& name : Tree(dir)) {
        if (name.size() > 8 && name.compare(name.size() - 8, 8, ".partial") == 0) left.push_back(name);
    }
    CHECK_MSG(left.empty(), "and the half-written copy is gone: " + Joined(left));
}

// PORT2-3 (a), the probe's file: it is a name of its own, so it never takes, overwrites or removes a file that somebody else's probe (or anything)
// left in the folder.
// MUTATION: ProbeWritable writing a fixed file name (".write-probe.tmp" or any of the others here): it would overwrite and then remove that file.
void TheProbeNeverTouchesAFileItDidNotMake() {
    const fs::path root = Folder("probe-name");
    std::error_code ec;
    fs::create_directories(root, ec);
    const std::vector<std::string> names = {".write-probe.tmp", ".write-probe", "write-probe.tmp", ".probe", "probe.tmp", ".write-probe-.tmp", "probe"};
    for (const std::string& name : names) {
        std::ofstream(root / name, std::ios::binary) << "somebody else's";
    }
    std::string why;
    CHECK_MSG(Scratch::ProbeWritable(root, why), why);
    bool whole = true;
    for (const std::string& name : names) {
        std::ifstream in(root / name, std::ios::binary);
        std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        whole = whole && text == "somebody else's";
    }
    CHECK_MSG(whole, "every file that was there is as it was");
    CHECK_MSG(Tree(root).size() == names.size(), "and the probe left none of its own: " + Joined(Tree(root)));
}

// The phone's order (planning step 203): the user-data scratch folder first, the rest as they were; a desktop keeps main's order.
// MUTATION: ArrangedFor that ignores `phone`, that always moves, that reverses, that drops the others, that sorts by label or moves the wrong kind.
void APhonePutsTheUserDataScratchFolderFirstAndADesktopChangesNothing() {
    std::vector<Scratch::Candidate> given = {At("temp", "/t"), At("user scratch", "/u/scratch", nullptr, true), At("user", "/u", nullptr, true), At("exe", "/x"),
                                             At("cwd", "/w")};
    given[1].kind = Scratch::Kind::UserDataScratch;
    const auto labels = [](const std::vector<Scratch::Candidate>& list) {
        std::vector<std::string> out;
        for (const Scratch::Candidate& candidate : list) out.push_back(candidate.label);
        return out;
    };
    CHECK_MSG(labels(Scratch::ArrangedFor(given, false)) == (std::vector<std::string>{"temp", "user scratch", "user", "exe", "cwd"}), "a desktop keeps the order it was given");
    CHECK_MSG(labels(Scratch::ArrangedFor(given, true)) == (std::vector<std::string>{"user scratch", "temp", "user", "exe", "cwd"}),
              "a phone puts the scratch folder first and keeps the others in their order: " + Joined(labels(Scratch::ArrangedFor(given, true))));
    // Nothing marked: nothing moves, on a phone too (the other candidates are not reordered by a rule that does not apply).
    std::vector<Scratch::Candidate> unmarked = given;
    unmarked[1].kind = Scratch::Kind::Other;
    CHECK_MSG(labels(Scratch::ArrangedFor(unmarked, true)) == labels(unmarked), "no candidate of the kind, no move");
    // Two of the kind keep their own order, both ahead.
    std::vector<Scratch::Candidate> two = given;
    two[3].kind = Scratch::Kind::UserDataScratch;
    CHECK_MSG(labels(Scratch::ArrangedFor(two, true)) == (std::vector<std::string>{"user scratch", "exe", "temp", "user", "cwd"}), "two of the kind: stable");
    CHECK_MSG(Scratch::ArrangedFor({}, true).empty() && Scratch::ArrangedFor({}, false).empty(), "an empty list stays empty");
    // The effect: a phone whose temporary folder IS writable still logs in its own scratch folder; a desktop logs in the temporary one.
    FakeProbe accepting;
    CHECK_MSG(Scratch::Choose(Scratch::ArrangedFor(given, true), accepting.Fn()).label == "user scratch", "a phone logs in its own folder though /t is writable");
    FakeProbe again;
    CHECK_MSG(Scratch::Choose(Scratch::ArrangedFor(given, false), again.Fn()).label == "temp", "a desktop still logs in the temporary folder");
    // And when the scratch folder cannot be made, the phone falls back to the rest, in their order.
    FakeProbe refusing;
    refusing.failing = {"/u/scratch"};
    const Scratch::Chosen fell = Scratch::Choose(Scratch::ArrangedFor(given, true), refusing.Fn());
    CHECK_MSG(fell.ok && fell.label == "temp", "a refused scratch folder falls to the next: " + fell.note);
    // The names that the Android manifest quotes.
    CHECK_MSG(std::string(Scratch::kUserScratchFolder) == "scratch" && std::string(Scratch::kLogFileName) == "magicportals.log", "the pieces of the log's path on a phone");
}

// The last run's log kept beside the log (Scratch::KeepPreviousLog; main.cpp's own function until planning step 204, when the Android entry needed the same
// rotation). MUTATION: delete the exists test (a missing log is then "kept" and an error), copy instead of rename, keep the log in place, ignore a failed copy,
// leave the previous log when the new one is moved.
void TheLastLogIsKeptBesideTheLogAndAnImpossibleKeepIsSaidNotThrown() {
    const fs::path dir = Folder("keep-previous-log");
    const fs::path log = dir / "magicportals.log";
    const fs::path previous = dir / "magicportals.prev.log";
    std::string why;
    CHECK_MSG(Scratch::KeepPreviousLog(log, previous, why) && why.empty() && Tree(dir).empty(), "no log, nothing to keep, nothing made");
    std::ofstream(log, std::ios::binary) << "last run";
    CHECK_MSG(Scratch::KeepPreviousLog(log, previous, why), why);
    CHECK_MSG(!fs::exists(log) && fs::exists(previous), "the log was moved");
    std::ifstream kept(previous, std::ios::binary);
    std::string text((std::istreambuf_iterator<char>(kept)), std::istreambuf_iterator<char>());
    CHECK_MSG(text == "last run", "with its bytes: " + text);
    kept.close();
    // The previous one is replaced by the next last log.
    std::ofstream(log, std::ios::binary) << "the run after";
    CHECK_MSG(Scratch::KeepPreviousLog(log, previous, why), why);
    std::ifstream again(previous, std::ios::binary);
    CHECK_MSG(std::string((std::istreambuf_iterator<char>(again)), std::istreambuf_iterator<char>()) == "the run after", "the earlier previous log is replaced");
    again.close();
    // A previous log that cannot be written (a non-empty folder stands at its name): said, the log stays where it was, and nothing throws.
    std::ofstream(log, std::ios::binary) << "x";
    const fs::path blocked = dir / "blocked.prev.log";
    fs::create_directories(blocked / "occupied");
    why.clear();
    CHECK_MSG(!Scratch::KeepPreviousLog(log, blocked, why), "a previous log that cannot be written is a false");
    CHECK_MSG(!why.empty() && fs::exists(log), "with a reason, and the log is still there");
    CHECK_MSG(std::string(Scratch::kPreviousLogFileName) == "magicportals.prev.log", "the name main.cpp has always used");
}

// WHERE THE LOG IS, one definition for main.cpp and the Android entry (planning step 205, PORT6-8 (b)): main opens the log in the folder the candidates chose, and
// AndroidMain writes the words of a start that stopped in the unpack into the same file, before main can run. The post-mortem report reads ONE path the manifest
// names, so the two must never spell it differently. MUTATION: LogPathsIn swapping the two names, or dropping the previous log's; PhoneLogFolder joining another
// folder name, or joining onto an empty base (a log at "scratch" in the working directory).
void TheLogsPathsAreOneDefinitionForTheDesktopMainAndThePhonesFailedStart() {
    const fs::path folder = fs::path("/data/user/0/app/files") / "Magic Portals" / "scratch";
    const Scratch::LogPaths logs = Scratch::LogPathsIn(folder);
    CHECK_MSG(logs.log == folder / "magicportals.log", "the log: " + logs.log.string());
    CHECK_MSG(logs.previous == folder / "magicportals.prev.log", "and the run before's: " + logs.previous.string());
    CHECK_MSG(logs.log.parent_path() == logs.previous.parent_path() && logs.log != logs.previous, "beside each other, and two files");
    // The phone's folder is the user's data folder's scratch folder, the manifest's supersonic.reportLog path.
    const fs::path userData = fs::path("/data/user/0/app/files") / "Magic Portals";
    CHECK_MSG(Scratch::PhoneLogFolder(userData) == userData / "scratch", "the phone's log folder: " + Scratch::PhoneLogFolder(userData).string());
    CHECK_MSG(Scratch::LogPathsIn(Scratch::PhoneLogFolder(userData)).log == userData / "scratch" / "magicportals.log",
              "so the phone's log is <user data>/scratch/magicportals.log, the path the manifest names");
    CHECK_MSG(Scratch::PhoneLogFolder(fs::path()).empty(), "no user data folder is no log folder (a name is never joined onto an empty path)");
    // The effect through the candidate machinery: the user-data scratch candidate main.cpp builds with PhoneLogFolder is chosen on a phone, and the log the
    // failed-start record writes is in the very folder that choice names.
    std::vector<Scratch::Candidate> given;
    given.push_back({"temp", [] { return fs::path("/t"); }, false, Scratch::Kind::Other});
    given.push_back({"user scratch", [&userData] { return Scratch::PhoneLogFolder(userData); }, true, Scratch::Kind::UserDataScratch});
    FakeProbe accepting;
    const Scratch::Chosen phone = Scratch::Choose(Scratch::ArrangedFor(given, true), accepting.Fn());
    CHECK_MSG(phone.ok && Scratch::LogPathsIn(phone.base).log == Scratch::LogPathsIn(Scratch::PhoneLogFolder(userData)).log,
              "a phone's chosen log folder is the failed-start record's: " + phone.base.string());
}

// MUTATION: UniqueSuffix returning a constant.
void TwoSuffixesAreNeverTheSame() {
    std::set<std::string> seen;
    for (int i = 0; i < 64; ++i) seen.insert(Files::UniqueSuffix());
    CHECK_MSG(seen.size() == 64, std::to_string(seen.size()) + " different in 64");
}

void runTests() {
    TheTwoLayoutsAreTheRepositoryAndThePackage();
    TheLanguageFilesAreWhereBothLayoutsKeepThem();
    NormalizeSpellsAPathOut();
    WhatIsAbsoluteIsJudgedOnTheTextAlone();
    JoinPutsARelativePathUnderAFolder();
    TheFlagBeatsEveryOtherTier();
    AFlagThatDoesNotHoldTheGameIsAnErrorNotAFallThrough();
    NextToTheExecutableIsContentFirstThenTheFolderItself();
    TheBundleComesBeforeTheUsersDataFolderWhichComesBeforeTheBuild();
    TheUsersDataFolderIsAskedForOnlyWhenNothingBeforeItAnswered();
    BothShapesAreTriedPackagedFirst();
    TheBuildsOwnPathsAreTheLastResort();
    NothingBakedAndNoRootIsAnErrorThatSaysWhereToLook();
    EachTierSaysItselfInOneLine();
    ARootIsSpelledOutBeforeItIsUsed();
    EachPathFlagReplacesItsOwnPathAndNoOther();
    LevelsBringTheirArtWithThemUnlessArtIsGiven();
    ResolvingAgainstARealTreeCreatesNothing();
    TheFirstFolderThatCanBeWrittenWins();
    ALaterCandidateIsNeverEvaluatedUnlessEarlierOnesFailed();
    AFolderWhosePrismFolderIsRefusedIsPassedOver();
    NothingUsableIsSaidAndNothingThrows();
    TheRealProbeMakesTheFolderTriesAFileAndLeavesNothingBehind();
    AnUnwritableFirstFolderFallsToTheSecondAndOnlyThenIsItMade();
    APrismIsWrittenWhateverStandsAtTheOldFixedPartialName();
    APrismWhoseNameIsHeldByAFolderFailsAndLeavesNoHalfFile();
    TheProbeNeverTouchesAFileItDidNotMake();
    APhonePutsTheUserDataScratchFolderFirstAndADesktopChangesNothing();
    TheLastLogIsKeptBesideTheLogAndAnImpossibleKeepIsSaidNotThrown();
    TheLogsPathsAreOneDefinitionForTheDesktopMainAndThePhonesFailedStart();
    TwoSuffixesAreNeverTheSame();
}

} // namespace

MP_TEST_MAIN("test_mp_assetroot", 200)
