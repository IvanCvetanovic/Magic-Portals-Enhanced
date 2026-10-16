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

// A setting that can differ by the kind of run.
struct PerRun {
    bool attended = true; // a person's run
    bool other = true;    // every other run: a capture, a route, a level walk, a script
};

// game/data/engine.json. The defaults are the engine AS IT WAS, and the layer as it was: nothing opted into.
struct Table {
    std::uint32_t shadowMapResolution = 0;     // 0: the engine's own sizes (2048 cascades, 1024 cubes and spot maps)
    bool spritesOnlyScenePipelines = false;    // the four scene pipelines built with every draw folded to the unlit path
    PerRun assetWatching;                      // the engine's stat of every watched file on every frame (default true)
    PerRun spriteReport;                       // the layer's per-frame walk over its sprites that logs one crossing the view (default true)
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
};

// Reads engine.json strictly: every key present and of its type, the resolution 0 or 16 to 4096. False, with `error` naming the file and
// the key, leaves `out` untouched.
bool Load(const std::string& path, Table& out, std::string& error);

// The table for this kind of run, then the flags over it. The flags win: they are for one run.
Resolved Resolve(const Table& table, bool attended, const Overrides& overrides);

// The flags' values. False for anything but exactly what the flag says it takes.
bool ParseSwitch(const std::string& text, bool& out);                      // "on" or "off"
bool ParseShadowMaps(const std::string& text, std::uint32_t& out);         // a whole number, 0 or kShadowMapMin..kShadowMapMax
bool ParsePerfLog(const std::string& text, float& out);                    // seconds, above 0 and at most kPerfLogMaxSeconds

// One line for the log, so a log says what the run asked for: "shadow maps 16, sprites-only on, asset watching off, ...".
std::string Describe(const Resolved& resolved);

} // namespace MagicPortals::EngineTuning
