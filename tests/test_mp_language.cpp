// The language a player chose, kept between runs (planning step 236, PORT-32): sim/Language's Store.
//
// Modelled on Controls::Store, whose cases in test_mp_controls and test_mp_options this follows. What is pinned:
//
//   memory-only    an empty directory holds a choice for the run and writes nothing, and a directory with no file is English
//   the file       one line, {"version":1,"language":"<code>"}, byte for byte, for each of the eleven languages, read back as itself
//   a player who never chose   Save writes nothing: no file appears, and a file that would not read is not destroyed by it
//   unchanged      the same bytes are not written twice: a repeated gesture, the same choice set again, a reopened store saving what it read
//   corrupt        a file that is not JSON, not an object, empty, a directory, without a string `language`, naming a language the table
//                  does not have (an unknown code, a capital one, an empty one), or carrying a `version` other than 1 (a number that is not 1,
//                  or no number) reads as ENGLISH with the reason kept, Open says false, and the file stays as it was until the player
//                  chooses - and then it is written over; no `version` at all still reads
//   the run's policy  ResolvePolicy: a player's run (no flag, none of the rig's three) reads the store and offers the picker; --fixed-step,
//                  --route or --visit-levels without --lang is forced to English and offers no picker; --lang wins in every kind of run
//   tidy           no temporary, no .bak is left beside the file
//
// Free of the engine, like the store: a scratch directory and no window.

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "sim/Language.hpp"
#include "sim/Strings.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

using namespace MagicPortals;
using Strings::Lang;

namespace {

namespace fs = std::filesystem;

// The port's own data: MP_TEST_PORTDATA when it is set (TEST ONLY), else the build's own. _dupenv_s: std::getenv is C4996 on Windows.
std::string PortDataDir() {
    std::string text;
#ifdef _WIN32
    char* value = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&value, &length, "MP_TEST_PORTDATA") == 0 && value != nullptr) {
        text = value;
        std::free(value);
    }
#else
    if (const char* value = std::getenv("MP_TEST_PORTDATA"); value != nullptr) text = value;
#endif
    if (text.empty()) return MAGICPORTALS_PORT_DATA_DIR;
    std::printf("test_mp_language: MUTATION - MP_TEST_PORTDATA is set, the port's data is read from %s, not from game/data\n", text.c_str());
    return text;
}

Strings::LanguageTable Languages() {
    Strings::LanguageTable table;
    std::string error;
    CHECK_MSG(Strings::LoadLanguages(PortDataDir() + Strings::kLanguagesFile, table, error), "languages.json reads: " + error);
    return table;
}

// A directory of this case's own, emptied first: a store that read what the last run left behind would pass or fail for reasons nothing
// here states.
fs::path Scratch(const char* name) {
    const fs::path dir = fs::temp_directory_path() / "supersonic-test-mp-language" / name;
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    return dir;
}

std::string ReadAll(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void WriteFile(const fs::path& path, const std::string& text) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file << text;
}

std::size_t FilesIn(const fs::path& dir) {
    std::size_t count = 0;
    std::error_code ec;
    for (const auto& item : fs::directory_iterator(dir, ec)) {
        (void)item;
        ++count;
    }
    return count;
}

const char* kFile = Language::kStoreFileName;

void AMemoryOnlyStoreWritesNothing() {
    const Strings::LanguageTable table = Languages();
    Language::Store store;
    std::string error;
    CHECK_MSG(store.Open("", table, error), "a memory-only store opens: " + error);
    CHECK_MSG(!store.Persists() && !store.FromFile() && store.Path().empty(), "and has nowhere to write");
    CHECK_MSG(store.Language() == Lang::En && !store.Chosen(), "English, no choice");
    CHECK_MSG(store.Text().empty(), "and nothing to say");

    // It still REMEMBERS - it is only the file that is absent.
    store.SetLanguage(Lang::Ja);
    CHECK_MSG(store.Language() == Lang::Ja && store.Chosen() == Lang::Ja, "it holds the choice for the run");
    CHECK_MSG(store.Save(error), "saving a memory-only store is not a failure: " + error);
    CHECK_EQ(store.SaveCount(), 0);
    CHECK_MSG(store.Text() == "{\"version\":1,\"language\":\"ja\"}", "though it knows what it would write: " + store.Text());
}

