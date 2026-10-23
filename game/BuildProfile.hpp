#pragma once

// WHAT KIND OF BUILD THIS IS, as two compile-time constants (planning step 201, PORT-3).
//
// Both come from CMake (game/CMakeLists.txt) and reach main.cpp ALONE, as per-source definitions, for the reason the pad-art folder and the
// engine commit do: a definition on a library every suite links would recompile and relink all of them for a fact no suite reads.
// Where a translation unit is given neither, this header says what a developer's desktop build is, so that a suite which includes it
// sees the build every suite is.
//
//   kMobileBuild  a phone's build (an Android or iOS target, or MAGICPORTALS_MOBILE=ON): the run starts under the phone profile,
//                 game/data/engine.json's "mobile" block, unless --profile says otherwise.
//   kDevTools     the developer's command-line flags (--route, --tap, --visit-levels ...) and the code behind them are compiled in.
//                 MAGICPORTALS_DEV_TOOLS=OFF, which a mobile build defaults to, leaves a player's build without them.
//
// A macro as well as a constant, because the dev tools are not a value to branch on: with them off, main.cpp does not name the types of
// LevelVisit.cpp and Route.cpp at all, and those two files are not part of the program.

#ifndef MAGICPORTALS_MOBILE
#define MAGICPORTALS_MOBILE 0
#endif
#ifndef MAGICPORTALS_DEV_TOOLS
#define MAGICPORTALS_DEV_TOOLS 1
#endif

namespace MagicPortals {

inline constexpr bool kMobileBuild = MAGICPORTALS_MOBILE != 0;
inline constexpr bool kDevTools = MAGICPORTALS_DEV_TOOLS != 0;

} // namespace MagicPortals
