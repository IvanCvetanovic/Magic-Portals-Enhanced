#pragma once

// What a player's run on a DESKTOP does that a phone's does not (players' first reports, 2026-11-26: on Windows the game
// could not be made full screen, and the phone's on-screen controls stayed on the picture).
//
//   - FULL SCREEN: F11, or Enter with either Alt, anywhere in the game, and the Options screen's switch. The choice is
//     kept in desktop.json beside the medals, and the next start opens in it from its first frame. The command line's
//     --fullscreen and --windowed still decide their own run, and neither writes the file.
//   - THE WALK ARROWS ARE OFF: the keys walk (A and D, or the left and right arrows). The Options screen's other switch
//     brings them back, which is what a touch screen laptop with its keys folded away needs. Restart, pause and
//     clear-portals stay: a mouse works them.
//
// Which run is a desktop player's is main's to say (MagicPortalsLayer::Paths::desktop): an attended run of a build that is
// not a phone's. Every capture, route and suite is neither, and plays what it always did.
//
// Pure, like sim/Controls: no registry, no window, no file but the ones it is given. The window is asked by the layer
// (and, for the first frame, by main).

#include <optional>
#include <string>

#include <glm/glm.hpp>

namespace MagicPortals::Desktop {

// The file the two choices are kept in, in the save directory.
inline constexpr const char* kStoreFile = "desktop.json";

// The key codes this needs that the engine's Key namespace does not name (it has no function keys); both are GLFW's, which
// the engine passes through, and its desktop polling reads F1 to F25 and the keypad.
inline constexpr int kKeyF11 = 300;
inline constexpr int kKeyKeypadEnter = 335;

// Whose run is a desktop player's (MagicPortalsLayer::Paths::desktop): a person at it (an attended run), a build that is not a phone's, and no phone
// profile.
constexpr bool PlayerRun(bool attended, bool mobileBuild, bool mobileProfile) {
    return attended && !mobileBuild && !mobileProfile;
}

// controls.json's `desktop` block: what a player who has chosen nothing gets, and the words that teach walking where the arrows are hidden.
struct Rules {
    bool fullscreenDefault = false;
    bool walkPadsDefault = false;

    struct WalkHint {
        std::string words;
        std::string font; // within the original's data directory
        double unitsPerFontPx = 0.0;
        glm::dvec2 atScreen{0.5}; // the words' centre, as a fraction of the view
        double maxWidthUnits = 0.0;
        int alphaByte = 255;
    } walkHint;
};

// Read strictly: a block or a value that is missing or not its own kind is refused with `error` naming it.
bool LoadRules(const std::string& path, Rules& out, std::string& error);

// Whether this tick's keys ask to switch between full screen and a window: F11 went down, or Enter went down while an Alt
// was held.
constexpr bool ToggleAsked(bool f11Pressed, bool enterPressed, bool altDown) {
    return f11Pressed || (enterPressed && altDown);
}

// Whether a desktop player's run opens full screen: --windowed says no, --fullscreen says yes (the engine refuses both at
// once), else the player's kept choice, else the shipped default.
constexpr bool StartFullscreen(const std::optional<bool>& kept, bool shippedDefault, bool flagFullscreen, bool flagWindowed) {
    if (flagWindowed) return false;
    if (flagFullscreen) return true;
    return kept.value_or(shippedDefault);
}

// The player's two choices. Opened and saved as the other small stores are (sim/Files: a temporary renamed over the file).
class Store {
public:
    // Reads `directory`/desktop.json if there is one. An empty directory is memory-only: nothing is read and nothing is
    // written. A missing file is no choice and no error; a member that is missing or not true or false is no choice for it.
    // False, with `error`, only for a file that exists and is not a JSON object: no choices are held, and the next Save
    // writes over it.
    bool Open(const std::string& directory, std::string& error);

    std::optional<bool> Fullscreen() const { return m_fullscreen; }
    std::optional<bool> WalkPads() const { return m_walkPads; }

    // The choice, in memory.
    void SetFullscreen(bool fullscreen) { m_fullscreen = fullscreen; }
    void SetWalkPads(bool shown) { m_walkPads = shown; }

    // The bytes Save writes for what the store holds: one line, no trailing newline, only the members chosen.
    std::string Text() const;

    // Writes desktop.json, only when the bytes differ from the ones last written. A memory-only store succeeds.
    bool Save(std::string& error);

    bool Persists() const { return !m_path.empty(); }
    const std::string& Path() const { return m_path; }

private:
    std::string m_path;
    std::string m_written;
    std::optional<bool> m_fullscreen;
    std::optional<bool> m_walkPads;
};

} // namespace MagicPortals::Desktop