void AFolderWithNoFileIsEnglishAndANeverChosenStoreWritesNone() {
    const Strings::LanguageTable table = Languages();
    const fs::path dir = Scratch("fresh");
    Language::Store store;
    std::string error;
    CHECK_MSG(store.Open(dir.string(), table, error), "a directory with no file opens: " + error);
    CHECK_MSG(store.Persists() && !store.FromFile(), "it has somewhere to write and read nothing");
    CHECK_MSG(store.Path() == (dir / kFile).string(), "the file is language.json in it: " + store.Path());
    CHECK_MSG(store.Language() == Lang::En && !store.Chosen() && store.Reason().empty(), "English, no choice, nothing to explain");
    CHECK_MSG(store.Save(error) && store.SaveCount() == 0, "a Save with nothing chosen is a success that writes nothing");
    CHECK_MSG(!fs::exists(dir / kFile), "no file appears for a player who never chose");
    CHECK_EQ(static_cast<int>(FilesIn(dir)), 0);
}

void EveryLanguageIsKeptAndReadBack() {
    const Strings::LanguageTable table = Languages();
    for (std::size_t i = 0; i < Strings::kLangCount; ++i) {
        const Lang lang = static_cast<Lang>(i);
        const std::string code = table.CodeOf(lang);
        const fs::path dir = Scratch(("roundtrip_" + code).c_str());
        {
            Language::Store store;
            std::string error;
            CHECK_MSG(store.Open(dir.string(), table, error), error);
            store.SetLanguage(lang);
            CHECK_MSG(store.Save(error), "saving " + code + ": " + error);
            CHECK_EQ(store.SaveCount(), 1);
        }
        CHECK_MSG(ReadAll(dir / kFile) == "{\"version\":1,\"language\":\"" + code + "\"}", code + ": the file's bytes: " + ReadAll(dir / kFile));
        Language::Store again;
        std::string error;
        CHECK_MSG(again.Open(dir.string(), table, error), "reopening " + code + ": " + error);
        CHECK_MSG(again.FromFile() && again.Language() == lang && again.Chosen() == lang, code + " reads back as itself");
        CHECK_MSG(again.Reason().empty(), code + ": nothing to explain");
    }
}

void TheSameBytesAreNotWrittenTwice() {
    const Strings::LanguageTable table = Languages();
    const fs::path dir = Scratch("once");
    Language::Store store;
    std::string error;
    CHECK(store.Open(dir.string(), table, error));
    store.SetLanguage(Lang::De);
    CHECK(store.Save(error));
    CHECK_EQ(store.SaveCount(), 1);
    CHECK(store.Save(error));
    CHECK(store.Save(error));
    store.SetLanguage(Lang::De);
    CHECK(store.Save(error));
    CHECK_MSG(store.SaveCount() == 1, "a gesture repeated is one write: " + std::to_string(store.SaveCount()));
    store.SetLanguage(Lang::Fr);
    CHECK(store.Save(error));
    CHECK_MSG(store.SaveCount() == 2 && ReadAll(dir / kFile).find("\"fr\"") != std::string::npos, "a different choice is a write");
    store.SetLanguage(Lang::De);
    CHECK(store.Save(error));
    CHECK_MSG(store.SaveCount() == 3 && ReadAll(dir / kFile).find("\"de\"") != std::string::npos, "and so is going back");

    // A store reopened on the file it wrote saves what it read without writing.
    Language::Store reopened;
    CHECK(reopened.Open(dir.string(), table, error));
    CHECK(reopened.Save(error));
    reopened.SetLanguage(Lang::De);
    CHECK(reopened.Save(error));
    CHECK_MSG(reopened.SaveCount() == 0, "a reopened store saving the choice it read writes nothing");
    reopened.SetLanguage(Lang::It);
    CHECK(reopened.Save(error));
    CHECK_MSG(reopened.SaveCount() == 1 && reopened.Language() == Lang::It, "but a new choice does");

    // Open starts the count over.
    CHECK(reopened.Open(dir.string(), table, error));
    CHECK_EQ(reopened.SaveCount(), 0);

    // No temporary and no .bak beside it: this is a choice and not progress.
    CHECK_MSG(FilesIn(dir) == 1, "language.json alone in the directory: " + std::to_string(FilesIn(dir)) + " entries");
}

