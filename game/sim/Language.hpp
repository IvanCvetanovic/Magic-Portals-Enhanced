#pragma once

// The language a player chose, kept between runs (planning step 236, PORT-32).
//
// Its own file and its own store, beside controls.json, audio.json, scores.json and achievements.json - modelled on Controls::Store, for
// the reason Controls.hpp gives: a third key of audio.json would be rewritten from the two members Switches::Store knows. Free of the
// engine, so a test exercises all of it with a temporary directory and no window.
//
//   {"version":1,"language":"de"}
//
// WHAT IT DECIDES AND WHAT IT DOES NOT. It keeps a choice and answers with a language. There is NO detection of the operating system's
// language anywhere in the layer (the suites must be deterministic, and English is the default): a player who never chose is English. Which
// run may read or write the file, and the `--lang <code>` flag that forces a language for one run and writes nothing (as `--controls
// modern` does), are Policy's below (planning step 238, Paths::language); this store is handed an empty directory for a run that must
// not touch a file.

#include "sim/Strings.hpp"

#include <optional>
#include <string>

namespace MagicPortals::Language {

// WHICH LANGUAGE THE RUN SPEAKS (planning step 238), as Controls::Policy says which controls it plays. `--lang <code>` forces a language for ONE run:
// it wins over the saved choice, the run neither reads nor writes language.json (a capture's flag, like `--controls modern`), and the choice made in a
// forced run is kept in memory only. Without it the run reads the store and speaks the player's choice, else English. Handed in `Paths::language`;
// every suite's bare Paths is "no flag, nothing saved" (the layer is also handed no save directory), so the suites are deterministic.
struct Policy {
    std::optional<std::string> forced; // the code as the flag gave it; main has checked it against the language table
    bool SaveAllowed() const { return !forced; }
    // Whether the run OFFERS the picker (planning step 239): the globe on the main menu and on Options, and the screen behind them. A player's run
    // does; a developer's run (the rig's three flags) and every suite's bare Paths do not, so a capture of the original's menus is the original's
    // menus and no pinned frame moves. It does not depend on the controls feature: the gear needs the modern controls' tables and the picker needs none.
    bool picker = false;
};

// The policy a run gets from its command line: `langFlag` is the `--lang` code main has checked against the table, and the three booleans are the
// rig's flags (`--fixed-step`, `--route`, `--visit-levels`). A developer's or parity run (any of the three) with no `--lang` is FORCED to English: the
// player's saved language.json is neither read nor written by it, so a capture compared with the original and a replayed route do not change with the
// language the machine's player chose, and the picker is not offered. A `--lang` wins in every kind of run. A player's run (none of the three, no
// flag) reads the store. Here, and not in main, so that a suite can pin it.
//
// THESE THREE AND NO OTHER. The capture helpers (`--press`, `--hold`, `--tap`, `--drag`, `--place`, `--burn-dragon`) do not imply a developer's run, so
// a capture made with them and without `--fixed-step` reads the saved language (the player's own save folder, or the one `--saves` names) and is offered
// the picker: a capture must pass `--fixed-step` (or `--lang`). A build without MAGICPORTALS_DEV_TOOLS has no `--route` or `--visit-levels`, and main passes false for both there.
Policy ResolvePolicy(const std::optional<std::string>& langFlag, bool fixedStep, bool route, bool visitLevels);

// The file, inside whichever directory the game was handed.
inline constexpr const char* kStoreFileName = "language.json";

namespace StoreKey {
inline constexpr const char* kVersion = "version";
inline constexpr const char* kLanguage = "language";
} // namespace StoreKey
inline constexpr int kStoreVersion = 1;

class Store {
public:
    // Reads `directory`/language.json if there is one. An EMPTY directory is memory-only, as Controls::Store's is (every suite builds the
    // layer bare, and none of them may write a file).
    //
    // `languages` is the table that says which codes there are (Strings::LoadLanguages): it is kept, since a code is only a language
    // by it. False, with `error` (and Reason()), for a file that exists and cannot be used - it is not JSON, not an object, has no
    // string `language`, names a language the table does not have, or carries a `version` other than kStoreVersion (a number; a file with none
    // reads): the store then holds NO choice, so Language() is English, and the
    // file is left exactly as it is (Save writes nothing until the player chooses, and then writes over it). A missing file is true.
    bool Open(const std::string& directory, const Strings::LanguageTable& languages, std::string& error);

    // The language in force: the player's choice, else English.
    Strings::Lang Language() const { return m_chosen.value_or(Strings::kDefaultLang); }
    // The player's choice, or nothing: no file, a file that would not read, or no SetLanguage yet.
    std::optional<Strings::Lang> Chosen() const { return m_chosen; }

    // The choice, in memory. A memory-only store keeps it for the run and writes nothing.
    void SetLanguage(Strings::Lang lang) { m_chosen = lang; }

    // The bytes Save writes for what the store holds now: one line, no trailing newline. Empty while nothing was chosen.
    std::string Text() const;

    // Writes language.json by sim/Files' atomic write, as Controls::Store::Save does and for its reason. Nothing is written when nothing has
    // been chosen (a player who never chose has no file, and a file that would not read is not destroyed by a Save that has nothing to say),
    // nor when these bytes are the ones this store wrote last, nor by a memory-only store (which succeeds). No backup is kept: this is a
    // choice and not progress.
    bool Save(std::string& error);

    // Whether this store has somewhere to write, and whether Open read a usable file.
    bool Persists() const { return !m_path.empty(); }
    bool FromFile() const { return m_fromFile; }
    const std::string& Path() const { return m_path; }
    // Why Open found the file unusable, or empty (the reason Open's `error` carried, kept for the layer's log).
    const std::string& Reason() const { return m_reason; }
    // How many times Save has WRITTEN the file since Open: a save that found the bytes it wrote last, one that had nowhere to write or nothing
    // to say, and one that failed are none of them a write. A number a test reads for "once per gesture".
    int SaveCount() const { return m_saves; }

private:
    Strings::LanguageTable m_languages;
    std::optional<Strings::Lang> m_chosen;
    std::string m_path;
    bool m_fromFile = false;
    std::string m_reason;
    std::string m_written;
    int m_saves = 0;
};

} // namespace MagicPortals::Language
