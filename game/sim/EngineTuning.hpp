#pragma once

// What the run asks of the engine's optional performance features, and of the port's own per-frame diagnostics (planning step 190).
//
// WHY THIS IS A TABLE AND NOT FOUR LINES IN main. The engine at the pin (5781518) has opt-ins that take weight off a game that does not use
// what they remove: the shadow maps no sprite samples (120 MiB), the lit surface shader no sprite reaches, the stat of every watched file
// on every frame. Each one is only worth keeping if it is measured to pay and leaves every picture byte for byte as it was, and a decision
// that rests on a measurement is data (CLAUDE.md rule 1): the numbers live in game/data/engine.json with where they came from, and keeping
// or declining one is an edit of that file, not of the code. A table that will not read leaves the engine AS IT WAS (Table{} below is that),
// never a half-set one.
//
// "ATTENDED" is RunKind.hpp's one word for a person sitting at the run (a window the game opened for someone to play): the settings that
// can only matter to one, or can only be wanted by a developer, differ for it. A capture, a route and a level walk are never attended, so
// hot reload of a file a developer saved and the sprites' on-screen log stay what parity work has always had.
//
// THE PHONE PROFILE (planning step 201, PORT-3). engine.json also carries a "mobile" block: the answers a phone's run gives where they are not the
// desktop's. A build made for a phone (BuildProfile.hpp, kMobileBuild) resolves under it by default; `--profile mobile` asks the same of a desktop
// run, for a test or a capture, and `--profile desktop` the other way. Resolve takes the profile as a plain bool and nothing else: with it false the
// answer is, for every table and every kind of run, the answer it was before the block existed (test_mp_tuning pins that against a copy of the old
// rule over a grid). The block holds only the keys named in MobileBlock; one it does not know is refused by name, so a decision not yet taken
// (the scene's sample cap, dynamic resolution) cannot be switched on by a key that nothing reads.
//
// Free of the engine and of the renderer, like the rest of sim/: main turns a Resolved into manifest fields.

#include <cstdint>
#include <optional>
#include <string>

namespace MagicPortals::EngineTuning {

// The engine clamps a shadow-map resolution into 16 to 4096 and keeps 0 as "the size it always was"
// (engine/src/core/GameRuntime.cpp, ResolveShadowMapResolution); the table says what it means and is refused outside it, so that a typo is
// not silently clamped to something else.
inline constexpr std::uint32_t kShadowMapMin = 16;
inline constexpr std::uint32_t kShadowMapMax = 4096;
// A perf log line every this many seconds at most: a flag that asks for hours is a typo.
inline constexpr float kPerfLogMaxSeconds = 3600.0f;

// The Vulkan 1.x minors the engine's device selection takes (GameManifest::minimumVulkanMinor, clamped into 1 to 2 by GameRuntime::ResolveVulkanMinor,
// engine/src/core/GameRuntime.cpp:117): the table says what it means and is refused outside it, as for the shadow maps.
inline constexpr std::uint32_t kVulkanMinorMin = 1;
inline constexpr std::uint32_t kVulkanMinorMax = 2;

// A setting that can differ by the kind of run.
struct PerRun {
    bool attended = true; // a person's run
    bool other = true;    // every other run: a capture, a route, a level walk, a script
};

// The "mobile" block: each member is a key the block may hold, and PRESENT means it replaces the desktop answer for a run under the phone profile.
// Absent means the phone profile answers as the desktop does.
struct MobileBlock {
    std::optional<std::uint32_t> minimumVulkanMinor; // minimum_vulkan_minor: 1 or 2. 1 lets a GPU that reports Vulkan 1.1 be used (a phone's stock driver)
    std::optional<std::uint32_t> shadowMapResolution; // shadow_map_resolution: as the top-level key
    std::optional<bool> assetWatching;                // asset_watching: a plain true/false, for both kinds of run (a phone has nobody to hot-reload for)
};

// game/data/engine.json. The defaults are the engine AS IT WAS, and the layer as it was: nothing opted into.
struct Table {
    std::uint32_t shadowMapResolution = 0;     // 0: the engine's own sizes (2048 cascades, 1024 cubes and spot maps)
    bool spritesOnlyScenePipelines = false;    // the four scene pipelines built with every draw folded to the unlit path
    PerRun assetWatching;                      // the engine's stat of every watched file on every frame (default true)
    PerRun spriteReport;                       // the layer's per-frame walk over its sprites that logs one crossing the view (default true)
    MobileBlock mobile;                        // the phone profile's answers; none asked for by default
};

// What the command line says, over the table. A developer's A/B: one exe, one setting changed, nothing else.
struct Overrides {
    std::optional<std::uint32_t> shadowMapResolution; // --shadow-maps <0 or 16..4096>
    std::optional<bool> spritesOnlyScenePipelines;    // --sprites-only <on|off>
    std::optional<bool> assetWatching;                // --asset-watching <on|off>
    std::optional<bool> spriteReport;                 // --sprite-report <on|off>
    float perfLogSeconds = 0.0f;                      // --perf-log <seconds>: the engine's frame statistics every so often (0: none)
};

struct Resolved {
    std::uint32_t shadowMapResolution = 0;
    bool spritesOnlyScenePipelines = false;
    bool assetWatching = true;
    bool spriteReport = true;
    float perfLogSeconds = 0.0f;
    std::uint32_t minimumVulkanMinor = 0; // 0: not asked, the engine's own (Vulkan 1.2); 1 or 2: set on the manifest
};

// Reads engine.json strictly: every key present and of its type, the resolution 0 or 16 to 4096, and a "mobile" object that holds only the keys of
// MobileBlock (each of its own type; one it does not know is refused by name, a key that begins with an underscore is a note and is skipped).
// False, with `error` naming the file and the key, leaves `out` untouched.
bool Load(const std::string& path, Table& out, std::string& error);

// The table for this kind of run and this profile, then the flags over it. The flags win: they are for one run. `mobile` false is the desktop
// answer, exactly the answer this function gave before the profile existed; true lays the table's "mobile" block over it.
Resolved Resolve(const Table& table, bool attended, bool mobile, const Overrides& overrides);

// The flags' values. False for anything but exactly what the flag says it takes.
bool ParseSwitch(const std::string& text, bool& out);                      // "on" or "off"
bool ParseShadowMaps(const std::string& text, std::uint32_t& out);         // a whole number, 0 or kShadowMapMin..kShadowMapMax
bool ParsePerfLog(const std::string& text, float& out);                    // seconds, above 0 and at most kPerfLogMaxSeconds
bool ParseProfile(const std::string& text, bool& mobile);                  // --profile: "desktop" (false) or "mobile" (true), exactly

// One line for the log, so a log says what the run asked for: "shadow maps 16, sprites-only on, asset watching off, ...".
std::string Describe(const Resolved& resolved);

} // namespace MagicPortals::EngineTuning
