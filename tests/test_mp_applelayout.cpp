// Where the game's data is in an Apple bundle, and what the macOS and iOS entries do with the command line (planning step 206, PORT-8): sim/AppleLayout.hpp.
//
// WHY A SUITE. game/macos/MacMain.mm and game/ios/IOSMain.mm are Objective-C++ that no machine of this project can compile or run, and the Windows and Linux
// builds do not contain them. Every decision they make is therefore a pure function of strings in AppleLayout.hpp, and this suite is the only thing here that
// ever runs those decisions. It pins the two layouts, the order they are looked in, what "holds the game" means (tied to the game's own packaged shape), the
// arguments the game is handed and the words of a start that found nothing. It says NOTHING about a Mac, an iPhone or an iPad: that an entry finds a folder
// on a real device is NOT KNOWN.
//
// The suite NEVER skips, reads no converted level and writes nothing: the "file system" is a set of invented names, and the few ties to other files read
// them (game/main.cpp, game/android/AndroidMain.cpp) through MAGICPORTALS_SOURCE_DIR, a definition of this source alone (tests/CMakeLists.txt).
//
// THE MUTATION EACH PIN FAILS UNDER is written beside it ("MUTATION:"). They were each run on a scratch copy of AppleLayout.hpp (the step's record lists the
// runs); a pin with no mutation would be a check that passes whatever the code does.

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "sim/AppleLayout.hpp"
#include "sim/AssetRoot.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#ifndef MAGICPORTALS_SOURCE_DIR
#error "test_mp_applelayout.cpp is given MAGICPORTALS_SOURCE_DIR by tests/CMakeLists.txt"
#endif

namespace AL = MagicPortals::AppleLayout;
namespace AR = MagicPortals::AssetRoot;

