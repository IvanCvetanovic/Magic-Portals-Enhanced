#pragma once

// The original's achievements: the list, as Enhanced's gitignored achievements.json
// holds it, and - since planning step 143 - what unlocks each, the set the player
// has unlocked, kept between runs, and the text the pause's popup shows of it.
//
// THE CONTENT IS NOT THIS REPOSITORY'S. The 82 titles, descriptions, icons and
// points - and the secret achievement's words - are Asantee-authored. They are
// extracted by Enhanced's tools/asbc/achievements.py from the decode into its
// out/data/, beside chapters.json, and read from there at run time the way the
// levels are (SUPERSONIC_MAGICPORTALS_ACHIEVEMENTS). Nothing here names one, and
// no suite embeds one: the suites build their own lists. Without the file the
// dashboard draws no rows and an unlock shows no card.
//
// WHAT UNLOCKS ONE is the port's own game/data/achievements.json (`Rules`): ids,
// numbers and entity names, each decoded and cited - checkForLevelAchievements'
// 67 dispatches, registerMedalAchievements' 12, the red key's and the two help
// blocks'. Evaluating a rule asks the level (`World`) what it has left standing;
// the layer answers from the simulation.
//
// THE SAVED SET (`Store`) is AchievementManager's m_list: the ids in the order they
// were unlocked, written on every unlock with the original's hash, in the port's
// own achievements.json beside scores.json. The dashboard and the popup read it.
// Content::fromCompletion and fromLevelMedal - what the tool's own walk found a
// save alone decides - stay as its cross-check (Unlocked).

#include <string>
#include <vector>

#include "sim/Chapters.hpp"
#include "sim/Locking.hpp"
#include "sim/Scores.hpp"

namespace MagicPortals::Achievements {

struct Entry {
    int id = -1;
    int world = -1;
    int level = -1; // -1 for a chapter's own
    int points = 0;
    std::string title;
    std::string description;
    std::string icon; // within the dashboard's icon directory
    bool isNew = false;
    bool isSecret = false;
};

// What a secret achievement shows while it is locked (Achievement::getTitle,
// getDescription, getIconSprite).
struct Secret {
    std::string title;
    std::string descriptionPrefix;    // then world + 1,
    std::string descriptionSeparator; // then level + 1
    std::string icon;
};

// registerMedalAchievements: a chapter at `minPercent` or more unlocks `id`.
struct CompletionRule {
    int world = 0;
    int minPercent = 0;
    int id = -1;
};

// checkForLevelAchievements, the conditions a save decides: a level's medal at
// `minMedal` or more unlocks `id`.
struct MedalRule {
    int world = 0;
    int level = 0;
    int minMedal = 0;
    int id = -1;
};

struct Content {
    std::vector<Entry> entries; // AchievementManager's order: the dashboard's rows
    Secret secret;
    std::vector<CompletionRule> fromCompletion;
    std::vector<MedalRule> fromLevelMedal;

    const Entry* Find(int id) const;
};

// The tool's format 1, strictly: false with `error` when the file is missing,
// unreadable or not that shape, leaving `out` as it was.
bool Load(const std::string& path, Content& out, std::string& error);

// The ids the TOOL's walk says a save alone unlocks, ascending: its two tables
// over the medals. Kept as the suites' cross-check of `Rules`; the game reads the
// saved set (Store).
std::vector<int> Unlocked(const Content& content, const Locking::Rules& locking, const Scores::Store& scores,
                          const Chapters::Table& chapters);

bool IsUnlocked(const std::vector<int>& unlocked, int id);

// AchievementManager::computeAchievementPoints: every unlocked entry's points.
int Points(const Content& content, const std::vector<int>& unlocked);

// Achievement::getTitle, getDescription and getIconSprite: the entry's own,
// unless it is secret and still locked.
std::string TitleOf(const Content& content, const Entry& entry, bool unlocked);
std::string DescriptionOf(const Content& content, const Entry& entry, bool unlocked);
std::string IconOf(const Content& content, const Entry& entry, bool unlocked);

// AchievementManager::getAchievementsFromLevel (249772..250229): the entries the
// content names (world, level) for, in its order; none for a negative world or
// level. The original returns null for none, which is what takes the pause's
// button away (CustomGameMenuLayer #229-280).
std::vector<const Entry*> FromLevel(const Content& content, int world, int level);

// ---- what unlocks one: game/data/achievements.json ---------------------------

// One term of a rule (the file's `level._source` lists them).
enum class TermKind {
    MedalAtLeast,
    MedalAbove,
    PortalsAtMost,
    PortalsExactly,
    Game,
    TimeAtMostMs,
    CountAtLeast,
    CountExactly,
    Exists,
    Absent,
    LiftDoorsShut,
    Unnoticed,
    LapsAtLeast,
};

struct Term {
    TermKind kind = TermKind::Game;
    double value = 0.0;
    std::vector<std::string> names; // Count*, LiftDoorsShut; Exists and Absent hold one
};

struct LevelRule {
    int id = -1;
    int world = 0;
    int level = 0;
    std::vector<Term> terms; // all of them
};

struct CompletionStep {
    int minPercent = 0;
    int id = -1;
};

struct Rules {
    struct Save {
        std::string file;   // achievements.json
        std::string entity; // achievements
        std::string listKey;
        std::string hashKey;
        long long firstMultiplierOffset = 0; // t + 2
        long long subtract = 0;              // 27
        long long multiply = 0;              // 3
        long long add = 0;                   // 2
    } save;

    double completionDelayMs = 0.0;
    std::vector<std::vector<CompletionStep>> completion; // by world, in unlocking order

