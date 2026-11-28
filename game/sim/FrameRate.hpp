#pragma once

// The frame rate a player may choose (planning step 285; the owner, 2026-11-28: "does the game have variable fps? I dont want it to only be 60fps max.
// If it spossible you should allow users to choose").
//
// DRAWING ONLY. The simulation ticks 60 times a second whatever this says: a cap changes how many frames are drawn, never what a tick does, so no route,
// replay or capture moves.
//
// AUTO IS THE GAME AS IT WAS. The default (controls.json frame_rate.default) asks nothing of the engine or the display: no cap request, no refresh-rate
// request, no new decision about the pictures' smoothing.
//
// What a choice asks for, by platform (the platform is the build's target, not the run's profile):
//   desktop   a cap of N frames a second, dropped (nothing asked) where the display paces the loop at or under N already: "60" on a 60 Hz FIFO screen is
//             Auto, with no second pacer beside the first.
//   Android   30 and 60: a cap, and the display asked for android_low_request_hz (or N, if that is higher). "Max": no cap, and the display's highest rate.
//   iOS       a cap only.
// The monitor's refresh rate is read on a desktop alone: Android's and iOS's WindowControl answer a fixed 60 whatever the display does.
//
// Pure, like sim/Desktop: no registry, no window, no file but the ones it is given. The layer asks the engine (core/FrameRateControl.hpp) and the window.

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace MagicPortals::FrameRate {

// The file the choice is kept in, in the save directory. A file of its own: desktop.json is never opened by a phone.
inline constexpr const char* kStoreFile = "framerate.json";

// The range a number of frames a second is held to, in the data and in the file: the engine raises a cap under its lowest (FramePacer::kLowestCapFps, 15)
// to it, and nothing draws a thousand.
inline constexpr int kLowestFps = 15;
inline constexpr int kHighestFps = 1000;

// Which choices a run offers and what one asks for.
enum class Platform { Desktop, Android, Ios };

// "auto", a number of frames a second, or "max" (no cap, the display's highest rate).
struct Choice {
    enum class Kind { Auto, Fps, Max };

    Kind kind = Kind::Auto;
    int fps = 0; // Kind::Fps alone

    static Choice Auto() { return Choice{}; }
    static Choice Of(int framesASecond) { return Choice{Kind::Fps, framesASecond}; }
    static Choice Max() { return Choice{Kind::Max, 0}; }

    bool operator==(const Choice&) const = default;
};

// The words the log and the file use: "auto", "max" or the number.
std::string NameOf(const Choice& choice);

// controls.json's `frame_rate` block. A default-built one is Auto with nothing to offer: what a run has when the block will not read.
struct Rules {
    Choice fallback; // `default`: what a player who has chosen nothing plays
    std::vector<Choice> desktop;
    std::vector<Choice> android;
    std::vector<Choice> ios;
    double refreshToleranceHz = 0.0; // how far over (offered) or under (already paced) the monitor's refresh counts as the same rate
    int androidLowRequestHz = 60;    // the lowest rate a number asks of an Android display
    bool smoothWhenLimited = false;  // where a cap holds the loop back, the pictures are smoothed (sim/Pictures SmoothingUnderCap)
};

// Read strictly: a block or a value that is missing, not its own kind or out of range is refused with `error` naming it. The lists go upwards; "max" is
// Android's alone and last.
bool LoadRules(const std::string& path, Rules& out, std::string& error);

// What the run may do with the setting, handed down by main (MagicPortalsLayer::Paths::frameRate). The default is "nothing": every suite's bare Paths, a
// capture, a route and any run that must reproduce read, apply and show nothing of it.
struct Policy {
    bool available = false;
    Platform platform = Platform::Desktop;
};

// The choices the Options row cycles through besides Auto: the platform's list; on a desktop, held to the monitor's refresh plus the tolerance
// (`refreshHz` 0: unknown, so the whole list).
std::vector<Choice> Offered(const Rules& rules, Platform platform, int refreshHz);

// The choice a press moves to: Auto, then each offered one upwards, then Auto again. A current choice that is not offered (kept from another monitor)
// goes to the next offered one above it, else Auto.
Choice Next(const Choice& current, const std::vector<Choice>& offered);

// What a choice asks of the run.
struct Request {
    int cap = 0;                             // frames a second asked of the engine, 0 for none
    std::optional<std::uint32_t> displayHz;  // WindowControl::SetPreferredRefreshRate's argument; nothing: the display is left to itself

    bool operator==(const Request&) const = default;
};

// `refreshHz` is the monitor's (0: unknown; a phone always passes 0) and `presentPaced` whether the display paces the loop (FIFO).
Request RequestFor(const Rules& rules, Platform platform, const Choice& choice, int refreshHz, bool presentPaced);

// Whether the row would change anything here: some offered choice, or the one in force, asks something Auto does not. A row that cannot is not shown.
bool DoesSomething(const Rules& rules, Platform platform, const Choice& current, int refreshHz, bool presentPaced);

// The player's choice. Opened and saved as the other small stores are (sim/Files: a temporary renamed over the file).
//
//   {"version":1,"frame_rate":60}      or "auto", or "max"
class Store {
public:
    // Reads `directory`/framerate.json if there is one. An empty directory is memory-only: nothing is read and nothing is written. A missing file is no
    // choice and no error. False, with `error`, for a file that exists and cannot be used (not a JSON object, another version, no usable frame_rate): no
    // choice is held, and the next Save writes over it.
    bool Open(const std::string& directory, std::string& error);

    std::optional<Choice> Chosen() const { return m_chosen; }

    // The choice, in memory.
    void SetChoice(const Choice& choice) { m_chosen = choice; }

    // The bytes Save writes for what the store holds: one line, no trailing newline. Empty while nothing was chosen.
    std::string Text() const;

    // Writes framerate.json, only when something was chosen and the bytes differ from the ones last written. A memory-only store succeeds.
    bool Save(std::string& error);

    bool Persists() const { return !m_path.empty(); }
    const std::string& Path() const { return m_path; }

private:
    std::string m_path;
    std::string m_written;
    std::optional<Choice> m_chosen;
};

} // namespace MagicPortals::FrameRate