namespace {

using Args = std::vector<std::string>;

// Both sides evaluated once, shown in the failure; the harness's CHECK_EQ prints through std::to_string and so takes numbers only.
#define CHECK_STR(a, b)                                                                       \
    do {                                                                                      \
        const std::string _checkA = (a);                                                      \
        const std::string _checkB = (b);                                                      \
        CHECK_MSG(_checkA == _checkB, "got '" + _checkA + "', expected '" + _checkB + "'");   \
    } while (false)

// A "file system" that is a set of paths, and a record of every question put to it.
struct FakeDisk {
    std::set<std::string> have;
    mutable std::vector<std::string> asked;
    AL::Exists Fn() const {
        return [this](const std::string& path) {
            asked.push_back(path);
            return have.count(path) != 0;
        };
    }
    // A GameData folder as the packaged shape lays it out: its levels folder and its chapters table.
    void Holds(const std::string& folder) {
        have.insert(folder + "/levels");
        have.insert(folder + "/tables/chapters.json");
    }
};

std::string Joined(const std::vector<std::string>& v) {
    std::string s;
    for (const std::string& item : v) s += (s.empty() ? "" : "; ") + item;
    return s.empty() ? "(none)" : s;
}

std::string ReadSource(const std::string& relative) {
    std::ifstream in(std::string(MAGICPORTALS_SOURCE_DIR) + "/" + relative, std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

// The string literal a source file gives a name: the text between the quotes after `name = ` on its line, or "" where the line is not there.
std::string LiteralOf(const std::string& source, const std::string& name) {
    const std::size_t at = source.find(name + " = \"");
    if (at == std::string::npos) return {};
    const std::size_t from = at + name.size() + 4;
    const std::size_t to = source.find('"', from);
    return to == std::string::npos ? std::string() : source.substr(from, to - from);
}

// ---- paths --------------------------------------------------------------------------------------------------------------------------------

// MUTATION: Join without its separator rule (a doubled or a missing '/'); an empty side not giving the other.
void JoinPutsOneSeparatorBetweenTwoNames() {
    CHECK_STR(AL::Join("/a", "b"), std::string("/a/b"));
    CHECK_STR(AL::Join("/a/", "b"), std::string("/a/b"));
    CHECK_STR(AL::Join("/a", "/b"), std::string("/a/b"));
    CHECK_STR(AL::Join("/a/", "/b"), std::string("/a/b"));
    CHECK_STR(AL::Join("", "b"), std::string("b"));
    CHECK_STR(AL::Join("/a", ""), std::string("/a"));
    CHECK_STR(AL::Join("", ""), std::string());
}

// MUTATION: the iOS answer given a bundle id (the folder would then differ from the entry's); the macOS answer without the id; an empty caches folder
// answered with a relative path; the fallback id dropped.
void TheEnginesWorkingFolderIsBelowTheCaches() {
    CHECK_STR(AL::EngineRoot(AL::Platform::MacOS, "/Users/x/Library/Caches", "com.example.app"), std::string("/Users/x/Library/Caches/com.example.app/engine"));
    CHECK_STR(AL::EngineRoot(AL::Platform::MacOS, "/c", ""), std::string("/c/MagicPortals/engine"));
    CHECK_STR(AL::EngineRoot(AL::Platform::IOS, "/var/mobile/Library/Caches", "com.example.app"), std::string("/var/mobile/Library/Caches/engine"));
    CHECK_STR(AL::EngineRoot(AL::Platform::MacOS, "", "x"), std::string());
    CHECK_STR(AL::EngineRoot(AL::Platform::IOS, "", "x"), std::string());
}

// ---- the two layouts ------------------------------------------------------------------------------------------------------------------------

// MUTATION: the two macOS candidates swapped; the Application Support folder spelled without the game's title; the bundle candidate spelled
// Contents/GameData (a folder CodeSign would not seal as a resource); a third candidate added (the folder BESIDE the app, which App Translocation
// hides).
void MacOSLooksInsideTheAppFirstAndThenInTheUsersApplicationSupport() {
    const std::vector<AL::Candidate> found = AL::Candidates(AL::Platform::MacOS, "/Applications/Magic Portals.app/Contents/Resources", "/Users/x/Library/Application Support");
    CHECK_EQ(found.size(), std::size_t(2));
    if (found.size() != 2) return;
    CHECK_STR(found[0].folder, std::string("/Applications/Magic Portals.app/Contents/Resources/GameData"));
    CHECK_STR(found[1].folder, std::string("/Users/x/Library/Application Support/Magic Portals/GameData"));
    CHECK_MSG(found[0].label != found[1].label && !found[0].label.empty() && !found[1].label.empty(), "each candidate has its own words for a failure");
}

// MUTATION: the iOS list given the Application Support candidate (an iOS app has none to drop a folder into); the iOS folder spelled Resources/GameData;
// the user's folder honoured on iOS.
void IOSLooksOnlyInsideTheBundle() {
    const std::vector<AL::Candidate> found = AL::Candidates(AL::Platform::IOS, "/var/containers/Bundle/Application/ID/Magic Portals.app", "/var/mobile/Library/Application Support");
    CHECK_EQ(found.size(), std::size_t(1));
    if (found.size() != 1) return;
    CHECK_STR(found[0].folder, std::string("/var/containers/Bundle/Application/ID/Magic Portals.app/GameData"));
}

// MUTATION: an empty bundle folder joined into "/GameData" (the file system's root); an empty user folder joined into "Magic Portals/GameData" (relative to
// the working directory, which the entry has just moved).
void AFolderTheSystemDidNotNameIsNotALookup() {
    CHECK_EQ(AL::Candidates(AL::Platform::MacOS, "", "").size(), std::size_t(0));
    const std::vector<AL::Candidate> noSupport = AL::Candidates(AL::Platform::MacOS, "/r", "");
    CHECK_EQ(noSupport.size(), std::size_t(1));
    const std::vector<AL::Candidate> noBundle = AL::Candidates(AL::Platform::MacOS, "", "/s");
    CHECK_EQ(noBundle.size(), std::size_t(1));
    if (noBundle.size() == 1) CHECK_STR(noBundle[0].folder, std::string("/s/Magic Portals/GameData"));
    CHECK_EQ(AL::Candidates(AL::Platform::IOS, "", "/s").size(), std::size_t(0));
}

// MUTATION: kGameDataFolder renamed; kTitle changed; the user folder's name not the title.
void TheNamesAreTheOnesTheDocsAndThePackagerUse() {
    CHECK_STR(std::string(AL::kGameDataFolder), std::string("GameData"));
    // Tied to the Android entry's own content folder (the same packaged folder, the same name), so a bundle's and an APK's data are one thing.
    CHECK_STR(LiteralOf(ReadSource("game/android/AndroidMain.cpp"), "constexpr const char* kContentFolder"), std::string(AL::kGameDataFolder));
    // And the title is main.cpp's, the folder the saves are in.
    const std::string title = LiteralOf(ReadSource("game/main.cpp"), "constexpr const char* kTitle");
    CHECK_MSG(!title.empty() && title == AL::kTitle, "main.cpp's kTitle is '" + title + "', this header's is '" + AL::kTitle + "'");
}

// ---- what holds the game ---------------------------------------------------------------------------------------------------------------------

// MUTATION: a marker renamed; Packaged() given another levels or chapters name (the entry would call a folder the game refuses a data folder);
// either marker dropped.
void HoldingTheGameMeansWhatThePackagedShapeSays() {
    const AR::Layout packaged = AR::Layout::Packaged();
    CHECK_STR(std::string(AL::kLevelsMarker), packaged.levels);
    CHECK_STR(std::string(AL::kChaptersMarker), packaged.chapters);
}

// MUTATION: HoldsGame satisfied by one of the two; by an empty folder; by a missing predicate.
void AFolderHoldsTheGameOnlyWithTheLevelsAndTheChaptersTable() {
    FakeDisk disk;
    CHECK_MSG(!AL::HoldsGame("/g", disk.Fn()), "nothing there");
    disk.have.insert("/g/levels");
    CHECK_MSG(!AL::HoldsGame("/g", disk.Fn()), "levels alone are not the game");
    disk.have.clear();
    disk.have.insert("/g/tables/chapters.json");
    CHECK_MSG(!AL::HoldsGame("/g", disk.Fn()), "the chapters table alone is not the game");
    disk.Holds("/g");
    CHECK_MSG(AL::HoldsGame("/g", disk.Fn()), "both are");
    CHECK_MSG(!AL::HoldsGame("", disk.Fn()), "an empty folder is no folder");
    // Joined onto nothing the markers would be RELATIVE paths, which the working directory may hold: an empty folder must still not be taken for the game.
    FakeDisk working;
    working.have.insert("levels");
    working.have.insert("tables/chapters.json");
    CHECK_MSG(!AL::HoldsGame("", working.Fn()) && working.asked.empty(), "an empty folder is refused before any question is put to the machine");
    CHECK_MSG(!AL::HoldsGame("/g", AL::Exists{}), "no way to ask is no");
}

// MUTATION: Choose taking the LAST holder; not stopping at the first (the user's folder asked about after the bundle's won); skipping the bundle.
void TheFirstFolderThatHoldsTheGameWinsAndTheRestAreNotAsked() {
    const std::vector<AL::Candidate> mac = AL::Candidates(AL::Platform::MacOS, "/app/Contents/Resources", "/support");
    FakeDisk both;
    both.Holds("/app/Contents/Resources/GameData");
    both.Holds("/support/Magic Portals/GameData");
    const AL::Choice first = AL::Choose(mac, both.Fn());
    CHECK_MSG(first.found && first.folder == "/app/Contents/Resources/GameData", "inside the app wins when both hold the game: " + first.folder);
    CHECK_MSG(first.tried.size() == 1, "and the second is never asked about: " + Joined(first.tried));
    for (const std::string& path : both.asked) {
        CHECK_MSG(path.rfind("/support", 0) != 0, "a path of the user's folder was asked about: " + path);
    }

    FakeDisk userOnly;
    userOnly.Holds("/support/Magic Portals/GameData");
    const AL::Choice second = AL::Choose(mac, userOnly.Fn());
    CHECK_MSG(second.found && second.folder == "/support/Magic Portals/GameData", "the user's folder when the bundle holds nothing: " + second.folder);
    CHECK_MSG(second.tried.size() == 2, "both were asked about: " + Joined(second.tried));

    // A bundle folder with only levels in it is passed over, not taken.
    FakeDisk halfAndUser;
    halfAndUser.have.insert("/app/Contents/Resources/GameData/levels");
    halfAndUser.Holds("/support/Magic Portals/GameData");
    CHECK_MSG(AL::Choose(mac, halfAndUser.Fn()).folder == "/support/Magic Portals/GameData", "a half-made bundle folder does not win");
}

// MUTATION: found true with nothing; `tried` not listing the candidates that were asked; the words not naming each.
void NothingFoundIsSaidInWordsThatNameEveryPlaceLookedIn() {
    FakeDisk empty;
    const std::vector<AL::Candidate> mac = AL::Candidates(AL::Platform::MacOS, "/app/Contents/Resources", "/support");
    const AL::Choice none = AL::Choose(mac, empty.Fn());
    CHECK_MSG(!none.found && none.folder.empty(), "nothing found");
    CHECK_EQ(none.tried.size(), std::size_t(2));
    const std::string words = AL::NoGameDataText(AL::Platform::MacOS, none.tried);
    CHECK_MSG(words.find("/app/Contents/Resources/GameData") != std::string::npos && words.find("/support/Magic Portals/GameData") != std::string::npos,
              "the words name both places: " + words);
    CHECK_MSG(words.find("cannot start") != std::string::npos && words.find("did not find the game's data") != std::string::npos, "and say what happened: " + words);
    // iOS says the data is meant to be part of the app and that this copy was made without it, and does not send the player to a folder to fill.
    const std::string ios = AL::NoGameDataText(AL::Platform::IOS, {"inside the app: /b/GameData"});
    CHECK_MSG(ios.find("made without them") != std::string::npos && ios.find("you put") == std::string::npos, "iOS words: " + ios);
    CHECK_MSG(words.find("you put") != std::string::npos, "macOS words tell where a data folder may be put");
    // Nowhere to look at all is still a sentence, not an empty list.
    CHECK_MSG(AL::NoGameDataText(AL::Platform::IOS, {}).find("nowhere") != std::string::npos, "the system named no folder");
    // And none of it names the program's maker, a person or a path of anyone's machine.
    for (const std::string& text : {words, ios}) CHECK_MSG(text.find("/Users/") == std::string::npos && text.find("icvet") == std::string::npos, "the words hold no account name");
}

// MUTATION: Choose calling a second predicate; the suite's FakeDisk records every question, and a candidate list that is empty must ask none.
void TheLookupAsksOnlyWhetherAPathExists() {
    FakeDisk disk;
    disk.Holds("/x/GameData");
    const std::vector<AL::Candidate> one = {{"inside the app", "/x/GameData"}};
    CHECK_MSG(AL::Choose(one, disk.Fn()).found, "found");
    CHECK_EQ(disk.asked.size(), std::size_t(2)); // exactly the two markers, in this order
    if (disk.asked.size() == 2) CHECK_MSG(disk.asked[0] == "/x/GameData/levels" && disk.asked[1] == "/x/GameData/tables/chapters.json", Joined(disk.asked));
    FakeDisk idle;
    CHECK_MSG(!AL::Choose({}, idle.Fn()).found && idle.asked.empty(), "no candidate, no question");
}

// ---- the arguments ---------------------------------------------------------------------------------------------------------------------------------

// MUTATION: "--root" inserted at the end; before argv[0]; twice; GivesRoot true for a value that spells "--root"; GivesRoot never true (a caller's own
// --root would then be doubled, which the game refuses: "--root is given twice").
void TheChosenFolderIsHandedOnThroughTheRootFlagAfterTheProgramsName() {
    CHECK_STR(Joined(AL::WithRoot({"/app/MagicPortals", "--level", "ci_level"}, "/g")), std::string("/app/MagicPortals; --root; /g; --level; ci_level"));
    CHECK_STR(Joined(AL::WithRoot({"/app/MagicPortals"}, "/g")), std::string("/app/MagicPortals; --root; /g"));
    CHECK_STR(Joined(AL::WithRoot({}, "/g")), std::string("MagicPortals; --root; /g"));
    // The caller's own --root is the player's (or CI's) word: nothing is added, nothing is looked up.
    const Args given = {"/app/MagicPortals", "--root", "/mine", "--level", "x"};
    CHECK_MSG(AL::GivesRoot(given) && AL::WithRoot(given, "/g") == given, "a --root given is kept, and only once");
    CHECK_MSG(!AL::GivesRoot({"/app/MagicPortals", "--saves", "--root"}), "a value that spells --root is a value, not the flag");
    CHECK_MSG(!AL::GivesRoot({"/app/MagicPortals"}) && !AL::GivesRoot({}), "no flag");
    CHECK_MSG(AL::WithRoot({"/p"}, "") == Args{"/p"}, "an empty root adds nothing");
}

// MUTATION: a flag removed from kPathFlags; a value made absolute that was already; a relative one left alone; the program's name treated as a flag;
// a relative value left alone. (Taking the `++i` after a value out of MakeAbsolute changes nothing anyone can observe: the value was rewritten to an absolute path before the next turn reads it, so
// it can no longer spell a flag. That mutant is EQUIVALENT and stays green; the line is kept as a guard, and the two cases below pin the behaviour it guards.)
void APathTypedOnTheCommandLineKeepsTheMeaningTheShellGaveIt() {
    const Args in = {"/app/MagicPortals", "--level", "ci_level", "--screenshot", "out/a.png", "--root", "fixtures/ci", "--saves", "/abs/saves",
                     "--data", "", "--frames", "300"};
    const Args out = AL::MakeAbsolute(in, "/work/dir");
    CHECK_STR(Joined(out), std::string("/app/MagicPortals; --level; ci_level; --screenshot; /work/dir/out/a.png; --root; /work/dir/fixtures/ci; --saves; /abs/saves; --data; ; "
                                      "--frames; 300"));
    CHECK_MSG(AL::MakeAbsolute(in, "") == in, "no launch directory, nothing changed");
    // Every flag the game and a capture give a path to.
    for (const char* flag : {"--root", "--levels", "--art", "--data", "--original", "--chapters", "--achievements", "--port-data", "--padart", "--saves",
                             "--screenshot"}) {
        CHECK_MSG(AL::IsPathFlag(flag), std::string(flag) + " takes a path");
        const Args one = AL::MakeAbsolute({"p", flag, "rel"}, "/w");
        CHECK_MSG(one.size() == 3 && one[2] == "/w/rel", std::string(flag) + " is made absolute");
    }
    // The ones that take a number, a name or nothing are not touched.
    for (const char* flag : {"--level", "--frames", "--window", "--controls", "--profile", "--fixed-step"}) {
        CHECK_MSG(!AL::IsPathFlag(flag), std::string(flag) + " takes no path");
    }
    // A flag's value is not read as a flag: "--saves --root" is a saves folder called "--root" (relative, so made absolute).
    CHECK_STR(Joined(AL::MakeAbsolute({"p", "--saves", "--root"}, "/w")), std::string("p; --saves; /w/--root"));
    // ... and the word after it is then not that flag's value: "rel" here belongs to nobody and stays as typed.
    CHECK_STR(Joined(AL::MakeAbsolute({"p", "--saves", "--root", "rel"}, "/w")), std::string("p; --saves; /w/--root; rel"));
}

// MUTATION: a flag removed from kUnattendedFlags (each one is checked); the program's name counted; a value counted as a flag is not what this reads
// (a flag is any element, so the table's only job is the NAMES).
void OnlyAnAttendedRunMayOpenADialog() {
    CHECK_MSG(!AL::Unattended({"/app/MagicPortals"}), "a double-click: the player is there");
    CHECK_MSG(!AL::Unattended({"/app/MagicPortals", "--level", "level3", "--window", "1280x720", "--controls", "modern"}), "player flags keep the dialog");
    for (const char* flag : {"--frames", "--fixed-step", "--hidden", "--screenshot", "--screenshot-ui", "--screenshot-every", "--record", "--replay",
                             "--import-assets", "--saves", "--route", "--route-trace", "--visit-levels", "--press", "--hold", "--tap", "--drag", "--burn-dragon",
                             "--place", "--light-masks-off"}) {
        CHECK_MSG(AL::Unattended({"/app/MagicPortals", flag}), std::string(flag) + " is a script's flag: nobody is there to close a dialog");
    }
    // The CI fixture run is the case that matters: a runner nobody watches must never wait on a dialog.
    CHECK_MSG(AL::Unattended({"/app/MagicPortals", "--root", "/f", "--level", "ci_level", "--window", "1280x720", "--fixed-step", "--frames", "300", "--screenshot", "/o.png"}),
              "the fixture run is unattended");
    // Mirrors RunKind.hpp's rule: every engine flag that makes AttendedRun false is here. The text tie: each of the engine's capture and replay flags
    // appears in RunKind's own list of what it reads (the fields), so a flag added there is noticed here.
    const std::string runKind = ReadSource("game/RunKind.hpp");
    for (const char* field : {"engine.maxFrames", "engine.fixedDelta", "engine.hidden", "engine.screenshotPath", "engine.screenshotUiPath", "engine.screenshotEvery",
                              "engine.recordPath", "engine.replayPath", "engine.importAssets", "dev.saves", "dev.route", "dev.visit", "dev.scripted", "dev.lightMasksOff"}) {
        CHECK_MSG(runKind.find(field) != std::string::npos, std::string("RunKind.hpp no longer reads ") + field + ": this table must follow it");
    }
}

// ---- ties to the rest of the repository --------------------------------------------------------------------------------------------------------------

// MUTATION: kFlagsFile or kFlagsUsed renamed in this header (the Android entry's names are the text read here).
void TheFlagsFileHasTheAndroidEntrysNames() {
    const std::string android = ReadSource("game/android/AndroidMain.cpp");
    CHECK_STR(LiteralOf(android, "constexpr const char* kArgsFile"), std::string(AL::kFlagsFile));
    CHECK_STR(LiteralOf(android, "constexpr const char* kArgsUsed"), std::string(AL::kFlagsUsed));
}

// MUTATION: none that this header could carry: it is a guard on the files that CONSUME the header. Each Apple entry must go through it for the three
// decisions that are pinned above, and must not reach for a lookup of its own.
void TheEntriesUseTheHeaderAndNoRuleOfTheirOwn() {
    const std::string mac = ReadSource("game/macos/MacMain.mm");
    const std::string ios = ReadSource("game/ios/IOSMain.mm");
    CHECK_MSG(!mac.empty() && !ios.empty(), "both entries are in the tree");
    for (const std::string* entry : {&mac, &ios}) {
        CHECK_MSG(entry->find("sim/AppleLayout.hpp") != std::string::npos, "an entry includes sim/AppleLayout.hpp");
        CHECK_MSG(entry->find("AppleLayout::Candidates") != std::string::npos && entry->find("AppleLayout::Choose") != std::string::npos &&
                      entry->find("AppleLayout::WithRoot") != std::string::npos && entry->find("AppleLayout::NoGameDataText") != std::string::npos,
                  "an entry asks the header for the candidates, the choice, the arguments and the words");
        CHECK_MSG(entry->find("MagicPortalsMain(") != std::string::npos, "an entry ends in the shared entry");
        // The entries hand on the folder by --root and do not touch the resolver or main.cpp's internals.
        CHECK_MSG(entry->find("AssetRoot.hpp") == std::string::npos && entry->find("AssetRoot::") == std::string::npos, "an entry does not call into AssetRoot");
        CHECK_MSG(entry->find("getenv") == std::string::npos, "an entry reads no environment variable");
    }
    CHECK_MSG(mac.find("AppleLayout::Unattended") != std::string::npos, "the macOS entry decides its dialog by the pure rule");
    CHECK_MSG(ios.find("Unpack::TakeFlagsFile(") != std::string::npos && ios.find("AppleLayout::kFlagsFile") != std::string::npos,
              "the iOS entry reads its flags through Unpack::TakeFlagsFile, the consume-once rule the Android entry uses");
}

// MUTATION: main.cpp's guard given __APPLE__ without the rename's removal (this fails and says delete the header); the header's macro made object-like (it then renames a member of
// MagicPortalsLayer.hpp too); the CMake line dropped, or made unconditional (then the Windows program's main.cpp would lose its main).
void TheDesktopMainIsRenamedAwayOnAppleByAForceIncludedHeaderAndNowhereElse() {
    const std::string mainCpp = ReadSource("game/main.cpp");
    const std::string definition = "int main(int argc, char** argv) { return MagicPortalsMain(argc, argv); }";
    CHECK_MSG(mainCpp.find(definition) != std::string::npos && mainCpp.find(definition) == mainCpp.rfind(definition), "main.cpp defines main() once, as the header expects");
    const std::size_t at = mainCpp.find(definition);
    // The line before the definition: the guard that keeps the definition out of the Android program.
    std::string guard;
    if (at != std::string::npos && at > 2) {
        const std::size_t lineEnd = mainCpp.rfind('\n', at - 1);
        const std::size_t lineStart = lineEnd == std::string::npos || lineEnd == 0 ? 0 : mainCpp.rfind('\n', lineEnd - 1) + 1;
        guard = mainCpp.substr(lineStart, lineEnd - lineStart);
        guard.erase(std::remove(guard.begin(), guard.end(), '\r'), guard.end());
    }
    CHECK_MSG(guard == "#if !defined(__ANDROID__)",
              "main.cpp still guards its main() with __ANDROID__ alone; if it gained __APPLE__, game/apple/RenameDesktopMain.hpp and its CMake line are to be deleted. The guard reads: '" + guard + "'");
    std::string header = ReadSource("game/apple/RenameDesktopMain.hpp");
    header.erase(std::remove(header.begin(), header.end(), '\r'), header.end());
    CHECK_MSG(header.find("\n#define main(...) MagicPortalsDesktopMainUnusedOnApple(__VA_ARGS__)\n") != std::string::npos, "the header's one macro is function-like");
    CHECK_MSG(header.find("#define main ") == std::string::npos, "and no object-like one");
    std::string cmake = ReadSource("game/CMakeLists.txt");
    cmake.erase(std::remove(cmake.begin(), cmake.end(), '\r'), cmake.end());
    const std::size_t apple = cmake.find("if (APPLE)\n    # ARC manages");
    const std::size_t rename = cmake.find("COMPILE_OPTIONS \"-include;${CMAKE_CURRENT_SOURCE_DIR}/apple/RenameDesktopMain.hpp\"");
    CHECK_MSG(apple != std::string::npos && rename != std::string::npos && rename > apple && cmake.find("\nendif()", apple) > rename,
              "game/CMakeLists.txt force-includes the header into main.cpp inside its if (APPLE) block");
    CHECK_MSG(cmake.find("set_property(SOURCE main.cpp APPEND PROPERTY COMPILE_OPTIONS") != std::string::npos, "on main.cpp alone");
}

void runTests() {
    JoinPutsOneSeparatorBetweenTwoNames();
    TheEnginesWorkingFolderIsBelowTheCaches();
    MacOSLooksInsideTheAppFirstAndThenInTheUsersApplicationSupport();
    IOSLooksOnlyInsideTheBundle();
    AFolderTheSystemDidNotNameIsNotALookup();
    TheNamesAreTheOnesTheDocsAndThePackagerUse();
    HoldingTheGameMeansWhatThePackagedShapeSays();
    AFolderHoldsTheGameOnlyWithTheLevelsAndTheChaptersTable();
    TheFirstFolderThatHoldsTheGameWinsAndTheRestAreNotAsked();
    NothingFoundIsSaidInWordsThatNameEveryPlaceLookedIn();
    TheLookupAsksOnlyWhetherAPathExists();
    TheChosenFolderIsHandedOnThroughTheRootFlagAfterTheProgramsName();
    APathTypedOnTheCommandLineKeepsTheMeaningTheShellGaveIt();
    OnlyAnAttendedRunMayOpenADialog();
    TheFlagsFileHasTheAndroidEntrysNames();
    TheEntriesUseTheHeaderAndNoRuleOfTheirOwn();
    TheDesktopMainIsRenamedAwayOnAppleByAForceIncludedHeaderAndNowhereElse();
}

} // namespace

MP_TEST_MAIN("test_mp_applelayout", 100)