void AFileThatCannotBeUsedReadsAsEnglishWithItsReason() {
    const Strings::LanguageTable table = Languages();
    struct Bad {
        const char* name;
        std::string text;
        const char* mention; // the reason names this
    };
    const std::vector<Bad> bad = {
        {"notjson", "{ nope", ""},
        {"empty", "", ""},
        {"array", "[]", "not an object"},
        {"nolanguage", "{\"version\":1}", "language"},
        {"number", "{\"version\":1,\"language\":3}", "language"},
        {"null", "{\"language\":null}", "language"},
        {"unknown", "{\"version\":1,\"language\":\"xx\"}", "xx"},
        {"capital", "{\"version\":1,\"language\":\"EN\"}", "EN"},
        {"blank", "{\"version\":1,\"language\":\"\"}", "not one of the languages"},
        {"region", "{\"version\":1,\"language\":\"de-DE\"}", "de-DE"},
        // A version other than ours, however well the rest reads: another build wrote it.
        {"version2", "{\"version\":2,\"language\":\"de\"}", "version"},
        {"version0", "{\"version\":0,\"language\":\"de\"}", "version"},
        {"versionfraction", "{\"version\":1.5,\"language\":\"de\"}", "version"},
        {"versionstring", "{\"version\":\"1\",\"language\":\"de\"}", "version"},
        {"versionnull", "{\"version\":null,\"language\":\"de\"}", "version"},
        {"versionandbadlanguage", "{\"version\":7,\"language\":\"xx\"}", "version"},
    };
    for (const Bad& b : bad) {
        const fs::path dir = Scratch((std::string("bad_") + b.name).c_str());
        WriteFile(dir / kFile, b.text);
        Language::Store store;
        std::string error;
        CHECK_MSG(!store.Open(dir.string(), table, error), std::string(b.name) + ": Open says the file cannot be used");
        CHECK_MSG(!error.empty() && error.find(kFile) != std::string::npos && error.find(b.mention) != std::string::npos,
                  std::string(b.name) + ": and says why, naming the file: " + error);
        CHECK_MSG(store.Reason() == error, std::string(b.name) + ": the reason is kept");
        CHECK_MSG(store.Language() == Lang::En && !store.Chosen() && !store.FromFile(), std::string(b.name) + ": English, no choice");
        CHECK_MSG(store.Persists(), std::string(b.name) + ": though it can still write");

        // Nothing chosen: Save does not touch the file.
        CHECK_MSG(store.Save(error) && store.SaveCount() == 0 && ReadAll(dir / kFile) == b.text, std::string(b.name) + ": the file is left as it was");

        // The player chooses: it is written over.
        store.SetLanguage(Lang::Es);
        CHECK_MSG(store.Save(error), std::string(b.name) + ": " + error);
        CHECK_MSG(store.SaveCount() == 1 && ReadAll(dir / kFile) == "{\"version\":1,\"language\":\"es\"}", std::string(b.name) + ": and then written over");
        Language::Store again;
        CHECK_MSG(again.Open(dir.string(), table, error) && again.Language() == Lang::Es, std::string(b.name) + ": and it reads");
        CHECK_MSG(FilesIn(dir) == 1, std::string(b.name) + ": with nothing else left beside it");
    }

    // A file that is a tolerable variation of ours still reads: another member, no version, extra white space.
    const std::vector<std::pair<const char*, std::string>> fine = {
        {"extra", "{\"version\":1,\"language\":\"ja\",\"extra\":true}"},
        {"noversion", "{\"language\":\"ja\"}"},
        {"spaces", "{ \"version\" : 1 ,\n  \"language\" : \"ja\" }\n"},
    };
    for (const auto& [name, text] : fine) {
        const fs::path dir = Scratch((std::string("fine_") + name).c_str());
        WriteFile(dir / kFile, text);
        Language::Store store;
        std::string error;
        CHECK_MSG(store.Open(dir.string(), table, error) && store.FromFile() && store.Language() == Lang::Ja && store.Reason().empty(),
                  std::string(name) + ": reads as Japanese: " + error);
        CHECK_MSG(store.Save(error) && store.SaveCount() == 0 && ReadAll(dir / kFile) == text, std::string(name) + ": and saving what it read leaves the file alone");
    }

    // A directory where the file should be cannot be read as one; the store does not throw, and a choice is refused by the write, with a
    // reason, and not counted.
    {
        const fs::path dir = Scratch("indir");
        std::error_code ec;
        fs::create_directories(dir / kFile, ec);
        Language::Store store;
        std::string error;
        CHECK_MSG(!store.Open(dir.string(), table, error) && !error.empty() && store.Language() == Lang::En, "a directory in the file's place reads as unusable: " + error);
        store.SetLanguage(Lang::Fr);
        CHECK_MSG(!store.Save(error) && !error.empty() && store.SaveCount() == 0, "and the write is refused, not counted: " + error);
        CHECK_MSG(store.Language() == Lang::Fr, "the choice is held for the run");
        CHECK_MSG(fs::is_directory(dir / kFile), "and the directory is still there");
    }
}