    double levelDelayMs = 0.0;
    std::vector<LevelRule> level; // checkForLevelAchievements' order

    struct Key {
        std::string colour;
        int id = -1;
        double delayMs = 0.0;
    } key;

    struct HelpBlock {
        int world = 0;
        int level = 0;
        int id = -1;
    };
    double helpBlockDelayMs = 0.0;
    std::vector<HelpBlock> helpBlocks;

    // checkPreviouslyUnlockAchievements' fixed arguments.
    int menuPortals = 0;
    double menuGameTimeMs = 0.0;

    // What a fire signal adds back in a lit torch's place (level_world).
    std::string signalTorch;

    struct Card {
        std::string background;
        std::string messagePrefix;
        std::string iconDirectory;
        double durationMs = 0.0;
        std::string soundEvent;
    } card;

    struct LockedCard {
        std::string background;
        std::string message;
        double durationMs = 0.0;
        double delayMs = 0.0;
    } locked;

    struct Popup {
        std::string tiles;
        double tileUnitsX = 0.0;
        double tileUnitsY = 0.0;
        int tileColumns = 0;
        int tileRows = 0;
        std::string font;
        double unitsPerFontPx = 0.0;
        std::string unlockedMark;
        std::string lockedMark;
        std::string closeSprite;
        double closeAtX = 0.0;
        double closeAtY = 0.0;
        double closeOriginX = 0.0;
        double closeOriginY = 0.0;
        double closeUnitsX = 0.0;
        double closeUnitsY = 0.0;
    } popup;

    // The ids every rule above names, each once - which the content must have.
    std::vector<int> AllIds() const;
};

// game/data/achievements.json, strictly: anything missing, a term this does not
// know, or an id named twice by different kinds of rule is refused.
bool LoadRules(const std::string& path, Rules& out, std::string& error);

// What a rule asks the level about, which only the layer can answer.
class World {
public:
    virtual ~World() = default;
    // countEntities: live entities named exactly as one of `names`.
    virtual int Count(const std::vector<std::string>& names) const = 0;
    // SeekEntity(name) is not null.
    virtual bool Exists(const std::string& name) const = 0;
    // Every lift door named so at its original place, at rest: `status` 0.
    virtual bool LiftDoorsShut(const std::vector<std::string>& names) const = 0;
    // spottedCount: the minions' enemySpotCount, summed.
    virtual int SpottedCount() const = 0;
    // retrieveRondaboutCount(SeekEntity('gravity_area'), SeekEntity('dark_mage.ent')).
    virtual int Laps() const = 0;
};

// One call of checkForLevelAchievements.
struct Play {
    int world = 0;
    int level = 0;
    int medal = 0;
    int portals = 0;
    double gameTimeMs = 0.0;
    bool game = false; // the current state is 'Game'
};

// The ids its rules dispatch, in their order. Terms are tried in order and the
// first that fails ends the rule; a term that needs the level is false where
// `world` is null.
std::vector<int> Dispatches(const Rules& rules, const Play& play, const World* world);

// registerMedalAchievements(world): the ids a chapter at `percent` unlocks, in order.
std::vector<int> CompletionIds(const Rules& rules, int world, int percent);

// The help block on (world, level) dispatches this id; -1 for none.
int HelpBlockId(const Rules& rules, int world, int level);

// generateAchivementHashCode over a list as m_list holds it ("45,6,7").
std::string HashOf(const Rules::Save& rules, const std::string& list);

// The ids, as m_list holds them.
std::string ListOf(const std::vector<int>& ids);

// The pause's AchievementsPopup text for a level's entries, as its constructor
// builds it: mark, ' ', title, ':\n', description, and '\n\n' between two - the
// title and description through the secret mask.
std::string PopupText(const Rules& rules, const Content& content, const std::vector<const Entry*>& entries,
                      const std::vector<int>& unlocked);

// The saved set, kept between runs: AchievementManager's m_list.
class Store {
public:
    // Reads `directory`/<save.file> if there is one. An EMPTY directory is
    // memory-only, as Scores::Store's is: every suite builds the layer bare. A
    // file whose hash is not its list's opens EMPTY, as loadList leaves m_list ''
    // (HashRefused says so). False, with `error`, only for a file that exists and
    // does not read; the store is then empty, and the next Save writes over it, as
    // the original's saveList writes its file fresh.
    bool Open(const std::string& directory, const Rules::Save& rules, std::string& error);

    // isUnlocked.
    bool Has(int id) const;
    // unlock's append: false, and nothing changed, for an id it has.
    bool Add(int id);

    // saveList: the list and its hash, in one step (Files::WriteAtomic), with the
    // last good file kept beside it as <file>.bak - unless the file Open found
    // would not read or had a hash that was not its list's, which is written
    // over and is no copy to keep. A memory-only store writes nothing and
    // succeeds.
    bool Save(std::string& error) const;

    const std::vector<int>& Ids() const { return m_ids; } // in the order of unlocking
    std::vector<int> Sorted() const;
    bool Persists() const { return !m_path.empty(); }
    bool FromFile() const { return m_fromFile; }
    bool HashRefused() const { return m_hashRefused; }
    const std::string& Path() const { return m_path; }

private:
    Rules::Save m_rules;
    std::vector<int> m_ids;
    std::string m_path;
    bool m_fromFile = false;
    bool m_hashRefused = false;
    // Whether the file on disk is one that read whole (Open) or one this store wrote (Save): what a backup may be made of.
    mutable bool m_liveIsGood = false;
};

} // namespace MagicPortals::Achievements
