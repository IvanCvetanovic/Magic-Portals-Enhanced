#pragma once

// The sound and music switches, kept between runs.
//
// The original saves both on every toggle and reads them back at the next
// start: GlobalSoundSwitch::manageSoundSwitch sets the global volume to 1 or 0
// and hands it to GlobalVolumeManager::saveVolume ('globalVolume'), and
// GlobalMusicSwitch::manageMusicSwitch hands its state to
// MusicSwitchManager::saveMusicState ('musicEnabled'), both under the entity
// 'audio' of the framework's user-data file. main sets the loaded volume before
// anything plays, and getState loads the music's lazily, on its first read.
// sounds.json's `switches` cites every instruction, and holds the two defaults a
// player who has never touched a switch gets.
//
// The port kept them for the session only, so every launch came up with the
// sound on whatever the player had chosen. This is the same two values under the
// same names, in the port's own JSON beside scores.json (sim/Scores.hpp says why
// not the original's enml).
//
// Free of the engine, like the rest of sim/, so a test can exercise all of it
// with a temporary directory and no window.

#include <string>

namespace MagicPortals::Switches {

// What a fresh save starts at: loadVolume's default and loadMusicState's.
struct Defaults {
    double globalVolume = 1.0;
    bool musicEnabled = true;
};

// sounds.json's `switches`. False, with `error`, for a file that does not read
// or a block without both values: they are the decode's, and a start that
// silently fell back to code would hide a broken table.
bool LoadDefaults(const std::string& path, Defaults& out, std::string& error);

// The file, inside whichever directory the game was handed. Named for the
// original's entity, and the switches' alone: nothing else is lost when an
// unreadable one is written over (Open).
inline constexpr const char* kFileName = "audio.json";

class Store {
public:
    // Reads `directory`/audio.json if there is one, and the defaults for what it
    // does not say - the original's loadValue gives the default for a value the
    // file lacks.
    //
    // An EMPTY directory is memory-only, as Scores::Store's is: every suite
    // builds the layer bare, and none of them may write a file.
    //
    // False, with `error`, only for a file that exists and does not read. Unlike
    // scores.json that file IS written over at the next toggle: it holds nothing
    // but these two values, and the original does the same - saveValue parses
    // whatever the file holds, adds the value and writes it all back.
    bool Open(const std::string& directory, const Defaults& defaults, std::string& error);

    // GlobalSoundSwitch::GlobalSoundSwitch #25-39: off exactly when the volume is 0.
    bool SoundOn() const { return m_globalVolume != 0.0; }
    bool MusicOn() const { return m_musicEnabled; }

    // manageSoundSwitch #11-24: the global volume 1 when switched on, 0 when off.
    void SetSound(bool on);
    void SetMusic(bool on);

    // Writes audio.json, via a temporary and a rename, as Scores::Store::Save
    // does and for its reason. A memory-only store writes nothing and succeeds.
    bool Save(std::string& error) const;

    // Whether this store has somewhere to write, and whether Open read a file.
    bool Persists() const { return !m_path.empty(); }
    bool FromFile() const { return m_fromFile; }
    const std::string& Path() const { return m_path; }

private:
    double m_globalVolume = 1.0;
    bool m_musicEnabled = true;
    std::string m_path;
    bool m_fromFile = false;
};

} // namespace MagicPortals::Switches