void TheRigsRunsAreEnglishAndAPlayersReadTheStore() {
    const std::string english(Strings::kLangCodes[Strings::IndexOf(Strings::kDefaultLang)]);
    // A player's run: nothing forced, so the store is read; the picker is offered.
    const Language::Policy player = Language::ResolvePolicy(std::nullopt, false, false, false);
    CHECK_MSG(!player.forced && player.SaveAllowed() && player.picker, "a player's run reads the saved choice, may write it, and offers the picker");
    // Each of the rig's three flags alone, and all together: English, forced (so nothing is read and nothing written), no picker.
    for (int mask = 1; mask < 8; ++mask) {
        const Language::Policy rig = Language::ResolvePolicy(std::nullopt, (mask & 1) != 0, (mask & 2) != 0, (mask & 4) != 0);
        CHECK_MSG(rig.forced && *rig.forced == english && !rig.SaveAllowed() && !rig.picker,
                  "a developer's run (mask " + std::to_string(mask) + ") is English and neither reads nor writes the saved choice");
    }
    // --lang wins everywhere, and a player's run with it offers the picker (the choice is kept in memory only).
    for (int mask = 0; mask < 8; ++mask) {
        const Language::Policy flagged = Language::ResolvePolicy(std::string("ja"), (mask & 1) != 0, (mask & 2) != 0, (mask & 4) != 0);
        CHECK_MSG(flagged.forced && *flagged.forced == "ja" && !flagged.SaveAllowed() && flagged.picker == (mask == 0),
                  "--lang is the flag as given in every kind of run (mask " + std::to_string(mask) + ")");
    }
    // The code English is in the table is the one the policy forces.
    const Strings::LanguageTable table = Languages();
    CHECK_MSG(table.LangOfCode(english) == Strings::kDefaultLang, "and it is a code of the table: " + english);
}

void TheStoreKeepsItsOwnCopyOfTheTable() {
    // The table handed to Open may be a temporary; the store maps codes with its own copy.
    const fs::path dir = Scratch("copy");
    Language::Store store;
    std::string error;
    {
        const Strings::LanguageTable table = Languages();
        CHECK(store.Open(dir.string(), table, error));
    }
    store.SetLanguage(Lang::Ar);
    CHECK(store.Save(error));
    CHECK_MSG(ReadAll(dir / kFile) == "{\"version\":1,\"language\":\"ar\"}", "Text is still written from the table: " + ReadAll(dir / kFile));
    // A store given a table that was never loaded knows no code, so no file can name a language.
    WriteFile(dir / kFile, "{\"version\":1,\"language\":\"de\"}");
    Language::Store empty;
    CHECK_MSG(!empty.Open(dir.string(), Strings::LanguageTable{}, error) && empty.Language() == Lang::En, "an unloaded table recognises nothing: " + error);
}

void runTests() {
    AMemoryOnlyStoreWritesNothing();
    AFolderWithNoFileIsEnglishAndANeverChosenStoreWritesNone();
    EveryLanguageIsKeptAndReadBack();
    TheSameBytesAreNotWrittenTwice();
    AFileThatCannotBeUsedReadsAsEnglishWithItsReason();
    TheRigsRunsAreEnglishAndAPlayersReadTheStore();
    TheStoreKeepsItsOwnCopyOfTheTable();
}

} // namespace

MP_TEST_MAIN("test_mp_language", 150)
